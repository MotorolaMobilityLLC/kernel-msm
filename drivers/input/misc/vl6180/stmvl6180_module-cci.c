/*
 *  stmvl6180.c - Linux kernel modules for STM VL6180 FlightSense TOF sensor
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <asm/uaccess.h>
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
#include <asm/uaccess.h>
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
#include "vl6180x_api.h"
#include "vl6180x_def.h"
#include "vl6180x_platform.h"
#include "vl6180x_i2c.h"
#include "stmvl6180-cci.h"
#include "stmvl6180-i2c.h"
#include "stmvl6180.h"

#ifdef CAMERA_CCI

#define I2C_MAX_MODE        4
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
	.i2c_write_table_w_microdelay = msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};
static int stmvl6180_get_dt_data(struct device *dev, struct cci_data *data);

/*
 * QCOM specific functions
 */
static int stmvl6180_get_dt_data(struct device *dev, struct cci_data *data)
{
	int rc = 0;
	vl6180_dbgmsg("Enter\n");

	if (dev->of_node) {
		struct device_node *of_node = dev->of_node;
		struct msm_tof_vreg *vreg_cfg;

		if (!of_node) {
			vl6180_errmsg("failed %d\n", __LINE__);
			return -EINVAL;
		}

		rc = of_property_read_u32(of_node, "cell-index", &data->pdev->id);
		if (rc < 0) {
			vl6180_errmsg("failed %d\n", __LINE__);
			return rc;
		}
		vl6180_dbgmsg("cell-index: %d\n", data->pdev->id);
		rc = of_property_read_u32(of_node, "qcom,cci-master", &data->cci_master);
		if (rc < 0) {
			vl6180_errmsg("failed %d\n", __LINE__);
			/* Set default master 0 */
			data->cci_master = MASTER_0;
			rc = 0;
		}
		vl6180_dbgmsg("cci_master: %d\n", data->cci_master);
		rc = of_property_read_u32(of_node, "qcom,slave-addr",&data->slave_addr);
		if (rc < 0) {
			vl6180_errmsg("failed %d\n", __LINE__);
			data->slave_addr = 0x29;
			rc = 0;
		}
		vl6180_dbgmsg("slave addr: %d\n", data->slave_addr);
		if (of_find_property(of_node, "qcom,cam-vreg-name", NULL)) {
			vreg_cfg = &data->vreg_cfg;
			rc = msm_camera_get_dt_vreg_data(of_node,
				&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
			if (rc < 0) {
				vl6180_errmsg("failed %d\n", __LINE__);
				return rc;
			}
		}
		vl6180_dbgmsg("vreg-name: %s min_volt: %d max_volt: %d",
					vreg_cfg->cam_vreg->reg_name,
					vreg_cfg->cam_vreg->min_voltage,
					vreg_cfg->cam_vreg->max_voltage);

		data->en_gpio = of_get_named_gpio(of_node,
						"stmvl6180,ldaf-en-gpio",0);
		if (data->en_gpio < 0 || !gpio_is_valid(data->en_gpio)) {
			vl6180_errmsg("en_gpio is unavailable or invalid\n");
			return data->en_gpio;
	        }
		data->int_gpio = of_get_named_gpio(of_node,
						"stmvl6180,ldaf-int-gpio",0);
		if (data->int_gpio < 0 || !gpio_is_valid(data->int_gpio)) {
			vl6180_errmsg("en_gpio is unvailable or invalid\n");
			return data->int_gpio;
		}
		vl6180_dbgmsg("ldaf_int: %u,ldaf_en: %u\n",
						data->int_gpio,data->en_gpio);

		rc = of_property_read_u32(of_node, "qcom,i2c-freq-mode",
							&data->i2c_freq_mode);
		if (rc < 0 || data->i2c_freq_mode >= I2C_MAX_MODES) {
			data->i2c_freq_mode = 0;
			vl6180_errmsg("invalid i2c frequency mode\n");
		}
		vl6180_dbgmsg("i2c_freq_mode: %u\n",data->i2c_freq_mode);

	}
	vl6180_dbgmsg("End rc =%d\n", rc);

	return rc;
}

static int32_t stmvl6180_vreg_control(struct cci_data *data, int config)
{
	int rc = 0, i, cnt;
	struct msm_tof_vreg *vreg_cfg;

	vl6180_dbgmsg("Enter\n");

	vreg_cfg = &data->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	vl6180_dbgmsg("num_vreg: %d\n", cnt);
	if (!cnt) {
		vl6180_errmsg("failed %d\n", __LINE__);
		return 0;
	}

	if (cnt >= MSM_TOF_MAX_VREGS) {
		vl6180_errmsg("failed %d cnt %d\n", __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(data->pdev->dev), &vreg_cfg->cam_vreg[i], (struct regulator **)&vreg_cfg->data[i], config);
	}

	vl6180_dbgmsg("EXIT rc =%d\n", rc);
	return rc;
}


static int msm_tof_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0;
/*
	struct msm_tof_ctrl_t *tof_ctrl =  v4l2_get_subdevdata(sd);
	if (!tof_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (tof_ctrl->tof_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = tof_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&tof_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
    tof_ctrl->i2c_state = TOF_I2C_RELEASE;
*/
	return rc;
}


