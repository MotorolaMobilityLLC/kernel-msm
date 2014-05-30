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

#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <mach/msm_rpcrouter.h>

#define SHOB_RAM_ADDR 0x8FA80000
#define DHOB_RAM_ADDR 0x8FAC0000
#define SHOB_RAM_SIZE 0x40000
#define DHOB_RAM_SIZE 0x1000

#define VALID_HOB_ADDRESS_RANGE(off, size) \
	((((off) == (SHOB_RAM_ADDR >> PAGE_SHIFT)) && \
	  ((size) <= SHOB_RAM_SIZE)) || \
	 (((off) == (DHOB_RAM_ADDR >> PAGE_SHIFT)) && \
	  ((size) <= DHOB_RAM_SIZE)))

static int open_excl;
static spinlock_t hob_lock;

static int hob_shared_ram_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	int ret = -EINVAL;

	/* Check if we are mapping the right HOB addresses */
	if (!VALID_HOB_ADDRESS_RANGE(vma->vm_pgoff, vsize)) {
		/* Invalid Physical address or size return -EINVAL */
		pr_err("%s: failed on invalid physical address %lx and size %lx \n",
				__func__, (vma->vm_pgoff << PAGE_SHIFT), vsize);
		return ret;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vsize, vma->vm_page_prot);
	if (ret < 0)
		pr_err("%s: failed with return val %d\n", __func__, ret);
	return ret;
}

static int hob_shared_ram_open(struct inode *ip, struct file *fp)
{
	int ret = 0;

	spin_lock(&hob_lock);
	if (!open_excl)
		open_excl = 1;
	else
		ret = -EBUSY;
	spin_unlock(&hob_lock);

	return ret;
}

static int hob_shared_ram_release(struct inode *ip, struct file *fp)
{
	spin_lock(&hob_lock);
	open_excl = 0;
	spin_unlock(&hob_lock);

	return 0;
}

const struct file_operations hob_shared_ram_fops = {
	.owner = THIS_MODULE,
	.open = hob_shared_ram_open,
	.mmap = hob_shared_ram_mmap,
	.release = hob_shared_ram_release,
};

static struct miscdevice hob_shared_ram_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mot_hob_ram",
	.fops = &hob_shared_ram_fops,
};


static int __init hob_shared_ram_init(void)
{
	int ret = 0;

	open_excl = 0;
	spin_lock_init(&hob_lock);

	ret = misc_register(&hob_shared_ram_device);
	if (ret) {
		pr_err("%s: Unable to register misc device %d\n", __func__,
				MISC_DYNAMIC_MINOR);
	}

	return ret;
}

module_init(hob_shared_ram_init);
MODULE_DESCRIPTION("Motorola Mobility Inc. HOB RAM MMAP Client");
MODULE_LICENSE("GPL v2");
