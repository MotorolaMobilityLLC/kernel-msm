/**
 * \file
 *
 * \authors   Pete Skeggs <pete.skeggs@emmicro-us.com>
 *
 * \brief     I2C register addresses, bit structures, and
 *       	  enumerations used by the generic Sentral host driver.
 *
 * \copyright (C) 2014 EM Microelectronic – US, Inc.
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


#ifndef _EM718X_REGISTERS_H_
#define _EM718X_REGISTERS_H_

#include "em718x_types.h"

/** \brief RO = read only; RW = read/write; U32 = unsigned 32
 *       bit register; U16 = unsigned 16 bit register; I16 =
 *       signed 16 bit register; U8 = unsigned 8 bit
 *       register */
#define SR_QX                    0x00 /**< RO; float 32, quaternion X */
#define SR_QY                    0x04 /**< RO; float 32, quaternion Y */
#define SR_QZ                    0x08 /**< RO; float 32, quaternion Z */
#define SR_QW                    0x0C /**< RO; float 32, quaternion W */
#define SR_QTIME                 0x10 /**< RO; U16; quaternion data timestamp */

#define SR_MX                    0x12 /**< RO; I16; magnetic field X axis */
#define SR_MY                    0x14 /**< RO; I16; magnetic field Y axis */
#define SR_MZ                    0x16 /**< RO; I16; magnetic field Z axis */
#define SR_MTIME                 0x18 /**< RO; U16; magnetometer interrupt timestamp */

#define SR_AX                    0x1A /**< RO; I16; linear acceleration, X axis */
#define SR_AY                    0x1C /**< RO; I16; linear acceleration, X axis */
#define SR_AZ                    0x1E /**< RO; I16; linear acceleration, X axis */
#define SR_ATIME                 0x20 /**< RO; U16; accelerometer interrupt timestamp */

#define SR_GX                    0x22 /**< RO; I16; rotational velocity, X axis */
#define SR_GY                    0x24 /**< RO; I16; rotational velocity, Y axis */
#define SR_GZ                    0x26 /**< RO; I16; rotational velocity, Z axis */
#define SR_GTIME                 0x28 /**< RO; U16; gyroscope interrupt timestamp */

#define SR_FEATURE0              0x2A /**< RO; I16; space for additional sensor data */
#define SR_FEATURE1              0x2C /**< RO; I16; space for additional sensor data */
#define SR_FEATURE2              0x2E /**< RO; I16; space for additional sensor data */
#define SR_FEATURE3              0x30 /**< RO; I16; space for additional sensor data */

#define SR_QRATE_DIVISOR         0x32 /**< RW; U8; divides into gyroscope rate to give host interrupt rate for quaternion data */

#define SR_ENABLE_EVENTS         0x33 /**< RW; U8; bit flags; see RegEnableEvents below */
#define SR_HOST_CONTROL          0x34 /**< RW; U8; bit flags; see RegHostControl below */
#define SR_EVENT_STATUS          0x35 /**< RO; U8; bit flags; see RegEventStatus below */
#define SR_SENSOR_STATUS         0x36 /**< RO; U8; bit flags; see RegSensorStatus below */
#define SR_EM718X_STATUS         0x37 /**< RO; U8; bit flags; see RegSentralStatus below */
#define SR_ALGO_STATUS           0x38 /**< RO; U8; AKA standby status; bit flags; see RegAlgorithmStatus below */
#define SR_FEATURE_FLAGS         0x39 /**< RO; U8; bit flags; see RegFeatureFlags below */

#define SR_PARAMETER_ACKNOWLEDGE 0x3A /**< R/O: U8: echos back SR_PARAMETER_REQUEST */
#define SR_SAVED_PARAM_B0        0x3B /**< R/O: U8/U16/U32: byte 0 of parameter being read back */
#define SR_SAVED_PARAM_B1        0x3C /**< R/O: U8: byte 1 of parameter being read back */
#define SR_SAVED_PARAM_B2        0x3D /**< R/O: U8: byte 2 of parameter being read back */
#define SR_SAVED_PARAM_B3        0x3E /**< R/O: U8: byte 3 of parameter being read back */

/* cal status registers 0x3F-0x44 and 0x4D-0x4F */

#define SR_ACTUAL_MAG_RATE       0x45 /**< R/O: U8: actual magnetometer rate in Hz */
#define SR_ACTUAL_ACCEL_RATE     0x46 /**< R/O: U8: actual accelerometer rate divided by 10 (multiples of 10 Hz) */
#define SR_ACTUAL_GYRO_RATE      0x47 /**< R/O: U8: actual gyroscope rate divided by 10 (multiples of 10 Hz) */
#define SR_ACTUAL_CUST_SENSOR0_RATE 0x48 /**< R/O: U8: actual sample rate of feature sensor 0 in Hz */
#define SR_ACTUAL_CUST_SENSOR1_RATE 0x49 /**< R/O: U8: actual sample rate of feature sensor 1 in Hz */
#define SR_ACTUAL_CUST_SENSOR2_RATE 0x4A /**< R/O: U8: actual sample rate of feature sensor 2 in Hz */

