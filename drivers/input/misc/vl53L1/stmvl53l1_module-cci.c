/*
 *  stmvl53l1_module-cci.c - Linux kernel modules for STM VL53L1 FlightSense TOF
 *							sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
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
#include "vl53l1_api.h"
#include "vl53l1_def.h"
#include "vl53l1_platform.h"
#include "stmvl53l1-cci.h"
#include "stmvl53l1-i2c.h"
#include "stmvl53l1.h"

#ifdef CAMERA_CCI


#define TOF_SENSOR_NAME         "tof_vl53l3"
#define STMVL53l1_DRV_NAME      "stmvl53l1"


#define TOF_DEVICE_TYPE         0x000100ff
struct cam_subdev  *g_v4l2_dev_str = NULL;

static int stmvl53l1_request_xsdn(struct tof_ctrl_t *tof_ctrl)
{
	int rc = 0;

	tof_ctrl->io_flag.xsdn_owned = 0;
	if (tof_ctrl->xsdn_gpio == -1) {
		vl53l1_errmsg("reset gpio is required");
		rc = -ENODEV;
		goto no_gpio;
	}

	vl53l1_dbgmsg("request xsdn_gpio %d", tof_ctrl->xsdn_gpio);
	rc = gpio_request(tof_ctrl->xsdn_gpio, "vl53l1_xsdn");
	if (rc) {
		vl53l1_errmsg("fail to acquire xsdn %d", rc);
		goto no_gpio;
	}

	rc = gpio_direction_output(tof_ctrl->xsdn_gpio, 0);
	if (rc) {
		vl53l1_errmsg("fail to configure xsdn as output %d", rc);
		goto direction_failed;
	}
	tof_ctrl->io_flag.xsdn_owned = 1;

	return rc;

direction_failed:
	gpio_free(tof_ctrl->xsdn_gpio);
no_gpio:
	return rc;
}

static void stmvl53l1_release_xsdn(struct tof_ctrl_t *tof_ctrl)
{
	if (tof_ctrl->io_flag.xsdn_owned) {
		vl53l1_dbgmsg("release xsdn_gpio %d", tof_ctrl->xsdn_gpio);
		gpio_free(tof_ctrl->xsdn_gpio);
		tof_ctrl->io_flag.xsdn_owned = 0;
	}
	tof_ctrl->xsdn_gpio = -1;
}

static int stmvl53l1_request_pwren(struct tof_ctrl_t *tof_ctrl)
{
	int rc = 0;

	tof_ctrl->io_flag.pwr_owned = 0;
	if (tof_ctrl->pwren_gpio == -1) {
		vl53l1_wanrmsg("pwren gpio disable");
		goto no_gpio;
	}

	vl53l1_dbgmsg("request pwren_gpio %d", tof_ctrl->pwren_gpio);
	rc = gpio_request(tof_ctrl->pwren_gpio, "vl53l1_pwren");
	if (rc) {
		vl53l1_errmsg("fail to acquire pwren %d", rc);
		goto no_gpio;
	}

	rc = gpio_direction_output(tof_ctrl->pwren_gpio, 0);
	if (rc) {
		vl53l1_errmsg("fail to configure pwren as output %d", rc);
		goto direction_failed;
	}
	tof_ctrl->io_flag.pwr_owned = 1;

	return rc;

direction_failed:
	gpio_free(tof_ctrl->xsdn_gpio);

no_gpio:
	return rc;
}

static void stmvl53l1_release_pwren(struct tof_ctrl_t *tof_ctrl)
{
	if (tof_ctrl->io_flag.pwr_owned) {
		vl53l1_dbgmsg("release pwren_gpio %d", tof_ctrl->pwren_gpio);
		gpio_free(tof_ctrl->pwren_gpio);
		tof_ctrl->io_flag.pwr_owned = 0;
	}

	tof_ctrl->pwren_gpio = -1;
}

static int stmvl53l1_request_intr(struct tof_ctrl_t *tof_ctrl)
{
	int rc = 0;
	const char *desc = "stm_irq";

	tof_ctrl->io_flag.intr_owned = 0;
	if (tof_ctrl->intr_gpio == -1) {
		vl53l1_wanrmsg("no interrupt gpio");
		goto end;
	}

	tof_ctrl->irq = gpio_to_irq(tof_ctrl->intr_gpio);
	if (tof_ctrl->irq < 0) {
		vl53l1_errmsg("fail to map GPIO: %d to interrupt:%d\n",
				tof_ctrl->intr_gpio, tof_ctrl->irq);
		goto end;
	}

	rc = devm_gpio_request_one(&tof_ctrl->pdev->dev, tof_ctrl->intr_gpio, GPIOF_IN, desc);
	if (rc < 0) {
		vl53l1_errmsg("failed to request gpio %d, error %d\n", tof_ctrl->intr_gpio, rc);
		goto end;
	}

	tof_ctrl->io_flag.intr_owned = 1;
end:
	return rc;
}

static void stmvl53l1_release_intr(struct tof_ctrl_t *tof_ctrl)
{
	if (tof_ctrl->io_flag.intr_owned) {
		if (tof_ctrl->io_flag.intr_started) {
			free_irq(tof_ctrl->irq, tof_ctrl);
			tof_ctrl->io_flag.intr_started = 0;
		}
		vl53l1_dbgmsg("release intr_gpio %d", tof_ctrl->intr_gpio);
		gpio_free(tof_ctrl->intr_gpio);
		tof_ctrl->io_flag.intr_owned = 0;
	}
	tof_ctrl->intr_gpio = -1;
}

static void stmvl53l1_release_gpios_cci(struct tof_ctrl_t *t_ctrl)
{
	stmvl53l1_release_xsdn(t_ctrl);
	if (t_ctrl->power_supply) {
		regulator_put(t_ctrl->power_supply);
		t_ctrl->power_supply = NULL;
	}
	stmvl53l1_release_pwren(t_ctrl);
	stmvl53l1_release_intr(t_ctrl);
}

static int stmvl53l1_get_dt_info(struct device *dev, struct tof_ctrl_t *t_ctrl)
{
	int rc = 0;
	struct device_node   *of_node  = NULL;

	if (!dev || !t_ctrl)
		return -EINVAL;

	of_node  = dev->of_node;
	if (!of_node) {
		vl53l1_errmsg("of_node is NULL %d\n", __LINE__);
		return -EINVAL;
	}

	t_ctrl->xsdn_gpio = -1;
	t_ctrl->pwren_gpio = -1;
	t_ctrl->intr_gpio = -1;

	rc = of_property_read_u32(of_node, "cell-index", &t_ctrl->pdev->id);
	if (rc < 0) {
		vl53l1_errmsg("failed to read cell index %d\n", __LINE__);
		return rc;
	}

	rc = of_property_read_u32(of_node, "cci-master", &t_ctrl->cci_master);
	if (rc < 0) {
		vl53l1_errmsg("failed to get the cci master %d\n", __LINE__);
		return rc;
	}
	rc = of_property_read_u32(of_node, "cci-device", &t_ctrl->cci_num);
	if (rc < 0) {
		/* Set default master 0 */
		t_ctrl->cci_num = CCI_DEVICE_0;
		rc = 0;
	}
	t_ctrl->io_master_info.cci_client->cci_device = t_ctrl->cci_num;

	t_ctrl->power_supply = regulator_get(dev, "laser");
	if (IS_ERR(t_ctrl->power_supply) || t_ctrl->power_supply == NULL) {
		t_ctrl->power_supply = NULL;
		/* try gpio */
		rc = of_property_read_u32_array(dev->of_node, "pwren-gpio", &t_ctrl->pwren_gpio, 1);
		if (rc) {
			t_ctrl->pwren_gpio = -1;
			vl53l1_wanrmsg("no regulator, nor power gpio => power ctrl disabled");
		}
	}

	t_ctrl->cci_supply = regulator_get(dev, "cci");
	if (IS_ERR(t_ctrl->cci_supply)) {
		t_ctrl->cci_supply = NULL;
		vl53l1_wanrmsg("Unable to cci power supply %d %d");
	}

	rc = of_property_read_u32_array(dev->of_node, "xsdn-gpio", &t_ctrl->xsdn_gpio, 1);
	if (rc) {
		vl53l1_wanrmsg("Unable to find xsdn-gpio %d %d",
		rc, t_ctrl->xsdn_gpio);
		if (of_gpio_count(dev->of_node) >= 2)
			t_ctrl->xsdn_gpio = of_get_gpio(dev->of_node, 0);
		else
			t_ctrl->xsdn_gpio = -1;
		}

	rc = of_property_read_u32_array(dev->of_node, "intr-gpio", &t_ctrl->intr_gpio, 1);
	if (rc) {
		vl53l1_wanrmsg("Unable to find intr-gpio %d %d",
		rc, t_ctrl->intr_gpio);
		if (of_gpio_count(dev->of_node) >= 2)
			t_ctrl->intr_gpio = of_get_gpio(dev->of_node, 1);
		else
			t_ctrl->intr_gpio = -1;
		}

	/* configure gpios */
	rc = stmvl53l1_request_xsdn(t_ctrl);
	if (rc)
		goto no_xsdn;
	rc = stmvl53l1_request_pwren(t_ctrl);
	if (rc)
		goto no_pwren;
	rc = stmvl53l1_request_intr(t_ctrl);
	if (rc)
		goto no_intr;

	return rc;

