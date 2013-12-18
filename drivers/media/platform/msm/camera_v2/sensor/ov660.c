/*
 * Copyright (C) 2013, Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "ov660.h"
#include "msm_camera_i2c.h"
#include "msm_cci.h"
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <media/msm_cam_sensor.h>

#define OV660_CHIP_ID_ADDR 0x6080
#define OV660_CHIP_ID_DATA 0x0660
#define OV660_I2C_ADDRESS 0x6A

static struct msm_sensor_ctrl_t ov660_ctrl;

/* Factory Check */
static int ov660_detected;
module_param(ov660_detected, int, 0444);

static int camera_dev_open(struct inode *inode, struct file *file);
static int camera_dev_release(struct inode *inode, struct file *file);
static long camera_dev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg);
static int camera_dev_read_stats(struct file *file, char __user *buf,
		size_t size, loff_t *ppos);

const struct file_operations camera_dev_fops = {
	.owner = THIS_MODULE,
	.open = camera_dev_open,
	.release = camera_dev_release,
	.unlocked_ioctl = camera_dev_ioctl,
	.read = camera_dev_read_stats
};

static struct miscdevice cam_misc_device0 = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "motcamera0",
	.fops = &camera_dev_fops
};

static uint16_t sensor_addr = 0x6c;

static int ov660_swap_i2c_address(int enable_swap)
{
	if (enable_swap == 1) {
		ov660_ctrl.sensordata->slave_info->sensor_slave_addr =
			OV660_I2C_ADDRESS;
		ov660_ctrl.sensor_i2c_client->cci_client->sid =
			(OV660_I2C_ADDRESS >> 1);
	} else {
		ov660_ctrl.sensordata->slave_info->sensor_slave_addr =
			sensor_addr;
		ov660_ctrl.sensor_i2c_client->cci_client->sid =
			(sensor_addr >> 1);
	}
	return 0;
}

static int camera_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int camera_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long camera_dev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	long rc = 0;
	void __user *argp = (void __user *)arg;
	struct msm_camera_i2c_reg_setting conf_array;
	struct msm_camera_i2c_reg_array *reg_setting = NULL;
	/* TODO: mutex_lock(); */

	switch (cmd) {
	case CAMERA_WRITE_I2C: {
		if (copy_from_user(&conf_array,
					(void *)argp,
					sizeof(struct
						msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
				(sizeof(struct msm_camera_i2c_reg_array)),
				GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
					(void *)conf_array.reg_setting,
					conf_array.size *
					sizeof(struct
						msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		ov660_swap_i2c_address(1);
		rc = ov660_ctrl.sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(ov660_ctrl.sensor_i2c_client,
					&conf_array);
		ov660_swap_i2c_address(0);
		kfree(reg_setting);
		break;
	}
	default:
		break;
	}
	/* TODO: mutex_unlock(); */
	return rc;
}

static ssize_t camera_dev_read_stats(struct file *file, char __user *buf,
		size_t size, loff_t *ppos)
{
	int rc = 0;
	return rc;
}

int32_t ov660_set_i2c_bypass(struct msm_sensor_ctrl_t *s_ctrl, int bypassOn)
{
	uint16_t data;
	int32_t rc = 0;

	ov660_swap_i2c_address(1);

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			I2C_ADDR_BYPASS,
			&data, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: Unable to read %x\n", __func__, I2C_ADDR_BYPASS);
		goto done;
	}

	if (bypassOn == 0)
		data = data & 0xBF; /* Disable */
	else
		data = data | 0x40; /* Enable */

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			I2C_ADDR_BYPASS,
			data, MSM_CAMERA_I2C_BYTE_DATA);
	pr_debug("ov660_set_i2c_bypass rc = %d", rc);

	if (rc < 0)
		pr_err("%s: unable to set 0x6106 to %x\n",
				__func__, data);
done:
	ov660_swap_i2c_address(0);
	return rc;
}

/* This function replaces the current probe since the power up
 * sequence and power down sequence is tied to the camera driver
 * itself. The camera being used will see if the ov660 is
 * active by calling this probe function */
int32_t ov660_check_probe(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t data = 0x0;

	/* Copy 10MP sensor information for ov660 use */
	memcpy(&ov660_ctrl, s_ctrl, sizeof(struct msm_sensor_ctrl_t));

	sensor_addr = s_ctrl->sensordata->slave_info->sensor_slave_addr;
	if (ov660_ctrl.sensordata == NULL) {
		rc = -ENODEV;
		goto exit;
	}

	ov660_swap_i2c_address(1);
	rc = ov660_ctrl.sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			OV660_CHIP_ID_ADDR,
			&data, MSM_CAMERA_I2C_WORD_DATA);

	/* Now checking to see if we match the correct chip id */
	if (data == OV660_CHIP_ID_DATA) {
		pr_info("%s: successful\n", __func__);
		ov660_detected = 1;
	} else {
		pr_err("%s: ov660 does not exist!\n", __func__);
		rc = -ENODEV;
		ov660_detected = 0;
	}
	ov660_swap_i2c_address(0);
exit:
	return rc;
}

static int __init ov660_init(void)
{
	if (misc_register(&cam_misc_device0)) {
		pr_err("%s: Unable to register Mot camera misc device!\n",
				__func__);
	}

	return 0;
}

static void __exit ov660_exit(void)
{
	misc_deregister(&cam_misc_device0);
}

module_init(ov660_init);
module_exit(ov660_exit);

MODULE_DESCRIPTION("ASIC driver for 10 MP RGBC Cameras");
MODULE_AUTHOR("MOTOROLA");
MODULE_LICENSE("GPL");
