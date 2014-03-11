/*
 * Copyright (c) 2013, Motorola Mobility, LLC.
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

#include <asm/mach-types.h>
#include <asm/page.h>
#include <linux/export.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

#define DROPBOX_TEXT_FLAG   0x00000002
#define DROPBOX_FILE_FLAG   0x40000000

#define EVENT_QUEUE_SIZE 5
#define EVENT_NAME_LENGTH 64

static DEFINE_MUTEX(dropbox_queue_lock);
static struct kobject dropbox_kobj;

struct dropbox_event {
	char name[EVENT_NAME_LENGTH];
	void *data;
	size_t size;
	int flags;
	struct list_head list;
};

static LIST_HEAD(dropbox_event_list);

static int dropbox_event_queue_depth_locked(void)
{
	struct dropbox_event *event = NULL;
	int queue_depth = 0;
	list_for_each_entry(event, &dropbox_event_list, list)
		queue_depth++;
	return queue_depth;
}

static bool dropbox_event_queue_full_locked(void)
{
	return dropbox_event_queue_depth_locked() >= EVENT_QUEUE_SIZE;
}

static bool dropbox_event_queue_empty_locked(void)
{
	return dropbox_event_queue_depth_locked() == 0;
}

static void dropbox_notify_if_events(void)
{
	if (mutex_lock_interruptible(&dropbox_queue_lock))
		sysfs_notify(&dropbox_kobj, NULL, "event");
	else {
		if (!dropbox_event_queue_empty_locked())
			sysfs_notify(&dropbox_kobj, NULL, "event");
		mutex_unlock(&dropbox_queue_lock);
	}
}

static void dropbox_free_event(struct dropbox_event *event)
{
	if (event) {
		if (event->data) {
			vfree(event->data);
			event->data = NULL;
		}
		vfree(event);
	}
}

static struct dropbox_event *dropbox_dequeue_event_locked(void)
{
	struct dropbox_event *event = NULL;

	if (!list_empty(&dropbox_event_list)) {
		event = list_first_entry(&dropbox_event_list,
			struct dropbox_event, list);
		list_del(&event->list);
	}

	return event;
}

static struct dropbox_event *dropbox_dequeue_event(void)
{
	struct dropbox_event *event = NULL;

	mutex_lock(&dropbox_queue_lock);
	event = dropbox_dequeue_event_locked();
	mutex_unlock(&dropbox_queue_lock);

	return event;
}

static void dropbox_queue_event(char *name, void *data, size_t size, int flags)
{
	struct dropbox_event *new_event = NULL;

	if (!name || strlen(name) <= 0 || strlen(name) > EVENT_NAME_LENGTH) {
		pr_err("%s: invalid name\n", __func__);
		return;
	}

	if (!data || !size) {
		pr_err("%s: invalid arguments [%s][%p][%d]\n", __func__, name,
			data, size);
		return;
	}

	mutex_lock(&dropbox_queue_lock);

	if (dropbox_event_queue_full_locked()) {
		struct dropbox_event *cur = NULL;
		/* drop oldest event */
		cur = dropbox_dequeue_event_locked();
		if (cur) {
			pr_warn("%s: Dropping oldest event [%s]",
				__func__, cur->name);
			dropbox_free_event(cur);
		}
	}

	new_event = vzalloc(sizeof(struct dropbox_event));
	if (new_event) {
		scnprintf(new_event->name,
			sizeof(new_event->name), "%s", name);
		new_event->data = vmalloc(size);
		if (new_event->data) {
			memcpy(new_event->data, data, size);
			new_event->size = size;
			new_event->flags = flags;

			list_add_tail(&new_event->list, &dropbox_event_list);
			sysfs_notify(&dropbox_kobj, NULL, "event");
		} else {
			pr_err("%s: Failed to allocate new event data",
				__func__);
			dropbox_free_event(new_event);
		}
	} else {
		pr_err("%s: Failed to allocate new event", __func__);
	}

	mutex_unlock(&dropbox_queue_lock);
}

