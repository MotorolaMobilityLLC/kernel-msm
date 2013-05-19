/* Copyright (c) 2011-2013, Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fs.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/clockchips.h>
#include <linux/vmalloc.h>


#define NRUN_PERIOD_NS  10000000
#define NRUN_MAX_TASK_NUM 6

#define MAX_UINT 0xFFFFFFFF

#define MS_TO_NS(ms) ((ms) * 1000 * 1000)

struct nrun_period {

	const unsigned int period;
	unsigned int count;
	unsigned int nrun_min;
	unsigned int freq_min[NR_CPUS];

} nrun_period_data[] = {
	{ .period =    1, },/*   X1 */
	{ .period =    5, },/*   X5 */
	{ .period =   10, },/*  X10 */
	{ .period =   50, },/*  X50 */
	{ .period =  100, } /* X100 */
};

struct nrun_sample_data {

	unsigned int nrun;
	unsigned int freq[NR_CPUS];
};

#define NRUN_PERIOD_MUL_NUM ARRAY_SIZE(nrun_period_data)

enum {
	NRUN_NORMAL_MODE = 0,
	NRUN_CPU_MODE,
	NRUN_MODE_NUM
};

static int nrun_running_mode = NRUN_NORMAL_MODE;

const char nrun_mode_str[][16] = {
	"Normal",
	"CPU"
};

enum {
	NRUN_STATE_START,
	NRUN_STATE_STOP,
};
static int nrun_state = NRUN_STATE_STOP;

enum {
	NRUN_SHOW_TOTAL,
	NRUN_SHOW_ONLY_ONE_CPU,
	NRUN_SHOW_ALL_CPU,
	NRUN_SHOW_ALL_CPU_HIGH_FREQ
};

unsigned long long nrun_start_time;
unsigned long long nrun_stop_time;
int nrun_data_dirty;
static struct mutex nrun_ctrl_mutex;

/*
 * nrun_data_buf is allocated with the following size:
 *  = NRUN_PERIOD_MUL_NUM *
 *    ( size of cpu freq table + 1) ^ num_possible_cpus() *
 *    (NRUN_MAX_TASK_NUM + 2)
 *
 *  for NRUN_MAX_TASK_NUM + 2:
 *  0 - for the case of  only idle task,
 *  +1- to count the case of =NRUN_MAX_TASK_NUM running
 *  +1- to count case of  > NRUN_MAX_TASK_NUM
 *
 * for (size of cpu freq table + 1)
 *  0 -  for corresponding CPU offline
 *  and assume all the CPU has the same freq table
 *
 *  Don't need to protect the data with lock because only
 *  hrtimer handler will change it, and it run in ISR.
 *
 */
unsigned long *nrun_data_buf;
unsigned long nrun_data_buf_size;
unsigned long nrun_buf_size_per_period;
unsigned long nrun_buf_size_per_cpu[NR_CPUS];


static struct hrtimer nrun_hrtimer;
static unsigned long long total_sample;
static unsigned long long total_nr;
static unsigned long max_nr;
static unsigned long long nrun_base_rate_ns = NRUN_PERIOD_NS;
static int nrun_timer_suspend_state;

static void nrun_reset_data(void);
static int is_nrun_timer_suspend(void);

/* CPU Freq Changes*/

static DEFINE_PER_CPU(unsigned int, nrun_cpu_freq_index);

static int nrun_top_cpu_freq;

static int nrun_get_cpu_freq_table_size(int cpu)
{
	struct cpufreq_frequency_table *table;
	int i;

	table = cpufreq_frequency_get_table(cpu);
	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
	}

	return i;
}
static int nrun_get_index_by_freq(unsigned int freq, int cpu)
{
	struct cpufreq_frequency_table *table;
	int i;

	table = cpufreq_frequency_get_table(cpu);
	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;
		if (table[i].frequency == freq)
			return i;
	}

	WARN(1, "It should not run to here!\n");
	return 0;
}
/* This Function can't be invoked in ISR */
static unsigned int nrun_get_freq_current_index(unsigned int cpu)
{
	unsigned int freq = cpufreq_get(cpu);

	return nrun_get_index_by_freq(freq, cpu);
}

