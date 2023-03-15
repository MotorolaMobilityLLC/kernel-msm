// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/minidump.h>
#include "minidump_private.h"
#include "elf.h"

/* Global ToC registers offset, read-only for guest */
#define MD_GLOBAL_TOC_INIT          (0x00)
#define MD_GLOBAL_TOC_REVISION      (0x04)
#define MD_GLOBAL_TOC_ENABLE_STATUS (0x08)

/* 0x0C ~ 0x1C Reserved */

/* Subsystem ToC regiseters offset, read/write for guest */
#define MD_SS_TOC_INIT                 (0x20)
#define MD_SS_TOC_ENABLE_STATUS        (0x24)
#define MD_SS_TOC_ENCRYPTION_STATUS    (0x28)
#define MD_SS_TOC_ENCRYPTION_REQUIRED  (0x2C)
#define MD_SS_TOC_REGION_COUNT         (0x30)
#define MD_SS_TOC_REGION_BASE_LOW      (0x34)
#define MD_SS_TOC_REGION_BASE_HIGH     (0x38)

/* MMIO base address */
static void __iomem *md_mmio_base;

/**
 * md_table : Local Minidump toc holder
 * @md_ss_toc  : HLOS toc pointer
 * @md_gbl_toc : Global toc pointer
 * @md_regions : HLOS regions base pointer
 * @entry : array of HLOS regions requested
 */
struct md_table {
	u32			revision;
	struct md_ss_toc	*md_ss_toc;
	struct md_global_toc	*md_gbl_toc;
	struct md_ss_region	*md_regions;
	struct md_region	entry[MAX_NUM_ENTRIES];
};

/* Protect elfheader and smem table from deferred calls contention */
static DEFINE_SPINLOCK(mdt_lock);
static DEFINE_RWLOCK(mdt_remove_lock);
static struct md_table minidump_table;
static struct md_global_toc *md_global_toc;
static struct md_ss_toc *md_ss_toc;
static int first_removed_entry = INT_MAX;

static bool md_mmio_get_toc_init(void)
{
	if (md_mmio_base)
		return !!readl_relaxed(md_mmio_base + MD_GLOBAL_TOC_INIT);
	else
		return false;
}

static u32 md_mmio_get_revision(void)
{
	return readl_relaxed(md_mmio_base + MD_GLOBAL_TOC_REVISION);
}

static u32 md_mmio_get_enable_status(void)
{
	return readl_relaxed(md_mmio_base + MD_GLOBAL_TOC_ENABLE_STATUS);
}

static void md_mmio_set_ss_toc_init(u32 init)
{
	writel_relaxed(init, md_mmio_base + MD_SS_TOC_INIT);
}

static void md_mmio_set_ss_enable_status(bool enable)
{
	if (enable)
		writel_relaxed(MD_SS_ENABLED, md_mmio_base + MD_SS_TOC_ENABLE_STATUS);
	else
		writel_relaxed(MD_SS_DISABLED, md_mmio_base + MD_SS_TOC_ENABLE_STATUS);
}

static u32 md_mmio_get_ss_enable_status(void)
{
	return readl_relaxed(md_mmio_base + MD_SS_TOC_ENABLE_STATUS);
}

static void md_mmio_set_ss_encryption(bool required, u32 status)
{
	writel_relaxed(status, md_mmio_base + MD_SS_TOC_ENCRYPTION_STATUS);

	if (required)
		writel_relaxed(MD_SS_ENCR_REQ, md_mmio_base + MD_SS_TOC_ENCRYPTION_REQUIRED);
	else
		writel_relaxed(MD_SS_ENCR_NOTREQ, md_mmio_base + MD_SS_TOC_ENCRYPTION_REQUIRED);
}

static void md_mmio_set_ss_region_base(u64 base)
{
	writel_relaxed((u32)base, md_mmio_base + MD_SS_TOC_REGION_BASE_LOW);
	writel_relaxed((u32)(base >> 32), md_mmio_base + MD_SS_TOC_REGION_BASE_HIGH);
}

static void md_mmio_set_ss_region_count(u32 count)
{
	writel_relaxed(count, md_mmio_base + MD_SS_TOC_REGION_COUNT);
}

