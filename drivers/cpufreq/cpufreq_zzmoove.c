/*
 *  drivers/cpufreq/cpufreq_zzmoove.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *            (C)  2009 Alexander Clouter <alex@digriz.org.uk>
 *            (C)  2012 Michael Weingaertner <mialwe@googlemail.com>
 *                      Zane Zaminsky <cyxman@yahoo.com>
 *                      Jean-Pierre Rasquin <yank555.lu@gmail.com>
 *                      ffolkes <ffolkes@ffolkes.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * -------------------------------------------------------------------------------------------------------------------------------------------------------
 * -  Description:																	 -
 * -------------------------------------------------------------------------------------------------------------------------------------------------------
 *
 * 'ZZMoove' governor is based on the modified 'conservative' (original author Alexander Clouter <alex@digriz.org.uk>) 'smoove' governor from Michael
 * Weingaertner <mialwe@googlemail.com> (source: https://github.com/mialwe/mngb/) ported/modified/optimzed for I9300 since November 2012 and further
 * improved for exynos and snapdragon platform (but also working on other platforms like OMAP) by ZaneZam,Yank555 and ffolkes in 2013/14/15/16
 *
 * --------------------------------------------------------------------------------------------------------------------------------------------------------
 * -																			  -
 * --------------------------------------------------------------------------------------------------------------------------------------------------------
 */

#include <linux/slab.h>
#include "cpufreq_governor.h"

// ZZ: for version information tunable
#define ZZMOOVE_VERSION "bLE-develop"

/* ZZMoove governor macros */
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_FREQUENCY_DOWN_THRESHOLD		(40)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(10)
#define DEF_SMOOTH_UP				(75)	// ZZ: default cpu load trigger for 'boosting' scaling frequency
#define DEF_SCALING_PROPORTIONAL		(0)	// ZZ: default for proportional scaling, disabled here
#define DEF_FAST_SCALING_UP			(0)	// Yank: default fast scaling for upscaling
#define DEF_FAST_SCALING_DOWN			(0)	// Yank: default fast scaling for downscaling
#define DEF_AFS_THRESHOLD1			(25)	// ZZ: default auto fast scaling step one
#define DEF_AFS_THRESHOLD2			(50)	// ZZ: default auto fast scaling step two
#define DEF_AFS_THRESHOLD3			(75)	// ZZ: default auto fast scaling step three
#define DEF_AFS_THRESHOLD4			(90)	// ZZ: default auto fast scaling step four

static DEFINE_PER_CPU(struct zz_cpu_dbs_info_s, zz_cpu_dbs_info);

static DEFINE_PER_CPU(struct zz_dbs_tuners *, cached_tuners);

static struct zz_governor_data {
	unsigned int prev_load;
} zz_data = {
	.prev_load = 0,
};

