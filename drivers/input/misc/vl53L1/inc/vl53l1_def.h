/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either
* 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0044
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, VL53L1 Core may be distributed under the terms of
* 'BSD 3-clause "New" or "Revised" License', in which case the following
* provisions apply instead of the ones
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

#include "vl53l1_ll_def.h"

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
#define VL53L1_IMPLEMENTATION_VER_MAJOR       4
/** VL53L1 IMPLEMENTATION minor version */
#define VL53L1_IMPLEMENTATION_VER_MINOR       1
/** VL53L1 IMPLEMENTATION sub version */
#define VL53L1_IMPLEMENTATION_VER_SUB         0
/** VL53L1 IMPLEMENTATION sub version */
#define VL53L1_IMPLEMENTATION_VER_REVISION  1249


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


#define VL53L1_DEVINFO_STRLEN 32

/** @brief Defines the parameters of the Get Device Info Functions
 */
typedef struct {
	char Name[VL53L1_DEVINFO_STRLEN];
		/*!< Name of the Device e.g. Left_Distance */
	char Type[VL53L1_DEVINFO_STRLEN];
		/*!< Type of the Device e.g VL53L1 */
	char ProductId[VL53L1_DEVINFO_STRLEN];
		/*!< Product Identifier String
		 * @warning Not yet implemented
		 */
	uint8_t ProductType;
		/*!< Product Type, VL53L1 = 1, VL53L1 = 2*/
	uint8_t ProductRevisionMajor;
		/*!< Product revision major */
	uint8_t ProductRevisionMinor;
		/*!< Product revision minor */
} VL53L1_DeviceInfo_t;



/** @defgroup VL53L1_define_PresetModes_group Defines Preset modes
 *  Defines all possible preset modes for the device
 *  @{
 */
typedef uint8_t VL53L1_PresetModes;

#define VL53L1_PRESETMODE_RANGING                   ((VL53L1_PresetModes)  1)
#define VL53L1_PRESETMODE_MULTIZONES_SCANNING       ((VL53L1_PresetModes)  2)
#define VL53L1_PRESETMODE_AUTONOMOUS                ((VL53L1_PresetModes)  3)
#define VL53L1_PRESETMODE_LITE_RANGING              ((VL53L1_PresetModes)  4)
#define VL53L1_PRESETMODE_OLT                       ((VL53L1_PresetModes)  7)

	/* ... Modes to be added depending on device */
/** @} VL53L1_define_PresetModes_group */


/** @defgroup VL53L1_define_DistanceModes_group Defines Distance modes
 *  Defines all possible Distance modes for the device
 *  @{
 */
typedef uint8_t VL53L1_DistanceModes;

#define VL53L1_DISTANCEMODE_SHORT             ((VL53L1_DistanceModes)  1)
#define VL53L1_DISTANCEMODE_MEDIUM            ((VL53L1_DistanceModes)  2)
#define VL53L1_DISTANCEMODE_LONG              ((VL53L1_DistanceModes)  3)
#define VL53L1_DISTANCEMODE_AUTO_LITE         ((VL53L1_DistanceModes)  4)
#define VL53L1_DISTANCEMODE_AUTO              ((VL53L1_DistanceModes)  5)

	/* ... Modes to be added depending on device */
/** @} VL53L1_define_DistanceModes_group */

/** @defgroup VL53L1_define_OutputModes_group Defines Output modes
 *  Defines all possible Output modes for the device
 *  @{
 */
typedef uint8_t VL53L1_OutputModes;

#define VL53L1_OUTPUTMODE_NEAREST          ((VL53L1_OutputModes)  1)
#define VL53L1_OUTPUTMODE_STRONGEST        ((VL53L1_OutputModes)  2)

/** @} VL53L1_define_OutputModes_group */

/** @defgroup VL53L1_define_OffsetCalibrationModes_group Defines Offset Calibration modes
*  Defines all possible Offset Calibration modes for the device
*  @{
*/
typedef uint8_t VL53L1_OffsetCalibrationModes;

#define VL53L1_OFFSETCALIBRATIONMODE_STANDARD \
	((VL53L1_OffsetCalibrationModes)  1)
#define VL53L1_OFFSETCALIBRATIONMODE_PRERANGE_ONLY  \
	((VL53L1_OffsetCalibrationModes)  2)
