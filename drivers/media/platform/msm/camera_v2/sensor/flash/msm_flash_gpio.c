/* Copyright (c) 2009-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/of_gpio.h>
#include "msm_flash.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_cci.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
enum msm_flash_driver_type flash_type_gpio = FLASH_DRIVER_DEFAULT;
DEFINE_MSM_MUTEX(msm_flash_mutex);

static struct v4l2_file_operations msm_flash_v4l2_subdev_fops;
static DEFINE_MUTEX(flash_lock);

static const struct of_device_id msm_flash_dt_match[] = {
	{.compatible = "qcom,camera-led-flash", .data = NULL},
	{}
};
static int32_t msm_flash_init_gpio_pin_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0;
	int32_t val = 0;

	gconf->gpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!gconf->gpio_num_info) {
		rc = -ENOMEM;
		return rc;
	}
	rc = of_property_read_u32(of_node, "qcom,gpio-flash-now", &val);
	if (rc < 0) {
		pr_err("%s:%d read qcom,gpio-flash-now failed rc %d\n",
			__func__, __LINE__, rc);
	} else if (val > gpio_array_size) {
		pr_err("%s:%d qcom,gpio-flash-now invalid %d\n",
			__func__, __LINE__, val);
		goto ERROR;
	} else if (rc == 0) {
	/*index 1 is for qcom,gpio-flash-now */
	gconf->gpio_num_info->gpio_num[1] =
		gpio_array[val];
	gconf->gpio_num_info->valid[1] = 1;
	CDBG("%s qcom,gpio-flash-now %d\n", __func__,
		gconf->gpio_num_info->gpio_num[1]);
	}
	rc = of_property_read_u32(of_node, "qcom,gpio-flash-en", &val);
	if (rc < 0) {
		pr_err("%s:%d read qcom,gpio-flash-en failed rc %d\n",
			__func__, __LINE__, rc);
		goto ERROR;
	} else if (val > gpio_array_size) {
		pr_err("%s:%d qcom,gpio-flash-en invalid %d\n",
			__func__, __LINE__, val);
		goto ERROR;
	}  else if (rc == 0) {
	/*index 0 is for qcom,gpio-flash-en */
	gconf->gpio_num_info->gpio_num[0] =
		gpio_array[val];
	gconf->gpio_num_info->valid[0] = 1;
	CDBG("%s qcom,gpio-flash-en %d\n", __func__,
		gconf->gpio_num_info->gpio_num[0]);
	}
	return rc;

ERROR:
	kfree(gconf->gpio_num_info);
	gconf->gpio_num_info = NULL;
	return rc;
}

static struct msm_flash_table msm_gpio_flash_table;

static struct msm_flash_table *flash_table[] = {
	&msm_gpio_flash_table,

};

static int32_t msm_flash_get_subdev_id(
	struct msm_flash_ctrl_t *flash_ctrl, void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;

	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (flash_ctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = flash_ctrl->pdev->id;
	else
		*subdev_id = flash_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}


#ifdef CONFIG_COMPAT
static void msm_flash_copy_power_settings_compat(
	struct msm_sensor_power_setting *ps,
	struct msm_sensor_power_setting32 *ps32, uint32_t size)
{
	uint16_t i = 0;

	for (i = 0; i < size; i++) {
		ps[i].config_val = ps32[i].config_val;
		ps[i].delay = ps32[i].delay;
		ps[i].seq_type = ps32[i].seq_type;
		ps[i].seq_val = ps32[i].seq_val;
	}
}
#endif

static int32_t msm_flash_gpio_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	int32_t rc = 0;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("%s:%d called\n", __func__, __LINE__);
	power_info = &flash_ctrl->power_info;
	CDBG("Enter request_gpio\n");
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}
	rc = flash_ctrl->func_tbl->camera_flash_off(flash_ctrl, flash_data);
	CDBG("Exit");
	return rc;
}

