/*
 * drivers/misc/emmc_ipanic.c
 *
 * Copyright (C) 2011 Intel Corp
 * Author: dongxing.zhang@intel.com
 * Author: jun.zhang@intel.com
 * Author: chuansheng.liu@intel.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/mmc/host.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/preempt.h>
#include <linux/pci.h>
#include <linux/nmi.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/panic_gbuffer.h>
#include "emmc_ipanic.h"

#include <linux/kmsg_dump.h>

static char *part_label = "";
module_param(part_label, charp, 0);
MODULE_PARM_DESC(part_label, "IPanic mmc partition device label (panic)");

static u32 disable_emmc_ipanic;
core_param(disable_emmc_ipanic, disable_emmc_ipanic, uint, 0644);

static struct mmc_emergency_info emmc_info = {
	.init = mmc_emergency_init,
	.write = mmc_emergency_write,
	.part_label = CONFIG_EMMC_IPANIC_PLABEL,
};

static unsigned char *ipanic_proc_entry_name[PROC_MAX_ENTRIES] = {
	"emmc_ipanic_header",
	"emmc_ipanic_console",
	"emmc_ipanic_threads",
	"emmc_ipanic_gbuffer"
};

static int in_panic;
static struct emmc_ipanic_data drv_ctx;
static struct work_struct proc_removal_work;
static int log_offset[IPANIC_LOG_MAX];
static int log_len[IPANIC_LOG_MAX];	/* sector count */
static int log_size[IPANIC_LOG_MAX];	/* byte count */
static size_t log_head[IPANIC_LOG_MAX];
static size_t log_woff[IPANIC_LOG_MAX];
static unsigned char last_chunk_buf[SECTOR_SIZE];
static int last_chunk_buf_len;
static DEFINE_MUTEX(drv_mutex);
static void (*func_stream_emmc) (void);

static struct kmsg_dumper ipanic_dumper;

static void emmc_panic_erase(unsigned char *buffer, Sector *sect)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct mmc_emergency_info *emmc = ctx->emmc;
	unsigned char *read_buf_ptr = buffer;
	Sector new_sect;
	int rc;

	if (!emmc) {
		pr_err("%s:invalid emmc infomation\n", __func__);
		return;
	}

	if (!read_buf_ptr || !sect) {
		sect = &new_sect;
		if (!emmc->bdev) {
			pr_err("%s:invalid emmc block device\n",
				__func__);
			goto out;
		}
		/* make sure the block device is open rw */
		rc = blkdev_get(emmc->bdev, FMODE_READ | FMODE_WRITE, emmc_panic_erase);
		if (rc < 0) {
			pr_err("%s: blk_dev_get failed!\n", __func__);
			goto out;
		}

		/*read panic header */
		read_buf_ptr =
		    read_dev_sector(emmc->bdev, emmc->start_block, sect);
		if (!read_buf_ptr) {
			pr_err("%s: read sector error(%llu)!\n",
				__func__, (u64) emmc->start_block);
			goto out;
		}
	}

	/*write all zero to panic header */
	lock_page(sect->v);
	memset(read_buf_ptr, 0, SECTOR_SIZE);
	set_page_dirty(sect->v);
	unlock_page(sect->v);
	sync_blockdev(emmc->bdev);

	if (!read_buf_ptr)
		put_dev_sector(*sect);
out:
	memset(&ctx->hdr, 0, SECTOR_SIZE);
	return;
}

static int emmc_read(struct mmc_emergency_info *emmc, void *holder,
		     char *buffer, off_t offset, int count, bool to_user)
{
	unsigned char *read_ptr;
	unsigned int sector_no;
	off_t sector_offset;
	Sector sect;
	int rc;

	if (!emmc) {
		pr_err("%s:invalid emmc infomation\n", __func__);
		return 0;
	}
	if (!emmc->bdev) {
		pr_err("%s:invalid emmc block device\n", __func__);
		return 0;
	}

	sector_no = offset >> SECTOR_SIZE_SHIFT;
	sector_offset = offset & (SECTOR_SIZE - 1);
	if (sector_no >= emmc->block_count) {
		pr_err("%s: reading an invalid address\n", __func__);
		return -EINVAL;
	}

	/* make sure the block device is open rw */
	rc = blkdev_get(emmc->bdev, FMODE_READ | FMODE_WRITE, holder);
	if (rc < 0) {
		pr_err("%s: blk_dev_get failed!\n", __func__);
		return 0;
	}

	read_ptr = read_dev_sector(emmc->bdev, sector_no + emmc->start_block,
				   &sect);
	if (!read_ptr) {
		put_dev_sector(sect);
		return -EINVAL;
	}
	/* count and read_ptr are updated to match flash page size */
	if (count + sector_offset > SECTOR_SIZE)
		count = SECTOR_SIZE - sector_offset;

	if (sector_offset)
		read_ptr += sector_offset;

	if (to_user) {
		if (copy_to_user(buffer, read_ptr, count)) {
			pr_err( "%s: Failed to copy buffer to User\n",
				__func__);
			return 0;
		}
	}
	else
		memcpy(buffer, read_ptr, count);

	put_dev_sector(sect);

	return count;
}

