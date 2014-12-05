#define pr_fmt(fmt) "aps_ts: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <linux/ktime.h>
#include "aps_ts.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS) && defined(CONFIG_FB)
#include <mach/mmi_panel_notifier.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define DRIVER_NAME "aps_ts"
#define INPUT_PHYS_NAME "aps_ts/input0"

#define RESET_DELAY_USEC	2000
#define UI_BOOT_STEP_MSEC	20
#define UI_BOOT_MAX_STEPS	20

#define APS_I2C_RETRY_TIMES	10
#define APS_I2C_RETRY_DELAY_MS	10


/* Firmware file name */
#define FW_NAME			"aps_ts.fw"
#define MAX_FW_NAME_LEN		80

#define MAX_FINGER_NUM		10
#define FINGER_EVENT_SZ		8
#define MAX_WIDTH		30
#define MAX_PRESSURE		255

#define APS_REG_BOOTMODE		0x00B0
#define APS_REG_FW_VER			0x00C0
#define APS_REG_XY_RESOLUTION_HIGH	0x1002
#define APS_REG_EVENT_PKT_SZ		0x100F
#define APS_REG_EVENT_PKT		0x1010
#define APS_REG_PROD_INFO		0x10E0

#define APS_BOOTMODE_BOOTLOADER		0x11
#define APS_BOOTMODE_APPLICATION	0x22
#define APS_BOOTMODE_VERIFYING		0x33

#define APS_PRODUCT_ID_SIZE	4
#define APS_FW_VERSION_SIZE	4

/* Event types */
#define APS_ET_LOG		0xD
#define APS_ET_NOTIFY		0xE
#define APS_ET_ERROR		0xF

/* ISP mode */
#define ISP_ENTRY1		{ 0xAC, 0xCE, 0xFF, 0xFD, 0xAC, 0xCE }
#define ISP_ENTRY2		{ 0xAC, 0xCE, 0xFF, 0xFE, 0xAC, 0xCE }
#define ISP_ENTRY3		{ 0xAC, 0xCE, 0xFF, 0xFF, 0xAC, 0xCE }

#define ISP_UNLOCK1		{ 0x00, 0x04, 0xFE, 0xDC, 0xBA, 0x98 }
#define ISP_UNLOCK2		{ 0x00, 0x04, 0x76, 0x54, 0x32, 0x10 }
#define ISP_UNLOCK3		{ 0x00, 0x4C, 0x00, 0x00, 0x00, 0x01 }
#define ISP_LOCK		{ 0x00, 0x04, 0x05, 0xFA, 0x00, 0x00 }

#define ISP_CLK_20MHZ		{ 0x00, 0x4C, 0x05, 0xFA, 0x00, 0x07 }

#define ISP_MAX_RETRIES		1000

#define ISP_FULL_ERASE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x44 }
#define ISP_CLEAR_DONE		{ 0x00, 0x0C, 0x00, 0x00, 0x00, 0x20 }
#define ISP_WRITE_MODE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x01 }
#define ISP_WRITE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x41 }
#define ISP_WRITE_A		{ 0x00, 0x28, 0x00, 0x00, 0x00, 0x00 }
#define ISP_WRITE_B		{ 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00 }
#define ISP_READ_MODE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x08 }
#define ISP_READ		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x48 }
#define ISP_ADDRESS		{ 0x00, 0x14, 0x00, 0x00, 0x00, 0x00 }

#define ISP_REG_STATUS		0x000C
#define ISP_REG_READ_A		0x0030
#define ISP_REG_READ_B		0x0034

#define ISP_MODE_BIT		(1<<12)
#define ISP_DONE_BIT		(1<<5)

enum {
	SYS_INTENSITY = 2,
	SYS_ROWNUM,
	SYS_COLNUM,
	SYS_CLEAR,
	SYS_ENABLE,
	SYS_DISABLE,
};

enum {
	GET_COL_NUM	= 1,
	GET_ROW_NUM,
	GET_EVENT_DATA,
};

enum {
	LOG_TYPE_U08	= 2,
	LOG_TYPE_S08,
	LOG_TYPE_U16,
	LOG_TYPE_S16,
	LOG_TYPE_U32	= 8,
	LOG_TYPE_S32,
};

#define PERF_IRQ_MAX		64
#define PERF_IRQ_MAX_BITS	0x3F
#define PERF_DEBUG_SZ		64
struct aps_irq_perf {
	struct timespec start;
	struct timespec end;
	int finger_count;
	char debug[PERF_DEBUG_SZ];
};

struct aps_ts_info {
	struct i2c_client		*client;
	struct input_dev		*input_dev;

#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS) && defined(CONFIG_FB)
	struct mmi_notifier panel_nb;
	struct notifier_block fb_notif;
#elif defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	int				max_x;
	int				max_y;
	int				irq;
	struct aps_ts_platform_data	*pdata;
	struct completion		init_done;
	struct mutex			lock;
	bool				enabled;
	bool				irq_enabled;
	bool				in_bootloader;
	bool				input_registered;
	bool				high_speed_isp;
	u8				prod_id[APS_PRODUCT_ID_SIZE+1];
	u8				default_prod_id[APS_PRODUCT_ID_SIZE+1];
	u8				version[APS_FW_VERSION_SIZE];
	u8				bootmode;
	char				fw_name[MAX_FW_NAME_LEN];
	int				perf_irq_count;
	int				perf_irq_first;
	int				perf_irq_last;
	struct aps_irq_perf		*perf_irq_data;
	bool				force_reflash;
	u32				ui_ready_msec;
};

static int aps_i2c_read(struct aps_ts_info *info, u16 addr, void *buf, u16 len);
static int aps_query_device(struct aps_ts_info *info);
static void aps_ts_enable(struct aps_ts_info *info);
static void aps_ts_disable(struct aps_ts_info *info);
static void aps_clear_input_data(struct aps_ts_info *info);

