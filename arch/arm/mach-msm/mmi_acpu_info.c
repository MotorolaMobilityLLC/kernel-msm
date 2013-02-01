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
#include <linux/proc_fs.h>
#include <linux/apanic_mmc.h>
#include <linux/persistent_ram.h>
#include <asm/io.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>


/* PTE EFUSE register. */
#define QFPROM_PTE_EFUSE_ADDR	(MSM_QFPROM_BASE + 0x00C0)

static int mmi_msm8960_acpu_proc_read(char *buf, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int len = snprintf(buf, 2, "%1x", (int)data);
	*eof = 1;
	return len;
}

static void __init mmi_msm8960_get_acpu_info(void)
{
	uint32_t pte_efuse, pvs;
	struct proc_dir_entry *proc;
	char acpu_type[32];

	pte_efuse = readl_relaxed(QFPROM_PTE_EFUSE_ADDR);

	pvs = (pte_efuse >> 10) & 0x7;
	if (pvs == 0x7)
		pvs = (pte_efuse >> 13) & 0x7;

	if (pvs == 0x7)
		snprintf(acpu_type, 30, "ACPU PVS: Defaulting to 0\n");
	else
		snprintf(acpu_type, 30, "ACPU PVS: %d\n", pvs);

	proc = create_proc_read_entry("cpu/msm_acpu_pvs",
			(S_IFREG | S_IRUGO), NULL,
			mmi_msm8960_acpu_proc_read, (void *)pvs);
	if (!proc)
		pr_err("Failed to create /proc/cpu/msm_acpu_pvs.\n");
	else
		proc->size = 1;

	apanic_mmc_annotate(acpu_type);
	persistent_ram_ext_oldbuf_print(acpu_type);
}

static int __init init_mmi_acpu_info(void)
{
	if (soc_class_is_msm8960())
		mmi_msm8960_get_acpu_info();
	return 0;
}
module_init(init_mmi_acpu_info);
MODULE_DESCRIPTION("Motorola Mobility LLC. ACPU Info");
MODULE_LICENSE("GPL v2");
