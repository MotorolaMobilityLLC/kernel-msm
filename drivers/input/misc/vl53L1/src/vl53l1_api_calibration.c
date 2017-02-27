
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either 'STMicroelectronics
* Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L1 Core may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones
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

#ifdef VL53L1_LOG_ENABLE
  #include "vl53l1_api_debug.h"
#endif




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
	VL53L1_DEV        Dev,
	VL53L1_Error     *pcal_status)
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

#ifdef VL53L1_LOG_ENABLE


	if (status == VL53L1_ERROR_NONE)
		VL53L1_print_customer_nvm_managed(
			&(pdev->customer),
			"run_ref_spad_char():pdev->lldata.customer.",
			VL53L1_TRACE_MODULE_REF_SPAD_CHAR);
#endif

	if (status == VL53L1_ERROR_NONE) {

		switch (pdev->sys_results.result__range_status) {

		case VL53L1_DEVICEERROR_REFSPADCHARNOTENOUGHDPADS:
			status = VL53L1_WARNING_REF_SPAD_CHAR_NOT_ENOUGH_SPADS;
			break;

		case VL53L1_DEVICEERROR_REFSPADCHARMORETHANTARGET:
			status = VL53L1_WARNING_REF_SPAD_CHAR_RATE_TOO_HIGH;
			break;

		case VL53L1_DEVICEERROR_REFSPADCHARLESSTHANTARGET:
			status = VL53L1_WARNING_REF_SPAD_CHAR_RATE_TOO_LOW;
			break;
		}
	}






	*pcal_status = status;




	IGNORE_STATUS(
		IGNORE_REF_SPAD_CHAR_NOT_ENOUGH_SPADS,
		VL53L1_WARNING_REF_SPAD_CHAR_NOT_ENOUGH_SPADS,
		status);

	IGNORE_STATUS(
		IGNORE_REF_SPAD_CHAR_RATE_TOO_HIGH,
		VL53L1_WARNING_REF_SPAD_CHAR_RATE_TOO_HIGH,
		status);

	IGNORE_STATUS(
		IGNORE_REF_SPAD_CHAR_RATE_TOO_LOW,
		VL53L1_WARNING_REF_SPAD_CHAR_RATE_TOO_LOW,
		status);


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_run_xtalk_extraction(
	VL53L1_DEV	                        Dev,
	uint16_t                            dss_config__target_total_rate_mcps,
	uint32_t                            phasecal_config_timeout_us,
	uint32_t                            mm_config_timeout_us,
	uint32_t                            range_config_timeout_us,
	uint8_t                             num_of_samples,
	VL53L1_Error                       *pcal_status)
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
				dss_config__target_total_rate_mcps,
				phasecal_config_timeout_us,
				mm_config_timeout_us,
				range_config_timeout_us,
				100);




	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_disable_xtalk_compensation(
			Dev);
	}






	pdev->xtalk_results.max_results    = VL53L1_MAX_XTALK_RANGE_RESULTS;
	pdev->xtalk_results.active_results = pdev->zone_cfg.active_zones+1;




	pdev->xtalk_results.central_histogram__window_start = 0xFF;
	pdev->xtalk_results.central_histogram__window_end   = 0x00;

	pdev->xtalk_results.num_of_samples_status = 0x00;
	pdev->xtalk_results.zero_samples_status   = 0x00;
	pdev->xtalk_results.max_sigma_status      = 0x00;

	for (i = 0 ; i < pdev->xtalk_results.max_results ; i++) {
		pdev->xtalk_results.VL53L1_PRM_00005[i].no_of_samples           = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].signal_total_events_avg = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].signal_total_events_sum = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].rate_per_spad_kcps_sum  = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].rate_per_spad_kcps_avg  = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].sigma_mm_sum            = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].sigma_mm_avg            = 0;
	}




	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_get_and_avg_xtalk_samples(
				Dev,
				num_of_samples,
				measurement_mode,
				pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_range_mm,
				pdev->xtalk_cfg.algo__crosstalk_detect_min_valid_range_mm,
				pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_rate_kcps,
				0x0,

				0x4,

				&(pdev->xtalk_results),
				&(pdev->xtalk_results.central_histogram_sum),
				&(pdev->xtalk_results.central_histogram_avg));
	}











	if (status == VL53L1_ERROR_NONE) {
			if ((pdev->xtalk_results.VL53L1_PRM_00005[4].no_of_samples == 0) ||
				(pdev->xtalk_results.VL53L1_PRM_00005[4].sigma_mm_avg >
				pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm)) {
				results_invalid = 0x01;
			}
	}

#ifdef VL53L1_LOG_ENABLE
		if (status == VL53L1_ERROR_NONE)
			VL53L1_print_xtalk_range_results(
				&(pdev->xtalk_results),
				"pdev->xtalk_results",
				VL53L1_TRACE_MODULE_CORE);
