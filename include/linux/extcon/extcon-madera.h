/*
 * extcon-madera.h - public extcon driver API for Cirrus Logic Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXTCON_MADERA_H_
#define _EXTCON_MADERA_H_

#include <linux/mfd/madera/registers.h>

#define MADERA_MICD_LVL_1_TO_7 \
		(MADERA_MICD_LVL_1 | MADERA_MICD_LVL_2 | \
		 MADERA_MICD_LVL_3 | MADERA_MICD_LVL_4 | \
		 MADERA_MICD_LVL_5 | MADERA_MICD_LVL_6 | \
		 MADERA_MICD_LVL_7)

#define MADERA_MICD_LVL_0_TO_7 (MADERA_MICD_LVL_0 | MADERA_MICD_LVL_1_TO_7)

#define MADERA_MICD_LVL_0_TO_8 (MADERA_MICD_LVL_0_TO_7 | MADERA_MICD_LVL_8)

/* Notify data structure for MADERA_NOTIFY_HPDET */
struct madera_hpdet_notify_data {
	unsigned int impedance_x100;	/* ohms * 100 */
};

/* Notify data structure for MADERA_NOTIFY_MICDET */
struct madera_micdet_notify_data {
	unsigned int impedance_x100;	/* ohms * 100 */
	bool present;
	int out_num;			/* 1 = OUT1, 2 = OUT2 */
};

struct madera_extcon_info;

enum madera_accdet_mode {
	MADERA_ACCDET_MODE_MIC,
	MADERA_ACCDET_MODE_HPL,
	MADERA_ACCDET_MODE_HPR,
	MADERA_ACCDET_MODE_HPM,
	MADERA_ACCDET_MODE_ADC,
	MADERA_ACCDET_MODE_INVALID,
};

struct madera_jd_state {
	enum madera_accdet_mode mode;

	int (*start)(struct madera_extcon_info *);
	void (*restart)(struct madera_extcon_info *);
	int (*reading)(struct madera_extcon_info *, int);
	void (*stop)(struct madera_extcon_info *);

	int (*timeout_ms)(struct madera_extcon_info *);
	void (*timeout)(struct madera_extcon_info *);
};

int madera_jds_set_state(struct madera_extcon_info *info,
			 const struct madera_jd_state *new_state);

extern void madera_set_headphone_imp(struct madera_extcon_info *info,
				     int imp);

extern const struct madera_jd_state madera_hpdet_left;
extern const struct madera_jd_state madera_hpdet_right;
extern const struct madera_jd_state madera_micd_button;
extern const struct madera_jd_state madera_micd_microphone;
extern const struct madera_jd_state madera_micd_adc_mic;

extern int madera_hpdet_start(struct madera_extcon_info *info);
extern void madera_hpdet_restart(struct madera_extcon_info *info);
extern void madera_hpdet_stop(struct madera_extcon_info *info);
extern int madera_hpdet_reading(struct madera_extcon_info *info, int val);

extern int madera_micd_start(struct madera_extcon_info *info);
extern void madera_micd_stop(struct madera_extcon_info *info);
extern int madera_micd_button_reading(struct madera_extcon_info *info, int val);

extern int madera_micd_mic_start(struct madera_extcon_info *info);
extern void madera_micd_mic_stop(struct madera_extcon_info *info);
extern int madera_micd_mic_reading(struct madera_extcon_info *info, int val);
extern int madera_micd_mic_timeout_ms(struct madera_extcon_info *info);
extern void madera_micd_mic_timeout(struct madera_extcon_info *info);

extern void madera_extcon_report(struct madera_extcon_info *info, int state);

#endif
