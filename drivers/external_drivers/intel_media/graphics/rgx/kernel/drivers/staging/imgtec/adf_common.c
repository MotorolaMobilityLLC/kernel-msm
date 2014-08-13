/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
/* vi: set ts=8: */

#include "adf_common.h"

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>

#include <video/adf_client.h>

#ifdef DEBUG_VALIDATE
#define val_dbg(dev, fmt, x...) dev_dbg(dev, fmt, x)
#else
#define val_dbg(dev, fmt, x...) do { } while(0)
#endif

long adf_img_ioctl_validate(struct adf_device *dev,
			    struct adf_validate_config_ext __user *arg)
{
	struct adf_interface **intfs = NULL;
	struct adf_buffer *bufs = NULL;
	struct adf_post post_cfg;
	void *driver_state;
	int err = 0;
	size_t i, j;

	if (!access_ok(VERIFY_READ, arg,
		       sizeof(struct adf_validate_config_ext))) {
		err = -EFAULT;
		goto err_out;
	}

	if (!access_ok(VERIFY_READ, arg->bufs,
		       sizeof(struct adf_buffer_config))) {
		err = -EFAULT;
		goto err_out;
	}

	if (!access_ok(VERIFY_READ, arg->bufs_ext,
		       sizeof(struct adf_buffer_config_ext))) {
		err = -EFAULT;
		goto err_out;
	}

	if (arg->n_interfaces > ADF_MAX_INTERFACES) {
		err = -EINVAL;
		goto err_out;
	}

	if (arg->n_bufs > ADF_MAX_BUFFERS) {
		err = -EINVAL;
		goto err_out;
	}

	if (arg->n_interfaces) {
		intfs = kmalloc(sizeof(intfs[0]) * arg->n_interfaces,
				GFP_KERNEL);
		if (!intfs) {
			err = -ENOMEM;
			goto err_out;
		}
	}

	for (i = 0; i < arg->n_interfaces; i++) {
		intfs[i] = idr_find(&dev->interfaces, arg->interfaces[i]);
		if (!intfs[i]) {
			err = -EINVAL;
			goto err_out;
		}
	}

	if (arg->n_bufs) {
		bufs = kzalloc(sizeof(bufs[0]) * arg->n_bufs, GFP_KERNEL);
		if (!bufs) {
			err = -ENOMEM;
			goto err_out;
		}
	}

	for (i = 0; i < arg->n_bufs; i++) {
		struct adf_buffer_config *config = &arg->bufs[i];

		memset(&bufs[i], 0, sizeof(bufs[i]));

		if (config->n_planes > ADF_MAX_PLANES) {
			err = -EINVAL;
			goto err_import;
		}

		bufs[i].overlay_engine = idr_find(&dev->overlay_engines,
						  config->overlay_engine);
		if (!bufs[i].overlay_engine) {
			err = -ENOENT;
			goto err_import;
		}

		bufs[i].w = config->w;
		bufs[i].h = config->h;
		bufs[i].format = config->format;

		for (j = 0; j < config->n_planes; j++) {
			bufs[i].dma_bufs[j] = dma_buf_get(config->fd[j]);
			if (IS_ERR_OR_NULL(bufs[i].dma_bufs[j])) {
				err = PTR_ERR(bufs[i].dma_bufs[j]);
				bufs[i].dma_bufs[j] = NULL;
				goto err_import;
			}
			bufs[i].offset[j] = config->offset[j];
			bufs[i].pitch[j] = config->pitch[j];
		}
		bufs[i].n_planes = config->n_planes;

		bufs[i].acquire_fence = NULL;
	}

	/* Fake up a post configuration to validate */
	post_cfg.custom_data_size = sizeof(struct adf_buffer_config_ext);
	post_cfg.custom_data = arg->bufs_ext;
	post_cfg.n_bufs = arg->n_bufs;
	post_cfg.bufs = bufs;

	/* Mapping dma bufs is too expensive for validate, and we don't
	 * need to do it at the moment.
	 */
	post_cfg.mappings = NULL;

	err = dev->ops->validate(dev, &post_cfg, &driver_state);
	if (err)
		goto err_import;

	/* For the validate ioctl, we don't need the driver state. If it
	 * was allocated, free it immediately.
	 */
	if (dev->ops->state_free)
		dev->ops->state_free(dev, driver_state);

err_import:
	for (i = 0; i < arg->n_bufs; i++)
		for (j = 0; j < ARRAY_SIZE(bufs[i].dma_bufs); j++)
			if (bufs[i].dma_bufs[j])
				dma_buf_put(bufs[i].dma_bufs[j]);
err_out:
	kfree(intfs);
	kfree(bufs);
	return err;
}

long adf_img_ioctl(struct adf_obj *obj, unsigned int cmd, unsigned long arg)
{
	struct adf_device *dev =
		(struct adf_device *)obj->parent;

	switch (cmd) {
	case ADF_VALIDATE_CONFIG_EXT:
		return adf_img_ioctl_validate(dev,
			(struct adf_validate_config_ext __user *)arg);
	}

	return -ENOTTY;
}

/* Callers of this function should have taken the dev->client_lock */

static struct adf_interface *
get_interface_attached_to_overlay(struct adf_device *dev,
				  struct adf_overlay_engine *overlay)
{
	struct adf_interface *interface = NULL;
	struct adf_attachment_list *entry;

