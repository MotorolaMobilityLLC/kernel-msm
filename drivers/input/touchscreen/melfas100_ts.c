/*
 * mms_ts.c - Touchscreen driver for Melfas MMS-series touch controllers
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *         Simon Wilson <simonwilson@google.com>
 *
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * ISP reflashing code based on original code from Melfas.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Initial driver source obtained from
 * https://android.googlesource.com/kernel/omap.git android-omap-tuna-3.0
 * branch, commit-id 4bcb2853a2c52a90e728e6af812032a479f8d17b
 */

#define DEBUG
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/input/touch_platform.h>
#include <linux/melfas100_ts.h>

#include <asm/unaligned.h>

#define MAX_FINGERS		10
#define MAX_WIDTH		30
#define MAX_PRESSURE		255

/* Registers */
#define MMS_MODE_CONTROL	0x01
#define MMS_XYRES_HI		0x02
#define MMS_XRES_LO		0x03
#define MMS_YRES_LO		0x04

#define MMS_INPUT_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT0	0x10
#define	FINGER_EVENT_SZ		6

#define MMS_TSP_REVISION	0xF0
#define MMS_HW_REVISION		0xF1
#define MMS_COMPAT_GROUP	0xF2
#define MMS_FW_VERSION		0xF3

#define TS_DBG_LVL_0    0
#define TS_DBG_LVL_1    1
#define TS_DBG_LVL_2    2
#define TS_DBG_LVL_3    3

enum {
	ISP_MODE_FLASH_ERASE	= 0x59F3,
	ISP_MODE_FLASH_WRITE	= 0x62CD,
	ISP_MODE_FLASH_READ	= 0x6AC9,
};

static uint8_t tsdebug;

/* each address addresses 4-byte words */
#define ISP_MAX_FW_SIZE		(0x1F00 * 4)
#define ISP_IC_INFO_ADDR	0x1F00

static bool mms_force_reflash;
module_param_named(force_reflash, mms_force_reflash, bool, S_IWUSR | S_IRUGO);

static bool mms_flash_from_probe;
module_param_named(flash_from_probe, mms_flash_from_probe, bool,
		   S_IWUSR | S_IRUGO);

static bool mms_die_on_flash_fail;
module_param_named(die_on_flash_fail, mms_die_on_flash_fail, bool,
		   S_IWUSR | S_IRUGO);

struct mms_ts_info {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	char				phys[32];

	int				max_x;
	int				max_y;

	bool				invert_x;
	bool				invert_y;

	int				irq;

	struct touch_platform_data	*pdata;

	struct melfas_ts_version_info version_info;

	char				*fw_name;
	struct completion		init_done;
	struct early_suspend		early_suspend;

	/* protects the enabled flag */
	struct mutex			lock;
	int				status;
	bool				enabled;
};

struct mms_fw_image {
	__le32 hdr_len;
	__le32 data_len;
	__le32 fw_ver;
	__le32 hdr_ver;
	u8 data[0];
} __packed;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
#endif

static irqreturn_t mms_ts_interrupt(int irq, void *dev_id)
{
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGERS*FINGER_EVENT_SZ] = { 0 };
	int ret;
	int i;
	int sz;
	u8 reg = MMS_INPUT_EVENT0;
	struct i2c_msg msg[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.buf    = &reg,
			.len    = 1,
		}, {
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.buf    = buf,
		},
	};

	sz = i2c_smbus_read_byte_data(client, MMS_INPUT_EVENT_PKT_SZ);
	if (sz < 0) {
		dev_err(&client->dev, "%s bytes=%d\n", __func__, sz);
		goto out;
	}
	if (tsdebug >= TS_DBG_LVL_1) {
		dev_dbg(&client->dev, "bytes available: %d\n", sz);
	}
	BUG_ON(sz > MAX_FINGERS*FINGER_EVENT_SZ);
	if (sz == 0)
		goto out;

	msg[1].len = sz;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev,
			"failed to read %d bytes of touch data (%d)\n",
			sz, ret);
		goto out;
	}

#if defined(VERBOSE_DEBUG)
	print_hex_dump(KERN_DEBUG, "mms_ts raw: ",
		       DUMP_PREFIX_OFFSET, 32, 1, buf, sz, false);
#endif
	for (i = 0; i < sz; i += FINGER_EVENT_SZ) {
		u8 *tmp = &buf[i];
		int id = (tmp[0] & 0xf) - 1;
		int x = tmp[2] | ((tmp[1] & 0xf) << 8);
		int y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);

		if (info->invert_x) {
			x = info->max_x - x;
			if (x < 0)
				x = 0;
		}
		if (info->invert_y) {
			y = info->max_y - y;
			if (y < 0)
				y = 0;
		}

		if ((tmp[0] & 0x80) == 0) {
			if (tsdebug >= TS_DBG_LVL_1) {
				dev_dbg(&client->dev,
						"finger %d up\n", id);
			}
			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, false);
			continue;
		}

		input_mt_slot(info->input_dev, id);
		input_mt_report_slot_state(info->input_dev,
					   MT_TOOL_FINGER, true);
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, tmp[4]);
		input_report_abs(info->input_dev, ABS_MT_PRESSURE, tmp[5]);
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);

		if (tsdebug >= TS_DBG_LVL_1) {
			dev_dbg(&client->dev,
				"finger %d: x=%d y=%d p=%d w=%d\n",
				id, x, y, tmp[5], tmp[4]);
		}
	}

	input_sync(info->input_dev);

	if (info->pdata->get_dbg_lvl &&
		(info->pdata->get_dbg_lvl() >= 1) &&
		info->pdata->int_latency)
			info->pdata->int_latency();
