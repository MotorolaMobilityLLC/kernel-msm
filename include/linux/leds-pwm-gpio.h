/*
 * PWM GPIO LED driver data - see drivers/leds/leds-pwm-gpio.c
 */
#ifndef __LINUX_LEDS_PWM_H
#define __LINUX_LEDS_PWM_H

struct led_pwm_gpio {
	const char	*name;
	const char	*default_trigger;
	unsigned	gpio;
	unsigned	pwm_id;
	unsigned	active_low:1;
	unsigned	retain_state_suspended:1;
	unsigned	default_state:2;
};

struct led_pwm_gpio_platform_data {
	int num_leds;
	struct led_pwm_gpio *leds;
	int max_brightness;
};

#endif
