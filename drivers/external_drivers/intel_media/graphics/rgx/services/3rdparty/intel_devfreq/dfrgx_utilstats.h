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

#ifndef _DF_RGXHWPERF_H_
#define _DF_RGXHWPERF_H_

#define DF_RGX_HWPERF_DEV    "dfrgxhwperf"
#define DFRGX_HWPERF_ALERT KERN_ALERT DF_RGX_HWPERF_DEV ": "

 /*****************************************************************************
 * Error values
 *****************************************************************************/
typedef enum _DFRGX_HWPERF_ERROR_ {
	DFRGX_HWPERF_OK,
	DFRGX_HWPERF_OBJ_NOT_CREATED,
	DFRGX_HWPERF_ALREADY_INITIAIZED,
	DFRGX_HWPERF_NODE_NOT_ACQUIRED,
	DFRGX_HWPERF_EVENTS_NOT_ENABLED,
	DFRGX_HWPERF_COUNTERS_NOT_CONFIGURED,
	DFRGX_HWPERF_ERROR_FORCE_I32 = 0x7fffffff

} DFRGX_HW_PERF_ERROR;

/******************************************************************************
 * RGX  Profiling Server API(s)
 *****************************************************************************/
unsigned int gpu_rgx_utilstats_init_obj(void);

unsigned int gpu_rgx_utilstats_deinit_obj(void);

unsigned int gpu_rgx_get_util_stats(void *pvData);


#endif	/* _DF_RGXHWPERF_H_*/
