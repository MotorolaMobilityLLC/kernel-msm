/*
 * sfi_cpufreq.c - sfi Processor P-States Driver
 *
 *
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Vishwesh M Rudramuni
 * Contact information: Vishwesh Rudramuni <vishwesh.m.rudramuni@intel.com>
 */

/*
 * This sfi Processor P-States Driver re-uses most part of the code available
 * in acpi cpufreq driver.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/compiler.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sfi.h>
#include <linux/io.h>

#include <asm/msr.h>
#include <asm/processor.h>
#include <asm/cpufeature.h>

#include "sfi-cpufreq.h"
#include "mperf.h"

MODULE_AUTHOR("Vishwesh Rudramuni");
MODULE_DESCRIPTION("SFI Processor P-States Driver");
MODULE_LICENSE("GPL");

DEFINE_PER_CPU(struct sfi_processor *, sfi_processors);

static DEFINE_MUTEX(performance_mutex);
static int sfi_cpufreq_num;
static u32 sfi_cpu_num;
static bool battlow;

#define SFI_FREQ_MAX		32
#define INTEL_MSR_RANGE		0xffff
#define INTEL_MSR_BUSRATIO_MASK	0xff00
#define SFI_CPU_MAX		8

#define X86_ATOM_ARCH_SLM	0x4a

struct sfi_cpufreq_data {
	struct sfi_processor_performance *sfi_data;
	struct cpufreq_frequency_table *freq_table;
	unsigned int max_freq;
	unsigned int resume;
};


struct drv_cmd {
	const struct cpumask *mask;
	u32 msr;
	u32 val;
};

static DEFINE_PER_CPU(struct sfi_cpufreq_data *, drv_data);
struct sfi_freq_table_entry sfi_cpufreq_array[SFI_FREQ_MAX];
static struct sfi_cpu_table_entry sfi_cpu_array[SFI_CPU_MAX];

/* sfi_perf_data is a pointer to percpu data. */
static struct sfi_processor_performance *sfi_perf_data;

static struct cpufreq_driver sfi_cpufreq_driver;

static int parse_freq(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_freq_table_entry *pentry;
	int totallen;

	sb = (struct sfi_table_simple *)table;
	if (!sb) {
		printk(KERN_WARNING "SFI: Unable to map FREQ\n");
		return -ENODEV;
	}

	if (!sfi_cpufreq_num) {
		sfi_cpufreq_num = SFI_GET_NUM_ENTRIES(sb,
			 struct sfi_freq_table_entry);
		pentry = (struct sfi_freq_table_entry *)sb->pentry;
		totallen = sfi_cpufreq_num * sizeof(*pentry);
		memcpy(sfi_cpufreq_array, pentry, totallen);
	}

	return 0;
}

static void get_cpu_sibling_mask(int cpu, struct cpumask *sibling_mask)
{
	unsigned int base = (cpu/CONFIG_NR_CPUS_PER_MODULE) * CONFIG_NR_CPUS_PER_MODULE;
	unsigned int i;

	cpumask_clear(sibling_mask);
	for (i = base; i < (base + CONFIG_NR_CPUS_PER_MODULE); i++)
		cpumask_set_cpu(i, sibling_mask);
}

static int sfi_processor_get_performance_states(struct sfi_processor *pr)
{
	int result = 0;
	int i;

	pr->performance->state_count = sfi_cpufreq_num;
	pr->performance->states =
	    kmalloc(sizeof(struct sfi_processor_px) * sfi_cpufreq_num,
		    GFP_KERNEL);
	if (!pr->performance->states)
		result = -ENOMEM;

	printk(KERN_INFO "Num p-states %d\n", sfi_cpufreq_num);

	/* Populate the P-states info from the SFI table here */
	for (i = 0; i < sfi_cpufreq_num; i++) {
		pr->performance->states[i].core_frequency =
			sfi_cpufreq_array[i].freq_mhz;
		pr->performance->states[i].transition_latency =
			sfi_cpufreq_array[i].latency;
		pr->performance->states[i].control =
			sfi_cpufreq_array[i].ctrl_val;
		printk(KERN_INFO "State [%d]: core_frequency[%d] transition_latency[%d] control[0x%x]\n",
			i,
			(u32) pr->performance->states[i].core_frequency,
			(u32) pr->performance->states[i].transition_latency,
			(u32) pr->performance->states[i].control);
	}

	return result;
}

