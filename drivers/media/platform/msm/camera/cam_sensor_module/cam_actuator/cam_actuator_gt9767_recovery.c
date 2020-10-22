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
#include "cam_actuator_gt9767_recovery.h"
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>

#define GT9767_RECOVERY_NAME "gt9767_recovery"

#define CCI_INTF_DEBUG
#ifdef CCI_INTF_DEBUG
#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug pr_err
#endif


#define CCI_SCL0 18
#define CCI_SDA0 17

struct msm_cam_gt9767_recovery_ctrl_t {
	struct cam_subdev v4l2_dev_str;
	struct platform_device *pdev;
	char device_name[20];
};

typedef enum {
	GT9767_RECOVERY_IDLE,
	GT9767_RECOVERY_INITED,
	GT9767_RECOVERY_RELEASED
} gt9767_recovery_state_e;

static struct msm_cam_gt9767_recovery_ctrl_t fctrl;
static gt9767_recovery_state_e gt9767_recovery_state = GT9767_RECOVERY_IDLE;
static struct regulator * afVdd = NULL;
static struct regulator * ioVdd = NULL;

static int32_t cam_sensor_gt9767_enter_test_mode(void)
{
	//Start-010-Stop -> Start-001-Stop
	const int cmd1ValueTable[] = {0,1,0};
	const int cmd2ValueTable[] = {0,0,1};
	int i;

	pr_debug("Tring enter test mode");
	gpio_direction_output(CCI_SCL0, 1);
	gpio_direction_output(CCI_SDA0, 1);
	usleep_range(10,15);

	//Start
	gpio_direction_output(CCI_SCL0, 1);
	usleep_range(1,2);
	gpio_direction_output(CCI_SDA0, 0);
	usleep_range(2,5);

	for (i=0; i< 3; i++) {
		gpio_direction_output(CCI_SCL0, 0);
		usleep_range(2,3);
		gpio_direction_output(CCI_SDA0, cmd1ValueTable[i]);
		usleep_range(2,3);
		gpio_direction_output(CCI_SCL0, 1);
		usleep_range(4,6);
	}
	gpio_direction_output(CCI_SCL0, 0);
	usleep_range(4,6);
	gpio_direction_output(CCI_SCL0, 1);
	usleep_range(4,6);
	//Stop
	gpio_direction_output(CCI_SDA0, 1);
	usleep_range(2,5);

	//Start
	gpio_direction_output(CCI_SCL0, 1);
	usleep_range(4,6);
	gpio_direction_output(CCI_SDA0, 0);
	usleep_range(4,6);

	for (i=0; i<3; i++) {
		gpio_direction_output(CCI_SCL0, 0);
		usleep_range(2,3);
		gpio_direction_output(CCI_SDA0, cmd2ValueTable[i]);
		usleep_range(2,3);
		gpio_direction_output(CCI_SCL0, 1);
		usleep_range(4,6);
	}
	gpio_direction_output(CCI_SCL0, 0);
	usleep_range(4,6);
	gpio_direction_output(CCI_SDA0, 0);
	usleep_range(4,6);
	gpio_direction_output(CCI_SCL0, 1);
	usleep_range(4,6);
	//Stop
	gpio_direction_output(CCI_SDA0, 1);
	usleep_range(2,5);

	pr_debug("Enter test mode done!");

	return 0;
}

static int32_t cam_gt9767_recovery_handle_init(uint32_t arg)
{
	int rc = 0;

	pr_debug("SCL:%d, SDA:%d", CCI_SDA0, CCI_SCL0);
	rc = gpio_request(CCI_SDA0, "SDA0");
	if (rc < 0) {
		pr_err("FATAL: request SDA0 failure!!! rc=%d", rc);
		//return rc;
	}

	rc = gpio_request(CCI_SCL0, "SCL0");
	if (rc < 0) {
		pr_err("FATAL: request SCL0 failure!!! rc=%d", rc);
		gpio_free(CCI_SDA0);
		//return rc;
	}

	if (afVdd == NULL){
		afVdd = regulator_get(&fctrl.v4l2_dev_str.pdev->dev, "pm6150_l5");
		pr_debug("AFVDD:%p", afVdd);
	}

	if (ioVdd == NULL) {
		ioVdd = regulator_get(&fctrl.v4l2_dev_str.pdev->dev, "pm6150_l13");
		pr_debug("IOVDD:%p", ioVdd);
	}

	regulator_enable(afVdd);
	regulator_enable(ioVdd);
	usleep_range(200, 300);

	cam_sensor_gt9767_enter_test_mode();

	gpio_free(CCI_SDA0);
	gpio_free(CCI_SCL0);

	gt9767_recovery_state = GT9767_RECOVERY_INITED;
	return 0;
}

