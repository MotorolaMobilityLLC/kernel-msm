/* drivers/misc/akm09912.c - akm09912 compass driver
 *
 * Copyright (C) 2007-2008 HTC Corporation.
 * Author: Hou-Kun Chen <houkun.chen@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


/*#define DEBUG*/
/*#define VERBOSE_DEBUG*/

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "linux/stml0xx.h"

#ifdef CONFIG_SENSORS_SH_AK09912

#include "linux/stml0xx_akm.h"

#define AKM_HAS_RESET			1
#define AKM_DRDY_TIMEOUT_MS		1000

#define AKM_I2C_Address 0x18

/***** Hardware I/O functions **************************************/
static int akm_rxdata(uint8_t *rxData, int length)
{
	unsigned char buf[SPI_RX_PAYLOAD_LEN];
	int loop_count;
	int err;

	dev_dbg(&stml0xx_misc_data->spi->dev, "akm_rxdata, length %d", length);
	buf[0] = 0x00;            /* Bus number */
	buf[1] = AKM_I2C_Address; /* I2C address */
	buf[2] = 0x00;            /* Register address MSB */
	buf[3] = rxData[0];       /* Register address LSB */
	buf[4] = 0x00;            /* Read */
	buf[5] = length;          /* Size */

	err = stml0xx_spi_send_write_reg(I2C_PASSTHROUGH_COMMAND, buf,
			STML0XX_PASSTHROUGH_SIZE);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
				"akm_rxdata: AKM command write error %d, reg 0x%02X",
				err, rxData[0]);
		return -EFAULT;
	}

	loop_count = 0;
	buf[0] = 0;
	do {
		err = stml0xx_spi_send_read_reg(
				I2C_PASSTHROUGH_RESPONSE,
				buf, STML0XX_PASSTHROUGH_SIZE);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
					"akm_rxdata: AKM data read error %d, reg 0x%02X",
					err, rxData[0]);
			return -EFAULT;
		}
		loop_count++;
		if (loop_count == 10)
			break;
	} while (buf[0] == 0);
	if (buf[0] != 1) {
		dev_err(&stml0xx_misc_data->spi->dev,
				"akm_rxdata: AKM read failed %d, reg 0x%02X",
				buf[0], rxData[0]);
		return -EFAULT;
	}

	memcpy(rxData, &buf[1], length);

	return 0;
}

static int akm_txdata(uint8_t *txData, int length)
{
	unsigned char buf[SPI_TX_PAYLOAD_LEN];
	int loop_count;
	int err;

	dev_dbg(&stml0xx_misc_data->spi->dev, "akm_txdata, length %d", length);
	buf[0] = 0x00;            /* Bus number */
	buf[1] = AKM_I2C_Address; /* I2C address */
	buf[2] = 0x00;            /* Register address MSB */
	buf[3] = txData[0];       /* Register address LSB */
	buf[4] = 0x01;            /* Write */
	buf[5] = (length - 1);    /* Size */
	memcpy(&buf[6], (txData + 1), (length - 1));

	err = stml0xx_spi_send_write_reg(I2C_PASSTHROUGH_COMMAND, buf,
			STML0XX_PASSTHROUGH_SIZE);
	if (err < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
				"akm_txdata: AKM command write error %d, reg 0x%02X",
				err, txData[0]);
		return -EFAULT;
	}

	loop_count = 0;
	buf[0] = 0;
	do {
		err = stml0xx_spi_send_read_reg(
				I2C_PASSTHROUGH_RESPONSE,
				buf, STML0XX_PASSTHROUGH_SIZE);
		if (err < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
					"akm_txdata: AKM data read error %d, reg 0x%02X",
					err, txData[0]);
			return -EFAULT;
		}
		loop_count++;
		if (loop_count == 10)
			break;
	} while (buf[0] == 0);
	if (buf[0] != 1) {
		dev_err(&stml0xx_misc_data->spi->dev,
				"akm_txdata: AKM write failed %d, reg 0x%02X",
				buf[0], txData[0]);
		return -EFAULT;
	}

	return 0;
}

