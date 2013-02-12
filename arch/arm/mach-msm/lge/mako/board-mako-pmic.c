/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/power/bq51051b_charger.h>
#include <linux/platform_data/battery_temp_ctrl.h>
#include <linux/gpio.h>
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
#include "board-mako-pmic.h"

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
	PM8921_GPIO_INPUT(13, PM_GPIO_PULL_DN), /* EARJACK_DEBUGGER */
	PM8921_GPIO_INPUT(14, PM_GPIO_PULL_DN), /* SLIMPORT_CBL_DET */
	PM8921_GPIO_OUTPUT(15, 0, HIGH), /* ANX_P_DWN_CTL */
	PM8921_GPIO_OUTPUT(16, 0, HIGH), /* ANX_AVDD33_EN */
	PM8921_GPIO_OUTPUT(17, 0, HIGH), /* CAM_VCM_EN */
	PM8921_GPIO_OUTPUT(19, 0, HIGH), /* AMP_EN_AMP */
	PM8921_GPIO_OUTPUT(20, 0, HIGH), /* PMIC - FSA8008 EAR_MIC_BIAS_EN */
	PM8921_GPIO_OUTPUT(31, 0, HIGH), /* PMIC - FSA8008_EAR_MIC_EN */
	PM8921_GPIO_INPUT(32, PM_GPIO_PULL_UP_1P5), /* PMIC - FSA8008_EARPOL_DETECT */
	PM8921_GPIO_OUTPUT(33, 0, HIGH), /* HAPTIC_EN */
	PM8921_GPIO_OUTPUT(34, 0, HIGH), /* WCD_RESET_N */
};

#ifdef CONFIG_WIRELESS_CHARGER
#define PM8921_GPIO_WLC_ACTIVE      25
#define PM8921_GPIO_WLC_ACTIVE_11   17
static struct pm8xxx_gpio_init pm8921_gpios_wlc[] __initdata = {
	PM8921_GPIO_INPUT(PM8921_GPIO_WLC_ACTIVE,PM_GPIO_PULL_UP_1P5_30),
	PM8921_GPIO_OUTPUT(26, 0, HIGH), /* WLC CHG_STAT */
};
static struct pm8xxx_gpio_init pm8921_gpios_wlc_rev11[] __initdata = {
	PM8921_GPIO_INPUT(PM8921_GPIO_WLC_ACTIVE_11,PM_GPIO_PULL_UP_1P5_30),
	PM8921_GPIO_OUTPUT(26, 0, HIGH), /* WLC CHG_STAT */
};
#endif

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
#ifdef CONFIG_WIRELESS_CHARGER
	if (lge_get_board_revno() >= HW_REV_1_1) {
		for (i = 0; i < ARRAY_SIZE(pm8921_gpios_wlc_rev11); i++) {
			rc = pm8xxx_gpio_config(pm8921_gpios_wlc_rev11[i].gpio,
					&pm8921_gpios_wlc_rev11[i].config);
			if (rc < 0) {
				pr_err("%s: pm8xxx_gpio_config: rc=%d\n",
						__func__, rc);
				break;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(pm8921_gpios_wlc); i++) {
			rc = pm8xxx_gpio_config(pm8921_gpios_wlc[i].gpio,
						&pm8921_gpios_wlc[i].config);
			if (rc < 0) {
				pr_err("%s: pm8xxx_gpio_config: rc=%d\n",
						__func__, rc);
				break;
			}
		}
	}
#endif
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
#define PM8XXX_LED_PWM_ADJUST_BRIGHTNESS_E 10	/* max duty percentage */
#define PM8XXX_LED_PWM_PERIOD     1000
#define PM8XXX_LED_PWM_DUTY_MS    50
#define PM8XXX_LED_PWM_DUTY_PCTS  16
#define PM8XXX_LED_PWM_START_IDX0 16
#define PM8XXX_LED_PWM_START_IDX1 32
#define PM8XXX_LED_PWM_START_IDX2 48

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

static int pm8921_led0_pwm_duty_pcts[PM8XXX_LED_PWM_DUTY_PCTS] = {0,};
static int pm8921_led1_pwm_duty_pcts[PM8XXX_LED_PWM_DUTY_PCTS] = {0,};
static int pm8921_led2_pwm_duty_pcts[PM8XXX_LED_PWM_DUTY_PCTS] = {0,};

static struct pm8xxx_pwm_duty_cycles pm8921_led0_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led0_pwm_duty_pcts,
	.num_duty_pcts = PM8XXX_LED_PWM_DUTY_PCTS,
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = PM8XXX_LED_PWM_START_IDX0,
};

