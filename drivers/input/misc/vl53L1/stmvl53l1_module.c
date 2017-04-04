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
 * @file /stmvl53l1_module.c  vl53l1_module  ST VL53L1 linux kernel module
 *
 * main file
 */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/kthread.h>

/*
 * API includes
 */

#include "stmvl53l1.h"
#include "stmvl53l1-i2c.h"
#include "stmvl53l1_ipp.h"

#include "stmvl53l1_if.h" /* our device interface to user space */
#include "stmvl53l1_internal_if.h"

/** @ingroup vl53l1_config
 * @{
 */
/**
 * default polling period delay in millisecond
 *
 * It can be set at run time via @ref vl53l1_ioctl or @ref sysfs_attrib
 *
 * @note apply only for device operating in polling mode only
 */
#define STMVL53L1_CFG_POLL_DELAY_MS	5

/**
 * default timing budget in microsecond
 *
 * Can be change at run time via @ref vl53l1_ioctl or @ref sysfs_attrib
 */
#define STMVL53L1_CFG_TIMING_BUDGET_US	33000

/** default preset ranging mode */
#define STMVL53L1_CFG_DEFAULT_MODE VL53L1_PRESETMODE_RANGING

/** default distance mode */
#define STMVL53L1_CFG_DEFAULT_DISTANCE_MODE	VL53L1_DISTANCEMODE_LONG

/** default crosstalk enable */
#define STMVL53L1_CFG_DEFAULT_CROSSTALK_ENABLE	1

/** default crosstalk for autonomous mode */
#define STMVL53L1_CFG_DEFAULT_CROSSTALK_AUTONOMOUS	1000

/** max distance value mm */
#define STMVL53L1_MAX_DISTANCE	0x1FFF

/** default output mode */
#define STMVL53L1_CFG_DEFAULT_OUTPUT_MODE	VL53L1_OUTPUTMODE_NEAREST

/** @} */ /* ingroup vl53l1_config */

/** @ingroup vl53l1_mod_dbg
 * @{
 */

/**
 * activate dump of roi in roi ctrl operation
 *
 * @note uses @a vl53l1_dbgmsg for output so make sure to enable debug
 * to get roi dump
 */
#define STMVL53L1_CFG_ROI_DEBUG	0

/** @} */ /* ingroup vl53l1_mod_dbg*/

/* #define DEBUG_TIME_LOG */


#ifdef DEBUG_TIME_LOG
struct timeval start_tv, stop_tv;
#endif

/* Set default value to 1 to allow to see module insertion debug messages */
int stmvl53l1_enable_debug = 1;

#define VL53L1_INPUT_DEVICE_NAME	"STM VL53L1 proximity sensor"

static long stmvl53l1_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg);
static int stmvl53l1_open(struct inode *inode, struct file *file);
static int stmvl53l1_release(struct inode *inode, struct file *file);
static int ctrl_start(struct stmvl53l1_data *data);
static int ctrl_stop(struct stmvl53l1_data *data);
static int ctrl_suspend(struct stmvl53l1_data *data);

#ifdef CONFIG_COMPAT
static long stmvl53l1_compat_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg);
#endif
static bool force_device_on_en_default = false;

module_param(force_device_on_en_default, bool, 0444);
MODULE_PARM_DESC(force_device_on_en_default,
	"select whether force_device_on_en is true or false by default");
/**
 *  module interface struct
 *  interface to platform speficic device handling , concern power/reset ...
 */
struct stmvl53l1_module_fn_t {
	int (*init)(void);	/*!< init */
	/**
	 * clean up job
	 * @param data module specific data ptr
	 */
	void (*deinit)(void *data);
	/**
	 *  give device power
	 * @param data  specific module storage ptr
	 * @return 0 on sucess
	 */
	int (*power_up)(void *data);
	/**
	 *  power down TOFO also stop intr
	 */
	int (*power_down)(void *data);
	/*
	 * release reset so device start.
	 */
	int (*reset_release)(void *data);
	/*
	 * put device under reset.
	 */
	int (*reset_hold)(void *data);

	 /**
	  * enable interrupt
	  *
	  * @param object : interface speficic ptr
	  * @note "module specfic ptr is data->client_object
	  * @return 0 on success else error then drievr wan't start ranging!
	  * if no interrupt or it can't be hooked but still to operated in poll
	  * mode then return 0  and force data->poll_mode
	  * might have to clear poll_mode exlplcilty if to operate in real intr
	  * mode as pool mode
	  * is the default
	  */
	int (*start_intr)(void *object, int *poll_mode);

	void (*clean_up)(void); /*!< optional can be void */

	/* increment reference counter */
	void *(*get)(void *object);

	/* decrement reference counter and deallocate memory when zero */
	void (*put)(void *object);
};

/** i2c module interface*/
static struct stmvl53l1_module_fn_t stmvl53l1_module_func_tbl = {
	.init = stmvl53l1_init_i2c,
	.deinit = stmvl53l1_exit_i2c,
	.power_up = stmvl53l1_power_up_i2c,
	.power_down = stmvl53l1_power_down_i2c,
	.reset_release = stmvl53l1_reset_release_i2c,
	.reset_hold = stmvl53l1_reset_hold_i2c,
	.clean_up = stmvl53l1_clean_up_i2c,
	.start_intr = stmvl53l1_start_intr,
	.get = stmvl53l1_get,
	.put = stmvl53l1_put,
};


#ifndef MIN
#	define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


/*
 * INPUT Subsys interface
 */

static void stmvl53l1_input_push_data(struct stmvl53l1_data *data);

/*
 * Global data
 */

/* We statically inc the value on each probe/setup but it could come from
 * the i2c or platform data and/or device tree
 */
static DEFINE_MUTEX(dev_table_mutex);

/**
 * in-used device LUT
 * we need this as the message reception from netlink message can't
 * associate directly to a device instance that is as we look up id
 * to device data structure
 */
struct stmvl53l1_data *stmvl53l1_dev_table[STMVL53L1_CFG_MAX_DEV];

/**
 * Misc device device operations
 */
static const struct file_operations stmvl53l1_ranging_fops = {
	.owner =		THIS_MODULE,
	.unlocked_ioctl =	stmvl53l1_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	=	stmvl53l1_compat_ioctl,
#endif
	.open =			stmvl53l1_open,
	.release =		stmvl53l1_release,
	/* .flush =		stmvl53l0_flush, */
};

static int store_last_error(struct stmvl53l1_data *data, int rc)
{
	data->last_error = rc;

	return -EIO;
}

static int allocate_dev_id(void)
{
	int i;

	mutex_lock(&dev_table_mutex);

	for (i = 0; i < STMVL53L1_CFG_MAX_DEV; i++)
		if (!stmvl53l1_dev_table[i])
			break;
	i = i < STMVL53L1_CFG_MAX_DEV ? i : -1;

	mutex_unlock(&dev_table_mutex);

	return i;
}

static void deallocate_dev_id(int id)
{
	mutex_lock(&dev_table_mutex);

	stmvl53l1_dev_table[id] = NULL;

	mutex_unlock(&dev_table_mutex);
}

/* helpers to manage reader list for blockint ioctl */
/* call them with lock */
static void empty_and_free_list(struct list_head *head)
{
	struct stmvl53l1_waiters *waiter;
	struct stmvl53l1_waiters *tmp;

	list_for_each_entry_safe(waiter, tmp, head, list) {
		list_del(&waiter->list);
		kfree(waiter);
	}
}

static int add_reader(pid_t pid, struct list_head *head)
{
	struct stmvl53l1_waiters *new_waiter;

	new_waiter = kmalloc(sizeof(struct stmvl53l1_waiters), GFP_KERNEL);
	if (!new_waiter)
		return -ENOMEM;
	new_waiter->pid = pid;
	list_add(&new_waiter->list, head);

	return 0;
}

static bool is_pid_in_list(pid_t pid, struct list_head *head)
{
	struct stmvl53l1_waiters *waiter;

	list_for_each_entry(waiter, head, list)
		if (waiter->pid == pid)
			return true;

	return false;
}

static void wake_up_data_waiters(struct stmvl53l1_data *data)
{
	empty_and_free_list(&data->simple_data_reader_list);
	empty_and_free_list(&data->mz_data_reader_list);
	wake_up(&data->waiter_for_data);
}

static void stmvl53l1_insert_flush_events_lock(struct stmvl53l1_data *data)
{
	while (data->flush_todo_counter) {
		data->flushCount++;
		input_report_abs(data->input_dev_ps, ABS_GAS, data->flushCount);
		input_sync(data->input_dev_ps);
		vl53l1_dbgmsg("Sensor HAL Flush Count = %u\n",
			data->flushCount);
		data->flush_todo_counter--;
	}
}

static int reset_release(struct stmvl53l1_data *data)
{
	int rc;

	if (!data->reset_state)
		return 0;

	vl53l1_dbgmsg("turn on vdd\n");
	rc = stmvl53l1_module_func_tbl.power_up(data->client_object);
	if (rc) {
		vl53l1_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}

	rc = stmvl53l1_module_func_tbl.reset_release(data->client_object);
	if (rc)
		vl53l1_errmsg("reset release fail rc=%d\n", rc);
	else
		data->reset_state = 0;

	return rc;
}

static int reset_hold(struct stmvl53l1_data *data)
{
	int rc;

	if (data->reset_state)
		return 0;

	if (data->force_device_on_en)
		return 0;

	rc = stmvl53l1_module_func_tbl.reset_hold(data->client_object);
	if (!rc)
		data->reset_state = 1;

	vl53l1_dbgmsg("turn off vdd\n");
	rc = stmvl53l1_module_func_tbl.power_down(data->client_object);
	if (rc) {
		vl53l1_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}

	return rc;
}

#ifdef DEBUG_TIME_LOG
static void stmvl53l0_DebugTimeGet(struct timeval *ptv)
{
	do_gettimeofday(ptv);
}

#endif

/**
 *
 * @param pstart_tv time val  starting point
 * @param pstop_tv time val  end point
 * @return time dif in usec
 */
long stmvl53l1_tv_dif(struct timeval *pstart_tv, struct timeval *pstop_tv)
{
	long total_sec, total_usec;

	total_sec = pstop_tv->tv_sec - pstart_tv->tv_sec;
	total_usec = (pstop_tv->tv_usec - pstart_tv->tv_usec);

	return total_sec*1000000+total_usec;
}

#if STMVL53L1_CFG_ROI_DEBUG
static void dump_roi(VL53L1_UserRoi_t *rois, uint32_t n)
{
	uint32_t i;

	vl53l1_dbgmsg("roi dump %d roi:\n", n);
	for (i = 0; i < n ; i++) {
		vl53l1_dbgmsg("ROI#%02d %2d %2d %2d %2d\n", (int)i,
				(int)rois[i].TopLeftX, (int)rois[i].TopLeftY,
				(int)rois[i].BotRightX, (int)rois[i].BotRightY);
	}
}
#else
#	define dump_roi(...) (void)0
#endif

/**
 *
 * @param data device data
 * @return non 0 if current "preset mode" is a multi zone one
 */
static int is_mz_mode(struct stmvl53l1_data *data)
{
	return data->preset_mode == VL53L1_PRESETMODE_RANGING ||
		data->preset_mode  == VL53L1_PRESETMODE_MULTIZONES_SCANNING;
}

static void kill_mz_data(VL53L1_MultiRangingData_t *pdata)
{
	int i;

	memset(pdata, 0, sizeof(*pdata));
	for (i = 0; i < VL53L1_MAX_RANGE_RESULTS; i++)
		pdata->RangeData[i].RangeStatus = -1;
	pdata->RoiStatus = VL53L1_ROISTATUS_NOT_VALID;
}

static void stmvl53l1_setup_auto_config(struct stmvl53l1_data *data)
{
	/* default config is detect object below 300mm with 1s period */
	data->auto_pollingTimeInMs = 1000;
	data->auto_config.DetectionMode = VL53L1_DETECTION_DISTANCE_ONLY;
	data->auto_config.IntrNoTarget = 0;
	data->auto_config.Distance.CrossMode = VL53L1_THRESHOLD_CROSSED_LOW;
	data->auto_config.Distance.High = 1000;
	data->auto_config.Distance.Low = 300;
	data->auto_config.Rate.CrossMode = VL53L1_THRESHOLD_CROSSED_LOW;
	data->auto_config.Rate.High = 0;
	data->auto_config.Rate.Low = 0;
}

/**
  * start sensor
  *
  * @warning must be used if only stopped
  * @param data device data
  * @return 0 on sucess
  */