static int sfi_processor_register_performance(struct sfi_processor_performance
				    *performance, unsigned int cpu)
{
	struct sfi_processor *pr;

	mutex_lock(&performance_mutex);

	pr = per_cpu(sfi_processors, cpu);
	if (!pr) {
		mutex_unlock(&performance_mutex);
		return -ENODEV;
	}

	if (pr->performance) {
		mutex_unlock(&performance_mutex);
		return -EBUSY;
	}

	WARN_ON(!performance);

	pr->performance = performance;

	/* parse the freq table from sfi */
	sfi_cpufreq_num = 0;
	sfi_table_parse(SFI_SIG_FREQ, NULL, NULL, parse_freq);

	sfi_processor_get_performance_states(pr);

	mutex_unlock(&performance_mutex);
	return 0;
}

void sfi_processor_unregister_performance(struct sfi_processor_performance
				      *performance, unsigned int cpu)
{
	struct sfi_processor *pr;


	mutex_lock(&performance_mutex);

	pr = per_cpu(sfi_processors, cpu);
	if (!pr) {
		mutex_unlock(&performance_mutex);
		return;
	}

	if (pr->performance)
		kfree(pr->performance->states);
	pr->performance = NULL;

	mutex_unlock(&performance_mutex);

	return;
}

static unsigned extract_freq(u32 msr, struct sfi_cpufreq_data *data)
{
	int i;
	struct sfi_processor_performance *perf;
	u32 sfi_ctrl;

	msr &= INTEL_MSR_BUSRATIO_MASK;
	perf = data->sfi_data;

	for (i = 0; data->freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		sfi_ctrl = perf->states[data->freq_table[i].index].control
			& INTEL_MSR_BUSRATIO_MASK;
		if (sfi_ctrl == msr)
			return data->freq_table[i].frequency;
	}
	return data->freq_table[0].frequency;
}

/* Called via smp_call_function_many(), on the target CPUs */
static void do_drv_write(void *_cmd)
{
	struct drv_cmd *cmd = _cmd;
	u32 lo, hi;

	rdmsr(cmd->msr, lo, hi);
	lo = (lo & ~INTEL_MSR_RANGE) | (cmd->val & INTEL_MSR_RANGE);
	wrmsr(cmd->msr, lo, hi);
}

static void drv_write(struct drv_cmd *cmd)
{
	int this_cpu;

	this_cpu = get_cpu();
	if (cpumask_test_cpu(this_cpu, cmd->mask))
		do_drv_write(cmd);
	smp_call_function_many(cmd->mask, do_drv_write, cmd, 1);
	put_cpu();
}

static u32 get_cur_val(const struct cpumask *mask)
{
	u32 val, dummy;

	if (unlikely(cpumask_empty(mask)))
		return 0;

	rdmsr_on_cpu(cpumask_any(mask), MSR_IA32_PERF_STATUS, &val, &dummy);

	return val;
}

static unsigned int get_cur_freq_on_cpu(unsigned int cpu)
{
	struct sfi_cpufreq_data *data = per_cpu(drv_data, cpu);
	unsigned int freq;
	unsigned int cached_freq;

	pr_debug("get_cur_freq_on_cpu (%d)\n", cpu);

	if (unlikely(data == NULL ||
		data->sfi_data == NULL || data->freq_table == NULL)) {
		return 0;
	}

	cached_freq = data->freq_table[data->sfi_data->state].frequency;
	freq = extract_freq(get_cur_val(cpumask_of(cpu)), data);
	if (freq != cached_freq) {
		/*
		 * The dreaded BIOS frequency change behind our back.
		 * Force set the frequency on next target call.
		 */
		data->resume = 1;
	}

	pr_debug("cur freq = %u\n", freq);

	return freq;
}

