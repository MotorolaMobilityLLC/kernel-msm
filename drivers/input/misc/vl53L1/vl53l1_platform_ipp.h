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
*
*/

/**
 * @file vl53l1_platform_ipp.h
 * @date : Oct 3, 2016
 *
 * ST protected ip wrapper for linux kernel
 */

#ifndef _VL53L1_PLATFORM_IPP_H_
#define _VL53L1_PLATFORM_IPP_H_

#include "vl53l1_platform.h"

VL53L1_Error VL53L1_ipp_hist_process_data(
	VL53L1_DEV                         Dev,
	VL53L1_hist_post_process_config_t *ppost_cfg,
	VL53L1_histogram_bin_data_t       *pbins0,
	VL53L1_histogram_bin_data_t       *pbins1,
	VL53L1_xtalk_histogram_data_t     *pxtalk,
	VL53L1_range_results_t            *presults);

VL53L1_Error VL53L1_ipp_xtalk_calibration_process_data(
	VL53L1_DEV                          Dev,
	VL53L1_xtalk_range_results_t       *pxtalk_ranges,
	VL53L1_xtalk_histogram_data_t      *pxtalk_shape,
	VL53L1_xtalk_calibration_results_t *pxtalk_cal);

#endif /* DRIVERS_INPUT_MISC_VL53L1_VL53L1_PLATFORM_IPP_H_ */
