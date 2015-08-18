/*
 * linux/sound/cs35l34.h -- Platform data for CS35l34
 *
 * Copyright (c) 2015 Cirrus Logic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CS35L34_H
#define __CS35L34_H

struct cs35l34_platform_data {

	/* Low Battery Threshold */
	unsigned int batt_thresh;
	/* Low Battery Recovery */
	unsigned int batt_recov;
	/* LED/Audio Power Management */
	/* LED Current Management*/
	unsigned int led_mng;
	/* Audio Gain w/ LED */
	unsigned int audiogain_mng;
	/* Boost Management */
	unsigned int boost_mng;
	/* Peak current */
	unsigned int boost_peak;
	/* Boost converter limiter */
	unsigned int boost_limit;
	/* Data CFG for DUAL device */
	unsigned int sdout_datacfg;
	/* SDOUT Sharing */
	unsigned int sdout_share;
	/* Boost Controller Voltage Setting */
	unsigned int boost_ctl;
	/* Gain Change Zero Cross */
	unsigned int gain_zc;
	/* Amplifier Drive Select */
	unsigned int amp_drv_sel;
	/* Predictive Browout Protection 1 = Off 0 = On */
	unsigned int pred_brownout;
	/* Predictive Brownout threshold voltage, volume and mute */
	unsigned int vpbr_thld1;
	unsigned int vpbr_vol;
	unsigned int vpbr_mute;
	/* Predictive Brownout wait, release and attack rates */
	unsigned int vpbr_wait;
	unsigned int vpbr_rel;
	unsigned int vpbr_atk;
	/* Predictive Brownout max attenuation and speaker load */
	unsigned int vpbr_att;
	unsigned int vpbr_preload;
	/* Predictive Brownout voltage release threshold,
	release volume and attack volume */
	unsigned int vpbr_rthld;
	unsigned int vpbr_relvol;
	unsigned int vpbr_atkvol;
	/* Predictive Brownout release rate,
	attack rate, predictive wait, pred safe and pred Z VP */
	unsigned int vpbr_relrate;
	unsigned int vpbr_atkrate;
	unsigned int vpbr_predwait;
	unsigned int vpbr_predsafe;
	unsigned int vpbr_predzvp;
	/* Predictive Brownout pred safe VP(I),
	mute status and attenuation status */
	unsigned int vpbr_safevpi;
	unsigned int vpbr_mutestat;
	unsigned int vpbr_status;
	unsigned int vpbr_predstat;
	/* IMON and VMON polarity and I2S SDIN channel,
	TX data locations when in ADSP I2S mode */
	unsigned int imon_pol;
	unsigned int vmon_pol;
	unsigned int i2s_sdinloc;
	unsigned int i2s_txout;
	/* Reactive Browout Protection Flag 1 = Off 0 = On */
	unsigned int react_brownout;

	/* PDM control */
	unsigned int pdm_chsel;
	unsigned int pdm_audio;

	/* GPIO RESET */
	int gpio_nreset;
};

#endif /* __CS35L34_H */
