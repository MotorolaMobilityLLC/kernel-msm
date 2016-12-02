
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
#include "vl53l1_platform.h"
#include "vl53l1_register_map.h"
#include "vl53l1_register_funcs.h"
#include "vl53l1_register_settings.h"
#include "vl53l1_hist_structs.h"
#include "vl53l1_core.h"

#if 1
#include <asm/div64.h>
#include <linux/math64.h>
#endif




#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_CORE, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_CORE, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_CORE, \
		status, fmt, ##__VA_ARGS__)


#define trace_print(level, ...) \
	VL53L1_trace_print_module_function(VL53L1_TRACE_MODULE_CORE, \
		level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


void  VL53L1_FCTN_00033(
	VL53L1_DEV        Dev)
{





	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->VL53L1_PRM_00107.VL53L1_PRM_00287    = VL53L1_DEF_00142;
	pdev->VL53L1_PRM_00107.VL53L1_PRM_00288    = VL53L1_DEF_00143;
	pdev->VL53L1_PRM_00107.VL53L1_PRM_00289    = VL53L1_DEF_00144;
	pdev->VL53L1_PRM_00107.VL53L1_PRM_00290 = VL53L1_DEF_00145;
}


void  VL53L1_FCTN_00038(
	VL53L1_DEV         Dev,
	VL53L1_DeviceState device_state)
{





	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->VL53L1_PRM_00069);

	pstate->VL53L1_PRM_00130  = device_state;
	pstate->VL53L1_PRM_00291  = 0;
	pstate->VL53L1_PRM_00124        = VL53L1_DEF_00146;
	pstate->VL53L1_PRM_00292 = 0;
	pstate->VL53L1_PRM_00123       = 0;

	pstate->VL53L1_PRM_00038   = device_state;
	pstate->VL53L1_PRM_00293   = 0;
	pstate->VL53L1_PRM_00294         = VL53L1_DEF_00146;
	pstate->VL53L1_PRM_00145  = 0;
	pstate->VL53L1_PRM_00070        = 0;

}


