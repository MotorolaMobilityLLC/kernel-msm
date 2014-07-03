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

#if !defined DF_RGX_DEFS_H
#define DF_RGX_DEFS_H

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/devfreq.h>

/**
 * THERMAL_COOLING_DEVICE_MAX_STATE - The maximum cooling state that this
 * driver (as a thermal cooling device by reducing frequency) supports.
 */
#define THERMAL_COOLING_DEVICE_MAX_STATE	4
#define NUMBER_OF_LEVELS_B0			8
#define NUMBER_OF_LEVELS			4

#define DF_RGX_FREQ_KHZ_MIN             200000
#define DF_RGX_FREQ_KHZ_MAX             533000

#define DF_RGX_FREQ_KHZ_MIN_INITIAL     DF_RGX_FREQ_KHZ_MIN

#define DF_RGX_FREQ_KHZ_MAX_INITIAL     320000

#define DF_RGX_INITIAL_FREQ_KHZ         320000

#define DF_RGX_THERMAL_LIMITED_FREQ_KHZ 200000

typedef enum _DFRGX_FREQ_ {
	DFRGX_FREQ_200_MHZ = 200000,
	DFRGX_FREQ_213_MHZ = 213000,
	DFRGX_FREQ_266_MHZ = 266000,
	DFRGX_FREQ_320_MHZ = 320000,
	DFRGX_FREQ_355_MHZ = 355000,
	DFRGX_FREQ_400_MHZ = 400000,
	DFRGX_FREQ_457_MHZ = 457000,
	DFRGX_FREQ_533_MHZ = 533000,
} DFRGX_FREQ;

typedef enum _DFRGX_TURBO_PROFILE_ {
	DFRGX_TURBO_PROFILE_SIMPLE_ON_DEMAND	= 0,
	DFRGX_TURBO_PROFILE_POWERSAVE		= 1,
	DFRGX_TURBO_PROFILE_CUSTOM		= 2,
	DFRGX_TURBO_PROFILE_USERSPACE		= 3,
	DFRGX_TURBO_PROFILE_PERFORMANCE 	= 4,
	DFRGX_TURBO_PROFILE_MAX			= 5,
} DFRGX_TURBO_PROFILE;

typedef enum _DFRGX_BURST_MODE_ {
	DFRGX_NO_BURST_REQ	= 0,
	DFRGX_BURST_REQ		= 1,
	DFRGX_UNBURST_REQ	= 2,
} DFRGX_BURST_MODE;

struct gpu_util_stats {
	/* if TRUE, statistict are valid, otherwise
	* there was not enough data to calculate the ratios
	*/
	unsigned int				bValid;
	unsigned int				bIncompleteData;
	/* GPU active  ratio expressed in 0,01% units */
	unsigned int				ui32GpuStatActiveHigh;
	/* GPU active  ratio expressed in 0,01% units */
	unsigned int				ui32GpuStatActiveLow;
	/* GPU blocked ratio expressed in 0,01% units */
	unsigned int				ui32GpuStatBlocked;
	/* GPU idle    ratio expressed in 0,01% units */
	unsigned int				ui32GpuStatIdle;
};

/**
 * struct gpu_profiling_record - profiling information
 */
struct gpu_profiling_record {
	ktime_t		last_timestamp_ns;
	long long	time_ms;
};

struct gpu_utilization_record {
	unsigned long		freq;
	int			code;
};

struct gpu_freq_thresholds {
	/*lower limit utilization percentage, unburst it!*/
	int			util_th_low;
	/*upper limit utilization percentage, burst it!*/
	int			util_th_high;
};

struct gpu_data {
	unsigned long int     freq_limit;
};

/**
 * struct df_rgx_data_s - dfrgx burst private data
 */
struct df_rgx_data_s {

	struct busfreq_data		*bus_freq_data;
	struct hrtimer			g_timer;

	/* gbp_task - pointer to task structure for work thread or NULL. */
	struct task_struct		*g_task;

	/* gbp_hrt_period - Period for timer interrupts as a ktime_t. */
	ktime_t				g_hrt_period;
	int				g_initialized;
	int				g_suspended;
	int				g_thread_check_utilization;


	/* gbp_enable - Usually 1.  If 0, gpu burst is disabled. */
	int				g_enable;
	int				g_profiling_enable;
	int				g_timer_is_enabled;

	struct mutex			g_mutex_sts;
	unsigned long			g_recommended_freq_level;
	unsigned long int		g_freq_mhz_min;
	unsigned long int		g_freq_mhz_max;
	int				gpu_utilization_record_index;
	int				g_min_freq_index;
	int				g_max_freq_index;
	int				g_profile_index;
};

struct busfreq_data {
	struct df_rgx_data_s g_dfrgx_data;
	struct device        *dev;
	struct devfreq       *devfreq;
	struct notifier_block pm_notifier;
	struct mutex          lock;
	bool                  disabled;
	unsigned long int     bf_freq_mhz_rlzd;
	unsigned long int     bf_prev_freq_rlzd;
	unsigned long int     bf_desired_freq;
	unsigned int	      b_resumed;
	unsigned int	      b_need_freq_update;
	char		      prev_governor[DEVFREQ_NAME_LEN + 1];

	struct thermal_cooling_device *gbp_cooldv_hdl;
	int                   gbp_cooldv_state_cur;
	int                   gbp_cooldv_state_prev;
	int                   gbp_cooldv_state_highest;
	int                   gbp_cooldv_state_override;
	unsigned int	      gbp_cooldv_latest_freq_max;
	unsigned int	      gbp_cooldv_latest_freq_min;
	struct gpu_data	      gpudata[THERMAL_COOLING_DEVICE_MAX_STATE];
};

/**
 * struct userspace_gov_data - Must the same as  userspace_data
 */
struct userspace_gov_data {
	unsigned long user_frequency;
	bool valid;
};


/*Available states - freq mapping table*/
static const struct gpu_utilization_record a_available_state_freq[] = {
					{DFRGX_FREQ_200_MHZ, 0xF},
					/*Need a proper value for this freq*/
					{DFRGX_FREQ_213_MHZ, 0xE},
					{DFRGX_FREQ_266_MHZ, 0xB},
					{DFRGX_FREQ_320_MHZ, 0x9},
					{DFRGX_FREQ_355_MHZ, 0x8},
					{DFRGX_FREQ_400_MHZ, 0x7},
					{DFRGX_FREQ_457_MHZ, 0x6},
					{DFRGX_FREQ_533_MHZ, 0x5}
					};

unsigned int df_rgx_is_valid_freq(unsigned long int freq);
unsigned int df_rgx_request_burst(struct df_rgx_data_s *pdfrgx_data,
					int util_percentage);
int df_rgx_get_util_record_index_by_freq(unsigned long freq);
long df_rgx_set_freq_khz(struct busfreq_data *bfdata,
				unsigned long freq_khz);
int df_rgx_set_governor_profile(const char *governor_name,
					struct df_rgx_data_s *g_dfrgx);

#endif /*DF_RGX_DEFS_H*/
