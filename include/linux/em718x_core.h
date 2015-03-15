/**
 * \file
 *
 * \authors   Pete Skeggs <pete.skeggs@emmicro-us.com>
 *
 * \brief     abstract interface to the core portions of the generic host driver,
 *            with entry points to manage all aspects of detecting, configuring,
 *            and transferring data.
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

/** \defgroup Driver_Core Core Generic host driver abstract interface
 *  \brief    This defines the Generic host driver abstract interface, with
 *         entry points to manage all aspects of detecting, configuring, and
 *         transferring data with Sentral.
 *  @{
 */

#ifndef _DRIVER_CORE_H_
#define _DRIVER_CORE_H_

#include "em718x_host.h"
#include "em718x_registers.h"
#include "em718x_paramio.h"
#include "em718x_eeprom.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/**\brief the driver version numbers match the Sentral firmware version numbers they are intended to match */
#define DRIVER_MAJOR_VERSION 1 /**< major version number; change this to match Sentral major version, which changes when features change drastically */
#define DRIVER_MINOR_VERSION 4 /**< minor version number; change this to match Sentral minor version, which changes when adding new features or modifying their behavior */
#define DRIVER_BUILD 3981      /**< build; change this to match Sentral RAM version this driver; probably works with other versions */

/** \brief an enumeration of driver-produced error codes */
	typedef enum DI_ERROR_CODE
	{
		DE_NONE,                                                       // 0
		DE_INVALID_INSTANCE,                                           // 1
		DE_INITIALIZATION_REQUIRED,                                    // 2
		DE_INVALID_PARAMETERS,                                         // 3
		DE_DETECTION_REQUIRED,                                         // 4
		DE_EXECUTING_REQUIRED,                                         // 5
		DE_SENSING_REQUIRED,                                           // 6
		DE_SENSOR_NOT_PRESENT,                                         // 7
		DE_RATE_NOT_SETTABLE,                                          // 8
		DE_GYRO_RATE_NEEDED,                                           // 9
		DE_TOO_MANY_FEATURE_SENSORS,                                   // 10
		DE_RATE_TOO_HIGH,                                              // 11
		DE_I2C_PENDING,                                                // 12
		DE_I2C_ERROR,                                                  // 13
		DE_I2C_PASSTHRU_ERROR,                                         // 14
		DE_FILE_OPEN_ERROR,                                            // 15
		DE_FILE_READ_ERROR,                                            // 16
		DE_INVALID_FILE_HEADER,                                        // 17
		DE_INCORRECT_ROM_VERSION,                                      // 18
		DE_INVALID_FILE_LENGTH,                                        // 19
		DE_INVALID_CRC,                                                // 20
		DE_UNEXPECTED_RESET,                                           // 21
		DE_EM718X_ERROR,                                               // 22
		DE_MISSING_SENSOR,                                             // 23
		DE_UNKNOWN_SENSOR,                                             // 24
		DE_PARAM_OUT_OF_RANGE,                                         // 25
		DE_PARAM_ACK_TIMEOUT,                                          // 26
		DE_SENSOR_SAMPLE_TIMEOUT,                                      // 27
		DE_PASSTHROUGH_TIMEOUT,                                        // 28
	} DI_ERROR_CODE_T;

/** \brief a structure containing all known information about
 *         an error */
	struct DI_ERROR
	{
		RegEventStatus event_status;                                   /**< a copy of the SR_EVENT_STATUS register */
		RegSensorStatus sensor_status;                                 /**< a copy of the SR_SENSOR_STATUS register */
		RegSentralStatus sentral_status;                               /**< a copy of the SR_EM718X_STATUS register */
		RegAlgorithmStatus algo_status;                                /**< a copy of the SR_ALGO_STATUS register */
		RegErrorValues error_register;                                 /**< a copy of the SR_ERROR_REGISTER register */
		DI_ERROR_CODE_T driver_error;                                  /**< error code specific to this driver (not generated by Sentral) */
		u16 driver_error_aux;                                          /**< additional information about a driver error */
		const char *driver_error_func;                                 /**< pointer to name of function in which the error occurred */
	};
/** \brief a typedef wrapping the DI_ERROR structure */
	typedef struct DI_ERROR DI_ERROR_T;