#endif


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




	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_enable_xtalk_compensation(
			Dev);
	}




	if (status == VL53L1_ERROR_NONE) {

		for (i = 0 ; i < pdev->xtalk_results.max_results ; i++) {


			if (pdev->xtalk_results.VL53L1_PRM_00005[i].no_of_samples !=
				num_of_samples) {
				pdev->xtalk_results.num_of_samples_status =
					pdev->xtalk_results.num_of_samples_status |
					(1 << i);
			}


			if (pdev->xtalk_results.VL53L1_PRM_00005[i].no_of_samples ==
				0x00) {
				pdev->xtalk_results.zero_samples_status =
					pdev->xtalk_results.zero_samples_status |
					(1 << i);
			}









			if (pdev->xtalk_results.VL53L1_PRM_00005[i].sigma_mm_avg >
				pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm) {
				pdev->xtalk_results.max_sigma_status =
					pdev->xtalk_results.max_sigma_status |
					(1 << i);
			}
		}
	}













	if (results_invalid > 0) {


		if (pdev->xtalk_results.VL53L1_PRM_00005[4].no_of_samples == 0) {
			status = VL53L1_ERROR_XTALK_EXTRACTION_NO_SAMPLE_FAIL;
		} else {
			if (pdev->xtalk_results.VL53L1_PRM_00005[4].sigma_mm_avg >
			    pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm) {
				status = VL53L1_ERROR_XTALK_EXTRACTION_SIGMA_LIMIT_FAIL;
			}
		}
	} else {


		if (pdev->xtalk_results.zero_samples_status != 0x00) {
			status = VL53L1_WARNING_XTALK_NO_SAMPLES_FOR_GRADIENT;
		} else {
			if (pdev->xtalk_results.max_sigma_status != 0x00) {
				status = VL53L1_WARNING_XTALK_SIGMA_LIMIT_FOR_GRADIENT;
			} else {
				if (pdev->xtalk_results.num_of_samples_status !=
						0x00) {
					status = VL53L1_WARNING_XTALK_MISSING_SAMPLES;
				}
			}
		}
	}






	pdev->xtalk_results.cal_status = status;
	*pcal_status = pdev->xtalk_results.cal_status;




	IGNORE_STATUS(
		IGNORE_XTALK_EXTRACTION_NO_SAMPLE_FAIL,
		VL53L1_ERROR_XTALK_EXTRACTION_NO_SAMPLE_FAIL,
		status);

	IGNORE_STATUS(
		IGNORE_XTALK_EXTRACTION_SIGMA_LIMIT_FAIL,
		VL53L1_ERROR_XTALK_EXTRACTION_SIGMA_LIMIT_FAIL,
		status);

	IGNORE_STATUS(
		IGNORE_XTALK_EXTRACTION_NO_SAMPLE_FOR_GRADIENT_WARN,
		VL53L1_WARNING_XTALK_NO_SAMPLES_FOR_GRADIENT,
		status);

	IGNORE_STATUS(
		IGNORE_XTALK_EXTRACTION_SIGMA_LIMIT_FOR_GRADIENT_WARN,
		VL53L1_WARNING_XTALK_SIGMA_LIMIT_FOR_GRADIENT,
		status);

	IGNORE_STATUS(
		IGNORE_XTALK_EXTRACTION_MISSING_SAMPLES_WARN,
		VL53L1_WARNING_XTALK_MISSING_SAMPLES,
		status);

