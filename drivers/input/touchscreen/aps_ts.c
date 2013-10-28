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

#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>

static int fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data);
#endif

#define DRIVER_NAME "aps_ts"
#define INPUT_PHYS_NAME "aps_ts/input0"

/* TODO: to be confirmed with touch spec */
#define RESET_DELAY_USEC	2000
#define UI_READY_MSEC		200

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

#define APS_BOOTMODE_BOOTLOADER		0x11
#define APS_BOOTMODE_APPLICATION	0x22
#define APS_BOOTMODE_VERIFYING		0x33

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

#define ISP_FULL_ERASE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x44 }
#define ISP_CLEAR_DONE		{ 0x00, 0x0C, 0x00, 0x00, 0x00, 0x20 }
#define ISP_WRITE_MODE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x01 }
#define ISP_WRITE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x41 }
#define ISP_READ_MODE		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x08 }
#define ISP_READ		{ 0x00, 0x10, 0x00, 0x00, 0x10, 0x48 }

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

#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	int				max_x;
	int				max_y;
	int				irq;
	struct aps_ts_platform_data	*pdata;
	struct completion		init_done;
	struct mutex			lock;
	bool				enabled;
	u8				version[4];
	u8				bootmode;
	char				fw_name[MAX_FW_NAME_LEN];
	int				perf_irq_count;
	int				perf_irq_first;
	int				perf_irq_last;
	struct aps_irq_perf		*perf_irq_data;
};

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

static DEVICE_ATTR(aps_perf, 0440, aps_irq_perf_show, NULL);

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
	msleep(UI_READY_MSEC);
	pr_debug("out of reset\n");
}

static void aps_ts_enable(struct aps_ts_info *info)
{
	if (info->enabled)
		return;

	mutex_lock(&info->lock);

	info->enabled = true;
	enable_irq(info->irq);
	aps_reset(info);

	mutex_unlock(&info->lock);
}

static void aps_ts_disable(struct aps_ts_info *info)
{
	if (!info->enabled)
		return;

	mutex_lock(&info->lock);

	disable_irq(info->irq);
	aps_set_reset_low(info);
	info->enabled = false;

	mutex_unlock(&info->lock);
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
	ret = i2c_transfer(client->adapter, msg, 2);

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

static void query_device(struct aps_ts_info *info)
{
	int ret;
	mutex_lock(&info->lock);

	ret = aps_i2c_read(info, APS_REG_BOOTMODE, &info->bootmode,
		sizeof(info->bootmode));

	if (ret <= 0) {
		pr_err("Failed to read boot mode\n");
		info->bootmode = 0;
	}

	ret = aps_i2c_read(info, APS_REG_FW_VER, &info->version,
		sizeof(info->version));

	if (ret <= 0) {
		pr_err("Failed to read FW version\n");
		memset(info->version, 0, sizeof(info->version));
	}

	mutex_unlock(&info->lock);
}

static void aps_clear_input_data(struct aps_ts_info *info)
{
	int i;

	for (i = 0; i < MAX_FINGER_NUM; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER,
			false);
	}
	input_sync(info->input_dev);

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
	int last;

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
	info->irq = client->irq;

	barrier();

	complete_all(&info->init_done);
out:
	return ret;
}

static int aps_isp_entry(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 entry_a[6] = ISP_ENTRY1;
	u8 entry_b[6] = ISP_ENTRY2;
	u8 entry_c[6] = ISP_ENTRY3;

	if (i2c_master_send(client, entry_a, 6) != 6) {
		ret = -1;
		return ret;
	}

	if (i2c_master_send(client, entry_b, 6) != 6) {
		ret = -1;
		return ret;
	}

	if (i2c_master_send(client, entry_c, 6) != 6) {
		ret = -1;
		return ret;
	}

	return ret;
}

static int aps_isp_unlock(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 unlock_a[6] = ISP_UNLOCK1;
	u8 unlock_b[6] = ISP_UNLOCK2;
	u8 unlock_c[6] = ISP_UNLOCK3;

	if (i2c_master_send(client, unlock_a, 6) != 6) {
		ret = -1;
		return ret;
	}

	if (i2c_master_send(client, unlock_b, 6) != 6) {
		ret = -1;
		return ret;
	}

	if (i2c_master_send(client, unlock_c, 6) != 6) {
		ret = -1;
		return ret;
	}

	return ret;
}

