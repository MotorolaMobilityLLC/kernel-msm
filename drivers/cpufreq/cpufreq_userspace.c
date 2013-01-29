
/*
 *  linux/drivers/cpufreq/cpufreq_userspace.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2002 - 2004 Dominik Brodowski <linux@brodo.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

/**
 * A few values needed by the userspace governor
 */
static DEFINE_PER_CPU(unsigned int, cpu_max_freq);
static DEFINE_PER_CPU(unsigned int, cpu_min_freq);
static DEFINE_PER_CPU(unsigned int, cpu_cur_freq); /* current CPU freq */
static DEFINE_PER_CPU(unsigned int, cpu_set_freq); /* CPU freq desired by
							userspace */
static DEFINE_PER_CPU(unsigned int, cpu_is_managed);
static DEFINE_PER_CPU(struct cpufreq_policy *, temp_policy);
static DEFINE_MUTEX(userspace_mutex);
static int cpus_using_userspace_governor;

/* keep track of frequency transitions */
static int
userspace_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
	void *data)
{
	struct cpufreq_freqs *freq = data;

	if (!per_cpu(cpu_is_managed, freq->cpu))
		return 0;

	if (val == CPUFREQ_POSTCHANGE) {
		pr_debug("saving cpu_cur_freq of cpu %u to be %u kHz\n",
				freq->cpu, freq->new);
		per_cpu(cpu_cur_freq, freq->cpu) = freq->new;
	}

	return 0;
}

static struct notifier_block userspace_cpufreq_notifier_block = {
	.notifier_call  = userspace_cpufreq_notifier
};
//=========dvfs stress test ===================
extern struct cpufreq_frequency_table * cpufreq_frequency_get_table(unsigned int cpu);
struct cpufreq_frequency_table *cpu_freq_table=NULL;
struct workqueue_struct *dvfs_test_work_queue=NULL;
struct delayed_work stress_test;
bool stress_test_enable=false;
unsigned int  max_cpu_freq_index=CPUFREQ_TABLE_END;
int  freq_index=0;
static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq);
static bool cpufreq_set_userspace_governor(void);
void dvfs_stress_test(struct work_struct *work)
{
	int cpu=0;
	static int start_testing=0;
	if(!start_testing){
		cpufreq_set_userspace_governor();
		start_testing=1;
	}
	cpu=0;

	if(stress_test_enable)
		mutex_lock(&userspace_mutex);

	for_each_online_cpu(cpu){
		if(per_cpu(temp_policy,cpu) && per_cpu(cpu_is_managed, cpu)){
			cpufreq_set(per_cpu(temp_policy,cpu), cpu_freq_table[freq_index].frequency);
		}
	}

	if(stress_test_enable)
		mutex_unlock(&userspace_mutex);

	freq_index--;
	if(freq_index<0)
		freq_index=max_cpu_freq_index;
	if(stress_test_enable)
		queue_delayed_work(dvfs_test_work_queue, &stress_test,1*HZ);
	return ;
}

