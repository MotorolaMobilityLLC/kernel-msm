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

/**
 * @file vl53l1_def.h
 *
 * @brief Type definitions for VL53L1 API.
 *
 */


#ifndef _VL53L1_DEF_H_
#define _VL53L1_DEF_H_

#include "vl53l1_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup VL53L1_globaldefine_group VL53L1 Defines
 *  @brief    VL53L1 Defines
 *  @{
 */


/** PAL SPECIFICATION major version */
#define VL53L1_SPECIFICATION_VER_MAJOR   1
/** PAL SPECIFICATION minor version */
#define VL53L1_SPECIFICATION_VER_MINOR   2
/** PAL SPECIFICATION sub version */
#define VL53L1_SPECIFICATION_VER_SUB     7
/** PAL SPECIFICATION sub version */
#define VL53L1_SPECIFICATION_VER_REVISION 1440

/** VL53L1 IMPLEMENTATION major version */
#define VL53L1_IMPLEMENTATION_VER_MAJOR       2
/** VL53L1 IMPLEMENTATION minor version */
#define VL53L1_IMPLEMENTATION_VER_MINOR       3
/** VL53L1 IMPLEMENTATION sub version */
#define VL53L1_IMPLEMENTATION_VER_SUB         0
/** VL53L1 IMPLEMENTATION sub version */
#define VL53L1_IMPLEMENTATION_VER_REVISION  753


#include "vl53l1_types.h"
#include "vl53l1_register_structs.h"


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
} VL53L1_Version_t;

#include "vl53l1_ll_def.h"
#include "vl53l1_ll_device.h"


#define VL53L1_DEVINFO_STRLEN 32

/** @brief Defines the parameters of the Get Device Info Functions
 */
typedef struct {
	char Name[VL53L1_DEVINFO_STRLEN];
		/*!< Name of the Device e.g. Left_Distance */
	char Type[VL53L1_DEVINFO_STRLEN];
		/*!< Type of the Device e.g VL53L1 */
	char ProductId[VL53L1_DEVINFO_STRLEN];
		/*!< Product Identifier String  */
	uint8_t ProductType;
		/*!< Product Type, VL53L1 = 1, VL53L1 = 2 */
	uint8_t ProductRevisionMajor;
		/*!< Product revision major */
	uint8_t ProductRevisionMinor;
		/*!< Product revision minor */
} VL53L1_DeviceInfo_t;



/** @defgroup VL53L1_define_PresetModes_group Defines Device modes
 *  Defines all possible preset modes for the device
 *  @{
 */
typedef uint8_t VL53L1_PresetModes;

#define VL53L1_PRESETMODE_STANDARD_RANGING          ((VL53L1_PresetModes)  1)
#define VL53L1_PRESETMODE_MULTI_OBJECT              ((VL53L1_PresetModes)  2)
#define VL53L1_PRESETMODE_MULTI_ZONES               ((VL53L1_PresetModes)  3)
#define VL53L1_PRESETMODE_OLT                       ((VL53L1_PresetModes)  7)

	/* ... Modes to be added depending on device */
/** @} VL53L1_define_PresetModes_group */


/** @defgroup VL53L1_define_DeviceModes_group Defines Device modes
 *  Defines all possible modes for the device
 *  @{
 */
typedef uint8_t VL53L1_DeviceModes;

#define VL53L1_DEVICEMODE_SINGLE_RANGING           ((VL53L1_DeviceModes)  0)
#define VL53L1_DEVICEMODE_CONTINUOUS_RANGING       ((VL53L1_DeviceModes)  1)
#define VL53L1_DEVICEMODE_SINGLE_HISTOGRAM         ((VL53L1_DeviceModes)  2)
#define VL53L1_DEVICEMODE_CONTINUOUS_TIMED_RANGING ((VL53L1_DeviceModes)  3)
#define VL53L1_DEVICEMODE_SINGLE_ALS               ((VL53L1_DeviceModes) 10)
#define VL53L1_DEVICEMODE_GPIO_DRIVE               ((VL53L1_DeviceModes) 20)
#define VL53L1_DEVICEMODE_GPIO_OSC                 ((VL53L1_DeviceModes) 21)
/** @} VL53L1_define_DeviceModes_group */

/** @defgroup VL53L1_CheckEnable_group Check Enable list
 *  @brief Check Enable code
 *
 *  Define used to specify the LimitCheckId.
 *  Use @a VL53L1_GetLimitCheckInfo() to get the string.
 *  @{
 */

#define VL53L1_CHECKENABLE_SIGMA_FINAL_RANGE           0
#define VL53L1_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE     1

