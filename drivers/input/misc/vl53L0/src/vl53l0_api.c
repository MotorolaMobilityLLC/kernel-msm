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
 ********************************************************************************/

#include "vl53l0_api.h"
#include "vl53l0_tuning.h"
#ifndef __KERNEL__
#include <stdlib.h>
#endif
#define LOG_FUNCTION_START(fmt, ... ) \
    _LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ... ) \
    _LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ... ) \
    _LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

#ifdef VL53L0_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(TRACE_MODULE_API, \
    level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#endif


/* Defines */
#define VL53L0_SETPARAMETERFIELD(Dev, field, value) \
    if(Status==VL53L0_ERROR_NONE){ \
        CurrentParameters = PALDevDataGet(Dev, CurrentParameters); \
        CurrentParameters.field = value; \
        CurrentParameters = \
            PALDevDataSet(Dev, CurrentParameters, \
                          CurrentParameters); }
#define VL53L0_SETARRAYPARAMETERFIELD(Dev, field, index, value) \
    if(Status==VL53L0_ERROR_NONE){ \
        CurrentParameters = PALDevDataGet(Dev, CurrentParameters); \
        CurrentParameters.field[index] = value; \
        CurrentParameters = \
            PALDevDataSet(Dev, CurrentParameters, \
                          CurrentParameters); }

#define VL53L0_GETPARAMETERFIELD(Dev, field, variable) \
    if(Status==VL53L0_ERROR_NONE){ \
        CurrentParameters = \
            PALDevDataGet(Dev, CurrentParameters);  \
        variable = CurrentParameters.field; }
#define VL53L0_GETARRAYPARAMETERFIELD(Dev, field, index, variable) \
    if(Status==VL53L0_ERROR_NONE){ \
        CurrentParameters = \
            PALDevDataGet(Dev, CurrentParameters);      \
        variable = CurrentParameters.field[index]; }

#define VL53L0_SETDEVICESPECIFICPARAMETER(Dev, field, value) \
	if(Status==VL53L0_ERROR_NONE){ \
		DeviceSpecificParameters = \
            PALDevDataGet(Dev, DeviceSpecificParameters);   \
		DeviceSpecificParameters.field = value; \
		DeviceSpecificParameters = \
            PALDevDataSet(Dev, DeviceSpecificParameters, \
                          DeviceSpecificParameters); }
#define VL53L0_GETDEVICESPECIFICPARAMETER(Dev, field) \
		PALDevDataGet(Dev, DeviceSpecificParameters).field

#define VL53L0_FIXPOINT1616TOFIXPOINT97(Value) \
            (uint16_t)((Value>>9)&0xFFFF)
#define VL53L0_FIXPOINT97TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<9)

#define VL53L0_FIXPOINT1616TOFIXPOINT88(Value) \
            (uint16_t)((Value>>8)&0xFFFF)
#define VL53L0_FIXPOINT88TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<8)

#define VL53L0_FIXPOINT1616TOFIXPOINT412(Value) \
            (uint16_t)((Value>>4)&0xFFFF)
#define VL53L0_FIXPOINT412TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<4)

#define VL53L0_FIXPOINT1616TOFIXPOINT313(Value) \
            (uint16_t)((Value>>3)&0xFFFF)
#define VL53L0_FIXPOINT313TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<3)

#define VL53L0_FIXPOINT1616TOFIXPOINT08(Value) \
            (uint8_t)((Value>>8)&0x00FF)
#define VL53L0_FIXPOINT08TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<8)

#define VL53L0_FIXPOINT1616TOFIXPOINT53(Value) \
            (uint8_t)((Value>>13)&0x00FF)
#define VL53L0_FIXPOINT53TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<13)

#define VL53L0_FIXPOINT1616TOFIXPOINT102(Value) \
            (uint16_t)((Value>>14)&0x0FFF)
#define VL53L0_FIXPOINT102TOFIXPOINT1616(Value) \
            (FixPoint1616_t)(Value<<12)

#define VL53L0_MAKEUINT16(lsb, msb) (uint16_t)((((uint16_t)msb)<<8) + \
		(uint16_t)lsb)

#define REF_ARRAY_SPAD_0  0
#define REF_ARRAY_SPAD_5  5
#define REF_ARRAY_SPAD_10 10

uint32_t refArrayQuadrants[4] = {REF_ARRAY_SPAD_10, REF_ARRAY_SPAD_5,
		REF_ARRAY_SPAD_0, REF_ARRAY_SPAD_5 };

/* Internal functions declaration */
VL53L0_Error VL53L0_get_vcsel_pulse_period(VL53L0_DEV Dev,
		uint8_t* pVCSELPulsePeriod, uint8_t RangeIndex);
uint8_t VL53L0_encode_vcsel_period(uint8_t vcsel_period_pclks);
uint8_t VL53L0_decode_vcsel_period(uint8_t vcsel_period_reg);
uint16_t VL53L0_calc_encoded_timeout(VL53L0_DEV Dev, uint32_t timeout_period_us,
		uint8_t vcsel_period);
uint32_t VL53L0_calc_ranging_wait_us(VL53L0_DEV Dev,
		uint16_t timeout_overall_periods, uint8_t vcsel_period);
VL53L0_Error VL53L0_check_part_used(VL53L0_DEV Dev, uint8_t* Revision,
		VL53L0_DeviceInfo_t* pVL53L0_DeviceInfo);
VL53L0_Error VL53L0_get_info_from_device(VL53L0_DEV Dev);
VL53L0_Error VL53L0_device_read_strobe(VL53L0_DEV Dev);
VL53L0_Error VL53L0_get_pal_range_status(VL53L0_DEV Dev,
					 uint8_t DeviceRangeStatus,
					 FixPoint1616_t SignalRate,
					 FixPoint1616_t CrosstalkCompensation,
					 uint16_t EffectiveSpadRtnCount,
					 VL53L0_RangingMeasurementData_t *pRangingMeasurementData,
					 uint8_t* pPalRangeStatus);
VL53L0_Error VL53L0_measurement_poll_for_completion(VL53L0_DEV Dev);
VL53L0_Error VL53L0_confirm_measurement_start(VL53L0_DEV Dev);
VL53L0_Error VL53L0_start_histogram_measurement(VL53L0_DEV Dev,
		VL53L0_HistogramModes histoMode, uint32_t count);
VL53L0_Error VL53L0_read_histo_measurement(VL53L0_DEV Dev, uint32_t *histoData,
		uint32_t offset, VL53L0_HistogramModes histoMode);
VL53L0_Error VL53L0_reverse_bytes(uint8_t *data, uint32_t size);
VL53L0_Error VL53L0_load_tuning_settings(VL53L0_DEV Dev,
		uint8_t* pTuningSettingBuffer);
VL53L0_Error enable_ref_spads(VL53L0_DEV Dev,
                              uint8_t apertureSpads,
                              uint8_t goodSpadArray[],
                              uint8_t spadArray[],
                              uint32_t size,
                              uint32_t start,
                              uint32_t offset,
                              uint32_t minimumSpadCount,
                              uint32_t *lastSpad);
VL53L0_Error perform_ref_signal_measurement(VL53L0_DEV Dev,
		uint16_t *refSignalRate);
uint8_t is_aperture(uint32_t spadIndex);
void get_next_good_spad(uint8_t array1[], uint32_t size, uint32_t current1,
		int32_t *next);
VL53L0_Error enable_spad_bit(uint8_t spadArray[], uint32_t size,
		uint32_t spadIndex);
VL53L0_Error set_ref_spad_map(VL53L0_DEV Dev, uint8_t *refSpadArray);
VL53L0_Error get_ref_spad_map(VL53L0_DEV Dev, uint8_t *refSpadArray);
VL53L0_Error VL53L0_apply_ref_spads(VL53L0_DEV Dev, uint8_t apertureSpads, uint32_t count);

/* Group PAL General Functions */

VL53L0_Error VL53L0_GetVersion(VL53L0_Version_t* pVersion) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    pVersion->major = VL53L0_IMPLEMENTATION_VER_MAJOR;
    pVersion->minor = VL53L0_IMPLEMENTATION_VER_MINOR;
    pVersion->build = VL53L0_IMPLEMENTATION_VER_SUB;

    pVersion->revision = VL53L0_IMPLEMENTATION_VER_REVISION;

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetPalSpecVersion(VL53L0_Version_t* pPalSpecVersion)
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

VL53L0_Error VL53L0_GetDeviceInfo(VL53L0_DEV Dev, VL53L0_DeviceInfo_t* pVL53L0_DeviceInfo) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t revision_id;
    uint8_t Revision;

    LOG_FUNCTION_START("");

    Status = VL53L0_check_part_used(Dev, &Revision, pVL53L0_DeviceInfo);

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
        Status = VL53L0_RdByte(Dev, VL53L0_REG_IDENTIFICATION_REVISION_ID,
        		&revision_id);
        pVL53L0_DeviceInfo->ProductRevisionMajor = 1;
        pVL53L0_DeviceInfo->ProductRevisionMinor = (revision_id & 0xF0) >> 4;
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetDeviceErrorStatus(VL53L0_DEV Dev,
		VL53L0_DeviceError* pDeviceErrorStatus)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t RangeStatus;

    LOG_FUNCTION_START("");

    Status = VL53L0_RdByte(Dev, VL53L0_REG_RESULT_RANGE_STATUS, &RangeStatus);

    *pDeviceErrorStatus = (VL53L0_DeviceError)((RangeStatus & 0x78) >> 3);

    LOG_FUNCTION_END(Status);
    return Status;
}

#define VL53L0_BUILDCASESTRING(BUFFER, CODE,STRINGVALUE) \
    case CODE: \
    VL53L0_COPYSTRING(BUFFER, STRINGVALUE); \
    break;\

VL53L0_Error VL53L0_GetDeviceErrorString(VL53L0_DeviceError ErrorCode,
		char* pDeviceErrorString)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    switch (ErrorCode)
    {
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_NONE,
            VL53L0_STRING_DEVICEERROR_NONE);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_VCSELCONTINUITYTESTFAILURE,
    		VL53L0_STRING_DEVICEERROR_VCSELCONTINUITYTESTFAILURE);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_VCSELWATCHDOGTESTFAILURE,
    		VL53L0_STRING_DEVICEERROR_VCSELWATCHDOGTESTFAILURE);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_NOVHVVALUEFOUND,
    		VL53L0_STRING_DEVICEERROR_NOVHVVALUEFOUND);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_MSRCNOTARGET,
    		VL53L0_STRING_DEVICEERROR_MSRCNOTARGET);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_SNRCHECK,
    		VL53L0_STRING_DEVICEERROR_SNRCHECK);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_RANGEPHASECHECK,
    		VL53L0_STRING_DEVICEERROR_RANGEPHASECHECK);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_SIGMATHRESHOLDCHECK,
    		VL53L0_STRING_DEVICEERROR_SIGMATHRESHOLDCHECK);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_TCC,
    		VL53L0_STRING_DEVICEERROR_TCC);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_PHASECONSISTENCY,
    		VL53L0_STRING_DEVICEERROR_PHASECONSISTENCY);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_MINCLIP,
    		VL53L0_STRING_DEVICEERROR_MINCLIP);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_RANGECOMPLETE,
    		VL53L0_STRING_DEVICEERROR_RANGECOMPLETE);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_ALGOUNDERFLOW,
    		VL53L0_STRING_DEVICEERROR_ALGOUNDERFLOW);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
            VL53L0_DEVICEERROR_ALGOOVERFLOW,
    		VL53L0_STRING_DEVICEERROR_ALGOOVERFLOW);
    VL53L0_BUILDCASESTRING(pDeviceErrorString,
    		VL53L0_DEVICEERROR_RANGEIGNORETHRESHOLD,
    		VL53L0_STRING_DEVICEERROR_RANGEIGNORETHRESHOLD);
    default:
        VL53L0_COPYSTRING(pDeviceErrorString,
            VL53L0_STRING_UNKNOW_ERROR_CODE);

    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetPalErrorString(VL53L0_Error PalErrorCode,
		char* pPalErrorString)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    switch (PalErrorCode)
    {
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_NONE,
            VL53L0_STRING_ERROR_NONE);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_CALIBRATION_WARNING,
            VL53L0_STRING_ERROR_CALIBRATION_WARNING);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_MIN_CLIPPED,
            VL53L0_STRING_ERROR_MIN_CLIPPED);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_UNDEFINED,
            VL53L0_STRING_ERROR_UNDEFINED);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_INVALID_PARAMS,
            VL53L0_STRING_ERROR_INVALID_PARAMS);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_NOT_SUPPORTED,
            VL53L0_STRING_ERROR_NOT_SUPPORTED);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_RANGE_ERROR,
            VL53L0_STRING_ERROR_RANGE_ERROR);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_TIME_OUT,
            VL53L0_STRING_ERROR_TIME_OUT);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_MODE_NOT_SUPPORTED,
            VL53L0_STRING_ERROR_MODE_NOT_SUPPORTED);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_NOT_IMPLEMENTED,
            VL53L0_STRING_ERROR_NOT_IMPLEMENTED);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_BUFFER_TOO_SMALL,
            VL53L0_STRING_ERROR_BUFFER_TOO_SMALL);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_GPIO_NOT_EXISTING,
            VL53L0_STRING_ERROR_GPIO_NOT_EXISTING);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED,
            VL53L0_STRING_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_DIVISION_BY_ZERO,
            VL53L0_STRING_ERROR_DIVISION_BY_ZERO);
    VL53L0_BUILDCASESTRING(pPalErrorString,
            VL53L0_ERROR_CONTROL_INTERFACE,
            VL53L0_STRING_ERROR_CONTROL_INTERFACE);
    default:
        VL53L0_COPYSTRING(pPalErrorString,
                          VL53L0_STRING_UNKNOW_ERROR_CODE);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}