static struct pm8xxx_pwm_duty_cycles pm8921_led1_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led1_pwm_duty_pcts,
	.num_duty_pcts = PM8XXX_LED_PWM_DUTY_PCTS,
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = PM8XXX_LED_PWM_START_IDX1,
};

static struct pm8xxx_pwm_duty_cycles pm8921_led2_pwm_duty_cycles = {
	.duty_pcts = (int *)&pm8921_led2_pwm_duty_pcts,
	.num_duty_pcts = PM8XXX_LED_PWM_DUTY_PCTS,
	.duty_ms = PM8XXX_LED_PWM_DUTY_MS,
	.start_idx = PM8XXX_LED_PWM_START_IDX2,
};

static struct pm8xxx_led_config pm8921_led_configs[] = {
	[0] = {
		.id = PM8XXX_ID_LED_0,
		.mode = PM8XXX_LED_MODE_PWM3,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 6,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led0_pwm_duty_cycles,
		.pwm_adjust_brightness = 52,
	},
	[1] = {
		.id = PM8XXX_ID_LED_1,
		.mode = PM8XXX_LED_MODE_PWM2,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 5,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led1_pwm_duty_cycles,
		.pwm_adjust_brightness = 43,
	},
	[2] = {
		.id = PM8XXX_ID_LED_2,
		.mode = PM8XXX_LED_MODE_PWM1,
		.max_current = PM8921_LC_LED_MAX_CURRENT,
		.pwm_channel = 4,
		.pwm_period_us = PM8XXX_LED_PWM_PERIOD,
		.pwm_duty_cycles = &pm8921_led2_pwm_duty_cycles,
		.pwm_adjust_brightness = 75,
	},
};

static __init void mako_fixed_leds(void) {
	if (lge_get_board_revno() <= HW_REV_E) {
		int i = 0;
		for (i = 0; i < ARRAY_SIZE(pm8921_led_configs); i++)
			pm8921_led_configs[i].pwm_adjust_brightness =
				PM8XXX_LED_PWM_ADJUST_BRIGHTNESS_E;
	}
}

static struct pm8xxx_led_platform_data apq8064_pm8921_leds_pdata = {
		.led_core = &pm8921_led_core_pdata,
		.configs = pm8921_led_configs,
		.num_configs = ARRAY_SIZE(pm8921_led_configs),
		.use_pwm = 1,
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
	.rtc_write_enable       = true,
	.rtc_alarm_powerup      = false,
};

static int apq8064_pm8921_therm_mitigation[] = {
	1100,
	700,
	600,
	400,
};

static int batt_temp_ctrl_level[] = {
	600,
	570,
	550,
	450,
	440,
	-50,
	-80,
	-100,
};

#ifdef CONFIG_WIRELESS_CHARGER
#define GPIO_WLC_ACTIVE        PM8921_GPIO_PM_TO_SYS(PM8921_GPIO_WLC_ACTIVE)
#define GPIO_WLC_ACTIVE_11     PM8921_GPIO_PM_TO_SYS(PM8921_GPIO_WLC_ACTIVE_11)
#define GPIO_WLC_STATE         PM8921_GPIO_PM_TO_SYS(26)

static int wireless_charger_is_plugged(void);

static struct bq51051b_wlc_platform_data bq51051b_wlc_pmic_pdata = {
	.chg_state_gpio  = GPIO_WLC_STATE,
	.active_n_gpio   = GPIO_WLC_ACTIVE,
	.wlc_is_plugged  = wireless_charger_is_plugged,
};

