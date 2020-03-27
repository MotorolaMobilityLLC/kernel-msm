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
#include <linux/miscdevice.h>
#include <linux/version.h>
#include <media/cci_intf.h>
#include <media/v4l2-ioctl.h>
#include <cam_cci_dev.h>

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

static int32_t cci_intf_xfer(
		struct msm_cci_intf_xfer *xfer,
		unsigned int cmd)
{
	int32_t rc=0, rc2=0;
	uint16_t addr;
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
		addr = xfer->reg.addr;
		for (i = 0; i < xfer->data.count; i++) {
			reg_conf_tbl[i].reg_addr = addr++;
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

static long cci_intf_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct msm_cci_intf_xfer xfer;
	int rc;

	pr_debug("%s cmd=%x arg=%lx\n", __func__, cmd, arg);

	switch (cmd) {
	case MSM_CCI_INTF_READ:
	case MSM_CCI_INTF_WRITE:
	case MSM_CCI_INTF_INIT:
	case MSM_CCI_INTF_RELEASE:
		if (copy_from_user(&xfer, (void __user *)arg, sizeof(xfer)))
			return -EFAULT;
		rc = cci_intf_xfer(&xfer, cmd);
		if (copy_to_user((void __user *)arg, &xfer, sizeof(xfer)))
			return -EFAULT;
		return rc;
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long cci_intf_ioctl_compat(struct file *file, unsigned int cmd,
		unsigned long arg)
{
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
	return cci_intf_ioctl(file, cmd, arg);
}
#endif

static const struct file_operations cci_intf_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = cci_intf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cci_intf_ioctl_compat,
#endif
};

static struct miscdevice cci_intf_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cci_intf",
	.fops = &cci_intf_fops,
};

static int __init cci_intf_init(void)
{
	int rc;

	pr_debug("%s\n", __func__);

	rc = misc_register(&cci_intf_misc);
	if (unlikely(rc)) {
		pr_err("failed to register misc device %s\n", cci_intf_misc.name);
		return rc;
	}

	return 0;
}

static void __exit cci_intf_exit(void)
{
	pr_debug("%s\n", __func__);

	misc_deregister(&cci_intf_misc);
}

module_init(cci_intf_init);
module_exit(cci_intf_exit);
MODULE_DESCRIPTION("CCI DEBUG INTF");
MODULE_LICENSE("GPL v2");
