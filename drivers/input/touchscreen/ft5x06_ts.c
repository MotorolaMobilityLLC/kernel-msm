/*
 *
 * FocalTech ft5x06 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/input/ft5x06_ts.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>


#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
/* Early-suspend level */
#define FT_SUSPEND_LEVEL 1
#endif

#if defined(CONFIG_FT_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
static irqreturn_t ft5x06_ts_interrupt(int irq, void *data);
#endif

#include <linux/usb.h>
#include <linux/power_supply.h>

#define FT_DRIVER_VERSION	0x02

#define FT_META_REGS		3
#define FT_ONE_TCH_LEN		6
#define FT_TCH_LEN(x)		(FT_META_REGS + FT_ONE_TCH_LEN * x)

#define FT_PRESS		0x7F
#define FT_MAX_ID		0x0F
#define FT_TOUCH_X_H_POS	3
#define FT_TOUCH_X_L_POS	4
#define FT_TOUCH_Y_H_POS	5
#define FT_TOUCH_Y_L_POS	6
#define FT_TD_STATUS		2
#define FT_TOUCH_EVENT_POS	3
#define FT_TOUCH_ID_POS		5
#define FT_TOUCH_DOWN		0
#define FT_TOUCH_CONTACT	2

/* register address*/
#define FT_REG_DEV_MODE		0x00
#define FT_DEV_MODE_REG_CAL	0x02
#define FT_REG_FW_ID		0xA1
#define FT_REG_ID		0xA3
#define FT_REG_PMODE		0xA5
#define FT_REG_FW_VER		0xA6
#define FT_REG_FW_VENDOR_ID	0xA8
#define FT_REG_POINT_RATE	0x88
#define FT_REG_THGROUP		0x80
#define FT_REG_ECC		0xCC
#define FT_REG_RESET_FW		0x07
#define FT_REG_FW_MIN_VER	0xB2
#define FT_REG_FW_SUB_MIN_VER	0xB3
#define FT_REG_I2C_MODE		0xEB
#define FT_REG_FW_LEN		0xB0
#define FT_REG_CHG_STATE	0x8B

/* i2c mode register value */
#define FT_VAL_I2C_MODE		0xAA
#define FT_VAL_I2C_MODE_STD	0x09
#define FT_VAL_I2C_MODE_STD_CFM	0x08

/* gesture register address*/
#define FT_REG_GESTURE_ENABLE	0xD0
#define FT_REG_GESTURE_OUTPUT	0xD3

/* gesture register bits*/
#define FT_GESTURE_DOUBLECLICK_COORD_X		100
#define FT_GESTURE_DOUBLECLICK_COORD_Y		100
#define FT_GESTURE_WAKEUP_TIMEOUT		500
#define FT_GESTURE_DEFAULT_TRACKING_ID		0x0A
#define FT_GESTURE_DOUBLECLICK_ID		0x24
#define FT_GESTURE_POINTER_NUM_MAX		128
#define FT_GESTURE_POINTER_SIZEOF		4
#define FT_GESTURE_ID_FLAG_SIZE			1
#define FT_GESTURE_POINTER_NUM_FLAG_SIZE	1
/* 6 bytes are taken to mark which gesture is supported in firmware */
#define FT_GESTURE_SET_FLAG_SIZE		6
#define I2C_TRANSFER_MAX_BYTE			255
#define FT_GESTURE_DATA_HEADER	(FT_GESTURE_ID_FLAG_SIZE + \
				FT_GESTURE_POINTER_NUM_FLAG_SIZE + \
				FT_GESTURE_SET_FLAG_SIZE)

/* power register bits*/
#define FT_PMODE_ACTIVE		0x00
#define FT_PMODE_MONITOR	0x01
#define FT_PMODE_STANDBY	0x02
#define FT_PMODE_HIBERNATE	0x03
#define FT_FACTORYMODE_VALUE	0x40
#define FT_WORKMODE_VALUE	0x00
#define FT_RST_CMD_REG1		0xFC
#define FT_RST_CMD_REG2		0xBC
#define FT_READ_ID_REG		0x90
#define FT_ERASE_APP_REG	0x61
#define FT_ERASE_PANEL_REG	0x63
#define FT_FW_START_REG		0xBF

#define FT_STATUS_NUM_TP_MASK	0x0F

#define FT_VTG_MIN_UV		2600000
#define FT_VTG_MAX_UV		3300000
#define FT_I2C_VTG_MIN_UV	1800000
#define FT_I2C_VTG_MAX_UV	1800000

#define FT_COORDS_ARR_SIZE	4
#define MAX_BUTTONS		4

#define FT_8BIT_SHIFT		8
#define FT_4BIT_SHIFT		4
#define FT_FW_NAME_MAX_LEN	50

#define FT5316_ID		0x0A
#define FT5306I_ID		0x55
#define FT6X06_ID		0x06
#define FT6X36_ID		0x36

#define FT_UPGRADE_AA		0xAA
#define FT_UPGRADE_55		0x55

#define FT_FW_MIN_SIZE		8
#define FT_FW_MAX_SIZE		65536

/* Firmware file is not supporting minor and sub minor so use 0 */
#define FT_FW_FILE_MAJ_VER(x)	((x)->data[(x)->size - 2])
#define FT_FW_FILE_MIN_VER(x)	0
#define FT_FW_FILE_SUB_MIN_VER(x) 0
#define FT_FW_FILE_VENDOR_ID(x)	((x)->data[(x)->size - 1])

#define FT_FW_FILE_MAJ_VER_FT6X36(x)	((x)->data[0x10a])
#define FT_FW_FILE_VENDOR_ID_FT6X36(x)	((x)->data[0x108])

/**
* Application data verification will be run before upgrade flow.
* Firmware image stores some flags with negative and positive value
* in corresponding addresses, we need pick them out do some check to
* make sure the application data is valid.
*/
#define FT_FW_CHECK(x, ts_data) \
	(ts_data->family_id == FT6X36_ID ? \
	(((x)->data[0x104] ^ (x)->data[0x105]) == 0xFF \
	&& ((x)->data[0x106] ^ (x)->data[0x107]) == 0xFF) : \
	(((x)->data[(x)->size - 8] ^ (x)->data[(x)->size - 6]) == 0xFF \
	&& ((x)->data[(x)->size - 7] ^ (x)->data[(x)->size - 5]) == 0xFF \
	&& ((x)->data[(x)->size - 3] ^ (x)->data[(x)->size - 4]) == 0xFF))

#define FT_MAX_TRIES		5
#define FT_RETRY_DLY		20

#define FT_MAX_WR_BUF		10
#define FT_MAX_RD_BUF		3
#define FT_FW_PKT_LEN		128
#define FT_FW_PKT_META_LEN	6
#define FT_FW_PKT_DLY_MS	20
#define FT_FW_LAST_PKT		0x6ffa
#define FT_EARSE_DLY_MS		100
#define FT_55_AA_DLY_NS		5000

#define FT_UPGRADE_LOOP		30
#define FT_CAL_START		0x04
#define FT_CAL_FIN		0x00
#define FT_CAL_STORE		0x05
#define FT_CAL_RETRY		100
#define FT_REG_CAL		0x00
#define FT_CAL_MASK		0x70

#define FT_INFO_MAX_LEN		512

#define FT_BLOADER_SIZE_OFF	12
#define FT_BLOADER_NEW_SIZE	30
#define FT_DATA_LEN_OFF_OLD_FW	8
#define FT_DATA_LEN_OFF_NEW_FW	14
#define FT_FINISHING_PKT_LEN_OLD_FW	6
#define FT_FINISHING_PKT_LEN_NEW_FW	12
#define FT_MAGIC_BLOADER_Z7	0x7bfa
#define FT_MAGIC_BLOADER_LZ4	0x6ffa
#define FT_MAGIC_BLOADER_GZF_30	0x7ff4
#define FT_MAGIC_BLOADER_GZF	0x7bf4

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

static irqreturn_t ft5x06_ts_interrupt(int irq, void *data);

enum {
	FT_BLOADER_VERSION_LZ4 = 0,
	FT_BLOADER_VERSION_Z7 = 1,
	FT_BLOADER_VERSION_GZF = 2,
};

enum {
	FT_FT5336_FAMILY_ID_0x11 = 0x11,
	FT_FT5336_FAMILY_ID_0x12 = 0x12,
	FT_FT5336_FAMILY_ID_0x13 = 0x13,
	FT_FT5336_FAMILY_ID_0x14 = 0x14,
};

#define FT_STORE_TS_INFO(buf, id, fw_maj, fw_min, fw_sub_min) \
			snprintf(buf, FT_INFO_MAX_LEN, \
				"vendor name = Focaltech\n" \
				"model = 0x%x\n" \
				"fw_version = %d.%d.%d\n", \
				id, fw_maj, fw_min, fw_sub_min)
#define FT_TS_INFO_SYSFS_DIR_NAME "ts_info"
static char *ts_info_buff;

#define FT_STORE_TS_DBG_INFO(buf, id, name, max_tch, group_id, \
			fw_vkey_support, fw_name, fw_maj, fw_min, fw_sub_min) \
			snprintf(buf, FT_INFO_MAX_LEN, \
				"controller\t= focaltech\n" \
				"model\t\t= 0x%x\n" \
				"name\t\t= %s\n" \
				"max_touches\t= %d\n" \
				"drv_ver\t\t= 0x%x\n" \
				"group_id\t= 0x%x\n" \
				"fw_vkey_support\t= %s\n" \
				"fw_name\t\t= %s\n" \
				"fw_ver\t\t= %d.%d.%d\n", id, name, \
				max_tch, FT_DRIVER_VERSION, group_id, \
				fw_vkey_support, fw_name, fw_maj, fw_min, \
				fw_sub_min)

#define FT_DEBUG_DIR_NAME	"ts_debug"
#define FT_FW_ID_LEN		2

#define FT_PROC_DIR_NAME	"ftxxxx-debug"

#define EXP_FN_DET_INTERVAL	1000 /* ms */

enum {
	PROC_UPGRADE		= 0,
	PROC_READ_REGISTER	= 1,
	PROC_WRITE_REGISTER	= 2,
	PROC_WRITE_DATA		= 6,
	PROC_READ_DATA		= 7
};

enum {
	FT_MOD_CHARGER,
	FT_MOD_FPS,
	FT_MOD_MAX
};

struct ft5x06_clip_area {
	unsigned xul_clip, yul_clip, xbr_clip, ybr_clip;
	unsigned inversion; /* clip inside (when 1) or ouside otherwise */
};

struct ft5x06_modifiers {
	int	mods_num;
	struct semaphore list_sema;
	struct list_head mod_head;

};

struct config_modifier {
	const char *name;
	int id;
	bool effective;
	struct ft5x06_clip_area *clipa;
	struct list_head link;
};

struct ft5x06_exp_fn_ctrl {
	struct delayed_work det_work;
	struct workqueue_struct *det_workqueue;
	struct ft5x06_ts_data *ft5x06_ts_data_ptr;
};

static struct ft5x06_exp_fn_ctrl exp_fn_ctrl;

struct ft5x06_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct ft5x06_ts_platform_data *pdata;
	struct ft5x06_gesture_platform_data *gesture_pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	struct mutex ft_clk_io_ctrl_mutex;
	char fw_name[FT_FW_NAME_MAX_LEN];
	bool loading_fw;
	u8 family_id;
	struct dentry *dir;
	u16 addr;
	bool suspended;
	char *ts_info;
	u8 *tch_data;
	u32 tch_data_len;
	u8 fw_ver[3];
	u8 fw_vendor_id;
	struct kobject *ts_info_kobj;
#if defined(CONFIG_FB)
	struct work_struct fb_notify_work;
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
#if defined(CONFIG_FT_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_irq_processed;
	bool st_initialized;
	struct clk *core_clk;
	struct clk *iface_clk;
#endif
	bool flash_enabled;
	bool force_reflash;
	u8 fw_id[FT_FW_ID_LEN];
	bool irq_enabled;
	struct proc_dir_entry *proc_entry;
	u8 proc_operate_mode;
	/* moto TP modifiers */
	bool patching_enabled;
	struct ft5x06_modifiers modifiers;
	bool charger_detection_enabled;
	struct work_struct ps_notify_work;
	struct notifier_block ps_notif;
	bool ps_is_present;

	bool fps_detection_enabled;
	bool is_fps_registered;
	struct notifier_block fps_notif;
	bool clipping_on;
	struct ft5x06_clip_area *clipa;
	bool is_tp_registered;
};

static int ft5x06_ts_start(struct device *dev);
static int ft5x06_ts_stop(struct device *dev);
static void ft5x06_resume_ps_chg_cm_state(struct ft5x06_ts_data *data);

#if defined(CONFIG_FT_SECURE_TOUCH)
static void ft5x06_secure_touch_init(struct ft5x06_ts_data *data)
{
	data->st_initialized = 0;

	init_completion(&data->st_powerdown);
	init_completion(&data->st_irq_processed);

	/* Get clocks */
	data->core_clk = devm_clk_get(&data->client->dev, "core_clk");
	if (IS_ERR(data->core_clk)) {
		data->core_clk = NULL;
		dev_warn(&data->client->dev,
			"%s: core_clk is not defined\n", __func__);
	}

	data->iface_clk = devm_clk_get(&data->client->dev, "iface_clk");
	if (IS_ERR(data->iface_clk)) {
		data->iface_clk = NULL;
		dev_warn(&data->client->dev,
			"%s: iface_clk is not defined", __func__);
	}
	data->st_initialized = 1;
}