static int32_t msm_flash_off(struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("Enter\n");
	CDBG("%s:%d called\n", __func__, __LINE__);

	power_info = &flash_ctrl->power_info;

	if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL)
		pr_err("%s:%d mux install\n", __func__, __LINE__);

	CDBG("%s:%d called\n", __func__, __LINE__);

	if (power_info->gpio_conf->gpio_num_info->valid[0] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[0],
		GPIO_OUT_LOW);
	}
	if (power_info->gpio_conf->gpio_num_info->valid[1] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[1],
		GPIO_OUT_LOW);
	}

	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_init(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	uint32_t i = 0;
	int32_t rc = -EFAULT;
	enum msm_flash_driver_type flash_driver_type = FLASH_DRIVER_DEFAULT;

	CDBG("Enter");
	flash_type_gpio = flash_data->cfg.flash_init_info->flash_driver_type;
	if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT) {
		pr_err("%s:%d Invalid flash state = %d",
			__func__, __LINE__, flash_ctrl->flash_state);
		return 0;
	}

	if (flash_data->cfg.flash_init_info->flash_driver_type ==
		FLASH_DRIVER_GPIO)
		flash_driver_type = FLASH_DRIVER_GPIO;

	if (flash_driver_type == FLASH_DRIVER_DEFAULT) {
		pr_err("%s:%d invalid flash_driver_type", __func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(flash_table); i++) {
		if (flash_driver_type == flash_table[i]->flash_driver_type) {
			flash_ctrl->func_tbl = &flash_table[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		pr_err("%s:%d failed invalid flash_driver_type %d\n",
			__func__, __LINE__,
			flash_data->cfg.flash_init_info->flash_driver_type);
	}

	if (flash_ctrl->func_tbl->camera_flash_init) {
		rc = flash_ctrl->func_tbl->camera_flash_init(
				flash_ctrl, flash_data);
		if (rc < 0) {
			pr_err("%s:%d camera_flash_init failed rc = %d",
				__func__, __LINE__, rc);
			return rc;
		}
	}

	flash_ctrl->flash_state = MSM_CAMERA_FLASH_INIT;

	CDBG("Exit");
	return 0;
}

static int32_t msm_flash_low(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{
	struct msm_camera_power_ctrl_t *power_info = NULL;
	static int32_t i;

	CDBG("%s:%d called\n", __func__, __LINE__);

	power_info = &flash_ctrl->power_info;

	if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL)
		pr_err("%s:%d mux install\n", __func__, __LINE__);

	mutex_lock(&flash_lock);
	if (power_info->gpio_conf->gpio_num_info->valid[0] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[0],
		GPIO_OUT_HIGH);
		for (i = 0 ; i <= 14 ; i++) {
			gpio_set_value(
			power_info->gpio_conf->gpio_num_info->gpio_num[0],
			GPIO_OUT_LOW);
			gpio_set_value(
			power_info->gpio_conf->gpio_num_info->gpio_num[0],
			GPIO_OUT_HIGH);
		}
	}
	mutex_unlock(&flash_lock);
	if (power_info->gpio_conf->gpio_num_info->valid[1] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[1],
		GPIO_OUT_LOW);
	}

	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_high(
	struct msm_flash_ctrl_t *flash_ctrl,
	struct msm_flash_cfg_data_t *flash_data)
{

	struct msm_camera_power_ctrl_t *power_info = NULL;
	static int32_t i;

	CDBG("%s:%d called\n", __func__, __LINE__);

	power_info = &flash_ctrl->power_info;

	if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL)
		pr_err("%s:%d mux install\n", __func__, __LINE__);

	mutex_lock(&flash_lock);
	if (power_info->gpio_conf->gpio_num_info->valid[0] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[0],
		GPIO_OUT_HIGH);
		for (i = 0 ; i <= 10 ; i++) {
			gpio_set_value(
			power_info->gpio_conf->gpio_num_info->gpio_num[0],
			GPIO_OUT_LOW);
			gpio_set_value(
			power_info->gpio_conf->gpio_num_info->gpio_num[0],
			GPIO_OUT_HIGH);
		}
	}
	mutex_unlock(&flash_lock);
	if (power_info->gpio_conf->gpio_num_info->valid[1] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[1],
		GPIO_OUT_HIGH);
	}

	CDBG("Exit\n");
	return 0;
}

