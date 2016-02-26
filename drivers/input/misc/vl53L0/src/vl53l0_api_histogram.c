/*******************************************************************************
 Copyright © 2016, STMicroelectronics International N.V.
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
#include "vl53l0_api_core.h"
#include "vl53l0_api_histogram.h"


#ifndef __KERNEL__
#include <stdlib.h>
#endif
#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)


VL53L0_Error VL53L0_start_histogram_measurement(VL53L0_DEV Dev,
			VL53L0_HistogramModes histoMode,
			uint32_t count)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t dataByte;
	LOG_FUNCTION_START("");


	dataByte = VL53L0_REG_SYSRANGE_MODE_SINGLESHOT |
		VL53L0_REG_SYSRANGE_MODE_START_STOP;

	/* First histogram measurement must have bit 5 set */
	if (count == 0)
		dataByte |= (1 << 5);

	switch (histoMode) {
	case VL53L0_HISTOGRAMMODE_DISABLED:
		/* Selected mode not supported */
		Status = VL53L0_ERROR_INVALID_COMMAND;
		break;

	case VL53L0_HISTOGRAMMODE_REFERENCE_ONLY:
	case VL53L0_HISTOGRAMMODE_RETURN_ONLY:
	case VL53L0_HISTOGRAMMODE_BOTH:
		dataByte |= (histoMode << 3);
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSRANGE_START,
			dataByte);
		if (Status == VL53L0_ERROR_NONE) {
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

VL53L0_Error VL53L0_confirm_measurement_start(VL53L0_DEV Dev)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	uint8_t NewDataReady = 0;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	LoopNb = 0;
	do {
		Status = VL53L0_GetMeasurementDataReady(Dev, &NewDataReady);
		if ((NewDataReady == 0x01) || Status != VL53L0_ERROR_NONE)
				break;

		LoopNb = LoopNb + 1;
		VL53L0_PollingDelay(Dev);
	} while (LoopNb < VL53L0_DEFAULT_MAX_LOOP);

	if (LoopNb >= VL53L0_DEFAULT_MAX_LOOP)
		Status = VL53L0_ERROR_TIME_OUT;

	LOG_FUNCTION_END(Status);

	return Status;
}


VL53L0_Error VL53L0_set_histogram_mode(VL53L0_DEV Dev,
		VL53L0_HistogramModes HistogramMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	switch (HistogramMode) {
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

	return Status;
}

VL53L0_Error VL53L0_get_histogram_mode(VL53L0_DEV Dev,
	VL53L0_HistogramModes *pHistogramMode)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;

	VL53L0_GETPARAMETERFIELD(Dev, HistogramMode, *pHistogramMode);

	return Status;
}


