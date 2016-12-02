
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


VL53L1_Error VL53L1_FCTN_00084(
	VL53L1_static_nvm_managed_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00096 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00320 & 0x7F;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00321 & 0xF;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00322 & 0x7F;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00323 & 0x3;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00324 & 0x7F;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00011,
		2,
		pbuffer +   5);
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00325;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00326;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00327 & 0x3F;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00328;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00147(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_nvm_managed_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00096 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00320 =
		(*(pbuffer +   0)) & 0x7F;
	pdata->VL53L1_PRM_00321 =
		(*(pbuffer +   1)) & 0xF;
	pdata->VL53L1_PRM_00322 =
		(*(pbuffer +   2)) & 0x7F;
	pdata->VL53L1_PRM_00323 =
		(*(pbuffer +   3)) & 0x3;
	pdata->VL53L1_PRM_00324 =
		(*(pbuffer +   4)) & 0x7F;
	pdata->VL53L1_PRM_00011 =
		(VL53L1_FCTN_00105(2, pbuffer +   5));
	pdata->VL53L1_PRM_00325 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00326 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00327 =
		(*(pbuffer +   9)) & 0x3F;
	pdata->VL53L1_PRM_00328 =
		(*(pbuffer +  10));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00148(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00096];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00084(
			pdata,
			VL53L1_DEF_00096,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00001,
			comms_buffer,
			VL53L1_DEF_00096);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00043(
	VL53L1_DEV                 Dev,
	VL53L1_static_nvm_managed_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00096];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00001,
			comms_buffer,
			VL53L1_DEF_00096);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00147(
			VL53L1_DEF_00096,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00085(
	VL53L1_customer_nvm_managed_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00097 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00053;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00054;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00055;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00056;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00057;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00058 & 0xF;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00329;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00051 & 0x3F;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00052 & 0x3;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00080,
		2,
		pbuffer +   9);
	VL53L1_FCTN_00118(
		pdata->VL53L1_PRM_00078,
		2,
		pbuffer +  11);
	VL53L1_FCTN_00118(
		pdata->VL53L1_PRM_00079,
		2,
		pbuffer +  13);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00102,
		2,
		pbuffer +  15);
	VL53L1_FCTN_00118(
		pdata->VL53L1_PRM_00084 & 0x1FFF,
		2,
		pbuffer +  17);
	VL53L1_FCTN_00118(
		pdata->VL53L1_PRM_00082,
		2,
		pbuffer +  19);
	VL53L1_FCTN_00118(
		pdata->VL53L1_PRM_00083,
		2,
		pbuffer +  21);
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00149(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_customer_nvm_managed_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00097 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00053 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00054 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00055 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00056 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00057 =
		(*(pbuffer +   4));
	pdata->VL53L1_PRM_00058 =
		(*(pbuffer +   5)) & 0xF;
	pdata->VL53L1_PRM_00329 =
		(*(pbuffer +   6));
	pdata->VL53L1_PRM_00051 =
		(*(pbuffer +   7)) & 0x3F;
	pdata->VL53L1_PRM_00052 =
		(*(pbuffer +   8)) & 0x3;
	pdata->VL53L1_PRM_00080 =
		(VL53L1_FCTN_00105(2, pbuffer +   9));
	pdata->VL53L1_PRM_00078 =
		(VL53L1_FCTN_00119(2, pbuffer +  11));
	pdata->VL53L1_PRM_00079 =
		(VL53L1_FCTN_00119(2, pbuffer +  13));
	pdata->VL53L1_PRM_00102 =
		(VL53L1_FCTN_00105(2, pbuffer +  15));
	pdata->VL53L1_PRM_00084 =
		(VL53L1_FCTN_00119(2, pbuffer +  17)) & 0x1FFF;
	pdata->VL53L1_PRM_00082 =
		(VL53L1_FCTN_00119(2, pbuffer +  19));
	pdata->VL53L1_PRM_00083 =
		(VL53L1_FCTN_00119(2, pbuffer +  21));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00027(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00097];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00085(
			pdata,
			VL53L1_DEF_00097,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00034,
			comms_buffer,
			VL53L1_DEF_00097);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00044(
	VL53L1_DEV                 Dev,
	VL53L1_customer_nvm_managed_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00097];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00034,
			comms_buffer,
			VL53L1_DEF_00097);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00149(
			VL53L1_DEF_00097,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00086(
	VL53L1_static_config_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00098 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00204,
		2,
		pbuffer +   0);
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00205 & 0x1;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00206 & 0xF;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00207 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00208 & 0x1F;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00209 & 0x7F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00210 & 0x7F;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00211 & 0x1;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00212;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00213 & 0x1;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00214 & 0x3;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00103 & 0x1F;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00215 & 0x3;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00216 & 0x3;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00217 & 0x7;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00143 & 0x1F;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00218 & 0x1;
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00219;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00220;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00200;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00221;
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00222;
	*(pbuffer +  23) =
		pdata->VL53L1_PRM_00223;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00224,
		2,
		pbuffer +  24);
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00225;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00226;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00203 & 0xF;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00227;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00228 & 0xF;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00229;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00150(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_static_config_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00098 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00204 =
		(VL53L1_FCTN_00105(2, pbuffer +   0));
	pdata->VL53L1_PRM_00205 =
		(*(pbuffer +   2)) & 0x1;
	pdata->VL53L1_PRM_00206 =
		(*(pbuffer +   3)) & 0xF;
	pdata->VL53L1_PRM_00207 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00208 =
		(*(pbuffer +   5)) & 0x1F;
	pdata->VL53L1_PRM_00209 =
		(*(pbuffer +   6)) & 0x7F;
	pdata->VL53L1_PRM_00210 =
		(*(pbuffer +   7)) & 0x7F;
	pdata->VL53L1_PRM_00211 =
		(*(pbuffer +   8)) & 0x1;
	pdata->VL53L1_PRM_00212 =
		(*(pbuffer +   9));
	pdata->VL53L1_PRM_00213 =
		(*(pbuffer +  10)) & 0x1;
	pdata->VL53L1_PRM_00214 =
		(*(pbuffer +  11)) & 0x3;
	pdata->VL53L1_PRM_00103 =
		(*(pbuffer +  12)) & 0x1F;
	pdata->VL53L1_PRM_00215 =
		(*(pbuffer +  13)) & 0x3;
	pdata->VL53L1_PRM_00216 =
		(*(pbuffer +  14)) & 0x3;
	pdata->VL53L1_PRM_00217 =
		(*(pbuffer +  15)) & 0x7;
	pdata->VL53L1_PRM_00143 =
		(*(pbuffer +  16)) & 0x1F;
	pdata->VL53L1_PRM_00218 =
		(*(pbuffer +  17)) & 0x1;
	pdata->VL53L1_PRM_00219 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00220 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00200 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00221 =
		(*(pbuffer +  21));
	pdata->VL53L1_PRM_00222 =
		(*(pbuffer +  22));
	pdata->VL53L1_PRM_00223 =
		(*(pbuffer +  23));
	pdata->VL53L1_PRM_00224 =
		(VL53L1_FCTN_00105(2, pbuffer +  24));
	pdata->VL53L1_PRM_00225 =
		(*(pbuffer +  26));
	pdata->VL53L1_PRM_00226 =
		(*(pbuffer +  27));
	pdata->VL53L1_PRM_00203 =
		(*(pbuffer +  28)) & 0xF;
	pdata->VL53L1_PRM_00227 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00228 =
		(*(pbuffer +  30)) & 0xF;
	pdata->VL53L1_PRM_00229 =
		(*(pbuffer +  31));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00151(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00098];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00086(
			pdata,
			VL53L1_DEF_00098,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00176,
			comms_buffer,
			VL53L1_DEF_00098);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00152(
	VL53L1_DEV                 Dev,
	VL53L1_static_config_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00098];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00176,
			comms_buffer,
			VL53L1_DEF_00098);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00150(
			VL53L1_DEF_00098,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00087(
	VL53L1_general_config_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00099 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00230;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00119;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00231;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00140 & 0x7F;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00232 & 0xFFF,
		2,
		pbuffer +   4);
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00142 & 0x7F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00100;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00233;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00234 & 0x1;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00235 & 0x7;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00236,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00237,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00238,
		2,
		pbuffer +  16);
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00239;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00240;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00241;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00242;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00153(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_general_config_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00099 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00230 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00119 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00231 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00140 =
		(*(pbuffer +   3)) & 0x7F;
	pdata->VL53L1_PRM_00232 =
		(VL53L1_FCTN_00105(2, pbuffer +   4)) & 0xFFF;
	pdata->VL53L1_PRM_00142 =
		(*(pbuffer +   6)) & 0x7F;
	pdata->VL53L1_PRM_00100 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00233 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00234 =
		(*(pbuffer +   9)) & 0x1;
	pdata->VL53L1_PRM_00235 =
		(*(pbuffer +  11)) & 0x7;
	pdata->VL53L1_PRM_00236 =
		(VL53L1_FCTN_00105(2, pbuffer +  12));
	pdata->VL53L1_PRM_00237 =
		(VL53L1_FCTN_00105(2, pbuffer +  14));
	pdata->VL53L1_PRM_00238 =
		(VL53L1_FCTN_00105(2, pbuffer +  16));
	pdata->VL53L1_PRM_00239 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00240 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00241 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00242 =
		(*(pbuffer +  21));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00154(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00099];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00087(
			pdata,
			VL53L1_DEF_00099,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00177,
			comms_buffer,
			VL53L1_DEF_00099);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00155(
	VL53L1_DEV                 Dev,
	VL53L1_general_config_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00099];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00177,
			comms_buffer,
			VL53L1_DEF_00099);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00153(
			VL53L1_DEF_00099,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00088(
	VL53L1_timing_config_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00100 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00243 & 0xF;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00244;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00245 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00246;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00146 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00147;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00101 & 0x3F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00148 & 0xF;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00149;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00150 & 0x3F;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00013,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00012,
		2,
		pbuffer +  12);
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00247;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00248;
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00114,
		4,
		pbuffer +  18);
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00249 & 0x1;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00156(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_timing_config_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00100 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00243 =
		(*(pbuffer +   0)) & 0xF;
	pdata->VL53L1_PRM_00244 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00245 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00246 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00146 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00147 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00101 =
		(*(pbuffer +   6)) & 0x3F;
	pdata->VL53L1_PRM_00148 =
		(*(pbuffer +   7)) & 0xF;
	pdata->VL53L1_PRM_00149 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00150 =
		(*(pbuffer +   9)) & 0x3F;
	pdata->VL53L1_PRM_00013 =
		(VL53L1_FCTN_00105(2, pbuffer +  10));
	pdata->VL53L1_PRM_00012 =
		(VL53L1_FCTN_00105(2, pbuffer +  12));
	pdata->VL53L1_PRM_00247 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00248 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00114 =
		(VL53L1_FCTN_00106(4, pbuffer +  18));
	pdata->VL53L1_PRM_00249 =
		(*(pbuffer +  22)) & 0x1;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00157(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00100];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00088(
			pdata,
			VL53L1_DEF_00100,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00178,
			comms_buffer,
			VL53L1_DEF_00100);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00158(
	VL53L1_DEV                 Dev,
	VL53L1_timing_config_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00100];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00178,
			comms_buffer,
			VL53L1_DEF_00100);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00156(
			VL53L1_DEF_00100,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00089(
	VL53L1_dynamic_config_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00101 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00125 & 0x3;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00273,
		2,
		pbuffer +   1);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00274,
		2,
		pbuffer +   3);
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00275 & 0x1;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00276 & 0x7;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00277;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00278;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00279 & 0x7F;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00280 & 0x7F;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00126 & 0x3;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00281 & 0x3;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00282 & 0xF;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00115;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00116;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00023;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00127 & 0x3;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00159(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_dynamic_config_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00101 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00125 =
		(*(pbuffer +   0)) & 0x3;
	pdata->VL53L1_PRM_00273 =
		(VL53L1_FCTN_00105(2, pbuffer +   1));
	pdata->VL53L1_PRM_00274 =
		(VL53L1_FCTN_00105(2, pbuffer +   3));
	pdata->VL53L1_PRM_00275 =
		(*(pbuffer +   5)) & 0x1;
	pdata->VL53L1_PRM_00276 =
		(*(pbuffer +   6)) & 0x7;
	pdata->VL53L1_PRM_00277 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00278 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00279 =
		(*(pbuffer +   9)) & 0x7F;
	pdata->VL53L1_PRM_00280 =
		(*(pbuffer +  10)) & 0x7F;
	pdata->VL53L1_PRM_00126 =
		(*(pbuffer +  11)) & 0x3;
	pdata->VL53L1_PRM_00281 =
		(*(pbuffer +  12)) & 0x3;
	pdata->VL53L1_PRM_00282 =
		(*(pbuffer +  13)) & 0xF;
	pdata->VL53L1_PRM_00115 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00116 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00023 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00127 =
		(*(pbuffer +  17)) & 0x3;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00160(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00101];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00089(
			pdata,
			VL53L1_DEF_00101,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00179,
			comms_buffer,
			VL53L1_DEF_00101);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00161(
	VL53L1_DEV                 Dev,
	VL53L1_dynamic_config_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00101];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00179,
			comms_buffer,
			VL53L1_DEF_00101);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00159(
			VL53L1_DEF_00101,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00090(
	VL53L1_system_control_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00095 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00286 & 0x1;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00283 & 0x1;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00284 & 0x1;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00285 & 0x3;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00122;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00162(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_control_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00095 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00286 =
		(*(pbuffer +   0)) & 0x1;
	pdata->VL53L1_PRM_00283 =
		(*(pbuffer +   1)) & 0x1;
	pdata->VL53L1_PRM_00284 =
		(*(pbuffer +   2)) & 0x1;
	pdata->VL53L1_PRM_00285 =
		(*(pbuffer +   3)) & 0x3;
	pdata->VL53L1_PRM_00122 =
		(*(pbuffer +   4));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00093(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00095];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00090(
			pdata,
			VL53L1_DEF_00095,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00156,
			comms_buffer,
			VL53L1_DEF_00095);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00163(
	VL53L1_DEV                 Dev,
	VL53L1_system_control_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00095];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00156,
			comms_buffer,
			VL53L1_DEF_00095);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00162(
			VL53L1_DEF_00095,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00164(
	VL53L1_system_results_t  *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00109 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00132 & 0x3F;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00105;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00106 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00026;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00162,
		2,
		pbuffer +   4);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00297,
		2,
		pbuffer +   6);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00166,
		2,
		pbuffer +   8);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00167,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00169,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00170,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00163,
		2,
		pbuffer +  16);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00298,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00299,
		2,
		pbuffer +  20);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00165,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00177,
		2,
		pbuffer +  24);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00178,
		2,
		pbuffer +  26);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00179,
		2,
		pbuffer +  28);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00180,
		2,
		pbuffer +  30);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00181,
		2,
		pbuffer +  32);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00182,
		2,
		pbuffer +  34);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00300,
		2,
		pbuffer +  36);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00301,
		2,
		pbuffer +  38);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00302,
		2,
		pbuffer +  40);
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00303;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00330;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00097(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_system_results_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00109 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00132 =
		(*(pbuffer +   0)) & 0x3F;
	pdata->VL53L1_PRM_00105 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00106 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00026 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00162 =
		(VL53L1_FCTN_00105(2, pbuffer +   4));
	pdata->VL53L1_PRM_00297 =
		(VL53L1_FCTN_00105(2, pbuffer +   6));
	pdata->VL53L1_PRM_00166 =
		(VL53L1_FCTN_00105(2, pbuffer +   8));
	pdata->VL53L1_PRM_00167 =
		(VL53L1_FCTN_00105(2, pbuffer +  10));
	pdata->VL53L1_PRM_00169 =
		(VL53L1_FCTN_00105(2, pbuffer +  12));
	pdata->VL53L1_PRM_00170 =
		(VL53L1_FCTN_00105(2, pbuffer +  14));
	pdata->VL53L1_PRM_00163 =
		(VL53L1_FCTN_00105(2, pbuffer +  16));
	pdata->VL53L1_PRM_00298 =
		(VL53L1_FCTN_00105(2, pbuffer +  18));
	pdata->VL53L1_PRM_00299 =
		(VL53L1_FCTN_00105(2, pbuffer +  20));
	pdata->VL53L1_PRM_00165 =
		(VL53L1_FCTN_00105(2, pbuffer +  22));
	pdata->VL53L1_PRM_00177 =
		(VL53L1_FCTN_00105(2, pbuffer +  24));
	pdata->VL53L1_PRM_00178 =
		(VL53L1_FCTN_00105(2, pbuffer +  26));
	pdata->VL53L1_PRM_00179 =
		(VL53L1_FCTN_00105(2, pbuffer +  28));
	pdata->VL53L1_PRM_00180 =
		(VL53L1_FCTN_00105(2, pbuffer +  30));
	pdata->VL53L1_PRM_00181 =
		(VL53L1_FCTN_00105(2, pbuffer +  32));
	pdata->VL53L1_PRM_00182 =
		(VL53L1_FCTN_00105(2, pbuffer +  34));
	pdata->VL53L1_PRM_00300 =
		(VL53L1_FCTN_00105(2, pbuffer +  36));
	pdata->VL53L1_PRM_00301 =
		(VL53L1_FCTN_00105(2, pbuffer +  38));
	pdata->VL53L1_PRM_00302 =
		(VL53L1_FCTN_00105(2, pbuffer +  40));
	pdata->VL53L1_PRM_00303 =
		(*(pbuffer +  42));
	pdata->VL53L1_PRM_00330 =
		(*(pbuffer +  43));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00165(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00109];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00164(
			pdata,
			VL53L1_DEF_00109,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00180,
			comms_buffer,
			VL53L1_DEF_00109);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00166(
	VL53L1_DEV                 Dev,
	VL53L1_system_results_t   *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00109];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00180,
			comms_buffer,
			VL53L1_DEF_00109);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00097(
			VL53L1_DEF_00109,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00167(
	VL53L1_core_results_t    *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00108 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00176,
		4,
		pbuffer +   0);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00172,
		4,
		pbuffer +   4);
	VL53L1_FCTN_00122(
		pdata->VL53L1_PRM_00173,
		4,
		pbuffer +   8);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00174,
		4,
		pbuffer +  12);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00186,
		4,
		pbuffer +  16);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00183,
		4,
		pbuffer +  20);
	VL53L1_FCTN_00122(
		pdata->VL53L1_PRM_00184,
		4,
		pbuffer +  24);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00185,
		4,
		pbuffer +  28);
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00331;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00096(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_core_results_t     *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00108 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00176 =
		(VL53L1_FCTN_00106(4, pbuffer +   0));
	pdata->VL53L1_PRM_00172 =
		(VL53L1_FCTN_00106(4, pbuffer +   4));
	pdata->VL53L1_PRM_00173 =
		(VL53L1_FCTN_00123(4, pbuffer +   8));
	pdata->VL53L1_PRM_00174 =
		(VL53L1_FCTN_00106(4, pbuffer +  12));
	pdata->VL53L1_PRM_00186 =
		(VL53L1_FCTN_00106(4, pbuffer +  16));
	pdata->VL53L1_PRM_00183 =
		(VL53L1_FCTN_00106(4, pbuffer +  20));
	pdata->VL53L1_PRM_00184 =
		(VL53L1_FCTN_00123(4, pbuffer +  24));
	pdata->VL53L1_PRM_00185 =
		(VL53L1_FCTN_00106(4, pbuffer +  28));
	pdata->VL53L1_PRM_00331 =
		(*(pbuffer +  32));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00168(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00108];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00167(
			pdata,
			VL53L1_DEF_00108,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00181,
			comms_buffer,
			VL53L1_DEF_00108);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00169(
	VL53L1_DEV                 Dev,
	VL53L1_core_results_t     *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00108];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00181,
			comms_buffer,
			VL53L1_DEF_00108);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00096(
			VL53L1_DEF_00108,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00170(
	VL53L1_debug_results_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00105 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00134,
		2,
		pbuffer +   0);
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00135 & 0x7F;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00048 & 0x3F;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00049 & 0x3;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00332 & 0x1;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00333 & 0x3F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00334 & 0x3F;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00113 & 0x3FF,
		2,
		pbuffer +   8);
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00335 & 0x3;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00336 & 0x3;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00337 & 0xF;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00338 & 0x7;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00339 & 0x1;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00340 & 0x3;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00341;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00342;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00343 & 0xFFF,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00344,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00345,
		2,
		pbuffer +  24);
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00346 & 0x1;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00347 & 0x7;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00348;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00349;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00350 & 0x7F;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00351 & 0x7F;
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00352 & 0x3;
	*(pbuffer +  33) =
		pdata->VL53L1_PRM_00353 & 0xF;
	*(pbuffer +  34) =
		pdata->VL53L1_PRM_00354;
	*(pbuffer +  35) =
		pdata->VL53L1_PRM_00355;
	*(pbuffer +  36) =
		pdata->VL53L1_PRM_00356;
	*(pbuffer +  37) =
		pdata->VL53L1_PRM_00357 & 0x1;
	*(pbuffer +  38) =
		pdata->VL53L1_PRM_00358 & 0x3;
	*(pbuffer +  39) =
		pdata->VL53L1_PRM_00315 & 0x1F;
	*(pbuffer +  40) =
		pdata->VL53L1_PRM_00316 & 0x1F;
	*(pbuffer +  41) =
		pdata->VL53L1_PRM_00317 & 0x1F;
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00318 & 0x1;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00319 & 0x1;
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00359 & 0x3;
	*(pbuffer +  45) =
		pdata->VL53L1_PRM_00360 & 0x3F;
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00131 & 0x3FFFF,
		4,
		pbuffer +  46);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00361,
		4,
		pbuffer +  50);
	*(pbuffer +  54) =
		pdata->VL53L1_PRM_00362 & 0x1;
	*(pbuffer +  55) =
		pdata->VL53L1_PRM_00363 & 0x1;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00095(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_debug_results_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00105 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00134 =
		(VL53L1_FCTN_00105(2, pbuffer +   0));
	pdata->VL53L1_PRM_00135 =
		(*(pbuffer +   2)) & 0x7F;
	pdata->VL53L1_PRM_00048 =
		(*(pbuffer +   3)) & 0x3F;
	pdata->VL53L1_PRM_00049 =
		(*(pbuffer +   4)) & 0x3;
	pdata->VL53L1_PRM_00332 =
		(*(pbuffer +   5)) & 0x1;
	pdata->VL53L1_PRM_00333 =
		(*(pbuffer +   6)) & 0x3F;
	pdata->VL53L1_PRM_00334 =
		(*(pbuffer +   7)) & 0x3F;
	pdata->VL53L1_PRM_00113 =
		(VL53L1_FCTN_00105(2, pbuffer +   8)) & 0x3FF;
	pdata->VL53L1_PRM_00335 =
		(*(pbuffer +  10)) & 0x3;
	pdata->VL53L1_PRM_00336 =
		(*(pbuffer +  11)) & 0x3;
	pdata->VL53L1_PRM_00337 =
		(*(pbuffer +  12)) & 0xF;
	pdata->VL53L1_PRM_00338 =
		(*(pbuffer +  13)) & 0x7;
	pdata->VL53L1_PRM_00339 =
		(*(pbuffer +  14)) & 0x1;
	pdata->VL53L1_PRM_00340 =
		(*(pbuffer +  15)) & 0x3;
	pdata->VL53L1_PRM_00341 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00342 =
		(*(pbuffer +  17));
	pdata->VL53L1_PRM_00343 =
		(VL53L1_FCTN_00105(2, pbuffer +  18)) & 0xFFF;
	pdata->VL53L1_PRM_00344 =
		(VL53L1_FCTN_00105(2, pbuffer +  22));
	pdata->VL53L1_PRM_00345 =
		(VL53L1_FCTN_00105(2, pbuffer +  24));
	pdata->VL53L1_PRM_00346 =
		(*(pbuffer +  26)) & 0x1;
	pdata->VL53L1_PRM_00347 =
		(*(pbuffer +  27)) & 0x7;
	pdata->VL53L1_PRM_00348 =
		(*(pbuffer +  28));
	pdata->VL53L1_PRM_00349 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00350 =
		(*(pbuffer +  30)) & 0x7F;
	pdata->VL53L1_PRM_00351 =
		(*(pbuffer +  31)) & 0x7F;
	pdata->VL53L1_PRM_00352 =
		(*(pbuffer +  32)) & 0x3;
	pdata->VL53L1_PRM_00353 =
		(*(pbuffer +  33)) & 0xF;
	pdata->VL53L1_PRM_00354 =
		(*(pbuffer +  34));
	pdata->VL53L1_PRM_00355 =
		(*(pbuffer +  35));
	pdata->VL53L1_PRM_00356 =
		(*(pbuffer +  36));
	pdata->VL53L1_PRM_00357 =
		(*(pbuffer +  37)) & 0x1;
	pdata->VL53L1_PRM_00358 =
		(*(pbuffer +  38)) & 0x3;
	pdata->VL53L1_PRM_00315 =
		(*(pbuffer +  39)) & 0x1F;
	pdata->VL53L1_PRM_00316 =
		(*(pbuffer +  40)) & 0x1F;
	pdata->VL53L1_PRM_00317 =
		(*(pbuffer +  41)) & 0x1F;
	pdata->VL53L1_PRM_00318 =
		(*(pbuffer +  42)) & 0x1;
	pdata->VL53L1_PRM_00319 =
		(*(pbuffer +  43)) & 0x1;
	pdata->VL53L1_PRM_00359 =
		(*(pbuffer +  44)) & 0x3;
	pdata->VL53L1_PRM_00360 =
		(*(pbuffer +  45)) & 0x3F;
	pdata->VL53L1_PRM_00131 =
		(VL53L1_FCTN_00106(4, pbuffer +  46)) & 0x3FFFF;
	pdata->VL53L1_PRM_00361 =
		(VL53L1_FCTN_00106(4, pbuffer +  50));
	pdata->VL53L1_PRM_00362 =
		(*(pbuffer +  54)) & 0x1;
	pdata->VL53L1_PRM_00363 =
		(*(pbuffer +  55)) & 0x1;

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00171(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00105];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00170(
			pdata,
			VL53L1_DEF_00105,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00113,
			comms_buffer,
			VL53L1_DEF_00105);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00172(
	VL53L1_DEV                 Dev,
	VL53L1_debug_results_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00105];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00113,
			comms_buffer,
			VL53L1_DEF_00105);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00095(
			VL53L1_DEF_00105,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00173(
	VL53L1_nvm_copy_data_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00182 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00364;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00004;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00003;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00365,
		2,
		pbuffer +   3);
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00366 & 0x7F;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00367 & 0x7;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00368 & 0x7;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00369 & 0x3F;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00370 & 0x3F;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00371 & 0x1;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00372 & 0x7F;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00373 & 0x1;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00374 & 0x3F;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00375 & 0x3F;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00376;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00377;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00378;
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00379;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00380;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00381;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00382;
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00383;
	*(pbuffer +  23) =
		pdata->VL53L1_PRM_00384;
	*(pbuffer +  24) =
		pdata->VL53L1_PRM_00385;
	*(pbuffer +  25) =
		pdata->VL53L1_PRM_00386;
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00387;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00388;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00389;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00390;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00391;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00392;
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00393;
	*(pbuffer +  33) =
		pdata->VL53L1_PRM_00394;
	*(pbuffer +  34) =
		pdata->VL53L1_PRM_00395;
	*(pbuffer +  35) =
		pdata->VL53L1_PRM_00396;
	*(pbuffer +  36) =
		pdata->VL53L1_PRM_00397;
	*(pbuffer +  37) =
		pdata->VL53L1_PRM_00398;
	*(pbuffer +  38) =
		pdata->VL53L1_PRM_00399;
	*(pbuffer +  39) =
		pdata->VL53L1_PRM_00400;
	*(pbuffer +  40) =
		pdata->VL53L1_PRM_00401;
	*(pbuffer +  41) =
		pdata->VL53L1_PRM_00402;
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00403;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00404;
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00405;
	*(pbuffer +  45) =
		pdata->VL53L1_PRM_00406;
	*(pbuffer +  46) =
		pdata->VL53L1_PRM_00407;
	*(pbuffer +  47) =
		pdata->VL53L1_PRM_00117;
	*(pbuffer +  48) =
		pdata->VL53L1_PRM_00118;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00174(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_nvm_copy_data_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00182 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00364 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00004 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00003 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00365 =
		(VL53L1_FCTN_00105(2, pbuffer +   3));
	pdata->VL53L1_PRM_00366 =
		(*(pbuffer +   5)) & 0x7F;
	pdata->VL53L1_PRM_00367 =
		(*(pbuffer +   6)) & 0x7;
	pdata->VL53L1_PRM_00368 =
		(*(pbuffer +   7)) & 0x7;
	pdata->VL53L1_PRM_00369 =
		(*(pbuffer +   8)) & 0x3F;
	pdata->VL53L1_PRM_00370 =
		(*(pbuffer +   9)) & 0x3F;
	pdata->VL53L1_PRM_00371 =
		(*(pbuffer +  10)) & 0x1;
	pdata->VL53L1_PRM_00372 =
		(*(pbuffer +  11)) & 0x7F;
	pdata->VL53L1_PRM_00373 =
		(*(pbuffer +  12)) & 0x1;
	pdata->VL53L1_PRM_00374 =
		(*(pbuffer +  13)) & 0x3F;
	pdata->VL53L1_PRM_00375 =
		(*(pbuffer +  14)) & 0x3F;
	pdata->VL53L1_PRM_00376 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00377 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00378 =
		(*(pbuffer +  17));
	pdata->VL53L1_PRM_00379 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00380 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00381 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00382 =
		(*(pbuffer +  21));
	pdata->VL53L1_PRM_00383 =
		(*(pbuffer +  22));
	pdata->VL53L1_PRM_00384 =
		(*(pbuffer +  23));
	pdata->VL53L1_PRM_00385 =
		(*(pbuffer +  24));
	pdata->VL53L1_PRM_00386 =
		(*(pbuffer +  25));
	pdata->VL53L1_PRM_00387 =
		(*(pbuffer +  26));
	pdata->VL53L1_PRM_00388 =
		(*(pbuffer +  27));
	pdata->VL53L1_PRM_00389 =
		(*(pbuffer +  28));
	pdata->VL53L1_PRM_00390 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00391 =
		(*(pbuffer +  30));
	pdata->VL53L1_PRM_00392 =
		(*(pbuffer +  31));
	pdata->VL53L1_PRM_00393 =
		(*(pbuffer +  32));
	pdata->VL53L1_PRM_00394 =
		(*(pbuffer +  33));
	pdata->VL53L1_PRM_00395 =
		(*(pbuffer +  34));
	pdata->VL53L1_PRM_00396 =
		(*(pbuffer +  35));
	pdata->VL53L1_PRM_00397 =
		(*(pbuffer +  36));
	pdata->VL53L1_PRM_00398 =
		(*(pbuffer +  37));
	pdata->VL53L1_PRM_00399 =
		(*(pbuffer +  38));
	pdata->VL53L1_PRM_00400 =
		(*(pbuffer +  39));
	pdata->VL53L1_PRM_00401 =
		(*(pbuffer +  40));
	pdata->VL53L1_PRM_00402 =
		(*(pbuffer +  41));
	pdata->VL53L1_PRM_00403 =
		(*(pbuffer +  42));
	pdata->VL53L1_PRM_00404 =
		(*(pbuffer +  43));
	pdata->VL53L1_PRM_00405 =
		(*(pbuffer +  44));
	pdata->VL53L1_PRM_00406 =
		(*(pbuffer +  45));
	pdata->VL53L1_PRM_00407 =
		(*(pbuffer +  46));
	pdata->VL53L1_PRM_00117 =
		(*(pbuffer +  47));
	pdata->VL53L1_PRM_00118 =
		(*(pbuffer +  48));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00175(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00182];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00173(
			pdata,
			VL53L1_DEF_00182,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00183,
			comms_buffer,
			VL53L1_DEF_00182);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00045(
	VL53L1_DEV                 Dev,
	VL53L1_nvm_copy_data_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00182];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00183,
			comms_buffer,
			VL53L1_DEF_00182);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00174(
			VL53L1_DEF_00182,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00176(
	VL53L1_prev_shadow_system_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00184 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00408 & 0x3F;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00409;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00410 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00411;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00412,
		2,
		pbuffer +   4);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00413,
		2,
		pbuffer +   6);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00414,
		2,
		pbuffer +   8);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00415,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00416,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00417,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00418,
		2,
		pbuffer +  16);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00419,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00420,
		2,
		pbuffer +  20);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00421,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00422,
		2,
		pbuffer +  24);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00423,
		2,
		pbuffer +  26);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00424,
		2,
		pbuffer +  28);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00425,
		2,
		pbuffer +  30);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00426,
		2,
		pbuffer +  32);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00427,
		2,
		pbuffer +  34);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00428,
		2,
		pbuffer +  36);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00429,
		2,
		pbuffer +  38);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00430,
		2,
		pbuffer +  40);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00431,
		2,
		pbuffer +  42);
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00177(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_system_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00184 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00408 =
		(*(pbuffer +   0)) & 0x3F;
	pdata->VL53L1_PRM_00409 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00410 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00411 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00412 =
		(VL53L1_FCTN_00105(2, pbuffer +   4));
	pdata->VL53L1_PRM_00413 =
		(VL53L1_FCTN_00105(2, pbuffer +   6));
	pdata->VL53L1_PRM_00414 =
		(VL53L1_FCTN_00105(2, pbuffer +   8));
	pdata->VL53L1_PRM_00415 =
		(VL53L1_FCTN_00105(2, pbuffer +  10));
	pdata->VL53L1_PRM_00416 =
		(VL53L1_FCTN_00105(2, pbuffer +  12));
	pdata->VL53L1_PRM_00417 =
		(VL53L1_FCTN_00105(2, pbuffer +  14));
	pdata->VL53L1_PRM_00418 =
		(VL53L1_FCTN_00105(2, pbuffer +  16));
	pdata->VL53L1_PRM_00419 =
		(VL53L1_FCTN_00105(2, pbuffer +  18));
	pdata->VL53L1_PRM_00420 =
		(VL53L1_FCTN_00105(2, pbuffer +  20));
	pdata->VL53L1_PRM_00421 =
		(VL53L1_FCTN_00105(2, pbuffer +  22));
	pdata->VL53L1_PRM_00422 =
		(VL53L1_FCTN_00105(2, pbuffer +  24));
	pdata->VL53L1_PRM_00423 =
		(VL53L1_FCTN_00105(2, pbuffer +  26));
	pdata->VL53L1_PRM_00424 =
		(VL53L1_FCTN_00105(2, pbuffer +  28));
	pdata->VL53L1_PRM_00425 =
		(VL53L1_FCTN_00105(2, pbuffer +  30));
	pdata->VL53L1_PRM_00426 =
		(VL53L1_FCTN_00105(2, pbuffer +  32));
	pdata->VL53L1_PRM_00427 =
		(VL53L1_FCTN_00105(2, pbuffer +  34));
	pdata->VL53L1_PRM_00428 =
		(VL53L1_FCTN_00105(2, pbuffer +  36));
	pdata->VL53L1_PRM_00429 =
		(VL53L1_FCTN_00105(2, pbuffer +  38));
	pdata->VL53L1_PRM_00430 =
		(VL53L1_FCTN_00105(2, pbuffer +  40));
	pdata->VL53L1_PRM_00431 =
		(VL53L1_FCTN_00105(2, pbuffer +  42));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00178(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00184];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00176(
			pdata,
			VL53L1_DEF_00184,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00185,
			comms_buffer,
			VL53L1_DEF_00184);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00179(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_system_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00184];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00185,
			comms_buffer,
			VL53L1_DEF_00184);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00177(
			VL53L1_DEF_00184,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00180(
	VL53L1_prev_shadow_core_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00186 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00432,
		4,
		pbuffer +   0);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00433,
		4,
		pbuffer +   4);
	VL53L1_FCTN_00122(
		pdata->VL53L1_PRM_00434,
		4,
		pbuffer +   8);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00435,
		4,
		pbuffer +  12);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00436,
		4,
		pbuffer +  16);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00437,
		4,
		pbuffer +  20);
	VL53L1_FCTN_00122(
		pdata->VL53L1_PRM_00438,
		4,
		pbuffer +  24);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00439,
		4,
		pbuffer +  28);
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00440;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00181(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_prev_shadow_core_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00186 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00432 =
		(VL53L1_FCTN_00106(4, pbuffer +   0));
	pdata->VL53L1_PRM_00433 =
		(VL53L1_FCTN_00106(4, pbuffer +   4));
	pdata->VL53L1_PRM_00434 =
		(VL53L1_FCTN_00123(4, pbuffer +   8));
	pdata->VL53L1_PRM_00435 =
		(VL53L1_FCTN_00106(4, pbuffer +  12));
	pdata->VL53L1_PRM_00436 =
		(VL53L1_FCTN_00106(4, pbuffer +  16));
	pdata->VL53L1_PRM_00437 =
		(VL53L1_FCTN_00106(4, pbuffer +  20));
	pdata->VL53L1_PRM_00438 =
		(VL53L1_FCTN_00123(4, pbuffer +  24));
	pdata->VL53L1_PRM_00439 =
		(VL53L1_FCTN_00106(4, pbuffer +  28));
	pdata->VL53L1_PRM_00440 =
		(*(pbuffer +  32));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00182(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00186];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00180(
			pdata,
			VL53L1_DEF_00186,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00187,
			comms_buffer,
			VL53L1_DEF_00186);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00183(
	VL53L1_DEV                 Dev,
	VL53L1_prev_shadow_core_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00186];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00187,
			comms_buffer,
			VL53L1_DEF_00186);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00181(
			VL53L1_DEF_00186,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00184(
	VL53L1_patch_debug_t     *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00188 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00441;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00442;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00185(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_debug_t      *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00188 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00441 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00442 =
		(*(pbuffer +   1));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00186(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00188];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00184(
			pdata,
			VL53L1_DEF_00188,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00189,
			comms_buffer,
			VL53L1_DEF_00188);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00187(
	VL53L1_DEV                 Dev,
	VL53L1_patch_debug_t      *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00188];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00189,
			comms_buffer,
			VL53L1_DEF_00188);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00185(
			VL53L1_DEF_00188,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00188(
	VL53L1_gph_general_config_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00190 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00443,
		2,
		pbuffer +   0);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00444,
		2,
		pbuffer +   2);
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00445;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00189(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_general_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00190 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00443 =
		(VL53L1_FCTN_00105(2, pbuffer +   0));
	pdata->VL53L1_PRM_00444 =
		(VL53L1_FCTN_00105(2, pbuffer +   2));
	pdata->VL53L1_PRM_00445 =
		(*(pbuffer +   4));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00190(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00190];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00188(
			pdata,
			VL53L1_DEF_00190,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00191,
			comms_buffer,
			VL53L1_DEF_00190);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00191(
	VL53L1_DEV                 Dev,
	VL53L1_gph_general_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00190];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00191,
			comms_buffer,
			VL53L1_DEF_00190);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00189(
			VL53L1_DEF_00190,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00192(
	VL53L1_gph_static_config_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00192 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00446 & 0x7;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00447,
		2,
		pbuffer +   1);
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00448;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00449;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00450;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00193(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_static_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00192 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00446 =
		(*(pbuffer +   0)) & 0x7;
	pdata->VL53L1_PRM_00447 =
		(VL53L1_FCTN_00105(2, pbuffer +   1));
	pdata->VL53L1_PRM_00448 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00449 =
		(*(pbuffer +   4));
	pdata->VL53L1_PRM_00450 =
		(*(pbuffer +   5));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00194(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00192];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00192(
			pdata,
			VL53L1_DEF_00192,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00193,
			comms_buffer,
			VL53L1_DEF_00192);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00195(
	VL53L1_DEV                 Dev,
	VL53L1_gph_static_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00192];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00193,
			comms_buffer,
			VL53L1_DEF_00192);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00193(
			VL53L1_DEF_00192,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00196(
	VL53L1_gph_timing_config_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00194 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00451 & 0xF;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00452;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00453 & 0xF;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00454;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00455 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00456;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00457 & 0x3F;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00458 & 0x3F;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00459 & 0xF;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00460;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00461,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00462,
		2,
		pbuffer +  12);
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00463;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00464;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00197(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_gph_timing_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00194 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00451 =
		(*(pbuffer +   0)) & 0xF;
	pdata->VL53L1_PRM_00452 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00453 =
		(*(pbuffer +   2)) & 0xF;
	pdata->VL53L1_PRM_00454 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00455 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00456 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00457 =
		(*(pbuffer +   6)) & 0x3F;
	pdata->VL53L1_PRM_00458 =
		(*(pbuffer +   7)) & 0x3F;
	pdata->VL53L1_PRM_00459 =
		(*(pbuffer +   8)) & 0xF;
	pdata->VL53L1_PRM_00460 =
		(*(pbuffer +   9));
	pdata->VL53L1_PRM_00461 =
		(VL53L1_FCTN_00105(2, pbuffer +  10));
	pdata->VL53L1_PRM_00462 =
		(VL53L1_FCTN_00105(2, pbuffer +  12));
	pdata->VL53L1_PRM_00463 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00464 =
		(*(pbuffer +  15));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00198(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00194];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00196(
			pdata,
			VL53L1_DEF_00194,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00195,
			comms_buffer,
			VL53L1_DEF_00194);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00199(
	VL53L1_DEV                 Dev,
	VL53L1_gph_timing_config_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00194];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00195,
			comms_buffer,
			VL53L1_DEF_00194);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00197(
			VL53L1_DEF_00194,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00200(
	VL53L1_fw_internal_t     *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00196 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00465;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00466;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00201(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_fw_internal_t      *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00196 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00465 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00466 =
		(*(pbuffer +   1));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00202(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00196];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00200(
			pdata,
			VL53L1_DEF_00196,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00197,
			comms_buffer,
			VL53L1_DEF_00196);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00203(
	VL53L1_DEV                 Dev,
	VL53L1_fw_internal_t      *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00196];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00197,
			comms_buffer,
			VL53L1_DEF_00196);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00201(
			VL53L1_DEF_00196,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00204(
	VL53L1_patch_results_t   *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00198 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00467 & 0x3;
	*(pbuffer +   1) =
		pdata->VL53L1_PRM_00468;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00469;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00470;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00471;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00472;
	*(pbuffer +   6) =
		pdata->VL53L1_PRM_00473;
	*(pbuffer +   7) =
		pdata->VL53L1_PRM_00474;
	*(pbuffer +   8) =
		pdata->VL53L1_PRM_00475;
	*(pbuffer +   9) =
		pdata->VL53L1_PRM_00476;
	*(pbuffer +  10) =
		pdata->VL53L1_PRM_00477;
	*(pbuffer +  11) =
		pdata->VL53L1_PRM_00478;
	*(pbuffer +  12) =
		pdata->VL53L1_PRM_00479;
	*(pbuffer +  13) =
		pdata->VL53L1_PRM_00480;
	*(pbuffer +  14) =
		pdata->VL53L1_PRM_00481;
	*(pbuffer +  15) =
		pdata->VL53L1_PRM_00482;
	*(pbuffer +  16) =
		pdata->VL53L1_PRM_00483;
	*(pbuffer +  17) =
		pdata->VL53L1_PRM_00484;
	*(pbuffer +  18) =
		pdata->VL53L1_PRM_00485;
	*(pbuffer +  19) =
		pdata->VL53L1_PRM_00486;
	*(pbuffer +  20) =
		pdata->VL53L1_PRM_00487;
	*(pbuffer +  21) =
		pdata->VL53L1_PRM_00488;
	*(pbuffer +  22) =
		pdata->VL53L1_PRM_00489;
	*(pbuffer +  23) =
		pdata->VL53L1_PRM_00490;
	*(pbuffer +  24) =
		pdata->VL53L1_PRM_00491;
	*(pbuffer +  25) =
		pdata->VL53L1_PRM_00492;
	*(pbuffer +  26) =
		pdata->VL53L1_PRM_00493;
	*(pbuffer +  27) =
		pdata->VL53L1_PRM_00494;
	*(pbuffer +  28) =
		pdata->VL53L1_PRM_00495;
	*(pbuffer +  29) =
		pdata->VL53L1_PRM_00496;
	*(pbuffer +  30) =
		pdata->VL53L1_PRM_00497;
	*(pbuffer +  31) =
		pdata->VL53L1_PRM_00498;
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00499;
	*(pbuffer +  33) =
		pdata->VL53L1_PRM_00500;
	*(pbuffer +  34) =
		pdata->VL53L1_PRM_00501;
	*(pbuffer +  35) =
		pdata->VL53L1_PRM_00502;
	*(pbuffer +  36) =
		pdata->VL53L1_PRM_00503;
	*(pbuffer +  37) =
		pdata->VL53L1_PRM_00504;
	*(pbuffer +  38) =
		pdata->VL53L1_PRM_00505;
	*(pbuffer +  39) =
		pdata->VL53L1_PRM_00506;
	*(pbuffer +  40) =
		pdata->VL53L1_PRM_00507;
	*(pbuffer +  41) =
		pdata->VL53L1_PRM_00508;
	*(pbuffer +  42) =
		pdata->VL53L1_PRM_00509;
	*(pbuffer +  43) =
		pdata->VL53L1_PRM_00510;
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00511;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00512,
		2,
		pbuffer +  46);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00513,
		4,
		pbuffer +  48);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00514,
		2,
		pbuffer +  52);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00515,
		2,
		pbuffer +  54);
	*(pbuffer +  56) =
		pdata->VL53L1_PRM_00516;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00517,
		2,
		pbuffer +  58);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00518,
		2,
		pbuffer +  62);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00519,
		2,
		pbuffer +  64);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00520,
		2,
		pbuffer +  66);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00521 & 0xFFFFFF,
		4,
		pbuffer +  68);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00522 & 0xFFFFFF,
		4,
		pbuffer +  72);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00523 & 0xFFFFFF,
		4,
		pbuffer +  76);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00524 & 0xFFFFFF,
		4,
		pbuffer +  80);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00525,
		4,
		pbuffer +  84);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00526,
		2,
		pbuffer +  88);
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00205(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_patch_results_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00198 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00467 =
		(*(pbuffer +   0)) & 0x3;
	pdata->VL53L1_PRM_00468 =
		(*(pbuffer +   1));
	pdata->VL53L1_PRM_00469 =
		(*(pbuffer +   2));
	pdata->VL53L1_PRM_00470 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00471 =
		(*(pbuffer +   4));
	pdata->VL53L1_PRM_00472 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00473 =
		(*(pbuffer +   6));
	pdata->VL53L1_PRM_00474 =
		(*(pbuffer +   7));
	pdata->VL53L1_PRM_00475 =
		(*(pbuffer +   8));
	pdata->VL53L1_PRM_00476 =
		(*(pbuffer +   9));
	pdata->VL53L1_PRM_00477 =
		(*(pbuffer +  10));
	pdata->VL53L1_PRM_00478 =
		(*(pbuffer +  11));
	pdata->VL53L1_PRM_00479 =
		(*(pbuffer +  12));
	pdata->VL53L1_PRM_00480 =
		(*(pbuffer +  13));
	pdata->VL53L1_PRM_00481 =
		(*(pbuffer +  14));
	pdata->VL53L1_PRM_00482 =
		(*(pbuffer +  15));
	pdata->VL53L1_PRM_00483 =
		(*(pbuffer +  16));
	pdata->VL53L1_PRM_00484 =
		(*(pbuffer +  17));
	pdata->VL53L1_PRM_00485 =
		(*(pbuffer +  18));
	pdata->VL53L1_PRM_00486 =
		(*(pbuffer +  19));
	pdata->VL53L1_PRM_00487 =
		(*(pbuffer +  20));
	pdata->VL53L1_PRM_00488 =
		(*(pbuffer +  21));
	pdata->VL53L1_PRM_00489 =
		(*(pbuffer +  22));
	pdata->VL53L1_PRM_00490 =
		(*(pbuffer +  23));
	pdata->VL53L1_PRM_00491 =
		(*(pbuffer +  24));
	pdata->VL53L1_PRM_00492 =
		(*(pbuffer +  25));
	pdata->VL53L1_PRM_00493 =
		(*(pbuffer +  26));
	pdata->VL53L1_PRM_00494 =
		(*(pbuffer +  27));
	pdata->VL53L1_PRM_00495 =
		(*(pbuffer +  28));
	pdata->VL53L1_PRM_00496 =
		(*(pbuffer +  29));
	pdata->VL53L1_PRM_00497 =
		(*(pbuffer +  30));
	pdata->VL53L1_PRM_00498 =
		(*(pbuffer +  31));
	pdata->VL53L1_PRM_00499 =
		(*(pbuffer +  32));
	pdata->VL53L1_PRM_00500 =
		(*(pbuffer +  33));
	pdata->VL53L1_PRM_00501 =
		(*(pbuffer +  34));
	pdata->VL53L1_PRM_00502 =
		(*(pbuffer +  35));
	pdata->VL53L1_PRM_00503 =
		(*(pbuffer +  36));
	pdata->VL53L1_PRM_00504 =
		(*(pbuffer +  37));
	pdata->VL53L1_PRM_00505 =
		(*(pbuffer +  38));
	pdata->VL53L1_PRM_00506 =
		(*(pbuffer +  39));
	pdata->VL53L1_PRM_00507 =
		(*(pbuffer +  40));
	pdata->VL53L1_PRM_00508 =
		(*(pbuffer +  41));
	pdata->VL53L1_PRM_00509 =
		(*(pbuffer +  42));
	pdata->VL53L1_PRM_00510 =
		(*(pbuffer +  43));
	pdata->VL53L1_PRM_00511 =
		(*(pbuffer +  44));
	pdata->VL53L1_PRM_00512 =
		(VL53L1_FCTN_00105(2, pbuffer +  46));
	pdata->VL53L1_PRM_00513 =
		(VL53L1_FCTN_00106(4, pbuffer +  48));
	pdata->VL53L1_PRM_00514 =
		(VL53L1_FCTN_00105(2, pbuffer +  52));
	pdata->VL53L1_PRM_00515 =
		(VL53L1_FCTN_00105(2, pbuffer +  54));
	pdata->VL53L1_PRM_00516 =
		(*(pbuffer +  56));
	pdata->VL53L1_PRM_00517 =
		(VL53L1_FCTN_00105(2, pbuffer +  58));
	pdata->VL53L1_PRM_00518 =
		(VL53L1_FCTN_00105(2, pbuffer +  62));
	pdata->VL53L1_PRM_00519 =
		(VL53L1_FCTN_00105(2, pbuffer +  64));
	pdata->VL53L1_PRM_00520 =
		(VL53L1_FCTN_00105(2, pbuffer +  66));
	pdata->VL53L1_PRM_00521 =
		(VL53L1_FCTN_00106(4, pbuffer +  68)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00522 =
		(VL53L1_FCTN_00106(4, pbuffer +  72)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00523 =
		(VL53L1_FCTN_00106(4, pbuffer +  76)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00524 =
		(VL53L1_FCTN_00106(4, pbuffer +  80)) & 0xFFFFFF;
	pdata->VL53L1_PRM_00525 =
		(VL53L1_FCTN_00106(4, pbuffer +  84));
	pdata->VL53L1_PRM_00526 =
		(VL53L1_FCTN_00105(2, pbuffer +  88));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00206(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00198];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00204(
			pdata,
			VL53L1_DEF_00198,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00199,
			comms_buffer,
			VL53L1_DEF_00198);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00207(
	VL53L1_DEV                 Dev,
	VL53L1_patch_results_t    *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00198];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00199,
			comms_buffer,
			VL53L1_DEF_00198);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00205(
			VL53L1_DEF_00198,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00208(
	VL53L1_shadow_system_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00200 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	*(pbuffer +   0) =
		pdata->VL53L1_PRM_00527;
	*(pbuffer +   2) =
		pdata->VL53L1_PRM_00528 & 0x3F;
	*(pbuffer +   3) =
		pdata->VL53L1_PRM_00529;
	*(pbuffer +   4) =
		pdata->VL53L1_PRM_00530 & 0xF;
	*(pbuffer +   5) =
		pdata->VL53L1_PRM_00531;
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00532,
		2,
		pbuffer +   6);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00533,
		2,
		pbuffer +   8);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00534,
		2,
		pbuffer +  10);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00535,
		2,
		pbuffer +  12);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00536,
		2,
		pbuffer +  14);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00537,
		2,
		pbuffer +  16);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00538,
		2,
		pbuffer +  18);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00539,
		2,
		pbuffer +  20);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00540,
		2,
		pbuffer +  22);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00541,
		2,
		pbuffer +  24);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00542,
		2,
		pbuffer +  26);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00543,
		2,
		pbuffer +  28);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00544,
		2,
		pbuffer +  30);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00545,
		2,
		pbuffer +  32);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00546,
		2,
		pbuffer +  34);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00547,
		2,
		pbuffer +  36);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00548,
		2,
		pbuffer +  38);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00549,
		2,
		pbuffer +  40);
	VL53L1_FCTN_00117(
		pdata->VL53L1_PRM_00550,
		2,
		pbuffer +  42);
	*(pbuffer +  44) =
		pdata->VL53L1_PRM_00551;
	*(pbuffer +  45) =
		pdata->VL53L1_PRM_00552;
	*(pbuffer +  80) =
		pdata->VL53L1_PRM_00553;
	*(pbuffer +  81) =
		pdata->VL53L1_PRM_00554;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00209(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_system_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00200 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00527 =
		(*(pbuffer +   0));
	pdata->VL53L1_PRM_00528 =
		(*(pbuffer +   2)) & 0x3F;
	pdata->VL53L1_PRM_00529 =
		(*(pbuffer +   3));
	pdata->VL53L1_PRM_00530 =
		(*(pbuffer +   4)) & 0xF;
	pdata->VL53L1_PRM_00531 =
		(*(pbuffer +   5));
	pdata->VL53L1_PRM_00532 =
		(VL53L1_FCTN_00105(2, pbuffer +   6));
	pdata->VL53L1_PRM_00533 =
		(VL53L1_FCTN_00105(2, pbuffer +   8));
	pdata->VL53L1_PRM_00534 =
		(VL53L1_FCTN_00105(2, pbuffer +  10));
	pdata->VL53L1_PRM_00535 =
		(VL53L1_FCTN_00105(2, pbuffer +  12));
	pdata->VL53L1_PRM_00536 =
		(VL53L1_FCTN_00105(2, pbuffer +  14));
	pdata->VL53L1_PRM_00537 =
		(VL53L1_FCTN_00105(2, pbuffer +  16));
	pdata->VL53L1_PRM_00538 =
		(VL53L1_FCTN_00105(2, pbuffer +  18));
	pdata->VL53L1_PRM_00539 =
		(VL53L1_FCTN_00105(2, pbuffer +  20));
	pdata->VL53L1_PRM_00540 =
		(VL53L1_FCTN_00105(2, pbuffer +  22));
	pdata->VL53L1_PRM_00541 =
		(VL53L1_FCTN_00105(2, pbuffer +  24));
	pdata->VL53L1_PRM_00542 =
		(VL53L1_FCTN_00105(2, pbuffer +  26));
	pdata->VL53L1_PRM_00543 =
		(VL53L1_FCTN_00105(2, pbuffer +  28));
	pdata->VL53L1_PRM_00544 =
		(VL53L1_FCTN_00105(2, pbuffer +  30));
	pdata->VL53L1_PRM_00545 =
		(VL53L1_FCTN_00105(2, pbuffer +  32));
	pdata->VL53L1_PRM_00546 =
		(VL53L1_FCTN_00105(2, pbuffer +  34));
	pdata->VL53L1_PRM_00547 =
		(VL53L1_FCTN_00105(2, pbuffer +  36));
	pdata->VL53L1_PRM_00548 =
		(VL53L1_FCTN_00105(2, pbuffer +  38));
	pdata->VL53L1_PRM_00549 =
		(VL53L1_FCTN_00105(2, pbuffer +  40));
	pdata->VL53L1_PRM_00550 =
		(VL53L1_FCTN_00105(2, pbuffer +  42));
	pdata->VL53L1_PRM_00551 =
		(*(pbuffer +  44));
	pdata->VL53L1_PRM_00552 =
		(*(pbuffer +  45));
	pdata->VL53L1_PRM_00553 =
		(*(pbuffer +  80));
	pdata->VL53L1_PRM_00554 =
		(*(pbuffer +  81));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00210(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00200];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00208(
			pdata,
			VL53L1_DEF_00200,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00201,
			comms_buffer,
			VL53L1_DEF_00200);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00211(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_system_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00200];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00201,
			comms_buffer,
			VL53L1_DEF_00200);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00209(
			VL53L1_DEF_00200,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00212(
	VL53L1_shadow_core_results_t *pdata,
	uint16_t                  buf_size,
	uint8_t                  *pbuffer)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00202 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00555,
		4,
		pbuffer +   0);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00556,
		4,
		pbuffer +   4);
	VL53L1_FCTN_00122(
		pdata->VL53L1_PRM_00557,
		4,
		pbuffer +   8);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00558,
		4,
		pbuffer +  12);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00559,
		4,
		pbuffer +  16);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00560,
		4,
		pbuffer +  20);
	VL53L1_FCTN_00122(
		pdata->VL53L1_PRM_00561,
		4,
		pbuffer +  24);
	VL53L1_FCTN_00120(
		pdata->VL53L1_PRM_00562,
		4,
		pbuffer +  28);
	*(pbuffer +  32) =
		pdata->VL53L1_PRM_00563;
	LOG_FUNCTION_END(status);


	return status;
}


