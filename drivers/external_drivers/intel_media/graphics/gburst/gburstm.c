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
 */

/**
 *  To-do:
 * - Select appropriate loglevel for each printk, instead of all ALERT.
 * - Verify thermal cooling device properly bound to thermal zones.
 *   At the moment, this is waiting on kernel work external to this driver.
 *   -  The four temperature states known to the firmware are
 *       normal, warning, alert, and critical.
 * - Check all smp_rmb, smp_wmb, smp_mb
 * - Access to gbprv->gbp_task protected enough?
 * - Comment functions with particular requirements for execution context:
 *   - Execution context: non-atomic
 *   - Execution context: hard irq level
	if (gbprv->gbp_task)
 * - Preference "long battery life" should disable burst.
 * - Low battery state should disable burst.
 * - Check TSC freq. properly (timestamp function). Now assumes 2GHz.
 */


#if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE)

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
#include <asm/intel-mid.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#define GBURST_DEBUG 1

#include "utilf.h"

#include <gburst_interface.h>
#include "gburst.h"

#define GBURST_GLOBAL_ENABLE_DEFAULT 1

#define GBURST_DRIVER_NAME "gburst"

#define GBURST_HEADING GBURST_DRIVER_NAME ": "

#define GBURST_ALERT KERN_ALERT GBURST_DRIVER_NAME ": "

#define GBURST_VERBOSITY_WHYMSG 3

/**
 * GBURST_PFS_NAME_DIR_GBURST - directory name under /proc for gburst.
 */
#define GBURST_PFS_NAME_DIR_GBURST          "gburst"

/**
 * GBURST_IRQ_LEVEL - could possibly be obtained dynamically via access to
 * platform device or via call to arch/x86/platform/intel-mid/mrst.c.
 */
#define GBURST_IRQ_LEVEL 73


/**
 * GBURST_GPU_FREQ_* - frequency specifications.
 * Request is in PWRGT_CNT and PWRGT_STS, bits 27:24.
 * As realized is in PWRGT_STS, bits 23:20.
 *
 *   Values for request or realized:
 *   0001b = Graphics Clock is 533 MHz Unthrottled
 *   0000b = Graphics Clock is 400 MHz Unthrottled
 *
 *   Values only used for realized in PWRGT_STS (not in PWRGT_CNT):
 *   1001b = Graphics Clock is 400 MHz at 12.5% Throttled (350 MHz effective)
 *   1010b = Graphics Clock is 400 MHz at 25%   Throttled (300 MHz effective)
 *   1011b = Graphics Clock is 400 MHz at 37.5% Throttled (250 MHz effective)
 *   1100b = Graphics Clock is 400 MHz at 50%   Throttled (200 MHz effective)
 *   1101b = Graphics Clock is 400 MHz at 62.5% Throttled (150 MHz effective)
 *   1110b = Graphics Clock is 400 MHz at 75%   Throttled (100 MHz effective)
 *   1111b = Graphics Clock is 400 MHz at 87.5% Throttled ( 50 MHz effective)
 */

#define GBURST_GPU_FREQ_400          0x00
#define GBURST_GPU_FREQ_533          0x01
/* The following names are not fully descriptive, but oh, well. */
#define GBURST_GPU_FREQ_350          0x09
#define GBURST_GPU_FREQ_300          0x0a
#define GBURST_GPU_FREQ_250          0x0b
#define GBURST_GPU_FREQ_200          0x0c
#define GBURST_GPU_FREQ_150          0x0d
#define GBURST_GPU_FREQ_100          0x0e
#define GBURST_GPU_FREQ_50           0x0f

#define GBURST_GPU_FREQ_LEN          0x10

/* MS bit of realized frequency field indicates throttled frequency. */
#define GBURST_GPU_FREQ_THROTTLE_BIT 0x08

/**
 * gpu burst register addresses.
 */
#define PWRGT_CNT_PORT 0x4
#define PWRGT_CNT_ADDR 0x60
#define PWRGT_STS_PORT 0x4
#define PWRGT_STS_ADDR 0x61

/**
 * gpu burst register PWRGT_CNT bits and fields.
 * From the perspective of the OS, this register is intended only for
 * writing and should be read only if necessary to obtain the current state
 * of the toggle bit.
 */

/* PWRGT_CNT_TOGGLE_BIT - toggle on every write so fw can detect change. */
#define PWRGT_CNT_TOGGLE_BIT              0x80000000

/* PWRGT_CNT_INT_ENABLE_BIT - enable interrupt for freq chg notification */
#define PWRGT_CNT_INT_ENABLE_BIT          0x40000000

#define PWRGT_CNT_RESERVED_1_BIT          0x20000000

/**
 * PWRGT_CNT_ENABLE_AUTO_BURST_ENTRY_BIT -
 * If set and the driver has requested gpu burst mode, but the request was
 * denied by the firmware due to burst mode inhibitors (such as high temp),
 * then when the inhibitors go away, automatically enter the previously
 * requested mode.
 * If not set, do not automatically enter the burst mode in that case.
 */
#define PWRGT_CNT_ENABLE_AUTO_BURST_ENTRY_BIT  0x10000000

/**
 * PWRGT_CNT_BURST_REQUEST_* - Burst entry/exit request from OS to fw,
 * bits 27:24
 */
#define PWRGT_CNT_BURST_REQUEST_M         0x0F000000
#define PWRGT_CNT_BURST_REQUEST_S         4
#define PWRGT_CNT_BURST_REQUEST_P         24

/* Values in the field are: GBURST_GPU_FREQ_*. */

#define PWRGT_CNT_BURST_REQUEST_M_400 \
	(GBURST_GPU_FREQ_400 << PWRGT_CNT_BURST_REQUEST_P)
#define PWRGT_CNT_BURST_REQUEST_M_533 \
	(GBURST_GPU_FREQ_533 << PWRGT_CNT_BURST_REQUEST_P)

/* All other PWRGT_CNT bits are reserved. */

/**
 * gpu burst register PWRGT_STS bits and fields.
 * From the perspective of the OS, this register is intended only for
 * reading.  Except for bit 31 and new field 23:20, it more or less
 * reflects the state of what was written to PWRGT_CNT as so far *realized*
 * by the firmware.
 */

#define PWRGT_STS_BURST_SUPPORT_PRESENT_BIT   0x80000000

/* PWRGT_STS_INT_ENABLE_BIT - interrupt enabled for freq chg notification */
#define PWRGT_STS_INT_ENABLE_BIT          0x40000000

#define PWRGT_STS_RESERVED_1_BIT          0x20000000

/**
 * PWRGT_STS_ENABLE_AUTO_BURST_ENTRY_BIT - Reflects previously set value
 * of PWRGT_CNT_ENABLE_AUTO_BURST_ENTRY_BIT in PWRGT_CNT.
 * See description of PWRGT_CNT_ENABLE_AUTO_BURST_ENTRY_BIT.
 */
#define PWRGT_STS_ENABLE_AUTO_BURST_ENTRY_BIT  0x10000000

/**
 * PWRGT_STS_BURST_REQUEST_M - Field containing GBURST_GPU_FREQ_*.
 * as requested via PWRGT_CNT, bits 27:24
 */
#define PWRGT_STS_BURST_REQUEST_M         0x0F000000
#define PWRGT_STS_BURST_REQUEST_S         4
#define PWRGT_STS_BURST_REQUEST_P         24

/**
 * PWRGT_STS_BURST_REALIZED_M - Field containing GBURST_GPU_FREQ_*.
 * as realized, based on request and firmware decisions,
 * bits 23:20
 */
#define PWRGT_STS_BURST_REALIZED_M        0x00F00000
#define PWRGT_STS_BURST_REALIZED_S        4
#define PWRGT_STS_BURST_REALIZED_P        20

#define PWRGT_STS_BURST_REALIZED_M_400 \
	(GBURST_GPU_FREQ_400 << PWRGT_STS_BURST_REALIZED_P)

#define PWRGT_STS_BURST_REALIZED_M_533 \
	(GBURST_GPU_FREQ_533 << PWRGT_STS_BURST_REALIZED_P)

#define PWRGT_STS_FREQ_THROTTLE_M (GBURST_GPU_FREQ_THROTTLE_BIT << \
	PWRGT_STS_BURST_REALIZED_P)

/* Macros to test for states */

#define GBURST_BURST_REQUESTED(gbprv) ((gbprv->gbp_pwrgt_cnt_last_written \
	& PWRGT_CNT_BURST_REQUEST_M) == PWRGT_CNT_BURST_REQUEST_M_533)

#define GBURST_BURST_REALIZED(gbprv) ((gbprv->gbp_pwrgt_sts_last_read \
	& PWRGT_STS_BURST_REALIZED_M) == PWRGT_STS_BURST_REALIZED_M_533)

#define GBURST_BURST_THROTTLED(gbprv) (gbprv->gbp_pwrgt_sts_last_read \
	& PWRGT_STS_FREQ_THROTTLE_M)


/**
 * THERMAL_COOLING_DEVICE_MAX_STATE - The maximum cooling state that
 * gburst (as a thermal cooling device by non-bursting) support.
 */
#define THERMAL_COOLING_DEVICE_MAX_STATE 1


#define GBURST_TIMER_PERIOD_DEFAULT_USECS 5000

#define GBURST_THRESHOLD_DEFAULT_HIGH       80
#define GBURST_THRESHOLD_DEFAULT_DOWN_DIFF  20

/**
 * Burst dynamic control parameters
 * VSYNC_FRAMES      - number of frames rendered within vsync time before
 * we deny burst request even if utilization would indicate otherwise
 * FRAME_TIME_BUFFER - additional buffer after frame 'ready' before the
 * next vsync event (as CPU cycles)
 * FRAME_DURATION    - frame time as max. CPU freq cycles, given system
 * latencies this value is taken as 17ms
 * OFFSCREEN_TIME    - time to last resume even after which we infer that
 * we have an offscreen rendering case (or a very long frame rendering)
 */
#define VSYNC_FRAMES         1
#define FRAME_TIME_BUFFER    ((unsigned long long)(0))
#define OFFSCREEN_FRAMES     ((unsigned long long)(20))
#define FRAME_DURATION       ((unsigned long long)(34000000))
#define OFFSCREEN_TIME       (OFFSCREEN_FRAMES*FRAME_DURATION)

/**
 * timestamp - Helper used when storing internal timestamps used for burst control
 */
unsigned long long timestamp(void)
{
	unsigned int a, d;
	__asm__ volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}


/**
 * pfs_data - Structure to describe one file under /proc/gburst.
 */
struct pfs_data {
	const char   *pfd_file_name;
	ssize_t (*pfd_func_read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*pfd_func_write) (struct file *, const char __user *, size_t, loff_t *);
	mode_t        pfd_mode;
};


/*
 * Forward references for procfs read and write functions:
 */

static int pfs_debug_message_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_debug_message_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_disable_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_disable_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_dump_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_enable_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_enable_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_gpu_monitored_counters_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_gpu_monitored_counters_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_pwrgt_sts_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_state_times_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_state_times_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_thermal_override_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_thermal_override_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_thermal_state_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_gb_threshold_down_diff_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_gb_threshold_down_diff_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_gb_threshold_high_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_gb_threshold_high_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_gb_threshold_low_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_gb_threshold_low_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_timer_period_usecs_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_timer_period_usecs_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_utilization_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_utilization_override_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_utilization_override_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);
static int pfs_verbosity_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos);
static int pfs_verbosity_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos);


/**
 * pfs_tab -- table specifying each gburst file under /proc/gburst.
 */
