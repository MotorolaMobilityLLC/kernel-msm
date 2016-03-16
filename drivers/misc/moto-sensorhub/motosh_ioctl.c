/*
 * Copyright (C) 2010-2016 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

#include <linux/firmware.h>
#include <linux/fs.h>

long motosh_misc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	static int brightness_table_loaded;
	static int lowpower_mode = 1;
	int err = 0;
	unsigned int addr = 0;
	unsigned int data_size = 0;
	unsigned char rw_bytes[4];
	struct motosh_data *ps_motosh = file->private_data;
	unsigned char byte;
	unsigned char bytes[3];
	unsigned short delay;
	struct motosh_moto_sensor_batch_cfg batch_cfg;
	unsigned long current_posix_time;
	unsigned int handle;
	struct timespec current_time;
	unsigned char cmdbuff[MOTOSH_MAXDATA_LENGTH];
	unsigned char readbuff[MOTOSH_MAXDATA_LENGTH];
	unsigned int read_write;
	unsigned int loop_count;

#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
	int chk;
#endif

	err = mutex_lock_interruptible(&ps_motosh->lock);
	if (err != 0)
		return err;

	motosh_wake(ps_motosh);

	switch (cmd) {
	case MOTOSH_IOCTL_BOOTLOADERMODE:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_BOOTLOADERMODE");
		err = switch_motosh_mode(BOOTMODE);
		break;
	case MOTOSH_IOCTL_NORMALMODE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_NORMALMODE");
		err = switch_motosh_mode(NORMALMODE);
		break;
	case MOTOSH_IOCTL_MASSERASE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_MASSERASE");
		err = motosh_boot_flash_erase();
		break;
	case MOTOSH_IOCTL_SETSTARTADDR:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SETSTARTADDR");
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			dev_err(&ps_motosh->client->dev,
				"Copy start address returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_misc_data->current_addr = addr;
		err = 0;
		break;
	case MOTOSH_IOCTL_SET_FACTORY_MODE:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_FACTORY_MODE");
		err = switch_motosh_mode(FACTORYMODE);
		break;
	case MOTOSH_IOCTL_TEST_BOOTMODE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_TEST_BOOTMODE");
		err = switch_motosh_mode(BOOTMODE);
		break;
	case MOTOSH_IOCTL_SET_DEBUG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_DEBUG");
		err = 0;
		break;
	case MOTOSH_IOCTL_GET_VERNAME:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_VERNAME");
		if (copy_to_user(argp, &(ps_motosh->pdata->fw_version),
				FW_VERSION_SIZE))
			err = -EFAULT;
		else
			err = 0;
		break;

	case MOTOSH_IOCTL_GET_VERSION_STR:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_VERSION_STR");
		if (ps_motosh->mode > BOOTMODE) {
			err = motosh_get_version_str(ps_motosh);
			if (err >= 0) {
				if (copy_to_user(argp,
					&(ps_motosh->pdata->fw_version_str),
					FW_VERSION_STR_MAX_LEN))
					err = -EFAULT;
			}
		} else {
			err = -EBUSY;
		}
		break;

	case MOTOSH_IOCTL_GET_FLASH_CRC:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_FLASH_CRC");

		if (ps_motosh->mode <= BOOTMODE) {
			dev_err(&ps_motosh->client->dev,
				"Tried to read flash CRC in boot mode\n");
			err = -EIO;
			break;
		}

		cmdbuff[0] = FW_CRC;
		err = motosh_i2c_write_read_no_reset(ps_motosh,
				cmdbuff, readbuff, 1, 4);
		if (err >= 0) {
			if (copy_to_user(argp, readbuff, 4))
				err = -EFAULT;
			else
				err = 0;
		}

		break;

	case MOTOSH_IOCTL_GET_BOOTED:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_BOOTED");
		byte = motosh_g_booted;
		if (copy_to_user(argp, &byte, 1))
			err = -EFAULT;
		else
			err = 0;
		break;
	case MOTOSH_IOCTL_GET_VERSION:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_VERSION");
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_get_version(ps_motosh);
		else
			err = -EBUSY;
		break;
	case MOTOSH_IOCTL_SET_ACC_DELAY:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ACC_DELAY");
		delay = 0;
		if (copy_from_user(&batch_cfg, argp, sizeof(batch_cfg))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy acc delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_g_acc_cfg = batch_cfg;

		/* set is_batching at batch startup based on timeout. The clear
		   will be delayed until after latest queue is read */
		if (motosh_g_acc_cfg.timeout > 0)
			ps_motosh->is_batching |= ACCEL_BATCHING;

		cmdbuff[0] = ACCEL_UPDATE_RATE;
		cmdbuff[1] = motosh_g_acc_cfg.delay;
		cmdbuff[2] = (unsigned char)
			((motosh_g_acc_cfg.timeout >> 24));
		cmdbuff[3] = (unsigned char)
			((motosh_g_acc_cfg.timeout >> 16) & 0xff);
		cmdbuff[4] = (unsigned char)
			((motosh_g_acc_cfg.timeout >> 8)  & 0xff);
		cmdbuff[5] = (unsigned char)
			((motosh_g_acc_cfg.timeout)       & 0xff);

		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 6);
		break;

	case MOTOSH_IOCTL_SET_MAG_DELAY:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_MAG_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy mag delay returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = MAG_UPDATE_RATE;
		cmdbuff[1] = delay;
		motosh_g_mag_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_GYRO_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_GYRO_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy gyro delay returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = GYRO_UPDATE_RATE;
		cmdbuff[1] = delay;
		motosh_g_gyro_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_STEP_COUNTER_DELAY:
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy step counter delay returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = STEP_COUNTER_INFO;
		cmdbuff[1] = (delay>>8);
		cmdbuff[2] = delay & 0xFF;
		motosh_g_step_counter_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 3);
		break;
	case MOTOSH_IOCTL_SET_PRES_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_PRES_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy pres delay returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = PRESSURE_UPDATE_RATE;
		cmdbuff[1] = delay;
		motosh_g_baro_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_SENSORS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_SENSORS");
		if (copy_from_user(bytes, argp, 3 * sizeof(unsigned char))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy set sensors returned error\n");
			err = -EFAULT;
			break;
		}

		if ((brightness_table_loaded == 0)
				&& (bytes[1] & (M_DISP_BRIGHTNESS >> 8))) {
			err = motosh_load_brightness_table(ps_motosh,
					cmdbuff);
			if (err) {
				dev_err(&ps_motosh->client->dev,
					"Loading brightness failed\n");
				break;
			}
			brightness_table_loaded = 1;
		}

		cmdbuff[0] = NONWAKESENSOR_CONFIG;
		cmdbuff[1] = bytes[0];
		cmdbuff[2] = bytes[1];
		cmdbuff[3] = bytes[2];
		motosh_g_nonwake_sensor_state = (cmdbuff[3] << 16)
			| (cmdbuff[2] << 8) | cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 4);
		dev_dbg(&ps_motosh->client->dev, "Sensor enable = 0x%lx\n",
			motosh_g_nonwake_sensor_state);
		break;
	case MOTOSH_IOCTL_GET_SENSORS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_SENSORS");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = NONWAKESENSOR_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, 3);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get sensors failed\n");
				break;
			}
			bytes[0] = readbuff[0];
			bytes[1] = readbuff[1];
			bytes[2] = readbuff[2];
		} else {
			bytes[0] = motosh_g_nonwake_sensor_state & 0xFF;
			bytes[1] = (motosh_g_nonwake_sensor_state >> 8) & 0xFF;
			bytes[2] = (motosh_g_nonwake_sensor_state >> 16) & 0xFF;
		}
		if (copy_to_user(argp, bytes, 3 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_WAKESENSORS:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_WAKESENSORS");
		if (copy_from_user(bytes, argp, 3 * sizeof(unsigned char))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy set sensors returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = WAKESENSOR_CONFIG;
		cmdbuff[1] = bytes[0];
		cmdbuff[2] = bytes[1];
		cmdbuff[3] = bytes[2];
		motosh_g_wake_sensor_state =  (cmdbuff[3] << 16)
			| (cmdbuff[2] << 8) | cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 4);
		dev_dbg(&ps_motosh->client->dev, "Sensor enable = 0x%06lX\n",
			motosh_g_wake_sensor_state);
		break;
	case MOTOSH_IOCTL_GET_WAKESENSORS:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_GET_WAKESENSORS");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = WAKESENSOR_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, 3);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get sensors failed\n");
				break;
			}
			bytes[0] = readbuff[0];
			bytes[1] = readbuff[1];
			bytes[2] = readbuff[2];
		} else {
			bytes[0] = motosh_g_wake_sensor_state & 0xFF;
			bytes[1] = (motosh_g_wake_sensor_state >> 8) & 0xFF;
			bytes[2] = (motosh_g_wake_sensor_state >> 16) & 0xFF;
		}
		if (copy_to_user(argp, bytes, 2 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_ALGOS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ALGOS");
		if (copy_from_user(&bytes, argp, 2 * sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Copy set algos returned error\n");
			err = -EFAULT;
			break;
		}
		dev_dbg(&ps_motosh->client->dev,
			"Set algos config: 0x%x", (bytes[1] << 8) | bytes[0]);
		cmdbuff[0] = ALGO_CONFIG;
		cmdbuff[1] = bytes[0];
		cmdbuff[2] = bytes[1];
		motosh_g_algo_state = (cmdbuff[2] << 8) | cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 3);
		break;
	case MOTOSH_IOCTL_GET_ALGOS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_ALGOS");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = ALGO_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, 2);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get algos failed\n");
				break;
			}
			bytes[0] = readbuff[0];
			bytes[1] = readbuff[1];
		} else {
			bytes[0] = motosh_g_algo_state & 0xFF;
			bytes[1] = (motosh_g_algo_state >> 8) & 0xFF;
		}
		dev_info(&ps_motosh->client->dev,
			"Get algos config: 0x%x", (bytes[1] << 8) | bytes[0]);
		if (copy_to_user(argp, bytes, 2 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_MAG_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_MAG_CAL");
		if (copy_from_user(&cmdbuff[1], argp,
			MOTOSH_MAG_CAL_SIZE)) {
			dev_err(&ps_motosh->client->dev,
				"Copy set mag cal returned error\n");
			err = -EFAULT;
			break;
		}
		memcpy(motosh_g_mag_cal, &cmdbuff[1],
			MOTOSH_MAG_CAL_SIZE);
		cmdbuff[0] = MAG_CALIBRATION;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff,
				(MOTOSH_MAG_CAL_SIZE + 1));
		break;
	case MOTOSH_IOCTL_GET_MAG_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_MAG_CAL");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = MAG_CALIBRATION;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, MOTOSH_MAG_CAL_SIZE);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get mag cal failed\n");
				break;
			}
		} else {
			memcpy(&readbuff[0], motosh_g_mag_cal,
				MOTOSH_MAG_CAL_SIZE);
		}
		if (copy_to_user(argp, &readbuff[0],
				MOTOSH_MAG_CAL_SIZE))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_MOTION_DUR:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_MOTION_DUR");
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy set motion dur returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = MOTION_DUR;
		cmdbuff[1] = addr & 0xFF;
		motosh_g_motion_dur =  cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_ZRMOTION_DUR:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_ZRMOTION_DUR");
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy zmotion dur returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = ZMOTION_DUR;
		cmdbuff[1] = addr & 0xFF;
		motosh_g_zmotion_dur =  cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_GET_DOCK_STATUS:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_GET_DOCK_STATUS");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = DOCKED_DATA;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, 1);
			byte = readbuff[0];
		} else
			byte = 0;
		if (copy_to_user(argp, &byte, sizeof(byte)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_TEST_READ:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_TEST_READ");
		if (ps_motosh->mode > BOOTMODE) {
			err = motosh_i2c_read(ps_motosh, &byte, 1);
			/* motosh will return num of bytes read or error */
			if (err > 0)
				err = byte;
		}
		break;
	case MOTOSH_IOCTL_TEST_WRITE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_TEST_WRITE");
		if (ps_motosh->mode <= BOOTMODE)
			break;
		if (copy_from_user(&byte, argp, sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Copy test write returned error\n");
			err = -EFAULT;
			break;
		}
		err = motosh_i2c_write(ps_motosh, &byte, 1);
		break;
	case MOTOSH_IOCTL_SET_POSIX_TIME:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_POSIX_TIME");
		if (ps_motosh->mode <= BOOTMODE)
			break;
		if (copy_from_user(&current_posix_time, argp,
			 sizeof(current_posix_time))) {
			dev_err(&ps_motosh->client->dev,
				"Copy from user returned error\n");
			err = -EFAULT;
			break;
		}
		getnstimeofday(&current_time);
		motosh_time_delta = current_posix_time - current_time.tv_sec;
		cmdbuff[0] = POSIX_TIME;
		cmdbuff[1] = (unsigned char)(current_posix_time >> 24);
		cmdbuff[2] = (unsigned char)((current_posix_time >> 16)
				& 0xff);
		cmdbuff[3] = (unsigned char)((current_posix_time >> 8) & 0xff);
		cmdbuff[4] = (unsigned char)((current_posix_time) & 0xff);
		err = motosh_i2c_write(ps_motosh, cmdbuff, 5);
		break;
	case MOTOSH_IOCTL_SET_ALGO_REQ:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ALGO_REQ");
		/* copy algo into bytes[2] */
		if (copy_from_user(&bytes, argp, 2 * sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (bytes[1] << 8) | bytes[0];
		/* copy len into byte */
		if (copy_from_user(&byte, argp + 2 * sizeof(unsigned char),
				sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req copy byte returned error\n");
			err = -EFAULT;
			break;
		}
		/* algo req register */
		dev_dbg(&ps_motosh->client->dev,
			"Set algo req, algo idx: %d, len: %u\n", addr, byte);
		if (addr < MOTOSH_NUM_ALGOS) {
			cmdbuff[0] = motosh_algo_info[addr].req_register;
			dev_dbg(&ps_motosh->client->dev,
				"Register: 0x%x", cmdbuff[0]);
		} else {
			dev_err(&ps_motosh->client->dev,
				"Set algo req invalid arg\n");
			err = -EFAULT;
			break;
		}
		if (byte > ALGO_RQST_DATA_SIZE) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req invalid size arg\n");
			err = -EFAULT;
			break;
		}
		if (copy_from_user(&cmdbuff[1],
			argp + 2 * sizeof(unsigned char)
			+ sizeof(byte), byte)) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req copy req info returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_g_algo_requst[addr].size = byte;
		memcpy(motosh_g_algo_requst[addr].data,
			&cmdbuff[1], byte);
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff,
				1 + byte);
		break;
	case MOTOSH_IOCTL_GET_ALGO_EVT:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_ALGO_EVT");
		if (ps_motosh->mode <= BOOTMODE) {
			err = -EFAULT;
			break;
		}
		/* copy algo into bytes[2] */
		if (copy_from_user(&bytes, argp, 2 * sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Get algo evt copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (bytes[1] << 8) | bytes[0];
		/* algo evt register */
		dev_dbg(&ps_motosh->client->dev,
			"Get algo evt, algo idx: %d\n", addr);
		if (addr < MOTOSH_NUM_ALGOS) {
			cmdbuff[0] = motosh_algo_info[addr].evt_register;
			dev_dbg(&ps_motosh->client->dev,
				"Register: 0x%x", cmdbuff[0]);
		} else {
			dev_err(&ps_motosh->client->dev,
				"Get algo evt invalid arg\n");
			err = -EFAULT;
			break;
		}
		err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff, 1,
			motosh_algo_info[addr].evt_size);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Get algo evt failed\n");
			break;
		}
		if (copy_to_user(argp + 2 * sizeof(unsigned char),
			readbuff, motosh_algo_info[addr].evt_size))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_WRITE_REG:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_WRITE_REG");
		if (ps_motosh->mode <= BOOTMODE) {
			err = -EFAULT;
			break;
		}
		/* copy addr and size */
		if (copy_from_user(&rw_bytes, argp, sizeof(rw_bytes))) {
			dev_err(&ps_motosh->client->dev,
				"Write Reg, copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (rw_bytes[0] << 8) | rw_bytes[1];
		data_size = (rw_bytes[2] << 8) | rw_bytes[3];

		/* fail if the write size is too large */
		if (data_size > MOTOSH_MAXDATA_LENGTH - 1) {
			err = -EFAULT;
			dev_err(&ps_motosh->client->dev,
				"Write Reg, data_size > %d\n",
				MOTOSH_MAXDATA_LENGTH - 1);
			break;
		}

		/* copy in the data */
		if (copy_from_user(&cmdbuff[1], argp +
			sizeof(rw_bytes), data_size)) {
			dev_err(&ps_motosh->client->dev,
				"Write Reg copy from user returned error\n");
			err = -EFAULT;
			break;
		}

		/* setup the address */
		cmdbuff[0] = addr;

		/* + 1 for the address in [0] */
		err = motosh_i2c_write(ps_motosh, cmdbuff, data_size + 1);

		if (err < 0)
			dev_err(&motosh_misc_data->client->dev,
				"Write Reg unable to write to direct reg %d\n",
				err);
		break;
	case MOTOSH_IOCTL_READ_REG:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_READ_REG");
		if (ps_motosh->mode <= BOOTMODE) {
			err = -EFAULT;
			break;
		}
		/* copy addr and size */
		if (copy_from_user(&rw_bytes, argp, sizeof(rw_bytes))) {
			dev_err(&ps_motosh->client->dev,
			    "Read Reg, copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (rw_bytes[0] << 8) | rw_bytes[1];
		data_size = (rw_bytes[2] << 8) | rw_bytes[3];

		if (data_size > MOTOSH_MAXDATA_LENGTH) {
			dev_err(&ps_motosh->client->dev,
				"Read Reg error, size too large\n");
			err = -EFAULT;
			break;
		}

		/* setup the address */
		cmdbuff[0] = addr;
		err = motosh_i2c_write_read(ps_motosh,
			cmdbuff, readbuff, 1, data_size);

		if (err < 0) {
			dev_err(&motosh_misc_data->client->dev,
				"Read Reg, unable to read from direct reg %d\n",
				err);
			break;
		}

		if (copy_to_user(argp, readbuff, data_size)) {
			dev_err(&ps_motosh->client->dev,
				"Read Reg error copying to user\n");
			err = -EFAULT;
			break;
		}
		break;
	case MOTOSH_IOCTL_SET_IR_CONFIG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_IR_CONFIG");
		cmdbuff[0] = IR_CONFIG;
		if (copy_from_user(&cmdbuff[1], argp, 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy size from user returned error\n");
			err = -EFAULT;
			break;
		}
		if (cmdbuff[1] > sizeof(motosh_g_ir_config_reg)) {
			dev_err(&ps_motosh->client->dev,
				"IR Config too big: %d > %zu\n",
				cmdbuff[1],
				sizeof(motosh_g_ir_config_reg));
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&cmdbuff[2], argp + 1,
				   cmdbuff[1] - 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy data from user returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_g_ir_config_reg_restore = 1;
		memcpy(motosh_g_ir_config_reg, &cmdbuff[1], cmdbuff[1]);

		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff,
					cmdbuff[1] + 1);
		dev_dbg(&motosh_misc_data->client->dev,
			"SET_IR_CONFIG: Writing %d bytes (err=%d)\n",
			cmdbuff[1] + 1, err);
		if (err < 0)
			dev_err(&motosh_misc_data->client->dev,
				"Unable to write IR config reg %d\n", err);
		break;
	case MOTOSH_IOCTL_GET_IR_CONFIG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_IR_CONFIG");
		if (copy_from_user(&byte, argp, 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy size from user returned error\n");
			err = -EFAULT;
			break;
		}
		if (byte > sizeof(motosh_g_ir_config_reg)) {
			dev_err(&ps_motosh->client->dev,
				"IR Config too big: %d > %d\n", byte,
				(int)sizeof(motosh_g_ir_config_reg));
			err = -EINVAL;
			break;
		}

		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = IR_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, byte);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Get IR config failed: %d\n", err);
				break;
			}
		} else {
			memcpy(readbuff, motosh_g_ir_config_reg, byte);
		}
		if (copy_to_user(argp, readbuff, byte))
			err = -EFAULT;

		break;
	case MOTOSH_IOCTL_SET_IR_GESTURE_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_IR_GESTURE_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy IR gesture delay returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = IR_GESTURE_RATE;
		cmdbuff[1] = delay;
		motosh_g_ir_gesture_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_IR_RAW_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_IR_RAW_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy IR raw delay returned error\n");
			err = -EFAULT;
			break;
		}
		cmdbuff[0] = IR_RAW_RATE;
		cmdbuff[1] = delay;
		motosh_g_ir_raw_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, cmdbuff, 2);
		break;
#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
	case MOTOSH_IOCTL_ENABLE_BREATHING:
		if (ps_motosh->mode <= BOOTMODE) {
			err = -EBUSY;
			break;
		}
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Enable Breathing, copy byte returned error\n");
			err = -EFAULT;
			break;
		}

		if (byte) {
			/* Check for any touch configuration updates
			 * and if needed write them to the sensorhub */
			motosh_check_touch_config_locked(NORMAL_CHECK);

			motosh_vote_aod_enabled_locked(ps_motosh,
				AOD_QP_ENABLED_VOTE_USER, true);
		} else {
			motosh_vote_aod_enabled_locked(ps_motosh,
				AOD_QP_ENABLED_VOTE_USER, false);
		}
		motosh_resolve_aod_enabled_locked(ps_motosh);
		/* the user's vote can not fail */
		err = 0;
		break;
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */
	case MOTOSH_IOCTL_SET_LOWPOWER_MODE:
		if (ps_motosh->mode <= BOOTMODE) {
			err = -EBUSY;
			break;
		}
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_LOWPOWER_MODE");
		if (copy_from_user(&cmdbuff[0], argp, 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy size from user returned error\n");
			err = -EFAULT;
			break;
		}

		err = 0;
		if (cmdbuff[0] != 0 && lowpower_mode == 0) {
			/* allow sensorhub to sleep */
			motosh_sleep(ps_motosh);
			lowpower_mode = cmdbuff[0];
		} else if (cmdbuff[0] == 0 && lowpower_mode == 1) {
			/* keep sensorhub awake */
			motosh_wake(ps_motosh);
			lowpower_mode = cmdbuff[0];
		}
		break;
	case MOTOSH_IOCTL_SET_FLUSH:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_FLUSH");
		if (ps_motosh->mode <= BOOTMODE)
			break;
		if (copy_from_user(&handle, argp, sizeof(unsigned int))) {
			dev_err(&ps_motosh->client->dev,
				"Copy flush handle returned error\n");
			err = -EFAULT;
			break;
		}
		/* batchable sensors must invoke irq work to retrieve
		   buffered data from the hub. Only Accel is currently
		   batched */
		handle = cpu_to_be32(handle);
		if (handle == ID_A) {
			ps_motosh->nwake_flush_req |= FLUSH_ACCEL_REQ;
			queue_work(ps_motosh->irq_work_queue,
				   &ps_motosh->irq_work);
		} else
			motosh_as_data_buffer_write(ps_motosh, DT_FLUSH,
						    (char *)&handle,
						    4, 0, false);
		break;
	case MOTOSH_IOCTL_GET_GYRO_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_GYRO_CAL");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = GYRO_CAL;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
				readbuff, 1, MOTOSH_GYRO_CAL_SIZE);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get gyro cal failed\n");
				break;
			}
			memcpy(motosh_g_gyro_cal, &readbuff[0],
				MOTOSH_GYRO_CAL_SIZE);
		}
		if (copy_to_user(argp, motosh_g_gyro_cal,
				MOTOSH_GYRO_CAL_SIZE))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_GYRO_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_GYRO_CAL");
		if (copy_from_user(&cmdbuff[1], argp,
			MOTOSH_GYRO_CAL_SIZE)) {
			dev_err(&ps_motosh->client->dev,
				"Copy set gyro cal returned error\n");
			err = -EFAULT;
			break;
		}
		memcpy(motosh_g_gyro_cal, &cmdbuff[1],
			MOTOSH_GYRO_CAL_SIZE);
		cmdbuff[0] = GYRO_CAL;
		if (ps_motosh->mode > BOOTMODE) {
			err = motosh_i2c_write(ps_motosh, cmdbuff,
				(MOTOSH_GYRO_CAL_SIZE + 1));
			if (err < 0)
				dev_err(&ps_motosh->client->dev,
					"Writing set gyro cal failed\n");
		}
		break;
	case MOTOSH_IOCTL_GET_ACCEL_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_ACCEL_CAL");
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = ACCEL_CAL;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
					readbuff, 1, MOTOSH_ACCEL_CAL_SIZE);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get accel cal failed\n");
				break;
			}
			memcpy(motosh_g_accel_cal, readbuff,
					MOTOSH_ACCEL_CAL_SIZE);
		}
		if (copy_to_user(argp, motosh_g_accel_cal,
					MOTOSH_ACCEL_CAL_SIZE))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_ACCEL_CAL:
		dev_dbg(&ps_motosh->client->dev,
				"MOTOSH_IOCTL_SET_ACCEL_CAL");
		if (copy_from_user(&cmdbuff[1], argp, MOTOSH_ACCEL_CAL_SIZE)) {
			dev_err(&ps_motosh->client->dev,
					"Copy set accel cal returned error\n");
			err = -EFAULT;
			break;
		}
		memcpy(motosh_g_accel_cal, &cmdbuff[1], MOTOSH_ACCEL_CAL_SIZE);
		cmdbuff[0] = ACCEL_CAL;
		if (ps_motosh->mode > BOOTMODE) {
			err = motosh_i2c_write(ps_motosh, cmdbuff,
					(MOTOSH_ACCEL_CAL_SIZE + 1));
			if (err < 0)
				dev_err(&ps_motosh->client->dev,
					"Writing set accel cal failed\n");
		}
		break;
	case MOTOSH_IOCTL_SET_ALS_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_ALS_DELAY");
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy als delay returned error");
			err = -EFAULT;
			break;
		}
		motosh_g_als_delay = delay;
		if (ps_motosh->mode > BOOTMODE) {
			cmdbuff[0] = ALS_UPDATE_RATE;
			cmdbuff[1] = delay >> 8;
			cmdbuff[2] = delay & 0xFF;
			err = motosh_i2c_write(ps_motosh, cmdbuff, 3);
		}
		break;
