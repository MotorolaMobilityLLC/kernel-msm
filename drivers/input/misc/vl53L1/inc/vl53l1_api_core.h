
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




































#ifndef _VL53L1_API_CORE_H_
#define _VL53L1_API_CORE_H_

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C" {
#endif












VL53L1_Error VL53L1_get_version(
	VL53L1_DEV            Dev,
	VL53L1_ll_version_t  *pversion);












VL53L1_Error VL53L1_get_device_firmware_version(
	VL53L1_DEV         Dev,
	uint16_t          *pfw_version);
















VL53L1_Error VL53L1_data_init(
	VL53L1_DEV         Dev,
	uint8_t            read_p2p_data);

















VL53L1_Error VL53L1_read_p2p_data(
	VL53L1_DEV      Dev);













VL53L1_Error VL53L1_software_reset(
	VL53L1_DEV      Dev);



















VL53L1_Error VL53L1_set_part_to_part_data(
	VL53L1_DEV                            Dev,
	VL53L1_calibration_data_t            *pcal_data);
















VL53L1_Error VL53L1_get_part_to_part_data(
	VL53L1_DEV                            Dev,
	VL53L1_calibration_data_t            *pcal_data);
















VL53L1_Error VL53L1_get_tuning_debug_data(
	VL53L1_DEV                            Dev,
	VL53L1_tuning_parameters_t            *ptun_data);















VL53L1_Error VL53L1_set_inter_measurement_period_ms(
	VL53L1_DEV          Dev,
	uint32_t            inter_measurement_period_ms);















VL53L1_Error VL53L1_get_inter_measurement_period_ms(
	VL53L1_DEV          Dev,
	uint32_t           *pinter_measurement_period_ms);


















VL53L1_Error VL53L1_set_timeouts_us(
	VL53L1_DEV          Dev,
	uint32_t            phasecal_config_timeout_us,
	uint32_t            mm_config_timeout_us,
	uint32_t            range_config_timeout_us);


















VL53L1_Error VL53L1_get_timeouts_us(
	VL53L1_DEV          Dev,
	uint32_t           *pphasecal_config_timeout_us,
	uint32_t           *pmm_config_timeout_us,
	uint32_t           *prange_config_timeout_us);






















VL53L1_Error VL53L1_set_calibration_repeat_period(
	VL53L1_DEV          Dev,
	uint16_t            cal_config__repeat_period);













VL53L1_Error VL53L1_get_calibration_repeat_period(
	VL53L1_DEV          Dev,
	uint16_t           *pcal_config__repeat_period);














VL53L1_Error VL53L1_set_sequence_config_bit(
	VL53L1_DEV                   Dev,
	VL53L1_DeviceSequenceConfig  bit_id,
	uint8_t                      value);














VL53L1_Error VL53L1_get_sequence_config_bit(
	VL53L1_DEV                   Dev,
	VL53L1_DeviceSequenceConfig  bit_id,
	uint8_t                     *pvalue);













VL53L1_Error VL53L1_set_interrupt_polarity(
	VL53L1_DEV                       Dev,
	VL53L1_DeviceInterruptPolarity  interrupt_polarity);













VL53L1_Error VL53L1_get_interrupt_polarity(
	VL53L1_DEV                      Dev,
	VL53L1_DeviceInterruptPolarity  *pinterrupt_polarity);












VL53L1_Error VL53L1_get_refspadchar_config_struct(
	VL53L1_DEV                     Dev,
	VL53L1_refspadchar_config_t   *pdata);












VL53L1_Error VL53L1_set_refspadchar_config_struct(
	VL53L1_DEV                     Dev,
	VL53L1_refspadchar_config_t   *pdata);














VL53L1_Error VL53L1_set_range_ignore_threshold(
	VL53L1_DEV              Dev,
	uint8_t                 range_ignore_thresh_mult,
	uint16_t                range_ignore_threshold_mcps);






















VL53L1_Error VL53L1_get_range_ignore_threshold(
	VL53L1_DEV              Dev,
	uint8_t                *prange_ignore_thresh_mult,
	uint16_t               *prange_ignore_threshold_mcps_internal,
	uint16_t               *prange_ignore_threshold_mcps_current);