static unsigned int nrun_get_freq_by_index(int cpu, int index)
{
	struct cpufreq_frequency_table *table;

	table = cpufreq_frequency_get_table(cpu);

	/* will table be changed?*/
	return table[index].frequency;
}
/* This Function will be invoked in ISR */
static unsigned int nrun_current_freq(int cpu)
{
	return per_cpu(nrun_cpu_freq_index, cpu);
}

static int nrun_cpufreq_notifier(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_freqs *freq = data;
	unsigned int new_freq;

	mutex_lock(&nrun_ctrl_mutex);
	if (val == CPUFREQ_POSTCHANGE) {
		new_freq = freq->new;
		per_cpu(nrun_cpu_freq_index, freq->cpu) =
			nrun_get_index_by_freq(new_freq, freq->cpu) + 1;
	}
	mutex_unlock(&nrun_ctrl_mutex);
	return 0;
}
static struct notifier_block nrun_cpufreq_notifier_block = {
	.notifier_call = nrun_cpufreq_notifier
};

/* This function should be invoked with mutex lock*/
static void nrun_start_cpu_freq_handle(void)
{
	int i;

	for_each_possible_cpu(i)
		per_cpu(nrun_cpu_freq_index, i) = 0;

	for_each_online_cpu(i)
		per_cpu(nrun_cpu_freq_index, i) =
			nrun_get_freq_current_index(i) + 1;

	cpufreq_register_notifier(&nrun_cpufreq_notifier_block,
		CPUFREQ_TRANSITION_NOTIFIER);

}
static void nrun_stop_cpu_freq_handle(void)
{
	cpufreq_unregister_notifier(&nrun_cpufreq_notifier_block,
		CPUFREQ_TRANSITION_NOTIFIER);
}

static void nrun_cpu_freq_init(void)
{

	/*
	 * Assumption:
	 * Top cpu freq is at the top of the table;
	 * All the CPUs's tables are identical.
	 */
	nrun_top_cpu_freq = nrun_get_cpu_freq_table_size(0) - 1;
}

/* NRUN  Data*/

static inline void nrun_reset_period_data(int period_id)
{
	struct nrun_period *p;
	int i;

	p = &nrun_period_data[period_id];
	p->count = p->period;
	p->nrun_min = MAX_UINT;
	for (i = 0; i < num_possible_cpus(); i++)
		p->freq_min[i] = MAX_UINT;
}

/* nrun_reset_data() should be invoked afer nrun_cancel_timer()*/
static void nrun_reset_data(void)
{
	int i;

	for (i = 0; i < NRUN_PERIOD_MUL_NUM; i++)
		nrun_reset_period_data(i);
}
/* nrun_reset_full_data() should be invoked afer nrun_cancel_timer()*/
static void nrun_reset_full_data(void)
{
	nrun_reset_data();
	memset(nrun_data_buf, 0, nrun_data_buf_size);

	total_sample = 0;
	total_nr = 0;
	max_nr = 0;

	nrun_start_time = 0;
	nrun_stop_time = 0;
	nrun_data_dirty = 0;
}

static inline void nrun_data_update(int period_id, struct nrun_sample_data *s)
{

	unsigned long *p = nrun_data_buf;
	int i;
	int nr_r = s->nrun;

	p += period_id * nrun_buf_size_per_period;

	for (i = 0; i < num_possible_cpus(); i++)
		p += s->freq[i] * nrun_buf_size_per_cpu[i];

	if (likely(nr_r <= NRUN_MAX_TASK_NUM))
		p += nr_r;
	else
		p += (NRUN_MAX_TASK_NUM + 1);

	(*p)++;
}

static inline void nrun_period_update(int period_id, struct nrun_sample_data *s)
{
	struct nrun_period *p = &nrun_period_data[period_id];
	struct nrun_sample_data d;
	int i;

	p->count--;
	p->nrun_min = min(p->nrun_min, s->nrun);
	for (i = 0; i < num_possible_cpus(); i++)
		p->freq_min[i] = min(p->freq_min[i], s->freq[i]);

	if (p->count)
		return;

	/* Period End */
	trace_printk("period %ums: min = %u, freq0 = %d, freq1 = %d\n",
			p->period, p->nrun_min, p->freq_min[0], p->freq_min[1]);

	d.nrun = p->nrun_min;
	for (i = 0; i < num_possible_cpus(); i++)
		d.freq[i] = p->freq_min[i];

	nrun_data_update(period_id, &d);

	/* Reset */
	nrun_reset_period_data(period_id);
}


