/* Copyright (c) 2015, Motorola Mobility, LLC.
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

#include <linux/msm_mdp.h>
#include <linux/fb_quickdraw.h>
#include <linux/fb_quickdraw_ops.h>
#include <linux/vmalloc.h>

#include "mdss.h"
#include "mdss_dsi.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"

static int overlay_id;

/* Quickdraw Helper Functions */

static int set_overlay(struct msm_fb_data_type *mfd,
	struct fb_quickdraw_buffer *buffer)
{
	int ret;
	struct mdp_overlay overlay;

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		pr_err("%s: buffer is NULL\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (overlay_id != MSMFB_NEW_REQUEST) {
		pr_err("%s: overlay already set!\n", __func__);
		ret = -EBUSY;
		goto exit;
	}

	memset(&overlay, 0, sizeof(struct mdp_overlay));
	overlay.src.width  = buffer->data.rect.w;
	overlay.src.height = buffer->data.rect.h;
	overlay.src.format = buffer->data.format;
	overlay.z_order = 0;
	overlay.alpha = 0xff;
	overlay.flags = 0;
	overlay.is_fg = 0;
	overlay.id = MSMFB_NEW_REQUEST;
	overlay.pipe_type = PIPE_TYPE_AUTO;

	memcpy(&overlay.src_rect, &buffer->data.rect, sizeof(struct mdp_rect));
	memcpy(&overlay.dst_rect, &buffer->data.rect, sizeof(struct mdp_rect));

	ret = mdss_mdp_overlay_set(mfd, &overlay);
	if (ret) {
		pr_err("%s: error setting overlay for buffer %d\n",
			__func__, buffer->data.buffer_id);
		goto exit;
	}

	overlay_id = overlay.id;

exit:
	pr_debug("%s-: (buffer: %p) (ret: %d)\n", __func__, buffer,
		ret);

	return ret;
}

static int unset_overlay(struct msm_fb_data_type *mfd)
{
	int ret = -EINVAL;

	pr_debug("%s+: (overlay_id: %d)\n", __func__, overlay_id);

	if (overlay_id == MSMFB_NEW_REQUEST) {
		pr_err("%s: no overlay set!\n", __func__);
		goto exit;
	}

	ret = mdss_mdp_overlay_release_sub(mfd, overlay_id, true);
	overlay_id = MSMFB_NEW_REQUEST;

exit:
	pr_debug("%s-: (ret: %d)\n", __func__, ret);

	return ret;
}

static int play_overlay(struct msm_fb_data_type *mfd,
	struct fb_quickdraw_buffer *buffer)
{
	int ret = -EINVAL;
	struct msmfb_overlay_data ovdata;

	pr_debug("%s+: (buffer: %p)\n", __func__, buffer);

	if (!buffer) {
		pr_err("%s: buffer is NULL\n", __func__);
		goto exit;
	}

	if (overlay_id == MSMFB_NEW_REQUEST) {
		pr_err("%s: no overlay set!\n", __func__);
		goto exit;
	}

	memset(&ovdata, 0, sizeof(ovdata));
	ovdata.id = overlay_id;
	ovdata.data.flags = 0;
	ovdata.data.offset = 0;
	ovdata.data.memory_id = buffer->mem_fd;

	ret = mdss_mdp_overlay_play(mfd, &ovdata);

exit:
	pr_debug("%s-: (buffer: %p) (ret: %d)\n", __func__, buffer,
		ret);

	return ret;
}

static void mdss_dsi_cmd_mdp_busy_wait(struct msm_fb_data_type *mfd)
{
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl;

	pr_debug("%s+\n", __func__);

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("%s: Panel data not available\n", __func__);
		return;
	}

	while (pdata) {
		ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
					panel_data);

		mdss_dsi_cmd_mdp_busy(ctrl);

		pdata = pdata->next;
	}

	pr_debug("%s-\n", __func__);
}

/* Quickdraw External Interface */

