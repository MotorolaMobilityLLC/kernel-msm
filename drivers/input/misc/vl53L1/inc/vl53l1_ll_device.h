
/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* This file is part of VL53L1 Core and is dual licensed, either 'STMicroelectronics
* Proprietary license'
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





































#ifndef _VL53L1_LL_DEVICE_H_
#define _VL53L1_LL_DEVICE_H_

#include "vl53l1_types.h"

#define   VL53L1_I2C                      0x01
#define   VL53L1_SPI                      0x00













typedef uint8_t VL53L1_WaitMethod;

#define VL53L1_WAIT_METHOD_BLOCKING               ((VL53L1_WaitMethod)  0)
#define VL53L1_WAIT_METHOD_NON_BLOCKING           ((VL53L1_WaitMethod)  1)










typedef uint8_t VL53L1_DeviceState;

#define VL53L1_DEVICESTATE_POWERDOWN              ((VL53L1_DeviceState)  0)
#define VL53L1_DEVICESTATE_HW_STANDBY             ((VL53L1_DeviceState)  1)
#define VL53L1_DEVICESTATE_FW_COLDBOOT            ((VL53L1_DeviceState)  2)
#define VL53L1_DEVICESTATE_SW_STANDBY             ((VL53L1_DeviceState)  3)
#define VL53L1_DEVICESTATE_RANGING_DSS_AUTO       ((VL53L1_DeviceState)  4)
#define VL53L1_DEVICESTATE_RANGING_DSS_MANUAL     ((VL53L1_DeviceState)  5)
#define VL53L1_DEVICESTATE_RANGING_WAIT_GPH_SYNC  ((VL53L1_DeviceState)  6)
#define VL53L1_DEVICESTATE_RANGING_GATHER_DATA    ((VL53L1_DeviceState)  7)
#define VL53L1_DEVICESTATE_RANGING_OUTPUT_DATA    ((VL53L1_DeviceState)  8)

#define VL53L1_DEVICESTATE_UNKNOWN               ((VL53L1_DeviceState) 98)
#define VL53L1_DEVICESTATE_ERROR                 ((VL53L1_DeviceState) 99)











typedef uint8_t VL53L1_DeviceZonePreset;

#define VL53L1_DEVICEZONEPRESET_NONE                 ((VL53L1_DeviceZonePreset)   0)

#define VL53L1_DEVICEZONEPRESET_XTALK_PLANAR         ((VL53L1_DeviceZonePreset)   1)
#define VL53L1_DEVICEZONEPRESET_1X1_SIZE_16X16       ((VL53L1_DeviceZonePreset)   2)
#define VL53L1_DEVICEZONEPRESET_1X2_SIZE_16X8        ((VL53L1_DeviceZonePreset)   3)
#define VL53L1_DEVICEZONEPRESET_2X1_SIZE_8X16        ((VL53L1_DeviceZonePreset)   4)
#define VL53L1_DEVICEZONEPRESET_2X2_SIZE_8X8         ((VL53L1_DeviceZonePreset)   5)
#define VL53L1_DEVICEZONEPRESET_3X3_SIZE_5X5         ((VL53L1_DeviceZonePreset)   6)
#define VL53L1_DEVICEZONEPRESET_4X4_SIZE_4X4         ((VL53L1_DeviceZonePreset)   7)
#define VL53L1_DEVICEZONEPRESET_5X5_SIZE_4X4         ((VL53L1_DeviceZonePreset)   8)
#define VL53L1_DEVICEZONEPRESET_11X11_SIZE_5X5       ((VL53L1_DeviceZonePreset)   9)
#define VL53L1_DEVICEZONEPRESET_13X13_SIZE_4X4       ((VL53L1_DeviceZonePreset)  10)

#define VL53L1_DEVICEZONEPRESET_1X1_SIZE_4X4_POS_8X8 ((VL53L1_DeviceZonePreset)  11)

#define VL53L1_DEVICEZONEPRESET_CUSTOM               ((VL53L1_DeviceZonePreset) 255)











typedef uint8_t VL53L1_DevicePresetModes;

