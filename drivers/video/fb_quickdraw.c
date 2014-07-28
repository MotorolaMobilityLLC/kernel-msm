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
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <mach/iommu_domains.h>

static LIST_HEAD(fb_quickdraw_buffer_list_head);
static DEFINE_MUTEX(list_lock);
static struct fb_quickdraw_ops *fb_quickdraw_ops;


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

static void delete_buffer(struct kref *kref)
{
	struct fb_quickdraw_buffer *buffer =
		container_of(kref, struct fb_quickdraw_buffer, kref);

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	fb_quickdraw_ops->delete_buffer(fb_quickdraw_ops->data, buffer);

	/* We need to schedule work to close the file descriptor and finish
	   cleaning up the buffer because THIS function is called by userspace
	   and the file descriptor is owned by the main kernel threads. This
	   is basically corollary to why we had to call setup_fd in a later
	   step instead of doing it in the ADD IOCTL itself. */
	schedule_work(&buffer->delete_work);

	pr_debug("%s-: (buffer: %p)\n", __func__, buffer);
}

static struct fb_quickdraw_buffer *fb_quickdraw_lookup_and_get_buffer(
	int buffer_id)
{
	struct fb_quickdraw_buffer *buffer = NULL, *cur = NULL;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	mutex_lock(&list_lock);

	list_for_each_entry(cur, &fb_quickdraw_buffer_list_head, list) {
		if (cur->data.buffer_id == buffer_id) {
			buffer = cur;
			fb_quickdraw_get_buffer(buffer);
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

/* Quickdraw Implementation Helper Functions */

struct fb_quickdraw_buffer *fb_quickdraw_alloc_buffer(
	struct fb_quickdraw_buffer_data *data, size_t size)
{
	struct fb_quickdraw_buffer *buffer = NULL;

	pr_debug("%s+ (data: %p)\n", __func__, data);

	BUG_ON(size < sizeof(struct fb_quickdraw_buffer));

	buffer = vzalloc(size);
	if (buffer) {
		memcpy(&buffer->data, data,
			sizeof(struct fb_quickdraw_buffer_data));
		buffer->mem_fd = -1;
		init_waitqueue_head(&buffer->wait_queue);
		INIT_WORK(&buffer->delete_work, delete_buffer_fd);
		kref_init(&buffer->kref);
	} else
		pr_err("%s: vzalloc failed!\n", __func__);

	pr_debug("%s- (buffer: %p)\n", __func__, buffer);

	return buffer;
}

int fb_quickdraw_get_buffer(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+ (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	kref_get(&buffer->kref);
exit:
	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

int fb_quickdraw_put_buffer(struct fb_quickdraw_buffer *buffer)
{
	int ret = -EINVAL;

	pr_debug("%s+ (buffer: %p)\n", __func__, buffer);

	if (!buffer)
		goto exit;

	ret = kref_put(&buffer->kref, delete_buffer);
exit:
	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

int fb_quickdraw_lock_buffer(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	wait_event(buffer->wait_queue, !atomic_cmpxchg(&buffer->locked, 0, 1));

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_unlock_buffer(struct fb_quickdraw_buffer *buffer)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	atomic_set(&buffer->locked, 0);
	wake_up_all(&buffer->wait_queue);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

void fb_quickdraw_clip_rect(int panel_xres, int panel_yres, int x, int y,
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

int fb_quickdraw_correct_alignment(int coord, int align)
{
	int ret = coord;

	pr_debug("%s+ (coord: %d, align: %d)\n", __func__, ret, align);

	if (align > 1) {
		int temp_coord;
		align = coord < 0 ? -align : align;
		temp_coord = coord + (int)(align / 2);
		ret = temp_coord - temp_coord % align;
	}

	if (ret != coord)
		pr_warn("%s: coord[%d] corrected to: %d", __func__, coord, ret);

	pr_debug("%s- (coord: %d, align: %d)\n", __func__, ret, align);

	return ret;
}

/* Quickdraw Userspace Interface */

static int fb_quickdraw_add_buffer(struct fb_quickdraw_buffer_data *data)
{
	struct fb_quickdraw_buffer *buffer, *cur;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!data) {
		pr_err("%s: Buffer data null\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (fb_quickdraw_ops->validate_buffer &&
	    fb_quickdraw_ops->validate_buffer(fb_quickdraw_ops->data, data)) {
		pr_err("%s: Invalid buffer[%d] data\n", __func__,
			data->buffer_id);
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

	buffer = fb_quickdraw_ops->alloc_buffer(fb_quickdraw_ops->data, data);
	if (!buffer) {
		pr_err("%s: fb_quickdraw_ops->alloc_buffer failed!\n",
			__func__);
		ret = -ENOMEM;
		goto exit_mutex;
	}

	if (buffer->data.user_fd >= 0) {
		buffer->file = fget(buffer->data.user_fd);
		if (!buffer->file) {
			pr_err("%s: Unable to get the file\n",
				__func__);
			fb_quickdraw_put_buffer(buffer);
			ret = -EBADF;
			goto exit_mutex;
		}
	}

	list_add_tail(&buffer->list, &fb_quickdraw_buffer_list_head);

exit_mutex:
	mutex_unlock(&list_lock);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int fb_quickdraw_remove_buffer(int buffer_id)
{
	struct fb_quickdraw_buffer *buffer = NULL;
	int ret = -EINVAL;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	buffer = fb_quickdraw_lookup_and_get_buffer(buffer_id);
	if (!buffer)
		goto exit;

	/* remove this buffer from the list */
	mutex_lock(&list_lock);
	list_del(&buffer->list);
	mutex_unlock(&list_lock);

	/* remove this function's reference acquired during lookup */
	ret = fb_quickdraw_put_buffer(buffer);
	if (ret) {
		/* this should not delete the buffer */
		pr_warn("%s: Unexpected buffer removal!\n", __func__);
		goto exit;
	}

	/* remove userspace's reference - this may not delete the buffer
	   immediately if the buffer is currently on the display */
	fb_quickdraw_put_buffer(buffer);

exit:
	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static int fb_quickdraw_user_lock_buffer(int buffer_id)
{
	int ret = -EINVAL;
	struct fb_quickdraw_buffer *buffer;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	buffer = fb_quickdraw_lookup_and_get_buffer(buffer_id);
	if (!buffer)
		goto exit;

	ret = wait_event_interruptible(buffer->wait_queue,
		!atomic_cmpxchg(&buffer->locked, 0, 1));

	fb_quickdraw_put_buffer(buffer);
exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int fb_quickdraw_user_unlock_buffer(int buffer_id)
{
	int ret = 0;
	struct fb_quickdraw_buffer *buffer;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	buffer = fb_quickdraw_lookup_and_get_buffer(buffer_id);
	if (!buffer) {
		ret = -EINVAL;
		goto exit;
	}

	atomic_set(&buffer->locked, 0);
	wake_up_all(&buffer->wait_queue);

	fb_quickdraw_put_buffer(buffer);
exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int fb_quickdraw_init_buffers(void)
{
	struct fb_quickdraw_buffer *buffer = NULL;
	struct list_head *entry, *temp;

	pr_debug("%s+\n", __func__);

	mutex_lock(&list_lock);

	list_for_each_safe(entry, temp, &fb_quickdraw_buffer_list_head) {
		buffer = list_entry(entry, struct fb_quickdraw_buffer, list);
		list_del(&buffer->list);
		fb_quickdraw_put_buffer(buffer);
	}

	mutex_unlock(&list_lock);

	pr_debug("%s-\n", __func__);

	return 0;
}

static long fb_quickdraw_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;

	BUG_ON(!fb_quickdraw_ops);

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
			ret = fb_quickdraw_user_lock_buffer(buffer_id);
		else if (cmd == FB_QUICKDRAW_UNLOCK_BUFFER)
			ret = fb_quickdraw_user_unlock_buffer(buffer_id);

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

	return ret;
}

/* Quickdraw Client Interface */

int fb_quickdraw_prepare(unsigned char panel_state)
{
	int ret;

	pr_debug("%s+\n", __func__);

	BUG_ON(!fb_quickdraw_ops);

	ret = fb_quickdraw_ops->prepare(fb_quickdraw_ops->data, panel_state);

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_execute(int buffer_id, int x, int y)
{
	int ret = -EINVAL;
	struct fb_quickdraw_buffer *buffer;

	pr_debug("%s+\n", __func__);

	BUG_ON(!fb_quickdraw_ops);

	buffer = fb_quickdraw_lookup_and_get_buffer(buffer_id);
	if (!buffer)
		goto exit;
	ret = setup_fd(buffer);
	if (ret >= 0)
		ret = fb_quickdraw_ops->execute(fb_quickdraw_ops->data, buffer,
			x, y);
	fb_quickdraw_put_buffer(buffer);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_erase(int x1, int y1, int x2, int y2)
{
	int ret;

	pr_debug("%s+\n", __func__);

	BUG_ON(!fb_quickdraw_ops);

	ret = fb_quickdraw_ops->erase(fb_quickdraw_ops->data, x1, y1, x2, y2);

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int fb_quickdraw_cleanup(void)
{
	int ret;

	pr_debug("%s+\n", __func__);

	BUG_ON(!fb_quickdraw_ops);

	ret = fb_quickdraw_ops->cleanup(fb_quickdraw_ops->data);

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

void fb_quickdraw_register_ops(struct fb_quickdraw_ops *ops)
{
	pr_debug("%s+\n", __func__);

	BUG_ON(fb_quickdraw_ops);

	fb_quickdraw_ops = ops;

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
MODULE_VERSION("1.0");

module_exit(fb_quickdraw_exit);
