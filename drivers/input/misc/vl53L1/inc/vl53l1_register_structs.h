
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

#define VL53L1_DEF_00068               VL53L1_DEF_00004
#define VL53L1_DEF_00069             VL53L1_DEF_00032
#define VL53L1_DEF_00071                    VL53L1_DEF_00155
#define VL53L1_DEF_00073                   VL53L1_DEF_00156
#define VL53L1_DEF_00075                    VL53L1_DEF_00157
#define VL53L1_DEF_00077                   VL53L1_DEF_00158
#define VL53L1_DEF_00078                   VL53L1_DEF_00138
#define VL53L1_DEF_00087                   VL53L1_DEF_00159
#define VL53L1_DEF_00091                     VL53L1_DEF_00160
#define VL53L1_DEF_00088                    VL53L1_DEF_00097
#define VL53L1_DEF_01412                    VL53L1_DEF_00162
#define VL53L1_DEF_01413       VL53L1_DEF_00164
#define VL53L1_DEF_01414         VL53L1_DEF_00166
#define VL53L1_DEF_01415                      VL53L1_DEF_00168
#define VL53L1_DEF_01416               VL53L1_DEF_00170
#define VL53L1_DEF_01417                VL53L1_DEF_00172
#define VL53L1_DEF_01418                VL53L1_DEF_00174
#define VL53L1_DEF_01419                      VL53L1_DEF_00176
#define VL53L1_DEF_01420                    VL53L1_DEF_00178
#define VL53L1_DEF_01421            VL53L1_DEF_00180
#define VL53L1_DEF_01422              VL53L1_DEF_00182

#define VL53L1_DEF_00080           11
#define VL53L1_DEF_00081         23
#define VL53L1_DEF_00082                32
#define VL53L1_DEF_00083               22
#define VL53L1_DEF_00084                23
#define VL53L1_DEF_00085               18
#define VL53L1_DEF_00079                5
#define VL53L1_DEF_00093               44
#define VL53L1_DEF_00092                 33
#define VL53L1_DEF_00089                56
#define VL53L1_DEF_00161                49
#define VL53L1_DEF_00163   44
#define VL53L1_DEF_00165     33
#define VL53L1_DEF_00167                   2
#define VL53L1_DEF_00169            5
#define VL53L1_DEF_00171             6
#define VL53L1_DEF_00173            16
#define VL53L1_DEF_00175                   2
#define VL53L1_DEF_00177                90
#define VL53L1_DEF_00179        82
#define VL53L1_DEF_00181          33












typedef struct {
	uint8_t   VL53L1_PRM_00274;










	uint8_t   VL53L1_PRM_00275;










	uint8_t   VL53L1_PRM_00276;










	uint8_t   VL53L1_PRM_00277;










	uint8_t   VL53L1_PRM_00278;










	uint16_t  VL53L1_PRM_00012;










	uint8_t   VL53L1_PRM_00279;











	uint8_t   VL53L1_PRM_00280;










	uint8_t   VL53L1_PRM_00281;










	uint8_t   VL53L1_PRM_00282;











} VL53L1_static_nvm_managed_t;












typedef struct {
	uint8_t   VL53L1_PRM_00052;










	uint8_t   VL53L1_PRM_00053;










	uint8_t   VL53L1_PRM_00054;










	uint8_t   VL53L1_PRM_00055;










	uint8_t   VL53L1_PRM_00056;










	uint8_t   VL53L1_PRM_00057;










	uint8_t   VL53L1_PRM_00283;










	uint8_t   VL53L1_PRM_00050;










	uint8_t   VL53L1_PRM_00051;










	uint16_t  VL53L1_PRM_00072;










	int16_t   VL53L1_PRM_00070;










	int16_t   VL53L1_PRM_00071;










	uint16_t  VL53L1_PRM_00077;










	int16_t   VL53L1_PRM_00284;










	int16_t   VL53L1_PRM_00285;










	int16_t   VL53L1_PRM_00286;










} VL53L1_customer_nvm_managed_t;












