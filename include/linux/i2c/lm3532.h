/*
 * Backlight drivers LM3532
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef __LINUX_I2C_LM3532_H
#define __LINUX_I2C_LM3532_H

#define ID_LM3532		3532

#define LM3532_MAX_BRIGHTNESS	0xFF

/*
 * Part configuration platform data
 */

#define LM3532_UNUSED_DEVICE    0
#define LM3532_BACKLIGHT_DEVICE 1
#define LM3532_LED_DEVICE       2  /* LED not part of the boost control loop */
#define LM3532_LED_DEVICE_FDBCK 3  /* LED in the boost control loop */

#define LM3532_CNTRL_A		0
#define LM3532_CNTRL_B		1
#define LM3532_CNTRL_C		2

/*
 * Backlight subdevice platform data
 */

#define LM3532_RAMP_STEP_8us	0
#define LM3532_RAMP_STEP_1ms	1
#define LM3532_RAMP_STEP_2ms	2
#define LM3532_RAMP_STEP_4ms	3
#define LM3532_RAMP_STEP_8ms	4
#define LM3532_RAMP_STEP_16ms	5
#define LM3532_RAMP_STEP_32ms	6
#define LM3532_RAMP_STEP_65ms	7

#define LM3532_FULL_SCALE_CUR_uA(I)	((I - 5000) / 800)

#define LM3532_RESISTOR_SLCT_HIGH_IMP	0
#define LM3532_RESISTOR_SLCT_37000Ohms	1
#define LM3532_RESISTOR_SLCT_18500Ohms	2
#define LM3532_RESISTOR_SLCT_12330Ohms	3
#define LM3532_RESISTOR_SLCT_9250Ohms	4
#define LM3532_RESISTOR_SLCT_7400Ohms	5
#define LM3532_RESISTOR_SLCT_6170Ohms	6
#define LM3532_RESISTOR_SLCT_5290Ohms	7
#define LM3532_RESISTOR_SLCT_4630Ohms	8
#define LM3532_RESISTOR_SLCT_4110Ohms	9
#define LM3532_RESISTOR_SLCT_3700Ohms	10
#define LM3532_RESISTOR_SLCT_3360Ohms	11
#define LM3532_RESISTOR_SLCT_3080Ohms	12
#define LM3532_RESISTOR_SLCT_2850Ohms	13
#define LM3532_RESISTOR_SLCT_2640Ohms	14
#define LM3532_RESISTOR_SLCT_2470Ohms	15
#define LM3532_RESISTOR_SLCT_2310Ohms	16
#define LM3532_RESISTOR_SLCT_2180Ohms	17
#define LM3532_RESISTOR_SLCT_2060Ohms	18
#define LM3532_RESISTOR_SLCT_1950Ohms	19
#define LM3532_RESISTOR_SLCT_1850Ohms	20
#define LM3532_RESISTOR_SLCT_1760Ohms	21
#define LM3532_RESISTOR_SLCT_1680Ohms	22
#define LM3532_RESISTOR_SLCT_1610Ohms	23
#define LM3532_RESISTOR_SLCT_1540Ohms	24
#define LM3532_RESISTOR_SLCT_1480Ohms	25
#define LM3532_RESISTOR_SLCT_1420Ohms	26
#define LM3532_RESISTOR_SLCT_1370Ohms	27
#define LM3532_RESISTOR_SLCT_1320Ohms	28
#define LM3532_RESISTOR_SLCT_1280Ohms	29
#define LM3532_RESISTOR_SLCT_1230Ohms	30
#define LM3532_RESISTOR_SLCT_1190Ohms	31

#define LM3532_NAME_SIZE 64

struct lm3532_backlight_platform_data {
	int (*power_up)(void);
	int (*power_down)(void);
	int (*reset_assert)(void);
	int (*reset_release)(void);

	u8 led1_controller;
	u8 led2_controller;
	u8 led3_controller;

	u8 ctrl_a_usage;
	u8 ctrl_b_usage;
	u8 ctrl_c_usage;

	u8 susd_ramp;
	u8 runtime_ramp;

	u8 als1_res;
	u8 als2_res;
	u8 als_dwn_delay;
	u8 als_cfgr;

	u8 en_ambl_sens;	/* 1 = enable ambient light sensor */

	int pwm_init_delay_ms;
	int pwm_resume_delay_ms;
	int pwm_disable_threshold;

	u8 ctrl_a_pwm;
	u8 ctrl_b_pwm;
	u8 ctrl_c_pwm;

	u8 ctrl_a_fsc;
	u8 ctrl_b_fsc;
	u8 ctrl_c_fsc;

	u8 ctrl_a_l4_daylight;
	u8 ctrl_a_l3_bright;
	u8 ctrl_a_l2_office;
	u8 ctrl_a_l1_indoor;
	u8 ctrl_a_l0_dark;

	u8 ctrl_b_l4_daylight;
	u8 ctrl_b_l3_bright;
	u8 ctrl_b_l2_office;
	u8 ctrl_b_l1_indoor;
	u8 ctrl_b_l0_dark;

	u8 ctrl_c_l4_daylight;
	u8 ctrl_c_l3_bright;
	u8 ctrl_c_l2_office;
	u8 ctrl_c_l1_indoor;
	u8 ctrl_c_l0_dark;

	u8 l4_high;
	u8 l4_low;
	u8 l3_high;
	u8 l3_low;
	u8 l2_high;
	u8 l2_low;
	u8 l1_high;
	u8 l1_low;

	char ctrl_a_name[LM3532_NAME_SIZE];
	char ctrl_b_name[LM3532_NAME_SIZE];
	char ctrl_c_name[LM3532_NAME_SIZE];

	u8 boot_brightness;
};

enum lm3532_display_connected_state {
	LM3522_STATE_UNKNOWN = 0,
	LM3522_STATE_DISCONNECTED,
	LM3522_STATE_CONNECTED,
};

#endif /* __LINUX_I2C_LM3532_H */