struct platform_device wireless_charger = {
	.name		= "bq51051b_wlc",
	.id		= -1,
	.dev = {
		.platform_data = &bq51051b_wlc_pmic_pdata,
	},
};

static int wireless_charger_is_plugged(void)
{
	static bool initialized = false;
	unsigned int wlc_active_n = 0;
	int ret = 0;

	wlc_active_n = bq51051b_wlc_pmic_pdata.active_n_gpio;
	if (!wlc_active_n) {
		pr_warn("wlc : active_n gpio is not defined yet");
		return 0;
	}

	if (!initialized) {
		ret =  gpio_request_one(wlc_active_n, GPIOF_DIR_IN,
				"active_n_gpio");
		if (ret < 0) {
			pr_err("wlc: active_n gpio request failed\n");
			return 0;
		}
		initialized = true;
	}

	return !(gpio_get_value(wlc_active_n));
}

static __init void mako_fixup_wlc_gpio(void) {
	if (lge_get_board_revno() >= HW_REV_1_1)
		bq51051b_wlc_pmic_pdata.active_n_gpio = GPIO_WLC_ACTIVE_11;
}

#else
static int wireless_charger_is_plugged(void) { return 0; }
static __init void mako_set_wlc_gpio(void) { }
#endif

/*
 * Battery characteristic
 * Typ.2100mAh capacity, Li-Ion Polymer 3.8V
 * Battery/VDD voltage programmable range, 20mV steps.
 */
#define MAX_VOLTAGE_MV		4360
#define CHG_TERM_MA		100
#define MAX_BATT_CHG_I_MA	900
#define WARM_BATT_CHG_I_MA	400
#define VBATDET_DELTA_MV	50
#define EOC_CHECK_SOC	1

static struct pm8921_charger_platform_data apq8064_pm8921_chg_pdata __devinitdata = {
	.safety_time  = 512,
	.update_time  = 60000,
	.max_voltage  = MAX_VOLTAGE_MV,
	.min_voltage  = 3200,
	.alarm_voltage  = 3500,
	.resume_voltage_delta  = VBATDET_DELTA_MV,
	.term_current  = CHG_TERM_MA,

	.cool_temp  = INT_MIN,
	.warm_temp  = INT_MIN,
	.cool_bat_chg_current  = 350,
	.warm_bat_chg_current  = WARM_BATT_CHG_I_MA,
	.cold_thr  = 1,
	.hot_thr  = 0,
	.ext_batt_temp_monitor  = 1,
	.temp_check_period  = 1,
	.max_bat_chg_current  = MAX_BATT_CHG_I_MA,
	.cool_bat_voltage  = 4100,
	.warm_bat_voltage  = 4100,
	.thermal_mitigation  = apq8064_pm8921_therm_mitigation,
	.thermal_levels  = ARRAY_SIZE(apq8064_pm8921_therm_mitigation),
	.led_src_config  = LED_SRC_5V,
	.rconn_mohm	 = 37,
	.eoc_check_soc  = EOC_CHECK_SOC,
};

static struct pm8xxx_ccadc_platform_data
apq8064_pm8xxx_ccadc_pdata = {
	.r_sense		= 10,
	.calib_delay_ms		= 600000,
};

static struct pm8921_bms_platform_data
apq8064_pm8921_bms_pdata __devinitdata = {
	.battery_type	= BATT_UNKNOWN, //FIXME Define correct type
	.r_sense  = 10,
	.v_cutoff  = 3500,
	.max_voltage_uv  = MAX_VOLTAGE_MV * 1000,
	.rconn_mohm  = 37,
	.shutdown_soc_valid_limit  = 20,
	.adjust_soc_low_threshold  = 25,
	.chg_term_ua  = CHG_TERM_MA * 1000,
	.eoc_check_soc  = EOC_CHECK_SOC,
	.bms_support_wlc  = 1,
	.wlc_term_ua = 110000,
	.wlc_max_voltage_uv = 4290000,
	.wlc_is_plugged  = wireless_charger_is_plugged,
	.first_fixed_iavg_ma  = 500,
};