/** \brief an enumeration of unique identifiers representing
 *         possible supported sensors connected to Sentral */
	typedef enum DI_SENSOR_TYPE
	{
		DST_NONE,
		DST_ACCEL,
		DST_QUATERNION,
		DST_MAG,
		DST_GYRO,
		DST_BAROM,
		DST_HUMID,
		DST_TEMP,
		DST_CUST0,
		DST_CUST1,
		DST_CUST2,
		DST_NUM_SENSOR_TYPES,
		DST_ALL,                                                 // special value for signaling need to read all sensors

		DST_FIRST=DST_ACCEL,
		DST_PEDO=DST_CUST0,
		DST_STILL=DST_CUST1

	} DI_SENSOR_TYPE_T;

#define SENSOR_SCALE_TIME_NUM 15625                            /**< convert Sentral Timestamp to time in microseconds (numerator) */
#define SENSOR_SCALE_TIME_DEN 512                              /**< convert Sentral Timestamp to time in microseconds (denomenator) */
#define SENSOR_TIMESTAMP_OVERFLOW_US 2000000ul                 /**< amount of time between Sentral sensor timestamp overflows */

/** \brief maximum number allowed */
#define MAX_FEATURE_SENSORS 3

/** \brief a data structure containing all information for a
 *         quaternion data sample */
	struct DI_QUATERNION_INT
	{
		u32 x;                                                         /**<  the x value (these 4 are actually floats, but treat it as a 32 bit int for now */
		u32 y;                                                         /**<  the y value */
		u32 z;                                                         /**<  the z value */
		u32 w;                                                         /**<  the w value */
		u32 t;                                                         /**<  the sample timestamp, in microseconds, range 0 to 32 bit overflow (71.58 minutes) */
		bool hpr;                                                      /**<  a flag to indicate that the heading/pitch/roll is stored in x, y, and z, with w unused; false if this is really a quaternion */
		bool valid;                                                    /**<  true if this sample is valid */
	};
/** \brief a typedef wrapping the DI_QUATERNION_INT structure */
	typedef struct DI_QUATERNION_INT DI_QUATERNION_INT_T;

/** \brief a structure representing a sample from a 3 axis
 *         sensor */
	struct DI_3AXIS_INT_DATA
	{
		s16 x;                                                         /**<  the x value */
		s16 y;                                                         /**<  the y value */
		s16 z;                                                         /**<  the z value */
		u32 t;                                                         /**<  the sample timestamp, in microseconds, range 0 to 32 bit overflow (71.58 minutes) */
		bool valid;                                                    /**<  true if this sample is valid */
	};
/** \brief a typedef wrapping the DI_3AXIS_INT_DATA structure */
	typedef struct DI_3AXIS_INT_DATA DI_3AXIS_INT_DATA_T;

/** \brief a structure representing a sample from a feature sensor */
	struct DI_FEATURE_DATA
	{
		u16 data;                                                      /**<  the value */
		u32 t;                                                         /**<  the sample timestamp, in microseconds, range 0 to 32 bit overflow (71.58 minutes) */
		bool valid;                                                    /**<  true if this sample is valid */
	};
/** \brief a typedef wrapping the DI_FEATURE_DATA structure */
	typedef struct DI_FEATURE_DATA DI_FEATURE_DATA_T;

/** \brief a structure containing all possible data samples
 *         available from an interrupt */
	struct DI_SENSOR_INT_DATA
	{
		DI_QUATERNION_INT_T quaternion;                                /**<  the quaternion sample */
		DI_3AXIS_INT_DATA_T mag;                                       /**<  the magnetometer sample */
		DI_3AXIS_INT_DATA_T accel;                                     /**<  the accelerometer sample */
		DI_3AXIS_INT_DATA_T gyro;                                      /**<  the gyroscope sample */
		DI_FEATURE_DATA_T feature[MAX_FEATURE_SENSORS];                /**<  data for all feature sensors, if present */
	};
/** \brief a typedef wrapping the DI_SENSOR_DATA structure */
	typedef struct DI_SENSOR_INT_DATA DI_SENSOR_INT_DATA_T;

