
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





































#include "vl53l1_ll_def.h"
#include "vl53l1_ll_device.h"
#include "vl53l1_platform.h"
#include "vl53l1_core.h"
#include "vl53l1_silicon_core.h"
#include "vl53l1_wait.h"
#include "vl53l1_register_settings.h"


#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, status, \
		fmt, ##__VA_ARGS__)


VL53L1_Error VL53L1_wait_for_boot_completion(
	VL53L1_DEV     Dev)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      fw_ready  = 0;

	LOG_FUNCTION_START("");

	if (pdev->wait_method == VL53L1_WAIT_METHOD_BLOCKING) {




		status =
			VL53L1_poll_for_boot_completion(
				Dev,
				VL53L1_BOOT_COMPLETION_POLLING_TIMEOUT_MS);

	} else {




		fw_ready = 0;
		while (fw_ready == 0x00 && status == VL53L1_ERROR_NONE) {
			status = VL53L1_is_boot_complete(
				Dev,
				&fw_ready);

			if (status == VL53L1_ERROR_NONE) {
				status = VL53L1_WaitMs(
					Dev,
					VL53L1_POLLING_DELAY_MS);
			}
		}
	}

	LOG_FUNCTION_END(status);

	return status;

}


VL53L1_Error VL53L1_wait_for_firmware_ready(
	VL53L1_DEV     Dev)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      fw_ready  = 0;
	uint8_t      mode_start  = 0;

	LOG_FUNCTION_START("");





	mode_start =
		pdev->sys_ctrl.system__mode_start &
		VL53L1_DEVICEMEASUREMENTMODE_MODE_MASK;







	if ((mode_start == VL53L1_DEVICEMEASUREMENTMODE_TIMED) ||
		(mode_start == VL53L1_DEVICEMEASUREMENTMODE_SINGLESHOT)) {

		if (pdev->wait_method == VL53L1_WAIT_METHOD_BLOCKING) {




			status =
				VL53L1_poll_for_firmware_ready(
					Dev,
					VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS);

		} else {




			fw_ready = 0;
			while (fw_ready == 0x00 && status == VL53L1_ERROR_NONE) {
				status = VL53L1_is_firmware_ready(
					Dev,
					&fw_ready);

				if (status == VL53L1_ERROR_NONE) {
					status = VL53L1_WaitMs(
						Dev,
						VL53L1_POLLING_DELAY_MS);
				}
			}
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_wait_for_range_completion(
	VL53L1_DEV     Dev)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      data_ready  = 0;

	LOG_FUNCTION_START("");

	if (pdev->wait_method == VL53L1_WAIT_METHOD_BLOCKING) {




		status =
			VL53L1_poll_for_range_completion(
				Dev,
				VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS);

	} else {




		data_ready = 0;
		while (data_ready == 0x00 && status == VL53L1_ERROR_NONE) {
			status = VL53L1_is_new_data_ready(
				Dev,
				&data_ready);

			if (status == VL53L1_ERROR_NONE) {
				status = VL53L1_WaitMs(
					Dev,
					VL53L1_POLLING_DELAY_MS);
			}
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_wait_for_test_completion(
	VL53L1_DEV     Dev)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t      data_ready  = 0;

	LOG_FUNCTION_START("");

	if (pdev->wait_method == VL53L1_WAIT_METHOD_BLOCKING) {




		status =
			VL53L1_poll_for_range_completion(
				Dev,
				VL53L1_TEST_COMPLETION_POLLING_TIMEOUT_MS);

	} else {




		data_ready = 0;
		while (data_ready == 0x00 && status == VL53L1_ERROR_NONE) {
			status = VL53L1_is_new_data_ready(
				Dev,
				&data_ready);

			if (status == VL53L1_ERROR_NONE) {
				status = VL53L1_WaitMs(
					Dev,
					VL53L1_POLLING_DELAY_MS);
			}
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}




VL53L1_Error VL53L1_is_boot_complete(
	VL53L1_DEV     Dev,
	uint8_t       *pready)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t  firmware__system_status = 0;

	LOG_FUNCTION_START("");




	status =
		VL53L1_RdByte(
			Dev,
			VL53L1_FIRMWARE__SYSTEM_STATUS,
			&firmware__system_status);






	if ((firmware__system_status & 0x01) == 0x01) {
		*pready = 0x01;
		VL53L1_init_ll_driver_state(
			Dev,
			VL53L1_DEVICESTATE_SW_STANDBY);
	} else {
		*pready = 0x00;
		VL53L1_init_ll_driver_state(
			Dev,
			VL53L1_DEVICESTATE_FW_COLDBOOT);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_is_firmware_ready(
	VL53L1_DEV     Dev,
	uint8_t       *pready)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	status = VL53L1_is_firmware_ready_silicon(
				Dev,
				pready);

	pdev->fw_ready = *pready;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_is_new_data_ready(
	VL53L1_DEV     Dev,
	uint8_t       *pready)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  gpio__mux_active_high_hv = 0;
	uint8_t  gpio__tio_hv_status      = 0;
	uint8_t  interrupt_ready          = 0;

	LOG_FUNCTION_START("");

	gpio__mux_active_high_hv =
			pdev->stat_cfg.gpio_hv_mux__ctrl &
			VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_MASK;

	if (gpio__mux_active_high_hv == VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_HIGH)
		interrupt_ready = 0x01;
	else
		interrupt_ready = 0x00;




	status = VL53L1_RdByte(
					Dev,
					VL53L1_GPIO__TIO_HV_STATUS,
					&gpio__tio_hv_status);




	if ((gpio__tio_hv_status & 0x01) == interrupt_ready)
		*pready = 0x01;
	else
		*pready = 0x00;

	LOG_FUNCTION_END(status);

	return status;
}




VL53L1_Error VL53L1_poll_for_boot_completion(
	VL53L1_DEV    Dev,
	uint32_t      timeout_ms)
{






	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");








	status = VL53L1_WaitUs(
			Dev,
			VL53L1_FIRMWARE_BOOT_TIME_US);

	if (status == VL53L1_ERROR_NONE)
		status =
			VL53L1_WaitValueMaskEx(
				Dev,
				timeout_ms,
				VL53L1_FIRMWARE__SYSTEM_STATUS,
				0x01,
				0x01,
				VL53L1_POLLING_DELAY_MS);

	if (status == VL53L1_ERROR_NONE)
		VL53L1_init_ll_driver_state(Dev, VL53L1_DEVICESTATE_SW_STANDBY);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_poll_for_firmware_ready(
	VL53L1_DEV    Dev,
	uint32_t      timeout_ms)
{






	VL53L1_Error status          = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint32_t     start_time_ms   = 0;
	uint32_t     current_time_ms = 0;
	int32_t      poll_delay_ms   = VL53L1_POLLING_DELAY_MS;
	uint8_t      fw_ready        = 0;




	VL53L1_GetTickCount(&start_time_ms);

	pdev->fw_ready_poll_duration_ms = 0;




	while ((status == VL53L1_ERROR_NONE) &&
		   (pdev->fw_ready_poll_duration_ms < timeout_ms) &&
		   (fw_ready == 0)) {

		status = VL53L1_is_firmware_ready(
			Dev,
			&fw_ready);

		if (status == VL53L1_ERROR_NONE &&
			fw_ready == 0 &&
			poll_delay_ms > 0) {
			status = VL53L1_WaitMs(
				Dev,
				poll_delay_ms);
		}






		VL53L1_GetTickCount(&current_time_ms);

		pdev->fw_ready_poll_duration_ms =
				current_time_ms - start_time_ms;
	}

	if (fw_ready == 0 && status == VL53L1_ERROR_NONE)
		status = VL53L1_ERROR_TIME_OUT;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_poll_for_range_completion(
	VL53L1_DEV     Dev,
	uint32_t       timeout_ms)
{









	VL53L1_Error status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t  gpio__mux_active_high_hv = 0;
	uint8_t  interrupt_ready          = 0;

	LOG_FUNCTION_START("");

	gpio__mux_active_high_hv =
			pdev->stat_cfg.gpio_hv_mux__ctrl &
			VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_MASK;

	if (gpio__mux_active_high_hv == VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_HIGH)
		interrupt_ready = 0x01;
	else
		interrupt_ready = 0x00;

	status =
		VL53L1_WaitValueMaskEx(
			Dev,
			timeout_ms,
			VL53L1_GPIO__TIO_HV_STATUS,
			interrupt_ready,
			0x01,
			VL53L1_POLLING_DELAY_MS);

	LOG_FUNCTION_END(status);

	return status;
}