static const struct v4l2_subdev_internal_ops msm_tof_internal_ops = {
	.close = msm_tof_close,
};

static long msm_tof_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	vl6180_dbgmsg("Subdev_ioctl not handled\n");
	return 0;
}

static int32_t msm_tof_power(struct v4l2_subdev *sd, int on)
{
	vl6180_dbgmsg("TOF power called\n");
	return 0;
}

static struct v4l2_subdev_core_ops msm_tof_subdev_core_ops = {
	.ioctl = msm_tof_subdev_ioctl,
	.s_power = msm_tof_power,
};

static struct v4l2_subdev_ops msm_tof_subdev_ops = {
	.core = &msm_tof_subdev_core_ops,
};

static int stmvl6180_cci_init(struct cci_data *data)
{
	int rc = 0;
	struct msm_camera_cci_client *cci_client = data->client->cci_client;

	if (FALSE == data->subdev_initialized) {
		data->client->i2c_func_tbl = &msm_sensor_cci_func_tbl;
		data->client->cci_client = kzalloc(sizeof(struct msm_camera_cci_client),  GFP_KERNEL);
		if (!data->client->cci_client) {
			vl6180_errmsg("%d, failed no memory\n", __LINE__);
			return -ENOMEM;
		}
		cci_client = data->client->cci_client;
		cci_client->cci_subdev = msm_cci_get_subdev();
		cci_client->cci_i2c_master = data->cci_master;
		v4l2_subdev_init(&data->msm_sd.sd, data->subdev_ops);
		v4l2_set_subdevdata(&data->msm_sd.sd, data);
		data->msm_sd.sd.internal_ops = &msm_tof_internal_ops;
		data->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
		snprintf(data->msm_sd.sd.name, ARRAY_SIZE(data->msm_sd.sd.name), "msm_tof");
		media_entity_init(&data->msm_sd.sd.entity, 0, NULL, 0);
		data->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
		data->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_TOF;
		data->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
		msm_sd_register(&data->msm_sd);
		msm_tof_v4l2_subdev_fops = v4l2_subdev_fops;
		/*
		#ifdef CONFIG_COMPAT
				msm_tof_v4l2_subdev_fops.compat_ioctl32 = msm_tof_subdev_fops_ioctl;
		#endif
		*/
		data->msm_sd.sd.devnode->fops = &msm_tof_v4l2_subdev_fops;
		data->subdev_initialized = TRUE;
	}

	cci_client->sid = data->slave_addr;
	cci_client->i2c_freq_mode = data->i2c_freq_mode;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->cci_i2c_master = data->cci_master;
	rc = data->client->i2c_func_tbl->i2c_util(data->client, MSM_CCI_INIT);
	if (rc < 0) {
		vl6180_errmsg("%d: CCI Init failed\n", __LINE__);
		return rc;
	}
	vl6180_dbgmsg("CCI Init Succeeded\n");

	data->client->addr_type = MSM_CAMERA_I2C_WORD_ADDR;

    return 0;
}