static void ft5x06_secure_touch_notify(struct ft5x06_ts_data *data)
{
	sysfs_notify(&data->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t ft5x06_filter_interrupt(struct ft5x06_ts_data *data)
{
	if (atomic_read(&data->st_enabled)) {
		if (atomic_cmpxchg(&data->st_pending_irqs, 0, 1) == 0) {
			reinit_completion(&data->st_irq_processed);
			ft5x06_secure_touch_notify(data);
			wait_for_completion_interruptible(
						&data->st_irq_processed);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

/*
 * 'blocking' variable will have value 'true' when we want to prevent the driver
 * from accessing the xPU/SMMU protected HW resources while the session is
 * active.
 */
static void ft5x06_secure_touch_stop(struct ft5x06_ts_data *data, bool blocking)
{
	if (atomic_read(&data->st_enabled)) {
		atomic_set(&data->st_pending_irqs, -1);
		ft5x06_secure_touch_notify(data);
		if (blocking)
			wait_for_completion_interruptible(
						&data->st_powerdown);
	}
}

static int ft5x06_clk_prepare_enable(struct ft5x06_ts_data *data)
{
	int ret;

	ret = clk_prepare_enable(data->iface_clk);
	if (ret) {
		dev_err(&data->client->dev,
			"error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(data->core_clk);
	if (ret) {
		clk_disable_unprepare(data->iface_clk);
		dev_err(&data->client->dev,
			"error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void ft5x06_clk_disable_unprepare(struct ft5x06_ts_data *data)
{
	clk_disable_unprepare(data->core_clk);
	clk_disable_unprepare(data->iface_clk);
}

static int ft5x06_bus_get(struct ft5x06_ts_data *data)
{
	int retval;

	mutex_lock(&data->ft_clk_io_ctrl_mutex);
	retval = pm_runtime_get_sync(data->client->adapter->dev.parent);
	if (retval >= 0 &&  data->core_clk != NULL && data->iface_clk != NULL) {
		retval = ft5x06_clk_prepare_enable(data);
		if (retval)
			pm_runtime_put_sync(data->client->adapter->dev.parent);
	}
	mutex_unlock(&data->ft_clk_io_ctrl_mutex);
	return retval;
}

static void ft5x06_bus_put(struct ft5x06_ts_data *data)
{
	mutex_lock(&data->ft_clk_io_ctrl_mutex);
	if (data->core_clk != NULL && data->iface_clk != NULL)
		ft5x06_clk_disable_unprepare(data);
	pm_runtime_put_sync(data->client->adapter->dev.parent);
	mutex_unlock(&data->ft_clk_io_ctrl_mutex);
}

static ssize_t ft5x06_secure_touch_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&data->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t ft5x06_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;
	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!data->st_initialized)
		return -EIO;

	err = count;
	switch (value) {
	case 0:
		if (atomic_read(&data->st_enabled) == 0)
			break;
		ft5x06_bus_put(data);
		atomic_set(&data->st_enabled, 0);
		ft5x06_secure_touch_notify(data);
		complete(&data->st_irq_processed);
		ft5x06_ts_interrupt(data->client->irq, data);
		complete(&data->st_powerdown);
		break;

	case 1:
		if (atomic_read(&data->st_enabled)) {
			err = -EBUSY;
			break;
		}
		synchronize_irq(data->client->irq);
		if (ft5x06_bus_get(data) < 0) {
			dev_err(&data->client->dev, "ft5x06_bus_get failed\n");
			err = -EIO;
			break;
		}
		reinit_completion(&data->st_powerdown);
		reinit_completion(&data->st_irq_processed);
		atomic_set(&data->st_enabled, 1);
		atomic_set(&data->st_pending_irqs,  0);
		break;

	default:
		dev_err(&data->client->dev, "unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

/*
 * This function returns whether there are pending interrupts, or
 * other error conditions that need to be signaled to the userspace library,
 * according tot he following logic:
 * - st_enabled is 0 if secure touch is not enabled, returning -EBADF
 * - st_pending_irqs is -1 to signal that secure touch is in being stopped,
 *   returning -EINVAL
 * - st_pending_irqs is 1 to signal that there is a pending irq, returning
 *   the value "1" to the sysfs read operation
 * - st_pending_irqs is 0 (only remaining case left) if the pending interrupt
 *   has been processed, so the interrupt handler can be allowed to continue.
 */
static ssize_t ft5x06_secure_touch_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&data->st_enabled) == 0)
		return -EBADF;
	if (atomic_cmpxchg(&data->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;
	if (atomic_cmpxchg(&data->st_pending_irqs, 1, 0) == 1)
		val = 1;
	else
		complete(&data->st_irq_processed);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
#else
static void ft5x06_secure_touch_init(struct ft5x06_ts_data *data)
{
}
static irqreturn_t ft5x06_filter_interrupt(struct ft5x06_ts_data *data)
{
	return IRQ_NONE;
}
static void ft5x06_secure_touch_stop(struct ft5x06_ts_data *data, bool blocking)
{
}
#endif

static struct device_attribute attrs[] = {
#if defined(CONFIG_FT_SECURE_TOUCH)
		__ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
				ft5x06_secure_touch_enable_show,
				ft5x06_secure_touch_enable_store),
		__ATTR(secure_touch, S_IRUGO ,
				ft5x06_secure_touch_show, NULL),
#endif
};

static inline bool ft5x06_gesture_support_enabled(void)
{
	return config_enabled(CONFIG_TOUCHSCREEN_FT5X06_GESTURE);
}

static int ft5x06_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int ft5x06_i2c_write(struct i2c_client *client, char *writebuf,
			    int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

static int ft5x0x_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return ft5x06_i2c_write(client, buf, sizeof(buf));
}

static int ft5x0x_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return ft5x06_i2c_read(client, &addr, 1, val, 1);
}

#ifdef CONFIG_TOUCHSCREEN_FT5X06_GESTURE
static ssize_t ft5x06_gesture_enable_to_set_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			data->gesture_pdata->gesture_enable_to_set);
}

static ssize_t ft5x06_gesture_enable_to_set_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;

	if (data->suspended)
		return -EINVAL;

	ret = kstrtoul(buf, 16, &value);
	if (ret < 0) {
		dev_err(dev, "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	if (1 == value)
		data->gesture_pdata->gesture_enable_to_set = 1;
	else
		data->gesture_pdata->gesture_enable_to_set = 0;
	return size;
}

static DEVICE_ATTR(enable, 0664,
		ft5x06_gesture_enable_to_set_show,
		ft5x06_gesture_enable_to_set_store);

static int ft5x06_entry_pocket(struct device *dev)
{
	return ft5x06_ts_stop(dev);
}

static int ft5x06_leave_pocket(struct device *dev)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	int err;

	ft5x06_ts_start(dev);
	ft5x0x_write_reg(data->client, FT_REG_GESTURE_ENABLE, 1);
	err = enable_irq_wake(data->client->irq);
	if (err)
		dev_err(&data->client->dev,
			"%s: set_irq_wake failed\n", __func__);
	data->suspended = true;

	return err;
}

static ssize_t gesture_in_pocket_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			data->gesture_pdata->in_pocket);
}

static ssize_t gesture_in_pocket_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long value = 0;
	int ret;

	ret = kstrtoul(buf, 16, &value);
	if (ret < 0) {
		dev_err(dev, "%s:kstrtoul failed, ret=0x%x\n",
			__func__, ret);
		return ret;
	}

	if (1 == value && data->gesture_pdata->in_pocket == 0) {
		data->gesture_pdata->in_pocket = 1;
		ft5x06_entry_pocket(dev);
	} else if (0 == value && data->gesture_pdata->in_pocket == 1) {
		ft5x06_leave_pocket(dev);
		data->gesture_pdata->in_pocket = 0;
	}
	return size;
}

static DEVICE_ATTR(pocket, 0664,
		gesture_in_pocket_mode_show,
		gesture_in_pocket_mode_store);

static int ft5x06_report_gesture_doubleclick(struct input_dev *ip_dev)
{
	int i;

	for (i = 0; i < 2; i++) {
		input_mt_slot(ip_dev, FT_GESTURE_DEFAULT_TRACKING_ID);
		input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 1);
		input_report_abs(ip_dev, ABS_MT_POSITION_X,
					FT_GESTURE_DOUBLECLICK_COORD_X);
		input_report_abs(ip_dev, ABS_MT_POSITION_Y,
					FT_GESTURE_DOUBLECLICK_COORD_Y);
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
		input_mt_slot(ip_dev, FT_GESTURE_DEFAULT_TRACKING_ID);
		input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
	}
	return 0;
}

static int ft5x06_report_gesture(struct i2c_client *i2c_client,
		struct input_dev *ip_dev)
{
	int i, temp, gesture_data_size;
	int gesture_coord_x, gesture_coord_y;
	int ret = -1;
	short pointnum = 0;
	unsigned char buf[FT_GESTURE_POINTER_NUM_MAX *
			FT_GESTURE_POINTER_SIZEOF + FT_GESTURE_DATA_HEADER];

	buf[0] = FT_REG_GESTURE_OUTPUT;
	ret = ft5x06_i2c_read(i2c_client, buf, 1,
				buf, FT_GESTURE_DATA_HEADER);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}

	/* FW support doubleclick */
	if (FT_GESTURE_DOUBLECLICK_ID == buf[0]) {
		ft5x06_report_gesture_doubleclick(ip_dev);
		return 0;
	}

	pointnum = (short)(buf[1]) & 0xff;
	gesture_data_size = pointnum * FT_GESTURE_POINTER_SIZEOF +
			FT_GESTURE_DATA_HEADER;
	buf[0] = FT_REG_GESTURE_OUTPUT;
	temp = gesture_data_size / I2C_TRANSFER_MAX_BYTE;
	for (i = 0; i < temp; i++)
		ret = ft5x06_i2c_read(i2c_client, buf, ((i == 0) ? 1 : 0),
			buf + I2C_TRANSFER_MAX_BYTE * i, I2C_TRANSFER_MAX_BYTE);
	ret = ft5x06_i2c_read(i2c_client, buf, ((temp == 0) ? 1 : 0),
			buf + I2C_TRANSFER_MAX_BYTE * temp,
			gesture_data_size - I2C_TRANSFER_MAX_BYTE * temp);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "%s read touchdata failed.\n",
			__func__);
		return ret;
	}

	for (i = 0; i < pointnum; i++) {
		gesture_coord_x = (((s16) buf[FT_GESTURE_DATA_HEADER +
				(FT_GESTURE_POINTER_SIZEOF * i)]) & 0x0F) << 8 |
				(((s16) buf[FT_GESTURE_DATA_HEADER + 1 +
				(FT_GESTURE_POINTER_SIZEOF * i)]) & 0xFF);
		gesture_coord_y = (((s16) buf[FT_GESTURE_DATA_HEADER + 2 +
				(FT_GESTURE_POINTER_SIZEOF * i)]) & 0x0F) << 8 |
				(((s16) buf[FT_GESTURE_DATA_HEADER + 3 +
				(FT_GESTURE_POINTER_SIZEOF * i)]) & 0xFF);
		input_mt_slot(ip_dev, FT_GESTURE_DEFAULT_TRACKING_ID);
		input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 1);
		input_report_abs(ip_dev, ABS_MT_POSITION_X, gesture_coord_x);
		input_report_abs(ip_dev, ABS_MT_POSITION_Y, gesture_coord_y);
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
	}
	input_mt_slot(ip_dev, FT_GESTURE_DEFAULT_TRACKING_ID);
	input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
	input_mt_report_pointer_emulation(ip_dev, false);
	input_sync(ip_dev);

	return 0;
}
#else
static DEVICE_ATTR(pocket, 0664, NULL, NULL);
static DEVICE_ATTR(enable, 0664, NULL, NULL);

static int ft5x06_report_gesture(struct i2c_client *i2c_client,
		struct input_dev *ip_dev)
{
	return 0;
}
#endif

static void ft5x06_update_fw_vendor_id(struct ft5x06_ts_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr;
	int err;

	reg_addr = FT_REG_FW_VENDOR_ID;
	err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_vendor_id, 1);
	if (err < 0)
		dev_err(&client->dev, "fw vendor id read failed");
}

static void ft5x06_update_fw_ver(struct ft5x06_ts_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr;
	int err;

	reg_addr = FT_REG_FW_VER;
	err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_ver[0], 1);
	if (err < 0)
		dev_err(&client->dev, "fw major version read failed");

	reg_addr = FT_REG_FW_MIN_VER;
	err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_ver[1], 1);
	if (err < 0)
		dev_err(&client->dev, "fw minor version read failed");

	reg_addr = FT_REG_FW_SUB_MIN_VER;
	err = ft5x06_i2c_read(client, &reg_addr, 1, &data->fw_ver[2], 1);
	if (err < 0)
		dev_err(&client->dev, "fw sub minor version read failed");

	dev_info(&client->dev, "Firmware version = %d.%d.%d\n",
		data->fw_ver[0], data->fw_ver[1], data->fw_ver[2]);
}

