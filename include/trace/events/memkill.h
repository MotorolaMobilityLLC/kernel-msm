#undef TRACE_SYSTEM
#define TRACE_SYSTEM memkill

#if !defined(_TRACE_MEMKILL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MEMKILL_H

#include <linux/tracepoint.h>
#include <linux/kernel.h>
#include <linux/swap.h>
#include <linux/sched.h>
#include <linux/types.h>

TRACE_EVENT(oom_kill,
		TP_PROTO(pid_t pid_nr, const char *comm,
			unsigned long total_vm, unsigned long anon_rss,
			unsigned long file_rss, int oom_score_adj, int order,
			unsigned int victim_points),

		TP_ARGS(pid_nr, comm, total_vm, anon_rss, file_rss,
			oom_score_adj, order, victim_points),

		TP_STRUCT__entry(
			__field(pid_t,		pid_nr)
			__array(char,		comm,	TASK_COMM_LEN)
			__field(unsigned long,	total_vm)
			__field(unsigned long,	anon_rss)
			__field(unsigned long,	file_rss)
			__field(int,		oom_score_adj)
			__field(int,		order)
			__field(unsigned int,	victim_points)
			__field(long,		cached)
			__field(unsigned long,	freeswap)
		),

		TP_fast_assign(
			struct sysinfo i;

			si_meminfo(&i);
			si_swapinfo(&i);

			__entry->cached = (global_page_state(NR_FILE_PAGES) -
					total_swapcache_pages - i.bufferram)
						<< (PAGE_SHIFT - 10);
			if (__entry->cached < 0)
				__entry->cached = 0;

			__entry->pid_nr = pid_nr;
			strncpy(__entry->comm, comm, TASK_COMM_LEN);
			__entry->total_vm = total_vm;
			__entry->anon_rss = anon_rss;
			__entry->file_rss = file_rss;
			__entry->oom_score_adj = oom_score_adj;
			__entry->order = order;
			__entry->victim_points = victim_points;
			__entry->freeswap = i.freeswap << (PAGE_SHIFT - 10);
		),

		TP_printk("%d %s %lu %lu %lu %d %d %u %ld %lu",
				__entry->pid_nr, __entry->comm,
				__entry->total_vm, __entry->anon_rss,
				__entry->file_rss, __entry->oom_score_adj,
				__entry->order, __entry->victim_points,
				__entry->cached, __entry->freeswap)
);

TRACE_EVENT(oom_kill_shared,
		TP_PROTO(pid_t pid_nr, const char *comm),

		TP_ARGS(pid_nr, comm),

		TP_STRUCT__entry(
			__field(pid_t,		pid_nr)
			__array(char,		comm,	TASK_COMM_LEN)
		),

		TP_fast_assign(
			__entry->pid_nr = pid_nr;
			strncpy(__entry->comm, comm, TASK_COMM_LEN);
		),

		TP_printk("%d %s", __entry->pid_nr, __entry->comm)
);

TRACE_EVENT(lmk_kill,
		TP_PROTO(pid_t pid_nr, const char *comm,
			int selected_oom_adj, int selected_tasksize,
			int min_adj),

		TP_ARGS(pid_nr, comm, selected_oom_adj, selected_tasksize,
			min_adj),

		TP_STRUCT__entry(
			__field(pid_t,		pid_nr)
			__array(char,		comm,	TASK_COMM_LEN)
			__field(int,		selected_oom_adj)
			__field(int,		selected_tasksize)
			__field(int,		min_adj)
			__field(long,		cached)
			__field(unsigned long,	freeswap)
		),

		TP_fast_assign(
			struct sysinfo i;

			si_meminfo(&i);
			si_swapinfo(&i);

			__entry->cached = (global_page_state(NR_FILE_PAGES) -
					total_swapcache_pages - i.bufferram)
						<< (PAGE_SHIFT - 10);
			if (__entry->cached < 0)
				__entry->cached = 0;

			__entry->pid_nr = pid_nr;
			strncpy(__entry->comm, comm, TASK_COMM_LEN);
			__entry->selected_oom_adj = selected_oom_adj;
			__entry->selected_tasksize = selected_tasksize;
			__entry->min_adj = min_adj;
			__entry->freeswap = i.freeswap << (PAGE_SHIFT - 10);
		),

		TP_printk("%d %s %d %d %d %ld %lu", __entry->pid_nr,
				__entry->comm, __entry->selected_oom_adj,
				__entry->selected_tasksize, __entry->min_adj,
				__entry->cached, __entry->freeswap)
);

#endif /* _TRACE_MEMKILL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
