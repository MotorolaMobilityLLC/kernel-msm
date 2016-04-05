/*
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/irq_work.h>

#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>

struct iio_sysfs_trig {
	struct iio_trigger *trig;
	struct irq_work work;
	struct delayed_work poll_work;
	unsigned int poll_interval; /* msec */
	int id;
	struct list_head l;
};

static LIST_HEAD(iio_sysfs_trig_list);
static DEFINE_MUTEX(iio_sysfs_trig_list_mut);

static int iio_sysfs_trigger_probe(int id);
static ssize_t iio_sysfs_trig_add(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t len)
{
	int ret;
	unsigned long input;

	ret = kstrtoul(buf, 10, &input);
	if (ret)
		return ret;
	ret = iio_sysfs_trigger_probe(input);
	if (ret)
		return ret;
	return len;
}
static DEVICE_ATTR(add_trigger, S_IWUSR, NULL, &iio_sysfs_trig_add);

static int iio_sysfs_trigger_remove(int id);
static ssize_t iio_sysfs_trig_remove(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t len)
{
	int ret;
	unsigned long input;

	ret = kstrtoul(buf, 10, &input);
	if (ret)
		return ret;
	ret = iio_sysfs_trigger_remove(input);
	if (ret)
		return ret;
	return len;
}

static DEVICE_ATTR(remove_trigger, S_IWUSR, NULL, &iio_sysfs_trig_remove);

static struct attribute *iio_sysfs_trig_attrs[] = {
	&dev_attr_add_trigger.attr,
	&dev_attr_remove_trigger.attr,
	NULL,
};

static const struct attribute_group iio_sysfs_trig_group = {
	.attrs = iio_sysfs_trig_attrs,
};

static const struct attribute_group *iio_sysfs_trig_groups[] = {
	&iio_sysfs_trig_group,
	NULL
};


/* Nothing to actually do upon release */
static void iio_trigger_sysfs_release(struct device *dev)
{
}

static struct device iio_sysfs_trig_dev = {
	.bus = &iio_bus_type,
	.groups = iio_sysfs_trig_groups,
	.release = &iio_trigger_sysfs_release,
};

static void iio_sysfs_trigger_work(struct irq_work *work)
{
	struct iio_sysfs_trig *trig = container_of(work, struct iio_sysfs_trig,
							work);

	iio_trigger_poll(trig->trig);
}

static ssize_t iio_sysfs_trigger_poll(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct iio_sysfs_trig *sysfs_trig = iio_trigger_get_drvdata(trig);

	irq_work_queue(&sysfs_trig->work);

	return count;
}

static void iio_sysfs_trigger_queue_poll_work(struct iio_sysfs_trig *trig)
{
	unsigned long delay;

	delay = msecs_to_jiffies(trig->poll_interval);
	if (delay >= HZ)
		delay = round_jiffies_relative(delay);

	queue_delayed_work(system_freezable_wq, &trig->poll_work, delay);
}

static void iio_sysfs_trigger_poll_work(struct work_struct *work)
{
	struct iio_sysfs_trig *trig = container_of(work, struct iio_sysfs_trig,
						   poll_work.work);

	irq_work_queue(&trig->work);
	iio_sysfs_trigger_queue_poll_work(trig);
}

static ssize_t iio_sysfs_trigger_get_poll(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct iio_sysfs_trig *sysfs_trig = iio_trigger_get_drvdata(trig);

	return snprintf(buf, PAGE_SIZE, "%d\n", sysfs_trig->poll_interval);
}

static ssize_t iio_sysfs_trigger_set_poll(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct iio_sysfs_trig *sysfs_trig = iio_trigger_get_drvdata(trig);
	unsigned int interval;
	int err;

	err = kstrtouint(buf, 0, &interval);
	if (err)
		return err;

	sysfs_trig->poll_interval = interval;

	cancel_delayed_work_sync(&sysfs_trig->poll_work);
	if (sysfs_trig->poll_interval > 0)
		iio_sysfs_trigger_queue_poll_work(sysfs_trig);

	return count;
}

