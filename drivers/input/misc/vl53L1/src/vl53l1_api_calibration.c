
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



































#include <linux/slab.h>
#include <linux/gfp.h>
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
#include "vl53l1_api_calibration.h"


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


VL53L1_Error VL53L1_FCTN_00014(
	VL53L1_DEV        Dev)
{
	




	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      comms_buffer[VL53L1_MAX_I2C_XFER_SIZE];

	VL53L1_refspadchar_config_t *prefspadchar  = &(pdev->VL53L1_PRM_00039);

	LOG_FUNCTION_START("");

	




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00015(Dev);

	




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00016(
				Dev,
				prefspadchar->VL53L1_PRM_00040,
				prefspadchar->VL53L1_PRM_00041,
				prefspadchar->VL53L1_PRM_00042,
				prefspadchar->VL53L1_PRM_00043,
				prefspadchar->VL53L1_PRM_00044,
				pdev->VL53L1_PRM_00011.VL53L1_PRM_00012);

	




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00017(
					Dev,
					prefspadchar->VL53L1_PRM_00045);

	




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				VL53L1_DEF_00029,
				comms_buffer,
				2);

	if (status == VL53L1_ERROR_NONE) {
		pdev->VL53L1_PRM_00046.VL53L1_PRM_00047 =
				comms_buffer[0];
		pdev->VL53L1_PRM_00046.VL53L1_PRM_00048 =
				comms_buffer[1];
	}

	




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				VL53L1_DEF_00030,
				comms_buffer,
				2);

	if (status == VL53L1_ERROR_NONE) {
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00050 =
				comms_buffer[0];
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00051 =
				comms_buffer[1];
	}

	








	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				VL53L1_DEF_00031,
				comms_buffer,
				6);

	





	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				VL53L1_DEF_00032,
				comms_buffer,
				6);

	if (status == VL53L1_ERROR_NONE) {
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00052 =
				comms_buffer[0];
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00053 =
				comms_buffer[1];
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00054 =
				comms_buffer[2];
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00055 =
				comms_buffer[3];
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00056 =
				comms_buffer[4];
		pdev->VL53L1_PRM_00049.VL53L1_PRM_00057 =
				comms_buffer[5];
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00018(
	VL53L1_DEV	                        Dev,
	uint32_t                            VL53L1_PRM_00007,
	uint32_t                            VL53L1_PRM_00008,
	VL53L1_hist_post_process_config_t  *phistpostprocess)
{
	









	VL53L1_Error status        = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_zone_results_t      *pzone_results = NULL;
	VL53L1_range_data_t        *prange_data;
	VL53L1_xtalk_range_data_t  *pxtalk_range_data;

	uint8_t i                = 0;
	uint8_t VL53L1_PRM_00036          = 0;
	uint8_t VL53L1_PRM_00006 = VL53L1_DEF_00005;
	

	(void) (phistpostprocess);

    


	








	pzone_results = kmalloc(sizeof(VL53L1_zone_results_t), GFP_KERNEL);
	if(pzone_results == NULL)
		return VL53L1_ERROR_INVALID_COMMAND;
	
	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00004(
				Dev,
				VL53L1_DEF_00003,
				VL53L1_PRM_00007,
				VL53L1_PRM_00008,
				100);

	




	pdev->VL53L1_PRM_00059.VL53L1_PRM_00035    = VL53L1_DEF_00033;
	pdev->VL53L1_PRM_00059.VL53L1_PRM_00037 = pdev->VL53L1_PRM_00015.VL53L1_PRM_00021+1;

	for (i = 0 ; i < pdev->VL53L1_PRM_00059.VL53L1_PRM_00035 ; i++) {
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00037          = 0;
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00031  = 0;
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00060        = 0;
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00033                = 0;
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00061           = 0;
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00062 = 0;
		pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[i].VL53L1_PRM_00063     = 0;
	}

    


	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00009(
				Dev,
				VL53L1_PRM_00006,
				VL53L1_DEF_00034);

	for (i = 0 ; i <= pdev->VL53L1_PRM_00015.VL53L1_PRM_00021+1 ; i++) {

		


		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00019(Dev);

		





		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_FCTN_00013(
					Dev,
					VL53L1_DEF_00028,
					pzone_results);

		




		if (status == VL53L1_ERROR_NONE &&
			pdev->VL53L1_PRM_00065.VL53L1_PRM_00064 != VL53L1_DEF_00035) {

			VL53L1_PRM_00036           = pdev->VL53L1_PRM_00065.VL53L1_PRM_00066;
			prange_data       = &(pzone_results->VL53L1_PRM_00034[VL53L1_PRM_00036].VL53L1_PRM_00034[0]);
			pxtalk_range_data = &(pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[VL53L1_PRM_00036]);

			pxtalk_range_data->VL53L1_PRM_00037 =
				pzone_results->VL53L1_PRM_00034[VL53L1_PRM_00036].VL53L1_PRM_00037;

			if (pxtalk_range_data->VL53L1_PRM_00037 > 0) {

				pxtalk_range_data->VL53L1_PRM_00031 =
					prange_data->VL53L1_PRM_00031;
				pxtalk_range_data->VL53L1_PRM_00060 =
					prange_data->VL53L1_PRM_00060;
				pxtalk_range_data->VL53L1_PRM_00033 =
					prange_data->VL53L1_PRM_00033;

				pxtalk_range_data->VL53L1_PRM_00061++;
				pxtalk_range_data->VL53L1_PRM_00062 +=
					prange_data->VL53L1_PRM_00063;

				pxtalk_range_data->VL53L1_PRM_00063 =
					pxtalk_range_data->VL53L1_PRM_00062 /
					pxtalk_range_data->VL53L1_PRM_00061;

			}
		}

		





		if (status == VL53L1_ERROR_NONE)
			status = VL53L1_FCTN_00020(Dev);

		









		if (status == VL53L1_ERROR_NONE)
			status =
				VL53L1_FCTN_00011(
					Dev,
					VL53L1_PRM_00006);

	}

	


	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00010(Dev);

    




	if (status == VL53L1_ERROR_NONE) {

		if ((pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[4].VL53L1_PRM_00037   >  0) &&
			(pdev->VL53L1_PRM_00059.VL53L1_PRM_00034[4].VL53L1_PRM_00033 < 20)) {

			if (status == VL53L1_ERROR_NONE)
				status = VL53L1_ipp_xtalk_calibration_process_data(
							Dev,
							&(pdev->VL53L1_PRM_00059),
							&(pdev->VL53L1_PRM_00067),
							&(pdev->VL53L1_PRM_00068),
							&(pdev->VL53L1_PRM_00069));

			if (status == VL53L1_ERROR_NONE) {
				pdev->VL53L1_PRM_00049.VL53L1_PRM_00070 =
					pdev->VL53L1_PRM_00069.VL53L1_PRM_00070;
				pdev->VL53L1_PRM_00049.VL53L1_PRM_00071 =
					pdev->VL53L1_PRM_00069.VL53L1_PRM_00071;
				pdev->VL53L1_PRM_00049.VL53L1_PRM_00072 =
					pdev->VL53L1_PRM_00069.VL53L1_PRM_00072;
			}


		} else {

			


			pdev->VL53L1_PRM_00049.VL53L1_PRM_00070 = 0;
			pdev->VL53L1_PRM_00049.VL53L1_PRM_00071 = 0;
			pdev->VL53L1_PRM_00049.VL53L1_PRM_00072     = 0;

		}

	}

	kfree(pzone_results);
	return status;

}


VL53L1_Error VL53L1_FCTN_00016(
	VL53L1_DEV    Dev,
	uint8_t       vcsel_period_a,
	uint32_t      phasecal_timeout_us,
	uint16_t      total_rate_target_mcps,
	uint16_t      max_count_rate_rtn_limit_mcps,
	uint16_t      min_count_rate_rtn_limit_mcps,
	uint16_t      VL53L1_PRM_00073)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t buffer[VL53L1_MAX_I2C_XFER_SIZE];

	uint32_t macro_period_us = 0;
	uint32_t timeout_mclks   = 0;

	LOG_FUNCTION_START("");

	



	macro_period_us =
		VL53L1_FCTN_00021(
			VL53L1_PRM_00073,
			vcsel_period_a);

	




	timeout_mclks = phasecal_timeout_us << 12;
	timeout_mclks = timeout_mclks + (macro_period_us>>1);
	timeout_mclks = timeout_mclks / macro_period_us;

	if (timeout_mclks > 0xFF)
		pdev->VL53L1_PRM_00074.VL53L1_PRM_00075 = 0xFF;
	else
		pdev->VL53L1_PRM_00074.VL53L1_PRM_00075 =
				(uint8_t)timeout_mclks;

	pdev->VL53L1_PRM_00010.VL53L1_PRM_00076 = vcsel_period_a;

	




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrByte(
				Dev,
				VL53L1_DEF_00036,
				pdev->VL53L1_PRM_00074.VL53L1_PRM_00075);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrByte(
				Dev,
				VL53L1_DEF_00037,
				pdev->VL53L1_PRM_00010.VL53L1_PRM_00076);

	





	buffer[0] = pdev->VL53L1_PRM_00010.VL53L1_PRM_00076;
	buffer[1] = pdev->VL53L1_PRM_00010.VL53L1_PRM_00076;

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WriteMulti(
				Dev,
				VL53L1_DEF_00038,
				buffer,
				2); 


	




	pdev->VL53L1_PRM_00049.VL53L1_PRM_00077 =
			total_rate_target_mcps;

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrWord(
				Dev,
				VL53L1_DEF_00039,
				total_rate_target_mcps);  


	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrWord(
				Dev,
				VL53L1_DEF_00040,
				max_count_rate_rtn_limit_mcps);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WrWord(
				Dev,
				VL53L1_DEF_00041,
				min_count_rate_rtn_limit_mcps);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00017(
	VL53L1_DEV    Dev,
	uint8_t       VL53L1_PRM_00045)
{
	




	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      comms_buffer[VL53L1_MAX_I2C_XFER_SIZE];
	uint8_t      VL53L1_PRM_00078 = 0;

	LOG_FUNCTION_START("");

	




	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_RdByte(
				Dev,
				VL53L1_DEF_00042,
				&VL53L1_PRM_00078);

	if (status == VL53L1_ERROR_NONE)
		pdev->VL53L1_PRM_00079.VL53L1_PRM_00078 = VL53L1_PRM_00078;

	



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00022(
					Dev,
					VL53L1_PRM_00045);

	



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00023(Dev);

	



	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_ReadMulti(
				Dev,
				VL53L1_DEF_00043,
				comms_buffer,
				2);

	if (status == VL53L1_ERROR_NONE) {
		pdev->VL53L1_PRM_00028.VL53L1_PRM_00080  = comms_buffer[0];
		pdev->VL53L1_PRM_00028.VL53L1_PRM_00081 = comms_buffer[1];
	}


	if (status == VL53L1_ERROR_NONE)
		trace_print(
			VL53L1_TRACE_LEVEL_INFO,
			"    Device Test Complete:\n\t%-32s = %3u\n\t%-32s = %3u\n",
			"VL53L1_PRM_00080",
			pdev->VL53L1_PRM_00028.VL53L1_PRM_00080,
			"VL53L1_PRM_00081",
			pdev->VL53L1_PRM_00028.VL53L1_PRM_00081);

	



	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00024(Dev);

	






	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_FCTN_00022(
				Dev,
				0x00);

	LOG_FUNCTION_END(status);

	return status;
}
