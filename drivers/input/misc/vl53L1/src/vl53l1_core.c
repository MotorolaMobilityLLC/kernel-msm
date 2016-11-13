
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




































#include "vl53l1_ll_def.h"
#include "vl53l1_platform.h"
#include "vl53l1_register_map.h"
#include "vl53l1_register_funcs.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_core.h"

#ifdef VL53L1_LOGGING
#include "vl53l1_debug.h"
#include "vl53l1_register_debug.h"
#endif

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, \
		status, fmt, ##__VA_ARGS__)


#define trace_print(level, ...) \
	VL53L1_trace_print_module_function(VL53L1_TRACE_MODULE_CORE, \
		level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


void  VL53L1_FCTN_00026(
	VL53L1_DEV        Dev)
{
	




	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->VL53L1_PRM_00082.VL53L1_PRM_00251    = VL53L1_DEF_00124;
	pdev->VL53L1_PRM_00082.VL53L1_PRM_00252    = VL53L1_DEF_00125;
	pdev->VL53L1_PRM_00082.VL53L1_PRM_00253    = VL53L1_DEF_00126;
	pdev->VL53L1_PRM_00082.VL53L1_PRM_00254 = VL53L1_DEF_00127;
}


void  VL53L1_FCTN_00030(
	VL53L1_DEV         Dev,
	VL53L1_DeviceState device_state)
{
	




	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->VL53L1_PRM_00065);

	pstate->VL53L1_PRM_00101  = device_state;
	pstate->VL53L1_PRM_00255  = 0;
	pstate->VL53L1_PRM_00256        = 0;
	pstate->VL53L1_PRM_00098       = 0;

	pstate->VL53L1_PRM_00064   = device_state;
	pstate->VL53L1_PRM_00257   = 0;
	pstate->VL53L1_PRM_00116  = 0;
	pstate->VL53L1_PRM_00066        = 0;

}


