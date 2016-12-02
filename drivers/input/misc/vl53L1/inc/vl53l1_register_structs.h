
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






































#ifndef _VL53L1_REGISTER_STRUCTS_H_
#define _VL53L1_REGISTER_STRUCTS_H_

#include "vl53l1_types.h"
#include "vl53l1_register_map.h"

#define VL53L1_DEF_00084               VL53L1_DEF_00001
#define VL53L1_DEF_00085             VL53L1_DEF_00034
#define VL53L1_DEF_00087                    VL53L1_DEF_00176
#define VL53L1_DEF_00089                   VL53L1_DEF_00177
#define VL53L1_DEF_00091                    VL53L1_DEF_00178
#define VL53L1_DEF_00093                   VL53L1_DEF_00179
#define VL53L1_DEF_00094                   VL53L1_DEF_00156
#define VL53L1_DEF_00103                   VL53L1_DEF_00180
#define VL53L1_DEF_00107                     VL53L1_DEF_00181
#define VL53L1_DEF_00104                    VL53L1_DEF_00113
#define VL53L1_DEF_01440                    VL53L1_DEF_00183
#define VL53L1_DEF_01441       VL53L1_DEF_00185
#define VL53L1_DEF_01442         VL53L1_DEF_00187
#define VL53L1_DEF_01443                      VL53L1_DEF_00189
#define VL53L1_DEF_01444               VL53L1_DEF_00191
#define VL53L1_DEF_01445                VL53L1_DEF_00193
#define VL53L1_DEF_01446                VL53L1_DEF_00195
#define VL53L1_DEF_01447                      VL53L1_DEF_00197
#define VL53L1_DEF_01448                    VL53L1_DEF_00199
#define VL53L1_DEF_01449            VL53L1_DEF_00201
#define VL53L1_DEF_01450              VL53L1_DEF_00203

#define VL53L1_DEF_00096           11
#define VL53L1_DEF_00097         23
#define VL53L1_DEF_00098                32
#define VL53L1_DEF_00099               22
#define VL53L1_DEF_00100                23
#define VL53L1_DEF_00101               18
#define VL53L1_DEF_00095                5
#define VL53L1_DEF_00109               44
#define VL53L1_DEF_00108                 33
#define VL53L1_DEF_00105                56
#define VL53L1_DEF_00182                49
#define VL53L1_DEF_00184   44
#define VL53L1_DEF_00186     33
#define VL53L1_DEF_00188                   2
#define VL53L1_DEF_00190            5
#define VL53L1_DEF_00192             6
#define VL53L1_DEF_00194            16
#define VL53L1_DEF_00196                   2
#define VL53L1_DEF_00198                90
#define VL53L1_DEF_00200        82
#define VL53L1_DEF_00202          33












typedef struct {
	uint8_t   VL53L1_PRM_00320;










	uint8_t   VL53L1_PRM_00321;










	uint8_t   VL53L1_PRM_00322;










	uint8_t   VL53L1_PRM_00323;










	uint8_t   VL53L1_PRM_00324;










	uint16_t  VL53L1_PRM_00011;










	uint8_t   VL53L1_PRM_00325;











	uint8_t   VL53L1_PRM_00326;










	uint8_t   VL53L1_PRM_00327;










	uint8_t   VL53L1_PRM_00328;











} VL53L1_static_nvm_managed_t;












typedef struct {
	uint8_t   VL53L1_PRM_00053;










	uint8_t   VL53L1_PRM_00054;










	uint8_t   VL53L1_PRM_00055;










	uint8_t   VL53L1_PRM_00056;










	uint8_t   VL53L1_PRM_00057;










	uint8_t   VL53L1_PRM_00058;










	uint8_t   VL53L1_PRM_00329;










	uint8_t   VL53L1_PRM_00051;










	uint8_t   VL53L1_PRM_00052;










	uint16_t  VL53L1_PRM_00080;










	int16_t   VL53L1_PRM_00078;










	int16_t   VL53L1_PRM_00079;










	uint16_t  VL53L1_PRM_00102;










	int16_t   VL53L1_PRM_00084;










	int16_t   VL53L1_PRM_00082;










	int16_t   VL53L1_PRM_00083;










} VL53L1_customer_nvm_managed_t;












