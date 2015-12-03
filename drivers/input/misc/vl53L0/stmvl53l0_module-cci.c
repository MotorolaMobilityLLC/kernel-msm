/*
*  stmvl53l0_module-cci.c - Linux kernel modules for STM VL53L0 FlightSense TOF
*							sensor
*
*  Copyright (C) 2015 STMicroelectronics Imaging Division.
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program;
*/
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/platform_device.h>
/*
 * power specific includes
 */
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
/*
 * API includes
 */
#include "vl53l0_api.h"
#include "vl53l0_def.h"
#include "vl53l0_platform.h"
#include "stmvl53l0-cci.h"
#include "stmvl53l0-i2c.h"
#include "stmvl53l0.h"
#define LASER_SENSOR_PINCTRL_STATE_SLEEP "laser_suspend"
#define LASER_SENSOR_PINCTRL_STATE_DEFAULT "laser_default"

/*
 * Global data
 */
static struct v4l2_file_operations msm_tof_v4l2_subdev_fops;
static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};
static int stmvl53l0_get_dt_data(struct device *dev, struct cci_data *data);

/*
 * QCOM specific functions
 */
static int stmvl53l0_get_dt_data(struct device *dev, struct cci_data *data)
{
	int rc = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	int i;
	struct msm_pinctrl_info *sensor_pctrl = NULL;
	struct msm_tof_vreg *vreg_cfg = NULL;

	vl53l0_dbgmsg("Enter\n");

	sensor_pctrl = &data->pinctrl_info;
	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
		LASER_SENSOR_PINCTRL_STATE_DEFAULT);

	sensor_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(sensor_pctrl->pinctrl,
		LASER_SENSOR_PINCTRL_STATE_SLEEP);

	if (dev->of_node) {
		struct device_node *of_node = dev->of_node;

		if (!of_node) {
			vl53l0_errmsg("failed %d\n", __LINE__);
			return -EINVAL;
		}

		rc = of_property_read_u32(of_node,
				"cell-index", &data->pdev->id);
		if (rc < 0) {
			vl53l0_errmsg("failed %d\n", __LINE__);
			return rc;
		}
		vl53l0_dbgmsg("cell-index: %d\n", data->pdev->id);
		rc = of_property_read_u32(of_node,
			"qcom,cci-master", &data->cci_master);
		if (rc < 0) {
			vl53l0_errmsg("failed %d\n", __LINE__);
			/* Set default master 0 */
			data->cci_master = MASTER_0;
			rc = 0;
		}
		vl53l0_dbgmsg("cci_master: %d\n", data->cci_master);

		if (of_find_property(of_node,
					"qcom,cam-vreg-name", NULL)) {
			vl53l0_dbgmsg("vreg setting is found");
			vreg_cfg = &data->vreg_cfg;
			rc = msm_camera_get_dt_vreg_data(of_node,
				 &vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
			if (rc < 0) {
				vl53l0_errmsg("failed %d\n", __LINE__);
				rc = 0;
			}
		}
		vl53l0_dbgmsg("vreg num: %d\n", vreg_cfg->num_vreg);

		gpio_array_size = of_gpio_count(of_node);
		gconf = &data->gconf;

		if (gpio_array_size) {
			gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t),
				GFP_KERNEL);

			for (i = 0; i < gpio_array_size; i++) {
				gpio_array[i] = of_get_gpio(of_node, i);
				pr_err("%s gpio_array[%d] = %d\n", __func__, i,
					gpio_array[i]);
			}

			rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				return rc;
			}

			rc = msm_camera_init_gpio_pin_tbl(of_node, gconf,
				gpio_array, gpio_array_size);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				return rc;
			}
		}
	}
	vl53l0_dbgmsg("End rc =%d\n", rc);

	return rc;
}

