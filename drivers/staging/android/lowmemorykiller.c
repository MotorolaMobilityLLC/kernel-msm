/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
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

#define REALLY_WANT_TRACEPOINTS
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/swap.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <trace/events/memkill.h>

static uint32_t lowmem_debug_level = 1;
static int lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
static int lmk_fast_run = 1;

static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			printk(x);			\
	} while (0)

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t = p;

	do {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			return 1;
		}
		task_unlock(t);
	} while_each_thread(p, t);

	return 0;
}

/*
 * The # of pages, that should be reduced for each zone, will be
 * minfree * p. and  p = (zone_size)/total_size * 100%.
 * Here is the algorithm of how to calculate it:
 *
 *     there is always such n that n/2^k < p < (n+1)/2^k
 *     so  n = 2^k * p; and k is the precision, set it 16.
 *
 *     Then, minfree * p =  (minfree * n) >> k
 */
#define LOWMEM_ZONE_ADJ_PRECISION 16
static int lowmem_zone_n[MAX_NR_ZONES];

static void lowmem_zone_adj_init(void)
{
	unsigned long int total_pages;
	unsigned long long int p;
	struct zone *zone;

	if (MAX_NR_ZONES == 1) /* if there is only one zone, just return */
		return;

	total_pages = 0;
	for_each_zone(zone)
		total_pages += zone->present_pages;

	lowmem_print(2, "%s: total pages = %lu in %d zones\n", __func__,
		total_pages, MAX_NR_ZONES);

	for_each_zone(zone) {

		p = (zone->present_pages << 10)/total_pages;
		lowmem_zone_n[zone_idx(zone)] =
			(int)((p << LOWMEM_ZONE_ADJ_PRECISION) >> 10);

		lowmem_print(2, "%s: zone (%s, %d) present=%lu,"
				"(0.%03lld, n=%d)\n",
				__func__, zone->name,
				zone_idx(zone), zone->present_pages,
				p, lowmem_zone_n[zone_idx(zone)]);
	}
}
static int lowmem_zone_adj(int min_size_indx, int high_zoneidx)
{
	int s, minfree, i;
	unsigned long long t;

	if (high_zoneidx >= MAX_NR_ZONES - 1)
		return 0;

	minfree = lowmem_minfree[min_size_indx];

	s = 0;
	for (i = MAX_NR_ZONES - 1; i > high_zoneidx; i--) {
		t = (unsigned long long) minfree * lowmem_zone_n[i];
		s += (int) t >> LOWMEM_ZONE_ADJ_PRECISION;
	}

	lowmem_print(5, "%s: original min = %d, high_indx = %d, reduce = %d\n",
			__func__, minfree, high_zoneidx, s);
	return s;
}

int can_use_cma_pages(gfp_t gfp_mask)
{
	int can_use = 0;
	int mtype = allocflags_to_migratetype(gfp_mask);
	int i = 0;
	int *mtype_fallbacks = get_migratetype_fallbacks(mtype);

	if (is_migrate_cma(mtype)) {
		can_use = 1;
	} else {
		for (i = 0;; i++) {
			int fallbacktype = mtype_fallbacks[i];

			if (is_migrate_cma(fallbacktype)) {
				can_use = 1;
				break;
			}

			if (fallbacktype == MIGRATE_RESERVE)
				break;
		}
	}
	return can_use;
}

struct zone_avail {
	unsigned long free;
	unsigned long file;
};


void tune_lmk_zone_param(struct zonelist *zonelist, int classzone_idx,
					int *other_free, int *other_file,
					int use_cma_pages,
				struct zone_avail zall[][MAX_NR_ZONES])
{
	struct zone *zone;
	struct zoneref *zoneref;
	int zone_idx;

