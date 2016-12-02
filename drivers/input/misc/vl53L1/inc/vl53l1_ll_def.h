
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


#ifdef __cplusplus
extern "C" {
#endif

#include "vl53l1_error_codes.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_platform_user_config.h"









#define VL53L1_DEF_00142       1


#define VL53L1_DEF_00143       1


#define VL53L1_DEF_00144         3


#define VL53L1_DEF_00145  1925

#define VL53L1_DEF_00226 "1.1.3.1925"



#define VL53L1_DEF_00227         398
#define VL53L1_DEF_00228         400

#define    VL53L1_DEF_00229     512












#define VL53L1_DEF_00164           ((VL53L1_Error) - 80)


#define VL53L1_DEF_00165           ((VL53L1_Error) - 85)


#define VL53L1_DEF_00166                    ((VL53L1_Error) - 90)


#define VL53L1_DEF_00167                    ((VL53L1_Error) - 95)


#define VL53L1_DEF_00168                   ((VL53L1_Error) - 96)













typedef struct {
	uint32_t     VL53L1_PRM_00290;

	uint8_t      VL53L1_PRM_00287;

	uint8_t      VL53L1_PRM_00288;

	uint8_t      VL53L1_PRM_00289;

} VL53L1_ll_version_t;






typedef struct {

	uint8_t    VL53L1_PRM_00046;

	uint8_t    VL53L1_PRM_00041;

	uint32_t   VL53L1_PRM_00042;

	uint16_t   VL53L1_PRM_00043;


	uint16_t   VL53L1_PRM_00045;


	uint16_t   VL53L1_PRM_00044;



} VL53L1_refspadchar_config_t;







typedef struct {

	uint8_t   VL53L1_PRM_00015;

	uint8_t   VL53L1_PRM_00016;

	uint8_t   VL53L1_PRM_00017;

	uint8_t   VL53L1_PRM_00018;


} VL53L1_user_zone_t;









typedef struct {

	uint8_t             VL53L1_PRM_00019;

	uint8_t             VL53L1_PRM_00020;


	VL53L1_user_zone_t   VL53L1_PRM_00021[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_config_t;









typedef struct {




	uint8_t  VL53L1_PRM_00036;


    uint8_t  VL53L1_PRM_00156;


    uint8_t  VL53L1_PRM_00095;


    uint32_t VL53L1_PRM_00024;



	uint8_t  VL53L1_PRM_00157;


	uint8_t  VL53L1_PRM_00137;


	uint8_t  VL53L1_PRM_00158;


	uint8_t  VL53L1_PRM_00159;


	uint8_t  VL53L1_PRM_00160;


	uint8_t  VL53L1_PRM_00161;



    uint16_t   VL53L1_PRM_00017;


    uint8_t    VL53L1_PRM_00190;




    uint16_t   VL53L1_PRM_00098;


    uint16_t   VL53L1_PRM_00306;


    uint16_t   VL53L1_PRM_00032;



    uint32_t   VL53L1_PRM_00152;



	uint32_t   VL53L1_PRM_00153;



	uint32_t   VL53L1_PRM_00154;







    uint32_t   VL53L1_PRM_00175;


    uint32_t   VL53L1_PRM_00171;



    int32_t    VL53L1_PRM_00072;







    uint16_t    VL53L1_PRM_00030;


    uint16_t    VL53L1_PRM_00164;


    uint16_t    VL53L1_PRM_00031;


    uint16_t    VL53L1_PRM_00314;


    uint32_t    VL53L1_PRM_00071;






    uint16_t   VL53L1_PRM_00033;






    uint16_t   VL53L1_PRM_00187;



    uint16_t   VL53L1_PRM_00168;


    uint16_t   VL53L1_PRM_00188;







    int16_t    VL53L1_PRM_00029;





    int16_t    VL53L1_PRM_00034;




    int16_t    VL53L1_PRM_00028;









    uint8_t    VL53L1_PRM_00025;

} VL53L1_range_data_t;










typedef struct {

	VL53L1_DeviceState     VL53L1_PRM_00130;


	VL53L1_DeviceState     VL53L1_PRM_00038;


	uint8_t                VL53L1_PRM_00063;



	uint8_t                VL53L1_PRM_00037;


	VL53L1_range_data_t    VL53L1_PRM_00035[VL53L1_MAX_RANGE_RESULTS];



} VL53L1_range_results_t;










typedef struct {

    uint8_t    VL53L1_PRM_00064;


    uint32_t   VL53L1_PRM_00067;


    uint32_t   VL53L1_PRM_00068;


    int32_t    VL53L1_PRM_00066;



    int32_t    VL53L1_PRM_00065;





} VL53L1_xtalk_range_data_t;










typedef struct {

	uint8_t                VL53L1_PRM_00063;



	uint8_t                VL53L1_PRM_00037;


	VL53L1_xtalk_range_data_t VL53L1_PRM_00035[VL53L1_MAX_XTALK_RANGE_RESULTS];


	VL53L1_histogram_bin_data_t VL53L1_PRM_00062;



	VL53L1_histogram_bin_data_t VL53L1_PRM_00060;




} VL53L1_xtalk_range_results_t;










typedef struct {

    uint8_t    VL53L1_PRM_00086;


    uint8_t    VL53L1_PRM_00064;


    uint32_t   VL53L1_PRM_00087;


    uint32_t   VL53L1_PRM_00088;


    uint32_t   VL53L1_PRM_00089;


    uint32_t   VL53L1_PRM_00090;


    int32_t    VL53L1_PRM_00091;



    int32_t    VL53L1_PRM_00092;



    int32_t    VL53L1_PRM_00096;



} VL53L1_offset_range_data_t;










typedef struct {

	uint8_t                VL53L1_PRM_00063;



	uint8_t                VL53L1_PRM_00037;


    int32_t    VL53L1_PRM_00081;


    VL53L1_offset_range_data_t VL53L1_PRM_00035[VL53L1_MAX_OFFSET_RANGE_RESULTS];



} VL53L1_offset_range_results_t;










typedef struct {

	uint8_t                VL53L1_PRM_00019;



	uint8_t                VL53L1_PRM_00020;


	VL53L1_range_results_t VL53L1_PRM_00035[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_results_t;










typedef struct {

	uint8_t                     VL53L1_PRM_00019;



	uint8_t                     VL53L1_PRM_00020;


	VL53L1_histogram_bin_data_t VL53L1_PRM_00035[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_histograms_t;









typedef struct {

	uint8_t      VL53L1_PRM_00295;


	uint8_t      VL53L1_PRM_00296;


	uint8_t      VL53L1_PRM_00566;


	uint16_t     VL53L1_PRM_00313;



	uint8_t      VL53L1_PRM_00567;


	uint8_t      VL53L1_PRM_00568;



	uint8_t  VL53L1_PRM_00115;


	uint8_t  VL53L1_PRM_00116;



} VL53L1_zone_private_dyn_cfg_t;









typedef struct {

	uint8_t                     VL53L1_PRM_00019;



	uint8_t                     VL53L1_PRM_00020;


	VL53L1_zone_private_dyn_cfg_t VL53L1_PRM_00035[VL53L1_MAX_USER_ZONES];



} VL53L1_zone_private_dyn_cfgs_t;









typedef struct {

	uint16_t  VL53L1_PRM_00080;


	int16_t   VL53L1_PRM_00078;


	int16_t   VL53L1_PRM_00079;



} VL53L1_xtalk_calibration_results_t;










typedef struct {

	VL53L1_DeviceState   VL53L1_PRM_00130;


	uint8_t   VL53L1_PRM_00291;



	uint8_t   VL53L1_PRM_00124;


	uint8_t   VL53L1_PRM_00292;


	uint8_t   VL53L1_PRM_00123;



	VL53L1_DeviceState   VL53L1_PRM_00038;


	uint8_t   VL53L1_PRM_00293;


	uint8_t   VL53L1_PRM_00294;


	uint8_t   VL53L1_PRM_00145;


	uint8_t   VL53L1_PRM_00070;



} VL53L1_ll_driver_state_t;










typedef struct {

    uint8_t   VL53L1_PRM_00108;


    uint8_t   VL53L1_PRM_00086;


    uint8_t   VL53L1_PRM_00109;


    uint8_t   VL53L1_PRM_00005;


    uint32_t  VL53L1_PRM_00006;


    uint32_t  VL53L1_PRM_00007;


    uint32_t  VL53L1_PRM_00008;


    uint32_t  VL53L1_PRM_00565;


    uint8_t   VL53L1_PRM_00564;


    uint8_t   VL53L1_PRM_00110;


    uint8_t  VL53L1_PRM_00112;





    VL53L1_ll_version_t                VL53L1_PRM_00107;



    VL53L1_ll_driver_state_t           VL53L1_PRM_00069;



    VL53L1_customer_nvm_managed_t      VL53L1_PRM_00050;
    VL53L1_zone_config_t               VL53L1_PRM_00014;



    VL53L1_refspadchar_config_t        VL53L1_PRM_00040;
    VL53L1_hist_post_process_config_t  VL53L1_PRM_00097;



    VL53L1_static_nvm_managed_t        VL53L1_PRM_00010;
    VL53L1_histogram_config_t          VL53L1_PRM_00120;
    VL53L1_static_config_t             VL53L1_PRM_00104;
    VL53L1_general_config_t            VL53L1_PRM_00099;
    VL53L1_timing_config_t             VL53L1_PRM_00009;
    VL53L1_dynamic_config_t            VL53L1_PRM_00022;
    VL53L1_system_control_t            VL53L1_PRM_00121;
    VL53L1_system_results_t            VL53L1_PRM_00027;
    VL53L1_nvm_copy_data_t             VL53L1_PRM_00002;



    VL53L1_histogram_bin_data_t        VL53L1_PRM_00073;
    VL53L1_histogram_bin_data_t        VL53L1_PRM_00111;



    VL53L1_xtalk_histogram_data_t      VL53L1_PRM_00076;
    VL53L1_xtalk_range_results_t       VL53L1_PRM_00061;
    VL53L1_xtalk_calibration_results_t VL53L1_PRM_00077;



    VL53L1_offset_range_results_t      VL53L1_PRM_00085;



    VL53L1_core_results_t              VL53L1_PRM_00128;
    VL53L1_debug_results_t             VL53L1_PRM_00047;

#ifdef PAL_EXTENDED


    VL53L1_patch_results_t                VL53L1_PRM_00569;
    VL53L1_shadow_core_results_t          VL53L1_PRM_00570;
    VL53L1_shadow_system_results_t        VL53L1_PRM_00571;
    VL53L1_prev_shadow_core_results_t     VL53L1_PRM_00572;
    VL53L1_prev_shadow_system_results_t   VL53L1_PRM_00573;
#endif

} VL53L1_LLDriverData_t;










typedef struct {



    VL53L1_range_results_t             VL53L1_PRM_00059;



    VL53L1_zone_private_dyn_cfgs_t     VL53L1_PRM_00155;



    VL53L1_zone_results_t              VL53L1_PRM_00075;
    VL53L1_zone_histograms_t           VL53L1_PRM_00074;

} VL53L1_LLDriverResults_t;







#ifdef __cplusplus
}
#endif


#endif

