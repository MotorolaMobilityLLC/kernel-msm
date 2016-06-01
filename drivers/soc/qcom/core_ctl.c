/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/moduleparam.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <soc/qcom/core_ctl.h>
#include <linux/mutex.h>

#include <trace/events/power.h>

#define MAX_CPUS_PER_GROUP 4

struct cpu_data {
	/* Per CPU data. */
	bool	inited;
	bool	online;
	bool	rejected;
	bool	is_busy;
	bool    not_preferred;
	unsigned int busy;
	unsigned int cpu;
	struct list_head sib;
	unsigned int first_cpu;
	struct list_head pending_sib;

	/* Per cluster data set only on first CPU */
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int offline_delay_ms;
	unsigned int busy_up_thres[MAX_CPUS_PER_GROUP];
	unsigned int busy_down_thres[MAX_CPUS_PER_GROUP];
	unsigned int online_cpus;
	unsigned int avail_cpus;
	unsigned int num_cpus;
	unsigned int need_cpus;
	unsigned int task_thres;
	s64 need_ts;
	struct list_head lru;
	bool pending;
	spinlock_t pending_lock;
	bool is_big_cluster;
	int nrrun;
	bool nrrun_changed;
	struct timer_list timer;
	struct task_struct *hotplug_thread;
	struct kobject kobj;
	struct list_head pending_lru;
};

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static DEFINE_SPINLOCK(state_lock);
static DEFINE_SPINLOCK(pending_lru_lock);
static DEFINE_MUTEX(lru_lock);

static void apply_need(struct cpu_data *f);
static void wake_up_hotplug_thread(struct cpu_data *state);
static void add_to_pending_lru(struct cpu_data *state);
static void update_lru(struct cpu_data *state);

/* ========================= sysfs interface =========================== */

static ssize_t store_min_cpus(struct cpu_data *state,
				const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	state->min_cpus = min(val, state->max_cpus);
	wake_up_hotplug_thread(state);

	return count;
}

static ssize_t show_min_cpus(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->min_cpus);
}

static ssize_t store_max_cpus(struct cpu_data *state,
				const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	val = min(val, state->num_cpus);
	state->max_cpus = val;
	state->min_cpus = min(state->min_cpus, state->max_cpus);
	wake_up_hotplug_thread(state);

	return count;
}

static ssize_t show_max_cpus(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->max_cpus);
}

static ssize_t store_offline_delay_ms(struct cpu_data *state,
					const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	state->offline_delay_ms = val;
	apply_need(state);

	return count;
}

static ssize_t show_task_thres(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->task_thres);
}

static ssize_t store_task_thres(struct cpu_data *state,
					const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val < state->num_cpus)
		return -EINVAL;

	state->task_thres = val;
	apply_need(state);

	return count;
}

static ssize_t show_offline_delay_ms(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->offline_delay_ms);
}

static ssize_t store_busy_up_thres(struct cpu_data *state,
					const char *buf, size_t count)
{
	unsigned int val[MAX_CPUS_PER_GROUP];
	int ret, i;

	ret = sscanf(buf, "%u %u %u %u\n", &val[0], &val[1], &val[2], &val[3]);
	if (ret != 1 && ret != state->num_cpus)
		return -EINVAL;

	if (ret == 1) {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_up_thres[i] = val[0];
	} else {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_up_thres[i] = val[i];
	}
	apply_need(state);
	return count;
}