#ifdef CONFIG_CYPRESS_CAPSENSE_HSSP
	case MOTOSH_IOCTL_SET_ANTCAP_ENABLE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ANTCAP_ENABLE");
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			dev_err(&ps_motosh->client->dev, "Copy set antcap enable returned error\n");
			err = -EFAULT;
			break;
		}

		motosh_g_antcap_sw_ready = 1;
		if (byte)
			motosh_g_antcap_enabled |=  ANTCAP_ENABLED;
		else
			motosh_g_antcap_enabled &= ~ANTCAP_ENABLED;

		err = motosh_antcap_i2c_send_enable(0);
		dev_info(&ps_motosh->client->dev,
			"motosh_antcap enable I (err=%04x): en=%02x st=%02x\n",
			err, motosh_g_antcap_enabled, motosh_g_conn_state);

		break;
	case MOTOSH_IOCTL_SET_ANTCAP_CFG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ANTCAP_CFG");
		memcpy(&cmdbuff[1], motosh_g_antcap_cfg,
			MOTOSH_ANTCAP_CFG_BUFF_SIZE);

		if (ps_motosh->mode > BOOTMODE) {
			/**********************************************/
			/* copy configuration data into ANTCAP_CONFIG */
			/**********************************************/
			cmdbuff[0] = ANTCAP_CONFIG;
			err = motosh_i2c_write(ps_motosh, cmdbuff,
				(MOTOSH_ANTCAP_CFG_BUFF_SIZE + 1));
			if (err) {
				dev_err(&ps_motosh->client->dev,
					"Copy set antcap cfg data: failed\n");
				break;
			}
		}
		break;
	case MOTOSH_IOCTL_SET_ANTCAP_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ANTCAP_CAL");
		if (copy_from_user(&cmdbuff[1], argp,
				   MOTOSH_ANTCAP_CAL_BUFF_SIZE)) {
			dev_err(&ps_motosh->client->dev, "Copy set antcap cal returned error\n");
			err = -EFAULT;
			break;
		}
		memcpy(motosh_g_antcap_cal, &cmdbuff[1],
		       MOTOSH_ANTCAP_CAL_BUFF_SIZE);

		if ((motosh_g_antcap_cal[0] != 0x86)
			|| (motosh_g_antcap_cal[1] < 0x10)
			|| (motosh_g_antcap_cal[2])) {

			dev_err(&ps_motosh->client->dev,
				"Copy set antcap cal data: cal data has issues\n");
			err = -EFAULT;
			break;
		}

		dev_info(&ps_motosh->client->dev, "motosh_antcap_i2c_getver\n");
		err = motosh_antcap_i2c_getver_poll(0);
		if (err) {
			dev_err(&ps_motosh->client->dev,
				"motosh_antcap_i2c_getver_poll 1 failed: %04x\n",
				err);
			break;
		}

		dev_info(&ps_motosh->client->dev, "motosh_antcap_i2c_getcal\n");
		err = motosh_antcap_i2c_getcal_poll(0);
		if (err) {
			dev_err(&ps_motosh->client->dev,
				"motosh_antcap_i2c_getcal_poll failed: %04x\n",
				err);
			break;
		}

		dev_info(&ps_motosh->client->dev,
			"motosh_antcap_i2c_check_cal\n");
		chk = motosh_antcap_check_cal();

		dev_info(&ps_motosh->client->dev,
			"motosh_antcap_i2c_getver\n");
		err = motosh_antcap_i2c_getver_poll(0);

		if (err) {
			dev_err(&ps_motosh->client->dev,
				"motosh_antcap_i2c_getver_poll 2 failed: %04x\n",
				err);
			break;
		}

		if (chk) {
			dev_info(&ps_motosh->client->dev,
				"motosh_antcap_i2c_setcal_poll\n");

			err = motosh_antcap_i2c_setcal_poll(0);
			if (err) {
				dev_err(&ps_motosh->client->dev,
				"motosh_antcap_i2c_setcal_poll failed: %04x\n",
				err);
				break;
			}
		}

		motosh_g_antcap_enabled &= ~ANTCAP_CHECK_CAL;

		break;
