
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
#include "vl53l1_ll_device.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_hist_structs.h"









#define VL53L1_DEF_00124       1


#define VL53L1_DEF_00125       0


#define VL53L1_DEF_00126         0


#define VL53L1_DEF_00127  1713

#define VL53L1_DEF_00206 "1.0.0.1713"



#define VL53L1_DEF_00045         398
#define VL53L1_DEF_00047         400

#define VL53L1_DEF_00033        5
	

















#define VL53L1_DEF_00046           ((VL53L1_Error) - 80)
	

#define VL53L1_DEF_00048           ((VL53L1_Error) - 85)
	

#define VL53L1_DEF_00145                    ((VL53L1_Error) - 90)
	

#define VL53L1_DEF_00146                    ((VL53L1_Error) - 95)
    

#define VL53L1_DEF_00147                   ((VL53L1_Error) - 96)
	

	











#define VL53L1_DEF_00207   0
#define VL53L1_DEF_00105     1







typedef struct {
	uint32_t     VL53L1_PRM_00254; 

	uint8_t      VL53L1_PRM_00251;    

	uint8_t      VL53L1_PRM_00252;    

	uint8_t      VL53L1_PRM_00253;    

} VL53L1_ll_version_t;






typedef struct {

	uint8_t    VL53L1_PRM_00045;     

	uint8_t    VL53L1_PRM_00040;         

	uint32_t   VL53L1_PRM_00041;           

	uint16_t   VL53L1_PRM_00042;
		

	uint16_t   VL53L1_PRM_00044;
		

	uint16_t   VL53L1_PRM_00043;
		


} VL53L1_refspadchar_config_t;







typedef struct {

	uint8_t   VL53L1_PRM_00016;   

	uint8_t   VL53L1_PRM_00017;   

	uint8_t   VL53L1_PRM_00018;      

	uint8_t   VL53L1_PRM_00019;     


} VL53L1_user_zone_t;









typedef struct {

	uint8_t             VL53L1_PRM_00020;      

	uint8_t             VL53L1_PRM_00021;   


	VL53L1_user_zone_t   VL53L1_PRM_00022[VL53L1_MAX_USER_ZONES];
		


} VL53L1_zone_config_t;









typedef struct {

	


	uint8_t  VL53L1_PRM_00036;
		

    uint8_t  VL53L1_PRM_00126;
    	

    uint8_t  VL53L1_PRM_00127;
		

    uint32_t VL53L1_PRM_00025;
		


    uint16_t   VL53L1_PRM_00018;
    	

    uint8_t    VL53L1_PRM_00523;
    	



    uint16_t   VL53L1_PRM_00073;
    	

    uint16_t   VL53L1_PRM_00265;
    	

    uint16_t   VL53L1_PRM_00031;
    	


    uint32_t   VL53L1_PRM_00123;
    	


	uint32_t   VL53L1_PRM_00060;
	    


	uint32_t   VL53L1_PRM_00124;
	    



    


    uint32_t   VL53L1_PRM_00140;
    	

    uint32_t   VL53L1_PRM_00136;
    	


    int32_t    VL53L1_PRM_00063;
    	



    


    uint16_t    VL53L1_PRM_00029;
    	

    uint16_t    VL53L1_PRM_00130;
    	

    uint16_t    VL53L1_PRM_00030;
    	

    uint16_t    VL53L1_PRM_00524;
    	


    


    uint16_t   VL53L1_PRM_00032;
    	

    uint16_t   VL53L1_PRM_00125;
    	

    int16_t    VL53L1_PRM_00033;
		




    


    uint8_t    VL53L1_PRM_00026;

} VL53L1_range_data_t;










typedef struct {

	uint8_t                VL53L1_PRM_00035;
		


	uint8_t                VL53L1_PRM_00037;
		

	VL53L1_range_data_t    VL53L1_PRM_00034[VL53L1_MAX_RANGE_RESULTS];
		


} VL53L1_range_results_t;










typedef struct {

	uint8_t    VL53L1_PRM_00037;
		

    uint16_t   VL53L1_PRM_00031;
    	

	uint32_t   VL53L1_PRM_00060;
	    

    int32_t    VL53L1_PRM_00061;
    	

    int32_t    VL53L1_PRM_00062;
    	

    int32_t    VL53L1_PRM_00063;
    	



    int16_t    VL53L1_PRM_00033;
    	




} VL53L1_xtalk_range_data_t;










typedef struct {

	uint8_t                VL53L1_PRM_00035;
		


	uint8_t                VL53L1_PRM_00037;
		

	VL53L1_xtalk_range_data_t VL53L1_PRM_00034[VL53L1_DEF_00033];
		


} VL53L1_xtalk_range_results_t;










typedef struct {

	VL53L1_DeviceState     VL53L1_PRM_00101;
		

	VL53L1_DeviceState     VL53L1_PRM_00064;
		

	uint8_t                VL53L1_PRM_00020;
		


	uint8_t                VL53L1_PRM_00021;
		

	VL53L1_range_results_t VL53L1_PRM_00034[VL53L1_MAX_USER_ZONES];
		


} VL53L1_zone_results_t;