static bool cpufreq_set_userspace_governor(void)
{
	static char *cpufreq_sysfs_governor_path="/sys/devices/system/cpu/cpu%i/cpufreq/scaling_governor";
	static char *cpufreq_sysfs_online_path="/sys/devices/system/cpu/cpu%i/online";
	struct file *scaling_gov = NULL;
	struct file *cpux_online= NULL;
	static char *governor = "userspace";
	static char cpu_on[1] = {'1'};
	mm_segment_t old_fs;
	char    buf[128];
	int i;
	bool err=0;
	loff_t offset = 0;
	struct cpufreq_policy *policy =NULL;

	printk("cpufreq_set_userspace_governor+\n");
	/* change to KERNEL_DS address limit */
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	for(i = 1; i <= 3; i++) {
		sprintf(buf, cpufreq_sysfs_online_path, i);
		cpux_online = filp_open(buf, O_RDWR, 0);

		if (IS_ERR_OR_NULL(cpux_online)){
			printk("cpufreq_set_userspace_governor: open %s fail\n", buf);
			err = true;
			goto error;
		}

		cpux_online->f_op->write(cpux_online, cpu_on, strlen(cpu_on), &offset);

		memset(buf,0,sizeof(buf));
		sprintf(buf, cpufreq_sysfs_governor_path, i);
		scaling_gov = filp_open(buf, O_RDWR, 0);
		if (IS_ERR_OR_NULL(scaling_gov)) {
			printk("cpufreq_set_userspace_governor: open %s fail\n", buf);
			err = true;
			goto error;
		}

		policy =cpufreq_cpu_get(i);
		if(policy) {
			if(!strncmp(policy->governor->name, governor, 9)) {
				printk("cpufreq_set_userspace_governor cpu%u is userspace\n", i);
				if (policy)
					cpufreq_cpu_put(policy);
				continue;
			}
			cpufreq_cpu_put(policy);
		}

		if (scaling_gov->f_op != NULL && scaling_gov->f_op->write != NULL) {
			scaling_gov->f_op->write(scaling_gov,
					governor,
					strlen(governor),
					&offset);
		} else
			pr_err("f_op might be null\n");

		filp_close(scaling_gov, NULL);
		filp_close(cpux_online, NULL);
	}

	printk("cpufreq_set_userspace_governor-\n");
error:
	set_fs(old_fs);
	return err;
}
static int start_test(struct cpufreq_policy *policy, unsigned int enable)
{
	int i=0;
	if(policy->cpu==0 && enable ){
		cpu_freq_table=cpufreq_frequency_get_table(0);

		for (i = 0; (cpu_freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (cpu_freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
		printk("start_test cpu_freq_table[%u].frequency=%u\n",i,cpu_freq_table[i].frequency);
		}

		freq_index=max_cpu_freq_index=i-1;
		stress_test_enable=true;
		queue_delayed_work(dvfs_test_work_queue, &stress_test,1*HZ);
	}else
		stress_test_enable=false;
	return true;
}

static ssize_t show_test(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%u\n", stress_test_enable);
}
//=========dvfs stress test ===================
/**
 * cpufreq_set - set the CPU frequency
 * @policy: pointer to policy struct where freq is being set
 * @freq: target frequency in kHz
 *
 * Sets the CPU frequency to freq.
 */
static int cpufreq_set(struct cpufreq_policy *policy, unsigned int freq)
{
	int ret = -EINVAL;

	printk("cpufreq_set for cpu %u, freq %u kHz\n", policy->cpu, freq);

	if(!stress_test_enable)
	mutex_lock(&userspace_mutex);
	if (!per_cpu(cpu_is_managed, policy->cpu))
		goto err;

	per_cpu(cpu_set_freq, policy->cpu) = freq;

	if (freq < per_cpu(cpu_min_freq, policy->cpu))
		freq = per_cpu(cpu_min_freq, policy->cpu);
	if (freq > per_cpu(cpu_max_freq, policy->cpu))
		freq = per_cpu(cpu_max_freq, policy->cpu);

	/*
	 * We're safe from concurrent calls to ->target() here
	 * as we hold the userspace_mutex lock. If we were calling
	 * cpufreq_driver_target, a deadlock situation might occur:
	 * A: cpufreq_set (lock userspace_mutex) ->
	 *      cpufreq_driver_target(lock policy->lock)
	 * B: cpufreq_set_policy(lock policy->lock) ->
	 *      __cpufreq_governor ->
	 *         cpufreq_governor_userspace (lock userspace_mutex)
	 */
	ret = __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);

 err:
	if(!stress_test_enable)
	mutex_unlock(&userspace_mutex);
	return ret;
}


static ssize_t show_speed(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%u\n", per_cpu(cpu_cur_freq, policy->cpu));
}

static int cpufreq_governor_userspace(struct cpufreq_policy *policy,
				   unsigned int event)
{
	unsigned int cpu = policy->cpu;
	int rc = 0;

	switch (event) {
	case CPUFREQ_GOV_START:
		if (!cpu_online(cpu))
			return -EINVAL;
		per_cpu(temp_policy, cpu) =policy;
		BUG_ON(!policy->cur);
		mutex_lock(&userspace_mutex);

		if (cpus_using_userspace_governor == 0) {
			cpufreq_register_notifier(
					&userspace_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}
		cpus_using_userspace_governor++;

		per_cpu(cpu_is_managed, cpu) = 1;
		per_cpu(cpu_min_freq, cpu) = policy->min;
		per_cpu(cpu_max_freq, cpu) = policy->max;
		per_cpu(cpu_cur_freq, cpu) = policy->cur;
		per_cpu(cpu_set_freq, cpu) = policy->cur;
		pr_debug("managing cpu %u started "
			"(%u - %u kHz, currently %u kHz)\n",
				cpu,
				per_cpu(cpu_min_freq, cpu),
				per_cpu(cpu_max_freq, cpu),
				per_cpu(cpu_cur_freq, cpu));

		mutex_unlock(&userspace_mutex);
		break;
	case CPUFREQ_GOV_STOP:
		mutex_lock(&userspace_mutex);
		cpus_using_userspace_governor--;
		if (cpus_using_userspace_governor == 0) {
			cpufreq_unregister_notifier(
					&userspace_cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}

		per_cpu(cpu_is_managed, cpu) = 0;
		per_cpu(cpu_min_freq, cpu) = 0;
		per_cpu(cpu_max_freq, cpu) = 0;
		per_cpu(cpu_set_freq, cpu) = 0;
                per_cpu(temp_policy, cpu) =NULL;
		pr_debug("managing cpu %u stopped\n", cpu);
		mutex_unlock(&userspace_mutex);
		break;
	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&userspace_mutex);
		pr_debug("limit event for cpu %u: %u - %u kHz, "
			"currently %u kHz, last set to %u kHz\n",
			cpu, policy->min, policy->max,
			per_cpu(cpu_cur_freq, cpu),
			per_cpu(cpu_set_freq, cpu));
		if (policy->max < per_cpu(cpu_set_freq, cpu)) {
			__cpufreq_driver_target(policy, policy->max,
						CPUFREQ_RELATION_H);
		} else if (policy->min > per_cpu(cpu_set_freq, cpu)) {
			__cpufreq_driver_target(policy, policy->min,
						CPUFREQ_RELATION_L);
		} else {
			__cpufreq_driver_target(policy,
						per_cpu(cpu_set_freq, cpu),
						CPUFREQ_RELATION_L);
		}
		per_cpu(cpu_min_freq, cpu) = policy->min;
		per_cpu(cpu_max_freq, cpu) = policy->max;
		per_cpu(cpu_cur_freq, cpu) = policy->cur;
		mutex_unlock(&userspace_mutex);
		break;
	}
	return rc;
}


#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_USERSPACE
static
#endif
struct cpufreq_governor cpufreq_gov_userspace = {
	.name		= "userspace",
	.governor	= cpufreq_governor_userspace,
	.store_setspeed	= cpufreq_set,
	.show_setspeed	= show_speed,
	.start_dvfs_test	 =  start_test,
	.show_dvfs_test	=  show_test,
	.owner		= THIS_MODULE,
};

static int __init cpufreq_gov_userspace_init(void)
{
	INIT_DELAYED_WORK(&stress_test,  dvfs_stress_test) ;
	dvfs_test_work_queue = create_singlethread_workqueue("dvfs_test_workqueue");
	return cpufreq_register_governor(&cpufreq_gov_userspace);
}


static void __exit cpufreq_gov_userspace_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_userspace);
}


MODULE_AUTHOR("Dominik Brodowski <linux@brodo.de>, "
		"Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("CPUfreq policy governor 'userspace'");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_USERSPACE
fs_initcall(cpufreq_gov_userspace_init);
#else
module_init(cpufreq_gov_userspace_init);
#endif
module_exit(cpufreq_gov_userspace_exit);
