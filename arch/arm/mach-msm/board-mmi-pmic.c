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
#include <linux/power/mmi-battery.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/w1-gpio.h>
#include <linux/leds-pwm-gpio.h>
#include <mach/devtree_util.h>
#include <mach/mmi-battery-data.h>
#include "board-8960.h"
#include "board-mmi.h"
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

#define PM8XXX_GPIO_INPUT(_gpio, _vin,  _pull) \
	PM8XXX_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, _vin, \
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
	PM8XXX_MPP_INIT(10, D_INPUT, PM8921_MPP_DIG_LEVEL_L17, DIN_TO_INT),
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
	u32 type;
	int count = 0, index = 0;
	struct pm8xxx_gpio_init *pm8921_gpios;
	int ret = -EINVAL;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent)
		goto out;

	/* count the child GPIO nodes */
	for_each_child_of_node(parent, child) {
		if (of_property_read_u32(child, "type", &type)) {
			pr_err("%s: type property not found\n", __func__);
			continue;
		} else {
			/* must match type identifiers defined in DT schema */
			switch (type) {
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
		if (!of_property_read_u32(child, "type", &type)) {
			u32 gpio, param1, param2;

			if (of_property_read_u32(child, "gpio", &gpio)) {
				pr_err("%s: gpio property not found\n",
								__func__);
				continue;
			}

			/* must match type identifiers defined in DT schema */
			switch (type) {
			case 0x001E0006: /* Disable */
				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_DISABLE(gpio);
				break;

			case 0x001E0007: /* Output */
				if (of_property_read_u32(
					child, "value", &param1))
					break;

				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT(gpio,
						(unsigned short)param1);
				break;

			case 0x001E0008: /* Input */
				if (of_property_read_u32(
					child, "vin", &param1))
					param1 = PM_GPIO_VIN_S4;

				if (of_property_read_u32(
					child, "pull", &param2))
					break;

				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_INPUT(gpio,
						(unsigned char)param1,
						(unsigned char)param2);
				break;

			case 0x001E0009: /* Output, Func */
				if (of_property_read_u32(
					child, "value", &param1))
					break;

				if (of_property_read_u32(
					child, "func", &param2))
					break;

				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT_FUNC(gpio,
						(unsigned short)param1,
						(unsigned char)param2);
				break;

			case 0x001E000A: /* Output, Vin */
				if (of_property_read_u32(
					child, "value", &param1))
					break;

				if (of_property_read_u32(
					child, "vin", &param2))
					break;

				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_OUTPUT_VIN(gpio,
						(unsigned short)param1,
						(unsigned char)param2);
				break;

			case 0x001E000B: /* Paired Input, Vin */
				if (of_property_read_u32(
					child, "vin", &param1))
					break;

				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_PAIRED_IN_VIN(gpio,
						(unsigned char)param1);
				break;

			case 0x001E000C: /* Paired Output, Vin */
				if (of_property_read_u32(
					child, "vin", &param1))
					break;

				pm8921_gpios[index++] =
					(struct pm8xxx_gpio_init)
					PM8XXX_GPIO_PAIRED_OUT_VIN(gpio,
						(unsigned char)param1);
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

#define PM8921_LC_LED_MAX_CURRENT	4	/* I = 4mA */
#define PM8XXX_LED_PWM_PERIOD		1000
#define PM8XXX_LED_PWM_DUTY_MS		20
/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

static struct led_info pm8921_led_info[] = {
	[0] = {
		.name			= "charging",
	},
	[1] = {
		.name			= "reserved",
	},
};

static struct led_platform_data pm8921_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8921_led_info),
	.leds = pm8921_led_info,
};

static int pm8921_led0_pwm_duty_pcts[56] = {
		1, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		40, 44, 46, 52, 56, 60, 64, 68, 72, 76,
		80, 84, 88, 92, 96, 100, 100, 100, 98, 95,
		92, 88, 84, 82, 78, 74, 70, 66, 62, 58,
		58, 54, 50, 48, 42, 38, 34, 30, 26, 22,
		14, 10, 6, 4, 1
};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pwm_duty_pcts,
	.num_duty_pcts = ARRAY_SIZE(pm8921_led0_pwm_duty_pcts),
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = 0,
};

#define ATC_LED_SRC 0x216
#define ATC_LED_SRC_MASK 0x30
#ifdef CONFIG_LEDS_PM8XXX_EXT_CTRL
static void pm8xxx_atc_led_ctrl(struct device *dev, unsigned on)
{
	u8 val;

	pm8xxx_readb(dev, ATC_LED_SRC, &val);
	if (on)
		val |= ATC_LED_SRC_MASK;
	else
		val &= ~ATC_LED_SRC_MASK;
	pm8xxx_writeb(dev, ATC_LED_SRC, val);
}
#endif

static struct pm8xxx_led_config pm8921_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_duty_cycles,
#ifdef CONFIG_LEDS_PM8XXX_EXT_CTRL
		.led_ctrl = pm8xxx_atc_led_ctrl,
#endif
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = 8,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
	},
};

