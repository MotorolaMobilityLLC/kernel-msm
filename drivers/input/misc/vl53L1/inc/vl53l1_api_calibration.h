
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either
* 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
*License terms : STMicroelectronics Proprietary in accordance with licensing
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











































VL53L1_Error VL53L1_run_ref_spad_char(
	VL53L1_DEV     Dev);

































VL53L1_Error VL53L1_set_ref_spad_char_config(
	VL53L1_DEV    Dev,
	uint8_t       vcsel_period_a,
	uint32_t      phasecal_timeout_us,
	uint16_t      total_rate_target_mcps,
	uint16_t      max_count_rate_rtn_limit_mcps,
	uint16_t      min_count_rate_rtn_limit_mcps,
	uint16_t      fast_osc_frequency);


















VL53L1_Error VL53L1_run_device_test(
	VL53L1_DEV                 Dev,
	uint8_t                    device_test_mode);

























VL53L1_Error   VL53L1_run_xtalk_extraction(
	VL53L1_DEV	                        Dev,
	uint32_t                            mm_config_timeout_us,
	uint32_t                            range_config_timeout_us,
	uint8_t                             num_of_samples,
	uint16_t                            xtalk_filter_thresh_mm);











































VL53L1_Error VL53L1_run_xtalk_extraction_dual_reflectance(
	VL53L1_DEV	                        Dev,
	uint32_t                            mm_config_timeout_us,
	uint32_t                            range_config_timeout_us,
	uint8_t                             num_of_samples,
	uint8_t                             calc_parms,
	uint8_t                             higher_reflectance,
	uint16_t                            expected_target_distance_mm,
	uint16_t                            xtalk_filter_thresh_mm);
























VL53L1_Error VL53L1_get_and_avg_xtalk_samples(
		VL53L1_DEV	                  Dev,
		uint8_t                       num_of_samples,
		uint8_t                       measurement_mode,
		int16_t                       xtalk_filter_thresh_mm,
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
	VL53L1_DEV	      Dev,
	uint32_t          range_config_timeout_us,
	uint8_t           pre_range_num_of_samples,
	uint8_t           mm1_num_of_samples,
	uint8_t           mm2_num_of_samples,
	int32_t           target_distance_mm);


#ifdef __cplusplus
}
#endif

#endif

