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

/*
 * $Date: 2014-12-05 15:06:01 +0100 (Fri, 05 Dec 2014) $
 * $Revision: 1915 $
 */

/**
 * @file VL53L0_def.h
 *
 * @brief Type definitions for VL53L0 API.
 *
 */


#ifndef _VL53L0_DEF_H_
#define _VL53L0_DEF_H_


#ifdef __cplusplus
extern "C" {
#endif


/** PAL SPECIFICATION major version */
#define VL53L0_SPECIFICATION_VER_MAJOR   1
/** PAL SPECIFICATION minor version */
#define VL53L0_SPECIFICATION_VER_MINOR   2
/** PAL SPECIFICATION sub version */
#define VL53L0_SPECIFICATION_VER_SUB     7
/** PAL SPECIFICATION sub version */
#define VL53L0_SPECIFICATION_VER_REVISION 1440

/** VL53L0 PAL IMPLEMENTATION major version */
#define VL53L0_IMPLEMENTATION_VER_MAJOR   1
/** VL53L0 PAL IMPLEMENTATION minor version */
#define VL53L0_IMPLEMENTATION_VER_MINOR   0
/** VL53L0 PAL IMPLEMENTATION sub version */
#define VL53L0_IMPLEMENTATION_VER_SUB     6
/** VL53L0 PAL IMPLEMENTATION sub version */
#define VL53L0_IMPLEMENTATION_VER_REVISION    2915

#define VL53L0_DEFAULT_MAX_LOOP 100
#define VL53L0_MAX_STRING_LENGTH 32


#include "vl53l0_device.h"
#include "vl53l0_types.h"

#ifdef PAL_EXTENDED
/* The following include should be removed for external use */
#include "vl53l0_extended_ewok_def.h"
#endif

/****************************************
 * PRIVATE define do not edit
 ****************************************/

/** @brief Defines the parameters of the Get Version Functions
 */
typedef struct {
	uint32_t     revision; /*!< revision number */
	uint8_t      major;    /*!< major number */
	uint8_t      minor;    /*!< minor number */
	uint8_t      build;    /*!< build number */
} VL53L0_Version_t;


/** @brief Defines the parameters of the Get Device Info Functions
 */
typedef struct {
	char Name[VL53L0_MAX_STRING_LENGTH];
	/*!< Name of the Device e.g. Left_Distance */
	char Type[VL53L0_MAX_STRING_LENGTH];
	/*!< Type of the Device e.g VL53L0 */
	uint8_t ProductType;
	/*!< Product Type, VL53L0 = 1, VL53L1 = 2 */
	uint8_t ProductRevisionMajor;
	/*!< Product revision major */
	uint8_t ProductRevisionMinor;
	/*!< Product revision minor */
} VL53L0_DeviceInfo_t ;


/** @defgroup VL53L0_define_Error_group PAL Error and Warning code returned by
 *	API
 *  The following DEFINE are used to identify the PAL ERROR
 *  @{
 */

typedef int8_t VL53L0_Error;

#define VL53L0_ERROR_NONE                              ((VL53L0_Error)  0)
/*!< No Error found */
#define VL53L0_ERROR_CALIBRATION_WARNING               ((VL53L0_Error) - 1)
/*!< Warning invalid calibration data may be in used
\a  VL53L0_InitData()
\a VL53L0_GetOffsetCalibrationData
\a VL53L0_SetOffsetCalibrationData */
#define VL53L0_ERROR_MIN_CLIPPED                       ((VL53L0_Error) - 2)
/*!< Warning parameter passed was clipped to min before to be applied */
#define VL53L0_ERROR_UNDEFINED                         ((VL53L0_Error) - 3)
/*!< Unqualified error */
#define VL53L0_ERROR_INVALID_PARAMS                    ((VL53L0_Error) - 4)
/*!< Parameter passed is invalid or out of range */
#define VL53L0_ERROR_NOT_SUPPORTED                     ((VL53L0_Error) - 5)
/*!< Function is not supported in current mode or configuration */
#define VL53L0_ERROR_RANGE_ERROR                       ((VL53L0_Error) - 6)
/*!< Device report a ranging error interrupt status */
#define VL53L0_ERROR_TIME_OUT                          ((VL53L0_Error) - 7)
/*!< Aborted due to time out */
#define VL53L0_ERROR_MODE_NOT_SUPPORTED                ((VL53L0_Error) - 8)
/*!< Asked mode is not supported by the device */
#define VL53L0_ERROR_BUFFER_TOO_SMALL                  ((VL53L0_Error) - 9)
/*!< Buffer is too small */
#define VL53L0_ERROR_GPIO_NOT_EXISTING                 ((VL53L0_Error) - 10)
/*!< User tried to setup a non-existing GPIO pin */
#define VL53L0_ERROR_GPIO_FUNCTIONALITY_NOT_SUPPORTED  ((VL53L0_Error) - 11)
/*!< unsupported GPIO functionality */
#define VL53L0_ERROR_CONTROL_INTERFACE                 ((VL53L0_Error) - 20)
/*!< error reported from IO functions */
#define VL53L0_ERROR_INVALID_COMMAND                   ((VL53L0_Error) - 30)
/*!< The command is not allowed in the current device state (power down) */
#define VL53L0_ERROR_DIVISION_BY_ZERO                  ((VL53L0_Error) - 40)
/*!< In the function a division by zero occurs */
#define VL53L0_ERROR_NOT_IMPLEMENTED                   ((VL53L0_Error) - 99)
/*!< Tells requested functionality has not been implemented yet or not
	compatible with the device */
/** @} */ /* end of VL53L0_define_Error_group */


/** @defgroup VL53L0_define_DeviceModes_group Defines all possible modes for the
 *  device
 *  Defines all possible modes for the device
 *  @{
 */
typedef uint8_t VL53L0_DeviceModes;

#define VL53L0_DEVICEMODE_SINGLE_RANGING           ((VL53L0_DeviceModes)  0)
#define VL53L0_DEVICEMODE_CONTINUOUS_RANGING       ((VL53L0_DeviceModes)  1)
#define VL53L0_DEVICEMODE_SINGLE_HISTOGRAM         ((VL53L0_DeviceModes)  2)
#define VL53L0_DEVICEMODE_CONTINUOUS_TIMED_RANGING ((VL53L0_DeviceModes)  3)
#define VL53L0_DEVICEMODE_SINGLE_ALS               ((VL53L0_DeviceModes) 10)
#define VL53L0_DEVICEMODE_GPIO_DRIVE               ((VL53L0_DeviceModes) 20)
#define VL53L0_DEVICEMODE_GPIO_OSC                 ((VL53L0_DeviceModes) 21)
/* ... Modes to be added depending on device */
/** @} */ /* end of VL53L0_define_DeviceModes_group */



/** @defgroup VL53L0_define_HistogramModes_group Defines all possible Histogram
 *  modes for the device
 *  Defines all possible Histogram modes for the device
 *  @{
 */
typedef uint8_t VL53L0_HistogramModes;

#define VL53L0_HISTOGRAMMODE_DISABLED        ((VL53L0_HistogramModes) 0)
/*!< Histogram Disabled */
#define VL53L0_HISTOGRAMMODE_REFERENCE_ONLY  ((VL53L0_HistogramModes) 1)
/*!< Histogram Reference array only */
#define VL53L0_HISTOGRAMMODE_RETURN_ONLY     ((VL53L0_HistogramModes) 2)
/*!< Histogram Return array only */
#define VL53L0_HISTOGRAMMODE_BOTH            ((VL53L0_HistogramModes) 3)
/*!< Histogram both Reference and Return Arrays */

/** @} */ /* end of VL53L0_define_HistogramModes_group */


/** @defgroup VL53L0_define_PowerModes_group List of available Power Modes
 *  List of available Power Modes
 *  @{
 */

typedef uint8_t VL53L0_PowerModes;

#define VL53L0_POWERMODE_STANDBY_LEVEL1 ((VL53L0_PowerModes) 0)
/*!< Standby level 1 */
#define VL53L0_POWERMODE_STANDBY_LEVEL2 ((VL53L0_PowerModes) 1)
/*!< Standby level 2 */
#define VL53L0_POWERMODE_IDLE_LEVEL1    ((VL53L0_PowerModes) 2)
/*!< Idle level 1 */
#define VL53L0_POWERMODE_IDLE_LEVEL2    ((VL53L0_PowerModes) 3)
/*!< Idle level 2 */

/** @} */ /* end of VL53L0_define_PowerModes_group */


/** @defgroup VL53L0_define_CheckPosition_group List of available Positions to
 *  be used in checks functions
 *  List of available Positions to be used in checks functions
 *  @{
 */

typedef uint8_t VL53L0_CheckPosition;


#define VL53L0_CHECKPOSITION_EARLY           ((VL53L0_CheckPosition) 0)
/*!< Early Position */
#define VL53L0_CHECKPOSITION_FINAL           ((VL53L0_CheckPosition) 1)
/*!< Final Position */
#define VL53L0_CHECKPOSITION_NO_OF_CHECKS    ((VL53L0_CheckPosition) 2)
/*!< Number of Checks */

/** @} */ /* end of VL53L0_define_PowerModes_group */


/** @brief Defines all parameters for the device
 */
typedef struct {
	VL53L0_DeviceModes DeviceMode;
	/*!< Defines type of measurement to be done for the next measure */
	VL53L0_HistogramModes HistogramMode;
	/*!< Defines type of histogram measurement to be done for the next
		measure */
	uint32_t MeasurementTimingBudgetMicroSeconds;
	/*!< Defines the allowed total time for a single measurement */
	uint32_t InterMeasurementPeriodMilliSeconds;
	/*!< Defines time between two consecutive measurements (between two
		measurement starts).If set to 0 means back-to-back mode */
	uint8_t XTalkCompensationEnable;
	/*!< Tells if Crosstalk compensation shall be enable or not  */
	uint16_t XTalkCompensationRangeMilliMeter;
	/*!< CrossTalk compensation range in millimeter  */
	FixPoint1616_t XTalkCompensationRateMegaCps;
	/*!< CrossTalk compensation rate in Mega counts per seconds.
		Expressed in 16.16 fixed point format.  */
	uint8_t SnrLimitCheckEnable[VL53L0_CHECKPOSITION_NO_OF_CHECKS];
	/*!< Tells if SNR limit Check shall be enable or not for each
		position. */
	uint8_t SignalLimitCheckEnable[VL53L0_CHECKPOSITION_NO_OF_CHECKS];
	/*!< Tells if Signal limit Check shall be enable or not for each
		position.*/
	FixPoint1616_t SignalLimitValue[VL53L0_CHECKPOSITION_NO_OF_CHECKS];
	/*!< Signal limit value for each position */
	uint8_t SigmaLimitCheckEnable[VL53L0_CHECKPOSITION_NO_OF_CHECKS];
	/*!< Tells if Sigma limit Check shall be enable or not for each
		position. */
	FixPoint1616_t SigmaLimitValue[VL53L0_CHECKPOSITION_NO_OF_CHECKS];
	/*!< Sigma limit value for each position */
	uint8_t WrapAroundCheckEnable;
	/*!< Tells if Wrap Around Check shall be enable or not */
} VL53L0_DeviceParameters_t;


/** @defgroup VL53L0_define_State_group Defines the current status of the device
 *  Defines the current status of the device
 *  @{
 */

typedef uint8_t VL53L0_State;

#define VL53L0_STATE_POWERDOWN       ((VL53L0_State)  0)
/*!< Device is in HW reset  */
#define VL53L0_STATE_WAIT_STATICINIT ((VL53L0_State)  1)
/*!< Device is initialized and wait for static initialization  */
#define VL53L0_STATE_STANDBY         ((VL53L0_State)  2)
/*!< Device is in Low power Standby mode   */
#define VL53L0_STATE_IDLE            ((VL53L0_State)  3)
/*!< Device has been initialized and ready to do measurements  */
#define VL53L0_STATE_RUNNING         ((VL53L0_State)  4)
/*!< Device is performing measurement */
#define VL53L0_STATE_UNKNOWN         ((VL53L0_State)  98)
/*!< Device is in unknown state and need to be rebooted  */
#define VL53L0_STATE_ERROR           ((VL53L0_State)  99)
/*!< Device is in error state and need to be rebooted  */

/** @} */ /* end of VL53L0_define_State_group */


/** @brief Structure containing the Dmax computation parameters and data
 */
typedef struct {
	int32_t AmbTuningWindowFactor_K;
	/*!<  internal algo tuning (*1000) */
	int32_t RetSignalAt0mm;
	/*!< intermediate dmax computation value caching */
} VL53L0_DMaxData_t;

/**
 * @struct VL53L0_RangeData_t
 * @brief Range measurement data.
 */
typedef struct {
	uint32_t TimeStamp;
	/*!< 32-bit time stamp. */
	uint32_t MeasurementTimeUsec;
	/*!< Give the Measurement time needed by the device to do the
		measurement.*/
	uint16_t RangeMilliMeter;
	/*!< range distance in millimeter. */
	uint16_t RangeDMaxMilliMeter;
	/*!< Tells what is the maximum detection distance of the device in
		current setup and environment conditions
		(Filled when applicable) */
	FixPoint1616_t SignalRateRtnMegaCps;
	/*!< Return signal rate (MCPS)\n these is a 16.16 fix point value,
		which is effectively a measure of target reflectance.*/
	FixPoint1616_t AmbientRateRtnMegaCps;
	/*!< Return ambient rate (MCPS)\n these is a 16.16 fix point value,
		which is effectively a measure of the ambient light.*/
	uint16_t EffectiveSpadRtnCount;
	/*!< Return the effective SPAD count for the return signal.
		To obtain Real value it should be divided by 32 */
	uint8_t ZoneId;
	/*!< Denotes which zone and range scheduler stage the range data
		relates	to. */
	uint8_t RangeFractionalPart;
	/*!< Fractional part of range distance. Final value is a FixPoint168
		value. */
	uint8_t RangeStatus;
	/*!< Range Status for the current measurement. This is device dependent.
		Value = 11 means value is valid. */
} VL53L0_RangingMeasurementData_t;


#define VL53L0_HISTOGRAM_BUFFER_SIZE 38

/**
 * @struct VL53L0_HistogramData_t
 * @brief Histogram measurement data.
 */
typedef struct {
	/* Histogram Measurement data */
	uint32_t HistogramData[VL53L0_HISTOGRAM_BUFFER_SIZE];
	/*!< Histogram data */
	uint8_t HistogramType;
	/*!< Indicate the types of histogram data : Return only, Reference only,
		both Return and Reference */
	uint8_t FirstBin;
	/*!< First Bin value */
	uint8_t BufferSize;
	/*!< Buffer Size */
	uint8_t NumberOfBins;
	/*!< Number of bins filled by the histogram measurement */
	VL53L0_DeviceError ErrorStatus;
	/*!< Error status of the current measurement.\n
	see @a ::VL53L0_DeviceError @a VL53L0_GetStatusErrorString() */
} VL53L0_HistogramMeasurementData_t;


/**
 * @struct VL53L0_DevData_t
 *
 * @brief VL53L0 PAL device ST private data structure \n
 * End user should never access any of these field directly
 *
 * These must never access directly but only via macro
 */
typedef struct {
	VL53L0_DMaxData_t DMaxData;
	/*!< Dmax Data */
	int16_t  Part2PartOffsetNVMMicroMeter;
	/*!< backed up NVM value */
	VL53L0_DeviceParameters_t CurrentParameters;
	/*!< Current Device Parameter */
	VL53L0_RangingMeasurementData_t LastRangeMeasure;
	/*!< Ranging Data */
	VL53L0_HistogramMeasurementData_t LastHistogramMeasure;
	/*!< Histogram Data */
	VL53L0_DeviceSpecificParameters_t DeviceSpecificParameters;
	/*!< Parameters specific to the device */
	uint8_t SequenceConfig;
	/*!< Internal value for the sequence config */
	VL53L0_State PalState;
	/*!< Current state of the PAL for this device */
	VL53L0_PowerModes PowerMode;
	/*!< Current Power Mode  */
	uint16_t SigmaEstRefArray;
	/*!< Reference array sigma value in 1/100th of [mm] e.g. 100 = 1mm */
	uint16_t SigmaEstEffPulseWidth;
	/*!< Effective Pulse width for sigma estimate in 1/100th of ns
		e.g. 900 = 9.0ns */
	uint16_t SigmaEstEffAmbWidth;
	/*!< Effective Ambient width for sigma estimate in 1/100th of ns
		e.g. 500 = 5.0ns */
	FixPoint1616_t SigmaEstimate;
	/*!< Sigma Estimate - based on ambient & VCSEL rates and
		signal_total_events */
	FixPoint1616_t SignalEstimate;
	/*!< Signal Estimate - based on ambient & VCSEL rates and cross talk */
#ifdef PAL_EXTENDED
	VL53L0_TuningDeviceParameters_t TuningDeviceParameters;
	/*!< Tuning Device Parameters specific to the device. */
#endif

} VL53L0_DevData_t;



/** @defgroup VL53L0_define_InterruptPolarity_group Defines the Polarity of the
 *  Interrupt
 *  Defines the Polarity of the Interrupt
 *  @{
 */
typedef uint8_t VL53L0_InterruptPolarity;

#define VL53L0_INTERRUPTPOLARITY_LOW       ((VL53L0_InterruptPolarity)  0)
/*!< Set active low polarity best setup for falling edge. */
#define VL53L0_INTERRUPTPOLARITY_HIGH      ((VL53L0_InterruptPolarity)  1)
/*!< Set active high polarity best setup for rising edge. */

/** @} */ /* end of VL53L0_define_InterruptPolarity_group */


#ifdef __cplusplus
}
#endif


#endif /* _VL53L0_DEF_H_ */