static int32_t cam_sensor_gt9767_exit_test_mode(void)
{
	//Start-0xB0-Stop
	const uint8_t gt9767ExitCmd = 0xB0;
	int i;
	int exitCmdBits = sizeof(gt9767ExitCmd)*8;

	pr_debug("Tring exit test mode");
	gpio_direction_output(CCI_SCL0, 1);
	gpio_direction_output(CCI_SDA0, 1);
	usleep_range(100,150);

	//Start
	gpio_direction_output(CCI_SCL0, 1);
	usleep_range(1,2);
	gpio_direction_output(CCI_SDA0, 0);
	usleep_range(2,5);

	for (i=0; i< exitCmdBits; i++) {
		gpio_direction_output(CCI_SCL0, 0);
		usleep_range(2,3);
		gpio_direction_output(CCI_SDA0, (gt9767ExitCmd >> (exitCmdBits-i-1))&0x01);
		usleep_range(2,3);
		gpio_direction_output(CCI_SCL0, 1);
		usleep_range(4,6);
	}
	gpio_direction_output(CCI_SCL0, 0);
	usleep_range(4,6);
	gpio_direction_output(CCI_SCL0, 1);
	usleep_range(4,6);
	//Stop
	gpio_direction_output(CCI_SDA0, 1);
	usleep_range(2,5);

	pr_debug("Enter test mode done!");

	return 0;
}

static int32_t cam_gt9767_recovery_handle_exit(uint32_t arg)
{
	int rc = 0;

	pr_debug("SCL:%d, SDA:%d", CCI_SDA0, CCI_SCL0);
	rc = gpio_request(CCI_SDA0, "SDA0");
	if (rc < 0) {
		pr_err("FATAL: request SDA0 failure!!! rc=%d", rc);
		//return rc;
	}

	rc = gpio_request(CCI_SCL0, "SCL0");
	if (rc < 0) {
		pr_err("FATAL: request SCL0 failure!!! rc=%d", rc);
		gpio_free(CCI_SDA0);
		//return rc;
	}

	cam_sensor_gt9767_exit_test_mode();

	gpio_free(CCI_SDA0);
	gpio_free(CCI_SCL0);

	return 0;
}

static int32_t cam_gt9767_recovery_handle_write(uint32_t arg)
{
	int rc = 0;

	if (gt9767_recovery_state != GT9767_RECOVERY_INITED) {
		pr_err("%s: CAM_GT9767_RECOVERY is not initiated!!", __func__);
		return -EINVAL;
	}

	return rc;
}

static void cam_gt9767_recovery_handle_release(void)
{
	if (gt9767_recovery_state == GT9767_RECOVERY_RELEASED) {
		pr_err("%s: Already released.", __func__);
		return;
	}

	if (gt9767_recovery_state == GT9767_RECOVERY_INITED) {
		pr_debug("Disable AF power!");
		regulator_disable(afVdd);
		regulator_disable(ioVdd);
	}
	pr_debug("%s: Vsync released.", __func__);
	gt9767_recovery_state = GT9767_RECOVERY_RELEASED;
	return;
}