	for_each_zone_zonelist(zone, zoneref, zonelist, MAX_NR_ZONES) {
		struct zone_avail *za;
		int node_idx = zone_to_nid(zone);

		zone_idx = zonelist_zone_idx(zoneref);
		za = &zall[node_idx][zone_idx];
		za->free = zone_page_state(zone, NR_FREE_PAGES);
		za->file = zone_page_state(zone, NR_FILE_PAGES)
					- zone_page_state(zone, NR_SHMEM);
		if (zone_idx == ZONE_MOVABLE) {
			if (!use_cma_pages) {
				unsigned long free_cma = zone_page_state(zone,
						NR_FREE_CMA_PAGES);
				za->free -= free_cma;
				*other_free -= free_cma;
			}
			continue;
		}

		if (zone_idx > classzone_idx) {
			if (other_free != NULL)
				*other_free -= za->free;
			if (other_file != NULL)
				*other_file -= za->file;
			za->free = za->file = 0;
		} else if (zone_idx < classzone_idx) {
			if (zone_watermark_ok(zone, 0, 0, classzone_idx, 0)) {
				unsigned long lowmem_reserve =
					  zone->lowmem_reserve[classzone_idx];
				if (!use_cma_pages) {
					unsigned long free_cma =
						zone_page_state(zone,
							NR_FREE_CMA_PAGES);
					unsigned long delta =
						min(lowmem_reserve + free_cma,
							za->free);
					*other_free -= delta;
					za->free -= delta;
				} else {
					*other_free -= lowmem_reserve;
					za->free -= lowmem_reserve;
				}
			} else {
				*other_free -= za->free;
				za->free = 0;
			}
		}
	}
}

void tune_lmk_param(int *other_free, int *other_file, struct shrink_control *sc,
				struct zone_avail zall[][MAX_NR_ZONES])
{
	gfp_t gfp_mask;
	struct zone *preferred_zone;
	struct zonelist *zonelist;
	enum zone_type high_zoneidx, classzone_idx;
	unsigned long balance_gap;
	int use_cma_pages;
	struct zone_avail *za;

	gfp_mask = sc->gfp_mask;
	zonelist = node_zonelist(0, gfp_mask);
	high_zoneidx = gfp_zone(gfp_mask);
	first_zones_zonelist(zonelist, high_zoneidx, NULL, &preferred_zone);
	classzone_idx = zone_idx(preferred_zone);
	use_cma_pages = can_use_cma_pages(gfp_mask);
	za = &zall[zone_to_nid(preferred_zone)][classzone_idx];

	balance_gap = min(low_wmark_pages(preferred_zone),
			  (preferred_zone->present_pages +
			   KSWAPD_ZONE_BALANCE_GAP_RATIO-1) /
			   KSWAPD_ZONE_BALANCE_GAP_RATIO);

	if (likely(current_is_kswapd() && zone_watermark_ok(preferred_zone, 0,
			  high_wmark_pages(preferred_zone) + SWAP_CLUSTER_MAX +
			  balance_gap, 0, 0))) {
		if (lmk_fast_run)
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       other_file, use_cma_pages, zall);
		else
			tune_lmk_zone_param(zonelist, classzone_idx, other_free,
				       NULL, use_cma_pages, zall);

		if (zone_watermark_ok(preferred_zone, 0, 0, ZONE_HIGHMEM, 0)) {
			unsigned long lowmem_reserve =
				preferred_zone->lowmem_reserve[ZONE_HIGHMEM];
			if (!use_cma_pages) {
				unsigned long free_cma = zone_page_state(
					preferred_zone, NR_FREE_CMA_PAGES);
				unsigned long delta = min(lowmem_reserve +
					free_cma, za->free);
				*other_free -= delta;
				za->free -= delta;
			} else {
				*other_free -= lowmem_reserve;
				za->free -= lowmem_reserve;
			}
		} else {
			*other_free -= za->free;
			za->free = 0;
		}

		lowmem_print(4, "lowmem_shrink of kswapd tunning for highmem "
			     "ofree %d, %d\n", *other_free, *other_file);
	} else {
		tune_lmk_zone_param(zonelist, classzone_idx, other_free,
			       other_file, use_cma_pages, zall);

		if (!use_cma_pages) {
			unsigned long free_cma = zone_page_state(preferred_zone,
						NR_FREE_CMA_PAGES);
			*other_free -= free_cma;
			za->free -= free_cma;
		}

		lowmem_print(4, "lowmem_shrink tunning for others ofree %d, "
			     "%d\n", *other_free, *other_file);
	}
}