static struct pm8xxx_led_platform_data pm8xxx_leds_pdata = {
	.led_core = &pm8921_led_core_pdata,
	.configs = pm8921_led_configs,
	.num_configs = ARRAY_SIZE(pm8921_led_configs),
};

static int pm8xxx_set_led_info(unsigned index, struct led_info *linfo)
{
	if (index >= ARRAY_SIZE(pm8921_led_info)) {
		pr_err("%s: index %d out of bounds\n", __func__, index);
		return -EINVAL;
	}
	if (!linfo) {
		pr_err("%s: invalid led_info pointer\n", __func__);
		return -EINVAL;
	}
	memcpy(&pm8921_led_info[index], linfo, sizeof(struct led_info));
	return 0;
}

static __init void load_pm8921_batt_eprom_pdata_from_dt(void)
{
	struct device_node *chosen;
	u32 gpio = 0;
	struct w1_gpio_platform_data *pdata =
		((struct w1_gpio_platform_data *)
		 (mmi_w1_gpio_device.dev.platform_data));

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	if (of_property_read_u32(chosen, "batt_eprom_gpio", &gpio)) {
		pr_err("%s: batt_eprom_gpio property not found\n", __func__);
	} else {
		pdata->pin = gpio;
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

__init void mmi_load_rgb_leds_from_dt(void)
{
	u32 max_brightness;
	struct device_node *parent;
	struct led_pwm_gpio_platform_data *pdata =
		mmi_pm8xxx_rgb_leds_device.dev.platform_data;

	parent = of_find_node_by_path("/System@0/PowerIC@0/RGBLED@0");
	if (parent) {
		if (!of_property_read_u32(parent, "max_brightness",
			&max_brightness))
			pdata->max_brightness = (int)max_brightness;
	}
	of_node_put(parent);
}

static  __init void mmi_load_pm8921_leds_from_dt(void)
{
	struct device_node *parent, *child;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent)
		goto out;

	for_each_child_of_node(parent, child) {
		u32 type = 0;

		if (of_property_read_u32(child, "type", &type)) {
			pr_err("%s: type property not found\n", __func__);
			continue;
		} else {
			/* Qualcomm_PM8921_LED as defined in DT schema */
			if (0x001E000E == type) {
				const char *name = NULL;
				const char *trigger = NULL;
				u32 index;
				struct led_info *led_info;

				if (of_property_read_u32(child,
							"index", &index))
					pr_err("%s: index property not found\n",
								__func__);
				led_info = kzalloc(sizeof(struct led_info),
						GFP_KERNEL);
				BUG_ON(!led_info);

				of_property_read_string(child, "name", &name);
				BUG_ON(!name);

				led_info->name = kstrndup(name,
						strlen(name), GFP_KERNEL);
				BUG_ON(!led_info->name);

				of_property_read_string(child,
						"default_trigger", &trigger);
				BUG_ON(!trigger);

				led_info->default_trigger = kstrndup(trigger,
						strlen(trigger), GFP_KERNEL);
				BUG_ON(!led_info->default_trigger);

				pm8xxx_set_led_info(index, led_info);
			}
		}
	}

	of_node_put(parent);

out:
	return;
}


static int pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	325,
};