typedef struct {

	uint8_t                     VL53L1_PRM_00020;
		


	uint8_t                     VL53L1_PRM_00021;
		

	VL53L1_histogram_bin_data_t VL53L1_PRM_00034[VL53L1_MAX_USER_ZONES];
		


} VL53L1_zone_histograms_t;









typedef struct {

	uint8_t      VL53L1_PRM_00525;
		

	uint8_t      VL53L1_PRM_00526;
		

	uint8_t      VL53L1_PRM_00527;
		

	uint16_t     VL53L1_PRM_00528;
		


	uint8_t      VL53L1_PRM_00529;
		

	uint8_t      VL53L1_PRM_00530;
		


} VL53L1_zone_private_dyn_cfg_t;









typedef struct {

	uint8_t                     VL53L1_PRM_00020;
	


	uint8_t                     VL53L1_PRM_00021;
	

	VL53L1_zone_private_dyn_cfg_t VL53L1_PRM_00034[VL53L1_MAX_USER_ZONES];
		


} VL53L1_zone_private_dyn_cfgs_t;









typedef struct {

	uint16_t  VL53L1_PRM_00072;
		

	int16_t   VL53L1_PRM_00070;
		

	int16_t   VL53L1_PRM_00071;
		


} VL53L1_xtalk_calibration_results_t;










typedef struct {
	VL53L1_DeviceState   VL53L1_PRM_00101;
		

	uint8_t   VL53L1_PRM_00255;
		


	uint8_t   VL53L1_PRM_00256;
		

	int8_t    VL53L1_PRM_00098;
		

	VL53L1_DeviceState   VL53L1_PRM_00064;
		

	uint8_t   VL53L1_PRM_00257;
		

	uint8_t   VL53L1_PRM_00116;
		

	uint8_t   VL53L1_PRM_00066;
		


} VL53L1_ll_driver_state_t;











typedef struct {

    uint8_t   VL53L1_PRM_00085;
    	

    uint8_t   VL53L1_PRM_00002;
    	

    uint8_t   VL53L1_PRM_00006;
    	

    uint32_t  VL53L1_PRM_00007;
    	

    uint32_t  VL53L1_PRM_00008;
    	

    uint32_t  VL53L1_PRM_00009;
    	

    uint32_t  VL53L1_PRM_00522;
    	

    uint8_t   VL53L1_PRM_00521;
    	

    uint8_t   VL53L1_PRM_00086;
    	

    uint16_t  VL53L1_PRM_00083;
    	


    

    VL53L1_ll_version_t                VL53L1_PRM_00082;

    

    VL53L1_ll_driver_state_t           VL53L1_PRM_00065;

    

    VL53L1_customer_nvm_managed_t      VL53L1_PRM_00049;
    VL53L1_zone_config_t               VL53L1_PRM_00015;

    

    VL53L1_refspadchar_config_t        VL53L1_PRM_00039;
    VL53L1_hist_post_process_config_t  VL53L1_PRM_00087;

    

    VL53L1_static_nvm_managed_t        VL53L1_PRM_00011;
    VL53L1_histogram_config_t          VL53L1_PRM_00095;
    VL53L1_static_config_t             VL53L1_PRM_00079;
    VL53L1_general_config_t            VL53L1_PRM_00074;
    VL53L1_timing_config_t             VL53L1_PRM_00010;
    VL53L1_dynamic_config_t            VL53L1_PRM_00023;
    VL53L1_system_control_t            VL53L1_PRM_00096;
    VL53L1_system_results_t            VL53L1_PRM_00028;
    VL53L1_nvm_copy_data_t             VL53L1_PRM_00003;

    

    VL53L1_histogram_bin_data_t        VL53L1_PRM_00067;
    VL53L1_histogram_bin_data_t        VL53L1_PRM_00088;

    

    VL53L1_xtalk_histogram_data_t      VL53L1_PRM_00068;
    VL53L1_xtalk_range_results_t       VL53L1_PRM_00059;
    VL53L1_xtalk_calibration_results_t VL53L1_PRM_00069;

    

    VL53L1_core_results_t              VL53L1_PRM_00099;
    VL53L1_debug_results_t             VL53L1_PRM_00046;

    

    VL53L1_range_results_t             VL53L1_PRM_00100;

    

    VL53L1_zone_results_t              VL53L1_PRM_00058;
    VL53L1_zone_histograms_t           VL53L1_PRM_00084;
    VL53L1_zone_private_dyn_cfgs_t     VL53L1_PRM_00531;

#ifdef PAL_EXTENDED
    

    VL53L1_patch_results_t                VL53L1_PRM_00532;
    VL53L1_shadow_core_results_t          VL53L1_PRM_00533;
    VL53L1_shadow_system_results_t        VL53L1_PRM_00534;
    VL53L1_prev_shadow_core_results_t     VL53L1_PRM_00535;
    VL53L1_prev_shadow_system_results_t   VL53L1_PRM_00536;
#endif

} VL53L1_LLDriverData_t;







#ifdef __cplusplus
}
#endif


#endif 