/** \brief state values for task loop */
	typedef enum DRIVER_STATE_ENUM
	{
		DS_IDLE,
		DS_QUERY,
		DS_WAIT_EVENT_STATUS,
		DS_READ_EACH_SENSOR,
		DS_WAIT_SENSOR_DATA,
		DS_READ_ALL_SENSORS,
		DS_WAIT_ALL_SENSOR_DATA,
	} DRIVER_STATE_T;



/** \brief a structure containing sensor-specific internal data */
	struct DI_SENSOR_INFO
	{
		const char* name;
		u8 enable_mask;                   /**<  ENABLE_EVENTS register bitmask for this sensor */
		u32 timestamp_base;               /**<  keep track of overflows of sensor timestamps (since they wrap every 2 seconds) */
		u32 timestamp_prev;               /**<  previous timestamp, uncorrected for overflows */
		u8  rate_reg;                     /**<  register used to set the rate */
		u8  actual_rate_reg;              /**<  register used to get the actual rate */
		u16 rate;                         /**<  most recent rate settings */
		u16 rate_min;                     /**<  minimum allowed rate */
		u16 rate_max;                     /**<  maximum allowed rate */
		u16 actual_rate;                  /**<  reported most recent rate settings */
		u32 power;                        /**<  bitmask of sensors requiring this sensor be on (including itself) */
		u32 deps;                         /**<  bitmaks of sensors that must be powered up when this one is */
		bool acquisition_enable;          /**<  enable flags for data acquisition */
		bool present;                     /**<  true if this sensor is available in this hardware */
	};
/** \brief a typedef wrapping the DI_SENSOR_INFO structure */
	typedef struct DI_SENSOR_INFO DI_SENSOR_INFO_T;


	struct DI_INSTANCE;

/** \brief a typedef wrapping the DI_INSTANCE structure */
	typedef struct DI_INSTANCE DI_INSTANCE_T;

/**
 * \brief data callback type
 * \param instance - the driver instance that generated the
 *                 callback
 * \param sensor - the sensor that was just updated
 * \param data - a pointer to the just-received set of data;
 * callback must copy this to its own local buffer and should
 * not retain the pointer as it may be invalid after the
 * callback returns
 * \param user_param - the value specified on the call to
 *                   di_register()
 */
	typedef bool (*DATA_CALLBACK)(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_INT_DATA_T *data, void *user_param);


/** \brief structure containing all items unique to an
 *         instance of the Sentral driver */
	struct DI_INSTANCE
	{
		I2C_HANDLE_T i2c_handle;                                       /**<  handle to the I2C bus and slave address for the Sentral chip */
		I2C_HANDLE_T eeprom_handle;                                    /**<  handle to the I2C bus and slave address for the Sentral chip */
#ifdef CONFIG_MANAGE_IRQ
		IRQ_HANDLE_T irq_handle;                                       /**<  handle to the IRQ connected to the Sentral data ready line */
#endif
		bool initialized;                                              /**<  a flag to indicate whether the driver is initialized */
		bool detected;                                                 /**<  a flag to indicate that detection has occurred, so rom_version is accurate */
		bool executing;                                                /**<  a flag to indicate the Sentral firmware is running */
		bool sensing;                                                  /**<  a flat to indicate the Sentral algorithm is running */
		bool passthrough;                                              /**<  passthrough is active */
		bool read_all_sensor_regs;                                     /**<  if true, reduce number of I2C transactions by reading all sensors at once; if false, read each one individually */

#ifdef CONFIG_MANAGE_IRQ
		VOLATILE ATOMIC_INT interrupt;                                 /**<  an atomic counter that increments when an interrupt occurs */
		VOLATILE u32 host_int_timestamp;                               /**<  time we received the host interrupt, in ms */
		VOLATILE ATOMIC_INT i2c_interrupt;                             /**<  an atomic counter that increments when an i2c interrupt occurs */
#endif
		u32 upload_ofs;                                                /**<  current offset into an upload to RAM or EEPROM */
		s32 upload_len;                                                /**<  total expected length for text section of firmware file */
		u32 header_crc;                                                /**<  CRC from header; stored for checking at end of upload to RAM */
		u16 rom_version;                                               /**<  rom version */
		u16 ram_version;                                               /**<  ram version */
		u8 product_id;                                                 /**<  product id */
		u8 revision_id;                                                /**<  revision id */
		RegAlgorithmControl algo_control;                              /**<  most recent control bits for algorithm and output format */
		RegEnableEvents events;                                        /**<  most recent event enable */
		u16 feature_events;                                            /**<  feature sensor flags; used to determine if event flag should be enabled or not */
		u8 num_feature_sensors;                                        /**<  the number of additional sensors installed */
		DI_SENSOR_TYPE_T feature_sensors[MAX_FEATURE_SENSORS];         /**<  table of which sensor types are associated with which of the 3 possible feature sensors */
		u8 num_sensors;                                                /**<  total number of sensors present */
		DI_SENSOR_INFO_T sensor_info[DST_NUM_SENSOR_TYPES];            /**<  internal data for each sensor */
		DI_SENSOR_INT_DATA_T last_sample;                              /**<  the most recently acquired samples from all sensors */
		DI_ERROR_T error;                                              /**<  most recent error-related registers */
		DATA_CALLBACK data_callback;                                   /**<  the user's callback function */
		void *user_param;                                              /**<  the user's parameter value */
		u8 sensor_reg;                                                 /**<  for debug, record the register we started to read asynchronously */
		u8 sensor_buf[0x32];                                           /**<  data buffer for sensor data acquisition */
		u16 sensor_len;                                                /**<  amount of data being acquired */
		u16 sensor_rlen;                                               /**<  amount of data acquired */
		TransferStatus sensor_complete;                                /**<  status if I2C transfer */
		DRIVER_STATE_T state;                                          /**<  state of task loop */
		DI_SENSOR_TYPE_T fastest_sensor;
		u32 rate_changed;                                              /**<  bitmask of sensors that have changed rates, forcing a request of 'actual_rate' on the next interrupt */
	};



