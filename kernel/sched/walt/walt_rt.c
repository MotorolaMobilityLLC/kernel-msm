// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <trace/hooks/sched.h>

#include "walt.h"
#include "trace.h"

#define UX_THREAD_PRIO	98     /* RT priority for UX critical threads like UI and RenderThread */
#define MAXCPUCAP_POINT_SHIFT 10				/*max cpu cap is 1024*/
#define MAXCPUCAP_POINT_SCALE (1ULL << MAXCPUCAP_POINT_SHIFT)
#define MAXCPUCAP_NORMALIZED_VALUE	(MAXCPUCAP_POINT_SCALE * MAXCPUCAP_POINT_SCALE)

static DEFINE_PER_CPU(cpumask_var_t, walt_local_cpu_mask);
DEFINE_PER_CPU(u64, rt_task_arrival_time) = 0;
static bool long_running_rt_task_trace_rgstrd;

static unsigned int cpu_reciprocal_table[WALT_NR_CPUS] = {0};

static bool rt_reciprocal_cpu_table_set(int index, unsigned int cap) {

	if(index < 0 || index >= WALT_NR_CPUS)
		return false;

	cpu_reciprocal_table[index] = MAXCPUCAP_NORMALIZED_VALUE / cap;

	pr_info("walt_local_cpu_reciprocal cpu%d-cap:%u val:%u\n", index, cap, cpu_reciprocal_table[index]);

	return true;
}

int rt_calculate_normalized_value(int overutil, int index) {
	if(index < 0 || index >= WALT_NR_CPUS)
		index = 0;

	/* overutil / cap ≈ (overutil * cpu_reciprocal_table[index]) >> MAXCPUCAP_POINT_SHIFT
		normalized_value = (overutil / cap) * MAXCPUCAP_POINT_SCALE
	The temp max value is 1024(overutil max)*8738(cpu_reciprocal_table[index] max)=894712
	894712 < 2147483647 														 */
	int temp = (int)overutil * cpu_reciprocal_table[index];
	return (int)(temp >> MAXCPUCAP_POINT_SHIFT);
}

static void rt_task_arrival_marker(void *unused, bool preempt,
	struct task_struct *prev, struct task_struct *next,
	unsigned int prev_state)
{
	unsigned int cpu = raw_smp_processor_id();

	if (next->policy == SCHED_FIFO && next != cpu_rq(cpu)->stop)
		per_cpu(rt_task_arrival_time, cpu) = rq_clock_task(this_rq());
	else
		per_cpu(rt_task_arrival_time, cpu) = 0;
}

static void long_running_rt_task_notifier(void *unused, struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	unsigned int cpu = raw_smp_processor_id();

	if (!sysctl_sched_long_running_rt_task_ms)
		return;

	if (!per_cpu(rt_task_arrival_time, cpu))
		return;

	if (per_cpu(rt_task_arrival_time, cpu) && curr->policy != SCHED_FIFO) {
		/*
		 * It is possible that the scheduling policy for the current
		 * task might get changed after task arrival time stamp is
		 * noted during sched_switch of RT task. To avoid such false
		 * positives, reset arrival time stamp.
		 */
		per_cpu(rt_task_arrival_time, cpu) = 0;
		return;
	}

	/*
	 * Since we are called from the main tick, rq clock task must have
	 * been updated very recently. Use it directly, instead of
	 * update_rq_clock_task() to avoid warnings.
	 */
	if (rq->clock_task -
		per_cpu(rt_task_arrival_time, cpu)
			> sysctl_sched_long_running_rt_task_ms * MSEC_TO_NSEC) {
		printk_deferred("RT task %s (%d) runtime > %u now=%llu task arrival time=%llu runtime=%llu\n",
				curr->comm, curr->pid,
				sysctl_sched_long_running_rt_task_ms * MSEC_TO_NSEC,
				rq->clock_task,
				per_cpu(rt_task_arrival_time, cpu),
				rq->clock_task -
				per_cpu(rt_task_arrival_time, cpu));
		BUG();
	}
}

