/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/leds.h>
#include <linux/leds-pm8xxx.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/restart.h>
#include <mach/board_lge.h>
#include "devices.h"
#include "board-mako.h"

#ifdef CONFIG_WIRELESS_CHARGER
#include <linux/power/bq51051b_charger.h>
#endif

struct pm8xxx_gpio_init {
	unsigned gpio;
	struct pm_gpio config;
};

struct pm8xxx_mpp_init {
	unsigned mpp;
	struct pm8xxx_mpp_config_data config;
};

#define PM8921_GPIO_INIT(_gpio, _dir, _buf, _val, _pull, _vin, _out_strength, \
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

#define PM8921_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8921_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8821_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8821_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8921_GPIO_DISABLE(_gpio) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, 0, 0, 0, PM_GPIO_VIN_S4, \
			 0, 0, 0, 1)

#define PM8921_GPIO_OUTPUT(_gpio, _val, _strength) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_##_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8921_GPIO_OUTPUT_BUFCONF(_gpio, _val, _strength, _bufconf) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT,\
			PM_GPIO_OUT_BUF_##_bufconf, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_##_strength, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8921_GPIO_INPUT(_gpio, _pull) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_IN, PM_GPIO_OUT_BUF_CMOS, 0, \
			_pull, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_NO, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

#define PM8921_GPIO_OUTPUT_FUNC(_gpio, _val, _func) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, PM_GPIO_VIN_S4, \
			PM_GPIO_STRENGTH_HIGH, \
			_func, 0, 0)

#define PM8921_GPIO_OUTPUT_VIN(_gpio, _val, _vin) \
	PM8921_GPIO_INIT(_gpio, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, _val, \
			PM_GPIO_PULL_NO, _vin, \
			PM_GPIO_STRENGTH_HIGH, \
			PM_GPIO_FUNC_NORMAL, 0, 0)

/* Initial PM8921 GPIO configurations */
static struct pm8xxx_gpio_init pm8921_gpios[] __initdata = {
	PM8921_GPIO_INPUT(14, PM_GPIO_PULL_DN), /* SLIMPORT_CBL_DET */
	PM8921_GPIO_OUTPUT(15, 0, HIGH), /* ANX_P_DWN_CTL */
	PM8921_GPIO_OUTPUT(16, 0, HIGH), /* ANX_AVDD33_EN */
	PM8921_GPIO_OUTPUT(17, 0, HIGH), /* CAM_VCM_EN */
	PM8921_GPIO_OUTPUT(19, 0, HIGH), /* AMP_EN_AMP */
	PM8921_GPIO_OUTPUT(20, 0, HIGH), /* PMIC - FSA8008 EAR_MIC_BIAS_EN */
#ifdef CONFIG_WIRELESS_CHARGER
	PM8921_GPIO_INPUT(25,PM_GPIO_PULL_UP_1P5_30), /* WLC ACTIVE_N */
	PM8921_GPIO_OUTPUT(26, 0, HIGH), /* WLC CHG_STAT */
#endif
	PM8921_GPIO_OUTPUT(31, 0, HIGH), /* PMIC - FSA8008_EAR_MIC_EN */
	PM8921_GPIO_INPUT(32, PM_GPIO_PULL_UP_1P5), /* PMIC - FSA8008_EARPOL_DETECT */
	PM8921_GPIO_OUTPUT(33, 0, HIGH), /* HAPTIC_EN */
	PM8921_GPIO_OUTPUT(34, 0, HIGH), /* WCD_RESET_N */
};

void __init apq8064_pm8xxx_gpio_mpp_init(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(pm8921_gpios); i++) {
		rc = pm8xxx_gpio_config(pm8921_gpios[i].gpio,
					&pm8921_gpios[i].config);
		if (rc) {
			pr_err("%s: pm8xxx_gpio_config: rc=%d\n", __func__, rc);
			break;
		}
	}
}

