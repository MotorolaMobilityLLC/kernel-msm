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
 * @file ipp_linux.c kernel side implementation of vl53l1 protected processing
 *
 *  @date  Sep 1, 2016
 *  @author : imaging
 *
 *  @ingroup ipp_dev
 */

#include "stmvl53l1.h"

#define IPP_ERR_CODE (VL53L1_ERROR_PLATFORM_SPECIFIC_START-1)

/* FIXME: factorize ipp code since logic is the same */

/**
 *
 * Kernel entry point for ipp_hist_process_data IPP

 *
 * @param dev
 * @param pdmax_cal
 * @param pdmax_cfg
 * @param ppost_cfg
 * @param pbins0
 * @param pbins1
 * @param pxtalk
 * @param presults
 * @return
 */
VL53L1_Error VL53L1_ipp_hist_process_data(
	VL53L1_DEV dev,
	VL53L1_dmax_calibration_data_t    *pdmax_cal,
	VL53L1_hist_gen3_dmax_config_t    *pdmax_cfg,
	VL53L1_hist_post_process_config_t *ppost_cfg,
	VL53L1_histogram_bin_data_t       *pbins0,
	VL53L1_histogram_bin_data_t       *pbins1,
	VL53L1_xtalk_histogram_data_t     *pxtalk,
	VL53L1_range_results_t            *presults)
{
	int rc;
	int payload;
	struct stmvl53l1_data *data;
	VL53L1_range_results_t *presults_ipp;

	IPP_SERIALIZE_VAR;

	struct ipp_work_t *pout;
	struct ipp_work_t *pin;

	data = (struct stmvl53l1_data *) container_of(dev,
			struct stmvl53l1_data, stdev);

	stmvl531_ipp_tim_start(data);
	mutex_lock(&data->ipp.mutex);
	/* re-entrant call ? how can */

	/* for beter safeness and re-entrance handling we shall use local
	 * work dyn allocated one or check that ipp is not locked already
	 * at least
	 */
	if (data->ipp.buzy) {
		vl53l1_errmsg("try exec new ipp but still buzy on previous");
		/*  TODO shall we discard it and push new ? */
		rc = IPP_ERR_CODE;
		goto done;
	};
	pin = &data->ipp.work;
	pout = &data->ipp.work_out;

	IPP_SERIALIZE_START(pin->data, 6);
	IPP_SET_ARG_PTR(pin->data, 0, pdmax_cal);
	IPP_SET_ARG_PTR(pin->data, 1, pdmax_cfg);
	IPP_SET_ARG_PTR(pin->data, 2, ppost_cfg);
	IPP_SET_ARG_PTR(pin->data, 3, pbins0);
	IPP_SET_ARG_PTR(pin->data, 4, pbins1);
	IPP_SET_ARG_PTR(pin->data, 5, pxtalk);

	pin->payload = IPP_SERIALIZE_PAYLAOD();
	pin->process_no = stmvl53l1_ipp_cal_hist;
	stmvl531_ipp_tim_start(data);
	rc = stmvl53l1_ipp_do(data, pin, pout);
	if (rc != 0) {
		/* FIXME shall we retry here more specific status ? */
		vl53l1_errmsg("stmvl53l1_ipp_do err %d\n", rc);
		rc = IPP_ERR_CODE;
		goto done;
	}
	vl53l1_dbgmsg("ipp ok \n");
	/* check what we got back if valid answer error etc */
	if (pout->status) {
		vl53l1_errmsg("ipp error status %d from user", pout->status);
		if (pout->status >= stmvl53l1_ipp_status_proc_code)
			rc = pout->status & (stmvl53l1_ipp_status_proc_code-1);
		else
			rc = IPP_ERR_CODE;
		goto done;
	}
	/* process status ok deserialize , check return data payload is ok */
	IPP_SERIALIZE_START(pout->data, 1);
	IPP_OUT_ARG_PTR(pout->data, 0, presults_ipp);
	payload = IPP_SERIALIZE_PAYLAOD();
	if (pout->payload != payload) {
		/* bad formated answer */
		vl53l1_errmsg("bad payload %d != %d in ipp work back",
				pout->payload, payload);
		rc = IPP_ERR_CODE;
		goto done;
	}
	/* ok copy final output */
	memcpy(presults, presults_ipp, sizeof(*presults));
	stmvl531_ipp_tim_stop(data);
	stmvl531_ipp_stat(data, "ipp #%5x to=%3d fm=%3d in %5ld us",
			pin->xfer_id, pin->payload,
			pout->payload,
			stmvl531_ipp_time(data));
	rc = 0;
done:
	mutex_unlock(&data->ipp.mutex);
	return rc;
}