static int sfi_cpufreq_target(struct cpufreq_policy *policy,
			       unsigned int target_freq, unsigned int relation)
{
	struct sfi_cpufreq_data *data = per_cpu(drv_data, policy->cpu);
	struct sfi_processor_performance *perf;
	struct cpufreq_freqs freqs;
	unsigned int next_state = 0; /* Index into freq_table */
	unsigned int next_perf_state = 0; /* Index into perf table */
	int result = 0;
	struct drv_cmd cmd;


	pr_debug("sfi_cpufreq_target %d (%d)\n", target_freq, policy->cpu);

	if (unlikely(data == NULL ||
	     data->sfi_data == NULL || data->freq_table == NULL)) {
		return -ENODEV;
	}

	perf = data->sfi_data;
	result = cpufreq_frequency_table_target(policy,
						data->freq_table,
						target_freq,
						relation, &next_state);
	if (unlikely(result))
		return -ENODEV;

	next_perf_state = data->freq_table[next_state].index;
	if (perf->state == next_perf_state) {
		if (unlikely(data->resume)) {
			pr_debug("Called after resume, resetting to P%d\n",
				next_perf_state);
			data->resume = 0;
		} else {
			pr_debug("Already at target state (P%d)\n",
				next_perf_state);
			return 0;
		}
	}

	cmd.msr = MSR_IA32_PERF_CTL;
	cmd.val = (u32) perf->states[next_perf_state].control;
	cmd.mask = policy->cpus;

	freqs.old = perf->states[perf->state].core_frequency * 1000;
	freqs.new = data->freq_table[next_state].frequency;

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	drv_write(&cmd);

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
	perf->state = next_perf_state;

	return result;
}

static int sfi_cpufreq_verify(struct cpufreq_policy *policy)
{
	struct sfi_cpufreq_data *data = per_cpu(drv_data, policy->cpu);

	pr_debug("sfi_cpufreq_verify\n");

	return cpufreq_frequency_table_verify(policy, data->freq_table);
}

/*
 * sfi_cpufreq_early_init - initialize SFI P-States library
 *
 * Initialize the SFI P-States library (drivers/sfi/processor_perflib.c)
 * in order to cope with the correct frequency and voltage pairings.
 */
static int __init sfi_cpufreq_early_init(void)
{
	sfi_perf_data = alloc_percpu(struct sfi_processor_performance);
	if (!sfi_perf_data) {
		pr_debug("Memory allocation error for sfi_perf_data.\n");
		return -ENOMEM;
	}

	return 0;
}


