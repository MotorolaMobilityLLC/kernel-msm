/* Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_mdp_hwio.h"
#include "mdss_debug.h"

#define DEFAULT_BASE_REG_CNT 0x100
#define ROW_BYTES 16
#define GROUP_BYTES 4
#define MAX_VSYNC_COUNT 0xFFFFFFF

struct mdss_debug_dump_info {
	char name[32];
	char *dump_addr;
	u32 len;
	u8 row_bytes;
	u8 grp_bytes;
	struct mdss_debug_dump_info *next;
};

static struct mdss_debug_dump_info *reg_dump_info;
static u32 reg_dump_lines;
static u32 reg_dump_bases;
static u32 dbg_bus_dump_lines;
static DEFINE_MUTEX(reg_dump_info_mutex);

static int mdss_debug_base_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static int mdss_debug_base_release(struct inode *inode, struct file *file)
{
	struct mdss_debug_base *dbg = file->private_data;
	if (dbg && dbg->buf) {
		kfree(dbg->buf);
		dbg->buf_len = 0;
		dbg->buf = NULL;
	}
	return 0;
}

static ssize_t mdss_debug_base_offset_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mdss_debug_base *dbg = file->private_data;
	u32 off = 0;
	u32 cnt = DEFAULT_BASE_REG_CNT;
	char buf[24];

	if (!dbg)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	sscanf(buf, "%5x %x", &off, &cnt);

	if (off > dbg->max_offset)
		return -EINVAL;

	if (cnt > (dbg->max_offset - off))
		cnt = dbg->max_offset - off;

	dbg->off = off;
	dbg->cnt = cnt;

	pr_debug("offset=%x cnt=%x\n", off, cnt);

	return count;
}

static ssize_t mdss_debug_base_offset_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct mdss_debug_base *dbg = file->private_data;
	int len = 0;
	char buf[24];

	if (!dbg)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf), "0x%08zx %zx\n", dbg->off, dbg->cnt);
	if (len < 0)
		return 0;

	if (copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;	/* increase offset */

	return len;
}

static ssize_t mdss_debug_base_reg_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mdss_debug_base *dbg = file->private_data;
	struct mdss_data_type *mdata = mdss_res;
	size_t off;
	u32 data, cnt;
	char buf[24];

	if (!dbg || !mdata)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	cnt = sscanf(buf, "%zx %x", &off, &data);

	if (cnt < 2)
		return -EFAULT;

	if (off >= dbg->max_offset)
		return -EFAULT;

	if (mdata->debug_inf.debug_enable_clock)
		mdata->debug_inf.debug_enable_clock(1);

	writel_relaxed(data, dbg->base + off);

	if (mdata->debug_inf.debug_enable_clock)
		mdata->debug_inf.debug_enable_clock(0);

	pr_debug("addr=%zx data=%x\n", off, data);

	return count;
}

static ssize_t mdss_debug_base_reg_read(struct file *file,
			char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mdss_debug_base *dbg = file->private_data;
	struct mdss_data_type *mdata = mdss_res;
	size_t len;

	if (!dbg || !mdata) {
		pr_err("invalid handle\n");
		return -ENODEV;
	}

	if (!dbg->buf) {
		char dump_buf[64];
		char *ptr;
		int cnt, tot;

		dbg->buf_len = sizeof(dump_buf) *
			DIV_ROUND_UP(dbg->cnt, ROW_BYTES);
		dbg->buf = kzalloc(dbg->buf_len, GFP_KERNEL);

		if (!dbg->buf) {
			pr_err("not enough memory to hold reg dump\n");
			return -ENOMEM;
		}

		ptr = dbg->base + dbg->off;
		tot = 0;

		if (mdata->debug_inf.debug_enable_clock)
			mdata->debug_inf.debug_enable_clock(1);

		for (cnt = dbg->cnt; cnt > 0; cnt -= ROW_BYTES) {
			hex_dump_to_buffer(ptr, min(cnt, ROW_BYTES),
					   ROW_BYTES, GROUP_BYTES, dump_buf,
					   sizeof(dump_buf), false);
			len = scnprintf(dbg->buf + tot, dbg->buf_len - tot,
					"0x%08x: %s\n",
					((int) (unsigned long) ptr) -
					((int) (unsigned long) dbg->base),
					dump_buf);

			ptr += ROW_BYTES;
			tot += len;
			if (tot >= dbg->buf_len)
				break;
		}
		if (mdata->debug_inf.debug_enable_clock)
			mdata->debug_inf.debug_enable_clock(0);

		dbg->buf_len = tot;
	}

	if (*ppos >= dbg->buf_len)
		return 0; /* done reading */

	len = min(count, dbg->buf_len - (size_t) *ppos);
	if (copy_to_user(user_buf, dbg->buf + *ppos, len)) {
		pr_err("failed to copy to user\n");
		return -EFAULT;
	}

	*ppos += len; /* increase offset */

	return len;
}

static const struct file_operations mdss_off_fops = {
	.open = mdss_debug_base_open,
	.release = mdss_debug_base_release,
	.read = mdss_debug_base_offset_read,
	.write = mdss_debug_base_offset_write,
};