/**
 * \brief request the start of a non-blocking I2C read
 * \param handle - the device to read from
 * \param reg_addr - which register address to write, then
 *                 perform an I2C RESTART, then read
 * \param buffer - where to store the data read
 * \param len - the number of bytes required
 * \return bool - false if parameters are invalid
 */
	extern bool i2c_blocking_read(I2C_HANDLE_T handle, u8 reg_addr, u8 *buffer, u16 len);

/**
 * \brief request the start of a non-blocking I2C write
 * \param handle - the device to write to
 * \param reg_addr - the register address to write first
 * \param buffer - the data to write after that
 * \param len - the number of bytes to write
 * \return bool - false if parameters are invalid
 */
	extern bool i2c_blocking_write(I2C_HANDLE_T handle, u8 reg_addr, u8 *buffer, u16 len);

/**
 * \brief request the start of a non-blocking I2C write
 * \param handle - the device to write to
 * \param wbuffer - the data to write after that
 * \param wlen - the number of bytes to write
 * \param rbuffer - the data to read after that
 * \param rlen - the number of bytes to read
 * \return bool - false if parameters are invalid
 */
	extern bool i2c_blocking_write_read(I2C_HANDLE_T handle, u8 *wbuffer, u16 wlen, u8 *rbuffer, u16 rlen);

/**
 * \brief read a line of text from a file
 * \param f - file handle
 * \param line - pointer to caller's buffer to store the line into
 * \param max_len - the size of the buffer
 * \return int number of characters read
 */

	extern int read_line(FILE_HANDLE_T f, char *line, int max_len);




/**
* \brief stores error information and logs it
* \param instance - the driver instance
* \param error - the error code
* \param line - the source code line number the error occurred on
* \param fn - the name of the function in which the error occurred
*/
	extern void record_error(DI_INSTANCE_T *instance, DI_ERROR_CODE_T error, int line, const char *fn);

/** \brief macro to simplify error reporting in the driver */
#define INSTANCE_ERROR(_x) record_error(instance, _x, __LINE__, __FUNCTION__)

/** \brief driver entry points */