static int aps_isp_write_done(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 cmd[2] = {0x00, 0x0C};
	u8 rbuf[4];
	int ret = 0;
	int i = 0;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = cmd,
			.len = 2,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = 4,
		},
	};

	while (true) {
		int sz = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (sz != ARRAY_SIZE(msg)) {
			dev_err(&client->dev,
				"isp write done i2c_transfer failed\n");
			ret = -1;
			return ret;
		}

		if (rbuf[3] & 0x20)
			break;

		dev_err(&client->dev, "write done while\n");

		if (i++ > 1000) {
			dev_err(&client->dev, "isp erase wait-done failed\n");
			ret = -1;
			return ret;
		}
	}

	return ret;
}

static int aps_isp_clean(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 clr_done[6] =  ISP_CLEAR_DONE;

	if (i2c_master_send(client, clr_done, 6) != 6) {
		dev_err(&client->dev, "isp clr_done i2c_master_send failed\n");
		ret = -1;
		return ret;
	}

	return ret;
}

static int aps_isp_erase(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 erase[6] = ISP_FULL_ERASE;
	int ret = 0;

	if (i2c_master_send(client, erase, 6) != 6) {
		dev_err(&client->dev, "isp erase i2c_transfer failed\n");
		ret = -1;
		return ret;
	}

	ret = aps_isp_write_done(info);

	ret = aps_isp_clean(info);

	return ret;
}

static int aps_isp_lock(struct aps_ts_info *info)
{
	struct i2c_client *client = info->client;
	u8 buf[6] = ISP_LOCK;
	int ret = 0;
	if (i2c_master_send(client, buf, 6) != 6) {
		ret = -1;
		dev_err(&client->dev, "isp fw-addr i2c_master_send failed\n");
		return ret;
	}

	return ret;
}

static int aps_isp_addr(struct aps_ts_info *info, const size_t addr)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 wbuf_addr[6];
	wbuf_addr[0] = 0x00;
	wbuf_addr[1] = 0x14;
	wbuf_addr[2] = (addr>>24)&0xFF;
	wbuf_addr[3] = (addr>>16)&0xFF;
	wbuf_addr[4] = (addr>>8)&0xFF;
	wbuf_addr[5] = (addr>>0)&0xFF;

	if (i2c_master_send(client, wbuf_addr, 6) != 6) {
		ret = -1;
		dev_err(&client->dev, "isp fw-addr i2c_master_send failed\n");
		return ret;
	}

	return ret;
}

static int aps_flash_data(struct aps_ts_info *info, const u8 *data,
	const size_t addr)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 write_mode[6] = ISP_WRITE_MODE;
	u8 write_cmd[6] = ISP_WRITE;
	u8 wbuf_a[6] = {0x00, 0x28, };
	u8 wbuf_b[6] = {0x00, 0x2C, };

	if (i2c_master_send(client, write_mode, 6) != 6) {
		dev_err(&client->dev,
			"isp write mode i2c_master_send failed\n");
		ret = -1;
		return ret;
	}

	wbuf_a[2] = data[3];
	wbuf_a[3] = data[2];
	wbuf_a[4] = data[1];
	wbuf_a[5] = data[0];

	if (i2c_master_send(client, wbuf_a, 6) != 6) {
		ret = -1;
		dev_err(&client->dev,
			"isp fw-data(a) i2c_master_send failed\n");
		return ret;
	}

	wbuf_b[2] = data[7];
	wbuf_b[3] = data[6];
	wbuf_b[4] = data[5];
	wbuf_b[5] = data[4];

	if (i2c_master_send(client, wbuf_b, 6) != 6) {
		ret = -1;
		dev_err(&client->dev,
			"isp fw-data(b) i2c_master_send failed\n");
		return ret;
	}

	ret = aps_isp_addr(info, addr);

	if (i2c_master_send(client, write_cmd, 6) != 6) {
		dev_err(&client->dev,
			"isp write cmd i2c_master_send failed\n");
		ret = -1;
		return ret;
	}

	ret = aps_isp_write_done(info);
	ret = aps_isp_clean(info);
	return ret;
}

