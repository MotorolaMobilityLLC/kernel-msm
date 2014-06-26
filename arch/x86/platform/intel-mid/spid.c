/*
 * spid.c: Soft Platform ID parsing and init code
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/sfi.h>
#include <linux/intel_pmic_gpio.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/skbuff.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/intel_mid_pm.h>
#include <linux/hsi/hsi.h>
#include <linux/spinlock.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/blkdev.h>
#include <linux/acpi.h>
#include <linux/intel_pidv_acpi.h>

#include <asm/setup.h>
#include <asm/mpspec_def.h>
#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/intel-mid.h>
#include <asm/io.h>
#include <asm/i8259.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/apb_timer.h>
#include <asm/reboot.h>
#include "intel_mid_weak_decls.h"

char intel_platform_ssn[INTEL_PLATFORM_SSN_SIZE + 1];
struct soft_platform_id spid;
EXPORT_SYMBOL(spid);

#ifdef CONFIG_ACPI
struct platform_id pidv;
struct kobject *pidv_kobj;
#endif

struct kobject *spid_kobj;

/*
 *
 */
static void populate_spid_cmdline(void)
{
	char *spid_param, *spid_default_value;
	char spid_cmdline[SPID_CMDLINE_SIZE+1];

	/* parameter format : cust:vend:manu:plat:prod:hard */
	snprintf(spid_cmdline, sizeof(spid_cmdline),
		 "%04x:%04x:%04x:%04x:%04x:%04x",
		 spid.vendor_id,
		 spid.customer_id,
		 spid.manufacturer_id,
		 spid.platform_family_id,
		 spid.product_line_id,
		 spid.hardware_id);

	/* is there a spid param ? */
	spid_param = strstr(saved_command_line, SPID_PARAM_NAME);
	if (spid_param) {
		/* is the param set to default value ? */
		spid_default_value = strstr(saved_command_line,
					    SPID_DEFAULT_VALUE);
		if (spid_default_value) {
			spid_param += strlen(SPID_PARAM_NAME);
			if (strlen(spid_param) > strlen(spid_cmdline))
				memcpy(spid_param, spid_cmdline,
						strlen(spid_cmdline));
			else
				pr_err("Not enough free space for SPID in command line.\n");
		} else
			pr_warn("SPID already populated. Don't overwrite.\n");
	} else
		pr_err("SPID not found in kernel command line.\n");
}

static ssize_t customer_id_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04x\n", spid.customer_id);
}
spid_attr(customer_id);

static ssize_t vendor_id_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	return sprintf(buf, "%04x\n", spid.vendor_id);
}
spid_attr(vendor_id);

static ssize_t manufacturer_id_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04x\n", spid.manufacturer_id);
}
spid_attr(manufacturer_id);

static ssize_t platform_family_id_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04x\n", spid.platform_family_id);
}
spid_attr(platform_family_id);

static ssize_t product_line_id_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04x\n", spid.product_line_id);
}
spid_attr(product_line_id);

static ssize_t hardware_id_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04x\n", spid.hardware_id);
}
spid_attr(hardware_id);

static ssize_t fru_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%02x\n%02x\n%02x\n%02x\n%02x\n"
			    "%02x\n%02x\n%02x\n%02x\n%02x\n",
			spid.fru[0], spid.fru[1], spid.fru[2], spid.fru[3],
			spid.fru[4], spid.fru[5], spid.fru[6], spid.fru[7],
			spid.fru[8], spid.fru[9]);
}
spid_attr(fru);

static struct attribute *spid_attrs[] = {
	&customer_id_attr.attr,
	&vendor_id_attr.attr,
	&manufacturer_id_attr.attr,
	&platform_family_id_attr.attr,
	&product_line_id_attr.attr,
	&hardware_id_attr.attr,
	&fru_attr.attr,
	NULL,
};

static struct attribute_group spid_attr_group = {
	.attrs = spid_attrs,
};

#ifdef CONFIG_ACPI
static ssize_t iafw_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04X.%04X\n", pidv.iafwrevvalues[revmajor],
		pidv.iafwrevvalues[revminor]);
}
pidv_attr(iafw_version);

static ssize_t secfw_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%01d.%01d.%02d.%04d\n",
		(pidv.secrevvalues[0]),
		(pidv.secrevvalues[1]),
		(pidv.secrevvalues[2]),
		(pidv.secrevvalues[3]));
}
pidv_attr(secfw_version);

static ssize_t pdr_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%04d.%04d\n", pidv.pdrrevvalues[revmajor],
		pidv.pdrrevvalues[revminor]);
}
pidv_attr(pdr_version);

static ssize_t ifwi_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%02d.%02d\n",
		(pidv.iasvn / 100),
		(pidv.iasvn % 100));
}
pidv_attr(ifwi_version);

static struct attribute *pidv_attrs[] = {
	&iafw_version_attr.attr,
	&secfw_version_attr.attr,
	&pdr_version_attr.attr,
	&ifwi_version_attr.attr,
	NULL,
};

static struct attribute_group pidv_attr_group = {
	.attrs = pidv_attrs,
};

static int acpi_pidv_revision;

