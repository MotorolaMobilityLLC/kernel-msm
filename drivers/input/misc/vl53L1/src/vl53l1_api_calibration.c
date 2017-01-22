
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L1 Core may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following provisions apply instead of the ones
* mentioned above :
*
********************************************************************************
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
*
*
********************************************************************************
*
*/





































#include "vl53l1_ll_def.h"
#include "vl53l1_ll_device.h"
#include "vl53l1_platform.h"
#include "vl53l1_platform_ipp.h"
#include "vl53l1_register_map.h"
#include "vl53l1_register_funcs.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_hist_map.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_core.h"
#include "vl53l1_wait.h"
#include "vl53l1_api_preset_modes.h"
#include "vl53l1_fpga_core.h"
#include "vl53l1_silicon_core.h"
#include "vl53l1_api_core.h"
#include "vl53l1_api_calibration.h"






#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, status, \
		fmt, ##__VA_ARGS__)

#define trace_print(level, ...) \
	_LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_CORE, \
	level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


VL53L1_Error VL53L1_run_ref_spad_char(
	VL53L1_DEV        Dev)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t comms_buffer[6];

	VL53L1_refspadchar_config_t *prefspadchar  = &(pdev->refspadchar);

	LOG_FUNCTION_START("");






	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_enable_powerforce(Dev);






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_ref_spad_char_config(
				Dev,
				prefspadchar->VL53L1_PRM_00007,
				prefspadchar->timeout_us,
				prefspadchar->target_count_rate_mcps,
				prefspadchar->max_count_rate_limit_mcps,
				prefspadchar->min_count_rate_limit_mcps,
				pdev->stat_nvm.osc_measured__fast_osc__frequency);






	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_run_device_test(
					Dev,
					prefspadchar->device_test_mode);






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				VL53L1_REF_SPAD_CHAR_RESULT__NUM_ACTUAL_REF_SPADS,
				comms_buffer,
				2);

	if (status == VL53L1_ERROR_NONE) {
		pdev->dbg_results.ref_spad_char_result__num_actual_ref_spads =
				comms_buffer[0];
		pdev->dbg_results.ref_spad_char_result__ref_location =
				comms_buffer[1];
	}






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				VL53L1_REF_SPAD_MAN__NUM_REQUESTED_REF_SPADS,
				comms_buffer,
				2);

	if (status == VL53L1_ERROR_NONE) {
		pdev->customer.ref_spad_man__num_requested_ref_spads =
				comms_buffer[0];
		pdev->customer.ref_spad_man__ref_location =
				comms_buffer[1];
	}










	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				VL53L1_RESULT__SPARE_0_SD1,
				comms_buffer,
				6);







	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				VL53L1_GLOBAL_CONFIG__SPAD_ENABLES_REF_0,
				comms_buffer,
				6);

	if (status == VL53L1_ERROR_NONE) {
		pdev->customer.global_config__spad_enables_ref_0 =
				comms_buffer[0];
		pdev->customer.global_config__spad_enables_ref_1 =
				comms_buffer[1];
		pdev->customer.global_config__spad_enables_ref_2 =
				comms_buffer[2];
		pdev->customer.global_config__spad_enables_ref_3 =
				comms_buffer[3];
		pdev->customer.global_config__spad_enables_ref_4 =
				comms_buffer[4];
		pdev->customer.global_config__spad_enables_ref_5 =
				comms_buffer[5];
	}

	if (status == VL53L1_ERROR_NONE) {

		switch (pdev->sys_results.result__range_status) {

		case VL53L1_DEVICEERROR_REFSPADCHARNOTENOUGHDPADS:
			status = VL53L1_ERROR_REF_SPAD_CHAR_NOT_ENOUGH_SPADS;
			break;

		case VL53L1_DEVICEERROR_REFSPADCHARMORETHANTARGET:
			status = VL53L1_ERROR_REF_SPAD_CHAR_RATE_TOO_HIGH;
			break;

		case VL53L1_DEVICEERROR_REFSPADCHARLESSTHANTARGET:
			status = VL53L1_ERROR_REF_SPAD_CHAR_RATE_TOO_LOW;
			break;
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_run_xtalk_extraction(
	VL53L1_DEV	                        Dev,
	uint32_t                            mm_config_timeout_us,
	uint32_t                            range_config_timeout_us,
	uint8_t                             num_of_samples,
	uint16_t                            xtalk_filter_thresh_mm)
{
















	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);






	uint8_t results_invalid  = 0;

	uint8_t i                = 0;
	uint8_t measurement_mode = VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;

	LOG_FUNCTION_START("");







	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.central_histogram_avg));

	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.central_histogram_sum));











	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_set_preset_mode(
				Dev,
				VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_PLANAR,
				mm_config_timeout_us,
				range_config_timeout_us,
				100);




	if (status == VL53L1_ERROR_NONE)
		VL53L1_disable_xtalk_compensation(
			Dev);






	pdev->xtalk_results.max_results    = VL53L1_MAX_XTALK_RANGE_RESULTS;
	pdev->xtalk_results.active_results = pdev->zone_cfg.active_zones+1;

	for (i = 0 ; i < pdev->xtalk_results.max_results ; i++) {
		pdev->xtalk_results.VL53L1_PRM_00004[i].no_of_samples           = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].signal_total_events_avg = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].signal_total_events_sum = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].rate_per_spad_kcps_sum  = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].rate_per_spad_kcps_avg  = 0;
	}




	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_get_and_avg_xtalk_samples(
				Dev,
				num_of_samples,
				measurement_mode,
				xtalk_filter_thresh_mm,
				0x0,

				0x4,

				&(pdev->xtalk_results),
				&(pdev->xtalk_results.central_histogram_sum),
				&(pdev->xtalk_results.central_histogram_avg));
	}











	if (status == VL53L1_ERROR_NONE) {
			if (pdev->xtalk_results.VL53L1_PRM_00004[4].no_of_samples == 0) {
				results_invalid = 0x01;
			}
	}







	if (status == VL53L1_ERROR_NONE) {

		if (results_invalid == 0)  {

			if (status == VL53L1_ERROR_NONE)

				status = VL53L1_ipp_xtalk_calibration_process_data(
							Dev,
							&(pdev->xtalk_results),
							&(pdev->xtalk_shapes),
							&(pdev->xtalk_cal));

			if (status == VL53L1_ERROR_NONE) {

				pdev->xtalk_cfg.algo__crosstalk_compensation_x_plane_gradient_kcps =
					pdev->xtalk_cal.algo__crosstalk_compensation_x_plane_gradient_kcps;
				pdev->xtalk_cfg.algo__crosstalk_compensation_y_plane_gradient_kcps =
					pdev->xtalk_cal.algo__crosstalk_compensation_y_plane_gradient_kcps;
				pdev->xtalk_cfg.algo__crosstalk_compensation_plane_offset_kcps =
					pdev->xtalk_cal.algo__crosstalk_compensation_plane_offset_kcps;


			}


		}
	}




	if (status == VL53L1_ERROR_NONE)
		VL53L1_enable_xtalk_compensation(
			Dev);




	if (results_invalid > 0)
		status = VL53L1_ERROR_XTALK_EXTRACTION_FAIL;

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_run_xtalk_extraction_dual_reflectance(
	VL53L1_DEV	                        Dev,
	uint32_t                            mm_config_timeout_us,
	uint32_t                            range_config_timeout_us,
	uint8_t                             num_of_samples,
	uint8_t                             calc_parms,
	uint8_t                             higher_reflectance,
	uint16_t                            expected_target_distance_mm,
	uint16_t                            xtalk_filter_thresh_mm)
{






















	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);






	VL53L1_histogram_bin_data_t *pxtalk_avg_samples = &(pdev->xtalk_results.xtalk_avg[0]);

	VL53L1_range_results_t       results_int;
	VL53L1_range_results_t      *presults_int = &(results_int);

	VL53L1_range_data_t          *prange_data = &(presults_int->VL53L1_PRM_00004[0]);

	uint8_t results_invalid  = 0;

	uint8_t i                = 0;
	uint8_t measurement_mode = VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;

	LOG_FUNCTION_START("");







	if (calc_parms == 0x00) {
		VL53L1_init_histogram_bin_data_struct(
				0,
				(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
				&(pdev->xtalk_results.histogram_avg_1[0]));

		VL53L1_init_histogram_bin_data_struct(
				0,
				(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
				&(pdev->xtalk_results.histogram_avg_1[1]));

		VL53L1_init_histogram_bin_data_struct(
				0,
				(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
				&(pdev->xtalk_results.histogram_avg_1[2]));

		VL53L1_init_histogram_bin_data_struct(
				0,
				(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
				&(pdev->xtalk_results.histogram_avg_1[3]));

		VL53L1_init_histogram_bin_data_struct(
				0,
				(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
				&(pdev->xtalk_results.histogram_avg_1[4]));
	}

	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.histogram_avg_2[0]));

	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.histogram_avg_2[1]));

	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.histogram_avg_2[2]));

	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.histogram_avg_2[3]));

	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->xtalk_results.histogram_avg_2[4]));












	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_set_preset_mode(
				Dev,
				VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_PLANAR,
				mm_config_timeout_us,
				range_config_timeout_us,
				100);




	if (status == VL53L1_ERROR_NONE)
		VL53L1_disable_xtalk_compensation(
			Dev);






	pdev->xtalk_results.max_results    = VL53L1_MAX_XTALK_RANGE_RESULTS;
	pdev->xtalk_results.active_results = pdev->zone_cfg.active_zones+1;

	for (i = 0 ; i < pdev->xtalk_results.max_results ; i++) {
		pdev->xtalk_results.VL53L1_PRM_00004[i].no_of_samples           = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].signal_total_events_avg = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].signal_total_events_sum = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].rate_per_spad_kcps_sum  = 0;
		pdev->xtalk_results.VL53L1_PRM_00004[i].rate_per_spad_kcps_avg  = 0;
	}




	if ((status == VL53L1_ERROR_NONE) &&
			(calc_parms == 0x00)) {

		status = VL53L1_get_and_avg_all_xtalk_samples(
				Dev,
				num_of_samples,
				measurement_mode,
				xtalk_filter_thresh_mm,
				&(pdev->xtalk_results),
				&(pdev->xtalk_results.histogram_avg_1[0]),
				&(pdev->xtalk_results.histogram_avg_1[1]),
				&(pdev->xtalk_results.histogram_avg_1[2]),
				&(pdev->xtalk_results.histogram_avg_1[3]),
				&(pdev->xtalk_results.histogram_avg_1[4])
				);

	} else {

		status = VL53L1_get_and_avg_all_xtalk_samples(
				Dev,
				num_of_samples,
				measurement_mode,
				xtalk_filter_thresh_mm,
				&(pdev->xtalk_results),
				&(pdev->xtalk_results.histogram_avg_2[0]),
				&(pdev->xtalk_results.histogram_avg_2[1]),
				&(pdev->xtalk_results.histogram_avg_2[2]),
				&(pdev->xtalk_results.histogram_avg_2[3]),
				&(pdev->xtalk_results.histogram_avg_2[4])
				);
	}




	if ((status == VL53L1_ERROR_NONE) &&
			(calc_parms == 0x01)) {
		status = VL53L1_ipp_generate_dual_reflectance_xtalk_samples(
				Dev,
				&(pdev->xtalk_results),
				expected_target_distance_mm,
				higher_reflectance,
				&(pdev->xtalk_results.xtalk_avg[0]));
	}




	for (i = 0 ; i < 5 ; i++) {

		if ((status == VL53L1_ERROR_NONE) &&
				(calc_parms == 0x01)) {

			status = VL53L1_ipp_hist_process_data(
				Dev,
				&(pdev->dmax_cal),
				&(pdev->dmax_cfg),
				&(pdev->histpostprocess),
				&(pxtalk_avg_samples[i]),
				&(pdev->xtalk_shapes),
				presults_int);
		}

		if (status == VL53L1_ERROR_NONE) {
			if ((presults_int->active_results > 0) &&
				(prange_data->median_range_mm <
				(int16_t)xtalk_filter_thresh_mm)) {

				pdev->xtalk_results.VL53L1_PRM_00004[i].no_of_samples++;

				pdev->xtalk_results.VL53L1_PRM_00004[i].rate_per_spad_kcps_avg =
					prange_data->VL53L1_PRM_00008;

				pdev->xtalk_results.VL53L1_PRM_00004[i].signal_total_events_avg =
							prange_data->VL53L1_PRM_00009;
			}
		}
	}




	memcpy(&(pdev->xtalk_results.central_histogram_avg), &(pxtalk_avg_samples[4]),  sizeof(VL53L1_histogram_bin_data_t));










	if ((status == VL53L1_ERROR_NONE) &&
			(calc_parms == 0x01)) {
			if (pdev->xtalk_results.VL53L1_PRM_00004[4].no_of_samples == 0) {
				results_invalid = 0x01;
			}
	}








	if ((status == VL53L1_ERROR_NONE) &&
			(calc_parms == 0x01)) {

		if (results_invalid == 0)  {

			if (status == VL53L1_ERROR_NONE)

				status = VL53L1_ipp_xtalk_calibration_process_data(
							Dev,
							&(pdev->xtalk_results),
							&(pdev->xtalk_shapes),
							&(pdev->xtalk_cal));

			if (status == VL53L1_ERROR_NONE) {

				pdev->xtalk_cfg.algo__crosstalk_compensation_x_plane_gradient_kcps =
					pdev->xtalk_cal.algo__crosstalk_compensation_x_plane_gradient_kcps;
				pdev->xtalk_cfg.algo__crosstalk_compensation_y_plane_gradient_kcps =
					pdev->xtalk_cal.algo__crosstalk_compensation_y_plane_gradient_kcps;
				pdev->xtalk_cfg.algo__crosstalk_compensation_plane_offset_kcps =
					pdev->xtalk_cal.algo__crosstalk_compensation_plane_offset_kcps;


			}


		}
	}




	if (status == VL53L1_ERROR_NONE)
		VL53L1_enable_xtalk_compensation(
			Dev);




	if (results_invalid > 0)
		status = VL53L1_ERROR_XTALK_EXTRACTION_FAIL;


	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_get_and_avg_xtalk_samples(
		VL53L1_DEV	                  Dev,
		uint8_t                       num_of_samples,
		uint8_t                       measurement_mode,
		int16_t                       xtalk_filter_thresh_mm,
		uint8_t                       xtalk_result_id,
		uint8_t                       xtalk_histo_id,
		VL53L1_xtalk_range_results_t *pxtalk_results,
		VL53L1_histogram_bin_data_t  *psum_histo,
		VL53L1_histogram_bin_data_t  *pavg_histo)
{











	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);






	VL53L1_range_results_t      range_results;
	VL53L1_range_results_t      *prange_results = &range_results;

	VL53L1_range_data_t         *prange_data;
	VL53L1_xtalk_range_data_t   *pxtalk_range_data;

	uint8_t i                = 0;
	uint8_t j                = 0;
	uint8_t zone_id          = 0;
	uint8_t final_zone       = pdev->zone_cfg.active_zones+1;




	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_init_and_start_range(
				Dev,
				measurement_mode,
				VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS);


	for (i = 0 ; i <= (final_zone*num_of_samples) ; i++) {




		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_wait_for_range_completion(Dev);







		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_get_device_results(
					Dev,
					VL53L1_DEVICERESULTSLEVEL_FULL,
					prange_results);






		if (status == VL53L1_ERROR_NONE &&
			pdev->ll_state.rd_device_state !=
					VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC) {

			zone_id           = pdev->ll_state.rd_zone_id + xtalk_result_id;
			prange_data       = &(prange_results->VL53L1_PRM_00004[0]);



			if (prange_results->active_results > 1) {
				for (j = 1 ; j < prange_results->active_results ; j++) {

					if (prange_results->VL53L1_PRM_00004[j].median_range_mm <
						prange_data->median_range_mm){
							prange_data = &(prange_results->VL53L1_PRM_00004[j]);
					}
				}
			}

			pxtalk_range_data = &(pxtalk_results->VL53L1_PRM_00004[zone_id]);





			if ((prange_results->active_results > 0) &&
				(prange_data->median_range_mm <
						(int16_t)xtalk_filter_thresh_mm)) {

				pxtalk_range_data->no_of_samples++;

				pxtalk_range_data->rate_per_spad_kcps_sum +=
					prange_data->VL53L1_PRM_00008;

				pxtalk_range_data->signal_total_events_sum +=
									prange_data->VL53L1_PRM_00009;








				if (zone_id >= 4) {
					status = VL53L1_sum_histogram_data(
							&(pdev->hist_data),
							psum_histo);
				}
			}
		}















		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_wait_for_firmware_ready(Dev);











		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_clear_interrupt_and_enable_next_range(
					Dev,
					measurement_mode);


	}





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_stop_range(Dev);





	for (i = 0 ; i < (pdev->zone_cfg.active_zones+1) ; i++) {

		pxtalk_range_data = &(pxtalk_results->VL53L1_PRM_00004[i+xtalk_result_id]);

		if (pxtalk_range_data->no_of_samples > 0) {
			pxtalk_range_data->rate_per_spad_kcps_avg =
				pxtalk_range_data->rate_per_spad_kcps_sum /
				(uint32_t)pxtalk_range_data->no_of_samples;

			pxtalk_range_data->signal_total_events_avg =
						pxtalk_range_data->signal_total_events_sum /
						(int32_t)pxtalk_range_data->no_of_samples;
		} else {
			pxtalk_range_data->rate_per_spad_kcps_avg =
					pxtalk_range_data->rate_per_spad_kcps_sum;
			pxtalk_range_data->signal_total_events_avg =
					pxtalk_range_data->signal_total_events_sum;
		}
	}




	memcpy(pavg_histo, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));




	if (status == VL53L1_ERROR_NONE) {

		pxtalk_range_data = &(pxtalk_results->VL53L1_PRM_00004[xtalk_histo_id]);

		status = VL53L1_avg_histogram_data(
			pxtalk_range_data->no_of_samples,
			psum_histo,
			pavg_histo);
	}


	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_get_and_avg_all_xtalk_samples(
		VL53L1_DEV	                  Dev,
		uint8_t                       num_of_samples,
		uint8_t                       measurement_mode,
		int16_t                       xtalk_filter_thresh_mm,
		VL53L1_xtalk_range_results_t *pxtalk_results,
		VL53L1_histogram_bin_data_t  *pavg_histo_z0,
		VL53L1_histogram_bin_data_t  *pavg_histo_z1,
		VL53L1_histogram_bin_data_t  *pavg_histo_z2,
		VL53L1_histogram_bin_data_t  *pavg_histo_z3,
		VL53L1_histogram_bin_data_t  *pavg_histo_z4)
{











	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);






	VL53L1_range_results_t      range_results;
	VL53L1_range_results_t      *prange_results = &range_results;

	VL53L1_range_data_t         *prange_data;


	VL53L1_histogram_bin_data_t z_sum[4];

	VL53L1_histogram_bin_data_t *pz0_sum = &z_sum[0];
	VL53L1_histogram_bin_data_t *pz1_sum = &z_sum[1];
	VL53L1_histogram_bin_data_t *pz2_sum = &z_sum[2];
	VL53L1_histogram_bin_data_t *pz3_sum = &z_sum[3];
	VL53L1_histogram_bin_data_t *pz4_sum = &z_sum[4];

	VL53L1_histogram_bin_data_t *pzcurrent_sum = &z_sum[0];

	VL53L1_histogram_bin_data_t *pzcurrent_avg = pavg_histo_z0;


	uint8_t i                = 0;
	uint8_t zone_id          = 0;
	uint8_t final_zone       = pdev->zone_cfg.active_zones+1;





	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			pz0_sum);
	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			pz1_sum);
	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			pz2_sum);
	VL53L1_init_histogram_bin_data_struct(
			0,
			(uint16_t)VL53L1_HISTOGRAM_BUFFER_SIZE,
			pz3_sum);





	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_init_and_start_range(
				Dev,
				measurement_mode,
				VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS);


	for (i = 0 ; i <= (final_zone*num_of_samples) ; i++) {




		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_wait_for_range_completion(Dev);







		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_get_device_results(
					Dev,
					VL53L1_DEVICERESULTSLEVEL_FULL,
					prange_results);






		if (status == VL53L1_ERROR_NONE &&
			pdev->ll_state.rd_device_state !=
					VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC) {

			zone_id           = pdev->ll_state.rd_zone_id;
			prange_data       = &(prange_results->VL53L1_PRM_00004[0]);





			if ((prange_results->active_results > 0) &&
				(prange_data->median_range_mm <
						(int16_t)xtalk_filter_thresh_mm)) {




				switch (zone_id) {
				case 0:
					pzcurrent_sum = pz0_sum;
					break;
				case 1:
					pzcurrent_sum = pz1_sum;
					break;
				case 2:
					pzcurrent_sum = pz2_sum;
					break;
				case 3:
					pzcurrent_sum = pz3_sum;
					break;
				case 4:
					pzcurrent_sum = pz4_sum;
					break;
				default:
					pzcurrent_sum = pz0_sum;
					break;
				}






				status = VL53L1_sum_histogram_data(
						&(pdev->hist_data),
						pzcurrent_sum);


			}
		}

















		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_wait_for_firmware_ready(Dev);











		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_clear_interrupt_and_enable_next_range(
					Dev,
					measurement_mode);


	}





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_stop_range(Dev);





	memcpy(pavg_histo_z0, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));
	memcpy(pavg_histo_z1, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));
	memcpy(pavg_histo_z2, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));
	memcpy(pavg_histo_z3, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));
	memcpy(pavg_histo_z4, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));





	pzcurrent_sum = pz0_sum;

	for (i = 0 ; i < (pdev->zone_cfg.active_zones+1) ; i++) {

		switch (i) {
		case 0:
			pzcurrent_avg = pavg_histo_z0;
			break;
		case 1:
			pzcurrent_avg = pavg_histo_z1;
			break;
		case 2:
			pzcurrent_avg = pavg_histo_z2;
			break;
		case 3:
			pzcurrent_avg = pavg_histo_z3;
			break;
		case 4:
			pzcurrent_avg = pavg_histo_z4;
			break;
		default:
			pzcurrent_avg = pavg_histo_z0;
			break;
		}

		if (status == VL53L1_ERROR_NONE) {
			status = VL53L1_avg_histogram_data(
				num_of_samples,
				pzcurrent_sum,
				pzcurrent_avg);
		}

		pzcurrent_sum++;

	}


	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_run_offset_calibration(
	VL53L1_DEV	   Dev,
	uint32_t       range_config_timeout_us,
	uint8_t        pre_range_num_of_samples,
	uint8_t        mm1_num_of_samples,
	uint8_t        mm2_num_of_samples,
	int32_t        target_distance_mm)
{












	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_DevicePresetModes device_preset_modes[VL53L1_MAX_OFFSET_RANGE_RESULTS] =	{
		VL53L1_DEVICEPRESETMODE_STANDARD_RANGING,
		VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM1_CAL,
		VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM2_CAL,
	};

	VL53L1_range_results_t      range_results;
	VL53L1_range_results_t     *prange_results = &range_results;
	VL53L1_range_data_t        *prange_data = NULL;
	VL53L1_offset_range_data_t *poffset     = NULL;

	uint8_t  i                      = 0;
	uint8_t  m                      = 0;
	uint8_t  measurement_mode       =
		VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	uint16_t manual_effective_spads =
		pdev->gen_cfg.dss_config__manual_effective_spads_select;

	uint8_t num_of_samples[VL53L1_MAX_OFFSET_RANGE_RESULTS];

	LOG_FUNCTION_START("");



	num_of_samples[0] = pre_range_num_of_samples;
	num_of_samples[1] = mm1_num_of_samples;
	num_of_samples[2] = mm2_num_of_samples;




	pdev->customer.mm_config__inner_offset_mm         = 0;
	pdev->customer.mm_config__outer_offset_mm         = 0;
	pdev->customer.algo__part_to_part_range_offset_mm = 0;




	pdev->offset_results.max_results        = VL53L1_MAX_OFFSET_RANGE_RESULTS;
	pdev->offset_results.active_results     = VL53L1_MAX_OFFSET_RANGE_RESULTS;
	pdev->offset_results.target_distance_mm = target_distance_mm;

	for (m = 0 ; m < VL53L1_MAX_OFFSET_RANGE_RESULTS ; m++) {

		poffset = &(pdev->offset_results.VL53L1_PRM_00004[m]);

		poffset->preset_mode         = device_preset_modes[m];
		poffset->no_of_samples       = 0;
		poffset->effective_spads_sum = 0;
		poffset->effective_spads_avg = 0;
		poffset->peak_rate_mcps_sum  = 0;
		poffset->peak_rate_mcps_avg  = 0;
		poffset->median_range_mm_sum = 0;
		poffset->median_range_mm_avg = 0;




		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_set_preset_mode(
					Dev,
					device_preset_modes[m],
					range_config_timeout_us,
					range_config_timeout_us,
					100);

		pdev->gen_cfg.dss_config__manual_effective_spads_select =
				manual_effective_spads;




		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_init_and_start_range(
					Dev,
					measurement_mode,
					VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS);

		for (i = 0 ; i <= (num_of_samples[m]+2) ; i++) {




			if (status == VL53L1_ERROR_NONE)
				status =
					VL53L1_wait_for_range_completion(Dev);








			if (status == VL53L1_ERROR_NONE)
				status =
					VL53L1_get_device_results(
						Dev,
						VL53L1_DEVICERESULTSLEVEL_FULL,
						prange_results);









			prange_data  = &(prange_results->VL53L1_PRM_00004[0]);

			if (prange_results->active_results > 0 &&
				prange_results->stream_count   > 1) {

				if (prange_data->range_status ==
						VL53L1_DEVICEERROR_RANGECOMPLETE) {

					poffset->no_of_samples++;
					poffset->effective_spads_sum += (uint32_t)prange_data->VL53L1_PRM_00002;
					poffset->peak_rate_mcps_sum  += (uint32_t)prange_data->peak_signal_count_rate_mcps;
					poffset->median_range_mm_sum += (int32_t)prange_data->median_range_mm;

					poffset->dss_config__roi_mode_control =
						pdev->gen_cfg.dss_config__roi_mode_control;
					poffset->dss_config__manual_effective_spads_select =
						pdev->gen_cfg.dss_config__manual_effective_spads_select;
				}
			}












			if (status == VL53L1_ERROR_NONE)
				status =
					VL53L1_wait_for_firmware_ready(Dev);











			if (status == VL53L1_ERROR_NONE)
				status =
					VL53L1_clear_interrupt_and_enable_next_range(
						Dev,
						measurement_mode);
		}




		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_stop_range(Dev);




		if (poffset->no_of_samples > 0) {

			poffset->effective_spads_avg  = poffset->effective_spads_sum;
			poffset->effective_spads_avg += (poffset->no_of_samples/2);
			poffset->effective_spads_avg /= poffset->no_of_samples;

			poffset->peak_rate_mcps_avg   = poffset->peak_rate_mcps_sum;
			poffset->peak_rate_mcps_avg  += (poffset->no_of_samples/2);
			poffset->peak_rate_mcps_avg  /= poffset->no_of_samples;

			poffset->median_range_mm_avg  = poffset->median_range_mm_sum;
			poffset->median_range_mm_avg += (poffset->no_of_samples/2);
			poffset->median_range_mm_avg /= poffset->no_of_samples;

			poffset->range_mm_offset = target_distance_mm;
			poffset->range_mm_offset -= poffset->median_range_mm_avg;



			if (poffset->preset_mode ==
				VL53L1_DEVICEPRESETMODE_STANDARD_RANGING)
				manual_effective_spads =
					(uint16_t)poffset->effective_spads_avg;
		}
	}




	pdev->customer.mm_config__inner_offset_mm =
		(int16_t)pdev->offset_results.VL53L1_PRM_00004[1].range_mm_offset;
	pdev->customer.mm_config__outer_offset_mm =
		(int16_t)pdev->offset_results.VL53L1_PRM_00004[2].range_mm_offset;
	pdev->customer.algo__part_to_part_range_offset_mm = 0;







	pdev->add_off_cal_data.result__mm_inner_actual_effective_spads =
		(uint16_t)pdev->offset_results.VL53L1_PRM_00004[1].effective_spads_avg;
	pdev->add_off_cal_data.result__mm_outer_actual_effective_spads =
		(uint16_t)pdev->offset_results.VL53L1_PRM_00004[2].effective_spads_avg;

	pdev->add_off_cal_data.result__mm_inner_peak_signal_count_rtn_mcps =
		(uint16_t)pdev->offset_results.VL53L1_PRM_00004[1].peak_rate_mcps_avg;
	pdev->add_off_cal_data.result__mm_outer_peak_signal_count_rtn_mcps =
		(uint16_t)pdev->offset_results.VL53L1_PRM_00004[2].peak_rate_mcps_avg;




	pdev->histpostprocess.mm_config__inner_offset_mm =
			pdev->customer.mm_config__inner_offset_mm;
	pdev->histpostprocess.mm_config__outer_offset_mm =
			pdev->customer.mm_config__outer_offset_mm;




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_customer_nvm_managed(
				Dev,
				&(pdev->customer));






	if (status == VL53L1_ERROR_NONE &&
	    pdev->add_off_cal_data.result__mm_inner_actual_effective_spads <
			VL53L1_OFFSET_CAL_MIN_MM1_EFFECTIVE_SPADS)
			status = VL53L1_ERROR_OFFSET_CAL_INSUFFICIENT_MM1_SPADS;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_ref_spad_char_config(
	VL53L1_DEV    Dev,
	uint8_t       vcsel_period_a,
	uint32_t      phasecal_timeout_us,
	uint16_t      total_rate_target_mcps,
	uint16_t      max_count_rate_rtn_limit_mcps,
	uint16_t      min_count_rate_rtn_limit_mcps,
	uint16_t      fast_osc_frequency)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[2];

	uint32_t macro_period_us = 0;
	uint32_t timeout_mclks   = 0;

	LOG_FUNCTION_START("");





	macro_period_us =
		VL53L1_calc_macro_period_us(
			fast_osc_frequency,
			vcsel_period_a);






	timeout_mclks = phasecal_timeout_us << 12;
	timeout_mclks = timeout_mclks + (macro_period_us>>1);
	timeout_mclks = timeout_mclks / macro_period_us;

	if (timeout_mclks > 0xFF)
		pdev->gen_cfg.phasecal_config__timeout_macrop = 0xFF;
	else
		pdev->gen_cfg.phasecal_config__timeout_macrop =
				(uint8_t)timeout_mclks;

	pdev->tim_cfg.range_config__vcsel_period_a = vcsel_period_a;






	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_WrByte(
				Dev,
				VL53L1_PHASECAL_CONFIG__TIMEOUT_MACROP,
				pdev->gen_cfg.phasecal_config__timeout_macrop);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrByte(
				Dev,
				VL53L1_RANGE_CONFIG__VCSEL_PERIOD_A,
				pdev->tim_cfg.range_config__vcsel_period_a);







	buffer[0] = pdev->tim_cfg.range_config__vcsel_period_a;
	buffer[1] = pdev->tim_cfg.range_config__vcsel_period_a;

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				VL53L1_SD_CONFIG__WOI_SD0,
				buffer,
				2);







	pdev->customer.ref_spad_char__total_rate_target_mcps =
			total_rate_target_mcps;

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrWord(
				Dev,
				VL53L1_REF_SPAD_CHAR__TOTAL_RATE_TARGET_MCPS,
				total_rate_target_mcps);


	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrWord(
				Dev,
				VL53L1_RANGE_CONFIG__SIGMA_THRESH,
				max_count_rate_rtn_limit_mcps);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrWord(
				Dev,
				VL53L1_RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS,
				min_count_rate_rtn_limit_mcps);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_run_device_test(
	VL53L1_DEV    Dev,
	uint8_t       device_test_mode)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      comms_buffer[2];
	uint8_t      gpio_hv_mux__ctrl = 0;

	LOG_FUNCTION_START("");






	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_RdByte(
				Dev,
				VL53L1_GPIO_HV_MUX__CTRL,
				&gpio_hv_mux__ctrl);

	if (status == VL53L1_ERROR_NONE)
		pdev->stat_cfg.gpio_hv_mux__ctrl = gpio_hv_mux__ctrl;





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_start_test(
					Dev,
					device_test_mode);





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_wait_for_test_completion(Dev);





	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				VL53L1_RESULT__RANGE_STATUS,
				comms_buffer,
				2);

	if (status == VL53L1_ERROR_NONE) {
		pdev->sys_results.result__range_status  = comms_buffer[0];
		pdev->sys_results.result__report_status = comms_buffer[1];
	}




	pdev->sys_results.result__range_status &=
		VL53L1_RANGE_STATUS__RANGE_STATUS_MASK;

	if (status == VL53L1_ERROR_NONE)
		trace_print(
			VL53L1_TRACE_LEVEL_INFO,
			"    Device Test Complete:\n\t%-32s = %3u\n\t%-32s = %3u\n",
			"result__range_status",
			pdev->sys_results.result__range_status,
			"result__report_status",
			pdev->sys_results.result__report_status);





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_clear_interrupt(Dev);








	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_start_test(
				Dev,
				0x00);

	LOG_FUNCTION_END(status);

	return status;
}