static const struct file_operations mdss_reg_fops = {
	.open = mdss_debug_base_open,
	.release = mdss_debug_base_release,
	.read = mdss_debug_base_reg_read,
	.write = mdss_debug_base_reg_write,
};

int mdss_debug_register_base(const char *name, void __iomem *base,
			     size_t max_offset)
{
	struct mdss_data_type *mdata = mdss_res;
	struct mdss_debug_data *mdd;
	struct mdss_debug_base *dbg;
	struct dentry *ent_off, *ent_reg;
	char dn[80] = "";
	int prefix_len = 0;

	if (!mdata || !mdata->debug_inf.debug_data)
		return -ENODEV;

	mdd = mdata->debug_inf.debug_data;

	dbg = kzalloc(sizeof(*dbg), GFP_KERNEL);
	if (!dbg)
		return -ENOMEM;

	if (name)
		strlcpy(dbg->name, name, sizeof(dbg->name));
	dbg->base = base;
	dbg->max_offset = max_offset;
	dbg->off = 0;
	dbg->cnt = DEFAULT_BASE_REG_CNT;

	if (name && strcmp(name, "mdp"))
		prefix_len = snprintf(dn, sizeof(dn), "%s_", name);

	strlcpy(dn + prefix_len, "off", sizeof(dn) - prefix_len);
	ent_off = debugfs_create_file(dn, 0644, mdd->root, dbg, &mdss_off_fops);
	if (IS_ERR_OR_NULL(ent_off)) {
		pr_err("debugfs_create_file: offset fail\n");
		goto off_fail;
	}

	strlcpy(dn + prefix_len, "reg", sizeof(dn) - prefix_len);
	ent_reg = debugfs_create_file(dn, 0644, mdd->root, dbg, &mdss_reg_fops);
	if (IS_ERR_OR_NULL(ent_reg)) {
		pr_err("debugfs_create_file: reg fail\n");
		goto reg_fail;
	}

	list_add(&dbg->head, &mdd->base_list);

	return 0;
reg_fail:
	debugfs_remove(ent_off);
off_fail:
	kfree(dbg);
	return -ENODEV;
}


static int mdss_debug_stat_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static int mdss_debug_stat_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mdss_debug_stat_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	struct mdss_data_type *mdata = file->private_data;
	int len, tot;
	char bp[512];

	if (*ppos)
		return 0;	/* the end */

	len = sizeof(bp);

	tot = scnprintf(bp, len, "\nmdp:\n");

	if (mdata->debug_inf.debug_dump_stats)
		tot += mdata->debug_inf.debug_dump_stats(mdata,
						bp + tot, len - tot);


	tot += scnprintf(bp + tot, len - tot, "\n");

	if (copy_to_user(buff, bp, tot))
		return -EFAULT;

	*ppos += tot;	/* increase offset */

	return tot;
}

static const struct file_operations mdss_stat_fops = {
	.open = mdss_debug_stat_open,
	.release = mdss_debug_stat_release,
	.read = mdss_debug_stat_read,
};

static int mdss_debug_reg_dump_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static char *reg_dump_buf;
static u32 reg_dump_buf_size;
static struct ion_client *dbg_iclient;
static struct ion_handle *dbg_ihandle;

static ssize_t mdss_debug_reg_dump_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	size_t len;

	if (!dbg_iclient) {
		pr_err("ion_debug_client is not valid. Cannot dump\n");
		return -EPERM;
	}

	if (!reg_dump_info) {
		pr_err("nothing to dump\n");
		return -EINVAL;
	}

	if (!reg_dump_buf) {
		char dump_buf[64];
		char *ptr;
		int cnt, tot;
		struct mdss_debug_dump_info *iterator = reg_dump_info;

		mutex_lock(&reg_dump_info_mutex);

		reg_dump_buf_size = (reg_dump_lines + reg_dump_bases +
				     dbg_bus_dump_lines) * 64;

		dbg_ihandle = ion_alloc(dbg_iclient, reg_dump_buf_size, SZ_4K,
				ION_HEAP(ION_SYSTEM_HEAP_ID), 0);
		if (IS_ERR_OR_NULL(dbg_ihandle)) {
			pr_err("unable to alloc debug mem from ion - %ld\n",
					PTR_ERR(dbg_ihandle));
			mutex_unlock(&reg_dump_info_mutex);
			return PTR_ERR(dbg_ihandle);
		}

		reg_dump_buf = (char *)ion_map_kernel(dbg_iclient, dbg_ihandle);
		if (IS_ERR_OR_NULL(reg_dump_buf)) {
			pr_err("ION memory mapping failed - %ld\n",
				PTR_ERR(reg_dump_buf));
			ion_free(dbg_iclient, dbg_ihandle);
			mutex_unlock(&reg_dump_info_mutex);
			return PTR_ERR(reg_dump_buf);
		}

		tot = 0;
		for (;iterator;) {
			int row_bytes = iterator->row_bytes;
			int grp_bytes = iterator->grp_bytes;

			ptr = iterator->dump_addr;
			len = snprintf(reg_dump_buf + tot, 64,
				"******** %s DUMP ********\n", iterator->name);
			tot += len;
			for (cnt = iterator->len; cnt > 0; cnt -= row_bytes) {

				hex_dump_to_buffer(ptr, min(cnt, row_bytes),
					row_bytes, grp_bytes, dump_buf,
					sizeof(dump_buf), false);

				if (row_bytes == 32)
					len = scnprintf(reg_dump_buf + tot,
						reg_dump_buf_size - tot, "%s\n",
						dump_buf);
				else
					len = scnprintf(reg_dump_buf + tot,
						reg_dump_buf_size - tot, "0x%08x: %s\n",
						(unsigned int)ptr, dump_buf);

				ptr += row_bytes;
				tot += len;
				if (tot >= reg_dump_buf_size)
					break;
			}
			iterator = iterator->next;
		}
		reg_dump_buf_size = tot;

		mutex_unlock(&reg_dump_info_mutex);
	}

	if (*ppos >= reg_dump_buf_size)
		return 0; /* done reading */

	len = min(count, reg_dump_buf_size - (size_t) *ppos);
	if (copy_to_user(user_buf, reg_dump_buf + *ppos, len)) {
		pr_err("failed to copy to user\n");
		return -EFAULT;
	}

	*ppos += len; /* increase offset */

	return len;
}