static void ft5x06_update_fw_id(struct ft5x06_ts_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr;
	int err;

	reg_addr = FT_REG_FW_ID;
	err = ft5x06_i2c_read(client, &reg_addr, 1,
		&data->fw_id[0], FT_FW_ID_LEN);
	if (err < 0)
		dev_err(&client->dev, "fw id read failed");

	dev_info(&client->dev, "Firmware id = 0x%02x%02x\n",
		data->fw_id[0], data->fw_id[1]);
}

static irqreturn_t ft5x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x06_ts_data *data = dev_id;
	struct input_dev *ip_dev;
	int rc, i;
	u32 id, x, y, status, num_touches;
	u8 reg, *buf, gesture_is_active;
	bool update_input = false;

	if (!data) {
		pr_err("%s: Invalid data\n", __func__);
		return IRQ_HANDLED;
	}

	if (IRQ_HANDLED == ft5x06_filter_interrupt(data))
		return IRQ_HANDLED;

	ip_dev = data->input_dev;
	buf = data->tch_data;

	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support) {
		ft5x0x_read_reg(data->client, FT_REG_GESTURE_ENABLE,
					&gesture_is_active);
		if (gesture_is_active) {
			pm_wakeup_event(&(data->client->dev),
					FT_GESTURE_WAKEUP_TIMEOUT);
			ft5x06_report_gesture(data->client, ip_dev);
			return IRQ_HANDLED;
		}
	}

	/*
	 * Read touch data start from register FT_REG_DEV_MODE.
	 * The touch x/y value start from FT_TOUCH_X_H/L_POS and
	 * FT_TOUCH_Y_H/L_POS in buf.
	 */
	reg = FT_REG_DEV_MODE;
	rc = ft5x06_i2c_read(data->client, &reg, 1, buf, data->tch_data_len);
	if (rc < 0) {
		dev_err(&data->client->dev, "%s: read data fail\n", __func__);
		return IRQ_HANDLED;
	}

	for (i = 0; i < data->pdata->num_max_touches; i++) {
		/*
		 * Getting the finger ID of the touch event incase of
		 * multiple touch events
		 */
		id = (buf[FT_TOUCH_ID_POS + FT_ONE_TCH_LEN * i]) >> 4;
		if (id >= FT_MAX_ID)
			break;

		update_input = true;

		x = (buf[FT_TOUCH_X_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
			(buf[FT_TOUCH_X_L_POS + FT_ONE_TCH_LEN * i]);
		y = (buf[FT_TOUCH_Y_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
			(buf[FT_TOUCH_Y_L_POS + FT_ONE_TCH_LEN * i]);

		status = buf[FT_TOUCH_EVENT_POS + FT_ONE_TCH_LEN * i] >> 6;

		num_touches = buf[FT_TD_STATUS] & FT_STATUS_NUM_TP_MASK;

		/* invalid combination */
		if (!num_touches && !status && !id)
			break;

		if (data->clipping_on && data->clipa) {
			bool inside;

			inside = (x >= data->clipa->xul_clip) &&
				(x <= data->clipa->xbr_clip) &&
				(y >= data->clipa->yul_clip) &&
				(y <= data->clipa->ybr_clip);

			if (inside == data->clipa->inversion) {
				/* Touch might still be active, but we're */
				/* sending release anyway to avoid touch  */
				/* stuck at the last reported position.   */
				/* Driver will suppress reporting to UI   */
				/* touch events from within clipping area */
				input_mt_slot(data->input_dev, id);
				input_mt_report_slot_state(data->input_dev,
					MT_TOOL_FINGER, 0);

				dev_dbg(&data->client->dev,
					" finger id-%d (%d,%d) within clipping area\n",
					id, x, y);

				continue;
			}
		}

		input_mt_slot(ip_dev, id);
		if (status == FT_TOUCH_DOWN || status == FT_TOUCH_CONTACT) {
			input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 1);
			input_report_abs(ip_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ip_dev, ABS_MT_POSITION_Y, y);
		} else {
			input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
		}
	}

	if (update_input) {
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
	}

	return IRQ_HANDLED;
}

static int ft5x06_gpio_configure(struct ft5x06_ts_data *data, bool on)
{
	int err = 0;

	if (on) {
		if (gpio_is_valid(data->pdata->irq_gpio)) {
			err = gpio_request(data->pdata->irq_gpio,
						"ft5x06_irq_gpio");
			if (err) {
				dev_err(&data->client->dev,
					"irq gpio request failed");
				goto err_irq_gpio_req;
			}

			err = gpio_direction_input(data->pdata->irq_gpio);
			if (err) {
				dev_err(&data->client->dev,
					"set_direction for irq gpio failed\n");
				goto err_irq_gpio_dir;
			}
		}

		if (gpio_is_valid(data->pdata->reset_gpio)) {
			err = gpio_request(data->pdata->reset_gpio,
						"ft5x06_reset_gpio");
			if (err) {
				dev_err(&data->client->dev,
					"reset gpio request failed");
				goto err_irq_gpio_dir;
			}

			err = gpio_direction_output(data->pdata->reset_gpio, 0);
			if (err) {
				dev_err(&data->client->dev,
				"set_direction for reset gpio failed\n");
				goto err_reset_gpio_dir;
			}
		}

		return 0;
	}
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

		return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	return err;
}

static int ft5x06_power_on(struct ft5x06_ts_data *data, bool on)
{
	int rc;

	if (!on)
		goto power_off;

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}

	return rc;

power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
		}
	}

	return rc;
}

static int ft5x06_power_init(struct ft5x06_ts_data *data, bool on)
{
	int rc;

	if (!on)
		goto pwr_deinit;

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->client->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FT_VTG_MIN_UV,
					   FT_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		dev_err(&data->client->dev,
			"Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FT_I2C_VTG_MIN_UV,
					   FT_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
			"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;

pwr_deinit:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vcc_i2c) > 0)
		regulator_set_voltage(data->vcc_i2c, 0, FT_I2C_VTG_MAX_UV);

	regulator_put(data->vcc_i2c);
	return 0;
}

static int ft5x06_ts_pinctrl_init(struct ft5x06_ts_data *ft5x06_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ft5x06_data->ts_pinctrl = devm_pinctrl_get(&(ft5x06_data->client->dev));
	if (IS_ERR_OR_NULL(ft5x06_data->ts_pinctrl)) {
		retval = PTR_ERR(ft5x06_data->ts_pinctrl);
		dev_dbg(&ft5x06_data->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	ft5x06_data->pinctrl_state_active
		= pinctrl_lookup_state(ft5x06_data->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(ft5x06_data->pinctrl_state_active)) {
		retval = PTR_ERR(ft5x06_data->pinctrl_state_active);
		dev_err(&ft5x06_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	ft5x06_data->pinctrl_state_suspend
		= pinctrl_lookup_state(ft5x06_data->ts_pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(ft5x06_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(ft5x06_data->pinctrl_state_suspend);
		dev_err(&ft5x06_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	ft5x06_data->pinctrl_state_release
		= pinctrl_lookup_state(ft5x06_data->ts_pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(ft5x06_data->pinctrl_state_release)) {
		retval = PTR_ERR(ft5x06_data->pinctrl_state_release);
		dev_dbg(&ft5x06_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(ft5x06_data->ts_pinctrl);
err_pinctrl_get:
	ft5x06_data->ts_pinctrl = NULL;
	return retval;
}

#ifdef CONFIG_PM
static int ft5x06_ts_start(struct device *dev)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	int err;

	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
				data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Cannot get active pinctrl state\n");
	}

	err = ft5x06_gpio_configure(data, true);
	if (err < 0) {
		dev_err(&data->client->dev,
			"failed to put gpios in resue state\n");
		goto err_gpio_configuration;
	}

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		usleep_range(800, 1000);
		if (data->pdata->power_on) {
			err = data->pdata->power_on(true);
			if (err) {
				dev_err(dev, "power on failed");
				goto err_gpio_configuration;
			}
		} else {
			err = ft5x06_power_on(data, true);
			if (err) {
				dev_err(dev, "power on failed");
				goto err_gpio_configuration;
			}
		}
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}

	msleep(data->pdata->soft_rst_dly);

	enable_irq(data->client->irq);
	data->irq_enabled = true;
	data->suspended = false;

	return 0;

err_gpio_configuration:
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_suspend);
		if (err < 0)
			dev_err(dev, "Cannot get suspend pinctrl state\n");
	}
	return err;
}

static int ft5x06_ts_stop(struct device *dev)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	char txbuf[2];
	int i, err;

	disable_irq(data->client->irq);
	data->irq_enabled = false;

	/* release all touches */
	for (i = 0; i < data->pdata->num_max_touches; i++) {
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
	}
	input_mt_report_pointer_emulation(data->input_dev, false);
	input_sync(data->input_dev);

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		txbuf[0] = FT_REG_PMODE;
		txbuf[1] = FT_PMODE_HIBERNATE;
		ft5x06_i2c_write(data->client, txbuf, sizeof(txbuf));
	}

	if (data->pdata->power_on) {
		err = data->pdata->power_on(false);
		if (err) {
			dev_err(dev, "power off failed");
			goto pwr_off_fail;
		}
	} else {
		err = ft5x06_power_on(data, false);
		if (err) {
			dev_err(dev, "power off failed");
			goto pwr_off_fail;
		}
	}

	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_suspend);
		if (err < 0)
			dev_err(dev, "Cannot get suspend pinctrl state\n");
	}

	err = ft5x06_gpio_configure(data, false);
	if (err < 0) {
		dev_err(&data->client->dev,
			"failed to put gpios in suspend state\n");
		goto gpio_configure_fail;
	}

	data->suspended = true;

	return 0;

gpio_configure_fail:
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Cannot get active pinctrl state\n");
	}
	if (data->pdata->power_on) {
		err = data->pdata->power_on(true);
		if (err)
			dev_err(dev, "power on failed");
	} else {
		err = ft5x06_power_on(data, true);
		if (err)
			dev_err(dev, "power on failed");
	}
pwr_off_fail:
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}
	enable_irq(data->client->irq);
	data->irq_enabled = true;
	return err;
}

static int ft5x06_ts_suspend(struct device *dev)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	int err;

	if (data->loading_fw) {
		dev_info(dev, "Firmware loading in process...\n");
		return 0;
	}

	if (data->suspended) {
		dev_info(dev, "Already in suspend state\n");
		return 0;
	}

	data->flash_enabled = false;
	ft5x06_secure_touch_stop(data, true);

	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support &&
		device_may_wakeup(dev) &&
		data->gesture_pdata->gesture_enable_to_set) {

		ft5x0x_write_reg(data->client, FT_REG_GESTURE_ENABLE, 1);
		err = enable_irq_wake(data->client->irq);
		if (err)
			dev_err(&data->client->dev,
				"%s: set_irq_wake failed\n", __func__);
		data->suspended = true;
		return err;
	}

	return ft5x06_ts_stop(dev);
}

static int ft5x06_ts_resume(struct device *dev)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	int err;

	if (!data->suspended) {
		dev_dbg(dev, "Already in awake state\n");
		return 0;
	}

	data->flash_enabled = true;
	ft5x06_secure_touch_stop(data, true);

	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support &&
		device_may_wakeup(dev) &&
		!(data->gesture_pdata->in_pocket) &&
		data->gesture_pdata->gesture_enable_to_set) {

		ft5x0x_write_reg(data->client, FT_REG_GESTURE_ENABLE, 0);
		err = disable_irq_wake(data->client->irq);
		if (err)
			dev_err(dev, "%s: disable_irq_wake failed\n",
				__func__);
		data->suspended = false;
		return err;
	}

	err = ft5x06_ts_start(dev);
	if (err < 0)
		return err;

	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support &&
		device_may_wakeup(dev) &&
		data->gesture_pdata->in_pocket &&
		data->gesture_pdata->gesture_enable_to_set) {

		ft5x0x_write_reg(data->client, FT_REG_GESTURE_ENABLE, 0);
		err = disable_irq_wake(data->client->irq);
		if (err)
			dev_err(dev, "%s: disable_irq_wake failed\n",
				__func__);
		data->suspended = false;
		data->gesture_pdata->in_pocket = 0;
	}

	if (data->charger_detection_enabled)
		ft5x06_resume_ps_chg_cm_state(data);

	return 0;
}

static const struct dev_pm_ops ft5x06_ts_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = ft5x06_ts_suspend,
	.resume = ft5x06_ts_resume,
#endif
};

#else
static int ft5x06_ts_suspend(struct device *dev)
{
	return 0;
}

static int ft5x06_ts_resume(struct device *dev)
{
	return 0;
}

#endif

