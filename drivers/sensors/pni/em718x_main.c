/**
 * \file
 *
 * \authors   Joshua Gleaton <joshua.gleaton@treadlighttech.com>
 *
 * \brief     Linux kernel module entry point and setup.
 *
 *            The kernel module creates a set of /dev/input/event<N> devices, one for each detected
 *            sensor.
 *
 *            Sensors can be identified using the sysfs 'phys' entry created by the input
 *            subsystem (/sys/class/input/event<N>/device/phys).
 *            Typical phys values are "em718x:accel", "em718x:mag", etc.
 *
 *            Sensor configuration is mostly automatic, though there are a few CONFIG_* defines
 *            below to modify default algorithm behavior.
 *            Additionally, sysfs entries are created under the 'sensor' directory for the
 *            input device (/sys/class/input/event<N>/device/sensor/):
 *              - "rate"   : set the acquisition rate in Hz. The sensor may be limited to
 *                           higher or lower rates. If the sensor is operating at a higher
 *                           rate, then this 'rate' specifies the maximum rate at which
 *                           input events will be delivered.
 *              - "enable" : Power the sensor on/off. This may cause dependant sensors to also
 *                           be enabled in which case -- unless manually enabled -- they will
 *                           be shutdown when this sensor is disabled
 *
 *            The input events sent by the driver are listed in em718x_input_events.h
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
 *
 */


#define DEBUG 1
#define DEBUG_I2C 0

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/jiffies.h>

#include <linux/math64.h>
#include <asm/div64.h>
#include <linux/em718x_main.h>
#include <linux/em718x_core.h>
#include <linux/em718x_host.h>
#include <linux/em718x_eeprom.h>
#include <linux/em718x_input_events.h>

#define FIRMWARE_FILENAME "em718x.fw"

#define CONFIG_OUTPUT_HPR                   0
#define CONFIG_OUTPUT_RAWDATA               0
#define CONFIG_OUTPUT_AG6DOF                0
#define CONFIG_OUTPUT_AM6DOF                0
#define CONFIG_OUTPUT_ENU                   0
#define CONFIG_OUTPUT_ENHANCED_STILL_MODE   0


#define INFO(fmt, ...) printk( KERN_INFO "EM718x: " fmt "\n", ## __VA_ARGS__ )

#if DEBUG>0
	#define DMSG(fmt, ...) printk( KERN_DEBUG "EM718x: " fmt "\n", ## __VA_ARGS__ )
#else
	#define DMSG(fmt, ...)
#endif

#if DEBUG>1
	#define INSANE(fmt, ...) printk( KERN_DEBUG "(INSANE) EM718x: " fmt "\n", ## __VA_ARGS__ )
#else
	#define INSANE(fmt, ...)
#endif

/* NOTE: The descriptions may be changed for these sensors, however the name should remain constant
   for upper layer software to find them */

#define SENSOR_ACCEL_NAME			"accel"
#define SENSOR_ACCEL_DESC			"SENtral Accelerometer"
#define SENSOR_ACCEL_VERSION		3
#define SENSOR_ACCEL_FLAGS			0

#define SENSOR_GYRO_NAME			"gyro"
#define SENSOR_GYRO_DESC			"SENtral Gyroscope"
#define SENSOR_GYRO_VERSION			4
#define SENSOR_GYRO_FLAGS			0

#define SENSOR_MAG_NAME				"mag"
#define SENSOR_MAG_DESC				"SENtral Magnetometer"
#define SENSOR_MAG_VERSION			2
#define SENSOR_MAG_FLAGS			0

#define SENSOR_QUAT_NAME			"quat"
#define SENSOR_QUAT_DESC			"SENtral Quaternion"
#define SENSOR_QUAT_VERSION			1
#define SENSOR_QUAT_FLAGS			0

#define SENSOR_BARO_NAME			"baro"
#define SENSOR_BARO_DESC			"SENtral Pressure"
#define SENSOR_BARO_VERSION			1
#define SENSOR_BARO_FLAGS			0

#define SENSOR_HUMID_NAME			"humid"
#define SENSOR_HUMID_DESC			"SENtral Humidity"
#define SENSOR_HUMID_VERSION		1
#define SENSOR_HUMID_FLAGS			0

#define SENSOR_TEMP_NAME			"temp"
#define SENSOR_TEMP_DESC			"SENtral Temperature"
#define SENSOR_TEMP_VERSION			1
#define SENSOR_TEMP_FLAGS			0

#define SENSOR_CUST0_NAME			"cust0"
#define SENSOR_CUST0_DESC			"SENtral Step Counter"
#define SENSOR_CUST0_VERSION		1
#define SENSOR_CUST0_FLAGS			0

#define SENSOR_CUST1_NAME			"cust1"
#define SENSOR_CUST1_DESC			"SENtral Tilt"
#define SENSOR_CUST1_VERSION		1
#define SENSOR_CUST1_FLAGS			0

#define SENSOR_CUST2_NAME			"cust2"
#define SENSOR_CUST2_DESC			"N/A"
#define SENSOR_CUST2_VERSION		1
#define SENSOR_CUST2_FLAGS			0

struct em718x *emdev = NULL;

#define REGISTER_COUNT 0xa1

static int PNI_major = 0;
#define DEVICE_NAME "em718x"
static struct class *PNI_class = NULL;
static dev_t PNI_dev;
struct device *PNI_class_dev = NULL;

static inline void em718x_lock(struct em718x * emdev)
{
	mutex_lock(&emdev->mutex);
}

static inline void em718x_unlock(struct em718x *emdev)
{
	mutex_unlock(&emdev->mutex);
}