static int stmvl53l1_start(struct stmvl53l1_data *data)
{
	int rc;
	VL53L1_CalibrationData_t cali;

	data->is_first_irq = true;
	data->is_data_valid = false;

	rc = reset_release(data);
	if (rc)
		goto done;

	/* full setup when out of reset or power up */
	rc = VL53L1_StaticInit(&data->stdev);
	if (rc) {
		vl53l1_errmsg("VL53L1_StaticInit @%d fail %d\n",
				__LINE__, rc);
		rc = store_last_error(data, rc);
		goto done;
	}

	/* activated  stored or last request defined mode */
	rc = VL53L1_SetPresetMode(&data->stdev, data->preset_mode);
	if (rc) {
		vl53l1_errmsg("VL53L1_SetPresetMode %d fail %d",
				data->preset_mode, rc);
		rc = store_last_error(data, rc);
		goto done;
	}

	if (data->preset_mode == VL53L1_PRESETMODE_AUTONOMOUS) {
		/* disable xtalk and set short distance mode
		 * for autonomous mode */
		data->crosstalk_enable = 0;
		data->distance_mode = VL53L1_DISTANCEMODE_SHORT;
	} else {
		/* restore xtalk and distance mode to default
		 * values for non-autonomous mode */
		data->crosstalk_enable =
			STMVL53L1_CFG_DEFAULT_CROSSTALK_ENABLE;
		data->distance_mode =
			STMVL53L1_CFG_DEFAULT_DISTANCE_MODE;
	}

	rc = VL53L1_SetXTalkCompensationEnable(&data->stdev,
		data->crosstalk_enable);
	if (rc) {
		vl53l1_errmsg("VL53L1_SetXTalkCompensationEnable %d fail %d",
				data->crosstalk_enable, rc);
		rc = store_last_error(data, rc);
		goto done;
	}

	/* apply distance mode only in lite and standard ranging */
	rc = VL53L1_SetDistanceMode(&data->stdev, data->distance_mode);
	if (rc) {
		vl53l1_errmsg("VL53L1_SetDistanceMode %d fail %d",
			data->distance_mode, rc);
		rc = store_last_error(data, rc);
		goto done;
	}

	/* apply timing budget */
	rc = VL53L1_SetMeasurementTimingBudgetMicroSeconds(&data->stdev,
			data->timing_budget);
	if (rc) {
		vl53l1_errmsg("SetTimingBudget %d fail %d",
				data->timing_budget, rc);
		rc = store_last_error(data, rc);
		goto done;
	}
	vl53l1_dbgmsg("timing budget @%d\n", data->timing_budget);

	/* aplly roi if any set */
	if (data->roi_cfg.NumberOfRoi) {
		rc = VL53L1_SetROI(&data->stdev, &data->roi_cfg);
		if (rc) {
			vl53l1_errmsg("VL53L1_SetROI fail %d\n", rc);
			rc = store_last_error(data, rc);
			goto done;
		}
		vl53l1_dbgmsg("#%d custom ROI set status\n",
				data->roi_cfg.NumberOfRoi);
	} else {
		vl53l1_dbgmsg("using default ROI\n");
	}

	/* init the timing  */
	do_gettimeofday(&data->start_tv);
	data->meas.start_tv = data->start_tv;
	/* init the ranging data => kill the previous ranging mz data */
	kill_mz_data(&data->meas.multi_range_data);

	memset(&cali, 0, sizeof(cali));
	rc = VL53L1_GetCalibrationData(&data->stdev, &cali);
	if (rc) {
		vl53l1_errmsg("VL53L1_GetCalibrationData fail\n");
		rc = store_last_error(data, rc);
		goto done;
	}

	/* set autonomous mode configuration */
	if (data->preset_mode == VL53L1_PRESETMODE_AUTONOMOUS) {
		if (data->cam_mode == 0) {
			rc = VL53L1_SetInterMeasurementPeriodMilliSeconds(
					&data->stdev,
					data->auto_pollingTimeInMs);
			if (rc) {
				vl53l1_errmsg("Fail to set auto period %d\n",
						rc);
				rc = store_last_error(data, rc);
				goto done;
			}
			rc = VL53L1_SetThresholdConfig(&data->stdev,
					&data->auto_config);
			if (rc) {
				vl53l1_errmsg("Fail to set auto config %d\n",
						rc);
				rc = store_last_error(data, rc);
				goto done;
			}
		} else {
			/* set camera autonomous configration */
			rc = VL53L1_SetInterMeasurementPeriodMilliSeconds(
					&data->stdev,
					data->auto_pollingTimeInMs_cam);
			if (rc) {
				vl53l1_errmsg("Fail to set auto period %d\n",
						rc);
				rc = store_last_error(data, rc);
				goto done;
			}
			rc = VL53L1_SetThresholdConfig(&data->stdev,
					&data->auto_config_cam);
			if (rc) {
				vl53l1_errmsg("Fail to set auto config %d\n",
						rc);
				rc = store_last_error(data, rc);
				goto done;
			}
		}
		cali.customer.algo__crosstalk_compensation_plane_offset_kcps
			= STMVL53L1_CFG_DEFAULT_CROSSTALK_AUTONOMOUS;
	} else {
		cali.customer.algo__crosstalk_compensation_plane_offset_kcps
			= data->xtalk_offset;
	}

	rc = VL53L1_SetCalibrationData(&data->stdev, &cali);
	if (rc) {
		vl53l1_errmsg("VL53L1_SetCalibrationData fail\n");
		rc = store_last_error(data, rc);
		goto done;
	}

	data->allow_hidden_start_stop =
		(data->distance_mode == VL53L1_DISTANCEMODE_AUTO ||
		data->distance_mode == VL53L1_DISTANCEMODE_AUTO_LITE);
	/* kick off ranging */
	rc = VL53L1_StartMeasurement(&data->stdev);
	if (rc) {
		vl53l1_errmsg("VL53L1_StartMeasurement @%d fail %d",
				__LINE__, rc);
		rc = store_last_error(data, rc);
		goto done;
	}

	data->meas.cnt = 0;
	data->meas.err_cnt = 0;
	data->meas.err_tot = 0;
	data->meas.poll_cnt = 0;
	data->meas.intr = 0;
	data->enable_sensor = 1;
	if (data->poll_mode) {
		/* kick off the periodical polling work */
		schedule_delayed_work(&data->dwork,
			msecs_to_jiffies(data->poll_delay_ms));
	}
done:
	return rc;
}

/**
 * stop sensor
 *
 * work lock must be held
 * @warning to be used if only started!
 */
static int stmvl53l1_stop(struct stmvl53l1_data *data)
{
	int rc;

	rc = VL53L1_StopMeasurement(&data->stdev);
	if (rc) {
		vl53l1_errmsg("VL53L1_StopMeasurement @%d fail %d",
				__LINE__, rc);
		rc = store_last_error(data, rc);
	}
	/* put device under reset */
	/* do we ask explicit intr stop or just use stop */
	reset_hold(data);

	data->enable_sensor = 0;
	data->timing_budget = STMVL53L1_CFG_TIMING_BUDGET_US;
	if (data->poll_mode) {
		/* cancel periodical polling work */
		cancel_delayed_work(&data->dwork);
	}

	/* if we are in ipp waiting mode then abort it */
	stmvl53l1_ipp_stop(data);
	/* wake up all waiters */
	/* they will receive -ENODEV error */
	wake_up_data_waiters(data);

	return rc;
}

static int activate_sar_mode(struct stmvl53l1_data *data)
{
	int rc = VL53L1_ERROR_NONE;

	if (data == NULL)
		return -EFAULT;

	if (data->enable_sensor == 0 &&
		!data->is_calibrating &&
		data->sar_mode == 1 &&
		data->cam_mode != 1) {
		data->preset_mode = VL53L1_PRESETMODE_AUTONOMOUS;
		rc = stmvl53l1_start(data);
		if (rc == VL53L1_ERROR_NONE)
			vl53l1_info("sensor in sar mode!\n");
		else
			vl53l1_errmsg("fail to active sar err %d\n", rc);
	} else
		vl53l1_info("skip sar activation\n");

	return rc;
}

static int deactivate_sar_mode(struct stmvl53l1_data *data)
{
	int rc = VL53L1_ERROR_NONE;

	if (data == NULL)
		return -EFAULT;

	if (data->enable_sensor == 1 &&
		!data->is_calibrating &&
		data->preset_mode == VL53L1_PRESETMODE_AUTONOMOUS &&
		data->cam_mode != 1) {
		rc = stmvl53l1_stop(data);
		if (rc == VL53L1_ERROR_NONE) {
			vl53l1_info("sensor is out of sar mode!\n");
			/* set preset mode back to default ranging mode */
			data->preset_mode = VL53L1_PRESETMODE_RANGING;
		} else
			vl53l1_errmsg("fail to deactivate sar err %d\n", rc);
	} else
		vl53l1_info("sar is not actived, skip deactivation\n");

	return rc;
}

/*
 * SysFS support
 */
static ssize_t stmvl53l1_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return snprintf(buf, 5, "%d\n", data->enable_sensor);
}

static ssize_t stmvl53l1_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	unsigned long val;

	rc = kstrtoul(buf, 10, &val);
	if (rc) {
		vl53l1_errmsg("enable sensor syntax in %s\n", buf);
		return -EINVAL;
	}
	val = val ? 1 : 0;
	vl53l1_dbgmsg("enable_sensor is %d => %d\n",
		data->enable_sensor, (int)val);

	rc = val == 1 ? ctrl_start(data) : ctrl_stop(data);
	vl53l1_dbgmsg("End\n");

	return rc ? rc : count;
}

/**
 * sysfs attribute "enable_ps_sensor" [rd/wr]
 *
 * @li read show the current enable state
 * @li write set the new state value "0" put sensor off "1"  put it on
 *
 * @return  0 on success , EINVAL if fail to start
 *
 * @warning their's no check and assume exclusive usage of sysfs and ioctl\n
 * Sensor will be put on/off disregard of any setup done by the ioctl channel.
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(enable_ps_sensor, 0664/*S_IWUGO | S_IRUGO*/,
		stmvl53l1_show_enable_ps_sensor,
		stmvl53l1_store_enable_ps_sensor);

static int stmvl53l1_set_poll_delay_ms(struct stmvl53l1_data *data, int delay)
{
	int rc = 0;

	if (delay <= 0)
		rc = -EINVAL;
	else
		data->poll_delay_ms = delay;

	return rc;
}

static ssize_t stmvl53l1_show_poll_delay_ms(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->poll_delay_ms);
}

/* for work handler scheduler time */
static ssize_t stmvl53l1_store_poll_delay_ms(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int delay;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &delay)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_poll_delay_ms(data, delay);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute "poll_delay_ms" [rd/wr]
 *
 * @li read show the current polling delay in millisecond
 * @li write set the new polling delay in millisecond
 *
 * @note apply only if device is in polling mode\n
 * for best performances (minimal delay and cpu load ) set it to the device
 * period operating period +1 millis

 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(set_delay_ms, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_poll_delay_ms,
				stmvl53l1_store_poll_delay_ms);

/* Timing Budget */
static int stmvl53l1_set_timing_budget(struct stmvl53l1_data *data, int timing)
{
	int rc = 0;

	if (timing <= 0) {
		vl53l1_errmsg("invalid timing valid %d\n", timing);
		rc = -EINVAL;
	} else if (data->enable_sensor) {
		rc = VL53L1_SetMeasurementTimingBudgetMicroSeconds(&data->stdev,
			timing);
		if (rc) {
			vl53l1_errmsg("SetTimingBudget %d fail %d", timing, rc);
			rc = store_last_error(data, rc);
		} else
			data->timing_budget = timing;
	} else
		data->timing_budget = timing;

	return rc;
}

static ssize_t stmvl53l1_show_timing_budget(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int n;
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->work_mutex);

	n = scnprintf(buf, PAGE_SIZE, "%d\n", data->timing_budget);

	mutex_unlock(&data->work_mutex);

	return n;
}

static ssize_t stmvl53l1_store_timing_budget(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int timing;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &timing)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_timing_budget(data, timing);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs "timing_budget"  [rd/wr]
 *
 *  set or get the ranging timing budget in microsecond
 *
 *  @warning can only be set while not ranging , trying to set it while ranging
 *  is an error (EBUSY)
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(timing_budget, 0660/*S_IWUGO | S_IRUGO*/,
			stmvl53l1_show_timing_budget,
			stmvl53l1_store_timing_budget);


