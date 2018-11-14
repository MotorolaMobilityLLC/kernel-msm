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

/**
 * @file  vl53l1_platform_user_config.h
 *
 * @brief EwokPlus compile time user modifiable configuration
 */


#ifndef _VL53L1_PLATFORM_USER_CONFIG_H_
#define _VL53L1_PLATFORM_USER_CONFIG_H_

#define    VL53L1_BYTES_PER_WORD              2
#define    VL53L1_BYTES_PER_DWORD             4

/* Define polling delays */
#define VL53L1_BOOT_COMPLETION_POLLING_TIMEOUT_MS     500
#define VL53L1_RANGE_COMPLETION_POLLING_TIMEOUT_MS   2000
#define VL53L1_TEST_COMPLETION_POLLING_TIMEOUT_MS   60000

#define VL53L1_POLLING_DELAY_MS                         1

/* Define LLD TuningParms Page Base Address
 * - Part of Patch_AddedTuningParms_11761
 */
#define VL53L1_TUNINGPARM_PUBLIC_PAGE_BASE_ADDRESS  0x8000
#define VL53L1_TUNINGPARM_PRIVATE_PAGE_BASE_ADDRESS 0xC000

#define VL53L1_GAIN_FACTOR__STANDARD_DEFAULT       0x0800
	/*!<  Default standard ranging gain correction factor
	 * 1.11 format. 1.0 = 0x0800, 0.980 = 0x07D7
	 */
#define VL53L1_GAIN_FACTOR__HISTOGRAM_DEFAULT      0x0800
	/*!<  Default histogram ranging gain correction factor
	 * 1.11 format. 1.0 = 0x0800, 0.975 = 0x07CC
	 */


#define VL53L1_OFFSET_CAL_MIN_EFFECTIVE_SPADS  0x0500
	/*!< Lower Limit for the  MM1 effective SPAD count during offset
	 * calibration Format 8.8 0x0500 -> 5.0 effective SPADs
	 */

#define VL53L1_OFFSET_CAL_MAX_PRE_PEAK_RATE_MCPS   0x1900
	/*!< Max Limit for the pre range preak rate during offset
	 * calibration Format 9.7 0x1900 -> 50.0 Mcps.
	 * If larger then in pile up
	 */

#define VL53L1_OFFSET_CAL_MAX_SIGMA_MM             0x0040
	/*!< Max sigma estimate limit during offset calibration
	 *   Check applies to pre-range, mm1 and mm2 ranges
	 *   Format 14.2 0x0040 -> 16.0mm.
	 */


#define VL53L1_ZONE_CAL_MAX_PRE_PEAK_RATE_MCPS     0x1900
	/*!< Max Peak Rate Limit for the during zone calibration
	 *   Format 9.7 0x1900 -> 50.0 Mcps.
	 *   If larger then in pile up
	 */

#define VL53L1_ZONE_CAL_MAX_SIGMA_MM               0x0040
	/*!< Max sigma estimate limit during zone calibration
	 *   Format 14.2 0x0040 -> 16.0mm.
	 */


#define VL53L1_XTALK_EXTRACT_MAX_SIGMA_MM          0x008C
	/*!< Max Sigma value allowed for a successful xtalk extraction
	 *   Format 14.2 0x008C -> 35.0 mm.
	 */


#define VL53L1_MAX_USER_ZONES                169
	/*!< Max number of user Zones - maximal limitation from
	 * FW stream divide - value of 254
	 */

#define VL53L1_MAX_RANGE_RESULTS              4
	/*!< Sets the maximum number of targets distances the histogram
	 * post processing can generate
	 */

#define VL53L1_MAX_STRING_LENGTH 512

#endif  /* _VL53L1_PLATFORM_USER_CONFIG_H_ */

