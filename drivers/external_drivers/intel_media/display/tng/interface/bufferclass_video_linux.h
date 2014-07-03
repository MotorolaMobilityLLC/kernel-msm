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

#ifndef __BC_VIDEO_LINUX_H__
#define __BC_VIDEO_LINUX_H__

#include <linux/ioctl.h>

#define BC_FOURCC(a, b, c, d) \
	((unsigned long) ((a) | (b)<<8 | (c)<<16 | (d)<<24))

#define BC_PIX_FMT_NV12     BC_FOURCC('N', 'V', '1', '2')	/*YUV 4:2:0 */
#define BC_PIX_FMT_UYVY     BC_FOURCC('U', 'Y', 'V', 'Y')	/*YUV 4:2:2 */
#define BC_PIX_FMT_YUYV     BC_FOURCC('Y', 'U', 'Y', 'V')	/*YUV 4:2:2 */
#define BC_PIX_FMT_RGB565   BC_FOURCC('R', 'G', 'B', 'P')	/*RGB 5:6:5 */

int FillBuffer(unsigned int uiBufferIndex);

typedef struct BC_Video_ioctl_package_TAG {
	int ioctl_cmd;
	int device_id;
	int inputparam;
	int outputparam;
} BC_Video_ioctl_package;

typedef struct bc_buf_ptr {
	unsigned int index;
	int size;
	unsigned long pa;
	unsigned long handle;
} bc_buf_ptr_t;

#define BC_Video_ioctl_fill_buffer              0
#define BC_Video_ioctl_get_buffer_count         1
/*get physical address by index */
#define BC_Video_ioctl_get_buffer_phyaddr       2
/*get index by physical address */
#define BC_Video_ioctl_get_buffer_index         3
#define BC_Video_ioctl_request_buffers          4
#define BC_Video_ioctl_set_buffer_phyaddr       5
#define BC_Video_ioctl_release_buffer_device    6

#define BC_Video_ioctl_alloc_buffer             7
#define BC_Video_ioctl_free_buffer              8
#define BC_Video_ioctl_get_buffer_handle        9

int BC_Camera_Bridge(BC_Video_ioctl_package *psBridge, unsigned long pAddr);

#endif
