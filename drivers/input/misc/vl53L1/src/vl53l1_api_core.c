
/*******************************************************************************
 * Copyright (c) 2017, STMicroelectronics - All Rights Reserved

 This file is part of VL53L1 Core and is dual licensed,
 either 'STMicroelectronics
 Proprietary license'
 or 'BSD 3-clause "New" or "Revised" License' , at your option.

********************************************************************************

 'STMicroelectronics Proprietary license'

********************************************************************************

 License terms: STMicroelectronics Proprietary in accordance with licensing
 terms at www.st.com/sla0081

 STMicroelectronics confidential
 Reproduction and Communication of this document is strictly prohibited unless
 specifically authorized in writing by STMicroelectronics.


********************************************************************************

 Alternatively, VL53L1 Core may be distributed under the terms of
 'BSD 3-clause "New" or "Revised" License', in which case the following
 provisions apply instead of the ones
 mentioned above :

********************************************************************************

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


********************************************************************************

*/





































#include "vl53l1_ll_def.h"
#include "vl53l1_ll_device.h"
#include "vl53l1_platform.h"
#include "vl53l1_platform_ipp.h"
#include "vl53l1_register_map.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_register_funcs.h"
#include "vl53l1_hist_map.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_nvm_map.h"
#include "vl53l1_nvm_structs.h"
#include "vl53l1_nvm.h"
#include "vl53l1_core.h"
#include "vl53l1_wait.h"
#include "vl53l1_zone_presets.h"
#include "vl53l1_api_preset_modes.h"
#include "vl53l1_silicon_core.h"
#include "vl53l1_api_core.h"
#include "vl53l1_tuning_parm_defaults.h"

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

#define VL53L1_MAX_I2C_XFER_SIZE 256