VL53L0_Error VL53L0_GetPalState(VL53L0_DEV Dev, VL53L0_State* pPalState) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    *pPalState = PALDevDataGet(Dev, PalState);

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetPowerMode(VL53L0_DEV Dev, VL53L0_PowerModes PowerMode) {
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
            PALDevDataSet(Dev, PowerMode, VL53L0_POWERMODE_STANDBY_LEVEL1);
        }

    } else {
        /* VL53L0_POWERMODE_IDLE_LEVEL1 */

        Status = VL53L0_WrByte(Dev, 0x80, 0x00);
        if (Status == VL53L0_ERROR_NONE) {
            Status = VL53L0_StaticInit(Dev);
        }
        if (Status == VL53L0_ERROR_NONE) {
            PALDevDataSet(Dev, PowerMode, VL53L0_POWERMODE_IDLE_LEVEL1);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetPowerMode(VL53L0_DEV Dev, VL53L0_PowerModes* pPowerMode) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t Byte;
    LOG_FUNCTION_START("");

    /* Only level1 of Power mode exists */
    Status = VL53L0_RdByte(Dev, 0x80, &Byte);

    if (Status == VL53L0_ERROR_NONE) {
        if (Byte == 1) {
            PALDevDataSet(Dev, PowerMode, VL53L0_POWERMODE_IDLE_LEVEL1);
        } else {
            PALDevDataSet(Dev, PowerMode, VL53L0_POWERMODE_STANDBY_LEVEL1);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetOffsetCalibrationDataMicroMeter(VL53L0_DEV Dev,
		int32_t OffsetCalibrationDataMicroMeter)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    int16_t cMaxOffset = 1023;
    int16_t cMinOffset = -1024;
    int16_t cOffsetRange = 2048;
    int16_t OffsetCalibrationDataMilliMeter =
    		(OffsetCalibrationDataMicroMeter + 500)/1000;

    LOG_FUNCTION_START("");

    if(OffsetCalibrationDataMilliMeter > cMaxOffset)
    {
        OffsetCalibrationDataMilliMeter = cMaxOffset;
    }
    else if(OffsetCalibrationDataMilliMeter < cMinOffset)
    {
        OffsetCalibrationDataMilliMeter = cMinOffset;
    }

    if(OffsetCalibrationDataMilliMeter < 0)
    {
        /* Apply 10 bit 2's compliment conversion */
        OffsetCalibrationDataMilliMeter += cOffsetRange;
    }

    Status = VL53L0_WrWord(Dev, VL53L0_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM,
    		(uint16_t)(OffsetCalibrationDataMilliMeter << 2));

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetOffsetCalibrationDataMicroMeter(VL53L0_DEV Dev,
		int32_t * pOffsetCalibrationDataMicroMeter)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint16_t RangeOffsetRegister;
    int16_t cMaxOffset = 1023;
    int16_t cOffsetRange = 2048;

    LOG_FUNCTION_START("");

    Status = VL53L0_RdWord(Dev,
                           VL53L0_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM,
                           &RangeOffsetRegister);

    if (Status == VL53L0_ERROR_NONE) {
        /*
         * Remove fractional part. We don't expect the offset to be less than 1mm resolution
         * as this is unrealistic.
         */
        
        RangeOffsetRegister = ((RangeOffsetRegister & 0x0fff) + 0x02) >> 2;

        /* Apply 10 bit 2's compliment conversion */
        if(RangeOffsetRegister > cMaxOffset)
        {
            *pOffsetCalibrationDataMicroMeter =
                (int16_t)(RangeOffsetRegister - cOffsetRange) * 1000;
        }
        else
        {
            *pOffsetCalibrationDataMicroMeter =
                (int16_t)RangeOffsetRegister * 1000;
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetGroupParamHold(VL53L0_DEV Dev, uint8_t GroupParamHold) {
    VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
    LOG_FUNCTION_START("");

    /* not implemented on VL53L0 */

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetUpperLimitMilliMeter(VL53L0_DEV Dev, uint16_t* pUpperLimitMilliMeter) {
    VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
    LOG_FUNCTION_START("");

    /* not implemented on VL53L0 */

    LOG_FUNCTION_END(Status);
    return Status;
}
/* End Group PAL General Functions */

/* Group PAL Init Functions */
VL53L0_Error VL53L0_SetDeviceAddress(VL53L0_DEV Dev, uint8_t DeviceAddress) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    Status = VL53L0_WrByte(Dev, VL53L0_REG_I2C_SLAVE_DEVICE_ADDRESS, DeviceAddress / 2);

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_DataInit(VL53L0_DEV Dev) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
    int32_t OffsetCalibrationData;

    int i;

    LOG_FUNCTION_START("");

    /* by default the I2C is running at 1V8 if you want to change it you
     * need to include this define at compilation level. */
#ifdef USE_I2C_2V8
    Status = VL53L0_UpdateByte(Dev,
                               VL53L0_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
                               0xFE,
                               0x01);
#endif

    /* Set I2C standard mode */
    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev, 0x88, 0x00);

    if (Status == VL53L0_ERROR_NONE) {
		/* read device info */
		VL53L0_SETDEVICESPECIFICPARAMETER(Dev, ReadDataFromDeviceDone, 0);

		Status = VL53L0_get_info_from_device(Dev);
    }

    /* Set Default static parameters
     *set first temporary values 11.3999MHz * 65536 = 748421 */
    VL53L0_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz, 748421);

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
    PALDevDataSet(Dev, targetRefRate, 0x0A00); /* 20 MCPS in 9:7 format */

    /* Use internal default settings */
    PALDevDataSet(Dev, UseInternalTuningSettings, 1);

    /* Enable all check */
    for (i = 0; i < VL53L0_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
		if (Status == VL53L0_ERROR_NONE) {
				Status |= VL53L0_SetLimitCheckEnable(Dev, i, 1);
			} else {
				break;
		}
    }

    /* Disable the following check */
    if (Status == VL53L0_ERROR_NONE)
    	Status = VL53L0_SetLimitCheckEnable(Dev,
    			VL53L0_CHECKENABLE_SIGNAL_REF_CLIP, 0);

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetLimitCheckValue(Dev,
        		VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
        		(FixPoint1616_t)(32<<16));
    }
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetLimitCheckValue(Dev,
        		VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
    			(FixPoint1616_t)(25 * 65536 / 100));
    			/* 0.25 * 65538 */
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetLimitCheckValue(Dev,
        		VL53L0_CHECKENABLE_SIGNAL_REF_CLIP,
    			(FixPoint1616_t)(35 * 65536));
    }

    /* Read back NVM offset */
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_GetOffsetCalibrationDataMicroMeter(Dev,
        		&OffsetCalibrationData);
    }

    if (Status == VL53L0_ERROR_NONE) {
        VL53L0_SETPARAMETERFIELD(Dev, RangeOffsetMicroMeters,
        		OffsetCalibrationData)
        PALDevDataSet(Dev, Part2PartOffsetNVMMicroMeter,
        		OffsetCalibrationData);

        PALDevDataSet(Dev, SequenceConfig, 0xFF);

        /* Set PAL state to tell that we are waiting for call to
         * VL53L0_StaticInit */
        PALDevDataSet(Dev, PalState, VL53L0_STATE_WAIT_STATICINIT);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetTuningSettingBuffer(VL53L0_DEV Dev,
		uint8_t* pTuningSettingBuffer, uint8_t UseInternalTuningSettings)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    if (UseInternalTuningSettings == 1) {
        /* Force use internal settings */
        PALDevDataSet(Dev, UseInternalTuningSettings, 1);
    } else {

        /* check that the first byte is not 0 */
        if (*pTuningSettingBuffer != 0) {
            PALDevDataSet(Dev, pTuningSettingsPointer, pTuningSettingBuffer);
            PALDevDataSet(Dev, UseInternalTuningSettings, 0);

        } else {
            Status = VL53L0_ERROR_INVALID_PARAMS;
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetTuningSettingBuffer(VL53L0_DEV Dev,
		uint8_t* pTuningSettingBuffer, uint8_t* pUseInternalTuningSettings)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    pTuningSettingBuffer = PALDevDataGet(Dev, pTuningSettingsPointer);
    *pUseInternalTuningSettings = PALDevDataGet(Dev, UseInternalTuningSettings);

    LOG_FUNCTION_END(Status);
    return Status;
}



VL53L0_Error VL53L0_StaticInit(VL53L0_DEV Dev)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t* pTuningSettingBuffer;
    uint16_t tempword;
    uint8_t  tempbyte;
    uint8_t  UseInternalTuningSettings;
    uint8_t ReferenceSpadCount; /* used for ref spad management */
    uint8_t ReferenceSpadType;  /* used for ref spad management */

    LOG_FUNCTION_START("");

	/* this function do nothing if it has been called before */
	Status = VL53L0_get_info_from_device(Dev);

    if (Status == VL53L0_ERROR_NONE) {
    	ReferenceSpadCount = VL53L0_GETDEVICESPECIFICPARAMETER(Dev,
    			ReferenceSpadCount);
    	ReferenceSpadType = VL53L0_GETDEVICESPECIFICPARAMETER(Dev,
    			ReferenceSpadType);

    	if ((Status == VL53L0_ERROR_NONE) && (ReferenceSpadCount >= 5)) {
    	    VL53L0_apply_ref_spads(Dev, ReferenceSpadType, ReferenceSpadCount);
    	}

    }

    /* Initialise tuning settings buffer to prevent compiler warning. */
    pTuningSettingBuffer = DefaultTuningSettings;

    if (Status == VL53L0_ERROR_NONE) {
		UseInternalTuningSettings = PALDevDataGet(Dev,
				UseInternalTuningSettings);

		if (UseInternalTuningSettings == 0) {
			pTuningSettingBuffer = PALDevDataGet(Dev, pTuningSettingsPointer);
		} else {
			pTuningSettingBuffer = DefaultTuningSettings;
		}
    }

    if (Status == VL53L0_ERROR_NONE)
    	Status = VL53L0_load_tuning_settings(Dev, pTuningSettingBuffer);

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_WrByte(Dev, 0x80, 0x00);
    }

    /* Set interrupt config to new sample ready */
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetGpioConfig(Dev, 0, 0,
                VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY,
                VL53L0_INTERRUPTPOLARITY_LOW);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_WrByte(Dev, 0xFF, 0x01);
        Status |= VL53L0_RdWord(Dev, 0x84, &tempword);
        Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
    }

    if (Status == VL53L0_ERROR_NONE) {
        VL53L0_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz,
                VL53L0_FIXPOINT412TOFIXPOINT1616(tempword));
    }

    /* After static init, some device parameters may be changed, so update them */
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_GetDeviceParameters(Dev, &CurrentParameters);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSTEM_RANGE_CONFIG, &tempbyte);
        PALDevDataSet(Dev, RangeFractionalEnable, tempbyte);
    }

    if (Status == VL53L0_ERROR_NONE) {
        PALDevDataSet(Dev, CurrentParameters, CurrentParameters);
    }

    /* read the sequence config and save it */
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdByte(Dev,
                VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, &tempbyte);
        if (Status == VL53L0_ERROR_NONE) {
            PALDevDataSet(Dev, SequenceConfig, tempbyte);
    	}
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_PerformRefCalibration(Dev);
    }

    /* Set PAL State to standby */
    if (Status == VL53L0_ERROR_NONE) {
        PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_WaitDeviceBooted(VL53L0_DEV Dev) {
    VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
    LOG_FUNCTION_START("");

    /* not implemented on VL53L0 */

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_ResetDevice(VL53L0_DEV Dev) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t Byte;
    LOG_FUNCTION_START("");

    /* Set reset bit */
    Status = VL53L0_WrByte(Dev, VL53L0_REG_SOFT_RESET_GO2_SOFT_RESET_N, 0x00);

    /* Wait for some time */
    if (Status == VL53L0_ERROR_NONE) {
        do {
            Status = VL53L0_RdByte(Dev,
                    VL53L0_REG_IDENTIFICATION_MODEL_ID, &Byte);
        } while (Byte != 0x00);
    }

    /* Release reset */
    Status = VL53L0_WrByte(Dev, VL53L0_REG_SOFT_RESET_GO2_SOFT_RESET_N, 0x01);

    /* Wait until correct boot-up of the device */
    if (Status == VL53L0_ERROR_NONE) {
        do {
            Status = VL53L0_RdByte(Dev,
                    VL53L0_REG_IDENTIFICATION_MODEL_ID, &Byte);
        } while (Byte == 0x00);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}
/* End Group PAL Init Functions */

/* Group PAL Parameters Functions */
VL53L0_Error VL53L0_SetDeviceParameters(VL53L0_DEV Dev,
        const VL53L0_DeviceParameters_t* pDeviceParameters) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    int i;
    LOG_FUNCTION_START("");
    Status = VL53L0_SetDeviceMode(Dev, pDeviceParameters->DeviceMode);



    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetHistogramMode(Dev, pDeviceParameters->HistogramMode);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetInterMeasurementPeriodMilliSeconds(Dev,
                pDeviceParameters->InterMeasurementPeriodMilliSeconds);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetXTalkCompensationRateMegaCps(Dev,
                pDeviceParameters->XTalkCompensationRateMegaCps);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetOffsetCalibrationDataMicroMeter(Dev,
                pDeviceParameters->RangeOffsetMicroMeters);
    }

    for (i = 0; i < VL53L0_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
        if (Status == VL53L0_ERROR_NONE) {
            Status |= VL53L0_SetLimitCheckEnable(Dev, i,
                    pDeviceParameters->LimitChecksEnable[i]);
        } else {
            break;
        }
        if (Status == VL53L0_ERROR_NONE) {
            Status |= VL53L0_SetLimitCheckValue(Dev, i,
                    pDeviceParameters->LimitChecksValue[i]);
        } else {
            break;
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
        VL53L0_DeviceParameters_t* pDeviceParameters) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    int i;

    LOG_FUNCTION_START("");

    Status = VL53L0_GetDeviceMode(Dev, &(pDeviceParameters->DeviceMode));

    if(Status==VL53L0_ERROR_NONE){
       Status = VL53L0_GetHistogramMode(Dev,
               &(pDeviceParameters->HistogramMode));
    }
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_GetInterMeasurementPeriodMilliSeconds(Dev,
                &(pDeviceParameters->InterMeasurementPeriodMilliSeconds));
    }
    if (Status == VL53L0_ERROR_NONE) {
        pDeviceParameters->XTalkCompensationEnable = 0;
    }
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_GetXTalkCompensationRateMegaCps(Dev,
                &(pDeviceParameters->XTalkCompensationRateMegaCps));
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_GetOffsetCalibrationDataMicroMeter(Dev,
                &(pDeviceParameters->RangeOffsetMicroMeters));
    }

    if (Status == VL53L0_ERROR_NONE) {
        for (i = 0; i < VL53L0_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
            /* get first the values, then the enables.
             * VL53L0_GetLimitCheckValue will modify the enable flags
             */
            if (Status == VL53L0_ERROR_NONE) {
                Status |= VL53L0_GetLimitCheckValue(Dev, i,
                        &(pDeviceParameters->LimitChecksValue[i]));
            } else {
                break;
            }
            if (Status == VL53L0_ERROR_NONE) {
                Status |= VL53L0_GetLimitCheckEnable(Dev, i,
                        &(pDeviceParameters->LimitChecksEnable[i]));
            } else {
                break;
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
        VL53L0_DeviceModes DeviceMode) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;

    LOG_FUNCTION_START("%d", (int)DeviceMode);

    switch (DeviceMode) {
    case VL53L0_DEVICEMODE_SINGLE_RANGING:
    case VL53L0_DEVICEMODE_CONTINUOUS_RANGING:
    case VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING:
    case VL53L0_DEVICEMODE_SINGLE_HISTOGRAM:
    case VL53L0_DEVICEMODE_GPIO_DRIVE:
    case VL53L0_DEVICEMODE_GPIO_OSC:
        /* Supported modes */
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
        VL53L0_DeviceModes* pDeviceMode) {
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

    switch (HistogramMode)
    {
    case VL53L0_HISTOGRAMMODE_DISABLED:
    case VL53L0_HISTOGRAMMODE_REFERENCE_ONLY:
    case VL53L0_HISTOGRAMMODE_RETURN_ONLY:
    case VL53L0_HISTOGRAMMODE_BOTH:
        /* Supported mode */
        VL53L0_SETPARAMETERFIELD(Dev, HistogramMode, HistogramMode);
        break;
    default:
        /* Unsupported mode */
        Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetHistogramMode(VL53L0_DEV Dev,
    VL53L0_HistogramModes* pHistogramMode)
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
    uint32_t FinalRangeTimingBudgetMicroSeconds;
    uint16_t encodedTimeOut;
    uint32_t MinTimingBudgetMicroSeconds = 26000;
    uint32_t PreRangeTimeoutMicroSeconds = 8000;
    uint32_t DccAndTccTimeoutMicroSeconds = 6000;
    uint32_t AdditionalOverheadsMicroSeconds = 5360;
    uint32_t TotalAdditionalTimingMicroSeconds = PreRangeTimeoutMicroSeconds +
    		DccAndTccTimeoutMicroSeconds + AdditionalOverheadsMicroSeconds;
    LOG_FUNCTION_START("");

    if (Status == VL53L0_ERROR_NONE) {
        if (MeasurementTimingBudgetMicroSeconds < MinTimingBudgetMicroSeconds) {
            Status = VL53L0_ERROR_INVALID_PARAMS;
        }
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_get_vcsel_pulse_period(Dev,
        		&CurrentVCSELPulsePeriodPClk, 1);
        if (Status == VL53L0_ERROR_NONE) {

            CurrentVCSELPulsePeriod =
            		VL53L0_encode_vcsel_period(CurrentVCSELPulsePeriodPClk);

            FinalRangeTimingBudgetMicroSeconds =
            		MeasurementTimingBudgetMicroSeconds -
            		TotalAdditionalTimingMicroSeconds;

            encodedTimeOut = VL53L0_calc_encoded_timeout(Dev,
            		FinalRangeTimingBudgetMicroSeconds,
            		(uint8_t) CurrentVCSELPulsePeriod);

            VL53L0_SETPARAMETERFIELD(Dev, MeasurementTimingBudgetMicroSeconds,
            		MeasurementTimingBudgetMicroSeconds);
            VL53L0_SETDEVICESPECIFICPARAMETER(Dev, LastEncodedTimeout,
            		encodedTimeOut);
        }
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_WrWord(Dev,
        		VL53L0_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
        		encodedTimeOut);
    }

    LOG_FUNCTION_END(Status);

    return Status;
}

VL53L0_Error VL53L0_GetMeasurementTimingBudgetMicroSeconds(VL53L0_DEV Dev,
		uint32_t* pMeasurementTimingBudgetMicroSeconds)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t CurrentVCSELPulsePeriod;
    uint8_t CurrentVCSELPulsePeriodPClk;
    uint16_t encodedTimeOut;
    uint32_t FinalRangeTimeoutMicroSeconds;
    uint32_t PreRangeTimeoutMicroSeconds = 8000;
    uint32_t DccAndTccTimeoutMicroSeconds = 6000;
    uint32_t AdditionalOverheadsMicroSeconds = 5360;
    uint32_t TotalAdditionalTimingMicroSeconds = PreRangeTimeoutMicroSeconds +
    		DccAndTccTimeoutMicroSeconds + AdditionalOverheadsMicroSeconds;

    LOG_FUNCTION_START("");

    if (Status == VL53L0_ERROR_NONE) {
        VL53L0_get_vcsel_pulse_period(Dev, &CurrentVCSELPulsePeriodPClk, 1);
        CurrentVCSELPulsePeriod =
        		VL53L0_encode_vcsel_period(CurrentVCSELPulsePeriodPClk);

        /* Read from register */
        Status = VL53L0_RdWord(Dev,
        		VL53L0_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
        		&encodedTimeOut);
        if (Status == VL53L0_ERROR_NONE) {
            FinalRangeTimeoutMicroSeconds = VL53L0_calc_ranging_wait_us(Dev,
            		encodedTimeOut, CurrentVCSELPulsePeriod);

            *pMeasurementTimingBudgetMicroSeconds =
            		FinalRangeTimeoutMicroSeconds +
            		TotalAdditionalTimingMicroSeconds;
            VL53L0_SETPARAMETERFIELD(Dev, MeasurementTimingBudgetMicroSeconds,
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
        if (osc_calibrate_val !=0) {
            IMPeriodMilliSeconds =
            		InterMeasurementPeriodMilliSeconds*osc_calibrate_val;
        } else {
            IMPeriodMilliSeconds = InterMeasurementPeriodMilliSeconds;
        }
        Status = VL53L0_WrDWord(Dev,
        		VL53L0_REG_SYSTEM_INTERMEASUREMENT_PERIOD,
        		IMPeriodMilliSeconds);
    }

    if (Status == VL53L0_ERROR_NONE) {
        VL53L0_SETPARAMETERFIELD(Dev, InterMeasurementPeriodMilliSeconds,
        		InterMeasurementPeriodMilliSeconds);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetInterMeasurementPeriodMilliSeconds(VL53L0_DEV Dev,
		uint32_t* pInterMeasurementPeriodMilliSeconds)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint16_t osc_calibrate_val;
    uint32_t IMPeriodMilliSeconds;

    LOG_FUNCTION_START("");

    Status = VL53L0_RdWord(Dev, VL53L0_REG_OSC_CALIBRATE_VAL,
    		&osc_calibrate_val);

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdDWord(Dev, VL53L0_REG_SYSTEM_INTERMEASUREMENT_PERIOD,
        		&IMPeriodMilliSeconds);
    }

    if (Status == VL53L0_ERROR_NONE) {
        if (osc_calibrate_val !=0) {
            *pInterMeasurementPeriodMilliSeconds = IMPeriodMilliSeconds /
            		osc_calibrate_val;
        }
        VL53L0_SETPARAMETERFIELD(Dev, InterMeasurementPeriodMilliSeconds,
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
    FixPoint1616_t TempFix1616;

    LOG_FUNCTION_START("");

    if (XTalkCompensationEnable == 0) {
        TempFix1616 = 0;
    } else {
        VL53L0_GETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
        		TempFix1616);
    }

    /* the following register has a format 3.13 */
    Status = VL53L0_WrWord(Dev,
                           VL53L0_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS,
                           VL53L0_FIXPOINT1616TOFIXPOINT313(TempFix1616));

    if (Status == VL53L0_ERROR_NONE) {
        if (XTalkCompensationEnable == 0) {
            VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationEnable, 0);
        } else {
            VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationEnable, 1);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetXTalkCompensationEnable(VL53L0_DEV Dev,
              uint8_t* pXTalkCompensationEnable)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t Temp8;
    LOG_FUNCTION_START("");

    VL53L0_GETPARAMETERFIELD(Dev, XTalkCompensationEnable, Temp8);
    *pXTalkCompensationEnable = Temp8;

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetXTalkCompensationRateMegaCps(VL53L0_DEV Dev,
              FixPoint1616_t XTalkCompensationRateMegaCps)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t Temp8;
    LOG_FUNCTION_START("");

    VL53L0_GETPARAMETERFIELD(Dev, XTalkCompensationEnable, Temp8);

    if (Temp8 == 0) { /* disabled write only internal value */
        VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps, XTalkCompensationRateMegaCps);
    } else {
        /* the following register has a format 3.13 */
        Status = VL53L0_WrWord(Dev,
                               VL53L0_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS,
                               VL53L0_FIXPOINT1616TOFIXPOINT412(XTalkCompensationRateMegaCps));
        if (Status == VL53L0_ERROR_NONE) {
            VL53L0_SETPARAMETERFIELD(Dev,
                                     XTalkCompensationRateMegaCps,
                                     XTalkCompensationRateMegaCps);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetXTalkCompensationRateMegaCps(VL53L0_DEV Dev,
		FixPoint1616_t* pXTalkCompensationRateMegaCps)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint16_t Value;
    FixPoint1616_t TempFix1616;
    VL53L0_DeviceParameters_t CurrentParameters;

    LOG_FUNCTION_START("");

    Status = VL53L0_RdWord(Dev,
    		VL53L0_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS,
    		(uint16_t*) &Value);
    if (Status == VL53L0_ERROR_NONE) {
        if (Value == 0) { /* the Xtalk is disabled return value from memory */
            VL53L0_GETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
            		TempFix1616);
            *pXTalkCompensationRateMegaCps = TempFix1616;
            VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationEnable, 0);
        } else {
            TempFix1616 = VL53L0_FIXPOINT412TOFIXPOINT1616(Value);
            *pXTalkCompensationRateMegaCps = TempFix1616;
            VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps,
            		TempFix1616);
            VL53L0_SETPARAMETERFIELD(Dev, XTalkCompensationEnable, 1);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}


/*
 * CHECK LIMIT FUNCTIONS
 */

VL53L0_Error VL53L0_GetNumberOfLimitCheck(uint16_t* pNumberOfLimitCheck) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    *pNumberOfLimitCheck = VL53L0_CHECKENABLE_NUMBER_OF_CHECKS;

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetLimitCheckInfo(VL53L0_DEV Dev, uint16_t LimitCheckId,
		char* pLimitCheckString)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    switch (LimitCheckId)
    {
    VL53L0_BUILDCASESTRING(pLimitCheckString,
    		VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE,
    		VL53L0_STRING_CHECKENABLE_SIGMA_FINAL_RANGE);

    VL53L0_BUILDCASESTRING(pLimitCheckString,
    		VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
    		VL53L0_STRING_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE);

    VL53L0_BUILDCASESTRING(pLimitCheckString,
    		VL53L0_CHECKENABLE_SIGNAL_REF_CLIP,
    		VL53L0_STRING_CHECKENABLE_SIGNAL_REF_CLIP);

    default:
        VL53L0_COPYSTRING(pLimitCheckString, VL53L0_STRING_UNKNOW_ERROR_CODE);

    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetLimitCheckEnable(VL53L0_DEV Dev, uint16_t LimitCheckId,
		uint8_t LimitCheckEnable)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    FixPoint1616_t TempFix1616 = 0;
    uint8_t LimitCheckEnableInt;

    LOG_FUNCTION_START("");

    if (LimitCheckId >= VL53L0_CHECKENABLE_NUMBER_OF_CHECKS) {
        Status = VL53L0_ERROR_INVALID_PARAMS;
    } else {
        if (LimitCheckEnable == 0) {
            TempFix1616 = 0;
            LimitCheckEnableInt = 0;
        } else {
            VL53L0_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
            		LimitCheckId, TempFix1616);
            /* this to be sure to have either 0 or 1 */
            LimitCheckEnableInt = 1;
        }

        switch (LimitCheckId) {

		case VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE:
			/* internal computation: */
			VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, LimitCheckEnableInt);

			break;

		case VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:

			Status = VL53L0_WrWord(Dev,
					VL53L0_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
					VL53L0_FIXPOINT1616TOFIXPOINT97(TempFix1616));

			break;

		case VL53L0_CHECKENABLE_SIGNAL_REF_CLIP:

			/* internal computation: */
			VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
					VL53L0_CHECKENABLE_SIGNAL_REF_CLIP, LimitCheckEnableInt);

			break;

		default:
			Status = VL53L0_ERROR_INVALID_PARAMS;

        }

    }

    if (Status == VL53L0_ERROR_NONE) {
        if (LimitCheckEnable == 0) {
            VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
            		LimitCheckId, 0);
        } else {
            VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
            		LimitCheckId, 1);
    }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}


VL53L0_Error VL53L0_GetLimitCheckEnable(VL53L0_DEV Dev, uint16_t LimitCheckId,
		uint8_t *pLimitCheckEnable)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t Temp8;

    LOG_FUNCTION_START("");

    if (LimitCheckId >= VL53L0_CHECKENABLE_NUMBER_OF_CHECKS) {
        Status = VL53L0_ERROR_INVALID_PARAMS;
    }

    VL53L0_GETARRAYPARAMETERFIELD(Dev, LimitChecksEnable, LimitCheckId, Temp8);
    *pLimitCheckEnable = Temp8;

    LOG_FUNCTION_END(Status);
    return Status;
}



