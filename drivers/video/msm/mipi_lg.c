/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_lg.h"

#include "../board-8064.h"
#include <asm/mach-types.h>
#include <linux/pwm.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/gpio.h>

#define PWM_FREQ_HZ 300
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_LEVEL 255
#define PWM_DUTY_LEVEL \
	(PWM_PERIOD_USEC / PWM_LEVEL)

#define gpio_EN_VDD_BL PM8921_GPIO_PM_TO_SYS(23)

static struct mipi_dsi_panel_platform_data *mipi_lg_pdata;
static struct pwm_device *bl_lpm;

static struct dsi_buf lg_tx_buf;
static struct dsi_buf lg_rx_buf;
static int mipi_lg_lcd_init(void);

static char enter_sleep[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */
static char exit_sleep[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
static char display_off[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */
static char display_on[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

static struct dsi_cmd_desc lg_display_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_on), display_on}
};

static struct dsi_cmd_desc lg_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
};

struct dcs_cmd_req cmdreq_lg;

static int mipi_lg_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int ret;

	printk("%s+\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	msleep(200);

	printk("%s, lg display on command+\n", __func__);
	cmdreq_lg.cmds = lg_display_on_cmds;
	cmdreq_lg.cmds_cnt = ARRAY_SIZE(lg_display_on_cmds);
	cmdreq_lg.flags = CMD_REQ_COMMIT;
	cmdreq_lg.rlen = 0;
	cmdreq_lg.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq_lg);
	printk("%s, lg display on command-\n", __func__);

	mdelay(210);

	if (bl_lpm) {
		ret = pwm_config(bl_lpm, PWM_DUTY_LEVEL * 40,
			PWM_PERIOD_USEC);
		if (ret) {
			pr_err("pwm_config on lpm failed %d\n", ret);
		}
		ret = pwm_enable(bl_lpm);
		if (ret)
			pr_err("pwm enable/disable on lpm failed"
				"for bl 255\n");
	}

	printk("%s-\n", __func__);
	return 0;
}

static int mipi_lg_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	int ret;

	printk("%s+\n", __func__);

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (bl_lpm) {
		ret = pwm_config(bl_lpm, 0, PWM_PERIOD_USEC);
		if (ret) {
			pr_err("pwm_config on lpm failed %d\n", ret);
		}
			pwm_disable(bl_lpm);
	}

	mdelay(210);

	printk("%s, lg display off command+\n", __func__);
	cmdreq_lg.cmds = lg_display_off_cmds;
	cmdreq_lg.cmds_cnt = ARRAY_SIZE(lg_display_off_cmds);
	cmdreq_lg.flags = CMD_REQ_COMMIT;
	cmdreq_lg.rlen = 0;
	cmdreq_lg.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq_lg);
	printk("%s, lg display off command-\n", __func__);

	mdelay(20);

	printk("%s-\n", __func__);
	return 0;
}

static void mipi_lg_set_backlight(struct msm_fb_data_type *mfd)
{
	int ret;

	pr_debug("%s: back light level %d\n", __func__, mfd->bl_level);

	if (bl_lpm) {
		ret = pwm_config(bl_lpm, PWM_DUTY_LEVEL *
			mfd->bl_level, PWM_PERIOD_USEC);
		if (ret) {
			pr_err("pwm_config on lpm failed %d\n", ret);
			return;
		}
		if (mfd->bl_level) {
			ret = pwm_enable(bl_lpm);
			if (ret)
				pr_err("pwm enable/disable on lpm failed"
				"for bl %d\n",	mfd->bl_level);
		} else {
			pwm_disable(bl_lpm);
		}
	}
}

static int __devinit mipi_lg_lcd_probe(struct platform_device *pdev)
{
	printk("%s+\n", __func__);

	if (pdev->id == 0) {
		mipi_lg_pdata = pdev->dev.platform_data;
		return 0;
	}

	if (mipi_lg_pdata == NULL) {
		pr_err("%s.invalid platform data.\n", __func__);
		return -ENODEV;
	} else {
		bl_lpm = pwm_request(mipi_lg_pdata->gpio[0],
			"backlight");
	}

	if (bl_lpm == NULL || IS_ERR(bl_lpm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_lpm = NULL;
	}
	pr_debug("bl_lpm = %p lpm = %d\n", bl_lpm,
		mipi_lg_pdata->gpio[0]);

	msm_fb_add_device(pdev);

	printk("%s-\n", __func__);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_lg_lcd_probe,
	.driver = {
		.name   = "mipi_lg",
	},
};

static struct msm_fb_panel_data lg_panel_data = {
	.on		= mipi_lg_lcd_on,
	.off		= mipi_lg_lcd_off,
	.set_backlight = mipi_lg_set_backlight,
};

static int ch_used[3];

int mipi_lg_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_lg_lcd_init();
	if (ret) {
		pr_err("mipi_lg_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_lg", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	lg_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &lg_panel_data,
		sizeof(lg_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int mipi_lg_lcd_init(void)
{
	mipi_dsi_buf_alloc(&lg_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&lg_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