static const struct pfs_data pfs_tab[] = {
	{ "debug_message",
		pfs_debug_message_read,
		pfs_debug_message_write,
		0644, },
	{ "disable",
		pfs_disable_read,
		pfs_disable_write,
		0666, },
	{ "dump",
		pfs_dump_read,
		NULL,
		0644, },
	{ "enable",
		pfs_enable_read,
		pfs_enable_write,
		0644, },
	{ "monitored",
		pfs_gpu_monitored_counters_read,
		pfs_gpu_monitored_counters_write,
		0644, },
	{ "pwrgt_sts",
		pfs_pwrgt_sts_read,
		NULL,
		0644, },
	{ "state_times",
		pfs_state_times_read,
		pfs_state_times_write,
		0644, },
	{ "thermal_override",
		pfs_thermal_override_read,
		pfs_thermal_override_write,
		0644, },
	{ "thermal_state",
		pfs_thermal_state_read,
		NULL,
		0644, },
	{ "threshold_down_diff",
		pfs_gb_threshold_down_diff_read,
		pfs_gb_threshold_down_diff_write,
		0644, },
	{ "threshold_high",
		pfs_gb_threshold_high_read,
		pfs_gb_threshold_high_write,
		0644, },
	{ "threshold_low",
		pfs_gb_threshold_low_read,
		pfs_gb_threshold_low_write,
		0644, },
	{ "timer_period_usecs",
		pfs_timer_period_usecs_read,
		pfs_timer_period_usecs_write,
		0644, },
	{ "utilization",
		pfs_utilization_read,
		NULL,
		0644, },
	{ "utilization_override",
		pfs_utilization_override_read,
		pfs_utilization_override_write,
		0644, },
	{ "verbosity",
		pfs_verbosity_read,
		pfs_verbosity_write,
		0644, },
};


/**
 * freq_mhz_table - "local data" array translating from frequency code to
 * associated frequency in MHz.
 */
static const int freq_mhz_table[GBURST_GPU_FREQ_LEN] = {
	[GBURST_GPU_FREQ_400] = 400,
	[GBURST_GPU_FREQ_533] = 533,
	[GBURST_GPU_FREQ_350] = 350,
	[GBURST_GPU_FREQ_300] = 300,
	[GBURST_GPU_FREQ_250] = 250,
	[GBURST_GPU_FREQ_200] = 200,
	[GBURST_GPU_FREQ_150] = 150,
	[GBURST_GPU_FREQ_100] = 100,
	[GBURST_GPU_FREQ_50]  =  50,
};

struct gb_state_times_s {
	struct timespec         gst_uptime;
	struct timespec         gst_time_gfx_power;
	struct timespec         gst_time_burst_requested;
	struct timespec         gst_time_burst_realized;
	struct timespec         gst_time_throttled;
};


/**
 * struct gburst_pvt_s - gburst private data
 */
struct gburst_pvt_s {
	struct hrtimer          gbp_timer;
	struct proc_dir_entry  *gbp_proc_parent;
	struct proc_dir_entry  *gbp_proc_gburst;
	struct thermal_cooling_device *gbp_cooldv_hdl;

	/* gbp_task - pointer to task structure for work thread or NULL. */
	struct task_struct     *gbp_task;

	struct mutex            gbp_mutex_pwrgt_sts;

	/* gbp_hrt_period - Period for timer interrupts as a ktime_t. */
	ktime_t                 gbp_hrt_period;

	/* gbp_pfs_handle */
	struct proc_dir_entry  *gbp_pfs_handle[ARRAY_SIZE(pfs_tab)];

	/**
	 * Multipe time values, all updated at once.
	 * All access to these times protected by mutex.
	 */
	struct mutex            gbp_state_times_mutex;
	struct gb_state_times_s gbp_state_times;
	int                     gbp_state_time_header;

	int                     gbp_initialized;
	int                     gbp_suspended;
	int                     gbp_thread_check_utilization;

#if GBURST_DEBUG
	unsigned int            gbp_interrupt_count;
	unsigned int            gbp_thread_work_count;
	unsigned int            gbp_thermal_state_change_count;

	unsigned int            gbp_suspend_count;
	unsigned int            gbp_resume_count;
#endif /* if GBURST_DEBUG */

	int                     gbp_cooldv_state_cur;
	int                     gbp_cooldv_state_prev;
	int                     gbp_cooldv_state_highest;
	int                     gbp_cooldv_state_override;

	/*  1 if disable requested via /proc/gburst/disable */
	int                     gbp_request_disable;

	/*  1 if enable requested via /proc/gburst/enable */
	int                     gbp_request_enable;

	/* gbp_enable - Usually 1.  If 0, gpu burst is disabled. */
	int                     gbp_enable;
	int                     gbp_timer_is_enabled;

	/**
	 * Utilization and threshold values, in percent, 0 to 100, or
	 * -1 if utilization not yet read.
	 */
	int                     gbp_utilization_percentage;
	int                     gbp_utilization_override;

	int                     gbp_burst_th_high;

	/**
	 * gbp_burst_th_down_diff and gbp_burst_th_low
	 * are related.  One of them (selected by gbp_thold_via) is definitive
	 * and the other is computed from the high and definitive values.
	 */
	enum {
		GBP_THOLD_VIA_LOW,
		GBP_THOLD_VIA_DOWN_DIFF
	}                       gbp_thold_via;

	int                     gbp_burst_th_down_diff;
	int                     gbp_burst_th_low;

	u32                     gbp_pwrgt_cnt_toggle_bit;
	u32                     gbp_pwrgt_sts_last_read;
	u32                     gbp_pwrgt_cnt_last_written;

	/**
	 * Burst dynamic control parameters
	 */
	unsigned long long	gbp_resume_time;
	int			gbp_offscreen_rendering;
	int			gbp_num_of_vsync_limited_frames;
};


/* Global variables.  */
int gburst_debug_msg_on;

static struct gburst_pvt_s gburst_private_data;

/**
 * gburst_private_ptr - Static place to save handle for access at module unload.
 * There will never be more than a single instantiation of this driver.
 */
static struct gburst_pvt_s *gburst_private_ptr;


/**
 * Module parameters:
 *
 * - can be updated (if permission allows) via writing:
 *     /sys/module/gburst/parameters/<name>
 * - can be set at module load time:
 *     insmod /lib/modules/gburst.ko enable=0
 * - For built-in modules, can be on kernel command line:
 *     gburst.enable=0
 */

/**
 * module parameter "enable" is not writable in sysfs as there is presently
 * no code to detect the transition between 0 and 1.
 */
static unsigned int mprm_enable = GBURST_GLOBAL_ENABLE_DEFAULT;
module_param_named(enable, mprm_enable, uint, S_IRUGO);

static unsigned int mprm_verbosity = 1;
module_param_named(verbosity, mprm_verbosity, uint, S_IRUGO|S_IWUSR);


#define DRIVER_AUTHOR "Intel Corporation"
#define DRIVER_DESC "gpu burst driver for Intel Clover Trail Plus"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

/**
 *  MODULE_VERSION - Allows specification of a module version.
 *  Version of form [<epoch>:]<version>[-<extra-version>].
 *  Or for CVS/RCS ID version, everything but the number is stripped.
 * <epoch>: A (small) unsigned integer which allows you to start versions
 *          anew. If not mentioned, it's zero.  eg. "2:1.0" is after
 *          "1:2.0".
 * <version>: The <version> may contain only alphanumerics and the
 *          character `.'.  Ordered by numeric sort for numeric parts,
 *          ascii sort for ascii parts (as per RPM or DEB algorithm).
 * <extraversion>: Like <version>, but inserted for local
 *          customizations, eg "rh3" or "rusty1".

 * Using this automatically adds a checksum of the .c files and the
 * local headers in "srcversion".
 *
 * Also, if the module is under drivers/staging, this causes a warning to
 * be issued:
 *     <mname>: module is from the staging directory, the quality is unknown,
 *     you have been warned.
 *
 * Example invocation:
 *     MODULE_VERSION("0.1");
 */


/**
 * update_state_times() - Uptime times, current states.
 * @gbprv: gb handle.
 * @gst:   NULL or pointer to receive struct gb_state_times_s output.
 */
static void update_state_times(struct gburst_pvt_s *gbprv,
	struct gb_state_times_s *gst)
{
	struct timespec ts;
	time_t delta_sec;
	s64 delta_nsec;
	struct gb_state_times_s *pst = &gbprv->gbp_state_times;

	mutex_lock(&gbprv->gbp_state_times_mutex);

	get_monotonic_boottime(&ts);

	delta_sec = ts.tv_sec - pst->gst_uptime.tv_sec;
	delta_nsec = ts.tv_nsec - pst->gst_uptime.tv_nsec;

	pst->gst_uptime = ts;

	if (GBURST_BURST_REQUESTED(gbprv)) {
		set_normalized_timespec(&pst->gst_time_burst_requested,
			pst->gst_time_burst_requested.tv_sec + delta_sec,
			pst->gst_time_burst_requested.tv_nsec + delta_nsec);
	}

	if (!gbprv->gbp_suspended) {
		set_normalized_timespec(&pst->gst_time_gfx_power,
			pst->gst_time_gfx_power.tv_sec + delta_sec,
			pst->gst_time_gfx_power.tv_nsec + delta_nsec);
	}

	if (GBURST_BURST_REALIZED(gbprv)) {
		set_normalized_timespec(&pst->gst_time_burst_realized,
			pst->gst_time_burst_realized.tv_sec + delta_sec,
			pst->gst_time_burst_realized.tv_nsec + delta_nsec);
	}

	if (GBURST_BURST_THROTTLED(gbprv)) {
		set_normalized_timespec(&pst->gst_time_throttled,
			pst->gst_time_throttled.tv_sec + delta_sec,
			pst->gst_time_throttled.tv_nsec + delta_nsec);
	}

	if (gst)
		*gst = *pst;

	mutex_unlock(&gbprv->gbp_state_times_mutex);
}


/**
 * write_PWRGT_CNT() - Write PUnit register PWRGT_CNT via MBI
 * (Message Bus Interface).
 * @gbprv: gb handle.
 * @value: value to be written to the register.
 */
static void write_PWRGT_CNT(struct gburst_pvt_s *gbprv, u32 value)
{
	u32 wvl;

	/*
	 * Change the state of the toggle bit.  Its state must be reversed
	 * for every write to this register.
	 */
	gbprv->gbp_pwrgt_cnt_toggle_bit ^= PWRGT_CNT_TOGGLE_BIT;

	wvl = (value & ~PWRGT_CNT_TOGGLE_BIT) | gbprv->gbp_pwrgt_cnt_toggle_bit;

	intel_mid_msgbus_write32(PWRGT_CNT_PORT, PWRGT_CNT_ADDR, wvl);

	/**
	 * If the requested burst state is being changed from before, update
	 * the cumulative times spent in particular states.
	 */
	if ((wvl ^ gbprv->gbp_pwrgt_cnt_last_written)
		& PWRGT_CNT_BURST_REQUEST_M) {
		/**
		  * Uptime cumulative times.
		  * Important: Not yet changed:
		  * gbprv->gbp_pwrgt_cnt_last_written
		  */
		update_state_times(gbprv, NULL);
	}

	gbprv->gbp_pwrgt_cnt_last_written = wvl;
}


/**
 * read_PWRGT_CNT_toggle() - Read PUnit register PWRGT_CNT via MBI
 * (Message Bus Interface).
 * Warning:  The HAS specifies that this register may be read only in
 * order to determine the current setting of the toggle bit (bit 31).
 * @gbprv: gb handle.
 */
static void read_PWRGT_CNT_toggle(struct gburst_pvt_s *gbprv)
{
	u32 uval;

	uval = intel_mid_msgbus_read32(PWRGT_CNT_PORT, PWRGT_CNT_ADDR);

	gbprv->gbp_pwrgt_cnt_toggle_bit = (uval & PWRGT_CNT_TOGGLE_BIT);
}


