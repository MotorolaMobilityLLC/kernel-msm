
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

#include "vl53l1_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define  VL53L1_DEF_00135  6
#define  VL53L1_DEF_00052   24
#define  VL53L1_DEF_00205        12








typedef struct {

	uint8_t                          VL53L1_PRM_00233;

	uint8_t                          VL53L1_PRM_00211;
	uint8_t                          VL53L1_PRM_00212;
	uint8_t                          VL53L1_PRM_00213;

	uint8_t                          VL53L1_PRM_00214;
	uint8_t                          VL53L1_PRM_00215;
	uint8_t                          VL53L1_PRM_00216;

	uint8_t                          VL53L1_PRM_00217;
	uint8_t                          VL53L1_PRM_00218;
	uint8_t                          VL53L1_PRM_00219;

	uint8_t                          VL53L1_PRM_00220;
	uint8_t                          VL53L1_PRM_00221;
	uint8_t                          VL53L1_PRM_00222;
	uint8_t                          VL53L1_PRM_00223;

	uint8_t                          VL53L1_PRM_00224;

	uint8_t                          VL53L1_PRM_00225;
	uint8_t                          VL53L1_PRM_00226;
	uint8_t                          VL53L1_PRM_00227;

	uint8_t                          VL53L1_PRM_00228;
	uint8_t                          VL53L1_PRM_00229;
	uint8_t                          VL53L1_PRM_00230;

	uint16_t                         VL53L1_PRM_00231;
		


	uint16_t                         VL53L1_PRM_00232;
		



} VL53L1_histogram_config_t;









typedef struct {

	uint8_t    VL53L1_PRM_00152;
		

	uint8_t    VL53L1_PRM_00153;
		

	uint8_t    VL53L1_PRM_00154;
		

	uint8_t    VL53L1_PRM_00155;
		

	uint16_t   VL53L1_PRM_00156;
		

	int32_t    VL53L1_PRM_00157;
		

	uint8_t	   VL53L1_PRM_00159;
		

	uint16_t   VL53L1_PRM_00160;
		

	int16_t    VL53L1_PRM_00161;
		


} VL53L1_hist_post_process_config_t;







typedef struct {
	


	uint8_t  VL53L1_PRM_00036;
		

	uint32_t VL53L1_PRM_00025;
		


	uint8_t  VL53L1_PRM_00108;
		

	uint8_t  VL53L1_PRM_00109;
		

	uint8_t  VL53L1_PRM_00110;
		


	uint8_t  VL53L1_PRM_00119;
		


	uint8_t  VL53L1_PRM_00267[VL53L1_DEF_00135];
		

	int32_t  VL53L1_PRM_00107[VL53L1_DEF_00052];
		


	uint8_t  VL53L1_PRM_00103;
		

	uint8_t  VL53L1_PRM_00080;
		

	uint8_t  VL53L1_PRM_00081;
		

	uint8_t  VL53L1_PRM_00027;
		

	uint16_t VL53L1_PRM_00104;
		


	uint16_t VL53L1_PRM_00105;
		

	uint8_t  VL53L1_PRM_00106;
		

	uint8_t  VL53L1_PRM_00111;
		

	uint16_t VL53L1_PRM_00112;
		

	uint8_t  VL53L1_PRM_00040;
		

	uint16_t VL53L1_PRM_00115;
		

	uint32_t  VL53L1_PRM_00123;
		


	uint32_t VL53L1_PRM_00060;
	    

	uint32_t VL53L1_PRM_00124;
	    


	uint16_t VL53L1_PRM_00265;
		

	int32_t  VL53L1_PRM_00268;
		

	int32_t  VL53L1_PRM_00266;
		


} VL53L1_histogram_bin_data_t;








typedef struct {
	


	uint8_t  VL53L1_PRM_00036;
		

	uint32_t VL53L1_PRM_00025;
		


	uint8_t  VL53L1_PRM_00108;
		

	uint8_t  VL53L1_PRM_00109;
		

	uint8_t  VL53L1_PRM_00110;
		

	uint32_t VL53L1_PRM_00107[VL53L1_DEF_00205];
		



	uint16_t VL53L1_PRM_00105;
		

	uint8_t  VL53L1_PRM_00106;
		

	uint8_t  VL53L1_PRM_00111;
		

	uint16_t VL53L1_PRM_00112;
		

	uint16_t VL53L1_PRM_00115;
		

	uint16_t VL53L1_PRM_00265;
		


} VL53L1_xtalk_histogram_data_t;



#ifdef __cplusplus
}
#endif

#endif