	/* We are open-coding adf_attachment_list_to_array. We can't use the
	 * adf_device_attachments helper because it takes the client lock,
	 * which is already held for calls to validate.
	 */
	list_for_each_entry(entry, &dev->attached, head) {
		/* If there are multiple interfaces attached to an overlay,
		 * this will return the last.
		 */
		if (entry->attachment.overlay_engine == overlay)
			interface = entry->attachment.interface;
	}

	return interface;
}

int adf_img_validate_simple(struct adf_device *dev, struct adf_post *cfg,
			    void **driver_state)
{
	struct adf_buffer_config_ext *buf_ext = cfg->custom_data;
	struct adf_overlay_engine *overlay;
	struct adf_interface *interface;
	struct adf_buffer *buffer;
	int i = 0;
	struct device *device = dev->dev;

	/* "Null" flip handling */
	if (cfg->n_bufs == 0)
		return 0;

	if (cfg->custom_data_size != sizeof(struct adf_buffer_config_ext)) {
		val_dbg(device, "Custom data size %zu not expected size %zu",
			 cfg->custom_data_size,
			 sizeof(struct adf_buffer_config_ext));
		return -EINVAL;
	}

	if (cfg->n_bufs != 1) {
		val_dbg(device, "Got %zu buffers in post. Should be 1.\n",
			cfg->n_bufs);
		return -EINVAL;
	}

	buffer = &cfg->bufs[0];
	overlay = buffer->overlay_engine;
	if (!overlay) {
		dev_err(device, "Buffer without an overlay engine.\n");
		return -EINVAL;
	}

	for (i = 0; i < overlay->ops->n_supported_formats; i++) {
		if (buffer->format == overlay->ops->supported_formats[i])
			break;
	}

	if (i == overlay->ops->n_supported_formats) {
		char req_format_str[ADF_FORMAT_STR_SIZE];

		adf_format_str(buffer->format, req_format_str);

		val_dbg(device, "Unsupported buffer format %s.\n", req_format_str);
		return -EINVAL;
	}

	interface = get_interface_attached_to_overlay(dev, overlay);
	if (!interface) {
		dev_err(device, "No interface attached to overlay\n");
		return -EINVAL;
	}

	if (buffer->w != interface->current_mode.hdisplay) {
		val_dbg(device, "Buffer width %u is not expected %u.\n",
			 buffer->w, interface->current_mode.hdisplay);
		return -EINVAL;
	}

	if (buffer->h != interface->current_mode.vdisplay) {
		val_dbg(device, "Buffer height %u is not expected %u.\n",
			 buffer->h, interface->current_mode.vdisplay);
		return -EINVAL;
	}

	if (buffer->n_planes != 1) {
		val_dbg(device, "Buffer n_planes %u is not 1.\n", buffer->n_planes);
		return -EINVAL;
	}

	if (buffer->offset[0] != 0) {
		val_dbg(device, "Buffer offset %u is not 0.\n", buffer->offset[0]);
		return -EINVAL;
	}

	if (buf_ext->crop.x1 != 0 ||
		buf_ext->crop.y1 != 0 ||
		buf_ext->crop.x2 != interface->current_mode.hdisplay ||
		buf_ext->crop.y2 != interface->current_mode.vdisplay) {
		val_dbg(device, "Buffer crop {%u,%u,%u,%u} not expected {%u,%u,%u,%u}.\n",
			 buf_ext->crop.x1, buf_ext->crop.y1,
			 buf_ext->crop.x2, buf_ext->crop.y2,
			 0, 0, interface->current_mode.hdisplay,
			 interface->current_mode.vdisplay);
		return -EINVAL;
	}

	if (buf_ext->display.x1 != 0 ||
		buf_ext->display.y1 != 0 ||
		buf_ext->display.x2 != interface->current_mode.hdisplay ||
		buf_ext->display.y2 != interface->current_mode.vdisplay) {
		val_dbg(device, "Buffer display {%u,%u,%u,%u} not expected {%u,%u,%u,%u}.\n",
			 buf_ext->display.x1, buf_ext->display.y1,
			 buf_ext->display.x2, buf_ext->display.y2,
			 0, 0, interface->current_mode.hdisplay,
			 interface->current_mode.vdisplay);
		return -EINVAL;
	}

	if (buf_ext->transform != ADF_BUFFER_TRANSFORM_NONE_EXT) {
		val_dbg(device, "Buffer transform 0x%x not expected transform 0x%x.\n",
			 buf_ext->transform, ADF_BUFFER_TRANSFORM_NONE_EXT);
		return -EINVAL;
	}

	if (buf_ext->blend_type != ADF_BUFFER_BLENDING_PREMULT_EXT) {
		val_dbg(device, "Buffer blend type %u not expected blend type %u.\n",
			 buf_ext->blend_type, ADF_BUFFER_BLENDING_PREMULT_EXT);
		return -EINVAL;
	}

	if (buf_ext->plane_alpha != 0xff) {
		val_dbg(device, "Buffer plane alpha %u not expected plane alpha %u.\n",
			 buf_ext->plane_alpha, 0xff);
		return -EINVAL;
	}

	return 0;
}