out:
	return IRQ_HANDLED;
}

static irqreturn_t mms_ts_primary_handler(int irq, void *handle)
{
	struct mms_ts_info *ts = (struct mms_ts_info *)handle;

	if (ts->pdata->get_dbg_lvl &&
		(ts->pdata->get_dbg_lvl() >= 1) &&
			(ts->pdata->int_time))
				ts->pdata->int_time();

	return IRQ_WAKE_THREAD;
}

static void hw_reboot(struct mms_ts_info *info, bool bootloader)
{
	gpio_direction_output(info->pdata->gpio_reset, 0);
	gpio_direction_output(info->pdata->gpio_sda, bootloader ? 0 : 1);
	gpio_direction_output(info->pdata->gpio_scl, bootloader ? 0 : 1);
	gpio_direction_output(info->pdata->gpio_interrupt, 0);
	msleep(30);
	gpio_set_value(info->pdata->gpio_reset, 1);
	msleep(30);

	if (bootloader) {
		gpio_set_value(info->pdata->gpio_scl, 0);
		gpio_set_value(info->pdata->gpio_sda, 1);
	} else {
		gpio_set_value(info->pdata->gpio_interrupt, 1);
		gpio_direction_input(info->pdata->gpio_interrupt);
		gpio_direction_input(info->pdata->gpio_scl);
		gpio_direction_input(info->pdata->gpio_sda);
	}
	msleep(40);
}

static inline void hw_reboot_bootloader(struct mms_ts_info *info)
{
	hw_reboot(info, true);
}

static inline void hw_reboot_normal(struct mms_ts_info *info)
{
	hw_reboot(info, false);
}

static inline void mms_pwr_on_reset(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	if (!info->pdata->mux_fw_flash) {
		dev_err(&info->client->dev,
			"missing platform data, can't do power-on-reset\n");
		return;
	}

	i2c_lock_adapter(adapter);
	info->pdata->mux_fw_flash(true);

	gpio_direction_output(info->pdata->gpio_reset, 0);
	gpio_direction_output(info->pdata->gpio_sda, 1);
	gpio_direction_output(info->pdata->gpio_scl, 1);
	gpio_direction_output(info->pdata->gpio_interrupt, 1);
	msleep(50);
	gpio_direction_output(info->pdata->gpio_reset, 1);
	msleep(50);

	info->pdata->mux_fw_flash(false);
	i2c_unlock_adapter(adapter);

	/* TODO: Seems long enough for the firmware to boot.
	 * Find the right value */
	msleep(250);
}

static void isp_toggle_clk(struct mms_ts_info *info, int start_lvl, int end_lvl,
			   int hold_us)
{
	gpio_set_value(info->pdata->gpio_scl, start_lvl);
	udelay(hold_us);
	gpio_set_value(info->pdata->gpio_scl, end_lvl);
	udelay(hold_us);
}