VL53L1_Error VL53L1_get_version(
	VL53L1_DEV           Dev,
	VL53L1_ll_version_t *pdata)
{






	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_init_version(Dev);

	memcpy(pdata, &(pdev->version), sizeof(VL53L1_ll_version_t));

	return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_get_device_firmware_version(
	VL53L1_DEV        Dev,
	uint16_t         *pfw_version)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_disable_firmware(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_RdWord(
				Dev,
				VL53L1_MCU_GENERAL_PURPOSE__GP_0,
				pfw_version);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_enable_firmware(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_data_init(
	VL53L1_DEV        Dev,
	uint8_t           read_p2p_data)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t    *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);




	VL53L1_zone_objects_t    *pobjects;

	uint8_t  i = 0;

	LOG_FUNCTION_START("");

	VL53L1_init_ll_driver_state(
			Dev,
			VL53L1_DEVICESTATE_UNKNOWN);

	pres->range_results.max_results    = VL53L1_MAX_RANGE_RESULTS;
	pres->range_results.active_results = 0;
	pres->zone_results.max_zones       = VL53L1_MAX_USER_ZONES;
	pres->zone_results.active_zones    = 0;

	for (i = 0; i < VL53L1_MAX_USER_ZONES; i++) {
		pobjects = &(pres->zone_results.VL53L1_p_002[i]);
		pobjects->xmonitor.VL53L1_p_020 = 0;
		pobjects->xmonitor.VL53L1_p_021  = 0;
		pobjects->xmonitor.VL53L1_p_014          = 0;
		pobjects->xmonitor.range_status =
				VL53L1_DEVICEERROR_NOUPDATE;
	}




	pres->zone_hists.max_zones         = VL53L1_MAX_USER_ZONES;
	pres->zone_hists.active_zones      = 0;




	pres->zone_cal.max_zones           = VL53L1_MAX_USER_ZONES;
	pres->zone_cal.active_zones        = 0;
	for (i = 0; i < VL53L1_MAX_USER_ZONES; i++) {
		pres->zone_cal.VL53L1_p_002[i].no_of_samples   = 0;
		pres->zone_cal.VL53L1_p_002[i].effective_spads = 0;
		pres->zone_cal.VL53L1_p_002[i].peak_rate_mcps  = 0;
		pres->zone_cal.VL53L1_p_002[i].median_range_mm = 0;
		pres->zone_cal.VL53L1_p_002[i].range_mm_offset = 0;
	}

	pdev->wait_method             = VL53L1_WAIT_METHOD_BLOCKING;
	pdev->preset_mode   = VL53L1_DEVICEPRESETMODE_STANDARD_RANGING;
	pdev->zone_preset             = VL53L1_DEVICEZONEPRESET_NONE;
	pdev->measurement_mode        = VL53L1_DEVICEMEASUREMENTMODE_STOP;

	pdev->offset_calibration_mode =
		VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD;
	pdev->offset_correction_mode  =
		VL53L1_OFFSETCORRECTIONMODE__MM1_MM2_OFFSETS;
	pdev->dmax_mode  =
		VL53L1_DEVICEDMAXMODE__FMT_CAL_DATA;

	pdev->phasecal_config_timeout_us  =  1000;
	pdev->mm_config_timeout_us        =  2000;
	pdev->range_config_timeout_us     = 13000;
	pdev->inter_measurement_period_ms =   100;
	pdev->dss_config__target_total_rate_mcps = 0x0A00;
	pdev->debug_mode                  =  0x00;

	pdev->offset_results.max_results    = VL53L1_MAX_OFFSET_RANGE_RESULTS;
	pdev->offset_results.active_results = 0;





	pdev->gain_cal.standard_ranging_gain_factor =
			VL53L1_TUNINGPARM_LITE_RANGING_GAIN_FACTOR_DEFAULT;
	pdev->gain_cal.histogram_ranging_gain_factor =
			VL53L1_TUNINGPARM_HIST_GAIN_FACTOR_DEFAULT;





	VL53L1_init_version(Dev);










	if (read_p2p_data > 0 && status == VL53L1_ERROR_NONE)

		status = VL53L1_read_p2p_data(Dev);



	status =
		VL53L1_init_refspadchar_config_struct(
			&(pdev->refspadchar));



	status =
		VL53L1_init_ssc_config_struct(
			&(pdev->ssc_cfg));





	status =
		VL53L1_init_xtalk_config_struct(
			&(pdev->customer),
			&(pdev->xtalk_cfg));




	status =
		VL53L1_init_xtalk_extract_config_struct(
			&(pdev->xtalk_extract_cfg));




	status =
		VL53L1_init_offset_cal_config_struct(
		    &(pdev->offsetcal_cfg));




	status = VL53L1_init_zone_cal_config_struct(
			&(pdev->zonecal_cfg));



	status =
		VL53L1_init_hist_post_process_config_struct(
			pdev->xtalk_cfg.global_crosstalk_compensation_enable,
			&(pdev->histpostprocess));



	status =
		VL53L1_init_hist_gen3_dmax_config_struct(
			&(pdev->dmax_cfg));





	status =
		VL53L1_init_tuning_parm_storage_struct(
			&(pdev->tuning_parms));







	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_set_preset_mode(
			Dev,
			pdev->preset_mode,
			pdev->dss_config__target_total_rate_mcps,

			pdev->phasecal_config_timeout_us,
			pdev->mm_config_timeout_us,
			pdev->range_config_timeout_us,
			pdev->inter_measurement_period_ms);



	VL53L1_init_histogram_bin_data_struct(
			0,
			VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->hist_data));

	VL53L1_init_histogram_bin_data_struct(
			0,
			VL53L1_HISTOGRAM_BUFFER_SIZE,
			&(pdev->hist_xtalk));



	VL53L1_init_xtalk_bin_data_struct(
			0,
			VL53L1_XTALK_HISTO_BINS,
			&(pdev->xtalk_shapes.xtalk_shape));





	VL53L1_xtalk_cal_data_init(
			Dev
			);





	VL53L1_dynamic_xtalk_correction_data_init(
			Dev
			);





	VL53L1_low_power_auto_data_init(
			Dev
			);

#ifdef VL53L1_LOG_ENABLE




	VL53L1_print_static_nvm_managed(
		&(pdev->stat_nvm),
		"data_init():pdev->lldata.stat_nvm.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_customer_nvm_managed(
		&(pdev->customer),
		"data_init():pdev->lldata.customer.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_nvm_copy_data(
		&(pdev->nvm_copy_data),
		"data_init():pdev->lldata.nvm_copy_data.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_dmax_calibration_data(
		&(pdev->fmt_dmax_cal),
		"data_init():pdev->lldata.fmt_dmax_cal.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_dmax_calibration_data(
		&(pdev->cust_dmax_cal),
		"data_init():pdev->lldata.cust_dmax_cal.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_additional_offset_cal_data(
		&(pdev->add_off_cal_data),
		"data_init():pdev->lldata.add_off_cal_data.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_user_zone(
		&(pdev->mm_roi),
		"data_init():pdev->lldata.mm_roi.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_optical_centre(
		&(pdev->optical_centre),
		"data_init():pdev->lldata.optical_centre.",
		VL53L1_TRACE_MODULE_DATA_INIT);

	VL53L1_print_cal_peak_rate_map(
		&(pdev->cal_peak_rate_map),
		"data_init():pdev->lldata.cal_peak_rate_map.",
		VL53L1_TRACE_MODULE_DATA_INIT);

#endif

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_read_p2p_data(
	VL53L1_DEV        Dev)
{










	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_customer_nvm_managed_t *pN = &(pdev->customer);
	VL53L1_additional_offset_cal_data_t *pCD = &(pdev->add_off_cal_data);

	VL53L1_decoded_nvm_fmt_range_data_t fmt_rrd;


	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_get_static_nvm_managed(
						Dev,
						&(pdev->stat_nvm));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_get_customer_nvm_managed(
						Dev,
						&(pdev->customer));

	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_get_nvm_copy_data(
						Dev,
						&(pdev->nvm_copy_data));



		if (status == VL53L1_ERROR_NONE)
			VL53L1_copy_rtn_good_spads_to_buffer(
					&(pdev->nvm_copy_data),
					&(pdev->rtn_good_spads[0]));
	}




	if (status == VL53L1_ERROR_NONE) {
		pHP->algo__crosstalk_compensation_plane_offset_kcps =
		pN->algo__crosstalk_compensation_plane_offset_kcps;
		pHP->algo__crosstalk_compensation_x_plane_gradient_kcps =
		pN->algo__crosstalk_compensation_x_plane_gradient_kcps;
		pHP->algo__crosstalk_compensation_y_plane_gradient_kcps =
		pN->algo__crosstalk_compensation_y_plane_gradient_kcps;
	}





	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_read_nvm_optical_centre(
				Dev,
				&(pdev->optical_centre));






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_read_nvm_cal_peak_rate_map(
				Dev,
				&(pdev->cal_peak_rate_map));






	if (status == VL53L1_ERROR_NONE) {

		status =
			VL53L1_read_nvm_additional_offset_cal_data(
				Dev,
				&(pdev->add_off_cal_data));






		if (pCD->result__mm_inner_peak_signal_count_rtn_mcps == 0 &&
			pCD->result__mm_outer_peak_signal_count_rtn_mcps == 0) {

			pCD->result__mm_inner_peak_signal_count_rtn_mcps
					= 0x0080;

			pCD->result__mm_outer_peak_signal_count_rtn_mcps
					= 0x0180;





			VL53L1_calc_mm_effective_spads(
			pdev->nvm_copy_data.roi_config__mode_roi_centre_spad,
			pdev->nvm_copy_data.roi_config__mode_roi_xy_size,
			0xC7,

			0xFF,
			&(pdev->rtn_good_spads[0]),
			VL53L1_RTN_SPAD_APERTURE_TRANSMISSION,
			&(pCD->result__mm_inner_actual_effective_spads),
			&(pCD->result__mm_outer_actual_effective_spads));
		}
	}





	if (status == VL53L1_ERROR_NONE) {

		status =
			VL53L1_read_nvm_fmt_range_results_data(
				Dev,
				VL53L1_NVM__FMT__RANGE_RESULTS__140MM_DARK,
				&fmt_rrd);

		if (status == VL53L1_ERROR_NONE) {
			pdev->fmt_dmax_cal.ref__actual_effective_spads =
			fmt_rrd.result__actual_effective_rtn_spads;
			pdev->fmt_dmax_cal.ref__peak_signal_count_rate_mcps =
			fmt_rrd.result__peak_signal_count_rate_rtn_mcps;
			pdev->fmt_dmax_cal.ref__distance_mm =
			fmt_rrd.measured_distance_mm;






			if (pdev->cal_peak_rate_map.cal_reflectance_pc != 0) {
				pdev->fmt_dmax_cal.ref_reflectance_pc =
				pdev->cal_peak_rate_map.cal_reflectance_pc;
			} else {
				pdev->fmt_dmax_cal.ref_reflectance_pc = 0x0014;
			}



			pdev->fmt_dmax_cal.coverglass_transmission = 0x0100;
		}
	}






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_RdWord(
				Dev,
				VL53L1_RESULT__OSC_CALIBRATE_VAL,
				&(pdev->dbg_results.result__osc_calibrate_val));






	if (pdev->stat_nvm.osc_measured__fast_osc__frequency < 0x1000) {
		trace_print(
			VL53L1_TRACE_LEVEL_WARNING,
			"\nInvalid %s value (0x%04X) - forcing to 0x%04X\n\n",
			"pdev->stat_nvm.osc_measured__fast_osc__frequency",
			pdev->stat_nvm.osc_measured__fast_osc__frequency,
			0xBCCC);
		pdev->stat_nvm.osc_measured__fast_osc__frequency = 0xBCCC;
	}






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_get_mode_mitigation_roi(
				Dev,
				&(pdev->mm_roi));






	if (pdev->optical_centre.x_centre == 0 &&
		pdev->optical_centre.y_centre == 0) {
		pdev->optical_centre.x_centre =
				pdev->mm_roi.x_centre << 4;
		pdev->optical_centre.y_centre =
				pdev->mm_roi.y_centre << 4;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_software_reset(
	VL53L1_DEV    Dev)
{






	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");



	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_WrByte(
						Dev,
						VL53L1_SOFT_RESET,
						0x00);



	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WaitUs(
				Dev,
				VL53L1_SOFTWARE_RESET_DURATION_US);



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
						Dev,
						VL53L1_SOFT_RESET,
						0x01);



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_wait_for_boot_completion(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_part_to_part_data(
	VL53L1_DEV                            Dev,
	VL53L1_calibration_data_t            *pcal_data)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_xtalk_config_t *pC = &(pdev->xtalk_cfg);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_customer_nvm_managed_t *pN = &(pdev->customer);

	uint32_t tempu32;

	LOG_FUNCTION_START("");

	if (pcal_data->struct_version !=
		VL53L1_LL_CALIBRATION_DATA_STRUCT_VERSION) {
		status = VL53L1_ERROR_INVALID_PARAMS;
	}

	if (status == VL53L1_ERROR_NONE) {



		memcpy(
			&(pdev->customer),
			&(pcal_data->customer),
			sizeof(VL53L1_customer_nvm_managed_t));



		memcpy(
			&(pdev->add_off_cal_data),
			&(pcal_data->add_off_cal_data),
			sizeof(VL53L1_additional_offset_cal_data_t));



		memcpy(
			&(pdev->fmt_dmax_cal),
			&(pcal_data->fmt_dmax_cal),
			sizeof(VL53L1_dmax_calibration_data_t));



		memcpy(
			&(pdev->cust_dmax_cal),
			&(pcal_data->cust_dmax_cal),
			sizeof(VL53L1_dmax_calibration_data_t));



		memcpy(
			&(pdev->xtalk_shapes),
			&(pcal_data->xtalkhisto),
			sizeof(VL53L1_xtalk_histogram_data_t));



		memcpy(
			&(pdev->gain_cal),
			&(pcal_data->gain_cal),
			sizeof(VL53L1_gain_calibration_data_t));



		memcpy(
			&(pdev->cal_peak_rate_map),
			&(pcal_data->cal_peak_rate_map),
			sizeof(VL53L1_cal_peak_rate_map_t));






		pC->algo__crosstalk_compensation_plane_offset_kcps =
			pN->algo__crosstalk_compensation_plane_offset_kcps;
		pC->algo__crosstalk_compensation_x_plane_gradient_kcps =
			pN->algo__crosstalk_compensation_x_plane_gradient_kcps;
		pC->algo__crosstalk_compensation_y_plane_gradient_kcps =
			pN->algo__crosstalk_compensation_y_plane_gradient_kcps;

		pHP->algo__crosstalk_compensation_plane_offset_kcps =
			VL53L1_calc_crosstalk_plane_offset_with_margin(
			pC->algo__crosstalk_compensation_plane_offset_kcps,
			pC->histogram_mode_crosstalk_margin_kcps);

		pHP->algo__crosstalk_compensation_x_plane_gradient_kcps =
			pC->algo__crosstalk_compensation_x_plane_gradient_kcps;
		pHP->algo__crosstalk_compensation_y_plane_gradient_kcps =
			pC->algo__crosstalk_compensation_y_plane_gradient_kcps;




		if (pC->global_crosstalk_compensation_enable == 0x00) {
			pN->algo__crosstalk_compensation_plane_offset_kcps =
				0x00;
			pN->algo__crosstalk_compensation_x_plane_gradient_kcps =
				0x00;
			pN->algo__crosstalk_compensation_y_plane_gradient_kcps =
				0x00;
		} else {
			tempu32 =
			VL53L1_calc_crosstalk_plane_offset_with_margin(
			pC->algo__crosstalk_compensation_plane_offset_kcps,
			pC->lite_mode_crosstalk_margin_kcps);



			if (tempu32 > 0xFFFF)
				tempu32 = 0xFFFF;

			pN->algo__crosstalk_compensation_plane_offset_kcps =
				(uint16_t)tempu32;
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_part_to_part_data(
	VL53L1_DEV                      Dev,
	VL53L1_calibration_data_t      *pcal_data)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_xtalk_config_t *pC = &(pdev->xtalk_cfg);
	VL53L1_customer_nvm_managed_t *pCN = &(pcal_data->customer);

	LOG_FUNCTION_START("");

	pcal_data->struct_version =
			VL53L1_LL_CALIBRATION_DATA_STRUCT_VERSION;



	memcpy(
		&(pcal_data->customer),
		&(pdev->customer),
		sizeof(VL53L1_customer_nvm_managed_t));






	if (pC->algo__crosstalk_compensation_plane_offset_kcps > 0xFFFF) {
		pCN->algo__crosstalk_compensation_plane_offset_kcps =
			0xFFFF;
	} else {
		pCN->algo__crosstalk_compensation_plane_offset_kcps =
		(uint16_t)pC->algo__crosstalk_compensation_plane_offset_kcps;
	}
	pCN->algo__crosstalk_compensation_x_plane_gradient_kcps =
		pC->algo__crosstalk_compensation_x_plane_gradient_kcps;
	pCN->algo__crosstalk_compensation_y_plane_gradient_kcps =
		pC->algo__crosstalk_compensation_y_plane_gradient_kcps;



	memcpy(
		&(pcal_data->fmt_dmax_cal),
		&(pdev->fmt_dmax_cal),
		sizeof(VL53L1_dmax_calibration_data_t));



	memcpy(
		&(pcal_data->cust_dmax_cal),
		&(pdev->cust_dmax_cal),
		sizeof(VL53L1_dmax_calibration_data_t));



	memcpy(
		&(pcal_data->add_off_cal_data),
		&(pdev->add_off_cal_data),
		sizeof(VL53L1_additional_offset_cal_data_t));



	memcpy(
		&(pcal_data->optical_centre),
		&(pdev->optical_centre),
		sizeof(VL53L1_optical_centre_t));



	memcpy(
		&(pcal_data->xtalkhisto),
		&(pdev->xtalk_shapes),
		sizeof(VL53L1_xtalk_histogram_data_t));



	memcpy(
		&(pcal_data->gain_cal),
		&(pdev->gain_cal),
		sizeof(VL53L1_gain_calibration_data_t));



	memcpy(
		&(pcal_data->cal_peak_rate_map),
		&(pdev->cal_peak_rate_map),
		sizeof(VL53L1_cal_peak_rate_map_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_inter_measurement_period_ms(
	VL53L1_DEV              Dev,
	uint32_t                inter_measurement_period_ms)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->dbg_results.result__osc_calibrate_val == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE) {
		pdev->inter_measurement_period_ms = inter_measurement_period_ms;
		pdev->tim_cfg.system__intermeasurement_period =
			inter_measurement_period_ms *
			(uint32_t)pdev->dbg_results.result__osc_calibrate_val;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_inter_measurement_period_ms(
	VL53L1_DEV              Dev,
	uint32_t               *pinter_measurement_period_ms)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->dbg_results.result__osc_calibrate_val == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE)
		*pinter_measurement_period_ms =
			pdev->tim_cfg.system__intermeasurement_period /
			(uint32_t)pdev->dbg_results.result__osc_calibrate_val;


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_timeouts_us(
	VL53L1_DEV          Dev,
	uint32_t            phasecal_config_timeout_us,
	uint32_t            mm_config_timeout_us,
	uint32_t            range_config_timeout_us)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->stat_nvm.osc_measured__fast_osc__frequency == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE) {

		pdev->phasecal_config_timeout_us = phasecal_config_timeout_us;
		pdev->mm_config_timeout_us       = mm_config_timeout_us;
		pdev->range_config_timeout_us    = range_config_timeout_us;

		status =
		VL53L1_calc_timeout_register_values(
			phasecal_config_timeout_us,
			mm_config_timeout_us,
			range_config_timeout_us,
			pdev->stat_nvm.osc_measured__fast_osc__frequency,
			&(pdev->gen_cfg),
			&(pdev->tim_cfg));
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_timeouts_us(
	VL53L1_DEV           Dev,
	uint32_t            *pphasecal_config_timeout_us,
	uint32_t            *pmm_config_timeout_us,
	uint32_t			*prange_config_timeout_us)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);

	uint32_t  macro_period_us = 0;
	uint16_t  timeout_encoded = 0;

	LOG_FUNCTION_START("");

	if (pdev->stat_nvm.osc_measured__fast_osc__frequency == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE) {



		macro_period_us =
			VL53L1_calc_macro_period_us(
			pdev->stat_nvm.osc_measured__fast_osc__frequency,
			pdev->tim_cfg.range_config__vcsel_period_a);




		*pphasecal_config_timeout_us =
			VL53L1_calc_timeout_us(
			(uint32_t)pdev->gen_cfg.phasecal_config__timeout_macrop,
			macro_period_us);




		timeout_encoded =
			(uint16_t)pdev->tim_cfg.mm_config__timeout_macrop_a_hi;
		timeout_encoded = (timeout_encoded << 8) +
			(uint16_t)pdev->tim_cfg.mm_config__timeout_macrop_a_lo;

		*pmm_config_timeout_us =
			VL53L1_calc_decoded_timeout_us(
				timeout_encoded,
				macro_period_us);




		timeout_encoded =
		(uint16_t)pdev->tim_cfg.range_config__timeout_macrop_a_hi;
		timeout_encoded = (timeout_encoded << 8) +
		(uint16_t)pdev->tim_cfg.range_config__timeout_macrop_a_lo;

		*prange_config_timeout_us =
			VL53L1_calc_decoded_timeout_us(
				timeout_encoded,
				macro_period_us);

		pdev->phasecal_config_timeout_us = *pphasecal_config_timeout_us;
		pdev->mm_config_timeout_us       = *pmm_config_timeout_us;
		pdev->range_config_timeout_us    = *prange_config_timeout_us;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_calibration_repeat_period(
	VL53L1_DEV          Dev,
	uint16_t            cal_config__repeat_period)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->gen_cfg.cal_config__repeat_rate = cal_config__repeat_period;

	return status;

}


VL53L1_Error VL53L1_get_calibration_repeat_period(
	VL53L1_DEV          Dev,
	uint16_t           *pcal_config__repeat_period)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	*pcal_config__repeat_period = pdev->gen_cfg.cal_config__repeat_rate;

	return status;

}


VL53L1_Error VL53L1_set_sequence_config_bit(
	VL53L1_DEV                    Dev,
	VL53L1_DeviceSequenceConfig   bit_id,
	uint8_t                       value)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  bit_mask        = 0x01;
	uint8_t  clr_mask        = 0xFF  - bit_mask;
	uint8_t  bit_value       = value & bit_mask;

	if (bit_id <= VL53L1_DEVICESEQUENCECONFIG_RANGE) {

		if (bit_id > 0) {
			bit_mask  = 0x01 << bit_id;
			bit_value = bit_value << bit_id;
			clr_mask  = 0xFF  - bit_mask;
		}

		pdev->dyn_cfg.system__sequence_config =
			(pdev->dyn_cfg.system__sequence_config & clr_mask) |
			bit_value;

	} else {
		status = VL53L1_ERROR_INVALID_PARAMS;
	}

	return status;

}


VL53L1_Error VL53L1_get_sequence_config_bit(
	VL53L1_DEV                    Dev,
	VL53L1_DeviceSequenceConfig   bit_id,
	uint8_t                      *pvalue)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  bit_mask        = 0x01;

	if (bit_id <= VL53L1_DEVICESEQUENCECONFIG_RANGE) {

		if (bit_id > 0)
			bit_mask  = 0x01 << bit_id;

		*pvalue =
			pdev->dyn_cfg.system__sequence_config & bit_mask;

		if (bit_id > 0)
			*pvalue  = *pvalue >> bit_id;

	} else {
		status = VL53L1_ERROR_INVALID_PARAMS;
	}

	return status;
}


VL53L1_Error VL53L1_set_interrupt_polarity(
	VL53L1_DEV                      Dev,
	VL53L1_DeviceInterruptPolarity  interrupt_polarity)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->stat_cfg.gpio_hv_mux__ctrl =
			(pdev->stat_cfg.gpio_hv_mux__ctrl &
			VL53L1_DEVICEINTERRUPTPOLARITY_CLEAR_MASK) |
			(interrupt_polarity &
			VL53L1_DEVICEINTERRUPTPOLARITY_BIT_MASK);

	return status;

}


VL53L1_Error VL53L1_set_refspadchar_config_struct(
	VL53L1_DEV                     Dev,
	VL53L1_refspadchar_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->refspadchar.device_test_mode = pdata->device_test_mode;
	pdev->refspadchar.VL53L1_p_009     = pdata->VL53L1_p_009;
	pdev->refspadchar.timeout_us       = pdata->timeout_us;
	pdev->refspadchar.target_count_rate_mcps    =
			pdata->target_count_rate_mcps;
	pdev->refspadchar.min_count_rate_limit_mcps =
			pdata->min_count_rate_limit_mcps;
	pdev->refspadchar.max_count_rate_limit_mcps =
			pdata->max_count_rate_limit_mcps;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_get_refspadchar_config_struct(
	VL53L1_DEV                     Dev,
	VL53L1_refspadchar_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdata->device_test_mode       = pdev->refspadchar.device_test_mode;
	pdata->VL53L1_p_009           = pdev->refspadchar.VL53L1_p_009;
	pdata->timeout_us             = pdev->refspadchar.timeout_us;
	pdata->target_count_rate_mcps =
			pdev->refspadchar.target_count_rate_mcps;
	pdata->min_count_rate_limit_mcps =
			pdev->refspadchar.min_count_rate_limit_mcps;
	pdata->max_count_rate_limit_mcps =
			pdev->refspadchar.max_count_rate_limit_mcps;

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_set_range_ignore_threshold(
	VL53L1_DEV              Dev,
	uint8_t                 range_ignore_thresh_mult,
	uint16_t                range_ignore_threshold_mcps)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->xtalk_cfg.crosstalk_range_ignore_threshold_rate_mcps =
		range_ignore_threshold_mcps;

	pdev->xtalk_cfg.crosstalk_range_ignore_threshold_mult =
		range_ignore_thresh_mult;

	return status;

}

VL53L1_Error VL53L1_get_range_ignore_threshold(
	VL53L1_DEV              Dev,
	uint8_t                *prange_ignore_thresh_mult,
	uint16_t               *prange_ignore_threshold_mcps_internal,
	uint16_t               *prange_ignore_threshold_mcps_current)
{










	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	*prange_ignore_thresh_mult =
		pdev->xtalk_cfg.crosstalk_range_ignore_threshold_mult;

	*prange_ignore_threshold_mcps_current =
		pdev->stat_cfg.algo__range_ignore_threshold_mcps;

	*prange_ignore_threshold_mcps_internal =
		pdev->xtalk_cfg.crosstalk_range_ignore_threshold_rate_mcps;

	return status;

}



VL53L1_Error VL53L1_get_interrupt_polarity(
	VL53L1_DEV                       Dev,
	VL53L1_DeviceInterruptPolarity  *pinterrupt_polarity)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	*pinterrupt_polarity =
		pdev->stat_cfg.gpio_hv_mux__ctrl &
		VL53L1_DEVICEINTERRUPTPOLARITY_BIT_MASK;

	return status;

}


VL53L1_Error VL53L1_set_user_zone(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *puser_zone)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	VL53L1_encode_row_col(
		puser_zone->y_centre,
		puser_zone->x_centre,
		&(pdev->dyn_cfg.roi_config__user_roi_centre_spad));



	VL53L1_encode_zone_size(
		puser_zone->width,
		puser_zone->height,
		&(pdev->dyn_cfg.roi_config__user_roi_requested_global_xy_size));




	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_user_zone(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *puser_zone)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	VL53L1_decode_row_col(
			pdev->dyn_cfg.roi_config__user_roi_centre_spad,
			&(puser_zone->y_centre),
			&(puser_zone->x_centre));



	VL53L1_decode_zone_size(
		pdev->dyn_cfg.roi_config__user_roi_requested_global_xy_size,
		&(puser_zone->width),
		&(puser_zone->height));

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_get_mode_mitigation_roi(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *pmm_roi)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  x       = 0;
	uint8_t  y       = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");



	VL53L1_decode_row_col(
			pdev->nvm_copy_data.roi_config__mode_roi_centre_spad,
			&y,
			&x);

	pmm_roi->x_centre = x;
	pmm_roi->y_centre = y;










	xy_size = pdev->nvm_copy_data.roi_config__mode_roi_xy_size;

	pmm_roi->height = xy_size >> 4;
	pmm_roi->width  = xy_size & 0x0F;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_zone_config(
	VL53L1_DEV                 Dev,
	VL53L1_zone_config_t      *pzone_cfg)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(&(pdev->zone_cfg.user_zones), &(pzone_cfg->user_zones),
			sizeof(pdev->zone_cfg.user_zones));



	pdev->zone_cfg.max_zones    = pzone_cfg->max_zones;
	pdev->zone_cfg.active_zones = pzone_cfg->active_zones;

	status = VL53L1_init_zone_config_histogram_bins(&pdev->zone_cfg);










	if (pzone_cfg->active_zones == 0)
		pdev->gen_cfg.global_config__stream_divider = 0;
	else if (pzone_cfg->active_zones < VL53L1_MAX_USER_ZONES)
		pdev->gen_cfg.global_config__stream_divider =
				pzone_cfg->active_zones + 1;
	else
		pdev->gen_cfg.global_config__stream_divider =
				VL53L1_MAX_USER_ZONES + 1;

	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_get_zone_config(
	VL53L1_DEV                 Dev,
	VL53L1_zone_config_t      *pzone_cfg)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(pzone_cfg, &(pdev->zone_cfg), sizeof(VL53L1_zone_config_t));

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_get_preset_mode_timing_cfg(
	VL53L1_DEV                   Dev,
	VL53L1_DevicePresetModes     device_preset_mode,
	uint16_t                    *pdss_config__target_total_rate_mcps,
	uint32_t                    *pphasecal_config_timeout_us,
	uint32_t                    *pmm_config_timeout_us,
	uint32_t                    *prange_config_timeout_us)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");


	switch (device_preset_mode) {

	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING:
	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_SHORT_RANGE:
	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_LONG_RANGE:
	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM1_CAL:
	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM2_CAL:
	case VL53L1_DEVICEPRESETMODE_OLT:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_lite_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_lite_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_lite_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_lite_us;
	break;

	case VL53L1_DEVICEPRESETMODE_TIMED_RANGING:
	case VL53L1_DEVICEPRESETMODE_TIMED_RANGING_SHORT_RANGE:
	case VL53L1_DEVICEPRESETMODE_TIMED_RANGING_LONG_RANGE:
	case VL53L1_DEVICEPRESETMODE_SINGLESHOT_RANGING:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_timed_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_timed_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_timed_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_timed_us;
	break;

	case VL53L1_DEVICEPRESETMODE_LOWPOWERAUTO_SHORT_RANGE:
	case VL53L1_DEVICEPRESETMODE_LOWPOWERAUTO_MEDIUM_RANGE:
	case VL53L1_DEVICEPRESETMODE_LOWPOWERAUTO_LONG_RANGE:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_timed_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_timed_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_lpa_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_lpa_us;
	break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_WITH_MM1:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_WITH_MM2:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM1_CAL:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM2_CAL:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_REF_ARRAY:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE_MM1:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE_MM2:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_CHARACTERISATION:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_histo_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_hist_long_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_histo_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_histo_us;

	break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_histo_mz_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_mz_med_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_mz_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_mz_us;
	break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_SHORT_RANGE:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_histo_mz_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_mz_short_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_mz_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_mz_us;
	break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_LONG_RANGE:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_histo_mz_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_mz_long_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_mz_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_mz_us;
	break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_SHORT_TIMING:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_histo_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_hist_short_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_histo_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_histo_us;
	break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE_MM1:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE_MM2:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_histo_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_hist_med_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_histo_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_histo_us;
	break;


	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE_MM1:
	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE_MM2:
		*pdss_config__target_total_rate_mcps =
				pdev->tuning_parms.tp_dss_target_histo_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_hist_short_us;
		*pmm_config_timeout_us =
				pdev->tuning_parms.tp_mm_timeout_histo_us;
		*prange_config_timeout_us =
				pdev->tuning_parms.tp_range_timeout_histo_us;
	break;

	case VL53L1_DEVICEPRESETMODE_SPECIAL_HISTOGRAM_SHORT_RANGE:
		*pdss_config__target_total_rate_mcps =
			pdev->tuning_parms.tp_dss_target_very_short_mcps;
		*pphasecal_config_timeout_us =
			pdev->tuning_parms.tp_phasecal_timeout_hist_short_us;
		*pmm_config_timeout_us =
			pdev->tuning_parms.tp_mm_timeout_histo_us;
		*prange_config_timeout_us =
			pdev->tuning_parms.tp_range_timeout_histo_us;
	break;

	default:
		status = VL53L1_ERROR_INVALID_PARAMS;
		break;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_preset_mode(
	VL53L1_DEV                   Dev,
	VL53L1_DevicePresetModes     device_preset_mode,
	uint16_t                     dss_config__target_total_rate_mcps,
	uint32_t                     phasecal_config_timeout_us,
	uint32_t                     mm_config_timeout_us,
	uint32_t                     range_config_timeout_us,
	uint32_t                     inter_measurement_period_ms)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_hist_post_process_config_t *phistpostprocess =
			&(pdev->histpostprocess);

	VL53L1_static_config_t        *pstatic       = &(pdev->stat_cfg);
	VL53L1_histogram_config_t     *phistogram    = &(pdev->hist_cfg);
	VL53L1_general_config_t       *pgeneral      = &(pdev->gen_cfg);
	VL53L1_timing_config_t        *ptiming       = &(pdev->tim_cfg);
	VL53L1_dynamic_config_t       *pdynamic      = &(pdev->dyn_cfg);
	VL53L1_system_control_t       *psystem       = &(pdev->sys_ctrl);
	VL53L1_zone_config_t          *pzone_cfg     = &(pdev->zone_cfg);
	VL53L1_tuning_parm_storage_t  *ptuning_parms = &(pdev->tuning_parms);
	VL53L1_low_power_auto_data_t  *plpadata      =
					&(pdev->low_power_auto_data);

	LOG_FUNCTION_START("");



	pdev->preset_mode                 = device_preset_mode;
	pdev->mm_config_timeout_us        = mm_config_timeout_us;
	pdev->range_config_timeout_us     = range_config_timeout_us;
	pdev->inter_measurement_period_ms = inter_measurement_period_ms;




	VL53L1_init_ll_driver_state(
			Dev,
			VL53L1_DEVICESTATE_SW_STANDBY);




	switch (device_preset_mode) {

	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING:
		status = VL53L1_preset_mode_standard_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_SHORT_RANGE:
		status = VL53L1_preset_mode_standard_ranging_short_range(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_LONG_RANGE:
		status = VL53L1_preset_mode_standard_ranging_long_range(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM1_CAL:
		status = VL53L1_preset_mode_standard_ranging_mm1_cal(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM2_CAL:
		status = VL53L1_preset_mode_standard_ranging_mm2_cal(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_TIMED_RANGING:
		status = VL53L1_preset_mode_timed_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_TIMED_RANGING_SHORT_RANGE:
		status = VL53L1_preset_mode_timed_ranging_short_range(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_TIMED_RANGING_LONG_RANGE:
		status = VL53L1_preset_mode_timed_ranging_long_range(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING:
		status = VL53L1_preset_mode_histogram_ranging(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_WITH_MM1:
		status = VL53L1_preset_mode_histogram_ranging_with_mm1(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_WITH_MM2:
		status = VL53L1_preset_mode_histogram_ranging_with_mm2(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM1_CAL:
		status = VL53L1_preset_mode_histogram_ranging_mm1_cal(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM2_CAL:
		status = VL53L1_preset_mode_histogram_ranging_mm2_cal(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE:
		status = VL53L1_preset_mode_histogram_multizone(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_SHORT_RANGE:
		status = VL53L1_preset_mode_histogram_multizone_short_range(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_LONG_RANGE:
		status = VL53L1_preset_mode_histogram_multizone_long_range(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_REF_ARRAY:
		status = VL53L1_preset_mode_histogram_ranging_ref(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_SHORT_TIMING:
		status = VL53L1_preset_mode_histogram_ranging_short_timing(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE:
		status = VL53L1_preset_mode_histogram_long_range(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE_MM1:
		status = VL53L1_preset_mode_histogram_long_range_mm1(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE_MM2:
		status = VL53L1_preset_mode_histogram_long_range_mm2(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE:
		status = VL53L1_preset_mode_histogram_medium_range(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE_MM1:
		status = VL53L1_preset_mode_histogram_medium_range_mm1(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE_MM2:
		status = VL53L1_preset_mode_histogram_medium_range_mm2(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE:
		status = VL53L1_preset_mode_histogram_short_range(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE_MM1:
		status = VL53L1_preset_mode_histogram_short_range_mm1(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE_MM2:
		status = VL53L1_preset_mode_histogram_short_range_mm2(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_CHARACTERISATION:
		status = VL53L1_preset_mode_histogram_characterisation(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_PLANAR:
		status = VL53L1_preset_mode_histogram_xtalk_planar(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_MM1:
		status = VL53L1_preset_mode_histogram_xtalk_mm1(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_MM2:
		status = VL53L1_preset_mode_histogram_xtalk_mm2(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_OLT:
		status = VL53L1_preset_mode_olt(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_SINGLESHOT_RANGING:
		status = VL53L1_preset_mode_singleshot_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
		break;

	case VL53L1_DEVICEPRESETMODE_LOWPOWERAUTO_SHORT_RANGE:
		status = VL53L1_preset_mode_low_power_auto_short_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg,
					plpadata);
		break;

	case VL53L1_DEVICEPRESETMODE_LOWPOWERAUTO_MEDIUM_RANGE:
		status = VL53L1_preset_mode_low_power_auto_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg,
					plpadata);
		break;

	case VL53L1_DEVICEPRESETMODE_LOWPOWERAUTO_LONG_RANGE:
		status = VL53L1_preset_mode_low_power_auto_long_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg,
					plpadata);
		break;


	case VL53L1_DEVICEPRESETMODE_SPECIAL_HISTOGRAM_SHORT_RANGE:
		status = VL53L1_preset_mode_special_histogram_short_range(
					phistpostprocess,
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					ptuning_parms,
					pzone_cfg);
	break;

	default:
		status = VL53L1_ERROR_INVALID_PARAMS;
		break;

	}




	if (status == VL53L1_ERROR_NONE) {

		pstatic->dss_config__target_total_rate_mcps =
				dss_config__target_total_rate_mcps;
		pdev->dss_config__target_total_rate_mcps    =
				dss_config__target_total_rate_mcps;

	}







	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_timeouts_us(
				Dev,
				phasecal_config_timeout_us,
				mm_config_timeout_us,
				range_config_timeout_us);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_set_inter_measurement_period_ms(
				Dev,
				inter_measurement_period_ms);




	V53L1_init_zone_results_structure(
			pdev->zone_cfg.active_zones+1,
			&(pres->zone_results));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_zone_preset(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceZonePreset    zone_preset)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_general_config_t       *pgeneral      = &(pdev->gen_cfg);
	VL53L1_zone_config_t          *pzone_cfg     = &(pdev->zone_cfg);

	LOG_FUNCTION_START("");



	pdev->zone_preset        = zone_preset;




	switch (zone_preset) {

	case VL53L1_DEVICEZONEPRESET_XTALK_PLANAR:
		status =
			VL53L1_zone_preset_xtalk_planar(
				pgeneral,
				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_1X1_SIZE_16X16:
		status =
			VL53L1_init_zone_config_structure(
				8, 1, 1,

				8, 1, 1,

				15, 15,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_1X2_SIZE_16X8:
		status =
			VL53L1_init_zone_config_structure(
				8, 1, 1,

				4, 8, 2,

				15, 7,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_2X1_SIZE_8X16:
		status =
			VL53L1_init_zone_config_structure(
				4, 8, 2,

				8, 1, 1,

				7, 15,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_2X2_SIZE_8X8:
		status =
			VL53L1_init_zone_config_structure(
				4, 8, 2,

				4, 8, 2,

				7, 7,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_3X3_SIZE_5X5:
		status =
			VL53L1_init_zone_config_structure(
				2, 5, 3,

				2, 5, 3,

				4, 4,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_4X4_SIZE_4X4:
		status =
			VL53L1_init_zone_config_structure(
				2, 4, 4,

				2, 4, 4,

				3, 3,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_5X5_SIZE_4X4:
		status =
			VL53L1_init_zone_config_structure(
				2, 3, 5,

				2, 3, 5,

				3, 3,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_11X11_SIZE_5X5:
		status =
			VL53L1_init_zone_config_structure(
				3, 1, 11,

				3, 1, 11,

				4, 4,

				pzone_cfg);
		break;

	case VL53L1_DEVICEZONEPRESET_13X13_SIZE_4X4:
		status =
			VL53L1_init_zone_config_structure(
				2, 1, 13,

				2, 1, 13,

				3, 3,

				pzone_cfg);

		break;

	case VL53L1_DEVICEZONEPRESET_1X1_SIZE_4X4_POS_8X8:
		status =
			VL53L1_init_zone_config_structure(
				8, 1, 1,

				8, 1, 1,

				3, 3,

				pzone_cfg);
		break;

	}










	if (pzone_cfg->active_zones == 0)
		pdev->gen_cfg.global_config__stream_divider = 0;
	else if (pzone_cfg->active_zones < VL53L1_MAX_USER_ZONES)
		pdev->gen_cfg.global_config__stream_divider =
			pzone_cfg->active_zones + 1;
	else
		pdev->gen_cfg.global_config__stream_divider =
			VL53L1_MAX_USER_ZONES + 1;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error  VL53L1_enable_xtalk_compensation(
	VL53L1_DEV                 Dev)
{








	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint32_t tempu32;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_xtalk_config_t *pC = &(pdev->xtalk_cfg);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_customer_nvm_managed_t *pN = &(pdev->customer);

	LOG_FUNCTION_START("");



	tempu32 = VL53L1_calc_crosstalk_plane_offset_with_margin(
		pC->algo__crosstalk_compensation_plane_offset_kcps,
		pC->lite_mode_crosstalk_margin_kcps);
	if (tempu32 > 0xFFFF)
		tempu32 = 0xFFFF;

	pN->algo__crosstalk_compensation_plane_offset_kcps =
		(uint16_t)tempu32;

	pN->algo__crosstalk_compensation_x_plane_gradient_kcps =
		pC->algo__crosstalk_compensation_x_plane_gradient_kcps;

	pN->algo__crosstalk_compensation_y_plane_gradient_kcps =
		pC->algo__crosstalk_compensation_y_plane_gradient_kcps;



	pHP->algo__crosstalk_compensation_plane_offset_kcps =
		VL53L1_calc_crosstalk_plane_offset_with_margin(
			pC->algo__crosstalk_compensation_plane_offset_kcps,
			pC->histogram_mode_crosstalk_margin_kcps);

	pHP->algo__crosstalk_compensation_x_plane_gradient_kcps
		= pC->algo__crosstalk_compensation_x_plane_gradient_kcps;
	pHP->algo__crosstalk_compensation_y_plane_gradient_kcps
		= pC->algo__crosstalk_compensation_y_plane_gradient_kcps;




	pC->global_crosstalk_compensation_enable = 0x01;

	pHP->algo__crosstalk_compensation_enable =
		pC->global_crosstalk_compensation_enable;





	if (status == VL53L1_ERROR_NONE) {
		pC->crosstalk_range_ignore_threshold_rate_mcps =
		VL53L1_calc_range_ignore_threshold(
			pC->algo__crosstalk_compensation_plane_offset_kcps,
			pC->algo__crosstalk_compensation_x_plane_gradient_kcps,
			pC->algo__crosstalk_compensation_y_plane_gradient_kcps,
			pC->crosstalk_range_ignore_threshold_mult);
}




	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_set_customer_nvm_managed(
				Dev,
				&(pdev->customer));

	LOG_FUNCTION_END(status);

	return status;

}

void VL53L1_get_xtalk_compensation_enable(
	VL53L1_DEV    Dev,
	uint8_t       *pcrosstalk_compensation_enable)
{














	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");




	*pcrosstalk_compensation_enable =
		pdev->xtalk_cfg.global_crosstalk_compensation_enable;

}


VL53L1_Error VL53L1_get_lite_xtalk_margin_kcps(
	VL53L1_DEV                          Dev,
	int16_t                           *pxtalk_margin)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pxtalk_margin = pdev->xtalk_cfg.lite_mode_crosstalk_margin_kcps;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_lite_xtalk_margin_kcps(
	VL53L1_DEV                     Dev,
	int16_t                        xtalk_margin)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->xtalk_cfg.lite_mode_crosstalk_margin_kcps = xtalk_margin;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_histogram_xtalk_margin_kcps(
	VL53L1_DEV                          Dev,
	int16_t                           *pxtalk_margin)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pxtalk_margin = pdev->xtalk_cfg.histogram_mode_crosstalk_margin_kcps;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_histogram_xtalk_margin_kcps(
	VL53L1_DEV                     Dev,
	int16_t                        xtalk_margin)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->xtalk_cfg.histogram_mode_crosstalk_margin_kcps = xtalk_margin;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_restore_xtalk_nvm_default(
	VL53L1_DEV                     Dev)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_xtalk_config_t *pC = &(pdev->xtalk_cfg);

	LOG_FUNCTION_START("");

	pC->algo__crosstalk_compensation_plane_offset_kcps =
		pC->nvm_default__crosstalk_compensation_plane_offset_kcps;
	pC->algo__crosstalk_compensation_x_plane_gradient_kcps =
		pC->nvm_default__crosstalk_compensation_x_plane_gradient_kcps;
	pC->algo__crosstalk_compensation_y_plane_gradient_kcps =
		pC->nvm_default__crosstalk_compensation_y_plane_gradient_kcps;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error  VL53L1_disable_xtalk_compensation(
	VL53L1_DEV                 Dev)
{








	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_customer_nvm_managed_t *pN = &(pdev->customer);

	LOG_FUNCTION_START("");



	pN->algo__crosstalk_compensation_plane_offset_kcps =
		0x00;

	pN->algo__crosstalk_compensation_x_plane_gradient_kcps =
		0x00;

	pN->algo__crosstalk_compensation_y_plane_gradient_kcps =
		0x00;




	pdev->xtalk_cfg.global_crosstalk_compensation_enable = 0x00;

	pHP->algo__crosstalk_compensation_enable =
		pdev->xtalk_cfg.global_crosstalk_compensation_enable;




	if (status == VL53L1_ERROR_NONE) {
		pdev->xtalk_cfg.crosstalk_range_ignore_threshold_rate_mcps =
			0x0000;
	}




	if (status == VL53L1_ERROR_NONE) {

		status =
			VL53L1_set_customer_nvm_managed(
				Dev,
				&(pdev->customer));
	}
	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_get_histogram_phase_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                            *pphase_consistency)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);

	LOG_FUNCTION_START("");

	*pphase_consistency =
		pHP->algo__consistency_check__phase_tolerance;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_histogram_phase_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                             phase_consistency)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->histpostprocess.algo__consistency_check__phase_tolerance =
			phase_consistency;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_get_histogram_event_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                            *pevent_consistency)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pevent_consistency =
		pdev->histpostprocess.algo__consistency_check__event_sigma;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_histogram_event_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                             event_consistency)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->histpostprocess.algo__consistency_check__event_sigma =
		event_consistency;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_get_histogram_ambient_threshold_sigma(
	VL53L1_DEV                          Dev,
	uint8_t                            *pamb_thresh_sigma)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pamb_thresh_sigma =
			pdev->histpostprocess.ambient_thresh_sigma1;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_histogram_ambient_threshold_sigma(
	VL53L1_DEV                          Dev,
	uint8_t                             amb_thresh_sigma)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->histpostprocess.ambient_thresh_sigma1 =
		amb_thresh_sigma;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_get_lite_sigma_threshold(
	VL53L1_DEV                          Dev,
	uint16_t                           *plite_sigma)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*plite_sigma =
			pdev->tim_cfg.range_config__sigma_thresh;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_lite_sigma_threshold(
	VL53L1_DEV                          Dev,
	uint16_t                           lite_sigma)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->tim_cfg.range_config__sigma_thresh = lite_sigma;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_get_lite_min_count_rate(
	VL53L1_DEV                          Dev,
	uint16_t                           *plite_mincountrate)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*plite_mincountrate =
		pdev->tim_cfg.range_config__min_count_rate_rtn_limit_mcps;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_lite_min_count_rate(
	VL53L1_DEV                          Dev,
	uint16_t                            lite_mincountrate)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->tim_cfg.range_config__min_count_rate_rtn_limit_mcps =
		lite_mincountrate;

	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_get_xtalk_detect_config(
	VL53L1_DEV                          Dev,
	int16_t                            *pmax_valid_range_mm,
	int16_t                            *pmin_valid_range_mm,
	uint16_t                           *pmax_valid_rate_kcps,
	uint16_t                           *pmax_sigma_mm)
{













	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pmax_valid_range_mm =
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_range_mm;
	*pmin_valid_range_mm =
		pdev->xtalk_cfg.algo__crosstalk_detect_min_valid_range_mm;
	*pmax_valid_rate_kcps =
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_rate_kcps;
	*pmax_sigma_mm =
		pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_xtalk_detect_config(
	VL53L1_DEV                          Dev,
	int16_t                             max_valid_range_mm,
	int16_t                             min_valid_range_mm,
	uint16_t                            max_valid_rate_kcps,
	uint16_t                            max_sigma_mm)
{













	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_range_mm =
		max_valid_range_mm;
	pdev->xtalk_cfg.algo__crosstalk_detect_min_valid_range_mm =
		min_valid_range_mm;
	pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_rate_kcps =
		max_valid_rate_kcps;
	pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm =
		max_sigma_mm;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_get_target_order_mode(
	VL53L1_DEV                          Dev,
	VL53L1_HistTargetOrder             *phist_target_order)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*phist_target_order =
			pdev->histpostprocess.hist_target_order;

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_target_order_mode(
	VL53L1_DEV                          Dev,
	VL53L1_HistTargetOrder              hist_target_order)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->histpostprocess.hist_target_order = hist_target_order;

	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_get_dmax_reflectance_values(
	VL53L1_DEV                          Dev,
	VL53L1_dmax_reflectance_array_t    *pdmax_reflectances)
{

	VL53L1_Error  status = VL53L1_ERROR_NONE;

	uint8_t i = 0;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");








	for (i = 0; i < VL53L1_MAX_AMBIENT_DMAX_VALUES; i++) {
		pdmax_reflectances->target_reflectance_for_dmax[i] =
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[i];
	}

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_set_dmax_reflectance_values(
	VL53L1_DEV                          Dev,
	VL53L1_dmax_reflectance_array_t    *pdmax_reflectances)
{

	VL53L1_Error  status = VL53L1_ERROR_NONE;

	uint8_t i = 0;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");








	for (i = 0; i < VL53L1_MAX_AMBIENT_DMAX_VALUES; i++) {
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[i] =
		pdmax_reflectances->target_reflectance_for_dmax[i];
	}

	LOG_FUNCTION_END(status);

	return status;

}

VL53L1_Error VL53L1_get_vhv_loopbound(
	VL53L1_DEV                   Dev,
	uint8_t                     *pvhv_loopbound)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pvhv_loopbound =
		pdev->stat_nvm.vhv_config__timeout_macrop_loop_bound / 4;

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_set_vhv_loopbound(
	VL53L1_DEV                   Dev,
	uint8_t                      vhv_loopbound)
{











	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->stat_nvm.vhv_config__timeout_macrop_loop_bound =
		(pdev->stat_nvm.vhv_config__timeout_macrop_loop_bound & 0x03) +
		(vhv_loopbound * 4);

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_get_vhv_config(
	VL53L1_DEV                   Dev,
	uint8_t                     *pvhv_init_en,
	uint8_t                     *pvhv_init_value)
{


















	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pvhv_init_en    = (pdev->stat_nvm.vhv_config__init & 0x80) >> 7;
	*pvhv_init_value =
			(pdev->stat_nvm.vhv_config__init & 0x7F);

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_set_vhv_config(
	VL53L1_DEV                   Dev,
	uint8_t                      vhv_init_en,
	uint8_t                      vhv_init_value)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->stat_nvm.vhv_config__init =
		((vhv_init_en   & 0x01) << 7) +
		(vhv_init_value & 0x7F);

	LOG_FUNCTION_END(status);

	return status;

}



VL53L1_Error VL53L1_init_and_start_range(
	VL53L1_DEV                     Dev,
	uint8_t                        measurement_mode,
	VL53L1_DeviceConfigLevel       device_config_level)
{













	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_static_nvm_managed_t   *pstatic_nvm   = &(pdev->stat_nvm);
	VL53L1_customer_nvm_managed_t *pcustomer_nvm = &(pdev->customer);
	VL53L1_static_config_t        *pstatic       = &(pdev->stat_cfg);
	VL53L1_general_config_t       *pgeneral      = &(pdev->gen_cfg);
	VL53L1_timing_config_t        *ptiming       = &(pdev->tim_cfg);
	VL53L1_dynamic_config_t       *pdynamic      = &(pdev->dyn_cfg);
	VL53L1_system_control_t       *psystem       = &(pdev->sys_ctrl);

	VL53L1_ll_driver_state_t  *pstate   = &(pdev->ll_state);
	VL53L1_customer_nvm_managed_t *pN = &(pdev->customer);

	uint8_t  *pbuffer                   = &buffer[0];
	uint16_t i                          = 0;
	uint16_t i2c_index                  = 0;
	uint16_t i2c_buffer_offset_bytes    = 0;
	uint16_t i2c_buffer_size_bytes      = 0;

	LOG_FUNCTION_START("");



	pdev->measurement_mode = measurement_mode;




	psystem->system__mode_start =
		(psystem->system__mode_start &
		VL53L1_DEVICEMEASUREMENTMODE_STOP_MASK) |
		measurement_mode;






	status =
		VL53L1_set_user_zone(
		Dev,
		&(pdev->zone_cfg.user_zones[pdev->ll_state.cfg_zone_id]));





	if (pdev->zone_cfg.active_zones > 0) {
		status =
		VL53L1_set_zone_dss_config(
		Dev,
		&(pres->zone_dyn_cfgs.VL53L1_p_002[pdev->ll_state.cfg_zone_id])
		);
	}






	if (((pdev->sys_ctrl.system__mode_start &
		VL53L1_DEVICESCHEDULERMODE_HISTOGRAM) == 0x00) &&
		(pdev->xtalk_cfg.global_crosstalk_compensation_enable
				== 0x01)) {
		pdev->stat_cfg.algo__range_ignore_threshold_mcps =
		pdev->xtalk_cfg.crosstalk_range_ignore_threshold_rate_mcps;
	}








	if (pdev->low_power_auto_data.low_power_auto_range_count == 0xFF)
		pdev->low_power_auto_data.low_power_auto_range_count = 0x0;



	if ((pdev->low_power_auto_data.is_low_power_auto_mode == 1) &&
		(pdev->low_power_auto_data.low_power_auto_range_count == 0)) {


		pdev->low_power_auto_data.saved_interrupt_config =
			pdev->gen_cfg.system__interrupt_config_gpio;


		pdev->gen_cfg.system__interrupt_config_gpio = 1 << 5;


		if ((pdev->dyn_cfg.system__sequence_config & (
			VL53L1_SEQUENCE_MM1_EN | VL53L1_SEQUENCE_MM2_EN)) ==
				0x0) {
			pN->algo__part_to_part_range_offset_mm =
			(pN->mm_config__outer_offset_mm << 2);
		} else {
			pN->algo__part_to_part_range_offset_mm = 0x0;
		}



		if (device_config_level <
				VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS) {
			device_config_level =
				VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS;
		}
	}

	if ((pdev->low_power_auto_data.is_low_power_auto_mode == 1) &&
		(pdev->low_power_auto_data.low_power_auto_range_count == 1)) {


		pdev->gen_cfg.system__interrupt_config_gpio =
			pdev->low_power_auto_data.saved_interrupt_config;



		device_config_level = VL53L1_DEVICECONFIGLEVEL_FULL;
	}









	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_save_cfg_data(Dev);






	switch (device_config_level) {
	case VL53L1_DEVICECONFIGLEVEL_FULL:
		i2c_index = VL53L1_STATIC_NVM_MANAGED_I2C_INDEX;
		break;
	case VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS:
		i2c_index = VL53L1_CUSTOMER_NVM_MANAGED_I2C_INDEX;
		break;
	case VL53L1_DEVICECONFIGLEVEL_STATIC_ONWARDS:
		i2c_index = VL53L1_STATIC_CONFIG_I2C_INDEX;
		break;
	case VL53L1_DEVICECONFIGLEVEL_GENERAL_ONWARDS:
		i2c_index = VL53L1_GENERAL_CONFIG_I2C_INDEX;
		break;
	case VL53L1_DEVICECONFIGLEVEL_TIMING_ONWARDS:
		i2c_index = VL53L1_TIMING_CONFIG_I2C_INDEX;
		break;
	case VL53L1_DEVICECONFIGLEVEL_DYNAMIC_ONWARDS:
		i2c_index = VL53L1_DYNAMIC_CONFIG_I2C_INDEX;
		break;
	default:
		i2c_index = VL53L1_SYSTEM_CONTROL_I2C_INDEX;
		break;
	}




	i2c_buffer_size_bytes =
			(VL53L1_SYSTEM_CONTROL_I2C_INDEX +
			VL53L1_SYSTEM_CONTROL_I2C_SIZE_BYTES) -
			i2c_index;




	pbuffer = &buffer[0];
	for (i = 0; i < i2c_buffer_size_bytes; i++)
		*pbuffer++ = 0;




	if (device_config_level >= VL53L1_DEVICECONFIGLEVEL_FULL &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
			VL53L1_STATIC_NVM_MANAGED_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_encode_static_nvm_managed(
				pstatic_nvm,
				VL53L1_STATIC_NVM_MANAGED_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
			VL53L1_CUSTOMER_NVM_MANAGED_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_encode_customer_nvm_managed(
				pcustomer_nvm,
				VL53L1_CUSTOMER_NVM_MANAGED_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEVICECONFIGLEVEL_STATIC_ONWARDS &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
			VL53L1_STATIC_CONFIG_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_encode_static_config(
				pstatic,
				VL53L1_STATIC_CONFIG_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEVICECONFIGLEVEL_GENERAL_ONWARDS &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_GENERAL_CONFIG_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_encode_general_config(
				pgeneral,
				VL53L1_GENERAL_CONFIG_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEVICECONFIGLEVEL_TIMING_ONWARDS &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_TIMING_CONFIG_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_encode_timing_config(
				ptiming,
				VL53L1_TIMING_CONFIG_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEVICECONFIGLEVEL_DYNAMIC_ONWARDS &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
			VL53L1_DYNAMIC_CONFIG_I2C_INDEX - i2c_index;



		if ((psystem->system__mode_start &
			VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK) ==
			VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK) {
			pdynamic->system__grouped_parameter_hold_0 =
					pstate->cfg_gph_id | 0x01;
			pdynamic->system__grouped_parameter_hold_1 =
					pstate->cfg_gph_id | 0x01;
			pdynamic->system__grouped_parameter_hold   =
					pstate->cfg_gph_id;
		}
		status =
			VL53L1_i2c_encode_dynamic_config(
				pdynamic,
				VL53L1_DYNAMIC_CONFIG_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_SYSTEM_CONTROL_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_encode_system_control(
				psystem,
				VL53L1_SYSTEM_CONTROL_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes]);
	}




	if (status == VL53L1_ERROR_NONE) {
		status =
			VL53L1_WriteMulti(
				Dev,
				i2c_index,
				buffer,
				(uint32_t)i2c_buffer_size_bytes);
	}





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_update_ll_driver_rd_state(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_update_ll_driver_cfg_state(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_stop_range(
	VL53L1_DEV     Dev)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);




	pdev->sys_ctrl.system__mode_start =
			(pdev->sys_ctrl.system__mode_start &
				VL53L1_DEVICEMEASUREMENTMODE_STOP_MASK) |
			 VL53L1_DEVICEMEASUREMENTMODE_ABORT;

	status = VL53L1_set_system_control(
				Dev,
				&pdev->sys_ctrl);



	pdev->sys_ctrl.system__mode_start =
			(pdev->sys_ctrl.system__mode_start &
				VL53L1_DEVICEMEASUREMENTMODE_STOP_MASK);



	VL53L1_init_ll_driver_state(
			Dev,
			VL53L1_DEVICESTATE_SW_STANDBY);



	V53L1_init_zone_results_structure(
			pdev->zone_cfg.active_zones+1,
			&(pres->zone_results));



	V53L1_init_zone_dss_configs(Dev);



	if (pdev->low_power_auto_data.is_low_power_auto_mode == 1)
		VL53L1_low_power_auto_data_stop_range(Dev);

	return status;
}


VL53L1_Error VL53L1_get_measurement_results(
	VL53L1_DEV                     Dev,
	VL53L1_DeviceResultsLevel      device_results_level)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_system_results_t   *psystem_results = &(pdev->sys_results);
	VL53L1_core_results_t     *pcore_results   = &(pdev->core_results);
	VL53L1_debug_results_t    *pdebug_results  = &(pdev->dbg_results);

	uint16_t i2c_index               = VL53L1_SYSTEM_RESULTS_I2C_INDEX;
	uint16_t i2c_buffer_offset_bytes = 0;
	uint16_t i2c_buffer_size_bytes   = 0;

	LOG_FUNCTION_START("");




	switch (device_results_level) {
	case VL53L1_DEVICERESULTSLEVEL_FULL:
		i2c_buffer_size_bytes =
				(VL53L1_DEBUG_RESULTS_I2C_INDEX +
				VL53L1_DEBUG_RESULTS_I2C_SIZE_BYTES) -
				i2c_index;
		break;
	case VL53L1_DEVICERESULTSLEVEL_UPTO_CORE:
		i2c_buffer_size_bytes =
				(VL53L1_CORE_RESULTS_I2C_INDEX +
				VL53L1_CORE_RESULTS_I2C_SIZE_BYTES) -
				i2c_index;
		break;
	default:
		i2c_buffer_size_bytes =
				VL53L1_SYSTEM_RESULTS_I2C_SIZE_BYTES;
		break;
	}




	if (status == VL53L1_ERROR_NONE)

		status =
			VL53L1_ReadMulti(
				Dev,
				i2c_index,
				buffer,
				(uint32_t)i2c_buffer_size_bytes);




	if (device_results_level >= VL53L1_DEVICERESULTSLEVEL_FULL &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_DEBUG_RESULTS_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_decode_debug_results(
				VL53L1_DEBUG_RESULTS_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes],
				pdebug_results);
	}

	if (device_results_level >= VL53L1_DEVICERESULTSLEVEL_UPTO_CORE &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_CORE_RESULTS_I2C_INDEX - i2c_index;

		status =
			VL53L1_i2c_decode_core_results(
				VL53L1_CORE_RESULTS_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes],
				pcore_results);
	}

	if (status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = 0;
		status =
			VL53L1_i2c_decode_system_results(
				VL53L1_SYSTEM_RESULTS_I2C_SIZE_BYTES,
				&buffer[i2c_buffer_offset_bytes],
				psystem_results);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_device_results(
	VL53L1_DEV                    Dev,
	VL53L1_DeviceResultsLevel     device_results_level,
	VL53L1_range_results_t       *prange_results)
{











	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_range_results_t   *presults =
			&(pres->range_results);
	VL53L1_zone_objects_t    *pobjects =
			&(pres->zone_results.VL53L1_p_002[0]);
	VL53L1_ll_driver_state_t *pstate   =
			&(pdev->ll_state);
	VL53L1_zone_config_t     *pzone_cfg =
			&(pdev->zone_cfg);
	VL53L1_zone_hist_info_t  *phist_info =
			&(pres->zone_hists.VL53L1_p_002[0]);

	VL53L1_dmax_calibration_data_t   dmax_cal;
	VL53L1_dmax_calibration_data_t *pdmax_cal = &dmax_cal;
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_xtalk_config_t *pC = &(pdev->xtalk_cfg);
	VL53L1_low_power_auto_data_t *pL = &(pdev->low_power_auto_data);
	VL53L1_histogram_bin_data_t *pHD = &(pdev->hist_data);
	VL53L1_customer_nvm_managed_t *pN = &(pdev->customer);
	VL53L1_zone_histograms_t *pZH = &(pres->zone_hists);

	uint8_t i;
	uint8_t tmp8;
	uint8_t zid;

	LOG_FUNCTION_START("");




	if ((pdev->sys_ctrl.system__mode_start &
		 VL53L1_DEVICESCHEDULERMODE_HISTOGRAM)
		 == VL53L1_DEVICESCHEDULERMODE_HISTOGRAM) {






		status = VL53L1_get_histogram_bin_data(
						Dev,
						&(pdev->hist_data));







		if (status == VL53L1_ERROR_NONE &&
			pHD->number_of_ambient_bins == 0) {
			zid = pdev->ll_state.rd_zone_id;
			status = VL53L1_hist_copy_and_scale_ambient_info(
			&(pZH->VL53L1_p_002[zid]),
			&(pdev->hist_data));
		}






		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;


		pHP->gain_factor =
			pdev->gain_cal.histogram_ranging_gain_factor;

		pHP->algo__crosstalk_compensation_plane_offset_kcps =
		VL53L1_calc_crosstalk_plane_offset_with_margin(
		pC->algo__crosstalk_compensation_plane_offset_kcps,
		pC->histogram_mode_crosstalk_margin_kcps);

		pHP->algo__crosstalk_compensation_x_plane_gradient_kcps =
		pC->algo__crosstalk_compensation_x_plane_gradient_kcps;
		pHP->algo__crosstalk_compensation_y_plane_gradient_kcps =
		pC->algo__crosstalk_compensation_y_plane_gradient_kcps;

		pdev->dmax_cfg.ambient_thresh_sigma =
			pHP->ambient_thresh_sigma1;
		pdev->dmax_cfg.min_ambient_thresh_events =
			pHP->min_ambient_thresh_events;
		pdev->dmax_cfg.signal_total_events_limit =
			pHP->signal_total_events_limit;
		pdev->dmax_cfg.dss_config__target_total_rate_mcps =
			pdev->stat_cfg.dss_config__target_total_rate_mcps;
		pdev->dmax_cfg.dss_config__aperture_attenuation =
			pdev->gen_cfg.dss_config__aperture_attenuation;

		pHP->algo__crosstalk_detect_max_valid_range_mm =
			pC->algo__crosstalk_detect_max_valid_range_mm;
		pHP->algo__crosstalk_detect_min_valid_range_mm =
			pC->algo__crosstalk_detect_min_valid_range_mm;
		pHP->algo__crosstalk_detect_max_valid_rate_kcps =
			pC->algo__crosstalk_detect_max_valid_rate_kcps;
		pHP->algo__crosstalk_detect_max_sigma_mm =
			pC->algo__crosstalk_detect_max_sigma_mm;




		VL53L1_copy_rtn_good_spads_to_buffer(
				&(pdev->nvm_copy_data),
				&(pdev->rtn_good_spads[0]));




		switch (pdev->offset_correction_mode) {

		case VL53L1_OFFSETCORRECTIONMODE__MM1_MM2_OFFSETS:
			tmp8 = pdev->gen_cfg.dss_config__aperture_attenuation;

			VL53L1_hist_combine_mm1_mm2_offsets(
			pN->mm_config__inner_offset_mm,
			pN->mm_config__outer_offset_mm,
			pdev->nvm_copy_data.roi_config__mode_roi_centre_spad,
			pdev->nvm_copy_data.roi_config__mode_roi_xy_size,
			pHD->roi_config__user_roi_centre_spad,
			pHD->roi_config__user_roi_requested_global_xy_size,
			&(pdev->add_off_cal_data),
			&(pdev->rtn_good_spads[0]),
			(uint16_t)tmp8,
			&(pHP->range_offset_mm));
		break;
		case VL53L1_OFFSETCORRECTIONMODE__PER_ZONE_OFFSETS:
			zid = pdev->ll_state.rd_zone_id;
			pHP->range_offset_mm = (int16_t)(
			pres->zone_cal.VL53L1_p_002[zid].range_mm_offset);
		break;
		default:
			pHP->range_offset_mm = 0;
		break;

		}






		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;


		VL53L1_calc_max_effective_spads(
		pHD->roi_config__user_roi_centre_spad,
		pHD->roi_config__user_roi_requested_global_xy_size,
		&(pdev->rtn_good_spads[0]),
		(uint16_t)pdev->gen_cfg.dss_config__aperture_attenuation,
		&(pdev->dmax_cfg.max_effective_spads));

		status =
			VL53L1_get_dmax_calibration_data(
				Dev,
				pdev->dmax_mode,
				pdev->ll_state.rd_zone_id,
				pdmax_cal);






		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;

		status = VL53L1_ipp_hist_process_data(
				Dev,
				pdmax_cal,
				&(pdev->dmax_cfg),
				&(pdev->histpostprocess),
				&(pdev->hist_data),
				&(pdev->xtalk_shapes),
				presults);



		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;

		status = VL53L1_hist_wrap_dmax(
				&(pdev->histpostprocess),
				&(pdev->hist_data),
				&(presults->wrap_dmax_mm));



		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;

		zid = pdev->ll_state.rd_zone_id;
		status = VL53L1_hist_phase_consistency_check(
			Dev,
			&(pZH->VL53L1_p_002[zid]),
			&(pres->zone_results.VL53L1_p_002[zid]),
			presults);



		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;

		zid = pdev->ll_state.rd_zone_id;
		status = VL53L1_hist_xmonitor_consistency_check(
			Dev,
			&(pZH->VL53L1_p_002[zid]),
			&(pres->zone_results.VL53L1_p_002[zid]),
			&(presults->xmonitor));





		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;


		zid = pdev->ll_state.rd_zone_id;
		pZH->max_zones    = VL53L1_MAX_USER_ZONES;
		pZH->active_zones =
				pdev->zone_cfg.active_zones+1;
		pHD->zone_id       = zid;

		if (zid <
				pres->zone_results.max_zones) {

			phist_info =
			&(pZH->VL53L1_p_002[zid]);

			phist_info->rd_device_state =
				pHD->rd_device_state;

			phist_info->number_of_ambient_bins =
				pHD->number_of_ambient_bins;

			phist_info->result__dss_actual_effective_spads =
			pHD->result__dss_actual_effective_spads;

			phist_info->VL53L1_p_009 =
				pHD->VL53L1_p_009;

			phist_info->total_periods_elapsed =
				pHD->total_periods_elapsed;

			phist_info->ambient_events_sum =
				pHD->ambient_events_sum;
		}







		if (status != VL53L1_ERROR_NONE)
			goto UPDATE_DYNAMIC_CONFIG;

		VL53L1_hist_copy_results_to_sys_and_core(
				&(pdev->hist_data),
				presults,
				&(pdev->sys_results),
				&(pdev->core_results));






UPDATE_DYNAMIC_CONFIG:
		if (pzone_cfg->active_zones > 0) {
			if (pstate->rd_device_state !=
				VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC) {
				if (status == VL53L1_ERROR_NONE) {
					status = VL53L1_dynamic_zone_update(
						Dev, presults);
				}
			}





			for (i = 0; i < VL53L1_MAX_USER_ZONES; i++) {
				pzone_cfg->bin_config[i] =
				((pdev->ll_state.cfg_internal_stream_count)
						& 0x01) ?
					VL53L1_ZONECONFIG_BINCONFIG__HIGHAMB :
					VL53L1_ZONECONFIG_BINCONFIG__LOWAMB;
			}

			if (status == VL53L1_ERROR_NONE)
				status = VL53L1_multizone_hist_bins_update(Dev);

		}





		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_dynamic_xtalk_correction_corrector(Dev);

#ifdef VL53L1_LOG_ENABLE
		if (status == VL53L1_ERROR_NONE)
			VL53L1_print_histogram_bin_data(
				&(pdev->hist_data),
				"get_device_results():pdev->lldata.hist_data.",
				VL53L1_TRACE_MODULE_HISTOGRAM_DATA);
#endif

	} else {

		if (status == VL53L1_ERROR_NONE)

			status = VL53L1_get_measurement_results(
							Dev,
							device_results_level);

		if (status == VL53L1_ERROR_NONE)
			VL53L1_copy_sys_and_core_results_to_range_results(
			(int32_t)pdev->gain_cal.standard_ranging_gain_factor,
			&(pdev->sys_results),
			&(pdev->core_results),
			presults);





		if (pL->is_low_power_auto_mode == 1) {



			if ((status == VL53L1_ERROR_NONE) &&
				(pL->low_power_auto_range_count == 0)) {

				status =
				VL53L1_low_power_auto_setup_manual_calibration(
						Dev);
				pL->low_power_auto_range_count = 1;
			} else if ((status == VL53L1_ERROR_NONE) &&
				(pL->low_power_auto_range_count == 1)) {
				pL->low_power_auto_range_count = 2;
			}



			if ((pL->low_power_auto_range_count != 0xFF) &&
				(status == VL53L1_ERROR_NONE)) {
				status = VL53L1_low_power_auto_update_DSS(
						Dev);
			}
		}


	}



	presults->cfg_device_state = pdev->ll_state.cfg_device_state;
	presults->rd_device_state  = pdev->ll_state.rd_device_state;
	presults->zone_id          = pdev->ll_state.rd_zone_id;

	if (status == VL53L1_ERROR_NONE) {



		pres->zone_results.max_zones    = VL53L1_MAX_USER_ZONES;
		pres->zone_results.active_zones = pdev->zone_cfg.active_zones+1;
		zid = pdev->ll_state.rd_zone_id;

		if (zid < pres->zone_results.max_zones) {

			pobjects =
			&(pres->zone_results.VL53L1_p_002[zid]);

			pobjects->cfg_device_state  =
					presults->cfg_device_state;
			pobjects->rd_device_state   = presults->rd_device_state;
			pobjects->zone_id           = presults->zone_id;
			pobjects->stream_count      = presults->stream_count;




			pobjects->xmonitor.VL53L1_p_020 =
				presults->xmonitor.VL53L1_p_020;
			pobjects->xmonitor.VL53L1_p_021 =
				presults->xmonitor.VL53L1_p_021;
			pobjects->xmonitor.VL53L1_p_014 =
				presults->xmonitor.VL53L1_p_014;
			pobjects->xmonitor.range_status =
				presults->xmonitor.range_status;

			pobjects->max_objects      = presults->max_results;
			pobjects->active_objects   = presults->active_results;

			for (i = 0; i < presults->active_results; i++) {
				pobjects->VL53L1_p_002[i].VL53L1_p_020 =
					presults->VL53L1_p_002[i].VL53L1_p_020;
				pobjects->VL53L1_p_002[i].VL53L1_p_021 =
					presults->VL53L1_p_002[i].VL53L1_p_021;
				pobjects->VL53L1_p_002[i].VL53L1_p_014 =
					presults->VL53L1_p_002[i].VL53L1_p_014;
				pobjects->VL53L1_p_002[i].range_status =
					presults->VL53L1_p_002[i].range_status;
			}



		}
	}




	memcpy(
		prange_results,
		presults,
		sizeof(VL53L1_range_results_t));







	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_check_ll_driver_rd_state(Dev);

#ifdef VL53L1_LOG_ENABLE
	if (status == VL53L1_ERROR_NONE)
		VL53L1_print_range_results(
			presults,
			"get_device_results():pdev->llresults.range_results.",
			VL53L1_TRACE_MODULE_RANGE_RESULTS_DATA);
#endif

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_clear_interrupt_and_enable_next_range(
	VL53L1_DEV        Dev,
	uint8_t           measurement_mode)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");















	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_init_and_start_range(
				Dev,
				measurement_mode,
				VL53L1_DEVICECONFIGLEVEL_GENERAL_ONWARDS);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_histogram_bin_data(
		VL53L1_DEV                   Dev,
		VL53L1_histogram_bin_data_t *pdata)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_zone_private_dyn_cfg_t *pzone_dyn_cfg;

	VL53L1_static_nvm_managed_t   *pstat_nvm = &(pdev->stat_nvm);
	VL53L1_static_config_t        *pstat_cfg = &(pdev->stat_cfg);
	VL53L1_general_config_t       *pgen_cfg  = &(pdev->gen_cfg);
	VL53L1_timing_config_t        *ptim_cfg  = &(pdev->tim_cfg);
	VL53L1_range_results_t        *presults  = &(pres->range_results);

	uint8_t    buffer[VL53L1_MAX_I2C_XFER_SIZE];
	uint8_t   *pbuffer = &buffer[0];
	uint8_t    bin_23_0 = 0x00;
	uint16_t   bin                      = 0;
	uint16_t   i2c_buffer_offset_bytes  = 0;
	uint16_t   encoded_timeout          = 0;

	uint32_t   pll_period_us            = 0;
	uint32_t   periods_elapsed_tmp      = 0;

	uint8_t    i                        = 0;

	LOG_FUNCTION_START("");






	if (status == VL53L1_ERROR_NONE)

		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX,
			pbuffer,
			VL53L1_HISTOGRAM_BIN_DATA_I2C_SIZE_BYTES);






	pdata->result__interrupt_status               = *(pbuffer +   0);
	pdata->result__range_status                   = *(pbuffer +   1);
	pdata->result__report_status                  = *(pbuffer +   2);
	pdata->result__stream_count                   = *(pbuffer +   3);
	pdata->result__dss_actual_effective_spads =
		VL53L1_i2c_decode_uint16_t(2, pbuffer +   4);






	i2c_buffer_offset_bytes =
			VL53L1_PHASECAL_RESULT__REFERENCE_PHASE -
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX;

	pbuffer = &buffer[i2c_buffer_offset_bytes];

	pdata->phasecal_result__reference_phase =
			VL53L1_i2c_decode_uint16_t(2, pbuffer);

	i2c_buffer_offset_bytes =
			VL53L1_PHASECAL_RESULT__VCSEL_START -
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX;

	pdata->phasecal_result__vcsel_start = buffer[i2c_buffer_offset_bytes];




	pdev->dbg_results.phasecal_result__reference_phase =
			pdata->phasecal_result__reference_phase;
	pdev->dbg_results.phasecal_result__vcsel_start =
			pdata->phasecal_result__vcsel_start;







	i2c_buffer_offset_bytes =
			VL53L1_RESULT__HISTOGRAM_BIN_23_0_MSB -
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX;

	bin_23_0 = buffer[i2c_buffer_offset_bytes] << 2;

	i2c_buffer_offset_bytes =
			VL53L1_RESULT__HISTOGRAM_BIN_23_0_LSB -
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX;

	bin_23_0 += buffer[i2c_buffer_offset_bytes];

	i2c_buffer_offset_bytes =
			VL53L1_RESULT__HISTOGRAM_BIN_23_0 -
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX;

	buffer[i2c_buffer_offset_bytes] = bin_23_0;







	i2c_buffer_offset_bytes =
			VL53L1_RESULT__HISTOGRAM_BIN_0_2 -
			VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX;

	pbuffer = &buffer[i2c_buffer_offset_bytes];
	for (bin = 0; bin < VL53L1_HISTOGRAM_BUFFER_SIZE; bin++) {
		pdata->bin_data[bin] =
			(int32_t)VL53L1_i2c_decode_uint32_t(3, pbuffer);
		pbuffer += 3;
	}



	pdata->zone_id                 = pdev->ll_state.rd_zone_id;
	pdata->VL53L1_p_022               = 0;
	pdata->VL53L1_p_023             = VL53L1_HISTOGRAM_BUFFER_SIZE;
	pdata->VL53L1_p_024          = VL53L1_HISTOGRAM_BUFFER_SIZE;

	pdata->cal_config__vcsel_start = pgen_cfg->cal_config__vcsel_start;




	pdata->vcsel_width =
		((uint16_t)pgen_cfg->global_config__vcsel_width) << 4;
	pdata->vcsel_width +=
		(uint16_t)pstat_cfg->ana_config__vcsel_pulse_width_offset;



	pdata->VL53L1_p_019 =
		pstat_nvm->osc_measured__fast_osc__frequency;




	VL53L1_hist_get_bin_sequence_config(Dev, pdata);






	if (pdev->ll_state.rd_timing_status == 0) {

		encoded_timeout =
			(ptim_cfg->range_config__timeout_macrop_a_hi << 8)
			+ ptim_cfg->range_config__timeout_macrop_a_lo;
		pdata->VL53L1_p_009 =  ptim_cfg->range_config__vcsel_period_a;
	} else {

		encoded_timeout =
			(ptim_cfg->range_config__timeout_macrop_b_hi << 8)
			+ ptim_cfg->range_config__timeout_macrop_b_lo;
		pdata->VL53L1_p_009 = ptim_cfg->range_config__vcsel_period_b;
	}




	pdata->number_of_ambient_bins  = 0;

	for (i = 0; i < 6; i++) {
		if ((pdata->bin_seq[i] & 0x07) == 0x07)
			pdata->number_of_ambient_bins  =
					pdata->number_of_ambient_bins + 0x04;
	}

	pdata->total_periods_elapsed =
		VL53L1_decode_timeout(encoded_timeout);







	pll_period_us =
		VL53L1_calc_pll_period_us(pdata->VL53L1_p_019);




	periods_elapsed_tmp = pdata->total_periods_elapsed + 1;






	pdata->peak_duration_us =
		VL53L1_duration_maths(
			pll_period_us,
			(uint32_t)pdata->vcsel_width,
			VL53L1_RANGING_WINDOW_VCSEL_PERIODS,
			periods_elapsed_tmp);

	pdata->woi_duration_us     = 0;




	VL53L1_hist_calc_zero_distance_phase(pdata);






	VL53L1_hist_estimate_ambient_from_ambient_bins(pdata);




	pdata->cfg_device_state = pdev->ll_state.cfg_device_state;
	pdata->rd_device_state  = pdev->ll_state.rd_device_state;




	pzone_dyn_cfg = &(pres->zone_dyn_cfgs.VL53L1_p_002[pdata->zone_id]);

	pdata->roi_config__user_roi_centre_spad =
		pzone_dyn_cfg->roi_config__user_roi_centre_spad;
	pdata->roi_config__user_roi_requested_global_xy_size =
		pzone_dyn_cfg->roi_config__user_roi_requested_global_xy_size;






	presults->device_status = VL53L1_DEVICEERROR_NOUPDATE;




	switch (pdata->result__range_status &
			VL53L1_RANGE_STATUS__RANGE_STATUS_MASK) {

	case VL53L1_DEVICEERROR_VCSELCONTINUITYTESTFAILURE:
	case VL53L1_DEVICEERROR_VCSELWATCHDOGTESTFAILURE:
	case VL53L1_DEVICEERROR_NOVHVVALUEFOUND:
	case VL53L1_DEVICEERROR_USERROICLIP:
	case VL53L1_DEVICEERROR_MULTCLIPFAIL:

		presults->device_status = (pdata->result__range_status &
				VL53L1_RANGE_STATUS__RANGE_STATUS_MASK);

		status = VL53L1_ERROR_RANGE_ERROR;

	break;

	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_copy_sys_and_core_results_to_range_results(
	int32_t                           gain_factor,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore,
	VL53L1_range_results_t           *presults)
{
	uint8_t  i = 0;

	VL53L1_range_data_t *pdata;
	int32_t range_mm = 0;
	uint32_t tmpu32 = 0;
	uint16_t rpscr_crosstalk_corrected_mcps_sd0;
	uint16_t rmmo_effective_spads_sd0;
	uint16_t rmmi_effective_spads_sd0;

	LOG_FUNCTION_START("");




	presults->zone_id         = 0;
	presults->stream_count    = psys->result__stream_count;
	presults->wrap_dmax_mm    = 0;
	presults->max_results     = VL53L1_MAX_RANGE_RESULTS;
	presults->active_results  = 1;
	rpscr_crosstalk_corrected_mcps_sd0 =
	psys->result__peak_signal_count_rate_crosstalk_corrected_mcps_sd0;
	rmmo_effective_spads_sd0 =
			psys->result__mm_outer_actual_effective_spads_sd0;
	rmmi_effective_spads_sd0 =
			psys->result__mm_inner_actual_effective_spads_sd0;


	for (i = 0; i < VL53L1_MAX_AMBIENT_DMAX_VALUES; i++)
		presults->VL53L1_p_007[i] = 0;

	pdata = &(presults->VL53L1_p_002[0]);

	for (i = 0; i < 2; i++) {

		pdata->range_id     = i;
		pdata->time_stamp   = 0;

		if ((psys->result__stream_count == 0) &&
			((psys->result__range_status &
				VL53L1_RANGE_STATUS__RANGE_STATUS_MASK) ==
			VL53L1_DEVICEERROR_RANGECOMPLETE)) {
			pdata->range_status =
				VL53L1_DEVICEERROR_RANGECOMPLETE_NO_WRAP_CHECK;
		} else {
			pdata->range_status =
				psys->result__range_status &
				VL53L1_RANGE_STATUS__RANGE_STATUS_MASK;
		}

		pdata->VL53L1_p_015 = 0;
		pdata->VL53L1_p_022    = 0;
		pdata->VL53L1_p_025   = 0;
		pdata->VL53L1_p_026     = 0;
		pdata->VL53L1_p_016   = 0;
		pdata->VL53L1_p_027    = 0;

		switch (i) {

		case 0:
			if (psys->result__report_status ==
				VL53L1_DEVICEREPORTSTATUS_MM1)
				pdata->VL53L1_p_006 =
						rmmi_effective_spads_sd0;
			else if (psys->result__report_status ==
					VL53L1_DEVICEREPORTSTATUS_MM2)
				pdata->VL53L1_p_006 =
						rmmo_effective_spads_sd0;
			else
				pdata->VL53L1_p_006 =
				psys->result__dss_actual_effective_spads_sd0;

			pdata->peak_signal_count_rate_mcps =
					rpscr_crosstalk_corrected_mcps_sd0;
			pdata->avg_signal_count_rate_mcps =
				psys->result__avg_signal_count_rate_mcps_sd0;
			pdata->ambient_count_rate_mcps =
				psys->result__ambient_count_rate_mcps_sd0;






			tmpu32 = ((uint32_t)psys->result__sigma_sd0 << 5);
			if (tmpu32 > 0xFFFF)
				tmpu32 = 0xFFFF;

			pdata->VL53L1_p_005 = (uint16_t)tmpu32;




			pdata->VL53L1_p_014 =
				psys->result__phase_sd0;

			range_mm = (int32_t)(
			psys->result__final_crosstalk_corrected_range_mm_sd0);



			range_mm *= gain_factor;
			range_mm += 0x0400;
			range_mm /= 0x0800;

			pdata->median_range_mm = (int16_t)range_mm;

			pdata->VL53L1_p_021 =
				pcore->result_core__ranging_total_events_sd0;
			pdata->VL53L1_p_013 =
				pcore->result_core__signal_total_events_sd0;
			pdata->total_periods_elapsed =
				pcore->result_core__total_periods_elapsed_sd0;
			pdata->VL53L1_p_020 =
				pcore->result_core__ambient_window_events_sd0;

			break;
		case 1:

			pdata->VL53L1_p_006 =
				psys->result__dss_actual_effective_spads_sd1;
			pdata->peak_signal_count_rate_mcps =
				psys->result__peak_signal_count_rate_mcps_sd1;
			pdata->avg_signal_count_rate_mcps =
				0xFFFF;
			pdata->ambient_count_rate_mcps =
				psys->result__ambient_count_rate_mcps_sd1;






			tmpu32 = ((uint32_t)psys->result__sigma_sd1 << 5);
			if (tmpu32 > 0xFFFF)
				tmpu32 = 0xFFFF;

			pdata->VL53L1_p_005 = (uint16_t)tmpu32;




			pdata->VL53L1_p_014 =
				psys->result__phase_sd1;

			range_mm = (int32_t)(
			psys->result__final_crosstalk_corrected_range_mm_sd1);



			range_mm *= gain_factor;
			range_mm += 0x0400;
			range_mm /= 0x0800;

			pdata->median_range_mm = (int16_t)range_mm;

			pdata->VL53L1_p_021 =
				pcore->result_core__ranging_total_events_sd1;
			pdata->VL53L1_p_013 =
				pcore->result_core__signal_total_events_sd1;
			pdata->total_periods_elapsed  =
				pcore->result_core__total_periods_elapsed_sd1;
			pdata->VL53L1_p_020 =
				pcore->result_core__ambient_window_events_sd1;

			break;
		}





		pdata->VL53L1_p_028    = pdata->VL53L1_p_014;
		pdata->VL53L1_p_029    = pdata->VL53L1_p_014;
		pdata->min_range_mm = pdata->median_range_mm;
		pdata->max_range_mm = pdata->median_range_mm;

		pdata++;
	}






	presults->device_status = VL53L1_DEVICEERROR_NOUPDATE;








	switch (psys->result__range_status &
			VL53L1_RANGE_STATUS__RANGE_STATUS_MASK) {

	case VL53L1_DEVICEERROR_VCSELCONTINUITYTESTFAILURE:
	case VL53L1_DEVICEERROR_VCSELWATCHDOGTESTFAILURE:
	case VL53L1_DEVICEERROR_NOVHVVALUEFOUND:
	case VL53L1_DEVICEERROR_USERROICLIP:
	case VL53L1_DEVICEERROR_MULTCLIPFAIL:

		presults->device_status = (psys->result__range_status &
				VL53L1_RANGE_STATUS__RANGE_STATUS_MASK);

		presults->VL53L1_p_002[0].range_status =
			VL53L1_DEVICEERROR_NOUPDATE;
	break;

	}

	LOG_FUNCTION_END(0);
}


VL53L1_Error VL53L1_set_zone_dss_config(
	VL53L1_DEV                      Dev,
	VL53L1_zone_private_dyn_cfg_t  *pzone_dyn_cfg)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->ll_state);

	LOG_FUNCTION_START("");

	if (pstate->cfg_device_state ==
		VL53L1_DEVICESTATE_RANGING_DSS_MANUAL) {
		pdev->gen_cfg.dss_config__roi_mode_control =
		VL53L1_DSS_CONTROL__MODE_EFFSPADS;
		pdev->gen_cfg.dss_config__manual_effective_spads_select =
			pzone_dyn_cfg->dss_requested_effective_spad_count;
	} else {
		pdev->gen_cfg.dss_config__roi_mode_control =
			VL53L1_DSS_CONTROL__MODE_TARGET_RATE;
	}

	LOG_FUNCTION_END(status);
	return status;
}


VL53L1_Error VL53L1_calc_ambient_dmax(
	VL53L1_DEV      Dev,
	uint16_t        target_reflectance,
	int16_t         *pambient_dmax_mm)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_dmax_calibration_data_t   dmax_cal;
	VL53L1_dmax_calibration_data_t *pdmax_cal = &dmax_cal;

	LOG_FUNCTION_START("");






	status =
		VL53L1_get_dmax_calibration_data(
			Dev,
			pdev->debug_mode,
			pdev->ll_state.rd_zone_id,
			pdmax_cal);






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ipp_hist_ambient_dmax(
				Dev,
				target_reflectance,
				&(pdev->fmt_dmax_cal),
				&(pdev->dmax_cfg),
				&(pdev->hist_data),
				pambient_dmax_mm);

	LOG_FUNCTION_END(status);

	return status;
}






VL53L1_Error VL53L1_set_GPIO_interrupt_config(
	VL53L1_DEV                      Dev,
	VL53L1_GPIO_Interrupt_Mode	intr_mode_distance,
	VL53L1_GPIO_Interrupt_Mode	intr_mode_rate,
	uint8_t				intr_new_measure_ready,
	uint8_t				intr_no_target,
	uint8_t				intr_combined_mode,
	uint16_t			thresh_distance_high,
	uint16_t			thresh_distance_low,
	uint16_t			thresh_rate_high,
	uint16_t			thresh_rate_low
	)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_GPIO_interrupt_config_t *pintconf =
			&(pdev->gpio_interrupt_config);

	LOG_FUNCTION_START("");



	pintconf->intr_mode_distance = intr_mode_distance;
	pintconf->intr_mode_rate = intr_mode_rate;
	pintconf->intr_new_measure_ready = intr_new_measure_ready;
	pintconf->intr_no_target = intr_no_target;
	pintconf->intr_combined_mode = intr_combined_mode;
	pintconf->threshold_distance_high = thresh_distance_high;
	pintconf->threshold_distance_low = thresh_distance_low;
	pintconf->threshold_rate_high = thresh_rate_high;
	pintconf->threshold_rate_low = thresh_rate_low;



	pdev->gen_cfg.system__interrupt_config_gpio =
		VL53L1_encode_GPIO_interrupt_config(pintconf);




	status = VL53L1_set_GPIO_thresholds_from_struct(
			Dev,
			pintconf);

	LOG_FUNCTION_END(status);
	return status;
}






VL53L1_Error VL53L1_set_GPIO_interrupt_config_struct(
	VL53L1_DEV                      Dev,
	VL53L1_GPIO_interrupt_config_t	intconf)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_GPIO_interrupt_config_t *pintconf =
			&(pdev->gpio_interrupt_config);

	LOG_FUNCTION_START("");



	memcpy(pintconf, &(intconf), sizeof(VL53L1_GPIO_interrupt_config_t));



	pdev->gen_cfg.system__interrupt_config_gpio =
		VL53L1_encode_GPIO_interrupt_config(pintconf);



	status = VL53L1_set_GPIO_thresholds_from_struct(
			Dev,
			pintconf);

	LOG_FUNCTION_END(status);
	return status;
}






VL53L1_Error VL53L1_get_GPIO_interrupt_config(
	VL53L1_DEV                      Dev,
	VL53L1_GPIO_interrupt_config_t	*pintconf)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");






	pdev->gpio_interrupt_config = VL53L1_decode_GPIO_interrupt_config(
			pdev->gen_cfg.system__interrupt_config_gpio);





	pdev->gpio_interrupt_config.threshold_distance_high =
		pdev->dyn_cfg.system__thresh_high;
	pdev->gpio_interrupt_config.threshold_distance_low =
		pdev->dyn_cfg.system__thresh_low;

	pdev->gpio_interrupt_config.threshold_rate_high =
		pdev->gen_cfg.system__thresh_rate_high;
	pdev->gpio_interrupt_config.threshold_rate_low =
		pdev->gen_cfg.system__thresh_rate_low;

	if (pintconf == &(pdev->gpio_interrupt_config))	{


	} else {



		memcpy(pintconf, &(pdev->gpio_interrupt_config),
				sizeof(VL53L1_GPIO_interrupt_config_t));
	}

	LOG_FUNCTION_END(status);
	return status;
}


VL53L1_Error VL53L1_set_dmax_mode(
	VL53L1_DEV               Dev,
	VL53L1_DeviceDmaxMode    dmax_mode)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->dmax_mode = dmax_mode;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_dmax_mode(
	VL53L1_DEV               Dev,
	VL53L1_DeviceDmaxMode   *pdmax_mode)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*pdmax_mode = pdev->dmax_mode;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_dmax_calibration_data(
	VL53L1_DEV                      Dev,
	VL53L1_DeviceDmaxMode           dmax_mode,
	uint8_t                         zone_id,
	VL53L1_dmax_calibration_data_t *pdmax_cal)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t    *pdev =
		VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
		VL53L1DevStructGetLLResultsHandle(Dev);

	LOG_FUNCTION_START("");

	switch (dmax_mode) {

	case VL53L1_DEVICEDMAXMODE__PER_ZONE_CAL_DATA:
		pdmax_cal->ref__actual_effective_spads =
		(uint16_t)pres->zone_cal.VL53L1_p_002[zone_id].effective_spads;
		pdmax_cal->ref__peak_signal_count_rate_mcps =
		(uint16_t)pres->zone_cal.VL53L1_p_002[zone_id].peak_rate_mcps;
		pdmax_cal->ref__distance_mm =
		pres->zone_cal.cal_distance_mm;
		pdmax_cal->ref_reflectance_pc =
		pres->zone_cal.cal_reflectance_pc;
		pdmax_cal->coverglass_transmission = 0x0100;
	break;

	case VL53L1_DEVICEDMAXMODE__CUST_CAL_DATA:
		memcpy(
			pdmax_cal,
			&(pdev->cust_dmax_cal),
			sizeof(VL53L1_dmax_calibration_data_t));
	break;

	case VL53L1_DEVICEDMAXMODE__FMT_CAL_DATA:
		memcpy(
			pdmax_cal,
			&(pdev->fmt_dmax_cal),
			sizeof(VL53L1_dmax_calibration_data_t));
	break;

	default:
		status = VL53L1_ERROR_INVALID_PARAMS;
	break;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_hist_dmax_config(
	VL53L1_DEV                      Dev,
	VL53L1_hist_gen3_dmax_config_t *pdmax_cfg)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(
		&(pdev->dmax_cfg),
		pdmax_cfg,
		sizeof(VL53L1_hist_gen3_dmax_config_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_hist_dmax_config(
	VL53L1_DEV                      Dev,
	VL53L1_hist_gen3_dmax_config_t *pdmax_cfg)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(
		pdmax_cfg,
		&(pdev->dmax_cfg),
		sizeof(VL53L1_hist_gen3_dmax_config_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_offset_calibration_mode(
	VL53L1_DEV                     Dev,
	VL53L1_OffsetCalibrationMode   offset_cal_mode)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->offset_calibration_mode = offset_cal_mode;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_offset_calibration_mode(
	VL53L1_DEV                     Dev,
	VL53L1_OffsetCalibrationMode  *poffset_cal_mode)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*poffset_cal_mode = pdev->offset_calibration_mode;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_offset_correction_mode(
	VL53L1_DEV                     Dev,
	VL53L1_OffsetCorrectionMode    offset_cor_mode)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->offset_correction_mode = offset_cor_mode;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_offset_correction_mode(
	VL53L1_DEV                     Dev,
	VL53L1_OffsetCorrectionMode   *poffset_cor_mode)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	*poffset_cor_mode = pdev->offset_correction_mode;

	LOG_FUNCTION_END(status);

	return status;
}





VL53L1_Error VL53L1_set_zone_calibration_data(
	VL53L1_DEV                          Dev,
	VL53L1_zone_calibration_results_t  *pzone_cal)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverResults_t *pres = VL53L1DevStructGetLLResultsHandle(Dev);

	LOG_FUNCTION_START("");

	if (pzone_cal->struct_version !=
		VL53L1_LL_ZONE_CALIBRATION_DATA_STRUCT_VERSION)
		status = VL53L1_ERROR_INVALID_PARAMS;


	if (status == VL53L1_ERROR_NONE)


		memcpy(
			&(pres->zone_cal),
			pzone_cal,
			sizeof(VL53L1_zone_calibration_results_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_zone_calibration_data(
	VL53L1_DEV                          Dev,
	VL53L1_zone_calibration_results_t  *pzone_cal)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverResults_t *pres = VL53L1DevStructGetLLResultsHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(
		pzone_cal,
		&(pres->zone_cal),
		sizeof(VL53L1_zone_calibration_results_t));

	pzone_cal->struct_version =
			VL53L1_LL_ZONE_CALIBRATION_DATA_STRUCT_VERSION;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_get_tuning_debug_data(
	VL53L1_DEV                            Dev,
	VL53L1_tuning_parameters_t           *ptun_data)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_xtalkextract_config_t *pXC = &(pdev->xtalk_extract_cfg);

	LOG_FUNCTION_START("");

	ptun_data->vl53l1_tuningparm_version =
		pdev->tuning_parms.tp_tuning_parm_version;

	ptun_data->vl53l1_tuningparm_key_table_version =
		pdev->tuning_parms.tp_tuning_parm_key_table_version;


	ptun_data->vl53l1_tuningparm_lld_version =
		pdev->tuning_parms.tp_tuning_parm_lld_version;

	ptun_data->vl53l1_tuningparm_hist_algo_select =
		pHP->hist_algo_select;

	ptun_data->vl53l1_tuningparm_hist_target_order =
		pHP->hist_target_order;

	ptun_data->vl53l1_tuningparm_hist_filter_woi_0 =
		pHP->filter_woi0;

	ptun_data->vl53l1_tuningparm_hist_filter_woi_1 =
		pHP->filter_woi1;

	ptun_data->vl53l1_tuningparm_hist_amb_est_method =
		pHP->hist_amb_est_method;

	ptun_data->vl53l1_tuningparm_hist_amb_thresh_sigma_0 =
		pHP->ambient_thresh_sigma0;

	ptun_data->vl53l1_tuningparm_hist_amb_thresh_sigma_1 =
		pHP->ambient_thresh_sigma1;

	ptun_data->vl53l1_tuningparm_hist_min_amb_thresh_events =
		pHP->min_ambient_thresh_events;

	ptun_data->vl53l1_tuningparm_hist_amb_events_scaler =
		pHP->ambient_thresh_events_scaler;

	ptun_data->vl53l1_tuningparm_hist_noise_threshold =
		pHP->noise_threshold;

	ptun_data->vl53l1_tuningparm_hist_signal_total_events_limit =
		pHP->signal_total_events_limit;

	ptun_data->vl53l1_tuningparm_hist_sigma_est_ref_mm =
		pHP->sigma_estimator__sigma_ref_mm;

	ptun_data->vl53l1_tuningparm_hist_sigma_thresh_mm =
		pHP->sigma_thresh;

	ptun_data->vl53l1_tuningparm_hist_gain_factor =
		pdev->gain_cal.histogram_ranging_gain_factor;

	ptun_data->vl53l1_tuningparm_consistency_hist_phase_tolerance =
		pHP->algo__consistency_check__phase_tolerance;

	ptun_data->vl53l1_tuningparm_consistency_hist_min_max_tolerance_mm =
		pHP->algo__consistency_check__min_max_tolerance;

	ptun_data->vl53l1_tuningparm_consistency_hist_event_sigma =
		pHP->algo__consistency_check__event_sigma;

	ptun_data->vl53l1_tuningparm_consistency_hist_event_sigma_min_spad_limit
		= pHP->algo__consistency_check__event_min_spad_count;

	ptun_data->vl53l1_tuningparm_initial_phase_rtn_histo_long_range =
		pdev->tuning_parms.tp_init_phase_rtn_hist_long;

	ptun_data->vl53l1_tuningparm_initial_phase_rtn_histo_med_range =
		pdev->tuning_parms.tp_init_phase_rtn_hist_med;

	ptun_data->vl53l1_tuningparm_initial_phase_rtn_histo_short_range =
		pdev->tuning_parms.tp_init_phase_rtn_hist_short;

	ptun_data->vl53l1_tuningparm_initial_phase_ref_histo_long_range =
		pdev->tuning_parms.tp_init_phase_ref_hist_long;

	ptun_data->vl53l1_tuningparm_initial_phase_ref_histo_med_range =
		pdev->tuning_parms.tp_init_phase_ref_hist_med;

	ptun_data->vl53l1_tuningparm_initial_phase_ref_histo_short_range =
		pdev->tuning_parms.tp_init_phase_ref_hist_short;

	ptun_data->vl53l1_tuningparm_xtalk_detect_min_valid_range_mm =
		pdev->xtalk_cfg.algo__crosstalk_detect_min_valid_range_mm;

	ptun_data->vl53l1_tuningparm_xtalk_detect_max_valid_range_mm =
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_range_mm;

	ptun_data->vl53l1_tuningparm_xtalk_detect_max_sigma_mm =
		pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm;

	ptun_data->vl53l1_tuningparm_xtalk_detect_min_max_tolerance =
		pHP->algo__crosstalk_detect_min_max_tolerance;

	ptun_data->vl53l1_tuningparm_xtalk_detect_max_valid_rate_kcps =
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_rate_kcps;

	ptun_data->vl53l1_tuningparm_xtalk_detect_event_sigma =
		pHP->algo__crosstalk_detect_event_sigma;

	ptun_data->vl53l1_tuningparm_hist_xtalk_margin_kcps =
		pdev->xtalk_cfg.histogram_mode_crosstalk_margin_kcps;

	ptun_data->vl53l1_tuningparm_consistency_lite_phase_tolerance =
		pdev->tuning_parms.tp_consistency_lite_phase_tolerance;

	ptun_data->vl53l1_tuningparm_phasecal_target =
		pdev->tuning_parms.tp_phasecal_target;

	ptun_data->vl53l1_tuningparm_lite_cal_repeat_rate =
		pdev->tuning_parms.tp_cal_repeat_rate;

	ptun_data->vl53l1_tuningparm_lite_ranging_gain_factor =
		pdev->gain_cal.standard_ranging_gain_factor;

	ptun_data->vl53l1_tuningparm_lite_min_clip_mm =
		pdev->tuning_parms.tp_lite_min_clip;

	ptun_data->vl53l1_tuningparm_lite_long_sigma_thresh_mm =
		pdev->tuning_parms.tp_lite_long_sigma_thresh_mm;

	ptun_data->vl53l1_tuningparm_lite_med_sigma_thresh_mm =
		pdev->tuning_parms.tp_lite_med_sigma_thresh_mm;

	ptun_data->vl53l1_tuningparm_lite_short_sigma_thresh_mm =
		pdev->tuning_parms.tp_lite_short_sigma_thresh_mm;

	ptun_data->vl53l1_tuningparm_lite_long_min_count_rate_rtn_mcps =
		pdev->tuning_parms.tp_lite_long_min_count_rate_rtn_mcps;

	ptun_data->vl53l1_tuningparm_lite_med_min_count_rate_rtn_mcps =
		pdev->tuning_parms.tp_lite_med_min_count_rate_rtn_mcps;

	ptun_data->vl53l1_tuningparm_lite_short_min_count_rate_rtn_mcps =
		pdev->tuning_parms.tp_lite_short_min_count_rate_rtn_mcps;

	ptun_data->vl53l1_tuningparm_lite_sigma_est_pulse_width =
		pdev->tuning_parms.tp_lite_sigma_est_pulse_width_ns;

	ptun_data->vl53l1_tuningparm_lite_sigma_est_amb_width_ns =
		pdev->tuning_parms.tp_lite_sigma_est_amb_width_ns;

	ptun_data->vl53l1_tuningparm_lite_sigma_ref_mm =
		pdev->tuning_parms.tp_lite_sigma_ref_mm;

	ptun_data->vl53l1_tuningparm_lite_rit_mult =
		pdev->xtalk_cfg.crosstalk_range_ignore_threshold_mult;

	ptun_data->vl53l1_tuningparm_lite_seed_config =
		pdev->tuning_parms.tp_lite_seed_cfg;

	ptun_data->vl53l1_tuningparm_lite_quantifier =
		pdev->tuning_parms.tp_lite_quantifier;

	ptun_data->vl53l1_tuningparm_lite_first_order_select =
		pdev->tuning_parms.tp_lite_first_order_select;

	ptun_data->vl53l1_tuningparm_lite_xtalk_margin_kcps =
		pdev->xtalk_cfg.lite_mode_crosstalk_margin_kcps;

	ptun_data->vl53l1_tuningparm_initial_phase_rtn_lite_long_range =
		pdev->tuning_parms.tp_init_phase_rtn_lite_long;

	ptun_data->vl53l1_tuningparm_initial_phase_rtn_lite_med_range =
		pdev->tuning_parms.tp_init_phase_rtn_lite_med;

	ptun_data->vl53l1_tuningparm_initial_phase_rtn_lite_short_range =
		pdev->tuning_parms.tp_init_phase_rtn_lite_short;

	ptun_data->vl53l1_tuningparm_initial_phase_ref_lite_long_range =
		pdev->tuning_parms.tp_init_phase_ref_lite_long;

	ptun_data->vl53l1_tuningparm_initial_phase_ref_lite_med_range =
		pdev->tuning_parms.tp_init_phase_ref_lite_med;

	ptun_data->vl53l1_tuningparm_initial_phase_ref_lite_short_range =
		pdev->tuning_parms.tp_init_phase_ref_lite_short;

	ptun_data->vl53l1_tuningparm_timed_seed_config =
		pdev->tuning_parms.tp_timed_seed_cfg;

	ptun_data->vl53l1_tuningparm_dmax_cfg_signal_thresh_sigma =
		pdev->dmax_cfg.signal_thresh_sigma;

	ptun_data->vl53l1_tuningparm_dmax_cfg_reflectance_array_0 =
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[0];

	ptun_data->vl53l1_tuningparm_dmax_cfg_reflectance_array_1 =
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[1];

	ptun_data->vl53l1_tuningparm_dmax_cfg_reflectance_array_2 =
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[2];

	ptun_data->vl53l1_tuningparm_dmax_cfg_reflectance_array_3 =
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[3];

	ptun_data->vl53l1_tuningparm_dmax_cfg_reflectance_array_4 =
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[4];

	ptun_data->vl53l1_tuningparm_vhv_loopbound =
		pdev->stat_nvm.vhv_config__timeout_macrop_loop_bound;

	ptun_data->vl53l1_tuningparm_refspadchar_device_test_mode =
		pdev->refspadchar.device_test_mode;

	ptun_data->vl53l1_tuningparm_refspadchar_vcsel_period =
		pdev->refspadchar.VL53L1_p_009;

	ptun_data->vl53l1_tuningparm_refspadchar_phasecal_timeout_us =
		pdev->refspadchar.timeout_us;

	ptun_data->vl53l1_tuningparm_refspadchar_target_count_rate_mcps =
		pdev->refspadchar.target_count_rate_mcps;

	ptun_data->vl53l1_tuningparm_refspadchar_min_countrate_limit_mcps =
		pdev->refspadchar.min_count_rate_limit_mcps;

	ptun_data->vl53l1_tuningparm_refspadchar_max_countrate_limit_mcps =
		pdev->refspadchar.max_count_rate_limit_mcps;

	ptun_data->vl53l1_tuningparm_xtalk_extract_num_of_samples =
		pXC->num_of_samples;

	ptun_data->vl53l1_tuningparm_xtalk_extract_min_filter_thresh_mm =
		pXC->algo__crosstalk_extract_min_valid_range_mm;

	ptun_data->vl53l1_tuningparm_xtalk_extract_max_filter_thresh_mm =
		pXC->algo__crosstalk_extract_max_valid_range_mm;

	ptun_data->vl53l1_tuningparm_xtalk_extract_dss_rate_mcps =
		pXC->dss_config__target_total_rate_mcps;

	ptun_data->vl53l1_tuningparm_xtalk_extract_phasecal_timeout_us =
		pXC->phasecal_config_timeout_us;

	ptun_data->vl53l1_tuningparm_xtalk_extract_max_valid_rate_kcps =
		pXC->algo__crosstalk_extract_max_valid_rate_kcps;

	ptun_data->vl53l1_tuningparm_xtalk_extract_sigma_threshold_mm =
		pXC->algo__crosstalk_extract_max_sigma_mm;

	ptun_data->vl53l1_tuningparm_xtalk_extract_dss_timeout_us =
		pXC->mm_config_timeout_us;

	ptun_data->vl53l1_tuningparm_xtalk_extract_bin_timeout_us =
		pXC->range_config_timeout_us;

	ptun_data->vl53l1_tuningparm_offset_cal_dss_rate_mcps =
		pdev->offsetcal_cfg.dss_config__target_total_rate_mcps;

	ptun_data->vl53l1_tuningparm_offset_cal_phasecal_timeout_us =
		pdev->offsetcal_cfg.phasecal_config_timeout_us;

	ptun_data->vl53l1_tuningparm_offset_cal_mm_timeout_us =
		pdev->offsetcal_cfg.mm_config_timeout_us;

	ptun_data->vl53l1_tuningparm_offset_cal_range_timeout_us =
		pdev->offsetcal_cfg.range_config_timeout_us;

	ptun_data->vl53l1_tuningparm_offset_cal_pre_samples =
		pdev->offsetcal_cfg.pre_num_of_samples;

	ptun_data->vl53l1_tuningparm_offset_cal_mm1_samples =
		pdev->offsetcal_cfg.mm1_num_of_samples;

	ptun_data->vl53l1_tuningparm_offset_cal_mm2_samples =
		pdev->offsetcal_cfg.mm2_num_of_samples;

	ptun_data->vl53l1_tuningparm_zone_cal_dss_rate_mcps =
		pdev->zonecal_cfg.dss_config__target_total_rate_mcps;

	ptun_data->vl53l1_tuningparm_zone_cal_phasecal_timeout_us =
		pdev->zonecal_cfg.phasecal_config_timeout_us;

	ptun_data->vl53l1_tuningparm_zone_cal_dss_timeout_us =
		pdev->zonecal_cfg.mm_config_timeout_us;

	ptun_data->vl53l1_tuningparm_zone_cal_phasecal_num_samples =
		pdev->zonecal_cfg.phasecal_num_of_samples;

	ptun_data->vl53l1_tuningparm_zone_cal_range_timeout_us =
		pdev->zonecal_cfg.range_config_timeout_us;

	ptun_data->vl53l1_tuningparm_zone_cal_zone_num_samples =
		pdev->zonecal_cfg.zone_num_of_samples;

	ptun_data->vl53l1_tuningparm_spadmap_vcsel_period =
		pdev->ssc_cfg.VL53L1_p_009;

	ptun_data->vl53l1_tuningparm_spadmap_vcsel_start =
		pdev->ssc_cfg.vcsel_start;

	ptun_data->vl53l1_tuningparm_spadmap_rate_limit_mcps =
		pdev->ssc_cfg.rate_limit_mcps;

	ptun_data->vl53l1_tuningparm_lite_dss_config_target_total_rate_mcps =
		pdev->tuning_parms.tp_dss_target_lite_mcps;

	ptun_data->vl53l1_tuningparm_ranging_dss_config_target_total_rate_mcps =
		pdev->tuning_parms.tp_dss_target_histo_mcps;

	ptun_data->vl53l1_tuningparm_mz_dss_config_target_total_rate_mcps =
		pdev->tuning_parms.tp_dss_target_histo_mz_mcps;

	ptun_data->vl53l1_tuningparm_timed_dss_config_target_total_rate_mcps =
		pdev->tuning_parms.tp_dss_target_timed_mcps;

	ptun_data->vl53l1_tuningparm_lite_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_lite_us;

	ptun_data->vl53l1_tuningparm_ranging_long_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_hist_long_us;

	ptun_data->vl53l1_tuningparm_ranging_med_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_hist_med_us;

	ptun_data->vl53l1_tuningparm_ranging_short_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_hist_short_us;

	ptun_data->vl53l1_tuningparm_mz_long_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_mz_long_us;

	ptun_data->vl53l1_tuningparm_mz_med_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_mz_med_us;

	ptun_data->vl53l1_tuningparm_mz_short_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_mz_short_us;

	ptun_data->vl53l1_tuningparm_timed_phasecal_config_timeout_us =
		pdev->tuning_parms.tp_phasecal_timeout_timed_us;

	ptun_data->vl53l1_tuningparm_lite_mm_config_timeout_us =
		pdev->tuning_parms.tp_mm_timeout_lite_us;

	ptun_data->vl53l1_tuningparm_ranging_mm_config_timeout_us =
		pdev->tuning_parms.tp_mm_timeout_histo_us;

	ptun_data->vl53l1_tuningparm_mz_mm_config_timeout_us =
		pdev->tuning_parms.tp_mm_timeout_mz_us;

	ptun_data->vl53l1_tuningparm_timed_mm_config_timeout_us =
		pdev->tuning_parms.tp_mm_timeout_timed_us;

	ptun_data->vl53l1_tuningparm_lite_range_config_timeout_us =
		pdev->tuning_parms.tp_range_timeout_lite_us;

	ptun_data->vl53l1_tuningparm_ranging_range_config_timeout_us =
		pdev->tuning_parms.tp_range_timeout_histo_us;

	ptun_data->vl53l1_tuningparm_mz_range_config_timeout_us =
		pdev->tuning_parms.tp_range_timeout_mz_us;

	ptun_data->vl53l1_tuningparm_timed_range_config_timeout_us =
		pdev->tuning_parms.tp_range_timeout_timed_us;

	ptun_data->vl53l1_tuningparm_dynxtalk_smudge_margin =
		pdev->smudge_correct_config.smudge_margin;

	ptun_data->vl53l1_tuningparm_dynxtalk_noise_margin =
		pdev->smudge_correct_config.noise_margin;

	ptun_data->vl53l1_tuningparm_dynxtalk_xtalk_offset_limit =
		pdev->smudge_correct_config.user_xtalk_offset_limit;

	ptun_data->vl53l1_tuningparm_dynxtalk_xtalk_offset_limit_hi =
		pdev->smudge_correct_config.user_xtalk_offset_limit_hi;

	ptun_data->vl53l1_tuningparm_dynxtalk_sample_limit =
		pdev->smudge_correct_config.sample_limit;

	ptun_data->vl53l1_tuningparm_dynxtalk_single_xtalk_delta =
		pdev->smudge_correct_config.single_xtalk_delta;

	ptun_data->vl53l1_tuningparm_dynxtalk_averaged_xtalk_delta =
		pdev->smudge_correct_config.averaged_xtalk_delta;

	ptun_data->vl53l1_tuningparm_dynxtalk_clip_limit =
		pdev->smudge_correct_config.smudge_corr_clip_limit;

	ptun_data->vl53l1_tuningparm_dynxtalk_scaler_calc_method =
		pdev->smudge_correct_config.scaler_calc_method;

	ptun_data->vl53l1_tuningparm_dynxtalk_xgradient_scaler =
		pdev->smudge_correct_config.x_gradient_scaler;

	ptun_data->vl53l1_tuningparm_dynxtalk_ygradient_scaler =
		pdev->smudge_correct_config.y_gradient_scaler;

	ptun_data->vl53l1_tuningparm_dynxtalk_user_scaler_set =
		pdev->smudge_correct_config.user_scaler_set;

	ptun_data->vl53l1_tuningparm_dynxtalk_smudge_cor_single_apply =
		pdev->smudge_correct_config.smudge_corr_single_apply;

	ptun_data->vl53l1_tuningparm_dynxtalk_xtalk_amb_threshold =
		pdev->smudge_correct_config.smudge_corr_ambient_threshold;

	ptun_data->vl53l1_tuningparm_dynxtalk_nodetect_amb_threshold_kcps =
		pdev->smudge_correct_config.nodetect_ambient_threshold;

	ptun_data->vl53l1_tuningparm_dynxtalk_nodetect_sample_limit =
		pdev->smudge_correct_config.nodetect_sample_limit;

	ptun_data->vl53l1_tuningparm_dynxtalk_nodetect_xtalk_offset_kcps =
		pdev->smudge_correct_config.nodetect_xtalk_offset;

	ptun_data->vl53l1_tuningparm_dynxtalk_nodetect_min_range_mm =
		pdev->smudge_correct_config.nodetect_min_range_mm;

	ptun_data->vl53l1_tuningparm_lowpowerauto_vhv_loop_bound =
		pdev->low_power_auto_data.vhv_loop_bound;

	ptun_data->vl53l1_tuningparm_lowpowerauto_mm_config_timeout_us =
		pdev->tuning_parms.tp_mm_timeout_lpa_us;

	ptun_data->vl53l1_tuningparm_lowpowerauto_range_config_timeout_us =
		pdev->tuning_parms.tp_range_timeout_lpa_us;

	ptun_data->vl53l1_tuningparm_very_short_dss_rate_mcps =
		pdev->tuning_parms.tp_dss_target_very_short_mcps;

	LOG_FUNCTION_END(status);

	return status;
}







VL53L1_Error VL53L1_get_tuning_parm(
	VL53L1_DEV                     Dev,
	VL53L1_TuningParms             tuning_parm_key,
	int32_t                       *ptuning_parm_value)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_xtalkextract_config_t *pXC = &(pdev->xtalk_extract_cfg);

	LOG_FUNCTION_START("");

	switch (tuning_parm_key) {

	case VL53L1_TUNINGPARM_VERSION:
		*ptuning_parm_value =
			(int32_t)pdev->tuning_parms.tp_tuning_parm_version;
	break;
	case VL53L1_TUNINGPARM_KEY_TABLE_VERSION:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_tuning_parm_key_table_version;
	break;
	case VL53L1_TUNINGPARM_LLD_VERSION:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_tuning_parm_lld_version;
	break;
	case VL53L1_TUNINGPARM_HIST_ALGO_SELECT:
		*ptuning_parm_value =
				(int32_t)pHP->hist_algo_select;
	break;
	case VL53L1_TUNINGPARM_HIST_TARGET_ORDER:
		*ptuning_parm_value =
				(int32_t)pHP->hist_target_order;
	break;
	case VL53L1_TUNINGPARM_HIST_FILTER_WOI_0:
		*ptuning_parm_value =
				(int32_t)pHP->filter_woi0;
	break;
	case VL53L1_TUNINGPARM_HIST_FILTER_WOI_1:
		*ptuning_parm_value =
				(int32_t)pHP->filter_woi1;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_EST_METHOD:
		*ptuning_parm_value =
				(int32_t)pHP->hist_amb_est_method;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_THRESH_SIGMA_0:
		*ptuning_parm_value =
				(int32_t)pHP->ambient_thresh_sigma0;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_THRESH_SIGMA_1:
		*ptuning_parm_value =
				(int32_t)pHP->ambient_thresh_sigma1;
	break;
	case VL53L1_TUNINGPARM_HIST_MIN_AMB_THRESH_EVENTS:
		*ptuning_parm_value =
				(int32_t)pHP->min_ambient_thresh_events;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_EVENTS_SCALER:
		*ptuning_parm_value =
				(int32_t)pHP->ambient_thresh_events_scaler;
	break;
	case VL53L1_TUNINGPARM_HIST_NOISE_THRESHOLD:
		*ptuning_parm_value =
				(int32_t)pHP->noise_threshold;
	break;
	case VL53L1_TUNINGPARM_HIST_SIGNAL_TOTAL_EVENTS_LIMIT:
		*ptuning_parm_value =
				(int32_t)pHP->signal_total_events_limit;
	break;
	case VL53L1_TUNINGPARM_HIST_SIGMA_EST_REF_MM:
		*ptuning_parm_value =
				(int32_t)pHP->sigma_estimator__sigma_ref_mm;
	break;
	case VL53L1_TUNINGPARM_HIST_SIGMA_THRESH_MM:
		*ptuning_parm_value =
				(int32_t)pHP->sigma_thresh;
	break;
	case VL53L1_TUNINGPARM_HIST_GAIN_FACTOR:
		*ptuning_parm_value =
		(int32_t)pdev->gain_cal.histogram_ranging_gain_factor;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_PHASE_TOLERANCE:
		*ptuning_parm_value =
	(int32_t)pHP->algo__consistency_check__phase_tolerance;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_MIN_MAX_TOLERANCE_MM:
		*ptuning_parm_value =
	(int32_t)pHP->algo__consistency_check__min_max_tolerance;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_EVENT_SIGMA:
		*ptuning_parm_value =
		(int32_t)pHP->algo__consistency_check__event_sigma;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_EVENT_SIGMA_MIN_SPAD_LIMIT:
		*ptuning_parm_value =
		(int32_t)pHP->algo__consistency_check__event_min_spad_count;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_HISTO_LONG_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_rtn_hist_long;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_HISTO_MED_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_rtn_hist_med;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_HISTO_SHORT_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_rtn_hist_short;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_HISTO_LONG_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_ref_hist_long;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_HISTO_MED_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_ref_hist_med;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_HISTO_SHORT_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_ref_hist_short;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MIN_VALID_RANGE_MM:
		*ptuning_parm_value = (int32_t)(
		pdev->xtalk_cfg.algo__crosstalk_detect_min_valid_range_mm);
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MAX_VALID_RANGE_MM:
		*ptuning_parm_value = (int32_t)(
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_range_mm);
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MAX_SIGMA_MM:
		*ptuning_parm_value =
		(int32_t)pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MIN_MAX_TOLERANCE:
		*ptuning_parm_value =
		(int32_t)pHP->algo__crosstalk_detect_min_max_tolerance;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MAX_VALID_RATE_KCPS:
		*ptuning_parm_value = (int32_t)(
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_rate_kcps);
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_EVENT_SIGMA:
		*ptuning_parm_value =
		(int32_t)pHP->algo__crosstalk_detect_event_sigma;
	break;
	case VL53L1_TUNINGPARM_HIST_XTALK_MARGIN_KCPS:
		*ptuning_parm_value =
		(int32_t)pdev->xtalk_cfg.histogram_mode_crosstalk_margin_kcps;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_LITE_PHASE_TOLERANCE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_consistency_lite_phase_tolerance;
	break;
	case VL53L1_TUNINGPARM_PHASECAL_TARGET:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_target;
	break;
	case VL53L1_TUNINGPARM_LITE_CAL_REPEAT_RATE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_cal_repeat_rate;
	break;
	case VL53L1_TUNINGPARM_LITE_RANGING_GAIN_FACTOR:
		*ptuning_parm_value =
		(int32_t)pdev->gain_cal.standard_ranging_gain_factor;
	break;
	case VL53L1_TUNINGPARM_LITE_MIN_CLIP_MM:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_min_clip;
	break;
	case VL53L1_TUNINGPARM_LITE_LONG_SIGMA_THRESH_MM:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_long_sigma_thresh_mm;
	break;
	case VL53L1_TUNINGPARM_LITE_MED_SIGMA_THRESH_MM:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_med_sigma_thresh_mm;
	break;
	case VL53L1_TUNINGPARM_LITE_SHORT_SIGMA_THRESH_MM:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_short_sigma_thresh_mm;
	break;
	case VL53L1_TUNINGPARM_LITE_LONG_MIN_COUNT_RATE_RTN_MCPS:
		*ptuning_parm_value = (int32_t)(
		pdev->tuning_parms.tp_lite_long_min_count_rate_rtn_mcps);
	break;
	case VL53L1_TUNINGPARM_LITE_MED_MIN_COUNT_RATE_RTN_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_med_min_count_rate_rtn_mcps;
	break;
	case VL53L1_TUNINGPARM_LITE_SHORT_MIN_COUNT_RATE_RTN_MCPS:
		*ptuning_parm_value = (int32_t)(
		pdev->tuning_parms.tp_lite_short_min_count_rate_rtn_mcps);
	break;
	case VL53L1_TUNINGPARM_LITE_SIGMA_EST_PULSE_WIDTH:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_sigma_est_pulse_width_ns;
	break;
	case VL53L1_TUNINGPARM_LITE_SIGMA_EST_AMB_WIDTH_NS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_sigma_est_amb_width_ns;
	break;
	case VL53L1_TUNINGPARM_LITE_SIGMA_REF_MM:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_sigma_ref_mm;
	break;
	case VL53L1_TUNINGPARM_LITE_RIT_MULT:
		*ptuning_parm_value =
		(int32_t)pdev->xtalk_cfg.crosstalk_range_ignore_threshold_mult;
	break;
	case VL53L1_TUNINGPARM_LITE_SEED_CONFIG:
		*ptuning_parm_value =
				(int32_t)pdev->tuning_parms.tp_lite_seed_cfg;
	break;
	case VL53L1_TUNINGPARM_LITE_QUANTIFIER:
		*ptuning_parm_value =
				(int32_t)pdev->tuning_parms.tp_lite_quantifier;
	break;
	case VL53L1_TUNINGPARM_LITE_FIRST_ORDER_SELECT:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_lite_first_order_select;
	break;
	case VL53L1_TUNINGPARM_LITE_XTALK_MARGIN_KCPS:
		*ptuning_parm_value =
		(int32_t)pdev->xtalk_cfg.lite_mode_crosstalk_margin_kcps;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_LITE_LONG_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_rtn_lite_long;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_LITE_MED_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_rtn_lite_med;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_LITE_SHORT_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_rtn_lite_short;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_LITE_LONG_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_ref_lite_long;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_LITE_MED_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_ref_lite_med;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_LITE_SHORT_RANGE:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_init_phase_ref_lite_short;
	break;
	case VL53L1_TUNINGPARM_TIMED_SEED_CONFIG:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_timed_seed_cfg;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_SIGNAL_THRESH_SIGMA:
		*ptuning_parm_value =
		(int32_t)pdev->dmax_cfg.signal_thresh_sigma;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_0:
		*ptuning_parm_value =
		(int32_t)pdev->dmax_cfg.target_reflectance_for_dmax_calc[0];
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_1:
		*ptuning_parm_value =
		(int32_t)pdev->dmax_cfg.target_reflectance_for_dmax_calc[1];
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_2:
		*ptuning_parm_value =
		(int32_t)pdev->dmax_cfg.target_reflectance_for_dmax_calc[2];
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_3:
		*ptuning_parm_value =
		(int32_t)pdev->dmax_cfg.target_reflectance_for_dmax_calc[3];
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_4:
		*ptuning_parm_value =
		(int32_t)pdev->dmax_cfg.target_reflectance_for_dmax_calc[4];
	break;
	case VL53L1_TUNINGPARM_VHV_LOOPBOUND:
		*ptuning_parm_value =
		(int32_t)pdev->stat_nvm.vhv_config__timeout_macrop_loop_bound;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_DEVICE_TEST_MODE:
		*ptuning_parm_value =
		(int32_t)pdev->refspadchar.device_test_mode;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_VCSEL_PERIOD:
		*ptuning_parm_value =
		(int32_t)pdev->refspadchar.VL53L1_p_009;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_PHASECAL_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->refspadchar.timeout_us;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_TARGET_COUNT_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->refspadchar.target_count_rate_mcps;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_MIN_COUNTRATE_LIMIT_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->refspadchar.min_count_rate_limit_mcps;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_MAX_COUNTRATE_LIMIT_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->refspadchar.max_count_rate_limit_mcps;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_NUM_OF_SAMPLES:
		*ptuning_parm_value =
		(int32_t)pXC->num_of_samples;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_MIN_FILTER_THRESH_MM:
		*ptuning_parm_value =
		(int32_t)pXC->algo__crosstalk_extract_min_valid_range_mm;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_MAX_FILTER_THRESH_MM:
		*ptuning_parm_value =
		(int32_t)pXC->algo__crosstalk_extract_max_valid_range_mm;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_DSS_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pXC->dss_config__target_total_rate_mcps;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_PHASECAL_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pXC->phasecal_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_MAX_VALID_RATE_KCPS:
		*ptuning_parm_value =
		(int32_t)pXC->algo__crosstalk_extract_max_valid_rate_kcps;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_SIGMA_THRESHOLD_MM:
		*ptuning_parm_value =
		(int32_t)pXC->algo__crosstalk_extract_max_sigma_mm;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_DSS_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pXC->mm_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_BIN_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pXC->range_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_DSS_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->offsetcal_cfg.dss_config__target_total_rate_mcps;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_PHASECAL_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->offsetcal_cfg.phasecal_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_MM_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->offsetcal_cfg.mm_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_RANGE_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->offsetcal_cfg.range_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_PRE_SAMPLES:
		*ptuning_parm_value =
		(int32_t)pdev->offsetcal_cfg.pre_num_of_samples;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_MM1_SAMPLES:
		*ptuning_parm_value =
	(int32_t)pdev->offsetcal_cfg.mm1_num_of_samples;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_MM2_SAMPLES:
		*ptuning_parm_value =
		(int32_t)pdev->offsetcal_cfg.mm2_num_of_samples;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_DSS_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->zonecal_cfg.dss_config__target_total_rate_mcps;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_PHASECAL_TIMEOUT_US:
		*ptuning_parm_value =
	(int32_t)pdev->zonecal_cfg.phasecal_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_DSS_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->zonecal_cfg.mm_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_PHASECAL_NUM_SAMPLES:
		*ptuning_parm_value =
		(int32_t)pdev->zonecal_cfg.phasecal_num_of_samples;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_RANGE_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->zonecal_cfg.range_config_timeout_us;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_ZONE_NUM_SAMPLES:
		*ptuning_parm_value =
		(int32_t)pdev->zonecal_cfg.zone_num_of_samples;
	break;
	case VL53L1_TUNINGPARM_SPADMAP_VCSEL_PERIOD:
		*ptuning_parm_value =
		(int32_t)pdev->ssc_cfg.VL53L1_p_009;
	break;
	case VL53L1_TUNINGPARM_SPADMAP_VCSEL_START:
		*ptuning_parm_value =
		(int32_t)pdev->ssc_cfg.vcsel_start;
	break;
	case VL53L1_TUNINGPARM_SPADMAP_RATE_LIMIT_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->ssc_cfg.rate_limit_mcps;
	break;
	case VL53L1_TUNINGPARM_LITE_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_dss_target_lite_mcps;
	break;
	case VL53L1_TUNINGPARM_RANGING_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_dss_target_histo_mcps;
	break;
	case VL53L1_TUNINGPARM_MZ_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_dss_target_histo_mz_mcps;
	break;
	case VL53L1_TUNINGPARM_TIMED_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_dss_target_timed_mcps;
	break;
	case VL53L1_TUNINGPARM_LITE_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_lite_us;
	break;
	case VL53L1_TUNINGPARM_RANGING_LONG_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_hist_long_us;
	break;
	case VL53L1_TUNINGPARM_RANGING_MED_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_hist_med_us;
	break;
	case VL53L1_TUNINGPARM_RANGING_SHORT_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_hist_short_us;
	break;
	case VL53L1_TUNINGPARM_MZ_LONG_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_mz_long_us;
	break;
	case VL53L1_TUNINGPARM_MZ_MED_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_mz_med_us;
	break;
	case VL53L1_TUNINGPARM_MZ_SHORT_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_mz_short_us;
	break;
	case VL53L1_TUNINGPARM_TIMED_PHASECAL_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_phasecal_timeout_timed_us;
	break;
	case VL53L1_TUNINGPARM_LITE_MM_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_mm_timeout_lite_us;
	break;
	case VL53L1_TUNINGPARM_RANGING_MM_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_mm_timeout_histo_us;
	break;
	case VL53L1_TUNINGPARM_MZ_MM_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_mm_timeout_mz_us;
	break;
	case VL53L1_TUNINGPARM_TIMED_MM_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_mm_timeout_timed_us;
	break;
	case VL53L1_TUNINGPARM_LITE_RANGE_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_range_timeout_lite_us;
	break;
	case VL53L1_TUNINGPARM_RANGING_RANGE_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_range_timeout_histo_us;
	break;
	case VL53L1_TUNINGPARM_MZ_RANGE_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_range_timeout_mz_us;
	break;
	case VL53L1_TUNINGPARM_TIMED_RANGE_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_range_timeout_timed_us;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SMUDGE_MARGIN:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.smudge_margin;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NOISE_MARGIN:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.noise_margin;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XTALK_OFFSET_LIMIT:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.user_xtalk_offset_limit;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XTALK_OFFSET_LIMIT_HI:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.user_xtalk_offset_limit_hi;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SAMPLE_LIMIT:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.sample_limit;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SINGLE_XTALK_DELTA:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.single_xtalk_delta;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_AVERAGED_XTALK_DELTA:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.averaged_xtalk_delta;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_CLIP_LIMIT:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.smudge_corr_clip_limit;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SCALER_CALC_METHOD:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.scaler_calc_method;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XGRADIENT_SCALER:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.x_gradient_scaler;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_YGRADIENT_SCALER:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.y_gradient_scaler;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_USER_SCALER_SET:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.user_scaler_set;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SMUDGE_COR_SINGLE_APPLY:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.smudge_corr_single_apply;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XTALK_AMB_THRESHOLD:
		*ptuning_parm_value = (int32_t)(
		pdev->smudge_correct_config.smudge_corr_ambient_threshold);
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_AMB_THRESHOLD_KCPS:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.nodetect_ambient_threshold;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_SAMPLE_LIMIT:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.nodetect_sample_limit;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_XTALK_OFFSET_KCPS:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.nodetect_xtalk_offset;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_MIN_RANGE_MM:
		*ptuning_parm_value =
		(int32_t)pdev->smudge_correct_config.nodetect_min_range_mm;
	break;
	case VL53L1_TUNINGPARM_LOWPOWERAUTO_VHV_LOOP_BOUND:
		*ptuning_parm_value =
		(int32_t)pdev->low_power_auto_data.vhv_loop_bound;
	break;
	case VL53L1_TUNINGPARM_LOWPOWERAUTO_MM_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_mm_timeout_lpa_us;
	break;
	case VL53L1_TUNINGPARM_LOWPOWERAUTO_RANGE_CONFIG_TIMEOUT_US:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_range_timeout_lpa_us;
	break;
	case VL53L1_TUNINGPARM_VERY_SHORT_DSS_RATE_MCPS:
		*ptuning_parm_value =
		(int32_t)pdev->tuning_parms.tp_dss_target_very_short_mcps;
	break;


	default:
		*ptuning_parm_value = 0x7FFFFFFF;
		status = VL53L1_ERROR_INVALID_PARAMS;
	break;

	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_set_tuning_parm(
	VL53L1_DEV            Dev,
	VL53L1_TuningParms    tuning_parm_key,
	int32_t               tuning_parm_value)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_hist_post_process_config_t *pHP = &(pdev->histpostprocess);
	VL53L1_xtalkextract_config_t *pXC = &(pdev->xtalk_extract_cfg);

	LOG_FUNCTION_START("");

	switch (tuning_parm_key) {

	case VL53L1_TUNINGPARM_VERSION:
		pdev->tuning_parms.tp_tuning_parm_version =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_KEY_TABLE_VERSION:
		pdev->tuning_parms.tp_tuning_parm_key_table_version =
					(uint16_t)tuning_parm_value;










		if ((uint16_t)tuning_parm_value
			!= VL53L1_TUNINGPARM_KEY_TABLE_VERSION_DEFAULT)
			status = VL53L1_ERROR_TUNING_PARM_KEY_MISMATCH;

	break;
	case VL53L1_TUNINGPARM_LLD_VERSION:
		pdev->tuning_parms.tp_tuning_parm_lld_version =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_ALGO_SELECT:
		pHP->hist_algo_select =
				(VL53L1_HistAlgoSelect)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_TARGET_ORDER:
		pHP->hist_target_order =
				(VL53L1_HistTargetOrder)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_FILTER_WOI_0:
		pHP->filter_woi0 =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_FILTER_WOI_1:
		pHP->filter_woi1 =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_EST_METHOD:
		pHP->hist_amb_est_method =
				(VL53L1_HistAmbEstMethod)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_THRESH_SIGMA_0:
		pHP->ambient_thresh_sigma0 =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_THRESH_SIGMA_1:
		pHP->ambient_thresh_sigma1 =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_MIN_AMB_THRESH_EVENTS:
		pHP->min_ambient_thresh_events =
		(int32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_AMB_EVENTS_SCALER:
		pHP->ambient_thresh_events_scaler =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_NOISE_THRESHOLD:
		pHP->noise_threshold =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_SIGNAL_TOTAL_EVENTS_LIMIT:
		pHP->signal_total_events_limit =
		(int32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_SIGMA_EST_REF_MM:
		pHP->sigma_estimator__sigma_ref_mm =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_SIGMA_THRESH_MM:
		pHP->sigma_thresh =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_GAIN_FACTOR:
		pdev->gain_cal.histogram_ranging_gain_factor =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_PHASE_TOLERANCE:
		pHP->algo__consistency_check__phase_tolerance =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_MIN_MAX_TOLERANCE_MM:
		pHP->algo__consistency_check__min_max_tolerance =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_EVENT_SIGMA:
		pHP->algo__consistency_check__event_sigma =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_HIST_EVENT_SIGMA_MIN_SPAD_LIMIT:
		pHP->algo__consistency_check__event_min_spad_count =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_HISTO_LONG_RANGE:
		pdev->tuning_parms.tp_init_phase_rtn_hist_long =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_HISTO_MED_RANGE:
		pdev->tuning_parms.tp_init_phase_rtn_hist_med =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_HISTO_SHORT_RANGE:
		pdev->tuning_parms.tp_init_phase_rtn_hist_short =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_HISTO_LONG_RANGE:
		pdev->tuning_parms.tp_init_phase_ref_hist_long =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_HISTO_MED_RANGE:
		pdev->tuning_parms.tp_init_phase_ref_hist_med =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_HISTO_SHORT_RANGE:
		pdev->tuning_parms.tp_init_phase_ref_hist_short =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MIN_VALID_RANGE_MM:
		pdev->xtalk_cfg.algo__crosstalk_detect_min_valid_range_mm =
				(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MAX_VALID_RANGE_MM:
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_range_mm =
				(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MAX_SIGMA_MM:
		pdev->xtalk_cfg.algo__crosstalk_detect_max_sigma_mm =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MIN_MAX_TOLERANCE:
		pHP->algo__crosstalk_detect_min_max_tolerance =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_MAX_VALID_RATE_KCPS:
		pdev->xtalk_cfg.algo__crosstalk_detect_max_valid_rate_kcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_DETECT_EVENT_SIGMA:
		pHP->algo__crosstalk_detect_event_sigma =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_HIST_XTALK_MARGIN_KCPS:
		pdev->xtalk_cfg.histogram_mode_crosstalk_margin_kcps =
				(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_CONSISTENCY_LITE_PHASE_TOLERANCE:
		pdev->tuning_parms.tp_consistency_lite_phase_tolerance =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_PHASECAL_TARGET:
		pdev->tuning_parms.tp_phasecal_target =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_CAL_REPEAT_RATE:
		pdev->tuning_parms.tp_cal_repeat_rate =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_RANGING_GAIN_FACTOR:
		pdev->gain_cal.standard_ranging_gain_factor =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_MIN_CLIP_MM:
		pdev->tuning_parms.tp_lite_min_clip =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_LONG_SIGMA_THRESH_MM:
		pdev->tuning_parms.tp_lite_long_sigma_thresh_mm =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_MED_SIGMA_THRESH_MM:
		pdev->tuning_parms.tp_lite_med_sigma_thresh_mm =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_SHORT_SIGMA_THRESH_MM:
		pdev->tuning_parms.tp_lite_short_sigma_thresh_mm =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_LONG_MIN_COUNT_RATE_RTN_MCPS:
		pdev->tuning_parms.tp_lite_long_min_count_rate_rtn_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_MED_MIN_COUNT_RATE_RTN_MCPS:
		pdev->tuning_parms.tp_lite_med_min_count_rate_rtn_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_SHORT_MIN_COUNT_RATE_RTN_MCPS:
		pdev->tuning_parms.tp_lite_short_min_count_rate_rtn_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_SIGMA_EST_PULSE_WIDTH:
		pdev->tuning_parms.tp_lite_sigma_est_pulse_width_ns =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_SIGMA_EST_AMB_WIDTH_NS:
		pdev->tuning_parms.tp_lite_sigma_est_amb_width_ns =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_SIGMA_REF_MM:
		pdev->tuning_parms.tp_lite_sigma_ref_mm =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_RIT_MULT:
		pdev->xtalk_cfg.crosstalk_range_ignore_threshold_mult =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_SEED_CONFIG:
		pdev->tuning_parms.tp_lite_seed_cfg =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_QUANTIFIER:
		pdev->tuning_parms.tp_lite_quantifier =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_FIRST_ORDER_SELECT:
		pdev->tuning_parms.tp_lite_first_order_select =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_XTALK_MARGIN_KCPS:
		pdev->xtalk_cfg.lite_mode_crosstalk_margin_kcps =
				(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_LITE_LONG_RANGE:
		pdev->tuning_parms.tp_init_phase_rtn_lite_long =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_LITE_MED_RANGE:
		pdev->tuning_parms.tp_init_phase_rtn_lite_med =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_RTN_LITE_SHORT_RANGE:
		pdev->tuning_parms.tp_init_phase_rtn_lite_short =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_LITE_LONG_RANGE:
		pdev->tuning_parms.tp_init_phase_ref_lite_long =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_LITE_MED_RANGE:
		pdev->tuning_parms.tp_init_phase_ref_lite_med =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_INITIAL_PHASE_REF_LITE_SHORT_RANGE:
		pdev->tuning_parms.tp_init_phase_ref_lite_short =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_TIMED_SEED_CONFIG:
		pdev->tuning_parms.tp_timed_seed_cfg =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_SIGNAL_THRESH_SIGMA:
		pdev->dmax_cfg.signal_thresh_sigma =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_0:
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[0] =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_1:
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[1] =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_2:
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[2] =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_3:
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[3] =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DMAX_CFG_REFLECTANCE_ARRAY_4:
		pdev->dmax_cfg.target_reflectance_for_dmax_calc[4] =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_VHV_LOOPBOUND:
		pdev->stat_nvm.vhv_config__timeout_macrop_loop_bound =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_DEVICE_TEST_MODE:
		pdev->refspadchar.device_test_mode =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_VCSEL_PERIOD:
		pdev->refspadchar.VL53L1_p_009 =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_PHASECAL_TIMEOUT_US:
		pdev->refspadchar.timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_TARGET_COUNT_RATE_MCPS:
		pdev->refspadchar.target_count_rate_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_MIN_COUNTRATE_LIMIT_MCPS:
		pdev->refspadchar.min_count_rate_limit_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_REFSPADCHAR_MAX_COUNTRATE_LIMIT_MCPS:
		pdev->refspadchar.max_count_rate_limit_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_NUM_OF_SAMPLES:
		pXC->num_of_samples =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_MIN_FILTER_THRESH_MM:
		pXC->algo__crosstalk_extract_min_valid_range_mm =
				(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_MAX_FILTER_THRESH_MM:
		pXC->algo__crosstalk_extract_max_valid_range_mm =
				(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_DSS_RATE_MCPS:
		pXC->dss_config__target_total_rate_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_PHASECAL_TIMEOUT_US:
		pXC->phasecal_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_MAX_VALID_RATE_KCPS:
		 pXC->algo__crosstalk_extract_max_valid_rate_kcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_SIGMA_THRESHOLD_MM:
		pXC->algo__crosstalk_extract_max_sigma_mm =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_DSS_TIMEOUT_US:
		pXC->mm_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_XTALK_EXTRACT_BIN_TIMEOUT_US:
		pXC->range_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_DSS_RATE_MCPS:
		pdev->offsetcal_cfg.dss_config__target_total_rate_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_PHASECAL_TIMEOUT_US:
		pdev->offsetcal_cfg.phasecal_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_MM_TIMEOUT_US:
		pdev->offsetcal_cfg.mm_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_RANGE_TIMEOUT_US:
		pdev->offsetcal_cfg.range_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_PRE_SAMPLES:
		pdev->offsetcal_cfg.pre_num_of_samples =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_MM1_SAMPLES:
		pdev->offsetcal_cfg.mm1_num_of_samples =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_OFFSET_CAL_MM2_SAMPLES:
		pdev->offsetcal_cfg.mm2_num_of_samples =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_DSS_RATE_MCPS:
		pdev->zonecal_cfg.dss_config__target_total_rate_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_PHASECAL_TIMEOUT_US:
		pdev->zonecal_cfg.phasecal_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_DSS_TIMEOUT_US:
		pdev->zonecal_cfg.mm_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_PHASECAL_NUM_SAMPLES:
		pdev->zonecal_cfg.phasecal_num_of_samples =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_RANGE_TIMEOUT_US:
		pdev->zonecal_cfg.range_config_timeout_us =
				(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_ZONE_CAL_ZONE_NUM_SAMPLES:
		pdev->zonecal_cfg.zone_num_of_samples =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_SPADMAP_VCSEL_PERIOD:
		pdev->ssc_cfg.VL53L1_p_009 =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_SPADMAP_VCSEL_START:
		pdev->ssc_cfg.vcsel_start =
				(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_SPADMAP_RATE_LIMIT_MCPS:
		pdev->ssc_cfg.rate_limit_mcps =
				(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		pdev->tuning_parms.tp_dss_target_lite_mcps =
			(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_RANGING_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		pdev->tuning_parms.tp_dss_target_histo_mcps =
			(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_MZ_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		pdev->tuning_parms.tp_dss_target_histo_mz_mcps =
			(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_TIMED_DSS_CONFIG_TARGET_TOTAL_RATE_MCPS:
		pdev->tuning_parms.tp_dss_target_timed_mcps =
			(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_lite_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_RANGING_LONG_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_hist_long_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_RANGING_MED_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_hist_med_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_RANGING_SHORT_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_hist_short_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_MZ_LONG_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_mz_long_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_MZ_MED_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_mz_med_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_MZ_SHORT_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_mz_short_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_TIMED_PHASECAL_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_phasecal_timeout_timed_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_MM_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_mm_timeout_lite_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_RANGING_MM_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_mm_timeout_histo_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_MZ_MM_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_mm_timeout_mz_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_TIMED_MM_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_mm_timeout_timed_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LITE_RANGE_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_range_timeout_lite_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_RANGING_RANGE_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_range_timeout_histo_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_MZ_RANGE_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_range_timeout_mz_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_TIMED_RANGE_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_range_timeout_timed_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SMUDGE_MARGIN:
		pdev->smudge_correct_config.smudge_margin =
			(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NOISE_MARGIN:
		pdev->smudge_correct_config.noise_margin =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XTALK_OFFSET_LIMIT:
		pdev->smudge_correct_config.user_xtalk_offset_limit =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XTALK_OFFSET_LIMIT_HI:
		pdev->smudge_correct_config.user_xtalk_offset_limit_hi =
			(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SAMPLE_LIMIT:
		pdev->smudge_correct_config.sample_limit =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SINGLE_XTALK_DELTA:
		pdev->smudge_correct_config.single_xtalk_delta =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_AVERAGED_XTALK_DELTA:
		pdev->smudge_correct_config.averaged_xtalk_delta =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_CLIP_LIMIT:
		pdev->smudge_correct_config.smudge_corr_clip_limit =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_SCALER_CALC_METHOD:
		pdev->smudge_correct_config.scaler_calc_method =
			(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XGRADIENT_SCALER:
		pdev->smudge_correct_config.x_gradient_scaler =
			(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_YGRADIENT_SCALER:
		pdev->smudge_correct_config.y_gradient_scaler =
			(int16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_USER_SCALER_SET:
		pdev->smudge_correct_config.user_scaler_set =
			(uint8_t)tuning_parm_value;
	break;

	case VL53L1_TUNINGPARM_DYNXTALK_SMUDGE_COR_SINGLE_APPLY:
		pdev->smudge_correct_config.smudge_corr_single_apply =
			(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_XTALK_AMB_THRESHOLD:
		pdev->smudge_correct_config.smudge_corr_ambient_threshold =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_AMB_THRESHOLD_KCPS:
		pdev->smudge_correct_config.nodetect_ambient_threshold =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_SAMPLE_LIMIT:
		pdev->smudge_correct_config.nodetect_sample_limit =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_XTALK_OFFSET_KCPS:
		pdev->smudge_correct_config.nodetect_xtalk_offset =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_DYNXTALK_NODETECT_MIN_RANGE_MM:
		pdev->smudge_correct_config.nodetect_min_range_mm =
			(uint16_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LOWPOWERAUTO_VHV_LOOP_BOUND:
		pdev->low_power_auto_data.vhv_loop_bound =
			(uint8_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LOWPOWERAUTO_MM_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_mm_timeout_lpa_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_LOWPOWERAUTO_RANGE_CONFIG_TIMEOUT_US:
		pdev->tuning_parms.tp_range_timeout_lpa_us =
			(uint32_t)tuning_parm_value;
	break;
	case VL53L1_TUNINGPARM_VERY_SHORT_DSS_RATE_MCPS:
		pdev->tuning_parms.tp_dss_target_very_short_mcps =
			(uint16_t)tuning_parm_value;
	break;


	default:
		status = VL53L1_ERROR_INVALID_PARAMS;
	break;

	}

	LOG_FUNCTION_END(status);

	return status;
}







VL53L1_Error VL53L1_dynamic_xtalk_correction_enable(
	VL53L1_DEV                          Dev
	)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.smudge_corr_enabled = 1;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_dynamic_xtalk_correction_disable(
	VL53L1_DEV                          Dev
	)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.smudge_corr_enabled = 0;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_dynamic_xtalk_correction_apply_enable(
	VL53L1_DEV                          Dev
	)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.smudge_corr_apply_enabled = 1;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_dynamic_xtalk_correction_apply_disable(
	VL53L1_DEV                          Dev
	)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.smudge_corr_apply_enabled = 0;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_dynamic_xtalk_correction_single_apply_enable(
	VL53L1_DEV                          Dev
	)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.smudge_corr_single_apply = 1;

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_dynamic_xtalk_correction_single_apply_disable(
	VL53L1_DEV                          Dev
	)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.smudge_corr_single_apply = 0;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_dynamic_xtalk_correction_set_scalers(
	VL53L1_DEV	Dev,
	int16_t		x_scaler_in,
	int16_t		y_scaler_in,
	uint8_t		user_scaler_set_in
	)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->smudge_correct_config.x_gradient_scaler = x_scaler_in;
	pdev->smudge_correct_config.y_gradient_scaler = y_scaler_in;
	pdev->smudge_correct_config.user_scaler_set = user_scaler_set_in;

	LOG_FUNCTION_END(status);

	return status;
}








VL53L1_Error VL53L1_get_current_xtalk_settings(
	VL53L1_DEV                          Dev,
	VL53L1_xtalk_calibration_results_t *pxtalk
	)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pxtalk->algo__crosstalk_compensation_plane_offset_kcps =
		pdev->xtalk_cfg.algo__crosstalk_compensation_plane_offset_kcps;
	pxtalk->algo__crosstalk_compensation_x_plane_gradient_kcps =
	pdev->xtalk_cfg.algo__crosstalk_compensation_x_plane_gradient_kcps;
	pxtalk->algo__crosstalk_compensation_y_plane_gradient_kcps =
	pdev->xtalk_cfg.algo__crosstalk_compensation_y_plane_gradient_kcps;



	LOG_FUNCTION_END(status);

	return status;

}







VL53L1_Error VL53L1_set_current_xtalk_settings(
	VL53L1_DEV                          Dev,
	VL53L1_xtalk_calibration_results_t *pxtalk
	)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->xtalk_cfg.algo__crosstalk_compensation_plane_offset_kcps =
		pxtalk->algo__crosstalk_compensation_plane_offset_kcps;
	pdev->xtalk_cfg.algo__crosstalk_compensation_x_plane_gradient_kcps =
		pxtalk->algo__crosstalk_compensation_x_plane_gradient_kcps;
	pdev->xtalk_cfg.algo__crosstalk_compensation_y_plane_gradient_kcps =
		pxtalk->algo__crosstalk_compensation_y_plane_gradient_kcps;



	LOG_FUNCTION_END(status);

	return status;

}