#define VL53L1_DEVICEPRESETMODE_NONE                             ((VL53L1_DevicePresetModes)  0)
#define VL53L1_DEVICEPRESETMODE_STANDARD_RANGING                 ((VL53L1_DevicePresetModes)  1)
#define VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_SHORT_RANGE     ((VL53L1_DevicePresetModes)  2)
#define VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_LONG_RANGE      ((VL53L1_DevicePresetModes)  3)
#define VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM1_CAL         ((VL53L1_DevicePresetModes)  4)
#define VL53L1_DEVICEPRESETMODE_STANDARD_RANGING_MM2_CAL         ((VL53L1_DevicePresetModes)  5)
#define VL53L1_DEVICEPRESETMODE_TIMED_RANGING                    ((VL53L1_DevicePresetModes)  6)
#define VL53L1_DEVICEPRESETMODE_TIMED_RANGING_SHORT_RANGE        ((VL53L1_DevicePresetModes)  7)
#define VL53L1_DEVICEPRESETMODE_TIMED_RANGING_LONG_RANGE         ((VL53L1_DevicePresetModes)  8)
#define VL53L1_DEVICEPRESETMODE_NEAR_FARRANGING                  ((VL53L1_DevicePresetModes)  9)
#define VL53L1_DEVICEPRESETMODE_QUADRANT_RANGING                 ((VL53L1_DevicePresetModes) 10)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING                ((VL53L1_DevicePresetModes) 11)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_SHORT_TIMING   ((VL53L1_DevicePresetModes) 12)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_CHARACTERISATION       ((VL53L1_DevicePresetModes) 13)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_PLANAR           ((VL53L1_DevicePresetModes) 14)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_MM1              ((VL53L1_DevicePresetModes) 15)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_XTALK_MM2              ((VL53L1_DevicePresetModes) 16)
#define VL53L1_DEVICEPRESETMODE_OLT                              ((VL53L1_DevicePresetModes) 17)
#define VL53L1_DEVICEPRESETMODE_SINGLESHOT_RANGING               ((VL53L1_DevicePresetModes) 18)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_REF_ARRAY              ((VL53L1_DevicePresetModes) 19)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_WITH_MM1       ((VL53L1_DevicePresetModes) 20)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_WITH_MM2       ((VL53L1_DevicePresetModes) 21)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM1_CAL        ((VL53L1_DevicePresetModes) 22)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_RANGING_MM2_CAL        ((VL53L1_DevicePresetModes) 23)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE              ((VL53L1_DevicePresetModes) 24)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_SHORT_RANGE  ((VL53L1_DevicePresetModes) 25)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_MULTIZONE_LONG_RANGE	 ((VL53L1_DevicePresetModes) 26)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE             ((VL53L1_DevicePresetModes) 27)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE_MM1         ((VL53L1_DevicePresetModes) 28)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_LONG_RANGE_MM2         ((VL53L1_DevicePresetModes) 29)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE           ((VL53L1_DevicePresetModes) 30)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE_MM1       ((VL53L1_DevicePresetModes) 31)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_MEDIUM_RANGE_MM2       ((VL53L1_DevicePresetModes) 32)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE            ((VL53L1_DevicePresetModes) 33)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE_MM1        ((VL53L1_DevicePresetModes) 34)
#define VL53L1_DEVICEPRESETMODE_HISTOGRAM_SHORT_RANGE_MM2        ((VL53L1_DevicePresetModes) 35)











typedef uint8_t VL53L1_DeviceMeasurementModes;

#define VL53L1_DEVICEMEASUREMENTMODE_STOP                        ((VL53L1_DeviceMeasurementModes)  0x00)
#define VL53L1_DEVICEMEASUREMENTMODE_SINGLESHOT                  ((VL53L1_DeviceMeasurementModes)  0x10)
#define VL53L1_DEVICEMEASUREMENTMODE_BACKTOBACK                  ((VL53L1_DeviceMeasurementModes)  0x20)
#define VL53L1_DEVICEMEASUREMENTMODE_TIMED                       ((VL53L1_DeviceMeasurementModes)  0x40)
#define VL53L1_DEVICEMEASUREMENTMODE_ABORT                       ((VL53L1_DeviceMeasurementModes)  0x80)











typedef uint8_t VL53L1_OffsetCalibrationMode;

#define VL53L1_OFFSETCALIBRATIONMODE__NONE                              ((VL53L1_OffsetCalibrationMode)  0)
#define VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD                 ((VL53L1_OffsetCalibrationMode)  1)
#define VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM                ((VL53L1_OffsetCalibrationMode)  2)
#define VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__STANDARD_PRE_RANGE_ONLY  ((VL53L1_OffsetCalibrationMode)  3)
#define VL53L1_OFFSETCALIBRATIONMODE__MM1_MM2__HISTOGRAM_PRE_RANGE_ONLY ((VL53L1_OffsetCalibrationMode)  4)











