/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

struct qcom_xr_smrtvwr {
	struct device	*dev;
};


static int qcom_xr_smrtvwr_probe(struct platform_device *pdev)
{
	int rc;
	struct regulator *reg1, *reg2, *reg3;
	int dp3p3_en_gpio = 142;
	int wcd_en_gpio = 93;
	int rgb_tck_oe_en_gpio = 108;

	reg1 = devm_regulator_get(&pdev->dev, "pm660l_l6");
	if (!IS_ERR(reg1)) {
		regulator_set_load(reg1, 600000);
		rc = regulator_enable(reg1);
		if (rc < 0) {
			pr_err("%s pm660l_l6 failed\n", __func__);
			goto reg1_fail;
		}
	}

	reg2 = devm_regulator_get(&pdev->dev, "pm660_l6");
	if (!IS_ERR(reg2)) {
		regulator_set_load(reg2, 600000);
		rc = regulator_enable(reg2);
		if (rc < 0) {
			pr_err("%s pm660_l6 failed\n", __func__);
			goto reg2_fail;
		}
	}

	reg3 = devm_regulator_get(&pdev->dev, "pm660_l7");
	if (!IS_ERR(reg3)) {
		regulator_set_load(reg3, 600000);
		rc = regulator_enable(reg3);
		if (rc < 0) {
			pr_err("%s pm660_l7 failed\n", __func__);
			goto reg3_fail;
		}
	}

	rc = gpio_request(dp3p3_en_gpio, "ti-dp-3v3-en-gpio");
	if (rc) {
		pr_err("%s dp3p3_en gpio request failed\n", __func__);
		goto gpio3p3_fail;
	}
	rc = gpio_direction_output(dp3p3_en_gpio, 0);
	if (rc) {
		pr_err("%s dp3p3_en_gpio direction failed\n", __func__);
		goto gpio3p3_fail;
	}
	gpio_set_value(dp3p3_en_gpio, 1);
	msleep(20);

	rc = gpio_request(wcd_en_gpio, "wcd9340_en_gpio");
	if (rc) {
		pr_err("%s wcd9340_en_gpio request failed\n", __func__);
		goto gpiowcd_fail;
	}
	rc = gpio_direction_output(wcd_en_gpio, 0);
	if (rc) {
		pr_err("%s wcd9340_en_gpio direction failed\n", __func__);
		goto gpiowcd_fail;
	}
	gpio_set_value(wcd_en_gpio, 1);
	msleep(20);

	rc = gpio_request(rgb_tck_oe_en_gpio, "rgb_tck_oe_en_gpio");
	if (rc) {
		pr_err("%s rgb_tck_oe_en_gpio request failed\n", __func__);
		goto gpio_oe_en_fail;
	}
	rc = gpio_direction_output(rgb_tck_oe_en_gpio, 0);
	if (rc) {
		pr_err("%s rgb_tck_oe_en_gpio direction failed\n", __func__);
		goto gpio_oe_en_fail;
	}
	gpio_set_value(rgb_tck_oe_en_gpio, 0);
	msleep(20);

	pr_debug("%s success\n", __func__);
	return 0;

gpio_oe_en_fail:
	gpio_free(rgb_tck_oe_en_gpio);
gpiowcd_fail:
	gpio_free(wcd_en_gpio);
gpio3p3_fail:
	gpio_free(dp3p3_en_gpio);
reg3_fail:
	devm_regulator_put(reg3);
reg2_fail:
	devm_regulator_put(reg2);
reg1_fail:
	devm_regulator_put(reg1);

	return rc;
}

static const struct of_device_id qcom_xr_smrtvwr_match_table[] = {
	{ .compatible = "qcom,xr-smrtvwr-misc", },
	{}
};

MODULE_DEVICE_TABLE(of, qcom_xr_smrtvwr_match_table);

static struct platform_driver qcom_xr_smrtvwr_driver = {
	.driver	= {
		.name		= "qcom-xr-smrtvwr-misc",
		.of_match_table	= qcom_xr_smrtvwr_match_table,
	},
	.probe		= qcom_xr_smrtvwr_probe,
};

module_platform_driver(qcom_xr_smrtvwr_driver);

MODULE_DESCRIPTION("QTI XR SMRTVWR MISC driver");
MODULE_LICENSE("GPL v2");
