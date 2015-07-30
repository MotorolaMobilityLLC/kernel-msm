/*******************************************************************************
Copyright © 2015, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/
/*
 * $Date: 2015-01-08 14:30:24 +0100 (Thu, 08 Jan 2015) $
 * $Revision: 2039 $
 */

#ifndef VL6180x_PLATFORM
#define VL6180x_PLATFORM


#include <linux/delay.h>

/**
 * @file vl6180x_platform.h
 *
 * @brief All end user OS/platform/application porting
 */

/** @defgroup api_platform Platform
 *  @brief Platform-dependent code
 */


/** @ingroup api_platform
 * @{*/

 /**
  * @brief For user convenience to place or give any required data attribute
  * to the built-in single device instance \n
  * Useful only when Configuration @a #VL6180x_SINGLE_DEVICE_DRIVER is active
  *
  * @ingroup api_platform
  */
#define VL6180x_DEV_DATA_ATTR

/**
 * @def ROMABLE_DATA
 * @brief API Read only data that can be place in rom/flash are declared with that extra keyword
 *
 * For user convenience, use compiler specific attribute or keyword to place all read-only data in required data area. \n
 * For example using gcc section  :
 *  @code
 *  #define ROMABLE_DATA  __attribute__ ((section ("user_rom")))
 *  // you may need to edit your link script file to place user_rom section in flash/rom memory
 *  @endcode
 *
 * @ingroup api_platform
 */
#define ROMABLE_DATA
/*  #define ROMABLE_DATA  __attribute__ ((section ("user_rom"))) */

/**
 * @def VL6180x_RANGE_STATUS_ERRSTRING
 * @brief  Activate error code translation into string.
 * TODO: michel to apdate
 * @ingroup api_platform
 */
#define VL6180x_RANGE_STATUS_ERRSTRING 1

/**
 * @def VL6180x_SINGLE_DEVICE_DRIVER
 * @brief Enable lightweight single vl6180x device driver.
 *
 * Value __1__ =>  Single device capable.
 * Configure optimized API for single device driver with static data and minimal use of ref pointer.\n
 * Limited to single device driver or application in non multi thread/core environment.\n
 *
 * Value __0__ =>  Multiple device capable. User must review "device" structure and type in vl6180x_platform.h files.
 * @ingroup api_platform
 */
#define VL6180x_SINGLE_DEVICE_DRIVER 1

/**
 * @def VL6180X_SAFE_POLLING_ENTER
 * @brief Ensure safe polling method when set
 *
 * Polling for a condition can be hazardous and result in infinite looping if any previous interrupt status
 * condition is not cleared. \n
 * Setting these flags enforce error clearing on start of polling method to avoid it.
 * the drawback are : \n
 * @li extra use-less i2c bus usage and traffic
 * @li potentially slower measure rate.
 * If application ensures interrupt get cleared on mode or interrupt configuration change
 * then keep option disabled. \n
 * To be safe set these option to 1
 * @ingroup api_platform
 */
#define VL6180X_SAFE_POLLING_ENTER  0

/**
 * @def VL6180x_MULTI_READ
 * @brief Enable I2C multi read support
 *
 * When set to 1, multi read operations are done (when necessary) by the API functions (mainly WAF) to access a bunch of registers
 * instead of individual ones (for speed increase). This requires the @a VL6180x_RdMulti() function to be implemented.
 */
#define VL6180x_HAVE_MULTI_READ          1


/**
 * @def VL6180x_CACHED_REG
 * @brief Enable Cached Register mode
 *
 * In addition to the multi read mode (#VL6180x_MULTI_READ set to 1), this mode implements an advanced register access mode to speed-up
 * ranging measurements by reading all results registers in one shot (using multi read operation). All post-processing operations (like WAF)
 * are done by accessing the cached registers instead of doing individual register access.
 * @warning It is mandatory to set #VL6180x_MULTI_READ to 1 to benefit from this advanced mode

 */
#define VL6180x_CACHED_REG          1


/**
 * @brief Enable start/end logging facilities. It can generates traces log to help problem tracking analysis and solving
 *
 * Requires porting  @a #LOG_FUNCTION_START, @a #LOG_FUNCTION_END, @a #LOG_FUNCTION_END_FMT
 * @ingroup api_platform
 */
#define VL6180X_LOG_ENABLE  0

#include "vl6180x_def.h"
/* TODO: modify this */
#if VL6180x_SINGLE_DEVICE_DRIVER
/**
 * @typedef  VL6180xDev_t
 * @brief    Generic VL6180x device type that does link between API and platform abstraction layer
 *
 * @ingroup api_platform
 */
    /* //![device_type_int] */
	typedef uint8_t VL6180xDev_t; /* simplest single device example "dev" is the i2c device address in the platform AL*/
    /* //![device_type_int] */
	typedef VL6180xDev_t stmvl6180x_dev;

#else /* VL6180x_SINGLE_DEVICE_DRIVER */

/*! [device_type_multi] */

struct MyDev_t {
	struct VL6180xDevData_t Data;          /*!< embed ST VL6180 Dev  data as "Data"*/
    /* TODO ADD HERE any extra device data */
    /*!< user specific field */
	int     i2c_bus_num;                   /*!< i2c bus number user specific field */
	int     i2c_dev_addr;                  /*!< i2c devcie address user specific field */
	mutex_t dev_lock   ;                   /*!< mutex user specific field */
	int     i2c_file;                      /*!< sample i2c file handle */
};
typedef struct MyDev_t *VL6180xDev_t;

