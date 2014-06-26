/*
 * platform_bcove_adc.c: Platform data for Merrifield Basincove GPADC driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/types.h>
#include <asm/intel-mid.h>
#include <asm/intel_basincove_gpadc.h>
#include <asm/intel_mid_remoteproc.h>

#include "platform_bcove_adc.h"

/* SRAM address where the GPADC interrupt register is cached */
#define GPADC_SRAM_INTR_ADDR	0xfffff615

static struct gpadc_regmap_t basincove_gpadc_regmaps[BCOVE_GPADC_CH_NUM] = {
	{"VBAT",        5, 0xE9, 0xEA, },
	{"BATID",       4, 0xEB, 0xEC, },
	{"IBAT",        5, 0xED, 0xEE, },
	{"PMICTEMP",    3, 0xCC, 0xCD, },
	{"BATTEMP0",    2, 0xC8, 0xC9, },
	{"BATTEMP1",    2, 0xCA, 0xCB, },
	{"SYSTEMP0",    3, 0xC2, 0xC3, },
	{"SYSTEMP1",    3, 0xC4, 0xC5, },
	{"SYSTEMP2",    3, 0xC6, 0xC7, },
};

static struct gpadc_regmap_t shadycove_gpadc_regmaps[SCOVE_GPADC_CH_NUM] = {
	{"VBAT",        5, 0xE9, 0xEA, },
	{"BATID",       4, 0xEC, 0xED, },
	{"PMICTEMP",    3, 0xD5, 0xD6, },
	{"BATTEMP0",    2, 0xD1, 0xD2, },
	{"BATTEMP1",    2, 0xD3, 0xD4, },
	{"SYSTEMP0",    3, 0xCB, 0xCC, },
	{"SYSTEMP1",    3, 0xCD, 0xCE, },
	{"SYSTEMP2",    3, 0xCF, 0xD0, },
	{"USBID",       1, 0xEE, 0xEF, },
	{"PEAK",        7, 0xF7, 0xF8, },
	{"AGND",	6, 0xF0, 0xF1, },
	{"VREF",	6, 0xF0, 0xF1, },
};

static struct gpadc_regs_t basincove_gpadc_regs = {
	.gpadcreq		= 0xDC,
	.gpadcreq_irqen		= (1 << 1),
	.gpadcreq_busy		= (1 << 0),
	.mirqlvl1		= 0x0C,
	.mirqlvl1_adc		= (1 << 4),
	.adc1cntl		= 0xDD,
	.adcirq			= 0x06,
	.madcirq		= 0x11,
};

static struct gpadc_regs_t shadycove_gpadc_regs = {
	.gpadcreq		= 0xDC,
	.gpadcreq_irqen		= 0,
	.gpadcreq_busy		= (1 << 0),
	.mirqlvl1		= 0x0C,
	.mirqlvl1_adc		= (1 << 4),
	.adc1cntl		= 0xEB,
	.adcirq			= 0x06,
	.madcirq		= 0x11,
};

#define MSIC_ADC_MAP(_adc_channel_label,			\
		     _consumer_dev_name,                        \
		     _consumer_channel)                         \
	{                                                       \
		.adc_channel_label = _adc_channel_label,        \
		.consumer_dev_name = _consumer_dev_name,        \
		.consumer_channel = _consumer_channel,          \
	}

struct iio_map basincove_iio_maps[] = {
	MSIC_ADC_MAP("CH0", "VIBAT", "VBAT"),
	MSIC_ADC_MAP("CH1", "BATID", "BATID"),
	MSIC_ADC_MAP("CH2", "VIBAT", "IBAT"),
	MSIC_ADC_MAP("CH3", "PMICTEMP", "PMICTEMP"),
	MSIC_ADC_MAP("CH4", "BATTEMP", "BATTEMP0"),
	MSIC_ADC_MAP("CH5", "BATTEMP", "BATTEMP1"),
	MSIC_ADC_MAP("CH6", "SYSTEMP", "SYSTEMP0"),
	MSIC_ADC_MAP("CH7", "SYSTEMP", "SYSTEMP1"),
	MSIC_ADC_MAP("CH8", "SYSTEMP", "SYSTEMP2"),
	MSIC_ADC_MAP("CH6", "bcove_thrm", "SYSTEMP0"),
	MSIC_ADC_MAP("CH7", "bcove_thrm", "SYSTEMP1"),
	MSIC_ADC_MAP("CH8", "bcove_thrm", "SYSTEMP2"),
	MSIC_ADC_MAP("CH3", "bcove_thrm", "PMICTEMP"),
	{ },
};

