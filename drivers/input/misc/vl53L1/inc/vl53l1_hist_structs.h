
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





































#ifndef _VL53L1_HIST_STRUCTS_H_
#define _VL53L1_HIST_STRUCTS_H_

#include "vl53l1_ll_device.h"
#include "vl53l1_dmax_structs.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define  VL53L1_MAX_BIN_SEQUENCE_LENGTH  6
#define  VL53L1_MAX_BIN_SEQUENCE_CODE   15
#define  VL53L1_HISTOGRAM_BUFFER_SIZE   24
#define  VL53L1_XTALK_HISTO_BINS        12








typedef struct {

	uint8_t                          histogram_config__spad_array_selection;

	uint8_t                          histogram_config__low_amb_even_bin_0_1;
	uint8_t                          histogram_config__low_amb_even_bin_2_3;
	uint8_t                          histogram_config__low_amb_even_bin_4_5;

	uint8_t                          histogram_config__low_amb_odd_bin_0_1;
	uint8_t                          histogram_config__low_amb_odd_bin_2_3;
	uint8_t                          histogram_config__low_amb_odd_bin_4_5;

	uint8_t                          histogram_config__mid_amb_even_bin_0_1;
	uint8_t                          histogram_config__mid_amb_even_bin_2_3;
	uint8_t                          histogram_config__mid_amb_even_bin_4_5;

	uint8_t                          histogram_config__mid_amb_odd_bin_0_1;
	uint8_t                          histogram_config__mid_amb_odd_bin_2;
	uint8_t                          histogram_config__mid_amb_odd_bin_3_4;
	uint8_t                          histogram_config__mid_amb_odd_bin_5;

	uint8_t                          histogram_config__user_bin_offset;

	uint8_t                     histogram_config__high_amb_even_bin_0_1;
	uint8_t                     histogram_config__high_amb_even_bin_2_3;
	uint8_t                     histogram_config__high_amb_even_bin_4_5;

	uint8_t                  histogram_config__high_amb_odd_bin_0_1;
	uint8_t                  histogram_config__high_amb_odd_bin_2_3;
	uint8_t                  histogram_config__high_amb_odd_bin_4_5;

	uint16_t                         histogram_config__amb_thresh_low;



	uint16_t                         histogram_config__amb_thresh_high;




} VL53L1_histogram_config_t;









typedef struct {

	VL53L1_HistAlgoSelect  hist_algo_select;



	VL53L1_HistTargetOrder hist_target_order;





	uint8_t   filter_woi0;



	uint8_t   filter_woi1;




	VL53L1_HistAmbEstMethod hist_amb_est_method;


	uint8_t   ambient_thresh_sigma0;



	uint8_t   ambient_thresh_sigma1;





	uint16_t  ambient_thresh_events_scaler;






	int32_t   min_ambient_thresh_events;


	uint16_t  noise_threshold;



	int32_t   signal_total_events_limit;


	uint8_t	  sigma_estimator__sigma_ref_mm;


	uint16_t  sigma_thresh;


	int16_t   range_offset_mm;


	uint16_t  gain_factor;



	uint8_t   valid_phase_low;



	uint8_t   valid_phase_high;



	uint8_t   algo__consistency_check__phase_tolerance;



	uint8_t   algo__consistency_check__event_sigma;






	uint16_t  algo__consistency_check__event_min_spad_count;






	uint16_t  algo__consistency_check__min_max_tolerance;



	uint8_t   algo__crosstalk_compensation_enable;


	uint32_t  algo__crosstalk_compensation_plane_offset_kcps;


	int16_t   algo__crosstalk_compensation_x_plane_gradient_kcps;


	int16_t   algo__crosstalk_compensation_y_plane_gradient_kcps;



	int16_t   algo__crosstalk_detect_min_valid_range_mm;


	int16_t   algo__crosstalk_detect_max_valid_range_mm;


	uint16_t  algo__crosstalk_detect_max_valid_rate_kcps;



	uint16_t  algo__crosstalk_detect_max_sigma_mm;






	uint8_t   algo__crosstalk_detect_event_sigma;






	uint16_t  algo__crosstalk_detect_min_max_tolerance;




} VL53L1_hist_post_process_config_t;







typedef struct {



	VL53L1_DeviceState     cfg_device_state;


	VL53L1_DeviceState     rd_device_state;



	uint8_t  zone_id;


	uint32_t time_stamp;



	uint8_t  VL53L1_p_022;


	uint8_t  VL53L1_p_023;


	uint8_t  VL53L1_p_024;



	uint8_t  number_of_ambient_bins;



	uint8_t  bin_seq[VL53L1_MAX_BIN_SEQUENCE_LENGTH];


	uint8_t  bin_rep[VL53L1_MAX_BIN_SEQUENCE_LENGTH];




	int32_t  bin_data[VL53L1_HISTOGRAM_BUFFER_SIZE];



	uint8_t  result__interrupt_status;


	uint8_t  result__range_status;


	uint8_t  result__report_status;


	uint8_t  result__stream_count;


	uint16_t result__dss_actual_effective_spads;



	uint16_t phasecal_result__reference_phase;


	uint8_t  phasecal_result__vcsel_start;


	uint8_t  cal_config__vcsel_start;


	uint16_t vcsel_width;


	uint8_t  VL53L1_p_009;


	uint16_t VL53L1_p_019;


	uint32_t  total_periods_elapsed;



	uint32_t peak_duration_us;


	uint32_t woi_duration_us;



	int32_t  min_bin_value;


	int32_t  max_bin_value;



	uint16_t zero_distance_phase;


	uint8_t  number_of_ambient_samples;


	int32_t  ambient_events_sum;


	int32_t  VL53L1_p_004;



	uint8_t  roi_config__user_roi_centre_spad;


	uint8_t  roi_config__user_roi_requested_global_xy_size;



} VL53L1_histogram_bin_data_t;








typedef struct {



	uint8_t  zone_id;


	uint32_t time_stamp;



	uint8_t  VL53L1_p_022;


	uint8_t  VL53L1_p_023;


	uint8_t  VL53L1_p_024;


	uint32_t bin_data[VL53L1_XTALK_HISTO_BINS];




	uint16_t phasecal_result__reference_phase;


	uint8_t  phasecal_result__vcsel_start;


	uint8_t  cal_config__vcsel_start;


	uint16_t vcsel_width;


	uint16_t VL53L1_p_019;


	uint16_t zero_distance_phase;



} VL53L1_xtalk_histogram_shape_t;








typedef struct {



	VL53L1_xtalk_histogram_shape_t  xtalk_shape;


	VL53L1_histogram_bin_data_t     xtalk_hist_removed;

} VL53L1_xtalk_histogram_data_t;




#ifdef __cplusplus
}
#endif

#endif