static int32_t msm_flash_release(
	struct msm_flash_ctrl_t *flash_ctrl)
{
	struct msm_camera_power_ctrl_t *power_info = NULL;

	CDBG("Enter\n");
	CDBG("%s:%d called\n", __func__, __LINE__);

	power_info = &flash_ctrl->power_info;

	if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_RELEASE) {
		pr_err("%s:%d Invalid flash state = %d",
			__func__, __LINE__, flash_ctrl->flash_state);
		return 0;
	}

	if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL)
		pr_err("%s:%d mux install\n", __func__, __LINE__);

	if (power_info->gpio_conf->gpio_num_info->valid[0] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[0],
		GPIO_OUT_LOW);
		CDBG("%s:%d set flash en LOW\n", __func__, __LINE__);
	}
	if (power_info->gpio_conf->gpio_num_info->valid[1] == 1) {
		gpio_set_value(
		power_info->gpio_conf->gpio_num_info->gpio_num[1],
		GPIO_OUT_LOW);
		CDBG("%s:%d set flash en LOW\n", __func__, __LINE__);
	}

	CDBG("Exit\n");
	flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;
	return 0;
}

static int32_t msm_flash_config(struct msm_flash_ctrl_t *flash_ctrl,
	void __user *argp)
{
	int32_t rc = 0;
	struct msm_flash_cfg_data_t *flash_data =
		(struct msm_flash_cfg_data_t *) argp;

	mutex_lock(flash_ctrl->flash_mutex);

	CDBG("Enter %s type %d\n", __func__, flash_data->cfg_type);

	switch (flash_data->cfg_type) {
	case CFG_FLASH_INIT:
		rc = msm_flash_init(flash_ctrl, flash_data);
		break;
	case CFG_FLASH_RELEASE:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_release(
				flash_ctrl);
		break;
	case CFG_FLASH_OFF:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_off(
				flash_ctrl, flash_data);
		break;
	case CFG_FLASH_LOW:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_low(
				flash_ctrl, flash_data);
		break;
	case CFG_FLASH_HIGH:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_high(
				flash_ctrl, flash_data);
		break;
	case CFG_FLASH_READ_I2C:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_read(
				flash_ctrl, flash_data);
		break;
	case CFG_FLASH_WRITE_I2C:
		if (flash_ctrl->flash_state == MSM_CAMERA_FLASH_INIT)
			rc = flash_ctrl->func_tbl->camera_flash_write(
				flash_ctrl, flash_data);
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(flash_ctrl->flash_mutex);

	CDBG("Exit %s type %d\n", __func__, flash_data->cfg_type);

	return rc;
}

static long msm_flash_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	struct msm_flash_ctrl_t *fctrl = NULL;
	void __user *argp = (void __user *)arg;

	CDBG("Enter\n");

	if (!sd) {
		pr_err("sd NULL\n");
		return -EINVAL;
	}
	fctrl = v4l2_get_subdevdata(sd);
	if (!fctrl) {
		pr_err("fctrl NULL\n");
		return -EINVAL;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_flash_get_subdev_id(fctrl, argp);
	case VIDIOC_MSM_FLASH_CFG:
		return msm_flash_config(fctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_UNNOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!fctrl->func_tbl) {
			pr_err("fctrl->func_tbl NULL\n");
			return -EINVAL;
		} else {
			return fctrl->func_tbl->camera_flash_release(fctrl);
		}
	default:
		pr_err_ratelimited("invalid cmd %d\n", cmd);
		return -ENOIOCTLCMD;
	}
	CDBG("Exit\n");
}