#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct ft5x06_ts_data *ft5x06_data =
		container_of(work, struct ft5x06_ts_data, fb_notify_work);
	ft5x06_ts_resume(&ft5x06_data->client->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct ft5x06_ts_data *ft5x06_data =
		container_of(self, struct ft5x06_ts_data, fb_notif);

	if ((event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK) &&
		evdata && evdata->data && ft5x06_data && ft5x06_data->client) {
		blank = evdata->data;
		pr_debug("fb notification: event = %lu blank = %d\n", event,
			*blank);
		if (ft5x06_data->pdata->resume_in_workqueue) {
			if (event == FB_EARLY_EVENT_BLANK) {
				if (*blank != FB_BLANK_POWERDOWN)
					return 0;
				flush_work(&ft5x06_data->fb_notify_work);
				ft5x06_ts_suspend(&ft5x06_data->client->dev);
			} else if (*blank == FB_BLANK_UNBLANK ||
				(*blank == FB_BLANK_NORMAL &&
				ft5x06_data->suspended)) {
				schedule_work(&ft5x06_data->fb_notify_work);
			}
		} else {
			if (event == FB_EARLY_EVENT_BLANK) {
				if (*blank != FB_BLANK_POWERDOWN)
					return 0;
				ft5x06_ts_suspend(&ft5x06_data->client->dev);
			} else if (*blank == FB_BLANK_UNBLANK ||
				(*blank == FB_BLANK_NORMAL &&
				ft5x06_data->suspended)) {
				ft5x06_ts_resume(&ft5x06_data->client->dev);
			}
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ft5x06_ts_early_suspend(struct early_suspend *handler)
{
	struct ft5x06_ts_data *data = container_of(handler,
						   struct ft5x06_ts_data,
						   early_suspend);

	/*
	 * During early suspend/late resume, the driver doesn't access xPU/SMMU
	 * protected HW resources. So, there is no compelling need to block,
	 * but notifying the userspace that a power event has occurred is
	 * enough. Hence 'blocking' variable can be set to false.
	 */
	ft5x06_secure_touch_stop(data, false);
	ft5x06_ts_suspend(&data->client->dev);
}

static void ft5x06_ts_late_resume(struct early_suspend *handler)
{
	struct ft5x06_ts_data *data = container_of(handler,
						   struct ft5x06_ts_data,
						   early_suspend);

	ft5x06_secure_touch_stop(data, false);
	ft5x06_ts_resume(&data->client->dev);
}
#endif

static int ft5x06_set_charger_state(struct ft5x06_ts_data *data,
					bool enable)
{
	int retval;
	struct device *dev = &data->client->dev;

	/* disable IRQ to set CHARGER_STATE reg */
	disable_irq(data->client->irq);
	data->irq_enabled = false;
	if (enable) {
		retval = ft5x0x_write_reg(data->client,
			FT_REG_CHG_STATE, 1);
		if (retval < 0)
			dev_err(dev, "enable chg state failed(%d)\n",
				retval);
		else
			dev_info(dev, "set chg state\n");
	} else {
		retval = ft5x0x_write_reg(data->client,
			FT_REG_CHG_STATE, 0);
		if (retval < 0)
			dev_err(dev, "enable chg state failed(%d)\n",
				retval);
		else
			dev_info(dev, "unset chg state\n");
	}
	enable_irq(data->client->irq);
	data->irq_enabled = true;

	return (retval > 0) ? 0 : retval;
}

static int ps_get_state(struct ft5x06_ts_data *data,
			struct power_supply *psy, bool *present)
{
	struct device *dev = &data->client->dev;
	union power_supply_propval pval = {0};
	int retval;

	retval = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval);
	if (retval) {
		dev_err(dev, "%s psy get property failed\n", psy->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	dev_dbg(dev, "%s is %s\n", psy->name,
			(*present) ? "present" : "not present");
	return 0;
}

static void ft5x06_update_ps_chg_cm_state(
				struct ft5x06_ts_data *data, bool present)
{
	struct config_modifier *cm = NULL;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		if (cm->id == FT_MOD_CHARGER) {
			cm->effective = present;
			break;
		}
	}
	up(&data->modifiers.list_sema);
}

static void ft5x06_resume_ps_chg_cm_state(struct ft5x06_ts_data *data)
{
	struct config_modifier *cm = NULL;
	bool is_active = false;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		if (cm->id == FT_MOD_CHARGER) {
			is_active = cm->effective;
			break;
		}
	}
	up(&data->modifiers.list_sema);

	/* FT_REG_CHG_STATE bit is 0 when powerup
	So only need set this bit when need to be set as 1
	*/
	if (is_active)
		ft5x06_set_charger_state(data, true);
}

static void ps_notify_callback_work(struct work_struct *work)
{
	struct ft5x06_ts_data *ft5x06_data =
		container_of(work, struct ft5x06_ts_data, ps_notify_work);
	bool present = ft5x06_data->ps_is_present;
	struct device *dev = &ft5x06_data->client->dev;
	int retval;

	/* enable IC if it in suspend state */
	if (ft5x06_data->suspended) {
		dev_dbg(dev, "charger resumes tp ic\n");
		ft5x06_ts_resume(&ft5x06_data->client->dev);
	}

	retval = ft5x06_set_charger_state(ft5x06_data, present);
	if (retval) {
		dev_err(dev, "set charger state failed rc=%d\n", retval);
		return;
	}

	ft5x06_update_ps_chg_cm_state(ft5x06_data, present);
}

static int ps_notify_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct ft5x06_ts_data *ft5x06_data =
		container_of(self, struct ft5x06_ts_data, ps_notif);
	struct power_supply *psy = data;
	struct device *dev = NULL;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_ADDED || event == PSY_EVENT_PROP_CHANGED)
		&& psy && psy->get_property && psy->name &&
		!strncmp(psy->name, "usb", sizeof("usb")) &&
		ft5x06_data) {
		dev = &ft5x06_data->client->dev;
		dev_dbg(dev, "ps notification: event = %lu\n", event);
		retval = ps_get_state(ft5x06_data, psy, &present);
		if (retval) {
			dev_err(dev, "psy get property failed\n");
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (ft5x06_data->ps_is_present == present) {
				dev_info(dev, "ps present state not change\n");
				return 0;
			}
		}
		ft5x06_data->ps_is_present = present;
		schedule_work(&ft5x06_data->ps_notify_work);
	}

	return 0;
}

static struct config_modifier *modifier_by_id(
	struct ft5x06_ts_data *data, int id)
{
	struct config_modifier *cm, *found = NULL;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		pr_debug("walk-thru: ptr=%p modifier[%s] id=%d\n",
			cm, cm->name, cm->id);
		if (cm->id == id) {
			found = cm;
			break;
		}
	}
	up(&data->modifiers.list_sema);
	pr_debug("returning modifier id=%d[%d]\n", found ? found->id : -1, id);
	return found;
}

static void ft5x06_detection_work(struct work_struct *work)
{
	struct ft5x06_ts_data *data;
	int error;

	data = exp_fn_ctrl.ft5x06_ts_data_ptr;

	if (data == NULL) {
		if (exp_fn_ctrl.det_workqueue)
			queue_delayed_work(exp_fn_ctrl.det_workqueue,
				&exp_fn_ctrl.det_work,
			msecs_to_jiffies(EXP_FN_DET_INTERVAL));
		return;
	}

	if (data->fps_detection_enabled &&
		!data->is_fps_registered) {
		error = FPS_register_notifier(
				&data->fps_notif, 0xBEEF, false);
		if (error) {
			if (exp_fn_ctrl.det_workqueue)
				queue_delayed_work(
					exp_fn_ctrl.det_workqueue,
					&exp_fn_ctrl.det_work,
					msecs_to_jiffies(EXP_FN_DET_INTERVAL));
			pr_err("Failed to register fps_notifier\n");
		} else {
			data->is_fps_registered = true;
			pr_debug("registered FPS notifier\n");
		}
	}

}

static int fps_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	int fps_state = *(int *)data;
	struct ft5x06_ts_data *ft5x06_data =
		container_of(self, struct ft5x06_ts_data, fps_notif);

	if (ft5x06_data && event == 0xBEEF &&
			ft5x06_data && ft5x06_data->client) {
		struct config_modifier *cm =
			modifier_by_id(ft5x06_data, FT_MOD_FPS);
		if (!cm) {
			dev_err(&ft5x06_data->client->dev,
				"No FPS modifier found\n");
			goto done;
		}

		if (fps_state) {/* on */
			ft5x06_data->clipping_on = true;
			ft5x06_data->clipa = cm->clipa;
			cm->effective = true;
		} else {/* off */
			ft5x06_data->clipping_on = false;
			ft5x06_data->clipa = NULL;
			cm->effective = false;
		}
		pr_info("FPS: clipping is %s\n",
		ft5x06_data->clipping_on ? "ON" : "OFF");
	}
done:
	return 0;
}

static int ft5x06_auto_cal(struct i2c_client *client)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(client);
	u8 temp = 0, i;

	/* set to factory mode */
	msleep(2 * data->pdata->soft_rst_dly);
	ft5x0x_write_reg(client, FT_REG_DEV_MODE, FT_FACTORYMODE_VALUE);
	msleep(data->pdata->soft_rst_dly);

	/* start calibration */
	ft5x0x_write_reg(client, FT_DEV_MODE_REG_CAL, FT_CAL_START);
	msleep(2 * data->pdata->soft_rst_dly);
	for (i = 0; i < FT_CAL_RETRY; i++) {
		ft5x0x_read_reg(client, FT_REG_CAL, &temp);
		/* return to normal mode, calibration finish */
		if (((temp & FT_CAL_MASK) >> FT_4BIT_SHIFT) == FT_CAL_FIN)
			break;
	}

	/*calibration OK */
	msleep(2 * data->pdata->soft_rst_dly);
	ft5x0x_write_reg(client, FT_REG_DEV_MODE, FT_FACTORYMODE_VALUE);
	msleep(data->pdata->soft_rst_dly);

	/* store calibration data */
	ft5x0x_write_reg(client, FT_DEV_MODE_REG_CAL, FT_CAL_STORE);
	msleep(2 * data->pdata->soft_rst_dly);

	/* set to normal mode */
	ft5x0x_write_reg(client, FT_REG_DEV_MODE, FT_WORKMODE_VALUE);
	msleep(2 * data->pdata->soft_rst_dly);

	return 0;
}