static int aps_verify_data(struct aps_ts_info *info, const u8 *data,
	const size_t addr)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	u8 read_a[2] = {0x00, 0x30};
	u8 read_b[2] = {0x00, 0x34};
	u8 rbuf[4];
	u8 r_cmp[8];
	u8 read_mode[6] = ISP_READ_MODE;
	u8 read_cmd[6] = ISP_READ;
	int sz;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = read_a,
			.len = 2,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = rbuf,
			.len = 4,
		},
	};
	if (i2c_master_send(client, read_mode, 6) != 6) {
		ret = -1;
		dev_err(&client->dev, "isp read mode i2c_master_send failed\n");
		return ret;
	}

	ret = aps_isp_addr(info, addr);

	if (i2c_master_send(client, read_cmd, 6) != 6) {
		ret = -1;
		dev_err(&client->dev, "isp read cmd i2c_master_send failed\n");
		return ret;
	}

	sz = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (sz != ARRAY_SIZE(msg)) {
		ret = -1;
		dev_err(&client->dev,
			"isp read data(a) i2c_master_send failed\n");
		return ret;
	}
	r_cmp[0] = rbuf[3];
	r_cmp[1] = rbuf[2];
	r_cmp[2] = rbuf[1];
	r_cmp[3] = rbuf[0];

	msg[0].buf = read_b;

	sz = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (sz != ARRAY_SIZE(msg)) {
		ret = -1;
		dev_err(&client->dev,
			"isp read data(b) i2c_master_send failed\n");
		return ret;
	}

	r_cmp[4] = rbuf[3];
	r_cmp[5] = rbuf[2];
	r_cmp[6] = rbuf[1];
	r_cmp[7] = rbuf[0];

	ret = aps_isp_write_done(info);

	if (memcmp(data, r_cmp, 8)) {
		ret = -1;
		return ret;
	}

	ret = aps_isp_clean(info);
	return ret;
}

static int aps_fw_flash(const struct firmware *fw, struct aps_ts_info *info)
{
	u8 *fw_data;
	int addr;
	int ret = 0;
	int fw_size = (int)fw->size;
	fw_data = kmalloc(0x10000, GFP_KERNEL);
	memset(fw_data, 0xff, 0x10000);
	memcpy(fw_data, fw->data, fw->size);
	if ((fw_size % 8) != 0)
		fw_size = fw_size + (8 - (fw_size % 8));

	if (aps_isp_entry(info)) {
		dev_err(&info->client->dev, "isp entry failed\n");
		ret = -1;
		goto out;
	}

	if (aps_isp_unlock(info)) {
		dev_err(&info->client->dev, "isp unlock failed\n");
		ret = -1;
		goto out;
	}

	if (aps_isp_erase(info)) {
		dev_err(&info->client->dev, "isp erase failed\n");
		ret = -1;
		goto out;
	}

	for (addr = fw_size - 8; addr >= 0; addr -= 8) {
		if (aps_flash_data(info, (u8 *)&fw_data[addr], addr)) {
			dev_err(&info->client->dev, "isp flash failed\n");
			ret = -1;
			goto out;
		}

		if (aps_verify_data(info, (u8 *)&fw_data[addr], addr)) {
			dev_err(&info->client->dev, "isp verify failed\n");
			ret = -1;
			goto out;
		}
	}

	ret = aps_isp_lock(info);

out:
	kfree(fw_data);
	release_firmware(fw);
	return ret;
}

static void aps_fw_update_controller(const struct firmware *fw, void *context)
{
	struct aps_ts_info *info = context;
	int ret;

	ret = aps_fw_flash(fw, info);
	if (ret)
		dev_err(&info->client->dev, "firmware flash failed\n");
	else
		dev_info(&info->client->dev, "firmware flash succeed\n");

	aps_reset(info);
	query_device(info);

	return;
}

static ssize_t aps_start_isp(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	int ret = 0;

	dev_info(&info->client->dev, "Requesting FW file: %s\n", FW_NAME);

	ret = request_firmware_nowait(THIS_MODULE, true, FW_NAME,
		&info->client->dev, GFP_KERNEL, info, aps_fw_update_controller);

	if (ret) {
		dev_err(&info->client->dev,
			"Failed to schedule firmware update\n");
		return -EIO;
	}

	return 1;
}

