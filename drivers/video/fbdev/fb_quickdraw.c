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

#include <linux/delay.h>
#include <linux/list.h>
#include <linux/fb_quickdraw.h>
#include <linux/fb_quickdraw_ops.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#define FRAMEBUFFER_ID_START (INT_MAX / 2)


static LIST_HEAD(fb_quickdraw_buffer_list_head);
static DEFINE_MUTEX(list_lock);
static struct fb_quickdraw_impl_data *fb_quickdraw_impl;

static int fb_quickdraw_add_buffer(struct fb_quickdraw_buffer_data *data);
static int fb_quickdraw_lock_buffer_write(int buffer_id);
static int fb_quickdraw_unlock_buffer_write(int buffer_id);
static struct fb_quickdraw_framebuffer *get_framebuffer(void);
static void delete_buffer(struct kobject *kobj);

static struct kobj_type ktype_fb_quickdraw = {
	.sysfs_ops = NULL,
	.default_attrs = NULL,
	.release = delete_buffer,
};

/* Quickdraw Internal Helper Functions */

/* This function creates a new file descriptor for use in the suspend
   context. This is necessary for permissions issues since the buffers
   were allocated in the app context. */
static int setup_fd(struct fb_quickdraw_buffer *buffer)
{
	int ret = -EINVAL;

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		pr_err("buffer is NULL\n");
		goto exit;
	}

	if (!buffer->file) {
		/* This is unexpected, but wont hurt anything. The architecture
		   implementation is welcome to handle this in whatever way they
		   want to. */
		ret = 0;
		goto exit;
	}

	if (buffer->mem_fd == -1) {
		int fd = get_unused_fd();
		if (fd < 0) {
			pr_err("%s: unable to get fd (ret: %d)\n",
				__func__, fd);
			goto exit;
		}

		buffer->mem_fd = fd;
		fd_install(buffer->mem_fd, buffer->file);
	}

	ret = buffer->mem_fd;

