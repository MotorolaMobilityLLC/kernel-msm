/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/err.h>
#include <linux/module.h>
#include <linux/msm-bus.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define DDR_CDEV_NAME "ddr-bus-scaling-cdev"

struct ddr_cdev {
	uint32_t cur_state;
	uint32_t max_state;
	uint32_t bus_clnt_handle;
	struct thermal_cooling_device *cdev;
	struct msm_bus_scale_pdata *ddr_bus_pdata;
	struct device *dev;
};

/**
 * ddr_set_cur_state - callback function to set the ddr state.
 * @cdev: thermal cooling device pointer.
 * @state: set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the
 * DDR state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int ddr_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct ddr_cdev *ddr_cdev = cdev->devdata;
	int ret = 0;

	/* Request state should be less than max_level */
	if (state > ddr_cdev->max_state)
		return -EINVAL;

	/* Check if the old cooling action is same as new cooling action */
	if (ddr_cdev->cur_state == state)
		return 0;

	ret = msm_bus_scale_client_update_request(ddr_cdev->bus_clnt_handle,
							state);
	if (ret) {
		dev_err(ddr_cdev->dev,
			"Error placing DDR freq for idx:%d. err:%d\n",
				state, ret);
		return ret;
	}
	ddr_cdev->cur_state = state;

	dev_dbg(ddr_cdev->dev, "Requested DDR bus scaling state:%ld for %s\n",
			state, ddr_cdev->cdev->type);

	return ret;
}

/**
 * ddr_get_cur_state - callback function to get the current cooling
 *				state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the
 * current DDR state request.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int ddr_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct ddr_cdev *ddr_cdev = cdev->devdata;
	*state = ddr_cdev->cur_state;

	return 0;
}

/**
 * ddr_get_max_state - callback function to get the max cooling state.
 * @cdev: thermal cooling device pointer.
 * @state: fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the DDR
 * max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int ddr_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct ddr_cdev *ddr_cdev = cdev->devdata;
	*state = ddr_cdev->max_state;

	return 0;
}

static struct thermal_cooling_device_ops ddr_cdev_ops = {
	.get_max_state = ddr_get_max_state,
	.get_cur_state = ddr_get_cur_state,
	.set_cur_state = ddr_set_cur_state,
};

static int ddr_cdev_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct ddr_cdev *ddr_cdev = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	char cdev_name[THERMAL_NAME_LENGTH] = "ddr-cdev";

	ddr_cdev = devm_kzalloc(dev, sizeof(*ddr_cdev), GFP_KERNEL);
	if (!ddr_cdev)
		return -ENOMEM;

	ddr_cdev->ddr_bus_pdata = msm_bus_cl_get_pdata(pdev);
	if (!ddr_cdev->ddr_bus_pdata) {
		ret = -EINVAL;
		dev_err(dev, "Unable to get ddr cdev bus pdata: %d\n");
		return ret;
	}

	if (ddr_cdev->ddr_bus_pdata->num_usecases < 2) {
		dev_err(dev, "Invalid number of ddr cdev levels, num_path:%d\n",
			ddr_cdev->ddr_bus_pdata->num_usecases);
		goto err_exit;
	}

	ddr_cdev->bus_clnt_handle = msm_bus_scale_register_client(
						ddr_cdev->ddr_bus_pdata);
	if (!ddr_cdev->bus_clnt_handle) {
		dev_err(dev, "%s: Failed to register ddr cdev client",
				__func__);
		ret = -ENXIO;
		goto err_exit;
	}

	ddr_cdev->cur_state = 0;
	/* max_state is index */
	ddr_cdev->max_state = ddr_cdev->ddr_bus_pdata->num_usecases - 1;
	ddr_cdev->dev = dev;

	/* Set initial value */
	ret = msm_bus_scale_client_update_request(ddr_cdev->bus_clnt_handle,
							ddr_cdev->cur_state);
	if (ret) {
		dev_err(dev, "Error placing DDR freq request, index:%d err:%d\n",
			ddr_cdev->cur_state, ret);
		goto err_exit;
	}

	ddr_cdev->cdev = thermal_of_cooling_device_register(np, cdev_name,
					ddr_cdev, &ddr_cdev_ops);
	if (IS_ERR(ddr_cdev->cdev)) {
		ret = PTR_ERR(ddr_cdev->cdev);
		dev_err(dev, "Cdev register failed for %s, ret:%d\n",
			cdev_name, ret);
		ddr_cdev->cdev = NULL;
		goto err_exit;
	}
	dev_dbg(dev, "Cooling device [%s] registered.\n", cdev_name);
	dev_set_drvdata(dev, ddr_cdev);

	return 0;
err_exit:
	if (ddr_cdev->bus_clnt_handle) {
		msm_bus_scale_unregister_client(ddr_cdev->bus_clnt_handle);
		ddr_cdev->bus_clnt_handle = 0;
	}
	if (ddr_cdev->ddr_bus_pdata) {
		msm_bus_cl_clear_pdata(ddr_cdev->ddr_bus_pdata);
		ddr_cdev->ddr_bus_pdata = NULL;
	}
	return ret;
}

static int ddr_cdev_remove(struct platform_device *pdev)
{
	struct ddr_cdev *ddr_cdev =
		(struct ddr_cdev *)dev_get_drvdata(&pdev->dev);

	if (ddr_cdev->cdev) {
		thermal_cooling_device_unregister(ddr_cdev->cdev);
		ddr_cdev->cdev = NULL;
	}
	if (ddr_cdev->bus_clnt_handle) {
		msm_bus_scale_client_update_request(
					ddr_cdev->bus_clnt_handle, 0);
		msm_bus_scale_unregister_client(ddr_cdev->bus_clnt_handle);
		ddr_cdev->bus_clnt_handle = 0;
	}
	if (ddr_cdev->ddr_bus_pdata) {
		msm_bus_cl_clear_pdata(ddr_cdev->ddr_bus_pdata);
		ddr_cdev->ddr_bus_pdata = NULL;
	}

	return 0;
}

static const struct of_device_id ddr_cdev_match[] = {
	{ .compatible = "qcom,ddr-cooling-device", },
	{},
};

static struct platform_driver ddr_cdev_driver = {
	.probe		= ddr_cdev_probe,
	.remove         = ddr_cdev_remove,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = ddr_cdev_match,
	},
};
module_platform_driver(ddr_cdev_driver);
MODULE_LICENSE("GPL v2");
