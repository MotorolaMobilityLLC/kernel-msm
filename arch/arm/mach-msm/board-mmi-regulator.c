/* Copyright (c) 2011-2012, Motorola Mobility. All rights reserved.
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
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <linux/bootmem.h>
#include <linux/of_fdt.h>
#include <linux/of.h>

#include <mach/devtree_util.h>
#include "board-8960.h"
#include "board-mmi.h"
#include "devices-mmi.h"

#define QUALCOMM_GPIO_REGULATOR    0x001e0017
#define QUALCOMM_SAW_REGULATOR     0x001e0018
#define QUALCOMM_PM8921_REGULATOR  0x001e0019
#define QUALCOMM_RPM_REGULATOR     0x001e001A

static inline void dbg_consumer_supply_dump(
	const struct regulator_init_data *init_data)
{
#ifdef DEBUG
	int i;

	pr_info("%s init_data.num_consumer_supplies = %d\n",
		__func__, init_data->num_consumer_supplies);
	for (i = 0; i < init_data->num_consumer_supplies; i++) {
		pr_info("%s \t%20s %20s\n", __func__,
			init_data->consumer_supplies[i].dev_name,
			init_data->consumer_supplies[i].supply);
	}
#endif
}

static struct regulator_consumer_supply * __init
consumer_supply_realloc(const struct regulator_init_data *init_data,
		unsigned int extra)
{
	struct regulator_consumer_supply *consumer_supplies;

	consumer_supplies = kmalloc((init_data->num_consumer_supplies + extra) *
			sizeof(struct regulator_consumer_supply), GFP_KERNEL);
	if (consumer_supplies)
		memcpy(consumer_supplies, init_data->consumer_supplies,
			init_data->num_consumer_supplies *
			sizeof(struct regulator_consumer_supply));

	return consumer_supplies;
}

static struct regulator_init_data * __init
mmi_gpio_regmap_lookup(u32 regulator_id)
{
	int i;
	const char *regulator_name;
	/* maps MMI device tree constants to strings */
	static const char * const mmi_gpio_reg_mapping[] = {
		[0] = "ext_5v",
		[1] = "ext_l2",
		[2] = "ext_3p3v",
		[3] = "ext_otg_sw",
	};

	if (regulator_id >= ARRAY_SIZE(mmi_gpio_reg_mapping))
		return NULL;
	regulator_name = mmi_gpio_reg_mapping[regulator_id];
	for (i = 0; i < msm_gpio_regulator_pdata_len; i++) {
		if (!strcmp(msm_gpio_regulator_pdata[i].regulator_name,
			regulator_name))
			return &msm_gpio_regulator_pdata[i].init_data;
	}

	return NULL;
}

/* We are going to have to go through the array of regulators a
 * couple of times, so build a map of regulator ids to their
 * array index position to map the enumerated values as
 * defined in rpm_vreg_id_8960 to their location in the
 * msm_rpm_regulator_init_data array
 */
static int mmi_rpm_regmap[RPM_VREG_ID_PM8921_MAX] __initdata = {
	 [0 ... RPM_VREG_ID_PM8921_MAX - 1] = -1
};

static struct regulator_init_data * __init
mmi_pm8921_regmap_lookup(u32 regulator_id)
{
	int i;
	const char *regulator_name = NULL;
	/* maps mmi devtree schema's ids to regulator names */
	static const struct {
		int reg_id;
		const char *reg_name;
	} mmi_pm8921_reg_mapping[] = {
		[0] = {0x00000017, "8921_l26"},
		[1] = {0x00000018, "8921_l27"},
		[2] = {0x00000019, "8921_l28"},
		[3] = {0x0000001A, "8921_l29"},
		[4] = {0x0000002A, "8921_usb_otg"},
		[5] = {0x0000002B, "8921_hdmi_mvs"},
	};

	for (i = 0; i < sizeof(mmi_pm8921_reg_mapping)/
			sizeof(mmi_pm8921_reg_mapping[0]); i++)
		if (mmi_pm8921_reg_mapping[i].reg_id == regulator_id) {
			regulator_name = mmi_pm8921_reg_mapping[i].reg_name;
			break;
		}

	if (!regulator_name)
		return NULL;

	for (i = 0; i < msm_pm8921_regulator_pdata_len; i++) {
		struct regulator_init_data *init_data;
		init_data = &msm_pm8921_regulator_pdata[i].init_data;
		if (!strcmp(init_data->constraints.name,
			regulator_name))
			return init_data;
	}

	return NULL;
}