/* 1 <= cnt <= 32 bits to write */
static void isp_send_bits(struct mms_ts_info *info, u32 data, int cnt)
{
	gpio_direction_output(info->pdata->gpio_interrupt, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 0);

	/* clock out the bits, msb first */
	while (cnt--) {
		gpio_set_value(info->pdata->gpio_sda, (data >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(info, 1, 0, 3);
	}
}

/* 1 <= cnt <= 32 bits to read */
static u32 isp_recv_bits(struct mms_ts_info *info, int cnt)
{
	u32 data = 0;

	gpio_direction_output(info->pdata->gpio_interrupt, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_set_value(info->pdata->gpio_sda, 0);
	gpio_direction_input(info->pdata->gpio_sda);

	/* clock in the bits, msb first */
	while (cnt--) {
		isp_toggle_clk(info, 0, 1, 1);
		data = (data << 1) | (!!gpio_get_value(info->pdata->gpio_sda));
	}

	gpio_direction_output(info->pdata->gpio_sda, 0);
	return data;
}

static void isp_enter_mode(struct mms_ts_info *info, u32 mode)
{
	int cnt;
	unsigned long flags;

	local_irq_save(flags);
	gpio_direction_output(info->pdata->gpio_interrupt, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 1);

	mode &= 0xffff;
	for (cnt = 15; cnt >= 0; cnt--) {
		gpio_set_value(info->pdata->gpio_interrupt, (mode >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(info, 1, 0, 3);
	}

	gpio_set_value(info->pdata->gpio_interrupt, 0);
	local_irq_restore(flags);
}

static void isp_exit_mode(struct mms_ts_info *info)
{
	int i;
	unsigned long flags;

	local_irq_save(flags);
	gpio_direction_output(info->pdata->gpio_interrupt, 0);
	udelay(3);

	for (i = 0; i < 10; i++)
		isp_toggle_clk(info, 1, 0, 3);
	local_irq_restore(flags);
}

static void flash_set_address(struct mms_ts_info *info, u16 addr)
{
	/* Only 13 bits of addr are valid.
	 * The addr is in bits 13:1 of cmd */
	isp_send_bits(info, (u32)(addr & 0x1fff) << 1, 18);
}

static void flash_erase(struct mms_ts_info *info)
{
	isp_enter_mode(info, ISP_MODE_FLASH_ERASE);

	gpio_direction_output(info->pdata->gpio_interrupt, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 1);

	/* 4 clock cycles with different timings for the erase to
	 * get processed, clk is already 0 from above */
	udelay(7);
	isp_toggle_clk(info, 1, 0, 3);
	udelay(7);
	isp_toggle_clk(info, 1, 0, 3);
	usleep_range(25000, 35000);
	isp_toggle_clk(info, 1, 0, 3);
	usleep_range(150, 200);
	isp_toggle_clk(info, 1, 0, 3);

	gpio_set_value(info->pdata->gpio_sda, 0);

	isp_exit_mode(info);
}

static u32 flash_readl(struct mms_ts_info *info, u16 addr)
{
	int i;
	u32 val;
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(info, ISP_MODE_FLASH_READ);
	flash_set_address(info, addr);

	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 0);
	udelay(40);

	/* data load cycle */
	for (i = 0; i < 6; i++)
		isp_toggle_clk(info, 1, 0, 10);

	val = isp_recv_bits(info, 32);
	isp_exit_mode(info);
	local_irq_restore(flags);

	return val;
}

static void flash_writel(struct mms_ts_info *info, u16 addr, u32 val)
{
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(info, ISP_MODE_FLASH_WRITE);
	flash_set_address(info, addr);
	isp_send_bits(info, val, 32);

	gpio_direction_output(info->pdata->gpio_sda, 1);
	/* 6 clock cycles with different timings for the data to get written
	 * into flash */
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 6);
	isp_toggle_clk(info, 0, 1, 12);
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 3);

	isp_toggle_clk(info, 1, 0, 1);

	gpio_direction_output(info->pdata->gpio_sda, 0);
	isp_exit_mode(info);
	local_irq_restore(flags);
	usleep_range(300, 400);
}

static bool flash_is_erased(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 val;
	u16 addr;

	for (addr = 0; addr < (ISP_MAX_FW_SIZE / 4); addr++) {
		udelay(40);
		val = flash_readl(info, addr);

		if (val != 0xffffffff) {
			if (tsdebug >= TS_DBG_LVL_1) {
				dev_dbg(&client->dev,
				"addr 0x%x not erased: 0x%08x != 0xffffffff\n",
				addr, val);
			}
			return false;
		}
	}
	return true;
}

static int fw_write_image(struct mms_ts_info *info, const u8 *data, size_t len)
{
	struct i2c_client *client = info->client;
	u16 addr = 0;

	for (addr = 0; addr < (len / 4); addr++, data += 4) {
		u32 val = get_unaligned_le32(data);
		u32 verify_val;
		int retries = 3;

		while (retries--) {
			flash_writel(info, addr, val);
			verify_val = flash_readl(info, addr);
			if (val == verify_val)
				break;
			dev_err(&client->dev,
				"mismatch @ addr 0x%x: 0x%x != 0x%x\n",
				addr, verify_val, val);
			hw_reboot_bootloader(info);
			continue;
		}
		if (retries < 0)
			return -ENXIO;
	}

	return 0;
}

static int fw_download(struct mms_ts_info *info, const u8 *data, size_t len)
{
	struct i2c_client *client = info->client;
	u32 val;
	int ret = 0;

	if (len % 4) {
		dev_err(&client->dev,
			"fw image size (%d) must be a multiple of 4 bytes\n",
			len);
		return -EINVAL;
	} else if (len > ISP_MAX_FW_SIZE) {
		dev_err(&client->dev,
			"fw image is too big, %d > %d\n", len, ISP_MAX_FW_SIZE);
		return -EINVAL;
	}

	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev, "fw download started\n");
	}

	gpio_direction_output(info->pdata->gpio_reset, 0);
	gpio_direction_output(info->pdata->gpio_sda, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_interrupt, 0);

	hw_reboot_bootloader(info);

	val = flash_readl(info, ISP_IC_INFO_ADDR);
	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev,
			"IC info: 0x%02x (%x)\n", val & 0xff, val);
	}

	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev, "fw erase...\n");
	}
	flash_erase(info);
	if (!flash_is_erased(info)) {
		ret = -ENXIO;
		goto err;
	}

	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev, "fw write...\n");
	}
	/* XXX: what does this do?! */
	flash_writel(info, ISP_IC_INFO_ADDR, 0xffffff00 | (val & 0xff));
	usleep_range(1000, 1500);
	ret = fw_write_image(info, data, len);
	if (ret)
		goto err;
	usleep_range(1000, 1500);

	hw_reboot_normal(info);
	usleep_range(1000, 1500);
	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev, "fw download done...\n");
	}
	return 0;

err:
	dev_err(&client->dev, "fw download failed...\n");
	hw_reboot_normal(info);
	return ret;
}