static int32_t stmvl53l0_vreg_control(struct cci_data *data, int config)
{
	int rc = 0, i, cnt;
	struct msm_tof_vreg *vreg_cfg;

	vl53l0_dbgmsg("Enter with config %d\n", config);

	vreg_cfg = &data->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt) {
		vl53l0_errmsg("failed %d\n", __LINE__);
		return 0;
	}

	if (cnt >= MSM_TOF_MAX_VREGS) {
		vl53l0_errmsg("failed %d cnt %d\n", __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(data->pdev->dev),
				&vreg_cfg->cam_vreg[i],
				(struct regulator **)&vreg_cfg->data[i],
				config);
	}

	vl53l0_dbgmsg("EXIT rc =%d\n", rc);
	return rc;
}

static int msm_tof_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	return rc;
}


static const struct v4l2_subdev_internal_ops msm_tof_internal_ops = {
	.close = msm_tof_close,
};

static long msm_tof_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	vl53l0_dbgmsg("Subdev_ioctl not handled\n");
	return 0;
}

static int32_t msm_tof_power(struct v4l2_subdev *sd, int on)
{
	vl53l0_dbgmsg("TOF power called\n");
	return 0;
}

static struct v4l2_subdev_core_ops msm_tof_subdev_core_ops = {
	.ioctl = msm_tof_subdev_ioctl,
	.s_power = msm_tof_power,
};

static struct v4l2_subdev_ops msm_tof_subdev_ops = {
	.core = &msm_tof_subdev_core_ops,
};

static int stmvl53l0_cci_init(struct cci_data *data)
{
	int rc = 0;
	struct msm_camera_cci_client *cci_client = data->client->cci_client;

	if (FALSE == data->subdev_initialized) {
		data->client->i2c_func_tbl = &msm_sensor_cci_func_tbl;
		data->client->cci_client =
			kzalloc(sizeof(struct msm_camera_cci_client),
			GFP_KERNEL);

		if (!data->client->cci_client) {
			vl53l0_errmsg("%d, failed no memory\n", __LINE__);
			return -ENOMEM;
		}
		cci_client = data->client->cci_client;
		cci_client->cci_subdev = msm_cci_get_subdev();
		cci_client->cci_i2c_master = data->cci_master;
		v4l2_subdev_init(&data->msm_sd.sd, data->subdev_ops);
		v4l2_set_subdevdata(&data->msm_sd.sd, data);
		data->msm_sd.sd.internal_ops = &msm_tof_internal_ops;
		data->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		snprintf(data->msm_sd.sd.name,
				ARRAY_SIZE(data->msm_sd.sd.name), "msm_tof");
		media_entity_init(&data->msm_sd.sd.entity, 0, NULL, 0);
		data->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
		data->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_TOF;
		data->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
		msm_sd_register(&data->msm_sd);
		msm_tof_v4l2_subdev_fops = v4l2_subdev_fops;
		data->msm_sd.sd.devnode->fops = &msm_tof_v4l2_subdev_fops;
		data->subdev_initialized = TRUE;
	}

	vl53l0_dbgmsg("inited\n");
	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = data->cci_master;
	cci_client->i2c_freq_mode = I2C_FAST_MODE;
	rc = data->client->i2c_func_tbl->i2c_util(data->client, MSM_CCI_INIT);
	if (rc < 0) {
		vl53l0_errmsg("%d: CCI Init failed\n", __LINE__);
		return rc;
	}
	vl53l0_dbgmsg("CCI Init Succeeded\n");
	data->client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;

	return 0;
}

