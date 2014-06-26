/*
 * intel_soc_pm_debug.c - This driver provides debug utilities across
 * multiple platforms
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/time.h>
#include <asm/intel_mid_rpmsg.h>
#include <linux/cpuidle.h>
#include "intel_soc_pm_debug.h"
#include <asm-generic/io-64-nonatomic-hi-lo.h>
#include <asm/tsc.h>

#ifdef CONFIG_PM_DEBUG
#define MAX_CSTATES_POSSIBLE	32

static struct latency_stat *lat_stat;

static void latency_measure_enable_disable(bool enable_measure)
{
	int err;
	u32 sub;

	if (enable_measure == lat_stat->latency_measure)
		return;

	if (enable_measure)
		sub = IPC_SUB_MEASURE_START_CLVP;
	else
		sub = IPC_SUB_MEASURE_STOP_CLVP;

	err = rpmsg_send_generic_command(IPC_CMD_S0IX_LATENCY_CLVP,
						sub, NULL, 0, NULL, 0);
	if (unlikely(err)) {
		pr_err("IPC to %s S0IX Latency Measurement failed!\n",
					enable_measure ? "start" : "stop");
		return;
	}

	if (enable_measure) {
		memset(lat_stat->scu_latency, 0, sizeof(lat_stat->scu_latency));
		memset(lat_stat->os_latency, 0, sizeof(lat_stat->os_latency));
		memset(lat_stat->s3_parts_lat, 0,
				sizeof(lat_stat->s3_parts_lat));
		memset(lat_stat->count, 0, sizeof(lat_stat->count));
	}

	lat_stat->latency_measure = enable_measure;
}

static void print_simple_stat(struct seq_file *s, int divisor, int rem_div,
					int count, struct simple_stat stat)
{
	unsigned long long min, avg, max;
	unsigned long min_rem = 0, avg_rem = 0, max_rem = 0;

	min = stat.min;
	max = stat.max;
	avg = stat.total;

	if (count)
		do_div(avg, count);

	if (divisor > 1) {
		min_rem = do_div(min, divisor);
		max_rem = do_div(max, divisor);
		avg_rem = do_div(avg, divisor);
	}

	if (rem_div > 1) {
		min_rem /= rem_div;
		max_rem /= rem_div;
		avg_rem /= rem_div;
	}

	seq_printf(s, " %5llu.%03lu/%5llu.%03lu/%5llu.%03lu",
			min, min_rem, avg, avg_rem, max, max_rem);
}

static int show_pmu_s0ix_lat(struct seq_file *s, void *unused)
{
	int i = 0;

	char *states[] = {
		"S0I1",
		"LPMP3",
		"S0I3",
		"S3"
	};

	char *s3_parts_names[] = {
		"PROC_FRZ",
		"DEV_SUS",
		"NB_CPU_OFF",
		"NB_CPU_ON",
		"DEV_RES",
		"PROC_UNFRZ"
	};

	seq_printf(s, "%29s %35s\n", "SCU Latency", "OS Latency");
	seq_printf(s, "%33s %35s\n", "min/avg/max(msec)", "min/avg/max(msec)");

	for (i = 0; i < ARRAY_SIZE(states); i++) {
		seq_printf(s, "\n%s(%llu)", states[i], lat_stat->count[i]);

		seq_printf(s, "\n%5s", "entry");
		print_simple_stat(s, USEC_PER_MSEC, 1, lat_stat->count[i],
						lat_stat->scu_latency[i].entry);
		seq_printf(s, "      ");
		print_simple_stat(s, NSEC_PER_MSEC, NSEC_PER_USEC,
			lat_stat->count[i], lat_stat->os_latency[i].entry);

		seq_printf(s, "\n%5s", "exit");
		print_simple_stat(s, USEC_PER_MSEC, 1, lat_stat->count[i],
						lat_stat->scu_latency[i].exit);
		seq_printf(s, "      ");
		print_simple_stat(s, NSEC_PER_MSEC, NSEC_PER_USEC,
			lat_stat->count[i], lat_stat->os_latency[i].exit);

	}

	seq_printf(s, "\n\n");

	if (!lat_stat->count[SYS_STATE_S3])
		return 0;

	seq_printf(s, "S3 Latency dissection:\n");
	seq_printf(s, "%38s\n", "min/avg/max(msec)");

	for (i = 0; i < MAX_S3_PARTS; i++) {
		seq_printf(s, "%10s\t", s3_parts_names[i]);
		print_simple_stat(s, NSEC_PER_MSEC, NSEC_PER_USEC,
					lat_stat->count[SYS_STATE_S3],
					lat_stat->s3_parts_lat[i]);
		seq_printf(s, "\n");
	}

	return 0;
}

static int pmu_s0ix_lat_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_pmu_s0ix_lat, NULL);
}

static ssize_t pmu_s0ix_lat_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, userbuf, buf_size))
		return -EFAULT;


	buf[buf_size] = 0;

	if (((strlen("start") + 1) == buf_size) &&
		!strncmp(buf, "start", strlen("start"))) {
		latency_measure_enable_disable(true);
	} else if (((strlen("stop") + 1) == buf_size) &&
		!strncmp(buf, "stop", strlen("stop"))) {
		latency_measure_enable_disable(false);
	}

	return buf_size;
}

static const struct file_operations s0ix_latency_ops = {
	.open		= pmu_s0ix_lat_open,
	.read		= seq_read,
	.write		= pmu_s0ix_lat_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void update_simple_stat(struct simple_stat *simple_stat, int count)
{
	u64 duration = simple_stat->curr;

	if (!count) {
		simple_stat->min =
		simple_stat->max =
		simple_stat->total = duration;
	} else {
		if (duration < simple_stat->min)
			simple_stat->min = duration;
		else if (duration > simple_stat->max)
			simple_stat->max = duration;
		simple_stat->total += duration;
	}
}

void s0ix_scu_latency_stat(int type)
{
	if (!lat_stat || !lat_stat->latency_measure)
		return;

	if (type < SYS_STATE_S0I1 || type > SYS_STATE_S3)
		return;

	lat_stat->scu_latency[type].entry.curr =
			readl(lat_stat->scu_s0ix_lat_addr);
	lat_stat->scu_latency[type].exit.curr =
			readl(lat_stat->scu_s0ix_lat_addr + 1);

	update_simple_stat(&lat_stat->scu_latency[type].entry,
					lat_stat->count[type]);
	update_simple_stat(&lat_stat->scu_latency[type].exit,
					lat_stat->count[type]);
}

void s0ix_lat_stat_init(void)
{
	if (!platform_is(INTEL_ATOM_CLV))
		return;

	lat_stat = devm_kzalloc(&mid_pmu_cxt->pmu_dev->dev,
			sizeof(struct latency_stat), GFP_KERNEL);
	if (unlikely(!lat_stat)) {
		pr_err("Failed to allocate memory for s0ix latency!\n");
		goto out_err;
	}

	lat_stat->scu_s0ix_lat_addr =
		devm_ioremap_nocache(&mid_pmu_cxt->pmu_dev->dev,
			S0IX_LAT_SRAM_ADDR_CLVP, S0IX_LAT_SRAM_SIZE_CLVP);
	if (unlikely(!lat_stat->scu_s0ix_lat_addr)) {
		pr_err("Failed to map SCU_S0IX_LAT_ADDR!\n");
		goto out_err;
	}

	lat_stat->dentry = debugfs_create_file("s0ix_latency",
			S_IFREG | S_IRUGO, NULL, NULL, &s0ix_latency_ops);
	if (unlikely(!lat_stat->dentry)) {
		pr_err("Failed to create debugfs for s0ix latency!\n");
		goto out_err;
	}

	return;

out_err:
	pr_err("%s: Initialization failed\n", __func__);
}

void s0ix_lat_stat_finish(void)
{
	if (!platform_is(INTEL_ATOM_CLV))
		return;

	if (unlikely(!lat_stat))
		return;

	if (likely(lat_stat->dentry))
		debugfs_remove(lat_stat->dentry);
}

void time_stamp_in_suspend_flow(int mark, bool start)
{
	if (!lat_stat || !lat_stat->latency_measure)
		return;

	if (start) {
		lat_stat->s3_parts_lat[mark].curr = cpu_clock(0);
		return;
	}

	lat_stat->s3_parts_lat[mark].curr = cpu_clock(0) -
				lat_stat->s3_parts_lat[mark].curr;
}

static void collect_sleep_state_latency_stat(int sleep_state)
{
	int i;
	if (sleep_state == SYS_STATE_S3)
		for (i = 0; i < MAX_S3_PARTS; i++)
			update_simple_stat(&lat_stat->s3_parts_lat[i],
						lat_stat->count[sleep_state]);

	update_simple_stat(&lat_stat->os_latency[sleep_state].entry,
						lat_stat->count[sleep_state]);
	update_simple_stat(&lat_stat->os_latency[sleep_state].exit,
						lat_stat->count[sleep_state]);
	lat_stat->count[sleep_state]++;
}

void time_stamp_for_sleep_state_latency(int sleep_state, bool start, bool entry)
{
	if (!lat_stat || !lat_stat->latency_measure)
		return;

	if (start) {
		if (entry)
			lat_stat->os_latency[sleep_state].entry.curr =
								cpu_clock(0);
		else
			lat_stat->os_latency[sleep_state].exit.curr =
								cpu_clock(0);
		return;
	}

	if (entry)
		lat_stat->os_latency[sleep_state].entry.curr = cpu_clock(0) -
				lat_stat->os_latency[sleep_state].entry.curr;
	else {
		lat_stat->os_latency[sleep_state].exit.curr = cpu_clock(0) -
				lat_stat->os_latency[sleep_state].exit.curr;
		collect_sleep_state_latency_stat(sleep_state);
	}
}
#else /* CONFIG_PM_DEBUG */
void s0ix_scu_latency_stat(int type) {}
void s0ix_lat_stat_init(void) {}
void s0ix_lat_stat_finish(void) {}
void time_stamp_for_sleep_state_latency(int sleep_state, bool start,
							bool entry) {}
void time_stamp_in_suspend_flow(int mark, bool start) {}
inline unsigned int pmu_get_new_cstate
		(unsigned int cstate, int *index) { return cstate; };
#endif /* CONFIG_PM_DEBUG */

static char *dstates[] = {"D0", "D0i1", "D0i2", "D0i3"};

/* This can be used to report NC power transitions */
void (*nc_report_power_state) (u32, int);

#if defined(CONFIG_REMOVEME_INTEL_ATOM_MDFLD_POWER)			\
			|| defined(CONFIG_REMOVEME_INTEL_ATOM_CLV_POWER)

#define PMU_DEBUG_PRINT_STATS	(1U << 0)
static int debug_mask;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define DEBUG_PRINT(logging_type, s, debug_level_mask, args...)		\
	do {								\
		if (logging_type)					\
			seq_printf(s, args);				\
		else if (debug_mask &					\
			PMU_DEBUG_PRINT_##debug_level_mask)		\
			pr_info(args);					\
	} while (0)

static struct island display_islands[] = {
	{APM_REG_TYPE, APM_GRAPHICS_ISLAND, "GFX"},
	{APM_REG_TYPE, APM_VIDEO_DEC_ISLAND, "Video Decoder"},
	{APM_REG_TYPE, APM_VIDEO_ENC_ISLAND, "Video Encoder"},
	{APM_REG_TYPE, APM_GL3_CACHE_ISLAND, "GL3 Cache"},
	{OSPM_REG_TYPE, OSPM_DISPLAY_A_ISLAND, "Display A"},
	{OSPM_REG_TYPE, OSPM_DISPLAY_B_ISLAND, "Display B"},
	{OSPM_REG_TYPE, OSPM_DISPLAY_C_ISLAND, "Display C"},
	{OSPM_REG_TYPE, OSPM_MIPI_ISLAND, "MIPI-DSI"}
};

static struct island camera_islands[] = {
	{APM_REG_TYPE, APM_ISP_ISLAND, "ISP"},
	{APM_REG_TYPE, APM_IPH_ISLAND, "Iunit PHY"}
};

static char *lss_device_status[4] = { "D0i0", "D0i1", "D0i2", "D0i3" };

static int lsses_num =
			sizeof(lsses)/sizeof(lsses[0]);

#ifdef LOG_PMU_EVENTS
static void pmu_log_timestamp(struct timespec *ts)
{
	if (timekeeping_suspended) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
	} else {
		ktime_get_ts(ts);
	}
}

