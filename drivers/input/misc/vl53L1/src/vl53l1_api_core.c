
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
#include "vl53l1_zone_presets.h"
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


VL53L1_Error VL53L1_FCTN_00032(
	VL53L1_DEV           Dev,
	VL53L1_ll_version_t *pdata)
{






	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_FCTN_00033(Dev);

	memcpy(pdata, &(pdev->VL53L1_PRM_00107), sizeof(VL53L1_ll_version_t));

	return VL53L1_ERROR_NONE;
}


VL53L1_Error VL53L1_FCTN_00034(
	VL53L1_DEV    Dev,
	uint8_t      *pfpga_system)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_RdByte(
			Dev,
			VL53L1_DEF_00049,
			pfpga_system);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00035(
	VL53L1_DEV        Dev,
	uint16_t         *pfw_version)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_RdWord(
				Dev,
				VL53L1_DEF_00050,
				pfw_version);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00001(
	VL53L1_DEV        Dev,
	uint8_t           read_p2p_data)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t    *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	LOG_FUNCTION_START("");

	VL53L1_FCTN_00038(
			Dev,
			VL53L1_DEF_00051);

	pres->VL53L1_PRM_00059.VL53L1_PRM_00063    = VL53L1_MAX_RANGE_RESULTS;
	pres->VL53L1_PRM_00059.VL53L1_PRM_00037 = 0;
	pres->VL53L1_PRM_00075.VL53L1_PRM_00019       = VL53L1_MAX_USER_ZONES;
	pres->VL53L1_PRM_00075.VL53L1_PRM_00020    = 0;
	pres->VL53L1_PRM_00074.VL53L1_PRM_00019         = VL53L1_MAX_USER_ZONES;
	pres->VL53L1_PRM_00074.VL53L1_PRM_00020      = 0;

	pdev->VL53L1_PRM_00108       = VL53L1_DEF_00052;
	pdev->VL53L1_PRM_00086       = VL53L1_DEF_00003;
	pdev->VL53L1_PRM_00109       = VL53L1_DEF_00053;
	pdev->VL53L1_PRM_00005  = VL53L1_DEF_00054;
	pdev->VL53L1_PRM_00006        =  2000;
	pdev->VL53L1_PRM_00007     = 13000;
	pdev->VL53L1_PRM_00008 =   100;
	pdev->VL53L1_PRM_00110                  =  0x00;

	pdev->VL53L1_PRM_00085.VL53L1_PRM_00063    = VL53L1_MAX_OFFSET_RANGE_RESULTS;
	pdev->VL53L1_PRM_00085.VL53L1_PRM_00037 = 0;




	VL53L1_FCTN_00033(Dev);










	if (read_p2p_data > 0 && status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00039(Dev);



	VL53L1_FCTN_00040(&(pdev->VL53L1_PRM_00040));



	VL53L1_FCTN_00041(&(pdev->VL53L1_PRM_00097));







	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00004(
						Dev,
						pdev->VL53L1_PRM_00086,
						pdev->VL53L1_PRM_00006,
						pdev->VL53L1_PRM_00007,
						pdev->VL53L1_PRM_00008);



	VL53L1_FCTN_00022(
			0,
			VL53L1_DEF_00035,
			&(pdev->VL53L1_PRM_00073));

	VL53L1_FCTN_00022(
			0,
			VL53L1_DEF_00035,
			&(pdev->VL53L1_PRM_00111));



	VL53L1_FCTN_00042(
			0,
			VL53L1_DEF_00035,
			&(pdev->VL53L1_PRM_00076));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00039(
	VL53L1_DEV        Dev)
{










	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00034(
						Dev,
						&(pdev->VL53L1_PRM_00112));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00043(
						Dev,
						&(pdev->VL53L1_PRM_00010));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00044(
						Dev,
						&(pdev->VL53L1_PRM_00050));

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00045(
						Dev,
						&(pdev->VL53L1_PRM_00002));




	if (status == VL53L1_ERROR_NONE) {

		pdev->VL53L1_PRM_00097.VL53L1_PRM_00080 =
				pdev->VL53L1_PRM_00050.VL53L1_PRM_00080;
		pdev->VL53L1_PRM_00097.VL53L1_PRM_00078 =
				pdev->VL53L1_PRM_00050.VL53L1_PRM_00078;
		pdev->VL53L1_PRM_00097.VL53L1_PRM_00079 =
				pdev->VL53L1_PRM_00050.VL53L1_PRM_00079;

		pdev->VL53L1_PRM_00097.VL53L1_PRM_00084 =
				pdev->VL53L1_PRM_00050.VL53L1_PRM_00084;
		pdev->VL53L1_PRM_00097.VL53L1_PRM_00082 =
				pdev->VL53L1_PRM_00050.VL53L1_PRM_00082;
		pdev->VL53L1_PRM_00097.VL53L1_PRM_00083 =
				pdev->VL53L1_PRM_00050.VL53L1_PRM_00083;
	}






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_RdWord(
				Dev,
				VL53L1_DEF_00055,
				&(pdev->VL53L1_PRM_00047.VL53L1_PRM_00113));






	if (pdev->VL53L1_PRM_00010.VL53L1_PRM_00011 < 0x1000) {
		trace_print(
			VL53L1_TRACE_LEVEL_WARNING,
			"\nInvalid %s value (0x%04X) - forcing to 0x%04X\n\n",
			"pdev->VL53L1_PRM_00010.VL53L1_PRM_00011",
			pdev->VL53L1_PRM_00010.VL53L1_PRM_00011,
			0xBCCC);
		pdev->VL53L1_PRM_00010.VL53L1_PRM_00011 = 0xBCCC;
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
						VL53L1_DEF_00056,
						0x00);



	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WaitUs(
				Dev,
				VL53L1_DEF_00057);



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
						Dev,
						VL53L1_DEF_00056,
						0x01);



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00046(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00047(
	VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(
		&(pdev->VL53L1_PRM_00050),
		pcustomer,
		sizeof(VL53L1_customer_nvm_managed_t));



	memcpy(
		&(pdev->VL53L1_PRM_00076),
		pxtalkhisto,
		sizeof(VL53L1_xtalk_histogram_data_t));






	pdev->VL53L1_PRM_00097.VL53L1_PRM_00080 =
			pdev->VL53L1_PRM_00050.VL53L1_PRM_00080;
	pdev->VL53L1_PRM_00097.VL53L1_PRM_00078 =
			pdev->VL53L1_PRM_00050.VL53L1_PRM_00078;
	pdev->VL53L1_PRM_00097.VL53L1_PRM_00079 =
			pdev->VL53L1_PRM_00050.VL53L1_PRM_00079;

	pdev->VL53L1_PRM_00097.VL53L1_PRM_00084 =
			pdev->VL53L1_PRM_00050.VL53L1_PRM_00084;
	pdev->VL53L1_PRM_00097.VL53L1_PRM_00082 =
			pdev->VL53L1_PRM_00050.VL53L1_PRM_00082;
	pdev->VL53L1_PRM_00097.VL53L1_PRM_00083 =
			pdev->VL53L1_PRM_00050.VL53L1_PRM_00083;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00048(
	VL53L1_DEV                      Dev,
	VL53L1_customer_nvm_managed_t  *pcustomer,
	VL53L1_xtalk_histogram_data_t  *pxtalkhisto)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(
		pcustomer,
		&(pdev->VL53L1_PRM_00050),
		sizeof(VL53L1_customer_nvm_managed_t));



	memcpy(
		pxtalkhisto,
		&(pdev->VL53L1_PRM_00076),
		sizeof(VL53L1_xtalk_histogram_data_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00006(
	VL53L1_DEV              Dev,
	uint32_t                VL53L1_PRM_00008)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->VL53L1_PRM_00047.VL53L1_PRM_00113 == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE) {
		pdev->VL53L1_PRM_00008 = VL53L1_PRM_00008;
		pdev->VL53L1_PRM_00009.VL53L1_PRM_00114 = \
			VL53L1_PRM_00008 *
			(uint32_t)pdev->VL53L1_PRM_00047.VL53L1_PRM_00113;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00007(
	VL53L1_DEV              Dev,
	uint32_t               *pinter_measurement_period_ms)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	if (pdev->VL53L1_PRM_00047.VL53L1_PRM_00113 == 0)
		status = VL53L1_ERROR_DIVISION_BY_ZERO;

	if (status == VL53L1_ERROR_NONE)
		*pinter_measurement_period_ms = \
			pdev->VL53L1_PRM_00009.VL53L1_PRM_00114 /
			(uint32_t)pdev->VL53L1_PRM_00047.VL53L1_PRM_00113;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00049(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *puser_zone)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  spad_no = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");



	VL53L1_FCTN_00050(
			puser_zone->VL53L1_PRM_00016,
			puser_zone->VL53L1_PRM_00015,
			&spad_no);










	xy_size = (puser_zone->VL53L1_PRM_00018 << 4) + puser_zone->VL53L1_PRM_00017;




	pdev->VL53L1_PRM_00022.VL53L1_PRM_00115              = spad_no;
	pdev->VL53L1_PRM_00022.VL53L1_PRM_00116 = xy_size;




	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00051(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *puser_zone)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  x       = 0;
	uint8_t  y       = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");



	VL53L1_FCTN_00052(
			pdev->VL53L1_PRM_00022.VL53L1_PRM_00115,
			&y,
			&x);

	puser_zone->VL53L1_PRM_00015 = x;
	puser_zone->VL53L1_PRM_00016 = y;










	xy_size = pdev->VL53L1_PRM_00022.VL53L1_PRM_00116;

	puser_zone->VL53L1_PRM_00018 = xy_size >> 4;
	puser_zone->VL53L1_PRM_00017  = xy_size & 0x0F;

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_FCTN_00053(
	VL53L1_DEV              Dev,
	VL53L1_user_zone_t     *pmm_roi)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  x       = 0;
	uint8_t  y       = 0;
	uint8_t  xy_size = 0;

	LOG_FUNCTION_START("");



	VL53L1_FCTN_00052(
			pdev->VL53L1_PRM_00002.VL53L1_PRM_00117,
			&y,
			&x);

	pmm_roi->VL53L1_PRM_00015 = x;
	pmm_roi->VL53L1_PRM_00016 = y;










	xy_size = pdev->VL53L1_PRM_00002.VL53L1_PRM_00118;

	pmm_roi->VL53L1_PRM_00018 = xy_size >> 4;
	pmm_roi->VL53L1_PRM_00017  = xy_size & 0x0F;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00008(
	VL53L1_DEV                 Dev,
	VL53L1_zone_config_t      *pzone_cfg)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(&(pdev->VL53L1_PRM_00014), pzone_cfg, sizeof(VL53L1_zone_config_t));










	if(pzone_cfg->VL53L1_PRM_00020 < VL53L1_MAX_USER_ZONES)
	    pdev->VL53L1_PRM_00099.VL53L1_PRM_00119 = pzone_cfg->VL53L1_PRM_00020 + 1;
	else
		pdev->VL53L1_PRM_00099.VL53L1_PRM_00119 = VL53L1_MAX_USER_ZONES + 1;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00009(
	VL53L1_DEV                 Dev,
	VL53L1_zone_config_t      *pzone_cfg)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");



	memcpy(pzone_cfg, &(pdev->VL53L1_PRM_00014), sizeof(VL53L1_zone_config_t));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00004(
	VL53L1_DEV                 Dev,
	VL53L1_DevicePresetModes   device_preset_mode,
	uint32_t                   VL53L1_PRM_00006,
	uint32_t                   VL53L1_PRM_00007,
	uint32_t                   VL53L1_PRM_00008)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_static_config_t        *pstatic       = &(pdev->VL53L1_PRM_00104);
	VL53L1_histogram_config_t     *phistogram    = &(pdev->VL53L1_PRM_00120);
	VL53L1_general_config_t       *pgeneral      = &(pdev->VL53L1_PRM_00099);
	VL53L1_timing_config_t        *ptiming       = &(pdev->VL53L1_PRM_00009);
	VL53L1_dynamic_config_t       *pdynamic      = &(pdev->VL53L1_PRM_00022);
	VL53L1_system_control_t       *psystem       = &(pdev->VL53L1_PRM_00121);
	VL53L1_zone_config_t          *pzone_cfg     = &(pdev->VL53L1_PRM_00014);

	LOG_FUNCTION_START("");



	pdev->VL53L1_PRM_00086                 = device_preset_mode;
	pdev->VL53L1_PRM_00006        = VL53L1_PRM_00006;
	pdev->VL53L1_PRM_00007     = VL53L1_PRM_00007;
	pdev->VL53L1_PRM_00008 = VL53L1_PRM_00008;




	VL53L1_FCTN_00038(
			Dev,
			VL53L1_DEF_00058);




	switch (device_preset_mode) {

	case VL53L1_DEF_00003:
		status = VL53L1_FCTN_00054(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00059:
		status = VL53L1_FCTN_00055(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00060:
		status = VL53L1_FCTN_00056(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00039:
		status = VL53L1_FCTN_00057(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00040:
		status = VL53L1_FCTN_00058(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00006:
		status = VL53L1_FCTN_00059(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00004:
		status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00061:
		status = VL53L1_FCTN_00061(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00062:
		status = VL53L1_FCTN_00062(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00005:
		status = VL53L1_FCTN_00063(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00063:
		status = VL53L1_FCTN_00064(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00064:
		status = VL53L1_FCTN_00065(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00065:
		status = VL53L1_FCTN_00066(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00066:
		status = VL53L1_FCTN_00067(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00067:
		status = VL53L1_FCTN_00068(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00068:
		status = VL53L1_FCTN_00069(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00069:
		status = VL53L1_FCTN_00070(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00070:
		status = VL53L1_FCTN_00071(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00071:
		status = VL53L1_FCTN_00072(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00072:
		status = VL53L1_FCTN_00073(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00073:
		status = VL53L1_FCTN_00074(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00074:
		status = VL53L1_FCTN_00075(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00036:
		status = VL53L1_FCTN_00076(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00007:
		status = VL53L1_FCTN_00077(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);
	break;

	case VL53L1_DEF_00075:
		status = VL53L1_FCTN_00078(
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
				VL53L1_PRM_00006,
				VL53L1_PRM_00007,
				pdev->VL53L1_PRM_00010.VL53L1_PRM_00011,
				ptiming);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00006(
				Dev,
				VL53L1_PRM_00008);




	VL53L1_FCTN_00079(
			pdev->VL53L1_PRM_00014.VL53L1_PRM_00020+1,
			&(pres->VL53L1_PRM_00075));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00080(
	VL53L1_DEV                 Dev,
	VL53L1_DeviceZonePreset    VL53L1_PRM_00109)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	VL53L1_general_config_t       *pgeneral      = &(pdev->VL53L1_PRM_00099);
	VL53L1_zone_config_t          *pzone_cfg     = &(pdev->VL53L1_PRM_00014);

	LOG_FUNCTION_START("");



	pdev->VL53L1_PRM_00109        = VL53L1_PRM_00109;




	switch (VL53L1_PRM_00109) {

	case VL53L1_DEF_00076:
		status =
			VL53L1_FCTN_00081(
				pgeneral,
				pzone_cfg);
	break;

	case VL53L1_DEF_00077:
		status =
			VL53L1_FCTN_00082(
				1, 8, 1,

				1, 8, 1,

				15, 15,

				pzone_cfg);
		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;
	break;

	case VL53L1_DEF_00078:
		status =
			VL53L1_FCTN_00082(
			  4, 8, 2,

			  4, 8, 2,

			  7, 7,

			  pzone_cfg);
		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;
	break;

	case VL53L1_DEF_00079:
		status =
			VL53L1_FCTN_00082(
				2, 5, 3,

				2, 5, 3,

				4, 4,

				pzone_cfg);
		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;
	break;

	case VL53L1_DEF_00080:
		status =
			VL53L1_FCTN_00082(
				2, 4, 4,

				2, 4, 4,

				3, 3,

				pzone_cfg);
		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;
	break;

	case VL53L1_DEF_00081:
		status =
			VL53L1_FCTN_00082(
				3, 1, 11,

				3, 1, 11,

				4, 4,

				pzone_cfg);
		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;
	break;

	case VL53L1_DEF_00082:
		status =
			VL53L1_FCTN_00082(
				2, 1, 13,

				2, 1, 13,

				3, 3,

				pzone_cfg);
		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;
	break;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00012(
	VL53L1_DEV                     Dev,
	uint8_t                        VL53L1_PRM_00005,
	VL53L1_DeviceConfigLevel       device_config_level)
{













	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_static_nvm_managed_t   *pstatic_nvm   = &(pdev->VL53L1_PRM_00010);
	VL53L1_customer_nvm_managed_t *pcustomer_nvm = &(pdev->VL53L1_PRM_00050);
	VL53L1_static_config_t        *pstatic       = &(pdev->VL53L1_PRM_00104);
	VL53L1_general_config_t       *pgeneral      = &(pdev->VL53L1_PRM_00099);
	VL53L1_timing_config_t        *ptiming       = &(pdev->VL53L1_PRM_00009);
	VL53L1_dynamic_config_t       *pdynamic      = &(pdev->VL53L1_PRM_00022);
	VL53L1_system_control_t       *psystem       = &(pdev->VL53L1_PRM_00121);

	VL53L1_ll_driver_state_t  *pstate   = &(pdev->VL53L1_PRM_00069);

	uint8_t  *pbuffer                   = &buffer[0];
	uint16_t i                          = 0;
	uint16_t i2c_index                  = 0;
	uint16_t i2c_buffer_offset_bytes    = 0;
	uint16_t i2c_buffer_size_bytes      = 0;

	LOG_FUNCTION_START("");



	pdev->VL53L1_PRM_00005 = VL53L1_PRM_00005;




	psystem->VL53L1_PRM_00122 =
		(psystem->VL53L1_PRM_00122 & VL53L1_DEF_00083) |
		VL53L1_PRM_00005;






	status =
		VL53L1_FCTN_00049(
			Dev,
			&(pdev->VL53L1_PRM_00014.VL53L1_PRM_00021[pdev->VL53L1_PRM_00069.VL53L1_PRM_00123]));






  if(status == VL53L1_ERROR_NONE)
    status = VL53L1_FCTN_00083(Dev);







	switch (device_config_level) {
	case VL53L1_DEF_00008:
		i2c_index = VL53L1_DEF_00084;
	break;
	case VL53L1_DEF_00037:
		i2c_index = VL53L1_DEF_00085;
	break;
	case VL53L1_DEF_00086:
		i2c_index = VL53L1_DEF_00087;
	break;
	case VL53L1_DEF_00088:
		i2c_index = VL53L1_DEF_00089;
	break;
	case VL53L1_DEF_00090:
		i2c_index = VL53L1_DEF_00091;
	break;
	case VL53L1_DEF_00092:
		i2c_index = VL53L1_DEF_00093;
	break;
	default:
		i2c_index = VL53L1_DEF_00094;
	break;
	}




	i2c_buffer_size_bytes = \
			(VL53L1_DEF_00094 +
			 VL53L1_DEF_00095) -
			 i2c_index;




	pbuffer = &buffer[0];
	for (i = 0 ; i < i2c_buffer_size_bytes ; i++)
		*pbuffer++ = 0;




	if (device_config_level >= VL53L1_DEF_00008 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
			VL53L1_DEF_00084 - i2c_index;

		status =
			VL53L1_FCTN_00084(
				pstatic_nvm,
				VL53L1_DEF_00096,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00037 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
			VL53L1_DEF_00085 - i2c_index;

		status =
			VL53L1_FCTN_00085(
				pcustomer_nvm,
				VL53L1_DEF_00097,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00086 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
			VL53L1_DEF_00087 - i2c_index;

		status =
			VL53L1_FCTN_00086(
				pstatic,
				VL53L1_DEF_00098,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00088 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_DEF_00089 - i2c_index;

		status =
			VL53L1_FCTN_00087(
				pgeneral,
				VL53L1_DEF_00099,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00090 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
				VL53L1_DEF_00091 - i2c_index;

		status =
			VL53L1_FCTN_00088(
				ptiming,
				VL53L1_DEF_00100,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (device_config_level >= VL53L1_DEF_00092 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
				VL53L1_DEF_00093 - i2c_index;



	  if ((psystem->VL53L1_PRM_00122 &
	  	VL53L1_DEF_00002) ==
	  	VL53L1_DEF_00002) {
        pdynamic->VL53L1_PRM_00125 = pstate->VL53L1_PRM_00124 | 0x01;
        pdynamic->VL53L1_PRM_00126 = pstate->VL53L1_PRM_00124 | 0x01;
        pdynamic->VL53L1_PRM_00127   = pstate->VL53L1_PRM_00124;
    }
		status =
			VL53L1_FCTN_00089(
				pdynamic,
				VL53L1_DEF_00101,
				&buffer[i2c_buffer_offset_bytes]);
	}

	if (status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = \
				VL53L1_DEF_00094 - i2c_index;

		status =
			VL53L1_FCTN_00090(
				psystem,
				VL53L1_DEF_00095,
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
		status = VL53L1_FCTN_00091(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00092(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00013(
	VL53L1_DEV     Dev)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);




	pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 =
			(pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 & VL53L1_DEF_00083) |
			 VL53L1_DEF_00102;

	status = VL53L1_FCTN_00093(
				Dev,
				&pdev->VL53L1_PRM_00121);



	pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 =
			(pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 & VL53L1_DEF_00083);



	VL53L1_FCTN_00038(
			Dev,
			VL53L1_DEF_00058);



	VL53L1_FCTN_00079(
			pdev->VL53L1_PRM_00014.VL53L1_PRM_00020+1,
			&(pres->VL53L1_PRM_00075));

	return status;
}

VL53L1_Error VL53L1_FCTN_00094(
	VL53L1_DEV                     Dev,
	VL53L1_DeviceResultsLevel      device_results_level)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_system_results_t   *psystem_results = &(pdev->VL53L1_PRM_00027);
	VL53L1_core_results_t     *pcore_results   = &(pdev->VL53L1_PRM_00128);
	VL53L1_debug_results_t    *pdebug_results  = &(pdev->VL53L1_PRM_00047);

	uint16_t i2c_index               = VL53L1_DEF_00103;
	uint16_t i2c_buffer_offset_bytes = 0;
	uint16_t i2c_buffer_size_bytes   = 0;

	LOG_FUNCTION_START("");




	switch (device_results_level) {
	case VL53L1_DEF_00028:
		i2c_buffer_size_bytes =
				(VL53L1_DEF_00104 +
				VL53L1_DEF_00105) -
				i2c_index;
	break;
	case VL53L1_DEF_00106:
		i2c_buffer_size_bytes =
				(VL53L1_DEF_00107 +
				VL53L1_DEF_00108) -
				i2c_index;
	break;
	default:
		i2c_buffer_size_bytes =
				VL53L1_DEF_00109;
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
				VL53L1_DEF_00104 - i2c_index;

		status =
			VL53L1_FCTN_00095(
				VL53L1_DEF_00105,
				&buffer[i2c_buffer_offset_bytes],
				pdebug_results);
	}

	if (device_results_level >= VL53L1_DEF_00106 &&
		status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes =
				VL53L1_DEF_00107 - i2c_index;

		status =
			VL53L1_FCTN_00096(
				VL53L1_DEF_00108,
				&buffer[i2c_buffer_offset_bytes],
				pcore_results);
	}

	if (status == VL53L1_ERROR_NONE) {

		i2c_buffer_offset_bytes = 0;
		status =
			VL53L1_FCTN_00097(
				VL53L1_DEF_00109,
				&buffer[i2c_buffer_offset_bytes],
				psystem_results);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00017(
	VL53L1_DEV                    Dev,
	VL53L1_DeviceResultsLevel     device_results_level,
	VL53L1_range_results_t       *prange_results)
{











	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);
	VL53L1_range_results_t *presults = &(pres->VL53L1_PRM_00059);
	VL53L1_ll_driver_state_t *pstate   = &(pdev->VL53L1_PRM_00069);

	uint8_t   i = 0;

	LOG_FUNCTION_START("");




	if ((pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 &
		 VL53L1_DEF_00110)
		 == VL53L1_DEF_00110) {






		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00099(
							Dev,
							&(pdev->VL53L1_PRM_00073));






		if (status == VL53L1_ERROR_NONE &&
			pdev->VL53L1_PRM_00073.VL53L1_PRM_00129 == 0)
			VL53L1_FCTN_00100(
					 &(pres->VL53L1_PRM_00074.VL53L1_PRM_00035[pdev->VL53L1_PRM_00069.VL53L1_PRM_00070]),
					 &(pdev->VL53L1_PRM_00073));







		if (status == VL53L1_ERROR_NONE) {

			pdev->VL53L1_PRM_00097.VL53L1_PRM_00080 =
					pdev->VL53L1_PRM_00050.VL53L1_PRM_00080;
			pdev->VL53L1_PRM_00097.VL53L1_PRM_00078 =
					pdev->VL53L1_PRM_00050.VL53L1_PRM_00078;
			pdev->VL53L1_PRM_00097.VL53L1_PRM_00079 =
					pdev->VL53L1_PRM_00050.VL53L1_PRM_00079;

			pdev->VL53L1_PRM_00097.VL53L1_PRM_00084 =
					pdev->VL53L1_PRM_00050.VL53L1_PRM_00084;
			pdev->VL53L1_PRM_00097.VL53L1_PRM_00082 =
					pdev->VL53L1_PRM_00050.VL53L1_PRM_00082;
			pdev->VL53L1_PRM_00097.VL53L1_PRM_00083 =
					pdev->VL53L1_PRM_00050.VL53L1_PRM_00083;
		}






		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_ipp_hist_process_data(
					Dev,
					&(pdev->VL53L1_PRM_00097),
					&(pdev->VL53L1_PRM_00073),
					&(pres->VL53L1_PRM_00074.VL53L1_PRM_00035[pdev->VL53L1_PRM_00069.VL53L1_PRM_00070]),
					&(pdev->VL53L1_PRM_00076),
					presults);




		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_FCTN_00101(
					Dev,
					&(pres->VL53L1_PRM_00075.VL53L1_PRM_00035[pdev->VL53L1_PRM_00069.VL53L1_PRM_00070]),
					presults);






		if (status == VL53L1_ERROR_NONE) {

			pres->VL53L1_PRM_00074.VL53L1_PRM_00019    = VL53L1_MAX_USER_ZONES;
			pres->VL53L1_PRM_00074.VL53L1_PRM_00020 = pdev->VL53L1_PRM_00014.VL53L1_PRM_00020+1;
			pdev->VL53L1_PRM_00073.VL53L1_PRM_00036       = pdev->VL53L1_PRM_00069.VL53L1_PRM_00070;

			if (pdev->VL53L1_PRM_00069.VL53L1_PRM_00070 < pres->VL53L1_PRM_00075.VL53L1_PRM_00019)
				memcpy(
					&(pres->VL53L1_PRM_00074.VL53L1_PRM_00035[pdev->VL53L1_PRM_00069.VL53L1_PRM_00070]),
					&(pdev->VL53L1_PRM_00073),
					sizeof(VL53L1_histogram_bin_data_t));
		}







		if (status == VL53L1_ERROR_NONE)
			VL53L1_FCTN_00102(
					&(pdev->VL53L1_PRM_00073),
					presults,
				    &(pdev->VL53L1_PRM_00027),
				    &(pdev->VL53L1_PRM_00128));








    if(status == VL53L1_ERROR_NONE)
      if(pstate->VL53L1_PRM_00038 != VL53L1_DEF_00038)
        status = VL53L1_FCTN_00103(Dev, presults);


	} else {

		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00094(
							Dev,
							device_results_level);

		if (status == VL53L1_ERROR_NONE)
			VL53L1_FCTN_00098(
					&(pdev->VL53L1_PRM_00027),
					&(pdev->VL53L1_PRM_00128),
					presults);
	}



	presults->VL53L1_PRM_00130 = pdev->VL53L1_PRM_00069.VL53L1_PRM_00130;
	presults->VL53L1_PRM_00038  = pdev->VL53L1_PRM_00069.VL53L1_PRM_00038;

	if (status == VL53L1_ERROR_NONE) {



		pres->VL53L1_PRM_00075.VL53L1_PRM_00019    = VL53L1_MAX_USER_ZONES;
		pres->VL53L1_PRM_00075.VL53L1_PRM_00020 = pdev->VL53L1_PRM_00014.VL53L1_PRM_00020+1;

		for (i = 0 ; i < presults->VL53L1_PRM_00037 ; i++)
			presults->VL53L1_PRM_00035[i].VL53L1_PRM_00036 = pdev->VL53L1_PRM_00069.VL53L1_PRM_00070;

		if (pdev->VL53L1_PRM_00069.VL53L1_PRM_00070 < pres->VL53L1_PRM_00075.VL53L1_PRM_00019)
			memcpy(
				&(pres->VL53L1_PRM_00075.VL53L1_PRM_00035[pdev->VL53L1_PRM_00069.VL53L1_PRM_00070]),
				presults,
				sizeof(VL53L1_range_results_t));
	}




	memcpy(
		prange_results,
		presults,
		sizeof(VL53L1_range_results_t));







	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00104(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00014(
	VL53L1_DEV        Dev,
	uint8_t           VL53L1_PRM_00005)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");















	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00012(
					Dev,
					VL53L1_PRM_00005,
					VL53L1_DEF_00088);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00099(
		VL53L1_DEV                   Dev,
		VL53L1_histogram_bin_data_t *pdata)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_zone_private_dyn_cfg_t *pzone_dyn_cfg;

	VL53L1_static_nvm_managed_t   *pstat_nvm = &(pdev->VL53L1_PRM_00010);
	VL53L1_static_config_t        *pstat_cfg = &(pdev->VL53L1_PRM_00104);
	VL53L1_general_config_t       *pgen_cfg  = &(pdev->VL53L1_PRM_00099);
	VL53L1_timing_config_t        *ptim_cfg  = &(pdev->VL53L1_PRM_00009);

	uint8_t    buffer[VL53L1_MAX_I2C_XFER_SIZE];
	uint8_t   *pbuffer = &buffer[0];
	uint8_t    bin_23_0 = 0x00;
	uint16_t   bin                      = 0;
	uint16_t   i2c_buffer_offset_bytes  = 0;
	uint16_t   encoded_timeout          = 0;

	uint32_t   VL53L1_PRM_00131            = 0;
	uint32_t   periods_elapsed_tmp      = 0;

	uint8_t    i                        = 0;

	LOG_FUNCTION_START("");






	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
					Dev,
					VL53L1_DEF_00111,
					pbuffer,
					VL53L1_DEF_00112);






	pdata->VL53L1_PRM_00132               = *(pbuffer +   0);
	pdata->VL53L1_PRM_00105                   = *(pbuffer +   1);
	pdata->VL53L1_PRM_00106                  = *(pbuffer +   2);
	pdata->VL53L1_PRM_00026                   = *(pbuffer +   3);
	pdata->VL53L1_PRM_00133 =
		VL53L1_FCTN_00105(2, pbuffer +   4);






	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00113 - \
			VL53L1_DEF_00111;

	pbuffer = &buffer[i2c_buffer_offset_bytes];

	pdata->VL53L1_PRM_00134 =
			VL53L1_FCTN_00105(2, pbuffer);

	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00114 - \
			VL53L1_DEF_00111;

	pdata->VL53L1_PRM_00135 = buffer[i2c_buffer_offset_bytes];




	pdev->VL53L1_PRM_00047.VL53L1_PRM_00134 =
			pdata->VL53L1_PRM_00134;
	pdev->VL53L1_PRM_00047.VL53L1_PRM_00135 =
			pdata->VL53L1_PRM_00135;







	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00115 - \
			VL53L1_DEF_00111;

	bin_23_0 = buffer[i2c_buffer_offset_bytes] << 2;

	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00116 - \
			VL53L1_DEF_00111;

	bin_23_0 += buffer[i2c_buffer_offset_bytes];

	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00117 - \
			VL53L1_DEF_00111;

	buffer[i2c_buffer_offset_bytes] = bin_23_0;







	i2c_buffer_offset_bytes = \
			VL53L1_DEF_00118 - \
			VL53L1_DEF_00111;

	pbuffer = &buffer[i2c_buffer_offset_bytes];
	for (bin = 0 ; bin < VL53L1_DEF_00035 ; bin++) {
		pdata->VL53L1_PRM_00136[bin] =
				(int32_t)VL53L1_FCTN_00106(3, pbuffer);
		pbuffer += 3;
	}



	pdata->VL53L1_PRM_00036                 = pdev->VL53L1_PRM_00069.VL53L1_PRM_00070;
	pdata->VL53L1_PRM_00137               = 0;
	pdata->VL53L1_PRM_00138             = VL53L1_DEF_00035;
	pdata->VL53L1_PRM_00139          = VL53L1_DEF_00035;

	pdata->VL53L1_PRM_00140 = pgen_cfg->VL53L1_PRM_00140;




	pdata->VL53L1_PRM_00141 =
		((uint16_t)pgen_cfg->VL53L1_PRM_00142) << 4;
	pdata->VL53L1_PRM_00141 +=
		(uint16_t)pstat_cfg->VL53L1_PRM_00143;



	pdata->VL53L1_PRM_00144 =
		pstat_nvm->VL53L1_PRM_00011;




	VL53L1_FCTN_00107(Dev, pdata);






	if (pdev->VL53L1_PRM_00069.VL53L1_PRM_00145 == 0) {

		encoded_timeout = \
			(ptim_cfg->VL53L1_PRM_00146 << 8) \
			+ ptim_cfg->VL53L1_PRM_00147;
		pdata->VL53L1_PRM_00041 =  ptim_cfg->VL53L1_PRM_00101;
	} else {

		encoded_timeout = \
			(ptim_cfg->VL53L1_PRM_00148 << 8) \
			 + ptim_cfg->VL53L1_PRM_00149;
		pdata->VL53L1_PRM_00041 = ptim_cfg->VL53L1_PRM_00150;
	}




	pdata->VL53L1_PRM_00129  = 0;

	for (i = 0; i < 6; i++){
		if (pdata->VL53L1_PRM_00151[i] == 0x7)
			pdata->VL53L1_PRM_00129  =
					pdata->VL53L1_PRM_00129 + 0x4;
	}

	pdata->VL53L1_PRM_00152 =
		VL53L1_FCTN_00108(encoded_timeout);







	VL53L1_PRM_00131 =
		VL53L1_FCTN_00109(pdata->VL53L1_PRM_00144);




	periods_elapsed_tmp = pdata->VL53L1_PRM_00152 + 1;






	pdata->VL53L1_PRM_00153 =
		VL53L1_FCTN_00110(
			VL53L1_PRM_00131,
			(uint32_t)pdata->VL53L1_PRM_00141,
			VL53L1_DEF_00119,
			periods_elapsed_tmp);

	pdata->VL53L1_PRM_00154     = 0;




	VL53L1_FCTN_00111(pdata);






	VL53L1_FCTN_00112(pdata);




	pdata->VL53L1_PRM_00130 = pdev->VL53L1_PRM_00069.VL53L1_PRM_00130;
	pdata->VL53L1_PRM_00038  = pdev->VL53L1_PRM_00069.VL53L1_PRM_00038;




	pzone_dyn_cfg = &(pres->VL53L1_PRM_00155.VL53L1_PRM_00035[pdata->VL53L1_PRM_00036]);

	pdata->VL53L1_PRM_00115 =
			pzone_dyn_cfg->VL53L1_PRM_00115;
	pdata->VL53L1_PRM_00116 =
			pzone_dyn_cfg->VL53L1_PRM_00116;

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00098(
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore,
	VL53L1_range_results_t           *presults)
{
	uint8_t  i = 0;

	VL53L1_range_data_t  *pdata;

	LOG_FUNCTION_START("");




	presults->VL53L1_PRM_00063    = VL53L1_MAX_RANGE_RESULTS;
	presults->VL53L1_PRM_00037 = 1;

	pdata = &(presults->VL53L1_PRM_00035[0]);

	for (i = 0 ; i < 2 ; i++) {

		pdata->VL53L1_PRM_00036      = 0;
		pdata->VL53L1_PRM_00156     = i;
		pdata->VL53L1_PRM_00024   = 0;
		pdata->VL53L1_PRM_00095 = psys->VL53L1_PRM_00026;
		pdata->VL53L1_PRM_00025 =
			psys->VL53L1_PRM_00105 & VL53L1_DEF_00120;

		pdata->VL53L1_PRM_00157 = 0;
		pdata->VL53L1_PRM_00137    = 0;
		pdata->VL53L1_PRM_00158   = 0;
		pdata->VL53L1_PRM_00159     = 0;
		pdata->VL53L1_PRM_00160   = 0;
		pdata->VL53L1_PRM_00161    = 0;

		switch (i) {

		case 0:

			pdata->VL53L1_PRM_00032 =
				psys->VL53L1_PRM_00162;
			pdata->VL53L1_PRM_00030 =
				psys->VL53L1_PRM_00163;
			pdata->VL53L1_PRM_00164 =
				psys->VL53L1_PRM_00165;
			pdata->VL53L1_PRM_00031 =
				psys->VL53L1_PRM_00166;

			pdata->VL53L1_PRM_00033 =
				psys->VL53L1_PRM_00167;
			pdata->VL53L1_PRM_00168 =
				psys->VL53L1_PRM_00169;
			pdata->VL53L1_PRM_00034 =
				(int16_t)psys->VL53L1_PRM_00170;

			pdata->VL53L1_PRM_00171 =
				pcore->VL53L1_PRM_00172;
			pdata->VL53L1_PRM_00072 =
				pcore->VL53L1_PRM_00173;
			pdata->VL53L1_PRM_00152 =
				pcore->VL53L1_PRM_00174;
			pdata->VL53L1_PRM_00175 =
				pcore->VL53L1_PRM_00176;

		break;
		case 1:

			pdata->VL53L1_PRM_00032 =
				psys->VL53L1_PRM_00177;
			pdata->VL53L1_PRM_00030 =
				psys->VL53L1_PRM_00178;
			pdata->VL53L1_PRM_00164 =
				0xFFFF;
			pdata->VL53L1_PRM_00031 =
				psys->VL53L1_PRM_00179;

			pdata->VL53L1_PRM_00033 =
				psys->VL53L1_PRM_00180;
			pdata->VL53L1_PRM_00168 =
				psys->VL53L1_PRM_00181;
			pdata->VL53L1_PRM_00034  =
				(int16_t)psys->VL53L1_PRM_00182;

			pdata->VL53L1_PRM_00171 =
				pcore->VL53L1_PRM_00183;
			pdata->VL53L1_PRM_00072 =
				pcore->VL53L1_PRM_00184;
			pdata->VL53L1_PRM_00152  =
				pcore->VL53L1_PRM_00185;
			pdata->VL53L1_PRM_00175 =
				pcore->VL53L1_PRM_00186;

		break;

		}





		pdata->VL53L1_PRM_00187    = pdata->VL53L1_PRM_00168;
		pdata->VL53L1_PRM_00188    = pdata->VL53L1_PRM_00168;
		pdata->VL53L1_PRM_00029 = pdata->VL53L1_PRM_00034;
		pdata->VL53L1_PRM_00028 = pdata->VL53L1_PRM_00034;

		pdata++;
	}

	LOG_FUNCTION_END(0);
}
