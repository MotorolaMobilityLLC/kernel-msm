/*
 *
 * Copyright (C) 2012 Motorola, Inc.
 * Copyright (c) 2013 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)     "%s: " fmt, __func__

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <mach/scm.h>

#define HOB_RAM_MISC_DEV_NAME "mmi,hob_ram"

struct hob_ram_misc_dev {
	struct miscdevice mdev;
	struct device *dev;
	struct mutex lock;
	int open_excl;
	struct resource *shob;
	struct resource *dhob;
};

struct hob_ram_misc_dev *hob_dev;

static int hob_valid_range(struct hob_ram_misc_dev *dev,
			unsigned long pgoff, unsigned long vsize)
{
	unsigned long dhob_size;
	unsigned long shob_size;

	dhob_size = dev->dhob->end - dev->dhob->start + 1;
	if ((dev->dhob->start >> PAGE_SHIFT) == pgoff && dhob_size == vsize)
		return 0;

	shob_size = dev->shob->end - dev->shob->start + 1;
	if ((dev->shob->start >> PAGE_SHIFT) == pgoff && shob_size == vsize)
		return 0;

	return -EINVAL;
}

static int hob_shared_ram_mmap(struct file *fp, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	int ret;

	/* Check if we are mapping the right HOB addresses */
	if (hob_valid_range(hob_dev, vma->vm_pgoff, vsize)) {
		dev_err(hob_dev->dev, "invalid address %lx and size %lx\n",
			vma->vm_pgoff << PAGE_SHIFT, vsize);
		return -EINVAL;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vsize, vma->vm_page_prot);
	if (ret < 0)
		dev_err(hob_dev->dev, "io_remap failed: %d\n", ret);

	return ret;
}

static int hob_shared_ram_open(struct inode *ip, struct file *fp)
{
	int ret = 0;

	mutex_lock(&hob_dev->lock);
	if (!hob_dev->open_excl)
		hob_dev->open_excl = 1;
	else
		ret = -EBUSY;
	mutex_unlock(&hob_dev->lock);

	return ret;
}

static int hob_shared_ram_release(struct inode *ip, struct file *fp)
{
	mutex_lock(&hob_dev->lock);
	hob_dev->open_excl = 0;
	mutex_unlock(&hob_dev->lock);

	return 0;
}

const struct file_operations hob_shared_ram_fops = {
	.owner = THIS_MODULE,
	.open = hob_shared_ram_open,
	.mmap = hob_shared_ram_mmap,
	.release = hob_shared_ram_release,
};

#define TZBSP_MEM_MPU_USAGE_MSS_AP_DYNAMIC_REGION	0x6

#define SCM_MP_LOCK_CMD_ID      0x1
#define SCM_MP_PROTECT          0x1

struct secure_memprot_cmd {
	u32	start;
	u32	size;
	u32	tag_id;
	u8	lock;
} __attribute__ ((__packed__));

static int hob_ram_do_scm_protect(phys_addr_t phy_base, unsigned long size)
{
	struct secure_memprot_cmd cmd;

	cmd.start = phy_base;
	cmd.size = size;
	cmd.tag_id = TZBSP_MEM_MPU_USAGE_MSS_AP_DYNAMIC_REGION;
	cmd.lock = SCM_MP_PROTECT;

	return scm_call(SCM_SVC_MP, SCM_MP_LOCK_CMD_ID, &cmd,
			sizeof(cmd), NULL, 0);
}

static int hob_ram_protect(struct hob_ram_misc_dev *hob_dev)
{
	int ret;
	if (!hob_dev->dhob || !hob_dev->shob) {
		pr_err("%s(): Invalid HOB resource: DHOB %p SHOB %p\n",
			__func__, hob_dev->dhob, hob_dev->shob);
		return -EINVAL;
	}
	if ((hob_dev->dhob->end + 1 == hob_dev->shob->start) ||
			(hob_dev->shob->end + 1 == hob_dev->dhob->start)) {
		ret = hob_ram_do_scm_protect(
			min(hob_dev->dhob->start, hob_dev->shob->start),
			hob_dev->dhob->end - hob_dev->dhob->start + 1 +
			hob_dev->shob->end - hob_dev->shob->start + 1);
	} else {
		ret = hob_ram_do_scm_protect(hob_dev->dhob->start,
			hob_dev->dhob->end - hob_dev->dhob->start + 1);
		if (!ret)
			ret = hob_ram_do_scm_protect(hob_dev->shob->start,
				hob_dev->shob->end - hob_dev->shob->start + 1);
	}
	return ret;
}

static int __devinit hob_ram_probe(struct platform_device *pdev)
{
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "no OF node supplied\n");
		return -EINVAL;
	}

	hob_dev = devm_kzalloc(&pdev->dev, sizeof(*hob_dev), GFP_KERNEL);
	if (!hob_dev) {
		dev_err(&pdev->dev, "hob device allocation failed\n");
		return -ENOMEM;
	}

	hob_dev->dhob = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!hob_dev->dhob) {
		dev_err(&pdev->dev, "couldn't get DHOB memory range\n");
		return -EINVAL;
	}

	hob_dev->shob = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!hob_dev->shob) {
		dev_err(&pdev->dev, "couldn't get SHOB memory range\n");
		return -EINVAL;
	}

	if (hob_ram_protect(hob_dev))
		dev_err(&pdev->dev, "couldn't protect DHOB/SHOB memory range\n");

	hob_dev->mdev.name = "mot_hob_ram";
	hob_dev->mdev.minor = MISC_DYNAMIC_MINOR;
	hob_dev->mdev.fops = &hob_shared_ram_fops;

	hob_dev->dev = &pdev->dev;

	ret = misc_register(&hob_dev->mdev);
	if (ret) {
		dev_err(&pdev->dev, "couldn't register misc: %d\n", ret);
		return ret;
	}

	mutex_init(&hob_dev->lock);

	return 0;
}

static int __devexit hob_ram_remove(struct platform_device *dev)
{
	misc_deregister(&hob_dev->mdev);
	mutex_destroy(&hob_dev->lock);

	return 0;
}

static struct of_device_id hob_ram_match_table[] = {
	{ .compatible = HOB_RAM_MISC_DEV_NAME },
	{ /* NULL */ },
};
EXPORT_COMPAT(HOB_RAM_MISC_DEV_NAME);


static struct platform_driver hob_driver = {
	.probe = hob_ram_probe,
	.remove = __devexit_p(hob_ram_remove),
	.driver = {
		.name = HOB_RAM_MISC_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = hob_ram_match_table,
	}
};

static int __init hob_shared_ram_init(void)
{
	return platform_driver_register(&hob_driver);
}

fs_initcall(hob_shared_ram_init);

MODULE_DESCRIPTION(HOB_RAM_MISC_DEV_NAME);
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" HOB_RAM_MISC_DEV_NAME);