static const struct pm8xxx_adc_map_pt adcmap_batt_therm[] = {
	{-400,	1512},
	{-390,	1509},
	{-380,	1506},
	{-370,	1503},
	{-360,	1499},
	{-350,	1494},
	{-340,	1491},
	{-330,	1487},
	{-320,	1482},
	{-310,	1477},
	{-300,	1471},
	{-290,	1467},
	{-280,	1462},
	{-270,	1456},
	{-260,	1450},
	{-250,	1443},
	{-240,	1437},
	{-230,	1431},
	{-220,	1424},
	{-210,	1416},
	{-200,	1407},
	{-190,	1400},
	{-180,	1393},
	{-170,	1384},
	{-160,	1375},
	{-150,	1365},
	{-140,	1356},
	{-130,	1347},
	{-120,	1337},
	{-110,	1327},
	{-100,	1315},
	{-90,	1305},
	{-80,	1295},
	{-70,	1283},
	{-60,	1271},
	{-50,	1258},
	{-40,	1247},
	{-30,	1235},
	{-20,	1223},
	{-10,	1209},
	{-0,	1194},
	{10,	1182},
	{20,	1170},
	{30,	1156},
	{40,	1141},
	{50,	1126},
	{60,	1113},
	{70,	1100},
	{80,	1085},
	{90,	1070},
	{100,	1054},
	{110,	1041},
	{120,	1027},
	{130,	1013},
	{140,	997},
	{150,	981},
	{160,	968},
	{170,	954},
	{180,	940},
	{190,	924},
	{200,	909},
	{210,	896},
	{220,	882},
	{230,	868},
	{240,	854},
	{250,	839},
	{260,	826},
	{270,	813},
	{280,	800},
	{290,	787},
	{300,	772},
	{310,	761},
	{320,	749},
	{330,	737},
	{340,	724},
	{350,	711},
	{360,	701},
	{370,	690},
	{380,	679},
	{390,	668},
	{400,	656},
	{410,	646},
	{420,	637},
	{430,	627},
	{440,	617},
	{450,	606},
	{460,	598},
	{470,	589},
	{480,	580},
	{490,	571},
	{500,	562},
	{510,	555},
	{520,	547},
	{530,	540},
	{540,	532},
	{550,	524},
	{560,	517},
	{570,	511},
	{580,	504},
	{590,	497},
	{600,	491},
	{610,	485},
	{620,	479},
	{630,	473},
	{640,	468},
	{650,	462},
	{660,	457},
	{670,	452},
	{680,	447},
	{690,	442},
	{700,	437},
	{710,	433},
	{720,	429},
	{730,	424},
	{740,	420},
	{750,	416},
	{760,	412},
	{770,	409},
	{780,	405},
	{790,	402},
	{800,	398},
	{810,	395},
	{820,	392},
	{830,	389},
	{840,	386},
	{850,	382},
	{860,	380},
	{870,	377},
	{880,	375},
	{890,	372},
	{900,	369},
	{950,	358},
	{1000,	348},
	{1050,	340},
	{1100,	333},
	{1150,	327},
	{1200,	322},
	{1250,	317}
};

static struct pm8xxx_adc_scale_tbl adc_scale_tbls[ADC_SCALE_NONE] = {
	[ADC_SCALE_BATT_THERM] = {
		.scale_lu_tbl = adcmap_batt_therm,
		.scale_lu_tbl_size = ARRAY_SIZE(adcmap_batt_therm),
	},
};

static int battery_timeout;
static enum pm8921_btm_state btm_state;
static struct regulator *therm_reg;
static int therm_reg_enable;

void pm8921_chg_force_therm_bias(struct device *dev, int enable)
{
	int rc = 0;

	if (!therm_reg) {
		therm_reg = regulator_get(dev, "8921_l14");
		therm_reg_enable = 0;
	}

	if (enable && !therm_reg_enable) {
		if (!IS_ERR_OR_NULL(therm_reg)) {
			rc = regulator_set_voltage(therm_reg, 1800000, 1800000);
			if (!rc)
				rc = regulator_enable(therm_reg);
			if (rc)
				pr_err("L14 failed to set voltage 8921_l14\n");
			else
				therm_reg_enable = 1;
		}
	} else if (!enable && therm_reg_enable) {
		if (!IS_ERR_OR_NULL(therm_reg)) {
			rc = regulator_disable(therm_reg);
			if (rc)
				pr_err("L14 unable to disable 8921_l14\n");
			else
				therm_reg_enable = 0;
		}
	}
}

static int get_hot_temp_dt(void)
{
	struct device_node *parent;
	u32 temp;
	int hot_temp = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}

	if (!of_property_read_u32(parent, "chg-hot-temp", &temp))
		hot_temp = (int)temp;

	of_node_put(parent);
	pr_info("DT Hot Temp = %d\n", hot_temp);
	return hot_temp;
}