static ssize_t stmvl53l1_show_roi(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int i;
	int n;


	mutex_lock(&data->work_mutex);
	if (data->roi_cfg.NumberOfRoi == 0) {
		/* none define by user */
		/* we could get what stored but may not even be default */
		n = scnprintf(buf, PAGE_SIZE, "device default\n");
	} else {
		for (i = 0, n = 0; i < data->roi_cfg.NumberOfRoi; i++) {
			n += scnprintf(buf+n, PAGE_SIZE-n, "%d %d %d %d%c",
					data->roi_cfg.UserRois[i].TopLeftX,
					data->roi_cfg.UserRois[i].TopLeftY,
					data->roi_cfg.UserRois[i].BotRightX,
					data->roi_cfg.UserRois[i].BotRightY,
					i == data->roi_cfg.NumberOfRoi-1 ?
							'\n' : ',');
		}
	}
	mutex_unlock(&data->work_mutex);
	return n;
}


static const char str_roi_ranging[] = "ERROR can't set roi while ranging\n";

static ssize_t stmvl53l1_store_roi(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	VL53L1_UserRoi_t rois[VL53L1_MAX_USER_ZONES];
	int rc;

	mutex_lock(&data->work_mutex);
	if (data->enable_sensor) {
		vl53l1_errmsg(" cant set roi now\n");
		rc = -EBUSY;
	} else {
		int n, n_roi = 0;
		const char *pc = buf;
		int tlx, tly, brx, bry;

		while (n_roi < VL53L1_MAX_USER_ZONES && pc != NULL
				&& *pc != 0 && *pc != '\n') {
			n = sscanf(pc, "%d %d %d %d", &tlx, &tly, &brx, &bry);
			if (n == 4) {
				rois[n_roi].TopLeftX = tlx;
				rois[n_roi].TopLeftY = tly;
				rois[n_roi].BotRightX = brx;
				rois[n_roi].BotRightY = bry;
				n_roi++;
			} else {
				vl53l1_errmsg(
"wrong roi #%d syntax around %s of %s", n_roi, pc, buf);
				n_roi = -1;
				break;
			}
			/* find next roi separator */
			pc = strchr(pc, ',');
			if (pc)
				pc++;
		}
		/*if any set them */
		if (n_roi >= 0) {
			if (n_roi)
				memcpy(data->roi_cfg.UserRois, rois,
						n_roi*sizeof(rois[0]));
			data->roi_cfg.NumberOfRoi = n_roi;
			dump_roi(data->roi_cfg.UserRois,
					data->roi_cfg.NumberOfRoi);
			rc = count;
		} else {
			rc = -EINVAL;
		}
	}
	mutex_unlock(&data->work_mutex);
	vl53l1_dbgmsg("ret %d count %d\n", rc, (int)count);

	return rc;
}

/**
 * sysfs attribute "roi" [rd/wr]
 *
 * @li read show the current user customized roi setting
 * @li write set user custom roi, it can only be done while not ranging.
 *
 * syntax for set input roi
 * @li "[tlx tly brx bry,]\n" repeat n time require will set the n roi
 * @li "\n" will reset
 *
 * @warning roi coordinate is not image x,y(down) but euclidian x,y(up)
 *
 * @warning roi validity is only check at next range start
 * @warning user is responsible to set approriate number an roi before each mode
 * change
 * @note roi can be retrun to defautl by setting none ""
 *
 *@code
 * >#to set 2x roi
 * >echo "0 15 15 0, 0 8 8 0" > /sys/class/input6/roi
 * >echo $?
 * 0
 * >cat /sys/class/input6/roi
 * "0 15 15 0,0 8 8 0"
 * #to cancel user define roi"
 * >echo "" > /sys/class/input1/roi
 * >echo $?
 * 0
 * >echo "1" > /sys/class/input6/enable_ps_senspor
 * #try to set roi while ranging
 * >echo "0 15 15 0, 0 8 8 0" > /sys/class/input6/roi
 * [58451.912109] stmvl53l1_store_roi:  cant set roi now
 * >echo $?
 * 1
 *@endcode
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(roi, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_roi,
				stmvl53l1_store_roi);


static int stmvl53l1_set_mode(struct stmvl53l1_data *data, int mode)
{
	int rc = 0;

	if (data->enable_sensor) {
		vl53l1_errmsg("can't change mode while ranging\n");
		rc = -EBUSY;
	} else {
		switch (mode) {
		case VL53L1_PRESETMODE_RANGING:
		case VL53L1_PRESETMODE_MULTIZONES_SCANNING:
		case VL53L1_PRESETMODE_LITE_RANGING:
		case VL53L1_PRESETMODE_AUTONOMOUS:
			data->preset_mode = mode;
			break;
		default:
			vl53l1_errmsg("invalid mode %d\n", mode);
			rc = -EINVAL;
			break;
		}
	}

	return rc;
}

static ssize_t stmvl53l1_show_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->preset_mode);
}

static ssize_t stmvl53l1_store_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int mode;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &mode)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_mode(data, mode);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute "mode " [rd/wr]
 *
 * set the mode value can only be used while: not ranging
 * @li 1 @a VL53L1_PRESETMODE_RANGING default ranging
 * @li 2 @a VL53L1_PRESETMODE_MULTIZONES_SCANNING multiple zone
 * @li 3 @a VL53L1_PRESETMODE_AUTONOMOUS autonomous mode
 * @li 4 @a VL53L1_PRESETMODE_LITE_RANGING low mips ranging mode
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(mode, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_mode,
				stmvl53l1_store_mode);

static int stmvl53l1_set_distance_mode(struct stmvl53l1_data *data,
	int distance_mode)
{
	int rc = 0;

	if (data->enable_sensor) {
		vl53l1_errmsg("can't change distance mode while ranging\n");
		rc = -EBUSY;
	} else {
		switch (distance_mode) {
		case VL53L1_DISTANCEMODE_SHORT:
		case VL53L1_DISTANCEMODE_MEDIUM:
		case VL53L1_DISTANCEMODE_LONG:
		case VL53L1_DISTANCEMODE_AUTO_LITE:
		case VL53L1_DISTANCEMODE_AUTO:
			data->distance_mode = distance_mode;
			break;
		default:
			vl53l1_errmsg("invalid distance mode %d\n",
				distance_mode);
			rc = -EINVAL;
			break;
		}
	}

	return rc;
}

static ssize_t stmvl53l1_show_distance_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->distance_mode);
}

static ssize_t stmvl53l1_store_distance_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int distance_mode;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &distance_mode)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_distance_mode(data, distance_mode);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute " distance mode" [rd/wr]
 *
 * set the distance mode value can only be used while: not ranging
 * @li 1 @a VL53L1_DISTANCEMODE_SHORT
 * @li 2 @a VL53L1_DISTANCEMODE_MEDIUM
 * @li 3 @a VL53L1_DISTANCEMODE_LONG
 * @li 4 @a VL53L1_DISTANCEMODE_AUTO_LITE
 * @li 5 @a VL53L1_DISTANCEMODE_AUTO
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(distance_mode, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_distance_mode,
				stmvl53l1_store_distance_mode);

static int stmvl53l1_set_crosstalk_enable(struct stmvl53l1_data *data,
	int crosstalk_enable)
{
	int rc = 0;

	if (data->enable_sensor) {
		vl53l1_errmsg("can't change crosstalk enable while ranging\n");
		rc = -EBUSY;
	} else if (crosstalk_enable == 0 || crosstalk_enable == 1) {
		data->crosstalk_enable = crosstalk_enable;
	} else {
		vl53l1_errmsg("invalid crosstalk enable %d\n",
			crosstalk_enable);
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t stmvl53l1_show_crosstalk_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->crosstalk_enable);
}

static ssize_t stmvl53l1_store_crosstalk_enable(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int crosstalk_enable;

	mutex_lock(&data->work_mutex);
	if (kstrtoint(buf, 0, &crosstalk_enable)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_crosstalk_enable(data, crosstalk_enable);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute " crosstalk enable" [rd/wr]
 *
 * control if crosstalk compensation is eanble or not
 * @li 0 disable crosstalk compensation
 * @li 1 enable crosstalk compensation
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(crosstalk_enable, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_crosstalk_enable,
				stmvl53l1_store_crosstalk_enable);

static int stmvl53l1_set_output_mode(struct stmvl53l1_data *data,
	int output_mode)
{
	int rc = 0;

	if (data->enable_sensor) {
		vl53l1_errmsg("can't change output mode while ranging\n");
		rc = -EBUSY;
	} else {
		switch (output_mode) {
		case VL53L1_OUTPUTMODE_NEAREST:
		case VL53L1_OUTPUTMODE_STRONGEST:
			data->output_mode = output_mode;
			break;
		default:
			vl53l1_errmsg("invalid output mode %d\n", output_mode);
			rc = -EINVAL;
			break;
		}
	}

	return rc;
}

static ssize_t stmvl53l1_show_output_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->output_mode);
}

static ssize_t stmvl53l1_store_output_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int output_mode;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &output_mode)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_output_mode(data, output_mode);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute " output mode" [rd/wr]
 *
 * set the output mode value can only be used while: not ranging
 * @li 1 @a VL53L1_OUTPUTMODE_NEAREST
 * @li 2 @a VL53L1_OUTPUTMODE_STRONGEST
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(output_mode, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_output_mode,
				stmvl53l1_store_output_mode);


static int stmvl53l1_set_force_device_on_en(struct stmvl53l1_data *data,
	int force_device_on_en)
{
	int rc;

	if (force_device_on_en != 0 && force_device_on_en != 1) {
		vl53l1_errmsg("invalid force_device_on_en mode %d\n",
			force_device_on_en);
		return -EINVAL;
	}

	data->force_device_on_en = force_device_on_en;

	/* don't update reset if sensor is enable */
	if (data->enable_sensor)
		return 0;

	/* ok update reset according force_device_on_en value */
	if (force_device_on_en)
		rc = reset_release(data);
	else
		rc = reset_hold(data);

	return rc;
}

static ssize_t stmvl53l1_show_force_device_on_en(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->force_device_on_en);
}

static ssize_t stmvl53l1_store_force_device_on_en(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc;
	int force_device_on_en;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &force_device_on_en)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		rc = stmvl53l1_set_force_device_on_en(data, force_device_on_en);

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute " force_device_on_enable" [rd/wr]
 *
 * Control if device is put under reset when stopped.
 * @li 0 feature is disable. Device is put under reset when stopped.
 * @li 1 feature is enable. Device is not put under reset when stopped.
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(force_device_on_enable, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_force_device_on_en,
				stmvl53l1_store_force_device_on_en);

static ssize_t stmvl53l1_do_flush(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count) {
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->work_mutex);

	data->flush_todo_counter++;
	stmvl53l1_insert_flush_events_lock(data);

	mutex_unlock(&data->work_mutex);

	return count;
}

static DEVICE_ATTR(do_flush, 0660/*S_IWUGO | S_IRUGO*/,
			NULL,
			stmvl53l1_do_flush);

static ssize_t stmvl53l1_show_enable_debug(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", stmvl53l1_enable_debug);
}

static ssize_t stmvl53l1_store_enable_debug(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int enable_debug;
	int rc = 0;

	if (kstrtoint(buf, 0, &enable_debug)) {
		vl53l1_errmsg("invalid syntax in %s", buf);
		rc = -EINVAL;
	} else
		stmvl53l1_enable_debug = enable_debug;

	return rc ? rc : count;
}

/**
 * sysfs attribute " debug enable" [rd/wr]
 *
 * dynamic control of vl53l1_dbgmsg messages. Note that in any case your code
 * must be enable with DEBUG in stmvl53l1.h at compile time.
 * @li 0 disable vl53l1_dbgmsg messages
 * @li 1 enable vl53l1_dbgmsg messages
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(enable_debug, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_enable_debug,
				stmvl53l1_store_enable_debug);

static ssize_t display_FixPoint1616(char *buf, size_t size, FixPoint1616_t fix)
{
	uint32_t msb = fix >> 16;
	uint32_t lsb = fix & 0xffff;

	lsb = (lsb * 1000000ULL + 32768) / 65536;

	return scnprintf(buf, size, "%d.%06d", msb, (uint32_t) lsb);
}

static ssize_t stmvl53l1_show_autonomous_config(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	ssize_t res = 0;

	res += scnprintf(&buf[res], PAGE_SIZE, "%d %d %d %d %d %d %d ",
		data->auto_pollingTimeInMs,
		data->auto_config.DetectionMode,
		data->auto_config.IntrNoTarget,
		data->auto_config.Distance.CrossMode,
		data->auto_config.Distance.High,
		data->auto_config.Distance.Low,
		data->auto_config.Rate.CrossMode);

	res += display_FixPoint1616(&buf[res], PAGE_SIZE - res,
		data->auto_config.Rate.High);

	res += scnprintf(&buf[res], PAGE_SIZE - res, " ");

	res += display_FixPoint1616(&buf[res], PAGE_SIZE - res,
		data->auto_config.Rate.Low);

	res += scnprintf(&buf[res], PAGE_SIZE - res, "\n");

	return res;
}

static const char *parse_integer(const char *buf, int *res)
{
	int rc;

	while (*buf == ' ')
		buf++;
	rc = sscanf(buf, "%d ", res);
	if (!rc)
		return NULL;

	return strchr(buf, ' ');
}

static bool is_float_format(const char *buf, bool is_last)
{
	char *dot = strchr(buf, '.');
	char *space_or_new_line = strchr(buf, is_last ? '\n' : ' ');

	if (!space_or_new_line)
		return !!dot;
	if (!dot)
		return false;

	return dot < space_or_new_line ? true : false;
}

static int parse_FixPoint16x16_lsb(const char *lsb_char)
{
	int lsb = 0;
	int digit_nb = 0;

	/* parse at most 6 digits */
	lsb_char++;
	while (*lsb_char != ' ' && *lsb_char != '\n' && digit_nb < 6) {
		lsb = lsb * 10 + (*lsb_char - '0');
		lsb_char++;
		digit_nb++;
	}
	while (digit_nb++ < 6)
		lsb = lsb * 10;

	return div64_s64(lsb * 65536ULL + 500000, 1000000);
}

