/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* License terms: BSD 3-clause "New" or "Revised" License.
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

/* #define DEBUG */
/* #define FORCE_CONSOLE_DEBUG */


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
#define STMVL531_CFG_NETLINK_USER 31

#define STMVL53L1_MAX_CCI_XFER_SZ	256
#define STMVL53L1_DRV_NAME	"stmvl53l1"

/**
 * configure usage of regulator device from device tree info
 * to enable/disable sensor power
 * see module-i2c or module-cci file
 */
/* define CFG_STMVL53L1_HAVE_REGULATOR */

#define STMVL53L1_SLAVE_ADDR	(0x52>>1)

#define DRIVER_VERSION		"1.0.0"

/** enable debug
 * if don't want to have output from vl53l0_dbgmsg, comment out #DEBUG macro
 */
//#define DEBUG	1
/** when defined enable debug direct to main console and log */
//#define FORCE_CONSOLE_DEBUG


/**
 * set to 0 1 static config activate debug from work (data interrupt/polling)
 */
#ifdef DEBUG
#	ifdef FORCE_CONSOLE_DEBUG
#		define vl53l1_dbgmsg(str, ...)\
	printk(KERN_INFO "%s: " str, __func__, ##__VA_ARGS__)
#	else
#		define vl53l1_dbgmsg(str, ...)\
	printk(KERN_DEBUG "%s: " str, __func__, ##__VA_ARGS__)
#	endif
#else
#	define vl53l1_dbgmsg(...) (void)0
#endif

#define WORK_DEBUG	0
#if WORK_DEBUG
#	define work_dbg(msg, ...)\
		printk("[D WK53L1] :" msg "\n", ##__VA_ARGS__)
#else
#	define work_dbg(...) (void)0
#endif

#define vl53l1_info(str, args...) \
	pr_info("%s: " str "\n", __func__, ##args)

#define vl53l1_errmsg(str, args...) \
	pr_err("%s: " str, __func__, ##args)

#define vl53l1_wanrmsg(str, args...) \
	pr_warning("%s: " str, __func__, ##args)

#define VL53L0_VDD_MIN      2600000
#define VL53L0_VDD_MAX      3000000

/* turn off poll log if not defined */
#ifndef STMVL53L1_LOG_POLL_TIMING
#	define STMVL53L1_LOG_POLL_TIMING	0
#endif
/* turn off cci log timing if not defined */
#ifndef STMVL53L1_LOG_CCI_TIMING
#	define STMVL53L1_LOG_CCI_TIMING	0
#endif

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/wait.h>


/** defgroup ipp_abort  abort and stop during ipp call
 *
 *  device will not be locked and IPP can be aborted "while it fly" so we can
 *  accept and executed stop/abort and flush (android) accept other command
 *  in a say "no block"  or reasonable "no wait" time
 *
 *  That is to ensure if anything goes wrong in user space (slow, dies ) driver
 *  is  not maintain in a dead-locked for ever state.
 *
 *  If an ipp is aborted from a stop/abort their's a rare but possible scenarios
 *  where a races can occur causing possibly a deadlock or bad-handling.
 *  This can be eliminated by putting some requirement constrains on
 *  back to back execution of stop re-start (wait previous flying ipp) .
 *
 *  In a  second step we' could make "start " blocking to eliminated this
 *  (extra wait queue)
 */

/** if set to 1 enable ipp execution timing */
#define IPP_LOG_TIMING	1
struct ipp_data_t {
	struct ipp_work_t work;
	struct ipp_work_t work_out;
	int test_n;
	int buzy;	/*!< buzy state 0 is idle
	any other value do not try to use (state value defined in source) */
	int waited_xfer_id;
	/*!< when buzy is set that is the id we are expecting
	 * note that value 0 is reserved and stand for "not waiting"
	 * as such never id 0 will be in any round trip exchange
	 * it's ok for daemon to use 0 in "ping" when it identify himself */
	int status;	/** if that is not 0 do not look at out work data */
	struct mutex mutex;
	/*!< the mutex that shall be held when looking at this struct
	 * but holding the main lock could be enough in opposite holding this
	lock does not granted using the device data without work lock */
	wait_queue_head_t waitq;
	/*!< ipp caller are put in that queue wait while job is posted to user
	 * @warning  ipp and dev mutex will be released before waiting
	 * see @ref ipp_abort */
#if IPP_LOG_TIMING
	struct timeval start_tv, stop_tv;
#endif
};

/*
 *  driver data structs
 */
struct stmvl53l1_data {
	int id;			/*!< multiple device id 0 based*/
	char name[64];		/*!< misc device name */

	VL53L1_DevData_t stdev;	/*!<embed ST VL53L0 Dev data as "stdev" */

	void *client_object;	/*!< cci or i2c moduel i/f speficic ptr  */


	struct mutex work_mutex; /*!< main dev mutex/lock */;
	struct delayed_work	dwork;
	/*!< work for pseudo irq polling check  */

	struct input_dev *input_dev_ps;
	/*!< input device used for sending event */

	/* misc device */
	struct miscdevice miscdev;
	int reset;
	/*!< set when device got reset or re-powered and require more setup */

	/* control data */
	int poll_mode;	/*!< use poll even if interrupt line present*/
	int poll_delay_ms;	/*!< rescheduled time use in poll mode  */
	int enable_sensor;	/*!< actual device enabled state  */
	struct timeval start_tv;/*!< stream start time */
	int enable_debug;

	/* Custom values set by app */

	int preset_mode;	/*!< preset working mode of the device */
	uint32_t timing_budget;	/*!< Timing Budget */

	/* PS parameters */

	/* Calibration parameters */

	/* Calibration values */
	int offset_cal_distance;

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

	/* Device parameters */
	/* Polling thread */
	/* Wait Queue on which the poll thread blocks */

	/* Recent interrupt status */
	/* roi */
	VL53L1_RoiConfig_t roi_cfg;
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
		printk(KERN_DEBUG "IPPSTAT " fmt "\n", ##__VA_ARGS__)
#else
#	define stmvl531_ipp_tim_stop(data) (void)0
#	define stmvl531_ipp_tim_start(data) (void)0
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

int stmvl53l1_ipp_do(struct stmvl53l1_data *data , struct ipp_work_t *work_in,
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
 * @return 0 on success
 */
int stmvl53l1_ipp_enable(int n_dev, struct stmvl53l1_data *data);

/*FIXMEto be removed */
int ipp_test(struct stmvl53l1_data *data);

/*
 *  function pointer structs
 */



#endif /* STMVL53L1_H */