static ssize_t aps_irq_perf_show(struct device *dev,
	struct device_attribute *attr, char *buf);

static ssize_t aps_drv_irq_show(struct device *dev,
	struct device_attribute *attr, char *buf);

static ssize_t aps_drv_irq_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

static ssize_t aps_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t aps_store_error(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

static DEVICE_ATTR(aps_perf, 0440, aps_irq_perf_show, NULL);

static DEVICE_ATTR(drv_irq, (S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP),
			aps_drv_irq_show,
			aps_drv_irq_store);

static DEVICE_ATTR(hw_irqstat, S_IRUSR | S_IRGRP,
			aps_hw_irqstat_show,
			aps_store_error);

static int timeval_sub(
	struct timespec *result, struct timespec x, struct timespec y)
{
	ktime_t kx = timespec_to_ktime(x);
	ktime_t ky = timespec_to_ktime(y);
	ktime_t kresult = ktime_sub(kx, ky);
	*result = ktime_to_timespec(kresult);

	return !(kresult.tv64 > 0);
}

static ssize_t aps_irq_perf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	struct aps_irq_perf *d = info->perf_irq_data;
	int offset = 0;
	int i;
	int prev_j;

	if (!d)
		return scnprintf(buf, PAGE_SIZE,
			"No performance info available.\n");

	offset += scnprintf(buf + offset, PAGE_SIZE - offset,
		"Prev  Delta  Start  End  Fingers  id x y maj press\n");

	for (i = 0; i < info->perf_irq_count && (PAGE_SIZE - offset) > 0; ++i) {
		struct timespec delta_prev = {0};
		struct timespec delta_irq;
		int j = (i + info->perf_irq_first) & PERF_IRQ_MAX_BITS;

		if (i != 0)
			timeval_sub(&delta_prev, d[j].start, d[prev_j].start);

		prev_j = j;

		timeval_sub(&delta_irq, d[j].end, d[j].start);
		offset += scnprintf(buf + offset, PAGE_SIZE - offset,
			"%4ld.%03ld  %03ld  %4ld.%03ld  %4ld.%03ld  %d %s\n",
			delta_prev.tv_sec%1000,
			delta_prev.tv_nsec/1000000,
			delta_irq.tv_nsec/1000000,
			d[j].start.tv_sec%1000,
			d[j].start.tv_nsec/1000000,
			d[j].end.tv_sec%1000,
			d[j].end.tv_nsec/1000000,
			d[j].finger_count,
			d[j].debug);
	}

	return offset;
}

static ssize_t aps_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			info->enabled ? "ENABLED" : "DISABLED");
}

static ssize_t aps_drv_irq_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	unsigned long value = 0;
	int err = 0;

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		pr_err("Failed to convert value.\n");
		return -EINVAL;
	}

	switch (value) {
	case 0:
		mutex_lock(&info->lock);
		aps_ts_disable(info);
		aps_clear_input_data(info);
		mutex_unlock(&info->lock);
		break;
	case 1:
		mutex_lock(&info->lock);
		aps_ts_enable(info);
		if (info->bootmode == 0)
			aps_query_device(info);
		mutex_unlock(&info->lock);
		break;
	default:
		pr_err("Invalid value\n");
	}

	return count;
}

static ssize_t aps_hw_irqstat_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);

	switch (gpio_get_value(info->pdata->gpio_irq)) {
	case 0:
		return scnprintf(buf, PAGE_SIZE, "Low\n");
	case 1:
		return scnprintf(buf, PAGE_SIZE, "High\n");
	default:
		pr_err("Failed to get GPIO value for irq GPIO %d.\n",
			info->pdata->gpio_irq);
		return scnprintf(buf, PAGE_SIZE, "Unknown\n");
	}

}

static ssize_t aps_store_error(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	pr_warn("Attempted to write to read-only attribute %s\n",
		attr->attr.name);
	return -EPERM;
}

static void aps_set_reset_low(struct aps_ts_info *info)
{
	pr_debug("reset to low\n");
	gpio_set_value(info->pdata->gpio_reset, 0);
	udelay(RESET_DELAY_USEC);
}

static void aps_reset(struct aps_ts_info *info)
{
	pr_debug("resetting...\n");
	gpio_set_value(info->pdata->gpio_reset, 0);
	udelay(RESET_DELAY_USEC);
	gpio_set_value(info->pdata->gpio_reset, 1);
	if (info->ui_ready_msec)
		msleep(info->ui_ready_msec);
	pr_debug("out of reset\n");
}

static void aps_ts_enable(struct aps_ts_info *info)
{
	if (info->enabled)
		return;

	info->enabled = true;

	if (info->input_dev) {
		enable_irq(info->irq);
		info->irq_enabled = true;
	}

	aps_reset(info);
}

static void aps_ts_disable(struct aps_ts_info *info)
{
	if (!info->enabled)
		return;

	if (info->irq_enabled) {
		disable_irq(info->irq);
		info->irq_enabled = false;
	}

	info->enabled = false;
}

static int aps_i2c_read(struct aps_ts_info *info, u16 addr, void *buf, u16 len)
{
	int ret;
	int retry;
	struct i2c_client *client = info->client;
	u8 cmd[2] = {addr >> 8, addr & 0x00FF};
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = cmd,
			.len = 2,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = len,
		},
	};

	for (retry = 0; retry < APS_I2C_RETRY_TIMES; ++retry) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (ret == 2) {
			ret = len;
			break;
		}
		dev_err(&client->dev, "I2C retry %d\n", retry + 1);
		msleep(APS_I2C_RETRY_DELAY_MS);
	}

	if (retry == APS_I2C_RETRY_TIMES) {
		dev_err(&client->dev,
			"I2C read failed(%d bytes at 0x%04x)\n",
			len, addr);
		ret = -EIO;
	}

	return ret;
}

