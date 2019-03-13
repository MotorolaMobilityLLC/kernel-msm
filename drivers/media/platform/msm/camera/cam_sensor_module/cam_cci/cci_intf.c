/*
 * Copyright (C) 2018 Motorola Mobility LLC.
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

#include <linux/module.h>
#include <uapi/linux/media.h>
#include <media/cci_intf.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/msmb_camera.h>
#include <cam_subdev.h>
#include <cam_sensor_cmn_header.h>
#include <cam_cci_dev.h>

#define CCI_INTF_NAME "msm_cci_intf"

//#define CCI_INTF_DEBUG
#ifdef CCI_INTF_DEBUG
#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug pr_err
#endif

struct msm_cci_intf_ctrl_t {
	struct cam_subdev v4l2_dev_str;
	struct platform_device *pdev;
	char device_name[20];
};

static struct msm_cci_intf_ctrl_t fctrl;

static enum camera_sensor_i2c_type cci_intf_xfer_addr_convert(unsigned short width)
{
	enum camera_sensor_i2c_type cci_width = CAMERA_SENSOR_I2C_TYPE_BYTE;
	switch (width) {
		case 1:
			cci_width = CAMERA_SENSOR_I2C_TYPE_BYTE;
			break;
		case 2:
			cci_width = CAMERA_SENSOR_I2C_TYPE_WORD;
			break;
		case 3:
			cci_width = CAMERA_SENSOR_I2C_TYPE_3B;
			break;
		case 4:
			cci_width = CAMERA_SENSOR_I2C_TYPE_DWORD;
			break;
		default:
			cci_width = CAMERA_SENSOR_I2C_TYPE_BYTE;
	}
	return cci_width;
}

static int32_t cci_intf_xfer(struct v4l2_subdev *sd,
		struct msm_cci_intf_xfer *xfer,
		unsigned int cmd)
{
	int32_t rc=0, rc2=0;
	struct cam_sensor_cci_client cci_info = {
		.cci_subdev     = cam_cci_get_subdev(0),
		.cci_i2c_master = xfer->cci_bus,
		.i2c_freq_mode = I2C_FAST_MODE,
		.sid            = xfer->slave_addr,
	};
	struct cam_cci_ctrl cci_ctrl = {
		.cci_info = &cci_info,
	};
	int i;
	struct cam_sensor_i2c_reg_array *reg_conf_tbl;

	pr_debug("%s cmd:%d bus:%d devaddr:%02x regw:%d rega:%04x count:%d\n",
			__func__, cmd, xfer->cci_bus, xfer->slave_addr,
			xfer->reg.width, xfer->reg.addr, xfer->data.count);

	if ((xfer->cci_bus > 1 || xfer->slave_addr > 0x7F ||
			xfer->reg.width < 1 || xfer->reg.width > 2 ||
			xfer->reg.addr > ((1<<(8*xfer->reg.width))-1) ||
			xfer->data.count < 1 ||
			xfer->data.count > MSM_CCI_INTF_MAX_XFER)  &&
		(cmd == MSM_CCI_INTF_WRITE || cmd == MSM_CCI_INTF_READ)){
		pr_err("%s: cci reg/data setting is invalid\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case MSM_CCI_INTF_INIT:
		/* init */
		cci_ctrl.cmd = MSM_CCI_INIT;
		rc = v4l2_subdev_call(cam_cci_get_subdev(0),
				core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		if (rc < 0) {
			pr_err("%s: cci init fail (%d)\n", __func__, rc);
			return rc;
		}
		pr_debug("%s: MSM_CCI_INIT (master:%d, slave_addr:%x) ret:%d", __func__,
		       xfer->cci_bus, xfer->slave_addr, rc);
		break;

	case MSM_CCI_INTF_READ:
		/* read */
		cci_ctrl.cmd = MSM_CCI_I2C_READ;
		cci_ctrl.cfg.cci_i2c_read_cfg.addr = xfer->reg.addr;
		cci_ctrl.cfg.cci_i2c_read_cfg.addr_type =
			cci_intf_xfer_addr_convert(xfer->reg.width);
		cci_ctrl.cfg.cci_i2c_read_cfg.data = xfer->data.buf;
		cci_ctrl.cfg.cci_i2c_read_cfg.num_byte = xfer->data.count;
		rc = v4l2_subdev_call(cam_cci_get_subdev(0),
			core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		if (rc < 0) {
			pr_err("%s: cci read fail (%d)\n", __func__, rc);
			goto release;
		}
		rc = cci_ctrl.status;
		break;
	case MSM_CCI_INTF_WRITE:
		/* write */
		reg_conf_tbl = kzalloc(xfer->data.count *
				sizeof(struct cam_sensor_i2c_reg_array),
				GFP_KERNEL);
		if (!reg_conf_tbl) {
			rc = -ENOMEM;
			goto release;
		}
		reg_conf_tbl[0].reg_addr = xfer->reg.addr;
		for (i = 0; i < xfer->data.count; i++) {
			reg_conf_tbl[i].reg_data = xfer->data.buf[i];
			reg_conf_tbl[i].delay = 0;
		}
		cci_ctrl.cmd = MSM_CCI_I2C_WRITE;
		cci_ctrl.cfg.cci_i2c_write_cfg.reg_setting = reg_conf_tbl;
		cci_ctrl.cfg.cci_i2c_write_cfg.addr_type = cci_intf_xfer_addr_convert(xfer->reg.width);
		cci_ctrl.cfg.cci_i2c_write_cfg.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		cci_ctrl.cfg.cci_i2c_write_cfg.size = xfer->data.count;
		rc = v4l2_subdev_call(cam_cci_get_subdev(0),
				core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		kfree(reg_conf_tbl);
		if (rc < 0) {
			pr_err("%s: cci write fail (%d)\n", __func__, rc);
			goto release;
		}
		rc = cci_ctrl.status;
		break;
	case MSM_CCI_INTF_RELEASE:
		/* release */
		cci_ctrl.cmd = MSM_CCI_RELEASE;
		rc2 = v4l2_subdev_call(cam_cci_get_subdev(0),
				core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		if (rc2 < 0) {
			pr_err("%s: cci release fail (%d)\n", __func__, rc2);
			return rc2;
		}
		pr_debug("%s: MSM_CCI_RELEASE (master:%d, slave_addr:%x) ret:%d",
		         __func__, xfer->cci_bus, xfer->slave_addr, rc2);
		break;
	default:
		pr_err("%s: Unknown command (%d)\n", __func__, cmd);
		rc = -EINVAL;
		break;
	}

release:

	return rc;
}

static long msm_cci_intf_ioctl(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	pr_debug("%s cmd=%x\n", __func__, cmd);

	switch (cmd) {
	case MSM_CCI_INTF_READ:
	case MSM_CCI_INTF_WRITE:
	case MSM_CCI_INTF_INIT:
	case MSM_CCI_INTF_RELEASE:
		return cci_intf_xfer(sd, (struct msm_cci_intf_xfer *)arg, cmd);
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_cci_intf_ioctl32(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct msm_cci_intf_xfer cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	pr_debug("%s cmd=%x\n", __func__, cmd);

	switch (cmd) {
	case MSM_CCI_INTF_READ32:
		cmd = MSM_CCI_INTF_READ;
		break;
	case MSM_CCI_INTF_WRITE32:
		cmd = MSM_CCI_INTF_WRITE;
		break;
	case MSM_CCI_INTF_INIT32:
		cmd = MSM_CCI_INTF_INIT;
		break;
	case MSM_CCI_INTF_RELEASE32:
		cmd = MSM_CCI_INTF_RELEASE;
		break;
	default:
		return -ENOIOCTLCMD;
	}

	rc = msm_cci_intf_ioctl(sd, cmd, (void *)&cmd_data);

	if (rc >= 0) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			pr_err("Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}
	return rc;
}
#endif


static struct v4l2_subdev_core_ops msm_cci_intf_subdev_core_ops = {
	.ioctl = msm_cci_intf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = msm_cci_intf_ioctl32,
#endif
};

static struct v4l2_subdev_ops msm_cci_intf_subdev_ops = {
	.core = &msm_cci_intf_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_cci_intf_internal_ops;

static int cci_intf_init_subdev(struct msm_cci_intf_ctrl_t *f_ctrl)
{
	int rc = 0;

	f_ctrl->v4l2_dev_str.internal_ops = &msm_cci_intf_internal_ops;
	f_ctrl->v4l2_dev_str.ops = &msm_cci_intf_subdev_ops;
	strlcpy(f_ctrl->device_name, CCI_INTF_NAME, sizeof(f_ctrl->device_name));
	f_ctrl->v4l2_dev_str.name = f_ctrl->device_name;
	f_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	f_ctrl->v4l2_dev_str.ent_function = MSM_CAMERA_SUBDEV_CCI_INTF;
	f_ctrl->v4l2_dev_str.token = f_ctrl;

	rc = cam_register_subdev(&(f_ctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_SENSOR, "Fail with cam_register_subdev rc: %d", rc);

	return rc;
}

static int __init cci_intf_init(void)
{
	pr_debug("%s\n", __func__);

	cci_intf_init_subdev(&fctrl);

	return 0;
}

static void __exit cci_intf_exit(void)
{
	pr_debug("%s\n", __func__);

	cam_unregister_subdev(&fctrl.v4l2_dev_str);
}

module_init(cci_intf_init);
module_exit(cci_intf_exit);
MODULE_DESCRIPTION("CCI DEBUG INTF");
MODULE_LICENSE("GPL v2");
