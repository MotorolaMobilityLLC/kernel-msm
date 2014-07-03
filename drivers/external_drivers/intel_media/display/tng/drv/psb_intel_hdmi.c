/*
 * Copyright © 2006-2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	jim liu <jim.liu@intel.com>
 */

#include "psb_drv.h"
#include "psb_intel_hdmi.h"

/* start of S3D functions */

/**
 * Check if the HDMI display supports S3D. If so, report supported S3D formats
 *
 */
int mrfld_hdmi_s3d_query(struct drm_device *dev, struct drm_psb_s3d_query
			 *s3d_query)
{
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	struct mid_intel_hdmi_priv *hdmi_priv = dev_priv->hdmi_priv;

	if (!hdmi_priv) {
		DRM_ERROR("%s, HDMI is not initialized. \n", __FUNCTION__);
		return -EINVAL;
	}

	if (hdmi_priv->s3d_info.s3d_mandatory) {
		if ((s3d_query->mode_resolution_x == 1920) &&
		    (s3d_query->mode_resolution_y == 1080) &&
		    (s3d_query->mode_refresh_rate == 24)) {
			s3d_query->is_s3d_supported = 1;
			s3d_query->s3d_format = HDMI_3D_MANDATORY_1080P24;
		} else if ((s3d_query->mode_resolution_x == 1280) &&
			   (s3d_query->mode_resolution_y == 720)) {
			if ((s3d_query->mode_refresh_rate == 50) ||
			    (s3d_query->mode_refresh_rate == 60)) {
				s3d_query->is_s3d_supported = 1;
				s3d_query->s3d_format = HDMI_3D_MANDATORY_720P;
			}
		} else if ((s3d_query->mode_resolution_x == 1920) &&
			   (s3d_query->mode_resolution_y == 1080) &&
			   s3d_query->is_interleaving) {
			if ((s3d_query->mode_refresh_rate == 50) ||
			    (s3d_query->mode_refresh_rate == 60)) {
				s3d_query->is_s3d_supported = 1;
				s3d_query->s3d_format = HDMI_3D_MANDATORY_1080I;
			}
		}
	}

	return 0;
}
