
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

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C" {
#endif









void VL53L1_FCTN_00033(
	VL53L1_DEV         Dev);










void VL53L1_FCTN_00038(
	VL53L1_DEV         Dev,
	VL53L1_DeviceState VL53L1_PRM_00069);












VL53L1_Error VL53L1_FCTN_00091(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_FCTN_00104(
	VL53L1_DEV         Dev);












VL53L1_Error VL53L1_FCTN_00092(
	VL53L1_DEV         Dev);












void VL53L1_FCTN_00115(
	VL53L1_system_results_t      *pdata);










void VL53L1_FCTN_00079(
	uint8_t                 VL53L1_PRM_00020,
	VL53L1_zone_results_t  *pdata);















































void VL53L1_FCTN_00113(
	uint8_t   even_bin0,
	uint8_t   even_bin1,
	uint8_t   even_bin2,
	uint8_t   even_bin3,
	uint8_t   even_bin4,
	uint8_t   even_bin5,
	uint8_t   odd_bin0,
	uint8_t   odd_bin1,
	uint8_t   odd_bin2,
	uint8_t   odd_bin3,
	uint8_t   odd_bin4,
	uint8_t   odd_bin5,
	VL53L1_histogram_config_t  *pdata);











void VL53L1_FCTN_00022(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00139,
	VL53L1_histogram_bin_data_t *pdata);











void VL53L1_FCTN_00042(
	uint32_t                       bin_value,
	uint16_t                       VL53L1_PRM_00139,
	VL53L1_xtalk_histogram_data_t *pdata);











void VL53L1_FCTN_00116(
		VL53L1_xtalk_histogram_data_t *pxtalk,
		VL53L1_histogram_bin_data_t   *phist);













void VL53L1_FCTN_00117(
	uint16_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














uint16_t VL53L1_FCTN_00105(
	uint16_t    count,
	uint8_t    *pbuffer);













void VL53L1_FCTN_00118(
	int16_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














int16_t VL53L1_FCTN_00119(
	uint16_t    count,
	uint8_t    *pbuffer);













void VL53L1_FCTN_00120(
	uint32_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














uint32_t VL53L1_FCTN_00106(
	uint16_t    count,
	uint8_t    *pbuffer);

















uint32_t VL53L1_FCTN_00121(
	uint16_t    count,
	uint8_t    *pbuffer,
	uint32_t    bit_mask,
	uint32_t    down_shift,
	uint32_t    offset);













void VL53L1_FCTN_00122(
	int32_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer);














int32_t VL53L1_FCTN_00123(
	uint16_t    count,
	uint8_t    *pbuffer);













VL53L1_Error VL53L1_FCTN_00029(
	VL53L1_DEV     Dev,
	uint8_t        VL53L1_PRM_00206);
















VL53L1_Error VL53L1_FCTN_00124(
	VL53L1_DEV         Dev,
	uint8_t            value);












VL53L1_Error VL53L1_FCTN_00037(
	VL53L1_DEV         Dev);














VL53L1_Error VL53L1_FCTN_00036(
	VL53L1_DEV         Dev);
















VL53L1_Error VL53L1_FCTN_00125(
	VL53L1_DEV         Dev,
	uint8_t            value);


















VL53L1_Error VL53L1_FCTN_00019(
	VL53L1_DEV         Dev);















VL53L1_Error VL53L1_FCTN_00126(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_FCTN_00031(
	VL53L1_DEV         Dev);













VL53L1_Error VL53L1_FCTN_00127(
	VL53L1_DEV         Dev);



















uint32_t VL53L1_FCTN_00028(
	uint16_t VL53L1_PRM_00098,
	uint8_t  VL53L1_PRM_00041);











uint32_t VL53L1_FCTN_00109(
	uint16_t VL53L1_PRM_00098);














uint32_t VL53L1_FCTN_00129(
	uint16_t VL53L1_PRM_00098);













uint16_t VL53L1_FCTN_00130(
	uint32_t  VL53L1_PRM_00042,
    uint32_t  macro_period_ns);











uint16_t VL53L1_FCTN_00131(
	uint32_t timeout_mclks);












uint32_t VL53L1_FCTN_00108(
	uint16_t encoded_timeout);


















VL53L1_Error  VL53L1_FCTN_00005(
	uint32_t                VL53L1_PRM_00006,
	uint32_t                VL53L1_PRM_00007,
	uint16_t                VL53L1_PRM_00098,
	VL53L1_timing_config_t *ptiming);












uint8_t VL53L1_FCTN_00132(
	uint8_t VL53L1_PRM_00311);












uint8_t VL53L1_FCTN_00128(
	uint8_t vcsel_period_reg);














uint32_t VL53L1_FCTN_00133(
	uint8_t  *pbuffer,
	uint8_t   no_of_bytes);












void   VL53L1_FCTN_00134(
	uint32_t  ip_value,
	uint8_t   no_of_bytes,
	uint8_t  *pbuffer);
















uint32_t VL53L1_FCTN_00110(
	uint32_t  VL53L1_PRM_00131,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  periods_elapsed_mclks);















uint16_t VL53L1_FCTN_00135(
	int32_t   VL53L1_PRM_00199,
	uint32_t  time_us);
















int32_t VL53L1_FCTN_00136(
	uint16_t  VL53L1_PRM_00098,
	uint16_t  VL53L1_PRM_00094,
	uint16_t  VL53L1_PRM_00306,
	int32_t   VL53L1_PRM_00202);


















uint16_t VL53L1_FCTN_00137(
	uint32_t  frac_bits,
	uint32_t  peak_count_rate,
	uint16_t  num_spads,
	uint32_t  max_output_value);
















uint32_t VL53L1_FCTN_00138(
	int32_t   VL53L1_PRM_00072,
	uint16_t  num_spads,
	uint32_t  duration);















void VL53L1_FCTN_00139(
	VL53L1_histogram_bin_data_t   *pdata);













void VL53L1_FCTN_00140(
	VL53L1_histogram_bin_data_t    *pdata);















void VL53L1_FCTN_00112(
	VL53L1_histogram_bin_data_t    *pdata);



















void VL53L1_FCTN_00141(
	int32_t                      ambient_threshold_sigma,
	VL53L1_histogram_bin_data_t *pdata);
















void VL53L1_FCTN_00100(
	VL53L1_histogram_bin_data_t    *pidata,
	VL53L1_histogram_bin_data_t    *podata);










void VL53L1_FCTN_00111(
	VL53L1_histogram_bin_data_t    *pdata);












void  VL53L1_FCTN_00107(
	VL53L1_DEV                     Dev,
	VL53L1_histogram_bin_data_t   *pdata);





















VL53L1_Error  VL53L1_FCTN_00101(
	VL53L1_DEV                Dev,
	VL53L1_range_results_t   *previous,
	VL53L1_range_results_t   *pcurrent);












void VL53L1_FCTN_00052(
	uint8_t   spad_number,
	uint8_t  *prow,
	uint8_t  *pcol);













void VL53L1_FCTN_00050(
	uint8_t  row,
	uint8_t  col,
	uint8_t *pspad_number);














void VL53L1_FCTN_00102(
	VL53L1_histogram_bin_data_t      *pbins,
	VL53L1_range_results_t           *phist,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore);














VL53L1_Error VL53L1_FCTN_00024 (
		VL53L1_histogram_bin_data_t *phist_input,
		VL53L1_histogram_bin_data_t *phist_output);















VL53L1_Error VL53L1_FCTN_00026 (
		uint8_t VL53L1_PRM_00064,
		VL53L1_histogram_bin_data_t *phist_sum,
		VL53L1_histogram_bin_data_t *phist_avg);













uint32_t VL53L1_FCTN_00142(
	uint32_t  num);












VL53L1_Error VL53L1_FCTN_00083(
	VL53L1_DEV  Dev);














VL53L1_Error VL53L1_FCTN_00103(
	VL53L1_DEV  Dev,
	VL53L1_range_results_t *presults);


#ifdef __cplusplus
}
#endif

#endif

