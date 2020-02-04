/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <linux/of.h>
#include <linux/of_gpio.h>
#include "cam_flash_soc.h"
#include "cam_res_mgr_api.h"

static int cam_flash_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct cam_soc_gpio_data *gconf,
	uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		CAM_DBG(CAM_FLASH, "gpio-req-tbl-num 0");
		return 0;
	}

	val_array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!val_array)
		return -ENOMEM;

	gconf->cam_gpio_req_tbl = kcalloc(count, sizeof(struct gpio),
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		rc = -ENOMEM;
		goto free_val_array;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "gpio-req-tbl-num",
		val_array, count);
	if (rc) {
		CAM_ERR(CAM_FLASH,
			"failed in reading gpio-req-tbl-num, rc = %d",
			rc);
		goto free_gpio_req_tbl;
	}

	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			CAM_ERR(CAM_FLASH, "gpio req tbl index %d invalid",
				val_array[i]);
			goto free_gpio_req_tbl;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CAM_DBG(CAM_FLASH, "cam_gpio_req_tbl[%d].gpio = %d", i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "gpio-req-tbl-flags",
		val_array, count);
	if (rc) {
		CAM_ERR(CAM_FLASH, "Failed in gpio-req-tbl-flags, rc %d", rc);
		goto free_gpio_req_tbl;
	}

	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CAM_DBG(CAM_FLASH, "cam_gpio_req_tbl[%d].flags = %ld", i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		if (rc) {
			CAM_ERR(CAM_FLASH, "Failed rc %d", rc);
			goto free_gpio_req_tbl;
		}
		CAM_DBG(CAM_FLASH, "cam_gpio_req_tbl[%d].label = %s", i,
			gconf->cam_gpio_req_tbl[i].label);
	}

	kfree(val_array);

	return rc;

free_gpio_req_tbl:
	kfree(gconf->cam_gpio_req_tbl);
free_val_array:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;

	return rc;
}

static int cam_flash_get_dt_gpio_delay_tbl(
	struct device_node *of_node, struct cam_flash_private_soc *soc_private)
{
	int32_t rc = 0, i = 0;
	uint32_t *val_array = NULL;
	uint32_t count = 0;

	soc_private->gpio_delay_tbl_size = 0;

	if (!of_get_property(of_node, "gpio-req-tbl-delay", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		CAM_ERR(CAM_FLASH, "gpio-req-tbl-delay 0");
		return 0;
	}

	if (count != soc_private->gpio_data->cam_gpio_req_tbl_size) {
		CAM_ERR(CAM_FLASH,
			"Invalid number of gpio-req-tbl-delay entries: %d",
			count);
		return 0;
	}

	val_array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!val_array)
		return -ENOMEM;

	soc_private->gpio_delay_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "gpio-req-tbl-delay",
		val_array, count);
	if (rc) {
		CAM_ERR(CAM_FLASH, "Failed in gpio-req-tbl-delay, rc %d", rc);
		goto free_val_array;
	}

	soc_private->gpio_delay_tbl = val_array;

	for (i = 0; i < count; i++) {
		CAM_DBG(CAM_FLASH, "gpio_delay_tbl[%d] = %ld", i,
			soc_private->gpio_delay_tbl[i]);
	}

	return 0;

free_val_array:
	kfree(val_array);
	soc_private->gpio_delay_tbl_size = 0;
	return rc;
}

static int cam_flash_get_gpio_info(
	struct device_node *of_node,
	struct cam_flash_private_soc *soc_private)
{
	int32_t rc = 0, i = 0;
	uint16_t *gpio_array = NULL;
	int16_t gpio_array_size = 0;
	struct cam_soc_gpio_data *gconf = NULL;

	gpio_array_size = of_gpio_count(of_node);

	CAM_DBG(CAM_FLASH, "gpio count %d", gpio_array_size);
	if (gpio_array_size <= 0)
		return 0;

	gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t), GFP_KERNEL);
	if (!gpio_array)
		goto free_gpio_conf;

	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio(of_node, i);
		CAM_DBG(CAM_FLASH, "gpio_array[%d] = %d", i, gpio_array[i]);
	}

	gconf = kzalloc(sizeof(*gconf), GFP_KERNEL);
	if (!gconf)
		return -ENOMEM;

	rc = cam_flash_get_dt_gpio_req_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc) {
		CAM_ERR(CAM_FLASH, "failed in msm_camera_get_dt_gpio_req_tbl");
		goto free_gpio_array;
	}

	gconf->cam_gpio_common_tbl = kcalloc(gpio_array_size,
		sizeof(struct gpio), GFP_KERNEL);
	if (!gconf->cam_gpio_common_tbl) {
		rc = -ENOMEM;
		goto free_gpio_array;
	}

	for (i = 0; i < gpio_array_size; i++)
		gconf->cam_gpio_common_tbl[i].gpio = gpio_array[i];

	gconf->cam_gpio_common_tbl_size = gpio_array_size;
	soc_private->gpio_data = gconf;
	kfree(gpio_array);

	cam_flash_get_dt_gpio_delay_tbl(of_node, soc_private);

	return rc;

free_gpio_array:
	kfree(gpio_array);
free_gpio_conf:
	kfree(gconf);
	soc_private->gpio_data = NULL;

	return rc;
}

static int cam_flash_request_gpio_table(
	struct cam_flash_private_soc *soc_private, bool gpio_en)
{
	int rc = 0, i = 0;
	uint8_t size = 0;
	struct cam_soc_gpio_data *gpio_conf =
		soc_private->gpio_data;
	struct gpio *gpio_tbl = NULL;

	if (!gpio_conf) {
		CAM_DBG(CAM_FLASH, "No GPIO entry");
		return 0;
	}
	if (gpio_conf->cam_gpio_common_tbl_size <= 0) {
		CAM_ERR(CAM_FLASH, "GPIO table size is invalid");
		return -EINVAL;
	}
	size = gpio_conf->cam_gpio_req_tbl_size;
	gpio_tbl = gpio_conf->cam_gpio_req_tbl;