// ZZ: function for frequency table order detection and limit optimization
static inline void evaluate_scaling_order_limit_range(struct dbs_data *dbs_data)
{
	int freq_table_size = 0;
	bool freq_table_desc = false;
	unsigned int max_scaling_freq_hard = 0;
	unsigned int max_scaling_freq_soft = 0;
	unsigned int min_scaling_freq_hard = 0;
	unsigned int min_scaling_freq = 0;
	unsigned int limit_table_start = 0;
	unsigned int limit_table_end = CPUFREQ_TABLE_END;
	int i = 0;
	int calc_index = 0;

	// ZZ: initialisation of freq search in scaling table
	for (i = 0; (likely(dbs_data->freq_table[i].frequency != CPUFREQ_TABLE_END)); i++) {
		if (unlikely(dbs_data->pol_max == dbs_data->freq_table[i].frequency))
			max_scaling_freq_hard = max_scaling_freq_soft = i;		// ZZ: init soft and hard max value
		if (unlikely(dbs_data->pol_min == dbs_data->freq_table[i].frequency))
			min_scaling_freq_hard = i;					// ZZ: init hard min value
	/*
	 * Yank: continue looping until table end is reached, 
	 * we need this to set the table size limit below
	 */
	}

	freq_table_size = i - 1;						// Yank: upper index limit of freq. table

        /*
         * ZZ: we have to take care about where we are in the frequency table. when using kernel sources without OC capability
         * it might be that the first few indexes are containg no frequencies so a save index start point is needed.
         */
	calc_index = freq_table_size - max_scaling_freq_hard;			// ZZ: calculate the difference and use it as start point

	if (calc_index == freq_table_size)					// ZZ: if we are at the end of the table
		calc_index = calc_index - 1;					// ZZ: shift in range for order calculation below

        // Yank: assert if CPU freq. table is in ascending or descending order
	if (dbs_data->freq_table[calc_index].frequency > dbs_data->freq_table[calc_index+1].frequency) {
		freq_table_desc = true;						// Yank: table is in descending order as expected, lowest freq at the bottom of the table
		min_scaling_freq = i - 1;					// Yank: last valid frequency step (lowest frequency)
		limit_table_start = max_scaling_freq_soft;			// ZZ: we should use the actual max scaling soft limit value as search start point
        } else {
		freq_table_desc = false;					// Yank: table is in ascending order, lowest freq at the top of the table
		min_scaling_freq = 0;						// Yank: first valid frequency step (lowest frequency)
		limit_table_start = min_scaling_freq_hard;			// ZZ: we should use the actual min scaling hard limit value as search start point
		limit_table_end = dbs_data->pol_max;				// ZZ: end searching at highest frequency limit
        }

	// ZZ: save values in policy dbs structure
	dbs_data->freq_table_desc = freq_table_desc;
	dbs_data->freq_table_size = freq_table_size;
	dbs_data->min_scaling_freq = min_scaling_freq;
	dbs_data->limit_table_start = limit_table_start;
	dbs_data->limit_table_end = limit_table_end;
	dbs_data->max_scaling_freq_hard = max_scaling_freq_hard;
	dbs_data->max_scaling_freq_soft = max_scaling_freq_soft;
}

// Yank: return a valid value between min and max
static int validate_min_max(int val, int min, int max)
{
	return min(max(val, min), max);
}

