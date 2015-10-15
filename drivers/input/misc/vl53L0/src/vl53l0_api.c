/*******************************************************************************
 Copyright © 2015, STMicroelectronics International N.V.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of STMicroelectronics nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
 IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include "vl53l0_api.h"
#include "vl53l0_tuning.h"
#ifndef __KERNEL__
#include <stdlib.h>
#endif

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

#ifdef VL53L0_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(TRACE_MODULE_API, \
	level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#endif


/* Defines */
#define VL53L0_SETPARAMETERFIELD(Dev, field, value) \
	if (Status == VL53L0_ERROR_NONE) { \
		CurrentParameters = PALDevDataGet(Dev, CurrentParameters); \
		CurrentParameters.field = value; \
		CurrentParameters =	\
		PALDevDataSet(Dev, CurrentParameters, \
		CurrentParameters); }
#define VL53L0_SETARRAYPARAMETERFIELD(Dev, field, index, value) \
	if (Status == VL53L0_ERROR_NONE) { \
		CurrentParameters = PALDevDataGet(Dev, CurrentParameters); \
		CurrentParameters.field[index] = value; \
		CurrentParameters = \
			PALDevDataSet(Dev, CurrentParameters, \
		CurrentParameters); }

#define VL53L0_GETPARAMETERFIELD(Dev, field, variable) \
	if (Status == VL53L0_ERROR_NONE) { \
		CurrentParameters = PALDevDataGet(Dev, CurrentParameters); \
		variable = CurrentParameters.field; }

#define VL53L0_GETARRAYPARAMETERFIELD(Dev, field, index, variable) \
	if (Status == VL53L0_ERROR_NONE) { \
		CurrentParameters = PALDevDataGet(Dev, CurrentParameters); \
		variable = CurrentParameters.field[index]; }

#define VL53L0_SETDEVICESPECIFICPARAMETER(Dev, field, value) \
	if (Status == VL53L0_ERROR_NONE) { \
		DeviceSpecificParameters = \
			PALDevDataGet(Dev, DeviceSpecificParameters); \
		DeviceSpecificParameters.field = value; \
		DeviceSpecificParameters = \
			PALDevDataSet(Dev, \
			DeviceSpecificParameters, \
			DeviceSpecificParameters); }

#define VL53L0_GETDEVICESPECIFICPARAMETER(Dev, field) \
		PALDevDataGet(Dev, DeviceSpecificParameters).field

#define VL53L0_FIXPOINT1616TOFIXPOINT97(Value) \
			(uint16_t)((Value >> 9) & 0xFFFF)
#define VL53L0_FIXPOINT97TOFIXPOINT1616(Value) \
			(FixPoint1616_t)(Value << 9)
#define VL53L0_FIXPOINT1616TOFIXPOINT412(Value) \
			(uint16_t)((Value >> 4) & 0xFFFF)
#define VL53L0_FIXPOINT412TOFIXPOINT1616(Value) \
			(FixPoint1616_t)(Value << 4)
#define VL53L0_FIXPOINT1616TOFIXPOINT08(Value) \
			(uint8_t)((Value >> 8) & 0x00FF)
#define VL53L0_FIXPOINT08TOFIXPOINT1616(Value) \
			(FixPoint1616_t)(Value << 8)
#define VL53L0_MAKEUINT16(lsb, msb) \
			(uint16_t)((((uint16_t)msb) << 8) + (uint16_t)lsb)

/* Internal functions declaration */
static VL53L0_Error VL53L0_get_vcsel_pulse_period(VL53L0_DEV Dev,
				uint8_t *pVCSELPulsePeriod, uint8_t RangeIndex);
static uint8_t VL53L0_encode_vcsel_period(uint8_t vcsel_period_pclks);
static uint8_t VL53L0_decode_vcsel_period(uint8_t vcsel_period_reg);
static uint16_t VL53L0_calc_encoded_timeout(VL53L0_DEV Dev,
			uint32_t timeout_period_us, uint8_t vcsel_period);
static uint32_t VL53L0_calc_ranging_wait_us(VL53L0_DEV Dev,
			uint16_t timeout_overall_periods, uint8_t vcsel_period);
static VL53L0_Error VL53L0_load_additional_settings1(VL53L0_DEV Dev);
static VL53L0_Error VL53L0_load_additional_settings3(VL53L0_DEV Dev);
static VL53L0_Error VL53L0_check_part_used(VL53L0_DEV Dev, uint8_t *Revision);
static VL53L0_Error VL53L0_get_info_from_device(VL53L0_DEV Dev,
				uint8_t *Revision);
static VL53L0_Error VL53L0_get_pal_range_status(VL53L0_DEV Dev,
				uint8_t DeviceRangeStatus,
				FixPoint1616_t SignalRate,
				FixPoint1616_t CrosstalkCompensation,
				uint16_t EffectiveSpadRtnCount,
				VL53L0_RangingMeasurementData_t
					*pRangingMeasurementData,
				uint8_t *pPalRangeStatus);

/* Group PAL General Functions */
VL53L0_Error VL53L0_GetVersion(VL53L0_Version_t *pVersion)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	pVersion->major = VL53L0_IMPLEMENTATION_VER_MAJOR;
	pVersion->minor = VL53L0_IMPLEMENTATION_VER_MINOR;
	pVersion->build = VL53L0_IMPLEMENTATION_VER_SUB;

	pVersion->revision = VL53L0_IMPLEMENTATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetPalSpecVersion(VL53L0_Version_t *pPalSpecVersion)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	pPalSpecVersion->major = VL53L0_SPECIFICATION_VER_MAJOR;
	pPalSpecVersion->minor = VL53L0_SPECIFICATION_VER_MINOR;
	pPalSpecVersion->build = VL53L0_SPECIFICATION_VER_SUB;

	pPalSpecVersion->revision = VL53L0_SPECIFICATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetDeviceInfo(VL53L0_DEV Dev,
				VL53L0_DeviceInfo_t *pVL53L0_DeviceInfo)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t model_id;
	uint8_t Revision;
	LOG_FUNCTION_START("");

	Status = VL53L0_check_part_used(Dev, &Revision);

	if (Status == VL53L0_ERROR_NONE) {
		if (Revision == 0) {
			VL53L0_COPYSTRING(pVL53L0_DeviceInfo->Name,
				VL53L0_STRING_DEVICE_INFO_NAME_TS0);
		} else if ((Revision <= 34) && (Revision != 32)) {
			VL53L0_COPYSTRING(pVL53L0_DeviceInfo->Name,
				VL53L0_STRING_DEVICE_INFO_NAME_TS1);
		} else if (Revision < 39) {
			VL53L0_COPYSTRING(pVL53L0_DeviceInfo->Name,
				VL53L0_STRING_DEVICE_INFO_NAME_TS2);
		} else {
			VL53L0_COPYSTRING(pVL53L0_DeviceInfo->Name,
				VL53L0_STRING_DEVICE_INFO_NAME_ES1);
		}

		VL53L0_COPYSTRING(pVL53L0_DeviceInfo->Type,
			VL53L0_STRING_DEVICE_INFO_TYPE);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev, VL53L0_REG_IDENTIFICATION_MODEL_ID,
					&pVL53L0_DeviceInfo->ProductType);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev, VL53L0_REG_IDENTIFICATION_MODEL_ID,
					&model_id);
		pVL53L0_DeviceInfo->ProductRevisionMajor =
			(model_id & 0xE0) >> 5;
		pVL53L0_DeviceInfo->ProductRevisionMinor =
			(model_id + 9) & 0x1F;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetDeviceErrorStatus(VL53L0_DEV Dev,
				VL53L0_DeviceError *pDeviceErrorStatus)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t RangeStatus;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_RESULT_RANGE_STATUS,
		&RangeStatus);

	*pDeviceErrorStatus = (VL53L0_DeviceError)((RangeStatus & 0x78) >> 3);

	LOG_FUNCTION_END(Status);
	return Status;
}

#define VL53L0_BUILDSTATUSERRORSTRING(BUFFER, ERRORCODE, STRINGVALUE) \
		do {\
			case ERRORCODE: \
				VL53L0_COPYSTRING(BUFFER, STRINGVALUE);\
				break;\
		} while (0)