typedef struct {
	uint16_t  VL53L1_PRM_00163;










	uint8_t   VL53L1_PRM_00164;










	uint8_t   VL53L1_PRM_00165;










	uint8_t   VL53L1_PRM_00166;













	uint8_t   VL53L1_PRM_00167;











	uint8_t   VL53L1_PRM_00168;










	uint8_t   VL53L1_PRM_00169;










	uint8_t   VL53L1_PRM_00170;










	uint8_t   VL53L1_PRM_00171;















	uint8_t   VL53L1_PRM_00172;










	uint8_t   VL53L1_PRM_00173;











	uint8_t   VL53L1_PRM_00078;











	uint8_t   VL53L1_PRM_00174;











	uint8_t   VL53L1_PRM_00175;










	uint8_t   VL53L1_PRM_00176;










	uint8_t   VL53L1_PRM_00114;










	uint8_t   VL53L1_PRM_00177;










	uint8_t   VL53L1_PRM_00178;










	uint8_t   VL53L1_PRM_00179;










	uint8_t   VL53L1_PRM_00159;










	uint8_t   VL53L1_PRM_00180;










	uint8_t   VL53L1_PRM_00181;










	uint8_t   VL53L1_PRM_00182;










	uint16_t  VL53L1_PRM_00183;










	uint8_t   VL53L1_PRM_00184;










	uint8_t   VL53L1_PRM_00185;











	uint8_t   VL53L1_PRM_00186;










	uint8_t   VL53L1_PRM_00187;










	uint8_t   VL53L1_PRM_00188;










	uint8_t   VL53L1_PRM_00189;











} VL53L1_static_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00190;










	uint8_t   VL53L1_PRM_00191;










	uint8_t   VL53L1_PRM_00192;















	uint8_t   VL53L1_PRM_00111;










	uint16_t  VL53L1_PRM_00193;










	uint8_t   VL53L1_PRM_00113;










	uint8_t   VL53L1_PRM_00075;










	uint8_t   VL53L1_PRM_00194;










	uint8_t   VL53L1_PRM_00195;










	uint8_t   VL53L1_PRM_00196;











	uint16_t  VL53L1_PRM_00197;










	uint16_t  VL53L1_PRM_00198;










	uint16_t  VL53L1_PRM_00199;










	uint8_t   VL53L1_PRM_00200;










	uint8_t   VL53L1_PRM_00201;










	uint8_t   VL53L1_PRM_00202;










	uint8_t   VL53L1_PRM_00203;










} VL53L1_general_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00204;










	uint8_t   VL53L1_PRM_00205;










	uint8_t   VL53L1_PRM_00206;










	uint8_t   VL53L1_PRM_00207;










	uint8_t   VL53L1_PRM_00117;










	uint8_t   VL53L1_PRM_00118;










	uint8_t   VL53L1_PRM_00076;










	uint8_t   VL53L1_PRM_00120;










	uint8_t   VL53L1_PRM_00121;










	uint8_t   VL53L1_PRM_00122;










	uint16_t  VL53L1_PRM_00014;










	uint16_t  VL53L1_PRM_00013;










	uint8_t   VL53L1_PRM_00208;










	uint8_t   VL53L1_PRM_00209;










	uint32_t  VL53L1_PRM_00090;










	uint8_t   VL53L1_PRM_00210;










} VL53L1_timing_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00234;











	uint16_t  VL53L1_PRM_00235;










	uint16_t  VL53L1_PRM_00236;










	uint8_t   VL53L1_PRM_00237;










	uint8_t   VL53L1_PRM_00238;











	uint8_t   VL53L1_PRM_00239;










	uint8_t   VL53L1_PRM_00240;










	uint8_t   VL53L1_PRM_00241;










	uint8_t   VL53L1_PRM_00242;










	uint8_t   VL53L1_PRM_00243;











	uint8_t   VL53L1_PRM_00244;











	uint8_t   VL53L1_PRM_00245;










	uint8_t   VL53L1_PRM_00091;










	uint8_t   VL53L1_PRM_00092;










	uint8_t   VL53L1_PRM_00024;

















	uint8_t   VL53L1_PRM_00246;











} VL53L1_dynamic_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00250;










	uint8_t   VL53L1_PRM_00247;










	uint8_t   VL53L1_PRM_00248;










	uint8_t   VL53L1_PRM_00249;











	uint8_t   VL53L1_PRM_00097;















} VL53L1_system_control_t;