VL53L1_Error  VL53L1_FCTN_00071(
	VL53L1_DEV         Dev)
{
	









	VL53L1_Error        status  = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->VL53L1_PRM_00065);

	LOG_FUNCTION_START("");

	


	if ((pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 &&
		VL53L1_DEF_00130) == 0x00) {

		pstate->VL53L1_PRM_00064  = VL53L1_DEF_00056;
		pstate->VL53L1_PRM_00257  = 0;
		pstate->VL53L1_PRM_00116 = 0;
		pstate->VL53L1_PRM_00066       = 0;

	} else {

		




		if (pstate->VL53L1_PRM_00257 == 0xFF)
			pstate->VL53L1_PRM_00257 = 0x80;
		else
			pstate->VL53L1_PRM_00257++;

		


		switch (pstate->VL53L1_PRM_00064) {

		case VL53L1_DEF_00056:

			if ((pdev->VL53L1_PRM_00023.VL53L1_PRM_00246 &
				VL53L1_DEF_00131) > 0)
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00035;
			else
				if (pstate->VL53L1_PRM_00098 >=
					pdev->VL53L1_PRM_00015.VL53L1_PRM_00021)
					pstate->VL53L1_PRM_00064 =
						VL53L1_DEF_00129;
				else
					pstate->VL53L1_PRM_00064 =
						VL53L1_DEF_00128;

			pstate->VL53L1_PRM_00257  = 0;
			pstate->VL53L1_PRM_00116 = 0;
			pstate->VL53L1_PRM_00066       = 0;

		break;

		case VL53L1_DEF_00035:
			pstate->VL53L1_PRM_00257 = 0;
			pstate->VL53L1_PRM_00066      = 0;
			if (pstate->VL53L1_PRM_00098 >=
				pdev->VL53L1_PRM_00015.VL53L1_PRM_00021)
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00129;
			else
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00128;
		break;

		case VL53L1_DEF_00128:
			pstate->VL53L1_PRM_00066++;
			if (pstate->VL53L1_PRM_00098 >=
				pdev->VL53L1_PRM_00015.VL53L1_PRM_00021)
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00129;
			else
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00128;
		break;

		case VL53L1_DEF_00129:
			pstate->VL53L1_PRM_00066        = 0;
			pstate->VL53L1_PRM_00116 ^= 0x01;

			if (pstate->VL53L1_PRM_00098 >=
				pdev->VL53L1_PRM_00015.VL53L1_PRM_00021)
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00129;
			else
				pstate->VL53L1_PRM_00064 =
					VL53L1_DEF_00128;
		break;

		default:
			pstate->VL53L1_PRM_00064  =
				VL53L1_DEF_00056;
			pstate->VL53L1_PRM_00257  = 0;
			pstate->VL53L1_PRM_00116 = 0;
			pstate->VL53L1_PRM_00066       = 0;
		break;

		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00081(
	VL53L1_DEV         Dev)
{
	







	VL53L1_Error         status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t  *pstate       = &(pdev->VL53L1_PRM_00065);
	VL53L1_system_results_t   *psys_results = &(pdev->VL53L1_PRM_00028);

	uint8_t   device_range_status   = 0;
	uint8_t   device_stream_count   = 0;
	uint8_t   histogram_mode        = 0;

	LOG_FUNCTION_START("");

	device_range_status =
			psys_results->VL53L1_PRM_00080 &
			VL53L1_DEF_00132;

	device_stream_count = psys_results->VL53L1_PRM_00027;

	


	histogram_mode =
		pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 |
		VL53L1_DEF_00094;

	


	if ((pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 &
		VL53L1_DEF_00005) ==
		VL53L1_DEF_00005) {

		












		if (pstate->VL53L1_PRM_00064 ==
			VL53L1_DEF_00035) {

			if (histogram_mode == 0)
				if (device_range_status !=
					VL53L1_DEF_00017)
					status = VL53L1_ERROR_GPH_SYNC_CHECK_FAIL;

		} else {

			if (pstate->VL53L1_PRM_00257 != device_stream_count)
				status = VL53L1_ERROR_STREAM_COUNT_CHECK_FAIL;

		}

#ifdef VL53L1_LOGGING
		VL53L1_print_ll_driver_state(pstate);
		VL53L1_print_system_results(psys_results);
#endif

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error  VL53L1_FCTN_00072(
	VL53L1_DEV         Dev)
{
	




	VL53L1_Error         status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->VL53L1_PRM_00065);

	LOG_FUNCTION_START("");

	


	if ((pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 &&
		VL53L1_DEF_00130) == 0x00) {

		pstate->VL53L1_PRM_00101 = VL53L1_DEF_00056;
		pstate->VL53L1_PRM_00255 = 0;
		pstate->VL53L1_PRM_00256 =
				(pdev->VL53L1_PRM_00023.VL53L1_PRM_00246 &
				VL53L1_DEF_00131);
		pstate->VL53L1_PRM_00098      = 0;

	} else {

		




		if (pstate->VL53L1_PRM_00255 == 0xFF)
			pstate->VL53L1_PRM_00255 = 0x80;
		else
			pstate->VL53L1_PRM_00255++;

		




		switch (pstate->VL53L1_PRM_00101) {

		case VL53L1_DEF_00056:
			pstate->VL53L1_PRM_00098      = 0;
			pstate->VL53L1_PRM_00255 = 1;
			pstate->VL53L1_PRM_00101 =
				VL53L1_DEF_00133;
		break;

		case VL53L1_DEF_00133:
			pstate->VL53L1_PRM_00098++;
			if (pstate->VL53L1_PRM_00098 > pdev->VL53L1_PRM_00015.VL53L1_PRM_00021) {
				pstate->VL53L1_PRM_00098 = 0;
				pstate->VL53L1_PRM_00256 ^=
					VL53L1_DEF_00131;
			}
		break;

		case VL53L1_DEF_00134:
			pstate->VL53L1_PRM_00098++;
			if (pstate->VL53L1_PRM_00098 > pdev->VL53L1_PRM_00015.VL53L1_PRM_00021) {
				pstate->VL53L1_PRM_00098 = 0;
				pstate->VL53L1_PRM_00256 ^=
					VL53L1_DEF_00131;
			}
		break;

		default:
			pstate->VL53L1_PRM_00101 =
				VL53L1_DEF_00056;
			pstate->VL53L1_PRM_00255 = 0;
			pstate->VL53L1_PRM_00256       = 0;
			pstate->VL53L1_PRM_00098      = 0;
		break;

		}

	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00091(
		VL53L1_system_results_t  *pdata)
{
	





	pdata->VL53L1_PRM_00103                       = 0xFF;
	pdata->VL53L1_PRM_00080                           = 0xFF;
	pdata->VL53L1_PRM_00081                          = 0xFF;
	pdata->VL53L1_PRM_00027                           = 0xFF;

	pdata->VL53L1_PRM_00128         = 0xFFFF;
	pdata->VL53L1_PRM_00129        = 0xFFFF;
	pdata->VL53L1_PRM_00132            = 0xFFFF;
	pdata->VL53L1_PRM_00133                              = 0xFFFF;
	pdata->VL53L1_PRM_00134                              = 0xFFFF;
	pdata->VL53L1_PRM_00135 = 0xFFFF;
	pdata->VL53L1_PRM_00258 =
			0xFFFF;
	pdata->VL53L1_PRM_00259    = 0xFFFF;
	pdata->VL53L1_PRM_00260    = 0xFFFF;
	pdata->VL53L1_PRM_00131         = 0xFFFF;

	pdata->VL53L1_PRM_00142         = 0xFFFF;
	pdata->VL53L1_PRM_00143        = 0xFFFF;
	pdata->VL53L1_PRM_00144            = 0xFFFF;
	pdata->VL53L1_PRM_00145                              = 0xFFFF;
	pdata->VL53L1_PRM_00146                              = 0xFFFF;
	pdata->VL53L1_PRM_00147 = 0xFFFF;
	pdata->VL53L1_PRM_00261                            = 0xFFFF;
	pdata->VL53L1_PRM_00262                            = 0xFFFF;
	pdata->VL53L1_PRM_00263                            = 0xFFFF;
	pdata->VL53L1_PRM_00264                            = 0xFF;

}


void VL53L1_FCTN_00034(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00110,
	VL53L1_histogram_bin_data_t *pdata)
{

	




	uint16_t          i = 0;

	pdata->VL53L1_PRM_00036                   = 0;
	pdata->VL53L1_PRM_00025                = 0;

	pdata->VL53L1_PRM_00108                 = 0;
	pdata->VL53L1_PRM_00109               = VL53L1_DEF_00052;
	pdata->VL53L1_PRM_00110            = (uint8_t)VL53L1_PRM_00110;
	pdata->VL53L1_PRM_00119    = 0;

	pdata->VL53L1_PRM_00103           = 0;
	pdata->VL53L1_PRM_00080               = 0;
	pdata->VL53L1_PRM_00081              = 0;
	pdata->VL53L1_PRM_00027               = 0;

	pdata->VL53L1_PRM_00104 = 0;
	pdata->VL53L1_PRM_00105   = 0;
	pdata->VL53L1_PRM_00106       = 0;
	pdata->VL53L1_PRM_00111            = 0;

	pdata->VL53L1_PRM_00112                        = 0;
	pdata->VL53L1_PRM_00040                       = 0;
	pdata->VL53L1_PRM_00115                = 0;
	pdata->VL53L1_PRM_00123              = 0;

	pdata->VL53L1_PRM_00265                = 0;
	pdata->VL53L1_PRM_00266             = 0;

	for (i = 0 ; i < VL53L1_DEF_00135 ; i++)
		pdata->VL53L1_PRM_00267[i] = 0;

	for (i = 0 ; i < VL53L1_DEF_00052 ; i++)
		if (i < VL53L1_PRM_00110)
			pdata->VL53L1_PRM_00107[i] = bin_value;
		else
			pdata->VL53L1_PRM_00107[i] = 0;
}


void VL53L1_FCTN_00035(
	uint32_t                       bin_value,
	uint16_t                       VL53L1_PRM_00110,
	VL53L1_xtalk_histogram_data_t *pdata)
{

	




	uint16_t          i = 0;

	pdata->VL53L1_PRM_00036                   = 0;
	pdata->VL53L1_PRM_00025                = 0;

	pdata->VL53L1_PRM_00108                 = 0;
	pdata->VL53L1_PRM_00109               = VL53L1_DEF_00052;
	pdata->VL53L1_PRM_00110            = (uint8_t)VL53L1_PRM_00110;

	pdata->VL53L1_PRM_00105   = 0;
	pdata->VL53L1_PRM_00106       = 0;
	pdata->VL53L1_PRM_00111            = 0;

	pdata->VL53L1_PRM_00112                        = 0;
	pdata->VL53L1_PRM_00115                = 0;

	pdata->VL53L1_PRM_00265                = 0;

	for (i = 0 ; i < VL53L1_DEF_00052 ; i++)
		if (i < VL53L1_PRM_00110)
			pdata->VL53L1_PRM_00107[i] = bin_value;
		else
			pdata->VL53L1_PRM_00107[i] = 0;
}


void VL53L1_FCTN_00092(
	uint16_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	uint16_t   i    = 0;
	uint16_t   VL53L1_PRM_00034 = 0;

	VL53L1_PRM_00034 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00034 & 0x00FF);
		VL53L1_PRM_00034 = VL53L1_PRM_00034 >> 8;
	}
}

uint16_t VL53L1_FCTN_00082(
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	uint16_t   value = 0x00;

	while (count-- > 0)
		value = (value << 8) | (uint16_t)*pbuffer++;
	return value;
}


void VL53L1_FCTN_00093(
	int16_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	uint16_t   i    = 0;
	int16_t    VL53L1_PRM_00034 = 0;

	VL53L1_PRM_00034 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00034 & 0x00FF);
		VL53L1_PRM_00034 = VL53L1_PRM_00034 >> 8;
	}
}

int16_t VL53L1_FCTN_00094(
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	int16_t    value = 0x00;

	

	if (*pbuffer >= 0x80)
		value = 0xFFFF;

	while (count-- > 0)
		value = (value << 8) | (int16_t)*pbuffer++;
	return value;
}

void VL53L1_FCTN_00095(
	uint32_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	uint16_t   i    = 0;
	uint32_t   VL53L1_PRM_00034 = 0;

	VL53L1_PRM_00034 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00034 & 0x00FF);
		VL53L1_PRM_00034 = VL53L1_PRM_00034 >> 8;
	}
}

