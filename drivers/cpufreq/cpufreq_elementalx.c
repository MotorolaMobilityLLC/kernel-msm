/*
 *  drivers/cpufreq/cpufreq_elementalx.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2015 Aaron Segaert <asegaert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include "cpufreq_governor.h"

/* elementalx governor macros */
#define DEF_FREQUENCY_UP_THRESHOLD		(90)
#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(20)
#define DEF_ACTIVE_FLOOR_FREQ			(960000)
#define MIN_SAMPLING_RATE			(10000)
#define DEF_SAMPLING_DOWN_FACTOR		(4)
#define MAX_SAMPLING_DOWN_FACTOR		(20)
#define FREQ_NEED_BURST(x)			(x < 800000 ? 1 : 0)
#define MAX(x,y)				(x > y ? x : y)
#define MIN(x,y)				(x < y ? x : y)
#define TABLE_SIZE				12
#define TABLE_NUM				6

static DEFINE_PER_CPU(struct ex_cpu_dbs_info_s, ex_cpu_dbs_info);

static DEFINE_PER_CPU(struct ex_dbs_tuners *, cached_tuners);

static unsigned int up_threshold_level[2] __read_mostly = {95, 85};
 
static struct ex_governor_data {
	unsigned int active_floor_freq;
	unsigned int prev_load;
} ex_data = {
	.active_floor_freq = DEF_ACTIVE_FLOOR_FREQ,
	.prev_load = 0,
};

static unsigned int tblmap[TABLE_NUM][TABLE_SIZE] __read_mostly = {

	//table 0
	{
		616400,
		757200,
		840000,
		960000,
		1248000,
		1324800,
		1478400,
		1593600,
		1632000,
		1728000,
		1824000,
		1996000,
	},

	//table 1
	{
		773040,
		899760,
		1014960,
		1072560,
		1248000,
		1324800,
		1478400,
		1593600,
		1632000,
		1728000,
		1824000,
		1996000,
	},

	//table 2
	{
		851100,
		956700,
		1052700,
		1100700,
		1350400,
		1416000,
		1593600,
		1708800,
		1824000,
		1996000,
		2073000,
		2150000,
	},

	//table 3
	{
		616400,
		757200,
		840000,
		960000,
		1248000,
		1324800,
		1478400,
		1593600,
		1593600,
		1593600,
		1593600,
		1593600,
	},

	//table 4
	{
		773040,
		899760,
		1014960,
		1072560,
		1248000,
		1324800,
		1478400,
		1593600,
		1593600,
		1593600,
		1593600,
		1593600,
	},

	//table 5
	{
		851100,
		956700,
		1052700,
		1100700,
		1324800,
		1416000,
		1593600,
		1708800,
		1708800,
		1708800,
		1708800,
		1708800,
	}

};

static inline int get_cpu_freq_index(unsigned int freq, struct dbs_data *dbs_data)
{
	static int saved_index = 0;
	int index;
	
	if (!dbs_data->freq_table) {
		pr_warn("tbl is NULL, use previous value %d\n", saved_index);
		return saved_index;
	}

	for (index = 0; (dbs_data->freq_table[index].frequency != CPUFREQ_TABLE_END); index++) {
		if (dbs_data->freq_table[index].frequency >= freq) {
			saved_index = index;
			break;
		}
	}

	return index;
}

static inline unsigned int ex_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	if (freq > p->max) {
		return p->max;
	} 
	
	return freq;
}

static void ex_check_cpu(int cpu, unsigned int load)
{
	struct ex_cpu_dbs_info_s *dbs_info = &per_cpu(ex_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int max_load_freq = 0, freq_next = 0;
	unsigned int j, avg_load, cur_freq, max_freq, target_freq = 0;

	cur_freq = policy->cur;
	max_freq = policy->max;

	for_each_cpu(j, policy->cpus) {
		if (load > max_load_freq)
			max_load_freq = load * policy->cur;
	}
	avg_load = (ex_data.prev_load + load) >> 1;

	if (max_load_freq > up_threshold_level[1] * cur_freq) {
		int index = get_cpu_freq_index(cur_freq, dbs_data);

		if (FREQ_NEED_BURST(cur_freq) &&
				load > up_threshold_level[0]) {
			freq_next = max_freq;
		}
		
		else if (avg_load > up_threshold_level[0]) {
			freq_next = tblmap[2 + ex_tuners->powersave][index];
		}
		
		else if (avg_load <= up_threshold_level[1]) {
			freq_next = tblmap[0 + ex_tuners->powersave][index];
		}
	
		else {
			if (load > up_threshold_level[0]) {
				freq_next = tblmap[2 + ex_tuners->powersave][index];
			}
		
			else {
				freq_next = tblmap[1 + ex_tuners->powersave][index];
			}
		}

		target_freq = ex_freq_increase(policy, freq_next);

		__cpufreq_driver_target(policy, target_freq, CPUFREQ_RELATION_H);

		if (target_freq > ex_data.active_floor_freq)
			dbs_info->down_floor = 0;

		goto finished;
	}

	if (cur_freq == policy->min)
		goto finished;

	if (cur_freq >= ex_data.active_floor_freq) {
		if (++dbs_info->down_floor > ex_tuners->sampling_down_factor)
			dbs_info->down_floor = 0;
	} else {
		dbs_info->down_floor = 0;
	}

	if (max_load_freq <
	    (ex_tuners->up_threshold - ex_tuners->down_differential) *
	     cur_freq) {

		freq_next = max_load_freq /
				(ex_tuners->up_threshold -
				 ex_tuners->down_differential);

		if (dbs_info->down_floor) {
			freq_next = MAX(freq_next, ex_data.active_floor_freq);
		} else {
			freq_next = MAX(freq_next, policy->min);
			if (freq_next < ex_data.active_floor_freq)
				dbs_info->down_floor = ex_tuners->sampling_down_factor;
		}

		__cpufreq_driver_target(policy, freq_next,
			CPUFREQ_RELATION_L);
	}

finished:
	ex_data.prev_load = load;
	return;
}

static void ex_dbs_timer(struct work_struct *work)
{
	struct ex_cpu_dbs_info_s *dbs_info = container_of(work,
			struct ex_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct ex_cpu_dbs_info_s *core_dbs_info = &per_cpu(ex_cpu_dbs_info,
			cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	int delay = delay_for_sampling_rate(ex_tuners->sampling_rate);
	bool modify_all = true;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if (!need_load_eval(&core_dbs_info->cdbs, ex_tuners->sampling_rate))
		modify_all = false;
	else
		dbs_check_cpu(dbs_data, cpu);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

/************************** sysfs interface ************************/
static struct common_dbs_data ex_dbs_cdata;

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	ex_tuners->sampling_rate = max(input, dbs_data->min_sampling_rate);
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input <= ex_tuners->down_differential)
		return -EINVAL;

	ex_tuners->up_threshold = input;
	return count;
}

static ssize_t store_down_differential(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input >= ex_tuners->up_threshold)
		return -EINVAL;

	ex_tuners->down_differential = input;
	return count;
}

static ssize_t store_active_floor_freq(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	ex_tuners->active_floor_freq = input;
	ex_data.active_floor_freq = ex_tuners->active_floor_freq;
	return count;
}

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 0)
		return -EINVAL;

	ex_tuners->sampling_down_factor = input;
	return count;
}

