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

#include <linux/fb.h>
#include <linux/file.h>
#include <linux/ion.h>
#include <linux/kobject.h>
#include <linux/rwsem.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

struct fb_quickdraw_buffer {
	struct fb_quickdraw_buffer_data data;
	struct file *file;
	int mem_fd;
	struct rw_semaphore rwsem;
	struct list_head list;
	struct kobject kobject;
	struct work_struct delete_work;
};

struct fb_quickdraw_framebuffer {
	struct ion_handle *ihdl;
	void *addr;
	size_t size;
	int fd;
	int w;
	int h;
	int buffer_id;
	int format;
	int bpp;
	int draw_count;
};

struct fb_quickdraw_ops {
	int (*prepare)(void *data, unsigned char panel_state);
	int (*execute)(void *data, struct fb_quickdraw_buffer *buffer);
	int (*cleanup)(void *data);
	int (*format2bpp)(void *data, int format);
	void *data; /* arbitrary data passed back to user */
};

struct fb_quickdraw_impl_data {
	struct fb_quickdraw_ops *ops;
	struct fb_info *fbi;
	struct ion_client *iclient;
	unsigned int ion_heap_id_mask;
	struct fb_quickdraw_framebuffer *fb;
};

void fb_quickdraw_register_impl(struct fb_quickdraw_impl_data *impl);

#endif /* _FB_QUICKDRAW_OPS_H_ */
