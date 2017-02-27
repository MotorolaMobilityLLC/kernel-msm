
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either 'STMicroelectronics
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































































#include "vl53l1_api.h"
#include "vl53l1_api_strings.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_core.h"
#include "vl53l1_api_calibration.h"
#include "vl53l1_wait.h"

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_API, status, \
			fmt, ##__VA_ARGS__)

#ifdef VL53L1_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(\
		VL53L1_TRACE_MODULE_API, level, VL53L1_TRACE_FUNCTION_NONE, \
		##__VA_ARGS__)
#endif

#ifndef MIN
#define MIN(VL53L1_PRM_00002,VL53L1_PRM_00030) ((VL53L1_PRM_00002) < (VL53L1_PRM_00030) ? (VL53L1_PRM_00002) : (VL53L1_PRM_00030))
#endif
#ifndef MAX
#define MAX(VL53L1_PRM_00002,VL53L1_PRM_00030) ((VL53L1_PRM_00002) < (VL53L1_PRM_00030) ? (VL53L1_PRM_00030) : (VL53L1_PRM_00002))
#endif

#define DMAX_REFLECTANCE_IDX 2















static VL53L1_Error CheckValidRectRoi(VL53L1_UserRoi_t ROI)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");



	if ((ROI.TopLeftX > 15) || (ROI.TopLeftY > 15) ||
		(ROI.BotRightX > 15) || (ROI.BotRightY > 15))
		Status = VL53L1_ERROR_INVALID_PARAMS;

	if ((ROI.TopLeftX > ROI.BotRightX) || (ROI.TopLeftY < ROI.BotRightY))
		Status = VL53L1_ERROR_INVALID_PARAMS;

	LOG_FUNCTION_END(Status);
	return Status;
}

static VL53L1_GPIO_Interrupt_Mode ConvertModeToLLD(VL53L1_Error *pStatus,
		VL53L1_ThresholdMode CrossMode)
{
	VL53L1_GPIO_Interrupt_Mode Mode;

	switch (CrossMode) {
	case VL53L1_THRESHOLD_CROSSED_LOW:
		Mode = VL53L1_GPIOINTMODE_LEVEL_LOW;
		break;
	case VL53L1_THRESHOLD_CROSSED_HIGH:
		Mode = VL53L1_GPIOINTMODE_LEVEL_HIGH;
		break;
	case VL53L1_THRESHOLD_OUT_OF_WINDOW:
		Mode = VL53L1_GPIOINTMODE_OUT_OF_WINDOW;
		break;
	case VL53L1_THRESHOLD_IN_WINDOW:
		Mode = VL53L1_GPIOINTMODE_IN_WINDOW;
		break;
	default:


		Mode = VL53L1_GPIOINTMODE_LEVEL_HIGH;
		*pStatus = VL53L1_ERROR_INVALID_PARAMS;
	}
	return Mode;
}

static VL53L1_ThresholdMode ConvertModeFromLLD(VL53L1_Error *pStatus,
		VL53L1_GPIO_Interrupt_Mode CrossMode)
{
	VL53L1_ThresholdMode Mode;

	switch (CrossMode) {
	case VL53L1_GPIOINTMODE_LEVEL_LOW:
		Mode = VL53L1_THRESHOLD_CROSSED_LOW;
		break;
	case VL53L1_GPIOINTMODE_LEVEL_HIGH:
		Mode = VL53L1_THRESHOLD_CROSSED_HIGH;
		break;
	case VL53L1_GPIOINTMODE_OUT_OF_WINDOW:
		Mode = VL53L1_THRESHOLD_OUT_OF_WINDOW;
		break;
	case VL53L1_GPIOINTMODE_IN_WINDOW:
		Mode = VL53L1_THRESHOLD_IN_WINDOW;
		break;
	default:


		Mode = VL53L1_THRESHOLD_CROSSED_HIGH;
		*pStatus = VL53L1_ERROR_UNDEFINED;
	}
	return Mode;
}




VL53L1_Error VL53L1_GetVersion(VL53L1_Version_t *pVersion)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	pVersion->major = VL53L1_IMPLEMENTATION_VER_MAJOR;
	pVersion->minor = VL53L1_IMPLEMENTATION_VER_MINOR;
	pVersion->build = VL53L1_IMPLEMENTATION_VER_SUB;

	pVersion->revision = VL53L1_IMPLEMENTATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetPalSpecVersion(VL53L1_Version_t *pPalSpecVersion)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	pPalSpecVersion->major = VL53L1_SPECIFICATION_VER_MAJOR;
	pPalSpecVersion->minor = VL53L1_SPECIFICATION_VER_MINOR;
	pPalSpecVersion->build = VL53L1_SPECIFICATION_VER_SUB;

	pPalSpecVersion->revision = VL53L1_SPECIFICATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetProductRevision(VL53L1_DEV Dev,
	uint8_t *pProductRevisionMajor, uint8_t *pProductRevisionMinor)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t revision_id;
	VL53L1_LLDriverData_t   *pLLData;

	LOG_FUNCTION_START("");

	pLLData =  VL53L1DevStructGetLLDriverHandle(Dev);
	revision_id = pLLData->nvm_copy_data.identification__revision_id;
	*pProductRevisionMajor = 1;
	*pProductRevisionMinor = (revision_id & 0xF0) >> 4;

	LOG_FUNCTION_END(Status);
	return Status;

}

VL53L1_Error VL53L1_GetDeviceInfo(VL53L1_DEV Dev,
	VL53L1_DeviceInfo_t *pVL53L1_DeviceInfo)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t revision_id;
	VL53L1_LLDriverData_t   *pLLData;

	LOG_FUNCTION_START("");

	pLLData =  VL53L1DevStructGetLLDriverHandle(Dev);

	strncpy(pVL53L1_DeviceInfo->ProductId, "",
			VL53L1_DEVINFO_STRLEN-1);
	pVL53L1_DeviceInfo->ProductType =
			pLLData->nvm_copy_data.identification__module_type;

	revision_id = pLLData->nvm_copy_data.identification__revision_id;
	pVL53L1_DeviceInfo->ProductRevisionMajor = 1;
	pVL53L1_DeviceInfo->ProductRevisionMinor = (revision_id & 0xF0) >> 4;

#ifndef VL53L1_USE_EMPTY_STRING
	if (pVL53L1_DeviceInfo->ProductRevisionMinor == 0)
		strncpy(pVL53L1_DeviceInfo->Name,
				VL53L1_STRING_DEVICE_INFO_NAME0,
				VL53L1_DEVINFO_STRLEN-1);
	else
		strncpy(pVL53L1_DeviceInfo->Name,
				VL53L1_STRING_DEVICE_INFO_NAME1,
				VL53L1_DEVINFO_STRLEN-1);
	strncpy(pVL53L1_DeviceInfo->Type,
			VL53L1_STRING_DEVICE_INFO_TYPE,
			VL53L1_DEVINFO_STRLEN-1);
#else
	pVL53L1_DeviceInfo->Name[0] = 0;
	pVL53L1_DeviceInfo->Type[0] = 0;
