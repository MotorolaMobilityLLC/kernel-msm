
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
#include "vl53l1_register_map.h"
#include "vl53l1_register_funcs.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_api_preset_modes.h"
#include "vl53l1_core.h"






#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, \
		status, fmt, ##__VA_ARGS__)

#define trace_print(level, ...) \
	_LOG_TRACE_PRINT(VL53L1_TRACE_MODULE_CORE, \
	level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


void  VL53L1_init_version(
	VL53L1_DEV        Dev)
{





	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->version.ll_major    = VL53L1_LL_API_IMPLEMENTATION_VER_MAJOR;
	pdev->version.ll_minor    = VL53L1_LL_API_IMPLEMENTATION_VER_MINOR;
	pdev->version.ll_build    = VL53L1_LL_API_IMPLEMENTATION_VER_SUB;
	pdev->version.ll_revision = VL53L1_LL_API_IMPLEMENTATION_VER_REVISION;
}


void  VL53L1_init_ll_driver_state(
	VL53L1_DEV         Dev,
	VL53L1_DeviceState device_state)
{





	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->ll_state);

	pstate->cfg_device_state  = device_state;
	pstate->cfg_stream_count  = 0;
	pstate->cfg_gph_id        = VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
	pstate->cfg_timing_status = 0;
	pstate->cfg_zone_id       = 0;

	pstate->rd_device_state   = device_state;
	pstate->rd_stream_count   = 0;
	pstate->rd_gph_id         = VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
	pstate->rd_timing_status  = 0;
	pstate->rd_zone_id        = 0;

}