static int aps_alloc_input(struct aps_ts_info *info)
{
	info->input_dev = input_allocate_device();
	if (IS_ERR_OR_NULL(info->input_dev))
		return PTR_ERR(info->input_dev);

	info->input_dev->name = DRIVER_NAME;
	info->input_dev->phys = INPUT_PHYS_NAME;
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->dev.parent = &info->client->dev;

	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);

	input_mt_init_slots(info->input_dev, MAX_FINGER_NUM);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
						0, MAX_WIDTH, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
						0, MAX_PRESSURE, 0, 0);
	input_set_drvdata(info->input_dev, info);

	pr_debug("allocated input device\n");
	return 0;
}

static void aps_clear_input_data(struct aps_ts_info *info)
{
	int i;

	if (info->input_dev) {
		for (i = 0; i < MAX_FINGER_NUM; i++) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev,
				MT_TOOL_FINGER,	false);
		}
		input_sync(info->input_dev);
	}

	return;
}

static void aps_report_input_data(struct aps_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	struct i2c_client *client = info->client;
	int id;
	int x;
	int y;
	int touch_major;
	int pressure;
	u8 *tmp;

	if (buf[0] == APS_ET_NOTIFY) {
		dev_info(&client->dev, "TSP mode changed (%d)\n", buf[1]);
		goto out;
	} else if (buf[0] == APS_ET_ERROR) {
		dev_info(&client->dev, "Error detected, restarting TSP\n");
		aps_clear_input_data(info);
		goto out;
	}

	for (i = 0; i < sz; i += FINGER_EVENT_SZ) {
		tmp = buf + i;
		id = (tmp[0] & 0xf) - 1;

		input_mt_slot(info->input_dev, id);

		if (!(tmp[0] & 0x80)) {
			input_mt_report_slot_state(info->input_dev,
				MT_TOOL_FINGER, false);
			dev_dbg(&client->dev, "id = %d, ", id);
			continue;
		}

		x = tmp[2] | ((tmp[1] & 0xf) << 8);
		y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
		touch_major = tmp[4];
		pressure = tmp[5];

		dev_dbg(&client->dev,
			"id = %d x = %d, y = %d, mjr = %d, pressure = %d",
			id, x, y, touch_major, pressure);

		if (info->perf_irq_data) {
			int last = info->perf_irq_last;
			scnprintf(info->perf_irq_data[last].debug,
				PERF_DEBUG_SZ, "%04d %04d %04d %03d %03d",
				id, x, y, touch_major, pressure);
		}

		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER,
			true);
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR,
			touch_major);
		input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
	}

	input_sync(info->input_dev);

out:
	return;
}

static irqreturn_t aps_ts_interrupt(int irq, void *data)
{
	struct aps_ts_info *info = data;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGER_NUM * FINGER_EVENT_SZ] = {0};
	int result;
	u8 packet_size = 0;
	int last = 0;

	if (info->perf_irq_data) {
		last = ++info->perf_irq_last;
		if (last >= PERF_IRQ_MAX)
			last = info->perf_irq_last = 0;

		if (info->perf_irq_count < PERF_IRQ_MAX)
			++info->perf_irq_count;
		else
			info->perf_irq_first = last + 1;

		getnstimeofday(&info->perf_irq_data[last].start);
	}

	result = aps_i2c_read(info, APS_REG_EVENT_PKT_SZ, &packet_size, 1);
	if (result <= 0) {
		dev_err(&client->dev, "Error reading event packet size\n");
		goto out;
	}

	if (packet_size == 0)
		goto out;

	packet_size = packet_size > sizeof(buf) ? sizeof(buf) : packet_size;

	result = aps_i2c_read(info, APS_REG_EVENT_PKT, buf, packet_size);
	if (result <= 0) {
		dev_err(&client->dev, "Error reading event packet %d bytes\n",
			packet_size);
		goto out;
	}

	aps_report_input_data(info, packet_size, buf);

out:
	if (info->perf_irq_data) {
		info->perf_irq_data[last].finger_count =
			packet_size/FINGER_EVENT_SZ;
		getnstimeofday(&info->perf_irq_data[last].end);
	}

	return IRQ_HANDLED;
}

static int aps_gpio_configure(struct aps_ts_info *data)
{
	struct device *dev = &data->client->dev;
	int error = -EINVAL;

	if (!gpio_is_valid(data->pdata->gpio_reset)) {
		dev_err(dev, "Invalid reset gpio\n");
		return error;
	}

	if (!gpio_is_valid(data->pdata->gpio_irq)) {
		dev_err(dev, "Invalid irq gpio\n");
		return error;
	}

	error = gpio_request(data->pdata->gpio_reset, "aps_reset_gpio");
	if (error) {
		dev_err(dev, "Unable to request gpio [%u]\n",
			data->pdata->gpio_reset);
		goto fail;
	}

	/* keep reset GPIO low */
	error = gpio_direction_output(data->pdata->gpio_reset, 0);
	if (error) {
		dev_err(dev, "Unable to set direction for gpio [%u]\n",
			data->pdata->gpio_reset);
		goto fail_release_reset_gpio;
	}

	error = gpio_request(data->pdata->gpio_irq, "aps_irq_gpio");
	if (error) {
		dev_err(dev, "Unable to request gpio [%u]\n",
			data->pdata->gpio_irq);
		goto fail_release_reset_gpio;
	}

	error = gpio_direction_input(data->pdata->gpio_irq);
	if (error) {
		dev_err(dev, "Unable to set direction for gpio [%u]\n",
			data->pdata->gpio_irq);
		goto fail_release_gpios;
	}

	return 0;

fail_release_gpios:
	gpio_free(data->pdata->gpio_irq);
fail_release_reset_gpio:
	gpio_free(data->pdata->gpio_reset);
fail:
	return error;
}

