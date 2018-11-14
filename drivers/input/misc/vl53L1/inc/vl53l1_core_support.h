
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




































#ifndef _VL53L1_CORE_SUPPORT_H_
#define _VL53L1_CORE_SUPPORT_H_

#include "vl53l1_types.h"
#include "vl53l1_hist_structs.h"

#ifdef __cplusplus
extern "C" {
#endif












uint32_t VL53L1_calc_pll_period_us(
	uint16_t fast_osc_frequency);


















uint32_t VL53L1_duration_maths(
	uint32_t  pll_period_us,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  periods_elapsed_mclks);
















uint32_t VL53L1_events_per_spad_maths(
	int32_t   VL53L1_PRM_00012,
	uint16_t  num_spads,
	uint32_t  duration);













uint32_t VL53L1_isqrt(
	uint32_t  num);










void VL53L1_hist_calc_zero_distance_phase(
	VL53L1_histogram_bin_data_t    *pdata);



















void VL53L1_hist_estimate_ambient_from_thresholded_bins(
	int32_t                      ambient_threshold_sigma,
	VL53L1_histogram_bin_data_t *pdata);













void VL53L1_hist_remove_ambient_bins(
	VL53L1_histogram_bin_data_t    *pdata);














uint32_t VL53L1_calc_pll_period_mm(
	uint16_t fast_osc_frequency);















uint16_t VL53L1_rate_maths(
	int32_t   VL53L1_PRM_00007,
	uint32_t  time_us);


















uint16_t VL53L1_rate_per_spad_maths(
	uint32_t  frac_bits,
	uint32_t  peak_count_rate,
	uint16_t  num_spads,
	uint32_t  max_output_value);



















int32_t VL53L1_range_maths(
	uint16_t  fast_osc_frequency,
	uint16_t  VL53L1_PRM_00016,
	uint16_t  zero_distance_phase,
	uint8_t   fractional_bits,
	int32_t   gain_factor,
	int32_t   range_offset_mm);












uint8_t VL53L1_decode_vcsel_period(
	uint8_t vcsel_period_reg);











void VL53L1_copy_xtalk_bin_data_to_histogram_data_struct(
		VL53L1_xtalk_histogram_shape_t *pxtalk,
		VL53L1_histogram_bin_data_t    *phist);











void VL53L1_init_histogram_bin_data_struct(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00021,
	VL53L1_histogram_bin_data_t *pdata);












void VL53L1_decode_row_col(
	uint8_t   spad_number,
	uint8_t  *prow,
	uint8_t  *pcol);















void VL53L1_hist_find_min_max_bin_values(
	VL53L1_histogram_bin_data_t   *pdata);















void VL53L1_hist_estimate_ambient_from_ambient_bins(
	VL53L1_histogram_bin_data_t    *pdata);


#ifdef __cplusplus
}
#endif

#endif


