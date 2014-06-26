/*
 * LP855x Backlight Driver
 *
 *			Copyright (C) 2011 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _LP855X_H
#define _LP855X_H

extern struct lp855x *lpdata;
int lp855x_ext_write_byte(u8 reg, u8 data);
int lp855x_ext_read_byte(u8 reg);

#define BL_CTL_SHFT	(0)
#define BRT_MODE_SHFT	(1)
#define BRT_MODE_MASK	(0x06)

/* Enable backlight. Only valid when BRT_MODE=10(I2C only) */
#define ENABLE_BL	(1)
#define DISABLE_BL	(0)

#define I2C_CONFIG(id)	id ## _I2C_CONFIG
#define PWM_CONFIG(id)	id ## _PWM_CONFIG

/* DEVICE CONTROL register - LP8550 */
#define LP8550_PWM_CONFIG	(LP8550_PWM_ONLY << BRT_MODE_SHFT)
#define LP8550_I2C_CONFIG	((ENABLE_BL << BL_CTL_SHFT) | \
				(LP8550_I2C_ONLY << BRT_MODE_SHFT))

/* DEVICE CONTROL register - LP8551 */
#define LP8551_PWM_CONFIG	LP8550_PWM_CONFIG
#define LP8551_I2C_CONFIG	LP8550_I2C_CONFIG

/* DEVICE CONTROL register - LP8552 */
#define LP8552_PWM_CONFIG	LP8550_PWM_CONFIG
#define LP8552_I2C_CONFIG	LP8550_I2C_CONFIG

/* DEVICE CONTROL register - LP8553 */
#define LP8553_PWM_CONFIG	LP8550_PWM_CONFIG
#define LP8553_I2C_CONFIG	LP8550_I2C_CONFIG

/* DEVICE CONTROL register - LP8556 */
#define LP8556_PWM_CONFIG	(LP8556_PWM_ONLY << BRT_MODE_SHFT)
#define LP8556_COMB1_CONFIG	(LP8556_COMBINED1 << BRT_MODE_SHFT)
#define LP8556_I2C_CONFIG	((ENABLE_BL << BL_CTL_SHFT) | \
				(LP8556_I2C_ONLY << BRT_MODE_SHFT))
#define LP8556_COMB2_CONFIG	(LP8556_COMBINED2 << BRT_MODE_SHFT)
#define LP8556_FAST_CONFIG	BIT(7) /* use it if EPROMs should be maintained
					  when exiting the low power mode */
#define	LP8556_5LEDSTR		0x1F  /* 5 led string definition for ffrd8 and bytcr-rvp boards.*/

#define	LP8556_LEDSTREN		0x16
#define LP8556_CFG98		0x98
#define LP8556_CFG9E		0x9E
#define LP8556_CFG0		0xA0
#define LP8556_CFG1		0xA1
#define LP8556_CFG2		0xA2
#define LP8556_CFG3		0xA3
#define LP8556_CFG4		0xA4
#define LP8556_CFG5		0xA5
	#define LP8556_PWM_DRECT_EN	0x80
	#define LP8556_PWM_DRECT_DIS	0x00
	#define LP8556_PS_MODE_6P6D	0x00
	#define LP8556_PS_MODE_5P5D	0x10
	#define LP8556_PS_MODE_4P4D	0x20
	#define LP8556_PS_MODE_3P3D	0x30
	#define LP8556_PS_MODE_2P2D	0x40
	#define LP8556_PS_MODE_3P6D	0x50
	#define LP8556_PS_MODE_2P6D	0x60
	#define LP8556_PS_MODE_1P6D	0x70
	#define LP8556_PWM_FREQ_4808HZ	0x00
	#define LP8556_PWM_FREQ_6010HZ	0x01
	#define LP8556_PWM_FREQ_7212HZ	0x02
	#define LP8556_PWM_FREQ_8414HZ	0x03
	#define LP8556_PWM_FREQ_9616HZ	0x04
	#define LP8556_PWM_FREQ_12020HZ	0x05
	#define LP8556_PWM_FREQ_13222HZ	0x06
	#define LP8556_PWM_FREQ_14424HZ	0x07
	#define LP8556_PWM_FREQ_15626HZ	0x08
	#define LP8556_PWM_FREQ_16828HZ	0x09
	#define LP8556_PWM_FREQ_18030HZ	0x0A
	#define LP8556_PWM_FREQ_19232HZ	0x0B
	#define LP8556_PWM_FREQ_24040HZ	0x0C
	#define LP8556_PWM_FREQ_28848HZ	0x0D
	#define LP8556_PWM_FREQ_33656HZ	0x0E
	#define LP8556_PWM_FREQ_38464HZ	0x0F