void pmu_log_pmu_irq(int status)
{
	struct mid_pmu_pmu_irq_log *log =
		&mid_pmu_cxt->pmu_irq_log[mid_pmu_cxt->pmu_irq_log_idx];

	log->status = status;
	pmu_log_timestamp(&log->ts);
	mid_pmu_cxt->pmu_irq_log_idx =
		(mid_pmu_cxt->pmu_irq_log_idx + 1) % LOG_SIZE;
}

static void pmu_dump_pmu_irq_log(void)
{
	struct mid_pmu_pmu_irq_log *log;
	int i = mid_pmu_cxt->pmu_irq_log_idx, j;

	printk(KERN_ERR"%d last pmu irqs:\n", LOG_SIZE);

	for (j = 0; j  < LOG_SIZE; j++) {
		i ? i-- : (i = LOG_SIZE - 1);
		log = &mid_pmu_cxt->pmu_irq_log[i];
		printk(KERN_ERR"Timestamp: %lu.%09lu\n",
			log->ts.tv_sec, log->ts.tv_nsec);
		printk(KERN_ERR"Status = 0x%02x", log->status);
		printk(KERN_ERR"\n");
	}
}

void pmu_log_ipc_irq(void)
{
	struct mid_pmu_ipc_irq_log *log =
		&mid_pmu_cxt->ipc_irq_log[mid_pmu_cxt->ipc_irq_log_idx];

	pmu_log_timestamp(&log->ts);
	mid_pmu_cxt->ipc_irq_log_idx =
	(mid_pmu_cxt->ipc_irq_log_idx + 1) % LOG_SIZE;
}

static void pmu_dump_ipc_irq_log(void)
{
	struct mid_pmu_ipc_irq_log *log;
	int i = mid_pmu_cxt->ipc_irq_log_idx, j;

	printk(KERN_ERR"%d last ipc irqs:\n", LOG_SIZE);

	for (j = 0; j  < LOG_SIZE; j++) {
		i ? i-- : (i = LOG_SIZE - 1);
		log = &mid_pmu_cxt->ipc_irq_log[i];
		printk(KERN_ERR"Timestamp: %lu.%09lu\n",
			log->ts.tv_sec, log->ts.tv_nsec);
		printk(KERN_ERR"\n");
	}
}

void pmu_log_ipc(u32 command)
{
	struct mid_pmu_ipc_log *log =
	&mid_pmu_cxt->ipc_log[mid_pmu_cxt->ipc_log_idx];

	log->command = command;
	pmu_log_timestamp(&log->ts);
	mid_pmu_cxt->ipc_log_idx = (mid_pmu_cxt->ipc_log_idx + 1) % LOG_SIZE;
}

static void pmu_dump_ipc_log(void)
{
	struct mid_pmu_ipc_log *log;
	int i = mid_pmu_cxt->ipc_log_idx, j;

	printk(KERN_ERR"%d last ipc commands:\n", LOG_SIZE);

	for (j = 0; j  < LOG_SIZE; j++) {
		i  ? i-- : (i = LOG_SIZE - 1);
		log = &mid_pmu_cxt->ipc_log[i];
		printk(KERN_ERR"Timestamp: %lu.%09lu\n",
			log->ts.tv_sec, log->ts.tv_nsec);
		printk(KERN_ERR"Command: 0x%08x", log->command);
		printk(KERN_ERR"\n");
	}
}

void pmu_log_command(u32 command, struct pmu_ss_states *pm_ssc)
{
	struct mid_pmu_cmd_log *log =
		&mid_pmu_cxt->cmd_log[mid_pmu_cxt->cmd_log_idx];

	if (pm_ssc != NULL)
		memcpy(&log->pm_ssc, pm_ssc, sizeof(struct pmu_ss_states));
	else
		memset(&log->pm_ssc, 0, sizeof(struct pmu_ss_states));
	log->command = command;
	pmu_log_timestamp(&log->ts);
	mid_pmu_cxt->cmd_log_idx = (mid_pmu_cxt->cmd_log_idx + 1) % LOG_SIZE;
}

static void pmu_dump_command_log(void)
{
	struct mid_pmu_cmd_log *log;
	int i = mid_pmu_cxt->cmd_log_idx, j, k;
	u32 cmd_state;
	printk(KERN_ERR"%d last pmu commands:\n", LOG_SIZE);

	for (j = 0; j  < LOG_SIZE; j++) {
		i ? i-- : (i = LOG_SIZE - 1);
		log = &mid_pmu_cxt->cmd_log[i];
		cmd_state = log->command;
		printk(KERN_ERR"Timestamp: %lu.%09lu\n",
			log->ts.tv_sec, log->ts.tv_nsec);
		switch (cmd_state) {
		case INTERACTIVE_VALUE:
			printk(KERN_ERR"PM_CMD = Interactive_CMD IOC bit not set.\n");
			break;
		case INTERACTIVE_IOC_VALUE:
			printk(KERN_ERR"PM_CMD = Interactive_CMD IOC bit set.\n");
			break;
		case S0I1_VALUE:
			printk(KERN_ERR"PM_CMD = S0i1_CMD\n");
			break;
		case S0I3_VALUE:
			printk(KERN_ERR"PM_CMD = S0i3_CMD\n");
			break;
		case LPMP3_VALUE:
			printk(KERN_ERR"PM_CMD = LPMP3_CMD\n");
			break;
		default:
			printk(KERN_ERR "Invalid PM_CMD\n");
			break;
		}
		for (k = 0; k < 4; k++)
			printk(KERN_ERR"pmu2_states[%d]: 0x%08lx\n",
				k, log->pm_ssc.pmu2_states[k]);
			printk(KERN_ERR"\n");
	}
}

void pmu_dump_logs(void)
{
	struct timespec ts;

	pmu_log_timestamp(&ts);
	printk(KERN_ERR"Dumping out pmu logs\n");
	printk(KERN_ERR"Timestamp: %lu.%09lu\n\n", ts.tv_sec, ts.tv_nsec);
	printk(KERN_ERR"---------------------------------------\n\n");
	pmu_dump_command_log();
	printk(KERN_ERR"---------------------------------------\n\n");
	pmu_dump_pmu_irq_log();
	printk(KERN_ERR"---------------------------------------\n\n");
	pmu_dump_ipc_log();
	printk(KERN_ERR"---------------------------------------\n\n");
	pmu_dump_ipc_irq_log();
}
#else
void pmu_log_pmu_irq(int status) {}
void pmu_log_command(u32 command, struct pmu_ss_states *pm_ssc) {}
void pmu_dump_logs(void) {}
#endif /* LOG_PMU_EVENTS */

void pmu_stat_start(enum sys_state type)
{
	mid_pmu_cxt->pmu_current_state = type;
	mid_pmu_cxt->pmu_stats[type].last_try = cpu_clock(smp_processor_id());
}

void pmu_stat_end(void)
{
	enum sys_state type = mid_pmu_cxt->pmu_current_state;

	if (type > SYS_STATE_S0I0 && type < SYS_STATE_MAX) {
		mid_pmu_cxt->pmu_stats[type].last_entry =
			mid_pmu_cxt->pmu_stats[type].last_try;

		if (!mid_pmu_cxt->pmu_stats[type].count)
			mid_pmu_cxt->pmu_stats[type].first_entry =
				mid_pmu_cxt->pmu_stats[type].last_entry;

		mid_pmu_cxt->pmu_stats[type].time +=
			cpu_clock(smp_processor_id())
			- mid_pmu_cxt->pmu_stats[type].last_entry;

		mid_pmu_cxt->pmu_stats[type].count++;

		s0ix_scu_latency_stat(type);
		if (type >= SYS_STATE_S0I1 && type <= SYS_STATE_S0I3)
			/* time stamp for end of s0ix exit */
			time_stamp_for_sleep_state_latency(type, false, false);
	}

	mid_pmu_cxt->pmu_current_state = SYS_STATE_S0I0;
}

void pmu_stat_error(u8 err_type)
{
	enum sys_state type = mid_pmu_cxt->pmu_current_state;
	u8 err_index;

	if (type > SYS_STATE_S0I0 && type < SYS_STATE_MAX) {
		switch (err_type) {
		case SUBSYS_POW_ERR_INT:
			trace_printk("S0ix_POW_ERR_INT\n");
			err_index = 0;
			break;
		case S0ix_MISS_INT:
			trace_printk("S0ix_MISS_INT\n");
			err_index = 1;
			break;
		case NO_ACKC6_INT:
			trace_printk("S0ix_NO_ACKC6_INT\n");
			err_index = 2;
			break;
		default:
			err_index = 3;
			break;
		}

		if (err_index < 3)
			mid_pmu_cxt->pmu_stats[type].err_count[err_index]++;
	}
}

static void pmu_stat_seq_printf(struct seq_file *s, int type, char *typestr)
{
	unsigned long long t;
	unsigned long nanosec_rem, remainder;
	unsigned long time, init_2_now_time;

	seq_printf(s, "%s\t%5llu\t%10llu\t%9llu\t%9llu\t", typestr,
		 mid_pmu_cxt->pmu_stats[type].count,
		 mid_pmu_cxt->pmu_stats[type].err_count[0],
		 mid_pmu_cxt->pmu_stats[type].err_count[1],
		 mid_pmu_cxt->pmu_stats[type].err_count[2]);

	t = mid_pmu_cxt->pmu_stats[type].time;
	nanosec_rem = do_div(t, NANO_SEC);

	/* convert time in secs */
	time = (unsigned long)t;

	seq_printf(s, "%5lu.%06lu\t",
	   (unsigned long) t, nanosec_rem / 1000);

	t = mid_pmu_cxt->pmu_stats[type].last_entry;
	nanosec_rem = do_div(t, NANO_SEC);
	seq_printf(s, "%5lu.%06lu\t",
	   (unsigned long) t, nanosec_rem / 1000);

	t = mid_pmu_cxt->pmu_stats[type].first_entry;
	nanosec_rem = do_div(t, NANO_SEC);
	seq_printf(s, "%5lu.%06lu\t",
	   (unsigned long) t, nanosec_rem / 1000);

	t =  cpu_clock(0);
	t -= mid_pmu_cxt->pmu_init_time;
	nanosec_rem = do_div(t, NANO_SEC);

	init_2_now_time =  (unsigned long) t;

	/* for calculating percentage residency */
	t = (u64) time;
	t *= 100;

	/* take care of divide by zero */
	if (init_2_now_time) {
		remainder = do_div(t, init_2_now_time);
		time = (unsigned long) t;

		/* for getting 3 digit precision after
		 * decimal dot */
		t = (u64) remainder;
		t *= 1000;
		remainder = do_div(t, init_2_now_time);
	} else
		time = t = 0;

	seq_printf(s, "%5lu.%03lu\n", time, (unsigned long) t);
}

