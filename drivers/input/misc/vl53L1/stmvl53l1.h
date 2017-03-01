/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
*License terms : BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * @file stmvl53l1.h header for vl53l1 sensor driver
 */
#ifndef STMVL53L1_H
#define STMVL53L1_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>

#include "vl53l1_api.h"

/**
 * IPP adapt
 */
#ifdef DEBUG
#	define IPP_PRINT(...) printk(__VA_ARGS__)
#else
#	define IPP_PRINT(...) (void)0
#endif

#include "stmvl53l1_ipp.h"
#include "stmvl53l1_if.h"

/**
 * Configure the Netlink-id use
 */
#define STMVL531_CFG_NETLINK_USER 23

#define STMVL53L1_MAX_CCI_XFER_SZ	256
#define STMVL53L1_DRV_NAME	"stmvl53l1"

/**
 * configure usage of regulator device from device tree info
 * to enable/disable sensor power
 * see module-i2c or module-cci file
 */
/* define CFG_STMVL53L1_HAVE_REGULATOR */

#define DRIVER_VERSION		"8.0.0_b4.1._1268"

/** @ingroup vl53l1_config
 * @{
 */
/**
 * Configure max number of device the driver can support
 */
#define STMVL53L1_CFG_MAX_DEV	2
/** @ingroup vl53l1_mod_dbg
 * @{
 */
#if 1
#define DEBUG	1
#endif
#if 0
#define FORCE_CONSOLE_DEBUG
#endif

extern int stmvl53l1_enable_debug;

#ifdef DEBUG
#	ifdef FORCE_CONSOLE_DEBUG
#define vl53l1_dbgmsg(str, ...) do { \
	if (stmvl53l1_enable_debug) \
		pr_info("%s: " str, __func__, ##__VA_ARGS__); \
	} while (0)
#	else
#define vl53l1_dbgmsg(str, ...) do { \
	if (stmvl53l1_enable_debug) \
		pr_debug("%s: " str, __func__, ##__VA_ARGS__); \
	} while (0)
#	endif
#else
#	define vl53l1_dbgmsg(...) (void)0
#endif

/**
 * set to 0 1 activate or not debug from work (data interrupt/polling)
 */
