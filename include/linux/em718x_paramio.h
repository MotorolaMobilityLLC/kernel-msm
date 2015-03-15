/**
 * \file
 *
 * \authors   Pete Skeggs <pete.skeggs@emmicro-us.com>
 *
 * \brief     parameter numbers for load/save
 *
 * \copyright (C) 2013-2014 EM Microelectronic – US, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _EM718X_PARAMETERS_H_
#define _EM718X_PARAMETERS_H_

#include "em718x_types.h"


/** \brief maximum parameter value */
#define MAX_PARAM_NUMBER 127

#define SP_STOP                     0  /**< terminate the parameter procedure */

/**************************************************************
* parameters 1 to 73 subject to change                        *
***************************************************************/
#define SP_PARAM_START               1 /**< first param (coefficient) in the set of params */
#define SP_PARAM_END                35 /**< last param */
#define SP_KNOB_START               51 /**< first knob in the set of knobs */
#define SP_KNOB_END                 73 /**< last knob */
#define SP_SENSOR_DYNAMIC_RANGE_1_2 74 /**< read or set dynamic range for mag and accel */
#define SP_SENSOR_DYNAMIC_RANGE_3_4 75 /**< read or set dynamic range for gyro and custom sensor 0 */
#define SP_SENSOR_DYNAMIC_RANGE_5_6 76 /**< read or set dynamic range for custom sensors 1 and 2 */
#define SP_DRIVER_ID_REV_1_2        77 /**< read driver ID and rev for mag and accel */
#define SP_DRIVER_ID_REV_3_4        78 /**< read driver ID and rev for gyro and custom sensor 0 */
#define SP_DRIVER_ID_REV_5_6        79 /**< read driver ID and rev for custom sensors 1 and 2 */
#define SP_SPACEPOINT_REV           80 /**< read revision of spacepoint algorithm */

/**
 * \brief decoding of 4 byte value for SP_SENSOR_DYNAMIC_RANGE_*
 */
PREPACK typedef struct MIDPACK SENSOR_DYNAMIC_RANGE
{
	u16 range_1;                                                   /**< first driver's range */
	u16 range_2;                                                   /**< second driver's range */
}
SENSOR_DYNAMIC_RANGE;                                             /**< typedef for storing the Sensor Dynamic Range value coming from the Sentral Parameters SP_SENSOR_DYNAMIC_RANGE_1_2, SP_SENSOR_DYNAMIC_RANGE_3_4, or SP_SENSOR_DYNAMIC_RANGE_5_6 */
POSTPACK

/**
 * \brief decoding of 4 byte value for SP_DRIVER_ID_REV_*
 */
PREPACK typedef struct MIDPACK DRIVER_ID_REV
{
	u8 rev_driver_1;                                               /**< first driver's revision number */
	u8 id_driver_1;                                                /**< first driver's ID */
	u8 rev_driver_2;                                               /**< second driver's revision number */
	u8 id_driver_2;                                                /**< second driver's ID */
}
DRIVER_ID_REV;                                                    /**< typedef for storing the Driver ID/Rev value coming from the Sentral Parameters SP_DRIVER_ID_REV_1_2, SP_DRIVER_ID_REV_3_4, or SP_DRIVER_ID_REV_5_6 */
POSTPACK

#define ID_BMX055 0x01           /**< driver ID for BMX055 accel, gyro, and mag sensors */
#define ID_RM3100 0x02           /**< driver ID for PNI RM3100 (Point Reyes) mag sensor */
#define ID_AK8963 0x03           /**< driver ID for AK8963 mag sensor */
#define ID_AK8975 0x04           /**< driver ID for AK8975 mag sensor */
#define ID_LSM330DLCACCEL 0x05   /**< driver ID for LSM330DLC accel sensor */
#define ID_LSM330GYRO 0x06       /**< driver ID for LSM330 gyro driver */
#define ID_LSM330ACCEL 0x07      /**< driver ID for LSM330 accel driver */

#endif
