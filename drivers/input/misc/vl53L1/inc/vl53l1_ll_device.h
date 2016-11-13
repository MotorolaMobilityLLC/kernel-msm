
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













typedef uint8_t VL53L1_WaitMethod;

#define VL53L1_DEF_00050               ((VL53L1_WaitMethod)  0)
#define VL53L1_DEF_00209           ((VL53L1_WaitMethod)  1)









typedef uint8_t VL53L1_DeviceState;

#define VL53L1_DEF_00210              ((VL53L1_DeviceState)  0)
#define VL53L1_DEF_00211             ((VL53L1_DeviceState)  1)
#define VL53L1_DEF_00184            ((VL53L1_DeviceState)  2)
#define VL53L1_DEF_00056             ((VL53L1_DeviceState)  3)
#define VL53L1_DEF_00133       ((VL53L1_DeviceState)  4)
#define VL53L1_DEF_00134     ((VL53L1_DeviceState)  5)
#define VL53L1_DEF_00035  ((VL53L1_DeviceState)  6)
#define VL53L1_DEF_00128    ((VL53L1_DeviceState)  7)
#define VL53L1_DEF_00129    ((VL53L1_DeviceState)  8)

#define VL53L1_DEF_00049               ((VL53L1_DeviceState) 98)
#define VL53L1_DEF_00212                 ((VL53L1_DeviceState) 99)









typedef uint8_t VL53L1_DevicePresetModes;

#define VL53L1_DEF_00213                             ((VL53L1_DevicePresetModes)  0)
#define VL53L1_DEF_00006                 ((VL53L1_DevicePresetModes)  1)
#define VL53L1_DEF_00057     ((VL53L1_DevicePresetModes)  2)
#define VL53L1_DEF_00058      ((VL53L1_DevicePresetModes)  3)
#define VL53L1_DEF_00059         ((VL53L1_DevicePresetModes)  4)
#define VL53L1_DEF_00060         ((VL53L1_DevicePresetModes)  5)
#define VL53L1_DEF_00061                    ((VL53L1_DevicePresetModes)  6)
#define VL53L1_DEF_00214                  ((VL53L1_DevicePresetModes)  7)
#define VL53L1_DEF_00215                 ((VL53L1_DevicePresetModes)  8)
#define VL53L1_DEF_00001                ((VL53L1_DevicePresetModes)  9)
#define VL53L1_DEF_00065   ((VL53L1_DevicePresetModes) 10)
#define VL53L1_DEF_00002       ((VL53L1_DevicePresetModes) 11)
#define VL53L1_DEF_00003           ((VL53L1_DevicePresetModes) 12)
#define VL53L1_DEF_00007                              ((VL53L1_DevicePresetModes) 13)
#define VL53L1_DEF_00066               ((VL53L1_DevicePresetModes) 14)
#define VL53L1_DEF_00064              ((VL53L1_DevicePresetModes) 15)
#define VL53L1_DEF_00062       ((VL53L1_DevicePresetModes) 16)
#define VL53L1_DEF_00063       ((VL53L1_DevicePresetModes) 17)










typedef uint8_t VL53L1_DeviceMeasurementModes;

#define VL53L1_DEF_00051                        ((VL53L1_DeviceMeasurementModes)  0x00)
#define VL53L1_DEF_00121                  ((VL53L1_DeviceMeasurementModes)  0x10)
#define VL53L1_DEF_00005                  ((VL53L1_DeviceMeasurementModes)  0x20)
#define VL53L1_DEF_00120                       ((VL53L1_DeviceMeasurementModes)  0x40)
#define VL53L1_DEF_00086                       ((VL53L1_DeviceMeasurementModes)  0x80)














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

#define VL53L1_DEF_00216                    ((VL53L1_DeviceReportStatus) 0)
	

