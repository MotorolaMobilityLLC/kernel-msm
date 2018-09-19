/*
 * Copyright (C) 2015 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MODS_CODEC_DEV_H_
#define _MODS_CODEC_DEV_H_

#include <linux/device.h>
#include <linux/platform_device.h>
#include <sound/soc.h>

extern struct bus_type mods_codec_bus_type;

#define to_mods_codec_device(x) container_of(x, struct mods_codec_device, \
			dev)
#define to_mods_codec_driver(x) container_of(x, \
			struct mods_codec_dev_driver, driver)

int mods_codec_bus_init(void);
void mods_codec_bus_exit(void);

struct mods_codec_device_ops {
	const struct snd_soc_dai_ops *dai_ops;
	int (*probe)(struct snd_soc_codec *);
	int (*remove)(struct snd_soc_codec *);
};

struct mods_codec_device {
	const struct mods_codec_device_ops *ops;
	struct device dev;
	void *priv_data;
};

struct mods_codec_device_id {
	char name[PLATFORM_NAME_SIZE];
	int id;
};

struct mods_codec_dev_driver {
	struct device_driver driver;
	const struct mods_codec_device_id *id_table;
	int (*probe)(struct mods_codec_device *);
	int (*remove)(struct mods_codec_device *);
};

struct mods_codec_device *mods_codec_register_device(
			struct device *dev,
			const struct mods_codec_device_ops *ops,
			void *priv_data);
void mods_codec_unregister_device(struct mods_codec_device *dev);

#endif