/* HR Timer */

static enum hrtimer_restart nrun_timer_handler(struct hrtimer *hrtimer)
{

	struct nrun_sample_data s;
	int  j, i;

	if (is_nrun_timer_suspend())
		return HRTIMER_RESTART;

	trace_printk("start to handle..\n");
	s.nrun = nr_running();
	for (i = 0; i < num_possible_cpus(); i++)
		s.freq[i] = 0;

	for_each_online_cpu(i)
		s.freq[i] = nrun_current_freq(i);

	total_sample++;
	total_nr += s.nrun;
	max_nr = max_t(unsigned long, s.nrun, max_nr);

	nrun_data_update(0, &s);/* For each base rate sampling, run ASAP*/

	for (j = 1; j < NRUN_PERIOD_MUL_NUM; j++)
		nrun_period_update(j, &s);


	hrtimer_forward_now(hrtimer, ns_to_ktime(nrun_base_rate_ns));

	trace_printk("nr_r = %u, total_nr = %llu total_sampes = %llu\n",
		s.nrun, total_nr, total_sample);

	return HRTIMER_RESTART;
}

static void nrun_prepare_hrtimer(void)
{
	struct hrtimer *hrtimer = &nrun_hrtimer;

	hrtimer_init(hrtimer, CLOCK_MONOTONIC,  HRTIMER_MODE_REL);
	hrtimer->function = nrun_timer_handler;
}

static void nrun_enable_hrtimer(void)
{
	struct hrtimer *hrtimer = &nrun_hrtimer;

	hrtimer_start(hrtimer, ns_to_ktime(nrun_base_rate_ns),
			HRTIMER_MODE_REL);
}

static void nrun_cancel_hrtimer(void)
{
	struct hrtimer *hrtimer = &nrun_hrtimer;

	hrtimer_cancel(hrtimer);
}

void nrun_timer_suspend(void)
{
	nrun_timer_suspend_state = 1;
	nrun_reset_data();
	trace_printk("handle suspend\n");
}
void nrun_timer_resume(void)
{
	nrun_timer_suspend_state = 0;
}
int is_nrun_timer_suspend(void)
{
	return nrun_timer_suspend_state;
}
static int nrun_clock_event_notify(struct notifier_block *nb,
				unsigned long reason, void *dev)
{
	switch (reason) {

	case CLOCK_EVT_NOTIFY_SUSPEND:
		nrun_timer_suspend();
		break;

	case CLOCK_EVT_NOTIFY_RESUME:
		nrun_timer_resume();
		break;
	default:
		break;

	}
	return NOTIFY_OK;
}

static struct notifier_block nrun_clock_nfb = {
	.notifier_call = nrun_clock_event_notify,
};

/* View: Show the Data*/

static unsigned int div_integer(unsigned long long p, unsigned long long t)
{
	if (!t)
		return 0;

	do_div(p, t);
	return p;
}
static unsigned int div_02decimal(unsigned long long p, unsigned long long t)
{
	unsigned int rem, tmp;

	if (!t)
		return 0;

	rem = do_div(p, t);
	rem *= 100;
	tmp = do_div(rem, t);
	if ((tmp * 2 > t) && (rem < 99))
		rem++;

	return rem;

}

#define DIVISION(p, t) \
	" %3u.%02u ", div_integer(p, t), div_02decimal(p, t)

#define PERCENT(p, t) DIVISION(p*100, t)


void nrun_print_time(struct seq_file *m)
{
	unsigned long long t;
	unsigned long rem;

	mutex_lock(&nrun_ctrl_mutex);

	t = nrun_start_time;
	rem = do_div(t, 1000000000);
	seq_printf(m, "Start@ %5lu.%06lu ", (unsigned long) t, rem / 1000);

	if (nrun_state == NRUN_STATE_STOP) {
		t = nrun_stop_time;
		seq_printf(m, "Stop@ ");
	} else {
		t = cpu_clock(smp_processor_id());
		seq_printf(m, "Now@ ");
	}

	rem = do_div(t, 1000000000);
	seq_printf(m, " %5lu.%06lu ", (unsigned long) t, rem / 1000);

	mutex_unlock(&nrun_ctrl_mutex);
}