#define VL53L1_DEF_00217                   ((VL53L1_DeviceReportStatus)  1)
#define VL53L1_DEF_00218                         ((VL53L1_DeviceReportStatus)  2)
#define VL53L1_DEF_00219                    ((VL53L1_DeviceReportStatus)  3)
#define VL53L1_DEF_00220             ((VL53L1_DeviceReportStatus)  4)
#define VL53L1_DEF_00221                        ((VL53L1_DeviceReportStatus)  5)
#define VL53L1_DEF_00222                        ((VL53L1_DeviceReportStatus)  6)
#define VL53L1_DEF_00223                         ((VL53L1_DeviceReportStatus)  7)
#define VL53L1_DEF_00224                         ((VL53L1_DeviceReportStatus)  8)
#define VL53L1_DEF_00225                       ((VL53L1_DeviceReportStatus)  9)
#define VL53L1_DEF_00226                   ((VL53L1_DeviceReportStatus) 10)












typedef uint8_t VL53L1_DeviceConfigLevel;

#define VL53L1_DEF_00190  \
	((VL53L1_DeviceConfigLevel)  0)
	

#define VL53L1_DEF_00076 \
	((VL53L1_DeviceConfigLevel)  1)
	

#define VL53L1_DEF_00074 \
	((VL53L1_DeviceConfigLevel)  2)
	


#define VL53L1_DEF_00072 \
	((VL53L1_DeviceConfigLevel)  3)
	


#define VL53L1_DEF_00070  \
	((VL53L1_DeviceConfigLevel)  4)
	


#define VL53L1_DEF_00034  \
	((VL53L1_DeviceConfigLevel)  5)
	


#define VL53L1_DEF_00008  \
	((VL53L1_DeviceConfigLevel)  6)
	













typedef uint8_t VL53L1_DeviceResultsLevel;

#define VL53L1_DEF_00191  \
	((VL53L1_DeviceResultsLevel)  0)
	

#define VL53L1_DEF_00090  \
	((VL53L1_DeviceResultsLevel)  1)
	

#define VL53L1_DEF_00028  \
	((VL53L1_DeviceResultsLevel)  2)
	













#define VL53L1_DEF_00227                         0x00
	

#define VL53L1_DEF_00228                     0x01
	

#define VL53L1_DEF_00229                     0x02
	

#define VL53L1_DEF_00230                        0x03
	

#define VL53L1_DEF_00231                          0x04
	

#define VL53L1_DEF_00232                0x05
	


#define VL53L1_DEF_00233                 0x06
	


#define VL53L1_DEF_00234           0x07
	

#define VL53L1_DEF_00104   0x08
	

#define VL53L1_DEF_00235           0x09
	












#define VL53L1_DEF_00236                   0x01
	

#define VL53L1_DEF_00237                0x10
	

#define VL53L1_DEF_00238    0x11
	

#define VL53L1_DEF_00239 0x00
	











#define VL53L1_DEF_00187               0x00
	

#define VL53L1_DEF_00240                0x10
	

#define VL53L1_DEF_00186               0x10
	











#define VL53L1_DEF_00241                     1000
	

#define VL53L1_DEF_00055            100
	

#define VL53L1_DEF_00188               16000
	


#define VL53L1_DEF_00242                       16
	

#define VL53L1_DEF_00243                      16
	

#define VL53L1_DEF_00244                     512
	

#define VL53L1_DEF_00245                    256
	

#define VL53L1_DEF_00246                   32
	

#define VL53L1_DEF_00247                    6
	

#define VL53L1_DEF_00248          256
	

#define VL53L1_DEF_00103         2048
	

#define VL53L1_DEF_00141 \
	(VL53L1_DEF_00248 + VL53L1_DEF_00103)
	

#define VL53L1_DEF_00249                    0xFFFF
	


#define VL53L1_DEF_00143                 ((0x01 << 29) - 1)
	

#define VL53L1_DEF_00144            (0x01 << 24)
	

#define VL53L1_DEF_00250              ((0x01 << 24) - 1)
	

#define VL53L1_DEF_00251                299704
	

#define VL53L1_DEF_00142          (299704 >> 3)
	










#endif