// ZZ: system table scaling mode with freq search optimizations and proportional frequency target option
static inline int zz_get_next_freq(unsigned int curfreq, unsigned int updown, unsigned int load, struct dbs_data *dbs_data)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	int i = 0;
	unsigned int prop_target = 0;
	unsigned int zz_target = 0;
	unsigned int dead_band_freq = 0;					// ZZ: proportional freq, system table freq, dead band freq
	int smooth_up_steps = 0;						// Yank: smooth up steps
	int scaling_mode_up = 0;
	int scaling_mode_down = 0;
	static int tmp_limit_table_start = 0;
	static int tmp_max_scaling_freq_soft = 0;
	static int tmp_limit_table_end = 0;

	prop_target = dbs_data->pol_min + load * (dbs_data->pol_max - dbs_data->pol_min) / 100;		// ZZ: prepare proportional target freq whitout deadband (directly mapped to min->max load)

	if (zz_tuners->scaling_proportional == 2)				// ZZ: mode '2' use proportional target frequencies only
	    return prop_target;

	if (zz_tuners->scaling_proportional == 3) {				// ZZ: mode '3' use proportional target frequencies only and switch to pol_min in deadband range
	    dead_band_freq = dbs_data->pol_max / 100 * load;			// ZZ: use old calculation to get deadband frequencies (=lower than pol_min)
	    if (dead_band_freq > dbs_data->pol_min)				// ZZ: the load usually is too unsteady so we rarely would reach pol_min when load is low
		return prop_target;						// ZZ: in fact it only will happen when load=0, so only return proportional frequencies if they
	    else								//     are out of deadband range and if we are in deadband range return min freq
		return dbs_data->pol_min;					//     (thats a similar behaving as with old propotional freq calculation)
	}

	if (load <= zz_tuners->smooth_up)					// Yank: consider smooth up
	    smooth_up_steps = 0;						// Yank: load not reached, move by one step
	else
	    smooth_up_steps = 1;						// Yank: load reached, move by two steps

	tmp_limit_table_start = dbs_data->limit_table_start;			// ZZ: first assign new limits...
	tmp_limit_table_end = dbs_data->limit_table_end;
	tmp_max_scaling_freq_soft = dbs_data->max_scaling_freq_soft;

	// ZZ: asc: min freq limit changed
	if (!dbs_data->freq_table_desc && curfreq
	    < dbs_data->freq_table[dbs_data->min_scaling_freq].frequency)	// ZZ: asc: but reset starting index if current freq is lower than soft/hard min limit otherwise we are
	    tmp_limit_table_start = 0;						//     shifting out of range and proportional freq is used instead because freq can't be found by loop

	// ZZ: asc: max freq limit changed
	if (!dbs_data->freq_table_desc && curfreq
	    > dbs_data->freq_table[dbs_data->max_scaling_freq_soft].frequency)	// ZZ: asc: but reset ending index if current freq is higher than soft/hard max limit otherwise we are
	    tmp_limit_table_end = dbs_data->freq_table[dbs_data->freq_table_size].frequency;	//     shifting out of range and proportional freq is used instead because freq can't be found by loop

	// ZZ: desc: max freq limit changed
	if (dbs_data->freq_table_desc && curfreq
	    > dbs_data->freq_table[dbs_data->limit_table_start].frequency)	// ZZ: desc: but reset starting index if current freq is higher than soft/hard max limit otherwise we are
	    tmp_limit_table_start = 0;						//     shifting out of range and proportional freq is used instead because freq can't be found by loop

	// ZZ: feq search loop with optimization
	if (dbs_data->freq_table_desc) {
	    for (i = tmp_limit_table_start; (likely(dbs_data->freq_table[i].frequency != tmp_limit_table_end)); i++) {
		if (unlikely(curfreq == dbs_data->freq_table[i].frequency)) {	// Yank: we found where we currently are (i)
		    if (updown == 1) {						// Yank: scale up, but don't go above softlimit
			zz_target = min(dbs_data->freq_table[tmp_max_scaling_freq_soft].frequency,
		        dbs_data->freq_table[validate_min_max(i - 1 - smooth_up_steps - scaling_mode_up, 0, dbs_data->freq_table_size)].frequency);
			if (zz_tuners->scaling_proportional == 1)		// ZZ: if proportional scaling is enabled
			    return min(zz_target, prop_target);			// ZZ: check which freq is lower and return it
			else
			    return zz_target;					// ZZ: or return the found system table freq as usual
		    } else {							// Yank: scale down, but don't go below min. freq.
			zz_target = max(dbs_data->freq_table[dbs_data->min_scaling_freq].frequency,
		        dbs_data->freq_table[validate_min_max(i + 1 + scaling_mode_down, 0, dbs_data->freq_table_size)].frequency);
			if (zz_tuners->scaling_proportional == 1)		// ZZ: if proportional scaling is enabled
			    return min(zz_target, prop_target);			// ZZ: check which freq is lower and return it
			else
			    return zz_target;					// ZZ: or return the found system table freq as usual
		    }
		}
	    }									// ZZ: this shouldn't happen but if the freq is not found in system table
	    return prop_target;							//     fall back to proportional freq target to avoid returning 0
	} else {
	    for (i = tmp_limit_table_start; (likely(dbs_data->freq_table[i].frequency <= tmp_limit_table_end)); i++) {
		if (unlikely(curfreq == dbs_data->freq_table[i].frequency)) {	// Yank: we found where we currently are (i)
		    if (updown == 1) {						// Yank: scale up, but don't go above softlimit
			zz_target = min(dbs_data->freq_table[tmp_max_scaling_freq_soft].frequency,
			dbs_data->freq_table[validate_min_max(i + 1 + smooth_up_steps + scaling_mode_up, 0, dbs_data->freq_table_size)].frequency);
			if (zz_tuners->scaling_proportional == 1)		// ZZ: if proportional scaling is enabled
			    return min(zz_target, prop_target);			// ZZ: check which freq is lower and return it
			else
			    return zz_target;					// ZZ: or return the found system table freq as usual
		    } else {							// Yank: scale down, but don't go below min. freq.
			zz_target = max(dbs_data->freq_table[dbs_data->min_scaling_freq].frequency,
			dbs_data->freq_table[validate_min_max(i - 1 - scaling_mode_down, 0, dbs_data->freq_table_size)].frequency);
			if (zz_tuners->scaling_proportional == 1)		// ZZ: if proportional scaling is enabled
			    return min(zz_target, prop_target);			// ZZ: check which freq is lower and return it
			else
			    return zz_target;					// ZZ: or return the found system table freq as usual
		    }
		}
	    }									// ZZ: this shouldn't happen but if the freq is not found in system table
	    return prop_target;							//     fall back to proportional freq target to avoid returning 0
	}
}

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Every sampling_rate *
 * sampling_down_factor, we check, if current idle time is more than 80%
 * (default), then we try to decrease frequency
 *
 * Any frequency increase takes it to the maximum frequency. Frequency reduction
 * happens at minimum steps of 5% (default) of maximum frequency
 */