VL53L0_Error VL53L0_perform_single_histogram_measurement(VL53L0_DEV Dev,
		VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData)
{
	VL53L0_Error Status = VL53L0_ERROR_NONE;
	VL53L0_DeviceModes DeviceMode;
	VL53L0_HistogramModes HistogramMode = VL53L0_HISTOGRAMMODE_DISABLED;
	uint32_t MeasCount;
	uint32_t Measurements;

	/* Get Current DeviceMode */
	Status = VL53L0_GetHistogramMode(Dev, &HistogramMode);


	if (Status != VL53L0_ERROR_NONE)
		return Status;


	if (HistogramMode == VL53L0_HISTOGRAMMODE_BOTH) {
		if (pHistogramMeasurementData->BufferSize <
				VL53L0_HISTOGRAM_BUFFER_SIZE) {
			Status = VL53L0_ERROR_BUFFER_TOO_SMALL;
		}
	} else {
		if (pHistogramMeasurementData->BufferSize <
				VL53L0_HISTOGRAM_BUFFER_SIZE/2) {
			Status = VL53L0_ERROR_BUFFER_TOO_SMALL;
		}
	}
	pHistogramMeasurementData->HistogramType = (uint8_t)HistogramMode;
	pHistogramMeasurementData->ErrorStatus   = VL53L0_DEVICEERROR_NONE;
	pHistogramMeasurementData->FirstBin      = 0;
	pHistogramMeasurementData->NumberOfBins  = 0;


	/* Get Current DeviceMode */
	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_GetDeviceMode(Dev, &DeviceMode);

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_WrByte(Dev, VL53L0_REG_SYSTEM_HISTOGRAM_BIN,
			0x00);

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_WrByte(Dev,
			VL53L0_REG_HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT, 0x00);

	if (Status == VL53L0_ERROR_NONE)
		Status = VL53L0_WrByte(Dev,
				VL53L0_REG_HISTOGRAM_CONFIG_READOUT_CTRL, 0x01);

	if (Status != VL53L0_ERROR_NONE)
		return Status;

	Measurements = 3;
	if (HistogramMode == VL53L0_HISTOGRAMMODE_BOTH)
		Measurements = 6;

	if (DeviceMode != VL53L0_DEVICEMODE_SINGLE_HISTOGRAM) {
		Status = VL53L0_ERROR_INVALID_COMMAND;
		return Status;
	}

	/* DeviceMode == VL53L0_DEVICEMODE_SINGLE_HISTOGRAM */
	MeasCount = 0;
	while ((MeasCount < Measurements) && (Status == VL53L0_ERROR_NONE)) {
		Status = VL53L0_start_histogram_measurement(Dev, HistogramMode,
			MeasCount);

		if (Status == VL53L0_ERROR_NONE)
			VL53L0_confirm_measurement_start(Dev);

		if (Status == VL53L0_ERROR_NONE)
			PALDevDataSet(Dev, PalState, VL53L0_STATE_RUNNING);

		if (Status == VL53L0_ERROR_NONE)
			Status = VL53L0_measurement_poll_for_completion(Dev);

		if (Status == VL53L0_ERROR_NONE) {
			Status = VL53L0_read_histo_measurement(Dev,
			pHistogramMeasurementData->HistogramData,
			MeasCount,
			HistogramMode);

			if (Status == VL53L0_ERROR_NONE) {
				/*
				* When reading both rtn and ref arrays,
				* histograms are read two bins at a time.
				*  For rtn or ref only, histograms are read four
				*  bins at a time.
				*/
				if (HistogramMode == VL53L0_HISTOGRAMMODE_BOTH)
					pHistogramMeasurementData->NumberOfBins
						+= 2;
				else
					pHistogramMeasurementData->NumberOfBins
						+= 4;

			}
		}

		if (Status == VL53L0_ERROR_NONE)
			Status = VL53L0_ClearInterruptMask(Dev, 0);

		MeasCount++;
	}

	/* Change PAL State in case of single ranging or single histogram */
	if (Status == VL53L0_ERROR_NONE) {
		pHistogramMeasurementData->NumberOfBins = 12;
		PALDevDataSet(Dev, PalState, VL53L0_STATE_IDLE);
	}

	return Status;
}


VL53L0_Error VL53L0_get_histogram_measurement_data(VL53L0_DEV Dev,
		VL53L0_HistogramMeasurementData_t *pHistogramMeasurementData)
{
	VL53L0_Error Status = VL53L0_ERROR_NOT_IMPLEMENTED;
	LOG_FUNCTION_START("");

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0_Error VL53L0_read_histo_measurement(VL53L0_DEV Dev,
			uint32_t *histoData,
			uint32_t offset,
			VL53L0_HistogramModes histoMode)
{
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

	if (Status == VL53L0_ERROR_NONE) {
		VL53L0_reverse_bytes(&localBuffer[0], cDataSize);
		VL53L0_reverse_bytes(&localBuffer[4], cDataSize);
		VL53L0_reverse_bytes(&localBuffer[20], cDataSize);
		VL53L0_reverse_bytes(&localBuffer[24], cDataSize);

		offset1 = offset * cDataSize;
		if (histoMode == VL53L0_HISTOGRAMMODE_BOTH) {
			/*
			 * When reading both return and ref data, each
			 * measurement reads two ref values and two return
			 * values. Data is stored in an interleaved sequence,
			 * starting with the return histogram.
			 *
			 * Some result Core registers are reused for the
			 * histogram measurements
			 *
			 * The bin values are retrieved in the following order
			 *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_RTN
			 *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_REF
			 *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN
			 *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF
			 */

			memcpy(&histoData[offset1],     &localBuffer[4],
				cDataSize); /* rtn */
			memcpy(&histoData[offset1 + 1], &localBuffer[24],
				cDataSize); /* ref */
			memcpy(&histoData[offset1 + 2], &localBuffer[0],
				cDataSize); /* rtn */
			memcpy(&histoData[offset1 + 3], &localBuffer[20],
				cDataSize); /* ref */

		} else {
			/*
			 * When reading either return and ref data, each
			 * measurement reads four bin values.
			 *
			 * The bin values are retrieved in the following order
			 *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_RTN
			 *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN
			 *  VL53L0_REG_RESULT_CORE_RANGING_TOTAL_EVENTS_REF
			 *  VL53L0_REG_RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF
			 */

			memcpy(&histoData[offset1],     &localBuffer[24],
				cDataSize);
			memcpy(&histoData[offset1 + 1], &localBuffer[20],
				cDataSize);
			memcpy(&histoData[offset1 + 2], &localBuffer[4],
				cDataSize);
			memcpy(&histoData[offset1 + 3], &localBuffer[0],
				cDataSize);

		}
	}

	LOG_FUNCTION_END(Status);

	return Status;
}

