/*
 * intel_soc_debug.c - This driver provides utility debug api's
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/intel-mid.h>
#include <asm/intel_soc_debug.h>

/* This module currently only supports Intel Tangier
 * and Anniedale SOCs (CONFIG_INTEL_DEBUG_FEATURE will
 * only be set in i386_mrfl_defconfig and i386_moor_defconfig).
 * In addition, a platform check is done in soc_debug_init()
 * to make sure that this module is only used by appropriate
 * platforms.
 */
#define PGRR_BASE           0xff03a0bc
#define MAX_MODE_NUMBER     9
#define MAX_DEBUG_NUMBER    5

static struct dentry *dfs_entry;

enum pgrr_mode {
	manufacturing_mode = 0x0F,
	production_mode = 0x07,
	intel_production_mode = 0x04,
	oem_production_mode = 0x05,
	gfx_production_mode = 0x0E,
	end_user_mode = 0x0B,
	intel_end_user_mode = 0x08,
	rma_mode = 0x03,
	permanent_mode = 0x00
};

static struct debug_mode {
	enum pgrr_mode mode;
	u32 bitmask;
	char *name;
} asset_array[] = {
	{ manufacturing_mode,
	  DEBUG_FEATURE_PTI | DEBUG_FEATURE_RTIT | DEBUG_FEATURE_USB3DFX |
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "ManufacturingMode",
	},
	{ production_mode,
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "ProductionMode",
	},
	{ intel_production_mode,
	  DEBUG_FEATURE_PTI | DEBUG_FEATURE_RTIT | DEBUG_FEATURE_USB3DFX |
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "IntelProductionMode",
	},
	{ oem_production_mode,
	  DEBUG_FEATURE_PTI | DEBUG_FEATURE_RTIT | DEBUG_FEATURE_USB3DFX |
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "OemProductionMode",
	},
	{ gfx_production_mode,
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "GfxProductionMode",
	},
	{ intel_end_user_mode,
	  DEBUG_FEATURE_PTI | DEBUG_FEATURE_RTIT | DEBUG_FEATURE_USB3DFX |
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "IntelEndUserMode",
	},
	{ end_user_mode,
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "EndUserMode",
	},
	{ rma_mode,
	  DEBUG_FEATURE_PTI | DEBUG_FEATURE_RTIT | DEBUG_FEATURE_USB3DFX |
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "RmaMode",
	},
	{ permanent_mode,
	  DEBUG_FEATURE_SOCHAPS | DEBUG_FEATURE_LAKEMORE,
	  "PermanentMode",
	}
};

static int debug_mode_idx; /* index in asset_array */

static struct debug_feature {
	u32 bit;
	char *name;
} debug_feature_array[] = {
	{ DEBUG_FEATURE_PTI,
	  "PTI",
	},
	{ DEBUG_FEATURE_RTIT,
	  "RTIT",
	},
	{ DEBUG_FEATURE_LAKEMORE,
	  "LAKERMORE",
	},
	{ DEBUG_FEATURE_SOCHAPS,
	  "SOCHAPS",
	},
	{ DEBUG_FEATURE_USB3DFX,
	  "USB3DFX",
	},
};

int cpu_has_debug_feature(u32 bit)
{
	if (asset_array[debug_mode_idx].bitmask & bit)
		return 1;

	return  0;
}
EXPORT_SYMBOL(cpu_has_debug_feature);

static int show_debug_feature(struct seq_file *s, void *unused)
{
	int i = 0;

	if (debug_mode_idx >= 0 && (debug_mode_idx < MAX_MODE_NUMBER)) {
		seq_printf(s, "Profile: %s\n",
			   asset_array[debug_mode_idx].name);

		for (i = 0; i < MAX_DEBUG_NUMBER; i++)
			if (cpu_has_debug_feature(debug_feature_array[i].bit))
				seq_printf(s, "%s\n",
					   debug_feature_array[i].name);
	}

	return 0;
}

static int debug_feature_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_debug_feature, NULL);
}

static const struct file_operations debug_feature_ops = {
	.open		= debug_feature_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int __init soc_debug_init(void)
{
	u32 __iomem *pgrr;
	int i = 0;
	enum pgrr_mode soc_debug_setting = 0;

	if ((intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_TANGIER) &&
	    (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_ANNIEDALE))
		return -EINVAL;

	/* Read Policy Generator Result Register */
	pgrr = ioremap_nocache(PGRR_BASE, sizeof(u32));
	if (pgrr == NULL)
		return -EFAULT;

	pr_info("pgrr = %08x\n", *pgrr);
	soc_debug_setting = *pgrr & 0x0F;
	iounmap(pgrr);

	for (i = 0; i < MAX_MODE_NUMBER; i++)
		if (asset_array[i].mode == soc_debug_setting)
			break;

	if (i == MAX_MODE_NUMBER)
		return -EFAULT;

	debug_mode_idx = i;

	dfs_entry = debugfs_create_file("debug_feature", S_IFREG | S_IRUGO,
					NULL, NULL, &debug_feature_ops);

	return 0;
}
arch_initcall(soc_debug_init);

void __exit soc_debug_exit(void)
{
	debugfs_remove(dfs_entry);
}
module_exit(soc_debug_exit);