static ssize_t show_busy_up_thres(struct cpu_data *state, char *buf)
{
	int i, count = 0;
	for (i = 0; i < state->num_cpus; i++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%u ",
				  state->busy_up_thres[i]);
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t store_busy_down_thres(struct cpu_data *state,
					const char *buf, size_t count)
{
	unsigned int val[MAX_CPUS_PER_GROUP];
	int ret, i;

	ret = sscanf(buf, "%u %u %u %u\n", &val[0], &val[1], &val[2], &val[3]);
	if (ret != 1 && ret != state->num_cpus)
		return -EINVAL;

	if (ret == 1) {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_down_thres[i] = val[0];
	} else {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_down_thres[i] = val[i];
	}
	apply_need(state);
	return count;
}

static ssize_t show_busy_down_thres(struct cpu_data *state, char *buf)
{
	int i, count = 0;
	for (i = 0; i < state->num_cpus; i++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%u ",
				  state->busy_down_thres[i]);
	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t store_is_big_cluster(struct cpu_data *state,
				const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	state->is_big_cluster = val ? 1 : 0;
	return count;
}

static ssize_t show_is_big_cluster(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->is_big_cluster);
}

static ssize_t show_cpus(struct cpu_data *state, char *buf)
{
	struct cpu_data *c;
	ssize_t count = 0;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry(c, &state->lru, sib) {
		count += snprintf(buf + count, PAGE_SIZE - count,
					"CPU%u (%s)\n", c->cpu,
					c->online ? "Online" : "Offline");
	}
	spin_unlock_irqrestore(&state_lock, flags);
	return count;
}

static ssize_t show_need_cpus(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->need_cpus);
}

static ssize_t show_online_cpus(struct cpu_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->online_cpus);
}

static ssize_t show_global_state(struct cpu_data *state, char *buf)
{
	struct cpu_data *c;
	ssize_t count = 0;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		count += snprintf(buf + count, PAGE_SIZE - count,
					"CPU%u\n", cpu);
		c = &per_cpu(cpu_state, cpu);
		if (!c->inited)
			continue;
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tCPU: %u\n", c->cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tOnline: %u\n", c->online);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tRejected: %u\n", c->rejected);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n", c->first_cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tBusy%%: %u\n", c->busy);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tIs busy: %u\n", c->is_busy);
		if (c->cpu != c->first_cpu)
			continue;
		count += snprintf(buf + count, PAGE_SIZE- count,
					"\tNr running: %u\n", c->nrrun);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tAvail CPUs: %u\n", c->avail_cpus);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNeed CPUs: %u\n", c->need_cpus);
	}

	return count;
}

static ssize_t store_not_preferred(struct cpu_data *state,
							const char *buf, size_t count)
{
	struct cpu_data *c;
	unsigned int i, first_cpu;
	unsigned int val[MAX_CPUS_PER_GROUP];
	int ret;

	ret = sscanf(buf, "%u %u %u %u\n", &val[0], &val[1], &val[2], &val[3]);
	if (ret != 1 && ret != state->num_cpus)
		return -EINVAL;

	first_cpu = state->first_cpu;

	for (i = 0; i < state->num_cpus; i++) {
		c = &per_cpu(cpu_state, first_cpu);
		c->not_preferred = val[i];
		first_cpu++;
	}

	return count;
}

