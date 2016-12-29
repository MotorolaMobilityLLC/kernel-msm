
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






































#ifndef _VL53L1_LL_DEF_H_
#define _VL53L1_LL_DEF_H_

#include "vl53l1_error_codes.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_platform_user_config.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_dmax_structs.h"

#ifdef __cplusplus
extern "C" {
#endif









#define VL53L1_LL_API_IMPLEMENTATION_VER_MAJOR       1


#define VL53L1_LL_API_IMPLEMENTATION_VER_MINOR       1


#define VL53L1_LL_API_IMPLEMENTATION_VER_SUB         6


#define VL53L1_LL_API_IMPLEMENTATION_VER_REVISION  11056111

#define VL53L1_LL_API_IMPLEMENTATION_VER_STRING "1.1.6.11056111"



#define VL53L1_FIRMWARE_VER_MINIMUM         398
#define VL53L1_FIRMWARE_VER_MAXIMUM         400

#define    VL53L1_MAX_STRING_LENGTH_PLT     512












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

	uint8_t    VL53L1_PRM_00006;

	uint32_t   timeout_us;

	uint16_t   target_count_rate_mcps;


	uint16_t   min_count_rate_limit_mcps;


	uint16_t   max_count_rate_limit_mcps;



} VL53L1_refspadchar_config_t;





typedef struct {


	uint16_t  algo__crosstalk_compensation_plane_offset_kcps;


	int16_t   algo__crosstalk_compensation_x_plane_gradient_kcps;


	int16_t   algo__crosstalk_compensation_y_plane_gradient_kcps;



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

	VL53L1_user_zone_t user_zones [VL53L1_MAX_USER_ZONES];



	uint8_t bin_config [VL53L1_MAX_USER_ZONES];



} VL53L1_zone_config_t;









typedef struct {




    uint8_t  range_id;


    uint32_t time_stamp;


	uint8_t  VL53L1_PRM_00015;


	uint8_t  VL53L1_PRM_00011;


	uint8_t  VL53L1_PRM_00016;


	uint8_t  VL53L1_PRM_00017;


	uint8_t  VL53L1_PRM_00018;


	uint8_t  VL53L1_PRM_00019;



    uint16_t   width;


    uint8_t    VL53L1_PRM_00025;




    uint16_t   fast_osc_frequency;


    uint16_t   zero_distance_phase;


    uint16_t   VL53L1_PRM_00002;



    uint32_t   total_periods_elapsed;



	uint32_t   peak_duration_us;



	uint32_t   woi_duration_us;







    uint32_t   VL53L1_PRM_00022;


    uint32_t   VL53L1_PRM_00021;



    int32_t    VL53L1_PRM_00008;







    uint16_t    peak_signal_count_rate_mcps;


    uint16_t    avg_signal_count_rate_mcps;


    uint16_t    ambient_count_rate_mcps;


    uint16_t    total_rate_per_spad_mcps;


    uint32_t    VL53L1_PRM_00007;






    uint16_t   VL53L1_PRM_00003;






    uint16_t   VL53L1_PRM_00023;



    uint16_t   VL53L1_PRM_00020;


    uint16_t   VL53L1_PRM_00024;







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



	int16_t                ambient_dmax_mm;




	int16_t                wrap_dmax_mm;



	uint8_t                max_results;



	uint8_t                active_results;


	VL53L1_range_data_t    VL53L1_PRM_00004[VL53L1_MAX_RANGE_RESULTS];




} VL53L1_range_results_t;










typedef struct {

    uint8_t    no_of_samples;


    uint32_t   rate_per_spad_kcps_sum;


    uint32_t   rate_per_spad_kcps_avg;


    int32_t    signal_total_events_sum;



    int32_t    signal_total_events_avg;





} VL53L1_xtalk_range_data_t;