/**
 * kernel entry point for IPP xtalk_calibration_process_data
 *
 * @param Dev
 * @param pxtalk_ranges
 * @param pxtalk_hist
 * @param pxtalk_shape
 * @param pxtalk_cal
 * @return
 */
VL53L1_API VL53L1_Error VL53L1_ipp_xtalk_calibration_process_data(
	VL53L1_DEV                          dev,
	VL53L1_xtalk_range_results_t       *pxtalk_ranges,
	VL53L1_xtalk_histogram_data_t      *pxtalk_shape,
	VL53L1_xtalk_calibration_results_t *pxtalk_cal)
{
	int rc;
	int payload;
	struct stmvl53l1_data *data;
	VL53L1_xtalk_calibration_results_t *presults_ipp;

	IPP_SERIALIZE_VAR;

	struct ipp_work_t *pout;
	struct ipp_work_t *pin;

	data = (struct stmvl53l1_data *) container_of(dev,
			struct stmvl53l1_data, stdev);

	stmvl531_ipp_tim_start(data);
	mutex_lock(&data->ipp.mutex);
	/* re-entrant call ? how can */

	/* for beter safeness and re-entrance handling we shall use local
	 * work dyn allocated one or check that ipp is not locked already
	 * at least
	 */
	if (data->ipp.buzy) {
		vl53l1_errmsg("try exec new ipp but still buzy on previous");
		/*  TODO shall we discard it and push new ? */
		rc = IPP_ERR_CODE;
		goto done;
	};
	pin = &data->ipp.work;
	pout = &data->ipp.work_out;

	IPP_SERIALIZE_START(pin->data, 2);
	IPP_SET_ARG_PTR(pin->data, 0, pxtalk_ranges);
	IPP_SET_ARG_PTR(pin->data, 1, pxtalk_shape);

	pin->payload = IPP_SERIALIZE_PAYLAOD();
	pin->process_no = stmvl53l1_ipp_xtalk_calibration;
	stmvl531_ipp_tim_start(data);
	rc = stmvl53l1_ipp_do(data, pin, pout);
	if (rc != 0) {
		/* FIXME shall we retry here more specific status ? */
		vl53l1_errmsg("stmvl53l1_ipp_do err %d\n", rc);
		rc = IPP_ERR_CODE;
		goto done;
	}
	vl53l1_dbgmsg("ipp ok \n");
	/* check what we got back if valid answer error etc */
	if (pout->status) {
		vl53l1_errmsg("ipp error status %d from user", pout->status);
		if (pout->status >= stmvl53l1_ipp_status_proc_code)
			rc = pout->status & (stmvl53l1_ipp_status_proc_code-1);
		else
			rc = IPP_ERR_CODE;
		goto done;
	}
	/* process status ok deserialize , check return data payload is ok */
	IPP_SERIALIZE_START(pout->data, 1);
	IPP_OUT_ARG_PTR(pout->data, 0, presults_ipp);
	payload = IPP_SERIALIZE_PAYLAOD();
	if (pout->payload != payload) {
		/* bad formated answer */
		vl53l1_errmsg("bad payload %d != %d in ipp work back",
				pout->payload, payload);
		rc = IPP_ERR_CODE;
		goto done;
	}
	/* ok copy final output */
	memcpy(pxtalk_cal, presults_ipp, sizeof(*pxtalk_cal));
	stmvl531_ipp_tim_stop(data);
	stmvl531_ipp_stat(data, "ipp #%5x to=%3d fm=%3d in %5ld us",
			pin->xfer_id, pin->payload,
			pout->payload,
			stmvl531_ipp_time(data));
	rc = 0;
done:
	mutex_unlock(&data->ipp.mutex);
	return rc;
}