static ssize_t show_not_preferred(struct cpu_data *state, char *buf)
{
	struct cpu_data *c;
	ssize_t count = 0;
	unsigned int i, first_cpu;

	first_cpu = state->first_cpu;

	for (i = 0; i < state->num_cpus; i++) {
		c = &per_cpu(cpu_state, first_cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tCPU:%d %u\n", first_cpu, c->not_preferred);
		first_cpu++;
	}

	return count;
}

struct core_ctl_attr {
	struct attribute attr;
	ssize_t (*show)(struct cpu_data *, char *);
	ssize_t (*store)(struct cpu_data *, const char *, size_t count);
};

#define core_ctl_attr_ro(_name)		\
static struct core_ctl_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define core_ctl_attr_rw(_name)			\
static struct core_ctl_attr _name =		\
__ATTR(_name, 0644, show_##_name, store_##_name)

core_ctl_attr_rw(min_cpus);
core_ctl_attr_rw(max_cpus);
core_ctl_attr_rw(offline_delay_ms);
core_ctl_attr_rw(busy_up_thres);
core_ctl_attr_rw(busy_down_thres);
core_ctl_attr_rw(task_thres);
core_ctl_attr_rw(is_big_cluster);
core_ctl_attr_ro(cpus);
core_ctl_attr_ro(need_cpus);
core_ctl_attr_ro(online_cpus);
core_ctl_attr_ro(global_state);
core_ctl_attr_rw(not_preferred);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&offline_delay_ms.attr,
	&busy_up_thres.attr,
	&busy_down_thres.attr,
	&task_thres.attr,
	&is_big_cluster.attr,
	&cpus.attr,
	&need_cpus.attr,
	&online_cpus.attr,
	&global_state.attr,
	&not_preferred.attr,
	NULL
};

#define to_cpu_data(k) container_of(k, struct cpu_data, kobj)
#define to_attr(a) container_of(a, struct core_ctl_attr, attr)
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cpu_data *data = to_cpu_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(data, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct cpu_data *data = to_cpu_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(data, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
};

/* ==================== runqueue based core count =================== */

#define RQ_AVG_TOLERANCE 2
#define RQ_AVG_DEFAULT_MS 20
#define NR_RUNNING_TOLERANCE 5
static unsigned int rq_avg_period_ms = RQ_AVG_DEFAULT_MS;
module_param(rq_avg_period_ms, uint, 0644);

static s64 rq_avg_timestamp_ms;
static struct timer_list rq_avg_timer;

static void update_running_avg(bool trigger_update)
{
	int cpu;
	struct cpu_data *pcpu;
	int avg, iowait_avg, big_avg, old_nrrun;
	s64 now;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);

	now = core_ctl_get_time();
	if (now - rq_avg_timestamp_ms < rq_avg_period_ms - RQ_AVG_TOLERANCE) {
		spin_unlock_irqrestore(&state_lock, flags);
		return;
	}
	rq_avg_timestamp_ms = now;
	sched_get_nr_running_avg(&avg, &iowait_avg, &big_avg);

	spin_unlock_irqrestore(&state_lock, flags);

	/*
	 * Round up to the next integer if the average nr running tasks
	 * is within NR_RUNNING_TOLERANCE/100 of the next integer.
	 * If normal rounding up is used, it will allow a transient task
	 * to trigger online event. By the time core is onlined, the task
	 * has finished.
	 * Rounding to closest suffers same problem because scheduler
	 * might only provide running stats per jiffy, and a transient
	 * task could skew the number for one jiffy. If core control
	 * samples every 2 jiffies, it will observe 0.5 additional running
	 * average which rounds up to 1 task.
	 */
	avg = (avg + NR_RUNNING_TOLERANCE) / 100;
	big_avg = (big_avg + NR_RUNNING_TOLERANCE) / 100;

	for_each_possible_cpu(cpu) {
		pcpu = &per_cpu(cpu_state, cpu);
		if (!pcpu->inited || pcpu->first_cpu != cpu)
			continue;
		old_nrrun = pcpu->nrrun;
		/*
		 * Big cluster only need to take care of big tasks, but if
		 * there are not enough big cores, big tasks need to be run
		 * on little as well. Thus for little's runqueue stat, it
		 * has to use overall runqueue average, or derive what big
		 * tasks would have to be run on little. The latter approach
		 * is not easy to get given core control reacts much slower
		 * than scheduler, and can't predict scheduler's behavior.
		 */
		pcpu->nrrun = pcpu->is_big_cluster ? big_avg : avg;
		if (pcpu->nrrun != old_nrrun) {
			if (trigger_update)
				apply_need(pcpu);
			else
				pcpu->nrrun_changed = true;
		}
	}
}

/* adjust needed CPUs based on current runqueue information */
static unsigned int apply_task_need(struct cpu_data *f, unsigned int new_need)
{
	/* Online all cores if there are enough tasks */
	if (f->nrrun >= f->task_thres)
		return f->num_cpus;

	/* only online more cores if there are tasks to run */
	if (f->nrrun > new_need)
		return new_need + 1;

	return new_need;
}

static u64 round_to_nw_start(void)
{
	unsigned long step = msecs_to_jiffies(rq_avg_period_ms);
	u64 jif = get_jiffies_64();

	do_div(jif, step);
	return (jif + 1) * step;
}

static void rq_avg_timer_func(unsigned long not_used)
{
	update_running_avg(true);
	mod_timer(&rq_avg_timer, round_to_nw_start());
}

/* ======================= load based core count  ====================== */

static unsigned int apply_limits(struct cpu_data *f, unsigned int need_cpus)
{
	return min(max(f->min_cpus, need_cpus), f->max_cpus);
}

static bool eval_need(struct cpu_data *f)
{
	unsigned long flags;
	struct cpu_data *c;
	unsigned int need_cpus = 0, last_need, thres_idx;
	int ret = 0;
	bool need_flag = false;
	s64 now;

	if (unlikely(!f->inited))
		return 0;

	spin_lock_irqsave(&state_lock, flags);
	thres_idx = f->online_cpus ? f->online_cpus - 1 : 0;
	list_for_each_entry(c, &f->lru, sib) {
		if (c->busy >= f->busy_up_thres[thres_idx])
			c->is_busy = true;
		else if (c->busy < f->busy_down_thres[thres_idx])
			c->is_busy = false;
		need_cpus += c->is_busy;
	}
	need_cpus = apply_task_need(f, need_cpus);
	need_flag = apply_limits(f, need_cpus) != apply_limits(f, f->need_cpus);
	last_need = f->need_cpus;

	now = core_ctl_get_time();

	if (need_cpus == last_need) {
		f->need_ts = now;
		spin_unlock_irqrestore(&state_lock, flags);
		return 0;
	}

	if (need_cpus > last_need) {
		ret = 1;
	} else if (need_cpus < last_need) {
		s64 elapsed = now - f->need_ts;
		if (elapsed >= f->offline_delay_ms) {
			ret = 1;
		} else {
			mod_timer(&f->timer, jiffies +
				  msecs_to_jiffies(f->offline_delay_ms));
		}
	}

	if (ret) {
		f->need_ts = now;
		f->need_cpus = need_cpus;
	}

	trace_core_ctl_eval_need(f->cpu, last_need, need_cpus, ret && need_flag);
	spin_unlock_irqrestore(&state_lock, flags);

	return ret && need_flag;
}

static void apply_need(struct cpu_data *f)
{
	if (eval_need(f))
		wake_up_hotplug_thread(f);
}

static int core_ctl_set_busy(unsigned int cpu, unsigned int busy)
{
	struct cpu_data *c = &per_cpu(cpu_state, cpu);
	struct cpu_data *f;
	unsigned int old_is_busy = c->is_busy;

	if (!c->inited)
		return 0;
	f = &per_cpu(cpu_state, c->first_cpu);

	update_running_avg(false);
	if (c->busy == busy && !f->nrrun_changed)
		return 0;
	c->busy = busy;
	f->nrrun_changed = false;

	apply_need(f);
	trace_core_ctl_set_busy(cpu, busy, old_is_busy, c->is_busy);
	return 0;
}

/* ========================= core count enforcement ==================== */

/*
 * If current thread is hotplug thread, don't attempt to wake up
 * itself or other hotplug threads because it will deadlock. Instead,
 * schedule a timer to fire in next timer tick and wake up the thread.
 */
static void wake_up_hotplug_thread(struct cpu_data *state)
{
	unsigned long flags;
	int cpu;
	struct cpu_data *pcpu;
	bool no_wakeup = false;

	for_each_possible_cpu(cpu) {
		pcpu = &per_cpu(cpu_state, cpu);
		if (cpu != pcpu->first_cpu)
			continue;
		if (pcpu->hotplug_thread == current) {
			no_wakeup = true;
			break;
		}
	}

	spin_lock_irqsave(&state->pending_lock, flags);
	state->pending = true;
	spin_unlock_irqrestore(&state->pending_lock, flags);

	if (no_wakeup) {
		spin_lock_irqsave(&state_lock, flags);
		mod_timer(&state->timer, jiffies);
		spin_unlock_irqrestore(&state_lock, flags);
	} else {
		wake_up_process(state->hotplug_thread);
	}
}

static void core_ctl_timer_func(unsigned long cpu)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	unsigned long flags;

	if (eval_need(state)) {
		spin_lock_irqsave(&state->pending_lock, flags);
		state->pending = true;
		spin_unlock_irqrestore(&state->pending_lock, flags);
		wake_up_process(state->hotplug_thread);
	}

}