no_intr:
	if (t_ctrl->power_supply) {
		regulator_put(t_ctrl->power_supply);
		t_ctrl->power_supply = NULL;
	}
	stmvl53l1_release_pwren(t_ctrl);
no_pwren:
	stmvl53l1_release_xsdn(t_ctrl);
no_xsdn:
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
	int32_t rc = 0;
	return rc;
}

static int32_t msm_tof_power(struct v4l2_subdev *sd, int on)
{
	vl53l1_dbgmsg("TOF power called\n");
	return 0;
}

static struct v4l2_subdev_core_ops msm_tof_subdev_core_ops = {
	.ioctl = msm_tof_subdev_ioctl,
	.s_power = msm_tof_power,
};

static struct v4l2_subdev_ops msm_tof_subdev_ops = {
	.core = &msm_tof_subdev_core_ops,
};

static int32_t stmvl53l1_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct stmvl53l1_data *vl53l1_data  = NULL;
	struct tof_ctrl_t *tof_ctrl         = NULL;
	struct cam_sensor_cci_client *cci_client = NULL;

	vl53l1_errmsg("Enter\n");

	if (!cam_cci_get_subdev(0)){
		return -EPROBE_DEFER;
	}

	vl53l1_data = kzalloc(sizeof(struct stmvl53l1_data), GFP_KERNEL);
	if (!vl53l1_data) {
		rc = -ENOMEM;
		return rc;
	}

	if (vl53l1_data) {
		vl53l1_data->client_object = kzalloc(sizeof(struct tof_ctrl_t), GFP_KERNEL);
		if (!vl53l1_data->client_object) {
			rc = -ENOMEM;
			goto free_vl53l1_data;
		}
		tof_ctrl = (struct tof_ctrl_t *)vl53l1_data->client_object;
	}

	tof_ctrl->pdev = pdev;

	tof_ctrl->vl53l1_data = vl53l1_data;
	tof_ctrl->device_type = MSM_CAMERA_PLATFORM_DEVICE;

	tof_ctrl->io_master_info.master_type = CCI_MASTER;
	tof_ctrl->io_master_info.cci_client = kzalloc(
		sizeof(struct cam_sensor_cci_client), GFP_KERNEL);
	if (!tof_ctrl->io_master_info.cci_client)
		goto free_tof_ctrl;

	rc = stmvl53l1_get_dt_info(&pdev->dev, tof_ctrl);
	if (rc < 0) {
		vl53l1_errmsg("%d, failed to get dt info rc %d\n", __LINE__, rc);
		goto free_cci_client;
	}
	rc = msm_camera_pinctrl_init(&(tof_ctrl->pinctrl_info), &tof_ctrl->pdev->dev);

	if (rc < 0) {
		/* Some sensor subdev no pinctrl. */
		vl53l1_errmsg("Initialization of pinctrl failed");
		tof_ctrl->cam_pinctrl_status = 0;
		goto free_cci_client;
	} else {
		tof_ctrl->cam_pinctrl_status = 1;
		vl53l1_errmsg("Initialization of pinctrl succeed");
	}

	if (tof_ctrl->cam_pinctrl_status) {
		rc = pinctrl_select_state(
			tof_ctrl->pinctrl_info.pinctrl,
			tof_ctrl->pinctrl_info.gpio_state_active);
		if (rc)
			vl53l1_errmsg("cannot set pin to active state");
	}

	cci_client = tof_ctrl->io_master_info.cci_client;
	cci_client->cci_i2c_master = tof_ctrl->cci_master;
	cci_client->sid = 0x29;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->i2c_freq_mode = I2C_FAST_MODE;

	tof_ctrl->v4l2_dev_str.internal_ops = &msm_tof_internal_ops;
	tof_ctrl->v4l2_dev_str.ops = &msm_tof_subdev_ops;
	strlcpy(tof_ctrl->device_name, TOF_SENSOR_NAME,
		sizeof(tof_ctrl->device_name));
	tof_ctrl->v4l2_dev_str.name = tof_ctrl->device_name;
	tof_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	tof_ctrl->v4l2_dev_str.ent_function = TOF_DEVICE_TYPE;
	tof_ctrl->v4l2_dev_str.token = tof_ctrl;

	rc = cam_register_subdev(&(tof_ctrl->v4l2_dev_str));
	if (rc) {
		vl53l1_errmsg("fail to create subdev");
		goto unregister_subdev;
	}

	g_v4l2_dev_str = &tof_ctrl->v4l2_dev_str;

	/* setup device data */
	dev_set_drvdata(&pdev->dev, vl53l1_data);

	/* setup other stuff */
	rc = stmvl53l1_setup(vl53l1_data);
	if (rc) {
		vl53l1_errmsg("fail to setup stmvl53l1");
		goto release_gpios;
	}
	vl53l1_data->sysfs_base = cci_client->sid;
	rc = stmvl53l1_sysfs_laser(vl53l1_data, true);
	if (rc) {
		vl53l1_errmsg("fail to create sysfs laser node");
		goto release_gpios;
	}
	kref_init(&tof_ctrl->ref);

	vl53l1_errmsg("End = %d\n", rc);

	return rc;