static int mdss_debug_reg_dump_release(struct inode *inode, struct file *file)
{
	if (reg_dump_buf) {
		ion_unmap_kernel(dbg_iclient, dbg_ihandle);
		ion_free(dbg_iclient, dbg_ihandle);
		dbg_ihandle = NULL;
		reg_dump_buf = NULL;
		reg_dump_buf_size = 0;
	}
	return 0;
}

static const struct file_operations mdss_reg_dump_fops = {
	.open = mdss_debug_reg_dump_open,
	.read = mdss_debug_reg_dump_read,
	.release = mdss_debug_reg_dump_release,
};

static ssize_t mdss_debug_factor_write(struct file *file,
		    const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mdss_fudge_factor *factor  = file->private_data;
	u32 numer = factor->numer;
	u32 denom = factor->denom;
	char buf[32];

	if (!factor)
		return -ENODEV;

	if (count >= sizeof(buf))
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	buf[count] = 0;	/* end of string */

	if (strnchr(buf, count, '/')) {
		/* Parsing buf as fraction */
		if (sscanf(buf, "%d/%d", &numer, &denom) != 2)
			return -EFAULT;
	} else {
		/* Parsing buf as percentage */
		if (sscanf(buf, "%d", &numer) != 1)
			return -EFAULT;
		denom = 100;
	}

	if (numer && denom) {
		factor->numer = numer;
		factor->denom = denom;
	}

	pr_debug("numer=%d  denom=%d\n", numer, denom);

	return count;
}

static ssize_t mdss_debug_factor_read(struct file *file,
			char __user *buff, size_t count, loff_t *ppos)
{
	struct mdss_fudge_factor *factor = file->private_data;
	int len = 0;
	char buf[32];

	if (!factor)
		return -ENODEV;

	if (*ppos)
		return 0;	/* the end */

	len = snprintf(buf, sizeof(buf), "%d/%d\n",
			factor->numer, factor->denom);
	if (len < 0)
		return 0;

	if (copy_to_user(buff, buf, len))
		return -EFAULT;

	*ppos += len;	/* increase offset */

	return len;
}

static const struct file_operations mdss_factor_fops = {
	.open = simple_open,
	.read = mdss_debug_factor_read,
	.write = mdss_debug_factor_write,
};

static int mdss_debugfs_cleanup(struct mdss_debug_data *mdd)
{
	struct mdss_debug_base *base, *tmp;

	if (!mdd)
		return 0;

	list_for_each_entry_safe(base, tmp, &mdd->base_list, head) {
		list_del(&base->head);
		kfree(base);
	}

	if (mdd->root)
		debugfs_remove_recursive(mdd->root);

	kfree(mdd);

	return 0;
}

static int mdss_debugfs_perf_init(struct mdss_debug_data *mdd,
			struct mdss_data_type *mdata) {
	debugfs_create_u32("min_mdp_clk", 0644, mdd->perf,
		(u32 *)&mdata->perf_tune.min_mdp_clk);

	debugfs_create_u64("min_bus_vote", 0644, mdd->perf,
		(u64 *)&mdata->perf_tune.min_bus_vote);

	debugfs_create_bool("enable_bw_release", 0644, mdd->perf,
		(u32 *)&mdata->enable_bw_release);

	debugfs_create_bool("enable_rotator_bw_release", 0644, mdd->perf,
		(u32 *)&mdata->enable_rotator_bw_release);

	debugfs_create_u64("min_uhd_bus_vote", 0644, mdd->perf,
		(u64 *)&mdata->perf_tune.min_uhd_bus_vote);

	debugfs_create_u64("min_qhd_bus_vote", 0644, mdd->perf,
		(u64 *)&mdata->perf_tune.min_qhd_bus_vote);

	debugfs_create_file("ab_factor", 0644, mdd->perf,
		&mdata->ab_factor, &mdss_factor_fops);

	debugfs_create_file("ib_factor", 0644, mdd->perf,
		&mdata->ib_factor, &mdss_factor_fops);

	debugfs_create_file("ib_factor_overlap", 0644, mdd->perf,
		&mdata->ib_factor_overlap, &mdss_factor_fops);

	debugfs_create_file("clk_factor", 0644, mdd->perf,
		&mdata->clk_factor, &mdss_factor_fops);

	debugfs_create_u32("threshold_low", 0644, mdd->perf,
		(u32 *)&mdata->max_bw_low);

	debugfs_create_u32("threshold_high", 0644, mdd->perf,
		(u32 *)&mdata->max_bw_high);

	debugfs_create_u32("threshold_pipe", 0644, mdd->perf,
		(u32 *)&mdata->max_bw_per_pipe);

	return 0;
}