static int get_fw_version(struct mms_ts_info *info)
{
	int ret;
	int retries = 3;

	/* this seems to fail sometimes after a reset.. retry a few times */
	do {
		ret = i2c_smbus_read_byte_data(info->client, MMS_FW_VERSION);
	} while (ret < 0 && retries-- > 0);

	return ret;
}

static int mms_ts_enable(struct mms_ts_info *info)
{
	mutex_lock(&info->lock);
	if (info->enabled)
		goto out;
	/* wake up the touch controller. */
	gpio_set_value(info->pdata->gpio_reset, 1);
	info->enabled = true;
	enable_irq(info->irq);
out:
	mutex_unlock(&info->lock);
	return 0;
}

static int mms_ts_disable(struct mms_ts_info *info)
{
	mutex_lock(&info->lock);
	if (!info->enabled)
		goto out;
	disable_irq(info->irq);
	gpio_set_value(info->pdata->gpio_reset, 0);
	info->enabled = false;
out:
	mutex_unlock(&info->lock);
	return 0;
}

static int mms_ts_input_open(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);
	int ret;

	ret = wait_for_completion_interruptible_timeout(&info->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));

	if (ret > 0) {
		if (info->irq != -1)
			ret = mms_ts_enable(info);
		else
			ret = -ENXIO;
	} else if (ret < 0) {
		dev_err(&dev->dev,
			"error while waiting for device to init (%d)\n", ret);
		ret = -ENXIO;
	} else if (ret == 0) {
		dev_err(&dev->dev,
			"timedout while waiting for device to init\n");
		ret = -ENXIO;
	}

	return ret;
}

static void mms_ts_input_close(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);
	mms_ts_disable(info);
}

static int mms_ts_finish_config(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	ret = request_threaded_irq(client->irq, mms_ts_primary_handler,
				mms_ts_interrupt,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"mms_ts", info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_req_irq;
	}
	disable_irq(client->irq);

	info->irq = client->irq;
	barrier();

	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev,
			"%s: Melfas MMS-series touch controller initialized\n",
			__func__);
	}

	complete_all(&info->init_done);
	return 0;

err_req_irq:
	return ret;
}

static void mms_ts_fw_load(const struct firmware *fw, void *context)
{
	struct mms_ts_info *info = context;
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret = 0;
	int ver;
	int retries = 3;
	struct mms_fw_image *fw_img;

	info->status = info->status & ~(1 << MELFAS_WAITING_FOR_FW_FLAG);
	ver = get_fw_version(info);
	if (ver < 0) {
		ver = 0;
		dev_err(&client->dev,
			"can't read version, controller dead? forcing reflash");
	}

	if (!fw) {
		if (tsdebug >= TS_DBG_LVL_2) {
			dev_info(&client->dev,
				"could not find firmware file '%s'\n",
				info->fw_name);
		}
		goto done;
	}

	fw_img = (struct mms_fw_image *)fw->data;

	if (ver == fw_img->fw_ver && !mms_force_reflash) {
		if (tsdebug >= TS_DBG_LVL_2) {
			dev_info(&client->dev,
				"fw version 0x%02x already present\n", ver);
		}
		goto done;
	}

	if (!info->pdata || !info->pdata->mux_fw_flash) {
		dev_err(&client->dev,
			"fw cannot be updated, missing platform data\n");
		goto err;
	}

	while (retries--) {
		i2c_lock_adapter(adapter);
		info->pdata->mux_fw_flash(true);

		ret = fw_download(info, fw->data, fw->size);

		info->pdata->mux_fw_flash(false);
		i2c_unlock_adapter(adapter);

		if (ret < 0) {
			dev_err(&client->dev,
				"error updating firmware to version 0x%02x\n",
				fw_img->fw_ver);
			if (retries) {
				if (tsdebug >= TS_DBG_LVL_2) {
					dev_info(&client->dev,
						"retrying flashing\n");
				}
			}
			continue;
		}

		ver = get_fw_version(info);
		goto done;
	}

	dev_err(&client->dev, "could not flash firmware, ran out of retries\n");
	BUG_ON(mms_die_on_flash_fail);

err:
	/* complete anyway, so open() doesn't get blocked */
	complete_all(&info->init_done);
	goto out;

done:
	mms_ts_finish_config(info);
out:
	release_firmware(fw);
}

static int __devinit mms_ts_config(struct mms_ts_info *info, bool nowait)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	const char *filename = info->pdata->fw_name ?: "mms144_ts.fw";

	mms_pwr_on_reset(info);

	if (nowait) {
		const struct firmware *fw;
		info->fw_name = kasprintf(GFP_KERNEL, "%s", filename);
		ret = request_firmware(&fw, info->fw_name, &client->dev);
		if (ret) {
			dev_err(&client->dev,
				"error requesting built-in firmware: %d\n",
				ret);
			goto out;
		}
		mms_ts_fw_load(fw, info);
	} else {
		info->fw_name = kstrdup(filename, GFP_KERNEL);
		ret = request_firmware_nowait(THIS_MODULE, true, info->fw_name,
					      &client->dev, GFP_KERNEL,
					      info, mms_ts_fw_load);
		if (ret)
			dev_err(&client->dev,
				"cannot schedule firmware update (%d)\n", ret);
	}

out:
	return ret;
}