int sched_long_running_rt_task_ms_handler(struct ctl_table *table, int write,
				       void __user *buffer, size_t *lenp,
				       loff_t *ppos)
{
	int ret;
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);

	ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);

	if (sysctl_sched_long_running_rt_task_ms > 0 &&
			sysctl_sched_long_running_rt_task_ms < 800)
		sysctl_sched_long_running_rt_task_ms = 800;

	if (write && !long_running_rt_task_trace_rgstrd) {
		register_trace_sched_switch(rt_task_arrival_marker, NULL);
		register_trace_android_vh_scheduler_tick(long_running_rt_task_notifier, NULL);
		long_running_rt_task_trace_rgstrd = true;
	}

	mutex_unlock(&mutex);

	return ret;
}

/* huangzq2: Check if the given task is RT UX task */
static inline bool is_rt_ux_task(struct task_struct *task)
{
	return task && task->mm && task_has_rt_policy(task) && task->prio == UX_THREAD_PRIO
		&& uclamp_eff_value(task, UCLAMP_MIN) > 0;
}

static void walt_rt_choose_overutil_backup_cpu(struct task_struct *task, int cpu, unsigned long tutil, int *backup_cpu,
												unsigned long *backup_cpu_origcap, int *backup_cpu_overutil, unsigned int *backup_cpu_rtnr)
{
	unsigned int cpu_rtnr = UINT_MAX;
	long cpu_overutil_val = LONG_MAX;
	int cpu_normalized_value = -1;
	int backup_normalized_value = -1;

	if(unlikely(!task || !backup_cpu || !backup_cpu_origcap || !backup_cpu_overutil || !backup_cpu_rtnr))
		return;

	cpu_overutil_val = __cpu_overutilized_relvalue(cpu, tutil);
	cpu_rtnr = cpu_rq(cpu)->rt.rt_nr_running;

	if(capacity_orig_of(cpu) == *backup_cpu_origcap) {		/*If CPUs have the same capacity, they belong to the same cluster.*/
		if(unlikely(cpu_rtnr > *backup_cpu_rtnr))
			return;

		if(cpu_rtnr == *backup_cpu_rtnr ) {
			if(capacity_of(cpu) < capacity_of(*backup_cpu))
				return;

			if((capacity_of(cpu) == capacity_of(*backup_cpu)) && cpu != task_cpu(task))
				return;
		}
	}
	else {			/*If CPU original capacities are not equal, use a normalized calculation. */
		cpu_normalized_value = rt_calculate_normalized_value(cpu_overutil_val, cpu);
		backup_normalized_value = rt_calculate_normalized_value(*backup_cpu_overutil, *backup_cpu);

		if(trace_sched_normalized_compare_rt_enabled())
			trace_sched_normalized_compare_rt(cpu, cpu_overutil_val, cpu_normalized_value, *backup_cpu, *backup_cpu_overutil, backup_normalized_value);

		if(cpu_normalized_value > backup_normalized_value)
			return;

		if(cpu_normalized_value == backup_normalized_value && capacity_orig_of(cpu) < *backup_cpu_origcap)
			return;
	}

	if(trace_sched_choose_backup_cpu_rt_enabled())
		trace_sched_choose_backup_cpu_rt(cpu, capacity_orig_of(cpu), cpu_overutil_val, cpu_rtnr, *backup_cpu, \
										*backup_cpu_overutil, *backup_cpu_rtnr, cpu_normalized_value, backup_normalized_value);

	*backup_cpu_origcap = capacity_orig_of(cpu);
	*backup_cpu_overutil = cpu_overutil_val;
	*backup_cpu_rtnr = cpu_rtnr;
	*backup_cpu = cpu;
}

