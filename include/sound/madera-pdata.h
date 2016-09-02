/*
 * Platform data for Madera codec driver
 *
 * Copyright 2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MADERA_CODEC_PDATA_H
#define _MADERA_CODEC_PDATA_H

#include <linux/kernel.h>
#include <dt-bindings/mfd/madera.h>

#define MADERA_MAX_INPUT		6
#define MADERA_MAX_MUXED_CHANNELS	4
#define MADERA_MAX_OUTPUT		6
#define MADERA_MAX_AIF			4
#define MADERA_MAX_PDM_SPK		2
#define MADERA_MAX_DSP			7

struct madera_codec_pdata {
	/**
	 * Maximum number of channels that clocks will be generated for,
	 * useful for systems where an I2S bus with multiple data
	 * lines is mastered.
	 */
	int max_channels_clocked[MADERA_MAX_AIF];

	/** Reference voltage for DMIC inputs */
	int dmic_ref[MADERA_MAX_INPUT];

	/** Clock Source for DMICs
	 * The meaning of the values here depends on the codec. See the
	 * datasheet for a list of valid values for the INn_DMIC_SUP fields
	 * [0] = IN1, [1]=IN2 [2]=IN3
	 */
	int dmic_clksrc[MADERA_MAX_INPUT];

	/** Mode of input structures
	 * One of the MADERA_INMODE_xxx values
	 * Two-dimensional array [input_number][channel number]
	 * Four slots per input in the order:
	 * [n][0]=INnAL [n][1]=INnAR [n][2]=INnBL [n][3]=INnBR
	 */
	int inmode[MADERA_MAX_INPUT][MADERA_MAX_MUXED_CHANNELS];

	/** Output mono mode control
	 * For each output set the value to TRUE to indicate that
	 * the output is mono
	 * [0]=OUT1, [1]=OUT2, ...
	 */
	bool out_mono[MADERA_MAX_OUTPUT];

	/** PDM mute control settings.
	 * See the PDM_SPKn_CTRL_1 register in the datasheet for the
	 * meaning of the values.
	 */
	unsigned int pdm_mute[MADERA_MAX_PDM_SPK];

	/** PDM speaker format
	 * See the PDM_SPKn_FMT field in the datasheet for the
	 * meaning of the values
	 */
	unsigned int pdm_fmt[MADERA_MAX_PDM_SPK];

	/** Override default list of firmwares */
	struct wm_adsp_fw_defs *fw_defs[MADERA_MAX_DSP];
	int num_fw_defs[MADERA_MAX_DSP];
};

#endif