static void update_lru(struct cpu_data *f)
{
	struct cpu_data *c, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&pending_lru_lock, flags);
	spin_lock(&state_lock);

	list_for_each_entry_safe(c, tmp, &f->pending_lru, pending_sib) {
		list_del_init(&c->pending_sib);
		list_del(&c->sib);
		list_add_tail(&c->sib, &f->lru);
	}

	spin_unlock(&state_lock);
	spin_unlock_irqrestore(&pending_lru_lock, flags);
}

static void __ref do_hotplug(struct cpu_data *f)
{
	unsigned int need;
	struct cpu_data *c, *tmp;

	need = apply_limits(f, f->need_cpus);
	pr_debug("Trying to adjust group %u to %u\n", f->first_cpu, need);

	mutex_lock(&lru_lock);
	if (f->online_cpus > need) {
		list_for_each_entry_safe(c, tmp, &f->lru, sib) {
			if (!c->online)
				continue;

			if (f->online_cpus == need)
				break;

			/* Don't offline busy CPUs. */
			if (c->is_busy)
				continue;

			pr_debug("Trying to Offline CPU%u\n", c->cpu);
			if (cpu_down(c->cpu))
				pr_debug("Unable to Offline CPU%u\n", c->cpu);
		}

		/*
		 * If the number of online CPUs is within the limits, then
		 * don't force any busy CPUs offline.
		 */
		if (f->online_cpus <= f->max_cpus)
			goto done;

		list_for_each_entry_safe(c, tmp, &f->lru, sib) {
			if (!c->online)
				continue;

			if (f->online_cpus <= f->max_cpus)
				break;

			pr_debug("Trying to Offline CPU%u\n", c->cpu);
			if (cpu_down(c->cpu))
				pr_debug("Unable to Offline CPU%u\n", c->cpu);
		}
	} else if (f->online_cpus < need) {
		list_for_each_entry_safe(c, tmp, &f->lru, sib) {
			if (c->online || c->rejected || c->not_preferred)
				continue;
			if (f->online_cpus == need)
				break;

			pr_debug("Trying to Online CPU%u\n", c->cpu);
			if (core_ctl_online_core(c->cpu))
				pr_debug("Unable to Online CPU%u\n", c->cpu);
		}

		if (f->online_cpus == need)
			goto done;


		list_for_each_entry_safe(c, tmp, &f->lru, sib) {
			if (c->online || c->rejected || !c->not_preferred)
				continue;
			if (f->online_cpus == need)
				break;

			pr_debug("Trying to Online CPU%u\n", c->cpu);
			if (core_ctl_online_core(c->cpu))
				pr_debug("Unable to Online CPU%u\n", c->cpu);
		}
	}
done:
	mutex_unlock(&lru_lock);
	update_lru(f);
}