//static int em718x_read(struct em718x *emdev, u8 reg)
//{
//	struct i2c_client *client = to_i2c_client(emdev->dev);
//	int ret;
//
//	ret = i2c_smbus_read_byte_data(client, reg);
//
//	INSANE("READ REG 0x%x: 0x%x (%d)", reg, ret, ret);
//
//	return ret;
//}

static int em718x_write(struct em718x *emdev, u8 reg, u8 val)
{
	struct i2c_client *client = to_i2c_client(emdev->dev);

#if DEBUG_I2C>0
	INSANE("WRITE REG 0x%x=0x%x (%d)", reg, val, val);
#endif

	return i2c_smbus_write_byte_data(client, reg, val);
}

static int em718x_read_block( struct em718x * emdev, u8 reg, int count, void * buf )
{
	struct i2c_client *client = to_i2c_client(emdev->dev);
	u8 * p = buf;
	int total = count;

	while (count) {
		int ret = i2c_smbus_read_i2c_block_data(client, reg, count, p);

#if DEBUG_I2C>0
		INSANE("READ BLOCK @ 0x%x %d/%d : %d", reg, count, total, ret);
#endif

		if (ret <= 0) {
			printk(KERN_ERR "error reading reg 0x%x: count:%d \n", reg, count);
			return ret;
		}

		count -= ret;
		reg += ret;
		p += ret;
	}

	return total;
}

static int em718x_write_block( struct em718x * emdev, u8 reg, int count, const void * buf )
{
	struct i2c_client *client = to_i2c_client(emdev->dev);
	int ret = i2c_smbus_write_i2c_block_data(client, reg, count, buf);

#if DEBUG_I2C>0
	int i = 0;
	u8 *pbuf = (u8*)buf;

	INSANE("WRITE BLOCK: reg 0x%x count:%d : %d", reg, count, ret);
	while (i < count) {
		INSANE("WRITE BLOCK: 0x%02X", pbuf[i++]);
	}
#endif

	if (ret)
		printk(KERN_ERR "failed to write block: %d\n", ret);


	return ret;
}

/** driver_core required functions */
u32 time_ms(void)
{
	u64 ms = get_jiffies_64() * (1000 / HZ);
	//u64 ms = (get_jiffies_64() * 1000) / HZ;
	return ms;
}

bool i2c_blocking_read(I2C_HANDLE_T handle, u8 reg_addr, u8 *buffer, u16 len)
{
	return em718x_read_block( (struct em718x*) handle, reg_addr, len, buffer) == len;
}


bool i2c_blocking_write(I2C_HANDLE_T handle, u8 reg_addr, u8 *buffer, u16 len)
{
	return em718x_write_block( (struct em718x*) handle, reg_addr, len, buffer) == 0;
}

bool i2c_blocking_write_read(I2C_HANDLE_T handle, u8 *wbuffer, u16 wlen, u8 *rbuffer, u16 rlen)
{
	printk(KERN_ERR "write/read not supported\n");
	return FALSE;
}


bool i2c_read_start(I2C_HANDLE_T handle, u8 reg_addr, u8 *buffer, u16 len)
{
	return em718x_read_block( (struct em718x*) handle, reg_addr, len, buffer) == len;
}

bool i2c_write_start(I2C_HANDLE_T handle, u8 reg_addr, u8 *buffer, u16 len)
{

	return em718x_write_block( (struct em718x*) handle, reg_addr, len, buffer) == 0;
}


bool i2c_write_read_start(I2C_HANDLE_T handle, u8 *wbuffer, u16 wlen, u8 *rbuffer, u16 rlen)
{
	DMSG("write_read_start not implemented");
	return FALSE;
}


void record_error(DI_INSTANCE_T *instance, DI_ERROR_CODE_T error, int line, const char *fn)
{
	if (error!=DE_NONE)
		printk(KERN_ERR "DIERROR: %s:%d %u\n", fn, line, error);

}


u64 em718x_gettime_us(struct em718x * emdev )
{
	u64 us = (get_jiffies_64() - emdev->timebase) * (1000000 / HZ);
	//u64 us = ( (get_jiffies_64() - emdev->timebase) * 1000000) / HZ;
	return us;
}



/*********************************************************************
  Sensor Interrupt Handlers

	Called when a sensor interrupt occurs

*********************************************************************/

static bool _em718x_sensor_ready( struct em718x_sensor * sensor, u32 t )
{
	s64 dt = 0;
	u64 timestamp;

	if ( !sensor->info->acquisition_enable )
		return FALSE;

	if ( sensor->rate == 0 ) {
		printk(KERN_ERR "sensor %s enabled with rate=0\n", sensor->name );
		return FALSE;
	}

	if (sensor->flags & SENSOR_FLAG_LOWHZ)
		timestamp = em718x_gettime_us(sensor->dev);
	else {
		timestamp = t;
		if ( timestamp < sensor->timestamp ) /* counter wrapped */
			timestamp += UINT_MAX;
	}

//	INSANE("sensor %s: enabled:%d t:%u timestamp:%llu sensortime:%llu",
//				sensor->name, sensor->info->acquisition_enable, t, timestamp, sensor->timestamp);

	dt = timestamp - sensor->timestamp;

	sensor->timestamp = timestamp;

	return TRUE;

	if ( dt >= sensor->dt ) {
		sensor->dt = 1000000 / sensor->rate;
		INSANE("%s: sample dt: %lld. sending sample. resetting sensor dt to %lld", sensor->name, dt, sensor->dt);
		return TRUE;
	}

	INSANE("%s: sample dt: %lld/%lld", sensor->name, dt, sensor->dt);

	sensor->dt -= dt;
	return FALSE;
}

