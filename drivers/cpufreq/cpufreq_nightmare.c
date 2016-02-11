/*
 *  drivers/cpufreq/cpufreq_nightmare.c
 *
 *  Copyright (C)  2011 Samsung Electronics co. ltd
 *    ByungChang Cha <bc.cha@samsung.com>
 *
 *  Based on ondemand governor
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * Created by Alucard_24@xda
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/slab.h>
/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

static void do_nightmare_timer(struct work_struct *work);
static int cpufreq_governor_nightmare(struct cpufreq_policy *policy,
				unsigned int event);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_NIGHTMARE
static
#endif
struct cpufreq_governor cpufreq_gov_nightmare = {
	.name                   = "nightmare",
	.governor               = cpufreq_governor_nightmare,
	.owner                  = THIS_MODULE,
};

struct cpufreq_nightmare_cpuinfo {
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_idle;
	struct cpufreq_frequency_table *freq_table;
	struct delayed_work work;
	struct cpufreq_policy *cur_policy;
	int cpu;
	unsigned int enable:1;
	/*
	 * mutex that serializes governor limit change with
	 * do_nightmare_timer invocation. We do not want do_nightmare_timer to run
	 * when user is changing the governor or limits.
	 */
	struct mutex timer_mutex;
};

static DEFINE_PER_CPU(struct cpufreq_nightmare_cpuinfo, od_nightmare_cpuinfo);

static unsigned int nightmare_enable;	/* number of CPUs using this policy */
/*
 * nightmare_mutex protects nightmare_enable in governor start/stop.
 */
static DEFINE_MUTEX(nightmare_mutex);

/*static atomic_t min_freq_limit[NR_CPUS];
static atomic_t max_freq_limit[NR_CPUS];*/

/* nightmare tuners */
static struct nightmare_tuners {
	atomic_t sampling_rate;
	atomic_t inc_cpu_load_at_min_freq;
	atomic_t inc_cpu_load;
	atomic_t dec_cpu_load;
	atomic_t freq_for_responsiveness;
	atomic_t freq_for_responsiveness_max;
	atomic_t freq_up_brake_at_min_freq;
	atomic_t freq_up_brake;
	atomic_t freq_step_at_min_freq;
	atomic_t freq_step;
	atomic_t freq_step_dec;
	atomic_t freq_step_dec_at_max_freq;
} nightmare_tuners_ins = {
	.sampling_rate = ATOMIC_INIT(40000),
	.inc_cpu_load_at_min_freq = ATOMIC_INIT(40),
	.inc_cpu_load = ATOMIC_INIT(70),
	.dec_cpu_load = ATOMIC_INIT(50),
	.freq_for_responsiveness = ATOMIC_INIT(1958400),
	.freq_for_responsiveness_max = ATOMIC_INIT(2534400),
	.freq_step_at_min_freq = ATOMIC_INIT(40),
	.freq_step = ATOMIC_INIT(40),
	.freq_up_brake_at_min_freq = ATOMIC_INIT(30),
	.freq_up_brake = ATOMIC_INIT(30),
	.freq_step_dec = ATOMIC_INIT(10),
	.freq_step_dec_at_max_freq = ATOMIC_INIT(10),
};

/************************** sysfs interface ************************/

/* cpufreq_nightmare Governor Tunables */
#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return sprintf(buf, "%d\n", atomic_read(&nightmare_tuners_ins.object));		\
}
show_one(sampling_rate, sampling_rate);
show_one(inc_cpu_load_at_min_freq, inc_cpu_load_at_min_freq);
show_one(inc_cpu_load, inc_cpu_load);
show_one(dec_cpu_load, dec_cpu_load);
show_one(freq_for_responsiveness, freq_for_responsiveness);
show_one(freq_for_responsiveness_max, freq_for_responsiveness_max);
show_one(freq_step_at_min_freq, freq_step_at_min_freq);
show_one(freq_step, freq_step);
show_one(freq_up_brake_at_min_freq, freq_up_brake_at_min_freq);
show_one(freq_up_brake, freq_up_brake);
show_one(freq_step_dec, freq_step_dec);
show_one(freq_step_dec_at_max_freq, freq_step_dec_at_max_freq);

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updaing
 * nightmare_tuners_ins.sampling_rate might not be appropriate. For example,
 * if the original sampling_rate was 1 second and the requested new sampling
 * rate is 10 ms because the user needs immediate reaction from ondemand
 * governor, but not sure if higher frequency will be required or not,
 * then, the governor may change the sampling rate too late; up to 1 second
 * later. Thus, if we are reducing the sampling rate, we need to make the
 * new value effective immediately.
 */
