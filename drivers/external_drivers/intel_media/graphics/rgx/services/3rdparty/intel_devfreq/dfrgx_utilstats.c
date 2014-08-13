/**************************************************************************
 * Copyright (c) 2013, Intel Corporation.
 * All Rights Reserved.

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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include "device.h"
#include "osfunc.h"
#include "rgxdebug.h"
#include "dfrgx_utilstats.h"
#include "pvr_tlcommon.h"
#include "img_types.h"
#include "pvrsrv.h"
#include "rgxdevice.h"


#define DFRGX_HWPERF_DEBUG 0

#if (defined DFRGX_HWPERF_DEBUG) && DFRGX_HWPERF_DEBUG
#define DFRGX_DEBUG_MSG(string)				printk(DFRGX_HWPERF_ALERT string, __func__)
#define DFRGX_DEBUG_MSG_1(string, var1)			printk(DFRGX_HWPERF_ALERT string, __func__, var1)
#define DFRGX_DEBUG_MSG_2(string, var1, var2)		printk(DFRGX_HWPERF_ALERT string, __func__, var1, var2)
#define DFRGX_DEBUG_MSG_3(string, var1, var2, var3)	printk(DFRGX_HWPERF_ALERT string, __func__, var1, var2, var3)
#else
#define DFRGX_DEBUG_MSG(string)
#define DFRGX_DEBUG_MSG_1(string, var1)
#define DFRGX_DEBUG_MSG_2(string, var1, var2)
#define DFRGX_DEBUG_MSG_3(string, var1, var2, var3)
#endif

typedef struct _DFRGX_HWPERF_OBJ_ {
	PVRSRV_DEVICE_NODE *pdev_node;
	PVRSRV_RGXDEV_INFO *prgx_dev_info;
	unsigned int is_device_acquired;
} DFRGX_HWPERF_OBJ;

static DFRGX_HWPERF_OBJ *pDFRGX_Obj = NULL;

/******************************************************************************
 * Helper Functions(s)
 *****************************************************************************/

static unsigned int gpu_rgx_acquire_device(void){

	PVRSRV_DEVICE_TYPE *peDeviceTypeInt = IMG_NULL;
	PVRSRV_DEVICE_CLASS *peDeviceClassInt = IMG_NULL;
	IMG_UINT32 *pui32DeviceIndexInt = IMG_NULL;
	IMG_HANDLE h_dev_cookie = IMG_NULL;
	IMG_UINT32 num_devices = 0;
	unsigned int error = DFRGX_HWPERF_OK;
	IMG_UINT32 rgx_index = IMG_UINT32_MAX;
	int i = 0;

	if (pDFRGX_Obj) {
		peDeviceTypeInt = kzalloc(PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_TYPE), GFP_KERNEL);
		if (!peDeviceTypeInt)
		{
			error = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto go_free;
		}

		peDeviceClassInt = kzalloc(PVRSRV_MAX_DEVICES * sizeof(PVRSRV_DEVICE_CLASS), GFP_KERNEL);
		if (!peDeviceClassInt)
		{
			error = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto go_free;
		}

		pui32DeviceIndexInt = kzalloc(PVRSRV_MAX_DEVICES * sizeof(IMG_UINT32), GFP_KERNEL);
		if (!pui32DeviceIndexInt)
		{
			error = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto go_free;
		}

		/* Enumerate active devices */
		error = PVRSRVEnumerateDevicesKM(
						&num_devices,
						peDeviceTypeInt,
						peDeviceClassInt,
						pui32DeviceIndexInt);
		if (error) {
			DFRGX_DEBUG_MSG_1("%s: PVRSRVEnumarateDevicesKM failed %d\n", error);
			goto go_free;
		}

		DFRGX_DEBUG_MSG_1("%s: Num Devices : %d \n", num_devices);

		for (i = 0; i < num_devices; i++) {
			DFRGX_DEBUG_MSG_2("%s: Index %d:  Device %d:\n",
				i, peDeviceTypeInt[i]);

			if (peDeviceTypeInt[i] == PVRSRV_DEVICE_TYPE_RGX) {
				rgx_index = i;
				break;
			}
		}

		if (rgx_index == IMG_UINT32_MAX) {
			error = PVRSRV_ERROR_INIT_FAILURE;
			goto go_free;
		}

		/* Now we have to acquire the node to work with, RGX device required*/
		error = PVRSRVAcquireDeviceDataKM(rgx_index, PVRSRV_DEVICE_TYPE_RGX, &h_dev_cookie);
		if (error) {

			DFRGX_DEBUG_MSG_1("%s: PVRSRVEnumarateDevicesKM failed %d \n", error);
			goto go_free;
		}

		pDFRGX_Obj->pdev_node = (PVRSRV_DEVICE_NODE*)h_dev_cookie;
		DFRGX_DEBUG_MSG_2("%s: Acquired Device node name: %s, Device type: %d \n",
			pDFRGX_Obj->pdev_node->szRAName, pDFRGX_Obj->pdev_node->sDevId.eDeviceType);
	} else {

		DFRGX_DEBUG_MSG_2("%s: Device node already acquired: %s, Device type: %d \n",
			pDFRGX_Obj->pdev_node->szRAName, pDFRGX_Obj->pdev_node->sDevId.eDeviceType);
	}