static int aps_ts_read_resolution(struct aps_ts_info *info)
{
	int ret;
	u8 num[3];

	ret = aps_i2c_read(info, APS_REG_XY_RESOLUTION_HIGH, &num,
		sizeof(num));

	if (ret <= 0) {
		pr_err("Failed to read resolution\n");
		return ret;
	}

	info->max_x = (num[0] & 0x0F) << 8 | num[1];
	info->max_y = (num[0] & 0xF0) << 4 | num[2];
	pr_debug("max_x = %d\n", info->max_x);
	pr_debug("max_y = %d\n", info->max_y);

	return 0;
}

static int aps_ts_config(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	ret = request_threaded_irq(client->irq, NULL, aps_ts_interrupt,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"aps_ts", info);
	if (ret) {
		dev_err(&client->dev, "Failed to register irq handler\n");
		goto out;
	}

	disable_irq(client->irq);
	barrier();
	complete_all(&info->init_done);
out:
	return ret;
}

static int aps_query_device(struct aps_ts_info *info)
{
	int ret, i;
	u8 bootmode;

	/* wait for UI mode when bootmode becomes readable */
	for (i = 0; i < UI_BOOT_MAX_STEPS; i++) {
		msleep(UI_BOOT_STEP_MSEC);
		ret = aps_i2c_read(info, APS_REG_BOOTMODE, &bootmode,
			sizeof(bootmode));
		if (ret <= 0) {
			pr_err("Failed to read boot mode\n");
			goto out_failure;
		}
		if (bootmode != 0)
			break;
	}
	pr_debug("bootmode: 0x%x waited: %d %d msec intervals\n",
		bootmode, i, UI_BOOT_STEP_MSEC);

	if (bootmode == 0) {
		/* Do not fail here if touch IC is unresponsive.	    */
		/* For example, bootloader did initialize the display and   */
		/* touch is not active. Will attempt to activate touch IC   */
		/* next time display is on. */
		goto out_touch_inactive;
	} else {
		info->bootmode = bootmode;
	}

	if (!info->input_dev) {
		ret = aps_alloc_input(info);
		if (ret) {
			pr_err("Error allocating input device\n");
			goto out_failure;
		}
	}

	info->in_bootloader = info->bootmode != APS_BOOTMODE_APPLICATION;
	if (info->in_bootloader)
		goto out_bootloader;

	ret = aps_i2c_read(info, APS_REG_FW_VER, info->version,
		APS_FW_VERSION_SIZE);
	if (ret <= 0) {
		pr_err("Failed to read FW version\n");
		goto out_failure;
	}

	ret = aps_i2c_read(info, APS_REG_PROD_INFO, info->prod_id,
		APS_PRODUCT_ID_SIZE);
	if (ret <= 0) {
		pr_err("Failed to read product id\n");
		goto out_failure;
	} else
		info->prod_id[APS_PRODUCT_ID_SIZE] = 0;

	ret = aps_ts_read_resolution(info);
	if (ret) {
		pr_err("Failed to read resolution\n");
		goto out_failure;
	}

	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
						0, info->max_x, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
						0, info->max_y, 0, 0);

	if (!info->in_bootloader && !info->input_registered) {
		ret = input_register_device(info->input_dev);
		if (ret) {
			dev_err(&info->client->dev,
				"failed to register input dev\n");
			goto out_failure;
		}
		info->input_registered = true;
	}
out_bootloader:
	if (info->in_bootloader)
		pr_info("Touch IC in bootloader mode (0x%x)\n",
			info->bootmode);
	else
		pr_info("Product: APS-%s, build-id: %x%02x\n",
			info->prod_id, info->version[2], info->version[3]);

	return 0;

out_touch_inactive:
	/* Touch IC is currently inactive - clear old info */
	pr_err("Touch IC failed to activate\n");
	memset(info->prod_id, 0, sizeof(info->prod_id));
	memset(info->version, 0, sizeof(info->version));
	info->bootmode = 0;
	return 0;

out_failure:
	return 1;
}

static int aps_isp_status(struct aps_ts_info *info, u32 success_mask,
		const char *func)
{
	struct i2c_client *client = info->client;
	u8 buf[4];
	u32 result;
	int i, ret = -EIO;

	for (i = 0; i < APS_I2C_RETRY_TIMES; i++) {
		ret = aps_i2c_read(info, ISP_REG_STATUS,
					&buf, sizeof(buf));
		if (ret != sizeof(buf))
			goto end;

		result = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];
		if (result & success_mask) {
			ret = 0;
			break;
		}

		msleep(50);
	}

	if (ret != 0)
		dev_warn(&client->dev,
			"isp [%s] status (0x%x) failed check (0x%x)\n",
			func, result, success_mask);
end:
	return ret;
}

static int aps_isp_clear(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 cmd[6] =  ISP_CLEAR_DONE;
	int ret = 0;

	if (i2c_master_send(client, cmd, 6) != 6) {
		dev_err(&client->dev, "isp clear i2c_master_send failed\n");
		ret = -EIO;
	}

	return ret;
}

static int aps_sendcmd(struct i2c_client *client, u8 *cmd, int cmd_size,
	const char *cmdname)
{
	int i;
	int ret = 0;

	pr_info("Sending command \"%s\"\n", cmdname);
	for (i = 0; i < ISP_MAX_RETRIES; i++)
		if (i2c_master_send(client, cmd, cmd_size) != cmd_size)
			continue;
		else
			break;

	if (i < ISP_MAX_RETRIES)
		pr_info("Command \"%s\" succeeded after %d retries\n",
			cmdname, i);
	else {
		pr_info("Command \"%s\" failed\n", cmdname);
		ret = -1;
	}

	return ret;
}