typedef struct {
	uint16_t  VL53L1_PRM_00204;










	uint8_t   VL53L1_PRM_00205;










	uint8_t   VL53L1_PRM_00206;










	uint8_t   VL53L1_PRM_00207;













	uint8_t   VL53L1_PRM_00208;











	uint8_t   VL53L1_PRM_00209;










	uint8_t   VL53L1_PRM_00210;










	uint8_t   VL53L1_PRM_00211;










	uint8_t   VL53L1_PRM_00212;















	uint8_t   VL53L1_PRM_00213;










	uint8_t   VL53L1_PRM_00214;











	uint8_t   VL53L1_PRM_00103;











	uint8_t   VL53L1_PRM_00215;











	uint8_t   VL53L1_PRM_00216;










	uint8_t   VL53L1_PRM_00217;










	uint8_t   VL53L1_PRM_00143;










	uint8_t   VL53L1_PRM_00218;










	uint8_t   VL53L1_PRM_00219;










	uint8_t   VL53L1_PRM_00220;










	uint8_t   VL53L1_PRM_00200;










	uint8_t   VL53L1_PRM_00221;










	uint8_t   VL53L1_PRM_00222;










	uint8_t   VL53L1_PRM_00223;










	uint16_t  VL53L1_PRM_00224;










	uint8_t   VL53L1_PRM_00225;










	uint8_t   VL53L1_PRM_00226;











	uint8_t   VL53L1_PRM_00203;










	uint8_t   VL53L1_PRM_00227;










	uint8_t   VL53L1_PRM_00228;










	uint8_t   VL53L1_PRM_00229;











} VL53L1_static_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00230;










	uint8_t   VL53L1_PRM_00119;










	uint8_t   VL53L1_PRM_00231;















	uint8_t   VL53L1_PRM_00140;










	uint16_t  VL53L1_PRM_00232;










	uint8_t   VL53L1_PRM_00142;










	uint8_t   VL53L1_PRM_00100;










	uint8_t   VL53L1_PRM_00233;










	uint8_t   VL53L1_PRM_00234;










	uint8_t   VL53L1_PRM_00235;











	uint16_t  VL53L1_PRM_00236;










	uint16_t  VL53L1_PRM_00237;










	uint16_t  VL53L1_PRM_00238;










	uint8_t   VL53L1_PRM_00239;










	uint8_t   VL53L1_PRM_00240;










	uint8_t   VL53L1_PRM_00241;










	uint8_t   VL53L1_PRM_00242;










} VL53L1_general_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00243;










	uint8_t   VL53L1_PRM_00244;










	uint8_t   VL53L1_PRM_00245;










	uint8_t   VL53L1_PRM_00246;










	uint8_t   VL53L1_PRM_00146;










	uint8_t   VL53L1_PRM_00147;










	uint8_t   VL53L1_PRM_00101;










	uint8_t   VL53L1_PRM_00148;










	uint8_t   VL53L1_PRM_00149;










	uint8_t   VL53L1_PRM_00150;










	uint16_t  VL53L1_PRM_00013;










	uint16_t  VL53L1_PRM_00012;










	uint8_t   VL53L1_PRM_00247;










	uint8_t   VL53L1_PRM_00248;










	uint32_t  VL53L1_PRM_00114;










	uint8_t   VL53L1_PRM_00249;










} VL53L1_timing_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00125;











	uint16_t  VL53L1_PRM_00273;










	uint16_t  VL53L1_PRM_00274;










	uint8_t   VL53L1_PRM_00275;










	uint8_t   VL53L1_PRM_00276;











	uint8_t   VL53L1_PRM_00277;










	uint8_t   VL53L1_PRM_00278;










	uint8_t   VL53L1_PRM_00279;










	uint8_t   VL53L1_PRM_00280;










	uint8_t   VL53L1_PRM_00126;











	uint8_t   VL53L1_PRM_00281;











	uint8_t   VL53L1_PRM_00282;










	uint8_t   VL53L1_PRM_00115;










	uint8_t   VL53L1_PRM_00116;










	uint8_t   VL53L1_PRM_00023;

















	uint8_t   VL53L1_PRM_00127;











} VL53L1_dynamic_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00286;










	uint8_t   VL53L1_PRM_00283;










	uint8_t   VL53L1_PRM_00284;










	uint8_t   VL53L1_PRM_00285;











	uint8_t   VL53L1_PRM_00122;















} VL53L1_system_control_t;