/* parse next fix point value and return a pointer to next blank or newline
 * character according to is_last parameter.
 * parse string must have digit for integer part (something like '.125' will
 * return an error) or an error will be return. Only the first 6 digit of the
 * decimal part will be parsed.
 */
static const char *parse_FixPoint16x16(const char *buf, FixPoint1616_t *res,
	bool is_last)
{
	bool is_float;
	int msb;
	int lsb = 0;
	int rc;

	while (*buf == ' ')
		buf++;
	is_float = is_float_format(buf, is_last);

	/* scan msb */
	rc = sscanf(buf, "%d ", &msb);
	if (!rc)
		return NULL;
	/* then lsb if present */
	if (is_float)
		lsb = parse_FixPoint16x16_lsb(strchr(buf, '.'));
	*res = (msb << 16) + lsb;

	return strchr(buf, is_last ? '\n' : ' ');
}

static ssize_t stmvl53l1_store_autonomous_config(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int pollingTimeInMs, DetectionMode, IntrNoTarget;
	int d_CrossMode, d_High, d_Low;
	int r_CrossMode;
	FixPoint1616_t r_High, r_Low;
	const char *buf_ori = buf;
	int rc;

	mutex_lock(&data->work_mutex);

	buf = parse_integer(buf, &pollingTimeInMs);
	if (!buf)
		goto invalid;
	buf = parse_integer(buf, &DetectionMode);
	if (!buf)
		goto invalid;
	buf = parse_integer(buf, &IntrNoTarget);
	if (!buf)
		goto invalid;
	buf = parse_integer(buf, &d_CrossMode);
	if (!buf)
		goto invalid;
	buf = parse_integer(buf, &d_High);
	if (!buf)
		goto invalid;
	buf = parse_integer(buf, &d_Low);
	if (!buf)
		goto invalid;
	buf = parse_integer(buf, &r_CrossMode);
	if (!buf)
		goto invalid;
	buf = parse_FixPoint16x16(buf, &r_High, false);
	if (!buf)
		goto invalid;
	buf = parse_FixPoint16x16(buf, &r_Low, true);
	if (!buf)
		goto invalid;

	data->auto_pollingTimeInMs = pollingTimeInMs;
	data->auto_config.DetectionMode = DetectionMode;
	data->auto_config.IntrNoTarget = IntrNoTarget;
	data->auto_config.Distance.CrossMode = d_CrossMode;
	data->auto_config.Distance.High = d_High;
	data->auto_config.Distance.Low = d_Low;
	data->auto_config.Rate.CrossMode = r_CrossMode;
	data->auto_config.Rate.High = r_High;
	data->auto_config.Rate.Low = r_Low;

	mutex_unlock(&data->work_mutex);

	return count;

invalid:
	vl53l1_errmsg("invalid syntax in %s", buf_ori);
	rc = -EINVAL;
	goto error;

error:
	mutex_unlock(&data->work_mutex);

	return rc;
}

/**
 * sysfs attribute " autonomous_config" [rd/wr]
 *
 * Will set/get autonomous configuration using sysfs.
 *
 * format is the following :
 * <poll_ms> <mode> <no_target_irq> <distance_mode> <distance_low>
 * <distance_high> <rate_mode> <rate_low> <rate_high>
 *
 *@code
 * > echo "1000 1 0 0 1000 300 0 2.2 1.001" > autonomous_config
 *@endcode
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(autonomous_config, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_autonomous_config,
				stmvl53l1_store_autonomous_config);
static ssize_t stmvl53l1_show_last_error_config(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->last_error);
}

static ssize_t stmvl53l1_show_enable_sar(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", data->sar_mode);
}

static ssize_t stmvl53l1_store_enable_sar(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	int rc = 0;
	int mode;

	mutex_lock(&data->work_mutex);

	if (kstrtoint(buf, 0, &mode)) {
		vl53l1_errmsg("invalid syntax in %s\n", buf);
		rc = -EINVAL;
	} else {
		data->sar_mode = mode?1:0;
		vl53l1_dbgmsg("sar mode is set to %d\n", data->sar_mode);

		if (data->sar_mode == 1) {
			if (mode == 1) {
				/* report far distance as initial value */
				struct input_dev *input = data->input_dev_ps;

				input_report_abs(input, ABS_HAT1X, 100);
				input_report_abs(input, ABS_HAT1X, 101);
				input_report_abs(input, ABS_HAT1X, 100);
				input_sync(input);
			}
			activate_sar_mode(data);
		} else
			deactivate_sar_mode(data);
	}

	mutex_unlock(&data->work_mutex);

	return rc ? rc : count;
}

/**
 * sysfs attribute "enable_sar" [rd/wr]
 *
 * To enable/disable the sar mode
 * @li 0 disable sar mode
 * @li 1 enable sar mode
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(enable_sar, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_enable_sar,
				stmvl53l1_store_enable_sar);

static ssize_t stmvl53l1_show_offset(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d,%d\n",
			data->inner_offset, data->outer_offset);
}

static ssize_t stmvl53l1_store_offset(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct stmvl53l1_data *data = dev_get_drvdata(dev);
	VL53L1_CalibrationData_t cali_data;
	int rc = 0;
	int n = 0;
	int inner = 0;
	int outer = 0;

	mutex_lock(&data->work_mutex);

	if (data->enable_sensor) {
		vl53l1_errmsg("can't set offset while ranging\n");
		rc = -EBUSY;
		goto finish;
	}

	n = sscanf(buf, "%d,%d", &inner, &outer);
	if(n != 2) {
		vl53l1_errmsg("wrong offset syntax around %s\n", buf);
		rc = -EINVAL;
		goto finish;
	}

	memset(&cali_data, 0, sizeof(cali_data));
	rc = VL53L1_GetCalibrationData(&data->stdev, &cali_data);
	if (rc) {
		vl53l1_errmsg("VL53L1_GetCalibrationData fail\n");
		rc = -EINVAL;
		goto finish;
	}

	/* update offset values */
	vl53l1_info("previous offset is %d,%d\n",
			cali_data.customer.mm_config__inner_offset_mm,
			cali_data.customer.mm_config__outer_offset_mm);
	cali_data.customer.mm_config__inner_offset_mm = inner;
	cali_data.customer.mm_config__outer_offset_mm = outer;

	rc = VL53L1_SetCalibrationData(&data->stdev, &cali_data);
	if (rc) {
		vl53l1_errmsg("VL53L1_SetCalibrationData fail\n");
		rc = -EINVAL;
		goto finish;
	}

	data->inner_offset = (int16_t)inner;
	data->outer_offset = (int16_t)outer;
	vl53l1_info("offset is set to %d,%d\n", inner, outer);
finish:
	mutex_unlock(&data->work_mutex);
	return rc ? rc : count;
}

/**
 * sysfs attribute "enable_sar" [rd/wr]
 *
 * To enable/disable the sar mode
 * @li 0 disable sar mode
 * @li 1 enable sar mode
 *
 * @ingroup sysfs_attrib
 */
static DEVICE_ATTR(offset, 0660/*S_IWUGO | S_IRUGO*/,
				stmvl53l1_show_offset,
				stmvl53l1_store_offset);
static DEVICE_ATTR(last_error, 0440/*S_IRUGO*/,
				stmvl53l1_show_last_error_config,
				NULL);
static struct attribute *stmvl53l1_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_set_delay_ms.attr,
	&dev_attr_timing_budget.attr,
	&dev_attr_roi.attr,
	&dev_attr_mode.attr,
	&dev_attr_do_flush.attr,
	&dev_attr_distance_mode.attr,
	&dev_attr_crosstalk_enable.attr,
	&dev_attr_enable_debug.attr,
	&dev_attr_output_mode.attr,
	&dev_attr_force_device_on_enable.attr,
	&dev_attr_autonomous_config.attr,
	&dev_attr_enable_sar.attr,
	&dev_attr_offset.attr,
	&dev_attr_last_error.attr,
	NULL
};

static const struct attribute_group stmvl53l1_attr_group = {
	.attrs = stmvl53l1_attributes,
};



static int ctrl_reg_access(struct stmvl53l1_data *data, void *p)
{
	struct stmvl53l1_register reg;
	size_t total_byte;
	int rc;

	if (data->is_device_remove)
		return -ENODEV;

	total_byte = offsetof(struct stmvl53l1_register, data.b);
	if (copy_from_user(&reg, p, total_byte)) {
		vl53l1_errmsg("%d, fail\n", __LINE__);
		return -EFAULT;
	}

	if (reg.cnt > STMVL53L1_MAX_CCI_XFER_SZ) {
		vl53l1_errmsg("reg len %d > size limit\n", reg.cnt);
		return -EINVAL;
	}

	total_byte = offsetof(struct stmvl53l1_register, data.bytes[reg.cnt]);
	/* for write get the effective data part of the structure */
	if (!reg.is_read) {
		if (copy_from_user(&reg, p, total_byte)) {
			vl53l1_errmsg(" data cpy fail\n");
			return -EFAULT;
		}
	}

	/* put back to user only needed amount of data */
	if (!reg.is_read) {
		rc = VL53L1_WriteMulti(&data->stdev, (uint16_t)reg.index,
				reg.data.bytes, reg.cnt);
		reg.status = rc;
		/* for write only write back status no data */
		total_byte = offsetof(struct stmvl53l1_register, data.b);
		vl53l1_dbgmsg("wr %x %d bytes statu %d\n",
				reg.index, reg.cnt, rc);
		if (rc)
			rc = store_last_error(data, rc);
	} else {
		rc = VL53L1_ReadMulti(&data->stdev, (uint16_t)reg.index,
				reg.data.bytes, reg.cnt);
		reg.status = rc;
		vl53l1_dbgmsg("rd %x %d bytes status %d\n",
				reg.index, reg.cnt, rc);
		/*  if fail do not copy back data only status */
		if (rc) {
			total_byte = offsetof(struct stmvl53l1_register,
					data.b);
			rc = store_last_error(data, rc);
		}
		/* else the total byte is already the full pay-load with data */
	}

	if (copy_to_user(p, &reg, total_byte)) {
		vl53l1_errmsg("%d, fail\n", __LINE__);
		return -EFAULT;
	}
	return rc;
}


/*
 *
 */
static int ctrl_start(struct stmvl53l1_data *data)
{
	int rc;

	mutex_lock(&data->work_mutex);

	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}

	vl53l1_dbgmsg(" state = %d\n", data->enable_sensor);

	/* turn on tof sensor only if it's not already started */
	if (data->enable_sensor == 0 && !data->is_calibrating) {
		/* to start */
		rc = stmvl53l1_start(data);
	} else{
		rc = -EBUSY;
	}
	vl53l1_dbgmsg(" final state = %d\n", data->enable_sensor);

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

/**
 * no lock version of ctrl_stop (mutex shall be held)
 *
 * @warning exist only for use in device exit to ensure "locked and started"
 * may also beuse in soem erro handling when mutex is already locked
 * @return 0 on success and was running >0 if already off <0 on error
 */
static int _ctrl_stop(struct stmvl53l1_data *data)
{
	int rc;

	vl53l1_dbgmsg("enter state = %d\n", data->enable_sensor);
	/* be sure waiters are woken */
	data->is_data_valid = true;
	/* turn on tof sensor only if it's not enabled by other	client */
	if (data->enable_sensor == 1) {
		/* to stop */
		rc = stmvl53l1_stop(data);
	} else {
		vl53l1_dbgmsg("already off did nothing\n");
		rc = 0;
	}
	stmvl53l1_insert_flush_events_lock(data);
	vl53l1_dbgmsg("	final state = %d\n", data->enable_sensor);

	return rc;
}