static int __ref try_hotplug(void *data)
{
	struct cpu_data *f = data;
	unsigned long flags;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&f->pending_lock, flags);
		if (!f->pending) {
			spin_unlock_irqrestore(&f->pending_lock, flags);
			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&f->pending_lock, flags);
		}
		set_current_state(TASK_RUNNING);
		f->pending = false;
		spin_unlock_irqrestore(&f->pending_lock, flags);

		do_hotplug(f);
	}

	return 0;
}

static void add_to_pending_lru(struct cpu_data *state)
{
	unsigned long flags;
	struct cpu_data *f = &per_cpu(cpu_state, state->first_cpu);

	spin_lock_irqsave(&pending_lru_lock, flags);

	if (!list_empty(&state->pending_sib))
		list_del(&state->pending_sib);
	list_add_tail(&state->pending_sib, &f->pending_lru);

	spin_unlock_irqrestore(&pending_lru_lock, flags);
}

static int __ref cpu_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	uint32_t cpu = (uintptr_t)hcpu;
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cpu_data *f;
	int ret = NOTIFY_OK;
	unsigned long flags;

	/* Don't affect suspend resume */
	if (action & CPU_TASKS_FROZEN)
		return NOTIFY_OK;

	if (unlikely(!state->inited))
		return NOTIFY_OK;

	f = &per_cpu(cpu_state, state->first_cpu);

	switch (action) {
	case CPU_UP_PREPARE:

		/* If online state of CPU somehow got out of sync, fix it. */
		if (state->online) {
			f->online_cpus--;
			state->online = false;
			pr_warn("CPU%d offline when state is online\n", cpu);
		}

		if (state->rejected) {
			state->rejected = false;
			f->avail_cpus++;
		}

		/*
		 * If a CPU is in the process of coming up, mark it as online
		 * so that there's no race with hotplug thread bringing up more
		 * CPUs than necessary.
		 */
		if (apply_limits(f, f->need_cpus) <= f->online_cpus) {
			pr_debug("Prevent CPU%d onlining\n", cpu);
			ret = NOTIFY_BAD;
		} else {
			state->online = true;
			f->online_cpus++;
		}
		break;

	case CPU_ONLINE:
		/*
		 * Moving to the end of the list should only happen in
		 * CPU_ONLINE and not on CPU_UP_PREPARE to prevent an
		 * infinite list traversal when thermal (or other entities)
		 * reject trying to online CPUs.
		 */
		ret = mutex_trylock(&lru_lock);
		if (ret) {
			spin_lock_irqsave(&state_lock, flags);
			list_del(&state->sib);
			list_add_tail(&state->sib, &f->lru);
			spin_unlock_irqrestore(&state_lock, flags);
			mutex_unlock(&lru_lock);
		} else {
			/*
			 * lru_lock is held by our hotplug thread to
			 * prevent concurrent access of lru list. The updates
			 * are maintained in pending_lru list and lru is
			 * updated at the end of do_hotplug().
			 */
			add_to_pending_lru(state);
		}
		break;

	case CPU_DEAD:
		/* Move a CPU to the end of the LRU when it goes offline. */
		ret = mutex_trylock(&lru_lock);
		if (ret) {
			spin_lock_irqsave(&state_lock, flags);
			list_del(&state->sib);
			list_add_tail(&state->sib, &f->lru);
			spin_unlock_irqrestore(&state_lock, flags);
			mutex_unlock(&lru_lock);
		} else {
			add_to_pending_lru(state);
		}
		/* Fall through */

	case CPU_UP_CANCELED:

		/* If online state of CPU somehow got out of sync, fix it. */
		if (!state->online) {
			f->online_cpus++;
			pr_warn("CPU%d online when state is offline\n", cpu);
		}

		if (!state->rejected && action == CPU_UP_CANCELED) {
			state->rejected = true;
			f->avail_cpus--;
		}

		state->online = false;
		state->busy = 0;
		f->online_cpus--;
		break;
	}

	if (f->online_cpus < apply_limits(f, f->need_cpus)
	    && f->online_cpus < f->avail_cpus
	    && action == CPU_DEAD)
		wake_up_hotplug_thread(f);

	return ret;
}

