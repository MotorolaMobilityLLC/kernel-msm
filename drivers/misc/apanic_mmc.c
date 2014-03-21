/* drivers/misc/apanic_mmc.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2012 Motorola Mobility LLC.
 * Copyright (C) 2014 Motorola Mobility LLC.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/nmi.h>
#include <linux/console.h>
#include <linux/debugfs.h>
#include <linux/apanic_mmc.h>
#include <linux/module.h>

#include <asm/setup.h>
#include <mach/mmi_watchdog.h>
#include <linux/kmsg_dump.h>

/* from kernel/printk.c */

#define memdump_wdt_disable() do {} while (0)

#define MEMDUMP_PLABEL "memdump"

struct panic_header {
	u32 magic;
#define PANIC_MAGIC 0xdeadf00d

	u32 version;
#define PHDR_VERSION   0x01

	u32 console_offset;
	u32 console_length;

	u32 app_threads_offset;
	u32 app_threads_length;

	u32 threads_offset;
	u32 threads_length;
};

struct memdump_header {
#define MEMDUMP_MAGIC_LEN 8
	char magic[MEMDUMP_MAGIC_LEN];
#define MEMDUMP_MAGIC "MEM-DUMP"
	u32 version;
	struct timespec ts;
	/* dump contents */
	u32 sdram_offset;
	u32 sdram_length;
	u32 sram_offset;
	u32 sram_length;
};

struct apanic_data {
	struct panic_header curr;
	void *bounce;
	int buf_offset;
	int written;
	struct proc_dir_entry *apanic_console;
	struct proc_dir_entry *apanic_app_threads;
	struct proc_dir_entry *apanic_threads;
	struct proc_dir_entry *apanic_annotate;
	struct hd_struct *hd;
	struct raw_mmc_panic_ops *mmc_panic_ops;
	char *annotation;
	struct kmsg_dumper dump;
	u32 annotation_size;
};

static int start_apanic_threads;
static int emergency_dump_flag;
static struct apanic_data drv_ctx;
static struct work_struct proc_removal_work;
/* avoid collision of proc interface operation and proc removal */
static DEFINE_MUTEX(drv_mutex);

#ifdef CONFIG_APANIC_MMC_MEMDUMP
struct memdump_data {
	struct hd_struct *hd;
	struct raw_mmc_panic_ops *mmc_memdump_ops;
};

static struct memdump_data memdump_ctx;
#endif

#define APANIC_INVALID_OFFSET 0xFFFFFFFF

static void mmc_bio_complete(struct bio *bio, int err)
{
	complete((struct completion *) bio->bi_private);
}

static void apanic_mmc(struct kmsg_dumper *dumper,
			enum kmsg_dump_reason reason);

