
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





































#ifndef _VL53L1_HIST_STRUCTS_H_
#define _VL53L1_HIST_STRUCTS_H_

#include "vl53l1_ll_device.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define  VL53L1_DEF_00153  6
#define  VL53L1_DEF_00163   15
#define  VL53L1_DEF_00035   24
#define  VL53L1_DEF_00225        12








typedef struct {

	uint8_t                          VL53L1_PRM_00272;

	uint8_t                          VL53L1_PRM_00250;
	uint8_t                          VL53L1_PRM_00251;
	uint8_t                          VL53L1_PRM_00252;

	uint8_t                          VL53L1_PRM_00253;
	uint8_t                          VL53L1_PRM_00254;
	uint8_t                          VL53L1_PRM_00255;

	uint8_t                          VL53L1_PRM_00256;
	uint8_t                          VL53L1_PRM_00257;
	uint8_t                          VL53L1_PRM_00258;

	uint8_t                          VL53L1_PRM_00259;
	uint8_t                          VL53L1_PRM_00260;
	uint8_t                          VL53L1_PRM_00261;
	uint8_t                          VL53L1_PRM_00262;

	uint8_t                          VL53L1_PRM_00263;

	uint8_t                          VL53L1_PRM_00264;
	uint8_t                          VL53L1_PRM_00265;
	uint8_t                          VL53L1_PRM_00266;

	uint8_t                          VL53L1_PRM_00267;
	uint8_t                          VL53L1_PRM_00268;
	uint8_t                          VL53L1_PRM_00269;

	uint16_t                         VL53L1_PRM_00270;



	uint16_t                         VL53L1_PRM_00271;




} VL53L1_histogram_config_t;









typedef struct {

	VL53L1_HistAlgoSelect  VL53L1_PRM_00189;


	uint8_t   VL53L1_PRM_00191;



	uint8_t   VL53L1_PRM_00192;




	VL53L1_HistAmbEstMethod VL53L1_PRM_00193;


	uint8_t   VL53L1_PRM_00194;



	uint8_t   VL53L1_PRM_00195;


	int32_t   VL53L1_PRM_00196;


	uint16_t  VL53L1_PRM_00197;



	int32_t   VL53L1_PRM_00198;


	uint8_t	  VL53L1_PRM_00200;


	uint16_t  VL53L1_PRM_00201;


	int16_t   VL53L1_PRM_00202;



	uint8_t   VL53L1_PRM_00203;




	uint16_t  VL53L1_PRM_00080;


	int16_t   VL53L1_PRM_00078;


	int16_t   VL53L1_PRM_00079;



	int16_t   VL53L1_PRM_00084;


	int16_t   VL53L1_PRM_00082;


	int16_t   VL53L1_PRM_00083;



} VL53L1_hist_post_process_config_t;







typedef struct {



	VL53L1_DeviceState     VL53L1_PRM_00130;


	VL53L1_DeviceState     VL53L1_PRM_00038;



	uint8_t  VL53L1_PRM_00036;


	uint32_t VL53L1_PRM_00024;



	uint8_t  VL53L1_PRM_00137;


	uint8_t  VL53L1_PRM_00138;


	uint8_t  VL53L1_PRM_00139;



	uint8_t  VL53L1_PRM_00129;



	uint8_t  VL53L1_PRM_00151[VL53L1_DEF_00153];


	uint8_t  VL53L1_PRM_00310[VL53L1_DEF_00153];




	int32_t  VL53L1_PRM_00136[VL53L1_DEF_00035];



	uint8_t  VL53L1_PRM_00132;


	uint8_t  VL53L1_PRM_00105;


	uint8_t  VL53L1_PRM_00106;


	uint8_t  VL53L1_PRM_00026;


	uint16_t VL53L1_PRM_00133;



	uint16_t VL53L1_PRM_00134;


	uint8_t  VL53L1_PRM_00135;


	uint8_t  VL53L1_PRM_00140;


	uint16_t VL53L1_PRM_00141;


	uint8_t  VL53L1_PRM_00041;


	uint16_t VL53L1_PRM_00144;


	uint32_t  VL53L1_PRM_00152;



	uint32_t VL53L1_PRM_00153;


	uint32_t VL53L1_PRM_00154;



	int32_t  VL53L1_PRM_00304;


	int32_t  VL53L1_PRM_00305;



	uint16_t VL53L1_PRM_00306;


	uint8_t  VL53L1_PRM_00307;


	int32_t  VL53L1_PRM_00308;


	int32_t  VL53L1_PRM_00309;



	uint8_t  VL53L1_PRM_00115;


	uint8_t  VL53L1_PRM_00116;



} VL53L1_histogram_bin_data_t;








typedef struct {



	uint8_t  VL53L1_PRM_00036;


	uint32_t VL53L1_PRM_00024;



	uint8_t  VL53L1_PRM_00137;


	uint8_t  VL53L1_PRM_00138;


	uint8_t  VL53L1_PRM_00139;


	uint32_t VL53L1_PRM_00136[VL53L1_DEF_00225];




	uint16_t VL53L1_PRM_00134;


	uint8_t  VL53L1_PRM_00135;


	uint8_t  VL53L1_PRM_00140;


	uint16_t VL53L1_PRM_00141;


	uint16_t VL53L1_PRM_00144;


	uint16_t VL53L1_PRM_00306;



} VL53L1_xtalk_histogram_data_t;



#ifdef __cplusplus
}
#endif

#endif
