
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










































#ifndef _VL53L1_NVM_H_
#define _VL53L1_NVM_H_

#include "vl53l1_ll_def.h"
#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define VL53L1_NVM_POWER_UP_DELAY_US             50
#define VL53L1_NVM_READ_TRIGGER_DELAY_US          5

















VL53L1_Error VL53L1_nvm_enable(
	VL53L1_DEV     Dev,
	uint16_t       nvm_ctrl_pulse_width,
	int32_t        nvm_power_up_delay_us);


















VL53L1_Error VL53L1_nvm_read(
	VL53L1_DEV     Dev,
	uint8_t        start_address,
	uint8_t        count,
	uint8_t       *pdata);














VL53L1_Error VL53L1_nvm_disable(
	VL53L1_DEV     Dev);


















VL53L1_Error VL53L1_nvm_format_decode(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_decoded_nvm_data_t *pdata);
















VL53L1_Error VL53L1_nvm_decode_optical_centre(
	uint16_t                             buf_size,
	uint8_t                             *pbuffer,
	VL53L1_optical_centre_t             *pdata);
















VL53L1_Error VL53L1_nvm_decode_cal_peak_rate_map(
	uint16_t                             buf_size,
	uint8_t                             *pbuffer,
	VL53L1_cal_peak_rate_map_t          *pdata);















VL53L1_Error VL53L1_nvm_decode_additional_offset_cal_data(
	uint16_t                             buf_size,
	uint8_t                             *pbuffer,
	VL53L1_additional_offset_cal_data_t *pdata);















VL53L1_Error VL53L1_nvm_decode_fmt_range_results_data(
	uint16_t                             buf_size,
	uint8_t                             *pbuffer,
	VL53L1_decoded_nvm_fmt_range_data_t *pdata);















VL53L1_Error VL53L1_nvm_decode_fmt_info(
	uint16_t                       buf_size,
	uint8_t                       *pbuffer,
	VL53L1_decoded_nvm_fmt_info_t *pdata);















VL53L1_Error VL53L1_nvm_decode_ews_info(
	uint16_t                       buf_size,
	uint8_t                       *pbuffer,
	VL53L1_decoded_nvm_ews_info_t *pdata);














void VL53L1_nvm_format_encode(
	VL53L1_decoded_nvm_data_t *pnvm_info,
	uint8_t                   *pnvm_data);























VL53L1_Error VL53L1_read_nvm_raw_data(
	VL53L1_DEV     Dev,
	uint8_t        start_address,
	uint8_t        count,
	uint8_t       *pnvm_raw_data);
















VL53L1_Error VL53L1_read_nvm(
	VL53L1_DEV                 Dev,
	uint8_t                    nvm_format,
	VL53L1_decoded_nvm_data_t *pnvm_info);













VL53L1_Error VL53L1_read_nvm_optical_centre(
	VL53L1_DEV                           Dev,
	VL53L1_optical_centre_t             *pcentre);













VL53L1_Error VL53L1_read_nvm_cal_peak_rate_map(
	VL53L1_DEV                           Dev,
	VL53L1_cal_peak_rate_map_t          *pcal_data);













VL53L1_Error VL53L1_read_nvm_additional_offset_cal_data(
	VL53L1_DEV                           Dev,
	VL53L1_additional_offset_cal_data_t *pcal_data);


















VL53L1_Error VL53L1_read_nvm_fmt_range_results_data(
	VL53L1_DEV                           Dev,
	uint16_t                             range_results_select,
	VL53L1_decoded_nvm_fmt_range_data_t *prange_data);


#ifdef __cplusplus
}
#endif

#endif