/* battery data */
static struct single_row_lut batt_2100_fcc_temp = {
	.x = {-20, 0, 25, 40, 60 },
	.y = {2068, 2064, 2103, 2072, 2084},
	.cols = 5
};

static struct single_row_lut batt_2100_fcc_sf = {
	.x     = {0 },
	.y     = {100 },
	.cols  = 1
};

static struct sf_lut batt_2100_rbatt_sf =  {
	.rows = 28,
	.cols = 5,
	.row_entries = {-20, 0, 25, 40, 60 },
	.percent =  {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50,
		     45, 40, 35, 30, 25, 20, 15, 10,
		     9, 8, 7, 6, 5, 4, 3, 2, 1 },
	.sf = {
		{639,265,100,75,66},
		{700,262,104,79,68},
		{727,259,106,81,70},
		{746,258,106,82,72},
		{764,260,106,83,74},
		{785,263,106,84,74},
		{818,268,100,84,75},
		{855,272,99,83,74},
		{901,275,100,76,68},
		{955,280,103,78,69},
		{1016,287,107,80,71},
		{1080,297,112,83,73},
		{1147,309,116,87,75},
		{1221,324,120,90,73},
		{1308,345,124,90,72},
		{1436,397,125,89,73},
		{1673,500,128,87,70},
		{2288,587,156,102,78},
		{1681,361,140,94,75},
		{1886,368,145,95,75},
		{2140,377,148,94,73},
		{2462,388,151,93,73},
		{2877,412,160,96,74},
		{3422,473,174,99,77},
		{4273,562,195,105,81},
		{5469,719,228,112,84},
		{8749,1722,274,120,93},
		{20487,18766,897,170,1139},
	}
};
static struct pc_temp_ocv_lut batt_2100_pc_temp_ocv = {
	.rows = 29,
	.cols = 5,
	.temp = {-20, 0, 25, 40, 60 },
	.percent = {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50,
		    45, 40, 35, 30, 25, 20, 15, 10,
		    9, 8, 7, 6, 5, 4, 3, 2, 1, 0 },
	.ocv = {
		{4331, 4325, 4327, 4317, 4315},
		{4237, 4252, 4261, 4253, 4253},
		{4163, 4190, 4202, 4195, 4196},
		{4091, 4132, 4147, 4140, 4141},
		{4034, 4074, 4093, 4088, 4088},
		{3990, 4017, 4043, 4039, 4039},
		{3952, 3964, 3992, 3993, 3993},
		{3920, 3923, 3937, 3953, 3952},
		{3890, 3887, 3895, 3913, 3914},
		{3863, 3854, 3860, 3864, 3865},
		{3839, 3825, 3831, 3832, 3833},
		{3815, 3804, 3809, 3808, 3809},
		{3794, 3786, 3791, 3789, 3790},
		{3775, 3772, 3775, 3774, 3774},
		{3757, 3758, 3764, 3763, 3755},
		{3739, 3741, 3750, 3747, 3735},
		{3720, 3724, 3725, 3721, 3710},
		{3694, 3707, 3689, 3686, 3673},
		{3654, 3684, 3674, 3669, 3659},
		{3645, 3679, 3670, 3667, 3657},
		{3635, 3673, 3664, 3663, 3653},
		{3620, 3662, 3648, 3655, 3640},
		{3601, 3643, 3612, 3631, 3611},
		{3575, 3611, 3563, 3591, 3570},
		{3538, 3562, 3501, 3540, 3521},
		{3484, 3495, 3430, 3480, 3460},
		{3405, 3403, 3347, 3401, 3377},
		{3284, 3276, 3223, 3280, 3244},
		{3000, 3000, 3000, 3000, 3000}
	}
};

static struct sf_lut batt_2100_pc_sf = {
	.rows = 1,
	.cols = 1,
	.row_entries = {0 },
	.percent = {100 },
	.sf = {
		{100 }
	}
};