VL53L1_Error VL53L1_set_user_zone(
	VL53L1_DEV          Dev,
	VL53L1_user_zone_t *puser_zone);













VL53L1_Error VL53L1_get_user_zone(
	VL53L1_DEV          Dev,
	VL53L1_user_zone_t *puser_zone);













VL53L1_Error VL53L1_get_mode_mitigation_roi(
	VL53L1_DEV          Dev,
	VL53L1_user_zone_t *pmm_roi);


















VL53L1_Error VL53L1_set_zone_config(
	VL53L1_DEV             Dev,
	VL53L1_zone_config_t  *pzone_cfg);















VL53L1_Error VL53L1_get_zone_config(
	VL53L1_DEV             Dev,
	VL53L1_zone_config_t  *pzone_cfg);

































VL53L1_Error VL53L1_set_preset_mode(
	VL53L1_DEV                   Dev,
	VL53L1_DevicePresetModes     device_preset_mode,
	uint16_t                     dss_config__target_total_rate_mcps,
	uint32_t                     phasecal_config_timeout_us,
	uint32_t                     mm_config_timeout_us,
	uint32_t                     range_config_timeout_us,
	uint32_t                     inter_measurement_period_ms);



















VL53L1_Error VL53L1_get_preset_mode_timing_cfg(
	VL53L1_DEV                   Dev,
	VL53L1_DevicePresetModes     device_preset_mode,
	uint16_t                    *pdss_config__target_total_rate_mcps,
	uint32_t                    *pphasecal_config_timeout_us,
	uint32_t                    *pmm_config_timeout_us,
	uint32_t                    *prange_config_timeout_us);












VL53L1_Error VL53L1_set_zone_preset(
	VL53L1_DEV               Dev,
	VL53L1_DeviceZonePreset  zone_preset);














VL53L1_Error VL53L1_enable_xtalk_compensation(
	VL53L1_DEV                 Dev);














VL53L1_Error VL53L1_disable_xtalk_compensation(
	VL53L1_DEV                 Dev);














void VL53L1_get_xtalk_compensation_enable(
	VL53L1_DEV    Dev,
	uint8_t       *pcrosstalk_compensation_enable);













































VL53L1_Error VL53L1_init_and_start_range(
	VL53L1_DEV                      Dev,
	uint8_t                         measurement_mode,
	VL53L1_DeviceConfigLevel        device_config_level);













VL53L1_Error VL53L1_stop_range(
	VL53L1_DEV  Dev);




























VL53L1_Error VL53L1_get_measurement_results(
	VL53L1_DEV                  Dev,
	VL53L1_DeviceResultsLevel   device_result_level);




























VL53L1_Error VL53L1_get_device_results(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceResultsLevel  device_result_level,
	VL53L1_range_results_t    *prange_results);




























VL53L1_Error VL53L1_clear_interrupt_and_enable_next_range(
	VL53L1_DEV       Dev,
	uint8_t          measurement_mode);










VL53L1_Error VL53L1_get_histogram_bin_data(
	VL53L1_DEV                   Dev,
	VL53L1_histogram_bin_data_t *phist_data);












void VL53L1_copy_sys_and_core_results_to_range_results(
	int32_t                           gain_factor,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore,
	VL53L1_range_results_t           *presults);









VL53L1_Error VL53L1_set_zone_dss_config(
	  VL53L1_DEV                      Dev,
	  VL53L1_zone_private_dyn_cfg_t  *pzone_dyn_cfg);
























VL53L1_Error VL53L1_calc_ambient_dmax(
	VL53L1_DEV    Dev,
	uint16_t      target_reflectance,
	int16_t      *pambient_dmax_mm);


















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
	);









VL53L1_Error VL53L1_set_GPIO_interrupt_config_struct(
	VL53L1_DEV                      Dev,
	VL53L1_GPIO_interrupt_config_t	intconf);










VL53L1_Error VL53L1_get_GPIO_interrupt_config(
	VL53L1_DEV                      Dev,
	VL53L1_GPIO_interrupt_config_t	*pintconf);