/* Driver debugging */
static ssize_t melfas_ts_drv_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Debug Setting: %u\n", tsdebug);
}

static ssize_t melfas_ts_drv_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value;
	int err = kstrtoul(buf, 0, &value);
	if (err < 0) {
		pr_err("%s: illegal sysfs data\n", __func__);
	return -EINVAL;
	}

	switch (value) {
	case TS_DBG_LVL_0:
	case TS_DBG_LVL_1:
	case TS_DBG_LVL_2:
	case TS_DBG_LVL_3:
		pr_info("%s: Debug setting=%d\n", __func__, (int)value);
		tsdebug = value;
	break;
	default:
		pr_err("%s: Invalid option, 1/0 = "
			"Intr enable/disable setting=%d\n",
			__func__, (int)value);
	break;
	}

	return size;
}
static DEVICE_ATTR(drv_debug, S_IWUSR | S_IRUGO, melfas_ts_drv_debug_show,
			melfas_ts_drv_debug_store);

static ssize_t melfas_latency_debug_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *ts = dev_get_drvdata(dev);
	int ret = 0;

	if (ts->pdata->get_dbg_lvl)
		ret = sprintf(buf, "Latency Debug Setting: %u\n", \
				ts->pdata->get_dbg_lvl());

	return ret;
}

static ssize_t melfas_latency_debug_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t size)
{
	struct mms_ts_info *ts = dev_get_drvdata(dev);
	unsigned long value;
	u64 *debug_ptr1;
	u32 *debug_ptr2;
	int err = kstrtoul(buf, 0, &value);
	if (err < 0) {
		pr_err("%s: illegal sysfs data\n", __func__);
		return -EINVAL;
	}

	switch (value) {
	case 0:
	if (ts->pdata->get_lat_ptr && ts->pdata->get_time_ptr &&
		ts->pdata->set_lat_ptr && ts->pdata->set_time_ptr) {
			ts->pdata->get_lat_ptr(&debug_ptr2);
			ts->pdata->get_time_ptr(&debug_ptr1);
			if ((debug_ptr1 > 0) && (debug_ptr2 > 0)) {
				kfree(debug_ptr1);
				kfree(debug_ptr2);
			}
			ts->pdata->set_lat_ptr(0);
			ts->pdata->set_time_ptr(0);
		}
	case 1:
	if (ts->pdata->get_lat_ptr && ts->pdata->get_time_ptr &&
		ts->pdata->set_lat_ptr && ts->pdata->set_time_ptr) {
			ts->pdata->get_lat_ptr(&debug_ptr2);
			ts->pdata->get_time_ptr(&debug_ptr1);
			if ((debug_ptr1 > 0) && (debug_ptr2 > 0)) {
				kfree(debug_ptr1);
				kfree(debug_ptr2);
			}
			ts->pdata->set_lat_ptr(0);
			ts->pdata->set_time_ptr(0);
		}
	case 2:
		if (ts->pdata->set_dbg_lvl)
			ts->pdata->set_dbg_lvl(value);
		break;
	default:
		pr_err("%s: %d Is an invalid option", __func__, (int)value);
		break;
	}

	if (ts->pdata->get_dbg_lvl && (ts->pdata->get_dbg_lvl() == value))
		pr_info("%s: Latency debug setting=%d\n", __func__, (int)value);

	return size;
}
static DEVICE_ATTR(latency_debug, S_IWUSR | S_IRUGO, \
			melfas_latency_debug_show, melfas_latency_debug_store);

static ssize_t melfas_ts_hw_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct mms_ts_info *ts = dev_get_drvdata(dev);
	unsigned long value;
	int err = kstrtoul(buf, 0, &value);
	if (err < 0) {
		pr_err("%s: illegal sysfs data\n", __func__);
		return -EINVAL;
	}

	disable_irq_nosync(ts->client->irq);

	if (value) {
		gpio_set_value(ts->pdata->gpio_reset, 0);
		msleep(50);
		gpio_set_value(ts->pdata->gpio_reset, 1);
		msleep(150);
		pr_info("%s: IC has been reset\n", __func__);
	}

	enable_irq(ts->client->irq);

	return size;
}
static DEVICE_ATTR(hw_reset, S_IWUSR | S_IRUGO, NULL,
			melfas_ts_hw_reset_store);


/* interrupt status */
static ssize_t melfas_ts_hw_irqstat_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *ts = i2c_get_clientdata(client);

	err = gpio_get_value(ts->pdata->gpio_interrupt);
	switch (err) {
	case 0:
		err = sprintf(buf, "Interrupt line is LOW.\n");
	break;
	case 1:
		err = sprintf(buf, "Interrupt line is HIGH.\n");
	break;
	default:
		err = sprintf(buf, "Function irq_stat() returned %d.\n", err);
	break;
	}

	return err;
}
static DEVICE_ATTR(hw_irqstat, S_IWUSR | S_IRUGO,
			melfas_ts_hw_irqstat_show, NULL);


static ssize_t melfas_ts_irq_enabled_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *ts = i2c_get_clientdata(client);
	return sprintf(buf, "Interrupt: %u\n", ts->enabled);
}