static ssize_t emmc_ipanic_gbuffer_proc_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	size_t log_len, log_head;
	off_t log_off;
	int rc;

	if (!ctx) {
		pr_err("%s:invalid panic handler\n", __func__);
		return 0;
	}

	if (!count)
		return 0;

	mutex_lock(&drv_mutex);

	log_off = ctx->curr.log_offset[IPANIC_LOG_GBUFFER];
	log_len = ctx->curr.log_length[IPANIC_LOG_GBUFFER];
	log_head = ctx->curr.log_head[IPANIC_LOG_GBUFFER];

	if (*ppos >= log_len) {
		mutex_unlock(&drv_mutex);
		return 0;
	}

	if (*ppos < log_len - log_head) {
		/* No overflow (log_head == 0)
		 * or
		 * overflow 2nd part buf (log_head = log_woff)
		 * |-------w--------|
		 *           off^
		 *         |--------|
		 */
		log_off += log_head;
		log_len -= log_head;
	} else {
		/* 1st part buf
		 * |-------w--------|
		 *   off^
		 * |-------|
		 */
		*ppos -= (log_len - log_head);
		log_len = log_head;
	}

	if ((*ppos + count) > log_len)
		count = log_len - *ppos;

	rc = emmc_read(ctx->emmc, emmc_ipanic_gbuffer_proc_read,
			  buffer, log_off + *ppos, count, true);
	if (rc <= 0) {
		mutex_unlock(&drv_mutex);
		pr_err("%s: emmc_read: invalid args: offset:0x%08llx, count:%zd",
		       __func__, (u64)(log_off + *ppos), count);
		return rc;
	}

	*ppos += rc;

	mutex_unlock(&drv_mutex);

	return rc;
}

static ssize_t emmc_ipanic_proc_read_by_log(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos, int log)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	size_t file_length;
	off_t file_offset;
	int rc;

	if (!ctx) {
		pr_err("%s:invalid panic handler\n", __func__);
		return 0;
	}

	if (!count)
		return 0;

	if (log < 0 || log > IPANIC_LOG_MAX) {
		pr_err("%s: Bad log number (%d)\n", __func__, log);
		return -EINVAL;
	}

	mutex_lock(&drv_mutex);

	if (log == IPANIC_LOG_HEADER) {
		file_length = ctx->hdr.log_size;
		file_offset = offsetof(struct panic_header, panic);
	}
	else {
		file_length = ctx->curr.log_length[log];
		file_offset = ctx->curr.log_offset[log];
	}

	if (*ppos >= file_length) {
		mutex_unlock(&drv_mutex);
		return 0;
	}

	if ((*ppos + count) > file_length)
		count = file_length - *ppos;

	rc= emmc_read(ctx->emmc, emmc_ipanic_proc_read_by_log,
		       buffer, file_offset + *ppos, count, true);
	if (rc <= 0) {
		mutex_unlock(&drv_mutex);
		pr_err("%s: emmc_read: invalid args: offset:0x%08llx, count:%zd",
		       __func__, (u64)(file_offset + *ppos), count);
		return rc;
	}

	*ppos += rc;

	mutex_unlock(&drv_mutex);

	return rc;
}

static ssize_t emmc_ipanic_proc_read_hdr(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	return emmc_ipanic_proc_read_by_log(file, buffer, count, ppos, IPANIC_LOG_HEADER);
}

static ssize_t emmc_ipanic_proc_read0(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	return emmc_ipanic_proc_read_by_log(file, buffer, count, ppos, IPANIC_LOG_CONSOLE);
}