void dropbox_queue_event_binary(char *name, void *data, size_t size)
{
	dropbox_queue_event(name, data, size, 0);
}

void dropbox_queue_event_text(char *name, void *data, size_t size)
{
	dropbox_queue_event(name, data, size, DROPBOX_TEXT_FLAG);
}

void dropbox_queue_event_binaryfile(char *name, char *path)
{
	dropbox_queue_event(name, path, strlen(path), DROPBOX_FILE_FLAG);
}

void dropbox_queue_event_textfile(char *name, char *path)
{
	dropbox_queue_event(name, path, strlen(path),
		DROPBOX_TEXT_FLAG | DROPBOX_FILE_FLAG);
}

void dropbox_queue_event_empty(char *name)
{
	dropbox_queue_event(name, name, strlen(name), DROPBOX_TEXT_FLAG);
}

struct dropbox_trigger_callback {
	char name[EVENT_NAME_LENGTH];
	void (*callback)(void *);
	void *data;
	struct list_head list;
};

static LIST_HEAD(dropbox_trigger_callback_list);

void dropbox_register_trigger_callback(const char *name,
				       void (*callback)(void *),
				       void *data)
{
	struct dropbox_trigger_callback *cur = NULL;

	if (!name || strlen(name) <= 0 || strlen(name) > EVENT_NAME_LENGTH) {
		pr_err("%s: invalid name\n", __func__);
		return;
	}

	list_for_each_entry(cur, &dropbox_trigger_callback_list, list) {
		if (!strncmp(cur->name, name, sizeof(cur->name))) {
			pr_err("%s: Event already registered with name:"
				" %s\n", __func__, name);
			return;
		}
	}

	cur = vzalloc(sizeof(struct dropbox_trigger_callback));
	if (cur) {
		scnprintf(cur->name, sizeof(cur->name), "%s", name);
		cur->callback = callback;
		cur->data = data;

		list_add_tail(&cur->list, &dropbox_trigger_callback_list);
	} else {
		pr_err("%s: Failed to allocate new trigger callback", __func__);
	}
}

static void do_dropbox_trigger_callback(const char *name)
{
	struct dropbox_trigger_callback *cur = NULL;

	if (!name || strlen(name) <= 0 || strlen(name) > EVENT_NAME_LENGTH) {
		pr_err("%s: invalid name\n", __func__);
		return;
	}

	list_for_each_entry(cur, &dropbox_trigger_callback_list, list) {
		if (!strncmp(cur->name, name, sizeof(cur->name))) {
			cur->callback(cur->data);
			return;
		}
	}

	pr_err("%s: No callback found for event [%s]", __func__, name);
}


/* sysfs entry code */


struct dropbox_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, char *buf);
	ssize_t (*store)(struct kobject *kobj, const char *buf,
		size_t count);
};

#define to_dropbox_attr(a) \
container_of(a, struct dropbox_attribute, attr)

static struct dropbox_event *dropbox_current_event;

/* Dump the sysfs binary data to the user */
static ssize_t dropbox_data_show(struct file *filep, struct kobject *kobj,
				 struct bin_attribute *attr, char *buf,
				 loff_t off, size_t count)
{
	int size = 0;

	if (!dropbox_current_event)
		dropbox_current_event = dropbox_dequeue_event();

	if (!dropbox_current_event) {
		pr_err("%s: no dropbox events", __func__);
		goto end;
	}

	if (dropbox_current_event->data) {
		void *src = dropbox_current_event->data + off;
		if (dropbox_current_event->size > off) {
			size = dropbox_current_event->size - off;
			size = size > count ? count : size;
			memcpy(buf, src, size);
		}
	} else
		pr_err("%s: dropbox_data is empty\n", __func__);

	if (!size) {
		dropbox_free_event(dropbox_current_event);
		dropbox_current_event = NULL;
		dropbox_notify_if_events();
	}

end:
	return size;
}

