/*
 *  Copyright (C) 2013 Motorola, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Adds ability to program periodic interrupts from user space that
 *  can wake the phone out of low power modes.
 *
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub_client_ioctl.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#define DISPLAY_CLIENT_DRIVER_NAME "m4sensorhub_display"
#define SENSORHUB_MIPI_DRIVER_REGULATOR "vcsi"
#define SYNC_CLOCK_RETRY_TIMES		3
#define INVALID_UTC_TIME		0xFFFFFFFF

struct display_client {
	struct m4sensorhub_data *m4sensorhub;
	struct regulator *regulator;
	atomic_t m4_lockcnt;
	struct mutex m4_mutex;
	int gpio_mipi_mux;
	int m4_control;
	int m4_enable;
	int timezone_offset;
	int m4_timezone_offset;
	int dailystep_offset;
	int m4_dailystep_offset;
};

struct display_client *global_display_data;

static u32 m4_display_get_clock(void);
static u32 m4_display_get_kernel_clock(void);
static int m4_display_set_clock(u32 ms);
static int m4_display_sync_timezone(struct display_client *data);
static int m4_display_sync_dailystep(struct display_client *data);
static int m4_display_sync_clock(void);
static int m4_display_sync_state(struct display_client *display_data);
static int m4_display_lock(struct display_client *display_data, int lock);
static int m4_display_set_control(int m4_ctrl, int gpio_mipi_mux);

static int display_client_open(struct inode *inode, struct file *file)
{
	int ret = -EFAULT;
	KDEBUG(M4SH_DEBUG, "%s:\n", __func__);
	if (global_display_data) {
		ret = nonseekable_open(inode, file);
		if (ret >= 0) {
			file->private_data = global_display_data;
			ret = 0;
		}
	}
	if (ret)
		KDEBUG(M4SH_ERROR, "%s: failed, err=%d\n", __func__, -ret);
	return ret;
}

static int display_client_close(struct inode *inode, struct file *file)
{
	KDEBUG(M4SH_DEBUG, "%s:\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long display_client_ioctl(
	struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct display_client *display_data = filp->private_data;
	int value;

	switch (cmd) {
	case M4_SENSOR_IOCTL_SET_TIMEZONE_OFFSET:
		if (copy_from_user(&value, argp, sizeof(value)))
			return -EFAULT;
		KDEBUG(M4SH_INFO, "%s update timezone offset %d\n",\
				__func__, value);
		m4_display_lock(display_data, true);
		display_data->timezone_offset = value;
		value = m4_display_sync_timezone(display_data);
		/* sync M4 clock every time when app set timezone offset */
		/* FIXME:Setting rtc clock on M4 happens in M4 ISR and leads to
		* i2c timeout for the first clock set, this causes the bus to
		* fail for a few mins on klockworks,hence commenting this part
		* of the code until fixed for klockworks
		if (value == 0)
			m4_display_sync_clock();
		*/
		m4_display_lock(display_data, false);
		return value;
	case M4_SENSOR_IOCTL_SET_DAILYSTEP_OFFSET:
		if (copy_from_user(&value, argp, sizeof(value)))
			return -EFAULT;
		KDEBUG(M4SH_INFO, "%s update dailystep offset %d\n",\
				__func__, value);
		display_data->dailystep_offset = value;
		return m4_display_sync_dailystep(display_data);
	case M4_SENSOR_IOCTL_LOCK_CLOCKFACE:
		if (copy_from_user(&value, argp, sizeof(value)))
			return -EFAULT;
		return m4_display_lock(display_data, value);
	default:
		KDEBUG(M4SH_ERROR, "%s Invaild ioctl cmd %d\n", __func__, cmd);
		break;
	}
	return -EINVAL;
}

/* Get current M4 RTC time of seconds elapsed since 00:00:00
 *   on January 1, 1970, Coordinated Universal Time
 * return 0xFFFFFFFF if it's something wrong
 */
static u32 m4_display_get_clock(void)
{
	u32 seconds;
	struct display_client *data = global_display_data;
	if (m4sensorhub_reg_getsize(data->m4sensorhub,\
		M4SH_REG_GENERAL_UTC) != m4sensorhub_reg_read(\
		data->m4sensorhub, M4SH_REG_GENERAL_UTC,\
		(char *)&seconds)) {
		seconds = INVALID_UTC_TIME;
		KDEBUG(M4SH_ERROR, "%s: Failed get M4 clock!\n", __func__);
	}
	return seconds;
}