VL53L0_Error VL53L0_SetLimitCheckValue(VL53L0_DEV Dev, uint16_t LimitCheckId,
		FixPoint1616_t LimitCheckValue)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t Temp8;

    LOG_FUNCTION_START("");

    VL53L0_GETARRAYPARAMETERFIELD(Dev, LimitChecksEnable, LimitCheckId, Temp8);

    if (Temp8 == 0) { /* disabled write only internal value */
        VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue, LimitCheckId,
        		LimitCheckValue);
    } else {

        switch (LimitCheckId) {

		case VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE:
			/* internal computation: */
			VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
					VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, LimitCheckValue);
			break;

		case VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:

			Status = VL53L0_WrWord(Dev,
					VL53L0_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
					VL53L0_FIXPOINT1616TOFIXPOINT97(LimitCheckValue));

			break;

		case VL53L0_CHECKENABLE_SIGNAL_REF_CLIP:

			/* internal computation: */
			VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
					VL53L0_CHECKENABLE_SIGNAL_REF_CLIP, LimitCheckValue);

			break;

		default:
			Status = VL53L0_ERROR_INVALID_PARAMS;

    }

    if (Status == VL53L0_ERROR_NONE) {
            VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue, LimitCheckId,
            		LimitCheckValue);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}


VL53L0_Error VL53L0_GetLimitCheckValue(VL53L0_DEV Dev, uint16_t LimitCheckId,
		FixPoint1616_t *pLimitCheckValue)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint8_t EnableZeroValue = 0;
    uint16_t Temp16;
    FixPoint1616_t TempFix1616;

    LOG_FUNCTION_START("");

    switch (LimitCheckId) {

	case VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE:
		/* internal computation: */
		VL53L0_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, TempFix1616);
		EnableZeroValue = 0;
		break;

	case VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
		Status = VL53L0_RdWord(Dev,
				VL53L0_REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
				&Temp16);
		if (Status == VL53L0_ERROR_NONE) {
			TempFix1616 = VL53L0_FIXPOINT97TOFIXPOINT1616(Temp16);
		}

		EnableZeroValue = 1;
		break;

	case VL53L0_CHECKENABLE_SIGNAL_REF_CLIP:
		/* internal computation: */
		VL53L0_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
				VL53L0_CHECKENABLE_SIGNAL_REF_CLIP, TempFix1616);
		EnableZeroValue = 0;
		break;

	default:
		Status = VL53L0_ERROR_INVALID_PARAMS;

    }

    if (Status == VL53L0_ERROR_NONE) {

        if (EnableZeroValue == 1) {

            if (TempFix1616 == 0) { /* disabled: return value from memory */
                VL53L0_GETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
                		LimitCheckId, TempFix1616);
                *pLimitCheckValue = TempFix1616;
                VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
                		LimitCheckId, 0);
            } else {
                *pLimitCheckValue = TempFix1616;
                VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksValue,
                		LimitCheckId, TempFix1616);
                VL53L0_SETARRAYPARAMETERFIELD(Dev, LimitChecksEnable,
                		LimitCheckId, 1);
            }
        } else {
            *pLimitCheckValue = TempFix1616;
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;

}