#define SR_ERROR_REGISTER        0x50 /**< RO; U8; enumeration; see RegErrorValues below */
#define SR_ALGO_CONTROL          0x54 /**< RW; U8; AKA standby control; bit flags; see RegAlgorithmControl below */
#define SR_MAG_RATE              0x55 /**< RW; U8; sample rate of magnetometer in Hz */
#define SR_ACCEL_RATE            0x56 /**< RW; U8; sample rate of accelerometer divided by 10 (multiples of 10 Hz) */
#define SR_GYRO_RATE             0x57 /**< RW; U8; sample rate of gyroscope divided by 10 (multiples of 10 Hz) */
#define SR_CUST_SENSOR0_RATE     0x58 /**< RW: U8; sample rate of feature sensor 0 in Hz */
#define SR_CUST_SENSOR1_RATE     0x59 /**< RW: U8; sample rate of feature sensor 1 in Hz */
#define SR_CUST_SENSOR2_RATE     0x5A /**< RW: U8; sample rate of feature sensor 2 in Hz */

#define SR_QUATERNION_RATE       SR_QRATE_DIVISOR
#define SR_CUST0_RATE            SR_CUST_SENSOR0_RATE
#define SR_CUST1_RATE            SR_CUST_SENSOR1_RATE
#define SR_CUST2_RATE            SR_CUST_SENSOR2_RATE



#define SR_LOAD_PARAM_B0         0x60 /**< R/W: U8/U16/U32: byte 0 of parameter to be set */
#define SR_LOAD_PARAM_B1         0x61 /**< R/W: U8: byte 1 of parameter */
#define SR_LOAD_PARAM_B2         0x62 /**< R/W: U8: byte 2 of parameter */
#define SR_LOAD_PARAM_B3         0x63 /**< R/W: U8: byte 3 of parameter */
#define SR_PARAMETER_REQUEST     0x64 /**< R/W: U8: parameter to access */

#define SR_ROM_VERSION           0x70 /**< RO; U32; Sentral ROM version (defined in hardware) */
#define SR_PRODUCT_ID            0x90 /**< RO; U8; Sentral Product ID */
#define SR_REVISION_ID           0x91 /**< RO; U8; Sentral Revision ID */

#define SR_UPLOAD_ADDR_H         0x94 /**< RW; U8; upper 5 bits of upload register's address; init to 0 when retrying */
#define SR_UPLOAD_ADDR_L         0x95 /**< RW; U8; lower 8 bits of upload register's address */
#define SR_UPLOAD_DATA           0x96 /**< RW; U8; a byte in the configuration file */
#define SR_CRC_HOST              0x97 /**< RO; U32; CRC-32 of previous host-uploaded configuration block as calculated by Sentral */

#define SR_RESET_REQ             0x9B /**< RW; U8; request a hard reset; bit flags; see RegResetRequest below */

#define SR_PASSTHRU_STATUS       0x9E /**< RO; U8; bit flags; see RegPassthruStatus below */
#define SR_SCL_LOW_CYCLES        0x9F /**< RW; U8; clock stretching control for passthru; enumeration; see RegSCLLowCyclesValues below */
#define SR_PASSTHRU_CONTROL      0xA0 /**< RW; U8; bit flags; see RegPassthruControl below */

/**
 * \brief Enabled events to the host.
 */
PREPACK typedef union MIDPACK RegEnableEvents
{
	// NOTE: This register needs to be cleared *after* an
	// I2C read to the register.
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 CPUResetRsvd:1;                                          /**<  bit 0: The CPU reset event is enabled */
		u8 Error:1;                                                 /**<  bit 1: The error event is enabled */
		u8 QuaternionResult:1;                                      /**<  bit 2: The quaternion event is enabled */
		u8 MagResult:1;                                             /**<  bit 3: The magnetometer event is enabled */
		u8 AccelResult:1;                                           /**<  bit 4: The accelerometer event is enabled */
		u8 GyroResult:1;                                            /**<  bit 5: The gyroscope event is enabled */
		u8 FeatureResults:1;                                        /**<  bit 6: The feature sensor event is enabled */
		u8 reserved:1;                                              /**<  bit 7: The reserved event is enabled */
	}
	bits;
}
RegEnableEvents;                                                  /**< typedef for storing Enable Events register values */
POSTPACK

