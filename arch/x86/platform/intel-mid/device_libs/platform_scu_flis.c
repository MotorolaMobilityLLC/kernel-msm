/*
 * platform_scu_flis.c: scu_flis platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Ning Li <ning.li@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_remoteproc.h>
#include <asm/intel_scu_flis.h>
#include "platform_scu_flis.h"

static struct pin_mmio_flis_t ann_pin_mmio_flis_table[ANN_PIN_TABLE_SIZE] = {
	/* gpioclk */
	[ann_gp_clkph_0] = { writable, 0x2D00 },
	[ann_gp_clkph_1] = { writable, 0x2D04 },
	[ann_gp_clkph_2] = { writable, 0x2D08 },
	[ann_gp_clkph_3] = { writable, 0x2D0C },
	[ann_gp_clkph_4] = { writable, 0x2D10 },
	[ann_gp_clkph_5] = { writable, 0x2D14 },
	[ann_gp_clkph_6] = { writable, 0x2D18 },
	[ann_osc_clk_ctrl_0] = { writable, 0x2D1C },
	[ann_osc_clk_ctrl_1] = { writable, 0x2D20 },
	[ann_osc_clk_out_0] = { writable, 0x2D24 },
	[ann_osc_clk_out_1] = { writable, 0x2D28 },
	[ann_osc_clk_out_2] = { writable, 0x2D2C },
	[ann_osc_clk_out_3] = { writable, 0x2D30 },
	[ann_osc_clk_out_4] = { writable, 0x2D34 },

	/* gpiocsb */
	[ann_gp_camerasb_0] = { writable, 0x2900 },
	[ann_gp_camerasb_1] = { writable, 0x2904 },
	[ann_gp_camerasb_2] = { writable, 0x2908 },
	[ann_gp_camerasb_3] = { writable, 0x290C },
	[ann_gp_camerasb_4] = { writable, 0x2910 },
	[ann_gp_camerasb_5] = { writable, 0x2914 },
	[ann_gp_camerasb_6] = { writable, 0x2918 },
	[ann_gp_camerasb_7] = { writable, 0x291C },
	[ann_gp_camerasb_8] = { writable, 0x2920 },
	[ann_gp_camerasb_9] = { writable, 0x2924 },
	[ann_gp_camerasb_10] = { writable, 0x2928 },
	[ann_gp_camerasb_11] = { writable, 0x292C },
	[ann_gp_hdmi_hpd] = { writable, 0x2930 },
	[ann_gp_intd_dsi_te1] = { writable, 0x2934 },
	[ann_gp_intd_dsi_te2] = { writable, 0x2938 },

	/* gpioemmc */
	[ann_emmc_0_clk] = { writable, 0x900 },
	[ann_emmc_0_cmd] = { writable, 0x904 },
	[ann_emmc_0_d_0] = { writable, 0x908 },
	[ann_emmc_0_d_1] = { writable, 0x90C },
	[ann_emmc_0_d_2] = { writable, 0x910 },
	[ann_emmc_0_d_3] = { writable, 0x914 },
	[ann_emmc_0_d_4] = { writable, 0x918 },
	[ann_emmc_0_d_5] = { writable, 0x91C },
	[ann_emmc_0_d_6] = { writable, 0x920 },
	[ann_emmc_0_d_7] = { writable, 0x924 },
	[ann_emmc_0_rst_b] = { writable, 0x928 },
	[ann_emmc_0_rclk] = { writable, 0x92C },

	/* gpiogpio */
	[ann_gp_12] = { writable, 0x2500 },
	[ann_gp_13] = { writable, 0x2504 },
	[ann_gp_14] = { writable, 0x2508 },
	[ann_gp_15] = { writable, 0x250C },
	[ann_gp_16] = { writable, 0x2510 },
	[ann_gp_17] = { writable, 0x2514 },
	[ann_gp_18] = { writable, 0x2518 },
	[ann_gp_19] = { writable, 0x251C },
	[ann_gp_20] = { writable, 0x2520 },
	[ann_gp_21] = { writable, 0x2524 },
	[ann_gp_22] = { writable, 0x2528 },
	[ann_gp_23] = { writable, 0x252C },
	[ann_gp_24] = { writable, 0x2530 },
	[ann_gp_25] = { writable, 0x2534 },
	[ann_gp_26] = { writable, 0x2538 },
	[ann_gp_27] = { writable, 0x253C },
	[ann_gp_28] = { writable, 0x2540 },
	[ann_gp_29] = { writable, 0x2544 },
	[ann_gp_30] = { writable, 0x2548 },
	[ann_gp_31] = { writable, 0x254C },

	/* gpiohsi */
	[ann_mhsi_acdata] = { writable, 0x1100 },
	[ann_mhsi_acflag] = { writable, 0x1104 },
	[ann_mhsi_acready] = { writable, 0x1108 },
	[ann_mhsi_acwake] = { writable, 0x110C },
	[ann_mhsi_cadata] = { writable, 0x1110 },
	[ann_mhsi_caflag] = { writable, 0x1114 },
	[ann_mhsi_caready] = { writable, 0x1118 },
	[ann_mhsi_cawake] = { writable, 0x111C },

	/* gpioi2c */
	[ann_i2c_0_scl] = { writable, 0x1D00 },
	[ann_i2c_0_sda] = { writable, 0x1D04 },
	[ann_gp_i2c_1_scl] = { writable, 0x1D08 },
	[ann_gp_i2c_1_sda] = { writable, 0x1D0C },
	[ann_gp_i2c_2_scl] = { writable, 0x1D10 },
	[ann_gp_i2c_2_sda] = { writable, 0x1D14 },
	[ann_gp_i2c_3_scl] = { writable, 0x1D18 },
	[ann_gp_i2c_3_sda] = { writable, 0x1D1C },
	[ann_gp_i2c_4_scl] = { writable, 0x1D20 },
	[ann_gp_i2c_4_sda] = { writable, 0x1D24 },
	[ann_gp_i2c_5_scl] = { writable, 0x1D28 },
	[ann_gp_i2c_5_sda] = { writable, 0x1D2C },
	[ann_gp_i2c_6_scl] = { writable, 0x1D30 },
	[ann_gp_i2c_6_sda] = { writable, 0x1D34 },
	[ann_gp_i2c_7_scl] = { writable, 0x1D38 },
	[ann_gp_i2c_7_sda] = { writable, 0x1D3C },
	[ann_i2c_8_scl] = { writable, 0x1D40 },
	[ann_i2c_8_sda] = { writable, 0x1D44 },
	[ann_i2c_9_scl] = { writable, 0x1D48 },
	[ann_i2c_9_sda] = { writable, 0x1D4C },

	/* gpiokbd */
	[ann_gp_kbd_dkin_0] = { writable, 0x3500 },
	[ann_gp_kbd_dkin_1] = { writable, 0x3504 },
	[ann_gp_kbd_dkin_2] = { writable, 0x3508 },
	[ann_gp_kbd_dkin_3] = { writable, 0x350C },
	[ann_gp_kbd_mkin_0] = { writable, 0x3510 },
	[ann_gp_kbd_mkin_1] = { writable, 0x3514 },
	[ann_gp_kbd_mkin_2] = { writable, 0x3518 },
	[ann_gp_kbd_mkin_3] = { writable, 0x351C },
	[ann_gp_kbd_mkin_4] = { writable, 0x3520 },
	[ann_gp_kbd_mkin_5] = { writable, 0x3524 },
	[ann_gp_kbd_mkin_6] = { writable, 0x3528 },
	[ann_gp_kbd_mkin_7] = { writable, 0x352C },
	[ann_gp_kbd_mkout_0] = { writable, 0x3530 },
	[ann_gp_kbd_mkout_1] = { writable, 0x3534 },
	[ann_gp_kbd_mkout_2] = { writable, 0x3538 },
	[ann_gp_kbd_mkout_3] = { writable, 0x353C },
	[ann_gp_kbd_mkout_4] = { writable, 0x3540 },
	[ann_gp_kbd_mkout_5] = { writable, 0x3544 },
	[ann_gp_kbd_mkout_6] = { writable, 0x3548 },
	[ann_gp_kbd_mkout_7] = { writable, 0x354C },

	/* gpiopmic */
	[ann_prochot_b] = { writable, 0x3100 },
	[ann_resetout_b] = { writable, 0x3104 },
	[ann_rtc_clk] = { writable, 0x3108 },
	[ann_standby] = { writable, 0x310C },
	[ann_svid_alert_b] = { writable, 0x3110 },
	[ann_svid_vclk] = { writable, 0x3114 },
	[ann_svid_vdio] = { writable, 0x3118 },
	[ann_thermtrip_b] = { writable, 0x311C },
	[ann_xxpmode] = { writable, 0x3120 },
	[ann_xxprdy] = { writable, 0x3124 },
	[ann_xxpreq_b] = { writable, 0x3128 },
	[ann_gp_fast_int_0] = { writable, 0x312C },
	[ann_gp_fast_int_1] = { writable, 0x3130 },
	[ann_gp_fast_int_2] = { writable, 0x3134 },
	[ann_gp_fast_int_3] = { writable, 0x3138 },

	/* gpiopti */
	[ann_gp_mpti_clk] = { writable, 0x3D00 },
	[ann_gp_mpti_data_0] = { writable, 0x3D04 },
	[ann_gp_mpti_data_1] = { writable, 0x3D08 },
	[ann_gp_mpti_data_2] = { writable, 0x3D0C },
	[ann_gp_mpti_data_3] = { writable, 0x3D10 },
	[ann_gp_0] = { writable, 0x3D14 },
	[ann_gp_1] = { writable, 0x3D18 },
	[ann_gp_2] = { writable, 0x3D1C },
	[ann_gp_3] = { writable, 0x3D20 },
	[ann_gp_4] = { writable, 0x3D24 },
	[ann_gp_5] = { writable, 0x3D28 },
	[ann_gp_6] = { writable, 0x3D2C },
	[ann_gp_7] = { writable, 0x3D30 },
	[ann_gp_8] = { writable, 0x3D34 },
	[ann_gp_9] = { writable, 0x3D38 },
	[ann_gp_10] = { writable, 0x3D3C },
	[ann_gp_11] = { writable, 0x3D40 },
	[ann_jtag_tckc] = { writable, 0x3D44 },
	[ann_jtag_tdic] = { writable, 0x3D48 },
	[ann_jtag_tdoc] = { writable, 0x3D4C },
	[ann_jtag_tmsc] = { writable, 0x3D50 },
	[ann_jtag_trst_b] = { writable, 0x3D54 },

	/* gpiosdio */
	[ann_gp_sdio_0_cd_b] = { writable, 0xD00 },
	[ann_gp_sdio_0_clk] = { writable, 0xD04 },
	[ann_gp_sdio_0_cmd] = { writable, 0xD08 },
	[ann_gp_sdio_0_dat_0] = { writable, 0xD0C },
	[ann_gp_sdio_0_dat_1] = { writable, 0xD10 },
	[ann_gp_sdio_0_dat_2] = { writable, 0xD14 },
	[ann_gp_sdio_0_dat_3] = { writable, 0xD18 },
	[ann_gp_sdio_0_lvl_clk_fb] = { writable, 0xD1C },
	[ann_gp_sdio_0_lvl_cmd_dir] = { writable, 0xD20 },
	[ann_gp_sdio_0_lvl_dat_dir] = { writable, 0xD24 },
	[ann_gp_sdio_0_lvl_sel] = { writable, 0xD28 },
	[ann_gp_sdio_0_powerdown_b] = { writable, 0xD2C },
	[ann_gp_sdio_0_wp] = { writable, 0xD30 },
	[ann_gp_sdio_1_clk] = { writable, 0xD34 },
	[ann_gp_sdio_1_cmd] = { writable, 0xD38 },
	[ann_gp_sdio_1_dat_0] = { writable, 0xD3C },
	[ann_gp_sdio_1_dat_1] = { writable, 0xD40 },
	[ann_gp_sdio_1_dat_2] = { writable, 0xD44 },
	[ann_gp_sdio_1_dat_3] = { writable, 0xD48 },
	[ann_gp_sdio_1_powerdown_b] = { writable, 0xD4C },

	/* gpiossp */
	[ann_gp_ssp_3_clk] = { writable, 0x1900 },
	[ann_gp_ssp_3_fs] = { writable, 0x1904 },
	[ann_gp_ssp_3_rxd] = { writable, 0x1908 },
	[ann_gp_ssp_3_txd] = { writable, 0x190C },
	[ann_gp_ssp_4_clk] = { writable, 0x1910 },
	[ann_gp_ssp_4_fs_0] = { writable, 0x1914 },
	[ann_gp_ssp_4_fs_1] = { writable, 0x1918 },
	[ann_gp_ssp_4_fs_2] = { writable, 0x191C },
	[ann_gp_ssp_4_fs_3] = { writable, 0x1920 },
	[ann_gp_ssp_4_rxd] = { writable, 0x1924 },
	[ann_gp_ssp_4_txd] = { writable, 0x1928 },
	[ann_gp_ssp_5_clk] = { writable, 0x192C },
	[ann_gp_ssp_5_fs_0] = { writable, 0x1930 },
	[ann_gp_ssp_5_fs_1] = { writable, 0x1934 },
	[ann_gp_ssp_5_fs_2] = { writable, 0x1938 },
	[ann_gp_ssp_5_fs_3] = { writable, 0x193C },
	[ann_gp_ssp_5_rxd] = { writable, 0x1940 },
	[ann_gp_ssp_5_txd] = { writable, 0x1944 },
	[ann_gp_ssp_6_clk] = { writable, 0x1948 },
	[ann_gp_ssp_6_fs] = { writable, 0x194C },
	[ann_gp_ssp_6_rxd] = { writable, 0x1950 },
	[ann_gp_ssp_6_txd] = { writable, 0x1954 },

	/* gpiosspa */
	[ann_gp_mslim_0_bclk] = { writable, 0x1500 },
	[ann_gp_mslim_0_bdat] = { writable, 0x1504 },
	[ann_gp_ssp_0_clk] = { writable, 0x1508 },
	[ann_gp_ssp_0_fs] = { writable, 0x150C },
	[ann_gp_ssp_0_rxd] = { writable, 0x1510 },
	[ann_gp_ssp_0_txd] = { writable, 0x1514 },
	[ann_gp_ssp_1_clk] = { writable, 0x1518 },
	[ann_gp_ssp_1_fs] = { writable, 0x151C },
	[ann_gp_ssp_1_rxd] = { writable, 0x1520 },
	[ann_gp_ssp_1_txd] = { writable, 0x1524 },
	[ann_gp_ssp_2_clk] = { writable, 0x1528 },
	[ann_gp_ssp_2_fs] = { writable, 0x152C },
	[ann_gp_ssp_2_rxd] = { writable, 0x1530 },
	[ann_gp_ssp_2_txd] = { writable, 0x1534 },

	/* gpiouart */
	[ann_gp_uart_0_cts] = { writable, 0x2100 },
	[ann_gp_uart_0_rts] = { writable, 0x2104 },
	[ann_gp_uart_0_rx] = { writable, 0x2108 },
	[ann_gp_uart_0_tx] = { writable, 0x210C },
	[ann_gp_uart_1_cts] = { writable, 0x2110 },
	[ann_gp_uart_1_rts] = { writable, 0x2114 },
	[ann_gp_uart_1_rx] = { writable, 0x2118 },
	[ann_gp_uart_1_tx] = { writable, 0x211C },
	[ann_gp_uart_2_cts] = { writable, 0x2120 },
	[ann_gp_uart_2_rts] = { writable, 0x2124 },
	[ann_gp_uart_2_rx] = { writable, 0x2128 },
	[ann_gp_uart_2_tx] = { writable, 0x212C },
	[ann_gp_32] = { writable, 0x2130 },
	[ann_gp_33] = { writable, 0x2134 },
	[ann_gp_34] = { writable, 0x2138 },
	[ann_gp_35] = { writable, 0x213C },
	[ann_gp_36] = { writable, 0x2140 },
	[ann_gp_37] = { writable, 0x2144 },
	[ann_gp_38] = { writable, 0x2148 },
	[ann_gp_39] = { writable, 0x214C },
	[ann_gp_40] = { writable, 0x2150 },
	[ann_gp_pwm_0] = { writable, 0x2154 },
	[ann_gp_pwm_1] = { writable, 0x2158 },

	/* gpioulpi */
	[ann_gp_ulpi_0_clk] = { writable, 0x500 },
	[ann_gp_ulpi_0_data_0] = { writable, 0x504 },
	[ann_gp_ulpi_0_data_1] = { writable, 0x508 },
	[ann_gp_ulpi_0_data_2] = { writable, 0x50C },
	[ann_gp_ulpi_0_data_3] = { writable, 0x510 },
	[ann_gp_ulpi_0_data_4] = { writable, 0x514 },
	[ann_gp_ulpi_0_data_5] = { writable, 0x518 },
	[ann_gp_ulpi_0_data_6] = { writable, 0x51C },
	[ann_gp_ulpi_0_data_7] = { writable, 0x520 },
	[ann_gp_ulpi_0_dir] = { writable, 0x524 },
	[ann_gp_ulpi_0_nxt] = { writable, 0x528 },
	[ann_gp_ulpi_0_refclk] = { writable, 0x52C },
	[ann_gp_ulpi_0_stp] = { writable, 0x530 },
};

static int __init intel_scu_flis_init(void)
{
	int ret;
	struct platform_device *pdev = NULL;
	static struct intel_scu_flis_platform_data flis_pdata;

	flis_pdata.pin_t = NULL;
	flis_pdata.pin_num = ANN_PIN_TABLE_SIZE;
	flis_pdata.flis_base = 0xFF0C0000;
	flis_pdata.flis_len = 0x8000;
	flis_pdata.mmio_flis_t = ann_pin_mmio_flis_table;

	pdev = platform_device_alloc(FLIS_DEVICE_NAME, -1);
	if (!pdev) {
		pr_err("out of memory for platform dev %s\n", FLIS_DEVICE_NAME);
		ret = -EINVAL;
		goto out;
	}

	pdev->dev.platform_data = &flis_pdata;

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add flis platform device\n");
		platform_device_put(pdev);
		goto out;
	}

	pr_info("intel_scu_flis platform device created\n");
out:
	return ret;
}
fs_initcall(intel_scu_flis_init);