static void zz_check_cpu(int cpu, unsigned int load)
{
	struct zz_cpu_dbs_info_s *dbs_info = &per_cpu(zz_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	struct dbs_data *dbs_data = policy->governor_data;
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;

	/*
	 * ZZ/Yank: Auto fast scaling mode
	 * Switch to all 4 fast scaling modes depending on load gradient
	 * the mode will start switching at given afs threshold load changes in both directions
	 */
	if (zz_tuners->fast_scaling_up       > 4) {
	    if (load > zz_data.prev_load && load - zz_data.prev_load <= zz_tuners->afs_threshold1) {
		dbs_data->scaling_mode_up = 0;
	    } else if (load - zz_data.prev_load <= zz_tuners->afs_threshold2) {
		dbs_data->scaling_mode_up = 1;
	    } else if (load - zz_data.prev_load <= zz_tuners->afs_threshold3) {
		dbs_data->scaling_mode_up = 2;
	    } else if (load - zz_data.prev_load <= zz_tuners->afs_threshold4) {
		dbs_data->scaling_mode_up = 3;
	    } else {
		dbs_data->scaling_mode_up = 4;
	    }
	}

	if (zz_tuners->fast_scaling_down       > 4) {
	  if (load < zz_data.prev_load && zz_data.prev_load - load <= zz_tuners->afs_threshold1) {
		dbs_data->scaling_mode_down = 0;
	    } else if (zz_data.prev_load - load <= zz_tuners->afs_threshold2) {
		dbs_data->scaling_mode_down = 1;
	    } else if (zz_data.prev_load - load <= zz_tuners->afs_threshold3) {
		dbs_data->scaling_mode_down = 2;
	    } else if (zz_data.prev_load - load <= zz_tuners->afs_threshold4) {
		dbs_data->scaling_mode_down = 3;
	    } else {
		dbs_data->scaling_mode_down = 4;
	    }
	}

	/* Check for frequency increase */
	if (load > zz_tuners->up_threshold) {
		dbs_info->down_skip = 0;

		/* if we are already at full speed then break out early */
		if (dbs_info->requested_freq == policy->max)
			return;

		dbs_info->requested_freq = zz_get_next_freq(policy->cur, 1, load, dbs_data);
		
		if (dbs_info->requested_freq > policy->max)
			dbs_info->requested_freq = policy->max;

		__cpufreq_driver_target(policy, dbs_info->requested_freq,
			CPUFREQ_RELATION_H);
		return;
	}

	/* if sampling_down_factor is active break out early */
	if (++dbs_info->down_skip < zz_tuners->sampling_down_factor)
		return;
	dbs_info->down_skip = 0;

	/* Check for frequency decrease */
	if (load < zz_tuners->down_threshold) {
		/*
		 * if we cannot reduce the frequency anymore, break out early
		 */
		if (policy->cur == policy->min)
			return;

		dbs_info->requested_freq = zz_get_next_freq(policy->cur, 0, load, dbs_data);
		
		__cpufreq_driver_target(policy, dbs_info->requested_freq,
				CPUFREQ_RELATION_L);
		return;
	}
	zz_data.prev_load = load;
}

static void zz_dbs_timer(struct work_struct *work)
{
	struct zz_cpu_dbs_info_s *dbs_info = container_of(work,
			struct zz_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct zz_cpu_dbs_info_s *core_dbs_info = &per_cpu(zz_cpu_dbs_info,
			cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	int delay = delay_for_sampling_rate(zz_tuners->sampling_rate);
	bool modify_all = true;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);
	if (!need_load_eval(&core_dbs_info->cdbs, zz_tuners->sampling_rate))
		modify_all = false;
	else
		dbs_check_cpu(dbs_data, cpu);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, modify_all);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

static int dbs_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
		void *data)
{
	struct cpufreq_freqs *freq = data;
	struct zz_cpu_dbs_info_s *dbs_info =
					&per_cpu(zz_cpu_dbs_info, freq->cpu);
	struct cpufreq_policy *policy;

	if (!dbs_info->enable)
		return 0;

	policy = dbs_info->cdbs.cur_policy;

	/*
	 * we only care if our internally tracked freq moves outside the 'valid'
	 * ranges of frequency available to us otherwise we do not change it
	*/
	if (dbs_info->requested_freq > policy->max
			|| dbs_info->requested_freq < policy->min)
		dbs_info->requested_freq = freq->new;

	return 0;
}

/************************** sysfs interface ************************/
static struct common_dbs_data zz_dbs_cdata;

static ssize_t store_sampling_down_factor(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;

	zz_tuners->sampling_down_factor = input;
	return count;
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	zz_tuners->sampling_rate = max(input, dbs_data->min_sampling_rate);
	return count;
}

static ssize_t store_up_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 100 || input <= zz_tuners->down_threshold)
		return -EINVAL;

	zz_tuners->up_threshold = input;
	return count;
}

