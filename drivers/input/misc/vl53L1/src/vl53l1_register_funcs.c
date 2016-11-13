
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
#include "vl53l1_platform_log.h"
#include "vl53l1_core.h"
#include "vl53l1_register_map.h"
#include "vl53l1_register_structs.h"
#include "vl53l1_register_funcs.h"

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(VL53L1_TRACE_MODULE_REGISTERS, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(VL53L1_TRACE_MODULE_REGISTERS, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(VL53L1_TRACE_MODULE_REGISTERS, status, fmt, ##__VA_ARGS__)

#define trace_print(level, ...) \
	VL53L1_trace_print_module_function(VL53L1_TRACE_MODULE_REGISTERS, level, VL53L1_TRACE_FUNCTION_NONE, ##__VA_ARGS__)


VL53L1_Error VL53L1_FCTN_00064(
	VL53L1_static_nvm_managed_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00080 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00274 & 0x7F;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00275 & 0xF;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00276 & 0x7F;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00277 & 0x3;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00278 & 0x7F;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00012,
		2,
		pbuffer +   5);
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00279;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00280;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00281 & 0x3F;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00282;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00116(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_nvm_managed_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00080 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00274 =
		(*(pbuffer +   0)) & 0x7F;
	pdata->VL53L1_PRM_00275 =
		(*(pbuffer +   1)) & 0xF;
	pdata->VL53L1_PRM_00276 =
		(*(pbuffer +   2)) & 0x7F;
	pdata->VL53L1_PRM_00277 =
		(*(pbuffer +   3)) & 0x3;
	pdata->VL53L1_PRM_00278 =
		(*(pbuffer +   4)) & 0x7F;
	pdata->VL53L1_PRM_00012 =
		(VL53L1_FCTN_00082(2, pbuffer +   5));
	pdata->VL53L1_PRM_00279 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00280 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00281 =
		(*(pbuffer +   9)) & 0x3F;
	pdata->VL53L1_PRM_00282 =
		(*(pbuffer +  10));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00117(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00080];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00064(
			pdata,
			VL53L1_DEF_00080,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00004,
			comms_buffer,
			VL53L1_DEF_00080);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00036(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00080];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00004,
			comms_buffer,
			VL53L1_DEF_00080);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00116(
			VL53L1_DEF_00080,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00065(
	VL53L1_customer_nvm_managed_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00081 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00052;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00053;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00054;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00055;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00056;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00057 & 0xF;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00283;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00050 & 0x3F;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00051 & 0x3;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00072,
		2,
		pbuffer +   9);
	VL53L1_FCTN_00093(
		pdata->VL53L1_PRM_00070,
		2,
		pbuffer +  11);
	VL53L1_FCTN_00093(
		pdata->VL53L1_PRM_00071,
		2,
		pbuffer +  13);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00077,
		2,
		pbuffer +  15);
	VL53L1_FCTN_00093(
		pdata->VL53L1_PRM_00284 & 0x1FFF,
		2,
		pbuffer +  17);
	VL53L1_FCTN_00093(
		pdata->VL53L1_PRM_00285,
		2,
		pbuffer +  19);
	VL53L1_FCTN_00093(
		pdata->VL53L1_PRM_00286,
		2,
		pbuffer +  21);
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00118(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_customer_nvm_managed_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00081 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00052 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00053 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00054 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00055 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00056 =
		(*(pbuffer +   4));
	pdata->VL53L1_PRM_00057 =
		(*(pbuffer +   5)) & 0xF;
	pdata->VL53L1_PRM_00283 =
		(*(pbuffer +   6));
	pdata->VL53L1_PRM_00050 =
		(*(pbuffer +   7)) & 0x3F;
	pdata->VL53L1_PRM_00051 =
		(*(pbuffer +   8)) & 0x3;
	pdata->VL53L1_PRM_00072 =
		(VL53L1_FCTN_00082(2, pbuffer +   9));
	pdata->VL53L1_PRM_00070 =
		(VL53L1_FCTN_00094(2, pbuffer +  11));
	pdata->VL53L1_PRM_00071 =
		(VL53L1_FCTN_00094(2, pbuffer +  13));
	pdata->VL53L1_PRM_00077 =
		(VL53L1_FCTN_00082(2, pbuffer +  15));
	pdata->VL53L1_PRM_00284 =
		(VL53L1_FCTN_00094(2, pbuffer +  17)) & 0x1FFF;
	pdata->VL53L1_PRM_00285 =
		(VL53L1_FCTN_00094(2, pbuffer +  19));
	pdata->VL53L1_PRM_00286 =
		(VL53L1_FCTN_00094(2, pbuffer +  21));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00119(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00081];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00065(
			pdata,
			VL53L1_DEF_00081,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00032,
			comms_buffer,
			VL53L1_DEF_00081);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00037(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00081];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00032,
			comms_buffer,
			VL53L1_DEF_00081);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00118(
			VL53L1_DEF_00081,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00066(
	VL53L1_static_config_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00082 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00163,
		2,
		pbuffer +   0);
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00164 & 0x1;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00165 & 0xF;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00166 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00167 & 0x1F;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00168 & 0x7F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00169 & 0x7F;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00170 & 0x1;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00171;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00172 & 0x1;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00173 & 0x3;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00078 & 0x1F;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00174 & 0x3;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00175 & 0x3;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00176 & 0x7;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00114 & 0x1F;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00177 & 0x1;
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00178;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00179;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00159;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00180;
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00181;
	*(pbuffer +  23) =
		pdata->VL53L1_PRM_00182;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00183,
		2,
		pbuffer +  24);
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00184;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00185;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00186 & 0xF;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00187;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00188 & 0xF;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00189;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00120(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_config_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00082 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00163 =
		(VL53L1_FCTN_00082(2, pbuffer +   0));
	pdata->VL53L1_PRM_00164 =
		(*(pbuffer +   2)) & 0x1;
	pdata->VL53L1_PRM_00165 =
		(*(pbuffer +   3)) & 0xF;
	pdata->VL53L1_PRM_00166 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00167 =
		(*(pbuffer +   5)) & 0x1F;
	pdata->VL53L1_PRM_00168 =
		(*(pbuffer +   6)) & 0x7F;
	pdata->VL53L1_PRM_00169 =
		(*(pbuffer +   7)) & 0x7F;
	pdata->VL53L1_PRM_00170 =
		(*(pbuffer +   8)) & 0x1;
	pdata->VL53L1_PRM_00171 =
		(*(pbuffer +   9));
	pdata->VL53L1_PRM_00172 =
		(*(pbuffer +  10)) & 0x1;
	pdata->VL53L1_PRM_00173 =
		(*(pbuffer +  11)) & 0x3;
	pdata->VL53L1_PRM_00078 =
		(*(pbuffer +  12)) & 0x1F;
	pdata->VL53L1_PRM_00174 =
		(*(pbuffer +  13)) & 0x3;
	pdata->VL53L1_PRM_00175 =
		(*(pbuffer +  14)) & 0x3;
	pdata->VL53L1_PRM_00176 =
		(*(pbuffer +  15)) & 0x7;
	pdata->VL53L1_PRM_00114 =
		(*(pbuffer +  16)) & 0x1F;
	pdata->VL53L1_PRM_00177 =
		(*(pbuffer +  17)) & 0x1;
	pdata->VL53L1_PRM_00178 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00179 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00159 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00180 =
		(*(pbuffer +  21));
	pdata->VL53L1_PRM_00181 =
		(*(pbuffer +  22));
	pdata->VL53L1_PRM_00182 =
		(*(pbuffer +  23));
	pdata->VL53L1_PRM_00183 =
		(VL53L1_FCTN_00082(2, pbuffer +  24));
	pdata->VL53L1_PRM_00184 =
		(*(pbuffer +  26));
	pdata->VL53L1_PRM_00185 =
		(*(pbuffer +  27));
	pdata->VL53L1_PRM_00186 =
		(*(pbuffer +  28)) & 0xF;
	pdata->VL53L1_PRM_00187 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00188 =
		(*(pbuffer +  30)) & 0xF;
	pdata->VL53L1_PRM_00189 =
		(*(pbuffer +  31));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00121(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00082];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00066(
			pdata,
			VL53L1_DEF_00082,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00155,
			comms_buffer,
			VL53L1_DEF_00082);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00122(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00082];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00155,
			comms_buffer,
			VL53L1_DEF_00082);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00120(
			VL53L1_DEF_00082,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00067(
	VL53L1_general_config_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00083 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00190;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00191;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00192;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00111 & 0x7F;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00193 & 0xFFF,
		2,
		pbuffer +   4);
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00113 & 0x7F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00075;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00194;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00195 & 0x1;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00196 & 0x7;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00197,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00198,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00199,
		2,
		pbuffer +  16);
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00200;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00201;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00202;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00203;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00123(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_general_config_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00083 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00190 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00191 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00192 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00111 =
		(*(pbuffer +   3)) & 0x7F;
	pdata->VL53L1_PRM_00193 =
		(VL53L1_FCTN_00082(2, pbuffer +   4)) & 0xFFF;
	pdata->VL53L1_PRM_00113 =
		(*(pbuffer +   6)) & 0x7F;
	pdata->VL53L1_PRM_00075 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00194 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00195 =
		(*(pbuffer +   9)) & 0x1;
	pdata->VL53L1_PRM_00196 =
		(*(pbuffer +  11)) & 0x7;
	pdata->VL53L1_PRM_00197 =
		(VL53L1_FCTN_00082(2, pbuffer +  12));
	pdata->VL53L1_PRM_00198 =
		(VL53L1_FCTN_00082(2, pbuffer +  14));
	pdata->VL53L1_PRM_00199 =
		(VL53L1_FCTN_00082(2, pbuffer +  16));
	pdata->VL53L1_PRM_00200 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00201 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00202 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00203 =
		(*(pbuffer +  21));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00124(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00083];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00067(
			pdata,
			VL53L1_DEF_00083,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00156,
			comms_buffer,
			VL53L1_DEF_00083);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00125(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00083];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00156,
			comms_buffer,
			VL53L1_DEF_00083);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00123(
			VL53L1_DEF_00083,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00068(
	VL53L1_timing_config_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00084 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00204 & 0xF;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00205;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00206 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00207;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00117 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00118;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00076 & 0x3F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00120 & 0xF;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00121;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00122 & 0x3F;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00014,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00013,
		2,
		pbuffer +  12);
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00208;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00209;
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00090,
		4,
		pbuffer +  18);
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00210 & 0x1;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00126(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_timing_config_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00084 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00204 =
		(*(pbuffer +   0)) & 0xF;
	pdata->VL53L1_PRM_00205 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00206 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00207 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00117 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00118 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00076 =
		(*(pbuffer +   6)) & 0x3F;
	pdata->VL53L1_PRM_00120 =
		(*(pbuffer +   7)) & 0xF;
	pdata->VL53L1_PRM_00121 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00122 =
		(*(pbuffer +   9)) & 0x3F;
	pdata->VL53L1_PRM_00014 =
		(VL53L1_FCTN_00082(2, pbuffer +  10));
	pdata->VL53L1_PRM_00013 =
		(VL53L1_FCTN_00082(2, pbuffer +  12));
	pdata->VL53L1_PRM_00208 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00209 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00090 =
		(VL53L1_FCTN_00083(4, pbuffer +  18));
	pdata->VL53L1_PRM_00210 =
		(*(pbuffer +  22)) & 0x1;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00127(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00084];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00068(
			pdata,
			VL53L1_DEF_00084,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00157,
			comms_buffer,
			VL53L1_DEF_00084);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00128(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00084];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00157,
			comms_buffer,
			VL53L1_DEF_00084);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00126(
			VL53L1_DEF_00084,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00069(
	VL53L1_dynamic_config_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00085 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00234 & 0x3;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00235,
		2,
		pbuffer +   1);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00236,
		2,
		pbuffer +   3);
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00237 & 0x1;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00238 & 0x7;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00239;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00240;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00241 & 0x7F;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00242 & 0x7F;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00243 & 0x3;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00244 & 0x3;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00245 & 0xF;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00091;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00092;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00024;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00246 & 0x3;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00129(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_dynamic_config_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00085 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00234 =
		(*(pbuffer +   0)) & 0x3;
	pdata->VL53L1_PRM_00235 =
		(VL53L1_FCTN_00082(2, pbuffer +   1));
	pdata->VL53L1_PRM_00236 =
		(VL53L1_FCTN_00082(2, pbuffer +   3));
	pdata->VL53L1_PRM_00237 =
		(*(pbuffer +   5)) & 0x1;
	pdata->VL53L1_PRM_00238 =
		(*(pbuffer +   6)) & 0x7;
	pdata->VL53L1_PRM_00239 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00240 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00241 =
		(*(pbuffer +   9)) & 0x7F;
	pdata->VL53L1_PRM_00242 =
		(*(pbuffer +  10)) & 0x7F;
	pdata->VL53L1_PRM_00243 =
		(*(pbuffer +  11)) & 0x3;
	pdata->VL53L1_PRM_00244 =
		(*(pbuffer +  12)) & 0x3;
	pdata->VL53L1_PRM_00245 =
		(*(pbuffer +  13)) & 0xF;
	pdata->VL53L1_PRM_00091 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00092 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00024 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00246 =
		(*(pbuffer +  17)) & 0x3;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00130(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00085];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00069(
			pdata,
			VL53L1_DEF_00085,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00158,
			comms_buffer,
			VL53L1_DEF_00085);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00131(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00085];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00158,
			comms_buffer,
			VL53L1_DEF_00085);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00129(
			VL53L1_DEF_00085,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00070(
	VL53L1_system_control_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00079 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00250 & 0x1;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00247 & 0x1;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00248 & 0x1;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00249 & 0x3;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00097;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00132(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_control_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00079 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00250 =
		(*(pbuffer +   0)) & 0x1;
	pdata->VL53L1_PRM_00247 =
		(*(pbuffer +   1)) & 0x1;
	pdata->VL53L1_PRM_00248 =
		(*(pbuffer +   2)) & 0x1;
	pdata->VL53L1_PRM_00249 =
		(*(pbuffer +   3)) & 0x3;
	pdata->VL53L1_PRM_00097 =
		(*(pbuffer +   4));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00073(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00079];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00070(
			pdata,
			VL53L1_DEF_00079,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00138,
			comms_buffer,
			VL53L1_DEF_00079);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00133(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00079];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00138,
			comms_buffer,
			VL53L1_DEF_00079);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00132(
			VL53L1_DEF_00079,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00134(
	VL53L1_system_results_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00093 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00103 & 0x3F;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00080;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00081 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00027;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00128,
		2,
		pbuffer +   4);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00129,
		2,
		pbuffer +   6);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00132,
		2,
		pbuffer +   8);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00133,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00134,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00135,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00258,
		2,
		pbuffer +  16);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00259,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00260,
		2,
		pbuffer +  20);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00131,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00142,
		2,
		pbuffer +  24);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00143,
		2,
		pbuffer +  26);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00144,
		2,
		pbuffer +  28);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00145,
		2,
		pbuffer +  30);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00146,
		2,
		pbuffer +  32);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00147,
		2,
		pbuffer +  34);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00261,
		2,
		pbuffer +  36);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00262,
		2,
		pbuffer +  38);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00263,
		2,
		pbuffer +  40);
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00264;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00287;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00077(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_results_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00093 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00103 =
		(*(pbuffer +   0)) & 0x3F;
	pdata->VL53L1_PRM_00080 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00081 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00027 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00128 =
		(VL53L1_FCTN_00082(2, pbuffer +   4));
	pdata->VL53L1_PRM_00129 =
		(VL53L1_FCTN_00082(2, pbuffer +   6));
	pdata->VL53L1_PRM_00132 =
		(VL53L1_FCTN_00082(2, pbuffer +   8));
	pdata->VL53L1_PRM_00133 =
		(VL53L1_FCTN_00082(2, pbuffer +  10));
	pdata->VL53L1_PRM_00134 =
		(VL53L1_FCTN_00082(2, pbuffer +  12));
	pdata->VL53L1_PRM_00135 =
		(VL53L1_FCTN_00082(2, pbuffer +  14));
	pdata->VL53L1_PRM_00258 =
		(VL53L1_FCTN_00082(2, pbuffer +  16));
	pdata->VL53L1_PRM_00259 =
		(VL53L1_FCTN_00082(2, pbuffer +  18));
	pdata->VL53L1_PRM_00260 =
		(VL53L1_FCTN_00082(2, pbuffer +  20));
	pdata->VL53L1_PRM_00131 =
		(VL53L1_FCTN_00082(2, pbuffer +  22));
	pdata->VL53L1_PRM_00142 =
		(VL53L1_FCTN_00082(2, pbuffer +  24));
	pdata->VL53L1_PRM_00143 =
		(VL53L1_FCTN_00082(2, pbuffer +  26));
	pdata->VL53L1_PRM_00144 =
		(VL53L1_FCTN_00082(2, pbuffer +  28));
	pdata->VL53L1_PRM_00145 =
		(VL53L1_FCTN_00082(2, pbuffer +  30));
	pdata->VL53L1_PRM_00146 =
		(VL53L1_FCTN_00082(2, pbuffer +  32));
	pdata->VL53L1_PRM_00147 =
		(VL53L1_FCTN_00082(2, pbuffer +  34));
	pdata->VL53L1_PRM_00261 =
		(VL53L1_FCTN_00082(2, pbuffer +  36));
	pdata->VL53L1_PRM_00262 =
		(VL53L1_FCTN_00082(2, pbuffer +  38));
	pdata->VL53L1_PRM_00263 =
		(VL53L1_FCTN_00082(2, pbuffer +  40));
	pdata->VL53L1_PRM_00264 =
		(*(pbuffer +  42));
	pdata->VL53L1_PRM_00287 =
		(*(pbuffer +  43));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00135(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00093];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00134(
			pdata,
			VL53L1_DEF_00093,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00159,
			comms_buffer,
			VL53L1_DEF_00093);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00136(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00093];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00159,
			comms_buffer,
			VL53L1_DEF_00093);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00077(
			VL53L1_DEF_00093,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00137(
	VL53L1_core_results_t    *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00092 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00141,
		4,
		pbuffer +   0);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00137,
		4,
		pbuffer +   4);
	VL53L1_FCTN_00097(
		pdata->VL53L1_PRM_00138,
		4,
		pbuffer +   8);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00139,
		4,
		pbuffer +  12);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00151,
		4,
		pbuffer +  16);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00148,
		4,
		pbuffer +  20);
	VL53L1_FCTN_00097(
		pdata->VL53L1_PRM_00149,
		4,
		pbuffer +  24);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00150,
		4,
		pbuffer +  28);
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00288;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00076(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_core_results_t     *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00092 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00141 =
		(VL53L1_FCTN_00083(4, pbuffer +   0));
	pdata->VL53L1_PRM_00137 =
		(VL53L1_FCTN_00083(4, pbuffer +   4));
	pdata->VL53L1_PRM_00138 =
		(VL53L1_FCTN_00098(4, pbuffer +   8));
	pdata->VL53L1_PRM_00139 =
		(VL53L1_FCTN_00083(4, pbuffer +  12));
	pdata->VL53L1_PRM_00151 =
		(VL53L1_FCTN_00083(4, pbuffer +  16));
	pdata->VL53L1_PRM_00148 =
		(VL53L1_FCTN_00083(4, pbuffer +  20));
	pdata->VL53L1_PRM_00149 =
		(VL53L1_FCTN_00098(4, pbuffer +  24));
	pdata->VL53L1_PRM_00150 =
		(VL53L1_FCTN_00083(4, pbuffer +  28));
	pdata->VL53L1_PRM_00288 =
		(*(pbuffer +  32));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00138(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00092];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00137(
			pdata,
			VL53L1_DEF_00092,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00160,
			comms_buffer,
			VL53L1_DEF_00092);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00139(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00092];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00160,
			comms_buffer,
			VL53L1_DEF_00092);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00076(
			VL53L1_DEF_00092,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00140(
	VL53L1_debug_results_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00089 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00105,
		2,
		pbuffer +   0);
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00106 & 0x7F;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00047 & 0x3F;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00048 & 0x3;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00289 & 0x1;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00290 & 0x3F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00291 & 0x3F;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00089 & 0x3FF,
		2,
		pbuffer +   8);
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00292 & 0x3;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00293 & 0x3;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00294 & 0xF;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00295 & 0x7;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00296 & 0x1;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00297 & 0x3;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00298;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00299;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00300 & 0xFFF,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00301,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00302,
		2,
		pbuffer +  24);
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00303 & 0x1;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00304 & 0x7;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00305;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00306;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00307 & 0x7F;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00308 & 0x7F;
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00309 & 0x3;
	*(pbuffer +  33) =
		pdata->VL53L1_PRM_00310 & 0xF;
	*(pbuffer +  34) =
		pdata->VL53L1_PRM_00311;
	*(pbuffer +  35) =
		pdata->VL53L1_PRM_00312;
	*(pbuffer +  36) =
		pdata->VL53L1_PRM_00313;
	*(pbuffer +  37) =
		pdata->VL53L1_PRM_00314 & 0x1;
	*(pbuffer +  38) =
		pdata->VL53L1_PRM_00315 & 0x3;
	*(pbuffer +  39) =
		pdata->VL53L1_PRM_00269 & 0x1F;
	*(pbuffer +  40) =
		pdata->VL53L1_PRM_00270 & 0x1F;
	*(pbuffer +  41) =
		pdata->VL53L1_PRM_00271 & 0x1F;
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00272 & 0x1;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00273 & 0x1;
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00316 & 0x3;
	*(pbuffer +  45) =
		pdata->VL53L1_PRM_00317 & 0x3F;
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00102 & 0x3FFFF,
		4,
		pbuffer +  46);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00318,
		4,
		pbuffer +  50);
	*(pbuffer +  54) =
		pdata->VL53L1_PRM_00319 & 0x1;
	*(pbuffer +  55) =
		pdata->VL53L1_PRM_00320 & 0x1;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00075(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_debug_results_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00089 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00105 =
		(VL53L1_FCTN_00082(2, pbuffer +   0));
	pdata->VL53L1_PRM_00106 =
		(*(pbuffer +   2)) & 0x7F;
	pdata->VL53L1_PRM_00047 =
		(*(pbuffer +   3)) & 0x3F;
	pdata->VL53L1_PRM_00048 =
		(*(pbuffer +   4)) & 0x3;
	pdata->VL53L1_PRM_00289 =
		(*(pbuffer +   5)) & 0x1;
	pdata->VL53L1_PRM_00290 =
		(*(pbuffer +   6)) & 0x3F;
	pdata->VL53L1_PRM_00291 =
		(*(pbuffer +   7)) & 0x3F;
	pdata->VL53L1_PRM_00089 =
		(VL53L1_FCTN_00082(2, pbuffer +   8)) & 0x3FF;
	pdata->VL53L1_PRM_00292 =
		(*(pbuffer +  10)) & 0x3;
	pdata->VL53L1_PRM_00293 =
		(*(pbuffer +  11)) & 0x3;
	pdata->VL53L1_PRM_00294 =
		(*(pbuffer +  12)) & 0xF;
	pdata->VL53L1_PRM_00295 =
		(*(pbuffer +  13)) & 0x7;
	pdata->VL53L1_PRM_00296 =
		(*(pbuffer +  14)) & 0x1;
	pdata->VL53L1_PRM_00297 =
		(*(pbuffer +  15)) & 0x3;
	pdata->VL53L1_PRM_00298 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00299 =
		(*(pbuffer +  17));
	pdata->VL53L1_PRM_00300 =
		(VL53L1_FCTN_00082(2, pbuffer +  18)) & 0xFFF;
	pdata->VL53L1_PRM_00301 =
		(VL53L1_FCTN_00082(2, pbuffer +  22));
	pdata->VL53L1_PRM_00302 =
		(VL53L1_FCTN_00082(2, pbuffer +  24));
	pdata->VL53L1_PRM_00303 =
		(*(pbuffer +  26)) & 0x1;
	pdata->VL53L1_PRM_00304 =
		(*(pbuffer +  27)) & 0x7;
	pdata->VL53L1_PRM_00305 =
		(*(pbuffer +  28));
	pdata->VL53L1_PRM_00306 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00307 =
		(*(pbuffer +  30)) & 0x7F;
	pdata->VL53L1_PRM_00308 =
		(*(pbuffer +  31)) & 0x7F;
	pdata->VL53L1_PRM_00309 =
		(*(pbuffer +  32)) & 0x3;
	pdata->VL53L1_PRM_00310 =
		(*(pbuffer +  33)) & 0xF;
	pdata->VL53L1_PRM_00311 =
		(*(pbuffer +  34));
	pdata->VL53L1_PRM_00312 =
		(*(pbuffer +  35));
	pdata->VL53L1_PRM_00313 =
		(*(pbuffer +  36));
	pdata->VL53L1_PRM_00314 =
		(*(pbuffer +  37)) & 0x1;
	pdata->VL53L1_PRM_00315 =
		(*(pbuffer +  38)) & 0x3;
	pdata->VL53L1_PRM_00269 =
		(*(pbuffer +  39)) & 0x1F;
	pdata->VL53L1_PRM_00270 =
		(*(pbuffer +  40)) & 0x1F;
	pdata->VL53L1_PRM_00271 =
		(*(pbuffer +  41)) & 0x1F;
	pdata->VL53L1_PRM_00272 =
		(*(pbuffer +  42)) & 0x1;
	pdata->VL53L1_PRM_00273 =
		(*(pbuffer +  43)) & 0x1;
	pdata->VL53L1_PRM_00316 =
		(*(pbuffer +  44)) & 0x3;
	pdata->VL53L1_PRM_00317 =
		(*(pbuffer +  45)) & 0x3F;
	pdata->VL53L1_PRM_00102 =
		(VL53L1_FCTN_00083(4, pbuffer +  46)) & 0x3FFFF;
	pdata->VL53L1_PRM_00318 =
		(VL53L1_FCTN_00083(4, pbuffer +  50));
	pdata->VL53L1_PRM_00319 =
		(*(pbuffer +  54)) & 0x1;
	pdata->VL53L1_PRM_00320 =
		(*(pbuffer +  55)) & 0x1;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00141(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00089];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00140(
			pdata,
			VL53L1_DEF_00089,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00097,
			comms_buffer,
			VL53L1_DEF_00089);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00142(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00089];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00097,
			comms_buffer,
			VL53L1_DEF_00089);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00075(
			VL53L1_DEF_00089,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00143(
	VL53L1_nvm_copy_data_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00161 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00321;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00005;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00004;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00322,
		2,
		pbuffer +   3);
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00323 & 0x7F;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00324 & 0x7;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00325 & 0x7;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00326 & 0x3F;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00327 & 0x3F;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00328 & 0x1;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00329 & 0x7F;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00330 & 0x1;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00331 & 0x3F;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00332 & 0x3F;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00333;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00334;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00335;
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00336;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00337;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00338;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00339;
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00340;
	*(pbuffer +  23) =
		pdata->VL53L1_PRM_00341;
	*(pbuffer +  24) =
		pdata->VL53L1_PRM_00342;
	*(pbuffer +  25) =
		pdata->VL53L1_PRM_00343;
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00344;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00345;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00346;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00347;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00348;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00349;
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00350;
	*(pbuffer +  33) =
		pdata->VL53L1_PRM_00351;
	*(pbuffer +  34) =
		pdata->VL53L1_PRM_00352;
	*(pbuffer +  35) =
		pdata->VL53L1_PRM_00353;
	*(pbuffer +  36) =
		pdata->VL53L1_PRM_00354;
	*(pbuffer +  37) =
		pdata->VL53L1_PRM_00355;
	*(pbuffer +  38) =
		pdata->VL53L1_PRM_00356;
	*(pbuffer +  39) =
		pdata->VL53L1_PRM_00357;
	*(pbuffer +  40) =
		pdata->VL53L1_PRM_00358;
	*(pbuffer +  41) =
		pdata->VL53L1_PRM_00359;
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00360;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00361;
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00362;
	*(pbuffer +  45) =
		pdata->VL53L1_PRM_00363;
	*(pbuffer +  46) =
		pdata->VL53L1_PRM_00364;
	*(pbuffer +  47) =
		pdata->VL53L1_PRM_00093;
	*(pbuffer +  48) =
		pdata->VL53L1_PRM_00094;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00144(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_nvm_copy_data_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00161 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00321 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00005 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00004 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00322 =
		(VL53L1_FCTN_00082(2, pbuffer +   3));
	pdata->VL53L1_PRM_00323 =
		(*(pbuffer +   5)) & 0x7F;
	pdata->VL53L1_PRM_00324 =
		(*(pbuffer +   6)) & 0x7;
	pdata->VL53L1_PRM_00325 =
		(*(pbuffer +   7)) & 0x7;
	pdata->VL53L1_PRM_00326 =
		(*(pbuffer +   8)) & 0x3F;
	pdata->VL53L1_PRM_00327 =
		(*(pbuffer +   9)) & 0x3F;
	pdata->VL53L1_PRM_00328 =
		(*(pbuffer +  10)) & 0x1;
	pdata->VL53L1_PRM_00329 =
		(*(pbuffer +  11)) & 0x7F;
	pdata->VL53L1_PRM_00330 =
		(*(pbuffer +  12)) & 0x1;
	pdata->VL53L1_PRM_00331 =
		(*(pbuffer +  13)) & 0x3F;
	pdata->VL53L1_PRM_00332 =
		(*(pbuffer +  14)) & 0x3F;
	pdata->VL53L1_PRM_00333 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00334 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00335 =
		(*(pbuffer +  17));
	pdata->VL53L1_PRM_00336 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00337 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00338 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00339 =
		(*(pbuffer +  21));
	pdata->VL53L1_PRM_00340 =
		(*(pbuffer +  22));
	pdata->VL53L1_PRM_00341 =
		(*(pbuffer +  23));
	pdata->VL53L1_PRM_00342 =
		(*(pbuffer +  24));
	pdata->VL53L1_PRM_00343 =
		(*(pbuffer +  25));
	pdata->VL53L1_PRM_00344 =
		(*(pbuffer +  26));
	pdata->VL53L1_PRM_00345 =
		(*(pbuffer +  27));
	pdata->VL53L1_PRM_00346 =
		(*(pbuffer +  28));
	pdata->VL53L1_PRM_00347 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00348 =
		(*(pbuffer +  30));
	pdata->VL53L1_PRM_00349 =
		(*(pbuffer +  31));
	pdata->VL53L1_PRM_00350 =
		(*(pbuffer +  32));
	pdata->VL53L1_PRM_00351 =
		(*(pbuffer +  33));
	pdata->VL53L1_PRM_00352 =
		(*(pbuffer +  34));
	pdata->VL53L1_PRM_00353 =
		(*(pbuffer +  35));
	pdata->VL53L1_PRM_00354 =
		(*(pbuffer +  36));
	pdata->VL53L1_PRM_00355 =
		(*(pbuffer +  37));
	pdata->VL53L1_PRM_00356 =
		(*(pbuffer +  38));
	pdata->VL53L1_PRM_00357 =
		(*(pbuffer +  39));
	pdata->VL53L1_PRM_00358 =
		(*(pbuffer +  40));
	pdata->VL53L1_PRM_00359 =
		(*(pbuffer +  41));
	pdata->VL53L1_PRM_00360 =
		(*(pbuffer +  42));
	pdata->VL53L1_PRM_00361 =
		(*(pbuffer +  43));
	pdata->VL53L1_PRM_00362 =
		(*(pbuffer +  44));
	pdata->VL53L1_PRM_00363 =
		(*(pbuffer +  45));
	pdata->VL53L1_PRM_00364 =
		(*(pbuffer +  46));
	pdata->VL53L1_PRM_00093 =
		(*(pbuffer +  47));
	pdata->VL53L1_PRM_00094 =
		(*(pbuffer +  48));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00145(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00161];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00143(
			pdata,
			VL53L1_DEF_00161,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00162,
			comms_buffer,
			VL53L1_DEF_00161);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00038(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00161];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00162,
			comms_buffer,
			VL53L1_DEF_00161);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00144(
			VL53L1_DEF_00161,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00146(
	VL53L1_prev_shadow_system_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00163 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00365 & 0x3F;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00366;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00367 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00368;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00369,
		2,
		pbuffer +   4);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00370,
		2,
		pbuffer +   6);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00371,
		2,
		pbuffer +   8);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00372,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00373,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00374,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00375,
		2,
		pbuffer +  16);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00376,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00377,
		2,
		pbuffer +  20);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00378,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00379,
		2,
		pbuffer +  24);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00380,
		2,
		pbuffer +  26);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00381,
		2,
		pbuffer +  28);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00382,
		2,
		pbuffer +  30);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00383,
		2,
		pbuffer +  32);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00384,
		2,
		pbuffer +  34);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00385,
		2,
		pbuffer +  36);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00386,
		2,
		pbuffer +  38);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00387,
		2,
		pbuffer +  40);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00388,
		2,
		pbuffer +  42);
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00147(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_system_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00163 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00365 =
		(*(pbuffer +   0)) & 0x3F;
	pdata->VL53L1_PRM_00366 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00367 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00368 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00369 =
		(VL53L1_FCTN_00082(2, pbuffer +   4));
	pdata->VL53L1_PRM_00370 =
		(VL53L1_FCTN_00082(2, pbuffer +   6));
	pdata->VL53L1_PRM_00371 =
		(VL53L1_FCTN_00082(2, pbuffer +   8));
	pdata->VL53L1_PRM_00372 =
		(VL53L1_FCTN_00082(2, pbuffer +  10));
	pdata->VL53L1_PRM_00373 =
		(VL53L1_FCTN_00082(2, pbuffer +  12));
	pdata->VL53L1_PRM_00374 =
		(VL53L1_FCTN_00082(2, pbuffer +  14));
	pdata->VL53L1_PRM_00375 =
		(VL53L1_FCTN_00082(2, pbuffer +  16));
	pdata->VL53L1_PRM_00376 =
		(VL53L1_FCTN_00082(2, pbuffer +  18));
	pdata->VL53L1_PRM_00377 =
		(VL53L1_FCTN_00082(2, pbuffer +  20));
	pdata->VL53L1_PRM_00378 =
		(VL53L1_FCTN_00082(2, pbuffer +  22));
	pdata->VL53L1_PRM_00379 =
		(VL53L1_FCTN_00082(2, pbuffer +  24));
	pdata->VL53L1_PRM_00380 =
		(VL53L1_FCTN_00082(2, pbuffer +  26));
	pdata->VL53L1_PRM_00381 =
		(VL53L1_FCTN_00082(2, pbuffer +  28));
	pdata->VL53L1_PRM_00382 =
		(VL53L1_FCTN_00082(2, pbuffer +  30));
	pdata->VL53L1_PRM_00383 =
		(VL53L1_FCTN_00082(2, pbuffer +  32));
	pdata->VL53L1_PRM_00384 =
		(VL53L1_FCTN_00082(2, pbuffer +  34));
	pdata->VL53L1_PRM_00385 =
		(VL53L1_FCTN_00082(2, pbuffer +  36));
	pdata->VL53L1_PRM_00386 =
		(VL53L1_FCTN_00082(2, pbuffer +  38));
	pdata->VL53L1_PRM_00387 =
		(VL53L1_FCTN_00082(2, pbuffer +  40));
	pdata->VL53L1_PRM_00388 =
		(VL53L1_FCTN_00082(2, pbuffer +  42));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00148(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00163];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00146(
			pdata,
			VL53L1_DEF_00163,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00164,
			comms_buffer,
			VL53L1_DEF_00163);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00149(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00163];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00164,
			comms_buffer,
			VL53L1_DEF_00163);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00147(
			VL53L1_DEF_00163,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00150(
	VL53L1_prev_shadow_core_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00165 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00389,
		4,
		pbuffer +   0);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00390,
		4,
		pbuffer +   4);
	VL53L1_FCTN_00097(
		pdata->VL53L1_PRM_00391,
		4,
		pbuffer +   8);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00392,
		4,
		pbuffer +  12);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00393,
		4,
		pbuffer +  16);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00394,
		4,
		pbuffer +  20);
	VL53L1_FCTN_00097(
		pdata->VL53L1_PRM_00395,
		4,
		pbuffer +  24);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00396,
		4,
		pbuffer +  28);
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00397;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00151(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_core_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00165 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00389 =
		(VL53L1_FCTN_00083(4, pbuffer +   0));
	pdata->VL53L1_PRM_00390 =
		(VL53L1_FCTN_00083(4, pbuffer +   4));
	pdata->VL53L1_PRM_00391 =
		(VL53L1_FCTN_00098(4, pbuffer +   8));
	pdata->VL53L1_PRM_00392 =
		(VL53L1_FCTN_00083(4, pbuffer +  12));
	pdata->VL53L1_PRM_00393 =
		(VL53L1_FCTN_00083(4, pbuffer +  16));
	pdata->VL53L1_PRM_00394 =
		(VL53L1_FCTN_00083(4, pbuffer +  20));
	pdata->VL53L1_PRM_00395 =
		(VL53L1_FCTN_00098(4, pbuffer +  24));
	pdata->VL53L1_PRM_00396 =
		(VL53L1_FCTN_00083(4, pbuffer +  28));
	pdata->VL53L1_PRM_00397 =
		(*(pbuffer +  32));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00152(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00165];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00150(
			pdata,
			VL53L1_DEF_00165,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00166,
			comms_buffer,
			VL53L1_DEF_00165);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00153(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00165];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00166,
			comms_buffer,
			VL53L1_DEF_00165);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00151(
			VL53L1_DEF_00165,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00154(
	VL53L1_patch_debug_t     *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00167 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00398;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00399;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00155(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_debug_t      *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00167 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00398 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00399 =
		(*(pbuffer +   1));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00156(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00167];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00154(
			pdata,
			VL53L1_DEF_00167,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00168,
			comms_buffer,
			VL53L1_DEF_00167);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00157(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00167];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00168,
			comms_buffer,
			VL53L1_DEF_00167);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00155(
			VL53L1_DEF_00167,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00158(
	VL53L1_gph_general_config_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00169 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00400,
		2,
		pbuffer +   0);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00401,
		2,
		pbuffer +   2);
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00402;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00159(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_general_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00169 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00400 =
		(VL53L1_FCTN_00082(2, pbuffer +   0));
	pdata->VL53L1_PRM_00401 =
		(VL53L1_FCTN_00082(2, pbuffer +   2));
	pdata->VL53L1_PRM_00402 =
		(*(pbuffer +   4));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00160(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00169];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00158(
			pdata,
			VL53L1_DEF_00169,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00170,
			comms_buffer,
			VL53L1_DEF_00169);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00161(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00169];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00170,
			comms_buffer,
			VL53L1_DEF_00169);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00159(
			VL53L1_DEF_00169,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00162(
	VL53L1_gph_static_config_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00171 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00403 & 0x7;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00404,
		2,
		pbuffer +   1);
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00405;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00406;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00407;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00163(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_static_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00171 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00403 =
		(*(pbuffer +   0)) & 0x7;
	pdata->VL53L1_PRM_00404 =
		(VL53L1_FCTN_00082(2, pbuffer +   1));
	pdata->VL53L1_PRM_00405 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00406 =
		(*(pbuffer +   4));
	pdata->VL53L1_PRM_00407 =
		(*(pbuffer +   5));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00164(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00171];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00162(
			pdata,
			VL53L1_DEF_00171,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00172,
			comms_buffer,
			VL53L1_DEF_00171);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00165(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00171];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00172,
			comms_buffer,
			VL53L1_DEF_00171);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00163(
			VL53L1_DEF_00171,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00166(
	VL53L1_gph_timing_config_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00173 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00408 & 0xF;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00409;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00410 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00411;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00412 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00413;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00414 & 0x3F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00415 & 0x3F;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00416 & 0xF;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00417;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00418,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00419,
		2,
		pbuffer +  12);
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00420;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00421;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00167(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_timing_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00173 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00408 =
		(*(pbuffer +   0)) & 0xF;
	pdata->VL53L1_PRM_00409 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00410 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00411 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00412 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00413 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00414 =
		(*(pbuffer +   6)) & 0x3F;
	pdata->VL53L1_PRM_00415 =
		(*(pbuffer +   7)) & 0x3F;
	pdata->VL53L1_PRM_00416 =
		(*(pbuffer +   8)) & 0xF;
	pdata->VL53L1_PRM_00417 =
		(*(pbuffer +   9));
	pdata->VL53L1_PRM_00418 =
		(VL53L1_FCTN_00082(2, pbuffer +  10));
	pdata->VL53L1_PRM_00419 =
		(VL53L1_FCTN_00082(2, pbuffer +  12));
	pdata->VL53L1_PRM_00420 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00421 =
		(*(pbuffer +  15));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00168(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00173];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00166(
			pdata,
			VL53L1_DEF_00173,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00174,
			comms_buffer,
			VL53L1_DEF_00173);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00169(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00173];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00174,
			comms_buffer,
			VL53L1_DEF_00173);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00167(
			VL53L1_DEF_00173,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00170(
	VL53L1_fw_internal_t     *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00175 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00422;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00423;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00171(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_fw_internal_t      *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00175 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00422 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00423 =
		(*(pbuffer +   1));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00172(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00175];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00170(
			pdata,
			VL53L1_DEF_00175,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00176,
			comms_buffer,
			VL53L1_DEF_00175);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00173(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00175];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00176,
			comms_buffer,
			VL53L1_DEF_00175);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00171(
			VL53L1_DEF_00175,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00174(
	VL53L1_patch_results_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00177 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00424 & 0x3;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00425;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00426;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00427;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00428;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00429;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00430;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00431;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00432;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00433;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00434;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00435;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00436;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00437;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00438;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00439;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00440;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00441;
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00442;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00443;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00444;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00445;
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00446;
	*(pbuffer +  23) =
		pdata->VL53L1_PRM_00447;
	*(pbuffer +  24) =
		pdata->VL53L1_PRM_00448;
	*(pbuffer +  25) =
		pdata->VL53L1_PRM_00449;
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00450;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00451;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00452;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00453;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00454;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00455;
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00456;
	*(pbuffer +  33) =
		pdata->VL53L1_PRM_00457;
	*(pbuffer +  34) =
		pdata->VL53L1_PRM_00458;
	*(pbuffer +  35) =
		pdata->VL53L1_PRM_00459;
	*(pbuffer +  36) =
		pdata->VL53L1_PRM_00460;
	*(pbuffer +  37) =
		pdata->VL53L1_PRM_00461;
	*(pbuffer +  38) =
		pdata->VL53L1_PRM_00462;
	*(pbuffer +  39) =
		pdata->VL53L1_PRM_00463;
	*(pbuffer +  40) =
		pdata->VL53L1_PRM_00464;
	*(pbuffer +  41) =
		pdata->VL53L1_PRM_00465;
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00466;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00467;
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00468;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00469,
		2,
		pbuffer +  46);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00470,
		4,
		pbuffer +  48);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00471,
		2,
		pbuffer +  52);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00472,
		2,
		pbuffer +  54);
	*(pbuffer +  56) =
		pdata->VL53L1_PRM_00473;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00474,
		2,
		pbuffer +  58);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00475,
		2,
		pbuffer +  62);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00476,
		2,
		pbuffer +  64);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00477,
		2,
		pbuffer +  66);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00478 & 0xFFFFFF,
		4,
		pbuffer +  68);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00479 & 0xFFFFFF,
		4,
		pbuffer +  72);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00480 & 0xFFFFFF,
		4,
		pbuffer +  76);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00481 & 0xFFFFFF,
		4,
		pbuffer +  80);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00482,
		4,
		pbuffer +  84);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00483,
		2,
		pbuffer +  88);
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00175(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_results_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00177 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00424 =
		(*(pbuffer +   0)) & 0x3;
	pdata->VL53L1_PRM_00425 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00426 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00427 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00428 =
		(*(pbuffer +   4));
	pdata->VL53L1_PRM_00429 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00430 =
		(*(pbuffer +   6));
	pdata->VL53L1_PRM_00431 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00432 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00433 =
		(*(pbuffer +   9));
	pdata->VL53L1_PRM_00434 =
		(*(pbuffer +  10));
	pdata->VL53L1_PRM_00435 =
		(*(pbuffer +  11));
	pdata->VL53L1_PRM_00436 =
		(*(pbuffer +  12));
	pdata->VL53L1_PRM_00437 =
		(*(pbuffer +  13));
	pdata->VL53L1_PRM_00438 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00439 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00440 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00441 =
		(*(pbuffer +  17));
	pdata->VL53L1_PRM_00442 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00443 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00444 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00445 =
		(*(pbuffer +  21));
	pdata->VL53L1_PRM_00446 =
		(*(pbuffer +  22));
	pdata->VL53L1_PRM_00447 =
		(*(pbuffer +  23));
	pdata->VL53L1_PRM_00448 =
		(*(pbuffer +  24));
	pdata->VL53L1_PRM_00449 =
		(*(pbuffer +  25));
	pdata->VL53L1_PRM_00450 =
		(*(pbuffer +  26));
	pdata->VL53L1_PRM_00451 =
		(*(pbuffer +  27));
	pdata->VL53L1_PRM_00452 =
		(*(pbuffer +  28));
	pdata->VL53L1_PRM_00453 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00454 =
		(*(pbuffer +  30));
	pdata->VL53L1_PRM_00455 =
		(*(pbuffer +  31));
	pdata->VL53L1_PRM_00456 =
		(*(pbuffer +  32));
	pdata->VL53L1_PRM_00457 =
		(*(pbuffer +  33));
	pdata->VL53L1_PRM_00458 =
		(*(pbuffer +  34));
	pdata->VL53L1_PRM_00459 =
		(*(pbuffer +  35));
	pdata->VL53L1_PRM_00460 =
		(*(pbuffer +  36));
	pdata->VL53L1_PRM_00461 =
		(*(pbuffer +  37));
	pdata->VL53L1_PRM_00462 =
		(*(pbuffer +  38));
	pdata->VL53L1_PRM_00463 =
		(*(pbuffer +  39));
	pdata->VL53L1_PRM_00464 =
		(*(pbuffer +  40));
	pdata->VL53L1_PRM_00465 =
		(*(pbuffer +  41));
	pdata->VL53L1_PRM_00466 =
		(*(pbuffer +  42));
	pdata->VL53L1_PRM_00467 =
		(*(pbuffer +  43));
	pdata->VL53L1_PRM_00468 =
		(*(pbuffer +  44));
	pdata->VL53L1_PRM_00469 =
		(VL53L1_FCTN_00082(2, pbuffer +  46));
	pdata->VL53L1_PRM_00470 =
		(VL53L1_FCTN_00083(4, pbuffer +  48));
	pdata->VL53L1_PRM_00471 =
		(VL53L1_FCTN_00082(2, pbuffer +  52));
	pdata->VL53L1_PRM_00472 =
		(VL53L1_FCTN_00082(2, pbuffer +  54));
	pdata->VL53L1_PRM_00473 =
		(*(pbuffer +  56));
	pdata->VL53L1_PRM_00474 =
		(VL53L1_FCTN_00082(2, pbuffer +  58));
	pdata->VL53L1_PRM_00475 =
		(VL53L1_FCTN_00082(2, pbuffer +  62));
	pdata->VL53L1_PRM_00476 =
		(VL53L1_FCTN_00082(2, pbuffer +  64));
	pdata->VL53L1_PRM_00477 =
		(VL53L1_FCTN_00082(2, pbuffer +  66));
	pdata->VL53L1_PRM_00478 =
		(VL53L1_FCTN_00083(4, pbuffer +  68)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00479 =
		(VL53L1_FCTN_00083(4, pbuffer +  72)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00480 =
		(VL53L1_FCTN_00083(4, pbuffer +  76)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00481 =
		(VL53L1_FCTN_00083(4, pbuffer +  80)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00482 =
		(VL53L1_FCTN_00083(4, pbuffer +  84));
	pdata->VL53L1_PRM_00483 =
		(VL53L1_FCTN_00082(2, pbuffer +  88));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00176(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00177];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00174(
			pdata,
			VL53L1_DEF_00177,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00178,
			comms_buffer,
			VL53L1_DEF_00177);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00177(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00177];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00178,
			comms_buffer,
			VL53L1_DEF_00177);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00175(
			VL53L1_DEF_00177,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00178(
	VL53L1_shadow_system_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00179 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00484;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00485 & 0x3F;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00486;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00487 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00488;
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00489,
		2,
		pbuffer +   6);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00490,
		2,
		pbuffer +   8);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00491,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00492,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00493,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00494,
		2,
		pbuffer +  16);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00495,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00496,
		2,
		pbuffer +  20);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00497,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00498,
		2,
		pbuffer +  24);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00499,
		2,
		pbuffer +  26);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00500,
		2,
		pbuffer +  28);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00501,
		2,
		pbuffer +  30);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00502,
		2,
		pbuffer +  32);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00503,
		2,
		pbuffer +  34);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00504,
		2,
		pbuffer +  36);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00505,
		2,
		pbuffer +  38);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00506,
		2,
		pbuffer +  40);
	VL53L1_FCTN_00092(
		pdata->VL53L1_PRM_00507,
		2,
		pbuffer +  42);
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00508;
	*(pbuffer +  45) =
		pdata->VL53L1_PRM_00509;
	*(pbuffer +  80) =
		pdata->VL53L1_PRM_00510;
	*(pbuffer +  81) =
		pdata->VL53L1_PRM_00511;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00179(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_system_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00179 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00484 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00485 =
		(*(pbuffer +   2)) & 0x3F;
	pdata->VL53L1_PRM_00486 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00487 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00488 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00489 =
		(VL53L1_FCTN_00082(2, pbuffer +   6));
	pdata->VL53L1_PRM_00490 =
		(VL53L1_FCTN_00082(2, pbuffer +   8));
	pdata->VL53L1_PRM_00491 =
		(VL53L1_FCTN_00082(2, pbuffer +  10));
	pdata->VL53L1_PRM_00492 =
		(VL53L1_FCTN_00082(2, pbuffer +  12));
	pdata->VL53L1_PRM_00493 =
		(VL53L1_FCTN_00082(2, pbuffer +  14));
	pdata->VL53L1_PRM_00494 =
		(VL53L1_FCTN_00082(2, pbuffer +  16));
	pdata->VL53L1_PRM_00495 =
		(VL53L1_FCTN_00082(2, pbuffer +  18));
	pdata->VL53L1_PRM_00496 =
		(VL53L1_FCTN_00082(2, pbuffer +  20));
	pdata->VL53L1_PRM_00497 =
		(VL53L1_FCTN_00082(2, pbuffer +  22));
	pdata->VL53L1_PRM_00498 =
		(VL53L1_FCTN_00082(2, pbuffer +  24));
	pdata->VL53L1_PRM_00499 =
		(VL53L1_FCTN_00082(2, pbuffer +  26));
	pdata->VL53L1_PRM_00500 =
		(VL53L1_FCTN_00082(2, pbuffer +  28));
	pdata->VL53L1_PRM_00501 =
		(VL53L1_FCTN_00082(2, pbuffer +  30));
	pdata->VL53L1_PRM_00502 =
		(VL53L1_FCTN_00082(2, pbuffer +  32));
	pdata->VL53L1_PRM_00503 =
		(VL53L1_FCTN_00082(2, pbuffer +  34));
	pdata->VL53L1_PRM_00504 =
		(VL53L1_FCTN_00082(2, pbuffer +  36));
	pdata->VL53L1_PRM_00505 =
		(VL53L1_FCTN_00082(2, pbuffer +  38));
	pdata->VL53L1_PRM_00506 =
		(VL53L1_FCTN_00082(2, pbuffer +  40));
	pdata->VL53L1_PRM_00507 =
		(VL53L1_FCTN_00082(2, pbuffer +  42));
	pdata->VL53L1_PRM_00508 =
		(*(pbuffer +  44));
	pdata->VL53L1_PRM_00509 =
		(*(pbuffer +  45));
	pdata->VL53L1_PRM_00510 =
		(*(pbuffer +  80));
	pdata->VL53L1_PRM_00511 =
		(*(pbuffer +  81));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00180(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00179];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00178(
			pdata,
			VL53L1_DEF_00179,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00180,
			comms_buffer,
			VL53L1_DEF_00179);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00181(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00179];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00180,
			comms_buffer,
			VL53L1_DEF_00179);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00179(
			VL53L1_DEF_00179,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00182(
	VL53L1_shadow_core_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00181 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00512,
		4,
		pbuffer +   0);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00513,
		4,
		pbuffer +   4);
	VL53L1_FCTN_00097(
		pdata->VL53L1_PRM_00514,
		4,
		pbuffer +   8);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00515,
		4,
		pbuffer +  12);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00516,
		4,
		pbuffer +  16);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00517,
		4,
		pbuffer +  20);
	VL53L1_FCTN_00097(
		pdata->VL53L1_PRM_00518,
		4,
		pbuffer +  24);
	VL53L1_FCTN_00095(
		pdata->VL53L1_PRM_00519,
		4,
		pbuffer +  28);
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00520;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00183(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_core_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00181 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00512 =
		(VL53L1_FCTN_00083(4, pbuffer +   0));
	pdata->VL53L1_PRM_00513 =
		(VL53L1_FCTN_00083(4, pbuffer +   4));
	pdata->VL53L1_PRM_00514 =
		(VL53L1_FCTN_00098(4, pbuffer +   8));
	pdata->VL53L1_PRM_00515 =
		(VL53L1_FCTN_00083(4, pbuffer +  12));
	pdata->VL53L1_PRM_00516 =
		(VL53L1_FCTN_00083(4, pbuffer +  16));
	pdata->VL53L1_PRM_00517 =
		(VL53L1_FCTN_00083(4, pbuffer +  20));
	pdata->VL53L1_PRM_00518 =
		(VL53L1_FCTN_00098(4, pbuffer +  24));
	pdata->VL53L1_PRM_00519 =
		(VL53L1_FCTN_00083(4, pbuffer +  28));
	pdata->VL53L1_PRM_00520 =
		(*(pbuffer +  32));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00184(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00181];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00182(
			pdata,
			VL53L1_DEF_00181,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00182,
			comms_buffer,
			VL53L1_DEF_00181);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00185(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata)
{
	





	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00181];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00028(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00182,
			comms_buffer,
			VL53L1_DEF_00181);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00029(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00183(
			VL53L1_DEF_00181,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}

