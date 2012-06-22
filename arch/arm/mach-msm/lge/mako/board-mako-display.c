/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/ion.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-mako.h"

#include "../../../../drivers/video/msm/msm_fb.h"
#include "../../../../drivers/video/msm/msm_fb_def.h"
#include "../../../../drivers/video/msm/mipi_dsi.h"

#include <mach/board_lge.h>

#include <linux/i2c.h>
#include <linux/kernel.h>

#ifndef LGE_DSDR_SUPPORT
#define LGE_DSDR_SUPPORT
#endif

#ifdef CONFIG_LGE_KCAL
#ifdef CONFIG_LGE_QC_LCDC_LUT
extern int set_qlut_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_qlut_display(void);
#else
#error only kcal by Qucalcomm LUT is supported now!!!
#endif
#endif

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
/* prim = 1366 x 768 x 3(bpp) x 3(pages) */
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(768 * 1280 * 4 * 3, 0x10000)
#else
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 3, 0x10000)
#endif
#else
/* prim = 1366 x 768 x 3(bpp) x 2(pages) */
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(768 * 1280 * 4 * 2, 0x10000)
#else
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 2, 0x10000)
#endif
#endif /*CONFIG_FB_MSM_TRIPLE_BUFFER */

#ifdef LGE_DSDR_SUPPORT
#define MSM_FB_EXT_BUF_SIZE \
        (roundup((1920 * 1088 * 4), 4096) * 3) /* 4 bpp x 3 page */
#else  /* LGE_DSDR_SUPPORT */
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((1920 * 1088 * 2), 4096) * 1) /* 2 bpp x 1 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((720 * 576 * 2), 4096) * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUF_SIZE	0
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#endif /* LGE_DSDR_SUPPORT */

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
#define MSM_FB_WFD_BUF_SIZE \
		(roundup((1280 * 736 * 2), 4096) * 3) /* 2 bpp x 3 page */
#else
#define MSM_FB_WFD_BUF_SIZE     0
#endif

#define MSM_FB_SIZE \
	roundup(MSM_FB_PRIM_BUF_SIZE + \
		MSM_FB_EXT_BUF_SIZE + MSM_FB_WFD_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
	#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((768 * 1280 * 3 * 2), 4096)
	#else
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
	#endif
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
#define LGIT_IEF
#endif
static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#define LVDS_CHIMEI_PANEL_NAME "lvds_chimei_wxga"
#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME "mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME "mipi_video_chimei_wxga"
#define HDMI_PANEL_NAME "hdmi_msm"
#define TVOUT_PANEL_NAME "tvout_msm"

#ifndef CONFIG_MACH_LGE
#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static unsigned char hdmi_is_primary = 1;
#else
static unsigned char hdmi_is_primary;
#endif

unsigned char apq8064_hdmi_as_primary_selected(void)
{
	return hdmi_is_primary;
}

static void set_mdp_clocks_for_wuxga(void);
#endif

static int msm_fb_detect_panel(const char *name)
{
	return 0;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name              = "msm_fb",
	.id                = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

void __init apq8064_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));
}

#define MDP_VSYNC_GPIO 0

static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 230400000 * 2,
		.ib = 288000000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 334080000 * 2,
		.ib = 417600000 * 2,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

static int mdp_core_clk_rate_table[] = {
	85330000,
	128000000,
	160000000,
	200000000,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_core_clk_rate = 85330000,
	.mdp_core_clk_table = mdp_core_clk_rate_table,
	.num_mdp_clk = ARRAY_SIZE(mdp_core_clk_rate_table),
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
	.mdp_rev = MDP_REV_44,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
	/* for early backlight on for APQ8064 */
	.cont_splash_enabled = 0x01,
};

void __init apq8064_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}

#ifdef CONFIG_LGE_KCAL
extern int set_kcal_values(int kcal_r, int kcal_g, int kcal_b);
extern int refresh_kcal_display(void);
extern int get_kcal_values(int *kcal_r, int *kcal_g, int *kcal_b);

static struct kcal_platform_data kcal_pdata = {
	.set_values = set_kcal_values,
	.get_values = get_kcal_values,
	.refresh_display = refresh_kcal_display
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};
#endif

static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
};
#endif

/* HDMI related GPIOs */
#define HDMI_CEC_VAR_GPIO	69
#define HDMI_DDC_CLK_GPIO	70
#define HDMI_DDC_DATA_GPIO	71
#define HDMI_HPD_GPIO		72