#ifdef CONFIG_SENSORS_MOTOSH_HEADSET
	case MOTOSH_IOCTL_SET_HEADSET_STATE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_HEADSET_STATE");
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Copy set headset state returned error\n");
			err = -EFAULT;
			break;
		}
		if (byte)
			motosh_g_conn_state |=  ANTCAP_HEADSET;
		else
			motosh_g_conn_state &= ~ANTCAP_HEADSET;

		err = motosh_antcap_i2c_send_enable(0);
		dev_info(&ps_motosh->client->dev,
			"motosh_antcap headset (err=%04x): en=%02x st=%02x\n",
			err, motosh_g_antcap_enabled, motosh_g_conn_state);

		break;
#endif /* CONFIG_SENSORS_MOTOSH_HEADSET */
	case MOTOSH_IOCTL_SET_USBCONN_STATE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_USBCONN_STATE");
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Copy set usbconn state returned error\n");
			err = -EFAULT;
			break;
		}
		if (byte)
			motosh_g_conn_state |=  ANTCAP_USB;
		else
			motosh_g_conn_state &= ~ANTCAP_USB;

		err = motosh_antcap_i2c_send_enable(0);
		dev_info(&ps_motosh->client->dev,
			"motosh_antcap usbconn (err=%04x): en=%02x st=%02x\n",
			err, motosh_g_antcap_enabled, motosh_g_conn_state);

		break;
	case MOTOSH_IOCTL_SET_AIRPLANE_MODE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_AIRPLANE_MODE");
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Copy set airplane mode returned error\n");
			err = -EFAULT;
			break;
		}
		if (byte) {
			motosh_g_antcap_enabled |=  ANTCAP_AIRPLANE;
			motosh_g_conn_state     |=  ANTCAP_APM;
		} else {
			motosh_g_antcap_enabled &= ~ANTCAP_AIRPLANE;
			motosh_g_conn_state     &= ~ANTCAP_APM;
		}

		err = motosh_antcap_i2c_send_enable(0);
		dev_info(&ps_motosh->client->dev,
			"motosh_antcap airplane (err=%04x): en=%02x st=%02x\n",
			err, motosh_g_antcap_enabled, motosh_g_conn_state);

		break;