release_gpios:
	stmvl53l1_release_gpios_cci(tof_ctrl);
unregister_subdev:
	cam_unregister_subdev(&(tof_ctrl->v4l2_dev_str));
free_cci_client:
	kfree(tof_ctrl->io_master_info.cci_client);
free_tof_ctrl:
	kfree(tof_ctrl);
free_vl53l1_data:
	kfree(vl53l1_data);
	return rc;
}

static int32_t stmvl53l1_platform_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct stmvl53l1_data *vl53l1_data = platform_get_drvdata(pdev);
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)vl53l1_data->client_object;

	mutex_lock(&vl53l1_data->work_mutex);
	stmvl53l1_sysfs_laser(vl53l1_data, false);
	/* main driver cleanup */
	stmvl53l1_clean_up_cci();

	if (tof_ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
				tof_ctrl->pinctrl_info.pinctrl,
				tof_ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			vl53l1_errmsg("cannot set pin to suspend state");

		devm_pinctrl_put(tof_ctrl->pinctrl_info.pinctrl);
	}

	/* release gpios */
	stmvl53l1_release_gpios_cci(tof_ctrl);

	platform_set_drvdata(pdev, NULL);

	mutex_unlock(&vl53l1_data->work_mutex);

	kfree(vl53l1_data->client_object);
	kfree(vl53l1_data);

	return 0;
}

