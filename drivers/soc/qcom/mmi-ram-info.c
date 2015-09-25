/*
 * Copyright (C) 2012 Motorola Mobility LLC
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
#include <soc/qcom/smsm.h>
#include <linux/pstore.h>

static struct {
	unsigned mr5;
	unsigned mr6;
	unsigned mr7;
	unsigned mr8;
	unsigned ramsize;
} *smem_ddr_info;

static char sysfsram_type_name[20] = "unknown";
static char sysfsram_vendor_name[20] = "unknown";
static uint32_t sysfsram_ramsize;

static ssize_t sysfsram_mr_register_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	uint32_t val = 0;
	const char *name = attr->attr.name;

	if (smem_ddr_info != NULL &&
		strnlen(name, 4) == 3 && name[0] == 'm' && name[1] == 'r')
	{
		switch (name[2]) {
		case '5': val = smem_ddr_info->mr5; break;
		case '6': val = smem_ddr_info->mr6; break;
		case '7': val = smem_ddr_info->mr7; break;
		case '8': val = smem_ddr_info->mr8; break;
		}
	}

	return snprintf(buf, 6, "0x%02x\n", val);
}

static ssize_t sysfsram_size_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 12, "%u\n", sysfsram_ramsize);
}

static ssize_t sysfsram_info_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 60, "%s:%s:%uMB\n",
			sysfsram_vendor_name,
			sysfsram_type_name,
			sysfsram_ramsize);
}

static ssize_t sysfsram_type_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%s\n", sysfsram_type_name);
}

static struct kobj_attribute ddr_mr5_register_attr =
	__ATTR(mr5, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_mr6_register_attr =
	__ATTR(mr6, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_mr7_register_attr =
	__ATTR(mr7, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_mr8_register_attr =
	__ATTR(mr8, 0444, sysfsram_mr_register_show, NULL);

static struct kobj_attribute ddr_size_attr =
	__ATTR(size, 0444, sysfsram_size_show, NULL);

static struct kobj_attribute ddr_type_attr =
	__ATTR(type, 0444, sysfsram_type_show, NULL);

static struct kobj_attribute ddr_info_attr =
	__ATTR(info, 0444, sysfsram_info_show, NULL);

static struct attribute *ram_info_properties_attrs[] = {
	&ddr_mr5_register_attr.attr,
	&ddr_mr6_register_attr.attr,
	&ddr_mr7_register_attr.attr,
	&ddr_mr8_register_attr.attr,
	&ddr_size_attr.attr,
	&ddr_type_attr.attr,
	&ddr_info_attr.attr,
	NULL
};

static struct attribute_group ram_info_properties_attr_group = {
	.attrs = ram_info_properties_attrs,
};

static int __init init_mmi_ram_info(void)
{
	int status = 0;
	static struct kobject *ram_info_properties_kobj;
	uint32_t vid, tid;
	const char *tname = "unknown";
	const char *vname = "unknown";
	static const char *vendors[] = {
		"unknown",
		"Samsung",
		"Qimonda",
		"Elpida",
		"Etron",
		"Nanya",
		"Hynix",
		"Mosel",
		"Winbond",
		"ESMT",
		"unknown",
		"Spansion",
		"SST",
		"ZMOS",
		"Intel"
	};
	static const char *types[] = {
		"S4 SDRAM",
		"S2 SDRAM",
		"N NVM",
		"Reserved"
	};

	smem_ddr_info = smem_alloc(SMEM_SDRAM_INFO, sizeof(*smem_ddr_info), 0,
			SMEM_ANY_HOST_FLAG);

	if (smem_ddr_info != NULL) {
		char apanic_annotation[128];

		/* identify vendor */
		vid = smem_ddr_info->mr5 & 0xFF;
		if (vid < (sizeof(vendors)/sizeof(vendors[0])))
			vname = vendors[vid];
		else if (vid == 0xFE)
			vname = "Numonyx";
		else if (vid == 0xFF)
			vname = "Micron";

		snprintf(sysfsram_vendor_name, sizeof(sysfsram_vendor_name),
			"%s", vname);

		/* identify type */
		tid = smem_ddr_info->mr8 & 0x03;
		if (tid < (sizeof(types)/sizeof(types[0])))
			tname = types[tid];

		snprintf(sysfsram_type_name, sizeof(sysfsram_type_name),
			"%s", tname);

		/* extract size */
		sysfsram_ramsize = smem_ddr_info->ramsize;

		snprintf(apanic_annotation, sizeof(apanic_annotation),
			"RAM: %s, %s, %u MB, MR5:0x%02X, MR6:0x%02X, "
			"MR7:0x%02X, MR8:0x%02X\n",
			vname, tname, smem_ddr_info->ramsize,
			smem_ddr_info->mr5, smem_ddr_info->mr6,
			smem_ddr_info->mr7, smem_ddr_info->mr8);
		pstore_annotate(apanic_annotation);
	}
	else {
		/* complain, but do not fail if SMEM was not allocated */
		/* defaults will be reported */
		pr_err("%s: failed to access RAM info in SMEM\n", __func__);
	}

	/* create sysfs object */
	ram_info_properties_kobj = kobject_create_and_add("ram", NULL);

	if (ram_info_properties_kobj)
		status = sysfs_create_group(ram_info_properties_kobj,
			&ram_info_properties_attr_group);

	if (!ram_info_properties_kobj || status) {
		pr_err("%s: failed to create /sys/ram\n", __func__);
		return 1;
	}
	else
		return 0;
}

module_init(init_mmi_ram_info);
MODULE_DESCRIPTION("Motorola Mobility Inc. RAM Info");
MODULE_LICENSE("GPL v2");