VL53L0_Error VL53L0_GetDeviceErrorString(VL53L0_DeviceError ErrorCode,
				char *pDeviceErrorString)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	switch (ErrorCode) {
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_NONE,
			VL53L0_STRING_DEVICEERROR_NONE);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_VCSELCONTINUITYTESTFAILURE,
			VL53L0_STRING_DEVICEERROR_VCSELCONTINUITYTESTFAILURE);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_VCSELWATCHDOGTESTFAILURE,
			VL53L0_STRING_DEVICEERROR_VCSELWATCHDOGTESTFAILURE);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_NOVHVVALUEFOUND,
			VL53L0_STRING_DEVICEERROR_NOVHVVALUEFOUND);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_MSRCNOTARGET,
			VL53L0_STRING_DEVICEERROR_MSRCNOTARGET);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_MSRCMINIMUMSNR,
			VL53L0_STRING_DEVICEERROR_MSRCMINIMUMSNR);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_MSRCWRAPAROUND,
			VL53L0_STRING_DEVICEERROR_MSRCWRAPAROUND);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_TCC, VL53L0_STRING_DEVICEERROR_TCC);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_RANGEAWRAPAROUND,
			VL53L0_STRING_DEVICEERROR_RANGEAWRAPAROUND);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_RANGEBWRAPAROUND,
			VL53L0_STRING_DEVICEERROR_RANGEBWRAPAROUND);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_MINCLIP,
			VL53L0_STRING_DEVICEERROR_MINCLIP);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_RANGECOMPLETE,
			VL53L0_STRING_DEVICEERROR_RANGECOMPLETE);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_ALGOUNDERFLOW,
			VL53L0_STRING_DEVICEERROR_ALGOUNDERFLOW);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_ALGOOVERFLOW,
			VL53L0_STRING_DEVICEERROR_ALGOOVERFLOW);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_FINALSNRLIMIT,
			VL53L0_STRING_DEVICEERROR_FINALSNRLIMIT);
	VL53L0_BUILDSTATUSERRORSTRING(pDeviceErrorString,
			VL53L0_DEVICEERROR_NOTARGETIGNORE,
			VL53L0_STRING_DEVICEERROR_NOTARGETIGNORE);
	default:
		VL53L0_COPYSTRING(pDeviceErrorString,
			VL53L0_STRING_UNKNOW_ERROR_CODE);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetPalErrorString(VL53L0_Error PalErrorCode,
				char *pPalErrorString)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	switch (PalErrorCode) {
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_NONE, VL53L0_STRING_ERROR_NONE);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_CALIBRATION_WARNING,
			VL53L0_STRING_ERROR_CALIBRATION_WARNING);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_MIN_CLIPPED,
			VL53L0_STRING_ERROR_MIN_CLIPPED);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_UNDEFINED, VL53L0_STRING_ERROR_UNDEFINED);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_INVALID_PARAMS,
			VL53L0_STRING_ERROR_INVALID_PARAMS);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_NOT_SUPPORTED,
			VL53L0_STRING_ERROR_NOT_SUPPORTED);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_RANGE_ERROR,
			VL53L0_STRING_ERROR_RANGE_ERROR);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_TIME_OUT,
			VL53L0_STRING_ERROR_TIME_OUT);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_MODE_NOT_SUPPORTED,
			VL53L0_STRING_ERROR_MODE_NOT_SUPPORTED);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_NOT_IMPLEMENTED,
			VL53L0_STRING_ERROR_NOT_IMPLEMENTED);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_BUFFER_TOO_SMALL,
			VL53L0_STRING_ERROR_BUFFER_TOO_SMALL);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_GPIO_NOT_EXISTING,
			VL53L0_STRING_ERROR_GPIO_NOT_EXISTING);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED,
			VL53L0_STRING_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED);
	VL53L0_BUILDSTATUSERRORSTRING(pPalErrorString,
			VL53L0_ERROR_CONTROL_INTERFACE,
			VL53L0_STRING_ERROR_CONTROL_INTERFACE);
	default:
		VL53L0_COPYSTRING(pPalErrorString,
			VL53L0_STRING_UNKNOW_ERROR_CODE);
		break;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0_Error VL53L0_GetPalState(VL53L0_DEV Dev, VL53L0_State *pPalState)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	*pPalState = PALDevDataGet(Dev, PalState);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetPowerMode(VL53L0_DEV Dev, VL53L0_PowerModes PowerMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	/* Only level1 of Power mode exists */
	if ((PowerMode != VL53L0_POWERMODE_STANDBY_LEVEL1) &&
		(PowerMode != VL53L0_POWERMODE_IDLE_LEVEL1)) {
		Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
	} else if (PowerMode == VL53L0_POWERMODE_STANDBY_LEVEL1) {
		/* set the standby level1 of power mode */
		Status = VL53L0_WrByte(Dev, 0x80, 0x00);
		if (Status == VL53L0_ERROR_NONE) {
			/* Set PAL State to standby */
			PALDevDataSet(Dev, PalState, VL53L0_STATE_STANDBY);
			PALDevDataSet(Dev, PowerMode,
				VL53L0_POWERMODE_STANDBY_LEVEL1);
		}

	} else {
		/* VL53L0_POWERMODE_IDLE_LEVEL1 */
		Status = VL53L0_WrByte(Dev, 0x80, 0x01);
		if (Status == VL53L0_ERROR_NONE)
			Status = VL53L0_StaticInit(Dev);
		if (Status == VL53L0_ERROR_NONE)
			PALDevDataSet(Dev, PowerMode,
				VL53L0_POWERMODE_IDLE_LEVEL1);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetPowerMode(VL53L0_DEV Dev, VL53L0_PowerModes *pPowerMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	/* Only level1 of Power mode exists */
	Status = VL53L0_RdByte(Dev, 0x80, &Byte);

	if (Status == VL53L0_ERROR_NONE) {
		if (Byte == 1)
			PALDevDataSet(Dev, PowerMode,
				VL53L0_POWERMODE_IDLE_LEVEL1);
		else
		    PALDevDataSet(Dev, PowerMode,
				VL53L0_POWERMODE_STANDBY_LEVEL1);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetOffsetCalibrationDataMicroMeter(VL53L0_DEV Dev,
				int32_t OffsetCalibrationDataMicroMeter)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t OffsetCalibrationData;
	LOG_FUNCTION_START("");

	OffsetCalibrationData = (uint8_t) (OffsetCalibrationDataMicroMeter
							/ 1000);
	Status = VL53L0_WrByte(Dev, VL53L0_REG_ALGO_PART_TO_PART_RANGE_OFFSET,
				*(uint8_t *) &OffsetCalibrationData);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetOffsetCalibrationDataMicroMeter(VL53L0_DEV Dev,
				int32_t *pOffsetCalibrationDataMicroMeter)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t RangeOffsetRegister;

	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_ALGO_PART_TO_PART_RANGE_OFFSET,
				&RangeOffsetRegister);
	if (Status == VL53L0_ERROR_NONE) {
		*pOffsetCalibrationDataMicroMeter =
				(*((int8_t *) (&RangeOffsetRegister))) * 1000;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetGroupParamHold(VL53L0_DEV Dev, uint8_t GroupParamHold)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	/* not implemented on VL53L0 */

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetUpperLimitMilliMeter(VL53L0_DEV Dev,
				uint16_t *pUpperLimitMilliMeter)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	/* not implemented on VL53L0 */

	LOG_FUNCTION_END(Status);
	return Status;
}
/* End Group PAL General Functions */

/* Group PAL Init Functions */
VL53L0_Error VL53L0_SetDeviceAddress(VL53L0_DEV Dev, uint8_t DeviceAddress)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	Status = VL53L0_WrByte(Dev, VL53L0_REG_I2C_SLAVE_DEVICE_ADDRESS,
				DeviceAddress / 2);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_DataInit(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
	int32_t OffsetCalibrationData;
	LOG_FUNCTION_START("");

	/* Set Default static parameters */
	/* set first temporary values */
	VL53L0_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz, 748421);
	/* 11.3999MHz * 65536 = 748421 */

	/* Get default parameters */
	Status = VL53L0_GetDeviceParameters(Dev, &CurrentParameters);
	if (Status == VL53L0_ERROR_NONE) {
		/* initialize PAL values */
		CurrentParameters.DeviceMode = VL53L0_DEVICEMODE_SINGLE_RANGING;
		CurrentParameters.HistogramMode = VL53L0_HISTOGRAMMODE_DISABLED;
		PALDevDataSet(Dev, CurrentParameters, CurrentParameters);
	}

	/* Sigma estimator variable */
	PALDevDataSet(Dev, SigmaEstRefArray, 100);
	PALDevDataSet(Dev, SigmaEstEffPulseWidth, 900);
	PALDevDataSet(Dev, SigmaEstEffAmbWidth, 500);

	/* Set Signal and Sigma check */
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_SetSigmaLimitCheckEnable(Dev, 0, 0);

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_SetSignalLimitCheckEnable(Dev, 0, 0);


	if (Status == VL53L0_ERROR_NONE)
		VL53L0_SetSigmaLimitValue(Dev, 0, (FixPoint1616_t)(7<<16));

	if (Status == VL53L0_ERROR_NONE)
		VL53L0_SetSignalLimitValue(Dev, 0, (FixPoint1616_t)(7<<16));

	/* Read back NVM offset */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetOffsetCalibrationDataMicroMeter(Dev,
			&OffsetCalibrationData);
	}

	if (Status == VL53L0_ERROR_NONE) {
		PALDevDataSet(Dev, Part2PartOffsetNVMMicroMeter,
			OffsetCalibrationData);

		PALDevDataSet(Dev, SequenceConfig, 0xFF);

		/* Set PAL state to tell that we are waiting for call
		to VL53L0_StaticInit */
		PALDevDataSet(Dev, PalState, VL53L0_STATE_WAIT_STATICINIT);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0_Error VL53L0_StaticInit(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint16_t TempWord;
	uint8_t TempByte;
	uint8_t localBuffer[32];
	uint8_t i;
	uint8_t Revision;
	LOG_FUNCTION_START("");

	Status = VL53L0_check_part_used(Dev, &Revision);

	if (Status == VL53L0_ERROR_NONE) {
		if (Revision == 0)
			Status = VL53L0_load_additional_settings1(Dev);
	}

    /* update13_05_15 */
	if (Status == VL53L0_ERROR_NONE) {
		if ((Revision <= 34) && (Revision != 32)) {

			for (i = 0; i < 32; i++)
				localBuffer[i] = 0xff;

			Status = VL53L0_WriteMulti(Dev, 0x90, localBuffer, 32);

			Status |= VL53L0_WrByte(Dev, 0xb6, 16);
			Status |= VL53L0_WrByte(Dev, 0xb0, 0x0);
			Status |= VL53L0_WrByte(Dev, 0xb1, 0x0);
			Status |= VL53L0_WrByte(Dev, 0xb2, 0xE0);
			Status |= VL53L0_WrByte(Dev, 0xb3, 0xE0);
			Status |= VL53L0_WrByte(Dev, 0xb4, 0xE0);
			Status |= VL53L0_WrByte(Dev, 0xb5, 0xE0);
		}
	}

    /* update 17_06_15_v10 */
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_load_tuning_settings(Dev);

    /* check if GO1 power is ON after load default tuning */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev, 0x80, &TempByte);
		if ((TempByte != 0) && (Status == VL53L0_ERROR_NONE)) {
			/* update 07_05_15 */
			Status = VL53L0_load_additional_settings3(Dev);
		}
	}

    /* Set interrupt config to new sample ready */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetGpioConfig(Dev, 0, 0,
			VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY,
			VL53L0_INTERRUPTPOLARITY_LOW);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrByte(Dev, 0xFF, 0x01);
		Status |= VL53L0_RdWord(Dev, 0x84, &TempWord);
		Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	}

	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz,
			VL53L0_FIXPOINT412TOFIXPOINT1616(TempWord));
	}

	/* After static init, some device parameters may be changed,
	so update them */
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_GetDeviceParameters(Dev, &CurrentParameters);

	if (Status == VL53L0_ERROR_NONE)
		PALDevDataSet(Dev, CurrentParameters, CurrentParameters);


	/* read the sequence config and save it */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG,
					&TempByte);
		if (Status == VL53L0_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, TempByte);

	}


	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_PerformRefCalibration(Dev);

	/* Set PAL State to standby */
	if (Status == VL53L0_ERROR_NONE)
		PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_WaitDeviceBooted(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	/* not implemented on VL53L0 */

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_ResetDevice(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	/* Set reset bit */
	Status = VL53L0_WrByte(Dev, VL53L0_REG_SOFT_RESET_GO2_SOFT_RESET_N,
				0x00);

	/* Wait for some time */
	if (Status == VL53L0_ERROR_NONE) {
		do {
			Status = VL53L0_RdByte(Dev,
				VL53L0_REG_IDENTIFICATION_MODEL_ID,	&Byte);
		} while (Byte != 0x00);
	}

	/* Release reset */
	Status = VL53L0_WrByte(Dev, VL53L0_REG_SOFT_RESET_GO2_SOFT_RESET_N,
		0x01);

	/* Wait until correct boot-up of the device */
	if (Status == VL53L0_ERROR_NONE) {
		do {
			Status = VL53L0_RdByte(Dev,
				VL53L0_REG_IDENTIFICATION_MODEL_ID,	&Byte);
		} while (Byte == 0x00);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}
/* End Group PAL Init Functions */

/* Group PAL Parameters Functions */
VL53L0_Error VL53L0_SetDeviceParameters(VL53L0_DEV Dev,
	const VL53L0_DeviceParameters_t*
	pDeviceParameters)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	int i;
	LOG_FUNCTION_START("");

	Status = VL53L0_SetDeviceMode(Dev, pDeviceParameters->DeviceMode);

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetHistogramMode(Dev,
			pDeviceParameters->HistogramMode);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetInterMeasurementPeriodMilliSeconds(Dev,

			pDeviceParameters->InterMeasurementPeriodMilliSeconds);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetXTalkCompensationEnable(Dev,

			pDeviceParameters->XTalkCompensationEnable);
	}
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetXTalkCompensationRateMegaCps(Dev,

			pDeviceParameters->XTalkCompensationRateMegaCps);
	}

	for (i = 0; i < VL53L0_CHECKPOSITION_NO_OF_CHECKS; i++) {
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetSnrLimitCheckEnable(Dev,
				(VL53L0_CheckPosition) i,

				pDeviceParameters->SnrLimitCheckEnable[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetSnrLimitValue(Dev,
				(VL53L0_CheckPosition) i,

				pDeviceParameters->SnrLimitValue[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetSignalLimitCheckEnable(Dev,
				(VL53L0_CheckPosition) i,

				pDeviceParameters->SignalLimitCheckEnable[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetSignalLimitValue(Dev,
				(VL53L0_CheckPosition) i,
				pDeviceParameters->SignalLimitValue[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetRateLimitCheckEnable(Dev,
				(VL53L0_CheckPosition) i,

				pDeviceParameters->RateLimitCheckEnable[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetRateLimitValue(Dev,
					(VL53L0_CheckPosition) i,
					pDeviceParameters->RateLimitValue[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetSigmaLimitCheckEnable(Dev,
				(VL53L0_CheckPosition) i,

				pDeviceParameters->SigmaLimitCheckEnable[i]);
		}
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetSigmaLimitValue(Dev,
				(VL53L0_CheckPosition) i,

				pDeviceParameters->SigmaLimitValue[i]);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetWrapAroundCheckEnable(Dev,

			pDeviceParameters->WrapAroundCheckEnable);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_SetMeasurementTimingBudgetMicroSeconds(Dev,

			pDeviceParameters->MeasurementTimingBudgetMicroSeconds);
	}


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetDeviceParameters(VL53L0_DEV Dev,
				VL53L0_DeviceParameters_t *pDeviceParameters)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	int i;

	LOG_FUNCTION_START("");

	Status = VL53L0_GetDeviceMode(Dev, &(pDeviceParameters->DeviceMode));

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetHistogramMode(Dev,
			&(pDeviceParameters->HistogramMode));
	}
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetInterMeasurementPeriodMilliSeconds(Dev,
		&(pDeviceParameters->InterMeasurementPeriodMilliSeconds));
	}
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetXTalkCompensationEnable(Dev,
		&(pDeviceParameters->XTalkCompensationEnable));
	}
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetXTalkCompensationRateMegaCps(Dev,

			&(pDeviceParameters->XTalkCompensationRateMegaCps));
	}

	if (Status == VL53L0_ERROR_NONE) {
		for (i = 0; i < VL53L0_CHECKPOSITION_NO_OF_CHECKS; i++) {
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetSnrLimitCheckEnable(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->SnrLimitCheckEnable[i]));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetSnrLimitValue(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->SnrLimitValue[i]));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetSignalLimitCheckEnable(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->SignalLimitCheckEnable[i]
				));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetSignalLimitValue(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->SignalLimitValue[i]));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetRateLimitCheckEnable(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->RateLimitCheckEnable[i]));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetRateLimitValue(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->RateLimitValue[i]));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetSigmaLimitCheckEnable(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->SigmaLimitCheckEnable[i]));
			}
			if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_GetSigmaLimitValue(Dev,
				(VL53L0_CheckPosition)
				i,
				&(pDeviceParameters->SigmaLimitValue[i]));
			}
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetWrapAroundCheckEnable(Dev,
			&(pDeviceParameters->WrapAroundCheckEnable));
	}

    /* Need to be done at the end as it uses VCSELPulsePeriod */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetMeasurementTimingBudgetMicroSeconds(Dev,
		&(pDeviceParameters->MeasurementTimingBudgetMicroSeconds));
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetDeviceMode(VL53L0_DEV Dev,
				VL53L0_DeviceModes DeviceMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("%d", (int)DeviceMode);

	switch (DeviceMode) {
	case VL53L0_DEVICEMODE_SINGLE_RANGING:
	case VL53L0_DEVICEMODE_CONTINUOUS_RANGING:
	case VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING:
	case VL53L0_DEVICEMODE_SINGLE_HISTOGRAM:
		/* Supported mode */
		VL53L0_SETPARAMETERFIELD(Dev, DeviceMode, DeviceMode);
		break;
	default:
		/* Unsupported mode */
		Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetDeviceMode(VL53L0_DEV Dev,
				VL53L0_DeviceModes *pDeviceMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	VL53L0_GETPARAMETERFIELD(Dev, DeviceMode, *pDeviceMode);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetHistogramMode(VL53L0_DEV Dev,
				VL53L0_HistogramModes HistogramMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("%d", (int)HistogramMode);

	switch (HistogramMode) {
	case VL53L0_HISTOGRAMMODE_DISABLED:
		/* Supported mode */
		VL53L0_SETPARAMETERFIELD(Dev, HistogramMode,
			HistogramMode);
		break;
	case VL53L0_HISTOGRAMMODE_REFERENCE_ONLY:
	case VL53L0_HISTOGRAMMODE_RETURN_ONLY:
	case VL53L0_HISTOGRAMMODE_BOTH:
	default:
		/* Unsupported mode */
		Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetHistogramMode(VL53L0_DEV Dev,
				VL53L0_HistogramModes *pHistogramMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	VL53L0_GETPARAMETERFIELD(Dev, HistogramMode, *pHistogramMode);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetMeasurementTimingBudgetMicroSeconds(VL53L0_DEV Dev,
				uint32_t MeasurementTimingBudgetMicroSeconds)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
	uint8_t CurrentVCSELPulsePeriod;
	uint8_t CurrentVCSELPulsePeriodPClk;
	uint8_t Byte;
	uint32_t NewTimingBudgetMicroSeconds;
	uint16_t encodedTimeOut;
	LOG_FUNCTION_START("");

	/* check if rangeB is done: */
	Status = VL53L0_GetWrapAroundCheckEnable(Dev, &Byte);

	if (Status == VL53L0_ERROR_NONE) {
		if (((Byte == 1) && (MeasurementTimingBudgetMicroSeconds <
17000)) ||
			((Byte == 0) && (MeasurementTimingBudgetMicroSeconds <
12000))) {
			Status = VL53L0_ERROR_INVALID_PARAMS;
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		NewTimingBudgetMicroSeconds =
MeasurementTimingBudgetMicroSeconds -
								7000;
		if (Byte == 1) {
			NewTimingBudgetMicroSeconds =
				(uint32_t)(NewTimingBudgetMicroSeconds >> 1);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_get_vcsel_pulse_period(Dev,
			&CurrentVCSELPulsePeriodPClk, 0);
	}

	if (Status == VL53L0_ERROR_NONE) {
		CurrentVCSELPulsePeriod = VL53L0_encode_vcsel_period(
			CurrentVCSELPulsePeriodPClk);
		encodedTimeOut = VL53L0_calc_encoded_timeout(Dev,
			NewTimingBudgetMicroSeconds,
			(uint8_t)
			CurrentVCSELPulsePeriod);
		VL53L0_SETPARAMETERFIELD(Dev,
MeasurementTimingBudgetMicroSeconds,
			MeasurementTimingBudgetMicroSeconds);
		VL53L0_SETDEVICESPECIFICPARAMETER(Dev, LastEncodedTimeout,
			encodedTimeOut);
	}

	/* Program in register */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrWord(Dev, VL53L0_REG_RNGA_TIMEOUT_MSB,
			encodedTimeOut);
	}

	/* Temp: program same value for rangeB1 and rangeB2 */
	/* Range B1 */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_get_vcsel_pulse_period(Dev,
			&CurrentVCSELPulsePeriodPClk, 1);
		if (Status == VL53L0_ERROR_NONE) {
			CurrentVCSELPulsePeriod = VL53L0_encode_vcsel_period(
				CurrentVCSELPulsePeriodPClk);
			encodedTimeOut = VL53L0_calc_encoded_timeout(Dev,
				NewTimingBudgetMicroSeconds,
				(uint8_t)
				CurrentVCSELPulsePeriod);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrWord(Dev, VL53L0_REG_RNGB1_TIMEOUT_MSB,
			encodedTimeOut);
	}

	/* Range B2 */
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_get_vcsel_pulse_period(Dev,
			&CurrentVCSELPulsePeriodPClk, 2);
		if (Status == VL53L0_ERROR_NONE) {
			CurrentVCSELPulsePeriod = VL53L0_encode_vcsel_period(
				CurrentVCSELPulsePeriodPClk);
			encodedTimeOut = VL53L0_calc_encoded_timeout(Dev,
				NewTimingBudgetMicroSeconds,
				(uint8_t)
				CurrentVCSELPulsePeriod);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrWord(Dev, VL53L0_REG_RNGB2_TIMEOUT_MSB,
			encodedTimeOut);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetMeasurementTimingBudgetMicroSeconds(VL53L0_DEV Dev,
				uint32_t *pMeasurementTimingBudgetMicroSeconds)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t CurrentVCSELPulsePeriod;
	uint8_t CurrentVCSELPulsePeriodPClk;
	uint16_t encodedTimeOut;
	uint32_t RangATimingBudgetMicroSeconds;
	uint32_t RangBTimingBudgetMicroSeconds;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	/* check if rangeB is done: */
	Status = VL53L0_GetWrapAroundCheckEnable(Dev, &Byte);

	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_get_vcsel_pulse_period(Dev,
		&CurrentVCSELPulsePeriodPClk, 0);
		CurrentVCSELPulsePeriod = VL53L0_encode_vcsel_period(
			CurrentVCSELPulsePeriodPClk);

		/* Read from register */
		Status = VL53L0_RdWord(Dev, VL53L0_REG_RNGA_TIMEOUT_MSB,
			&encodedTimeOut);
		if (Status == VL53L0_ERROR_NONE) {
			RangATimingBudgetMicroSeconds =
			VL53L0_calc_ranging_wait_us(Dev,
				encodedTimeOut,
				CurrentVCSELPulsePeriod);
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		if (Byte == 0) {
			*pMeasurementTimingBudgetMicroSeconds =
				RangATimingBudgetMicroSeconds + 7000;
			VL53L0_SETPARAMETERFIELD(Dev,
			MeasurementTimingBudgetMicroSeconds,
				*pMeasurementTimingBudgetMicroSeconds);
		} else {
			VL53L0_get_vcsel_pulse_period(Dev,
			&CurrentVCSELPulsePeriodPClk, 1);
			CurrentVCSELPulsePeriod = VL53L0_encode_vcsel_period(
				CurrentVCSELPulsePeriodPClk);

			/* Read from register */
			Status = VL53L0_RdWord(Dev,
			VL53L0_REG_RNGB1_TIMEOUT_MSB,
						&encodedTimeOut);
			if (Status == VL53L0_ERROR_NONE) {
				RangBTimingBudgetMicroSeconds =
				VL53L0_calc_ranging_wait_us(
					Dev, encodedTimeOut,
					CurrentVCSELPulsePeriod);
			}

			*pMeasurementTimingBudgetMicroSeconds =
				RangATimingBudgetMicroSeconds +
				RangBTimingBudgetMicroSeconds +
				7000;
			VL53L0_SETPARAMETERFIELD(Dev,
			MeasurementTimingBudgetMicroSeconds,
				*pMeasurementTimingBudgetMicroSeconds);
		}
	}



	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetInterMeasurementPeriodMilliSeconds(VL53L0_DEV Dev,
				uint32_t InterMeasurementPeriodMilliSeconds)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint16_t osc_calibrate_val;
	uint32_t IMPeriodMilliSeconds;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdWord(Dev, VL53L0_REG_OSC_CALIBRATE_VAL,
		&osc_calibrate_val);

	if (Status == VL53L0_ERROR_NONE) {
		if (osc_calibrate_val != 0) {
			IMPeriodMilliSeconds =
			InterMeasurementPeriodMilliSeconds *
				osc_calibrate_val;
		} else {
			IMPeriodMilliSeconds =
			InterMeasurementPeriodMilliSeconds;
		}
		Status = VL53L0_WrDWord(Dev,
		VL53L0_REG_SYSTEM_INTERMEASUREMENT_PERIOD,
			IMPeriodMilliSeconds);
	}

	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETPARAMETERFIELD(Dev,
		InterMeasurementPeriodMilliSeconds,
			InterMeasurementPeriodMilliSeconds);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetInterMeasurementPeriodMilliSeconds(VL53L0_DEV Dev,
				uint32_t *pInterMeasurementPeriodMilliSeconds)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint16_t osc_calibrate_val;
	uint32_t IMPeriodMilliSeconds;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdWord(Dev, VL53L0_REG_OSC_CALIBRATE_VAL,
				&osc_calibrate_val);

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdDWord(Dev,
		VL53L0_REG_SYSTEM_INTERMEASUREMENT_PERIOD,
			&IMPeriodMilliSeconds);
	}

	if (Status == VL53L0_ERROR_NONE) {
		if (osc_calibrate_val != 0)
			*pInterMeasurementPeriodMilliSeconds =
				IMPeriodMilliSeconds /
				osc_calibrate_val;

		VL53L0_SETPARAMETERFIELD(Dev,
		InterMeasurementPeriodMilliSeconds,
			*pInterMeasurementPeriodMilliSeconds);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetXTalkCompensationEnable(VL53L0_DEV Dev,
		uint8_t XTalkCompensationEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t XTalkCompensationEnableValue;
	LOG_FUNCTION_START("");

	if (XTalkCompensationEnable == 0) {
		/* Disable the crosstalk compensation */
		XTalkCompensationEnableValue = 0x00;
	} else {
		/* Enable the crosstalk compensation */
		XTalkCompensationEnableValue = 0x01;
	}
	Status = VL53L0_UpdateByte(Dev, VL53L0_REG_ALGO_RANGE_CHECK_ENABLES,
		0xFE,
		XTalkCompensationEnableValue);
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationEnable,
			XTalkCompensationEnableValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetXTalkCompensationEnable(VL53L0_DEV Dev, uint8_t*
	pXTalkCompensationEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t data;
	uint8_t Temp;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_ALGO_RANGE_CHECK_ENABLES, &data);
	if (Status == VL53L0_ERROR_NONE) {
		if (data & 0x01)
			Temp = 0x01;
		else
			Temp = 0x00;

		*pXTalkCompensationEnable = Temp;
	}
	if (Status == VL53L0_ERROR_NONE)
		VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationEnable, Temp);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetXTalkCompensationRateMegaCps(VL53L0_DEV Dev,
	FixPoint1616_t XTalkCompensationRateMegaCps)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	Status = VL53L0_WrWord(Dev, VL53L0_REG_ALGO_CROSSTALK_COMPENSATION_RATE,
		VL53L0_FIXPOINT1616TOFIXPOINT412(XTalkCompensationRateMegaCps));
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
			XTalkCompensationRateMegaCps);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetXTalkCompensationRateMegaCps(VL53L0_DEV Dev,
	FixPoint1616_t *pXTalkCompensationRateMegaCps)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint16_t Value;
	FixPoint1616_t TempFix1616;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdWord(Dev, VL53L0_REG_ALGO_CROSSTALK_COMPENSATION_RATE,
		(uint16_t *) &Value);
	if (Status == VL53L0_ERROR_NONE) {
		TempFix1616 = VL53L0_FIXPOINT412TOFIXPOINT1616(Value);
		*pXTalkCompensationRateMegaCps = TempFix1616;
		VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
			TempFix1616);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/*
 * SNR LIMIT
 */
VL53L0_Error VL53L0_SetSnrLimitCheckEnable(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, uint8_t SnrLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t tmp8;
	uint8_t SnrLimitCheckEnableInt;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;

	if (SnrLimitCheckEnable == 0) {
		/* Disable the SNR check */
		SnrLimitCheckEnableInt = 0;
	} else {
		/* Enable the SNR check */
		SnrLimitCheckEnableInt = 1;
	}

	tmp8 = (uint8_t)(SnrLimitCheckEnableInt << 3);

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_UpdateByte(Dev,
			VL53L0_REG_ALGO_RANGE_CHECK_ENABLES,
			0xF7, tmp8);
	}
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETARRAYPARAMETERFIELD(Dev, SnrLimitCheckEnable,
			Position,
			SnrLimitCheckEnableInt);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSnrLimitCheckEnable(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, uint8_t *pSnrLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t data;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_ALGO_RANGE_CHECK_ENABLES, &data);
	}

	if (Status == VL53L0_ERROR_NONE) {
		if (data & (0x01 << 3))
			*pSnrLimitCheckEnable = 0x01;
		else
			*pSnrLimitCheckEnable = 0x00;
	}
	if (Status == VL53L0_ERROR_NONE)
		VL53L0_SETARRAYPARAMETERFIELD(Dev,
			SnrLimitCheckEnable, Position,
			*pSnrLimitCheckEnable);


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetSnrLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t SnrLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	Status = VL53L0_WrByte(Dev, VL53L0_REG_ALGO_SNR_RATIO,
		VL53L0_FIXPOINT1616TOFIXPOINT08(SnrLimitValue));
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETARRAYPARAMETERFIELD(Dev, SnrLimitValue, Position,
			SnrLimitValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSnrLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t *pSnrLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Value;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev, VL53L0_REG_ALGO_SNR_RATIO,
			(uint8_t *)&Value);
	}

	if (Status == VL53L0_ERROR_NONE) {
		*pSnrLimitValue = VL53L0_FIXPOINT08TOFIXPOINT1616(Value);
		VL53L0_SETARRAYPARAMETERFIELD(Dev, SnrLimitValue, Position,
			*pSnrLimitValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/*
 * SIGNAL LIMIT
 */
VL53L0_Error VL53L0_SetSignalLimitCheckEnable(VL53L0_DEV Dev,
	VL53L0_CheckPosition Position, uint8_t SignalLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t i;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		for (i = 0; i < VL53L0_CHECKPOSITION_NO_OF_CHECKS; i++) {
			VL53L0_SETARRAYPARAMETERFIELD(Dev,
				SignalLimitCheckEnable,
				(VL53L0_CheckPosition) i,
				SignalLimitCheckEnable);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSignalLimitCheckEnable(VL53L0_DEV Dev,
	VL53L0_CheckPosition Position, uint8_t *pSignalLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */

	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS) {
		Status = VL53L0_ERROR_INVALID_PARAMS;
	} else {
		VL53L0_GETARRAYPARAMETERFIELD(Dev, SignalLimitCheckEnable,
			Position,
			*pSignalLimitCheckEnable);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetSignalLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t SignalLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t i;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored, set is done on all
	positions */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		for (i = 0; i < VL53L0_CHECKPOSITION_NO_OF_CHECKS; i++) {
			VL53L0_SETARRAYPARAMETERFIELD(Dev,
				SignalLimitValue,
				(VL53L0_CheckPosition) i,
				SignalLimitValue);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSignalLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t *pSignalLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_GETARRAYPARAMETERFIELD(Dev, SignalLimitValue, Position,
			*pSignalLimitValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/*
 * SIGMA LIMIT
 */
VL53L0_Error VL53L0_SetSigmaLimitCheckEnable(VL53L0_DEV Dev,
	VL53L0_CheckPosition Position, uint8_t SigmaLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t i;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored, set is done on all
	positions */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		for (i = 0; i < VL53L0_CHECKPOSITION_NO_OF_CHECKS; i++) {
			VL53L0_SETARRAYPARAMETERFIELD(Dev,
				SigmaLimitCheckEnable,
				(VL53L0_CheckPosition) i,
				SigmaLimitCheckEnable);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSigmaLimitCheckEnable(VL53L0_DEV Dev,
	VL53L0_CheckPosition Position, uint8_t *pSigmaLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS) {
		Status = VL53L0_ERROR_INVALID_PARAMS;
	} else {
		VL53L0_GETARRAYPARAMETERFIELD(Dev, SigmaLimitCheckEnable,
			Position,
			*pSigmaLimitCheckEnable);
	}


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetSigmaLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t SigmaLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t i;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored, set is done on all
	positions */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		for (i = 0; i < VL53L0_CHECKPOSITION_NO_OF_CHECKS; i++) {
			VL53L0_SETARRAYPARAMETERFIELD(Dev, SigmaLimitValue,
				(VL53L0_CheckPosition) i, SigmaLimitValue);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSigmaLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t *pSigmaLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_GETARRAYPARAMETERFIELD(Dev, SigmaLimitValue, Position,
			*pSigmaLimitValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/*
 * RATE LIMIT
 */

VL53L0_Error VL53L0_SetRateLimitCheckEnable(VL53L0_DEV Dev,
	VL53L0_CheckPosition Position, uint8_t RateLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t tmp8;
	uint8_t RateLimitCheckEnableInt;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (RateLimitCheckEnable == 0) {
		/* Disable the Rate Limit Check */
		RateLimitCheckEnableInt = 0;
	} else {
		/* Enable the Rate Limit Check */
		RateLimitCheckEnableInt = 1;
	}

	tmp8 = (uint8_t)(RateLimitCheckEnableInt << 1);

	Status = VL53L0_UpdateByte(Dev, VL53L0_REG_ALGO_RANGE_CHECK_ENABLES,
				0xFD,
				tmp8);
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETARRAYPARAMETERFIELD(Dev, RateLimitCheckEnable,
			Position,
			RateLimitCheckEnableInt);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetRateLimitCheckEnable(VL53L0_DEV Dev,
	VL53L0_CheckPosition Position, uint8_t *pRateLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	uint8_t data;
	LOG_FUNCTION_START("");

	/* In this function VL53L0_CheckPosition is ignored */
	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	Status = VL53L0_RdByte(Dev, VL53L0_REG_ALGO_RANGE_CHECK_ENABLES, &data);
	if (Status == VL53L0_ERROR_NONE) {
		if (data & (0x02))
			*pRateLimitCheckEnable = 0x01;
		else
			*pRateLimitCheckEnable = 0x00;

	}
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETARRAYPARAMETERFIELD(Dev, RateLimitCheckEnable,
			Position,
			*pRateLimitCheckEnable);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetRateLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t RateLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrByte(Dev,
			VL53L0_REG_ALGO_RANGE_IGNORE_THRESHOLD,
			VL53L0_FIXPOINT1616TOFIXPOINT08(RateLimitValue));
	}

	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETARRAYPARAMETERFIELD(Dev, RateLimitValue, Position,
			RateLimitValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetRateLimitValue(VL53L0_DEV Dev, VL53L0_CheckPosition
	Position, FixPoint1616_t *pRateLimitValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Value;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	if (Position >= VL53L0_CHECKPOSITION_NO_OF_CHECKS)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_ALGO_RANGE_IGNORE_THRESHOLD,
			(uint8_t *) &Value);
	}

	if (Status == VL53L0_ERROR_NONE) {
		*pRateLimitValue = VL53L0_FIXPOINT08TOFIXPOINT1616(Value);
		VL53L0_SETARRAYPARAMETERFIELD(Dev, RateLimitValue, Position,
			*pRateLimitValue);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}



/*
 * CHECK LIMIT FUNCTIONS
 */

VL53L0_Error VL53L0_GetNumberOfLimitCheck(uint16_t *pNumberOfLimitCheck)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	*pNumberOfLimitCheck = VL53L0_CHECKENABLE_NUMBER_OF_CHECKS;

	LOG_FUNCTION_END(Status);
	return Status;
}


#define VL53L0_BUILDCASESTRING(BUFFER, CODE, STRINGVALUE) \
	do { \
		case CODE: \
			VL53L0_COPYSTRING(BUFFER, STRINGVALUE); \
			break; \
	} while (0)

VL53L0_Error VL53L0_GetLimitCheckInfo(VL53L0_DEV Dev, uint16_t LimitCheckId,
	char *pLimitCheckString)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	switch (LimitCheckId) {
	VL53L0_BUILDCASESTRING(pLimitCheckString,
			VL53L0_CHECKENABLE_SNR,
			VL53L0_STRING_CHECKENABLE_SNR);
	VL53L0_BUILDCASESTRING(pLimitCheckString,
			VL53L0_CHECKENABLE_SIGMA,
			VL53L0_STRING_CHECKENABLE_SIGMA);
	VL53L0_BUILDCASESTRING(pLimitCheckString,
			VL53L0_CHECKENABLE_RANGE,
			VL53L0_STRING_CHECKENABLE_RANGE);
	VL53L0_BUILDCASESTRING(pLimitCheckString,
			VL53L0_CHECKENABLE_SIGNAL_RATE,
			VL53L0_STRING_CHECKENABLE_SIGNAL_RATE);
		break;
	default:
		VL53L0_COPYSTRING(pLimitCheckString,
			VL53L0_STRING_UNKNOW_ERROR_CODE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetLimitCheckEnable(VL53L0_DEV Dev, uint16_t LimitCheckId,
	uint8_t LimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	switch (LimitCheckId) {
	case VL53L0_CHECKENABLE_SNR:
		Status = VL53L0_SetSnrLimitCheckEnable(Dev, 0,
			LimitCheckEnable);
		break;

	case VL53L0_CHECKENABLE_SIGMA:
		Status = VL53L0_SetSigmaLimitCheckEnable(Dev, 0,
			LimitCheckEnable);
		break;

	case VL53L0_CHECKENABLE_RANGE:
		Status = VL53L0_SetRateLimitCheckEnable(Dev, 0,
			LimitCheckEnable);
		break;

	case VL53L0_CHECKENABLE_SIGNAL_RATE:
		Status = VL53L0_SetSignalLimitCheckEnable(Dev, 0,
			LimitCheckEnable);
		break;

	default:
		Status = VL53L0_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0_Error VL53L0_GetLimitCheckEnable(VL53L0_DEV Dev, uint16_t LimitCheckId,
	uint8_t *pLimitCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	switch (LimitCheckId) {
	case VL53L0_CHECKENABLE_SNR:
		Status = VL53L0_GetSnrLimitCheckEnable(Dev, 0,
			pLimitCheckEnable);
		break;

	case VL53L0_CHECKENABLE_SIGMA:
		Status = VL53L0_GetSigmaLimitCheckEnable(Dev, 0,
			pLimitCheckEnable);
		break;

	case VL53L0_CHECKENABLE_RANGE:
		Status = VL53L0_GetRateLimitCheckEnable(Dev, 0,
			pLimitCheckEnable);
		break;

	case VL53L0_CHECKENABLE_SIGNAL_RATE:
		Status = VL53L0_GetSignalLimitCheckEnable(Dev, 0,
			pLimitCheckEnable);
		break;

	default:
		Status = VL53L0_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}



VL53L0_Error VL53L0_SetLimitCheckValue(VL53L0_DEV Dev, uint16_t LimitCheckId,
	FixPoint1616_t LimitCheckValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");


	switch (LimitCheckId) {
	case VL53L0_CHECKENABLE_SNR:
		Status = VL53L0_SetSnrLimitValue(Dev, 0, LimitCheckValue);
		break;

	case VL53L0_CHECKENABLE_SIGMA:
		Status = VL53L0_SetSigmaLimitValue(Dev, 0, LimitCheckValue);
		break;

	case VL53L0_CHECKENABLE_RANGE:
		Status = VL53L0_SetRateLimitValue(Dev, 0, LimitCheckValue);
		break;

	case VL53L0_CHECKENABLE_SIGNAL_RATE:
		Status = VL53L0_SetSignalLimitValue(Dev, 0, LimitCheckValue);
		break;

	default:
		Status = VL53L0_ERROR_INVALID_PARAMS;
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0_Error VL53L0_GetLimitCheckValue(VL53L0_DEV Dev, uint16_t LimitCheckId,
	FixPoint1616_t *pLimitCheckValue)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL53L0_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L0_ERROR_INVALID_PARAMS;
	} else {
		switch (LimitCheckId) {
		case VL53L0_CHECKENABLE_SNR:
			Status = VL53L0_GetSnrLimitValue(Dev, 0,
				pLimitCheckValue);
			break;

		case VL53L0_CHECKENABLE_SIGMA:
			Status = VL53L0_GetSigmaLimitValue(Dev, 0,
				pLimitCheckValue);
			break;

		case VL53L0_CHECKENABLE_RANGE:
			Status = VL53L0_GetRateLimitValue(Dev, 0,
				pLimitCheckValue);
			break;

		case VL53L0_CHECKENABLE_SIGNAL_RATE:
			Status = VL53L0_GetSignalLimitValue(Dev, 0,
				pLimitCheckValue);
			break;

		default:
			Status = VL53L0_ERROR_INVALID_PARAMS;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;

}




/*
 * WRAPAROUND LIMIT
 */
VL53L0_Error VL53L0_SetWrapAroundCheckEnable(VL53L0_DEV Dev, uint8_t
	WrapAroundCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Byte;
	uint8_t WrapAroundCheckEnableInt;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, &Byte);
	if (WrapAroundCheckEnable == 0) {
		/* Disable wraparound */
		Byte = Byte & 0x7F;
		WrapAroundCheckEnableInt = 0;
	} else {
		/* Enable wraparound */
		Byte = Byte | 0x80;
		WrapAroundCheckEnableInt = 1;
	}

	Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, Byte);

	if (Status == VL53L0_ERROR_NONE) {
		PALDevDataSet(Dev, SequenceConfig, Byte);
		VL53L0_SETPARAMETERFIELD(Dev, WrapAroundCheckEnable,
			WrapAroundCheckEnableInt);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetWrapAroundCheckEnable(VL53L0_DEV Dev, uint8_t
	*pWrapAroundCheckEnable)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t data;
	VL53L0_DeviceParameters_t CurrentParameters;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, &data);
	if (Status == VL53L0_ERROR_NONE) {
		PALDevDataSet(Dev, SequenceConfig, data);
		if (data & (0x01 << 7))
			*pWrapAroundCheckEnable = 0x01;
		else
			*pWrapAroundCheckEnable = 0x00;

	}
	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETPARAMETERFIELD(Dev, WrapAroundCheckEnable,
			*pWrapAroundCheckEnable);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/*  End Group PAL Parameters Functions */

/* Group PAL Measurement Functions */
VL53L0_Error VL53L0_PerformSingleMeasurement(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceModes DeviceMode;
	uint8_t NewDatReady = 0;
	uint32_t LoopNb;
	LOG_FUNCTION_START("");

	/* Get Current DeviceMode */
	Status = VL53L0_GetDeviceMode(Dev, &DeviceMode);

	/* Start immediately to run a single ranging measurement in case of
	single ranging or single histogram */
	if ((Status == VL53L0_ERROR_NONE) &&
		((DeviceMode == VL53L0_DEVICEMODE_SINGLE_RANGING) ||
		(DeviceMode ==
		VL53L0_DEVICEMODE_SINGLE_HISTOGRAM))) {
		Status = VL53L0_StartMeasurement(Dev);
	}

	/* Wait until it finished
	use timeout to avoid deadlock
	*/
	if (Status == VL53L0_ERROR_NONE) {
		LoopNb = 0;
		do {
			Status = VL53L0_GetMeasurementDataReady(Dev,
				&NewDatReady);
			if ((NewDatReady == 0x01) || Status !=
				VL53L0_ERROR_NONE) {
				break;
			}
			LoopNb = LoopNb + 1;
			VL53L0_PollingDelay(Dev);
		} while (LoopNb < VL53L0_DEFAULT_MAX_LOOP);

		if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP)
			Status = VL53L0_ERROR_TIME_OUT;

	}

	/* Change PAL State in case of single ranging or single histogram
	*/
	if ((Status == VL53L0_ERROR_NONE) &&
		((DeviceMode == VL53L0_DEVICEMODE_SINGLE_RANGING) ||
			(DeviceMode ==
			VL53L0_DEVICEMODE_SINGLE_HISTOGRAM))) {
		PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_PerformRefCalibration(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t NewDatReady = 0;
	uint8_t Byte = 0;
	uint8_t SequenceConfig = 0;
	uint32_t LoopNb;
	LOG_FUNCTION_START("");

	/* store the value of the sequence config,
	 * this will be reset before the end of the function
	 */

	SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

	Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, 0x03);

	if (Status == VL53L0_ERROR_NONE) {
		PALDevDataSet(Dev, SequenceConfig, 0x03);
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
			VL53L0_REG_SYSRANGE_MODE_START_STOP);
	}

	if (Status == VL53L0_ERROR_NONE) {
		/* Wait until start bit has been cleared */
		LoopNb = 0;
		do {
			if (LoopNb > 0)
				Status = VL53L0_RdByte(Dev,
					VL53L0_REG_SYSRANGE_START, &Byte);
			LoopNb = LoopNb + 1;
		} while (((Byte & VL53L0_REG_SYSRANGE_MODE_START_STOP) ==
			VL53L0_REG_SYSRANGE_MODE_START_STOP) &&
			(Status == VL53L0_ERROR_NONE) &&
			(LoopNb < VL53L0_DEFAULT_MAX_LOOP));
	}

	/* Wait until it finished
	use timeout to avoid deadlock
	*/
	if (Status == VL53L0_ERROR_NONE) {
		LoopNb = 0;
		do {
			Status = VL53L0_GetMeasurementDataReady(Dev,
				&NewDatReady);
			if ((NewDatReady == 0x01) || Status !=
				VL53L0_ERROR_NONE) {
				break;
			}
			LoopNb = LoopNb + 1;
			VL53L0_PollingDelay(Dev);
		} while (LoopNb < VL53L0_DEFAULT_MAX_LOOP);

		if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP)
			Status = VL53L0_ERROR_TIME_OUT;

	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrByte(Dev, 0xFF, 0x01);
		Status |= VL53L0_WrByte(Dev, 0x00, 0x00);

		Status |= VL53L0_WrByte(Dev, 0xFF, 0x04);
		Status |= VL53L0_RdByte(Dev, 0x30, &Byte);

		Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
		Status |= VL53L0_WrByte(Dev, 0x31, Byte);
		Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
		Status |= VL53L0_WrByte(Dev, 0x00, 0x01);
		Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	}

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_ClearInterruptMask(Dev, 0);


	if (Status == VL53L0_ERROR_NONE) {
		/* restore the previous Sequence Config */
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG,
			SequenceConfig);
	}
	if (Status == VL53L0_ERROR_NONE)
		PALDevDataSet(Dev, SequenceConfig, SequenceConfig);


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_API VL53L0_Error VL53L0_PerformXTalkCalibration(VL53L0_DEV Dev,
	FixPoint1616_t XTalkCalDistance, FixPoint1616_t
	*pXTalkCompensationRateMegaCps)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint16_t sum_ranging = 0;
	uint16_t sum_spads = 0;
	FixPoint1616_t sum_signalRate = 0;
	FixPoint1616_t total_count = 0;
	uint8_t xtalk_meas = 0;
	VL53L0_RangingMeasurementData_t RangingMeasurementData;
	FixPoint1616_t xTalkStoredMeanSignalRate;
	FixPoint1616_t xTalkStoredMeanRange;
	FixPoint1616_t xTalkStoredMeanRtnSpads;
	uint32_t signalXTalkTotalPerSpad;
	uint32_t xTalkStoredMeanRtnSpadsAsInt;
	uint32_t xTalkCalDistanceAsInt;
	FixPoint1616_t XTalkCompensationRateMegaCps;
	LOG_FUNCTION_START("");

	if (XTalkCalDistance <= 0)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	/* Disable the XTalk compensation */
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_SetXTalkCompensationEnable(Dev, 0);


	/* Perform 50 measurements and compute the averages */
	if (Status == VL53L0_ERROR_NONE) {
		sum_ranging = 0;
		sum_spads = 0;
		sum_signalRate = 0;
		total_count = 0;
		for (xtalk_meas = 0; xtalk_meas < 50; xtalk_meas++) {
			Status = VL53L0_PerformSingleRangingMeasurement(Dev,
				&RangingMeasurementData);

			if (Status != VL53L0_ERROR_NONE)
				break;


			if (RangingMeasurementData.RangeMilliMeter < 8000) {
				sum_ranging = sum_ranging +
				RangingMeasurementData.RangeMilliMeter;
				sum_signalRate = sum_signalRate +
				RangingMeasurementData.SignalRateRtnMegaCps;
				sum_spads = sum_spads +
				RangingMeasurementData.EffectiveSpadRtnCount
				/ 32;
				total_count = total_count + 1;
			}
		}

		if (total_count == 0) {
			/* no valid values found */
			Status = VL53L0_ERROR_RANGE_ERROR;
		}
	}


	if (Status == VL53L0_ERROR_NONE) {
		/* FixPoint1616_t / uint16_t = FixPoint1616_t */
		xTalkStoredMeanSignalRate = sum_signalRate / total_count;
		xTalkStoredMeanRange =
			(FixPoint1616_t)((uint32_t)(sum_ranging << 16) /
			total_count);
		xTalkStoredMeanRtnSpads =
			(FixPoint1616_t)((uint32_t)(sum_spads<<16) /
			total_count);

		/* Round Mean Spads to Whole Number.
		Typically the calculated mean SPAD count is a whole number or
		very close to a whole
		number, therefore any truncation will not result in a
		significant	loss in accuracy.
		Also, for a grey target at a typical distance of around 400mm,
		around 220 SPADs will
		be enabled, therefore, any truncation will result in a loss of
		accuracy of less than 0.5%.
		*/
		xTalkStoredMeanRtnSpadsAsInt = (xTalkStoredMeanRtnSpads +
			0x8000) >> 16;

		/* Round Cal Distance to Whole Number.
		Note that the cal distance is in mm, therefore no resolution is
		lost.
		*/
		xTalkCalDistanceAsInt = (XTalkCalDistance + 0x8000) >> 16;

		if (xTalkStoredMeanRtnSpadsAsInt == 0 || xTalkCalDistanceAsInt
			== 0 ||
			xTalkStoredMeanRange >= XTalkCalDistance) {
			XTalkCompensationRateMegaCps = 0;
		} else {
			/* Round Cal Distance to Whole Number.
			Note that the cal distance is in mm, therefore no
			resolution is lost.
			*/
			xTalkCalDistanceAsInt = (XTalkCalDistance + 0x8000) >>
				16;

			/* Apply division by mean spad count early in the
			calculation to keep the numbers small.
			This ensures we can maintain a 32bit calculation.
			Fixed1616 / int := Fixed1616
			*/
			signalXTalkTotalPerSpad =
				(xTalkStoredMeanSignalRate) /
				xTalkStoredMeanRtnSpadsAsInt;

			/* Complete the calculation for total Signal XTalk per
			SPAD Fixed1616 * (Fixed1616 - Fixed1616/int)
			:= (2^16 * Fixed1616)
			*/
			signalXTalkTotalPerSpad *= ((1<<16) -
				(xTalkStoredMeanRange/xTalkCalDistanceAsInt));

			/* Round from 2^16 * Fixed1616, to Fixed1616. */
			XTalkCompensationRateMegaCps = (signalXTalkTotalPerSpad
				+ 0x8000) >> 16;
		}

		*pXTalkCompensationRateMegaCps = XTalkCompensationRateMegaCps;

		/* Enable the XTalk compensation */
		if (Status == VL53L0_ERROR_NONE)
			Status = VL53L0_SetXTalkCompensationEnable(Dev, 1);


		/* Enable the XTalk compensation */
		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_SetXTalkCompensationRateMegaCps(Dev,
				XTalkCompensationRateMegaCps);
		}

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_StartMeasurement(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceModes DeviceMode;
	uint8_t Byte = 0;
	uint32_t LoopNb;
	LOG_FUNCTION_START("");

	/* Get Current DeviceMode */
	VL53L0_GetDeviceMode(Dev, &DeviceMode);

	switch (DeviceMode) {
	case VL53L0_DEVICEMODE_SINGLE_RANGING:
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
			VL53L0_REG_SYSRANGE_MODE_SINGLESHOT |
			VL53L0_REG_SYSRANGE_MODE_START_STOP);
		break;
	case VL53L0_DEVICEMODE_CONTINUOUS_RANGING:
		/* Back-to-back mode */
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
			VL53L0_REG_SYSRANGE_MODE_BACKTOBACK |
			VL53L0_REG_SYSRANGE_MODE_START_STOP);
		if (Status == VL53L0_ERROR_NONE) {
			/* Set PAL State to Running */
			PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);
		}
		break;
	case VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING:
		/* Continuous mode */
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
			VL53L0_REG_SYSRANGE_MODE_TIMED |
			VL53L0_REG_SYSRANGE_MODE_START_STOP);
		if (Status == VL53L0_ERROR_NONE) {
			/* Set PAL State to Running */
			PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);
		}
		break;
	default:
		/* Selected mode not supported */
		Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
	}

	if (Status == VL53L0_ERROR_NONE) {
		/* Wait until start bit has been cleared */
		LoopNb = 0;
		do {
			if (LoopNb > 0)
				Status = VL53L0_RdByte(Dev,
					VL53L0_REG_SYSRANGE_START, &Byte);
			LoopNb = LoopNb + 1;
		} while (((Byte & VL53L0_REG_SYSRANGE_MODE_START_STOP) ==
			VL53L0_REG_SYSRANGE_MODE_START_STOP) &&
			(Status == VL53L0_ERROR_NONE) &&
			(LoopNb < VL53L0_DEFAULT_MAX_LOOP));

		if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP)
			Status = VL53L0_ERROR_TIME_OUT;

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_StopMeasurement(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
		VL53L0_REG_SYSRANGE_MODE_SINGLESHOT);

	if (Status == VL53L0_ERROR_NONE) {
		/* Set PAL State to Idle */
		PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetMeasurementDataReady(VL53L0_DEV Dev, uint8_t
	*pMeasurementDataReady)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t SysRangeStatusRegister;
	uint8_t InterruptConfig;
	uint32_t InterruptMask;
	LOG_FUNCTION_START("");

	InterruptConfig = VL53L0_GETDEVICESPECIFICPARAMETER(Dev,
		Pin0GpioFunctionality);

	if (InterruptConfig ==
	VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY) {
		VL53L0_GetInterruptMaskStatus(Dev, &InterruptMask);
		if (InterruptMask ==
		VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY){
			*pMeasurementDataReady = 1;
		} else {
			*pMeasurementDataReady = 0;
		}
	} else {
		Status = VL53L0_RdByte(Dev, VL53L0_REG_RESULT_RANGE_STATUS,
			&SysRangeStatusRegister);
		if (Status == VL53L0_ERROR_NONE) {
			if (SysRangeStatusRegister & 0x01)
				*pMeasurementDataReady = 1;
			else
				*pMeasurementDataReady = 0;

		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_WaitDeviceReadyForNewMeasurement(VL53L0_DEV Dev, uint32_t
	MaxLoop)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	/* not implemented for VL53L0 */

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetRangingMeasurementData(VL53L0_DEV Dev,
	VL53L0_RangingMeasurementData_t *pRangingMeasurementData)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t DeviceRangeStatus;
	uint8_t PalRangeStatus;
	uint16_t AmbientRate;
	FixPoint1616_t SignalRate;
	FixPoint1616_t CrosstalkCompensation;
	uint16_t EffectiveSpadRtnCount;
	uint8_t localBuffer[14];
	VL53L0_RangingMeasurementData_t LastRangeDataBuffer;
	LOG_FUNCTION_START("");

	/* use multi read even if some registers are not useful, result will be
	more efficient
	start reading at 0x14 dec20
	end reading at 0x21 dec33 total 14 bytes to read
	*/
	Status = VL53L0_ReadMulti(Dev, 0x14, localBuffer, 14);

	if (Status == VL53L0_ERROR_NONE) {

		pRangingMeasurementData->ZoneId = 0; /* Only one zone */
		pRangingMeasurementData->TimeStamp = 0; /* Not Implemented */

		pRangingMeasurementData->RangeMilliMeter =
			VL53L0_MAKEUINT16(localBuffer[11], localBuffer[10]);

		pRangingMeasurementData->RangeDMaxMilliMeter = 0;
		pRangingMeasurementData->RangeFractionalPart = 0;
		pRangingMeasurementData->MeasurementTimeUsec = 0;

		SignalRate =
			VL53L0_FIXPOINT97TOFIXPOINT1616(
			VL53L0_MAKEUINT16(localBuffer[7], localBuffer[6]));
		pRangingMeasurementData->SignalRateRtnMegaCps = SignalRate;

		AmbientRate = VL53L0_MAKEUINT16(localBuffer[9], localBuffer[8]);
		pRangingMeasurementData->AmbientRateRtnMegaCps =
			VL53L0_FIXPOINT97TOFIXPOINT1616(AmbientRate);

		EffectiveSpadRtnCount = VL53L0_MAKEUINT16(localBuffer[3],
			localBuffer[2]);
		pRangingMeasurementData->EffectiveSpadRtnCount =
			EffectiveSpadRtnCount;

		DeviceRangeStatus = localBuffer[0];

		/* initial format = 4.12, when pass to 16.16 from 9.7 we shift
		5 bit more this will be absorbed in the further computation
		*/
		CrosstalkCompensation =
			VL53L0_FIXPOINT97TOFIXPOINT1616(
			VL53L0_MAKEUINT16(localBuffer[13], localBuffer[12]));

		/*
		 * For a standard definition of RangeStatus, this should return
		 * 0 in case of good result after a ranging
		 * The range status depends on the device so call a device
		 * specific	function to obtain the right Status.
		 */
		Status = VL53L0_get_pal_range_status(Dev, DeviceRangeStatus,
			SignalRate, CrosstalkCompensation,
			EffectiveSpadRtnCount,
			pRangingMeasurementData, &PalRangeStatus);

		if (Status == VL53L0_ERROR_NONE)
			pRangingMeasurementData->RangeStatus = PalRangeStatus;

	}

	if (Status == VL53L0_ERROR_NONE) {
		/* Copy last read data into Dev buffer */
		LastRangeDataBuffer = PALDevDataGet(Dev, LastRangeMeasure);

		LastRangeDataBuffer.RangeMilliMeter =
			pRangingMeasurementData->RangeMilliMeter;
		LastRangeDataBuffer.RangeFractionalPart =
			pRangingMeasurementData->RangeFractionalPart;
		LastRangeDataBuffer.RangeDMaxMilliMeter =
			pRangingMeasurementData->RangeDMaxMilliMeter;
		LastRangeDataBuffer.MeasurementTimeUsec =
			pRangingMeasurementData->MeasurementTimeUsec;
		LastRangeDataBuffer.SignalRateRtnMegaCps =
			pRangingMeasurementData->SignalRateRtnMegaCps;
		LastRangeDataBuffer.AmbientRateRtnMegaCps =
			pRangingMeasurementData->AmbientRateRtnMegaCps;
		LastRangeDataBuffer.EffectiveSpadRtnCount =
			pRangingMeasurementData->EffectiveSpadRtnCount;
		LastRangeDataBuffer.RangeStatus =
			pRangingMeasurementData->RangeStatus;

		PALDevDataSet(Dev, LastRangeMeasure, LastRangeDataBuffer);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetHistogramMeasurementData(VL53L0_DEV Dev,
	VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0_Error VL53L0_PerformSingleRangingMeasurement(VL53L0_DEV Dev,
	VL53L0_RangingMeasurementData_t *pRangingMeasurementData)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	/*	This function will do a complete single ranging
	*	Here we fix the mode!
	*/
	Status = VL53L0_SetDeviceMode(Dev, VL53L0_DEVICEMODE_SINGLE_RANGING);

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_PerformSingleMeasurement(Dev);


	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_GetRangingMeasurementData(Dev,
			pRangingMeasurementData);
	}

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_ClearInterruptMask(Dev, 0);


	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0_Error VL53L0_PerformSingleHistogramMeasurement(VL53L0_DEV Dev,
	VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetNumberOfROIZones(VL53L0_DEV Dev, uint8_t
	NumberOfROIZones)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	if (NumberOfROIZones != 1)
		Status = VL53L0_ERROR_INVALID_PARAMS;


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetNumberOfROIZones(VL53L0_DEV Dev, uint8_t*
	pNumberOfROIZones)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	*pNumberOfROIZones = 1;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetMaxNumberOfROIZones(VL53L0_DEV Dev, uint8_t
	*pMaxNumberOfROIZones)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	*pMaxNumberOfROIZones = 1;

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL Measurement Functions */


VL53L0_Error VL53L0_SetGpioConfig(VL53L0_DEV Dev, uint8_t Pin,
	VL53L0_DeviceModes DeviceMode, VL53L0_GpioFunctionality Functionality,
	VL53L0_InterruptPolarity Polarity)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
	uint8_t data;
	LOG_FUNCTION_START("");

	if (Pin != 0)
		Status = VL53L0_ERROR_GPIO_NOT_EXISTING;


	if (Status == VL53L0_ERROR_NONE) {
		switch (Functionality) {
		case VL53L0_GPIOFUNCTIONALITY_OFF:
			data = 0x00;
			break;
		case VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW:
			data = 0x01;
			break;
		case VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH:
			data = 0x02;
			break;
		case VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT:
			data = 0x03;
			break;
		case VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY:
			data = 0x04;
			break;
		default:
			Status = VL53L0_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED;
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_WrByte(Dev,
			VL53L0_REG_SYSTEM_INTERRUPT_CONFIG_GPIO,
			data);
	}

	if (Status == VL53L0_ERROR_NONE) {
		if (Polarity == VL53L0_INTERRUPTPOLARITY_LOW)
			data = 0;
		else
			data = (uint8_t)(1<<4);

		Status = VL53L0_UpdateByte(Dev,
			VL53L0_REG_GPIO_HV_MUX_ACTIVE_HIGH,
			0xEF, data);
	}

	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_SETDEVICESPECIFICPARAMETER(Dev, Pin0GpioFunctionality,
			Functionality);
	}

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_ClearInterruptMask(Dev, 0);


	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetGpioConfig(VL53L0_DEV Dev, uint8_t Pin,
	VL53L0_DeviceModes *DeviceMode,
	VL53L0_GpioFunctionality *pFunctionality,
	VL53L0_InterruptPolarity *pPolarity)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
	VL53L0_GpioFunctionality GpioFunctionality;
	uint8_t data;
	LOG_FUNCTION_START("");

	if (Pin != 0) {
		Status = VL53L0_ERROR_GPIO_NOT_EXISTING;
	} else {
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_SYSTEM_INTERRUPT_CONFIG_GPIO,
			&data);
	}

	if (Status == VL53L0_ERROR_NONE) {
		switch (data&0x07) {
		case 0x00:
		    GpioFunctionality = VL53L0_GPIOFUNCTIONALITY_OFF;
		    break;
		case 0x01:
			GpioFunctionality =
				VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW;
		    break;
		case 0x02:
			GpioFunctionality =
				VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH;
		    break;
		case 0x03:
			GpioFunctionality =
				VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT;
		    break;
		case 0x04:
		    GpioFunctionality =
				VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
		    break;
		default:
		    Status = VL53L0_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED;
		}
	}

	if (Status == VL53L0_ERROR_NONE) {
		*pFunctionality = GpioFunctionality;
		VL53L0_SETDEVICESPECIFICPARAMETER(Dev, Pin0GpioFunctionality,
			GpioFunctionality);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetInterruptThresholds(VL53L0_DEV Dev, VL53L0_DeviceModes
	DeviceMode, FixPoint1616_t ThresholdLow, FixPoint1616_t ThresholdHigh)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint16_t Threshold16;
	LOG_FUNCTION_START("");

	/* no dependency on DeviceMode for Ewok */

	Threshold16 = (uint16_t)((ThresholdLow >> 16) & 0x00fff);
	Status = VL53L0_WrWord(Dev, VL53L0_REG_SYSTEM_THRESH_LOW, Threshold16);

	if (Status == VL53L0_ERROR_NONE) {
		Threshold16 = (uint16_t)((ThresholdHigh >> 16) & 0x00fff);
		Status = VL53L0_WrWord(Dev, VL53L0_REG_SYSTEM_THRESH_HIGH,
			Threshold16);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetInterruptThresholds(VL53L0_DEV Dev, VL53L0_DeviceModes
	DeviceMode, FixPoint1616_t *pThresholdLow,
	FixPoint1616_t *pThresholdHigh)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint16_t Threshold16;
	LOG_FUNCTION_START("");

	/* no dependency on DeviceMode for Ewok */

	Status = VL53L0_RdWord(Dev, VL53L0_REG_SYSTEM_THRESH_LOW, &Threshold16);
	*pThresholdLow = (FixPoint1616_t)((0x00fff & Threshold16)<<16);
	/* 12 bit */

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdWord(Dev, VL53L0_REG_SYSTEM_THRESH_HIGH,
			&Threshold16);
		*pThresholdHigh = (FixPoint1616_t)(
			(0x00fff & Threshold16) << 16); /* 12 bit */
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/* Group PAL Interrupt Functions */
VL53L0_Error VL53L0_ClearInterruptMask(VL53L0_DEV Dev, uint32_t InterruptMask)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t LoopCount;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	/* clear bit 0 range interrupt, bit 1 error interrupt */
	Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
	LoopCount = 0;
	do {
		VL53L0_RdByte(Dev, VL53L0_REG_RESULT_INTERRUPT_STATUS, &Byte);
		LoopCount++;
	} while (((Byte & 0x07) != 0x00) && (LoopCount < 8));
	Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_INTERRUPT_CLEAR, 0x00);
	/* clear all */

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetInterruptMaskStatus(VL53L0_DEV Dev, uint32_t
	*pInterruptMaskStatus)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_RESULT_INTERRUPT_STATUS, &Byte);
	*pInterruptMaskStatus = Byte & 0x07;

	if (Byte & 0x18) {
		Status = VL53L0_ERROR_RANGE_ERROR; /* check if some error
		occurs */
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_EnableInterruptMask(VL53L0_DEV Dev, uint32_t InterruptMask)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	/* not implemented for VL53L0 */

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL Interrupt Functions */

/* Group SPAD functions */

VL53L0_Error VL53L0_SetSpadAmbientDamperThreshold(VL53L0_DEV Dev, uint16_t
	SpadAmbientDamperThreshold)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status = VL53L0_WrWord(Dev, 0x40, SpadAmbientDamperThreshold);
	VL53L0_WrByte(Dev, 0xFF, 0x00);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSpadAmbientDamperThreshold(VL53L0_DEV Dev, uint16_t
	*pSpadAmbientDamperThreshold)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status = VL53L0_RdWord(Dev, 0x40, pSpadAmbientDamperThreshold);
	VL53L0_WrByte(Dev, 0xFF, 0x00);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_SetSpadAmbientDamperFactor(VL53L0_DEV Dev, uint16_t
	SpadAmbientDamperFactor)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	Byte = (uint8_t) (SpadAmbientDamperFactor & 0x00FF);

	VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status = VL53L0_WrByte(Dev, 0x42, Byte);
	VL53L0_WrByte(Dev, 0xFF, 0x00);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_GetSpadAmbientDamperFactor(VL53L0_DEV Dev, uint16_t
	*pSpadAmbientDamperFactor)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t Byte;
	LOG_FUNCTION_START("");

	VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status = VL53L0_RdByte(Dev, 0x42, &Byte);
	VL53L0_WrByte(Dev, 0xFF, 0x00);
	*pSpadAmbientDamperFactor = (uint16_t) Byte;

	LOG_FUNCTION_END(Status);
	return Status;
}

/* END Group SPAD functions */

/*//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////
// Internal functions
////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////
*/
static uint32_t VL53L0_calc_macro_period_ps(VL53L0_DEV Dev,
		uint8_t vcsel_period);
static uint16_t VL53L0_encode_timeout(uint32_t timeout_mclks);
static uint32_t VL53L0_decode_timeout(uint16_t encoded_timeout);

static VL53L0_Error VL53L0_get_vcsel_pulse_period(VL53L0_DEV Dev, uint8_t
	*pVCSELPulsePeriod, uint8_t RangeIndex)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t vcsel_period_reg;
	LOG_FUNCTION_START("");

	switch (RangeIndex) {
	case 0:
		Status = VL53L0_RdByte(Dev, VL53L0_REG_RNGA_CONFIG_VCSEL_PERIOD,
			&vcsel_period_reg);
		break;
	case 1:
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_RNGB1_CONFIG_VCSEL_PERIOD,
			&vcsel_period_reg);
		break;
	case 2:
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_RNGB2_CONFIG_VCSEL_PERIOD,
			&vcsel_period_reg);
		break;
	default:
		Status = VL53L0_RdByte(Dev, VL53L0_REG_RNGA_CONFIG_VCSEL_PERIOD,
			&vcsel_period_reg);
	}

	if (Status == VL53L0_ERROR_NONE) {
		*pVCSELPulsePeriod =
			VL53L0_decode_vcsel_period(vcsel_period_reg);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

/* To convert ms into register value */
static uint16_t VL53L0_calc_encoded_timeout(VL53L0_DEV Dev, uint32_t
	timeout_period_us, uint8_t vcsel_period)
{
	uint32_t macro_period_ps;
	uint32_t macro_period_ns;
	uint32_t timeout_period_mclks = 0;
	uint16_t timeout_overall_periods = 0;

	macro_period_ps = VL53L0_calc_macro_period_ps(Dev, vcsel_period);
	macro_period_ns = macro_period_ps / 1000;

	timeout_period_mclks = (uint32_t) (((timeout_period_us * 1000) +
		(macro_period_ns / 2)) / macro_period_ns);
	timeout_overall_periods = VL53L0_encode_timeout(timeout_period_mclks);

	return timeout_overall_periods;
}

/* To convert register value into us */
static uint32_t VL53L0_calc_ranging_wait_us(VL53L0_DEV Dev, uint16_t
	timeout_overall_periods, uint8_t vcsel_period)
{
	uint32_t macro_period_ps;
	uint32_t macro_period_ns;
	uint32_t timeout_period_mclks = 0;
	uint32_t actual_timeout_period_us = 0;

	macro_period_ps = VL53L0_calc_macro_period_ps(Dev, vcsel_period);
	macro_period_ns = macro_period_ps / 1000;

	timeout_period_mclks = VL53L0_decode_timeout(timeout_overall_periods);
	actual_timeout_period_us = ((timeout_period_mclks * macro_period_ns) +
		(macro_period_ns / 2)) / 1000;

	return actual_timeout_period_us;
}

static uint32_t VL53L0_calc_macro_period_ps(VL53L0_DEV Dev,
		uint8_t vcsel_period)
{
	uint32_t PLL_multiplier;
	uint64_t PLL_period_ps;
	uint8_t vcsel_period_pclks;
	uint32_t macro_period_vclks;
	uint32_t macro_period_ps;

	LOG_FUNCTION_START("");

	PLL_multiplier = 65536 / 64; /* PLL multiplier is 64 */
	PLL_period_ps = (1000 * 1000 * PLL_multiplier) /
		VL53L0_GETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz);

	vcsel_period_pclks = VL53L0_decode_vcsel_period(vcsel_period);

	macro_period_vclks = 2304;
	macro_period_ps = (uint32_t)(macro_period_vclks * vcsel_period_pclks *
		PLL_period_ps);

	LOG_FUNCTION_END("");
	return macro_period_ps;
}

static uint8_t VL53L0_decode_vcsel_period(uint8_t vcsel_period_reg)
{

	/*!
	 * Converts the encoded VCSEL period register value into the real
	 * period in PLL clocks
	 */

	uint8_t vcsel_period_pclks = 0;

	vcsel_period_pclks = (vcsel_period_reg + 1) << 1;

	return vcsel_period_pclks;
}

static uint8_t VL53L0_encode_vcsel_period(uint8_t vcsel_period_pclks)
{

	/*!
	 * Converts the encoded VCSEL period register value into the real
	 * period in PLL clocks
	 */

	uint8_t vcsel_period_reg = 0;

	vcsel_period_reg = (vcsel_period_pclks >> 1) - 1;

	return vcsel_period_reg;
}

static uint16_t VL53L0_encode_timeout(uint32_t timeout_mclks)
{
    /*!
     * Encode timeout in macro periods in (LSByte * 2^MSByte) + 1 format
     *
     */

	uint16_t encoded_timeout = 0;
	uint32_t ls_byte = 0;
	uint16_t ms_byte = 0;

	if (timeout_mclks > 0) {
		ls_byte = timeout_mclks - 1;

		while ((ls_byte & 0xFFFFFF00) > 0) {
			ls_byte = ls_byte >> 1;
			ms_byte++;
		}

		encoded_timeout = (ms_byte << 8) + (uint16_t) (ls_byte &
			0x000000FF);

	}

	return encoded_timeout;

}

static uint32_t VL53L0_decode_timeout(uint16_t encoded_timeout)
{
	/*!
	 * Decode 16-bit timeout register value - format (LSByte * 2^MSByte) + 1
	 *
	 */

	uint32_t timeout_mclks = 0;

	timeout_mclks = ((uint32_t) (encoded_timeout & 0x00FF) << (uint32_t)
		((encoded_timeout & 0xFF00) >> 8)) + 1;

	return timeout_mclks;

}


static VL53L0_Error VL53L0_load_additional_settings1(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	/* update 12_05_15_v6 */
	/* OSCT */
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x14, 0x01);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xCD, 0x6C);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x86, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x87, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

	/* update 12_05_15_v6 */
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xcd, 0x6c);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x90, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x91, 0x3f);
	Status |= VL53L0_WrByte(Dev, 0x92, 0x3f);
	Status |= VL53L0_WrByte(Dev, 0x88, 0x2b);
	Status |= VL53L0_WrByte(Dev, 0x89, 0x03);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xcd, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

	/* update 12_05_15 */
	Status |= VL53L0_WrByte(Dev, 0xb0, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xb1, 0xfc);
	Status |= VL53L0_WrByte(Dev, 0xb2, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xb3, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xb4, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xb5, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xb6, 0xb0);

	Status |= VL53L0_WrByte(Dev, 0x32, 0x03);

	Status |= VL53L0_WrByte(Dev, 0x41, 0xff);
	Status |= VL53L0_WrByte(Dev, 0x42, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x43, 0x01);

	Status |= VL53L0_WrByte(Dev, 0x01, 0x01);

	if (Status != 0)
		Status = VL53L0_ERROR_CONTROL_INTERFACE;

	LOG_FUNCTION_END(Status);
	return Status;
}


static VL53L0_Error VL53L0_load_additional_settings3(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	LOG_FUNCTION_START("");

	/* update 150624_b */

	Status |= VL53L0_WrByte(Dev, 0xff, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);

	Status |= VL53L0_WrByte(Dev, 0xff, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);

	Status |= VL53L0_WrByte(Dev, 0xff, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x4f, 0x0B);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x0E);

	Status |= VL53L0_WrByte(Dev, 0x00, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x01, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x02, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0x03, 0x0D);
	Status |= VL53L0_WrByte(Dev, 0x04, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x05, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x06, 0x06);
	Status |= VL53L0_WrByte(Dev, 0x07, 0x47);
	Status |= VL53L0_WrByte(Dev, 0x08, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x09, 0x20);
	Status |= VL53L0_WrByte(Dev, 0x0A, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x0B, 0x49);
	Status |= VL53L0_WrByte(Dev, 0x0C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x0D, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x0E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x0F, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x10, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x11, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0x12, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x13, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x14, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x15, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x16, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x17, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x18, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x19, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x1A, 0x11);
	Status |= VL53L0_WrByte(Dev, 0x1B, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x1C, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x1D, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x1E, 0x11);
	Status |= VL53L0_WrByte(Dev, 0x1F, 0x08);
	Status |= VL53L0_WrByte(Dev, 0x20, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x21, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x22, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x23, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x24, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0x25, 0x0D);
	Status |= VL53L0_WrByte(Dev, 0x26, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x27, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x28, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x29, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x2A, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x2B, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x2C, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x2D, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x2E, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x2F, 0x92);
	Status |= VL53L0_WrByte(Dev, 0x30, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x31, 0x64);
	Status |= VL53L0_WrByte(Dev, 0x32, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x33, 0x8A);
	Status |= VL53L0_WrByte(Dev, 0x34, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x35, 0xE0);
	Status |= VL53L0_WrByte(Dev, 0x36, 0x0F);
	Status |= VL53L0_WrByte(Dev, 0x37, 0xAA);
	Status |= VL53L0_WrByte(Dev, 0x38, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x39, 0xE4);
	Status |= VL53L0_WrByte(Dev, 0x3A, 0x0F);
	Status |= VL53L0_WrByte(Dev, 0x3B, 0xAE);
	Status |= VL53L0_WrByte(Dev, 0x3C, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x3D, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x3E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x3F, 0x54);
	Status |= VL53L0_WrByte(Dev, 0x40, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x41, 0x88);
	Status |= VL53L0_WrByte(Dev, 0x42, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x43, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x44, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x45, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x46, 0x06);
	Status |= VL53L0_WrByte(Dev, 0x47, 0x87);
	Status |= VL53L0_WrByte(Dev, 0x48, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x49, 0x38);
	Status |= VL53L0_WrByte(Dev, 0x4A, 0x2B);
	Status |= VL53L0_WrByte(Dev, 0x4B, 0x89);
	Status |= VL53L0_WrByte(Dev, 0x4C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x4D, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x4E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x4F, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x50, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x51, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0x52, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x53, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x54, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x55, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x56, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x57, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x58, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x59, 0x0D);
	Status |= VL53L0_WrByte(Dev, 0x5A, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x5B, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x5C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x5D, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x5E, 0x0D);
	Status |= VL53L0_WrByte(Dev, 0x5F, 0x67);
	Status |= VL53L0_WrByte(Dev, 0x60, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x61, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x62, 0x0D);
	Status |= VL53L0_WrByte(Dev, 0x63, 0xB0);
	Status |= VL53L0_WrByte(Dev, 0x64, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x65, 0x20);
	Status |= VL53L0_WrByte(Dev, 0x66, 0x29);
	Status |= VL53L0_WrByte(Dev, 0x67, 0xC1);
	Status |= VL53L0_WrByte(Dev, 0x68, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x69, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x6A, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x6B, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x6C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x6D, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x6E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x6F, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x70, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x71, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0x72, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x73, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x74, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x75, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x76, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x77, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x78, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x79, 0x0D);
	Status |= VL53L0_WrByte(Dev, 0x7A, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x7B, 0x1B);
	Status |= VL53L0_WrByte(Dev, 0x7C, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x7D, 0x82);
	Status |= VL53L0_WrByte(Dev, 0x7E, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x7F, 0x24);
	Status |= VL53L0_WrByte(Dev, 0x80, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x81, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x82, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x83, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x84, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x85, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x86, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x87, 0x21);
	Status |= VL53L0_WrByte(Dev, 0x88, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x89, 0x58);
	Status |= VL53L0_WrByte(Dev, 0x8A, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x8B, 0xCC);
	Status |= VL53L0_WrByte(Dev, 0x8C, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x8D, 0xC3);
	Status |= VL53L0_WrByte(Dev, 0x8E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x8F, 0x94);
	Status |= VL53L0_WrByte(Dev, 0x90, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x91, 0x53);
	Status |= VL53L0_WrByte(Dev, 0x92, 0x1E);
	Status |= VL53L0_WrByte(Dev, 0x93, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x94, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x95, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x96, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x97, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x98, 0x20);
	Status |= VL53L0_WrByte(Dev, 0x99, 0x20);
	Status |= VL53L0_WrByte(Dev, 0x9A, 0x08);
	Status |= VL53L0_WrByte(Dev, 0x9B, 0x10);
	Status |= VL53L0_WrByte(Dev, 0x9C, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x9D, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x9E, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x9F, 0x50);
	Status |= VL53L0_WrByte(Dev, 0xA0, 0x2B);
	Status |= VL53L0_WrByte(Dev, 0xA1, 0xB1);
	Status |= VL53L0_WrByte(Dev, 0xA2, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xA3, 0x02);
	Status |= VL53L0_WrByte(Dev, 0xA4, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xA5, 0x50);
	Status |= VL53L0_WrByte(Dev, 0xA6, 0x2C);
	Status |= VL53L0_WrByte(Dev, 0xA7, 0x11);
	Status |= VL53L0_WrByte(Dev, 0xA8, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xA9, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xAA, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xAB, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xAC, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xAD, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0xAE, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xAF, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0xB0, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xB1, 0x04);
	Status |= VL53L0_WrByte(Dev, 0xB2, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xB3, 0x4E);
	Status |= VL53L0_WrByte(Dev, 0xB4, 0x2D);
	Status |= VL53L0_WrByte(Dev, 0xB5, 0x47);
	Status |= VL53L0_WrByte(Dev, 0xB6, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xB7, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xB8, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xB9, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xBA, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xBB, 0xA7);
	Status |= VL53L0_WrByte(Dev, 0xBC, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xBD, 0xA6);
	Status |= VL53L0_WrByte(Dev, 0xBE, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xBF, 0x02);
	Status |= VL53L0_WrByte(Dev, 0xC0, 0x04);
	Status |= VL53L0_WrByte(Dev, 0xC1, 0x30);
	Status |= VL53L0_WrByte(Dev, 0xC2, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xC3, 0x04);
	Status |= VL53L0_WrByte(Dev, 0xC4, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xC5, 0x60);
	Status |= VL53L0_WrByte(Dev, 0xC6, 0x2D);
	Status |= VL53L0_WrByte(Dev, 0xC7, 0x89);
	Status |= VL53L0_WrByte(Dev, 0xC8, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xC9, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xCA, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xCB, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xCC, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xCD, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0xCE, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xCF, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0xD0, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xD1, 0x04);
	Status |= VL53L0_WrByte(Dev, 0xD2, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xD3, 0x25);
	Status |= VL53L0_WrByte(Dev, 0xD4, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xD5, 0x2E);
	Status |= VL53L0_WrByte(Dev, 0xD6, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xD7, 0x25);
	Status |= VL53L0_WrByte(Dev, 0xD8, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xD9, 0x2E);
	Status |= VL53L0_WrByte(Dev, 0xDA, 0x03);
	Status |= VL53L0_WrByte(Dev, 0xDB, 0xF3);
	Status |= VL53L0_WrByte(Dev, 0xDC, 0x03);
	Status |= VL53L0_WrByte(Dev, 0xDD, 0xEA);
	Status |= VL53L0_WrByte(Dev, 0xDE, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xDF, 0x58);
	Status |= VL53L0_WrByte(Dev, 0xE0, 0x2C);
	Status |= VL53L0_WrByte(Dev, 0xE1, 0xD9);
	Status |= VL53L0_WrByte(Dev, 0xE2, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xE3, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xE4, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xE5, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xE6, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xE7, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0xE8, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xE9, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0xEA, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xEB, 0x04);
	Status |= VL53L0_WrByte(Dev, 0xEC, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xED, 0x26);
	Status |= VL53L0_WrByte(Dev, 0xEE, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xEF, 0xDC);
	Status |= VL53L0_WrByte(Dev, 0xF0, 0x28);
	Status |= VL53L0_WrByte(Dev, 0xF1, 0x58);
	Status |= VL53L0_WrByte(Dev, 0xF2, 0x2F);
	Status |= VL53L0_WrByte(Dev, 0xF3, 0x21);
	Status |= VL53L0_WrByte(Dev, 0xF4, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xF5, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xF6, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xF7, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xF8, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xF9, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0xFA, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFB, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0xFC, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFD, 0x04);
	Status |= VL53L0_WrWord(Dev, 0xFE, 0x01E3);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x0F);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x01, 0x48);
	Status |= VL53L0_WrByte(Dev, 0x02, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x03, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x04, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x05, 0xA4);
	Status |= VL53L0_WrByte(Dev, 0x06, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x07, 0xB8);
	Status |= VL53L0_WrByte(Dev, 0x08, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x09, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x0A, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x0B, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x0C, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x0D, 0x6B);
	Status |= VL53L0_WrByte(Dev, 0x0E, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x0F, 0x64);
	Status |= VL53L0_WrByte(Dev, 0x10, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x11, 0x3C);
	Status |= VL53L0_WrByte(Dev, 0x12, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x13, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x14, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x15, 0x74);
	Status |= VL53L0_WrByte(Dev, 0x16, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x17, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x18, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x19, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x1A, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x1B, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x1C, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x1D, 0xA2);
	Status |= VL53L0_WrByte(Dev, 0x1E, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x1F, 0x8E);
	Status |= VL53L0_WrByte(Dev, 0x20, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x21, 0x50);
	Status |= VL53L0_WrByte(Dev, 0x22, 0x2E);
	Status |= VL53L0_WrByte(Dev, 0x23, 0xC9);
	Status |= VL53L0_WrByte(Dev, 0x24, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x25, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x26, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x27, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x28, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x29, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0x2A, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x2B, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x2C, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x2D, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x2E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x2F, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x30, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x31, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x32, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x33, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x34, 0x11);
	Status |= VL53L0_WrByte(Dev, 0x35, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x36, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x37, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x38, 0x11);
	Status |= VL53L0_WrByte(Dev, 0x39, 0x08);
	Status |= VL53L0_WrByte(Dev, 0x3A, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x3B, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x3C, 0x11);
	Status |= VL53L0_WrByte(Dev, 0x3D, 0x18);
	Status |= VL53L0_WrByte(Dev, 0x3E, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x3F, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x40, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x41, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x42, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x43, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x44, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0x45, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x46, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x47, 0x08);
	Status |= VL53L0_WrByte(Dev, 0x48, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0x49, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x4A, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x4B, 0x40);
	Status |= VL53L0_WrByte(Dev, 0x4C, 0x2F);
	Status |= VL53L0_WrByte(Dev, 0x4D, 0xD1);
	Status |= VL53L0_WrByte(Dev, 0x4E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x4F, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x50, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x51, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x52, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x53, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0x54, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x55, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x56, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x57, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x58, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x59, 0x11);
	Status |= VL53L0_WrByte(Dev, 0x5A, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x5B, 0x48);
	Status |= VL53L0_WrByte(Dev, 0x5C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x5D, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x5E, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0x5F, 0xA2);
	Status |= VL53L0_WrByte(Dev, 0x60, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x61, 0x60);
	Status |= VL53L0_WrByte(Dev, 0x62, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x63, 0x3E);
	Status |= VL53L0_WrByte(Dev, 0x64, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x65, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x66, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x67, 0x54);
	Status |= VL53L0_WrByte(Dev, 0x68, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x69, 0x80);
	Status |= VL53L0_WrByte(Dev, 0x6A, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x6B, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x6C, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x6D, 0x38);
	Status |= VL53L0_WrByte(Dev, 0x6E, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x6F, 0xE1);
	Status |= VL53L0_WrByte(Dev, 0x70, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x71, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x72, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x73, 0x38);
	Status |= VL53L0_WrByte(Dev, 0x74, 0x29);
	Status |= VL53L0_WrByte(Dev, 0x75, 0x21);
	Status |= VL53L0_WrByte(Dev, 0x76, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x77, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x78, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x79, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x7A, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x7B, 0xA1);
	Status |= VL53L0_WrByte(Dev, 0x7C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x7D, 0xA0);
	Status |= VL53L0_WrByte(Dev, 0x7E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x7F, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x80, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x81, 0x33);
	Status |= VL53L0_WrByte(Dev, 0x82, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x83, 0x6A);
	Status |= VL53L0_WrByte(Dev, 0x84, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x85, 0x61);
	Status |= VL53L0_WrByte(Dev, 0x86, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x87, 0xF9);
	Status |= VL53L0_WrByte(Dev, 0x88, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x89, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x8A, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x8B, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x8C, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x8D, 0x09);
	Status |= VL53L0_WrByte(Dev, 0x8E, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x8F, 0x03);
	Status |= VL53L0_WrByte(Dev, 0x90, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x91, 0x66);
	Status |= VL53L0_WrByte(Dev, 0x92, 0x2A);
	Status |= VL53L0_WrByte(Dev, 0x93, 0x67);
	Status |= VL53L0_WrByte(Dev, 0x94, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x95, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x96, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x97, 0x66);
	Status |= VL53L0_WrByte(Dev, 0x98, 0x2A);
	Status |= VL53L0_WrByte(Dev, 0x99, 0xAF);
	Status |= VL53L0_WrByte(Dev, 0x9A, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x9B, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x9C, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x9D, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x9E, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x9F, 0xA7);
	Status |= VL53L0_WrByte(Dev, 0xA0, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xA1, 0xA6);
	Status |= VL53L0_WrByte(Dev, 0xA2, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xA3, 0x04);

	Status |= VL53L0_WrByte(Dev, 0xff, 0x04);

	Status |= VL53L0_WrByte(Dev, 0x79, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0x7B, 0x16);
	Status |= VL53L0_WrByte(Dev, 0x7D, 0x2B);
	Status |= VL53L0_WrByte(Dev, 0x7F, 0x3B);
	Status |= VL53L0_WrByte(Dev, 0x81, 0x59);
	Status |= VL53L0_WrByte(Dev, 0x83, 0x62);
	Status |= VL53L0_WrByte(Dev, 0x85, 0x69);
	Status |= VL53L0_WrByte(Dev, 0x87, 0x76);
	Status |= VL53L0_WrByte(Dev, 0x89, 0x7F);
	Status |= VL53L0_WrByte(Dev, 0x8B, 0x98);
	Status |= VL53L0_WrByte(Dev, 0x8D, 0xAC);
	Status |= VL53L0_WrByte(Dev, 0x8F, 0xC0);
	Status |= VL53L0_WrByte(Dev, 0x90, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0x91, 0x30);
	Status |= VL53L0_WrByte(Dev, 0x92, 0x28);
	Status |= VL53L0_WrByte(Dev, 0x93, 0x02);
	Status |= VL53L0_WrByte(Dev, 0x94, 0x37);
	Status |= VL53L0_WrByte(Dev, 0x95, 0x62);

	Status |= VL53L0_WrByte(Dev, 0x96, 0x04);
	Status |= VL53L0_WrByte(Dev, 0x97, 0x08);
	Status |= VL53L0_WrByte(Dev, 0x98, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x99, 0x18);
	Status |= VL53L0_WrByte(Dev, 0x9A, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x9B, 0x6F);
	Status |= VL53L0_WrByte(Dev, 0x9C, 0x05);
	Status |= VL53L0_WrByte(Dev, 0x9D, 0xD4);
	Status |= VL53L0_WrByte(Dev, 0x9E, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0x9F, 0x6E);
	Status |= VL53L0_WrByte(Dev, 0xA0, 0x09);
	Status |= VL53L0_WrByte(Dev, 0xA1, 0xA2);
	Status |= VL53L0_WrByte(Dev, 0xA2, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0xA3, 0xAA);
	Status |= VL53L0_WrByte(Dev, 0xA4, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xA5, 0x97);
	Status |= VL53L0_WrByte(Dev, 0xA6, 0x0B);
	Status |= VL53L0_WrByte(Dev, 0xA7, 0xD8);
	Status |= VL53L0_WrByte(Dev, 0xA8, 0x0A);
	Status |= VL53L0_WrByte(Dev, 0xA9, 0xD7);
	Status |= VL53L0_WrByte(Dev, 0xAA, 0x08);
	Status |= VL53L0_WrByte(Dev, 0xAB, 0xF6);
	Status |= VL53L0_WrByte(Dev, 0xAC, 0x07);
	Status |= VL53L0_WrByte(Dev, 0xAD, 0x1A);
	Status |= VL53L0_WrByte(Dev, 0xAE, 0x0C);
	Status |= VL53L0_WrByte(Dev, 0xAF, 0x49);
	Status |= VL53L0_WrByte(Dev, 0xB0, 0x09);
	Status |= VL53L0_WrByte(Dev, 0xB1, 0x17);
	Status |= VL53L0_WrByte(Dev, 0xB2, 0x03);
	Status |= VL53L0_WrByte(Dev, 0xB3, 0xCD);
	Status |= VL53L0_WrByte(Dev, 0xB4, 0x04);
	Status |= VL53L0_WrByte(Dev, 0xB5, 0x55);

	Status |= VL53L0_WrByte(Dev, 0x72, 0xFF);
	Status |= VL53L0_WrByte(Dev, 0x73, 0xFF);

	Status |= VL53L0_WrByte(Dev, 0x74, 0xE0);

	Status |= VL53L0_WrByte(Dev, 0x70, 0x01);

	Status |= VL53L0_WrByte(Dev, 0xff, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xff, 0x00);

	if (Status != 0)
		Status = VL53L0_ERROR_CONTROL_INTERFACE;

	LOG_FUNCTION_END(Status);
	return Status;
}

static VL53L0_Error VL53L0_check_part_used(VL53L0_DEV Dev, uint8_t *Revision)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t ModuleIdInt;

	LOG_FUNCTION_START("");

	Status = VL53L0_RdByte(Dev, VL53L0_REG_IDENTIFICATION_MODULE_ID,
		&ModuleIdInt);

	if (Status == VL53L0_ERROR_NONE) {
		if (ModuleIdInt == 0)
			Revision = 0;
		else
			Status = VL53L0_get_info_from_device(Dev, Revision);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

static VL53L0_Error VL53L0_get_info_from_device(VL53L0_DEV Dev,
		uint8_t *Revision)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t strobe;
	uint8_t byte;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x06);
	Status |= VL53L0_RdByte(Dev, 0x83, &byte);
	Status |= VL53L0_WrByte(Dev, 0x83, byte|4);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x07);
	Status |= VL53L0_WrByte(Dev, 0x81, 0x01);

	Status |= VL53L0_PollingDelay(Dev);

	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);

	Status |= VL53L0_WrByte(Dev, 0x94, 0x7B);

	Status |= VL53L0_WrByte(Dev, 0x83, 0x00);

	/*	polling
	*	use timeout to avoid deadlock
	*/
	if (Status == VL53L0_ERROR_NONE) {
		LoopNb = 0;
		do {
			Status = VL53L0_RdByte(Dev, 0x83, &strobe);
			if ((strobe != 0x00) || Status != VL53L0_ERROR_NONE)
				break;

			LoopNb = LoopNb + 1;
		} while (LoopNb < VL53L0_DEFAULT_MAX_LOOP);

		if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP)
			Status = VL53L0_ERROR_TIME_OUT;

	}

	Status |= VL53L0_WrByte(Dev, 0x83, 0x01);
	Status |= VL53L0_RdByte(Dev, 0x90, Revision);

	Status |= VL53L0_WrByte(Dev, 0x83, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x81, 0x00);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x06);
	Status |= VL53L0_RdByte(Dev, 0x83, &byte);
	Status |= VL53L0_WrByte(Dev, 0x83, byte&0xfb);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_WrByte(Dev, 0x00, 0x01);

	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);

	LOG_FUNCTION_END(Status);
	return Status;
}

uint32_t VL53L0_isqrt(uint32_t num)
{

    /*
     * Implements an integer square root
     *
     * From: http://en.wikipedia.org/wiki/Methods_of_computing_square_roots
     */

	uint32_t  res = 0;
	uint32_t  bit = 1 << 30;
	/* The second-to-top bit is set: 1 << 14 for
	16-bits, 1 << 30 for 32 bits
	*/
    /* "bit" starts at the highest power of four <= the argument.*/
	while (bit > num)
		bit >>= 2;


	while (bit != 0) {
		if (num >= res + bit) {
			num -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;

		bit >>= 2;
	}

	return res;
}

uint32_t VL53L0_quadrature_sum(uint32_t a,
	uint32_t b)
{
	/*
	* Implements a quadrature sum
	*
	* rea = sqrt(a^2 + b^2)
	*
	* Trap overflow case max input value is 65535 (16-bit value)
	* as internal calc are 32-bit wide
	*
	* If overflow then seta output to maximum
	*/
	uint32_t  res = 0;

	if (a > 65535 || b > 65535)
		res = 65535;
	else
		res = VL53L0_isqrt(a*a + b*b);


	return res;
}



VL53L0_Error VL53L0_get_jmp_vcsel_ambient_rate(VL53L0_DEV Dev,
	uint32_t *pAmbient_rate_kcps,
	uint32_t *pVcsel_rate_kcps,
	uint32_t *pSignalTotalEventsRtn)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint16_t encodedTimeOut;

	uint32_t    total_periods_elapsed_rtn__macrop  = 0;
	uint32_t    result_core__total_periods_elapsed_rtn  = 0;
	uint32_t    rngb1_config__timeout__macrop = 0;
	uint32_t    rngb2_config__timeout__macrop = 0;
	uint32_t    result_core__ambient_window_events_rtn = 0;
	uint32_t     result_core__signal_total_events_rtn = 0;
	uint8_t     last_woi_period;
	uint8_t     rnga_config__vcsel_period;
	uint8_t     rngb1_config__vcsel_period;
	uint8_t     rngb2_config__vcsel_period;
	uint8_t     global_config__vcsel_width;

	uint32_t    ambient_duration_us = 0;
	uint32_t    vcsel_duration_us = 0;

	uint32_t    pll_period_us  = 0;

	LOG_FUNCTION_START("");

	/* read the following */
	Status = VL53L0_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0_RdDWord(Dev, 0xC8,
		&result_core__total_periods_elapsed_rtn);
	Status |= VL53L0_RdDWord(Dev, 0xF0, &pll_period_us);
	Status |= VL53L0_RdDWord(Dev, 0xbc,
		&result_core__ambient_window_events_rtn);
	Status |= VL53L0_RdDWord(Dev, 0xc4,
		&result_core__signal_total_events_rtn);
	Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);


	if (Status == VL53L0_ERROR_NONE) {
		result_core__total_periods_elapsed_rtn =
			(int32_t)(result_core__total_periods_elapsed_rtn &
			0x00ffffff);
		pll_period_us = (int32_t)(pll_period_us & 0x3ffff);
	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdWord(Dev, VL53L0_REG_RNGB1_TIMEOUT_MSB,
			&encodedTimeOut);
	if (Status == VL53L0_ERROR_NONE)
		rngb1_config__timeout__macrop =
			VL53L0_decode_timeout(encodedTimeOut) - 1;

	}

	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev, VL53L0_REG_RNGA_CONFIG_VCSEL_PERIOD,
			&rnga_config__vcsel_period);
	}
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_RNGB1_CONFIG_VCSEL_PERIOD,
			&rngb1_config__vcsel_period);
	}
	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdByte(Dev,
			VL53L0_REG_RNGB2_CONFIG_VCSEL_PERIOD,
			&rngb2_config__vcsel_period);
	}
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_RdByte(Dev, 0x32, &global_config__vcsel_width);


	if (Status == VL53L0_ERROR_NONE) {
		Status = VL53L0_RdWord(Dev, VL53L0_REG_RNGB2_TIMEOUT_MSB,
			&encodedTimeOut);
		if (Status == VL53L0_ERROR_NONE)
			rngb2_config__timeout__macrop =
				VL53L0_decode_timeout(encodedTimeOut) - 1;

	}

	if (Status == VL53L0_ERROR_NONE) {
		total_periods_elapsed_rtn__macrop =
			result_core__total_periods_elapsed_rtn + 1;

		if (result_core__total_periods_elapsed_rtn ==
			rngb1_config__timeout__macrop) {
			last_woi_period = rngb1_config__vcsel_period;
		} else if (result_core__total_periods_elapsed_rtn ==
			rngb2_config__timeout__macrop) {
			last_woi_period = rngb2_config__vcsel_period;
		} else {
			last_woi_period = rnga_config__vcsel_period;

		}
		/* 512 = 1<<9  ==> 24-9=15 */
		ambient_duration_us = last_woi_period *
			total_periods_elapsed_rtn__macrop * pll_period_us;
		ambient_duration_us = ambient_duration_us / 1000;

		*pAmbient_rate_kcps = ((1 << 15) *
			result_core__ambient_window_events_rtn) /
			ambient_duration_us;

		/* 2048 = 1<<11  ==> 24-11=13 */
		vcsel_duration_us = (10*global_config__vcsel_width + 4) *
			total_periods_elapsed_rtn__macrop * pll_period_us ;
		vcsel_duration_us = vcsel_duration_us / 10000 ;

		*pVcsel_rate_kcps = ((1 << 13) *
			result_core__signal_total_events_rtn)
			/ vcsel_duration_us;
		*pSignalTotalEventsRtn = result_core__signal_total_events_rtn;
	}

	LOG_FUNCTION_END(Status);
	return Status;

}