/* Set current M4 RTC time of seconds elapsed since 00:00:00
 *   on January 1, 1970, Coordinated Universal Time
 * return < 0 if it's something wrong
 */
static int m4_display_set_clock(u32 seconds)
{
	struct display_client *data = global_display_data;
	if (m4sensorhub_reg_getsize(data->m4sensorhub,\
		M4SH_REG_GENERAL_UTC) != m4sensorhub_reg_write(\
		data->m4sensorhub, M4SH_REG_GENERAL_UTC,\
		(char *)&seconds, m4sh_no_mask)) {
		KDEBUG(M4SH_ERROR, "%s: Failed set M4 clock!\n", __func__);
		return -EIO;
	}
	return 0;
}

static void m4_notify_clock_change(void)
{
	struct display_client *data = global_display_data;
	char notify = 1;
	if (m4sensorhub_reg_getsize(data->m4sensorhub,\
		M4SH_REG_USERSETTINGS_RTCRESET) != m4sensorhub_reg_write(\
		data->m4sensorhub, M4SH_REG_USERSETTINGS_RTCRESET,\
		&notify, m4sh_no_mask)) {
		KDEBUG(M4SH_ERROR, "Failed to notify clock change");
	}
}

/* Get current Kernel RTC time of seconds elapsed since 00:00:00
 *   on January 1, 1970, Coordinated Universal Time
 */
static u32 m4_display_get_kernel_clock(void)
{
	struct timespec now = current_kernel_time();
	/* adjust with 500ms */
	if (now.tv_nsec > 500000000)
		now.tv_sec++;
	return (u32)now.tv_sec;
}

void print_time(char *info, u32 time)
{
	struct tm t; /* it convert to year since 1900 */
	time_to_tm((time_t)(time), 0, &t);
	KDEBUG(M4SH_INFO, "%s(%d) %02d:%02d:%02d %02d/%02d/%04d\n",\
		info, (int)time, t.tm_hour, t.tm_min, t.tm_sec,\
		t.tm_mon+1, t.tm_mday, (int)t.tm_year+1900);
}

/* Sync M4 clock with current kernel time */
static int m4_display_sync_clock(void)
{
	int retry = 0;
	do {
		u32 m4_time = m4_display_get_clock();
		u32 kernel_time = m4_display_get_kernel_clock();
		u32 diff_time = m4_time > kernel_time \
			? m4_time-kernel_time : kernel_time-m4_time;
#ifdef	DEBUG_CLOCK
		print_time("M4 :", m4_time);
		print_time("KNL:", kernel_time);
#endif
		/* it needs adjust M4 time if different large than 1 second */
		if (diff_time < 2) {
			if (retry) {
				print_time("Synced M4 clock to", m4_time);
				m4_notify_clock_change();
			}
			return 0;
		}
		m4_display_set_clock(kernel_time);
	} while (retry++ < SYNC_CLOCK_RETRY_TIMES);
	KDEBUG(M4SH_ERROR, "%s: Failed to sync M4 clock!\n",  __func__);
	return -EIO;
}

/* Sync M4 TimeZone offset */
static int m4_display_sync_timezone(struct display_client *data)
{
	if (data->m4_timezone_offset == data->timezone_offset)
		return 0;
	if (m4sensorhub_reg_getsize(data->m4sensorhub,\
		M4SH_REG_GENERAL_LOCALTIMEZONE) != m4sensorhub_reg_write(\
		data->m4sensorhub, M4SH_REG_GENERAL_LOCALTIMEZONE,\
		(char *)&(data->timezone_offset), m4sh_no_mask)) {
		KDEBUG(M4SH_ERROR, "%s: Failed set M4 timezone!\n", __func__);
		return -EIO;
	}
	data->m4_timezone_offset = data->timezone_offset;
	return 0;
}

/* Sync M4 Daily Steps offset */
static int m4_display_sync_dailystep(struct display_client *data)
{
	if (data->m4_dailystep_offset == data->dailystep_offset)
		return 0;
	if (m4sensorhub_reg_getsize(data->m4sensorhub,\
		M4SH_REG_TIMEPIECE_OFFSETSTEPS) != m4sensorhub_reg_write(\
		data->m4sensorhub, M4SH_REG_TIMEPIECE_OFFSETSTEPS,\
		(char *)&(data->dailystep_offset), m4sh_no_mask)) {
		KDEBUG(M4SH_ERROR, "%s: Failed set M4 dailystep!\n", __func__);
		return -EIO;
	}
	data->m4_dailystep_offset = data->dailystep_offset;
	return 0;
}