#define VL53L1_CHECKENABLE_NUMBER_OF_CHECKS            2

/** @}  end of VL53L1_CheckEnable_group */


/** @defgroup VL53L1_define_HistogramModes_group Defines Histogram modes
 *  Defines all possible Histogram modes for the device
 *  @{
 */
typedef uint8_t VL53L1_HistogramModes;

#define VL53L1_HISTOGRAMMODE_DISABLED        ((VL53L1_HistogramModes) 0)
	/*!< Histogram Disabled */
#define VL53L1_HISTOGRAMMODE_REFERENCE_ONLY  ((VL53L1_HistogramModes) 1)
	/*!< Histogram Reference array only */
#define VL53L1_HISTOGRAMMODE_RETURN_ONLY     ((VL53L1_HistogramModes) 2)
	/*!< Histogram Return array only */
#define VL53L1_HISTOGRAMMODE_BOTH            ((VL53L1_HistogramModes) 3)
	/*!< Histogram both Reference and Return Arrays */
	/* ... Modes to be added depending on device */
/** @} VL53L1_define_HistogramModes_group */


/** @defgroup VL53L1_define_PowerModes_group List of available Power Modes
 *  List of available Power Modes
 *  @{
 */

typedef uint8_t VL53L1_PowerModes;

#define VL53L1_POWERMODE_STANDBY_LEVEL1 ((VL53L1_PowerModes) 0)
	/*!< Standby level 1 */
#define VL53L1_POWERMODE_STANDBY_LEVEL2 ((VL53L1_PowerModes) 1)
	/*!< Standby level 2 */
#define VL53L1_POWERMODE_IDLE_LEVEL1    ((VL53L1_PowerModes) 2)
	/*!< Idle level 1 */
#define VL53L1_POWERMODE_IDLE_LEVEL2    ((VL53L1_PowerModes) 3)
	/*!< Idle level 2 */

/** @} VL53L1_define_PowerModes_group */


/** @brief Defines all parameters for the device
 */