/**
 * \brief initialize the driver
 * \param instance - pointer to an instance structure allocated
 *                   by the caller
 * \param i2c_handle - an abstract handle used to access the
 *                   Sentral chip via the host I2C services
 *                   (defined in host_services.h); the operating
 *                   system's driver configuration mechanism is
 *                   responsible for knowing how to reach the
 *                   Sentral chip (which bus and slave address,
 *                   etc.), and for initializing this interfce
 * \param irq_handle - an abstract handle used to represent the
 *                   data ready IRQ from the Sentral chip; it is
 *                   up to the operating system's driver
 *                   configuration mechanism to know how to
 *                   create this, and to initialize this interface
 * \param reset -    set TRUE to force reset of Sentral, FALSE
 *                   to leave it running as is
 * \return bool      true if successful
 */
	extern bool di_init_core(DI_INSTANCE_T *instance, I2C_HANDLE_T i2c_handle, IRQ_HANDLE_T irq_handle, bool reset);

/**
 * \brief shutdown the driver core
 * \param instance - which instance to close
 * \return bool - true if succeeded, false if the instance was
 *         never initialized
 */
	extern bool di_deinit_core(DI_INSTANCE_T *instance);

/**
 * \brief detect whether the Sentral cip is present, and obtain
 *        information about it; blocks
 * \param instance - the driver instance
 * \param product_id - pointer to an 8 bit integer to receive
 *                   the product ID register value
 * \param revision_id - pointer to an 8 bit register to receive
 *                    the revision ID register value
 * \param rom_version - pointer to a 16 bit integer to receive
 *                    the ROM version number
 * \param ram_version - pointer to a 16 bit integer to receive
 *                    the RAM version number (or 0 if not
 *                    supported)
 * \param eeprom_present - the pointer to a bool to receive a flag indicating
 *        whether an EEPROM was detected
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_detect_sentral(DI_INSTANCE_T *instance, u8 *product_id, u8 *revision_id, u16 *rom_version, u16 *ram_version, bool *eeprom_present);

#define di_detect_sentral_simple(instance ) di_detect_sentral(instance, NULL, NULL, NULL, NULL, NULL)

/**
 * \brief confirm the header is valid
 * \param header - pointer to the header structure to check
 * \param file_size - additional info useful for checking header; may be 0
 * \return bool - false if header is invalid
 */
	extern bool eeprom_check_header(EEPROMHeader *header, u16 file_size);

/**
 * \brief start uploading firmware to the Sentral chip's RAM
 * \param instance - the driver instance
 * \param header - pointer to EEPROM header structure from start of .fw file
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_upload_firmware_start(DI_INSTANCE_T *instance, EEPROMHeader *header);

/**
 * \brief submit bytes to upload firmware to the Sentral chip's RAM; can be
 *        called multiple times
 * \param instance - the driver instance
 * \param words - pointer to array of 32 bit words of data
 * \param len - number of words
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_upload_firmware_data(DI_INSTANCE_T *instance, u32 *words, u16 len);

/**
 * \brief finish uploading firmware to the Sentral chip's RAM
 * \param instance - the driver instance
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_upload_firmware_finish(DI_INSTANCE_T *instance);

/**
 * \brief start to upload firmware to the EEPROM attached to the Sentral
 *        chip; we check the header, but rather than striping it
 *        as we do when uploading to RAM, we pass it as is;
 *        blocks
 * \param instance - the driver instance
 * \param eeprom_handle - I2C handle (host-specific) to reach
 *                      the EEPROM at slave address 0x50
 *                      (NOTE: Sentral assumes this address as
 *                      well as a register layout the same as a
 *                      Microchip 24LC256 EEPROM)
 * \param header - pointer to header from start of .fw file
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_upload_eeprom_start(DI_INSTANCE_T *instance, I2C_HANDLE_T eeprom_handle, EEPROMHeader *header);

/**
 * \brief submit bytes to upload firmware to the EEPROM; can be called multiple
 *        times
 * \param instance - the driver instance
 * \param eeprom_handle - I2C handle (host-specific) to reach
 *                      the EEPROM at slave address 0x50
 *                      (NOTE: Sentral assumes this address as
 *                      well as a register layout the same as a
 *                      Microchip 24LC256 EEPROM)
 * \param words - pointer to array of 32 bit words of data
 * \param len - number of words
 * \return bool - false if the EEPROM could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_upload_eeprom_data(DI_INSTANCE_T *instance, I2C_HANDLE_T eeprom_handle, u32 *words, u16 len);


/**
 * \brief finish uploading firmware to the EEPROM
 * \param instance - the driver instance
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_upload_eeprom_finish(DI_INSTANCE_T *instance);


/**
 * \brief erase the EEPROM (render it not loadable by Sentral)
 * \param instance - the driver instance
 * \param eeprom_handle - I2C handle (host-specific) to reach
 *                      the EEPROM at slave address 0x50
 *                      (NOTE: Sentral assumes this address as
 *                      well as a register layout the same as a
 *                      Microchip 24LC256 EEPROM)
 * \return bool - false if Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_erase_eeprom(DI_INSTANCE_T *instance, I2C_HANDLE_T eeprom_handle);

/**
 * \brief query which sensors are present on Sentral; blocks
 * \param instance - the driver instance
 * \return bool false if Sentral could not be reached
 */
	extern bool di_query_features(DI_INSTANCE_T *instance);

