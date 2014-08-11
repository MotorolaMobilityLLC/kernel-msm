/*
 * arch/x86/platform/intel-mid/fugu/asustek-pcbid.c
 *
 * Copyright (C) 2014 ASUSTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <linux/board_asustek.h>
#include <linux/lnw_gpio.h>
#include <linux/sfi.h>
#include "intel-mid-fugu.h"

#define PCBID_VALUE_INVALID 0x4E2F4100 /* N/A */

enum {
	DEBUG_STATE = 1U << 0,
	DEBUG_VERBOSE = 1U << 1,
};

static int debug_mask = DEBUG_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static unsigned int asustek_pcbid = PCBID_VALUE_INVALID;
static unsigned int hw_rev_pcbid[] = {0, 1, 2};

#ifdef CONFIG_SFI
static bool sfi_pcbid_initialized = false;
static unsigned int asustek_pcbid_oem1 = PCBID_VALUE_INVALID;
#endif

struct pcbid_maps {
	unsigned char name[16];
	unsigned int *pcbid;
	unsigned int pcbid_num;
} asustek_pcbid_maps[] = {
	{"HW_REV", hw_rev_pcbid, ARRAY_SIZE(hw_rev_pcbid)},
};

#define NUM_MAPS (sizeof(asustek_pcbid_maps) / sizeof(asustek_pcbid_maps[0]))

int get_pcbid_type(const char *func)
{
	int i = 0, ret = 0;
	struct pcbid_maps *map = NULL;

#ifdef CONFIG_SFI
	if (!sfi_pcbid_initialized && (asustek_pcbid == PCBID_VALUE_INVALID)) {
#else
	if (asustek_pcbid == PCBID_VALUE_INVALID) {
#endif
		pr_err("ASUSTek PCBID was invalid\n");
		return -ENODEV;
	}

	for (i = 0; i < NUM_MAPS; i++) {
		if (!strcmp(func, asustek_pcbid_maps[i].name)) {
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("%s was found\n", func);

			map = &asustek_pcbid_maps[i];
			break;
		}
	}

	if (map) {
		/* found */
		for (i = 0; i < map->pcbid_num; i++) {
#ifdef CONFIG_SFI
			if (asustek_pcbid == PCBID_VALUE_INVALID)
				ret += asustek_pcbid_oem1 & BIT(map->pcbid[i]);
			else
#endif
				ret += asustek_pcbid & BIT(map->pcbid[i]);
		}
		ret = ret >> map->pcbid[0];
	} else
		ret = -ENODEV;

	return ret;
}

hw_rev asustek_get_hw_rev(void)
{
	hw_rev ret = get_pcbid_type("HW_REV");

	if (debug_mask & DEBUG_VERBOSE)
		pr_info("%s: %d\n", __func__, ret);

	if ((ret == -ENODEV) || (ret >= HW_REV_MAX))
		ret = HW_REV_INVALID;

	return ret;
}
EXPORT_SYMBOL(asustek_get_hw_rev);

#define ASUSTEK_PCBID_ATTR(module) \
static struct kobj_attribute module##_attr = { \
	.attr = { \
		.name = __stringify(module), \
		.mode = 0444, \
	}, \
	.show = module##_show, \
}

static ssize_t asustek_pcbid_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%03x\n", asustek_pcbid);
}

static ssize_t asustek_projectid_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%02x\n", (asustek_pcbid >> 3) & 0x7);
}

ASUSTEK_PCBID_ATTR(asustek_pcbid);
ASUSTEK_PCBID_ATTR(asustek_projectid);

static struct attribute *attr_list[] = {
	&asustek_pcbid_attr.attr,
	&asustek_projectid_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attr_list,
};

#ifdef CONFIG_SFI
int sfi_parse_oem1(struct sfi_table_header *table)
{
	struct sfi_table_oem1 *oem1;
	u8 sig[SFI_SIGNATURE_SIZE + 1] = {'\0'};
	u8 oem_id[SFI_OEM_ID_SIZE + 1] = {'\0'};
	u8 oem_table_id[SFI_OEM_TABLE_ID_SIZE + 1] = {'\0'};

	oem1 = (struct sfi_table_oem1 *) table;

	if (!oem1) {
		pr_err("%s: fail to read SFI OEM1 Layout\n",
			__func__);
		return -ENODEV;
	}

	snprintf(sig, (SFI_SIGNATURE_SIZE + 1), "%s", oem1->header.sig);
	snprintf(oem_id, (SFI_OEM_ID_SIZE + 1), "%s", oem1->header.oem_id);
	snprintf(oem_table_id, (SFI_OEM_TABLE_ID_SIZE + 1), "%s",
		 oem1->header.oem_table_id);

	pr_info("SFI OEM1 Layout\n");
	pr_info("\tOEM1 signature               : %s\n"
		"\tOEM1 length                  : %d\n"
		"\tOEM1 revision                : %d\n"
		"\tOEM1 checksum                : 0x%X\n"
		"\tOEM1 oem_id                  : %s\n"
		"\tOEM1 oem_table_id            : %s\n"
		"\tOEM1 ifwi_rc			: 0x%02X\n"
		"\tPCBID hardware_id            : 0x%02X\n"
		"\tPCBID project_id             : 0x%02X\n"
		"\tPCBID ram_id                 : 0x%02X\n",
		sig,
		oem1->header.len,
		oem1->header.rev,
		oem1->header.csum,
		oem_id,
		oem_table_id,
		oem1->ifwi_rc,
		oem1->hardware_id,
		oem1->project_id,
		oem1->ram_id
		);

	asustek_pcbid_oem1 = oem1->hardware_id | oem1->project_id << 3 |
			oem1->ram_id << 6;

	return 0;
}