VL53L1_Error  VL53L1_FCTN_00091(
	VL53L1_DEV         Dev)
{










	VL53L1_Error        status  = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_ll_driver_state_t *pstate = &(pdev->VL53L1_PRM_00069);




	LOG_FUNCTION_START("");





	if ((pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 &&
		VL53L1_DEF_00147) == 0x00) {

		pstate->VL53L1_PRM_00038  = VL53L1_DEF_00058;
		pstate->VL53L1_PRM_00293  = 0;
		pstate->VL53L1_PRM_00294 = VL53L1_DEF_00146;
		pstate->VL53L1_PRM_00145 = 0;
		pstate->VL53L1_PRM_00070       = 0;

	} else {






		if (pstate->VL53L1_PRM_00293 == 0xFF)
			pstate->VL53L1_PRM_00293 = 0x80;
		else
			pstate->VL53L1_PRM_00293++;






		pstate->VL53L1_PRM_00294 ^=
			VL53L1_DEF_00146;




		switch (pstate->VL53L1_PRM_00038) {

		case VL53L1_DEF_00058:

			if ((pdev->VL53L1_PRM_00022.VL53L1_PRM_00127 &
				VL53L1_DEF_00146) > 0)
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00038;
			else
				if (pstate->VL53L1_PRM_00070 >=
					pdev->VL53L1_PRM_00014.VL53L1_PRM_00020)
					pstate->VL53L1_PRM_00038 =
						VL53L1_DEF_00030;
				else
					pstate->VL53L1_PRM_00038 =
						VL53L1_DEF_00029;

			pstate->VL53L1_PRM_00293  = 0;
			pstate->VL53L1_PRM_00145 = 0;
			pstate->VL53L1_PRM_00070       = 0;

		break;

		case VL53L1_DEF_00038:
			pstate->VL53L1_PRM_00293 = 0;
			pstate->VL53L1_PRM_00070      = 0;
			if (pstate->VL53L1_PRM_00070 >=
				pdev->VL53L1_PRM_00014.VL53L1_PRM_00020)
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00030;
			else
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00029;
		break;

		case VL53L1_DEF_00029:
			pstate->VL53L1_PRM_00070++;
			if (pstate->VL53L1_PRM_00070 >=
				pdev->VL53L1_PRM_00014.VL53L1_PRM_00020)
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00030;
			else
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00029;
		break;

		case VL53L1_DEF_00030:
			pstate->VL53L1_PRM_00070        = 0;
			pstate->VL53L1_PRM_00145 ^= 0x01;

			if (pstate->VL53L1_PRM_00070 >=
				pdev->VL53L1_PRM_00014.VL53L1_PRM_00020)
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00030;
			else
				pstate->VL53L1_PRM_00038 =
					VL53L1_DEF_00029;
		break;

		default:
			pstate->VL53L1_PRM_00038  =
				VL53L1_DEF_00058;
			pstate->VL53L1_PRM_00293  = 0;
			pstate->VL53L1_PRM_00294 = VL53L1_DEF_00146;
			pstate->VL53L1_PRM_00145 = 0;
			pstate->VL53L1_PRM_00070       = 0;
		break;

		}
	}





	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00104(
	VL53L1_DEV         Dev)
{








	VL53L1_Error         status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_ll_driver_state_t  *pstate       = &(pdev->VL53L1_PRM_00069);
	VL53L1_system_results_t   *psys_results = &(pdev->VL53L1_PRM_00027);
	VL53L1_histogram_bin_data_t *phist_data = &(pdev->VL53L1_PRM_00073);

	uint8_t   device_range_status   = 0;
	uint8_t   device_stream_count   = 0;
	uint8_t   device_gph_id         = 0;
	uint8_t   histogram_mode        = 0;
	uint8_t   VL53L1_PRM_00295 = 0;
	uint8_t   VL53L1_PRM_00296       = 0;

	LOG_FUNCTION_START("");





	device_range_status =
			psys_results->VL53L1_PRM_00105 &
			VL53L1_DEF_00120;

	device_stream_count = psys_results->VL53L1_PRM_00026;




	histogram_mode =
		(pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 &
		VL53L1_DEF_00110) ==
    VL53L1_DEF_00110;



  device_gph_id = (psys_results->VL53L1_PRM_00132 &
                    VL53L1_DEF_00148) >> 4;
  if(histogram_mode)  {
    device_gph_id = (phist_data->VL53L1_PRM_00132 &
                      VL53L1_DEF_00148) >> 4;
  }




	if ((pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 &
		VL53L1_DEF_00002) ==
		VL53L1_DEF_00002) {














		if (pstate->VL53L1_PRM_00038 ==
			VL53L1_DEF_00038) {

			if (histogram_mode == 0)
				if (device_range_status !=
					VL53L1_DEF_00017)
					status = VL53L1_ERROR_GPH_SYNC_CHECK_FAIL;

		} else {

			if (pstate->VL53L1_PRM_00293 != device_stream_count)
				status = VL53L1_ERROR_STREAM_COUNT_CHECK_FAIL;






			if (pstate->VL53L1_PRM_00294 != device_gph_id) {
				status = VL53L1_ERROR_GPH_ID_CHECK_FAIL;








      } else {








      }






      VL53L1_PRM_00295 = pres->VL53L1_PRM_00155.VL53L1_PRM_00035[pstate->VL53L1_PRM_00070].VL53L1_PRM_00295;
      VL53L1_PRM_00296 = pres->VL53L1_PRM_00155.VL53L1_PRM_00035[pstate->VL53L1_PRM_00070].VL53L1_PRM_00296;






			if (VL53L1_PRM_00295 != device_stream_count)  {




        if( (pdev->VL53L1_PRM_00014.VL53L1_PRM_00020 == 0) && (device_stream_count == 255) ) {









        } else {

				  status = VL53L1_ERROR_ZONE_STREAM_COUNT_CHECK_FAIL;








        }
      }






			if (VL53L1_PRM_00296 != device_gph_id) {
				status = VL53L1_ERROR_ZONE_GPH_ID_CHECK_FAIL;








      }

		}











	}


	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error  VL53L1_FCTN_00092(
	VL53L1_DEV         Dev)
{





	VL53L1_Error         status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_ll_driver_state_t *pstate = &(pdev->VL53L1_PRM_00069);

	uint8_t prev_cfg_zone_id;
	uint8_t prev_cfg_gph_id;
	uint8_t prev_cfg_stream_count;

	LOG_FUNCTION_START("");








	if ((pdev->VL53L1_PRM_00121.VL53L1_PRM_00122 &&
		VL53L1_DEF_00147) == 0x00) {

		pstate->VL53L1_PRM_00130  = VL53L1_DEF_00058;
		pstate->VL53L1_PRM_00291  = 0;
		pstate->VL53L1_PRM_00124 = VL53L1_DEF_00146;
		pstate->VL53L1_PRM_00292 = 0;
		pstate->VL53L1_PRM_00123       = 0;
		prev_cfg_zone_id          = 0;
		prev_cfg_gph_id           = 0;
		prev_cfg_stream_count     = 0;


		pdev->VL53L1_PRM_00099.VL53L1_PRM_00235 = VL53L1_DEF_00149;

	} else {





		prev_cfg_gph_id           = pstate->VL53L1_PRM_00124;
		prev_cfg_zone_id          = pstate->VL53L1_PRM_00123;
		prev_cfg_stream_count     = pstate->VL53L1_PRM_00291;






		if (pstate->VL53L1_PRM_00291 == 0xFF)
			pstate->VL53L1_PRM_00291 = 0x80;
		else
			pstate->VL53L1_PRM_00291++;






		pstate->VL53L1_PRM_00124 ^=
			VL53L1_DEF_00146;






		switch (pstate->VL53L1_PRM_00130) {

		case VL53L1_DEF_00058:
			pstate->VL53L1_PRM_00123 = 1;
			if (pstate->VL53L1_PRM_00123 >
				pdev->VL53L1_PRM_00014.VL53L1_PRM_00020) {
				pstate->VL53L1_PRM_00123 = 0;
				pstate->VL53L1_PRM_00292 ^= 0x01;
			}
			pstate->VL53L1_PRM_00291 = 1;
			pstate->VL53L1_PRM_00130 =
				VL53L1_DEF_00150;
		break;

		case VL53L1_DEF_00150:
			pstate->VL53L1_PRM_00123++;
			pdev->VL53L1_PRM_00099.VL53L1_PRM_00235 = 0x01;
			if (pstate->VL53L1_PRM_00123 >
				pdev->VL53L1_PRM_00014.VL53L1_PRM_00020) {

				pstate->VL53L1_PRM_00123 = 0;
				pstate->VL53L1_PRM_00292 ^= 0x01;






				if(pdev->VL53L1_PRM_00014.VL53L1_PRM_00020 > 0){
					pstate->VL53L1_PRM_00130 =
							VL53L1_DEF_00151;


					pdev->VL53L1_PRM_00099.VL53L1_PRM_00235 =
							VL53L1_DEF_00152;
					pdev->VL53L1_PRM_00099.VL53L1_PRM_00238 =
						pres->VL53L1_PRM_00075.VL53L1_PRM_00035[0].VL53L1_PRM_00035[0].VL53L1_PRM_00032;
				}
			}
		break;

		case VL53L1_DEF_00151:
			pstate->VL53L1_PRM_00123++;
			if (pstate->VL53L1_PRM_00123 >
				pdev->VL53L1_PRM_00014.VL53L1_PRM_00020) {
				pstate->VL53L1_PRM_00123 = 0;
				pstate->VL53L1_PRM_00292 ^= 0x01;
			}
			pdev->VL53L1_PRM_00099.VL53L1_PRM_00238 =
					pres->VL53L1_PRM_00075.VL53L1_PRM_00035[pstate->VL53L1_PRM_00123].VL53L1_PRM_00035[0].VL53L1_PRM_00032;
		break;

		default:
			pstate->VL53L1_PRM_00130 =
				VL53L1_DEF_00058;
			pstate->VL53L1_PRM_00291 = 0;
			pstate->VL53L1_PRM_00124 = VL53L1_DEF_00146;
			pstate->VL53L1_PRM_00292 = 0;
			pstate->VL53L1_PRM_00123       = 0;
		break;

		}

	}






  if(pdev->VL53L1_PRM_00014.VL53L1_PRM_00020 == 0)  {



    pres->VL53L1_PRM_00155.VL53L1_PRM_00035[prev_cfg_zone_id].VL53L1_PRM_00295 =
      prev_cfg_stream_count - 1;

    pres->VL53L1_PRM_00155.VL53L1_PRM_00035[pstate->VL53L1_PRM_00070].VL53L1_PRM_00296 =
      prev_cfg_gph_id ^
      VL53L1_DEF_00146;
  }
  else  {
    pres->VL53L1_PRM_00155.VL53L1_PRM_00035[prev_cfg_zone_id].VL53L1_PRM_00295 =
      prev_cfg_stream_count;
    pres->VL53L1_PRM_00155.VL53L1_PRM_00035[prev_cfg_zone_id].VL53L1_PRM_00296 =
      prev_cfg_gph_id;
  }






	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00115(
		VL53L1_system_results_t  *pdata)
{






	pdata->VL53L1_PRM_00132                       = 0xFF;
	pdata->VL53L1_PRM_00105                           = 0xFF;
	pdata->VL53L1_PRM_00106                          = 0xFF;
	pdata->VL53L1_PRM_00026                           = 0xFF;

	pdata->VL53L1_PRM_00162         = 0xFFFF;
	pdata->VL53L1_PRM_00297        = 0xFFFF;
	pdata->VL53L1_PRM_00166            = 0xFFFF;
	pdata->VL53L1_PRM_00167                              = 0xFFFF;
	pdata->VL53L1_PRM_00169                              = 0xFFFF;
	pdata->VL53L1_PRM_00170 = 0xFFFF;
	pdata->VL53L1_PRM_00163 =
			0xFFFF;
	pdata->VL53L1_PRM_00298    = 0xFFFF;
	pdata->VL53L1_PRM_00299    = 0xFFFF;
	pdata->VL53L1_PRM_00165         = 0xFFFF;

	pdata->VL53L1_PRM_00177         = 0xFFFF;
	pdata->VL53L1_PRM_00178        = 0xFFFF;
	pdata->VL53L1_PRM_00179            = 0xFFFF;
	pdata->VL53L1_PRM_00180                              = 0xFFFF;
	pdata->VL53L1_PRM_00181                              = 0xFFFF;
	pdata->VL53L1_PRM_00182 = 0xFFFF;
	pdata->VL53L1_PRM_00300                            = 0xFFFF;
	pdata->VL53L1_PRM_00301                            = 0xFFFF;
	pdata->VL53L1_PRM_00302                            = 0xFFFF;
	pdata->VL53L1_PRM_00303                            = 0xFF;

}


void VL53L1_FCTN_00079(
	uint8_t                 VL53L1_PRM_00020,
	VL53L1_zone_results_t  *pdata)
{




	uint8_t  z = 0;
	VL53L1_range_results_t *presults;

	pdata->VL53L1_PRM_00019    = VL53L1_MAX_USER_ZONES;
	pdata->VL53L1_PRM_00020 = VL53L1_PRM_00020;

	for (z = 0 ; z < pdata->VL53L1_PRM_00019 ; z++) {

		presults = &(pdata->VL53L1_PRM_00035[z]);
		presults->VL53L1_PRM_00130 = VL53L1_DEF_00058;
		presults->VL53L1_PRM_00038  = VL53L1_DEF_00058;
		presults->VL53L1_PRM_00063      = VL53L1_MAX_RANGE_RESULTS;
		presults->VL53L1_PRM_00037   = 0;

	}
}


void VL53L1_FCTN_00113(
	uint8_t   even_bin0,
	uint8_t   even_bin1,
	uint8_t   even_bin2,
	uint8_t   even_bin3,
	uint8_t   even_bin4,
	uint8_t   even_bin5,
	uint8_t   odd_bin0,
	uint8_t   odd_bin1,
	uint8_t   odd_bin2,
	uint8_t   odd_bin3,
	uint8_t   odd_bin4,
	uint8_t   odd_bin5,
	VL53L1_histogram_config_t  *pdata)
{






	pdata->VL53L1_PRM_00250  = (even_bin1 << 4) + even_bin0;
	pdata->VL53L1_PRM_00251  = (even_bin3 << 4) + even_bin2;
	pdata->VL53L1_PRM_00252  = (even_bin5 << 4) + even_bin4;

	pdata->VL53L1_PRM_00253   = (odd_bin1 << 4) + odd_bin0;
	pdata->VL53L1_PRM_00254   = (odd_bin3 << 4) + odd_bin2;
	pdata->VL53L1_PRM_00255   = (odd_bin5 << 4) + odd_bin4;

	pdata->VL53L1_PRM_00256  = pdata->VL53L1_PRM_00250;
	pdata->VL53L1_PRM_00257  = pdata->VL53L1_PRM_00251;
	pdata->VL53L1_PRM_00258  = pdata->VL53L1_PRM_00252;

	pdata->VL53L1_PRM_00259   = pdata->VL53L1_PRM_00253;
	pdata->VL53L1_PRM_00260     = odd_bin2;
	pdata->VL53L1_PRM_00261   = (odd_bin4 << 4) + odd_bin3;
	pdata->VL53L1_PRM_00262     = odd_bin5;

	pdata->VL53L1_PRM_00263       = 0x00;

	pdata->VL53L1_PRM_00264 = pdata->VL53L1_PRM_00250;
	pdata->VL53L1_PRM_00265 = pdata->VL53L1_PRM_00251;
	pdata->VL53L1_PRM_00266 = pdata->VL53L1_PRM_00252;

	pdata->VL53L1_PRM_00267  = pdata->VL53L1_PRM_00253;
	pdata->VL53L1_PRM_00268  = pdata->VL53L1_PRM_00254;
	pdata->VL53L1_PRM_00269  = pdata->VL53L1_PRM_00255;




	pdata->VL53L1_PRM_00270        = 0xFFFF;
	pdata->VL53L1_PRM_00271       = 0xFFFF;




	pdata->VL53L1_PRM_00272  = 0x00;

}


void VL53L1_FCTN_00022(
	int32_t                      bin_value,
	uint16_t                     VL53L1_PRM_00139,
	VL53L1_histogram_bin_data_t *pdata)
{






	uint16_t          i = 0;

	pdata->VL53L1_PRM_00130          = VL53L1_DEF_00058;
	pdata->VL53L1_PRM_00038           = VL53L1_DEF_00058;

	pdata->VL53L1_PRM_00036                   = 0;
	pdata->VL53L1_PRM_00024                = 0;

	pdata->VL53L1_PRM_00137                 = 0;
	pdata->VL53L1_PRM_00138               = VL53L1_DEF_00035;
	pdata->VL53L1_PRM_00139            = (uint8_t)VL53L1_PRM_00139;
	pdata->VL53L1_PRM_00129    = 0;

	pdata->VL53L1_PRM_00132           = 0;
	pdata->VL53L1_PRM_00105               = 0;
	pdata->VL53L1_PRM_00106              = 0;
	pdata->VL53L1_PRM_00026               = 0;

	pdata->VL53L1_PRM_00133 = 0;
	pdata->VL53L1_PRM_00134   = 0;
	pdata->VL53L1_PRM_00135       = 0;
	pdata->VL53L1_PRM_00140            = 0;

	pdata->VL53L1_PRM_00141                        = 0;
	pdata->VL53L1_PRM_00041                       = 0;
	pdata->VL53L1_PRM_00144                = 0;
	pdata->VL53L1_PRM_00152              = 0;

	pdata->VL53L1_PRM_00304                      = 0;
	pdata->VL53L1_PRM_00305                      = 0;

	pdata->VL53L1_PRM_00306                = 0;
	pdata->VL53L1_PRM_00307          = 0;
	pdata->VL53L1_PRM_00308                 = 0;
	pdata->VL53L1_PRM_00309             = 0;

	for (i = 0 ; i < VL53L1_DEF_00153 ; i++)
		pdata->VL53L1_PRM_00151[i] = (uint8_t)i;
	for (i = 0 ; i < VL53L1_DEF_00153 ; i++)
		pdata->VL53L1_PRM_00310[i] = 1;

	for (i = 0 ; i < VL53L1_DEF_00035 ; i++)
		if (i < VL53L1_PRM_00139)
			pdata->VL53L1_PRM_00136[i] = bin_value;
		else
			pdata->VL53L1_PRM_00136[i] = 0;
}


void VL53L1_FCTN_00116(
		VL53L1_xtalk_histogram_data_t *pxtalk,
		VL53L1_histogram_bin_data_t   *phist)
{






	phist->VL53L1_PRM_00140 =
			pxtalk->VL53L1_PRM_00140;
	phist->VL53L1_PRM_00144     =
			pxtalk->VL53L1_PRM_00144;
	phist->VL53L1_PRM_00137               =
			pxtalk->VL53L1_PRM_00137;

	phist->VL53L1_PRM_00134   =
			pxtalk->VL53L1_PRM_00134;
	phist->VL53L1_PRM_00135       =
			pxtalk->VL53L1_PRM_00135;

	phist->VL53L1_PRM_00141             =
			pxtalk->VL53L1_PRM_00141;
	phist->VL53L1_PRM_00306     =
			pxtalk->VL53L1_PRM_00306;

	phist->VL53L1_PRM_00036      = pxtalk->VL53L1_PRM_00036;
	phist->VL53L1_PRM_00138  = pxtalk->VL53L1_PRM_00138;
	phist->VL53L1_PRM_00024   = pxtalk->VL53L1_PRM_00024;


}


void VL53L1_FCTN_00042(
	uint32_t                       bin_value,
	uint16_t                       VL53L1_PRM_00139,
	VL53L1_xtalk_histogram_data_t *pdata)
{






	uint16_t          i = 0;

	pdata->VL53L1_PRM_00036                   = 0;
	pdata->VL53L1_PRM_00024                = 0;

	pdata->VL53L1_PRM_00137                 = 0;
	pdata->VL53L1_PRM_00138               = VL53L1_DEF_00035;
	pdata->VL53L1_PRM_00139            = (uint8_t)VL53L1_PRM_00139;

	pdata->VL53L1_PRM_00134   = 0;
	pdata->VL53L1_PRM_00135       = 0;
	pdata->VL53L1_PRM_00140            = 0;

	pdata->VL53L1_PRM_00141                        = 0;
	pdata->VL53L1_PRM_00144                = 0;

	pdata->VL53L1_PRM_00306                = 0;

	for (i = 0 ; i < VL53L1_DEF_00035 ; i++)
		if (i < VL53L1_PRM_00139)
			pdata->VL53L1_PRM_00136[i] = bin_value;
		else
			pdata->VL53L1_PRM_00136[i] = 0;
}


void VL53L1_FCTN_00117(
	uint16_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	uint16_t   VL53L1_PRM_00035 = 0;

	VL53L1_PRM_00035 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00035 & 0x00FF);
		VL53L1_PRM_00035 = VL53L1_PRM_00035 >> 8;
	}
}

uint16_t VL53L1_FCTN_00105(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   value = 0x00;

	while (count-- > 0)
		value = (value << 8) | (uint16_t)*pbuffer++;
	return value;
}


void VL53L1_FCTN_00118(
	int16_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	int16_t    VL53L1_PRM_00035 = 0;

	VL53L1_PRM_00035 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00035 & 0x00FF);
		VL53L1_PRM_00035 = VL53L1_PRM_00035 >> 8;
	}
}