#endif

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetRangeStatusString(uint8_t RangeStatus,
	char *pRangeStatusString)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_range_status_string(RangeStatus,
		pRangeStatusString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetPalErrorString(VL53L1_Error PalErrorCode,
	char *pPalErrorString)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_pal_error_string(PalErrorCode, pPalErrorString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetPalStateString(VL53L1_State PalStateCode,
	char *pPalStateString)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_pal_state_string(PalStateCode, pPalStateString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetPalState(VL53L1_DEV Dev, VL53L1_State *pPalState)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pPalState = PALDevDataGet(Dev, PalState);

	LOG_FUNCTION_END(Status);
	return Status;
}






VL53L1_Error VL53L1_SetDeviceAddress(VL53L1_DEV Dev, uint8_t DeviceAddress)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_WrByte(Dev, VL53L1_I2C_SLAVE__DEVICE_ADDRESS,
		DeviceAddress / 2);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_DataInit(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t i;

	LOG_FUNCTION_START("");

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_data_init(Dev, 1);

	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev, PalState, VL53L1_STATE_WAIT_STATICINIT);
		PALDevDataSet(Dev, CurrentParameters.PresetMode,
				VL53L1_PRESETMODE_RANGING);
	}



	for (i = 0; i < VL53L1_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
		if (Status == VL53L1_ERROR_NONE)
			Status |= VL53L1_SetLimitCheckEnable(Dev, i, 1);
		else
			break;

	}



	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetLimitCheckValue(Dev,
			VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE,
				(FixPoint1616_t)(18 * 65536));
	}
	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_SetLimitCheckValue(Dev,
			VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
				(FixPoint1616_t)(25 * 65536 / 100));


	}


	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_StaticInit(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t  measurement_mode;

	LOG_FUNCTION_START("");

	if (Status == VL53L1_ERROR_NONE)
		PALDevDataSet(Dev, PalState, VL53L1_STATE_IDLE);

	measurement_mode  = VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	PALDevDataSet(Dev, LLData.measurement_mode, measurement_mode);

	PALDevDataSet(Dev, CurrentParameters.NewDistanceMode,
			VL53L1_DISTANCEMODE_LONG);

	PALDevDataSet(Dev, CurrentParameters.InternalDistanceMode,
			VL53L1_DISTANCEMODE_LONG);

	PALDevDataSet(Dev, CurrentParameters.DistanceMode,
			VL53L1_DISTANCEMODE_LONG);

	PALDevDataSet(Dev, CurrentParameters.OutputMode,
			VL53L1_OUTPUTMODE_NEAREST);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_WaitDeviceBooted(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_poll_for_boot_completion(Dev,
			VL53L1_BOOT_COMPLETION_POLLING_TIMEOUT_MS);

	LOG_FUNCTION_END(Status);
	return Status;
}






static VL53L1_Error ComputeDevicePresetMode(
		VL53L1_PresetModes PresetMode,
		VL53L1_DistanceModes DistanceMode,
		uint16_t *pdss_config__target_total_rate_mcps,
		uint32_t *pphasecal_config_timeout_us,
		VL53L1_DevicePresetModes *pDevicePresetMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	*pDevicePresetMode = VL53L1_DEVICEPRESETMODE_STANDARD_RANGING;

	switch (PresetMode) {
	case VL53L1_PRESETMODE_LITE_RANGING:
		*pdss_config__target_total_rate_mcps = 0x0A00;
		*pphasecal_config_timeout_us = 1000;

		if (DistanceMode == VL53L1_DISTANCEMODE_SHORT)
			*pDevicePresetMode =
			VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_SHORT_RANGE;
		else if (DistanceMode == VL53L1_DISTANCEMODE_MEDIUM)
			*pDevicePresetMode =
				VL53L1_DEVICEPRESETMODE_STANDARD_RANGING;
		else

			*pDevicePresetMode =
			VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_LONG_RANGE;
		break;
	case VL53L1_PRESETMODE_RANGING:
		*pdss_config__target_total_rate_mcps = 0x1400;
		*pphasecal_config_timeout_us = 4000;

		if (DistanceMode == VL53L1_DISTANCEMODE_SHORT)
			*pDevicePresetMode =
				VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE;
		else if (DistanceMode == VL53L1_DISTANCEMODE_MEDIUM)
			*pDevicePresetMode =
				VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE;
		else

			*pDevicePresetMode =
				VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE;
		break;
	case VL53L1_PRESETMODE_MULTIZONES_SCANNING:
		*pdss_config__target_total_rate_mcps = 0x1400;
		*pphasecal_config_timeout_us = 4000;

		if (DistanceMode == VL53L1_DISTANCEMODE_SHORT)
			*pDevicePresetMode =
			VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_SHORT_RANGE;
		else if (DistanceMode == VL53L1_DISTANCEMODE_MEDIUM)
			*pDevicePresetMode =
				VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE;
		else

			*pDevicePresetMode =
			VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_LONG_RANGE;
		break;
	case VL53L1_PRESETMODE_AUTONOMOUS:
		*pdss_config__target_total_rate_mcps = 0x0A00;
		*pphasecal_config_timeout_us = 1000;

		if (DistanceMode == VL53L1_DISTANCEMODE_SHORT)
			*pDevicePresetMode =
			VL53L1_DEVICEPRESETMODE_TIMED_RANGING_SHORT_RANGE;
		else if (DistanceMode == VL53L1_DISTANCEMODE_MEDIUM)
			*pDevicePresetMode =
				VL53L1_DEVICEPRESETMODE_TIMED_RANGING;
		else

			*pDevicePresetMode =
			VL53L1_DEVICEPRESETMODE_TIMED_RANGING_LONG_RANGE;
		break;
	case VL53L1_PRESETMODE_OLT:
		*pdss_config__target_total_rate_mcps = 0x0A00;
		*pphasecal_config_timeout_us = 1000;

		*pDevicePresetMode = VL53L1_DEVICEPRESETMODE_OLT;
		break;
	default:


		Status = VL53L1_ERROR_MODE_NOT_SUPPORTED;
	}

	return Status;
}

static VL53L1_Error SetPresetMode(VL53L1_DEV Dev,
		VL53L1_PresetModes PresetMode,
		VL53L1_DistanceModes DistanceMode,
		uint32_t mm_config_timeout_us,
		uint32_t range_config_timeout_us,
		uint32_t inter_measurement_period_ms)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_DevicePresetModes   device_preset_mode;
	uint8_t measurement_mode;
	uint16_t dss_config__target_total_rate_mcps;
	uint32_t phasecal_config_timeout_us;

	LOG_FUNCTION_START("%d", (int)PresetMode);

	if (PresetMode == VL53L1_PRESETMODE_AUTONOMOUS)
		measurement_mode  = VL53L1_DEVICEMEASUREMENTMODE_TIMED;
	else
		measurement_mode  = VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;


	Status = ComputeDevicePresetMode(
			PresetMode,
			DistanceMode,
			&dss_config__target_total_rate_mcps,
			&phasecal_config_timeout_us,
			&device_preset_mode);


	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_set_preset_mode(
				Dev,
				device_preset_mode,
				dss_config__target_total_rate_mcps,
				phasecal_config_timeout_us,
				mm_config_timeout_us,
				range_config_timeout_us,
				inter_measurement_period_ms);

	if (Status == VL53L1_ERROR_NONE)
		PALDevDataSet(Dev, LLData.measurement_mode, measurement_mode);

	if (Status == VL53L1_ERROR_NONE)
		PALDevDataSet(Dev, CurrentParameters.PresetMode, PresetMode);

	PALDevDataSet(Dev, CurrentParameters.OutputMode,
			VL53L1_OUTPUTMODE_NEAREST);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetPresetMode(VL53L1_DEV Dev, VL53L1_PresetModes PresetMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("%d", (int)PresetMode);






	Status = SetPresetMode(Dev,
			PresetMode,
			VL53L1_DISTANCEMODE_LONG,
			2000,
			16000,
			1000);


	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev, CurrentParameters.InternalDistanceMode,
				VL53L1_DISTANCEMODE_LONG);

		PALDevDataSet(Dev, CurrentParameters.NewDistanceMode,
				VL53L1_DISTANCEMODE_LONG);

		if (PresetMode == VL53L1_PRESETMODE_LITE_RANGING)


			Status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(
				Dev, 33000);
		else


			Status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(
				Dev, 16000);
	}

	if (Status == VL53L1_ERROR_NONE) {


		Status = VL53L1_SetInterMeasurementPeriodMilliSeconds(Dev,
				1000);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetPresetMode(VL53L1_DEV Dev,
	VL53L1_PresetModes *pPresetMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pPresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetDistanceMode(VL53L1_DEV Dev,
		VL53L1_DistanceModes DistanceMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_PresetModes PresetMode;
	VL53L1_DistanceModes InternalDistanceMode;
	uint32_t mm_config_timeout_us;
	uint32_t range_config_timeout_us;
	uint32_t inter_measurement_period_ms;
	uint8_t manual_not_auto = 0;

	LOG_FUNCTION_START("%d", (int)DistanceMode);

	PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);








	if ((DistanceMode == VL53L1_DISTANCEMODE_SHORT) ||
		(DistanceMode == VL53L1_DISTANCEMODE_MEDIUM) ||
		(DistanceMode == VL53L1_DISTANCEMODE_LONG))
		manual_not_auto = 1;
	else
		manual_not_auto = 0;


	if ((manual_not_auto == 0) &&
		((PresetMode != VL53L1_PRESETMODE_LITE_RANGING) &&
		(PresetMode != VL53L1_PRESETMODE_RANGING))) {
		Status = VL53L1_ERROR_MODE_NOT_SUPPORTED;
	}





	if (Status == VL53L1_ERROR_NONE) {
		if ((DistanceMode == VL53L1_DISTANCEMODE_SHORT) ||
			(DistanceMode == VL53L1_DISTANCEMODE_MEDIUM))
			InternalDistanceMode = DistanceMode;
		else

			InternalDistanceMode = VL53L1_DISTANCEMODE_LONG;
	}

	mm_config_timeout_us =  PALDevDataGet(Dev, LLData.mm_config_timeout_us);
	range_config_timeout_us =  PALDevDataGet(Dev,
				LLData.range_config_timeout_us);
	inter_measurement_period_ms =  PALDevDataGet(Dev,
				LLData.inter_measurement_period_ms);

	if (Status == VL53L1_ERROR_NONE)
		Status = SetPresetMode(Dev,
				PresetMode,
				InternalDistanceMode,
				mm_config_timeout_us,
				range_config_timeout_us,
				inter_measurement_period_ms);

	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev, CurrentParameters.InternalDistanceMode,
				InternalDistanceMode);
		PALDevDataSet(Dev, CurrentParameters.NewDistanceMode,
				InternalDistanceMode);
		PALDevDataSet(Dev, CurrentParameters.DistanceMode,
				DistanceMode);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetDistanceMode(VL53L1_DEV Dev,
	VL53L1_DistanceModes *pDistanceMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pDistanceMode = PALDevDataGet(Dev, CurrentParameters.DistanceMode);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetOutputMode(VL53L1_DEV Dev,
		VL53L1_OutputModes OutputMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if ((OutputMode != VL53L1_OUTPUTMODE_NEAREST) &&
		(OutputMode != VL53L1_OUTPUTMODE_STRONGEST))
		Status = VL53L1_ERROR_MODE_NOT_SUPPORTED;
	else
		PALDevDataSet(Dev, CurrentParameters.OutputMode, OutputMode);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetOutputMode(VL53L1_DEV Dev,
		VL53L1_OutputModes *pOutputMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pOutputMode = PALDevDataGet(Dev, CurrentParameters.OutputMode);

	LOG_FUNCTION_END(Status);
	return Status;
}



VL53L1_Error VL53L1_SetMeasurementTimingBudgetMicroSeconds(VL53L1_DEV Dev,
	uint32_t MeasurementTimingBudgetMicroSeconds)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t Mm1Enabled;
	uint8_t Mm2Enabled;
	uint32_t TimingGuard;
	uint32_t divisor;
	uint32_t TimingBudget;
	uint32_t  MmTimeoutUs;
	VL53L1_PresetModes PresetMode;
	uint32_t PhaseCalTimeoutUs;

	LOG_FUNCTION_START("");



	if (MeasurementTimingBudgetMicroSeconds > 10000000)
		Status = VL53L1_ERROR_INVALID_PARAMS;

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM1, &Mm1Enabled);
	}

	if (Status == VL53L1_ERROR_NONE) {
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM2, &Mm2Enabled);
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_get_timeouts_us(Dev,
			&PhaseCalTimeoutUs,
			&MmTimeoutUs,
			&TimingBudget);

	if (Status == VL53L1_ERROR_NONE) {
		PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);

		TimingGuard = 0;
		divisor = 1;
		switch (PresetMode) {
		case VL53L1_PRESETMODE_LITE_RANGING:
		case VL53L1_PRESETMODE_AUTONOMOUS:
			if ((Mm1Enabled == 1) || (Mm2Enabled == 1))
				TimingGuard = 5000;
			else
				TimingGuard = 1000;
		break;
		case VL53L1_PRESETMODE_RANGING:
		case VL53L1_PRESETMODE_MULTIZONES_SCANNING:
			TimingGuard = 1700;
			divisor = 6;
		break;
		case VL53L1_PRESETMODE_OLT:
			TimingGuard = MmTimeoutUs + 5000;
		break;
		default:


			Status = VL53L1_ERROR_MODE_NOT_SUPPORTED;
		}

		if (MeasurementTimingBudgetMicroSeconds > TimingGuard) {
			TimingBudget = (MeasurementTimingBudgetMicroSeconds
					- TimingGuard) / divisor;

			Status = VL53L1_set_timeouts_us(
					Dev,
					PhaseCalTimeoutUs,
					MmTimeoutUs,
					TimingBudget);

			if (Status == VL53L1_ERROR_NONE)
				PALDevDataSet(Dev,
					LLData.range_config_timeout_us,
					TimingBudget);
		} else {
			Status = VL53L1_ERROR_INVALID_PARAMS;
		}
	}
	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev,
			CurrentParameters.MeasurementTimingBudgetMicroSeconds,
			MeasurementTimingBudgetMicroSeconds);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetMeasurementTimingBudgetMicroSeconds(VL53L1_DEV Dev,
	uint32_t *pMeasurementTimingBudgetMicroSeconds)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t Mm1Enabled = 0;
	uint8_t Mm2Enabled = 0;
	uint32_t  MmTimeoutUs = 0;
	uint32_t  RangeTimeoutUs = 0;
	uint32_t  MeasTimingBdg = 0;
	uint32_t PhaseCalTimeoutUs = 0;
	VL53L1_PresetModes PresetMode;

	LOG_FUNCTION_START("");

	*pMeasurementTimingBudgetMicroSeconds = 0;

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM1, &Mm1Enabled);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM2, &Mm2Enabled);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_get_timeouts_us(Dev,
			&PhaseCalTimeoutUs,
			&MmTimeoutUs,
			&RangeTimeoutUs);

	if (Status == VL53L1_ERROR_NONE) {
		PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);

		switch (PresetMode) {
		case VL53L1_PRESETMODE_LITE_RANGING:
		case VL53L1_PRESETMODE_AUTONOMOUS:
			if ((Mm1Enabled == 1) || (Mm2Enabled == 1))
				MeasTimingBdg = RangeTimeoutUs + 5000;
			else
				MeasTimingBdg = RangeTimeoutUs + 1000;

		break;
		case VL53L1_PRESETMODE_RANGING:
			MeasTimingBdg = (6 * RangeTimeoutUs) + 1700;
		break;
		case VL53L1_PRESETMODE_MULTIZONES_SCANNING:
			MeasTimingBdg = (6 * RangeTimeoutUs) + 1700;
		break;
		case VL53L1_PRESETMODE_OLT:
			MeasTimingBdg = RangeTimeoutUs + MmTimeoutUs + 5000;
		break;
		default:


			Status = VL53L1_ERROR_MODE_NOT_SUPPORTED;
		}
	}
	if (Status == VL53L1_ERROR_NONE)
		*pMeasurementTimingBudgetMicroSeconds = MeasTimingBdg;

	LOG_FUNCTION_END(Status);
	return Status;
}