static const struct of_device_id st_stmvl53l1_dt_match[] = {
	{.compatible = "st,stmvl53l1",},
	{},
};

static struct platform_driver stmvl53l1_platform_driver = {
	.probe = stmvl53l1_platform_probe,
	.remove = stmvl53l1_platform_remove,
	.driver = {
		   .name = STMVL53l1_DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = st_stmvl53l1_dt_match,
		   },
};

int stmvl53l1_power_up_cci(void *object)
{
	int rc = 0;
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)object;

	vl53l1_dbgmsg("Enter");

	if (!tof_ctrl) {
		vl53l1_errmsg("stmvl53l1_power_up_cci failed %d\n", __LINE__);
		return -EINVAL;
	}


	/* turn on power */
	if (tof_ctrl->power_supply) {
		rc = cam_soc_util_regulator_enable(tof_ctrl->power_supply, "laser", 2800000, 2800000, 80000, 0);
		rc |= regulator_enable(tof_ctrl->cci_supply);
		if (rc) {
			vl53l1_errmsg("fail to turn on regulator");
			return rc;
		}
	} else if (tof_ctrl->pwren_gpio != -1) {
		gpio_set_value_cansleep(tof_ctrl->pwren_gpio, 1);
		vl53l1_info("slow power on");
	} else
		vl53l1_wanrmsg("no power control");

	rc = camera_io_init(&tof_ctrl->io_master_info);
	if (rc < 0)
		vl53l1_errmsg("cci init failed: rc: %d", rc);

	vl53l1_dbgmsg("End\n");

	return rc;
}