static ssize_t dropbox_event_show(struct kobject *kobj, char *buf)
{
	if (!list_empty(&dropbox_event_list)) {
		struct dropbox_event *event = list_first_entry(
			&dropbox_event_list, struct dropbox_event, list);
		return scnprintf(buf, PAGE_SIZE, "%s:%d\n", event->name,
			event->flags);
	} else
		return 0;
}

static ssize_t dropbox_trigger_store(struct kobject *kobj, const char *buf,
				     size_t count)
{
	unsigned int ret = -EINVAL;
	char event_name[EVENT_NAME_LENGTH];

	if (count <= 0 || count > EVENT_NAME_LENGTH) {
		pr_err("%s: invalid string, max length is 64 characters\n",
			__func__);
	} else {
		ret = sscanf(buf, "%63s", event_name);
		if (ret != 1)
			return -EINVAL;
		pr_info("%s: forcing event [%s]\n", __func__, event_name);
		do_dropbox_trigger_callback(event_name);
	}

	return count;
}

static ssize_t dropbox_available_triggers_show(struct kobject *kobj, char *buf)
{
	char *cur = buf;
	struct dropbox_trigger_callback *cb_cur = NULL;

	list_for_each_entry(cb_cur, &dropbox_trigger_callback_list, list) {
		int count = scnprintf(cur, PAGE_SIZE-(cur-buf), "%s\n",
			cb_cur->name);
		cur = cur + count;
	}

	return cur-buf;
}

static struct bin_attribute attr_data = {
	.attr.name = "data",
	.attr.mode = 0440,
	.size = 0,
	.read = dropbox_data_show
};

#define DROPBOX_ATTR(_name, _mode, _show, _store) \
static struct dropbox_attribute attr_##_name = { \
	.attr = { .name = __stringify(_name), .mode = _mode }, \
	.show = _show, \
	.store = _store, \
}

DROPBOX_ATTR(event, 0440, dropbox_event_show, NULL);
DROPBOX_ATTR(trigger, 0200, NULL, dropbox_trigger_store);
DROPBOX_ATTR(available_triggers, 0400, dropbox_available_triggers_show, NULL);

static void dropbox_sysfs_release(struct kobject *kobj)
{
}

static ssize_t dropbox_sysfs_show(struct kobject *kobj,
				  struct attribute *attr, char *buf)
{
	struct dropbox_attribute *pattr = to_dropbox_attr(attr);
	ssize_t ret = -EIO;

	if (kobj && pattr->show)
		ret = pattr->show(kobj, buf);

	return ret;
}

static ssize_t dropbox_sysfs_store(struct kobject *kobj,
				   struct attribute *attr,
				   const char *buf, size_t count)
{
	struct dropbox_attribute *pattr = to_dropbox_attr(attr);
	ssize_t ret = -EIO;

	if (kobj && pattr->store)
		ret = pattr->store(kobj, buf, count);

	return ret;
}

static const struct sysfs_ops dropbox_sysfs_ops = {
	.show = dropbox_sysfs_show,
	.store = dropbox_sysfs_store,
};

static struct kobj_type ktype_dropbox = {
	.sysfs_ops = &dropbox_sysfs_ops,
	.default_attrs = NULL,
	.release = dropbox_sysfs_release,
};

int __init dropbox_init(void)
{
	int ret = 0;

	ret = kobject_init_and_add(&dropbox_kobj, &ktype_dropbox,
		kernel_kobj, "dropbox");
	if (ret)
		goto done;

	ret = sysfs_create_file(&dropbox_kobj, &attr_event.attr);
	if (ret)
		goto done;

	ret = sysfs_create_file(&dropbox_kobj, &attr_trigger.attr);
	if (ret)
		goto done;

	ret = sysfs_create_file(&dropbox_kobj, &attr_available_triggers.attr);
	if (ret)
		goto done;

	ret = sysfs_create_bin_file(&dropbox_kobj, &attr_data);
	if (ret)
		goto done;

done:
	return ret;
}

core_initcall(dropbox_init);