static int sfi_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	unsigned int i;
	unsigned int freq;
	unsigned int cpufreqidx = 0;
	unsigned int valid_states = 0;
	unsigned int cpu = policy->cpu;
	struct sfi_cpufreq_data *data;
	unsigned int result = 0;
	struct cpuinfo_x86 *c = &cpu_data(policy->cpu);
	struct sfi_processor_performance *perf;
	struct cpumask sibling_mask;

	pr_debug("sfi_cpufreq_cpu_init CPU:%d\n", policy->cpu);

	data = kzalloc(sizeof(struct sfi_cpufreq_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->sfi_data = per_cpu_ptr(sfi_perf_data, cpu);
	per_cpu(drv_data, cpu) = data;

	sfi_cpufreq_driver.flags |= CPUFREQ_CONST_LOOPS;


	result = sfi_processor_register_performance(data->sfi_data, cpu);
	if (result)
		goto err_free;

	perf = data->sfi_data;
	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;

	get_cpu_sibling_mask(cpu, &sibling_mask);
	cpumask_copy(policy->cpus, &sibling_mask);
	cpumask_set_cpu(policy->cpu, policy->related_cpus);

	/* capability check */
	if (perf->state_count <= 1) {
		pr_debug("No P-States\n");
		result = -ENODEV;
		goto err_unreg;
	}

	data->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table) *
		    (perf->state_count+1), GFP_KERNEL);
	if (!data->freq_table) {
		result = -ENOMEM;
		goto err_unreg;
	}

	/* detect transition latency */
	policy->cpuinfo.transition_latency = 0;
	for (i = 0; i < perf->state_count; i++) {
		if ((perf->states[i].transition_latency * 1000) >
		    policy->cpuinfo.transition_latency)
			policy->cpuinfo.transition_latency =
			    perf->states[i].transition_latency * 1000;
	}

	data->max_freq = perf->states[0].core_frequency * 1000;
	/* table init */
	for (i = 0; i < perf->state_count; i++) {
		if (i > 0 && perf->states[i].core_frequency >=
		    data->freq_table[valid_states-1].frequency / 1000)
			continue;

		data->freq_table[valid_states].index = i;
		data->freq_table[valid_states].frequency =
		    perf->states[i].core_frequency * 1000;
		valid_states++;
	}
	cpufreqidx = valid_states - 1;
	data->freq_table[valid_states].frequency = CPUFREQ_TABLE_END;
	perf->state = 0;

	result = cpufreq_frequency_table_cpuinfo(policy, data->freq_table);
	if (result)
		goto err_freqfree;

	policy->cur = get_cur_freq_on_cpu(cpu);


	/* Check for APERF/MPERF support in hardware */
	if (cpu_has(c, X86_FEATURE_APERFMPERF))
		sfi_cpufreq_driver.getavg = cpufreq_get_measured_perf;

	pr_debug("CPU%u - SFI performance management activated.\n", cpu);
	for (i = 0; i < perf->state_count; i++)
		pr_debug("     %cP%d: %d MHz, %d uS\n",
			(i == perf->state ? '*' : ' '), i,
			(u32) perf->states[i].core_frequency,
			(u32) perf->states[i].transition_latency);

	cpufreq_frequency_table_get_attr(data->freq_table, policy->cpu);

	/*
	 * the first call to ->target() should result in us actually
	 * writing something to the appropriate registers.
	 */
	data->resume = 1;

	/**
	 * Capping the cpu frequency to LFM during boot, if battery is detected
	 * as critically low.
	 */
	if (battlow) {
		freq = data->freq_table[cpufreqidx].frequency;
		if (freq != CPUFREQ_ENTRY_INVALID) {
			pr_info("CPU%u freq is capping to %uKHz\n", cpu, freq);
			policy->max = freq;
		} else {
			pr_err("CPU%u table entry %u is invalid.\n",
					cpu, cpufreqidx);
			goto err_freqfree;
		}
	}

	return result;

err_freqfree:
	kfree(data->freq_table);
err_unreg:
	sfi_processor_unregister_performance(perf, cpu);
err_free:
	kfree(data);
	per_cpu(drv_data, cpu) = NULL;

	return result;
}

static int sfi_cpufreq_cpu_exit(struct cpufreq_policy *policy)
{
	struct sfi_cpufreq_data *data = per_cpu(drv_data, policy->cpu);

	pr_debug("sfi_cpufreq_cpu_exit\n");

	if (data) {
		cpufreq_frequency_table_put_attr(policy->cpu);
		per_cpu(drv_data, policy->cpu) = NULL;
		sfi_processor_unregister_performance(data->sfi_data,
							policy->cpu);
		kfree(data->freq_table);
		kfree(data);
	}

	return 0;
}

static int sfi_cpufreq_resume(struct cpufreq_policy *policy)
{
	struct sfi_cpufreq_data *data = per_cpu(drv_data, policy->cpu);

	pr_debug("sfi_cpufreq_resume\n");

	data->resume = 1;

	return 0;
}

static struct freq_attr *sfi_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver sfi_cpufreq_driver = {
	.get = get_cur_freq_on_cpu,
	.verify = sfi_cpufreq_verify,
	.target = sfi_cpufreq_target,
	.init = sfi_cpufreq_cpu_init,
	.exit = sfi_cpufreq_cpu_exit,
	.resume = sfi_cpufreq_resume,
	.name = "sfi-cpufreq",
	.owner = THIS_MODULE,
	.attr = sfi_cpufreq_attr,
};

