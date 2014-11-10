/*
 * Copyright (C) 2013 Motorola Mobility LLC
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
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <soc/qcom/bootinfo.h>

#include <soc/qcom/smem.h>
#include "mmi-unit-info.h"

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#ifndef CONFIG_ARM64
#include <asm/mach/arch.h>
#endif

static u32 prod_id;

#define SERIALNO_MAX_LEN 64
static char serialno[SERIALNO_MAX_LEN];
int __init board_serialno_init(char *s)
{
	strlcpy(serialno, s, SERIALNO_MAX_LEN);
	return 1;
}
__setup("androidboot.serialno=", board_serialno_init);

static char carrier[CARRIER_MAX_LEN];
int __init board_carrier_init(char *s)
{
	strlcpy(carrier, s, SERIALNO_MAX_LEN);
	return 1;
}
__setup("androidboot.carrier=", board_carrier_init);

static char baseband[BASEBAND_MAX_LEN];
int __init board_baseband_init(char *s)
{
	strlcpy(baseband, s, SERIALNO_MAX_LEN);
	return 1;
}
__setup("androidboot.baseband=", board_baseband_init);

#define ANDROIDBOOT_DEVICE_MAX_LEN 32
static char androidboot_device[ANDROIDBOOT_DEVICE_MAX_LEN];
int __init setup_androidboot_device_init(char *s)
{
	strlcpy(androidboot_device, s, ANDROIDBOOT_DEVICE_MAX_LEN);
	return 1;
}
__setup("androidboot.device=", setup_androidboot_device_init);

static unsigned int androidboot_radio;
int __init setup_androidboot_radio_init(char *s)
{
	int retval = kstrtouint(s, 16, &androidboot_radio);

	if (retval < 0)
		return 0;

	return 1;
}
__setup("androidboot.radio=", setup_androidboot_radio_init);

static struct mmi_unit_info *mui;
void mmi_set_pureason(uint32_t val)
{
	if (mui) {
		mui->powerup_reason = val;
		pr_debug("%s: Set modem PU reason value in SMEM to %d\n",
				__func__, mui->powerup_reason);
	}
}

static char msm_hw[MSMHW_MAX_LEN+1];

void mach_cpuinfo_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Device\t\t: %s\n", androidboot_device);
	/* Zero is not a valid "Radio" value.      */
	/* Lack of "Radio" entry in cpuinfo means: */
	/*	look for radio in "Revision"       */
	if (androidboot_radio)
		seq_printf(m, "Radio\t\t: %x\n", androidboot_radio);

	seq_printf(m, "MSM Hardware\t: %s\n", msm_hw);
}

static char extended_baseband[BASEBAND_MAX_LEN+1] = "\0";

struct mmi_of_lookup {
	const char *compatible;
	unsigned int *data;
};

static struct mmi_of_lookup mmi_of_setup[] __initdata = {
	{ .compatible = "linux,seriallow", .data = &system_serial_low },
	{ .compatible = "linux,serialhigh", .data = &system_serial_high },
	{ .compatible = "linux,hwrev", .data = &system_rev },
	{ .compatible = "mmi,prod_id", .data = &prod_id },
	{ }
};

static void __init mmi_of_populate_setup(void)
{
	struct device_node *n = of_find_node_by_path("/chosen");
	struct mmi_of_lookup *tbl = mmi_of_setup;
	const char *baseband;
	const char *temp;

	while (tbl->data) {
		of_property_read_u32(n, tbl->compatible, tbl->data);
		tbl++;
	}

	if (0 == of_property_read_string(n, "mmi,baseband", &baseband))
		strlcpy(extended_baseband, baseband, sizeof(extended_baseband));

	if (0 == of_property_read_string(n, "mmi,msm_hw", &temp))
		strlcpy(msm_hw, temp, sizeof(msm_hw));

	of_node_put(n);
}

static int __init mmi_unit_info_init(void)
{
	int ret = 0;
	struct mmi_unit_info *mui_copy;

	mmi_of_populate_setup();

	#define SMEM_KERNEL_RESERVE_SIZE 1024
	mui_copy = kzalloc(SMEM_KERNEL_RESERVE_SIZE, GFP_KERNEL);
	if (!mui_copy) {
		pr_err("%s: failed to allocate space for mmi_unit_info\n",
			__func__);
		ret = 1;
		goto err;
	}

	mui_copy->version = MMI_UNIT_INFO_VER;
	mui_copy->system_rev = system_rev;
	mui_copy->system_serial_low = system_serial_low;
	mui_copy->system_serial_high = system_serial_high;
#ifndef CONFIG_ARM64
	strlcpy(mui_copy->machine, machine_desc->name, MACHINE_MAX_LEN);
#else
	strlcpy(mui_copy->machine, "", MACHINE_MAX_LEN);
#endif
	strlcpy(mui_copy->barcode, serialno, BARCODE_MAX_LEN);
	strlcpy(mui_copy->baseband, extended_baseband, BASEBAND_MAX_LEN);
	strlcpy(mui_copy->carrier, carrier, CARRIER_MAX_LEN);
	strlcpy(mui_copy->device, androidboot_device, DEVICE_MAX_LEN);
	mui_copy->radio = androidboot_radio;
	mui_copy->powerup_reason = bi_powerup_reason();

	pr_info("mmi_unit_info (SMEM) for modem: version = 0x%02x,"
		" device = '%s', radio = %d, system_rev = 0x%04x,"
		" system_serial = 0x%08x%08x, machine = '%s',"
		" barcode = '%s', baseband = '%s', carrier = '%s'"
		" pu_reason = 0x%08x\n",
		mui_copy->version,
		mui_copy->device, mui_copy->radio, mui_copy->system_rev,
		mui_copy->system_serial_high, mui_copy->system_serial_low,
		mui_copy->machine, mui_copy->barcode,
		mui_copy->baseband, mui_copy->carrier,
		mui_copy->powerup_reason);

	mui = (struct mmi_unit_info *) smem_alloc(SMEM_KERNEL_RESERVE,
		SMEM_KERNEL_RESERVE_SIZE, 0, SMEM_ANY_HOST_FLAG);

	if (!mui) {
		pr_err("%s: failed to allocate mmi_unit_info in SMEM\n",
			__func__);
		ret = 1;
		goto err_free;
	} else if (PTR_ERR(mui_copy) == -EPROBE_DEFER) {
		pr_err("%s: SMEM not yet initialized\n", __func__);
		ret = 1;
		goto err_free;
	}

	memcpy(mui, mui_copy, SMEM_KERNEL_RESERVE_SIZE);

err_free:
	kfree(mui_copy);
err:
	return ret;
}

module_init(mmi_unit_info_init);
MODULE_DESCRIPTION("Motorola Mobility LLC. Unit Info");
MODULE_LICENSE("GPL v2");