static inline void send_event( struct input_dev * input, u32 code, u32 value )
{
	input_event(input, INPUT_EVENT_TYPE, code, value);
}

static void _em718x_sensor_3axes_notify( struct em718x_sensor * sensor, DI_3AXIS_INT_DATA_T * d )
{
	struct input_dev * input = sensor->input;

	if (!d->valid)
		return;

	if (!_em718x_sensor_ready( sensor, d->t ) )
		return;
	//send_event( input, INPUT_EVENT_XY, (((u32)d->y)<<16) | d->x);
	send_event(input, INPUT_EVENT_XY, (((u32)d->y)<<16) | (((u32)d->x)&0xffff));
	send_event(input, INPUT_EVENT_ZW, d->z);
	send_event(input, INPUT_EVENT_TIME_MSB, (u32)(sensor->timestamp >> 32));
	send_event(input, INPUT_EVENT_TIME_LSB, (u32)(sensor->timestamp & 0xffffffff));
	input_sync(input);

	d->valid = FALSE;
}


static void _em718x_sensor_accel_notify( struct em718x_sensor * sensor,  DI_SENSOR_INT_DATA_T* data )
{
	_em718x_sensor_3axes_notify( sensor, &data->accel );
}
static void _em718x_sensor_mag_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data )
{
	_em718x_sensor_3axes_notify( sensor, &data->mag );
}
static void _em718x_sensor_gyro_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
	_em718x_sensor_3axes_notify( sensor, &data->gyro );
}

static void _em718x_sensor_quat_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
	struct input_dev    * input = sensor->input;
	DI_QUATERNION_INT_T * d = &data->quaternion;

	if (!d->valid)
		return;

	if (!_em718x_sensor_ready( sensor, d->t ) )
		return;

	send_event( input, INPUT_EVENT_X, d->x);
	send_event( input, INPUT_EVENT_Y, d->y);
	send_event( input, INPUT_EVENT_Z, d->z);
	send_event( input, INPUT_EVENT_W, d->w);
	send_event(input, INPUT_EVENT_TIME_MSB, (u32)(sensor->timestamp >> 32));
	send_event(input, INPUT_EVENT_TIME_LSB, (u32)(sensor->timestamp & 0xffffffff));
	input_sync( input );

	d->valid = FALSE;
}
static void _em718x_sensor_baro_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
}
static void _em718x_sensor_temp_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
}
static void _em718x_sensor_humid_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
}

static void _em718x_sensor_cust0_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
	struct input_dev  * input = sensor->input;
	DI_FEATURE_DATA_T * d = &data->feature[0];

//	INSANE("sensor:%s valid:%d", sensor->name, d->valid );

	if (!d->valid)
		return;

	if (!_em718x_sensor_ready( sensor, d->t ) )
		return;

	send_event( input, INPUT_EVENT_XY, d->data);
	send_event(input, INPUT_EVENT_TIME_MSB, (u32)(sensor->timestamp >> 32));
	send_event(input, INPUT_EVENT_TIME_LSB, (u32)(sensor->timestamp & 0xffffffff));
	input_sync( input );

	d->valid = FALSE;
}
static void _em718x_sensor_cust1_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
	struct input_dev  * input = sensor->input;
	DI_FEATURE_DATA_T * d = &data->feature[1];

//	INSANE("sensor:%s valid:%d", sensor->name, d->valid );

	if (!d->valid)
		return;

	if (!_em718x_sensor_ready( sensor, d->t ) )
		return;

	send_event( input, INPUT_EVENT_XY, d->data );
	send_event(input, INPUT_EVENT_TIME_MSB, (u32)(sensor->timestamp >> 32));
	send_event(input, INPUT_EVENT_TIME_LSB, (u32)(sensor->timestamp & 0xffffffff));
	input_sync( input );

	d->valid = FALSE;
}
static void _em718x_sensor_cust2_notify( struct em718x_sensor * sensor, DI_SENSOR_INT_DATA_T* data  )
{
	struct input_dev  * input = sensor->input;
	DI_FEATURE_DATA_T * d = &data->feature[2];

	if (!d->valid)
		return;

	if (!_em718x_sensor_ready( sensor, d->t ) )
		return;

	send_event( input, INPUT_EVENT_XY, d->data);
	send_event(input, INPUT_EVENT_TIME_MSB, (u32)(sensor->timestamp >> 32));
	send_event(input, INPUT_EVENT_TIME_LSB, (u32)(sensor->timestamp & 0xffffffff));
	input_sync( input );

	d->valid = FALSE;
}

static irqreturn_t em718x_irq_thread(int irq, void *handle)
{
	struct em718x *emdev = handle;
	static int k = 0;

	em718x_lock(emdev);
	if (k >= 0) {
		INSANE("irq enter");
		k = 0;
	} else
		k++;

#if 0
	if (emdev->host_enabled == 0) {
		if ((++emdev->interrupt_errors % 100) == 0) {
			printk(KERN_ERR "received %u interrupts while host disabled\n", emdev->interrupt_errors);
			em718x_unlock(emdev);
			return IRQ_HANDLED;
		}
	}
#endif

	if (di_handle_interrupt(emdev->di)) {

#define SENSOR_NOTIFY(t)                                                             \
		if (emdev->sensors[t].notify) {                                             \
			emdev->sensors[t].notify( &emdev->sensors[t], &emdev->di->last_sample ); \
		}                                                                            \

		SENSOR_NOTIFY(DST_ACCEL);
		SENSOR_NOTIFY(DST_MAG);
		SENSOR_NOTIFY(DST_GYRO);
		SENSOR_NOTIFY(DST_QUATERNION);
		SENSOR_NOTIFY(DST_CUST0);
		SENSOR_NOTIFY(DST_CUST1);
	}

	em718x_unlock(emdev);

	return IRQ_HANDLED;
}





