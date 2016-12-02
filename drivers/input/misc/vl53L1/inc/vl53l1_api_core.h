
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

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C" {
#endif












VL53L1_Error VL53L1_FCTN_00032(
	VL53L1_DEV            Dev,
	VL53L1_ll_version_t  *pversion);
















VL53L1_Error VL53L1_FCTN_00034(
	VL53L1_DEV        Dev,
	uint8_t          *pfpga_system);












VL53L1_Error VL53L1_FCTN_00035(
	VL53L1_DEV         Dev,
	uint16_t          *pfw_version);
















VL53L1_Error VL53L1_FCTN_00001(
	VL53L1_DEV         Dev,
	uint8_t            read_p2p_data);

















VL53L1_Error VL53L1_FCTN_00039(
	VL53L1_DEV      Dev);













VL53L1_Error VL53L1_FCTN_00003(
	VL53L1_DEV      Dev);



















VL53L1_Error VL53L1_FCTN_00047(
    VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto);
















VL53L1_Error VL53L1_FCTN_00048(
    VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto);















VL53L1_Error VL53L1_FCTN_00006(
    VL53L1_DEV          Dev,
    uint32_t            VL53L1_PRM_00008);















VL53L1_Error VL53L1_FCTN_00007(
    VL53L1_DEV          Dev,
    uint32_t           *pinter_measurement_period_ms);













VL53L1_Error VL53L1_FCTN_00049(
    VL53L1_DEV          Dev,
    VL53L1_user_zone_t *puser_zone);













VL53L1_Error VL53L1_FCTN_00051(
    VL53L1_DEV          Dev,
    VL53L1_user_zone_t *puser_zone);













VL53L1_Error VL53L1_FCTN_00053(
    VL53L1_DEV          Dev,
    VL53L1_user_zone_t *pmm_roi);


















VL53L1_Error VL53L1_FCTN_00008(
    VL53L1_DEV             Dev,
    VL53L1_zone_config_t  *pzone_cfg);















VL53L1_Error VL53L1_FCTN_00009(
    VL53L1_DEV             Dev,
	VL53L1_zone_config_t  *pzone_cfg);































VL53L1_Error VL53L1_FCTN_00004(
    VL53L1_DEV                 Dev,
	VL53L1_DevicePresetModes   device_preset_mode,
	uint32_t                   VL53L1_PRM_00006,
	uint32_t                   VL53L1_PRM_00007,
	uint32_t                   VL53L1_PRM_00008);













VL53L1_Error VL53L1_FCTN_00080(
    VL53L1_DEV               Dev,
	VL53L1_DeviceZonePreset  VL53L1_PRM_00109);














































VL53L1_Error VL53L1_FCTN_00012(
	VL53L1_DEV                      Dev,
	uint8_t                         VL53L1_PRM_00005,
	VL53L1_DeviceConfigLevel        device_config_level);













VL53L1_Error VL53L1_FCTN_00013(
	VL53L1_DEV  Dev);




























VL53L1_Error VL53L1_FCTN_00094(
	VL53L1_DEV                  Dev,
	VL53L1_DeviceResultsLevel   device_result_level);




























VL53L1_Error VL53L1_FCTN_00017(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceResultsLevel  device_result_level,
	VL53L1_range_results_t    *prange_results);




























VL53L1_Error VL53L1_FCTN_00014(
	VL53L1_DEV       Dev,
	uint8_t          VL53L1_PRM_00005);










VL53L1_Error VL53L1_FCTN_00099(
	VL53L1_DEV                   Dev,
	VL53L1_histogram_bin_data_t *phist_data);











void VL53L1_FCTN_00098(
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore,
	VL53L1_range_results_t           *presults);


#ifdef __cplusplus
}
#endif

#endif