static unsigned long long nrun_total_in_period(int period)
{
	unsigned long *p = nrun_data_buf;
	unsigned long long total = 0;
	int i;

	p += period * nrun_buf_size_per_period;

	for (i = 0; i < nrun_buf_size_per_period; i++)
		total += *(p + i);

	return total;
}
/*
 * The counter[NR_CPUS + 1] is defined corresponding to nrun_data_buf[]
 *  counter[0] : for CPU0,
 *  counter[1] : for CPU1,...
 *  counter[NR_CPUS] : for task number
 *
 * When accessing data from nrun_data_buf[] one by one, the counter should
 * be increased one by one from [NR_CPUS] to [0].
 *
 * for example, when there are two CPUs, the counter is [1, 1, 2], then
 * it's reading data from nrun_data_buf[][1][1][2] -- both CPUs in index
 * 1 freq and runnable task # is 2.
 *
 */
static inline void init_counter(int counter[])
{
	int i;

	for (i = 0; i <= num_possible_cpus(); i++)
		counter[i] = 0;
}
static inline void inc_counter(int counter[])
{
	int i;
	int freq_size = nrun_get_cpu_freq_table_size(0);

	counter[num_possible_cpus()]++;

	if (counter[num_possible_cpus()] >  NRUN_MAX_TASK_NUM + 1) {

		counter[num_possible_cpus()] = 0;

		for (i = num_possible_cpus() - 1; i >= 0; i--) {

			counter[i]++;

			if (counter[i] > freq_size)
				counter[i] = 0;
			else
				return;
		}
	}
}
static void nrun_proc_raw_print_item(struct seq_file *m, unsigned long value,
				unsigned long long total, int counter[])
{
	int i, t_n;
	unsigned int freq = 0;

	if (!value)
		return;

	seq_printf(m, "\n(");

	for (i = 0; i < num_possible_cpus(); i++) {

		if (counter[i] == 0)
			seq_printf(m, "      0,");
		else {
			freq = nrun_get_freq_by_index(i, counter[i] - 1);
			seq_printf(m, "%8u,", freq);
		}
	}
	seq_printf(m, ")");

	t_n = counter[num_possible_cpus()];

	if (t_n < NRUN_MAX_TASK_NUM + 1)
		seq_printf(m, " \t\t  %u: ", t_n);
	else
		seq_printf(m, " \t\t >%u: ", t_n - 1);

	seq_printf(m, " %10lu ", value);
	seq_printf(m, PERCENT(value, total));
	seq_printf(m, "%%");

}
static void nrun_proc_raw_show_items(struct seq_file *m, int period,
				unsigned long long total)
{
	unsigned long *p = nrun_data_buf;
	int counter[NR_CPUS + 1];
	int i;

	init_counter(counter);
	p += period * nrun_buf_size_per_period;

	for (i = 0; i < nrun_buf_size_per_period; i++) {
		nrun_proc_raw_print_item(m, *(p+i), total, counter);
		inc_counter(counter);
	}
}

static int nrun_proc_raw_show(struct seq_file *m, void *v)
{
	unsigned long long base;
	unsigned long long t_nr;
	int j;
	unsigned long long total;

	base = nrun_base_rate_ns;
	do_div(base, 1000000);

	for (j = 0; j < NRUN_PERIOD_MUL_NUM; j++) {

		total = nrun_total_in_period(j);

		seq_printf(m, "\n");

		seq_printf(m, "----------------------------------");
		seq_printf(m, "------------------------------------\n");
		seq_printf(m, "\t\t%5llums   ",
				nrun_period_data[j].period * base);

		seq_printf(m, "total samples %llu\n", total);
		seq_printf(m, "(cpu0 freq, cpu1 freq , , ) runnable task#:");
		seq_printf(m, "sample# (samples/total)\n");
		seq_printf(m, "-----------------------------------");
		seq_printf(m, "-----------------------------------\n");
		nrun_proc_raw_show_items(m, j, total);

	}

	t_nr = nrun_base_rate_ns;
	do_div(t_nr, 1000000);
	seq_printf(m, "\ntotal samples (%llums): %llu  ",
					t_nr, total_sample);
	if (total_sample) {
		t_nr = total_nr;
		seq_printf(m, ", average :"
					DIVISION(t_nr, total_sample));

		seq_printf(m, "\t max : %lu\n", max_nr);
	}

	nrun_print_time(m);

	seq_printf(m, "\n\n");

	return 0;
}