/**
 * generate_freq_string() - Convert frequency enum to a string.
 * @freq_enum: Frequency enum: GBURST_GPU_FREQ_* .
 *             Must be in the range 0 to 15.
 * @sbuf: Buffer in which string result is returned.
 * @slen: size of sbuf.
 * Function return value: String describing the frequency.
 */
static const char *generate_freq_string(u32 freq_enum, char *sbuf, int slen)
{
	if (freq_mhz_table[freq_enum] != 0)
		snprintf(sbuf, slen, "%d MHz", freq_mhz_table[freq_enum]);
	else
		snprintf(sbuf, slen, "unrecognized_code_%u", freq_enum);

	return sbuf;
}


/**
 * read_PWRGT_STS_simple() - Read register PWRGT_STS.
 * Restriction to non-atomic context by intel_mid_msgbus_read32 use of
 * pci_get_bus_and_slot.
 * Execution context: non-atomic
 */
static inline u32 read_PWRGT_STS_simple(void)
{
	return intel_mid_msgbus_read32(PWRGT_STS_PORT, PWRGT_STS_ADDR);
}


/**
 * hrt_start() - start (or restart) timer.
 * @gbprv: gb handle.
 */
static void hrt_start(struct gburst_pvt_s *gbprv)
{
	if (gbprv->gbp_enable) {
		if (gbprv->gbp_timer_is_enabled) {
			/* Due to the gbp_timer is auto-restart timer
			 * in most case, we must use hrtimer_cancel
			 * it at first if it is in active state, to avoid
			 * hitting the BUG_ON(timer->state !=
			 * HRTIMER_STATE_CALLBACK) in hrtimer.c.
			*/
			hrtimer_cancel(&gbprv->gbp_timer);
		} else {
			gbprv->gbp_timer_is_enabled = 1;
		}

		hrtimer_start(&gbprv->gbp_timer, gbprv->gbp_hrt_period,
			HRTIMER_MODE_REL);
	}
}


/**
 * hrt_cancel() - cancel a timer.
 * @gbprv: gb handle.
 */
static void hrt_cancel(struct gburst_pvt_s *gbprv)
{
	/* The timer can be restarted with hrtimer_start. */
	hrtimer_cancel(&gbprv->gbp_timer);

	gbprv->gbp_timer_is_enabled = 0;
}


/**
 * set_state_pwrgt_cnt() - write bits to pwrgt control register.
 * @gbprv: gb handle.
 * @more_bits_to_set: Additional bits to set, beyond those that are set
 *     automatically.
 */
static void set_state_pwrgt_cnt(struct gburst_pvt_s *gbprv,
	u32 more_bits_to_set)
{
	u32 gt_cnt;

	gt_cnt = 0;

	smp_rmb();
	if (gbprv->gbp_initialized && gbprv->gbp_enable) {
		gt_cnt |= PWRGT_CNT_INT_ENABLE_BIT | more_bits_to_set;

		if ((gt_cnt & PWRGT_CNT_BURST_REQUEST_M)
			== PWRGT_CNT_BURST_REQUEST_M_533)
			gt_cnt |= PWRGT_CNT_ENABLE_AUTO_BURST_ENTRY_BIT;
	}

	write_PWRGT_CNT(gbprv, gt_cnt);
}


/**
 * read_and_process_PWRGT_STS() - Read PUnit register PWRGT_STS
 * @gbprv: gb handle.
 */
static u32 read_and_process_PWRGT_STS(struct gburst_pvt_s *gbprv)
{
	u32 uval;
	u32 valprv;

	mutex_lock(&gbprv->gbp_mutex_pwrgt_sts);

	uval = read_PWRGT_STS_simple();

	valprv = gbprv->gbp_pwrgt_sts_last_read;

	/**
	 * If either the burst_request or the burst_realized states have
	 * changed (as evidenced by their bit fields within this register),
	 * then process the new state.
	 */
	if ((uval ^ valprv) & (PWRGT_STS_BURST_REQUEST_M
			| PWRGT_STS_BURST_REALIZED_M)) {
		int freq_code;
		int freq_mhz;

		/**
		  * Uptime cumulative times.
		  * Important: Not yet changed: gbprv->gbp_pwrgt_sts_last_read
		  */
		update_state_times(gbprv, NULL);

		freq_code = (uval & PWRGT_STS_BURST_REALIZED_M) >>
			PWRGT_STS_BURST_REALIZED_P;
		freq_mhz = freq_mhz_table[freq_code];

		/**
		 * If either the burst_realized state has changed (as
		 * evidenced by their bit fields within this register), then
		 * process the new state.
		 */
		if (!gbprv->gbp_suspended &&
			((uval ^ valprv) & PWRGT_STS_BURST_REALIZED_M)
			&& (freq_mhz != 0)) {
#if GBURST_UPDATE_GPU_TIMING
			PVRSRV_ERROR eError;
			eError = PVRSRVPowerLock(KERNEL_ID, IMG_FALSE);
			if (eError == PVRSRV_OK) {
				/**
				 * Tell graphics subsystem the updated frequency,
				 * including both pvr km and utilization computations
				 * (which may or may not use the information).
				 */
				gburst_stats_gpu_freq_mhz_info(freq_mhz);
				PVRSRVPowerUnlock(KERNEL_ID);
			}
#else
			/**
			 * Tell graphics subsystem the updated frequency,
			 * including both pvr km and utilization computations
			 * (which may or may not use the information).
			 */
			gburst_stats_gpu_freq_mhz_info(freq_mhz);
#endif
		}

		if (mprm_verbosity >= 2) {
			int freq_code_req;
			freq_code_req = (uval & PWRGT_STS_BURST_REQUEST_M) >>
				PWRGT_STS_BURST_REQUEST_P;

			if (freq_code_req != freq_code) {
				printk(GBURST_ALERT
					"freq req/rlzd = %d/%d MHz\n",
					freq_mhz_table[freq_code_req],
					freq_mhz);
			} else {
				printk(GBURST_ALERT "freq = %d MHz\n",
					freq_mhz);
			}
		}

		/* If GPU clock throttling is entered or exited... */
		if ((valprv ^ uval) & PWRGT_STS_FREQ_THROTTLE_M) {
			if (uval & PWRGT_STS_FREQ_THROTTLE_M) {
				/* GPU clock throttling state entered... */
				hrt_cancel(gbprv);

				if ((gbprv->gbp_pwrgt_cnt_last_written
					& PWRGT_CNT_BURST_REQUEST_M)
					!= PWRGT_CNT_BURST_REQUEST_M_400) {
					/**
					 * Remove any outstanding burst
					 * request.
					 */
					set_state_pwrgt_cnt(gbprv,
						PWRGT_CNT_BURST_REQUEST_M_400);
				}
			} else  {
				/* GPU clock throttling state exited... */
				hrt_start(gbprv);
			}
		}
	}

	gbprv->gbp_pwrgt_sts_last_read = uval;

	mutex_unlock(&gbprv->gbp_mutex_pwrgt_sts);

	return uval;
}


#define GBURST_VERBOSE_EXPLANATION 1

/**
 * desired_burst_state_query() - determine desired burst state.
 * @gbprv: gb handle.
 * @p_whymsg: An explanatory string.
 * @sbuf: A buffer that may be used to store a string.
 * @slen: length of sbuf.
 *
 * Function return values:
 * 0 -> request un-burst,
 * 1 -> request burst,
 * 2 -> no change.
 */
static int desired_burst_state_query(struct gburst_pvt_s *gbprv,
	const char **p_whymsg
#if GBURST_VERBOSE_EXPLANATION
	, char *sbuf, int slen
#endif /* if GBURST_VERBOSE_EXPLANATION */
	)
{
	int utilpct;
	int thermal_state;

	if (gbprv->gbp_utilization_override >= 0)
		utilpct = gbprv->gbp_utilization_override;
	else
		utilpct = gbprv->gbp_utilization_percentage;

	if (gbprv->gbp_cooldv_state_override >= 0)
		thermal_state = gbprv->gbp_cooldv_state_override;
	else
		thermal_state = gbprv->gbp_cooldv_state_cur;

	smp_rmb();
	if (!gbprv->gbp_initialized) {
		*p_whymsg = "!gbprv->gbp_initialized";
		return 0;
	}

	if (!gbprv->gbp_enable) {
		*p_whymsg = "!enable";
		return 0;
	}

	if (gbprv->gbp_suspended) {
		*p_whymsg = "suspended";
		return 0;
	}

	if (thermal_state != 0) {
#if GBURST_VERBOSE_EXPLANATION
		if (mprm_verbosity >= GBURST_VERBOSITY_WHYMSG) {
			if (gbprv->gbp_cooldv_state_override >= 0) {
				snprintf(sbuf, slen,
					"thermal_state = %d (%d)",
					gbprv->gbp_cooldv_state_override,
					gbprv->gbp_cooldv_state_cur);
			} else {
				snprintf(sbuf, slen,
					"thermal_state = %d",
					gbprv->gbp_cooldv_state_cur);
			}
			*p_whymsg = sbuf;
		} else
#endif /* if !GBURST_VERBOSE_EXPLANATION */
		{
			*p_whymsg = "thermal_state != 0";
		}
		return 0;
	}

	/**
	 * Utilization values and utilization thresholds are represented as
	 * a number from 0 through 100, which is considered a percentage of
	 * nominal maximum utilization.
	 *
	 * When current utilization (range 0..100) falls below
	 * gbprv->gbp_burst_th_low (which is known as threshold_low), then
	 * this driver removes any outstanding request for gpu clock burst.
	 *
	 * When current utilization (range 0..100) rises above
	 * gbprv->gbp_burst_th_high (which is known as threshold_high), then
	 * (if not already doing so) this driver submits a request for gpu
	 * clock burst.
	 *
	 * Normally, the threshold_low is less than threshold_high,and the
	 * difference between them represents a range of values that provide
	 * hystersis.
	 *
	 * In order to facilitate testing and validation, the following
	 * special case "magic" numbers are implemented:
	 *
	 *   threshold_low == threshold_high == 0
	 *       Force a request for burst mode.
	 *   threshold_low == threshold_high == 100
	 *       Force no request for burst mode.
	 */

	if ((gbprv->gbp_burst_th_low == 100)
		&& (gbprv->gbp_burst_th_high == 100)) {
		/**
		 * Threshold values (normal range 0 to 100) are both set to
		 * 100, so force that no request be made for burst.
		 */
#if GBURST_VERBOSE_EXPLANATION
		if (mprm_verbosity >= GBURST_VERBOSITY_WHYMSG) {
			snprintf(sbuf, slen,
				"util == %d, forced_non_burst_request ",
				gbprv->gbp_utilization_percentage);
			*p_whymsg = sbuf;
		} else
#endif
		{
			*p_whymsg = "forced_non_burst_request";
		}
		return 0;
	}

	if ((gbprv->gbp_burst_th_low == 0)
		&& (gbprv->gbp_burst_th_high == 0)) {
		/**
		 * Threshold values (normal range 0 to 100) are both set to
		 * 0, so force that a request will be made for burst.
		 */
#if GBURST_VERBOSE_EXPLANATION
		if (mprm_verbosity >= GBURST_VERBOSITY_WHYMSG) {
			snprintf(sbuf, slen,
				"util == %d, forced_burst_request ",
				gbprv->gbp_utilization_percentage);
			*p_whymsg = sbuf;
		} else
#endif
		{
			*p_whymsg = "forced_burst_request";
		}
		return 1;
	}

	if (utilpct <= gbprv->gbp_burst_th_low) {
#if GBURST_VERBOSE_EXPLANATION
		if (mprm_verbosity >= GBURST_VERBOSITY_WHYMSG) {
			if (gbprv->gbp_utilization_override >= 0) {
				snprintf(sbuf, slen,
					"util (%d (%d)) <= threshold_low (%d)",
					gbprv->gbp_utilization_override,
					gbprv->gbp_utilization_percentage,
					gbprv->gbp_burst_th_low);
			} else {
				snprintf(sbuf, slen,
					"util (%d) <= threshold_low (%d)",
					gbprv->gbp_utilization_percentage,
					gbprv->gbp_burst_th_low);
			}
			*p_whymsg = sbuf;
		} else
#endif
		{
			*p_whymsg = "below threshold_low";
		}
		return 0;
	}

	if (utilpct >= gbprv->gbp_burst_th_high) {
#if GBURST_VERBOSE_EXPLANATION
		if (mprm_verbosity >= GBURST_VERBOSITY_WHYMSG) {
			if (gbprv->gbp_utilization_override >= 0) {
				snprintf(sbuf, slen,
					"util (%d (%d)) >= threshold_high (%d)",
					gbprv->gbp_utilization_override,
					gbprv->gbp_utilization_percentage,
					gbprv->gbp_burst_th_high);
			} else {
				snprintf(sbuf, slen,
					"util (%d) >= threshold_high (%d)",
					gbprv->gbp_utilization_percentage,
					gbprv->gbp_burst_th_high);
			}
			*p_whymsg = sbuf;
		} else
#endif
		{
			*p_whymsg = "above threshold_high";
		}
		return 1;
	}

	/* No change, return same as before. */
	*p_whymsg = "same as before";
	return 2;
}