/* Sync M4 clockface state, it will be enable only when clockface is unlocked
 */
static int m4_display_sync_state(struct display_client *display_data)
{
	int enable = !!display_data->m4_control;
	if (atomic_inc_return(&(display_data->m4_lockcnt)) != 1)
		enable = 0;
	atomic_dec(&(display_data->m4_lockcnt));

	mutex_lock(&(display_data->m4_mutex));
	if (enable == display_data->m4_enable)
		goto __done__;
	if (enable) {
		/* switch display control to M4 */
		m4_display_sync_clock();
		m4_display_sync_timezone(display_data);
		m4_display_sync_dailystep(display_data);
		gpio_set_value(display_data->gpio_mipi_mux, 1);
		if (regulator_enable(display_data->regulator)) {
			KDEBUG(M4SH_ERROR, "Failed enable regulator!\n");
			goto __error__;
		}
	}
	/* comunicate with M4 via I2C */
	if (1 != m4sensorhub_reg_write_1byte(\
		display_data->m4sensorhub,\
		M4SH_REG_TIMEPIECE_ENABLE, enable, 0xFF)) {
		KDEBUG(M4SH_ERROR, "Failed set m4 display!\n");
		goto __error__;
	}
	if (!enable) {
		mdelay(2);
		/* switch display control to OMAP */
		regulator_disable(display_data->regulator);
		gpio_set_value(display_data->gpio_mipi_mux, 0);
	}
	display_data->m4_enable = enable;
	KDEBUG(M4SH_INFO, "Synced M4 display state(%d)\n", enable);
__done__:
	mutex_unlock(&(display_data->m4_mutex));
	return 0;
__error__:
	/* when error occured, it always set control to OMAP */
	regulator_disable(display_data->regulator);
	gpio_set_value(display_data->gpio_mipi_mux, 0);
	mutex_unlock(&(display_data->m4_mutex));
	KDEBUG(M4SH_ERROR, "Failed sync m4 with state(%d)!\n", enable);
	return -EIO;
}

/* M4 clockface lock/unlock */
static int m4_display_lock(struct display_client *display_data, int lock)
{
	if (lock) {
		atomic_inc(&(display_data->m4_lockcnt));
	} else {
		if (atomic_dec_return(&(display_data->m4_lockcnt)) == -1) {
			atomic_inc(&(display_data->m4_lockcnt));
			KDEBUG(M4SH_ERROR, "%s zero unlock count!\n",\
				__func__);
			return -EINVAL;
		}
	}
	return m4_display_sync_state(display_data);
}

static int m4_display_set_control(int m4_ctrl, int gpio_mipi_mux)
{
	struct display_client *display_data = global_display_data;
	KDEBUG(M4SH_INFO, "%s(%d)\n", __func__, m4_ctrl);

	if (!display_data || !display_data->m4sensorhub || (gpio_mipi_mux < 0))
		return -EINVAL;

	if (m4_ctrl == display_data->m4_control) {
		KDEBUG(M4SH_DEBUG, "%s is already set!\n", __func__);
		return 0;
	}

	display_data->gpio_mipi_mux = gpio_mipi_mux;
	display_data->m4_control = m4_ctrl;

	return m4_display_sync_state(display_data);
}

int m4sensorhub_set_display_control(int m4_ctrl, int gpio_mipi_mux)
{
	return m4_display_set_control(m4_ctrl, gpio_mipi_mux);
}
EXPORT_SYMBOL_GPL(m4sensorhub_set_display_control);

static const struct file_operations display_client_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = display_client_ioctl,
	.open  = display_client_open,
	.release = display_client_close,
};

static struct miscdevice display_client_miscdrv = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DISPLAY_CLIENT_DRIVER_NAME,
	.fops = &display_client_fops,
};