static void walt_rt_energy_aware_wake_cpu(struct task_struct *task, struct cpumask *lowest_mask,
					  int ret, int *best_cpu)
{
	int cpu;
	unsigned long util, best_cpu_util = ULONG_MAX;
	unsigned long best_cpu_util_cum = ULONG_MAX;
	unsigned long util_cum;
	unsigned long tutil = task_util(task);
	unsigned int best_idle_exit_latency = UINT_MAX;
	unsigned int cpu_idle_exit_latency = UINT_MAX;
	bool boost_on_big = rt_boost_on_big();
	int cluster;
	int order_index = (boost_on_big && num_sched_clusters > 1) ? 1 : 0;
	int end_index = 0;
	bool best_cpu_lt = true;
	bool strict_cpu_overutil = true;
	int backup_cpu = -1;
	unsigned long bp_cpu_orig = ULONG_MAX;
	int bp_cpu_overutil = MAXCPUCAP_POINT_SCALE;
	unsigned int bp_cpu_rtnr = UINT_MAX;

	if (unlikely(walt_disabled))
		return;

	if (!ret)
		return; /* No targets found */

	rcu_read_lock();

	if(is_rt_ux_task(task)) {
		end_index = num_sched_clusters - 1;
		strict_cpu_overutil = false;
	}
	else if (soc_feat(SOC_ENABLE_SILVER_RT_SPREAD_BIT) && order_index == 0)
		end_index = 1;

	for (cluster = 0; cluster < num_sched_clusters; cluster++) {
		for_each_cpu_and(cpu, lowest_mask, &cpu_array[order_index][cluster]) {
			bool lt;

			trace_sched_cpu_util(cpu, lowest_mask);

			if (!cpu_active(cpu))
				continue;

			if (cpu_halted(cpu))
				continue;

			if (sched_cpu_high_irqload(cpu))
				continue;

			if (__cpu_overutilized(cpu, tutil)) {
				if(!strict_cpu_overutil)
					walt_rt_choose_overutil_backup_cpu(task, cpu, tutil, &backup_cpu, &bp_cpu_orig, &bp_cpu_overutil, &bp_cpu_rtnr);

				continue;
			}

			util = cpu_util(cpu);

			lt = (walt_low_latency_task(cpu_rq(cpu)->curr) ||
				walt_nr_rtg_high_prio(cpu));

			/*
			 * When the best is suitable and the current is not,
			 * skip it
			 */
			if (lt && !best_cpu_lt)
				continue;

			/*
			 * Either both are sutilable or unsuitable, load takes
			 * precedence.
			 */
			if (!(best_cpu_lt ^ lt) && (util > best_cpu_util))
				continue;

			/*
			 * If the previous CPU has same load, keep it as
			 * best_cpu.
			 */
			if (best_cpu_util == util && *best_cpu == task_cpu(task))
				continue;

			/*
			 * If candidate CPU is the previous CPU, select it.
			 * Otherwise, if its load is same with best_cpu and in
			 * a shallower C-state, select it.  If all above
			 * conditions are same, select the least cumulative
			 * window demand CPU.
			 */
			cpu_idle_exit_latency = walt_get_idle_exit_latency(cpu_rq(cpu));

			util_cum = cpu_util_cum(cpu);
			if (cpu != task_cpu(task) && best_cpu_util == util) {
				if (best_idle_exit_latency < cpu_idle_exit_latency)
					continue;

				if (best_idle_exit_latency == cpu_idle_exit_latency &&
						best_cpu_util_cum < util_cum)
					continue;
			}

			best_idle_exit_latency = cpu_idle_exit_latency;
			best_cpu_util_cum = util_cum;
			best_cpu_util = util;
			*best_cpu = cpu;
			best_cpu_lt = lt;
		}
		if (cluster < end_index) {
			if (*best_cpu == -1 || !available_idle_cpu(*best_cpu))
				continue;
		}

		if (*best_cpu != -1)
			break;
	}

	if(*best_cpu == -1 && !strict_cpu_overutil)
		*best_cpu = backup_cpu;

	if(trace_sched_select_energy_cpu_rt_enabled())
		trace_sched_select_energy_cpu_rt(order_index, end_index, cluster, *best_cpu, backup_cpu, strict_cpu_overutil, tutil);

	rcu_read_unlock();
}

#ifdef CONFIG_UCLAMP_TASK
static inline bool walt_rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned int min_cap;
	unsigned int max_cap;
	unsigned int cpu_cap;

	min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	max_cap = uclamp_eff_value(p, UCLAMP_MAX);

	cpu_cap = capacity_orig_of(cpu);

	return cpu_cap >= min(min_cap, max_cap);
}
#else
static inline bool walt_rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	return true;
}
#endif