#ifdef VL53L1_LOG_ENABLE




	VL53L1_print_customer_nvm_managed(
		&(pdev->customer),
		"run_xtalk_extraction():pdev->lldata.customer.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_xtalk_config(
		&(pdev->xtalk_cfg),
		"run_xtalk_extraction():pdev->lldata.xtalk_cfg.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_histogram_bin_data(
		&(pdev->hist_data),
		"run_xtalk_extraction():pdev->lldata.hist_data.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_xtalk_histogram_data(
		&(pdev->xtalk_shapes),
		"pdev->lldata.xtalk_shapes.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_xtalk_range_results(
		&(pdev->xtalk_results),
		"run_xtalk_extraction():pdev->lldata.xtalk_results.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

#endif

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_run_xtalk_extraction_dual_reflectance(
	VL53L1_DEV	                        Dev,
	uint16_t                            dss_config__target_total_rate_mcps,
	uint32_t                            phasecal_config_timeout_us,
	uint32_t                            mm_config_timeout_us,
	uint32_t                            range_config_timeout_us,
	uint8_t                             num_of_samples,
	uint8_t                             calc_parms,
	uint8_t                             higher_reflectance,
	uint16_t                            expected_target_distance_mm,
	uint16_t                            xtalk_filter_thresh_mm,
	VL53L1_Error                       *pcal_status)
{






















	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);






	VL53L1_histogram_bin_data_t *pxtalk_avg_samples = &(pdev->xtalk_results.xtalk_avg[0]);

	VL53L1_range_results_t       results_int;
	VL53L1_range_results_t      *presults_int = &(results_int);

	VL53L1_range_data_t          *prange_data = &(presults_int->VL53L1_PRM_00005[0]);

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
				dss_config__target_total_rate_mcps,
				phasecal_config_timeout_us,
				mm_config_timeout_us,
				range_config_timeout_us,
				100);




	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_disable_xtalk_compensation(
			Dev);
	}






	pdev->xtalk_results.max_results    = VL53L1_MAX_XTALK_RANGE_RESULTS;
	pdev->xtalk_results.active_results = pdev->zone_cfg.active_zones+1;

	for (i = 0 ; i < pdev->xtalk_results.max_results ; i++) {
		pdev->xtalk_results.VL53L1_PRM_00005[i].no_of_samples           = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].signal_total_events_avg = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].signal_total_events_sum = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].rate_per_spad_kcps_sum  = 0;
		pdev->xtalk_results.VL53L1_PRM_00005[i].rate_per_spad_kcps_avg  = 0;
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
				&(pdev->fmt_dmax_cal),
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

				pdev->xtalk_results.VL53L1_PRM_00005[i].no_of_samples++;

				pdev->xtalk_results.VL53L1_PRM_00005[i].rate_per_spad_kcps_avg =
					prange_data->VL53L1_PRM_00008;

				pdev->xtalk_results.VL53L1_PRM_00005[i].signal_total_events_avg =
							prange_data->VL53L1_PRM_00009;
			}
		}
	}




	memcpy(&(pdev->xtalk_results.central_histogram_avg),
		   &(pxtalk_avg_samples[4]),
		   sizeof(VL53L1_histogram_bin_data_t));










	if ((status == VL53L1_ERROR_NONE) &&
			(calc_parms == 0x01)) {
			if (pdev->xtalk_results.VL53L1_PRM_00005[4].no_of_samples == 0) {
				results_invalid = 0x01;
			}
	}


#ifdef VL53L1_LOG_ENABLE
		if (status == VL53L1_ERROR_NONE)
			VL53L1_print_xtalk_range_results(
				&(pdev->xtalk_results),
				"pdev->xtalk_results",
				VL53L1_TRACE_MODULE_CORE);
#endif


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




	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_enable_xtalk_compensation(
			Dev);
	}




	if (results_invalid > 0)
		status = VL53L1_ERROR_XTALK_EXTRACTION_NO_SAMPLE_FAIL;






	pdev->xtalk_results.cal_status = status;
	*pcal_status = pdev->xtalk_results.cal_status;




	IGNORE_STATUS(
		IGNORE_XTALK_EXTRACTION_NO_SAMPLE_FAIL,
		VL53L1_ERROR_XTALK_EXTRACTION_NO_SAMPLE_FAIL,
		status);

