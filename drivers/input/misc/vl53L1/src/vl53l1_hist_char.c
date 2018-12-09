
/*******************************************************************************
 * Copyright (c) 2017, STMicroelectronics - All Rights Reserved

 This file is part of VL53L1 Core and is dual licensed,
 either 'STMicroelectronics
 Proprietary license'
 or 'BSD 3-clause "New" or "Revised" License' , at your option.

********************************************************************************

 'STMicroelectronics Proprietary license'

********************************************************************************

 License terms: STMicroelectronics Proprietary in accordance with licensing
 terms at www.st.com/sla0081

 STMicroelectronics confidential
 Reproduction and Communication of this document is strictly prohibited unless
 specifically authorized in writing by STMicroelectronics.


********************************************************************************

 Alternatively, VL53L1 Core may be distributed under the terms of
 'BSD 3-clause "New" or "Revised" License', in which case the following
 provisions apply instead of the ones
 mentioned above :

********************************************************************************

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


********************************************************************************

*/





































#include <stdio.h>

#include <stdlib.h>







#include "vl53l1_core.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_hist_char.h"

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_HISTOGRAM, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_HISTOGRAM, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_HISTOGRAM,\
		status, fmt, ##__VA_ARGS__)


VL53L1_Error VL53L1_set_calib_config(
	VL53L1_DEV      Dev,
	uint8_t         vcsel_delay__a0,
	uint8_t         calib_1,
	uint8_t         calib_2,
	uint8_t         calib_3,
	uint8_t         calib_2__a0,
	uint8_t         spad_readout)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	uint8_t      comms_buffer[3];

	LOG_FUNCTION_START("");




	status = VL53L1_enable_powerforce(Dev);


	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_disable_firmware(Dev);





	if (status == VL53L1_ERROR_NONE) {
		status = VL53L1_WrByte(
					Dev,
					VL53L1_RANGING_CORE__VCSEL_DELAY__A0,
					vcsel_delay__a0);
	}




	if (status == VL53L1_ERROR_NONE) {







		comms_buffer[0] = calib_1;
		comms_buffer[1] = calib_2;
		comms_buffer[2] = calib_3;

		status = VL53L1_WriteMulti(
					Dev,
					VL53L1_RANGING_CORE__CALIB_1,
					comms_buffer,
					3);
	}




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
					Dev,
					VL53L1_RANGING_CORE__CALIB_2__A0,
					calib_2__a0);




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
					Dev,
					VL53L1_RANGING_CORE__SPAD_READOUT,
					spad_readout);




	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_enable_firmware(Dev);

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_set_hist_calib_pulse_delay(
	VL53L1_DEV      Dev,
	uint8_t         calib_delay)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status =
		VL53L1_set_calib_config(
			Dev,
			0x01,

			calib_delay,

			0x04,

			0x08,

			0x14,

			VL53L1_RANGING_CORE__SPAD_READOUT__CALIB_PULSES);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_disable_calib_pulse_delay(
	VL53L1_DEV      Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status =
		VL53L1_set_calib_config(
			Dev,
			0x00,

			0x00,

			0x00,

			0x00,

			0x00,

			VL53L1_RANGING_CORE__SPAD_READOUT__STANDARD);

	LOG_FUNCTION_END(status);

	return status;
}