int16_t VL53L1_FCTN_00119(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	int16_t    value = 0x00;



	if (*pbuffer >= 0x80)
		value = 0xFFFF;

	while (count-- > 0)
		value = (value << 8) | (int16_t)*pbuffer++;
	return value;
}

void VL53L1_FCTN_00120(
	uint32_t    ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	uint32_t   VL53L1_PRM_00035 = 0;

	VL53L1_PRM_00035 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00035 & 0x00FF);
		VL53L1_PRM_00035 = VL53L1_PRM_00035 >> 8;
	}
}

uint32_t VL53L1_FCTN_00106(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint32_t   value = 0x00;

	while (count-- > 0)
		value = (value << 8) | (uint32_t)*pbuffer++;
	return value;
}


uint32_t VL53L1_FCTN_00121(
	uint16_t    count,
	uint8_t    *pbuffer,
	uint32_t    bit_mask,
	uint32_t    down_shift,
	uint32_t    offset)
{






	uint32_t   value = 0x00;



	while (count-- > 0)
		value = (value << 8) | (uint32_t)*pbuffer++;



	value =  value & bit_mask;
	if (down_shift > 0)
		value = value >> down_shift;



	value = value + offset;

	return value;
}


void VL53L1_FCTN_00122(
	int32_t     ip_value,
	uint16_t    count,
	uint8_t    *pbuffer)
{






	uint16_t   i    = 0;
	int32_t    VL53L1_PRM_00035 = 0;

	VL53L1_PRM_00035 =  ip_value;

	for (i = 0; i < count ; i++) {
		pbuffer[count-i-1] = (uint8_t)(VL53L1_PRM_00035 & 0x00FF);
		VL53L1_PRM_00035 = VL53L1_PRM_00035 >> 8;
	}
}

