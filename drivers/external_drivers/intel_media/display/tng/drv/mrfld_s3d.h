/*
 * Copyright (c) 2011, Intel Corporation.
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
 *	jim liu <jim.liu@intel.com>
 */

#ifndef MRFLD_S3D_H
#define MRFLD_S3D_H

int mrfld_s3d_query(struct drm_device *dev, struct drm_psb_s3d_query
		    *s3d_query);
int mrfld_hdmi_s3d_query(struct drm_device *dev, struct drm_psb_s3d_query
			 *s3d_query);
#endif				/* MRFLD_S3D_H */