static unsigned long pmu_dev_res_print(int index, unsigned long *precision,
				unsigned long *sampled_time, bool dev_state)
{
	unsigned long long t, delta_time = 0;
	unsigned long nanosec_rem, remainder;
	unsigned long time, init_to_now_time;

	t =  cpu_clock(0);

	if (dev_state) {
		/* print for d0ix */
		if ((mid_pmu_cxt->pmu_dev_res[index].state != PCI_D0))
			delta_time = t -
				mid_pmu_cxt->pmu_dev_res[index].d0i3_entry;

			delta_time += mid_pmu_cxt->pmu_dev_res[index].d0i3_acc;
	} else {
		/* print for d0i0 */
		if ((mid_pmu_cxt->pmu_dev_res[index].state == PCI_D0))
			delta_time = t -
				mid_pmu_cxt->pmu_dev_res[index].d0i0_entry;

		delta_time += mid_pmu_cxt->pmu_dev_res[index].d0i0_acc;
	}

	t -= mid_pmu_cxt->pmu_dev_res[index].start;
	nanosec_rem = do_div(t, NANO_SEC);

	init_to_now_time =  (unsigned long) t;

	t = delta_time;
	nanosec_rem = do_div(t, NANO_SEC);

	/* convert time in secs */
	time = (unsigned long)t;
	*sampled_time = time;

	/* for calculating percentage residency */
	t = (u64) time;
	t *= 100;

	/* take care of divide by zero */
	if (init_to_now_time) {
		remainder = do_div(t, init_to_now_time);
		time = (unsigned long) t;

		/* for getting 3 digit precision after
		* decimal dot */
		t = (u64) remainder;
		t *= 1000;
		remainder = do_div(t, init_to_now_time);
	} else
		time = t = 0;

	*precision = (unsigned long)t;

	return time;
}

static void nc_device_state_show(struct seq_file *s, struct pci_dev *pdev)
{
	int off, i, islands_num, state;
	struct island *islands;

	if (PCI_SLOT(pdev->devfn) == DEV_GFX &&
			PCI_FUNC(pdev->devfn) == FUNC_GFX) {
		off = mid_pmu_cxt->display_off;
		islands_num = ISLANDS_GFX;
		islands = &display_islands[0];
	} else if (PCI_SLOT(pdev->devfn) == DEV_ISP &&
			PCI_FUNC(pdev->devfn) == FUNC_ISP) {
		off = mid_pmu_cxt->camera_off;
		islands_num = ISLANDS_ISP;
		islands = &camera_islands[0];
	} else {
		return;
	}

	seq_printf(s, "pci %04x %04X %s %20s: %41s %s\n",
		pdev->vendor, pdev->device, dev_name(&pdev->dev),
		dev_driver_string(&pdev->dev),
		"", off ? "" : "blocking s0ix");
	for (i = 0; i < islands_num; i++) {
		state = pmu_nc_get_power_state(islands[i].index,
				islands[i].type);
		seq_printf(s, "%52s %15s %17s %s\n",
				 "|------->", islands[i].name, "",
				(state >= 0) ? dstates[state & 3] : "ERR");
	}
}

static int pmu_devices_state_show(struct seq_file *s, void *unused)
{
	struct pci_dev *pdev = NULL;
	int index, i, pmu_num, ss_idx, ss_pos;
	unsigned int base_class;
	u32 target_mask, mask, val, needed;
	struct pmu_ss_states cur_pmsss;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	_pmu2_wait_not_busy();
	pmu_read_sss(&cur_pmsss);
	up(&mid_pmu_cxt->scu_ready_sem);

	seq_printf(s, "TARGET_CFG: ");
	seq_printf(s, "SSS0:%08X ", S0IX_TARGET_SSS0_MASK);
	seq_printf(s, "SSS1:%08X ", S0IX_TARGET_SSS1_MASK);
	seq_printf(s, "SSS2:%08X ", S0IX_TARGET_SSS2_MASK);
	seq_printf(s, "SSS3:%08X ", S0IX_TARGET_SSS3_MASK);

	seq_printf(s, "\n");
	seq_printf(s, "CONDITION FOR S0I3: ");
	seq_printf(s, "SSS0:%08X ", S0IX_TARGET_SSS0);
	seq_printf(s, "SSS1:%08X ", S0IX_TARGET_SSS1);
	seq_printf(s, "SSS2:%08X ", S0IX_TARGET_SSS2);
	seq_printf(s, "SSS3:%08X ", S0IX_TARGET_SSS3);

	seq_printf(s, "\n");
	seq_printf(s, "SSS: ");

	for (i = 0; i < 4; i++)
		seq_printf(s, "%08lX ", cur_pmsss.pmu2_states[i]);

	if (!mid_pmu_cxt->display_off)
		seq_printf(s, "display not suspended: blocking s0ix\n");
	else if (!mid_pmu_cxt->camera_off)
		seq_printf(s, "camera not suspended: blocking s0ix\n");
	else if (mid_pmu_cxt->s0ix_possible & MID_S0IX_STATE)
		seq_printf(s, "can enter s0i1 or s0i3\n");
	else if (mid_pmu_cxt->s0ix_possible & MID_LPMP3_STATE)
		seq_printf(s, "can enter lpmp3\n");
	else
		seq_printf(s, "blocking s0ix\n");

	seq_printf(s, "cmd_error_int count: %d\n", mid_pmu_cxt->cmd_error_int);

	seq_printf(s,
	"\tcount\tsybsys_pow\ts0ix_miss\tno_ack_c6\ttime (secs)\tlast_entry");
	seq_printf(s, "\tfirst_entry\tresidency(%%)\n");

	pmu_stat_seq_printf(s, SYS_STATE_S0I1, "s0i1");
	pmu_stat_seq_printf(s, SYS_STATE_S0I2, "lpmp3");
	pmu_stat_seq_printf(s, SYS_STATE_S0I3, "s0i3");
	pmu_stat_seq_printf(s, SYS_STATE_S3, "s3");

	for_each_pci_dev(pdev) {
		/* find the base class info */
		base_class = pdev->class >> 16;

		if (base_class == PCI_BASE_CLASS_BRIDGE)
			continue;

		if (pmu_pci_to_indexes(pdev, &index, &pmu_num, &ss_idx,
								  &ss_pos))
			continue;

		if (pmu_num == PMU_NUM_1) {
			nc_device_state_show(s, pdev);
			continue;
		}

		mask	= (D0I3_MASK << (ss_pos * BITS_PER_LSS));
		val	= (cur_pmsss.pmu2_states[ss_idx] & mask) >>
						(ss_pos * BITS_PER_LSS);
		switch (ss_idx) {
		case 0:
			target_mask = S0IX_TARGET_SSS0_MASK;
			break;
		case 1:
			target_mask = S0IX_TARGET_SSS1_MASK;
			break;
		case 2:
			target_mask = S0IX_TARGET_SSS2_MASK;
			break;
		case 3:
			target_mask = S0IX_TARGET_SSS3_MASK;
			break;
		default:
			target_mask = 0;
			break;
		}
		needed	= ((target_mask &  mask) != 0);

		seq_printf(s, "pci %04x %04X %s %20s: lss:%02d reg:%d"
			"mask:%08X wk:%02d:%02d:%02d:%03d %s  %s\n",
			pdev->vendor, pdev->device, dev_name(&pdev->dev),
			dev_driver_string(&pdev->dev),
			index - mid_pmu_cxt->pmu1_max_devs, ss_idx, mask,
			mid_pmu_cxt->num_wakes[index][SYS_STATE_S0I1],
			mid_pmu_cxt->num_wakes[index][SYS_STATE_S0I2],
			mid_pmu_cxt->num_wakes[index][SYS_STATE_S0I3],
			mid_pmu_cxt->num_wakes[index][SYS_STATE_S3],
			dstates[val & 3],
			(needed && !val) ? "blocking s0ix" : "");

	}

	return 0;
}

static int devices_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmu_devices_state_show, NULL);
}

static ssize_t devices_state_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, userbuf, buf_size))
		return -EFAULT;


	buf[buf_size] = 0;

	if (((strlen("clear")+1) == buf_size) &&
		!strncmp(buf, "clear", strlen("clear"))) {
		down(&mid_pmu_cxt->scu_ready_sem);
		memset(mid_pmu_cxt->pmu_stats, 0,
					sizeof(mid_pmu_cxt->pmu_stats));
		memset(mid_pmu_cxt->num_wakes, 0,
					sizeof(mid_pmu_cxt->num_wakes));
		mid_pmu_cxt->pmu_current_state = SYS_STATE_S0I0;
		mid_pmu_cxt->pmu_init_time =
			cpu_clock(0);
		clear_d0ix_stats();
		up(&mid_pmu_cxt->scu_ready_sem);
	}

	return buf_size;
}

static const struct file_operations devices_state_operations = {
	.open		= devices_state_open,
	.read		= seq_read,
	.write		= devices_state_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_pmu_lss_status(struct seq_file *s, void *unused)
{
	int sss_reg_index;
	int offset;
	int lss;
	unsigned long status;
	unsigned long sub_status;
	unsigned long lss_status[4];
	struct lss_definition *entry;

	down(&mid_pmu_cxt->scu_ready_sem);

	lss_status[0] = readl(&mid_pmu_cxt->pmu_reg->pm_sss[0]);
	lss_status[1] = readl(&mid_pmu_cxt->pmu_reg->pm_sss[1]);
	lss_status[2] = readl(&mid_pmu_cxt->pmu_reg->pm_sss[2]);
	lss_status[3] = readl(&mid_pmu_cxt->pmu_reg->pm_sss[3]);

	up(&mid_pmu_cxt->scu_ready_sem);

	lss = 0;
	seq_printf(s, "%5s\t%12s %35s %5s %4s %4s %4s %4s\n",
			"lss", "block", "subsystem", "state", "D0i0", "D0i1",
			"D0i2", "D0i3");
	seq_printf(s, "====================================================="
		      "=====================\n");
	for (sss_reg_index = 0; sss_reg_index < 4; sss_reg_index++) {
		status = lss_status[sss_reg_index];
		for (offset = 0; offset < sizeof(unsigned long) * 8 / 2;
								offset++) {
			sub_status = status & 3;
			if (lss >= lsses_num)
				entry = &lsses[lsses_num - 1];
			else
				entry = &lsses[lss];
			seq_printf(s, "%5s\t%12s %35s %4s %4d %4d %4d %4d\n",
					entry->lss_name, entry->block,
					entry->subsystem,
					lss_device_status[sub_status],
					get_d0ix_stat(lss, SS_STATE_D0I0),
					get_d0ix_stat(lss, SS_STATE_D0I1),
					get_d0ix_stat(lss, SS_STATE_D0I2),
					get_d0ix_stat(lss, SS_STATE_D0I3));

			status >>= 2;
			lss++;
		}
	}

	return 0;
}

static int pmu_sss_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_pmu_lss_status, NULL);
}

static const struct file_operations pmu_sss_state_operations = {
	.open		= pmu_sss_state_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_pmu_dev_stats(struct seq_file *s, void *unused)
{
	struct pci_dev *pdev = NULL;
	unsigned long sampled_time, precision;
	int index, pmu_num, ss_idx, ss_pos;
	unsigned int base_class;

	seq_printf(s, "%5s\t%20s\t%10s\t%10s\t%s\n",
		"lss", "Name", "D0_res", "D0ix_res", "Sampled_Time");
	seq_printf(s,
	"==================================================================\n");

	for_each_pci_dev(pdev) {
		/* find the base class info */
		base_class = pdev->class >> 16;

		if (base_class == PCI_BASE_CLASS_BRIDGE)
			continue;

		if (pmu_pci_to_indexes(pdev, &index, &pmu_num, &ss_idx,
							&ss_pos))
			continue;

		if (pmu_num == PMU_NUM_1) {
			seq_printf(s,
			"%5s%20s\t%5lu.%03lu%%\t%5lu.%03lu%%\t%lu\n",
			"NC", dev_driver_string(&pdev->dev),
			pmu_dev_res_print(index, &precision,
				 &sampled_time, false),
			precision,
			pmu_dev_res_print(index, &precision,
				 &sampled_time, true),
			precision, sampled_time);
			continue;
		}

		/* Print for South Complex devices */
		seq_printf(s, "%5d\t%20s\t%5lu.%03lu%%\t%5lu.%03lu%%\t%lu\n",
		index - mid_pmu_cxt->pmu1_max_devs,
		dev_driver_string(&pdev->dev),
		pmu_dev_res_print(index, &precision, &sampled_time, false),
		precision,
		pmu_dev_res_print(index, &precision, &sampled_time, true),
		precision, sampled_time);
	}
	return 0;
}

static int pmu_dev_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_pmu_dev_stats, NULL);
}