static ssize_t emmc_ipanic_proc_read1(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	return emmc_ipanic_proc_read_by_log(file, buffer, count, ppos, IPANIC_LOG_THREADS);
}

static void emmc_ipanic_remove_proc_work(struct work_struct *work)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	int log;

	mutex_lock(&drv_mutex);
	emmc_panic_erase(NULL, NULL);

	for (log = 0; log < PROC_MAX_ENTRIES; log++) {
		if (ctx->ipanic_proc_entry[log]) {
			remove_proc_entry(ctx->ipanic_proc_entry_name
					  [log], NULL);
			ctx->ipanic_proc_entry[log] = NULL;
		}
	}
	mutex_unlock(&drv_mutex);
}

static ssize_t emmc_ipanic_proc_write(struct file *file,
					const char __user *buffer,
					size_t count, loff_t *ppos)
{
	schedule_work(&proc_removal_work);
	return count;
}

/* In section order inside panic partition : */
static const struct file_operations ipanic_emmc_read_header_fops = {
	.read = emmc_ipanic_proc_read_hdr,
	.write = emmc_ipanic_proc_write,
};

static const struct file_operations ipanic_emmc0_fops = {
	.read = emmc_ipanic_proc_read0,
	.write = emmc_ipanic_proc_write,
};

static const struct file_operations ipanic_emmc1_fops = {
	.read = emmc_ipanic_proc_read1,
	.write = emmc_ipanic_proc_write,
};

static const struct file_operations ipanic_emmc_gbuffer_fops = {
	.read = emmc_ipanic_gbuffer_proc_read,
	.write = emmc_ipanic_proc_write
};

static void emmc_panic_notify_add(void)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct mmc_emergency_info *emmc;
	unsigned char *read_buf_ptr;
	Sector sect;
	int rc, idx_log, idx_proc;
	int proc_entry_created = 0;

	if (!ctx) {
		pr_err("%s:invalid panic handler\n", __func__);
		return;
	}

	emmc = ctx->emmc;
	if (!emmc) {
		pr_err("%s:invalid emmc infomation\n", __func__);
		goto out_err;
	}

	if (!emmc->bdev) {
		pr_err("%s:invalid emmc block device\n", __func__);
		goto out_err;
	}

	/* make sure the block device is open rw */
	rc = blkdev_get(emmc->bdev, FMODE_READ | FMODE_WRITE, emmc_panic_notify_add);
	if (rc < 0) {
		pr_err("%s: blk_dev_get failed!\n", __func__);
		goto out_err;
	}

	/* read panic header */
	read_buf_ptr = read_dev_sector(emmc->bdev, emmc->start_block, &sect);
	if (!read_buf_ptr) {
		pr_err("%s: read sector error(%llu)!\n", __func__,
			(u64) emmc->start_block);
		return;
	}

	memcpy(&ctx->hdr, read_buf_ptr, sizeof(struct panic_header));

	if (ctx->hdr.magic != PANIC_MAGIC) {
		pr_info("%s: bad magic %x, no data available\n",
			__func__, ctx->hdr.magic);
		emmc_panic_erase(read_buf_ptr, &sect);
		goto put_sector;
	}

	pr_info("%s: Data available in panic partition\n", __func__);

	if (ctx->hdr.version != PHDR_VERSION) {
		pr_err("%s: Version mismatch (%d != %d)\n",
			__func__, ctx->hdr.version, PHDR_VERSION);
		emmc_panic_erase(read_buf_ptr, &sect);
		goto put_sector;
	}

	/* Create proc entry for the panic header */
	ctx->ipanic_proc_entry[PROC_HEADER_INDEX] =
		proc_create(ctx->ipanic_proc_entry_name
			[PROC_HEADER_INDEX], S_IFREG | S_IRUGO, NULL,
			&ipanic_emmc_read_header_fops);

	if (!ctx->ipanic_proc_entry[PROC_HEADER_INDEX])
		pr_err("%s: failed creating proc file\n", __func__);
	else {
		proc_entry_created = 1;
		pr_info("%s: proc entry created: %s\n", __func__,
			ctx->ipanic_proc_entry_name[PROC_HEADER_INDEX]);
	}

	/* read log_info to retrieve block numbers and offsets */
	read_buf_ptr = read_dev_sector(emmc->bdev, emmc->start_block+1, &sect);
	if (!read_buf_ptr) {
		pr_err("%s: read sector error(%llu)!\n", __func__,
			(u64)emmc->start_block + 1);
		return;
	}

	memcpy(&ctx->curr, read_buf_ptr, sizeof(struct log_info));

	/* Log files other than header */
	for (idx_log = 0; idx_log < IPANIC_LOG_MAX; idx_log++) {

		pr_info("%s: log file %u(%u, %u)\n", __func__, idx_log,
			ctx->curr.log_offset[idx_log],
			ctx->curr.log_length[idx_log]);

		/* Skip empty file. */
		if (ctx->curr.log_length[idx_log] == 0) {
			pr_info("%s: empty log file %u\n", __func__, idx_log);
			continue;
		}

		/* Create proc entry for console, threads and gbuffer log. */
		if (idx_log == IPANIC_LOG_CONSOLE) {
			idx_proc = PROC_CONSOLE_INDEX;
			ctx->ipanic_proc_entry[PROC_CONSOLE_INDEX] =
				proc_create(ctx->ipanic_proc_entry_name
					[PROC_CONSOLE_INDEX], S_IFREG | S_IRUGO,
					NULL,
					&ipanic_emmc0_fops);
		} else if (idx_log == IPANIC_LOG_THREADS) {
			idx_proc = PROC_THREADS_INDEX;
			ctx->ipanic_proc_entry[PROC_THREADS_INDEX] =
				proc_create(ctx->ipanic_proc_entry_name
					[PROC_THREADS_INDEX], S_IFREG | S_IRUGO,
					NULL,
					&ipanic_emmc1_fops);
		} else if (idx_log == IPANIC_LOG_GBUFFER) {
			idx_proc = PROC_GBUFFER_INDEX;
			ctx->ipanic_proc_entry[PROC_GBUFFER_INDEX] =
				proc_create(ctx->ipanic_proc_entry_name
					[PROC_GBUFFER_INDEX], S_IFREG | S_IRUGO,
					NULL,
					&ipanic_emmc_gbuffer_fops);
		} else {
			/* No proc entry for this index */
			idx_proc = 0;
			continue;
		}
		if (!ctx->ipanic_proc_entry[idx_proc])
			pr_err("%s: failed creating proc file\n",
				__func__);
		else {
			proc_entry_created = 1;
			pr_info("%s: proc entry created: %s\n",
				__func__,
				ctx->ipanic_proc_entry_name[idx_proc]);
		}
	}

	if (!proc_entry_created)
		emmc_panic_erase(read_buf_ptr, &sect);