static struct v4l2_subdev_core_ops msm_flash_subdev_core_ops = {
	.ioctl = msm_flash_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_flash_subdev_ops = {
	.core = &msm_flash_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_flash_internal_ops;

static int32_t msm_flash_get_dt_data(struct device_node *of_node,
	struct msm_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;
		uint32_t i = 0;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	struct msm_camera_power_ctrl_t *power_info =
		&fctrl->power_info;

	CDBG("called\n");

	if (!of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	/* Read the sub device */
	rc = of_property_read_u32(of_node, "cell-index", &fctrl->pdev->id);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	CDBG("subdev id %d\n", fctrl->subdev_id);

	fctrl->flash_driver_type = FLASH_DRIVER_DEFAULT;

	power_info->gpio_conf =
			 kzalloc(sizeof(struct msm_camera_gpio_conf),
				 GFP_KERNEL);
	if (!power_info->gpio_conf) {
		rc = -ENOMEM;
		return rc;
	}
	gconf = power_info->gpio_conf;

	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size) {
		gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t),
			GFP_KERNEL);
		if (!gpio_array) {
			rc = -ENOMEM;
			goto ERROR4;
		}
		for (i = 0; i < gpio_array_size; i++)
			gpio_array[i] = of_get_gpio(of_node, i);

		rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}
		rc = msm_flash_init_gpio_pin_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR5;
		}
		if (power_info->gpio_conf->cam_gpiomux_conf_tbl != NULL)
			pr_err("%s:%d mux install\n", __func__, __LINE__);
		kfree(gpio_array);
	}
	if (fctrl->flash_driver_type == FLASH_DRIVER_DEFAULT)
		fctrl->flash_driver_type = FLASH_DRIVER_GPIO;
	CDBG("%s:%d fctrl->flash_driver_type = %d", __func__, __LINE__,
		fctrl->flash_driver_type);

	return rc;
ERROR5:
		kfree(gconf->cam_gpio_req_tbl);
