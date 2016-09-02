/*
 * Madera MFD internals
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MADERA_CORE_H
#define _MADERA_CORE_H

#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/madera/pdata.h>
#include <linux/irqchip/irq-madera.h>

enum madera_type {
	CS47L35 = 1,
	CS47L85 = 2,
	CS47L90 = 3,
	CS47L91 = 4,
	WM1840 = 7,
};

#define MADERA_MAX_CORE_SUPPLIES 2

/* Notifier events */
#define MADERA_NOTIFY_VOICE_TRIGGER	0x1
#define MADERA_NOTIFY_HPDET		0x2
#define MADERA_NOTIFY_MICDET		0x4

struct snd_soc_dapm_context;
struct madera_extcon_info;

struct madera {
	struct regmap *regmap;
	struct regmap *regmap_32bit;

	struct device *dev;

	enum madera_type type;
	unsigned int rev;

	int num_core_supplies;
	struct regulator_bulk_data core_supplies[MADERA_MAX_CORE_SUPPLIES];
	struct regulator *dcvdd;
	struct notifier_block dcvdd_notifier;
	bool internal_dcvdd;
	bool micvdd_regulated;
	bool bypass_cache;
	bool dcvdd_powered_off;

	struct madera_pdata pdata;

	struct device *irq_dev;
	int irq;

	bool hpdet_clamp[MADERA_MAX_ACCESSORY];
	unsigned int hp_ena;
	unsigned int hp_impedance_x100[MADERA_MAX_ACCESSORY];

	struct madera_extcon_info *extcon_info;

	struct snd_soc_dapm_context *dapm;

	struct mutex reg_setting_lock;

	struct blocking_notifier_head notifier;
};

extern int madera_of_read_uint_array(struct madera *madera, const char *prop,
				     bool mandatory,
				     unsigned int *dest,
				     int minlen, int maxlen);
extern int madera_of_read_uint(struct madera *madera, const char *prop,
				bool mandatory, unsigned int *data);

extern int madera_get_num_micbias(struct madera *madera,
				  unsigned int *n_micbiases,
				  unsigned int *n_child_micbiases);

extern const char *madera_name_from_type(enum madera_type type);

static inline int madera_of_read_int(struct madera *madera, const char *prop,
				     bool mandatory, int *data)
{
	return madera_of_read_uint(madera, prop, mandatory,
				   (unsigned int *)data);
}

static inline int madera_call_notifiers(struct madera *madera,
					unsigned long event,
					void *data)
{
	return blocking_notifier_call_chain(&madera->notifier, event, data);
}
#endif
