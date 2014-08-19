/*
 * board-blackbay.c: Intel Medfield based board (Blackbay)
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/sfi.h>
#include <linux/intel_pmic_gpio.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mfd/intel_msic.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/i2c-gpio.h>

#include <asm/setup.h>
#include <asm/mpspec_def.h>
#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_vrtc.h>
#include <linux/io.h>
#include <asm/i8259.h>
#include <asm/intel_scu_ipc.h>
#include <asm/apb_timer.h>
#include <linux/reboot.h>

/*
 * IPC devices
 */
#include "device_libs/platform_ipc.h"
#include "device_libs/platform_pmic_gpio.h"
#include "device_libs/platform_msic.h"
#include "device_libs/platform_msic_gpio.h"
#include "device_libs/platform_msic_audio.h"
#include "device_libs/platform_msic_power_btn.h"
#include "device_libs/platform_msic_ocd.h"
#include "device_libs/platform_mrfl_pmic.h"
#include "device_libs/platform_mrfl_pmic_i2c.h"
#include "device_libs/platform_mrfl_ocd.h"
#include "device_libs/platform_soc_thermal.h"
#include "device_libs/platform_msic_adc.h"
#include "device_libs/platform_bcove_adc.h"
#include <asm/platform_mrfld_audio.h>
#include "device_libs/platform_moor_thermal.h"
#include "device_libs/platform_scu_log.h"

/*
 * SPI devices
 */
#include "device_libs/platform_max3111.h"

/*
 * I2C devices
 */
#include "device_libs/platform_max7315.h"
#include "device_libs/platform_tca6416.h"
#include "device_libs/platform_tc35876x.h"
#include "device_libs/platform_rmi4.h"
#include "device_libs/platform_bq24192.h"
#include "device_libs/platform_bq24261.h"
#include "device_libs/platform_r69001.h"
#include "device_libs/platform_pn544.h"
#include "device_libs/platform_pca9574.h"

/* SW devices */
#include "device_libs/platform_panel.h"

#include "device_libs/platform_wm8994.h"

/*
 * SPI devices
 */
#include "device_libs/platform_max17042.h"

/* HSI devices */

/* SW devices */

/* WIFI devices */
#include "device_libs/platform_wifi.h"

/* USB devices */
#include "device_libs/pci/platform_usb_otg.h"

static void __init *no_platform_data(void *info)
{
	return NULL;
}

struct devs_id __initconst device_ids[] = {
	/* SD devices */
	{"bcm43xx_clk_vmmc", SFI_DEV_TYPE_SD, 0, &wifi_platform_data, NULL},
	{"bcm43xx_vmmc", SFI_DEV_TYPE_SD, 0, &wifi_platform_data, NULL},
	{"iwlwifi_clk_vmmc", SFI_DEV_TYPE_SD, 0, &wifi_platform_data, NULL},
	{"WLAN_FAST_IRQ", SFI_DEV_TYPE_SD, 0, &no_platform_data,
	 &wifi_platform_data_fastirq},

	/* SPI devices */
	{"bma023", SFI_DEV_TYPE_I2C, 1, &no_platform_data, NULL},
	{"pmic_gpio", SFI_DEV_TYPE_SPI, 1, &pmic_gpio_platform_data, NULL},
	{"pmic_gpio", SFI_DEV_TYPE_IPC, 1, &pmic_gpio_platform_data,
					&ipc_device_handler},
	{"spi_max3111", SFI_DEV_TYPE_SPI, 0, &max3111_platform_data, NULL},
	{"i2c_max7315", SFI_DEV_TYPE_I2C, 1, &max7315_platform_data, NULL},
	{"i2c_max7315_2", SFI_DEV_TYPE_I2C, 1, &max7315_platform_data, NULL},
	{"tca6416", SFI_DEV_TYPE_I2C, 1, &tca6416_platform_data, NULL},
	{"pmic_audio", SFI_DEV_TYPE_IPC, 1, &no_platform_data,
					&ipc_device_handler},
	{"i2c_disp_brig", SFI_DEV_TYPE_I2C, 0, &tc35876x_platform_data, NULL},
	{"r69001-ts-i2c", SFI_DEV_TYPE_I2C, 0, &r69001_platform_data, NULL},
	{"synaptics_3202", SFI_DEV_TYPE_I2C, 0, &rmi4_platform_data, NULL},
	{"syn_3400_cgs", SFI_DEV_TYPE_I2C, 0, &rmi4_platform_data, NULL},
	{"syn_3400_igzo", SFI_DEV_TYPE_I2C, 0, &rmi4_platform_data, NULL},
	{"synaptics_3402", SFI_DEV_TYPE_I2C, 0, &rmi4_platform_data, NULL},