/**
 * get work lock and stop sensor
 *
 * see @ref _ctrl_stop
 *
 * @param data device
 * @return 0 on success EBUSY if arleady off
 */
static int ctrl_stop(struct stmvl53l1_data *data)
{
	int rc;

	mutex_lock(&data->work_mutex);

	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}

	if (data->enable_sensor) {
		rc = _ctrl_stop(data);

		if (data->cam_mode == 1) {
			/* only restore sar mode when
			   it quits from camera mode */
			data->cam_mode = 0;

			if (data->sar_mode == 1) {
				vl53l1_info("restore sar mode\n");
				activate_sar_mode(data);
			}
		}
	} else
		rc = -EBUSY;
done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

/**
 * get work lock and suspend sensor without checking sar mode
 *
 * @param data device
 * @return 0 on success EBUSY if arleady off
 */
static int ctrl_suspend(struct stmvl53l1_data *data)
{
	int rc;

	mutex_lock(&data->work_mutex);
	vl53l1_info("enter ctrl_suspend\n");
	if (data->enable_sensor)
		rc = _ctrl_stop(data);
	else
		rc = -EBUSY;
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int get_output_data_index(struct stmvl53l1_data *data)
{
	VL53L1_MultiRangingData_t *meas = &data->meas.multi_range_data;
	int i;
	int index = 0;

	/* multi_range_data already sort by proximity */
	if (data->output_mode == VL53L1_OUTPUTMODE_NEAREST)
		return 0;

	/* else output mode is VL53L1_OUTPUTMODE_STRONGEST */
	for (i = 1; i < meas->NumberOfObjectsFound; i++)
		if (meas->RangeData[i].SignalRateRtnMegaCps >
		    meas->RangeData[index].SignalRateRtnMegaCps)
			index = i;

	return index;
}

static int ctrl_getdata(struct stmvl53l1_data *data, void __user *p)
{
	int rc;
	int index;

	mutex_lock(&data->work_mutex);

	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	index = get_output_data_index(data);
	rc = copy_to_user(p,
		&data->meas.multi_range_data.RangeData[index],
		sizeof(stmvl531_range_data_t));
	if (rc) {
		vl53l1_dbgmsg("copy to user fail %d", rc);
		rc = -EFAULT;
	}

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

static bool is_new_data_for_me(struct stmvl53l1_data *data, pid_t pid,
	struct list_head *head)
{
	return data->is_data_valid && !is_pid_in_list(pid, head);
}

static bool sleep_for_data_condition(struct stmvl53l1_data *data, pid_t pid,
	struct list_head *head)
{
	bool res;

	mutex_lock(&data->work_mutex);
	res = is_new_data_for_me(data, pid, head);
	mutex_unlock(&data->work_mutex);

	return res;
}

static int sleep_for_data(struct stmvl53l1_data *data, pid_t pid,
				struct list_head *head)
{
	int rc;

	mutex_unlock(&data->work_mutex);
	rc = wait_event_killable(data->waiter_for_data,
				sleep_for_data_condition(data, pid, head));
	mutex_lock(&data->work_mutex);

	return data->enable_sensor ? rc : -ENODEV;
}

static int ctrl_getdata_blocking(struct stmvl53l1_data *data, void __user *p)
{
	int rc = 0;
	pid_t pid = current->pid;
	int index;

	mutex_lock(&data->work_mutex);
	/* If device not ranging then exit on error */
	if (data->is_device_remove || !data->enable_sensor) {
		rc = -ENODEV;
		goto done;
	}
	/* sleep if data already read */
	if (!is_new_data_for_me(data, pid, &data->simple_data_reader_list))
		rc = sleep_for_data(data, pid, &data->simple_data_reader_list);
	if (rc)
		goto done;

	/* unless we got interrupted we return data to user and note read */
	index = get_output_data_index(data);
	rc = copy_to_user(p, &data->meas.multi_range_data.RangeData[index],
		sizeof(stmvl531_range_data_t));
	if (rc)
		goto done;
	rc = add_reader(pid, &data->simple_data_reader_list);

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

/**
 * Get multi zone data
 * @param data
 * @param p [out] user ptr to @ref VL53L1_MultiRangingData_t structure\n
 *  is always set but on EFAULT error
 *
 * @return
 * @li 0 on success
 * @li ENODEV if not ranging
 * @li ENOEXEC not in multi zone mode
 * @li EFAULT  copy to user error
 */
static int ctrl_mz_data(struct stmvl53l1_data *data, void __user *p)
{
	int rc;

	mutex_lock(&data->work_mutex);
	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	rc = copy_to_user(p, &data->meas.multi_range_data,
		sizeof(VL53L1_MultiRangingData_t));
	if (rc) {
		vl53l1_dbgmsg("copy to user fail %d", rc);
		rc = -EFAULT;
	}
	if (!data->enable_sensor) {
		rc = -ENODEV;
	} else
	if (!is_mz_mode(data))
		rc = -ENOEXEC;

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int ctrl_mz_data_blocking(struct stmvl53l1_data *data, void __user *p)
{
	int rc = 0;
	VL53L1_MultiRangingData_t *meas =  &data->meas.multi_range_data;
	pid_t pid = current->pid;

	mutex_lock(&data->work_mutex);
	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	/* mode 2 or 3 */
	if (!is_mz_mode(data)) {
		rc = -ENOEXEC;
		goto done;
	}
	/* If device not ranging then exit on error */
	if (!data->enable_sensor) {
		rc = -ENODEV;
		goto done;
	}
	/* sleep if data already read */
	if (!is_new_data_for_me(data, pid, &data->mz_data_reader_list))
		rc = sleep_for_data(data, pid, &data->mz_data_reader_list);
	if (rc)
		goto done;

	/* unless we got interrupted we return data to user and note read */
	rc = copy_to_user(p, meas, sizeof(VL53L1_MultiRangingData_t));
	if (rc)
		goto done;
	rc = add_reader(pid, &data->mz_data_reader_list);

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int ctrl_param_timing_budget(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		/* GET */
		param->value = data->timing_budget;
		param->status = 0;
		rc = 0;
		vl53l1_dbgmsg("TIMINGBUDGET_PAR get status %d", rc);
	} else {
		rc = stmvl53l1_set_timing_budget(data, param->value);
		vl53l1_dbgmsg("TIMINGBUDGET_PAR set status %d is %d", rc,
				data->timing_budget);
	}

	return rc;
}
/**
 * handle ioctl set param poll delay
 *
 * @param data
 * @param param in/out
 * @return
 */
static int ctrl_param_poll_delay(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->poll_delay_ms;
		param->status = 0;
		vl53l1_dbgmsg("poll delay get %d ms", param->value);
		rc = 0;
	} else {
		rc = stmvl53l1_set_poll_delay_ms(data, param->value);
		vl53l1_dbgmsg("poll delay set %d ms", data->poll_delay_ms);
	}

	return rc;
}


static int ctrl_param_mode(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->preset_mode;
		param->status = 0;
		vl53l1_dbgmsg("get mode %d", param->value);
		rc = 0;
	} else {
		rc = stmvl53l1_set_mode(data, param->value);
		vl53l1_dbgmsg("rc %d req %d now %d", rc,
				param->value, data->preset_mode);
	}
	return rc;
}

static int ctrl_param_distance_mode(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->distance_mode;
		param->status = 0;
		vl53l1_dbgmsg("get distance mode %d", param->value);
		rc = 0;
	} else {
		rc = stmvl53l1_set_distance_mode(data, param->value);
		vl53l1_dbgmsg("rc %d req %d now %d", rc,
				param->value, data->distance_mode);
	}

	return rc;
}

static int ctrl_param_crosstalk_enable(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->crosstalk_enable;
		param->status = 0;
		vl53l1_dbgmsg("get crosstalk enable mode %d", param->value);
		rc = 0;
	} else {
		rc = stmvl53l1_set_crosstalk_enable(data, param->value);
		vl53l1_dbgmsg("rc %d req %d now %d", rc,
				param->value, data->crosstalk_enable);
	}

	return rc;
}

static int ctrl_param_output_mode(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->output_mode;
		param->status = 0;
		vl53l1_dbgmsg("get output mode %d", param->value);
		rc = 0;
	} else {
		rc = stmvl53l1_set_output_mode(data, param->value);
		vl53l1_dbgmsg("rc %d req %d now %d", rc,
				param->value, data->output_mode);
	}

	return rc;
}

static int ctrl_force_device_on_en(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->force_device_on_en;
		param->status = 0;
		vl53l1_dbgmsg("get force device on %d", param->value);
		rc = 0;
	} else {
		rc = stmvl53l1_set_force_device_on_en(data, param->value);
		vl53l1_dbgmsg("rc %d req %d now %d", rc,
				param->value, data->force_device_on_en);
	}

	return rc;
}

static int ctrl_param_last_error(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->last_error;
		param->status = 0;
		vl53l1_dbgmsg("get last error %d", param->value);
		rc = 0;
	} else {
		rc = -EINVAL;
	}

	return rc;
}

static int ctrl_param_camera_mode(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	if (param->is_read) {
		param->value = data->cam_mode;
		param->status = 0;
		vl53l1_dbgmsg("get camera mode %d", param->value);
	} else {
		data->cam_mode = param->value;
		vl53l1_dbgmsg("set camera mode %d", param->value);
	}

	return 0;
}

static int ctrl_param_hw_rev(struct stmvl53l1_data *data,
		struct stmvl53l1_parameter *param)
{
	int rc;

	if (param->is_read) {
		param->value = data->hw_rev;
		param->status = 0;
		vl53l1_dbgmsg("get hw rev %d", param->value);
		rc = 0;
	} else {
		rc = -EINVAL;
	}

	return rc;
}

/**
 * handle ioctl set param mode
 *
 * @param data
 * @param p
 * @return 0 on success
 */
static int ctrl_params(struct stmvl53l1_data *data, void __user *p)
{
	int rc, rc2;
	struct stmvl53l1_parameter param;

	mutex_lock(&data->work_mutex);

	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	rc = copy_from_user(&param, p, sizeof(param));
	if (rc) {
		rc = -EFAULT;
		goto done; /* no need for status in user struct */
	}
	switch (param.name) {
	case VL53L1_POLLDELAY_PAR:
		rc = ctrl_param_poll_delay(data, &param);
		break;
	case VL53L1_TIMINGBUDGET_PAR:
		rc = ctrl_param_timing_budget(data, &param);
		break;
	case VL53L1_DEVICEMODE_PAR:
		rc = ctrl_param_mode(data, &param);
	break;
	case VL53L1_DISTANCEMODE_PAR:
		rc = ctrl_param_distance_mode(data, &param);
	break;
	case VL53L1_XTALKENABLE_PAR:
		rc = ctrl_param_crosstalk_enable(data, &param);
	break;
	case VL53L1_OUTPUTMODE_PAR:
		rc = ctrl_param_output_mode(data, &param);
	break;
	case VL53L1_FORCEDEVICEONEN_PAR:
		rc = ctrl_force_device_on_en(data, &param);
	break;
	case VL53L1_LASTERROR_PAR:
		rc = ctrl_param_last_error(data, &param);
	break;
	case VL53L1_CAMERAMODE_PAR:
		rc = ctrl_param_camera_mode(data, &param);
		break;
	case VL53L1_HWREV_PAR:
		rc = ctrl_param_hw_rev(data, &param);
		break;
	default:
		vl53l1_errmsg("unknown or unsupported %d\n", param.name);
		rc = -EINVAL;
	}
	/* copy back (status at least ) to user */
	if (param.is_read  && rc == 0) {
		rc2 = copy_to_user(p, &param, sizeof(param));
		if (rc2) {
			rc = -EFAULT; /* kill prev status if that fail */
			vl53l1_errmsg("copy to user fail %d\n", rc);
		}
	}
done:
	mutex_unlock(&data->work_mutex);
	return rc;
}

/**
 * implement set/get roi ioctl
 * @param data device
 * @param p user space ioctl arg ptr
 * @return 0 on success <0 errno code
 *	@li -EINVAL invalid number of roi
 *	@li -EBUSY when trying to set roi while ranging
 *	@li -EFAULT if cpy to/fm user fail for requested number of roi
 */