VL53L0_Error VL53L0_calc_sigma_estimate(VL53L0_DEV Dev,
	VL53L0_RangingMeasurementData_t
	*pRangingMeasurementData,
	FixPoint1616_t *pSigmaEstimate)
{
	const uint32_t cPulseEffectiveWidth_centi_ns   = 800;
	/* Expressed in 100ths of a ns, i.e. centi-ns */
	const uint32_t cAmbientEffectiveWidth_centi_ns = 600;
	/* Expressed in 100ths of a ns, i.e. centi-ns */
	const FixPoint1616_t cSigmaEstRef              = 0x00000042;
	const uint32_t cVcselPulseWidth_ps             = 4700;
	/* pico secs */
	const FixPoint1616_t cSigmaEstMax              = 0x028F87AE;
	const FixPoint1616_t cTOF_per_mm_ps            = 0x0006999A;
	/* Time Of Flight per mm (6.6 pico secs) */
	const uint32_t c16BitRoundingParam             = 0x00008000;
	const FixPoint1616_t cMaxXTalk_kcps            = 0x00320000;

	uint32_t signalTotalEventsRtn;
	FixPoint1616_t sigmaEstimateP1;
	FixPoint1616_t sigmaEstimateP2;
	FixPoint1616_t sigmaEstimateP3;
	FixPoint1616_t deltaT_ps;
	FixPoint1616_t pwMult;
	FixPoint1616_t sigmaEstRtn;
	FixPoint1616_t sigmaEstimate;
	FixPoint1616_t xTalkCorrection;
	uint32_t signalTotalEventsRtnRawVal;
	FixPoint1616_t ambientRate_kcps;
	FixPoint1616_t vcselRate_kcps;
	FixPoint1616_t xTalkCompRate_mcps;
	uint32_t xTalkCompRate_kcps;
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceParameters_t CurrentParameters;
	FixPoint1616_t diff1_mcps;
	FixPoint1616_t diff2_mcps;
	FixPoint1616_t sqr1;
	FixPoint1616_t sqr2;
	FixPoint1616_t sqrSum;
	FixPoint1616_t sqrtResult_centi_ns;
	FixPoint1616_t sqrtResult;

    /*! \addtogroup calc_sigma_estimate
	* @{
	*
	* Estimates the range sigma based on the
	*
	*  - vcsel_rate_kcps
	*  - ambient_rate_kcps
	*  - signal_total_events
	*  - xtalk_rate
	*
	* and the following parameters
	*
	*  - SigmaEstRefArray
	*  - SigmaEstEffPulseWidth
	*  - SigmaEstEffAmbWidth
	*/

	LOG_FUNCTION_START("");

	VL53L0_GETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
		xTalkCompRate_mcps);

    /*
	* We work in kcps rather than mcps as this helps keep within the
	* confines of the 32 Fix1616 type.
	*/

	xTalkCompRate_kcps = xTalkCompRate_mcps * 1000;
	if (xTalkCompRate_kcps > cMaxXTalk_kcps)
		xTalkCompRate_kcps = cMaxXTalk_kcps;

	VL53L0_get_jmp_vcsel_ambient_rate(Dev,
		&ambientRate_kcps,
		&vcselRate_kcps,
		&signalTotalEventsRtnRawVal);

	signalTotalEventsRtn = signalTotalEventsRtnRawVal;
	if (signalTotalEventsRtn < 1)
		signalTotalEventsRtn = 1;


    /*
	* Calculate individual components of the main equation - replicating
	* the equation implemented in the script OpenAll_Ewok_ranging_data.jsl.
	*
	* sigmaEstimateP1 represents the effective pulse width, which is a
	* tuning parameter, rather than a real value.
	*
	* sigmaEstimateP2 represents the ambient/signal rate ratio expressed as
	* a multiple of the effective ambient width (tuning parameter).
	*
	* sigmaEstimateP3 provides the signal event component, with the
	* knowledge that
	*  - Noise of a square pulse is 1/sqrt(12) of the pulse width.
	*  - at 0Lux, sigma is proportional to effectiveVcselPulseWidth/
	*    sqrt(12 * signalTotalEvents)
	*
	* deltaT_ps represents the time of flight in pico secs for the current
	* range measurement,
	* using the "TOF per mm" constant (in ps).
	*/

	sigmaEstimateP1 = cPulseEffectiveWidth_centi_ns;

	/* ((FixPoint1616 << 16)* uint32)/FixPoint1616 = FixPoint1616 */
	sigmaEstimateP2 = (ambientRate_kcps << 16)/vcselRate_kcps;
	sigmaEstimateP2 *= cAmbientEffectiveWidth_centi_ns;

	sigmaEstimateP3 = 2 * VL53L0_isqrt(signalTotalEventsRtn * 12);

    /* uint32 * FixPoint1616 = FixPoint1616 */
	deltaT_ps = pRangingMeasurementData->RangeMilliMeter * cTOF_per_mm_ps;

    /*
	* vcselRate - xtalkCompRate
	* (uint32 << 16) - FixPoint1616 = FixPoint1616.
	* Divide result by 1000 to convert to mcps.
	* 500 is added to ensure rounding when integer division truncates.
	*/
	diff1_mcps = (((vcselRate_kcps << 16) - xTalkCompRate_kcps) + 500)/1000;
	/* vcselRate + xtalkCompRate */
	diff2_mcps = (((vcselRate_kcps << 16) + xTalkCompRate_kcps) + 500)/1000;

	/* Shift by 12 bits to increase resolution prior to the division */
	diff1_mcps <<= 12;

	/* FixPoint0428/FixPoint1616 = FixPoint2012 */
	xTalkCorrection  = abs(diff1_mcps/diff2_mcps);

	/* FixPoint2012 << 4 = FixPoint1616 */
	xTalkCorrection <<= 4;

	/* FixPoint1616/uint32 = FixPoint1616 */
	pwMult = deltaT_ps/cVcselPulseWidth_ps; /* smaller than 1.0f */

	/*
	* FixPoint1616 * FixPoint1616 = FixPoint3232, however both values are
	* small enough such that32 bits will not be exceeded.
	*/
	pwMult *= ((1 << 16) - xTalkCorrection);

	/* (FixPoint3232 >> 16) = FixPoint1616 */
	pwMult =  (pwMult + c16BitRoundingParam) >> 16;

	/* FixPoint1616 + FixPoint1616 = FixPoint1616 */
	pwMult += (1 << 16);

	/*
	* At this point the value will be 1.xx, therefore if we square the value
	* this will exceed
	* 32 bits. To address this perform a single shift to the right before
	* the multiplication.
	*/
	pwMult >>= 1;
	/* FixPoint1715 * FixPoint1715 = FixPoint3430 */
	pwMult = pwMult * pwMult;

	/* (FixPoint3430 >> 14) = Fix1616 */
	pwMult >>= 14;

	/* FixPoint1616 * FixPoint1616 = FixPoint3232 */
	sqr1 = pwMult * sigmaEstimateP1;

	/* (FixPoint1616 >> 12) = FixPoint2804 */
	sqr1 = (sqr1 + 0x800) >> 12;

	/* FixPoint2804 * FixPoint2804 = FixPoint5608 */
	sqr1 *= sqr1;

	sqr2 = sigmaEstimateP2;

	/* (FixPoint1616 >> 12) = FixPoint2804 */
	sqr2 = (sqr2 + 0x800) >> 12;

	/* FixPoint2804 * FixPoint2804 = FixPoint5608 */
	sqr2 *= sqr2;

	/* FixPoint5608 + FixPoint5608 = FixPoint5608 */
	sqrSum = sqr1 + sqr2;

	/* SQRT(FixPoint5608) = FixPoint2804 */
	sqrtResult_centi_ns = VL53L0_isqrt(sqrSum);

	/* (FixPoint2804 << 12) = FixPoint1616 */
	sqrtResult_centi_ns <<= 12;

	/*
	* Note that the Speed Of Light is expressed in um per 1E-10 seconds
	* (2997)
	* Therefore to get mm/ns we have to divide by 10000
	*/
	sigmaEstRtn      = ((sqrtResult_centi_ns+50)/100 *
		VL53L0_SPEED_OF_LIGHT_IN_AIR);
	sigmaEstRtn      /= (sigmaEstimateP3);
	sigmaEstRtn      += 5000; /* Add 5000 before dividing by 10000 to ensure
	rounding. */
	sigmaEstRtn      /= 10000;

	/* FixPoint1616 * FixPoint1616 = FixPoint3232 */
	sqr1 = sigmaEstRtn * sigmaEstRtn;
	/* FixPoint1616 * FixPoint1616 = FixPoint3232 */
	sqr2 = cSigmaEstRef * cSigmaEstRef;

	/* sqrt(FixPoint3232 << 12) = FixPoint1022 */
	sqrtResult = VL53L0_isqrt((sqr1 + sqr2) << 12);
	sqrtResult = (sqrtResult + 0x20) >> 6;
	/*
	* Note that the Shift by 12bits increases resolution prior to the sqrt,
	* therefore the result must be shifted by 6bits to the right to revert
	* back to the FixPoint1616 format.
	*/

	sigmaEstimate    = 1000 * sqrtResult;

	if ((vcselRate_kcps < 1) || (signalTotalEventsRtn < 1) ||
		(sigmaEstimate > cSigmaEstMax)) {
		sigmaEstimate = cSigmaEstMax;
	}

	*pSigmaEstimate = (uint32_t)(sigmaEstimate);
	PALDevDataSet(Dev, SigmaEstimate, *pSigmaEstimate);

	LOG_FUNCTION_END(Status);

	return Status;
}

