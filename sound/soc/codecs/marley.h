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

#include "arizona_marley.h"
#include "wm_adsp.h"
#include <linux/wakelock.h>

#define MARLEY_FLL1        1
#define MARLEY_FLL1_REFCLK 3
#define MARLEY_FLL_COUNT   1
/* Number of compressed DAI hookups, each pair of DSP and dummy CPU
 * are counted as one DAI
 */
#define MARLEY_NUM_COMPR_DAI 2
#define MARLEY_NUM_COMPR_DEVICES 6

struct marley_compr {
	const char *dai_name;
	struct snd_compr_stream *stream;
	struct wm_adsp *adsp;

	size_t total_copied;
	bool trig;
	bool forced;
	bool allocated;
	bool freed;
	int stream_index;
	struct mutex lock;
};

struct marley_priv {
	struct arizona_priv core;
	struct arizona_fll fll[MARLEY_FLL_COUNT];
	struct marley_compr compr_info[MARLEY_NUM_COMPR_DEVICES];

	struct mutex fw_lock;
	struct wake_lock wakelock;
};

#endif