typedef struct {
	uint8_t   VL53L1_PRM_00132;












	uint8_t   VL53L1_PRM_00105;













	uint8_t   VL53L1_PRM_00106;










	uint8_t   VL53L1_PRM_00026;










	uint16_t  VL53L1_PRM_00162;










	uint16_t  VL53L1_PRM_00297;










	uint16_t  VL53L1_PRM_00166;










	uint16_t  VL53L1_PRM_00167;










	uint16_t  VL53L1_PRM_00169;










	uint16_t  VL53L1_PRM_00170;










	uint16_t  VL53L1_PRM_00163;










	uint16_t  VL53L1_PRM_00298;










	uint16_t  VL53L1_PRM_00299;










	uint16_t  VL53L1_PRM_00165;










	uint16_t  VL53L1_PRM_00177;










	uint16_t  VL53L1_PRM_00178;










	uint16_t  VL53L1_PRM_00179;










	uint16_t  VL53L1_PRM_00180;










	uint16_t  VL53L1_PRM_00181;










	uint16_t  VL53L1_PRM_00182;










	uint16_t  VL53L1_PRM_00300;










	uint16_t  VL53L1_PRM_00301;










	uint16_t  VL53L1_PRM_00302;










	uint8_t   VL53L1_PRM_00303;










	uint8_t   VL53L1_PRM_00330;











} VL53L1_system_results_t;












typedef struct {
	uint32_t  VL53L1_PRM_00176;










	uint32_t  VL53L1_PRM_00172;










	int32_t   VL53L1_PRM_00173;










	uint32_t  VL53L1_PRM_00174;










	uint32_t  VL53L1_PRM_00186;










	uint32_t  VL53L1_PRM_00183;










	int32_t   VL53L1_PRM_00184;










	uint32_t  VL53L1_PRM_00185;










	uint8_t   VL53L1_PRM_00331;










} VL53L1_core_results_t;












typedef struct {
	uint16_t  VL53L1_PRM_00134;










	uint8_t   VL53L1_PRM_00135;










	uint8_t   VL53L1_PRM_00048;










	uint8_t   VL53L1_PRM_00049;










	uint8_t   VL53L1_PRM_00332;










	uint8_t   VL53L1_PRM_00333;










	uint8_t   VL53L1_PRM_00334;










	uint16_t  VL53L1_PRM_00113;










	uint8_t   VL53L1_PRM_00335;











	uint8_t   VL53L1_PRM_00336;











	uint8_t   VL53L1_PRM_00337;












	uint8_t   VL53L1_PRM_00338;












	uint8_t   VL53L1_PRM_00339;










	uint8_t   VL53L1_PRM_00340;











	uint8_t   VL53L1_PRM_00341;










	uint8_t   VL53L1_PRM_00342;










	uint16_t  VL53L1_PRM_00343;










	uint16_t  VL53L1_PRM_00344;










	uint16_t  VL53L1_PRM_00345;










	uint8_t   VL53L1_PRM_00346;










	uint8_t   VL53L1_PRM_00347;












	uint8_t   VL53L1_PRM_00348;










	uint8_t   VL53L1_PRM_00349;










	uint8_t   VL53L1_PRM_00350;










	uint8_t   VL53L1_PRM_00351;










	uint8_t   VL53L1_PRM_00352;











	uint8_t   VL53L1_PRM_00353;










	uint8_t   VL53L1_PRM_00354;










	uint8_t   VL53L1_PRM_00355;










	uint8_t   VL53L1_PRM_00356;

















	uint8_t   VL53L1_PRM_00357;










	uint8_t   VL53L1_PRM_00358;











	uint8_t   VL53L1_PRM_00315;














	uint8_t   VL53L1_PRM_00316;














	uint8_t   VL53L1_PRM_00317;














	uint8_t   VL53L1_PRM_00318;










	uint8_t   VL53L1_PRM_00319;










	uint8_t   VL53L1_PRM_00359;











	uint8_t   VL53L1_PRM_00360;













	uint32_t  VL53L1_PRM_00131;










	uint32_t  VL53L1_PRM_00361;










	uint8_t   VL53L1_PRM_00362;










	uint8_t   VL53L1_PRM_00363;










} VL53L1_debug_results_t;












