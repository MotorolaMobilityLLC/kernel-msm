
/*******************************************************************************
 * Copyright (c) 2017, STMicroelectronics - All Rights Reserved

 This file is part of VL53L1 Core and is dual licensed,
 either 'STMicroelectronics
 Proprietary license'
 or 'BSD 3-clause "New" or "Revised" License' , at your option.

********************************************************************************

 'STMicroelectronics Proprietary license'

********************************************************************************

 License terms: STMicroelectronics Proprietary in accordance with licensing
 terms at www.st.com/sla0081

 STMicroelectronics confidential
 Reproduction and Communication of this document is strictly prohibited unless
 specifically authorized in writing by STMicroelectronics.


********************************************************************************

 Alternatively, VL53L1 Core may be distributed under the terms of
 'BSD 3-clause "New" or "Revised" License', in which case the following
 provisions apply instead of the ones
 mentioned above :

********************************************************************************

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


********************************************************************************

*/






































#ifndef _VL53L1_REGISTER_FUNCS_H_
#define _VL53L1_REGISTER_FUNCS_H_

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif













VL53L1_Error VL53L1_i2c_encode_static_nvm_managed(
	VL53L1_static_nvm_managed_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_static_nvm_managed(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_set_static_nvm_managed(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_get_static_nvm_managed(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_customer_nvm_managed(
	VL53L1_customer_nvm_managed_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_customer_nvm_managed(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_customer_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_set_customer_nvm_managed(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata);
















VL53L1_Error VL53L1_get_customer_nvm_managed(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_static_config(
	VL53L1_static_config_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_static_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_config_t    *pdata);
















VL53L1_Error VL53L1_set_static_config(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata);
















VL53L1_Error VL53L1_get_static_config(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata);













VL53L1_Error VL53L1_i2c_encode_general_config(
	VL53L1_general_config_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_general_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_general_config_t   *pdata);
















VL53L1_Error VL53L1_set_general_config(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata);
















VL53L1_Error VL53L1_get_general_config(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata);













VL53L1_Error VL53L1_i2c_encode_timing_config(
	VL53L1_timing_config_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_timing_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_timing_config_t    *pdata);
















VL53L1_Error VL53L1_set_timing_config(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata);
















VL53L1_Error VL53L1_get_timing_config(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata);













VL53L1_Error VL53L1_i2c_encode_dynamic_config(
	VL53L1_dynamic_config_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_dynamic_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_dynamic_config_t   *pdata);
















VL53L1_Error VL53L1_set_dynamic_config(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata);
















VL53L1_Error VL53L1_get_dynamic_config(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata);













VL53L1_Error VL53L1_i2c_encode_system_control(
	VL53L1_system_control_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_system_control(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_control_t   *pdata);
















VL53L1_Error VL53L1_set_system_control(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata);
















VL53L1_Error VL53L1_get_system_control(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata);













VL53L1_Error VL53L1_i2c_encode_system_results(
	VL53L1_system_results_t   *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_system_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_results_t   *pdata);
















VL53L1_Error VL53L1_set_system_results(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata);
















VL53L1_Error VL53L1_get_system_results(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata);













VL53L1_Error VL53L1_i2c_encode_core_results(
	VL53L1_core_results_t     *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_core_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_core_results_t     *pdata);
















VL53L1_Error VL53L1_set_core_results(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata);
















VL53L1_Error VL53L1_get_core_results(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata);













VL53L1_Error VL53L1_i2c_encode_debug_results(
	VL53L1_debug_results_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_debug_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_debug_results_t    *pdata);
















VL53L1_Error VL53L1_set_debug_results(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata);
















VL53L1_Error VL53L1_get_debug_results(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata);













VL53L1_Error VL53L1_i2c_encode_nvm_copy_data(
	VL53L1_nvm_copy_data_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_nvm_copy_data(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_nvm_copy_data_t    *pdata);
















VL53L1_Error VL53L1_set_nvm_copy_data(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata);
















VL53L1_Error VL53L1_get_nvm_copy_data(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata);













VL53L1_Error VL53L1_i2c_encode_prev_shadow_system_results(
	VL53L1_prev_shadow_system_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_prev_shadow_system_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_set_prev_shadow_system_results(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_get_prev_shadow_system_results(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_prev_shadow_core_results(
	VL53L1_prev_shadow_core_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_prev_shadow_core_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_set_prev_shadow_core_results(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_get_prev_shadow_core_results(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_patch_debug(
	VL53L1_patch_debug_t      *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_patch_debug(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_debug_t      *pdata);
















VL53L1_Error VL53L1_set_patch_debug(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata);
















VL53L1_Error VL53L1_get_patch_debug(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata);













VL53L1_Error VL53L1_i2c_encode_gph_general_config(
	VL53L1_gph_general_config_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_gph_general_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_general_config_t  *pdata);
















VL53L1_Error VL53L1_set_gph_general_config(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata);
















VL53L1_Error VL53L1_get_gph_general_config(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_gph_static_config(
	VL53L1_gph_static_config_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_gph_static_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_static_config_t  *pdata);
















VL53L1_Error VL53L1_set_gph_static_config(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata);
















VL53L1_Error VL53L1_get_gph_static_config(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_gph_timing_config(
	VL53L1_gph_timing_config_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_gph_timing_config(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_timing_config_t  *pdata);
















VL53L1_Error VL53L1_set_gph_timing_config(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata);
















VL53L1_Error VL53L1_get_gph_timing_config(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_fw_internal(
	VL53L1_fw_internal_t      *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_fw_internal(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_fw_internal_t      *pdata);
















VL53L1_Error VL53L1_set_fw_internal(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata);
















VL53L1_Error VL53L1_get_fw_internal(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata);













VL53L1_Error VL53L1_i2c_encode_patch_results(
	VL53L1_patch_results_t    *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_patch_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_results_t    *pdata);
















VL53L1_Error VL53L1_set_patch_results(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata);
















VL53L1_Error VL53L1_get_patch_results(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata);













VL53L1_Error VL53L1_i2c_encode_shadow_system_results(
	VL53L1_shadow_system_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_shadow_system_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_set_shadow_system_results(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata);
















VL53L1_Error VL53L1_get_shadow_system_results(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata);













VL53L1_Error VL53L1_i2c_encode_shadow_core_results(
	VL53L1_shadow_core_results_t  *pdata,
	uint16_t                   buf_size,
	uint8_t                   *pbuffer);
















VL53L1_Error VL53L1_i2c_decode_shadow_core_results(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_set_shadow_core_results(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata);
















VL53L1_Error VL53L1_get_shadow_core_results(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata);


#ifdef __cplusplus
}
#endif

#endif


