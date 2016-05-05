/*
 * Copyright (C) 2016 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/pstore.h>
#include "mmi_storage_info.h"

struct mmi_storage_info *info;

static ssize_t sysfsst_fw_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%s\n", info->firmware_version);
}

static ssize_t sysfsst_model_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%s\n", info->product_name);
}

static ssize_t sysfsst_vendor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 32, "%s\n", info->card_manufacturer);
}

static ssize_t sysfsst_size_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 16, "%s\n", info->size);
}

static ssize_t sysfsst_type_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 16, "%s\n", info->type);
}

static struct kobj_attribute st_fw_attr =
	__ATTR(fw, 0444, sysfsst_fw_show, NULL);

static struct kobj_attribute st_model_attr =
	__ATTR(model, 0444, sysfsst_model_show, NULL);

static struct kobj_attribute st_vendor_attr =
	__ATTR(vendor, 0444, sysfsst_vendor_show, NULL);

static struct kobj_attribute st_size_attr =
	__ATTR(size, 0444, sysfsst_size_show, NULL);

static struct kobj_attribute st_type_attr =
	__ATTR(type, 0444, sysfsst_type_show, NULL);

static struct attribute *st_info_properties_attrs[] = {
	&st_fw_attr.attr,
	&st_model_attr.attr,
	&st_vendor_attr.attr,
	&st_size_attr.attr,
	&st_type_attr.attr,
	NULL
};

static struct attribute_group st_info_properties_attr_group = {
	.attrs = st_info_properties_attrs,
};

static int __init mmi_storage_info_init(void)
{
	int ret = 0;
	int status = 0;
	struct property *p;
	struct device_node *n;
	char apanic_annotation[128];
	static struct kobject *st_info_properties_kobj;

	n = of_find_node_by_path("/chosen/mmi,storage");
	if (n == NULL) {
		ret = 1;
		goto err;
	}

	info = kzalloc(sizeof(struct mmi_storage_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: failed to allocate space for mmi_storage_info\n",
			__func__);
		ret = 1;
		goto err;
	}

	for_each_property_of_node(n, p) {
		if (!strcmp(p->name, "type") && p->value)
			strlcpy(info->type, (char *)p->value,
				sizeof(info->type));
		if (!strcmp(p->name, "size") && p->value)
			strlcpy(info->size, (char *)p->value,
				sizeof(info->size));
		if (!strcmp(p->name, "manufacturer") && p->value)
			strlcpy(info->card_manufacturer, (char *)p->value,
				sizeof(info->card_manufacturer));
		if (!strcmp(p->name, "product") && p->value)
			strlcpy(info->product_name, (char *)p->value,
				sizeof(info->product_name));
		if (!strcmp(p->name, "firmware") && p->value)
			strlcpy(info->firmware_version, (char *)p->value,
				sizeof(info->firmware_version));
	}

	of_node_put(n);

	pr_info("mmi_storage_info :%s: %s %s %s FV=%s\n",
		info->type,
		info->size,
		info->card_manufacturer,
		info->product_name,
		info->firmware_version);

	/* Append the storage info to apanic annotation */
	snprintf(apanic_annotation, sizeof(apanic_annotation),
		"%s: %s %s %s FV=%s\n",
		info->type,
		info->size,
		info->card_manufacturer,
		info->product_name,
		info->firmware_version);
	pstore_annotate(apanic_annotation);

	/* Export to sysfs*/
	st_info_properties_kobj = kobject_create_and_add("storage", NULL);
	if (st_info_properties_kobj)
		status = sysfs_create_group(st_info_properties_kobj,
				&st_info_properties_attr_group);

	if (!st_info_properties_kobj || status) {
		pr_err("%s: failed to create /sys/storage\n", __func__);
		ret = 1;
		goto err;
	}

err:
	return ret;
}

void __exit mmi_storage_info_exit(void)
{
	if (info) {
		kfree(info);
		info = NULL;
	}
}

module_init(mmi_storage_info_init);
module_exit(mmi_storage_info_exit);
MODULE_DESCRIPTION("Motorola Mobility LLC. Storage Info");
MODULE_LICENSE("GPL v2");