static int aps_isp_entry(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 entry_a[6] = ISP_ENTRY1;
	u8 entry_b[6] = ISP_ENTRY2;
	u8 entry_c[6] = ISP_ENTRY3;
	u8 speed_up[6] = ISP_CLK_20MHZ;

	if (aps_sendcmd(client, entry_a, 6, "ISP entry 1"))
		return -EINVAL;

	if (aps_sendcmd(client, entry_b, 6, "ISP entry 2"))
		return -EINVAL;

	if (aps_sendcmd(client, entry_c, 6, "ISP entry 3"))
		return -EINVAL;

	if (info->high_speed_isp)
		if (aps_sendcmd(client, speed_up, 6, "Switch to 20MHz"))
			return -EINVAL;

	return 0;
}

static int aps_isp_mode_check(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	/* verify if ISP mode is entered */
	ret = aps_isp_status(info, ISP_MODE_BIT, __func__);
	if (ret)
		return -EAGAIN;
	/* clear command done */
	aps_isp_clear(info);

	if (info->high_speed_isp) {
		u8 clock_20mhz[6] = ISP_CLK_20MHZ;
		if (i2c_master_send(client, clock_20mhz, 6) != 6)
			ret = -EIO;
	}

	return ret;
}

static int aps_isp_unlock(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 unlock_a[6] = ISP_UNLOCK1;
	u8 unlock_b[6] = ISP_UNLOCK2;
	u8 unlock_c[6] = ISP_UNLOCK3;
	int ret = 0;

	if (i2c_master_send(client, unlock_a, 6) != 6)
		ret = -EIO;
	else if (i2c_master_send(client, unlock_b, 6) != 6)
		ret = -EIO;
	else if (i2c_master_send(client, unlock_c, 6) != 6)
		ret = -EIO;

	return ret;
}

static int aps_isp_erase(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 cmd[6] = ISP_FULL_ERASE;
	int ret;

	if (i2c_master_send(client, cmd, 6) != 6) {
		dev_err(&client->dev, "isp erase i2c_transfer failed\n");
		return -EIO;
	}

	ret = aps_isp_status(info, ISP_DONE_BIT, __func__);
	aps_isp_clear(info);

	return ret;
}

static int aps_isp_lock(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 cmd[6] = ISP_LOCK;
	int ret = 0;

	if (i2c_master_send(client, cmd, 6) != 6) {
		dev_err(&client->dev, "isp lock i2c_master_send failed\n");
		ret = -EIO;
	}

	return ret;
}

static int aps_isp_addr(struct aps_ts_info *info, const size_t addr)
{
	struct i2c_client *client = info->client;
	u8 wbuf_addr[6] = ISP_ADDRESS;
	int ret = 0;

	wbuf_addr[2] = (addr>>24)&0xFF;
	wbuf_addr[3] = (addr>>16)&0xFF;
	wbuf_addr[4] = (addr>>8)&0xFF;
	wbuf_addr[5] = (addr>>0)&0xFF;

	if (i2c_master_send(client, wbuf_addr, 6) != 6) {
		dev_err(&client->dev, "isp fw-addr i2c_master_send failed\n");
		ret = -EIO;
	}

	return ret;
}

static int aps_flash_data(struct aps_ts_info *info, const u8 *data,
	const size_t addr)
{
	struct i2c_client *client = info->client;
	u8 write_cmd[6] = ISP_WRITE;
	u8 wbuf_a[6] = ISP_WRITE_A;
	u8 wbuf_b[6] = ISP_WRITE_B;
	int ret = 0;

	wbuf_a[2] = data[3];
	wbuf_a[3] = data[2];
	wbuf_a[4] = data[1];
	wbuf_a[5] = data[0];

	if (i2c_master_send(client, wbuf_a, 6) != 6) {
		dev_err(&client->dev, "isp data(a) i2c_master_send failed\n");
		return -EIO;
	}

	wbuf_b[2] = data[7];
	wbuf_b[3] = data[6];
	wbuf_b[4] = data[5];
	wbuf_b[5] = data[4];

	if (i2c_master_send(client, wbuf_b, 6) != 6) {
		dev_err(&client->dev, "isp data(b) i2c_master_send failed\n");
		return -EIO;
	}

	ret = aps_isp_addr(info, addr);

	if (i2c_master_send(client, write_cmd, 6) != 6) {
		dev_err(&client->dev, "isp write cmd i2c_master_send failed\n");
		return -EIO;
	}

	ret = aps_isp_status(info, ISP_DONE_BIT, __func__);
	aps_isp_clear(info);

	return ret;
}

static int aps_verify_data(struct aps_ts_info *info, const u8 *data,
	const size_t addr)
{
	struct i2c_client *client = info->client;
	u8 read_cmd[6] = ISP_READ;
	u8 read_b[4], r_cmp[8];
	int ret = 0;

	if (aps_isp_addr(info, addr))
		return -EIO;

	if (i2c_master_send(client, read_cmd, 6) != 6) {
		dev_err(&client->dev, "isp read cmd i2c_master_send failed\n");
		return -EIO;
	}

	ret = aps_i2c_read(info, ISP_REG_READ_A, read_b, sizeof(read_b));
	if (ret != sizeof(read_b)) {
		dev_err(&client->dev, "isp read data(a) failed\n");
		return -EIO;
	}

	r_cmp[0] = read_b[3];
	r_cmp[1] = read_b[2];
	r_cmp[2] = read_b[1];
	r_cmp[3] = read_b[0];

	ret = aps_i2c_read(info, ISP_REG_READ_B, read_b, sizeof(read_b));
	if (ret != sizeof(read_b)) {
		dev_err(&client->dev, "isp read data(b) failed\n");
		return -EIO;
	}

	r_cmp[4] = read_b[3];
	r_cmp[5] = read_b[2];
	r_cmp[6] = read_b[1];
	r_cmp[7] = read_b[0];

	ret = aps_isp_status(info, ISP_DONE_BIT, __func__);
	aps_isp_clear(info);

	if (memcmp(data, r_cmp, 8)) {
		dev_err(&client->dev, "isp data check failed @%x\n", addr);
		ret = -EINVAL;
	}

	return ret;
}

