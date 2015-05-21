/**
 * \file
 *
 * \authors   Pete Skeggs <pete.skeggs@emmicro-us.com>
 *
 * \brief     the core of the generic host driver, with entry points to manage all
 *            aspects of detecting, configuring, and transferring data with Sentral
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

#define DEBUG 1



#ifndef __KERNEL__
	#include <stdio.h>
	#include <string.h> // will need conditionals to offer cross-platform version
#else
	#include <linux/string.h>
	#include <linux/io.h>
#endif

#include <linux/em718x_core.h>
#include <linux/em718x_eeprom.h>

#ifndef __KERNEL__
#include "driver_util.h"
#endif


#define SENSOR_ACCEL_RATE_MIN		10
#define SENSOR_ACCEL_RATE_MAX		1000
#define SENSOR_GYRO_RATE_MIN		10
#define SENSOR_GYRO_RATE_MAX		1000
#define SENSOR_MAG_RATE_MIN			1
#define SENSOR_MAG_RATE_MAX			100
#define SENSOR_QUATERNION_RATE_MIN		10
#define SENSOR_QUATERNION_RATE_MAX		1000
#define SENSOR_QUATERNION_ACCEL_RATE_MIN 100
#define SENSOR_QUATERNION_GYRO_RATE_MIN 100
#define SENSOR_QUATERNION_MAG_RATE_MIN 30
#define SENSOR_CUST0_RATE_MIN		30
#define SENSOR_CUST0_RATE_MAX		100
#define SENSOR_CUST0_ACCEL_RATE_MIN	30
#define SENSOR_CUST1_RATE_MIN		30
#define SENSOR_CUST1_RATE_MAX		100
#define SENSOR_CUST1_ACCEL_RATE_MIN	30

#define SENSOR_ACCEL_NAME			"accel"
#define SENSOR_GYRO_NAME			"gyro"
#define SENSOR_MAG_NAME				"mag"
#define SENSOR_QUATERNION_NAME		"quat"
#define SENSOR_CUST0_NAME			"pedo"
#define SENSOR_CUST1_NAME			"tilt"
#define SENSOR_CUST2_NAME			"na"


/** \addtogroup Driver_Core
 *  @{
 */

#define RAM_BUF_LEN 64                 /**< number of bytes to write to Sentral RAM at a time; allocated on the stack */
#define EEPROM_BUF_LEN 64              /**< number of bytes to write to EEPROM at a time; allocated on the stack */
#define PARAM_ACK_TIMEOUT_MS 1000      /**< timeout value for acknowledgement from Sentral for parameter request; this could be tighter, but will at least stop utter paralysis */
#define EEPROM_WRITE_TIMEOUT_MS 10     /**< timeout value for writing a byte or page to the EEPROM */
#define RUN_TIMEOUT_MS 1000            /**< timeout value for algorithm start */
#define PAUSE_TIMEOUT_MS 1000          /**< timeout value for algorithm pause */
#define QUERY_SENSOR_TIMEOUT_MS 1000   /**< timeout value for querying a sensor */
#define SHUTDOWN_TIMEOUT_MS 2000       /**< timeout value for shutting down */
#define EEPROM_UPLOAD_TIME_MS 4000     /**< maximum expected time to upload an EEPROM, in milliseconds */
#define PASSTHROUGH_TIMEOUT_MS 1000    /**< maximum time to wait to enter passthrough mode */
//#define DEBUG_EVENTS                 /**< uncomment this to generate a circular buffer of sensor event history for debugging */
//#define RESET_EM718X_BEFORE_UPLOAD  /**< force a reset of Sentral before doing an upload to RAM */




#define DI_PRECHECK(x) ((x) && (x)->initialized) /**< this checks that the handle is valid */
#define DI_PRECHECK_FULL(x) ((x) && (x)->initialized && (x)->detected) /**< this checks that the handle is valid and we've confirmed that Sentral is accessible */
#define DI_PREAMBLE(x) if (!DI_PRECHECK(x)) { INSTANCE_ERROR(DE_INVALID_INSTANCE); return FALSE; }
#define DI_PREAMBLE_FULL(x) if (!DI_PRECHECK_FULL(x)) { INSTANCE_ERROR(DE_INVALID_INSTANCE); return FALSE; }

#if defined(DEBUG_EVENTS)
#define EVENT_COUNT 32                                               /**< maximum amount of history to gather (new events overwrite oldest) */
u8 event_hist[EVENT_COUNT];                                          /**< list of event status register values at each interrupt */
u32 event_time[EVENT_COUNT];                                         /**< system time in milliseconds that this was read */
u8 event_index = 0;                                                  /**< index into history at which the next event will be stored */
#endif

#ifdef CONFIG_MANAGE_IRQ
	#define IRQ_CHECK(di)		irq_check(di->irq_handle)
#else
	#define IRQ_CHECK(di)
#endif

//u16 Asus_feature_events = 0;

static bool di_request_power(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_TYPE_T requestor, bool enable);



/**
	Get the rate min/max for the specified @sensor given the specified @power state
*/
static bool  di_sensor_get_rate_range(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, u32 power,
										int * rate_min, int * rate_max)
{
	int min = instance->sensor_info[sensor].rate_min;
	int max = instance->sensor_info[sensor].rate_max;

	switch (sensor) {
	case DST_ACCEL:
		if (power & BIT(DST_QUATERNION))
			min = SENSOR_QUATERNION_ACCEL_RATE_MIN;
		else if (power & BIT(DST_CUST0))
			min = SENSOR_CUST0_ACCEL_RATE_MIN;
		else if (power & BIT(DST_CUST1))
			min = SENSOR_CUST1_ACCEL_RATE_MIN;
		break;

	case DST_GYRO:
		if (power & BIT(DST_QUATERNION))
				min = SENSOR_QUATERNION_GYRO_RATE_MIN;
		break;

	case DST_MAG:
		if (power & BIT(DST_QUATERNION))
			min = SENSOR_QUATERNION_MAG_RATE_MIN;
		break;

	default:
		break;
	}


	if (rate_min)
		*rate_min = min;

	if (rate_max)
		*rate_max = max;

	debug_log("sensor %s power %08X rate_min: %d rate_max:%d\n", instance->sensor_info[sensor].name, power, min, max);

	return TRUE;
}


int di_scale_rate(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, int rate_hz)
{
	switch (sensor)
	{
	case DST_ACCEL:
	case DST_GYRO:
	case DST_CUST0:
		return rate_hz / 10;

	case DST_QUATERNION:
		/* this should not be a problem, as the rate is only scaled if the sensor is on, the sensor will only be on if the gyro is on,
			the gyro will have a valid rate (perhaps minimal) before it's turned on */
		if (!instance->sensor_info[DST_GYRO].rate)
		{
			INSTANCE_ERROR(DE_GYRO_RATE_NEEDED);
			return -1;
		}
		if (!rate_hz)
			return 0;
		else
		{
			int val = instance->sensor_info[DST_GYRO].rate / rate_hz;
			if (val)
				return val-1;
		}
		break;
	default: ;
	}

	return rate_hz;
}




static bool di_set_power_state( DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, u32 power )
{
	DI_SENSOR_INFO_T * s = &instance->sensor_info[sensor];
	int i;
	u8 hw_rate = 0;
	u16 fastest_rate = 0;
	DI_SENSOR_TYPE_T fastest_sensor = DST_NONE;
	u8 Asus_tilt_threshold = 10;

	debug_log("sensor %s: set_power_state: power:%08X\n", s->name, power);

	if (power) {
		int rate;
		int rate_min = 0, rate_max = 0;
		int scaled_rate = 0;

		rate = s->rate;

		debug_log("sensor %s: powered on, checking set rate of %d hz", s->name, rate);

		if (!di_sensor_get_rate_range( instance, sensor, power, &rate_min, &rate_max))
			return FALSE;

		if (rate<rate_min)
			rate = rate_min;
		else if(rate > rate_max)
			rate = rate_max;

		debug_log("sensor %s: setting rate to %d hz", s->name, rate);

		scaled_rate = di_scale_rate( instance, sensor, rate );

		if (scaled_rate < 0) {
			error_log("error scaling rate %d for sensor %s", rate, s->name);
			return FALSE;
		}

		s->rate = rate;
		hw_rate = scaled_rate;

		if ((sensor == DST_CUST0) || (sensor == DST_CUST1))
			hw_rate |= 0x80;
	}

	if (power) {
		debug_log("sensor %s: setting rate register to %d\n", s->name, hw_rate );

		if (!i2c_blocking_write(instance->i2c_handle, s->rate_reg, &hw_rate, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
	}

	s->power = power;

	/* rate changed, recalculate fastest sensor */
	fastest_sensor = DST_NONE;
	fastest_rate = 0;

	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++)
	{
		if (!instance->sensor_info[i].power)
			continue;

		if (instance->sensor_info[i].rate > fastest_rate)
		{
			fastest_sensor = i;
			fastest_rate = instance->sensor_info[i].rate;
		}
	}

/*
	if ((Asus_feature_events & 0x00FE) == 0) {
		if ((Asus_feature_events & 0x100)) {
			fastest_sensor = 8;
			fastest_rate = instance->sensor_info[8].rate;
		} else if (Asus_feature_events & 0x200) {
			fastest_sensor = 9;
			fastest_rate = instance->sensor_info[9].rate;
		}
	}
*/
	if ( fastest_sensor != instance->fastest_sensor ) {
		int count = 0;

		if (fastest_sensor != DST_NONE)
			count = 1;

		info_log("recalculated fastest sensor %s: %d hz\n", instance->sensor_info[fastest_sensor].name, fastest_rate);

		if (!di_enable_sensor_interrupts(instance, &fastest_sensor, count))
			return FALSE;

		instance->fastest_sensor = fastest_sensor;
	} else {
		debug_log("fastest sensor still %s\n", instance->sensor_info[fastest_sensor].name);
	}

	// ASUS_BSP+++Evan : add threshoult setting of PNI tilt
	// ASUS will use register 0x59 for tilt angle threshold input.
	// There is no scaling required for the angle input and input range is 5 degrees to 85 degrees.
	// For example if you write 30 to 0x59, the tilt threshold is 30 degrees.
	if (sensor == DST_CUST1) {
		if (!i2c_blocking_write(instance->i2c_handle, 0x59, &Asus_tilt_threshold, 1)) {
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
	}
	// ASUS_BSP---Evan : add threshoult setting of PNI tilt

	return TRUE;
}

static bool di_request_deps_power(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, bool enable )
{
	int i;

	u32 deps = instance->sensor_info[sensor].deps;

	if (!deps)
		return TRUE;

	debug_log("sensor %s: request deps 0x%08X power\n", instance->sensor_info[sensor].name, deps);

	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++)
	{
		if (i == sensor)
			continue;

		if ( deps & BIT(i) ) {
			if (!di_request_power( instance, i, sensor, enable )) {
				error_log("failed to request power %d for sensor %d dependency %d\n",
							enable, sensor, i);
				return FALSE;
			}
		}
	}
	return TRUE;
}

static bool di_request_power(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_TYPE_T requestor, bool enable)
{
	DI_SENSOR_INFO_T * s = &instance->sensor_info[sensor];
	u32 power = s->power;

	debug_log("sensor %s: request power %d", s->name, enable);

	/*	set or clear the bit for the requestor. For example, when powering on the quaternion, on the first pass through
		di_request_power(quaternion, quaternion) where sensor=quaternion, requestor=quaternion. The quaternion dependencies will then be evaluated,
		and di_request_power(accel, quaternion) will be called. This will cause the 'quaternion' bit to be set on the accels 'power'
		setting. NOTE: the accels, 'accel' bit will NOT be set, unless di_request_power() is explicitly called to enable the
		accel, eg di_request_power(accel, accel).
		*/
	if (enable)
		power |= BIT(requestor);
	else
		power &= ~BIT(requestor);

	if (power == s->power) {
		debug_log("sensor %s: power unchanged\n", s->name);
		return TRUE;
	}

	/* power changed */
	if ( (power && !s->power) ||
		(!power && s->power) ) {
		debug_log("sensor %s: powering %s", s->name, ( (power) ? "on" : "off") );
		if (!di_request_deps_power( instance, sensor, (power>0) )) {
			error_log("sensor %d: failed to set dependency power\n", sensor);
			return FALSE;
		}
	}

	debug_log("sensor %s: power changed %x=>%x\n", s->name, s->power, power);
	/* power up or power changed. (try to) restore the current rate */
	if (!di_set_power_state( instance, sensor, power ))
		return FALSE;

	return TRUE;
}

bool di_request_deps_rate( DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, int rate )
{
	int i;

	u32 deps = instance->sensor_info[sensor].deps;

	if (!deps)
		return TRUE;

	debug_log("sensor %s: request deps 0x%08X rate %d\n", instance->sensor_info[sensor].name, deps, rate);

	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++)
	{
		if (i==sensor)
			continue;

		if ( deps & BIT(i) ) {
			if (!di_request_rate( instance, i, rate )) {
				error_log("failed to request rate %d for sensor %d dependency %d\n",
							rate, sensor, i);
				return FALSE;
			}
		}
	}
	return TRUE;

}

