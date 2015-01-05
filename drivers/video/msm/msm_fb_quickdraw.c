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

#include <linux/delay.h>
#include <linux/msm_mdp.h>
#include <linux/msp430.h>
#include <linux/syscalls.h>

#include "mdp.h"
#include "mipi_mot.h"
#include "msm_fb.h"
#include "mdp4.h"
#include "msm_fb_quickdraw.h"
#include <mach/iommu_domains.h>

#define ERASE_BUFFER_ID -1
#define COORD_NO_OVERRIDE -1

static struct msp430_quickdraw_ops msm_fb_quickdraw_ops;
static LIST_HEAD(msm_fb_quickdraw_buffer_list_head);
static DEFINE_MUTEX(list_lock);
static u32 saved_panel_xres;
static u32 saved_panel_yres;
static struct msmfb_quickdraw_buffer *active_buffer;

/* Quickdraw Helper Functions */

static void clip_rect(int panel_xres, int panel_yres, int x, int y,
		      int w, int h, struct mdp_rect *src_rect,
		      struct mdp_rect *dst_rect)
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

static inline int check_alignment(int value, int align)
{
	return value % align;
}

static int correct_alignment(int coord, int align)
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

/* This function creates a new file descriptor for use in the suspend
   context. This is necessary for permissions issues since the buffers
   were allocated in the app context. */
static int setup_fd(struct msmfb_quickdraw_buffer *buffer)
{
	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (buffer->file && buffer->mem_fd == -1) {
		int fd = get_unused_fd();
		if (fd < 0)
			pr_err("%s: unable to get fd (ret: %d)\n",
				__func__, fd);
		else {
			buffer->mem_fd = fd;
			fd_install(buffer->mem_fd, buffer->file);
		}
	}

	pr_debug("%s-: (buffer: %p) (fd: %d)\n", __func__, buffer,
		buffer->mem_fd);

	return buffer->mem_fd;
}

static int set_overlay(struct msm_fb_data_type *mfd,
	struct msmfb_quickdraw_buffer *buffer, int x, int y)
{
	int ret;
	struct mdp_overlay overlay;

	pr_debug("%s+: (buffer: %p) [x:%d, y:%d]\n", __func__, buffer, x, y);

