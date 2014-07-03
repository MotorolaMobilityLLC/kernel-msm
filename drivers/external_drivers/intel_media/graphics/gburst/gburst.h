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
 *    Dale B. Stimson <dale.b.stimson@intel.com>
 *    Jari Luoma-aho  <jari.luoma-aho@intel.com>
 *    Jari Nippula    <jari.nippula@intel.com>
 *
 */

#if !defined GBURST_H
#define GBURST_H

#if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE)

#include <linux/types.h>
#include <linux/proc_fs.h>

/* Global variables */

/**
 * gburst_debug_msg_on - Enables some debug messages.
 *
 * Initialized to zero.  May be set via /proc
 * If set, one function in gburst_stats.c
 * uses printk to output index, perf_counter->utilization.
 */
extern int gburst_debug_msg_on;

/* Functions for gathering utilization information. */

/**
 * gburst_stats_gpu_freq_mhz_info() - Give cpu frequency to stats and to
 * graphics system.
 * @freq_MHz: Frequency in MHz.
 */
int gburst_stats_gpu_freq_mhz_info(int freq_MHz);

void gburst_stats_gfx_hw_perf_max_values_clear(void);

int gburst_stats_gfx_hw_perf_max_values_to_string(int ix_in,
	char *buf, size_t buflen);

int gburst_stats_gfx_hw_perf_counters_to_string(int ix_in,
	char *buf, size_t buflen);

int gburst_stats_gfx_hw_perf_counters_set(const char *buf);

int gburst_stats_gfx_hw_perf_record();

int gburst_stats_active_counters_from_string(const char *buf, int nbytes);

int gburst_stats_active_counters_to_string(char *buf, int breq);

int gburst_stats_shutdown(void);

void gburst_stats_cleanup_gfx_load_data(void);

#endif /* if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE) */

#endif /* if !defined GBURST_H */