/**
 * \brief Called by the kernel when the input device file for the sensor is opened
 * \param sensor - The sensor to open
 * \return bool - 0 on success, <1 on error
 */
static int em718x_sensor_open( struct em718x_sensor * sensor )
{
	return 0;
}

/**
 * \brief Called by the kernel when the input device file for the sensor is closed
 * \param sensor - The sensor to open
 * \return bool - 0 on success, <1 on error
 */
static void em718x_sensor_close( struct em718x_sensor * sensor )
{
}

/**
 * \brief Called by the kernel when the input device file for the sensor is opened
 * \param input - The input device to open
 * \return bool - 0 on success, <1 on error
 */

static int em718x_input_sensor_open( struct input_dev * input )
{
	struct em718x_sensor * sensor = input_get_drvdata(input);
	return em718x_sensor_open( sensor );
}

/**
 * \brief Called by the kernel when the input device file for the sensor is opened
 * \param sensor - The input device to close
 * \return bool - 0 on success, <1 on error
 */
static void em718x_input_sensor_close( struct input_dev * input )
{
	struct em718x_sensor * sensor = input_get_drvdata(input);
	return em718x_sensor_close( sensor );
}


/**
 * \brief Called by the kernel when userspace reads the "enable" sysfs file
 * \param dev - The device that owns the sysfs file, in this case an input_dev device
 * \param attr - The attribute being requested
 * \param buf - The character buffer to format the response into
 * \return bool - number of characters in buf on success, <1 on error
 */
static ssize_t em718x_sensor_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct em718x_sensor *sensor = dev_get_drvdata(dev);
	u8 val = 0;

	em718x_lock(sensor->dev);

	val = di_is_sensor_acquisition_enabled(sensor->dev->di, sensor->type);

	em718x_unlock(sensor->dev);

	DMSG("sensor %s enable: %d", sensor->name, val);

	val = (val > 0);

	return sprintf(buf, "%u\n", val );
}

/**
 * \brief Called by the kernel when userspace writes the "enable" sysfs file
 * \param dev - The device that owns the sysfs file, in this case an input_dev device
 * \param attr - The attribute being requested
 * \param buf - The character buffer to read the ascii value from
 * \param count - The number of bytes in buf
 * \return bool - number of characters in buf on success, <1 on error
 */
static ssize_t em718x_sensor_enable_store(struct device *dev, struct device_attribute *attr,
										const char *buf, size_t count)
{
	struct em718x_sensor *sensor = dev_get_drvdata(dev);
	unsigned long enable;
	int ret;
	bool enabled = 0;

	ret = strict_strtoul(buf, 10, &enable);

	if (ret)
		return ret;

	DMSG("set sensor %s enable: %ld", sensor->name, enable);

	em718x_lock(sensor->dev);

	enabled = di_is_sensor_acquisition_enabled(sensor->dev->di, sensor->type);

	if (enabled == enable)
		goto unlock;

	if (!di_enable_sensor_acquisition(sensor->dev->di, sensor->type, enable))
		goto unlock;

	if (enable) {

		if (sensor->rate == 0)
			sensor->rate = 1;

		sensor->dt = 0;

		if ( sensor->flags & SENSOR_FLAG_UPDATE_ON_ENABLE ) {
			DI_SENSOR_INT_DATA_T value;
			if ( di_get_sensor_value(sensor->dev->di, sensor->type, &value) ) {
				if (sensor->notify)
					sensor->notify(sensor, &value);
			}
		}
	}

unlock:
	em718x_unlock(sensor->dev);

	return count;
}


/**
 * \brief Called by the kernel when userspace reads the "rate" sysfs file
 * \param dev - The device that owns the sysfs file, in this case an input_dev device
 * \param attr - The attribute being requested
 * \param buf - The character buffer to format the response into
 * \return bool - number of characters in buf on success, <1 on error
 */
static ssize_t em718x_sensor_rate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct em718x_sensor *sensor = dev_get_drvdata(dev);
	u16 val;

	em718x_lock(sensor->dev);

	val = sensor->rate;

	em718x_unlock(sensor->dev);

	DMSG("sensor %s rate %d", sensor->name, val);

	return sprintf(buf, "%u\n", val );
}

/**
 * \brief Called by the kernel when userspace writes the "rate" sysfs file
 * \param dev - The device that owns the sysfs file, in this case an input_dev device
 * \param attr - The attribute being requested
 * \param buf - The character buffer to read the ascii value from
 * \param count - The number of bytes in buf
 * \return bool - number of characters in buf on success, <1 on error
 */
static ssize_t em718x_sensor_rate_store(struct device *dev, struct device_attribute *attr,
										const char *buf, size_t count)
{
	struct em718x_sensor *sensor = dev_get_drvdata(dev);

	unsigned long val;
	int error;

	error = strict_strtoul(buf, 10, &val);

	if (error)
		return error;

	DMSG("set sensor %s rate %ld", sensor->name, val);

	em718x_lock(sensor->dev);

	if (val < 1)
		val = 1;

	if (di_request_rate( sensor->dev->di, sensor->type, val)) {
		sensor->rate = val;
		sensor->dt = 0;
	}

	em718x_unlock(sensor->dev);

	return count;
}