/**
 * \brief Host writable register for setting hardware states.
 */
PREPACK typedef union MIDPACK RegHostControl
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 CPURunReq:1;                                             /**< bit 0: The host has requested that the firmware continue executing. Note: the CPU may not enter a halt state */
		u8 HostUpload:1;                                            /**< bit 1: The host has enabled upload of firmware directly to program RAM. */
	}
	bits;
}
RegHostControl;                                                   /**< typedef for storing Host Control register values */
POSTPACK

/**
 * \brief Triggered events.
 */
PREPACK typedef union MIDPACK RegEventStatus
{
	// NOTE: This register needs to be cleared *after* an
	// I2C read of the register.
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 CPUReset:1;                                              /**<  bit 0: The CPU reset event has triggered */
		u8 Error:1;                                                 /**<  bit 1: The error event has triggered */
		u8 QuaternionResult:1;                                      /**<  bit 2: The quaternion event has triggered */
		u8 MagResult:1;                                             /**<  bit 3: The magnetometer event has triggered */
		u8 AccelResult:1;                                           /**<  bit 4: The accelerometer event has triggered */
		u8 GyroResult:1;                                            /**<  bit 5: The gyroscope event has triggered */
		u8 FeatureResults:1;                                        /**<  bit 6: The feature sensor event has triggered */
		u8 reserved:1;                                              /**<  bit 7: The reserved event has triggered */
	}
	bits;
}
RegEventStatus;                                                   /**< typedef for storing Event Status register values */
POSTPACK

/**
 * \brief Current sensor states.
 */
PREPACK typedef union MIDPACK RegSensorStatus
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 MagNACK:1;                                               /**<  bit 0: The magnetometer has failed to ACK */
		u8 AccelNACK:1;                                             /**<  bit 1: The accelerometer has failed to ACK */
		u8 GyroNACK:1;                                              /**<  bit 2: The gyroscope has failed to ACK */
		u8 ReserveNACK:1;                                           /**<  bit 3: The reserved sensor has failed to ACK */
		u8 MagDeviceIDErr:1;                                        /**<  bit 4: The magnetometer has the incorrect ID */
		u8 AccelDeviceIDErr:1;                                      /**<  bit 5: The accelerometer has the incorrect ID */
		u8 GyroDeviceIDErr:1;                                       /**<  bit 6: The gyroscope has the incorrect ID */
		u8 ReservedDeviceIDErr:1;                                   /**<  bit 7: The reserved sensor has the incorrect ID */
	}
	bits;
}
RegSensorStatus;                                                  /**< typedef for storing Sensor Status register values */
POSTPACK

/**
 * \brief Sentral state.
 */
PREPACK typedef union MIDPACK RegSentralStatus
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 EEPROM:1;                                                /**<  bit 0: EEPROM with firmware has been detected */
		u8 EEUploadDone:1;                                          /**<  bit 1: EEPROM upload has compelted */
		u8 EEUploadError:1;                                         /**<  bit 2: EEPROM upload has failed */
		u8 StandbySt:1;                                             /**<  bit 3: The system is in standby */
		u8 NoEEPROM:1;                                              /**<  bit 4: no eeprom was detected */
	}
	bits;
}
RegSentralStatus;                                                 /**< typedef for storing Sentral Status register values */
POSTPACK

/**
 * \brief Algorithm status.
 */
PREPACK typedef union MIDPACK RegAlgorithmStatus
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 Halted:1;                                                /**<  bit 0: algorithm stopped and all sensors shut down */
		u8 AlgSlowdown:1;                                           /**<  bit 1: algorithm slowdown */
		u8 AlgStill:1;                                              /**<  bit 2: algorithm still */
		u8 CalibStable:1;                                           /**<  bit 3: calibration stable */
		u8 MagTransient:1;                                          /**<  bit 4: magnetic transient */
		u8 UnreliableSensorData:1;                                  /**<  bit 5: unreliable sensor data */
	}
	bits;
}
RegAlgorithmStatus;                                               /**< typedef for storing Algorithm status register values */
POSTPACK

/**
 * \brief Feature flags.
 */
PREPACK typedef union MIDPACK RegFeatureFlags
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 BaromInstalled:1;                                        /**< bit 0: a barometer sensor is present */
		u8 HumidityInstalled:1;                                     /**< bit 1: a humidity sensor is present */
		u8 TemperatureInstalled:1;                                  /**< bit 2: a temperature sensor is present */
		u8 CustomSensor0Installed:1;                                /**< bit 3: custom sensor 0 is installed */
		u8 CustomSensor1Installed:1;                                /**< bit 4: custom sensor 1 is installed */
		u8 CustomSensor2Installed:1;                                /**< bit 5: custom sensor 2 is installed */
		u8 reserved:2;                                              /**< bit 6-7: reserved */
	}
	bits;
}
RegFeatureFlags;                                                  /**< typedef for storing Feature Flags register values */
POSTPACK