int mdss_debugfs_init(struct mdss_data_type *mdata)
{
	struct mdss_debug_data *mdd;

	if (mdata->debug_inf.debug_data) {
		pr_warn("mdss debugfs already initialized\n");
		return -EBUSY;
	}

	mdd = kzalloc(sizeof(*mdd), GFP_KERNEL);
	if (!mdd) {
		pr_err("no memory to create mdss debug data\n");
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&mdd->base_list);

	mdd->root = debugfs_create_dir("mdp", NULL);
	if (IS_ERR_OR_NULL(mdd->root)) {
		pr_err("debugfs_create_dir for mdp failed, error %ld\n",
		       PTR_ERR(mdd->root));
		goto err;
	}
	debugfs_create_file("stat", 0644, mdd->root, mdata, &mdss_stat_fops);
	debugfs_create_file("reg_dump", 0644, mdd->root, mdd, &mdss_reg_dump_fops);
	debugfs_create_bool("serialize_wait4pp", 0644, mdd->root,
		(u32 *)&mdata->serialize_wait4pp);

	mdd->perf = debugfs_create_dir("perf", mdd->root);
	if (IS_ERR_OR_NULL(mdd->perf)) {
		pr_err("debugfs_create_dir perf fail, error %ld\n",
			PTR_ERR(mdd->perf));
		goto err;
	}

	mdss_debugfs_perf_init(mdd, mdata);

	if (mdss_create_xlog_debug(mdd))
		goto err;

	mdata->debug_inf.debug_data = mdd;

	dbg_iclient = msm_ion_client_create(-1 , "mdss_debug_iclient");
	if (IS_ERR_OR_NULL(dbg_iclient)) {
		pr_err("Err:client not created, val %d\n",
			PTR_RET(dbg_iclient));
		dbg_iclient = NULL;
	}

	return 0;

err:
	mdss_debugfs_cleanup(mdd);
	return -ENODEV;
}

int mdss_debugfs_remove(struct mdss_data_type *mdata)
{
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;

	mdss_debugfs_cleanup(mdd);
	mdata->debug_inf.debug_data = NULL;

	return 0;
}