static ssize_t store_down_threshold(struct dbs_data *dbs_data, const char *buf,
		size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	/* cannot be lower than 11 otherwise freq will not fall */
	if (ret != 1 || input < 11 || input > 100 ||
			input >= zz_tuners->up_threshold)
		return -EINVAL;

	zz_tuners->down_threshold = input;
	return count;
}

static ssize_t store_ignore_nice_load(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input, j;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == zz_tuners->ignore_nice_load) /* nothing to do */
		return count;

	zz_tuners->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	for_each_online_cpu(j) {
		struct zz_cpu_dbs_info_s *dbs_info;
		dbs_info = &per_cpu(zz_cpu_dbs_info, j);
		dbs_info->cdbs.prev_cpu_idle = get_cpu_idle_time(j,
					&dbs_info->cdbs.prev_cpu_wall, 0);
		if (zz_tuners->ignore_nice_load)
			dbs_info->cdbs.prev_cpu_nice =
				kcpustat_cpu(j).cpustat[CPUTIME_NICE];
	}
	return count;
}

// ZZ: tuneable -> possible values: range from 1 to 100, if not set default is 75
static ssize_t store_smooth_up(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1 || input > 100 || input < 1)
	    return -EINVAL;

	zz_tuners->smooth_up = input;

	return count;
}

/*
 * ZZ: tuneable scaling proportinal -> possible values: 0 to disable, 
 * 1 to enable comparision between proportional and optimized freq, 
 * 2 to enable propotional freq usage only
 * 3 to enable propotional freq usage only but with dead brand range 
 * to avoid not reaching of pol min freq, 
 * if not set default is 0
 */
static ssize_t store_scaling_proportional(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input < 0 || input > 3)
	    return -EINVAL;

	zz_tuners->scaling_proportional = input;

	return count;
}

/*
 * Yank: tuneable -> possible values 1-4 to enable fast scaling 
 * and 5 for auto fast scaling (insane scaling)
 */
static ssize_t store_fast_scaling_up(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 5 || input < 0)
	    return -EINVAL;

	zz_tuners->fast_scaling_up = input;

	if (input > 4)				// ZZ: auto fast scaling mode
	    return count;

	dbs_data->scaling_mode_up = input;	// Yank: fast scaling up only

	return count;
}

/*
* Yank: tuneable -> possible values 1-4 to enable fast scaling 
* and 5 for auto fast scaling (insane scaling)
*/
static ssize_t store_fast_scaling_down(struct dbs_data *dbs_data,
		const char *buf, size_t count)
{
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > 5 || input < 0)
	    return -EINVAL;

	zz_tuners->fast_scaling_down = input;

	if (input > 4)				// ZZ: auto fast scaling mode
	    return count;

	dbs_data->scaling_mode_down = input;	// Yank: fast scaling up only

	return count;
}

