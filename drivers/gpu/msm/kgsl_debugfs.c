/* Copyright (c) 2002,2008-2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/debugfs.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

/*default log levels is error for everything*/
#define KGSL_LOG_LEVEL_MAX     7

struct dentry *kgsl_debugfs_dir;
struct dentry *proc_d_debugfs;

static inline int kgsl_log_set(unsigned int *log_val, void *data, u64 val)
{
	*log_val = min((unsigned int)val, (unsigned int)KGSL_LOG_LEVEL_MAX);
	return 0;
}

#define KGSL_DEBUGFS_LOG(__log)                         \
static int __log ## _set(void *data, u64 val)           \
{                                                       \
	struct kgsl_device *device = data;              \
	return kgsl_log_set(&device->__log, data, val); \
}                                                       \
static int __log ## _get(void *data, u64 *val)	        \
{                                                       \
	struct kgsl_device *device = data;              \
	*val = device->__log;                           \
	return 0;                                       \
}                                                       \
DEFINE_SIMPLE_ATTRIBUTE(__log ## _fops,                 \
__log ## _get, __log ## _set, "%llu\n");                \

KGSL_DEBUGFS_LOG(drv_log);
KGSL_DEBUGFS_LOG(cmd_log);
KGSL_DEBUGFS_LOG(ctxt_log);
KGSL_DEBUGFS_LOG(mem_log);
KGSL_DEBUGFS_LOG(pwr_log);

static int memfree_hist_print(struct seq_file *s, void *unused)
{
	void *base = kgsl_driver.memfree_hist.base_hist_rb;

	struct kgsl_memfree_hist_elem *wptr = kgsl_driver.memfree_hist.wptr;
	struct kgsl_memfree_hist_elem *p;
	char str[16];

	seq_printf(s, "%8s %8s %8s %11s\n",
			"pid", "gpuaddr", "size", "flags");

	mutex_lock(&kgsl_driver.memfree_hist_mutex);
	p = wptr;
	for (;;) {
		kgsl_get_memory_usage(str, sizeof(str), p->flags);
		/*
		 * if the ring buffer is not filled up yet
		 * all its empty elems have size==0
		 * just skip them ...
		*/
		if (p->size)
			seq_printf(s, "%8d %08x %8d %11s\n",
				p->pid, p->gpuaddr, p->size, str);
		p++;
		if ((void *)p >= base + kgsl_driver.memfree_hist.size)
			p = (struct kgsl_memfree_hist_elem *) base;

		if (p == kgsl_driver.memfree_hist.wptr)
			break;
	}
	mutex_unlock(&kgsl_driver.memfree_hist_mutex);
	return 0;
}

static int memfree_hist_open(struct inode *inode, struct file *file)
{
	return single_open(file, memfree_hist_print, inode->i_private);
}

static const struct file_operations memfree_hist_fops = {
	.open = memfree_hist_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void kgsl_device_debugfs_init(struct kgsl_device *device)
{
	if (kgsl_debugfs_dir && !IS_ERR(kgsl_debugfs_dir))
		device->d_debugfs = debugfs_create_dir(device->name,
						       kgsl_debugfs_dir);

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	debugfs_create_file("log_level_cmd", 0644, device->d_debugfs, device,
			    &cmd_log_fops);
	debugfs_create_file("log_level_ctxt", 0644, device->d_debugfs, device,
			    &ctxt_log_fops);
	debugfs_create_file("log_level_drv", 0644, device->d_debugfs, device,
			    &drv_log_fops);
	debugfs_create_file("log_level_mem", 0644, device->d_debugfs, device,
				&mem_log_fops);
	debugfs_create_file("log_level_pwr", 0644, device->d_debugfs, device,
				&pwr_log_fops);
	debugfs_create_file("memfree_history", 0444, device->d_debugfs, device,
				&memfree_hist_fops);
}

struct type_entry {
	int type;
	const char *str;
};

static const struct type_entry memtypes[] = { KGSL_MEM_TYPES };

static const char *memtype_str(int memtype)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(memtypes); i++)
		if (memtypes[i].type == memtype)
			return memtypes[i].str;
	return "unknown";
}

