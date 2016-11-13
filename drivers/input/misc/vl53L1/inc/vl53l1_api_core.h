
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




































#ifndef _VL53L1_API_CORE_H_
#define _VL53L1_API_CORE_H_

#include "vl53l1_ll_def.h"
#include "vl53l1_ll_device.h"
#include "vl53l1_platform.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_hist_structs.h"


#ifdef __cplusplus
extern "C" {
#endif












VL53L1_Error VL53L1_FCTN_00025(
	VL53L1_DEV            Dev,
	VL53L1_ll_version_t  *pversion);











VL53L1_Error VL53L1_FCTN_00027(
	VL53L1_DEV         Dev,
	uint16_t          *pfw_version);
















VL53L1_Error VL53L1_FCTN_00001(
	VL53L1_DEV         Dev,
	uint8_t            read_p2p_data);

















VL53L1_Error VL53L1_FCTN_00031(
	VL53L1_DEV      Dev);













VL53L1_Error VL53L1_FCTN_00003(
	VL53L1_DEV      Dev);



















VL53L1_Error VL53L1_FCTN_00040(
    VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto);
















VL53L1_Error VL53L1_FCTN_00041(
    VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto);















VL53L1_Error VL53L1_FCTN_00042(
    VL53L1_DEV          Dev,
    uint32_t            VL53L1_PRM_00009);















VL53L1_Error VL53L1_FCTN_00043(
    VL53L1_DEV          Dev,
    uint32_t           *pinter_measurement_period_ms);













VL53L1_Error VL53L1_FCTN_00044(
    VL53L1_DEV          Dev,
    VL53L1_user_zone_t *puser_zone);













VL53L1_Error VL53L1_FCTN_00046(
    VL53L1_DEV          Dev,
    VL53L1_user_zone_t *puser_zone);













VL53L1_Error VL53L1_FCTN_00048(
    VL53L1_DEV          Dev,
    VL53L1_user_zone_t *pmm_roi);


















VL53L1_Error VL53L1_FCTN_00006(
    VL53L1_DEV             Dev,
    VL53L1_zone_config_t  *pzone_cfg);















VL53L1_Error VL53L1_FCTN_00007(
    VL53L1_DEV             Dev,
	VL53L1_zone_config_t  *pzone_cfg);































VL53L1_Error VL53L1_FCTN_00004(
    VL53L1_DEV                 Dev,
	VL53L1_DevicePresetModes   device_preset_mode,
	uint32_t                   VL53L1_PRM_00007,
	uint32_t                   VL53L1_PRM_00008,
	uint32_t                   VL53L1_PRM_00009);














































VL53L1_Error VL53L1_FCTN_00009(
	VL53L1_DEV                      Dev,
	uint8_t                         VL53L1_PRM_00006,
	VL53L1_DeviceConfigLevel        device_config_level);













VL53L1_Error VL53L1_FCTN_00010(
	VL53L1_DEV  Dev);




























VL53L1_Error VL53L1_FCTN_00074(
	VL53L1_DEV                  Dev,
	VL53L1_DeviceResultsLevel   device_result_level);




























VL53L1_Error VL53L1_FCTN_00013(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceResultsLevel  device_result_level,
	VL53L1_zone_results_t     *pzone_results);




























VL53L1_Error VL53L1_FCTN_00011(
	VL53L1_DEV       Dev,
	uint8_t          VL53L1_PRM_00006);










VL53L1_Error VL53L1_FCTN_00079(
	VL53L1_DEV                   Dev,
	VL53L1_histogram_bin_data_t *phist_data);











void VL53L1_FCTN_00078(
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore,
	VL53L1_range_results_t           *presults);


#ifdef __cplusplus
}
#endif

#endif 

