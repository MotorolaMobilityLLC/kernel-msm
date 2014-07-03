/**********************************************************************
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

#ifndef __BC_VIDEO_H__
#define __BC_VIDEO_H__

#include "img_defs.h"
#include "bufferclass_interface.h"
#include "imgpixfmts_km.h"

#if defined(__cplusplus)
extern "C"
{
#endif

	#define MAX_BUFFER_DEVICE_NAME_SIZE	(50) /*!< Max size of the buffer device name */

	/*! buffer information structure */
	typedef struct BUFFER_INFO_TAG
	{
		unsigned int	ui32BufferCount;     /*!< Number of supported buffers */
		unsigned int	ui32BufferDeviceID;  /*!< DeviceID assigned by Services*/
		IMG_PIXFMT		eIMGPixFmt;          /*!< Pixel format of the buffer */
		unsigned int	ui32ByteStride;      /*!< Byte stride of the buffer */
		unsigned int	ui32Width;           /*!< Width of the buffer, in pixels */
		unsigned int	ui32Height;          /*!< Height of the buffer, in pixels */
		unsigned int	ui32Flags;           /*!< Flags */
		char			szDeviceName[MAX_BUFFER_DEVICE_NAME_SIZE];  /*!< Name of the device */
	} BUFFER_INFO;

	enum BC_memory
	{
		BC_MEMORY_MMAP = 1,
		BC_MEMORY_USERPTR = 2,
	};

	/*
	 * the following types are tested for fourcc in struct bc_buf_params_t
	 *   NV12
	 *   UYVY
	 *   RGB565 - not tested yet
	 *   YUYV
	 */
	typedef struct bc_buf_params {
		int count;            /*number of buffers, [in/out] */
		int width;            /*buffer width in pixel, multiple of 8 or 32 */
		int height;            /*buffer height in pixel */
		int stride;
		unsigned int fourcc;    /*buffer pixel format */
		enum BC_memory type;
	} bc_buf_params_t;

#define BC_VIDEO_DEVICE_MAX_ID          5
#define BC_CAMERA_DEVICEID              8

	typedef void *BCE_HANDLE;

	typedef enum tag_bce_bool {
		BCE_FALSE = 0,
		BCE_TRUE = 1,
	} BCE_BOOL, *BCE_PBOOL;

	typedef struct BC_VIDEO_BUFFER_TAG {
		unsigned long ulSize;
		BCE_HANDLE hMemHandle;

		IMG_SYS_PHYADDR *psSysAddr;

		IMG_CPU_VIRTADDR sCPUVAddr;

		struct BC_VIDEO_BUFFER_TAG *psNext;
		int sBufferHandle;
		IMG_BOOL is_conti_addr;
	} BC_VIDEO_BUFFER;

	typedef struct BC_VIDEO_DEVINFO_TAG {
		IMG_UINT32 ulDeviceID;

		BC_VIDEO_BUFFER *psSystemBuffer;

		unsigned long ulNumBuffers;

		BCE_HANDLE hPVRServices;

		unsigned long ulRefCount;

		BUFFER_INFO sBufferInfo;
		enum BC_memory buf_type;
	} BC_VIDEO_DEVINFO;

	typedef enum _BCE_ERROR_ {
		BCE_OK = 0,
		BCE_ERROR_GENERIC = 1,
		BCE_ERROR_OUT_OF_MEMORY = 2,
		BCE_ERROR_TOO_FEW_BUFFERS = 3,
		BCE_ERROR_INVALID_PARAMS = 4,
		BCE_ERROR_INIT_FAILURE = 5,
		BCE_ERROR_CANT_REGISTER_CALLBACK = 6,
		BCE_ERROR_INVALID_DEVICE = 7,
		BCE_ERROR_DEVICE_REGISTER_FAILED = 8,
		BCE_ERROR_NO_PRIMARY = 9
	} BCE_ERROR;

#ifndef UNREFERENCED_PARAMETER
#define    UNREFERENCED_PARAMETER(param) (param) = (param)
#endif

#ifndef NULL
#define NULL 0
#endif

	BCE_ERROR BC_Video_Register(int id);
	BCE_ERROR BC_Video_Unregister(int id);
	BCE_ERROR BC_Video_Buffers_Create(int id);
	BCE_ERROR BC_Video_Buffers_Destroy(int id);
	BCE_ERROR BC_Video_Init(int id);
	BCE_ERROR BC_Video_Deinit(int id);

	BCE_ERROR BCOpenPVRServices(BCE_HANDLE * phPVRServices);
	BCE_ERROR BCClosePVRServices(BCE_HANDLE hPVRServices);

	void *BCAllocKernelMem(unsigned long ulSize);
	void BCFreeKernelMem(void *pvMem);

#if defined(BC_DISCONTIG_BUFFERS)
	BCE_ERROR BCAllocDiscontigMemory(unsigned long ulSize,
					 BCE_HANDLE unref__ * phMemHandle,
					 IMG_CPU_VIRTADDR * pLinAddr,
					 IMG_SYS_PHYADDR ** ppPhysAddr);

	void BCFreeDiscontigMemory(unsigned long ulSize,
				   BCE_HANDLE unref__ hMemHandle,
				   IMG_CPU_VIRTADDR LinAddr,
				   IMG_SYS_PHYADDR * pPhysAddr);
#else

	BCE_ERROR BCAllocContigMemory(unsigned long    ulSize,
			BCE_HANDLE       *phMemHandle,
			IMG_CPU_VIRTADDR *pLinAddr,
			IMG_CPU_PHYADDR  *pPhysAddr);

	void BCFreeContigMemory(unsigned long ulSize,
			BCE_HANDLE hMemHandle,
			IMG_CPU_VIRTADDR LinAddr,
			IMG_CPU_PHYADDR PhysAddr);
#endif

	IMG_SYS_PHYADDR CpuPAddrToSysPAddrBC(IMG_CPU_PHYADDR cpu_paddr);
	IMG_CPU_PHYADDR SysPAddrToCpuPAddrBC(IMG_SYS_PHYADDR sys_paddr);

	void *MapPhysAddr(IMG_SYS_PHYADDR sSysAddr, unsigned long ulSize);
	void UnMapPhysAddr(void *pvAddr, unsigned long ulSize);

	BC_VIDEO_DEVINFO *GetAnchorPtr(int id);
	int GetBufferCount(unsigned int *puiBufferCount, int id);

	extern unsigned int bc_video_id_usage[BC_VIDEO_DEVICE_MAX_ID];

#if defined(__cplusplus)
}
#endif

#endif