static void __init mmi_reg_map_initialize(
	struct rpm_regulator_init_data reg_init_data[],
	size_t reg_init_size)
{
	struct rpm_regulator_init_data *rpm_data;
	int i;

	for (i = 0; i < reg_init_size; i++) {
		rpm_data = &reg_init_data[i];
		pr_debug("%s regulator_id = %4d regulator_ndx = %4d\n",
			__func__, rpm_data->id, i);
		if (rpm_data->id < RPM_VREG_ID_PM8921_MAX)
			mmi_rpm_regmap[rpm_data->id] = i;
	}
}

static struct regulator_init_data * __init
mmi_rpm_regmap_lookup(u32 regulator_id)
{
	int regulator_ndx;
	struct rpm_regulator_init_data *rpm_data = NULL;

	if (regulator_id < RPM_VREG_ID_PM8921_MAX) {
		regulator_ndx = mmi_rpm_regmap[regulator_id];
		pr_debug("%s regulator_ndx = %d\n", __func__,
			 mmi_rpm_regmap[regulator_id]);

		if (regulator_ndx < 0)
			pr_err("bad array index\n");

		else
			rpm_data = &msm_rpm_regulator_init_data[regulator_ndx];
	}
	return rpm_data ? &rpm_data->init_data : NULL;
}

static void __init dt_regulator_init(struct device_node *reg_parent_node,
	struct regulator_init_data *(lookup)(u32))
{
	struct device_node *reg_child_node;
	struct device_node *consumer_supply_node;
	struct regulator_init_data *init_data;
	u32 reg_id;
	int num_cs; /* number of consumer_supplies */
	int num_added; /* number of consumer_supplies added in devtree */
	struct regulator_consumer_supply *cs; /* our new table */

	for_each_child_of_node(reg_parent_node, reg_child_node) {
		if (of_property_read_u32(reg_child_node, "id",  &reg_id))
			continue;

		pr_debug("%s %s regulator_id = %d\n", __func__,
			reg_child_node->full_name, reg_id);

		init_data = (*lookup)(reg_id);
		if (!init_data) {
			pr_err("%s: invalid regulator for id = %x\n",
			       __func__, reg_id);
			continue;
		}

		num_cs = init_data->num_consumer_supplies;
		num_added = dt_children_count(reg_child_node);
		if (num_added == 0)
			continue;

		cs = consumer_supply_realloc(init_data, num_added);
		if (!cs) {
			pr_err("%s: error allocating memory for regulator %s\n",
				__func__, reg_child_node->full_name);
			continue;
		}

		for_each_child_of_node(reg_child_node, consumer_supply_node) {
			if (of_property_read_string(consumer_supply_node,
				"supply", &cs[num_cs].supply)) {
				pr_err("%s failed to read supply for %s\n",
					__func__, reg_child_node->full_name);
				continue;
			}
			if (of_property_read_string(consumer_supply_node,
				"dev_name", &cs[num_cs].dev_name)) {
				pr_err("%s failed to read dev_name for %s\n",
					__func__, reg_child_node->full_name);
				continue;
			}
			num_cs++;
		}

		if (num_cs == init_data->num_consumer_supplies) {
			pr_err("%s: no nodes successfully added for %s\n",
				__func__, reg_child_node->full_name);
			kfree(cs);
			continue;
		}

		init_data->consumer_supplies = cs;
		init_data->num_consumer_supplies = num_cs;
		dbg_consumer_supply_dump(init_data);
	}
}

void __init mmi_regulator_init(struct msm8960_oem_init_ptrs *self)
{
	struct device_node *reg_parent_node;
	struct device_node *reg_chip_node;
	u32 device_id;

	reg_parent_node = of_find_node_by_path("/System@0/Regulators@0");
	if (!reg_parent_node)
		goto register_device;

	mmi_reg_map_initialize(msm_rpm_regulator_init_data,
		msm_rpm_regulator_init_data_len);

	for_each_child_of_node(reg_parent_node, reg_chip_node) {
		if (of_property_read_u32(reg_chip_node, "type", &device_id))
			continue;
		switch (device_id) {
		case QUALCOMM_RPM_REGULATOR:
			dt_regulator_init(reg_chip_node,
				mmi_rpm_regmap_lookup);
			break;
		case QUALCOMM_GPIO_REGULATOR:
			dt_regulator_init(reg_chip_node,
				mmi_gpio_regmap_lookup);
			break;
		case QUALCOMM_PM8921_REGULATOR:
			dt_regulator_init(reg_chip_node,
				mmi_pm8921_regmap_lookup);
			break;
		case QUALCOMM_SAW_REGULATOR:
		default:
			pr_err("%s ignoring unsupported regulator " \
				"chip\n", __func__);
			break;
		}
	}

	of_node_put(reg_parent_node);

register_device:
	/* register device after device tree is handled */
	platform_device_register(&mmi_device_ext_5v_vreg);
}