static int ft5x06_fw_upgrade_start(struct i2c_client *client,
			const u8 *data, u32 data_len)
{
	struct ft5x06_ts_data *ts_data = i2c_get_clientdata(client);
	struct fw_upgrade_info info = ts_data->pdata->info;
	u8 reset_reg;
	u8 w_buf[FT_MAX_WR_BUF] = {0}, r_buf[FT_MAX_RD_BUF] = {0};
	u8 pkt_buf[FT_FW_PKT_LEN + FT_FW_PKT_META_LEN];
	int i, j, temp;
	u32 pkt_num, pkt_len;
	u8 is_5336_new_bootloader = false;
	u8 is_5336_fwsize_30 = false;
	u8 fw_ecc;

	/* determine firmware size */
	if (*(data + data_len - FT_BLOADER_SIZE_OFF) == FT_BLOADER_NEW_SIZE)
		is_5336_fwsize_30 = true;
	else
		is_5336_fwsize_30 = false;

	for (i = 0, j = 0; i < FT_UPGRADE_LOOP; i++) {
		msleep(FT_EARSE_DLY_MS);
		/* reset - write 0xaa and 0x55 to reset register */
		if (ts_data->family_id == FT6X06_ID
			|| ts_data->family_id == FT6X36_ID)
			reset_reg = FT_RST_CMD_REG2;
		else
			reset_reg = FT_RST_CMD_REG1;

		ft5x0x_write_reg(client, reset_reg, FT_UPGRADE_AA);
		msleep(info.delay_aa);

		ft5x0x_write_reg(client, reset_reg, FT_UPGRADE_55);
		if (i <= (FT_UPGRADE_LOOP / 2))
			msleep(info.delay_55 + i * 3);
		else
			msleep(info.delay_55 - (i - (FT_UPGRADE_LOOP / 2)) * 2);

		/* Set i2c to std i2c mode */
		w_buf[0] = FT_REG_I2C_MODE;
		w_buf[1] = FT_VAL_I2C_MODE;
		w_buf[2] = FT_VAL_I2C_MODE_STD;
		ft5x06_i2c_read(client, w_buf, 3, r_buf, 3);
		if (FT_REG_I2C_MODE != r_buf[0] ||
			FT_VAL_I2C_MODE != r_buf[1] ||
			FT_VAL_I2C_MODE_STD_CFM != r_buf[2]) {
			dev_err(&client->dev,
				"set std i2c error. r_val = 0x%02x%02x%02x\n",
				r_buf[0], r_buf[1], r_buf[2]);
			continue;
		}

		/* Enter upgrade mode */
		w_buf[0] = FT_UPGRADE_55;
		ft5x06_i2c_write(client, w_buf, 1);
		usleep_range(FT_55_AA_DLY_NS, FT_55_AA_DLY_NS + 1);
		w_buf[0] = FT_UPGRADE_AA;
		ft5x06_i2c_write(client, w_buf, 1);

		/* check READ_ID */
		msleep(info.delay_readid);
		w_buf[0] = FT_READ_ID_REG;
		w_buf[1] = 0x00;
		w_buf[2] = 0x00;
		w_buf[3] = 0x00;

		ft5x06_i2c_read(client, w_buf, 4, r_buf, 2);

		if (r_buf[0] != info.upgrade_id_1
			|| r_buf[1] != info.upgrade_id_2) {
			dev_err(&client->dev, "Upgrade ID mismatch(%d), IC=0x%x 0x%x, info=0x%x 0x%x\n",
				i, r_buf[0], r_buf[1],
				info.upgrade_id_1, info.upgrade_id_2);
		} else
			break;
	}

	if (i >= FT_UPGRADE_LOOP) {
		dev_err(&client->dev, "Abort upgrade\n");
		return -EIO;
	}

	w_buf[0] = 0xcd;
	ft5x06_i2c_read(client, w_buf, 1, r_buf, 1);

	if (r_buf[0] <= 4)
		is_5336_new_bootloader = FT_BLOADER_VERSION_LZ4;
	else if (r_buf[0] == 7)
		is_5336_new_bootloader = FT_BLOADER_VERSION_Z7;
	else if (r_buf[0] >= 0x0f &&
		((ts_data->family_id == FT_FT5336_FAMILY_ID_0x11) ||
		(ts_data->family_id == FT_FT5336_FAMILY_ID_0x12) ||
		(ts_data->family_id == FT_FT5336_FAMILY_ID_0x13) ||
		(ts_data->family_id == FT_FT5336_FAMILY_ID_0x14)))
		is_5336_new_bootloader = FT_BLOADER_VERSION_GZF;
	else
		is_5336_new_bootloader = FT_BLOADER_VERSION_LZ4;

	dev_dbg(&client->dev, "bootloader type=%d, r_buf=0x%x, family_id=0x%x\n",
		is_5336_new_bootloader, r_buf[0], ts_data->family_id);
	/* is_5336_new_bootloader = FT_BLOADER_VERSION_GZF; */

	/* erase app and panel paramenter area */
	w_buf[0] = FT_ERASE_APP_REG;
	ft5x06_i2c_write(client, w_buf, 1);
	msleep(info.delay_erase_flash);

	if (is_5336_fwsize_30) {
		w_buf[0] = FT_ERASE_PANEL_REG;
		ft5x06_i2c_write(client, w_buf, 1);
	}
	msleep(FT_EARSE_DLY_MS);

	w_buf[0] = FT_REG_FW_LEN;
	w_buf[1] = (u8)((data_len>>16) & 0xff);
	w_buf[2] = (u8)((data_len>>8) & 0xff);
	w_buf[3] = (u8)((data_len) & 0xff);
	ft5x06_i2c_write(client, w_buf, 4);

	pkt_num = (data_len) / FT_FW_PKT_LEN;
	pkt_len = FT_FW_PKT_LEN;
	pkt_buf[0] = FT_FW_START_REG;
	pkt_buf[1] = 0x00;
	fw_ecc = 0;

	for (i = 0; i < pkt_num; i++) {
		temp = i * FT_FW_PKT_LEN;
		pkt_buf[2] = (u8) (temp >> FT_8BIT_SHIFT);
		pkt_buf[3] = (u8) temp;
		pkt_buf[4] = (u8) (pkt_len >> FT_8BIT_SHIFT);
		pkt_buf[5] = (u8) pkt_len;

		for (j = 0; j < FT_FW_PKT_LEN; j++) {
			pkt_buf[6 + j] = data[i * FT_FW_PKT_LEN + j];
			fw_ecc ^= pkt_buf[6 + j];
		}

		ft5x06_i2c_write(client, pkt_buf,
				FT_FW_PKT_LEN + FT_FW_PKT_META_LEN);
		msleep(FT_FW_PKT_DLY_MS);
	}

	/* send remaining bytes */
	if ((data_len) % FT_FW_PKT_LEN > 0) {
		temp = pkt_num * FT_FW_PKT_LEN;
		pkt_buf[2] = (u8) (temp >> FT_8BIT_SHIFT);
		pkt_buf[3] = (u8) temp;
		temp = (data_len) % FT_FW_PKT_LEN;
		pkt_buf[4] = (u8) (temp >> FT_8BIT_SHIFT);
		pkt_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) {
			pkt_buf[6 + i] = data[pkt_num * FT_FW_PKT_LEN + i];
			fw_ecc ^= pkt_buf[6 + i];
		}

		ft5x06_i2c_write(client, pkt_buf, temp + FT_FW_PKT_META_LEN);
		msleep(FT_FW_PKT_DLY_MS);
	}

	/* send the finishing packet */
	if (is_5336_new_bootloader == FT_BLOADER_VERSION_LZ4 ||
		is_5336_new_bootloader == FT_BLOADER_VERSION_Z7) {
		for (i = 0; i < FT_FINISHING_PKT_LEN_OLD_FW; i++) {
			if (is_5336_new_bootloader  == FT_BLOADER_VERSION_Z7)
				temp = FT_MAGIC_BLOADER_Z7 + i;
			else if (is_5336_new_bootloader ==
						FT_BLOADER_VERSION_LZ4)
				temp = FT_MAGIC_BLOADER_LZ4 + i;
			pkt_buf[2] = (u8)(temp >> 8);
			pkt_buf[3] = (u8)temp;
			temp = 1;
			pkt_buf[4] = (u8)(temp >> 8);
			pkt_buf[5] = (u8)temp;
			pkt_buf[6] = data[data_len + i];
			fw_ecc ^= pkt_buf[6];

			ft5x06_i2c_write(client,
				pkt_buf, temp + FT_FW_PKT_META_LEN);
			msleep(FT_FW_PKT_DLY_MS);
		}
	} else if (is_5336_new_bootloader == FT_BLOADER_VERSION_GZF) {
		for (i = 0; i < FT_FINISHING_PKT_LEN_NEW_FW; i++) {
			if (is_5336_fwsize_30)
				temp = FT_MAGIC_BLOADER_GZF_30 + i;
			else
				temp = FT_MAGIC_BLOADER_GZF + i;
			pkt_buf[2] = (u8)(temp >> 8);
			pkt_buf[3] = (u8)temp;
			temp = 1;
			pkt_buf[4] = (u8)(temp >> 8);
			pkt_buf[5] = (u8)temp;
			pkt_buf[6] = data[data_len + i];
			fw_ecc ^= pkt_buf[6];

			ft5x06_i2c_write(client,
				pkt_buf, temp + FT_FW_PKT_META_LEN);
			msleep(FT_FW_PKT_DLY_MS);

		}
	}

	/* verify checksum */
	w_buf[0] = FT_REG_ECC;
	ft5x06_i2c_read(client, w_buf, 1, r_buf, 1);
	if (r_buf[0] != fw_ecc) {
		dev_err(&client->dev, "ECC error! dev_ecc=%02x fw_ecc=%02x\n",
					r_buf[0], fw_ecc);
		return -EIO;
	}

	/* reset */
	w_buf[0] = FT_REG_RESET_FW;
	ft5x06_i2c_write(client, w_buf, 1);
	msleep(ts_data->pdata->soft_rst_dly);

	dev_info(&client->dev, "Firmware upgrade successful\n");

	return 0;
}

static int ft5x06_fw_upgrade(struct device *dev, bool force)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	int rc;
	u8 fw_file_maj, fw_file_min, fw_file_sub_min, fw_file_vendor_id;
	bool fw_upgrade = false;

	if (data->suspended) {
		dev_err(dev, "Device is in suspend state: Exit FW upgrade\n");
		return -EBUSY;
	}

	rc = request_firmware(&fw, data->fw_name, dev);
	if (rc < 0) {
		dev_err(dev, "Request firmware failed - %s (%d)\n",
						data->fw_name, rc);
		return rc;
	}

	if (fw->size < FT_FW_MIN_SIZE || fw->size > FT_FW_MAX_SIZE) {
		dev_err(dev, "Invalid firmware size (%zu)\n", fw->size);
		rc = -EIO;
		goto rel_fw;
	}

	if (data->family_id == FT6X36_ID) {
		fw_file_maj = FT_FW_FILE_MAJ_VER_FT6X36(fw);
		fw_file_vendor_id = FT_FW_FILE_VENDOR_ID_FT6X36(fw);
	} else {
		fw_file_maj = FT_FW_FILE_MAJ_VER(fw);
		fw_file_vendor_id = FT_FW_FILE_VENDOR_ID(fw);
	}
	fw_file_min = FT_FW_FILE_MIN_VER(fw);
	fw_file_sub_min = FT_FW_FILE_SUB_MIN_VER(fw);
	fw_file_vendor_id = FT_FW_FILE_VENDOR_ID(fw);

	dev_info(dev, "Current firmware: %d.%d.%d", data->fw_ver[0],
				data->fw_ver[1], data->fw_ver[2]);
	dev_info(dev, "New firmware: %d.%d.%d", fw_file_maj,
				fw_file_min, fw_file_sub_min);

	if (force)
		fw_upgrade = true;
	else if ((data->fw_ver[0] < fw_file_maj) &&
		data->fw_vendor_id == fw_file_vendor_id)
		fw_upgrade = true;

	if (!fw_upgrade) {
		dev_info(dev, "Exiting fw upgrade...\n");
		rc = -EFAULT;
		goto rel_fw;
	}

	/* start firmware upgrade */
	if (FT_FW_CHECK(fw, data)) {
		rc = ft5x06_fw_upgrade_start(data->client, fw->data, fw->size);
		if (rc < 0)
			dev_err(dev, "update failed (%d). try later...\n", rc);
		else if (data->pdata->info.auto_cal)
			ft5x06_auto_cal(data->client);
	} else {
		dev_err(dev, "FW format error\n");
		rc = -EIO;
	}

	ft5x06_update_fw_ver(data);
	ft5x06_update_fw_id(data);

	FT_STORE_TS_DBG_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);
	FT_STORE_TS_INFO(ts_info_buff, data->family_id, data->fw_ver[0],
			 data->fw_ver[1], data->fw_ver[2]);
rel_fw:
	release_firmware(fw);
	return rc;
}

static ssize_t ft5x06_update_fw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 2, "%d\n", data->loading_fw);
}

static ssize_t ft5x06_update_fw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	if (data->suspended) {
		dev_info(dev, "In suspend state, try again later...\n");
		return size;
	}

	mutex_lock(&data->input_dev->mutex);
	if (!data->loading_fw  && val) {
		data->loading_fw = true;
		ft5x06_fw_upgrade(dev, false);
		data->loading_fw = false;
	}
	mutex_unlock(&data->input_dev->mutex);

	return size;
}

static DEVICE_ATTR(update_fw, 0664, ft5x06_update_fw_show,
				ft5x06_update_fw_store);

static ssize_t ft5x06_force_update_fw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = kstrtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	mutex_lock(&data->input_dev->mutex);
	if (!data->loading_fw  && val) {
		data->loading_fw = true;
		ft5x06_fw_upgrade(dev, true);
		data->loading_fw = false;
	}
	mutex_unlock(&data->input_dev->mutex);

	return size;
}

static DEVICE_ATTR(force_update_fw, 0664, ft5x06_update_fw_show,
				ft5x06_force_update_fw_store);

static ssize_t ft5x06_fw_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return snprintf(buf, FT_FW_NAME_MAX_LEN - 1, "%s\n", data->fw_name);
}

static ssize_t ft5x06_fw_name_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	if (size > FT_FW_NAME_MAX_LEN - 1)
		return -EINVAL;

	strlcpy(data->fw_name, buf, size);
	if (data->fw_name[size-1] == '\n')
		data->fw_name[size-1] = 0;

	return size;
}

static DEVICE_ATTR(fw_name, 0664, ft5x06_fw_name_show, ft5x06_fw_name_store);

static ssize_t ft5x06_poweron_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	bool val;

	mutex_lock(&data->input_dev->mutex);
	val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		val == false &&
		data->flash_enabled);
}

static DEVICE_ATTR(poweron, 0444, ft5x06_poweron_show, NULL);

static ssize_t ft5x06_productinfo_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		data->pdata->name);
}

static DEVICE_ATTR(productinfo, 0444, ft5x06_productinfo_show, NULL);

static ssize_t ft5x06_force_reflash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	data->force_reflash = (input == 0) ? false : true;

	return count;
}

static DEVICE_ATTR(forcereflash, 0220, NULL, ft5x06_force_reflash_store);

static ssize_t ft5x06_flashprog_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		(data->loading_fw) ? 1 : 0);
}

static DEVICE_ATTR(flashprog, 0444, ft5x06_flashprog_show, NULL);

