/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp.h"
#include <linux/fs.h>

#define SSP_DEBUG_TIMER_SEC		(10 * HZ)

#define LIMIT_RESET_CNT		20
#define LIMIT_TIMEOUT_CNT		3


/*************************************************************************/
/* SSP Debug timer function                                              */
/*************************************************************************/

int print_mcu_debug(char *pchRcvDataFrame, int *pDataIdx,
		int iRcvDataFrameLength)
{
	int iLength = pchRcvDataFrame[(*pDataIdx)++];
	int cur = *pDataIdx;

	if (iLength > iRcvDataFrameLength - *pDataIdx || iLength <= 0) {
		ssp_dbg("[SSP]: MSG From MCU - invalid debug length(%d/%d/%d)\n",
			iLength, iRcvDataFrameLength, cur);
		return iLength ? iLength : ERROR;
	}

	ssp_dbg("[SSP]: MSG From MCU - %s\n", &pchRcvDataFrame[*pDataIdx]);
	*pDataIdx += iLength;
	return 0;
}

void reset_mcu(struct ssp_data *data)
{
	func_dbg();
	ssp_enable(data, false);
	clean_pending_list(data);
	toggle_mcu_reset(data);
	ssp_enable(data, true);
}

void sync_sensor_state(struct ssp_data *data)
{
	unsigned char uBuf[9] = {0,};
	unsigned int uSensorCnt;
	int iRet = 0;

	iRet = set_gyro_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_gyro_cal failed\n", __func__);

	iRet = set_accel_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_accel_cal failed\n", __func__);

	udelay(10);

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++) {
		if (atomic_read(&data->aSensorEnable) & (1 << uSensorCnt)) {
			s32 dMsDelay =
				get_msdelay(data->adDelayBuf[uSensorCnt]);
			memcpy(&uBuf[0], &dMsDelay, 4);
			memcpy(&uBuf[4], &data->batchLatencyBuf[uSensorCnt], 4);
			uBuf[8] = data->batchOptBuf[uSensorCnt];
			send_instruction(data, ADD_SENSOR, uSensorCnt, uBuf, 9);
			udelay(10);
		}
	}

	data->bMcuDumpMode = ssp_check_sec_dump_mode();
	iRet = ssp_send_cmd(data, MSG2SSP_AP_MCU_SET_DUMPMODE,
				data->bMcuDumpMode);
	if (iRet < 0)
		pr_err("[SSP]: %s - MSG2SSP_AP_MCU_SET_DUMPMODE failed\n",
				__func__);
}

static void print_sensordata(struct ssp_data *data, unsigned int uSensor)
{
	switch (uSensor) {
	case ACCELEROMETER_SENSOR:
	case GYROSCOPE_SENSOR:
	case TILT_TO_WAKE:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].x, data->buf[uSensor].y,
			data->buf[uSensor].z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GEOMAGNETIC_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].cal_x, data->buf[uSensor].cal_y,
			data->buf[uSensor].cal_y, data->buf[uSensor].accuracy,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GEOMAGNETIC_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y,
			data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case STEP_DETECTOR:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].step_det,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GAME_ROTATION_VECTOR:
	case ROTATION_VECTOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].quat_a, data->buf[uSensor].quat_b,
			data->buf[uSensor].quat_c, data->buf[uSensor].quat_d,
			data->buf[uSensor].acc_rot,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case SIG_MOTION_SENSOR:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].sig_motion,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GYRO_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y,
			data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case STEP_COUNTER:
		ssp_dbg("[SSP] %u : %u(%ums)\n", uSensor,
			data->buf[uSensor].step_diff,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
#ifdef CONFIG_SENSORS_SSP_ADPD142
	case BIO_HRM_LIB:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].hr, data->buf[uSensor].rri,
			data->buf[uSensor].snr,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
#endif
	default:
		ssp_dbg("[SSP] Wrong sensorCnt: %u\n", uSensor);
		break;
	}
}

static void debug_work_func(struct work_struct *work)
{
	unsigned int uSensorCnt;
	struct ssp_data *data = container_of(work, struct ssp_data, work_debug);

	ssp_dbg("[SSP]: %s(%u) - Sensor state: 0x%x, RC: %u, CC: %u DC: %u\n",
		__func__, data->uIrqCnt, data->uSensorState, data->uResetCnt,
		data->uComFailCnt, data->uDumpCnt);

	switch (data->fw_dl_state) {
	case FW_DL_STATE_FAIL:
	case FW_DL_STATE_DOWNLOADING:
	case FW_DL_STATE_SYNC:
		pr_info("[SSP] : %s firmware downloading state = %d\n",
				__func__, data->fw_dl_state);
		return;
	}

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++)
		if ((atomic_read(&data->aSensorEnable) & (1 << uSensorCnt))
			|| data->batchLatencyBuf[uSensorCnt])
			print_sensordata(data, uSensorCnt);

	if (data->uTimeOutCnt > LIMIT_TIMEOUT_CNT) {
		if (data->uComFailCnt < LIMIT_RESET_CNT) {
			pr_info("[SSP] : %s - uTimeOutCnt(%u), pending(%u)\n",
				__func__, data->uTimeOutCnt,
				!list_empty(&data->pending_list));
			data->uComFailCnt++;
			reset_mcu(data);
		} else
			ssp_enable(data, false);
		data->uTimeOutCnt = 0;
	}

	data->uIrqCnt = 0;
}

static void debug_timer_func(unsigned long ptr)
{
	struct ssp_data *data = (struct ssp_data *)ptr;

	queue_work(data->debug_wq, &data->work_debug);
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void enable_debug_timer(struct ssp_data *data)
{
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void disable_debug_timer(struct ssp_data *data)
{
	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
}

int initialize_debug_timer(struct ssp_data *data)
{
	setup_timer(&data->debug_timer, debug_timer_func, (unsigned long)data);

	data->debug_wq = create_singlethread_workqueue("ssp_debug_wq");
	if (!data->debug_wq)
		return ERROR;

	INIT_WORK(&data->work_debug, debug_work_func);
	return SUCCESS;
}

/* if returns true dump mode on */
unsigned int  ssp_check_sec_dump_mode()
{
		return 0;
}
