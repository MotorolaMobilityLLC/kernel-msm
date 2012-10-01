/* Copyright (c) 2012, Motorola Mobility, Inc
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
#include <asm/mach/map.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/w1-gpio.h>
#include <mach/devtree_util.h>
#include "board-8960.h"
#include "devices-mmi.h"

struct pm8xxx_gpio_init {
	unsigned			gpio;
	struct pm_gpio			config;
};

struct pm8xxx_mpp_init {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8XXX_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
			_func, _inv, _disable) \
{ \
	.gpio	= PM8921_GPIO_PM_TO_SYS(_gpio), \
	.config	= { \
		.direction	= _dir, \
		.output_buffer	= _buf, \
		.output_value	= _val, \
		.pull		= _pull, \
		.vin_sel	= _vin, \
		.out_strength	= _out_strength, \
		.function	= _func, \
		.inv_int_pol	= _inv, \
		.disable_pin	= _disable, \
	} \
}

#define PM8XXX_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8921_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8XXX_GPIO_DISABLE(_gpio) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
			 0, 0, 0, 1)

#define PM8XXX_GPIO_OUTPUT(_gpio, _val) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_INPUT(_gpio, _pull) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8XXX_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_OUTPUT_STRENGTH(_gpio, _val, _out_strength) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			_out_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8XXX_GPIO_PAIRED_IN_VIN(_gpio, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			PM_GPIO_PULL_UP_1P5, _vin, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_PAIRED, 0, 0)

#define PM8XXX_GPIO_PAIRED_OUT_VIN(_gpio, _vin) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, \
			PM_GPIO_PULL_UP_1P5, _vin, \
			PM_GPIO_STRENGTH_MED, \
			PM_GPIO_FUNC_PAIRED, 0, 0)

static struct pm8xxx_mpp_init mmi_pm8921_mpps[] __initdata = {
	PM8XXX_MPP_INIT(7, D_OUTPUT, PM8921_MPP_DIG_LEVEL_S4, DOUT_CTRL_HIGH),
	PM8XXX_MPP_INIT(8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8, DOUT_CTRL_LOW),
	PM8XXX_MPP_INIT(11, D_BI_DIR, PM8921_MPP_DIG_LEVEL_S4, BI_PULLUP_1KOHM),
	PM8XXX_MPP_INIT(12, D_BI_DIR, PM8921_MPP_DIG_LEVEL_L17, BI_PULLUP_OPEN),
};

static __init int load_pm8921_mpps_from_dt(struct pm8xxx_mpp_init **ptr,
			unsigned int *entries)
{
	*ptr = mmi_pm8921_mpps;
	*entries = ARRAY_SIZE(mmi_pm8921_mpps);
	return 0;
}

static __init int load_pm8921_gpios_from_dt(struct pm8xxx_gpio_init **ptr,
			unsigned int *entries)
{
	struct device_node *parent, *child;
	int count = 0, index = 0;
	struct pm8xxx_gpio_init *pm8921_gpios;
	int ret = -EINVAL;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent)
		goto out;

	/* count the child GPIO nodes */
	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *prop;

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32))) {
			/* must match type identifiers defined in DT schema */
			switch (*(u32 *)prop) {
			case 0x001E0006: /* Disable */
			case 0x001E0007: /* Output */
			case 0x001E0008: /* Input */
			case 0x001E0009: /* Output, Func */
			case 0x001E000A: /* Output, Vin */
			case 0x001E000B: /* Paired Input, Vin */
			case 0x001E000C: /* Paired Output, Vin */
				count++;
				break;
			}
		}
	}

	/* if no GPIO entries were found, just use the defaults */
	if (!count)
		goto out;

	/* allocate the space */
	pm8921_gpios = kmalloc(sizeof(struct pm8xxx_gpio_init) * count,
			GFP_KERNEL);
	*entries = count;
	*ptr = pm8921_gpios;

	/* fill out the array */
	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *type_prop;

		type_prop = of_get_property(child, "type", &len);
		if (type_prop && (len == sizeof(u32))) {
			const void *gpio_prop;
			u16 gpio;

			gpio_prop = of_get_property(child, "gpio", &len);
			if (!gpio_prop || (len != sizeof(u16)))
				continue;

			gpio = *(u16 *)gpio_prop;

			/* must match type identifiers defined in DT schema */
			switch (*(u32 *)type_prop) {
			case 0x001E0006: /* Disable */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_DISABLE(gpio);
				break;

			case 0x001E0007: /* Output */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT(gpio,
						dt_get_u16_or_die(child,
							"value"));
				break;

			case 0x001E0008: /* Input */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_INPUT(gpio,
						dt_get_u8_or_die(child,
							"pull"));
				break;

			case 0x001E0009: /* Output, Func */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT_FUNC(
						gpio, dt_get_u16_or_die(child,
							"value"),
						dt_get_u8_or_die(child,
							"func"));
				break;

			case 0x001E000A: /* Output, Vin */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT_VIN(
						gpio, dt_get_u16_or_die(child,
							"value"),
						dt_get_u8_or_die(child,
							"vin"));
				break;

			case 0x001E000B: /* Paired Input, Vin */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_PAIRED_IN_VIN(
						gpio, dt_get_u8_or_die(child,
							"vin"));
				break;

			case 0x001E000C: /* Paired Output, Vin */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_PAIRED_OUT_VIN(
						gpio, dt_get_u8_or_die(child,
							"vin"));
				break;
			}
		}
	}

	BUG_ON(index != count);

	of_node_put(parent);
	ret = 0;