static bool dsi_power_on = false;
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l2, *reg_lvs6;
	static int gpio42;
	int rc;

	struct pm_gpio gpio42_param = {
		.direction = PM_GPIO_DIR_OUT,
		.output_buffer = PM_GPIO_OUT_BUF_CMOS,
		.output_value = 0,
		.pull = PM_GPIO_PULL_NO,
		.vin_sel = 2,
		.out_strength = PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_PAIRED,
		.inv_int_pol = 0,
		.disable_pin = 0,
	};
	printk(KERN_INFO"%s: mipi lcd function started status = %d \n", __func__, on);

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

		gpio42 = PM8921_GPIO_PM_TO_SYS(42);

		rc = gpio_request(gpio42, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 42 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vci");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8921_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}
	
		reg_lvs6 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_iovcc");
		if (IS_ERR(reg_lvs6)) {
			pr_err("could not get 8921_lvs6, rc = %ld\n",
				 PTR_ERR(reg_lvs6));
			return -ENODEV;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l8, 3000000, 3000000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		dsi_power_on = true;
	}
	if (on) {

		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
      
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
		rc = regulator_enable(reg_l8);  // dsi_vci
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		
		udelay(100);

		rc = regulator_enable(reg_lvs6); // IOVCC
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		udelay(100);
#endif
		
		rc = regulator_enable(reg_l2);  // DSI
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		printk(KERN_INFO " %s : reset start.", __func__);
		/* LCD RESET HIGH */
		mdelay(2);
		gpio42_param.output_value = 1;
		rc = pm8xxx_gpio_config(gpio42,&gpio42_param);	
		if (rc) {
			pr_err("gpio_config 42 failed (3), rc=%d\n", rc);
			return -EINVAL;
		}
		mdelay(11);

	} else {
		/* LCD RESET LOW */
		gpio42_param.output_value = 0;
		rc = pm8xxx_gpio_config(gpio42,&gpio42_param);
		if (rc) {
			pr_err("gpio_config 42 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		udelay(100);

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
		rc = regulator_disable(reg_lvs6); // IOVCC
		if (rc) {
			pr_err("disable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		udelay(100);
			
		rc = regulator_disable(reg_l8);	//VCI
		if (rc) {
			pr_err("disable reg_l8  failed, rc=%d\n", rc);
			return -ENODEV;
		}
		udelay(100);
#endif
		rc = regulator_disable(reg_l2);	//DSI
		if (rc) {
			pr_err("disable reg_l2  failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	
	return 0;
}

static char mipi_dsi_splash_is_enabled(void)
{
	return mdp_pdata.cont_splash_enabled;
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.dsi_power_save = mipi_dsi_panel_power,
	.splash_is_enabled = mipi_dsi_splash_is_enabled,
};

static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
	},
};

static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
	.lcdc_power_save = hdmi_panel_power,
};

static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}

static int hdmi_enable_5v(int on)
{
	return 0;
}

static int hdmi_core_power(int on, int show)
{
	static struct regulator *reg_8921_lvs7;
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8921_lvs7) {
		reg_8921_lvs7 = regulator_get(&hdmi_msm_device.dev,
					      "hdmi_vdda");
		if (IS_ERR(reg_8921_lvs7)) {
			pr_err("could not get reg_8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_8921_lvs7));
			reg_8921_lvs7 = NULL;
			return -ENODEV;
		}
	}
	
	if (on) {
		rc = regulator_enable(reg_8921_lvs7);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vdda", rc);
			goto error1;
		}
	} else {
		rc = regulator_disable(reg_8921_lvs7);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error1:
	return rc;
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(HDMI_DDC_CLK_GPIO, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", HDMI_DDC_CLK_GPIO, rc);
			goto error1;
		}
		rc = gpio_request(HDMI_DDC_DATA_GPIO, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", HDMI_DDC_DATA_GPIO, rc);
			goto error2;
		}
		rc = gpio_request(HDMI_HPD_GPIO, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", HDMI_HPD_GPIO, rc);
			goto error3;
		}
		pr_debug("%s(on): success\n", __func__);

	} else {
		gpio_free(HDMI_DDC_CLK_GPIO);
		gpio_free(HDMI_DDC_DATA_GPIO);
		gpio_free(HDMI_HPD_GPIO);

		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error3:
	gpio_free(HDMI_DDC_DATA_GPIO);
error2:
	gpio_free(HDMI_DDC_CLK_GPIO);
error1:
	return rc;	
}

static int hdmi_cec_power(int on)
{
	return 0;
}

#if defined (CONFIG_BACKLIGHT_LM3530)
extern void lm3530_lcd_backlight_set_level( int level);
#elif defined (CONFIG_BACKLIGHT_LM3533)
extern void lm3533_lcd_backlight_set_level( int level);
#endif

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
static int mipi_lgit_backlight_level(int level, int max, int min)
{
#ifdef CONFIG_BACKLIGHT_LM3530
	lm3530_lcd_backlight_set_level(level);
#endif

	return 0;
}


/* for making one source of DSV feature. */
char lcd_mirror [2] = {0x36, 0x02};

// values of DSV setting START
static char panel_setting_1_for_DSV [6] = {0xB0, 0x43, 0xFF, 0x80, 0x00, 0x00};
static char panel_setting_2_for_DSV [3] = {0xB3, 0x0A, 0x9F};
//static char panel_setting_3_for_DSV [2] = {0xB4, 0x02}; //2 dot inversion

static char display_mode1_for_DSV [6] = {0xB5, 0x50, 0x20, 0x40, 0x00, 0x20};
static char display_mode2_for_DSV [8] = {0xB6, 0x00, 0x14, 0x0F, 0x16, 0x13, 0x05, 0x05};

static char p_gamma_r_setting_for_DSV[10] = {0xD0, 0x40, 0x14, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_r_setting_for_DSV[10] = {0xD1, 0x40, 0x14, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char p_gamma_g_setting_for_DSV[10] = {0xD2, 0x40, 0x14, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_g_setting_for_DSV[10] = {0xD3, 0x40, 0x14, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char p_gamma_b_setting_for_DSV[10] = {0xD4, 0x40, 0x14, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_b_setting_for_DSV[10] = {0xD5, 0x40, 0x14, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};

#if defined(LGIT_IEF)
static char ief_set0_for_DSV[2] = {0xE0, 0x07};
static char ief_set1_for_DSV[5] = {0xE1, 0x00, 0x00, 0x01, 0x01};
static char ief_set2_for_DSV[3] = {0xE2, 0x01, 0x0F};
static char ief_set3_for_DSV[6] = {0xE3, 0x00, 0x00, 0x42, 0x35, 0x00};
static char ief_set4_for_DSV[4] = {0xE4, 0x04, 0x01, 0x17};
static char ief_set5_for_DSV[4] = {0xE5, 0x03, 0x0F, 0x17};
static char ief_set6_for_DSV[4] = {0xE6, 0x07, 0x00, 0x15};
static char ief_set7_for_DSV[9] = {0xE7, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static char ief_set8_for_DSV[9] = {0xE8, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static char ief_set9_for_DSV[9] = {0xE9, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static char ief_setA_for_DSV[9] = {0xEA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static char ief_setB_for_DSV[9] = {0xEB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static char ief_setC_for_DSV[9] = {0xEC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

static char osc_setting_for_DSV[4] =     {0xC0, 0x00, 0x0A, 0x10};
static char power_setting3_for_DSV[13] = {0xC3, 0x00, 0x88, 0x03, 0x20, 0x00, 0x55, 0x4F, 0x33,0x02,0x38,0x38,0x00};
static char power_setting4_for_DSV[6] =  {0xC4, 0x22, 0x24, 0x13, 0x13, 0x3D};
static char power_setting5_for_DSV[4] =  {0xC5, 0x3B, 0x3B, 0x03};
static char power_setting6_for_DSV[2] =  {0x11,0x00};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static char cabc_set0[2] = {0x51, 0xFF};
static char cabc_set1[2] = {0x5E, 0x00}; // CABC MIN
//static char cabc_set1[2] = {0x5E, 0x00};   //CABC MAX
static char cabc_set2[2] = {0x53, 0x2C};
static char cabc_set3[2] = {0x55, 0x02};
static char cabc_set4[6] = {0xC8, 0x21, 0x21, 0x21, 0x33, 0x80};//A-CABC applied
//static char cabc_set4[6] = {0xC8, 0x04, 0x04, 0x04, 0x33, 0x9F};//CABC MIN
//static char cabc_set4[6] = {0xC8, 0x74, 0x74, 0x74, 0x33, 0x9F};//CABC MAX
//static char cabc_set4[6] = {0xC8, 0x74, 0x74, 0x74, 0x33, 0x80};//CABC MAX - Tuning
#endif

static char exit_sleep_power_control_1_for_DSV[2] =  {0xC2,0x02};
static char exit_sleep_power_control_2_for_DSV[2] =  {0xC2,0x06};
static char exit_sleep_power_control_3_for_DSV[2] =  {0xC2,0x0E};
static char exit_sleep_power_control_4_for_DSV[2] =  {0xC1,0x08};

static char display_on_for_DSV[2] =  {0x29,0x00};

static char display_off_for_DSV[2] = {0x28,0x00};

static char enter_sleep_for_DSV[2] = {0x10,0x00};

static char enter_sleep_power_control_1_for_DSV[2] = {0xC1,0x00};
static char enter_sleep_power_control_3_for_DSV[2] = {0xC2,0x01};
static char enter_sleep_power_control_2_for_DSV[2] = {0xC2,0x00};

static char deep_standby_for_DSV[2] = {0xC1,0x03};

/* initialize device */
static struct dsi_cmd_desc lgit_power_on_set_1_for_DSV[] = {
	// Display Initial Set
	
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(lcd_mirror ),lcd_mirror},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_1_for_DSV ),panel_setting_1_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_2_for_DSV ),panel_setting_2_for_DSV},
//	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_3_for_DSV ),panel_setting_3_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_mode1_for_DSV ),display_mode1_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_mode2_for_DSV ),display_mode2_for_DSV},

	// Gamma Set
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_r_setting_for_DSV),p_gamma_r_setting_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_r_setting_for_DSV),n_gamma_r_setting_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_g_setting_for_DSV),p_gamma_g_setting_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_g_setting_for_DSV),n_gamma_g_setting_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_b_setting_for_DSV),p_gamma_b_setting_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_b_setting_for_DSV),n_gamma_b_setting_for_DSV},

#if defined(LGIT_IEF)
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set0_for_DSV),ief_set0_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set1_for_DSV),ief_set1_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set2_for_DSV),ief_set2_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set3_for_DSV),ief_set3_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set4_for_DSV),ief_set4_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set5_for_DSV),ief_set5_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set6_for_DSV),ief_set6_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set7_for_DSV),ief_set7_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set8_for_DSV),ief_set8_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set9_for_DSV),ief_set9_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_setA_for_DSV),ief_setA_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_setB_for_DSV),ief_setB_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_setC_for_DSV),ief_setC_for_DSV},
#endif

	// Power Supply Set
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(osc_setting_for_DSV   ),osc_setting_for_DSV   }, 
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_setting3_for_DSV),power_setting3_for_DSV}, 
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_setting4_for_DSV),power_setting4_for_DSV},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_setting5_for_DSV),power_setting5_for_DSV},
	{DTYPE_DCS_WRITE, 1, 0, 0, 5, sizeof(power_setting6_for_DSV),power_setting6_for_DSV},
		
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set0),cabc_set0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set1),cabc_set1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set2),cabc_set2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set3),cabc_set3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set4),cabc_set4},
#endif
	
	{DTYPE_GEN_LWRITE,  1, 0, 0, 20, sizeof(exit_sleep_power_control_1_for_DSV	),exit_sleep_power_control_1_for_DSV	},
	{DTYPE_GEN_LWRITE,  1, 0, 0, 20, sizeof(exit_sleep_power_control_2_for_DSV	),exit_sleep_power_control_2_for_DSV	},
	{DTYPE_GEN_LWRITE,  1, 0, 0, 0, sizeof(exit_sleep_power_control_3_for_DSV	),exit_sleep_power_control_3_for_DSV	},
	
};