static int aps_fw_flash(const struct firmware *fw, struct aps_ts_info *info)
{
	u8 write_mode[6] = ISP_WRITE_MODE;
	u8 read_mode[6] = ISP_READ_MODE;
	int fw_size = (int)fw->size;
	u8 *fw_data = (u8 *)fw->data;
	int addr;
	int ret = 0;

	if ((fw_size % 8) != 0)
		fw_size = fw_size + (8 - (fw_size % 8));

	if (aps_isp_entry(info)) {
		dev_err(&info->client->dev, "isp entry command failed\n");
		ret = -EAGAIN;
		goto out;
	}

	if (aps_isp_mode_check(info)) {
		dev_err(&info->client->dev, "isp entry failed\n");
		ret = -EAGAIN;
		goto lock_and_exit;
	}

	if (aps_isp_unlock(info)) {
		dev_err(&info->client->dev, "isp unlock failed\n");
		ret = -EAGAIN;
		goto lock_and_exit;
	}

	if (aps_isp_erase(info)) {
		dev_err(&info->client->dev, "isp erase failed\n");
		ret = -EAGAIN;
		goto lock_and_exit;
	}

	if (i2c_master_send(info->client, write_mode, 6) != 6) {
		dev_err(&info->client->dev,
				"isp write mode i2c_master_send failed\n");
		ret = -EIO;
		goto lock_and_exit;
	}

	for (addr = fw_size - 8; addr >= 0; addr -= 8)
		if (aps_flash_data(info, (u8 *)&fw_data[addr], addr)) {
			dev_err(&info->client->dev, "isp flash failed\n");
			ret = -EFAULT;
			goto lock_and_exit;
		}

	if (i2c_master_send(info->client, read_mode, 6) != 6) {
		dev_err(&info->client->dev,
				"isp read mode i2c_master_send failed\n");
		ret = -EIO;
		goto lock_and_exit;
	}

	for (addr = fw_size - 8; addr >= 0; addr -= 8)
		if (aps_verify_data(info, (u8 *)&fw_data[addr], addr)) {
			dev_err(&info->client->dev, "isp verify failed\n");
			ret = -EFAULT;
			goto lock_and_exit;
		}

lock_and_exit:
	ret = aps_isp_lock(info);
out:
	return ret;
}

static void aps_fw_update(struct aps_ts_info *info, const struct firmware *fw)
{
	int ret;
	struct i2c_client *client = info->client;

	pr_debug("About to flash FW...");
	mutex_lock(&info->lock);

	if (info->irq_enabled)
		disable_irq(client->irq);

	if (!info->enabled) {
		pr_info("Touch IC is not powered on - abort FW flashing");
		goto out;
	}

	ret = aps_fw_flash(fw, info);
	if (ret)
		dev_err(&info->client->dev, "firmware flash failed\n");
	else
		dev_info(&info->client->dev, "firmware flash succeeded\n");

	aps_reset(info);

	if (info->input_registered) {
		input_unregister_device(info->input_dev);
		info->input_dev = NULL;
		info->input_registered = false;
		pr_debug("de-allocated input device\n");
	}

	aps_query_device(info);
out:
	if (info->irq_enabled)
		enable_irq(client->irq);

	mutex_unlock(&info->lock);
}

static ssize_t aps_start_isp(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	int ret = 0;

	if (count >= sizeof(info->fw_name)) {
		dev_err(&info->client->dev, "firmware filename is too long\n");
		return -EINVAL;
	}

	strlcpy(info->fw_name, buf, count);
	dev_info(&info->client->dev, "firmware filename %s\n", info->fw_name);

	ret = request_firmware(&fw, info->fw_name, &info->client->dev);
	if (ret || !fw) {
		dev_err(&info->client->dev,
			"unable to download firmware ret = %d\n", ret);
		return -EINVAL;
	}

	aps_fw_update(info, fw);
	release_firmware(fw);

	return count;
}

static ssize_t aps_version(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	ssize_t count;

	if (info->bootmode == APS_BOOTMODE_APPLICATION) {
		count = scnprintf(buf, PAGE_SIZE, "%02x%02x-%02x%02x0000\n",
			info->version[2], info->version[3],
			info->version[2], info->version[3]);
	} else if (info->bootmode == APS_BOOTMODE_BOOTLOADER) {
		count = scnprintf(buf, PAGE_SIZE, "%02x%02x-%02x%02x0000\n",
			info->version[0], info->version[1],
			info->version[0], info->version[1]);
	} else
		count = scnprintf(buf, PAGE_SIZE, "0000-00000000\n");

	return count;
}

static ssize_t aps_product_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	ssize_t count;

	if (info->bootmode == APS_BOOTMODE_APPLICATION)
		count = scnprintf(buf, PAGE_SIZE, "%s\n",
			info->prod_id);
	else
		/* Supply default product id needed for FW flashing   */
		/* if actual product id can not be read from touch IC */
		count = scnprintf(buf, PAGE_SIZE, "%s\n",
			info->default_prod_id);

	return count;
}