VL53L0_Error VL53L0_GetLimitCheckCurrent(VL53L0_DEV Dev,
		uint16_t LimitCheckId, FixPoint1616_t *pLimitCheckCurrent)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_RangingMeasurementData_t LastRangeDataBuffer;

	LOG_FUNCTION_START("");

	if (LimitCheckId >= VL53L0_CHECKENABLE_NUMBER_OF_CHECKS) {
		Status = VL53L0_ERROR_INVALID_PARAMS;
	} else {
		switch (LimitCheckId) {
		case VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE:
			/* Need to run a ranging to have the latest values */
			*pLimitCheckCurrent = PALDevDataGet(Dev, SigmaEstimate);

			break;

		case VL53L0_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE:
			/* Need to run a ranging to have the latest values */
			LastRangeDataBuffer = PALDevDataGet(Dev, LastRangeMeasure);
			*pLimitCheckCurrent = LastRangeDataBuffer.SignalRateRtnMegaCps;

			break;

		case VL53L0_CHECKENABLE_SIGNAL_REF_CLIP:
			/* Need to run a ranging to have the latest values */
			*pLimitCheckCurrent = PALDevDataGet(Dev, LastSignalRefMcps);

			break;

		default:
			Status = VL53L0_ERROR_INVALID_PARAMS;
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;

}


/*
 * WRAPAROUND Check
 */
VL53L0_Error VL53L0_SetWrapAroundCheckEnable(VL53L0_DEV Dev,
		uint8_t WrapAroundCheckEnable)
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
        /*Enable wraparound */
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

VL53L0_Error VL53L0_GetWrapAroundCheckEnable(VL53L0_DEV Dev,
		uint8_t* pWrapAroundCheckEnable)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t data;
    VL53L0_DeviceParameters_t CurrentParameters;

    LOG_FUNCTION_START("");

    Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, &data);
    if (Status == VL53L0_ERROR_NONE) {
        PALDevDataSet(Dev, SequenceConfig, data);
        if (data & (0x01 << 7)) {
            *pWrapAroundCheckEnable = 0x01;
        } else {
            *pWrapAroundCheckEnable = 0x00;
        }
    }
    if (Status == VL53L0_ERROR_NONE) {
        VL53L0_SETPARAMETERFIELD(Dev, WrapAroundCheckEnable,
        		*pWrapAroundCheckEnable);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

/* End Group PAL Parameters Functions */

 /* Group PAL Measurement Functions */
VL53L0_Error VL53L0_PerformSingleMeasurement(VL53L0_DEV Dev)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceModes DeviceMode;

    LOG_FUNCTION_START("");

    /* Get Current DeviceMode */
    Status = VL53L0_GetDeviceMode(Dev, &DeviceMode);

    /* Start immediately to run a single ranging measurement in case of single
     * ranging or single histogram */
    if (Status     == VL53L0_ERROR_NONE &&
        DeviceMode == VL53L0_DEVICEMODE_SINGLE_RANGING)
    {
        Status = VL53L0_StartMeasurement(Dev);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_measurement_poll_for_completion(Dev);
    }

    /* Change PAL State in case of single ranging or single histogram */
    if (Status     == VL53L0_ERROR_NONE &&
        DeviceMode == VL53L0_DEVICEMODE_SINGLE_RANGING)
    {
        PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_PerformSingleHistogramMeasurement(VL53L0_DEV Dev,
		VL53L0_HistogramMeasurementData_t* pHistogramMeasurementData)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceModes DeviceMode;
    VL53L0_HistogramModes HistogramMode = VL53L0_HISTOGRAMMODE_DISABLED;
    uint32_t MeasCount;
    uint32_t Measurements;

    /* Get Current DeviceMode */
    Status = VL53L0_GetHistogramMode(Dev, &HistogramMode);

    if (Status == VL53L0_ERROR_NONE)
    {
        if(HistogramMode == VL53L0_HISTOGRAMMODE_BOTH)
        {
            if(pHistogramMeasurementData->BufferSize < VL53L0_HISTOGRAM_BUFFER_SIZE)
            {
                Status = VL53L0_ERROR_BUFFER_TOO_SMALL;
            }
        }
        else
        {
            if(pHistogramMeasurementData->BufferSize < VL53L0_HISTOGRAM_BUFFER_SIZE/2)
            {
                Status = VL53L0_ERROR_BUFFER_TOO_SMALL;
            }
        }
        pHistogramMeasurementData->HistogramType = (uint8_t)HistogramMode;
        pHistogramMeasurementData->ErrorStatus   = VL53L0_DEVICEERROR_NONE;
        pHistogramMeasurementData->FirstBin      = 0;
        pHistogramMeasurementData->NumberOfBins  = 0;
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        /* Get Current DeviceMode */
        Status = VL53L0_GetDeviceMode(Dev, &DeviceMode);
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_HISTOGRAM_BIN, 0x00);
    }

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT, 0x00);

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_HISTOGRAM_CONFIG_READOUT_CTRL, 0x01);

    if (Status == VL53L0_ERROR_NONE) {
        Measurements = 3;
        if(HistogramMode == VL53L0_HISTOGRAMMODE_BOTH)
        {
            Measurements = 6;
        }

        if(DeviceMode == VL53L0_DEVICEMODE_SINGLE_HISTOGRAM)
        {
            MeasCount = 0;
            while((MeasCount < Measurements) && (Status == VL53L0_ERROR_NONE))
            {
                Status = VL53L0_start_histogram_measurement(Dev, HistogramMode,
                		MeasCount);

                if (Status == VL53L0_ERROR_NONE)
                    VL53L0_confirm_measurement_start(Dev);

                if (Status == VL53L0_ERROR_NONE)
                    PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);

                if (Status == VL53L0_ERROR_NONE)
                    Status = VL53L0_measurement_poll_for_completion(Dev);

                if (Status == VL53L0_ERROR_NONE)
                {
                    Status = VL53L0_read_histo_measurement(Dev,
								   pHistogramMeasurementData->HistogramData,
								   MeasCount,
								   HistogramMode);
                    if(Status==VL53L0_ERROR_NONE)
                    {
                        /*
                         * When reading both rtn and ref arrays, histograms
                         * are read two bins at a time. For rtn or ref only,
                         * histograms are read four bins at a time.
                         */
                        if(HistogramMode == VL53L0_HISTOGRAMMODE_BOTH)
                            pHistogramMeasurementData->NumberOfBins+= 2;
                        else
                            pHistogramMeasurementData->NumberOfBins+= 4;

                    }
                }

                if(Status==VL53L0_ERROR_NONE)
                    Status = VL53L0_ClearInterruptMask(Dev, 0);

                MeasCount++;
            }
        } else {
            Status = VL53L0_ERROR_INVALID_COMMAND;
        }
    }


    /* Change PAL State in case of single ranging or single histogram */
    if (Status     == VL53L0_ERROR_NONE)
    {
        pHistogramMeasurementData->NumberOfBins = 12;
        PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
    }


    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_measurement_poll_for_completion(VL53L0_DEV Dev){
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t NewDataReady=0;
    uint32_t LoopNb;

    LOG_FUNCTION_START("");

    LoopNb = 0;
    do {
        Status = VL53L0_GetMeasurementDataReady(Dev, &NewDataReady);
        VL53L0_PollingDelay(Dev);
        LoopNb++;
    } while((NewDataReady == 0 ) &&
            (Status == VL53L0_ERROR_NONE) &&
            (LoopNb < VL53L0_DEFAULT_MAX_LOOP));

    LOG_FUNCTION_END(Status);

    return Status;
}

VL53L0_Error VL53L0_confirm_measurement_start(VL53L0_DEV Dev){
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t NewDataReady=0;
    uint32_t LoopNb;

    LOG_FUNCTION_START("");

    LoopNb = 0;
    do {
        Status = VL53L0_GetMeasurementDataReady(Dev, &NewDataReady);
        if ((NewDataReady == 0x01) || Status != VL53L0_ERROR_NONE) {
            break;
        }
        LoopNb = LoopNb + 1;
        VL53L0_PollingDelay(Dev);
    } while (LoopNb < VL53L0_DEFAULT_MAX_LOOP);

    if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP) {
        Status = VL53L0_ERROR_TIME_OUT;
    }

    LOG_FUNCTION_END(Status);

    return Status;
}