static char get_alignflag(const struct kgsl_memdesc *m)
{
	int align = kgsl_memdesc_get_align(m);
	if (align >= ilog2(SZ_1M))
		return 'L';
	else if (align >= ilog2(SZ_64K))
		return 'l';
	return '-';
}

static char get_cacheflag(const struct kgsl_memdesc *m)
{
	static const char table[] = {
		[KGSL_CACHEMODE_WRITECOMBINE] = '-',
		[KGSL_CACHEMODE_UNCACHED] = 'u',
		[KGSL_CACHEMODE_WRITEBACK] = 'b',
		[KGSL_CACHEMODE_WRITETHROUGH] = 't',
	};
	return table[kgsl_memdesc_get_cachemode(m)];
}

static void print_mem_entry(struct seq_file *s, struct kgsl_mem_entry *entry)
{
	char flags[7];
	char usage[16];
	struct kgsl_memdesc *m = &entry->memdesc;

	flags[0] = kgsl_memdesc_is_global(m) ?  'g' : '-';
	flags[1] = m->flags & KGSL_MEMFLAGS_GPUREADONLY ? 'r' : '-';
	flags[2] = get_alignflag(m);
	flags[3] = get_cacheflag(m);
	flags[4] = kgsl_memdesc_use_cpu_map(m) ? 'p' : '-';
	flags[5] = (m->useraddr) ? 'Y' : 'N';
	flags[6] = '\0';

	kgsl_get_memory_usage(usage, sizeof(usage), m->flags);

	seq_printf(s, "%08x %08lx %8zd %5d %6s %10s %16s %5d\n",
			m->gpuaddr, m->useraddr, m->size, entry->id, flags,
			memtype_str(kgsl_memdesc_usermem_type(m)),
			usage, m->sglen);
}

struct process_mem_entry {
	struct kgsl_process_private *pprivate;
	int unbound;
};

static struct kgsl_mem_entry *process_mem_seq_find(
			struct seq_file *s, void *v, loff_t l)
{
	struct process_mem_entry *e = s->private;
	struct kgsl_process_private *private = e->pprivate;
	struct kgsl_mem_entry *entry = v;
	struct rb_node *node = NULL;
	int id = 0;

	l--;
	spin_lock(&private->mem_lock);
	if (entry == SEQ_START_TOKEN) {
		node = rb_first(&private->mem_rb);
		e->unbound = 0;
	} else if (!e->unbound) {
		node = rb_next(&entry->node);
	} else {
		id = entry->id + 1;
	}
	for (; node; node = rb_next(node)) {
		if (l-- == 0) {
			entry = rb_entry(node, struct kgsl_mem_entry, node);
			if (kgsl_mem_entry_get(entry)) {
				e->unbound = 0;
				goto found;
			}
			l++;
		}
	}
	for (entry = idr_get_next(&private->mem_idr, &id); entry;
			id++, entry = idr_get_next(&private->mem_idr, &id)) {
		if (!entry->memdesc.gpuaddr && (l-- == 0)) {
			if (kgsl_mem_entry_get(entry)) {
				e->unbound = 1;
				goto found;
			}
			l++;
		}
	}
	entry = NULL;
found:
	spin_unlock(&private->mem_lock);
	if (v != SEQ_START_TOKEN)
		kgsl_mem_entry_put(v);
	return entry;
}

static void *process_mem_seq_start(struct seq_file *s, loff_t *pos)
{
	loff_t l = *pos;

	if (l == 0)
		return SEQ_START_TOKEN;
	else
		return process_mem_seq_find(s, SEQ_START_TOKEN, l);
}

static void *process_mem_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	++*pos;
	return process_mem_seq_find(s, v, 1);
}