VL53L1_Error VL53L1_set_dmax_mode(
	VL53L1_DEV              Dev,
	VL53L1_DeviceDmaxMode   dmax_mode);












VL53L1_Error VL53L1_get_dmax_mode(
	VL53L1_DEV               Dev,
	VL53L1_DeviceDmaxMode   *pdmax_mode);
















VL53L1_Error VL53L1_get_dmax_calibration_data(
	VL53L1_DEV                      Dev,
	VL53L1_DeviceDmaxMode           dmax_mode,
	uint8_t                         zone_id,
	VL53L1_dmax_calibration_data_t *pdmax_cal);














VL53L1_Error VL53L1_set_hist_dmax_config(
	VL53L1_DEV                      Dev,
	VL53L1_hist_gen3_dmax_config_t *pdmax_cfg);













VL53L1_Error VL53L1_get_hist_dmax_config(
	VL53L1_DEV                      Dev,
	VL53L1_hist_gen3_dmax_config_t *pdmax_cfg);













VL53L1_Error VL53L1_set_offset_calibration_mode(
	VL53L1_DEV                      Dev,
	VL53L1_OffsetCalibrationMode   offset_cal_mode);













VL53L1_Error VL53L1_get_offset_calibration_mode(
	VL53L1_DEV                      Dev,
	VL53L1_OffsetCalibrationMode  *poffset_cal_mode);













VL53L1_Error VL53L1_set_offset_correction_mode(
	VL53L1_DEV                     Dev,
	VL53L1_OffsetCalibrationMode   offset_cor_mode);













VL53L1_Error VL53L1_get_offset_correction_mode(
	VL53L1_DEV                    Dev,
	VL53L1_OffsetCorrectionMode  *poffset_cor_mode);













VL53L1_Error VL53L1_set_zone_calibration_data(
	VL53L1_DEV                         Dev,
	VL53L1_zone_calibration_results_t *pzone_cal);













VL53L1_Error VL53L1_get_zone_calibration_data(
	VL53L1_DEV                         Dev,
	VL53L1_zone_calibration_results_t *pzone_cal);















VL53L1_Error VL53L1_get_lite_xtalk_margin_kcps(
	VL53L1_DEV                          Dev,
	int16_t                           *pxtalk_margin);














VL53L1_Error VL53L1_set_lite_xtalk_margin_kcps(
	VL53L1_DEV                          Dev,
	int16_t                             xtalk_margin);















VL53L1_Error VL53L1_get_histogram_xtalk_margin_kcps(
	VL53L1_DEV                          Dev,
	int16_t                           *pxtalk_margin);














VL53L1_Error VL53L1_set_histogram_xtalk_margin_kcps(
	VL53L1_DEV                          Dev,
	int16_t                             xtalk_margin);













VL53L1_Error VL53L1_get_histogram_phase_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                            *pphase_consistency);













VL53L1_Error VL53L1_set_histogram_phase_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                             phase_consistency);















VL53L1_Error VL53L1_get_histogram_event_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                            *pevent_consistency);















VL53L1_Error VL53L1_set_histogram_event_consistency(
	VL53L1_DEV                          Dev,
	uint8_t                             event_consistency);















VL53L1_Error VL53L1_get_histogram_ambient_threshold_sigma(
	VL53L1_DEV                          Dev,
	uint8_t                            *pamb_thresh_sigma);















VL53L1_Error VL53L1_set_histogram_ambient_threshold_sigma(
	VL53L1_DEV                          Dev,
	uint8_t                             amb_thresh_sigma);



















VL53L1_Error VL53L1_get_lite_min_count_rate(
	VL53L1_DEV                          Dev,
	uint16_t                           *plite_mincountrate);




















VL53L1_Error VL53L1_set_lite_min_count_rate(
	VL53L1_DEV                          Dev,
	uint16_t                            lite_mincountrate);





















VL53L1_Error VL53L1_get_lite_sigma_threshold(
	VL53L1_DEV                          Dev,
	uint16_t                           *plite_sigma);




















VL53L1_Error VL53L1_set_lite_sigma_threshold(
	VL53L1_DEV                          Dev,
	uint16_t                            lite_sigma);


