VL53L0_Error VL53L0_perform_single_ref_calibration(VL53L0_DEV Dev)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t Byte = 0;
    uint32_t LoopNb;

    LOG_FUNCTION_START("");

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
        		VL53L0_REG_SYSRANGE_MODE_START_STOP);
    }

    if (Status == VL53L0_ERROR_NONE) {
        /* Wait until start bit has been cleared */
        LoopNb = 0;
        do {
            if (LoopNb > 0)
                Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSRANGE_START, &Byte);
            LoopNb = LoopNb + 1;
        } while (((Byte & VL53L0_REG_SYSRANGE_MODE_START_STOP) ==
        		VL53L0_REG_SYSRANGE_MODE_START_STOP) &&
                  (Status == VL53L0_ERROR_NONE) &&
                  (LoopNb < VL53L0_DEFAULT_MAX_LOOP));
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_measurement_poll_for_completion(Dev);
    }
    if(Status==VL53L0_ERROR_NONE){
        Status = VL53L0_ClearInterruptMask(Dev, 0);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_PerformRefCalibration(VL53L0_DEV Dev)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t SequenceConfig = 0;

    LOG_FUNCTION_START("");

    /* store the value of the sequence config,
     * this will be reset before the end of the function
     */

    SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

    Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, 0x03);

    /* Run VHV */
    PALDevDataSet(Dev, SequenceConfig, 0x01);

    if(Status==VL53L0_ERROR_NONE)
    	Status = VL53L0_perform_single_ref_calibration(Dev);

    /* Run PhaseCal */
    PALDevDataSet(Dev, SequenceConfig, 0x02);

    if(Status==VL53L0_ERROR_NONE)
    	Status = VL53L0_perform_single_ref_calibration(Dev);

    if(Status==VL53L0_ERROR_NONE){
        /* restore the previous Sequence Config */
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG,
        		SequenceConfig);
		if(Status==VL53L0_ERROR_NONE){
			PALDevDataSet(Dev, SequenceConfig, SequenceConfig);
		}
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_API VL53L0_Error VL53L0_PerformXTalkCalibration(VL53L0_DEV Dev,
            FixPoint1616_t XTalkCalDistance,
            FixPoint1616_t* pXTalkCompensationRateMegaCps) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint16_t sum_ranging = 0;
    uint16_t sum_spads =0;
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

    if (XTalkCalDistance<=0) {
        Status = VL53L0_ERROR_INVALID_PARAMS;
    }

    /* Disable the XTalk compensation */
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_SetXTalkCompensationEnable(Dev, 0);
    }

    /* Perform 50 measurements and compute the averages */
    if (Status == VL53L0_ERROR_NONE) {
        sum_ranging = 0;
        sum_spads =0;
        sum_signalRate = 0;
        total_count = 0;
        for(xtalk_meas=0;xtalk_meas<50;xtalk_meas++)
        {
            Status = VL53L0_PerformSingleRangingMeasurement(Dev, &RangingMeasurementData);

            if (Status != VL53L0_ERROR_NONE) {
                break;
            }

            /* The range is valid when RangeStatus = 0 */
            if (RangingMeasurementData.RangeStatus == 0) {
                sum_ranging = sum_ranging + RangingMeasurementData.RangeMilliMeter;
                sum_signalRate = sum_signalRate + RangingMeasurementData.SignalRateRtnMegaCps;
                sum_spads = sum_spads + RangingMeasurementData.EffectiveSpadRtnCount/32;
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
        xTalkStoredMeanRange = (FixPoint1616_t)((uint32_t)(sum_ranging<<16) / total_count);
        xTalkStoredMeanRtnSpads = (FixPoint1616_t)((uint32_t)(sum_spads<<16) / total_count);

        /* Round Mean Spads to Whole Number.
         * Typically the calculated mean SPAD count is a whole number or very close to a whole
         * number, therefore any truncation will not result in a significant loss in accuracy.
         * Also, for a grey target at a typical distance of around 400mm, around 220 SPADs will
         * be enabled, therefore, any truncation will result in a loss of accuracy of less than
         * 0.5%.
         */
        xTalkStoredMeanRtnSpadsAsInt = (xTalkStoredMeanRtnSpads + 0x8000) >> 16;

        /* Round Cal Distance to Whole Number.
         * Note that the cal distance is in mm, therefore no resolution is lost.*/
         xTalkCalDistanceAsInt = (XTalkCalDistance + 0x8000) >> 16;

        if(xTalkStoredMeanRtnSpadsAsInt == 0 ||
           xTalkCalDistanceAsInt == 0 ||
           xTalkStoredMeanRange >= XTalkCalDistance)
        {
            XTalkCompensationRateMegaCps = 0;
        }
        else
        {
            /* Round Cal Distance to Whole Number.
               Note that the cal distance is in mm, therefore no resolution is lost.*/
            xTalkCalDistanceAsInt = (XTalkCalDistance + 0x8000) >> 16;

            /* Apply division by mean spad count early in the calculation to keep the numbers small.
             * This ensures we can maintain a 32bit calculation.
             * Fixed1616 / int := Fixed1616 */
            signalXTalkTotalPerSpad = (xTalkStoredMeanSignalRate)/xTalkStoredMeanRtnSpadsAsInt;

            /* Complete the calculation for total Signal XTalk per SPAD
             * Fixed1616 * (Fixed1616 - Fixed1616/int) := (2^16 * Fixed1616)
             */
            signalXTalkTotalPerSpad *= ((1<<16) - (xTalkStoredMeanRange/xTalkCalDistanceAsInt));

            /* Round from 2^16 * Fixed1616, to Fixed1616. */
            XTalkCompensationRateMegaCps = (signalXTalkTotalPerSpad + 0x8000) >> 16;
        }

        *pXTalkCompensationRateMegaCps = XTalkCompensationRateMegaCps;

        /* Enable the XTalk compensation */
        if (Status == VL53L0_ERROR_NONE) {
            Status = VL53L0_SetXTalkCompensationEnable(Dev, 1);
        }

        /* Enable the XTalk compensation */
        if (Status == VL53L0_ERROR_NONE) {
            Status = VL53L0_SetXTalkCompensationRateMegaCps(Dev, XTalkCompensationRateMegaCps);
        }

    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_API VL53L0_Error VL53L0_PerformOffsetCalibration(VL53L0_DEV Dev,
            FixPoint1616_t CalDistanceMilliMeter,
            int32_t* pOffsetMicroMeter) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint16_t sum_ranging = 0;
    FixPoint1616_t total_count = 0;
    VL53L0_RangingMeasurementData_t RangingMeasurementData;
    FixPoint1616_t StoredMeanRange;
    uint32_t StoredMeanRangeAsInt;
    VL53L0_DeviceParameters_t CurrentParameters;
    uint32_t CalDistanceAsInt_mm;
    int meas = 0;
    LOG_FUNCTION_START("");

    if (CalDistanceMilliMeter<=0) {
        Status = VL53L0_ERROR_INVALID_PARAMS;
    }

    if (Status == VL53L0_ERROR_NONE) {
        VL53L0_SetOffsetCalibrationDataMicroMeter(Dev, 0);
    }

    /* Perform 50 measurements and compute the averages */
    if (Status == VL53L0_ERROR_NONE) {
        sum_ranging = 0;
        total_count = 0;
        for(meas=0;meas<50;meas++)
        {
            Status = VL53L0_PerformSingleRangingMeasurement(Dev, &RangingMeasurementData);

            if (Status != VL53L0_ERROR_NONE) {
                break;
            }

            /* The range is valid when RangeStatus = 0 */
            if (RangingMeasurementData.RangeStatus == 0) {
                sum_ranging = sum_ranging + RangingMeasurementData.RangeMilliMeter;
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
        StoredMeanRange = (FixPoint1616_t)((uint32_t)(sum_ranging<<16) / total_count);

        StoredMeanRangeAsInt = (StoredMeanRange + 0x8000) >> 16;

        /* Round Cal Distance to Whole Number.
         * Note that the cal distance is in mm, therefore no resolution is lost.*/
         CalDistanceAsInt_mm = (CalDistanceMilliMeter + 0x8000) >> 16;

         *pOffsetMicroMeter = (CalDistanceAsInt_mm - StoredMeanRangeAsInt) * 1000;

        /* Apply the calculated offset */
        if (Status == VL53L0_ERROR_NONE) {
            VL53L0_SETPARAMETERFIELD(Dev, RangeOffsetMicroMeters, *pOffsetMicroMeter)
            Status = VL53L0_SetOffsetCalibrationDataMicroMeter(Dev, *pOffsetMicroMeter);
        }

    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_StartMeasurement(VL53L0_DEV Dev) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceModes DeviceMode;
    uint8_t Byte;
    uint32_t LoopNb;
    LOG_FUNCTION_START("");

    /* Get Current DeviceMode */
    VL53L0_GetDeviceMode(Dev, &DeviceMode);

    switch (DeviceMode)
    {
    case VL53L0_DEVICEMODE_SINGLE_RANGING:
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START, VL53L0_REG_SYSRANGE_MODE_SINGLESHOT | VL53L0_REG_SYSRANGE_MODE_START_STOP);
        break;
    case VL53L0_DEVICEMODE_CONTINUOUS_RANGING:
        /* Back-to-back mode */
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START, VL53L0_REG_SYSRANGE_MODE_BACKTOBACK | VL53L0_REG_SYSRANGE_MODE_START_STOP);
        if (Status == VL53L0_ERROR_NONE) {
            /* Set PAL State to Running */
            PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);
        }
        break;
    case VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING:
        /* Continuous mode */
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START, VL53L0_REG_SYSRANGE_MODE_TIMED | VL53L0_REG_SYSRANGE_MODE_START_STOP);
        if (Status == VL53L0_ERROR_NONE) {
            /* Set PAL State to Running */
            PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);
        }
        break;
    default:
        /* Selected mode not supported */
        Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
    }

    Byte = VL53L0_REG_SYSRANGE_MODE_START_STOP;
    if (Status == VL53L0_ERROR_NONE) {
        /* Wait until start bit has been cleared */
        LoopNb = 0;
        do {
            if (LoopNb > 0)
                Status = VL53L0_RdByte(Dev, VL53L0_REG_SYSRANGE_START, &Byte);
            LoopNb = LoopNb + 1;
        } while (((Byte & VL53L0_REG_SYSRANGE_MODE_START_STOP) == VL53L0_REG_SYSRANGE_MODE_START_STOP) &&
                  (Status == VL53L0_ERROR_NONE) &&
                  (LoopNb < VL53L0_DEFAULT_MAX_LOOP));

        if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP) {
            Status = VL53L0_ERROR_TIME_OUT;
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}


VL53L0_Error VL53L0_start_histogram_measurement(VL53L0_DEV Dev,
              VL53L0_HistogramModes histoMode,
              uint32_t count)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t dataByte;
    LOG_FUNCTION_START("");


    dataByte = VL53L0_REG_SYSRANGE_MODE_SINGLESHOT | VL53L0_REG_SYSRANGE_MODE_START_STOP;
    if(count == 0)
    {
        /* First histogram measurement must have bit 5 set */
        dataByte |= (1 << 5);
    }

    switch (histoMode)
    {
        case VL53L0_HISTOGRAMMODE_DISABLED:
            /* Selected mode not supported */
            Status = VL53L0_ERROR_INVALID_COMMAND;
            break;

        case VL53L0_HISTOGRAMMODE_REFERENCE_ONLY:
        case VL53L0_HISTOGRAMMODE_RETURN_ONLY:
        case VL53L0_HISTOGRAMMODE_BOTH:
            dataByte |= (histoMode << 3);
            Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START, dataByte);
            if (Status == VL53L0_ERROR_NONE)
            {
                /* Set PAL State to Running */
                PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);
            }
            break;

        default:
            /* Selected mode not supported */
            Status = VL53L0_ERROR_MODE_NOT_SUPPORTED;
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_StopMeasurement(VL53L0_DEV Dev) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START, VL53L0_REG_SYSRANGE_MODE_SINGLESHOT);

    if (Status == VL53L0_ERROR_NONE) {
        /* Set PAL State to Idle */
        PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetMeasurementDataReady(VL53L0_DEV Dev, uint8_t *pMeasurementDataReady) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t SysRangeStatusRegister;
    uint8_t InterruptConfig;
    uint32_t InterruptMask;
    LOG_FUNCTION_START("");

    InterruptConfig = VL53L0_GETDEVICESPECIFICPARAMETER(Dev, Pin0GpioFunctionality);

    if (InterruptConfig == VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY) {
        VL53L0_GetInterruptMaskStatus(Dev, &InterruptMask);
        if (InterruptMask == VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY) {
            *pMeasurementDataReady = 1;
        } else {
            *pMeasurementDataReady = 0;
        }
    } else {
        Status = VL53L0_RdByte(Dev, VL53L0_REG_RESULT_RANGE_STATUS, &SysRangeStatusRegister);
        if (Status == VL53L0_ERROR_NONE) {
            if (SysRangeStatusRegister & 0x01) {
                *pMeasurementDataReady = 1;
            } else {
                *pMeasurementDataReady = 0;
            }
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_WaitDeviceReadyForNewMeasurement(VL53L0_DEV Dev, uint32_t MaxLoop) {
    VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
    LOG_FUNCTION_START("");

    /* not implemented for VL53L0 */

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetRangingMeasurementData(VL53L0_DEV Dev,
              VL53L0_RangingMeasurementData_t* pRangingMeasurementData)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t DeviceRangeStatus;
    uint8_t RangeFractionalEnable;
    uint8_t PalRangeStatus;
    uint16_t AmbientRate;
    FixPoint1616_t SignalRate;
    uint16_t CrosstalkCompensation;
    uint16_t EffectiveSpadRtnCount;
    uint16_t tmpuint16;
    uint8_t localBuffer[14];
    VL53L0_RangingMeasurementData_t LastRangeDataBuffer;
    LOG_FUNCTION_START("");

    /*
     * use multi read even if some registers are not useful, result will be more efficient
     * start reading at 0x14 dec20
     * end reading at 0x21 dec33 total 14 bytes to read
     */
    Status = VL53L0_ReadMulti(Dev, 0x14, localBuffer, 14);

    if (Status == VL53L0_ERROR_NONE) {

        pRangingMeasurementData->ZoneId = 0; /* Only one zone */
        pRangingMeasurementData->TimeStamp = 0; /* Not Implemented */

        tmpuint16 = VL53L0_MAKEUINT16(localBuffer[11], localBuffer[10]);
        /* cut1.1 if SYSTEM__RANGE_CONFIG if 1 range is 2bits fractional
         *(format 11.2) else no fractional
         */

        /* Get ranging configuration */
        RangeFractionalEnable = PALDevDataGet(Dev, RangeFractionalEnable);

        if (RangeFractionalEnable) {
            pRangingMeasurementData->RangeMilliMeter = (uint16_t)((tmpuint16)>>2);
            pRangingMeasurementData->RangeFractionalPart = (uint8_t)((tmpuint16 & 0x03)<<6);
        } else {
            pRangingMeasurementData->RangeMilliMeter = tmpuint16;
            pRangingMeasurementData->RangeFractionalPart = 0;
        }


        pRangingMeasurementData->RangeDMaxMilliMeter = 0;
        pRangingMeasurementData->MeasurementTimeUsec = 0;

        SignalRate = VL53L0_FIXPOINT97TOFIXPOINT1616(VL53L0_MAKEUINT16(localBuffer[7], localBuffer[6]));
        pRangingMeasurementData->SignalRateRtnMegaCps = SignalRate; /* peak_signal_count_rate_rtn_mcps */

        AmbientRate = VL53L0_MAKEUINT16(localBuffer[9], localBuffer[8]);
        pRangingMeasurementData->AmbientRateRtnMegaCps = VL53L0_FIXPOINT97TOFIXPOINT1616(AmbientRate);

        EffectiveSpadRtnCount = VL53L0_MAKEUINT16(localBuffer[3], localBuffer[2]);
        pRangingMeasurementData->EffectiveSpadRtnCount = EffectiveSpadRtnCount; /* 8.8 format */

        DeviceRangeStatus = localBuffer[0];

        /* format is = 3.13 */
        CrosstalkCompensation = VL53L0_MAKEUINT16(localBuffer[13], localBuffer[12]);

        /*
         * For a standard definition of RangeStatus, this should return 0 in
         * case of good result after a ranging
         * The range status depends on the device so call a device specific
         * function to obtain the right Status.
         */
        Status = VL53L0_get_pal_range_status(Dev,
                                             DeviceRangeStatus,
                                             SignalRate,
                                             CrosstalkCompensation,
                                             EffectiveSpadRtnCount,
                                             pRangingMeasurementData,
                                             &PalRangeStatus);

        if (Status == VL53L0_ERROR_NONE) {
            pRangingMeasurementData->RangeStatus = PalRangeStatus;
        }
    }

    if (Status == VL53L0_ERROR_NONE) {
        /* Copy last read data into Dev buffer */
        LastRangeDataBuffer = PALDevDataGet(Dev, LastRangeMeasure);

        LastRangeDataBuffer.RangeMilliMeter = pRangingMeasurementData->RangeMilliMeter;
        LastRangeDataBuffer.RangeFractionalPart = pRangingMeasurementData->RangeFractionalPart;
        LastRangeDataBuffer.RangeDMaxMilliMeter = pRangingMeasurementData->RangeDMaxMilliMeter;
        LastRangeDataBuffer.MeasurementTimeUsec = pRangingMeasurementData->MeasurementTimeUsec;
        LastRangeDataBuffer.SignalRateRtnMegaCps = pRangingMeasurementData->SignalRateRtnMegaCps;
        LastRangeDataBuffer.AmbientRateRtnMegaCps = pRangingMeasurementData->AmbientRateRtnMegaCps;
        LastRangeDataBuffer.EffectiveSpadRtnCount = pRangingMeasurementData->EffectiveSpadRtnCount;
        LastRangeDataBuffer.RangeStatus = pRangingMeasurementData->RangeStatus;

        PALDevDataSet(Dev, LastRangeMeasure, LastRangeDataBuffer);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetHistogramMeasurementData(VL53L0_DEV Dev,
              VL53L0_HistogramMeasurementData_t* pHistogramMeasurementData) {
    VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
    LOG_FUNCTION_START("");

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_read_histo_measurement(VL53L0_DEV Dev,
              uint32_t *histoData,
              uint32_t offset,
              VL53L0_HistogramModes histoMode) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t localBuffer[28];
    uint32_t cDataSize  = 4;
    uint32_t offset1;

    LOG_FUNCTION_START("");

    Status = VL53L0_WrByte(Dev, 0xFF, VL53L0_REG_RESULT_CORE_PAGE);
    Status = VL53L0_ReadMulti(Dev,
                              (uint8_t)VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN,
                              localBuffer,
                              28);
    Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);

    if (Status == VL53L0_ERROR_NONE)
    {
        VL53L0_reverse_bytes(&localBuffer[0], cDataSize);
        VL53L0_reverse_bytes(&localBuffer[4], cDataSize);
        VL53L0_reverse_bytes(&localBuffer[20], cDataSize);
        VL53L0_reverse_bytes(&localBuffer[24], cDataSize);

        offset1 = offset * cDataSize;
        if(histoMode == VL53L0_HISTOGRAMMODE_BOTH)
        {
            /*
             * When reading both return and ref data, each measurement reads two
             * ref values and two return values.
             * Data is stored in an interleaved sequence, starting with the return
             * histogram.
             *
             * Some result Core registers are reused for the histogram measurements
             *
             * The bin values are retrieved in the following order
             *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_RTN
             *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_REF
             *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN
             *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF
             */

            memcpy(&histoData[offset1],     &localBuffer[4],  cDataSize); /* rtn */
            memcpy(&histoData[offset1 + 1], &localBuffer[24], cDataSize); /* ref */
            memcpy(&histoData[offset1 + 2], &localBuffer[0],  cDataSize); /* rtn */
            memcpy(&histoData[offset1 + 3], &localBuffer[20], cDataSize); /* ref */

        }
        else
        {
            /*
             * When reading either return and ref data, each measurement reads four
             * bin values.
             *
             * The bin values are retrieved in the following order
             *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_RTN
             *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN
             *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_REF
             *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF
             */

            memcpy(&histoData[offset1],     &localBuffer[24], cDataSize);
            memcpy(&histoData[offset1 + 1], &localBuffer[20], cDataSize);
            memcpy(&histoData[offset1 + 2], &localBuffer[4],  cDataSize);
            memcpy(&histoData[offset1 + 3], &localBuffer[0],  cDataSize);

        }
    }

    LOG_FUNCTION_END(Status);

    return Status;
}


VL53L0_Error VL53L0_PerformSingleRangingMeasurement(VL53L0_DEV Dev,
		VL53L0_RangingMeasurementData_t* pRangingMeasurementData)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    /* This function will do a complete single ranging
     * Here we fix the mode! */
    Status = VL53L0_SetDeviceMode(Dev, VL53L0_DEVICEMODE_SINGLE_RANGING);

    if (Status == VL53L0_ERROR_NONE) {
    	Status = VL53L0_PerformSingleMeasurement(Dev);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_GetRangingMeasurementData(Dev, pRangingMeasurementData);
    }

    if(Status==VL53L0_ERROR_NONE){
        Status = VL53L0_ClearInterruptMask(Dev, 0);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetNumberOfROIZones(VL53L0_DEV Dev,
		uint8_t NumberOfROIZones)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    if (NumberOfROIZones != 1){
        Status = VL53L0_ERROR_INVALID_PARAMS;
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetNumberOfROIZones(VL53L0_DEV Dev,
		uint8_t* pNumberOfROIZones)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    *pNumberOfROIZones = 1;

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetMaxNumberOfROIZones(VL53L0_DEV Dev,
		uint8_t* pMaxNumberOfROIZones)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;

    LOG_FUNCTION_START("");

    *pMaxNumberOfROIZones = 1;

    LOG_FUNCTION_END(Status);
    return Status;
}

/* End Group PAL Measurement Functions */


VL53L0_Error VL53L0_SetGpioConfig(VL53L0_DEV Dev, uint8_t Pin,
		VL53L0_DeviceModes DeviceMode,
        VL53L0_GpioFunctionality Functionality,
		VL53L0_InterruptPolarity Polarity)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
    uint8_t data;

    LOG_FUNCTION_START("");

    if (Pin != 0) {
        Status = VL53L0_ERROR_GPIO_NOT_EXISTING;
    } else if (DeviceMode == VL53L0_DEVICEMODE_GPIO_DRIVE) {
        if (Polarity == VL53L0_INTERRUPTPOLARITY_LOW) {
            data = 0x10;
        } else {
            data = 1;
        }
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_GPIO_HV_MUX_ACTIVE_HIGH, data);

    } else if (DeviceMode == VL53L0_DEVICEMODE_GPIO_OSC) {

    	Status |= VL53L0_WrByte(Dev, 0xff, 0x01);
    	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);

    	Status |= VL53L0_WrByte(Dev, 0xff, 0x00);
    	Status |= VL53L0_WrByte(Dev, 0x80, 0x01);
    	Status |= VL53L0_WrByte(Dev, 0x85, 0x02);

    	Status |= VL53L0_WrByte(Dev, 0xff, 0x04);
    	Status |= VL53L0_WrByte(Dev, 0xcd, 0x00);
    	Status |= VL53L0_WrByte(Dev, 0xcc, 0x11);

    	Status |= VL53L0_WrByte(Dev, 0xff, 0x07);
    	Status |= VL53L0_WrByte(Dev, 0xbe, 0x00);

    	Status |= VL53L0_WrByte(Dev, 0xff, 0x06);
    	Status |= VL53L0_WrByte(Dev, 0xcc, 0x09);

    	Status |= VL53L0_WrByte(Dev, 0xff, 0x00);
    	Status |= VL53L0_WrByte(Dev, 0xff, 0x01);
    	Status |= VL53L0_WrByte(Dev, 0x00, 0x00);

    } else {

		if (Status == VL53L0_ERROR_NONE) {
			switch(Functionality){
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

		if(Status==VL53L0_ERROR_NONE)
			Status = VL53L0_WrByte(Dev,
					VL53L0_REG_SYSTEM_INTERRUPT_CONFIG_GPIO, data);


		if(Status==VL53L0_ERROR_NONE){
			if (Polarity == VL53L0_INTERRUPTPOLARITY_LOW) {
				data = 0;
			} else {
				data = (uint8_t)(1<<4);
			}
			Status = VL53L0_UpdateByte(Dev,
					VL53L0_REG_GPIO_HV_MUX_ACTIVE_HIGH, 0xEF, data);
		}

		if(Status==VL53L0_ERROR_NONE)
			VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
					Pin0GpioFunctionality, Functionality);


		if(Status==VL53L0_ERROR_NONE)
			Status = VL53L0_ClearInterruptMask(Dev, 0);

    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetGpioConfig(VL53L0_DEV Dev, uint8_t Pin,
		VL53L0_DeviceModes* DeviceMode,
		VL53L0_GpioFunctionality* pFunctionality,
		VL53L0_InterruptPolarity* pPolarity)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
    VL53L0_GpioFunctionality GpioFunctionality;
    uint8_t data;

    LOG_FUNCTION_START("");

    if(Pin != 0){
        Status = VL53L0_ERROR_GPIO_NOT_EXISTING;
    } else {
        Status = VL53L0_RdByte(Dev,
        		VL53L0_REG_SYSTEM_INTERRUPT_CONFIG_GPIO, &data);
    }

    if (Status == VL53L0_ERROR_NONE) {
        switch(data&0x07){
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
                GpioFunctionality = VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY;
                break;
            default:
                Status = VL53L0_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED;
        }
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdByte(Dev, VL53L0_REG_GPIO_HV_MUX_ACTIVE_HIGH, &data);
    }

    if (Status == VL53L0_ERROR_NONE) {
        if ((data & (uint8_t)(1<<4)) == 0) {
            *pPolarity = VL53L0_INTERRUPTPOLARITY_LOW;
        } else {
            *pPolarity = VL53L0_INTERRUPTPOLARITY_HIGH;
        }
    }

    if(Status==VL53L0_ERROR_NONE){
        *pFunctionality = GpioFunctionality;
        VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
        		Pin0GpioFunctionality, GpioFunctionality);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetInterruptThresholds(VL53L0_DEV Dev,
        VL53L0_DeviceModes DeviceMode, FixPoint1616_t ThresholdLow,
        FixPoint1616_t ThresholdHigh) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint16_t Threshold16;
    LOG_FUNCTION_START("");

    /* no dependency on DeviceMode for Ewok */
    /* Need to divide by 2 because the FW will apply a x2 */
    Threshold16 = (uint16_t)((ThresholdLow>>17) & 0x00fff);
    Status = VL53L0_WrWord(Dev, VL53L0_REG_SYSTEM_THRESH_LOW, Threshold16);

    if(Status==VL53L0_ERROR_NONE){
        /* Need to divide by 2 because the FW will apply a x2 */
        Threshold16 = (uint16_t)((ThresholdHigh>>17) & 0x00fff);
        Status = VL53L0_WrWord(Dev, VL53L0_REG_SYSTEM_THRESH_HIGH, Threshold16);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetInterruptThresholds(VL53L0_DEV Dev,
    VL53L0_DeviceModes DeviceMode, FixPoint1616_t* pThresholdLow,
    FixPoint1616_t* pThresholdHigh) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint16_t Threshold16;
    LOG_FUNCTION_START("");

    /* no dependency on DeviceMode for Ewok */

    Status = VL53L0_RdWord(Dev, VL53L0_REG_SYSTEM_THRESH_LOW, &Threshold16);
    /* Need to multiply by 2 because the FW will apply a x2 */
    *pThresholdLow = (FixPoint1616_t)((0x00fff & Threshold16)<<17);

    if(Status==VL53L0_ERROR_NONE){
       Status = VL53L0_RdWord(Dev, VL53L0_REG_SYSTEM_THRESH_HIGH, &Threshold16);
       /* Need to multiply by 2 because the FW will apply a x2 */
       *pThresholdHigh = (FixPoint1616_t)((0x00fff & Threshold16)<<17);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

/* Group PAL Interrupt Functions */
VL53L0_Error VL53L0_ClearInterruptMask(VL53L0_DEV Dev, uint32_t InterruptMask) {
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

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetInterruptMaskStatus(VL53L0_DEV Dev,
        uint32_t *pInterruptMaskStatus)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t Byte;
    LOG_FUNCTION_START("");

    Status = VL53L0_RdByte(Dev, VL53L0_REG_RESULT_INTERRUPT_STATUS, &Byte);
    *pInterruptMaskStatus = Byte & 0x07;

    if (Byte & 0x18){
        Status = VL53L0_ERROR_RANGE_ERROR;
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_EnableInterruptMask(VL53L0_DEV Dev,
        uint32_t InterruptMask)
{
    VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
    LOG_FUNCTION_START("");

    /* not implemented for VL53L0 */

    LOG_FUNCTION_END(Status);
    return Status;
}

/* End Group PAL Interrupt Functions */

/* Group SPAD functions */

VL53L0_Error VL53L0_SetSpadAmbientDamperThreshold(VL53L0_DEV Dev,
        uint16_t SpadAmbientDamperThreshold)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    VL53L0_WrByte(Dev, 0xFF, 0x01);
    Status = VL53L0_WrWord(Dev, 0x40, SpadAmbientDamperThreshold);
    VL53L0_WrByte(Dev, 0xFF, 0x00);

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_GetSpadAmbientDamperThreshold(VL53L0_DEV Dev,
        uint16_t* pSpadAmbientDamperThreshold)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    LOG_FUNCTION_START("");

    VL53L0_WrByte(Dev, 0xFF, 0x01);
    Status = VL53L0_RdWord(Dev, 0x40, pSpadAmbientDamperThreshold);
    VL53L0_WrByte(Dev, 0xFF, 0x00);

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_SetSpadAmbientDamperFactor(VL53L0_DEV Dev,
        uint16_t SpadAmbientDamperFactor)
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

VL53L0_Error VL53L0_GetSpadAmbientDamperFactor(VL53L0_DEV Dev,
        uint16_t* pSpadAmbientDamperFactor)
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

/*****************************************************************************
 * Internal functions
 *****************************************************************************/

uint32_t VL53L0_calc_macro_period_ps(VL53L0_DEV Dev, uint8_t vcsel_period);
uint16_t VL53L0_encode_timeout(uint32_t timeout_mclks);
uint32_t VL53L0_decode_timeout(uint16_t encoded_timeout);

VL53L0_Error VL53L0_get_vcsel_pulse_period(VL53L0_DEV Dev,
		uint8_t* pVCSELPulsePeriod, uint8_t RangeIndex)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t vcsel_period_reg;

    LOG_FUNCTION_START("");

    switch (RangeIndex)
    {
        case 0:
            Status = VL53L0_RdByte(Dev,
            		VL53L0_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD,
            		&vcsel_period_reg);
            break;
        case 1:
            Status = VL53L0_RdByte(Dev,
            		VL53L0_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD,
            		&vcsel_period_reg);
            break;
        case 2:
            Status = VL53L0_RdByte(Dev,
            		VL53L0_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD,
            		&vcsel_period_reg);
            break;
        default:
            Status = VL53L0_RdByte(Dev,
            		VL53L0_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD,
            		&vcsel_period_reg);
    }

    if (Status == VL53L0_ERROR_NONE) {
        *pVCSELPulsePeriod = VL53L0_decode_vcsel_period(vcsel_period_reg);
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

/* To convert ms into register value */
uint16_t VL53L0_calc_encoded_timeout(VL53L0_DEV Dev,
          uint32_t timeout_period_us,
          uint8_t vcsel_period)
{
    uint32_t macro_period_ps;
    uint32_t macro_period_ns;
    uint32_t timeout_period_mclks = 0;
    uint16_t timeout_overall_periods = 0;

    macro_period_ps = VL53L0_calc_macro_period_ps(Dev, vcsel_period);
    macro_period_ns = macro_period_ps / 1000;

    timeout_period_mclks =
        (uint32_t) (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
    timeout_overall_periods = VL53L0_encode_timeout(timeout_period_mclks);

    return timeout_overall_periods;
}

/* To convert register value into us */
uint32_t VL53L0_calc_ranging_wait_us(VL53L0_DEV Dev,
          uint16_t timeout_overall_periods,
          uint8_t vcsel_period)
{
    uint32_t macro_period_ps;
    uint32_t macro_period_ns;
    uint32_t timeout_period_mclks = 0;
    uint32_t actual_timeout_period_us = 0;

    macro_period_ps = VL53L0_calc_macro_period_ps(Dev, vcsel_period);
    macro_period_ns = macro_period_ps / 1000;

    timeout_period_mclks = VL53L0_decode_timeout(timeout_overall_periods);
    actual_timeout_period_us =
        ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) / 1000;

    return actual_timeout_period_us;
}

uint32_t VL53L0_calc_macro_period_ps(VL53L0_DEV Dev, uint8_t vcsel_period) {
    uint32_t PLL_multiplier;
    uint64_t PLL_period_ps;
    uint8_t vcsel_period_pclks;
    uint32_t macro_period_vclks;
    uint32_t macro_period_ps;

    LOG_FUNCTION_START("");

    PLL_multiplier = 65536 / 64; /* PLL multiplier is 64 */

    PLL_period_ps =
        (1000 * 1000 * PLL_multiplier) / VL53L0_GETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz);

    vcsel_period_pclks = VL53L0_decode_vcsel_period(vcsel_period);

    macro_period_vclks = 2304;
    macro_period_ps = (uint32_t)(macro_period_vclks * vcsel_period_pclks * PLL_period_ps);

    LOG_FUNCTION_END("");
    return macro_period_ps;
}

uint8_t VL53L0_decode_vcsel_period(uint8_t vcsel_period_reg) {

    /*!
     * Converts the encoded VCSEL period register value into the real period in PLL clocks
     */

    uint8_t vcsel_period_pclks = 0;

    vcsel_period_pclks = (vcsel_period_reg + 1) << 1;

    return vcsel_period_pclks;
}

uint8_t VL53L0_encode_vcsel_period(uint8_t vcsel_period_pclks) {

    /*!
     * Converts the encoded VCSEL period register value into the real period in PLL clocks
     */

    uint8_t vcsel_period_reg = 0;

    vcsel_period_reg = (vcsel_period_pclks >> 1) - 1;

    return vcsel_period_reg;
}

uint16_t VL53L0_encode_timeout(uint32_t timeout_mclks) {
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

        encoded_timeout = (ms_byte << 8) + (uint16_t) (ls_byte & 0x000000FF);

    }

    return encoded_timeout;

}

uint32_t VL53L0_decode_timeout(uint16_t encoded_timeout) {
    /*!
     * Decode 16-bit timeout register value - format (LSByte * 2^MSByte) + 1
     *
     */

    uint32_t timeout_mclks = 0;

    timeout_mclks = ((uint32_t) (encoded_timeout & 0x00FF) << (uint32_t) ((encoded_timeout & 0xFF00) >> 8)) + 1;

    return timeout_mclks;

}


VL53L0_Error VL53L0_check_part_used(VL53L0_DEV Dev,
		uint8_t* Revision,
        VL53L0_DeviceInfo_t* pVL53L0_DeviceInfo)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t ModuleIdInt;
    char *ProductId_tmp;

    LOG_FUNCTION_START("");

    Status = VL53L0_get_info_from_device(Dev);

    if (Status == VL53L0_ERROR_NONE) {
		ModuleIdInt = VL53L0_GETDEVICESPECIFICPARAMETER(Dev, ModuleId);

        if (ModuleIdInt == 0) {
            *Revision = 0;
            VL53L0_COPYSTRING(pVL53L0_DeviceInfo->ProductId, "");
        } else {
            *Revision = VL53L0_GETDEVICESPECIFICPARAMETER(Dev, Revision);
        	ProductId_tmp = VL53L0_GETDEVICESPECIFICPARAMETER(Dev, ProductId);
        	VL53L0_COPYSTRING(pVL53L0_DeviceInfo->ProductId, ProductId_tmp);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_get_info_from_device(VL53L0_DEV Dev)
{

	VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t byte;
    uint32_t TmpDWord;
    VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
    uint8_t ModuleId;
    uint8_t Revision;
    uint8_t ReferenceSpadCount;
    uint8_t ReferenceSpadType;
    char ProductId[19];
    char *ProductId_tmp;
    uint8_t ReadDataFromDeviceDone;

    LOG_FUNCTION_START("");

    ReadDataFromDeviceDone = VL53L0_GETDEVICESPECIFICPARAMETER(Dev,
    		ReadDataFromDeviceDone);

    /* This access is done only once after that a GetDeviceInfo or
     * datainit is done*/
    if (ReadDataFromDeviceDone == 0) {

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

        Status |= VL53L0_WrByte(Dev, 0x94, 0x6b);
        Status |= VL53L0_device_read_strobe(Dev);
        Status |= VL53L0_RdDWord(Dev, 0x90, &TmpDWord);

        ReferenceSpadCount = (uint8_t)((TmpDWord >> 8) & 0x07f);
        ReferenceSpadType  = (uint8_t)((TmpDWord >> 15) & 0x01);

        Status |= VL53L0_WrByte(Dev, 0x94, 0x02);
        Status |= VL53L0_device_read_strobe(Dev);
        Status |= VL53L0_RdByte(Dev, 0x90, &ModuleId);

        Status |= VL53L0_WrByte(Dev, 0x94, 0x7B);
        Status |= VL53L0_device_read_strobe(Dev);
        Status |= VL53L0_RdByte(Dev, 0x90, &Revision);

        Status |= VL53L0_WrByte(Dev, 0x94, 0x77);
        Status |= VL53L0_device_read_strobe(Dev);
        Status |= VL53L0_RdDWord(Dev, 0x90, &TmpDWord);

        ProductId[0] = (char)((TmpDWord >> 25) & 0x07f);
        ProductId[1] = (char)((TmpDWord >> 18) & 0x07f);
        ProductId[2] = (char)((TmpDWord >> 11) & 0x07f);
        ProductId[3] = (char)((TmpDWord >> 4) & 0x07f);

        byte = (uint8_t)((TmpDWord & 0x00f) << 3);

        Status |= VL53L0_WrByte(Dev, 0x94, 0x78);
        Status |= VL53L0_device_read_strobe(Dev);
        Status |= VL53L0_RdDWord(Dev, 0x90, &TmpDWord);

        ProductId[4] = (char)(byte +
        		((TmpDWord >> 29) & 0x07f));
        ProductId[5] = (char)((TmpDWord >> 22) & 0x07f);
        ProductId[6] = (char)((TmpDWord >> 15) & 0x07f);
        ProductId[7] = (char)((TmpDWord >> 8) & 0x07f);
        ProductId[8] = (char)((TmpDWord >> 1) & 0x07f);

        byte = (uint8_t)((TmpDWord & 0x001) << 6);

        Status |= VL53L0_WrByte(Dev, 0x94, 0x79);

        Status |= VL53L0_device_read_strobe(Dev);

        Status |= VL53L0_RdDWord(Dev, 0x90, &TmpDWord);

        ProductId[9] = (char)(byte +
        		((TmpDWord >> 26) & 0x07f));
        ProductId[10] = (char)((TmpDWord >> 19) & 0x07f);
        ProductId[11] = (char)((TmpDWord >> 12) & 0x07f);
        ProductId[12] = (char)((TmpDWord >> 5) & 0x07f);

        byte = (uint8_t)((TmpDWord & 0x01f) << 2);

        Status |= VL53L0_WrByte(Dev, 0x94, 0x80);

        Status |= VL53L0_device_read_strobe(Dev);

        Status |= VL53L0_RdDWord(Dev, 0x90, &TmpDWord);

        ProductId[13] = (char)(byte +
        		((TmpDWord >> 30) & 0x07f));
        ProductId[14] = (char)((TmpDWord >> 23) & 0x07f);
        ProductId[15] = (char)((TmpDWord >> 16) & 0x07f);
        ProductId[16] = (char)((TmpDWord >> 9) & 0x07f);
        ProductId[17] = (char)((TmpDWord >> 2) & 0x07f);
        ProductId[18] = '\0';

        Status |= VL53L0_WrByte(Dev, 0x81, 0x00);
        Status |= VL53L0_WrByte(Dev, 0xFF, 0x06);
        Status |= VL53L0_RdByte(Dev, 0x83, &byte);
        Status |= VL53L0_WrByte(Dev, 0x83, byte&0xfb);
        Status |= VL53L0_WrByte(Dev, 0xFF, 0x01);
        Status |= VL53L0_WrByte(Dev, 0x00, 0x01);

        Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);
        Status |= VL53L0_WrByte(Dev, 0x80, 0x00);

        if (Status == VL53L0_ERROR_NONE) {
        	VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
        			ModuleId, ModuleId);

        	VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
        			Revision, Revision);

        	VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
        			ReferenceSpadCount, ReferenceSpadCount);

        	VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
        			ReferenceSpadType, ReferenceSpadType);

        	ProductId_tmp = VL53L0_GETDEVICESPECIFICPARAMETER(Dev,
        			ProductId);
        	VL53L0_COPYSTRING(ProductId_tmp, ProductId);

        	VL53L0_SETDEVICESPECIFICPARAMETER(Dev,
        	    		ReadDataFromDeviceDone, 1);
        }
    }





    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_device_read_strobe(VL53L0_DEV Dev) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t strobe;
    uint32_t LoopNb;
    LOG_FUNCTION_START("");

    Status |= VL53L0_WrByte(Dev, 0x83, 0x00);

    /* polling
     * use timeout to avoid deadlock*/
    if (Status == VL53L0_ERROR_NONE) {
        LoopNb = 0;
        do {
            Status =VL53L0_RdByte(Dev, 0x83, &strobe);
            if ((strobe != 0x00) || Status != VL53L0_ERROR_NONE) {
                break;
            }
            LoopNb = LoopNb + 1;
        } while (LoopNb < VL53L0_DEFAULT_MAX_LOOP);

        if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP) {
            Status = VL53L0_ERROR_TIME_OUT;
        }
    }

    Status |= VL53L0_WrByte(Dev, 0x83, 0x01);

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
    uint32_t  bit = 1 << 30; /* The second-to-top bit is set:
                              * 1 << 14 for 16-bits, 1 << 30 for 32 bits */

     /* "bit" starts at the highest power of four <= the argument. */
    while (bit > num)
    {
        bit >>= 2;
    }

    while (bit != 0)
    {
        if (num >= res + bit)
        {
            num -= res + bit;
            res = (res >> 1) + bit;
        }
        else
        {
            res >>= 1;
        }
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

    if( a > 65535 || b > 65535)
    {
        res = 65535;
    }
    else
    {
        res = VL53L0_isqrt(a*a + b*b);
    }

    return res;
}



VL53L0_Error VL53L0_get_jmp_vcsel_ambient_rate(VL53L0_DEV Dev,
                                               uint32_t *pAmbient_rate_kcps,
                                               uint32_t *pVcsel_rate_kcps,
                                               uint32_t *pSignalTotalEventsRtn){
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
    Status |= VL53L0_RdDWord(Dev, 0xc0, &result_core__signal_total_events_rtn);
    Status |= VL53L0_RdDWord(Dev, 0xC8, &result_core__total_periods_elapsed_rtn);
    Status |= VL53L0_RdDWord(Dev, 0xbc, &result_core__ambient_window_events_rtn);
    Status |= VL53L0_RdDWord(Dev, 0xF0, &pll_period_us);
    Status |= VL53L0_WrByte(Dev, 0xFF, 0x00);


    if (Status == VL53L0_ERROR_NONE) {
        result_core__total_periods_elapsed_rtn =
            (int32_t)(result_core__total_periods_elapsed_rtn & 0x00ffffff);
        pll_period_us = (int32_t)(pll_period_us & 0x3ffff);
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdWord(Dev,
                               VL53L0_REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
                               &encodedTimeOut);
        if (Status == VL53L0_ERROR_NONE) {
            rngb1_config__timeout__macrop = VL53L0_decode_timeout(encodedTimeOut) - 1;
        }
    }

    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdByte(Dev, VL53L0_REG_PRE_RANGE_CONFIG_VCSEL_PERIOD,
        		&rnga_config__vcsel_period);
    }
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdByte(Dev, VL53L0_REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD,
        		&rngb1_config__vcsel_period);
        rngb2_config__vcsel_period = rngb1_config__vcsel_period;
    }
    if (Status == VL53L0_ERROR_NONE) {
        Status = VL53L0_RdByte(Dev, 0x32, &global_config__vcsel_width);
    }

    if (Status == VL53L0_ERROR_NONE) {
        if (Status == VL53L0_ERROR_NONE) {
            rngb2_config__timeout__macrop =
            		VL53L0_decode_timeout(encodedTimeOut) - 1;
        }
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        total_periods_elapsed_rtn__macrop =
        		result_core__total_periods_elapsed_rtn + 1;

        if (result_core__total_periods_elapsed_rtn ==
        		rngb1_config__timeout__macrop)
        {
           last_woi_period = rngb1_config__vcsel_period;
        }
        else if (result_core__total_periods_elapsed_rtn ==
        		rngb2_config__timeout__macrop)
        {
           last_woi_period = rngb2_config__vcsel_period;
        }
        else
        {
           last_woi_period = rnga_config__vcsel_period;

        }
        /* 512 = 1<<9  ==> 24-9=15 */
        ambient_duration_us = last_woi_period *
        		total_periods_elapsed_rtn__macrop * pll_period_us;
        ambient_duration_us = ambient_duration_us / 1000;

        if (ambient_duration_us != 0) {
            *pAmbient_rate_kcps = ((1 << 15) *
            		result_core__ambient_window_events_rtn) /
            				ambient_duration_us;
        } else {
            Status = VL53L0_ERROR_DIVISION_BY_ZERO;
        }

        if (Status == VL53L0_ERROR_NONE)
        {

            /* 2048 = 1<<11  ==> 24-11=13 */
            vcsel_duration_us = (10*global_config__vcsel_width + 4) *
            		total_periods_elapsed_rtn__macrop * pll_period_us ;
            vcsel_duration_us = vcsel_duration_us / 10000 ;


            if (vcsel_duration_us != 0) {
                *pVcsel_rate_kcps = ((1 << 13) *
                		result_core__signal_total_events_rtn) /
                				vcsel_duration_us;
                *pSignalTotalEventsRtn = result_core__signal_total_events_rtn;
            } else {
                Status = VL53L0_ERROR_DIVISION_BY_ZERO;
            }

        }
    }

    LOG_FUNCTION_END(Status);
    return Status;

}

VL53L0_Error VL53L0_calc_sigma_estimate(VL53L0_DEV Dev,
				VL53L0_RangingMeasurementData_t *pRangingMeasurementData,
				FixPoint1616_t* pSigmaEstimate)
{
	/* Expressed in 100ths of a ns, i.e. centi-ns */
    const uint32_t cPulseEffectiveWidth_centi_ns   = 800;
    /* Expressed in 100ths of a ns, i.e. centi-ns */
    const uint32_t cAmbientEffectiveWidth_centi_ns = 600;
    const FixPoint1616_t cSigmaEstRef              = 0x00000042;
    const uint32_t cVcselPulseWidth_ps             = 4700; /* pico secs */
    const FixPoint1616_t cSigmaEstMax              = 0x028F87AE;
    /* Time Of Flight per mm (6.6 pico secs) */
    const FixPoint1616_t cTOF_per_mm_ps            = 0x0006999A;
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
     * We work in kcps rather than mcps as this helps keep within the confines
     * of the 32 Fix1616 type.
     */

    xTalkCompRate_kcps = xTalkCompRate_mcps * 1000;
    if(xTalkCompRate_kcps > cMaxXTalk_kcps)
    {
        xTalkCompRate_kcps = cMaxXTalk_kcps;
    }

    Status =  VL53L0_get_jmp_vcsel_ambient_rate(Dev,
                                                &ambientRate_kcps,
                                                &vcselRate_kcps,
                                                &signalTotalEventsRtnRawVal);
    if (Status == VL53L0_ERROR_NONE) {
        if (vcselRate_kcps == 0) {
            *pSigmaEstimate = 0;
            PALDevDataSet(Dev, SigmaEstimate, 0);
        } else {
            signalTotalEventsRtn = signalTotalEventsRtnRawVal;
            if(signalTotalEventsRtn < 1)
            {
                signalTotalEventsRtn = 1;
            }

            /*
             * Calculate individual components of the main equation - replicating
             * the equation implemented in the script OpenAll_Ewok_ranging_data.jsl.
             *
             * sigmaEstimateP1 represents the effective pulse width, which is a tuning
             * parameter, rather than a real value.
             *
             * sigmaEstimateP2 represents the ambient/signal rate ratio expressed as
             * a multiple of the effective ambient width (tuning parameter).
             *
             * sigmaEstimateP3 provides the signal event component, with the knowledge
             * that
             *  - Noise of a square pulse is 1/sqrt(12) of the pulse width.
             *  - at 0Lux, sigma is proportional to
             *    effectiveVcselPulseWidth/sqrt(12 * signalTotalEvents)
             *
             * deltaT_ps represents the time of flight in pico secs for the
             * current range measurement, using the "TOF per mm" constant (in ps).
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
             * At this point the value will be 1.xx, therefore if we square the value this will exceed
             * 32 bits. To address this perform a single shift to the right before the multiplication.
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
             * Note that the Speed Of Light is expressed in um per 1E-10 seconds (2997)
             * Therefore to get mm/ns we have to divide by 10000
             */
            sigmaEstRtn      = ((sqrtResult_centi_ns+50)/100 * VL53L0_SPEED_OF_LIGHT_IN_AIR);
            sigmaEstRtn      /= (sigmaEstimateP3);
            sigmaEstRtn      += 5000; /* Add 5000 before dividing by 10000 to ensure rounding. */
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

            if((vcselRate_kcps < 1) || (signalTotalEventsRtn < 1) || (sigmaEstimate > cSigmaEstMax))
            {
                sigmaEstimate = cSigmaEstMax;
            }
        
            *pSigmaEstimate = (uint32_t)(sigmaEstimate);
            PALDevDataSet(Dev, SigmaEstimate, *pSigmaEstimate);
        }
    }

    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_get_pal_range_status(VL53L0_DEV Dev,
                                         uint8_t DeviceRangeStatus,
                                         FixPoint1616_t SignalRate,
                                         FixPoint1616_t CrosstalkCompensation,
                                         uint16_t EffectiveSpadRtnCount,
                                         VL53L0_RangingMeasurementData_t *pRangingMeasurementData,
                                         uint8_t* pPalRangeStatus) {
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t tmpByte;
    uint8_t SigmaLimitCheckEnable;
    uint8_t SignalRefClipLimitCheckEnable;
    FixPoint1616_t SigmaEstimate;
    FixPoint1616_t SigmaLimitValue;
    FixPoint1616_t SignalRefClipValue;
    uint8_t DeviceRangeStatusInternal = 0;
    uint16_t tmpWord;
    FixPoint1616_t LastSignalRefMcps;

    LOG_FUNCTION_START("");

    /*
     * VL53L0 has a good ranging when the value of the DeviceRangeStatus = 11.
     * This function will replace the value 0 with the value 11 in the
     * DeviceRangeStatus.
     * In addition, the SigmaEstimator is not included in the VL53L0
     * DeviceRangeStatus, this will be added in the PalRangeStatus.
     */

    DeviceRangeStatusInternal = ((DeviceRangeStatus & 0x78) >> 3);

    if (DeviceRangeStatusInternal == 11) {
        tmpByte = 0;
    } else if (DeviceRangeStatusInternal == 0) {
        tmpByte = 11;
    } else {
        tmpByte = DeviceRangeStatusInternal;
    }

    /* LastSignalRefMcps */
    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev, 0xFF, 0x01);

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_RdWord(Dev, VL53L0_REG_RESULT_PEAK_SIGNAL_RATE_REF,
        		&tmpWord);

    LastSignalRefMcps = VL53L0_FIXPOINT97TOFIXPOINT1616(tmpWord);

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev, 0xFF, 0x00);

    PALDevDataSet(Dev, LastSignalRefMcps, LastSignalRefMcps);

    /*
     * Check if Sigma limit is enabled, if yes then do comparison with limit
     * value and put the result back into pPalRangeStatus.
     */
    if (Status == VL53L0_ERROR_NONE)
    	Status =  VL53L0_GetLimitCheckEnable(Dev,
    		VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, &SigmaLimitCheckEnable);

    if ((SigmaLimitCheckEnable != 0) && (Status == VL53L0_ERROR_NONE)) {
        /*
         * compute the Sigma and check with limit
         */
        Status = VL53L0_calc_sigma_estimate(Dev, pRangingMeasurementData,
        		&SigmaEstimate);

        if (Status == VL53L0_ERROR_NONE) {
            Status = VL53L0_GetLimitCheckValue(Dev,
            		VL53L0_CHECKENABLE_SIGMA_FINAL_RANGE, &SigmaLimitValue);

            if ((SigmaLimitValue > 0) && (SigmaEstimate > SigmaLimitValue)) {
                /* Limit Fail add 2^4 to range status */
                tmpByte += 16;
            }
        }
    }

    /*
     * Check if Signal ref clip limit is enabled, if yes then do comparison
     * with limit value and put the result back into pPalRangeStatus.
     */
    if (Status == VL53L0_ERROR_NONE)
    	Status =  VL53L0_GetLimitCheckEnable(Dev,
    			VL53L0_CHECKENABLE_SIGNAL_REF_CLIP,
    			&SignalRefClipLimitCheckEnable);

    if ((SignalRefClipLimitCheckEnable != 0) && (Status == VL53L0_ERROR_NONE)) {

		Status = VL53L0_GetLimitCheckValue(Dev,
				VL53L0_CHECKENABLE_SIGNAL_REF_CLIP, &SignalRefClipValue);

		if ((SignalRefClipValue > 0) &&
				(LastSignalRefMcps > SignalRefClipValue)) {
			/* Limit Fail add 2^5 to range status */
			tmpByte += 32;
		}
    }

    if (Status == VL53L0_ERROR_NONE) {
        *pPalRangeStatus = tmpByte;
    }


    LOG_FUNCTION_END(Status);
    return Status;

}

VL53L0_Error VL53L0_reverse_bytes(uint8_t *data, uint32_t size)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    uint8_t tempData;
    uint32_t mirrorIndex;
    uint32_t middle = size/2;
    uint32_t index;

    for(index = 0; index < middle; index++)
    {
        mirrorIndex      = size - index - 1;
        tempData         = data[index];
        data[index]      = data[mirrorIndex];
        data[mirrorIndex] = tempData;
    }
    return Status;
}


VL53L0_Error VL53L0_load_tuning_settings(VL53L0_DEV Dev,
		uint8_t* pTuningSettingBuffer)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    int i;
    int Index;
    uint8_t msb;
    uint8_t lsb;
    uint8_t SelectParam;
    uint8_t NumberOfWrites;
    uint8_t Address;
    uint8_t localBuffer[4]; /* max */
    uint16_t Temp16;

    LOG_FUNCTION_START("");

    Index = 0;

    while ((*(pTuningSettingBuffer+Index) != 0) &&
    		(Status == VL53L0_ERROR_NONE)) {
        NumberOfWrites = *(pTuningSettingBuffer+Index);
        Index++;
        if (NumberOfWrites == 0xFF) {
            /* internal parameters */
            SelectParam = *(pTuningSettingBuffer+Index);
            Index++;
            switch (SelectParam) {
                case 0: /* uint16_t SigmaEstRefArray -> 2 bytes */
                    msb = *(pTuningSettingBuffer+Index);
                    Index++;
                    lsb = *(pTuningSettingBuffer+Index);
                    Index++;
                    Temp16 = VL53L0_MAKEUINT16(lsb, msb);
                    PALDevDataSet(Dev, SigmaEstRefArray, Temp16);
                    break;
                case 1: /* uint16_t SigmaEstEffPulseWidth -> 2 bytes */
                    msb = *(pTuningSettingBuffer+Index);
                    Index++;
                    lsb = *(pTuningSettingBuffer+Index);
                    Index++;
                    Temp16 = VL53L0_MAKEUINT16(lsb, msb);
                    PALDevDataSet(Dev, SigmaEstEffPulseWidth, Temp16);
                    break;
                case 2: /* uint16_t SigmaEstEffAmbWidth -> 2 bytes */
                    msb = *(pTuningSettingBuffer+Index);
                    Index++;
                    lsb = *(pTuningSettingBuffer+Index);
                    Index++;
                    Temp16 = VL53L0_MAKEUINT16(lsb, msb);
                    PALDevDataSet(Dev, SigmaEstEffAmbWidth, Temp16);
                    break;
                case 3: /* uint16_t targetRefRate -> 2 bytes */
                    msb = *(pTuningSettingBuffer+Index);
                    Index++;
                    lsb = *(pTuningSettingBuffer+Index);
                    Index++;
                    Temp16 = VL53L0_MAKEUINT16(lsb, msb);
                    PALDevDataSet(Dev, targetRefRate, Temp16);
                    break;
                default: /* invalid parameter */
                    Status = VL53L0_ERROR_INVALID_PARAMS;
            }

        } else if (NumberOfWrites <= 4) {
            Address = *(pTuningSettingBuffer+Index);
            Index++;

            for (i=0; i<NumberOfWrites; i++) {
                localBuffer[i] = *(pTuningSettingBuffer+Index);
                Index++;
            }

            Status = VL53L0_WriteMulti(Dev, Address, localBuffer,
            		NumberOfWrites);

        } else {
            Status = VL53L0_ERROR_INVALID_PARAMS;
        }
    }


    LOG_FUNCTION_END(Status);
    return Status;
}