/***** akm miscdevice functions *************************************/
static int AKECS_Set_CNTL(
	struct stml0xx_data *ps_stml0xx,
	uint8_t mode)
{
	uint8_t buffer[2];
	int err;

	/***** lock *****/
	mutex_lock(&ps_stml0xx->akm_sensor_mutex);
	/* Busy check */
	if (ps_stml0xx->akm_is_busy > 0) {
		dev_err(&ps_stml0xx->spi->dev,
				"%s: device is busy.", __func__);
		err = -EBUSY;
	} else {
		/* Set measure mode */
		buffer[0] = AKM_REG_MODE;
		buffer[1] = mode;
		err = akm_txdata(buffer, 2);
		if (err < 0) {
			dev_err(&ps_stml0xx->spi->dev,
					"%s: Can not set CNTL.", __func__);
		} else {
			dev_vdbg(&ps_stml0xx->spi->dev,
					"Mode is set to (%d).", mode);
			/* Set flag */
			ps_stml0xx->akm_is_busy = 1;
			atomic_set(&ps_stml0xx->akm_drdy, 0);
			/* wait at least 100us after changing mode */
			udelay(100);
		}
	}

	mutex_unlock(&ps_stml0xx->akm_sensor_mutex);
	/***** unlock *****/

	return err;
}

static int AKECS_Set_PowerDown(
	struct stml0xx_data *ps_stml0xx)
{
	uint8_t buffer[2];
	int err = 0;

	/***** lock *****/
	mutex_lock(&ps_stml0xx->akm_sensor_mutex);

	/* Set powerdown mode */
	buffer[0] = AKM_REG_MODE;
	buffer[1] = AKM_MODE_POWERDOWN;
	err = akm_txdata(buffer, 2);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"%s: Can not set to powerdown mode.", __func__);
	} else {
		dev_dbg(&ps_stml0xx->spi->dev, "Powerdown mode is set.");
		/* wait at least 100us after changing mode */
		udelay(100);
	}
	/* Clear status */
	ps_stml0xx->akm_is_busy = 0;
	atomic_set(&ps_stml0xx->akm_drdy, 0);

	mutex_unlock(&ps_stml0xx->akm_sensor_mutex);
	/***** unlock *****/

	return err;
}

static int AKECS_SetMode(
	struct stml0xx_data *ps_stml0xx,
	uint8_t mode)
{
	int err;

	switch (mode & 0x1F) {
	case AKM_MODE_SNG_MEASURE:
	case AKM_MODE_SELF_TEST:
	case AKM_MODE_FUSE_ACCESS:
		err = AKECS_Set_CNTL(ps_stml0xx, mode);
		break;
	case AKM_MODE_POWERDOWN:
		err = AKECS_Set_PowerDown(ps_stml0xx);
		break;
	default:
		dev_err(&ps_stml0xx->spi->dev,
			"%s: Unknown mode(%d).", __func__, mode);
		return -EINVAL;
	}

	return err;
}

