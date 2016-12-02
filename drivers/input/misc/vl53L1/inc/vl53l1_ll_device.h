
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





































#ifndef _VL53L1_LL_DEVICE_H_
#define _VL53L1_LL_DEVICE_H_

#include "vl53l1_types.h"

#define   VL53L1_DEF_00231                      0x01
#define   VL53L1_DEF_00232                      0x00













typedef uint8_t VL53L1_WaitMethod;

#define VL53L1_DEF_00052               ((VL53L1_WaitMethod)  0)
#define VL53L1_DEF_00233           ((VL53L1_WaitMethod)  1)










typedef uint8_t VL53L1_DeviceState;

#define VL53L1_DEF_00234              ((VL53L1_DeviceState)  0)
#define VL53L1_DEF_00235             ((VL53L1_DeviceState)  1)
#define VL53L1_DEF_00205            ((VL53L1_DeviceState)  2)
#define VL53L1_DEF_00058             ((VL53L1_DeviceState)  3)
#define VL53L1_DEF_00150       ((VL53L1_DeviceState)  4)
#define VL53L1_DEF_00151     ((VL53L1_DeviceState)  5)
#define VL53L1_DEF_00038  ((VL53L1_DeviceState)  6)
#define VL53L1_DEF_00029    ((VL53L1_DeviceState)  7)
#define VL53L1_DEF_00030    ((VL53L1_DeviceState)  8)

#define VL53L1_DEF_00051               ((VL53L1_DeviceState) 98)
#define VL53L1_DEF_00236                 ((VL53L1_DeviceState) 99)











typedef uint8_t VL53L1_DeviceZonePreset;

#define VL53L1_DEF_00053            ((VL53L1_DeviceZonePreset)  0)
#define VL53L1_DEF_00076    ((VL53L1_DeviceZonePreset)  1)
#define VL53L1_DEF_00077  ((VL53L1_DeviceZonePreset)  2)
#define VL53L1_DEF_00078    ((VL53L1_DeviceZonePreset)  3)
#define VL53L1_DEF_00079    ((VL53L1_DeviceZonePreset)  4)
#define VL53L1_DEF_00080    ((VL53L1_DeviceZonePreset)  5)
#define VL53L1_DEF_00081  ((VL53L1_DeviceZonePreset)  6)
#define VL53L1_DEF_00082  ((VL53L1_DeviceZonePreset)  7)











typedef uint8_t VL53L1_DevicePresetModes;

#define VL53L1_DEF_00237                             ((VL53L1_DevicePresetModes)  0)
#define VL53L1_DEF_00003                 ((VL53L1_DevicePresetModes)  1)
#define VL53L1_DEF_00059     ((VL53L1_DevicePresetModes)  2)
#define VL53L1_DEF_00060      ((VL53L1_DevicePresetModes)  3)
#define VL53L1_DEF_00039         ((VL53L1_DevicePresetModes)  4)
#define VL53L1_DEF_00040         ((VL53L1_DevicePresetModes)  5)
#define VL53L1_DEF_00006                    ((VL53L1_DevicePresetModes)  6)
#define VL53L1_DEF_00238                  ((VL53L1_DevicePresetModes)  7)
#define VL53L1_DEF_00239                 ((VL53L1_DevicePresetModes)  8)
#define VL53L1_DEF_00004                ((VL53L1_DevicePresetModes)  9)
#define VL53L1_DEF_00064   ((VL53L1_DevicePresetModes) 10)
#define VL53L1_DEF_00074       ((VL53L1_DevicePresetModes) 11)
#define VL53L1_DEF_00036           ((VL53L1_DevicePresetModes) 12)
#define VL53L1_DEF_00007                              ((VL53L1_DevicePresetModes) 13)
#define VL53L1_DEF_00075               ((VL53L1_DevicePresetModes) 14)
#define VL53L1_DEF_00063              ((VL53L1_DevicePresetModes) 15)
#define VL53L1_DEF_00061       ((VL53L1_DevicePresetModes) 16)
#define VL53L1_DEF_00062       ((VL53L1_DevicePresetModes) 17)
#define VL53L1_DEF_00240        ((VL53L1_DevicePresetModes) 18)
#define VL53L1_DEF_00241        ((VL53L1_DevicePresetModes) 19)
#define VL53L1_DEF_00005              ((VL53L1_DevicePresetModes) 20)
#define VL53L1_DEF_00065        ((VL53L1_DevicePresetModes) 21)
#define VL53L1_DEF_00066    ((VL53L1_DevicePresetModes) 22)
#define VL53L1_DEF_00067    ((VL53L1_DevicePresetModes) 23)
#define VL53L1_DEF_00068        ((VL53L1_DevicePresetModes) 24)
#define VL53L1_DEF_00069    ((VL53L1_DevicePresetModes) 25)
#define VL53L1_DEF_00070    ((VL53L1_DevicePresetModes) 26)
#define VL53L1_DEF_00071       ((VL53L1_DevicePresetModes) 27)
#define VL53L1_DEF_00072   ((VL53L1_DevicePresetModes) 28)
#define VL53L1_DEF_00073   ((VL53L1_DevicePresetModes) 29)