typedef struct {
	uint8_t   VL53L1_PRM_00364;










	uint8_t   VL53L1_PRM_00004;










	uint8_t   VL53L1_PRM_00003;











	uint16_t  VL53L1_PRM_00365;










	uint8_t   VL53L1_PRM_00366;










	uint8_t   VL53L1_PRM_00367;










	uint8_t   VL53L1_PRM_00368;










	uint8_t   VL53L1_PRM_00369;










	uint8_t   VL53L1_PRM_00370;










	uint8_t   VL53L1_PRM_00371;










	uint8_t   VL53L1_PRM_00372;










	uint8_t   VL53L1_PRM_00373;










	uint8_t   VL53L1_PRM_00374;










	uint8_t   VL53L1_PRM_00375;










	uint8_t   VL53L1_PRM_00376;










	uint8_t   VL53L1_PRM_00377;










	uint8_t   VL53L1_PRM_00378;










	uint8_t   VL53L1_PRM_00379;










	uint8_t   VL53L1_PRM_00380;










	uint8_t   VL53L1_PRM_00381;










	uint8_t   VL53L1_PRM_00382;










	uint8_t   VL53L1_PRM_00383;










	uint8_t   VL53L1_PRM_00384;










	uint8_t   VL53L1_PRM_00385;










	uint8_t   VL53L1_PRM_00386;










	uint8_t   VL53L1_PRM_00387;










	uint8_t   VL53L1_PRM_00388;










	uint8_t   VL53L1_PRM_00389;










	uint8_t   VL53L1_PRM_00390;










	uint8_t   VL53L1_PRM_00391;










	uint8_t   VL53L1_PRM_00392;










	uint8_t   VL53L1_PRM_00393;










	uint8_t   VL53L1_PRM_00394;










	uint8_t   VL53L1_PRM_00395;










	uint8_t   VL53L1_PRM_00396;










	uint8_t   VL53L1_PRM_00397;










	uint8_t   VL53L1_PRM_00398;










	uint8_t   VL53L1_PRM_00399;










	uint8_t   VL53L1_PRM_00400;










	uint8_t   VL53L1_PRM_00401;










	uint8_t   VL53L1_PRM_00402;










	uint8_t   VL53L1_PRM_00403;










	uint8_t   VL53L1_PRM_00404;










	uint8_t   VL53L1_PRM_00405;










	uint8_t   VL53L1_PRM_00406;










	uint8_t   VL53L1_PRM_00407;










	uint8_t   VL53L1_PRM_00117;










	uint8_t   VL53L1_PRM_00118;










} VL53L1_nvm_copy_data_t;