static int apanic_proc_read_mmc(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos, void *dat)
{
	int i, index = 0;
	int ret;
	int start_sect;
	int end_sect;
	size_t file_length;
	off_t file_offset;
	struct apanic_data *ctx = &drv_ctx;
	struct block_device *bdev;
	struct bio bio;
	struct bio_vec bio_vec;
	struct completion complete;
	struct page *page;
	struct device *dev = part_to_dev(drv_ctx.hd);
	loff_t offset = *ppos;

	if (!ctx->hd || !ctx->mmc_panic_ops)
		return -EBUSY;

	if (!count)
		return 0;

	mutex_lock(&drv_mutex);

	switch ((int) dat) {
	case 1:		/* apanic_console */
		file_length = ctx->curr.console_length;
		file_offset = ctx->curr.console_offset;
		break;
	case 2:		/* apanic_threads */
		file_length = ctx->curr.threads_length;
		file_offset = ctx->curr.threads_offset;
		break;
	case 3:	/* apanic_app_threads */
		file_length = ctx->curr.app_threads_length;
		file_offset = ctx->curr.app_threads_offset;
		break;
	default:
		pr_err("Bad dat (%d)\n", (int) dat);
		mutex_unlock(&drv_mutex);
		return -EINVAL;
	}

	if ((offset + count) > file_length) {
		mutex_unlock(&drv_mutex);
		return 0;
	}

	bdev = blkdev_get_by_dev(dev->devt, FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("apanic: open device failed with %ld\n", PTR_ERR(bdev));
		ret = PTR_ERR(bdev);
		goto out_err;
	}
	page = virt_to_page(ctx->bounce);

	start_sect = (file_offset + offset) / 512;
	end_sect = (file_offset + offset + count - 1) / 512;

	for (i = start_sect; i <= end_sect; i++) {
		bio_init(&bio);
		bio.bi_io_vec = &bio_vec;
		bio_vec.bv_page = page;
		bio_vec.bv_len = 512;
		bio_vec.bv_offset = 0;
		bio.bi_vcnt = 1;
		bio.bi_idx = 0;
		bio.bi_size = 512;
		bio.bi_bdev = bdev;
		bio.bi_sector = i;
		init_completion(&complete);
		bio.bi_private = &complete;
		bio.bi_end_io = mmc_bio_complete;
		submit_bio(READ, &bio);
		wait_for_completion(&complete);
		if (!test_bit(BIO_UPTODATE, &bio.bi_flags)) {
			ret = -EIO;
			goto out_err;
		}

		if ((i == start_sect)
		    && ((file_offset + offset) % 512 != 0)) {
			/* first sect, may be the only sect */
			ret = copy_to_user(buffer,
				ctx->bounce + (file_offset + offset)
			       % 512, min((unsigned long) count,
					  (unsigned long)
					  (512 -
					   (file_offset + offset) % 512)));
			index += min((unsigned long) count, (unsigned long)
				     (512 - (file_offset + offset) % 512));
		} else if ((i == end_sect)
			   && ((file_offset + offset + count)
			       % 512 != 0)) {
			/* last sect */
			ret = copy_to_user(buffer + index,
						ctx->bounce,
						(file_offset + offset +
						count) % 512);
		} else {
			/* middle sect */
			ret = copy_to_user(buffer + index, ctx->bounce, 512);
			index += 512;
		}
		if (ret)
			goto out_err;
	}
	blkdev_put(bdev, FMODE_READ);

	*ppos = offset + count;

	mutex_unlock(&drv_mutex);
	return count;

out_err:
	mutex_unlock(&drv_mutex);
	return ret;
}

static int apanic_con_proc_read_mmc(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	void *dat = (void *)1;
	return apanic_proc_read_mmc(file, buffer, count, ppos, dat);
}

static int apanic_th_proc_read_mmc(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	void *dat = (void *)2;
	return apanic_proc_read_mmc(file, buffer, count, ppos, dat);
}

static int apanic_app_th_proc_read_mmc(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	void *dat = (void *)3;
	return apanic_proc_read_mmc(file, buffer, count, ppos, dat);
}

static void mmc_panic_erase(void)
{
	int i = 0;
	struct apanic_data *ctx = &drv_ctx;
	struct block_device *bdev;
	struct bio bio;
	struct bio_vec bio_vec;
	struct completion complete;
	struct page *page;
	struct device *dev = part_to_dev(drv_ctx.hd);

	if (!ctx->hd || !ctx->mmc_panic_ops)
		goto out_err;

	bdev = blkdev_get_by_dev(dev->devt, FMODE_WRITE, NULL);
	if (IS_ERR(bdev)) {
		pr_err("apanic: open device failed with %ld\n", PTR_ERR(bdev));
		goto out_err;
	}
	page = virt_to_page(ctx->bounce);
	memset(ctx->bounce, 0, PAGE_SIZE);

	while (i < ctx->hd->nr_sects) {
		bio_init(&bio);
		bio.bi_io_vec = &bio_vec;
		bio_vec.bv_offset = 0;
		bio_vec.bv_page = page;
		bio.bi_vcnt = 1;
		bio.bi_idx = 0;
		bio.bi_sector = i;
		if (ctx->hd->nr_sects - i >= 8) {
			bio_vec.bv_len = PAGE_SIZE;
			bio.bi_size = PAGE_SIZE;
			i += 8;
		} else {
			bio_vec.bv_len = (ctx->hd->nr_sects - i) * 512;
			bio.bi_size = (ctx->hd->nr_sects - i) * 512;
			i = ctx->hd->nr_sects;
		}
		bio.bi_bdev = bdev;
		init_completion(&complete);
		bio.bi_private = &complete;
		bio.bi_end_io = mmc_bio_complete;
		submit_bio(WRITE, &bio);
		wait_for_completion(&complete);
	}
	blkdev_put(bdev, FMODE_WRITE);
out_err:
	return;
}