static struct pm8xxx_pwrkey_platform_data apq8064_pm8921_pwrkey_pdata = {
	.pull_up = 1,
	.kpd_trigger_delay_us = 15625,
	.wakeup = 1,
};

static struct pm8xxx_misc_platform_data apq8064_pm8921_misc_pdata = {
	.priority = 0,
};

#define PM8921_LC_LED_MAX_CURRENT 4	/* I = 4mA */
#define PM8921_LC_LED_LOW_CURRENT 1	/* I = 1mA */
#define PM8XXX_LED_PWM_PERIOD     1000
#define PM8XXX_LED_PWM_DUTY_MS    20
/**
 * PM8XXX_PWM_CHANNEL_NONE shall be used when LED shall not be
 * driven using PWM feature.
 */
#define PM8XXX_PWM_CHANNEL_NONE		-1

static struct led_info pm8921_led_info[] = {
	[0] = {
		.name = "red",
	},
	[1] = {
		.name = "green",
	},
	[2] = {
		.name = "blue",
	},
};

static struct led_platform_data pm8921_led_core_pdata = {
	.num_leds = ARRAY_SIZE(pm8921_led_info),
	.leds = pm8921_led_info,
};

static struct pm8xxx_led_config pm8921_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
	[2] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_MANUAL,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
	},
};

static struct pm8xxx_led_platform_data apq8064_pm8921_leds_pdata = {
		.led_core = &pm8921_led_core_pdata,
		.configs = pm8921_led_configs,
		.num_configs = ARRAY_SIZE(pm8921_led_configs),
};

static struct pm8xxx_adc_amux apq8064_pm8921_adc_channels_data[] = {
	{"vcoin", CHANNEL_VCOIN, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vbat", CHANNEL_VBAT, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"dcin", CHANNEL_DCIN, CHAN_PATH_SCALING4, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ichg", CHANNEL_ICHG, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"vph_pwr", CHANNEL_VPH_PWR, CHAN_PATH_SCALING2, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"ibat", CHANNEL_IBAT, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"batt_therm", CHANNEL_BATT_THERM, CHAN_PATH_SCALING1, AMUX_RSV2,
		ADC_DECIMATION_TYPE2, ADC_SCALE_BATT_THERM},
	{"batt_id", CHANNEL_BATT_ID, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"usbin", CHANNEL_USBIN, CHAN_PATH_SCALING3, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"pmic_therm", CHANNEL_DIE_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_PMIC_THERM},
	{"625mv", CHANNEL_625MV, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"125v", CHANNEL_125V, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"chg_temp", CHANNEL_CHG_TEMP, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
	{"xo_therm", CHANNEL_MUXOFF, CHAN_PATH_SCALING1, AMUX_RSV0,
		ADC_DECIMATION_TYPE2, ADC_SCALE_XOTHERM},
	{"usb_id", ADC_MPP_1_AMUX6, CHAN_PATH_SCALING1, AMUX_RSV1,
		ADC_DECIMATION_TYPE2, ADC_SCALE_DEFAULT},
};

static struct pm8xxx_adc_properties apq8064_pm8921_adc_data = {
	.adc_vdd_reference	= 1800, /* milli-voltage for this adc */
	.bitresolution		= 15,
	.bipolar                = 0,
};

static struct pm8xxx_adc_platform_data apq8064_pm8921_adc_pdata = {
	.adc_channel		= apq8064_pm8921_adc_channels_data,
	.adc_num_board_channel	= ARRAY_SIZE(apq8064_pm8921_adc_channels_data),
	.adc_prop		= &apq8064_pm8921_adc_data,
	.adc_mpp_base		= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_mpp_platform_data
apq8064_pm8921_mpp_pdata __devinitdata = {
	.mpp_base	= PM8921_MPP_PM_TO_SYS(1),
};

static struct pm8xxx_gpio_platform_data
apq8064_pm8921_gpio_pdata __devinitdata = {
	.gpio_base	= PM8921_GPIO_PM_TO_SYS(1),
};

static struct pm8xxx_irq_platform_data
apq8064_pm8921_irq_pdata __devinitdata = {
	.irq_base		= PM8921_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(74),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
	.dev_id			= 0,
};

static struct pm8xxx_rtc_platform_data
apq8064_pm8921_rtc_pdata = {
	.rtc_write_enable       = false,
	.rtc_alarm_powerup      = false,
};

static int apq8064_pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	325,
};

/*
 * Battery characteristic
 * Typ.2100mAh capacity, Li-Ion Polymer 3.8V
 * Battery/VDD voltage programmable range, 20mV steps.
 */
#define MAX_VOLTAGE_MV		4360

static struct pm8921_charger_platform_data apq8064_pm8921_chg_pdata __devinitdata = {