	if (!gpio_tbl || !size) {
		CAM_ERR(CAM_FLASH, "Invalid gpio_tbl %pK / size %d",
			gpio_tbl, size);
		return -EINVAL;
	}
	for (i = 0; i < size; i++) {
		CAM_DBG(CAM_FLASH,
			"cam_flash_request_gpio_table: i=%d, gpio=%d dir=%ld",
			i,
			gpio_tbl[i].gpio,
			gpio_tbl[i].flags);
	}
	if (gpio_en) {
		for (i = 0; i < size; i++) {
			rc = gpio_request_one(gpio_tbl[i].gpio,
				gpio_tbl[i].flags, gpio_tbl[i].label);
			if (rc) {
				/*
				 * After GPIO request fails, contine to
				 * apply new gpios, outout a error message
				 * for driver bringup debug
				 */
				CAM_ERR(CAM_FLASH, "gpio %d:%s request fails",
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		gpio_free_array(gpio_tbl, size);
	}

	return rc;
}

static int32_t cam_get_source_node_info(
	struct device_node *of_node,
	struct cam_flash_ctrl *fctrl,
	struct cam_flash_private_soc *soc_private)
{
	int32_t rc = 0;
	uint32_t count = 0, i = 0;
	struct device_node *flash_src_node = NULL;
	struct device_node *torch_src_node = NULL;
	struct device_node *switch_src_node = NULL;

	switch_src_node = of_parse_phandle(of_node, "switch-source", 0);
	if (!switch_src_node) {
		CAM_DBG(CAM_FLASH, "switch_src_node NULL");
	} else {
		rc = of_property_read_string(switch_src_node,
			"qcom,default-led-trigger",
			&soc_private->switch_trigger_name);
		if (rc) {
			CAM_ERR(CAM_FLASH,
				"default-led-trigger read failed rc=%d", rc);
		} else {
			CAM_DBG(CAM_FLASH, "switch trigger %s",
				soc_private->switch_trigger_name);
			cam_res_mgr_led_trigger_register(
				soc_private->switch_trigger_name,
				&fctrl->switch_trigger);
		}

		of_node_put(switch_src_node);
	}

	if (of_get_property(of_node, "flash-source", &count)) {
		count /= sizeof(uint32_t);

		if (count > CAM_FLASH_MAX_LED_TRIGGERS) {
			CAM_ERR(CAM_FLASH, "Invalid LED count: %d", count);
			return -EINVAL;
		}

		fctrl->flash_num_sources = count;

		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"flash-source", i);
			if (!flash_src_node) {
				CAM_WARN(CAM_FLASH, "flash_src_node NULL");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"qcom,default-led-trigger",
				&soc_private->flash_trigger_name[i]);
			if (rc) {
				CAM_WARN(CAM_FLASH,
				"defalut-led-trigger read failed rc=%d", rc);
				of_node_put(flash_src_node);
				continue;
			}

			CAM_DBG(CAM_FLASH, "default trigger %s",
				soc_private->flash_trigger_name[i]);

			/* Read operational-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,current-ma",
				&soc_private->flash_op_current[i]);
			if (rc) {
				CAM_WARN(CAM_FLASH, "op-current: read failed");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&soc_private->flash_max_current[i]);
			if (rc) {
				CAM_WARN(CAM_FLASH,
					"max-current: read failed");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-duration */
			rc = of_property_read_u32(flash_src_node,
				"qcom,duration-ms",
				&soc_private->flash_max_duration[i]);
			if (rc)
				CAM_WARN(CAM_FLASH,
					"max-duration: read failed");

			of_node_put(flash_src_node);

			CAM_DBG(CAM_FLASH, "max_current[%d]: %d",
				i, soc_private->flash_max_current[i]);

			cam_res_mgr_led_trigger_register(
				soc_private->flash_trigger_name[i],
				&fctrl->flash_trigger[i]);
		}
	}

	if (of_get_property(of_node, "torch-source", &count)) {
		count /= sizeof(uint32_t);
		if (count > CAM_FLASH_MAX_LED_TRIGGERS) {
			CAM_ERR(CAM_FLASH, "Invalid LED count : %d", count);
			return -EINVAL;
		}

		fctrl->torch_num_sources = count;

		CAM_DBG(CAM_FLASH, "torch_num_sources = %d",
			fctrl->torch_num_sources);
		for (i = 0; i < count; i++) {
			torch_src_node = of_parse_phandle(of_node,
				"torch-source", i);
			if (!torch_src_node) {
				CAM_WARN(CAM_FLASH, "torch_src_node NULL");
				continue;
			}

			rc = of_property_read_string(torch_src_node,
				"qcom,default-led-trigger",
				&soc_private->torch_trigger_name[i]);
			if (rc < 0) {
				CAM_WARN(CAM_FLASH,
					"default-trigger read failed");
				of_node_put(torch_src_node);
				continue;
			}

			/* Read operational-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,current-ma",
				&soc_private->torch_op_current[i]);
			if (rc < 0) {
				CAM_WARN(CAM_FLASH, "current: read failed");
				of_node_put(torch_src_node);
				continue;
			}

			/* Read max-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,max-current",
				&soc_private->torch_max_current[i]);
			if (rc < 0) {
				CAM_WARN(CAM_FLASH,
					"max-current: read failed");
				of_node_put(torch_src_node);
				continue;
			}

			of_node_put(torch_src_node);

			CAM_DBG(CAM_FLASH, "max_current[%d]: %d",
				i, soc_private->torch_max_current[i]);

			cam_res_mgr_led_trigger_register(
				soc_private->torch_trigger_name[i],
				&fctrl->torch_trigger[i]);
		}
	}

	(void) cam_flash_get_gpio_info(of_node, soc_private);
	if (soc_private->gpio_data != NULL) {
		rc = cam_flash_request_gpio_table(soc_private, true);
		if (rc < 0)
			CAM_ERR(CAM_FLASH,
			"Failed in request gpio table, rc=%d",
			rc);
	}

	return rc;
}

int cam_flash_get_dt_data(struct cam_flash_ctrl *fctrl,
	struct cam_hw_soc_info *soc_info)
{
	int32_t rc = 0;
	struct device_node *of_node = NULL;

	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "NULL flash control structure");
		return -EINVAL;
	}

	soc_info->soc_private =
		kzalloc(sizeof(struct cam_flash_private_soc), GFP_KERNEL);
	if (!soc_info->soc_private) {
		rc = -ENOMEM;
		goto release_soc_res;
	}
	of_node = fctrl->pdev->dev.of_node;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc) {
		CAM_ERR(CAM_FLASH, "Get_dt_properties failed rc %d", rc);
		goto free_soc_private;
	}

	rc = cam_get_source_node_info(of_node, fctrl, soc_info->soc_private);
	if (rc) {
		CAM_ERR(CAM_FLASH,
			"cam_flash_get_pmic_source_info failed rc %d", rc);
		goto free_soc_private;
	}
	return rc;

free_soc_private:
	cam_flash_request_gpio_table(soc_info->soc_private, false);
	kfree(soc_info->soc_private);
	soc_info->soc_private = NULL;
release_soc_res:
	cam_soc_util_release_platform_resource(soc_info);
	return rc;
}