static int nrun_proc_raw_open(struct inode *inode, struct file *file)
{
	return single_open(file, nrun_proc_raw_show, NULL);
}

static void nrun_cal_smode(int period, int smode, unsigned long long s[],
				unsigned long long *total)
{
	unsigned long *p = nrun_data_buf;
	int counter[NR_CPUS + 1];
	unsigned long value;
	int t_n, i, j;
	unsigned long long  t = 0;

	init_counter(counter);
	p += period * nrun_buf_size_per_period;

	for (i = 0; i < nrun_buf_size_per_period; i++) {

		value = *(p + i);
		t_n = counter[num_possible_cpus()];

		switch (smode) {

		case NRUN_SHOW_TOTAL:
			s[t_n] += value;
			t += value;
			break;

		case NRUN_SHOW_ONLY_ONE_CPU:

			for (j = 1; j < num_possible_cpus(); j++)
				if (counter[j])
					goto next_data;
			s[t_n] += value;
			t += value;
			break;

		case NRUN_SHOW_ALL_CPU:

			for (j = 0; j < num_possible_cpus(); j++)
				if (counter[j] == 0)
					goto next_data;
			s[t_n] += value;
			t += value;
			break;

		case NRUN_SHOW_ALL_CPU_HIGH_FREQ:

			for (j = 0; j < num_possible_cpus(); j++)
				if (counter[j] != (nrun_top_cpu_freq + 1))
					goto next_data;
			s[t_n] += value;
			t += value;
			break;

		default:
			break;

		}

next_data:
		inc_counter(counter);
	}

	*total = t;
}

static void nrun_proc_show_mode(struct seq_file *m, int smode)
{
	unsigned long long s[NRUN_MAX_TASK_NUM + 1 + 1];
	unsigned long long accu[NRUN_MAX_TASK_NUM + 1 + 1];
	int i, p, a;
	unsigned long long total;
	unsigned long long t_nr, base;

	seq_printf(m, "Number of Runnable Tasks:");

	for (i = 0; i <= NRUN_MAX_TASK_NUM + 1; i++)
		seq_printf(m, "  >=%2d  ", i);

	seq_printf(m, "\n");
	base = nrun_base_rate_ns;
	do_div(base, 1000000);

	for (i = 0; i < NRUN_PERIOD_MUL_NUM; i++) {

		total = 0;
		memset(s, 0,
			sizeof(unsigned long long)*(NRUN_MAX_TASK_NUM + 1 + 1));

		nrun_cal_smode(i, smode, s, &total);

		for (p = 0; p <= NRUN_MAX_TASK_NUM + 1; p++) {
			accu[p] = s[p];
			for (a = p + 1; a <= NRUN_MAX_TASK_NUM + 1; a++)
				accu[p] += s[a];
		}
		seq_printf(m, "For Every of %4llums (%%):",
			(base * nrun_period_data[i].period));

		for (p = 0; p <= NRUN_MAX_TASK_NUM + 1; p++)
			seq_printf(m, PERCENT(accu[p] , total));

		seq_printf(m, " (total %llu)\n", total);
	}

	if (smode == NRUN_SHOW_TOTAL) {
		t_nr = nrun_base_rate_ns;
		do_div(t_nr, 1000000);
		seq_printf(m, "\ntotal samples (%llums): %llu  ",
					t_nr, total_sample);
		if (total_sample) {
			t_nr = total_nr;
			seq_printf(m, ", average :"
					DIVISION(t_nr, total_sample));

			seq_printf(m, "\t max : %lu\n", max_nr);
		}
	}

	nrun_print_time(m);

	seq_printf(m, "\n\n");

}
static int nrun_proc_show_total(struct seq_file *m, void *v)
{
	nrun_proc_show_mode(m, NRUN_SHOW_TOTAL);
	return 0;
}

static int nrun_proc_total_open(struct inode *inode, struct file *file)
{
	return single_open(file, nrun_proc_show_total, NULL);
}

static const struct file_operations nrun_proc_total_fops = {
	.open		= nrun_proc_total_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nrun_proc_show_single_cpu(struct seq_file *m, void *v)
{
	nrun_proc_show_mode(m, NRUN_SHOW_ONLY_ONE_CPU);
	return 0;
}

static int nrun_proc_single_cpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, nrun_proc_show_single_cpu, NULL);
}