VL53L1_Error  VL53L1_update_ll_driver_rd_state(
	VL53L1_DEV         Dev)
{










	VL53L1_Error        status  = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->ll_state);




	LOG_FUNCTION_START("");





	if ((pdev->sys_ctrl.system__mode_start &
		VL53L1_DEVICEMEASUREMENTMODE_MODE_MASK) == 0x00) {

		pstate->rd_device_state  = VL53L1_DEVICESTATE_SW_STANDBY;
		pstate->rd_stream_count  = 0;
		pstate->rd_internal_stream_count = 0;
		pstate->rd_internal_stream_count_val = 0;
		pstate->rd_gph_id = VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
		pstate->rd_timing_status = 0;
		pstate->rd_zone_id       = 0;

	} else {






		if (pstate->rd_stream_count == 0xFF) {
			pstate->rd_stream_count = 0x80;
		} else {
			pstate->rd_stream_count++;
		}





		VL53L1_update_internal_stream_counters(Dev,
			pstate->rd_stream_count,
			&(pstate->rd_internal_stream_count),
			&(pstate->rd_internal_stream_count_val));






		pstate->rd_gph_id ^= VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;




		switch (pstate->rd_device_state) {

		case VL53L1_DEVICESTATE_SW_STANDBY:

			if ((pdev->dyn_cfg.system__grouped_parameter_hold &
				VL53L1_GROUPEDPARAMETERHOLD_ID_MASK) > 0) {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC;
			} else {
				if (pstate->rd_zone_id >=
					pdev->zone_cfg.active_zones) {
					pstate->rd_device_state =
						VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA;
				} else {
					pstate->rd_device_state =
						VL53L1_DEVICESTATE_RANGING_GATHER_DATA;
				}
			}

			pstate->rd_stream_count  = 0;
			pstate->rd_internal_stream_count = 0;
			pstate->rd_internal_stream_count_val = 0;
			pstate->rd_timing_status = 0;
			pstate->rd_zone_id       = 0;

			break;

		case VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC:
			pstate->rd_stream_count = 0;
			pstate->rd_internal_stream_count = 0;
			pstate->rd_internal_stream_count_val = 0;
			pstate->rd_zone_id      = 0;
			if (pstate->rd_zone_id >=
				pdev->zone_cfg.active_zones) {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA;
			} else {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_GATHER_DATA;
			}
			break;

		case VL53L1_DEVICESTATE_RANGING_GATHER_DATA:
			pstate->rd_zone_id++;
			if (pstate->rd_zone_id >=
				pdev->zone_cfg.active_zones) {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA;
			} else {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_GATHER_DATA;
			}
			break;

		case VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA:
			pstate->rd_zone_id        = 0;
			pstate->rd_timing_status ^= 0x01;

			if (pstate->rd_zone_id >=
				pdev->zone_cfg.active_zones) {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA;
			} else {
				pstate->rd_device_state =
					VL53L1_DEVICESTATE_RANGING_GATHER_DATA;
			}
			break;

		default:
			pstate->rd_device_state  =
				VL53L1_DEVICESTATE_SW_STANDBY;
			pstate->rd_stream_count  = 0;
			pstate->rd_internal_stream_count = 0;
			pstate->rd_internal_stream_count_val = 0;
			pstate->rd_gph_id = VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
			pstate->rd_timing_status = 0;
			pstate->rd_zone_id       = 0;
			break;
		}
	}





	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_check_ll_driver_rd_state(
	VL53L1_DEV         Dev)
{








	VL53L1_Error         status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_ll_driver_state_t  *pstate       = &(pdev->ll_state);
	VL53L1_system_results_t   *psys_results = &(pdev->sys_results);
	VL53L1_histogram_bin_data_t *phist_data = &(pdev->hist_data);

	uint8_t   device_range_status   = 0;
	uint8_t   device_stream_count   = 0;
	uint8_t   device_gph_id         = 0;
	uint8_t   histogram_mode        = 0;
	uint8_t   expected_stream_count = 0;
	uint8_t   expected_gph_id       = 0;

	LOG_FUNCTION_START("");





	device_range_status =
			psys_results->result__range_status &
			VL53L1_RANGE_STATUS__RANGE_STATUS_MASK;

	device_stream_count = psys_results->result__stream_count;




	histogram_mode =
		(pdev->sys_ctrl.system__mode_start &
		VL53L1_DEVICESCHEDULERMODE_HISTOGRAM) ==
		VL53L1_DEVICESCHEDULERMODE_HISTOGRAM;



	device_gph_id = (psys_results->result__interrupt_status &
		VL53L1_INTERRUPT_STATUS__GPH_ID_INT_STATUS_MASK) >> 4;

	if (histogram_mode) {
		device_gph_id = (phist_data->result__interrupt_status &
			VL53L1_INTERRUPT_STATUS__GPH_ID_INT_STATUS_MASK) >> 4;
	}




	if ((pdev->sys_ctrl.system__mode_start &
		VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK) ==
		VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK) {














		if (pstate->rd_device_state ==
			VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC) {

			if (histogram_mode == 0) {
				if (device_range_status !=
					VL53L1_DEVICEERROR_GPHSTREAMCOUNT0READY) {
					status = VL53L1_ERROR_GPH_SYNC_CHECK_FAIL;
				}
			}
		} else {
			if (pstate->rd_stream_count != device_stream_count) {
				status = VL53L1_ERROR_STREAM_COUNT_CHECK_FAIL;
			}






			if (pstate->rd_gph_id != device_gph_id) {
				status = VL53L1_ERROR_GPH_ID_CHECK_FAIL;







			} else {







			}






			expected_stream_count =
				pres->zone_dyn_cfgs.VL53L1_PRM_00004[pstate->rd_zone_id].expected_stream_count;
			expected_gph_id =
				pres->zone_dyn_cfgs.VL53L1_PRM_00004[pstate->rd_zone_id].expected_gph_id;






			if (expected_stream_count != device_stream_count) {




				if ((pdev->zone_cfg.active_zones == 0) &&
					(device_stream_count == 255)) {








				} else {
					status = VL53L1_ERROR_ZONE_STREAM_COUNT_CHECK_FAIL;







				}
			}






			if (expected_gph_id != device_gph_id) {
				status = VL53L1_ERROR_ZONE_GPH_ID_CHECK_FAIL;







			}
		}











	}


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error  VL53L1_update_ll_driver_cfg_state(
	VL53L1_DEV         Dev)
{





	VL53L1_Error         status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_ll_driver_state_t *pstate = &(pdev->ll_state);

	uint8_t prev_cfg_zone_id;
	uint8_t prev_cfg_gph_id;
	uint8_t prev_cfg_stream_count;

	LOG_FUNCTION_START("");








	if ((pdev->sys_ctrl.system__mode_start &
		VL53L1_DEVICEMEASUREMENTMODE_MODE_MASK) == 0x00) {

		pstate->cfg_device_state  = VL53L1_DEVICESTATE_SW_STANDBY;
		pstate->cfg_stream_count  = 0;
		pstate->cfg_internal_stream_count = 0;
		pstate->cfg_internal_stream_count_val = 0;
		pstate->cfg_gph_id = VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
		pstate->cfg_timing_status = 0;
		pstate->cfg_zone_id       = 0;
		prev_cfg_zone_id          = 0;
		prev_cfg_gph_id           = 0;
		prev_cfg_stream_count     = 0;

	} else {




		prev_cfg_gph_id           = pstate->cfg_gph_id;
		prev_cfg_zone_id          = pstate->cfg_zone_id;
		prev_cfg_stream_count     = pstate->cfg_stream_count;






		if (pstate->cfg_stream_count == 0xFF) {
			pstate->cfg_stream_count = 0x80;
		} else {
			pstate->cfg_stream_count++;
		}





		VL53L1_update_internal_stream_counters(
			Dev,
			pstate->cfg_stream_count,
			&(pstate->cfg_internal_stream_count),
			&(pstate->cfg_internal_stream_count_val));






		pstate->cfg_gph_id ^= VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;






		switch (pstate->cfg_device_state) {

		case VL53L1_DEVICESTATE_SW_STANDBY:
			pstate->cfg_zone_id = 1;
			if (pstate->cfg_zone_id >
				pdev->zone_cfg.active_zones) {
				pstate->cfg_zone_id = 0;
				pstate->cfg_timing_status ^= 0x01;
			}
			pstate->cfg_stream_count = 1;

			if (pdev->gen_cfg.global_config__stream_divider == 0)  {
				pstate->cfg_internal_stream_count = 1;
				pstate->cfg_internal_stream_count_val = 0;
			} else {
				pstate->cfg_internal_stream_count = 0;
				pstate->cfg_internal_stream_count_val = 1;
			}
			pstate->cfg_device_state = VL53L1_DEVICESTATE_RANGING_DSS_AUTO;
			break;

		case VL53L1_DEVICESTATE_RANGING_DSS_AUTO:
			pstate->cfg_zone_id++;
			if (pstate->cfg_zone_id >
				pdev->zone_cfg.active_zones) {

				pstate->cfg_zone_id = 0;
				pstate->cfg_timing_status ^= 0x01;






				if (pdev->zone_cfg.active_zones > 0) {
					pstate->cfg_device_state =
							VL53L1_DEVICESTATE_RANGING_DSS_MANUAL;
				}
			}
			break;

		case VL53L1_DEVICESTATE_RANGING_DSS_MANUAL:
			pstate->cfg_zone_id++;
			if (pstate->cfg_zone_id >
				pdev->zone_cfg.active_zones) {
				pstate->cfg_zone_id = 0;
				pstate->cfg_timing_status ^= 0x01;
			}
			break;

		default:
			pstate->cfg_device_state = VL53L1_DEVICESTATE_SW_STANDBY;
			pstate->cfg_stream_count = 0;
			pstate->cfg_internal_stream_count = 0;
			pstate->cfg_internal_stream_count_val = 0;
			pstate->cfg_gph_id = VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
			pstate->cfg_timing_status = 0;
			pstate->cfg_zone_id       = 0;
			break;
		}
	}






	if (pdev->zone_cfg.active_zones == 0) {





		pres->zone_dyn_cfgs.VL53L1_PRM_00004[prev_cfg_zone_id].expected_stream_count =
			prev_cfg_stream_count - 1;

		pres->zone_dyn_cfgs.VL53L1_PRM_00004[pstate->rd_zone_id].expected_gph_id =
			prev_cfg_gph_id ^ VL53L1_GROUPEDPARAMETERHOLD_ID_MASK;
	} else {
		pres->zone_dyn_cfgs.VL53L1_PRM_00004[prev_cfg_zone_id].expected_stream_count =
			prev_cfg_stream_count;
		pres->zone_dyn_cfgs.VL53L1_PRM_00004[prev_cfg_zone_id].expected_gph_id =
			prev_cfg_gph_id;
	}





	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_copy_rtn_good_spads_to_buffer(
	VL53L1_nvm_copy_data_t  *pdata,
	uint8_t                 *pbuffer)
{





	*(pbuffer +  0) = pdata->global_config__spad_enables_rtn_0;
	*(pbuffer +  1) = pdata->global_config__spad_enables_rtn_1;
	*(pbuffer +  2) = pdata->global_config__spad_enables_rtn_2;
	*(pbuffer +  3) = pdata->global_config__spad_enables_rtn_3;
	*(pbuffer +  4) = pdata->global_config__spad_enables_rtn_4;
	*(pbuffer +  5) = pdata->global_config__spad_enables_rtn_5;
	*(pbuffer +  6) = pdata->global_config__spad_enables_rtn_6;
	*(pbuffer +  7) = pdata->global_config__spad_enables_rtn_7;
	*(pbuffer +  8) = pdata->global_config__spad_enables_rtn_8;
	*(pbuffer +  9) = pdata->global_config__spad_enables_rtn_9;
	*(pbuffer + 10) = pdata->global_config__spad_enables_rtn_10;
	*(pbuffer + 11) = pdata->global_config__spad_enables_rtn_11;
	*(pbuffer + 12) = pdata->global_config__spad_enables_rtn_12;
	*(pbuffer + 13) = pdata->global_config__spad_enables_rtn_13;
	*(pbuffer + 14) = pdata->global_config__spad_enables_rtn_14;
	*(pbuffer + 15) = pdata->global_config__spad_enables_rtn_15;
	*(pbuffer + 16) = pdata->global_config__spad_enables_rtn_16;
	*(pbuffer + 17) = pdata->global_config__spad_enables_rtn_17;
	*(pbuffer + 18) = pdata->global_config__spad_enables_rtn_18;
	*(pbuffer + 19) = pdata->global_config__spad_enables_rtn_19;
	*(pbuffer + 20) = pdata->global_config__spad_enables_rtn_20;
	*(pbuffer + 21) = pdata->global_config__spad_enables_rtn_21;
	*(pbuffer + 22) = pdata->global_config__spad_enables_rtn_22;
	*(pbuffer + 23) = pdata->global_config__spad_enables_rtn_23;
	*(pbuffer + 24) = pdata->global_config__spad_enables_rtn_24;
	*(pbuffer + 25) = pdata->global_config__spad_enables_rtn_25;
	*(pbuffer + 26) = pdata->global_config__spad_enables_rtn_26;
	*(pbuffer + 27) = pdata->global_config__spad_enables_rtn_27;
	*(pbuffer + 28) = pdata->global_config__spad_enables_rtn_28;
	*(pbuffer + 29) = pdata->global_config__spad_enables_rtn_29;
	*(pbuffer + 30) = pdata->global_config__spad_enables_rtn_30;
	*(pbuffer + 31) = pdata->global_config__spad_enables_rtn_31;
}


void VL53L1_init_system_results(
		VL53L1_system_results_t  *pdata)
{






	pdata->result__interrupt_status                       = 0xFF;
	pdata->result__range_status                           = 0xFF;
	pdata->result__report_status                          = 0xFF;
	pdata->result__stream_count                           = 0xFF;

	pdata->result__dss_actual_effective_spads_sd0         = 0xFFFF;
	pdata->result__peak_signal_count_rate_mcps_sd0        = 0xFFFF;
	pdata->result__ambient_count_rate_mcps_sd0            = 0xFFFF;
	pdata->result__sigma_sd0                              = 0xFFFF;
	pdata->result__phase_sd0                              = 0xFFFF;
	pdata->result__final_crosstalk_corrected_range_mm_sd0 = 0xFFFF;
	pdata->result__peak_signal_count_rate_crosstalk_corrected_mcps_sd0 =
			0xFFFF;
	pdata->result__mm_inner_actual_effective_spads_sd0    = 0xFFFF;
	pdata->result__mm_outer_actual_effective_spads_sd0    = 0xFFFF;
	pdata->result__avg_signal_count_rate_mcps_sd0         = 0xFFFF;

	pdata->result__dss_actual_effective_spads_sd1         = 0xFFFF;
	pdata->result__peak_signal_count_rate_mcps_sd1        = 0xFFFF;
	pdata->result__ambient_count_rate_mcps_sd1            = 0xFFFF;
	pdata->result__sigma_sd1                              = 0xFFFF;
	pdata->result__phase_sd1                              = 0xFFFF;
	pdata->result__final_crosstalk_corrected_range_mm_sd1 = 0xFFFF;
	pdata->result__spare_0_sd1                            = 0xFFFF;
	pdata->result__spare_1_sd1                            = 0xFFFF;
	pdata->result__spare_2_sd1                            = 0xFFFF;
	pdata->result__spare_3_sd1                            = 0xFF;

}


void V53L1_init_zone_results_structure(
	uint8_t                 active_zones,
	VL53L1_zone_results_t  *pdata)
{




	uint8_t  z = 0;
	VL53L1_zone_objects_t *pobjects;

	pdata->max_zones    = VL53L1_MAX_USER_ZONES;
	pdata->active_zones = active_zones;

	for (z = 0 ; z < pdata->max_zones ; z++) {
		pobjects = &(pdata->VL53L1_PRM_00004[z]);
		pobjects->cfg_device_state = VL53L1_DEVICESTATE_SW_STANDBY;
		pobjects->rd_device_state  = VL53L1_DEVICESTATE_SW_STANDBY;
		pobjects->max_objects      = VL53L1_MAX_RANGE_RESULTS;
		pobjects->active_objects   = 0;
	}
}


void VL53L1_init_histogram_config_structure(
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
	VL53L1_histogram_config_t  *pdata)
{






	pdata->histogram_config__low_amb_even_bin_0_1  = (even_bin1 << 4) + even_bin0;
	pdata->histogram_config__low_amb_even_bin_2_3  = (even_bin3 << 4) + even_bin2;
	pdata->histogram_config__low_amb_even_bin_4_5  = (even_bin5 << 4) + even_bin4;

	pdata->histogram_config__low_amb_odd_bin_0_1   = (odd_bin1 << 4) + odd_bin0;
	pdata->histogram_config__low_amb_odd_bin_2_3   = (odd_bin3 << 4) + odd_bin2;
	pdata->histogram_config__low_amb_odd_bin_4_5   = (odd_bin5 << 4) + odd_bin4;

	pdata->histogram_config__mid_amb_even_bin_0_1  = pdata->histogram_config__low_amb_even_bin_0_1;
	pdata->histogram_config__mid_amb_even_bin_2_3  = pdata->histogram_config__low_amb_even_bin_2_3;
	pdata->histogram_config__mid_amb_even_bin_4_5  = pdata->histogram_config__low_amb_even_bin_4_5;

	pdata->histogram_config__mid_amb_odd_bin_0_1   = pdata->histogram_config__low_amb_odd_bin_0_1;
	pdata->histogram_config__mid_amb_odd_bin_2     = odd_bin2;
	pdata->histogram_config__mid_amb_odd_bin_3_4   = (odd_bin4 << 4) + odd_bin3;
	pdata->histogram_config__mid_amb_odd_bin_5     = odd_bin5;

	pdata->histogram_config__user_bin_offset       = 0x00;

	pdata->histogram_config__high_amb_even_bin_0_1 = pdata->histogram_config__low_amb_even_bin_0_1;
	pdata->histogram_config__high_amb_even_bin_2_3 = pdata->histogram_config__low_amb_even_bin_2_3;
	pdata->histogram_config__high_amb_even_bin_4_5 = pdata->histogram_config__low_amb_even_bin_4_5;

	pdata->histogram_config__high_amb_odd_bin_0_1  = pdata->histogram_config__low_amb_odd_bin_0_1;
	pdata->histogram_config__high_amb_odd_bin_2_3  = pdata->histogram_config__low_amb_odd_bin_2_3;
	pdata->histogram_config__high_amb_odd_bin_4_5  = pdata->histogram_config__low_amb_odd_bin_4_5;




	pdata->histogram_config__amb_thresh_low        = 0xFFFF;
	pdata->histogram_config__amb_thresh_high       = 0xFFFF;




	pdata->histogram_config__spad_array_selection  = 0x00;

}

void VL53L1_init_histogram_multizone_config_structure(
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
	VL53L1_histogram_config_t  *pdata)
{







	pdata->histogram_config__low_amb_even_bin_0_1  = (even_bin1 << 4) + even_bin0;
	pdata->histogram_config__low_amb_even_bin_2_3  = (even_bin3 << 4) + even_bin2;
	pdata->histogram_config__low_amb_even_bin_4_5  = (even_bin5 << 4) + even_bin4;

	pdata->histogram_config__low_amb_odd_bin_0_1   =
	  pdata->histogram_config__low_amb_even_bin_0_1;
	pdata->histogram_config__low_amb_odd_bin_2_3
	  = pdata->histogram_config__low_amb_even_bin_2_3;
	pdata->histogram_config__low_amb_odd_bin_4_5
	  = pdata->histogram_config__low_amb_even_bin_4_5;

	pdata->histogram_config__mid_amb_even_bin_0_1  =
	  pdata->histogram_config__low_amb_even_bin_0_1;
	pdata->histogram_config__mid_amb_even_bin_2_3
	  = pdata->histogram_config__low_amb_even_bin_2_3;
	pdata->histogram_config__mid_amb_even_bin_4_5
	  = pdata->histogram_config__low_amb_even_bin_4_5;

	pdata->histogram_config__mid_amb_odd_bin_0_1
	  = pdata->histogram_config__low_amb_odd_bin_0_1;
	pdata->histogram_config__mid_amb_odd_bin_2     = odd_bin2;
	pdata->histogram_config__mid_amb_odd_bin_3_4   = (odd_bin4 << 4) + odd_bin3;
	pdata->histogram_config__mid_amb_odd_bin_5     = odd_bin5;

	pdata->histogram_config__user_bin_offset       = 0x00;

	pdata->histogram_config__high_amb_even_bin_0_1 = (odd_bin1 << 4) + odd_bin0;
	pdata->histogram_config__high_amb_even_bin_2_3 = (odd_bin3 << 4) + odd_bin2;
	pdata->histogram_config__high_amb_even_bin_4_5 = (odd_bin5 << 4) + odd_bin4;

	pdata->histogram_config__high_amb_odd_bin_0_1
	  = pdata->histogram_config__high_amb_even_bin_0_1;
	pdata->histogram_config__high_amb_odd_bin_2_3
	  = pdata->histogram_config__high_amb_even_bin_2_3;
	pdata->histogram_config__high_amb_odd_bin_4_5
	  = pdata->histogram_config__high_amb_even_bin_4_5;




	pdata->histogram_config__amb_thresh_low        = 0xFFFF;
	pdata->histogram_config__amb_thresh_high       = 0xFFFF;




	pdata->histogram_config__spad_array_selection  = 0x00;
}


void VL53L1_init_histogram_bin_data_struct(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00015,
	VL53L1_histogram_bin_data_t *pdata)
{






	uint16_t          i = 0;

	pdata->cfg_device_state          = VL53L1_DEVICESTATE_SW_STANDBY;
	pdata->rd_device_state           = VL53L1_DEVICESTATE_SW_STANDBY;

	pdata->zone_id                   = 0;
	pdata->time_stamp                = 0;

	pdata->VL53L1_PRM_00013                 = 0;
	pdata->VL53L1_PRM_00014               = VL53L1_HISTOGRAM_BUFFER_SIZE;
	pdata->VL53L1_PRM_00015            = (uint8_t)VL53L1_PRM_00015;
	pdata->number_of_ambient_bins    = 0;

	pdata->result__interrupt_status           = 0;
	pdata->result__range_status               = 0;
	pdata->result__report_status              = 0;
	pdata->result__stream_count               = 0;

	pdata->result__dss_actual_effective_spads = 0;
	pdata->phasecal_result__reference_phase   = 0;
	pdata->phasecal_result__vcsel_start       = 0;
	pdata->cal_config__vcsel_start            = 0;

	pdata->vcsel_width                        = 0;
	pdata->VL53L1_PRM_00007                       = 0;
	pdata->VL53L1_PRM_00016                = 0;
	pdata->total_periods_elapsed              = 0;

	pdata->min_bin_value                      = 0;
	pdata->max_bin_value                      = 0;

	pdata->zero_distance_phase                = 0;
	pdata->number_of_ambient_samples          = 0;
	pdata->ambient_events_sum                 = 0;
	pdata->VL53L1_PRM_00027             = 0;

	for (i = 0 ; i < VL53L1_MAX_BIN_SEQUENCE_LENGTH ; i++) {
		pdata->bin_seq[i] = (uint8_t)i;
	}
	for (i = 0 ; i < VL53L1_MAX_BIN_SEQUENCE_LENGTH ; i++) {
		pdata->bin_rep[i] = 1;
	}

	for (i = 0 ; i < VL53L1_HISTOGRAM_BUFFER_SIZE ; i++) {
		if (i < VL53L1_PRM_00015) {
			pdata->bin_data[i] = bin_value;
		} else {
			pdata->bin_data[i] = 0;
		}
	}
}


void VL53L1_copy_xtalk_bin_data_to_histogram_data_struct(
		VL53L1_xtalk_histogram_shape_t *pxtalk,
		VL53L1_histogram_bin_data_t    *phist)
{






	phist->cal_config__vcsel_start =
			pxtalk->cal_config__vcsel_start;
	phist->VL53L1_PRM_00016 =
			pxtalk->VL53L1_PRM_00016;
	phist->VL53L1_PRM_00013 =
			pxtalk->VL53L1_PRM_00013;

	phist->phasecal_result__reference_phase   =
			pxtalk->phasecal_result__reference_phase;
	phist->phasecal_result__vcsel_start       =
			pxtalk->phasecal_result__vcsel_start;

	phist->vcsel_width =
			pxtalk->vcsel_width;
	phist->zero_distance_phase =
			pxtalk->zero_distance_phase;

	phist->zone_id      = pxtalk->zone_id;
	phist->VL53L1_PRM_00014  = pxtalk->VL53L1_PRM_00014;
	phist->time_stamp   = pxtalk->time_stamp;
}


void VL53L1_init_xtalk_bin_data_struct(
	uint32_t                        bin_value,
	uint16_t                        VL53L1_PRM_00015,
	VL53L1_xtalk_histogram_shape_t *pdata)
{






	uint16_t          i = 0;

	pdata->zone_id                   = 0;
	pdata->time_stamp                = 0;

	pdata->VL53L1_PRM_00013                 = 0;
	pdata->VL53L1_PRM_00014               = VL53L1_HISTOGRAM_BUFFER_SIZE;
	pdata->VL53L1_PRM_00015            = (uint8_t)VL53L1_PRM_00015;

	pdata->phasecal_result__reference_phase   = 0;
	pdata->phasecal_result__vcsel_start       = 0;
	pdata->cal_config__vcsel_start            = 0;

	pdata->vcsel_width                        = 0;
	pdata->VL53L1_PRM_00016                = 0;

	pdata->zero_distance_phase                = 0;

	for (i = 0 ; i < VL53L1_HISTOGRAM_BUFFER_SIZE ; i++) {
		if (i < VL53L1_PRM_00015) {
			pdata->bin_data[i] = bin_value;
		} else {
			pdata->bin_data[i] = 0;
		}
	}
}


void VL53L1_i2c_encode_uint16_t(
	uint16_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	uint16_t   VL53L1_PRM_00004 = 0;

	VL53L1_PRM_00004 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00004 & 0x00FF);
		VL53L1_PRM_00004 = VL53L1_PRM_00004 >> 8;
	}
}

