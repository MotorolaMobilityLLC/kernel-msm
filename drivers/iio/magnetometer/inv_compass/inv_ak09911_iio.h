/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#ifndef _INV_AK09911_IIO_H_
#define _INV_AK09911_IIO_H_

#include <linux/i2c.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>

#include "iio.h"
#include "buffer.h"
#include "trigger.h"

/**
 *  struct inv_ak09911_state_s - Driver state variables.
 *  @plat_data:     board file platform data.
 *  @i2c:           i2c client handle.
 *  @trig:          not used. for compatibility.
 *  @work:          work data structure.
 *  @delay:         delay between each scheduled work.
 *  @compass_id:    id of compass.
 *  @compass_data:  compass data.
 *  @asa:           fuse data to adjust final data.
 *  @timestamp:     timestamp of each data.
 *  @i2c_addr:      i2c address
 *  @sl_handle:		Handle to I2C port.
 */
struct inv_ak09911_state_s {
	struct mpu_platform_data plat_data;
	struct i2c_client *i2c;
	struct iio_trigger  *trig;
	struct delayed_work work;
	int delay;                 /* msec */
	unsigned char compass_id;
	short compass_data[3];
	u8 asa[3];	           /* axis sensitivity adjustment */
	s64 timestamp;
	short i2c_addr;
	void *sl_handle;
};

/* scan element definition */
enum inv_mpu_scan {
	INV_AK09911_SCAN_MAGN_X,
	INV_AK09911_SCAN_MAGN_Y,
	INV_AK09911_SCAN_MAGN_Z,
	INV_AK09911_SCAN_TIMESTAMP,
};

/*! \name ak09911 constant definition
 \anchor ak09911_Def
 Constant definitions of the ak09911.*/
#define AK09911_MEASUREMENT_TIME_US	10000

/*! \name ak09911 operation mode
 \anchor AK09911_Mode
 Defines an operation mode of the ak09911.*/
/*! @{*/
#define AK09911_CNTL_MODE_SNG_MEASURE    0x01
#define	AK09911_CNTL_MODE_SELF_TEST      0x08
#define	AK09911_CNTL_MODE_FUSE_ACCESS    0x1F
#define	AK09911_CNTL_MODE_POWER_DOWN     0x00
/*! @}*/

/*! \name ak09911 register address
\anchor AK09911_REG
Defines a register address of the ak09911.*/
/*! @{*/
#define AK09911_REG_WIA		0x00
#define AK09911_REG_INFO	0x01
#define AK09911_REG_ST1		0x10
#define AK09911_REG_HXL		0x11
#define AK09911_REG_ST2		0x18
#define AK09911_REG_CNTL	0x31
/*! @}*/

/*! \name ak09911 fuse-rom address
\anchor AK09911_FUSE
Defines a read-only address of the fuse ROM of the ak09911.*/
/*! @{*/
#define AK09911_FUSE_ASAX	0x60
/*! @}*/

#define AK09911_MAX_DELAY        (200)
#define AK09911_MIN_DELAY        (10)
#define AK09911_DEFAULT_DELAY    (100)

#define INV_ERROR_COMPASS_DATA_OVERFLOW  (-1)
#define INV_ERROR_COMPASS_DATA_NOT_READY (-2)

int inv_ak09911_configure_ring(struct iio_dev *indio_dev);
void inv_ak09911_unconfigure_ring(struct iio_dev *indio_dev);
int inv_ak09911_probe_trigger(struct iio_dev *indio_dev);
void inv_ak09911_remove_trigger(struct iio_dev *indio_dev);
void set_ak09911_enable(struct iio_dev *indio_dev, bool enable);
int ak09911_read_raw_data(struct inv_ak09911_state_s *st,
				short dat[3]);
void inv_read_ak09911_fifo(struct iio_dev *indio_dev);
int ak09911_read(struct inv_ak09911_state_s *st, short rawfixed[3]);

#endif

