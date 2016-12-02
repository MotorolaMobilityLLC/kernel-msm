
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













static VL53L1_Error CheckValidRectRoi(VL53L1_UserRoi_t ROI)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	LOG_FUNCTION_START("");



	if ((ROI.TopLeftX > 15) || (ROI.TopLeftY > 15) ||
		(ROI.BotRightX > 15) || (ROI.BotRightY > 15) )
	{
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	if ((ROI.TopLeftX > ROI.BotRightX) || (ROI.TopLeftY < ROI.BotRightY))
	{
		Status = VL53L1_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

static uint8_t CheckLiteMode(VL53L1_DEV Dev)
{
	uint8_t PresetMode = 0;

	PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);

	if (PresetMode == VL53L1_PRESETMODE_LITE_RANGING)
		return 1;
	else
		return 0;
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
	revision_id = pLLData->VL53L1_PRM_00002.VL53L1_PRM_00003;
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
			pLLData->VL53L1_PRM_00002.VL53L1_PRM_00004;

	revision_id = pLLData->VL53L1_PRM_00002.VL53L1_PRM_00003;
	pVL53L1_DeviceInfo->ProductRevisionMajor = 1;
	pVL53L1_DeviceInfo->ProductRevisionMinor =(revision_id & 0xF0) >> 4;

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
	pVL53L1_DeviceInfo->Name[0]=0;
	pVL53L1_DeviceInfo->Type[0]=0;
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

	Status = VL53L1_WrByte(Dev, VL53L1_DEF_00001,
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
		Status = VL53L1_FCTN_00001(Dev, 1);

	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev, PalState, VL53L1_STATE_WAIT_STATICINIT);
		PALDevDataSet(Dev, CurrentParameters.PresetMode,
				VL53L1_PRESETMODE_LITE_RANGING);
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
	uint8_t  VL53L1_PRM_00005;
	LOG_FUNCTION_START("");

	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev, PalState, VL53L1_STATE_IDLE);
	}

	VL53L1_PRM_00005  = VL53L1_DEF_00002;
	PALDevDataSet(Dev, LLData.VL53L1_PRM_00005, VL53L1_PRM_00005);


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_WaitDeviceBooted(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	LOG_FUNCTION_START("");

	Status = VL53L1_FCTN_00002(Dev,
			VL53L1_BOOT_COMPLETION_POLLING_TIMEOUT_MS);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_ResetDevice(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	LOG_FUNCTION_START("");

	Status = VL53L1_FCTN_00003(Dev);

	if (Status == VL53L1_ERROR_NONE) {
		PALDevDataSet(Dev, PalState, VL53L1_STATE_RESET);
	}


	LOG_FUNCTION_END(Status);
	return Status;
}






VL53L1_Error VL53L1_SetPresetMode(VL53L1_DEV Dev, VL53L1_PresetModes PresetMode)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_DevicePresetModes   device_preset_mode;
	uint32_t                   VL53L1_PRM_00006;
	uint32_t                   VL53L1_PRM_00007;
	uint32_t                   VL53L1_PRM_00008;

	device_preset_mode = VL53L1_DEF_00003;
	VL53L1_PRM_00006 = 2000;
	VL53L1_PRM_00007 = 16000;
	VL53L1_PRM_00008 = 10;

	LOG_FUNCTION_START("%d", (int)PresetMode);

	switch (PresetMode) {
	case VL53L1_PRESETMODE_LITE_RANGING:
		device_preset_mode = VL53L1_DEF_00003;
		break;
	case VL53L1_PRESETMODE_RANGING:
		device_preset_mode = VL53L1_DEF_00004;
		break;
	case VL53L1_PRESETMODE_MULTIZONES_SCANNING:
		device_preset_mode =
				VL53L1_DEF_00005;
		break;
	case VL53L1_PRESETMODE_AUTONOMOUS:
		device_preset_mode = VL53L1_DEF_00006;
		break;
	case VL53L1_PRESETMODE_OLT:
		device_preset_mode = VL53L1_DEF_00007;
		break;
	default:


		Status = VL53L1_ERROR_MODE_NOT_SUPPORTED;
	}


	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00004(
				Dev,
				device_preset_mode,
				VL53L1_PRM_00006,
				VL53L1_PRM_00007,
				VL53L1_PRM_00008);

	if (Status == VL53L1_ERROR_NONE)
		PALDevDataSet(Dev, CurrentParameters.PresetMode, PresetMode);

	if (Status == VL53L1_ERROR_NONE) {


		Status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(Dev,
				16000);
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
	uint16_t  FastOscfrequency;
	VL53L1_timing_config_t *ptiming;
	VL53L1_PresetModes PresetMode;

	LOG_FUNCTION_START("");

	ptiming = &(PALDevDataGet(Dev, LLData.VL53L1_PRM_00009));



	if (MeasurementTimingBudgetMicroSeconds > 10000000)
		Status = VL53L1_ERROR_INVALID_PARAMS;

	if (Status == VL53L1_ERROR_NONE){
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM1, &Mm1Enabled);
	}

	if (Status == VL53L1_ERROR_NONE){
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM2, &Mm2Enabled);
	}

	if (Status == VL53L1_ERROR_NONE){
		MmTimeoutUs =  PALDevDataGet(Dev, LLData.VL53L1_PRM_00006);
		FastOscfrequency =  PALDevDataGet(Dev,
			LLData.VL53L1_PRM_00010.VL53L1_PRM_00011);
		PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);

		TimingGuard = 0;
		divisor = 1;
		switch (PresetMode) {
			case VL53L1_PRESETMODE_LITE_RANGING:
				if ((Mm1Enabled == 1) || (Mm2Enabled == 1)) {
					TimingGuard = 5000;
				} else {
					TimingGuard = 1000;
				}
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
			TimingBudget =(MeasurementTimingBudgetMicroSeconds
					- TimingGuard) / divisor;
			PALDevDataSet(Dev, LLData.VL53L1_PRM_00007,
					TimingBudget);
			VL53L1_FCTN_00005(
				MmTimeoutUs, TimingBudget, FastOscfrequency,
				ptiming);

		}
		else {
			Status = VL53L1_ERROR_INVALID_PARAMS;
		}
	}
	if (Status == VL53L1_ERROR_NONE){
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
	VL53L1_PresetModes PresetMode;
	LOG_FUNCTION_START("");

	*pMeasurementTimingBudgetMicroSeconds = 0;

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM1, &Mm1Enabled);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_GetSequenceStepEnable(Dev,
			VL53L1_SEQUENCESTEP_MM2, &Mm2Enabled);

	MmTimeoutUs =  PALDevDataGet(Dev, LLData.VL53L1_PRM_00006);
	RangeTimeoutUs =  PALDevDataGet(Dev, LLData.VL53L1_PRM_00007);

	if (Status == VL53L1_ERROR_NONE) {
		PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);

		switch (PresetMode) {
		case VL53L1_PRESETMODE_LITE_RANGING:
			if ((Mm1Enabled == 1) || (Mm2Enabled == 1)) {
				MeasTimingBdg = RangeTimeoutUs + 5000;
			} else {
				MeasTimingBdg = RangeTimeoutUs + 1000;
			}
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

	Status = VL53L1_FCTN_00006(Dev,
			InterMeasurementPeriodMilliSeconds);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetInterMeasurementPeriodMilliSeconds(VL53L1_DEV Dev,
	uint32_t *pInterMeasurementPeriodMilliSeconds)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L1_FCTN_00007(Dev,
			pInterMeasurementPeriodMilliSeconds);

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
		LLData.VL53L1_PRM_00009.VL53L1_PRM_00012));

	switch (LimitCheckId) {
	case VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE:
		tmpuint16 = VL53L1_FIXPOINT1616TOFIXPOINT142(value);
		PALDevDataSet(Dev, LLData.VL53L1_PRM_00009.VL53L1_PRM_00013,
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

	SUPPRESS_UNUSED_WARNING(Dev);
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
		LLData.VL53L1_PRM_00009.VL53L1_PRM_00012);

	switch (LimitCheckId) {
	case VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE:
		SigmaThresh = PALDevDataGet(Dev,
			LLData.VL53L1_PRM_00009.VL53L1_PRM_00013);
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
	VL53L1_zone_config_t  VL53L1_PRM_00014;
	VL53L1_UserRoi_t CurrROI;
	uint8_t  i;
	uint8_t  VL53L1_PRM_00015;
	uint8_t  VL53L1_PRM_00016;
	uint8_t  VL53L1_PRM_00017, VL53L1_PRM_00018;

	LOG_FUNCTION_START("");



	PresetMode = PALDevDataGet(Dev, CurrentParameters.PresetMode);



	if (PresetMode == VL53L1_PRESETMODE_MULTIZONES_SCANNING)
		MaxNumberOfROI = VL53L1_MAX_USER_ZONES;
	else
		MaxNumberOfROI = 1;

	if ((pRoiConfig->NumberOfRoi > MaxNumberOfROI) ||
			(pRoiConfig->NumberOfRoi < 1))
		Status = VL53L1_ERROR_INVALID_PARAMS;

	if (Status == VL53L1_ERROR_NONE)
	{


		VL53L1_PRM_00014.VL53L1_PRM_00019 = MaxNumberOfROI;
		VL53L1_PRM_00014.VL53L1_PRM_00020 = pRoiConfig->NumberOfRoi - 1;
		for (i=0; i<pRoiConfig->NumberOfRoi; i++)
		{
			CurrROI = pRoiConfig->UserRois[i];









			Status = CheckValidRectRoi(CurrROI);
			if (Status != VL53L1_ERROR_NONE) { break;}
			VL53L1_PRM_00015 = (CurrROI.BotRightX + CurrROI.TopLeftX  + 1)
					/ 2;
			VL53L1_PRM_00016 = (CurrROI.TopLeftY  + CurrROI.BotRightY + 1)
					/ 2;
			VL53L1_PRM_00017 =     (CurrROI.BotRightX - CurrROI.TopLeftX);
			VL53L1_PRM_00018 =    (CurrROI.TopLeftY  - CurrROI.BotRightY);
			if ((VL53L1_PRM_00017 < 3) || (VL53L1_PRM_00018 < 3))
			{
				Status = VL53L1_ERROR_INVALID_PARAMS;
				break;
			}
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00015 = VL53L1_PRM_00015;
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00016 = VL53L1_PRM_00016;
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00017 = VL53L1_PRM_00017;
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00018 = VL53L1_PRM_00018;
		}
	}

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00008(Dev, &VL53L1_PRM_00014);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_GetROI(VL53L1_DEV Dev,
		VL53L1_RoiConfig_t *pRoiConfig)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_zone_config_t      VL53L1_PRM_00014;
	uint8_t  i;
	uint8_t  TopLeftX;
	uint8_t  TopLeftY;
	uint8_t  BotRightX;
	uint8_t  BotRightY;
	LOG_FUNCTION_START("");

	VL53L1_FCTN_00009(Dev, &VL53L1_PRM_00014);

	pRoiConfig->NumberOfRoi = VL53L1_PRM_00014.VL53L1_PRM_00020 + 1;

	for (i=0; i<pRoiConfig->NumberOfRoi; i++) {
		TopLeftX = (2 * VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00015 -
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00017) >> 1;
		TopLeftY = (2 * VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00016 +
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00018) >> 1;
		BotRightX = (2 * VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00015 +
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00017) >> 1;
		BotRightY = (2 * VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00016 -
			VL53L1_PRM_00014.VL53L1_PRM_00021[i].VL53L1_PRM_00018) >> 1;
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
			LLData.VL53L1_PRM_00022.VL53L1_PRM_00023);

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
		PALDevDataSet(Dev, LLData.VL53L1_PRM_00022.VL53L1_PRM_00023,
				SequenceConfigNew);



		MeasurementTimingBudgetMicroSeconds =PALDevDataGet(Dev,
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
			LLData.VL53L1_PRM_00022.VL53L1_PRM_00023);

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











VL53L1_Error VL53L1_PerformXTalkCalibration(VL53L1_DEV Dev,
		uint8_t CalibrationOption)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint32_t                   VL53L1_PRM_00006;
	uint32_t                   VL53L1_PRM_00007;
	SUPPRESS_UNUSED_WARNING(CalibrationOption);

	LOG_FUNCTION_START("");






	VL53L1_PRM_00006 = 2000;
	VL53L1_PRM_00007 = 10000;

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00010(
			Dev,
			VL53L1_PRM_00006,
			VL53L1_PRM_00007,
			0x01);


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_PerformOffsetCalibration(VL53L1_DEV Dev,
	int32_t CalDistanceMilliMeter)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint32_t                   VL53L1_PRM_00007;
	uint8_t        num_of_samples = 32;
	LOG_FUNCTION_START("");

	VL53L1_PRM_00007 = 10000;

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00011(
				Dev,
				VL53L1_PRM_00007,
				num_of_samples,
				CalDistanceMilliMeter);

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

	DeviceMeasurementMode = PALDevDataGet(Dev, LLData.VL53L1_PRM_00005);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00012(
				Dev,
				DeviceMeasurementMode,
				VL53L1_DEF_00008);

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

	Status = VL53L1_FCTN_00013(Dev);

	if (Status == VL53L1_ERROR_NONE) {


		PALDevDataSet(Dev, PalState, VL53L1_STATE_IDLE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_ClearInterruptAndStartMeasurement(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	uint8_t DeviceMeasurementMode;
	LOG_FUNCTION_START("");

	DeviceMeasurementMode = PALDevDataGet(Dev, LLData.VL53L1_PRM_00005);

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00014(
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

	Status = VL53L1_FCTN_00015(Dev, pMeasurementDataReady);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L1_Error VL53L1_WaitMeasurementDataReady(VL53L1_DEV Dev)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	LOG_FUNCTION_START("");






	Status = VL53L1_FCTN_00016(Dev,
			VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS);

	LOG_FUNCTION_END(Status);
	return Status;
}

static void SetSimpleData(VL53L1_DEV Dev,
	VL53L1_range_data_t *presults_data,
	VL53L1_RangingMeasurementData_t *pRangeData)
{
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
	uint8_t StreamCount;

	pRangeData->TimeStamp = presults_data->VL53L1_PRM_00024;
	FilteredRangeStatus = presults_data->VL53L1_PRM_00025 & 0x1F;

	StreamCount = PALDevDataGet(Dev,
		LLData.VL53L1_PRM_00027.VL53L1_PRM_00026);
	pRangeData->StreamCount = StreamCount;

	pRangeData->ConfidenceLevel = 0;
	pRangeData->RangeMaxMilliMeter = presults_data->VL53L1_PRM_00028;
	pRangeData->RangeMinMilliMeter = presults_data->VL53L1_PRM_00029;

	SignalRate = VL53L1_FIXPOINT97TOFIXPOINT1616(
		presults_data->VL53L1_PRM_00030);
	pRangeData->SignalRateRtnMegaCps
		= SignalRate;

	AmbientRate = VL53L1_FIXPOINT97TOFIXPOINT1616(
		presults_data->VL53L1_PRM_00031);
	pRangeData->AmbientRateRtnMegaCps = AmbientRate;

	pRangeData->EffectiveSpadRtnCount =
		presults_data->VL53L1_PRM_00032;

	TempFix1616 = VL53L1_FIXPOINT142TOFIXPOINT1616(
			presults_data->VL53L1_PRM_00033);

	pRangeData->SigmaMilliMeter = TempFix1616;

	pRangeData->RangeMilliMeter = presults_data->VL53L1_PRM_00034;

	pRangeData->RangeFractionalPart = 0;

	if (FilteredRangeStatus ==
			VL53L1_DEF_00009||
		FilteredRangeStatus ==
				VL53L1_DEF_00010 ||
		FilteredRangeStatus ==
				VL53L1_DEF_00011 ||
		FilteredRangeStatus ==
				VL53L1_DEF_00012 ||
		FilteredRangeStatus ==
				VL53L1_DEF_00013 ||
		FilteredRangeStatus ==
				VL53L1_DEF_00014 ||
		FilteredRangeStatus ==
				VL53L1_DEF_00015 ||
		FilteredRangeStatus == VL53L1_DEF_00016 ||
		FilteredRangeStatus == VL53L1_DEF_00017
			) {
		NoneFlag = 1;
	} else {
		NoneFlag = 0;
	}

	if (FilteredRangeStatus ==
			VL53L1_DEF_00018)
		pRangeData->RangeStatus =
				VL53L1_RANGESTATUS_RANGE_VALID;
	else {
		if (NoneFlag == 1) {
			pRangeData->RangeStatus = VL53L1_RANGESTATUS_NONE;
		} else if (FilteredRangeStatus ==
				VL53L1_DEF_00019 ||
			FilteredRangeStatus ==
				VL53L1_DEF_00020 ||
			FilteredRangeStatus ==
					VL53L1_DEF_00021) {
			pRangeData->RangeStatus =
				VL53L1_RANGESTATUS_HARDWARE_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEF_00022 ||
				FilteredRangeStatus ==
					VL53L1_DEF_00023) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_PHASE_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEF_00024 ||
				FilteredRangeStatus ==
					VL53L1_DEF_00025) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_MIN_RANGE_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEF_00026) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_SIGNAL_FAIL;
		} else if (FilteredRangeStatus ==
				VL53L1_DEF_00027) {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_SIGMA_FAIL;
		} else {
			pRangeData->RangeStatus =
					VL53L1_RANGESTATUS_NONE;
		}

	}



	TempFix1616 = VL53L1_FIXPOINT142TOFIXPOINT1616(
			presults_data->VL53L1_PRM_00033);
	VL53L1_SETARRAYPARAMETERFIELD(Dev,
		LimitChecksCurrent, VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE,
		TempFix1616);

	TempFix1616 = VL53L1_FIXPOINT97TOFIXPOINT1616(
			presults_data->VL53L1_PRM_00030);
	VL53L1_SETARRAYPARAMETERFIELD(Dev,
		LimitChecksCurrent, VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
		TempFix1616);





	VL53L1_GetLimitCheckValue(Dev,
			VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE,
			&LimitCheckValue);

	SigmaLimitflag = (FilteredRangeStatus ==
			VL53L1_DEF_00027)
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
			VL53L1_DEF_00026)
			? 1 : 0;

	VL53L1_GetLimitCheckEnable(Dev,
			VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
			&Temp8Enable);

	Temp8 = ((Temp8Enable == 1) && (SignalLimitflag == 1)) ? 1 : 0;
	VL53L1_SETARRAYPARAMETERFIELD(Dev, LimitChecksStatus,
			VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, Temp8);

}

VL53L1_Error VL53L1_GetRangingMeasurementData(VL53L1_DEV Dev,
	VL53L1_RangingMeasurementData_t *pRangingMeasurementData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_range_results_t       results;
	VL53L1_range_results_t       *presults = &results;
	uint8_t LiteModeUsed;
	VL53L1_range_data_t *presults_data;
	LOG_FUNCTION_START("");

	LiteModeUsed = CheckLiteMode(Dev);

	if (LiteModeUsed != 1)
		Status = VL53L1_ERROR_NOT_SUPPORTED;

	if (Status == VL53L1_ERROR_NONE) {


		memset(pRangingMeasurementData, 0x0,
			sizeof(VL53L1_RangingMeasurementData_t) );



		Status = VL53L1_FCTN_00017(
				Dev,
				VL53L1_DEF_00028,
				presults);
	}

	if (Status == VL53L1_ERROR_NONE) {
		presults_data = &(presults->VL53L1_PRM_00035[0]);
		SetSimpleData(Dev, presults_data, pRangingMeasurementData);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

static void SetMeasurementData(VL53L1_DEV Dev,
	VL53L1_range_results_t *presults,
	VL53L1_MultiRangingData_t *pMultiRangingData)
{
	uint8_t LiteModeUsed;
	uint8_t i;
	uint8_t iteration;
	uint8_t FilteredRangeStatus;
	VL53L1_RangingMeasurementData_t *pRangeData;
	VL53L1_range_data_t *presults_data;

	LiteModeUsed = CheckLiteMode(Dev);
	FilteredRangeStatus = presults->VL53L1_PRM_00035[0].VL53L1_PRM_00025 & 0x1F;

	pMultiRangingData->RoiNumber = presults->VL53L1_PRM_00035[0].VL53L1_PRM_00036;
	if (FilteredRangeStatus != VL53L1_DEF_00018)
		pMultiRangingData->NumberOfObjectsFound = 0;
	else
		pMultiRangingData->NumberOfObjectsFound
			= ((LiteModeUsed == 1) ?  1 :
			presults->VL53L1_PRM_00037);


	if ((FilteredRangeStatus != VL53L1_DEF_00018) ||
			  (LiteModeUsed == 1))
		iteration = 1;
	else
		iteration = presults->VL53L1_PRM_00037;

	for (i=0; i<iteration; i++) {
		pRangeData = &(pMultiRangingData->RangeData[i]);

		presults_data = &(presults->VL53L1_PRM_00035[i]);

		SetSimpleData(Dev, presults_data, pRangeData);

	}

}

VL53L1_Error Vl53L1_GetMultiRangingData(VL53L1_DEV Dev,
		VL53L1_MultiRangingData_t *pMultiRangingData)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	VL53L1_range_results_t       results;
	VL53L1_range_results_t       *presults = &results;

	LOG_FUNCTION_START("");



	memset(pMultiRangingData, 0x0,
		sizeof(VL53L1_MultiRangingData_t) );

	memset(&results, 0x0, sizeof(VL53L1_range_results_t));



	Status = VL53L1_FCTN_00017(
				Dev,
				VL53L1_DEF_00028,
				presults);


	if (Status == VL53L1_ERROR_NONE) {

		switch (presults->VL53L1_PRM_00038) {
		case VL53L1_DEF_00029:
			pMultiRangingData->RoiStatus =
					VL53L1_ROISTATUS_VALID_NOT_LAST;
			break;
		case VL53L1_DEF_00030:
			pMultiRangingData->RoiStatus =
					VL53L1_ROISTATUS_VALID_LAST;
			break;
		default:
			pMultiRangingData->RoiStatus =
					VL53L1_ROISTATUS_NOT_VALID;
		}

		SetMeasurementData(Dev,
				presults,
				pMultiRangingData);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}









VL53L1_Error VL53L1_PerformRefSpadManagement(VL53L1_DEV Dev,
		uint8_t *pRefSpadCount, uint8_t *pRefSpadLocation)
{
	VL53L1_Error Status = VL53L1_ERROR_NONE;
	LOG_FUNCTION_START("");

	if (Status == VL53L1_ERROR_NONE)
		Status = VL53L1_FCTN_00018(Dev);

	*pRefSpadCount = 0;
	*pRefSpadLocation = 0;

	LOG_FUNCTION_END(Status);
	return Status;
}