/* used in drivers/power/pm8921-bms.c */
struct pm8921_bms_battery_data lge_2100_mako_data =  {
	.fcc = 2100,
	.fcc_temp_lut = &batt_2100_fcc_temp,
	.fcc_sf_lut = &batt_2100_fcc_sf,
	.pc_temp_ocv_lut = &batt_2100_pc_temp_ocv,
	.pc_sf_lut = &batt_2100_pc_sf,
	.rbatt_sf_lut = &batt_2100_rbatt_sf,
	.default_rbatt_mohm = 182,
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

static int batt_temp_charger_enable(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = pm8921_charger_enable(1);
	if (ret)
		pr_err("%s: failed to enable charging\n", __func__);

	return ret;
}

static int batt_temp_charger_disable(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = pm8921_charger_enable(0);
	if (ret)
		pr_err("%s: failed to disable charging\n", __func__);

	return ret;
}

static int batt_temp_ext_power_plugged(void)
{
	if (pm8921_is_usb_chg_plugged_in() ||
			pm8921_is_dc_chg_plugged_in())
		return 1;
	else
		return 0;
}

static int batt_temp_set_current_limit(int value)
{
	int ret = 0;

	pr_info("%s: value = %d\n", __func__, value);

	ret = pm8921_set_max_battery_charge_current(value);
	if (ret)
		pr_err("%s: failed to set i limit\n", __func__);
	return ret;
}

static int batt_temp_get_current_limit(void)
{
	static struct power_supply *psy;
	union power_supply_propval ret = {0,};
	int rc = 0;

	if (psy == NULL) {
		psy = power_supply_get_by_name("usb");
		if (!psy) {
			pr_err("%s: failed to get usb power supply\n", __func__);
			return 0;
		}
	}

	rc = psy->get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
	if (rc) {
		pr_err("%s: failed to get usb property\n", __func__);
		return 0;
	}
	pr_info("%s: value = %d\n", __func__, ret.intval);
	return ret.intval;
}

static int batt_temp_set_state(int health, int i_value)
{
	int ret = 0;

	ret = pm8921_set_ext_battery_health(health, i_value);
	if (ret)
		pr_err("%s: failed to set health\n", __func__);

	return ret;
}

static struct batt_temp_pdata mako_batt_temp_pada = {
	.set_chg_i_limit = batt_temp_set_current_limit,
	.get_chg_i_limit = batt_temp_get_current_limit,
	.set_health_state = batt_temp_set_state,
	.enable_charging = batt_temp_charger_enable,
	.disable_charging = batt_temp_charger_disable,
	.is_ext_power = batt_temp_ext_power_plugged,
	.update_time = 10000, // 10 sec
	.temp_level = batt_temp_ctrl_level,
	.temp_nums = ARRAY_SIZE(batt_temp_ctrl_level),
	.thr_mvolt = 4000, //4.0V
	.i_decrease = WARM_BATT_CHG_I_MA,
	.i_restore = MAX_BATT_CHG_I_MA,
};

struct platform_device batt_temp_ctrl = {
	.name = "batt_temp_ctrl",
	.id = -1,
	.dev = {
		.platform_data = &mako_batt_temp_pada,
	},
};

void __init mako_set_adcmap(void)
{
	pm8xxx_set_adcmap_btm_threshold(adcmap_btm_threshold,
			ARRAY_SIZE(adcmap_btm_threshold));
	pm8xxx_set_adcmap_pa_therm(adcmap_pa_therm,
			ARRAY_SIZE(adcmap_pa_therm));
	/*
	pm8xxx_set_adcmap_ntcg_104ef_104fb(adcmap_ntcg_104ef_104fb,
			ARRAY_SIZE(adcmap_ntcg_104ef_104fb));
	*/
}

void __init apq8064_init_pmic(void)
{
	pmic_reset_irq = PM8921_IRQ_BASE + PM8921_RESOUT_IRQ;

	mako_fixed_keymap();
	mako_set_adcmap();
	mako_fixed_leds();
	mako_fixup_wlc_gpio();

	apq8064_device_ssbi_pmic1.dev.platform_data =
		&apq8064_ssbi_pm8921_pdata;
	apq8064_device_ssbi_pmic2.dev.platform_data =
		&apq8064_ssbi_pm8821_pdata;
	apq8064_pm8921_platform_data.num_regulators =
		msm8064_pm8921_regulator_pdata_len;
}
