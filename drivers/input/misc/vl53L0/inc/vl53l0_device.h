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
*******************************************************************************/

/**
 * Device specific defines. To be adapted by implementer for the targeted device.
 */

#ifndef _VL53L0_DEVICE_H_
#define _VL53L0_DEVICE_H_

#include "vl53l0_types.h"


/** @defgroup VL53L0_DeviceError_group Device Error
 *  @brief Device Error code
 *
 *  This enum is Device specific it should be updated in the implementation
 *  Use @a VL53L0_GetStatusErrorString() to get the string.
 *  It is related to Status Register of the Device.
 *  @ingroup regdef
 *  @{
 */
typedef uint8_t VL53L0_DeviceError;

#define VL53L0_DEVICEERROR_NONE \
	((VL53L0_DeviceError)   0) /*!< 0  0b0000 NoError  */
#define VL53L0_DEVICEERROR_VCSELCONTINUITYTESTFAILURE \
	((VL53L0_DeviceError)   1) /*!< 1  0b0001  */
#define VL53L0_DEVICEERROR_VCSELWATCHDOGTESTFAILURE \
	((VL53L0_DeviceError)   2) /*!< 2  0b0010  */
#define VL53L0_DEVICEERROR_NOVHVVALUEFOUND \
	((VL53L0_DeviceError)   3) /*!< 3  0b0011  */
#define VL53L0_DEVICEERROR_MSRCNOTARGET \
	((VL53L0_DeviceError)   4) /*!< 4  0b0100  */
#define VL53L0_DEVICEERROR_MSRCMINIMUMSNR \
	((VL53L0_DeviceError)   5) /*!< 5  0b0101  */
#define VL53L0_DEVICEERROR_MSRCWRAPAROUND \
	((VL53L0_DeviceError)   6) /*!< 6  0b0110  */
#define VL53L0_DEVICEERROR_TCC \
	((VL53L0_DeviceError)   7) /*!< 7  0b0111  */
#define VL53L0_DEVICEERROR_RANGEAWRAPAROUND \
	((VL53L0_DeviceError)   8) /*!< 8  0b1000  */
#define VL53L0_DEVICEERROR_RANGEBWRAPAROUND \
	((VL53L0_DeviceError)   9) /*!< 9  0b1001  */
#define VL53L0_DEVICEERROR_MINCLIP \
	((VL53L0_DeviceError)   10) /*!< 10 0b1010  */
#define VL53L0_DEVICEERROR_RANGECOMPLETE \
	((VL53L0_DeviceError)   11) /*!< 11 0b1011  */
#define VL53L0_DEVICEERROR_ALGOUNDERFLOW \
	((VL53L0_DeviceError)   12) /*!< 12 0b1100  */
#define VL53L0_DEVICEERROR_ALGOOVERFLOW \
	((VL53L0_DeviceError)   13) /*!< 13 0b1101  */
#define VL53L0_DEVICEERROR_FINALSNRLIMIT \
	((VL53L0_DeviceError)   14) /*!< 14 0b1110  */
#define VL53L0_DEVICEERROR_NOTARGETIGNORE \
	((VL53L0_DeviceError)   15) /*!< 15 0b1111  */

/** @} */ /* end of VL53L0_DeviceError_group */

/** @defgroup VL53L0_CheckEnable_group Check Enable list
 *  @brief Check Enable code
 *
 *  Define used to specify the LimitCheckId.
 *  Use @a VL53L0_GetLimitCheckInfo() to get the string.
 *  @ingroup regdef
 *  @{
 */

#define VL53L0_CHECKENABLE_SNR                    0
#define VL53L0_CHECKENABLE_SIGMA                  1
#define VL53L0_CHECKENABLE_RANGE                  2
#define VL53L0_CHECKENABLE_SIGNAL_RATE            3

#define VL53L0_CHECKENABLE_NUMBER_OF_CHECKS       4

/** @} */ /* end of VL53L0_CheckEnable_group */


/** @defgroup VL53L0_GpioFunctionality_group Gpio Functionality
 *  @brief Defines the different functionalities for the device GPIO(s)
 *  @ingroup regdef
 *  @{
 */
typedef uint8_t VL53L0_GpioFunctionality;