put_sector:
	put_dev_sector(sect);
	return;
out_err:
	ctx->emmc = NULL;
}

static void emmc_panic_notify_remove(void)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;

	if (ctx->emmc && ctx->emmc->part_dev) {
		put_device(ctx->emmc->part_dev);
		ctx->emmc->bdev = NULL;
	}

	ctx->emmc = NULL;
}

static int emmc_ipanic_writeflashpage(struct mmc_emergency_info *emmc,
				      loff_t to, const u_char *buf)
{
	int rc;
	size_t wlen = SECTOR_SIZE;

	if (to >= emmc->start_block + emmc->block_count) {
		pr_emerg("%s: panic partition is full.\n", __func__);
		return 0;
	}

	rc = emmc->write((char *)buf, (unsigned int)to);
	if (rc) {
		pr_emerg("%s: Error writing data to flash (%d)\n",
			__func__, rc);
		return rc;
	}

	return wlen;
}

/*
 * Writes the contents of the console to the specified offset in flash.
 * Returns number of bytes written
 */
static int emmc_ipanic_write_console(struct mmc_emergency_info *emmc,
				     unsigned int off, int *actual_size)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	int saved_oip, rc, block_shift = 0, bounce_idx = 0;
	size_t line_len = 0;
	bool ret;

	static unsigned char line[SECTOR_SIZE];

	*actual_size = 0;
	while (1) {
		saved_oip = oops_in_progress;
		oops_in_progress = 1;
		bounce_idx = 0;

		if (last_chunk_buf_len) {
			memcpy(ctx->bounce, last_chunk_buf, last_chunk_buf_len);
			bounce_idx += last_chunk_buf_len;
			last_chunk_buf_len = 0;
		}

		do {
			ret = kmsg_dump_get_line(&ipanic_dumper, false,
						 line, SECTOR_SIZE, &line_len);

			if (ret) {
				if (bounce_idx + line_len < SECTOR_SIZE) {
					memcpy(ctx->bounce + bounce_idx,
					       line, line_len);
					bounce_idx += line_len;
				} else {
					int len = SECTOR_SIZE - bounce_idx;
					memcpy(ctx->bounce + bounce_idx,
					       line, len);
					bounce_idx = SECTOR_SIZE;
					memcpy(last_chunk_buf,
					       line + len, line_len - len);
					last_chunk_buf_len = line_len - len;
				}
			}
		} while (ret && (bounce_idx != SECTOR_SIZE));

		oops_in_progress = saved_oip;

		/* If it is the last chunk, just copy it to last chunk
		 * buffer and exit loop.
		 */
		if (!ret) {
			/* Leave the last chunk for next writing */
			memcpy(last_chunk_buf, ctx->bounce, bounce_idx);
			last_chunk_buf_len = bounce_idx;
			break;
		}

		rc = emmc_ipanic_writeflashpage(emmc, off + block_shift,
						 ctx->bounce);
		if (rc <= 0) {
			pr_emerg("%s: Flash write failed (%d)\n",
				__func__, rc);
			return block_shift;
		}

		block_shift++;
		*actual_size += SECTOR_SIZE;
	}

	return block_shift;
}