static int ctrl_roi(struct stmvl53l1_data *data, void __user *p)
{
	int rc;
	int roi_cnt;
	struct stmvl53l1_roi_full_t rois;

	mutex_lock(&data->work_mutex);
	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	/* copy fixed part of args at first */
	rc = copy_from_user(&rois, p,
			offsetof(struct stmvl53l1_roi_full_t,
					roi_cfg.UserRois[0]));
	if (rc) {
		rc = -EFAULT;
		goto done;
	}
	/* check no of roi limit is ok */
	roi_cnt = rois.roi_cfg.NumberOfRoi;
	if (roi_cnt > VL53L1_MAX_USER_ZONES) {
		vl53l1_errmsg("invalid roi spec cnt=%d > %d",
				rois.roi_cfg.NumberOfRoi,
				VL53L1_MAX_USER_ZONES);
		rc = -EINVAL;
		goto done;
	}

	if (rois.is_read) {
		int cpy_size;

		roi_cnt = MIN(rois.roi_cfg.NumberOfRoi,
				data->roi_cfg.NumberOfRoi);
		cpy_size = offsetof(VL53L1_RoiConfig_t, UserRois[roi_cnt]);
		/* copy from local to user only effective part requested */
		rc = copy_to_user(&((struct stmvl53l1_roi_full_t *)p)->roi_cfg,
			&data->roi_cfg, cpy_size);
		vl53l1_dbgmsg("return %d of %d\n", roi_cnt,
				data->roi_cfg.NumberOfRoi);
	} else {
		/* SET check cnt roi is ok */
		if (data->enable_sensor) {
			rc = -EBUSY;
			vl53l1_errmsg("can't set roi while ranging\n");
			goto done;
		}
		/* get full data that  required from user */
		rc = copy_from_user(&rois, p,
					offsetof(struct stmvl53l1_roi_full_t,
						roi_cfg.UserRois[roi_cnt]));
		if (rc) {
			vl53l1_errmsg("get %d roi fm user fail", roi_cnt);
			rc = -EFAULT;
			goto done;
		}
		dump_roi(data->roi_cfg.UserRois, data->roi_cfg.NumberOfRoi);
		/* we may ask ll driver to check but check is mode dependent
		 * and so we could get erroneous error back
		 */
		memcpy(&data->roi_cfg, &rois.roi_cfg, sizeof(data->roi_cfg));
	}
done:
	mutex_unlock(&data->work_mutex);
	return rc;
}