void mdss_dump_reg(const char *name, char __iomem *base, int len, bool dump_in_memory)
{
	char *addr;
	u32 x0, x4, x8, xc;
	int i;
	u32 *dump_addr;

	addr = base;
	if (len % 16)
		len += 16;
	len /= 16;

	if (dump_in_memory) {
		struct mdss_debug_dump_info *dump_info =
			kzalloc(sizeof(struct mdss_debug_dump_info), GFP_KERNEL);
		if (!dump_info) {
			pr_err("failed to allocate memory for reg_dump_info\n");
			return;
		}

		/* 16Byte for x0 + x4 +x8 +xc */
		dump_addr = kzalloc(len * 16, GFP_KERNEL);
		if (!dump_addr) {
			pr_err("failed to allocate register dump memory\n");
			return;
		}

		mutex_lock(&reg_dump_info_mutex);

		strlcpy(dump_info->name, name, sizeof(dump_info->name));
		dump_info->dump_addr = (char *)dump_addr;
		dump_info->len = len * 16;
		dump_info->row_bytes = ROW_BYTES;
		dump_info->grp_bytes = GROUP_BYTES;

		reg_dump_lines += len;
		reg_dump_bases++;
		if (!reg_dump_info) {
			reg_dump_info = dump_info;
		} else {
			struct mdss_debug_dump_info *iterator = reg_dump_info;

			for (;iterator && iterator->next;)
				iterator = iterator->next;
			iterator->next = dump_info;
		}
		pr_info("start_address:%p end_address:%p\n", dump_addr,
			dump_addr + dump_info->len);

		mutex_unlock(&reg_dump_info_mutex);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	for (i = 0; i < len; i++) {
		x0 = readl_relaxed(addr+0x0);
		x4 = readl_relaxed(addr+0x4);
		x8 = readl_relaxed(addr+0x8);
		xc = readl_relaxed(addr+0xc);
		if (!dump_in_memory) {
			pr_info("%p : %08x %08x %08x %08x\n", addr, x0, x4, x8, xc);
		} else {
			dump_addr[i*4] = x0;
			dump_addr[i*4 + 1] = x4;
			dump_addr[i*4 + 2] = x8;
			dump_addr[i*4 + 3] = xc;
		}

		addr += 16;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
}

#define MAX_DBG_BUS_TEST_POINTS 4
enum mdss_debug_bus_type {
	MDSS_UP_DBG_BUS,
	MDSS_LP_DBG_BUS,
	MDSS_MAX_DBG_BUS,
};

struct mdss_debug_bus_data {
	u8 bus_type;
	u8 block_id;
	/* 0 = don't collect, 1 = collect */
	u32 test_point[MAX_DBG_BUS_TEST_POINTS];
};

static struct mdss_debug_bus_data dbg_bus_list[] = {
	{MDSS_UP_DBG_BUS,   4, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  24, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  42, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  88, {1, 1, 0, 0}},

	{MDSS_UP_DBG_BUS,  12, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  34, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  52, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  96, {1, 1, 0, 0}},

	{MDSS_UP_DBG_BUS,   5, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  25, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  43, {1, 1, 0, 0}},
	{MDSS_UP_DBG_BUS,  89, {1, 1, 0, 0}},

	{MDSS_UP_DBG_BUS,   6, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  26, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  44, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  90, {1, 0, 0, 0}},

	{MDSS_UP_DBG_BUS,   7, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  27, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  45, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  91, {1, 1, 1, 0}},

	{MDSS_UP_DBG_BUS,  13, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  35, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  53, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  97, {1, 1, 1, 0}},

	{MDSS_UP_DBG_BUS,  58, {1, 1, 1, 0}},
	{MDSS_UP_DBG_BUS,  65, {1, 1, 1, 0}},

	{MDSS_LP_DBG_BUS,  14, {1, 1, 1, 0}},
	{MDSS_LP_DBG_BUS,  20, {1, 1, 1, 0}},
	{MDSS_LP_DBG_BUS,  26, {1, 1, 1, 0}},
	{MDSS_LP_DBG_BUS,   4, {1, 1, 1, 0}},

	{MDSS_LP_DBG_BUS,  18, {0, 1, 0, 0}},
	{MDSS_LP_DBG_BUS,  24, {0, 1, 0, 0}},
	{MDSS_LP_DBG_BUS,  30, {0, 1, 0, 0}},
	{MDSS_LP_DBG_BUS,   8, {0, 1, 0, 0}},

	{MDSS_LP_DBG_BUS,  31, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  33, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  35, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  43, {1, 0, 0, 0}},

	{MDSS_LP_DBG_BUS,  32, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  34, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  36, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  42, {1, 0, 0, 0}},

	{MDSS_UP_DBG_BUS,   0, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,   8, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  20, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  30, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  38, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  48, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  54, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  61, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  84, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS,  92, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS, 111, {0, 0, 0, 1}},
	{MDSS_UP_DBG_BUS, 118, {0, 0, 0, 1}},

	{MDSS_UP_DBG_BUS,  29, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  37, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  47, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  60, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS,  67, {1, 0, 0, 0}},
	{MDSS_UP_DBG_BUS, 107, {1, 0, 0, 0}},

	{MDSS_LP_DBG_BUS,  63, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  64, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  65, {1, 0, 0, 0}},
	{MDSS_LP_DBG_BUS,  75, {1, 0, 0, 0}},
};

void mdss_dump_debug_bus(bool dump_in_memory)
{
	u32 *dump_addr;
	u32 dump_cnt = 0;
	int i, j;
	int list_items = sizeof(dbg_bus_list)/sizeof(struct mdss_debug_bus_data);
	int size = list_items * ALIGN(sizeof(struct mdss_debug_bus_data), 32);
	struct mdss_data_type *mdata = mdss_res;

	if (!mdata || !mdata->mdp_base)
		return;

	if (dump_in_memory) {
		struct mdss_debug_dump_info *dump_info =
			kzalloc(sizeof(struct mdss_debug_dump_info), GFP_KERNEL);
		if (!dump_info) {
			pr_err("failed to allocate memory for reg_dump_info\n");
			return;
		}

		/* 16Byte for x0 + x4 +x8 +xc */
		dump_addr = kzalloc(size, GFP_KERNEL);
		if (!dump_addr) {
			pr_err("failed to allocate register dump memory\n");
			return;
		}

		mutex_lock(&reg_dump_info_mutex);

		strlcpy(dump_info->name, "debug bus", sizeof(dump_info->name));
		dump_info->dump_addr = (char *)dump_addr;
		dump_info->len = size;
		dump_info->row_bytes = 32;
		dump_info->grp_bytes = 4;

		dbg_bus_dump_lines += list_items;
		if (!reg_dump_info) {
			reg_dump_info = dump_info;
		} else {
			struct mdss_debug_dump_info *iterator = reg_dump_info;

			for (;iterator && iterator->next;)
				iterator = iterator->next;
			iterator->next = dump_info;
		}
		pr_info("debug bus dump start_address:%p end_address:%p\n",
			dump_addr, dump_addr + dump_info->len);

		mutex_unlock(&reg_dump_info_mutex);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	pr_debug("number of dbg bus items %d\n", list_items);
	for (i = 0; i < list_items; i++) {
		u32 tp_val[MAX_DBG_BUS_TEST_POINTS];
		u32 ctrl_reg_offset, status_reg_offset;
		struct mdss_debug_bus_data *item = &dbg_bus_list[i];

		/* turn off all debugbus first */
		writel_relaxed(0, mdata->mdp_base + MDSS_MDP_SMP_DEBUGBUS_CTRL);
		writel_relaxed(0, mdata->mdp_base + MDSS_MDP_UP_DEBUGBUS_CTRL);
		writel_relaxed(0, mdata->mdp_base + MDSS_MDP_LP_DEBUGBUS_CTRL);

		if (item->bus_type == MDSS_UP_DBG_BUS) {
			ctrl_reg_offset = MDSS_MDP_UP_DEBUGBUS_CTRL;
			status_reg_offset = MDSS_MDP_UP_DEBUGBUS_STATUS;
		} else if (item->bus_type == MDSS_LP_DBG_BUS) {
			ctrl_reg_offset = MDSS_MDP_LP_DEBUGBUS_CTRL;
			status_reg_offset = MDSS_MDP_LP_DEBUGBUS_STATUS;
		} else {
			pr_err("unsupported debug bus type %d\n", i);
			continue;
		}

		for (j = 0; j < MAX_DBG_BUS_TEST_POINTS; j++) {
			u32 ctrl_val, tg_sel = j;
			/* skip if tp read is not needed */
			if (item->test_point[j] == 0) {
				tp_val[j] = 0xFFFFFFFF;
				continue;
			}

			if (j == 2)
				tg_sel = 3;
			else if (j == 3)
				tg_sel = 6;

			ctrl_val = 1 | (tg_sel << 1) | (item->block_id << 4);
			writel_relaxed(ctrl_val,
				mdata->mdss_base + ctrl_reg_offset);
			tp_val[j] = readl_relaxed(mdata->mdss_base + status_reg_offset);
			pr_debug("ctrl: off=0x%x val=0x%x status:off=0x%x val=0x%x\n",
				ctrl_reg_offset, ctrl_val,
				status_reg_offset, tp_val[j]);
		}
		if (!dump_in_memory) {
			pr_info("dbg_bus_type:%d: block_id:%d tp0:0x%08x tp1:0x%08x tp3:0x%08x tp6:0x%08x\n",
				i, item->block_id,
				tp_val[0], tp_val[1], tp_val[2], tp_val[3]);
		} else {
			dump_addr[dump_cnt++] = item->bus_type;
			dump_addr[dump_cnt++] = item->block_id;

			for (j = 0; j < MAX_DBG_BUS_TEST_POINTS; j++)
				dump_addr[dump_cnt++] = tp_val[j];

			dump_addr[dump_cnt++] = 0xFFFFFFFF;
			dump_addr[dump_cnt++] = 0xFFFFFFFF;
		}
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
}

int vsync_count;
static struct mdss_mdp_misr_map {
	u32 ctrl_reg;
	u32 value_reg;
	u32 crc_op_mode;
	u32 crc_index;
	bool use_ping;
	bool is_ping_full;
	bool is_pong_full;
	struct mutex crc_lock;
	u32 crc_ping[MISR_CRC_BATCH_SIZE];
	u32 crc_pong[MISR_CRC_BATCH_SIZE];
} mdss_mdp_misr_table[DISPLAY_MISR_MAX] = {
	[DISPLAY_MISR_DSI0] = {
		.ctrl_reg = MDSS_MDP_LP_MISR_CTRL_DSI0,
		.value_reg = MDSS_MDP_LP_MISR_SIGN_DSI0,
		.crc_op_mode = 0,
		.crc_index = 0,
		.use_ping = true,
		.is_ping_full = false,
		.is_pong_full = false,
	},
	[DISPLAY_MISR_DSI1] = {
		.ctrl_reg = MDSS_MDP_LP_MISR_CTRL_DSI1,
		.value_reg = MDSS_MDP_LP_MISR_SIGN_DSI1,
		.crc_op_mode = 0,
		.crc_index = 0,
		.use_ping = true,
		.is_ping_full = false,
		.is_pong_full = false,
	},
	[DISPLAY_MISR_EDP] = {
		.ctrl_reg = MDSS_MDP_LP_MISR_CTRL_EDP,
		.value_reg = MDSS_MDP_LP_MISR_SIGN_EDP,
		.crc_op_mode = 0,
		.crc_index = 0,
		.use_ping = true,
		.is_ping_full = false,
		.is_pong_full = false,
	},
	[DISPLAY_MISR_HDMI] = {
		.ctrl_reg = MDSS_MDP_LP_MISR_CTRL_HDMI,
		.value_reg = MDSS_MDP_LP_MISR_SIGN_HDMI,
		.crc_op_mode = 0,
		.crc_index = 0,
		.use_ping = true,
		.is_ping_full = false,
		.is_pong_full = false,
	},
	[DISPLAY_MISR_MDP] = {
		.ctrl_reg = MDSS_MDP_LP_MISR_CTRL_MDP,
		.value_reg = MDSS_MDP_LP_MISR_SIGN_MDP,
		.crc_op_mode = 0,
		.crc_index = 0,
		.use_ping = true,
		.is_ping_full = false,
		.is_pong_full = false,
	},
};

static inline struct mdss_mdp_misr_map *mdss_misr_get_map(u32 block_id)
{
	struct mdss_mdp_misr_map *map;

	if (block_id > DISPLAY_MISR_MDP) {
		pr_err("MISR Block id (%d) out of range\n", block_id);
		return NULL;
	}

	map = mdss_mdp_misr_table + block_id;
	if ((map->ctrl_reg == 0) || (map->value_reg == 0)) {
		pr_err("MISR Block id (%d) config not found\n", block_id);
		return NULL;
	}

	return map;
}

/*
 * switch_mdp_misr_offset() - Update MDP MISR register offset for MDSS
 * Hardware Revision 103.
 * @map: mdss_mdp_misr_map
 * @mdp_rev: MDSS Hardware Revision
 * @block_id: Logical MISR Block ID
 *
 * Return: true when MDSS Revision is 103 else false.
 */
static bool switch_mdp_misr_offset(struct mdss_mdp_misr_map *map, u32 mdp_rev,
					u32 block_id)
{
	bool use_mdp_up_misr = false;

	if ((IS_MDSS_MAJOR_MINOR_SAME(mdp_rev, MDSS_MDP_HW_REV_103)) &&
		(block_id == DISPLAY_MISR_MDP)) {
		/* Use Upper pipe MISR for Layer Mixer CRC */
		map->ctrl_reg = MDSS_MDP_UP_MISR_CTRL_MDP;
		map->value_reg = MDSS_MDP_UP_MISR_SIGN_MDP;
		use_mdp_up_misr = true;
	}
	pr_debug("MISR Module(%d) Offset of MISR_CTRL = 0x%x MISR_SIG = 0x%x\n",
			block_id, map->ctrl_reg, map->value_reg);
	return use_mdp_up_misr;
}

int mdss_misr_set(struct mdss_data_type *mdata,
			struct mdp_misr *req,
			struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_misr_map *map;
	struct mdss_mdp_mixer *mixer;
	u32 config = 0, val = 0;
	u32 mixer_num = 0;
	bool is_valid_wb_mixer = true;
	bool use_mdp_up_misr = false;

	map = mdss_misr_get_map(req->block_id);
	use_mdp_up_misr = switch_mdp_misr_offset(map, mdata->mdp_rev,
				req->block_id);
	if (!map) {
		pr_err("Invalid MISR Block=%d\n", req->block_id);
		return -EINVAL;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (req->block_id == DISPLAY_MISR_MDP) {
		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_DEFAULT);
		mixer_num = mixer->num;
		pr_debug("SET MDP MISR BLK to MDSS_MDP_LP_MISR_SEL_LMIX%d_GC\n",
			req->block_id);
		switch (mixer_num) {
		case MDSS_MDP_INTF_LAYERMIXER0:
			pr_debug("Use Layer Mixer 0 for WB CRC\n");
			val = MDSS_MDP_LP_MISR_SEL_LMIX0_GC;
			break;
		case MDSS_MDP_INTF_LAYERMIXER1:
			pr_debug("Use Layer Mixer 1 for WB CRC\n");
			val = MDSS_MDP_LP_MISR_SEL_LMIX1_GC;
			break;
		case MDSS_MDP_INTF_LAYERMIXER2:
			pr_debug("Use Layer Mixer 2 for WB CRC\n");
			val = MDSS_MDP_LP_MISR_SEL_LMIX2_GC;
			break;
		default:
			pr_err("Invalid Layer Mixer %d selected for WB CRC\n",
				mixer_num);
			is_valid_wb_mixer = false;
			break;
		}
		if (is_valid_wb_mixer) {
			if (use_mdp_up_misr)
				writel_relaxed((val +
					MDSS_MDP_UP_MISR_LMIX_SEL_OFFSET),
					(mdata->mdp_base +
					 MDSS_MDP_UP_MISR_SEL));
			else
				writel_relaxed(val, (mdata->mdp_base +
					MDSS_MDP_LP_MISR_SEL));
		}
	}
	vsync_count = 0;
	map->crc_op_mode = req->crc_op_mode;
	config = (MDSS_MDP_MISR_CTRL_FRAME_COUNT_MASK & req->frame_count) |
			(MDSS_MDP_MISR_CTRL_ENABLE);

	writel_relaxed(MDSS_MDP_MISR_CTRL_STATUS_CLEAR,
			mdata->mdp_base + map->ctrl_reg);
	/* ensure clear is done */
	wmb();

	memset(map->crc_ping, 0, sizeof(map->crc_ping));
	memset(map->crc_pong, 0, sizeof(map->crc_pong));
	map->crc_index = 0;
	map->use_ping = true;
	map->is_ping_full = false;
	map->is_pong_full = false;

	if (MISR_OP_BM != map->crc_op_mode) {

		writel_relaxed(config,
				mdata->mdp_base + map->ctrl_reg);
		pr_debug("MISR_CTRL = 0x%x",
				readl_relaxed(mdata->mdp_base + map->ctrl_reg));
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	return 0;
}

int mdss_misr_get(struct mdss_data_type *mdata,
			struct mdp_misr *resp,
			struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_misr_map *map;
	u32 status;
	int ret = -1;
	int i;

	map = mdss_misr_get_map(resp->block_id);
	switch_mdp_misr_offset(map, mdata->mdp_rev, resp->block_id);
	if (!map) {
		pr_err("Invalid MISR Block=%d\n", resp->block_id);
		return -EINVAL;
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	switch (map->crc_op_mode) {
	case MISR_OP_SFM:
	case MISR_OP_MFM:
		ret = readl_poll_timeout(mdata->mdp_base + map->ctrl_reg,
				status, status & MDSS_MDP_MISR_CTRL_STATUS,
				MISR_POLL_SLEEP, MISR_POLL_TIMEOUT);
		if (ret == 0) {
			resp->crc_value[0] = readl_relaxed(mdata->mdp_base +
				map->value_reg);
			pr_debug("CRC %d=0x%x\n", resp->block_id,
				resp->crc_value[0]);
			writel_relaxed(0, mdata->mdp_base + map->ctrl_reg);
		} else {
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
			ret = readl_poll_timeout(mdata->mdp_base +
					map->ctrl_reg, status,
					status & MDSS_MDP_MISR_CTRL_STATUS,
					MISR_POLL_SLEEP, MISR_POLL_TIMEOUT);
			if (ret == 0) {
				resp->crc_value[0] =
					readl_relaxed(mdata->mdp_base +
					map->value_reg);
			}
			writel_relaxed(0, mdata->mdp_base + map->ctrl_reg);
		}
		break;
	case MISR_OP_BM:
		if (map->is_ping_full) {
			for (i = 0; i < MISR_CRC_BATCH_SIZE; i++)
				resp->crc_value[i] = map->crc_ping[i];
			memset(map->crc_ping, 0, sizeof(map->crc_ping));
			map->is_ping_full = false;
			ret = 0;
		} else if (map->is_pong_full) {
			for (i = 0; i < MISR_CRC_BATCH_SIZE; i++)
				resp->crc_value[i] = map->crc_pong[i];
			memset(map->crc_pong, 0, sizeof(map->crc_pong));
			map->is_pong_full = false;
			ret = 0;
		} else {
			pr_debug("mdss_mdp_misr_crc_get PING BUF %s\n",
				map->is_ping_full ? "FULL" : "EMPTRY");
			pr_debug("mdss_mdp_misr_crc_get PONG BUF %s\n",
				map->is_pong_full ? "FULL" : "EMPTRY");
		}
		resp->crc_op_mode = map->crc_op_mode;
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	return ret;
}

/* This function is expected to be called from interrupt context */
void mdss_misr_crc_collect(struct mdss_data_type *mdata, int block_id)
{
	struct mdss_mdp_misr_map *map;
	u32 status = 0;
	u32 crc = 0x0BAD0BAD;
	bool crc_stored = false;

	map = mdss_misr_get_map(block_id);
	switch_mdp_misr_offset(map, mdata->mdp_rev, block_id);
	if (!map || (map->crc_op_mode != MISR_OP_BM))
		return;

	status = readl_relaxed(mdata->mdp_base + map->ctrl_reg);
	if (MDSS_MDP_MISR_CTRL_STATUS & status) {
		crc = readl_relaxed(mdata->mdp_base + map->value_reg);
		if (map->use_ping) {
			if (map->is_ping_full) {
				pr_err("PING Buffer FULL\n");
			} else {
				map->crc_ping[map->crc_index] = crc;
				crc_stored = true;
			}
		} else {
			if (map->is_pong_full) {
				pr_err("PONG Buffer FULL\n");
			} else {
				map->crc_pong[map->crc_index] = crc;
				crc_stored = true;
			}
		}

		if (crc_stored) {
			map->crc_index = (map->crc_index + 1);
			if (map->crc_index == MISR_CRC_BATCH_SIZE) {
				map->crc_index = 0;
				if (true == map->use_ping) {
					map->is_ping_full = true;
					map->use_ping = false;
				} else {
					map->is_pong_full = true;
					map->use_ping = true;
				}
				pr_debug("USE BUFF %s\n", map->use_ping ?
					"PING" : "PONG");
				pr_debug("mdss_misr_crc_collect PING BUF %s\n",
					map->is_ping_full ? "FULL" : "EMPTRY");
				pr_debug("mdss_misr_crc_collect PONG BUF %s\n",
					map->is_pong_full ? "FULL" : "EMPTRY");
			}
		} else {
			pr_err("CRC(%d) Not saved\n", crc);
		}

		writel_relaxed(MDSS_MDP_MISR_CTRL_STATUS_CLEAR,
				mdata->mdp_base + map->ctrl_reg);
		writel_relaxed(MISR_CRC_BATCH_CFG,
				mdata->mdp_base + map->ctrl_reg);
	} else if (0 == status) {
		writel_relaxed(MISR_CRC_BATCH_CFG,
				mdata->mdp_base + map->ctrl_reg);
		pr_debug("$$ Batch CRC Start $$\n");
	}
	pr_debug("$$ Vsync Count = %d, CRC=0x%x Indx = %d$$\n",
		vsync_count, crc, map->crc_index);

	if (MAX_VSYNC_COUNT == vsync_count) {
		pr_err("RESET vsync_count(%d)\n", vsync_count);
		vsync_count = 0;
	} else {
		vsync_count += 1;
	}
}