u32 md_mmio_get_ss_region_count(void)
{
	return readl_relaxed(md_mmio_base + MD_SS_TOC_REGION_COUNT);
}

static inline int md_mmio_entry_num(const struct md_region *entry)
{
	struct md_region *mdr;
	int i, regno = md_num_regions;

	for (i = 0; i < regno; i++) {
		mdr = &minidump_table.entry[i];
		if (!strcmp(mdr->name, entry->name))
			return i;
	}
	return -ENOENT;
}

static inline int md_mmio_region_num(const char *name, int *seqno)
{
	struct md_ss_region *mde = minidump_table.md_regions;
	int i, regno = md_mmio_get_ss_region_count();
	int ret = -EINVAL;

	for (i = 0; i < regno; i++, mde++) {
		if (!strcmp(mde->name, name)) {
			ret = i;
			if (mde->seq_num > *seqno)
				*seqno = mde->seq_num;
		}
	}
	return ret;
}

static int md_mmio_init_md_table(void)
{
	int ret = 0;

	md_global_toc = kzalloc(sizeof(struct md_global_toc), GFP_KERNEL);
	if (!md_global_toc)
		return -ENOMEM;

	/*Check global minidump support initialization */
	md_global_toc->md_toc_init = md_mmio_get_toc_init();
	if (!md_global_toc->md_toc_init) {
		pr_err("System Minidump TOC not initialized\n");
		return -ENODEV;
	}

	md_global_toc->md_revision = md_mmio_get_revision();
	md_global_toc->md_enable_status = md_mmio_get_enable_status();

	minidump_table.md_gbl_toc = md_global_toc;
	minidump_table.revision = md_global_toc->md_revision;
	md_ss_toc = &md_global_toc->md_ss_toc[MD_SS_HLOS_ID];

	md_mmio_set_ss_encryption(true, MD_SS_ENCR_NONE);

	minidump_table.md_ss_toc = md_ss_toc;
	minidump_table.md_regions = kzalloc((MAX_NUM_ENTRIES *
							sizeof(struct md_ss_region)), GFP_KERNEL);
	if (!minidump_table.md_regions)
		return -ENOMEM;

	md_mmio_set_ss_region_base(virt_to_phys(minidump_table.md_regions));

	md_mmio_set_ss_region_count(1);

	return ret;
}

static void md_mmio_add_ss_toc(const struct md_region *entry)
{
	struct md_ss_region *ss_mdr;
	struct md_region *mdr;
	int seq = 0, reg_cnt = md_mmio_get_ss_region_count();

	mdr = &minidump_table.entry[md_num_regions];
	strscpy(mdr->name, entry->name, sizeof(mdr->name));
	mdr->virt_addr = entry->virt_addr;
	mdr->phys_addr = entry->phys_addr;
	mdr->size = entry->size;
	mdr->id = entry->id;

	ss_mdr = &minidump_table.md_regions[reg_cnt];
	strscpy(ss_mdr->name, entry->name, sizeof(ss_mdr->name));
	ss_mdr->region_base_address = entry->phys_addr;
	ss_mdr->region_size = entry->size;
	if (md_mmio_region_num(entry->name, &seq) >= 0)
		ss_mdr->seq_num = seq + 1;

	ss_mdr->md_valid = MD_REGION_VALID;
	md_mmio_set_ss_region_count(reg_cnt + 1);
}

static int md_mmio_add_pending_entry(struct list_head *pending_list)
{
	unsigned int region_number;
	struct md_pending_region *pending_region, *tmp;
	unsigned long flags;

	/* Add pending entries to HLOS TOC */
	spin_lock_irqsave(&mdt_lock, flags);

	md_mmio_set_ss_toc_init(1);
	md_mmio_set_ss_enable_status(true);

	region_number = 0;
	list_for_each_entry_safe(pending_region, tmp, pending_list, list) {
		/* Add pending entry to minidump table and ss toc */
		minidump_table.entry[region_number] =
			pending_region->entry;
		md_mmio_add_ss_toc(&minidump_table.entry[region_number]);
		list_del(&pending_region->list);
		kfree(pending_region);
		region_number++;
	}
	spin_unlock_irqrestore(&mdt_lock, flags);

	return 0;
}

