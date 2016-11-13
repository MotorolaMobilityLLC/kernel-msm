
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
#include "vl53l1_api_preset_modes.h"


#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)


VL53L1_Error VL53L1_FCTN_00032(
	VL53L1_refspadchar_config_t   *pdata)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	









	pdata->VL53L1_PRM_00045 =
			VL53L1_DEF_00104;
	pdata->VL53L1_PRM_00040              = 0x0B;
	pdata->VL53L1_PRM_00041                = 1000;
	pdata->VL53L1_PRM_00042    = 0x0A00;
	pdata->VL53L1_PRM_00044 = 0x0500;
	pdata->VL53L1_PRM_00043 = 0x1400;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00033(
	VL53L1_hist_post_process_config_t   *pdata)
{
	




	VL53L1_Error  status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	


	pdata->VL53L1_PRM_00152 =
			VL53L1_DEF_00105;
	pdata->VL53L1_PRM_00153                   =   1;  

	pdata->VL53L1_PRM_00154                   =   2;  

	pdata->VL53L1_PRM_00155          =  96;  

	pdata->VL53L1_PRM_00156               =  10;  

	pdata->VL53L1_PRM_00157     =  10;  

	pdata->VL53L1_PRM_00159 =   1;  

	pdata->VL53L1_PRM_00160                  = 128;  

	pdata->VL53L1_PRM_00161               =   0;  


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00049(
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

	


	

	pstatic->VL53L1_PRM_00163               = 0x0A00;
	pstatic->VL53L1_PRM_00164                                      = 0x00;
	pstatic->VL53L1_PRM_00165                                  = 0x00;
	pstatic->VL53L1_PRM_00166                                 = 0x00;
	pstatic->VL53L1_PRM_00167                                   = 0x00;
	pstatic->VL53L1_PRM_00168                          = 0x00;
	pstatic->VL53L1_PRM_00169                          = 0x00;
	pstatic->VL53L1_PRM_00170                                  = 0x00;
	pstatic->VL53L1_PRM_00171                               = 0x00;
	pstatic->VL53L1_PRM_00172                        = 0x00;
	pstatic->VL53L1_PRM_00173                                = 0x00;
	

	pstatic->VL53L1_PRM_00078                                = 0x11;
	pstatic->VL53L1_PRM_00174                              = 0x02;
	pstatic->VL53L1_PRM_00175                              = 0x00;
	pstatic->VL53L1_PRM_00176                     = 0x02;
	pstatic->VL53L1_PRM_00114             = 0x08;
	pstatic->VL53L1_PRM_00177                = 0x00;

	pstatic->VL53L1_PRM_00178        = 0x08;
	pstatic->VL53L1_PRM_00179      = 0x10;
	pstatic->VL53L1_PRM_00159                    = 0x01;
	pstatic->VL53L1_PRM_00180     = 0x00;
	pstatic->VL53L1_PRM_00181         = 0x00;
	pstatic->VL53L1_PRM_00182         = 0x00;
	pstatic->VL53L1_PRM_00183                = 0x0000;

	

	pstatic->VL53L1_PRM_00184               = 0x14;
	pstatic->VL53L1_PRM_00185                             = 0x00;
	

	pstatic->VL53L1_PRM_00186               = 0x08;
	pstatic->VL53L1_PRM_00187         = 0x00;
	pstatic->VL53L1_PRM_00188                      = 0x00;
	pstatic->VL53L1_PRM_00189                      = 0x00;

	pgeneral->VL53L1_PRM_00190           = 0x00;
	pgeneral->VL53L1_PRM_00191                   = 0x00;
	pgeneral->VL53L1_PRM_00192 =
			VL53L1_DEF_00106;
	pgeneral->VL53L1_PRM_00111                         = 0x0B;
	pgeneral->VL53L1_PRM_00193                         = 0x0000;
	pgeneral->VL53L1_PRM_00113                      = 0x02;
	

	pgeneral->VL53L1_PRM_00075                 = 0x0D;
	

	pgeneral->VL53L1_PRM_00194                         = 0x21;
	pgeneral->VL53L1_PRM_00195                       = 0x00;
	pgeneral->VL53L1_PRM_00196                    = 0x01;
	

	pgeneral->VL53L1_PRM_00197                        = 0x0000;
	pgeneral->VL53L1_PRM_00198                         = 0x0000;
	

	pgeneral->VL53L1_PRM_00199       = 0x8C00;
	pgeneral->VL53L1_PRM_00200                 = 0x00;
	pgeneral->VL53L1_PRM_00201                = 0x33;
	pgeneral->VL53L1_PRM_00202                     = 0xFF;
	pgeneral->VL53L1_PRM_00203                     = 0x01;

	


	ptiming->VL53L1_PRM_00204                   = 0x00;
	ptiming->VL53L1_PRM_00205                   = 0x06;
	ptiming->VL53L1_PRM_00206                   = 0x00;
	ptiming->VL53L1_PRM_00207                   = 0x06;
	ptiming->VL53L1_PRM_00117                = 0x02;
	ptiming->VL53L1_PRM_00118                = 0x85;
	

	ptiming->VL53L1_PRM_00076                     = 0x0B;
	ptiming->VL53L1_PRM_00120                = 0x01;
	ptiming->VL53L1_PRM_00121                = 0x92;
	

	ptiming->VL53L1_PRM_00122                     = 0x09;
	

	ptiming->VL53L1_PRM_00014                       = 0x0050;
	

	ptiming->VL53L1_PRM_00013      = 0x0020;

	




	ptiming->VL53L1_PRM_00208                    = 0x08;
	ptiming->VL53L1_PRM_00209                   = 0x78;
	ptiming->VL53L1_PRM_00090                  = 0x00000000;
	ptiming->VL53L1_PRM_00210                        = 0x00;

	


























	phistogram->VL53L1_PRM_00211        = 0x07;
	phistogram->VL53L1_PRM_00212        = 0x21;
	phistogram->VL53L1_PRM_00213        = 0x43;

	phistogram->VL53L1_PRM_00214         = 0x10;
	phistogram->VL53L1_PRM_00215         = 0x32;
	phistogram->VL53L1_PRM_00216         = 0x54;

	phistogram->VL53L1_PRM_00217        = 0x07;
	phistogram->VL53L1_PRM_00218        = 0x21;
	phistogram->VL53L1_PRM_00219        = 0x43;

	phistogram->VL53L1_PRM_00220         = 0x10;
	phistogram->VL53L1_PRM_00221           = 0x02;
	phistogram->VL53L1_PRM_00222         = 0x43;
	phistogram->VL53L1_PRM_00223           = 0x05;

	phistogram->VL53L1_PRM_00224             = 0x00;

	phistogram->VL53L1_PRM_00225       = 0x07;
	phistogram->VL53L1_PRM_00226       = 0x21;
	phistogram->VL53L1_PRM_00227       = 0x43;

	phistogram->VL53L1_PRM_00228        = 0x10;
	phistogram->VL53L1_PRM_00229        = 0x32;
	phistogram->VL53L1_PRM_00230        = 0x54;

	phistogram->VL53L1_PRM_00231              = 0xFFFF;
	phistogram->VL53L1_PRM_00232             = 0xFFFF;

	phistogram->VL53L1_PRM_00233        = 0x00;

	






	pzone_cfg->VL53L1_PRM_00020                     = VL53L1_MAX_USER_ZONES;
	pzone_cfg->VL53L1_PRM_00021                                   = 0x00;
	pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00019                           = 0x0f;
	pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00018                            = 0x0f;
	pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00016                         = 0x08;
	pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00017                         = 0x08;

	


	pdynamic->VL53L1_PRM_00234                 = 0x01;

	pdynamic->VL53L1_PRM_00235                              = 0x0000;
	pdynamic->VL53L1_PRM_00236                               = 0x0000;
	pdynamic->VL53L1_PRM_00237                = 0x00;
	pdynamic->VL53L1_PRM_00238 =
			VL53L1_DEF_00107;
	

	pdynamic->VL53L1_PRM_00239                               = 0x0B;
	

	pdynamic->VL53L1_PRM_00240                               = 0x09;
	pdynamic->VL53L1_PRM_00241                     = 0x0A;
	pdynamic->VL53L1_PRM_00242                     = 0x0A;

	pdynamic->VL53L1_PRM_00243                 = 0x01;

	


























	pdynamic->VL53L1_PRM_00244 = 0x00;
	pdynamic->VL53L1_PRM_00245         = 0x02;

	




	pdynamic->VL53L1_PRM_00091              = 0xC7;
	

	pdynamic->VL53L1_PRM_00092 = 0xFF;


	pdynamic->VL53L1_PRM_00024                          = \
			VL53L1_DEF_00108 | \
			VL53L1_DEF_00109 | \
			VL53L1_DEF_00110 | \
			VL53L1_DEF_00111 | \
			VL53L1_DEF_00112 | \
			VL53L1_DEF_00113;

	pdynamic->VL53L1_PRM_00246                   = 0x02;

	



	psystem->VL53L1_PRM_00247                         = 0x00;
	psystem->VL53L1_PRM_00248                                  = 0x01;
	psystem->VL53L1_PRM_00249                           = \
			VL53L1_DEF_00114;

	psystem->VL53L1_PRM_00097                                = \
			VL53L1_DEF_00115 | \
			VL53L1_DEF_00116 | \
			VL53L1_DEF_00005;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00050(
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

	




	status = VL53L1_FCTN_00049(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		










		ptiming->VL53L1_PRM_00076                = 0x07;
		ptiming->VL53L1_PRM_00122                = 0x05;
		ptiming->VL53L1_PRM_00014                  = 0x0050;
		ptiming->VL53L1_PRM_00013 = 0x0020;
		ptiming->VL53L1_PRM_00208               = 0x08;
		ptiming->VL53L1_PRM_00209              = 0x38;

		





		pdynamic->VL53L1_PRM_00239                         = 0x07;
		pdynamic->VL53L1_PRM_00240                         = 0x05;
		pdynamic->VL53L1_PRM_00241               = 0x06;
		pdynamic->VL53L1_PRM_00242               = 0x06;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00051(
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

	




	status = VL53L1_FCTN_00049(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		










		ptiming->VL53L1_PRM_00076                = 0x0F;
		ptiming->VL53L1_PRM_00122                = 0x0D;
		ptiming->VL53L1_PRM_00014                  = 0x0050;
		ptiming->VL53L1_PRM_00013 = 0x0020;
		ptiming->VL53L1_PRM_00208               = 0x08;
		ptiming->VL53L1_PRM_00209              = 0xB8;

		





		pdynamic->VL53L1_PRM_00239                         = 0x0F;
		pdynamic->VL53L1_PRM_00240                         = 0x0D;
		pdynamic->VL53L1_PRM_00241               = 0x0E;
		pdynamic->VL53L1_PRM_00242               = 0x0E;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00052(
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

	




	status = VL53L1_FCTN_00049(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		pdynamic->VL53L1_PRM_00024  = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00111 | \
				VL53L1_DEF_00117;
	}

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00053(
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

	




	status = VL53L1_FCTN_00049(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		pdynamic->VL53L1_PRM_00024  = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00111 | \
				VL53L1_DEF_00112;
	}

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

	


	status = VL53L1_FCTN_00049(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		


		

		pdynamic->VL53L1_PRM_00246 = 0x00;


		


		ptiming->VL53L1_PRM_00090 = 0x00000600;
		pdynamic->VL53L1_PRM_00238 =
				VL53L1_DEF_00118;

		


		

		psystem->VL53L1_PRM_00097 =
				VL53L1_DEF_00119 | \
				VL53L1_DEF_00116     | \
				VL53L1_DEF_00120;
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

	


	status = VL53L1_FCTN_00049(
		pstatic,
		phistogram,
		pgeneral,
		ptiming,
		pdynamic,
		psystem,
		pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		


		

		pdynamic->VL53L1_PRM_00246 = 0x00;

		


		pdynamic->VL53L1_PRM_00238 =
				VL53L1_DEF_00118;

		


		

		psystem->VL53L1_PRM_00097 = \
				VL53L1_DEF_00119 | \
				VL53L1_DEF_00116     | \
				VL53L1_DEF_00121;
	}

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

	


	status = VL53L1_FCTN_00049(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		




		pstatic->VL53L1_PRM_00163           = 0x1400;

		



























		phistogram->VL53L1_PRM_00211      = 0x07;
		phistogram->VL53L1_PRM_00212      = 0x21;
		phistogram->VL53L1_PRM_00213      = 0x43;

		phistogram->VL53L1_PRM_00214       = 0x10;
		phistogram->VL53L1_PRM_00215       = 0x32;
		phistogram->VL53L1_PRM_00216       = 0x54;

		phistogram->VL53L1_PRM_00217      = 0x07;
		phistogram->VL53L1_PRM_00218      = 0x21;
		phistogram->VL53L1_PRM_00219      = 0x43;

		phistogram->VL53L1_PRM_00220       = 0x10;
		phistogram->VL53L1_PRM_00221         = 0x02;
		phistogram->VL53L1_PRM_00222       = 0x43;
		phistogram->VL53L1_PRM_00223         = 0x05;

		phistogram->VL53L1_PRM_00224           = 0x00;

		phistogram->VL53L1_PRM_00225     = 0x07;
		phistogram->VL53L1_PRM_00226     = 0x21;
		phistogram->VL53L1_PRM_00227     = 0x43;

		phistogram->VL53L1_PRM_00228      = 0x10;
		phistogram->VL53L1_PRM_00229      = 0x32;
		phistogram->VL53L1_PRM_00230      = 0x54;

		


		phistogram->VL53L1_PRM_00231          = 0xFFFF;
		phistogram->VL53L1_PRM_00232         = 0xFFFF;

		phistogram->VL53L1_PRM_00233    = 0x00;

		







		ptiming->VL53L1_PRM_00076                   = 0x09;
		ptiming->VL53L1_PRM_00122                   = 0x0B;
		pdynamic->VL53L1_PRM_00239                            = 0x09;
		pdynamic->VL53L1_PRM_00240                            = 0x0B;

		


		VL53L1_FCTN_00090(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);

		








		pgeneral->VL53L1_PRM_00075               = 0x40;

		


		pdynamic->VL53L1_PRM_00024 = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00111 | \
				

				

				VL53L1_DEF_00113;


		


		psystem->VL53L1_PRM_00097 = \
				VL53L1_DEF_00094 | \
				VL53L1_DEF_00122 | \
				VL53L1_DEF_00005;
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

	


	status = VL53L1_FCTN_00055(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		



























		phistogram->VL53L1_PRM_00214       = 0x98;
		phistogram->VL53L1_PRM_00215       = 0x3A;

		phistogram->VL53L1_PRM_00220       = 0x98;
		phistogram->VL53L1_PRM_00221         = 0x0A;

		phistogram->VL53L1_PRM_00228      = 0x98;
		phistogram->VL53L1_PRM_00229      = 0x3A;

		


		VL53L1_FCTN_00090(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);

		


		pdynamic->VL53L1_PRM_00024 = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00111 | \
				VL53L1_DEF_00117 | \
				VL53L1_DEF_00113;

		


		psystem->VL53L1_PRM_00097 = \
				VL53L1_DEF_00094 | \
				VL53L1_DEF_00122 | \
				VL53L1_DEF_00005;
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

	


	status = VL53L1_FCTN_00056(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		


		pdynamic->VL53L1_PRM_00024 = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00111 | \
				VL53L1_DEF_00112 | \
				VL53L1_DEF_00113;
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

	


	status = VL53L1_FCTN_00049(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		




		pstatic->VL53L1_PRM_00163           = 0x1400;

		


		phistogram->VL53L1_PRM_00211      = 0x07;
		phistogram->VL53L1_PRM_00212      = 0x21;
		phistogram->VL53L1_PRM_00213      = 0x43;

		phistogram->VL53L1_PRM_00214       = 0x07;
		phistogram->VL53L1_PRM_00215       = 0x21;
		phistogram->VL53L1_PRM_00216       = 0x43;

		phistogram->VL53L1_PRM_00217      = 0x07;
		phistogram->VL53L1_PRM_00218      = 0x21;
		phistogram->VL53L1_PRM_00219      = 0x43;

		phistogram->VL53L1_PRM_00220       = 0x07;
		phistogram->VL53L1_PRM_00221         = 0x01;
		phistogram->VL53L1_PRM_00222       = 0x32;
		phistogram->VL53L1_PRM_00223         = 0x04;

		phistogram->VL53L1_PRM_00224           = 0x00;

		phistogram->VL53L1_PRM_00225     = 0x07;
		phistogram->VL53L1_PRM_00226     = 0x21;
		phistogram->VL53L1_PRM_00227     = 0x43;

		phistogram->VL53L1_PRM_00228      = 0x07;
		phistogram->VL53L1_PRM_00229      = 0x21;
		phistogram->VL53L1_PRM_00230      = 0x43;

		


		phistogram->VL53L1_PRM_00231          = 0xFFFF;
		phistogram->VL53L1_PRM_00232         = 0xFFFF;

		


		VL53L1_FCTN_00090(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);

		




		ptiming->VL53L1_PRM_00076             = 0x04;
		ptiming->VL53L1_PRM_00122             = 0x03;
		ptiming->VL53L1_PRM_00204           = 0x00;
		ptiming->VL53L1_PRM_00205           = 0x42;
		ptiming->VL53L1_PRM_00206           = 0x00;
		ptiming->VL53L1_PRM_00207           = 0x42;
		ptiming->VL53L1_PRM_00117        = 0x00;
		ptiming->VL53L1_PRM_00118        = 0x52;
		ptiming->VL53L1_PRM_00120        = 0x00;
		ptiming->VL53L1_PRM_00121        = 0x66;

		








		pgeneral->VL53L1_PRM_00075         = 0xa4;

		


		pdynamic->VL53L1_PRM_00024 = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00110 | \
				VL53L1_DEF_00111 | \
				

				

				VL53L1_DEF_00113;


		


		psystem->VL53L1_PRM_00097 = \
				VL53L1_DEF_00094 | \
				VL53L1_DEF_00122 | \
				VL53L1_DEF_00005;
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

	


	status = VL53L1_FCTN_00055(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		


		pstatic->VL53L1_PRM_00164                            = 0x01;
	    psystem->VL53L1_PRM_00250      = 0x01;

		pdynamic->VL53L1_PRM_00024               = \
				VL53L1_DEF_00108 | \
				VL53L1_DEF_00109 | \
				VL53L1_DEF_00113;

		psystem->VL53L1_PRM_00097                     = \
				VL53L1_DEF_00094    | \
				VL53L1_DEF_00123   | \
				VL53L1_DEF_00005;
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

	


	status = VL53L1_FCTN_00055(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		




		pgeneral->VL53L1_PRM_00191 = 0x05;

		

		pzone_cfg->VL53L1_PRM_00020                    = VL53L1_MAX_USER_ZONES;
		

		pzone_cfg->VL53L1_PRM_00021                 = 0x04;

		pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00019         = 7;
		pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00018          = 7;
		pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00016       = 4;
		pzone_cfg->VL53L1_PRM_00022[0].VL53L1_PRM_00017       = 4;

		pzone_cfg->VL53L1_PRM_00022[1].VL53L1_PRM_00019         = 7;
		pzone_cfg->VL53L1_PRM_00022[1].VL53L1_PRM_00018          = 7;
		pzone_cfg->VL53L1_PRM_00022[1].VL53L1_PRM_00016       = 4;
		pzone_cfg->VL53L1_PRM_00022[1].VL53L1_PRM_00017       = 12;

		pzone_cfg->VL53L1_PRM_00022[2].VL53L1_PRM_00019         = 7;
		pzone_cfg->VL53L1_PRM_00022[2].VL53L1_PRM_00018          = 7;
		pzone_cfg->VL53L1_PRM_00022[2].VL53L1_PRM_00016       = 12;
		pzone_cfg->VL53L1_PRM_00022[2].VL53L1_PRM_00017       = 4;

		pzone_cfg->VL53L1_PRM_00022[3].VL53L1_PRM_00019         = 7;
		pzone_cfg->VL53L1_PRM_00022[3].VL53L1_PRM_00018          = 7;
		pzone_cfg->VL53L1_PRM_00022[3].VL53L1_PRM_00016       = 12;
		pzone_cfg->VL53L1_PRM_00022[3].VL53L1_PRM_00017       = 12;

		pzone_cfg->VL53L1_PRM_00022[4].VL53L1_PRM_00019         = 15;
		pzone_cfg->VL53L1_PRM_00022[4].VL53L1_PRM_00018          = 15;
		pzone_cfg->VL53L1_PRM_00022[4].VL53L1_PRM_00016       = 8;
		pzone_cfg->VL53L1_PRM_00022[4].VL53L1_PRM_00017       = 8;

		




		phistogram->VL53L1_PRM_00211      = 0x07;
		phistogram->VL53L1_PRM_00212      = 0x21;
		phistogram->VL53L1_PRM_00213      = 0x43;
		phistogram->VL53L1_PRM_00214       = 0x07;
		phistogram->VL53L1_PRM_00215       = 0x21;
		phistogram->VL53L1_PRM_00216       = 0x43;

		






		

		ptiming->VL53L1_PRM_00014                  = 0x0000;
		

		ptiming->VL53L1_PRM_00013 = 0x0000;

		




		VL53L1_FCTN_00090(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);

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

	


	status = VL53L1_FCTN_00049(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		

		psystem->VL53L1_PRM_00247  = 0x01;
	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00090(
	VL53L1_histogram_config_t *phistogram,
	VL53L1_static_config_t    *pstatic,
	VL53L1_general_config_t   *pgeneral,
	VL53L1_timing_config_t    *ptiming,
	VL53L1_dynamic_config_t   *pdynamic)
{
	





	LOG_FUNCTION_START("");
	

	(void) (pgeneral);

	pstatic->VL53L1_PRM_00178 =
			phistogram->VL53L1_PRM_00225;
	pstatic->VL53L1_PRM_00179 =
			phistogram->VL53L1_PRM_00226;
	pstatic->VL53L1_PRM_00159 =
			phistogram->VL53L1_PRM_00227;

	pstatic->VL53L1_PRM_00180 =
			phistogram->VL53L1_PRM_00228;

	pstatic->VL53L1_PRM_00181 =
			phistogram->VL53L1_PRM_00229;
	pstatic->VL53L1_PRM_00182 =
			phistogram->VL53L1_PRM_00230;

	pstatic->VL53L1_PRM_00183 =
		(((uint16_t)phistogram->VL53L1_PRM_00217) << 8)
		+ (uint16_t)phistogram->VL53L1_PRM_00218;

	pstatic->VL53L1_PRM_00184 =
			phistogram->VL53L1_PRM_00219;
	pstatic->VL53L1_PRM_00185 =
			phistogram->VL53L1_PRM_00220;
	pstatic->VL53L1_PRM_00186 =
			phistogram->VL53L1_PRM_00221;

	pstatic->VL53L1_PRM_00187 =
			phistogram->VL53L1_PRM_00222;
	pstatic->VL53L1_PRM_00188 =
			phistogram->VL53L1_PRM_00223;

	pstatic->VL53L1_PRM_00189 =
			phistogram->VL53L1_PRM_00224;

	ptiming->VL53L1_PRM_00014 =
		(((uint16_t)phistogram->VL53L1_PRM_00211) << 8)
		+ (uint16_t)phistogram->VL53L1_PRM_00212;

	ptiming->VL53L1_PRM_00013 =
		(((uint16_t)phistogram->VL53L1_PRM_00213) << 8)
		+ (uint16_t)phistogram->VL53L1_PRM_00214;

	ptiming->VL53L1_PRM_00208 =
			phistogram->VL53L1_PRM_00215;
	ptiming->VL53L1_PRM_00209 =
			phistogram->VL53L1_PRM_00216;

	pdynamic->VL53L1_PRM_00235 =
			phistogram->VL53L1_PRM_00231;

	pdynamic->VL53L1_PRM_00236 =
			phistogram->VL53L1_PRM_00232;

	pdynamic->VL53L1_PRM_00237 =
			phistogram->VL53L1_PRM_00233;

	LOG_FUNCTION_END(0);

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

	


	status = VL53L1_FCTN_00055(
					pstatic,
					phistogram,
					pgeneral,
					ptiming,
					pdynamic,
					psystem,
					pzone_cfg);

	


	if (status == VL53L1_ERROR_NONE) {

		


		phistogram->VL53L1_PRM_00233 = 0x00;

		


		VL53L1_FCTN_00090(
				phistogram,
				pstatic,
				pgeneral,
				ptiming,
				pdynamic);
	}

	LOG_FUNCTION_END(status);

	return status;
}




