/*
*  stmvl53l0_module-i2c.c - Linux kernel modules for STM VL53L0 FlightSense TOF
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
#include "stmvl53l0-i2c.h"
#include "stmvl53l0-cci.h"
#include "stmvl53l0.h"

#define LASER_SENSOR_PINCTRL_STATE_SLEEP "laser_suspend"
#define LASER_SENSOR_PINCTRL_STATE_DEFAULT "laser_default"

/*
 * Global data
 */
static int stmvl53l0_parse_vdd(struct device *dev, struct i2c_data *data);

/*
 * QCOM specific functions
 */
static int stmvl53l0_parse_vdd(struct device *dev, struct i2c_data *data)
{
	int ret = 0;
	vl53l0_dbgmsg("Enter\n");

	if (dev->of_node) {
		data->vana = regulator_get_optional(dev, "vdd");
		if (IS_ERR(data->vana)) {
			vl53l0_dbgmsg("%d,vdd supply is not provided\n",
				__LINE__);
			ret = -1;
		}
	}
	vl53l0_dbgmsg("End\n");

	return ret;
}
int get_dt_xtalk_data(struct device_node *of_node, int *xtalk)
{
	int rc = 0;
	uint32_t count = 0;
	uint32_t v_array[1];

	count = of_property_count_strings(of_node, "st,xtalkval");

	if (!count)
		return 0;

	rc = of_property_read_u32_array(of_node, "st,xtalkval",
		v_array, 1);

	if (rc != -EINVAL) {
		if (rc < 0)
			pr_err("%s failed %d\n", __func__, __LINE__);
		else
			*xtalk = v_array[0];
	} else
		rc = 0;

	return rc;
}
int get_dt_threshold_data(struct device_node *of_node, int *lowv, int *highv)
{
	int rc = 0;
	uint32_t count = 0;
	uint32_t v_array[2];

	count = of_property_count_strings(of_node, "st,sensorthreshold");

	if (!count)
		return 0;

	rc = of_property_read_u32_array(of_node, "st,sensorthreshold",
		v_array, 2);

	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
		} else {
			*lowv = v_array[0];
			*highv = v_array[1];
		}
	} else {
		rc = 0;
	}

	return rc;
}

static int stmvl53l0_get_dt_data(struct device *dev, struct i2c_data *data)
{
	int rc = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	int i;
	struct msm_pinctrl_info *sensor_pctrl = NULL;

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
		rc = get_dt_threshold_data(of_node,
			&(data->lowv), &(data->highv));
		rc = get_dt_xtalk_data(of_node,
			&(data->xtalk));
	}
	vl53l0_dbgmsg("End rc =%d\n", rc);

	return rc;
}

static int stmvl53l0_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int rc = 0;
	struct stmvl53l0_data *vl53l0_data = NULL;
	struct i2c_data *i2c_object = NULL;
	int present;

	vl53l0_dbgmsg("Enter\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE)) {
		rc = -EIO;
		return rc;
	}

	vl53l0_data = stmvl53l0_getobject();
	if (!vl53l0_data) {
		vl53l0_errmsg("%d,data NULL\n", __LINE__);
		return -EINVAL;
	}

	if (vl53l0_data)
		i2c_object = &(vl53l0_data->i2c_client_object);
	i2c_object->client = client;

	/* setup regulator */
	stmvl53l0_parse_vdd(&i2c_object->client->dev, i2c_object);

	/* setup device name */
	vl53l0_data->dev_name = dev_name(&client->dev);

	/* setup device data */
	dev_set_drvdata(&client->dev, vl53l0_data);

	/* setup client data */
	i2c_set_clientdata(client, vl53l0_data);

	stmvl53l0_get_dt_data(&client->dev, i2c_object);
	rc = stmvl53l0_power_up_i2c(i2c_object, &present);
	if (rc) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		return rc;
	}
	rc = stmvl53l0_checkmoduleid(vl53l0_data, i2c_object->client, I2C_BUS);
	if (rc != 0) {
		vl53l0_errmsg("%d,error rc %d\n", __LINE__, rc);
		stmvl53l0_power_down_i2c(i2c_object);
		return rc;
	}
	stmvl53l0_power_down_i2c(i2c_object);

	rc = stmvl53l0_setup(vl53l0_data, I2C_BUS);

	vl53l0_dbgmsg("End\n");
	return rc;
}