static const struct file_operations nrun_proc_cpu0_fops = {
	.open		= nrun_proc_single_cpu_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nrun_proc_show_all_cpus(struct seq_file *m, void *v)
{
	nrun_proc_show_mode(m, NRUN_SHOW_ALL_CPU);
	return 0;
}

static int nrun_proc_all_cpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, nrun_proc_show_all_cpus, NULL);
}

static const struct file_operations nrun_proc_all_cpu_fops = {
	.open		= nrun_proc_all_cpu_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nrun_proc_show_high_freq(struct seq_file *m, void *v)
{
	nrun_proc_show_mode(m, NRUN_SHOW_ALL_CPU_HIGH_FREQ);
	return 0;
}

static int nrun_proc_high_freq_open(struct inode *inode, struct file *file)
{
	return single_open(file, nrun_proc_show_high_freq, NULL);
}

static const struct file_operations nrun_proc_high_freq_fops = {
	.open		= nrun_proc_high_freq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* Ctrl:  Start, Stop, Reset, Mode Switch */


static void nrun_start(void)
{

	mutex_lock(&nrun_ctrl_mutex);

	if (nrun_state == NRUN_STATE_START) {
		mutex_unlock(&nrun_ctrl_mutex);
		return;
	}

	nrun_start_cpu_freq_handle();

	if (nrun_running_mode == NRUN_NORMAL_MODE)
		nrun_enable_hrtimer();

	nrun_state = NRUN_STATE_START;

	if (!nrun_data_dirty) {
		nrun_start_time = cpu_clock(smp_processor_id());
		nrun_data_dirty = 1;
	}

	mutex_unlock(&nrun_ctrl_mutex);

}
static void nrun_stop(void)
{
	mutex_lock(&nrun_ctrl_mutex);

	if (nrun_state == NRUN_STATE_STOP) {
		mutex_unlock(&nrun_ctrl_mutex);
		return;
	}

	nrun_state = NRUN_STATE_STOP;

	nrun_cancel_hrtimer();
	nrun_reset_data();

	nrun_stop_cpu_freq_handle();
	nrun_stop_time = cpu_clock(smp_processor_id());

	mutex_unlock(&nrun_ctrl_mutex);

}
static void nrun_reset(void)
{
	nrun_stop();
	mutex_lock(&nrun_ctrl_mutex);
	nrun_reset_full_data();
	mutex_unlock(&nrun_ctrl_mutex);
}

/* Ctrl:  CPU Mode */

static int __cpuinit
cpu_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (unsigned long) hcpu;

	trace_printk("cpu_callback cpu = %d, smp_processor_id = %d\n",
		cpu, smp_processor_id());

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		if ((nrun_running_mode == NRUN_CPU_MODE) &&
		     (nrun_state == NRUN_STATE_START))
			nrun_enable_hrtimer();
		break;

#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		if (nrun_running_mode == NRUN_CPU_MODE) {
			nrun_cancel_hrtimer();
			nrun_reset_data();
		}
		break;
#endif /* CONFIG_HOTPLUG_CPU */

	}
	return NOTIFY_OK;

}

static struct notifier_block __cpuinitdata cpu_nfb = {
	.notifier_call = cpu_callback,
};
static void nrun_normal(void)
{
	nrun_reset();
	mutex_lock(&nrun_ctrl_mutex);
	nrun_running_mode = NRUN_NORMAL_MODE;
	mutex_unlock(&nrun_ctrl_mutex);
}

static void nrun_cpu(void)
{
	nrun_reset();
	mutex_lock(&nrun_ctrl_mutex);
	nrun_running_mode = NRUN_CPU_MODE;
	mutex_unlock(&nrun_ctrl_mutex);
}

struct nrun_cmd {

	char name[32];
	void (*fun)(void);

} nrun_cmd_items[] = {

	{"start",	nrun_start},
	{"stop",	nrun_stop},
	{"reset",	nrun_reset},
	{"cpu",		nrun_cpu},
	{"normal",	nrun_normal}
};

static int nrun_num_of_cmd_items = ARRAY_SIZE(nrun_cmd_items);