VL53L1_Error VL53L1_restore_xtalk_nvm_default(
	VL53L1_DEV                     Dev);



















VL53L1_Error VL53L1_get_xtalk_detect_config(
	VL53L1_DEV                          Dev,
	int16_t                            *pmax_valid_range_mm,
	int16_t                            *pmin_valid_range_mm,
	uint16_t                           *pmax_valid_rate_kcps,
	uint16_t                           *pmax_sigma_mm);



















VL53L1_Error VL53L1_set_xtalk_detect_config(
	VL53L1_DEV                          Dev,
	int16_t                             max_valid_range_mm,
	int16_t                             min_valid_range_mm,
	uint16_t                            max_valid_rate_kcps,
	uint16_t                            max_sigma_mm);














VL53L1_Error VL53L1_get_target_order_mode(
	VL53L1_DEV                          Dev,
	VL53L1_HistTargetOrder             *phist_target_order);













VL53L1_Error VL53L1_set_target_order_mode(
	VL53L1_DEV                          Dev,
	VL53L1_HistTargetOrder              hist_target_order);














VL53L1_Error VL53L1_set_dmax_reflectance_values(
	VL53L1_DEV                          Dev,
	VL53L1_dmax_reflectance_array_t    *pdmax_reflectances);













VL53L1_Error VL53L1_get_dmax_reflectance_values(
	VL53L1_DEV                          Dev,
	VL53L1_dmax_reflectance_array_t    *pdmax_reflectances);

















VL53L1_Error VL53L1_set_vhv_config(
	VL53L1_DEV                   Dev,
	uint8_t                      vhv_init_en,
	uint8_t                      vhv_init_value);

















VL53L1_Error VL53L1_get_vhv_config(
	VL53L1_DEV                   Dev,
	uint8_t                     *pvhv_init_en,
	uint8_t                     *pvhv_init_value);


















VL53L1_Error VL53L1_set_vhv_loopbound(
	VL53L1_DEV                   Dev,
	uint8_t                      vhv_loopbound);


















VL53L1_Error VL53L1_get_vhv_loopbound(
	VL53L1_DEV                   Dev,
	uint8_t                     *pvhv_loopbound);























VL53L1_Error VL53L1_get_tuning_parm(
	VL53L1_DEV                     Dev,
	VL53L1_TuningParms             tuning_parm_key,
	int32_t                       *ptuning_parm_value);























VL53L1_Error VL53L1_set_tuning_parm(
	VL53L1_DEV                     Dev,
	VL53L1_TuningParms             tuning_parm_key,
	int32_t                        tuning_parm_value);















VL53L1_Error VL53L1_dynamic_xtalk_correction_enable(
	VL53L1_DEV                     Dev
	);















VL53L1_Error VL53L1_dynamic_xtalk_correction_disable(
	VL53L1_DEV                     Dev
	);
















VL53L1_Error VL53L1_dynamic_xtalk_correction_apply_enable(
	VL53L1_DEV                          Dev
	);















VL53L1_Error VL53L1_dynamic_xtalk_correction_apply_disable(
	VL53L1_DEV                          Dev
	);















VL53L1_Error VL53L1_dynamic_xtalk_correction_single_apply_enable(
	VL53L1_DEV                          Dev
	);















VL53L1_Error VL53L1_dynamic_xtalk_correction_single_apply_disable(
	VL53L1_DEV                          Dev
	);



















VL53L1_Error VL53L1_dynamic_xtalk_correction_set_scalers(
	VL53L1_DEV	Dev,
	int16_t		x_scaler_in,
	int16_t		y_scaler_in,
	uint8_t		user_scaler_set_in
	);
















VL53L1_Error VL53L1_get_current_xtalk_settings(
	VL53L1_DEV                          Dev,
	VL53L1_xtalk_calibration_results_t *pxtalk
	);
















VL53L1_Error VL53L1_set_current_xtalk_settings(
	VL53L1_DEV                          Dev,
	VL53L1_xtalk_calibration_results_t *pxtalk
	);

#ifdef __cplusplus
}
#endif

#endif


