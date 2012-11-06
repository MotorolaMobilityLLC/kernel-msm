/*
 * drivers/misc/vib-timed.c
 *
 * Copyright (C) 2011 Motorola Mobility Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/vib-timed.h>

#include <linux/rtc.h>
#include "../staging/android/timed_output.h"

struct vib_timed_data {
	struct timed_output_dev dev;
	struct work_struct work;
	struct hrtimer timer;
	struct mutex lock;	/* only one vibration at a time */
	struct vib_timed *vib;
	int id;
};

int vib_timed_debug = 0;

static struct vib_timed *vibs;
static int vib_count;

static DEFINE_SPINLOCK(vib_logbuf_lock);
static char vib_log_buf[4096];
static char vib_print_buf[256];
static char *vib_log_tail = vib_log_buf;

int vib_print(const char *fmt, ...)
{
	va_list args;
	unsigned long flags;
	int len = 0;

	spin_lock_irqsave(&vib_logbuf_lock, flags);
	va_start(args, fmt);
	len += vsnprintf(vib_print_buf + len, sizeof(vib_print_buf) - len,
		fmt, args);
	va_end(args);

	if (vib_log_tail + len > vib_log_buf + sizeof(vib_log_buf) - 1) {
		int part = vib_log_buf + sizeof(vib_log_buf)
				 - vib_log_tail - 1;
		memcpy(vib_log_tail, vib_print_buf, part);
		memcpy(vib_log_buf, vib_print_buf + part, len - part);
		vib_log_tail = vib_log_buf + len - part;
	} else {
		memcpy(vib_log_tail, vib_print_buf, len);
		vib_log_tail += len;
	}
	*vib_log_tail = '\0';
	spin_unlock_irqrestore(&vib_logbuf_lock, flags);
	return len;
}

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_vib_timed;
#endif

static enum hrtimer_restart vib_timed_timer_func(struct hrtimer *timer)
{
	struct vib_timed_data *data = container_of(timer,
					struct vib_timed_data, timer);
	schedule_work(&data->work);
	return HRTIMER_NORESTART;
}

static void vib_timed_off(struct vib_timed_data *data)
{
	if (data->vib->power_off)
		data->vib->power_off(data->vib);
	dvib_tprint("e%d\n", data->id);
}

static void vib_timed_work(struct work_struct *work)
{
	struct vib_timed_data *data = container_of(work,
					struct vib_timed_data, work);
	mutex_lock(&data->lock);
	vib_timed_off(data);
	mutex_unlock(&data->lock);
}