typedef struct {
	uint8_t   VL53L1_PRM_00103;












	uint8_t   VL53L1_PRM_00080;













	uint8_t   VL53L1_PRM_00081;










	uint8_t   VL53L1_PRM_00027;










	uint16_t  VL53L1_PRM_00128;










	uint16_t  VL53L1_PRM_00129;










	uint16_t  VL53L1_PRM_00132;










	uint16_t  VL53L1_PRM_00133;










	uint16_t  VL53L1_PRM_00134;










	uint16_t  VL53L1_PRM_00135;










	uint16_t  VL53L1_PRM_00258;










	uint16_t  VL53L1_PRM_00259;










	uint16_t  VL53L1_PRM_00260;










	uint16_t  VL53L1_PRM_00131;










	uint16_t  VL53L1_PRM_00142;










	uint16_t  VL53L1_PRM_00143;










	uint16_t  VL53L1_PRM_00144;










	uint16_t  VL53L1_PRM_00145;










	uint16_t  VL53L1_PRM_00146;










	uint16_t  VL53L1_PRM_00147;










	uint16_t  VL53L1_PRM_00261;










	uint16_t  VL53L1_PRM_00262;










	uint16_t  VL53L1_PRM_00263;










	uint8_t   VL53L1_PRM_00264;










	uint8_t   VL53L1_PRM_00287;











} VL53L1_system_results_t;












typedef struct {
	uint32_t  VL53L1_PRM_00141;










	uint32_t  VL53L1_PRM_00137;










	int32_t   VL53L1_PRM_00138;










	uint32_t  VL53L1_PRM_00139;










	uint32_t  VL53L1_PRM_00151;










	uint32_t  VL53L1_PRM_00148;










	int32_t   VL53L1_PRM_00149;










	uint32_t  VL53L1_PRM_00150;










	uint8_t   VL53L1_PRM_00288;










} VL53L1_core_results_t;












typedef struct {
	uint16_t  VL53L1_PRM_00105;










	uint8_t   VL53L1_PRM_00106;










	uint8_t   VL53L1_PRM_00047;










	uint8_t   VL53L1_PRM_00048;










	uint8_t   VL53L1_PRM_00289;










	uint8_t   VL53L1_PRM_00290;










	uint8_t   VL53L1_PRM_00291;










	uint16_t  VL53L1_PRM_00089;










	uint8_t   VL53L1_PRM_00292;











	uint8_t   VL53L1_PRM_00293;











	uint8_t   VL53L1_PRM_00294;












	uint8_t   VL53L1_PRM_00295;












	uint8_t   VL53L1_PRM_00296;










	uint8_t   VL53L1_PRM_00297;











	uint8_t   VL53L1_PRM_00298;










	uint8_t   VL53L1_PRM_00299;










	uint16_t  VL53L1_PRM_00300;










	uint16_t  VL53L1_PRM_00301;










	uint16_t  VL53L1_PRM_00302;










	uint8_t   VL53L1_PRM_00303;










	uint8_t   VL53L1_PRM_00304;












	uint8_t   VL53L1_PRM_00305;










	uint8_t   VL53L1_PRM_00306;










	uint8_t   VL53L1_PRM_00307;










	uint8_t   VL53L1_PRM_00308;










	uint8_t   VL53L1_PRM_00309;











	uint8_t   VL53L1_PRM_00310;










	uint8_t   VL53L1_PRM_00311;










	uint8_t   VL53L1_PRM_00312;










	uint8_t   VL53L1_PRM_00313;

















	uint8_t   VL53L1_PRM_00314;










	uint8_t   VL53L1_PRM_00315;











	uint8_t   VL53L1_PRM_00269;














	uint8_t   VL53L1_PRM_00270;














	uint8_t   VL53L1_PRM_00271;














	uint8_t   VL53L1_PRM_00272;










	uint8_t   VL53L1_PRM_00273;










	uint8_t   VL53L1_PRM_00316;











	uint8_t   VL53L1_PRM_00317;













	uint32_t  VL53L1_PRM_00102;










	uint32_t  VL53L1_PRM_00318;










	uint8_t   VL53L1_PRM_00319;










	uint8_t   VL53L1_PRM_00320;










} VL53L1_debug_results_t;












typedef struct {
	uint8_t   VL53L1_PRM_00321;










	uint8_t   VL53L1_PRM_00005;










	uint8_t   VL53L1_PRM_00004;











	uint16_t  VL53L1_PRM_00322;










	uint8_t   VL53L1_PRM_00323;










	uint8_t   VL53L1_PRM_00324;










	uint8_t   VL53L1_PRM_00325;










	uint8_t   VL53L1_PRM_00326;










	uint8_t   VL53L1_PRM_00327;










	uint8_t   VL53L1_PRM_00328;










	uint8_t   VL53L1_PRM_00329;










	uint8_t   VL53L1_PRM_00330;










	uint8_t   VL53L1_PRM_00331;










	uint8_t   VL53L1_PRM_00332;










	uint8_t   VL53L1_PRM_00333;










	uint8_t   VL53L1_PRM_00334;










	uint8_t   VL53L1_PRM_00335;










	uint8_t   VL53L1_PRM_00336;










	uint8_t   VL53L1_PRM_00337;










	uint8_t   VL53L1_PRM_00338;










	uint8_t   VL53L1_PRM_00339;










	uint8_t   VL53L1_PRM_00340;










	uint8_t   VL53L1_PRM_00341;










	uint8_t   VL53L1_PRM_00342;










	uint8_t   VL53L1_PRM_00343;










	uint8_t   VL53L1_PRM_00344;










	uint8_t   VL53L1_PRM_00345;










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










	uint8_t   VL53L1_PRM_00359;










	uint8_t   VL53L1_PRM_00360;










	uint8_t   VL53L1_PRM_00361;










	uint8_t   VL53L1_PRM_00362;










	uint8_t   VL53L1_PRM_00363;










	uint8_t   VL53L1_PRM_00364;










	uint8_t   VL53L1_PRM_00093;










	uint8_t   VL53L1_PRM_00094;










} VL53L1_nvm_copy_data_t;