typedef uint8_t VL53L1_DeviceMeasurementModes;

#define VL53L1_DEF_00054                        ((VL53L1_DeviceMeasurementModes)  0x00)
#define VL53L1_DEF_00139                  ((VL53L1_DeviceMeasurementModes)  0x10)
#define VL53L1_DEF_00002                  ((VL53L1_DeviceMeasurementModes)  0x20)
#define VL53L1_DEF_00138                       ((VL53L1_DeviceMeasurementModes)  0x40)
#define VL53L1_DEF_00102                       ((VL53L1_DeviceMeasurementModes)  0x80)















typedef uint8_t VL53L1_DeviceError;

#define VL53L1_DEF_00009                    ((VL53L1_DeviceError) 0)


#define VL53L1_DEF_00019  ((VL53L1_DeviceError) 1)
#define VL53L1_DEF_00020    ((VL53L1_DeviceError) 2)
#define VL53L1_DEF_00021             ((VL53L1_DeviceError) 3)
#define VL53L1_DEF_00026                ((VL53L1_DeviceError) 4)
#define VL53L1_DEF_00022             ((VL53L1_DeviceError) 5)
#define VL53L1_DEF_00027         ((VL53L1_DeviceError) 6)
#define VL53L1_DEF_00023            ((VL53L1_DeviceError) 7)
#define VL53L1_DEF_00024                     ((VL53L1_DeviceError) 8)
#define VL53L1_DEF_00018               ((VL53L1_DeviceError) 9)
#define VL53L1_DEF_00010               ((VL53L1_DeviceError) 10)
#define VL53L1_DEF_00011                ((VL53L1_DeviceError) 11)
#define VL53L1_DEF_00012        ((VL53L1_DeviceError) 12)
#define VL53L1_DEF_00025                 ((VL53L1_DeviceError) 13)
#define VL53L1_DEF_00013   ((VL53L1_DeviceError) 14)
#define VL53L1_DEF_00014   ((VL53L1_DeviceError) 15)
#define VL53L1_DEF_00015   ((VL53L1_DeviceError) 16)
#define VL53L1_DEF_00016                ((VL53L1_DeviceError) 17)
#define VL53L1_DEF_00017        ((VL53L1_DeviceError) 18)











typedef uint8_t VL53L1_DeviceReportStatus;

#define VL53L1_DEF_00242                    ((VL53L1_DeviceReportStatus) 0)


#define VL53L1_DEF_00243                   ((VL53L1_DeviceReportStatus)  1)
#define VL53L1_DEF_00244                         ((VL53L1_DeviceReportStatus)  2)
#define VL53L1_DEF_00245                    ((VL53L1_DeviceReportStatus)  3)
#define VL53L1_DEF_00246             ((VL53L1_DeviceReportStatus)  4)
#define VL53L1_DEF_00247                        ((VL53L1_DeviceReportStatus)  5)
#define VL53L1_DEF_00248                        ((VL53L1_DeviceReportStatus)  6)
#define VL53L1_DEF_00249                         ((VL53L1_DeviceReportStatus)  7)
#define VL53L1_DEF_00250                         ((VL53L1_DeviceReportStatus)  8)
#define VL53L1_DEF_00251                       ((VL53L1_DeviceReportStatus)  9)
#define VL53L1_DEF_00252                   ((VL53L1_DeviceReportStatus) 10)