static void update_sampling_rate(unsigned int new_rate)
{
	int cpu;

	atomic_set(&nightmare_tuners_ins.sampling_rate,new_rate);

	get_online_cpus();
	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct cpufreq_nightmare_cpuinfo *nightmare_cpuinfo;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		nightmare_cpuinfo = &per_cpu(od_nightmare_cpuinfo, policy->cpu);
		cpufreq_cpu_put(policy);

		mutex_lock(&nightmare_cpuinfo->timer_mutex);

		if (!delayed_work_pending(&nightmare_cpuinfo->work)) {
			mutex_unlock(&nightmare_cpuinfo->timer_mutex);
			continue;
		}

		next_sampling  = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = nightmare_cpuinfo->work.timer.expires;


		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&nightmare_cpuinfo->timer_mutex);
			cancel_delayed_work_sync(&nightmare_cpuinfo->work);
			mutex_lock(&nightmare_cpuinfo->timer_mutex);

			queue_delayed_work_on(nightmare_cpuinfo->cpu, system_wq, &nightmare_cpuinfo->work, usecs_to_jiffies(new_rate));
		}
		mutex_unlock(&nightmare_cpuinfo->timer_mutex);
	}
	put_online_cpus();
}

/* sampling_rate */
static ssize_t store_sampling_rate(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(input,10000);
	
	if (input == atomic_read(&nightmare_tuners_ins.sampling_rate))
		return count;

	update_sampling_rate(input);

	return count;
}

/* inc_cpu_load_at_min_freq */
static ssize_t store_inc_cpu_load_at_min_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1) {
		return -EINVAL;
	}

	input = min(input,atomic_read(&nightmare_tuners_ins.inc_cpu_load));

	if (input == atomic_read(&nightmare_tuners_ins.inc_cpu_load_at_min_freq))
		return count;

	atomic_set(&nightmare_tuners_ins.inc_cpu_load_at_min_freq,input);

	return count;
}

/* inc_cpu_load */
static ssize_t store_inc_cpu_load(struct kobject *a, struct attribute *b,
					const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.inc_cpu_load))
		return count;

	atomic_set(&nightmare_tuners_ins.inc_cpu_load,input);

	return count;
}

/* dec_cpu_load */
static ssize_t store_dec_cpu_load(struct kobject *a, struct attribute *b,
					const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,95),5);

	if (input == atomic_read(&nightmare_tuners_ins.dec_cpu_load))
		return count;

	atomic_set(&nightmare_tuners_ins.dec_cpu_load,input);

	return count;
}

/* freq_for_responsiveness */
static ssize_t store_freq_for_responsiveness(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	if (input == atomic_read(&nightmare_tuners_ins.freq_for_responsiveness))
		return count;

	atomic_set(&nightmare_tuners_ins.freq_for_responsiveness,input);

	return count;
}

/* freq_for_responsiveness_max */
static ssize_t store_freq_for_responsiveness_max(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	if (input == atomic_read(&nightmare_tuners_ins.freq_for_responsiveness_max))
		return count;

	atomic_set(&nightmare_tuners_ins.freq_for_responsiveness_max,input);

	return count;
}

/* freq_step_at_min_freq */
static ssize_t store_freq_step_at_min_freq(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.freq_step_at_min_freq))
		return count;

	atomic_set(&nightmare_tuners_ins.freq_step_at_min_freq,input);

	return count;
}

/* freq_step */
static ssize_t store_freq_step(struct kobject *a, struct attribute *b,
			       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.freq_step))
		return count;

	atomic_set(&nightmare_tuners_ins.freq_step,input);

	return count;
}