typedef struct {
	uint8_t   VL53L1_PRM_00365;












	uint8_t   VL53L1_PRM_00366;













	uint8_t   VL53L1_PRM_00367;










	uint8_t   VL53L1_PRM_00368;










	uint16_t  VL53L1_PRM_00369;










	uint16_t  VL53L1_PRM_00370;










	uint16_t  VL53L1_PRM_00371;










	uint16_t  VL53L1_PRM_00372;










	uint16_t  VL53L1_PRM_00373;










	uint16_t  VL53L1_PRM_00374;










	uint16_t  VL53L1_PRM_00375;










	uint16_t  VL53L1_PRM_00376;










	uint16_t  VL53L1_PRM_00377;










	uint16_t  VL53L1_PRM_00378;










	uint16_t  VL53L1_PRM_00379;










	uint16_t  VL53L1_PRM_00380;










	uint16_t  VL53L1_PRM_00381;










	uint16_t  VL53L1_PRM_00382;










	uint16_t  VL53L1_PRM_00383;










	uint16_t  VL53L1_PRM_00384;










	uint16_t  VL53L1_PRM_00385;










	uint16_t  VL53L1_PRM_00386;










	uint16_t  VL53L1_PRM_00387;










	uint16_t  VL53L1_PRM_00388;










} VL53L1_prev_shadow_system_results_t;












typedef struct {
	uint32_t  VL53L1_PRM_00389;










	uint32_t  VL53L1_PRM_00390;










	int32_t   VL53L1_PRM_00391;










	uint32_t  VL53L1_PRM_00392;










	uint32_t  VL53L1_PRM_00393;










	uint32_t  VL53L1_PRM_00394;










	int32_t   VL53L1_PRM_00395;










	uint32_t  VL53L1_PRM_00396;










	uint8_t   VL53L1_PRM_00397;










} VL53L1_prev_shadow_core_results_t;












typedef struct {
	uint8_t   VL53L1_PRM_00398;










	uint8_t   VL53L1_PRM_00399;










} VL53L1_patch_debug_t;












typedef struct {
	uint16_t  VL53L1_PRM_00400;










	uint16_t  VL53L1_PRM_00401;










	uint8_t   VL53L1_PRM_00402;















} VL53L1_gph_general_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00403;











	uint16_t  VL53L1_PRM_00404;










	uint8_t   VL53L1_PRM_00405;










	uint8_t   VL53L1_PRM_00406;










	uint8_t   VL53L1_PRM_00407;










} VL53L1_gph_static_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00408;










	uint8_t   VL53L1_PRM_00409;










	uint8_t   VL53L1_PRM_00410;










	uint8_t   VL53L1_PRM_00411;










	uint8_t   VL53L1_PRM_00412;










	uint8_t   VL53L1_PRM_00413;










	uint8_t   VL53L1_PRM_00414;










	uint8_t   VL53L1_PRM_00415;










	uint8_t   VL53L1_PRM_00416;










	uint8_t   VL53L1_PRM_00417;










	uint16_t  VL53L1_PRM_00418;










	uint16_t  VL53L1_PRM_00419;










	uint8_t   VL53L1_PRM_00420;










	uint8_t   VL53L1_PRM_00421;










} VL53L1_gph_timing_config_t;












typedef struct {
	uint8_t   VL53L1_PRM_00422;










	uint8_t   VL53L1_PRM_00423;










} VL53L1_fw_internal_t;