static struct dsi_cmd_desc lgit_power_on_set_2_for_DSV[] = {
	// Power Supply Set
	{DTYPE_GEN_LWRITE,  1, 0, 0, 20, sizeof(exit_sleep_power_control_4_for_DSV	),exit_sleep_power_control_4_for_DSV	},
	{DTYPE_DCS_WRITE,  1, 0, 0, 20, sizeof(display_on_for_DSV	),display_on_for_DSV	},
};

static struct dsi_cmd_desc lgit_power_off_set_1_for_DSV[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_off_for_DSV), display_off_for_DSV},
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(enter_sleep_for_DSV), enter_sleep_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 20, sizeof(enter_sleep_power_control_1_for_DSV), enter_sleep_power_control_1_for_DSV},
};

static struct dsi_cmd_desc lgit_power_off_set_2_for_DSV[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 20, sizeof(enter_sleep_power_control_3_for_DSV), enter_sleep_power_control_3_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 20, sizeof(enter_sleep_power_control_2_for_DSV), enter_sleep_power_control_2_for_DSV},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 20, sizeof(deep_standby_for_DSV), deep_standby_for_DSV}	
};
// values of DSV setting END

// values of normal setting START(not DSV)


static struct msm_panel_common_pdata mipi_lgit_pdata = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set_1 = lgit_power_on_set_1_for_DSV,
	.power_on_set_2 = lgit_power_on_set_2_for_DSV,
	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set_1_for_DSV),
	.power_on_set_size_2 =ARRAY_SIZE(lgit_power_on_set_2_for_DSV),
	.power_off_set_1 = lgit_power_off_set_1_for_DSV,
	.power_off_set_2 = lgit_power_off_set_2_for_DSV,
	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set_1_for_DSV),
	.power_off_set_size_2 =ARRAY_SIZE(lgit_power_off_set_2_for_DSV),

};