static ssize_t ft5x06_do_reflash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	int retval;
	char prefix[FT_FW_NAME_MAX_LEN] = "focaltech";
	char template[FT_FW_NAME_MAX_LEN];

	if (count > FT_FW_NAME_MAX_LEN) {
		dev_err(&data->client->dev,
			"%s: FW filename is too long\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (data->suspended) {
		dev_err(&data->client->dev,
			"%s: In suspend state, try again later\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (data->loading_fw) {
		dev_err(&data->client->dev,
			"%s: In FW flashing state, try again later\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (!data->force_reflash) {
		if (strncmp(buf, prefix,
			strnlen(prefix, sizeof(prefix)))) {
			dev_err(&data->client->dev,
				"%s: FW does not belong to Focaltech\n",
				__func__);
			retval = -EINVAL;
			goto exit;
		}

		snprintf(template, sizeof(template), "-%s-",
						data->pdata->name);
		if (!strnstr(buf + strnlen(prefix, sizeof(prefix)), template,
			count)) {
			dev_err(&data->client->dev,
				"%s: FW does not belong to %s\n",
				__func__,
				data->pdata->name);
			retval = -EINVAL;
			goto exit;
		}
	}

	strlcpy(data->fw_name, buf, count);
	dev_dbg(&data->client->dev,
			"%s: FW filename: %s\n",
			__func__,
			data->fw_name);

	mutex_lock(&data->input_dev->mutex);
	data->loading_fw = true;
	retval = ft5x06_fw_upgrade(dev, true);
	if (retval)
		dev_err(&data->client->dev,
				"%s: FW %s upgrade failed\n",
				__func__,
				data->pdata->name);
	data->loading_fw = false;
	mutex_unlock(&data->input_dev->mutex);

	retval = count;
exit:
	data->fw_name[0] = 0;
	return retval;
}

static DEVICE_ATTR(doreflash, 0220, NULL, ft5x06_do_reflash_store);

static ssize_t ft5x06_build_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%02x%02x-%02x\n",
		data->fw_id[0], data->fw_id[1], data->fw_ver[0]);
}

static DEVICE_ATTR(buildid, 0444, ft5x06_build_id_show, NULL);

static ssize_t ft5x06_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	if ('1' != buf[0]) {
		pr_err("Invalid argument for reset\n");
		return -EINVAL;
	}

	mutex_lock(&data->input_dev->mutex);
	if (data->irq_enabled) {
		disable_irq(data->client->irq);
		data->irq_enabled = false;
	}
	retval = gpio_direction_output(data->pdata->reset_gpio, 0);
	if (0 == retval) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	} else {
		dev_err(&data->client->dev,
			"set direction for reset gpio failed\n");
	}
	msleep(100);
	enable_irq(data->client->irq);
	data->irq_enabled = true;
	mutex_unlock(&data->input_dev->mutex);
	return count;
}
static DEVICE_ATTR(reset, 0220, NULL, ft5x06_reset_store);

static ssize_t ft5x06_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(to_i2c_client(dev));

	switch (gpio_get_value(data->pdata->irq_gpio)) {
	case 0:
		return scnprintf(buf, PAGE_SIZE, "Low\n");
	case 1:
		return scnprintf(buf, PAGE_SIZE, "High\n");
	default:
		pr_err("Failed to get GPIO for irq %d\n", data->client->irq);
		return scnprintf(buf, PAGE_SIZE, "Unknown\n");
	}
}
static DEVICE_ATTR(hw_irqstat, 0444, ft5x06_hw_irqstat_show, NULL);

static ssize_t ft5x06_drv_irq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		data->irq_enabled ? "ENABLED" : "DISABLED");
}

static ssize_t ft5x06_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value = 0;
	int err = 0;
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		pr_err("Failed to convert value\n");
		return -EINVAL;
	}

	switch (value) {
	case 0:
		/* Disable irq */
		if (data->irq_enabled) {
			disable_irq(data->client->irq);
			data->irq_enabled = false;
		}
		break;
	case 1:
		/* Enable irq */
		if (!data->irq_enabled) {
			enable_irq(data->client->irq);
			data->irq_enabled = true;
		}
		break;
	default:
		pr_err("Invalid value\n");
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR(drv_irq, 0664, ft5x06_drv_irq_show, ft5x06_drv_irq_store);

static ssize_t ts_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	strlcpy(buf, ts_info_buff, FT_INFO_MAX_LEN);
	return strnlen(buf, FT_INFO_MAX_LEN);
}
static struct kobj_attribute ts_info_attr = __ATTR_RO(ts_info);

#include <linux/major.h>
#include <linux/kdev_t.h>

/* Attribute: path (RO) */
static ssize_t path_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ft5x06_ts_data *data = dev_get_drvdata(dev);
	ssize_t blen;
	const char *path;

	if (!data) {
		pr_err("cannot get ft_data pointer\n");
		return (ssize_t)0;
	}
	path = kobject_get_path(&data->client->dev.kobj, GFP_KERNEL);
	blen = scnprintf(buf, PAGE_SIZE, "%s", path ? path : "na");
	kfree(path);
	return blen;
}

/* Attribute: vendor (RO) */
static ssize_t vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "focaltech");
}

static struct device_attribute touchscreen_attributes[] = {
	__ATTR_RO(path),
	__ATTR_RO(vendor),
	__ATTR_NULL
};

#define TSDEV_MINOR_BASE 128
#define TSDEV_MINOR_MAX 32

static int ft5x06_ts_sysfs_class(void *_data, bool create)
{
	struct ft5x06_ts_data *data = _data;
	const struct ft5x06_ts_platform_data *pdata = data->pdata;
	struct device_attribute *attrs = touchscreen_attributes;
	int i, error = 0;
	static struct class *touchscreen_class;
	static struct device *ts_class_dev;
	static int minor;

	if (create) {
		minor = input_get_new_minor(data->client->addr,
						1, false);
		if (minor < 0)
			minor = input_get_new_minor(TSDEV_MINOR_BASE,
					TSDEV_MINOR_MAX, true);
		pr_info("assigned minor %d\n", minor);

		touchscreen_class = class_create(THIS_MODULE, "touchscreen");
		if (IS_ERR(touchscreen_class)) {
			error = PTR_ERR(touchscreen_class);
			touchscreen_class = NULL;
			return error;
		}

		ts_class_dev = device_create(touchscreen_class, NULL,
				MKDEV(INPUT_MAJOR, minor),
				data, pdata->name);
		if (IS_ERR(ts_class_dev)) {
			error = PTR_ERR(ts_class_dev);
			ts_class_dev = NULL;
			return error;
		}

		for (i = 0; attrs[i].attr.name != NULL; ++i) {
			error = device_create_file(ts_class_dev, &attrs[i]);
			if (error)
				break;
		}

		if (error)
			goto device_destroy;
	} else {
		if (!touchscreen_class || !ts_class_dev)
			return -ENODEV;

		for (i = 0; attrs[i].attr.name != NULL; ++i)
			device_remove_file(ts_class_dev, &attrs[i]);

		device_unregister(ts_class_dev);
		class_unregister(touchscreen_class);
	}

	return 0;

device_destroy:
	for (--i; i >= 0; --i)
		device_remove_file(ts_class_dev, &attrs[i]);
	device_destroy(touchscreen_class, MKDEV(INPUT_MAJOR, minor));
	ts_class_dev = NULL;
	class_unregister(touchscreen_class);
	pr_err("error creating touchscreen class\n");

	return -ENODEV;
}

static bool ft5x06_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0xFF) {
		pr_err("FT reg address is invalid: 0x%x\n", addr);
		return false;
	}

	return true;
}

static int ft5x06_debug_data_set(void *_data, u64 val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft5x06_debug_addr_is_valid(data->addr))
		dev_info(&data->client->dev,
			"Writing into FT registers not supported\n");

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft5x06_debug_data_get(void *_data, u64 *val)
{
	struct ft5x06_ts_data *data = _data;
	int rc;
	u8 reg;

	mutex_lock(&data->input_dev->mutex);

	if (ft5x06_debug_addr_is_valid(data->addr)) {
		rc = ft5x0x_read_reg(data->client, data->addr, &reg);
		if (rc < 0)
			dev_err(&data->client->dev,
				"FT read register 0x%x failed (%d)\n",
				data->addr, rc);
		else
			*val = reg;
	}

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, ft5x06_debug_data_get,
			ft5x06_debug_data_set, "0x%02llX\n");

static int ft5x06_debug_addr_set(void *_data, u64 val)
{
	struct ft5x06_ts_data *data = _data;

	if (ft5x06_debug_addr_is_valid(val)) {
		mutex_lock(&data->input_dev->mutex);
		data->addr = val;
		mutex_unlock(&data->input_dev->mutex);
	}

	return 0;
}

static int ft5x06_debug_addr_get(void *_data, u64 *val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft5x06_debug_addr_is_valid(data->addr))
		*val = data->addr;

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, ft5x06_debug_addr_get,
			ft5x06_debug_addr_set, "0x%02llX\n");

static int ft5x06_debug_suspend_set(void *_data, u64 val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (val)
		ft5x06_ts_suspend(&data->client->dev);
	else
		ft5x06_ts_resume(&data->client->dev);

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft5x06_debug_suspend_get(void *_data, u64 *val)
{
	struct ft5x06_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);
	*val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, ft5x06_debug_suspend_get,
			ft5x06_debug_suspend_set, "%lld\n");

static int ft5x06_debug_dump_info(struct seq_file *m, void *v)
{
	struct ft5x06_ts_data *data = m->private;

	seq_printf(m, "%s\n", data->ts_info);

	return 0;
}

static int debugfs_dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ft5x06_debug_dump_info, inode->i_private);
}

static const struct file_operations debug_dump_info_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_dump_info_open,
	.read		= seq_read,
	.release	= single_release,
};

static ssize_t ft5x06_proc_read(struct file *filp, char __user *buff,
				size_t count, loff_t *ppos)
{
	unsigned char *read_buf = NULL;
	size_t read_buf_len = (count > PAGE_SIZE) ? count : PAGE_SIZE;
	u8 regval = 0x00;
	u8 regaddr = 0x00;
	struct ft5x06_ts_data *data = PDE_DATA(filp->f_inode);
	int num_read_chars = 0;
	int retval;

	if (NULL == data) {
		pr_err("%s: cannot get ts data\n", __func__);
		return -EFAULT;
	}
	read_buf = kmalloc(read_buf_len, GFP_KERNEL);
	if (NULL == read_buf)
		return -ENOMEM;

	mutex_lock(&data->input_dev->mutex);
	switch (data->proc_operate_mode) {
	case PROC_UPGRADE:
		regaddr = FT_REG_FW_VER;
		retval = ft5x0x_read_reg(data->client,
					regaddr, &regval);
		if (retval < 0)
			num_read_chars = snprintf(read_buf, read_buf_len,
					"get fw version failed.\n");
		else
			num_read_chars = snprintf(read_buf, read_buf_len,
					"current fw version:0x%02x\n", regval);
		break;
	case PROC_READ_REGISTER:
		retval = ft5x06_i2c_read(data->client, NULL, 0, read_buf, 1);
		if (retval < 0)
			dev_err(&data->client->dev,
					"%s: read I2C error\n", __func__);
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		retval = ft5x06_i2c_read(data->client,
					NULL, 0, read_buf, count);
		if (retval < 0)
			dev_err(&data->client->dev,
					"%s: read I2C error\n", __func__);
		num_read_chars = count;
		break;
	default:
		break;
	}

	mutex_unlock(&data->input_dev->mutex);

	if (copy_to_user(buff, read_buf, num_read_chars)) {
		dev_err(&data->client->dev,
			"%s: copy to user error\n", __func__);
		kfree(read_buf);
		return -EFAULT;
	}
	kfree(read_buf);

	return num_read_chars;
}

static ssize_t ft5x06_proc_write(struct file *filp, const char __user *buff,
				size_t count, loff_t *ppos)
{
	unsigned char *write_buf = NULL;
	size_t write_buf_len = count;
	struct ft5x06_ts_data *data = PDE_DATA(filp->f_inode);
	int writelen = 0;
	int retval;

	if (NULL == data) {
		pr_err("%s: cannot get ts data\n", __func__);
		return -EFAULT;
	}
	write_buf = kmalloc(write_buf_len, GFP_KERNEL);
	if (NULL == write_buf)
		return -ENOMEM;

	if (copy_from_user(write_buf, buff, count)) {
		dev_err(&data->client->dev,
			"%s: copy from user error\n", __func__);
		kfree(write_buf);
		return -EFAULT;
	}

	mutex_lock(&data->input_dev->mutex);
	data->proc_operate_mode = write_buf[0];
	switch (data->proc_operate_mode) {
	case PROC_READ_REGISTER:
		retval = ft5x06_i2c_write(data->client, write_buf + 1, 1);
		if (retval < 0)
			dev_err(&data->client->dev,
				"%s: write I2C error\n", __func__);
		break;
	case PROC_WRITE_REGISTER:
		retval = ft5x06_i2c_write(data->client, write_buf + 1, 2);
		if (retval < 0)
			dev_err(&data->client->dev,
				"%s: write I2C error\n", __func__);
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = count - 1;
		if (writelen > 0) {
			retval = ft5x06_i2c_write(data->client,
					write_buf + 1, writelen);
			if (retval < 0)
				dev_err(&data->client->dev,
					"%s: write I2C error\n", __func__);
		}
		break;
	default:
		dev_err(&data->client->dev,
				"%s: unsupport opt %d\n",
				__func__, data->proc_operate_mode);
		break;
	}

	mutex_unlock(&data->input_dev->mutex);
	kfree(write_buf);

	return count;
}

static const struct file_operations ft5x06_proc_ops = {
	.owner		= THIS_MODULE,
	.read		= ft5x06_proc_read,
	.write		= ft5x06_proc_write
};

