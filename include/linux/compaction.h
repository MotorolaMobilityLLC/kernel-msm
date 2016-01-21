#ifndef _LINUX_COMPACTION_H
#define _LINUX_COMPACTION_H

#include <linux/page-flags.h>
#include <linux/pagemap.h>

/* Return values for compact_zone() and try_to_compact_pages() */
/* compaction didn't start as it was deferred due to past failures */
#define COMPACT_DEFERRED	0
/* compaction didn't start as it was not possible or direct reclaim was more suitable */
#define COMPACT_SKIPPED		1
/* compaction should continue to another pageblock */
#define COMPACT_CONTINUE	2
/* direct compaction partially compacted a zone and there are suitable pages */
#define COMPACT_PARTIAL		3
/* The full zone was compacted */
#define COMPACT_COMPLETE	4

/* Used to signal whether compaction detected need_sched() or lock contention */
/* No contention detected */
#define COMPACT_CONTENDED_NONE	0
/* Either need_sched() was true or fatal signal pending */
#define COMPACT_CONTENDED_SCHED	1
/* Zone lock or lru_lock was contended in async compaction */
#define COMPACT_CONTENDED_LOCK	2

#ifdef CONFIG_COMPACTION
extern int sysctl_compact_memory;
extern int sysctl_compaction_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *length, loff_t *ppos);
extern int sysctl_extfrag_threshold;
extern int sysctl_extfrag_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *length, loff_t *ppos);
extern int sysctl_mobile_page_compaction;
extern int sysctl_mobile_page_compaction_handler(struct ctl_table *table,
			int write, void __user *buffer, size_t *length,
			loff_t *ppos);

extern int fragmentation_index(struct zone *zone, unsigned int order);
extern unsigned long try_to_compact_pages(struct zonelist *zonelist,
			int order, gfp_t gfp_mask, nodemask_t *mask,
			enum migrate_mode mode, int *contended,
			int alloc_flags, int classzone_idx,
			struct zone **candidate_zone);
extern void compact_pgdat(pg_data_t *pgdat, int order);
extern void reset_isolation_suitable(pg_data_t *pgdat);
extern unsigned long compaction_suitable(struct zone *zone, int order,
					int alloc_flags, int classzone_idx);

/* Do not skip compaction more than 64 times */
#define COMPACT_MAX_DEFER_SHIFT 6

/*
 * Compaction is deferred when compaction fails to result in a page
 * allocation success. 1 << compact_defer_limit compactions are skipped up
 * to a limit of 1 << COMPACT_MAX_DEFER_SHIFT
 */
static inline void defer_compaction(struct zone *zone, int order)
{
	zone->compact_considered = 0;
	zone->compact_defer_shift++;

	if (order < zone->compact_order_failed)
		zone->compact_order_failed = order;

	if (zone->compact_defer_shift > COMPACT_MAX_DEFER_SHIFT)
		zone->compact_defer_shift = COMPACT_MAX_DEFER_SHIFT;
}

/* Returns true if compaction should be skipped this time */
static inline bool compaction_deferred(struct zone *zone, int order)
{
	unsigned long defer_limit = 1UL << zone->compact_defer_shift;

	if (order < zone->compact_order_failed)
		return false;

	/* Avoid possible overflow */
	if (++zone->compact_considered > defer_limit)
		zone->compact_considered = defer_limit;

	return zone->compact_considered < defer_limit;
}

/*
 * Update defer tracking counters after successful compaction of given order,
 * which means an allocation either succeeded (alloc_success == true) or is
 * expected to succeed.
 */
static inline void compaction_defer_reset(struct zone *zone, int order,
		bool alloc_success)
{
	if (alloc_success) {
		zone->compact_considered = 0;
		zone->compact_defer_shift = 0;
	}
	if (order >= zone->compact_order_failed)
		zone->compact_order_failed = order + 1;
}