static void md_mmio_reg_kelfhdr_entry(unsigned int elfh_size)
{
	struct md_ss_region *mdreg;

	mdreg = &minidump_table.md_regions[0];
	strscpy(mdreg->name, "KELF_HDR", sizeof(mdreg->name));
	mdreg->region_base_address = virt_to_phys(minidump_elfheader.ehdr);
	mdreg->region_size = elfh_size;

	mdreg->md_valid = MD_REGION_VALID;
}

static elf_addr_t md_mmio_get_md_table(void)
{
	return (elf_addr_t)(&minidump_table);
}

static int md_mmio_remove_ss_toc(const struct md_region *entry)
{
	int ecount, rcount, entryno, rgno, seq = 0, ret;

	if (!minidump_table.md_ss_toc ||
	    (md_mmio_get_ss_enable_status() != MD_SS_ENABLED))
		return -EINVAL;

	entryno = md_mmio_entry_num(entry);
	if (entryno < 0) {
		printk_deferred("Not able to find the entry %s in table\n",
				entry->name);
		return entryno;
	}
	ecount = md_num_regions;
	rgno = md_mmio_region_num(entry->name, &seq);
	if (rgno < 0) {
		printk_deferred(
			"Not able to find the region %s (%d,%d) in table\n",
			entry->name, entryno, rgno);
		return -EINVAL;
	}
	rcount = md_mmio_get_ss_region_count();
	if (first_removed_entry > entryno)
		first_removed_entry = entryno;
	md_mmio_set_ss_toc_init(0);
	/* Remove entry from: entry list, ss region list and elf header */
	memmove(&minidump_table.entry[entryno],
		&minidump_table.entry[entryno + 1],
		((ecount - entryno - 1) * sizeof(struct md_region)));
	memset(&minidump_table.entry[ecount - 1], 0, sizeof(struct md_region));

	memmove(&minidump_table.md_regions[rgno],
		&minidump_table.md_regions[rgno + 1],
		((rcount - rgno - 1) * sizeof(struct md_ss_region)));
	memset(&minidump_table.md_regions[rcount - 1], 0,
	       sizeof(struct md_ss_region));

	ret = msm_minidump_clear_headers(entry);
	if (ret)
		return ret;

	md_mmio_set_ss_region_count(rcount - 1);
	md_mmio_set_ss_toc_init(1);
	md_num_regions--;

	return 0;
}

static int md_mmio_remove_region(const struct md_region *entry)
{
	int ret;
	unsigned long flags;

	if (!minidump_table.md_ss_toc ||
	     (md_mmio_get_ss_enable_status() != MD_SS_ENABLED))
		return -EINVAL;

	spin_lock_irqsave(&mdt_lock, flags);
	write_lock(&mdt_remove_lock);

	ret = md_mmio_remove_ss_toc(entry);

	write_unlock(&mdt_remove_lock);
	spin_unlock_irqrestore(&mdt_lock, flags);

	return ret;
}