/* display_panic_restore()

   Panic Callback Handler is called after M4 has been restarted

*/
static void display_panic_restore(\
	struct m4sensorhub_data *m4sensorhub, void *data)
{
	struct display_client *display_data = (struct display_client *)data;
	display_data->m4_timezone_offset = 0;
	display_data->m4_dailystep_offset = 0;
	if (display_data->m4_enable) {
		m4_display_sync_clock();
		m4_display_sync_timezone(display_data);
		m4_display_sync_dailystep(display_data);
		/* comunicate with M4 via I2C */
		if (1 != m4sensorhub_reg_write_1byte(\
			display_data->m4sensorhub,\
			M4SH_REG_TIMEPIECE_ENABLE, 1, 0xFF)) {
			KDEBUG(M4SH_ERROR, "Failed re-enable m4 display!\n");
			/* TODO retry ? */
		}
	}
}

static int display_driver_init(struct init_calldata *p_arg)
{
	int err;
	struct m4sensorhub_data *m4sensorhub = p_arg->p_m4sensorhub_data;

	err = m4sensorhub_panic_register(m4sensorhub, PANICHDL_DISPLAY_RESTORE,
		display_panic_restore, global_display_data);
	if (err < 0)
		KDEBUG(M4SH_ERROR, "Display driver init failed\n");

	return err;
}

static int display_client_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct display_client *display_data;
	struct m4sensorhub_data *m4sensorhub = m4sensorhub_client_get_drvdata();

	if (!m4sensorhub)
		return -EFAULT;

	display_data = kzalloc(sizeof(*display_data),
						GFP_KERNEL);
	if (!display_data)
		return -ENOMEM;

	display_data->m4sensorhub = m4sensorhub;
	display_data->timezone_offset = 0;
	display_data->dailystep_offset = 0;
	display_data->m4_timezone_offset = 0;
	display_data->m4_dailystep_offset = 0;

	platform_set_drvdata(pdev, display_data);

	display_data->regulator = regulator_get(NULL,
					    SENSORHUB_MIPI_DRIVER_REGULATOR);
	if (IS_ERR(display_data->regulator)) {
		KDEBUG(M4SH_ERROR, "Error requesting %s regulator\n",
			SENSORHUB_MIPI_DRIVER_REGULATOR);
		ret = -EFAULT;
		goto free_mem;
	}

	ret = misc_register(&display_client_miscdrv);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering %s driver\n",
				 DISPLAY_CLIENT_DRIVER_NAME);
		goto disable_regulator;
	}

	/* default to host control */
	display_data->m4_control = 0;
	display_data->m4_enable = 0;
	atomic_set(&(display_data->m4_lockcnt), 0);
	mutex_init(&(display_data->m4_mutex));

	global_display_data = display_data;
	ret = m4sensorhub_register_initcall(display_driver_init, display_data);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Unable to register init function "
			"for display client = %d\n", ret);
		goto unregister_misc_device;
	}

	KDEBUG(M4SH_INFO, "Initialized %s driver\n", __func__);
	return 0;

unregister_misc_device:
	misc_deregister(&display_client_miscdrv);
disable_regulator:
	regulator_put(display_data->regulator);
free_mem:
	global_display_data = NULL;
	platform_set_drvdata(pdev, NULL);
	kfree(display_data);
	return ret;
}

static int __exit display_client_remove(struct platform_device *pdev)
{
	struct display_client *display_data =
						platform_get_drvdata(pdev);

	m4sensorhub_unregister_initcall(display_driver_init);
	misc_deregister(&display_client_miscdrv);
	global_display_data = NULL;
	regulator_put(display_data->regulator);
	platform_set_drvdata(pdev, NULL);
	display_data->m4sensorhub = NULL;
	kfree(display_data);
	return 0;
}

static void display_client_shutdown(struct platform_device *pdev)
{
	return;
}

static struct of_device_id m4display_match_tbl[] = {
	{ .compatible = "mot,m4display" },
	{},
};

static struct platform_driver display_client_driver = {
	.probe		= display_client_probe,
	.remove		= __exit_p(display_client_remove),
	.shutdown	= display_client_shutdown,
	.suspend	= NULL,
	.resume		= NULL,
	.driver		= {
		.name	= DISPLAY_CLIENT_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4display_match_tbl),
	},
};

static int __init display_client_init(void)
{
	return platform_driver_register(&display_client_driver);
}

static void __exit display_client_exit(void)
{
	platform_driver_unregister(&display_client_driver);
}

module_init(display_client_init);
module_exit(display_client_exit);

MODULE_ALIAS("platform:display_client");
MODULE_DESCRIPTION("M4 Sensor Hub AOD display driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
