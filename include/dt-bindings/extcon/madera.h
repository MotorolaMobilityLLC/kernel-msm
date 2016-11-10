/*
 * Device Tree defines for Madera codec extcon driver
 *
 * Copyright 2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DT_BINDINGS_EXTCON_MADERA_H
#define _DT_BINDINGS_EXTCON_MADERA_H

/* output clamp pin for cirrus,hpd-pins */
#define MADERA_HPD_OUT_OUT1L		0
#define MADERA_HPD_OUT_OUT1R		1
#define MADERA_HPD_OUT_OUT2L		2
#define MADERA_HPD_OUT_OUT2R		3

/* Sense pin selection for cirrus,hpd-pins */
#define MADERA_HPD_SENSE_MICDET1	0
#define MADERA_HPD_SENSE_MICDET2	1
#define MADERA_HPD_SENSE_MICDET3	2
#define MADERA_HPD_SENSE_MICDET4	3
#define MADERA_HPD_SENSE_HPDET1		4
#define MADERA_HPD_SENSE_HPDET2		5
#define MADERA_HPD_SENSE_JD1		6
#define MADERA_HPD_SENSE_JD2		7

/* source setting for cirrus,micd-configs (cs47l35, cs47l85) */
#define MADERA_ACCD_SENSE_MICDET1	0
#define MADERA_ACCD_SENSE_MICDET2	1

/* bias setting for  cirrus,micd-configs (cs47l35, cs47l85) */
#define MADERA_ACCD_BIAS_SRC_MICBIAS1	1
#define MADERA_ACCD_BIAS_SRC_MICBIAS2	2

/* source setting for cirrus,micd-configs */
#define MADERA_MICD1_SENSE_MICDET1	0
#define MADERA_MICD1_SENSE_MICDET2	1
#define MADERA_MICD1_SENSE_MICDET3	2
#define MADERA_MICD1_SENSE_MICDET4	3

/* ground setting for cirrus,micd-configs */
#define MADERA_MICD1_GND_MICDET1	0
#define MADERA_MICD1_GND_MICDET2	1
#define MADERA_MICD1_GND_MICDET3	2
#define MADERA_MICD1_GND_MICDET4	3

/* bias setting for cirrus,micd-configs */
#define MADERA_MICD_BIAS_SRC_MICBIAS1A	0x0
#define MADERA_MICD_BIAS_SRC_MICBIAS1B	0x1
#define MADERA_MICD_BIAS_SRC_MICBIAS1C	0x2
#define MADERA_MICD_BIAS_SRC_MICBIAS1D	0x3
#define MADERA_MICD_BIAS_SRC_MICBIAS2A	0x4
#define MADERA_MICD_BIAS_SRC_MICBIAS2B	0x5
#define MADERA_MICD_BIAS_SRC_MICBIAS2C	0x6
#define MADERA_MICD_BIAS_SRC_MICBIAS2D	0x7
#define MADERA_MICD_BIAS_SRC_MICVDD	0xF

/* HP gnd setting for cirrus,micd-configs */
#define MADERA_HPD_GND_HPOUTFB1		0
#define MADERA_HPD_GND_HPOUTFB2		1
#define MADERA_HPD_GND_HPOUTFB3		2
#define MADERA_HPD_GND_HPOUTFB4		3

#endif
