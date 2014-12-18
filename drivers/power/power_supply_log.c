/*
 *  You may use this code as per GPL version 2
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/namei.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/unistd.h>
#include <asm/syscall.h>

static struct notifier_block power_notifier;

asmlinkage int (*my_sys_open)(const char *, int, int);
asmlinkage int (*my_sys_close)(int fd);

static char *log_file;
module_param(log_file, charp, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(log_file, "The log file name");

static int write_log_file(unsigned char *data)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int fd = 0, len = 0;
	loff_t pos = 0;
	struct path k_path;
	struct kstat stat;

	if (!log_file) {
		pr_err("didn't set log file path\n");
		return 0;
	}

	if (kern_path(log_file, 0, &k_path) == 0 &&
			vfs_getattr(&k_path, &stat) == 0) {
		pos = stat.size;
	}

	oldfs = get_fs();
	set_fs(get_ds());
	my_sys_open = (void *)sys_call_table[__NR_open];
	my_sys_close = (void *)sys_call_table[__NR_close];
	fd = my_sys_open(log_file, O_RDWR|O_CREAT, 0654);
	if (fd < 0) {
		pr_err("open file failed\n");
		set_fs(oldfs);
		return 0;
	}
	filp = fget(fd);
	filp->f_pos = pos;
	len = vfs_write(filp, data, strlen(data), &pos);
	fput(filp);
	if (len == 0)
		pr_err("writing file failed\n");
	else
		vfs_fsync(filp, 0);

	set_fs(oldfs);
	my_sys_close(fd);

	return len;
}

int power_off_notifier_handler(struct notifier_block *nb,
			unsigned long action, void *msg)
{

	struct power_supply *battery;

	if (!log_file) {
		pr_err("didn't get log file name, no save\n");
		goto exit;
	}

	battery = power_supply_get_by_name("battery");
	if (battery) {
		union power_supply_propval val;
		int v = 0, len = 0, c = 0;
		unsigned long t = 0;
		char buf[112] = {0};
		struct timeval time;
		unsigned long local_time;
		struct rtc_time tm;
		const char *message = "No msg";

		if (battery->get_property(battery,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val) == 0)
			v = val.intval;

		if (battery->get_property(battery,
				POWER_SUPPLY_PROP_CAPACITY, &val) == 0)
			c = val.intval;
#ifdef CONFIG_THERMAL
		if (battery->tzd) {
			struct power_supply *thermal = battery->tzd->devdata;
			if (thermal->get_property(thermal,
					POWER_SUPPLY_PROP_TEMP, &val) == 0)
				t = val.intval * 100;
		}
#endif
		if (msg)
			message = msg;

		do_gettimeofday(&time);
		local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
		rtc_time_to_tm(local_time, &tm);
		snprintf(buf, 112,
			"%04d-%02d-%02d %02d:%02d:%02d - %s: v=%d, c=%d%%, t=%lu\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, message, v, c, t);

		len = write_log_file(buf);
		if (!len)
			dev_err(battery->dev, "writing log failed\n");
	}
exit:
	return NOTIFY_DONE;
}

static int __init power_supply_log_init(void)
{
	power_notifier.notifier_call = power_off_notifier_handler;
	power_notifier.next = NULL;
	power_notifier.priority = 0;

	if (register_fs_notifier(&power_notifier)) {
		pr_err("power_supply_core failed to register reboot\n");
		power_notifier.notifier_call = NULL;
	}
	return 0;
}

static void __exit power_supply_log_exit(void)
{
	if (power_notifier.notifier_call)
		unregister_fs_notifier(&power_notifier);
}

module_init(power_supply_log_init);
module_exit(power_supply_log_exit);

MODULE_DESCRIPTION("Power supply log");
MODULE_LICENSE("GPL");
