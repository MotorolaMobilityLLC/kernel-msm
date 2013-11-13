/*
 * Copyright (C) 2010 Melfas, Inc.
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

#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include "mms100a_ts.h"

enum {
	ISP_MODE_FLASH_ERASE	= 0x59F3,
	ISP_MODE_FLASH_WRITE	= 0x62CD,
	ISP_MODE_FLASH_READ	= 0x6AC9,
};

/* each address addresses 4-byte words */
#define ISP_MAX_FW_SIZE		(0x1F00 * 4)
#define ISP_IC_INFO_ADDR	0x1F00

#define ISP_CAL_INFO_ADDR	7936
#define ISP_CAL_DATA_SIZE	256

struct mms_info {
	struct i2c_client		*client;
	struct melfas_tsi_platform_data	*pdata;
};

extern int melfas_poweron(char on);

static struct gpiomux_setting gpio_i2c_config[] = {
	{
		.pull = GPIOMUX_PULL_DOWN,
		.drv = GPIOMUX_DRV_8MA,
		.func = GPIOMUX_FUNC_GPIO,
		.dir = GPIOMUX_OUT_LOW,
	},
	{
		.pull = GPIOMUX_PULL_NONE,
		.drv = GPIOMUX_DRV_2MA,
		.func = GPIOMUX_FUNC_3,
	},
	{
		.pull = GPIOMUX_PULL_NONE,
		.drv = GPIOMUX_DRV_2MA,
		.func = GPIOMUX_FUNC_GPIO,
	}
};

static struct gpiomux_setting gpio_tocuhint_config[] = {
	{
		.pull = GPIOMUX_PULL_UP,
		.drv = GPIOMUX_DRV_8MA,
		.func = GPIOMUX_FUNC_GPIO,
		.dir = GPIOMUX_IN,
	},
	{
		.pull = GPIOMUX_PULL_NONE,
		.drv = GPIOMUX_DRV_8MA,
		.func = GPIOMUX_FUNC_GPIO,
		.dir = GPIOMUX_IN,
	}
};

static struct msm_gpiomux_config melfas_gpio_configs[] = {
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_i2c_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config[0],
		}
	},
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_i2c_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config[0],
		}
	},
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_tocuhint_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_tocuhint_config[0],
		}
	},
};

static struct msm_gpiomux_config melfas_i2c_configs[] = {
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_i2c_config[1],
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config[1],
		}
	},
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_i2c_config[1],
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config[1],
		}
	},
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_tocuhint_config[0],
			[GPIOMUX_SUSPENDED] = &gpio_tocuhint_config[0],
		}
	},
};

static struct msm_gpiomux_config melfas_fx_configs[] = {
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_i2c_config[2],
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config[2],
		}
	},
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_i2c_config[2],
			[GPIOMUX_SUSPENDED] = &gpio_i2c_config[2],
		}
	},
	{
		.settings = {
			[GPIOMUX_ACTIVE]    = &gpio_tocuhint_config[1],
			[GPIOMUX_SUSPENDED] = &gpio_tocuhint_config[1],
		}
	},
};


int mms100_set_gpio_mux(char mode, struct melfas_tsi_platform_data *pdata)
{
	int ret;

	melfas_gpio_configs[0].gpio = pdata->i2c_sda_gpio;
	melfas_i2c_configs[0].gpio = pdata->i2c_sda_gpio;
	melfas_fx_configs[0].gpio = pdata->i2c_sda_gpio;
	melfas_gpio_configs[1].gpio = pdata->i2c_scl_gpio;
	melfas_i2c_configs[1].gpio = pdata->i2c_scl_gpio;
	melfas_fx_configs[1].gpio = pdata->i2c_scl_gpio;
	melfas_gpio_configs[2].gpio = pdata->i2c_int_gpio;
	melfas_i2c_configs[2].gpio = pdata->i2c_int_gpio;
	melfas_fx_configs[2].gpio = pdata->i2c_int_gpio;

	if (mode == 0) { /* gpio mode for isp */
		TOUCH_INFO_MSG("mms100_set_gpio_mux(GPIO) \n");
		msm_gpiomux_install(melfas_gpio_configs,
					ARRAY_SIZE(melfas_gpio_configs));
		msleep(30);

		ret = gpio_request(pdata->i2c_sda_gpio, "Melfas_I2C_SDA");
		if (ret) {
			gpio_free(pdata->i2c_sda_gpio);
			ret = gpio_request(pdata->i2c_sda_gpio,
							"Melfas_I2C_SDA");
			if (ret)
				TOUCH_INFO_MSG("gpio_request(Melfas_I2C_SDA) failed \n");
		}

		ret = gpio_request(pdata->i2c_scl_gpio, "Melfas_I2C_SCL");
		if (ret) {
			gpio_free(pdata->i2c_scl_gpio);
			ret = gpio_request(pdata->i2c_scl_gpio,
							"Melfas_I2C_SCL");
			if (ret)
				TOUCH_INFO_MSG("gpio_request(Melfas_I2C_SCL) failed \n");
		}

		ret = gpio_request(pdata->i2c_int_gpio, "Melfas_I2C_INT");
		if (ret) {
			gpio_free(pdata->i2c_int_gpio);
			ret = gpio_request(pdata->i2c_int_gpio,
							"Melfas_I2C_INT");
			if (ret)
				TOUCH_INFO_MSG("gpio_request(Melfas_I2C_INT) failed \n");
		}
	} else if (mode == 1) { /* i2c mode after isp update */
		TOUCH_INFO_MSG("mms100_set_gpio_mux(I2C) \n");
		msm_gpiomux_install(melfas_i2c_configs,
					ARRAY_SIZE(melfas_i2c_configs));
		gpio_free(pdata->i2c_sda_gpio);
		gpio_free(pdata->i2c_scl_gpio);
		gpio_free(pdata->i2c_int_gpio);
	} else if (mode == 2) {	/* i2c mode after fx mode */
		TOUCH_INFO_MSG("mms100_set_gpio_mux(I2C) \n");
		msm_gpiomux_install(melfas_i2c_configs,
					ARRAY_SIZE(melfas_i2c_configs));
	} else {
		TOUCH_INFO_MSG("mms100_set_gpio_mux(FX) \n");
		msm_gpiomux_install(melfas_fx_configs,
					ARRAY_SIZE(melfas_fx_configs));
	}

	return ret;
}