#endif /* CONFIG_CYPRESS_CAPSENSE_HSSP */

	case MOTOSH_IOCTL_PASSTHROUGH:
		dev_dbg(&ps_motosh->client->dev,
				"MOTOSH_IOCTL_PASSTHROUGH");
		if (ps_motosh->mode <= BOOTMODE) {
			err = -EFAULT;
			break;
		}

		memset(cmdbuff, 0, MOTOSH_PASSTHROUGH_SIZE + 1);
		/* copy bus, addr, reg, R/W and size */
		if (copy_from_user(&cmdbuff[1], argp, 6)) {
			dev_err(&ps_motosh->client->dev,
					"Passthrough, copy info error");
			err = -EFAULT;
			break;
		}
		read_write = cmdbuff[5];
		data_size = cmdbuff[6];

		if (read_write == 0) { /* Read operation */
			if (data_size > (MOTOSH_PASSTHROUGH_SIZE - 1)) {
				dev_err(&ps_motosh->client->dev,
					"Passthrough, data size too large %d",
					data_size);
				err = -EFAULT;
				break;
			}
		} else if (read_write == 1) { /* Write operation */
			if (data_size > (MOTOSH_PASSTHROUGH_SIZE - 6)) {
				dev_err(&ps_motosh->client->dev,
					"Passthrough, data size too large %d",
					data_size);
				err = -EFAULT;
				break;
			}

			/* copy in the data to write */
			if (copy_from_user(&cmdbuff[7], argp + 6, data_size)) {
				dev_err(&ps_motosh->client->dev,
						"Passthrough, copy data error");
				err = -EFAULT;
				break;
			}
		} else {
			dev_err(&ps_motosh->client->dev,
					"Passthrough, invalid r/w %d",
					read_write);
			err = -EFAULT;
			break;
		}

		/* Write the command to the sensor hub */
		cmdbuff[0] = I2C_PASSTHROUGH_COMMAND;
		err = motosh_i2c_write(ps_motosh, cmdbuff,
				MOTOSH_PASSTHROUGH_SIZE + 1);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
					"Passthrough, write error %d",
					err);
			err = -EFAULT;
			break;
		}

		/* Read the response from  the sensor hub */
		loop_count = 0;
		readbuff[0] = 0;
		do {
			cmdbuff[0] = I2C_PASSTHROUGH_RESPONSE;
			err = motosh_i2c_write_read(ps_motosh, cmdbuff,
					readbuff, 1, MOTOSH_PASSTHROUGH_SIZE);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
						"Passthrough, read error %d",
						err);
				err = -EFAULT;
				break;
			}
			loop_count++;
			if (loop_count == 10)
				break;
		} while (readbuff[0] == 0);
		if (readbuff[0] != 1) {
			dev_err(&ps_motosh->client->dev,
					"Passthrough, read failed %d",
					readbuff[0]);
			err = -EFAULT;
			break;
		}
		err = 0;

		if (read_write == 0) {
			/* Return the hub register data read */
			if (copy_to_user(argp, &readbuff[1], data_size)) {
				dev_err(&ps_motosh->client->dev,
						"Passthrough, copy back error");
				err = -EFAULT;
				break;
			}
		}
	}

	motosh_sleep(ps_motosh);
	mutex_unlock(&ps_motosh->lock);
	return err;
}

