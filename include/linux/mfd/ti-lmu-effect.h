/*
 * TI LMU(Lighting Management Unit) Effect Driver
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __MFD_TI_LMU_EFFECT_H__
#define __MFD_TI_LMU_EFFECT_H__

#include <linux/mfd/ti-lmu.h>

/*
 * LMU effect IO resource structure
 *
 *	shift[23:16] | mask[15:8] | address[7:0]
 *
 * All TI LMU device is 8-bit register base. address, mask and shift are 8-bit.
 * To activate a light effect, updating registers is required.
 * However, register information depends on the device.
 * In other words, each chip driver should manage own its register address,
 * mask and shift bits.
 * To make it simpler, registers are configured with IORESOURCE_REG.
 * LMU uses bit combination for efficient IO resource.
 *
 * LMU MFD    : Register configuration by combining address/mask/shift.
 *              XXX_LMU_EFFECT_REGISTER() macros are used.
 * LMU effect : Get address/mask/shift by using platform_get_resource().
 *              LMU_EFFECT_GET_XXX() macros are used.
 */

#define _LMU_EFFECT_REGISTER(addr, mask, shift)		\
	((shift << 16) | (mask << 8) | addr)

#define LMU_EFFECT_REGISTER(chip, effect)		\
	_LMU_EFFECT_REGISTER(chip##_REG_##effect,	\
			    chip##_##effect##_MASK,	\
			    chip##_##effect##_SHIFT)	\

#define LM3532_EFFECT_REGISTER(effect)			\
	LMU_EFFECT_REGISTER(LM3532, effect)
#define LM3631_EFFECT_REGISTER(effect)			\
	LMU_EFFECT_REGISTER(LM3631, effect)
#define LM3633_EFFECT_REGISTER(effect)			\
	LMU_EFFECT_REGISTER(LM3633, effect)
#define LM3697_EFFECT_REGISTER(effect)			\
	LMU_EFFECT_REGISTER(LM3697, effect)

#define LMU_EFFECT_GET_ADDR(x)			(x & 0xFF)
#define LMU_EFFECT_GET_MASK(x)			((x >> 8) & 0xFF)
#define LMU_EFFECT_GET_SHIFT(x)			((x >> 16) & 0xFF)

#define LM3532_EFFECT_RAMPUP			"lm3532:ramp_up"
#define LM3532_EFFECT_RAMPDOWN			"lm3532:ramp_down"
#define LM3631_EFFECT_SLOPE			"lm3631:slope"
#define LM3633_EFFECT_BL0_RAMPUP		"lm3633_bl:0:ramp_up"
#define LM3633_EFFECT_BL0_RAMPDOWN		"lm3633_bl:0:ramp_down"
#define LM3633_EFFECT_BL1_RAMPUP		"lm3633_bl:1:ramp_up"
#define LM3633_EFFECT_BL1_RAMPDOWN		"lm3633_bl:1:ramp_down"
#define LM3633_EFFECT_PTN_DELAY			"lm3633_ptn:delay"
#define LM3633_EFFECT_PTN_HIGHTIME		"lm3633_ptn:high_time"
#define LM3633_EFFECT_PTN_LOWTIME		"lm3633_ptn:low_time"
#define LM3633_EFFECT_PTN0_RAMPUP		"lm3633_ptn:0:ramp_up"
#define LM3633_EFFECT_PTN0_RAMPDOWN		"lm3633_ptn:0:ramp_down"
#define LM3633_EFFECT_PTN1_RAMPUP		"lm3633_ptn:1:ramp_up"
#define LM3633_EFFECT_PTN1_RAMPDOWN		"lm3633_ptn:1:ramp_down"
#define LM3633_EFFECT_PTN_LOWBRT		"lm3633_ptn:low_brt"
#define LM3633_EFFECT_PTN_HIGHBRT		"lm3633_ptn:high_brt"
#define LM3697_EFFECT_BL0_RAMPUP		"lm3697:0:ramp_up"
#define LM3697_EFFECT_BL0_RAMPDOWN		"lm3697:0:ramp_down"
#define LM3697_EFFECT_BL1_RAMPUP		"lm3697:1:ramp_up"
#define LM3697_EFFECT_BL1_RAMPDOWN		"lm3697:1:ramp_down"

enum lmu_effect_request_id {
	/* Backlight effect */
	BL_EFFECT_RAMPUP,
	BL_EFFECT_RAMPDN,

	/* LED pattern effect */
	LED_EFFECT_DELAY,
	LED_EFFECT_HIGHTIME,
	LED_EFFECT_LOWTIME,
	LED_EFFECT_PTN0_RAMPUP,
	LED_EFFECT_PTN0_RAMPDN,
	LED_EFFECT_PTN1_RAMPUP,
	LED_EFFECT_PTN1_RAMPDN,
	LED_EFFECT_LOWBRT,
	LED_EFFECT_HIGHBRT,
};

struct ti_lmu_effect;

typedef void (ti_lmu_effect_cb_t)(struct ti_lmu_effect *, int, void *);

int ti_lmu_effect_request(const char *name, ti_lmu_effect_cb_t cbfunc,
			  int req_id, void *data);
int ti_lmu_effect_set_ramp(struct ti_lmu_effect *lmu_effect, int msec);
int ti_lmu_effect_set_time(struct ti_lmu_effect *lmu_effect, int msec,
			   u8 reg_offset);
int ti_lmu_effect_set_level(struct ti_lmu_effect *lmu_effect, u8 val,
			    u8 reg_offset);

#endif
