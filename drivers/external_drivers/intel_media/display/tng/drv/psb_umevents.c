/*
 * Copyright © 2009 Intel Corporation
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
 * Authors:
 *    James C. Gualario <james.c.gualario@intel.com>
 *
 */
#include "psb_umevents.h"
/**
 * define sysfs operations supported by umevent objects.
 *
 */
static const struct sysfs_ops umevent_obj_sysfs_ops = {
	.show = psb_umevent_attr_show,
	.store = psb_umevent_attr_store,
};

/**
 * define the data attributes we will expose through sysfs.
 *
 */
static struct umevent_attribute data_0 =
__ATTR(data_0_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_1 =
__ATTR(data_1_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_2 =
__ATTR(data_2_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_3 =
__ATTR(data_3_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_4 =
__ATTR(data_4_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_5 =
__ATTR(data_5_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_6 =
__ATTR(data_6_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
static struct umevent_attribute data_7 =
__ATTR(data_7_val, 0664, psb_umevent_attr_show_imp,
       psb_umevent_attr_store_imp);
/**
 * define the structure used to seed our ktype.
 *
 */
static struct attribute *umevent_obj_default_attrs[] = {
	&data_0.attr,
	&data_1.attr,
	&data_2.attr,
	&data_3.attr,
	&data_4.attr,
	&data_5.attr,
	&data_6.attr,
	&data_7.attr,
	NULL,			/* need to NULL terminate the list of attributes */
};

/**
 * specify the ktype for our kobjects.
 *
 */
static struct kobj_type umevent_obj_ktype = {
	.sysfs_ops = &umevent_obj_sysfs_ops,
	.release = psb_umevent_obj_release,
	.default_attrs = umevent_obj_default_attrs,
};

/**
 * psb_umevent_attr_show - default kobject show function
 *
 * @kobj: kobject associated with the show operation
 * @attr: attribute being requested
 * @buf: pointer to the return buffer
 *
 */
ssize_t psb_umevent_attr_show(struct kobject *kobj,
			      struct attribute *attr, char *buf)
{
	struct umevent_attribute *attribute;
	struct umevent_obj *any_umevent_obj;
	attribute = to_umevent_attr(attr);
	any_umevent_obj = to_umevent_obj(kobj);
	if (!attribute->show)
		return -EIO;

	return attribute->show(any_umevent_obj, attribute, buf);
}

/**
 * psb_umevent_attr_store - default kobject store function
 *
 * @kobj: kobject associated with the store operation
 * @attr: attribute being requested
 * @buf: input data to write to attribute
 * @len: character count
 *
 */
ssize_t psb_umevent_attr_store(struct kobject * kobj,
			       struct attribute * attr,
			       const char *buf, size_t len)
{
	struct umevent_attribute *attribute;
	struct umevent_obj *any_umevent_obj;
	attribute = to_umevent_attr(attr);
	any_umevent_obj = to_umevent_obj(kobj);
	if (!attribute->store)
		return -EIO;

	return attribute->store(any_umevent_obj, attribute, buf, len);
}

/**
 * psb_umevent_obj_release - kobject release funtion
 *
 * @kobj: kobject to be released.
 */
void psb_umevent_obj_release(struct kobject *kobj)
{
	struct umevent_obj *any_umevent_obj;
	any_umevent_obj = to_umevent_obj(kobj);
	kfree(any_umevent_obj);
}

/**
 *  psb_umevent_attr_show_imp - attribute show implementation
 *
 * @any_umevent_obj: kobject managed data to read from
 * @attr: attribute being requested
 * @buf: pointer to the return buffer
 *
 */
ssize_t psb_umevent_attr_show_imp(struct umevent_obj
				  *any_umevent_obj,
				  struct umevent_attribute *attr, char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "data_0_val") == 0)
		var = any_umevent_obj->data_0_val;
	else if (strcmp(attr->attr.name, "data_1_val") == 0)
		var = any_umevent_obj->data_1_val;
	else if (strcmp(attr->attr.name, "data_2_val") == 0)
		var = any_umevent_obj->data_2_val;
	else if (strcmp(attr->attr.name, "data_3_val") == 0)
		var = any_umevent_obj->data_3_val;
	else if (strcmp(attr->attr.name, "data_4_val") == 0)
		var = any_umevent_obj->data_4_val;
	else if (strcmp(attr->attr.name, "data_5_val") == 0)
		var = any_umevent_obj->data_5_val;
	else if (strcmp(attr->attr.name, "data_6_val") == 0)
		var = any_umevent_obj->data_6_val;
	else
		var = any_umevent_obj->data_7_val;

	return sprintf(buf, "%d\n", var);
}

/**
 * psb_umevent_attr_store_imp - attribute store implementation
 *
 * @any_umevent_obj: kobject managed data to write to
 * @attr: attribute being requested
 * @buf: input data to write to attribute
 * @count: character count
 *
 */
ssize_t psb_umevent_attr_store_imp(struct umevent_obj
				   * any_umevent_obj,
				   struct umevent_attribute * attr,
				   const char *buf, size_t count)
{
	int var;

	sscanf(buf, "%du", &var);
	if (strcmp(attr->attr.name, "data_0_val") == 0)
		any_umevent_obj->data_0_val = var;
	else if (strcmp(attr->attr.name, "data_1_val") == 0)
		any_umevent_obj->data_1_val = var;
	else if (strcmp(attr->attr.name, "data_2_val") == 0)
		any_umevent_obj->data_2_val = var;
	else if (strcmp(attr->attr.name, "data_3_val") == 0)
		any_umevent_obj->data_3_val = var;
	else if (strcmp(attr->attr.name, "data_4_val") == 0)
		any_umevent_obj->data_4_val = var;
	else if (strcmp(attr->attr.name, "data_5_val") == 0)
		any_umevent_obj->data_5_val = var;
	else if (strcmp(attr->attr.name, "data_6_val") == 0)
		any_umevent_obj->data_6_val = var;
	else
		any_umevent_obj->data_7_val = var;
	return count;
}

/**
 * psb_create_umevent_obj - create and track new event objects
 *
 * @name: name to give to new sysfs / kobject entry
 * @list: event object list to track the kobject in
 */
struct umevent_obj *psb_create_umevent_obj(const char *name, struct umevent_list
					   *list)
{
	struct umevent_obj *new_umevent_obj;
	int retval;
	new_umevent_obj = kzalloc(sizeof(*new_umevent_obj), GFP_KERNEL);
	if (!new_umevent_obj)
		return NULL;

	new_umevent_obj->kobj.kset = list->umevent_disp_pool;
	retval = kobject_init_and_add(&new_umevent_obj->kobj,
				      &umevent_obj_ktype, NULL, "%s", name);
	if (retval) {
		kobject_put(&new_umevent_obj->kobj);
		return NULL;
	}
	psb_umevent_add_to_list(list, new_umevent_obj);
	return new_umevent_obj;
}

/*EXPORT_SYMBOL(psb_create_umevent_obj); */
/**
 * psb_umevent_notify - info user mode of a new device
 *
 * @notify_disp_obj: event object to perform notification for
 *
 */
void psb_umevent_notify(struct umevent_obj *notify_disp_obj)
{
	kobject_uevent(&notify_disp_obj->kobj, KOBJ_ADD);
}

/*EXPORT_SYMBOL(psb_umevent_notify); */

void psb_umevent_notify_gfxsock(struct umevent_obj *notify_disp_obj,
				int dst_group_id)
{
	psb_kobject_uevent(&notify_disp_obj->kobj, KOBJ_ADD, dst_group_id);
}

/*EXPORT_SYMBOL(psb_umevent_notify_gfxsock);*/
/**
 * psb_umevent_notify_change - notify user mode of a change to a device
 *
 * @notify_disp_obj: event object to perform notification for
 *
 */
void psb_umevent_notify_change(struct umevent_obj *notify_disp_obj)
{
	kobject_uevent(&notify_disp_obj->kobj, KOBJ_CHANGE);
}

/*EXPORT_SYMBOL(psb_umevent_notify_change); */
/**
 * psb_umevent_notify_change - notify user mode of a change to a device
 *
 * @notify_disp_obj: event object to perform notification for
 *
 */
void psb_umevent_notify_change_gfxsock(struct umevent_obj *notify_disp_obj,
				       int dst_group_id)
{
	psb_kobject_uevent(&notify_disp_obj->kobj, KOBJ_CHANGE, dst_group_id);
}

/*EXPORT_SYMBOL(psb_umevent_notify_change_gfxsock); */
/**
 * psb_destroy_umvent_obj - decrement ref count on event so kernel can kill it
 *
 * @any_umevent_obj: event object to destroy
 *
 */
void psb_destroy_umevent_obj(struct umevent_obj
			     *any_umevent_obj)
{
	kobject_put(&any_umevent_obj->kobj);
}

/**
 *
 * psb_umevent_init - init the event pool
 *
 * @parent_kobj: parent kobject to associate new kset with
 * @new_umevent_list: event list to associate kset with
 * @name: name to give to new sysfs entry
 *
 */
int psb_umevent_init(struct kobject *parent_kobj,
		     struct umevent_list *new_umevent_list, const char *name)
{
	psb_umevent_init_list(new_umevent_list);
	new_umevent_list->umevent_disp_pool = kset_create_and_add(name, NULL,
								  parent_kobj);
	if (!new_umevent_list->umevent_disp_pool)
		return -ENOMEM;

	return 0;
}

/*EXPORT_SYMBOL(psb_umevent_init); */
/**
 *
 * psb_umevent_cleanup - cleanup all event objects
 *
 * @kill_list: list of events to destroy
 *
 */
void psb_umevent_cleanup(struct umevent_list *kill_list)
{
	psb_umevent_destroy_list(kill_list);
}

/*EXPORT_SYMBOL(psb_umevent_cleanup); */
/**
 * psb_umevent_add_to_list - add an event to the event list
 *
 * @list: list to add the event to
 * @umevent_obj_to_add: event to add
 *
 */
void psb_umevent_add_to_list(struct umevent_list *list,
			     struct umevent_obj *umevent_obj_to_add)
{
	unsigned long flags;
	spin_lock_irqsave(&list->list_lock, flags);
	list_add(&umevent_obj_to_add->head, &list->head);
	spin_unlock_irqrestore(&list->list_lock, flags);
}

/**
 * psb_umevent_init_list - initialize event list
 *
 * @list: list to initialize
 *
 */
void psb_umevent_init_list(struct umevent_list *list)
{
	spin_lock_init(&list->list_lock);
	INIT_LIST_HEAD(&list->head);
}

/**
 * psb_umevent_create_list - allocate an event list
 *
 */
struct umevent_list *psb_umevent_create_list()
{
	struct umevent_list *new_umevent_list;
	new_umevent_list = NULL;
	new_umevent_list = kmalloc(sizeof(struct umevent_list), GFP_ATOMIC);
	return new_umevent_list;
}

/*EXPORT_SYMBOL(psb_umevent_create_list); */
/**
 * psb_umevent_destroy_list - destroy a list and clean up all mem
 *
 * @list: list to destroy and clean up after
 *
 */
void psb_umevent_destroy_list(struct umevent_list *list)
{
	struct umevent_obj *umevent_obj_curr;
	struct list_head *node;
	struct list_head *node_kill;
	int i;
	i = 0;
	node = NULL;
	node_kill = NULL;
	node = list->head.next;
	while (node != (&list->head)) {
		umevent_obj_curr = list_entry(node, struct umevent_obj, head);
		node_kill = node;
		node = umevent_obj_curr->head.next;
		list_del(node_kill);
		psb_destroy_umevent_obj(umevent_obj_curr);
		umevent_obj_curr = NULL;
		i++;
	}
	kset_unregister(list->umevent_disp_pool);
	kfree(list);
}

/**
 * psb_umevent_remove_from_list - remove an event from tracking list
 *
 * @list: list to remove the event from
 * @disp_to_remove: name of event to remove.
 *
 */
void psb_umevent_remove_from_list(struct umevent_list *list,
				  const char *disp_to_remove)
{
	struct umevent_obj *umevent_obj_curr = NULL;
	struct list_head *node = NULL;
	struct list_head *node_kill = NULL;
	int i = 0;
	int found_match = 0;
	i = 0;
	node = NULL;
	node_kill = NULL;
	node = list->head.next;
	while (node != (&list->head)) {
		umevent_obj_curr = list_entry(node, struct umevent_obj, head);
		if (strcmp(umevent_obj_curr->kobj.name, disp_to_remove) == 0) {
			found_match = 1;
			break;
		}
		node = NULL;
		node = umevent_obj_curr->head.next;
		i++;
	}
	if (found_match == 1) {
		node_kill = node;
		node = umevent_obj_curr->head.next;
		list_del(node_kill);
		psb_destroy_umevent_obj(umevent_obj_curr);
		umevent_obj_curr = NULL;
	}
}

/*EXPORT_SYMBOL(psb_umevent_remove_from_list); */
/**
 * psb_umevent_find_obj - find an event in a tracking list
 *
 * @name: name of the event to find
 * @list: list to find the event in
 *
 */
struct umevent_obj *psb_umevent_find_obj(const char *name,
					 struct umevent_list *list)
{
	struct umevent_obj *umevent_obj_curr = NULL;
	struct list_head *node = NULL;
	struct list_head *node_find = NULL;
	int i = 0;
	int found_match = 0;
	i = 0;
	node = NULL;
	node_find = NULL;
	node = list->head.next;
	while (node != (&list->head)) {
		umevent_obj_curr = list_entry(node, struct umevent_obj, head);
		if (strcmp(umevent_obj_curr->kobj.name, name) == 0) {
			found_match = 1;
			break;
		}
		node = NULL;
		node = umevent_obj_curr->head.next;
		i++;
	}
	if (found_match == 1)
		return umevent_obj_curr;

	return NULL;
}

/*EXPORT_SYMBOL(psb_umevent_find_obj); */
/**
 * psb_umevent_debug_dump_list - debug list dump
 *
 * @list: list to dump
 *
 */
void psb_umevent_debug_dump_list(struct umevent_list *list)
{
	struct umevent_obj *umevent_obj_curr;
	unsigned long flags;
	struct list_head *node;
	int i;
	spin_lock_irqsave(&list->list_lock, flags);
	i = 0;
	node = NULL;
	node = list->head.next;
	while (node != (&list->head)) {
		umevent_obj_curr = list_entry(node, struct umevent_obj, head);
		/*TBD: DUMP ANY REQUIRED VALUES WITH PRINTK */
		node = NULL;
		node = umevent_obj_curr->head.next;
		i++;
	}
	spin_unlock_irqrestore(&list->list_lock, flags);
}

void psb_sysfs_uevent(struct drm_device *dev, char *event_string)
{
	char *envp[] = { event_string, NULL };

	kobject_uevent_env(&dev->primary->kdev.kobj, KOBJ_CHANGE, envp);
}