/*
 * walt specific should_honor_rt_sync (see rt.c).  this will honor
 * the sync flag regardless of whether the current waker is cfs or rt
 */
static inline bool walt_should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					     bool sync)
{
    /* huangzq2: RT UX task has its own core selection logic.*/
    if (is_rt_ux_task(rq->curr) || is_rt_ux_task(p)) return false;

	return sync &&
		p->prio <= rq->rt.highest_prio.next &&
		rq->rt.rt_nr_running <= 2;
}

enum rt_fastpaths {
	NONE = 0,
	NON_WAKEUP,
	SYNC_WAKEUP,
	CLUSTER_PACKING_FASTPATH,
};

static void walt_select_task_rq_rt(void *unused, struct task_struct *task, int cpu,
					int sd_flag, int wake_flags, int *new_cpu)
{
	struct task_struct *curr;
	struct rq *rq, *this_cpu_rq;
	bool may_not_preempt;
	bool sync = !!(wake_flags & WF_SYNC);
	int ret, target = -1, this_cpu;
	struct cpumask *lowest_mask = NULL;
	int packing_cpu = -1;
	int fastpath = NONE;
	struct cpumask lowest_mask_reduced = { CPU_BITS_NONE };
	struct walt_task_struct *wts;

	if (unlikely(walt_disabled))
		return;

	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK) {
		fastpath = NON_WAKEUP;
		goto out;
	}

	this_cpu = raw_smp_processor_id();
	this_cpu_rq = cpu_rq(this_cpu);
	wts = (struct walt_task_struct *) task->android_vendor_data1;

	/*
	 * Respect the sync flag as long as the task can run on this CPU.
	 */
	if (sysctl_sched_sync_hint_enable && cpu_active(this_cpu) && !cpu_halted(this_cpu) &&
	    cpumask_test_cpu(this_cpu, task->cpus_ptr) &&
	    cpumask_test_cpu(this_cpu, &wts->reduce_mask) &&
	    walt_should_honor_rt_sync(this_cpu_rq, task, sync)) {
		fastpath = SYNC_WAKEUP;
		*new_cpu = this_cpu;
		goto out;
	}

	*new_cpu = cpu; /* previous CPU as back up */
	rq = cpu_rq(cpu);

	rcu_read_lock();
	curr = READ_ONCE(rq->curr); /* unlocked access */

	/*
	 * If the current task on @p's runqueue is a softirq task,
	 * it may run without preemption for a time that is
	 * ill-suited for a waiting RT task. Therefore, try to
	 * wake this RT task on another runqueue.
	 *
	 * Otherwise, just let it ride on the affined RQ and the
	 * post-schedule router will push the preempted task away
	 *
	 * This test is optimistic, if we get it wrong the load-balancer
	 * will have to sort it out.
	 *
	 * We take into account the capacity of the CPU to ensure it fits the
	 * requirement of the task - which is only important on heterogeneous
	 * systems like big.LITTLE.
	 */
	may_not_preempt = cpu_busy_with_softirqs(cpu);

	lowest_mask = this_cpu_cpumask_var_ptr(walt_local_cpu_mask);

	/*
	 * If we're on asym system ensure we consider the different capacities
	 * of the CPUs when searching for the lowest_mask.
	 */
	ret = cpupri_find_fitness(&task_rq(task)->rd->cpupri, task,
				lowest_mask, walt_rt_task_fits_capacity);

	packing_cpu = walt_find_and_choose_cluster_packing_cpu(0, task);
	if (packing_cpu >= 0) {
		while (packing_cpu < WALT_NR_CPUS) {
			if (cpumask_test_cpu(packing_cpu, &wts->reduce_mask) &&
				cpumask_test_cpu(packing_cpu, task->cpus_ptr) &&
				cpu_active(packing_cpu) &&
				!cpu_halted(packing_cpu) &&
				(cpu_rq(packing_cpu)->rt.rt_nr_running <= 1))
				break;
			packing_cpu++;
		}

		if (packing_cpu < WALT_NR_CPUS) {
			fastpath = CLUSTER_PACKING_FASTPATH;
			*new_cpu = packing_cpu;
			goto unlock;
		}
	}

	cpumask_and(&lowest_mask_reduced, lowest_mask, &wts->reduce_mask);
	if (!cpumask_empty(&lowest_mask_reduced))
		walt_rt_energy_aware_wake_cpu(task, &lowest_mask_reduced, ret, &target);
	if (target == -1)
		walt_rt_energy_aware_wake_cpu(task, lowest_mask, ret, &target);

	/*
	 * If cpu is non-preemptible, prefer remote cpu
	 * even if it's running a higher-prio task.
	 * Otherwise: Don't bother moving it if the destination CPU is
	 * not running a lower priority task.
	 */
	if (target != -1 &&
	    (may_not_preempt || task->prio < cpu_rq(target)->rt.highest_prio.curr))
		*new_cpu = target;

	/* if backup or chosen cpu is halted, pick something else */
	if (cpu_halted(*new_cpu)) {
		cpumask_t non_halted;

		/* choose the lowest-order, unhalted, allowed CPU */
		cpumask_andnot(&non_halted, task->cpus_ptr, cpu_halt_mask);
		target = cpumask_first(&non_halted);
		if (target < nr_cpu_ids)
			*new_cpu = target;
	}