/**
 * request_desired_burst_mode() - Determine and issue a request for the
 * desired burst state.
 * @gbprv: gb handle.
 */
static void request_desired_burst_mode(struct gburst_pvt_s *gbprv)
{
	int rva;
	u32 reqbits;
	const char *whymsg;
	int burst_request_prev;
#if GBURST_VERBOSE_EXPLANATION
	char sbuf[64];
#endif /* if GBURST_VERBOSE_EXPLANATION */

	smp_rmb();
	if (!gbprv->gbp_initialized)
		return;

	if (gbprv->gbp_offscreen_rendering) {
		if (!GBURST_BURST_REQUESTED(gbprv)) {
			reqbits = PWRGT_CNT_BURST_REQUEST_M_533;
			set_state_pwrgt_cnt(gbprv, reqbits);
		}
	} else {

		rva = desired_burst_state_query(gbprv, &whymsg
#if GBURST_VERBOSE_EXPLANATION
			, sbuf, sizeof(sbuf)
#endif /* if GBURST_VERBOSE_EXPLANATION */
			);

		/**
		* The value returned by desired_burst_state_query indicates
		* the desired burst state, vis a vis the presentvburst state.
		* 0 -> request un-burst
		* 1 -> request burst
		* 2 -> no change.
		*/

		/* Get previous burst_request state. */
		burst_request_prev = GBURST_BURST_REQUESTED(gbprv);

		/**
		 * If desired burst_request state changed, then issue the request.
		 */
		if ((rva != 2) && (rva != burst_request_prev)) {
			if ((rva) && (gbprv->gbp_num_of_vsync_limited_frames < VSYNC_FRAMES))
				reqbits = PWRGT_CNT_BURST_REQUEST_M_533;
			else
				reqbits = PWRGT_CNT_BURST_REQUEST_M_400;

			set_state_pwrgt_cnt(gbprv, reqbits);
		}
	}
}


/**
 * wake_thread() - Wake the work thread.
 * @gbprv: gb handle.
 */
static void wake_thread(struct gburst_pvt_s *gbprv)
{
	if (gbprv->gbp_task)
		wake_up_process(gbprv->gbp_task);
}


/**
 * hrt_event_processor() - Process timer-driven things.
 * Called by kernel hrtimer system when the timer expires.
 * @hrthdl: Pointer to the associated hrtimer struct.
 *
 * Execution context: hard irq level.
 * Invoked via interrupt/callback.
 */
static enum hrtimer_restart hrt_event_processor(struct hrtimer *hrthdl)
{
	struct gburst_pvt_s *gbprv =
		container_of(hrthdl, struct gburst_pvt_s, gbp_timer);
	ktime_t mc_now;

	smp_rmb();

	if (gbprv->gbp_initialized && gbprv->gbp_enable &&
		!gbprv->gbp_suspended) {
		gbprv->gbp_thread_check_utilization = 1;
		smp_wmb();
		wake_thread(gbprv);
	}

	if (!gbprv->gbp_timer_is_enabled)
		return HRTIMER_NORESTART;

	mc_now = ktime_get();
	hrtimer_forward(hrthdl, mc_now, gbprv->gbp_hrt_period);

	return HRTIMER_RESTART;
}


/**
 * thread_action() - Perform desired thread actions when woken due to
 * interrupt or timer expiration.
 * @gbprv: gb handle.
 *
 * Called only from work_thread_loop.
 * Function return value:
 * 0 to request thread exit.  Either an error or gpu burst is disabled.
 * 1 otherwise.
 */
static int thread_action(struct gburst_pvt_s *gbprv)
{
	int gpustate;
	int utilpct;
	unsigned long long ctime;
	unsigned long long delta;

	smp_rmb();
	if (!gbprv->gbp_initialized || gbprv->gbp_suspended)
		return 1;

#if GBURST_DEBUG
	gbprv->gbp_thread_work_count++;
#endif /* if GBURST_DEBUG */

	if (!gbprv->gbp_thread_check_utilization) {
		read_and_process_PWRGT_STS(gbprv);
		return 1;
	}

	gbprv->gbp_thread_check_utilization = 0;

	ctime = timestamp();
	delta = ctime - gbprv->gbp_resume_time;

	if (delta > FRAME_DURATION)
		gbprv->gbp_num_of_vsync_limited_frames = 0;
	if (delta > OFFSCREEN_TIME) {
		gbprv->gbp_offscreen_rendering = 1;
		hrt_cancel(gbprv);
	}

	if (!gbprv->gbp_offscreen_rendering) {
		utilpct = gburst_stats_gfx_hw_perf_record();

		if (mprm_verbosity >= 4)
			printk(GBURST_ALERT "util: %d %%\n", utilpct);

		if (utilpct < 0) {
			/**
			 * This should only fail if not initialized and is
			 * most likely because some initialization has
			 * yet to complete.
			 */
			if (gbprv->gbp_utilization_percentage >= 0) {
				/* Only fail if already succeeded once. */
				printk(GBURST_ALERT "obtaining counters failed\n");
				return 0;
			}
		} else
			gbprv->gbp_utilization_percentage = utilpct;
	}

	/* Read current status. */
	read_and_process_PWRGT_STS(gbprv);
	request_desired_burst_mode(gbprv);

	return 1;
}


/**
 * work_thread_loop() - the main loop for the worker thread.
 * @pvd: The "void *" private data provided to kthread_create.
 *       This can be cast to the gbprv handle.
 *
 * Upon return, thread will exit.
 */
static int work_thread_loop(void *pvd)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *) pvd;
	int rva;

	if (mprm_verbosity >= 4)
		printk(GBURST_ALERT "kernel thread started\n");

	for ( ; ; ) {
		/**
		 * Synchronization is via a call to:
		 * int wake_up_process(struct task_struct *p)
		 */
		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop())
			break;

		schedule();

		if (kthread_should_stop())
			break;

		rva = thread_action(gbprv);
		if (rva == 0) {
			/* Thread exit requested */
			break;
		}
	}

	if (mprm_verbosity >= 4)
		printk(GBURST_ALERT "kernel thread stopping\n");

	return 0;
}


/**
 * work_thread_create() - Create work thread.
 * @gbprv: gb handle.
 *
 * This thread is not truely a "real-time" thread, in that there will be no
 * catastrophe if its execution is somewhat delayed.  However, knowing that
 * the nominal execution interval for this timer-woken thread is 5 msecs and
 * knowing that the thread execution will be very short, it seems appropriate
 * to request an elevated scheduling priority.  Perhaps a consensus will be
 * reached as to whether or not this is truely a good idea.
 *
 * Function return value:  < 0 if error, otherwise 0.
 */
static int work_thread_create(struct gburst_pvt_s *gbprv)
{
	struct task_struct *tskhdl;

	if (!gbprv->gbp_task) {
		tskhdl = kthread_create(work_thread_loop,
			(void *) gbprv, "kernel_gburst");

		/* Keep a reference on task structure. */
		get_task_struct(tskhdl);

		if (IS_ERR(tskhdl)) {
			printk(GBURST_ALERT "kernel thread create fail\n");
			return PTR_ERR(tskhdl);
		}

		{
			/**
			 *  Potential values for policy:
			 *    SCHED_NORMAL
			 *    SCHED_FIFO
			 *    SCHED_RR
			 *    SCHED_BATCH
			 *    SCHED_IDLE
			 * optionally OR'd with
			 *   SCHED_RESET_ON_FORK
			 * Valid priorities for SCHED_FIFO and SCHED_RR are
			 * 1..MAX_USER_RT_PRIO-1,
			 * valid priority for SCHED_NORMAL, SCHED_BATCH, and
			 * SCHED_IDLE is 0.
			 */

			/**
			 * An alternative should normal thread priority be
			 * desired would be the following.
			 *     static const int sc_policy = SCHED_NORMAL;
			 *     static const struct sched_param sc_param = {
			 *             .sched_priority = 0,
			 *     };
			 */

			/**
			 * It seems advisable to run our kernel thread
			 * at elevated priority.
			 */
			static const int sc_policy = SCHED_RR;
			static const struct sched_param sc_param = {
				.sched_priority = 1,
			};
			int rva;

			rva = sched_setscheduler_nocheck(tskhdl,
				sc_policy, &sc_param);
			if (rva < 0) {
				printk(GBURST_ALERT
					"task priority set failed,"
					" code: %d\n", -rva);
			}
		}

		gbprv->gbp_task = tskhdl;

		wake_up_process(tskhdl);
	}

	return 0;
}


/**
 * gburst_thread_stop - kill the worker thread.
 * @gbprv: gb handle.
 */
static void gburst_thread_stop(struct gburst_pvt_s *gbprv)
{
	if (gbprv->gbp_task) {
		/* kthread_stop will not return until the thread is gone. */
		kthread_stop(gbprv->gbp_task);

		put_task_struct(gbprv->gbp_task);
		gbprv->gbp_task = NULL;
	}
}


/**
 * gburst_enable_set() - gburst enable/disable.
 * @gbprv: gb handle.
 *
 * Typically triggered through a /proc reference.
 * When disabled, overhead is reduced by turning off the kernel thread
 * and timer.
 */
static void gburst_enable_set(struct gburst_pvt_s *gbprv)
{
	int flgenb;

	flgenb = gbprv->gbp_request_enable &&
		!gbprv->gbp_request_disable;

	if (gbprv->gbp_enable == flgenb)
		return;

	gbprv->gbp_enable = flgenb;

	if (!gbprv->gbp_enable) {
		hrt_cancel(gbprv);

		/* stop the thread */
		if (gbprv->gbp_task) {
			/* Stop thread. */
			gburst_thread_stop(gbprv);
		}
	} else {
		work_thread_create(gbprv);
		hrt_start(gbprv);
	}

	request_desired_burst_mode(gbprv);
}


/**
 * state_times_to_string - Return string describing state_times.
 * @gbprv:  data structure handle
 * @prhdg:  non-zero to include a heading.
 * @slen:   length of buffer
 * @sbuf:   buffer to receive string.
 */