static ssize_t em718x_set_register(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct em718x *emdev = dev_get_drvdata(dev);
	char op;
	s32 addr;
	s32 value;
	int rc = 0;
	if (3 != sscanf(buf, "%c,%i,%i", &op, &addr, &value))
		return -EINVAL;
	DMSG("em718x_set_register: %c, %i, %i", op, addr, value);
	if (op == 'r' && value && value <= 32) {
		if (emdev) {
			u8 regs[value];
			u8 buf[32];
			char str[sizeof(buf) * 3 + 1];
			char *p;
			int i;

			em718x_lock(emdev);
			em718x_read_block(emdev, addr, value, regs);
			em718x_unlock(emdev);

			for (i = 0, p = str; i < value; i++) {
				DMSG("em718x_set_register->read: reg: 0x%02X, value: 0x%02X", i, regs[i]);
				p += scnprintf(p, sizeof(str) - (unsigned)(p - str),
								"%02x ", regs[i]);
			}
			dev_info(dev, "%s\n", str);
			rc = value;
		}
	} else if (op == 'w') {
		DMSG("em718x_set_register->write: reg: 0x%02X, value: 0x%02X", addr, value);

		em718x_lock(emdev);
		em718x_write(emdev, addr, value);
		em718x_unlock(emdev);
		rc = 1;
	} else
		rc = -EINVAL;

	return rc;
}

static DEVICE_ATTR(reg, S_IWUGO, NULL, em718x_set_register);


static ssize_t em718x_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct em718x *emdev = dev_get_drvdata(dev);
	u8 regs[6];
	int reg_index = 0;
	int bit_index = 0;
	int count = 0;
	const char *empty_string = "";

	const char reg_names[6][16] =
	{
		"EnableEvents",
		"HostControl",
		"EventStatus",
		"SensorStatus",
		"SENtralStatus",
		"AlgorithmStatus"
	};

	const char reg_bit_names[6][8][17] =
	{
		{ "CPUReset", "Error", "QuaternionResult", "MagResult", "AccelResult", "GyroResult", "FeatureResult", "CustResult" },
		{ "RunEnable", "HostUpload", "", "", "", "", "", "" },
		{ "CPUReset", "Error", "QuaternionResult", "MagResult", "AccelResult", "GyroResult", "FeatureResult", "CustResult" },
		{ "MagNACK", "AccelNACK", "GyroNACK", "MagDeviceIdErr", "AccelDeviceIdErr", "GyroDeviceIdErr", "", "" },
		{ "EEPROM", "EEUploadDone", "EEUploadError", "Idle", "NoEEPROM", "", "", "" },
		{ "StandbyStatus", "AlgorithmSlow", "Stillness", "CalStable", "", "", "", "" }
	};

	em718x_lock(emdev);
	em718x_read_block(emdev, SR_ENABLE_EVENTS, 6, regs);
	em718x_unlock(emdev);

	for (reg_index = 0; reg_index < 6; reg_index++) {
		count += scnprintf(buf + count, PAGE_SIZE - count, "== %s ==\n", reg_names[reg_index]);
		for (bit_index = 0; bit_index < 8; bit_index++) {
			if (strcmp(reg_bit_names[reg_index][bit_index], empty_string))
				count += scnprintf(buf + count, PAGE_SIZE - count, "%-16s: %s\n", reg_bit_names[reg_index][bit_index], (regs[reg_index] & 1 << bit_index) ? "true" : "false");
		}
	}
	return count;
}

static DEVICE_ATTR(status, S_IRUGO, em718x_status_show, NULL);

static struct attribute *em718x_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_reg.attr,
	NULL
};

static const struct attribute_group em718x_attribute_group = {
	.attrs = em718x_attributes
};

/**
 * \brief Called by probe() to load firmware onto the SFP
 * \param emdev - The em718x device
 * \param filename - The name of the firmware file to request from userspace
 * \return bool - number of characters in buf on success, <1 on error
 */
static int em718x_load_firmware( struct em718x * emdev, const char * device_name )
{
	const struct firmware * fw = 0;
	int ret;
	EEPROMHeader *header;
	u8 *data;
	u32 size;
	char filename[128];

	snprintf(filename, 127, "%s.fw", device_name);
	filename[127] = 0;

	em718x_lock(emdev);

	ret = request_firmware(&fw, filename, emdev->dev);

	if (ret) {
		printk(KERN_ERR "failed to find firmware file %s: %d\n", filename, ret);
		goto unlock;
	}

	DMSG("found firmware size: %d", fw->size);

	if (fw->size < sizeof(*header)) {
		printk(KERN_ERR "not large enough for firmware\n");
		ret = -ENODEV;
		goto end_upload;
	}

	header = (EEPROMHeader*) fw->data;

	if (header->magic != EEPROM_MAGIC_VALUE) {
		printk(KERN_ERR "invalid firmware magic: %x\n", header->magic);
		ret = -ENODEV;
		goto end_upload;
	}

	if ((sizeof(EEPROMHeader) + header->text_length) > fw->size) {
		printk(KERN_ERR "firmware too large\n");
		ret = -ENODEV;
		goto end_upload;
	}


	if (!di_reset_sentral(emdev->di, TRUE)) {
		printk(KERN_ERR "failed to reset device\n");
		goto end_upload;
	}

	if (!di_upload_firmware_start(emdev->di, header)) {
		printk(KERN_ERR "failed to start upload\n");
		ret = -ENODEV;
		goto end_upload;
	}

	data = ((u8*)fw->data) + sizeof(*header);
	size = fw->size - sizeof(*header);

	if (!di_upload_firmware_data(emdev->di, (u32*) data, size/4)) {
		printk(KERN_ERR "failed to upload data\n");
		ret = -ENODEV;
		goto end_upload;
	}

	if (!di_upload_firmware_finish(emdev->di)) {
		printk(KERN_ERR "failed to finish upload\n");
		ret = -ENODEV;
		goto end_upload;
	}

	DMSG("firmware upload completed successfully");

//	ret = _em718x_detect(emdev);

	//mdelay(1000);

end_upload:
	release_firmware(fw);

	emdev->timebase = get_jiffies_64();

	di_configure_output(emdev->di,
						CONFIG_OUTPUT_HPR,
						CONFIG_OUTPUT_RAWDATA,
						CONFIG_OUTPUT_AG6DOF,
						CONFIG_OUTPUT_AM6DOF,
						CONFIG_OUTPUT_ENU,
						CONFIG_OUTPUT_ENHANCED_STILL_MODE);


	if (!di_run_request(emdev->di)) {
		printk(KERN_ERR "run_request failed\n");
		ret = -ENODEV;
		goto unlock;
	}

	if ( !di_detect_sentral_simple( emdev->di ) ) {
		printk(KERN_ERR "detect error\n");
		ret = -ENODEV;
		goto unlock;
	}

	if (!di_query_features(emdev->di)) {
		printk(KERN_ERR "query_features failed\n");
		ret = -ENODEV;
		goto unlock;
	}

	if (!di_shutdown_request(emdev->di)) {
		printk(KERN_ERR "failed to shutdown sensors\n");
		ret = -ENODEV;
		goto unlock;
	}

	if (!di_query_status(emdev->di, NULL, NULL)) {
		printk(KERN_ERR "failed to query status\n");
		ret = -ENODEV;
		goto unlock;
	}

unlock:
	em718x_unlock(emdev);


	return ret;
}