static void emmc_ipanic_flush_lastchunk_emmc(loff_t to,
					     int *size_written,
					     int *sector_written)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct mmc_emergency_info *emmc = ctx->emmc;
	int rc = 0;

	if (last_chunk_buf_len) {
		memset(last_chunk_buf + last_chunk_buf_len, 0,
		       SECTOR_SIZE - last_chunk_buf_len);

		rc = emmc_ipanic_writeflashpage(emmc, to, last_chunk_buf);
		if (rc <= 0) {
			pr_emerg("emmc_ipanic: write last chunk failed (%d)\n",
				rc);
			return;
		}

		*size_written += last_chunk_buf_len;
		(*sector_written)++;
		last_chunk_buf_len = 0;
	}
	return;
}

static void emmc_ipanic_write_thread_func(void)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct mmc_emergency_info *emmc = ctx->emmc;
	int size_written;
	int thread_sector_count;

	thread_sector_count =
	    emmc_ipanic_write_console(emmc,
				      log_offset[IPANIC_LOG_THREADS] +
				      log_len[IPANIC_LOG_THREADS],
				      &size_written);
	if (thread_sector_count < 0) {
		pr_emerg("Error writing threads to panic log! (%d)\n",
			log_len[IPANIC_LOG_THREADS]);
		return;
	}
	log_size[IPANIC_LOG_THREADS] += size_written;
	log_len[IPANIC_LOG_THREADS] += thread_sector_count;

	/*reset the log buffer */
	log_buf_clear();
	kmsg_dump_rewind(&ipanic_dumper);
}

static void emmc_ipanic_write_logbuf(struct mmc_emergency_info *emmc, int log)
{
	/*
	 * Write the log data from the third block :
	 * - the first block is reserved for panic header
	 * - the second one is reserved for offset information
	 */
	log_offset[log] = emmc->start_block + 2;
	log_len[log] = emmc_ipanic_write_console(emmc, log_offset[log],
			&log_size[log]);
	if (log_size[log] < 0) {
		pr_emerg("Error writing console to panic log! (%d)\n",
			log_len[log]);
		log_size[log] = 0;
		log_len[log] = 0;
	}
	/* flush last chunk buffer for console */
	emmc_ipanic_flush_lastchunk_emmc(log_offset[log] +
					 log_len[log],
					 &log_size[log], &log_len[log]);
}

static void emmc_ipanic_write_calltrace(struct mmc_emergency_info *emmc,
					int log)
{
	log_offset[log] = log_offset[log - 1] + log_len[log - 1];
	/*
	 * config func_stream_emmc to emmc_ipanic_write_thread_func to
	 * stream thread call trace.
	 */
	log_buf_clear();
	kmsg_dump_rewind(&ipanic_dumper);
	func_stream_emmc = emmc_ipanic_write_thread_func;
	show_state_filter(0);

	/* flush last chunk buffer */
	emmc_ipanic_flush_lastchunk_emmc(log_offset[log] +
					 log_len[log],
					 &log_size[log], &log_len[log]);
}