static VL53L0_Error VL53L0_get_pal_range_status(VL53L0_DEV Dev,
	uint8_t DeviceRangeStatus,
	FixPoint1616_t SignalRate,
	FixPoint1616_t CrosstalkCompensation,
	uint16_t EffectiveSpadRtnCount,
	VL53L0_RangingMeasurementData_t
	*pRangingMeasurementData,
	uint8_t *pPalRangeStatus)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t tmpByte;
	uint8_t SigmaLimitCheckEnable;
	uint8_t SignalLimitCheckEnable;
/*	FixPoint1616_t SigmaEstimate;*/
	FixPoint1616_t SignalEstimate;
/*	FixPoint1616_t SigmaLimitValue;*/
	FixPoint1616_t SignalLimitValue;
	uint8_t DeviceRangeStatusInternal = 0;
	LOG_FUNCTION_START("");

	/*
	* VL53L0 has a good ranging when the value of the DeviceRangeStatus
	* = 11.
	* This function will replace the value 0 with the value 11 in the
	* DeviceRangeStatus.
	* In addition, the SigmaEstimator is not included in the VL53L0
	* DeviceRangeStatus, this will be added in the PalRangeStatus.
	*/

	DeviceRangeStatusInternal = ((DeviceRangeStatus & 0x78) >> 3);

	if (DeviceRangeStatusInternal == 11)
		tmpByte = 0;
	else if (DeviceRangeStatusInternal == 0)
		tmpByte = 11;
	else
		tmpByte = DeviceRangeStatusInternal;


	/*
	* Check if Sigma limit is enabled, if yes then do comparison with limit
	* value and put the result back into pPalRangeStatus.
	*/
	Status =  VL53L0_GetSigmaLimitCheckEnable(Dev,
		VL53L0_CHECKPOSITION_EARLY,
		&SigmaLimitCheckEnable);