static DEVICE_ATTR(trigger_now, S_IWUSR, NULL, iio_sysfs_trigger_poll);
static DEVICE_ATTR(trigger_poll, S_IRUGO | S_IWUSR, iio_sysfs_trigger_get_poll,
						    iio_sysfs_trigger_set_poll);

static struct attribute *iio_sysfs_trigger_attrs[] = {
	&dev_attr_trigger_now.attr,
	&dev_attr_trigger_poll.attr,
	NULL,
};

static const struct attribute_group iio_sysfs_trigger_attr_group = {
	.attrs = iio_sysfs_trigger_attrs,
};

static const struct attribute_group *iio_sysfs_trigger_attr_groups[] = {
	&iio_sysfs_trigger_attr_group,
	NULL
};

static const struct iio_trigger_ops iio_sysfs_trigger_ops = {
	.owner = THIS_MODULE,
};

static int iio_sysfs_trigger_probe(int id)
{
	struct iio_sysfs_trig *t;
	int ret;
	bool foundit = false;
	mutex_lock(&iio_sysfs_trig_list_mut);
	list_for_each_entry(t, &iio_sysfs_trig_list, l)
		if (id == t->id) {
			foundit = true;
			break;
		}
	if (foundit) {
		ret = -EINVAL;
		goto out1;
	}
	t = kmalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL) {
		ret = -ENOMEM;
		goto out1;
	}
	t->id = id;
	t->trig = iio_trigger_alloc("sysfstrig%d", id);
	if (!t->trig) {
		ret = -ENOMEM;
		goto free_t;
	}

	t->trig->dev.groups = iio_sysfs_trigger_attr_groups;
	t->trig->ops = &iio_sysfs_trigger_ops;
	t->trig->dev.parent = &iio_sysfs_trig_dev;
	iio_trigger_set_drvdata(t->trig, t);

	init_irq_work(&t->work, iio_sysfs_trigger_work);
	INIT_DELAYED_WORK(&t->poll_work, iio_sysfs_trigger_poll_work);

	ret = iio_trigger_register(t->trig);
	if (ret)
		goto out2;
	list_add(&t->l, &iio_sysfs_trig_list);
	__module_get(THIS_MODULE);
	mutex_unlock(&iio_sysfs_trig_list_mut);
	return 0;

out2:
	iio_trigger_put(t->trig);
free_t:
	kfree(t);
out1:
	mutex_unlock(&iio_sysfs_trig_list_mut);
	return ret;
}

static int iio_sysfs_trigger_remove(int id)
{
	bool foundit = false;
	struct iio_sysfs_trig *t;
	mutex_lock(&iio_sysfs_trig_list_mut);
	list_for_each_entry(t, &iio_sysfs_trig_list, l)
		if (id == t->id) {
			foundit = true;
			break;
		}
	if (!foundit) {
		mutex_unlock(&iio_sysfs_trig_list_mut);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&t->poll_work);

	iio_trigger_unregister(t->trig);
	iio_trigger_free(t->trig);

	list_del(&t->l);
	kfree(t);
	module_put(THIS_MODULE);
	mutex_unlock(&iio_sysfs_trig_list_mut);
	return 0;
}


static int __init iio_sysfs_trig_init(void)
{
	device_initialize(&iio_sysfs_trig_dev);
	dev_set_name(&iio_sysfs_trig_dev, "iio_sysfs_trigger");
	return device_add(&iio_sysfs_trig_dev);
}
module_init(iio_sysfs_trig_init);

static void __exit iio_sysfs_trig_exit(void)
{
	device_unregister(&iio_sysfs_trig_dev);
}
module_exit(iio_sysfs_trig_exit);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("Sysfs based trigger for the iio subsystem");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:iio-trig-sysfs");
