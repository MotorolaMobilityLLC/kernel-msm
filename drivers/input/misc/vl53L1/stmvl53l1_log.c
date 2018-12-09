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
/**
 * @file /stmvl53l1_module.c  vl53l1_module  ST VL53L1 linux kernel module
 *
 * This is implementation of low level driver trace support
 */
#include <linux/module.h>
#include <linux/printk.h>

#include "stmvl53l1.h"

#ifdef VL53L1_LOG_ENABLE

static bool trace_function;
static int trace_module;
static int trace_level;

module_param(trace_function, bool, 0644);
MODULE_PARM_DESC(trace_function,
	"allow tracing of low level function entry and exit");

module_param(trace_module, int, 0644);
MODULE_PARM_DESC(trace_module,
	"control tracing of low level per module");

module_param(trace_level, int, 0644);
MODULE_PARM_DESC(trace_level,
	"control tracing of low level per level");

void log_trace_print(uint32_t module, uint32_t level, uint32_t function,
	const char *format, ...)
{
	va_list args;

	if (function && !trace_function)
		return;

	if (!(module & trace_module))
		return;

	if (level > trace_level)
		return;

	va_start(args, format);
	vprintk(format, args);
	va_end(args);
}

#endif