static ssize_t melfas_ts_irq_enabled_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *ts = i2c_get_clientdata(client);
	unsigned long value;
	int err = kstrtoul(buf, 0, &value);
	if (err < 0) {
		pr_err("%s: illegal sysfs data\n", __func__);
		return -EINVAL;
	}

	switch (value) {
	case 0:
		disable_irq(ts->irq);
		ts->enabled = false;
		pr_info("%s: touch irq disabled, %d\n",
				__func__, ts->enabled);
	break;
	case 1:
		ts->enabled = true;
		enable_irq(ts->irq);
		pr_info("%s: touch irq enabled, %d\n",
				__func__, ts->enabled);
	break;
	default:
		pr_info("melfas_irq_enable failed -> irq_enabled = %d\n",
				ts->enabled);
	break;
	}

	return size;
}
static DEVICE_ATTR(irq_enabled, S_IWUSR | S_IRUGO, melfas_ts_irq_enabled_show,
					melfas_ts_irq_enabled_store);

static ssize_t melfas_ts_reset_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *ts = i2c_get_clientdata(client);
	int	pin = gpio_get_value(ts->pdata->gpio_reset);
	return sprintf(buf, "Reset: %s\n",
				(pin ? "HIGH" : "LOW"));
}

static ssize_t melfas_ts_reset_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *ts = i2c_get_clientdata(client);
	unsigned long value;
	int	pin = -1;
	int err = kstrtoul(buf, 0, &value);
	if (err < 0) {
		pr_err("%s: illegal sysfs data\n", __func__);
		return -EINVAL;
	}

	switch (value) {
	case 0:
		gpio_set_value(ts->pdata->gpio_reset, 0);
	break;
	case 1:
		gpio_set_value(ts->pdata->gpio_reset, 1);
	break;
	default:
		pin = gpio_get_value(ts->pdata->gpio_reset);
		pr_info("%s: failed: No value\n", __func__);

	break;
	}
	pin = gpio_get_value(ts->pdata->gpio_reset);
	pr_info("%s: reset is set to %s\n",
		__func__,
		(pin ? "HIGH" : "LOW"));

	return size;
}
static DEVICE_ATTR(drv_reset, S_IWUSR | S_IRUGO, melfas_ts_reset_show,
					melfas_ts_reset_set);


static ssize_t melfas_ts_ic_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	int	ret;

	ret = i2c_smbus_read_i2c_block_data(info->client,
			MMS_TSP_REVISION,
			sizeof(info->version_info),
			(u8 *)&info->version_info);
	if (ret < 0)
		return sprintf(buf, "failed getting information\n");

	return sprintf(buf, " Panel Version: %x, HW Revision: %x\n"
		"HW Comp Grp: %x,"
		" Core FW ver: %x\n Private FW ver: %x,"
		"Public FW ver: %x\n",\
		info->version_info.panel_ver, info->version_info.hw_ver, \
		info->version_info.hw_comp_grp, info->version_info.core_fw_ver,\
		info->version_info.priv_fw_ver, info->version_info.pub_fw_ver);
}
static DEVICE_ATTR(ic_ver, S_IWUSR | S_IRUGO, melfas_ts_ic_ver_show, NULL);

static ssize_t melfas_latency_values_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *ts = dev_get_drvdata(dev);
	int ret = 0;

	if (ts->pdata->get_avg_lat && ts->pdata->get_high_lat &&
		ts->pdata->get_slow_cnt && ts->pdata->get_int_cnt) {
		ret = sprintf(buf, "Touch Latency Time: Average %duS,"\
				" High %duS,%d interrupts of %d were slow\n",
				ts->pdata->get_avg_lat(),
				ts->pdata->get_high_lat(),
				ts->pdata->get_slow_cnt(),
				ts->pdata->get_int_cnt());
	}

	return ret;
}
static DEVICE_ATTR(latency_values, S_IWUSR | S_IRUGO, \
			melfas_latency_values_show, NULL);

static ssize_t melfas_all_latency_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *ts = dev_get_drvdata(dev);
	u32 *latency_ptr;
	u32 latency_len;
	u32 buf_offset = 0;
	u32 index = 0;

	if (ts->pdata->get_lat_ptr && (ts->pdata->get_dbg_lvl() >= 2)) {
		latency_len = ts->pdata->get_lat_ptr(&latency_ptr);
		if (latency_ptr > 0) {
			do {
				/* if changing the string to be output to sysfs
				also update the (PAGE_SIZE - x) below
				so that x = the max number of bytes the string
				can be + 1 for the null terminator.
				sysfs limits the size of a single read to
				be <= memory page size  */
				buf_offset += sprintf(buf + buf_offset, "%d,",
							latency_ptr[index++]);
			} while ((buf_offset < (PAGE_SIZE - 13)) &&
				(index < (latency_len / sizeof(u32))));
		}
	}

	return buf_offset;
}
static DEVICE_ATTR(latency_times, S_IWUSR | S_IRUGO, \
			melfas_all_latency_show, NULL);

static ssize_t melfas_debug_ic_reflash_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *ts = dev_get_drvdata(dev);

	if (ts->status & (1 << MELFAS_WAITING_FOR_FW_FLAG))
		return sprintf(buf, "Driver is waiting for firmware load.\n");
	else
		return sprintf(buf, "No firmware loading in progress.\n");
}