static ssize_t nrun_ctrl_write(struct file *f, const char __user *buf,
			size_t cnt, loff_t *ppos)
{
	char *pdata;
	int i;

	if (cnt > 32)
		return -EINVAL;

	pdata = kzalloc(cnt, GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

	if (copy_from_user(pdata, buf, cnt)) {
		kfree(pdata);
		return -EFAULT;
	}

	for (i = 0; i < nrun_num_of_cmd_items; i++)
		if (!strncmp(pdata, nrun_cmd_items[i].name,
				strlen(nrun_cmd_items[i].name)))
			nrun_cmd_items[i].fun();

	kfree(pdata);
	return (ssize_t)cnt;
}

static const struct file_operations nrun_proc_ctrl_fops = {
	.write		= nrun_ctrl_write,
};
static const struct file_operations nrun_proc_raw_fops = {
	.open		= nrun_proc_raw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nrun_proc_rate_show(struct seq_file *m, void *v)
{
	unsigned long long tmp;

	tmp = nrun_base_rate_ns;
	do_div(tmp, 1000000);
	seq_printf(m, "%llums\n", tmp);
	return 0;

}
static ssize_t nrun_rate_write(struct file *f, const char __user *buf,
			size_t cnt, loff_t *ppos)
{
	char *pdata;
	int ret;
	long value;

	if (cnt > 32)
		return -EINVAL;

	pdata = kzalloc(cnt, GFP_KERNEL);
	if (pdata == NULL)
		return -ENOMEM;

	if (copy_from_user(pdata, buf, cnt)) {
		kfree(pdata);
		return -EFAULT;
	}

	ret = kstrtol(pdata, 10, &value);
	if (ret) {
		kfree(pdata);
		return ret;
	}

	if (value)
		nrun_base_rate_ns = MS_TO_NS(value);

	nrun_reset();

	kfree(pdata);
	return (ssize_t)cnt;

}

static int nrun_proc_rate_open(struct inode *inode, struct file *file)
{
	return single_open(file, nrun_proc_rate_show, NULL);
}

static const struct file_operations nrun_proc_rate_fops = {
	.open		= nrun_proc_rate_open,
	.read		= seq_read,
	.write		= nrun_rate_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/* Initialization */

static int nrun_data_init(void)
{
	int cpu_num = num_possible_cpus();
	int freq_table_size = (nrun_get_cpu_freq_table_size(0) + 1);
	int i;

	nrun_data_buf_size = NRUN_PERIOD_MUL_NUM * (NRUN_MAX_TASK_NUM + 2) \
			* sizeof(unsigned long);

	for (i = 0; i < cpu_num; i++)
		nrun_data_buf_size *= freq_table_size;

	nrun_data_buf = vmalloc(nrun_data_buf_size);
	if (!nrun_data_buf)
		return -ENOMEM;

	nrun_buf_size_per_cpu[cpu_num - 1] = (NRUN_MAX_TASK_NUM + 2);
	for (i = cpu_num - 2; i >= 0; i--)
		nrun_buf_size_per_cpu[i] = nrun_buf_size_per_cpu[i + 1] *\
							freq_table_size;

	nrun_buf_size_per_period = nrun_buf_size_per_cpu[0] *\
						freq_table_size;

	nrun_reset_full_data();

	return 0;
}

static int __init proc_nrun_init(void)
{
	int ret = 0;

	struct proc_dir_entry *nrun_root;

	mutex_init(&nrun_ctrl_mutex);

	ret = nrun_data_init();
	if (ret != 0)
		return ret;

	nrun_cpu_freq_init();

	nrun_prepare_hrtimer();

	clockevents_register_notifier(&nrun_clock_nfb);

	register_cpu_notifier(&cpu_nfb);

	nrun_root = proc_mkdir("nrun", NULL);

	proc_create("raw", 0440, nrun_root, &nrun_proc_raw_fops);
	proc_create("total", 0440, nrun_root, &nrun_proc_total_fops);
	proc_create("cpu0", 0440, nrun_root, &nrun_proc_cpu0_fops);
	proc_create("all_cpu", 0440, nrun_root, &nrun_proc_all_cpu_fops);
	proc_create("high_freq", 0440, nrun_root, &nrun_proc_high_freq_fops);
	proc_create("cmd", 0220, nrun_root, &nrun_proc_ctrl_fops);
	proc_create("base_rate", 0660, nrun_root, &nrun_proc_rate_fops);


	return ret;
}
module_init(proc_nrun_init);