/* freq_up_brake_at_min_freq */
static ssize_t store_freq_up_brake_at_min_freq(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.freq_up_brake_at_min_freq)) {/* nothing to do */
		return count;
	}

	atomic_set(&nightmare_tuners_ins.freq_up_brake_at_min_freq,input);

	return count;
}

/* freq_up_brake */
static ssize_t store_freq_up_brake(struct kobject *a, struct attribute *b,
				      const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.freq_up_brake)) {/* nothing to do */
		return count;
	}

	atomic_set(&nightmare_tuners_ins.freq_up_brake,input);

	return count;
}

/* freq_step_dec */
static ssize_t store_freq_step_dec(struct kobject *a, struct attribute *b,
				       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.freq_step_dec)) {/* nothing to do */
		return count;
	}

	atomic_set(&nightmare_tuners_ins.freq_step_dec,input);

	return count;
}

/* freq_step_dec_at_max_freq */
static ssize_t store_freq_step_dec_at_max_freq(struct kobject *a, struct attribute *b,
				       const char *buf, size_t count)
{
	int input;
	int ret;

	ret = sscanf(buf, "%d", &input);
	if (ret != 1)
		return -EINVAL;

	input = max(min(input,100),0);

	if (input == atomic_read(&nightmare_tuners_ins.freq_step_dec_at_max_freq)) {/* nothing to do */
		return count;
	}

	atomic_set(&nightmare_tuners_ins.freq_step_dec_at_max_freq,input);

	return count;
}

define_one_global_rw(sampling_rate);
define_one_global_rw(inc_cpu_load_at_min_freq);
define_one_global_rw(inc_cpu_load);
define_one_global_rw(dec_cpu_load);
define_one_global_rw(freq_for_responsiveness);
define_one_global_rw(freq_for_responsiveness_max);
define_one_global_rw(freq_step_at_min_freq);
define_one_global_rw(freq_step);
define_one_global_rw(freq_up_brake_at_min_freq);
define_one_global_rw(freq_up_brake);
define_one_global_rw(freq_step_dec);
define_one_global_rw(freq_step_dec_at_max_freq);

static struct attribute *nightmare_attributes[] = {
	&sampling_rate.attr,
	&inc_cpu_load_at_min_freq.attr,
	&inc_cpu_load.attr,
	&dec_cpu_load.attr,
	&freq_for_responsiveness.attr,
	&freq_for_responsiveness_max.attr,
	&freq_step_at_min_freq.attr,
	&freq_step.attr,
	&freq_up_brake_at_min_freq.attr,
	&freq_up_brake.attr,
	&freq_step_dec.attr,
	&freq_step_dec_at_max_freq.attr,
	NULL
};

static struct attribute_group nightmare_attr_group = {
	.attrs = nightmare_attributes,
	.name = "nightmare",
};

/************************** sysfs end ************************/