static ssize_t melfas_debug_ic_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct mms_ts_info *ts = dev_get_drvdata(dev);


	if (ts->status & (1 << MELFAS_WAITING_FOR_FW_FLAG)) {
		printk(KERN_ERR "%s: Driver is already waiting for firmware.\n",
			__func__);
		err = -EALREADY;
		goto melfas_debug_ic_reflash_store_fail;
	}

	printk(KERN_INFO "%s: Enabling firmware class loader...\n", __func__);

	err = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_NOHOTPLUG, "", &(ts->client->dev),
		GFP_KERNEL, ts, mms_ts_fw_load);
	if (err < 0) {
		printk(KERN_ERR
		"%s: Firmware request failed with error code %d.\n",
		__func__, err);
		goto melfas_debug_ic_reflash_store_fail;
	}

	ts->status = ts->status | (1 << MELFAS_WAITING_FOR_FW_FLAG);
	err = size;

melfas_debug_ic_reflash_store_fail:

	return err;
}

static DEVICE_ATTR(ic_reflash, S_IWUSR | S_IRUGO,
		melfas_debug_ic_reflash_show, melfas_debug_ic_reflash_store);

static int __devinit mms_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct touch_platform_data *pdata;
	struct input_dev *input_dev;
	int ret = 0;

	if (tsdebug >= TS_DBG_LVL_1) {
		dev_dbg(&client->dev, "Entering probe\n");
	}
	mms_force_reflash = false;
	mms_flash_from_probe = false;
	mms_die_on_flash_fail = false;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		ret =  -EIO;
		goto err_check_functionality_failed;
	}

	info = kzalloc(sizeof(struct mms_ts_info), GFP_KERNEL);
	if (!info) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate memory.\n");
		goto err_alloc_info_failed;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate device.\n");
		goto err_alloc_device_failed;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->pdata = client->dev.platform_data;
	info->status = 0;
	pdata = client->dev.platform_data;
	init_completion(&info->init_done);
	info->irq = -1;
	mutex_init(&info->lock);

	if (info->pdata) {
		info->max_x = info->pdata->max_x;
		info->max_y = info->pdata->max_y;
		info->invert_x = info->pdata->invert_x;
		info->invert_y = info->pdata->invert_y;
	} else {
		info->max_x = 720;
		info->max_y = 1280;
	}

	input_mt_init_slots(input_dev, MAX_FINGERS);

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	input_dev->name = MELFAS_TS_NAME;
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = mms_ts_input_open;
	input_dev->close = mms_ts_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, info->max_y, 0, 0);

	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev (%d)\n",
			ret);
		goto err_input_register_device_failed;
	}

	i2c_set_clientdata(client, info);

	/* Get version information from the IC */
	ret = i2c_smbus_read_i2c_block_data(info->client,
			MMS_TSP_REVISION,
			sizeof(info->version_info),
			(u8 *)&info->version_info);
	if (ret < 0) {
		if (ret == -ENOTCONN) {
			pr_err("%s: device not found\n", __func__);
			goto err_device_not_found;
		}
		pr_err("%s: i2c version read fail, force reflash\n", __func__);
	}

	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev,
			"%s:Panel Version: %x,HW Revision: %x,HW Comp Grp: %x,"
			" Core FW ver: %x,Private FW ver: %x,"
			"Public FW ver: %x\n", __func__,
			info->version_info.panel_ver,
			info->version_info.hw_ver,
			info->version_info.hw_comp_grp,
			info->version_info.core_fw_ver,
			info->version_info.priv_fw_ver,
			info->version_info.pub_fw_ver);
		dev_info(&client->dev,
			"%s: Platform data: core: %x, priv: %x, pub: %x\n",
			__func__,
			pdata->fw->ver[0],
			pdata->fw->private_fw_v[0],
			pdata->fw->public_fw_v[0]);
	}
	/* if the version of the IC firmware is 0x56, always reflash. It was a
	 * bad version number from Melfas
	 */
	if (ret < 0 || (info->version_info.core_fw_ver == 0x56) ||
		info->version_info.core_fw_ver < pdata->fw->ver[0] ||
		info->version_info.priv_fw_ver < pdata->fw->private_fw_v[0] ||
		info->version_info.pub_fw_ver < pdata->fw->public_fw_v[0]) {
		ret = mms_ts_config(info, mms_flash_from_probe);
		if (ret) {
			dev_err(&client->dev,
				"failed to initialize (%d)\n", ret);
			goto err_config;
		} else {
			if (tsdebug >= TS_DBG_LVL_2) {
				dev_info(&client->dev,
					"mms_ts_config is finished\n");
			}
		}
	} else {
		mms_ts_finish_config(info);
		if (tsdebug >= TS_DBG_LVL_2) {
			dev_info(&client->dev,
				"New firmware is not going to be downloaded!");
		}
	}
	ret = device_create_file(&info->client->dev, &dev_attr_hw_reset);
	if (ret) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_hw_reset_device_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_drv_reset);
	if (ret) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_drv_reset_device_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_drv_debug);
	if (ret) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_dbg_device_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_irq_enabled);
	if (ret) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_irq_enabled_device_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_ic_ver);
	if (ret) {
		pr_err("%s:File device creation failed: %d\n", __func__, ret);
		ret = -ENODEV;
		goto err_create_ic_ver_device_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_hw_irqstat);
	if (ret) {
		pr_err("%s:irqstat device creation failed: %d\n",
			__func__, ret);
		ret = -ENODEV;
		goto err_create_hw_irqstat_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_ic_reflash);
	if (ret) {
		pr_err("%s: ic_reflash file device creation failed: %d\n",
		       __func__, ret);
		ret = -ENODEV;
		goto err_create_hw_ic_reflash_failed;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_latency_debug);
	if (ret) {
		pr_err("%s:latency debug file device creation failed: %d\n",
		       __func__, ret);
		ret = -ENODEV;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_latency_values);
	if (ret) {
		pr_err("%s:latency values file device creation failed: %d\n",
		       __func__, ret);
		ret = -ENODEV;
	}

	ret = device_create_file(&info->client->dev, &dev_attr_latency_times);
	if (ret) {
		pr_err("%s:all latency file device creation failed: %d\n",
		       __func__, ret);
		ret = -ENODEV;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = mms_ts_early_suspend;
	info->early_suspend.resume = mms_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	if (tsdebug >= TS_DBG_LVL_2) {
		dev_info(&client->dev, "Probe completed!\n");
	}
	return 0;

err_create_hw_ic_reflash_failed:
	device_remove_file(&info->client->dev, &dev_attr_hw_irqstat);
err_create_hw_irqstat_failed:
	device_remove_file(&info->client->dev, &dev_attr_ic_ver);
err_create_ic_ver_device_failed:
	device_remove_file(&info->client->dev, &dev_attr_irq_enabled);
err_create_irq_enabled_device_failed:
	device_remove_file(&info->client->dev, &dev_attr_drv_debug);
err_create_dbg_device_failed:
	device_remove_file(&info->client->dev, &dev_attr_drv_reset);
err_create_drv_reset_device_failed:
	device_remove_file(&info->client->dev, &dev_attr_hw_reset);
err_create_hw_reset_device_failed:
err_config:
err_device_not_found:
	input_unregister_device(info->input_dev);
err_input_register_device_failed:
	input_free_device(info->input_dev);
	input_dev = NULL;
err_alloc_device_failed:
	kfree(info->fw_name);
	kfree(info);
err_alloc_info_failed:
err_check_functionality_failed:
	return ret;
}