static int ft5x06_create_proc_entry(struct ft5x06_ts_data *data)
{
	struct proc_dir_entry *entry = data->proc_entry;
	struct i2c_client *client = data->client;

	entry = proc_create_data(FT_PROC_DIR_NAME,
			0664,
			NULL,
			&ft5x06_proc_ops,
			data);
	if (NULL == entry) {
		dev_err(&client->dev, "Couldn't create proc entry\n");
		return -ENOMEM;
	}
	dev_info(&client->dev, "Create proc entry success\n");

	return 0;
}

static void ft5x06_remove_proc_entry(struct ft5x06_ts_data *data)
{
	proc_remove(data->proc_entry);
}

#ifdef CONFIG_OF
static int ft5x06_get_dt_coords(struct device *dev, char *name,
				struct ft5x06_ts_platform_data *pdata)
{
	u32 coords[FT_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FT_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "focaltech,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "focaltech,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

/* ASCII names order MUST match enum */
static const char const *ascii_names[] = {"charger", "fps", "na"};

static int ft5x06_modifier_name2id(const char *name)
{
	int i, len2cmp, chosen = -1;

	for (i = 0; i < FT_MOD_MAX; i++) {
		len2cmp = min_t(int, strlen(name), strlen(ascii_names[i]));
		if (!strncmp(name, ascii_names[i], len2cmp)) {
			chosen = i;
			break;
		}
	}
	return chosen;
}

static void ft5x06_dt_parse_modifier(struct ft5x06_ts_data *data,
		struct device_node *parent, struct config_modifier *config,
		const char *modifier_name, bool active)
{
	struct device *dev = &data->client->dev;
	char node_name[64];
	struct ft5x06_clip_area clipa;
	struct device_node *np_config;
	int err;

	scnprintf(node_name, 63, "%s-%s", modifier_name,
		active ? "active" : "suspended");
	np_config = of_find_node_by_name(parent, node_name);
	if (!np_config) {
		dev_dbg(dev, "%s: node does not exist\n", node_name);
		return;
	}

	err = of_property_read_u32_array(np_config, "touch-clip-area",
		(unsigned int *)&clipa, sizeof(clipa)/sizeof(unsigned int));
	if (!err) {
		config->clipa = kzalloc(sizeof(clipa), GFP_KERNEL);
		if (!config->clipa) {
			dev_err(dev, "clip area allocation failure\n");
			return;
		}
		memcpy(config->clipa, &clipa, sizeof(clipa));
		pr_notice("using touch clip area in %s\n", node_name);
	}

	of_node_put(np_config);
}

static int ft5x06_dt_parse_modifiers(struct ft5x06_ts_data *data)
{
	struct device *dev = &data->client->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_mod;
	int i, num_names, ret = 0;
	char node_name[64];
	static const char **modifiers_names;

	sema_init(&data->modifiers.list_sema, 1);
	INIT_LIST_HEAD(&data->modifiers.mod_head);
	data->modifiers.mods_num = 0;

	num_names = of_property_count_strings(np, "config_modifier-names");
	if (num_names < 0) {
		dev_err(dev, "Cannot parse config_modifier-names: %d\n",
			num_names);
		return -ENODEV;
	}

	modifiers_names = devm_kzalloc(dev,
			sizeof(*modifiers_names) * num_names, GFP_KERNEL);
	if (!modifiers_names)
		return -ENOMEM;

	for (i = 0; i < num_names; i++) {
		ret = of_property_read_string_index(np, "config_modifier-names",
			i, &modifiers_names[i]);
		if (ret < 0) {
			dev_err(dev, "Cannot parse modifier-names: %d\n", ret);
			return ret;
		}
	}

	data->modifiers.mods_num = num_names;

	for (i = 0; i < num_names; i++) {
		int id;
		struct config_modifier *cm, *config;

		scnprintf(node_name, 63, "config_modifier-%s",
				modifiers_names[i]);
		np_mod = of_find_node_by_name(np, node_name);
		if (!np_mod) {
			dev_warn(dev, "cannot find modifier node %s\n",
				node_name);
			continue;
		}

		/* check for duplicate nodes in devtree */
		id = ft5x06_modifier_name2id(modifiers_names[i]);
		dev_info(dev, "processing modifier %s[%d]\n", node_name, id);
		list_for_each_entry(cm, &data->modifiers.mod_head, link) {
			if (cm->id == id) {
				dev_err(dev, "duplicate modifier node %s\n",
					node_name);
				return -EFAULT;
			}
		}
		/* allocate modifier's structure */
		config = devm_kzalloc(dev, sizeof(*config), GFP_KERNEL);
		if (!config)
			return -ENOMEM;

		list_add_tail(&config->link, &data->modifiers.mod_head);
		config->name = modifiers_names[i];
		config->id = id;

		if (of_property_read_bool(np_mod, "enable-notification")) {
			switch (id) {
			case FT_MOD_CHARGER:
				pr_notice("using charger detection\n");
				data->charger_detection_enabled = true;
				break;
			case FT_MOD_FPS:
				pr_notice("sing fingerprint sensor detection\n");
				data->fps_detection_enabled = true;
				break;
			default:
				pr_notice("no notification found\n");
				break;
			}
		} else {
			config->effective = true;
			dev_dbg(dev, "modifier %s enabled unconditionally\n",
					node_name);
		}

		dev_dbg(dev, "processing modifier %s[%d]\n",
			node_name, config->id);

		ft5x06_dt_parse_modifier(data, np_mod, config,
				modifiers_names[i], true);
		ft5x06_dt_parse_modifier(data, np_mod, config,
				modifiers_names[i], false);

		of_node_put(np_mod);
	}

	return 0;
}

static int ft5x06_parse_dt(struct device *dev,
			struct ft5x06_ts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val, num_buttons;
	u32 button_map[MAX_BUTTONS];

	pdata->name = "focaltech";
	rc = of_property_read_string(np, "focaltech,name", &pdata->name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read name\n");
		return rc;
	}

	rc = ft5x06_get_dt_coords(dev, "focaltech,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = ft5x06_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (rc)
		return rc;

	pdata->i2c_pull_up = of_property_read_bool(np,
						"focaltech,i2c-pull-up");

	pdata->no_force_update = of_property_read_bool(np,
						"focaltech,no-force-update");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	pdata->fw_name = "ft_fw.bin";
	rc = of_property_read_string(np, "focaltech,fw-name", &pdata->fw_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
		return rc;
	}

	rc = of_property_read_u32(np, "focaltech,group-id", &temp_val);
	if (!rc)
		pdata->group_id = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,hard-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->hard_rst_dly = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,soft-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->soft_rst_dly = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,num-max-touches", &temp_val);
	if (!rc)
		pdata->num_max_touches = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,fw-delay-aa-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay aa\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_aa =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-55-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay 55\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_55 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-upgrade-id1", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw upgrade id1\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.upgrade_id_1 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-upgrade-id2", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw upgrade id2\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.upgrade_id_2 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-readid-ms",
							&temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay read id\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_readid =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-era-flsh-ms",
							&temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay erase flash\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_erase_flash =  temp_val;

	pdata->info.auto_cal = of_property_read_bool(np,
					"focaltech,fw-auto-cal");

	pdata->fw_vkey_support = of_property_read_bool(np,
						"focaltech,fw-vkey-support");

	pdata->ignore_id_check = of_property_read_bool(np,
						"focaltech,ignore-id-check");

	pdata->gesture_support = of_property_read_bool(np,
						"focaltech,gesture-support");

	pdata->resume_in_workqueue = of_property_read_bool(np,
					"focaltech,resume-in-workqueue");

	rc = of_property_read_u32(np, "focaltech,family-id", &temp_val);
	if (!rc)
		pdata->family_id = temp_val;
	else
		return rc;

	prop = of_find_property(np, "focaltech,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);
		if (num_buttons > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
			"focaltech,button-map", button_map,
			num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
	}

	return 0;
}
#else
static int ft5x06_parse_dt(struct device *dev,
			struct ft5x06_ts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

void ft5x06_ts_shutdown(struct i2c_client *client)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(client);
	int retval;

	if (!data->is_tp_registered)
		return;
	free_irq(client->irq, data);

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		retval = gpio_direction_output(data->pdata->reset_gpio, 0);
		if (retval) {
			dev_err(&data->client->dev,
			"set_direction for reset gpio failed\n");
		}
		msleep(data->pdata->hard_rst_dly);
		gpio_free(data->pdata->reset_gpio);
	}
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	if (data->pdata->power_on)
		data->pdata->power_on(false);
	else
		ft5x06_power_on(data, false);

	if (data->pdata->power_init)
		data->pdata->power_init(false);
	else
		ft5x06_power_init(data, false);
}
static int ft5x06_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ft5x06_ts_platform_data *pdata;
	struct ft5x06_gesture_platform_data *gesture_pdata;
	struct ft5x06_ts_data *data;
	struct input_dev *input_dev;
	struct dentry *temp;
	u8 reg_value;
	u8 reg_addr;
	int err, len, retval, attr_count;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ft5x06_ts_platform_data), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		err = ft5x06_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "DT parsing failed\n");
			return err;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev,
			sizeof(struct ft5x06_ts_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (pdata->fw_name) {
		len = strlen(pdata->fw_name);
		if (len > FT_FW_NAME_MAX_LEN - 1) {
			dev_err(&client->dev, "Invalid firmware name\n");
			return -EINVAL;
		}

		strlcpy(data->fw_name, pdata->fw_name, len + 1);
	}

	data->tch_data_len = FT_TCH_LEN(pdata->num_max_touches);
	data->tch_data = devm_kzalloc(&client->dev,
				data->tch_data_len, GFP_KERNEL);
	if (!data->tch_data)
		return -ENOMEM;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	data->input_dev = input_dev;
	data->client = client;
	data->pdata = pdata;
	data->force_reflash = (data->pdata->no_force_update) ? false : true;
	data->flash_enabled = true;
	data->irq_enabled = false;
	data->is_tp_registered = false;

	data->patching_enabled = true;
	err = ft5x06_dt_parse_modifiers(data);
	if (err)
		data->patching_enabled = false;

	input_dev->name = "ft5x06_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, pdata->num_max_touches, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min,
			     pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min,
			     pdata->y_max, 0, 0);

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev, "Input device registration failed\n");
		input_free_device(input_dev);
		return err;
	}

	if (pdata->power_init) {
		err = pdata->power_init(true);
		if (err) {
			dev_err(&client->dev, "power init failed");
			goto unreg_inputdev;
		}
	} else {
		err = ft5x06_power_init(data, true);
		if (err) {
			dev_err(&client->dev, "power init failed");
			goto unreg_inputdev;
		}
	}

	err = ft5x06_ts_pinctrl_init(data);
	if (!err && data->ts_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0) {
			dev_err(&client->dev,
				"failed to select pin to active state");
		}
	}

	err = ft5x06_gpio_configure(data, true);
	if (err < 0) {
		dev_err(&client->dev,
			"Failed to configure the gpios\n");
		goto err_gpio_req;
	}
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		usleep_range(800, 1000);
		if (data->pdata->power_on) {
			err = data->pdata->power_on(true);
			if (err) {
				dev_err(&client->dev, "power on failed");
				goto free_gpio;
			}
		} else {
			err = ft5x06_power_on(data, true);
			if (err) {
				dev_err(&client->dev, "power on failed");
				goto free_gpio;
			}
		}
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}

	/* make sure CTP already finish startup process */
	msleep(data->pdata->soft_rst_dly);

	/* check the controller id */
	reg_addr = FT_REG_ID;
	err = ft5x06_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0) {
		dev_err(&client->dev, "version read failed");
		goto power_off;
	}

	dev_info(&client->dev, "Device ID = 0x%x\n", reg_value);

	if ((pdata->family_id != reg_value) && (!pdata->ignore_id_check)) {
		dev_err(&client->dev, "%s:Unsupported controller\n", __func__);
		goto power_off;
	}

	data->family_id = pdata->family_id;

	err = request_threaded_irq(client->irq, NULL,
				ft5x06_ts_interrupt,
	/*
	* the interrupt trigger mode will be set in Device Tree with property
	* "interrupts", so here we just need to set the flag IRQF_ONESHOT
	*/
				IRQF_ONESHOT,
				client->dev.driver->name, data);
	if (err) {
		dev_err(&client->dev, "request irq failed\n");
		goto power_off;
	}

	/* request_threaded_irq enable irq,so set the flag irq_enabled */
	data->irq_enabled = true;

	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support) {
		device_init_wakeup(&client->dev, 1);
		gesture_pdata = devm_kzalloc(&client->dev,
				sizeof(struct ft5x06_gesture_platform_data),
				GFP_KERNEL);
		if (!gesture_pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			goto free_gesture_dev;
		}
		data->gesture_pdata = gesture_pdata;
		gesture_pdata->data = data;

		gesture_pdata->gesture_class =
					class_create(THIS_MODULE, "gesture");
		if (IS_ERR(gesture_pdata->gesture_class)) {
			err = PTR_ERR(gesture_pdata->gesture_class);
			dev_err(&client->dev, "Failed to create class.\n");
			goto free_gesture_pdata;
		}

		gesture_pdata->dev = device_create(gesture_pdata->gesture_class,
				NULL, 0, NULL, "gesture_ft5x06");
		if (IS_ERR(gesture_pdata->dev)) {
			err = PTR_ERR(gesture_pdata->dev);
			dev_err(&client->dev, "Failed to create device.\n");
			goto free_gesture_class;
		}

		dev_set_drvdata(gesture_pdata->dev, data);
		err = device_create_file(gesture_pdata->dev,
					&dev_attr_enable);
		if (err) {
			dev_err(gesture_pdata->dev,
					"sys file creation failed\n");
			goto free_gesture_dev;
		}
		err = device_create_file(gesture_pdata->dev,
					&dev_attr_pocket);
		if (err) {
			dev_err(gesture_pdata->dev,
					"sys file creation failed\n");
			goto free_enable_sys;
		}
	}

	err = ft5x06_ts_sysfs_class(data, true);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_class_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_buildid);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_buildid_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_doreflash);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_doreflash_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_flashprog);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_flashprog_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_forcereflash);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_forcereflash_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_productinfo);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_productinfo_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_poweron);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_poweron_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_fw_name);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_pocket_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_update_fw);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_fw_name_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_force_update_fw);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_update_fw_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_drv_irq);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_force_update_fw_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_reset);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_drv_irq_sys;
	}

	err = device_create_file(&client->dev, &dev_attr_hw_irqstat);
	if (err) {
		dev_err(&client->dev, "sys file creation failed\n");
		goto free_reset_sys;
	}

	data->dir = debugfs_create_dir(FT_DEBUG_DIR_NAME, NULL);
	if (data->dir == NULL || IS_ERR(data->dir)) {
		pr_err("debugfs_create_dir failed(%ld)\n", PTR_ERR(data->dir));
		err = PTR_ERR(data->dir);
		goto free_hw_irqstat_sys;
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("dump_info", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_dump_info_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	err = ft5x06_create_proc_entry(data);
	if (err) {
		dev_err(&client->dev, "create proc entry failed");
		goto free_debug_dir;
	}

	data->ts_info = devm_kzalloc(&client->dev,
				FT_INFO_MAX_LEN, GFP_KERNEL);
	if (!data->ts_info)
		goto free_proc_entry;

	/*get some register information */
	reg_addr = FT_REG_POINT_RATE;
	ft5x06_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0)
		dev_err(&client->dev, "report rate read failed");

	dev_info(&client->dev, "report rate = %dHz\n", reg_value * 10);

	reg_addr = FT_REG_THGROUP;
	err = ft5x06_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0)
		dev_err(&client->dev, "threshold read failed");

	dev_dbg(&client->dev, "touch threshold = %d\n", reg_value * 4);

	/*creation touch panel info kobj*/
	data->ts_info_kobj = kobject_create_and_add(FT_TS_INFO_SYSFS_DIR_NAME,
					kernel_kobj);
	if (!data->ts_info_kobj) {
		dev_err(&client->dev, "kobject creation failed.\n");
	} else {
		err = sysfs_create_file(data->ts_info_kobj, &ts_info_attr.attr);
		if (err) {
			kobject_put(data->ts_info_kobj);
			dev_err(&client->dev, "sysfs creation failed.\n");
		} else {
			ts_info_buff = devm_kzalloc(&client->dev,
						 FT_INFO_MAX_LEN, GFP_KERNEL);
			if (!ts_info_buff)
				goto free_proc_entry;
		}
	}

	/*Initialize secure touch */
	ft5x06_secure_touch_init(data);
	ft5x06_secure_touch_stop(data, true);
	mutex_init(&(data->ft_clk_io_ctrl_mutex));

	/* Creation of secure touch sysfs files */
	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&data->input_dev->dev.kobj,
						&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&client->dev,
				"%s: Failed to create sysfs attributes\n",
							__func__);
			goto free_secure_touch_sysfs;
		}
	}

	ft5x06_update_fw_ver(data);
	ft5x06_update_fw_vendor_id(data);
	ft5x06_update_fw_id(data);

	FT_STORE_TS_DBG_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);
	FT_STORE_TS_INFO(ts_info_buff, data->family_id, data->fw_ver[0],
			 data->fw_ver[1], data->fw_ver[2]);