static int mdss_quickdraw_prepare(void *data, unsigned char panel_state)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)(data);
	struct mdss_panel_data *pdata = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret = 0;

	pr_debug("%s+ (panel_state: %d)\n", __func__, panel_state);

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("%s: Panel data not available\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	mfd->quickdraw_panel_state = panel_state;
	mfd->quickdraw_in_progress = 1;
	mfd->quickdraw_reset_panel = false;

	mdss_fb_blank_sub(FB_BLANK_UNBLANK, mfd->fbi, true);

	ctl = mfd_to_ctl(mfd);

	if (mfd->quickdraw_reset_panel) {
		pdata->panel_info.panel_dead = true;
		mfd->quickdraw_in_progress = 0;
		mdss_fb_blank_sub(FB_BLANK_POWERDOWN, mfd->fbi, true);
		mdss_fb_blank_sub(FB_BLANK_UNBLANK, mfd->fbi, true);
		mfd->quickdraw_in_progress = 1;
		ret = QUICKDRAW_ESD_RECOVERED;
	}
exit:
	pr_debug("%s- (quickdraw_reset_panel: %d)\n", __func__,
		mfd->quickdraw_reset_panel);

	return ret;
}

static int mdss_quickdraw_execute(void *data,
	struct fb_quickdraw_buffer *buffer)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)(data);
	struct mdp_display_commit prim_commit;
	int ret = 0;

	pr_debug("%s+ (id: %d)\n", __func__, buffer->data.buffer_id);

	/* Make sure dsi link is idle */
	mdss_dsi_cmd_mdp_busy_wait(mfd);

	ret = set_overlay(mfd, buffer);
	if (ret) {
		pr_err("%s: error setting up overlay, cleanup\n", __func__);
		goto exit;
	}

	ret = play_overlay(mfd, buffer);
	if (ret) {
		pr_err("%s: error playing overlay, cleanup\n", __func__);
		goto exit_unset;
	}

	memset(&prim_commit, 0, sizeof(struct mdp_display_commit));
	prim_commit.flags = MDP_DISPLAY_COMMIT_OVERLAY;
	prim_commit.wait_for_finish = 1;
	prim_commit.l_roi.w = mfd->fbi->var.xres;
	prim_commit.l_roi.h = mfd->fbi->var.yres;

	mdss_fb_pan_display_ex(mfd->fbi, &prim_commit);

	/* Make sure dsi link is idle */
	mdss_dsi_cmd_mdp_busy_wait(mfd);

exit_unset:
	unset_overlay(mfd);

exit:
	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int mdss_quickdraw_cleanup(void *data)
{
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)(data);
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	LIST_HEAD(destroy_pipes);
	int ret = 0;

	pr_debug("%s+\n", __func__);

	list_splice_init(&mdp5_data->pipes_cleanup, &destroy_pipes);

	mdss_mdp_overlay_cleanup(mfd, &destroy_pipes);

	ret = mdss_fb_blank_sub(FB_BLANK_POWERDOWN, mfd->fbi, true);

	mfd->quickdraw_in_progress = 0;

	pr_debug("%s- (ret: %d)\n", __func__, ret);

	return ret;
}

static int mdss_quickdraw_format2bpp(void *data, int format)
{
	switch (format) {
	case MDP_RGB_565:
		return 2;
	case MDP_RGB_888:
		return 3;
	case MDP_RGBX_8888:
		return 4;
	default:
		pr_err("%s: Unsupported format: %d\n", __func__, format);
		return -EINVAL;
	}
}

static struct fb_quickdraw_ops mdss_quickdraw_ops = {
	.prepare = mdss_quickdraw_prepare,
	.execute = mdss_quickdraw_execute,
	.cleanup = mdss_quickdraw_cleanup,
	.format2bpp = mdss_quickdraw_format2bpp,
};

static struct fb_quickdraw_impl_data mdss_quickdraw_impl_data = {
	.ops = &mdss_quickdraw_ops,
	.ion_heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID),
};

void mdss_quickdraw_register(struct msm_fb_data_type *mfd)
{
	struct mdss_data_type *mdata;

	BUG_ON(!mfd);

	mdata = mfd_to_mdata(mfd);

	mdss_quickdraw_ops.data = (void *)mfd;
	mdss_quickdraw_impl_data.fbi = mfd->fbi;
	mdss_quickdraw_impl_data.iclient = mdata->iclient;

	overlay_id = MSMFB_NEW_REQUEST;

	fb_quickdraw_register_impl(&mdss_quickdraw_impl_data);
}