static const struct file_operations pmu_dev_stat_operations = {
	.open		= pmu_dev_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_PM_DEBUG
static int pmu_stats_interval = PMU_LOG_INTERVAL_SECS;
module_param_named(pmu_stats_interval, pmu_stats_interval,
				int, S_IRUGO | S_IWUSR | S_IWGRP);

void pmu_s0ix_demotion_stat(int req_state, int grant_state)
{
	struct pmu_ss_states cur_pmsss;
	int i, req_sys_state, offset;
	unsigned long status, sub_status;
	unsigned long s0ix_target_sss_mask[4] = {
				S0IX_TARGET_SSS0_MASK,
				S0IX_TARGET_SSS1_MASK,
				S0IX_TARGET_SSS2_MASK,
				S0IX_TARGET_SSS3_MASK};

	unsigned long s0ix_target_sss[4] = {
				S0IX_TARGET_SSS0,
				S0IX_TARGET_SSS1,
				S0IX_TARGET_SSS2,
				S0IX_TARGET_SSS3};

	unsigned long lpmp3_target_sss_mask[4] = {
				LPMP3_TARGET_SSS0_MASK,
				LPMP3_TARGET_SSS1_MASK,
				LPMP3_TARGET_SSS2_MASK,
				LPMP3_TARGET_SSS3_MASK};

	unsigned long lpmp3_target_sss[4] = {
				LPMP3_TARGET_SSS0,
				LPMP3_TARGET_SSS1,
				LPMP3_TARGET_SSS2,
				LPMP3_TARGET_SSS3};

	req_sys_state = mid_state_to_sys_state(req_state);
	if ((grant_state >= C4_STATE_IDX) && (grant_state <= S0I3_STATE_IDX))
		mid_pmu_cxt->pmu_stats
			[req_sys_state].demote_count
				[grant_state-C4_STATE_IDX]++;

	if (down_trylock(&mid_pmu_cxt->scu_ready_sem))
		return;

	pmu_read_sss(&cur_pmsss);
	up(&mid_pmu_cxt->scu_ready_sem);

	if (!mid_pmu_cxt->camera_off)
		mid_pmu_cxt->pmu_stats[req_sys_state].camera_blocker_count++;

	if (!mid_pmu_cxt->display_off)
		mid_pmu_cxt->pmu_stats[req_sys_state].display_blocker_count++;

	if (!mid_pmu_cxt->s0ix_possible) {
		for (i = 0; i < 4; i++) {
			unsigned int lss_per_register;
			if (req_state == MID_LPMP3_STATE)
				status = lpmp3_target_sss[i] ^
					(cur_pmsss.pmu2_states[i] &
						lpmp3_target_sss_mask[i]);
			else
				status = s0ix_target_sss[i] ^
					(cur_pmsss.pmu2_states[i] &
						s0ix_target_sss_mask[i]);
			if (!status)
				continue;

			lss_per_register =
				(sizeof(unsigned long)*8)/BITS_PER_LSS;

			for (offset = 0; offset < lss_per_register; offset++) {
				sub_status = status & SS_IDX_MASK;
				if (sub_status) {
					mid_pmu_cxt->pmu_stats[req_sys_state].
						blocker_count
						[offset + lss_per_register*i]++;
				}

				status >>= BITS_PER_LSS;
			}
		}
	}
}
EXPORT_SYMBOL(pmu_s0ix_demotion_stat);

static void pmu_log_s0ix_status(int type, char *typestr,
		struct seq_file *s, bool logging_type)
{
	unsigned long long t;
	unsigned long time, remainder, init_2_now_time;

	t = mid_pmu_cxt->pmu_stats[type].time;
	remainder = do_div(t, NANO_SEC);

	/* convert time in secs */
	time = (unsigned long)t;

	t =  cpu_clock(0);
	t -= mid_pmu_cxt->pmu_init_time;
	remainder = do_div(t, NANO_SEC);

	init_2_now_time =  (unsigned long) t;

	/* for calculating percentage residency */
	t = (u64) time;
	t *= 100;

	/* take care of divide by zero */
	if (init_2_now_time) {
		remainder = do_div(t, init_2_now_time);
		time = (unsigned long) t;

		/* for getting 3 digit precision after
		 * decimal dot */
		t = (u64) remainder;
		t *= 1000;
		remainder = do_div(t, init_2_now_time);
	} else
		time = t = 0;
	DEBUG_PRINT(logging_type, s, STATS,
			"%s\t%5llu\t%9llu\t%9llu\t%5lu.%03lu\n"
			, typestr, mid_pmu_cxt->pmu_stats[type].count,
			mid_pmu_cxt->pmu_stats[type].err_count[1],
			mid_pmu_cxt->pmu_stats[type].err_count[2],
			time, (unsigned long) t);
}

static void pmu_log_s0ix_demotion(int type, char *typestr,
		struct seq_file *s, bool logging_type)
{
	DEBUG_PRINT(logging_type, s, STATS, "%s:\t%6d\t%6d\t%6d\t%6d\t%6d\n",
		typestr,
		mid_pmu_cxt->pmu_stats[type].demote_count[0],
		mid_pmu_cxt->pmu_stats[type].demote_count[1],
		mid_pmu_cxt->pmu_stats[type].demote_count[2],
		mid_pmu_cxt->pmu_stats[type].demote_count[3],
		mid_pmu_cxt->pmu_stats[type].demote_count[4]);
}

static void pmu_log_s0ix_lss_blocked(int type, char *typestr,
		struct seq_file *s, bool logging_type)
{
	int i, block_count;

	DEBUG_PRINT(logging_type, s, STATS, "%s: Block Count\n", typestr);

	block_count = mid_pmu_cxt->pmu_stats[type].display_blocker_count;

	if (block_count)
		DEBUG_PRINT(logging_type, s, STATS,
			 "\tDisplay blocked: %d times\n", block_count);

	block_count = mid_pmu_cxt->pmu_stats[type].camera_blocker_count;

	if (block_count)
		DEBUG_PRINT(logging_type, s, STATS,
			"\tCamera blocked: %d times\n", block_count);

	DEBUG_PRINT(logging_type, s, STATS, "\tLSS\t #blocked\n");

	for  (i = 0; i < MAX_LSS_POSSIBLE; i++) {
		block_count = mid_pmu_cxt->pmu_stats[type].blocker_count[i];
		if (block_count)
			DEBUG_PRINT(logging_type, s, STATS, "\t%02d\t %6d\n", i,
						block_count);
	}
	DEBUG_PRINT(logging_type, s, STATS, "\n");
}

static void pmu_stats_logger(bool logging_type, struct seq_file *s)
{

	if (!logging_type)
		DEBUG_PRINT(logging_type, s, STATS,
			"\n----MID_PMU_STATS_LOG_BEGIN----\n");

	DEBUG_PRINT(logging_type, s, STATS,
			"\tcount\ts0ix_miss\tno_ack_c6\tresidency(%%)\n");
	pmu_log_s0ix_status(SYS_STATE_S0I1, "s0i1", s, logging_type);
	pmu_log_s0ix_status(SYS_STATE_S0I2, "lpmp3", s, logging_type);
	pmu_log_s0ix_status(SYS_STATE_S0I3, "s0i3", s, logging_type);
	pmu_log_s0ix_status(SYS_STATE_S3, "s3", s, logging_type);

	DEBUG_PRINT(logging_type, s, STATS, "\nFrom:\tTo\n");
	DEBUG_PRINT(logging_type, s, STATS,
		"\t    C4\t   C6\t  S0i1\t  Lpmp3\t  S0i3\n");

	/* storing C6 demotion info in S0I0 */
	pmu_log_s0ix_demotion(SYS_STATE_S0I0, "  C6", s, logging_type);

	pmu_log_s0ix_demotion(SYS_STATE_S0I1, "s0i1", s, logging_type);
	pmu_log_s0ix_demotion(SYS_STATE_S0I2, "lpmp3", s, logging_type);
	pmu_log_s0ix_demotion(SYS_STATE_S0I3, "s0i3", s, logging_type);

	DEBUG_PRINT(logging_type, s, STATS, "\n");
	pmu_log_s0ix_lss_blocked(SYS_STATE_S0I1, "s0i1", s, logging_type);
	pmu_log_s0ix_lss_blocked(SYS_STATE_S0I2, "lpmp3", s, logging_type);
	pmu_log_s0ix_lss_blocked(SYS_STATE_S0I3, "s0i3", s, logging_type);

	if (!logging_type)
		DEBUG_PRINT(logging_type, s, STATS,
				"\n----MID_PMU_STATS_LOG_END----\n");
}

static void pmu_log_stat(struct work_struct *work)
{

	pmu_stats_logger(false, NULL);

	schedule_delayed_work(&mid_pmu_cxt->log_work,
			msecs_to_jiffies(pmu_stats_interval*1000));
}

static int show_pmu_stats_log(struct seq_file *s, void *unused)
{
	pmu_stats_logger(true, s);
	return 0;
}

static int pmu_stats_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_pmu_stats_log, NULL);
}