static int emmc_ipanic_write_gbuffer_data(struct mmc_emergency_info *emmc,
					 struct g_buffer_header *gbuffer,
					 unsigned int off,
					 int *actual_size)
{
	int rc, block_shift = 0;
	size_t log_off = 0;
	size_t log_size;
	unsigned char *buf = gbuffer->base;

	if (gbuffer->head)
		/* has overflow */
		log_size = gbuffer->size;
	else
		/* no overflow */
		log_size = gbuffer->woff;

	while (log_off < log_size) {
		size_t size_copy = log_size - log_off;
		if (size_copy < SECTOR_SIZE) {
			/*
			 * flash page not complete, flushed with
			 * emmc_ipanic_flush_lastchunk_emmc
			 */
			memcpy(last_chunk_buf, buf + log_off, size_copy);
			last_chunk_buf_len = size_copy;
			break;
		}
		rc = emmc_ipanic_writeflashpage(emmc, off + block_shift,
						buf + log_off);
		if (rc <= 0) {
			pr_emerg("%s: Flash write failed (%d)\n",
				__func__, rc);
			return 0;
		}
		log_off += rc;
		block_shift++;
	}
	*actual_size = log_off;

	return block_shift;
}

static struct g_buffer_header gbuffer = {
	.base = NULL,
};

static void emmc_ipanic_write_gbuffer(struct mmc_emergency_info *emmc,
				    int log)
{
	struct g_buffer_header *m_gbuffer = &gbuffer;

	log_offset[log] = log_offset[log - 1] + log_len[log - 1];

	pr_info("write gbuffer data\n");
	if (!m_gbuffer->base) {
		pr_err("Ipanic error, no gbuffer data\n");
		return;
	}

	log_len[log] = emmc_ipanic_write_gbuffer_data(emmc, m_gbuffer,
						    log_offset[log],
						    &log_size[log]);
	if (log_len[log] < 0) {
		pr_emerg("Error writing gbuffer to panic log! (%d)\n",
			log_len[log]);
		log_size[log] = 0;
		log_len[log] = 0;
	}
	/* flush last chunk buffer */
	emmc_ipanic_flush_lastchunk_emmc(log_offset[log] + log_len[log],
					 &log_size[log], &log_len[log]);
	log_head[log] = m_gbuffer->head;
	log_woff[log] = m_gbuffer->woff;
	pr_info("write gbuffer data END\n");
}

/*
 * Exported in <linux/panic_gbuffer.h>
 */
void panic_set_gbuffer(struct g_buffer_header *buf)
{
	if (gbuffer.base) {
		pr_err("%s: gbuffer already set to 0x%p, can not set again",
		       __func__, gbuffer.base);
		return;
	}

	gbuffer.base = buf->base;
	gbuffer.size = buf->size;
	gbuffer.woff = buf->woff;
	gbuffer.head = buf->head;
}
EXPORT_SYMBOL(panic_set_gbuffer);

static void emmc_ipanic_write_pageheader(struct mmc_emergency_info *emmc)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct panic_header *hdr = (struct panic_header *)ctx->bounce;
	int wc;
	size_t len, total, max;

	memset(ctx->bounce, 0, SECTOR_SIZE);
	hdr->magic = PANIC_MAGIC;
	hdr->version = PHDR_VERSION;

	total = snprintf(hdr->panic, SECTOR_SIZE,
			"###Kernel panic###\n");

	max =  SECTOR_SIZE - offsetof(struct panic_header, panic) - total;
	kmsg_dump_get_buffer(&ipanic_dumper, false,  last_chunk_buf, max, &len);
	kmsg_dump_rewind(&ipanic_dumper);

	memcpy(hdr->panic + total, last_chunk_buf, len);
	hdr->log_size = len + total;

	/* Write header block */
	wc = emmc_ipanic_writeflashpage(emmc, emmc->start_block, ctx->bounce);
	if (wc <= 0) {
		pr_emerg("emmc_ipanic: Info write failed (%d)\n", wc);
		return;
	}
}

static void emmc_ipanic_clean_loginfo(struct mmc_emergency_info *emmc)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	int rc;

	memset(log_offset, 0, IPANIC_LOG_MAX * sizeof(int));
	memset(log_len, 0, IPANIC_LOG_MAX * sizeof(int));
	memset(log_size, 0, IPANIC_LOG_MAX * sizeof(int));

	memset(ctx->bounce, 0, SECTOR_SIZE);

	rc = emmc_ipanic_writeflashpage(emmc, emmc->start_block+1, ctx->bounce);
	if (rc <= 0) {
		pr_emerg("emmc_ipanic: Header write failed (%d)\n",
			rc);
		return;
	}
}

