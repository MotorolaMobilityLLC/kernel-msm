/*
 * drivers/misc/logger_kernel.c
 *
 * A Kernel Logging Subsystem
 *
 * Copyright (C) 2013 Intel, Inc.
 *
 * This porgram is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/console.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/hardirq.h>
#include <asm/ioctls.h>
#include "logger.h"
#include "logger_kernel.h"

static DEFINE_SPINLOCK(log_lock);
static struct work_struct write_console_wq;

/*
 * copy_log_to_log1 - flush the remaining bytes of 'log' into 'log1'
 *
 * The caller needs to lock the kernel or/and hold log->mutex
 */
void copy_log_to_log1(struct logger_log *log,
					struct logger_log *log1)
{
	struct logger_reader *reader;
	size_t len, ret;

	list_for_each_entry(reader, &log->readers, list)
		while (log->w_off != reader->r_off) {

			ret = sizeof(struct logger_entry) +
				get_entry_msg_len(log, reader->r_off);

			fix_up_readers(log1, ret);

			/*
			 * We read from the log in two disjoint operations.
			 * First, we read from the current read head offset
			 * up to 'count' bytes or to the end of the log,
			 * whichever comes first.
			 */
			len = min(ret, log->size - reader->r_off);
			do_write_log(log1, log->buffer + reader->r_off, len);

			/*
			 * Second, we read any remaining bytes, starting back at
			 * the head of the log.
			 */
			if (ret != len)
				do_write_log(log1, log->buffer, ret - len);

			reader->r_off = logger_offset(log, reader->r_off + ret);
		}
}

static void flush_to_bottom_log(struct logger_log *log,
					const char *buf, unsigned int count)
{
	struct logger_entry header;
	char extendedtag[8] = "\4KERNEL\0";
	u64 ts;
	unsigned long rem_nsec;
	unsigned long flags;
	struct logger_plugin *plugin;
	struct timespec boottime;

	ts = local_clock();
	rem_nsec = do_div(ts, 1000000000);

	getboottime(&boottime);

	header.pid = current->tgid;
	header.tid = task_pid_nr(current);
	header.sec = boottime.tv_sec + ts;
	header.nsec = boottime.tv_nsec + rem_nsec;
	header.euid = current_euid();

	/* length is computed like this:
	 * 1 byte for the log priority (harcoded to 4 meaning INFO)
	 * 6 bytes for the tag string (harcoded to KERNEL)
	 * 1 byte added at the end of the tag required by logcat
	 * the length of the buf added into the kernel log buffer
	 * 1 byte added at the end of the buf required by logcat
	 */
	header.len = min_t(size_t, sizeof(extendedtag) + count + 1,
					LOGGER_ENTRY_MAX_PAYLOAD);
	header.hdr_size = sizeof(struct logger_entry);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return;

	if (oops_in_progress) {
		if (!spin_trylock_irqsave(&log_lock, flags))
			return;
	} else
		spin_lock_irqsave(&log_lock, flags);

	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));
	do_write_log(log, &extendedtag, sizeof(extendedtag));
	do_write_log(log, buf, header.len - sizeof(extendedtag) - 1);

	/* send this segment's payload to the plugins */
	list_for_each_entry(plugin, &log->plugins, list)
		plugin->write_seg((void *)buf,
				  header.len - sizeof(extendedtag) - 1,
				  false, /* not from user */
				  true,  /* start of msg */
				  true,  /* end of msg */
				  plugin->data);

	/* the write offset is updated to add the final extra byte */
	log->w_off = logger_offset(log, log->w_off + 1);

	spin_unlock_irqrestore(&log_lock, flags);
};


/*
 * update_log_from_bottom - copy bottom log buffer into a log buffer
 */
static void update_log_from_bottom(struct logger_log *log_orig,
					struct logger_log *log_dst)
{
	unsigned long flags;

	mutex_lock(&log_dst->mutex);
	spin_lock_irqsave(&log_lock, flags);

	copy_log_to_log1(log_orig, log_dst);

