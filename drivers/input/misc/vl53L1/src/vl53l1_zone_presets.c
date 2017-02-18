
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
#include "vl53l1_ll_device.h"
#include "vl53l1_platform_log.h"
#include "vl53l1_zone_presets.h"


#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, status, fmt, ##__VA_ARGS__)


VL53L1_Error VL53L1_init_zone_config_structure(
	uint8_t x_off,
	uint8_t x_inc,
	uint8_t x_zones,
	uint8_t y_off,
	uint8_t y_inc,
	uint8_t y_zones,
	uint8_t width,
	uint8_t height,
	VL53L1_zone_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	uint8_t  x  = 0;
	uint8_t  y  = 0;
	uint16_t  i  = 0;

	LOG_FUNCTION_START("");

	pdata->max_zones = VL53L1_MAX_USER_ZONES;

	i = 0;

	for (x = 0 ; x < x_zones ; x++) {
		for (y = 0 ; y <  y_zones ; y++) {

			if (i < VL53L1_MAX_USER_ZONES) {

				pdata->active_zones = (uint8_t)i;
				pdata->user_zones[i].height   = height;
				pdata->user_zones[i].width    = width;
				pdata->user_zones[i].x_centre = x_off + (x * x_inc);
				pdata->user_zones[i].y_centre = y_off + (y * y_inc);
			}

			i++;
		}
	}

	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_init_zone_config_histogram_bins(pdata);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_zone_preset_xtalk_planar(
	VL53L1_general_config_t	*pgeneral,
	VL53L1_zone_config_t    *pzone_cfg)
{






	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");






	pgeneral->global_config__stream_divider = 0x05;



	pzone_cfg->active_zones                 = 0x04;

	pzone_cfg->user_zones[0].height         = 15;
	pzone_cfg->user_zones[0].width          = 7;
	pzone_cfg->user_zones[0].x_centre       = 4;
	pzone_cfg->user_zones[0].y_centre       = 8;

	pzone_cfg->user_zones[1].height         = 15;
	pzone_cfg->user_zones[1].width          = 7;
	pzone_cfg->user_zones[1].x_centre       = 12;
	pzone_cfg->user_zones[1].y_centre       = 8;

	pzone_cfg->user_zones[2].height         = 7;
	pzone_cfg->user_zones[2].width          = 15;
	pzone_cfg->user_zones[2].x_centre       = 8;
	pzone_cfg->user_zones[2].y_centre       = 4;

	pzone_cfg->user_zones[3].height         = 7;
	pzone_cfg->user_zones[3].width          = 15;
	pzone_cfg->user_zones[3].x_centre       = 8;
	pzone_cfg->user_zones[3].y_centre       = 12;




	pzone_cfg->user_zones[4].height         = 15;
	pzone_cfg->user_zones[4].width          = 15;
	pzone_cfg->user_zones[4].x_centre       = 8;
	pzone_cfg->user_zones[4].y_centre       = 8;

	if (status == VL53L1_ERROR_NONE) {

		status = VL53L1_init_zone_config_histogram_bins(pzone_cfg);
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_init_zone_config_histogram_bins(
	VL53L1_zone_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	uint8_t i;

	LOG_FUNCTION_START("");

	for (i = 0; i < pdata->max_zones; i++)
		pdata->bin_config[i] = VL53L1_ZONECONFIG_BINCONFIG__LOWAMB;

	LOG_FUNCTION_END(status);

	return status;
}