static ssize_t aps_ic_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	ssize_t count;

	if (info->bootmode == APS_BOOTMODE_APPLICATION)
		count = scnprintf(buf, PAGE_SIZE, "Product ID: %s\n"
			"Build ID: %02x%02x\n"
			"Config ID: %02x%02x0000\n",
			info->prod_id,
			info->version[2], info->version[3],
			info->version[2], info->version[3]);
	else
		count = scnprintf(buf, PAGE_SIZE, "Product ID: %s\n"
			"Build ID: %02x%02x\n"
			"Config ID: %02x%02x0000\n",
			info->default_prod_id,
			info->version[0], info->version[1],
			info->version[0], info->version[1]);

	return count;
}

static ssize_t aps_poweron_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%d\n", info->enabled);
	return count;
}

static ssize_t aps_flashprog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	ssize_t count;

	count = scnprintf(buf, PAGE_SIZE, "%d\n", info->in_bootloader);
	return count;
}

static ssize_t aps_forcereflash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	unsigned long value = 0;
	int err = 0;

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		pr_err("Failed to convert value.\n");
		return -EINVAL;
	}

	switch (value) {
	case 0:
	case 1:
		mutex_lock(&info->lock);
		info->force_reflash = value;
		mutex_unlock(&info->lock);
		break;
	default:
		pr_err("Invalid value\n");
	}
	return count;
}

static DEVICE_ATTR(doreflash, 0220, NULL, aps_start_isp);
static DEVICE_ATTR(buildid, 0444, aps_version, NULL);
static DEVICE_ATTR(productinfo, 0444, aps_product_show, NULL);
static DEVICE_ATTR(ic_ver, 0444, aps_ic_ver_show, NULL);
static DEVICE_ATTR(forcereflash, 0220, NULL, aps_forcereflash_store);
static DEVICE_ATTR(poweron, 0444, aps_poweron_show, NULL);
static DEVICE_ATTR(flashprog, 0444, aps_flashprog_show, NULL);

static struct attribute *aps_attrs[] = {
	&dev_attr_doreflash.attr,
	&dev_attr_buildid.attr,
	&dev_attr_aps_perf.attr,
	&dev_attr_drv_irq.attr,
	&dev_attr_hw_irqstat.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_ic_ver.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_poweron.attr,
	&dev_attr_flashprog.attr,
	NULL,
};

static const struct attribute_group aps_attr_group = {
	.attrs = aps_attrs,
};

#ifdef CONFIG_OF
static struct aps_ts_platform_data *aps_ts_of_init(struct i2c_client *client,
	struct aps_ts_info *info)
{
	struct aps_ts_platform_data *pdata;
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	const char *default_prod_id;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failed\n");
		return NULL;
	}

	pdata->gpio_irq = of_get_gpio(np, 0);
	if (!gpio_is_valid(pdata->gpio_irq)) {
		dev_err(dev, "Invalid irq gpio\n");
		return NULL;
	}

	pdata->gpio_reset = of_get_gpio(np, 1);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		dev_err(dev, "Invalid reset gpio\n");
		return NULL;
	}

	if (of_property_read_bool(np, "aps_ts,high-speed-isp")) {
		pr_notice("using 20MHz clock in ISP mode\n");
		info->high_speed_isp = true;
	}

	if (!of_property_read_string(np, "aps_ts,default-product-id",
		&default_prod_id)) {
		pr_notice("default productid is \"%s\"\n", default_prod_id);
		strlcpy(info->default_prod_id, default_prod_id,
			sizeof(info->default_prod_id));
	}

	if (of_property_read_u32(np, "aps_ts,ui-ready-msec", &info->ui_ready_msec)) {
		pr_notice("waiting %u msec for UI on reset\n",
			info->ui_ready_msec);
	}

	return pdata;
}
#else
static struct aps_ts_platform_data *aps_ts_of_init(struct i2c_client *client,
	struct aps_ts_info *info)
{
	pr_err("Not defined\n");
	return NULL;
}
#endif

#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS) && defined(CONFIG_FB)
/* Need both MMI notifications and frame buffer notifications.
   MMI notifications:
	suspend - delay suspend while FW flashing is in progress
	resume - reset touch IC. Can't read any touch IC registers here
		since display is not processing frames yet and touch IC
		is inactive
   FB notifications:
	unblank - activate touch IC if was inactive
*/
static int aps_ts_suspend(struct device *dev)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	mutex_lock(&info->lock);
	aps_ts_disable(info);
	aps_clear_input_data(info);
	mutex_unlock(&info->lock);
	pr_debug("SUSPENDED\n");
	return 0;
}

static int aps_ts_resume(struct device *dev)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	mutex_lock(&info->lock);
	aps_ts_enable(info);
	mutex_unlock(&info->lock);
	pr_debug("RESUMED\n");
	return 0;
}

static int fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct aps_ts_info *info = container_of(self, struct aps_ts_info,
		fb_notif);

	if (!evdata || !evdata->data || event != FB_EVENT_BLANK
		|| !info || !info->client)
		return 0;

	blank = evdata->data;
	if (*blank == FB_BLANK_UNBLANK) {
		if (info->bootmode == 0 && info->enabled) {
			mutex_lock(&info->lock);
			aps_query_device(info);
			mutex_unlock(&info->lock);
		}
		pr_debug("DISPLAY-ON\n");
	} else if (*blank == FB_BLANK_POWERDOWN) {
		pr_debug("DISPLAY-OFF\n");
	}

	return 0;
}
#endif

