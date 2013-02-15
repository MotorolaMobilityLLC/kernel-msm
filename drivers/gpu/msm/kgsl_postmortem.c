/*
 * Copyright (c) 2013, Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/export.h>
#include <linux/sysfs.h>

#include "kgsl_device.h"

struct kgsl_postmortem_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
		  size_t count);
};

#define to_postmortem_attr(a) \
container_of(a, struct kgsl_postmortem_attribute, attr)

#define kobj_to_device(a) \
container_of(a, struct kgsl_device, postmortem_kobj)

static ssize_t dump_show(struct kgsl_device *device, char *buf)
{
	int ret = 0;

	if (device->postmortem_dump != NULL && device->postmortem_size > 0)
		ret = scnprintf(buf, PAGE_SIZE,
			"%s\n", device->postmortem_dump);
	else
		ret = scnprintf(buf, PAGE_SIZE,
			"device [%s] does not support postmortem dump\n",
			device->name);

	return ret;
}

#define POSTMORTEM_ATTR(_name, _mode, _show, _store) \
struct kgsl_postmortem_attribute attr_##_name = { \
	.attr = { .name = __stringify(_name), .mode = _mode }, \
	.show = _show, \
	.store = _store, \
}

POSTMORTEM_ATTR(dump, 0444, dump_show, NULL);

static void postmortem_sysfs_release(struct kobject *kobj)
{
}

static ssize_t postmortem_sysfs_show(struct kobject *kobj,
				     struct attribute *attr, char *buf)
{
	struct kgsl_postmortem_attribute *pattr = to_postmortem_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret;

	if (device && pattr->show)
		ret = pattr->show(device, buf);
	else
		ret = -EIO;

	return ret;
}

static ssize_t postmortem_sysfs_store(struct kobject *kobj,
				      struct attribute *attr,
	  const char *buf, size_t count)
{
	struct kgsl_postmortem_attribute *pattr = to_postmortem_attr(attr);
	struct kgsl_device *device = kobj_to_device(kobj);
	ssize_t ret;

	if (device && pattr->store)
		ret = pattr->store(device, buf, count);
	else
		ret = -EIO;

	return ret;
}

static const struct sysfs_ops postmortem_sysfs_ops = {
	.show = postmortem_sysfs_show,
	.store = postmortem_sysfs_store,
};

static struct kobj_type ktype_postmortem = {
	.sysfs_ops = &postmortem_sysfs_ops,
	.default_attrs = NULL,
	.release = postmortem_sysfs_release,
};

int kgsl_device_postmortem_init(struct kgsl_device *device)
{
	int ret = 0;

	device->postmortem_dump = NULL;
	device->postmortem_pos = 0;
	device->postmortem_size = 0;

	ret = kobject_init_and_add(&device->postmortem_kobj, &ktype_postmortem,
				    &device->dev->kobj, "postmortem");
	if (ret)
		goto done;

	ret  = sysfs_create_file(&device->postmortem_kobj, &attr_dump.attr);
	if (ret)
		goto done;

done:
	return ret;
}
EXPORT_SYMBOL(kgsl_device_postmortem_init);

void kgsl_device_postmortem_close(struct kgsl_device *device)
{
	sysfs_remove_file(&device->postmortem_kobj, &attr_dump.attr);

	kobject_put(&device->postmortem_kobj);

	device->postmortem_dump = NULL;
	device->postmortem_pos = 0;
	device->postmortem_size = 0;
}
EXPORT_SYMBOL(kgsl_device_postmortem_close);
