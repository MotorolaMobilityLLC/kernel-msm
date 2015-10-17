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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mods_codec_dev.h>

static int mods_codec_dev_match(struct device *dev,
					struct device_driver *driver)
{
	const struct mods_codec_device_id *id;
	struct mods_codec_device *mods_dev;
	struct mods_codec_dev_driver *mods_drv;

	if (!dev || !driver) {
		pr_err("%s(): device driver not found\n", __func__);
		return 0;
	}

	mods_drv = to_mods_codec_driver(driver);
	mods_dev = to_mods_codec_device(dev);

	if (!mods_dev || !mods_drv)
		return 0;
	id = mods_drv->id_table;

	while (id->name[0]) {
		if (!strncmp(id->name, dev_name(dev), strlen(id->name)))
			return 1;
		id++;
	}

	return 0;
}

static int mods_codec_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	return add_uevent_var(env, "mods_codec_bus=%s",
					dev_name(dev));
}

static void mods_codec_release_dev(struct device *dev)
{
	struct mods_codec_device *mods_dev = to_mods_codec_device(dev);

	kfree(mods_dev);
}

static int mods_codec_bus_probe(struct device *_dev)
{
	int ret;
	struct mods_codec_device *dev;
	struct mods_codec_dev_driver *drv;

	if (!_dev) {
		pr_err("%s(): device is null\n", __func__);
		return -ENODEV;
	}

	dev = to_mods_codec_device(_dev);
	drv = to_mods_codec_driver(dev->dev.driver);
	if (!dev || !drv) {
		pr_err("%s(): mods device is null\n", __func__);
		return -ENODEV;
	}

	if (drv->probe) {
		ret = drv->probe(dev);
		if (ret) {
			dev_err(_dev, "failed probe\n");
			return ret;
		}
	}

	return 0;
}

static int mods_codec_bus_remove(struct device *_dev)
{
	int ret;
	struct mods_codec_device *dev;
	struct mods_codec_dev_driver *drv;

	if (!_dev) {
		pr_err("%s(): device is null\n", __func__);
		return -ENODEV;
	}

	dev = to_mods_codec_device(_dev);
	drv = to_mods_codec_driver(dev->dev.driver);
	if (!dev || !drv)
		return -ENODEV;

	if (drv->remove) {
		ret = drv->remove(dev);
		if (ret) {
			dev_err(_dev, "failed remove\n");
			return ret;
		}
	}

	return 0;
}

struct bus_type mods_codec_bus_type = {
	.name  = "mods_codec_bus",
	.match = mods_codec_dev_match,
	.uevent = mods_codec_uevent,
	.probe = mods_codec_bus_probe,
	.remove = mods_codec_bus_remove,
};

struct mods_codec_device *mods_codec_register_device(
				struct device *mdev,
				const struct mods_codec_device_ops *ops,
				void *priv)
{
	struct mods_codec_device *mods_dev;
	int ret;

	if (!mdev)
		return NULL;

	mods_dev = kzalloc(sizeof(*mods_dev), GFP_KERNEL);
	if (!mods_dev)
		return NULL;
	mods_dev->ops = ops;
	mods_dev->dev.parent = mdev;
	mods_dev->dev.bus = &mods_codec_bus_type;
	mods_dev->dev.release = mods_codec_release_dev;
	mods_dev->priv_data = priv;
	dev_set_name(&mods_dev->dev, "%s.0", dev_name(mdev));

	ret = device_register(&mods_dev->dev);
	if (ret) {
		dev_err(mdev, "%s: failed to register device %d",
				__func__, ret);
		kfree(mods_dev);
		return NULL;
	}

	return mods_dev;
}
EXPORT_SYMBOL_GPL(mods_codec_register_device);

void mods_codec_unregister_device(struct mods_codec_device *dev)
{
	if (dev)
		device_unregister(&dev->dev);
}
EXPORT_SYMBOL_GPL(mods_codec_unregister_device);

static int mods_codec_bus_remove_dev(struct device *dev, void *data)
{
	struct mods_codec_device *mdev;

	if (dev) {
		mdev = to_mods_codec_device(dev);
		mods_codec_unregister_device(mdev);
	}

	return 0;
}

int __init mods_codec_bus_init(void)
{
	int retval;

	retval = bus_register(&mods_codec_bus_type);
	if (retval) {
		pr_err("bus_register failed (%d)\n", retval);
		return retval;
	}

	return 0;
}

void __exit mods_codec_bus_exit(void)
{
	bus_for_each_dev(&mods_codec_bus_type, NULL, NULL,
			mods_codec_bus_remove_dev);
	bus_unregister(&mods_codec_bus_type);
}
