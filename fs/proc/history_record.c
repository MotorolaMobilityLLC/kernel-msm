#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/history_record.h>

static struct saved_history_record all_history_record[SAVED_HISTORY_MAX];
static atomic_t saved_history_current = ATOMIC_INIT(-1);

/* mapping entry name according type */
static char *entry_name[] = {
			" ",
			"sgx-1",
			"sgx-2",
			"pipe",
			"msvdx_stat",
			"vdc_stat",
};

static void history_record_init(struct saved_history_record *record)
{
	int cpu;

	cpu = raw_smp_processor_id();
	record->ts = cpu_clock(cpu);
	record->record_value.value = 0;
}

struct saved_history_record *get_new_history_record(void)
{
	struct saved_history_record *precord = NULL;
	int ret = atomic_add_return(1, &saved_history_current);
	/* in case overflow */
	if (ret < 0) {
		atomic_set(&saved_history_current, 0);
		ret = 0;
	}
	precord = &all_history_record[ret%SAVED_HISTORY_MAX];
	history_record_init(precord);
	return precord;
}
EXPORT_SYMBOL(get_new_history_record);

static void print_saved_history_record(struct saved_history_record *record)
{
	unsigned long long ts = record->ts;
	unsigned long nanosec_rem = do_div(ts, 1000000000);

	printk(KERN_INFO "----\n");
	switch (record->type) {
	case 1:
	case 2:
		printk(KERN_INFO "name:[%s] ts[%5lu.%06lu] HostIrqCountSample[%u] InterruptCount[%u]\n",
				entry_name[record->type],
				(unsigned long)ts,
				nanosec_rem / 1000,
				record->record_value.sgx.HostIrqCountSample,
				record->record_value.sgx.InterruptCount);
		break;

	case 3:
		printk(KERN_INFO "name:[%s] ts[%5lu.%06lu] pipe[%u] pipe_stat_val[%#x]\n",
				entry_name[record->type],
				(unsigned long)ts,
				nanosec_rem / 1000,
				record->record_value.pipe.pipe_nu,
				record->record_value.pipe.pipe_stat_val);
		break;

	case 4:
		printk(KERN_INFO "name:[%s] ts[%5lu.%06lu] msvdx_stat[%#lx]\n",
				entry_name[record->type],
				(unsigned long)ts,
				nanosec_rem / 1000,
				record->record_value.msvdx_stat);
		break;

	case 5:
		printk(KERN_INFO "name:[%s] ts[%5lu.%06lu] vdc_stat[%#lx]\n",
				entry_name[record->type],
				(unsigned long)ts,
				nanosec_rem / 1000,
				record->record_value.vdc_stat);
		break;

	default:
		break;

	}
}


void interrupt_dump_history(void)
{
	int i, start;
	unsigned int total = atomic_read(&saved_history_current);

	start = total % SAVED_HISTORY_MAX;
	printk(KERN_INFO "<----current timestamp\n");
	printk(KERN_INFO "start[%d] saved[%d]\n",
			start, total);
	for (i = start; i >= 0; i--) {
		if (i % 10 == 0)
			schedule();
		print_saved_history_record(&all_history_record[i]);
	}
	for (i = SAVED_HISTORY_MAX - 1; i > start; i--) {
		if (i % 10 == 0)
			schedule();
		print_saved_history_record(&all_history_record[i]);
	}
}
EXPORT_SYMBOL(interrupt_dump_history);

static ssize_t debug_read_history_record(struct file *f,
		char __user *buf, size_t size, loff_t *off)
{
	interrupt_dump_history();
	return 0;
}

static const struct file_operations debug_history_proc_fops = {
           .owner = THIS_MODULE,
           .read = debug_read_history_record,
};

static int __init debug_read_history_record_entry(void)
{
	struct proc_dir_entry *res = NULL;
	res = proc_create_data("debug_read_sgx_history", S_IRUGO | S_IWUSR,
				NULL, &debug_history_proc_fops, NULL);
	return 0;
}

device_initcall(debug_read_history_record_entry);