void em718x_dump_registers(struct seq_file *s)
{
	u8 regs[REGISTER_COUNT];

	if (emdev) {
		int i;
/*
//==================Pass through mode======================================
		u8 buf_tem[1];
		buf_tem[0]=1;

		if (!i2c_blocking_write(emdev, SR_ALGO_CONTROL, (u8 *)buf_tem, 1))
		{
			printk("em718x : write reg error :  0x54 \n");
		}
		if (!i2c_blocking_write(emdev, SR_PASSTHRU_CONTROL, (u8 *)buf_tem, 1))
		{
			printk("em718x : write reg error :  0xA0 \n");
		}
//==================Pass through mode======================================
*/
		em718x_lock(emdev);

		em718x_read_block(emdev, 0, REGISTER_COUNT-1, regs);

		for(i=0; i<REGISTER_COUNT; i++)
			seq_printf(s, "0x%02X: 0x%02X\n", i, regs[i]);

		em718x_unlock(emdev);
	}
}


static int em718x_debug_show(struct seq_file *s, void *unused)
{
	void (*func)(struct seq_file *) = s->private;
	func(s);
	return 0;
}

static int em718x_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, em718x_debug_show, inode->i_private);
}

static const struct file_operations em718x_debug_fops = {
	.open           = em718x_debug_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static struct dentry *em718x_debugfs_dir;

static int em718x_init_debugfs(void)
{
	em718x_debugfs_dir = debugfs_create_dir("em718x", NULL);
	if (IS_ERR(em718x_debugfs_dir)) {
		int err = PTR_ERR(em718x_debugfs_dir);
		em718x_debugfs_dir = NULL;
		return err;
	}

	debugfs_create_file("registers", S_IRUGO, em718x_debugfs_dir,
			&em718x_dump_registers, &em718x_debug_fops);

	return 0;
}

static void em718x_free_debugfs(void)
{
	if (em718x_debugfs_dir)
		debugfs_remove_recursive(em718x_debugfs_dir);
}


static ssize_t PNI_FW_UPLOAD(struct device *device,
					struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(emdev->dev);
	int ret = 0;
	int i = 0;

	// allow the load to fail... there may still be eeprom
	if (em718x_load_firmware(emdev, client->name))
		printk("em718x_load_firmware ok\n");


	if (sysfs_create_group(&emdev->dev->kobj, &em718x_attribute_group))
		printk(KERN_ERR "could not create dev sysfs\n");


	/** allocate and initialize sensor and input devices */
	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++) {
		struct em718x_sensor * sensor = &emdev->sensors[i];
		struct input_dev * input;

		sensor->dev = emdev;

		if (!di_has_sensor( emdev->di, i) )
			continue;

		DMSG("initializing sensor %d/%d", i, DST_NUM_SENSOR_TYPES);

		input = input_allocate_device();
		if (!input) {
			printk(KERN_ERR "failed to allocate input device\n");
			continue;
		}

		sensor->input       = input;
		sensor->info        = &emdev->di->sensor_info[i];
		sensor->type        = i;
		sensor->rate        = 1;
		sysfs_attr_init(&sensor->attr_rate);
		sysfs_attr_init(&sensor->attr_enable);
		sensor->attr_rate.attr.name = "rate";
		sensor->attr_rate.attr.mode = 0666;
		sensor->attr_rate.show = em718x_sensor_rate_show;
		sensor->attr_rate.store = em718x_sensor_rate_store;
		sensor->attr_enable.attr.name = "enable";
		sensor->attr_enable.attr.mode = 0666;
		sensor->attr_enable.show = em718x_sensor_enable_show;
		sensor->attr_enable.store = em718x_sensor_enable_store;
		sensor->attrs[0] = &sensor->attr_rate.attr;
		sensor->attrs[1] = &sensor->attr_enable.attr;
		sensor->attrs_group.name = "sensor";
		sensor->attrs_group.attrs = sensor->attrs;

		input->phys         = sensor->path;
		input->dev.parent   = emdev->dev;
		input->id.product   = emdev->di->rom_version;
		input->id.vendor    = emdev->di->product_id;
		input->id.version   = emdev->di->ram_version;
		input->id.bustype   = BUS_I2C;
		input->open         = em718x_input_sensor_open;
		input->close        = em718x_input_sensor_close;
		input_set_drvdata(input,sensor);

		switch (i) {

#define setup_sensor(l,u)                                     \
			sensor->notify  = _em718x_sensor_##l##_notify;    \
			sensor->name    = SENSOR_##u##_NAME;              \
			sensor->desc    = SENSOR_##u##_DESC;              \
			sensor->flags   = SENSOR_##u##_FLAGS;             \
			input->name     = SENSOR_##u##_DESC;

#define setup_event_type                                      \
			__set_bit( INPUT_EVENT_TYPE, input->evbit );

#define setup_msc_16()                                        \
			setup_event_type                                  \
			__set_bit( INPUT_EVENT_TIME_MSB, input->mscbit ); \
			__set_bit( INPUT_EVENT_TIME_LSB, input->mscbit ); \
			__set_bit( INPUT_EVENT_XY, input->mscbit );       \
			__set_bit( INPUT_EVENT_ZW, input->mscbit );

#define setup_msc_32()                                        \
			setup_event_type                                  \
			__set_bit( INPUT_EVENT_TIME_MSB, input->mscbit ); \
			__set_bit( INPUT_EVENT_TIME_LSB, input->mscbit ); \
			__set_bit( INPUT_EVENT_X, input->mscbit );        \
			__set_bit( INPUT_EVENT_Y, input->mscbit );        \
			__set_bit( INPUT_EVENT_Z, input->mscbit );        \
			__set_bit( INPUT_EVENT_W, input->mscbit );

			case DST_QUATERNION:
				setup_sensor(quat, QUAT)
				setup_msc_32();
				break;

			case DST_MAG:
				setup_sensor(mag, MAG)
				setup_msc_16();
				break;

			case DST_ACCEL:
				setup_sensor(accel, ACCEL)
				setup_msc_16();
				break;

			case DST_GYRO:
				setup_sensor(gyro,  GYRO)
				setup_msc_16();
				break;

			case DST_BAROM:
				setup_sensor(baro,  BARO)
				setup_msc_16();
				break;

			case DST_HUMID:
				setup_sensor(humid, HUMID)
				setup_msc_16();
				break;

			case DST_TEMP:
				setup_sensor(temp, TEMP)
				setup_msc_16();
				break;

			case DST_CUST0:
				setup_sensor(cust0, CUST0)
				setup_msc_16();
				break;

			case DST_CUST1:
				setup_sensor(cust1, CUST1)
				setup_msc_16();
				break;

			case DST_CUST2:
				setup_sensor(cust2, CUST2)
				setup_msc_16();
				break;

			default:
				printk(KERN_ERR "unknown sensor type: %d\n", i);
		}

		snprintf(sensor->path, SENSOR_PATH_MAX, "em718x:%s", sensor->name);
	}

	/* register input devices */
	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++) {
		struct em718x_sensor * sensor = &emdev->sensors[i];
		struct input_dev *input = sensor->input;

		if (!input)
			continue;

		DMSG("registering input device %s", emdev->sensors[i].name);

		ret = input_register_device(input);
		if (ret) {
			printk(KERN_ERR "failed to register input for device %s\n", sensor->name);
			input_free_device( input );
			sensor->input = NULL;
		}

		if ( sysfs_create_group(& input->dev.kobj, &emdev->sensors[i].attrs_group) ) {
			printk(KERN_ERR "failed to create sysfs rate files for sensor %s\n", sensor->name);
			input_unregister_device(input);
			emdev->sensors[i].input = NULL;
		}
	}

	ret = em718x_init_debugfs();
	if (ret)
		goto err_free_input;

	DMSG("requesting irq %d", emdev->irq);
	ret = request_threaded_irq(	emdev->irq, NULL, em718x_irq_thread,
								IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
								"em718x", emdev);
	if (ret) {
		dev_err(emdev->dev, "unable to acquire em718x irq %d\n", emdev->irq);
		goto err_free_debugfs;
	}

	DMSG("em718x initialized successfully");

	return 0;

