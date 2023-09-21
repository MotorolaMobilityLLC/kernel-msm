/* SPDX-License-Identifier: GPL-2.0 */
#ifdef PROTECT_TRACE_INCLUDE_PATH
#undef PROTECT_TRACE_INCLUDE_PATH

#include <trace/hooks/save_incpath.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/restore_incpath.h>

#else /* PROTECT_TRACE_INCLUDE_PATH */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mm

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_MM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MM_H

#include <linux/types.h>

#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>
#include <linux/rwsem.h>

#ifdef __GENKSYMS__
struct slabinfo;
struct cgroup_subsys_state;
struct device;
struct mem_cgroup;
struct readahead_control;
#else
/* struct slabinfo */
#include <../mm/slab.h>
/* struct cgroup_subsys_state */
#include <linux/cgroup-defs.h>
/* struct device */
#include <linux/device.h>
/* struct mem_cgroup */
#include <linux/memcontrol.h>
/* struct readahead_control */
#include <linux/pagemap.h>
#endif /* __GENKSYMS__ */
struct cma;
struct swap_slots_cache;
struct page_vma_mapped_walk;

DECLARE_HOOK(android_vh_rmqueue,
	TP_PROTO(struct zone *preferred_zone, struct zone *zone,
		unsigned int order, gfp_t gfp_flags,
		unsigned int alloc_flags, int migratetype),
	TP_ARGS(preferred_zone, zone, order,
		gfp_flags, alloc_flags, migratetype));

DECLARE_HOOK(android_vh_mem_cgroup_alloc,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_free,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_id_remove,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_css_online,
	TP_PROTO(struct cgroup_subsys_state *css, struct mem_cgroup *memcg),
	TP_ARGS(css, memcg));
DECLARE_HOOK(android_vh_mem_cgroup_css_offline,
	TP_PROTO(struct cgroup_subsys_state *css, struct mem_cgroup *memcg),
	TP_ARGS(css, memcg));
/* macro versions of hooks are no longer required */

#endif /* _TRACE_HOOK_MM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

#endif /* PROTECT_TRACE_INCLUDE_PATH */