static const struct file_operations pmu_stats_log_operations = {
	.open		= pmu_stats_log_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#else
void pmu_s0ix_demotion_stat(int req_state, int grant_state) {}
EXPORT_SYMBOL(pmu_s0ix_demotion_stat);
#endif

void pmu_stats_init(void)
{
	struct dentry *fentry;

	/* /sys/kernel/debug/mid_pmu_states */
	(void) debugfs_create_file("mid_pmu_states", S_IFREG | S_IRUGO,
				NULL, NULL, &devices_state_operations);

	/* /sys/kernel/debug/pmu_sss_states */
	(void) debugfs_create_file("pmu_sss_states", S_IFREG | S_IRUGO,
				NULL, NULL, &pmu_sss_state_operations);

	/* /sys/kernel/debug/pmu_dev_stats */
	(void) debugfs_create_file("pmu_dev_stats", S_IFREG | S_IRUGO,
				NULL, NULL, &pmu_dev_stat_operations);

	s0ix_lat_stat_init();

#ifdef CONFIG_PM_DEBUG
	/* dynamic debug tracing in every 5 mins */
	INIT_DEFERRABLE_WORK(&mid_pmu_cxt->log_work, pmu_log_stat);
	schedule_delayed_work(&mid_pmu_cxt->log_work,
				msecs_to_jiffies(pmu_stats_interval*1000));

	debug_mask = PMU_DEBUG_PRINT_STATS;

	/* /sys/kernel/debug/pmu_stats_log */
	fentry = debugfs_create_file("pmu_stats_log", S_IFREG | S_IRUGO,
				NULL, NULL, &pmu_stats_log_operations);
	if (fentry == NULL)
		printk(KERN_ERR "Failed to create pmu_stats_log debugfs\n");
#endif
}

void pmu_s3_stats_update(int enter)
{

}

void pmu_stats_finish(void)
{
#ifdef CONFIG_PM_DEBUG
	cancel_delayed_work_sync(&mid_pmu_cxt->log_work);
#endif
	s0ix_lat_stat_finish();
}

#endif /*if CONFIG_X86_MDFLD_POWER || CONFIG_X86_CLV_POWER*/

#ifdef CONFIG_REMOVEME_INTEL_ATOM_MRFLD_POWER

static u32 prev_s0ix_cnt[SYS_STATE_MAX];
static unsigned long long prev_s0ix_res[SYS_STATE_MAX];
static unsigned long long cur_s0ix_res[SYS_STATE_MAX];
static unsigned long long cur_s0ix_cnt[SYS_STATE_MAX];
static u32 S3_count;
static unsigned long long S3_res;

static inline u32 s0ix_count_read(int state)
{
	return readl(s0ix_counters + s0ix_counter_reg_map[state]);
}

static inline u64 s0ix_residency_read(int state)
{
	return readq(s0ix_counters + s0ix_residency_reg_map[state]);
}

static void pmu_stat_seq_printf(struct seq_file *s, int type, char *typestr,
							long long uptime)
{
	unsigned long long t;
	u32 scu_val = 0, time = 0;
	u32 remainder;
	unsigned long init_2_now_time;
	unsigned long long tsc_freq = 1330000;

	/* If tsc calibration fails use the default as 1330Mhz */
	if (tsc_khz)
		tsc_freq = tsc_khz;

	/* Print S0ix residency counter */
	if (type == SYS_STATE_S0I0) {
		for (t = SYS_STATE_S0I1; t <= SYS_STATE_S3; t++)
			time += cur_s0ix_res[t];
	} else if (type < SYS_STATE_S3) {
		t = s0ix_residency_read(type);
		if (t < prev_s0ix_res[type])
			t += (((unsigned long long)~0) - prev_s0ix_res[type]);
		else
			t -= prev_s0ix_res[type];

		if (type == SYS_STATE_S0I3)
			t -= prev_s0ix_res[SYS_STATE_S3];
	} else
		t = prev_s0ix_res[SYS_STATE_S3];

	if (type == SYS_STATE_S0I0) {
		/* uptime(nanoS) - sum_res(miliSec) */
		t = uptime;
		do_div(t, MICRO_SEC);
		time = t - time;
	} else {
		/* s0ix residency counters are in TSC cycle count domain
		 * convert this to milli second time domain
		 */
		remainder = do_div(t, tsc_freq);

		/* store time in millisecs */
		time = (unsigned int)t;
	}
	cur_s0ix_res[type] = (unsigned int)time;

	seq_printf(s, "%s\t%5lu.%03lu\t", typestr,
		(unsigned long)(time/1000), (unsigned long)(time%1000));

	t = uptime;
	do_div(t, MICRO_SEC); /* time in milli secs */

	/* Note: with millisecs accuracy we get more
	 * precise residency percentages, but we have
	 * to trade off with the max number of days
	 * that we can run without clearing counters,
	 * with 32bit counter this value is ~50days.
	 */
	init_2_now_time =  (unsigned long) t;

	/* for calculating percentage residency */
	t	= (u64)(time);
	t	*= 100;

	/* take care of divide by zero */
	if (init_2_now_time) {
		remainder = do_div(t, init_2_now_time);
		time = (unsigned long) t;

		/* for getting 3 digit precision after
		 * decimal dot */
		t = (u64) remainder;
		t *= 1000;
		remainder = do_div(t, init_2_now_time);
	} else
		time = t = 0;

	seq_printf(s, "%5lu.%03lu\t", (unsigned long) time, (unsigned long) t);

	/* Print S0ix counters */
	if (type == SYS_STATE_S0I0) {
		for (t = SYS_STATE_S0I1; t <= SYS_STATE_S3; t++)
			scu_val += cur_s0ix_cnt[t];
		if (scu_val == 0) /* S0I0 residency 100% */
			scu_val = 1;
	} else if (type < SYS_STATE_S3) {
		scu_val = s0ix_count_read(type);
		if (scu_val < prev_s0ix_cnt[type])
			scu_val += (((u32)~0) - prev_s0ix_cnt[type]);
		else
			scu_val -= prev_s0ix_cnt[type];

		if (type == SYS_STATE_S0I3)
			scu_val -= prev_s0ix_cnt[SYS_STATE_S3];
	} else
			scu_val = prev_s0ix_cnt[SYS_STATE_S3];

	if (type != SYS_STATE_S0I0)
		cur_s0ix_cnt[type] = scu_val;

	seq_printf(s, "%5lu\t", (unsigned long) scu_val);

	remainder = 0;
	t = cur_s0ix_res[type];
	if (scu_val) { /* s0ix_time in millisecs */
		do_div(t, scu_val);
		remainder = do_div(t, 1000);
	}
	seq_printf(s, "%5lu.%03lu\n", (unsigned long) t,
			(unsigned long) remainder);
}

static int pmu_devices_state_show(struct seq_file *s, void *unused)
{
	struct pci_dev *pdev = NULL;
	int index, i, pmu_num, ss_idx, ss_pos;
	unsigned int base_class;
	u32 mask, val, nc_pwr_sts;
	struct pmu_ss_states cur_pmsss;
	long long uptime, uptime_t;
	int ret;

	if (!pmu_initialized)
		return 0;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	_pmu2_wait_not_busy();
	pmu_read_sss(&cur_pmsss);
	up(&mid_pmu_cxt->scu_ready_sem);

	seq_printf(s, "SSS: ");

	for (i = 0; i < 4; i++)
		seq_printf(s, "%08lX ", cur_pmsss.pmu2_states[i]);

	seq_printf(s, "cmd_error_int count: %d\n", mid_pmu_cxt->cmd_error_int);

	seq_printf(s, "\t\t\ttime(secs)\tresidency(%%)\tcount\tAvg.Res(Sec)\n");

	down(&mid_pmu_cxt->scu_ready_sem);
	/* Dump S0ix residency counters */
	ret = intel_scu_ipc_simple_command(DUMP_RES_COUNTER, 0);
	if (ret)
		seq_printf(s, "IPC command to DUMP S0ix residency failed\n");

	/* Dump number of interations of S0ix */
	ret = intel_scu_ipc_simple_command(DUMP_S0IX_COUNT, 0);
	if (ret)
		seq_printf(s, "IPC command to DUMP S0ix count failed\n");
	up(&mid_pmu_cxt->scu_ready_sem);

	uptime =  cpu_clock(0);
	uptime -= mid_pmu_cxt->pmu_init_time;
	pmu_stat_seq_printf(s, SYS_STATE_S0I1, "s0i1             ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_LPMP3, "s0i1-lpe         ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_PSH, "s0i1-psh         ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_DISP, "s0i1-disp        ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_LPMP3_PSH, "s0i1-lpe-psh     ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_LPMP3_DISP, "s0i1-lpe-disp    ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_PSH, "s0i1-psh-disp    ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I1_LPMP3_PSH_DISP, "s0i1-lpe-psh-disp", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I2, "s0i2             ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I3, "s0i3             ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I3_PSH_RET, "s0i3-psh-ret     ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S3, "s3               ", uptime);
	pmu_stat_seq_printf(s, SYS_STATE_S0I0, "s0               ", uptime);

	val = do_div(uptime, NANO_SEC);
	seq_printf(s, "\n\nTotal time: %5lu.%03lu Sec\n", (unsigned long)uptime,
		   (unsigned long) val/1000000);

	seq_puts(s, "\nNORTH COMPLEX DEVICES :\n\n");
	seq_puts(s, "  IP_NAME : State D0i0_Time D0i0\% Count\n");
	seq_puts(s, "========================================\n");

	nc_pwr_sts = intel_mid_msgbus_read32(PUNIT_PORT, NC_PM_SSS);
	for (i = 0; i < mrfl_no_of_nc_devices; i++) {
		unsigned long long t, t1;
		u32 remainder, time, d0i0_time_secs;

		val = nc_pwr_sts & 3;
		nc_pwr_sts >>= BITS_PER_LSS;

		/* For Islands after VED, we dont receive
		 * requests for D0ix
		 */
		if (i <= VED) {
			down(&mid_pmu_cxt->scu_ready_sem);

			t = mid_pmu_cxt->nc_d0i0_time[i];
			/* If in D0i0 add current time */
			if (val == D0I0_MASK)
				t += (cpu_clock(0) - mid_pmu_cxt->nc_d0i0_prev_time[i]);

			uptime_t =  cpu_clock(0);
			uptime_t -= mid_pmu_cxt->pmu_init_time;

			up(&mid_pmu_cxt->scu_ready_sem);

			t1 = t;
			d0i0_time_secs = do_div(t1, NANO_SEC);

			/* convert to usecs */
			do_div(t, 10000);
			do_div(uptime_t, 1000000);

			if (uptime_t) {
				remainder = do_div(t, uptime_t);

				time = (unsigned long) t;

				/* for getting 2 digit precision after
				 * decimal dot */
				t = (u64) remainder;
				t *= 100;
				remainder = do_div(t, uptime_t);
			} else {
				time = t = 0;
			}
		}

		seq_printf(s, "%9s : %s", mrfl_nc_devices[i], dstates[val]);
		if (i <= VED) {
			seq_printf(s, " %5lu.%02lu", (unsigned long)t1,
						   (unsigned long) d0i0_time_secs/10000000);
			seq_printf(s, "   %3lu.%02lu", (unsigned long) time, (unsigned long) t);
			seq_printf(s, " %5lu\n", (unsigned long) mid_pmu_cxt->nc_d0i0_count[i]);
		} else
			seq_puts(s, "\n");
	}

	seq_printf(s, "\nSOUTH COMPLEX DEVICES :\n\n");

	seq_puts(s, "PCI VNDR DEVC DEVICE_NAME  DEVICE_DRIVER_STRING  LSS#");
	seq_puts(s, "   State    D0i0_Time        D0i0\% Count\n");
	seq_puts(s, "=====================================================");
	seq_puts(s, "=========================================\n");
	for_each_pci_dev(pdev) {
		unsigned long long t, t1;
		u32 remainder, time, d0i0_time_secs;
		int lss;

		/* find the base class info */
		base_class = pdev->class >> 16;

		if (base_class == PCI_BASE_CLASS_BRIDGE)
			continue;

		if (pmu_pci_to_indexes(pdev, &index, &pmu_num, &ss_idx,
								  &ss_pos))
			continue;

		if (pmu_num == PMU_NUM_1)
			continue;

		mask	= (D0I3_MASK << (ss_pos * BITS_PER_LSS));
		val	= (cur_pmsss.pmu2_states[ss_idx] & mask) >>
						(ss_pos * BITS_PER_LSS);

		lss = index - mid_pmu_cxt->pmu1_max_devs;

		/* for calculating percentage residency */
		down(&mid_pmu_cxt->scu_ready_sem);

		t = mid_pmu_cxt->d0i0_time[lss];
		/* If in D0i0 add current time */
		if (val == D0I0_MASK)
			t += (cpu_clock(0) - mid_pmu_cxt->d0i0_prev_time[lss]);

		uptime_t =  cpu_clock(0);
		uptime_t -= mid_pmu_cxt->pmu_init_time;

		up(&mid_pmu_cxt->scu_ready_sem);

		t1 = t;
		d0i0_time_secs = do_div(t1, NANO_SEC);

		/* convert to usecs */
		do_div(t, 10000);
		do_div(uptime_t, 1000000);

		if (uptime_t) {
			remainder = do_div(t, uptime_t);

			time = (unsigned long) t;

			/* for getting 2 digit precision after
			 * decimal dot */
			t = (u64) remainder;
			t *= 100;
			remainder = do_div(t, uptime_t);
		} else {
			time = t = 0;
		}


		seq_printf(s, "pci %04x %04X %s %20.20s: lss:%02d",
			pdev->vendor, pdev->device, dev_name(&pdev->dev),
			dev_driver_string(&pdev->dev), lss);
		seq_printf(s, " %s", dstates[val & 3]);
		seq_printf(s, "\t%5lu.%02lu", (unsigned long)t1,
						   (unsigned long) d0i0_time_secs/10000000);
		seq_printf(s, "\t%3lu.%02lu", (unsigned long) time, (unsigned long) t);
		seq_printf(s, "\t%5lu\n", (unsigned long) mid_pmu_cxt->d0i0_count[lss]);
	}

	return 0;
}

static int devices_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmu_devices_state_show, NULL);
}