VL53L1_Error VL53L1_SetInterMeasurementPeriodMilliSeconds(VL53L1_DEV Dev,
	uint32_t InterMeasurementPeriodMilliSeconds)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_set_inter_measurement_period_ms(Dev,
			InterMeasurementPeriodMilliSeconds);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetInterMeasurementPeriodMilliSeconds(VL53L1_DEV Dev,
	uint32_t *pInterMeasurementPeriodMilliSeconds)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_inter_measurement_period_ms(Dev,
			pInterMeasurementPeriodMilliSeconds);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetDmaxReflectance(VL53L1_DEV Dev,
		FixPoint1616_t value)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_dmax_reflectance_array_t dmax_reflectances;

	LOG_FUNCTION_START("");
	Status = VL53L1_get_dmax_reflectance_values(Dev, &dmax_reflectances);
	if (Status == VL53L1_ERROR_NONE) {
		dmax_reflectances.target_reflectance_for_dmax[DMAX_REFLECTANCE_IDX] =
				VL53L1_FIXPOINT1616TOFIXPOINT72(value);
		Status = VL53L1_set_dmax_reflectance_values(Dev, &dmax_reflectances);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetDmaxReflectance(VL53L1_DEV Dev,
		FixPoint1616_t *pvalue)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_dmax_reflectance_array_t dmax_reflectances;
	uint16_t r;

	LOG_FUNCTION_START("");
	Status = VL53L1_get_dmax_reflectance_values(Dev, &dmax_reflectances);
	if (Status == VL53L1_ERROR_NONE) {
		r = dmax_reflectances.target_reflectance_for_dmax[DMAX_REFLECTANCE_IDX];
		*pvalue = VL53L1_FIXPOINT72TOFIXPOINT1616(r);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_SetDmaxMode(VL53L1_DEV Dev,
		VL53L1_DeviceDmaxModes DmaxMode)
{

	VL53L1_Error  Status = VL53L1_ERROR_NONE;
	VL53L1_DeviceDmaxMode dmax_mode;

	LOG_FUNCTION_START("");

	switch(DmaxMode) {
	case DMAXMODE_FMT_CAL_DATA:
		dmax_mode = VL53L1_DEVICEDMAXMODE__FMT_CAL_DATA;
		break;
	case DMAXMODE_CUSTCAL_DATA:
		dmax_mode = VL53L1_DEVICEDMAXMODE__CUST_CAL_DATA;
		break;
	case DMAXMODE_PER_ZONE_CAL_DATA:
		dmax_mode = VL53L1_DEVICEDMAXMODE__PER_ZONE_CAL_DATA;
		break;
	default:
		Status = VL53L1_ERROR_INVALID_PARAMS;
		break;
	}
	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_set_dmax_mode(Dev, dmax_mode);

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetDmaxMode(VL53L1_DEV Dev,
	VL53L1_DeviceDmaxModes *pDmaxMode)
{
	VL53L1_Error  Status = VL53L1_ERROR_NONE;
	VL53L1_DeviceDmaxMode dmax_mode;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_dmax_mode(Dev, &dmax_mode);
	if (Status == VL53L1_ERROR_NONE) {
		switch(dmax_mode) {
		case VL53L1_DEVICEDMAXMODE__FMT_CAL_DATA:
			*pDmaxMode = DMAXMODE_FMT_CAL_DATA;
			break;
		case VL53L1_DEVICEDMAXMODE__CUST_CAL_DATA:
			*pDmaxMode = DMAXMODE_CUSTCAL_DATA;
			break;
		case VL53L1_DEVICEDMAXMODE__PER_ZONE_CAL_DATA:
			*pDmaxMode = DMAXMODE_PER_ZONE_CAL_DATA;
			break;
		default:


			*pDmaxMode = VL53L1_ERROR_NOT_IMPLEMENTED;
			break;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}








VL53L1_Error VL53L1_GetNumberOfLimitCheck(uint16_t *pNumberOfLimitCheck)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pNumberOfLimitCheck = VL53L1_CHECKENABLE_NUMBER_OF_CHECKS;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetLimitCheckInfo(uint16_t LimitCheckId,
	char *pLimitCheckString)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_limit_check_info(LimitCheckId,
		pLimitCheckString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetLimitCheckStatus(VL53L1_DEV Dev, uint16_t LimitCheckId,
	uint8_t *pLimitCheckStatus)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL53L1_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	} else {
		VL53L1_GETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
			LimitCheckId, Temp8);
		*pLimitCheckStatus = Temp8;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

static VL53L1_Error SetLimitValue(VL53L1_DEV Dev, uint16_t LimitCheckId,
		FixPoint1616_t value)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint16_t *pmin_count_rate;

	uint16_t tmpuint16;


	LOG_FUNCTION_START("");

	pmin_count_rate = &(PALDevDataGet(Dev,
		LLData.tim_cfg.range_config__min_count_rate_rtn_limit_mcps));

	switch (LimitCheckId) {
	case VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE:
		tmpuint16 = VL53L1_FIXPOINT1616TOFIXPOINT142(value);
		PALDevDataSet(Dev, LLData.tim_cfg.range_config__sigma_thresh,
			tmpuint16);
	break;
	case VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
		tmpuint16 = VL53L1_FIXPOINT1616TOFIXPOINT97(value);

		*pmin_count_rate = tmpuint16;
	break;
	default:
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_SetLimitCheckEnable(VL53L1_DEV Dev, uint16_t LimitCheckId,
	uint8_t LimitCheckEnable)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	FixPoint1616_t TempFix1616 = 0;

	LOG_FUNCTION_START("");


	if (LimitCheckId >= VL53L1_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	} else {


		if (LimitCheckEnable == 0)
			TempFix1616 = 0;
		else
			VL53L1_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				LimitCheckId, TempFix1616);

		Status = SetLimitValue(Dev, LimitCheckId, TempFix1616);
	}

	if (Status == VL53L1_ERROR_NONE)
		VL53L1_SETARRAYPARAMETERFIELD(Dev,
			LimitChecksEnable,
			LimitCheckId,
			((LimitCheckEnable == 0) ? 0 : 1));



	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetLimitCheckEnable(VL53L1_DEV Dev, uint16_t LimitCheckId,
	uint8_t *pLimitCheckEnable)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t Temp8;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL53L1_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L1_ERROR_INVALID_PARAMS;
		*pLimitCheckEnable = 0;
	} else {
		VL53L1_GETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
			LimitCheckId, Temp8);
		*pLimitCheckEnable = Temp8;
	}


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetLimitCheckValue(VL53L1_DEV Dev, uint16_t LimitCheckId,
	FixPoint1616_t LimitCheckValue)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t LimitChecksEnable;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL53L1_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	} else {

		VL53L1_GETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
				LimitCheckId,
				LimitChecksEnable);

		if (LimitChecksEnable == 0) {


			VL53L1_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				LimitCheckId, LimitCheckValue);
		} else {

			Status = SetLimitValue(Dev, LimitCheckId,
					LimitCheckValue);

			if (Status == VL53L1_ERROR_NONE) {
				VL53L1_SETARRAYPARAMETERFIELD(Dev,
					LimitChecksValue,
					LimitCheckId, LimitCheckValue);
			}
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetLimitCheckValue(VL53L1_DEV Dev, uint16_t LimitCheckId,
	FixPoint1616_t *pLimitCheckValue)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint16_t MinCountRate;
	FixPoint1616_t TempFix1616;
	uint16_t SigmaThresh;

	LOG_FUNCTION_START("");

	MinCountRate = PALDevDataGet(Dev,
		LLData.tim_cfg.range_config__min_count_rate_rtn_limit_mcps);

	switch (LimitCheckId) {
	case VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE:
		SigmaThresh = PALDevDataGet(Dev,
			LLData.tim_cfg.range_config__sigma_thresh);
		TempFix1616 = VL53L1_FIXPOINT142TOFIXPOINT1616(SigmaThresh);
		break;
	case VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
		TempFix1616 = VL53L1_FIXPOINT97TOFIXPOINT1616(MinCountRate);
		break;
	default:
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	if (Status == VL53L1_ERROR_NONE) {

		if (TempFix1616 == 0) {


			VL53L1_GETARRAYPARAMETERFIELD(Dev,
				LimitChecksValue, LimitCheckId,
				TempFix1616);
			*pLimitCheckValue = TempFix1616;
			VL53L1_SETARRAYPARAMETERFIELD(Dev,
				LimitChecksEnable, LimitCheckId, 0);
		} else {
			*pLimitCheckValue = TempFix1616;
			VL53L1_SETARRAYPARAMETERFIELD(Dev,
				LimitChecksValue, LimitCheckId,
				TempFix1616);
			VL53L1_SETARRAYPARAMETERFIELD(Dev,
				LimitChecksEnable, LimitCheckId, 1);
		}
	}
	LOG_FUNCTION_END(Status);
	return Status;

}

VL53L1_Error VL53L1_GetLimitCheckCurrent(VL53L1_DEV Dev, uint16_t LimitCheckId,
	FixPoint1616_t *pLimitCheckCurrent)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	FixPoint1616_t TempFix1616 = 0;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL53L1_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	} else {
		VL53L1_GETARRAYPARAMETERFIELD(Dev, LimitChecksCurrent,
			LimitCheckId, TempFix1616);
		*pLimitCheckCurrent = TempFix1616;
	}

	LOG_FUNCTION_END(Status);
	return Status;

}









