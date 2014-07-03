/*
 * Copyright (c) 2008, Intel Corporation
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
 *      Eric Anholt <eric@anholt.net>
 *
 */

#ifndef _PSB_FB_H_
#define _PSB_FB_H_

#include <linux/version.h>
#include <drm/drmP.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
#include <drm/drm_fb_helper.h>
#endif

#include "psb_drv.h"

#include "displayclass_interface.h"

extern struct psb_framebuffer *psbfb;
int MRSTLFBHandleChangeFB(struct drm_device *dev,
			  struct psb_framebuffer *psbfb);

struct psb_fbdev {
	struct drm_fb_helper psb_fb_helper;
	struct psb_framebuffer *pfb;
};

#define to_psb_fb(x) container_of(x, struct psb_framebuffer, base)

extern int psb_intel_connector_clones(struct drm_device *dev, int type_mask);

#endif
