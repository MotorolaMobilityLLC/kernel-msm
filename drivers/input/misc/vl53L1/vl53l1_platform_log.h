/*******************************************************************************
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
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

*/
/**
 * @file  vl53l1_platform_log.h
 *
 * @brief EwokPlus25 platform logging function definition
 *
 * st bare/ll driver logging capability can be turn on  by defining
 * @ref VL53L1_LOG_ENABLE if so minimal list below are required
 * @li  _LOG_FUNCTION_START( ... )
 * @li  _LOG_FUNCTION_END(... )
 * @li  _LOG_FUNCTION_END_FMT( ... )
 * @li   VL53L1_trace_print_module_function(...)
 * check the file for full prototype
 */


#ifndef _VL53L1_PLATFORM_LOG_H_
#define _VL53L1_PLATFORM_LOG_H_

#include <vl53l1_def.h>

/* LOG Functions */

#ifdef __cplusplus
extern "C" {
#endif



enum {
	VL53L1_TRACE_LEVEL_NONE,
	VL53L1_TRACE_LEVEL_ERRORS,
	VL53L1_TRACE_LEVEL_WARNING,
	VL53L1_TRACE_LEVEL_INFO,
	VL53L1_TRACE_LEVEL_DEBUG,
	VL53L1_TRACE_LEVEL_ALL,
	VL53L1_TRACE_LEVEL_IGNORE
};

enum {
	VL53L1_TRACE_FUNCTION_NONE = 0,
	VL53L1_TRACE_FUNCTION_I2C  = 1,
	VL53L1_TRACE_FUNCTION_ALL  = 0x7fffffff
	/* all bits except sign */
};

enum {
	VL53L1_TRACE_MODULE_NONE               = 0x0,
	VL53L1_TRACE_MODULE_API                = 0x1,
	VL53L1_TRACE_MODULE_CORE               = 0x2,
	VL53L1_TRACE_MODULE_PROTECTED          = 0x3,
	VL53L1_TRACE_MODULE_HISTOGRAM          = 0x4,
	VL53L1_TRACE_MODULE_REGISTERS          = 0x5,
	VL53L1_TRACE_MODULE_PLATFORM           = 0x6,

	VL53L1_TRACE_MODULE_ALL               = 0x7fffffff
	/* all bits except sign */
};

#ifdef VL53L1_LOG_ENABLE



/**
 * @brief Set the level, output and specific functions for module logging.
 *
 *
 * @param filename  - full path of output log file, NULL for print to stdout
 *
 * @param modules   - Module or None or All to trace
 *                      VL53L1_TRACE_MODULE_NONE
 *                      VL53L1_TRACE_MODULE_API
 *                      VL53L1_TRACE_MODULE_CORE
 *                      VL53L1_TRACE_MODULE_TUNING
 *                      VL53L1_TRACE_MODULE_CHARACTERISATION
 *                      VL53L1_TRACE_MODULE_PLATFORM
 *                      VL53L1_TRACE_MODULE_ALL
 *
 * @param level     - trace level
 *                      VL53L1_TRACE_LEVEL_NONE
 *                      VL53L1_TRACE_LEVEL_ERRORS
 *                      VL53L1_TRACE_LEVEL_WARNING
 *                      VL53L1_TRACE_LEVEL_INFO
 *                      VL53L1_TRACE_LEVEL_DEBUG
 *                      VL53L1_TRACE_LEVEL_ALL
 *                      VL53L1_TRACE_LEVEL_IGNORE
 *
 *  @param functions - function level to trace
 *                      VL53L1_TRACE_FUNCTION_NONE
 *                      VL53L1_TRACE_FUNCTION_I2C
 *                      VL53L1_TRACE_FUNCTION_ALL
 *
 * @return status - always VL53L1_ERROR_NONE
 *
 */


/**
 * @brief Print trace module function.
 *
 * @param module   - ??
 * @param level    - ??
 * @param function - ??
 * @param format   - ??
 *
 */

VL53L1_API void VL53L1_trace_print_module_function(uint32_t module,
		uint32_t level, uint32_t function,  const char *format, ...);

/**
 * @brief Get global _trace_modules parameter
 *
 * @return _trace_modules
 */

VL53L1_API uint32_t VL53L1_get_trace_modules(void);

/**
 * @brief Set global _trace_modules parameter
 *
 * @param[in] module : new module to trace
 */

VL53L1_API void VL53L1_set_trace_modules(uint32_t module);

/**
 * @brief Get global _trace_level parameter
 *
 * @return _trace_level
 */

VL53L1_API uint32_t VL53L1_get_trace_level(void);

/**
 * @brief Set global _trace_level parameter
 *
 * @param[in] level : new  trace level
 */

VL53L1_API void VL53L1_set_trace_level(uint32_t level);

/**
 * @brief Get global _trace_functions parameter
 *
 * @return _trace_functions
 */

VL53L1_API uint32_t VL53L1_get_trace_functions(void);

/**
 * @brief Set global _trace_functions parameter
 *
 * @param[in] function : new function code
 */

VL53L1_API void VL53L1_set_trace_functions(uint32_t function);


/**
 * @brief Returns the current system tick count in [ms]
 *
 * @return  time_ms : current time in [ms]
 *
 */

VL53L1_API uint32_t VL53L1_clock(void);


#define LOG_GET_TIME() (int)VL53L1_clock()

#define _LOG_FUNCTION_START(module, fmt, ...) \
	VL53L1_trace_print_module_function(module, _trace_level,\
		VL53L1_TRACE_FUNCTION_ALL, "%6ld <START> %s "fmt"\n",\
		LOG_GET_TIME(), __func__, ##__VA_ARGS__);

#define _LOG_FUNCTION_END(module, status, ...)\
	VL53L1_trace_print_module_function(module, _trace_level, \
	VL53L1_TRACE_FUNCTION_ALL, "%6ld <END> %s %d\n", LOG_GET_TIME(),\
	__func__, (int)status, ##__VA_ARGS__)

#define _LOG_FUNCTION_END_FMT(module, status, fmt, ...)\
	VL53L1_trace_print_module_function(module, _trace_level, \
		VL53L1_TRACE_FUNCTION_ALL, "%6ld <END> %s %d "fmt"\n", \
		LOG_GET_TIME(),  __func__, (int)status, ##__VA_ARGS__)


#else /* VL53L1_LOG_ENABLE no logging */
	#define _LOG_FUNCTION_START(...) (void)0
	#define _LOG_FUNCTION_END(...) (void)0
	#define _LOG_FUNCTION_END_FMT(...) (void)0
	#define VL53L1_trace_print_module_function(...) (void)0
#endif /* else */



#ifdef __cplusplus
}
#endif

#endif  /* _VL53L1_PLATFORM_LOG_H_ */