/**
 * \brief determine if a specific sensor is supported by this hardware
 * \param instance - the driver instance
 * \param sensor - the sensor to check
 * \return bool - true if it is present
 */
	extern bool di_has_sensor(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor);

/**
 * \brief set the sample rate for a specific sensor; blocks
 * \param instance - the driver instance
 * \param sensor - the sensor type to configure
 * \param rate_hz - the rate in hz; 0 disables the sensor(?)
 * \return bool - false if the sensor is not present, the rate
 *         is invalid, or Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_configure_rate(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, u16 rate_hz);

	bool di_request_rate(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, int rate);


/**
 * \brief get the sample rate for a specific sensor; blocks
 * \param instance - the driver instance
 * \param sensor - the sensor type to configure
 * \param rate_hz - pointer to an unsigned 16 bit integer to
 *                which this function stores the actual rate in
 *                hz
 * \return bool - false if the sensor is not present, the rate
 *         is invalid, or Sentral could not be reached; call
 *         di_query_error() to determine the cause
 */
	extern bool di_query_actual_rate(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, u16 *rate_hz);

/**
 * \brief save (read) one or more parameters from Sentral
 * \param instance - the driver instance
 * \param param_numbers - array of predefined parameters to read
 *                     (may depend on exact firmware running in
 *                     Sentral); 0 to 127
 * \param num_params - number of parameters to read in one
 *                   sequence
 * \param params - pointer to an array of four 8 bit
 *                 integers PER num_params to which the param
 *                 values will be saved
 * \return bool - false if timed out waiting for param
 *         acknowledge, param number out of range, or Sentral
 *         could not be reached; call di_get_last_error() to
 *         determine the cause
 */
	extern bool di_save_parameters(DI_INSTANCE_T *instance, u8 *param_numbers, u8 num_params, u8 *params);

/**
 * \brief load (write) one or more parameters to Sentral
 * \param instance - the driver instance
 * \param param_numbers - array of predefined parameters to write
 *                     (may depend on exact firmware running in
 *                     Sentral); 0 to 127
 * \param num_params - number of parameters to write in one
 *                   sequence
 * \param params - pointer to an array of four 8 bit
 *                 integers PER num_params from which the param
 *                 values will be loaded
 * \return bool - false if timed out waiting for param
 *         acknowledge, param number out of range, or Sentral
 *         could not be reached; call di_get_last_error() to
 *         determine the cause
 */
	extern bool di_load_parameters(DI_INSTANCE_T *instance, u8 *param_numbers, u8 num_params, u8 *params);

/**
 * \brief set the output format for samples; blocks
 * \param instance - the driver instance
 * \param hpr_output - true to select heading/pitch/roll; false
 *                   to select quaternion
 * \param raw_data_output - true to select uncalibrated/unscaled
 *                        samples; false for calibrated/scaled
 * \param ag6dof - select 6 degree of freedom accel / gyro fusion (must be in
 *        shutdown by calling \ref di_shutdown_request)
 * \param am6dof - select 6 degree of freedom accel / mag fusion (must be in
 *        shutdown; this is here as a future-looking feature)
 * \param enu - select ENU instead of NED coordinate
 * \param enhanced_still_mode - disable the gyro in addition to the mag to save
 *        more power when still
 * \return bool - false if Sentral could not be reached
 */
	extern bool di_configure_output(DI_INSTANCE_T *instance, bool hpr_output, bool raw_data_output, bool ag6dof, bool am6dof, bool enu, bool enhanced_still_mode);

