
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






































#ifndef _VL53L1_REGISTER_FUNCS_H_
#define _VL53L1_REGISTER_FUNCS_H_

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif













VL53L1_Error VL53L1_FCTN_00084(
	VL53L1_static_nvm_managed_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00147(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00148(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00043(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata);













VL53L1_Error VL53L1_FCTN_00085(
	VL53L1_customer_nvm_managed_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00149(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_customer_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00027(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00044(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata);













VL53L1_Error VL53L1_FCTN_00086(
	VL53L1_static_config_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00150(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_config_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00151(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00152(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata);













VL53L1_Error VL53L1_FCTN_00087(
	VL53L1_general_config_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00153(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_general_config_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00154(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00155(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata);













VL53L1_Error VL53L1_FCTN_00088(
	VL53L1_timing_config_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00156(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_timing_config_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00157(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00158(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata);













VL53L1_Error VL53L1_FCTN_00089(
	VL53L1_dynamic_config_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00159(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_dynamic_config_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00160(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00161(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata);













VL53L1_Error VL53L1_FCTN_00090(
	VL53L1_system_control_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00162(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_control_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00093(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00163(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata);













VL53L1_Error VL53L1_FCTN_00164(
	VL53L1_system_results_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00097(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_results_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00165(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata);
















VL53L1_Error VL53L1_FCTN_00166(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata);













VL53L1_Error VL53L1_FCTN_00167(
	VL53L1_core_results_t     *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00096(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_core_results_t     *pdata);
















VL53L1_Error VL53L1_FCTN_00168(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata);
















VL53L1_Error VL53L1_FCTN_00169(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata);













VL53L1_Error VL53L1_FCTN_00170(
	VL53L1_debug_results_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00095(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_debug_results_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00171(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00172(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata);













VL53L1_Error VL53L1_FCTN_00173(
	VL53L1_nvm_copy_data_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00174(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_nvm_copy_data_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00175(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00045(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata);













VL53L1_Error VL53L1_FCTN_00176(
	VL53L1_prev_shadow_system_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00177(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00178(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00179(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata);













VL53L1_Error VL53L1_FCTN_00180(
	VL53L1_prev_shadow_core_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00181(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00182(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00183(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata);













VL53L1_Error VL53L1_FCTN_00184(
	VL53L1_patch_debug_t      *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00185(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_debug_t      *pdata);
















VL53L1_Error VL53L1_FCTN_00186(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata);
















VL53L1_Error VL53L1_FCTN_00187(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata);













VL53L1_Error VL53L1_FCTN_00188(
	VL53L1_gph_general_config_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00189(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_general_config_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00190(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00191(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata);













VL53L1_Error VL53L1_FCTN_00192(
	VL53L1_gph_static_config_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00193(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_static_config_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00194(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00195(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata);













VL53L1_Error VL53L1_FCTN_00196(
	VL53L1_gph_timing_config_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00197(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_timing_config_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00198(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00199(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata);













VL53L1_Error VL53L1_FCTN_00200(
	VL53L1_fw_internal_t      *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00201(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_fw_internal_t      *pdata);
















VL53L1_Error VL53L1_FCTN_00202(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata);
















VL53L1_Error VL53L1_FCTN_00203(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata);













VL53L1_Error VL53L1_FCTN_00204(
	VL53L1_patch_results_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00205(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_results_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00206(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata);
















VL53L1_Error VL53L1_FCTN_00207(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata);













VL53L1_Error VL53L1_FCTN_00208(
	VL53L1_shadow_system_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00209(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00210(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00211(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata);













VL53L1_Error VL53L1_FCTN_00212(
	VL53L1_shadow_core_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_FCTN_00213(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00214(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_FCTN_00215(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata);


#ifdef __cplusplus
}
#endif

#endif