static int __init acpi_parse_pidv(struct acpi_table_header *table)
{
	struct acpi_table_pidv *pidv_tbl = (struct acpi_table_pidv *)table;

	acpi_pidv_revision = table->revision;

	if (sizeof(pidv_tbl) > table->length) {
		pr_warn("SPID: Invalid PIDV table size\n");
		return -EINVAL;
	}

	memcpy(&pidv, &(pidv_tbl->pidv), sizeof(struct platform_id));
	memcpy(&spid, &(pidv_tbl->pidv.ext_id_1),
	       sizeof(struct soft_platform_id));
	memcpy(intel_platform_ssn, &(pidv_tbl->pidv.part_number),
	       INTEL_PLATFORM_SSN_SIZE);
	intel_platform_ssn[INTEL_PLATFORM_SSN_SIZE] = '\0';

	pr_info("SPID updated according to ACPI Table:\n");
	pr_info("\tspid customer id : %04x\n"
			"\tspid vendor id : %04x\n"
			"\tspid manufacturer id : %04x\n"
			"\tspid platform family id : %04x\n"
			"\tspid product line id : %04x\n"
			"\tspid hardware id : %04x\n"
			"\tspid fru[4..0] : %02x %02x %02x %02x %02x\n"
			"\tspid fru[9..5] : %02x %02x %02x %02x %02x\n"
			"\tssn : %s\n",
			spid.customer_id,
			spid.vendor_id,
			spid.manufacturer_id,
			spid.platform_family_id,
			spid.product_line_id,
			spid.hardware_id,
			spid.fru[4], spid.fru[3], spid.fru[2], spid.fru[1],
			spid.fru[0], spid.fru[9], spid.fru[8], spid.fru[7],
			spid.fru[6], spid.fru[5],
			intel_platform_ssn);

	return 0;
}

static int __init acpi_spid_init(void)
{
	int ret;

	/* parse SPID table from firmware/bios */
	ret = acpi_table_parse(ACPI_SIG_PIDV, acpi_parse_pidv);
	if (ret) {
		pr_err("SPID: PIDV ACPI table not found\n");
		return -EINVAL;
	}

	/* create sysfs entries for soft platform id */
	spid_kobj = kobject_create_and_add("spid", NULL);
	if (!spid_kobj) {
		pr_err("SPID: ENOMEM for spid_kobj\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(spid_kobj, &spid_attr_group);
	if (ret) {
		pr_err("SPID: failed to create /sys/spid\n");
		goto err_sysfs_spid;
	}

	if (acpi_pidv_revision < 2) {
		pidv_kobj = kobject_create_and_add("pidv", firmware_kobj);
		if (!pidv_kobj) {
			pr_err("SPID: ENOMEM for pidv_kobj\n");
			ret = -ENOMEM;
			goto err_kobj_create_pidv;
		}

		ret = sysfs_create_group(pidv_kobj, &pidv_attr_group);
		if (ret) {
			pr_err("SPID: failed to create /sys/%s/pidv\n",
			       firmware_kobj->name);
			goto err_sysfs_pidv;
		}
	}

	/* Populate command line with SPID values */
	populate_spid_cmdline();

	return 0;

err_sysfs_pidv:
	kobject_put(pidv_kobj);
err_kobj_create_pidv:
	sysfs_remove_group(spid_kobj, &spid_attr_group);
err_sysfs_spid:
	kobject_put(spid_kobj);

	return ret;
}
arch_initcall(acpi_spid_init);
#endif

/**
 * Check if buffer contains printable character, from SPACE(0x20) to
 * TILDE (0x7E), until \0 or maxlen characters occur.
 * param char *str_buf buffer of characters to look for
 * param int maxlen max number of characters to look for
 * return true if valid, otherwise false
 * */
static bool chk_prt_validity(char *strbuf, int max_len)
{
	int idx = 0;
	while ((idx < max_len) && (strbuf[idx] != '\0')) {
		if ((strbuf[idx] < 0x20) || (strbuf[idx] > 0x7E))
			return false;
		idx++;
	}
	return true;
}

int __init sfi_handle_spid(struct sfi_table_header *table)
{
	struct sfi_table_oemb *oemb;
	int ret = 0;

	/* create sysfs entries for soft platform id */
	spid_kobj = kobject_create_and_add("spid", NULL);
	if (!spid_kobj) {
		pr_err("SPID: ENOMEM for spid_kobj\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(spid_kobj, &spid_attr_group);
	if (ret) {
		pr_err("SPID: failed to create /sys/spid\n");
		goto err_sfi_sysfs_spid;
	}

	oemb = (struct sfi_table_oemb *) table;
	if (!oemb) {
		pr_err("%s: fail to read MFD Validation SFI OEMB Layout\n",
			__func__);
		ret = -ENODEV;
		goto err_sfi_oemb_tbl;
	}

	memcpy(&spid, &oemb->spid, sizeof(struct soft_platform_id));

	if (oemb->header.len <
			(char *)oemb->ssn + INTEL_PLATFORM_SSN_SIZE -
			(char *)oemb) {
		pr_err("SFI OEMB does not contains SSN\n");
		intel_platform_ssn[0] = '\0';
	} else {
		if (!chk_prt_validity(oemb->ssn, INTEL_PLATFORM_SSN_SIZE)) {
			pr_err("SSN contains non printable character\n");
			intel_platform_ssn[0] = '\0';
		} else {
			memcpy(intel_platform_ssn, oemb->ssn,
				INTEL_PLATFORM_SSN_SIZE);
			intel_platform_ssn[INTEL_PLATFORM_SSN_SIZE] = '\0';
		}
	}

	/* Populate command line with SPID values */
	populate_spid_cmdline();

	return 0;

err_sfi_oemb_tbl:
	sysfs_remove_group(spid_kobj, &spid_attr_group);
err_sfi_sysfs_spid:
	kobject_put(spid_kobj);

	return ret;
}