#ifdef VL53L1_LOG_ENABLE




	VL53L1_print_customer_nvm_managed(
		&(pdev->customer),
		"pdev->lldata.customer.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_xtalk_config(
		&(pdev->xtalk_cfg),
		"pdev->lldata.xtalk_cfg.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_histogram_bin_data(
		&(pdev->hist_data),
		"pdev->lldata.hist_data.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_xtalk_histogram_data(
		&(pdev->xtalk_shapes),
		"pdev->lldata.xtalk_shapes.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

	VL53L1_print_xtalk_range_results(
		&(pdev->xtalk_results),
		"pdev->lldata.xtalk_results.",
		VL53L1_TRACE_MODULE_XTALK_DATA);

#endif

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_get_and_avg_xtalk_samples(
		VL53L1_DEV	                  Dev,
		uint8_t                       num_of_samples,
		uint8_t                       measurement_mode,
		int16_t                       xtalk_filter_thresh_max_mm,
		int16_t                       xtalk_filter_thresh_min_mm,
		uint16_t                      xtalk_max_valid_rate_kcps,
		uint8_t                       xtalk_result_id,
		uint8_t                       xtalk_histo_id,
		VL53L1_xtalk_range_results_t *pxtalk_results,
		VL53L1_histogram_bin_data_t  *psum_histo,
		VL53L1_histogram_bin_data_t  *pavg_histo)
{











	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

#ifdef VL53L1_LOG_ENABLE
	VL53L1_LLDriverResults_t *pres =
		VL53L1DevStructGetLLResultsHandle(Dev);
#endif

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
			prange_data       = &(prange_results->VL53L1_PRM_00005[0]);



			if (prange_results->active_results > 1) {
				for (j = 1 ; j < prange_results->active_results ; j++) {

					if (prange_results->VL53L1_PRM_00005[j].median_range_mm <
						prange_data->median_range_mm){
							prange_data = &(prange_results->VL53L1_PRM_00005[j]);
					}
				}
			}

			pxtalk_range_data = &(pxtalk_results->VL53L1_PRM_00005[zone_id]);





			if ((prange_results->active_results > 0) &&
				(prange_data->median_range_mm <
						xtalk_filter_thresh_max_mm) &&
				(prange_data->median_range_mm >
						xtalk_filter_thresh_min_mm) &&
				(prange_data->VL53L1_PRM_00008 <
					(uint32_t)(xtalk_max_valid_rate_kcps * 16))) {

				pxtalk_range_data->no_of_samples++;

				pxtalk_range_data->rate_per_spad_kcps_sum +=
					prange_data->VL53L1_PRM_00008;

				pxtalk_range_data->signal_total_events_sum +=
					prange_data->VL53L1_PRM_00009;

				pxtalk_range_data->sigma_mm_sum +=
					prange_data->VL53L1_PRM_00004;








				if (zone_id >= 4) {
					status = VL53L1_sum_histogram_data(
							&(pdev->hist_data),
							psum_histo);





					if (prange_data->VL53L1_PRM_00010 <
							pxtalk_results->central_histogram__window_start) {
						pxtalk_results->central_histogram__window_start =
								prange_data->VL53L1_PRM_00010;
					}



					if (prange_data->VL53L1_PRM_00011 >
							pxtalk_results->central_histogram__window_end) {
						pxtalk_results->central_histogram__window_end =
								prange_data->VL53L1_PRM_00011;
					}

				}
			}
		}







#ifdef VL53L1_LOG_ENABLE
			if ((status == VL53L1_ERROR_NONE)) {
			VL53L1_print_range_results(
					&(pres->range_results),
					"pres->range_results.",
					VL53L1_TRACE_MODULE_CORE);
		}
#endif







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

		pxtalk_range_data = &(pxtalk_results->VL53L1_PRM_00005[i+xtalk_result_id]);

		if (pxtalk_range_data->no_of_samples > 0) {
			pxtalk_range_data->rate_per_spad_kcps_avg =
				pxtalk_range_data->rate_per_spad_kcps_sum /
				(uint32_t)pxtalk_range_data->no_of_samples;

			pxtalk_range_data->signal_total_events_avg =
						pxtalk_range_data->signal_total_events_sum /
						(int32_t)pxtalk_range_data->no_of_samples;

			pxtalk_range_data->sigma_mm_avg =
					pxtalk_range_data->sigma_mm_sum /
					(uint32_t)pxtalk_range_data->no_of_samples;

		} else {
			pxtalk_range_data->rate_per_spad_kcps_avg =
					pxtalk_range_data->rate_per_spad_kcps_sum;
			pxtalk_range_data->signal_total_events_avg =
					pxtalk_range_data->signal_total_events_sum;
			pxtalk_range_data->sigma_mm_avg =
					pxtalk_range_data->sigma_mm_sum;
		}
	}




	memcpy(pavg_histo, &(pdev->hist_data), sizeof(VL53L1_histogram_bin_data_t));




	if (status == VL53L1_ERROR_NONE) {

		pxtalk_range_data = &(pxtalk_results->VL53L1_PRM_00005[xtalk_histo_id]);

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

	SUPPRESS_UNUSED_WARNING(pxtalk_results);




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
			prange_data       = &(prange_results->VL53L1_PRM_00005[0]);





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
	VL53L1_DEV	                  Dev,
	uint16_t                      dss_config__target_total_rate_mcps,
	uint32_t                      phasecal_config_timeout_us,
	uint32_t                      range_config_timeout_us,
	uint8_t                       pre_num_of_samples,
	uint8_t                       mm1_num_of_samples,
	uint8_t                       mm2_num_of_samples,
	int16_t                       cal_distance_mm,
	uint16_t                      cal_reflectance_pc,
	VL53L1_Error                 *pcal_status)
{
























	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_DevicePresetModes device_preset_modes[VL53L1_MAX_OFFSET_RANGE_RESULTS];

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




	switch (pdev->offset_calibration_mode) {

	case VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM:
	case VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM_PRE_RANGE_ONLY:
		device_preset_modes[0] =
			VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING;
		device_preset_modes[1] =
			VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM1_CAL;
		device_preset_modes[2] =
			VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM2_CAL;
	break;

	default:
		device_preset_modes[0] =
			VL53L1_DEVICEPRESETMODE_STANDARD_RANGING;
		device_preset_modes[1] =
			VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM1_CAL;
		device_preset_modes[2] =
			VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM2_CAL;
	break;
	}



	num_of_samples[0] = pre_num_of_samples;
	num_of_samples[1] = mm1_num_of_samples;
	num_of_samples[2] = mm2_num_of_samples;




	switch (pdev->offset_calibration_mode) {

	case VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD_PRE_RANGE_ONLY:
	case VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM_PRE_RANGE_ONLY:


		pdev->offset_results.active_results  = 1;

	break;

	default:

		pdev->customer.mm_config__inner_offset_mm  = 0;
		pdev->customer.mm_config__outer_offset_mm  = 0;
		pdev->offset_results.active_results  =
			VL53L1_MAX_OFFSET_RANGE_RESULTS;

	break;
	}

	pdev->customer.algo__part_to_part_range_offset_mm = 0;




	pdev->offset_results.max_results           = VL53L1_MAX_OFFSET_RANGE_RESULTS;
	pdev->offset_results.cal_distance_mm       = cal_distance_mm;
	pdev->offset_results.cal_reflectance_pc    = cal_reflectance_pc;

	for (m = 0 ; m <  VL53L1_MAX_OFFSET_RANGE_RESULTS; m++) {

		poffset = &(pdev->offset_results.VL53L1_PRM_00005[m]);
		poffset->preset_mode         = 0;
		poffset->no_of_samples       = 0;
		poffset->effective_spads     = 0;
		poffset->peak_rate_mcps      = 0;
		poffset->VL53L1_PRM_00004            = 0;
		poffset->median_range_mm     = 0;
	}

	for (m = 0 ; m < pdev->offset_results.active_results ; m++) {

		poffset = &(pdev->offset_results.VL53L1_PRM_00005[m]);

		poffset->preset_mode         = device_preset_modes[m];




		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_set_preset_mode(
					Dev,
					device_preset_modes[m],
					dss_config__target_total_rate_mcps,
					phasecal_config_timeout_us,
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









			prange_data  = &(prange_results->VL53L1_PRM_00005[0]);

			if (prange_results->active_results > 0 &&
				prange_results->stream_count   > 1) {

				if (prange_data->range_status ==
						VL53L1_DEVICEERROR_RANGECOMPLETE) {

					poffset->no_of_samples++;
					poffset->effective_spads +=
						(uint32_t)prange_data->VL53L1_PRM_00003;
					poffset->peak_rate_mcps  +=
						(uint32_t)prange_data->peak_signal_count_rate_mcps;
					poffset->VL53L1_PRM_00004        +=
						(uint32_t)prange_data->VL53L1_PRM_00004;
					poffset->median_range_mm +=
						(int32_t)prange_data->median_range_mm;

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

			poffset->effective_spads += (poffset->no_of_samples/2);
			poffset->effective_spads /= poffset->no_of_samples;

			poffset->peak_rate_mcps  += (poffset->no_of_samples/2);
			poffset->peak_rate_mcps  /= poffset->no_of_samples;

			poffset->VL53L1_PRM_00004        += (poffset->no_of_samples/2);
			poffset->VL53L1_PRM_00004        /= poffset->no_of_samples;

			poffset->median_range_mm += (poffset->no_of_samples/2);
			poffset->median_range_mm /= poffset->no_of_samples;

			poffset->range_mm_offset  =  (int32_t)cal_distance_mm;
			poffset->range_mm_offset -= poffset->median_range_mm;



			if (poffset->preset_mode ==
				VL53L1_DEVICEPRESETMODE_STANDARD_RANGING)
				manual_effective_spads =
					(uint16_t)poffset->effective_spads;
		}
	}




	switch (pdev->offset_calibration_mode) {

	case VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD_PRE_RANGE_ONLY:
	case VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM_PRE_RANGE_ONLY:



		pdev->customer.mm_config__inner_offset_mm +=
			(int16_t)pdev->offset_results.VL53L1_PRM_00005[0].range_mm_offset;
		pdev->customer.mm_config__outer_offset_mm +=
			(int16_t)pdev->offset_results.VL53L1_PRM_00005[0].range_mm_offset;
	break;

	default:


		pdev->customer.mm_config__inner_offset_mm =
			(int16_t)pdev->offset_results.VL53L1_PRM_00005[1].range_mm_offset;
		pdev->customer.mm_config__outer_offset_mm =
			(int16_t)pdev->offset_results.VL53L1_PRM_00005[2].range_mm_offset;
		pdev->customer.algo__part_to_part_range_offset_mm = 0;





		pdev->add_off_cal_data.result__mm_inner_actual_effective_spads =
			(uint16_t)pdev->offset_results.VL53L1_PRM_00005[1].effective_spads;
		pdev->add_off_cal_data.result__mm_outer_actual_effective_spads =
			(uint16_t)pdev->offset_results.VL53L1_PRM_00005[2].effective_spads;

		pdev->add_off_cal_data.result__mm_inner_peak_signal_count_rtn_mcps =
			(uint16_t)pdev->offset_results.VL53L1_PRM_00005[1].peak_rate_mcps;
		pdev->add_off_cal_data.result__mm_outer_peak_signal_count_rtn_mcps =
			(uint16_t)pdev->offset_results.VL53L1_PRM_00005[2].peak_rate_mcps;

		break;
	}









	pdev->cust_dmax_cal.ref__actual_effective_spads =
		(uint16_t)pdev->offset_results.VL53L1_PRM_00005[0].effective_spads;
	pdev->cust_dmax_cal.ref__peak_signal_count_rate_mcps =
		(uint16_t)pdev->offset_results.VL53L1_PRM_00005[0].peak_rate_mcps;



	pdev->cust_dmax_cal.ref__distance_mm = cal_distance_mm * 16;

	pdev->cust_dmax_cal.ref_reflectance_pc = cal_reflectance_pc;
	pdev->cust_dmax_cal.coverglass_transmission = 0x0100;




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_customer_nvm_managed(
				Dev,
				&(pdev->customer));






	for (m = 0 ; m < pdev->offset_results.active_results ; m++) {

		poffset = &(pdev->offset_results.VL53L1_PRM_00005[m]);

		if (status == VL53L1_ERROR_NONE) {

			pdev->offset_results.cal_report = m;

			if (poffset->no_of_samples < num_of_samples[m])
				status = VL53L1_WARNING_OFFSET_CAL_MISSING_SAMPLES;






			if (m == 0 && poffset->VL53L1_PRM_00004 >
				VL53L1_OFFSET_CAL_MAX_SIGMA_MM)
				status = VL53L1_WARNING_OFFSET_CAL_SIGMA_TOO_HIGH;

			if (poffset->peak_rate_mcps >
				VL53L1_OFFSET_CAL_MAX_PRE_PEAK_RATE_MCPS)
				status = VL53L1_WARNING_OFFSET_CAL_RATE_TOO_HIGH;

			if (poffset->dss_config__manual_effective_spads_select <
				VL53L1_OFFSET_CAL_MIN_EFFECTIVE_SPADS)
				status = VL53L1_WARNING_OFFSET_CAL_SPAD_COUNT_TOO_LOW;

			if (poffset->dss_config__manual_effective_spads_select == 0)
				status = VL53L1_ERROR_OFFSET_CAL_NO_SPADS_ENABLED_FAIL;

			if (poffset->no_of_samples == 0)
				status = VL53L1_ERROR_OFFSET_CAL_NO_SAMPLE_FAIL;
		}
	}






	pdev->offset_results.cal_status = status;
	*pcal_status = pdev->offset_results.cal_status;




	IGNORE_STATUS(
		IGNORE_OFFSET_CAL_MISSING_SAMPLES,
		VL53L1_WARNING_OFFSET_CAL_MISSING_SAMPLES,
		status);

	IGNORE_STATUS(
		IGNORE_OFFSET_CAL_SIGMA_TOO_HIGH,
		VL53L1_WARNING_OFFSET_CAL_SIGMA_TOO_HIGH,
		status);

	IGNORE_STATUS(
		IGNORE_OFFSET_CAL_RATE_TOO_HIGH,
		VL53L1_WARNING_OFFSET_CAL_RATE_TOO_HIGH,
		status);

	IGNORE_STATUS(
		IGNORE_OFFSET_CAL_SPAD_COUNT_TOO_LOW,
		VL53L1_WARNING_OFFSET_CAL_SPAD_COUNT_TOO_LOW,
		status);

#ifdef VL53L1_LOG_ENABLE




	VL53L1_print_customer_nvm_managed(
		&(pdev->customer),
		"run_offset_calibration():pdev->lldata.customer.",
		VL53L1_TRACE_MODULE_OFFSET_DATA);

	VL53L1_print_dmax_calibration_data(
		&(pdev->fmt_dmax_cal),
		"run_offset_calibration():pdev->lldata.fmt_dmax_cal.",
		VL53L1_TRACE_MODULE_OFFSET_DATA);

	VL53L1_print_dmax_calibration_data(
		&(pdev->cust_dmax_cal),
		"run_offset_calibration():pdev->lldata.cust_dmax_cal.",
		VL53L1_TRACE_MODULE_OFFSET_DATA);

	VL53L1_print_additional_offset_cal_data(
		&(pdev->add_off_cal_data),
		"run_offset_calibration():pdev->lldata.add_off_cal_data.",
		VL53L1_TRACE_MODULE_OFFSET_DATA);

	VL53L1_print_offset_range_results(
		&(pdev->offset_results),
		"run_offset_calibration():pdev->lldata.offset_results.",
		VL53L1_TRACE_MODULE_OFFSET_DATA);
#endif

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_run_zone_calibration(
	VL53L1_DEV	                  Dev,
	VL53L1_DevicePresetModes      device_preset_mode,
	VL53L1_DeviceZonePreset       zone_preset,
	VL53L1_zone_config_t         *pzone_cfg,
	uint16_t                      dss_config__target_total_rate_mcps,
	uint32_t                      phasecal_config_timeout_us,
	uint32_t                      mm_config_timeout_us,
	uint32_t                      range_config_timeout_us,
	uint16_t                      num_of_samples,
	int16_t                       cal_distance_mm,
	uint16_t                      cal_reflectance_pc,
	VL53L1_Error                 *pcal_status)
{















	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_LLDriverResults_t *pres =
		VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_range_results_t          range_results;
	VL53L1_range_results_t         *prange_results = &range_results;
	VL53L1_range_data_t            *prange_data = NULL;
	VL53L1_zone_calibration_data_t *pzone_data  = NULL;

	uint16_t  i                      = 0;
	uint16_t  m                      = 0;

	uint8_t   z                      = 0;
	uint8_t   measurement_mode       =
		VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;

	VL53L1_OffsetCorrectionMode  offset_cor_mode =
			VL53L1_OFFSETCORRECTIONMODE__NONE;

	LOG_FUNCTION_START("");




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_preset_mode(
				Dev,
				device_preset_mode,
				dss_config__target_total_rate_mcps,
				phasecal_config_timeout_us,
				mm_config_timeout_us,
				range_config_timeout_us,
				100);




	if (zone_preset == VL53L1_DEVICEZONEPRESET_CUSTOM) {

		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_set_zone_config(
					Dev,
					pzone_cfg);

	} else if (zone_preset != VL53L1_DEVICEZONEPRESET_NONE) {

		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_set_zone_preset(
					Dev,
					zone_preset);
	}






	pres->zone_cal.preset_mode        = device_preset_mode;
	pres->zone_cal.zone_preset        = zone_preset;


	pres->zone_cal.cal_distance_mm    = cal_distance_mm * 16;
	pres->zone_cal.cal_reflectance_pc = cal_reflectance_pc;
	pres->zone_cal.max_zones          = VL53L1_MAX_USER_ZONES;
	pres->zone_cal.active_zones       = pdev->zone_cfg.active_zones + 1;

	for (i = 0 ; i < VL53L1_MAX_USER_ZONES ; i++) {
		pres->zone_cal.VL53L1_PRM_00005[i].no_of_samples   = 0;
		pres->zone_cal.VL53L1_PRM_00005[i].effective_spads = 0;
		pres->zone_cal.VL53L1_PRM_00005[i].peak_rate_mcps  = 0;
		pres->zone_cal.VL53L1_PRM_00005[i].VL53L1_PRM_00014    = 0;
		pres->zone_cal.VL53L1_PRM_00005[i].VL53L1_PRM_00004        = 0;
		pres->zone_cal.VL53L1_PRM_00005[i].median_range_mm = 0;
		pres->zone_cal.VL53L1_PRM_00005[i].range_mm_offset = 0;
    }

	pres->zone_cal.phasecal_result__reference_phase = 0;
	pres->zone_cal.zero_distance_phase              = 0;







	status =
		VL53L1_get_offset_correction_mode(
			Dev,
			&offset_cor_mode);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_offset_correction_mode(
				Dev,
				VL53L1_OFFSETCORRECTIONMODE__NONE);




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_init_and_start_range(
				Dev,
				measurement_mode,
				VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS);




	m = (num_of_samples + 2) * (uint16_t)pres->zone_cal.active_zones;

	for (i = 0 ; i <= m ; i++) {




		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_wait_for_range_completion(Dev);








		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_get_device_results(
					Dev,
					VL53L1_DEVICERESULTSLEVEL_FULL,
					prange_results);









		prange_data  = &(prange_results->VL53L1_PRM_00005[0]);

		if (prange_results->active_results > 0 &&
			i > (uint16_t)pres->zone_cal.active_zones) {

			if (prange_data->range_status ==
				VL53L1_DEVICEERROR_RANGECOMPLETE) {

				pres->zone_cal.phasecal_result__reference_phase =
						pdev->hist_data.phasecal_result__reference_phase;
				pres->zone_cal.zero_distance_phase =
						pdev->hist_data.zero_distance_phase;

				pzone_data = &(pres->zone_cal.VL53L1_PRM_00005[prange_results->zone_id]);
				pzone_data->no_of_samples++;
				pzone_data->effective_spads +=
					(uint32_t)prange_data->VL53L1_PRM_00003;
				pzone_data->peak_rate_mcps  +=
					(uint32_t)prange_data->peak_signal_count_rate_mcps;
				pzone_data->VL53L1_PRM_00014  +=
					(uint32_t)prange_data->VL53L1_PRM_00014;
				pzone_data->VL53L1_PRM_00004        +=
						(uint32_t)prange_data->VL53L1_PRM_00004;
				pzone_data->median_range_mm +=
					(int32_t)prange_data->median_range_mm;

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




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_offset_correction_mode(
				Dev,
				offset_cor_mode);




	if (status == VL53L1_ERROR_NONE) {

		for (z = 0 ; z < pres->zone_cal.active_zones ; z++) {

			pzone_data = &(pres->zone_cal.VL53L1_PRM_00005[z]);



			if (pzone_data->no_of_samples > 0) {

				pzone_data->effective_spads += (pzone_data->no_of_samples/2);
				pzone_data->effective_spads /=  pzone_data->no_of_samples;

				pzone_data->peak_rate_mcps  += (pzone_data->no_of_samples/2);
				pzone_data->peak_rate_mcps  /=  pzone_data->no_of_samples;

				pzone_data->VL53L1_PRM_00014    += (pzone_data->no_of_samples/2);
				pzone_data->VL53L1_PRM_00014    /=  pzone_data->no_of_samples;

				pzone_data->VL53L1_PRM_00004        += (pzone_data->no_of_samples/2);
				pzone_data->VL53L1_PRM_00004        /=  pzone_data->no_of_samples;



				pzone_data->median_range_mm *= 4;
				pzone_data->median_range_mm += (pzone_data->no_of_samples/2);
				pzone_data->median_range_mm /=  pzone_data->no_of_samples;

				pzone_data->range_mm_offset  = ((int32_t)cal_distance_mm) * 4;
				pzone_data->range_mm_offset -= pzone_data->median_range_mm;

				if (pzone_data->no_of_samples < num_of_samples)
					status = VL53L1_WARNING_ZONE_CAL_MISSING_SAMPLES;

				if (pzone_data->VL53L1_PRM_00004 >
					VL53L1_ZONE_CAL_MAX_SIGMA_MM)
					status = VL53L1_WARNING_ZONE_CAL_SIGMA_TOO_HIGH;

				if (pzone_data->peak_rate_mcps >
					VL53L1_ZONE_CAL_MAX_PRE_PEAK_RATE_MCPS)
					status = VL53L1_WARNING_ZONE_CAL_RATE_TOO_HIGH;

			} else {
				status = VL53L1_ERROR_ZONE_CAL_NO_SAMPLE_FAIL;
			}
		}
	}






	pres->zone_cal.cal_status = status;
	*pcal_status = pres->zone_cal.cal_status;




	IGNORE_STATUS(
		IGNORE_ZONE_CAL_MISSING_SAMPLES,
		VL53L1_WARNING_ZONE_CAL_MISSING_SAMPLES,
		status);

	IGNORE_STATUS(
		IGNORE_ZONE_CAL_SIGMA_TOO_HIGH,
		VL53L1_WARNING_ZONE_CAL_SIGMA_TOO_HIGH,
		status);

	IGNORE_STATUS(
		IGNORE_ZONE_CAL_RATE_TOO_HIGH,
		VL53L1_WARNING_ZONE_CAL_RATE_TOO_HIGH,
		status);













	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_run_spad_rate_map(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceTestMode      device_test_mode,
	VL53L1_DeviceSscArray      array_select,
	uint32_t                   ssc_config_timeout_us,
    VL53L1_spad_rate_data_t   *pspad_rate_data)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_enable_powerforce(Dev);






	if (status == VL53L1_ERROR_NONE) {
		pdev->ssc_cfg.array_select = array_select;
		pdev->ssc_cfg.timeout_us   = ssc_config_timeout_us;
		status =
			VL53L1_set_ssc_config(
				Dev,
				&(pdev->ssc_cfg),
				pdev->stat_nvm.osc_measured__fast_osc__frequency);
	}






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_run_device_test(
				Dev,
				device_test_mode);






    if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_get_spad_rate_data(
				Dev,
				pspad_rate_data);

	if (device_test_mode == VL53L1_DEVICETESTMODE_LCR_VCSEL_ON)
		pspad_rate_data->fractional_bits =  7;
	else
		pspad_rate_data->fractional_bits = 15;




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_disable_powerforce(Dev);

#ifdef VL53L1_LOG_ENABLE



    if (status == VL53L1_ERROR_NONE) {
		VL53L1_print_spad_rate_data(
			pspad_rate_data,
			"run_spad_rate_map():",
			VL53L1_TRACE_MODULE_SPAD_RATE_MAP);
		VL53L1_print_spad_rate_map(
			pspad_rate_data,
			"run_spad_rate_map():",
			VL53L1_TRACE_MODULE_SPAD_RATE_MAP);
    }
#endif

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_run_device_test(
	VL53L1_DEV             Dev,
	VL53L1_DeviceTestMode  device_test_mode)
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

	if (status == VL53L1_ERROR_NONE) {
		trace_print(
			VL53L1_TRACE_LEVEL_INFO,
			"    Device Test Complete:\n\t%-32s = %3u\n\t%-32s = %3u\n",
			"result__range_status",
			pdev->sys_results.result__range_status,
			"result__report_status",
			pdev->sys_results.result__report_status);





		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_clear_interrupt(Dev);
	}








	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_start_test(
				Dev,
				0x00);

	LOG_FUNCTION_END(status);

	return status;
}