/**
 * set_battlow_status - enables "battlow" to cap the max scaling cpu frequency.
 */
static int __init set_battlow_status(char *unused)
{
	pr_notice("Low Battery detected! Frequency shall be capped.\n");
	battlow = true;
	return 0;
}
/* Checking "battlow" param on boot, whether battery is critically low or not */
early_param("battlow", set_battlow_status);

static int __init parse_cpus(struct sfi_table_header *table)
{
	struct sfi_table_simple *sb;
	struct sfi_cpu_table_entry *pentry;
	int i;

	sb = (struct sfi_table_simple *)table;

	sfi_cpu_num = SFI_GET_NUM_ENTRIES(sb, struct sfi_cpu_table_entry);

	pentry = (struct sfi_cpu_table_entry *) sb->pentry;
	for (i = 0; i < sfi_cpu_num; i++) {
		sfi_cpu_array[i].apic_id = pentry->apic_id;
		printk(KERN_INFO "APIC ID: %d\n", pentry->apic_id);
		pentry++;
	}

	return 0;

}


static int __init init_sfi_processor_list(void)
{
	struct sfi_processor *pr;
	int i;
	int result;

	/* parse the cpus from the sfi table */
	result = sfi_table_parse(SFI_SIG_CPUS, NULL, NULL, parse_cpus);

	if (result < 0)
		return result;

	pr = kzalloc(sfi_cpu_num * sizeof(struct sfi_processor), GFP_KERNEL);
	if (!pr)
		return -ENOMEM;

	for (i = 0; i < sfi_cpu_num; i++) {
		pr->id = sfi_cpu_array[i].apic_id;
		per_cpu(sfi_processors, i) = pr;
		pr++;
	}

	return 0;
}

static int __init sfi_cpufreq_init(void)
{
	int ret;

	pr_debug("sfi_cpufreq_init\n");

	ret = init_sfi_processor_list();
	if (ret)
		return ret;

	ret = sfi_cpufreq_early_init();
	if (ret)
		return ret;

	return cpufreq_register_driver(&sfi_cpufreq_driver);
}

static void __exit sfi_cpufreq_exit(void)
{

	struct sfi_processor *pr;

	pr_debug("sfi_cpufreq_exit\n");

	pr = per_cpu(sfi_processors, 0);
	kfree(pr);

	cpufreq_unregister_driver(&sfi_cpufreq_driver);

	free_percpu(sfi_perf_data);

	return;
}
late_initcall(sfi_cpufreq_init);
module_exit(sfi_cpufreq_exit);


unsigned int turbo_enable  __read_mostly = 1; /* default enable */
int set_turbo_feature(const char *val, struct kernel_param *kp)
{
	int i, nc;
	u32 lo, hi;
	int rv = param_set_int(val, kp);

	if (rv)
		return rv;

	/* enable/disable Turbo */
	nc = num_possible_cpus();
	if (boot_cpu_data.x86_model == X86_ATOM_ARCH_SLM) {
		for (i = 0; i < nc; i++) {
			rdmsr_on_cpu(i, MSR_IA32_MISC_ENABLE, &lo, &hi);
			if (turbo_enable)
				hi = hi &
				 (~(MSR_IA32_MISC_ENABLE_TURBO_DISABLE >> 32));
			else
				hi = hi |
				(MSR_IA32_MISC_ENABLE_TURBO_DISABLE >> 32);
			wrmsr_on_cpu(i, MSR_IA32_MISC_ENABLE, lo, hi);
		}
	}
	return 0;
}
MODULE_PARM_DESC(turbo_enable, "to enable/disable turbo feature(1:Enable; 0:Disable)");
module_param_call(turbo_enable, set_turbo_feature, param_get_uint,
		  &turbo_enable, S_IRUGO | S_IWUSR);

MODULE_ALIAS("sfi");
