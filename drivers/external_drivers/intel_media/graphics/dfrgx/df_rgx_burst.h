/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
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

#if !defined DFRGX_BURST_H
#define  DFRGX_BURST_H
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/thermal.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/hugetlb.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

#include <linux/string.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/hardirq.h>
#include <linux/timer.h>
#include "df_rgx_defs.h"
#include "dev_freq_graphics_pm.h"

#define DFRGX_BURST_TIMER_PERIOD_DEFAULT_USECS 5000


int dfrgx_burst_init(struct df_rgx_data_s *g_dfrgx);
void dfrgx_burst_deinit(struct df_rgx_data_s *g_dfrgx);
void dfrgx_burst_set_enable(struct df_rgx_data_s *g_dfrgx, int enable);
void dfrgx_profiling_set_enable(struct df_rgx_data_s *g_dfrgx, int enable);
int dfrgx_burst_is_enabled(struct df_rgx_data_s *g_dfrgx);
int dfrgx_profiling_is_enabled(struct df_rgx_data_s *g_dfrgx);
void gpu_profiling_records_restart(void);
int gpu_profiling_records_show(char *buf);

#endif /*DFRGX_BURST*/