static void AKECS_SetYPR(
	struct stml0xx_data *ps_stml0xx,
	int *rbuf)
{
	uint8_t buf[UNCALIB_SENSOR_DATA_SIZE];
	uint64_t timestamp;
	int16_t bias_x, bias_y, bias_z;

	dev_vdbg(&ps_stml0xx->spi->dev, "%s: flag =0x%X", __func__, rbuf[0]);
	dev_vdbg(&ps_stml0xx->spi->dev, "  Acc [LSB]   : %6d,%6d,%6d stat=%d",
		rbuf[1], rbuf[2], rbuf[3], rbuf[4]);
	dev_vdbg(&ps_stml0xx->spi->dev, "  Geo [LSB]   : %6d,%6d,%6d stat=%d",
		rbuf[5], rbuf[6], rbuf[7], rbuf[8]);
	dev_vdbg(&ps_stml0xx->spi->dev, "  Orientation : %6d,%6d,%6d",
		rbuf[9], rbuf[10], rbuf[11]);
	dev_vdbg(&ps_stml0xx->spi->dev, "  Rotation V  : %6d,%6d,%6d,%6d",
		rbuf[12], rbuf[13], rbuf[14], rbuf[15]);
	dev_vdbg(&ps_stml0xx->spi->dev, "  Raw MAG     : %6d,%6d,%6d",
		rbuf[16], rbuf[17], rbuf[18]);
	dev_vdbg(&ps_stml0xx->spi->dev, "  Timestamp   : 0x%04x%04x",
		rbuf[19], rbuf[20]);

	memcpy(&timestamp, &rbuf[AKM_YPR_DATA_SIZE], AKM_SENSOR_TIME_SIZE);

	/* Report magnetic vector information */
	buf[0] = (rbuf[5] >> 8) & 0xFF;
	buf[1] = rbuf[5] & 0xFF;
	buf[2] = (rbuf[6] >> 8) & 0xFF;
	buf[3] = rbuf[6] & 0xFF;
	buf[4] = (rbuf[7] >> 8) & 0xFF;
	buf[5] = rbuf[7] & 0xFF;
	stml0xx_as_data_buffer_write(ps_stml0xx,
			DT_MAG,
			buf,
			SENSOR_DATA_SIZE,
			(uint8_t)rbuf[8],
			timestamp);

	bias_x = rbuf[16] - rbuf[5];
	bias_y = rbuf[17] - rbuf[6];
	bias_z = rbuf[18] - rbuf[7];
	buf[0] = (rbuf[16] >> 8) & 0xFF;
	buf[1] = rbuf[16] & 0xFF;
	buf[2] = (rbuf[17] >> 8) & 0xFF;
	buf[3] = rbuf[17] & 0xFF;
	buf[4] = (rbuf[18] >> 8) & 0xFF;
	buf[5] = rbuf[18] & 0xFF;
	buf[6] = (bias_x >> 8) & 0xFF;
	buf[7] = bias_x & 0xFF;
	buf[8] = (bias_y >> 8) & 0xFF;
	buf[9] = bias_y & 0xFF;
	buf[10] = (bias_z >> 8) & 0xFF;
	buf[11] = bias_z & 0xFF;
	stml0xx_as_data_buffer_write(ps_stml0xx,
			DT_UNCALIB_MAG,
			buf,
			UNCALIB_SENSOR_DATA_SIZE,
			(uint8_t)rbuf[8],
			timestamp);

	/* Report fusion sensor information */
	buf[0] = (rbuf[9] >> 8) & 0xFF;
	buf[1] = rbuf[9] & 0xFF;
	buf[2] = (rbuf[10] >> 8) & 0xFF;
	buf[3] = rbuf[10] & 0xFF;
	buf[4] = (rbuf[11] >> 8) & 0xFF;
	buf[5] = rbuf[11] & 0xFF;
	stml0xx_as_data_buffer_write(ps_stml0xx,
			DT_ORIENT,
			buf,
			SENSOR_DATA_SIZE,
			rbuf[8],
			timestamp);
}

/* This function will block a process until the latest measurement
 * data is available.
 */