VL53L0_Error VL53L0_apply_ref_spads(VL53L0_DEV Dev, uint8_t apertureSpads, uint32_t count)
{
    VL53L0_Error status = VL53L0_ERROR_NONE;
    uint32_t currentSpadIndex = 0;
    uint8_t startSelect = 0xB4;
    uint32_t spadArraySize = 6;
    uint32_t maxSpadCount = 44;
    uint32_t lastSpadIndex;
    uint32_t index;

    /*
     * This function applies a requested number of reference spads, either aperture or
     * non-aperture, as requested.
     * The good spad map will be applied.
     */
    
    status = VL53L0_WrByte(Dev, 0xFF, 0x01);
    
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev,
        		VL53L0_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET,    0x00);

    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev,
        		VL53L0_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);

    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev, 0xFF, 0x00);
    
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev,
        		VL53L0_REG_GLOBAL_CONFIG_REF_EN_START_SELECT, startSelect);

    for(index = 0; index < spadArraySize; index++)
    {
        Dev->Data.SpadData.RefSpadEnables[index] = 0;
        Dev->Data.SpadData.RefGoodSpadMap[index] = 0xFF;
    }

    if(apertureSpads)
    {
        /* Increment to the first APERTURE spad */
        while((is_aperture(startSelect + currentSpadIndex) == 0) &&
              (currentSpadIndex < maxSpadCount))
        {
            currentSpadIndex++;
        }
    }
    status = enable_ref_spads(Dev,
                              apertureSpads,
                              Dev->Data.SpadData.RefGoodSpadMap,
                              Dev->Data.SpadData.RefSpadEnables,
                              spadArraySize,
                              startSelect,
                              currentSpadIndex,
                              count,
                              &lastSpadIndex);
    return status;
}