int32_t VL53L1_FCTN_00123(
	uint16_t    count,
	uint8_t    *pbuffer)
{






	int32_t    value = 0x00;



	if (*pbuffer >= 0x80)
		value = 0xFFFFFFFF;

	while (count-- > 0)
		value = (value << 8) | (int32_t)*pbuffer++;
	return value;
}


VL53L1_Error VL53L1_FCTN_00029(
	VL53L1_DEV    Dev,
	uint8_t       VL53L1_PRM_00206)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
					Dev,
					VL53L1_DEF_00154,
					VL53L1_PRM_00206);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00124(
	VL53L1_DEV    Dev,
	uint8_t       value)
{





	VL53L1_Error status         = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->VL53L1_PRM_00121.VL53L1_PRM_00284 = value;

	status = VL53L1_WrByte(
				Dev,
				VL53L1_DEF_00155,
				pdev->VL53L1_PRM_00121.VL53L1_PRM_00284);

	return status;
}

VL53L1_Error VL53L1_FCTN_00037(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00124(Dev, 0x01);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00036(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00124(Dev, 0x00);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00125(
	VL53L1_DEV    Dev,
	uint8_t       value)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	pdev->VL53L1_PRM_00121.VL53L1_PRM_00286 = value;

	status = VL53L1_WrByte(
			Dev,
			VL53L1_DEF_00156,
			pdev->VL53L1_PRM_00121.VL53L1_PRM_00286);

	return status;
}


VL53L1_Error VL53L1_FCTN_00019(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00125(Dev, 0x01);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00126(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	status = VL53L1_FCTN_00125(Dev, 0x00);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00031(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	LOG_FUNCTION_START("");

	pdev->VL53L1_PRM_00121.VL53L1_PRM_00285 = VL53L1_DEF_00132;

	status = VL53L1_WrByte(
					Dev,
					VL53L1_DEF_00157,
					pdev->VL53L1_PRM_00121.VL53L1_PRM_00285);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00127(
	VL53L1_DEV    Dev)
{





	VL53L1_Error status       = VL53L1_ERROR_NONE;

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WrByte(
				Dev,
				VL53L1_DEF_00158,
				0x00);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	return status;
}


uint32_t VL53L1_FCTN_00028(
	uint16_t  VL53L1_PRM_00098,
	uint8_t   VL53L1_PRM_00041)
{








	uint32_t  VL53L1_PRM_00131        = 0;
	uint8_t   VL53L1_PRM_00311   = 0;
	uint32_t  macro_period_us      = 0;

	LOG_FUNCTION_START("");






	VL53L1_PRM_00131 = VL53L1_FCTN_00109(VL53L1_PRM_00098);






	VL53L1_PRM_00311 = VL53L1_FCTN_00128(VL53L1_PRM_00041);














	macro_period_us =
			(uint32_t)VL53L1_DEF_00159 *
			VL53L1_PRM_00131;
	macro_period_us = macro_period_us >> 6;

	macro_period_us = macro_period_us * (uint32_t)VL53L1_PRM_00311;
	macro_period_us = macro_period_us >> 6;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "VL53L1_PRM_00131",
			VL53L1_PRM_00131);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "VL53L1_PRM_00311",
			VL53L1_PRM_00311);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "macro_period_us",
			macro_period_us);

	LOG_FUNCTION_END(0);

	return macro_period_us;
}


uint32_t VL53L1_FCTN_00109(
	uint16_t  VL53L1_PRM_00098)
{














	uint32_t  VL53L1_PRM_00131        = 0;

	LOG_FUNCTION_START("");

	VL53L1_PRM_00131 = (0x01 << 30) / VL53L1_PRM_00098;

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u\n", "VL53L1_PRM_00131",
			VL53L1_PRM_00131);

	LOG_FUNCTION_END(0);

	return VL53L1_PRM_00131;
}


uint32_t VL53L1_FCTN_00129(
	uint16_t VL53L1_PRM_00098)
{






	uint32_t VL53L1_PRM_00131 = 0;
	uint32_t pll_period_mm = 0;

	LOG_FUNCTION_START("");






	VL53L1_PRM_00131  = VL53L1_FCTN_00109(VL53L1_PRM_00098);












	pll_period_mm =
			VL53L1_DEF_00160 *
			(VL53L1_PRM_00131 >> 2);



	pll_period_mm = (pll_period_mm + (0x01<<15)) >> 16;

	LOG_FUNCTION_END(0);

	return pll_period_mm;
}


uint16_t VL53L1_FCTN_00130(
	uint32_t VL53L1_PRM_00042,
	uint32_t macro_period_us)
{










	uint32_t timeout_mclks   = 0;
	uint16_t timeout_encoded = 0;

	LOG_FUNCTION_START("");

	timeout_mclks   =
			((VL53L1_PRM_00042 << 12) + (macro_period_us>>1)) /
			macro_period_us;
	timeout_encoded = VL53L1_FCTN_00131(timeout_mclks);

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_mclks",
			timeout_mclks, timeout_mclks);
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    %-48s : %10u  (0x%04X)\n", "timeout_encoded",
			timeout_encoded, timeout_encoded);

	LOG_FUNCTION_END(0);

	return timeout_encoded;
}


uint16_t VL53L1_FCTN_00131(uint32_t timeout_mclks)
{





	uint16_t encoded_timeout = 0;
	uint32_t ls_byte = 0;
	uint16_t ms_byte = 0;

	if (timeout_mclks > 0) {
		ls_byte = timeout_mclks - 1;

		while ((ls_byte & 0xFFFFFF00) > 0) {
			ls_byte = ls_byte >> 1;
			ms_byte++;
		}

		encoded_timeout = (ms_byte << 8)
				+ (uint16_t) (ls_byte & 0x000000FF);
	}

	return encoded_timeout;
}


uint32_t VL53L1_FCTN_00108(uint16_t encoded_timeout)
{






	uint32_t timeout_macro_clks = 0;

	timeout_macro_clks = ((uint32_t) (encoded_timeout & 0x00FF)
			<< (uint32_t) ((encoded_timeout & 0xFF00) >> 8)) + 1;

	return timeout_macro_clks;
}


VL53L1_Error VL53L1_FCTN_00005(
	uint32_t                VL53L1_PRM_00006,
	uint32_t                VL53L1_PRM_00007,
	uint16_t                VL53L1_PRM_00098,
	VL53L1_timing_config_t *ptiming)
{








	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint32_t macro_period_us    = 0;
	uint16_t timeout_encoded    = 0;

	LOG_FUNCTION_START("");

	if (VL53L1_PRM_00098 == 0) {
		status = VL53L1_ERROR_DIVISION_BY_ZERO;
	} else {



		macro_period_us =
				VL53L1_FCTN_00028(
					VL53L1_PRM_00098,
					ptiming->VL53L1_PRM_00101);



		timeout_encoded =
				VL53L1_FCTN_00130(
					VL53L1_PRM_00006,
					macro_period_us);

		ptiming->VL53L1_PRM_00243 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00244 =
				(uint8_t) (timeout_encoded & 0x00FF);



		timeout_encoded =
				VL53L1_FCTN_00130(
					VL53L1_PRM_00007,
					macro_period_us);

		ptiming->VL53L1_PRM_00146 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00147 =
				(uint8_t) (timeout_encoded & 0x00FF);



		macro_period_us =
				VL53L1_FCTN_00028(
					VL53L1_PRM_00098,
					ptiming->VL53L1_PRM_00150);



		timeout_encoded =
				VL53L1_FCTN_00130(
					VL53L1_PRM_00006,
					macro_period_us);

		ptiming->VL53L1_PRM_00245 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00246 =
				(uint8_t) (timeout_encoded & 0x00FF);



		timeout_encoded = VL53L1_FCTN_00130(
							VL53L1_PRM_00007,
							macro_period_us);

		ptiming->VL53L1_PRM_00148 =
				(uint8_t)((timeout_encoded & 0xFF00) >> 8);
		ptiming->VL53L1_PRM_00149 =
				(uint8_t) (timeout_encoded & 0x00FF);
	}

	LOG_FUNCTION_END(0);

	return status;

}


