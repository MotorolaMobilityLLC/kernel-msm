
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
#include "vl53l1_ll_device.h"
#include "vl53l1_platform.h"
#include "vl53l1_platform_ipp.h"
#include "vl53l1_register_map.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_register_funcs.h"
#include "vl53l1_hist_map.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_core.h"
#include "vl53l1_wait.h"
#include "vl53l1_api_preset_modes.h"
#include "vl53l1_fpga_core.h"
#include "vl53l1_silicon_core.h"
#include "vl53l1_api_core.h"


#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, status, \
		fmt, ##__VA_ARGS__)


#define trace_print(level, ...) \
	VL53L1_trace_print_module_function(VL53L1_TRACE_MODULE_CORE, \
			level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


VL53L1_Error VL53L1_FCTN_00025(
	VL53L1_DEV           Dev,
	VL53L1_ll_version_t *pdata)
{
	





	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_FCTN_00026(Dev);

	memcpy(pdata, &(pdev->VL53L1_PRM_00082), sizeof(VL53L1_ll_version_t));

	return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_FCTN_00027(
	VL53L1_DEV        Dev,
	uint16_t         *pfw_version)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_RdWord(
				Dev,
				VL53L1_DEF_00044,
				pfw_version);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	




	if (status == VL53L1_ERROR_NONE) {

		trace_print(
			VL53L1_TRACE_LEVEL_INFO,
			"\tDevice Firmware Version : %6u\n",
			pdev->VL53L1_PRM_00083);

		if (pdev->VL53L1_PRM_00083 > 0x00 &&
			VL53L1_DEF_00045 > pdev->VL53L1_PRM_00083) {
			status = VL53L1_DEF_00046;
			trace_print(
				VL53L1_TRACE_LEVEL_ERRORS,
				"\tDevice Firmware too old! : found = %6u, min valid = %6u\n",
				pdev->VL53L1_PRM_00083,
				VL53L1_DEF_00045);
		}

		if (pdev->VL53L1_PRM_00083 > 0x00 &&
			VL53L1_DEF_00047 < pdev->VL53L1_PRM_00083) {
			status = VL53L1_DEF_00048;
			trace_print(
				VL53L1_TRACE_LEVEL_ERRORS,
				"\tDevice Firmware too new! : found = %6u, max valid = %6u\n",
				pdev->VL53L1_PRM_00083,
				VL53L1_DEF_00047);
		}

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00001(
	VL53L1_DEV        Dev,
	uint8_t           read_p2p_data)
{
	




	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	VL53L1_FCTN_00030(
			Dev,
			VL53L1_DEF_00049);

	pdev->VL53L1_PRM_00058.VL53L1_PRM_00020    = VL53L1_MAX_USER_ZONES;
	pdev->VL53L1_PRM_00058.VL53L1_PRM_00021 = 0;
	pdev->VL53L1_PRM_00084.VL53L1_PRM_00020      = VL53L1_MAX_USER_ZONES;
	pdev->VL53L1_PRM_00084.VL53L1_PRM_00021   = 0;

	pdev->VL53L1_PRM_00085       = VL53L1_DEF_00050;
	pdev->VL53L1_PRM_00002       = VL53L1_DEF_00006;
	pdev->VL53L1_PRM_00006  = VL53L1_DEF_00051;
	pdev->VL53L1_PRM_00007        =  2000;
	pdev->VL53L1_PRM_00008     = 13000;
	pdev->VL53L1_PRM_00009 =   100;
	pdev->VL53L1_PRM_00086                  =  0x00;

	



	VL53L1_FCTN_00026(Dev);

	








	if (read_p2p_data > 0 && status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00031(Dev);

	

	VL53L1_FCTN_00032(&(pdev->VL53L1_PRM_00039));

	

	VL53L1_FCTN_00033(&(pdev->VL53L1_PRM_00087));

	





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00004(
						Dev,
						pdev->VL53L1_PRM_00002,
						pdev->VL53L1_PRM_00007,
						pdev->VL53L1_PRM_00008,
						pdev->VL53L1_PRM_00009);

	

	VL53L1_FCTN_00034(
			0,
			VL53L1_DEF_00052,
			&(pdev->VL53L1_PRM_00067));

	VL53L1_FCTN_00034(
			0,
			VL53L1_DEF_00052,
			&(pdev->VL53L1_PRM_00088));

	

	VL53L1_FCTN_00035(
			0,
			VL53L1_DEF_00052,
			&(pdev->VL53L1_PRM_00068));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00031(
	VL53L1_DEV        Dev)
{

	








	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00027(
						Dev,
						&(pdev->VL53L1_PRM_00083));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(
						Dev,
						&(pdev->VL53L1_PRM_00011));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(
						Dev,
						&(pdev->VL53L1_PRM_00049));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00038(
						Dev,
						&(pdev->VL53L1_PRM_00003));

	




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_RdWord(
				Dev,
				VL53L1_DEF_00053,
				&(pdev->VL53L1_PRM_00046.VL53L1_PRM_00089));

	




	if (pdev->VL53L1_PRM_00011.VL53L1_PRM_00012 < 0x1000) {
		trace_print(
			VL53L1_TRACE_LEVEL_WARNING,
			"\nInvalid %s value (0x%04X) - forcing to 0x%04X\n\n",
			"pdev->VL53L1_PRM_00011.VL53L1_PRM_00012",
			pdev->VL53L1_PRM_00011.VL53L1_PRM_00012,
			0xBCCC);
		pdev->VL53L1_PRM_00011.VL53L1_PRM_00012 = 0xBCCC;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00003(
	VL53L1_DEV    Dev)
{
	





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
						Dev,
						VL53L1_DEF_00054,
						0x00);

	

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WaitUs(
				Dev,
				VL53L1_DEF_00055);

	

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
						Dev,
						VL53L1_DEF_00054,
						0x01);

	

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00039(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00040(
	VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	

	memcpy(
		&(pdev->VL53L1_PRM_00049),
		pcustomer,
		sizeof(VL53L1_customer_nvm_managed_t));

	

	memcpy(
		&(pdev->VL53L1_PRM_00068),
		pxtalkhisto,
		sizeof(VL53L1_xtalk_histogram_data_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00041(
	VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	

	memcpy(
		pcustomer,
		&(pdev->VL53L1_PRM_00049),
		sizeof(VL53L1_customer_nvm_managed_t));

	

	memcpy(
		pxtalkhisto,
		&(pdev->VL53L1_PRM_00068),
		sizeof(VL53L1_xtalk_histogram_data_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00042(
	VL53L1_DEV              Dev,
	uint32_t                VL53L1_PRM_00009)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->VL53L1_PRM_00046.VL53L1_PRM_00089 == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE) {
		pdev->VL53L1_PRM_00009 = VL53L1_PRM_00009;
		pdev->VL53L1_PRM_00010.VL53L1_PRM_00090 = \
			VL53L1_PRM_00009 *
			(uint32_t)pdev->VL53L1_PRM_00046.VL53L1_PRM_00089;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00043(
	VL53L1_DEV              Dev,
	uint32_t               *pinter_measurement_period_ms)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->VL53L1_PRM_00046.VL53L1_PRM_00089 == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE)
		*pinter_measurement_period_ms = \
			pdev->VL53L1_PRM_00010.VL53L1_PRM_00090 /
			(uint32_t)pdev->VL53L1_PRM_00046.VL53L1_PRM_00089;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00044(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *puser_zone)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  spad_no = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");

	

	VL53L1_FCTN_00045(
			puser_zone->VL53L1_PRM_00017,
			puser_zone->VL53L1_PRM_00016,
			&spad_no);

	








	xy_size = (puser_zone->VL53L1_PRM_00019 << 4) + puser_zone->VL53L1_PRM_00018;

	


	pdev->VL53L1_PRM_00023.VL53L1_PRM_00091              = spad_no;
	pdev->VL53L1_PRM_00023.VL53L1_PRM_00092 = xy_size;

	


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00046(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *puser_zone)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  x       = 0;
	uint8_t  y       = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");

	

	VL53L1_FCTN_00047(
			pdev->VL53L1_PRM_00023.VL53L1_PRM_00091,
			&y,
			&x);

	puser_zone->VL53L1_PRM_00016 = x;
	puser_zone->VL53L1_PRM_00017 = y;

	








	xy_size = pdev->VL53L1_PRM_00023.VL53L1_PRM_00092;

	puser_zone->VL53L1_PRM_00019 = xy_size >> 4;
	puser_zone->VL53L1_PRM_00018  = xy_size & 0x0F;

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_FCTN_00048(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *pmm_roi)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  x       = 0;
	uint8_t  y       = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");

	

	VL53L1_FCTN_00047(
			pdev->VL53L1_PRM_00003.VL53L1_PRM_00093,
			&y,
			&x);

	pmm_roi->VL53L1_PRM_00016 = x;
	pmm_roi->VL53L1_PRM_00017 = y;

	








	xy_size = pdev->VL53L1_PRM_00003.VL53L1_PRM_00094;

	pmm_roi->VL53L1_PRM_00019 = xy_size >> 4;
	pmm_roi->VL53L1_PRM_00018  = xy_size & 0x0F;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00006(
	VL53L1_DEV                 Dev,
	VL53L1_zone_config_t      *pzone_cfg)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	

	memcpy(&(pdev->VL53L1_PRM_00015), pzone_cfg, sizeof(VL53L1_zone_config_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00007(
	VL53L1_DEV                 Dev,
	VL53L1_zone_config_t      *pzone_cfg)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	

	memcpy(pzone_cfg, &(pdev->VL53L1_PRM_00015), sizeof(VL53L1_zone_config_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00004(
	VL53L1_DEV                 Dev,
	VL53L1_DevicePresetModes   device_preset_mode,
	uint32_t                   VL53L1_PRM_00007,
	uint32_t                   VL53L1_PRM_00008,
	uint32_t                   VL53L1_PRM_00009)
{
	





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_static_config_t        *pstatic       = &(pdev->VL53L1_PRM_00079);
	VL53L1_histogram_config_t     *phistogram    = &(pdev->VL53L1_PRM_00095);
	VL53L1_general_config_t       *pgeneral      = &(pdev->VL53L1_PRM_00074);
	VL53L1_timing_config_t        *ptiming       = &(pdev->VL53L1_PRM_00010);
	VL53L1_dynamic_config_t       *pdynamic      = &(pdev->VL53L1_PRM_00023);
	VL53L1_system_control_t       *psystem       = &(pdev->VL53L1_PRM_00096);
	VL53L1_zone_config_t          *pzone_cfg     = &(pdev->VL53L1_PRM_00015);

	LOG_FUNCTION_START("");

	

	pdev->VL53L1_PRM_00002                 = device_preset_mode;
	pdev->VL53L1_PRM_00007        = VL53L1_PRM_00007;
	pdev->VL53L1_PRM_00008     = VL53L1_PRM_00008;
	pdev->VL53L1_PRM_00009 = VL53L1_PRM_00009;

	


	VL53L1_FCTN_00030(
			Dev,
			VL53L1_DEF_00056);

	


	switch (device_preset_mode) {

	case VL53L1_DEF_00006:
		status = VL53L1_FCTN_00049(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00057:
		status = VL53L1_FCTN_00050(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00058:
		status = VL53L1_FCTN_00051(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00059:
		status = VL53L1_FCTN_00052(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00060:
		status = VL53L1_FCTN_00053(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00061:
		status = VL53L1_FCTN_00054(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00001:
		status = VL53L1_FCTN_00055(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00062:
		status = VL53L1_FCTN_00056(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00063:
		status = VL53L1_FCTN_00057(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00064:
		status = VL53L1_FCTN_00058(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00065:
		status = VL53L1_FCTN_00059(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00002:
		status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00003:
		status = VL53L1_FCTN_00061(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00007:
		status = VL53L1_FCTN_00062(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00066:
		status = VL53L1_FCTN_00063(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	default:
		status = VL53L1_ERROR_INVALID_PARAMS;
	break;

	}

	





	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00005(
				VL53L1_PRM_00007,
				VL53L1_PRM_00008,
				pdev->VL53L1_PRM_00011.VL53L1_PRM_00012,
				ptiming);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00042(
				Dev,
				VL53L1_PRM_00009);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00009(
	VL53L1_DEV                     Dev,
	uint8_t                        VL53L1_PRM_00006,
	VL53L1_DeviceConfigLevel       device_config_level)
{
	












	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_static_nvm_managed_t   *pstatic_nvm   = &(pdev->VL53L1_PRM_00011);
	VL53L1_customer_nvm_managed_t *pcustomer_nvm = &(pdev->VL53L1_PRM_00049);
	VL53L1_static_config_t        *pstatic       = &(pdev->VL53L1_PRM_00079);
	VL53L1_general_config_t       *pgeneral      = &(pdev->VL53L1_PRM_00074);
	VL53L1_timing_config_t        *ptiming       = &(pdev->VL53L1_PRM_00010);
	VL53L1_dynamic_config_t       *pdynamic      = &(pdev->VL53L1_PRM_00023);
	VL53L1_system_control_t       *psystem       = &(pdev->VL53L1_PRM_00096);

	uint8_t  *pbuffer                   = &buffer[0];
	uint16_t i                          = 0;
	uint16_t i2c_index                  = 0;
	uint16_t i2c_buffer_offset_bytes    = 0;
	uint16_t i2c_buffer_size_bytes      = 0;

	LOG_FUNCTION_START("");

	

	pdev->VL53L1_PRM_00006 = VL53L1_PRM_00006;

	


	psystem->VL53L1_PRM_00097 =
		(psystem->VL53L1_PRM_00097 & VL53L1_DEF_00067) |
		VL53L1_PRM_00006;

	


	status =
		VL53L1_FCTN_00044(
			Dev,
			&(pdev->VL53L1_PRM_00015.VL53L1_PRM_00022[pdev->VL53L1_PRM_00065.VL53L1_PRM_00098]));

	


	switch (device_config_level) {
	case VL53L1_DEF_00008:
		i2c_index = VL53L1_DEF_00068;
	break;
	case VL53L1_DEF_00034:
		i2c_index = VL53L1_DEF_00069;
	break;
	case VL53L1_DEF_00070:
		i2c_index = VL53L1_DEF_00071;
	break;
	case VL53L1_DEF_00072:
		i2c_index = VL53L1_DEF_00073;
	break;
	case VL53L1_DEF_00074:
		i2c_index = VL53L1_DEF_00075;
	break;
	case VL53L1_DEF_00076:
		i2c_index = VL53L1_DEF_00077;
	break;
	default:
		i2c_index = VL53L1_DEF_00078;
	break;
	}

	


	i2c_buffer_size_bytes = \
			(VL53L1_DEF_00078 +
			 VL53L1_DEF_00079) -
			 i2c_index;

	


	pbuffer = &buffer[0];
	for (i = 0 ; i < i2c_buffer_size_bytes ; i++)
		*pbuffer++ = 0;

	


	if (device_config_level >= VL53L1_DEF_00008 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
			VL53L1_DEF_00068 - i2c_index;

		status =
			VL53L1_FCTN_00064(
				pstatic_nvm,
				VL53L1_DEF_00080,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00034 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
			VL53L1_DEF_00069 - i2c_index;

		status =
			VL53L1_FCTN_00065(
				pcustomer_nvm,
				VL53L1_DEF_00081,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00070 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
			VL53L1_DEF_00071 - i2c_index;

		status =
			VL53L1_FCTN_00066(
				pstatic,
				VL53L1_DEF_00082,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00072 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_DEF_00073 - i2c_index;

		status =
			VL53L1_FCTN_00067(
				pgeneral,
				VL53L1_DEF_00083,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00074 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
				VL53L1_DEF_00075 - i2c_index;

		status =
			VL53L1_FCTN_00068(
				ptiming,
				VL53L1_DEF_00084,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00076 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
				VL53L1_DEF_00077 - i2c_index;

		status =
			VL53L1_FCTN_00069(
				pdynamic,
				VL53L1_DEF_00085,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
				VL53L1_DEF_00078 - i2c_index;

		status =
			VL53L1_FCTN_00070(
				psystem,
				VL53L1_DEF_00079,
				&buffer[i2c_buffer_offset_bytes]);
	}

	


	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				i2c_index,
				buffer,
				(uint32_t)i2c_buffer_size_bytes);

	


	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00071(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00072(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00010(
	VL53L1_DEV     Dev)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	


	pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 =
			(pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 & VL53L1_DEF_00067) |
			 VL53L1_DEF_00086;

	status = VL53L1_FCTN_00073(
				Dev,
				&pdev->VL53L1_PRM_00096);

	

	pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 =
			(pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 & VL53L1_DEF_00067);

	

	VL53L1_FCTN_00030(
			Dev,
			VL53L1_DEF_00056);

	return status;
}

VL53L1_Error VL53L1_FCTN_00074(
	VL53L1_DEV                     Dev,
	VL53L1_DeviceResultsLevel      device_results_level)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_system_results_t   *psystem_results = &(pdev->VL53L1_PRM_00028);
	VL53L1_core_results_t     *pcore_results   = &(pdev->VL53L1_PRM_00099);
	VL53L1_debug_results_t    *pdebug_results  = &(pdev->VL53L1_PRM_00046);

	uint16_t i2c_index               = VL53L1_DEF_00087;
	uint16_t i2c_buffer_offset_bytes = 0;
	uint16_t i2c_buffer_size_bytes   = 0;

	LOG_FUNCTION_START("");

	


	switch (device_results_level) {
	case VL53L1_DEF_00028:
		i2c_buffer_size_bytes =
				(VL53L1_DEF_00088 +
				VL53L1_DEF_00089) -
				i2c_index;
	break;
	case VL53L1_DEF_00090:
		i2c_buffer_size_bytes =
				(VL53L1_DEF_00091 +
				VL53L1_DEF_00092) -
				i2c_index;
	break;
	default:
		i2c_buffer_size_bytes =
				VL53L1_DEF_00093;
	break;
	}

	


	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				i2c_index,
				buffer,
				(uint32_t)i2c_buffer_size_bytes);

	


	if (device_results_level >= VL53L1_DEF_00028 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_DEF_00088 - i2c_index;

		status =
			VL53L1_FCTN_00075(
				VL53L1_DEF_00089,
				&buffer[i2c_buffer_offset_bytes],
				pdebug_results);
	}

	if (device_results_level >= VL53L1_DEF_00090 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_DEF_00091 - i2c_index;

		status =
			VL53L1_FCTN_00076(
				VL53L1_DEF_00092,
				&buffer[i2c_buffer_offset_bytes],
				pcore_results);
	}

	if (status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = 0;
		status =
			VL53L1_FCTN_00077(
				VL53L1_DEF_00093,
				&buffer[i2c_buffer_offset_bytes],
				psystem_results);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00013(
	VL53L1_DEV                    Dev,
	VL53L1_DeviceResultsLevel     device_results_level,
	VL53L1_zone_results_t        *pzone_results)
{
	










	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_range_results_t *presults = &(pdev->VL53L1_PRM_00100);

	uint8_t   i = 0;

	LOG_FUNCTION_START("");

	


	if ((pdev->VL53L1_PRM_00096.VL53L1_PRM_00097 &
		 VL53L1_DEF_00094)
		 == VL53L1_DEF_00094) {

		




		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00079(
							Dev,
							&(pdev->VL53L1_PRM_00067));

		if (status == VL53L1_ERROR_NONE) {

			pdev->VL53L1_PRM_00084.VL53L1_PRM_00020    = VL53L1_MAX_USER_ZONES;
			pdev->VL53L1_PRM_00084.VL53L1_PRM_00021 = pdev->VL53L1_PRM_00015.VL53L1_PRM_00021+1;
			pdev->VL53L1_PRM_00067.VL53L1_PRM_00036       = pdev->VL53L1_PRM_00065.VL53L1_PRM_00066;

			if (pdev->VL53L1_PRM_00065.VL53L1_PRM_00066 < pdev->VL53L1_PRM_00058.VL53L1_PRM_00020)
				memcpy(
					&(pdev->VL53L1_PRM_00084.VL53L1_PRM_00034[pdev->VL53L1_PRM_00065.VL53L1_PRM_00066]),
					&(pdev->VL53L1_PRM_00067),
					sizeof(VL53L1_histogram_bin_data_t));
		}

		




		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_ipp_hist_process_data(
						Dev,
						&(pdev->VL53L1_PRM_00087),
						&(pdev->VL53L1_PRM_00067),
						&(pdev->VL53L1_PRM_00088),
						presults);

		





		if (status == VL53L1_ERROR_NONE)
			VL53L1_FCTN_00080(
					&(pdev->VL53L1_PRM_00067),
					presults,
				    &(pdev->VL53L1_PRM_00028),
				    &(pdev->VL53L1_PRM_00099));

	} else {

		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00074(
							Dev,
							device_results_level);

		if (status == VL53L1_ERROR_NONE)
			VL53L1_FCTN_00078(
					&(pdev->VL53L1_PRM_00028),
					&(pdev->VL53L1_PRM_00099),
					presults);
	}

	

	pdev->VL53L1_PRM_00058.VL53L1_PRM_00101 = pdev->VL53L1_PRM_00065.VL53L1_PRM_00101;
	pdev->VL53L1_PRM_00058.VL53L1_PRM_00064  = pdev->VL53L1_PRM_00065.VL53L1_PRM_00064;

	if (status == VL53L1_ERROR_NONE) {

		

		pdev->VL53L1_PRM_00058.VL53L1_PRM_00020    = VL53L1_MAX_USER_ZONES;

		pdev->VL53L1_PRM_00058.VL53L1_PRM_00021 = pdev->VL53L1_PRM_00015.VL53L1_PRM_00021+1;
		for (i = 0 ; i < presults->VL53L1_PRM_00037 ; i++)
			presults->VL53L1_PRM_00034[i].VL53L1_PRM_00036 = pdev->VL53L1_PRM_00065.VL53L1_PRM_00066;

		if (pdev->VL53L1_PRM_00065.VL53L1_PRM_00066 < pdev->VL53L1_PRM_00058.VL53L1_PRM_00020)
			memcpy(
				&(pdev->VL53L1_PRM_00058.VL53L1_PRM_00034[pdev->VL53L1_PRM_00065.VL53L1_PRM_00066]),
				presults,
				sizeof(VL53L1_range_results_t));
	}

	


	memcpy(
		pzone_results,
		&(pdev->VL53L1_PRM_00058),
		sizeof(VL53L1_zone_results_t));

	





	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00081(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00011(
	VL53L1_DEV        Dev,
	uint8_t           VL53L1_PRM_00006)
{

	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	

	



	

	


	


	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00009(
					Dev,
					VL53L1_PRM_00006,
					VL53L1_DEF_00072);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00079(
		VL53L1_DEV                   Dev,
		VL53L1_histogram_bin_data_t *pdata)
{
	




	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_static_nvm_managed_t   *pstat_nvm = &(pdev->VL53L1_PRM_00011);
	VL53L1_static_config_t        *pstat_cfg = &(pdev->VL53L1_PRM_00079);
	VL53L1_general_config_t       *pgen_cfg  = &(pdev->VL53L1_PRM_00074);
	VL53L1_timing_config_t        *ptim_cfg  = &(pdev->VL53L1_PRM_00010);

	uint8_t    buffer[VL53L1_MAX_I2C_XFER_SIZE];
	uint8_t   *pbuffer = &buffer[0];
	uint8_t    bin_23_0 = 0x00;
	uint16_t   bin                      = 0;
	uint16_t   i2c_buffer_offset_bytes  = 0;
	uint16_t   encoded_timeout          = 0;

	uint32_t   VL53L1_PRM_00102            = 0;
	uint32_t   periods_elapsed_tmp      = 0;

	LOG_FUNCTION_START("");

	




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
					Dev,
					VL53L1_DEF_00095,
					pbuffer,
					VL53L1_DEF_00096);

	




	pdata->VL53L1_PRM_00103               = *(pbuffer +   0);
	pdata->VL53L1_PRM_00080                   = *(pbuffer +   1);
	pdata->VL53L1_PRM_00081                  = *(pbuffer +   2);
	pdata->VL53L1_PRM_00027                   = *(pbuffer +   3);
	pdata->VL53L1_PRM_00104 =
		VL53L1_FCTN_00082(2, pbuffer +   4);

	




	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00097 - \
			VL53L1_DEF_00095;

	pbuffer = &buffer[i2c_buffer_offset_bytes];

	pdata->VL53L1_PRM_00105 =
			VL53L1_FCTN_00082(2, pbuffer);

	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00098 - \
			VL53L1_DEF_00095;

	pdata->VL53L1_PRM_00106 = buffer[i2c_buffer_offset_bytes];

	


	pdev->VL53L1_PRM_00046.VL53L1_PRM_00105 =
			pdata->VL53L1_PRM_00105;
	pdev->VL53L1_PRM_00046.VL53L1_PRM_00106 =
			pdata->VL53L1_PRM_00106;

	





	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00099 - \
			VL53L1_DEF_00095;

	bin_23_0 = buffer[i2c_buffer_offset_bytes] << 2;

	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00100 - \
			VL53L1_DEF_00095;

	bin_23_0 += buffer[i2c_buffer_offset_bytes];

	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00101 - \
			VL53L1_DEF_00095;

	buffer[i2c_buffer_offset_bytes] = bin_23_0;

	





	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00102 - \
			VL53L1_DEF_00095;

	pbuffer = &buffer[i2c_buffer_offset_bytes];
	for (bin = 0 ; bin < VL53L1_DEF_00052 ; bin++) {
		pdata->VL53L1_PRM_00107[bin] =
				(int32_t)VL53L1_FCTN_00083(3, pbuffer);
		pbuffer += 3;
	}

	

	pdata->VL53L1_PRM_00108               = 0;
	pdata->VL53L1_PRM_00109             = VL53L1_DEF_00052;
	pdata->VL53L1_PRM_00110          = VL53L1_DEF_00052;

	pdata->VL53L1_PRM_00111 = pgen_cfg->VL53L1_PRM_00111;

	


	pdata->VL53L1_PRM_00112 =
		((uint16_t)pgen_cfg->VL53L1_PRM_00113) << 4;
	pdata->VL53L1_PRM_00112 +=
		(uint16_t)pstat_cfg->VL53L1_PRM_00114;

	

	pdata->VL53L1_PRM_00115 =
		pstat_nvm->VL53L1_PRM_00012;

	




	if (pdev->VL53L1_PRM_00065.VL53L1_PRM_00116 == 0) {

		encoded_timeout = \
			(ptim_cfg->VL53L1_PRM_00117 << 8) \
			+ ptim_cfg->VL53L1_PRM_00118;
		pdata->VL53L1_PRM_00040 =  ptim_cfg->VL53L1_PRM_00076;
		pdata->VL53L1_PRM_00119  = 4;

	} else {

		encoded_timeout = \
			(ptim_cfg->VL53L1_PRM_00120 << 8) \
			 + ptim_cfg->VL53L1_PRM_00121;
		pdata->VL53L1_PRM_00040 = ptim_cfg->VL53L1_PRM_00122;
		pdata->VL53L1_PRM_00119  = 0;

	}

	pdata->VL53L1_PRM_00123 =
		VL53L1_FCTN_00084(encoded_timeout);


	




	VL53L1_PRM_00102 =
		VL53L1_FCTN_00085(pdata->VL53L1_PRM_00115);

	


	periods_elapsed_tmp = pdata->VL53L1_PRM_00123 + 1;

	




	pdata->VL53L1_PRM_00060 =
		VL53L1_FCTN_00086(
			VL53L1_PRM_00102,
			(uint32_t)pdata->VL53L1_PRM_00112,
			VL53L1_DEF_00103,
			periods_elapsed_tmp);

	pdata->VL53L1_PRM_00124     = 0;

	


	VL53L1_FCTN_00087(pdata);

	


	VL53L1_FCTN_00088(pdata, 0);

	


	VL53L1_FCTN_00089(Dev, pdata);

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00078(
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore,
	VL53L1_range_results_t           *presults)
{
	uint8_t  i = 0;

	VL53L1_range_data_t  *pdata;

	LOG_FUNCTION_START("");

	


	presults->VL53L1_PRM_00035    = VL53L1_MAX_RANGE_RESULTS;
	presults->VL53L1_PRM_00037 = 2;

	pdata = &(presults->VL53L1_PRM_00034[0]);

	for (i = 0 ; i < presults->VL53L1_PRM_00037 ; i++) {

		pdata->VL53L1_PRM_00036      = 0;
		pdata->VL53L1_PRM_00126     = i;
		pdata->VL53L1_PRM_00025   = 0;
		pdata->VL53L1_PRM_00127 = psys->VL53L1_PRM_00027;
		pdata->VL53L1_PRM_00026 = psys->VL53L1_PRM_00080;

		switch (i) {

		case 0:

			pdata->VL53L1_PRM_00031 =
				psys->VL53L1_PRM_00128;
			pdata->VL53L1_PRM_00029 =
				psys->VL53L1_PRM_00129;
			pdata->VL53L1_PRM_00130 =
				psys->VL53L1_PRM_00131;
			pdata->VL53L1_PRM_00030 =
				psys->VL53L1_PRM_00132;

			pdata->VL53L1_PRM_00032 =
				psys->VL53L1_PRM_00133;
			pdata->VL53L1_PRM_00125 =
				psys->VL53L1_PRM_00134;
			pdata->VL53L1_PRM_00033 =
				(int16_t)psys->VL53L1_PRM_00135;

			pdata->VL53L1_PRM_00136 =
				pcore->VL53L1_PRM_00137;
			pdata->VL53L1_PRM_00063 =
				pcore->VL53L1_PRM_00138;
			pdata->VL53L1_PRM_00123 =
				pcore->VL53L1_PRM_00139;
			pdata->VL53L1_PRM_00140 =
				pcore->VL53L1_PRM_00141;

		break;
		case 1:

			pdata->VL53L1_PRM_00031 =
				psys->VL53L1_PRM_00142;
			pdata->VL53L1_PRM_00029 =
				psys->VL53L1_PRM_00143;
			pdata->VL53L1_PRM_00130 =
				0xFFFF;
			pdata->VL53L1_PRM_00030 =
				psys->VL53L1_PRM_00144;

			pdata->VL53L1_PRM_00032 =
				psys->VL53L1_PRM_00145;
			pdata->VL53L1_PRM_00125 =
				psys->VL53L1_PRM_00146;
			pdata->VL53L1_PRM_00033  =
				(int16_t)psys->VL53L1_PRM_00147;

			pdata->VL53L1_PRM_00136 =
				pcore->VL53L1_PRM_00148;
			pdata->VL53L1_PRM_00063 =
				pcore->VL53L1_PRM_00149;
			pdata->VL53L1_PRM_00123  =
				pcore->VL53L1_PRM_00150;
			pdata->VL53L1_PRM_00140 =
				pcore->VL53L1_PRM_00151;

		break;

		}

		pdata++;
	}

	LOG_FUNCTION_END(0);
}