static int32_t stmvl6180_platform_probe(struct platform_device *pdev)
{
	struct stmvl6180_data *vl6180_data = NULL;
	struct cci_data *data = NULL;
	int32_t rc = 0;

	vl6180_dbgmsg("Enter\n");

	if (!pdev->dev.of_node) {
		vl6180_errmsg("of_node NULL\n");
		return -EINVAL;
	}

	vl6180_data = stmvl6180_getobject();
	if (NULL == vl6180_data) {
		vl6180_errmsg("Object not found!\n");
		return -EINVAL;
	}

	data = &(vl6180_data->client_object);
	if (!data) {
		vl6180_errmsg("data NULL\n");
		return -EINVAL;
	}

	data->client = (struct msm_camera_i2c_client *)&data->g_client;

	/* setup platform i2c client */
	i2c_setclient((void *)data->client);

	/* Set platform device handle */
	data->subdev_ops = &msm_tof_subdev_ops;
	data->pdev = pdev;
	rc = stmvl6180_get_dt_data(&pdev->dev, data);
	if (rc < 0) {
		vl6180_errmsg("%d, failed rc %d\n", __LINE__, rc);
		return rc;
	}
	data->subdev_id = pdev->id;

	rc = gpio_request_one(data->en_gpio,GPIOF_OUT_INIT_LOW,
						"stmvl6180_ldaf_en_gpio");
	if(rc) {
		vl6180_errmsg("failed gpio request %u\n",data->en_gpio);
		return -EINVAL;
	}
	/* ldaf_int not used */
	data->int_gpio = -1;

	/* Set device type as platform device */
	data->device_type = MSM_CAMERA_PLATFORM_DEVICE;
	data->subdev_initialized = FALSE;

	vl6180_dbgmsg("End\n");

	return rc;

}


static const struct of_device_id st_stmv16180_dt_match[] = {
	{ .compatible = "st,stmvl6180", },
	{ },
};

static struct platform_driver stmvl6180_platform_driver = {
	.probe = stmvl6180_platform_probe,
	.driver = {
		.name = STMVL6180_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = st_stmv16180_dt_match,
	},
};

int stmvl6180_power_up_cci(void *cci_object, unsigned int *preset_flag)
{
	int ret = 0;
	struct cci_data *data = (struct cci_data *)cci_object;

	vl6180_dbgmsg("Enter");

	/* need to init cci first */
	ret = stmvl6180_cci_init(data);
	if (ret) {
		vl6180_errmsg("stmvl6180_stmvl6180_cci_init failed %d\n", __LINE__);
		return ret;
	}
	/* actual power up */
	if (data && data->device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		ret = stmvl6180_vreg_control(data, 1);
		if (ret < 0) {
			vl6180_errmsg("stmvl6180_vreg_control failed %d\n", __LINE__);
			return ret;
		}
		gpio_set_value(data->en_gpio,STMVL6180_GPIO_ENABLE);
	}
	data->power_up = 1;
	*preset_flag = 1;
	vl6180_dbgmsg("End\n");

	return ret;
}

int stmvl6180_power_down_cci(void *cci_object)
{
	int ret = 0;
	struct cci_data *data = (struct cci_data *)cci_object;

	vl6180_dbgmsg("Enter\n");
	if (data->power_up) {
		/* need to release cci first */
		ret = data->client->i2c_func_tbl->i2c_util(data->client, MSM_CCI_RELEASE);
		if (ret < 0) {
			vl6180_errmsg("CCI Release failed rc %d\n", ret);
		}
		/* actual power down */
		if (data->device_type == MSM_CAMERA_PLATFORM_DEVICE) {
			gpio_set_value(data->en_gpio,STMVL6180_GPIO_DISABLE);
			ret = stmvl6180_vreg_control(data, 0);
			if (ret < 0) {
				vl6180_errmsg("stmvl6180_vreg_control failed %d\n", __LINE__);
				return ret;
			}
		}
	}
	//data->power_up = 0;
	vl6180_dbgmsg("End\n");
	return ret;
}

int stmvl6180_init_cci(void)
{
	int ret = 0;
	vl6180_dbgmsg("Enter\n");

	/* register as a platform device */
	ret = platform_driver_register(&stmvl6180_platform_driver);
	if (ret)
		vl6180_errmsg("%d, error ret:%d\n", __LINE__, ret);

	vl6180_dbgmsg("End\n");

	return ret;
}

void stmvl6180_exit_cci(void *cci_object)
{
	struct cci_data *data = (struct cci_data *)cci_object;
	vl6180_dbgmsg("Enter\n");

	if (data && data->client->cci_client)
		kfree(data->client->cci_client);

	vl6180_dbgmsg("End\n");
}
#endif /* end of CAMERA_CCI */