uint8_t VL53L1_FCTN_00132(uint8_t VL53L1_PRM_00311)
{






	uint8_t vcsel_period_reg = 0;

	vcsel_period_reg = (VL53L1_PRM_00311 >> 1) - 1;

	return vcsel_period_reg;
}


uint8_t VL53L1_FCTN_00128(uint8_t vcsel_period_reg)
{






	uint8_t VL53L1_PRM_00311 = 0;

	VL53L1_PRM_00311 = (vcsel_period_reg + 1) << 1;

	return VL53L1_PRM_00311;
}


uint32_t VL53L1_FCTN_00133(
	uint8_t  *pbuffer,
	uint8_t   no_of_bytes)
{





	uint8_t   i = 0;
	uint32_t  decoded_value = 0;

	for (i = 0 ; i < no_of_bytes ; i++)
		decoded_value = (decoded_value << 8) + (uint32_t)pbuffer[i];

	return decoded_value;
}


void VL53L1_FCTN_00134(
	uint32_t  ip_value,
	uint8_t   no_of_bytes,
	uint8_t  *pbuffer)
{





	uint8_t   i    = 0;
	uint32_t  VL53L1_PRM_00035 = 0;

	VL53L1_PRM_00035 = ip_value;
	for (i = 0; i < no_of_bytes ; i++) {
		pbuffer[no_of_bytes-i-1] = VL53L1_PRM_00035 & 0x00FF;
		VL53L1_PRM_00035 = VL53L1_PRM_00035 >> 8;
	}
}

uint32_t  VL53L1_FCTN_00110(
	uint32_t  VL53L1_PRM_00131,
	uint32_t  vcsel_parm_pclks,
	uint32_t  window_vclks,
	uint32_t  elapsed_mclks)
{










	uint64_t  tmp_long_int = 0;
	uint32_t  duration_us  = 0;






	duration_us = window_vclks * VL53L1_PRM_00131;





	duration_us = duration_us >> 12;



	tmp_long_int = (uint64_t)duration_us;






	duration_us = elapsed_mclks * vcsel_parm_pclks;





	duration_us = duration_us >> 4;





	tmp_long_int = tmp_long_int * (uint64_t)duration_us;





	tmp_long_int = tmp_long_int >> 12;



	if (tmp_long_int > 0xFFFFFFFF)
		tmp_long_int = 0xFFFFFFFF;

	duration_us  = (uint32_t)tmp_long_int;

	return duration_us;
}


uint16_t VL53L1_FCTN_00135(
	int32_t   VL53L1_PRM_00199,
	uint32_t  time_us)
{












	uint32_t  tmp_int   = 0;
	uint32_t  frac_bits = 7;
	uint16_t  rate_mcps = 0;







	if (VL53L1_PRM_00199 > VL53L1_DEF_00161)
		tmp_int = VL53L1_DEF_00161;
	else if (VL53L1_PRM_00199 > 0)
		tmp_int = (uint32_t)VL53L1_PRM_00199;







	if (VL53L1_PRM_00199 > VL53L1_DEF_00162)
		frac_bits = 3;
	else
		frac_bits = 7;







	if (time_us > 0)
		tmp_int = ((tmp_int << frac_bits) + (time_us / 2)) / time_us;





	if (VL53L1_PRM_00199 > VL53L1_DEF_00162)
		tmp_int = tmp_int << 4;







	if (tmp_int > 0xFFFF)
		tmp_int = 0xFFFF;

	rate_mcps =  (uint16_t)tmp_int;

	return rate_mcps;
}


int32_t VL53L1_FCTN_00136(
	uint16_t  VL53L1_PRM_00098,
	uint16_t  VL53L1_PRM_00094,
	uint16_t  VL53L1_PRM_00306,
	int32_t   VL53L1_PRM_00202)
{





	uint32_t    VL53L1_PRM_00131 = 0;

	int64_t     tmp_long_int  = 0;
	int32_t     range_mm      = 0;




	VL53L1_PRM_00131  = VL53L1_FCTN_00109(VL53L1_PRM_00098);










	tmp_long_int = (int64_t)VL53L1_PRM_00094 - (int64_t)VL53L1_PRM_00306;










	tmp_long_int =  tmp_long_int * (int64_t)VL53L1_PRM_00131;






	tmp_long_int =  tmp_long_int / (0x01 << 9);











	tmp_long_int =  tmp_long_int * VL53L1_DEF_00160;






	tmp_long_int =  tmp_long_int / (0x01 << 22);



	range_mm  = (int32_t)tmp_long_int + VL53L1_PRM_00202;



	range_mm = range_mm / (0x01 << 2);

	return range_mm;
}


uint16_t VL53L1_FCTN_00137(
	uint32_t  frac_bits,
	uint32_t  peak_count_rate,
	uint16_t  num_spads,
	uint32_t  max_output_value)
{

	uint32_t  tmp_int   = 0;



	uint16_t  rate_per_spad = 0;









	if (num_spads > 0) {
		tmp_int = (peak_count_rate << 8) << frac_bits;
	    tmp_int = (tmp_int + ((uint32_t)num_spads / 2)) / (uint32_t)num_spads;
	} else {
		tmp_int = ((peak_count_rate) << frac_bits);
	}




	if (tmp_int > max_output_value)
		tmp_int = max_output_value;

	rate_per_spad = (uint16_t)tmp_int;

	return rate_per_spad;
}


uint32_t VL53L1_FCTN_00138(
	int32_t   VL53L1_PRM_00072,
	uint16_t  num_spads,
	uint32_t  duration)
{

    uint64_t total_hist_counts  = 0;
    uint64_t xtalk_per_spad     = 0;
    uint32_t rate_per_spad_kcps = 0;










#if 0
	total_hist_counts = ((uint64_t)VL53L1_PRM_00072
			* 1000 * 256) / (uint64_t)num_spads;
#else
	{
		uint64_t n = ((uint64_t)VL53L1_PRM_00072 * 1000 * 256);
		uint32_t base = (uint32_t)num_spads;

		total_hist_counts = do_div(n, base);
	}
#endif

	if (duration > 0) {
#if 0
		xtalk_per_spad = (((uint64_t)(total_hist_counts << 11))
			+ ((uint64_t)duration / 2))
			/ (uint64_t) duration;
#else
		uint64_t n = (((uint64_t)(total_hist_counts << 11))
				+ ((uint64_t)duration / 2));
		uint32_t base = (uint32_t)duration;

		xtalk_per_spad = do_div(n, base);
#endif
	} else {
		xtalk_per_spad =   (uint64_t)(total_hist_counts << 11);
	}

	rate_per_spad_kcps = (uint32_t)xtalk_per_spad;

	return rate_per_spad_kcps;
}


