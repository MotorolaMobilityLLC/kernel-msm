/*
 * marley.h  --  ALSA SoC Audio driver for Marley class codecs
 *
 * Copyright 2015 Cirrus Logic
 *
 * Author: Piotr Stankiewicz <piotrs@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MARLEY_H
#define _MARLEY_H

#include "arizona.h"

#define MARLEY_FLL1        1
#define MARLEY_FLL1_REFCLK 3
#define MARLEY_FLL_COUNT   1
/* Number of compressed DAI hookups, each pair of DSP and dummy CPU
 * are counted as one DAI
 */
#define MARLEY_NUM_COMPR_DAI 2

struct marley_compr {
#if 0
	struct wm_adsp_compr adsp_compr;
#endif
	const char *dai_name;
	bool trig;
	struct mutex trig_lock;
	struct marley_priv *priv;
};

struct marley_priv {
	struct arizona_priv core;
	struct arizona_fll fll[MARLEY_FLL_COUNT];
	struct marley_compr compr_info[MARLEY_NUM_COMPR_DAI];

	struct mutex fw_lock;

#ifdef CONFIG_SND_SOC_OPALUM
	struct workqueue_struct *ospl2xx_wq;
	struct work_struct ospl2xx_config;
#endif
};

#endif