VL53L0_Error VL53L0_PerformRefSpadManagement(VL53L0_DEV Dev)
{
    VL53L0_Error Status = VL53L0_ERROR_NONE;
    VL53L0_RangingMeasurementData_t rangingMeasurementData;
    uint8_t lastSpadArray[6];
    uint8_t startSelect = 0xB4;
    uint32_t minimumSpadCount = 5;
    uint32_t maxSpadCount = 44;
    uint32_t currentSpadIndex = 0;
    uint32_t lastSpadIndex;
    int32_t nextGoodSpad;
    uint16_t targetRefRate = 0x0A00; /* 20 MCPS in 9:7 format */
    uint16_t peakSignalRateRef;
    uint32_t needAptSpads = -1;
    uint32_t index;
    uint32_t spadArraySize = 6;
    uint32_t signalRateDiff;
    uint32_t lastSignalRateDiff;
    uint8_t complete;
    uint8_t SequenceConfig = 0;


    /*
     * The reference SPAD initialization procedure determines the minimum
     * amount of reference spads to be enables to achieve a target reference
     * signal rate and should be performed once during initialization.
     *
     * Either aperture or non-aperture spads are applied but never both.
     * Firstly non-aperture spads are set, begining with 5 spads, and increased
     * one spad at a time until the closest measurement to the target rate is
     * achieved.
     *
     * If the target rate is exceeded when 5 non-aperture spads are enabled,
     * initialization is performed instead with aperture spads.
     *
     * When setting spads, a 'Good Spad Map' is applied.
     *
     * This procedure operates within a SPAD window of interest of a maximum 44
     * spads.
     * The start point is currently fixed to 180, which lies towards the end of
     * the non-aperture quadrant and runs in to the adjacent aperture quadrant.
     */


    targetRefRate = PALDevDataGet(Dev, targetRefRate);

    /*
     * Initialize Spad arrays.
     * Currently the good spad map is initialised to 'All good'.
     * This is a short term implementation. The good spad map will be provided
     * as an input.
     * Note that there are 6 bytes. Only the furst 44 bits will be used to
     * represent spads.
     */
    for(index = 0; index < spadArraySize; index++)
    {
        Dev->Data.SpadData.RefSpadEnables[index] = 0;
        Dev->Data.SpadData.RefGoodSpadMap[index] = 0xFF;
    }

    Status = VL53L0_WrByte(Dev, 0xFF, 0x01);
    
    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_DYNAMIC_SPAD_REF_EN_START_OFFSET,    0x00);

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev, 0xFF, 0x00);
    
    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_GLOBAL_CONFIG_REF_EN_START_SELECT, startSelect);


    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_WrByte(Dev,
        		VL53L0_REG_POWER_MANAGEMENT_GO1_POWER_FORCE, 0);

    if (Status == VL53L0_ERROR_NONE)
    {
        /* store the value of the sequence config,
         * this will be reset before the end of the function
         */
        SequenceConfig = PALDevDataGet(Dev, SequenceConfig);

        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, 1);

        PALDevDataSet(Dev, SequenceConfig, 0x01);
    }

    if (Status == VL53L0_ERROR_NONE)
        Status = VL53L0_PerformSingleRangingMeasurement(Dev,
        		&rangingMeasurementData);

    if (Status == VL53L0_ERROR_NONE)
    {
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, 2);
        PALDevDataSet(Dev, SequenceConfig, 0x02);

    }
    if (Status == VL53L0_ERROR_NONE)
    {
        Status = VL53L0_PerformSingleRangingMeasurement(Dev,
        		&rangingMeasurementData);
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        /* Enable Minimum NON-APERTURE Spads */
        currentSpadIndex = 0;
        lastSpadIndex = currentSpadIndex;
        needAptSpads = 0;
        Status = enable_ref_spads(Dev,
                                  needAptSpads,
                                  Dev->Data.SpadData.RefGoodSpadMap,
                                  Dev->Data.SpadData.RefSpadEnables,
                                  spadArraySize,
                                  startSelect,
                                  currentSpadIndex,
                                  minimumSpadCount,
                                  &lastSpadIndex);
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        currentSpadIndex = lastSpadIndex;

        if (Status == VL53L0_ERROR_NONE)
        {
            Status = perform_ref_signal_measurement(Dev, &peakSignalRateRef);
            if (Status == VL53L0_ERROR_NONE)
            {
                if(peakSignalRateRef > targetRefRate)
                {
                    /* Signal rate measurement too high, switch to
                     * APERTURE SPADs */

                    for(index = 0; index < spadArraySize; index++)
                    {
                        Dev->Data.SpadData.RefSpadEnables[index] = 0;
                    }

                    /* Increment to the first APERTURE spad */
                    while((is_aperture(startSelect + currentSpadIndex) == 0) &&
                    		(currentSpadIndex < maxSpadCount))
                    {
                        currentSpadIndex++;
                    }

                    needAptSpads = 1;
                    Status = enable_ref_spads(Dev,
									  needAptSpads,
									  Dev->Data.SpadData.RefGoodSpadMap,
									  Dev->Data.SpadData.RefSpadEnables,
									  spadArraySize,
									  startSelect,
									  currentSpadIndex,
									  minimumSpadCount,
									  &lastSpadIndex);
                
                    if (Status == VL53L0_ERROR_NONE)
                    {
                        currentSpadIndex = lastSpadIndex;
                        Status = perform_ref_signal_measurement(Dev,
                        		&peakSignalRateRef);
                        
                        if ((Status == VL53L0_ERROR_NONE) &&
                            (peakSignalRateRef > targetRefRate))
                        {
                            /* Signal rate still too high after setting the
                             * minimum number of APERTURE spads.
                             * Can do no more.
                             */
                            Status = VL53L0_ERROR_REF_SPAD_INIT;
                            needAptSpads    = -1;
                        }
                    }
                }
                else
                {
                    needAptSpads = 0;
                }
            }
        }
    }

    if (Status == VL53L0_ERROR_NONE)
    {
        if(peakSignalRateRef < targetRefRate)
        {
            /* At this point, the minimum number of either aperture or
             * non-aperture spads have been set proceed to add spads and
             * perform measurements until the target reference is reached.
             *
             */

            memcpy(lastSpadArray, Dev->Data.SpadData.RefSpadEnables,
            		spadArraySize);
            lastSignalRateDiff = abs(peakSignalRateRef - targetRefRate);
            complete = 0;

            while(!complete)
            {
                get_next_good_spad(Dev->Data.SpadData.RefGoodSpadMap,
                		spadArraySize, currentSpadIndex, &nextGoodSpad);
                if(nextGoodSpad == -1)
                {
                    Status = VL53L0_ERROR_REF_SPAD_INIT;
                    break;
                }

                /* Cannot combine Aperture and Non-Aperture spads, so ensure
                 * the current spad is of the correct type.
                 */
                if(is_aperture((uint32_t)startSelect + nextGoodSpad) !=
                		needAptSpads)
                {
                    Status = VL53L0_ERROR_REF_SPAD_INIT;
                    break;
                }

                currentSpadIndex = nextGoodSpad;
                Status = enable_spad_bit(Dev->Data.SpadData.RefSpadEnables,
                		spadArraySize, currentSpadIndex);
                if (Status == VL53L0_ERROR_NONE) {
					currentSpadIndex++;
					/* Proceed to apply the additional spad and perform
					 * mesurement. */
					Status = set_ref_spad_map(Dev,
							Dev->Data.SpadData.RefSpadEnables);
                }

                if (Status == VL53L0_ERROR_NONE)
                {
                    Status = perform_ref_signal_measurement(Dev,
                    		&peakSignalRateRef);
                    if (Status == VL53L0_ERROR_NONE)
                    {
                        if(peakSignalRateRef > targetRefRate)
                        {
                            /* Select the spad map that provides the measurement
                             * closest to the target rate, either above or
                             * below it.
                             */
                            signalRateDiff = abs(peakSignalRateRef -
                            		targetRefRate);
                            if(signalRateDiff > lastSignalRateDiff)
                            {
                                /* Previous spad map produced a closer
                                 * measurement, so choose this. */
                                Status = set_ref_spad_map(Dev, lastSpadArray);
                                memcpy(Dev->Data.SpadData.RefSpadEnables,
                                		lastSpadArray, spadArraySize);
                            }
                            complete = 1;
                        }
                        else
                        {
                            /* Continue to add spads */
                            lastSignalRateDiff = signalRateDiff;
                            memcpy(lastSpadArray,
                            		Dev->Data.SpadData.RefSpadEnables,
                            		spadArraySize);
                        }
                    }
                    else
                        break;

                }
                else
                    break;
            }
        }
    }

    if(Status==VL53L0_ERROR_NONE){
        /* restore the previous Sequence Config */
        Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG,
        		SequenceConfig);
		if(Status==VL53L0_ERROR_NONE)
			PALDevDataSet(Dev, SequenceConfig, SequenceConfig);
    }

    return Status;
}