	/* max charging time in minutes incl. fast and trkl. it will be changed in future  */
	.safety_time		= 512, /* 300 change max value for charging time */
	.update_time		= 60000,
	.max_voltage		= MAX_VOLTAGE_MV,

	/* the voltage (mV) where charging method switches from trickle to fast.
	 * This is also the minimum voltage the system operates at */
	.min_voltage		= 3200,
	/* the (mV) drop to wait for before resume charging after the battery has been fully charged */
	.resume_voltage_delta	= 50,
	.term_current		= 100,

#ifdef CONFIG_LGE_CHARGER_TEMP_SCENARIO
	/* Configuration of cool and warm thresholds (JEITA compliance only) */
	.cool_temp		= 0, /* 10 */	/* 10 degree celsius */
	.warm_temp		= 0, /* 40 */	/* 40 degree celsius */
	.cool_bat_chg_current	= 350,	/* 350 mA (max value = 2A) */
	.warm_bat_chg_current	= 350,
	.temp_level_1		= 550,
	.temp_level_2		= 450,
	.temp_level_3		= 420,
	.temp_level_4		= -50,
	.temp_level_5		= -100,
	/* Temperature Thresholds (JEITA compliance) (-10'C ~ 60'C) */
	.cold_thr		= 1,	/* 80% */
	.hot_thr		= 0,	/* 20% */
#else /* qualcomm original value */
	.cool_temp		= 10,
	.warm_temp		= 40,
	.cool_bat_chg_current	= 350,
	.warm_bat_chg_current	= 350,
#endif

	.temp_check_period	= 1,

	/*  Battery charge current programmable range 50mA steps */
	/*  max_bat_chg_current:
	 *    Max charge current of the battery in mA
	 *    Usually 70% of full charge capacity
	 */
	.max_bat_chg_current	= 1350,