static ssize_t store_powersave(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct ex_dbs_tuners *ex_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 1)
		return -EINVAL;

	if (input == 0)
		ex_tuners->powersave = input;
	else if (input == 1)
		ex_tuners->powersave = 3;

	return count;
}

show_store_one(ex, sampling_rate);
show_store_one(ex, up_threshold);
show_store_one(ex, down_differential);
show_store_one(ex, active_floor_freq);
show_store_one(ex, sampling_down_factor);
show_store_one(ex, powersave);
declare_show_sampling_rate_min(ex);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(down_differential);
gov_sys_pol_attr_rw(active_floor_freq);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(powersave);
gov_sys_pol_attr_ro(sampling_rate_min);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&down_differential_gov_sys.attr,
	&active_floor_freq_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&powersave_gov_sys.attr,
	NULL
};

static struct attribute_group ex_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "elementalx",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&down_differential_gov_pol.attr,
	&active_floor_freq_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&powersave_gov_pol.attr,
	NULL
};

static struct attribute_group ex_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "elementalx",
};

/************************** sysfs end ************************/

static void save_tuners(struct cpufreq_policy *policy,
			  struct ex_dbs_tuners *tuners)
{
	int cpu;

	if (have_governor_per_policy())
		cpu = cpumask_first(policy->related_cpus);
	else
		cpu = 0;

	WARN_ON(per_cpu(cached_tuners, cpu) &&
		per_cpu(cached_tuners, cpu) != tuners);
	per_cpu(cached_tuners, cpu) = tuners;
}

static struct ex_dbs_tuners *alloc_tuners(struct cpufreq_policy *policy)
{
	struct ex_dbs_tuners *tuners;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
	tuners->down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL;
	tuners->ignore_nice_load = 0;
	tuners->active_floor_freq = DEF_ACTIVE_FLOOR_FREQ;
	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->powersave = 0;

	save_tuners(policy, tuners);

	return tuners;
}

static struct ex_dbs_tuners *restore_tuners(struct cpufreq_policy *policy)
{
	int cpu;

	if (have_governor_per_policy())
		cpu = cpumask_first(policy->related_cpus);
	else
		cpu = 0;

	return per_cpu(cached_tuners, cpu);
}

static int ex_init(struct dbs_data *dbs_data, struct cpufreq_policy *policy)
{
	struct ex_dbs_tuners *tuners;

	tuners = restore_tuners(policy);
	if (!tuners) {
		tuners = alloc_tuners(policy);
		if (IS_ERR(tuners))
			return PTR_ERR(tuners);
	}

	dbs_data->tuners = tuners;
	dbs_data->min_sampling_rate = MIN_SAMPLING_RATE;
	dbs_data->freq_table = cpufreq_frequency_get_table(policy->cpu);

	mutex_init(&dbs_data->mutex);

	return 0;
}

static void ex_exit(struct dbs_data *dbs_data)
{
	//nothing to do
}

define_get_cpu_dbs_routines(ex_cpu_dbs_info);

static struct common_dbs_data ex_dbs_cdata = {
	.governor = GOV_ELEMENTALX,
	.attr_group_gov_sys = &ex_attr_group_gov_sys,
	.attr_group_gov_pol = &ex_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = ex_dbs_timer,
	.gov_check_cpu = ex_check_cpu,
	.init_ex = ex_init,
	.exit = ex_exit,
};

static int ex_cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	return cpufreq_governor_dbs(policy, &ex_dbs_cdata, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_ELEMENTALX
static
#endif
struct cpufreq_governor cpufreq_gov_elementalx = {
	.name			= "elementalx",
	.governor		= ex_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_elementalx);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	int cpu;

	cpufreq_unregister_governor(&cpufreq_gov_elementalx);
	for_each_possible_cpu(cpu) {
		kfree(per_cpu(cached_tuners, cpu));
		per_cpu(cached_tuners, cpu) = NULL;
	}

}

MODULE_AUTHOR("Aaron Segaert <asegaert@gmail.com>");
MODULE_DESCRIPTION("'cpufreq_elementalx' - multiphase cpufreq governor");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ELEMENTALX
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