static void emmc_ipanic_write_loginfo(struct mmc_emergency_info *emmc, int newlog)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct log_info *info = (struct log_info *)ctx->bounce;
	int log = IPANIC_LOG_CONSOLE;
	int rc;

	if ((newlog < 0) || (newlog >= IPANIC_LOG_MAX))
		return;

	if (log_size[newlog] == 0)
		return;

	memset(ctx->bounce, 0, SECTOR_SIZE);
	/*Fill up log offset and size */
	while (log < IPANIC_LOG_MAX) {
		/*Configurate log offset and log size */
		info->log_offset[log] = (log_offset[log] - emmc->start_block)
		    << SECTOR_SIZE_SHIFT;
		info->log_length[log] = log_size[log];
		info->log_head[log] = log_head[log];
		info->log_woff[log] = log_woff[log];
		log++;
	}
	rc = emmc_ipanic_writeflashpage(emmc, emmc->start_block+1, ctx->bounce);
	if (rc <= 0) {
		pr_emerg("emmc_ipanic: Header write failed (%d)\n",
			rc);
		return;
	}
}

static int emmc_ipanic(struct notifier_block *this, unsigned long event,
		       void *ptr)
{
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct mmc_emergency_info *emmc;
	int rc, log;

	pr_emerg("panic notified\n");

	if (in_panic || disable_emmc_ipanic)
		return NOTIFY_DONE;

	in_panic = 1;

#ifdef CONFIG_PREEMPT
	/* Ensure that cond_resched() won't try to preempt anybody */
	add_preempt_count(PREEMPT_ACTIVE);
#endif
	touch_nmi_watchdog();

	if (!ctx)
		goto out;
	emmc = ctx->emmc;
	if (!emmc)
		goto out;
	if (ctx->hdr.magic) {
		pr_emerg("Crash partition in use!\n");
		goto out;
	}

	rc = emmc->init();
	if (rc) {
		/* String too long to fit on 1 80-char line */
		pr_emerg("%s %s, rc=%d\n",
			"Emmc emergency driver is",
			"not initialized successfully!",
			rc);
		goto out;
	}

	/* Prepare kmsg dumper */
	ipanic_dumper.active = 1;
	/* Rewind kmsg dumper */
	kmsg_dump_rewind(&ipanic_dumper);

	/* Write emmc ipanic partition header */
	emmc_ipanic_write_pageheader(emmc);
	/* Clean emmc ipanic sections offsets */
	emmc_ipanic_clean_loginfo(emmc);

	/*Write all buffer into emmc */
	log = IPANIC_LOG_CONSOLE;
	while (log < IPANIC_LOG_MAX) {
		/* Clear temporary buffer */
		memset(ctx->bounce, 0, SECTOR_SIZE);
		/* Log every buffer into emmc */
		switch (log) {
		case IPANIC_LOG_CONSOLE:
			emmc_ipanic_write_logbuf(emmc, log);
			break;
		case IPANIC_LOG_THREADS:
			emmc_ipanic_write_calltrace(emmc, log);
			break;
		case IPANIC_LOG_GBUFFER:
			emmc_ipanic_write_gbuffer(emmc, log);
			break;
		default:
			break;
		}
		/* Update emmc ipanic sections offsets */
		emmc_ipanic_write_loginfo(emmc, log);
		log++;
	}
	pr_info("Panic log data written done!\n");

	ipanic_dumper.active = 0;

out:
#ifdef CONFIG_PREEMPT
	sub_preempt_count(PREEMPT_ACTIVE);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call = emmc_ipanic,
	.priority = 100,
};

static int panic_dbg_set(void *data, u64 val)
{
	BUG();
	return -1;
}

DEFINE_SIMPLE_ATTRIBUTE(panic_dbg_fops, NULL, panic_dbg_set, "%llu\n");

static int match_dev_panic_part(struct device *dev, const void *data)
{
	struct hd_struct *part;
	const char *name = (char *)data;

	if (!name || !dev || dev->class != &block_class)
		return 0;

	part = dev_to_part(dev);

	return part->info && part->info->volname &&
		!strcmp(name, part->info->volname);
}