VL53L1_Error VL53L1_FCTN_00213(
	uint16_t                   buf_size,
	uint8_t                   *pbuffer,
	VL53L1_shadow_core_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;

	LOG_FUNCTION_START("");

	if (VL53L1_DEF_00202 > buf_size)
		return VL53L1_ERROR_COMMS_BUFFER_TOO_SMALL;

	pdata->VL53L1_PRM_00555 =
		(VL53L1_FCTN_00106(4, pbuffer +   0));
	pdata->VL53L1_PRM_00556 =
		(VL53L1_FCTN_00106(4, pbuffer +   4));
	pdata->VL53L1_PRM_00557 =
		(VL53L1_FCTN_00123(4, pbuffer +   8));
	pdata->VL53L1_PRM_00558 =
		(VL53L1_FCTN_00106(4, pbuffer +  12));
	pdata->VL53L1_PRM_00559 =
		(VL53L1_FCTN_00106(4, pbuffer +  16));
	pdata->VL53L1_PRM_00560 =
		(VL53L1_FCTN_00106(4, pbuffer +  20));
	pdata->VL53L1_PRM_00561 =
		(VL53L1_FCTN_00123(4, pbuffer +  24));
	pdata->VL53L1_PRM_00562 =
		(VL53L1_FCTN_00106(4, pbuffer +  28));
	pdata->VL53L1_PRM_00563 =
		(*(pbuffer +  32));

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00214(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00202];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00212(
			pdata,
			VL53L1_DEF_00202,
			comms_buffer);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_WriteMulti(
			Dev,
			VL53L1_DEF_00203,
			comms_buffer,
			VL53L1_DEF_00202);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	LOG_FUNCTION_END(status);

	return status;
}


VL53L1_Error VL53L1_FCTN_00215(
	VL53L1_DEV                 Dev,
	VL53L1_shadow_core_results_t  *pdata)
{






	VL53L1_Error status = VL53L1_ERROR_NONE;
	uint8_t comms_buffer[VL53L1_DEF_00202];

	LOG_FUNCTION_START("");

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00036(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_ReadMulti(
			Dev,
			VL53L1_DEF_00203,
			comms_buffer,
			VL53L1_DEF_00202);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00037(Dev);

	if (status == VL53L1_ERROR_NONE)
		status = VL53L1_FCTN_00213(
			VL53L1_DEF_00202,
			comms_buffer,
			pdata);

	LOG_FUNCTION_END(status);

	return status;
}