typedef uint8_t VL53L1_HistAlgoSelect;

#define VL53L1_DEF_00253 \
	((VL53L1_HistAlgoSelect) 1)
#define VL53L1_DEF_00254 \
	((VL53L1_HistAlgoSelect) 2)
#define VL53L1_DEF_00122 \
	((VL53L1_HistAlgoSelect) 3)











typedef uint8_t VL53L1_HistAmbEstMethod;

#define VL53L1_DEF_00123 \
	((VL53L1_HistAmbEstMethod) 1)
#define VL53L1_DEF_00255  \
	((VL53L1_HistAmbEstMethod) 2)











typedef uint8_t VL53L1_DeviceConfigLevel;

#define VL53L1_DEF_00211  \
	((VL53L1_DeviceConfigLevel)  0)


#define VL53L1_DEF_00092 \
	((VL53L1_DeviceConfigLevel)  1)


#define VL53L1_DEF_00090 \
	((VL53L1_DeviceConfigLevel)  2)



#define VL53L1_DEF_00088 \
	((VL53L1_DeviceConfigLevel)  3)



#define VL53L1_DEF_00086  \
	((VL53L1_DeviceConfigLevel)  4)



#define VL53L1_DEF_00037  \
	((VL53L1_DeviceConfigLevel)  5)



#define VL53L1_DEF_00008  \
	((VL53L1_DeviceConfigLevel)  6)














typedef uint8_t VL53L1_DeviceResultsLevel;

#define VL53L1_DEF_00212  \
	((VL53L1_DeviceResultsLevel)  0)


#define VL53L1_DEF_00106  \
	((VL53L1_DeviceResultsLevel)  1)


#define VL53L1_DEF_00028  \
	((VL53L1_DeviceResultsLevel)  2)















#define VL53L1_DEF_00256                         0x00


#define VL53L1_DEF_00257                     0x01


#define VL53L1_DEF_00258                     0x02


#define VL53L1_DEF_00259                        0x03


#define VL53L1_DEF_00260                          0x04


#define VL53L1_DEF_00261                0x05



#define VL53L1_DEF_00262                 0x06



#define VL53L1_DEF_00263           0x07


#define VL53L1_DEF_00121   0x08


#define VL53L1_DEF_00264           0x09













#define VL53L1_DEF_00265                   0x01


#define VL53L1_DEF_00266                0x10


#define VL53L1_DEF_00267    0x11


#define VL53L1_DEF_00268 0x00












#define VL53L1_DEF_00208               0x00


#define VL53L1_DEF_00269                0x10


#define VL53L1_DEF_00207               0x10












#define VL53L1_DEF_00270                     1000


#define VL53L1_DEF_00057            100


#define VL53L1_DEF_00209               16000



#define VL53L1_DEF_00271                       16


#define VL53L1_DEF_00272                      16


#define VL53L1_DEF_00273                     512


#define VL53L1_DEF_00274                    256


#define VL53L1_DEF_00275                   32


#define VL53L1_DEF_00276                    6


#define VL53L1_DEF_00277          256


#define VL53L1_DEF_00119         2048


#define VL53L1_DEF_00159 \
	(VL53L1_DEF_00277 + VL53L1_DEF_00119)


#define VL53L1_DEF_00278                    0xFFFF



#define VL53L1_DEF_00161                 ((0x01 << 29) - 1)


#define VL53L1_DEF_00162            (0x01 << 24)


#define VL53L1_DEF_00279              ((0x01 << 24) - 1)


#define VL53L1_DEF_00280                299704


#define VL53L1_DEF_00160          (299704 >> 3)











#endif