	/* I2C devices*/
	{"max17042", SFI_DEV_TYPE_I2C, 1, &max17042_platform_data, NULL},
	{"max17047", SFI_DEV_TYPE_I2C, 1, &max17042_platform_data, NULL},
	{"max17050", SFI_DEV_TYPE_I2C, 1, &max17042_platform_data, NULL},
	{"bq24192", SFI_DEV_TYPE_I2C, 1, &bq24192_platform_data},
	{"bq24261_charger", SFI_DEV_TYPE_I2C, 1, &bq24261_platform_data, NULL},
	{"pn544", SFI_DEV_TYPE_I2C, 0, &pn544_platform_data, NULL},
	{"MNZX8000", SFI_DEV_TYPE_I2C, 0, &no_platform_data, NULL},
	{"pca953x", SFI_DEV_TYPE_I2C, 0, &nxp_pca9574_platform_data, NULL},
	{"it8566_hdmi_cec", SFI_DEV_TYPE_I2C, 1, &no_platform_data, NULL},
	{"it8566_flash_mod", SFI_DEV_TYPE_I2C, 1, &no_platform_data, NULL},

	/* MSIC subdevices */
	{"msic_adc", SFI_DEV_TYPE_IPC, 1, &msic_adc_platform_data,
						&ipc_device_handler},
	{"bcove_power_btn", SFI_DEV_TYPE_IPC, 1, &msic_power_btn_platform_data,
					&ipc_device_handler},
	{"scove_power_btn", SFI_DEV_TYPE_IPC, 1,
					&msic_power_btn_platform_data,
					&ipc_device_handler},
	{"msic_gpio", SFI_DEV_TYPE_IPC, 1, &msic_gpio_platform_data,
					&ipc_device_handler},
	{"msic_audio", SFI_DEV_TYPE_IPC, 1, &msic_audio_platform_data,
					&ipc_device_handler},
	{"msic_power_btn", SFI_DEV_TYPE_IPC, 1, &msic_power_btn_platform_data,
					&ipc_device_handler},
	{"msic_ocd", SFI_DEV_TYPE_IPC, 1, &msic_ocd_platform_data,
					&ipc_device_handler},
	{"bcove_bcu", SFI_DEV_TYPE_IPC, 1, &mrfl_ocd_platform_data,
					&ipc_device_handler},
	{"bcove_adc", SFI_DEV_TYPE_IPC, 1, &bcove_adc_platform_data,
					&ipc_device_handler},
	{"scove_thrm", SFI_DEV_TYPE_IPC, 1, &moor_thermal_platform_data,
					&ipc_device_handler},
	{"scuLog", SFI_DEV_TYPE_IPC, 1, &scu_log_platform_data,
					&ipc_device_handler},

	/* Panel */
	{"PANEL_CMI_CMD", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PANEL_JDI_VID", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PANEL_JDI_CMD", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	/*  above 3 items will be removed
	* after firmware changing
	*/
	{"PNC_CMI_7x12", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNV_JDI_7x12", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNC_JDI_7x12", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNC_SHARP_10x19", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNCD_SHARP_10x19", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNV_SHARP_25x16", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNC_SHARP_25x16", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNV_JDI_25x16", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNC_JDI_25x16", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNC_SDC_16x25", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},
	{"PNC_SDC_25x16", SFI_DEV_TYPE_MDM, 0, &no_platform_data,
		&panel_handler},

	/* IPC devices */
	{"pmic_charger", SFI_DEV_TYPE_IPC, 1, &no_platform_data, NULL},
	{"pmic_ccsm", SFI_DEV_TYPE_IPC, 1, &mrfl_pmic_ccsm_platform_data,
						&ipc_device_handler},
	{"i2c_pmic_adap", SFI_DEV_TYPE_IPC, 1, &mrfl_pmic_i2c_platform_data,
						&ipc_device_handler},

	/* IPC devices */
	{"mrfld_lm49453", SFI_DEV_TYPE_IPC, 1, &merfld_audio_platform_data,
						&ipc_device_handler},
	{"mrfld_wm8958", SFI_DEV_TYPE_IPC, 1, &merfld_wm8958_audio_platform_data,
						&ipc_device_handler},
	{"soc_thrm", SFI_DEV_TYPE_IPC, 1, &no_platform_data,
						&soc_thrm_device_handler},
	{"wm8958", SFI_DEV_TYPE_I2C, 0, &wm8994_platform_data, NULL},
	{"lm49453_codec", SFI_DEV_TYPE_I2C, 1, &no_platform_data, NULL},
	/* USB */
	{"ULPICAL_7F", SFI_DEV_TYPE_USB, 0, &no_platform_data,
		&sfi_handle_usb},
	{"ULPICAL_7D", SFI_DEV_TYPE_USB, 0, &no_platform_data,
		&sfi_handle_usb},
	{"UTMICAL_PEDE3TX0", SFI_DEV_TYPE_USB, 0, &no_platform_data,
		&sfi_handle_usb},
	{"UTMICAL_PEDE6TX7", SFI_DEV_TYPE_USB, 0, &no_platform_data,
		&sfi_handle_usb},
	{},
};