/** Set the rate for @sensor. if its power state is 0, the rate will be stored locally only. Otherwise, the hardware
	will be sent the new rate
*/
bool di_request_rate(DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, int rate)
{
	DI_SENSOR_INFO_T * s = &instance->sensor_info[sensor];
	int rate_min = 0, rate_max = 0;

	info_log("sensor %s requested rate %d:\n", s->name, rate );

	if (!di_sensor_get_rate_range( instance, sensor, s->power, &rate_min, &rate_max))
		return FALSE;

	if (rate<rate_min)
		rate = rate_min;
	else if (rate > rate_max)
		rate = rate_max;

	s->rate = rate;

	info_log("sensor %s updated rate %d:\n", s->name, rate );
	/* if powered on, the update hardware rate */
	if ( s->power )
		return di_set_power_state( instance, sensor, s->power );

	return TRUE;
}




bool di_init_core(DI_INSTANCE_T *instance, I2C_HANDLE_T i2c_handle, IRQ_HANDLE_T irq_handle, bool reset)
{
	if (!i2c_handle )
	{
		INSTANCE_ERROR(DE_INVALID_PARAMETERS);
		goto error_exit;
	}

	instance->i2c_handle = i2c_handle;

#ifdef CONFIG_MANAGE_IRQ
	if(!irq_handle)
	{
		INSTANCE_ERROR(DE_INVALID_PARAMETERS);
		goto error_exit;
	}

	instance->irq_handle = irq_handle;
#endif


	instance->state = DS_IDLE;
	instance->initialized = TRUE;
	INSTANCE_ERROR(DE_NONE);
	if (reset)
	{
		if (!di_reset_sentral(instance, TRUE))
			goto error_exit;
	}
	else
	{
		if (!read_sentral_error_registers(instance))
			goto error_exit;
	}
	return TRUE;

error_exit:
	return FALSE;
}


