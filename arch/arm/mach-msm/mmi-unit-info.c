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

struct comp_value {
	char compatible[128];
	void *data;
};

static struct comp_value mmi_of_setup[] __initdata = {
	{ .compatible = "linux,seriallow", .data = &system_serial_low },
	{ .compatible = "linux,serialhigh", .data = &system_serial_high },
	{ .compatible = "linux,hwrev", .data = &system_rev },
	{ .compatible = "mmi,prod_id", .data = &prod_id },
	{ }
};

static void __init mmi_of_populate_setup(void)
{
	struct device_node *n = of_find_node_by_path("/chosen");
	struct comp_value *tbl = mmi_of_setup;
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
	return 0;
}

early_initcall(mmi_unit_info_init);
MODULE_DESCRIPTION("Motorola Mobility LLC. Unit Info");
MODULE_LICENSE("GPL v2");