int stmvl53l1_power_down_cci(void *cci_object)
{
	int rc = 0;
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)cci_object;

	if (!tof_ctrl) {
		vl53l1_errmsg("stmvl53l1_power_down_cci failed %d\n", __LINE__);
		return -EINVAL;
	}

	vl53l1_dbgmsg("Enter\n");

	/* turn off power */
	if (tof_ctrl->power_supply) {
		rc = cam_soc_util_regulator_disable(tof_ctrl->power_supply, "laser", 2800000, 2800000, 80000, 0);
		rc = regulator_disable(tof_ctrl->cci_supply);
		if (rc)
			vl53l1_errmsg("reg disable failed. rc=%d\n",
				rc);
	} else if (tof_ctrl->pwren_gpio != -1) {
		gpio_set_value_cansleep(tof_ctrl->pwren_gpio, 0);
	}

	camera_io_release(&tof_ctrl->io_master_info);

	vl53l1_dbgmsg("power off");

	return rc;
}

void stmvl53l1_clean_up_cci(void)
{
	int rc = 0;
	rc = cam_unregister_subdev(g_v4l2_dev_str);
}

static irqreturn_t stmvl53l1_irq_handler_cci(int irq, void *object)
{
	int rc = 0;
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)object;

	if (!tof_ctrl) {
		vl53l1_errmsg("Invalid parameter of intr function = %d\n", rc);
		return -EINVAL;
	}

	if (tof_ctrl->irq == irq) {
		stmvl53l1_intr_handler(tof_ctrl->vl53l1_data);
	}

	return IRQ_HANDLED;
}


int stmvl53l1_start_intr_cci(void *object, int *poll_mode)
{
	int rc = 0;
	struct tof_ctrl_t *tof_ctrl  = NULL;

	if (!object || !poll_mode) {
		vl53l1_errmsg("Invalid parameter in intr function = %d\n", rc);
	}

	tof_ctrl = (struct tof_ctrl_t *)object;
	/* irq and gpio acquire config done in parse_tree */
	if (tof_ctrl->irq == 0) {
		/* the i2c tree as no intr force polling mode */
		*poll_mode = 1;
		return 0;
	}
	/* if started do no nothing */
	if (tof_ctrl->io_flag.intr_started) {
		*poll_mode = 0;
		return 0;
	}

	vl53l1_dbgmsg("to register_irq:%d\n", tof_ctrl->irq);
	rc = request_threaded_irq(tof_ctrl->irq, NULL,
					stmvl53l1_irq_handler_cci,
					IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
					"vl53l1_interrupt",
					(void *)tof_ctrl);
	if (rc) {
		vl53l1_errmsg("fail to req threaded irq rc = %d\n", rc);
		*poll_mode = 1;
	} else {
		vl53l1_errmsg("irq %d now handled \n", tof_ctrl->irq);
		tof_ctrl->io_flag.intr_started = 1;
		*poll_mode = 0;
	}
	return rc;
}