void sfi_parsing_done(bool initialized)
{
	sfi_pcbid_initialized = initialized;
}
#endif

static int __init pcbid_driver_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct resource *res;
	unsigned int value;
	int gpio = -1;

	if (!pdev)
		return -EINVAL;

#ifdef CONFIG_SFI
	/* Get SFI OEM1 Layout first */
	sfi_parsing_done(sfi_table_parse(SFI_SIG_OEM1, NULL, NULL,
				sfi_parse_oem1) ? false : true);
	if (!sfi_pcbid_initialized) {
		pr_info("ASUSTek: Cannot parse PCB_ID layout from SFI OEM1.\n");
		asustek_pcbid_oem1 = 0;
	}
#endif

	asustek_pcbid = 0;

	for (i = 0; i < pdev->num_resources; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IO, i);
		if (!res)
			return -ENODEV;

		gpio = res->start;
		/*
		 * change necessary GPIO pin mode for PCB_ID module working.
		 * This is something should be done in IA firmware.
		 * But, anyway, just do it here in case IA firmware
		 * forget to do so.
		 */
		lnw_gpio_set_alt(gpio, LNW_GPIO);

		if (debug_mask & DEBUG_VERBOSE)
			pr_info("ASUSTek: Requesting gpio%d\n", gpio);

		ret = gpio_request(gpio, res->name);
		if (ret) {
			/* indicate invalid pcbid value when error happens */
			pr_err("ASUSTek: Failed to request gpio%d\n", gpio);
			asustek_pcbid = PCBID_VALUE_INVALID;
			res = NULL;
			break;
		}

		ret = gpio_direction_input(gpio);
		if (ret) {
			/* indicate invalid pcbid value when error happens */
			pr_err("ASUSTek: Failed to configure direction for gpio%d\n",
					gpio);
			asustek_pcbid = PCBID_VALUE_INVALID;
			res = NULL;
			break;
		}

		/* read input value through gpio library directly */
		value = gpio_get_value(gpio) ? 1 : 0;
		if (debug_mask & DEBUG_VERBOSE)
			pr_info("ASUSTek: Input value of gpio%d is %s\n", gpio,
					value ? "high" : "low");

		asustek_pcbid |= value << i;
	}


#ifdef CONFIG_SFI
	if (sfi_pcbid_initialized && (asustek_pcbid_oem1 != asustek_pcbid)) {
		if (debug_mask && DEBUG_STATE)
			pr_info("ASUSTek: OEM1 PCBID=%03x\n",
						asustek_pcbid_oem1);
		WARN_ON(1);
	}
#endif

	if (asustek_pcbid == PCBID_VALUE_INVALID) {

#ifdef CONFIG_SFI
		asustek_pcbid = asustek_pcbid_oem1;
#endif

		/* error handler to free allocated gpio resources */
		while (i >= 0) {
			res = platform_get_resource(pdev, IORESOURCE_IO, i);
			if (!res)
				return -ENODEV;

			if (debug_mask & DEBUG_VERBOSE)
				pr_info("ASUSTek: Freeing gpio%d\n", gpio);

			gpio = res->start;
			gpio_free(gpio);
			i--;
		}
	} else {
		/* report pcbid info to dmesg */
		if (debug_mask && DEBUG_STATE)
			pr_info("ASUSTek: PCBID=%05x\n", asustek_pcbid);

		/* create a sysfs interface */
		ret = sysfs_create_group(&pdev->dev.kobj, &attr_group);

		if (ret)
			pr_err("ASUSTek: Failed to create sysfs group\n");
	}

	return ret;
}

static int pcbid_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver asustek_pcbid_driver __refdata = {
	.probe = pcbid_driver_probe,
	.remove = pcbid_driver_remove,
	.driver = {
		.name = "asustek_pcbid",
		.owner = THIS_MODULE,
	},
};

static int asustek_pcbid_init(void)
{
	return platform_driver_register(&asustek_pcbid_driver);
}

rootfs_initcall(asustek_pcbid_init);

MODULE_DESCRIPTION("ASUSTek PCBID driver");
MODULE_AUTHOR("Paris Yeh <paris_yeh@asus.com>");
MODULE_LICENSE("GPL");