static int AKECS_GetData(
	struct stml0xx_data *ps_stml0xx,
	uint8_t *rbuf,
	int size)
{
	uint32_t enable;
	long ret = 0;

	dev_dbg(&ps_stml0xx->spi->dev, "AKECS_GetData, DRDY %d, num_data %d",
		atomic_read(&ps_stml0xx->akm_drdy),
		ps_stml0xx->akm_data_queue.num_data);

	/* Block if data queue empty */
	ret = wait_event_interruptible_timeout(
				ps_stml0xx->akm_drdy_wq,
				atomic_read(&ps_stml0xx->akm_drdy),
				msecs_to_jiffies(AKM_DRDY_TIMEOUT_MS));

	mutex_lock(&ps_stml0xx->akm_val_mutex);
	enable = ps_stml0xx->akm_enable_flag & MAG_DATA_READY;
	mutex_unlock(&ps_stml0xx->akm_val_mutex);
	if (enable == 0) {
		/* Mag disabled. Clear queue */
		mutex_lock(&ps_stml0xx->akm_sensor_mutex);
		memset(&ps_stml0xx->akm_data_queue, 0,
			sizeof(struct akm_data_queue_t));
		mutex_unlock(&ps_stml0xx->akm_sensor_mutex);
		return -ENODATA;
	}

	if (ret == -ERESTARTSYS)
		return -EINTR;

	if (ret < 0) {
		dev_err(&ps_stml0xx->spi->dev,
			"%s: wait_event failed (%ld).", __func__, ret);
		return ret;
	}
	if (!atomic_read(&ps_stml0xx->akm_drdy)) {
		dev_err(&ps_stml0xx->spi->dev,
			"%s: DRDY is not set.", __func__);
		return -ENODATA;
	}

	/* Process data from queue */
	mutex_lock(&ps_stml0xx->akm_sensor_mutex);
	if (ps_stml0xx->akm_data_queue.num_data > 0) {
		memcpy(rbuf, stml0xx_akm_data_queue_remove(ps_stml0xx),
				AKM_DATA_QUEUE_ENTRY_SIZE);
		/* Clear data ready if no more samples remaining */
		if (ps_stml0xx->akm_data_queue.num_data == 0)
			atomic_set(&ps_stml0xx->akm_drdy, 0);
	} else {
		atomic_set(&ps_stml0xx->akm_drdy, 0);
		mutex_unlock(&ps_stml0xx->akm_sensor_mutex);
		return -ENODATA;
	}
	mutex_unlock(&ps_stml0xx->akm_sensor_mutex);

	return 0;
}

static int AKECS_GetOpenStatus(
	struct stml0xx_data *ps_stml0xx)
{
	return wait_event_interruptible(
			ps_stml0xx->akm_open_wq,
			(atomic_read(&ps_stml0xx->akm_active) > 0));
}

static int AKECS_GetCloseStatus(
	struct stml0xx_data *ps_stml0xx)
{
	return wait_event_interruptible(
			ps_stml0xx->akm_open_wq,
			(atomic_read(&ps_stml0xx->akm_active) <= 0));
}