static ssize_t aps_version(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct aps_ts_info *info = dev_get_drvdata(dev);
	ssize_t count;

	mutex_lock(&info->lock);

	if (info->bootmode == APS_BOOTMODE_APPLICATION) {
		count = scnprintf(buf, PAGE_SIZE, "%x%02x-00000000\n",
			info->version[2],
			info->version[3]);
	} else if (info->bootmode == APS_BOOTMODE_BOOTLOADER) {
		count = scnprintf(buf, PAGE_SIZE, "%x%02x-00000000\n",
			info->version[0],
			info->version[1]);
	} else
		count = scnprintf(buf, PAGE_SIZE, "000-00000000\n");

	mutex_unlock(&info->lock);
	return count;
}

static DEVICE_ATTR(aps_isp, 0440, aps_start_isp, NULL);
static DEVICE_ATTR(buildid, 0444, aps_version, NULL);

static struct attribute *aps_attrs[] = {
	&dev_attr_aps_isp.attr,
	&dev_attr_buildid.attr,
	&dev_attr_aps_perf.attr,
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

static int aps_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct aps_ts_info *info = NULL;
	struct input_dev *input_dev = NULL;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate input_dev\n");
		ret = -ENOMEM;
		goto out_free_mem;
	}

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
	info->input_dev = input_dev;
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
	/* keep reset GPIO low */
	udelay(RESET_DELAY_USEC);

	/* set reset GPIO high to let touch IC out of reset */
	gpio_set_value(info->pdata->gpio_reset, 1);
	msleep(UI_READY_MSEC);

	ret = aps_ts_read_resolution(info);
	if (ret) {
		pr_err("Failed to read resolution\n");
		ret = -EIO;
		goto out_free_gpio;
	}

	input_mt_init_slots(input_dev, MAX_FINGER_NUM);

	input_dev->name = DRIVER_NAME;
	input_dev->phys = INPUT_PHYS_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(input_dev,
		ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev,
		ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);

	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev\n");
		ret = -EIO;
		goto out_free_gpio;
	}

	i2c_set_clientdata(client, info);
	if (aps_ts_config(info))
		goto out_unregister_input_dev;

	query_device(info);

	/* let IC sleep, it will be awoken by frame buffer notificaiton */
	aps_set_reset_low(info);

	if (sysfs_create_group(&client->dev.kobj, &aps_attr_group)) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		ret = -EAGAIN;
		goto out_free_irq;
	}

#ifdef CONFIG_FB
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

out_unregister_input_dev:
	input_unregister_device(input_dev);

out_free_gpio:
	gpio_free(info->pdata->gpio_reset);
	gpio_free(info->pdata->gpio_irq);

out_free_mem:
	kfree(input_dev);
	if (info) {
		kfree(info->perf_irq_data);
		kfree(info);
	}

	return ret;
}

static int aps_ts_remove(struct i2c_client *client)
{
	struct aps_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0)
		free_irq(info->irq, info);

	input_unregister_device(info->input_dev);
	gpio_free(info->pdata->gpio_reset);
	gpio_free(info->pdata->gpio_irq);
	kfree(info->perf_irq_data);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM)
static int aps_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aps_ts_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users) {
		aps_ts_disable(info);
		aps_clear_input_data(info);
	}

	mutex_unlock(&info->input_dev->mutex);
	return 0;

}

static int aps_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aps_ts_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		aps_ts_enable(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}
#endif

#ifdef CONFIG_FB
static int fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct aps_ts_info *info = container_of(self, struct aps_ts_info,
		fb_notif);

	if (!evdata || !evdata->data || !event == FB_EVENT_BLANK
		|| !info || !info->client)
		return 0;

	blank = evdata->data;
	if (*blank == FB_BLANK_UNBLANK) {
		aps_ts_resume(&info->client->dev);
		pr_debug("DISPLAY-ON\n");
	} else if (*blank == FB_BLANK_POWERDOWN) {
		aps_ts_suspend(&info->client->dev);
		pr_debug("DISPLAY-OFF\n");
	}

	return 0;
}
#endif

#if defined(CONFIG_PM)
static const struct dev_pm_ops aps_ts_pm_ops = {
#if !defined(CONFIG_FB)
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
	{ .compatible = "melfas,aps_ts",},
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
MODULE_DESCRIPTION("APS Touchscreen driver");
MODULE_LICENSE("GPL");