go_free:
	if (peDeviceTypeInt)
		kfree(peDeviceTypeInt);
	if (peDeviceClassInt)
		kfree(peDeviceClassInt);
	if (pui32DeviceIndexInt)
		kfree(pui32DeviceIndexInt);

	return error;
}

unsigned int gpu_rgx_get_util_stats(void* pvData)
{
	RGXFWIF_GPU_UTIL_STATS*	putil_stats = (RGXFWIF_GPU_UTIL_STATS*)pvData;
	RGXFWIF_GPU_UTIL_STATS utils;

	if (!pDFRGX_Obj || !pDFRGX_Obj->prgx_dev_info ||
		!pDFRGX_Obj->prgx_dev_info->pfnGetGpuUtilStats ||
		!pDFRGX_Obj->pdev_node)
		return 0;

	utils = pDFRGX_Obj->prgx_dev_info->pfnGetGpuUtilStats(pDFRGX_Obj->pdev_node);

	putil_stats->bValid = utils.bValid;
	putil_stats->bIncompleteData = utils.bIncompleteData;
	putil_stats->ui32GpuStatActiveHigh = utils.ui32GpuStatActiveHigh;
	putil_stats->ui32GpuStatActiveLow = utils.ui32GpuStatActiveLow;
	putil_stats->ui32GpuStatBlocked = utils.ui32GpuStatBlocked;
	putil_stats->ui32GpuStatIdle = utils.ui32GpuStatIdle;

	return putil_stats->bValid;

}
EXPORT_SYMBOL(gpu_rgx_get_util_stats);

unsigned int gpu_rgx_utilstats_init_obj(void){

	unsigned int error = DFRGX_HWPERF_OK;

	if (pDFRGX_Obj) {
		DFRGX_DEBUG_MSG("%s: pDFRGX Object already initialized! \n");
		goto go_out;
	}

	pDFRGX_Obj = kzalloc(sizeof(DFRGX_HWPERF_OBJ), GFP_KERNEL);
	if (!pDFRGX_Obj) {
		error = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto go_out;
	}

	error = gpu_rgx_acquire_device();
	if (error) {
		DFRGX_DEBUG_MSG_1("%s: gpu_rgx_acquire_device failed %d \n", error);
		goto go_free_obj;
	}

	pDFRGX_Obj->prgx_dev_info = (PVRSRV_RGXDEV_INFO*)pDFRGX_Obj->pdev_node->pvDevice;
go_out:
	return error;
go_free_obj:
	kfree(pDFRGX_Obj);
	pDFRGX_Obj = NULL;
	return error;
}
EXPORT_SYMBOL(gpu_rgx_utilstats_init_obj);

unsigned int gpu_rgx_utilstats_deinit_obj(void){

	if (!pDFRGX_Obj) {
		return 0;
	}

	kfree(pDFRGX_Obj);
	pDFRGX_Obj = NULL;
	return 0;
}
EXPORT_SYMBOL(gpu_rgx_utilstats_deinit_obj);