	if (!buffer) {
		pr_err("%s: buffer is NULL\n", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	memset(&overlay, 0, sizeof(struct mdp_overlay));
	overlay.src.width  = buffer->data.w;
	overlay.src.height = buffer->data.h;
	overlay.src.format = buffer->data.format;
	overlay.z_order = 0;
	overlay.alpha = 0xff;
	overlay.flags = 0;
	overlay.is_fg = 0;
	overlay.id = buffer->overlay_id;

	clip_rect(saved_panel_xres, saved_panel_yres, x, y, buffer->data.w,
		buffer->data.h, &overlay.src_rect, &overlay.dst_rect);
	/* always start draw at [0,0] of resized window */
	overlay.dst_rect.x = 0;
	overlay.dst_rect.y = 0;

	ret = mdp4_overlay_set(mfd->fbi, &overlay);
	if (ret) {
		pr_err("%s: error setting overlay for buffer %d\n", __func__,
			buffer->data.buffer_id);
		goto EXIT;
	}

	buffer->overlay_id = overlay.id;
EXIT:
	pr_debug("%s-: (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static int unset_overlay(struct msm_fb_data_type *mfd,
	struct msmfb_quickdraw_buffer *buffer)
{
	int ret = -EINVAL;

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (buffer) {
		/* The erase buffer doesnt have a file,
		   so it doesnt use overlays */
		if (!buffer->file) {
			ret = 0;
			goto EXIT;
		}

		if (buffer->overlay_id != MSMFB_NEW_REQUEST) {
			ret = mdp4_overlay_unset(mfd->fbi, buffer->overlay_id);
			buffer->overlay_id = MSMFB_NEW_REQUEST;
		} else {
			pr_err("%s: invalid buffer (overlay.id = MSMFB_NEW_REQUEST)!\n",
				__func__);
		}
	} else {
		pr_err("%s: NULL buffer!\n", __func__);
	}
EXIT:
	pr_debug("%s-: (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static int play_overlay(struct msm_fb_data_type *mfd,
	struct msmfb_quickdraw_buffer *buffer)
{
	int ret;
	struct msmfb_overlay_data ovdata;

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		pr_err("%s: buffer is NULL\n", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	ovdata.id = buffer->overlay_id;
	ovdata.data.flags = 0;
	ovdata.data.offset = 0;
	ovdata.data.memory_id = buffer->mem_fd;

	ret = msmfb_overlay_play_sub(mfd->fbi, &ovdata);
EXIT:
	pr_debug("%s-: (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

static void delete_buffer_fd(struct work_struct *work)
{
	struct msmfb_quickdraw_buffer *buffer =
		container_of(work, struct msmfb_quickdraw_buffer, delete_work);

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
	struct msmfb_quickdraw_buffer *buffer =
		container_of(kref, struct msmfb_quickdraw_buffer, kref);
	struct msm_fb_data_type *mfd =
		(struct msm_fb_data_type *)msm_fb_quickdraw_ops.data;

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (buffer->overlay_id != MSMFB_NEW_REQUEST)
		unset_overlay(mfd, buffer);
	schedule_work(&buffer->delete_work);

	pr_debug("%s-: (buffer: %p)\n", __func__, buffer);
}

static struct msmfb_quickdraw_buffer *get_buffer(int buffer_id)
{
	struct msmfb_quickdraw_buffer *buffer = NULL, *cur = NULL;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	mutex_lock(&list_lock);

	list_for_each_entry(cur, &msm_fb_quickdraw_buffer_list_head, list) {
		if (cur->data.buffer_id == buffer_id) {
			buffer = cur;
			kref_get(&buffer->kref);
			break;
		}
	}

	mutex_unlock(&list_lock);

	pr_debug("%s- (buffer: %p)\n", __func__, buffer);

	return buffer;
}

static int put_buffer(struct msmfb_quickdraw_buffer *buffer)
{
	int deleted = 0;

	pr_debug("%s+ (buffer: %p)\n", __func__, buffer);

	if (buffer)
		deleted = kref_put(&buffer->kref,
			delete_buffer);

	pr_debug("%s- (buffer: %p) (deleted: %d)\n", __func__, buffer, deleted);

	return deleted;
}

static int lock_buffer(struct msmfb_quickdraw_buffer *buffer)
{
	int ret = -EINVAL;

	pr_debug("%s+\n", __func__);

	if (!buffer) {
		ret = -ENOENT;
		goto exit;
	}
	ret = wait_event_interruptible(buffer->wait_queue,
		!atomic_cmpxchg(&buffer->locked, 0, 1));

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int unlock_buffer(struct msmfb_quickdraw_buffer *buffer)
{
	int ret = -EINVAL;

	pr_debug("%s+\n", __func__);

	if (!buffer) {
		ret = -ENOENT;
		goto exit;
	}
	atomic_set(&buffer->locked, 0);
	wake_up_all(&buffer->wait_queue);

	ret = 0;

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

/* Quickdraw Userspace Interface */

int msm_fb_quickdraw_init(void)
{
	struct msmfb_quickdraw_buffer *buffer = NULL;
	struct list_head *entry, *temp;
	struct msmfb_quickdraw_buffer_data buffer_data;

	pr_debug("%s+\n", __func__);

	mutex_lock(&list_lock);

	list_for_each_safe(entry, temp, &msm_fb_quickdraw_buffer_list_head) {
		buffer = list_entry(entry, struct msmfb_quickdraw_buffer, list);
		list_del(&buffer->list);
		kref_put(&buffer->kref, delete_buffer);
	}

	mutex_unlock(&list_lock);

	/* Allocate the special erase buffer */
	memset(&buffer_data, 0, sizeof(struct msmfb_quickdraw_buffer_data));
	buffer_data.buffer_id = ERASE_BUFFER_ID;
	buffer_data.user_fd = -1;
	msm_fb_quickdraw_create_buffer(&buffer_data);

	pr_debug("%s-\n", __func__);

	return 0;
}

int msm_fb_quickdraw_create_buffer(struct msmfb_quickdraw_buffer_data *data)
{
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	struct msmfb_quickdraw_buffer *buffer;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!data) {
		pr_err("%s: Buffer data null\n", __func__);
		ret = -EINVAL;
		goto EXIT;
	} else if ((data->buffer_id != ERASE_BUFFER_ID) &&
		   (data->w <= 0 || data->h <= 0)) {
		pr_err("%s: Buffer width[%d]/height[%d] invalid\n",
			__func__, data->w, data->h);
		ret = -EINVAL;
		goto EXIT;
	}

	if (check_alignment(data->w, mot_panel->pinfo.col_align)) {
		pr_err("%s: Buffer [id: %d] width [%d] not aligned [%d]\n",
			__func__, data->buffer_id, data->w,
			mot_panel->pinfo.col_align);
		ret = -EINVAL;
		goto EXIT;
	}
	data->x = correct_alignment(data->x, mot_panel->pinfo.col_align);

	mutex_lock(&list_lock);

	list_for_each_entry(buffer, &msm_fb_quickdraw_buffer_list_head, list) {
		if (buffer->data.buffer_id == data->buffer_id) {
			pr_err("%s: Duplicate buffer_id: %d\n", __func__,
				data->buffer_id);
			ret = -EEXIST;
			goto EXIT;
		}
	}

	buffer = vzalloc(sizeof(struct msmfb_quickdraw_buffer));
	if (buffer) {
		memcpy(&buffer->data, data,
			sizeof(struct msmfb_quickdraw_buffer_data));
		init_waitqueue_head(&buffer->wait_queue);
		if (buffer->data.user_fd >= 0) {
			buffer->file = fget(buffer->data.user_fd);
			if (!buffer->file) {
				pr_err("%s: Unable to get the file\n",
					__func__);
				vfree(buffer);
				ret = -EPERM;
				goto EXIT;
			}
		}
		buffer->mem_fd = -1;
		buffer->overlay_id = MSMFB_NEW_REQUEST;
		INIT_WORK(&buffer->delete_work, delete_buffer_fd);
		kref_init(&buffer->kref);
		list_add_tail(&buffer->list,
			&msm_fb_quickdraw_buffer_list_head);
	} else {
		pr_err("%s: vzalloc failed!\n", __func__);
		ret = -ENOMEM;
	}
EXIT:
	mutex_unlock(&list_lock);

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int msm_fb_quickdraw_destroy_buffer(int buffer_id)
{
	struct msmfb_quickdraw_buffer *buffer = NULL, *cur = NULL;
	int ret = -EINVAL;

	pr_debug("%s+ (id: %d)\n", __func__, buffer_id);

	mutex_lock(&list_lock);

	list_for_each_entry(cur, &msm_fb_quickdraw_buffer_list_head, list) {
		if (cur->data.buffer_id == buffer_id) {
			buffer = cur;
			list_del(&buffer->list);
			break;
		}
	}

	mutex_unlock(&list_lock);

	if (buffer)
		ret = kref_put(&buffer->kref, delete_buffer);
	else
		pr_err("%s: No buffer found with ID: %d\n", __func__,
			buffer_id);

	pr_debug("%s- (buffer: %p) (ret: %d)\n", __func__, buffer, ret);

	return ret;
}

int msm_fb_quickdraw_lock_buffer(int buffer_id)
{
	int ret = -EINVAL;
	struct msmfb_quickdraw_buffer *buffer;

	pr_debug("%s+\n", __func__);

	buffer = get_buffer(buffer_id);
	ret = lock_buffer(buffer);
	put_buffer(buffer);

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

int msm_fb_quickdraw_unlock_buffer(int buffer_id)
{
	int ret = -EINVAL;
	struct msmfb_quickdraw_buffer *buffer;

	pr_debug("%s+\n", __func__);

	buffer = get_buffer(buffer_id);
	ret = unlock_buffer(buffer);
	put_buffer(buffer);

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

/* Quickdraw External Interface */

static int msm_fb_quickdraw_prepare(void *data, unsigned char panel_state)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)(data);
	int ret = 0;

	pr_debug("%s+\n", __func__);

	mfd->quickdraw_panel_state = panel_state;
	mfd->quickdraw_in_progress = 1;
	mfd->quickdraw_esd_recovered = 0;

	saved_panel_xres = mfd->panel_info.xres;
	saved_panel_yres = mfd->panel_info.yres;

	mfd->quickdraw_mdp_resume();
	mfd->quickdraw_fb_resume(mfd);

	if (mfd->quickdraw_esd_recovered)
		ret = QUICKDRAW_ESD_RECOVERED;

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int msm_fb_quickdraw_execute(void *data, int buffer_id, int x, int y)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)(data);
	struct mdp_display_commit prim_commit;
	struct mdp_rect window_size;
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	struct msm_fb_panel_data *pdata =
		(struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
	int w, h;
	int ret = 0;
	struct msmfb_quickdraw_buffer *buffer = NULL;

	pr_debug("%s+ buffer [%d] (x:%d, y:%d)\n", __func__, buffer_id, x, y);

	buffer = get_buffer(buffer_id);
	if (!buffer) {
		pr_err("%s: Unknown buffer [%d]\n", __func__, buffer_id);
		ret = -EINVAL;
		goto EXIT;
	}

	if (x == COORD_NO_OVERRIDE)
		x = buffer->data.x;
	if (y == COORD_NO_OVERRIDE)
		y = buffer->data.y;

	x = correct_alignment(x, mfd->panel_info.col_align);
	w = buffer->data.w;
	h = buffer->data.h;

	clip_rect(saved_panel_xres, saved_panel_yres, x, y, w, h, NULL,
		&window_size);

	if ((window_size.w == 0) || (window_size.h == 0)) {
		pr_debug("%s: Draw skipped, buffer off-screen [x:%d y:%d w:%d h:%d]\n",
			__func__, x, y, w, h);
		put_buffer(buffer);
		goto EXIT;
	}

	/* Make sure dsi link is idle */
	mipi_dsi_mdp_busy_wait();

	/* Unlock previous buffer */
	if (active_buffer)
		unlock_buffer(active_buffer);

	lock_buffer(buffer);

	mfd->panel_info.xres = window_size.w;
	mfd->fbi->var.xres = window_size.w;
	mfd->panel_info.yres = window_size.h;
	mfd->fbi->var.yres = window_size.h;

	mdp4_overlay_update_dsi_cmd(mfd);

	mot_panel->set_partial_window(mfd,
		window_size.x, window_size.y,
		window_size.w, window_size.h);

	pdata->set_mdp_stream_params(mfd->pdev,
		window_size.w, window_size.h);

	if (buffer->file) {
		ret = set_overlay(mfd, buffer, x, y);

		if (!ret) {
			ret = setup_fd(buffer);
			if (ret < 0)
				unset_overlay(mfd, buffer);
			else
				ret = play_overlay(mfd, buffer);
		}
	}

	/* Unset the previous overlay now that we have a new pipe */
	if (active_buffer) {
		if (active_buffer != buffer)
			unset_overlay(mfd, active_buffer);
		/* Free the previous buffer if we're done with it */
		put_buffer(active_buffer);
	}
	active_buffer = buffer;

	/* If we had errors earlier, we need to cleanup */
	if (ret) {
		pr_err("%s: error setting up overlay, cleanup\n", __func__);
		unlock_buffer(buffer);
		put_buffer(buffer);
		active_buffer = NULL;
		goto EXIT;
	}

	memset(&prim_commit, 0, sizeof(struct mdp_display_commit));
	prim_commit.flags = MDP_DISPLAY_COMMIT_OVERLAY;
	prim_commit.wait_for_finish = 1;

	msm_fb_pan_display_ex(mfd->fbi, &prim_commit);

EXIT:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int msm_fb_quickdraw_erase(void *data, int x1, int y1, int x2, int y2)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)data;
	struct msmfb_quickdraw_buffer *buffer = NULL;
	int w = x2 - x1;
	int h = y2 - y1;
	int ret;

	pr_debug("%s+ (x1:%d, y1:%d) (x2:%d, y2:%d)\n", __func__,
		x1, y1, x2, y2);

	if (w <= 0 || h <= 0) {
		pr_err("%s: Erase width/height invalid (w:%d, h:%d) (x1:%d,y1:%d)(x2:%d,y2:%d)\n",
		       __func__, w, h, x1, y1, x2, y2);
		ret = -EINVAL;
		goto EXIT;
	}

	if (check_alignment(w, mfd->panel_info.col_align)) {
		pr_err("%s: Erase width [%d] not aligned [%d]\n", __func__, w,
		       mfd->panel_info.col_align);
		ret = -EINVAL;
		goto EXIT;
	}
	x1 = correct_alignment(x1, mfd->panel_info.col_align);
	x2 = correct_alignment(x2, mfd->panel_info.col_align);

	buffer = get_buffer(ERASE_BUFFER_ID);
	if (!buffer) {
		pr_err("%s: Unable to find erase buffer\n", __func__);
		ret = -EINVAL;
		goto EXIT;
	}

	buffer->data.x = x1;
	buffer->data.y = y1;
	buffer->data.w = w;
	buffer->data.h = h;

	put_buffer(buffer);

	ret = msm_fb_quickdraw_execute(data, ERASE_BUFFER_ID,
					COORD_NO_OVERRIDE, COORD_NO_OVERRIDE);

EXIT:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int msm_fb_quickdraw_cleanup(void *data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)(data);
	struct msm_fb_panel_data *pdata =
		(struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	int ret = 0;

	pr_debug("%s+\n", __func__);

	/* Make sure dsi link is idle */
	mipi_dsi_mdp_busy_wait();

	/* Free the last used buffer */
	if (active_buffer) {
		unset_overlay(mfd, active_buffer);
		unlock_buffer(active_buffer);
		put_buffer(active_buffer);
	}
	active_buffer = NULL;

	mfd->panel_info.xres = saved_panel_xres;
	mfd->fbi->var.xres = saved_panel_xres;
	mfd->panel_info.yres = saved_panel_yres;
	mfd->fbi->var.yres = saved_panel_yres;

	pdata->set_mdp_stream_params(mfd->pdev,
		mfd->panel_info.xres,
		mfd->panel_info.yres);

	mot_panel->set_full_window(mfd);

	mfd->quickdraw_fb_suspend(mfd);
	mfd->quickdraw_mdp_suspend();

	/* Just in case we had to bring the panel out of sleep */
	mipi_mot_exit_sleep_wait();

	mfd->quickdraw_in_progress = 0;

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static struct msp430_quickdraw_ops msm_fb_quickdraw_ops = {
	.prepare = msm_fb_quickdraw_prepare,
	.execute = msm_fb_quickdraw_execute,
	.erase   = msm_fb_quickdraw_erase,
	.cleanup = msm_fb_quickdraw_cleanup,
};

void msm_fb_quickdraw_register_notifier(struct msm_fb_data_type *mfd)
{
	msm_fb_quickdraw_ops.data = (void *) mfd;
	msp430_register_quickdraw(&msm_fb_quickdraw_ops);
}