int motosh_set_rv_6axis_update_rate(
	struct motosh_data *ps_motosh,
	const uint8_t newDelay)
{
	int err = 0;
	unsigned char cmdbuff[2];

	if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
		return -EINTR;
	motosh_wake(ps_motosh);

	motosh_g_rv_6axis_delay = newDelay;
	if (ps_motosh->mode <= BOOTMODE)
		goto EPILOGUE;

	cmdbuff[0] = QUAT_6AXIS_UPDATE_RATE;
	cmdbuff[1] = newDelay;
	err = motosh_i2c_write(ps_motosh, cmdbuff, 2);

EPILOGUE:
	motosh_sleep(ps_motosh);
	mutex_unlock(&ps_motosh->lock);

	return err;
}

int motosh_set_rv_9axis_update_rate(
	struct motosh_data *ps_motosh,
	const uint8_t newDelay)
{
	int err = 0;
	unsigned char cmdbuff[2];

	if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
		return -EINTR;
	motosh_wake(ps_motosh);

	motosh_g_rv_9axis_delay = newDelay;
	if (ps_motosh->mode <= BOOTMODE)
		goto EPILOGUE;

	cmdbuff[0] = QUAT_9AXIS_UPDATE_RATE;
	cmdbuff[1] = newDelay;
	err = motosh_i2c_write(ps_motosh, cmdbuff, 2);

EPILOGUE:
	motosh_sleep(ps_motosh);
	mutex_unlock(&ps_motosh->lock);

	return err;
}