err_free_debugfs:
	em718x_free_debugfs();


err_free_input:
	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++) {
		struct em718x_sensor * sensor = &emdev->sensors[i];
		struct input_dev * input = sensor->input;
		if ( input ) {
			sysfs_remove_group(&input->dev.kobj, &sensor->attrs_group);
			input_unregister_device(input);
		}
	}

	return 1;
}

static ssize_t PNI_BMMI_TEST(struct device *device,
					struct device_attribute *attr, char *buf)
{
	u8 buf_tem[1];
	u8 err_sensors;
	buf_tem[0] = 0xff;
	err_sensors = 0x77;

	em718x_lock(emdev);
	i2c_blocking_read(emdev, SR_SENSOR_STATUS, (u8 *)buf_tem, 1);
	em718x_unlock(emdev);
	if (buf_tem[0] & err_sensors) {
		printk("sensors A+G+M init fail, ret: 0x%2x\n", buf_tem[0]);
		return (sprintf(buf,"0\n"));
	}
	printk("sensors A+G+M init successfully\n");
	return (sprintf(buf,"1\n"));
}

static ssize_t PNI_SENTRAL_BMMI_TEST(struct device *device,
					struct device_attribute *attr, char *buf)
{
	u8 buf_tem[1];
	u8 err_eeprom;
	buf_tem[0] = 0xff;
	err_eeprom = 0x04;

