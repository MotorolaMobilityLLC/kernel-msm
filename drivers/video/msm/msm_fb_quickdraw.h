/* Copyright (c) 2013, Motorola Mobility, LLC.
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

#ifndef MSM_FB_QUICKDRAW_H
#define MSM_FB_QUICKDRAW_H

#include <linux/file.h>
#include <linux/msm_mdp.h>

struct msmfb_quickdraw_buffer {
	struct msmfb_quickdraw_buffer_data data;
	struct file *file;
	int mem_fd;
	int overlay_id;
	atomic_t locked;
	wait_queue_head_t wait_queue;
	struct list_head list;
	struct kref kref;
};

#ifdef CONFIG_FB_MSM_QUICKDRAW

int msm_fb_quickdraw_init(void);
int msm_fb_quickdraw_create_buffer(struct msmfb_quickdraw_buffer_data *buffer);
int msm_fb_quickdraw_destroy_buffer(int buffer_id);
int msm_fb_quickdraw_lock_buffer(int buffer_id);
int msm_fb_quickdraw_unlock_buffer(int buffer_id);
void msm_fb_quickdraw_register_notifier(struct msm_fb_data_type *mfd);

#else

static inline int msm_fb_quickdraw_init(void) { return 0; };
static inline int msm_fb_quickdraw_create_buffer(
	struct msmfb_quickdraw_buffer_data *buffer) { return 0; };
static inline int msm_fb_quickdraw_destroy_buffer(int buffer_id) { return 0; };
static inline int msm_fb_quickdraw_lock_buffer(int buffer_id) { return 0; };
static inline int msm_fb_quickdraw_unlock_buffer(int buffer_id) { return 0; };
static inline void msm_fb_quickdraw_register_notifier(
	struct msm_fb_data_type *mfd) {};

#endif

#endif /* MSM_FB_QUICKDRAW_H */