int motosh_set_gravity_update_rate(
	struct motosh_data *ps_motosh,
	const uint8_t newDelay)
{
	int err = 0;
	unsigned char cmdbuff[2];

	if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
		return -EINTR;
	motosh_wake(ps_motosh);

	motosh_g_gravity_delay = newDelay;
	if (ps_motosh->mode <= BOOTMODE)
		goto EPILOGUE;

	cmdbuff[0] = GRAVITY_UPDATE_RATE;
	cmdbuff[1] = newDelay;
	err = motosh_i2c_write(ps_motosh, cmdbuff, 2);

EPILOGUE:
	motosh_sleep(ps_motosh);
	mutex_unlock(&ps_motosh->lock);

	return err;
}

int motosh_set_linear_accel_update_rate(
	struct motosh_data *ps_motosh,
	const uint8_t newDelay)
{
	int err = 0;
	unsigned char cmdbuff[2];

	if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
		return -EINTR;
	motosh_wake(ps_motosh);

	motosh_g_linear_accel_delay = newDelay;
	if (ps_motosh->mode <= BOOTMODE)
		goto EPILOGUE;

	cmdbuff[0] = LINEAR_ACCEL_UPDATE_RATE;
	cmdbuff[1] = newDelay;
	err = motosh_i2c_write(ps_motosh, cmdbuff, 2);

EPILOGUE:
	motosh_sleep(ps_motosh);
	mutex_unlock(&ps_motosh->lock);

	return err;
}
