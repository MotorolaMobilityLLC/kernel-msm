/*******************************************************************************
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
/**
 * @file   vl53l1_platform.h
 *
 * @brief  linux kernel driver OS/platform/application porting
 */
#ifndef _VL53L1_PLATFORM_H_
#define _VL53L1_PLATFORM_H_

/** API fct export type
 *
 *  N/A undefined
 **/
#define VL53L1_API

/**
 * time out for polling/Wait method  use in ST bare drivers
 * even not used require for code to compile
 */
#define VL53L1_BOOT_COMPLETION_POLLING_TIMEOUT_MS     500
#define VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS   1000
#define VL53L1_TEST_COMPLETION_POLLING_TIMEOUT_MS   10000
#define VL53L1_POLLING_DELAY_MS                         1

#include "vl53l1_def.h"


#define VL53L1_MAX_I2C_XFER_SIZE	256
/* linux porting of bare driver */
#define _LOG_FUNCTION_START(...) (void)0
#define _LOG_FUNCTION_END(...) (void)0

/**
 * ST bare driver data type access macro
 */
#define PALDevDataGet(Dev, field) (Dev->field)

#define PALDevDataSet(Dev, field, data) ((Dev->field) = (data))

/**
 * ST bare LL driver data type access macro
 */
#define VL53L1DevStructGetLLDriverHandle(Dev) (&PALDevDataGet(Dev, LLData))



/**
 * @brief Typedef defining 8 bit unsigned char type.\n
 * The developer should modify this to suit the platform being deployed.
 *
 */

#ifndef bool_t
#define bool_t unsigned char
#endif

/**
 * VL53L1_DEV ST ll/bare driver absract type
 *
 * we include st base stuct within our driver data and will pass ptr to it
 * on any call back from st layer we'll find out or struct ptr using
 *  container_of(pdev,struct stmvl53l1_data, stdev);
 */
#define VL53L1_DEV VL53L1_DevData_t *

/**
 * @brief Writes the supplied byte buffer to the device
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index value
 * @param[in]   pdata     : pointer to uint8_t (byte) buffer containing the
 *  data to be written
 * @param[in]   count     : number of bytes in the supplied byte buffer
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WriteMulti(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint8_t    *pdata,
		uint32_t   count);


/**
 * @brief  Reads the requested number of bytes from the device
 *
 * @param[in]   pdev  : pointer to device structure (device handle)
 * @param[in]   index : uint16_t register index value
 * @param[out]  pdata : pointer to the uint8_t (byte) buffer to store read data
 * @param[in]   count : number of bytes to read
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_ReadMulti(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint8_t    *pdata,
		uint32_t    count);


/**
 * @brief  Writes a single byte to the device
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index value
 * @param[in]   data      : uint8_t data value to write
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WrByte(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint8_t    data);


/**
 * @brief  Writes a single w(ord (16-bit unsigned) to the device
 *
 * Manages the big-endian nature of the device register map
 * (first byte written is the MS byte).
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index value
 * @param[in]   data      : uin16_t data value write
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WrWord(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint16_t   data);


/**
 * @brief  Writes a single dword (32-bit unsigned) to the device
 *
 * Manages the big-endian nature of the device register map
 * (first byte written is the MS byte).
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index value
 * @param[in]   data      : uint32_t data value to write
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WrDWord(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint32_t   data);



/**
 * @brief  Reads a single byte from the device
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index
 * @param[out]  pdata     : pointer to uint8_t data value
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 *
 */

VL53L1_API VL53L1_Error VL53L1_RdByte(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint8_t    *pdata);


/**
 * @brief  Reads a single word (16-bit unsigned) from the device
 *
 * Manages the big-endian nature of the device (first byte read is the MS byte).
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index value
 * @param[out]  pdata     : pointer to uint16_t data value
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_RdWord(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint16_t   *pdata);


/**
 * @brief  Reads a single dword (32-bit unsigned) from the device
 *
 * Manages the big-endian nature of the device (first byte read is the MS byte).
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   index     : uint16_t register index value
 * @param[out]  pdata     : pointer to uint32_t data value
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_RdDWord(
		VL53L1_DEV pdev,
		uint16_t   index,
		uint32_t   *pdata);

/**
 * @brief Register "wait for value" polling routine
 * Poll for
 *
 * @param[in]   pdev          : pointer to device structure (device handle)
 * @param[in]   timeout_ms    : timeout in [ms]
 * @param[in]   index         : uint16_t register index value
 * @param[in]   value         : value to wait for
 * @param[in]   mask          : mask to be applied before comparison with value
 * @param[in]   poll_delay_ms : polling delay been each read transaction in [ms]
 *
 * @return  VL53L1_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WaitValueMaskEx(
		VL53L1_DEV pdev,
		uint32_t   timeout_ms,
		uint16_t   index,
		uint8_t    value,
		uint8_t    mask,
		uint32_t   poll_delay_ms);

/**
 * @brief  Implements a programmable wait in us
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   wait_us   : integer wait in micro seconds
 *
 * @return  VL53L1_ERROR_NONE     Success
 * @return  VL53L1_ERROR_PLATFORM_SPECIFIC_START if context disallow wait
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WaitUs(VL53L1_DEV pdev, int32_t wait_us);

/**
 * @brief  Implements a programmable wait in ms
 *
 * @param[in]   pdev      : pointer to device structure (device handle)
 * @param[in]   wait_ms   : integer wait in milliseconds
 *
 * @return  VL53L1_ERROR_NONE     Success
 * @return  VL53L1_ERROR_PLATFORM_SPECIFIC_START if context disallow wait
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_API VL53L1_Error VL53L1_WaitMs(VL53L1_DEV pdev, int32_t wait_ms);

/**
 * we do not support Get tick count
 * the implementation will hang kernel with BUG_ON() if called
 */
void VL53L1_GetTickCount(void *);


#include "vl53l1_platform_log.h"

#endif