VL53L1_Error VL53L1_GetMaxNumberOfROI(VL53L1_DEV Dev,
	uint8_t *pMaxNumberOfROI)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_PresetModes PresetMode;

	LOG_FUNCTION_START("");

	PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);



	if (PresetMode == VL53L1_PRESETMODE_MULTIZONES_SCANNING)
		*pMaxNumberOfROI = VL53L1_MAX_USER_ZONES;
	else
		*pMaxNumberOfROI = 1;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetROI(VL53L1_DEV Dev,
		VL53L1_RoiConfig_t *pRoiConfig)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_PresetModes PresetMode;
	uint8_t MaxNumberOfROI;
	VL53L1_zone_config_t  zone_cfg;
	VL53L1_UserRoi_t CurrROI;
	uint8_t  i;
	uint8_t  x_centre;
	uint8_t  y_centre;
	uint8_t  width, height;

	LOG_FUNCTION_START("");



	PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);



	if (PresetMode == VL53L1_PRESETMODE_MULTIZONES_SCANNING)
		MaxNumberOfROI = VL53L1_MAX_USER_ZONES;
	else
		MaxNumberOfROI = 1;

	if ((pRoiConfig->NumberOfRoi > MaxNumberOfROI) ||
			(pRoiConfig->NumberOfRoi < 1))
		Status = VL53L1_ERROR_INVALID_PARAMS;

	if (Status == VL53L1_ERROR_NONE) {



		zone_cfg.max_zones = MaxNumberOfROI;
		zone_cfg.active_zones = pRoiConfig->NumberOfRoi - 1;

		for (i = 0; i < pRoiConfig->NumberOfRoi; i++) {
			CurrROI = pRoiConfig->UserRois[i];









			Status = CheckValidRectRoi(CurrROI);
			if (Status != VL53L1_ERROR_NONE)
				break;

			x_centre = (CurrROI.BotRightX + CurrROI.TopLeftX  + 1)
					/ 2;
			y_centre = (CurrROI.TopLeftY  + CurrROI.BotRightY + 1)
					/ 2;
			width =     (CurrROI.BotRightX - CurrROI.TopLeftX);
			height =    (CurrROI.TopLeftY  - CurrROI.BotRightY);
			if ((width < 3) || (height < 3)) {
				Status = VL53L1_ERROR_INVALID_PARAMS;
				break;
			}
			zone_cfg.user_zones[i].x_centre = x_centre;
			zone_cfg.user_zones[i].y_centre = y_centre;
			zone_cfg.user_zones[i].width = width;
			zone_cfg.user_zones[i].height = height;
		}
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_set_zone_config(Dev, &zone_cfg);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetROI(VL53L1_DEV Dev,
		VL53L1_RoiConfig_t *pRoiConfig)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_zone_config_t      zone_cfg;
	uint8_t  i;
	uint8_t  TopLeftX;
	uint8_t  TopLeftY;
	uint8_t  BotRightX;
	uint8_t  BotRightY;

	LOG_FUNCTION_START("");

	VL53L1_get_zone_config(Dev, &zone_cfg);

	pRoiConfig->NumberOfRoi = zone_cfg.active_zones + 1;

	for (i = 0; i < pRoiConfig->NumberOfRoi; i++) {
		TopLeftX = (2 * zone_cfg.user_zones[i].x_centre -
			zone_cfg.user_zones[i].width) >> 1;
		TopLeftY = (2 * zone_cfg.user_zones[i].y_centre +
			zone_cfg.user_zones[i].height) >> 1;
		BotRightX = (2 * zone_cfg.user_zones[i].x_centre +
			zone_cfg.user_zones[i].width) >> 1;
		BotRightY = (2 * zone_cfg.user_zones[i].y_centre -
			zone_cfg.user_zones[i].height) >> 1;
		pRoiConfig->UserRois[i].TopLeftX = TopLeftX;
		pRoiConfig->UserRois[i].TopLeftY = TopLeftY;
		pRoiConfig->UserRois[i].BotRightX = BotRightX;
		pRoiConfig->UserRois[i].BotRightY = BotRightY;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}









VL53L1_Error VL53L1_GetNumberOfSequenceSteps(VL53L1_DEV Dev,
	uint8_t *pNumberOfSequenceSteps)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	SUPPRESS_UNUSED_WARNING(Dev);

	LOG_FUNCTION_START("");

	*pNumberOfSequenceSteps = VL53L1_SEQUENCESTEP_NUMBER_OF_ITEMS;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetSequenceStepsInfo(VL53L1_SequenceStepId SequenceStepId,
	char *pSequenceStepsString)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_sequence_steps_info(
			SequenceStepId,
			pSequenceStepsString);

	LOG_FUNCTION_END(Status);

	return Status;
}