exit:
	pr_debug("%s-: (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static void delete_buffer_fd(struct work_struct *work)
{
	struct fb_quickdraw_buffer *buffer =
		container_of(work, struct fb_quickdraw_buffer, delete_work);

	pr_debug("%s+: (buffer: %p) (fd: %d)\n", __func__, buffer,
		buffer->mem_fd);

	if (buffer->mem_fd >= 0)
		sys_close(buffer->mem_fd);
	else if (buffer->file)
		fput(buffer->file);
	vfree(buffer);

	pr_debug("%s-: (buffer: %p)\n", __func__, buffer);
}

static void delete_buffer(struct kobject *kobj)
{
	struct fb_quickdraw_buffer *buffer =
		container_of(kobj, struct fb_quickdraw_buffer, kobject);

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	/* We need to schedule work to close the file descriptor and finish
	   cleaning up the buffer because THIS function is called by userspace
	   and the file descriptor is owned by the main kernel threads. This
	   is basically corollary to why we had to call setup_fd in a later
	   step instead of doing it in the ADD IOCTL itself. */
	schedule_work(&buffer->delete_work);

	pr_debug("%s-: (buffer: %p)\n", __func__, buffer);
}

struct fb_quickdraw_buffer *alloc_buffer(struct fb_quickdraw_buffer_data *data)
{
	struct fb_quickdraw_buffer *buffer = NULL;

	pr_debug("%s+ (data: %p)\n", __func__, data);

	buffer = vzalloc(sizeof(struct fb_quickdraw_buffer));
	if (buffer) {
		memcpy(&buffer->data, data,
			sizeof(struct fb_quickdraw_buffer_data));
		buffer->mem_fd = -1;
		init_rwsem(&buffer->rwsem);
		INIT_WORK(&buffer->delete_work, delete_buffer_fd);
		kobject_init(&buffer->kobject, &ktype_fb_quickdraw);
	} else
		pr_err("%s: vzalloc failed!\n", __func__);

	pr_debug("%s- (buffer: %p)\n", __func__, buffer);

	return buffer;
}

static int get_buffer(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+ (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	kobject_get(&buffer->kobject);
exit:
	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static int put_buffer(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+ (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	kobject_put(&buffer->kobject);
exit:
	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static int lock_buffer_read(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	down_read(&buffer->rwsem);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int unlock_buffer_read(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	up_read(&buffer->rwsem);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static struct fb_quickdraw_buffer *lookup_and_get_buffer(int buffer_id)
{
	struct fb_quickdraw_buffer *buffer = NULL, *cur = NULL;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	mutex_lock(&list_lock);

	list_for_each_entry(cur, &fb_quickdraw_buffer_list_head, list) {
		if (cur->data.buffer_id == buffer_id) {
			buffer = cur;
			get_buffer(buffer);
			break;
		}
	}

	mutex_unlock(&list_lock);

	if (!buffer)
		pr_err("%s: No buffer found with ID: %d\n", __func__,
			buffer_id);

	pr_debug("%s- (buffer: %p)\n", __func__, buffer);

	return buffer;
}

static void clip_rect(int panel_xres, int panel_yres, int x, int y,
	int w, int h, struct fb_quickdraw_rect *src_rect,
	struct fb_quickdraw_rect *dst_rect)
{
	int clip_x = x;
	int clip_y = y;

	pr_debug("%s+ [panel: %d,%d] [x:%d|y:%d][w:%d|h:%d]\n", __func__,
		panel_xres, panel_yres, x, y, w, h);

	if (clip_x < 0) {
		w += clip_x;
		clip_x = 0;
	}

	if (clip_x + w > panel_xres)
		w = panel_xres - clip_x;

	if (clip_y < 0) {
		h += clip_y;
		clip_y = 0;
	}

	if (clip_y + h > panel_yres)
		h = panel_yres - clip_y;

	w = w < 0 ? 0 : w;
	h = h < 0 ? 0 : h;

	if (src_rect) {
		src_rect->x = clip_x - x;
		src_rect->y = clip_y - y;
		src_rect->w = w;
		src_rect->h = h;
		pr_debug("%s src[x:%d|y:%d][w:%d|h:%d]\n", __func__,
			src_rect->x, src_rect->y, src_rect->w, src_rect->h);
	}

	if (dst_rect) {
		dst_rect->x = clip_x;
		dst_rect->y = clip_y;
		dst_rect->w = w;
		dst_rect->h = h;
		pr_debug("%s dst[x:%d|y:%d][w:%d|h:%d]\n", __func__,
			dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);
	}

	pr_debug("%s-\n", __func__);
}

static int blit_into_framebuffer(struct fb_quickdraw_framebuffer *framebuffer,
	void *src_addr, struct fb_quickdraw_rect *rect)
{
	struct fb_quickdraw_rect src_rect;
	struct fb_quickdraw_rect dst_rect;
	int stride, dst_stride, src_stride;
	void *dst, *dst_end;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!framebuffer) {
		ret = -EINVAL;
		goto exit;
	}

	fb_quickdraw_lock_buffer_write(framebuffer->buffer_id);

	clip_rect(framebuffer->w, framebuffer->h,
		rect->x, rect->y, rect->w, rect->h, &src_rect, &dst_rect);

	stride = dst_rect.w * framebuffer->bpp;
	dst_stride = framebuffer->w * framebuffer->bpp;
	src_stride = rect->w * framebuffer->bpp;
	dst = framebuffer->addr + (dst_rect.y * framebuffer->w + dst_rect.x) *
		framebuffer->bpp;
	dst_end = dst + (dst_rect.h * dst_stride);

	if (stride == 0)
		goto skip_blit;

	if (src_addr) {
		void *src = src_addr + (src_rect.y * rect->w + src_rect.x) *
			framebuffer->bpp;
		for (; dst < dst_end; dst += dst_stride, src += src_stride)
			memcpy(dst, src, stride);
	} else {
		for (; dst < dst_end; dst += dst_stride)
			memset(dst, 0, stride);
	}

skip_blit:
	/* Track that the framebuffer has been updated */
	framebuffer->draw_count++;

	fb_quickdraw_unlock_buffer_write(framebuffer->buffer_id);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int blit_draw(struct fb_quickdraw_framebuffer *framebuffer,
	struct fb_quickdraw_buffer *buffer, int x, int y)
{
	struct ion_handle *src_ihdl = NULL;
	struct fb_quickdraw_rect src_rect;
	void *src_addr = NULL;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!framebuffer || !buffer) {
		ret = -EINVAL;
		goto exit;
	}

	lock_buffer_read(buffer);

	memcpy(&src_rect, &buffer->data.rect, sizeof(struct fb_quickdraw_rect));

	if (x != COORD_NO_OVERRIDE)
		src_rect.x = x;
	if (y != COORD_NO_OVERRIDE)
		src_rect.y = y;

	src_ihdl = ion_import_dma_buf(fb_quickdraw_impl->iclient,
		buffer->mem_fd);
	if (IS_ERR_OR_NULL(src_ihdl)) {
		ret = PTR_ERR(src_ihdl);
		src_ihdl = NULL;
		pr_err("%s: error importing ion buffer!", __func__);
		goto unlock_buffer;
	}

	src_addr = ion_map_kernel(fb_quickdraw_impl->iclient, src_ihdl);
	if (IS_ERR_OR_NULL(src_addr)) {
		ret = PTR_ERR(src_addr);
		src_addr = NULL;
		pr_err("%s: error mapping ion buffer!", __func__);
		goto unlock_buffer;
	}

	pr_debug("%s: ihdl=%p virt=%p\n", __func__, src_ihdl, src_addr);

	ret = blit_into_framebuffer(framebuffer, src_addr, &src_rect);

unlock_buffer:
	unlock_buffer_read(buffer);

exit:
	if (src_addr)
		ion_unmap_kernel(fb_quickdraw_impl->iclient, src_ihdl);

	if (src_ihdl)
		ion_free(fb_quickdraw_impl->iclient, src_ihdl);

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int blit_commit(struct fb_quickdraw_framebuffer *framebuffer)
{
	struct fb_quickdraw_buffer *buffer;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!framebuffer) {
		ret = -EINVAL;
		goto exit;
	}

	if (!framebuffer->draw_count) {
		pr_warn("%s: No framebuffer updates! Skipping commit!\n",
			__func__);
		goto exit;
	}

	buffer = lookup_and_get_buffer(framebuffer->buffer_id);
	if (!buffer)
		goto exit;

	lock_buffer_read(buffer);

	ret = setup_fd(buffer);
	if (ret >= 0) {
		ret = fb_quickdraw_impl->ops->execute(
			fb_quickdraw_impl->ops->data, buffer);
	}

	framebuffer->draw_count = 0;

	unlock_buffer_read(buffer);

	put_buffer(buffer);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);
	return ret;
}

static void free_framebuffer(struct fb_quickdraw_framebuffer *framebuffer)
{
	pr_debug("%s+\n", __func__);

	if (!framebuffer) {
		pr_err("%s: framebuffer is NULL\n", __func__);
		goto exit;
	}

	if (framebuffer->addr)
		ion_unmap_kernel(fb_quickdraw_impl->iclient, framebuffer->ihdl);

	if (framebuffer->ihdl)
		ion_free(fb_quickdraw_impl->iclient, framebuffer->ihdl);

	kfree(framebuffer);
	framebuffer = NULL;

exit:
	pr_debug("%s-\n", __func__);
}

static struct fb_quickdraw_framebuffer *create_framebuffer(int w, int h,
	int format, int bpp)
{
	int ret = 0;
	struct fb_quickdraw_framebuffer *framebuffer;

	pr_debug("%s+\n", __func__);

	framebuffer = kzalloc(sizeof(struct fb_quickdraw_framebuffer),
		GFP_KERNEL);
	if (!framebuffer) {
		pr_err("%s: failed to alloc framebuffer\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	framebuffer->size = w * h * bpp;
	framebuffer->w = w;
	framebuffer->h = h;
	framebuffer->format = format;
	framebuffer->bpp = bpp;
	framebuffer->buffer_id = FRAMEBUFFER_ID_START;
	framebuffer->draw_count = 0;

	framebuffer->ihdl = ion_alloc(fb_quickdraw_impl->iclient,
		framebuffer->size, SZ_4K, fb_quickdraw_impl->ion_heap_id_mask,
		0);
	if (IS_ERR_OR_NULL(framebuffer->ihdl)) {
		ret = PTR_ERR(framebuffer->ihdl);
		framebuffer->ihdl = NULL;
		pr_err("%s: unable to alloc framebuffer from ion\n", __func__);
		goto exit;
	}

	framebuffer->addr = ion_map_kernel(fb_quickdraw_impl->iclient,
		framebuffer->ihdl);
	if (IS_ERR_OR_NULL(framebuffer->addr)) {
		ret = PTR_ERR(framebuffer->addr);
		framebuffer->addr = NULL;
		pr_err("%s: unable to map framebuffer from ion\n", __func__);
		goto exit;
	}

	framebuffer->fd = ion_share_dma_buf_fd(fb_quickdraw_impl->iclient,
		framebuffer->ihdl);
	if (framebuffer->fd < 0) {
		ret = framebuffer->fd;
		pr_err("%s: unable to get fd from ion\n", __func__);
		goto exit;
	}

	pr_debug("%s: ihdl=%p virt=%p fd=%d size=%zu w=%d h=%d\n", __func__,
		framebuffer->ihdl, framebuffer->addr, framebuffer->fd,
		framebuffer->size, w, h);

exit:
	pr_debug("%s-\n", __func__);

	if (ret) {
		if (framebuffer)
			free_framebuffer(framebuffer);
		framebuffer = ERR_PTR(ret);
	}

	return framebuffer;
}

static struct fb_quickdraw_framebuffer *get_framebuffer(void)
{
	/* This function is simple now, but this could be extended to allow for
	   double buffering of the framebuffer. */
	return fb_quickdraw_impl->fb;
}

/* Quickdraw Userspace Interface */

static int fb_quickdraw_add_buffer(struct fb_quickdraw_buffer_data *data)
{
	struct fb_quickdraw_framebuffer *framebuffer;
	struct fb_quickdraw_buffer *buffer, *cur;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!data) {
		pr_err("%s: Buffer data null\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (data->rect.w <= 0 || data->rect.h <= 0) {
		pr_err("%s: Buffer width[%d]/height[%d] invalid\n",
			__func__, data->rect.w, data->rect.h);
		ret = -EINVAL;
		goto exit;
	}

	framebuffer = get_framebuffer();

	if (framebuffer && (data->format != framebuffer->format)) {
		pr_err("%s: Format doesnt match framebuffer! [%d, %d]\n",
			__func__, data->format, framebuffer->format);
		ret = -EINVAL;
		goto exit;
	}

	mutex_lock(&list_lock);

	list_for_each_entry(cur, &fb_quickdraw_buffer_list_head, list) {
		if (cur->data.buffer_id == data->buffer_id) {
			pr_err("%s: Duplicate buffer_id: %d\n", __func__,
				data->buffer_id);
			ret = -EEXIST;
			goto exit_mutex;
		}
	}

	buffer = alloc_buffer(data);
	if (!buffer) {
		pr_err("%s: alloc_buffer failed!\n",
			__func__);
		ret = -ENOMEM;
		goto exit_mutex;
	}

	if (buffer->data.user_fd >= 0) {
		buffer->file = fget(buffer->data.user_fd);
		if (!buffer->file) {
			pr_err("%s: Unable to get the file\n",
				__func__);
			put_buffer(buffer);
			ret = -EBADF;
			goto exit_mutex;
		}
	}

	list_add_tail(&buffer->list, &fb_quickdraw_buffer_list_head);

exit_mutex:
	mutex_unlock(&list_lock);

	if (!ret && !framebuffer) {
		struct fb_quickdraw_buffer_data fb_data;
		int bpp = fb_quickdraw_impl->ops->format2bpp(
			fb_quickdraw_impl->ops->data, data->format);

		framebuffer = create_framebuffer(
			fb_quickdraw_impl->fbi->var.xres,
			fb_quickdraw_impl->fbi->var.yres,
			data->format, bpp);
		if (IS_ERR_OR_NULL(framebuffer)) {
			ret = PTR_ERR(framebuffer);
			pr_err("%s: error creating framebuffer!", __func__);
			goto exit;
		}

		fb_quickdraw_impl->fb = framebuffer;

		fb_data.buffer_id = framebuffer->buffer_id;
		fb_data.format = framebuffer->format;
		fb_data.rect.x = 0;
		fb_data.rect.y = 0;
		fb_data.rect.w = fb_quickdraw_impl->fbi->var.xres;
		fb_data.rect.h = fb_quickdraw_impl->fbi->var.yres;
		fb_data.user_fd = framebuffer->fd;

		ret = fb_quickdraw_add_buffer(&fb_data);
		if (ret) {
			pr_err("%s: unable to add framebuffer\n", __func__);
			free_framebuffer(framebuffer);
			fb_quickdraw_impl->fb = NULL;
		}
	}

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int fb_quickdraw_remove_buffer(int buffer_id)
{
	struct fb_quickdraw_buffer *buffer = NULL;
	int ret = -EINVAL;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	buffer = lookup_and_get_buffer(buffer_id);
	if (!buffer)
		goto exit;

	/* remove this buffer from the list */
	mutex_lock(&list_lock);
	list_del(&buffer->list);
	mutex_unlock(&list_lock);

	/* remove this function's reference acquired during lookup */
	ret = put_buffer(buffer);
	if (ret) {
		/* this should not delete the buffer */
		pr_warn("%s: Unexpected buffer removal!\n", __func__);
		goto exit;
	}

	/* remove userspace's reference - this may not delete the buffer
	   immediately if the buffer is currently on the display */
	put_buffer(buffer);

exit:
	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static int fb_quickdraw_lock_buffer_write(int buffer_id)
{
	int ret = 0;
	struct fb_quickdraw_buffer *buffer;

	pr_debug("%s+\n", __func__);

	buffer = lookup_and_get_buffer(buffer_id);
	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	down_write(&buffer->rwsem);

	put_buffer(buffer);
exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int fb_quickdraw_unlock_buffer_write(int buffer_id)
{
	int ret = 0;
	struct fb_quickdraw_buffer *buffer;

	pr_debug("%s+\n", __func__);

	buffer = lookup_and_get_buffer(buffer_id);
	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	up_write(&buffer->rwsem);

	put_buffer(buffer);
exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int fb_quickdraw_init_buffers(void)
{
	struct fb_quickdraw_framebuffer *framebuffer;
	struct fb_quickdraw_buffer *buffer = NULL;
	struct list_head *entry, *temp;

	pr_debug("%s+\n", __func__);

	mutex_lock(&list_lock);

	list_for_each_safe(entry, temp, &fb_quickdraw_buffer_list_head) {
		buffer = list_entry(entry, struct fb_quickdraw_buffer, list);
		list_del(&buffer->list);
		put_buffer(buffer);
	}

	mutex_unlock(&list_lock);

	framebuffer = get_framebuffer();
	if (framebuffer) {
		free_framebuffer(framebuffer);
		fb_quickdraw_impl->fb = NULL;
	}

	pr_debug("%s-\n", __func__);

	return 0;
}

static long fb_quickdraw_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	BUG_ON(!fb_quickdraw_impl);

	switch (cmd) {
	case FB_QUICKDRAW_INIT:
		ret = fb_quickdraw_init_buffers();
		if (ret)
			pr_err("%s: FB_QUICKDRAW_INIT failed (ret=%d)\n",
				__func__, ret);
		break;

	case FB_QUICKDRAW_ADD_BUFFER:
		{
		struct fb_quickdraw_buffer_data data;
		if (copy_from_user(&data, (void __user *)arg,
			sizeof(struct fb_quickdraw_buffer_data))) {
			pr_err("%s: FB_QUICKDRAW_ADD_BUFFER copy_from_user failed\n",
				__func__);
			ret = -EFAULT;
			break;
		}
		ret = fb_quickdraw_add_buffer(&data);
		if (ret)
			pr_err("%s: FB_QUICKDRAW_ADD_BUFFER failed (ret=%d)\n",
				__func__, ret);
		}
		break;

	case FB_QUICKDRAW_REMOVE_BUFFER:
	case FB_QUICKDRAW_LOCK_BUFFER:
	case FB_QUICKDRAW_UNLOCK_BUFFER:
		{
		int buffer_id;
		if (copy_from_user(&buffer_id, (void __user *)arg,
			sizeof(int))) {
			pr_err("%s: FB_QUICKDRAW (cmd: 0x%x) copy_from_user failed\n",
				__func__, cmd);
			ret = -EFAULT;
			break;
		}

		if (cmd == FB_QUICKDRAW_REMOVE_BUFFER)
			ret = fb_quickdraw_remove_buffer(buffer_id);
		else if (cmd == FB_QUICKDRAW_LOCK_BUFFER)
			ret = fb_quickdraw_lock_buffer_write(buffer_id);
		else if (cmd == FB_QUICKDRAW_UNLOCK_BUFFER)
			ret = fb_quickdraw_unlock_buffer_write(buffer_id);

		if (ret)
			pr_err("%s: FB_QUICKDRAW (cmd: 0x%x) failed (ret=%d)\n",
				__func__, cmd, ret);
		}
		break;

	default:
		pr_err("%s: FB_QUICKDRAW (cmd: 0x%x) unknown ioctl\n", __func__,
			cmd);
		ret = -EINVAL;
		break;
	}

	pr_debug("%s-\n", __func__);

	return ret;
}

/* Quickdraw Client Interface */

int fb_quickdraw_prepare(unsigned char panel_state)
{
	struct fb_quickdraw_framebuffer *framebuffer;
	int ret = -EINVAL;

	pr_debug("%s+\n", __func__);

	BUG_ON(!fb_quickdraw_impl);

	framebuffer = get_framebuffer();
	if (!framebuffer) {
		pr_err("%s: Unable to get framebuffer!\n", __func__);
		goto exit;
	}

	framebuffer->draw_count = 0;

	ret = fb_quickdraw_impl->ops->prepare(fb_quickdraw_impl->ops->data,
		panel_state);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_execute(int buffer_id, int commit, int x, int y)
{
	struct fb_quickdraw_framebuffer *framebuffer;
	struct fb_quickdraw_buffer *buffer;
	int ret = -EINVAL;

	pr_debug("%s+ (buffer_id: %d commit: %d x: %d y: %d)\n", __func__,
		buffer_id, commit, x, y);

	BUG_ON(!fb_quickdraw_impl);

	framebuffer = get_framebuffer();
	if (!framebuffer) {
		pr_err("%s: Unable to get framebuffer!\n", __func__);
		goto exit;
	}

	buffer = lookup_and_get_buffer(buffer_id);
	if (!buffer)
		goto exit;
	ret = setup_fd(buffer);
	if (ret >= 0) {
		ret = blit_draw(framebuffer, buffer, x, y);
		if (ret)
			pr_err("%s: Failed to draw in framebuffer!\n",
				__func__);
		else if (commit)
			ret = blit_commit(framebuffer);
	}

	put_buffer(buffer);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_erase(int commit, int x1, int y1, int x2, int y2)
{
	struct fb_quickdraw_framebuffer *framebuffer;
	struct fb_quickdraw_rect src_rect = {x1, y1, x2 - x1, y2 - y1};
	int ret = -EINVAL;

	pr_debug("%s+ (commit: %d x1: %d y1: %d x2: %d y2: %d)\n", __func__,
		commit, x1, y1, x2, y2);

	BUG_ON(!fb_quickdraw_impl);

	framebuffer = get_framebuffer();
	if (!framebuffer) {
		pr_err("%s: Unable to get framebuffer!\n", __func__);
		goto exit;
	}

	ret = blit_into_framebuffer(framebuffer, NULL, &src_rect);
	if (ret)
		pr_err("%s: Failed to erase in framebuffer!\n", __func__);
	else if (commit)
		ret = blit_commit(framebuffer);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_cleanup(int commit)
{
	struct fb_quickdraw_framebuffer *framebuffer;
	int ret = -EINVAL;

	pr_debug("%s+ (commit: %d)\n", __func__, commit);

	BUG_ON(!fb_quickdraw_impl);

	framebuffer = get_framebuffer();
	if (!framebuffer) {
		pr_err("%s: Unable to get framebuffer!\n", __func__);
		goto exit;
	}

	if (commit)
		ret = blit_commit(framebuffer);
	else if (framebuffer->draw_count > 0)
		pr_warn("%s: %d updates are not being sent to the panel!\n",
			__func__, framebuffer->draw_count);

	ret = fb_quickdraw_impl->ops->cleanup(fb_quickdraw_impl->ops->data);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static const struct file_operations fb_quickdraw_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = fb_quickdraw_ioctl,
};

static struct miscdevice fb_quickdraw_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = FB_QUICKDRAW_NAME,
	.fops = &fb_quickdraw_fops,
};

void fb_quickdraw_register_impl(struct fb_quickdraw_impl_data *impl)
{
	pr_debug("%s+\n", __func__);

	BUG_ON(fb_quickdraw_impl);
	BUG_ON(!impl->fbi);
	BUG_ON(!impl->ops);
	BUG_ON(!impl->iclient);

	fb_quickdraw_impl = impl;

	misc_register(&fb_quickdraw_misc_device);

	pr_debug("%s-\n", __func__);
}

static void __exit fb_quickdraw_exit(void)
{
	misc_deregister(&fb_quickdraw_misc_device);
}

MODULE_AUTHOR("Jared Suttles <jared.suttles@motorola.com>, Joseph Swantek <jswantek@motorola.com>");
MODULE_DESCRIPTION("Framebuffer quickdraw driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");

module_exit(fb_quickdraw_exit);