typedef uint8_t VL53L1_OffsetCorrectionMode;

#define VL53L1_OFFSETCORRECTIONMODE__NONE               ((VL53L1_OffsetCorrectionMode)  0)
#define VL53L1_OFFSETCORRECTIONMODE__MM1_MM2_OFFSETS    ((VL53L1_OffsetCorrectionMode)  1)
#define VL53L1_OFFSETCORRECTIONMODE__PER_ZONE_OFFSETS   ((VL53L1_OffsetCorrectionMode)  2)











typedef uint8_t VL53L1_DeviceDmaxMode;

#define VL53L1_DEVICEDMAXMODE__NONE                    ((VL53L1_DeviceDmaxMode)  0)
#define VL53L1_DEVICEDMAXMODE__FMT_CAL_DATA            ((VL53L1_DeviceDmaxMode)  1)
#define VL53L1_DEVICEDMAXMODE__CUST_CAL_DATA           ((VL53L1_DeviceDmaxMode)  2)
#define VL53L1_DEVICEDMAXMODE__PER_ZONE_CAL_DATA       ((VL53L1_DeviceDmaxMode)  3)












typedef uint8_t VL53L1_DeviceSequenceConfig;

#define VL53L1_DEVICESEQUENCECONFIG_VHV		         ((VL53L1_DeviceSequenceConfig) 0)
#define VL53L1_DEVICESEQUENCECONFIG_PHASECAL         ((VL53L1_DeviceSequenceConfig) 1)
#define VL53L1_DEVICESEQUENCECONFIG_REFERENCE_PHASE  ((VL53L1_DeviceSequenceConfig) 2)
#define VL53L1_DEVICESEQUENCECONFIG_DSS1             ((VL53L1_DeviceSequenceConfig) 3)
#define VL53L1_DEVICESEQUENCECONFIG_DSS2             ((VL53L1_DeviceSequenceConfig) 4)
#define VL53L1_DEVICESEQUENCECONFIG_MM1              ((VL53L1_DeviceSequenceConfig) 5)
#define VL53L1_DEVICESEQUENCECONFIG_MM2              ((VL53L1_DeviceSequenceConfig) 6)
#define VL53L1_DEVICESEQUENCECONFIG_RANGE            ((VL53L1_DeviceSequenceConfig) 7)











typedef uint8_t VL53L1_DeviceInterruptPolarity;

#define VL53L1_DEVICEINTERRUPTPOLARITY_ACTIVE_HIGH              ((VL53L1_DeviceInterruptPolarity)  0x00)
#define VL53L1_DEVICEINTERRUPTPOLARITY_ACTIVE_LOW               ((VL53L1_DeviceInterruptPolarity)  0x10)
#define VL53L1_DEVICEINTERRUPTPOLARITY_BIT_MASK                 ((VL53L1_DeviceInterruptPolarity)  0x10)
#define VL53L1_DEVICEINTERRUPTPOLARITY_CLEAR_MASK               ((VL53L1_DeviceInterruptPolarity)  0xEF)











typedef uint8_t VL53L1_DeviceGpioMode;

#define VL53L1_DEVICEGPIOMODE_OUTPUT_CONSTANT_ZERO                    ((VL53L1_DeviceGpioMode)  0x00)
#define VL53L1_DEVICEGPIOMODE_OUTPUT_RANGE_AND_ERROR_INTERRUPTS       ((VL53L1_DeviceGpioMode)  0x01)
#define VL53L1_DEVICEGPIOMODE_OUTPUT_TIMIER_INTERRUPTS                ((VL53L1_DeviceGpioMode)  0x02)
#define VL53L1_DEVICEGPIOMODE_OUTPUT_RANGE_MODE_INTERRUPT_STATUS      ((VL53L1_DeviceGpioMode)  0x03)
#define VL53L1_DEVICEGPIOMODE_OUTPUT_SLOW_OSCILLATOR_CLOCK            ((VL53L1_DeviceGpioMode)  0x04)
#define VL53L1_DEVICEGPIOMODE_BIT_MASK                                ((VL53L1_DeviceGpioMode)  0x0F)
#define VL53L1_DEVICEGPIOMODE_CLEAR_MASK                              ((VL53L1_DeviceGpioMode)  0xF0)















typedef uint8_t VL53L1_DeviceError;

#define VL53L1_DEVICEERROR_NOUPDATE                    ((VL53L1_DeviceError) 0)


