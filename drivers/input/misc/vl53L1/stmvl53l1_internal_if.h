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

#ifndef STMVL53L1_INTERNAL_IF_H
#define STMVL53L1_INTERNAL_IF_H

#include "vl53l1_def.h"

/* interface definition move in this file is not supposed to be use by a normal
 * client. It's only here for internal testing purpose.
 */

/* structure and ioctl that allow raw access to vl53l1 register */
struct stmvl53l1_register {
	uint32_t is_read;   /*!< type of the access 1: read 0: write*/
	uint32_t index;     /*!< register index */
	uint32_t cnt;       /*!< register size shall be 1 to n */
	int32_t status;     /*!< operation status 0 ok else error */

	union reg_data_t {
		uint8_t b;  /*!< single data byte*/
		uint16_t w; /*!< single data word (16 bits)*/
		uint32_t dw;    /*!< single data dword (32 bits)*/
		uint8_t bytes[256]; /*!< any size byte array
		* @note only effectively used array size is needed and will be
		* set/used another possible register definition is
		* @ref stmvl53l1_register_flexi
		*/
	} data; /*!< data only *@warning device is big endian and
	* no endianess adaptation is performed by @ref VL53L1_IOCTL_REGISTER
	*/
};

struct stmvl53l1_register_flexi {
	uint32_t is_read;   /*!< [in] type of the access 1: read 0: write*/
	uint32_t index;     /*!< [in] register index */
	uint32_t cnt;       /*!< [în] register size shall be 1 to n */
	int32_t status;     /*!< [out] operation status 0 ok else error */
	uint8_t data[];     /*!< [in/out] flexible array size data */
	/*!< data only *@warning device is big endian and
	* no endianess adaptation is performed by @ref VL53L1_IOCTL_REGISTER
	*/
};

#define VL53L1_IOCTL_REGISTER   _IOWR('p', 0x0c, struct stmvl53l1_register)

struct stmvl53l1_data_with_additional {
	VL53L1_MultiRangingData_t data;
	VL53L1_AdditionalData_t additional_data;
};

/**
 * Get multi object/zone ranging data with additional data for debug
 *
 * this call is non blocking and will return what available internally
 * in all case (veen error)
 *
 * @param [out] multi zone range @ref VL53L1_MultiRangingData_t always update
 * but -EFAULT error case
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copyed
 * @li -ENOEXEC active mode is not mutli-zone
 * @li -ENODEV device is not ranging or device has been removed.
 * as in that case MZ data may not be fully valid
 */
#define VL53L1_IOCTL_MZ_DATA_ADDITIONAL\
			_IOR('p', 0x15, struct stmvl53l1_data_with_additional)

/**
 * Get multi object/zone ranging data
 *
 * this call is equivalent to VL53L1_IOCTL_MZ_DATA_ADDITIONAL but will block
 * until new data are available since previous call.
 *
 * @param [out] multi zone range @ref VL53L1_MultiRangingData_t always update
 * but -EFAULT error case
 *
 * @return 0 on success else o, error check errno
 * @li -EFAULT fault in cpy to f/m user out range data not copyed
 * @li -ENOEXEC active mode is not mutli-zone
 * @li -ENODEV device is not ranging or device has been removed.
 * @li -ERESTARTSYS interrupt while sleeping.
 * as in that case MZ data may not be fully valid
 */
#define VL53L1_IOCTL_MZ_DATA_ADDITIONAL_BLOCKING\
			_IOR('p', 0x16, struct stmvl53l1_data_with_additional)

#endif