static ssize_t devices_state_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	char buf[32];
	int ret;
	int buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, userbuf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (((strlen("clear")+1) == buf_size) &&
		!strncmp(buf, "clear", strlen("clear"))) {
		down(&mid_pmu_cxt->scu_ready_sem);

		/* Dump S0ix residency counters */
		ret = intel_scu_ipc_simple_command(DUMP_RES_COUNTER, 0);
		if (ret)
			printk(KERN_ERR "IPC command to DUMP S0ix residency failed\n");

		/* Dump number of interations of S0ix */
		ret = intel_scu_ipc_simple_command(DUMP_S0IX_COUNT, 0);
		if (ret)
			printk(KERN_ERR "IPC command to DUMP S0ix count failed\n");

		mid_pmu_cxt->pmu_init_time = cpu_clock(0);
		prev_s0ix_cnt[SYS_STATE_S0I1] = s0ix_count_read(SYS_STATE_S0I1);
		prev_s0ix_cnt[SYS_STATE_S0I1_LPMP3] = s0ix_count_read(SYS_STATE_S0I1_LPMP3);
		prev_s0ix_cnt[SYS_STATE_S0I1_PSH] = s0ix_count_read(SYS_STATE_S0I1_PSH);
		prev_s0ix_cnt[SYS_STATE_S0I1_DISP] = s0ix_count_read(SYS_STATE_S0I1_DISP);
		prev_s0ix_cnt[SYS_STATE_S0I1_LPMP3_PSH] = s0ix_count_read(SYS_STATE_S0I1_LPMP3_PSH);
		prev_s0ix_cnt[SYS_STATE_S0I1_LPMP3_DISP] = s0ix_count_read(SYS_STATE_S0I1_LPMP3_DISP);
		prev_s0ix_cnt[SYS_STATE_S0I1_PSH_DISP] = s0ix_count_read(SYS_STATE_S0I1_PSH_DISP);
		prev_s0ix_cnt[SYS_STATE_S0I1_LPMP3_PSH_DISP] = s0ix_count_read(SYS_STATE_S0I1_LPMP3_PSH_DISP);
		prev_s0ix_cnt[SYS_STATE_S0I2] = s0ix_count_read(SYS_STATE_S0I2);
		prev_s0ix_cnt[SYS_STATE_S0I3] = s0ix_count_read(SYS_STATE_S0I3);
		prev_s0ix_cnt[SYS_STATE_S0I3_PSH_RET] = s0ix_count_read(SYS_STATE_S0I3_PSH_RET);
		prev_s0ix_cnt[SYS_STATE_S3] = 0;
		prev_s0ix_res[SYS_STATE_S0I1] = s0ix_residency_read(SYS_STATE_S0I1);
		prev_s0ix_res[SYS_STATE_S0I1_LPMP3] = s0ix_residency_read(SYS_STATE_S0I1_LPMP3);
		prev_s0ix_res[SYS_STATE_S0I1_PSH] = s0ix_residency_read(SYS_STATE_S0I1_PSH);
		prev_s0ix_res[SYS_STATE_S0I1_DISP] = s0ix_residency_read(SYS_STATE_S0I1_DISP);
		prev_s0ix_res[SYS_STATE_S0I1_LPMP3_PSH] = s0ix_residency_read(SYS_STATE_S0I1_LPMP3_PSH);
		prev_s0ix_res[SYS_STATE_S0I1_LPMP3_DISP] = s0ix_residency_read(SYS_STATE_S0I1_LPMP3_DISP);
		prev_s0ix_res[SYS_STATE_S0I1_PSH_DISP] = s0ix_residency_read(SYS_STATE_S0I1_PSH_DISP);
		prev_s0ix_res[SYS_STATE_S0I1_LPMP3_PSH_DISP] = s0ix_residency_read(SYS_STATE_S0I1_LPMP3_PSH_DISP);
		prev_s0ix_res[SYS_STATE_S0I2] = s0ix_residency_read(SYS_STATE_S0I2);
		prev_s0ix_res[SYS_STATE_S0I3] = s0ix_residency_read(SYS_STATE_S0I3);
		prev_s0ix_res[SYS_STATE_S0I3_PSH_RET] = s0ix_residency_read(SYS_STATE_S0I3_PSH_RET);
		prev_s0ix_res[SYS_STATE_S3] = 0 ;

		/* D0i0 time stats clear */
		{
			int i;
			for (i = 0; i < MAX_LSS_POSSIBLE; i++) {
				mid_pmu_cxt->d0i0_count[i] = 0;
				mid_pmu_cxt->d0i0_time[i] = 0;
				mid_pmu_cxt->d0i0_prev_time[i] = cpu_clock(0);
			}

			for (i = 0; i < OSPM_MAX_POWER_ISLANDS; i++) {
				mid_pmu_cxt->nc_d0i0_count[i] = 0;
				mid_pmu_cxt->nc_d0i0_time[i] = 0;
				mid_pmu_cxt->nc_d0i0_prev_time[i] = cpu_clock(0);
			}
		}

		up(&mid_pmu_cxt->scu_ready_sem);
	}

	return buf_size;
}


static const struct file_operations devices_state_operations = {
	.open		= devices_state_open,
	.read		= seq_read,
	.write		= devices_state_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#ifdef CONFIG_PM_DEBUG
static int ignore_lss_show(struct seq_file *s, void *unused)
{
	u32 local_ignore_lss[4];

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	memcpy(local_ignore_lss, mid_pmu_cxt->ignore_lss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	seq_printf(s, "IGNORE_LSS[0]: %08X\n", local_ignore_lss[0]);
	seq_printf(s, "IGNORE_LSS[1]: %08X\n", local_ignore_lss[1]);
	seq_printf(s, "IGNORE_LSS[2]: %08X\n", local_ignore_lss[2]);
	seq_printf(s, "IGNORE_LSS[3]: %08X\n", local_ignore_lss[3]);

	return 0;
}

static int ignore_add_open(struct inode *inode, struct file *file)
{
	return single_open(file, ignore_lss_show, NULL);
}

static ssize_t ignore_add_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	int sub_sys_pos, sub_sys_index;
	u32 lss, local_ignore_lss[4];
	u32 pm_cmd_val;

	res = kstrtou32_from_user(userbuf, count, 0, &lss);
	if (res)
		return -EINVAL;

	if (lss > MAX_LSS_POSSIBLE)
		return -EINVAL;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	memcpy(local_ignore_lss, mid_pmu_cxt->ignore_lss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	/* If set to MAX_LSS_POSSIBLE it means
	 * ignore all.
	 */
	if (lss == MAX_LSS_POSSIBLE) {
		local_ignore_lss[0] = 0xFFFFFFFF;
		local_ignore_lss[1] = 0xFFFFFFFF;
		local_ignore_lss[2] = 0xFFFFFFFF;
		local_ignore_lss[3] = 0xFFFFFFFF;
	} else {
		sub_sys_index	= lss / mid_pmu_cxt->ss_per_reg;
		sub_sys_pos	= lss % mid_pmu_cxt->ss_per_reg;

		pm_cmd_val =
			(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));
		local_ignore_lss[sub_sys_index] |= pm_cmd_val;
	}

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	memcpy(mid_pmu_cxt->ignore_lss, local_ignore_lss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	return count;
}

static const struct file_operations ignore_add_ops = {
	.open		= ignore_add_open,
	.read		= seq_read,
	.write		= ignore_add_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int ignore_remove_open(struct inode *inode, struct file *file)
{
	return single_open(file, ignore_lss_show, NULL);
}

static ssize_t ignore_remove_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	int sub_sys_pos, sub_sys_index;
	u32 lss, local_ignore_lss[4];
	u32 pm_cmd_val;

	res = kstrtou32_from_user(userbuf, count, 0, &lss);
	if (res)
		return -EINVAL;

	if (lss > MAX_LSS_POSSIBLE)
		return -EINVAL;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	memcpy(local_ignore_lss, mid_pmu_cxt->ignore_lss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	/* If set to MAX_LSS_POSSIBLE it means
	 * remove all from ignore list.
	 */
	if (lss == MAX_LSS_POSSIBLE) {
		local_ignore_lss[0] = 0;
		local_ignore_lss[1] = 0;
		local_ignore_lss[2] = 0;
		local_ignore_lss[3] = 0;
	} else {
		sub_sys_index	= lss / mid_pmu_cxt->ss_per_reg;
		sub_sys_pos	= lss % mid_pmu_cxt->ss_per_reg;

		pm_cmd_val =
			(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));
		local_ignore_lss[sub_sys_index] &= ~pm_cmd_val;
	}

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	memcpy(mid_pmu_cxt->ignore_lss, local_ignore_lss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	return count;
}

static const struct file_operations ignore_remove_ops = {
	.open		= ignore_remove_open,
	.read		= seq_read,
	.write		= ignore_remove_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int pmu_sync_d0ix_show(struct seq_file *s, void *unused)
{
	int i;
	u32 local_os_sss[4];
	struct pmu_ss_states cur_pmsss;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	_pmu2_wait_not_busy();
	/* Read SCU SSS */
	pmu_read_sss(&cur_pmsss);
	/* Read OS SSS */
	memcpy(local_os_sss, mid_pmu_cxt->os_sss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	for (i = 0; i < 4; i++)
		seq_printf(s, "OS_SSS[%d]: %08X\tSSS[%d]: %08lX\n", i,
				local_os_sss[i], i, cur_pmsss.pmu2_states[i]);

	return 0;
}

static int pmu_sync_d0ix_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmu_sync_d0ix_show, NULL);
}

static ssize_t pmu_sync_d0ix_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res, i;
	bool send_cmd;
	u32 lss, local_os_sss[4];
	int sub_sys_pos, sub_sys_index;
	u32 pm_cmd_val;
	u32 temp_sss;

	struct pmu_ss_states cur_pmsss;

	res = kstrtou32_from_user(userbuf, count, 0, &lss);
	if (res)
		return -EINVAL;

	if (lss > MAX_LSS_POSSIBLE)
		return -EINVAL;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	_pmu2_wait_not_busy();
	/* Read SCU SSS */
	pmu_read_sss(&cur_pmsss);

	for (i = 0; i < 4; i++)
		local_os_sss[i] = mid_pmu_cxt->os_sss[i] &
				~mid_pmu_cxt->ignore_lss[i];

	send_cmd = false;
	for (i = 0; i < 4; i++) {
		if (local_os_sss[i] != cur_pmsss.pmu2_states[i]) {
			send_cmd = true;
			break;
		}
	}

	if (send_cmd) {
		int status;

		if (lss == MAX_LSS_POSSIBLE) {
			memcpy(cur_pmsss.pmu2_states, local_os_sss,
							 (sizeof(u32)*4));
		} else {
			bool same;
			sub_sys_index	= lss / mid_pmu_cxt->ss_per_reg;
			sub_sys_pos	= lss % mid_pmu_cxt->ss_per_reg;
			pm_cmd_val =
				(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));

			/* dont send d0ix request if its same */
			same =
			((cur_pmsss.pmu2_states[sub_sys_index] & pm_cmd_val)
			== (mid_pmu_cxt->os_sss[sub_sys_index] & pm_cmd_val));

			if (same)
				goto unlock;

			cur_pmsss.pmu2_states[sub_sys_index] &= ~pm_cmd_val;
			temp_sss =
				mid_pmu_cxt->os_sss[sub_sys_index] & pm_cmd_val;
			cur_pmsss.pmu2_states[sub_sys_index] |= temp_sss;
		}

		/* Issue the pmu command to PMU 2
		 * flag is needed to distinguish between
		 * S0ix vs interactive command in pmu_sc_irq()
		 */
		status = pmu_issue_interactive_command(&cur_pmsss, false,
							false);

		if (unlikely(status != PMU_SUCCESS)) {
			dev_dbg(&mid_pmu_cxt->pmu_dev->dev,
				 "Failed to Issue a PM command to PMU2\n");
			goto unlock;
		}

		/*
		 * Wait for interactive command to complete.
		 * If we dont wait, there is a possibility that
		 * the driver may access the device before its
		 * powered on in SCU.
		 *
		 */
		status = _pmu2_wait_not_busy();
		if (unlikely(status)) {
			printk(KERN_CRIT "%s: D0ix transition failure\n",
				__func__);
		}
	}

unlock:
	up(&mid_pmu_cxt->scu_ready_sem);

	return count;
}

static const struct file_operations pmu_sync_d0ix_ops = {
	.open		= pmu_sync_d0ix_open,
	.read		= seq_read,
	.write		= pmu_sync_d0ix_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int pmu_force_d0ix_show(struct seq_file *s, void *unused)
{
	int i;
	u32 local_os_sss[4];

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);
	/* Read OS SSS */
	memcpy(local_os_sss, mid_pmu_cxt->os_sss, (sizeof(u32)*4));
	up(&mid_pmu_cxt->scu_ready_sem);

	for (i = 0; i < 4; i++)
		seq_printf(s, "OS_SSS[%d]: %08X\n", i, local_os_sss[i]);

	return 0;
}

static int pmu_force_d0ix_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmu_force_d0ix_show, NULL);
}