uint16_t VL53L1_i2c_decode_uint16_t(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   value = 0x00;

	while (count-- > 0) {
		value = (value << 8) | (uint16_t)*pbuffer++;
	}

	return value;
}


void VL53L1_i2c_encode_int16_t(
	int16_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	int16_t    VL53L1_PRM_00004 = 0;

	VL53L1_PRM_00004 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00004 & 0x00FF);
		VL53L1_PRM_00004 = VL53L1_PRM_00004 >> 8;
	}
}

int16_t VL53L1_i2c_decode_int16_t(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	int16_t    value = 0x00;



	if (*pbuffer >= 0x80) {
		value = 0xFFFF;
	}

	while (count-- > 0) {
		value = (value << 8) | (int16_t)*pbuffer++;
	}

	return value;
}

void VL53L1_i2c_encode_uint32_t(
	uint32_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	uint32_t   VL53L1_PRM_00004 = 0;

	VL53L1_PRM_00004 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00004 & 0x00FF);
		VL53L1_PRM_00004 = VL53L1_PRM_00004 >> 8;
	}
}

uint32_t VL53L1_i2c_decode_uint32_t(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint32_t   value = 0x00;

	while (count-- > 0) {
		value = (value << 8) | (uint32_t)*pbuffer++;
	}

	return value;
}


uint32_t VL53L1_i2c_decode_with_mask(
	uint16_t    count,
	uint8_t    *pbuffer,
	uint32_t    bit_mask,
	uint32_t    down_shift,
	uint32_t    offset)
{






	uint32_t   value = 0x00;



	while (count-- > 0) {
		value = (value << 8) | (uint32_t)*pbuffer++;
	}



	value =  value & bit_mask;
	if (down_shift > 0) {
		value = value >> down_shift;
	}



	value = value + offset;

	return value;
}


void VL53L1_i2c_encode_int32_t(
	int32_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	int32_t    VL53L1_PRM_00004 = 0;

	VL53L1_PRM_00004 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00004 & 0x00FF);
		VL53L1_PRM_00004 = VL53L1_PRM_00004 >> 8;
	}
}

int32_t VL53L1_i2c_decode_int32_t(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	int32_t    value = 0x00;



	if (*pbuffer >= 0x80) {
		value = 0xFFFFFFFF;
	}

	while (count-- > 0) {
		value = (value << 8) | (int32_t)*pbuffer++;
	}

	return value;
}


VL53L1_Error VL53L1_start_test(
	VL53L1_DEV    Dev,
	uint8_t       test_mode__ctrl)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_WrByte(
					Dev,
					VL53L1_TEST_MODE__CTRL,
					test_mode__ctrl);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_firmware_enable_register(
	VL53L1_DEV    Dev,
	uint8_t       value)
{





	VL53L1_Error status         = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->sys_ctrl.firmware__enable = value;

	status = VL53L1_WrByte(
				Dev,
				VL53L1_FIRMWARE__ENABLE,
				pdev->sys_ctrl.firmware__enable);

	return status;
}

VL53L1_Error VL53L1_enable_firmware(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_set_firmware_enable_register(Dev, 0x01);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_disable_firmware(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_set_firmware_enable_register(Dev, 0x00);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_set_powerforce_register(
	VL53L1_DEV    Dev,
	uint8_t       value)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->sys_ctrl.power_management__go1_power_force = value;

	status = VL53L1_WrByte(
			Dev,
			VL53L1_POWER_MANAGEMENT__GO1_POWER_FORCE,
			pdev->sys_ctrl.power_management__go1_power_force);

	return status;
}


VL53L1_Error VL53L1_enable_powerforce(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_set_powerforce_register(Dev, 0x01);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_disable_powerforce(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_set_powerforce_register(Dev, 0x00);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_clear_interrupt(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->sys_ctrl.system__interrupt_clear = VL53L1_CLEAR_RANGE_INT;

	status = VL53L1_WrByte(
					Dev,
					VL53L1_SYSTEM__INTERRUPT_CLEAR,
					pdev->sys_ctrl.system__interrupt_clear);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_force_shadow_stream_count_to_zero(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_disable_firmware(Dev);
	}

	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_WrByte(
				Dev,
				VL53L1_SHADOW_RESULT__STREAM_COUNT,
				0x00);
	}

	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_enable_firmware(Dev);
	}

	return status;
}


uint32_t VL53L1_calc_macro_period_us(
	uint16_t  fast_osc_frequency,
	uint8_t   VL53L1_PRM_00007)
{








	uint32_t  pll_period_us        = 0;
	uint8_t   VL53L1_PRM_00028   = 0;
	uint32_t  macro_period_us      = 0;

	LOG_FUNCTION_START("");






	pll_period_us = VL53L1_calc_pll_period_us(fast_osc_frequency);






	VL53L1_PRM_00028 = VL53L1_decode_vcsel_period(VL53L1_PRM_00007);














	macro_period_us =
			(uint32_t)VL53L1_MACRO_PERIOD_VCSEL_PERIODS *
			pll_period_us;
	macro_period_us = macro_period_us >> 6;

	macro_period_us = macro_period_us * (uint32_t)VL53L1_PRM_00028;
	macro_period_us = macro_period_us >> 6;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "pll_period_us",
			pll_period_us);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "VL53L1_PRM_00028",
			VL53L1_PRM_00028);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "macro_period_us",
			macro_period_us);

	LOG_FUNCTION_END(0);

	return macro_period_us;
}


uint32_t VL53L1_calc_pll_period_us(
	uint16_t  fast_osc_frequency)
{














	uint32_t  pll_period_us        = 0;

	LOG_FUNCTION_START("");

	pll_period_us = (0x01 << 30) / fast_osc_frequency;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "pll_period_us",
			pll_period_us);

	LOG_FUNCTION_END(0);

	return pll_period_us;
}


uint16_t VL53L1_calc_range_ignore_threshold(
	uint16_t central_rate,
	int16_t  x_gradient,
	int16_t  y_gradient,
	uint8_t  rate_mult)
{















	int32_t    range_ignore_thresh_int  = 0;
	uint16_t   range_ignore_thresh_kcps = 0;
	int32_t    central_rate_int         = 0;
	int16_t    x_gradient_int           = 0;
	int16_t    y_gradient_int           = 0;

	LOG_FUNCTION_START("");




	central_rate_int = ((int32_t)central_rate * (1 << 7)) / (1000);

	if (x_gradient < 0) {
		x_gradient_int = x_gradient * -1;
	}

	if (y_gradient < 0) {
		y_gradient_int = y_gradient * -1;
	}








	range_ignore_thresh_int = (8 * x_gradient_int * 4) + (8 * y_gradient_int * 4);




	range_ignore_thresh_int = range_ignore_thresh_int / 1000;




	range_ignore_thresh_int = range_ignore_thresh_int + central_rate_int;




	range_ignore_thresh_int = (int32_t)rate_mult * range_ignore_thresh_int;

	range_ignore_thresh_int = (range_ignore_thresh_int + (1<<4)) / (1<<5);




	if (range_ignore_thresh_int > 0xFFFF) {
		range_ignore_thresh_kcps = 0xFFFF;
	} else {
		range_ignore_thresh_kcps = (uint16_t)range_ignore_thresh_int;
	}

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "range_ignore_thresh_kcps",
			range_ignore_thresh_kcps);


	LOG_FUNCTION_END(0);

	return range_ignore_thresh_kcps;
}