static int state_times_to_string(struct gburst_pvt_s *gbprv, const char *pfx,
	int prhdg, int ix, int slen, char *sbuf)
{
	struct gb_state_times_s gst;
	struct gb_state_times_s *pst = &gst;

	update_state_times(gbprv, pst);

	if (prhdg) {
		ix = ut_isnprintf(ix, sbuf, slen, "%s%s\n", pfx,
			"        uptime      gfx_power burst_request "
			" burst_realized  gpu_throttled");
	}
	if (prhdg == 2) {
		ix = ut_isnprintf(ix, sbuf, slen, "%s%s\n",
			pfx,
			"-------------- -------------- --------------"
			" -------------- --------------");
	}

	ix = ut_isnprintf(ix, sbuf, slen,
		"%s%7lu.%06lu %7lu.%06lu %7lu.%06lu %7lu.%06lu %7lu.%06lu\n",
		pfx,
		pst->gst_uptime.tv_sec,
		pst->gst_uptime.tv_nsec / NSEC_PER_USEC,
		pst->gst_time_gfx_power.tv_sec,
		pst->gst_time_gfx_power.tv_nsec / NSEC_PER_USEC,
		pst->gst_time_burst_requested.tv_sec,
		pst->gst_time_burst_requested.tv_nsec / NSEC_PER_USEC,
		pst->gst_time_burst_realized.tv_sec,
		pst->gst_time_burst_realized.tv_nsec / NSEC_PER_USEC,
		pst->gst_time_throttled.tv_sec,
		pst->gst_time_throttled.tv_nsec / NSEC_PER_USEC);

	return ix;
}


/**
 * copy_from_user_nt() - Like copy_from_user, but ensures null termination,
 * plus accepts as an input the size of the destination buffer.
 * @tbuf: kernel buffer to receive data
 * @tlen: length of kernel buffer
 * @ubuf: user-space buffer
 * @ucnt: Number of bytes in ubuf.
 */
static int copy_from_user_nt(char *tbuf, size_t tlen,
		const void __user *ubuf, unsigned long ucnt)
{
	if ((ucnt >= tlen) || copy_from_user(tbuf, ubuf, ucnt))
		return -EINVAL;

	tbuf[ucnt] = '\0';

	return 0;
}


/**
 * threshold_derive_low() - Compute low, given high and down_diff.
 * @gbprv: gb handle.
 *
 * The bottom threshold may be specified either as a specific value ("low") or
 * as a delta ("down_diff") below the high threshold's current value.  This is
 * done because of differing desires of testers versus those characterizing
 * utilization numbers.  Ultimately, the "low" value is computed and used
 * in utilization comparisons.
 */
static void threshold_derive_low(struct gburst_pvt_s *gbprv)
{
	int tlow;

	tlow = gbprv->gbp_burst_th_high - gbprv->gbp_burst_th_down_diff;
	if (tlow < 0)
		tlow = 0;

	gbprv->gbp_burst_th_low = tlow;
}


/**
 * threshold_derive_down_diff() - Compute down_diff, given high and low.
 * @gbprv: gb handle.
 *
 * The bottom threshold may be specified either as a specific value ("low") or
 * as a delta ("down_diff") below the high threshold's current value.  This is
 * done because of differing desires of testers versus those characterizing
 * utilization numbers.  Ultimately, the "low" value is computed and used
 * in utilization comparisons.
 */
static void threshold_derive_down_diff(struct gburst_pvt_s *gbprv)
{
	int tdd;

	tdd = gbprv->gbp_burst_th_high - gbprv->gbp_burst_th_low;
	if (tdd < 0)
		tdd = 0;

	gbprv->gbp_burst_th_down_diff = tdd;
}


/**
 * threshold_derive_either() - Compute either down_diff or low, given
 * high and the other.
 * @gbprv: gb handle.
 *
 * The bottom threshold may be specified either as a specific value ("low") or
 * as a delta ("down_diff") below the high threshold's current value.  This is
 * done because of differing desires of testers versus those characterizing
 * utilization numbers.  Ultimately, the "low" value is computed and used
 * in utilization comparisons.
 */
static void threshold_derive_either(struct gburst_pvt_s *gbprv)
{
	if (gbprv->gbp_thold_via == GBP_THOLD_VIA_LOW)
		threshold_derive_down_diff(gbprv);
	else /* if (gbprv->gbp_thold_via == GBP_THOLD_VIA_DOWN_DIFF) */
		threshold_derive_low(gbprv);
}


/**
 * generate_dump_string() - Dump all status variables for gpu burst.
 * Useful during development and test.
 * @gbprv: gb handle.
 * @buflen: Length of buf.
 * @buf: Buffer to receive output string.
 *
 * Function return value: negative if error or number of character stored
 * in buf.
 *
 * Side effect: Output of dump string via printk.
 * Useful during development.  Candidate for removal later.
 */
static int generate_dump_string(struct gburst_pvt_s *gbprv, size_t buflen,
	char *buf)
{
	u32 pwrgt_sts;
	u32 tmpv0;
	int ix;

	ix = 0;

	/* Get Punit Status Register */
	pwrgt_sts = read_and_process_PWRGT_STS(gbprv);

	if (!gbprv->gbp_enable) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"enable = %d\n",
			gbprv->gbp_enable);
	}

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"thermal_state = %d\n",
		gbprv->gbp_cooldv_state_cur);

	if (gbprv->gbp_cooldv_state_highest > gbprv->gbp_cooldv_state_cur) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"thermal_state_highest = %d\n",
			gbprv->gbp_cooldv_state_highest);
	}

	if (gbprv->gbp_cooldv_state_override >= 0) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"thermal_override = %d\n",
			gbprv->gbp_cooldv_state_override);
	}

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"utilization_threshold_low = %d\n",
		gbprv->gbp_burst_th_low);

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"utilization_threshold_high = %d\n",
		gbprv->gbp_burst_th_high);

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"utilization = %d\n",
		gbprv->gbp_utilization_percentage);

	if (gbprv->gbp_utilization_override >= 0) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"utilization_override = %d\n",
			gbprv->gbp_utilization_override);
	}

	ix = state_times_to_string(gbprv, GBURST_HEADING, 1, ix, buflen, buf);

	if (!gbprv->gbp_task) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"task_handle = %p\n",
			gbprv->gbp_task);
	}

#if GBURST_DEBUG
	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"counts (thrd work, intr, thrm_chg) = %u, %u, %u\n",
		gbprv->gbp_thread_work_count,
		gbprv->gbp_interrupt_count,
		gbprv->gbp_thermal_state_change_count);

	if (gbprv->gbp_suspend_count || gbprv->gbp_resume_count) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"suspend_resume_count = %u, %u\n",
			gbprv->gbp_suspend_count,
			gbprv->gbp_resume_count);
	}

	if (gbprv->gbp_suspended) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"gfx_suspended = %u\n",
			gbprv->gbp_suspended);
	}
#endif /* if GBURST_DEBUG */

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING "hrt_enable_state = %d\n",
		gbprv->gbp_timer_is_enabled);

	tmpv0 = hrtimer_active(&gbprv->gbp_timer);
	if (gbprv->gbp_timer_is_enabled != tmpv0) {
		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING "hrt_enable_state_actual = %d\n",
			tmpv0);
	}

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"vsync_limited_frames mode = %d\n",
		(gbprv->gbp_num_of_vsync_limited_frames >= VSYNC_FRAMES));

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"offscreen_rendering mode = %d\n",
		gbprv->gbp_offscreen_rendering);

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"PWRGT_CNT_last_write = %#8.8x\n",
		gbprv->gbp_pwrgt_cnt_last_written);

	ix = ut_isnprintf(ix, buf, buflen,
		GBURST_HEADING
		"PWRGT_STS = %#8.8x\n", pwrgt_sts);

	{
		u32 tmpv1;
		char sbuf0[32];
		char sbuf1[32];

		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"gpu_burst_interrupt enable = %d\n",
			!!(pwrgt_sts & PWRGT_STS_INT_ENABLE_BIT));

		if (pwrgt_sts & PWRGT_STS_RESERVED_1_BIT)
			ix = ut_isnprintf(ix, buf, buflen,
				GBURST_HEADING
				"reserved_bit_29 = 1\n");

		if (pwrgt_sts & PWRGT_STS_ENABLE_AUTO_BURST_ENTRY_BIT)
			ix = ut_isnprintf(ix, buf, buflen,
				GBURST_HEADING
				"ENABLE_AUTO_BURST_ENTRY = 1\n");

		tmpv0 = (pwrgt_sts & PWRGT_STS_BURST_REQUEST_M) >>
			PWRGT_STS_BURST_REQUEST_P;
		tmpv1 = (pwrgt_sts & PWRGT_STS_BURST_REALIZED_M) >>
			PWRGT_STS_BURST_REALIZED_P;

		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"frequency_requested = %s\n",
			generate_freq_string(tmpv0, sbuf0, sizeof(sbuf0)));

		ix = ut_isnprintf(ix, buf, buflen,
			GBURST_HEADING
			"frequency_realized = %s\n",
			generate_freq_string(tmpv1, sbuf1, sizeof(sbuf1)));
	}

	if (ix >= buflen)
		return -EINVAL;

	printk(GBURST_ALERT
		"Begin - Read from /proc/gburst/dump\n"
		"%s"
		GBURST_HEADING
		"End   - Read from /proc/gburst/dump\n",
		buf);

	return ix;
}


/**
 * pfs_debug_message_read - Procfs read function for
 * /proc/gburst/debug_message read
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Return current enable state (0 or 1) of some debug messages.
 * Associated variable to control debug messages: gburst_debug_msg_on
 */
static int pfs_debug_message_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;

	res = scnprintf(msg, sizeof(msg), "%d\n", gburst_debug_msg_on);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_debug_message_write() - Procfs writer function for
 * /proc/gburst/debug_message write
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * Control for some debug messages: Non-zero to enable, zero to disable.
 * Associated variable to control debug messages: gburst_debug_msg_on
 */
static int pfs_debug_message_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	gburst_debug_msg_on = uval;

	return count;
}


/**
 * pfs_disable_read - Procfs read function for
 * /proc/gburst/disable -- Global and non-privileged disable for gpu burst.
 * This is a non-privileged override that provides a way for any application
 * (especially those that require exclusive use of the performance counters)
 * to disable gburst.
 *
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns current state of this *disable* flag as "0" or "1".
 * Write:
 * 0: Revert any previous disable done via /proc/gburst/disable.
 *    Also happens when writing 1 to /proc/gburst/enable .
 * 1: Disable gpu burst mode.  Stop the timer.  Stop the background thread.
 */
static int pfs_disable_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_request_disable);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_disable_write - Procfs write function for
 * /proc/gburst/disable -- Global and non-privileged disable for gpu burst.
 * This is a non-privileged override that provides a way for any application
 * (especially those that require exclusive use of the performance counters)
 * to disable gburst.
 *
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns current state of this *disable* flag as "0" or "1".
 * Write:
 * 0: Revert any previous disable done via /proc/gburst/disable.
 *    Also happens when writing 1 to /proc/gburst/enable .
 * 1: Disable gpu burst mode.  Stop the timer.  Stop the background thread.
 */
static int pfs_disable_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	if (gbprv->gbp_request_disable != uval) {
		gbprv->gbp_request_disable = uval;

		gburst_enable_set(gbprv);
	}

	return count;
}


/**
 * pfs_dump_read() - Procfs read function for
 * /proc/gburst/dump
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * read: return a verbose textual status dump.
 * write: null.
 */
static int pfs_dump_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[4096];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = generate_dump_string(gbprv, nbytes, msg);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_enable_read - Procfs read function for
 * /proc/gburst/enable -- Global enable/disable for gpu burst.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns current state as "0" or "1".
 * Write:
 * 0: Disable gpu burst mode.  Stop the timer.  Stop the background thread.
 * 1: Enable gpu burst mode.
 *    Also, revert any previous disable done via /proc/gburst/disable.
 */
