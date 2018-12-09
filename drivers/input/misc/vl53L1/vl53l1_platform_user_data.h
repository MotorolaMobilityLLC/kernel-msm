/**************************************************************************
 * Copyright (c) 2016, STMicroelectronics - All Rights Reserved

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************/


#ifndef _VL53L1_PLATFORM_USER_DATA_H_
#define _VL53L1_PLATFORM_USER_DATA_H_

#include "vl53l1_ll_def.h"

#include <linux/string.h>
#include "vl53l1_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include <linux/string.h>

#define VL53L1_Dev_t VL53L1_DevData_t
#define VL53L1_DEV VL53L1_DevData_t *

#define VL53L1DevDataGet(Dev, field) (Dev->field)
#define VL53L1DevDataSet(Dev, field, data) ((Dev->field) = (data))

#define VL53L1DevStructGetLLDriverHandle(Dev) (&VL53L1DevDataGet(Dev, LLData))
#define VL53L1DevStructGetLLResultsHandle(Dev) (&VL53L1DevDataGet(Dev,\
		llresults))

#ifdef __cplusplus
}
#endif

#endif