	spin_unlock_irqrestore(&log_lock, flags);
	mutex_unlock(&log_dst->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log_dst->wq);
}

/*
 * write_console - a write method for kernel logs
 */
static void write_console(struct work_struct *work)
{
	struct logger_log *log_bot = get_log_from_name(LOGGER_LOG_KERNEL_BOT);
	struct logger_log *log_kernel = get_log_from_name(LOGGER_LOG_KERNEL);

	update_log_from_bottom(log_bot, log_kernel);
}

static void
logger_console_write(struct console *console, const char *s, unsigned int count)
{
	struct logger_log *log_bot = get_log_from_name(LOGGER_LOG_KERNEL_BOT);
	struct logger_log *log_kernel = get_log_from_name(LOGGER_LOG_KERNEL);

	if (!log_bot)
		return;

	flush_to_bottom_log(log_bot, s, count);

	if (unlikely(!log_kernel))
		return;
	if (unlikely(!keventd_up()))
		return;
	if (!oops_in_progress && !in_nmi())
		schedule_work(&write_console_wq);
}

/* logger console uses CON_IGNORELEVEL that provides a way to ignore
 * the log level set in the kernel command line
 */

static struct console logger_console = {
	.name	= "logk",
	.write	= logger_console_write,
	.flags	= CON_PRINTBUFFER | CON_IGNORELEVEL,
	.index	= -1,
};

/*
 * Kernel bottom buffer must be allocated in console init phase to ensure
 * that the first kernel logs will be retrieved when the kernel log device
 * will be registered. The bottom buffer is not registered
 */
static int __init create_console_log(char *log_name, int size)
{
	int ret = 0;
	struct logger_log *log;
	unsigned char *buffer;

	buffer = vmalloc(size);
	if (buffer == NULL)
		return -ENOMEM;

	log = kzalloc(sizeof(struct logger_log), GFP_KERNEL);
	if (log == NULL) {
		ret = -ENOMEM;
		goto out_free_buffer;
	}
	log->buffer = buffer;

	log->misc.minor = MISC_DYNAMIC_MINOR;
	log->misc.name = kstrdup(log_name, GFP_KERNEL);
	if (log->misc.name == NULL) {
		ret = -ENOMEM;
		goto out_free_log;
	}

	init_waitqueue_head(&log->wq);
	INIT_LIST_HEAD(&log->readers);
	INIT_LIST_HEAD(&log->plugins);
	mutex_init(&log->mutex);
	log->w_off = 0;
	log->head = 0;
	log->size = size;

	INIT_LIST_HEAD(&log->logs);
	list_add_tail(&log->logs, &log_list);

	pr_info("created %luK log '%s'\n",
		(unsigned long) log->size >> 10, log->misc.name);

	return 0;

out_free_log:
	kfree(log);

out_free_buffer:
	vfree(buffer);
	return ret;
}

static int init_log_reader(const char *name)
{
	struct logger_reader *reader;
	struct logger_log *log = get_log_from_name(name);

	if (!log)
		return -ENODEV;

	reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
	if (!reader)
		return -ENOMEM;

	reader->log = log;

	INIT_LIST_HEAD(&reader->list);

	mutex_lock(&log->mutex);
	reader->r_off = log->head;
	list_add_tail(&reader->list, &log->readers);
	mutex_unlock(&log->mutex);

	return 0;
}

static int __init logger_console_init(void)
{
	int ret;

	INIT_WORK(&write_console_wq, write_console);

	ret = create_console_log(LOGGER_LOG_KERNEL_BOT, 256 * 1024);
	if (unlikely(ret))
		goto out;

	ret = init_log_reader(LOGGER_LOG_KERNEL_BOT);
	if (unlikely(ret))
		goto out;

	register_console(&logger_console);
	pr_info("register logcat console\n");
out:
	return ret;
}

console_initcall(logger_console_init);

static int __init logger_kernel_init(void)
{
	int ret;
	if (!(logger_console.flags & CON_ENABLED))
		return 0;

	ret = create_log(LOGGER_LOG_KERNEL, 256*1024);
	return ret;
}
device_initcall(logger_kernel_init);