#define VL53L1_DEVICEERROR_VCSELCONTINUITYTESTFAILURE  ((VL53L1_DeviceError) 1)
#define VL53L1_DEVICEERROR_VCSELWATCHDOGTESTFAILURE    ((VL53L1_DeviceError) 2)
#define VL53L1_DEVICEERROR_NOVHVVALUEFOUND             ((VL53L1_DeviceError) 3)
#define VL53L1_DEVICEERROR_MSRCNOTARGET                ((VL53L1_DeviceError) 4)
#define VL53L1_DEVICEERROR_RANGEPHASECHECK             ((VL53L1_DeviceError) 5)
#define VL53L1_DEVICEERROR_SIGMATHRESHOLDCHECK         ((VL53L1_DeviceError) 6)
#define VL53L1_DEVICEERROR_PHASECONSISTENCY            ((VL53L1_DeviceError) 7)
#define VL53L1_DEVICEERROR_MINCLIP                     ((VL53L1_DeviceError) 8)
#define VL53L1_DEVICEERROR_RANGECOMPLETE               ((VL53L1_DeviceError) 9)
#define VL53L1_DEVICEERROR_ALGOUNDERFLOW               ((VL53L1_DeviceError) 10)
#define VL53L1_DEVICEERROR_ALGOOVERFLOW                ((VL53L1_DeviceError) 11)
#define VL53L1_DEVICEERROR_RANGEIGNORETHRESHOLD        ((VL53L1_DeviceError) 12)
#define VL53L1_DEVICEERROR_USERROICLIP                 ((VL53L1_DeviceError) 13)
#define VL53L1_DEVICEERROR_REFSPADCHARNOTENOUGHDPADS   ((VL53L1_DeviceError) 14)
#define VL53L1_DEVICEERROR_REFSPADCHARMORETHANTARGET   ((VL53L1_DeviceError) 15)
#define VL53L1_DEVICEERROR_REFSPADCHARLESSTHANTARGET   ((VL53L1_DeviceError) 16)
#define VL53L1_DEVICEERROR_MULTCLIPFAIL                ((VL53L1_DeviceError) 17)
#define VL53L1_DEVICEERROR_GPHSTREAMCOUNT0READY        ((VL53L1_DeviceError) 18)
#define VL53L1_DEVICEERROR_RANGECOMPLETE_NO_WRAP_CHECK ((VL53L1_DeviceError) 19)
#define VL53L1_DEVICEERROR_EVENTCONSISTENCY            ((VL53L1_DeviceError) 20)











typedef uint8_t VL53L1_DeviceReportStatus;

#define VL53L1_DEVICEREPORTSTATUS_NOUPDATE                    ((VL53L1_DeviceReportStatus) 0)


#define VL53L1_DEVICEREPORTSTATUS_ROI_SETUP                   ((VL53L1_DeviceReportStatus)  1)
#define VL53L1_DEVICEREPORTSTATUS_VHV                         ((VL53L1_DeviceReportStatus)  2)
#define VL53L1_DEVICEREPORTSTATUS_PHASECAL                    ((VL53L1_DeviceReportStatus)  3)
#define VL53L1_DEVICEREPORTSTATUS_REFERENCE_PHASE             ((VL53L1_DeviceReportStatus)  4)
#define VL53L1_DEVICEREPORTSTATUS_DSS1                        ((VL53L1_DeviceReportStatus)  5)
#define VL53L1_DEVICEREPORTSTATUS_DSS2                        ((VL53L1_DeviceReportStatus)  6)
#define VL53L1_DEVICEREPORTSTATUS_MM1                         ((VL53L1_DeviceReportStatus)  7)
#define VL53L1_DEVICEREPORTSTATUS_MM2                         ((VL53L1_DeviceReportStatus)  8)
#define VL53L1_DEVICEREPORTSTATUS_RANGE                       ((VL53L1_DeviceReportStatus)  9)
#define VL53L1_DEVICEREPORTSTATUS_HISTOGRAM                   ((VL53L1_DeviceReportStatus) 10)










typedef uint8_t VL53L1_DeviceDssMode;

#define VL53L1_DEVICEDSSMODE__DISABLED \
	((VL53L1_DeviceDssMode) 0)
#define VL53L1_DEVICEDSSMODE__TARGET_RATE \
	((VL53L1_DeviceDssMode) 1)