/**
 * \brief enable interrupts for a list of sensors; blocks
 * \param instance - the driver instance
 * \param sensors_to_enable - list of sensor IDs for all sensors
 *                          to enable
 * \param num_to_enable - the size of the sensors_to_enable
 *                      array
 * \return bool - false if the sensor is not present or Sentral
 *         could not be reached
 */
	extern bool di_enable_sensor_interrupts(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T *sensors_to_enable, u8 num_to_enable);

/**
 * \brief enable sensor data acquisition for a list of sensors by
 *        di_task_loop()
 * \param instance - the driver instance
 * \param sensors_to_enable - list of sensor IDs for all sensors
 *                          to enable
 * \param num_to_enable - the size of the sensors_to_enable
 *                      array
 * \return bool - false if the sensor is not present or Sentral
 *         could not be reached
 */
	extern bool di_set_sensors_to_acquire(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T *sensors_to_enable, u8 num_to_enable);

/**
 * \brief enable or disable sensor data acquisition for a single sensor
 * by di_task_loop()
 * \param instance - the driver instance
 * \param sensor - the sensor to enable or disable
 * \param enable - whether to acquire from this sensor or not
 * \return bool - false if the sensor is not present or Sentral
 *         could not be reached
 */
	extern bool di_enable_sensor_acquisition(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, bool enable);
	static inline bool	di_is_sensor_acquisition_enabled(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor) { return instance->sensor_info[sensor].acquisition_enable; }

/**
 * \brief register a callback
 * \param instance - the driver instance
 * \param data_callback - function to call when samples arrive
 * \param user_param - user parameter value to pass to callback
 * \return bool - false if instance invalid
 */
	extern bool di_register(DI_INSTANCE_T *instance, DATA_CALLBACK data_callback, void *user_param);

/**
 * \brief remove the callback
 * \param instance - the driver instance
 * \return bool - false if instance is invalid
 */
	extern bool di_deregister(DI_INSTANCE_T *instance);

/**
 * \brief start the firmware running in Sentral (issue a
 *        RunRequest to the Host Control register)
 * \param instance - the driver instance
 * \return bool - false if the instance is invalid or Sentral
 *         could not be reached
 */
	extern bool di_run_request(DI_INSTANCE_T *instance);

/**
 * \brief start the algorithm running; data samples will start
 *        being generated (issue a RequestHalt = 0 to the
 *        Algorithm Control register)
 * \param instance - the driver instance
 * \return bool - false if the instance is invalid or Sentral
 *         could not be reached
 */
	extern bool di_normal_exec_request(DI_INSTANCE_T *instance);

/**
 * \brief a function that the operating system must call
 *        periodically to handle driver data transfers;
 *        non-blocking
 * \param instance - the driver instance
 * \param done - a pointer to the caller's bool, which will be
 *             set true if we are done with our timeslice, or
 *             false if we'd like to be called again quickly if
 *             the host has nothing else to do
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached
 */
	extern bool di_task_loop(DI_INSTANCE_T *instance, bool *done);


	bool di_handle_interrupt(DI_INSTANCE_T *instance);

/**
 * \brief blocking call to get task loop to get to a point that
 *        other driver calls can be made again
 * \param instance - the driver instance
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached
 */
	extern bool di_pause_task_loop(DI_INSTANCE_T *instance);

/** \brief request a pause so that settings can be changed,
 *         passthrough can be used, etc.; call di_normal_exec_request() again
 *         to unpause
 *  \param instance - the driver instance
 *  \return bool - false if the instance is invalid or if
 *          Sentral could not be reached */
	extern bool di_standby_request(DI_INSTANCE_T *instance);

/** \brief request shutdown of all sensors and Sentral, to
 *         save power
 *  \param instance - the driver instance
 *  \return bool - false if the instance is invalid or if
 *          Sentral could not be reached */
	extern bool di_shutdown_request(DI_INSTANCE_T *instance);