#define VL53L1_OFFSETCALIBRATIONMODE_MULTI_ZONE    \
	((VL53L1_OffsetCalibrationModes)  3)

/** @} VL53L1_define_OffsetCalibrationModes_group */

/** @defgroup VL53L1_define_DeviceDmaxModes_group Defines Dmax source modes
*  Defines all possible sources for Dmax calibration for the device
*  @{
*/
typedef uint8_t VL53L1_DeviceDmaxModes;

#define DMAXMODE_FMT_CAL_DATA      ((VL53L1_DeviceDmaxModes)  1)
#define DMAXMODE_CUSTCAL_DATA      ((VL53L1_DeviceDmaxModes)  2)
#define DMAXMODE_PER_ZONE_CAL_DATA ((VL53L1_DeviceDmaxModes)  3)

/** @} VL53L1_define_DeviceDmaxModes_group */

/** @defgroup VL53L1_define_OffsetCalibrationModesBD_group
 *  Device Offset Correction Mode
 *
 *  @brief Defines all possible offset correction modes for the device
 *  @{
 */
typedef uint8_t VL53L1_OffsetCorrectionModes;

#define VL53L1_OFFSETCORRECTIONMODE_STANDARD ((VL53L1_OffsetCorrectionMode)  1)
#define VL53L1_OFFSETCORRECTIONMODE_PERZONE  ((VL53L1_OffsetCorrectionMode)  2)

/** @} VL53L1_define_OffsetCalibrationModesBD_group */


/** @defgroup VL53L1_define_RoiStatus_group Defines Roi Status
 *  Defines the read status mode
 *  @{
 */
typedef uint8_t VL53L1_RoiStatus;

#define VL53L1_ROISTATUS_NOT_VALID                 ((VL53L1_RoiStatus)  0)
#define VL53L1_ROISTATUS_VALID_NOT_LAST            ((VL53L1_RoiStatus)  1)
#define VL53L1_ROISTATUS_VALID_LAST                ((VL53L1_RoiStatus)  2)
/** @} VL53L1_define_RoiStatus_group */


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

/** @defgroup VL53L1_ThresholdMode_gropup Detection Functionality
 *  @brief Defines the different functionalities for the detection feature
 *  @{
 */
typedef uint8_t VL53L1_ThresholdMode;

#define VL53L1_THRESHOLD_CROSSED_LOW   \
	((VL53L1_ThresholdMode)  0)
	/*!< Trigger interrupt if value < thresh_low */
#define VL53L1_THRESHOLD_CROSSED_HIGH   \
	((VL53L1_ThresholdMode)  1)
	/*!< Trigger interrupt if value > thresh_high */
#define VL53L1_THRESHOLD_OUT_OF_WINDOW    \
	((VL53L1_ThresholdMode)  2)
	/*!< Trigger interrupt if value < thresh_low OR value > thresh_high */
#define VL53L1_THRESHOLD_IN_WINDOW        \
	((VL53L1_ThresholdMode)  3)
	/*!< Trigger interrupt if value > thresh_low AND value < thresh_high */

/** @} end of VL53L1_ThresholdMode_gropup */

/** @brief Defines parameters for Distance detection Thresholds configuration
 */
typedef struct {
	VL53L1_ThresholdMode CrossMode; /*!< See #VL53L1_GpioThreshold */
	uint16_t High; /*!< Distance threshold high limit in mm */
	uint16_t Low;  /*!< Distance threshold low limit  in mm */
} VL53L1_DistanceThreshold_t;

/** @brief Defines parameters for Signal rate detection Thresholds configuration
 */
typedef struct {
	VL53L1_ThresholdMode CrossMode; /*!< See #VL53L1_GpioThreshold */
	FixPoint1616_t High; /*!< Signal rate threshold high limit */
	FixPoint1616_t Low;  /*!< Signal rate threshold low limit */
} VL53L1_RateThreshold_t;

/** @defgroup VL53L1_DetectionMode_group Gpio Functionality
 *  @brief Defines conditions leading to device's IT on GPIO
 *  @{
 */
typedef uint8_t VL53L1_DetectionMode;

#define VL53L1_DETECTION_NORMAL_RUN   \
	((VL53L1_DetectionMode)  0)
	/*!< Trigger interrupt on new measurement regardless of threshold
	 * just like after a VL53L1_SetPresetMode() call
	 */