/**
 * @def VL6180xDevDataGet
 * @brief Get ST private structure @a VL6180xDevData_t data access (needed only in multi devices configuration)
 *
 * It may be used and a real data "ref" not just as "get" for sub-structure item
 * like VL6180xDevDataGet(FilterData.field)[i] or VL6180xDevDataGet(FilterData.MeasurementIndex)++
 * @param dev   The device
 * @param field ST structure filed name
 * @ingroup api_platform
 */
#define VL6180xDevDataGet(dev, field) (dev->Data.field)

/**
 * @def VL6180xDevDataSet(dev, field, data)
 * @brief  Set ST private structure @a VL6180xDevData_t data field (needed only in multi devices configuration)
 * @param dev    The device
 * @param field  ST structure field name
 * @param data   Data to be set
 * @ingroup api_platform
 */
#define VL6180xDevDataSet(dev, field, data) (dev->Data.field) = (data)
/*! [device_type_multi] */

#endif /* #else VL6180x_SINGLE_DEVICE_DRIVER */

/**
 * @brief execute delay in all polling api calls : @a VL6180x_RangePollMeasurement() and @a VL6180x_AlsPollMeasurement()
 *
 * A typical multi-thread or RTOs implementation is to sleep the task for some 5ms (with 100Hz max rate faster polling is not needed).
 * if nothing specific is needed, you can define it as an empty/void macro
 * @code
 * #define VL6180x_PollDelay(...) (void)0
 * @endcode
 * @param dev The device
 * @ingroup api_platform
 */
void VL6180x_PollDelay(VL6180xDev_t dev); /* usualy best implemanted a a real fucntion */

/** @def VL6180x_PollDelay
 *  @brief Default value : does nothing. Macro to be deleted it you implement a real function
 * @ingroup api_platform
 */
#define VL6180x_PollDelay(dev)  msleep(5)





#if VL6180X_LOG_ENABLE
#include <linux/module.h>
#define LOG_GET_TIME() (int)0

/**
 * @brief Log function start.
 *
 * @param  fmt  Text and printf formating for any extra argument.\n
 *              It can be " " when there is nothing else than the function entry to log.
 * @param  ...  (\__VA_ARGS__) extra argument if any, but it can be none.
 *
 * @note Formatting a string can take considerable amount of cycles. Typically "sprinting" 4 int arguments and function name take 5-10K on common 32 bit ÂµC\n
 * It depends on implementation. The tracing is very intrusive and resources consuming .
 *
 * Example of time logging using printf and LOG_GET_TIME() macro \n
 * @code
 * #define LOG_FUNCTION_START(fmt, ... )  printf("beg %s @%d\t" fmt "\n", __func__, LOG_GET_TIME(), ##__VA_ARGS__)
 * @endcode
 * @ingroup api_platform
 */
#define LOG_FUNCTION_START(fmt, ...) \
	printk(KERN_INFO"beg %s start @%d\t" fmt "\n", __func__, LOG_GET_TIME(), ##__VA_ARGS__)

/**
 * @brief  Logging function end with status.
 *
 * @param  status final status (%d)
 *
 * Example of time logging using printf and LOG_GET_TIME() macro \n
 * @code
 * #define LOG_FUNCTION_END(status)   printf("end %s @%d %d\n", __func__, LOG_GET_TIME(), (int)status)
 * @endcode

 * @ingroup api_platform
 */
#define LOG_FUNCTION_END(status)\
	printk(KERN_INFO"end %s @%d %d\n", __func__, LOG_GET_TIME(), (int)status)


/**
 * @brief  Log function end along with extra optional formated arguments.
 *
 * @param  status final status (%d)
 * @param  fmt printf format and string
 * @param  ... (\__VA_ARGS__) extra argument for printf or va_args type function
 *
 * Example of time logging using printf and LOG_GET_TIME() macro. \n
 * @code
 * #define LOG_FUNCTION_END_FMT(status, fmt, ... )  printf("End %s @%d %d\t"fmt"\n" , __func__, LOG_GET_TIME(), (int)status,##__VA_ARGS__)
 * @endcode

 * @ingroup api_platform
 */
#define LOG_FUNCTION_END_FMT(status, fmt, ...)\
	printk(KERN_INFO"End %s @%d %d\t"fmt"\n" , __func__, LOG_GET_TIME(), (int)status, ##__VA_ARGS__)


/**
 * @brief Log error raised in API
 *
 * Implementation may abort execution but it's not an API requirement
 *
 * @param  fmt      text and printf like format list
 * @param  ...      (\__VA_ARGS__)    optional variable to be formated
 *
 * Example of error logging using printf and LOG_GET_TIME() macro \n
 * @code
 * #define VL6180x_ErrLog(msg, ... ) printf("Err in  %s line %d @%dt" msg "\n" , __func__, __LINE__, LOG_GET_TIME(), msg ,##__VA_ARGS__)
 * @endcode
 * @ingroup api_platform
 */
#define VL6180x_ErrLog(fmt, ...)\
	do {\
		printk(KERN_ERR"ERR in %s line %d\n" fmt, __func__, __LINE__, ##__VA_ARGS__);\
	} while (0)

#else /* VL6180X_LOG_ENABLE no logging */
    #define LOG_FUNCTION_START(...) (void)0
    #define LOG_FUNCTION_END(...) (void)0
    #define LOG_FUNCTION_END_FMT(...) (void)0
    #define VL6180x_ErrLog(...) (void)0
#endif /* else */

#endif  /* VL6180x_PLATFORM */