/* Returns true if restarting compaction after many failures */
static inline bool compaction_restarting(struct zone *zone, int order)
{
	if (order < zone->compact_order_failed)
		return false;

	return zone->compact_defer_shift == COMPACT_MAX_DEFER_SHIFT &&
		zone->compact_considered >= 1UL << zone->compact_defer_shift;
}

static inline bool mobile_page(struct page *page)
{
	return sysctl_mobile_page_compaction &&
		page->mapping && PageMobile(page);
}

static inline int mobilepage_isolate(struct page *page)
{
	int err = -EINVAL;

	/*
	 * Avoid burning cycles with pages that are yet under __free_pages(),
	 * or just got freed under us.
	 *
	 * In case we 'win' a race for a mobile page being freed under us and
	 * raise its refcount preventing __free_pages() from doing its job
	 * the put_page() at the end of this block will take care of
	 * release this page, thus avoiding a nasty leakage.
	 */
	if (unlikely(!get_page_unless_zero(page)))
		goto out;

	/*
	 * As mobile pages are not isolated from LRU lists, concurrent
	 * compaction threads can race against page migration functions
	 * as well as race against the releasing a page.
	 *
	 * In order to avoid having an already isolated mobile page
	 * being (wrongly) re-isolated while it is under migration,
	 * or to avoid attempting to isolate pages being released,
	 * lets be sure we have the page lock
	 * before proceeding with the mobile page isolation steps.
	 */
	if (unlikely(!trylock_page(page)))
		goto out_putpage;

	if (!(mobile_page(page) && page->mapping->a_ops->isolatepage))
		goto out_not_isolated;
	err = page->mapping->a_ops->isolatepage(page);
	if (err)
		goto out_not_isolated;
	unlock_page(page);
	return err;

out_not_isolated:
	unlock_page(page);
out_putpage:
	put_page(page);
out:
	return err;
}

static inline void mobilepage_putback(struct page *page)
{
	/*
	 * 'lock_page()' stabilizes the page and prevents races against
	 * concurrent isolation threads attempting to re-isolate it.
	 */
	lock_page(page);
	if (page->mapping && page->mapping->a_ops->putbackpage)
		page->mapping->a_ops->putbackpage(page);
	unlock_page(page);
	/* drop the extra ref count taken for mobile page isolation */
	put_page(page);
}

static inline void mobilepage_free(struct page *page)
{
	/* drop the extra ref count taken for mobile page isolation */
	put_page(page);
	__free_page(page);
}
#else
static inline unsigned long try_to_compact_pages(struct zonelist *zonelist,
			int order, gfp_t gfp_mask, nodemask_t *nodemask,
			enum migrate_mode mode, int *contended,
			int alloc_flags, int classzone_idx,
			struct zone **candidate_zone)
{
	return COMPACT_CONTINUE;
}

static inline void compact_pgdat(pg_data_t *pgdat, int order)
{
}

static inline void reset_isolation_suitable(pg_data_t *pgdat)
{
}

static inline unsigned long compaction_suitable(struct zone *zone, int order,
					int alloc_flags, int classzone_idx)
{
	return COMPACT_SKIPPED;
}

static inline void defer_compaction(struct zone *zone, int order)
{
}

static inline bool compaction_deferred(struct zone *zone, int order)
{
	return true;
}

static inline bool mobile_page(struct page *page)
{
	return false;
}

static inline bool mobilepage_isolate(struct page *page)
{
	return false;
}

static inline void mobilepage_putback(struct page *page)
{
}
static inline void mobilepage_free(struct page *page)
{
}
#endif /* CONFIG_COMPACTION */

#if defined(CONFIG_COMPACTION) && defined(CONFIG_SYSFS) && defined(CONFIG_NUMA)
extern int compaction_register_node(struct node *node);
extern void compaction_unregister_node(struct node *node);

#else

static inline int compaction_register_node(struct node *node)
{
	return 0;
}

static inline void compaction_unregister_node(struct node *node)
{
}
#endif /* CONFIG_COMPACTION && CONFIG_SYSFS && CONFIG_NUMA */

#endif /* _LINUX_COMPACTION_H */