static int get_hot_offset_dt(void)
{
	struct device_node *parent;
	u32 temp_off;
	int hot_temp_off = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}

	if (!of_property_read_u32(parent, "chg-hot-temp-offset", &temp_off))
		hot_temp_off = (int)temp_off;

	of_node_put(parent);
	pr_info("DT Hot Temp Offset = %d\n", hot_temp_off);
	return hot_temp_off;
}

static int get_hot_temp_pcb_dt(void)
{
	struct device_node *parent;
	u32 temp;
	int hot_temp_pcb = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}

	if (!of_property_read_u32(parent, "chg-hot-temp-pcb", &temp))
		hot_temp_pcb = (int)temp;

	of_node_put(parent);
	pr_info("DT Hot Temp PCB = %d\n", hot_temp_pcb);
	return hot_temp_pcb;
}

static signed char get_hot_pcb_offset_dt(void)
{
	struct device_node *parent;
	u32 temp_off;
	signed char hot_temp_pcb_off = 0;

	parent = of_find_node_by_path("/System@0/PowerIC@0");
	if (!parent) {
		pr_info("Parent Not Found\n");
		return 0;
	}

	if (!of_property_read_u32(parent, "chg-hot-temp-pcb-offset", &temp_off))
		hot_temp_pcb_off = (signed char)temp_off;

	of_node_put(parent);

	pr_info("DT Hot Temp Offset PCB = %d\n", (int)hot_temp_pcb_off);
	return hot_temp_pcb_off;
}

int find_mmi_battery(struct mmi_battery_cell *cell_info)
{
	int i;

	if (cell_info->cell_id != OLD_EPROM_CELL_ID) {
		/* Search Based off Cell ID */
		for (i = (MMI_BATTERY_DEFAULT + 1); i < MMI_BATTERY_NUM; i++) {
			if (cell_info->cell_id ==
			    mmi_batts.cell_list[i]->cell_id) {
				pr_info("Battery Cell ID Found: %d\n", i);
				return i;
			}
		}
	}

	/* Search based off Typical Values */
	for (i = (MMI_BATTERY_DEFAULT + 1); i < MMI_BATTERY_NUM; i++) {
		if (cell_info->capacity == mmi_batts.cell_list[i]->capacity) {
			if (cell_info->peak_voltage ==
			    mmi_batts.cell_list[i]->peak_voltage) {
				if (cell_info->dc_impedance ==
				   mmi_batts.cell_list[i]->dc_impedance) {
					pr_info("Battery Match Found: %d\n", i);
					return i;
				}
			}
		}
	}

	pr_err("Battery Match Not Found!\n");
	return MMI_BATTERY_DEFAULT;
}

static int64_t read_mmi_battery_chrg(int64_t battery_id,
			      struct pm8921_charger_battery_data *data)
{
	struct mmi_battery_cell *cell_info;
	int i = 0;
	int ret;
	size_t len = sizeof(struct pm8921_charger_battery_data);

	for (i = 0; i < 50; i++) {
		if (battery_timeout)
			break;
		cell_info = mmi_battery_get_info();
		if (cell_info) {
			if (cell_info->batt_valid != MMI_BATTERY_UNKNOWN) {
				ret = find_mmi_battery(cell_info);
				memcpy(data, mmi_batts.chrg_list[ret], len);
				return (int64_t) cell_info->batt_valid;
			}
		}
		msleep(500);
	}

	battery_timeout = 1;
	memcpy(data, mmi_batts.chrg_list[MMI_BATTERY_DEFAULT], len);
	return 0;
}

static int64_t read_mmi_battery_bms(int64_t battery_id,
			     struct pm8921_bms_battery_data *data)
{
	struct mmi_battery_cell *cell_info;
	int i = 0;
	int ret;
	size_t len = sizeof(struct pm8921_bms_battery_data);

	for (i = 0; i < 50; i++) {
		if (battery_timeout)
			break;
		cell_info = mmi_battery_get_info();
		if (cell_info) {
			if (cell_info->batt_valid != MMI_BATTERY_UNKNOWN) {
				ret = find_mmi_battery(cell_info);
				memcpy(data, mmi_batts.bms_list[ret], len);
				return (int64_t) cell_info->batt_valid;
			}
		}
		msleep(500);
	}

	battery_timeout = 1;
	memcpy(data, mmi_batts.bms_list[MMI_BATTERY_DEFAULT], len);
	return 0;
}