#define WORK_DEBUG	0
#if WORK_DEBUG
#	define work_dbg(msg, ...)\
		printk("[D WK53L1] :" msg "\n", ##__VA_ARGS__)
#else
#	define work_dbg(...) (void)0
#endif

#define vl53l1_info(str, args...) \
	pr_info("%s: " str, __func__, ##args)

#define vl53l1_errmsg(str, args...) \
	pr_err("%s: " str, __func__, ##args)

#define vl53l1_wanrmsg(str, args...) \
	pr_warn("%s: " str, __func__, ##args)

/* turn off poll log if not defined */
#ifndef STMVL53L1_LOG_POLL_TIMING
#	define STMVL53L1_LOG_POLL_TIMING	0
#endif
/* turn off cci log timing if not defined */
#ifndef STMVL53L1_LOG_CCI_TIMING
#	define STMVL53L1_LOG_CCI_TIMING	0
#endif

/**@} */ /* ingroup mod_dbg*/

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/wait.h>

/** if set to 1 enable ipp execution timing (if debug enabled)
 * @ingroup vl53l1_mod_dbg
 */
#define IPP_LOG_TIMING	1

struct ipp_data_t {
	struct ipp_work_t work;
	struct ipp_work_t work_out;
	int test_n;
	int buzy;	/*!< buzy state 0 is idle
	 * any other value do not try to use (state value defined in source)
	*/
	int waited_xfer_id;
	/*!< when buzy is set that is the id we are expecting
	 * note that value 0 is reserved and stand for "not waiting"
	 * as such never id 0 will be in any round trip exchange
	 * it's ok for daemon to use 0 in "ping" when it identify himself
	*/
	int status;	/** if that is not 0 do not look at out work data */
	wait_queue_head_t waitq;
	/*!< ipp caller are put in that queue wait while job is posted to user
	 * @warning  ipp and dev mutex will be released before waiting
	 * see @ref ipp_abort
	*/
#if IPP_LOG_TIMING
	struct timeval start_tv, stop_tv;
#endif
};

struct stmvl53l1_waiters {
	struct list_head list;
	pid_t pid;
};

/*
 *  driver data structs
 */
struct stmvl53l1_data {
	int id;			/*!< multiple device id 0 based*/
	char name[64];		/*!< misc device name */
	int32_t hw_rev;		/*!< hardware revision number*/

	VL53L1_DevData_t stdev;	/*!<embed ST VL53L0 Dev data as "stdev" */

	void *client_object;	/*!< cci or i2c moduel i/f speficic ptr  */
	bool is_device_remove;	/*!< true when device has been remove */

	struct mutex work_mutex; /*!< main dev mutex/lock */;
	struct delayed_work	dwork;
	/*!< work for pseudo irq polling check  */

	struct input_dev *input_dev_ps;
	/*!< input device used for sending event */

	/* misc device */
	struct miscdevice miscdev;
	/* first irq has no valid data, so avoid to update data on first one */
	int is_first_irq;

	/* control data */
	int poll_mode;	/*!< use poll even if interrupt line present*/
	int poll_delay_ms;	/*!< rescheduled time use in poll mode  */
	int enable_sensor;	/*!< actual device enabled state  */
	struct timeval start_tv;/*!< stream start time */
	int enable_debug;
	bool allow_hidden_start_stop; /*!< allow stop/start sequence in bare */

	/* Custom values set by app */

	int preset_mode;	/*!< preset working mode of the device */
	uint32_t timing_budget;	/*!< Timing Budget */
	int distance_mode;	/*!< distance mode of the device */
	int crosstalk_enable;	/*!< is crosstalk compensation is enable */
	int output_mode;	/*!< output mode of the device */
	bool force_device_on_en;/*!< keep device active when stopped */
	int sar_mode; /*!< is sar mode enable/disabled */
	int cam_mode; /*!< is camera mode enable/disabled */
	VL53L1_Error last_error;/*!< last device internal error */
	/* PS parameters */

	/* Calibration parameters */
	bool is_calibrating;	/*!< active during calibration phases */
	uint16_t xtalk_offset; /*!< crosstalk compensation plane offset kcps */
	int16_t xtalk_x; /*!< crosstalk compensation x plane gradient kcps */
	int16_t xtalk_y; /*!< crosstalk compensation y plane gradient kcps */
	int16_t inner_offset; /*!< inner offset mm */
	int16_t outer_offset; /*!< outer offset mm */

	/* Range Data and stat */
	struct range_t {
		uint32_t	cnt;
		uint32_t	intr;
		int	poll_cnt;
		uint32_t	err_cnt; /* on actual measurement */
		uint32_t	err_tot; /* from start */
		struct timeval start_tv;
		struct timeval comp_tv;
		VL53L1_MultiRangingData_t multi_range_data;
		VL53L1_MultiRangingData_t tmp_range_data;
		/* non mode 1 for data agregation */
	} meas;

	/* workqueue use to fire flush event */
	uint32_t flushCount;
	int flush_todo_counter;

	/* Device parameters */
	/* Polling thread */
	/* Wait Queue on which the poll thread blocks */

	/* Manage blocking ioctls */
	struct list_head simple_data_reader_list;
	struct list_head mz_data_reader_list;
	wait_queue_head_t waiter_for_data;
	bool is_data_valid;

	/* control when using delay is acceptable */
	bool is_delay_allowed;

	/* maintain reset state */
	int reset_state;

	/* Recent interrupt status */
	/* roi */
	VL53L1_RoiConfig_t roi_cfg;

	/* autonomous config */
	uint32_t auto_pollingTimeInMs;
	VL53L1_DetectionConfig_t auto_config;

	/* autonomous config for camera */
	uint32_t auto_pollingTimeInMs_cam;
	VL53L1_DetectionConfig_t auto_config_cam;

	/* Debug */
	struct ipp_data_t ipp;
#if IPP_LOG_TIMING
#	define stmvl531_ipp_tim_stop(data)\
		do_gettimeofday(&data->ipp.stop_tv)
#	define stmvl531_ipp_tim_start(data)\
		do_gettimeofday(&data->ipp.start_tv)
#	define stmvl531_ipp_time(data)\
		stmvl53l1_tv_dif(&data->ipp.start_tv, &data->ipp.stop_tv)
#	define stmvl531_ipp_stat(data, fmt, ...)\
		vl53l1_dbgmsg("IPPSTAT " fmt "\n", ##__VA_ARGS__)
#else
#	define stmvl531_ipp_tim_stop(data) (void)0
#	define stmvl531_ipp_tim_start(data) (void)0
#	define stmvl531_ipp_stat(...) (void)0
#endif
};


/**
 * timeval diff in us
 *
 * @param pstart_tv
 * @param pstop_tv
 */
long stmvl53l1_tv_dif(struct timeval *pstart_tv, struct timeval *pstop_tv);


/**
 * The device table list table is update as device get added
 * we do not support adding removing device mutiple time !
 * use for clean "unload" purpose
 */
extern struct stmvl53l1_data *stmvl53l1_dev_table[];

int stmvl53l1_setup(struct stmvl53l1_data *data);
void stmvl53l1_cleanup(struct stmvl53l1_data *data);
int stmvl53l1_intr_handler(struct stmvl53l1_data *data);


/**
 * request ipp to abort or stop
 *
 * require dev work_mutex held
 *
 * @warning because the "waiting" work can't be aborted we must wake it up
 * it will happen and at some later time not earlier than release of lock
 * if after lock release we have a new request to start the race may not be
 * handled correctly
 *
 * @param data the device
 * @return 0 if no ipp got canceled, @warning this is maybe not grant we
 * can't re-sched "dev work"  and re-run the worker back
 */
int stmvl53l1_ipp_stop(struct stmvl53l1_data *data);

int stmvl53l1_ipp_do(struct stmvl53l1_data *data, struct ipp_work_t *work_in,
		struct ipp_work_t *work_out);

/**
 * per device netlink init
 * @param data
 * @return
 */
int stmvl53l1_ipp_setup(struct stmvl53l1_data *data);
/**
 * per device ipp netlink cleaning
 * @param data
 * @return
 */
void stmvl53l1_ipp_cleanup(struct stmvl53l1_data *data);

/**
 * Module init for netlink
 * @return 0 on success
 */
int stmvl53l1_ipp_init(void);

/**
 * Module exit for netlink
 * @return 0 on success
 */
void __exit stmvl53l1_ipp_exit(void);

/**
 * enable and start ipp exhange
 * @param n_dev number of device to run on
 * @param data  dev struct
 * @return 0 on success
 */
int stmvl53l1_ipp_enable(int n_dev, struct stmvl53l1_data *data);


/*
 *  function pointer structs
 */



#endif /* STMVL53L1_H */