static int32_t stmvl53l0_platform_probe(struct platform_device *pdev)
{
	struct stmvl53l0_data *vl53l0_data = NULL;
	struct cci_data *data = NULL;
	unsigned int present = 0;

	int32_t rc = 0;

	vl53l0_dbgmsg("Enter\n");

	if (!pdev->dev.of_node) {
		vl53l0_errmsg("%d,of_node NULL\n", __LINE__);
		return -EINVAL;
	}

	vl53l0_data = stmvl53l0_getobject();
	if (NULL == vl53l0_data) {
		vl53l0_errmsg("%d,Object not found!\n", __LINE__);
		return -EINVAL;
	}
	data = &(vl53l0_data->cci_client_object);
	if (!data) {
		vl53l0_errmsg("%d,data NULL\n", __LINE__);
		return -EINVAL;
	}

	/* Set platform device handle */
	data->subdev_ops = &msm_tof_subdev_ops;
	data->pdev = pdev;
	stmvl53l0_get_dt_data(&pdev->dev, data);
	data->subdev_id = pdev->id;

	/* Set device type as platform device */
	data->device_type = MSM_CAMERA_PLATFORM_DEVICE;
	data->subdev_initialized = FALSE;

	data->client = (struct msm_camera_i2c_client *)&data->g_client;

	rc = stmvl53l0_power_up_cci(data, &present);
	if (rc) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	rc = stmvl53l0_checkmoduleid(vl53l0_data, data->client, CCI_BUS);
	if (rc != 0) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		stmvl53l0_power_down_cci(data);
		return rc;
	}
	stmvl53l0_power_down_cci(data);
	stmvl53l0_setup(vl53l0_data, CCI_BUS);

	vl53l0_dbgmsg("End\n");

	return rc;

}


static const struct of_device_id st_stmvl53l0_dt_match[] = {
	{ .compatible = "st,stmvl53l0", },
	{ },
};

static struct platform_driver stmvl53l0_platform_driver = {
	.probe = stmvl53l0_platform_probe,
	.driver = {
		.name = STMVL53L0_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = st_stmvl53l0_dt_match,
	},
};

int stmvl53l0_power_up_cci(void *cci_object, unsigned int *preset_flag)
{
	int ret = 0;
	struct cci_data *data = (struct cci_data *)cci_object;

	vl53l0_dbgmsg("Enter");
	pinctrl_select_state(data->pinctrl_info.pinctrl,
		data->pinctrl_info.gpio_state_active);

	stmvl53l0_vreg_control(data, 1);
	msleep(20);

	/* need to init cci first */
	ret = stmvl53l0_cci_init(data);
	if (ret) {
		vl53l0_errmsg("stmvl53l0_cci_init failed %d\n", __LINE__);
		return ret;
	}

	msm_camera_request_gpio_table(
		data->gconf.cam_gpio_req_tbl,
		data->gconf.cam_gpio_req_tbl_size, 1);
	gpio_set_value_cansleep(data->gconf.cam_gpio_req_tbl[0].gpio, 1);
	data->power_up = 1;
	*preset_flag = 1;
	vl53l0_dbgmsg("End\n");
	msleep(20);
	return ret;
}

int stmvl53l0_power_down_cci(void *cci_object)
{
	int ret = 0;
	struct cci_data *data = (struct cci_data *)cci_object;

	vl53l0_dbgmsg("Enter\n");
	if (data->power_up) {
		/* need to release cci first */
		ret = data->client->i2c_func_tbl->i2c_util(data->client,
				MSM_CCI_RELEASE);
		if (ret < 0)
			vl53l0_errmsg("%d,CCI Release failed rc %d\n",
			__LINE__, ret);

		pinctrl_select_state(data->pinctrl_info.pinctrl,
			data->pinctrl_info.gpio_state_suspend);

		msm_camera_request_gpio_table(
			data->gconf.cam_gpio_req_tbl,
			data->gconf.cam_gpio_req_tbl_size, 0);

		gpio_set_value_cansleep(
			data->gconf.cam_gpio_req_tbl[0].gpio, 0);

		stmvl53l0_vreg_control(data, 0);
	}
	data->power_up = 0;
	vl53l0_dbgmsg("End\n");
	return ret;
}

int stmvl53l0_init_cci(void)
{
	int ret = 0;
	vl53l0_dbgmsg("Enter\n");

	/* register as a platform device */
	ret = platform_driver_register(&stmvl53l0_platform_driver);
	if (ret)
		vl53l0_errmsg("%d, error ret:%d\n", __LINE__, ret);

	vl53l0_dbgmsg("End\n");

	return ret;
}

void stmvl53l0_exit_cci(void *cci_object)
{
	struct cci_data *data = (struct cci_data *)cci_object;
	vl53l0_dbgmsg("Enter\n");

	if (data && data->client->cci_client)
		kfree(data->client->cci_client);

	vl53l0_dbgmsg("End\n");
}