static int stmvl53l0_remove(struct i2c_client *client)
{
	struct stmvl53l0_data *data = stmvl53l0_getobject();

	vl53l0_dbgmsg("Enter\n");

	/* Power down the device */
	stmvl53l0_power_down_i2c((void *)data->client_object);

	vl53l0_dbgmsg("End\n");
	return 0;
}

static const struct i2c_device_id stmvl53l0_id[] = {
	{ STMVL53L0_DRV_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, stmvl53l0_id);

static const struct of_device_id st_stmvl53l0_dt_match[] = {
	{ .compatible = "st,stmvl53l0_i2c", },
	{ },
};

static struct i2c_driver stmvl6180_driver = {
	.driver = {
		.name	= STMVL53L0_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = st_stmvl53l0_dt_match,
	},
	.probe	= stmvl53l0_probe,
	.remove	= stmvl53l0_remove,
	.id_table = stmvl53l0_id,

};

int stmvl53l0_power_up_i2c(void *i2c_object, unsigned int *preset_flag)
{
	int ret = 0;

	struct i2c_data *data = (struct i2c_data *)i2c_object;

	vl53l0_dbgmsg("Enter i2c powerup\n");
	pinctrl_select_state(data->pinctrl_info.pinctrl,
					data->pinctrl_info.gpio_state_active);

	if (!IS_ERR(data->vana)) {
		ret = regulator_set_voltage(data->vana, VL53L0_VDD_MIN,
				VL53L0_VDD_MAX);
		if (ret < 0) {
			vl53l0_errmsg("set_vol(%p) fail %d\n", data->vana, ret);
			return ret;
		}
		ret = regulator_enable(data->vana);

		if (ret < 0) {
			vl53l0_errmsg("reg enable(%p) failed.rc=%d\n",
					data->vana, ret);
			return ret;
		}
	}

	msm_camera_request_gpio_table(
		data->gconf.cam_gpio_req_tbl,
		data->gconf.cam_gpio_req_tbl_size, 1);

	gpio_set_value_cansleep(data->gconf.cam_gpio_req_tbl[0].gpio, 1);
	msleep(200);
	data->power_up = 1;
	*preset_flag = 1;

	vl53l0_dbgmsg("End\n");
	return ret;
}

int stmvl53l0_power_down_i2c(void *i2c_object)
{
	int ret = 0;

	struct i2c_data *data = (struct i2c_data *)i2c_object;

	vl53l0_dbgmsg("Enter\n");
	if (data->power_up) {
		pinctrl_select_state(data->pinctrl_info.pinctrl,
			data->pinctrl_info.gpio_state_suspend);

		msm_camera_request_gpio_table(
			data->gconf.cam_gpio_req_tbl,
			data->gconf.cam_gpio_req_tbl_size, 0);

		gpio_set_value_cansleep(
			data->gconf.cam_gpio_req_tbl[0].gpio, 0);

		if (!IS_ERR(data->vana)) {
			ret = regulator_disable(data->vana);
			if (ret < 0)
				vl53l0_errmsg("reg disable(%p) failed.rc=%d\n",
				data->vana, ret);
		}

		data->power_up = 0;
	}


	vl53l0_dbgmsg("End\n");
	return ret;
}

int stmvl53l0_init_i2c(void)
{
	int ret = 0;

	vl53l0_dbgmsg("Enter\n");

	/* register as a i2c client device */
	ret = i2c_add_driver(&stmvl6180_driver);
	if (ret)
		vl53l0_errmsg("%d erro ret:%d\n", __LINE__, ret);

	vl53l0_dbgmsg("End with rc:%d\n", ret);

	return ret;
}

void stmvl53l0_exit_i2c(void *i2c_object)
{
	vl53l0_dbgmsg("Enter\n");
	i2c_del_driver(&stmvl6180_driver);

	vl53l0_dbgmsg("End\n");
}

