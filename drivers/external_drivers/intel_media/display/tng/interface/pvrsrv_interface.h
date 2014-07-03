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

#if !defined(__PVR_DRM_EXPORT_H__)
#define __PVR_DRM_EXPORT_H__

int PVRSRVOpen(struct drm_device *dev, struct drm_file *pFile);
int PVRSRVDrmLoad(struct drm_device *dev, unsigned long flags);
int PVRSRVDrmUnload(struct drm_device *dev);
void PVRSRVDrmPostClose(struct drm_device *dev, struct drm_file *file);
void PVRSRVQueryIoctls(struct drm_ioctl_desc *ioctls);

unsigned int PVRSRVGetMeminfoSize(void *hKernelMemInfo);
void *PVRSRVGetMeminfoCPUAddr(void *hMemHandle);
int PVRSRVGetMeminfoPages(void *hMemHandle, int npages, struct page ***pages);
int PVRSRVGetMeminfoPfn(void *hMemHandle, int npages, unsigned long **pfns);
int PVRSRVMMap(struct file *pFile, struct vm_area_struct *ps_vma);
int PVRSRVInterrupt(struct drm_device *dev);

#endif