static void apanic_remove_proc_work(struct work_struct *work)
{
	struct apanic_data *ctx = &drv_ctx;

	mutex_lock(&drv_mutex);
	mmc_panic_erase();
	memset(&ctx->curr, 0, sizeof(struct panic_header));
	if (ctx->apanic_console) {
		remove_proc_entry("apanic_console", NULL);
		ctx->apanic_console = NULL;
	}
	if (ctx->apanic_threads) {
		remove_proc_entry("apanic_threads", NULL);
		ctx->apanic_threads = NULL;
	}
	if (ctx->apanic_app_threads) {
		remove_proc_entry("apanic_app_threads", NULL);
		ctx->apanic_app_threads = NULL;
	}
	mutex_unlock(&drv_mutex);
}

static int apanic_proc_write(struct file *file, const char __user *buffer,
			     size_t count, loff_t *offset)
{
	schedule_work(&proc_removal_work);
	return count;
}

static int apanic_save_annotation(const char *annotation, unsigned long len)
{
	struct apanic_data *ctx = &drv_ctx;
	char *buffer;
	size_t oldlen = 0;
	size_t totalen = 0;

	pr_debug("%s: %s (%lu)\n", __func__, annotation, len);
	if (ctx->annotation)
		oldlen = strlen(ctx->annotation);

	buffer = kmalloc(len + oldlen + 1, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	totalen = len + oldlen + 1;
	if (ctx->annotation) {
		strlcpy(buffer, ctx->annotation, totalen);
		kfree(ctx->annotation);
	} else
		buffer[0] = '\0';

	strlcat(buffer, annotation, totalen);
	if (ctx->apanic_annotate) {
		proc_set_size(ctx->apanic_annotate, strlen(buffer));
		ctx->annotation_size = strlen(buffer);
	}
	ctx->annotation = buffer;

	return (int)len;
}

int apanic_mmc_annotate(const char *annotation)
{
	size_t len;

	len = strlen(annotation);
	if (len == 0)
		return -EINVAL;

	return apanic_save_annotation(annotation, len);
}
EXPORT_SYMBOL(apanic_mmc_annotate);

static int apanic_proc_read_annotation(struct file *file, char __user *buffer,
				size_t count, loff_t *ppos)
{
	struct apanic_data *ctx = &drv_ctx;
	loff_t offset = *ppos;

	if (offset < 0)
		return -EINVAL;
	if (offset + count > ctx->annotation_size)
		count = ctx->annotation_size - offset;
	if (count <= 0)
		return -EINVAL;

	if (copy_to_user(buffer, ctx->annotation + offset, count))
		return -EFAULT;

	*ppos = offset + count;
	return count;
}

static int apanic_proc_annotate(struct file *file,
				const char __user *annotation,
				size_t count, loff_t *offset)
{
	return apanic_save_annotation(annotation, count);
}

static const struct file_operations apanic_con_fops = {
	.read	= apanic_con_proc_read_mmc,
	.write	= apanic_proc_write,
};
static const struct file_operations apanic_th_fops = {
	.read	= apanic_th_proc_read_mmc,
	.write	= apanic_proc_write,
};
static const struct file_operations apanic_app_th_fops = {
	.read	= apanic_app_th_proc_read_mmc,
	.write	= apanic_proc_write,
};
static const struct file_operations apanic_anno_fops = {
	.read	= apanic_proc_read_annotation,
	.write	= apanic_proc_annotate,
};

static void mmc_panic_notify_add(struct hd_struct *hd)
{
	struct apanic_data *ctx = &drv_ctx;
	struct panic_header *hdr = ctx->bounce;
	struct block_device *bdev;
	struct bio bio;
	struct bio_vec bio_vec;
	struct completion complete;
	struct page *page;
	struct device *dev = part_to_dev(hd);

	if (!ctx->mmc_panic_ops) {
		pr_err("apanic: found apanic partition,without initialized\n");
		return;
	}

	bdev = blkdev_get_by_dev(dev->devt, FMODE_WRITE, NULL);
	if (IS_ERR(bdev)) {
		pr_err("apanic: open device failed with %ld\n",
		       PTR_ERR(bdev));
		goto out;
	}

	ctx->dump.max_reason = KMSG_DUMP_PANIC;
	ctx->dump.dump = apanic_mmc;
	if (kmsg_dump_register(&ctx->dump) < 0) {
		pr_err("apanic: kmsg_dumper register failed\n");
		goto out;

	}
	ctx->hd = hd;
	page = virt_to_page(ctx->bounce);

	bio_init(&bio);
	bio.bi_io_vec = &bio_vec;
	bio_vec.bv_page = page;
	bio_vec.bv_len = PAGE_SIZE;
	bio_vec.bv_offset = 0;
	bio.bi_vcnt = 1;
	bio.bi_idx = 0;
	bio.bi_size = PAGE_SIZE;
	bio.bi_bdev = bdev;
	bio.bi_sector = 0;
	init_completion(&complete);
	bio.bi_private = &complete;
	bio.bi_end_io = mmc_bio_complete;
	submit_bio(READ, &bio);
	wait_for_completion(&complete);

	blkdev_put(bdev, FMODE_READ);

	pr_err("apanic: Bound to mmc block device '%s(%u:%u)'\n",
	       dev_name(dev), MAJOR(dev->devt), MINOR(dev->devt));

	if (hdr->magic != PANIC_MAGIC) {
		pr_info("apanic: No panic data available\n");
		goto out;
	}

	if (hdr->version != PHDR_VERSION) {
		pr_info("apanic: Version mismatch (%d != %d)\n",
		       hdr->version, PHDR_VERSION);
		goto out;
	}

	memcpy(&ctx->curr, hdr, sizeof(struct panic_header));

	pr_info("apanic: c(%u, %u) t(%u, %u) a(%u, %u)\n",
	       hdr->console_offset, hdr->console_length,
	       hdr->threads_offset, hdr->threads_length,
	       hdr->app_threads_offset, hdr->app_threads_length);

	if (hdr->console_length) {
		ctx->apanic_console = proc_create("apanic_console",
							S_IFREG | S_IRUGO,
							NULL,
							&apanic_con_fops);
		if (!ctx->apanic_console)
			pr_err("apanic: failed creating procfile\n");
		else
			proc_set_size(ctx->apanic_console, hdr->console_length);
	}

	if (hdr->threads_length) {
		ctx->apanic_threads = proc_create("apanic_threads",
							S_IFREG | S_IRUGO,
							NULL,
							&apanic_th_fops);
		if (!ctx->apanic_threads)
			pr_err("apanic: failed creating procfile\n");
		else
			proc_set_size(ctx->apanic_threads, hdr->threads_length);
	}

	if (hdr->app_threads_length) {
		ctx->apanic_app_threads = proc_create(
						"apanic_app_threads",
						S_IFREG | S_IRUGO,
						NULL,
						&apanic_app_th_fops);
		if (!ctx->apanic_app_threads)
			pr_err("%s: failed creating procfile\n", __func__);
		else
			proc_set_size(ctx->apanic_app_threads,
					hdr->app_threads_length);

	}

out:
	ctx->apanic_annotate = proc_create("apanic_annotate",
				S_IFREG | S_IRUGO | S_IWUSR,
				NULL,
				&apanic_anno_fops);
	if (!ctx->apanic_annotate)
		pr_err("%s: failed creating procfile\n", __func__);
	else {
		proc_set_size(ctx->apanic_annotate, 0);
		ctx->annotation_size = 0;
	}

	return;
}

static void mmc_panic_notify_remove(struct hd_struct *hd)
{
	struct device *dev = part_to_dev(hd);

	if (kmsg_dump_unregister(&drv_ctx.dump) < 0)
		pr_err("apanic_mmc: could not unregister kmsg_dumper\n");

	drv_ctx.hd = NULL;
	pr_info(KERN_INFO "apanic: Unbound from %s\n", dev_name(dev));
}

#ifdef CONFIG_APANIC_MMC_MEMDUMP
static void mmc_memdump_notify_add(struct hd_struct *hd)
{
	struct device *dev = part_to_dev(hd);

	/* no need to disturb the backing device now */
	memdump_ctx.hd = hd;
	pr_err("apanic: memdump backing device set to '%s(%u:%u)'\n",
			dev_name(dev), MAJOR(dev->devt), MINOR(dev->devt));

	return;
}

static void mmc_memdump_notify_remove(struct hd_struct *hd)
{
	struct device *dev = part_to_dev(hd);

	memdump_ctx.hd = NULL;
	pr_info("apanic: Unbound from memdump backing dev" " %s\n",
			dev_name(dev));
}
#endif				/* CONFIG_APANIC_MMC_MEMDUMP */

static int in_panic;

/*
 * Writes the contents of the console to the specified offset in mmc.
 * Returns number of bytes written
 */
static int apanic_write_console_mmc(unsigned int off,
					struct kmsg_dumper *dumper)
{
	struct apanic_data *ctx = &drv_ctx;
	int saved_oip;
	int idx = 0;
	int rc, rc2;
	int totallen = 0;
	int len;
	int n = 0;
	int copied = 0;

	if (!ctx->hd || !ctx->mmc_panic_ops)
		return -EBUSY;

	kmsg_dump_rewind_nolock(dumper);
	while (kmsg_dump_get_line_nolock(dumper, 1, NULL, 0, &len)) {
		n++;
		totallen += len;
	}

	kmsg_dump_rewind_nolock(dumper);
	while (copied < totallen) {
		saved_oip = oops_in_progress;
		oops_in_progress = 1;

		if (!kmsg_dump_get_buffer_panic(dumper, 1, ctx->bounce,
						PAGE_SIZE, &rc))
			break;

		if (rc <= 0)
			break;

		copied += rc;

		oops_in_progress = saved_oip;

		if (rc <= 0)
			break;
		if (rc != PAGE_SIZE) {
			memset(ctx->bounce + rc, 0, PAGE_SIZE - rc - 1);
			memset(ctx->bounce  + PAGE_SIZE - 1, '\n', 1);
		}

		rc2 =
		    ctx->mmc_panic_ops->panic_write(ctx->hd,
						    ctx->bounce, off, rc);
		if (rc2 <= 0) {
			pr_emerg("apanic: Flash write failed (%d)\n", rc2);
			return idx;
		}
		idx += rc2;
		off += rc2;
	}
	return idx;
}

int is_apanic_threads_dump(void)
{
	return start_apanic_threads;
}

int is_emergency_dump(void)
{
	return emergency_dump_flag;
}

void emergency_dump(void)
{
	struct apanic_data *ctx = &drv_ctx;
	struct kmsg_dumper *dumper = &drv_ctx.dump;

	emergency_dump_flag = 1;
	ctx->buf_offset = ALIGN(ctx->written, 512);
	ctx->written += apanic_write_console_mmc(ctx->buf_offset, dumper);
	emergency_dump_flag = 0;
}

extern int do_syslog(int type, char __user *bug, int count);
static void apanic_mmc_logbuf_dump(struct kmsg_dumper *dumper)
{
	struct apanic_data *ctx = &drv_ctx;
	struct panic_header *hdr = (struct panic_header *) ctx->bounce;
	int console_offset = 0;
	int console_len = 0;
	int threads_offset = 0;
	int threads_len = 0;
	int app_threads_offset = 0;
	int app_threads_len = 0;
	int rc = 0;
	struct timespec now;
	struct timespec uptime;
	struct rtc_time rtc_timestamp;
	struct console *con;

	if (!ctx->hd || !ctx->mmc_panic_ops ||
	    !ctx->mmc_panic_ops->panic_probe)
		return;
	if (ctx->mmc_panic_ops->panic_probe(ctx->hd,
					    ctx->mmc_panic_ops->type)) {
		pr_err("apanic: choose to use emmc, but card not detected\n");
		return;
	}
	console_offset = 1024;

	if (ctx->curr.magic) {
		pr_emerg("Crash partition in use!\n");
		return;
	}

	/*
	 * Add timestamp to displays current UTC time and uptime (in seconds).
	 */
	now = current_kernel_time();
	rtc_time_to_tm((unsigned long) now.tv_sec, &rtc_timestamp);
	do_posix_clock_monotonic_gettime(&uptime);
	bust_spinlocks(1);
	pr_emerg("Timestamp = %lu.%03lu\n",
	       (unsigned long) now.tv_sec,
	       (unsigned long) (now.tv_nsec / 1000000));
	pr_emerg("Current Time = "
	       "%02d-%02d %02d:%02d:%lu.%03lu, "
	       "Uptime = %lu.%03lu seconds\n",
	       rtc_timestamp.tm_mon + 1, rtc_timestamp.tm_mday,
	       rtc_timestamp.tm_hour, rtc_timestamp.tm_min,
	       (unsigned long) rtc_timestamp.tm_sec,
	       (unsigned long) (now.tv_nsec / 1000000),
	       (unsigned long) uptime.tv_sec,
	       (unsigned long) (uptime.tv_nsec / USEC_PER_SEC));
	bust_spinlocks(0);

	if (ctx->annotation)
		pr_emerg("%s\n", ctx->annotation);

	touch_hw_watchdog();
	/*
	 * Write out the console
	 */
	console_len = apanic_write_console_mmc(console_offset, dumper);
	if (console_len < 0) {
		pr_emerg("Error writing console to panic log! (%d)\n",
				console_len);
		console_len = 0;
	}

	/*
	 * Write out all threads
	 */
	app_threads_offset = (ALIGN(console_offset + console_len,
				1024) == 0) ? 1024 :
	    ALIGN(console_offset + console_len, 1024);

	/*
	 * Clear ring buffer
	 */
	do_syslog(5 /* clear */, NULL, 0);

	for (con = console_drivers; con; con = con->next)
		con->flags &= ~CON_ENABLED;

	ctx->buf_offset = app_threads_offset;
	ctx->written = app_threads_offset;
	start_apanic_threads = 1;
/*
	if (tracing_get_trace_buf_size() < (SZ_512K + 1))
		ftrace_dump(1);
*/
	show_state_thread_filter(0, SHOW_APP_THREADS);
	ctx->buf_offset = ALIGN(ctx->written, 512);
	start_apanic_threads = 0;
	ctx->written += apanic_write_console_mmc(ctx->buf_offset, dumper);
	app_threads_len = ctx->written - app_threads_offset;

	touch_hw_watchdog();

	/*
	 * Clear ring buffer
	 */
	do_syslog(5 /* clear */, NULL, 0);

	threads_offset = ALIGN(ctx->written, 512);
	ctx->buf_offset = threads_offset;
	ctx->written = threads_offset;
	start_apanic_threads = 1;
	show_state_thread_filter(0, SHOW_KTHREADS);
	start_apanic_threads = 0;
	ctx->buf_offset = ALIGN(ctx->written, 512);
	ctx->written += apanic_write_console_mmc(ctx->buf_offset, dumper);
	threads_len = ctx->written - threads_offset + 512;

	touch_hw_watchdog();

	for (con = console_drivers; con; con = con->next)
		con->flags |= CON_ENABLED;

	/*
	 * Finally write the panic header
	 */
	memset(ctx->bounce, 0, PAGE_SIZE);
	hdr->magic = PANIC_MAGIC;
	hdr->version = PHDR_VERSION;

	hdr->console_offset = console_offset;
	hdr->console_length = console_len;

	hdr->app_threads_offset = app_threads_offset;
	hdr->app_threads_length = app_threads_len;

	hdr->threads_offset = threads_offset;
	hdr->threads_length = threads_len;

	rc = ctx->mmc_panic_ops->panic_write(ctx->hd, ctx->bounce, 0,
					     console_offset);
	if (rc <= 0) {
		pr_emerg("apanic: Header write failed (%d)\n", rc);
		return;
	}

	pr_emerg("apanic: Panic dump successfully written\n");
}

#ifdef CONFIG_APANIC_MMC_MEMDUMP
static void apanic_mmc_memdump(void)
{
	int rc;
	struct membank *bank;
	struct apanic_data *ctx = &drv_ctx;
	struct memdump_header *hdr =
	    (struct memdump_header *) drv_ctx.bounce;

	if (!memdump_ctx.hd || !memdump_ctx.mmc_memdump_ops ||
	    !memdump_ctx.mmc_memdump_ops->panic_probe)
		return;

	memdump_wdt_disable();
	if (memdump_ctx.mmc_memdump_ops->
	    panic_probe(memdump_ctx.hd,
			memdump_ctx.mmc_memdump_ops->type)) {
		pr_err("apanic: full memeory dump backing device"
		       " not detected, fail to do memory dump\n");
		return;
	}

	memset(ctx->bounce, 0, PAGE_SIZE);
	rc = memdump_ctx.mmc_memdump_ops->panic_write(memdump_ctx.
						      hd,
						      ctx->bounce, 0,
						      PAGE_SIZE);
	if (rc <= 0) {
		pr_emerg("apanic: memdump erase header failed (%d)\n", rc);
		return;
	}

	/* no need to flush_cache_all(); */
	/* get the last memory bank, probably the only bank */
	if (meminfo.nr_banks >= 1 && meminfo.nr_banks < NR_BANKS)
		bank = &meminfo.bank[meminfo.nr_banks - 1];

	rc = memdump_ctx.mmc_memdump_ops->panic_write(memdump_ctx.
						      hd,
						      __va(bank->start),
						      PAGE_SIZE,
						      bank->size);
	if (rc <= 0) {
		pr_emerg("apanic: full memory write failed rc=%d)\n", rc);
		return;
	}

	strlcpy(hdr->magic, MEMDUMP_MAGIC, MEMDUMP_MAGIC_LEN);
	hdr->version = PHDR_VERSION;
	hdr->ts = current_kernel_time();
	hdr->sdram_offset = PAGE_SIZE;
	hdr->sdram_length = bank->size;

	rc = memdump_ctx.mmc_memdump_ops->panic_write(memdump_ctx.
						      hd,
						      ctx->bounce, 0,
						      PAGE_SIZE);
	if (rc <= 0) {
		pr_emerg("apanic: memdump header write failed (%d)\n", rc);
		return;
	}

	pr_emerg("apanic: full memory dump successfully written\n");
}
#endif				/* CONFIG_APANIC_MMC_MEMDUMP */

static void apanic_mmc(struct kmsg_dumper *dumper, enum kmsg_dump_reason reason)
{
	if (in_panic)
		return;
	in_panic = 1;
#ifdef CONFIG_PREEMPT
	/* Ensure that cond_resched() won't try to preempt anybody */
	add_preempt_count(PREEMPT_ACTIVE);
#endif
	touch_softlockup_watchdog();

	apanic_mmc_logbuf_dump(dumper);

#ifdef CONFIG_APANIC_MMC_MEMDUMP
	apanic_mmc_memdump();
#endif

#ifdef CONFIG_PREEMPT
	sub_preempt_count(PREEMPT_ACTIVE);
#endif
	in_panic = 0;
	return;
}

static int panic_dbg_get(void *data, u64 *val)
{
	struct kmsg_dumper *dumper = &drv_ctx.dump;
	apanic_mmc(dumper, KMSG_DUMP_PANIC);
	return 0;
}

static int panic_dbg_set(void *data, u64 val)
{
	BUG();
	return -EIO;
}

DEFINE_SIMPLE_ATTRIBUTE(panic_dbg_fops, panic_dbg_get, panic_dbg_set,
			"%llu\n");

#ifdef CONFIG_APANIC_MMC_MEMDUMP
static void apanic_mmc_memdump_add(struct hd_struct *part)
{
	if (strncmp(part->info->volname, MEMDUMP_PLABEL, BDEVNAME_SIZE)
			== 0) {
		if (memdump_ctx.hd == part)
			return;

		if (memdump_ctx.hd) {
			pr_err("apanic: found duplicate memdump partition: "
					"%s\n", dev_name(dev));
			return;
		}

		mmc_memdump_notify_add(part);
	}
}
static void apanic_mmc_memdump_remove(struct hd_struct *part)
{
	if (part == memdump_ctx.hd)
		mmc_memdump_notify_remove(part);
}
#else
static inline void apanic_mmc_memdump_add(struct hd_struct *part) {}
static inline void apanic_mmc_memdump_remove(struct hd_struct *part) {}
#endif
void apanic_mmc_partition_add(struct hd_struct *part)
{
	struct device *dev = part_to_dev(part);

	if (!part->info || !part->info->volname[0])
		return;

	if (strncmp(part->info->volname, CONFIG_APANIC_PLABEL, BDEVNAME_SIZE)
			== 0) {
		if (drv_ctx.hd == part)
			return;

		if (drv_ctx.hd) {
			pr_err("apanic: found duplicate panic partition: %s\n",
					dev_name(dev));
			return;
		}

		mmc_panic_notify_add(part);
	}

	apanic_mmc_memdump_add(part);
}

void apanic_mmc_partition_remove(struct hd_struct *part)
{
	if (part == drv_ctx.hd)
		mmc_panic_notify_remove(part);

	apanic_mmc_memdump_remove(part);
}

int __init apanic_mmc_init(struct raw_mmc_panic_ops *panic_ops)
{

	debugfs_create_file("apanic_mmc", 0644, NULL, NULL,
			    &panic_dbg_fops);

	drv_ctx.bounce = (void *) __get_free_page(GFP_KERNEL);

	drv_ctx.mmc_panic_ops = panic_ops;
#ifdef CONFIG_APANIC_MMC_MEMDUMP
	memdump_ctx.mmc_memdump_ops = panic_ops;
#endif

	INIT_WORK(&proc_removal_work, apanic_remove_proc_work);

	pr_info("Android kernel panic mmc handler initialized\n");

	return 0;
}