static int ctrl_autonomous_config(struct stmvl53l1_data *data, void __user *p)
{
	int rc = 0;
	struct stmvl53l1_autonomous_config_t full;

	mutex_lock(&data->work_mutex);
	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	/* first copy all data */
	rc = copy_from_user(&full, p, sizeof(full));
	if (rc) {
		rc = -EFAULT;
		goto done;
	}

	if (full.is_read) {
		full.pollingTimeInMs = data->auto_pollingTimeInMs;
		full.config = data->auto_config;
		rc = copy_to_user(p, &full, sizeof(full));
		if (rc)
			rc = -EFAULT;
	} else {
		if (data->enable_sensor) {
			rc = -EBUSY;
			vl53l1_errmsg("can't change config while ranging\n");
			goto done;
		}
		data->auto_pollingTimeInMs = full.pollingTimeInMs;
		data->auto_config = full.config;
	}

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int ctrl_autonomous_config_cam(struct stmvl53l1_data *data,
		void __user *p)
{
	int rc = 0;
	struct stmvl53l1_autonomous_config_t full;

	mutex_lock(&data->work_mutex);
	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	/* first copy all data */
	rc = copy_from_user(&full, p, sizeof(full));
	if (rc) {
		rc = -EFAULT;
		goto done;
	}

	if (full.is_read) {
		full.pollingTimeInMs = data->auto_pollingTimeInMs_cam;
		full.config = data->auto_config_cam;
		rc = copy_to_user(p, &full, sizeof(full));
		if (rc)
			rc = -EFAULT;
	} else {
		if (data->enable_sensor) {
			rc = -EBUSY;
			vl53l1_errmsg("can't change config while ranging\n");
			goto done;
		}
		data->auto_pollingTimeInMs_cam = full.pollingTimeInMs;
		data->auto_config_cam = full.config;
	}

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int ctrl_calibration_data(struct stmvl53l1_data *data, void __user *p)
{
	int rc;
	struct stmvl53l1_ioctl_calibration_data_t calib;
	int data_offset = offsetof(struct stmvl53l1_ioctl_calibration_data_t,
					data);

	mutex_lock(&data->work_mutex);

	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	rc = copy_from_user(&calib, p, data_offset);
	if (rc) {
		vl53l1_errmsg("fail to detect read or write %d", rc);
		rc = -EFAULT;
		goto done;
	}

	if (calib.is_read) {
		memset(&calib.data, 0, sizeof(calib.data));
		rc = VL53L1_GetCalibrationData(&data->stdev, &calib.data);
		if (rc) {
			vl53l1_errmsg("VL53L1_GetCalibrationData fail %d", rc);
			rc = store_last_error(data, rc);
			goto done;
		}
		rc = copy_to_user(p + data_offset, &calib.data,
			sizeof(calib.data));
	} else {
		if (data->enable_sensor) {
			rc = -EBUSY;
			vl53l1_errmsg("can't set calib data while ranging\n");
			goto done;
		}
		rc = copy_from_user(&calib.data, p + data_offset,
			sizeof(calib.data));
		if (rc) {
			vl53l1_errmsg("fail to copy calib data");
			rc = -EFAULT;
			goto done;
		}
		rc = VL53L1_SetCalibrationData(&data->stdev, &calib.data);
		if (rc) {
			vl53l1_errmsg("VL53L1_SetCalibrationData fail %d", rc);
			rc = store_last_error(data, rc);
		}
	}

done:
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int ctrl_perform_calibration_ref_spad_lock(struct stmvl53l1_data *data,
	struct stmvl53l1_ioctl_perform_calibration_t *calib)
{
	int rc = VL53L1_PerformRefSpadManagement(&data->stdev);

	if (rc) {
		vl53l1_errmsg("VL53L1_PerformRefSpadManagement fail => %d", rc);
		rc = store_last_error(data, rc);
	}

	return rc;
}

static int ctrl_perform_calibration_crosstalk_lock(struct stmvl53l1_data *data,
	struct stmvl53l1_ioctl_perform_calibration_t *calib)
{
	int rc = VL53L1_PerformXTalkCalibration(&data->stdev, calib->param1);

	if (rc) {
		vl53l1_errmsg("VL53L1_PerformXTalkCalibration fail => %d", rc);
		rc = store_last_error(data, rc);
	}

	return rc;
}

static int ctrl_perform_calibration_offset_lock(struct stmvl53l1_data *data,
	struct stmvl53l1_ioctl_perform_calibration_t *calib)
{
	int rc;

	vl53l1_info("VL53L1_SetOffsetCalibrationMode para: %d %d %d\n", calib->param1,
			calib->param2, calib->param3);
	rc = VL53L1_SetOffsetCalibrationMode(&data->stdev, calib->param1);
	if (rc) {
		vl53l1_errmsg("VL53L1_SetOffsetCalibrationMode fail => %d", rc);
		rc = store_last_error(data, rc);
		goto done;
	}
	rc = VL53L1_PerformOffsetCalibration(&data->stdev, calib->param2,
		calib->param3);
	if (rc) {
		vl53l1_errmsg("VL53L1_PerformOffsetCalibration fail => %d", rc);
		rc = store_last_error(data, rc);
	}

done:
	return rc;
}

static int ctrl_perform_calibration(struct stmvl53l1_data *data, void __user *p)
{
	int rc;
	struct stmvl53l1_ioctl_perform_calibration_t calib;

	mutex_lock(&data->work_mutex);

	if (data->is_device_remove) {
		rc = -ENODEV;
		goto done;
	}
	data->is_calibrating = true;
	rc = copy_from_user(&calib, p, sizeof(calib));
	if (rc) {
		rc = -EFAULT;
		goto done;
	}
	if (data->enable_sensor) {
		rc = -EBUSY;
		vl53l1_errmsg("can't perform calibration while ranging\n");
		goto done;
	}

	rc = reset_release(data);
	if (rc)
		goto done;

	rc = VL53L1_StaticInit(&data->stdev);
	if (rc) {
		vl53l1_errmsg("VL53L1_StaticInit fail => %d", rc);
		rc = store_last_error(data, rc);
		goto done;
	}

	switch (calib.calibration_type) {
	case VL53L1_CALIBRATION_REF_SPAD:
		rc = ctrl_perform_calibration_ref_spad_lock(data,
			&calib);
		break;
	case VL53L1_CALIBRATION_CROSSTALK:
		rc = ctrl_perform_calibration_ref_spad_lock(data,
			&calib);
		if (rc) {
			vl53l1_errmsg("RefSpad Calibration error = %d\n", rc);
			goto done;
		}
		rc = ctrl_perform_calibration_crosstalk_lock(data,
			&calib);
		break;
	case VL53L1_CALIBRATION_OFFSET:
		vl53l1_info("perform ref spad calibration first...\n");
		rc = ctrl_perform_calibration_ref_spad_lock(data,
			&calib);
		if (rc) {
			vl53l1_errmsg("RefSpad Calibration error = %d\n", rc);
			goto done;
		}
		rc = ctrl_perform_calibration_offset_lock(data,
			&calib);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	reset_hold(data);

done:
	data->is_calibrating = false;
	mutex_unlock(&data->work_mutex);

	return rc;
}

static int stmvl53l1_ioctl_handler(
		struct stmvl53l1_data *data,
		unsigned int cmd, unsigned long arg,
		void __user *p)
{
	int rc = 0;

	if (!data)
		return -EINVAL;

	switch (cmd) {

	case VL53L1_IOCTL_START:
		vl53l1_dbgmsg("VL53L1_IOCTL_START\n");
		rc = ctrl_start(data);
		break;

	case VL53L1_IOCTL_STOP:
		vl53l1_dbgmsg("VL53L1_IOCTL_STOP\n");
		rc = ctrl_stop(data);
		break;

	case VL53L1_IOCTL_SUSPEND:
		vl53l1_dbgmsg("VL53L1_IOCTL_SUSPEND\n");
		rc = ctrl_suspend(data);
		break;

	case VL53L1_IOCTL_GETDATAS:
		/* vl53l1_dbgmsg("VL53L1_IOCTL_GETDATAS\n"); */
		rc = ctrl_getdata(data, p);
		break;

	case VL53L1_IOCTL_GETDATAS_BLOCKING:
		/* vl53l1_dbgmsg("VL53L1_IOCTL_GETDATAS_BLOCKING\n"); */
		rc = ctrl_getdata_blocking(data, p);
		break;

	/* Register tool */
	case VL53L1_IOCTL_REGISTER:
		vl53l1_dbgmsg("VL53L1_IOCTL_REGISTER\n");
		rc = ctrl_reg_access(data, p);
		break;

	case VL53L1_IOCTL_PARAMETER:
		vl53l1_dbgmsg("VL53L1_IOCTL_PARAMETER\n");
		rc = ctrl_params(data, p);
		break;

	case VL53L1_IOCTL_ROI:
		vl53l1_dbgmsg("VL53L1_IOCTL_ROI\n");
		rc = ctrl_roi(data, p);
		break;
	case VL53L1_IOCTL_MZ_DATA:
		/* vl53l1_dbgmsg("VL53L1_IOCTL_MZ_DATA\n"); */
		rc = ctrl_mz_data(data, p);
		break;
	case VL53L1_IOCTL_MZ_DATA_BLOCKING:
		/* vl53l1_dbgmsg("VL53L1_IOCTL_MZ_DATA_BLOCKING\n"); */
		rc = ctrl_mz_data_blocking(data, p);
		break;
	case VL53L1_IOCTL_CALIBRATION_DATA:
		vl53l1_dbgmsg("VL53L1_IOCTL_CALIBRATION_DATA\n");
		rc = ctrl_calibration_data(data, p);
		break;
	case VL53L1_IOCTL_PERFORM_CALIBRATION:
		vl53l1_dbgmsg("VL53L1_IOCTL_PERFORM_CALIBRATION\n");
		rc = ctrl_perform_calibration(data, p);
		break;
	case VL53L1_IOCTL_AUTONOMOUS_CONFIG:
		vl53l1_dbgmsg("VL53L1_IOCTL_AUTONOMOUS_CONFIG\n");
		if (data->cam_mode == 0)
			rc = ctrl_autonomous_config(data, p);
		else
			rc = ctrl_autonomous_config_cam(data, p);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}



static int stmvl53l1_open(struct inode *inode, struct file *file)
{
	struct stmvl53l1_data *data = container_of(file->private_data,
		struct stmvl53l1_data, miscdev);

	vl53l1_dbgmsg("Start\n");
	stmvl53l1_module_func_tbl.get(data->client_object);
	vl53l1_dbgmsg("End\n");

	return 0;
}

static int stmvl53l1_release(struct inode *inode, struct file *file)
{
	struct stmvl53l1_data *data = container_of(file->private_data,
		struct stmvl53l1_data, miscdev);

	vl53l1_dbgmsg("Start\n");
	stmvl53l1_module_func_tbl.put(data->client_object);
	vl53l1_dbgmsg("End\n");

	return 0;
}


/** max number or error per measure too abort */
#define stvm531_get_max_meas_err(...) 3
/** max number or error per stream too abort */
#define stvm531_get_max_stream_err(...) 6

/**
 * handle data retrieval and dispatch
 *
 * work lock must be held
 *
 * called  form work or interrupt thread it must be a blocable context !
 * @param data the device
 */
static void stmvl53l1_on_newdata_event(struct stmvl53l1_data *data)
{
	int rc;
	VL53L1_MultiRangingData_t *pmrange;
	VL53L1_MultiRangingData_t *tmprange;
	long ts_msec;
	int i;

	do_gettimeofday(&data->meas.comp_tv);
	ts_msec = stmvl53l1_tv_dif(&data->start_tv, &data->meas.comp_tv)/1000;

	pmrange = &data->meas.multi_range_data;
	tmprange = &data->meas.tmp_range_data;
	data->meas.intr++;

	/* use get data method based on active mode */
	switch (data->preset_mode) {
	case VL53L1_PRESETMODE_LITE_RANGING:
	case VL53L1_PRESETMODE_AUTONOMOUS:
		rc = VL53L1_GetRangingMeasurementData(&data->stdev,
			&pmrange->RangeData[0]);
		/* set top level status so we got correct events in input
		 * subsystem.
		 */
		pmrange->RoiNumber = 0;
		pmrange->RoiStatus = VL53L1_ROISTATUS_VALID_LAST;
		pmrange->NumberOfObjectsFound = 1;
	break;
	case VL53L1_PRESETMODE_RANGING:
	case VL53L1_PRESETMODE_MULTIZONES_SCANNING:
		/* IMPORTANT : during VL53L1_GetMultiRangingData() call
		* work_mutex is release during ipp. This is why we use
		* tmp_range_data which is not access somewhere else. When we are
		* back we then copy tmp_range_data in multi_range_data.
		 */

		rc = VL53L1_GetMultiRangingData(&data->stdev,
			&data->meas.tmp_range_data);

		/* be sure we got VL53L1_RANGESTATUS_NONE for object 0 if we got
		 * invalid roi or no object. So if client read data using
		 * VL53L1_IOCTL_GETDATAS we got correct status.
		 */
		if (tmprange->RoiStatus == VL53L1_ROISTATUS_NOT_VALID ||
			tmprange->NumberOfObjectsFound == 0)
			tmprange->RangeData[0].RangeStatus =
						VL53L1_RANGESTATUS_NONE;

		memcpy(pmrange, tmprange, sizeof(VL53L1_MultiRangingData_t));
	break;
	default:
		/* that must be some bug or data corruption stop now */
		rc = -1;
		vl53l1_errmsg("unsorted mode %d=>stop\n", data->preset_mode);
		_ctrl_stop(data);
	}
	/* check if not stopped yet
	 * as we may have been unlocked we must re-check
	 */
	if (data->enable_sensor == 0) {
		vl53l1_dbgmsg("at meas #%d we got stoped\n", data->meas.cnt);
		return;
	}
	if (rc) {
		vl53l1_errmsg("VL53L1_GetRangingMeasurementData @%d %d",
				__LINE__, rc);
		data->meas.err_cnt++;
		data->meas.err_tot++;
		if (data->meas.err_cnt > stvm531_get_max_meas_err(data) ||
			data->meas.err_tot > stvm531_get_max_stream_err(data)) {
			vl53l1_errmsg("on #%d %d err %d tot stop",
				data->meas.cnt, data->meas.err_cnt,
				data->meas.err_tot);
			_ctrl_stop(data);
		}
		return;
	}

	/* FIXME: remove when implemented by ll or bare driver */
	pmrange->RangeData[0].TimeStamp = ts_msec;
	for (i = 1; i < pmrange->NumberOfObjectsFound; i++)
		pmrange->RangeData[i].TimeStamp = ts_msec;

	data->meas.cnt++;
	vl53l1_dbgmsg("#%3d %2d poll ts %5d status=%d obj cnt=%d\n",
		data->meas.cnt,
		data->meas.poll_cnt,
		pmrange->RangeData[0].TimeStamp,
		pmrange->RangeData[0].RangeStatus,
		pmrange->NumberOfObjectsFound);
#if 0
	vl53l1_dbgmsg(
"meas m#%04d i#%04d  p#%04d in %d ms data range status %d range %d\n",
			(int)data->meas.cnt,
			(int)data->meas.intr,
			(int)data->meas.poll_cnt,
			(int)stmvl53l1_tv_dif(&data->meas.start_tv,
					&data->meas.comp_tv)/1000,
			(int)data->meas.range_data.RangeStatus,
			(int)data->meas.range_data.RangeMilliMeter);
#endif
	/* ready that is not always on each new data event */

	/* mark data as valid from now */
	data->is_data_valid = true;

	/* wake up sleeping client */
	wake_up_data_waiters(data);

	/* push data to input susbys and only and make avl for ioctl*/
	stmvl53l1_input_push_data(data);
	stmvl53l1_insert_flush_events_lock(data);

	/* roll time now data got used */
	data->meas.start_tv = data->meas.comp_tv;
	data->meas.poll_cnt = 0;
	data->meas.err_cnt = 0;
}


/**
 * * handle interrupt/pusdo irq by polling handling
 *
 * work lock must be held
 *
 * @param data driver
 * @return 0 on success
 */
static int stmvl53l1_intr_process(struct stmvl53l1_data *data)
{
	uint8_t data_rdy;
	int rc = 0;
	struct timeval tv_now;

	if (!data->enable_sensor)
		goto done;

	data->meas.poll_cnt++;
	rc = VL53L1_GetMeasurementDataReady(&data->stdev, &data_rdy);
	if (rc) {
		vl53l1_errmsg("GetMeasurementDataReady @%d %d, fail\n",
				__LINE__, rc);
		/* too many successive fail => stop but do not try to do any new
		 * i/o
		 */
		goto stop_io;
	}

	if (!data_rdy) {
		/* FIXME this part to completely skip
		 * if using interrupt and sure we have
		 * no false interrupt to handle or no to do any timing check
		 */
		long poll_ms;

		do_gettimeofday(&tv_now);
		poll_ms = stmvl53l1_tv_dif(&data->meas.start_tv, &tv_now)/1000;
		if (poll_ms > data->timing_budget*4) {
			vl53l1_errmsg("we're polling %ld  too long\n", poll_ms);
			/*  fixme stop or just warn ? */
			goto stop_io;
		}
		/*  keep trying it could be intr with no processing */
		work_dbg("intr with no data rdy");
		goto done;
	}
	/* we have data to handle */
	/* first irq after reset has no data so we skip it */
	if (data->is_first_irq)
		data->is_first_irq = false;
	else
		stmvl53l1_on_newdata_event(data);
	/* enable_sensor could change on event handling check again */
	if (data->enable_sensor) {
		/* clear interrupt and continue ranging */
		work_dbg("intr clr");
		/* In autonomous mode, bare driver will trigger stop/start
		 * sequence. In that case it wall call platform delay functions.
		 * So allow delay in VL53L1_ClearInterruptAndStartMeasurement()
		 * call.
		 */
		data->is_delay_allowed = data->allow_hidden_start_stop;
		rc = VL53L1_ClearInterruptAndStartMeasurement(&data->stdev);
		data->is_delay_allowed = 0;
		if (rc) {
			/* go to stop but stop any new i/o for dbg */
			vl53l1_errmsg("Cltr intr restart fail %d\n", rc);
			goto stop_io;
		}
	}
done:
	return rc;
stop_io:
	/* too many successive fail take action => stop but do not try to do
	 * any new i/o
	 */
	vl53l1_errmsg("GetDatardy fail stop\n");
	data->enable_sensor = 0;
	return rc;

}

static void stmvl53l1_work_handler(struct work_struct *work)
{
	struct stmvl53l1_data *data;

	data = container_of(work, struct stmvl53l1_data, dwork.work);
	work_dbg("enter");
	mutex_lock(&data->work_mutex);
	stmvl53l1_intr_process(data);
	if (data->poll_mode && data->enable_sensor) {
		/* re-sched ourself */
		schedule_delayed_work(&data->dwork,
			msecs_to_jiffies(data->poll_delay_ms));
	}
	mutex_unlock(&data->work_mutex);
}

static void stmvl53l1_input_push_data_object(struct stmvl53l1_data *data,
	int roi, int obj, stmvl531_range_data_t *meas, int obj_number,
	VL53L1_RoiStatus RoiStatus)
{
	struct input_dev *input = data->input_dev_ps;
	FixPoint1616_t LimitCheckCurrent;
	VL53L1_Error st = VL53L1_ERROR_NONE;

	/* set the distance value to max if range
	 * status is invalid in autonomous mode */
	if (data->preset_mode == VL53L1_PRESETMODE_AUTONOMOUS &&
		meas->RangeStatus != VL53L1_RANGESTATUS_RANGE_VALID)
		meas->RangeMilliMeter = STMVL53L1_MAX_DISTANCE;

	input_report_abs(input, ABS_DISTANCE, (meas->RangeMilliMeter + 5) / 10);
	input_report_abs(input, ABS_HAT0X, meas->TimeStamp / 1000);
	input_report_abs(input, ABS_HAT0Y, (meas->TimeStamp % 1000) * 1000);
	input_report_abs(input, ABS_HAT1X, meas->RangeMilliMeter);
	input_report_abs(input, ABS_HAT1Y, meas->RangeStatus);
	input_report_abs(input, ABS_HAT2X, meas->SignalRateRtnMegaCps);
	input_report_abs(input, ABS_HAT2Y, meas->AmbientRateRtnMegaCps);
	input_report_abs(input, ABS_HAT3X, meas->SigmaMilliMeter);
	input_report_abs(input, ABS_HAT3Y, meas->DmaxMilliMeter);
	st = VL53L1_GetLimitCheckCurrent(&data->stdev,
		VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE, &LimitCheckCurrent);
	if (st == VL53L1_ERROR_NONE)
		input_report_abs(input, ABS_WHEEL, LimitCheckCurrent);
	input_report_abs(input, ABS_MISC, meas->EffectiveSpadRtnCount);
	input_report_abs(input, ABS_BRAKE, (RoiStatus << 24) | (roi << 16) |
		(obj_number << 8) | obj);
	input_report_abs(input, ABS_TILT_X, (meas->RangeMaxMilliMeter << 16) |
		meas->RangeMinMilliMeter);
	input_report_abs(input, ABS_TOOL_WIDTH, meas->RangeQualityLevel);

	input_sync(input);
}

static void stmvl53l1_input_push_data(struct stmvl53l1_data *data)
{
	VL53L1_MultiRangingData_t *meas = &data->meas.multi_range_data;
	int obj_number = meas->NumberOfObjectsFound;
	int i;

	/* be sure threre is at least one object */
	obj_number = obj_number ? obj_number : 1;
	for (i = 0; i < obj_number; i++)
		stmvl53l1_input_push_data_object(data, meas->RoiNumber, i,
			&meas->RangeData[i], obj_number, meas->RoiStatus);
}

static int stmvl53l1_input_setup(struct stmvl53l1_data *data)
{
	int rc;
	struct input_dev *idev;
	/* Register to Input Device */
	idev = input_allocate_device();
	if (idev == NULL) {
		rc = -ENOMEM;
		vl53l1_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_err;
	}
	/*  setup all event */
	set_bit(EV_ABS, idev->evbit);
	/* range in cm*/
	input_set_abs_params(idev, ABS_DISTANCE, 0, 0xff, 0, 0);
	/* tv_sec */
	input_set_abs_params(idev, ABS_HAT0X, 0, 0xffffffff, 0, 0);
	/* tv_usec */
	input_set_abs_params(idev, ABS_HAT0Y, 0, 0xffffffff, 0, 0);
	/* range in_mm */
	input_set_abs_params(idev, ABS_HAT1X, 0, 765, 0, 0);
	/* error code change maximum to 0xff for more flexibility */
	input_set_abs_params(idev, ABS_HAT1Y, 0, 0xff, 0, 0);
	/* rtnRate */
	input_set_abs_params(idev, ABS_HAT2X, 0, 0xffffffff, 0, 0);
	/* rtn_amb_rate */
	input_set_abs_params(idev, ABS_HAT2Y, 0, 0xffffffff, 0, 0);
	/* SigmaMilliMeter */
	input_set_abs_params(idev, ABS_HAT3X, 0, 0xffffffff, 0, 0);
	/* dmax */
	input_set_abs_params(idev, ABS_HAT3Y, 0, 0xffffffff, 0, 0);
	/* LimitCheckCurrent*/
	input_set_abs_params(idev, ABS_WHEEL, 0, 0xffffffff, 0, 0);
	/* EffectiveSpadRtnCount */
	input_set_abs_params(idev, ABS_MISC, 0, 0xffffffff, 0, 0);
	/* roi number in 16 msb bits with object number in 16 lsb bits */
	input_set_abs_params(idev, ABS_BRAKE, 0, 0xffffffff, 0, 0);
	/* RangeMaxMilliMeter in 16 msb bits with RangeMinMilliMeter in lsb */
	input_set_abs_params(idev, ABS_TILT_X, 0, 0xffffffff, 0, 0);
	/* ConfidenceLevel */
	input_set_abs_params(idev, ABS_TOOL_WIDTH, 0, 0xffffffff, 0, 0);

	/* flushCount */
	input_set_abs_params(idev, ABS_GAS, 0, 0xffffffff, 0, 0);

	/* those are still free to use
	 * input_set_abs_params(idev, ABS_RUDDER, 0, 0xffffffff, 0, 0);
	 * input_set_abs_params(idev, ABS_TILT_Y, 0, 0xffffffff, 0, 0);
	*/

	idev->name = VL53L1_INPUT_DEVICE_NAME;
	rc = input_register_device(idev);
	if (rc) {
		rc = -ENOMEM;
		vl53l1_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_free_dev_ps;
	}
	/* setup drv data */
	input_set_drvdata(idev, data);
	data->input_dev_ps = idev;
	return 0;


exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_err:
	return rc;
}

/**
 * handler to be called by interface module on interrupt
 *
 * managed poll/irq filtering in case poll/irq can be soft forced
 * and the module side still fire interrupt
 *
 * @param data
 * @return 0 if all ok else for error
 */
int stmvl53l1_intr_handler(struct stmvl53l1_data *data)
{
	int rc;

	mutex_lock(&data->work_mutex);

	/* handle it only if if we are not stopped */
	if (data->enable_sensor) {
		rc = stmvl53l1_intr_process(data);
	} else {
		/* it's likely race/last unhandled interrupt after
		 * stop.
		 * Such dummy irq also occurred during offset and crosstalk
		 * calibration procedures.
		 */
		vl53l1_dbgmsg("got intr but not on (dummy or calibration)\n");
		rc = 0;
	}

	mutex_unlock(&data->work_mutex);
	return rc;
}


/**
 * One time device  setup
 *
 * call by bus (i2c/cci) level probe to finalize non bus related device setup
 *
 * @param	data The device data
 * @return	0 on success
 */
int stmvl53l1_setup(struct stmvl53l1_data *data)
{
	int rc = 0;
	VL53L1_CalibrationData_t cali_data;
	VL53L1_DeviceInfo_t dev_info;

	vl53l1_dbgmsg("Enter\n");

	data->hw_rev = 0;

	/* acquire an id */
	data->id = allocate_dev_id();
	if (data->id < 0) {
		vl53l1_errmsg("too many device already created");
		return -1;
	}
	vl53l1_dbgmsg("Dev id %d is @%p\n", data->id, data);
	stmvl53l1_dev_table[data->id] = data;

	/* init mutex */
	/* mutex_init(&data->update_lock); */
	mutex_init(&data->work_mutex);

	/* init work handler */
	INIT_DELAYED_WORK(&data->dwork, stmvl53l1_work_handler);

	/* init ipp side */
	stmvl53l1_ipp_setup(data);

	data->force_device_on_en = force_device_on_en_default;
	data->reset_state = 1;
	data->is_calibrating = false;
	data->last_error = VL53L1_ERROR_NONE;
	data->is_device_remove = false;

	/* vdd will be controlled in reset_release() */
	rc = reset_release(data);
	if (rc)
		goto exit_ipp_cleanup;

	rc = stmvl53l1_input_setup(data);
	if (rc)
		goto exit_ipp_cleanup;

	/* init blocking ioctl stuff */
	INIT_LIST_HEAD(&data->simple_data_reader_list);
	INIT_LIST_HEAD(&data->mz_data_reader_list);
	init_waitqueue_head(&data->waiter_for_data);
	data->is_data_valid = false;

	/* Register sysfs hooks under input dev */
	rc = sysfs_create_group(&data->input_dev_ps->dev.kobj,
			&stmvl53l1_attr_group);
	if (rc) {
		rc = -ENOMEM;
		vl53l1_errmsg("%d error:%d\n", __LINE__, rc);
		goto exit_unregister_dev_ps;
	}

	data->enable_sensor = 0;

	data->poll_delay_ms = STMVL53L1_CFG_POLL_DELAY_MS;
	data->timing_budget = STMVL53L1_CFG_TIMING_BUDGET_US;
	data->preset_mode = STMVL53L1_CFG_DEFAULT_MODE;
	data->distance_mode = STMVL53L1_CFG_DEFAULT_DISTANCE_MODE;
	data->crosstalk_enable = STMVL53L1_CFG_DEFAULT_CROSSTALK_ENABLE;
	data->output_mode = STMVL53L1_CFG_DEFAULT_OUTPUT_MODE;
	data->cam_mode = 0;
	data->sar_mode = 0;
	stmvl53l1_setup_auto_config(data);

	data->is_delay_allowed = true;
	/* need to be done once */
	rc = VL53L1_DataInit(&data->stdev);
	data->is_delay_allowed = false;
	if (rc) {
		vl53l1_errmsg("VL53L1_data_init %d\n", rc);
		goto exit_unregister_dev_ps;
	}

	rc = VL53L1_GetDeviceInfo(&data->stdev, &dev_info);
	if (rc) {
		vl53l1_errmsg("VL53L1_GetDeviceInfo %d\n", rc);
		goto exit_unregister_dev_ps;
	}
	vl53l1_errmsg("device Product type 0x%x name %s\ntype %s\n",
			dev_info.ProductType, dev_info.Name, dev_info.Type);

	data->hw_rev = dev_info.ProductRevisionMinor;

	memset(&cali_data, 0, sizeof(cali_data));
	rc = VL53L1_GetCalibrationData(&data->stdev, &cali_data);
	if (rc)
		vl53l1_errmsg("VL53L1_GetCalibrationData fail\n");
	else {
		data->inner_offset = cali_data.customer.mm_config__inner_offset_mm;
		data->outer_offset = cali_data.customer.mm_config__outer_offset_mm;

		/* update xtalk values */
		cali_data.customer.algo__crosstalk_compensation_plane_offset_kcps
			= data->xtalk_offset;
		cali_data.customer.algo__crosstalk_compensation_x_plane_gradient_kcps
			= data->xtalk_x;
		cali_data.customer.algo__crosstalk_compensation_y_plane_gradient_kcps
			= data->xtalk_y;

		cali_data.xtalkhisto.xtalk_shape.VL53L1_PRM_00015    = 0 ;
		cali_data.xtalkhisto.xtalk_shape.VL53L1_PRM_00016    = 24;
		cali_data.xtalkhisto.xtalk_shape.VL53L1_PRM_00017    = 12;
		cali_data.xtalkhisto.xtalk_shape.bin_data[0]    = 353;
		cali_data.xtalkhisto.xtalk_shape.bin_data[1]    = 472;
		cali_data.xtalkhisto.xtalk_shape.bin_data[2]    = 472;
		cali_data.xtalkhisto.xtalk_shape.bin_data[3]    = 353;
		cali_data.xtalkhisto.xtalk_shape.bin_data[4] 	  = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[5]    = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[6]    = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[7]    = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[8]    = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[9]    = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[10]   = 0;
		cali_data.xtalkhisto.xtalk_shape.bin_data[11]   = 0;
		cali_data.xtalkhisto.xtalk_shape.phasecal_result__reference_phase = 10240;
		cali_data.xtalkhisto.xtalk_shape.phasecal_result__vcsel_start     = 6;
		cali_data.xtalkhisto.xtalk_shape.cal_config__vcsel_start          = 9;
		cali_data.xtalkhisto.xtalk_shape.vcsel_width                      = 40;
		cali_data.xtalkhisto.xtalk_shape.VL53L1_PRM_00018                 = 48332;
		cali_data.xtalkhisto.xtalk_shape.zero_distance_phase              = 4096;
		cali_data.gain_cal.standard_ranging_gain_factor = 0x07D7;
		cali_data.gain_cal.histogram_ranging_gain_factor = 0x07CC;

		rc = VL53L1_SetCalibrationData(&data->stdev, &cali_data);
		if (rc)
			vl53l1_errmsg("VL53L1_SetCalibrationData fail\n");
		else {
			vl53l1_info("device xtalk data updated: %u %d %d\n",
                    cali_data.customer.algo__crosstalk_compensation_plane_offset_kcps,
                    cali_data.customer.algo__crosstalk_compensation_x_plane_gradient_kcps,
                    cali_data.customer.algo__crosstalk_compensation_y_plane_gradient_kcps);
		}
	}

	/* if working in interrupt ask intr to enable and hook the handler */
	data->poll_mode = 0;
	rc = stmvl53l1_module_func_tbl.start_intr(data->client_object,
		&data->poll_mode);
	if (rc < 0) {
		vl53l1_errmsg("can't start no  intr\n");
		goto exit_unregister_dev_ps;
	}

	data->is_first_irq = true;
	data->is_delay_allowed = false;

	/* to register as a misc device */
	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	/* multiple dev name use id in name but 1st */
	if (data->id == 0)
		strcpy(data->name, VL53L1_MISC_DEV_NAME);
	else
		sprintf(data->name, "%s%d", VL53L1_MISC_DEV_NAME, data->id);

	data->miscdev.name = data->name;
	data->miscdev.fops = &stmvl53l1_ranging_fops;
	vl53l1_errmsg("Misc device registration name:%s\n", data->miscdev.name);
	rc = misc_register(&data->miscdev);
	if (rc != 0) {
		vl53l1_errmsg("misc dev reg fail\n");
		goto exit_unregister_dev_ps;
	}
	/* bring back device under reset */
	reset_hold(data);

	return 0;

exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_ipp_cleanup:
	stmvl53l1_ipp_cleanup(data);

	return rc;
}


void stmvl53l1_cleanup(struct stmvl53l1_data *data)
{
int rc;

	vl53l1_dbgmsg("enter\n");
	rc = _ctrl_stop(data);
	if (rc < 0)
		vl53l1_errmsg("stop failed %d aborting anyway\n", rc);

	if (data->input_dev_ps) {
		vl53l1_dbgmsg("to remove sysfs group\n");
		sysfs_remove_group(&data->input_dev_ps->dev.kobj,
				&stmvl53l1_attr_group);

		vl53l1_dbgmsg("to unregister input dev\n");
		input_unregister_device(data->input_dev_ps);
	}

	if (!IS_ERR(data->miscdev.this_device) &&
			data->miscdev.this_device != NULL) {
		vl53l1_dbgmsg("to unregister misc dev\n");
		misc_deregister(&data->miscdev);
	}
	stmvl53l1_ipp_cleanup(data);
	/* be sure device is put under reset */
	data->force_device_on_en = false;
	reset_hold(data);
	vl53l1_dbgmsg("done\n");
	deallocate_dev_id(data->id);
	data->is_device_remove = true;
}

#ifdef CONFIG_COMPAT
static long stmvl53l1_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret;
	struct stmvl53l1_data *data =
			container_of(file->private_data,
			struct stmvl53l1_data, miscdev);
	ret = stmvl53l1_ioctl_handler(data, cmd, arg, compat_ptr(arg));
	return ret;
}
#endif

static long stmvl53l1_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg){
	long ret;
	struct stmvl53l1_data *data =
			container_of(file->private_data,
				struct stmvl53l1_data, miscdev);
	ret = stmvl53l1_ioctl_handler(data, cmd, arg, (void __user *)arg);
	return ret;
}

static int __init stmvl53l1_init(void)
{
	int rc = -1;

	vl53l1_dbgmsg("Enter\n");

	rc = stmvl53l1_ipp_init();
	if (rc)
		goto done;
	/* i2c/cci client specific init function */
	rc = stmvl53l1_module_func_tbl.init();
done:
	vl53l1_dbgmsg("End %d\n", rc);

	return rc;
}

static void __exit stmvl53l1_exit(void)
{
	vl53l1_dbgmsg("Enter\n");
	stmvl53l1_module_func_tbl.deinit(NULL);
	if (stmvl53l1_module_func_tbl.clean_up != NULL)
		stmvl53l1_module_func_tbl.clean_up();
	stmvl53l1_ipp_exit();
	vl53l1_dbgmsg("End\n");
}


/* MODULE_DEVICE_TABLE(i2c, stmvl53l1_id); */
MODULE_AUTHOR("STMicroelectronics Imaging Division");
MODULE_DESCRIPTION("ST FlightSense Time-of-Flight sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(stmvl53l1_init);
module_exit(stmvl53l1_exit);