/**
 * \brief Error register values.
 * NOTE: it is possible to receive values not in this
 * enumeration; these are classified as internal errors
 * requiring contacting EM Microelectronic for assistance.
 */
typedef enum RegErrorValues
{
	RE_NO_ERROR = 0x00,                                               /**< no error has occurred */
	RE_INVALID_SAMPLE_RATE = 0x80,                                    /**< check sensor rate settings */
	RE_MATH_ERROR = 0x30,                                             /**< check for software updates */
	RE_MAG_INIT_FAILED = 0x21,                                        /**< bad sensor physical connection, wrong driver, or incorrect I2C address */
	RE_ACCEL_INIT_FAILED = 0x22,                                      /**< bad sensor physical connection, wrong driver, or incorrect I2C address */
	RE_GYRO_INIT_FAILED = 0x24,                                       /**< bad sensor physical connection, wrong driver, or incorrect I2C address */
	RE_MAG_RATE_FAILURE = 0x11,                                       /**< sensor is unreliable and stops producing data */
	RE_ACCEL_RATE_FAILURE = 0x12,                                     /**< sensor is unreliable and stops producing data */
	RE_GYRO_RATE_FAILURE = 0x14,                                      /**< sensor is unreliable and stops producing data */
}
RegErrorValues;                                                      /**< typedef for storing Error register values */



/**
 * \brief Algorithm control.
 */
PREPACK typedef union MIDPACK RegAlgorithmControl
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 RequestHalt:1;                                           /**< bit 0: ask Sentral to stop the algorithm and shutdown sensors */
		u8 RawDataOutput:1;                                         /**< bit 1: output raw, uncalibated, unscaled sensor data */
		u8 HPROutput:1;                                             /**< bit 2: output heading, pitch, and roll in QX, QY, QZ instead of quaternion */
		u8 AccelGyro6DOF:1;                                         /**< bit 3: enable 6DOF accel / gyro fusion; must be done when in shutdown */
		u8 AccelMag6DOF:1;                                          /**< bit 4: enable 6DOF accel / mag fusion; must be done when in shutdown; not yet implemented in Sentral */
		u8 ENU: 1;                                                  /**< bit 5: enable ENU instead of NED coordinates */
		u8 EnhancedStillMode : 1;                                   /**< bit 6: disable the gyro during still mode in addition to disabling the magnetometer to save additional power */
		u8 ParameterProcedure:1;                                    /**< bit 7: set this to load or retrieve parameters */
	}
	bits;
}
RegAlgorithmControl;                                              /**< typedef for storing Algorithm Control register values */
POSTPACK

/**
 * \brief Reset request flags.
 */
PREPACK typedef union MIDPACK RegResetRequest
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 ResetRequest:1;                                          /**< bit 0: reset the chip; mimics a full hardware reset */
	}
	bits;
}
RegResetRequest;                                                  /**< typedef for storing Reset Request register values */
POSTPACK

/**
 * \brief I2C slave bus to master bus passthrough status.
 */
PREPACK typedef union MIDPACK RegPassthruStatus
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 PassthruReady:1;                                         /**< bit 0: passthrough state is enabled / ready */
	}
	bits;
}
RegPassthruStatus;                                                /**< typedef for storing Passthru Status register values */
POSTPACK

/**
 * \brief Suggested passthrough clock stretch values.
 */
typedef enum RegSCLLowCyclesValues
{
	/** \brief */
	RS_NONE = 0x00,                                                   /**< no clock stretching*/
	RS_STANDARD_I2C_CYCLES = 46,                                      /**< rate for 100KHz I2C slaves */
	RS_FAST_I2C_CYCLES = 12,                                          /**< rate for 400KHz I2C slaves */
	RS_HIGHSPEED_I2C_CYCLES = 4,                                      /**< rate for 1MHz I2C slaves */
}
RegSCLLowCyclesValues;                                               /**< typedef for storing SCL Low Cycles values */

/**
 * \brief Passthrough control.
 */
PREPACK typedef union MIDPACK RegPassthruControl
{
	/**
	 * \brief Direct access to the complete 8bit register
	 */
	u8 reg;
	/**
	 * \brief Access to individual bits in the register.
	 */
	struct
	{
		u8 PassthroughEn:1;                                         /**< bit 0: enable sensor to host connection */
		u8 ClockStretchEn:1;                                        /**< bit 1: enable clock stretching */
	}
	bits;
}
RegPassthruControl;                                               /**< typedef for storing Passthru Control register values */
POSTPACK

#endif