#define VL53L1_DEVICEDSSMODE__REQUESTED_EFFFECTIVE_SPADS \
	((VL53L1_DeviceDssMode) 2)
#define VL53L1_DEVICEDSSMODE__BLOCK_SELECT \
	((VL53L1_DeviceDssMode) 3)












typedef uint8_t VL53L1_HistAlgoSelect;

#define VL53L1_HIST_ALGO_SELECT__PW_HIST_GEN1 \
	((VL53L1_HistAlgoSelect) 1)
#define VL53L1_HIST_ALGO_SELECT__PW_HIST_GEN2 \
	((VL53L1_HistAlgoSelect) 2)
#define VL53L1_HIST_ALGO_SELECT__PW_HIST_GEN3 \
	((VL53L1_HistAlgoSelect) 3)
#define VL53L1_HIST_ALGO_SELECT__PW_HIST_GEN4 \
	((VL53L1_HistAlgoSelect) 4)











typedef uint8_t VL53L1_HistTargetOrder;

#define VL53L1_HIST_TARGET_ORDER__INCREASING_DISTANCE \
	((VL53L1_HistTargetOrder) 1)
#define VL53L1_HIST_TARGET_ORDER__STRONGEST_FIRST \
	((VL53L1_HistTargetOrder) 2)











typedef uint8_t VL53L1_HistAmbEstMethod;

#define VL53L1_HIST_AMB_EST_METHOD__AMBIENT_BINS \
	((VL53L1_HistAmbEstMethod) 1)
#define VL53L1_HIST_AMB_EST_METHOD__THRESHOLDED_BINS  \
	((VL53L1_HistAmbEstMethod) 2)












typedef uint8_t VL53L1_HistXtalkCompEnable;

#define VL53L1_HIST_XTALK_COMP__DIS \
	((VL53L1_HistXtalkCompEnable) 0)
#define VL53L1_HIST_XTALK_COMP__EN \
	((VL53L1_HistXtalkCompEnable) 1)









typedef uint8_t VL53L1_DeviceConfigLevel;

#define VL53L1_DEVICECONFIGLEVEL_SYSTEM_CONTROL  \
	((VL53L1_DeviceConfigLevel)  0)


#define VL53L1_DEVICECONFIGLEVEL_DYNAMIC_ONWARDS \
	((VL53L1_DeviceConfigLevel)  1)


#define VL53L1_DEVICECONFIGLEVEL_TIMING_ONWARDS \
	((VL53L1_DeviceConfigLevel)  2)



#define VL53L1_DEVICECONFIGLEVEL_GENERAL_ONWARDS \
	((VL53L1_DeviceConfigLevel)  3)



#define VL53L1_DEVICECONFIGLEVEL_STATIC_ONWARDS  \
	((VL53L1_DeviceConfigLevel)  4)



#define VL53L1_DEVICECONFIGLEVEL_CUSTOMER_ONWARDS  \
	((VL53L1_DeviceConfigLevel)  5)



#define VL53L1_DEVICECONFIGLEVEL_FULL  \
	((VL53L1_DeviceConfigLevel)  6)














typedef uint8_t VL53L1_DeviceResultsLevel;

#define VL53L1_DEVICERESULTSLEVEL_SYSTEM_RESULTS  \
	((VL53L1_DeviceResultsLevel)  0)


#define VL53L1_DEVICERESULTSLEVEL_UPTO_CORE  \
	((VL53L1_DeviceResultsLevel)  1)


#define VL53L1_DEVICERESULTSLEVEL_FULL  \
	((VL53L1_DeviceResultsLevel)  2)















typedef uint8_t VL53L1_DeviceTestMode;

#define VL53L1_DEVICETESTMODE_NONE \
	((VL53L1_DeviceTestMode) 0x00)


#define VL53L1_DEVICETESTMODE_NVM_ZERO \
	((VL53L1_DeviceTestMode) 0x01)


#define VL53L1_DEVICETESTMODE_NVM_COPY \
	((VL53L1_DeviceTestMode) 0x02)


#define VL53L1_DEVICETESTMODE_PATCH \
	((VL53L1_DeviceTestMode) 0x03)


#define VL53L1_DEVICETESTMODE_DCR \
	((VL53L1_DeviceTestMode) 0x04)


#define VL53L1_DEVICETESTMODE_LCR_VCSEL_OFF \
	((VL53L1_DeviceTestMode) 0x05)



#define VL53L1_DEVICETESTMODE_LCR_VCSEL_ON \
	((VL53L1_DeviceTestMode) 0x06)