typedef struct {
	uint8_t   VL53L1_PRM_00408;












	uint8_t   VL53L1_PRM_00409;













	uint8_t   VL53L1_PRM_00410;










	uint8_t   VL53L1_PRM_00411;










	uint16_t  VL53L1_PRM_00412;










	uint16_t  VL53L1_PRM_00413;










	uint16_t  VL53L1_PRM_00414;










	uint16_t  VL53L1_PRM_00415;










	uint16_t  VL53L1_PRM_00416;










	uint16_t  VL53L1_PRM_00417;










	uint16_t  VL53L1_PRM_00418;










	uint16_t  VL53L1_PRM_00419;










	uint16_t  VL53L1_PRM_00420;










	uint16_t  VL53L1_PRM_00421;










	uint16_t  VL53L1_PRM_00422;










	uint16_t  VL53L1_PRM_00423;










	uint16_t  VL53L1_PRM_00424;










	uint16_t  VL53L1_PRM_00425;










	uint16_t  VL53L1_PRM_00426;










	uint16_t  VL53L1_PRM_00427;










	uint16_t  VL53L1_PRM_00428;










	uint16_t  VL53L1_PRM_00429;










	uint16_t  VL53L1_PRM_00430;










	uint16_t  VL53L1_PRM_00431;










} VL53L1_prev_shadow_system_results_t;












typedef struct {
	uint32_t  VL53L1_PRM_00432;










	uint32_t  VL53L1_PRM_00433;










	int32_t   VL53L1_PRM_00434;










	uint32_t  VL53L1_PRM_00435;










	uint32_t  VL53L1_PRM_00436;










	uint32_t  VL53L1_PRM_00437;










	int32_t   VL53L1_PRM_00438;










	uint32_t  VL53L1_PRM_00439;










	uint8_t   VL53L1_PRM_00440;










} VL53L1_prev_shadow_core_results_t;












typedef struct {
	uint8_t   VL53L1_PRM_00441;










	uint8_t   VL53L1_PRM_00442;










} VL53L1_patch_debug_t;












typedef struct {
	uint16_t  VL53L1_PRM_00443;










	uint16_t  VL53L1_PRM_00444;










	uint8_t   VL53L1_PRM_00445;















} VL53L1_gph_general_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00446;











	uint16_t  VL53L1_PRM_00447;










	uint8_t   VL53L1_PRM_00448;










	uint8_t   VL53L1_PRM_00449;










	uint8_t   VL53L1_PRM_00450;










} VL53L1_gph_static_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00451;










	uint8_t   VL53L1_PRM_00452;










	uint8_t   VL53L1_PRM_00453;










	uint8_t   VL53L1_PRM_00454;










	uint8_t   VL53L1_PRM_00455;










	uint8_t   VL53L1_PRM_00456;










	uint8_t   VL53L1_PRM_00457;










	uint8_t   VL53L1_PRM_00458;










	uint8_t   VL53L1_PRM_00459;










	uint8_t   VL53L1_PRM_00460;










	uint16_t  VL53L1_PRM_00461;










	uint16_t  VL53L1_PRM_00462;










	uint8_t   VL53L1_PRM_00463;










	uint8_t   VL53L1_PRM_00464;










} VL53L1_gph_timing_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00465;










	uint8_t   VL53L1_PRM_00466;










} VL53L1_fw_internal_t;