static int32_t cam_gt9767_recovery_handle_cmd(
		struct v4l2_subdev *sd,
		uint32_t arg,
		unsigned int cmd)
{
	int32_t rc=0;

	switch (cmd) {
	case CAM_GT9767_RECOVERY_INIT:
		/* init */
		rc = cam_gt9767_recovery_handle_init(arg);
		pr_debug("%s: CAM_GT9767_RECOVERY_INIT ret:%d", __func__, rc);
		break;
	case CAM_GT9767_RECOVERY_READ:
		/* read */
		pr_debug("%s: CAM_GT9767_RECOVERY_READ ret:%d", __func__, rc);
		break;
	case CAM_GT9767_RECOVERY_WRITE:
		/* write */
		rc = cam_gt9767_recovery_handle_write(arg);
		pr_debug("%s: CAM_GT9767_RECOVERY_WRITE ret:%d", __func__, rc);
		break;
	case CAM_GT9767_RECOVERY_EXIT:
		/* write */
		rc = cam_gt9767_recovery_handle_exit(arg);
		pr_debug("%s: CAM_GT9767_RECOVERY_EXIT ret:%d", __func__, rc);
		break;
	case CAM_GT9767_RECOVERY_RELEASE:
		/* release */
		cam_gt9767_recovery_handle_release();
		pr_debug("%s: CAM_GT9767_RECOVERY_RELEASE ret:%d", __func__, rc);
		break;
	default:
		pr_err("%s: Unknown command (%d)\n", __func__, cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static long msm_cam_gt9767_recovery_ioctl(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	pr_debug("%s cmd=%x, arg=%x\n", __func__, cmd, *(int *)arg);

	switch (cmd) {
	case CAM_GT9767_RECOVERY_READ:
	case CAM_GT9767_RECOVERY_WRITE:
	case CAM_GT9767_RECOVERY_INIT:
	case CAM_GT9767_RECOVERY_EXIT:
	case CAM_GT9767_RECOVERY_RELEASE:
		return cam_gt9767_recovery_handle_cmd(sd, *(int *)arg, cmd);
	default:
		pr_err("%s Unsupported cmd=%x, arg=%x\n", __func__, cmd, arg);
		pr_debug("%s supported cmd: %x, %x, %x, %x, %x\n", __func__,
		        CAM_GT9767_RECOVERY_READ,
		        CAM_GT9767_RECOVERY_WRITE,
		        CAM_GT9767_RECOVERY_INIT,
		        CAM_GT9767_RECOVERY_EXIT,
		        CAM_GT9767_RECOVERY_RELEASE);
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_cam_gt9767_recovery_ioctl32(struct v4l2_subdev *sd,
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
		case CAM_GT9767_RECOVERY_READ32:
			cmd = CAM_GT9767_RECOVERY_READ;
			break;
		case CAM_GT9767_RECOVERY_WRITE32:
			cmd = CAM_GT9767_RECOVERY_WRITE;
			break;
		case CAM_GT9767_RECOVERY_INIT32:
			cmd = CAM_GT9767_RECOVERY_INIT;
			break;
		case CAM_GT9767_RECOVERY_EXIT32:
			cmd = CAM_GT9767_RECOVERY_EXIT;
			break;
		case CAM_GT9767_RECOVERY_RELEASE32:
			cmd = CAM_GT9767_RECOVERY_RELEASE;
			break;
		default:
			pr_err("%s Unsupported cmd=%x, arg=%x\n", __func__, cmd, arg);
			pr_debug("%s supported cmd: %x, %x, %x, %x, %x\n", __func__,
			        CAM_GT9767_RECOVERY_READ32,
			        CAM_GT9767_RECOVERY_WRITE32,
			        CAM_GT9767_RECOVERY_INIT32,
			        CAM_GT9767_RECOVERY_EXIT32,
			        CAM_GT9767_RECOVERY_RELEASE32);
			return -ENOIOCTLCMD;
	}

	rc = msm_cam_gt9767_recovery_ioctl(sd, cmd, (void *)&cmd_data);

	return rc;
}
#endif


static struct v4l2_subdev_core_ops msm_cam_gt9767_recovery_subdev_core_ops = {
	.ioctl = msm_cam_gt9767_recovery_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = msm_cam_gt9767_recovery_ioctl32,
#endif
};

static struct v4l2_subdev_ops msm_cam_gt9767_recovery_subdev_ops = {
	.core = &msm_cam_gt9767_recovery_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_cam_gt9767_recovery_internal_ops;

static struct platform_device mot_gt9767_revovery_device = {
	.name = "mot_gt9767_recovery",
	.id = -1,
};

static int cam_gt9767_recovery_init_subdev(struct msm_cam_gt9767_recovery_ctrl_t *f_ctrl)
{
	int rc = 0;

	f_ctrl->v4l2_dev_str.internal_ops = &msm_cam_gt9767_recovery_internal_ops;
	f_ctrl->v4l2_dev_str.ops = &msm_cam_gt9767_recovery_subdev_ops;
	strlcpy(f_ctrl->device_name, GT9767_RECOVERY_NAME, sizeof(f_ctrl->device_name));
	f_ctrl->v4l2_dev_str.name = f_ctrl->device_name;
	f_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	f_ctrl->v4l2_dev_str.ent_function = MSM_CAMERA_SUBDEV_GT9767_RECOVERY;
	f_ctrl->v4l2_dev_str.token = f_ctrl;

	platform_device_register(&mot_gt9767_revovery_device);
	f_ctrl->v4l2_dev_str.pdev = &mot_gt9767_revovery_device;
	
	rc = cam_register_subdev(&(f_ctrl->v4l2_dev_str));
	if (rc)
		pr_err("%s: Fail with cam_register_subdev rc: %d", __func__, rc);

	return rc;
}

static int __init cam_gt9767_recovery_init(void)
{
	pr_debug("%s\n", __func__);

	cam_gt9767_recovery_init_subdev(&fctrl);

	return 0;
}

static void __exit cam_gt9767_recovery_exit(void)
{
	pr_debug("%s\n", __func__);

	cam_unregister_subdev(&fctrl.v4l2_dev_str);
}

module_init(cam_gt9767_recovery_init);
module_exit(cam_gt9767_recovery_exit);
MODULE_DESCRIPTION("GT9767 RECOVERY");
MODULE_LICENSE("GPL v2");