typedef struct {

	uint8_t                max_results;



	uint8_t                active_results;


	VL53L1_xtalk_range_data_t VL53L1_PRM_00004[VL53L1_MAX_XTALK_RANGE_RESULTS];


	VL53L1_histogram_bin_data_t central_histogram_sum;



	VL53L1_histogram_bin_data_t central_histogram_avg;




} VL53L1_xtalk_range_results_t;










typedef struct {

    uint8_t    preset_mode;


    uint8_t    dss_config__roi_mode_control;


    uint16_t   dss_config__manual_effective_spads_select;


    uint8_t    no_of_samples;


    uint32_t   effective_spads_sum;


    uint32_t   effective_spads_avg;


    uint32_t   peak_rate_mcps_sum;


    uint32_t   peak_rate_mcps_avg;


    int32_t    median_range_mm_sum;



    int32_t    median_range_mm_avg;



    int32_t    range_mm_offset;



} VL53L1_offset_range_data_t;










typedef struct {

	uint8_t                max_results;



	uint8_t                active_results;


    int32_t    target_distance_mm;


    VL53L1_offset_range_data_t VL53L1_PRM_00004[VL53L1_MAX_OFFSET_RANGE_RESULTS];



} VL53L1_offset_range_results_t;










typedef struct {

	uint8_t                max_zones;



	uint8_t                active_zones;


	VL53L1_range_results_t VL53L1_PRM_00004[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_results_t;










typedef struct {

	uint8_t                     max_zones;



	uint8_t                     active_zones;


	VL53L1_histogram_bin_data_t VL53L1_PRM_00004[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_histograms_t;









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


	VL53L1_zone_private_dyn_cfg_t VL53L1_PRM_00004[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_private_dyn_cfgs_t;









typedef struct {

	uint16_t  algo__crosstalk_compensation_plane_offset_kcps;


	int16_t   algo__crosstalk_compensation_x_plane_gradient_kcps;


	int16_t   algo__crosstalk_compensation_y_plane_gradient_kcps;



} VL53L1_xtalk_calibration_results_t;










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


    uint8_t   preset_mode;


    uint8_t   zone_preset;


    uint8_t   measurement_mode;


    uint32_t  mm_config_timeout_us;


    uint32_t  range_config_timeout_us;


    uint32_t  inter_measurement_period_ms;


    uint32_t  fw_ready_poll_duration_ms;


    uint8_t   fw_ready;


    uint8_t   debug_mode;


    uint8_t  fpga_system;





    VL53L1_ll_version_t                version;



    VL53L1_ll_driver_state_t           ll_state;



    VL53L1_customer_nvm_managed_t      customer;
    VL53L1_dmax_calibration_data_t     dmax_cal;
    VL53L1_zone_config_t               zone_cfg;



    VL53L1_refspadchar_config_t        refspadchar;
    VL53L1_hist_post_process_config_t  histpostprocess;
    VL53L1_hist_gen3_dmax_config_t     dmax_cfg;
    VL53L1_xtalk_config_t              xtalk_cfg;



    VL53L1_static_nvm_managed_t        stat_nvm;
    VL53L1_histogram_config_t          hist_cfg;
    VL53L1_static_config_t             stat_cfg;
    VL53L1_general_config_t            gen_cfg;
    VL53L1_timing_config_t             tim_cfg;
    VL53L1_dynamic_config_t            dyn_cfg;
    VL53L1_system_control_t            sys_ctrl;
    VL53L1_system_results_t            sys_results;
    VL53L1_nvm_copy_data_t             nvm_copy_data;



    VL53L1_histogram_bin_data_t        hist_data;
    VL53L1_histogram_bin_data_t        hist_xtalk;



    VL53L1_xtalk_histogram_data_t      xtalk_shape;
    VL53L1_xtalk_range_results_t       xtalk_results;
    VL53L1_xtalk_calibration_results_t xtalk_cal;



    VL53L1_offset_range_results_t      offset_results;



    VL53L1_core_results_t              core_results;
    VL53L1_debug_results_t             dbg_results;

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

} VL53L1_LLDriverResults_t;







#ifdef __cplusplus
}
#endif


#endif