#if defined(CONFIG_FB)
	INIT_WORK(&data->fb_notify_work, fb_notify_resume_work);
	data->fb_notif.notifier_call = fb_notifier_callback;

	err = fb_register_client(&data->fb_notif);

	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
			err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						    FT_SUSPEND_LEVEL;
	data->early_suspend.suspend = ft5x06_ts_early_suspend;
	data->early_suspend.resume = ft5x06_ts_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	if (data->charger_detection_enabled) {
		struct power_supply *psy = NULL;

		INIT_WORK(&data->ps_notify_work, ps_notify_callback_work);
		data->ps_notif.notifier_call = ps_notify_callback;
		err = power_supply_reg_notifier(&data->ps_notif);
		if (err) {
			dev_err(&client->dev,
				"Unable to register ps_notifier: %d\n", err);
			goto free_fb_notifier;
		}

		/* if power supply supplier registered brfore TP
		ps_notify_callback will not receive PSY_EVENT_PROP_ADDED
		event, and will cause miss to set TP into charger state.
		So check PS state in probe */
		psy = power_supply_get_by_name("usb");
		if (psy) {
			retval = ps_get_state(data, psy, &data->ps_is_present);
			if (retval) {
				dev_err(&client->dev,
					"psy get property failed rc=%d\n",
					retval);
				goto free_fb_notifier;
			}

			retval = ft5x06_set_charger_state(data,
							data->ps_is_present);
			if (retval) {
				dev_err(&client->dev,
					"set charger state failed rc=%d\n",
					retval);
				goto free_fb_notifier;
			}

			ft5x06_update_ps_chg_cm_state(data,
						data->ps_is_present);
		}
	}

	exp_fn_ctrl.ft5x06_ts_data_ptr = data;
	if (data->fps_detection_enabled) {
		data->fps_notif.notifier_call = fps_notifier_callback;
		dev_dbg(&client->dev, "registering FPS notifier\n");
		retval = FPS_register_notifier(
				&data->fps_notif, 0xBEEF, false);
		if (retval) {
			exp_fn_ctrl.det_workqueue =
			create_singlethread_workqueue("ft5x06_det_workqueue");
			if (IS_ERR_OR_NULL(exp_fn_ctrl.det_workqueue)) {
				pr_err("unable to create a workqueue\n");
			} else {
				INIT_DELAYED_WORK(&exp_fn_ctrl.det_work,
					ft5x06_detection_work);
				queue_delayed_work(exp_fn_ctrl.det_workqueue,
					&exp_fn_ctrl.det_work,
					msecs_to_jiffies(EXP_FN_DET_INTERVAL));
			}
			dev_err(&client->dev,
				"Failed to register fps_notifier: %d\n",
				retval);
			retval = 0;
		} else
			data->is_fps_registered = true;
	}
	data->is_tp_registered = true;
	return 0;

free_fb_notifier:
#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif
free_secure_touch_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&data->input_dev->dev.kobj,
					&attrs[attr_count].attr);
	}
free_proc_entry:
	ft5x06_remove_proc_entry(data);
free_debug_dir:
	debugfs_remove_recursive(data->dir);
free_hw_irqstat_sys:
	device_remove_file(&client->dev, &dev_attr_hw_irqstat);
free_reset_sys:
	device_remove_file(&client->dev, &dev_attr_reset);
free_drv_irq_sys:
	device_remove_file(&client->dev, &dev_attr_drv_irq);
free_force_update_fw_sys:
	device_remove_file(&client->dev, &dev_attr_force_update_fw);
free_update_fw_sys:
	device_remove_file(&client->dev, &dev_attr_update_fw);
free_fw_name_sys:
	device_remove_file(&client->dev, &dev_attr_fw_name);
free_pocket_sys:
	device_remove_file(&client->dev, &dev_attr_poweron);
free_poweron_sys:
	device_remove_file(&client->dev, &dev_attr_productinfo);
free_productinfo_sys:
	device_remove_file(&client->dev, &dev_attr_forcereflash);
free_forcereflash_sys:
	device_remove_file(&client->dev, &dev_attr_flashprog);
free_flashprog_sys:
	device_remove_file(&client->dev, &dev_attr_doreflash);
free_doreflash_sys:
	device_remove_file(&client->dev, &dev_attr_buildid);
free_buildid_sys:
	ft5x06_ts_sysfs_class(data, false);
free_class_sys:
	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support)
		device_remove_file(&client->dev, &dev_attr_pocket);
free_enable_sys:
	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support)
		device_remove_file(&client->dev, &dev_attr_enable);
free_gesture_dev:
	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support)
		device_destroy(gesture_pdata->gesture_class, 0);
free_gesture_class:
	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support)
		class_destroy(gesture_pdata->gesture_class);
free_gesture_pdata:
	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support) {
		devm_kfree(&client->dev, gesture_pdata);
		data->gesture_pdata = NULL;
	}
power_off:
	if (pdata->power_on)
		pdata->power_on(false);
	else
		ft5x06_power_on(data, false);
free_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_gpio_req:
	if (data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->ts_pinctrl);
			data->ts_pinctrl = NULL;
		} else {
			err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_release);
			if (err)
				pr_err("failed to select relase pinctrl state\n");
		}
	}
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft5x06_power_init(data, false);
unreg_inputdev:
	input_unregister_device(input_dev);
	return err;
}

static int ft5x06_ts_remove(struct i2c_client *client)
{
	struct ft5x06_ts_data *data = i2c_get_clientdata(client);
	int retval, attr_count;

	if (ft5x06_gesture_support_enabled() && data->pdata->gesture_support) {
		device_init_wakeup(&client->dev, 0);
		device_remove_file(&client->dev, &dev_attr_pocket);
		device_remove_file(&client->dev, &dev_attr_enable);
		device_destroy(data->gesture_pdata->gesture_class, 0);
		class_destroy(data->gesture_pdata->gesture_class);
		devm_kfree(&client->dev, data->gesture_pdata);
		data->gesture_pdata = NULL;
	}

	ft5x06_ts_sysfs_class(data, false);
	ft5x06_remove_proc_entry(data);
	debugfs_remove_recursive(data->dir);
	device_remove_file(&client->dev, &dev_attr_force_update_fw);
	device_remove_file(&client->dev, &dev_attr_update_fw);
	device_remove_file(&client->dev, &dev_attr_fw_name);
	device_remove_file(&client->dev, &dev_attr_poweron);
	device_remove_file(&client->dev, &dev_attr_productinfo);
	device_remove_file(&client->dev, &dev_attr_forcereflash);
	device_remove_file(&client->dev, &dev_attr_flashprog);
	device_remove_file(&client->dev, &dev_attr_doreflash);
	device_remove_file(&client->dev, &dev_attr_buildid);
	device_remove_file(&client->dev, &dev_attr_drv_irq);
	device_remove_file(&client->dev, &dev_attr_reset);
	device_remove_file(&client->dev, &dev_attr_hw_irqstat);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif

	if (data->charger_detection_enabled)
		power_supply_unreg_notifier(&data->ps_notif);

	free_irq(client->irq, data);

	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	if (data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->ts_pinctrl);
			data->ts_pinctrl = NULL;
		} else {
			retval = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_release);
			if (retval < 0)
				pr_err("failed to select release pinctrl state\n");
		}
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&data->input_dev->dev.kobj,
					&attrs[attr_count].attr);
	}

	if (data->pdata->power_on)
		data->pdata->power_on(false);
	else
		ft5x06_power_on(data, false);

	if (data->pdata->power_init)
		data->pdata->power_init(false);
	else
		ft5x06_power_init(data, false);

	input_unregister_device(data->input_dev);
	kobject_put(data->ts_info_kobj);
	return 0;
}

static const struct i2c_device_id ft5x06_ts_id[] = {
	{"ft5x06_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ft5x06_ts_id);

#ifdef CONFIG_OF
static struct of_device_id ft5x06_match_table[] = {
	{ .compatible = "focaltech,5x06",},
	{ },
};
#else
#define ft5x06_match_table NULL
#endif

static struct i2c_driver ft5x06_ts_driver = {
	.probe = ft5x06_ts_probe,
	.remove = ft5x06_ts_remove,
	.shutdown = ft5x06_ts_shutdown,
	.driver = {
		   .name = "ft5x06_ts",
		   .owner = THIS_MODULE,
		.of_match_table = ft5x06_match_table,
#ifdef CONFIG_PM
		   .pm = &ft5x06_ts_pm_ops,
#endif
		   },
	.id_table = ft5x06_ts_id,
};

static int __init ft5x06_ts_init(void)
{
	return i2c_add_driver(&ft5x06_ts_driver);
}
module_init(ft5x06_ts_init);

static void __exit ft5x06_ts_exit(void)
{
	i2c_del_driver(&ft5x06_ts_driver);
}
module_exit(ft5x06_ts_exit);

MODULE_DESCRIPTION("FocalTech ft5x06 TouchScreen driver");
MODULE_LICENSE("GPL v2");