static struct notifier_block __refdata cpu_notifier = {
	.notifier_call = cpu_callback,
};

/* ============================ init code ============================== */

#define HOTPLUG_THREAD_NICE_VAL -7
static int group_init(struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cpu_data *f = &per_cpu(cpu_state, first_cpu);
	struct cpu_data *state;
	unsigned int cpu;

	if (likely(f->inited))
		return 0;

	dev = core_ctl_find_cpu_device(first_cpu);
	if (!dev)
		return -ENODEV;

	pr_info("Creating CPU group %d\n", first_cpu);

	f->num_cpus = cpumask_weight(mask);
	if (f->num_cpus > MAX_CPUS_PER_GROUP) {
		pr_err("HW configuration not supported\n");
		return -EINVAL;
	}
	f->min_cpus = 1;
	f->max_cpus = f->num_cpus;
	f->need_cpus  = f->num_cpus;
	f->avail_cpus  = f->num_cpus;
	f->offline_delay_ms = 100;
	f->task_thres = UINT_MAX;
	f->nrrun = f->num_cpus;
	INIT_LIST_HEAD(&f->lru);
	INIT_LIST_HEAD(&f->pending_lru);
	init_timer(&f->timer);
	spin_lock_init(&f->pending_lock);
	f->timer.function = core_ctl_timer_func;
	f->timer.data = first_cpu;

	for_each_cpu(cpu, mask) {
		pr_info("Init CPU%u state\n", cpu);

		state = &per_cpu(cpu_state, cpu);
		state->cpu = cpu;
		state->first_cpu = first_cpu;

		if (cpu_online(cpu)) {
			f->online_cpus++;
			state->online = true;
		}

		list_add_tail(&state->sib, &f->lru);
		INIT_LIST_HEAD(&state->pending_sib);
	}

	f->hotplug_thread = kthread_run(try_hotplug, (void *) f,
					"core_ctl/%d", first_cpu);
	set_user_nice(f->hotplug_thread, HOTPLUG_THREAD_NICE_VAL);

	for_each_cpu(cpu, mask) {
		state = &per_cpu(cpu_state, cpu);
		state->inited = true;
	}

	kobject_init(&f->kobj, &ktype_core_ctl);
	return kobject_add(&f->kobj, &dev->kobj, "core_ctl");
}