uint32_t VL53L1_FCTN_00083(
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	uint32_t   value = 0x00;

	while (count-- > 0)
		value = (value << 8) | (uint32_t)*pbuffer++;
	return value;
}


uint32_t VL53L1_FCTN_00096(
	uint16_t    count,
	uint8_t    *pbuffer,
	uint32_t    bit_mask,
	uint32_t    down_shift,
	uint32_t    offset)
{
	





	uint32_t   value = 0x00;

	

	while (count-- > 0)
		value = (value << 8) | (uint32_t)*pbuffer++;

	

	value =  value & bit_mask;
	if (down_shift > 0)
		value = value >> down_shift;

	

	value = value + offset;

	return value;
}


void VL53L1_FCTN_00097(
	int32_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	uint16_t   i    = 0;
	int32_t    VL53L1_PRM_00034 = 0;

	VL53L1_PRM_00034 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00034 & 0x00FF);
		VL53L1_PRM_00034 = VL53L1_PRM_00034 >> 8;
	}
}

int32_t VL53L1_FCTN_00098(
	uint16_t    count,
	uint8_t    *pbuffer)
{
	





	int32_t    value = 0x00;

	

	if (*pbuffer >= 0x80)
		value = 0xFFFFFFFF;

	while (count-- > 0)
		value = (value << 8) | (int32_t)*pbuffer++;
	return value;
}