typedef struct {
	uint8_t   VL53L1_PRM_00424;











	uint8_t   VL53L1_PRM_00425;










	uint8_t   VL53L1_PRM_00426;










	uint8_t   VL53L1_PRM_00427;










	uint8_t   VL53L1_PRM_00428;










	uint8_t   VL53L1_PRM_00429;










	uint8_t   VL53L1_PRM_00430;










	uint8_t   VL53L1_PRM_00431;










	uint8_t   VL53L1_PRM_00432;










	uint8_t   VL53L1_PRM_00433;










	uint8_t   VL53L1_PRM_00434;










	uint8_t   VL53L1_PRM_00435;










	uint8_t   VL53L1_PRM_00436;










	uint8_t   VL53L1_PRM_00437;










	uint8_t   VL53L1_PRM_00438;










	uint8_t   VL53L1_PRM_00439;










	uint8_t   VL53L1_PRM_00440;










	uint8_t   VL53L1_PRM_00441;










	uint8_t   VL53L1_PRM_00442;










	uint8_t   VL53L1_PRM_00443;










	uint8_t   VL53L1_PRM_00444;










	uint8_t   VL53L1_PRM_00445;










	uint8_t   VL53L1_PRM_00446;










	uint8_t   VL53L1_PRM_00447;










	uint8_t   VL53L1_PRM_00448;










	uint8_t   VL53L1_PRM_00449;










	uint8_t   VL53L1_PRM_00450;










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










	uint8_t   VL53L1_PRM_00461;










	uint8_t   VL53L1_PRM_00462;










	uint8_t   VL53L1_PRM_00463;










	uint8_t   VL53L1_PRM_00464;










	uint8_t   VL53L1_PRM_00465;










	uint8_t   VL53L1_PRM_00466;










	uint8_t   VL53L1_PRM_00467;










	uint8_t   VL53L1_PRM_00468;










	uint16_t  VL53L1_PRM_00469;










	uint32_t  VL53L1_PRM_00470;










	uint16_t  VL53L1_PRM_00471;










	uint16_t  VL53L1_PRM_00472;










	uint8_t   VL53L1_PRM_00473;










	uint16_t  VL53L1_PRM_00474;










	uint16_t  VL53L1_PRM_00475;










	uint16_t  VL53L1_PRM_00476;










	uint16_t  VL53L1_PRM_00477;










	uint32_t  VL53L1_PRM_00478;










	uint32_t  VL53L1_PRM_00479;










	uint32_t  VL53L1_PRM_00480;










	uint32_t  VL53L1_PRM_00481;










	uint32_t  VL53L1_PRM_00482;










	uint16_t  VL53L1_PRM_00483;










} VL53L1_patch_results_t;












typedef struct {
	uint8_t   VL53L1_PRM_00484;










	uint8_t   VL53L1_PRM_00485;












	uint8_t   VL53L1_PRM_00486;













	uint8_t   VL53L1_PRM_00487;










	uint8_t   VL53L1_PRM_00488;










	uint16_t  VL53L1_PRM_00489;










	uint16_t  VL53L1_PRM_00490;










	uint16_t  VL53L1_PRM_00491;










	uint16_t  VL53L1_PRM_00492;










	uint16_t  VL53L1_PRM_00493;










	uint16_t  VL53L1_PRM_00494;










	uint16_t  VL53L1_PRM_00495;










	uint16_t  VL53L1_PRM_00496;










	uint16_t  VL53L1_PRM_00497;










	uint16_t  VL53L1_PRM_00498;










	uint16_t  VL53L1_PRM_00499;










	uint16_t  VL53L1_PRM_00500;










	uint16_t  VL53L1_PRM_00501;










	uint16_t  VL53L1_PRM_00502;










	uint16_t  VL53L1_PRM_00503;










	uint16_t  VL53L1_PRM_00504;










	uint16_t  VL53L1_PRM_00505;










	uint16_t  VL53L1_PRM_00506;










	uint16_t  VL53L1_PRM_00507;










	uint8_t   VL53L1_PRM_00508;










	uint8_t   VL53L1_PRM_00509;











	uint8_t   VL53L1_PRM_00510;










	uint8_t   VL53L1_PRM_00511;










} VL53L1_shadow_system_results_t;












typedef struct {
	uint32_t  VL53L1_PRM_00512;










	uint32_t  VL53L1_PRM_00513;










	int32_t   VL53L1_PRM_00514;










	uint32_t  VL53L1_PRM_00515;










	uint32_t  VL53L1_PRM_00516;










	uint32_t  VL53L1_PRM_00517;










	int32_t   VL53L1_PRM_00518;










	uint32_t  VL53L1_PRM_00519;










	uint8_t   VL53L1_PRM_00520;










} VL53L1_shadow_core_results_t;


#endif