#define VL53L1_DETECTION_DISTANCE_ONLY   \
	((VL53L1_DetectionMode)  1)
	/*!< Trigger interrupt if "threshold event" occurs on distance */
#define VL53L1_DETECTION_RATE_ONLY   \
	((VL53L1_DetectionMode)  2)
	/*!< Trigger interrupt if "threshold event" occurs on signal rate */
#define VL53L1_DETECTION_DISTANCE_AND_RATE   \
	((VL53L1_DetectionMode)  3)
	/*!< Trigger interrupt if "threshold event" occurs on distance AND rate
	 */
#define VL53L1_DETECTION_DISTANCE_OR_RATE   \
	((VL53L1_DetectionMode)  4)
	/*!< Trigger interrupt if "threshold event" occurs on distance OR rate
	*/

/** @} end of VL53L1_DetectionMode_group */

/** @brief Defines parameters for User/object Detection configuration
 */
typedef struct {
	VL53L1_DetectionMode DetectionMode;
		/*!< See #VL53L1_GPIODetectionMode*/
	uint8_t IntrNoTarget; /*!< 1 to trigger IT in case of no target found */
	VL53L1_DistanceThreshold_t Distance; /*!< limits in mm */
	VL53L1_RateThreshold_t Rate;/*!< limits in FixPoint1616_t */
} VL53L1_DetectionConfig_t;


/** @brief Defines all parameters for the device
 */
