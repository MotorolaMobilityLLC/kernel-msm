/*****************************************************************************
 *
 * Copyright © 2010 Intel Corporation
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
 ******************************************************************************/

#ifndef __DC_INTERFACE_H__
#define __DC_INTERFACE_H__

/*NOTE: this file is exported to user mode*/

#ifdef __KERNEL__

#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include "ttm/ttm_bo_driver.h"
#include <ttm/ttm_bo_api.h>

/*
 * TODO: remove these macros as we don't need swapchains anymore
 */
#define PVRSRV_SWAPCHAIN_ATTACHED_PLANE_NONE (0 << 0)
#define PVRSRV_SWAPCHAIN_ATTACHED_PLANE_A    (1 << 0)
#define PVRSRV_SWAPCHAIN_ATTACHED_PLANE_B    (1 << 1)
#define PVRSRV_SWAPCHAIN_ATTACHED_PLANE_C    (1 << 2)

/*
 * TODO: move this definition back to psb_fb.h
 * This is NOT a part of IMG display class
 */
struct psb_framebuffer {
	struct drm_framebuffer base;
	struct address_space *addr_space;
	struct ttm_buffer_object *bo;
	struct fb_info *fbdev;
	uint32_t tt_pages;
	uint32_t stolen_base;
	void *vram_addr;
	/* struct ttm_bo_kmap_obj kmap; */
	void *hKernelMemInfo;
	uint32_t depth;
	uint32_t size;
	uint32_t offset;
	uint32_t user_virtual_addr;  /* user space address */
};

#endif /*__KERNEL__*/

void DCAttachPipe(uint32_t uiPipe);
void DCUnAttachPipe(uint32_t uiPipe);
void DC_MRFLD_onPowerOn(uint32_t iPipe);
void DC_MRFLD_onPowerOff(uint32_t iPipe);
int DC_MRFLD_Enable_Plane(int type, int index, uint32_t ctx);
int DC_MRFLD_Disable_Plane(int type, int index, uint32_t ctx);
bool DC_MRFLD_Is_Plane_Disabled(int type, int index, uint32_t ctx);
void DCLockMutex(void);
void DCUnLockMutex(void);
int DCUpdateCursorPos(uint32_t pipe, uint32_t pos);

#endif				/* __DC_INTERFACE_H__ */