static int md_mmio_add_region(const struct md_region *entry, struct list_head *pending_list)
{
	u32 toc_init;
	struct md_pending_region *pending_region;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mdt_lock, flags);

	if (md_num_regions >= MAX_NUM_ENTRIES) {
		printk_deferred("Maximum entries reached\n");
		ret = -ENOMEM;
		goto out;
	}

	toc_init = 0;
	if (minidump_table.md_ss_toc &&
		(md_mmio_get_ss_enable_status() == MD_SS_ENABLED)) {
		toc_init = 1;
		if (md_mmio_get_ss_region_count() >= MAX_NUM_ENTRIES) {
			printk_deferred("Maximum regions in minidump table reached\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	if (toc_init) {
		if (md_mmio_entry_num(entry) >= 0) {
			printk_deferred("Entry name already exist\n");
			ret = -EEXIST;
			goto out;
		}
		md_mmio_add_ss_toc(entry);
		md_add_elf_header(entry);
	} else {
		/* Local table not initialized
		 * add to pending list, need free after initialized
		 */
		pending_region = kzalloc(sizeof(*pending_region), GFP_ATOMIC);
		if (!pending_region) {
			ret = -ENOMEM;
			goto out;
		}
		pending_region->entry = *entry;
		list_add_tail(&pending_region->list, pending_list);
	}
	ret = md_num_regions;
	md_num_regions++;

out:
	spin_unlock_irqrestore(&mdt_lock, flags);

	return ret;
}

static int md_mmio_update_ss_toc(int regno, const struct md_region *entry)
{
	int ret = 0;
	struct md_region *mdr;
	struct md_ss_region *mdssr;

	if (regno >= first_removed_entry) {
		printk_deferred("Region:[%s] was moved\n", entry->name);
		return -EINVAL;
	}

	ret = md_mmio_entry_num(entry);
	if (ret < 0) {
		printk_deferred("Region:[%s] does not exist to update\n",
				entry->name);
		return ret;
	}

	mdr = &minidump_table.entry[regno];
	mdr->virt_addr = entry->virt_addr;
	mdr->phys_addr = entry->phys_addr;

	mdssr = &minidump_table.md_regions[regno + 1];
	mdssr->region_base_address = entry->phys_addr;
	return 0;
}

static int md_mmio_update_region(int regno, const struct md_region *entry)
{
	int ret = 0;
	unsigned long flags;

	read_lock_irqsave(&mdt_remove_lock, flags);

	ret = md_mmio_update_ss_toc(regno, entry);
	md_update_elf_header(regno, entry);

	read_unlock_irqrestore(&mdt_remove_lock, flags);

	return ret;
}

static int md_mmio_get_available_region(void)
{
	int res = -EBUSY;
	unsigned long flags;

	spin_lock_irqsave(&mdt_lock, flags);
	res = MAX_NUM_ENTRIES - md_num_regions;
	spin_unlock_irqrestore(&mdt_lock, flags);

	return res;
}

static bool md_mmio_md_enable(void)
{
	bool ret = false;
	unsigned long flags;

	spin_lock_irqsave(&mdt_lock, flags);
	if (minidump_table.md_ss_toc &&
		(md_mmio_get_enable_status() ==
		 MD_SS_ENABLED))
		ret = true;
	spin_unlock_irqrestore(&mdt_lock, flags);

	return ret;
}

static struct md_region md_mmio_get_region(char *name)
{
	struct md_region *mdr = NULL, tmp = {0};
	int i, regno;

	regno = md_num_regions;
	for (i = 0; i < regno; i++) {
		mdr = &minidump_table.entry[i];
		if (!strcmp(mdr->name, name)) {
			tmp = *mdr;
			goto out;
		}
	}

out:
	return tmp;
}

static const struct md_ops md_mmio_ops = {
	.init_md_table = md_mmio_init_md_table,
	.add_pending_entry = md_mmio_add_pending_entry,
	.reg_kelfhdr_entry = md_mmio_reg_kelfhdr_entry,
	.get_md_table = md_mmio_get_md_table,
	.remove_region = md_mmio_remove_region,
	.add_region = md_mmio_add_region,
	.update_region = md_mmio_update_region,
	.get_available_region = md_mmio_get_available_region,
	.md_enable = md_mmio_md_enable,
	.get_region = md_mmio_get_region,
};

static struct md_init_data md_mmio_init_data = {
	.ops = &md_mmio_ops,
};

static int minidump_mmio_driver_probe(struct platform_device *pdev)
{
	md_mmio_base = devm_of_iomap(&pdev->dev, pdev->dev.of_node, 0, NULL);
	if (!md_mmio_base) {
		dev_err(&pdev->dev, "Failed to map address\n");
		return -ENODEV;
	}

	return msm_minidump_driver_probe(&md_mmio_init_data);
}

static const struct of_device_id minidump_mmio_device_tbl[] = {
	{ .compatible = "qcom,virt-minidump", },
	{},
};

static struct platform_driver minidump_mmio_driver = {
	.probe = minidump_mmio_driver_probe,
	.driver = {
		.name = "qcom-minidump",
		.of_match_table = minidump_mmio_device_tbl,
	},
};

static int __init minidump_mmio_init(void)
{
	return platform_driver_register(&minidump_mmio_driver);
}

subsys_initcall(minidump_mmio_init);

static void __exit minidump_mmio_exit(void)
{
	platform_driver_unregister(&minidump_mmio_driver);
}

module_exit(minidump_mmio_exit);

MODULE_DESCRIPTION("QTI minidump MMIO driver");
MODULE_LICENSE("GPL v2");