// ZZ: afs tuneable -> possible values from 0 to 100
#define store_afs_threshold(name)					\
static ssize_t store_afs_threshold##name(struct dbs_data *dbs_data,	\
		const char *buf, size_t count)				\
{									\
	struct zz_dbs_tuners *zz_tuners = dbs_data->tuners;		\
	unsigned int input;						\
	int ret;							\
									\
	ret = sscanf(buf, "%u", &input);				\
									\
	if (ret != 1 || input > 100 || input < 0)			\
	    return -EINVAL;						\
									\
	zz_tuners->afs_threshold##name = input;				\
									\
	return count;							\
}									\

store_afs_threshold(1);
store_afs_threshold(2);
store_afs_threshold(3);
store_afs_threshold(4);

// ZZ: show zzmoove version info in sysfs
#define declare_show_version(_gov)					\
static ssize_t show_version_gov_sys					\
	(struct kobject *kobj, struct attribute *attr, char *buf)	\
{									\
	return sprintf(buf, "%s\n", ZZMOOVE_VERSION);			\
}									\
									\
static ssize_t show_version_gov_pol					\
	(struct cpufreq_policy *policy, char *buf)			\
{									\
	return sprintf(buf, "%s\n", ZZMOOVE_VERSION);			\
}

show_store_one(zz, sampling_rate);
show_store_one(zz, sampling_down_factor);
show_store_one(zz, up_threshold);
show_store_one(zz, down_threshold);
show_store_one(zz, ignore_nice_load);
show_store_one(zz, smooth_up);
show_store_one(zz, scaling_proportional);
show_store_one(zz, fast_scaling_up);
show_store_one(zz, fast_scaling_down);
show_store_one(zz, afs_threshold1);
show_store_one(zz, afs_threshold2);
show_store_one(zz, afs_threshold3);
show_store_one(zz, afs_threshold4);
declare_show_version(zz);
declare_show_sampling_rate_min(zz);

gov_sys_pol_attr_rw(sampling_rate);
gov_sys_pol_attr_rw(sampling_down_factor);
gov_sys_pol_attr_rw(up_threshold);
gov_sys_pol_attr_rw(down_threshold);
gov_sys_pol_attr_rw(ignore_nice_load);
gov_sys_pol_attr_rw(smooth_up);
gov_sys_pol_attr_rw(scaling_proportional);
gov_sys_pol_attr_rw(fast_scaling_up);
gov_sys_pol_attr_rw(fast_scaling_down);
gov_sys_pol_attr_rw(afs_threshold1);
gov_sys_pol_attr_rw(afs_threshold2);
gov_sys_pol_attr_rw(afs_threshold3);
gov_sys_pol_attr_rw(afs_threshold4);
gov_sys_pol_attr_ro(version);
gov_sys_pol_attr_ro(sampling_rate_min);

static struct attribute *dbs_attributes_gov_sys[] = {
	&version_gov_sys.attr,
	&sampling_rate_min_gov_sys.attr,
	&sampling_rate_gov_sys.attr,
	&sampling_down_factor_gov_sys.attr,
	&up_threshold_gov_sys.attr,
	&down_threshold_gov_sys.attr,
	&ignore_nice_load_gov_sys.attr,
	&smooth_up_gov_sys.attr,
	&scaling_proportional_gov_sys.attr,
	&fast_scaling_up_gov_sys.attr,
	&fast_scaling_down_gov_sys.attr,
	&afs_threshold1_gov_sys.attr,
	&afs_threshold2_gov_sys.attr,
	&afs_threshold3_gov_sys.attr,
	&afs_threshold4_gov_sys.attr,
	NULL
};

static struct attribute_group zz_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "zzmoove",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&version_gov_pol.attr,
	&sampling_rate_min_gov_pol.attr,
	&sampling_rate_gov_pol.attr,
	&sampling_down_factor_gov_pol.attr,
	&up_threshold_gov_pol.attr,
	&down_threshold_gov_pol.attr,
	&ignore_nice_load_gov_pol.attr,
	&smooth_up_gov_pol.attr,
	&scaling_proportional_gov_pol.attr,
	&fast_scaling_up_gov_pol.attr,
	&fast_scaling_down_gov_pol.attr,
	&afs_threshold1_gov_pol.attr,
	&afs_threshold2_gov_pol.attr,
	&afs_threshold3_gov_pol.attr,
	&afs_threshold4_gov_pol.attr,
	NULL
};

static struct attribute_group zz_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "zzmoove",
};

/************************** sysfs end ************************/