#define TEMP_HYSTERISIS_DEGC 2
#define TEMP_OVERSHOOT 10
#define TEMP_HOT 60
#define TEMP_COLD -20
static int64_t temp_range_check(int batt_temp, int batt_mvolt,
				struct pm8921_charger_battery_data *data,
				int64_t *enable, enum pm8921_btm_state *state)
{
	int64_t chrg_enable = 0;
	int64_t btm_change = 0;

	if (!enable || !data)
		return btm_change;

	/* Check for a State Change */
	if (btm_state == BTM_NORM) {
		if (batt_temp >= (signed int)(data->warm_temp)) {
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT - data->hot_temp_offset;
			if (batt_mvolt >= data->warm_bat_voltage) {
				btm_state = BTM_WARM_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_WARM_LV;
				data->max_voltage = data->warm_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		} else if (batt_temp <= (signed int)(data->cool_temp)) {
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			if (batt_mvolt >= data->cool_bat_voltage) {
				btm_state = BTM_COOL_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_COOL_LV;
				data->max_voltage = data->cool_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		}
	} else if (btm_state == BTM_COLD) {
		if (batt_temp >= TEMP_COLD + TEMP_HYSTERISIS_DEGC) {
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			if (batt_mvolt >= data->cool_bat_voltage) {
				btm_state = BTM_COOL_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_COOL_LV;
				data->max_voltage = data->cool_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		}
	} else if (btm_state == BTM_COOL_LV) {
		if (batt_temp <= TEMP_COLD) {
			btm_state = BTM_COLD;
			data->cool_temp = TEMP_COLD - TEMP_OVERSHOOT;
			data->warm_temp = TEMP_COLD + TEMP_HYSTERISIS_DEGC;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp >=
			   ((signed int)(data->cool_temp) +
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt >= data->cool_bat_voltage) {
			btm_state = BTM_COOL_HV;
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			data->max_voltage = data->cool_bat_voltage;
			chrg_enable = 0;
			btm_change = 1;
		}
	} else if (btm_state == BTM_COOL_HV) {
		if (batt_temp <= TEMP_COLD) {
			btm_state = BTM_COLD;
			data->cool_temp = TEMP_COLD - TEMP_OVERSHOOT;
			data->warm_temp = TEMP_COLD + TEMP_HYSTERISIS_DEGC;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp >=
			   ((signed int)(data->cool_temp) +
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt < data->cool_bat_voltage) {
			btm_state = BTM_COOL_LV;
			data->warm_temp =
				data->cool_temp + TEMP_HYSTERISIS_DEGC;
			data->cool_temp = TEMP_COLD;
			data->max_voltage = data->cool_bat_voltage;
			chrg_enable = 1;
			btm_change = 1;
		}
	} else if (btm_state == BTM_WARM_LV) {
		if (batt_temp >= TEMP_HOT) {
			btm_state = BTM_HOT;
			data->cool_temp = TEMP_HOT - TEMP_HYSTERISIS_DEGC
				- data->hot_temp_offset;
			data->warm_temp = TEMP_HOT + TEMP_OVERSHOOT
				- data->hot_temp_offset;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp <=
			   ((signed int)(data->warm_temp) -
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt >= data->warm_bat_voltage) {
			btm_state = BTM_WARM_HV;
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT
				- data->hot_temp_offset;
			data->max_voltage = data->warm_bat_voltage;
			chrg_enable = 0;
			btm_change = 1;
		}
	} else if (btm_state == BTM_WARM_HV) {
		if (batt_temp >= TEMP_HOT) {
			btm_state = BTM_HOT;
			data->cool_temp = TEMP_HOT - TEMP_HYSTERISIS_DEGC
				- data->hot_temp_offset;
			data->warm_temp = TEMP_HOT + TEMP_OVERSHOOT
				- data->hot_temp_offset;
			chrg_enable = 0;
			btm_change = 1;
		} else if (batt_temp <=
			   ((signed int)(data->warm_temp) -
			    TEMP_HYSTERISIS_DEGC)) {
			btm_state = BTM_NORM;
			chrg_enable = 1;
			btm_change = 1;
		} else if (batt_mvolt < data->warm_bat_voltage) {
			btm_state = BTM_WARM_LV;
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT
				- data->hot_temp_offset;
			data->max_voltage = data->warm_bat_voltage;
			chrg_enable = 1;
			btm_change = 1;
		}
	} else if (btm_state == BTM_HOT) {
		if (batt_temp <= TEMP_HOT - TEMP_HYSTERISIS_DEGC) {
			data->cool_temp =
				data->warm_temp - TEMP_HYSTERISIS_DEGC;
			data->warm_temp = TEMP_HOT
				- data->hot_temp_offset;
			if (batt_mvolt >= data->warm_bat_voltage) {
				btm_state = BTM_WARM_HV;
				chrg_enable = 0;
			} else {
				btm_state = BTM_WARM_LV;
				data->max_voltage = data->warm_bat_voltage;
				chrg_enable = 1;
			}
			btm_change = 1;
		}
	}

	*enable = chrg_enable;
	*state = btm_state;

	return btm_change;
}

#define MAX_VOLTAGE_MV		4350
static struct pm8921_charger_platform_data pm8921_chg_pdata __devinitdata = {
	.safety_time		= 512,
	.update_time		= 60000,
	.ttrkl_time		= 64,
	.max_voltage		= MAX_VOLTAGE_MV,
	.min_voltage		= 3200,
	.resume_voltage_delta	= 100,
	.term_current		= 100,
	.cool_temp		= 0,
	.warm_temp		= 45,
	.temp_check_period	= 1,
	.max_bat_chg_current	= 2000,
	.cool_bat_chg_current	= 1500,
	.warm_bat_chg_current	= 1500,
	.cool_bat_voltage	= 3800,
	.warm_bat_voltage	= 3800,
	.batt_id_min		= 0,
	.batt_id_max		= 1,
	.thermal_mitigation	= pm8921_therm_mitigation,
	.thermal_levels		= ARRAY_SIZE(pm8921_therm_mitigation),
	.cold_thr		= PM_SMBC_BATT_TEMP_COLD_THR__HIGH,
	.hot_thr		= PM_SMBC_BATT_TEMP_HOT_THR__LOW,
	.rconn_mohm		= 18,
	.factory_mode		= 0,
	.meter_lock		= 0,
#ifdef CONFIG_PM8921_EXTENDED_INFO
	.get_batt_info		= read_mmi_battery_chrg,
	.temp_range_cb		= temp_range_check,
	.batt_alarm_delta	= 100,
	.lower_battery_threshold = 3400,
	.force_therm_bias	= pm8921_chg_force_therm_bias,
#endif
};

static struct pm8921_bms_platform_data pm8921_bms_pdata __devinitdata = {
	.battery_type		= BATT_MMI,
	.r_sense		= 10,
	.i_test			= 0,
	.v_cutoff		= 3200,
	.max_voltage_uv		= MAX_VOLTAGE_MV * 1000,
#ifdef CONFIG_PM8921_EXTENDED_INFO
	.get_batt_info=		read_mmi_battery_bms,
#endif
};

void __init mmi_pm8921_init(struct mmi_oem_data *mmi_data, void *pdata)
{
	struct pm8921_platform_data *pm8921_pdata;

	pm8921_pdata = (struct pm8921_platform_data *) pdata;

	pm8921_pdata->adc_pdata->scale_tbls = adc_scale_tbls;
	pm8921_pdata->charger_pdata = &pm8921_chg_pdata;
	pm8921_pdata->bms_pdata = &pm8921_bms_pdata;
	if (mmi_data->is_factory) {
		pm8921_pdata->charger_pdata->factory_mode =
			mmi_data->is_factory();
		pm8921_pdata->rtc_pdata->rtc_write_enable =
			mmi_data->is_factory();
	}
	if (mmi_data->is_meter_locked)
		pm8921_pdata->charger_pdata->meter_lock =
			mmi_data->is_meter_locked();

#ifdef CONFIG_PM8921_EXTENDED_INFO
	pm8921_pdata->charger_pdata->hot_temp = get_hot_temp_dt();
	pm8921_pdata->charger_pdata->hot_temp_offset = get_hot_offset_dt();
	pm8921_pdata->charger_pdata->hot_temp_pcb = get_hot_temp_pcb_dt();
	pm8921_pdata->charger_pdata->hot_temp_pcb_offset =
		get_hot_pcb_offset_dt();
#endif

	load_pm8921_batt_eprom_pdata_from_dt();
	mmi_load_rgb_leds_from_dt();
	mmi_load_pm8921_leds_from_dt();
	pm8921_pdata->leds_pdata = &pm8xxx_leds_pdata;
}
