
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L1 Core may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following provisions apply instead of the ones
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


VL53L1_Error VL53L1_FCTN_00040(
	VL53L1_refspadchar_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");











	pdata->VL53L1_PRM_00046 =
			VL53L1_DEF_00121;
	pdata->VL53L1_PRM_00041              = 0x0B;
	pdata->VL53L1_PRM_00042                = 1000;
	pdata->VL53L1_PRM_00043    = 0x0A00;
	pdata->VL53L1_PRM_00045 = 0x0500;
	pdata->VL53L1_PRM_00044 = 0x1400;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00041(
	VL53L1_hist_post_process_config_t   *pdata)
{





	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");




	pdata->VL53L1_PRM_00189 =
		VL53L1_DEF_00122;




	pdata->VL53L1_PRM_00191                   =   1;

	pdata->VL53L1_PRM_00192                   =   2;


	pdata->VL53L1_PRM_00193 =
		VL53L1_DEF_00123;
	pdata->VL53L1_PRM_00194         =  80;

	pdata->VL53L1_PRM_00195         = 112;

	pdata->VL53L1_PRM_00196     =  16;


	pdata->VL53L1_PRM_00197               =  50;


	pdata->VL53L1_PRM_00198     = 100;

	pdata->VL53L1_PRM_00200 =   1;

	pdata->VL53L1_PRM_00201                  = 128;


	pdata->VL53L1_PRM_00202               =   0;










	pdata->VL53L1_PRM_00203 = 8;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00054(
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






	pstatic->VL53L1_PRM_00204               = 0x0A00;
	pstatic->VL53L1_PRM_00205                                      = 0x00;
	pstatic->VL53L1_PRM_00206                                  = 0x00;
	pstatic->VL53L1_PRM_00207                                 = 0x00;
	pstatic->VL53L1_PRM_00208                                   = 0x00;
	pstatic->VL53L1_PRM_00209                          = 0x00;
	pstatic->VL53L1_PRM_00210                          = 0x00;
	pstatic->VL53L1_PRM_00211                                  = 0x00;
	pstatic->VL53L1_PRM_00212                               = 0x00;
	pstatic->VL53L1_PRM_00213                        = 0x00;
	pstatic->VL53L1_PRM_00214                                = 0x00;


	pstatic->VL53L1_PRM_00103                                = 0x11;
	pstatic->VL53L1_PRM_00215                              = 0x02;
	pstatic->VL53L1_PRM_00216                              = 0x00;
	pstatic->VL53L1_PRM_00217                     = 0x02;
	pstatic->VL53L1_PRM_00143             = 0x08;
	pstatic->VL53L1_PRM_00218                = 0x00;

	pstatic->VL53L1_PRM_00219        = 0x08;
	pstatic->VL53L1_PRM_00220      = 0x10;
	pstatic->VL53L1_PRM_00200                    = 0x01;
	pstatic->VL53L1_PRM_00221     = 0x00;
	pstatic->VL53L1_PRM_00222         = 0x00;
	pstatic->VL53L1_PRM_00223         = 0x00;
	pstatic->VL53L1_PRM_00224                = 0x0000;



	pstatic->VL53L1_PRM_00225               = 0x14;
	pstatic->VL53L1_PRM_00226                             = 0x00;






	pstatic->VL53L1_PRM_00203               = 0x02;
	pstatic->VL53L1_PRM_00227         = 0x00;
	pstatic->VL53L1_PRM_00228                      = 0x00;
	pstatic->VL53L1_PRM_00229                      = 0x00;

	pgeneral->VL53L1_PRM_00230           = 0x00;
	pgeneral->VL53L1_PRM_00119                   = 0x00;
	pgeneral->VL53L1_PRM_00231 =
			VL53L1_DEF_00124;
	pgeneral->VL53L1_PRM_00140                         = 0x0B;
	pgeneral->VL53L1_PRM_00232                         = 0x0000;
	pgeneral->VL53L1_PRM_00142                      = 0x02;


	pgeneral->VL53L1_PRM_00100                 = 0x0D;


	pgeneral->VL53L1_PRM_00233                         = 0x21;
	pgeneral->VL53L1_PRM_00234                       = 0x00;
	pgeneral->VL53L1_PRM_00235                    = 0x01;


	pgeneral->VL53L1_PRM_00236                        = 0x0000;
	pgeneral->VL53L1_PRM_00237                         = 0x0000;


	pgeneral->VL53L1_PRM_00238       = 0x8C00;
	pgeneral->VL53L1_PRM_00239                 = 0x00;
	pgeneral->VL53L1_PRM_00240                = 0x33;
	pgeneral->VL53L1_PRM_00241                     = 0xFF;
	pgeneral->VL53L1_PRM_00242                     = 0x01;




	ptiming->VL53L1_PRM_00243                   = 0x00;
	ptiming->VL53L1_PRM_00244                   = 0x06;
	ptiming->VL53L1_PRM_00245                   = 0x00;
	ptiming->VL53L1_PRM_00246                   = 0x06;
	ptiming->VL53L1_PRM_00146                = 0x02;
	ptiming->VL53L1_PRM_00147                = 0x85;


	ptiming->VL53L1_PRM_00101                     = 0x0B;
	ptiming->VL53L1_PRM_00148                = 0x01;
	ptiming->VL53L1_PRM_00149                = 0x92;


	ptiming->VL53L1_PRM_00150                     = 0x09;







	ptiming->VL53L1_PRM_00013                       = 0x003C;






	ptiming->VL53L1_PRM_00012      = 0x0080;






	ptiming->VL53L1_PRM_00247                    = 0x08;
	ptiming->VL53L1_PRM_00248                   = 0x78;
	ptiming->VL53L1_PRM_00114                  = 0x00000000;
	ptiming->VL53L1_PRM_00249                        = 0x00;




























	phistogram->VL53L1_PRM_00250        = 0x07;
	phistogram->VL53L1_PRM_00251        = 0x21;
	phistogram->VL53L1_PRM_00252        = 0x43;

	phistogram->VL53L1_PRM_00253         = 0x10;
	phistogram->VL53L1_PRM_00254         = 0x32;
	phistogram->VL53L1_PRM_00255         = 0x54;

	phistogram->VL53L1_PRM_00256        = 0x07;
	phistogram->VL53L1_PRM_00257        = 0x21;
	phistogram->VL53L1_PRM_00258        = 0x43;

	phistogram->VL53L1_PRM_00259         = 0x10;
	phistogram->VL53L1_PRM_00260           = 0x02;
	phistogram->VL53L1_PRM_00261         = 0x43;
	phistogram->VL53L1_PRM_00262           = 0x05;

	phistogram->VL53L1_PRM_00263             = 0x00;

	phistogram->VL53L1_PRM_00264       = 0x07;
	phistogram->VL53L1_PRM_00265       = 0x21;
	phistogram->VL53L1_PRM_00266       = 0x43;

	phistogram->VL53L1_PRM_00267        = 0x10;
	phistogram->VL53L1_PRM_00268        = 0x32;
	phistogram->VL53L1_PRM_00269        = 0x54;

	phistogram->VL53L1_PRM_00270              = 0xFFFF;
	phistogram->VL53L1_PRM_00271             = 0xFFFF;

	phistogram->VL53L1_PRM_00272        = 0x00;








	pzone_cfg->VL53L1_PRM_00019                     = VL53L1_MAX_USER_ZONES;
	pzone_cfg->VL53L1_PRM_00020                                   = 0x00;
	pzone_cfg->VL53L1_PRM_00021[0].VL53L1_PRM_00018                           = 0x0f;
	pzone_cfg->VL53L1_PRM_00021[0].VL53L1_PRM_00017                            = 0x0f;
	pzone_cfg->VL53L1_PRM_00021[0].VL53L1_PRM_00015                         = 0x08;
	pzone_cfg->VL53L1_PRM_00021[0].VL53L1_PRM_00016                         = 0x08;




	pdynamic->VL53L1_PRM_00125                 = 0x01;

	pdynamic->VL53L1_PRM_00273                              = 0x0000;
	pdynamic->VL53L1_PRM_00274                               = 0x0000;
	pdynamic->VL53L1_PRM_00275                = 0x00;
	pdynamic->VL53L1_PRM_00276 =
			VL53L1_DEF_00125;


	pdynamic->VL53L1_PRM_00277                               = 0x0B;


	pdynamic->VL53L1_PRM_00278                               = 0x09;
	pdynamic->VL53L1_PRM_00279                     = 0x0A;
	pdynamic->VL53L1_PRM_00280                     = 0x0A;

	pdynamic->VL53L1_PRM_00126                 = 0x01;




























	pdynamic->VL53L1_PRM_00281 = 0x00;
	pdynamic->VL53L1_PRM_00282         = 0x02;






	pdynamic->VL53L1_PRM_00115              = 0xC7;


	pdynamic->VL53L1_PRM_00116 = 0xFF;


	pdynamic->VL53L1_PRM_00023                          = \
			VL53L1_DEF_00126 | \
			VL53L1_DEF_00127 | \
			VL53L1_DEF_00128 | \
			VL53L1_DEF_00129 | \
			VL53L1_DEF_00130 | \
			VL53L1_DEF_00131;

	pdynamic->VL53L1_PRM_00127                   = 0x02;





	psystem->VL53L1_PRM_00283                         = 0x00;
	psystem->VL53L1_PRM_00284                                  = 0x01;
	psystem->VL53L1_PRM_00285                           = \
			VL53L1_DEF_00132;

	psystem->VL53L1_PRM_00122                                = \
			VL53L1_DEF_00133 | \
			VL53L1_DEF_00134 | \
			VL53L1_DEF_00002;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00055(
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






	status = VL53L1_FCTN_00054(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {












		ptiming->VL53L1_PRM_00101                = 0x07;
		ptiming->VL53L1_PRM_00150                = 0x05;
		ptiming->VL53L1_PRM_00013                  = 0x0050;
		ptiming->VL53L1_PRM_00012 = 0x0020;
		ptiming->VL53L1_PRM_00247               = 0x08;
		ptiming->VL53L1_PRM_00248              = 0x38;







		pdynamic->VL53L1_PRM_00277                         = 0x07;
		pdynamic->VL53L1_PRM_00278                         = 0x05;
		pdynamic->VL53L1_PRM_00279               = 0x06;
		pdynamic->VL53L1_PRM_00280               = 0x06;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00056(
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






	status = VL53L1_FCTN_00054(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {












		ptiming->VL53L1_PRM_00101                = 0x0F;
		ptiming->VL53L1_PRM_00150                = 0x0D;
		ptiming->VL53L1_PRM_00013                  = 0x0050;
		ptiming->VL53L1_PRM_00012 = 0x0020;
		ptiming->VL53L1_PRM_00247               = 0x08;
		ptiming->VL53L1_PRM_00248              = 0xB8;







		pdynamic->VL53L1_PRM_00277                         = 0x0F;
		pdynamic->VL53L1_PRM_00278                         = 0x0D;
		pdynamic->VL53L1_PRM_00279               = 0x0E;
		pdynamic->VL53L1_PRM_00280               = 0x0E;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00057(
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






	status = VL53L1_FCTN_00054(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {

		pdynamic->VL53L1_PRM_00023  = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00135;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00058(
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






	status = VL53L1_FCTN_00054(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {

		pdynamic->VL53L1_PRM_00023  = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00130;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00059(

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




	status = VL53L1_FCTN_00054(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pdynamic->VL53L1_PRM_00127 = 0x00;





		ptiming->VL53L1_PRM_00114 = 0x00000600;
		pdynamic->VL53L1_PRM_00276 =
				VL53L1_DEF_00136;






		psystem->VL53L1_PRM_00122 =
				VL53L1_DEF_00137 | \
				VL53L1_DEF_00134     | \
				VL53L1_DEF_00138;
	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_FCTN_00078(

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




	status = VL53L1_FCTN_00054(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pdynamic->VL53L1_PRM_00127 = 0x00;




		pdynamic->VL53L1_PRM_00276 =
				VL53L1_DEF_00136;






		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00137 | \
				VL53L1_DEF_00134     | \
				VL53L1_DEF_00139;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00060(
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




	status = VL53L1_FCTN_00054(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pstatic->VL53L1_PRM_00204           = 0x1400;





























		VL53L1_FCTN_00113(
				7, 0, 1, 2, 3, 4,
				0, 1, 2, 3, 4, 5,
				phistogram);









		ptiming->VL53L1_PRM_00101                   = 0x09;
		ptiming->VL53L1_PRM_00150                   = 0x0B;
		pdynamic->VL53L1_PRM_00277                            = 0x09;
		pdynamic->VL53L1_PRM_00278                            = 0x0B;




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);










		pgeneral->VL53L1_PRM_00100               = 0x40;




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \




				VL53L1_DEF_00131;





		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00140 | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00061(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {





























		VL53L1_FCTN_00113(
				  7,   0,   1, 2, 3, 4,
				8+0, 8+1, 8+2, 3, 4, 5,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00135 | \
				VL53L1_DEF_00131;




		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00140 | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00062(
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




	status = VL53L1_FCTN_00061(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00130 | \
				VL53L1_DEF_00131;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00065(
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




	status = VL53L1_FCTN_00054(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		pstatic->VL53L1_PRM_00204           = 0x1400;




		VL53L1_FCTN_00113(
				7, 0, 1, 2, 3, 4,
				7, 0, 1, 2, 3, 4,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->VL53L1_PRM_00101             = 0x04;
		ptiming->VL53L1_PRM_00150             = 0x03;
		ptiming->VL53L1_PRM_00243           = 0x00;
		ptiming->VL53L1_PRM_00244           = 0x42;
		ptiming->VL53L1_PRM_00245           = 0x00;
		ptiming->VL53L1_PRM_00246           = 0x42;
		ptiming->VL53L1_PRM_00146        = 0x00;
		ptiming->VL53L1_PRM_00147        = 0x52;
		ptiming->VL53L1_PRM_00148        = 0x00;
		ptiming->VL53L1_PRM_00149        = 0x66;

		pgeneral->VL53L1_PRM_00140                 = 0x04;










		pgeneral->VL53L1_PRM_00100         = 0xa4;




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \




				VL53L1_DEF_00131;





		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00140 | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00066(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_FCTN_00113(
				7, 0, 1, 2, 3, 4,
				0, 1, 2, 3, 4, 5,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->VL53L1_PRM_00101             = 0x09;
		ptiming->VL53L1_PRM_00150             = 0x0b;






		ptiming->VL53L1_PRM_00243           = 0x00;
		ptiming->VL53L1_PRM_00244           = 0x21;
		ptiming->VL53L1_PRM_00245           = 0x00;
		ptiming->VL53L1_PRM_00246           = 0x1b;




		ptiming->VL53L1_PRM_00146        = 0x00;
		ptiming->VL53L1_PRM_00147        = 0x29;
		ptiming->VL53L1_PRM_00148        = 0x00;
		ptiming->VL53L1_PRM_00149        = 0x22;




		pgeneral->VL53L1_PRM_00140                 = 0x09;













		pgeneral->VL53L1_PRM_00100         = 0x42;




		pdynamic->VL53L1_PRM_00277                      = 0x09;
		pdynamic->VL53L1_PRM_00278                      = 0x0B;
		pdynamic->VL53L1_PRM_00279            = 0x09;
		pdynamic->VL53L1_PRM_00280            = 0x09;

		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00131;





		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00140 | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00067(
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




	status = VL53L1_FCTN_00066(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_FCTN_00113(
				  7,   0,   1, 2, 3, 4,
				8+0, 8+1, 8+2, 3, 4, 5,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00135 | \
				VL53L1_DEF_00131;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00068(
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




	status = VL53L1_FCTN_00067(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00130 | \
				VL53L1_DEF_00131;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00069(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_FCTN_00113(
				7, 0, 1, 1, 2, 2,
				0, 1, 2, 1, 2, 3,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->VL53L1_PRM_00101             = 0x05;
		ptiming->VL53L1_PRM_00150             = 0x07;






		ptiming->VL53L1_PRM_00243           = 0x00;
		ptiming->VL53L1_PRM_00244           = 0x37;
		ptiming->VL53L1_PRM_00245           = 0x00;
		ptiming->VL53L1_PRM_00246           = 0x29;




		ptiming->VL53L1_PRM_00146        = 0x00;
		ptiming->VL53L1_PRM_00147        = 0x44;
		ptiming->VL53L1_PRM_00148        = 0x00;
		ptiming->VL53L1_PRM_00149        = 0x33;




		pgeneral->VL53L1_PRM_00140                 = 0x05;













		pgeneral->VL53L1_PRM_00100         = 0x6D;




		pdynamic->VL53L1_PRM_00277                      = 0x05;
		pdynamic->VL53L1_PRM_00278                      = 0x07;
		pdynamic->VL53L1_PRM_00279            = 0x05;
		pdynamic->VL53L1_PRM_00280            = 0x05;

		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00131;





		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00140 | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00070(
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




	status = VL53L1_FCTN_00069(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {




		VL53L1_FCTN_00113(
				  7,   0,   1, 1, 2, 2,
				8+0, 8+1, 8+2, 1, 2, 3,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00135  | \
				VL53L1_DEF_00131;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00071(
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




	status = VL53L1_FCTN_00070(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00130 | \
				VL53L1_DEF_00131;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00072(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_FCTN_00113(
				7, 0, 1, 1, 1, 1,
				0, 1, 1, 1, 2, 2,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);






		ptiming->VL53L1_PRM_00101             = 0x03;
		ptiming->VL53L1_PRM_00150             = 0x05;






		ptiming->VL53L1_PRM_00243           = 0x00;
		ptiming->VL53L1_PRM_00244           = 0x52;
		ptiming->VL53L1_PRM_00245           = 0x00;
		ptiming->VL53L1_PRM_00246           = 0x37;




		ptiming->VL53L1_PRM_00146        = 0x00;
		ptiming->VL53L1_PRM_00147        = 0x66;
		ptiming->VL53L1_PRM_00148        = 0x00;
		ptiming->VL53L1_PRM_00149        = 0x44;




		pgeneral->VL53L1_PRM_00140                 = 0x03;













		pgeneral->VL53L1_PRM_00100         = 0xA4;




		pdynamic->VL53L1_PRM_00277                      = 0x03;
		pdynamic->VL53L1_PRM_00278                      = 0x05;
		pdynamic->VL53L1_PRM_00279            = 0x03;
		pdynamic->VL53L1_PRM_00280            = 0x03;

		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00135  | \


				VL53L1_DEF_00131;





		psystem->VL53L1_PRM_00122 = \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00140 | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00073(
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




	status = VL53L1_FCTN_00072(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);











	if (status == VL53L1_ERROR_NONE) {









		VL53L1_FCTN_00113(
				  7,   0, 1, 1, 1, 1,
				8+0, 8+1, 1, 1, 2, 2,
				phistogram);




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00135  | \
				VL53L1_DEF_00131;

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00074(
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




	status = VL53L1_FCTN_00073(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pdynamic->VL53L1_PRM_00023 = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00128 | \
				VL53L1_DEF_00129 | \
				VL53L1_DEF_00130 | \
				VL53L1_DEF_00131;
	}

	LOG_FUNCTION_END(status);

	return status;
}



VL53L1_Error VL53L1_FCTN_00075(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		pstatic->VL53L1_PRM_00205                            = 0x01;
	    psystem->VL53L1_PRM_00286      = 0x01;

		pdynamic->VL53L1_PRM_00023               = \
				VL53L1_DEF_00126 | \
				VL53L1_DEF_00127 | \
				VL53L1_DEF_00131;

		psystem->VL53L1_PRM_00122                     = \
				VL53L1_DEF_00110    | \
				VL53L1_DEF_00141   | \
				VL53L1_DEF_00002;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00076(
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




	status = VL53L1_FCTN_00063(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		status =
			VL53L1_FCTN_00081(
				pgeneral,
				pzone_cfg);










		ptiming->VL53L1_PRM_00013                  = 0x0000;


		ptiming->VL53L1_PRM_00012 = 0x0000;







		ptiming->VL53L1_PRM_00101                   = 0x09;
		ptiming->VL53L1_PRM_00150                   = 0x09;






		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);

	}

	LOG_FUNCTION_END(status);

	return status;
}

VL53L1_Error VL53L1_FCTN_00063(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {






		status =
			VL53L1_FCTN_00082(
			  4, 8, 2,

			  4, 8, 2,

			  7, 7,

			  pzone_cfg);

		pgeneral->VL53L1_PRM_00119 =
			pzone_cfg->VL53L1_PRM_00020 + 1;










		VL53L1_FCTN_00113(
				7, 0, 1, 2, 3, 4,
				7, 0, 1, 2, 3, 4,
				phistogram);






		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);

	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00077(
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




	status = VL53L1_FCTN_00054(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {



		psystem->VL53L1_PRM_00283  = 0x01;
	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00114(
	VL53L1_histogram_config_t *phistogram,
	VL53L1_static_config_t    *pstatic,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic)
{






	LOG_FUNCTION_START("");

	pstatic->VL53L1_PRM_00219 =
			phistogram->VL53L1_PRM_00264;
	pstatic->VL53L1_PRM_00220 =
			phistogram->VL53L1_PRM_00265;
	pstatic->VL53L1_PRM_00200 =
			phistogram->VL53L1_PRM_00266;

	pstatic->VL53L1_PRM_00221 =
			phistogram->VL53L1_PRM_00267;

	pstatic->VL53L1_PRM_00222 =
			phistogram->VL53L1_PRM_00268;
	pstatic->VL53L1_PRM_00223 =
			phistogram->VL53L1_PRM_00269;

	pstatic->VL53L1_PRM_00224 =
		(((uint16_t)phistogram->VL53L1_PRM_00256) << 8)
		+ (uint16_t)phistogram->VL53L1_PRM_00257;

	pstatic->VL53L1_PRM_00225 =
			phistogram->VL53L1_PRM_00258;
	pstatic->VL53L1_PRM_00226 =
			phistogram->VL53L1_PRM_00259;
	pstatic->VL53L1_PRM_00203 =
			phistogram->VL53L1_PRM_00260;

	pstatic->VL53L1_PRM_00227 =
			phistogram->VL53L1_PRM_00261;
	pstatic->VL53L1_PRM_00228 =
			phistogram->VL53L1_PRM_00262;

	pstatic->VL53L1_PRM_00229 =
			phistogram->VL53L1_PRM_00263;

	ptiming->VL53L1_PRM_00013 =
		(((uint16_t)phistogram->VL53L1_PRM_00250) << 8)
		+ (uint16_t)phistogram->VL53L1_PRM_00251;

	ptiming->VL53L1_PRM_00012 =
		(((uint16_t)phistogram->VL53L1_PRM_00252) << 8)
		+ (uint16_t)phistogram->VL53L1_PRM_00253;

	ptiming->VL53L1_PRM_00247 =
			phistogram->VL53L1_PRM_00254;
	ptiming->VL53L1_PRM_00248 =
			phistogram->VL53L1_PRM_00255;

	pdynamic->VL53L1_PRM_00273 =
			phistogram->VL53L1_PRM_00270;

	pdynamic->VL53L1_PRM_00274 =
			phistogram->VL53L1_PRM_00271;

	pdynamic->VL53L1_PRM_00275 =
			phistogram->VL53L1_PRM_00272;

	LOG_FUNCTION_END(0);

}


VL53L1_Error VL53L1_FCTN_00064(
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




	status = VL53L1_FCTN_00060(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);




	if (status == VL53L1_ERROR_NONE) {




		phistogram->VL53L1_PRM_00272 = 0x00;




		VL53L1_FCTN_00114(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);
	}

	LOG_FUNCTION_END(status);

	return status;
}