typedef struct {
	VL53L1_PresetModes PresetMode;
	/*!< Defines the operating mode to be used for the next measure */
	VL53L1_DeviceModes DeviceMode;
	/*!< Defines type of measurement to be done for the next measure */
	VL53L1_HistogramModes HistogramMode;
	/*!< Defines type of histogram measurement to be done for the next
	 *  measure */
	VL53L1_PowerModes PowerMode; /* Store the power mode */
	uint32_t MeasurementTimingBudgetMicroSeconds;
	/*!< Defines the allowed total time for a single measurement */
	uint32_t InterMeasurementPeriodMilliSeconds;
	/*!< Defines time between two consecutive measurements (between two
	 *  measurement starts). If set to 0 means back-to-back mode */
	uint8_t XTalkCompensationEnable;
	/*!< Tells if Crosstalk compensation shall be enable or not  */
	uint16_t XTalkCompensationRangeMilliMeter;
	/*!< CrossTalk compensation range in millimeter  */
	FixPoint1616_t XTalkCompensationRateMegaCps;
	/*!< CrossTalk compensation rate in Mega counts per seconds.
	 *  Expressed in 16.16 fixed point format.  */
	int32_t RangeOffsetMicroMeters;
	/*!< Range offset adjustment (mm).  */

	uint8_t LimitChecksEnable[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Limit Check enable for this device. */
	uint8_t LimitChecksStatus[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Status of the check linked to last
	* measurement. */
	FixPoint1616_t LimitChecksValue[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Limit Check value for this device */
	FixPoint1616_t LimitChecksCurrent[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Limit Check current value from latest
	 * ranging */

	uint8_t WrapAroundCheckEnable;
	/*!< Tells if Wrap Around Check shall be enable or not */
} VL53L1_DeviceParameters_t;


/** @defgroup VL53L1_define_State_group Defines the current status of the device
 *  Defines the current status of the device
 *  @{
 */

typedef uint8_t VL53L1_State;

#define VL53L1_STATE_POWERDOWN       ((VL53L1_State)  0)
	/*!< Device is in HW reset  */
#define VL53L1_STATE_WAIT_STATICINIT ((VL53L1_State)  1)
	/*!< Device is initialized and wait for static initialization  */
#define VL53L1_STATE_STANDBY         ((VL53L1_State)  2)
	/*!< Device is in Low power Standby mode   */
#define VL53L1_STATE_IDLE            ((VL53L1_State)  3)
	/*!< Device has been initialized and ready to do measurements  */
#define VL53L1_STATE_RUNNING         ((VL53L1_State)  4)
	/*!< Device is performing measurement */
#define VL53L1_STATE_RESET           ((VL53L1_State)  5)
	/*!< Soft reset has been run on Device */
#define VL53L1_STATE_UNKNOWN         ((VL53L1_State)  98)
	/*!< Device is in unknown state and need to be rebooted  */
#define VL53L1_STATE_ERROR           ((VL53L1_State)  99)
	/*!< Device is in error state and need to be rebooted  */

/** @} VL53L1_define_State_group */




/**
 * @struct VL53L1_SingleRangingData_t
 * @brief Single Range measurement data.
 */
typedef struct {
	uint32_t TimeStamp;               /*!< 32-bit time stamp. */

	uint8_t StreamCount;            /*!< 8-bit Stream Count. */

	uint8_t ConfidenceLevel;
		/*!< indicate a confidance level in percentage from 0 to 100 */

	uint16_t DmaxMilliMeter;
		/*!< range Dmax distance in millimeter. */

	uint16_t RangeMaxMilliMeter;
		/*!< Tells what is the maximum detection distance of the device
		 * in current setup and environment conditions (Filled when
		 *  applicable) */

	uint16_t RangeMinMilliMeter;
		/*!< Tells what is the minimum detection distance of the device
		 * in current setup and environment conditions (Filled when
		 *  applicable) */

	FixPoint1616_t SignalRateRtnMegaCps;
		/*!< Return signal rate (MCPS)\n these is a 16.16 fix point
		 *  value, which is effectively a measure of target
		 *   reflectance.*/

	FixPoint1616_t AmbientRateRtnMegaCps;
		/*!< Return ambient rate (MCPS)\n these is a 16.16 fix point
		 *  value, which is effectively a measure of the ambien
		 *  t light.*/

	uint16_t EffectiveSpadRtnCount;
		/*!< Return the effective SPAD count for the return signal.
		 *  To obtain Real value it should be divided by 256 */

	FixPoint1616_t SigmaMilliMeter;
		/*!< Return the Sigma value in millimeter */

	uint16_t RangeMilliMeter;         /*!< range distance in millimeter. */

	uint8_t RangeFractionalPart;
		/*!< Fractional part of range distance. Final value is a
		 *  RangeMilliMeter + RangeFractionalPart/256. */

	uint8_t RangeStatus;
		/*!< Range Status for the current measurement. This is device
		 *  dependent. Value = 0 means value is valid.
		 */
} VL53L1_RangingMeasurementData_t;

/**
 * @struct  VL53L1_ROIRangeResults_t
 * @brief   Structure for storing the set of range results for a single ROI
 *
 */

typedef struct {

	uint8_t MaxResults;
		/*!< Max number of results requested for this ROI */
	uint8_t RoiNumber;
		/*!< Denotes on which ROI the range data is related to. */
	uint8_t NumberOfObjectsFound;
		/*!< Indicate the number of objects found in the current ROI.
		* This is used to know how many ranging data should be get.
		* Value 0 means this ranging data has not been updated yet or
		* it doesn't contains valid data.
		* Valid value are from 1 to VL53L1_MAX_NUMBER_OF_DATA_PER_ROI.
		*/
	uint16_t DmaxMilliMeter;
		/*!< range Dmax distance in millimeter. */

	VL53L1_RangingMeasurementData_t RangeData[VL53L1_MAX_RANGE_RESULTS];
		/*!< Range data each target distance */

} VL53L1_ROIRangeResults_t;

/**
 * @struct  VL53L1_range_results_t
 * @brief   Structure for storing the set of range results generated
 *          for all ROIs
 *
 */

typedef struct {

	uint8_t                MaxROIs;
		/*!< Max number of ROI requested from user */
	uint8_t                ActiveROIs;
		/*!< Number of ROI active */
	VL53L1_ROIRangeResults_t ROIRangeData[VL53L1_MAX_USER_ZONES];
		/*!< All ROI data  */

} VL53L1_MultiRangingData_t;



#define VL53L1_HISTOGRAM_BUFFER_SIZE 24

/**
 * @struct VL53L1_HistogramData_t
 * @brief Histogram measurement data.
 */
typedef struct {
	/* Histogram Measurement data */
	uint32_t HistogramData[VL53L1_HISTOGRAM_BUFFER_SIZE];
	/*!< Histogram data */
	uint8_t HistogramType; /*!< Indicate the types of histogram data :
	Return only, Reference only, both Return and Reference */
	uint8_t FirstBin; /*!< First Bin value */
	uint8_t BufferSize; /*!< Buffer Size - Set by the user.*/
	uint8_t NumberOfBins;
	/*!< Number of bins filled by the histogram measurement */

} VL53L1_HistogramMeasurementData_t;


/** @brief Defines User Zone(ROI) parameters
 *
 */

typedef struct {

	uint8_t   TopLeftX;   /*!< Top Left x coordinate:  0-15 range */
	uint8_t   TopLeftY;   /*!< Top Left y coordinate:  0-15 range */
	uint8_t   BotRightX;  /*!< Bot Right x coordinate: 0-15 range */
	uint8_t   BotRightY;  /*!< Bot Right x coordinate:0-15 range  */

} VL53L1_UserRoi_t;


/** @brief Defines ROI configuration parameters
 *
 *  Support up a max of 16 zones, Each Zone has the same size
 *
 */

typedef struct {

	uint8_t             NumberOfRoi;   /*!< Number of Rois defined*/

	uint8_t             FirstRoiToScan;
		/*!< First Roi to start the scan */

	VL53L1_UserRoi_t    UserRois[VL53L1_MAX_USER_ZONES];
		/*!< List of Rois */

} VL53L1_RoiConfig_t;



/** @defgroup VL53L1_define_InterruptPolarity_group Defines the Polarity
 * of the Interrupt
 *  Defines the Polarity of the Interrupt
 *  @{
 */
typedef uint8_t VL53L1_InterruptPolarity;

#define VL53L1_INTERRUPTPOLARITY_LOW       ((VL53L1_InterruptPolarity)  0)
/*!< Set active low polarity best setup for falling edge. */
#define VL53L1_INTERRUPTPOLARITY_HIGH      ((VL53L1_InterruptPolarity)  1)
/*!< Set active high polarity best setup for rising edge. */

/** @} VL53L1_define_InterruptPolarity_group */


/** @defgroup VL53L1_define_SequenceStepId_group Defines the Polarity
 *	of the Interrupt
 *	Defines the the sequence steps performed during ranging..
 *	@{
 */
typedef uint8_t VL53L1_SequenceStepId;

#define	 VL53L1_SEQUENCESTEP_VHV		 ((VL53L1_SequenceStepId) 0)
/*!<VHV. */
#define	 VL53L1_SEQUENCESTEP_PHASECAL		 ((VL53L1_SequenceStepId) 1)
/*!<Phase Calibration. */
#define	 VL53L1_SEQUENCESTEP_REFPHASE		 ((VL53L1_SequenceStepId) 2)
/*!<Reference Phase. */
#define	 VL53L1_SEQUENCESTEP_DSS1		 ((VL53L1_SequenceStepId) 3)
/*!<DSS1. */
#define	 VL53L1_SEQUENCESTEP_DSS2		 ((VL53L1_SequenceStepId) 4)
/*!<DSS2. */
#define	 VL53L1_SEQUENCESTEP_MM1		 ((VL53L1_SequenceStepId) 5)
/*!<Mode Mitigation 1. */
#define	 VL53L1_SEQUENCESTEP_MM2		 ((VL53L1_SequenceStepId) 6)
/*!<Mode Mitigation 2. */
#define	 VL53L1_SEQUENCESTEP_RANGE		 ((VL53L1_SequenceStepId) 7)
/*!<Final Range step. */

#define	 VL53L1_SEQUENCESTEP_NUMBER_OF_ITEMS			 8
/*!<Number of Sequence Step Managed by the API. */

/** @} VL53L1_define_SequenceStepId_group */

/** @defgroup VL53L1_define_RangeStatus_group Defines the Range Status
 *	@{
 */
#define	 VL53L1_RANGESTATUS_RANGE_VALID		0
/*!<The Range is valid. */
#define	 VL53L1_RANGESTATUS_SIGMA_FAIL		1
/*!<Sigma Fail. */
#define	 VL53L1_RANGESTATUS_SIGNAL_FAIL		2
/*!<Signal fail. */
#define	 VL53L1_RANGESTATUS_MIN_RANGE_FAIL	3
/*!<Min Range fail. */
#define	 VL53L1_RANGESTATUS_PHASE_FAIL		4
/*!<Phase fail. */
#define	 VL53L1_RANGESTATUS_HARDWARE_FAIL	5
/*!<Hardware fail. */
#define	 VL53L1_RANGESTATUS_NONE		255
/*!<No Update. */

/** @} VL53L1_define_RangeStatus_group */



/** @defgroup VL53L1_GpioFunctionality_group Gpio Functionality
 *  @brief Defines the different functionalities for the device GPIO(s)
 *  @{
 */
typedef uint8_t VL53L1_GpioFunctionality;

#define VL53L1_GPIOFUNCTIONALITY_OFF                     \
	((VL53L1_GpioFunctionality)  0) /*!< NO Interrupt  */
#define VL53L1_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_LOW   \
	((VL53L1_GpioFunctionality)  1) /*!< Level Low (value < thresh_low)  */
#define VL53L1_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_HIGH   \
	((VL53L1_GpioFunctionality)  2) /*!< Level High (value > thresh_high) */
#define VL53L1_GPIOFUNCTIONALITY_THRESHOLD_CROSSED_OUT    \
	((VL53L1_GpioFunctionality)  3)
	/*!< Out Of Window (value < thresh_low OR value > thresh_high)  */
#define VL53L1_GPIOFUNCTIONALITY_NEW_MEASURE_READY        \
	((VL53L1_GpioFunctionality)  4) /*!< New Sample Ready  */

/** @} end of VL53L1_GpioFunctionality_group */


/** @brief  Contains the Internal data of the Bare Driver
 */

typedef struct {
	VL53L1_LLDriverData_t   LLData;
	/*!< Low Level Driver data structure */

	VL53L1_State      PalState; /* Store the pal state */
	VL53L1_DeviceParameters_t CurrentParameters;
	/*!< Current Device Parameter */

} VL53L1_DevData_t;


#define VL53L1DevStructGetLLDriverHandle(Dev) (&PALDevDataGet(Dev, LLData))

/* MACRO Definitions */
/** @defgroup VL53L1_define_GeneralMacro_group General Macro Defines
 *  General Macro Defines
 *  @{
 */

/* Defines */
#define VL53L1_SETPARAMETERFIELD(Dev, field, value) \
	PALDevDataSet(Dev, CurrentParameters.field, value)

#define VL53L1_GETPARAMETERFIELD(Dev, field, variable) \
	variable = PALDevDataGet(Dev, CurrentParameters).field


#define VL53L1_SETARRAYPARAMETERFIELD(Dev, field, index, value) \
	PALDevDataSet(Dev, CurrentParameters.field[index], value)

#define VL53L1_GETARRAYPARAMETERFIELD(Dev, field, index, variable) \
	variable = PALDevDataGet(Dev, CurrentParameters).field[index]


#define VL53L1_SETDEVICESPECIFICPARAMETER(Dev, field, value) \
		PALDevDataSet(Dev, DeviceSpecificParameters.field, value)

#define VL53L1_GETDEVICESPECIFICPARAMETER(Dev, field) \
		PALDevDataGet(Dev, DeviceSpecificParameters).field


#define VL53L1_FIXPOINT1616TOFIXPOINT97(Value) \
	(uint16_t)((Value>>9)&0xFFFF)
#define VL53L1_FIXPOINT97TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<9)

#define VL53L1_FIXPOINT1616TOFIXPOINT88(Value) \
	(uint16_t)((Value>>8)&0xFFFF)
#define VL53L1_FIXPOINT88TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<8)

#define VL53L1_FIXPOINT1616TOFIXPOINT412(Value) \
	(uint16_t)((Value>>4)&0xFFFF)
#define VL53L1_FIXPOINT412TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<4)

#define VL53L1_FIXPOINT1616TOFIXPOINT313(Value) \
	(uint16_t)((Value>>3)&0xFFFF)
#define VL53L1_FIXPOINT313TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<3)

#define VL53L1_FIXPOINT1616TOFIXPOINT08(Value) \
	(uint8_t)((Value>>8)&0x00FF)
#define VL53L1_FIXPOINT08TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<8)

#define VL53L1_FIXPOINT1616TOFIXPOINT53(Value) \
	(uint8_t)((Value>>13)&0x00FF)
#define VL53L1_FIXPOINT53TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<13)

#define VL53L1_FIXPOINT1616TOFIXPOINT102(Value) \
	(uint16_t)((Value>>14)&0x0FFF)
#define VL53L1_FIXPOINT102TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<14)

#define VL53L1_FIXPOINT1616TOFIXPOINT142(Value) \
	(uint16_t)((Value>>14)&0xFFFF)
#define VL53L1_FIXPOINT142TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<14)

#define VL53L1_FIXPOINT1616TOFIXPOINT160(Value) \
	(uint16_t)((Value>>16)&0xFFFF)
#define VL53L1_FIXPOINT160TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<16)

#define VL53L1_MAKEUINT16(lsb, msb) (uint16_t)((((uint16_t)msb)<<8) + \
		(uint16_t)lsb)

#define SUPPRESS_UNUSED_WARNING(x) (void) (x)

/** @} VL53L1_define_GeneralMacro_group */


/** @} VL53L1_globaldefine_group */



#ifdef __cplusplus
}
#endif


#endif /* _VL53L1_DEF_H_ */