bool di_deinit_core(DI_INSTANCE_T *instance)
{
	if (!DI_PRECHECK(instance))
		return FALSE;

	// stop Sentral
	di_shutdown_request(instance);

	// release resources
	if (instance->i2c_handle)
	{
		i2c_deregister(instance->i2c_handle);
		instance->i2c_handle = NULL;
	}

#ifdef CONFIG_MANAGE_IRQ
	if (instance->irq_handle)
	{
		irq_deregister(instance->irq_handle);
		instance->irq_handle = NULL;
	}
#endif

	instance->initialized = FALSE;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_detect_sentral(DI_INSTANCE_T *instance, u8 *product_id, u8 *revision_id, u16 *rom_version, u16 *ram_version, bool *eeprom_present)
{
	u8 buf[2];
	u8 version[4];

	DI_PREAMBLE(instance);

	// read informational registers that are present even without uploaded RAM
	if (!i2c_blocking_read(instance->i2c_handle, SR_PRODUCT_ID, buf, 2))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	instance->product_id = buf[0];
	instance->revision_id = buf[1];
	if (!i2c_blocking_read(instance->i2c_handle, SR_ROM_VERSION, version, 4))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	instance->rom_version = ((u16)version[0]) | (((u16)version[1]) << 8);
	instance->ram_version = ((u16)version[2]) | (((u16)version[3]) << 8);

	if (!i2c_blocking_read(instance->i2c_handle, SR_EM718X_STATUS, &instance->error.sentral_status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	// this bit is clear in Sentral Status if the firmware is executing
	instance->executing = !instance->error.sentral_status.bits.StandbySt;

	// return register values that the caller cares about
	if (product_id)
		*product_id = buf[0];
	if (revision_id)
		*revision_id = buf[1];
	if (rom_version)
		*rom_version = instance->rom_version;
	if (ram_version)
		*ram_version = instance->ram_version;
	if (eeprom_present)
		*eeprom_present = instance->error.sentral_status.bits.EEUploadDone;

	info_log("VERSION: PRODUCT:0x%x REVISION:0x%x ROM:%d RAM:%d", buf[0], buf[1],
			instance->rom_version, instance->ram_version);

	instance->detected = TRUE;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool eeprom_check_header(EEPROMHeader *header, u16 file_size)
{
	if (!header)                                                      // need a pointer
		return FALSE;
	if (header->magic != EEPROM_MAGIC_VALUE)                          // check the magic number
		return FALSE;
	if (!file_size)                                                   // host service could not determine, so skip this check
		return TRUE;
	if ((sizeof(EEPROMHeader) + header->text_length) > file_size)     // sanity check the lengths
		return FALSE;
	return TRUE;
}


bool di_upload_firmware_start(DI_INSTANCE_T *instance, EEPROMHeader *header)
{
	RegHostControl host;
	u8 buf[2];

	DI_PREAMBLE_FULL(instance);

	// check the file header
	info_log("check the header...\n");
	if (!eeprom_check_header(header, 0))
	{
		INSTANCE_ERROR(DE_INVALID_FILE_HEADER);
		return FALSE;
	}

	info_log("header: EEPROMNoExec: %u, I2CClockSpeed: %u, ROMVerExp: %u, ImageLength: %u\n",
			header->flags.bits.EEPROMNoExec, header->flags.bits.I2CClockSpeed, header->flags.bits.ROMVerExp, header->text_length);

	// check whether the file is appropriate for this Sentral's ROM
	if (header->flags.bits.ROMVerExp != ROM_VERSION_ANY)               // does it matter what ROM the Sentral has?
	{
		if (((header->flags.bits.ROMVerExp == ROM_VERSION_DI01) && (instance->rom_version != ROM_VER_REG_DI01)) ||
			((header->flags.bits.ROMVerExp == ROM_VERSION_DI02) && (instance->rom_version != ROM_VER_REG_DI02)))
		{
			INSTANCE_ERROR(DE_INCORRECT_ROM_VERSION);
			return FALSE;
		}
	}

#if defined(RESET_EM718X_BEFORE_UPLOAD)
	info_log("reset Sentral...\n");
	// reset
	if (!di_reset_sentral(instance, TRUE))
		return FALSE;
#endif

	info_log("set upload address to 0...\n");
	// start upload at address 0
	buf[0] = buf[1] = 0;
	if (!i2c_blocking_write(instance->i2c_handle, SR_UPLOAD_ADDR_H, buf, 2))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	info_log("request upload mode...\n");
	// go into upload mode
	host.reg = 0;
	host.bits.HostUpload = 1;
	if (!i2c_blocking_write(instance->i2c_handle, SR_HOST_CONTROL, &host.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	instance->header_crc = header->text_CRC;
	instance->upload_ofs = 0;
	instance->upload_len = (s32)((u32)header->text_length);
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_upload_firmware_data(DI_INSTANCE_T *instance, u32 *words, u16 wlen)
{
	u32 buf[RAM_BUF_LEN / 4];
	u32 temp;
	u16 eewlen;
	int i;
	u16 maxw = i2c_get_max_write_length(instance->i2c_handle) / 4;

	DI_PREAMBLE_FULL(instance);

	// upload all the text section data from the file
#if !defined(__KERNEL__)
	printf("load data ofs %u...\r", instance->upload_ofs);
#endif
	while ((instance->upload_len > 0) && wlen)
	{
		if (wlen > RAM_BUF_LEN / 4)   // limit writes to the buffer size
			eewlen = RAM_BUF_LEN / 4;
		else
			eewlen = wlen;
		if (eewlen > maxw)           // also limit to transport max size
			eewlen = maxw;

		for (i = 0; i < eewlen; i++)
		{
			temp = *(words++);
			buf[i] = swab32(temp); // the bytes are stored in little endian order in the .fw image, but need to be uploaded in big endian order
		}

		if (!i2c_blocking_write(instance->i2c_handle, SR_UPLOAD_DATA, (u8 *)buf, eewlen * 4))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
		instance->upload_ofs += eewlen * 4;
		instance->upload_len -= (s32)((u32)eewlen * 4);
		wlen -= eewlen;
	}
	if (wlen) // they gave us extra
	{
		INSTANCE_ERROR(DE_INVALID_FILE_LENGTH);
		return FALSE;
	}

	INSTANCE_ERROR(DE_NONE);

	return TRUE;
}

bool di_upload_firmware_finish(DI_INSTANCE_T *instance)
{
	RegHostControl host;
	u32 our_crc;

	DI_PREAMBLE_FULL(instance);

	if (!i2c_blocking_read(instance->i2c_handle, SR_CRC_HOST, (u8 *)&our_crc, sizeof(our_crc)))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	info_log("calculated CRC: 0x%08X, original CRC: 0x%08X\n", our_crc, instance->header_crc);

	// end upload mode
	host.reg = 0;
	host.bits.HostUpload = 0;
	if (!i2c_blocking_write(instance->i2c_handle, SR_HOST_CONTROL, &host.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	IRQ_CHECK(instance);

	// check results
	if (instance->upload_len != 0)
	{
		INSTANCE_ERROR(DE_INVALID_FILE_LENGTH);
		return FALSE;
	}
	if (our_crc != instance->header_crc)
	{
		INSTANCE_ERROR(DE_INVALID_CRC);
		return FALSE;
	}
	info_log("upload complete\n");
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_upload_eeprom_start(DI_INSTANCE_T *instance, I2C_HANDLE_T eeprom_handle, EEPROMHeader *header)
{
	u8 buf[EEPROM_BUF_LEN + 2];                                  // add two more bytes for the EEPROM address
	u16 eeprom_addr;
	u8 *src;
	u8 *dst;
	int i;
	u32 start;
	u16 eelen;
	u16 len;
	u16 maxb = i2c_get_max_write_length(instance->i2c_handle);

	DI_PREAMBLE_FULL(instance);

	// check the file header
	info_log("check the header...\n");
	if (!eeprom_check_header(header, 0))
	{
		INSTANCE_ERROR(DE_INVALID_FILE_HEADER);
		return FALSE;
	}

	info_log("header: EEPROMNoExec: %u, I2CClockSpeed: %u, ROMVerExp: %u, ImageLength: %u\n",
			header->flags.bits.EEPROMNoExec, header->flags.bits.I2CClockSpeed, header->flags.bits.ROMVerExp, header->text_length);

	// check whether the file is appropriate for this Sentral's ROM
	if (header->flags.bits.ROMVerExp != ROM_VERSION_ANY)               // does it matter what ROM the Sentral has?
	{
		if (((header->flags.bits.ROMVerExp == ROM_VERSION_DI01) && (instance->rom_version != ROM_VER_REG_DI01)) ||
			((header->flags.bits.ROMVerExp == ROM_VERSION_DI02) && (instance->rom_version != ROM_VER_REG_DI02)))
		{
			INSTANCE_ERROR(DE_INCORRECT_ROM_VERSION);
			return FALSE;
		}
	}

#if defined(RESET_EM718X_BEFORE_UPLOAD)
	info_log("reset Sentral...\n");
	// reset
	if (!di_reset_sentral(instance, TRUE))
		return FALSE;
#endif

	info_log("enter passthrough mode...\n");
	// go into passthrough mode
	if (!di_set_passthrough_mode(instance, TRUE))
		return FALSE;

	// write the header to address 0
	info_log("write EEPROM header...\n");

	len = sizeof(EEPROMHeader);
	eeprom_addr = 0;
	src = (u8 *)header;

	while (len)
	{
		buf[0] = (eeprom_addr >> 8) & 0x0ff;
		buf[1] = eeprom_addr & 0x0ff;
		if (len > EEPROM_BUF_LEN)  // limit to EEPROM page size
			eelen = EEPROM_BUF_LEN;
		else
			eelen = len;
		if (eelen > maxb)          // limit to maximum transport size, so we can rewrite the EEPROM address at the start of each transfer
			eelen = maxb;
		dst = &buf[2];
		for (i = 0; i < eelen; i++)
			*(dst++) = *(src++);
		if (!i2c_blocking_write(eeprom_handle, buf[0], &buf[1], eelen + 1))
		{
			INSTANCE_ERROR(DE_I2C_PASSTHRU_ERROR);
			return FALSE;
		}

		// wait for page to finish
		start = time_ms();
		while (!i2c_blocking_write(eeprom_handle, 0, NULL, 0))
		{
			time_delay_ms(1);
			if (time_ms() > (start + EEPROM_WRITE_TIMEOUT_MS))
				break;
		}
		eeprom_addr += eelen;
		len -= eelen;
	}

	// after the header, upload all the text section data from the file
	instance->upload_len = (s32)((u32)header->text_length);
	instance->upload_ofs = sizeof(EEPROMHeader);

	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}

bool di_upload_eeprom_data(DI_INSTANCE_T *instance, I2C_HANDLE_T eeprom_handle, u32 *words, u16 wlen)
{
	u16 eelen;
	u8 buf[EEPROM_BUF_LEN + 2];                                  // add two more bytes for the EEPROM address
	u32 start;
	u16 maxb = i2c_get_max_write_length(instance->i2c_handle);

	DI_PREAMBLE(instance);

#if !defined(__KERNEL__)
	printf("load data ofs %u...\r", instance->upload_ofs);
#endif
	while ((instance->upload_len > 0) && wlen)
	{
		buf[0] = ((instance->upload_ofs) >> 8) & 0x0ff;
		buf[1] = (instance->upload_ofs) & 0x0ff;
		if (wlen * 4 > EEPROM_BUF_LEN)   // limit to EEPROM page size
			eelen = EEPROM_BUF_LEN;
		else
			eelen = wlen * 4;
		if (eelen > maxb)                // also limit to transport size
			eelen = maxb;
		memcpy(&buf[2], words, eelen);
		if (!i2c_blocking_write(eeprom_handle, buf[0], &buf[1], eelen + 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
		// wait for page to finish
		start = time_ms();
		while (!i2c_blocking_write(eeprom_handle, 0, NULL, 0))
		{
			time_delay_ms(1);
			if (time_ms() > (start + EEPROM_WRITE_TIMEOUT_MS))
				break;
		}
		instance->upload_ofs += eelen;
		instance->upload_len -= (s32)((u32)eelen);
		wlen -= eelen / 4;
	}
	if (wlen) // they gave us extra
	{
		INSTANCE_ERROR(DE_INVALID_FILE_LENGTH);
		return FALSE;
	}

	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}

bool di_upload_eeprom_finish(DI_INSTANCE_T *instance)
{
	if (!DI_PRECHECK_FULL(instance))
	{
		INSTANCE_ERROR(DE_INVALID_INSTANCE);
		return FALSE;
	}

	// leave passthrough mode
	IRQ_CHECK(instance);
	if (!di_set_passthrough_mode(instance, FALSE))
		return FALSE;

	IRQ_CHECK(instance);
	info_log("finished uploading...\nresetting Sentral...\n");
	// reset
	if (!di_reset_sentral(instance, TRUE))
		return FALSE;

	// check results
	if (instance->upload_len != 0)
	{
		INSTANCE_ERROR(DE_INVALID_FILE_LENGTH);
		return FALSE;
	}
	info_log("upload complete\n");
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_erase_eeprom(DI_INSTANCE_T *instance, I2C_HANDLE_T eeprom_handle)
{
	u8 buf[sizeof(EEPROMHeader) + 2];                                 // add two more bytes for the EEPROM address
	u16 eeprom_addr;
	u32 start;

	DI_PREAMBLE_FULL(instance);

	info_log("reset Sentral...\n");
	// reset
#if defined(RESET_EM718X_BEFORE_UPLOAD)
	di_reset_sentral(instance, TRUE); // if this fails, it's ok -- we might have a bad upload
#endif

	info_log("enter passthrough mode...\n");
	// go into passthrough mode
	if (!di_set_passthrough_mode(instance, TRUE))
		return FALSE;

	// write the header to address 0
	info_log("write invalid EEPROM header...\n");
	memset(buf, 0, sizeof(buf));
	eeprom_addr = 0;
	buf[0] = (eeprom_addr >> 8) & 0x0ff;
	buf[1] = eeprom_addr & 0x0ff;
	if (!i2c_blocking_write(eeprom_handle, buf[0], &buf[1], sizeof(EEPROMHeader) + 1))
	{
		INSTANCE_ERROR(DE_I2C_PASSTHRU_ERROR);
		return FALSE;
	}
	// wait for page to finish
	start = time_ms();
	while (!i2c_blocking_write(eeprom_handle, 0, NULL, 0))
	{
		time_delay_ms(1);
		if (time_ms() > (start + EEPROM_WRITE_TIMEOUT_MS))
			break;
	}

	// leave passthrough mode
	if (!di_set_passthrough_mode(instance, FALSE))
		return FALSE;

	info_log("finished erasing EEPROM...\nresetting Sentral...\n");
	// reset
	if (!di_reset_sentral(instance, TRUE))
		return FALSE;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}



bool di_query_features(DI_INSTANCE_T *instance)
{
	RegSensorStatus status;
	RegFeatureFlags features;
	int quaternion_prerequisites = 0;
	int i;
	DI_SENSOR_TYPE_T sensor;

	DI_PREAMBLE(instance);

	debug_log("query features\n");

	if (!instance->executing)
	{
		INSTANCE_ERROR(DE_EXECUTING_REQUIRED);
		return FALSE;
	}
	// clear present flag for all sensors
	for (sensor = DST_FIRST; sensor < DST_NUM_SENSOR_TYPES; sensor++)
		instance->sensor_info[sensor].present = FALSE;

	instance->num_sensors = 0;

	// find out if the basic sensors are present and working
	if (!i2c_blocking_read(instance->i2c_handle, SR_SENSOR_STATUS, &status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	// find out if there are any additional sensors
	if (!i2c_blocking_read(instance->i2c_handle, SR_FEATURE_FLAGS, &features.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// populate list of sensors
	INSTANCE_ERROR(DE_NONE);

#define SETUP_SENSOR(t, d)												\
	instance->sensor_info[DST_##t].present = TRUE;						\
	instance->sensor_info[DST_##t].rate_min = SENSOR_##t##_RATE_MIN;	\
	instance->sensor_info[DST_##t].rate_max = SENSOR_##t##_RATE_MAX;	\
	instance->sensor_info[DST_##t].rate_reg = SR_##t##_RATE;			\
	instance->sensor_info[DST_##t].deps = (d);							\
	instance->sensor_info[DST_##t].name = SENSOR_##t##_NAME;

	// first check the standard ones

	if (!status.bits.MagNACK && !status.bits.MagDeviceIDErr)
	{
		quaternion_prerequisites++;
		SETUP_SENSOR(MAG, 0);
		instance->num_sensors++;
	}
	if (!status.bits.AccelNACK && !status.bits.AccelDeviceIDErr)
	{
		quaternion_prerequisites++;
		SETUP_SENSOR(ACCEL, 0);
		instance->num_sensors++;
	}
	if (!status.bits.GyroNACK && !status.bits.GyroDeviceIDErr)
	{
		quaternion_prerequisites++;
		SETUP_SENSOR(GYRO, 0);
		instance->num_sensors++;
	}
	// we can get a quaternion for 6DOF or 9DOF
	if ((quaternion_prerequisites >= 2) && instance->sensor_info[DST_ACCEL].present)
	{
		SETUP_SENSOR(QUATERNION, BIT(DST_ACCEL) | BIT(DST_GYRO) | BIT(DST_MAG));
		instance->num_sensors++;
	}

	// now check for additional sensors (features)
	instance->num_feature_sensors = 0;
	for (i = 0; i < MAX_FEATURE_SENSORS; i++)
		instance->feature_sensors[i] = DST_NONE;
	if (features.bits.BaromInstalled)
	{
		if (instance->num_feature_sensors >= MAX_FEATURE_SENSORS)
		{
			INSTANCE_ERROR(DE_TOO_MANY_FEATURE_SENSORS);
			return FALSE;
		}
		instance->feature_sensors[instance->num_feature_sensors++] = DST_BAROM;
		instance->sensor_info[DST_BAROM].present = TRUE;
		instance->num_sensors++;
	}
	if (features.bits.HumidityInstalled)
	{
		if (instance->num_feature_sensors >= MAX_FEATURE_SENSORS)
		{
			INSTANCE_ERROR(DE_TOO_MANY_FEATURE_SENSORS);
			return FALSE;
		}
		instance->feature_sensors[instance->num_feature_sensors++] = DST_HUMID;
		instance->sensor_info[DST_HUMID].present = TRUE;
		instance->num_sensors++;
	}
	if (features.bits.TemperatureInstalled)
	{
		if (instance->num_feature_sensors >= MAX_FEATURE_SENSORS)
		{
			INSTANCE_ERROR(DE_TOO_MANY_FEATURE_SENSORS);
			return FALSE;
		}
		instance->feature_sensors[instance->num_feature_sensors++] = DST_TEMP;
		instance->sensor_info[DST_TEMP].present = TRUE;
		instance->num_sensors++;
	}
	if (features.bits.CustomSensor0Installed)
	{
		if (instance->num_feature_sensors >= MAX_FEATURE_SENSORS)
		{
			INSTANCE_ERROR(DE_TOO_MANY_FEATURE_SENSORS);
			return FALSE;
		}
		instance->feature_sensors[instance->num_feature_sensors++] = DST_CUST0;
		SETUP_SENSOR(CUST0, BIT(DST_ACCEL) );
		instance->num_sensors++;
	}
	if (features.bits.CustomSensor1Installed)
	{
		if (instance->num_feature_sensors >= MAX_FEATURE_SENSORS)
		{
			INSTANCE_ERROR(DE_TOO_MANY_FEATURE_SENSORS);
			return FALSE;
		}
		instance->feature_sensors[instance->num_feature_sensors++] = DST_CUST1;
		SETUP_SENSOR(CUST1, BIT(DST_ACCEL) );
		instance->num_sensors++;
	}
/* N/A
	if (features.bits.CustomSensor2Installed)
	{
		if (instance->num_feature_sensors >= MAX_FEATURE_SENSORS)
		{
			INSTANCE_ERROR(DE_TOO_MANY_FEATURE_SENSORS);
			return FALSE;
		}
		instance->feature_sensors[instance->num_feature_sensors++] = DST_CUST2;
		instance->sensor_info[DST_CUST2].present = TRUE;
		instance->num_sensors++;
	}
*/
	return TRUE;
}


bool di_has_sensor( DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor)
{
	DI_PREAMBLE(instance);

	if (sensor < DST_NUM_SENSOR_TYPES) {
		return instance->sensor_info[sensor].present;
	} else {
		INSTANCE_ERROR(DE_UNKNOWN_SENSOR);
		return FALSE;
	}
}



bool di_configure_rate(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, u16 rate_hz)
{
	u8 val;

	DI_PREAMBLE_FULL(instance);

	switch (sensor)
	{
	case DST_MAG:
		if (rate_hz > 255)
		{
			INSTANCE_ERROR(DE_RATE_TOO_HIGH);                        // requested quaternion rate is faster than the gyro rate
			return FALSE;
		}
		val = (u8)rate_hz;
		break;
	case DST_ACCEL:
		if (rate_hz > 2550)
		{
			INSTANCE_ERROR(DE_RATE_TOO_HIGH);                        // requested quaternion rate is faster than the gyro rate
			return FALSE;
		}
		val = rate_hz / 10;
		//rate_hz = val * 10; // record the actual rate in the instance's sensor_rates array
		break;
	case DST_GYRO:
		if (rate_hz > 2550)
		{
			INSTANCE_ERROR(DE_RATE_TOO_HIGH);                        // requested quaternion rate is faster than the gyro rate
			return FALSE;
		}
		val = rate_hz / 10;
		//rate_hz = val * 10; // record actual rate
		break;
	case DST_QUATERNION:
		if (!instance->sensor_info[DST_GYRO].rate)
		{
			INSTANCE_ERROR(DE_GYRO_RATE_NEEDED);
			return FALSE;
		}
		if (!rate_hz)
			val = 0;
		else
		{
			val = instance->sensor_info[DST_GYRO].rate / rate_hz;
			if (val) {
				rate_hz = instance->sensor_info[DST_GYRO].rate / val; // record actual rate
				if (val == 1)
					val = 0;                                           // try 0 which should be equivalent to 1
			} else {
				INSTANCE_ERROR(DE_RATE_TOO_HIGH);                     // requested quaternion rate is faster than the gyro rate
				return FALSE;
			}
		}
		break;
	case DST_CUST0:
		if (rate_hz > 255)
		{
			INSTANCE_ERROR(DE_RATE_TOO_HIGH);                        // requested quaternion rate is faster than the gyro rate
			return FALSE;
		}
		val = (u8)rate_hz;
		break;
	case DST_CUST1:
		if (rate_hz > 255)
		{
			INSTANCE_ERROR(DE_RATE_TOO_HIGH);                        // requested quaternion rate is faster than the gyro rate
			return FALSE;
		}
		val = (u8)rate_hz;
		break;
	case DST_CUST2:
		if (rate_hz > 255)
		{
			INSTANCE_ERROR(DE_RATE_TOO_HIGH);                        // requested quaternion rate is faster than the gyro rate
			return FALSE;
		}
		val = (u8)rate_hz;
		break;
	default:
		// we don't know how to set the rate for this sensor
		INSTANCE_ERROR(DE_RATE_NOT_SETTABLE);
		return FALSE;
	}

	// set the specified sensor's rate
	if (val || (sensor != DST_QUATERNION))
	{
		debug_log("setting rate reg:0x%x=%d\n", instance->sensor_info[sensor].rate_reg, val);
		if (!i2c_blocking_write(instance->i2c_handle, instance->sensor_info[sensor].rate_reg, &val, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
	}

	instance->sensor_info[sensor].rate = rate_hz;
	INSTANCE_ERROR(DE_NONE);

	return TRUE;
}










bool di_query_actual_rate(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, u16 *rate_hz)
{
	u8 reg;
	u8 val;
	u8 multiplier = 1;
	u16 rate;

	DI_PREAMBLE_FULL(instance);

	switch (sensor)
	{
	case DST_MAG:
		reg = SR_ACTUAL_MAG_RATE;
		break;
	case DST_ACCEL:
		reg = SR_ACTUAL_ACCEL_RATE;
		multiplier = 10;
		break;
	case DST_GYRO:
		reg = SR_ACTUAL_GYRO_RATE;
		multiplier = 10;
		break;
	case DST_QUATERNION:
		reg = SR_QRATE_DIVISOR;
		break;
	case DST_CUST0:
		reg = SR_ACTUAL_CUST_SENSOR0_RATE;
		break;
	case DST_CUST1:
		reg = SR_ACTUAL_CUST_SENSOR1_RATE;
		break;
	case DST_CUST2:
		reg = SR_ACTUAL_CUST_SENSOR2_RATE;
		break;
	default:
		// we don't know how to set the rate for this sensor
		INSTANCE_ERROR(DE_UNKNOWN_SENSOR);
		return FALSE;
	}

	// get the specified sensor's rate
	if (!i2c_blocking_read(instance->i2c_handle, reg, &val, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// handle special case for quaternion
	if (sensor == DST_QUATERNION)
	{
		u8 gyro_val;
		u16 gyro_rate;

		if (!i2c_blocking_read(instance->i2c_handle, SR_ACTUAL_GYRO_RATE, &gyro_val, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
		gyro_rate = ((u16)gyro_val) * 10;
		if (!val)                                                      // a 0 quaternion divisor is the same as 1
			val = 1;
		rate = gyro_rate / val;
	}
	else
		rate = ((u16)val) * multiplier;

	if (rate_hz)
		*rate_hz = rate;

	instance->sensor_info[sensor].actual_rate = rate;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_save_parameters(DI_INSTANCE_T *instance, u8 *param_numbers, u8 num_params, u8 *params)
{
	int i;
	u8 param;
	u32 start_tick;
	int in_halt = 0;

	DI_PREAMBLE_FULL(instance);

	if (!param_numbers || !num_params || !params)
	{
		INSTANCE_ERROR(DE_INVALID_PARAMETERS);
		return FALSE;
	}
	INSTANCE_ERROR(DE_NONE);

	for (i = 0; i < num_params; i++)
	{
		// make sure param is valid
		if (param_numbers[i] > MAX_PARAM_NUMBER)
		{
			INSTANCE_ERROR(DE_PARAM_OUT_OF_RANGE);
			break;
		}
		// hmm, this is a legal value which means "terminate the procedure" -- but we do this automatically, so the caller should not specify this
		if (param_numbers[i] == 0)
			break;

		// write the parameter number first
		if (!i2c_blocking_write(instance->i2c_handle, SR_PARAMETER_REQUEST, &param_numbers[i], 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		// take out of standby
		if (instance->algo_control.bits.RequestHalt) {
			in_halt = 1;
			if (!di_normal_exec_request(instance)) {
				INSTANCE_ERROR(DE_I2C_ERROR);
				return FALSE;
			}
		}

		// we have not requested the procedure yet; do so now
		if (!instance->algo_control.bits.ParameterProcedure)
		{
			instance->algo_control.bits.ParameterProcedure = 1;
			if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
			{
				instance->algo_control.bits.ParameterProcedure = 0;      // we don't know that it worked...
				INSTANCE_ERROR(DE_I2C_ERROR);
				return FALSE;
			}
		}

		// wait for acknowledge from Sentral
		start_tick = time_ms();
		for (;;)
		{
			if (!i2c_blocking_read(instance->i2c_handle, SR_PARAMETER_ACKNOWLEDGE, &param, 1))
			{
				INSTANCE_ERROR(DE_I2C_ERROR);
				return FALSE;
			}
			// it matches... we can move on
			if (param == param_numbers[i])
				break;
			// check for timeout
			if ((time_ms() - start_tick) > PARAM_ACK_TIMEOUT_MS)
			{
				INSTANCE_ERROR(DE_PARAM_ACK_TIMEOUT);
				break;
			}
			time_delay_ms(10);
		}

		// now read the parameter value
		if (!i2c_blocking_read(instance->i2c_handle, SR_SAVED_PARAM_B0, &params[i * 4], 4))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
	}

	// we have requested the procedure, so tell it we're done
	// note: normally we return immediately when an I2C error occurs, but we'll try harder to
	// actually get out of the param procedure mode first (may result in another I2C error)
	if (instance->algo_control.bits.ParameterProcedure)
	{
		param = 0;                                                     // 0 means stop
		if (!i2c_blocking_write(instance->i2c_handle, SR_PARAMETER_REQUEST, &param, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
		}

		// if it was originally in standby, put it back
		if (in_halt) {
			if (!di_standby_request(instance)) {
				return FALSE;
			}
		}

		instance->algo_control.bits.ParameterProcedure = 0;
		if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
		{
			instance->algo_control.bits.ParameterProcedure = 1;         // we don't know that it worked...
			INSTANCE_ERROR(DE_I2C_ERROR);
		}
	}

	if (instance->error.driver_error != DE_NONE)
		return FALSE;
	else
		return TRUE;
}


bool di_load_parameters(DI_INSTANCE_T *instance, u8 *param_numbers, u8 num_params, u8 *params)
{
	int i;
	u8 param;
	u32 start_tick;
	u8 read_param[4];

	DI_PREAMBLE_FULL(instance);

	if (!param_numbers || !num_params || !params)
	{
		INSTANCE_ERROR(DE_INVALID_PARAMETERS);
		return FALSE;
	}
	INSTANCE_ERROR(DE_NONE);

	for (i = 0; i < num_params; i++)
	{
		// make sure param is valid
		if (param_numbers[i] > MAX_PARAM_NUMBER)
		{
			INSTANCE_ERROR(DE_PARAM_OUT_OF_RANGE);
			break;
		}
		// hmm, this is a legal value which means "terminate the procedure" -- but we do this automatically, so the caller should not specify this
		if (param_numbers[i] == 0)
			break;

		// write the parameter value first
		if (!i2c_blocking_write(instance->i2c_handle, SR_LOAD_PARAM_B0, &params[i * 4], 4))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		// now set the parameter number with MSB high to request a load of the parameter
		param = param_numbers[i] | 0x80;
		if (!i2c_blocking_write(instance->i2c_handle, SR_PARAMETER_REQUEST, &param, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		// we have not requested the procedure yet; do so now
		if (!instance->algo_control.bits.ParameterProcedure)
		{
			instance->algo_control.bits.ParameterProcedure = 1;
			if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
			{
				instance->algo_control.bits.ParameterProcedure = 0;      // we don't know that it worked...
				INSTANCE_ERROR(DE_I2C_ERROR);
				return FALSE;
			}
		}

		// wait for acknowledge from Sentral
		start_tick = time_ms();
		for (;;)
		{
			if (!i2c_blocking_read(instance->i2c_handle, SR_PARAMETER_ACKNOWLEDGE, &param, 1))
			{
				INSTANCE_ERROR(DE_I2C_ERROR);
				return FALSE;
			}
			// it matches... we can move on
			if (param == (param_numbers[i] | 0x80))
				break;
			// check for timeout
			if ((time_ms() - start_tick) > PARAM_ACK_TIMEOUT_MS)
			{
				INSTANCE_ERROR(DE_PARAM_ACK_TIMEOUT);
				break;
			}
			time_delay_ms(10);
		}

		// request a save of the same parameter
		param &= 0x7f;
		if (!i2c_blocking_write(instance->i2c_handle, SR_PARAMETER_REQUEST, &param, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		// wait for acknowledge from Sentral
		start_tick = time_ms();
		for (;;)
		{
			if (!i2c_blocking_read(instance->i2c_handle, SR_PARAMETER_ACKNOWLEDGE, &param, 1))
			{
				INSTANCE_ERROR(DE_I2C_ERROR);
				return FALSE;
			}
			// it matches... we can move on
			if (param == param_numbers[i])
				break;
			// check for timeout
			if ((time_ms() - start_tick) > PARAM_ACK_TIMEOUT_MS)
			{
				INSTANCE_ERROR(DE_PARAM_ACK_TIMEOUT);
				break;
			}
			time_delay_ms(10);
		}

		// read back to check the buffer
		if (!i2c_blocking_read(instance->i2c_handle, SR_SAVED_PARAM_B0, read_param, 4))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		// compare to what we wrote
		if (memcmp(read_param, &params[i * 4], 4) != 0)
		{
			error_log("miscompare param %u, wrote: 0x%02X, 0x%02X, 0x%02X, 0x%02X; read: 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
					param & 0x7f,
					params[i * 4], params[i * 4 + 1], params[i * 4 + 2], params[i * 4 + 3],
					read_param[0], read_param[1], read_param[2], read_param[3]);
			INSTANCE_ERROR(DE_EM718X_ERROR);
			break;
		}
	}

	// we have requested the procedure, so tell it we're done
	// note: normally we return immediately when an I2C error occurs, but we'll try harder to
	// actually get out of the param procedure mode first (may result in another I2C error)
	if (instance->algo_control.bits.ParameterProcedure)
	{
		param = 0;                                                     // 0 means stop
		if (!i2c_blocking_write(instance->i2c_handle, SR_PARAMETER_REQUEST, &param, 1))
			INSTANCE_ERROR(DE_I2C_ERROR);
		instance->algo_control.bits.ParameterProcedure = 0;
		if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
		{
			instance->algo_control.bits.ParameterProcedure = 1;         // we don't know that it worked...
			INSTANCE_ERROR(DE_I2C_ERROR);
		}
	}

	if (instance->error.driver_error != DE_NONE)
		return FALSE;
	else
		return TRUE;
}


bool di_configure_output(DI_INSTANCE_T *instance, bool hpr_output, bool raw_data_output, bool ag6dof, bool am6dof, bool enu, bool enhanced_still_mode)
{
	DI_PREAMBLE_FULL(instance);

	// set the output format
	instance->algo_control.bits.RawDataOutput = raw_data_output;
	instance->algo_control.bits.HPROutput = hpr_output;
	instance->algo_control.bits.AccelGyro6DOF = ag6dof;               // note: this and the next bit will only be effective if in shutdown mode
	instance->algo_control.bits.AccelMag6DOF = am6dof;
	instance->algo_control.bits.ENU = enu;
	instance->algo_control.bits.EnhancedStillMode = enhanced_still_mode;
	if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	INSTANCE_ERROR(DE_NONE);

	if (ag6dof)
		/* no mag dependency for 6DOF mode */
		instance->sensor_info[DST_QUATERNION].deps &= ~BIT(DST_MAG);


	return TRUE;
}


bool di_enable_sensor_interrupts(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T *sensors_to_enable, u8 num_to_enable)
{
	int i;

	DI_PREAMBLE_FULL(instance);

	instance->events.reg = 0;

	if (num_to_enable && sensors_to_enable)                         // caller wants to turn off all events
	{
		instance->events.bits.CPUResetRsvd = TRUE;                        // we always want to know about resets
		instance->events.bits.Error = TRUE;                               // we always want to know about errors

		for (i = 0; i < num_to_enable; i++)
		{
			switch (sensors_to_enable[i])
			{
			case DST_MAG:
				instance->events.bits.MagResult = 1;
				debug_log("enabling mag events\n");
				break;
			case DST_ACCEL:
				instance->events.bits.AccelResult = 1;
				debug_log("enabling accel events\n");
				break;
			case DST_GYRO:
				instance->events.bits.GyroResult = 1;
				debug_log("enabling gyro events\n");
				break;
			case DST_QUATERNION:
				instance->events.bits.QuaternionResult = 1;
				debug_log("enabling quaternion events\n");
				break;
			case DST_BAROM:
			case DST_HUMID:
			case DST_TEMP:
			case DST_CUST0:
			case DST_CUST1:
			case DST_CUST2:
				if (1)
					instance->feature_events |= (1 << sensors_to_enable[i]); // set a bit for this sensor
				else
					instance->feature_events &= ~(1 << sensors_to_enable[i]); // clear a bit
				if (instance->feature_events) {                            // if any bits are still set, keep feature results enabled
					instance->events.bits.FeatureResults = TRUE;
					debug_log("enabling feature events\n");
				} else {
					instance->events.bits.FeatureResults = FALSE;
				}
				break;
			default:
				break;
			}
		}

	}

	//di_shutdown_request(instance);

	debug_log("setting interrupts sources to 0x%x\n", instance->events.reg);

	if (!i2c_blocking_write(instance->i2c_handle, SR_ENABLE_EVENTS, &instance->events.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// if any interrupts requested, also make sure host control is enabled
	if ( instance->events.reg )
		di_normal_exec_request(instance);
/*
	else
		//di_shutdown_request(instance);
		di_standby_request(instance);
*/

	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_set_sensors_to_acquire(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T *sensors_to_enable, u8 num_to_enable)
{
	int i;

	DI_PREAMBLE_FULL(instance);

	// clear all enabled sensors
	for (i = 0; i < DST_NUM_SENSOR_TYPES; i++) instance->sensor_info[i].acquisition_enable = FALSE;
	for (i = 0; i < num_to_enable; i++)
	{
		switch (sensors_to_enable[i])
		{
		case DST_MAG:
		case DST_ACCEL:
		case DST_GYRO:
		case DST_QUATERNION:
		case DST_BAROM:
		case DST_HUMID:
		case DST_TEMP:
		case DST_CUST0:
		case DST_CUST1:
		case DST_CUST2:
			// build array indexable by sensor ID of sensors the caller wants to acquire
			instance->sensor_info[sensors_to_enable[i]].acquisition_enable = TRUE;
			break;
		default:
			INSTANCE_ERROR(DE_INVALID_PARAMETERS);
			return FALSE;
		}
	}

	if (num_to_enable > 1)                                            // combine reading more than one sensor into a single I2C transfer
		instance->read_all_sensor_regs = TRUE;
	else                                                              // user only cares about 1 sensor, so only read it
		instance->read_all_sensor_regs = FALSE;

	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}

bool di_enable_sensor_acquisition(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, bool enable)
{
	DI_SENSOR_INFO_T *info = instance->sensor_info;
	int i;
	int enabled_count = 0;

	DI_PREAMBLE_FULL(instance);

	if (info[sensor].acquisition_enable == enable)
		return TRUE;

	debug_log("sensor enable %d: %d\n", sensor, enable);
/*
	if (!enable) {
		Asus_feature_events &= ~(1 << sensor);
		printk("Evan Asus_feature_events disable :%d\n", Asus_feature_events);
	} else {
		Asus_feature_events |= (1 << sensor);
		printk("Evan Asus_feature_events enable :%d\n", Asus_feature_events);
	}
*/
	if ( !di_request_power( instance, sensor, sensor, enable) )
		return FALSE;

	info[sensor].acquisition_enable = enable;
	info[sensor].timestamp_prev = 0;

	for (i = DST_FIRST; i < DST_NUM_SENSOR_TYPES; i++)
	{
		if (!info[i].present)
			break;

		if (info[i].power)
			enabled_count++;
	}

	debug_log("%d sensor(s) enabled\n", enabled_count);

	if (enabled_count > 1)                                            // combine reading more than one sensor into a single I2C transfer
		instance->read_all_sensor_regs = TRUE;
	else                                                              // user only cares about 1 sensor, so only read it
		instance->read_all_sensor_regs = FALSE;

	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}

bool di_register(DI_INSTANCE_T *instance, DATA_CALLBACK data_callback, void *user_param)
{
	DI_PREAMBLE(instance);

	/// @todo: prevent race conditions while modifying these two values...
	instance->data_callback = data_callback;
	instance->user_param = user_param;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_deregister(DI_INSTANCE_T *instance)
{
	DI_PREAMBLE(instance);

	/// @todo: prevent race conditions while modifying these two values...
	instance->data_callback = NULL;
	instance->user_param = NULL;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_run_request(DI_INSTANCE_T *instance)
{
	RegHostControl host;

	DI_PREAMBLE_FULL(instance);

//	if(instance->executing)
//		return TRUE;

	// start the firmware running
	host.reg = 0;
	host.bits.CPURunReq = 1;
	debug_log("set host control 0x%x\n", host.reg);
	if (!i2c_blocking_write(instance->i2c_handle, SR_HOST_CONTROL, &host.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	instance->executing = TRUE;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_normal_exec_request(DI_INSTANCE_T *instance)
{
	RegAlgorithmStatus status;
	u32 start;

	DI_PREAMBLE_FULL(instance);

//	if (!instance->executing)                                         // we need to start it
//	{
	if (!di_run_request(instance))
		return FALSE;
//	}

	debug_log("updating algo control register\n");

	// get the algorithm to run
	instance->algo_control.bits.RequestHalt = 0;
	if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// check that it has started
	start = time_ms();
	do
	{
		IRQ_CHECK(instance);
		if (!i2c_blocking_read(instance->i2c_handle, SR_ALGO_STATUS, &status.reg, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
		time_delay_ms(1);
		if (time_ms() > (start + RUN_TIMEOUT_MS))
			break;
	}
	while (status.bits.Halted);
	instance->sensing = !status.bits.Halted;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_standby_request(DI_INSTANCE_T *instance)
{
	RegAlgorithmStatus status;
	u32 start;

	DI_PREAMBLE(instance);
	if (!instance->executing)
	{
		INSTANCE_ERROR(DE_EXECUTING_REQUIRED);
		return FALSE;
	}

	// get the algorithm to pause
	instance->algo_control.bits.RequestHalt = 1;
	if (!i2c_blocking_write(instance->i2c_handle, SR_ALGO_CONTROL, &instance->algo_control.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// check that it has paused
	start = time_ms();
	do
	{
		IRQ_CHECK(instance);
		if (!i2c_blocking_read(instance->i2c_handle, SR_ALGO_STATUS, &status.reg, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
		time_delay_ms(1);
		if (time_ms() > (start + PAUSE_TIMEOUT_MS))
			break;
	}
	while (!status.bits.Halted);

	instance->sensing = FALSE;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_shutdown_request(DI_INSTANCE_T *instance)
{
	RegHostControl host;
	u32 start;

	DI_PREAMBLE_FULL(instance);

	debug_log("shutdown\n");

	if (!instance->executing)
		return TRUE;

	if (instance->sensing)                                            // algorithm is running, so shut it down first to turn off the sensors
	{
		if (!di_standby_request(instance))
			return FALSE;
	}

	host.reg = 0;
	host.bits.CPURunReq = 0;
	debug_log("host control: 0x%x", host.reg);
	if (!i2c_blocking_write(instance->i2c_handle, SR_HOST_CONTROL, &host.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// check that it has stopped
	start = time_ms();
	do
	{
		IRQ_CHECK(instance);
		if (!i2c_blocking_read(instance->i2c_handle, SR_EM718X_STATUS, &instance->error.sentral_status.reg, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}
		time_delay_ms(10);
		if (time_ms() > (start + SHUTDOWN_TIMEOUT_MS))
			break;
	}
	while (!instance->error.sentral_status.bits.StandbySt);

	instance->executing = FALSE;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_query_status(DI_INSTANCE_T *instance, bool *executing, bool *sensing)
{
	RegAlgorithmStatus status;

	DI_PREAMBLE(instance);

	if (!i2c_blocking_read(instance->i2c_handle, SR_EM718X_STATUS, &instance->error.sentral_status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	if (executing)
		*executing = !instance->error.sentral_status.bits.StandbySt;

	if (!i2c_blocking_read(instance->i2c_handle, SR_ALGO_STATUS, &status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	instance->sensing = !status.bits.Halted;
	if (sensing)
		*sensing = !status.bits.Halted;
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_query_error(DI_INSTANCE_T *instance, DI_ERROR_T *error_info)
{
	DI_PREAMBLE(instance);
	if (!error_info)
	{
		INSTANCE_ERROR(DE_INVALID_PARAMETERS);
		return FALSE;
	}
	if (read_sentral_error_registers(instance))
	{
		memcpy(error_info, &instance->error, sizeof(DI_ERROR_T));
		return TRUE;
	}
	return FALSE;
}


bool di_get_last_error(DI_INSTANCE_T *instance, DI_ERROR_T *error_info)
{
	DI_PREAMBLE(instance);
	if (!error_info)
	{
		INSTANCE_ERROR(DE_INVALID_PARAMETERS);
		return FALSE;
	}
	memcpy(error_info, &instance->error, sizeof(DI_ERROR_T));
	return TRUE;
}


bool di_read_registers(DI_INSTANCE_T *instance, u8 start_reg, u8 end_reg, u8 *dest)
{
	DI_PREAMBLE(instance);

	if (!i2c_blocking_read(instance->i2c_handle, start_reg, dest, end_reg - start_reg + 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}

bool di_write_registers(DI_INSTANCE_T *instance, u8 start_reg, u8 end_reg, u8 *src)
{
	DI_PREAMBLE(instance);

	if (!i2c_blocking_write(instance->i2c_handle, start_reg, src, end_reg - start_reg + 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


bool di_set_passthrough_mode(DI_INSTANCE_T * instance, bool enable)
{
	RegPassthruStatus status;
	RegPassthruControl control;
	u32 start;
	bool ret = TRUE;

	// go into passthrough mode
	control.reg = 0;
	control.bits.PassthroughEn = enable;
	if (!i2c_blocking_write(instance->i2c_handle, SR_PASSTHRU_CONTROL, &control.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	// check that we're in passthrough mode
	start = time_ms();
	do
	{
		IRQ_CHECK(instance);
		if (!i2c_blocking_read(instance->i2c_handle, SR_PASSTHRU_STATUS, &status.reg, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			ret = FALSE;
		}
		if (time_ms() > (start + PASSTHROUGH_TIMEOUT_MS))
		{
			INSTANCE_ERROR(DE_PASSTHROUGH_TIMEOUT);
			ret = FALSE;
		}
		if (!ret) // it failed, so try cleaning up by writing it back to non-passthrough
		{
			if (enable)
			{
				control.bits.PassthroughEn = FALSE;
				i2c_blocking_write(instance->i2c_handle, SR_PASSTHRU_CONTROL, &control.reg, 1);
			}
			break;
		}
	}
	while (status.bits.PassthruReady != enable);

	instance->passthrough = enable;
	return ret;
}


bool di_reset_sentral(DI_INSTANCE_T * instance, bool wait_for_standby)
{
	RegResetRequest reset;
	u32 start;

#ifdef CONFIG_MANAGE_IRQ
	ATOMIC_INT int_count;

	// reset
	int_count = instance->interrupt;
	IRQ_CHECK(instance);

	instance->executing = FALSE;
	instance->sensing = FALSE;
#endif /* CONFIG_MANAGE_IRQ */

	reset.reg = 0;
	reset.bits.ResetRequest = 1;

	if (!i2c_blocking_write(instance->i2c_handle, SR_RESET_REQ, &reset.reg, 1))
	{
		warn_log("error writing to reset register; ignoring\n");
		time_delay_ms(100);
		//INSTANCE_ERROR(DE_I2C_ERROR);
		//return FALSE;
	}

#ifdef CONFIG_MANAGE_IRQ
	// wait for interrupt
	start = time_ms();
	while (instance->interrupt == int_count)
	{
		IRQ_CHECK(instance);
		if (time_ms() > (start + EEPROM_UPLOAD_TIME_MS))
		{
			warn_log("timeout waiting for interrupt\n");
			break;
		}
	}
#endif /* CONFIG_MANAGE_IRQ */

	start = time_ms();
	// keep reading Sentral status until EEPROM Upload comes to a conclusion
	for (;;)
	{
		IRQ_CHECK(instance);

		if (!i2c_blocking_read(instance->i2c_handle, SR_EM718X_STATUS, &instance->error.sentral_status.reg, 1))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		if (instance->error.sentral_status.bits.StandbySt || !wait_for_standby)
		{
			if (instance->error.sentral_status.bits.EEUploadError ||
				instance->error.sentral_status.bits.EEUploadDone ||
				instance->error.sentral_status.bits.NoEEPROM)
			break;
		}

		if (time_ms() > (start + EEPROM_UPLOAD_TIME_MS))
		{
			warn_log("timeout waiting for proper Sentral status: 0x%02X  start:%u %u\n",
					instance->error.sentral_status.reg, start, time_ms() );
			break;
		}

		time_delay_ms(50);
	}

	return read_sentral_error_registers(instance);
}


bool read_sentral_error_registers(DI_INSTANCE_T * instance)
{
	if (!i2c_blocking_read(instance->i2c_handle, SR_EM718X_STATUS, &instance->error.sentral_status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	if (!i2c_blocking_read(instance->i2c_handle, SR_EVENT_STATUS, &instance->error.event_status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	if (!i2c_blocking_read(instance->i2c_handle, SR_SENSOR_STATUS, &instance->error.sensor_status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	instance->executing = !instance->error.sentral_status.bits.StandbySt;
	if (!i2c_blocking_read(instance->i2c_handle, SR_ALGO_STATUS, &instance->error.algo_status.reg, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	instance->sensing = !instance->error.algo_status.bits.Halted;
	instance->error.error_register = (RegErrorValues)0;
	if (!i2c_blocking_read(instance->i2c_handle, SR_ERROR_REGISTER, (u8 *)&instance->error.error_register, 1))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}

	debug_log("current state: device_status:%x event:%x sensor_status:%x algo_status:%x error_status:%x",
			instance->error.sentral_status.reg,
			instance->error.event_status.reg,
			instance->error.sensor_status.reg,
			instance->error.algo_status.reg,
			instance->error.error_register );

	return TRUE;
}


bool di_query_sensor_start(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, bool int_callback)
{
	u8 reg;

	DI_PREAMBLE_FULL(instance);

	insane_log("query sensor start: sensor %d\n", sensor);

	if (!instance->executing)
	{
		INSTANCE_ERROR(DE_EXECUTING_REQUIRED);
		return FALSE;
	}
	if (!instance->sensing)
	{
		INSTANCE_ERROR(DE_SENSING_REQUIRED);
		return FALSE;
	}

	switch (sensor)
	{
	case DST_ALL:
		reg = SR_QX;                                                // read all data at once
		instance->sensor_len = 0x32;
		break;
	case DST_MAG:
		reg = SR_MX;
		instance->sensor_len = 8;
		break;
	case DST_ACCEL:
		reg = SR_AX;
		instance->sensor_len = 8;
		break;
	case DST_GYRO:
		reg = SR_GX;
		instance->sensor_len = 8;
		break;
	case DST_QUATERNION:
		reg = SR_QX;
		instance->sensor_len = 18;
		break;
	case DST_BAROM:
	case DST_HUMID:
	case DST_TEMP:
	case DST_CUST0:
	case DST_CUST1:
	case DST_CUST2:
		// should this be simpler and just always read all feature sensors that are enabled?
		if (instance->feature_sensors[0] == sensor) {
			reg = SR_FEATURE0;
			if (instance->num_feature_sensors < MAX_FEATURE_SENSORS)
				instance->sensor_len = 4;
			else
				instance->sensor_len = 8;
		} else if (instance->feature_sensors[1] == sensor) {
			reg = SR_FEATURE1;
			if (instance->num_feature_sensors < MAX_FEATURE_SENSORS)
				instance->sensor_len = 4;
			else
				instance->sensor_len = 6;
		} else if (instance->feature_sensors[2] == sensor) {
			reg = SR_FEATURE2;
			instance->sensor_len = 4;
		} else {
			INSTANCE_ERROR(DE_SENSOR_NOT_PRESENT);
			return FALSE;
		}
		break;
	default:
		INSTANCE_ERROR(DE_SENSOR_NOT_PRESENT);
		return FALSE;
	}

	instance->sensor_rlen = 0;
	instance->sensor_reg = reg;
	if (!i2c_read_start(instance->i2c_handle, reg, instance->sensor_buf, instance->sensor_len))
	{
		INSTANCE_ERROR(DE_I2C_ERROR);
		return FALSE;
	}
	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}


u32 uint16_reg_to_microseconds(u8 *buf)
{
	insane_log("u16_to_us: [ 0x%02X, 0x%02X ]", buf[0], buf[1]);
	return ((u32)((((u16)buf[0]) + (((u16)buf[1]) << 8))) * SENSOR_SCALE_TIME_NUM) / SENSOR_SCALE_TIME_DEN;
}


bool di_query_sensor_finish(DI_INSTANCE_T *instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_INT_DATA_T *data, bool force)
{
	u8 *buf = instance->sensor_buf;
#if DEBUG>2
	u8 *pbuf = instance->sensor_buf;
	u16 plen = instance->sensor_len;
#endif
	DI_3AXIS_INT_DATA_T *p3 = NULL;
	int feature_num = 0;
	u32 t;
	u32 prev_t;
	DI_SENSOR_TYPE_T cur_sensor;
	int num_sensors;
	int i;
	int ofs;
	int tm_ofs;
	static const DI_SENSOR_TYPE_T sensor_list[4 + MAX_FEATURE_SENSORS] = {DST_QUATERNION, DST_MAG, DST_ACCEL, DST_GYRO,
			DST_NONE, DST_NONE, DST_NONE};
	static const int offsets[4] = {SR_QX - SR_QX, SR_MX - SR_QX, SR_AX - SR_QX, SR_GX - SR_QX};
	static const int data_offsets_1_feature[1] = {SR_FEATURE0 - SR_QX};
	static const int time_offsets_1_feature[1] = {SR_FEATURE1 - SR_QX};
	static const int data_offsets_2_feature[2] = {SR_FEATURE0 - SR_QX, SR_FEATURE2 - SR_QX};
	static const int time_offsets_2_feature[2] = {SR_FEATURE1 - SR_QX, SR_FEATURE3 - SR_QX};
	static const int data_offsets_3_feature[3] = {SR_FEATURE0 - SR_QX, SR_FEATURE1 - SR_QX, SR_FEATURE2 - SR_QX};
	static const int time_offsets_3_feature[3] = {SR_FEATURE3 - SR_QX, SR_FEATURE3 - SR_QX, SR_FEATURE3 - SR_QX};
	// 1 feature:     data0,          timestamp0,                srvd,                rsvd
	// 2 features:    data0,          timestamp0,               data1,          timestamp1
	// 3 features:    data0,               data1,               data2,           timestamp

	DI_PREAMBLE_FULL(instance);

#if DEBUG>2
	insane_log("# read 0x%02X ", instance->sensor_reg);
	while (plen--) insane_log("0x%02X ", *(pbuf++));
	insane_log("; %u\n", time_ms());
#endif

	if (sensor == DST_ALL) {
		num_sensors = 4 + instance->num_feature_sensors;
		cur_sensor = sensor_list[0];                                   // not strictly needed, but suppresses a warning
	} else {                                                             // just read a single sensor
		num_sensors = 1;
		cur_sensor = sensor;
		ofs = 0;
	}
	for (i = 0; i < num_sensors; i++)
	{
		if (sensor == DST_ALL)
		{
			if (i < 4)
			{
				cur_sensor = sensor_list[i];
				ofs = offsets[i];
			}
			else
				cur_sensor = instance->feature_sensors[i - 4];
		}

		if (!force && !instance->sensor_info[cur_sensor].acquisition_enable)     // higher level code doesn't care about this sensor; ignore it
			continue;

		prev_t = instance->sensor_info[cur_sensor].timestamp_prev;
		switch (cur_sensor)
		{
		case DST_MAG:
			p3 = &data->mag;
			t = uint16_reg_to_microseconds(&buf[ofs + 6]);
			if (t != prev_t)
			{
				if (t < prev_t)                                       // timer wraparound
					instance->sensor_info[cur_sensor].timestamp_base += SENSOR_TIMESTAMP_OVERFLOW_US;
				instance->sensor_info[cur_sensor].timestamp_prev = t; // save for next time
				p3->t = t + instance->sensor_info[cur_sensor].timestamp_base; // correct for wraparound
				p3->x = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8); /// \todo replace these with le16_to_cpu() etc.
				ofs += 2;
				p3->y = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				ofs += 2;
				p3->z = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				p3->valid = TRUE;
			}
			break;
		case DST_ACCEL:
			p3 = &data->accel;
			t = uint16_reg_to_microseconds(&buf[ofs + 6]);
			if (t != prev_t)
			{
				if (t < prev_t)                                       // timer wraparound
					instance->sensor_info[cur_sensor].timestamp_base += SENSOR_TIMESTAMP_OVERFLOW_US;
				instance->sensor_info[cur_sensor].timestamp_prev = t; // save for next time
				p3->t = t + instance->sensor_info[cur_sensor].timestamp_base; // correct for wraparound
				p3->x = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				ofs += 2;
				p3->y = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				ofs += 2;
				p3->z = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				p3->valid = TRUE;
			}
			break;
		case DST_GYRO:
			p3 = &data->gyro;
			t = uint16_reg_to_microseconds(&buf[ofs + 6]);
			if (t != prev_t)
			{
				if (t < prev_t)                                       // timer wraparound
					instance->sensor_info[cur_sensor].timestamp_base += SENSOR_TIMESTAMP_OVERFLOW_US;
				instance->sensor_info[cur_sensor].timestamp_prev = t; // save for next time
				p3->t = t + instance->sensor_info[cur_sensor].timestamp_base; // correct for wraparound
				p3->x = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				ofs += 2;
				p3->y = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				ofs += 2;
				p3->z = (s16)(((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				p3->valid = TRUE;
			}
			break;
		case DST_QUATERNION:
			t = uint16_reg_to_microseconds(&buf[ofs + 16]);
			if (t != prev_t)
			{
				if (t < prev_t)                                       // timer wraparound
					instance->sensor_info[cur_sensor].timestamp_base += SENSOR_TIMESTAMP_OVERFLOW_US;
				instance->sensor_info[cur_sensor].timestamp_prev = t; // save for next time
				data->quaternion.t = t + instance->sensor_info[cur_sensor].timestamp_base; // correct for wraparound
				data->quaternion.x = (s32)(((u32)buf[ofs + 0]) | ((u32)buf[ofs + 1]) << 8 | ((u32)buf[ofs + 2]) << 16 | ((u32)buf[ofs + 3]) << 24); /// \todo: replace with le32_to_cpu() etc.
				ofs += 4;
				data->quaternion.y = (s32)(((u32)buf[ofs + 0]) | ((u32)buf[ofs + 1]) << 8 | ((u32)buf[ofs + 2]) << 16 | ((u32)buf[ofs + 3]) << 24);
				ofs += 4;
				data->quaternion.z = (s32)(((u32)buf[ofs + 0]) | ((u32)buf[ofs + 1]) << 8 | ((u32)buf[ofs + 2]) << 16 | ((u32)buf[ofs + 3]) << 24);
				ofs += 4;
				data->quaternion.w = (s32)(((u32)buf[ofs + 0]) | ((u32)buf[ofs + 1]) << 8 | ((u32)buf[ofs + 2]) << 16 | ((u32)buf[ofs + 3]) << 24);
				data->quaternion.valid = TRUE;
			}
			break;
		case DST_BAROM:
		case DST_HUMID:
		case DST_TEMP:
		case DST_CUST0:
		case DST_CUST1:
		case DST_CUST2:
			// should this be simpler and just always read all feature sensors that are enabled?
			if (instance->feature_sensors[0] == cur_sensor)
				feature_num = 0;
			else if (instance->feature_sensors[1] == cur_sensor)
				feature_num = 1;
			else if (instance->feature_sensors[2] == cur_sensor)
				feature_num = 2;

			if ((instance->num_feature_sensors == 1) && (feature_num < instance->num_feature_sensors))
			{
				ofs = data_offsets_1_feature[feature_num];
				tm_ofs = time_offsets_1_feature[feature_num];
			}
			else if ((instance->num_feature_sensors == 2) && (feature_num < instance->num_feature_sensors))
			{
				ofs = data_offsets_2_feature[feature_num];
				tm_ofs = time_offsets_2_feature[feature_num];
			}
			else if ((instance->num_feature_sensors == 3) && (feature_num < instance->num_feature_sensors))
			{
				ofs = data_offsets_3_feature[feature_num];
				tm_ofs = time_offsets_3_feature[feature_num];
			}
			else
				break;

			t = uint16_reg_to_microseconds(&buf[tm_ofs]);

			if(force)
				t = prev_t + 1;

			if ( t != prev_t )
			{
				if (t < prev_t)                                       // timer wraparound
					instance->sensor_info[cur_sensor].timestamp_base += SENSOR_TIMESTAMP_OVERFLOW_US;
				instance->sensor_info[cur_sensor].timestamp_prev = t; // save for next time

				data->feature[feature_num].t = t + instance->sensor_info[cur_sensor].timestamp_base; // correct for wraparound
				data->feature[feature_num].data = (((u16)buf[ofs + 0]) | ((u16)buf[ofs + 1]) << 8);
				data->feature[feature_num].valid = TRUE;
			}
			break;
		default:
			break;
		}
	}

	INSTANCE_ERROR(DE_NONE);
	return TRUE;
}

#ifdef CONFIG_MANAGE_IRQ
bool i2c_callback(I2C_HANDLE_T handle, TransferStatus complete, u16 len_transferred, void *user_param)
{
	DI_INSTANCE_T *instance = (DI_INSTANCE_T *)user_param;
	if (DI_PRECHECK(instance))
	{
		instance->i2c_interrupt++;                                  // we define the interrupt field as being an atomic integer -- we want the increment to occur in 1 cycle ideally
		instance->sensor_complete = complete;
		instance->sensor_rlen = len_transferred;
	}
	return TRUE;
}


bool irq_callback(IRQ_HANDLE_T handle, u32 os_param, void *user_param)
{
	DI_INSTANCE_T *instance = (DI_INSTANCE_T *)user_param;
	if (DI_PRECHECK(instance))
	{
		instance->host_int_timestamp = time_ms();
		instance->interrupt++;                                      // we define the interrupt field as being an atomic integer -- we want the increment to occur in 1 cycle ideally
		irq_acknowledge(handle);
	}
	return TRUE;
}
#endif

bool di_task_loop(DI_INSTANCE_T *instance, bool *done)
{
#ifdef CONFIG_MANAGE_IRQ
	static ATOMIC_INT int_count = 0;
#endif
	//static ATOMIC_INT i2c_count = 0;
	static u8 status_regs[4];
	static RegEventStatus event_status = {0};
	static int num_features = 0;
	static DI_SENSOR_TYPE_T sensor = DST_NONE;
	//static u32 last_ms = 0;

	if (!DI_PRECHECK_FULL(instance))
	{
		INSTANCE_ERROR(DE_INVALID_INSTANCE);
		instance->state = DS_IDLE;                                     // reset state machine as a side-effect
		return FALSE;
	}

	// call routine used to simulate interrupts on some platforms
	IRQ_CHECK(instance);

	/// @todo: use the done flag
	if (done)
		*done = TRUE;

	insane_log("task loop state: %d\n", instance->state);

	switch (instance->state)
	{
	case DS_IDLE:
#ifdef CONFIG_MANAGE_IRQ
		if (instance->interrupt == int_count)                       // check for recent data ready interrupt
		{
			if ((time_ms() - last_ms) < 1000)                        // if it has been less than 1 second since the last sample, wait for interrupt
				return TRUE;
		}
#endif
	// fall through
	case DS_QUERY:
#ifdef CONFIG_MANAGE_IRQ
		while (int_count != instance->interrupt)                    // loop to prevent race conditions
			int_count = instance->interrupt;
		i2c_count = instance->i2c_interrupt;
#endif
		if (!i2c_read_start(instance->i2c_handle, SR_EVENT_STATUS, status_regs, 4))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			return FALSE;
		}

		IRQ_CHECK(instance);                            // on systems with simulated interrupts, check immediately after reading event status to try to catch deassertion of interrupt
		instance->state = DS_WAIT_EVENT_STATUS;
		break;


	case DS_WAIT_EVENT_STATUS:
#ifdef CONFIG_MANAGE_IRQ
		if (instance->i2c_interrupt == i2c_count)
			return TRUE;
		i2c_count = instance->i2c_interrupt;                        // no need to loop; we presume no other I2C int can occur now
		if ((instance->sensor_complete != TS_I2C_COMPLETE) || (instance->sensor_rlen != 4))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			instance->state = DS_IDLE;
			return FALSE;
		}
#endif
		// we have the 4 main status registers; save them for later
		instance->error.event_status.reg = status_regs[0];
		instance->error.sensor_status.reg = status_regs[1];
		instance->error.sentral_status.reg = status_regs[2];
		instance->error.algo_status.reg = status_regs[3];

		event_status.reg = status_regs[0];

#if defined(DEBUG_EVENTS)
		/** for debugging only, keep a history of recent events */
		event_hist[event_index] = event_status.reg;
		event_time[event_index++] = time_ms();
		if (event_index > EVENT_COUNT)
			event_index = 0;
#endif

		// reading this is asynchronous, so log it manually here
		insane_log("# read 0x%02X 0x%02X ; %u\n", SR_EVENT_STATUS, event_status.reg, time_ms());

		if (instance->read_all_sensor_regs || event_status.bits.FeatureResults)
		{
			if (event_status.bits.CPUReset)
			{
				// INSTANCE_ERROR(DE_UNEXPECTED_RESET);
				// return FALSE;
				event_status.bits.CPUReset = 0;
			}
			if (event_status.bits.Error)
			{
				INSTANCE_ERROR(DE_EM718X_ERROR);
				instance->state = DS_IDLE;
				return FALSE;
			}
			instance->state = DS_READ_ALL_SENSORS;
		}
		else
		{
			instance->state = DS_READ_EACH_SENSOR;
			if (event_status.reg == 0)
				instance->state = DS_IDLE;                            // nothing to do
			if (event_status.bits.FeatureResults)
				num_features = 0;
		}
		break;

	case DS_READ_ALL_SENSORS:
		if (di_query_sensor_start(instance, DST_ALL, TRUE))
			instance->state = DS_WAIT_ALL_SENSOR_DATA;
		else
		{
			instance->state = DS_QUERY;
			return FALSE;
		}
		break;

	case DS_WAIT_ALL_SENSOR_DATA:

#ifdef CONFIG_MANAGE_IRQ
		if (instance->i2c_interrupt == i2c_count)
			return TRUE;
		i2c_count = instance->i2c_interrupt;                        // no need to loop; we presume no other I2C int can occur now
		if ((instance->sensor_complete != TS_I2C_COMPLETE) || (instance->sensor_rlen != instance->sensor_len))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			instance->state = DS_IDLE;
			return FALSE;
		}
#endif
		instance->state = DS_IDLE;                                  // wait for next host interrupt

		di_query_sensor_finish(instance, DST_ALL, &instance->last_sample, FALSE );

#ifndef __KERNEL__
		last_ms = time_ms();
		if (instance->data_callback)
			instance->data_callback(instance, DST_ALL, &instance->last_sample, instance->user_param);
#endif

		break;

	case DS_READ_EACH_SENSOR:
		if (event_status.bits.CPUReset)
		{
			// INSTANCE_ERROR(DE_UNEXPECTED_RESET);
			// return FALSE;
			event_status.bits.CPUReset = 0;
		}
		if (event_status.bits.Error)
		{
			INSTANCE_ERROR(DE_EM718X_ERROR);
			instance->state = DS_IDLE;
			return FALSE;
		}
		if (event_status.bits.QuaternionResult && instance->sensor_info[DST_QUATERNION].acquisition_enable)
		{
			sensor = DST_QUATERNION;
			event_status.bits.QuaternionResult = 0;
		}
		else if (event_status.bits.MagResult && instance->sensor_info[DST_MAG].acquisition_enable)
		{
			sensor = DST_MAG;
			event_status.bits.MagResult = 0;
		}
		else if (event_status.bits.AccelResult && instance->sensor_info[DST_ACCEL].acquisition_enable)
		{
			sensor = DST_ACCEL;
			event_status.bits.AccelResult = 0;
		}
		else if (event_status.bits.GyroResult && instance->sensor_info[DST_GYRO].acquisition_enable)
		{
			sensor = DST_GYRO;
			event_status.bits.GyroResult = 0;
		}
		else if (event_status.bits.FeatureResults)
		{
			sensor = (DI_SENSOR_TYPE_T)instance->feature_sensors[num_features++];
			if (num_features >= instance->num_feature_sensors)
				event_status.bits.FeatureResults = 0;
			if (!instance->sensor_info[sensor].acquisition_enable)
			{
				instance->state = DS_IDLE;
				break;
			}
		}
		else                                                        // all bits are clear
		{
			instance->state = DS_IDLE;
			break;
		}

		if (di_query_sensor_start(instance, sensor, TRUE))
			instance->state = DS_WAIT_SENSOR_DATA;
		else
		{
			instance->state = DS_QUERY;
			return FALSE;
		}
		break;

	case DS_WAIT_SENSOR_DATA:
#ifdef CONFIG_MANAGE_IRQ
		if (instance->i2c_interrupt == i2c_count)
			return TRUE;
		i2c_count = instance->i2c_interrupt;                        // no need to loop; we presume no other I2C int can occur now
		if ((instance->sensor_complete != TS_I2C_COMPLETE) || (instance->sensor_rlen != instance->sensor_len))
		{
			INSTANCE_ERROR(DE_I2C_ERROR);
			instance->state = DS_IDLE;
			return FALSE;
		}
#endif
		instance->state = DS_IDLE;
		di_query_sensor_finish(instance, sensor, &instance->last_sample, FALSE);

#ifdef CONFIG_MANAGE_IRQ
		last_ms = time_ms();
		if (instance->data_callback)
			instance->data_callback(instance, sensor, &instance->last_sample, instance->user_param);
#endif
		break;
	}
	return TRUE;
}



bool di_handle_interrupt(DI_INSTANCE_T *instance)
{
	int loopcount = 0;
	instance->state = DS_QUERY;

	while (instance->state != DS_IDLE) {
		if (loopcount++ > 10) {
			error_log("loopcount exceeded. state machine error");
			return FALSE;
		}

		if (!di_task_loop(instance, NULL)) {
			return FALSE;
		}
		insane_log("IRQ: task loop state: %d", instance->state);
	}

	return TRUE;
}


bool di_pause_task_loop(DI_INSTANCE_T *instance)
{
	while (instance->state != DS_IDLE)
	{
		if (!di_task_loop(instance, NULL))
		{
			return FALSE;
		}
	}
	return TRUE;
}

bool di_get_sensor_value( DI_INSTANCE_T * instance, DI_SENSOR_TYPE_T sensor, DI_SENSOR_INT_DATA_T * value )
{
	memset(value, 0, sizeof(*value));

	if ( !di_query_sensor_start(instance, sensor, FALSE) )
	{
		error_log("failed to read sensor data\n");
		return FALSE;
	}

	if ( !di_query_sensor_finish(instance, sensor, value, TRUE ) )
	{
		error_log("failed to process sensor data\n");
		return FALSE;
	}

	return TRUE;
}



/** @}*/