static void save_tuners(struct cpufreq_policy *policy,
		struct zz_dbs_tuners *tuners)
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

static struct zz_dbs_tuners *alloc_tuners(struct cpufreq_policy *policy)
{
	struct zz_dbs_tuners *tuners;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners) {
	    pr_err("%s: kzalloc failed\n", __func__);
	    return ERR_PTR(-ENOMEM);
	}

	tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
	tuners->down_threshold = DEF_FREQUENCY_DOWN_THRESHOLD;
	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice_load = 0;
	tuners->smooth_up = DEF_SMOOTH_UP;
	tuners->scaling_proportional = DEF_SCALING_PROPORTIONAL;
	tuners->fast_scaling_up = DEF_FAST_SCALING_UP;
	tuners->fast_scaling_down = DEF_FAST_SCALING_DOWN;
	tuners->afs_threshold1 = DEF_AFS_THRESHOLD1;
	tuners->afs_threshold2 = DEF_AFS_THRESHOLD2;
	tuners->afs_threshold3 = DEF_AFS_THRESHOLD3;
	tuners->afs_threshold4 = DEF_AFS_THRESHOLD4;

	save_tuners(policy, tuners);

	return tuners;
}

static struct zz_dbs_tuners *restore_tuners(struct cpufreq_policy *policy)
{
	int cpu;

	if (have_governor_per_policy())
	    cpu = cpumask_first(policy->related_cpus);
	else
	    cpu = 0;

	return per_cpu(cached_tuners, cpu);
}

static int zz_init(struct dbs_data *dbs_data, struct cpufreq_policy *policy)
{
	struct zz_dbs_tuners *tuners;

	tuners = restore_tuners(policy);
	if (!tuners) {
		tuners = alloc_tuners(policy);
		if (IS_ERR(tuners))
			return PTR_ERR(tuners);
	}

	dbs_data->tuners = tuners;
	dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
		jiffies_to_usecs(10);
	dbs_data->freq_table = cpufreq_frequency_get_table(policy->cpu);
	dbs_data->pol_min = policy->min;
	dbs_data->pol_max = policy->max;
	evaluate_scaling_order_limit_range(dbs_data);
	mutex_init(&dbs_data->mutex);
	return 0;
}

static void zz_exit(struct dbs_data *dbs_data)
{
//	nothing to do
}

define_get_cpu_dbs_routines(zz_cpu_dbs_info);

static struct notifier_block zz_cpufreq_notifier_block = {
	.notifier_call = dbs_cpufreq_notifier,
};

static struct zz_ops zz_ops = {
	.notifier_block = &zz_cpufreq_notifier_block,
};

static struct common_dbs_data zz_dbs_cdata = {
	.governor = GOV_ZZMOOVE,
	.attr_group_gov_sys = &zz_attr_group_gov_sys,
	.attr_group_gov_pol = &zz_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = zz_dbs_timer,
	.gov_check_cpu = zz_check_cpu,
	.gov_ops = &zz_ops,
	.init_zz = zz_init,
	.exit = zz_exit,
};

static int zz_cpufreq_governor_dbs(struct cpufreq_policy *policy,
				   unsigned int event)
{
	return cpufreq_governor_dbs(policy, &zz_dbs_cdata, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_ZZMOOVE
static
#endif
struct cpufreq_governor cpufreq_gov_zzmoove = {
	.name			= "zzmoove",
	.governor		= zz_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_zzmoove);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	int cpu;

	cpufreq_unregister_governor(&cpufreq_gov_zzmoove);
	for_each_possible_cpu(cpu) {
		kfree(per_cpu(cached_tuners, cpu));
		per_cpu(cached_tuners, cpu) = NULL;
	}
}

MODULE_AUTHOR("Zane Zaminsky <cyxman@yahoo.com>");
MODULE_DESCRIPTION("'cpufreq_zzmoove' - A dynamic cpufreq governor based "
	"on smoove governor from Michael Weingaertner which was originally based on "
	"conservative governor from Alexander Clouter. Optimized for use with Samsung I9300 "
	"using a fast scaling logic - ported/modified/optimized for I9300 since November 2012 "
	"and further improved for exynos and snapdragon platform "
	"by ZaneZam,Yank555 and ffolkes in 2013/14/15/16");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_ZZMOOVE
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
