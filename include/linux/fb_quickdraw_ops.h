/* Copyright (c) 2014, Motorola Mobility, LLC.
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
#ifndef _FB_QUICKDRAW_OPS_H_
#define _FB_QUICKDRAW_OPS_H_

#include <linux/file.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

struct fb_quickdraw_buffer {
	struct fb_quickdraw_buffer_data data;
	struct file *file;
	int mem_fd;
	atomic_t locked;
	wait_queue_head_t wait_queue;
	struct list_head list;
	struct kref kref;
	struct work_struct delete_work;
};

struct fb_quickdraw_ops {
	int (*prepare)(void *data, unsigned char panel_state);
	int (*execute)(void *data, struct fb_quickdraw_buffer *buffer,
		int x, int y);
	int (*erase)(void *data, int x1, int y1, int x2, int y2);
	int (*cleanup)(void *data);
	int (*validate_buffer)(void *data,
		struct fb_quickdraw_buffer_data *buffer_data);
	struct fb_quickdraw_buffer *(*alloc_buffer)(void *data,
		struct fb_quickdraw_buffer_data *buffer_data);
	int (*delete_buffer)(void *data, struct fb_quickdraw_buffer *buffer);
	void *data; /* arbitrary data passed back to user */
};

void fb_quickdraw_register_ops(struct fb_quickdraw_ops *ops);
struct fb_quickdraw_buffer *fb_quickdraw_alloc_buffer(
	struct fb_quickdraw_buffer_data *data, size_t size);
int fb_quickdraw_get_buffer(struct fb_quickdraw_buffer *buffer);
int fb_quickdraw_put_buffer(struct fb_quickdraw_buffer *buffer);
int fb_quickdraw_lock_buffer(struct fb_quickdraw_buffer *buffer);
int fb_quickdraw_unlock_buffer(struct fb_quickdraw_buffer *buffer);

static inline int fb_quickdraw_check_alignment(int value, int align)
{
	return value % align;
}
int fb_quickdraw_correct_alignment(int coord, int align);
void fb_quickdraw_clip_rect(int panel_xres, int panel_yres, int x, int y,
			    int w, int h, struct fb_quickdraw_rect *src_rect,
			    struct fb_quickdraw_rect *dst_rect);

#endif /* _FB_QUICKDRAW_OPS_H_ */