static int pfs_enable_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_enable);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_enable_write - Procfs write function for
 * /proc/gburst/enable -- Global enable/disable for gpu burst.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns current state as "0" or "1".
 * Write:
 * 0: Disable gpu burst mode.  Stop the timer.  Stop the background thread.
 * 1: Enable gpu burst mode.
 *    Also, revert any previous disable done via /proc/gburst/disable.
 */
static int pfs_enable_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	/*  Setting enable also clears disable. */
	if ((uval && gbprv->gbp_request_disable) ||
		(gbprv->gbp_request_enable != uval)) {

		gbprv->gbp_request_enable = uval;

		if (uval)
			gbprv->gbp_request_disable = 0;

		gburst_enable_set(gbprv);

		if (!gbprv->gbp_enable)
			printk(GBURST_ALERT "gpu burst globally disabled via procfs.\n");
		else
			printk(GBURST_ALERT "gpu burst globally enabled via procfs.\n");
	}

	return count;
}


/**
 * pfs_gpu_monitored_counters_read - Procfs read function for
 * /proc/gburst/monitored_counters -- write or read which hardware counters are
 * monitored and therefore available to be considered in performance
 * calculation.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * The output string is formatted in a way acceptable to use as an input
 * string for writing to this proc file.
 *
 * See function pfs_gpu_monitored_counters_write.
 */
static int pfs_gpu_monitored_counters_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[1024];
	int ix;

	ix = gburst_stats_gfx_hw_perf_counters_to_string(0, msg, sizeof(msg));
	if (ix < 0)
		return ix;

	if (ix >= sizeof(msg))
		return -EINVAL;

	return simple_read_from_buffer(buf, nbytes, ppos, msg, ix);
}


/**
 * pfs_gpu_monitored_counters_write - Procfs write function for
 * /proc/gburst/monitored_counters -- write or read which hardware counters are
 * monitored and therefore available to be considered in performance
 * calculation.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * See comments at function gburst_stats_gfx_hw_perf_counters_set
 * definition, from which the following format description is excerpted:
 *
 * The input string specifies which counters are to be visible as
 * whitespace-separated (e.g., space, tab, newline) groups of: %u:%u:%u:%u
 * which correspond to counter:group:bit:coeff .
 * then assigns the values into data structures for all cores.
 * These per-counter values are:
 * 1.  counter - An index into this module's counter data arrays.
 * 2.  group -- The hardware "group" from which this counter is taken.
 * 3.  bit   -- The hardware bit (for this group) that selects this counter.
 * 4.  coeff -- A counter specific increment value.
 * Example input string: "1:0:1:16   6:0:24:32"
 */
static int pfs_gpu_monitored_counters_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	char buf[128];
	int rva;

	if (copy_from_user_nt(buf, sizeof(buf), buffer, count))
		return -EINVAL;

	rva = gburst_stats_gfx_hw_perf_counters_set(buf);
	if (rva < 0)
		return rva;

	return count;
}


/**
 * pfs_pwrgt_sts_read() - Procfs read function for
 * /proc/gburst/pwrgt_sts -- Return register PWRGT_STS contents.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns register PWRGT_STS content as a hex number
 * with a leading "0x".
 * Write: N/A
 */
static int pfs_pwrgt_sts_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	read_and_process_PWRGT_STS(gbprv);

	res = scnprintf(msg, sizeof(msg), "%#x\n", gbprv->gbp_pwrgt_sts_last_read);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_thermal_override_read() - Procfs read function for
 * /proc/gburst/thermal_override -- thermal state override.  For testing.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * The maximum of the current thermal state and the thermal override state
 * are used to make thermal decisions in the driver.
 * Read: Returns thermal override state.  0 is base state, > 0 indicates
 * hot.  Currently 0..1.
 * Initial value is 0.
 * Write: >= 0 to set thermal override state, -1 to reset.
 * Silently limited to range -1, 0..1.
 * Warning: setting override to 0 disables OS gpu burst thermal controls,
 * although firmware controls will still be in effect.
 */
static int pfs_thermal_override_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_cooldv_state_override);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_thermal_override_write() - Procfs write function for
 * /proc/gburst/thermal_override -- thermal state override.  For testing.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * The maximum of the current thermal state and the thermal override state
 * are used to make thermal decisions in the driver.
 * Read: Returns thermal override state.  0 is base state, > 0 indicates
 * hot.  Currently 0..1.
 * Initial value is 0.
 * Write: >= 0 to set thermal override state, -1 to reset.
 * Silently limited to range -1, 0..1.
 * Warning: setting override to 0 disables OS gpu burst thermal controls,
 * although firmware controls will still be in effect.
 */
static int pfs_thermal_override_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	int sval;
	int rva;

	rva = kstrtoint_from_user(buffer, count, 0, &sval);
	if (rva < 0)
		return rva;

	if (sval < 0) {
		gbprv->gbp_cooldv_state_override = -1;
	} else if (sval <= 1) {
		gbprv->gbp_cooldv_state_override = sval;
	} else {
		gbprv->gbp_cooldv_state_override = -1;
		return -EINVAL;
	}

	return count;
}


/**
 * pfs_thermal_state_read() - Procfs read function for
 * /proc/gburst/thermal_state -- current thermal state.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns current thermal state.  0 is base state, > 0 indicates
 * hot.  Currently 0..1.
 * This value read does not reflect any override that may be in effect.
 * Write: N/A
 */
static int pfs_thermal_state_read(struct file *file, char __user *buf,
		size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_cooldv_state_cur);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_gb_threshold_down_diff_read() - Procfs read function for
 * /proc/gburst/threshold_down_diff -- read/write un-burst trigger threshold
 * as a "down_diff" from the high threshold.  (0-100).
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 */
static int pfs_gb_threshold_down_diff_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_burst_th_down_diff);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_gb_threshold_down_diff_write() - Procfs write function for
 * /proc/gburst/threshold_down_diff -- read/write un-burst trigger threshold
 * as a "down_diff" from the high threshold.  (0-100).
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 */
static int pfs_gb_threshold_down_diff_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	if (uval > 100)
		return -EINVAL;

	printk(GBURST_ALERT "Changing threshold_down_diff from %d to %d\n",
		gbprv->gbp_burst_th_down_diff, uval);

	gbprv->gbp_burst_th_down_diff = uval;
	gbprv->gbp_thold_via = GBP_THOLD_VIA_DOWN_DIFF;

	threshold_derive_low(gbprv);

	return count;
}


/**
 * pfs_gb_threshold_high_write - Procfs write function for
 * /proc/gburst/threshold_high -- read/write burst trigger threshold (0-100).
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 */
static int pfs_gb_threshold_high_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	if (uval > 100)
		return -EINVAL;

	if (gbprv->gbp_burst_th_high != uval) {
		printk(GBURST_ALERT "Changing threshold_high from %u to %u\n",
			gbprv->gbp_burst_th_high, uval);
	}

	gbprv->gbp_burst_th_high = uval;

	threshold_derive_either(gbprv);

	return count;
}


/**
 * pfs_gb_threshold_high_read - Procfs read function for
 * /proc/gburst/threshold_high -- read/write burst trigger threshold (0-100).
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 */
static int pfs_gb_threshold_high_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_burst_th_high);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_gb_threshold_low_read() - Procfs read function for
 * /proc/gburst/threshold_low -- read/write un-burst trigger threshold (0-100).
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 */
static int pfs_gb_threshold_low_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_burst_th_low);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_gb_threshold_low_write() - Procfs write function for
 * /proc/gburst/threshold_low -- read/write un-burst trigger threshold (0-100).
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 */
static int pfs_gb_threshold_low_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	if (uval > 100)
		return -EINVAL;

	printk(GBURST_ALERT "Changing threshold_low from %d to %d\n",
		gbprv->gbp_burst_th_low, uval);

	gbprv->gbp_burst_th_low = uval;
	gbprv->gbp_thold_via = GBP_THOLD_VIA_LOW;

	threshold_derive_down_diff(gbprv);

	return count;
}


/**
 * pfs_state_times_read() - Procfs read function for
 * /proc/gburst/state_times -- uptime and times in states, seconds.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Write: 0, 1, 2 for increasing verbosity of header line output.
 * Read:
 * Three times, which all include time when system or device is suspended:
 * - uptime - time since boot
 * - cumulative time that burst has been requested.
 * - cumulative time that burst has been realized.
 * - cumulative time that gpu frequency has been throttled.
 *
 * The time values are stored internally with nanosecond resolution,
 * but are returned by this read function to microsecond resolution.
 * The times are expressed for this interface as <seconds>.<fraction>.
 * Example output:
 * "  12721.541788       1.349854       1.349790       0.000000"
 */
static int pfs_state_times_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = state_times_to_string(gbprv, "", 0, gbprv->gbp_state_time_header,
		sizeof(msg), msg);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_state_times_write() - Procfs write function for
 * /proc/gburst/state_times -- uptime and times in states, seconds.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * Write: 0, 1, 2 for increasing verbosity of header line output.
 * Read:
 * Three times, which all include time when system or device is suspended:
 * - uptime - time since boot
 * - cumulative time that burst has been requested.
 * - cumulative time that burst has been realized.
 * - cumulative time that gpu frequency has been throttled.
 *
 * The time values are stored internally with nanosecond resolution,
 * but are returned by this read function to microsecond resolution.
 * The times are expressed for this interface as <seconds>.<fraction>.
 * Example output:
 * "  12721.541788       1.349854       1.349790       0.000000"
 */
static int pfs_state_times_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	gbprv->gbp_state_time_header = uval;

	return count;
}


/**
 * pfs_timer_period_usecs_read() - Procfs read function for
 * /proc/gburst/timer_period_usecs -- Kernel thread's timer period, usecs.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read/write.
 */
static int pfs_timer_period_usecs_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	int period_usecs;

	period_usecs = ktime_to_us(gbprv->gbp_hrt_period);
	if (mprm_verbosity >= 3)
		printk(GBURST_ALERT "timer period = %u microseconds\n",
			period_usecs);

	res = scnprintf(msg, sizeof(msg), "%u microseconds\n", period_usecs);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_timer_period_usecs_write() - Procfs write function for
 * /proc/gburst/timer_period_usecs -- Kernel thread's timer period, usecs.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read/write.
 */
static int pfs_timer_period_usecs_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	unsigned int uval;
	int rva;

	/* Get period in microseconds. */
	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	if (uval > USEC_PER_SEC)
		uval = USEC_PER_SEC;

	gbprv->gbp_hrt_period = ktime_set(0, uval * NSEC_PER_USEC);

	/**
	 * Change the time immediately, abandoning current interval.
	 * If this call was not made, then change would take place
	 * after the next timer expiration.
	 */
	hrt_start(gbprv);

	return count;
}


/**
 * pfs_utilization_read() - Procfs read function for
 * /proc/gburst/utilization -- current utilization.  (0 to 100).
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Read: Returns current utilization as a number from 0 to 100 or -1
 * if no value is available.
 * This value read does not reflect any override that may be in effect.
 * Write: N/A
 */
static int pfs_utilization_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_utilization_percentage);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_utilization_override_read() - Procfs read function for
 * /proc/gburst/utilization_override -- utilization override.  For testing.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * If set, a utilization override value to be used instead of actual
 * utilization.
 * Read: Returns current utilization override as a number from 0 to 100
 * or -1 if override state is not set.
 * Write:
 * 0 to 100 - utilization override value or -1 to reset override.
 */
static int pfs_utilization_override_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));

	res = scnprintf(msg, sizeof(msg), "%d\n", gbprv->gbp_utilization_override);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);
}


/**
 * pfs_utilization_override_write - Procfs write function for
 * /proc/gburst/utilization_override -- utilization override.  For testing.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * If set, a utilization override value to be used instead of actual
 * utilization.
 * Read: Returns current utilization override as a number from 0 to 100
 * or -1 if override state is not set.
 * Write:
 * 0 to 100 - utilization override value or -1 to reset override.
 */