VL53L0_Error perform_ref_signal_measurement(VL53L0_DEV Dev,
		uint16_t *refSignalRate)
{
    VL53L0_Error status = VL53L0_ERROR_NONE;
    VL53L0_RangingMeasurementData_t rangingMeasurementData;

    /*
     * This function performs a reference signal rate measurement.
     */
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_SEQUENCE_CONFIG, 0xC0);
        
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_PerformSingleRangingMeasurement(Dev,
        		&rangingMeasurementData);
  
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev, 0xFF, 0x01);
  
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_RdWord(Dev, VL53L0_REG_RESULT_PEAK_SIGNAL_RATE_REF,
        		refSignalRate);
    
    if (status == VL53L0_ERROR_NONE)
        status = VL53L0_WrByte(Dev, 0xFF, 0x00);

    return status;
}
VL53L0_Error enable_ref_spads(VL53L0_DEV Dev,
                              uint8_t apertureSpads,
                              uint8_t goodSpadArray[],
                              uint8_t spadArray[],
                              uint32_t size,
                              uint32_t start,
                              uint32_t offset,
                              uint32_t spadCount,
                              uint32_t *lastSpad)
{
    VL53L0_Error status = VL53L0_ERROR_NONE;
    uint32_t index;
    uint32_t i;
    int32_t nextGoodSpad = offset;
    uint32_t currentSpad;
    uint8_t checkSpadArray[6];

    /*
     * This function takes in a spad array which may or may not have SPADS
     * already enabled and appends from a given offset a requested number
     * of new SPAD enables. The 'good spad map' is applies to
     * determine the next SPADs to enable.
     *
     * This function applies to only aperture or only non-aperture spads.
     * Checks are performed to ensure this.
     */

    currentSpad = offset;
    for(index = 0; index < spadCount; index++)
    {
        get_next_good_spad(goodSpadArray, size, currentSpad, &nextGoodSpad);
        if(nextGoodSpad == -1)
        {
            status = VL53L0_ERROR_REF_SPAD_INIT;
            break;
        }

        /* Confirm that the next good SPAD is non-aperture */
        if(is_aperture(start + nextGoodSpad) != apertureSpads)
        {
            /* if we can't get the required number of good aperture spads
             * from the current quadrant then this is an error.
             */
            status = VL53L0_ERROR_REF_SPAD_INIT;
            break;
        }
        currentSpad = (uint32_t)nextGoodSpad;
        enable_spad_bit(spadArray, size, currentSpad);
        currentSpad++;
    }
    *lastSpad = currentSpad;
    
    if (status == VL53L0_ERROR_NONE)
    {
        status = set_ref_spad_map(Dev, spadArray);
    }
    
    if (status == VL53L0_ERROR_NONE)
    {
        status = get_ref_spad_map(Dev, checkSpadArray);
    
        i = 0;
        
        /* Compare spad maps. If not equal report error. */
        while(i < size)
        {
            if (spadArray[i] != checkSpadArray[i])
            {
                status = VL53L0_ERROR_REF_SPAD_INIT;
                break;
            }
            i++;
        }
    }
    return status;
}

uint8_t is_aperture(uint32_t spadIndex)
{
    /*
     * This function reports if a given spad index is an aperture SPAD by
     * deriving the quadrant.
     */
    uint32_t quadrant;
    uint8_t isAperture = 1;
    quadrant = spadIndex >> 6;
    if(refArrayQuadrants[quadrant] == REF_ARRAY_SPAD_0)
    {
        isAperture = 0;
    }
    return isAperture;
}

void get_next_good_spad(uint8_t goodSpadArray[], uint32_t size, uint32_t current1, int32_t *next)
{
    uint32_t startIndex;
    uint32_t fineOffset;
    uint32_t cSpadsPerByte = 8;
    uint32_t coarseIndex;
    uint32_t fineIndex;
    uint8_t dataByte;
    uint8_t success = 0;

    /*
     * Starting with the current good spad, loop through the array to find the next.
     * i.e. the next bit set in the sequence.
     *
     * The coarse index is the byte index of the array and the fine index is the index
     * of the bit within each byte.
     */

    *next = -1;

    startIndex = current1 / cSpadsPerByte;
    fineOffset = current1 % cSpadsPerByte;

    for(coarseIndex = startIndex; ((coarseIndex < size) && !success); coarseIndex++)
    {
        fineIndex = 0;
        dataByte = goodSpadArray[coarseIndex];

        if(coarseIndex == startIndex)
        {
            /* locate the bit position of the provided current spad bit before iterating */
            dataByte >>= fineOffset;
            fineIndex = fineOffset;
        }

        while(fineIndex < cSpadsPerByte)
        {
            if((dataByte & 0x1) == 1)
            {
                success = 1;
                *next = coarseIndex * cSpadsPerByte + fineIndex;
                break;
            }
            dataByte >>= 1;
            fineIndex++;
        }
    }
}

VL53L0_Error enable_spad_bit(uint8_t spadArray[], uint32_t size, uint32_t spadIndex)
{
    VL53L0_Error status = VL53L0_ERROR_NONE;
    uint32_t cSpadsPerByte = 8;
    uint32_t coarseIndex;
    uint32_t fineIndex;

    coarseIndex = spadIndex / cSpadsPerByte;
    fineIndex = spadIndex % cSpadsPerByte;
    if(coarseIndex >= size)
    {
        status = VL53L0_ERROR_REF_SPAD_INIT;
    }
    spadArray[coarseIndex] |= (1 << fineIndex);
    return status;
}

VL53L0_Error set_ref_spad_map(VL53L0_DEV Dev, uint8_t *refSpadArray)
{
    VL53L0_Error status = VL53L0_WriteMulti(Dev,
                                            VL53L0_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0,
                                            refSpadArray, 6);
    return status;
}

VL53L0_Error get_ref_spad_map(VL53L0_DEV Dev, uint8_t *refSpadArray)
{
    VL53L0_Error status = VL53L0_ReadMulti(Dev,
                                           VL53L0_REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0,
                                           refSpadArray,
                                           6);
    return status;
}