static struct platform_device mipi_dsi_lgit_panel_device = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata,
	}
};
#endif

static struct platform_device *mako_panel_devices[] __initdata = {
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
	&mipi_dsi_lgit_panel_device,
#endif
#ifdef CONFIG_LGE_KCAL
	&kcal_platrom_device,
#endif
};

void __init apq8064_init_fb(void)
{
	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

	platform_add_devices(mako_panel_devices,
			ARRAY_SIZE(mako_panel_devices));

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);

	platform_device_register(&hdmi_msm_device);
	msm_fb_register_device("dtv", &dtv_pdata);

}

#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_LIQUID (1 << 4)
#define I2C_J1V (1 << 5)

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define PWM_SIMPLE_EN 0xA0
#define PWM_BRIGHTNESS 0x20
#endif

struct backlight_platform_data {
   void (*platform_init)(void);
   int gpio;
   unsigned int mode;
   int max_current;
   int init_on_boot;
   int min_brightness;
   int max_brightness;
   int default_brightness;
   int factory_brightness;
};

#if defined (CONFIG_BACKLIGHT_LM3530)
static struct backlight_platform_data lm3530_data = {

	.gpio = PM8921_GPIO_PM_TO_SYS(24),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.max_current = 0x17 | PWM_BRIGHTNESS,
#else
	.max_current = 0x17,
#endif
	.min_brightness = 0x01,
	.max_brightness = 0x71,
	
};
#elif defined(CONFIG_BACKLIGHT_LM3533)
static struct backlight_platform_data lm3533_data = {
	.gpio = PM8921_GPIO_PM_TO_SYS(24),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.max_current = 0x17 | PWM_SIMPLE_EN,
#else
	.max_current = 0x17,
#endif
	.min_brightness = 0x05,
	.max_brightness = 0xFF,
	.default_brightness = 0x9C,
	.factory_brightness = 0x78,
};
#endif
static struct i2c_board_info msm_i2c_backlight_info[] = {
	{

#if defined(CONFIG_BACKLIGHT_LM3530)
		I2C_BOARD_INFO("lm3530", 0x38),
		.platform_data = &lm3530_data,
#elif defined(CONFIG_BACKLIGHT_LM3533)
		I2C_BOARD_INFO("lm3533", 0x36),
		.platform_data = &lm3533_data,
#endif
	}
};

static struct i2c_registry apq8064_i2c_backlight_device[] __initdata = {

	{
	    I2C_SURF | I2C_FFA | I2C_RUMI | I2C_SIM | I2C_LIQUID | I2C_J1V,
		APQ_8064_GSBI1_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
	},
};

void __init register_i2c_backlight_devices(void)
{
	u8 mach_mask = 0;
	int i;

	/* Build the matching 'supported_machs' bitmask */
	if (machine_is_apq8064_cdp())
		mach_mask = I2C_SURF;
	else if (machine_is_apq8064_mtp())
		mach_mask = I2C_FFA;
	else if (machine_is_apq8064_liquid())
		mach_mask = I2C_LIQUID;
	else if (machine_is_apq8064_rumi3())
		mach_mask = I2C_RUMI;
	else if (machine_is_apq8064_sim())
		mach_mask = I2C_SIM;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");	

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(apq8064_i2c_backlight_device); ++i) {
		if (apq8064_i2c_backlight_device[i].machs & mach_mask)
			i2c_register_board_info(apq8064_i2c_backlight_device[i].bus,
						apq8064_i2c_backlight_device[i].info,
						apq8064_i2c_backlight_device[i].len);
	}
}