void *stmvl53l1_get_cci(void *object)
{
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)object;
	kref_get(&tof_ctrl->ref);

	return object;
}

static void memory_release_cci(struct kref *kref)
{
	struct tof_ctrl_t *tof_ctrl = container_of(kref, struct tof_ctrl_t, ref);

	vl53l1_dbgmsg("Enter\n");
	kfree(tof_ctrl->vl53l1_data);
	kfree(tof_ctrl);

	vl53l1_dbgmsg("memory_release\n");
}

void stmvl53l1_put_cci(void *object)
{
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)object;

	kref_put(&tof_ctrl->ref, memory_release_cci);
}

/*
static void stmvl53l1_work_handler_cci(struct work_struct *work)
{
	int rc = 0;
	struct tof_ctrl_t *data;
	vl53l1_errmsg("stmvl53l1_work_handler_cci %d", rc);

	data = container_of(work, struct tof_ctrl_t, dwork.work);

	data->vl53l1_data->is_delay_allowed = true;
	rc = VL53L1_WaitDeviceBooted(&data->vl53l1_data->stdev);
	data->vl53l1_data->is_delay_allowed = false;

	// re-sched ourself
	if (rc )
		schedule_delayed_work(&data->dwork, msecs_to_jiffies(5000));
}
*/
int stmvl53l1_reset_release_cci(void *object)
{
	int rc = 0;
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *) object;

	vl53l1_dbgmsg("Enter\n");

	gpio_set_value_cansleep(tof_ctrl->xsdn_gpio, 1);

	/* and now wait for device end of boot */
	tof_ctrl->vl53l1_data->is_delay_allowed = true;
	rc = VL53L1_WaitDeviceBooted(&tof_ctrl->vl53l1_data->stdev);
	tof_ctrl->vl53l1_data->is_delay_allowed = false;
	if (rc) {
		//INIT_DELAYED_WORK(&tof_ctrl->dwork, stmvl53l1_work_handler_cci);
		gpio_set_value_cansleep(tof_ctrl->xsdn_gpio, 0);
		vl53l1_errmsg("boot fail with error %d", rc);
		tof_ctrl->vl53l1_data->last_error = rc;	
		//schedule_delayed_work(&tof_ctrl->dwork, msecs_to_jiffies(5000));
		rc = -EIO;
	}
	vl53l1_errmsg("stmvl53l3 probe status, 0 means successful rc = %d", rc);
	return rc;
}


int stmvl53l1_reset_hold_cci(void *object)
{
	int rc = 0;
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *) object;

	if (!tof_ctrl) {
		vl53l1_errmsg("Invalid parameter = %d\n", rc);
	}

	gpio_set_value_cansleep(tof_ctrl->xsdn_gpio, 0);

	vl53l1_dbgmsg("End\n");

	return 0;
}


int stmvl53l1_init_cci(void)
{
	int ret = 0;

	vl53l1_errmsg("Enter\n");

	/* register as a platform device */
	ret = platform_driver_register(&stmvl53l1_platform_driver);
	if (ret)
		vl53l1_errmsg("%d, error ret:%d\n", __LINE__, ret);

	vl53l1_dbgmsg("End\n");

	return ret;
}

void stmvl53l1_exit_cci(void *object)
{
	struct tof_ctrl_t *tof_ctrl = (struct tof_ctrl_t *)object;

	vl53l1_dbgmsg("Enter\n");

	if (tof_ctrl && tof_ctrl->io_master_info.cci_client)
		kfree(tof_ctrl->io_master_info.cci_client);

	vl53l1_dbgmsg("End\n");
}
#endif				/* end of CAMERA_CCI */
