
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




































#ifndef _VL53L1_CORE_H_
#define _VL53L1_CORE_H_

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C" {
#endif









void VL53L1_init_version(
	VL53L1_DEV         Dev);










void VL53L1_init_ll_driver_state(
	VL53L1_DEV         Dev,
	VL53L1_DeviceState ll_state);












VL53L1_Error VL53L1_update_ll_driver_rd_state(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_check_ll_driver_rd_state(
	VL53L1_DEV         Dev);












VL53L1_Error VL53L1_update_ll_driver_cfg_state(
	VL53L1_DEV         Dev);












void VL53L1_init_system_results(
	VL53L1_system_results_t      *pdata);










void V53L1_init_zone_results_structure(
	uint8_t                 active_zones,
	VL53L1_zone_results_t  *pdata);















































void VL53L1_init_histogram_config_structure(
	uint8_t   even_bin0,
	uint8_t   even_bin1,
	uint8_t   even_bin2,
	uint8_t   even_bin3,
	uint8_t   even_bin4,
	uint8_t   even_bin5,
	uint8_t   odd_bin0,
	uint8_t   odd_bin1,
	uint8_t   odd_bin2,
	uint8_t   odd_bin3,
	uint8_t   odd_bin4,
	uint8_t   odd_bin5,
	VL53L1_histogram_config_t  *pdata);











void VL53L1_init_histogram_bin_data_struct(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00013,
	VL53L1_histogram_bin_data_t *pdata);











void VL53L1_init_xtalk_bin_data_struct(
	uint32_t                       bin_value,
	uint16_t                       VL53L1_PRM_00013,
	VL53L1_xtalk_histogram_data_t *pdata);











void VL53L1_copy_xtalk_bin_data_to_histogram_data_struct(
		VL53L1_xtalk_histogram_data_t *pxtalk,
		VL53L1_histogram_bin_data_t   *phist);













void VL53L1_i2c_encode_uint16_t(
	uint16_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














uint16_t VL53L1_i2c_decode_uint16_t(
	uint16_t    count,
	uint8_t    *pbuffer);













void VL53L1_i2c_encode_int16_t(
	int16_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














int16_t VL53L1_i2c_decode_int16_t(
	uint16_t    count,
	uint8_t    *pbuffer);













void VL53L1_i2c_encode_uint32_t(
	uint32_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














uint32_t VL53L1_i2c_decode_uint32_t(
	uint16_t    count,
	uint8_t    *pbuffer);

















uint32_t VL53L1_i2c_decode_with_mask(
	uint16_t    count,
	uint8_t    *pbuffer,
	uint32_t    bit_mask,
	uint32_t    down_shift,
	uint32_t    offset);













void VL53L1_i2c_encode_int32_t(
	int32_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














int32_t VL53L1_i2c_decode_int32_t(
	uint16_t    count,
	uint8_t    *pbuffer);













VL53L1_Error VL53L1_start_test(
	VL53L1_DEV     Dev,
	uint8_t        test_mode__ctrl);
















VL53L1_Error VL53L1_set_firmware_enable_register(
	VL53L1_DEV         Dev,
	uint8_t            value);












VL53L1_Error VL53L1_enable_firmware(
	VL53L1_DEV         Dev);














VL53L1_Error VL53L1_disable_firmware(
	VL53L1_DEV         Dev);
















VL53L1_Error VL53L1_set_powerforce_register(
	VL53L1_DEV         Dev,
	uint8_t            value);


















VL53L1_Error VL53L1_enable_powerforce(
	VL53L1_DEV         Dev);















VL53L1_Error VL53L1_disable_powerforce(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_clear_interrupt(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_force_shadow_stream_count_to_zero(
	VL53L1_DEV         Dev);



















uint32_t VL53L1_calc_macro_period_us(
	uint16_t fast_osc_frequency,
	uint8_t  VL53L1_PRM_00006);











uint32_t VL53L1_calc_pll_period_us(
	uint16_t fast_osc_frequency);














uint32_t VL53L1_calc_pll_period_mm(
	uint16_t fast_osc_frequency);













uint16_t VL53L1_calc_encoded_timeout(
	uint32_t  timeout_us,
    uint32_t  macro_period_us);













uint32_t VL53L1_calc_decoded_timeout_us(
	uint16_t  timeout_encoded,
    uint32_t  macro_period_us);











uint16_t VL53L1_encode_timeout(
	uint32_t timeout_mclks);












uint32_t VL53L1_decode_timeout(
	uint16_t encoded_timeout);


















VL53L1_Error  VL53L1_calc_timeout_register_values(
	uint32_t                mm_config_timeout_us,
	uint32_t                range_config_timeout_us,
	uint16_t                fast_osc_frequency,
	VL53L1_timing_config_t *ptiming);












uint8_t VL53L1_encode_vcsel_period(
	uint8_t VL53L1_PRM_00028);












uint8_t VL53L1_decode_vcsel_period(
	uint8_t vcsel_period_reg);














uint32_t VL53L1_decode_unsigned_integer(
	uint8_t  *pbuffer,
	uint8_t   no_of_bytes);












void   VL53L1_encode_unsigned_integer(
	uint32_t  ip_value,
	uint8_t   no_of_bytes,
	uint8_t  *pbuffer);
















uint32_t VL53L1_duration_maths(
	uint32_t  pll_period_us,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  periods_elapsed_mclks);















uint16_t VL53L1_rate_maths(
	int32_t   VL53L1_PRM_00026,
	uint32_t  time_us);
















int32_t VL53L1_range_maths(
	uint16_t  fast_osc_frequency,
	uint16_t  VL53L1_PRM_00010,
	uint16_t  zero_distance_phase,
	int32_t   range_offset_mm);


















uint16_t VL53L1_rate_per_spad_maths(
	uint32_t  frac_bits,
	uint32_t  peak_count_rate,
	uint16_t  num_spads,
	uint32_t  max_output_value);
















uint32_t VL53L1_events_per_spad_maths(
	int32_t   VL53L1_PRM_00008,
	uint16_t  num_spads,
	uint32_t  duration);















void VL53L1_hist_find_min_max_bin_values(
	VL53L1_histogram_bin_data_t   *pdata);













void VL53L1_hist_remove_ambient_bins(
	VL53L1_histogram_bin_data_t    *pdata);















void VL53L1_hist_estimate_ambient_from_ambient_bins(
	VL53L1_histogram_bin_data_t    *pdata);



















void VL53L1_hist_estimate_ambient_from_thresholded_bins(
	int32_t                      ambient_threshold_sigma,
	VL53L1_histogram_bin_data_t *pdata);
















VL53L1_Error VL53L1_hist_copy_and_scale_ambient_info(
	VL53L1_histogram_bin_data_t    *pidata,
	VL53L1_histogram_bin_data_t    *podata);










void VL53L1_hist_calc_zero_distance_phase(
	VL53L1_histogram_bin_data_t    *pdata);












void  VL53L1_hist_get_bin_sequence_config(
	VL53L1_DEV                     Dev,
	VL53L1_histogram_bin_data_t   *pdata);





















VL53L1_Error  VL53L1_hist_phase_consistency_check(
	VL53L1_DEV                Dev,
	VL53L1_range_results_t   *previous,
	VL53L1_range_results_t   *pcurrent);



















VL53L1_Error  VL53L1_hist_wrap_dmax(
	VL53L1_histogram_bin_data_t  *previous,
	VL53L1_histogram_bin_data_t  *pcurrent,
	int16_t                      *pwrap_dmax_mm);















void VL53L1_hist_combine_mm1_mm2_offsets(
	int16_t   mm1_offset_mm,
	uint16_t  mm1_peak_rate_mcps,
	int16_t   mm2_offset_mm,
	uint16_t  mm2_peak_rate_mcps,
	int16_t   *prange_offset_mm);












void VL53L1_decode_row_col(
	uint8_t   spad_number,
	uint8_t  *prow,
	uint8_t  *pcol);













void VL53L1_encode_row_col(
	uint8_t  row,
	uint8_t  col,
	uint8_t *pspad_number);














void VL53L1_hist_copy_results_to_sys_and_core(
	VL53L1_histogram_bin_data_t      *pbins,
	VL53L1_range_results_t           *phist,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore);














VL53L1_Error VL53L1_sum_histogram_data (
		VL53L1_histogram_bin_data_t *phist_input,
		VL53L1_histogram_bin_data_t *phist_output);















VL53L1_Error VL53L1_avg_histogram_data (
		uint8_t no_of_samples,
		VL53L1_histogram_bin_data_t *phist_sum,
		VL53L1_histogram_bin_data_t *phist_avg);













uint32_t VL53L1_isqrt(
	uint32_t  num);












VL53L1_Error VL53L1_save_cfg_data(
	VL53L1_DEV  Dev);














VL53L1_Error VL53L1_dynamic_zone_update(
	VL53L1_DEV  Dev,
	VL53L1_range_results_t *presults);

















VL53L1_Error VL53L1_update_internal_stream_counters(
	  VL53L1_DEV  Dev,
    uint8_t     external_stream_count,
    uint8_t     *pinternal_stream_count,
    uint8_t     *pinternal_stream_count_val
    );

#ifdef __cplusplus
}
#endif

#endif