#if defined(CONFIG_FB) && !defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
static int fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct aps_ts_info *info = container_of(self, struct aps_ts_info,
		fb_notif);

	if (!evdata || !evdata->data || event != FB_EVENT_BLANK
		|| !info || !info->client)
		return 0;

	blank = evdata->data;
	if (*blank == FB_BLANK_UNBLANK) {
		pr_debug("DISPLAY-ON\n");
		aps_ts_resume(&info->client->dev);
		if (info->bootmode == 0) {
			mutex_lock(&info->lock);
			aps_query_device(info);
			mutex_unlock(&info->lock);
		}
	} else if (*blank == FB_BLANK_POWERDOWN) {
		aps_ts_suspend(&info->client->dev);
		pr_debug("DISPLAY-OFF\n");
	}

	return 0;
}
#endif

static int aps_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct aps_ts_info *info = NULL;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Failed to allocate info\n");
		ret = -ENOMEM;
		goto out_free_mem;
	}

	info->perf_irq_count = info->perf_irq_first = info->perf_irq_last = 0;
	info->perf_irq_data = kzalloc(sizeof(struct aps_irq_perf)*PERF_IRQ_MAX,
		GFP_KERNEL);
	if (!info->perf_irq_data) {
		dev_err(&client->dev, "Failed to allocate irq perf\n");
		ret = -ENOMEM;
		goto out_free_mem;
	}

	info->client = client;
	info->irq = -1;
	init_completion(&info->init_done);
	mutex_init(&info->lock);

	if (client->dev.of_node)
		info->pdata = aps_ts_of_init(client, info);
	else
		info->pdata = client->dev.platform_data;

	if (!info->pdata) {
		dev_err(&client->dev, "No platform data\n");
		ret = -ENOMEM;
		goto out_free_mem;
	}

	/* init GPIOs and set reset GPIO low */
	ret = aps_gpio_configure(info);
	if (ret) {
		ret = -EIO;
		goto out_free_mem;
	}

	info->client = client;
	info->irq = client->irq;
	i2c_set_clientdata(client, info);

	init_completion(&info->init_done);
	mutex_init(&info->lock);

	aps_reset(info);

	ret = aps_query_device(info);
	if (ret) {
		ret = -ENODEV;
		goto out_free_gpio;
	}

	ret = aps_ts_config(info);
	if (ret) {
		ret = -EIO;
		goto out_free_gpio;
	}

	/* hold IC in reset, display notification will move it out of reset */
	aps_set_reset_low(info);

	if (sysfs_create_group(&client->dev.kobj, &aps_attr_group)) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		ret = -EAGAIN;
		goto out_free_irq;
	}
#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS) && defined(CONFIG_FB)
	info->panel_nb.pre_display_off = aps_ts_suspend;
	info->panel_nb.display_on = aps_ts_resume;
	info->panel_nb.dev = &client->dev;
	if (!mmi_panel_register_notifier(&info->panel_nb))
		pr_info("registered MMI panel notifier\n");
	else {
		dev_err(&client->dev, "unable to register MMI notifier");
		goto out_sysfs_remove_group;
	}

	info->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&info->fb_notif);
	if (ret) {
		dev_err(&client->dev,
			"Error registering fb_notifier: %d\n", ret);
		ret = -EINVAL;
		goto out_sysfs_remove_group;
	}
#elif defined(CONFIG_FB)
	info->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&info->fb_notif);
	if (ret) {
		dev_err(&client->dev,
			"Error registering fb_notifier: %d\n", ret);
		ret = -EINVAL;
		goto out_sysfs_remove_group;
	}
#endif

	dev_notice(&client->dev, "aps_ts initialized\n");
	return 0;

out_sysfs_remove_group:
	sysfs_remove_group(&client->dev.kobj, &aps_attr_group);

out_free_irq:
	free_irq(client->irq, info);

out_free_gpio:
	gpio_free(info->pdata->gpio_reset);
	gpio_free(info->pdata->gpio_irq);

	if (info->input_registered)
		input_unregister_device(info->input_dev);
	else
		input_free_device(info->input_dev);

out_free_mem:
	if (info) {
		kfree(info->perf_irq_data);
		kfree(info);
	}

	dev_notice(&client->dev, "aps_ts probe failed\n");
	return ret;
}

static int aps_ts_remove(struct i2c_client *client)
{
	struct aps_ts_info *info = i2c_get_clientdata(client);

	free_irq(info->irq, info);
	if (info->input_registered)
		input_unregister_device(info->input_dev);
	else
		input_free_device(info->input_dev);
	gpio_free(info->pdata->gpio_reset);
	gpio_free(info->pdata->gpio_irq);
	kfree(info->perf_irq_data);
#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS) && defined(CONFIG_FB)
	mmi_panel_unregister_notifier(&info->panel_nb);
	fb_unregister_client(&info->fb_notif);
#elif defined(CONFIG_FB)
	fb_unregister_client(&info->fb_notif);
#endif
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM)
static const struct dev_pm_ops aps_ts_pm_ops = {
#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS) && defined(CONFIG_FB)
	.suspend	= aps_ts_suspend,
	.resume		= aps_ts_resume,
#endif
};
#endif

static struct i2c_device_id aps_ts_id[] = {
	{"aps_ts", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, aps_ts_id);

#ifdef CONFIG_OF
static struct of_device_id aps_ts_match_table[] = {
	{ .compatible = "melfas-aps,aps_ts",},
	{ },
};
#endif

static struct i2c_driver aps_ts_driver = {
	.probe		= aps_ts_probe,
	.remove		= __devexit_p(aps_ts_remove),
	.driver		= {
		.name	= "aps_ts",
		.of_match_table = of_match_ptr(aps_ts_match_table),
#if defined(CONFIG_PM)
		.pm	= &aps_ts_pm_ops,
#endif
	},
	.id_table	= aps_ts_id,
};
module_i2c_driver(aps_ts_driver);

MODULE_VERSION("1.1");
MODULE_DESCRIPTION("Melfas APS Touchscreen driver");
MODULE_LICENSE("GPL");