typedef struct {
	VL53L1_PresetModes PresetMode;
	/*!< Defines the operating mode to be used for the next measure */
	VL53L1_OutputModes OutputMode;
	/*!< Defines the Output mode to be used for the next measure */
	VL53L1_DistanceModes DistanceMode;
	/*!< Defines the operating mode to be used for the next measure */
	VL53L1_DistanceModes InternalDistanceMode;
	/*!< Defines the internal operating mode to be used for the next
	 * measure
	 */
	VL53L1_DistanceModes NewDistanceMode;
	/*!< Defines the new operating mode to be programmed for the next
	 * measure
	 */
	uint32_t MeasurementTimingBudgetMicroSeconds;
	/*!< Defines the allowed total time for a single measurement */
	uint8_t LimitChecksEnable[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array store all the Limit Check enable for this device. */
	uint8_t LimitChecksStatus[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array stores all the Status of the check linked to last
	* measurement.
	*/
	FixPoint1616_t LimitChecksValue[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array stores all the Limit Check value for this device */
	FixPoint1616_t LimitChecksCurrent[VL53L1_CHECKENABLE_NUMBER_OF_CHECKS];
	/*!< This Array stores all the Limit Check current value from latest
	 * ranging
	 */
	uint8_t AmbientDmaxIndex;
	/*!< This value stores the index of the ambient Dmax to take from
	 * ambient_dmax_mm[] table to populate ranging results
	 */
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
	uint32_t TimeStamp;
	/*!< 32-bit time stamp.
	 * @warning Not yet implemented
	 */

	uint8_t StreamCount;            /*!< 8-bit Stream Count. */

	uint8_t RangeQualityLevel;
		/*!< indicate a quality level in percentage from 0 to 100
		 * @warning Not yet implemented
		 */

	int16_t DmaxMilliMeter;
		/*!< range Dmax distance in millimeter.
		 */

	int16_t RangeMaxMilliMeter;
		/*!< Tells what is the maximum detection distance of the object
		 * in current setup and environment conditions (Filled when
		 *  applicable)
		 */

	int16_t RangeMinMilliMeter;
		/*!< Tells what is the minimum detection distance of the object
		 * in current setup and environment conditions (Filled when
		 *  applicable)
		 */

	FixPoint1616_t SignalRateRtnMegaCps;
		/*!< Return signal rate (MCPS)\n these is a 16.16 fix point
		 *  value, which is effectively a measure of target
		 *   reflectance.
		 */

	FixPoint1616_t AmbientRateRtnMegaCps;
		/*!< Return ambient rate (MCPS)\n these is a 16.16 fix point
		 *  value, which is effectively a measure of the ambien
		 *  t light.
		 */

	uint16_t EffectiveSpadRtnCount;
		/*!< Return the effective SPAD count for the return signal.
		 *  To obtain Real value it should be divided by 256
		 */

	FixPoint1616_t SigmaMilliMeter;
		/*!< Return the Sigma value in millimeter */

	int16_t RangeMilliMeter;
		/*!< range distance in millimeter. This should be between
		 *  RangeMinMilliMeter and RangeMaxMilliMeter */

	uint8_t RangeFractionalPart;
		/*!< Fractional part of range distance. Final value is a
		 *  RangeMilliMeter + RangeFractionalPart/256.
		 *  @warning Not yet implemented
		 */

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

	uint8_t RoiNumber;
		/*!< Denotes on which ROI the range data is related to. */
	uint8_t NumberOfObjectsFound;
		/*!< Indicate the number of objects found in the current ROI.
		* This is used to know how many ranging data should be get.
		* NumberOfObjectsFound is in the range 0 to
		* VL53L1_MAX_RANGE_RESULTS.
		*/
	VL53L1_RoiStatus RoiStatus;
		/*!< Indicate if the data read is valid or not or if this is
		 * the last valid data in the ROI.
		*/
	VL53L1_RangingMeasurementData_t RangeData[VL53L1_MAX_RANGE_RESULTS];
		/*!< Range data each target distance */

} VL53L1_MultiRangingData_t;


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

	VL53L1_UserRoi_t    UserRois[VL53L1_MAX_USER_ZONES];
		/*!< List of Rois */

} VL53L1_RoiConfig_t;



/**
 * @struct  VL53L1_CalibrationData_t
 * @brief   Structure for storing the Calibration Data
 *
 */
typedef VL53L1_calibration_data_t VL53L1_CalibrationData_t;


/** @defgroup VL53L1_define_SequenceStepId_group Defines the SequenceStep
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
#define	 VL53L1_RANGESTATUS_RANGE_VALID_NO_WRAP_CHECK 6
/*!<The Range is valid but the wraparound check has not been done. */
#define	 VL53L1_RANGESTATUS_NONE		255
/*!<No Update. */

/** @} VL53L1_define_RangeStatus_group */


/** @brief  Contains the Internal data of the Bare Driver
 */

typedef struct {
	VL53L1_LLDriverData_t   LLData;
	/*!< Low Level Driver data structure */

	VL53L1_LLDriverResults_t llresults;
	/*!< Low Level Driver data structure */

	VL53L1_State      PalState; /* Store the pal state */
	VL53L1_DeviceParameters_t CurrentParameters;
	/*!< Current Device Parameter */

} VL53L1_DevData_t;


/* MACRO Definitions */
/** @defgroup VL53L1_define_GeneralMacro_group General Macro Defines
 *  General Macro Defines
 *  @{
 */

/* Defines */
#define VL53L1_SETPARAMETERFIELD(Dev, field, value) \
	(PALDevDataSet(Dev, CurrentParameters.field, value))

#define VL53L1_GETPARAMETERFIELD(Dev, field, variable) \
	(variable = PALDevDataGet(Dev, CurrentParameters).field)


#define VL53L1_SETARRAYPARAMETERFIELD(Dev, field, index, value) \
	(PALDevDataSet(Dev, CurrentParameters.field[index], value))

#define VL53L1_GETARRAYPARAMETERFIELD(Dev, field, index, variable) \
	(variable = PALDevDataGet(Dev, CurrentParameters).field[index])


#define VL53L1_SETDEVICESPECIFICPARAMETER(Dev, field, value) \
	(PALDevDataSet(Dev, DeviceSpecificParameters.field, value))

#define VL53L1_GETDEVICESPECIFICPARAMETER(Dev, field) \
	(PALDevDataGet(Dev, DeviceSpecificParameters).field)


#define VL53L1_FIXPOINT1616TOFIXPOINT72(Value) \
	(uint16_t)((Value>>2)&0xFFFF)
#define VL53L1_FIXPOINT72TOFIXPOINT1616(Value) \
	(FixPoint1616_t)(Value<<2)

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

#ifndef SUPPRESS_UNUSED_WARNING
#define SUPPRESS_UNUSED_WARNING(x) ((void) (x))
#endif

/** @} VL53L1_define_GeneralMacro_group */


/** @} VL53L1_globaldefine_group */



#ifdef __cplusplus
}
#endif


#endif /* _VL53L1_DEF_H_ */