static ssize_t pmu_force_d0i3_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	u32 lss, local_os_sss[4];
	int sub_sys_pos, sub_sys_index;
	u32 pm_cmd_val;

	res = kstrtou32_from_user(userbuf, count, 0, &lss);
	if (res)
		return -EINVAL;

	if (lss > MAX_LSS_POSSIBLE)
		return -EINVAL;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);

	if (lss == MAX_LSS_POSSIBLE) {
		local_os_sss[0] =
		local_os_sss[1] =
		local_os_sss[2] =
		local_os_sss[3] = 0xFFFFFFFF;
	} else {
		memcpy(local_os_sss, mid_pmu_cxt->os_sss, (sizeof(u32)*4));
		sub_sys_index	= lss / mid_pmu_cxt->ss_per_reg;
		sub_sys_pos	= lss % mid_pmu_cxt->ss_per_reg;
		pm_cmd_val =
			(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));

		local_os_sss[sub_sys_index] |= pm_cmd_val;
	}

	memcpy(mid_pmu_cxt->os_sss, local_os_sss, (sizeof(u32)*4));

	up(&mid_pmu_cxt->scu_ready_sem);

	return count;
}

static const struct file_operations pmu_force_d0i3_ops = {
	.open		= pmu_force_d0ix_open,
	.read		= seq_read,
	.write		= pmu_force_d0i3_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t pmu_force_d0i0_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	u32 lss, local_os_sss[4];
	int sub_sys_pos, sub_sys_index;
	u32 pm_cmd_val;

	res = kstrtou32_from_user(userbuf, count, 0, &lss);
	if (res)
		return -EINVAL;

	if (lss > MAX_LSS_POSSIBLE)
		return -EINVAL;

	/* Acquire the scu_ready_sem */
	down(&mid_pmu_cxt->scu_ready_sem);

	if (lss == MAX_LSS_POSSIBLE) {
		local_os_sss[0] =
		local_os_sss[1] =
		local_os_sss[2] =
		local_os_sss[3] = 0;
	} else {
		memcpy(local_os_sss, mid_pmu_cxt->os_sss, (sizeof(u32)*4));
		sub_sys_index	= lss / mid_pmu_cxt->ss_per_reg;
		sub_sys_pos	= lss % mid_pmu_cxt->ss_per_reg;
		pm_cmd_val =
			(D0I3_MASK << (sub_sys_pos * BITS_PER_LSS));

		local_os_sss[sub_sys_index] &= ~pm_cmd_val;
	}

	memcpy(mid_pmu_cxt->os_sss, local_os_sss, (sizeof(u32)*4));

	up(&mid_pmu_cxt->scu_ready_sem);

	return count;
}

static const struct file_operations pmu_force_d0i0_ops = {
	.open		= pmu_force_d0ix_open,
	.read		= seq_read,
	.write		= pmu_force_d0i0_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int cstate_ignore_add_show(struct seq_file *s, void *unused)
{
	int i;
	seq_printf(s, "CSTATES IGNORED: ");
	for (i = 0; i < (CPUIDLE_STATE_MAX-1); i++)
		if ((mid_pmu_cxt->cstate_ignore & (1 << i)))
			seq_printf(s, "%d, ", i+1);

	seq_printf(s, "\n");
	return 0;
}

static int cstate_ignore_add_open(struct inode *inode, struct file *file)
{
	return single_open(file, cstate_ignore_add_show, NULL);
}

static ssize_t cstate_ignore_add_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	int cstate;

	res = kstrtou32_from_user(userbuf, count, 0, &cstate);
	if (res)
		return -EINVAL;

	if (cstate > MAX_CSTATES_POSSIBLE)
		return -EINVAL;

	/* cannot add/remove C0, C1 */
	if (((cstate == 0) || (cstate == 1))) {
		printk(KERN_CRIT "C0 C1 state cannot be used.\n");
		return -EINVAL;
	}

	if (!mid_pmu_cxt->cstate_qos)
		return -EINVAL;

	if (cstate == MAX_CSTATES_POSSIBLE) {
		mid_pmu_cxt->cstate_ignore = ((1 << (CPUIDLE_STATE_MAX-1)) - 1);
		pm_qos_update_request(mid_pmu_cxt->cstate_qos,
					CSTATE_EXIT_LATENCY_C1 - 1);
	} else {
		u32 cstate_exit_latency[CPUIDLE_STATE_MAX];
		u32 local_cstate_allowed;
		int max_cstate_allowed;

		/* 0 is C1 state */
		cstate--;
		mid_pmu_cxt->cstate_ignore |= (1 << cstate);

		/* by default remove C1 from ignore list */
		mid_pmu_cxt->cstate_ignore &= ~(1 << 0);

		/* populate cstate latency table */
		cstate_exit_latency[0] = CSTATE_EXIT_LATENCY_C1;
		cstate_exit_latency[1] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[2] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[3] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[4] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[5] = CSTATE_EXIT_LATENCY_C6;
		cstate_exit_latency[6] = CSTATE_EXIT_LATENCY_S0i1;
		cstate_exit_latency[7] = CSTATE_EXIT_LATENCY_S0i2;
		cstate_exit_latency[8] = CSTATE_EXIT_LATENCY_S0i3;
		cstate_exit_latency[9] = PM_QOS_DEFAULT_VALUE;

		local_cstate_allowed = ~mid_pmu_cxt->cstate_ignore;

		/* restrict to max c-states */
		local_cstate_allowed &= ((1<<(CPUIDLE_STATE_MAX-1))-1);

		/* If no states allowed will return 0 */
		max_cstate_allowed = fls(local_cstate_allowed);

		printk(KERN_CRIT "max_cstate: %d local_cstate_allowed = %x\n",
			max_cstate_allowed, local_cstate_allowed);
		printk(KERN_CRIT "exit latency = %d\n",
				(cstate_exit_latency[max_cstate_allowed]-1));
		pm_qos_update_request(mid_pmu_cxt->cstate_qos,
				(cstate_exit_latency[max_cstate_allowed]-1));
	}

	return count;
}

static const struct file_operations cstate_ignore_add_ops = {
	.open		= cstate_ignore_add_open,
	.read		= seq_read,
	.write		= cstate_ignore_add_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int cstate_ignore_remove_show(struct seq_file *s, void *unused)
{
	int i;
	seq_printf(s, "CSTATES ALLOWED: ");
	for (i = 0; i < (CPUIDLE_STATE_MAX-1); i++)
		if (!(mid_pmu_cxt->cstate_ignore & (1 << i)))
			seq_printf(s, "%d, ", i+1);

	seq_printf(s, "\n");

	return 0;
}

static int cstate_ignore_remove_open(struct inode *inode, struct file *file)
{
	return single_open(file, cstate_ignore_remove_show, NULL);
}

static ssize_t cstate_ignore_remove_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	int cstate;
	res = kstrtou32_from_user(userbuf, count, 0, &cstate);
	if (res)
		return -EINVAL;

	if (cstate > MAX_CSTATES_POSSIBLE)
		return -EINVAL;

	/* cannot add/remove C0, C1 */
	if (((cstate == 0) || (cstate == 1))) {
		printk(KERN_CRIT "C0 C1 state cannot be used.\n");
		return -EINVAL;
	}

	if (!mid_pmu_cxt->cstate_qos)
		return -EINVAL;

	if (cstate == MAX_CSTATES_POSSIBLE) {
		mid_pmu_cxt->cstate_ignore =
				~((1 << (CPUIDLE_STATE_MAX-1)) - 1);
		/* Ignore C2, C3, C5, C8 states */
		mid_pmu_cxt->cstate_ignore |= (1 << 1);
		mid_pmu_cxt->cstate_ignore |= (1 << 2);
		mid_pmu_cxt->cstate_ignore |= (1 << 4);
		mid_pmu_cxt->cstate_ignore |= (1 << 7);

		pm_qos_update_request(mid_pmu_cxt->cstate_qos,
						PM_QOS_DEFAULT_VALUE);
	} else {
		u32 cstate_exit_latency[CPUIDLE_STATE_MAX];
		u32 local_cstate_allowed;
		int max_cstate_allowed;

		/* populate cstate latency table */
		cstate_exit_latency[0] = CSTATE_EXIT_LATENCY_C1;
		cstate_exit_latency[1] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[2] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[3] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[4] = CSTATE_EXIT_LATENCY_C2;
		cstate_exit_latency[5] = CSTATE_EXIT_LATENCY_C6;
		cstate_exit_latency[6] = CSTATE_EXIT_LATENCY_S0i1;
		cstate_exit_latency[7] = CSTATE_EXIT_LATENCY_S0i2;
		cstate_exit_latency[8] = CSTATE_EXIT_LATENCY_S0i3;
		cstate_exit_latency[9] = PM_QOS_DEFAULT_VALUE;

		/* 0 is C1 state */
		cstate--;
		mid_pmu_cxt->cstate_ignore &= ~(1 << cstate);

		/* by default remove C1 from ignore list */
		mid_pmu_cxt->cstate_ignore &= ~(1 << 0);

		/* Ignore C2, C3, C5, C8 states */
		mid_pmu_cxt->cstate_ignore |= (1 << 1);
		mid_pmu_cxt->cstate_ignore |= (1 << 2);
		mid_pmu_cxt->cstate_ignore |= (1 << 4);
		mid_pmu_cxt->cstate_ignore |= (1 << 7);

		local_cstate_allowed = ~mid_pmu_cxt->cstate_ignore;
		/* restrict to max c-states */
		local_cstate_allowed &= ((1<<(CPUIDLE_STATE_MAX-1))-1);

		/* If no states allowed will return 0 */
		max_cstate_allowed = fls(local_cstate_allowed);
		printk(KERN_CRIT "max_cstate: %d local_cstate_allowed = %x\n",
			max_cstate_allowed, local_cstate_allowed);
		printk(KERN_CRIT "exit latency = %d\n",
				(cstate_exit_latency[max_cstate_allowed]-1));
		pm_qos_update_request(mid_pmu_cxt->cstate_qos,
				(cstate_exit_latency[max_cstate_allowed]-1));
	}

	return count;
}