static int cpufreq_policy_cb(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_policy *policy = data;

	switch (val) {
	case CPUFREQ_CREATE_POLICY:
		group_init(policy->related_cpus);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_pol_nb = {
	.notifier_call = cpufreq_policy_cb,
};

static int cpufreq_gov_cb(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_govinfo *info = data;

	switch (val) {
	case CPUFREQ_LOAD_CHANGE:
		core_ctl_set_busy(info->cpu, info->load);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_gov_nb = {
	.notifier_call = cpufreq_gov_cb,
};

static int __init core_ctl_init(void)
{
	struct cpufreq_policy *policy;
	unsigned int cpu;

	register_cpu_notifier(&cpu_notifier);
	cpufreq_register_notifier(&cpufreq_pol_nb, CPUFREQ_POLICY_NOTIFIER);
	cpufreq_register_notifier(&cpufreq_gov_nb, CPUFREQ_GOVINFO_NOTIFIER);
	init_timer_deferrable(&rq_avg_timer);
	rq_avg_timer.function = rq_avg_timer_func;

	core_ctl_block_hotplug();
	for_each_online_cpu(cpu) {
		policy = core_ctl_get_policy(cpu);
		if (policy) {
			group_init(policy->related_cpus);
			core_ctl_put_policy(policy);
		}
	}
	core_ctl_unblock_hotplug();
	mod_timer(&rq_avg_timer, round_to_nw_start());
	return 0;
}

static void __exit core_ctl_exit(void)
{
	int cpu;
	struct cpu_data *pcpu;

	unregister_cpu_notifier(&cpu_notifier);
	cpufreq_unregister_notifier(&cpufreq_pol_nb, CPUFREQ_POLICY_NOTIFIER);
	cpufreq_unregister_notifier(&cpufreq_gov_nb, CPUFREQ_GOVINFO_NOTIFIER);
	del_timer_sync(&rq_avg_timer);

	for_each_possible_cpu(cpu) {
		pcpu = &per_cpu(cpu_state, cpu);
		if (pcpu->inited && cpu == pcpu->first_cpu) {
			pcpu->inited = false;
			del_timer_sync(&pcpu->timer);
			kthread_stop(pcpu->hotplug_thread);
			kobject_put(&pcpu->kobj);
		}
		pcpu->inited = false;
	}
}

module_init(core_ctl_init);
module_exit(core_ctl_exit);

MODULE_DESCRIPTION("MSM Core Control Driver");
MODULE_LICENSE("GPL v2");