VL53L1_Error VL53L1_SetSequenceStepEnable(VL53L1_DEV Dev,
	VL53L1_SequenceStepId SequenceStepId, uint8_t SequenceStepEnabled)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t SequenceConfig = 0;
	uint8_t SequenceConfigNew = 0;
	uint32_t MeasurementTimingBudgetMicroSeconds;

	LOG_FUNCTION_START("");

	SequenceConfig = PALDevDataGet(Dev,
			LLData.dyn_cfg.system__sequence_config);

	SequenceConfigNew = SequenceConfig;

	if (Status == VL53L1_ERROR_NONE) {
		if (SequenceStepEnabled == 1) {




			switch (SequenceStepId) {
			case VL53L1_SEQUENCESTEP_VHV:
				SequenceConfigNew |= 0x01;
				break;
			case VL53L1_SEQUENCESTEP_PHASECAL:
				SequenceConfigNew |= 0x02;
				break;
			case VL53L1_SEQUENCESTEP_REFPHASE:
				SequenceConfigNew |= 0x04;
				break;
			case VL53L1_SEQUENCESTEP_DSS1:
				SequenceConfigNew |= 0x08;
				break;
			case VL53L1_SEQUENCESTEP_DSS2:
				SequenceConfigNew |= 0x10;
				break;
			case VL53L1_SEQUENCESTEP_MM1:
				SequenceConfigNew |= 0x20;
				break;
			case VL53L1_SEQUENCESTEP_MM2:
				SequenceConfigNew |= 0x40;
				break;
			case VL53L1_SEQUENCESTEP_RANGE:
				SequenceConfigNew |= 0x80;
				break;
			default:
				Status = VL53L1_ERROR_INVALID_PARAMS;
			}
		} else {



			switch (SequenceStepId) {
			case VL53L1_SEQUENCESTEP_VHV:
				SequenceConfigNew &= 0xfe;
				break;
			case VL53L1_SEQUENCESTEP_PHASECAL:
				SequenceConfigNew &= 0xfd;
				break;
			case VL53L1_SEQUENCESTEP_REFPHASE:
				SequenceConfigNew &= 0xfb;
				break;
			case VL53L1_SEQUENCESTEP_DSS1:
				SequenceConfigNew &= 0xf7;
				break;
			case VL53L1_SEQUENCESTEP_DSS2:
				SequenceConfigNew &= 0xef;
				break;
			case VL53L1_SEQUENCESTEP_MM1:
				SequenceConfigNew &= 0xdf;
				break;
			case VL53L1_SEQUENCESTEP_MM2:
				SequenceConfigNew &= 0xbf;
				break;
			case VL53L1_SEQUENCESTEP_RANGE:
				SequenceConfigNew &= 0x7f;
				break;
			default:
				Status = VL53L1_ERROR_INVALID_PARAMS;
			}
		}
	}



	if ((SequenceConfigNew != SequenceConfig) &&
			(Status == VL53L1_ERROR_NONE)) {
		PALDevDataSet(Dev, LLData.dyn_cfg.system__sequence_config,
				SequenceConfigNew);



		MeasurementTimingBudgetMicroSeconds = PALDevDataGet(Dev,
			CurrentParameters.MeasurementTimingBudgetMicroSeconds);

		VL53L1_SetMeasurementTimingBudgetMicroSeconds(Dev,
			MeasurementTimingBudgetMicroSeconds);
	}


	LOG_FUNCTION_END(Status);

	return Status;
}


VL53L1_Error VL53L1_GetSequenceStepEnable(VL53L1_DEV Dev,
	VL53L1_SequenceStepId SequenceStepId, uint8_t *pSequenceStepEnabled)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t SequenceConfig = 0;

	LOG_FUNCTION_START("");

	SequenceConfig = PALDevDataGet(Dev,
			LLData.dyn_cfg.system__sequence_config);

	switch (SequenceStepId) {
	case VL53L1_SEQUENCESTEP_VHV:
		*pSequenceStepEnabled = SequenceConfig & 0x01;
		break;
	case VL53L1_SEQUENCESTEP_PHASECAL:
		*pSequenceStepEnabled = (SequenceConfig & 0x02) >> 1;
		break;
	case VL53L1_SEQUENCESTEP_REFPHASE:
		*pSequenceStepEnabled = (SequenceConfig & 0x04) >> 2;
		break;
	case VL53L1_SEQUENCESTEP_DSS1:
		*pSequenceStepEnabled = (SequenceConfig & 0x08) >> 3;
		break;
	case VL53L1_SEQUENCESTEP_DSS2:
		*pSequenceStepEnabled = (SequenceConfig & 0x10) >> 4;
		break;
	case VL53L1_SEQUENCESTEP_MM1:
		*pSequenceStepEnabled = (SequenceConfig & 0x20) >> 5;
		break;
	case VL53L1_SEQUENCESTEP_MM2:
		*pSequenceStepEnabled = (SequenceConfig & 0x40) >> 6;
		break;
	case VL53L1_SEQUENCESTEP_RANGE:
		*pSequenceStepEnabled = (SequenceConfig & 0x80) >> 7;
		break;
	default:
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}