VL53L1_Error VL53L1_FCTN_00022(
	VL53L1_DEV    Dev,
	uint8_t       VL53L1_PRM_00165)
{
	




	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
					Dev,
					VL53L1_DEF_00136,
					VL53L1_PRM_00165);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00099(
	VL53L1_DEV    Dev,
	uint8_t       value)
{
	




	VL53L1_Error status         = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->VL53L1_PRM_00096.VL53L1_PRM_00248 = value;

	status = VL53L1_WrByte(
				Dev,
				VL53L1_DEF_00137,
				pdev->VL53L1_PRM_00096.VL53L1_PRM_00248);

	return status;
}

VL53L1_Error VL53L1_FCTN_00029(
	VL53L1_DEV    Dev)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00099(Dev, 0x01);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00028(
	VL53L1_DEV    Dev)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00099(Dev, 0x00);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00100(
	VL53L1_DEV    Dev,
	uint8_t       value)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->VL53L1_PRM_00096.VL53L1_PRM_00250 = value;

	status = VL53L1_WrByte(
			Dev,
			VL53L1_DEF_00138,
			pdev->VL53L1_PRM_00096.VL53L1_PRM_00250);

	return status;
}


VL53L1_Error VL53L1_FCTN_00015(
	VL53L1_DEV    Dev)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00100(Dev, 0x01);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00101(
	VL53L1_DEV    Dev)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00100(Dev, 0x00);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00024(
	VL53L1_DEV    Dev)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->VL53L1_PRM_00096.VL53L1_PRM_00249 = VL53L1_DEF_00114;

	status = VL53L1_WrByte(
					Dev,
					VL53L1_DEF_00139,
					pdev->VL53L1_PRM_00096.VL53L1_PRM_00249);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00102(
	VL53L1_DEV    Dev)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
				Dev,
				VL53L1_DEF_00140,
				0x00);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	return status;
}


uint32_t VL53L1_FCTN_00021(
	uint16_t  VL53L1_PRM_00073,
	uint8_t   VL53L1_PRM_00040)
{
	







	uint32_t  VL53L1_PRM_00102        = 0;
	uint8_t   vcsel_period_pclks   = 0;
	uint32_t  macro_period_us      = 0;

	LOG_FUNCTION_START("");

	




	VL53L1_PRM_00102 = VL53L1_FCTN_00085(VL53L1_PRM_00073);

	




	vcsel_period_pclks = VL53L1_FCTN_00103(VL53L1_PRM_00040);

	












	macro_period_us =
			(uint32_t)VL53L1_DEF_00141 *
			VL53L1_PRM_00102;
	macro_period_us = macro_period_us >> 6;

	macro_period_us = macro_period_us * (uint32_t)vcsel_period_pclks;
	macro_period_us = macro_period_us >> 6;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "VL53L1_PRM_00102",
			VL53L1_PRM_00102);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "vcsel_period_pclks",
			vcsel_period_pclks);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "macro_period_us",
			macro_period_us);

	LOG_FUNCTION_END(0);

	return macro_period_us;
}


