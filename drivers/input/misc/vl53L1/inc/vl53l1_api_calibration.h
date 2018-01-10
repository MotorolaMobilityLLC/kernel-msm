
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed,
* either 'STMicroelectronics
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




































#ifndef _VL53L1_API_CALIBRATION_H_
#define _VL53L1_API_CALIBRATION_H_

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C" {
#endif












































VL53L1_Error VL53L1_run_ref_spad_char(VL53L1_DEV Dev, VL53L1_Error *pcal_status);



















VL53L1_Error VL53L1_run_device_test(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceTestMode      device_test_mode);


























VL53L1_Error VL53L1_run_spad_rate_map(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceTestMode      device_test_mode,
	VL53L1_DeviceSscArray      array_select,
	uint32_t                   ssc_config_timeout_us,
	VL53L1_spad_rate_data_t   *pspad_rate_data);





















































































VL53L1_Error   VL53L1_run_xtalk_extraction(
	VL53L1_DEV	                        Dev,
	VL53L1_Error                       *pcal_status);
















































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
	VL53L1_Error                       *pcal_status);


























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
		VL53L1_histogram_bin_data_t  *pavg_histo);





















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
		VL53L1_histogram_bin_data_t  *pavg_histo_z4);















































VL53L1_Error   VL53L1_run_offset_calibration(
	VL53L1_DEV	                  Dev,
	int16_t                       cal_distance_mm,
	uint16_t                      cal_reflectance_pc,
	VL53L1_Error                 *pcal_status);





















VL53L1_Error   VL53L1_run_phasecal_average(
	VL53L1_DEV	            Dev,
	uint8_t                 measurement_mode,
	uint8_t                 phasecal_result__vcsel_start,
	uint16_t                phasecal_num_of_samples,
	VL53L1_range_results_t *prange_results,
	uint16_t               *pphasecal_result__reference_phase,
	uint16_t               *pzero_distance_phase);
















































VL53L1_Error VL53L1_run_zone_calibration(
	VL53L1_DEV	                  Dev,
	VL53L1_DevicePresetModes      device_preset_mode,
	VL53L1_DeviceZonePreset       zone_preset,
	VL53L1_zone_config_t         *pzone_cfg,
	int16_t                       cal_distance_mm,
	uint16_t                      cal_reflectance_pc,
	VL53L1_Error                 *pcal_status);


#ifdef __cplusplus
}
#endif

#endif


