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
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/system_info.h>
#include <asm/bootinfo.h>

#include <mach/msm_smsm.h>
#include "mmi-unit-info.h"

#include <linux/of.h>
#include <linux/of_platform.h>

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

static char msm_hw[MSMHW_MAX_LEN+1];
static struct mmi_unit_info *mui;

void mmi_set_pureason(uint32_t val)
{
	if (mui) {
		mui->pureason = val;
		pr_debug("%s: Set modem PU reason value in SMEM to %d\n",
				__func__, mui->pureason);
	}
}

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

static struct of_device_id mmi_of_setup[] __initdata = {
	{ .compatible = "linux,seriallow", .data = &system_serial_low },
	{ .compatible = "linux,serialhigh", .data = &system_serial_high },
	{ .compatible = "linux,hwrev", .data = &system_rev },
	{ .compatible = "mmi,prod_id", .data = &prod_id },
	{ }
};

static void __init mmi_of_populate_setup(void)
{
	struct device_node *n = of_find_node_by_path("/chosen");
	struct of_device_id *tbl = mmi_of_setup;
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
	mmi_of_populate_setup();

	#define SMEM_KERNEL_RESERVE_SIZE 1024
	mui = (struct mmi_unit_info *) smem_alloc(SMEM_KERNEL_RESERVE,
		SMEM_KERNEL_RESERVE_SIZE);

	if (!mui) {
		pr_err("%s: failed to allocate mmi_unit_info in SMEM\n",
			__func__);
		return 1;
	}

	mui->version = MMI_UNIT_INFO_VER;
	mui->system_rev = system_rev;
	mui->system_serial_low = system_serial_low;
	mui->system_serial_high = system_serial_high;
	strlcpy(mui->machine, machine_desc->name, MACHINE_MAX_LEN);
	strlcpy(mui->barcode, serialno, BARCODE_MAX_LEN);
	strlcpy(mui->baseband, extended_baseband, BASEBAND_MAX_LEN);
	strlcpy(mui->carrier, carrier, CARRIER_MAX_LEN);
	strlcpy(mui->device, androidboot_device, DEVICE_MAX_LEN);
	mui->radio = androidboot_radio;
	mui->pureason = bi_powerup_reason();
	pr_debug("%s: Set modem PU reason value in SMEM to %d\n",
			__func__, mui->pureason);

	if (mui->version != MMI_UNIT_INFO_VER) {
		pr_err("%s: unexpected unit_info version %d in SMEM\n",
			__func__, mui->version);
	}

	pr_err("mmi_unit_info (SMEM) for modem: version = 0x%02x,"
		" device = '%s', radio = %d, system_rev = 0x%04x,"
		" system_serial = 0x%08x%08x, machine = '%s',"
		" barcode = '%s', baseband = '%s', carrier = '%s'\n",
		mui->version,
		mui->device, mui->radio, mui->system_rev,
		mui->system_serial_high, mui->system_serial_low,
		mui->machine, mui->barcode,
		mui->baseband, mui->carrier);

	return 0;
}

module_init(mmi_unit_info_init);
MODULE_DESCRIPTION("Motorola Mobility LLC. Unit Info");
MODULE_LICENSE("GPL v2");