uint32_t VL53L1_FCTN_00085(
	uint16_t  VL53L1_PRM_00073)
{
	













	uint32_t  VL53L1_PRM_00102        = 0;

	LOG_FUNCTION_START("");

	VL53L1_PRM_00102 = (0x01 << 30) / VL53L1_PRM_00073;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "VL53L1_PRM_00102",
			VL53L1_PRM_00102);

	LOG_FUNCTION_END(0);

	return VL53L1_PRM_00102;
}


uint32_t VL53L1_FCTN_00104(
	uint16_t VL53L1_PRM_00073)
{
	





	uint32_t VL53L1_PRM_00102 = 0;
	uint32_t pll_period_mm = 0;

	LOG_FUNCTION_START("");

	




	VL53L1_PRM_00102  = VL53L1_FCTN_00085(VL53L1_PRM_00073);

	








	

	pll_period_mm =
			VL53L1_DEF_00142 *
			(VL53L1_PRM_00102 >> 2);

	

	pll_period_mm = (pll_period_mm + (0x01<<15)) >> 16;

	LOG_FUNCTION_END(0);

	return pll_period_mm;
}


uint16_t VL53L1_FCTN_00105(
	uint32_t VL53L1_PRM_00041,
	uint32_t macro_period_us)
{
	









	uint32_t timeout_mclks   = 0;
	uint16_t timeout_encoded = 0;

	LOG_FUNCTION_START("");

	timeout_mclks   =
			((VL53L1_PRM_00041 << 12) + (macro_period_us>>1)) /
			macro_period_us;
	timeout_encoded = VL53L1_FCTN_00106(timeout_mclks);

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_mclks",
			timeout_mclks, timeout_mclks);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_encoded",
			timeout_encoded, timeout_encoded);

	LOG_FUNCTION_END(0);

	return timeout_encoded;
}


uint16_t VL53L1_FCTN_00106(uint32_t timeout_mclks)
{
	




	uint16_t encoded_timeout = 0;
	uint32_t ls_byte = 0;
	uint16_t ms_byte = 0;

	if (timeout_mclks > 0) {
		ls_byte = timeout_mclks - 1;

		while ((ls_byte & 0xFFFFFF00) > 0) {
			ls_byte = ls_byte >> 1;
			ms_byte++;
		}

		encoded_timeout = (ms_byte << 8)
				+ (uint16_t) (ls_byte & 0x000000FF);
	}

	return encoded_timeout;
}


uint32_t VL53L1_FCTN_00084(uint16_t encoded_timeout)
{
	





	uint32_t timeout_macro_clks = 0;

	timeout_macro_clks = ((uint32_t) (encoded_timeout & 0x00FF)
			<< (uint32_t) ((encoded_timeout & 0xFF00) >> 8)) + 1;

	return timeout_macro_clks;
}


VL53L1_Error VL53L1_FCTN_00005(
	uint32_t                VL53L1_PRM_00007,
	uint32_t                VL53L1_PRM_00008,
	uint16_t                VL53L1_PRM_00073,
	VL53L1_timing_config_t *ptiming)
{
	







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint32_t macro_period_us    = 0;
	uint16_t timeout_encoded    = 0;

	LOG_FUNCTION_START("");

	if (VL53L1_PRM_00073 == 0) {
		status = VL53L1_ERROR_DIVISION_BY_ZERO;
	} else {

		

		macro_period_us =
				VL53L1_FCTN_00021(
					VL53L1_PRM_00073,
					ptiming->VL53L1_PRM_00076);

		

		timeout_encoded =
				VL53L1_FCTN_00105(
					VL53L1_PRM_00007,
					macro_period_us);

		ptiming->VL53L1_PRM_00204 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00205 =
				(uint8_t) (timeout_encoded & 0x00FF);

		

		timeout_encoded =
				VL53L1_FCTN_00105(
					VL53L1_PRM_00008,
					macro_period_us);

		ptiming->VL53L1_PRM_00117 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00118 =
				(uint8_t) (timeout_encoded & 0x00FF);

		

		macro_period_us =
				VL53L1_FCTN_00021(
					VL53L1_PRM_00073,
					ptiming->VL53L1_PRM_00122);

		

		timeout_encoded =
				VL53L1_FCTN_00105(
					VL53L1_PRM_00007,
					macro_period_us);

		ptiming->VL53L1_PRM_00206 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00207 =
				(uint8_t) (timeout_encoded & 0x00FF);

		

		timeout_encoded = VL53L1_FCTN_00105(
							VL53L1_PRM_00008,
							macro_period_us);

		ptiming->VL53L1_PRM_00120 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00121 =
				(uint8_t) (timeout_encoded & 0x00FF);
	}

	LOG_FUNCTION_END(0);

	return status;

}


