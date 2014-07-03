/*
 * Copyright (c) 2009, Intel Corporation.
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
#ifndef _PSB_UMEVENT_H_
#define _PSB_UMEVENT_H_
/**
 * required includes
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <drm/drmP.h>
#include <drm/drm_core.h>
#include <drm/drm_pciids.h>
#include <linux/spinlock.h>
/**
 * event groups for routing to different user mode threads
 *
 */
#define DRM_DPST_SOCKET_GROUP_ID 1
#define DRM_HOTPLUG_SOCKET_GROUP_ID 2
#define DRM_HDMI_AUDIO_SOCKET_GROUP 4
#define DRM_HDMI_HDCP_SOCKET_GROUP 8
#define DRM_GFX_SOCKET_GROUPS 15
/**
 * event structure managed by kobjects
 *
 */
struct umevent_obj {
	struct kobject kobj;
	struct list_head head;
	int data_0_val;
	int data_1_val;
	int data_2_val;
	int data_3_val;
	int data_4_val;
	int data_5_val;
	int data_6_val;
	int data_7_val;
};
/**
 * event tracking list element
 *
 */
struct umevent_list {
	struct list_head head;
	struct kset *umevent_disp_pool;
	spinlock_t list_lock;
};
/**
 * to go back and forth between kobjects and their main container
 *
 */
#define to_umevent_obj(x) \
	container_of(x, struct umevent_obj, kobj)

/**
 * event attributes exposed via sysfs
 *
 */
struct umevent_attribute {
	struct attribute attr;
	 ssize_t(*show) (struct umevent_obj * any_umevent_obj,
			 struct umevent_attribute * attr, char *buf);
	 ssize_t(*store) (struct umevent_obj * any_umevent_obj,
			  struct umevent_attribute * attr,
			  const char *buf, size_t count);
};
/**
 * to go back and forth between the attribute passed to us by the OS
 * and the umevent_attribute
 *
 */
#define to_umevent_attr(x) \
	container_of(x, struct umevent_attribute, \
	attr)

/**
 * umevent function prototypes
 *
 */
extern struct umevent_obj *psb_create_umevent_obj(const char *name, struct umevent_list
						  *list);
extern ssize_t psb_umevent_attr_show(struct kobject *kobj,
				     struct attribute *attr, char *buf);
extern ssize_t psb_umevent_attr_store(struct kobject *kobj,
				      struct attribute *attr,
				      const char *buf, size_t len);
extern ssize_t psb_umevent_attr_show_imp(struct umevent_obj
					 *any_umevent_obj,
					 struct umevent_attribute *attr,
					 char *buf);
extern ssize_t psb_umevent_attr_store_imp(struct umevent_obj
					  *any_umevent_obj,
					  struct umevent_attribute *attr,
					  const char *buf, size_t count);
extern void psb_umevent_cleanup(struct umevent_list *kill_list);
extern int psb_umevent_init(struct kobject *parent_kobj,
			    struct umevent_list *new_umevent_list,
			    const char *name);
extern void psb_umevent_init_list(struct umevent_list *list);
extern void psb_umevent_debug_dump_list(struct umevent_list *list);
extern void psb_umevent_add_to_list(struct umevent_list *list, struct umevent_obj
				    *umevent_obj_to_add);
extern void psb_umevent_destroy_list(struct umevent_list *list);
extern struct umevent_list *psb_umevent_create_list(void);
extern void psb_umevent_notify(struct umevent_obj *notify_disp_obj);
extern void psb_umevent_notify_gfxsock(struct umevent_obj *notify_disp_obj,
				       int dst_group_id);
extern void psb_umevent_obj_release(struct kobject *kobj);
extern void psb_umevent_remove_from_list(struct umevent_list *list,
					 const char *disp_to_remove);
extern void psb_umevent_workqueue_dispatch(int work_type, const char *name,
					   struct umevent_list *list);
extern void psb_umevent_notify_change(struct umevent_obj *notify_disp_obj);
extern void psb_umevent_notify_change_gfxsock(struct umevent_obj
					      *notify_disp_obj,
					      int dst_group_id);
extern struct umevent_obj *psb_umevent_find_obj(const char *name, struct umevent_list
						*list);
/**
 * socket function prototypes
 *
 */
extern int psb_kobject_uevent(struct kobject *kobj,
			      enum kobject_action action, int dst_group_id);
extern int psb_kobject_uevent_env(struct kobject *kobj,
				  enum kobject_action action,
				  char *envp[], int dst_group_id);
int psb_add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
    __attribute__ ((format(printf, 2, 3)));
int psb_kobject_action_type(const char *buf,
			    size_t count, enum kobject_action *type);

extern void psb_sysfs_uevent(struct drm_device *dev, char *event_string);

#if defined(CONFIG_NET)
int psb_kobject_uevent_init(void);
#endif

#endif
