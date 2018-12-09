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


#ifndef _VL53L1_PLATFORM_USER_DEFINES_H_
#define _VL53L1_PLATFORM_USER_DEFINES_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef __KERNEL__
#include <linux/math64.h>
#endif

/**
 * @file   vl53l1_platform_user_defines.h
 *
 * @brief  All end user OS/platform/application definitions
 */


/**
 * @def do_division_u
 * @brief customer supplied division operation - 64-bit unsigned
 *
 * @param dividend      unsigned 64-bit numerator
 * @param divisor       unsigned 64-bit denominator
 */
#ifdef __KERNEL__
#define do_division_u(dividend, divisor) div64_u64(dividend, divisor)
#else
#define do_division_u(dividend, divisor) (dividend / divisor)
#endif

/**
 * @def do_division_s
 * @brief customer supplied division operation - 64-bit signed
 *
 * @param dividend      signed 64-bit numerator
 * @param divisor       signed 64-bit denominator
 */
#ifdef __KERNEL__
#define do_division_s(dividend, divisor) div64_s64(dividend, divisor)
#else
#define do_division_s(dividend, divisor) (dividend / divisor)
#endif

#define WARN_OVERRIDE_STATUS(__X__)\
	trace_print(VL53L1_TRACE_LEVEL_WARNING, #__X__)


#define DISABLE_WARNINGS()
#define ENABLE_WARNINGS()



#ifdef __cplusplus
}
#endif

#endif