out:
	return ret;
}

void __init mmi_init_pm8921_gpio_mpp(void)
{
	int i;
	unsigned int size;
	struct pm8xxx_gpio_init *gpios = NULL;
	struct pm8xxx_mpp_init *mpps = NULL;

	if (load_pm8921_gpios_from_dt(&gpios, &size)) {
		size = 0;
		pr_err("%s: error initializing pm8921 GPIOS\n", __func__);
	}

	for (i = 0; i < size; i++) {
		int rc = pm8xxx_gpio_config(gpios[i].gpio, &gpios[i].config);
		if (rc)
			pr_err("%s: pm config GPIO%2d: rc=%d\n", __func__,
				gpios[i].gpio, rc);
	}

	if (load_pm8921_mpps_from_dt(&mpps, &size)) {
		size = 0;
		pr_err("%s: error initializing pm8921 MPPS\n", __func__);
	}

	for (i = 0; i < size; i++) {
		int rc = pm8xxx_mpp_config(mpps[i].mpp, &mpps[i].config);
		if (rc)
			pr_err("%s: pm config MPP%2d: rc=%d\n", __func__,
				mpps[i].mpp, rc);
	}
}

void w1_gpio_enable_regulators(int enable)
{
	static struct regulator *vdd1;
	int rc;

	if (!vdd1) {
		vdd1 = regulator_get(&mmi_w1_gpio_device.dev, "8921_l17");
		if (IS_ERR_OR_NULL(vdd1)) {
			pr_err("w1_gpio failed to get 8921_l17\n");
			return;
		}
	}

	if (enable) {
		if (!IS_ERR_OR_NULL(vdd1)) {
			rc = regulator_set_voltage(vdd1,
						2850000, 2850000);
			if (!rc) {
				rc = regulator_enable(vdd1);
			}
			if (rc)
				pr_err("w1_gpio failed to set voltage "\
								"8921_l17\n");
		}
	} else {
		if (!IS_ERR_OR_NULL(vdd1)) {
			rc = regulator_disable(vdd1);
			if (rc)
				pr_err("w1_gpio unable to disable 8921_l17\n");
		}
	}
}

static __init void load_pm8921_batt_eprom_pdata_from_dt(void)
{
	struct device_node *chosen;
	int len = 0;
	const void *prop;
	struct w1_gpio_platform_data *pdata =
		((struct w1_gpio_platform_data *)
		 (mmi_w1_gpio_device.dev.platform_data));

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "batt_eprom_gpio", &len);
	if (prop && (len == sizeof(u8))) {
		pdata->pin =  *(u8 *)prop;
		pdata->enable_external_pullup = w1_gpio_enable_regulators;
	}

	of_node_put(chosen);

out:
	if (pdata->pin < 0) {
		pr_err("w1_gpio unable to get GPIO from devtree!\n");
	}
	pr_info("w1_gpio = %d\n",pdata->pin);
	return;
}

void __init mmi_pm8921_init(void *pdata)
{
	load_pm8921_batt_eprom_pdata_from_dt();
}