#define LP8556_CFG6		0xA6
#define LP8556_CFG7		0xA7
	#define LP8556_RSRVD_76	0xC0
	#define LP8556_DRV3_EN	0x20
	#define LP8556_DRV3_DIS	0x00
	#define LP8556_DRV2_EN	0x10
	#define LP8556_DRV2_DIS	0x00
	#define LP8556_RSRVD_32	0x0C
	#define LP8556_IBOOST_LIM_0_9A_1_6A	0x00
	#define LP8556_IBOOST_LIM_1_2A_2_1A	0x01
	#define LP8556_IBOOST_LIM_1_5A_2_6A	0x02
	#define LP8556_IBOOST_LIM_1_8A_NA	0x03
#define LP8556_CFG8		0xA8
#define LP8556_CFG9		0xA9
	#define LP8556_VBOOST_MAX_NA_21V	0x40
	#define LP8556_VBOOST_MAX_NA_25V	0x60
	#define LP8556_VBOOST_MAX_21V_30V	0x80
	#define LP8556_VBOOST_MAX_25V_34_5V	0xA0
	#define LP8556_VBOOST_MAX_30V_39V	0xC0
	#define LP8556_VBOOST_MAX_34V_43V	0xE0
	#define LP8556_JUMP_EN			0x10
	#define LP8556_JUMP_DIS			0x00
	#define LP8556_JMP_TSHOLD_10P		0x00
	#define LP8556_JMP_TSHOLD_30P		0x04
	#define LP8556_JMP_TSHOLD_50P		0x08
	#define LP8556_JMP_TSHOLD_70P		0x0C
	#define LP8556_JMP_VOLT_0_5V		0x00
	#define LP8556_JMP_VOLT_1V		0x01
	#define LP8556_JMP_VOLT_2V		0x02
	#define LP8556_JMP_VOLT_4V		0x03
#define LP8556_CFGA		0xAA
#define LP8556_CFGB		0xAB
#define LP8556_CFGC		0xAC
#define LP8556_CFGD		0xAD
#define LP8556_CFGE		0xAE
#define LP8556_CFGF		0xAF

/* CONFIG register - LP8557 */
#define LP8557_PWM_STANDBY	BIT(7)
#define LP8557_PWM_FILTER	BIT(6)
#define LP8557_RELOAD_EPROM	BIT(3)	/* use it if EPROMs should be reset
					   when the backlight turns on */
#define LP8557_OFF_OPENLEDS	BIT(2)
#define LP8557_PWM_CONFIG	LP8557_PWM_ONLY
#define LP8557_I2C_CONFIG	LP8557_I2C_ONLY
#define LP8557_COMB1_CONFIG	LP8557_COMBINED1
#define LP8557_COMB2_CONFIG	LP8557_COMBINED2

enum lp855x_chip_id {
	LP8550,
	LP8551,
	LP8552,
	LP8553,
	LP8556,
	LP8557,
};

enum lp8550_brighntess_source {
	LP8550_PWM_ONLY,
	LP8550_I2C_ONLY = 2,
};

enum lp8551_brighntess_source {
	LP8551_PWM_ONLY = LP8550_PWM_ONLY,
	LP8551_I2C_ONLY = LP8550_I2C_ONLY,
};

enum lp8552_brighntess_source {
	LP8552_PWM_ONLY = LP8550_PWM_ONLY,
	LP8552_I2C_ONLY = LP8550_I2C_ONLY,
};

enum lp8553_brighntess_source {
	LP8553_PWM_ONLY = LP8550_PWM_ONLY,
	LP8553_I2C_ONLY = LP8550_I2C_ONLY,
};

enum lp8556_brightness_source {
	LP8556_PWM_ONLY,
	LP8556_COMBINED1,	/* pwm + i2c before the shaper block */
	LP8556_I2C_ONLY,
	LP8556_COMBINED2,	/* pwm + i2c after the shaper block */
};

enum lp8557_brightness_source {
	LP8557_PWM_ONLY,
	LP8557_I2C_ONLY,
	LP8557_COMBINED1,	/* pwm + i2c after the shaper block */
	LP8557_COMBINED2,	/* pwm + i2c before the shaper block */
};

struct lp855x_rom_data {
	u8 addr;
	u8 val;
};

/**
 * struct lp855x_platform_data
 * @name : Backlight driver name. If it is not defined, default name is set.
 * @device_control : value of DEVICE CONTROL register
 * @initial_brightness : initial value of backlight brightness
 * @period_ns : platform specific pwm period value. unit is nano.
		Only valid when mode is PWM_BASED.
 * @size_program : total size of lp855x_rom_data
 * @rom_data : list of new eeprom/eprom registers
 */
struct lp855x_platform_data {
	const char *name;
	u8 device_control;
	u8 initial_brightness;
	unsigned int period_ns;
	int size_program;
	struct lp855x_rom_data *rom_data;
};

#endif