/**
 * \brief file out status of Sentral
 * \param instance - the driver instance
 * \param executing - pointer to bool modified to reflect whether
 *             firmware is running or not
 * \param sensing - pointer to bool modified to reflect whether
 *             algorithm is running or not
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached
 */
	extern bool di_query_status(DI_INSTANCE_T *instance, bool *executing, bool *sensing);

/**
 * \brief find out information about an error
 * \param instance - the driver instance
 * \param error_info - pointer to an error structure allocated
 *                   by the caller to which the error
 *                   information will be stored
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached
 */
	extern bool di_query_error(DI_INSTANCE_T *instance, DI_ERROR_T *error_info);

/**
 *  \brief for debugging, read and return all registers in a
 *         range
 *  \param instance - the driver instance
 *  \param start_reg - the first to read
 *  \param end_reg - the last to read
 *  \param dest - array into which the values will be stored
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached; also returns false if transfer
 *         fails
*/
	extern bool di_read_registers(DI_INSTANCE_T *instance, u8 start_reg, u8 end_reg, u8 *dest);


/**
 *  \brief for debugging, write registers in a range
 *  \param instance - the driver instance
 *  \param start_reg - the first to write
 *  \param end_reg - the last to write
 *  \param src - array from which the values will be written
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached; also returns false if transfer
 *         fails
*/
	extern bool di_write_registers(DI_INSTANCE_T *instance, u8 start_reg, u8 end_reg, u8 *src);

/**
 * \brief control I2C passthrough mode
 * \param instance - the instance to control
 * \param enable - true to enable passthrough, false to disable
 * \return bool - false if parameters are invalid or Sentral
 *         could not be reached
 */
	extern bool di_set_passthrough_mode(DI_INSTANCE_T *instance, bool enable);

/**
 * \brief perform reset of Sentral
 * \param instance - the instance to reset
 * \param wait_for_standby
 * \return bool - false if parameters are invalid, or Sentral
 *         could not be reached
 */
	extern bool di_reset_sentral(DI_INSTANCE_T *instance, bool wait_for_standby);

/**
 * \brief read standard error registers from Sentral
 * \param instance - the instance to check
 * \return bool - false if parameters are invalid or Sentral
 *         could not be reached
 */
	extern bool read_sentral_error_registers(DI_INSTANCE_T *instance);

/**
 * \brief get last error
 * \param instance - the driver instance
 * \param error_info - pointer to an error structure allocated
 *                   by the caller to which the error
 *                   information will be stored
 * \return bool - false if the instance is invalid
 */
	extern bool di_get_last_error(DI_INSTANCE_T *instance, DI_ERROR_T *error_info);

/**
 * \brief convert a 16 bit timestamp register to time in
 *        microseconds
 * \param buf - pointer to the 2 bytes
 * \return u32 time in microseconds (wraps every 2 seconds)
 */
	extern u32 uint16_reg_to_microseconds(u8 *buf);
/**
 *  \brief start to read most recent value for sensor from
 *         registers; DOES NOT block
 *  \param instance - the driver instance
 *  \param sensor - the sensor type to query
 *  \param int_callback - true if int callback should be used
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached
*/
	extern bool di_query_sensor_start(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, bool int_callback);

/**
 *  \brief complete reading of sensor value from registers; DOES
 *         NOT block
 *  \param instance - the driver instance
 *  \param sensor - the sensor type to query
 *  \param data - pointer to caller's DI_SENSOR_INT_DATA_T buffer in which the
 *              sensor data will be returned
 *  \param force - force update, even if sensor isn't enabled
 * \return bool - false if the instance is invalid or if Sentral
 *         could not be reached; also returns false if transfer
 *         is incomplete -- check driver_error ==
 *         DE_I2C_PENDING
*/
	extern bool di_query_sensor_finish(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_INT_DATA_T *data, bool force);



	bool di_get_sensor_value( DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_INT_DATA_T * value );


// may not be necessary to make these public...
/** \brief define the driver's instance of its IRQ_CALLBACK */
	extern IRQ_CALLBACK di_irq_callback;
/** \brief define the driver's instance of its I2C_CALLBACK */
	extern I2C_CALLBACK di_i2c_callback;



#ifdef __cplusplus
}
#endif

#endif

/** @}*/
