/*
 * Copyright (C) 2014 Motorola Mobility LLC.
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
#include "msm_sd.h"
#include "sensor/cci/msm_cci.h"
#include <media/cci_intf.h>
#include <media/v4l2-ioctl.h>

#ifdef CONFIG_COMPAT
static struct v4l2_file_operations msm_cci_intf_v4l2_subdev_fops;
#endif

struct msm_cci_intf_ctrl_t {
	struct msm_sd_subdev msm_sd;
	struct platform_device *pdev;
	uint32_t subdev_id;
};

static struct msm_cci_intf_ctrl_t fctrl;

static int32_t cci_intf_xfer(struct v4l2_subdev *sd,
		struct msm_cci_intf_xfer *xfer,
		int cmd)
{
	int32_t rc, rc2;
	uint16_t addr;
	struct msm_camera_cci_client cci_info = {
		.cci_subdev     = msm_cci_get_subdev(),
		.cci_i2c_master = xfer->cci_bus,
		.sid            = xfer->slave_addr,
	};
	struct msm_camera_cci_ctrl cci_ctrl = {
		.cci_info = &cci_info,
	};
	int i;
	struct msm_camera_i2c_reg_array *reg_conf_tbl;

	pr_debug("%s cmd:%d bus:%d devaddr:%02x regw:%d rega:%04x count:%d\n",
			__func__, cmd, xfer->cci_bus, xfer->slave_addr,
			xfer->reg.width, xfer->reg.addr, xfer->data.count);

	if (xfer->cci_bus > 1 || xfer->slave_addr > 0x7F ||
			xfer->reg.width < 1 || xfer->reg.width > 2 ||
			xfer->reg.addr > ((1<<(8*xfer->reg.width))-1) ||
			xfer->data.count < 1 ||
			xfer->data.count > MSM_CCI_INTF_MAX_XFER)
		return -EINVAL;

	/* init */
	cci_ctrl.cmd = MSM_CCI_INIT;
	rc = v4l2_subdev_call(msm_cci_get_subdev(),
			core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc < 0) {
		pr_err("%s: cci init fail (%d)\n", __func__, rc);
		return rc;
	}

	switch (cmd) {
	case MSM_CCI_INTF_READ:
		/* read */
		cci_ctrl.cmd = MSM_CCI_I2C_READ;
		cci_ctrl.cfg.cci_i2c_read_cfg.addr = xfer->reg.addr;
		cci_ctrl.cfg.cci_i2c_read_cfg.addr_type =
			(xfer->reg.width == 1 ?
			 MSM_CAMERA_I2C_BYTE_ADDR :
			 MSM_CAMERA_I2C_WORD_ADDR);
		cci_ctrl.cfg.cci_i2c_read_cfg.data = xfer->data.buf;
		cci_ctrl.cfg.cci_i2c_read_cfg.num_byte = xfer->data.count;
		rc = v4l2_subdev_call(msm_cci_get_subdev(),
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
				sizeof(struct msm_camera_i2c_reg_array),
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
		cci_ctrl.cfg.cci_i2c_write_cfg.addr_type =
			(xfer->reg.width == 1 ?
				MSM_CAMERA_I2C_BYTE_ADDR :
				MSM_CAMERA_I2C_WORD_ADDR);
		cci_ctrl.cfg.cci_i2c_write_cfg.data_type =
			MSM_CAMERA_I2C_BYTE_DATA;
		cci_ctrl.cfg.cci_i2c_write_cfg.size = xfer->data.count;
		rc = v4l2_subdev_call(msm_cci_get_subdev(),
				core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
		kfree(reg_conf_tbl);
		if (rc < 0) {
			pr_err("%s: cci write fail (%d)\n", __func__, rc);
			goto release;
		}
		rc = cci_ctrl.status;
		break;
	default:
		pr_err("%s: Unknown command (%d)\n", __func__, cmd);
		rc = -EINVAL;
		break;
	}

release:
	/* release */
	cci_ctrl.cmd = MSM_CCI_RELEASE;
	rc2 = v4l2_subdev_call(msm_cci_get_subdev(),
			core, ioctl, VIDIOC_MSM_CCI_CFG, &cci_ctrl);
	if (rc2 < 0) {
		pr_err("%s: cci release fail (%d)\n", __func__, rc2);
		return rc2;
	}

	return rc;
}

static long msm_cci_intf_ioctl(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	pr_debug("%s cmd=%x\n", __func__, cmd);

	switch (cmd) {
	case MSM_CCI_INTF_READ:
	case MSM_CCI_INTF_WRITE:
		return cci_intf_xfer(sd, (struct msm_cci_intf_xfer *)arg, cmd);
	default:
		return -ENOIOCTLCMD;
	}
}

static struct v4l2_subdev_core_ops msm_cci_intf_subdev_core_ops = {
	.ioctl = msm_cci_intf_ioctl,
};

static struct v4l2_subdev_ops msm_cci_intf_subdev_ops = {
	.core = &msm_cci_intf_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_cci_intf_internal_ops;

#ifdef CONFIG_COMPAT
static long msm_cci_intf_subdev_do_ioctl(
		struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	switch (cmd) {
	case MSM_CCI_INTF_READ32:
		cmd = MSM_CCI_INTF_READ;
		return cci_intf_xfer(sd, (struct msm_cci_intf_xfer *)arg, cmd);
	case MSM_CCI_INTF_WRITE32:
		cmd = MSM_CCI_INTF_WRITE;
		return cci_intf_xfer(sd, (struct msm_cci_intf_xfer *)arg, cmd);
	default:
		return -ENOIOCTLCMD;
	}
}

static long msm_cci_intf_subdev_fops_ioctl(
		struct file *file, unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_cci_intf_subdev_do_ioctl);
}
#endif

static int __init cci_intf_init(void)
{
	int rc;

	pr_debug("%s\n", __func__);

	v4l2_subdev_init(&fctrl.msm_sd.sd, &msm_cci_intf_subdev_ops);
	v4l2_set_subdevdata(&fctrl.msm_sd.sd, &fctrl);

	fctrl.msm_sd.sd.internal_ops = &msm_cci_intf_internal_ops;
	fctrl.msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(fctrl.msm_sd.sd.name, ARRAY_SIZE(fctrl.msm_sd.sd.name),
			"msm_cci_intf");
	rc = media_entity_init(&fctrl.msm_sd.sd.entity, 0, NULL, 0);
	if (rc < 0) {
		pr_err("%s: failed media_entity_init (%d)\n", __func__, rc);
		return rc;
	}
	fctrl.msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	fctrl.msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_CCI_INTF;
	rc = msm_sd_register(&fctrl.msm_sd);
	if (rc < 0) {
		pr_err("%s: failed msm_sd_register (%d)\n", __func__, rc);
		media_entity_cleanup(&fctrl.msm_sd.sd.entity);
		return rc;
	}

#ifdef CONFIG_COMPAT
	msm_cci_intf_v4l2_subdev_fops = v4l2_subdev_fops;
	msm_cci_intf_v4l2_subdev_fops.compat_ioctl32 =
		msm_cci_intf_subdev_fops_ioctl;
	fctrl.msm_sd.sd.devnode->fops =
		&msm_cci_intf_v4l2_subdev_fops;
#endif
	return 0;
}

static void __exit cci_intf_exit(void)
{
	pr_debug("%s\n", __func__);

	msm_sd_unregister(&fctrl.msm_sd);
	media_entity_cleanup(&fctrl.msm_sd.sd.entity);
}

module_init(cci_intf_init);
module_exit(cci_intf_exit);
MODULE_DESCRIPTION("CCI DEBUG INTF");
MODULE_LICENSE("GPL v2");
