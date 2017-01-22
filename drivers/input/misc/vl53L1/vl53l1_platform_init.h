/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
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
*/


#ifndef _VL53L1_PLATFORM_INIT_H_
#define _VL53L1_PLATFORM_INIT_H_

#include "vl53l1_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file   vl53l1_platform_init.h
 *
 * @brief  EwokPlus25 comms and GPIO init
 */


/**
 * @brief  Initialise platform comms, GPIO and reset device
 *
 * Initialises comms, sets the states of GPIO (xshutdown, ncs,
 * EVK device power regulator enable) and resets the device
 *
 * @param[in]   pdev              : pointer to device structure (device handle)
 * @param[in]   i2c_slave_address : I2C slave address
 * @param[in]   comms_type        : Comms type: VL53L1_I2C or VL53L1_SPI
 * @param[in]   comms_speed_khz   : 400kHz recommended for I2C
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_platform_init(
	VL53L1_Dev_t *pdev,
	uint8_t       i2c_slave_address,
	uint8_t       comms_type,
	uint16_t      comms_speed_khz);


/**
 * @brief  Close platform comms and GPIO
 *
 * Puts the device into reset, disables the EVK device power regulator
 * and closes comms
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_platform_terminate(
	VL53L1_Dev_t *pdev);


#ifdef __cplusplus
}
#endif

#endif