static int vib_timed_get_time(struct timed_output_dev *dev)
{
	struct vib_timed_data *data = container_of(dev,
					struct vib_timed_data, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void vib_timed_enable(struct timed_output_dev *dev, int value)
{
	struct vib_timed_data *data = container_of(dev,
					struct vib_timed_data, dev);
	hrtimer_cancel(&data->timer);
	cancel_work_sync(&data->work);

	mutex_lock(&data->lock);
	dvib_tprint("r%d %d\n", data->id, value);
	if (value) {
		if (value > 0) {
			if (value < data->vib->min_timeout)
				value = data->vib->min_timeout;
			if (value > data->vib->max_timeout)
				value = data->vib->max_timeout;

			if (data->vib->power_on)
				value = data->vib->power_on(value, data->vib);
			dvib_tprint("s%d %d\n", data->id, value);
			if (value > 0)
				hrtimer_start(&data->timer,
					ns_to_ktime((u64)value * NSEC_PER_MSEC),
					HRTIMER_MODE_REL);
			else
				vib_timed_off(data);

		}
	} else
		vib_timed_off(data);

	mutex_unlock(&data->lock);
}

static struct vib_timed_data *vib_timed_alloc(struct vib_timed *vib)
{
	struct vib_timed_data *data;
	int ret;

	data = kzalloc(sizeof(struct vib_timed_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err0;
	}

	data->vib = vib;

	INIT_WORK(&data->work, vib_timed_work);

	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	data->timer.function = vib_timed_timer_func;
	mutex_init(&data->lock);

	data->dev.name = vib->name;
	data->dev.get_time = vib_timed_get_time;
	data->dev.enable = vib_timed_enable;
	ret = timed_output_dev_register(&data->dev);
	if (ret < 0)
		goto err1;

	if (data->vib->init)
		ret = data->vib->init(vib);
	if (ret < 0)
		goto err2;

	return data;
err2:
	timed_output_dev_unregister(&data->dev);
err1:
	kfree(data);
err0:
	return NULL;
}

#ifdef CONFIG_PROC_FS
static int proc_vib_timed_show(struct seq_file *m, void *v)
{
	unsigned long flags;
	struct timespec ts;
	struct rtc_time tm;
	struct vib_timed_data *data;
	int i;

	getnstimeofday(&ts); \
	rtc_time_to_tm(ts.tv_sec, &tm);
	seq_printf(m, "%lld %d-%02d-%02d %02d:%02d:%02d.%09lu\n",
		ktime_to_us(ktime_get()), tm.tm_year + 1900,
		tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
		tm.tm_sec, ts.tv_nsec);

	for (i = 0; i < vib_count; i++) {
		data = vibs[i].drv_data;
		data->vib->dump(data->vib);
	}

	spin_lock_irqsave(&vib_logbuf_lock, flags);
	vib_log_buf[sizeof(vib_log_buf) - 1] = '\0';
	if (((vib_log_tail + 1) < (vib_log_buf + sizeof(vib_log_buf)))
		&& *(vib_log_tail + 1))
		seq_printf(m, "%s", vib_log_tail + 1);
	seq_printf(m, "%s", vib_log_buf);
	spin_unlock_irqrestore(&vib_logbuf_lock, flags);
	return 0;
}

static int proc_vib_timed_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_vib_timed_show, NULL);
}

static const struct file_operations proc_vib_timed_ops = {
	.open           = proc_vib_timed_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
#endif

static int vib_timed_probe(struct platform_device *pdev)
{
	struct vib_timed_platform_data *pdata = pdev->dev.platform_data;
	struct vib_timed_data *data;
	int ret = 0;
	int i;

	if (!pdata) {
		ret = -ENODEV;
		goto err0;
	}

	if (!pdata->count) {
		ret = -EINVAL;
		goto err0;
	}

	vibs = pdata->vibs;
	vib_count = pdata->count;

	for (i = 0; i < vib_count; i++) {
		data = vib_timed_alloc(&vibs[i]);
		if (data) {
			data->id = i;
			vibs[i].drv_data = data;
		}
	}
#ifdef CONFIG_PROC_FS
	proc_vib_timed = proc_create(VIB_TIMED_NAME, S_IFREG | S_IRUGO,
					NULL, &proc_vib_timed_ops);
	if (!proc_vib_timed)
		printk(KERN_ERR "%s: failed creating procfile\n", __func__);
#endif

	return 0;

err0:
	return ret;
}

static int vib_timed_remove(struct platform_device *pdev)
{
	struct vib_timed_data *data;
	int i;

	for (i = vib_count; i > 0; i--) {
		data = vibs[i].drv_data;
		if (data && data->vib->exit) {
			data->vib->exit(data->vib);
			timed_output_dev_unregister(&data->dev);
			kfree(data);
		}
	}
#ifdef CONFIG_PROC_FS
	if (proc_vib_timed)
		remove_proc_entry(VIB_TIMED_NAME, NULL);
#endif

	return 0;
}


static struct platform_driver vib_timed_driver = {
	.probe = vib_timed_probe,
	.remove = vib_timed_remove,
	.driver = {
		.name = VIB_TIMED_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init vib_timed_init(void)
{
	return platform_driver_register(&vib_timed_driver);
}

static void __exit vib_timed_exit(void)
{
	platform_driver_unregister(&vib_timed_driver);
}

module_init(vib_timed_init);
module_exit(vib_timed_exit);
module_param_named(debug, vib_timed_debug, int, 0644);

MODULE_AUTHOR("Motorola Mobility Inc.");
MODULE_DESCRIPTION("Timed-output Vibrator Driver");
MODULE_LICENSE("GPL");