void  VL53L1_FCTN_00139(
	VL53L1_histogram_bin_data_t   *pdata)
{





	uint8_t  bin            = 0;

	LOG_FUNCTION_START("");

	for (bin = 0 ; bin < pdata->VL53L1_PRM_00139 ; bin++) {

		if (bin == 0 || pdata->VL53L1_PRM_00304 >= pdata->VL53L1_PRM_00136[bin])
			pdata->VL53L1_PRM_00304 = pdata->VL53L1_PRM_00136[bin];

		if (bin == 0 || pdata->VL53L1_PRM_00305 <= pdata->VL53L1_PRM_00136[bin])
			pdata->VL53L1_PRM_00305 = pdata->VL53L1_PRM_00136[bin];

	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00140(
	VL53L1_histogram_bin_data_t   *pdata)
{







	uint8_t bin = 0;
	uint8_t VL53L1_PRM_00039 = 0;
	uint8_t i = 0;




	if (pdata->VL53L1_PRM_00151[0] == 0x07) {

		i = 0;
		for (VL53L1_PRM_00039 = 0 ; VL53L1_PRM_00039 < VL53L1_DEF_00153 ; VL53L1_PRM_00039++) {
			if (pdata->VL53L1_PRM_00151[VL53L1_PRM_00039] != 0x07) {
				pdata->VL53L1_PRM_00151[i] = pdata->VL53L1_PRM_00151[VL53L1_PRM_00039];
				pdata->VL53L1_PRM_00310[i] = pdata->VL53L1_PRM_00310[VL53L1_PRM_00039];
				i++;
			}
		}






		for (VL53L1_PRM_00039 = i ; VL53L1_PRM_00039 < VL53L1_DEF_00153 ; VL53L1_PRM_00039++) {
			pdata->VL53L1_PRM_00151[VL53L1_PRM_00039] = VL53L1_DEF_00163 + 1;
			pdata->VL53L1_PRM_00310[VL53L1_PRM_00039] = 0;
		}

	}

	if (pdata->VL53L1_PRM_00129 > 0) {



		for (bin = pdata->VL53L1_PRM_00129 ;
				bin < pdata->VL53L1_PRM_00138 ; bin++)
			pdata->VL53L1_PRM_00136[bin-pdata->VL53L1_PRM_00129] =
				pdata->VL53L1_PRM_00136[bin];



		pdata->VL53L1_PRM_00139 =
				pdata->VL53L1_PRM_00139 -
				pdata->VL53L1_PRM_00129;
		pdata->VL53L1_PRM_00129 = 0;

	}
}


void  VL53L1_FCTN_00112(
	VL53L1_histogram_bin_data_t   *pdata)
{






	uint8_t  bin            = 0;

	LOG_FUNCTION_START("");

	if (pdata->VL53L1_PRM_00129 > 0) {

		pdata->VL53L1_PRM_00307 =
			pdata->VL53L1_PRM_00129;




		pdata->VL53L1_PRM_00308 = 0;
		for (bin = 0 ; bin < pdata->VL53L1_PRM_00129 ; bin++)
			pdata->VL53L1_PRM_00308 += pdata->VL53L1_PRM_00136[bin];

		pdata->VL53L1_PRM_00309 = pdata->VL53L1_PRM_00308;
		pdata->VL53L1_PRM_00309 +=
				((int32_t)pdata->VL53L1_PRM_00129 / 2);
		pdata->VL53L1_PRM_00309 /=
			(int32_t)pdata->VL53L1_PRM_00129;

	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00141(
	int32_t                        ambient_threshold_sigma,
	VL53L1_histogram_bin_data_t   *pdata)
{









	uint8_t  bin                      = 0;
	int32_t  VL53L1_PRM_00312 = 0;

	LOG_FUNCTION_START("");






	VL53L1_FCTN_00139(pdata);







	VL53L1_PRM_00312  = (int32_t)VL53L1_FCTN_00142((uint32_t)pdata->VL53L1_PRM_00304);
	VL53L1_PRM_00312 *= ambient_threshold_sigma;
	VL53L1_PRM_00312 += 0x07;
	VL53L1_PRM_00312  = VL53L1_PRM_00312 >> 4;
	VL53L1_PRM_00312 += pdata->VL53L1_PRM_00304;






	pdata->VL53L1_PRM_00307 = 0;
	pdata->VL53L1_PRM_00308        = 0;

	for (bin = 0 ; bin < pdata->VL53L1_PRM_00139 ; bin++)
		if (pdata->VL53L1_PRM_00136[bin] < VL53L1_PRM_00312) {
			pdata->VL53L1_PRM_00308 += pdata->VL53L1_PRM_00136[bin];
			pdata->VL53L1_PRM_00307++;
		}






	if (pdata->VL53L1_PRM_00307 > 0) {
		pdata->VL53L1_PRM_00309 = pdata->VL53L1_PRM_00308;
		pdata->VL53L1_PRM_00309 += ((int32_t)pdata->VL53L1_PRM_00307/2);
		pdata->VL53L1_PRM_00309 /= (int32_t)pdata->VL53L1_PRM_00307;
	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00100(
	VL53L1_histogram_bin_data_t   *pidata,
	VL53L1_histogram_bin_data_t   *podata)
{






	int64_t  VL53L1_PRM_00199            = 0;
	int64_t  tmpi              = 0;
	int64_t  tmpo              = 0;

	LOG_FUNCTION_START("");

	if (pidata->VL53L1_PRM_00129 >  0 &&
		podata->VL53L1_PRM_00129 == 0) {










		tmpo    = 1 + (int64_t)podata->VL53L1_PRM_00152;
		tmpo   *= (int64_t)podata->VL53L1_PRM_00133;

		tmpi    = 1 + (int64_t)pidata->VL53L1_PRM_00152;
		tmpi   *= (int64_t)pidata->VL53L1_PRM_00133;

		VL53L1_PRM_00199  = tmpo * (int64_t)pidata->VL53L1_PRM_00308;
		VL53L1_PRM_00199 += (tmpi/2);
#if 0
		VL53L1_PRM_00199 /=  tmpi;
#else
		VL53L1_PRM_00199 = div64_s64(VL53L1_PRM_00199, tmpi);
#endif

		podata->VL53L1_PRM_00308 = (int32_t)VL53L1_PRM_00199;






		podata->VL53L1_PRM_00309 = podata->VL53L1_PRM_00308;
		podata->VL53L1_PRM_00309 +=
				((int32_t)pidata->VL53L1_PRM_00129 / 2);
		podata->VL53L1_PRM_00309 /=
			(int32_t)pidata->VL53L1_PRM_00129;
	}

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00111(
	VL53L1_histogram_bin_data_t   *pdata)
{






	uint32_t  period        = 0;
	uint32_t  VL53L1_PRM_00094         = 0;

	LOG_FUNCTION_START("");

	period = 2048 *
		(uint32_t)VL53L1_FCTN_00128(pdata->VL53L1_PRM_00041);

	VL53L1_PRM_00094  = period;
	VL53L1_PRM_00094 += (uint32_t)pdata->VL53L1_PRM_00134;
	VL53L1_PRM_00094 += (2048 * (uint32_t)pdata->VL53L1_PRM_00135);
	VL53L1_PRM_00094 -= (2048 * (uint32_t)pdata->VL53L1_PRM_00140);

	VL53L1_PRM_00094  = VL53L1_PRM_00094 % period;

	pdata->VL53L1_PRM_00306 = (uint16_t)VL53L1_PRM_00094;

	LOG_FUNCTION_END(0);
}


void  VL53L1_FCTN_00107(
	VL53L1_DEV                     Dev,
	VL53L1_histogram_bin_data_t   *pdata)
{






	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	int32_t amb_thresh_low   = 0;
	int32_t amb_thresh_high  = 0;

	uint8_t i = 0;

	LOG_FUNCTION_START("");




	amb_thresh_low  = 1024 *
		(int32_t)pdev->VL53L1_PRM_00120.VL53L1_PRM_00270;
	amb_thresh_high = 1024 *
		(int32_t)pdev->VL53L1_PRM_00120.VL53L1_PRM_00271;

	if (pdev->VL53L1_PRM_00069.VL53L1_PRM_00145 == 0) {

		pdata->VL53L1_PRM_00151[5] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00258 >> 4;
		pdata->VL53L1_PRM_00151[4] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00258 & 0x0F;
		pdata->VL53L1_PRM_00151[3] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00257 >> 4;
		pdata->VL53L1_PRM_00151[2] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00257 & 0x0F;
		pdata->VL53L1_PRM_00151[1] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00256 >> 4;
		pdata->VL53L1_PRM_00151[0] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00256 & 0x0F;

		if (pdata->VL53L1_PRM_00308 > amb_thresh_high) {
			pdata->VL53L1_PRM_00151[5] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00266 >> 4;
			pdata->VL53L1_PRM_00151[4] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00266 & 0x0F;
			pdata->VL53L1_PRM_00151[3] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00265 >> 4;
			pdata->VL53L1_PRM_00151[2] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00265 & 0x0F;
			pdata->VL53L1_PRM_00151[1] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00264 >> 4;
			pdata->VL53L1_PRM_00151[0] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00264 & 0x0F;
		}

		if (pdata->VL53L1_PRM_00308 < amb_thresh_low) {
			pdata->VL53L1_PRM_00151[5] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00252 >> 4;
			pdata->VL53L1_PRM_00151[4] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00252 & 0x0F;
			pdata->VL53L1_PRM_00151[3] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00251 >> 4;
			pdata->VL53L1_PRM_00151[2] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00251 & 0x0F;
			pdata->VL53L1_PRM_00151[1] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00250 >> 4;
			pdata->VL53L1_PRM_00151[0] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00250 & 0x0F;
		}

	} else {

		pdata->VL53L1_PRM_00151[5] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00262 & 0x0F;
		pdata->VL53L1_PRM_00151[4] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00261 & 0x0F;
		pdata->VL53L1_PRM_00151[3] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00261 >> 4;
		pdata->VL53L1_PRM_00151[2] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00260 & 0x0F;
		pdata->VL53L1_PRM_00151[1] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00259 >> 4;
		pdata->VL53L1_PRM_00151[0] =
			pdev->VL53L1_PRM_00120.VL53L1_PRM_00259 & 0x0F;

		if (pdata->VL53L1_PRM_00308 > amb_thresh_high) {
			pdata->VL53L1_PRM_00151[5] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00269 >> 4;
			pdata->VL53L1_PRM_00151[4] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00269 & 0x0F;
			pdata->VL53L1_PRM_00151[3] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00268 >> 4;
			pdata->VL53L1_PRM_00151[2] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00268 & 0x0F;
			pdata->VL53L1_PRM_00151[1] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00267 >> 4;
			pdata->VL53L1_PRM_00151[0] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00267 & 0x0F;
		}

		if (pdata->VL53L1_PRM_00308 < amb_thresh_low) {
			pdata->VL53L1_PRM_00151[5] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00255 >> 4;
			pdata->VL53L1_PRM_00151[4] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00255 & 0x0F;
			pdata->VL53L1_PRM_00151[3] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00254 >> 4;
			pdata->VL53L1_PRM_00151[2] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00254 & 0x0F;
			pdata->VL53L1_PRM_00151[1] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00253 >> 4;
			pdata->VL53L1_PRM_00151[0] =
				pdev->VL53L1_PRM_00120.VL53L1_PRM_00253 & 0x0F;
		}
	}




	for (i = 0 ; i < VL53L1_DEF_00153 ; i++)
		pdata->VL53L1_PRM_00310[i] = 1;

	LOG_FUNCTION_END(0);

}


VL53L1_Error  VL53L1_FCTN_00101(
	VL53L1_DEV                Dev,
	VL53L1_range_results_t   *previous,
	VL53L1_range_results_t   *pcurrent)
{







	VL53L1_Error  status = VL53L1_ERROR_NONE;
	VL53L1_LLDriverData_t *pdev = VL53L1DevStructGetLLDriverHandle(Dev);

	uint8_t   VL53L1_PRM_00039 = 0;
	uint8_t   p = 0;

	uint16_t  phase_delta     = 0;
	uint16_t  phase_tolerance = 0;

	LOG_FUNCTION_START("");




	phase_tolerance =
		(uint16_t)pdev->VL53L1_PRM_00097.VL53L1_PRM_00203;
	phase_tolerance = phase_tolerance << 8;




	if (previous->VL53L1_PRM_00038 != VL53L1_DEF_00029 &&
		previous->VL53L1_PRM_00038 != VL53L1_DEF_00030)
		return status;




	if (phase_tolerance == 0)
		return status;

	for (VL53L1_PRM_00039 = 0 ; VL53L1_PRM_00039 < pcurrent->VL53L1_PRM_00037 ; VL53L1_PRM_00039++) {




		pcurrent->VL53L1_PRM_00035[VL53L1_PRM_00039].VL53L1_PRM_00025 =
			VL53L1_DEF_00023;







		for (p = 0 ; p < previous->VL53L1_PRM_00037 ; p++) {

			if (pcurrent->VL53L1_PRM_00035[VL53L1_PRM_00039].VL53L1_PRM_00168 >
				previous->VL53L1_PRM_00035[p].VL53L1_PRM_00168) {
				phase_delta =
					pcurrent->VL53L1_PRM_00035[VL53L1_PRM_00039].VL53L1_PRM_00168 -
					previous->VL53L1_PRM_00035[p].VL53L1_PRM_00168;
			} else {
				phase_delta =
					previous->VL53L1_PRM_00035[p].VL53L1_PRM_00168 -
					pcurrent->VL53L1_PRM_00035[VL53L1_PRM_00039].VL53L1_PRM_00168;
			}

			if (phase_delta < phase_tolerance)
				pcurrent->VL53L1_PRM_00035[VL53L1_PRM_00039].VL53L1_PRM_00025 =
					VL53L1_DEF_00018;
		}
	}

	LOG_FUNCTION_END(status);

	return status;
}


void VL53L1_FCTN_00052(
	uint8_t  spad_number,
	uint8_t  *prow,
	uint8_t  *pcol)
{







	if (spad_number > 127) {
		*prow = 8 + ((255-spad_number) & 0x07);
		*pcol = (spad_number-128) >> 3;
	} else {
		*prow = spad_number & 0x07;
		*pcol = (127-spad_number) >> 3;
	}

}


void VL53L1_FCTN_00050(
	uint8_t  row,
	uint8_t  col,
	uint8_t *pspad_number)
{





	if (row > 7)
		*pspad_number = 128 + (col << 3) + (15-row);
	else
		*pspad_number =  ((15-col) << 3)     + row;

}


void VL53L1_FCTN_00102(
	VL53L1_histogram_bin_data_t      *pbins,
	VL53L1_range_results_t           *phist,
	VL53L1_system_results_t          *psys,
	VL53L1_core_results_t            *pcore)
{





	uint8_t  i = 0;

	VL53L1_range_data_t  *pdata;

	LOG_FUNCTION_START("");




	VL53L1_FCTN_00115(psys);




	psys->VL53L1_PRM_00132 = pbins->VL53L1_PRM_00132;
	psys->VL53L1_PRM_00105     = phist->VL53L1_PRM_00037;
	psys->VL53L1_PRM_00106    = pbins->VL53L1_PRM_00106;
	psys->VL53L1_PRM_00026     = pbins->VL53L1_PRM_00026;

	pdata = &(phist->VL53L1_PRM_00035[0]);

	for (i = 0 ; i < phist->VL53L1_PRM_00037 ; i++) {

		switch (i) {

		case 0:

			psys->VL53L1_PRM_00162 = \
					pdata->VL53L1_PRM_00032;
			psys->VL53L1_PRM_00297 = \
					pdata->VL53L1_PRM_00030;
			psys->VL53L1_PRM_00165 = \
					pdata->VL53L1_PRM_00164;
			psys->VL53L1_PRM_00166 = \
					pdata->VL53L1_PRM_00031;

			psys->VL53L1_PRM_00167 = pdata->VL53L1_PRM_00033;
			psys->VL53L1_PRM_00169 = pdata->VL53L1_PRM_00168;

			psys->VL53L1_PRM_00170 = \
					(uint16_t)pdata->VL53L1_PRM_00034;

			psys->VL53L1_PRM_00181  = pdata->VL53L1_PRM_00306;

			pcore->VL53L1_PRM_00172 = \
					pdata->VL53L1_PRM_00171;
			pcore->VL53L1_PRM_00173 = \
					pdata->VL53L1_PRM_00072;
			pcore->VL53L1_PRM_00174 = \
					pdata->VL53L1_PRM_00152;
			pcore->VL53L1_PRM_00176 = \
					pdata->VL53L1_PRM_00175;

		break;
		case 1:

			psys->VL53L1_PRM_00177 = \
				pdata->VL53L1_PRM_00032;
			psys->VL53L1_PRM_00178 = \
				pdata->VL53L1_PRM_00030;
			psys->VL53L1_PRM_00179 = \
				pdata->VL53L1_PRM_00031;

			psys->VL53L1_PRM_00180 = pdata->VL53L1_PRM_00033;
			psys->VL53L1_PRM_00181 = pdata->VL53L1_PRM_00168;

			psys->VL53L1_PRM_00182 = \
				(uint16_t)pdata->VL53L1_PRM_00034;

			pcore->VL53L1_PRM_00183 = \
				pdata->VL53L1_PRM_00171;
			pcore->VL53L1_PRM_00184 = \
				pdata->VL53L1_PRM_00072;
			pcore->VL53L1_PRM_00185 = \
				pdata->VL53L1_PRM_00152;
			pcore->VL53L1_PRM_00186 = \
				pdata->VL53L1_PRM_00175;

		break;

		}

		pdata++;
	}

	LOG_FUNCTION_END(0);

}


VL53L1_Error VL53L1_FCTN_00024 (
		VL53L1_histogram_bin_data_t *phist_input,
		VL53L1_histogram_bin_data_t *phist_output)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t i = 0;
	uint8_t smallest_bin_num = 0;

	LOG_FUNCTION_START("");




	if (status == VL53L1_ERROR_NONE) {
		if (phist_output->VL53L1_PRM_00139 >= phist_input->VL53L1_PRM_00139)
			smallest_bin_num = phist_input->VL53L1_PRM_00139;
		else
			smallest_bin_num = phist_output->VL53L1_PRM_00139;
	}








	if (status == VL53L1_ERROR_NONE) {
        for(i = 0 ; i < smallest_bin_num ; i ++){




	        phist_output->VL53L1_PRM_00136[i] += phist_input->VL53L1_PRM_00136[i];
       }
	}

	if (status == VL53L1_ERROR_NONE)
        phist_output->VL53L1_PRM_00309 +=
    		phist_input->VL53L1_PRM_00309;

    LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00026(
	    	uint8_t VL53L1_PRM_00064,
	    	VL53L1_histogram_bin_data_t *phist_sum,
	    	VL53L1_histogram_bin_data_t *phist_avg)
{







	VL53L1_Error status = VL53L1_ERROR_NONE;

	uint8_t i = 0;

	LOG_FUNCTION_START("");








	if (status == VL53L1_ERROR_NONE) {
        for(i = 0 ; i < phist_sum->VL53L1_PRM_00139 ; i ++){




        	if (VL53L1_PRM_00064 > 0)
	            phist_avg->VL53L1_PRM_00136[i] = phist_sum->VL53L1_PRM_00136[i] /
	                (int32_t)VL53L1_PRM_00064;
        	else
        		phist_avg->VL53L1_PRM_00136[i] = phist_sum->VL53L1_PRM_00136[i];
       }
	}

	if (status == VL53L1_ERROR_NONE) {
		if (VL53L1_PRM_00064 > 0)
			phist_avg->VL53L1_PRM_00309 =
			    		phist_sum->VL53L1_PRM_00309 / (int32_t)VL53L1_PRM_00064;
		else
			phist_avg->VL53L1_PRM_00309 =
					phist_sum->VL53L1_PRM_00309;
	}


    LOG_FUNCTION_END(status);

	return status;
}


uint32_t VL53L1_FCTN_00142(uint32_t num)
{








	uint32_t  res = 0;
	uint32_t  bit = 1 << 30;




	while (bit > num)
		bit >>= 2;

	while (bit != 0) {
		if (num >= res + bit)  {
			num -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;
		bit >>= 2;
	}

	return res;
}


VL53L1_Error VL53L1_FCTN_00083(
	VL53L1_DEV  Dev)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	VL53L1_zone_private_dyn_cfg_t *pzone_dyn_cfg;
	VL53L1_dynamic_config_t       *pdynamic = &(pdev->VL53L1_PRM_00022);

	LOG_FUNCTION_START("");

	pzone_dyn_cfg = &(pres->VL53L1_PRM_00155.VL53L1_PRM_00035[pdev->VL53L1_PRM_00069.VL53L1_PRM_00123]);

	pzone_dyn_cfg->VL53L1_PRM_00295 =
			pdev->VL53L1_PRM_00069.VL53L1_PRM_00291;

	pzone_dyn_cfg->VL53L1_PRM_00296=
			pdev->VL53L1_PRM_00069.VL53L1_PRM_00124;

	pzone_dyn_cfg->VL53L1_PRM_00115 =
		pdynamic->VL53L1_PRM_00115;

	pzone_dyn_cfg->VL53L1_PRM_00116 =
		pdynamic->VL53L1_PRM_00116;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00103(
	VL53L1_DEV  Dev,
	VL53L1_range_results_t *presults)
{





	VL53L1_Error status = VL53L1_ERROR_NONE;

	VL53L1_LLDriverData_t  *pdev =
			VL53L1DevStructGetLLDriverHandle(Dev);
	VL53L1_LLDriverResults_t  *pres =
			VL53L1DevStructGetLLResultsHandle(Dev);

	uint8_t   VL53L1_PRM_00036 = pdev->VL53L1_PRM_00069.VL53L1_PRM_00070;
	uint8_t   i;
	uint16_t  max_total_rate_per_spads;
	uint16_t  target_rate = pdev->VL53L1_PRM_00104.VL53L1_PRM_00204;
	uint32_t  temp = 0;





	LOG_FUNCTION_START("");





	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: active zones: %u\n",
			pdev->VL53L1_PRM_00014.VL53L1_PRM_00020
      );

  pres->VL53L1_PRM_00155.VL53L1_PRM_00035[VL53L1_PRM_00036].VL53L1_PRM_00313 = 0;
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: peak signal count rate mcps: %u actual effective spads: %u\n",
      presults->VL53L1_PRM_00035[0].VL53L1_PRM_00030,
      presults->VL53L1_PRM_00035[0].VL53L1_PRM_00032
      );

  pres->VL53L1_PRM_00155.VL53L1_PRM_00035[VL53L1_PRM_00036].VL53L1_PRM_00313 = 0;
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: active results: %u\n",
      presults->VL53L1_PRM_00037
      );

  max_total_rate_per_spads = presults->VL53L1_PRM_00035[0].VL53L1_PRM_00314;
	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: max total rate per spad at start: %u\n",
      max_total_rate_per_spads
      );

  for(i = 1; i < presults->VL53L1_PRM_00037; i++) {
	  trace_print(VL53L1_TRACE_LEVEL_DEBUG,
	  		"    DYNZONEUPDATE: zone total rate per spad: VL53L1_PRM_00036: %u, total rate per spad: %u\n",
        i,
        presults->VL53L1_PRM_00035[i].VL53L1_PRM_00314
        );
    if(presults->VL53L1_PRM_00035[i].VL53L1_PRM_00314 > max_total_rate_per_spads)
      max_total_rate_per_spads = presults->VL53L1_PRM_00035[i].VL53L1_PRM_00314;
  }

  if(max_total_rate_per_spads == 0) {
    status = VL53L1_ERROR_DIVISION_BY_ZERO;
  } else {




    temp = target_rate << 14;
	  trace_print(VL53L1_TRACE_LEVEL_DEBUG,
	  		"    DYNZONEUPDATE: 1: temp: %u\n",
        temp
        );


    temp = temp / max_total_rate_per_spads;
	  trace_print(VL53L1_TRACE_LEVEL_DEBUG,
	  		"    DYNZONEUPDATE: 2: temp: %u\n",
        temp
        );


    temp = temp & 0xFFFF;
	  trace_print(VL53L1_TRACE_LEVEL_DEBUG,
	  		"    DYNZONEUPDATE: 3: temp: %u\n",
        temp
        );

    pres->VL53L1_PRM_00155.VL53L1_PRM_00035[VL53L1_PRM_00036].VL53L1_PRM_00313 =
            (uint16_t)temp;
  }

	trace_print(VL53L1_TRACE_LEVEL_DEBUG,
			"    DYNZONEUPDATE: VL53L1_PRM_00036: %u, target_rate: %u, max_total_rate_per_spads: %u, requested_spads: %u\n",
      VL53L1_PRM_00036,
      target_rate,
			max_total_rate_per_spads,
      pres->VL53L1_PRM_00155.VL53L1_PRM_00035[VL53L1_PRM_00036].VL53L1_PRM_00313
      );

  LOG_FUNCTION_END(status);

	return status;
}