VL53L1_Error VL53L1_StartMeasurement(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t DeviceMeasurementMode;
	VL53L1_State CurrPalState;

	LOG_FUNCTION_START("");

	CurrPalState = PALDevDataGet(Dev, PalState);
	switch (CurrPalState) {
	case VL53L1_STATE_IDLE:
		Status = VL53L1_ERROR_NONE;
		break;
	case VL53L1_STATE_POWERDOWN:
	case VL53L1_STATE_WAIT_STATICINIT:
	case VL53L1_STATE_STANDBY:
	case VL53L1_STATE_RUNNING:
	case VL53L1_STATE_RESET:
	case VL53L1_STATE_UNKNOWN:
	case VL53L1_STATE_ERROR:
		Status = VL53L1_ERROR_INVALID_COMMAND;
		break;
	default:
		Status = VL53L1_ERROR_UNDEFINED;
	}

	DeviceMeasurementMode = PALDevDataGet(Dev, LLData.measurement_mode);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_init_and_start_range(
				Dev,
				DeviceMeasurementMode,
				VL53L1_DEVICECONFIGLEVEL_FULL);

	if (Status == VL53L1_ERROR_NONE) {


		PALDevDataSet(Dev, PalState, VL53L1_STATE_RUNNING);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_StopMeasurement(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_stop_range(Dev);

	if (Status == VL53L1_ERROR_NONE) {


		PALDevDataSet(Dev, PalState, VL53L1_STATE_IDLE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

static VL53L1_Error ChangePresetMode(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_PresetModes PresetMode;
	VL53L1_DistanceModes NewDistanceMode;
	VL53L1_zone_config_t     zone_config;
	uint8_t DeviceMeasurementMode;
	uint32_t mm_config_timeout_us;
	uint32_t range_config_timeout_us;
	uint32_t inter_measurement_period_ms;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_zone_config(Dev, &zone_config);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_stop_range(Dev);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_WaitUs(Dev, 500);

	if (Status == VL53L1_ERROR_NONE) {
		PresetMode = PALDevDataGet(Dev,
				CurrentParameters.PresetMode);
		NewDistanceMode = PALDevDataGet(Dev,
				CurrentParameters.NewDistanceMode);
		mm_config_timeout_us =  PALDevDataGet(Dev,
				LLData.mm_config_timeout_us);
		range_config_timeout_us =  PALDevDataGet(Dev,
					LLData.range_config_timeout_us);
		inter_measurement_period_ms =  PALDevDataGet(Dev,
					LLData.inter_measurement_period_ms);

		Status = SetPresetMode(Dev,
				PresetMode,
				NewDistanceMode,
				mm_config_timeout_us,
				range_config_timeout_us,
				inter_measurement_period_ms);
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_set_zone_config(Dev, &zone_config);

	if (Status == VL53L1_ERROR_NONE) {
		DeviceMeasurementMode = PALDevDataGet(Dev,
				LLData.measurement_mode);

		Status = VL53L1_init_and_start_range(
				Dev,
				DeviceMeasurementMode,
				VL53L1_DEVICECONFIGLEVEL_FULL);
	}

	if (Status == VL53L1_ERROR_NONE)
		PALDevDataSet(Dev,
			CurrentParameters.InternalDistanceMode,
			NewDistanceMode);

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_ClearInterruptAndStartMeasurement(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t DeviceMeasurementMode;
	VL53L1_DistanceModes InternalDistanceMode;
	VL53L1_DistanceModes NewDistanceMode;

	LOG_FUNCTION_START("");

	DeviceMeasurementMode = PALDevDataGet(Dev, LLData.measurement_mode);
	InternalDistanceMode = PALDevDataGet(Dev,
			CurrentParameters.InternalDistanceMode);
	NewDistanceMode = PALDevDataGet(Dev,
			CurrentParameters.NewDistanceMode);

	if (NewDistanceMode != InternalDistanceMode)
		Status = ChangePresetMode(Dev);
	else
		Status = VL53L1_clear_interrupt_and_enable_next_range(
						Dev,
						DeviceMeasurementMode);

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetMeasurementDataReady(VL53L1_DEV Dev,
	uint8_t *pMeasurementDataReady)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_is_new_data_ready(Dev, pMeasurementDataReady);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_WaitMeasurementDataReady(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	Status = VL53L1_poll_for_range_completion(Dev,
			VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS);

	LOG_FUNCTION_END(Status);
	return Status;
}

static void GenNewPresetMode(VL53L1_DEV Dev, int16_t RefRange)
{
	VL53L1_DistanceModes InternalDistanceMode;
	VL53L1_DistanceModes NewDistanceMode;
	uint16_t HRLI = 600;
	uint16_t HRLH = 700;
	uint16_t MRLI = 1400;
	uint16_t MRLH = 1500;

	InternalDistanceMode = PALDevDataGet(Dev,
			CurrentParameters.InternalDistanceMode);

	NewDistanceMode = InternalDistanceMode;
	switch (InternalDistanceMode) {
	case VL53L1_DISTANCEMODE_SHORT:





		if (RefRange > MRLH)
			NewDistanceMode = VL53L1_DISTANCEMODE_LONG;
		else if (RefRange > HRLH)
			NewDistanceMode = VL53L1_DISTANCEMODE_MEDIUM;
		break;
	case VL53L1_DISTANCEMODE_MEDIUM:





		if (RefRange > MRLH)
			NewDistanceMode = VL53L1_DISTANCEMODE_LONG;
		else if (RefRange < HRLI)
			NewDistanceMode = VL53L1_DISTANCEMODE_SHORT;
		break;
	default:






		if (RefRange < HRLI)
			NewDistanceMode = VL53L1_DISTANCEMODE_SHORT;
		else if (RefRange < MRLI)
			NewDistanceMode = VL53L1_DISTANCEMODE_MEDIUM;
		break;
	}

	PALDevDataSet(Dev, CurrentParameters.NewDistanceMode, NewDistanceMode);

}

static void CheckAndChangeDistanceMode(VL53L1_DEV Dev,
		VL53L1_RangingMeasurementData_t *pRangeData,
		int16_t AmbientDmaxMm)
{

	VL53L1_DistanceModes DistanceMode;
	int16_t RefRange;
	uint8_t RangeValid;






	RangeValid = (pRangeData->RangeStatus == VL53L1_RANGESTATUS_RANGE_VALID)
			? 1: 0;

	RefRange = (RangeValid == 1) ? pRangeData->RangeMilliMeter :
			AmbientDmaxMm;

	DistanceMode = PALDevDataGet(Dev, CurrentParameters.DistanceMode);

	if (((DistanceMode == VL53L1_DISTANCEMODE_AUTO_LITE) &&
		(RangeValid == 0)) ||
		((DistanceMode == VL53L1_DISTANCEMODE_AUTO) &&
		(RangeValid == 0) && (AmbientDmaxMm == 0))) {
		PALDevDataSet(Dev, CurrentParameters.NewDistanceMode,
				VL53L1_DISTANCEMODE_LONG);

	} else if ((DistanceMode == VL53L1_DISTANCEMODE_AUTO_LITE) ||
			(DistanceMode == VL53L1_DISTANCEMODE_AUTO))
		GenNewPresetMode(Dev, RefRange);

}


static uint8_t ComputeRQL(uint8_t active_results,
		VL53L1_range_data_t *presults_data)
{
	int16_t T_Wide = 150;
	int16_t SRL = 300;
	uint16_t SRAS = 30;
	FixPoint1616_t RAS;
	FixPoint1616_t SRQL;
	FixPoint1616_t GI = 117.7 * 65536;
	FixPoint1616_t GGm = 48.8 * 65536;
	FixPoint1616_t LRAP = 0.1 * 65536;
	FixPoint1616_t partial;
	uint8_t finalvalue;

	if (active_results == 0)
		return 0;
	else if ((presults_data->max_range_mm -
			presults_data->min_range_mm) >= T_Wide)
		return 50;
	else {
		if (presults_data->median_range_mm < SRL)
			RAS = SRAS * 65536;
		else
			RAS = LRAP * presults_data->median_range_mm;



		if (RAS != 0) {
			partial = (GGm * presults_data->VL53L1_PRM_00004) + (RAS >> 1);
			partial = partial / RAS;
			partial = partial * 65536;
			if (GI > partial)
				SRQL = GI - partial;
			else
				SRQL = 50 * 65536;
		} else
			SRQL = 100 * 65536;

		finalvalue = (uint8_t)(SRQL >> 16);
		return MAX(50, MIN(100, finalvalue));
	}
}

static VL53L1_Error SetSimpleData(VL53L1_DEV Dev,
	uint8_t active_results,
	VL53L1_range_data_t *presults_data,
	VL53L1_RangingMeasurementData_t *pRangeData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t FilteredRangeStatus;
	uint8_t SigmaLimitflag;
	uint8_t SignalLimitflag;
	uint8_t Temp8Enable;
	uint8_t Temp8;
	uint8_t NoneFlag = 0;
	FixPoint1616_t AmbientRate;
	FixPoint1616_t SignalRate;
	FixPoint1616_t TempFix1616;
	FixPoint1616_t LimitCheckValue;

	pRangeData->TimeStamp = presults_data->time_stamp;
	FilteredRangeStatus = presults_data->range_status & 0x1F;

	pRangeData->RangeQualityLevel = ComputeRQL(active_results,
						presults_data);

	pRangeData->RangeMaxMilliMeter = presults_data->max_range_mm;
	pRangeData->RangeMinMilliMeter = presults_data->min_range_mm;

	SignalRate = VL53L1_FIXPOINT97TOFIXPOINT1616(
		presults_data->peak_signal_count_rate_mcps);
	pRangeData->SignalRateRtnMegaCps
		= SignalRate;

	AmbientRate = VL53L1_FIXPOINT97TOFIXPOINT1616(
		presults_data->ambient_count_rate_mcps);
	pRangeData->AmbientRateRtnMegaCps = AmbientRate;

	pRangeData->EffectiveSpadRtnCount =
		presults_data->VL53L1_PRM_00003;

	TempFix1616 = VL53L1_FIXPOINT142TOFIXPOINT1616(
			presults_data->VL53L1_PRM_00004);

	pRangeData->SigmaMilliMeter = TempFix1616;

	pRangeData->RangeMilliMeter = presults_data->median_range_mm;

	pRangeData->RangeFractionalPart = 0;

	if (FilteredRangeStatus ==
			VL53L1_DEVICEERROR_NOUPDATE ||
		FilteredRangeStatus ==
				VL53L1_DEVICEERROR_ALGOUNDERFLOW ||
		FilteredRangeStatus ==
				VL53L1_DEVICEERROR_ALGOOVERFLOW ||
		FilteredRangeStatus ==
				VL53L1_DEVICEERROR_RANGEIGNORETHRESHOLD ||
		FilteredRangeStatus ==
				VL53L1_DEVICEERROR_REFSPADCHARNOTENOUGHDPADS ||
		FilteredRangeStatus ==
				VL53L1_DEVICEERROR_REFSPADCHARMORETHANTARGET ||
		FilteredRangeStatus ==
				VL53L1_DEVICEERROR_REFSPADCHARLESSTHANTARGET ||
		FilteredRangeStatus == VL53L1_DEVICEERROR_MULTCLIPFAIL ||
		FilteredRangeStatus == VL53L1_DEVICEERROR_GPHSTREAMCOUNT0READY
			) {
		NoneFlag = 1;
	} else {
		NoneFlag = 0;
	}

	if (FilteredRangeStatus ==
			VL53L1_DEVICEERROR_RANGECOMPLETE) {
		pRangeData->RangeStatus =
				VL53L1_RANGESTATUS_RANGE_VALID;
	} else if (FilteredRangeStatus ==
			VL53L1_DEVICEERROR_RANGECOMPLETE_NO_WRAP_CHECK) {
		pRangeData->RangeStatus =
				VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK;
	} else {
		if (NoneFlag == 1) {
			pRangeData->RangeStatus = VL53L1_RANGESTATUS_NONE;
		} else if (FilteredRangeStatus ==
				VL53L1_DEVICEERROR_VCSELCONTINUITYTESTFAILURE ||
			FilteredRangeStatus ==
				VL53L1_DEVICEERROR_VCSELWATCHDOGTESTFAILURE ||
			FilteredRangeStatus ==
					VL53L1_DEVICEERROR_NOVHVVALUEFOUND) {
			pRangeData->RangeStatus =
				VL53L1_RANGESTATUS_HARDWARE_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEVICEERROR_RANGEPHASECHECK ||
				FilteredRangeStatus ==
					VL53L1_DEVICEERROR_PHASECONSISTENCY) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_PHASE_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEVICEERROR_MINCLIP ||
				FilteredRangeStatus ==
					VL53L1_DEVICEERROR_USERROICLIP) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_MIN_RANGE_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEVICEERROR_MSRCNOTARGET) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_SIGNAL_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEVICEERROR_SIGMATHRESHOLDCHECK) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_SIGMA_FAIL;
		} else {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_NONE;
		}

	}



	TempFix1616 = VL53L1_FIXPOINT142TOFIXPOINT1616(
			presults_data->VL53L1_PRM_00004);
	VL53L1_SETARRAYPARAMETERFIELD(Dev,
		LimitChecksCurrent, VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE,
		TempFix1616);

	TempFix1616 = VL53L1_FIXPOINT97TOFIXPOINT1616(
			presults_data->peak_signal_count_rate_mcps);
	VL53L1_SETARRAYPARAMETERFIELD(Dev,
		LimitChecksCurrent, VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
		TempFix1616);





	VL53L1_GetLimitCheckValue(Dev,
			VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE,
			&LimitCheckValue);

	SigmaLimitflag = (FilteredRangeStatus ==
			VL53L1_DEVICEERROR_SIGMATHRESHOLDCHECK)
			? 1 : 0;

	VL53L1_GetLimitCheckEnable(Dev,
			VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE,
			&Temp8Enable);

	Temp8 = ((Temp8Enable == 1) && (SigmaLimitflag == 1)) ? 1 : 0;
	VL53L1_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
			VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE, Temp8);



	VL53L1_GetLimitCheckValue(Dev,
			VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			&LimitCheckValue);

	SignalLimitflag = (FilteredRangeStatus ==
			VL53L1_DEVICEERROR_MSRCNOTARGET)
			? 1 : 0;

	VL53L1_GetLimitCheckEnable(Dev,
			VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			&Temp8Enable);

	Temp8 = ((Temp8Enable == 1) && (SignalLimitflag == 1)) ? 1 : 0;
	VL53L1_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
			VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, Temp8);

	return Status;
}


static uint8_t GetOutputDataIndex(VL53L1_DEV Dev,
	VL53L1_range_results_t *presults)
{
	uint8_t i;
	uint8_t index = 0;
	VL53L1_OutputModes OutputMode;

	OutputMode = PALDevDataGet(Dev, CurrentParameters.OutputMode);




	if (OutputMode == VL53L1_OUTPUTMODE_NEAREST)
		return 0;





	for (i = 1; i < presults->active_results; i++) {
		if (presults->VL53L1_PRM_00005[i].peak_signal_count_rate_mcps >
			presults->VL53L1_PRM_00005[index].peak_signal_count_rate_mcps)
			index = i;
	}

	return index;
}

VL53L1_Error VL53L1_GetRangingMeasurementData(VL53L1_DEV Dev,
	VL53L1_RangingMeasurementData_t *pRangingMeasurementData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_range_results_t       results;
	VL53L1_range_results_t       *presults = &results;
	VL53L1_range_data_t *presults_data;
	uint8_t index = 0;
	int16_t dmax_min;

	LOG_FUNCTION_START("");



	memset(pRangingMeasurementData, 0xFF,
		sizeof(VL53L1_RangingMeasurementData_t));



	Status = VL53L1_get_device_results(
			Dev,
			VL53L1_DEVICERESULTSLEVEL_FULL,
			presults);
	dmax_min = MIN(presults->wrap_dmax_mm,
			presults->ambient_dmax_mm[DMAX_REFLECTANCE_IDX]);

	if (Status == VL53L1_ERROR_NONE) {
		pRangingMeasurementData->StreamCount = presults->stream_count;
		pRangingMeasurementData->DmaxMilliMeter = dmax_min;




		index = GetOutputDataIndex(Dev, presults);
		presults_data = &(presults->VL53L1_PRM_00005[index]);
		Status = SetSimpleData(Dev, presults->active_results,
				presults_data,
				pRangingMeasurementData);
	}

	if (Status == VL53L1_ERROR_NONE) {
		CheckAndChangeDistanceMode(Dev, pRangingMeasurementData,
			presults->ambient_dmax_mm[VL53L1_MAX_AMBIENT_DMAX_VALUES
				- 1]);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

static VL53L1_Error SetMeasurementData(VL53L1_DEV Dev,
	VL53L1_range_results_t *presults,
	VL53L1_MultiRangingData_t *pMultiRangingData)
{
	uint8_t i;
	uint8_t iteration;
	VL53L1_RangingMeasurementData_t *pRangeData;
	VL53L1_range_data_t *presults_data;
	int16_t dmax_min;
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t index = 0;

	pMultiRangingData->NumberOfObjectsFound = presults->active_results;
	pMultiRangingData->RoiNumber = presults->zone_id;

	dmax_min = MIN(presults->wrap_dmax_mm,
			presults->ambient_dmax_mm[DMAX_REFLECTANCE_IDX]);




	if (presults->active_results < 1)
		iteration = 1;
	else
		iteration = presults->active_results;

	for (i = 0; i < iteration; i++) {
		pRangeData = &(pMultiRangingData->RangeData[i]);

		presults_data = &(presults->VL53L1_PRM_00005[i]);
		pRangeData->StreamCount = presults->stream_count;
		pRangeData->DmaxMilliMeter = dmax_min;

		if (Status == VL53L1_ERROR_NONE)
			Status = SetSimpleData(Dev, presults->active_results,
					presults_data,
					pRangeData);
	}

	if (Status == VL53L1_ERROR_NONE) {
		index = GetOutputDataIndex(Dev, presults);
		pRangeData = &(pMultiRangingData->RangeData[index]);
		CheckAndChangeDistanceMode(Dev, pRangeData,
			presults->ambient_dmax_mm[
			VL53L1_MAX_AMBIENT_DMAX_VALUES - 1]);
	}

	return Status;
}

VL53L1_Error VL53L1_GetMultiRangingData(VL53L1_DEV Dev,
		VL53L1_MultiRangingData_t *pMultiRangingData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_range_results_t       results;
	VL53L1_range_results_t       *presults = &results;

	LOG_FUNCTION_START("");



	memset(pMultiRangingData, 0xFF,
		sizeof(VL53L1_MultiRangingData_t));



	Status = VL53L1_get_device_results(
				Dev,
				VL53L1_DEVICERESULTSLEVEL_FULL,
				presults);


	if (Status == VL53L1_ERROR_NONE) {

		switch (presults->rd_device_state) {
		case VL53L1_DEVICESTATE_RANGING_GATHER_DATA:
			pMultiRangingData->RoiStatus =
					VL53L1_ROISTATUS_VALID_NOT_LAST;
			break;
		case VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA:
			pMultiRangingData->RoiStatus =
					VL53L1_ROISTATUS_VALID_LAST;
			break;
		default:
			pMultiRangingData->RoiStatus =
					VL53L1_ROISTATUS_NOT_VALID;
		}

		Status = SetMeasurementData(Dev,
					presults,
					pMultiRangingData);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}









VL53L1_Error VL53L1_PerformRefSpadManagement(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_Error UnfilteredStatus;

	LOG_FUNCTION_START("");

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_run_ref_spad_char(Dev, &UnfilteredStatus);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetXTalkCompensationEnable(VL53L1_DEV Dev,
	uint8_t XTalkCompensationEnable)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (XTalkCompensationEnable == 0)
		Status = VL53L1_disable_xtalk_compensation(Dev);
	else
		Status = VL53L1_enable_xtalk_compensation(Dev);

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetXTalkCompensationEnable(VL53L1_DEV Dev,
	uint8_t *pXTalkCompensationEnable)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	VL53L1_get_xtalk_compensation_enable(
		Dev,
		pXTalkCompensationEnable);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_PerformXTalkCalibration(VL53L1_DEV Dev,
		uint8_t CalibrationOption)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_Error UnfilteredStatus;
	uint32_t mm_config_timeout_us = 2000;
	uint32_t range_config_timeout_us = 10000;
	uint16_t dss_config__target_total_rate_mcps =  0x1400;
	uint32_t phasecal_config_timeout_us = 4000;
	uint8_t num_of_samples = 7;

	SUPPRESS_UNUSED_WARNING(CalibrationOption);

	LOG_FUNCTION_START("");






	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_run_xtalk_extraction(
				Dev,
				dss_config__target_total_rate_mcps,
				phasecal_config_timeout_us,
				mm_config_timeout_us,
				range_config_timeout_us,
				num_of_samples,
				&UnfilteredStatus);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetOffsetCalibrationMode(VL53L1_DEV Dev,
		VL53L1_OffsetCalibrationModes OffsetCalibrationMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_OffsetCalibrationMode   offset_cal_mode;

	LOG_FUNCTION_START("");

	if (OffsetCalibrationMode == VL53L1_OFFSETCALIBRATIONMODE_STANDARD) {
		offset_cal_mode =
			VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD;
	} else if (OffsetCalibrationMode ==
			VL53L1_OFFSETCALIBRATIONMODE_PRERANGE_ONLY) {
		offset_cal_mode =
		VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD_PRE_RANGE_ONLY;
	} else if (OffsetCalibrationMode ==
			VL53L1_OFFSETCALIBRATIONMODE_MULTI_ZONE) {
		offset_cal_mode =
			VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM;
	} else {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	if (Status == VL53L1_ERROR_NONE)
		Status =  VL53L1_set_offset_calibration_mode(Dev,
				offset_cal_mode);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetOffsetCorrectionMode(VL53L1_DEV Dev,
		VL53L1_OffsetCorrectionModes OffsetCorrectionMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_OffsetCorrectionMode   offset_cor_mode;

	LOG_FUNCTION_START("");

	if (OffsetCorrectionMode == VL53L1_OFFSETCORRECTIONMODE_STANDARD) {
		offset_cor_mode =
				VL53L1_OFFSETCORRECTIONMODE__MM1_MM2_OFFSETS;
	} else if (OffsetCorrectionMode ==
			VL53L1_OFFSETCORRECTIONMODE_PERZONE) {
		offset_cor_mode =
				VL53L1_OFFSETCORRECTIONMODE__PER_ZONE_OFFSETS;
	} else {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	if (Status == VL53L1_ERROR_NONE)
		Status =  VL53L1_set_offset_correction_mode(Dev,
				offset_cor_mode);

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_PerformOffsetCalibration(VL53L1_DEV Dev,
	int32_t CalDistanceMilliMeter, FixPoint1616_t CalReflectancePercent)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_Error UnfilteredStatus;
	uint32_t range_config_timeout_us = 13000;
	uint8_t pre_num_of_samples = 32;
	uint8_t mm1_num_of_samples = 100;
	uint8_t mm2_num_of_samples = 64;
	uint16_t dss_config__target_total_rate_mcps = 0x1400;
	uint32_t phasecal_config_timeout_us = 1000;
	uint16_t CalReflectancePercent_int;


	VL53L1_DevicePresetModes      device_preset_mode;
	VL53L1_DeviceZonePreset       zone_preset;
	VL53L1_zone_config_t         zone_cfg;
	uint32_t phasecal_config_timeout_us_pz;
	uint32_t mm_config_timeout_us_pz;
	uint32_t range_config_timeout_us_pz;
	uint16_t num_of_samples;
	VL53L1_OffsetCalibrationMode   offset_cal_mode;

	LOG_FUNCTION_START("");

	CalReflectancePercent_int =
			VL53L1_FIXPOINT1616TOFIXPOINT72(CalReflectancePercent);

	if (Status == VL53L1_ERROR_NONE)
		Status =  VL53L1_get_offset_calibration_mode(Dev,
				&offset_cal_mode);

	if ((offset_cal_mode ==
			VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD) ||
		(offset_cal_mode ==
		VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD_PRE_RANGE_ONLY)) {

		if (Status == VL53L1_ERROR_NONE)
			Status = VL53L1_run_offset_calibration(
					Dev,
					dss_config__target_total_rate_mcps,
					phasecal_config_timeout_us,
					range_config_timeout_us,
					pre_num_of_samples,
					mm1_num_of_samples,
					mm2_num_of_samples,
					(int16_t)CalDistanceMilliMeter,
					CalReflectancePercent_int,
					&UnfilteredStatus);

	} else if (offset_cal_mode ==
			VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM) {

		device_preset_mode =
			VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_LONG_RANGE;
		zone_preset = VL53L1_DEVICEZONEPRESET_CUSTOM;
		phasecal_config_timeout_us_pz         =  4000;
		mm_config_timeout_us_pz               =  2000;
		range_config_timeout_us_pz            = 10000;
		num_of_samples                     =    64;

		Status = VL53L1_get_zone_config(Dev, &zone_cfg);

		if (Status == VL53L1_ERROR_NONE)
			Status = VL53L1_run_zone_calibration(
					Dev,
					device_preset_mode,
					zone_preset,
					&zone_cfg,
					dss_config__target_total_rate_mcps,
					phasecal_config_timeout_us_pz,
					mm_config_timeout_us_pz,
					range_config_timeout_us_pz,
					num_of_samples,
					(int16_t)CalDistanceMilliMeter,
					CalReflectancePercent_int,
					&UnfilteredStatus);

	} else {
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_SetCalibrationData(VL53L1_DEV Dev,
		VL53L1_CalibrationData_t *pCalibrationData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_set_part_to_part_data(
		Dev, pCalibrationData);

	LOG_FUNCTION_END(Status);
	return Status;

}

VL53L1_Error VL53L1_GetCalibrationData(VL53L1_DEV Dev,
		VL53L1_CalibrationData_t  *pCalibrationData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_part_to_part_data(
		Dev, pCalibrationData);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetOpticalCenter(VL53L1_DEV Dev,
		uint8_t *pOpticalCenterX, uint8_t *pOpticalCenterY)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_CalibrationData_t  CalibrationData;

	LOG_FUNCTION_START("");

	*pOpticalCenterX = 0;
	*pOpticalCenterY = 0;
	Status = VL53L1_get_part_to_part_data(Dev, &CalibrationData);
	if (Status == VL53L1_ERROR_NONE) {
		*pOpticalCenterX = CalibrationData.mm_roi.x_centre;
		*pOpticalCenterY = CalibrationData.mm_roi.y_centre;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}








VL53L1_Error VL53L1_SetThresholdConfig(VL53L1_DEV Dev,
		VL53L1_DetectionConfig_t *pConfig)
{
#define BADTHRESBOUNDS(T) \
	(((T.CrossMode == VL53L1_THRESHOLD_OUT_OF_WINDOW) || \
	(T.CrossMode == VL53L1_THRESHOLD_IN_WINDOW)) && (T.Low > T.High))

	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_GPIO_interrupt_config_t Cfg;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_GPIO_interrupt_config(Dev, &Cfg);
	if (Status == VL53L1_ERROR_NONE) {
		if (pConfig->DetectionMode == VL53L1_DETECTION_NORMAL_RUN) {
			Cfg.intr_new_measure_ready = 1;
			Status = VL53L1_set_GPIO_interrupt_config_struct(Dev,
					Cfg);
		} else {
			if (BADTHRESBOUNDS(pConfig->Distance))
				Status = VL53L1_ERROR_INVALID_PARAMS;
			if ((Status == VL53L1_ERROR_NONE) &&
					(BADTHRESBOUNDS(pConfig->Rate)))
				Status = VL53L1_ERROR_INVALID_PARAMS;
			if (Status == VL53L1_ERROR_NONE) {
				Cfg.intr_new_measure_ready = 0;
				Cfg.intr_no_target = pConfig->IntrNoTarget;
				Cfg.threshold_distance_high =
						pConfig->Distance.High;
				Cfg.threshold_distance_low =
						pConfig->Distance.Low;
				Cfg.threshold_rate_high =
					VL53L1_FIXPOINT1616TOFIXPOINT97(
							pConfig->Rate.High);
				Cfg.threshold_rate_low =
					VL53L1_FIXPOINT1616TOFIXPOINT97(
							pConfig->Rate.Low);

				Cfg.intr_mode_distance = ConvertModeToLLD(
						&Status,
						pConfig->Distance.CrossMode);
				if (Status == VL53L1_ERROR_NONE)
					Cfg.intr_mode_rate = ConvertModeToLLD(
						&Status,
						pConfig->Rate.CrossMode);
			}



			if (Status == VL53L1_ERROR_NONE) {
				Cfg.intr_combined_mode = 1;
				switch (pConfig->DetectionMode) {
				case VL53L1_DETECTION_DISTANCE_ONLY:
					Cfg.threshold_rate_high = 0;
					Cfg.threshold_rate_low = 0;
					break;
				case VL53L1_DETECTION_RATE_ONLY:
					Cfg.threshold_distance_high = 0;
					Cfg.threshold_distance_low = 0;
					break;
				case VL53L1_DETECTION_DISTANCE_OR_RATE:




					break;
				case VL53L1_DETECTION_DISTANCE_AND_RATE:
					Cfg.intr_combined_mode = 0;
					break;
				default:
					Status = VL53L1_ERROR_INVALID_PARAMS;
				}
			}

			if (Status == VL53L1_ERROR_NONE)
				Status =
				VL53L1_set_GPIO_interrupt_config_struct(Dev,
						Cfg);

		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L1_Error VL53L1_GetThresholdConfig(VL53L1_DEV Dev,
		VL53L1_DetectionConfig_t *pConfig)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_GPIO_interrupt_config_t Cfg;

	LOG_FUNCTION_START("");

	Status = VL53L1_get_GPIO_interrupt_config(Dev, &Cfg);

	if (Status != VL53L1_ERROR_NONE) {
		LOG_FUNCTION_END(Status);
		return Status;
	}

	pConfig->IntrNoTarget = Cfg.intr_no_target;
	pConfig->Distance.High = Cfg.threshold_distance_high;
	pConfig->Distance.Low = Cfg.threshold_distance_low;
	pConfig->Rate.High =
		VL53L1_FIXPOINT97TOFIXPOINT1616(
				Cfg.threshold_rate_high);
	pConfig->Rate.Low =
		VL53L1_FIXPOINT97TOFIXPOINT1616(Cfg.threshold_rate_low);
	pConfig->Distance.CrossMode =
		ConvertModeFromLLD(&Status, Cfg.intr_mode_distance);
	if (Status == VL53L1_ERROR_NONE)
		pConfig->Rate.CrossMode =
			ConvertModeFromLLD(&Status, Cfg.intr_mode_rate);

	if (Cfg.intr_new_measure_ready == 1) {
		pConfig->DetectionMode = VL53L1_DETECTION_NORMAL_RUN;
	} else {


		if (Status == VL53L1_ERROR_NONE) {
			if (Cfg.intr_combined_mode == 0)
				pConfig->DetectionMode =
				VL53L1_DETECTION_DISTANCE_AND_RATE;
			else {
				if ((Cfg.threshold_distance_high == 0) &&
					(Cfg.threshold_distance_low == 0))
					pConfig->DetectionMode =
					VL53L1_DETECTION_RATE_ONLY;
				else if ((Cfg.threshold_rate_high == 0) &&
					(Cfg.threshold_rate_low == 0))
					pConfig->DetectionMode =
					VL53L1_DETECTION_DISTANCE_ONLY;
				else
					pConfig->DetectionMode =
					VL53L1_DETECTION_DISTANCE_OR_RATE;
			}
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}





