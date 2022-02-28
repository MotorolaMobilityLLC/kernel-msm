// SPDX-License-Identifier: GPL-2.0
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/pm_wakeup.h>

/*
 * /proc/wakeup_sources
 */

struct proc_dir_entry *g_proc_wakeup_sources;
int g_lock_wakeup_sources;

/**
 * print_wakeup_source_stats - Print wakeup source statistics information.
 * @m: seq_file to print the statistics into.
 * @ws: Wakeup source object to print the statistics for.
 */
static int print_wakeup_source_stats(struct seq_file *m,
				     struct wakeup_source *ws)
{
	unsigned long flags;
	ktime_t total_time;
	ktime_t max_time;
	unsigned long active_count;
	ktime_t active_time;
	ktime_t prevent_sleep_time;

	spin_lock_irqsave(&ws->lock, flags);

	total_time = ws->total_time;
	max_time = ws->max_time;
	prevent_sleep_time = ws->prevent_sleep_time;
	active_count = ws->active_count;
	if (ws->active) {
		ktime_t now = ktime_get();

		active_time = ktime_sub(now, ws->last_time);
		total_time = ktime_add(total_time, active_time);
		if (active_time > max_time)
			max_time = active_time;

		if (ws->autosleep_enabled)
			prevent_sleep_time = ktime_add(prevent_sleep_time,
				ktime_sub(now, ws->start_prevent_time));
	} else {
		active_time = 0;
	}

	seq_printf(m, "%-12s\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lld\t\t%lld\t\t%lld\t\t%lld\t\t%lld\n",
		   ws->name, active_count, ws->event_count,
		   ws->wakeup_count, ws->expire_count,
		   ktime_to_ms(active_time), ktime_to_ms(total_time),
		   ktime_to_ms(max_time), ktime_to_ms(ws->last_time),
		   ktime_to_ms(prevent_sleep_time));

	spin_unlock_irqrestore(&ws->lock, flags);

	return 0;
}

static void *wakeup_sources_stats_seq_start(struct seq_file *m,
					loff_t *pos)
{
	struct wakeup_source *ws;
	loff_t n = *pos;

	if (n == 0) {
		seq_puts(m, "name\t\tactive_count\tevent_count\twakeup_count\t"
			"expire_count\tactive_since\ttotal_time\tmax_time\t"
			"last_change\tprevent_suspend_time\n");
	}

	g_lock_wakeup_sources = wakeup_sources_read_lock();
	for_each_wakeup_source(ws) {
		if (n-- <= 0)
			return ws;
	}

	return NULL;
}

static void *wakeup_sources_stats_seq_next(struct seq_file *m,
					void *v, loff_t *pos)
{
	struct wakeup_source *ws = v;
	struct wakeup_source *next_ws = NULL;

	++(*pos);
	next_ws = wakeup_sources_walk_next(ws);

	return next_ws;
}

static void wakeup_sources_stats_seq_stop(struct seq_file *m, void *v)
{
	wakeup_sources_read_unlock(g_lock_wakeup_sources);
}

/**
 * wakeup_sources_stats_seq_show - Print wakeup sources statistics information.
 * @m: seq_file to print the statistics into.
 * @v: wakeup_source of each iteration
 */
static int wakeup_sources_stats_seq_show(struct seq_file *m, void *v)
{
	struct wakeup_source *ws = v;

	print_wakeup_source_stats(m, ws);

	return 0;
}

static const struct seq_operations wakeup_sources_stats_seq_ops = {
	.start = wakeup_sources_stats_seq_start,
	.next  = wakeup_sources_stats_seq_next,
	.stop  = wakeup_sources_stats_seq_stop,
	.show  = wakeup_sources_stats_seq_show,
};

static int __init proc_wakeup_sources_init(void)
{
	pr_debug("%s(), %d\n", __func__, __LINE__);
	g_proc_wakeup_sources = proc_create_seq("wakeup_sources",
		S_IRUGO, NULL, &wakeup_sources_stats_seq_ops);
	return 0;
}

static void __exit proc_wakeup_sources_exit(void)
{
	pr_debug("%s(), %d\n", __func__, __LINE__);
	proc_remove(g_proc_wakeup_sources);
}

module_init(proc_wakeup_sources_init);
module_exit(proc_wakeup_sources_exit);

MODULE_LICENSE("GPL");