uint8_t VL53L1_FCTN_00107(uint8_t vcsel_period_pclks)
{
	





	uint8_t vcsel_period_reg = 0;

	vcsel_period_reg = (vcsel_period_pclks >> 1) - 1;

	return vcsel_period_reg;
}


uint8_t VL53L1_FCTN_00103(uint8_t vcsel_period_reg)
{
	





	uint8_t vcsel_period_pclks = 0;

	vcsel_period_pclks = (vcsel_period_reg + 1) << 1;

	return vcsel_period_pclks;
}


uint32_t VL53L1_FCTN_00108(
	uint8_t  *pbuffer,
	uint8_t   no_of_bytes)
{
	




	uint8_t   i = 0;
	uint32_t  decoded_value = 0;

	for (i = 0 ; i < no_of_bytes ; i++)
		decoded_value = (decoded_value << 8) + (uint32_t)pbuffer[i];

	return decoded_value;
}


void VL53L1_FCTN_00109(
	uint32_t  ip_value,
	uint8_t   no_of_bytes,
	uint8_t  *pbuffer)
{
	




	uint8_t   i    = 0;
	uint32_t  VL53L1_PRM_00034 = 0;

	VL53L1_PRM_00034 = ip_value;
	for (i = 0; i < no_of_bytes ; i++) {
		pbuffer[no_of_bytes-i-1] = VL53L1_PRM_00034 & 0x00FF;
		VL53L1_PRM_00034 = VL53L1_PRM_00034 >> 8;
	}
}

uint32_t  VL53L1_FCTN_00086(
	uint32_t  VL53L1_PRM_00102,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  elapsed_mclks)
{
	









	uint64_t  tmp_long_int = 0;
	uint32_t  duration_us  = 0;

	




	duration_us = window_vclks * VL53L1_PRM_00102;

	



	duration_us = duration_us >> 12;

	

	tmp_long_int = (uint64_t)duration_us;

	




	duration_us = elapsed_mclks * vcsel_parm_pclks;

	



	duration_us = duration_us >> 4;

	



	tmp_long_int = tmp_long_int * (uint64_t)duration_us;

	



	tmp_long_int = tmp_long_int >> 12;

	

	if (tmp_long_int > 0xFFFFFFFF)
		tmp_long_int = 0xFFFFFFFF;

	duration_us  = (uint32_t)tmp_long_int;

	return duration_us;
}


uint16_t VL53L1_FCTN_00110(
	int32_t   VL53L1_PRM_00158,
	uint32_t  time_us)
{
	











	uint32_t  tmp_int   = 0;
	uint32_t  frac_bits = 7;
	uint16_t  rate_mcps = 0; 


	




	if (VL53L1_PRM_00158 > VL53L1_DEF_00143)
		tmp_int = VL53L1_DEF_00143;
	else if (VL53L1_PRM_00158 > 0)
		tmp_int = (uint32_t)VL53L1_PRM_00158;

	





	if (VL53L1_PRM_00158 > VL53L1_DEF_00144)
		frac_bits = 3;
	else
		frac_bits = 7;

	





	if (time_us > 0)
		tmp_int = ((tmp_int << frac_bits) + (time_us / 2)) / time_us;

	



	if (VL53L1_PRM_00158 > VL53L1_DEF_00144)
		tmp_int = tmp_int << 4;

	





	if (tmp_int > 0xFFFF)
		tmp_int = 0xFFFF;

	rate_mcps =  (uint16_t)tmp_int;

	return rate_mcps;
}


uint16_t VL53L1_FCTN_00111(
	uint32_t frac_bits,
	int32_t  peak_count_rate,
	int32_t  num_spads)
{

	uint32_t  tmp_int   = 0;

	

	uint16_t  rate_per_spad = 0;

	


	




	if (num_spads > 0) {
		tmp_int = (peak_count_rate << 8) << frac_bits;
	    tmp_int = (tmp_int + (num_spads / 2)) / num_spads;
	} else {
		tmp_int = ((peak_count_rate) << frac_bits);
	}

	


	if (tmp_int > 0xFFFF)
		tmp_int = 0xFFFF;

	rate_per_spad = (uint16_t)tmp_int;

	return rate_per_spad;
}