static void hw_reboot(struct mms_info *info, bool bootloader)
{
	melfas_poweron(0);
	gpio_direction_output(info->pdata->i2c_sda_gpio, bootloader ? 0 : 1);
	gpio_direction_output(info->pdata->i2c_scl_gpio, bootloader ? 0 : 1);
	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	msleep(30);
	melfas_poweron(1);

	if (bootloader) {
		gpio_set_value(info->pdata->i2c_scl_gpio, 0);
		gpio_set_value(info->pdata->i2c_sda_gpio, 1);
	} else {
		gpio_set_value(info->pdata->i2c_int_gpio, 1);
		gpio_direction_input(info->pdata->i2c_int_gpio);
		gpio_direction_input(info->pdata->i2c_scl_gpio);
		gpio_direction_input(info->pdata->i2c_sda_gpio);
	}
	msleep(40);
}

static void isp_toggle_clk(struct mms_info *info, int start_lvl, int end_lvl, int hold_us)
{
	gpio_set_value(info->pdata->i2c_scl_gpio, start_lvl);
	udelay(hold_us);
	gpio_set_value(info->pdata->i2c_scl_gpio, end_lvl);
	udelay(hold_us);
}

/* 1 <= cnt <= 32 bits to write */
static void isp_send_bits(struct mms_info *info, u32 data, int cnt)
{
	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	gpio_direction_output(info->pdata->i2c_scl_gpio, 0);
	gpio_direction_output(info->pdata->i2c_sda_gpio, 0);

	/* clock out the bits, msb first */
	while (cnt--) {
		gpio_set_value(info->pdata->i2c_sda_gpio, (data >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(info, 1, 0, 3);
	}
}

/* 1 <= cnt <= 32 bits to read */
static u32 isp_recv_bits(struct mms_info *info, int cnt)
{
	u32 data = 0;

	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	gpio_direction_output(info->pdata->i2c_scl_gpio, 0);
	gpio_set_value(info->pdata->i2c_sda_gpio, 0);
	gpio_direction_input(info->pdata->i2c_sda_gpio);

	/* clock in the bits, msb first */
	while (cnt--) {
		isp_toggle_clk(info, 0, 1, 1);
		data = (data << 1) | (!!gpio_get_value(info->pdata->i2c_sda_gpio));
	}

	gpio_direction_output(info->pdata->i2c_sda_gpio, 0);
	return data;
}

static void isp_enter_mode(struct mms_info *info, u32 mode)
{
	int cnt;
	unsigned long flags;

	local_irq_save(flags);
	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	gpio_direction_output(info->pdata->i2c_scl_gpio, 0);
	gpio_direction_output(info->pdata->i2c_sda_gpio, 1);

	mode &= 0xffff;
	for (cnt = 15; cnt >= 0; cnt--) {
		gpio_set_value(info->pdata->i2c_int_gpio, (mode >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(info, 1, 0, 3);
	}

	gpio_set_value(info->pdata->i2c_int_gpio, 0);
	local_irq_restore(flags);
}

static void isp_exit_mode(struct mms_info *info)
{
	int i;
	unsigned long flags;

	local_irq_save(flags);
	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	udelay(3);

	for (i = 0; i < 10; i++)
		isp_toggle_clk(info, 1, 0, 3);
	local_irq_restore(flags);
}

static void flash_set_address(struct mms_info *info, u16 addr)
{
	/* Only 13 bits of addr are valid.
	 * The addr is in bits 13:1 of cmd */
	isp_send_bits(info, (u32)(addr & 0x1fff) << 1, 18);
}

static void flash_erase(struct mms_info *info)
{
	isp_enter_mode(info, ISP_MODE_FLASH_ERASE);

	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	gpio_direction_output(info->pdata->i2c_scl_gpio, 0);
	gpio_direction_output(info->pdata->i2c_sda_gpio, 1);

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

	gpio_set_value(info->pdata->i2c_sda_gpio, 0);

	isp_exit_mode(info);
}

static u32 flash_readl(struct mms_info *info, u16 addr)
{
	int i;
	u32 val;
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(info, ISP_MODE_FLASH_READ);
	flash_set_address(info, addr);

	gpio_direction_output(info->pdata->i2c_scl_gpio, 0);
	gpio_direction_output(info->pdata->i2c_sda_gpio, 0);
	udelay(40);

	/* data load cycle */
	for (i = 0; i < 6; i++)
		isp_toggle_clk(info, 1, 0, 10);

	val = isp_recv_bits(info, 32);
	isp_exit_mode(info);
	local_irq_restore(flags);

	return val;
}

static void flash_writel(struct mms_info *info, u16 addr, u32 val)
{
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(info, ISP_MODE_FLASH_WRITE);
	flash_set_address(info, addr);
	isp_send_bits(info, val, 32);

	gpio_direction_output(info->pdata->i2c_sda_gpio, 1);
	/* 6 clock cycles with different timings for the data to get written
	 * into flash */
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 6);
	isp_toggle_clk(info, 0, 1, 12);
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 3);

	isp_toggle_clk(info, 1, 0, 1);

	gpio_direction_output(info->pdata->i2c_int_gpio, 0);
	isp_exit_mode(info);
	local_irq_restore(flags);
	usleep_range(300, 400);
}

static int fw_write_image(struct mms_info *info, const u8 *data, size_t len)
{
	u16 addr = 0;
	u32 val = 0;

	for (addr = 0; addr < (len / 4); addr++, data += 4) {
		val = get_unaligned_le32(data);

		flash_writel(info, addr, val);
	}

	return 0;
}

static int fw_download(struct mms_info *info, const u8 *data, size_t len)
{
	struct i2c_client *client = info->client;
	u32 val;
	int ret = 0;
	u32 *buf = kzalloc(ISP_CAL_DATA_SIZE * 4, GFP_KERNEL);
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	if (len % 4) {
		TOUCH_INFO_MSG("fw image size (%d) must be a multiple of 4 bytes\n", len);
		ret = -EINVAL;
		goto out;
	} else if (len > ISP_MAX_FW_SIZE) {
		TOUCH_INFO_MSG("fw image is too big, %d > %d\n", len, ISP_MAX_FW_SIZE);
		ret = -EINVAL;
		goto out;
	}

	TOUCH_INFO_MSG("fw download start\n");

	i2c_lock_adapter(adapter);
	mms100_set_gpio_mux(0, info->pdata);

	hw_reboot(info, true);

	val = flash_readl(info, ISP_IC_INFO_ADDR);
	TOUCH_INFO_MSG("IC info: 0x%02x (%x)\n", val & 0xff, val);

	TOUCH_INFO_MSG("fw erase...\n");
	flash_erase(info);

	TOUCH_INFO_MSG("fw write...\n");

	/* XXX: what does this do?! */
	flash_writel(info, ISP_IC_INFO_ADDR, 0xffffff00 | (val & 0xff));
	usleep_range(1000, 1500);

	ret = fw_write_image(info, data, len);
	if (ret)
		goto out;

	TOUCH_INFO_MSG("fw download done...\n");

	ret = 0;

out:
	if (ret != 0)
		TOUCH_INFO_MSG("fw download failed...\n");

	hw_reboot(info, false);
	kfree(buf);

	mms100_set_gpio_mux(1, info->pdata);
	i2c_unlock_adapter(adapter);

	return ret;
}

int mms_flash_fw_file_isp(struct i2c_client *client, struct melfas_tsi_platform_data *pdata, const struct firmware *fw)
{
	int ret;
	struct mms_info *info = kzalloc(sizeof(struct mms_info), GFP_KERNEL);
	const u8 *fw_data = fw->data + sizeof(struct mms_bin_hdr) +
						sizeof(struct mms_fw_img)*3;
	size_t fw_size = fw->size - sizeof(struct mms_bin_hdr) -
						sizeof(struct mms_fw_img)*3;

	info->client = client;
	info->pdata = pdata;

	TOUCH_INFO_MSG("Update external firmware (fw_size = %d) \n", fw_size);
	if (fw_size == 31744)
		ret = fw_download(info, fw_data, fw_size);
	else
		TOUCH_INFO_MSG("External firmware size error\n");

	kfree(info);

	return ret;
}
EXPORT_SYMBOL(mms_flash_fw_file_isp);