	.cool_bat_voltage	= 4100,
	.warm_bat_voltage	= 4100,
	.thermal_mitigation	= apq8064_pm8921_therm_mitigation,
	.thermal_levels		= ARRAY_SIZE(apq8064_pm8921_therm_mitigation),
	/* for led on, off control */
	.led_src_config		= LED_SRC_MIN_VPH_5V,
};

static struct pm8xxx_ccadc_platform_data
apq8064_pm8xxx_ccadc_pdata = {
	.r_sense		= 10,
	.calib_delay_ms		= 600000,
};

static struct pm8921_bms_platform_data
apq8064_pm8921_bms_pdata __devinitdata = {
	.battery_type		= BATT_LGE,
	.r_sense		= 10,
	.i_test			= 834,
	.v_failure		= 3300,
	.max_voltage_uv		= MAX_VOLTAGE_MV * 1000,
};

static unsigned int keymap[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(0, 1, KEY_VOLUMEUP),
};

static struct matrix_keymap_data keymap_data = {
	.keymap_size    = ARRAY_SIZE(keymap),
	.keymap         = keymap,
};

static __init void mako_fixed_keymap(void) {
	if (lge_get_board_revno() < HW_REV_C) {
		keymap[0] = KEY(0, 0, KEY_VOLUMEUP);
		keymap[1] = KEY(0, 1, KEY_VOLUMEDOWN);
	}
}

static struct pm8xxx_keypad_platform_data keypad_data = {
	.input_name             = "keypad_8064",
	.input_phys_device      = "keypad_8064/input0",
	.num_rows               = 1,
	.num_cols               = 5,
	.rows_gpio_start	= PM8921_GPIO_PM_TO_SYS(9),
	.cols_gpio_start	= PM8921_GPIO_PM_TO_SYS(1),
	.debounce_ms            = 15,
	.scan_delay_ms          = 32,
	.row_hold_ns            = 91500,
	.wakeup                 = 1,
	.keymap_data            = &keymap_data,
};

static struct pm8921_platform_data
apq8064_pm8921_platform_data __devinitdata = {
	.regulator_pdatas	= msm8064_pm8921_regulator_pdata,
	.irq_pdata		= &apq8064_pm8921_irq_pdata,
	.gpio_pdata		= &apq8064_pm8921_gpio_pdata,
	.mpp_pdata		= &apq8064_pm8921_mpp_pdata,
	.rtc_pdata		= &apq8064_pm8921_rtc_pdata,
	.pwrkey_pdata		= &apq8064_pm8921_pwrkey_pdata,
	.keypad_pdata		= &keypad_data,
	.misc_pdata		= &apq8064_pm8921_misc_pdata,
	.leds_pdata		= &apq8064_pm8921_leds_pdata,
	.adc_pdata		= &apq8064_pm8921_adc_pdata,
	.charger_pdata		= &apq8064_pm8921_chg_pdata,
	.bms_pdata		= &apq8064_pm8921_bms_pdata,
	.ccadc_pdata		= &apq8064_pm8xxx_ccadc_pdata,
};

static struct pm8xxx_irq_platform_data
apq8064_pm8821_irq_pdata __devinitdata = {
	.irq_base		= PM8821_IRQ_BASE,
	.devirq			= PM8821_SEC_IRQ_N,
	.irq_trigger_flag	= IRQF_TRIGGER_HIGH,
	.dev_id			= 1,
};

static struct pm8xxx_mpp_platform_data
apq8064_pm8821_mpp_pdata __devinitdata = {
	.mpp_base	= PM8821_MPP_PM_TO_SYS(1),
};

static struct pm8821_platform_data
apq8064_pm8821_platform_data __devinitdata = {
	.irq_pdata = &apq8064_pm8821_irq_pdata,
	.mpp_pdata = &apq8064_pm8821_mpp_pdata,
};

static struct msm_ssbi_platform_data apq8064_ssbi_pm8921_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name = "pm8921-core",
		.platform_data = &apq8064_pm8921_platform_data,
	},
};

static struct msm_ssbi_platform_data apq8064_ssbi_pm8821_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name = "pm8821-core",
		.platform_data = &apq8064_pm8821_platform_data,
	},
};

#ifdef CONFIG_WIRELESS_CHARGER
static struct bq51051b_wlc_platform_data bq51051b_wlc_pmic_pdata = {
	.chg_state_gpio		= PM8921_GPIO_PM_TO_SYS(26),
	.active_n_gpio		= PM8921_GPIO_PM_TO_SYS(25),
};

struct platform_device wireless_charger = {
	.name		= "bq51051b_wlc",
	.id		= -1,
	.dev = {
		.platform_data = &bq51051b_wlc_pmic_pdata,
	},
};
#endif

void __init apq8064_init_pmic(void)
{
	pmic_reset_irq = PM8921_IRQ_BASE + PM8921_RESOUT_IRQ;

	mako_fixed_keymap();

	apq8064_device_ssbi_pmic1.dev.platform_data =
		&apq8064_ssbi_pm8921_pdata;
	apq8064_device_ssbi_pmic2.dev.platform_data =
		&apq8064_ssbi_pm8821_pdata;
	apq8064_pm8921_platform_data.num_regulators =
		msm8064_pm8921_regulator_pdata_len;
}