static const struct file_operations cstate_ignore_remove_ops = {
	.open		= cstate_ignore_remove_open,
	.read		= seq_read,
	.write		= cstate_ignore_remove_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int s3_ctrl_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%d\n", enable_s3);
	return 0;
}

static int s3_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, s3_ctrl_show, NULL);
}

static ssize_t s3_ctrl_write(struct file *file,
		     const char __user *userbuf, size_t count, loff_t *ppos)
{
	int res;
	int local_s3_ctrl;

	res = kstrtou32_from_user(userbuf, count, 0, &local_s3_ctrl);
	if (res)
		return -EINVAL;

	enable_s3 = local_s3_ctrl ? 1 : 0;

	if (enable_s3)
		__pm_relax(mid_pmu_cxt->pmu_wake_lock);
	else
		__pm_stay_awake(mid_pmu_cxt->pmu_wake_lock);

	return count;
}

static const struct file_operations s3_ctrl_ops = {
	.open		= s3_ctrl_open,
	.read		= seq_read,
	.write		= s3_ctrl_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*
 * cstate: c1=1, c2=2, ..., c6=6, c7=7, c8=7, c9=7
 *         for s0i1/s0i2/s0i3 cstate=7.
 * index: this is the index in cpuidle_driver cstates table
 *        where c1 is the 2nd element of the table
 */
unsigned int pmu_get_new_cstate(unsigned int cstate, int *index)
{
	static int cstate_index_table[CPUIDLE_STATE_MAX-1] = {
					1, 1, 1, 1, 1, 2, 3, 3, 4};
	unsigned int new_cstate = cstate;
	u32 local_cstate = (u32)(cstate);
	u32 local_cstate_allowed = ~mid_pmu_cxt->cstate_ignore;
	u32 cstate_mask, cstate_no_s0ix_mask = (u32)((1 << 6) - 1);

	if (platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD)) {
		/* cstate is also 7 for C9 so correct */
		/* this is supposing that C9 is the 4th cstate allowed */
		if ((local_cstate == 7) && (*index == 4))
			local_cstate = 9;

		/* get next low cstate allowed */
		cstate_mask = (u32)((1 << local_cstate)-1);
		local_cstate_allowed	&= ((1<<(CPUIDLE_STATE_MAX-1))-1);
		local_cstate_allowed	&= cstate_mask;

		/* Make sure we dont end up with new_state == 0 */
		local_cstate_allowed |= 1;
		new_cstate	= fls(local_cstate_allowed);

		*index	= cstate_index_table[new_cstate-1];
	}

	return new_cstate;
}
#endif

DEFINE_PER_CPU(u64[NUM_CSTATES_RES_MEASURE], c_states_res);

static int read_c_states_res(void)
{
	int cpu, i;
	u32 lo, hi;

	u32 c_states_res_msr[NUM_CSTATES_RES_MEASURE] = {
		PUNIT_CR_CORE_C1_RES_MSR,
		PUNIT_CR_CORE_C4_RES_MSR,
		PUNIT_CR_CORE_C6_RES_MSR
	};

	for_each_online_cpu(cpu)
		for (i = 0; i < NUM_CSTATES_RES_MEASURE; i++) {
			u64 temp;
			rdmsr_on_cpu(cpu, c_states_res_msr[i], &lo, &hi);
			temp = hi;
			temp <<= 32;
			temp |= lo;
			per_cpu(c_states_res, cpu)[i] = temp;
		}

	return 0;
}

static int c_states_stat_show(struct seq_file *s, void *unused)
{
	char *c_states_name[] = {
		"C1",
		"C4",
		"C6"
	};

	int i, cpu;

	seq_printf(s, "C STATES: %20s\n", "Residecy");
	for_each_online_cpu(cpu)
		seq_printf(s, "%18s %d", "Core", cpu);
	seq_printf(s, "\n");

	read_c_states_res();
	for (i = 0; i < NUM_CSTATES_RES_MEASURE; i++) {
		seq_printf(s, "%s", c_states_name[i]);
		for_each_online_cpu(cpu)
			seq_printf(s, "%18llu", per_cpu(c_states_res, cpu)[i]);
		seq_printf(s, "\n");
	}
	return 0;
}

static int c_states_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, c_states_stat_show, NULL);
}

static const struct file_operations c_states_stat_ops = {
	.open		= c_states_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*These are place holders and will be enabled in next patch*/

void pmu_log_pmu_irq(int status) { return; };
void pmu_log_ipc_irq(void) { return; };
void pmu_log_ipc(u32 command) { return; };
void pmu_log_command(u32 command, struct pmu_ss_states *pm_ssc) { return; };
void pmu_dump_logs(void) { return; };
void pmu_stat_start(enum sys_state type) { return; };
void pmu_stat_end(void) { return; };
void pmu_stat_error(u8 err_type) { return; };
void pmu_s0ix_demotion_stat(int req_state, int grant_state) { return; };
EXPORT_SYMBOL(pmu_s0ix_demotion_stat);

void pmu_stats_finish(void)
{
#ifdef CONFIG_PM_DEBUG
	if (mid_pmu_cxt->cstate_qos) {
		pm_qos_remove_request(mid_pmu_cxt->cstate_qos);
		kfree(mid_pmu_cxt->cstate_qos);
		mid_pmu_cxt->cstate_qos = NULL;
	}
#endif

	return;
}

void pmu_s3_stats_update(int enter)
{
#ifdef CONFIG_PM_DEBUG
	int ret;

	down(&mid_pmu_cxt->scu_ready_sem);
	/* Dump S0ix residency counters */
	ret = intel_scu_ipc_simple_command(DUMP_RES_COUNTER, 0);
	if (ret)
		printk(KERN_ERR "IPC command to DUMP S0ix residency failed\n");

	/* Dump number of interations of S0ix */
	ret = intel_scu_ipc_simple_command(DUMP_S0IX_COUNT, 0);
	if (ret)
		printk(KERN_ERR "IPC command to DUMP S0ix count failed\n");

	up(&mid_pmu_cxt->scu_ready_sem);

	if (enter == 1) {
		S3_count  = s0ix_count_read(SYS_STATE_S0I3);
		S3_res = s0ix_residency_read(SYS_STATE_S0I3);
	} else {
		prev_s0ix_cnt[SYS_STATE_S3] +=
			(s0ix_count_read(SYS_STATE_S0I3)) - S3_count;
		prev_s0ix_res[SYS_STATE_S3] += (s0ix_residency_read(SYS_STATE_S0I3)) - S3_res;
	}

#endif
	return;
}


void pmu_stats_init(void)
{
	/* /sys/kernel/debug/mid_pmu_states */
	(void) debugfs_create_file("mid_pmu_states", S_IFREG | S_IRUGO,
				NULL, NULL, &devices_state_operations);

	/* /sys/kernel/debug/c_p_states_stat */
	(void) debugfs_create_file("c_states_stat", S_IFREG | S_IRUGO,
				NULL, NULL, &c_states_stat_ops);
#ifdef CONFIG_PM_DEBUG
	if (platform_is(INTEL_ATOM_MRFLD) || platform_is(INTEL_ATOM_MOORFLD)) {
		/* If s0ix is disabled then restrict to C6 */
		if (!enable_s0ix) {
			mid_pmu_cxt->cstate_ignore =
				~((1 << (CPUIDLE_STATE_MAX-1)) - 1);

			/* Ignore C2, C3, C5 states */
			mid_pmu_cxt->cstate_ignore |= (1 << 1);
			mid_pmu_cxt->cstate_ignore |= (1 << 2);
			mid_pmu_cxt->cstate_ignore |= (1 << 4);

			/* For now ignore C7, C8, C9 states */
			mid_pmu_cxt->cstate_ignore |= (1 << 6);
			mid_pmu_cxt->cstate_ignore |= (1 << 7);
			mid_pmu_cxt->cstate_ignore |= (1 << 8);
		} else {
			mid_pmu_cxt->cstate_ignore =
				~((1 << (CPUIDLE_STATE_MAX-1)) - 1);

			/* Ignore C2, C3, C5, C8 states */
			mid_pmu_cxt->cstate_ignore |= (1 << 1);
			mid_pmu_cxt->cstate_ignore |= (1 << 2);
			mid_pmu_cxt->cstate_ignore |= (1 << 4);
			mid_pmu_cxt->cstate_ignore |= (1 << 7);
		}

		mid_pmu_cxt->cstate_qos =
			kzalloc(sizeof(struct pm_qos_request), GFP_KERNEL);
		if (mid_pmu_cxt->cstate_qos) {
			pm_qos_add_request(mid_pmu_cxt->cstate_qos,
				 PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		}

		/* If s0ix is disabled then restrict to C6 */
		if (!enable_s0ix) {
			/* Restrict platform Cx state to C6 */
			pm_qos_update_request(mid_pmu_cxt->cstate_qos,
						(CSTATE_EXIT_LATENCY_S0i1-1));
		}

		/* D0i0 time stats clear */
		{
			int i;
			for (i = 0; i < MAX_LSS_POSSIBLE; i++) {
				mid_pmu_cxt->d0i0_time[i] = 0;
				mid_pmu_cxt->d0i0_prev_time[i] = cpu_clock(0);
			}

			for (i = 0; i < OSPM_MAX_POWER_ISLANDS; i++) {
				mid_pmu_cxt->nc_d0i0_time[i] = 0;
				mid_pmu_cxt->nc_d0i0_prev_time[i] = cpu_clock(0);
			}
		}

		/* /sys/kernel/debug/ignore_add */
		(void) debugfs_create_file("ignore_add", S_IFREG | S_IRUGO,
					NULL, NULL, &ignore_add_ops);
		/* /sys/kernel/debug/ignore_remove */
		(void) debugfs_create_file("ignore_remove", S_IFREG | S_IRUGO,
					NULL, NULL, &ignore_remove_ops);
		/* /sys/kernel/debug/pmu_sync_d0ix */
		(void) debugfs_create_file("pmu_sync_d0ix", S_IFREG | S_IRUGO,
					NULL, NULL, &pmu_sync_d0ix_ops);
		/* /sys/kernel/debug/pmu_force_d0i0 */
		(void) debugfs_create_file("pmu_force_d0i0", S_IFREG | S_IRUGO,
					NULL, NULL, &pmu_force_d0i0_ops);
		/* /sys/kernel/debug/pmu_force_d0i3 */
		(void) debugfs_create_file("pmu_force_d0i3", S_IFREG | S_IRUGO,
					NULL, NULL, &pmu_force_d0i3_ops);
		/* /sys/kernel/debug/cstate_ignore_add */
		(void) debugfs_create_file("cstate_ignore_add",
			S_IFREG | S_IRUGO, NULL, NULL, &cstate_ignore_add_ops);
		/* /sys/kernel/debug/cstate_ignore_remove */
		(void) debugfs_create_file("cstate_ignore_remove",
		S_IFREG | S_IRUGO, NULL, NULL, &cstate_ignore_remove_ops);
		/* /sys/kernel/debug/cstate_ignore_remove */
		(void) debugfs_create_file("s3_ctrl",
		S_IFREG | S_IRUGO, NULL, NULL, &s3_ctrl_ops);
	}
#endif
}

#endif /*if CONFIG_REMOVEME_INTEL_ATOM_MRFLD_POWER*/