#define VL53L1_DEVICETESTMODE_SPOT_CENTRE_LOCATE \
	((VL53L1_DeviceTestMode) 0x07)


#define VL53L1_DEVICETESTMODE_REF_SPAD_CHAR_WITH_PRE_VHV \
	((VL53L1_DeviceTestMode) 0x08)


#define VL53L1_DEVICETESTMODE_REF_SPAD_CHAR_ONLY \
	((VL53L1_DeviceTestMode) 0x09)













typedef uint8_t VL53L1_DeviceSscArray;

#define VL53L1_DEVICESSCARRAY_RTN ((VL53L1_DeviceSscArray) 0x00)


#define VL53L1_DEVICETESTMODE_REF ((VL53L1_DeviceSscArray) 0x01)













#define VL53L1_RETURN_ARRAY_ONLY                   0x01


#define VL53L1_REFERENCE_ARRAY_ONLY                0x10


#define VL53L1_BOTH_RETURN_AND_REFERENCE_ARRAYS    0x11


#define VL53L1_NEITHER_RETURN_AND_REFERENCE_ARRAYS 0x00












#define VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_HIGH               0x00


#define VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_LOW                0x10


#define VL53L1_DEVICEINTERRUPTLEVEL_ACTIVE_MASK               0x10












#define VL53L1_POLLING_DELAY_US                     1000


#define VL53L1_SOFTWARE_RESET_DURATION_US            100


#define VL53L1_FIRMWARE_BOOT_TIME_US                1200



#define VL53L1_ENABLE_POWERFORCE_SETTLING_TIME_US    250




#define VL53L1_SPAD_ARRAY_WIDTH                       16


#define VL53L1_SPAD_ARRAY_HEIGHT                      16


#define VL53L1_NVM_SIZE_IN_BYTES                     512


#define VL53L1_NO_OF_SPAD_ENABLES                    256


#define VL53L1_RTN_SPAD_BUFFER_SIZE                   32


#define VL53L1_REF_SPAD_BUFFER_SIZE                    6


#define VL53L1_AMBIENT_WINDOW_VCSEL_PERIODS          256


#define VL53L1_RANGING_WINDOW_VCSEL_PERIODS         2048


#define VL53L1_MACRO_PERIOD_VCSEL_PERIODS \
	(VL53L1_AMBIENT_WINDOW_VCSEL_PERIODS + VL53L1_RANGING_WINDOW_VCSEL_PERIODS)


#define VL53L1_MAX_ALLOWED_PHASE                    0xFFFF



#define VL53L1_RTN_SPAD_UNITY_TRANSMISSION      0x0100


#define VL53L1_RTN_SPAD_APERTURE_TRANSMISSION   0x0038





#define VL53L1_SPAD_TOTAL_COUNT_MAX                 ((0x01 << 29) - 1)


#define VL53L1_SPAD_TOTAL_COUNT_RES_THRES            (0x01 << 24)


#define VL53L1_COUNT_RATE_INTERNAL_MAX              ((0x01 << 24) - 1)


#define VL53L1_SPEED_OF_LIGHT_IN_AIR                299704


#define VL53L1_SPEED_OF_LIGHT_IN_AIR_DIV_8          (299704 >> 3)
















typedef uint8_t VL53L1_ZoneConfig_BinConfig_select;

#define VL53L1_ZONECONFIG_BINCONFIG__LOWAMB \
	((VL53L1_ZoneConfig_BinConfig_select) 1)
#define VL53L1_ZONECONFIG_BINCONFIG__MIDAMB \
	((VL53L1_ZoneConfig_BinConfig_select) 2)
#define VL53L1_ZONECONFIG_BINCONFIG__HIGHAMB \
	((VL53L1_ZoneConfig_BinConfig_select) 3)










typedef uint8_t VL53L1_GPIO_Interrupt_Mode;

#define VL53L1_GPIOINTMODE_LEVEL_LOW \
	((VL53L1_GPIO_Interrupt_Mode) 0)


#define VL53L1_GPIOINTMODE_LEVEL_HIGH \
	((VL53L1_GPIO_Interrupt_Mode) 1)


#define VL53L1_GPIOINTMODE_OUT_OF_WINDOW \
	((VL53L1_GPIO_Interrupt_Mode) 2)


#define VL53L1_GPIOINTMODE_IN_WINDOW \
	((VL53L1_GPIO_Interrupt_Mode) 3)







#endif






