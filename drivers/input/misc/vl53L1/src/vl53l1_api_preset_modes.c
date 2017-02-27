
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




































#include "vl53l1_ll_def.h"
#include "vl53l1_platform_log.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_zone_presets.h"
#include "vl53l1_core.h"
#include "vl53l1_api_preset_modes.h"


#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)


VL53L1_Error VL53L1_init_refspadchar_config_struct(
	VL53L1_refspadchar_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");











	pdata->device_test_mode =
			VL53L1_DEVICETESTMODE_REF_SPAD_CHAR_WITH_PRE_VHV;
	pdata->VL53L1_PRM_00007              = 0x0B;
	pdata->timeout_us                = 1000;
	pdata->target_count_rate_mcps    = 0x0A00;
	pdata->min_count_rate_limit_mcps = 0x0500;
	pdata->max_count_rate_limit_mcps = 0x1400;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_init_ssc_config_struct(
	VL53L1_ssc_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");








	pdata->array_select = VL53L1_DEVICESSCARRAY_RTN;



	pdata->VL53L1_PRM_00007 = 0x12;



	pdata->vcsel_start  = 0x0F;



	pdata->vcsel_start  = 0x02;



	pdata->timeout_us   = 36000;






	pdata->rate_limit_mcps = 0x000C;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_init_xtalk_config_struct(
	VL53L1_customer_nvm_managed_t *pnvm,
	VL53L1_xtalk_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");
















	pdata->algo__crosstalk_compensation_plane_offset_kcps      =
		pnvm->algo__crosstalk_compensation_plane_offset_kcps;
	pdata->algo__crosstalk_compensation_x_plane_gradient_kcps  =
		pnvm->algo__crosstalk_compensation_x_plane_gradient_kcps;
	pdata->algo__crosstalk_compensation_y_plane_gradient_kcps  =
		pnvm->algo__crosstalk_compensation_y_plane_gradient_kcps;




	pdata->nvm_default__crosstalk_compensation_plane_offset_kcps      =
		pnvm->algo__crosstalk_compensation_plane_offset_kcps;
	pdata->nvm_default__crosstalk_compensation_x_plane_gradient_kcps  =
		pnvm->algo__crosstalk_compensation_x_plane_gradient_kcps;
	pdata->nvm_default__crosstalk_compensation_y_plane_gradient_kcps  =
		pnvm->algo__crosstalk_compensation_y_plane_gradient_kcps;

	pdata->histogram_mode_crosstalk_margin_kcps                =
			0x00;
	pdata->lite_mode_crosstalk_margin_kcps                     =
			0x00;




	pdata->crosstalk_range_ignore_threshold_mult = 64;

	if ((pdata->algo__crosstalk_compensation_plane_offset_kcps == 0x00)
		&& (pdata->algo__crosstalk_compensation_x_plane_gradient_kcps == 0x00)
		&& (pdata->algo__crosstalk_compensation_y_plane_gradient_kcps == 0x00))
		pdata->global_crosstalk_compensation_enable = 0x00;
	else
		pdata->global_crosstalk_compensation_enable = 0x01;


	if ((status == VL53L1_ERROR_NONE) &&
		(pdata->global_crosstalk_compensation_enable == 0x01)) {
		pdata->crosstalk_range_ignore_threshold_rate_mcps =
			VL53L1_calc_range_ignore_threshold(
				pdata->algo__crosstalk_compensation_plane_offset_kcps,
				pdata->algo__crosstalk_compensation_x_plane_gradient_kcps,
				pdata->algo__crosstalk_compensation_y_plane_gradient_kcps,
				pdata->crosstalk_range_ignore_threshold_mult);
	} else {
		pdata->crosstalk_range_ignore_threshold_rate_mcps = 0;
	}









	pdata->algo__crosstalk_detect_min_valid_range_mm  =    -50;
	pdata->algo__crosstalk_detect_max_valid_range_mm  =     50;
	pdata->algo__crosstalk_detect_max_valid_rate_kcps = 0xC800;
	pdata->algo__crosstalk_detect_max_sigma_mm        =
			VL53L1_XTALK_EXTRACT_MAX_SIGMA_MM;


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_init_hist_post_process_config_struct(
	uint8_t                             xtalk_compensation_enable,
	VL53L1_hist_post_process_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	pdata->hist_algo_select =
		VL53L1_HIST_ALGO_SELECT__PW_HIST_GEN4;

	pdata->hist_target_order =
			VL53L1_HIST_TARGET_ORDER__INCREASING_DISTANCE;




	pdata->filter_woi0                   =   1;

	pdata->filter_woi1                   =   2;


	pdata->hist_amb_est_method =
		VL53L1_HIST_AMB_EST_METHOD__AMBIENT_BINS;
	pdata->ambient_thresh_sigma0         =  80;

	pdata->ambient_thresh_sigma1         = 112;

	pdata->min_ambient_thresh_events     =  16;


	pdata->noise_threshold               =  50;


	pdata->signal_total_events_limit     = 100;

	pdata->sigma_estimator__sigma_ref_mm =   1;














	pdata->sigma_thresh                  = 160;

	pdata->range_offset_mm            =      0;


	pdata->gain_factor                = 0x0800;















	pdata->valid_phase_low  = 0x08;
	pdata->valid_phase_high = 0x88;









	pdata->algo__consistency_check__phase_tolerance = 8;







	pdata->algo__consistency_check__event_sigma    = 112;



	pdata->algo__crosstalk_compensation_enable = xtalk_compensation_enable;







	pdata->algo__crosstalk_detect_min_valid_range_mm  =    -50;
	pdata->algo__crosstalk_detect_max_valid_range_mm  =     50;
	pdata->algo__crosstalk_detect_max_valid_rate_kcps = 0xC800;
	pdata->algo__crosstalk_detect_max_sigma_mm        =
			VL53L1_XTALK_EXTRACT_MAX_SIGMA_MM;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_init_dmax_calibration_data_struct(
	VL53L1_dmax_calibration_data_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	pdata->ref__actual_effective_spads         = 0x5F2D;


	pdata->ref__peak_signal_count_rate_mcps    = 0x0844;


	pdata->ref__distance_mm                    = 0x08A5;








	pdata->ref_reflectance_pc                  = 0x000F;



	pdata->coverglass_transmission             = 0x0100;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_init_hist_gen3_dmax_config_struct(
	VL53L1_hist_gen3_dmax_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");



	pdata->dss_config__target_total_rate_mcps  = 0x1400;
	pdata->dss_config__aperture_attenuation    = 0x38;

	pdata->signal_thresh_sigma                 = 0x20;
	pdata->ambient_thresh_sigma                = 0x70;
	pdata->min_ambient_thresh_events           = 16;
	pdata->estimated_spad_yield                = 85;











	pdata->target_reflectance_for_dmax_calc[0] =  15;
	pdata->target_reflectance_for_dmax_calc[1] =  52;
	pdata->target_reflectance_for_dmax_calc[2] = 200;
	pdata->target_reflectance_for_dmax_calc[3] = 364;
	pdata->target_reflectance_for_dmax_calc[4] = 400;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_standard_ranging(
	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	pstatic->dss_config__target_total_rate_mcps               = 0x0A00;
	pstatic->debug__ctrl                                      = 0x00;
	pstatic->test_mode__ctrl                                  = 0x00;
	pstatic->clk_gating__ctrl                                 = 0x00;
	pstatic->nvm_bist__ctrl                                   = 0x00;
	pstatic->nvm_bist__num_nvm_words                          = 0x00;
	pstatic->nvm_bist__start_address                          = 0x00;
	pstatic->host_if__status                                  = 0x00;
	pstatic->pad_i2c_hv__config                               = 0x00;
	pstatic->pad_i2c_hv__extsup_config                        = 0x00;






	pstatic->gpio_hv_pad__ctrl                                = 0x00;








	pstatic->gpio_hv_mux__ctrl  = \
			VL53L1_DEVICEINTERRUPTPOLARITY_ACTIVE_LOW | \
			VL53L1_DEVICEGPIOMODE_OUTPUT_RANGE_AND_ERROR_INTERRUPTS;

	pstatic->gpio__tio_hv_status                              = 0x02;
	pstatic->gpio__fio_hv_status                              = 0x00;
	pstatic->ana_config__spad_sel_pswidth                     = 0x02;
	pstatic->ana_config__vcsel_pulse_width_offset             = 0x08;
	pstatic->ana_config__fast_osc__config_ctrl                = 0x00;

	pstatic->sigma_estimator__effective_pulse_width_ns        = 0x08;
	pstatic->sigma_estimator__effective_ambient_width_ns      = 0x10;
	pstatic->sigma_estimator__sigma_ref_mm                    = 0x01;


	pstatic->algo__crosstalk_compensation_valid_height_mm     = 0x01;
	pstatic->spare_host_config__static_config_spare_0         = 0x00;
	pstatic->spare_host_config__static_config_spare_1         = 0x00;

	pstatic->algo__range_ignore_threshold_mcps                = 0x0000;



	pstatic->algo__range_ignore_valid_height_mm               = 0xff;
	pstatic->algo__range_min_clip                             = 0x00;






	pstatic->algo__consistency_check__tolerance               = 0x02;
	pstatic->spare_host_config__static_config_spare_2         = 0x00;
	pstatic->sd_config__reset_stages_msb                      = 0x00;
	pstatic->sd_config__reset_stages_lsb                      = 0x00;

	pgeneral->gph_config__stream_count_update_value           = 0x00;
	pgeneral->global_config__stream_divider                   = 0x00;
	pgeneral->system__interrupt_config_gpio =
			VL53L1_INTERRUPT_CONFIG_NEW_SAMPLE_READY;
	pgeneral->cal_config__vcsel_start                         = 0x0B;








	pgeneral->cal_config__repeat_rate                         =    0;
	pgeneral->global_config__vcsel_width                      = 0x02;


	pgeneral->phasecal_config__timeout_macrop                 = 0x0D;


	pgeneral->phasecal_config__target                         = 0x21;
	pgeneral->phasecal_config__override                       = 0x00;
	pgeneral->dss_config__roi_mode_control =
			VL53L1_DEVICEDSSMODE__TARGET_RATE;


	pgeneral->system__thresh_rate_high                        = 0x0000;
	pgeneral->system__thresh_rate_low                         = 0x0000;


	pgeneral->dss_config__manual_effective_spads_select       = 0x8C00;
	pgeneral->dss_config__manual_block_select                 = 0x00;








	pgeneral->dss_config__aperture_attenuation                = 0x38;
	pgeneral->dss_config__max_spads_limit                     = 0xFF;
	pgeneral->dss_config__min_spads_limit                     = 0x01;




	ptiming->mm_config__timeout_macrop_a_hi                   = 0x00;
	ptiming->mm_config__timeout_macrop_a_lo                   = 0x06;
	ptiming->mm_config__timeout_macrop_b_hi                   = 0x00;
	ptiming->mm_config__timeout_macrop_b_lo                   = 0x06;
	ptiming->range_config__timeout_macrop_a_hi                = 0x02;
	ptiming->range_config__timeout_macrop_a_lo                = 0x85;


	ptiming->range_config__vcsel_period_a                     = 0x0B;
	ptiming->range_config__timeout_macrop_b_hi                = 0x01;
	ptiming->range_config__timeout_macrop_b_lo                = 0x92;


	ptiming->range_config__vcsel_period_b                     = 0x09;







	ptiming->range_config__sigma_thresh                       = 0x003C;






	ptiming->range_config__min_count_rate_rtn_limit_mcps      = 0x0080;






	ptiming->range_config__valid_phase_low                    = 0x08;
	ptiming->range_config__valid_phase_high                   = 0x78;
	ptiming->system__intermeasurement_period                  = 0x00000000;
	ptiming->system__fractional_enable                        = 0x00;




























	phistogram->histogram_config__low_amb_even_bin_0_1        = 0x07;
	phistogram->histogram_config__low_amb_even_bin_2_3        = 0x21;
	phistogram->histogram_config__low_amb_even_bin_4_5        = 0x43;

	phistogram->histogram_config__low_amb_odd_bin_0_1         = 0x10;
	phistogram->histogram_config__low_amb_odd_bin_2_3         = 0x32;
	phistogram->histogram_config__low_amb_odd_bin_4_5         = 0x54;

	phistogram->histogram_config__mid_amb_even_bin_0_1        = 0x07;
	phistogram->histogram_config__mid_amb_even_bin_2_3        = 0x21;
	phistogram->histogram_config__mid_amb_even_bin_4_5        = 0x43;

	phistogram->histogram_config__mid_amb_odd_bin_0_1         = 0x10;
	phistogram->histogram_config__mid_amb_odd_bin_2           = 0x02;
	phistogram->histogram_config__mid_amb_odd_bin_3_4         = 0x43;
	phistogram->histogram_config__mid_amb_odd_bin_5           = 0x05;

	phistogram->histogram_config__user_bin_offset             = 0x00;

	phistogram->histogram_config__high_amb_even_bin_0_1       = 0x07;
	phistogram->histogram_config__high_amb_even_bin_2_3       = 0x21;
	phistogram->histogram_config__high_amb_even_bin_4_5       = 0x43;

	phistogram->histogram_config__high_amb_odd_bin_0_1        = 0x10;
	phistogram->histogram_config__high_amb_odd_bin_2_3        = 0x32;
	phistogram->histogram_config__high_amb_odd_bin_4_5        = 0x54;

	phistogram->histogram_config__amb_thresh_low              = 0xFFFF;
	phistogram->histogram_config__amb_thresh_high             = 0xFFFF;

	phistogram->histogram_config__spad_array_selection        = 0x00;








	pzone_cfg->max_zones                     = VL53L1_MAX_USER_ZONES;
	pzone_cfg->active_zones                                   = 0x00;
	pzone_cfg->user_zones[0].height                           = 0x0f;
	pzone_cfg->user_zones[0].width                            = 0x0f;
	pzone_cfg->user_zones[0].x_centre                         = 0x08;
	pzone_cfg->user_zones[0].y_centre                         = 0x08;




	pdynamic->system__grouped_parameter_hold_0                 = 0x01;

	pdynamic->system__thresh_high                              = 0x0000;
	pdynamic->system__thresh_low                               = 0x0000;
	pdynamic->system__enable_xtalk_per_quadrant                = 0x00;
	pdynamic->system__seed_config =
			VL53L1_SYSTEM__SEED_CONFIG__EVEN_UPDATE_ONLY;


	pdynamic->sd_config__woi_sd0                               = 0x0B;


	pdynamic->sd_config__woi_sd1                               = 0x09;
	pdynamic->sd_config__initial_phase_sd0                     = 0x0A;
	pdynamic->sd_config__initial_phase_sd1                     = 0x0A;

	pdynamic->system__grouped_parameter_hold_1                 = 0x01;




























	pdynamic->sd_config__first_order_select = 0x00;
	pdynamic->sd_config__quantifier         = 0x02;






	pdynamic->roi_config__user_roi_centre_spad              = 0xC7;


	pdynamic->roi_config__user_roi_requested_global_xy_size = 0xFF;


	pdynamic->system__sequence_config                          = \
			VL53L1_SEQUENCE_VHV_EN | \
			VL53L1_SEQUENCE_PHASECAL_EN | \
			VL53L1_SEQUENCE_DSS1_EN | \
			VL53L1_SEQUENCE_DSS2_EN | \
			VL53L1_SEQUENCE_MM2_EN | \
			VL53L1_SEQUENCE_RANGE_EN;

	pdynamic->system__grouped_parameter_hold                   = 0x02;





	psystem->system__stream_count_ctrl                         = 0x00;
	psystem->firmware__enable                                  = 0x01;
	psystem->system__interrupt_clear                           = \
			VL53L1_CLEAR_RANGE_INT;

	psystem->system__mode_start                                = \
			VL53L1_DEVICESCHEDULERMODE_STREAMING | \
			VL53L1_DEVICEREADOUTMODE_SINGLE_SD | \
			VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_standard_ranging_short_range(
	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{











	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	status = VL53L1_preset_mode_standard_ranging(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {












		ptiming->range_config__vcsel_period_a                = 0x07;
		ptiming->range_config__vcsel_period_b                = 0x05;
		ptiming->range_config__sigma_thresh                  = 0x0050;
		ptiming->range_config__min_count_rate_rtn_limit_mcps = 0x0020;
		ptiming->range_config__valid_phase_low               = 0x08;
		ptiming->range_config__valid_phase_high              = 0x38;







		pdynamic->sd_config__woi_sd0                         = 0x07;
		pdynamic->sd_config__woi_sd1                         = 0x05;
		pdynamic->sd_config__initial_phase_sd0               = 0x06;
		pdynamic->sd_config__initial_phase_sd1               = 0x06;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_standard_ranging_long_range(
	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{











	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	status = VL53L1_preset_mode_standard_ranging(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {












		ptiming->range_config__vcsel_period_a                = 0x0F;
		ptiming->range_config__vcsel_period_b                = 0x0D;
		ptiming->range_config__sigma_thresh                  = 0x0050;
		ptiming->range_config__min_count_rate_rtn_limit_mcps = 0x0020;
		ptiming->range_config__valid_phase_low               = 0x08;
		ptiming->range_config__valid_phase_high              = 0xB8;







		pdynamic->sd_config__woi_sd0                         = 0x0F;
		pdynamic->sd_config__woi_sd1                         = 0x0D;
		pdynamic->sd_config__initial_phase_sd0               = 0x0E;
		pdynamic->sd_config__initial_phase_sd1               = 0x0E;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_standard_ranging_mm1_cal(
	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{










	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	status = VL53L1_preset_mode_standard_ranging(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {

		pgeneral->dss_config__roi_mode_control =
				VL53L1_DEVICEDSSMODE__REQUESTED_EFFFECTIVE_SPADS;

		pdynamic->system__sequence_config  = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_standard_ranging_mm2_cal(
	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{










	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	status = VL53L1_preset_mode_standard_ranging(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {

		pgeneral->dss_config__roi_mode_control =
				VL53L1_DEVICEDSSMODE__REQUESTED_EFFFECTIVE_SPADS;

		pdynamic->system__sequence_config  = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_timed_ranging(

	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status = VL53L1_preset_mode_standard_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pdynamic->system__grouped_parameter_hold = 0x00;





		ptiming->system__intermeasurement_period = 0x00000600;
		pdynamic->system__seed_config =
				VL53L1_SYSTEM__SEED_CONFIG__STANDARD;






		psystem->system__mode_start =
				VL53L1_DEVICESCHEDULERMODE_PSEUDO_SOLO | \
				VL53L1_DEVICEREADOUTMODE_SINGLE_SD     | \
				VL53L1_DEVICEMEASUREMENTMODE_TIMED;
	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_preset_mode_timed_ranging_short_range(

	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status = VL53L1_preset_mode_standard_ranging_short_range(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pdynamic->system__grouped_parameter_hold = 0x00;





		ptiming->system__intermeasurement_period = 0x00000600;
		pdynamic->system__seed_config =
				VL53L1_SYSTEM__SEED_CONFIG__STANDARD;






		psystem->system__mode_start =
				VL53L1_DEVICESCHEDULERMODE_PSEUDO_SOLO | \
				VL53L1_DEVICEREADOUTMODE_SINGLE_SD     | \
				VL53L1_DEVICEMEASUREMENTMODE_TIMED;
	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_preset_mode_timed_ranging_long_range(

	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status = VL53L1_preset_mode_standard_ranging_long_range(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pdynamic->system__grouped_parameter_hold = 0x00;





		ptiming->system__intermeasurement_period = 0x00000600;
		pdynamic->system__seed_config =
				VL53L1_SYSTEM__SEED_CONFIG__STANDARD;






		psystem->system__mode_start =
				VL53L1_DEVICESCHEDULERMODE_PSEUDO_SOLO | \
				VL53L1_DEVICEREADOUTMODE_SINGLE_SD     | \
				VL53L1_DEVICEMEASUREMENTMODE_TIMED;
	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_preset_mode_singleshot_ranging(

	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status = VL53L1_preset_mode_standard_ranging(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pdynamic->system__grouped_parameter_hold = 0x00;




		pdynamic->system__seed_config =
				VL53L1_SYSTEM__SEED_CONFIG__STANDARD;






		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_PSEUDO_SOLO | \
				VL53L1_DEVICEREADOUTMODE_SINGLE_SD     | \
				VL53L1_DEVICEMEASUREMENTMODE_SINGLESHOT;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_ranging(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_standard_ranging(
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pstatic->dss_config__target_total_rate_mcps           = 0x1400;





























		VL53L1_init_histogram_config_structure(
				7, 0, 1, 2, 3, 4,
				0, 1, 2, 3, 4, 5,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				7, 0, 1, 2, 3, 4,
				0, 1, 2, 3, 4, 5,
				&(pzone_cfg->multizone_hist_cfg));










		ptiming->range_config__vcsel_period_a                   = 0x09;
		ptiming->range_config__vcsel_period_b                   = 0x0B;
		pdynamic->sd_config__woi_sd0                            = 0x09;
		pdynamic->sd_config__woi_sd1                            = 0x0B;








		phistpostprocess->valid_phase_low  = 0x08;
		phistpostprocess->valid_phase_high = 0x88;




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);










		pgeneral->phasecal_config__timeout_macrop               = 0x40;




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \




				VL53L1_SEQUENCE_RANGE_EN;





		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_ranging_with_mm1(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {





























		VL53L1_init_histogram_config_structure(
				  7,   0,   1, 2, 3, 4,
				8+0, 8+1, 8+2, 3, 4, 5,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				  7,  0,    1, 2, 3, 4,
				8+0, 8+1, 8+2, 3, 4, 5,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN | \
				VL53L1_SEQUENCE_RANGE_EN;




		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_ranging_with_mm2(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging_with_mm1(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_ranging_mm1_cal(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {
















		VL53L1_init_histogram_config_structure(
				  7, 8+0, 8+1, 8+2, 8+3, 8+4,
				8+0, 8+1, 8+2, 8+3, 8+4, 8+5,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				  7, 8+0, 8+1, 8+2, 8+3, 8+4,
				8+0, 8+1, 8+2, 8+3, 8+4, 8+5,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pgeneral->dss_config__roi_mode_control =
				VL53L1_DEVICEDSSMODE__REQUESTED_EFFFECTIVE_SPADS;




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN | \
				VL53L1_SEQUENCE_RANGE_EN;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_ranging_mm2_cal(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging_mm1_cal(
				phistpostprocess,
				pstatic,
				phistogram,
				pgeneral,
				ptiming,
				pdynamic,
				psystem,
				pzone_cfg);

	if (status == VL53L1_ERROR_NONE) {




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_ranging_short_timing(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pstatic->dss_config__target_total_rate_mcps           = 0x1400;




		VL53L1_init_histogram_config_structure(
				7, 0, 1, 2, 3, 4,
				7, 0, 1, 2, 3, 4,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				7, 0, 1, 2, 3, 4,
				7, 0, 1, 2, 3, 4,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->range_config__vcsel_period_a             = 0x04;
		ptiming->range_config__vcsel_period_b             = 0x03;
		ptiming->mm_config__timeout_macrop_a_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_a_lo           = 0x42;
		ptiming->mm_config__timeout_macrop_b_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_b_lo           = 0x42;
		ptiming->range_config__timeout_macrop_a_hi        = 0x00;
		ptiming->range_config__timeout_macrop_a_lo        = 0x52;
		ptiming->range_config__timeout_macrop_b_hi        = 0x00;
		ptiming->range_config__timeout_macrop_b_lo        = 0x66;

		pgeneral->cal_config__vcsel_start                 = 0x04;










		pgeneral->phasecal_config__timeout_macrop         = 0xa4;




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \




				VL53L1_SEQUENCE_RANGE_EN;





		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_long_range(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_init_histogram_config_structure(
			7, 0, 1, 2, 3, 4,
			0, 1, 2, 3, 4, 5,
			phistogram);






		VL53L1_init_histogram_multizone_config_structure(
			7, 0, 1, 2, 3, 4,

			0, 1, 2, 3, 4, 5,

			&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
			phistogram,
			pstatic,
			pgeneral,
			ptiming,
			pdynamic);






		ptiming->range_config__vcsel_period_a             = 0x09;
		ptiming->range_config__vcsel_period_b             = 0x0b;






		ptiming->mm_config__timeout_macrop_a_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_a_lo           = 0x21;
		ptiming->mm_config__timeout_macrop_b_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_b_lo           = 0x1b;




		ptiming->range_config__timeout_macrop_a_hi        = 0x00;
		ptiming->range_config__timeout_macrop_a_lo        = 0x29;
		ptiming->range_config__timeout_macrop_b_hi        = 0x00;
		ptiming->range_config__timeout_macrop_b_lo        = 0x22;




		pgeneral->cal_config__vcsel_start                 = 0x09;













		pgeneral->phasecal_config__timeout_macrop         = 0x42;




		pdynamic->sd_config__woi_sd0                      = 0x09;
		pdynamic->sd_config__woi_sd1                      = 0x0B;
		pdynamic->sd_config__initial_phase_sd0            = 0x09;
		pdynamic->sd_config__initial_phase_sd1            = 0x09;








		phistpostprocess->valid_phase_low  = 0x08;
		phistpostprocess->valid_phase_high = 0x88;

		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;





		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_long_range_mm1(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_long_range(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_init_histogram_config_structure(
				  7,   0,   1, 2, 3, 4,
				8+0, 8+1, 8+2, 3, 4, 5,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				  7,   0,   1, 2, 3, 4,
				8+0, 8+1, 8+2, 3, 4, 5,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN | \
				VL53L1_SEQUENCE_RANGE_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_long_range_mm2(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_long_range_mm1(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_preset_mode_histogram_medium_range(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_init_histogram_config_structure(
				7, 0, 1, 1, 2, 2,
				0, 1, 2, 1, 2, 3,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				7, 0, 1, 2, 3, 4,
				0, 1, 2, 1, 2, 3,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->range_config__vcsel_period_a             = 0x05;
		ptiming->range_config__vcsel_period_b             = 0x07;






		ptiming->mm_config__timeout_macrop_a_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_a_lo           = 0x37;
		ptiming->mm_config__timeout_macrop_b_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_b_lo           = 0x29;




		ptiming->range_config__timeout_macrop_a_hi        = 0x00;
		ptiming->range_config__timeout_macrop_a_lo        = 0x44;
		ptiming->range_config__timeout_macrop_b_hi        = 0x00;
		ptiming->range_config__timeout_macrop_b_lo        = 0x33;




		pgeneral->cal_config__vcsel_start                 = 0x05;













		pgeneral->phasecal_config__timeout_macrop         = 0x6D;




		pdynamic->sd_config__woi_sd0                      = 0x05;
		pdynamic->sd_config__woi_sd1                      = 0x07;
		pdynamic->sd_config__initial_phase_sd0            = 0x05;
		pdynamic->sd_config__initial_phase_sd1            = 0x05;








		phistpostprocess->valid_phase_low  = 0x08;
		phistpostprocess->valid_phase_high = 0x48;

		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;





		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_medium_range_mm1(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_medium_range(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {




		VL53L1_init_histogram_config_structure(
				  7,   0,   1, 1, 2, 2,
				8+0, 8+1, 8+2, 1, 2, 3,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				  7,   0,   1, 1, 2, 2,
				8+0, 8+1, 8+2, 1, 2, 3,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN  | \
				VL53L1_SEQUENCE_RANGE_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_medium_range_mm2(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_medium_range_mm1(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_short_range(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_init_histogram_config_structure(
				7, 7, 0, 1, 1, 1,
				0, 1, 1, 1, 2, 2,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				7, 7, 0, 1, 1, 1,
				0, 1, 1, 1, 2, 2,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->range_config__vcsel_period_a             = 0x03;
		ptiming->range_config__vcsel_period_b             = 0x05;






		ptiming->mm_config__timeout_macrop_a_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_a_lo           = 0x52;
		ptiming->mm_config__timeout_macrop_b_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_b_lo           = 0x37;




		ptiming->range_config__timeout_macrop_a_hi        = 0x00;
		ptiming->range_config__timeout_macrop_a_lo        = 0x66;
		ptiming->range_config__timeout_macrop_b_hi        = 0x00;
		ptiming->range_config__timeout_macrop_b_lo        = 0x44;




		pgeneral->cal_config__vcsel_start                 = 0x03;













		pgeneral->phasecal_config__timeout_macrop         = 0xA4;




		pdynamic->sd_config__woi_sd0                      = 0x03;
		pdynamic->sd_config__woi_sd1                      = 0x05;
		pdynamic->sd_config__initial_phase_sd0            = 0x03;
		pdynamic->sd_config__initial_phase_sd1            = 0x03;








		phistpostprocess->valid_phase_low  = 0x08;
		phistpostprocess->valid_phase_high = 0x28;

		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN  | \


				VL53L1_SEQUENCE_RANGE_EN;





		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_short_range_mm1(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{














	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_short_range(
			phistpostprocess,
				pstatic,
				phistogram,
				pgeneral,
				ptiming,
				pdynamic,
				psystem,
				pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_init_histogram_config_structure(
				  7,   7, 0, 1, 1, 1,
				8+0, 8+1, 1, 1, 2, 2,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				  7,   0, 1, 1, 1, 1,
				8+0, 8+1, 1, 1, 2, 2,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN  | \
				VL53L1_SEQUENCE_RANGE_EN;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_short_range_mm2(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_short_range_mm1(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;
	}

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_preset_mode_histogram_characterisation(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pstatic->debug__ctrl                            = 0x01;
		psystem->power_management__go1_power_force      = 0x01;

		pdynamic->system__sequence_config               = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_RANGE_EN;

		psystem->system__mode_start                     = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM    | \
				VL53L1_DEVICEREADOUTMODE_SPLIT_MANUAL   | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_xtalk_planar(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_multizone_long_range(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		status =
			VL53L1_zone_preset_xtalk_planar(
				pgeneral,
				pzone_cfg);










		ptiming->range_config__sigma_thresh                  = 0x0000;


		ptiming->range_config__min_count_rate_rtn_limit_mcps = 0x0000;







		ptiming->range_config__vcsel_period_a                   = 0x09;
		ptiming->range_config__vcsel_period_b                   = 0x09;




		VL53L1_init_histogram_config_structure(
			7, 0, 1, 2, 3, 4,
			7, 0, 1, 2, 3, 4,
			phistogram);







		VL53L1_init_histogram_multizone_config_structure(
			7, 0, 1, 2, 3, 4,

			7, 0, 1, 2, 3, 4,

			&(pzone_cfg->multizone_hist_cfg));











		if (status == VL53L1_ERROR_NONE) {
			status = VL53L1_set_histogram_multizone_initial_bin_config(
			pzone_cfg,
			phistogram,
			&(pzone_cfg->multizone_hist_cfg));
		}






		VL53L1_copy_hist_cfg_to_static_cfg(
			phistogram,
			pstatic,
			pgeneral,
			ptiming,
			pdynamic);

	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_preset_mode_histogram_xtalk_mm1(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);










	ptiming->range_config__sigma_thresh                  = 0x0000;


	ptiming->range_config__min_count_rate_rtn_limit_mcps = 0x0000;












	if (status == VL53L1_ERROR_NONE) {









		VL53L1_init_histogram_config_structure(
				8+7, 8+0, 8+1, 8+2, 8+3, 8+4,
				8+7, 8+0, 8+1, 8+2, 8+3, 8+4,
				phistogram);






		VL53L1_init_histogram_multizone_config_structure(
				8+7, 8+0, 8+1, 8+2, 8+3, 8+4,
				8+7, 8+0, 8+1, 8+2, 8+3, 8+4,
				&(pzone_cfg->multizone_hist_cfg));




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->range_config__vcsel_period_a             = 0x09;
		ptiming->range_config__vcsel_period_b             = 0x09;






		ptiming->mm_config__timeout_macrop_a_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_a_lo           = 0x21;
		ptiming->mm_config__timeout_macrop_b_hi           = 0x00;
		ptiming->mm_config__timeout_macrop_b_lo           = 0x21;




		ptiming->range_config__timeout_macrop_a_hi        = 0x00;
		ptiming->range_config__timeout_macrop_a_lo        = 0x29;
		ptiming->range_config__timeout_macrop_b_hi        = 0x00;
		ptiming->range_config__timeout_macrop_b_lo        = 0x29;




		pgeneral->cal_config__vcsel_start                 = 0x09;













		pgeneral->phasecal_config__timeout_macrop         = 0x42;




		pdynamic->sd_config__woi_sd0                      = 0x09;
		pdynamic->sd_config__woi_sd1                      = 0x09;
		pdynamic->sd_config__initial_phase_sd0            = 0x09;
		pdynamic->sd_config__initial_phase_sd1            = 0x09;

		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM1_EN | \
				VL53L1_SEQUENCE_RANGE_EN;





		psystem->system__mode_start = \
				VL53L1_DEVICESCHEDULERMODE_HISTOGRAM | \
				VL53L1_DEVICEREADOUTMODE_DUAL_SD | \
				VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_xtalk_mm2(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_xtalk_mm1(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);


		pdynamic->system__sequence_config = \
				VL53L1_SEQUENCE_VHV_EN | \
				VL53L1_SEQUENCE_PHASECAL_EN | \
				VL53L1_SEQUENCE_DSS1_EN | \
				VL53L1_SEQUENCE_DSS2_EN | \
				VL53L1_SEQUENCE_MM2_EN | \
				VL53L1_SEQUENCE_RANGE_EN;



	LOG_FUNCTION_END(status);

	return status;
}




VL53L1_Error VL53L1_preset_mode_histogram_multizone(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_medium_range(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		status =
			VL53L1_init_zone_config_structure(
				4, 8, 2,

				4, 8, 2,

				7, 7,

				pzone_cfg);

		pgeneral->global_config__stream_divider =
			pzone_cfg->active_zones + 1;






		if (status == VL53L1_ERROR_NONE) {
			status = VL53L1_set_histogram_multizone_initial_bin_config (
				pzone_cfg,
				phistogram,
				&(pzone_cfg->multizone_hist_cfg));
		}

		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);
	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_preset_mode_histogram_multizone_short_range(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_short_range(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		status =
			VL53L1_init_zone_config_structure(
				4, 8, 2,

				4, 8, 2,

				7, 7,

				pzone_cfg);

		pgeneral->global_config__stream_divider =
			pzone_cfg->active_zones + 1;






		if (status == VL53L1_ERROR_NONE) {
			status = VL53L1_set_histogram_multizone_initial_bin_config (
			pzone_cfg,
			phistogram,
			&(pzone_cfg->multizone_hist_cfg)
			);
		}




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_preset_mode_histogram_multizone_long_range(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_long_range(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		status =
			VL53L1_init_zone_config_structure(
				4, 8, 2,

				4, 8, 2,

				7, 7,

				pzone_cfg);

		pgeneral->global_config__stream_divider =
			pzone_cfg->active_zones + 1;






		if (status == VL53L1_ERROR_NONE) {
			status = VL53L1_set_histogram_multizone_initial_bin_config (
				pzone_cfg,
				phistogram,
				&(pzone_cfg->multizone_hist_cfg));
		}




		VL53L1_copy_hist_cfg_to_static_cfg(
			phistogram,
			pstatic,
			pgeneral,
			ptiming,
			pdynamic);
	}

	LOG_FUNCTION_END(status);

	return status;
}




VL53L1_Error VL53L1_preset_mode_olt(
	VL53L1_static_config_t    *pstatic,
	VL53L1_histogram_config_t *phistogram,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic,
	VL53L1_system_control_t   *psystem,
	VL53L1_zone_config_t      *pzone_cfg)
{








	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status = VL53L1_preset_mode_standard_ranging(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {



		psystem->system__stream_count_ctrl  = 0x01;
	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_copy_hist_cfg_to_static_cfg(
	VL53L1_histogram_config_t *phistogram,
	VL53L1_static_config_t    *pstatic,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic)
{






	LOG_FUNCTION_START("");

	SUPPRESS_UNUSED_WARNING(pgeneral);

	pstatic->sigma_estimator__effective_pulse_width_ns =
			phistogram->histogram_config__high_amb_even_bin_0_1;
	pstatic->sigma_estimator__effective_ambient_width_ns =
			phistogram->histogram_config__high_amb_even_bin_2_3;
	pstatic->sigma_estimator__sigma_ref_mm =
			phistogram->histogram_config__high_amb_even_bin_4_5;

	pstatic->algo__crosstalk_compensation_valid_height_mm =
			phistogram->histogram_config__high_amb_odd_bin_0_1;

	pstatic->spare_host_config__static_config_spare_0 =
			phistogram->histogram_config__high_amb_odd_bin_2_3;
	pstatic->spare_host_config__static_config_spare_1 =
			phistogram->histogram_config__high_amb_odd_bin_4_5;

	pstatic->algo__range_ignore_threshold_mcps =
		(((uint16_t)phistogram->histogram_config__mid_amb_even_bin_0_1) << 8)
		+ (uint16_t)phistogram->histogram_config__mid_amb_even_bin_2_3;

	pstatic->algo__range_ignore_valid_height_mm =
			phistogram->histogram_config__mid_amb_even_bin_4_5;
	pstatic->algo__range_min_clip =
			phistogram->histogram_config__mid_amb_odd_bin_0_1;
	pstatic->algo__consistency_check__tolerance =
			phistogram->histogram_config__mid_amb_odd_bin_2;

	pstatic->spare_host_config__static_config_spare_2 =
			phistogram->histogram_config__mid_amb_odd_bin_3_4;
	pstatic->sd_config__reset_stages_msb =
			phistogram->histogram_config__mid_amb_odd_bin_5;

	pstatic->sd_config__reset_stages_lsb =
			phistogram->histogram_config__user_bin_offset;

	ptiming->range_config__sigma_thresh =
		(((uint16_t)phistogram->histogram_config__low_amb_even_bin_0_1) << 8)
		+ (uint16_t)phistogram->histogram_config__low_amb_even_bin_2_3;

	ptiming->range_config__min_count_rate_rtn_limit_mcps =
		(((uint16_t)phistogram->histogram_config__low_amb_even_bin_4_5) << 8)
		+ (uint16_t)phistogram->histogram_config__low_amb_odd_bin_0_1;

	ptiming->range_config__valid_phase_low =
			phistogram->histogram_config__low_amb_odd_bin_2_3;
	ptiming->range_config__valid_phase_high =
			phistogram->histogram_config__low_amb_odd_bin_4_5;

	pdynamic->system__thresh_high =
			phistogram->histogram_config__amb_thresh_low;

	pdynamic->system__thresh_low =
			phistogram->histogram_config__amb_thresh_high;

	pdynamic->system__enable_xtalk_per_quadrant =
			phistogram->histogram_config__spad_array_selection;

	LOG_FUNCTION_END(0);

}

void VL53L1_copy_hist_bins_to_static_cfg(
	VL53L1_histogram_config_t *phistogram,
	VL53L1_static_config_t    *pstatic,
	VL53L1_timing_config_t    *ptiming
  )
{






	LOG_FUNCTION_START("");

	pstatic->sigma_estimator__effective_pulse_width_ns =
			phistogram->histogram_config__high_amb_even_bin_0_1;
	pstatic->sigma_estimator__effective_ambient_width_ns =
			phistogram->histogram_config__high_amb_even_bin_2_3;
	pstatic->sigma_estimator__sigma_ref_mm =
			phistogram->histogram_config__high_amb_even_bin_4_5;

	pstatic->algo__crosstalk_compensation_valid_height_mm =
			phistogram->histogram_config__high_amb_odd_bin_0_1;

	pstatic->spare_host_config__static_config_spare_0 =
			phistogram->histogram_config__high_amb_odd_bin_2_3;
	pstatic->spare_host_config__static_config_spare_1 =
			phistogram->histogram_config__high_amb_odd_bin_4_5;

	pstatic->algo__range_ignore_threshold_mcps =
		(((uint16_t)phistogram->histogram_config__mid_amb_even_bin_0_1) << 8)
		+ (uint16_t)phistogram->histogram_config__mid_amb_even_bin_2_3;

	pstatic->algo__range_ignore_valid_height_mm =
			phistogram->histogram_config__mid_amb_even_bin_4_5;
	pstatic->algo__range_min_clip =
			phistogram->histogram_config__mid_amb_odd_bin_0_1;
	pstatic->algo__consistency_check__tolerance =
			phistogram->histogram_config__mid_amb_odd_bin_2;

	pstatic->spare_host_config__static_config_spare_2 =
			phistogram->histogram_config__mid_amb_odd_bin_3_4;
	pstatic->sd_config__reset_stages_msb =
			phistogram->histogram_config__mid_amb_odd_bin_5;

	ptiming->range_config__sigma_thresh =
		(((uint16_t)phistogram->histogram_config__low_amb_even_bin_0_1) << 8)
		+ (uint16_t)phistogram->histogram_config__low_amb_even_bin_2_3;

	ptiming->range_config__min_count_rate_rtn_limit_mcps =
		(((uint16_t)phistogram->histogram_config__low_amb_even_bin_4_5) << 8)
		+ (uint16_t)phistogram->histogram_config__low_amb_odd_bin_0_1;

	ptiming->range_config__valid_phase_low =
			phistogram->histogram_config__low_amb_odd_bin_2_3;
	ptiming->range_config__valid_phase_high =
			phistogram->histogram_config__low_amb_odd_bin_4_5;

	LOG_FUNCTION_END(0);

}


VL53L1_Error VL53L1_preset_mode_histogram_ranging_ref(
	VL53L1_hist_post_process_config_t  *phistpostprocess,
	VL53L1_static_config_t             *pstatic,
	VL53L1_histogram_config_t          *phistogram,
	VL53L1_general_config_t            *pgeneral,
	VL53L1_timing_config_t             *ptiming,
	VL53L1_dynamic_config_t            *pdynamic,
	VL53L1_system_control_t            *psystem,
	VL53L1_zone_config_t               *pzone_cfg)
{












	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	status =
		VL53L1_preset_mode_histogram_ranging(
			phistpostprocess,
			pstatic,
			phistogram,
			pgeneral,
			ptiming,
			pdynamic,
			psystem,
			pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		phistogram->histogram_config__spad_array_selection = 0x00;




		VL53L1_copy_hist_cfg_to_static_cfg(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);
	}

	LOG_FUNCTION_END(status);

	return status;
}