#define VL53L0_GPIOFUNCTIONALITY_OFF \
	((VL53L0_GpioFunctionality)  0) /*!< NO Interrupt  */
#define VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW \
	((VL53L0_GpioFunctionality)  1) /*!< Level Low (value < thresh_low)  */
#define VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH \
	((VL53L0_GpioFunctionality)  2) /*!< Level High (value > thresh_high) */
#define VL53L0_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT \
	((VL53L0_GpioFunctionality)  3) /*!< Out Of Window (value < thresh_low
						OR value > thresh_high) */
#define VL53L0_GPIOFUNCTIONALITY_NEW_MEASURE_READY \
	((VL53L0_GpioFunctionality)  4) /*!< New Sample Ready  */

/** @} */ /* end of VL53L0_GpioFunctionality_group */


typedef struct {
	FixPoint1616_t OscFrequencyMHz;

	uint16_t LastEncodedTimeout;

	VL53L0_GpioFunctionality Pin0GpioFunctionality;

} VL53L0_DeviceSpecificParameters_t;


/* Device register map */

/** @defgroup VL53L0_DefineRegisters_group Define Registers
 *  @brief List of all the defined registers
 *  @ingroup regdef
 *  @{
 */
#define VL53L0_REG_SYSRANGE_START                  0x000
    /** mask existing bit in #VL53L0_REG_SYSRANGE_START*/
    #define VL53L0_REG_SYSRANGE_MODE_MASK          0x0F
    /** bit 0 in #VL53L0_REG_SYSRANGE_START write 1 toggle state in
		continuous mode and arm next shot in single shot mode */
    #define VL53L0_REG_SYSRANGE_MODE_START_STOP    0x01
    /** bit 1 write 0 in #VL53L0_REG_SYSRANGE_START set single shot mode */
    #define VL53L0_REG_SYSRANGE_MODE_SINGLESHOT    0x00
	/** bit 1 write 1 in #VL53L0_REG_SYSRANGE_START set back-to-back
		operation mode */
    #define VL53L0_REG_SYSRANGE_MODE_BACKTOBACK    0x02
	/** bit 2 write 1 in #VL53L0_REG_SYSRANGE_START set timed operation
		mode */
	#define VL53L0_REG_SYSRANGE_MODE_TIMED         0x04
	/** bit 3 write 1 in #VL53L0_REG_SYSRANGE_START set histogram
		operation mode */
	#define VL53L0_REG_SYSRANGE_MODE_HISTOGRAM     0x08


#define VL53L0_REG_SYSTEM_THRESH_HIGH               0x000C  /* NOSLC  2 bytes */
#define VL53L0_REG_SYSTEM_THRESH_LOW                0x000E  /* NOSLC  2 bytes */


/* FPGA bitstream */
#define VL53L0_REG_SYSTEM_SEQUENCE_CONFIG			0x0001
#define VL53L0_REG_SYSTEM_INTERMEASUREMENT_PERIOD	0x0004

#define VL53L0_REG_SYSTEM_REPORT_REQUEST	        0x0009
	#define	VL53L0_REG_SYSTEM_RANGEA_DATA			0x04
	#define	VL53L0_REG_SYSTEM_RANGEB_DATA			0x05

#define VL53L0_REG_SYSTEM_INTERRUPT_CONFIG_GPIO     0x000A
	#define VL53L0_REG_SYSTEM_INTERRUPT_GPIO_DISABLED		0x00
	#define VL53L0_REG_SYSTEM_INTERRUPT_GPIO_LEVEL_LOW		0x01
	#define VL53L0_REG_SYSTEM_INTERRUPT_GPIO_LEVEL_HIGH	0x02
	#define VL53L0_REG_SYSTEM_INTERRUPT_GPIO_OUT_OF_WINDOW	0x03
	#define VL53L0_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY	0x04

#define VL53L0_REG_GPIO_HV_MUX_ACTIVE_HIGH          0x0084

#define VL53L0_REG_SYSTEM_INTERRUPT_CLEAR	0x000B

/* Result registers */
#define VL53L0_REG_RESULT_INTERRUPT_STATUS		    0x0013
#define VL53L0_REG_RESULT_RANGE_STATUS		        0x0014