static void nightmare_check_cpu(struct cpufreq_nightmare_cpuinfo *this_nightmare_cpuinfo)
{
	struct cpufreq_policy *cpu_policy;
	unsigned int min_freq;
	unsigned int max_freq;
	unsigned int freq_for_responsiveness;
	unsigned int freq_for_responsiveness_max;
	int dec_cpu_load;
	int inc_cpu_load;
	int freq_step;
	int freq_up_brake;
	int freq_step_dec;
	cputime64_t cur_wall_time, cur_idle_time;
	unsigned int wall_time, idle_time;
	unsigned int index = 0;
	unsigned int tmp_freq = 0;
	unsigned int next_freq = 0;
	int cur_load = -1;
	unsigned int cpu;

	cpu = this_nightmare_cpuinfo->cpu;
	cpu_policy = this_nightmare_cpuinfo->cur_policy;

	cur_idle_time = get_cpu_idle_time_us(cpu, NULL);
	cur_idle_time += get_cpu_iowait_time_us(cpu, &cur_wall_time);

	wall_time = (unsigned int)
			(cur_wall_time - this_nightmare_cpuinfo->prev_cpu_wall);
	this_nightmare_cpuinfo->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int)
			(cur_idle_time - this_nightmare_cpuinfo->prev_cpu_idle);
	this_nightmare_cpuinfo->prev_cpu_idle = cur_idle_time;

	/*min_freq = atomic_read(&min_freq_limit[cpu]);
	max_freq = atomic_read(&max_freq_limit[cpu]);*/

	freq_for_responsiveness = atomic_read(&nightmare_tuners_ins.freq_for_responsiveness);
	freq_for_responsiveness_max = atomic_read(&nightmare_tuners_ins.freq_for_responsiveness_max);
	dec_cpu_load = atomic_read(&nightmare_tuners_ins.dec_cpu_load);
	inc_cpu_load = atomic_read(&nightmare_tuners_ins.inc_cpu_load);
	freq_step = atomic_read(&nightmare_tuners_ins.freq_step);
	freq_up_brake = atomic_read(&nightmare_tuners_ins.freq_up_brake);
	freq_step_dec = atomic_read(&nightmare_tuners_ins.freq_step_dec);

	if (!cpu_policy || cpu_policy == NULL)
		return;

	/*printk(KERN_ERR "TIMER CPU[%u], wall[%u], idle[%u]\n",cpu, wall_time, idle_time);*/
	if (wall_time >= idle_time) { /*if wall_time < idle_time, evaluate cpu load next time*/
		cur_load = wall_time > idle_time ? (100 * (wall_time - idle_time)) / wall_time : 1;/*if wall_time is equal to idle_time cpu_load is equal to 1*/
		min_freq = cpu_policy->min;
		max_freq = cpu_policy->max;		
		/* CPUs Online Scale Frequency*/
		if (cpu_policy->cur < freq_for_responsiveness) {
			inc_cpu_load = atomic_read(&nightmare_tuners_ins.inc_cpu_load_at_min_freq);
			freq_step = atomic_read(&nightmare_tuners_ins.freq_step_at_min_freq);
			freq_up_brake = atomic_read(&nightmare_tuners_ins.freq_up_brake_at_min_freq);
		} else if (cpu_policy->cur > freq_for_responsiveness_max) {
			freq_step_dec = atomic_read(&nightmare_tuners_ins.freq_step_dec_at_max_freq);
		}		
		/* Check for frequency increase or for frequency decrease */
		if (cur_load >= inc_cpu_load && cpu_policy->cur < max_freq) {
			tmp_freq = max(min((cpu_policy->cur + ((cur_load + freq_step - freq_up_brake == 0 ? 1 : cur_load + freq_step - freq_up_brake) * 3780)), max_freq), min_freq);
		} else if (cur_load < dec_cpu_load && cpu_policy->cur > min_freq) {
			tmp_freq = max(min((cpu_policy->cur - ((100 - cur_load + freq_step_dec == 0 ? 1 : 100 - cur_load + freq_step_dec) * 3780)), max_freq), min_freq);
		} else {
			/* if cpu frequency is already at maximum or minimum or cur_load is between inc_cpu_load and dec_cpu_load var, we don't need to set frequency!
			return; */
			tmp_freq = cpu_policy->cur;
		}
		cpufreq_frequency_table_target(cpu_policy, this_nightmare_cpuinfo->freq_table, tmp_freq,
			CPUFREQ_RELATION_L, &index);
	 	next_freq = this_nightmare_cpuinfo->freq_table[index].frequency;
		if (next_freq != cpu_policy->cur && cpu_online(cpu)) {
			__cpufreq_driver_target(cpu_policy, next_freq, CPUFREQ_RELATION_L);
		}
	}

}