void  VL53L1_FCTN_00088(
	VL53L1_histogram_bin_data_t   *pdata,
	uint8_t                        remove_ambient_bins)
{
	





	uint8_t  bin            = 0;

	LOG_FUNCTION_START("");

	if (pdata->VL53L1_PRM_00119 > 0) {

		


		pdata->VL53L1_PRM_00268 = 0;
		for (bin = 0 ; bin < pdata->VL53L1_PRM_00119 ; bin++)
			pdata->VL53L1_PRM_00268 += pdata->VL53L1_PRM_00107[bin];

		pdata->VL53L1_PRM_00266 =
				pdata->VL53L1_PRM_00268 /
				(int32_t)pdata->VL53L1_PRM_00119;

		




		if (remove_ambient_bins > 0) {

			

			for (bin = pdata->VL53L1_PRM_00119 ;
					bin < pdata->VL53L1_PRM_00109 ; bin++)
				pdata->VL53L1_PRM_00107[bin-pdata->VL53L1_PRM_00119] =
					pdata->VL53L1_PRM_00107[bin];

			

			pdata->VL53L1_PRM_00110 =
					pdata->VL53L1_PRM_00110 -
					pdata->VL53L1_PRM_00119;
			pdata->VL53L1_PRM_00119 = 0;
		}

	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00087(
	VL53L1_histogram_bin_data_t   *pdata)
{
	





	uint32_t  period        = 0;
	uint32_t  VL53L1_PRM_00125         = 0;

	LOG_FUNCTION_START("");

	period = 2048 *
		(uint32_t)VL53L1_FCTN_00103(pdata->VL53L1_PRM_00040);

	VL53L1_PRM_00125  = period;
	VL53L1_PRM_00125 += (uint32_t)pdata->VL53L1_PRM_00105;
	VL53L1_PRM_00125 += (2048 * (uint32_t)pdata->VL53L1_PRM_00106);
	VL53L1_PRM_00125 -= (2048 * (uint32_t)pdata->VL53L1_PRM_00111);

	VL53L1_PRM_00125  = VL53L1_PRM_00125 % period;

	pdata->VL53L1_PRM_00265 = (uint16_t)VL53L1_PRM_00125;

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00089(
	VL53L1_DEV                     Dev,
	VL53L1_histogram_bin_data_t   *pdata)
{
	





	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	int32_t amb_thresh_low   = 0;
	int32_t amb_thresh_high  = 0;

	LOG_FUNCTION_START("");

	


	amb_thresh_low  = 1024 *
		(int32_t)pdev->VL53L1_PRM_00095.VL53L1_PRM_00231;
	amb_thresh_high = 1024 *
		(int32_t)pdev->VL53L1_PRM_00095.VL53L1_PRM_00232;

	if (pdev->VL53L1_PRM_00065.VL53L1_PRM_00116 == 0) {

		pdata->VL53L1_PRM_00267[5] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00219 >> 4;
		pdata->VL53L1_PRM_00267[4] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00219 & 0x0F;
		pdata->VL53L1_PRM_00267[3] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00218 >> 4;
		pdata->VL53L1_PRM_00267[2] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00218 & 0x0F;
		pdata->VL53L1_PRM_00267[1] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00217 >> 4;
		pdata->VL53L1_PRM_00267[0] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00217 & 0x0F;

		if (pdata->VL53L1_PRM_00268 > amb_thresh_high) {
			pdata->VL53L1_PRM_00267[5] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00227 >> 4;
			pdata->VL53L1_PRM_00267[4] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00227 & 0x0F;
			pdata->VL53L1_PRM_00267[3] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00226 >> 4;
			pdata->VL53L1_PRM_00267[2] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00226 & 0x0F;
			pdata->VL53L1_PRM_00267[1] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00225 >> 4;
			pdata->VL53L1_PRM_00267[0] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00225 & 0x0F;
		}

		if (pdata->VL53L1_PRM_00268 < amb_thresh_low) {
			pdata->VL53L1_PRM_00267[5] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00213 >> 4;
			pdata->VL53L1_PRM_00267[4] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00213 & 0x0F;
			pdata->VL53L1_PRM_00267[3] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00212 >> 4;
			pdata->VL53L1_PRM_00267[2] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00212 & 0x0F;
			pdata->VL53L1_PRM_00267[1] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00211 >> 4;
			pdata->VL53L1_PRM_00267[0] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00211 & 0x0F;
		}

	} else {

		pdata->VL53L1_PRM_00267[5] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00223 & 0x0F;
		pdata->VL53L1_PRM_00267[4] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00222 & 0x0F;
		pdata->VL53L1_PRM_00267[3] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00222 >> 4;
		pdata->VL53L1_PRM_00267[2] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00221 & 0x0F;
		pdata->VL53L1_PRM_00267[1] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00220 >> 4;
		pdata->VL53L1_PRM_00267[0] =
			pdev->VL53L1_PRM_00095.VL53L1_PRM_00220 & 0x0F;

		if (pdata->VL53L1_PRM_00268 > amb_thresh_high) {
			pdata->VL53L1_PRM_00267[5] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00230 >> 4;
			pdata->VL53L1_PRM_00267[4] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00230 & 0x0F;
			pdata->VL53L1_PRM_00267[3] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00229 >> 4;
			pdata->VL53L1_PRM_00267[2] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00229 & 0x0F;
			pdata->VL53L1_PRM_00267[1] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00228 >> 4;
			pdata->VL53L1_PRM_00267[0] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00228 & 0x0F;
		}

		if (pdata->VL53L1_PRM_00268 < amb_thresh_low) {
			pdata->VL53L1_PRM_00267[5] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00216 >> 4;
			pdata->VL53L1_PRM_00267[4] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00216 & 0x0F;
			pdata->VL53L1_PRM_00267[3] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00215 >> 4;
			pdata->VL53L1_PRM_00267[2] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00215 & 0x0F;
			pdata->VL53L1_PRM_00267[1] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00214 >> 4;
			pdata->VL53L1_PRM_00267[0] =
				pdev->VL53L1_PRM_00095.VL53L1_PRM_00214 & 0x0F;
		}
	}

	LOG_FUNCTION_END(0);

}


void VL53L1_FCTN_00047(
	uint8_t  spad_number,
	uint8_t  *prow,
	uint8_t  *pcol)
{

    





	if (spad_number > 127) {
		*prow = 8 + ((255-spad_number) & 0x07);
		*pcol = (spad_number-128) >> 3;
	} else {
		*prow = spad_number & 0x07;
		*pcol = (127-spad_number) >> 3;
	}

}


void VL53L1_FCTN_00045(
	uint8_t  row,
	uint8_t  col,
	uint8_t *pspad_number)
{
    




	if (row > 7)
		*pspad_number = 128 + (col << 3) + (15-row);
	else
		*pspad_number =  ((15-col) << 3)     + row;

}


void VL53L1_FCTN_00080(
	VL53L1_histogram_bin_data_t      *pbins,
	VL53L1_range_results_t           *phist,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore)
{
	




	uint8_t  i = 0;

	VL53L1_range_data_t  *pdata;

	LOG_FUNCTION_START("");

	


	VL53L1_FCTN_00091(psys);

	


	psys->VL53L1_PRM_00103 = pbins->VL53L1_PRM_00103;
	psys->VL53L1_PRM_00080     = phist->VL53L1_PRM_00037;
	psys->VL53L1_PRM_00081    = pbins->VL53L1_PRM_00081;
	psys->VL53L1_PRM_00027     = pbins->VL53L1_PRM_00027;

	pdata = &(phist->VL53L1_PRM_00034[0]);

	for (i = 0 ; i < phist->VL53L1_PRM_00037 ; i++) {

		switch (i) {

		case 0:

			psys->VL53L1_PRM_00128 = \
					pdata->VL53L1_PRM_00031;
			psys->VL53L1_PRM_00129 = \
					pdata->VL53L1_PRM_00029;
			psys->VL53L1_PRM_00131 = \
					pdata->VL53L1_PRM_00130;
			psys->VL53L1_PRM_00132 = \
					pdata->VL53L1_PRM_00030;

			psys->VL53L1_PRM_00133 = pdata->VL53L1_PRM_00032;
			psys->VL53L1_PRM_00134 = pdata->VL53L1_PRM_00125;

			psys->VL53L1_PRM_00135 = \
					(uint16_t)pdata->VL53L1_PRM_00033;

			psys->VL53L1_PRM_00146  = pdata->VL53L1_PRM_00265;

			pcore->VL53L1_PRM_00137 = \
					pdata->VL53L1_PRM_00136;
			pcore->VL53L1_PRM_00138 = \
					pdata->VL53L1_PRM_00063;
			pcore->VL53L1_PRM_00139 = \
					pdata->VL53L1_PRM_00123;
			pcore->VL53L1_PRM_00141 = \
					pdata->VL53L1_PRM_00140;

		break;
		case 1:

			psys->VL53L1_PRM_00142 = \
				pdata->VL53L1_PRM_00031;
			psys->VL53L1_PRM_00143 = \
				pdata->VL53L1_PRM_00029;
			psys->VL53L1_PRM_00144 = \
				pdata->VL53L1_PRM_00030;

			psys->VL53L1_PRM_00145 = pdata->VL53L1_PRM_00032;
			psys->VL53L1_PRM_00146 = pdata->VL53L1_PRM_00125;

			psys->VL53L1_PRM_00147 = \
				(uint16_t)pdata->VL53L1_PRM_00033;

			pcore->VL53L1_PRM_00148 = \
				pdata->VL53L1_PRM_00136;
			pcore->VL53L1_PRM_00149 = \
				pdata->VL53L1_PRM_00063;
			pcore->VL53L1_PRM_00150 = \
				pdata->VL53L1_PRM_00123;
			pcore->VL53L1_PRM_00151 = \
				pdata->VL53L1_PRM_00140;

		break;

		}

		pdata++;
	}

	LOG_FUNCTION_END(0);

}