static int emmc_panic_partition_notify(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	struct device *dev = data;
	struct emmc_ipanic_data *ctx = &drv_ctx;
	struct mmc_emergency_info *emmc;
	struct gendisk *disk;

	if (!ctx) {
		pr_err("%s:invalid panic handler\n", __func__);
		return 0;
	}

	emmc = ctx->emmc;
	if (!emmc) {
		pr_err("%s:invalid emmc information\n", __func__);
		return 0;
	}

	switch (action) {
	case BUS_NOTIFY_ADD_DEVICE:
	case BUS_NOTIFY_BOUND_DRIVER:
		/* if emmc already found, exit the function */
		if (emmc->bdev)
			return 0;

		emmc->part_dev = class_find_device(&block_class, NULL,
						   emmc->part_label,
						   &match_dev_panic_part);
		if (emmc->part_dev) {
			emmc->part = dev_to_part(emmc->part_dev);
			if (!emmc->part) {
				pr_err("unable to get partition\n");
				goto put_dev;
			}

			disk = part_to_disk(emmc->part);
			if (!disk) {
				pr_err("unable to get disk\n");
				goto put_dev;
			}

			/* get whole disk */
			emmc->bdev = bdget_disk(disk, 0);
			if (!emmc->bdev) {
				pr_err("unable to get emmc block device\n");
				goto put_dev;
			}

			emmc->start_block = emmc->part->start_sect;
			emmc->block_count = emmc->part->nr_sects;

			pr_info("panic partition found, label:%s, device:%s\n",
				emmc->part_label, dev_name(emmc->part_dev));

			/* notify to add the panic device */
			emmc_panic_notify_add();

			atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);

			INIT_WORK(&proc_removal_work, emmc_ipanic_remove_proc_work);
		}
		break;
	case BUS_NOTIFY_DEL_DEVICE:
	case BUS_NOTIFY_UNBIND_DRIVER:
		if (match_dev_panic_part(dev, emmc->part_label)) {
			pr_info("bus notify removed device '%s', cleaning.\n", dev_name(dev));
			flush_scheduled_work();
			atomic_notifier_chain_unregister(&panic_notifier_list, &panic_blk);
			emmc_panic_notify_remove();
		}
		break;
	case BUS_NOTIFY_BIND_DRIVER:
	case BUS_NOTIFY_UNBOUND_DRIVER:
		/* Nothing to do here, but we don't want
		 * these actions to generate error messages,
		 * so we need to catch them
		 */
		break;
	default:
		pr_err("Unknown action (%lu) on %s\n",
			action, dev_name(dev));
		return 0;
	}
	return 1;

put_dev:
	put_device(emmc->part_dev);
	return 0;
}

static struct notifier_block panic_partition_notifier = {
	.notifier_call = emmc_panic_partition_notify,
};

void emmc_ipanic_stream_emmc(void)
{
	if (func_stream_emmc)
		(*func_stream_emmc) ();
}

EXPORT_SYMBOL(emmc_ipanic_stream_emmc);

static struct dentry *emmc_ipanic_d;
static struct dentry *emmc_ipanic_disable_d;

static int __init emmc_ipanic_init(void)
{
	/* initialization of drv_ctx */
	memset(&drv_ctx, 0, sizeof(drv_ctx));
	drv_ctx.emmc = &emmc_info;

	if (*part_label)
		strcpy(emmc_info.part_label, part_label);

	drv_ctx.ipanic_proc_entry_name = ipanic_proc_entry_name;
	drv_ctx.bounce = (void *)__get_free_page(GFP_KERNEL);

	bus_register_notifier(&pci_bus_type, &panic_partition_notifier);

	emmc_ipanic_d = debugfs_create_file("emmc_ipanic", 0644, NULL, NULL,
					    &panic_dbg_fops);
	emmc_ipanic_disable_d = debugfs_create_u32("disable_emmc_ipanic", 0644,
						   NULL, &disable_emmc_ipanic);

	pr_info("init success\n");

	return 0;
}

static void __exit emmc_ipanic_exit(void)
{
	debugfs_remove(emmc_ipanic_d);
	debugfs_remove(emmc_ipanic_disable_d);
	bus_unregister_notifier(&pci_bus_type, &panic_partition_notifier);
	flush_scheduled_work();
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_blk);
	emmc_panic_notify_remove();
}

module_init(emmc_ipanic_init);
module_exit(emmc_ipanic_exit);