static void do_nightmare_timer(struct work_struct *work)
{
	struct cpufreq_nightmare_cpuinfo *nightmare_cpuinfo;
	int delay;
	unsigned int cpu;

	nightmare_cpuinfo = container_of(work, struct cpufreq_nightmare_cpuinfo, work.work);
	cpu = nightmare_cpuinfo->cpu;

	mutex_lock(&nightmare_cpuinfo->timer_mutex);
	nightmare_check_cpu(nightmare_cpuinfo);
	/* We want all CPUs to do sampling nearly on
	 * same jiffy
	 */
	delay = usecs_to_jiffies(atomic_read(&nightmare_tuners_ins.sampling_rate));
	if (num_online_cpus() > 1) {
		delay -= jiffies % delay;
	}

	queue_delayed_work_on(cpu, system_wq, &nightmare_cpuinfo->work, delay);
	mutex_unlock(&nightmare_cpuinfo->timer_mutex);
}

static int cpufreq_governor_nightmare(struct cpufreq_policy *policy,
				unsigned int event)
{
	unsigned int cpu;
	struct cpufreq_nightmare_cpuinfo *this_nightmare_cpuinfo;
	int rc, delay;

	cpu = policy->cpu;
	this_nightmare_cpuinfo = &per_cpu(od_nightmare_cpuinfo, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

		mutex_lock(&nightmare_mutex);

		this_nightmare_cpuinfo->cur_policy = policy;

		this_nightmare_cpuinfo->prev_cpu_idle = get_cpu_idle_time_us(cpu, NULL);
		this_nightmare_cpuinfo->prev_cpu_idle += get_cpu_iowait_time_us(cpu, &this_nightmare_cpuinfo->prev_cpu_wall);

		this_nightmare_cpuinfo->freq_table = cpufreq_frequency_get_table(cpu);
		this_nightmare_cpuinfo->cpu = cpu;

		mutex_init(&this_nightmare_cpuinfo->timer_mutex);

		nightmare_enable++;
		/*
		 * Start the timerschedule work, when this governor
		 * is used for first time
		 */
		if (nightmare_enable == 1) {
			rc = sysfs_create_group(cpufreq_global_kobject,
						&nightmare_attr_group);
			if (rc) {
				mutex_unlock(&nightmare_mutex);
				return rc;
			}
		}

		mutex_unlock(&nightmare_mutex);

		delay=usecs_to_jiffies(atomic_read(&nightmare_tuners_ins.sampling_rate));
		if (num_online_cpus() > 1) {
			delay -= jiffies % delay;
		}

		this_nightmare_cpuinfo->enable = 1;
		INIT_DEFERRABLE_WORK(&this_nightmare_cpuinfo->work, do_nightmare_timer);
		queue_delayed_work_on(this_nightmare_cpuinfo->cpu, system_wq, &this_nightmare_cpuinfo->work, delay);

		break;

	case CPUFREQ_GOV_STOP:
		this_nightmare_cpuinfo->enable = 0;
		cancel_delayed_work_sync(&this_nightmare_cpuinfo->work);

		mutex_lock(&nightmare_mutex);
		nightmare_enable--;
		mutex_destroy(&this_nightmare_cpuinfo->timer_mutex);

		if (!nightmare_enable) {
			sysfs_remove_group(cpufreq_global_kobject,
					   &nightmare_attr_group);			
		}
		mutex_unlock(&nightmare_mutex);
		
		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&this_nightmare_cpuinfo->timer_mutex);
		if (policy->max < this_nightmare_cpuinfo->cur_policy->cur)
			__cpufreq_driver_target(this_nightmare_cpuinfo->cur_policy,
				policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > this_nightmare_cpuinfo->cur_policy->cur)
			__cpufreq_driver_target(this_nightmare_cpuinfo->cur_policy,
				policy->min, CPUFREQ_RELATION_L);
		mutex_unlock(&this_nightmare_cpuinfo->timer_mutex);

		break;
	}
	return 0;
}

static int __init cpufreq_gov_nightmare_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_nightmare);
}

static void __exit cpufreq_gov_nightmare_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_nightmare);
}

MODULE_AUTHOR("Alucard24@XDA");
MODULE_DESCRIPTION("'cpufreq_nightmare' - A dynamic cpufreq/cpuhotplug governor v4.1 (SnapDragon)");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_NIGHTMARE
fs_initcall(cpufreq_gov_nightmare_init);
#else
module_init(cpufreq_gov_nightmare_init);
#endif
module_exit(cpufreq_gov_nightmare_exit);