static int __devexit mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);
	u64 *debug_ptr1;
	u32 *debug_ptr2;

	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);
	kfree(info->fw_name);

	device_remove_file(&info->client->dev, &dev_attr_ic_ver);
	device_remove_file(&info->client->dev, &dev_attr_drv_debug);
	device_remove_file(&info->client->dev, &dev_attr_irq_enabled);
	device_remove_file(&info->client->dev, &dev_attr_latency_debug);
	device_remove_file(&info->client->dev, &dev_attr_latency_times);
	device_remove_file(&info->client->dev, &dev_attr_latency_values);

	if (info->pdata->get_lat_ptr && info->pdata->get_time_ptr &&
		info->pdata->set_lat_ptr && info->pdata->set_time_ptr) {
		info->pdata->get_lat_ptr(&debug_ptr2);
		info->pdata->get_time_ptr(&debug_ptr1);
		if ((debug_ptr1 > 0) && (debug_ptr2 > 0)) {
			kfree(debug_ptr1);
			kfree(debug_ptr2);
		}
		info->pdata->set_lat_ptr(0);
		info->pdata->set_time_ptr(0);
	}

	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int mms_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	int i;

	/* TODO: turn off the power (set vdd_en to 0) to the touchscreen
	 * on suspend
	 */

	mutex_lock(&info->input_dev->mutex);
	if (!info->input_dev->users)
		goto out;

	mms_ts_disable(info);
	for (i = 0; i < MAX_FINGERS; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER,
					   false);
	}
	input_sync(info->input_dev);

out:
	mutex_unlock(&info->input_dev->mutex);
	return 0;
}

static int mms_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&info->input_dev->mutex);
	if (info->input_dev->users)
		ret = mms_ts_enable(info);
	mutex_unlock(&info->input_dev->mutex);

	return ret;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_suspend(&info->client->dev);
}

static void mms_ts_late_resume(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops mms_ts_pm_ops = {
	.suspend	= mms_ts_suspend,
	.resume		= mms_ts_resume,
};
#endif

static const struct i2c_device_id mms_ts_id[] = {
	{ MELFAS_TS_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mms_ts_id);

static struct i2c_driver mms_ts_driver = {
	.probe		= mms_ts_probe,
	.remove		= __devexit_p(mms_ts_remove),
	.driver = {
		.name = MELFAS_TS_NAME,
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &mms_ts_pm_ops,
#endif
	},
	.id_table	= mms_ts_id,
};

static int __init mms_ts_init(void)
{
	return i2c_add_driver(&mms_ts_driver);
}

static void __exit mms_ts_exit(void)
{
	i2c_del_driver(&mms_ts_driver);
}

module_init(mms_ts_init);
module_exit(mms_ts_exit);

/* Module information */
MODULE_DESCRIPTION("Touchscreen driver for Melfas MMS-series controllers");
MODULE_LICENSE("GPL");
