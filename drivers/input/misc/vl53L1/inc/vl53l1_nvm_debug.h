
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





































#ifndef _VL53L1_NVM_DEBUG_H_
#define _VL53L1_NVM_DEBUG_H_

#include "vl53l1_ll_def.h"
#include "vl53l1_nvm_structs.h"



#ifdef __cplusplus
extern "C"
{
#endif

#ifdef VL53L1_LOG_ENABLE












void VL53L1_print_nvm_raw_data(
	uint8_t                       *pnvm_raw_data,
	uint32_t                       trace_flags);












void VL53L1_print_decoded_nvm_data(
	VL53L1_decoded_nvm_data_t *pdata,
	char                      *pprefix,
	uint32_t                   trace_flags);












void VL53L1_print_decoded_nvm_fmt_range_data(
	VL53L1_decoded_nvm_fmt_range_data_t *pdata,
	char                                *pprefix,
	uint32_t                             trace_flags);












void VL53L1_print_decoded_nvm_fmt_info(
	VL53L1_decoded_nvm_fmt_info_t *pdata,
	char                          *pprefix,
	uint32_t                       trace_flags);











void VL53L1_print_decoded_nvm_ews_info(
	VL53L1_decoded_nvm_ews_info_t *pdata,
	char                          *pprefix,
	uint32_t                       trace_flags);

#endif


#ifdef __cplusplus
}
#endif

#endif

