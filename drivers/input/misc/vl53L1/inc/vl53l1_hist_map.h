
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





































#ifndef _VL53L1_HIST_MAP_H_
#define _VL53L1_HIST_MAP_H_

#include "vl53l1_register_map.h"

#ifdef __cplusplus
extern "C"
{
#endif





#define VL53L1_HISTOGRAM_CONFIG__OPCODE_SEQUENCE_0 \
			VL53L1_SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS

#define VL53L1_HISTOGRAM_CONFIG__OPCODE_SEQUENCE_1 \
		    VL53L1_SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS

#define VL53L1_HISTOGRAM_CONFIG__OPCODE_SEQUENCE_2 \
		    VL53L1_SIGMA_ESTIMATOR__SIGMA_REF_MM

#define VL53L1_HISTOGRAM_CONFIG__AMB_THRESH_HIGH \
		    VL53L1_ALGO__RANGE_IGNORE_THRESHOLD_MCPS





#define VL53L1_RESULT__HISTOGRAM_BIN_0_2                               0x008E
#define VL53L1_RESULT__HISTOGRAM_BIN_0_1                               0x008F
#define VL53L1_RESULT__HISTOGRAM_BIN_0_0                               0x0090

#define VL53L1_RESULT__HISTOGRAM_BIN_23_2                              0x00D3
#define VL53L1_RESULT__HISTOGRAM_BIN_23_1                              0x00D4
#define VL53L1_RESULT__HISTOGRAM_BIN_23_0                              0x00D5

#define VL53L1_RESULT__HISTOGRAM_BIN_23_0_MSB                          0x00D9
#define VL53L1_RESULT__HISTOGRAM_BIN_23_0_LSB                          0x00DA




#define VL53L1_HISTOGRAM_BIN_DATA_I2C_INDEX       \
	VL53L1_RESULT__INTERRUPT_STATUS
#define VL53L1_HISTOGRAM_BIN_DATA_I2C_SIZE_BYTES  \
	(VL53L1_RESULT__HISTOGRAM_BIN_23_0_LSB - VL53L1_RESULT__INTERRUPT_STATUS + 1)

#ifdef __cplusplus
}
#endif

#endif