#define VL53L0_REG_RESULT_SIGNAL_COUNT_RATE_RET     0x001A
#define VL53L0_REG_RESULT_AMBIENT_COUNT_RATE_RET    0x001C
#define VL53L0_REG_RESULT_FINAL_RANGE		        0x001E

/* Algo register */
#define VL53L0_REG_ALGO_CROSSTALK_COMPENSATION_RATE	0x0020
#define VL53L0_REG_ALGO_RANGE_IGNORE_VALID_HEIGHT	0x0025
#define VL53L0_REG_ALGO_RANGE_IGNORE_THRESHOLD		0x0026
#define VL53L0_REG_ALGO_SNR_RATIO					0x0027
#define VL53L0_REG_ALGO_RANGE_CHECK_ENABLES		    0x0028

#define VL53L0_REG_ALGO_PART_TO_PART_RANGE_OFFSET	0x0029

#define VL53L0_REG_I2C_SLAVE_DEVICE_ADDRESS	        0x008a

/* MSRC registers */
#define VL53L0_REG_MSRC_CONFIG_COUNT		        0x0044
#define VL53L0_REG_MSRC_CONFIG_TIMEOUT		        0x0046
#define VL53L0_REG_MSRC_CONFIG_MIN_SNR		        0x0055
#define VL53L0_REG_MSRC_CONFIG_VALID_PHASE_LOW	    0x0047
#define VL53L0_REG_MSRC_CONFIG_VALID_PHASE_HIGH     0x0048

/* RANGE A registers */
#define VL53L0_REG_RNGA_CONFIG_VCSEL_PERIOD	0x0050
#define VL53L0_REG_RNGA_TIMEOUT_MSB			0x0051
#define VL53L0_REG_RNGA_TIMEOUT_LSB			0x0052
#define VL53L0_REG_RNGA_CONFIG_VALID_PHASE_LOW	0x0056
#define VL53L0_REG_RNGA_CONFIG_VALID_PHASE_HIGH	0x0057

/* RANGE B1 registers */
#define VL53L0_REG_RNGB1_CONFIG_VCSEL_PERIOD	0x0060
#define VL53L0_REG_RNGB1_TIMEOUT_MSB			0x0061
#define VL53L0_REG_RNGB1_TIMEOUT_LSB			0x0062
#define VL53L0_REG_RNGB1_CONFIG_VALID_PHASE_LOW	0x0066
#define VL53L0_REG_RNGB1_CONFIG_VALID_PHASE_HIGH	0x0067

/* RANGE B2 registers */
#define VL53L0_REG_RNGB2_CONFIG_VCSEL_PERIOD	0x0070
#define VL53L0_REG_RNGB2_TIMEOUT_MSB			0x0071
#define VL53L0_REG_RNGB2_TIMEOUT_LSB			0x0072
#define VL53L0_REG_RNGB2_CONFIG_VALID_PHASE_LOW	0x0076
#define VL53L0_REG_RNGB2_CONFIG_VALID_PHASE_HIGH	0x0077


#define VL53L0_REG_SOFT_RESET_GO2_SOFT_RESET_N	  0x00bf
#define VL53L0_REG_IDENTIFICATION_MODEL_ID        0x00c0
#define VL53L0_REG_IDENTIFICATION_MODEL_TYPE      0x00c1
#define VL53L0_REG_IDENTIFICATION_REVISION_ID     0x00c2
#define VL53L0_REG_IDENTIFICATION_MODULE_ID       0x00c3

#define VL53L0_REG_OSC_CALIBRATE_VAL              0x00f8

#define VL53L0_REG_FIRMWARE_MODE_STATUS		           0x00C5

#define VL53L0_REG_DYNAMIC_SPAD_ACTUAL_RTN_SPADS_INT   0x0016

#define VL53L0_SIGMA_ESTIMATE_MAX_VALUE                65535
/*equivalent to a range sigma of 655.35mm */

/*
 * Speed of light in um per 1E-10 Seconds
 */

#define VL53L0_SPEED_OF_LIGHT_IN_AIR 2997


/** @} */ /* end of VL53L0_DefineRegisters_group */

#endif

/* _VL53L0_DEVICE_H_ */