static long AKECS_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct stml0xx_data *ps_stml0xx = file->private_data;

	/* NOTE: In this function the size of "char" should be 1-byte. */
	uint8_t i2c_buf[AKM_RWBUF_SIZE];       /* READ/WRITE */
	uint8_t dat_buf[AKM_SENSOR_DATA_SIZE
			+ AKM_SENSOR_TIME_SIZE]; /* GET_DATA */
	int32_t ypr_buf[AKM_YPR_DATA_SIZE
		+ (AKM_SENSOR_TIME_SIZE / sizeof(int32_t))]; /* SET_YPR */
	int64_t delay[AKM_NUM_SENSORS];        /* GET_DELAY */
	int16_t acc_buf[3];                    /* GET_ACCEL */
	uint8_t mode;                          /* SET_MODE*/
	int status;                            /* OPEN/CLOSE_STATUS */
	uint8_t boot_status;
	int ret = 0;                           /* Return value. */

	if (stml0xx_g_booted != 1)
		return -EAGAIN;

	switch (cmd) {
	case ECS_IOCTL_READ:
	case ECS_IOCTL_WRITE:
		if (argp == NULL) {
			dev_err(&ps_stml0xx->spi->dev,
					"invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(&i2c_buf, argp, sizeof(i2c_buf))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_from_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_SET_MODE:
		if (argp == NULL) {
			dev_err(&ps_stml0xx->spi->dev,
					"invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(&mode, argp, sizeof(mode))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_from_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_SET_YPR:
		if (argp == NULL) {
			dev_err(&ps_stml0xx->spi->dev,
					"invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(&ypr_buf, argp, sizeof(ypr_buf))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_from_user failed.");
			return -EFAULT;
		}
	case ECS_IOCTL_GET_INFO:
	case ECS_IOCTL_GET_CONF:
	case ECS_IOCTL_GET_DATA:
	case ECS_IOCTL_GET_OPEN_STATUS:
	case ECS_IOCTL_GET_CLOSE_STATUS:
	case ECS_IOCTL_GET_DELAY:
	case ECS_IOCTL_GET_LAYOUT:
	case ECS_IOCTL_GET_ACCEL:
	case ECS_IOCTL_CHECK_READY:
		/* Check buffer pointer for writing a data later. */
		if (argp == NULL) {
			dev_err(&ps_stml0xx->spi->dev,
					"invalid argument.");
			return -EINVAL;
		}
		break;
	default:
		break;
	}

	switch (cmd) {
	case ECS_IOCTL_READ:
		dev_vdbg(&ps_stml0xx->spi->dev, "IOCTL_READ called.");
		if ((i2c_buf[0] < 1) || (i2c_buf[0] > (AKM_RWBUF_SIZE-1))) {
			dev_err(&ps_stml0xx->spi->dev,
					"invalid argument.");
			return -EINVAL;
		}
		ret = akm_rxdata(&i2c_buf[1], i2c_buf[0]);
		if (ret < 0)
			return ret;
		break;
	case ECS_IOCTL_WRITE:
		dev_vdbg(&ps_stml0xx->spi->dev, "IOCTL_WRITE called.");
		if ((i2c_buf[0] < 2) || (i2c_buf[0] > (AKM_RWBUF_SIZE-1))) {
			dev_err(&ps_stml0xx->spi->dev,
					"invalid argument.");
			return -EINVAL;
		}
		ret = akm_txdata(&i2c_buf[1], i2c_buf[0]);
		if (ret < 0)
			return ret;
		break;
	case ECS_IOCTL_RESET:
		dev_vdbg(&ps_stml0xx->spi->dev, "IOCTL_RESET called.");
		return 1; /*AKD_SUCCESS;*/
	case ECS_IOCTL_SET_MODE:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_SET_MODE called. %d", mode);
		return 1; /*AKD_SUCCESS;*/
	case ECS_IOCTL_SET_YPR:
		dev_vdbg(&ps_stml0xx->spi->dev, "IOCTL_SET_YPR called.");
		AKECS_SetYPR(ps_stml0xx, ypr_buf);
		break;
	case ECS_IOCTL_GET_DATA:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_DATA called.");
		ret = AKECS_GetData(ps_stml0xx, dat_buf,
					AKM_SENSOR_DATA_SIZE
					+ AKM_SENSOR_TIME_SIZE);

		if (ret < 0)
			return ret;
		break;
	case ECS_IOCTL_GET_OPEN_STATUS:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_OPEN_STATUS called.");
		ret = AKECS_GetOpenStatus(ps_stml0xx);
		if (ret < 0) {
			if (ret != -ERESTARTSYS)
				dev_err(&ps_stml0xx->spi->dev,
					"Get Open returns error (%d).", ret);
			return ret;
		}
		break;
	case ECS_IOCTL_GET_CLOSE_STATUS:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_CLOSE_STATUS called.");
		ret = AKECS_GetCloseStatus(ps_stml0xx);
		if (ret < 0) {
			if (ret != -ERESTARTSYS)
				dev_err(&ps_stml0xx->spi->dev,
					"Get Close returns error (%d).", ret);
			return ret;
		}
		break;
	case ECS_IOCTL_GET_DELAY:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_DELAY called.");
		mutex_lock(&ps_stml0xx->akm_val_mutex);
		delay[0] = -1;
		delay[1] = ((ps_stml0xx->akm_enable_flag & MAG_DATA_READY) ?
				(stml0xx_g_mag_delay * 1000000) : -1);
		delay[2] = -1;
		mutex_unlock(&ps_stml0xx->akm_val_mutex);
		break;
	case ECS_IOCTL_GET_INFO:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_INFO called.");
		break;
	case ECS_IOCTL_GET_CONF:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_CONF called.");
		break;
	case ECS_IOCTL_GET_LAYOUT:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_LAYOUT called.");
		break;
	case ECS_IOCTL_GET_ACCEL:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"IOCTL_GET_ACCEL called.");
		mutex_lock(&ps_stml0xx->akm_accel_mutex);
		acc_buf[0] = ps_stml0xx->akm_accel_data[0];
		acc_buf[1] = ps_stml0xx->akm_accel_data[1];
		acc_buf[2] = ps_stml0xx->akm_accel_data[2];
		mutex_unlock(&ps_stml0xx->akm_accel_mutex);
		break;
	case ECS_IOCTL_CHECK_READY:
		dev_vdbg(&ps_stml0xx->spi->dev,
				"ECS_IOCTL_CHECK_READY called");
		break;
	default:
		return -ENOTTY;
	}

	switch (cmd) {
	case ECS_IOCTL_READ:
		/* +1  is for the first byte */
		if (copy_to_user(argp, &i2c_buf, i2c_buf[0]+1)) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_INFO:
		if (copy_to_user(argp, &ps_stml0xx->akm_sense_info,
					sizeof(ps_stml0xx->akm_sense_info))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_CONF:
		if (copy_to_user(argp, &ps_stml0xx->akm_sense_conf,
					sizeof(ps_stml0xx->akm_sense_conf))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_DATA:
		if (copy_to_user(argp, &dat_buf, sizeof(dat_buf))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_OPEN_STATUS:
	case ECS_IOCTL_GET_CLOSE_STATUS:
		status = atomic_read(&ps_stml0xx->akm_active);
		if (copy_to_user(argp, &status, sizeof(status))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_DELAY:
		if (copy_to_user(argp, &delay, sizeof(delay))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_LAYOUT:
		if (copy_to_user(argp, &ps_stml0xx->akm_layout,
					sizeof(ps_stml0xx->akm_layout))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_GET_ACCEL:
		if (copy_to_user(argp, &acc_buf, sizeof(acc_buf))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case ECS_IOCTL_CHECK_READY:
		boot_status = stml0xx_g_booted;
		if (copy_to_user(argp, &boot_status,
				sizeof(boot_status))) {
			dev_err(&ps_stml0xx->spi->dev,
					"copy_to_user failed.");
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	return 0;
}

int akm09912_enable_mag(int enable, struct stml0xx_data *ps_stml0xx)
{
	static int old_enable = -1;

	if (enable == old_enable)
		return 0;
	old_enable = enable;

	mutex_lock(&ps_stml0xx->akm_val_mutex);
	ps_stml0xx->akm_enable_flag &= ~MAG_DATA_READY;
	ps_stml0xx->akm_enable_flag &= ~FUSION_DATA_READY;
	if (enable) {
		ps_stml0xx->akm_enable_flag |= MAG_DATA_READY;
		ps_stml0xx->akm_enable_flag |= FUSION_DATA_READY;
	}

	enable = ps_stml0xx->akm_enable_flag;
	mutex_unlock(&ps_stml0xx->akm_val_mutex);

	if (enable == 0) {
		if (atomic_cmpxchg(&ps_stml0xx->akm_active, 1, 0) == 1) {
			wake_up(&ps_stml0xx->akm_open_wq);
			dev_dbg(&ps_stml0xx->spi->dev, "Deactivated");
		}
	} else {
		if (atomic_cmpxchg(&ps_stml0xx->akm_active, 0, 1) == 0) {
			wake_up(&ps_stml0xx->akm_open_wq);
			dev_dbg(&ps_stml0xx->spi->dev, "Activated");
		}
	}

	dev_dbg(&ps_stml0xx->spi->dev,
			"Status updated: enable=0x%X, active=%d",
			enable, atomic_read(&ps_stml0xx->akm_active));

	return 0;
}

int akm09912_i2c_check_device(
	struct stml0xx_data *ps_stml0xx)
{
	int err;

	ps_stml0xx->akm_sense_info[0] = AK09912_REG_WIA1;
	err = akm_rxdata(ps_stml0xx->akm_sense_info, AKM_SENSOR_INFO_SIZE);
	if (err < 0)
		return err;

	/* Set FUSE access mode */
	err = AKECS_SetMode(ps_stml0xx, AK09912_MODE_FUSE_ACCESS);
	if (err < 0)
		return err;

	ps_stml0xx->akm_sense_conf[0] = AK09912_FUSE_ASAX;
	err = akm_rxdata(ps_stml0xx->akm_sense_conf, AKM_SENSOR_CONF_SIZE);
	if (err < 0)
		return err;

	err = AKECS_SetMode(ps_stml0xx, AK09912_MODE_POWERDOWN);
	if (err < 0)
		return err;

	/* Check read data */
	if (ps_stml0xx->akm_sense_info[0] != AKM_WIA_VALE) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"%s: The device is not AKM Compass.", __func__);
		return -ENXIO;
	}

	return err;
}

static int stml0xx_akm_open(struct inode *inode, struct file *file)
{
	int err = 0;

	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_akm_open");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = stml0xx_misc_data;

	return err;
}

static int stml0xx_akm_release(struct inode *inode, struct file *file)
{
	return 0;
}

const struct file_operations stml0xx_akm_fops = {
	.owner = THIS_MODULE,
	.open = stml0xx_akm_open,
	.release = stml0xx_akm_release,
	.unlocked_ioctl = AKECS_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = AKECS_ioctl,
#endif
};

struct miscdevice stml0xx_akm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "stml0xx_akm",
	.fops = &stml0xx_akm_fops,
};

void stml0xx_akm_init(struct stml0xx_data *ps_stml0xx)
{
	int err = 0;

	dev_info(&ps_stml0xx->spi->dev, "stml0xx_akm_init");

	ps_stml0xx->akm_layout = ps_stml0xx->pdata->mag_layout;

	init_waitqueue_head(&ps_stml0xx->akm_drdy_wq);
	init_waitqueue_head(&ps_stml0xx->akm_open_wq);

	mutex_init(&ps_stml0xx->akm_sensor_mutex);
	mutex_init(&ps_stml0xx->akm_val_mutex);
	mutex_init(&ps_stml0xx->akm_accel_mutex);

	atomic_set(&ps_stml0xx->akm_active, 0);
	atomic_set(&ps_stml0xx->akm_drdy, 0);

	ps_stml0xx->akm_is_busy = 0;
	ps_stml0xx->akm_enable_flag = 0;

	memset(&ps_stml0xx->akm_data_queue, 0, sizeof(struct akm_data_queue_t));

	err = misc_register(&stml0xx_akm_device);
	if (err < 0) {
		dev_err(&ps_stml0xx->spi->dev,
				"akm device create failed %d", err);
		goto exit;
	}

exit:
	return;
}

int stml0xx_akm_data_queue_insert(struct stml0xx_data *ps_stml0xx,
					uint8_t *data)
{
	int ret = 0;
	struct akm_data_queue_t *queue = &ps_stml0xx->akm_data_queue;

	if (queue->num_data >= AKM_DATA_MAX_QUEUE_SIZE) {
		dev_err(&ps_stml0xx->spi->dev, "akm data queue full");
		ret = -1;
	} else {
		queue->num_data++;
		memcpy(&queue->data[queue->tail * AKM_DATA_QUEUE_ENTRY_SIZE],
			data, AKM_DATA_QUEUE_ENTRY_SIZE);
		queue->tail = (queue->tail + 1) % AKM_DATA_MAX_QUEUE_SIZE;
	}

	return ret;
}

uint8_t *stml0xx_akm_data_queue_remove(struct stml0xx_data *ps_stml0xx)
{
	struct akm_data_queue_t *queue = &ps_stml0xx->akm_data_queue;
	uint8_t old_head;
	uint8_t *ret = NULL;

	if (queue->num_data == 0) {
		dev_dbg(&ps_stml0xx->spi->dev, "akm data queue empty");
	} else {
		queue->num_data--;
		old_head = queue->head;
		queue->head = (queue->head + 1) % AKM_DATA_MAX_QUEUE_SIZE;

		ret = &queue->data[old_head * AKM_DATA_QUEUE_ENTRY_SIZE];
	}
	return ret;
}

#endif /* CONFIG_SENSORS_SH_AK09912 */
