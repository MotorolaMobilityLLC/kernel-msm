/*
 * Platform data for Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MADERA_PDATA_H
#define _MADERA_PDATA_H

#include <linux/kernel.h>

#include <dt-bindings/mfd/madera.h>
#include <linux/regulator/madera-ldo1.h>
#include <linux/regulator/madera-micsupp.h>
#include <linux/irqchip/irq-madera-pdata.h>
#include <sound/madera-pdata.h>
#include <linux/extcon/extcon-madera-pdata.h>

#define MADERA_MAX_GPIO_REGS		80

#define CS47L35_NUM_GPIOS	16
#define CS47L85_NUM_GPIOS	40
#define CS47L90_NUM_GPIOS	38

#define MADERA_MAX_MICBIAS		4
#define MADERA_MAX_CHILD_MICBIAS	4

#define MADERA_MAX_GPSW			2

struct regulator_init_data;

struct madera_micbias {
	int mV;                    /** Regulated voltage */
	unsigned int ext_cap:1;    /** External capacitor fitted */
	unsigned int discharge[MADERA_MAX_CHILD_MICBIAS]; /** Actively discharge */
	unsigned int soft_start:1; /** Disable aggressive startup ramp rate */
	unsigned int bypass:1;     /** Use bypass mode */
};

struct madera_pdata {
	int reset;      /** GPIO controlling /RESET, if any */

	/** Substruct of pdata for the LDO1 regulator */
	struct madera_ldo1_pdata ldo1;

	/** Substruct of pdata for the MICSUPP regulator */
	struct madera_micsupp_pdata micsupp;

	/** Substruct of pdata for the irqchip driver */
	struct madera_irqchip_pdata irqchip;

	/** If a direct 32kHz clock is provided on an MCLK specify it here */
	unsigned int clk32k_src;

	/** Base GPIO */
	int gpio_base;

	/** Pin state for GPIO pins
	 * Defines default pin function and state for each GPIO. The values in
	 * here are written into the GPn_CONFIGx register block. There are
	 * two entries for each GPIO pin, for the GPn_CONFIG1 and GPn_CONFIG2
	 * registers.
	 *
	 * See the codec datasheet for a description of the contents of the
	 * GPn_CONFIGx registers.
	 *
	 * 0 = leave at chip default
	 * values 0x1..0xffff = set to this value
	 * 0xffffffff = set to 0
	 */
	unsigned int gpio_defaults[MADERA_MAX_GPIO_REGS];

	/** MICBIAS configurations */
	struct madera_micbias micbias[MADERA_MAX_MICBIAS];

	/** Substructure of pdata for the ASoC codec driver
	 * See include/sound/madera-pdata.h
	 */
	struct madera_codec_pdata codec;

	/** General purpose switch mode setting
	 * See the SW1_MODE field in the datasheet for the available values
	 */
	unsigned int gpsw[MADERA_MAX_GPSW];

	/** Accessory detection configurations */
	struct madera_accdet_pdata accdet[MADERA_MAX_ACCESSORY];
};

#endif