typedef struct {
	uint8_t   VL53L1_PRM_00467;











	uint8_t   VL53L1_PRM_00468;










	uint8_t   VL53L1_PRM_00469;










	uint8_t   VL53L1_PRM_00470;










	uint8_t   VL53L1_PRM_00471;










	uint8_t   VL53L1_PRM_00472;










	uint8_t   VL53L1_PRM_00473;










	uint8_t   VL53L1_PRM_00474;










	uint8_t   VL53L1_PRM_00475;










	uint8_t   VL53L1_PRM_00476;










	uint8_t   VL53L1_PRM_00477;










	uint8_t   VL53L1_PRM_00478;










	uint8_t   VL53L1_PRM_00479;










	uint8_t   VL53L1_PRM_00480;










	uint8_t   VL53L1_PRM_00481;










	uint8_t   VL53L1_PRM_00482;










	uint8_t   VL53L1_PRM_00483;










	uint8_t   VL53L1_PRM_00484;










	uint8_t   VL53L1_PRM_00485;










	uint8_t   VL53L1_PRM_00486;










	uint8_t   VL53L1_PRM_00487;










	uint8_t   VL53L1_PRM_00488;










	uint8_t   VL53L1_PRM_00489;










	uint8_t   VL53L1_PRM_00490;










	uint8_t   VL53L1_PRM_00491;










	uint8_t   VL53L1_PRM_00492;










	uint8_t   VL53L1_PRM_00493;










	uint8_t   VL53L1_PRM_00494;










	uint8_t   VL53L1_PRM_00495;










	uint8_t   VL53L1_PRM_00496;










	uint8_t   VL53L1_PRM_00497;










	uint8_t   VL53L1_PRM_00498;










	uint8_t   VL53L1_PRM_00499;










	uint8_t   VL53L1_PRM_00500;










	uint8_t   VL53L1_PRM_00501;










	uint8_t   VL53L1_PRM_00502;










	uint8_t   VL53L1_PRM_00503;










	uint8_t   VL53L1_PRM_00504;










	uint8_t   VL53L1_PRM_00505;










	uint8_t   VL53L1_PRM_00506;










	uint8_t   VL53L1_PRM_00507;










	uint8_t   VL53L1_PRM_00508;










	uint8_t   VL53L1_PRM_00509;










	uint8_t   VL53L1_PRM_00510;










	uint8_t   VL53L1_PRM_00511;










	uint16_t  VL53L1_PRM_00512;










	uint32_t  VL53L1_PRM_00513;










	uint16_t  VL53L1_PRM_00514;










	uint16_t  VL53L1_PRM_00515;










	uint8_t   VL53L1_PRM_00516;










	uint16_t  VL53L1_PRM_00517;










	uint16_t  VL53L1_PRM_00518;










	uint16_t  VL53L1_PRM_00519;










	uint16_t  VL53L1_PRM_00520;










	uint32_t  VL53L1_PRM_00521;










	uint32_t  VL53L1_PRM_00522;










	uint32_t  VL53L1_PRM_00523;










	uint32_t  VL53L1_PRM_00524;










	uint32_t  VL53L1_PRM_00525;










	uint16_t  VL53L1_PRM_00526;










} VL53L1_patch_results_t;












typedef struct {
	uint8_t   VL53L1_PRM_00527;










	uint8_t   VL53L1_PRM_00528;












	uint8_t   VL53L1_PRM_00529;













	uint8_t   VL53L1_PRM_00530;










	uint8_t   VL53L1_PRM_00531;










	uint16_t  VL53L1_PRM_00532;










	uint16_t  VL53L1_PRM_00533;










	uint16_t  VL53L1_PRM_00534;










	uint16_t  VL53L1_PRM_00535;










	uint16_t  VL53L1_PRM_00536;










	uint16_t  VL53L1_PRM_00537;










	uint16_t  VL53L1_PRM_00538;










	uint16_t  VL53L1_PRM_00539;










	uint16_t  VL53L1_PRM_00540;










	uint16_t  VL53L1_PRM_00541;










	uint16_t  VL53L1_PRM_00542;










	uint16_t  VL53L1_PRM_00543;










	uint16_t  VL53L1_PRM_00544;










	uint16_t  VL53L1_PRM_00545;










	uint16_t  VL53L1_PRM_00546;










	uint16_t  VL53L1_PRM_00547;










	uint16_t  VL53L1_PRM_00548;










	uint16_t  VL53L1_PRM_00549;










	uint16_t  VL53L1_PRM_00550;










	uint8_t   VL53L1_PRM_00551;










	uint8_t   VL53L1_PRM_00552;











	uint8_t   VL53L1_PRM_00553;










	uint8_t   VL53L1_PRM_00554;










} VL53L1_shadow_system_results_t;












typedef struct {
	uint32_t  VL53L1_PRM_00555;










	uint32_t  VL53L1_PRM_00556;










	int32_t   VL53L1_PRM_00557;










	uint32_t  VL53L1_PRM_00558;










	uint32_t  VL53L1_PRM_00559;










	uint32_t  VL53L1_PRM_00560;










	int32_t   VL53L1_PRM_00561;










	uint32_t  VL53L1_PRM_00562;










	uint8_t   VL53L1_PRM_00563;










} VL53L1_shadow_core_results_t;


#endif