struct iio_map shadycove_iio_maps[] = {
	MSIC_ADC_MAP("CH0", "VIBAT", "VBAT"),
	MSIC_ADC_MAP("CH1", "BATID", "BATID"),
	MSIC_ADC_MAP("CH2", "PMICTEMP", "PMICTEMP"),
	MSIC_ADC_MAP("CH3", "BATTEMP", "BATTEMP0"),
	MSIC_ADC_MAP("CH4", "BATTEMP", "BATTEMP1"),
	MSIC_ADC_MAP("CH5", "SYSTEMP", "SYSTEMP0"),
	MSIC_ADC_MAP("CH6", "SYSTEMP", "SYSTEMP1"),
	MSIC_ADC_MAP("CH7", "SYSTEMP", "SYSTEMP2"),
	MSIC_ADC_MAP("CH8", "USBID", "USBID"),
	MSIC_ADC_MAP("CH9", "PEAK", "PEAK"),
	MSIC_ADC_MAP("CH10", "GPMEAS", "AGND"),
	MSIC_ADC_MAP("CH11", "GPMEAS", "VREF"),
	MSIC_ADC_MAP("CH5", "scove_thrm", "SYSTEMP0"),
	MSIC_ADC_MAP("CH6", "scove_thrm", "SYSTEMP1"),
	MSIC_ADC_MAP("CH7", "scove_thrm", "SYSTEMP2"),
	MSIC_ADC_MAP("CH2", "scove_thrm", "PMICTEMP"),
	{ },
};

#define MSIC_ADC_CHANNEL(_type, _channel, _datasheet_name) \
	{                               \
		.indexed = 1,           \
		.type = _type,          \
		.channel = _channel,    \
		.datasheet_name = _datasheet_name,      \
	}

static const struct iio_chan_spec const basincove_adc_channels[] = {
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 0, "CH0"),
	MSIC_ADC_CHANNEL(IIO_RESISTANCE, 1, "CH1"),
	MSIC_ADC_CHANNEL(IIO_CURRENT, 2, "CH2"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 3, "CH3"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 4, "CH4"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 5, "CH5"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 6, "CH6"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 7, "CH7"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 8, "CH8"),
};

static const struct iio_chan_spec const shadycove_adc_channels[] = {
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 0, "CH0"),
	MSIC_ADC_CHANNEL(IIO_RESISTANCE, 1, "CH1"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 2, "CH2"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 3, "CH3"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 4, "CH4"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 5, "CH5"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 6, "CH6"),
	MSIC_ADC_CHANNEL(IIO_TEMP, 7, "CH7"),
	MSIC_ADC_CHANNEL(IIO_RESISTANCE, 8, "CH8"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 9, "CH9"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 10, "CH10"),
	MSIC_ADC_CHANNEL(IIO_VOLTAGE, 11, "CH11"),
};

static struct intel_basincove_gpadc_platform_data bcove_adc_pdata;

void __init *bcove_adc_platform_data(void *info)
{
	struct platform_device *pdev = NULL;
	struct sfi_device_table_entry *entry = info;
	int ret;

	pdev = platform_device_alloc(BCOVE_ADC_DEV_NAME, -1);

	if (!pdev) {
		pr_err("out of memory for SFI platform dev %s\n",
					BCOVE_ADC_DEV_NAME);
		goto out;
	}

	if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
		INTEL_MID_BOARD(1, TABLET, MRFL)) {
		bcove_adc_pdata.channel_num = BCOVE_GPADC_CH_NUM;
		bcove_adc_pdata.intr = GPADC_SRAM_INTR_ADDR;
		bcove_adc_pdata.intr_mask = MBATTEMP | MSYSTEMP | MBATT
			| MVIBATT | MCCTICK;
		bcove_adc_pdata.gpadc_iio_maps = basincove_iio_maps;
		bcove_adc_pdata.gpadc_regmaps = basincove_gpadc_regmaps;
		bcove_adc_pdata.gpadc_regs = &basincove_gpadc_regs;
		bcove_adc_pdata.gpadc_channels = basincove_adc_channels;
	} else if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
		INTEL_MID_BOARD(1, TABLET, MOFD)) {
		bcove_adc_pdata.channel_num = SCOVE_GPADC_CH_NUM;
		bcove_adc_pdata.intr = GPADC_SRAM_INTR_ADDR;
		bcove_adc_pdata.intr_mask = MUSBID | MPEAK | MBATTEMP
			| MSYSTEMP | MBATT | MVIBATT | MGPMEAS | MCCTICK;
		bcove_adc_pdata.gpadc_iio_maps = shadycove_iio_maps;
		bcove_adc_pdata.gpadc_regmaps = shadycove_gpadc_regmaps;
		bcove_adc_pdata.gpadc_regs = &shadycove_gpadc_regs;
		bcove_adc_pdata.gpadc_channels = shadycove_adc_channels;
	}

	pdev->dev.platform_data = &bcove_adc_pdata;

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add bcove adc platform device\n");
		platform_device_put(pdev);
		goto out;
	}

	install_irq_resource(pdev, entry->irq);

	register_rpmsg_service("rpmsg_bcove_adc", RPROC_SCU,
				RP_BCOVE_ADC);
out:
	return &bcove_adc_pdata;
}
