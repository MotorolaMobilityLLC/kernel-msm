
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






































#ifndef _VL53L1_LL_DEF_H_
#define _VL53L1_LL_DEF_H_

#include "vl53l1_error_codes.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_platform_user_config.h"
#include "vl53l1_platform_user_defines.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_dmax_structs.h"
#include "vl53l1_error_exceptions.h"

#ifdef __cplusplus
extern "C" {
#endif









#define VL53L1_LL_API_IMPLEMENTATION_VER_MAJOR       1


#define VL53L1_LL_API_IMPLEMENTATION_VER_MINOR       1


#define VL53L1_LL_API_IMPLEMENTATION_VER_SUB         16


#define VL53L1_LL_API_IMPLEMENTATION_VER_REVISION  11634

#define VL53L1_LL_API_IMPLEMENTATION_VER_STRING "1.1.16.11634"



#define VL53L1_FIRMWARE_VER_MINIMUM         398
#define VL53L1_FIRMWARE_VER_MAXIMUM         400







#define VL53L1_LL_CALIBRATION_DATA_STRUCT_VERSION  0xECAB0101



#define VL53L1_MAX_XTALK_RANGE_RESULTS        5






#define VL53L1_MAX_OFFSET_RANGE_RESULTS       3





#define VL53L1_NVM_MAX_FMT_RANGE_DATA         4



#define VL53L1_NVM_PEAK_RATE_MAP_SAMPLES  25


#define VL53L1_NVM_PEAK_RATE_MAP_WIDTH     5


#define VL53L1_NVM_PEAK_RATE_MAP_HEIGHT     5









#define VL53L1_ERROR_DEVICE_FIRMWARE_TOO_OLD           ((VL53L1_Error) - 80)


#define VL53L1_ERROR_DEVICE_FIRMWARE_TOO_NEW           ((VL53L1_Error) - 85)


#define VL53L1_ERROR_UNIT_TEST_FAIL                    ((VL53L1_Error) - 90)


#define VL53L1_ERROR_FILE_READ_FAIL                    ((VL53L1_Error) - 95)


#define VL53L1_ERROR_FILE_WRITE_FAIL                   ((VL53L1_Error) - 96)












typedef struct {
	uint32_t     ll_revision;

	uint8_t      ll_major;

	uint8_t      ll_minor;

	uint8_t      ll_build;

} VL53L1_ll_version_t;






typedef struct {

	uint8_t    device_test_mode;

	uint8_t    VL53L1_PRM_00007;

	uint32_t   timeout_us;

	uint16_t   target_count_rate_mcps;


	uint16_t   min_count_rate_limit_mcps;


	uint16_t   max_count_rate_limit_mcps;



} VL53L1_refspadchar_config_t;







typedef struct {

	VL53L1_DeviceSscArray  array_select;




	uint8_t    VL53L1_PRM_00007;


	uint8_t    vcsel_start;


	uint8_t    vcsel_width;


	uint32_t   timeout_us;


	uint16_t   rate_limit_mcps;





} VL53L1_ssc_config_t;






typedef struct {


	uint16_t  algo__crosstalk_compensation_plane_offset_kcps;


	int16_t   algo__crosstalk_compensation_x_plane_gradient_kcps;


	int16_t   algo__crosstalk_compensation_y_plane_gradient_kcps;


	uint16_t  nvm_default__crosstalk_compensation_plane_offset_kcps;


	int16_t   nvm_default__crosstalk_compensation_x_plane_gradient_kcps;


	int16_t   nvm_default__crosstalk_compensation_y_plane_gradient_kcps;


	uint8_t   global_crosstalk_compensation_enable;


	int16_t   histogram_mode_crosstalk_margin_kcps;






	int16_t   lite_mode_crosstalk_margin_kcps;






	uint8_t   crosstalk_range_ignore_threshold_mult;


	uint16_t  crosstalk_range_ignore_threshold_rate_mcps;




	int16_t   algo__crosstalk_detect_min_valid_range_mm;


	int16_t   algo__crosstalk_detect_max_valid_range_mm;


	uint16_t  algo__crosstalk_detect_max_valid_rate_kcps;



	uint16_t  algo__crosstalk_detect_max_sigma_mm;





} VL53L1_xtalk_config_t;







typedef struct {

	uint8_t   x_centre;

	uint8_t   y_centre;

	uint8_t   width;

	uint8_t   height;


} VL53L1_user_zone_t;









typedef struct {

	uint8_t             max_zones;

	uint8_t             active_zones;









VL53L1_histogram_config_t multizone_hist_cfg;

	VL53L1_user_zone_t user_zones[VL53L1_MAX_USER_ZONES];



	uint8_t bin_config[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_config_t;









typedef struct {



	VL53L1_GPIO_Interrupt_Mode	intr_mode_distance;



	VL53L1_GPIO_Interrupt_Mode	intr_mode_rate;





	uint8_t				intr_new_measure_ready;



	uint8_t				intr_no_target;






	uint8_t				intr_combined_mode;










	uint16_t			threshold_distance_high;



	uint16_t			threshold_distance_low;



	uint16_t			threshold_rate_high;



	uint16_t			threshold_rate_low;

} VL53L1_GPIO_interrupt_config_t;








typedef struct {




	uint8_t  range_id;


	uint32_t time_stamp;


	uint8_t  VL53L1_PRM_00010;


	uint8_t  VL53L1_PRM_00015;


	uint8_t  VL53L1_PRM_00019;


	uint8_t  VL53L1_PRM_00020;


	uint8_t  VL53L1_PRM_00011;


	uint8_t  VL53L1_PRM_00021;



	uint16_t   width;


	uint8_t    VL53L1_PRM_00027;




	uint16_t   fast_osc_frequency;


	uint16_t   zero_distance_phase;


	uint16_t   VL53L1_PRM_00003;



	uint32_t   total_periods_elapsed;



	uint32_t   peak_duration_us;



	uint32_t   woi_duration_us;







	uint32_t   VL53L1_PRM_00023;


	uint32_t   VL53L1_PRM_00022;



	int32_t    VL53L1_PRM_00009;







	uint16_t    peak_signal_count_rate_mcps;


	uint16_t    avg_signal_count_rate_mcps;


	uint16_t    ambient_count_rate_mcps;


	uint16_t    total_rate_per_spad_mcps;


	uint32_t    VL53L1_PRM_00008;






	uint16_t   VL53L1_PRM_00004;






	uint16_t   VL53L1_PRM_00024;



	uint16_t   VL53L1_PRM_00014;


	uint16_t   VL53L1_PRM_00025;







	int16_t    min_range_mm;





	int16_t    median_range_mm;




	int16_t    max_range_mm;









	uint8_t    range_status;

} VL53L1_range_data_t;










typedef struct {

	VL53L1_DeviceState     cfg_device_state;


	VL53L1_DeviceState     rd_device_state;


	uint8_t                zone_id;


	uint8_t                stream_count;



	int16_t                ambient_dmax_mm[VL53L1_MAX_AMBIENT_DMAX_VALUES];




	int16_t                wrap_dmax_mm;



	uint8_t                max_results;



	uint8_t                active_results;


	VL53L1_range_data_t    VL53L1_PRM_00005[VL53L1_MAX_RANGE_RESULTS];




} VL53L1_range_results_t;










typedef struct {

	uint8_t    no_of_samples;


	uint32_t   rate_per_spad_kcps_sum;



	uint32_t   rate_per_spad_kcps_avg;


	int32_t    signal_total_events_sum;



	int32_t    signal_total_events_avg;



	uint32_t   sigma_mm_sum;



	uint32_t   sigma_mm_avg;



} VL53L1_xtalk_range_data_t;










typedef struct {

	VL53L1_Error                cal_status;


	uint8_t                     num_of_samples_status;








	uint8_t                     zero_samples_status;








	uint8_t                     max_sigma_status;


















	uint8_t                     max_results;



	uint8_t                     active_results;



	VL53L1_xtalk_range_data_t   VL53L1_PRM_00005[VL53L1_MAX_XTALK_RANGE_RESULTS];


	VL53L1_histogram_bin_data_t central_histogram_sum;



	VL53L1_histogram_bin_data_t central_histogram_avg;



	uint8_t central_histogram__window_start;



	uint8_t central_histogram__window_end;



	VL53L1_histogram_bin_data_t histogram_avg_1[VL53L1_MAX_XTALK_RANGE_RESULTS];



	VL53L1_histogram_bin_data_t histogram_avg_2[VL53L1_MAX_XTALK_RANGE_RESULTS];



	VL53L1_histogram_bin_data_t xtalk_avg[VL53L1_MAX_XTALK_RANGE_RESULTS];




} VL53L1_xtalk_range_results_t;











typedef struct {

	uint8_t    preset_mode;


	uint8_t    dss_config__roi_mode_control;


	uint16_t   dss_config__manual_effective_spads_select;


	uint8_t    no_of_samples;


	uint32_t   effective_spads;


	uint32_t   peak_rate_mcps;


	uint32_t   VL53L1_PRM_00004;


	int32_t    median_range_mm;



	int32_t    range_mm_offset;



} VL53L1_offset_range_data_t;










typedef struct {

	int16_t      cal_distance_mm;


	uint16_t     cal_reflectance_pc;


	VL53L1_Error cal_status;


	uint8_t      cal_report;


	uint8_t      max_results;



	uint8_t      active_results;


	VL53L1_offset_range_data_t VL53L1_PRM_00005[VL53L1_MAX_OFFSET_RANGE_RESULTS];



} VL53L1_offset_range_results_t;












typedef struct {

	uint16_t  result__mm_inner_actual_effective_spads;


	uint16_t  result__mm_outer_actual_effective_spads;


	uint16_t  result__mm_inner_peak_signal_count_rtn_mcps;


	uint16_t  result__mm_outer_peak_signal_count_rtn_mcps;



} VL53L1_additional_offset_cal_data_t;










typedef struct {

	uint32_t   VL53L1_PRM_00023;


	uint32_t   VL53L1_PRM_00022;



	uint16_t   VL53L1_PRM_00014;



} VL53L1_object_data_t;










typedef struct {

	VL53L1_DeviceState     cfg_device_state;


	VL53L1_DeviceState     rd_device_state;


	uint8_t                zone_id;


	uint8_t                stream_count;



	uint8_t                max_objects;



	uint8_t                active_objects;


	VL53L1_object_data_t   VL53L1_PRM_00005[VL53L1_MAX_RANGE_RESULTS];



} VL53L1_zone_objects_t;










typedef struct {

	uint8_t                max_zones;



	uint8_t                active_zones;


	VL53L1_zone_objects_t VL53L1_PRM_00005[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_results_t;









typedef struct {

	VL53L1_DeviceState     rd_device_state;



	uint8_t  number_of_ambient_bins;




	uint16_t result__dss_actual_effective_spads;


	uint8_t  VL53L1_PRM_00007;


	uint32_t total_periods_elapsed;



	int32_t  ambient_events_sum;



} VL53L1_zone_hist_info_t;










typedef struct {

	uint8_t                     max_zones;



	uint8_t                     active_zones;


	VL53L1_zone_hist_info_t     VL53L1_PRM_00005[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_histograms_t;









typedef struct {

	uint32_t   no_of_samples;


	uint32_t   effective_spads;


	uint32_t   peak_rate_mcps;


	uint32_t   VL53L1_PRM_00014;


	uint32_t   VL53L1_PRM_00004;



	int32_t    median_range_mm;



	int32_t    range_mm_offset;



} VL53L1_zone_calibration_data_t;








typedef struct {

	VL53L1_DevicePresetModes         preset_mode;


	VL53L1_DeviceZonePreset          zone_preset;


	int16_t                          cal_distance_mm;


	uint16_t                         cal_reflectance_pc;


	uint16_t                         phasecal_result__reference_phase;


	uint16_t                         zero_distance_phase;


	VL53L1_Error                     cal_status;


	uint8_t                          max_zones;



	uint8_t                          active_zones;


	VL53L1_zone_calibration_data_t   VL53L1_PRM_00005[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_calibration_results_t;











typedef struct {

	int16_t     cal_distance_mm;


	uint16_t    cal_reflectance_pc;


	uint16_t    max_samples;


	uint16_t    width;


	uint16_t    height;


	uint16_t    peak_rate_mcps[VL53L1_NVM_PEAK_RATE_MAP_SAMPLES];



} VL53L1_cal_peak_rate_map_t;









typedef struct {

	uint8_t      expected_stream_count;


	uint8_t      expected_gph_id;


	uint8_t      dss_mode;


	uint16_t     dss_requested_effective_spad_count;



	uint8_t      seed_cfg;


	uint8_t      initial_phase_seed;



	uint8_t  roi_config__user_roi_centre_spad;


	uint8_t  roi_config__user_roi_requested_global_xy_size;



} VL53L1_zone_private_dyn_cfg_t;









typedef struct {

	uint8_t                     max_zones;



	uint8_t                     active_zones;


	VL53L1_zone_private_dyn_cfg_t VL53L1_PRM_00005[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_private_dyn_cfgs_t;









typedef struct {

	uint16_t  algo__crosstalk_compensation_plane_offset_kcps;


	int16_t   algo__crosstalk_compensation_x_plane_gradient_kcps;


	int16_t   algo__crosstalk_compensation_y_plane_gradient_kcps;



} VL53L1_xtalk_calibration_results_t;










typedef struct {

	uint16_t   standard_ranging_gain_factor;


	uint16_t   histogram_ranging_gain_factor;



} VL53L1_gain_calibration_data_t;










typedef struct {

	VL53L1_DeviceState   cfg_device_state;


	uint8_t   cfg_stream_count;



	uint8_t   cfg_internal_stream_count;


	uint8_t   cfg_internal_stream_count_val;


	uint8_t   cfg_gph_id;


	uint8_t   cfg_timing_status;


	uint8_t   cfg_zone_id;



	VL53L1_DeviceState   rd_device_state;


	uint8_t   rd_stream_count;


	uint8_t   rd_internal_stream_count;


	uint8_t   rd_internal_stream_count_val;


	uint8_t   rd_gph_id;


	uint8_t   rd_timing_status;


	uint8_t   rd_zone_id;



} VL53L1_ll_driver_state_t;










typedef struct {

	uint8_t   wait_method;


	VL53L1_DevicePresetModes        preset_mode;


	VL53L1_DeviceZonePreset         zone_preset;


	VL53L1_DeviceMeasurementModes   measurement_mode;


	VL53L1_OffsetCalibrationMode    offset_calibration_mode;


	VL53L1_OffsetCorrectionMode     offset_correction_mode;


	VL53L1_DeviceDmaxMode           dmax_mode;


	uint32_t  phasecal_config_timeout_us;


	uint32_t  mm_config_timeout_us;


	uint32_t  range_config_timeout_us;


	uint32_t  inter_measurement_period_ms;


	uint32_t  fw_ready_poll_duration_ms;


	uint8_t   fw_ready;


	uint8_t   debug_mode;


	uint8_t  fpga_system;





	VL53L1_ll_version_t                 version;



	VL53L1_ll_driver_state_t            ll_state;



	VL53L1_GPIO_interrupt_config_t	    gpio_interrupt_config;



	VL53L1_customer_nvm_managed_t       customer;
	VL53L1_cal_peak_rate_map_t          cal_peak_rate_map;
	VL53L1_additional_offset_cal_data_t add_off_cal_data;
	VL53L1_dmax_calibration_data_t      fmt_dmax_cal;
	VL53L1_dmax_calibration_data_t      cust_dmax_cal;
	VL53L1_gain_calibration_data_t      gain_cal;
	VL53L1_user_zone_t                  mm_roi;
	VL53L1_zone_config_t                zone_cfg;



	uint8_t rtn_good_spads[VL53L1_RTN_SPAD_BUFFER_SIZE];



	VL53L1_refspadchar_config_t         refspadchar;
	VL53L1_ssc_config_t                 ssc_cfg;
	VL53L1_hist_post_process_config_t   histpostprocess;
	VL53L1_hist_gen3_dmax_config_t      dmax_cfg;
	VL53L1_xtalk_config_t               xtalk_cfg;



	VL53L1_static_nvm_managed_t         stat_nvm;
	VL53L1_histogram_config_t           hist_cfg;
	VL53L1_static_config_t              stat_cfg;
	VL53L1_general_config_t             gen_cfg;
	VL53L1_timing_config_t              tim_cfg;
	VL53L1_dynamic_config_t             dyn_cfg;
	VL53L1_system_control_t             sys_ctrl;
	VL53L1_system_results_t             sys_results;
	VL53L1_nvm_copy_data_t              nvm_copy_data;



	VL53L1_histogram_bin_data_t         hist_data;
	VL53L1_histogram_bin_data_t         hist_xtalk;



	VL53L1_xtalk_histogram_data_t       xtalk_shapes;
	VL53L1_xtalk_range_results_t        xtalk_results;
	VL53L1_xtalk_calibration_results_t  xtalk_cal;



	VL53L1_offset_range_results_t       offset_results;



	VL53L1_core_results_t               core_results;
	VL53L1_debug_results_t              dbg_results;

#ifdef PAL_EXTENDED


	VL53L1_patch_results_t                patch_results;
	VL53L1_shadow_core_results_t          shadow_core_results;
	VL53L1_shadow_system_results_t        shadow_sys_results;
	VL53L1_prev_shadow_core_results_t     prev_shadow_core_results;
	VL53L1_prev_shadow_system_results_t   prev_shadow_sys_results;
#endif

} VL53L1_LLDriverData_t;










typedef struct {



	VL53L1_range_results_t             range_results;



	VL53L1_zone_private_dyn_cfgs_t     zone_dyn_cfgs;



	VL53L1_zone_results_t              zone_results;
	VL53L1_zone_histograms_t           zone_hists;
	VL53L1_zone_calibration_results_t  zone_cal;

} VL53L1_LLDriverResults_t;










typedef struct {

	uint32_t                             struct_version;
	VL53L1_customer_nvm_managed_t        customer;
	VL53L1_dmax_calibration_data_t       fmt_dmax_cal;
	VL53L1_dmax_calibration_data_t       cust_dmax_cal;
	VL53L1_additional_offset_cal_data_t  add_off_cal_data;
	VL53L1_user_zone_t                   mm_roi;
	VL53L1_xtalk_histogram_data_t        xtalkhisto;
	VL53L1_gain_calibration_data_t       gain_cal;
	VL53L1_cal_peak_rate_map_t           cal_peak_rate_map;

} VL53L1_calibration_data_t;










typedef struct {

	VL53L1_customer_nvm_managed_t        customer;
	VL53L1_xtalk_config_t                xtalk_cfg;
	VL53L1_histogram_bin_data_t          hist_data;
	VL53L1_xtalk_histogram_data_t        xtalk_shapes;
	VL53L1_xtalk_range_results_t         xtalk_results;

} VL53L1_xtalk_debug_data_t;










typedef struct {

	VL53L1_customer_nvm_managed_t        customer;
	VL53L1_dmax_calibration_data_t       fmt_dmax_cal;
	VL53L1_dmax_calibration_data_t       cust_dmax_cal;
	VL53L1_additional_offset_cal_data_t  add_off_cal_data;
	VL53L1_offset_range_results_t        offset_results;

} VL53L1_offset_debug_data_t;











typedef struct {

	uint16_t  target_reflectance_for_dmax[VL53L1_MAX_AMBIENT_DMAX_VALUES];

} VL53L1_dmax_reflectance_array_t;














typedef struct {

	uint8_t    spad_type;


	uint16_t   VL53L1_PRM_00016;


	uint16_t   rate_data[VL53L1_NO_OF_SPAD_ENABLES];


	uint16_t    no_of_values;


	uint8_t    fractional_bits;


	uint8_t    error_status;



} VL53L1_spad_rate_data_t;





#define SUPPRESS_UNUSED_WARNING(x) \
	((void) (x))


#define IGNORE_STATUS(__FUNCTION_ID__, __ERROR_STATUS_CHECK__, __STATUS__) \
	do { \
		DISABLE_WARNINGS(); \
		if (__FUNCTION_ID__) { \
			if (__STATUS__ == __ERROR_STATUS_CHECK__) { \
				__STATUS__ = VL53L1_ERROR_NONE; \
				WARN_OVERRIDE_STATUS(__FUNCTION_ID__); \
			} \
		} \
		ENABLE_WARNINGS(); \
	} \
	while (0)

#define VL53L1_COPYSTRING(str, ...) \
	(strncpy(str, ##__VA_ARGS__, VL53L1_MAX_STRING_LENGTH-1))

#ifdef __cplusplus
}
#endif

#endif