unlock:
	rcu_read_unlock();
out:
	trace_sched_select_task_rt(task, fastpath, *new_cpu, lowest_mask);
}


static void walt_rt_find_lowest_rq(void *unused, struct task_struct *task,
				   struct cpumask *lowest_mask, int ret, int *best_cpu)

{
	int packing_cpu = -1;
	int fastpath = 0;
	struct walt_task_struct *wts;
	struct cpumask lowest_mask_reduced = { CPU_BITS_NONE };

	if (unlikely(walt_disabled))
		return;

	wts = (struct walt_task_struct *) task->android_vendor_data1;

	packing_cpu = walt_find_and_choose_cluster_packing_cpu(0, task);
	if (packing_cpu >= 0) {
		while (packing_cpu < WALT_NR_CPUS) {
			if (cpumask_test_cpu(packing_cpu, &wts->reduce_mask) &&
				cpumask_test_cpu(packing_cpu, task->cpus_ptr) &&
				cpu_active(packing_cpu) &&
				!cpu_halted(packing_cpu) &&
				(cpu_rq(packing_cpu)->rt.rt_nr_running <= 2))
				break;
			packing_cpu++;
		}

		if (packing_cpu < WALT_NR_CPUS) {
			fastpath = CLUSTER_PACKING_FASTPATH;
			*best_cpu = packing_cpu;
			goto out;
		}
	}

	cpumask_and(&lowest_mask_reduced, lowest_mask, &wts->reduce_mask);
	if (!cpumask_empty(&lowest_mask_reduced))
		walt_rt_energy_aware_wake_cpu(task, &lowest_mask_reduced, ret, best_cpu);
	if (*best_cpu == -1)
		walt_rt_energy_aware_wake_cpu(task, lowest_mask, ret, best_cpu);

	/*
	 * Walt was not able to find a non-halted best cpu. Ensure that
	 * find_lowest_rq doesn't use a halted cpu going forward, but
	 * does a best effort itself to find a good CPU.
	 */
	if (*best_cpu == -1)
		cpumask_andnot(lowest_mask, lowest_mask, cpu_halt_mask);
out:
	trace_sched_rt_find_lowest_rq(task, fastpath, *best_cpu, lowest_mask);
}

void walt_rt_init(void)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		if (!(zalloc_cpumask_var_node(&per_cpu(walt_local_cpu_mask, i),
					      GFP_KERNEL, cpu_to_node(i)))) {
			pr_err("walt_local_cpu_mask alloc failed for cpu%d\n", i);
			return;
		}

		rt_reciprocal_cpu_table_set(i, capacity_orig_of(i));
	}

	register_trace_android_rvh_select_task_rq_rt(walt_select_task_rq_rt, NULL);
	register_trace_android_rvh_find_lowest_rq(walt_rt_find_lowest_rq, NULL);
}