uint32_t VL53L1_calc_pll_period_mm(
	uint16_t fast_osc_frequency)
{






	uint32_t pll_period_us = 0;
	uint32_t pll_period_mm = 0;

	LOG_FUNCTION_START("");






	pll_period_us  = VL53L1_calc_pll_period_us(fast_osc_frequency);












	pll_period_mm =
			VL53L1_SPEED_OF_LIGHT_IN_AIR_DIV_8 *
			(pll_period_us >> 2);



	pll_period_mm = (pll_period_mm + (0x01<<15)) >> 16;

	LOG_FUNCTION_END(0);

	return pll_period_mm;
}


uint16_t VL53L1_calc_encoded_timeout(
	uint32_t timeout_us,
	uint32_t macro_period_us)
{










	uint32_t timeout_mclks   = 0;
	uint16_t timeout_encoded = 0;

	LOG_FUNCTION_START("");

	timeout_mclks   =
			((timeout_us << 12) + (macro_period_us>>1)) /
			macro_period_us;
	timeout_encoded = VL53L1_encode_timeout(timeout_mclks);

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_mclks",
			timeout_mclks, timeout_mclks);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_encoded",
			timeout_encoded, timeout_encoded);

	LOG_FUNCTION_END(0);

	return timeout_encoded;
}


uint32_t VL53L1_calc_decoded_timeout_us(
	uint16_t timeout_encoded,
	uint32_t macro_period_us)
{










	uint32_t timeout_mclks  = 0;
	uint32_t timeout_us     = 0;
	uint64_t tmp            = 0;

	LOG_FUNCTION_START("");

	timeout_mclks = VL53L1_decode_timeout(timeout_encoded);

	tmp  = (uint64_t)timeout_mclks * (uint64_t)macro_period_us;
	tmp += 0x00800;
	tmp  = tmp >> 12;

	timeout_us = (uint32_t)tmp;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_mclks",
			timeout_mclks, timeout_mclks);

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u us\n", "timeout_us",
			timeout_us, timeout_us);

	LOG_FUNCTION_END(0);

	return timeout_us;
}


uint16_t VL53L1_encode_timeout(uint32_t timeout_mclks)
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


uint32_t VL53L1_decode_timeout(uint16_t encoded_timeout)
{






	uint32_t timeout_macro_clks = 0;

	timeout_macro_clks = ((uint32_t) (encoded_timeout & 0x00FF)
			<< (uint32_t) ((encoded_timeout & 0xFF00) >> 8)) + 1;

	return timeout_macro_clks;
}


VL53L1_Error VL53L1_calc_timeout_register_values(
	uint32_t                mm_config_timeout_us,
	uint32_t                range_config_timeout_us,
	uint16_t                fast_osc_frequency,
	VL53L1_timing_config_t *ptiming)
{








	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint32_t macro_period_us    = 0;
	uint16_t timeout_encoded    = 0;

	LOG_FUNCTION_START("");

	if (fast_osc_frequency == 0) {
		status = VL53L1_ERROR_DIVISION_BY_ZERO;
	} else {


		macro_period_us =
				VL53L1_calc_macro_period_us(
					fast_osc_frequency,
					ptiming->range_config__vcsel_period_a);



		timeout_encoded =
				VL53L1_calc_encoded_timeout(
					mm_config_timeout_us,
					macro_period_us);

		ptiming->mm_config__timeout_macrop_a_hi =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->mm_config__timeout_macrop_a_lo =
				(uint8_t) (timeout_encoded & 0x00FF);



		timeout_encoded =
				VL53L1_calc_encoded_timeout(
					range_config_timeout_us,
					macro_period_us);

		ptiming->range_config__timeout_macrop_a_hi =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->range_config__timeout_macrop_a_lo =
				(uint8_t) (timeout_encoded & 0x00FF);



		macro_period_us =
				VL53L1_calc_macro_period_us(
					fast_osc_frequency,
					ptiming->range_config__vcsel_period_b);



		timeout_encoded =
				VL53L1_calc_encoded_timeout(
					mm_config_timeout_us,
					macro_period_us);

		ptiming->mm_config__timeout_macrop_b_hi =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->mm_config__timeout_macrop_b_lo =
				(uint8_t) (timeout_encoded & 0x00FF);



		timeout_encoded = VL53L1_calc_encoded_timeout(
							range_config_timeout_us,
							macro_period_us);

		ptiming->range_config__timeout_macrop_b_hi =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->range_config__timeout_macrop_b_lo =
				(uint8_t) (timeout_encoded & 0x00FF);
	}

	LOG_FUNCTION_END(0);

	return status;

}


uint8_t VL53L1_encode_vcsel_period(uint8_t VL53L1_PRM_00028)
{






	uint8_t vcsel_period_reg = 0;

	vcsel_period_reg = (VL53L1_PRM_00028 >> 1) - 1;

	return vcsel_period_reg;
}


uint8_t VL53L1_decode_vcsel_period(uint8_t vcsel_period_reg)
{






	uint8_t VL53L1_PRM_00028 = 0;

	VL53L1_PRM_00028 = (vcsel_period_reg + 1) << 1;

	return VL53L1_PRM_00028;
}


uint32_t VL53L1_decode_unsigned_integer(
	uint8_t  *pbuffer,
	uint8_t   no_of_bytes)
{





	uint8_t   i = 0;
	uint32_t  decoded_value = 0;

	for (i = 0 ; i < no_of_bytes ; i++) {
		decoded_value = (decoded_value << 8) + (uint32_t)pbuffer[i];
	}

	return decoded_value;
}


void VL53L1_encode_unsigned_integer(
	uint32_t  ip_value,
	uint8_t   no_of_bytes,
	uint8_t  *pbuffer)
{





	uint8_t   i    = 0;
	uint32_t  VL53L1_PRM_00004 = 0;

	VL53L1_PRM_00004 = ip_value;
	for (i = 0; i < no_of_bytes ; i++) {
		pbuffer[no_of_bytes-i-1] = VL53L1_PRM_00004 & 0x00FF;
		VL53L1_PRM_00004 = VL53L1_PRM_00004 >> 8;
	}
}


uint32_t  VL53L1_duration_maths(
	uint32_t  pll_period_us,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  elapsed_mclks)
{










	uint64_t  tmp_long_int = 0;
	uint32_t  duration_us  = 0;






	duration_us = window_vclks * pll_period_us;





	duration_us = duration_us >> 12;



	tmp_long_int = (uint64_t)duration_us;






	duration_us = elapsed_mclks * vcsel_parm_pclks;





	duration_us = duration_us >> 4;





	tmp_long_int = tmp_long_int * (uint64_t)duration_us;





	tmp_long_int = tmp_long_int >> 12;



	if (tmp_long_int > 0xFFFFFFFF) {
		tmp_long_int = 0xFFFFFFFF;
	}

	duration_us  = (uint32_t)tmp_long_int;

	return duration_us;
}


uint16_t VL53L1_rate_maths(
	int32_t   VL53L1_PRM_00005,
	uint32_t  time_us)
{












	uint32_t  tmp_int   = 0;
	uint32_t  frac_bits = 7;
	uint16_t  rate_mcps = 0;







	if (VL53L1_PRM_00005 > VL53L1_SPAD_TOTAL_COUNT_MAX) {
		tmp_int = VL53L1_SPAD_TOTAL_COUNT_MAX;
	} else if (VL53L1_PRM_00005 > 0) {
		tmp_int = (uint32_t)VL53L1_PRM_00005;
	}







	if (VL53L1_PRM_00005 > VL53L1_SPAD_TOTAL_COUNT_RES_THRES) {
		frac_bits = 3;
	} else {
		frac_bits = 7;
	}







	if (time_us > 0) {
		tmp_int = ((tmp_int << frac_bits) + (time_us / 2)) / time_us;
	}





	if (VL53L1_PRM_00005 > VL53L1_SPAD_TOTAL_COUNT_RES_THRES) {
		tmp_int = tmp_int << 4;
	}







	if (tmp_int > 0xFFFF) {
		tmp_int = 0xFFFF;
	}

	rate_mcps =  (uint16_t)tmp_int;

	return rate_mcps;
}


int32_t VL53L1_range_maths(
	uint16_t  fast_osc_frequency,
	uint16_t  VL53L1_PRM_00011,
	uint16_t  zero_distance_phase,
	int32_t   range_offset_mm)
{





	uint32_t    pll_period_us = 0;

	int64_t     tmp_long_int  = 0;
	int32_t     range_mm      = 0;




	pll_period_us  = VL53L1_calc_pll_period_us(fast_osc_frequency);










	tmp_long_int = (int64_t)VL53L1_PRM_00011 - (int64_t)zero_distance_phase;










	tmp_long_int =  tmp_long_int * (int64_t)pll_period_us;






	tmp_long_int =  tmp_long_int / (0x01 << 9);











	tmp_long_int =  tmp_long_int * VL53L1_SPEED_OF_LIGHT_IN_AIR_DIV_8;






	tmp_long_int =  tmp_long_int / (0x01 << 22);



	range_mm  = (int32_t)tmp_long_int + range_offset_mm;



	range_mm = range_mm / (0x01 << 2);

	return range_mm;
}


uint16_t VL53L1_rate_per_spad_maths(
	uint32_t  frac_bits,
	uint32_t  peak_count_rate,
	uint16_t  num_spads,
	uint32_t  max_output_value)
{

	uint32_t  tmp_int   = 0;



	uint16_t  rate_per_spad = 0;









	if (num_spads > 0) {
		tmp_int = (peak_count_rate << 8) << frac_bits;
		tmp_int = (tmp_int + ((uint32_t)num_spads / 2)) / (uint32_t)num_spads;
	} else {
		tmp_int = ((peak_count_rate) << frac_bits);
	}




	if (tmp_int > max_output_value) {
		tmp_int = max_output_value;
	}

	rate_per_spad = (uint16_t)tmp_int;

	return rate_per_spad;
}


uint32_t VL53L1_events_per_spad_maths(
	int32_t   VL53L1_PRM_00009,
	uint16_t  num_spads,
	uint32_t  duration)
{
	uint64_t total_hist_counts  = 0;
	uint64_t xtalk_per_spad     = 0;
	uint32_t rate_per_spad_kcps = 0;











#if 0
	total_hist_counts = ((uint64_t)VL53L1_PRM_00009
			* 1000 * 256) / (uint64_t)num_spads;
#else
	{
		uint64_t dividend = ((uint64_t)VL53L1_PRM_00009 * 1000 * 256);

		total_hist_counts = do_division_u(dividend,
			(uint64_t)num_spads);
	}
#endif




	if (duration > 0) {








		uint64_t dividend = (((uint64_t)(total_hist_counts << 11))
			+ ((uint64_t)duration / 2));

		xtalk_per_spad = do_division_u(dividend, (uint64_t)duration);
	} else {
		xtalk_per_spad =   (uint64_t)(total_hist_counts << 11);
	}

	rate_per_spad_kcps = (uint32_t)xtalk_per_spad;

	return rate_per_spad_kcps;
}


