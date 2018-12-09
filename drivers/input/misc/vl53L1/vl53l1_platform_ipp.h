/**************************************************************************
 * Copyright (c) 2016, STMicroelectronics - All Rights Reserved

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
 ****************************************************************************/


#ifndef _VL53L1_PLATFORM_IPP_H_
#define _VL53L1_PLATFORM_IPP_H_

#include "vl53l1_ll_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @file   vl53l1_platform_ipp.h
 *
 * @brief  EwokPlus25 IPP Wrapper Functions
 */

/**
 * @brief  IPP Wrapper call for histogram post processing
 *
 *
 * @param[in]    Dev                       : Device handle
 * @param[in]    pdmax_cal                 : DMAX calibration data
 * @param[in]    pdmax_cfg                 : DMAX configuration data
 * @param[in]    ppost_cfg                 : VL53L1_hist_post_process_config_t
 * @param[in]    pbins                     : Input histogram raw bin data
 * @param[in]    pxtalk                    : Cross talk histogram data
 * @param[out]   presults                  : Output VL53L1_range_results_t
 *                                           structure
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_ipp_hist_process_data(
	VL53L1_DEV                         Dev,
	VL53L1_dmax_calibration_data_t    *pdmax_cal,
	VL53L1_hist_gen3_dmax_config_t    *pdmax_cfg,
	VL53L1_hist_post_process_config_t *ppost_cfg,
	VL53L1_histogram_bin_data_t       *pbins,
	VL53L1_xtalk_histogram_data_t     *pxtalk,
	VL53L1_range_results_t            *presults);


/**
 * @brief  IPP Wrapper call for histogram ambient dmax calc
 *
 * The target reflectance in percent for the DMAX calculation
 * is set by target_reflectance input
 *
 * The fixed point format is 7.2
 *
 * @param[in]    Dev                : Device handle
 * @param[in]    target_reflectance : target reflectance to report ambient DMAX
 *                                    Percentage in 7.2 fixed point format
 * @param[in]    pdmax_cal          : DMAX calibration data
 * @param[in]    pdmax_cfg          : DMAX configuration data
 * @param[in]    pbins              : Input histogram raw bin data
 * @param[out]   pambient_dmax_mm   : Output ambient DMAX distance in [mm]
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_ipp_hist_ambient_dmax(
	VL53L1_DEV                         Dev,
	uint16_t                           target_reflectance,
	VL53L1_dmax_calibration_data_t    *pdmax_cal,
	VL53L1_hist_gen3_dmax_config_t    *pdmax_cfg,
	VL53L1_histogram_bin_data_t       *pbins,
	int16_t                           *pambient_dmax_mm);


/**
 * @brief  IPP Wrapper call for xtalk calibration post processing
 *
 * @param[in]      Dev                 : Device handle
 * @param[in]      pxtalk_ranges       : Input VL53L1_xtalk_range_results_t
 *                                       Must contain 5 ranges, 4 quadrants + 1
 *                                       full FoV
 * @param[out]     pxtalk_shape        : Output normalised Cross talk  histogram
 *                                       shape
 * @param[out]     pxtalk_cal        : Output VL53L1_xtalk_calibration_results_t
 *                                     structure
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_ipp_xtalk_calibration_process_data(
	VL53L1_DEV                          Dev,
	VL53L1_xtalk_range_results_t       *pxtalk_ranges,
	VL53L1_xtalk_histogram_data_t      *pxtalk_shape,
	VL53L1_xtalk_calibration_results_t *pxtalk_cal);


/**
 * @brief  IPP Wrapper call for applying histogram xtalk correction
 *
 * @param[in]   Dev                 : Device handle
 * @param[in]   pcustomer           : Pointer to input customer data structure
 * @param[in]   pdyn_cfg            : Pointer to input dynamic parameters
 *                                    structure
 * @param[in]   pxtalk_shape        : Pointer to input normalised xtalk
 *                                    histogram shape
 * @param[in]   pip_hist_data       : Pointer to input histogram data struct
 * @param[out]  pop_hist_data       : Pointer to output xtalk corrected
 *                                    histogram data struct
 * @param[out]  pxtalk_count_data    : Pointer to output xtalk histogram
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_ipp_hist_xtalk_correction(
	VL53L1_DEV                    Dev,
	VL53L1_customer_nvm_managed_t *pcustomer,
	VL53L1_dynamic_config_t       *pdyn_cfg,
	VL53L1_xtalk_histogram_data_t *pxtalk_shape,
	VL53L1_histogram_bin_data_t   *pip_hist_data,
	VL53L1_histogram_bin_data_t   *pop_hist_data,
	VL53L1_histogram_bin_data_t   *pxtalk_count_data);

/**
 * @brief  IPP Wrapper call for Generating Xtalk data from dual reflectance
 * histogram data
 *
 * @param[in]  Dev                          : Device handle
 * @param[in]  pxtalk_results               : Pointer to xtalk_results structure
 *                                            containing dual reflectance
 *                                            histogram data
 * @param[in]  expected_target_distance_mm  : User input of true target distance
 * @param[in]  higher_reflectance           : User input detailing which
 *                                            histogram data 1 or 2 has the
 *                                            highest reflectance.
 * @param[out] pxtalk_avg_samples           : Pointer to output xtalk histogram
 *                                            data
 *
 * @return   VL53L1_ERROR_NONE    Success
 * @return  "Other error code"    See ::VL53L1_Error
 */

VL53L1_Error VL53L1_ipp_generate_dual_reflectance_xtalk_samples(
	VL53L1_DEV                     Dev,
	VL53L1_xtalk_range_results_t  *pxtalk_results,
	uint16_t                       expected_target_distance_mm,
	uint8_t                        higher_reflectance,
	VL53L1_histogram_bin_data_t	  *pxtalk_avg_samples);



#ifdef __cplusplus
}
#endif

#endif

