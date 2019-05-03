/*
 * Copyright (C) 2019 Motorola Mobility LLC.
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
#include <linux/gpio.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/msmb_camera.h>
#include <cam_subdev.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_vsync.h"

#define CAM_VSYNC_NAME "cam_sensor_vsync"

#define CCI_INTF_DEBUG
#ifdef CCI_INTF_DEBUG
#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug pr_err
#endif

#define CAM_SENSOR_VSYNC_GPIO1 95
#define CAM_SENSOR_VSYNC_GPIO2 96

struct msm_cam_sensor_vsync_ctrl_t {
	struct cam_subdev v4l2_dev_str;
	struct platform_device *pdev;
	char device_name[20];
};

typedef enum {
	CAM_VSYNC_IDLE,
	CAM_VSYNC_INITED,
	CAM_VSYNC_RELEASED
} cam_vsync_state_e;

static struct msm_cam_sensor_vsync_ctrl_t fctrl;
static cam_vsync_state_e cam_vsync_state = CAM_VSYNC_IDLE;

static int32_t cam_sensor_vsync_handle_init(uint32_t arg)
{
	int rc = 0;
	rc = gpio_request(CAM_SENSOR_VSYNC_GPIO1, "cam_vsync_gpio1");
	if (rc < 0) {
		pr_err("FATAL: request cam_vsync_gpio1 failure!!! rc=%d", rc);
		return rc;
	}

	rc = gpio_request(CAM_SENSOR_VSYNC_GPIO2, "cam_vsync_gpio2");
	if (rc < 0) {
		pr_err("FATAL: request cam_vsync_gpio2 failure!!! rc=%d", rc);
		gpio_free(CAM_SENSOR_VSYNC_GPIO1);
		return rc;
	}

	if (GET_VSYNC_VALID(arg)) {
		if (GET_VSYNC1_SETING(arg)) {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO1, 1);
		} else {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO1, 0);
		}
		pr_debug("%s: CAM_SENSOR_VSYNC_GPIO1 output rc=%d", __func__, rc);
		if (GET_VSYNC2_SETING(arg)) {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO2, 1);
		} else {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO2, 0);
		}
		pr_debug("%s: CAM_SENSOR_VSYNC_GPIO2 output rc=%d", __func__, rc);
	} else {
			pr_debug("%s: init without pre-setting state, arg=%d", __func__, arg);
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO1, 0);
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO2, 0);
	}
	cam_vsync_state = CAM_VSYNC_INITED;
	return rc;
}

static int32_t cam_sensor_vsync_handle_write(uint32_t arg)
{
	int rc = 0;

	if (cam_vsync_state != CAM_VSYNC_INITED) {
		pr_err("%s: CAM_SENSOR_VSYNC is not initiated!!", __func__);
		return -EINVAL;
	}

	if (GET_VSYNC_VALID(arg)) {
		if (GET_VSYNC1_SETING(arg)) {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO1, 1);
		} else {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO1, 0);
		}
		if (GET_VSYNC2_SETING(arg)) {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO2, 1);
		} else {
			rc = gpio_direction_output(CAM_SENSOR_VSYNC_GPIO2, 0);
		}
	} else {
		pr_err("%s: FATAL: VSYNC CONFIG VAL INVALID: %x",__func__, arg);
		rc = -EINVAL;
	}
	return rc;
}

static void cam_sensor_vsync_handle_release(void)
{
	if (cam_vsync_state == CAM_VSYNC_RELEASED) {
		pr_err("%s: Already released.", __func__);
		return;
	}

	if (cam_vsync_state == CAM_VSYNC_INITED) {
		gpio_set_value(CAM_SENSOR_VSYNC_GPIO1, 0);
		gpio_set_value(CAM_SENSOR_VSYNC_GPIO2, 0);
		gpio_free(CAM_SENSOR_VSYNC_GPIO1);
		gpio_free(CAM_SENSOR_VSYNC_GPIO2);
	}
	pr_debug("%s: Vsync released.", __func__);
	cam_vsync_state = CAM_VSYNC_RELEASED;
	return;
}

static int32_t cam_sensor_vsync_handle_cmd(
		struct v4l2_subdev *sd,
		uint32_t arg,
		unsigned int cmd)
{
	int32_t rc=0;

	switch (cmd) {
	case CAM_SENSOR_VSYNC_INIT:
		/* init */
		rc = cam_sensor_vsync_handle_init(arg);
		pr_debug("%s: CAM_SENSOR_VSYNC_INIT ret:%d", __func__, rc);
		break;
	case CAM_SENSOR_VSYNC_READ:
		/* read */
		pr_debug("%s: CAM_SENSOR_VSYNC_READ ret:%d", __func__, rc);
		break;
	case CAM_SENSOR_VSYNC_WRITE:
		/* write */
		rc = cam_sensor_vsync_handle_write(arg);
		pr_debug("%s: CAM_SENSOR_VSYNC_WRITE ret:%d", __func__, rc);
		break;
	case CAM_SENSOR_VSYNC_RELEASE:
		/* release */
		cam_sensor_vsync_handle_release();
		pr_debug("%s: CAM_SENSOR_VSYNC_RELEASE ret:%d", __func__, rc);
		break;
	default:
		pr_err("%s: Unknown command (%d)\n", __func__, cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static long msm_cam_sensor_vsync_ioctl(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	pr_debug("%s cmd=%x, arg=%x\n", __func__, cmd, *(int *)arg);

	switch (cmd) {
	case CAM_SENSOR_VSYNC_READ:
	case CAM_SENSOR_VSYNC_WRITE:
	case CAM_SENSOR_VSYNC_INIT:
	case CAM_SENSOR_VSYNC_RELEASE:
		return cam_sensor_vsync_handle_cmd(sd, *(int *)arg, cmd);
	default:
		pr_err("%s Unsupported cmd=%x, arg=%x\n", __func__, cmd, arg);
		pr_debug("%s supported cmd: %x, %x, %x, %x\n", __func__,
		        CAM_SENSOR_VSYNC_READ,
		        CAM_SENSOR_VSYNC_WRITE,
		        CAM_SENSOR_VSYNC_INIT,
		        CAM_SENSOR_VSYNC_RELEASE);
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_cam_sensor_vsync_ioctl32(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	int cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		pr_err("Failed to copy from user_ptr=%pK size=%zu", (void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	pr_debug("%s cmd=%x, arg=%x\n", __func__, cmd, arg);

	switch (cmd) {
		case CAM_SENSOR_VSYNC_READ32:
			cmd = CAM_SENSOR_VSYNC_READ;
			break;
		case CAM_SENSOR_VSYNC_WRITE32:
			cmd = CAM_SENSOR_VSYNC_WRITE;
			break;
		case CAM_SENSOR_VSYNC_INIT32:
			cmd = CAM_SENSOR_VSYNC_INIT;
			break;
		case CAM_SENSOR_VSYNC_RELEASE32:
			cmd = CAM_SENSOR_VSYNC_RELEASE;
			break;
		default:
			pr_err("%s Unsupported cmd=%x, arg=%x\n", __func__, cmd, arg);
			pr_debug("%s supported cmd: %x, %x, %x, %x\n", __func__,
			        CAM_SENSOR_VSYNC_READ32,
			        CAM_SENSOR_VSYNC_WRITE32,
			        CAM_SENSOR_VSYNC_INIT32,
			        CAM_SENSOR_VSYNC_RELEASE32);
			return -ENOIOCTLCMD;
	}

	rc = msm_cam_sensor_vsync_ioctl(sd, cmd, (void *)&cmd_data);

	return rc;
}
#endif


static struct v4l2_subdev_core_ops msm_cam_sensor_vsync_subdev_core_ops = {
	.ioctl = msm_cam_sensor_vsync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = msm_cam_sensor_vsync_ioctl32,
#endif
};

static struct v4l2_subdev_ops msm_cam_sensor_vsync_subdev_ops = {
	.core = &msm_cam_sensor_vsync_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_cam_sensor_vsync_internal_ops;

static int cam_sensor_vsync_init_subdev(struct msm_cam_sensor_vsync_ctrl_t *f_ctrl)
{
	int rc = 0;

	f_ctrl->v4l2_dev_str.internal_ops = &msm_cam_sensor_vsync_internal_ops;
	f_ctrl->v4l2_dev_str.ops = &msm_cam_sensor_vsync_subdev_ops;
	strlcpy(f_ctrl->device_name, CAM_VSYNC_NAME, sizeof(f_ctrl->device_name));
	f_ctrl->v4l2_dev_str.name = f_ctrl->device_name;
	f_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	f_ctrl->v4l2_dev_str.ent_function = MSM_CAMERA_SUBDEV_CAM_VSYNC;
	f_ctrl->v4l2_dev_str.token = f_ctrl;

	rc = cam_register_subdev(&(f_ctrl->v4l2_dev_str));
	if (rc)
		pr_err("%s: Fail with cam_register_subdev rc: %d", __func__, rc);

	return rc;
}

static int __init cam_sensor_vsync_init(void)
{
	pr_debug("%s\n", __func__);

	cam_sensor_vsync_init_subdev(&fctrl);

	return 0;
}

static void __exit cam_sensor_vsync_exit(void)
{
	pr_debug("%s\n", __func__);

	cam_unregister_subdev(&fctrl.v4l2_dev_str);
}

module_init(cam_sensor_vsync_init);
module_exit(cam_sensor_vsync_exit);
MODULE_DESCRIPTION("CAM SENSOR VSYNC");
MODULE_LICENSE("GPL v2");