#if 0
	/* temp fix of kernel panic as divide 0 some time*/
	if ((SigmaLimitCheckEnable != 0) && (Status == VL53L0_ERROR_NONE)) {
		/*
		* compute the Sigma and check with limit
		*/
		Status = VL53L0_calc_sigma_estimate(Dev,
			pRangingMeasurementData,
			&SigmaEstimate);

		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_GetSigmaLimitValue(Dev,
				VL53L0_CHECKPOSITION_EARLY,
				&SigmaLimitValue);

			if ((SigmaLimitValue > 0) && (SigmaEstimate >
				SigmaLimitValue)) {
				/* Limit Fail add 2^4 to range status */
				tmpByte += 16;
			}
		}
	}
#endif
	/*
	* Check if Signal limit is enabled, if yes then do comparison with
	* limit value and put the result back into pPalRangeStatus.
	*/
	Status =  VL53L0_GetSignalLimitCheckEnable(Dev,
		VL53L0_CHECKPOSITION_EARLY,	&SignalLimitCheckEnable);

	if ((SignalLimitCheckEnable != 0) && (Status == VL53L0_ERROR_NONE)) {
		/*
		* compute the Signal and check with limit
		*/

		SignalEstimate  = (FixPoint1616_t)(SignalRate -
			(FixPoint1616_t)((EffectiveSpadRtnCount *
			CrosstalkCompensation) >> 1));

		PALDevDataSet(Dev, SignalEstimate, SignalEstimate);

		Status = VL53L0_GetSignalLimitValue(Dev,
			VL53L0_CHECKPOSITION_EARLY, &SignalLimitValue);

		if ((SignalLimitValue > 0) && (SignalEstimate <
			SignalLimitValue)) {
			/* Limit Fail add 2^5 to range status */
			tmpByte += 32;
		}
	}

	if (Status == VL53L0_ERROR_NONE)
		*pPalRangeStatus = tmpByte;



	LOG_FUNCTION_END(Status);
	return Status;

}