static int pfs_utilization_override_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *)PDE_DATA(file_inode(file));
	int sval;
	int rva;

	rva = kstrtoint_from_user(buffer, count, 0, &sval);
	if (rva < 0) {
		printk(GBURST_ALERT "read int failed\n");
		return rva;
	}

	if (sval < 0) {
		gbprv->gbp_utilization_override = -1;
	} else if (sval <= 100) {
		gbprv->gbp_utilization_override = sval;
	} else {
		if (mprm_verbosity >= 4)
			printk(GBURST_ALERT "invalid value\n");
		return -EINVAL;
	}

	return count;
}


/**
 * pfs_verbosity_read() - Procfs read function for
 * /proc/gburst/verbosity -- Specify routine verbosity.
 * Parameters are the standard ones for procfs read functions.
 * @buf: buffer into which output can be provided for read function.
 * @start: May be used to provide multiple buffers of output.
 * @offset: May be used to provide multiple buffers of output.
 * @breq: Number of bytes available in buf.
 * @eof: Set by this function to indicate EOF.
 * @pvd: Private data (in this case, gbprv).
 *
 * Associated variable to control verbosity of message issuance.
 * read:  number >= 0 indicating verbosity, with 0 being most quiet.
 * write: number >= 0 indicating verbosity, with 0 being most quiet.
 */
static int pfs_verbosity_read(struct file *file, char __user *buf,
        size_t nbytes,loff_t *ppos)
{
	char msg[128];
	int res;

	res = scnprintf(msg, sizeof(msg), "%d\n", mprm_verbosity);
	return simple_read_from_buffer(buf, nbytes, ppos, msg, res);

}


/**
 * pfs_verbosity_write() - Procfs write function for
 * /proc/gburst/verbosity -- Specify routine verbosity.
 * Parameters are the standard ones for procfs read functions.
 * @file: Pointer to procfs associated struct file.
 * @buffer: buffer with data written to the proc file, input to this function.
 * @count: number of bytes of data present in buffer.
 * @pvd: Private data (in this case, gbprv).
 *
 * Associated variable to control verbosity of message issuance.
 * read:  number >= 0 indicating verbosity, with 0 being most quiet.
 * write: number >= 0 indicating verbosity, with 0 being most quiet.
 */
static int pfs_verbosity_write(struct file *file,
	const char *buffer, size_t count, loff_t *ppos)
{
	unsigned int uval;
	int rva;

	rva = kstrtouint_from_user(buffer, count, 0, &uval);
	if (rva < 0)
		return rva;

	mprm_verbosity = uval;

	return count;
}


/**
 * pfs_init() - Create all /proc/gburst entries.
 * @gbprv: gb handle.
 *
 * Table driven from pfs_tab.
 */
static void pfs_init(struct gburst_pvt_s *gbprv)
{
	struct proc_dir_entry *pdehdl;
	const struct pfs_data *pfsdat;
	int fmode;
	int ix;
	struct file_operations *pfd_proc_fops;

	/* Create /proc/gburst */
	if (!gbprv->gbp_proc_gburst) {
		/**
		 * gbprv->gbp_proc_parent will be NULL at this point,
		 * which means to create at the top level of /proc.
		 */
		gbprv->gbp_proc_gburst = proc_mkdir(GBURST_PFS_NAME_DIR_GBURST,
			gbprv->gbp_proc_parent);
		if (!gbprv->gbp_proc_gburst) {
			printk(GBURST_ALERT "Error creating gburst proc dir: %s\n",
				GBURST_PFS_NAME_DIR_GBURST);
			return;
		}
	}

	for (ix = 0; ix < ARRAY_SIZE(pfs_tab); ix++) {
		pfsdat = pfs_tab + ix;

		fmode = pfsdat->pfd_mode;
		if (!pfsdat->pfd_func_read)
			fmode &= ~0444;
		if (!pfsdat->pfd_func_write)
			fmode &= ~0222;

                pfd_proc_fops = kzalloc(sizeof(struct file_operations), GFP_KERNEL);
                if(!pfd_proc_fops) {
                    printk(GBURST_ALERT "Error creating gburst proc file: %s\n",
                              pfsdat->pfd_file_name);
                    continue;
                }
                pfd_proc_fops->read = pfsdat->pfd_func_read;
                pfd_proc_fops->write = pfsdat->pfd_func_write;

                pdehdl = proc_create_data(pfsdat->pfd_file_name, fmode,
                               gbprv->gbp_proc_gburst, pfd_proc_fops, gbprv);

		gbprv->gbp_pfs_handle[ix] = pdehdl;
		if (!pdehdl) {
			printk(GBURST_ALERT "Error creating gburst proc file: %s\n",
				pfsdat->pfd_file_name);
		}
	}
}


/**
 * pfs_cleanup() - Remove /proc/gburst files (e.g., at module exit).
 * @gbprv: gb handle.
 */
static void pfs_cleanup(struct gburst_pvt_s *gbprv)
{
	struct proc_dir_entry *pde_root = gbprv->gbp_proc_gburst;
	int ix;

	if (!pde_root)
		return;

	for (ix = 0; ix < ARRAY_SIZE(pfs_tab); ix++) {
		if (gbprv->gbp_pfs_handle[ix]) {
			const struct pfs_data *pfsdat = pfs_tab + ix;

			/**
			 * Function remove_proc_entry may wait for completion
			 * of in-progress operations on the corresponding
			 * proc file.
			 */
			remove_proc_entry(pfsdat->pfd_file_name, pde_root);
			gbprv->gbp_pfs_handle[ix] = NULL;
		}
	}

	remove_proc_entry(GBURST_PFS_NAME_DIR_GBURST, gbprv->gbp_proc_parent);
}


/**
 * gburst_irq_handler() - Interrupt handler for frequency change interrupts.
 * @irq: The irq for this interrupt.
 * @pvd: Private data for this interrupt.  In this case, gbprv.
 *
 * Execution context: hard irq level
 *
 * Warning: Currently, reading or writing registers (via intel_mid_msgbus_read32
 * and intel_mid_msgbus_write32 family) must not be done at interrupt level.
 */
static irqreturn_t gburst_irq_handler(int irq, void *pvd)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *) pvd;

	smp_rmb();
	if (!gbprv->gbp_initialized)
		return IRQ_HANDLED;

#if GBURST_DEBUG
	gbprv->gbp_interrupt_count++;
#endif /* if GBURST_DEBUG */

	if (gbprv->gbp_suspended) {
		/* Interrupt while suspended is not an abnormal event. */
		return IRQ_HANDLED;
	}

	wake_thread(gbprv);

	/**
	 * Potential return values from an interrupt handler:
	 * IRQ_NONE        -- Not our interrupt
	 * IRQ_HANDLED     -- Interrupt handled.
	 * IRQ_WAKE_THREAD -- Pass on to this interrupt's kernel thread.
	 */
	return IRQ_HANDLED;
}


/**
 * gburst_suspend() - Callback for gfx hw transition to state D3.
 * @gbprv: gb handle.
 *
 * Invoked via interrupt/callback.
 * Execution context: non-atomic
 */
static int gburst_suspend(struct gburst_pvt_s *gbprv)
{
	unsigned long long ctime;

	smp_rmb();
	if (!gbprv || !gbprv->gbp_initialized)
		return 0;

	if (mprm_verbosity >= 3)
		printk(GBURST_ALERT "suspend\n");

#if GBURST_DEBUG
	gbprv->gbp_suspend_count++;
#endif

	/* Must update times before changing flag. */
	update_state_times(gbprv, NULL);
	mutex_lock(&gbprv->gbp_mutex_pwrgt_sts);
	gbprv->gbp_suspended = 1;
	mutex_unlock(&gbprv->gbp_mutex_pwrgt_sts);
	smp_wmb();

	/* Cancel timer events. */
	hrt_cancel(gbprv);

	ctime = timestamp();
	if ((ctime-gbprv->gbp_resume_time+FRAME_TIME_BUFFER) < FRAME_DURATION)
		gbprv->gbp_num_of_vsync_limited_frames += 1;
	else
		gbprv->gbp_num_of_vsync_limited_frames = 0;

	write_PWRGT_CNT(gbprv, 0);

/*  GBURST_TIMING_BEFORE_SUSPEND - Code currently under test. */
#define GBURST_TIMING_BEFORE_SUSPEND 1

#if GBURST_TIMING_BEFORE_SUSPEND
	/*
	 * Before suspend takes place, tell gfx hw driver that gpu
	 * frequency is normal.  It is hoped that this will lead to
	 * better stability.
	 * No additional call to update_state_times is desired here, as
	 * the delta time should be insignificant from the previous call.
	 */

	mutex_lock(&gbprv->gbp_mutex_pwrgt_sts);

	if ((gbprv->gbp_pwrgt_sts_last_read & PWRGT_STS_BURST_REALIZED_M)
		!= PWRGT_STS_BURST_REALIZED_M_400) {
		/*  Notify driver. */
		gburst_stats_gpu_freq_mhz_info(
			freq_mhz_table[GBURST_GPU_FREQ_400]);
	}

	gbprv->gbp_pwrgt_sts_last_read = (gbprv->gbp_pwrgt_sts_last_read &
		~(PWRGT_STS_BURST_REQUEST_M | PWRGT_STS_BURST_REALIZED_M))
		| (GBURST_GPU_FREQ_400 << PWRGT_STS_BURST_REQUEST_P)
		| (GBURST_GPU_FREQ_400 << PWRGT_STS_BURST_REALIZED_P);

	mutex_unlock(&gbprv->gbp_mutex_pwrgt_sts);
#endif /* if GBURST_TIMING_BEFORE_SUSPEND */

	/* Clear current utilization value before suspend as there has
	been 5ms GPU idle period anyway due to suspend request */
	if (gbprv->gbp_utilization_percentage > 0)
		gbprv->gbp_utilization_percentage = 0;

	/* Clear utilization check request to avoid extra calculation
	on next freq change wakeup in case timer expires during suspend
	sequence */
	gbprv->gbp_thread_check_utilization = 0;

	/* Clean up GFX load information storage from old and obsolete data */
	gburst_stats_cleanup_gfx_load_data();

	return 0;
}


/**
 * gburst_resume() - Callback for gfx hw transition from state D3.
 * @gbprv: gb handle.
 *
 * Device power on.  Assume the device has retained no state.
 * Invoked via interrupt/callback.
 * Execution context: non-atomic
 */
static int gburst_resume(struct gburst_pvt_s *gbprv)
{
	smp_rmb();

	if (!gbprv || !gbprv->gbp_initialized)
		return 0;

	if (mprm_verbosity >= 3)
		printk(GBURST_ALERT "resume\n");

#if GBURST_DEBUG
	gbprv->gbp_resume_count++;
#endif /* if GBURST_DEBUG */

	update_state_times(gbprv, NULL);

	read_PWRGT_CNT_toggle(gbprv);

	/* Assume thermal state is current or will be updated soon. */

	/* PWRGT_CNT to 0 except toggle and interrupt enable. */
        if ((gbprv->gbp_burst_th_low == 0) &&
             (gbprv->gbp_burst_th_high == 0)) {
	        set_state_pwrgt_cnt(gbprv, PWRGT_CNT_BURST_REQUEST_M_533);
	} else {
		set_state_pwrgt_cnt(gbprv, PWRGT_CNT_BURST_REQUEST_M_400);
	}

	gbprv->gbp_suspended = 0;
	gbprv->gbp_resume_time = timestamp();
	gbprv->gbp_offscreen_rendering = 0;

	smp_wmb();

	mutex_lock(&gbprv->gbp_mutex_pwrgt_sts);
	if (!(read_PWRGT_STS_simple() & PWRGT_STS_FREQ_THROTTLE_M)) {
		hrt_start(gbprv);
	}
	mutex_unlock(&gbprv->gbp_mutex_pwrgt_sts);

	return 0;
}