static void process_mem_seq_stop(struct seq_file *s, void *v)
{
	if (v && v != SEQ_START_TOKEN)
		kgsl_mem_entry_put(v);
}

static int process_mem_seq_show(struct seq_file *s, void *v)
{
	if (v == SEQ_START_TOKEN)
		seq_printf(s, "%8s %8s %8s %5s %6s %10s %16s %5s\n",
			"gpuaddr", "useraddr", "size", "id", "flags", "type",
			"usage", "sglen");
	else
		print_mem_entry(s, v);
	return 0;
}

static const struct seq_operations process_mem_seq_ops = {
	.start = process_mem_seq_start,
	.next = process_mem_seq_next,
	.stop = process_mem_seq_stop,
	.show = process_mem_seq_show,
};

static int process_mem_open(struct inode *inode, struct file *file)
{
	int ret;
	pid_t pid = (pid_t) (unsigned long) inode->i_private;
	struct kgsl_process_private *private = NULL;

	private = kgsl_process_private_find(pid);

	if (!private)
		return -ENODEV;

	ret = seq_open_private(file, &process_mem_seq_ops,
			sizeof(struct process_mem_entry));
	if (ret) {
		kgsl_process_private_put(private);
	} else {
		struct seq_file *s = file->private_data;
		struct process_mem_entry *e = s->private;
		e->pprivate = private;
	}

	return ret;
}

static int process_mem_release(struct inode *inode, struct file *file)
{
	struct seq_file *s = file->private_data;
	struct process_mem_entry *e = s->private;
	struct kgsl_process_private *private = e->pprivate;

	if (private)
		kgsl_process_private_put(private);

	return seq_release_private(inode, file);
}

static const struct file_operations process_mem_fops = {
	.open = process_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = process_mem_release,
};


/**
 * kgsl_process_init_debugfs() - Initialize debugfs for a process
 * @private: Pointer to process private structure created for the process
 *
 * @returns: 0 on success, error code otherwise
 *
 * kgsl_process_init_debugfs() is called at the time of creating the
 * process struct when a process opens kgsl device for the first time.
 * The function creates the debugfs files for the process. If debugfs is
 * disabled in the kernel, we ignore that error and return as successful.
 */
int
kgsl_process_init_debugfs(struct kgsl_process_private *private)
{
	unsigned char name[16];
	int ret = 0;
	struct dentry *dentry;

	snprintf(name, sizeof(name), "%d", private->pid);

	private->debug_root = debugfs_create_dir(name, proc_d_debugfs);

	if (!private->debug_root)
		return -EINVAL;

	private->debug_root->d_inode->i_uid = proc_d_debugfs->d_inode->i_uid;
	private->debug_root->d_inode->i_gid = proc_d_debugfs->d_inode->i_gid;

	/*
	 * debugfs_create_dir() and debugfs_create_file() both
	 * return -ENODEV if debugfs is disabled in the kernel.
	 * We make a distinction between these two functions
	 * failing and debugfs being disabled in the kernel.
	 * In the first case, we abort process private struct
	 * creation, in the second we continue without any changes.
	 * So if debugfs is disabled in kernel, return as
	 * success.
	 */
	dentry = debugfs_create_file("mem", 0400, private->debug_root,
		(void *) ((unsigned long) private->pid), &process_mem_fops);

	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);

		if (ret == -ENODEV)
			ret = 0;
	} else if (dentry) {
		dentry->d_inode->i_uid = proc_d_debugfs->d_inode->i_uid;
		dentry->d_inode->i_gid = proc_d_debugfs->d_inode->i_gid;
	}

	return ret;
}

void kgsl_core_debugfs_init(void)
{
	kgsl_debugfs_dir = debugfs_create_dir("kgsl", 0);
	proc_d_debugfs = debugfs_create_dir("proc", kgsl_debugfs_dir);
}

void kgsl_core_debugfs_close(void)
{
	debugfs_remove_recursive(kgsl_debugfs_dir);
}