	em718x_lock(emdev);
	i2c_blocking_read(emdev, SR_EM718X_STATUS, (u8 *)buf_tem, 1);
	em718x_unlock(emdev);
	if (buf_tem[0] & err_eeprom) {
		printk("sensors PNI SENtral init fail, ret: 0x%2x\n", buf_tem[0]);
		return (sprintf(buf,"0\n"));
	}
	printk("sensors PNI SENtral init successfully\n");
	return (sprintf(buf,"1\n"));
}

static struct device_attribute device_attrs[] = {
	__ATTR(pni_fw_update, S_IRUGO, PNI_FW_UPLOAD, NULL),
	__ATTR(gsensor_status, S_IRUGO, PNI_BMMI_TEST, NULL),
	__ATTR(gyroscope_status, S_IRUGO, PNI_BMMI_TEST, NULL),
	__ATTR(SensorHubStatus, S_IRUGO, PNI_SENTRAL_BMMI_TEST, NULL),
};

/**
 * \brief Called by the kernel if devicetree or a boardfile specifies this driver for an I2C device
 * \param client - The i2c_client device created by the kernel
 * \param id - unused
 * \return bool - 0 on success, <1 on error
 */
static int em718x_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int ret = 0;

	dev_t dev = MKDEV(PNI_major, 0);

	DMSG("probe for device %s", id->name);

	ret = i2c_check_functionality(client->adapter,
									I2C_FUNC_SMBUS_BYTE_DATA);

	if (!ret) {
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	emdev = kzalloc(sizeof(*emdev), GFP_KERNEL);
	if (!emdev) {
		printk(KERN_ERR "failed to allocate em718x data\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, emdev);

	emdev->di  = kzalloc(sizeof(struct DI_INSTANCE), GFP_KERNEL);
	if (!emdev->di) {
		printk(KERN_ERR "failed to allocate DI_INSTANCE\n");
		ret = -ENOMEM;
		goto err_free_dev;
	}

	emdev->dev = &client->dev;
	emdev->irq = client->irq;

	if (!di_init_core(emdev->di, emdev, emdev, FALSE)) {
		printk(KERN_ERR "failed to init driver core\n");
		ret = -ENODEV;
		goto err_free_di;
	}

	if ( !di_detect_sentral_simple( emdev->di ) ) {
		printk(KERN_ERR "attached device does not appear to be an EM7180\n");
		ret = -ENODEV;
		goto err_free_di;
	}


	mutex_init(&emdev->mutex);

	PNI_major = MAJOR(dev);
	PNI_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(PNI_class))
		DMSG("Err: failed in creating class.\n");

	PNI_dev = MKDEV(PNI_major, PNI_major);
	PNI_class_dev = device_create(PNI_class, NULL, PNI_dev, NULL, DEVICE_NAME);
	if (PNI_class_dev == NULL)
		DMSG("Err: failed in creating device.\n");

	device_create_file(PNI_class_dev, &device_attrs[0]);
	device_create_file(PNI_class_dev, &device_attrs[1]);
	device_create_file(PNI_class_dev, &device_attrs[2]);
	device_create_file(PNI_class_dev, &device_attrs[3]);

	DMSG("em718x initialized successfully");

	return 0;

err_free_di:
	kfree(emdev->di);

err_free_dev:
	kfree(emdev);

	return ret;
}

static int em718x_remove(struct i2c_client *client)
{
	struct em718x *emdev = i2c_get_clientdata(client);
	int i;

	// unregister class
	device_destroy(PNI_class, PNI_dev);
	class_destroy(PNI_class);

	free_irq(emdev->irq, emdev);

	em718x_free_debugfs();

	DMSG("remove");
	for (i=DST_FIRST; i<DST_NUM_SENSOR_TYPES; i++) {
		struct em718x_sensor * sensor = &emdev->sensors[i];
		struct input_dev * input = sensor->input;
		if ( input ) {
			sysfs_remove_group(&input->dev.kobj, &sensor->attrs_group);
			input_unregister_device(input);
		}
	}

	sysfs_remove_group(&emdev->dev->kobj, &em718x_attribute_group);

	kfree(emdev);

	return 0;
}

#ifdef CONFIG_PM
static int em718x_suspend(struct device *dev)
{
	DMSG("suspend");

	return 0;
}

static int em718x_resume(struct device *dev)
{
	DMSG("resume");

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(em718x_pm, em718x_suspend,
						em718x_resume);

static const struct i2c_device_id em718x_id[] = {
	{ "em718x", 0 },
	//{ "em7180", 0 },
	//{ "em7181", 1 },
	{ }
};

static struct of_device_id sentral_match_table[]={
	{.compatible = "pni,em718x"},
	{},
};

MODULE_DEVICE_TABLE(i2c, em718x_id);

static struct i2c_driver em718x_driver = {
	.driver = {
		.name = "em718x",
		.owner = THIS_MODULE,
		.pm = &em718x_pm,
		.of_match_table = sentral_match_table,
	},
	.probe    = em718x_probe,
	.remove   = em718x_remove,
	.id_table = em718x_id,
};

static int __init em718x_module_init(void)
{
	int status;

	status=i2c_add_driver(&em718x_driver);
	DMSG("i2c_add_driver status=%x", status);
	return status;

	//return i2c_add_driver(&em718x_driver);
}
module_init(em718x_module_init);

static void __exit em718x_module_exit(void)
{
	i2c_del_driver(&em718x_driver);
}
module_exit(em718x_module_exit);

MODULE_AUTHOR("Joshua Gleaton <joshua.gleaton@treadlighttech.com>");
MODULE_DESCRIPTION("EM Microelectronic 718x Sensor Hub");
MODULE_LICENSE("GPL");