/**
 * gburst_power_state_set() - Callback informing of gfx hw power state change.
 * @gbprv: gb handle.
 * @st_on: 1 if powering on, 0 if powering down.
 */
static void gburst_power_state_set(struct gburst_pvt_s *gbprv, int st_on)
{
	if (gbprv->gbp_enable) {
		if (st_on)
			gburst_resume(gbprv);
		else
			gburst_suspend(gbprv);
	}
}


/**
 * tcd_get_max_state() - thermal cooling device callback get_max_state.
 * @tcd: Thermal cooling device structure.
 * @pms: Pointer to integer through which output value is stored.
 *
 * Invoked via interrupt/callback.
 * Function return value: 0 if success, otherwise -error.
 * Execution context: non-atomic
 */
static int tcd_get_max_state(struct thermal_cooling_device *tcd,
	unsigned long *pms)
{
	*pms = THERMAL_COOLING_DEVICE_MAX_STATE;

	return 0;
}


/**
 * tcd_get_cur_state() - thermal cooling device callback get_cur_state.
 * @tcd: Thermal cooling device structure.
 * @pcs: Pointer to integer through which output value is stored.
 *
 * Invoked via interrupt/callback.
 * Function return value: 0 if success, otherwise -error.
 * Execution context: non-atomic
 */
static int tcd_get_cur_state(struct thermal_cooling_device *tcd,
	unsigned long *pcs)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *) tcd->devdata;
	*pcs = gbprv->gbp_cooldv_state_cur;

	return 0;
}


/**
 * tcd_set_cur_state() - thermal cooling device callback set_cur_state.
 * @tcd: Thermal cooling device structure.
 * @cs: Input state.
 *
 * Invoked via interrupt/callback.
 * Function return value: 0 if success, otherwise -error.
 * Execution context: non-atomic
 */
static int tcd_set_cur_state(struct thermal_cooling_device *tcd,
	unsigned long cs)
{
	struct gburst_pvt_s *gbprv = (struct gburst_pvt_s *) tcd->devdata;
	if (cs > THERMAL_COOLING_DEVICE_MAX_STATE)
		cs = THERMAL_COOLING_DEVICE_MAX_STATE;

	/* If state change between zero and non-zero... */
	if (!!gbprv->gbp_cooldv_state_cur != !!cs) {
		gbprv->gbp_cooldv_state_prev = gbprv->gbp_cooldv_state_cur;
		gbprv->gbp_cooldv_state_cur = cs;

		if (gbprv->gbp_cooldv_state_highest <
			gbprv->gbp_cooldv_state_cur) {
			gbprv->gbp_cooldv_state_highest =
				gbprv->gbp_cooldv_state_cur;
		}

#if GBURST_DEBUG
		gbprv->gbp_thermal_state_change_count++;
#endif

		if (mprm_verbosity >= 2)
			printk(GBURST_ALERT "Thermal state changed from %d to %d\n",
				gbprv->gbp_cooldv_state_prev,
				gbprv->gbp_cooldv_state_cur);

		request_desired_burst_mode(gbprv);
	}

	return 0;
}


/**
 * gburst_cleanup() -- Clean up module data structures, release resources, etc.
 * @gbprv: gb handle.
 */
static void __exit gburst_cleanup(struct gburst_pvt_s *gbprv)
{
	if (!gbprv)
		return;

	gbprv->gbp_initialized = 0;

	smp_mb();

	{
		struct gburst_interface_s gd_interface;
		memset(&gd_interface, 0, sizeof(struct gburst_interface_s));
		gburst_interface_set_data(&gd_interface);
	}

	if (gbprv->gbp_cooldv_hdl) {
		thermal_cooling_device_unregister(gbprv->gbp_cooldv_hdl);
		gbprv->gbp_cooldv_hdl = NULL;
	}

	write_PWRGT_CNT(gbprv, 0);

	/**
	 * Interestingly, free_irq will (if defined CONFIG_DEBUG_SHIRQ,
	 * for shared interrupts) call the irq handler spuriously
	 * just in order to make sure the driver handles the
	 * spurious call correctly.
	 */
	free_irq(GBURST_IRQ_LEVEL, gbprv);

	/* De-install /proc/gburst entries. */
	if (gbprv->gbp_proc_gburst)
		pfs_cleanup(gbprv);

	hrtimer_cancel(&gbprv->gbp_timer);

	/* stop the thread */
	if (gbprv->gbp_task)
		gburst_thread_stop(gbprv);

	mutex_destroy(&gbprv->gbp_state_times_mutex);
	mutex_destroy(&gbprv->gbp_mutex_pwrgt_sts);
}


/**
 * gburst_init() - gburst module initialization.
 * @gbprv: gb handle.
 *
 * Invokes sub-function to initialize.  If failure, invokes cleanup.
 *
 * Function return value: negative to abort module installation.
 */
static int gburst_init(struct gburst_pvt_s *gbprv)
{
	u32 gt_sts;
	int sts;

	gt_sts = read_PWRGT_STS_simple();

	if (!(gt_sts & PWRGT_STS_BURST_SUPPORT_PRESENT_BIT)) {
		printk(GBURST_ALERT "gpu burst mode is not supported\n");
		return -ENODEV;
	}

	printk(GBURST_ALERT "gpu burst mode initialization -- begin\n");

	mutex_init(&gbprv->gbp_mutex_pwrgt_sts);
	mutex_init(&gbprv->gbp_state_times_mutex);

	gbprv->gbp_request_disable = 0;
	gbprv->gbp_request_enable = mprm_enable;

	gbprv->gbp_enable = gbprv->gbp_request_enable &&
		!gbprv->gbp_request_disable;

	gburst_debug_msg_on = 0;

	/* -1 indicates that no utilization value has been seen. */
	gbprv->gbp_utilization_percentage = -1;

	/* -1 indicates no override is in effect. */
	gbprv->gbp_utilization_override = -1;

	/* No 3D activity is initialized to run until kernel finish
	 * its initialization. so here make the gburst status consistent
	 * with HW
	 * */
	gbprv->gbp_suspended = 1;

	read_PWRGT_CNT_toggle(gbprv);

	{       /* Not a shared interrupt, so no IRQF_SHARED. */
		const unsigned long request_flags = IRQF_TRIGGER_RISING;

		sts = request_irq(GBURST_IRQ_LEVEL, gburst_irq_handler,
			request_flags, GBURST_DRIVER_NAME, gbprv);
		if (sts != 0) {
			printk(GBURST_ALERT "Interrupt assignment failed: %d\n",
				GBURST_IRQ_LEVEL);
			sts = -ENXIO;
			goto err_interrupt;
		}
	}

	/* Set default values for GPU burst control and counters monitored */
	gbprv->gbp_burst_th_high = GBURST_THRESHOLD_DEFAULT_HIGH;

	gbprv->gbp_thold_via = GBP_THOLD_VIA_DOWN_DIFF;
	gbprv->gbp_burst_th_down_diff = GBURST_THRESHOLD_DEFAULT_DOWN_DIFF;

	threshold_derive_either(gbprv);

	gbprv->gbp_hrt_period = ktime_set(0,
		GBURST_TIMER_PERIOD_DEFAULT_USECS * NSEC_PER_USEC);

	{
		struct gburst_interface_s gd_interface;

		gd_interface.gbs_priv = gbprv;
		gd_interface.gbs_power_state_set = gburst_power_state_set;

		gburst_interface_set_data(&gd_interface);
	}

	gbprv->gbp_cooldv_state_override = -1;

	/* Burst dynamic control parameters*/
	gbprv->gbp_resume_time = 0;
	gbprv->gbp_offscreen_rendering = 0;
	gbprv->gbp_num_of_vsync_limited_frames = 0;

	/* Initialize timer.  This does not start the timer. */
	hrtimer_init(&gbprv->gbp_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gbprv->gbp_timer.function = hrt_event_processor;

	{
		static const char *tcd_type = "gpu_burst";
		static const struct thermal_cooling_device_ops tcd_ops = {
			.get_max_state = tcd_get_max_state,
			.get_cur_state = tcd_get_cur_state,
			.set_cur_state = tcd_set_cur_state,
		};
		struct thermal_cooling_device *tcdhdl;

		/**
		  * Example: Thermal zone "type"s and temps milli-deg-C.
		  * These are just examples and are not specific to our usage.
		  *   type              temp
		  *   --------          -------
		  *   skin0             15944
		  *   skin1             22407
		  *   msicdie           37672
		  *
		  * See /sys/class/thermal/thermal_zone<i>
		  * See /sys/class/thermal/cooling_device<i>
		  */

		tcdhdl = thermal_cooling_device_register(
			(char *) tcd_type, gbprv, &tcd_ops);
		if (IS_ERR(tcdhdl)) {
			printk(GBURST_ALERT "Cooling device registration failed: %ld\n",
				-PTR_ERR(tcdhdl));
			sts = PTR_ERR(tcdhdl);
			goto err_thermal;
		}
		gbprv->gbp_cooldv_hdl = tcdhdl;
	}

	if (gbprv->gbp_enable) {
		sts = work_thread_create(gbprv);
		if (sts < 0) {
			/* abort init if unable to create thread. */
			printk(GBURST_ALERT "Thread creation failed: %d\n",
				-sts);
			goto err_thread_creation;

		}
	}

	/**
	 * Ensure that all preceding stores are realized before
	 * any following stores.
	 */
	smp_wmb();

	gbprv->gbp_initialized = 1;

	smp_wmb();

	pfs_init(gbprv);

	printk(GBURST_ALERT "gpu burst mode initialization -- done\n");

	return 0;

err_thread_creation:
	thermal_cooling_device_unregister(gbprv->gbp_cooldv_hdl);
	gbprv->gbp_cooldv_hdl = NULL;

err_thermal:
	{
		struct gburst_interface_s gd_interface;
		memset(&gd_interface, 0, sizeof(struct gburst_interface_s));
		gburst_interface_set_data(&gd_interface);
	}

	free_irq(GBURST_IRQ_LEVEL, gbprv);

err_interrupt:
	mutex_destroy(&gbprv->gbp_state_times_mutex);
	mutex_destroy(&gbprv->gbp_mutex_pwrgt_sts);

	return sts;
}


/**
 * gburst_module_init() - Classic module init function.
 *
 * Calls lower-level initialization.
 *
 * Function return value: negative to abort module installation.
 */
int gburst_module_init(void)
{
	struct gburst_pvt_s *gbprv;
	int rva;

	memset(&gburst_private_data, 0, sizeof(gburst_private_data));
	gburst_private_ptr = &gburst_private_data;
	gbprv = gburst_private_ptr;

	rva = gburst_init(gbprv);

	return rva;
}
EXPORT_SYMBOL(gburst_module_init);


/**
 * gburst_module_exit() - Classic module exit function.
 */
void __exit gburst_module_exit(void)
{
	gburst_cleanup(gburst_private_ptr);
}
EXPORT_SYMBOL(gburst_module_exit);


#if 0
#if (defined MODULE)
module_init(gburst_module_init);
#else
/**
 * Ensure that init happens for this module after init happens for the
 * graphics driver, which in turn is observed to potentially be done late
 * via late_initcall (which is in order right before the late_initcall_sync
 * that this module may use).
 */

late_initcall_sync(gburst_module_init);
#endif
#endif

/* module_exit(gburst_module_exit); */

#endif /* if (defined CONFIG_GPU_BURST) || (defined CONFIG_GPU_BURST_MODULE) */
