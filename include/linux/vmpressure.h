#ifndef __LINUX_VMPRESSURE_H
#define __LINUX_VMPRESSURE_H

#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include <linux/cgroup.h>

enum vmpressure_levels {
	VMPRESSURE_LOW = 0,
	VMPRESSURE_MEDIUM,
	VMPRESSURE_CRITICAL,
	VMPRESSURE_NUM_LEVELS,
};

struct vmpressure {
	/*
	 * The window size is the number of scanned pages before
	 * we try to analyze scanned/reclaimed ratio. Using small window
	 * sizes can cause lot of false positives, but too big window size will
	 * delay the notifications.
	 *
	 * In order to reduce the amount of false positives for low and medium
	 * levels, those levels aren't reported until we've seen multiple
	 * windows at those respective pressure levels. This makes sure
	 * sure that we don't delay notifications when encountering critical
	 * levels of memory pressure, but also don't spam userspace in case
	 * nothing serious is going on. The number of windows seen at each
	 * pressure level is kept in nr_windows below.
	 *
	 * For the root mem cgroup, the window size is computed based on the
	 * total amount of pages available in the system. For non-root cgroups,
	 * we compute the window size based on the hard memory limit, or if
	 * that is not set, we fall back to the default window size.
	 */
	unsigned long window_size;
	/* The number of windows we've seen each pressure level occur for */
	unsigned int nr_windows[VMPRESSURE_NUM_LEVELS];
	unsigned long scanned;
	unsigned long reclaimed;
	/* The lock is used to keep the members above in sync. */
	struct mutex sr_lock;

	/* The list of vmpressure_event structs. */
	struct list_head events;
	/* Have to grab the lock on events traversal or modifications. */
	struct mutex events_lock;

	struct work_struct work;
};

struct mem_cgroup;

#ifdef CONFIG_MEMCG
extern void vmpressure(gfp_t gfp, struct mem_cgroup *memcg,
		       unsigned long scanned, unsigned long reclaimed);
extern void vmpressure_prio(gfp_t gfp, struct mem_cgroup *memcg, int prio);

extern void vmpressure_init(struct vmpressure *vmpr, bool is_root);
extern struct vmpressure *memcg_to_vmpressure(struct mem_cgroup *memcg);
extern struct cgroup_subsys_state *vmpressure_to_css(struct vmpressure *vmpr);
extern struct vmpressure *css_to_vmpressure(struct cgroup_subsys_state *css);
extern void vmpressure_update_mem_limit(struct mem_cgroup *memcg,
					unsigned long new_limit);
extern int vmpressure_register_event(struct cgroup *cg, struct cftype *cft,
				     struct eventfd_ctx *eventfd,
				     const char *args);
extern void vmpressure_unregister_event(struct cgroup *cg, struct cftype *cft,
					struct eventfd_ctx *eventfd);
#else
static inline void vmpressure(gfp_t gfp, struct mem_cgroup *memcg,
			      unsigned long scanned, unsigned long reclaimed) {}
static inline void vmpressure_prio(gfp_t gfp, struct mem_cgroup *memcg,
				   int prio) {}
#endif /* CONFIG_MEMCG */
#endif /* __LINUX_VMPRESSURE_H */