void  VL53L1_hist_find_min_max_bin_values(
	VL53L1_histogram_bin_data_t   *pdata)
{





	uint8_t  bin            = 0;

	LOG_FUNCTION_START("");

	for (bin = 0 ; bin < pdata->VL53L1_PRM_00015 ; bin++) {

		if (bin == 0 || pdata->min_bin_value >= pdata->bin_data[bin]) {
			pdata->min_bin_value = pdata->bin_data[bin];
		}

		if (bin == 0 || pdata->max_bin_value <= pdata->bin_data[bin]) {
			pdata->max_bin_value = pdata->bin_data[bin];
		}
	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_hist_remove_ambient_bins(
	VL53L1_histogram_bin_data_t   *pdata)
{







	uint8_t bin = 0;
	uint8_t VL53L1_PRM_00006 = 0;
	uint8_t i = 0;




	if ((pdata->bin_seq[0] & 0x07) == 0x07) {

		i = 0;
		for (VL53L1_PRM_00006 = 0 ; VL53L1_PRM_00006 < VL53L1_MAX_BIN_SEQUENCE_LENGTH ; VL53L1_PRM_00006++) {
			if ((pdata->bin_seq[VL53L1_PRM_00006] & 0x07) != 0x07) {
				pdata->bin_seq[i] = pdata->bin_seq[VL53L1_PRM_00006];
				pdata->bin_rep[i] = pdata->bin_rep[VL53L1_PRM_00006];
				i++;
			}
		}






		for (VL53L1_PRM_00006 = i ; VL53L1_PRM_00006 < VL53L1_MAX_BIN_SEQUENCE_LENGTH ; VL53L1_PRM_00006++) {
			pdata->bin_seq[VL53L1_PRM_00006] = VL53L1_MAX_BIN_SEQUENCE_CODE + 1;
			pdata->bin_rep[VL53L1_PRM_00006] = 0;
		}
	}

	if (pdata->number_of_ambient_bins > 0) {



		for (bin = pdata->number_of_ambient_bins ;
				bin < pdata->VL53L1_PRM_00014 ; bin++) {
			pdata->bin_data[bin-pdata->number_of_ambient_bins] =
				pdata->bin_data[bin];
		}



		pdata->VL53L1_PRM_00015 =
				pdata->VL53L1_PRM_00015 -
				pdata->number_of_ambient_bins;
		pdata->number_of_ambient_bins = 0;
	}
}


void  VL53L1_hist_estimate_ambient_from_ambient_bins(
	VL53L1_histogram_bin_data_t   *pdata)
{






	uint8_t  bin            = 0;

	LOG_FUNCTION_START("");

	if (pdata->number_of_ambient_bins > 0) {

		pdata->number_of_ambient_samples =
			pdata->number_of_ambient_bins;




		pdata->ambient_events_sum = 0;
		for (bin = 0 ; bin < pdata->number_of_ambient_bins ; bin++) {
			pdata->ambient_events_sum += pdata->bin_data[bin];
		}

		pdata->VL53L1_PRM_00027 = pdata->ambient_events_sum;
		pdata->VL53L1_PRM_00027 +=
				((int32_t)pdata->number_of_ambient_bins / 2);
		pdata->VL53L1_PRM_00027 /=
			(int32_t)pdata->number_of_ambient_bins;

	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_hist_estimate_ambient_from_thresholded_bins(
	int32_t                        ambient_threshold_sigma,
	VL53L1_histogram_bin_data_t   *pdata)
{









	uint8_t  bin                      = 0;
	int32_t  VL53L1_PRM_00029 = 0;

	LOG_FUNCTION_START("");






	VL53L1_hist_find_min_max_bin_values(pdata);







	VL53L1_PRM_00029  = (int32_t)VL53L1_isqrt((uint32_t)pdata->min_bin_value);
	VL53L1_PRM_00029 *= ambient_threshold_sigma;
	VL53L1_PRM_00029 += 0x07;
	VL53L1_PRM_00029  = VL53L1_PRM_00029 >> 4;
	VL53L1_PRM_00029 += pdata->min_bin_value;






	pdata->number_of_ambient_samples = 0;
	pdata->ambient_events_sum        = 0;

	for (bin = 0 ; bin < pdata->VL53L1_PRM_00015 ; bin++) {
		if (pdata->bin_data[bin] < VL53L1_PRM_00029) {
			pdata->ambient_events_sum += pdata->bin_data[bin];
			pdata->number_of_ambient_samples++;
		}
	}






	if (pdata->number_of_ambient_samples > 0) {
		pdata->VL53L1_PRM_00027 = pdata->ambient_events_sum;
		pdata->VL53L1_PRM_00027 += ((int32_t)pdata->number_of_ambient_samples/2);
		pdata->VL53L1_PRM_00027 /= (int32_t)pdata->number_of_ambient_samples;
	}

	LOG_FUNCTION_END(0);
}


VL53L1_Error  VL53L1_hist_copy_and_scale_ambient_info(
	VL53L1_zone_hist_info_t       *pidata,
	VL53L1_histogram_bin_data_t   *podata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	int64_t  VL53L1_PRM_00005            = 0;
	int64_t  tmpi              = 0;
	int64_t  tmpo              = 0;

	LOG_FUNCTION_START("");


	if (pidata->result__dss_actual_effective_spads == 0) {
		status = VL53L1_ERROR_DIVISION_BY_ZERO;
	} else {
		if (pidata->number_of_ambient_bins >  0 &&
			podata->number_of_ambient_bins == 0) {










			tmpo    = 1 + (int64_t)podata->total_periods_elapsed;
			tmpo   *= (int64_t)podata->result__dss_actual_effective_spads;

			tmpi    = 1 + (int64_t)pidata->total_periods_elapsed;
			tmpi   *= (int64_t)pidata->result__dss_actual_effective_spads;

			VL53L1_PRM_00005  = tmpo * (int64_t)pidata->ambient_events_sum;
			VL53L1_PRM_00005 += (tmpi/2);




			VL53L1_PRM_00005 = do_division_s(VL53L1_PRM_00005, tmpi);

			podata->ambient_events_sum = (int32_t)VL53L1_PRM_00005;






			podata->VL53L1_PRM_00027 = podata->ambient_events_sum;
			podata->VL53L1_PRM_00027 +=
					((int32_t)pidata->number_of_ambient_bins / 2);
			podata->VL53L1_PRM_00027 /=
				(int32_t)pidata->number_of_ambient_bins;
		}
	}

	LOG_FUNCTION_END(0);

	return status;
}


void  VL53L1_hist_calc_zero_distance_phase(
	VL53L1_histogram_bin_data_t   *pdata)
{






	uint32_t  period        = 0;
	uint32_t  VL53L1_PRM_00011         = 0;

	LOG_FUNCTION_START("");

	period = 2048 *
		(uint32_t)VL53L1_decode_vcsel_period(pdata->VL53L1_PRM_00007);

	VL53L1_PRM_00011  = period;
	VL53L1_PRM_00011 += (uint32_t)pdata->phasecal_result__reference_phase;
	VL53L1_PRM_00011 += (2048 * (uint32_t)pdata->phasecal_result__vcsel_start);
	VL53L1_PRM_00011 -= (2048 * (uint32_t)pdata->cal_config__vcsel_start);

	VL53L1_PRM_00011  = VL53L1_PRM_00011 % period;

	pdata->zero_distance_phase = (uint16_t)VL53L1_PRM_00011;

	LOG_FUNCTION_END(0);
}


void  VL53L1_hist_get_bin_sequence_config(
	VL53L1_DEV                     Dev,
	VL53L1_histogram_bin_data_t   *pdata)
{






	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	int32_t amb_thresh_low   = 0;
	int32_t amb_thresh_high  = 0;

	uint8_t i = 0;

	LOG_FUNCTION_START("");




	amb_thresh_low  = 1024 *
		(int32_t)pdev->hist_cfg.histogram_config__amb_thresh_low;
	amb_thresh_high = 1024 *
		(int32_t)pdev->hist_cfg.histogram_config__amb_thresh_high;










	if ((pdev->ll_state.rd_stream_count & 0x01) == 0) {

		pdata->bin_seq[5] =
			pdev->hist_cfg.histogram_config__mid_amb_even_bin_4_5 >> 4;
		pdata->bin_seq[4] =
			pdev->hist_cfg.histogram_config__mid_amb_even_bin_4_5 & 0x0F;
		pdata->bin_seq[3] =
			pdev->hist_cfg.histogram_config__mid_amb_even_bin_2_3 >> 4;
		pdata->bin_seq[2] =
			pdev->hist_cfg.histogram_config__mid_amb_even_bin_2_3 & 0x0F;
		pdata->bin_seq[1] =
			pdev->hist_cfg.histogram_config__mid_amb_even_bin_0_1 >> 4;
		pdata->bin_seq[0] =
			pdev->hist_cfg.histogram_config__mid_amb_even_bin_0_1 & 0x0F;

		if (pdata->ambient_events_sum > amb_thresh_high) {
			pdata->bin_seq[5] =
				pdev->hist_cfg.histogram_config__high_amb_even_bin_4_5 >> 4;
			pdata->bin_seq[4] =
				pdev->hist_cfg.histogram_config__high_amb_even_bin_4_5 & 0x0F;
			pdata->bin_seq[3] =
				pdev->hist_cfg.histogram_config__high_amb_even_bin_2_3 >> 4;
			pdata->bin_seq[2] =
				pdev->hist_cfg.histogram_config__high_amb_even_bin_2_3 & 0x0F;
			pdata->bin_seq[1] =
				pdev->hist_cfg.histogram_config__high_amb_even_bin_0_1 >> 4;
			pdata->bin_seq[0] =
				pdev->hist_cfg.histogram_config__high_amb_even_bin_0_1 & 0x0F;
		}

		if (pdata->ambient_events_sum < amb_thresh_low) {
			pdata->bin_seq[5] =
				pdev->hist_cfg.histogram_config__low_amb_even_bin_4_5 >> 4;
			pdata->bin_seq[4] =
				pdev->hist_cfg.histogram_config__low_amb_even_bin_4_5 & 0x0F;
			pdata->bin_seq[3] =
				pdev->hist_cfg.histogram_config__low_amb_even_bin_2_3 >> 4;
			pdata->bin_seq[2] =
				pdev->hist_cfg.histogram_config__low_amb_even_bin_2_3 & 0x0F;
			pdata->bin_seq[1] =
				pdev->hist_cfg.histogram_config__low_amb_even_bin_0_1 >> 4;
			pdata->bin_seq[0] =
				pdev->hist_cfg.histogram_config__low_amb_even_bin_0_1 & 0x0F;
		}

	} else {
		pdata->bin_seq[5] =
			pdev->hist_cfg.histogram_config__mid_amb_odd_bin_5 & 0x0F;
		pdata->bin_seq[4] =
			pdev->hist_cfg.histogram_config__mid_amb_odd_bin_3_4 & 0x0F;
		pdata->bin_seq[3] =
			pdev->hist_cfg.histogram_config__mid_amb_odd_bin_3_4 >> 4;
		pdata->bin_seq[2] =
			pdev->hist_cfg.histogram_config__mid_amb_odd_bin_2 & 0x0F;
		pdata->bin_seq[1] =
			pdev->hist_cfg.histogram_config__mid_amb_odd_bin_0_1 >> 4;
		pdata->bin_seq[0] =
			pdev->hist_cfg.histogram_config__mid_amb_odd_bin_0_1 & 0x0F;

		if (pdata->ambient_events_sum > amb_thresh_high) {
			pdata->bin_seq[5] =
				pdev->hist_cfg.histogram_config__high_amb_odd_bin_4_5 >> 4;
			pdata->bin_seq[4] =
				pdev->hist_cfg.histogram_config__high_amb_odd_bin_4_5 & 0x0F;
			pdata->bin_seq[3] =
				pdev->hist_cfg.histogram_config__high_amb_odd_bin_2_3 >> 4;
			pdata->bin_seq[2] =
				pdev->hist_cfg.histogram_config__high_amb_odd_bin_2_3 & 0x0F;
			pdata->bin_seq[1] =
				pdev->hist_cfg.histogram_config__high_amb_odd_bin_0_1 >> 4;
			pdata->bin_seq[0] =
				pdev->hist_cfg.histogram_config__high_amb_odd_bin_0_1 & 0x0F;
		}

		if (pdata->ambient_events_sum < amb_thresh_low) {
			pdata->bin_seq[5] =
				pdev->hist_cfg.histogram_config__low_amb_odd_bin_4_5 >> 4;
			pdata->bin_seq[4] =
				pdev->hist_cfg.histogram_config__low_amb_odd_bin_4_5 & 0x0F;
			pdata->bin_seq[3] =
				pdev->hist_cfg.histogram_config__low_amb_odd_bin_2_3 >> 4;
			pdata->bin_seq[2] =
				pdev->hist_cfg.histogram_config__low_amb_odd_bin_2_3 & 0x0F;
			pdata->bin_seq[1] =
				pdev->hist_cfg.histogram_config__low_amb_odd_bin_0_1 >> 4;
			pdata->bin_seq[0] =
				pdev->hist_cfg.histogram_config__low_amb_odd_bin_0_1 & 0x0F;
		}
	}




	for (i = 0 ; i < VL53L1_MAX_BIN_SEQUENCE_LENGTH ; i++) {
		pdata->bin_rep[i] = 1;
	}

	LOG_FUNCTION_END(0);

}


VL53L1_Error  VL53L1_hist_phase_consistency_check(
	VL53L1_DEV                Dev,
	VL53L1_zone_objects_t    *previous,
	VL53L1_range_results_t   *pcurrent)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t   VL53L1_PRM_00006 = 0;
	uint8_t   p = 0;

	uint16_t  phase_delta     = 0;
	uint16_t  phase_tolerance = 0;

	LOG_FUNCTION_START("");




	phase_tolerance =
		(uint16_t)pdev->histpostprocess.algo__consistency_check__tolerance;
	phase_tolerance = phase_tolerance << 8;




	if (previous->rd_device_state != VL53L1_DEVICESTATE_RANGING_GATHER_DATA &&
		previous->rd_device_state != VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA) {
		return status;
	}




	if (phase_tolerance == 0) {
		return status;
	}

	for (VL53L1_PRM_00006 = 0 ; VL53L1_PRM_00006 < pcurrent->active_results ; VL53L1_PRM_00006++) {

		if (pcurrent->VL53L1_PRM_00004[VL53L1_PRM_00006].range_status ==
				VL53L1_DEVICEERROR_RANGECOMPLETE_NO_WRAP_CHECK) {




			pcurrent->VL53L1_PRM_00004[VL53L1_PRM_00006].range_status =
				VL53L1_DEVICEERROR_PHASECONSISTENCY;







			for (p = 0 ; p < previous->active_objects ; p++) {

				if (pcurrent->VL53L1_PRM_00004[VL53L1_PRM_00006].VL53L1_PRM_00012 >
					previous->VL53L1_PRM_00004[p].VL53L1_PRM_00012) {
					phase_delta =
						pcurrent->VL53L1_PRM_00004[VL53L1_PRM_00006].VL53L1_PRM_00012 -
						previous->VL53L1_PRM_00004[p].VL53L1_PRM_00012;
				} else {
					phase_delta =
						previous->VL53L1_PRM_00004[p].VL53L1_PRM_00012 -
						pcurrent->VL53L1_PRM_00004[VL53L1_PRM_00006].VL53L1_PRM_00012;
				}

				if (phase_delta < phase_tolerance) {
					pcurrent->VL53L1_PRM_00004[VL53L1_PRM_00006].range_status =
						VL53L1_DEVICEERROR_RANGECOMPLETE;
				}
			}
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error  VL53L1_hist_wrap_dmax(
	VL53L1_zone_hist_info_t      *previous,
	VL53L1_histogram_bin_data_t  *pcurrent,
	int16_t                      *pwrap_dmax_mm)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;


	uint8_t   encoded_vcsel_period = 0;
	uint8_t   VL53L1_PRM_00028   = 0;
	uint32_t  pll_period_mm        = 0;
	uint32_t  wrap_dmax_phase      = 0;
	uint32_t  range_mm             = 0;

	LOG_FUNCTION_START("");

	*pwrap_dmax_mm = 0;




	if (previous->rd_device_state != VL53L1_DEVICESTATE_RANGING_GATHER_DATA &&
		previous->rd_device_state != VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA) {
		return status;
	}

	if (pcurrent->VL53L1_PRM_00016 != 0) {





		pll_period_mm =
			VL53L1_calc_pll_period_mm(
				pcurrent->VL53L1_PRM_00016);






		encoded_vcsel_period = pcurrent->VL53L1_PRM_00007;
		if (previous->VL53L1_PRM_00007 < encoded_vcsel_period) {
			encoded_vcsel_period  = previous->VL53L1_PRM_00007;
		}

		VL53L1_PRM_00028 =
			VL53L1_decode_vcsel_period(
				encoded_vcsel_period);








		wrap_dmax_phase = 2048 * (uint32_t)VL53L1_PRM_00028;
		wrap_dmax_phase -= pcurrent->zero_distance_phase;





		range_mm = wrap_dmax_phase * pll_period_mm;
		range_mm = (range_mm + (1<<14)) >> 15;

		*pwrap_dmax_mm = (int16_t)range_mm;
	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_hist_combine_mm1_mm2_offsets(
	int16_t                               mm1_offset_mm,
	int16_t                               mm2_offset_mm,
	uint8_t                               encoded_mm_roi_centre,
	uint8_t                               encoded_mm_roi_size,
	uint8_t                               encoded_zone_centre,
	uint8_t                               encoded_zone_size,
	VL53L1_additional_offset_cal_data_t  *pcal_data,
	uint8_t                              *pgood_spads,
	uint16_t                              aperture_attenuation,
	int16_t                               *prange_offset_mm)
{















	uint16_t max_mm_inner_effective_spads = 0;
	uint16_t max_mm_outer_effective_spads = 0;
	uint16_t mm_inner_effective_spads     = 0;
	uint16_t mm_outer_effective_spads     = 0;

	uint32_t scaled_mm1_peak_rate_mcps    = 0;
	uint32_t scaled_mm2_peak_rate_mcps    = 0;

	int32_t tmp0 = 0;
	int32_t tmp1 = 0;




	VL53L1_calc_mm_effective_spads(
		encoded_mm_roi_centre,
		encoded_mm_roi_size,
		0xC7,

		0xFF,
		pgood_spads,
		aperture_attenuation,
		&max_mm_inner_effective_spads,
		&max_mm_outer_effective_spads);




	VL53L1_calc_mm_effective_spads(
		encoded_mm_roi_centre,
		encoded_mm_roi_size,
		encoded_zone_centre,
		encoded_zone_size,
		pgood_spads,
		aperture_attenuation,
		&mm_inner_effective_spads,
		&mm_outer_effective_spads);




	scaled_mm1_peak_rate_mcps  = (uint32_t)pcal_data->result__mm_inner_peak_signal_count_rtn_mcps;
	scaled_mm1_peak_rate_mcps *= (uint32_t)mm_inner_effective_spads;
	scaled_mm1_peak_rate_mcps /= (uint32_t)max_mm_inner_effective_spads;

	scaled_mm2_peak_rate_mcps  = (uint32_t)pcal_data->result__mm_outer_peak_signal_count_rtn_mcps;
	scaled_mm2_peak_rate_mcps *= (uint32_t)mm_outer_effective_spads;
	scaled_mm2_peak_rate_mcps /= (uint32_t)max_mm_outer_effective_spads;




	tmp0  = ((int32_t)mm1_offset_mm * (int32_t)scaled_mm1_peak_rate_mcps);
	tmp0 += ((int32_t)mm2_offset_mm * (int32_t)scaled_mm2_peak_rate_mcps);

	tmp1 =  (int32_t)scaled_mm1_peak_rate_mcps +
			(int32_t)scaled_mm2_peak_rate_mcps;






	if (tmp1 != 0) {
		tmp0 = (tmp0 * 4) / tmp1;
	}

	*prange_offset_mm = (int16_t)tmp0;

}


void VL53L1_spad_number_to_byte_bit_index(
	uint8_t  spad_number,
	uint8_t *pbyte_index,
	uint8_t *pbit_index,
	uint8_t *pbit_mask)
{










    *pbyte_index  = spad_number >> 3;
    *pbit_index   = spad_number & 0x07;
    *pbit_mask    = 0x01 << *pbit_index;

}


void VL53L1_decode_row_col(
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


void VL53L1_encode_row_col(
	uint8_t  row,
	uint8_t  col,
	uint8_t *pspad_number)
{





	if (row > 7) {
		*pspad_number = 128 + (col << 3) + (15-row);
	} else {
		*pspad_number = ((15-col) << 3) + row;
	}
}


void VL53L1_decode_zone_size(
	uint8_t  encoded_xy_size,
	uint8_t  *pwidth,
	uint8_t  *pheight)
{











	*pheight = encoded_xy_size >> 4;
	*pwidth  = encoded_xy_size & 0x0F;

}


void VL53L1_encode_zone_size(
	uint8_t  width,
	uint8_t  height,
	uint8_t *pencoded_xy_size)
{










	*pencoded_xy_size = (height << 4) + width;

}


void VL53L1_decode_zone_limits(
	uint8_t   encoded_xy_centre,
	uint8_t   encoded_xy_size,
	int16_t  *px_ll,
	int16_t  *py_ll,
	int16_t  *px_ur,
	int16_t  *py_ur)
{









	uint8_t x_centre = 0;
	uint8_t y_centre = 0;
	uint8_t width    = 0;
	uint8_t height   = 0;




	VL53L1_decode_row_col(
		encoded_xy_centre,
		&y_centre,
		&x_centre);

	VL53L1_decode_zone_size(
		encoded_xy_size,
		&width,
		&height);




	*px_ll = (int16_t)x_centre - ((int16_t)width + 1) / 2;
	if (*px_ll < 0)
		*px_ll = 0;

	*px_ur = *px_ll + (int16_t)width;
	if (*px_ur > (VL53L1_SPAD_ARRAY_WIDTH-1))
		*px_ur = VL53L1_SPAD_ARRAY_WIDTH-1;

	*py_ll = (int16_t)y_centre - ((int16_t)height + 1) / 2;
	if (*py_ll < 0)
		*py_ll = 0;

	*py_ur = *py_ll + (int16_t)height;
	if (*py_ur > (VL53L1_SPAD_ARRAY_HEIGHT-1))
		*py_ur = VL53L1_SPAD_ARRAY_HEIGHT-1;
}


uint8_t VL53L1_is_aperture_location(
	uint8_t row,
	uint8_t col)
{





	uint8_t is_aperture = 0;
	uint8_t mod_row     = row % 4;
	uint8_t mod_col     = col % 4;

	if (mod_row == 0 && mod_col == 2)
		is_aperture = 1;

	if (mod_row == 2 && mod_col == 0)
		is_aperture = 1;

	return is_aperture;
}


void VL53L1_calc_mm_effective_spads(
	uint8_t     encoded_mm_roi_centre,
	uint8_t     encoded_mm_roi_size,
	uint8_t     encoded_zone_centre,
	uint8_t     encoded_zone_size,
	uint8_t    *pgood_spads,
	uint16_t    aperture_attenuation,
	uint16_t   *pmm_inner_effective_spads,
	uint16_t   *pmm_outer_effective_spads)
{







	int16_t   x         = 0;
	int16_t   y         = 0;

	int16_t   mm_x_ll   = 0;
	int16_t   mm_y_ll   = 0;
	int16_t   mm_x_ur   = 0;
	int16_t   mm_y_ur   = 0;

	int16_t   zone_x_ll = 0;
	int16_t   zone_y_ll = 0;
	int16_t   zone_x_ur = 0;
	int16_t   zone_y_ur = 0;

	uint8_t   spad_number = 0;
	uint8_t   byte_index  = 0;
	uint8_t   bit_index   = 0;
	uint8_t   bit_mask    = 0;

	uint8_t   is_aperture = 0;
	uint16_t  spad_attenuation = 0;




	VL53L1_decode_zone_limits(
		encoded_mm_roi_centre,
		encoded_mm_roi_size,
		&mm_x_ll,
		&mm_y_ll,
		&mm_x_ur,
		&mm_y_ur);

	VL53L1_decode_zone_limits(
		encoded_zone_centre,
		encoded_zone_size,
		&zone_x_ll,
		&zone_y_ll,
		&zone_x_ur,
		&zone_y_ur);









	*pmm_inner_effective_spads = 0;
	*pmm_outer_effective_spads = 0;

	for (y = zone_y_ll ; y <= zone_y_ur ; y++) {
		for (x = zone_x_ll ; x <= zone_x_ur ; x++) {




			VL53L1_encode_row_col(
				(uint8_t)y,
				(uint8_t)x,
				&spad_number);








			VL53L1_spad_number_to_byte_bit_index(
				spad_number,
				&byte_index,
				&bit_index,
				&bit_mask);




			if ((pgood_spads[byte_index] & bit_mask) > 0) {



				is_aperture = VL53L1_is_aperture_location(
					(uint8_t)y,
					(uint8_t)x);

				if (is_aperture > 0)
					spad_attenuation = aperture_attenuation;
				else
					spad_attenuation = 0x0100;







				if (x >= mm_x_ll && x <= mm_x_ur &&
					y >= mm_y_ll && y <= mm_y_ur)
					*pmm_inner_effective_spads +=
						spad_attenuation;
				else
					*pmm_outer_effective_spads +=
						spad_attenuation;
			}
		}
	}
}


void VL53L1_hist_copy_results_to_sys_and_core(
	VL53L1_histogram_bin_data_t      *pbins,
	VL53L1_range_results_t           *phist,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore)
{





	uint8_t  i = 0;

	VL53L1_range_data_t  *pdata;

	LOG_FUNCTION_START("");




	VL53L1_init_system_results(psys);




	psys->result__interrupt_status = pbins->result__interrupt_status;
	psys->result__range_status     = phist->active_results;
	psys->result__report_status    = pbins->result__report_status;
	psys->result__stream_count     = pbins->result__stream_count;

	pdata = &(phist->VL53L1_PRM_00004[0]);

	for (i = 0 ; i < phist->active_results ; i++) {

		switch (i) {
		case 0:
			psys->result__dss_actual_effective_spads_sd0 = \
					pdata->VL53L1_PRM_00002;
			psys->result__peak_signal_count_rate_mcps_sd0 = \
					pdata->peak_signal_count_rate_mcps;
			psys->result__avg_signal_count_rate_mcps_sd0 = \
					pdata->avg_signal_count_rate_mcps;
			psys->result__ambient_count_rate_mcps_sd0 = \
					pdata->ambient_count_rate_mcps;

			psys->result__sigma_sd0 = pdata->VL53L1_PRM_00003;
			psys->result__phase_sd0 = pdata->VL53L1_PRM_00012;

			psys->result__final_crosstalk_corrected_range_mm_sd0 = \
					(uint16_t)pdata->median_range_mm;

			psys->result__phase_sd1  = pdata->zero_distance_phase;

			pcore->result_core__ranging_total_events_sd0 = \
					pdata->VL53L1_PRM_00022;
			pcore->result_core__signal_total_events_sd0 = \
					pdata->VL53L1_PRM_00009;
			pcore->result_core__total_periods_elapsed_sd0 = \
					pdata->total_periods_elapsed;
			pcore->result_core__ambient_window_events_sd0 = \
					pdata->VL53L1_PRM_00023;

			break;
		case 1:
			psys->result__dss_actual_effective_spads_sd1 = \
				pdata->VL53L1_PRM_00002;
			psys->result__peak_signal_count_rate_mcps_sd1 = \
				pdata->peak_signal_count_rate_mcps;
			psys->result__ambient_count_rate_mcps_sd1 = \
				pdata->ambient_count_rate_mcps;

			psys->result__sigma_sd1 = pdata->VL53L1_PRM_00003;
			psys->result__phase_sd1 = pdata->VL53L1_PRM_00012;

			psys->result__final_crosstalk_corrected_range_mm_sd1 = \
				(uint16_t)pdata->median_range_mm;

			pcore->result_core__ranging_total_events_sd1 = \
				pdata->VL53L1_PRM_00022;
			pcore->result_core__signal_total_events_sd1 = \
				pdata->VL53L1_PRM_00009;
			pcore->result_core__total_periods_elapsed_sd1 = \
				pdata->total_periods_elapsed;
			pcore->result_core__ambient_window_events_sd1 = \
				pdata->VL53L1_PRM_00023;
			break;
		}

		pdata++;
	}

	LOG_FUNCTION_END(0);

}


VL53L1_Error VL53L1_sum_histogram_data (
		VL53L1_histogram_bin_data_t *phist_input,
		VL53L1_histogram_bin_data_t *phist_output)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t i = 0;
	uint8_t smallest_bin_num = 0;

	LOG_FUNCTION_START("");




	if (status == VL53L1_ERROR_NONE) {

		if (phist_output->VL53L1_PRM_00015 >= phist_input->VL53L1_PRM_00015) {
			smallest_bin_num = phist_input->VL53L1_PRM_00015;
		} else {
			smallest_bin_num = phist_output->VL53L1_PRM_00015;
		}
	}








	if (status == VL53L1_ERROR_NONE) {

		for (i = 0 ; i < smallest_bin_num ; i++) {




			phist_output->bin_data[i] += phist_input->bin_data[i];
		}
	}

	if (status == VL53L1_ERROR_NONE) {

		phist_output->VL53L1_PRM_00027 +=
			phist_input->VL53L1_PRM_00027;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_avg_histogram_data(
			uint8_t no_of_samples,
			VL53L1_histogram_bin_data_t *phist_sum,
			VL53L1_histogram_bin_data_t *phist_avg)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t i = 0;

	LOG_FUNCTION_START("");








	if (status == VL53L1_ERROR_NONE) {

		for (i = 0 ; i < phist_sum->VL53L1_PRM_00015 ; i++) {




			if (no_of_samples > 0) {
				phist_avg->bin_data[i] = phist_sum->bin_data[i] /
					(int32_t)no_of_samples;
			} else {
				phist_avg->bin_data[i] = phist_sum->bin_data[i];
			}
		}
	}

	if (status == VL53L1_ERROR_NONE) {

		if (no_of_samples > 0) {
			phist_avg->VL53L1_PRM_00027 =
				phist_sum->VL53L1_PRM_00027 / (int32_t)no_of_samples;
		} else {
			phist_avg->VL53L1_PRM_00027 =
					phist_sum->VL53L1_PRM_00027;
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


uint32_t VL53L1_isqrt(uint32_t num)
{








	uint32_t  res = 0;
	uint32_t  bit = 1 << 30;




	while (bit > num) {
		bit >>= 2;
	}

	while (bit != 0) {
		if (num >= res + bit)  {
			num -= res + bit;
			res = (res >> 1) + bit;
		} else {
			res >>= 1;
		}
		bit >>= 2;
	}

	return res;
}


VL53L1_Error VL53L1_save_cfg_data(
	VL53L1_DEV  Dev)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_zone_private_dyn_cfg_t *pzone_dyn_cfg;
	VL53L1_dynamic_config_t       *pdynamic = &(pdev->dyn_cfg);

	LOG_FUNCTION_START("");

	pzone_dyn_cfg = &(pres->zone_dyn_cfgs.VL53L1_PRM_00004[pdev->ll_state.cfg_zone_id]);

	pzone_dyn_cfg->expected_stream_count =
			pdev->ll_state.cfg_stream_count;

	pzone_dyn_cfg->expected_gph_id =
			pdev->ll_state.cfg_gph_id;

	pzone_dyn_cfg->roi_config__user_roi_centre_spad =
		pdynamic->roi_config__user_roi_centre_spad;

	pzone_dyn_cfg->roi_config__user_roi_requested_global_xy_size =
		pdynamic->roi_config__user_roi_requested_global_xy_size;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_dynamic_zone_update(
	VL53L1_DEV  Dev,
	VL53L1_range_results_t *presults)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->ll_state);
	VL53L1_zone_config_t     *pzone_cfg;

	uint8_t   zone_id = pdev->ll_state.rd_zone_id;
	uint8_t   i;
	uint16_t  max_total_rate_per_spads;
	uint16_t  target_rate = pdev->stat_cfg.dss_config__target_total_rate_mcps;
	uint32_t  temp = 0xFFFF;

	LOG_FUNCTION_START("");

	pzone_cfg = &(pdev->zone_cfg);









	if (pstate->cfg_device_state ==
		VL53L1_DEVICESTATE_RANGING_DSS_MANUAL) {

		trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: active zones: %u\n",
			pzone_cfg->active_zones);

		pres->zone_dyn_cfgs.VL53L1_PRM_00004[zone_id].dss_requested_effective_spad_count = 0;

		trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: peak signal count rate mcps:\
			%u actual effective spads: %u\n",
			presults->VL53L1_PRM_00004[0].peak_signal_count_rate_mcps,
			presults->VL53L1_PRM_00004[0].VL53L1_PRM_00002);

		trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: active results: %u\n",
			presults->active_results);

		max_total_rate_per_spads = presults->VL53L1_PRM_00004[0].total_rate_per_spad_mcps;

		trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: max total rate per spad at start: %u\n",
			max_total_rate_per_spads);

		for (i = 1; i < presults->active_results; i++) {
			trace_print(VL53L1_TRACE_LEVEL_DEBUG,
				"    DYNZONEUPDATE: zone total rate per spad: zone_id: %u,\
				total rate per spad: %u\n",
				i,
				presults->VL53L1_PRM_00004[i].total_rate_per_spad_mcps);

			if (presults->VL53L1_PRM_00004[i].total_rate_per_spad_mcps >
				max_total_rate_per_spads) {
					max_total_rate_per_spads =
						presults->VL53L1_PRM_00004[i].total_rate_per_spad_mcps;
			}
		}

		if (max_total_rate_per_spads == 0) {





			temp = 0xFFFF;
		} else {





			temp = target_rate << 14;
			trace_print(VL53L1_TRACE_LEVEL_DEBUG,
				"    DYNZONEUPDATE: 1: temp: %u\n",
				temp);





			temp = temp / max_total_rate_per_spads;

			trace_print(VL53L1_TRACE_LEVEL_DEBUG,
				"    DYNZONEUPDATE: 2: temp: %u\n",
				temp);





			if (temp > 0xFFFF) {
				temp = 0xFFFF;
			}

			trace_print(VL53L1_TRACE_LEVEL_DEBUG,
				"    DYNZONEUPDATE: 3: temp: %u\n",
				temp);
		}

		pres->zone_dyn_cfgs.VL53L1_PRM_00004[zone_id].dss_requested_effective_spad_count =
				(uint16_t)temp;

		trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: zone_id: %u, target_rate: %u,\
			max_total_rate_per_spads: %u, requested_spads: %u\n",
			zone_id,
			target_rate,
			max_total_rate_per_spads,
			pres->zone_dyn_cfgs.VL53L1_PRM_00004[zone_id].dss_requested_effective_spad_count);
	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_multizone_hist_bins_update(
	VL53L1_DEV  Dev)
{





















	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->ll_state);
	VL53L1_zone_config_t *pzone_cfg = &(pdev->zone_cfg);
	VL53L1_histogram_config_t *phist_cfg = &(pdev->hist_cfg);
	VL53L1_histogram_config_t *pmulti_hist = &(pzone_cfg->multizone_hist_cfg);

	uint8_t   next_range_is_odd_timing = (pstate->cfg_stream_count) % 2;

	LOG_FUNCTION_START("");





	if (pzone_cfg->bin_config[pdev->ll_state.cfg_zone_id] ==
		VL53L1_ZONECONFIG_BINCONFIG__LOWAMB) {
		if (!next_range_is_odd_timing) {
			trace_print (VL53L1_TRACE_LEVEL_DEBUG,
				"    HISTBINCONFIGUPDATE: Setting LOWAMB EVEN timing\n");
			phist_cfg->histogram_config__low_amb_even_bin_0_1  =
				pmulti_hist->histogram_config__low_amb_even_bin_0_1;
			phist_cfg->histogram_config__low_amb_even_bin_2_3  =
				pmulti_hist->histogram_config__low_amb_even_bin_2_3;
			phist_cfg->histogram_config__low_amb_even_bin_4_5  =
				pmulti_hist->histogram_config__low_amb_even_bin_4_5;
		}

		if (next_range_is_odd_timing) {
			trace_print (VL53L1_TRACE_LEVEL_DEBUG,
				"    HISTBINCONFIGUPDATE: Setting LOWAMB ODD timing\n");
			phist_cfg->histogram_config__low_amb_odd_bin_0_1  =
				pmulti_hist->histogram_config__low_amb_even_bin_0_1;
			phist_cfg->histogram_config__low_amb_odd_bin_2_3  =
				pmulti_hist->histogram_config__low_amb_even_bin_2_3;
			phist_cfg->histogram_config__low_amb_odd_bin_4_5  =
				pmulti_hist->histogram_config__low_amb_even_bin_4_5;
		}
	} else if (pzone_cfg->bin_config[pdev->ll_state.cfg_zone_id] ==
		VL53L1_ZONECONFIG_BINCONFIG__MIDAMB) {

		trace_print (VL53L1_TRACE_LEVEL_DEBUG,
			"    HISTBINCONFIGUPDATE: Setting MIDAMB timing\n");
		if (!next_range_is_odd_timing) {
			trace_print(VL53L1_TRACE_LEVEL_DEBUG,
				"    HISTBINCONFIGUPDATE: Setting MIDAMB EVEN timing\n");
			phist_cfg->histogram_config__low_amb_even_bin_0_1  =
				pmulti_hist->histogram_config__mid_amb_even_bin_0_1;
			phist_cfg->histogram_config__low_amb_even_bin_2_3  =
				pmulti_hist->histogram_config__mid_amb_even_bin_2_3;
			phist_cfg->histogram_config__low_amb_even_bin_4_5  =
				pmulti_hist->histogram_config__mid_amb_even_bin_4_5;
		}

		if (next_range_is_odd_timing) {
			trace_print (VL53L1_TRACE_LEVEL_DEBUG,
				"    HISTBINCONFIGUPDATE: Setting MIDAMB ODD timing\n");
			phist_cfg->histogram_config__low_amb_odd_bin_0_1  =
				pmulti_hist->histogram_config__mid_amb_even_bin_0_1;
			phist_cfg->histogram_config__low_amb_odd_bin_2_3  =
				pmulti_hist->histogram_config__mid_amb_even_bin_2_3;
			phist_cfg->histogram_config__low_amb_odd_bin_4_5  =
				pmulti_hist->histogram_config__mid_amb_even_bin_4_5;
		}
	} else if (pzone_cfg->bin_config[pdev->ll_state.cfg_zone_id] ==
			VL53L1_ZONECONFIG_BINCONFIG__HIGHAMB) {
		if (!next_range_is_odd_timing) {
			trace_print (VL53L1_TRACE_LEVEL_DEBUG,
				"    HISTBINCONFIGUPDATE: Setting HIGHAMB EVEN timing\n");
			phist_cfg->histogram_config__low_amb_even_bin_0_1  =
				pmulti_hist->histogram_config__high_amb_even_bin_0_1;
			phist_cfg->histogram_config__low_amb_even_bin_2_3  =
				pmulti_hist->histogram_config__high_amb_even_bin_2_3;
			phist_cfg->histogram_config__low_amb_even_bin_4_5  =
				pmulti_hist->histogram_config__high_amb_even_bin_4_5;
		}

		if (next_range_is_odd_timing) {
			trace_print (VL53L1_TRACE_LEVEL_DEBUG,
				"    HISTBINCONFIGUPDATE: Setting HIGHAMB ODD timing\n");
			phist_cfg->histogram_config__low_amb_odd_bin_0_1  =
				pmulti_hist->histogram_config__high_amb_even_bin_0_1;
			phist_cfg->histogram_config__low_amb_odd_bin_2_3  =
				pmulti_hist->histogram_config__high_amb_even_bin_2_3;
			phist_cfg->histogram_config__low_amb_odd_bin_4_5  =
				pmulti_hist->histogram_config__high_amb_even_bin_4_5;
		}
	}






	if (status == VL53L1_ERROR_NONE) {

		VL53L1_copy_hist_bins_to_static_cfg(
			phist_cfg,
			&(pdev->stat_cfg),
			&(pdev->tim_cfg));
	}

	LOG_FUNCTION_END(status);

	return status;
}
















VL53L1_Error VL53L1_update_internal_stream_counters(
	VL53L1_DEV  Dev,
	uint8_t     external_stream_count,
	uint8_t    *pinternal_stream_count,
	uint8_t    *pinternal_stream_count_val)
{

	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t stream_divider;

	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	stream_divider = pdev->gen_cfg.global_config__stream_divider;

	if (stream_divider == 0) {






		*pinternal_stream_count = external_stream_count;

	} else if (*pinternal_stream_count_val == (stream_divider-1)) {





		if (*pinternal_stream_count == 0xFF) {
			*pinternal_stream_count = 0x80;
		} else {
			*pinternal_stream_count = *pinternal_stream_count + 1;
		}





		*pinternal_stream_count_val = 0;

	} else {





		*pinternal_stream_count_val = *pinternal_stream_count_val + 1;
	}

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
		"UPDINTSTREAMCOUNT   internal_steam_count:  %d,\
		internal_stream_count_val: %d, divider: %d\n",
		*pinternal_stream_count,
		*pinternal_stream_count_val,
		stream_divider);

	LOG_FUNCTION_END(status);

	return status;
}







VL53L1_Error VL53L1_set_histogram_multizone_initial_bin_config(
	VL53L1_zone_config_t		*pzone_cfg,
	VL53L1_histogram_config_t	*phist_cfg,
	VL53L1_histogram_config_t	*pmulti_hist
    )
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");





	if (pzone_cfg->bin_config[0] ==
			VL53L1_ZONECONFIG_BINCONFIG__LOWAMB) {
		phist_cfg->histogram_config__low_amb_even_bin_0_1  =
			pmulti_hist->histogram_config__low_amb_even_bin_0_1;
		phist_cfg->histogram_config__low_amb_even_bin_2_3  =
			pmulti_hist->histogram_config__low_amb_even_bin_2_3;
		phist_cfg->histogram_config__low_amb_even_bin_4_5  =
			pmulti_hist->histogram_config__low_amb_even_bin_4_5;

		phist_cfg->histogram_config__low_amb_odd_bin_0_1  =
			pmulti_hist->histogram_config__low_amb_even_bin_0_1;
		phist_cfg->histogram_config__low_amb_odd_bin_2_3  =
			pmulti_hist->histogram_config__low_amb_even_bin_2_3;
		phist_cfg->histogram_config__low_amb_odd_bin_4_5  =
			pmulti_hist->histogram_config__low_amb_even_bin_4_5;
	} else if (pzone_cfg->bin_config[0] ==
			VL53L1_ZONECONFIG_BINCONFIG__MIDAMB) {

		phist_cfg->histogram_config__low_amb_even_bin_0_1  =
			pmulti_hist->histogram_config__mid_amb_even_bin_0_1;
		phist_cfg->histogram_config__low_amb_even_bin_2_3  =
			pmulti_hist->histogram_config__mid_amb_even_bin_2_3;
		phist_cfg->histogram_config__low_amb_even_bin_4_5  =
			pmulti_hist->histogram_config__mid_amb_even_bin_4_5;

		phist_cfg->histogram_config__low_amb_odd_bin_0_1  =
			pmulti_hist->histogram_config__mid_amb_even_bin_0_1;
		phist_cfg->histogram_config__low_amb_odd_bin_2_3  =
			pmulti_hist->histogram_config__mid_amb_even_bin_2_3;
		phist_cfg->histogram_config__low_amb_odd_bin_4_5  =
			pmulti_hist->histogram_config__mid_amb_even_bin_4_5;
	} else if (pzone_cfg->bin_config[0] ==
			VL53L1_ZONECONFIG_BINCONFIG__HIGHAMB) {
		phist_cfg->histogram_config__low_amb_even_bin_0_1  =
			pmulti_hist->histogram_config__high_amb_even_bin_0_1;
		phist_cfg->histogram_config__low_amb_even_bin_2_3  =
			pmulti_hist->histogram_config__high_amb_even_bin_2_3;
		phist_cfg->histogram_config__low_amb_even_bin_4_5  =
			pmulti_hist->histogram_config__high_amb_even_bin_4_5;
		phist_cfg->histogram_config__low_amb_odd_bin_0_1  =
			pmulti_hist->histogram_config__high_amb_even_bin_0_1;
		phist_cfg->histogram_config__low_amb_odd_bin_2_3  =
			pmulti_hist->histogram_config__high_amb_even_bin_2_3;
		phist_cfg->histogram_config__low_amb_odd_bin_4_5  =
			pmulti_hist->histogram_config__high_amb_even_bin_4_5;
	}

	LOG_FUNCTION_END(status);
	return status;
}






uint8_t	VL53L1_encode_GPIO_interrupt_config(
	VL53L1_GPIO_interrupt_config_t	*pintconf)
{
	uint8_t system__interrupt_config;

	system__interrupt_config = pintconf->intr_mode_distance;
	system__interrupt_config |= ((pintconf->intr_mode_rate) << 2);
	system__interrupt_config |= ((pintconf->intr_new_measure_ready) << 5);
	system__interrupt_config |= ((pintconf->intr_no_target) << 6);
	system__interrupt_config |= ((pintconf->intr_combined_mode) << 7);

	return system__interrupt_config;
}






VL53L1_GPIO_interrupt_config_t VL53L1_decode_GPIO_interrupt_config(
	uint8_t		system__interrupt_config)
{
	VL53L1_GPIO_interrupt_config_t	intconf;

	intconf.intr_mode_distance = system__interrupt_config & 0x03;
	intconf.intr_mode_rate = (system__interrupt_config >> 2) & 0x03;
	intconf.intr_new_measure_ready = (system__interrupt_config >> 5) & 0x01;
	intconf.intr_no_target = (system__interrupt_config >> 6) & 0x01;
	intconf.intr_combined_mode = (system__interrupt_config >> 7) & 0x01;

	return intconf;
}






VL53L1_Error VL53L1_set_GPIO_distance_threshold(
	VL53L1_DEV                      Dev,
	uint16_t			threshold_high,
	uint16_t			threshold_low)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->dyn_cfg.system__thresh_high = threshold_high;
	pdev->dyn_cfg.system__thresh_low = threshold_low;

	LOG_FUNCTION_END(status);
	return status;
}






VL53L1_Error VL53L1_set_GPIO_rate_threshold(
	VL53L1_DEV                      Dev,
	uint16_t			threshold_high,
	uint16_t			threshold_low)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->gen_cfg.system__thresh_rate_high = threshold_high;
	pdev->gen_cfg.system__thresh_rate_low = threshold_low;

	LOG_FUNCTION_END(status);
	return status;
}






VL53L1_Error VL53L1_set_GPIO_thresholds_from_struct(
	VL53L1_DEV                      Dev,
	VL53L1_GPIO_interrupt_config_t *pintconf)
{
	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_set_GPIO_distance_threshold(
			Dev,
			pintconf->threshold_distance_high,
			pintconf->threshold_distance_low);

	if (status == VL53L1_ERROR_NONE)	{
		status = VL53L1_set_GPIO_rate_threshold (
			Dev,
			pintconf->threshold_rate_high,
			pintconf->threshold_rate_low);
	}

	LOG_FUNCTION_END(status);
	return status;
}