ERROR4:
		kfree(gconf);
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_flash_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	int32_t i = 0;
	int32_t rc = 0;
	struct video_device *vdev;
	struct v4l2_subdev *sd;
	struct msm_flash_cfg_data_t32 *u32;
	struct msm_flash_cfg_data_t flash_data;
	struct msm_flash_init_info_t32 flash_init_info32;
	struct msm_flash_init_info_t flash_init_info;

	CDBG("Enter");

	if (!file || !arg) {
		pr_err("%s:failed NULL parameter\n", __func__);
		return -EINVAL;
	}
	vdev = video_devdata(file);
	sd = vdev_to_v4l2_subdev(vdev);
	u32 = (struct msm_flash_cfg_data_t32 *)arg;

	flash_data.cfg_type = u32->cfg_type;
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		flash_data.flash_current[i] = u32->flash_current[i];
		flash_data.flash_duration[i] = u32->flash_duration[i];
	}
	switch (cmd) {
	case VIDIOC_MSM_FLASH_CFG32:
		cmd = VIDIOC_MSM_FLASH_CFG;
		switch (flash_data.cfg_type) {
		case CFG_FLASH_OFF:
		case CFG_FLASH_LOW:
		case CFG_FLASH_HIGH:
		case CFG_FLASH_READ_I2C:
		case CFG_FLASH_WRITE_I2C:
			flash_data.cfg.settings = compat_ptr(u32->cfg.settings);
			break;
		case CFG_FLASH_INIT:
			flash_data.cfg.flash_init_info = &flash_init_info;
			if (copy_from_user(&flash_init_info32,
				(void *)compat_ptr(u32->cfg.flash_init_info),
				sizeof(struct msm_flash_init_info_t32))) {
				pr_err("%s copy_from_user failed %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
			flash_init_info.flash_driver_type =
				flash_init_info32.flash_driver_type;
			flash_init_info.slave_addr =
				flash_init_info32.slave_addr;
			flash_init_info.i2c_freq_mode =
				flash_init_info32.i2c_freq_mode;
			flash_init_info.settings =
				compat_ptr(flash_init_info32.settings);
			flash_init_info.power_setting_array =
				compat_ptr(
				flash_init_info32.power_setting_array);
			break;
		default:
			break;
		}
		break;
	default:
		return msm_flash_subdev_ioctl(sd, cmd, arg);
	}

	rc =  msm_flash_subdev_ioctl(sd, cmd, &flash_data);
	for (i = 0; i < MAX_LED_TRIGGERS; i++) {
		u32->flash_current[i] = flash_data.flash_current[i];
		u32->flash_duration[i] = flash_data.flash_duration[i];
	}
	CDBG("Exit");
	return rc;
}

static long msm_flash_subdev_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_flash_subdev_do_ioctl);
}
#endif
static int32_t msm_flash_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_flash_ctrl_t *flash_ctrl = NULL;

	CDBG("Enter");
	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	flash_ctrl = kzalloc(sizeof(struct msm_flash_ctrl_t), GFP_KERNEL);
	if (!flash_ctrl)
		return -ENOMEM;

	memset(flash_ctrl, 0, sizeof(struct msm_flash_ctrl_t));

	flash_ctrl->pdev = pdev;

	rc = msm_flash_get_dt_data(pdev->dev.of_node, flash_ctrl);
	if (rc < 0) {
		pr_err("%s:%d msm_flash_get_dt_data failed\n",
			__func__, __LINE__);
		kfree(flash_ctrl);
		return -EINVAL;
	}

	flash_ctrl->flash_state = MSM_CAMERA_FLASH_RELEASE;
	flash_ctrl->power_info.dev = &flash_ctrl->pdev->dev;
	flash_ctrl->flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	flash_ctrl->flash_mutex = &msm_flash_mutex;

	/* Initialize sub device */
	v4l2_subdev_init(&flash_ctrl->msm_sd.sd, &msm_flash_subdev_ops);
	v4l2_set_subdevdata(&flash_ctrl->msm_sd.sd, flash_ctrl);

	flash_ctrl->msm_sd.sd.internal_ops = &msm_flash_internal_ops;
	flash_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(flash_ctrl->msm_sd.sd.name,
		ARRAY_SIZE(flash_ctrl->msm_sd.sd.name),
		"msm_camera_flash_gpio");
	media_entity_init(&flash_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	flash_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	flash_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_FLASH;
	flash_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x1;
	msm_sd_register(&flash_ctrl->msm_sd);

	CDBG("%s:%d flash sd name = %s", __func__, __LINE__,
		flash_ctrl->msm_sd.sd.entity.name);
	msm_cam_copy_v4l2_subdev_fops(&msm_flash_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_flash_v4l2_subdev_fops.compat_ioctl32 =
		msm_flash_subdev_fops_ioctl;
#endif
	flash_ctrl->msm_sd.sd.devnode->fops = &msm_flash_v4l2_subdev_fops;

	CDBG("probe success\n");
	return rc;
}

MODULE_DEVICE_TABLE(of, msm_flash_dt_match);

static struct platform_driver msm_flash_platform_driver = {
	.probe = msm_flash_platform_probe,
	.driver = {
		.name = "qcom,camera-led-flash",
		.owner = THIS_MODULE,
		.of_match_table = msm_flash_dt_match,
	},
};

static int __init msm_flash_init_module(void)
{
	int32_t rc = 0;

	CDBG("Enter\n");
	rc = platform_driver_register(&msm_flash_platform_driver);
	if (rc)
		pr_err("platform probe for flash failed");

	return rc;
}

static void __exit msm_flash_exit_module(void)
{
	platform_driver_unregister(&msm_flash_platform_driver);
}

static struct msm_flash_table msm_gpio_flash_table = {
	.flash_driver_type = FLASH_DRIVER_GPIO,
	.func_tbl = {
		.camera_flash_init = msm_flash_gpio_init,
		.camera_flash_release = msm_flash_release,
		.camera_flash_off = msm_flash_off,
		.camera_flash_low = msm_flash_low,
		.camera_flash_high = msm_flash_high,
	},
};
module_init(msm_flash_init_module);
module_exit(msm_flash_exit_module);
MODULE_DESCRIPTION("MSM FLASH");
MODULE_LICENSE("GPL v2");
