
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




































#ifndef _VL53L1_CORE_H_
#define _VL53L1_CORE_H_

#include "vl53l1_ll_def.h"
#include "vl53l1_platform.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_hist_structs.h"

#ifdef __cplusplus
extern "C" {
#endif









void VL53L1_FCTN_00026(
	VL53L1_DEV         Dev);










void VL53L1_FCTN_00030(
	VL53L1_DEV         Dev,
	VL53L1_DeviceState VL53L1_PRM_00065);












VL53L1_Error VL53L1_FCTN_00071(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_FCTN_00081(
	VL53L1_DEV         Dev);












VL53L1_Error VL53L1_FCTN_00072(
	VL53L1_DEV         Dev);












void VL53L1_FCTN_00091(
	VL53L1_system_results_t      *pdata);











void VL53L1_FCTN_00034(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00110,
	VL53L1_histogram_bin_data_t *pdata);










void VL53L1_FCTN_00035(
	uint32_t                       bin_value,
	uint16_t                       VL53L1_PRM_00110,
	VL53L1_xtalk_histogram_data_t *pdata);













void VL53L1_FCTN_00092(
	uint16_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














uint16_t VL53L1_FCTN_00082(
	uint16_t    count,
	uint8_t    *pbuffer);













void VL53L1_FCTN_00093(
	int16_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














int16_t VL53L1_FCTN_00094(
	uint16_t    count,
	uint8_t    *pbuffer);













void VL53L1_FCTN_00095(
	uint32_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














uint32_t VL53L1_FCTN_00083(
	uint16_t    count,
	uint8_t    *pbuffer);

















uint32_t VL53L1_FCTN_00096(
	uint16_t    count,
	uint8_t    *pbuffer,
	uint32_t    bit_mask,
	uint32_t    down_shift,
	uint32_t    offset);













void VL53L1_FCTN_00097(
	int32_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














int32_t VL53L1_FCTN_00098(
	uint16_t    count,
	uint8_t    *pbuffer);













VL53L1_Error VL53L1_FCTN_00022(
	VL53L1_DEV     Dev,
	uint8_t        VL53L1_PRM_00165);
















VL53L1_Error VL53L1_FCTN_00099(
	VL53L1_DEV         Dev,
	uint8_t            value);












VL53L1_Error VL53L1_FCTN_00029(
	VL53L1_DEV         Dev);














VL53L1_Error VL53L1_FCTN_00028(
	VL53L1_DEV         Dev);
















VL53L1_Error VL53L1_FCTN_00100(
	VL53L1_DEV         Dev,
	uint8_t            value);


















VL53L1_Error VL53L1_FCTN_00015(
	VL53L1_DEV         Dev);















VL53L1_Error VL53L1_FCTN_00101(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_FCTN_00024(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_FCTN_00102(
	VL53L1_DEV         Dev);



















uint32_t VL53L1_FCTN_00021(
	uint16_t VL53L1_PRM_00073,
	uint8_t  VL53L1_PRM_00040);











uint32_t VL53L1_FCTN_00085(
	uint16_t VL53L1_PRM_00073);














uint32_t VL53L1_FCTN_00104(
	uint16_t VL53L1_PRM_00073);













uint16_t VL53L1_FCTN_00105(
	uint32_t  VL53L1_PRM_00041,
    uint32_t  macro_period_ns);











uint16_t VL53L1_FCTN_00106(
	uint32_t timeout_mclks);












uint32_t VL53L1_FCTN_00084(
	uint16_t encoded_timeout);


















VL53L1_Error  VL53L1_FCTN_00005(
	uint32_t                VL53L1_PRM_00007,
	uint32_t                VL53L1_PRM_00008,
	uint16_t                VL53L1_PRM_00073,
	VL53L1_timing_config_t *ptiming);












uint8_t VL53L1_FCTN_00107(
	uint8_t vcsel_period_pclks);












uint8_t VL53L1_FCTN_00103(
	uint8_t vcsel_period_reg);














uint32_t VL53L1_FCTN_00108(
	uint8_t  *pbuffer,
	uint8_t   no_of_bytes);












void   VL53L1_FCTN_00109(
	uint32_t  ip_value,
	uint8_t   no_of_bytes,
	uint8_t  *pbuffer);
















uint32_t VL53L1_FCTN_00086(
	uint32_t  VL53L1_PRM_00102,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  periods_elapsed_mclks);















uint16_t VL53L1_FCTN_00110(
	int32_t   VL53L1_PRM_00158,
	uint32_t  time_us);
















uint16_t VL53L1_FCTN_00111(
	uint32_t frac_bits,
	int32_t  peak_count_rate,
	int32_t  num_spads);
















void VL53L1_FCTN_00088(
	VL53L1_histogram_bin_data_t    *pdata,
	uint8_t                         remove_ambient_bins);










void VL53L1_FCTN_00087(
	VL53L1_histogram_bin_data_t    *pdata);












void  VL53L1_FCTN_00089(
	VL53L1_DEV                     Dev,
	VL53L1_histogram_bin_data_t   *pdata);











void VL53L1_FCTN_00047(
	uint8_t   spad_number,
	uint8_t  *prow,
	uint8_t  *pcol);













void VL53L1_FCTN_00045(
	uint8_t  row,
	uint8_t  col,
	uint8_t *pspad_number);














void VL53L1_FCTN_00080(
	VL53L1_histogram_bin_data_t      *pbins,
	VL53L1_range_results_t           *phist,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore);


#ifdef __cplusplus
}
#endif

#endif 