static DEFINE_MUTEX(scan_mutex);

static int lowmem_shrink(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int i;
	int min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int selected_tasksize = 0;
	int selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free;
	int other_file;
	int high_zoneidx = gfp_zone(sc->gfp_mask);
	int zone_adj;
	unsigned long nr_to_scan = sc->nr_to_scan;
	struct zone_avail zall[MAX_NUMNODES][MAX_NR_ZONES];

	if (nr_to_scan > 0) {
		if (mutex_lock_interruptible(&scan_mutex) < 0)
			return 0;
	}

	other_free = global_page_state(NR_FREE_PAGES);
	other_file = global_page_state(NR_FILE_PAGES) -
						global_page_state(NR_SHMEM);

	memset(zall, 0, sizeof(zall));
	tune_lmk_param(&other_free, &other_file, sc, zall);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {

		zone_adj = lowmem_zone_adj(i, high_zoneidx);

		if (other_free < (lowmem_minfree[i] - zone_adj) &&
		    other_file < (lowmem_minfree[i] - zone_adj)) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}
	if (nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %lu, %x, ofree %d %d, ma %d\n",
				nr_to_scan, sc->gfp_mask, other_free,
				other_file, min_score_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (nr_to_scan <= 0 || min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %lu, %x, return %d\n",
			     nr_to_scan, sc->gfp_mask, rem);

		if (nr_to_scan > 0)
			mutex_unlock(&scan_mutex);

		return rem;
	}
	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		int oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		/* if task no longer has any memory ignore it */
		if (test_task_flag(tsk, TIF_MM_RELEASED))
			continue;

		if (time_before_eq(jiffies, lowmem_deathpending_timeout)) {
			if (test_task_flag(tsk, TIF_MEMDIE)) {
				rcu_read_unlock();
				/* give the system time to free up the memory */
				msleep_interruptible(20);
				mutex_unlock(&scan_mutex);
				return 0;
			}
		}

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		if (fatal_signal_pending(p)) {
			lowmem_print(2, "skip slow dying process %d\n", p->pid);
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_score_adj, tasksize);
	}
	if (selected) {
		int i, j;
		char zinfo[ZINFO_LENGTH];
		char *p = zinfo;
		lowmem_print(1, "send sigkill to %d (%s), adj %d, size %d\n",
			     selected->pid, selected->comm,
			     selected_oom_score_adj, selected_tasksize);
		lowmem_deathpending_timeout = jiffies + HZ;
		for (i = 0; i < MAX_NUMNODES; i++)
			for (j = 0; j < MAX_NR_ZONES; j++)
				if (zall[i][j].free || zall[i][j].file)
					p += snprintf(p, ZINFO_DIGITS,
						"%d:%d:%lu:%lu ", i, j,
						zall[i][j].free,
						zall[i][j].file);

		trace_lmk_kill(selected->pid, selected->comm,
				selected_oom_score_adj, selected_tasksize,
				min_score_adj, sc->gfp_mask, zinfo);
		send_sig(SIGKILL, selected, 0);
		set_tsk_thread_flag(selected, TIF_MEMDIE);
		rem -= selected_tasksize;
		rcu_read_unlock();
		/* give the system time to free up the memory */
		msleep_interruptible(20);
	} else
		rcu_read_unlock();

	lowmem_print(4, "lowmem_shrink %lu, %x, return %d\n",
		     nr_to_scan, sc->gfp_mask, rem);
	mutex_unlock(&scan_mutex);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	register_shrinker(&lowmem_shrinker);

	lowmem_zone_adj_init();

	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static int lowmem_oom_adj_to_oom_score_adj(int oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	int oom_adj;
	int oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_int,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
__module_param_call(MODULE_PARAM_PREFIX, adj,
		    &lowmem_adj_array_ops,
		    .arr = &__param_arr_adj,
		    S_IRUGO | S_IWUSR, -1);
__MODULE_PARM_TYPE(adj, "array of int");
#else
module_param_array_named(adj, lowmem_adj, int, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);
module_param_named(lmk_fast_run, lmk_fast_run, int, S_IRUGO | S_IWUSR);

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

