/*
 * clearwater-tables.c  --  data tables for CLEARWATER class codecs
 *
 * Copyright 2014 Wolfson Microelectronics plc
 *
 * Author: Nariman Poushin <nariman@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>
#include <linux/device.h>

#include "arizona.h"

static const struct reg_default clearwater_reva_16_patch[] = {
	{ 0x80,  0x0003 },
	{ 0x213, 0x03E4 },
	{ 0x177, 0x0281 },
	{ 0x197, 0x0281 },
	{ 0x1B7, 0x0281 },
	{ 0x4B1, 0x010A },
	{ 0x4CF, 0x0933 },
	{ 0x36C, 0x011B },
	{ 0x4B8, 0x1120 },
	{ 0x4A0, 0x3280 },
	{ 0x4A1, 0x3200 },
	{ 0x4A2, 0x3200 },
	{ 0x441, 0xC050 },
	{ 0x4A4, 0x000B },
	{ 0x4A5, 0x000B },
	{ 0x4A6, 0x000B },
	{ 0x4E2, 0x1E1D },
	{ 0x4E3, 0x1E1D },
	{ 0x4E4, 0x1E1D },
	{ 0x293, 0x0080 },
	{ 0x17D, 0x0303 },
	{ 0x19D, 0x0303 },
	{ 0x27E, 0x0000 },
	{ 0x80,  0x0000 },
	{ 0x80,  0x0000 },
};

static const struct reg_default clearwater_revc_16_patch[] = {
	{ 0x27E, 0x0000 },
	{ 0x2C2, 0x5 },
};

/* this patch is required for EDRE on RevA*/
static const struct reg_default clearwater_reva_32_patch[] = {
	{  0x3000, 0xC2253632},
	{  0x3002, 0xC2300001},
	{  0x3004, 0x8225100E},
	{  0x3006, 0x22251803},
	{  0x3008, 0x82310B00},
	{  0x300A, 0xE231023B},
	{  0x300C, 0x2313B01},
	{  0x300E, 0x62300000},
	{  0x3010, 0xE2314288},
	{  0x3012, 0x2310B00},
	{  0x3014, 0x2310B00},
	{  0x3016, 0x4050100},
	{  0x3018, 0x42310C02},
	{  0x301A, 0xE2310227},
	{  0x301C, 0x2313B01},
	{  0x301E, 0xE2314266},
	{  0x3020, 0xE2315294},
	{  0x3022, 0x2310B00},
	{  0x3024, 0x2310B00},
	{  0x3026, 0x2251100},
	{  0x3028, 0x2251401},
	{  0x302A, 0x2250200},
	{  0x302C, 0x2251001},
	{  0x302E, 0x2250200},
	{  0x3030, 0xE2310266},
	{  0x3032, 0x82314B15},
	{  0x3034, 0x82310B15},
	{  0x3036, 0xE2315294},
	{  0x3038, 0x2310B00},
	{  0x303A, 0x8225160D},
	{  0x303C, 0x225F501},
	{  0x303E, 0x8225061C},
	{  0x3040, 0x2251000},
	{  0x3042, 0x4051101},
	{  0x3044, 0x2251800},
	{  0x3046, 0x42251203},
	{  0x3048, 0x2251101},
	{  0x304A, 0xC2251300},
	{  0x304C, 0x2225FB02},
	{  0x3050, 0xC2263632},
	{  0x3052, 0xC2300001},
	{  0x3054, 0x8226100E},
	{  0x3056, 0x22261803},
	{  0x3058, 0x82310B02},
	{  0x305A, 0xE231023B},
	{  0x305C, 0x2313B01},
	{  0x305E, 0x62300000},
	{  0x3060, 0xE2314288},
	{  0x3062, 0x2310B00},
	{  0x3064, 0x2310B00},
	{  0x3066, 0x4050000},
	{  0x3068, 0x42310C03},
	{  0x306A, 0xE2310227},
	{  0x306C, 0x2313B01},
	{  0x306E, 0xE2314266},
	{  0x3070, 0xE2315294},
	{  0x3072, 0x2310B00},
	{  0x3074, 0x2310B00},
	{  0x3076, 0x2261100},
	{  0x3078, 0x2261401},
	{  0x307A, 0x2260200},
	{  0x307C, 0x2261001},
	{  0x307E, 0x2260200},
	{  0x3080, 0xE2310266},
	{  0x3082, 0x82314B17},
	{  0x3084, 0x82310B17},
	{  0x3086, 0xE2315294},
	{  0x3088, 0x2310B00},
	{  0x308A, 0x8226160D},
	{  0x308C, 0x226F501},
	{  0x308E, 0x8226061C},
	{  0x3090, 0x2261000},
	{  0x3092, 0x4051101},
	{  0x3094, 0x2261800},
	{  0x3096, 0x42261203},
	{  0x3098, 0x2261101},
	{  0x309A, 0xC2261300},
	{  0x309C, 0x2226FB02},
	{  0x309E, 0xF000},
	{  0x30A0, 0xC2273632},
	{  0x30A2, 0xC2400001},
	{  0x30A4, 0x8227100E},
	{  0x30A6, 0x22271803},
	{  0x30A8, 0x82410B00},
	{  0x30AA, 0xE241023B},
	{  0x30AC, 0x2413B01},
	{  0x30AE, 0x62400000},
	{  0x30B0, 0xE2414288},
	{  0x30B2, 0x2410B00},
	{  0x30B4, 0x2410B00},
	{  0x30B6, 0x4050300},
	{  0x30B8, 0x42410C02},
	{  0x30BA, 0xE2410227},
	{  0x30BC, 0x2413B01},
	{  0x30BE, 0xE2414266},
	{  0x30C0, 0xE2415294},
	{  0x30C2, 0x2410B00},
	{  0x30C4, 0x2410B00},
	{  0x30C6, 0x2271100},
	{  0x30C8, 0x2271401},
	{  0x30CA, 0x2270200},
	{  0x30CC, 0x2271001},
	{  0x30CE, 0x2270200},
	{  0x30D0, 0xE2410266},
	{  0x30D2, 0x82414B15},
	{  0x30D4, 0x82410B15},
	{  0x30D6, 0xE2415294},
	{  0x30D8, 0x2410B00},
	{  0x30DA, 0x8227160D},
	{  0x30DC, 0x227F501},
	{  0x30DE, 0x8227061C},
	{  0x30E0, 0x2271000},
	{  0x30E2, 0x4051101},
	{  0x30E4, 0x2271800},
	{  0x30E6, 0x42271203},
	{  0x30E8, 0x2271101},
	{  0x30EA, 0xC2271300},
	{  0x30EC, 0x2227FB02},
	{  0x30F0, 0xC2283632},
	{  0x30F2, 0xC2400001},
	{  0x30F4, 0x8228100E},
	{  0x30F6, 0x22281803},
	{  0x30F8, 0x82410B02},
	{  0x30FA, 0xE241023B},
	{  0x30FC, 0x2413B01},
	{  0x30FE, 0x62400000},
	{  0x3100, 0xE2414288},
	{  0x3102, 0x2410B00},
	{  0x3104, 0x2410B00},
	{  0x3106, 0x4050200},
	{  0x3108, 0x42410C03},
	{  0x310A, 0xE2410227},
	{  0x310C, 0x2413B01},
	{  0x310E, 0xE2414266},
	{  0x3110, 0xE2415294},
	{  0x3112, 0x2410B00},
	{  0x3114, 0x2410B00},
	{  0x3116, 0x2281100},
	{  0x3118, 0x2281401},
	{  0x311A, 0x2280200},
	{  0x311C, 0x2281001},
	{  0x311E, 0x2280200},
	{  0x3120, 0xE2410266},
	{  0x3122, 0x82414B17},
	{  0x3124, 0x82410B17},
	{  0x3126, 0xE2415294},
	{  0x3128, 0x2410B00},
	{  0x312A, 0x8228160D},
	{  0x312C, 0x228F501},
	{  0x312E, 0x8228061C},
	{  0x3130, 0x2281000},
	{  0x3132, 0x4051101},
	{  0x3134, 0x2281800},
	{  0x3136, 0x42281203},
	{  0x3138, 0x2281101},
	{  0x313A, 0xC2281300},
	{  0x313C, 0x2228FB02},
	{  0x3140, 0xC2293632},
	{  0x3142, 0xC2500001},
	{  0x3144, 0x8229100E},
	{  0x3146, 0x22291803},
	{  0x3148, 0x82510B00},
	{  0x314A, 0xE251023B},
	{  0x314C, 0x2513B01},
	{  0x314E, 0x62500000},
	{  0x3150, 0xE2514288},
	{  0x3152, 0x2510B00},
	{  0x3154, 0x2510B00},
	{  0x3156, 0x4050500},
	{  0x3158, 0x42510C02},
	{  0x315A, 0xE2510227},
	{  0x315C, 0x2513B01},
	{  0x315E, 0xE2514266},
	{  0x3160, 0xE2515294},
	{  0x3162, 0x2510B00},
	{  0x3164, 0x2510B00},
	{  0x3166, 0x2291100},
	{  0x3168, 0x2291401},
	{  0x316A, 0x2290200},
	{  0x316C, 0x2291001},
	{  0x316E, 0x2290200},
	{  0x3170, 0xE2510266},
	{  0x3172, 0x82514B15},
	{  0x3174, 0x82510B15},
	{  0x3176, 0xE2515294},
	{  0x3178, 0x2510B00},
	{  0x317A, 0x8229160D},
	{  0x317C, 0x229F501},
	{  0x317E, 0x8229061C},
	{  0x3180, 0x2291000},
	{  0x3182, 0x4051101},
	{  0x3184, 0x2291800},
	{  0x3186, 0x42291203},
	{  0x3188, 0x2291101},
	{  0x318A, 0xC2291300},
	{  0x318C, 0x2229FB02},
	{  0x3190, 0xC22A3632},
	{  0x3192, 0xC2500001},
	{  0x3194, 0x822A100E},
	{  0x3196, 0x222A1803},
	{  0x3198, 0x82510B02},
	{  0x319A, 0xE251023B},
	{  0x319C, 0x2513B01},
	{  0x319E, 0x62500000},
	{  0x31A0, 0xE2514288},
	{  0x31A2, 0x2510B00},
	{  0x31A4, 0x2510B00},
	{  0x31A6, 0x4050400},
	{  0x31A8, 0x42510C03},
	{  0x31AA, 0xE2510227},
	{  0x31AC, 0x2513B01},
	{  0x31AE, 0xE2514266},
	{  0x31B0, 0xE2515294},
	{  0x31B2, 0x2510B00},
	{  0x31B4, 0x2510B00},
	{  0x31B6, 0x22A1100},
	{  0x31B8, 0x22A1401},
	{  0x31BA, 0x22A0200},
	{  0x31BC, 0x22A1001},
	{  0x31BE, 0x22A0200},
	{  0x31C0, 0xE2510266},
	{  0x31C2, 0x82514B17},
	{  0x31C4, 0x82510B17},
	{  0x31C6, 0xE2515294},
	{  0x31C8, 0x2510B00},
	{  0x31CA, 0x822A160D},
	{  0x31CC, 0x22AF501},
	{  0x31CE, 0x822A061C},
	{  0x31D0, 0x22A1000},
	{  0x31D2, 0x4051101},
	{  0x31D4, 0x22A1800},
	{  0x31D6, 0x422A1203},
	{  0x31D8, 0x22A1101},
	{  0x31DA, 0xC22A1300},
	{  0x31DC, 0x222AFB02},
};

static const struct reg_default clearwater_revc_32_patch[] = {
	{ 0x3380, 0xE4103066 },
	{ 0x3382, 0xE4103070 },
	{ 0x3384, 0xE4103078 },
	{ 0x3386, 0xE4103080 },
	{ 0x3388, 0xE410F080 },
	{ 0x338A, 0xE4143066 },
	{ 0x338C, 0xE4143070 },
	{ 0x338E, 0xE4143078 },
	{ 0x3390, 0xE4143080 },
	{ 0x3392, 0xE414F080 },
	{ 0x3394, 0xE4103078 },
	{ 0x3396, 0xE4103070 },
	{ 0x3398, 0xE4103066 },
	{ 0x339A, 0xE410F056 },
	{ 0x339C, 0xE4143078 },
	{ 0x339E, 0xE4143070 },
	{ 0x33A0, 0xE4143066 },
	{ 0x33A2, 0xE414F056 },
};

/* We use a function so we can use ARRAY_SIZE() */
int clearwater_patch(struct arizona *arizona)
{
	int ret = 0;
	const struct reg_default *patch16 = NULL;
	const struct reg_default *patch32 = NULL;
	unsigned int num16, num32;

	switch (arizona->rev) {
	case 0:
	case 1:
		patch16 = clearwater_reva_16_patch;
		num16 = ARRAY_SIZE(clearwater_reva_16_patch);

		patch32 = clearwater_reva_32_patch;
		num32 = ARRAY_SIZE(clearwater_reva_32_patch);
		break;
	default:
		patch16 = clearwater_revc_16_patch;
		num16 = ARRAY_SIZE(clearwater_revc_16_patch);

		patch32 = clearwater_revc_32_patch;
		num32 = ARRAY_SIZE(clearwater_revc_32_patch);
		break;
	}

	if (patch16) {
		ret = regmap_register_patch(arizona->regmap, patch16, num16);
		if (ret < 0) {
			dev_err(arizona->dev,
				"Error in applying 16-bit patch: %d\n", ret);
			return ret;
		}
	}

	if (patch32) {
		ret = regmap_register_patch(arizona->regmap_32bit,
					    patch32, num32);
		if (ret < 0) {
			dev_err(arizona->dev,
				"Error in applying 32-bit patch: %d\n", ret);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(clearwater_patch);

static const struct regmap_irq clearwater_irqs[ARIZONA_NUM_IRQ] = {
	[ARIZONA_IRQ_BOOT_DONE] = { .reg_offset = 0,
				  .mask = CLEARWATER_BOOT_DONE_EINT1 },
	[ARIZONA_IRQ_CTRLIF_ERR] = { .reg_offset = 0,
				  .mask = CLEARWATER_CTRLIF_ERR_EINT1 },

	[ARIZONA_IRQ_FLL1_CLOCK_OK] = { .reg_offset = 1,
				  .mask = CLEARWATER_FLL1_LOCK_EINT1 },
	[ARIZONA_IRQ_FLL2_CLOCK_OK] = { .reg_offset = 1,
				  .mask = CLEARWATER_FLL2_LOCK_EINT1},
	[ARIZONA_IRQ_FLL3_CLOCK_OK] = { .reg_offset = 1,
				  .mask = CLEARWATER_FLL3_LOCK_EINT1},

	[ARIZONA_IRQ_MICDET] = { .reg_offset = 5,
				  .mask = CLEARWATER_MICDET_EINT1 },
	[ARIZONA_IRQ_HPDET] = { .reg_offset = 5,
				  .mask = CLEARWATER_HPDET_EINT1},

	[ARIZONA_IRQ_MICD_CLAMP_RISE] = { .reg_offset = 6,
				  .mask = CLEARWATER_MICD_CLAMP_RISE_EINT1 },
	[ARIZONA_IRQ_MICD_CLAMP_FALL] = { .reg_offset = 6,
				  .mask = CLEARWATER_MICD_CLAMP_FALL_EINT1 },
	[ARIZONA_IRQ_JD_FALL] = { .reg_offset = 6,
				  .mask = CLEARWATER_JD1_FALL_EINT1 },
	[ARIZONA_IRQ_JD_RISE] = { .reg_offset = 6,
				  .mask = CLEARWATER_JD1_RISE_EINT1 },

	[ARIZONA_IRQ_ASRC2_LOCK] = { .reg_offset = 8,
				  .mask = CLEARWATER_ASRC2_IN1_LOCK_EINT1 },
	[ARIZONA_IRQ_ASRC1_LOCK] = { .reg_offset = 8,
				  .mask = CLEARWATER_ASRC1_IN1_LOCK_EINT1 },
	[ARIZONA_IRQ_DRC2_SIG_DET] = { .reg_offset = 8,
				  .mask = CLEARWATER_DRC2_SIG_DET_EINT1 },
	[ARIZONA_IRQ_DRC1_SIG_DET] = { .reg_offset = 8,
				  .mask = CLEARWATER_DRC1_SIG_DET_EINT1 },

	[ARIZONA_IRQ_DSP_IRQ1] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ1_EINT1},
	[ARIZONA_IRQ_DSP_IRQ2] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ2_EINT1},
	[ARIZONA_IRQ_DSP_IRQ3] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ3_EINT1},
	[ARIZONA_IRQ_DSP_IRQ4] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ4_EINT1},
	[ARIZONA_IRQ_DSP_IRQ5] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ5_EINT1},
	[ARIZONA_IRQ_DSP_IRQ6] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ6_EINT1},
	[ARIZONA_IRQ_DSP_IRQ7] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ7_EINT1},
	[ARIZONA_IRQ_DSP_IRQ8] = { .reg_offset = 10,
				  .mask = CLEARWATER_DSP_IRQ8_EINT1},

	[ARIZONA_IRQ_SPK_OVERHEAT_WARN] = { .reg_offset = 14,
				.mask = CLEARWATER_SPK_OVERHEAT_WARN_EINT1},
	[ARIZONA_IRQ_SPK_OVERHEAT] = { .reg_offset = 14,
				.mask = CLEARWATER_SPK_SHUTDOWN_EINT1},
};

const struct regmap_irq_chip clearwater_irq = {
	.name = "clearwater IRQ",
	.status_base = CLEARWATER_IRQ1_STATUS_1,
	.mask_base = CLEARWATER_IRQ1_MASK_1,
	.ack_base = CLEARWATER_IRQ1_STATUS_1,
	.num_regs = 15,
	.irqs = clearwater_irqs,
	.num_irqs = ARRAY_SIZE(clearwater_irqs),
};
EXPORT_SYMBOL_GPL(clearwater_irq);

static const struct reg_default clearwater_reg_default[] = {
	{ 0x00000008, 0x0309 }, /* R8     - Ctrl IF CFG 1 */
	{ 0x00000009, 0x0200 }, /* R9     - Ctrl IF CFG 2 */
	{ 0x0000000A, 0x0309 }, /* R10    - Ctrl IF CFG 3 */
	{ 0x00000016, 0x0000 }, /* R22 (0x16) - Write Sequencer Ctrl 0 */
	{ 0x00000017, 0x0000 }, /* R23 (0x17) - Write Sequencer Ctrl 1 */
	{ 0x00000018, 0x0000 }, /* R24 (0x18) - Write Sequencer Ctrl 2 */
	{ 0x0000001a, 0x0000 }, /* R26 (0x1A) - Write Sequencer PROM */
	{ 0x00000020, 0x0000 }, /* R32 (0x20) - Tone Generator 1 */
	{ 0x00000021, 0x1000 }, /* R33 (0x21) - Tone Generator 2 */
	{ 0x00000022, 0x0000 }, /* R34 (0x22) - Tone Generator 3 */
	{ 0x00000023, 0x1000 }, /* R35 (0x23) - Tone Generator 4 */
	{ 0x00000024, 0x0000 }, /* R36 (0x24) - Tone Generator 5 */
	{ 0x00000030, 0x0000 }, /* R48 (0x30) - PWM Drive 1 */
	{ 0x00000031, 0x0100 }, /* R49 (0x31) - PWM Drive 2 */
	{ 0x00000032, 0x0100 }, /* R50 (0x32) - PWM Drive 3 */
	{ 0x00000041, 0x0000 }, /* R65 (0x41) - Sequence control */
	{ 0x00000061, 0x01ff }, /* R97 (0x61) - Sample Rate Sequence Select 1 */
	{ 0x00000062, 0x01ff }, /* R98 (0x62) - Sample Rate Sequence Select 2 */
	{ 0x00000063, 0x01ff }, /* R99 (0x63) - Sample Rate Sequence Select 3 */
	{ 0x00000064, 0x01ff }, /* R100 (0x64) - Sample Rate Sequence Select 4 */
	{ 0x00000066, 0x01ff },
	{ 0x00000067, 0x01ff },
	{ 0x00000068, 0x01ff }, /* R104 (0x68) - Always On Triggers Sequence Select 1 */
	{ 0x00000069, 0x01ff }, /* R105 (0x69) - Always On Triggers Sequence Select 2 */
	{ 0x0000006a, 0x01ff }, /* R106 (0x6A) - Always On Triggers Sequence Select 3 */
	{ 0x0000006b, 0x01ff }, /* R107 (0x6B) - Always On Triggers Sequence Select 4 */
	{ 0x00000090, 0x0000 }, /* R144 (0x90) - Haptics Control 1 */
	{ 0x00000091, 0x7fff }, /* R145 (0x91) - Haptics Control 2 */
	{ 0x00000092, 0x0000 }, /* R146 (0x92) - Haptics phase 1 intensity */
	{ 0x00000093, 0x0000 }, /* R147 (0x93) - Haptics phase 1 duration */
	{ 0x00000094, 0x0000 }, /* R148 (0x94) - Haptics phase 2 intensity */
	{ 0x00000095, 0x0000 }, /* R149 (0x95) - Haptics phase 2 duration */
	{ 0x00000096, 0x0000 }, /* R150 (0x96) - Haptics phase 3 intensity */
	{ 0x00000097, 0x0000 }, /* R151 (0x97) - Haptics phase 3 duration */
	{ 0x000000A0, 0x0000 }, /* R160 (0xA0) - Clearwater Comfort Noise Generator */
	{ 0x00000100, 0x0002 }, /* R256 (0x100) - Clock 32k 1 */
	{ 0x00000101, 0x0404 }, /* R257 (0x101) - System Clock 1 */
	{ 0x00000102, 0x0011 }, /* R258 (0x102) - Sample rate 1 */
	{ 0x00000103, 0x0011 }, /* R259 (0x103) - Sample rate 2 */
	{ 0x00000104, 0x0011 }, /* R260 (0x104) - Sample rate 3 */
	{ 0x00000112, 0x0305 }, /* R274 (0x112) - Async clock 1 */
	{ 0x00000113, 0x0011 }, /* R275 (0x113) - Async sample rate 1 */
	{ 0x00000114, 0x0011 }, /* R276 (0x114) - Async sample rate 2 */
	{ 0x00000120, 0x0305 },
	{ 0x00000122, 0x0000 },
	{ 0x00000149, 0x0000 }, /* R329 (0x149) - Output system clock */
	{ 0x0000014a, 0x0000 }, /* R330 (0x14A) - Output async clock */
	{ 0x00000152, 0x0000 }, /* R338 (0x152) - Rate Estimator 1 */
	{ 0x00000153, 0x0000 }, /* R339 (0x153) - Rate Estimator 2 */
	{ 0x00000154, 0x0000 }, /* R340 (0x154) - Rate Estimator 3 */
	{ 0x00000155, 0x0000 }, /* R341 (0x155) - Rate Estimator 4 */
	{ 0x00000156, 0x0000 }, /* R342 (0x156) - Rate Estimator 5 */
	{ 0x00000171, 0x0002 }, /* R369 (0x171) - FLL1 Control 1 */
	{ 0x00000172, 0x0008 }, /* R370 (0x172) - FLL1 Control 2 */
	{ 0x00000173, 0x0018 }, /* R371 (0x173) - FLL1 Control 3 */
	{ 0x00000174, 0x007d }, /* R372 (0x174) - FLL1 Control 4 */
	{ 0x00000175, 0x0000 }, /* R373   - FLL1 Control 5 */
	{ 0x00000176, 0x0000 }, /* R374 (0x176) - FLL1 Control 6 */
	{ 0x00000177, 0x0281 }, /* R375 (0x177) - FLL1 Loop Filter Test 1 */
	{ 0x00000178, 0x0000 },
	{ 0x00000179, 0x0000 }, /* R377 (0x179) - FLL1 Control 7 */
	{ 0x00000181, 0x0000 }, /* R385 (0x181) - FLL1 Synchroniser 1 */
	{ 0x00000182, 0x0000 }, /* R386 (0x182) - FLL1 Synchroniser 2 */
	{ 0x00000183, 0x0000 }, /* R387 (0x183) - FLL1 Synchroniser 3 */
	{ 0x00000184, 0x0000 }, /* R388 (0x184) - FLL1 Synchroniser 4 */
	{ 0x00000185, 0x0000 }, /* R389 (0x185) - FLL1 Synchroniser 5 */
	{ 0x00000186, 0x0000 }, /* R390 (0x186) - FLL1 Synchroniser 6 */
	{ 0x00000187, 0x0001 }, /* R391 (0x187) - FLL1 Synchroniser 7 */
	{ 0x00000189, 0x0000 }, /* R393 (0x189) - FLL1 Spread Spectrum */
	{ 0x0000018a, 0x000c }, /* R394 (0x18A) - FLL1 GPIO Clock */
	{ 0x00000191, 0x0002 }, /* R401 (0x191) - FLL2 Control 1 */
	{ 0x00000192, 0x0008 }, /* R402 (0x192) - FLL2 Control 2 */
	{ 0x00000193, 0x0018 }, /* R403 (0x193) - FLL2 Control 3 */
	{ 0x00000194, 0x007d }, /* R404 (0x194) - FLL2 Control 4 */
	{ 0x00000195, 0x0000 }, /* R405   - FLL2 Control 5 */
	{ 0x00000196, 0x0000 }, /* R406 (0x196) - FLL2 Control 6 */
	{ 0x00000197, 0x0281 }, /* R407 (0x197) - FLL2 Loop Filter Test 1 */
	{ 0x00000198, 0x0000 },
	{ 0x00000199, 0x0000 }, /* R409 (0x199) - FLL2 Control 7 */
	{ 0x000001a1, 0x0000 }, /* R417 (0x1A1) - FLL2 Synchroniser 1 */
	{ 0x000001a2, 0x0000 }, /* R418 (0x1A2) - FLL2 Synchroniser 2 */
	{ 0x000001a3, 0x0000 }, /* R419 (0x1A3) - FLL2 Synchroniser 3 */
	{ 0x000001a4, 0x0000 }, /* R420 (0x1A4) - FLL2 Synchroniser 4 */
	{ 0x000001a5, 0x0000 }, /* R421 (0x1A5) - FLL2 Synchroniser 5 */
	{ 0x000001a6, 0x0000 }, /* R422 (0x1A6) - FLL2 Synchroniser 6 */
	{ 0x000001a7, 0x0001 }, /* R423 (0x1A7) - FLL2 Synchroniser 7 */
	{ 0x000001a9, 0x0000 }, /* R425 (0x1A9) - FLL2 Spread Spectrum */
	{ 0x000001aa, 0x000c }, /* R426 (0x1AA) - FLL2 GPIO Clock */
	{ 0x000001B1, 0x0002 }, /* R433  - FLL3 Control 1 */
	{ 0x000001B2, 0x0008 }, /* R434  - FLL3 Control 2 */
	{ 0x000001B3, 0x0018 }, /* R435  - FLL3 Control 3 */
	{ 0x000001B4, 0x007D }, /* R436  - FLL3 Control 4 */
	{ 0x000001B5, 0x0000 }, /* R437  - FLL3 Control 5 */
	{ 0x000001B6, 0x0000 }, /* R438  - FLL3 Control 6 */
	{ 0x000001B7, 0x0281 }, /* R439  - FLL3 Loop Filter Test 1 */
	{ 0x000001B8, 0x0000 }, /* R440  - FLL3 NCO Test 0 */
	{ 0x000001B9, 0x0000 }, /* R441  - FLL3 Control 7 */
	{ 0x000001C1, 0x0000 }, /* R449  - FLL3 Synchroniser 1 */
	{ 0x000001C2, 0x0000 }, /* R450  - FLL3 Synchroniser 2 */
	{ 0x000001C3, 0x0000 }, /* R451  - FLL3 Synchroniser 3 */
	{ 0x000001C4, 0x0000 }, /* R452  - FLL3 Synchroniser 4 */
	{ 0x000001C5, 0x0000 }, /* R453  - FLL3 Synchroniser 5 */
	{ 0x000001C6, 0x0000 }, /* R454  - FLL3 Synchroniser 6 */
	{ 0x000001C7, 0x0001 }, /* R455  - FLL3 Synchroniser 7 */
	{ 0x000001C9, 0x0000 }, /* R457  - FLL3 Spread Spectrum */
	{ 0x000001CA, 0x000C }, /* R458  - FLL3 GPIO Clock */
	{ 0x00000200, 0x0006 }, /* R512 (0x200) - Mic Charge Pump 1 */
	{ 0x0000020B, 0x0400 },
	{ 0x00000210, 0x0184 }, /* R528 (0x210) - LDO1 Control 1 */
	{ 0x00000213, 0x03e4 }, /* R531 (0x213) - LDO2 Control 1 */
	{ 0x00000218, 0x00e6 }, /* R536 (0x218) - Mic Bias Ctrl 1 */
	{ 0x00000219, 0x00e6 }, /* R537 (0x219) - Mic Bias Ctrl 2 */
	{ 0x0000021a, 0x00e6 }, /* R538 (0x21A) - Mic Bias Ctrl 3 */
	{ 0x0000021B, 0x00e6 }, /* R539  - Mic Bias Ctrl 4 */
	{ 0x0000027e, 0x0000 }, /* R638 (0x27E) - Clearwater EDRE HP stereo control */
	{ 0x00000293, 0x0000 }, /* R659 (0x293) - Accessory Detect Mode 1 */
	{ 0x0000029b, 0x0000 }, /* R667 (0x29B) - Headphone Detect 1 */
	{ 0x000002a3, 0x1102 }, /* R675 (0x2A3) - Mic Detect 1 */
	{ 0x000002a4, 0x009f }, /* R676 (0x2A4) - Mic Detect 2 */
	{ 0x000002a6, 0x3737 },
	{ 0x000002a7, 0x2c37 },
	{ 0x000002a8, 0x1422 },
	{ 0x000002a9, 0x030a },
	{ 0x000002c3, 0x0000 }, /* R707 (0x2C3) - Mic noise mix control 1 */
	{ 0x000002c6, 0x0010 },
	{ 0x000002c8, 0x0000 }, /* R712 (0x2C8) - GP switch 1 */
	{ 0x000002d3, 0x0000 }, /* R723 (0x2D3) - Jack detect analogue */
	{ 0x00000308, 0x0000 }, /* R776 (0x308) - Input Rate */
	{ 0x00000309, 0x0022 }, /* R777 (0x309) - Input Volume Ramp */
	{ 0x0000030c, 0x0002 }, /* R780 (0x30C) - HPF Control */
	{ 0x00000310, 0x0080 }, /* R784 (0x310) - IN1L Control */
	{ 0x00000311, 0x0180 }, /* R785 (0x311) - ADC Digital Volume 1L */
	{ 0x00000312, 0x0500 }, /* R786 (0x312) - DMIC1L Control */
	{ 0x00000314, 0x0080 }, /* R788 (0x314) - IN1R Control */
	{ 0x00000315, 0x0180 }, /* R789 (0x315) - ADC Digital Volume 1R */
	{ 0x00000316, 0x0000 }, /* R790 (0x316) - DMIC1R Control */
	{ 0x00000318, 0x0080 }, /* R792 (0x318) - IN2L Control */
	{ 0x00000319, 0x0180 }, /* R793 (0x319) - ADC Digital Volume 2L */
	{ 0x0000031a, 0x0500 }, /* R794 (0x31A) - DMIC2L Control */
	{ 0x0000031c, 0x0080 }, /* R796 (0x31C) - IN2R Control */
	{ 0x0000031d, 0x0180 }, /* R797 (0x31D) - ADC Digital Volume 2R */
	{ 0x0000031e, 0x0000 }, /* R798 (0x31E) - DMIC2R Control */
	{ 0x00000320, 0x0080 }, /* R800 (0x320) - IN3L Control */
	{ 0x00000321, 0x0180 }, /* R801 (0x321) - ADC Digital Volume 3L */
	{ 0x00000322, 0x0500 }, /* R802 (0x322) - DMIC3L Control */
	{ 0x00000324, 0x0080 }, /* R804 (0x324) - IN3R Control */
	{ 0x00000325, 0x0180 }, /* R805 (0x325) - ADC Digital Volume 3R */
	{ 0x00000326, 0x0000 }, /* R806 (0x326) - DMIC3R Control */
	{ 0x00000328, 0x0000 }, /* R808 (0x328) - IN4 Control */
	{ 0x00000329, 0x0180 }, /* R809 (0x329) - ADC Digital Volume 4L */
	{ 0x0000032a, 0x0500 }, /* R810 (0x32A) - DMIC4L Control */
	{ 0x0000032c, 0x0000 }, /* R812 (0x32C) - IN4R Control */
	{ 0x0000032d, 0x0180 }, /* R813 (0x32D) - ADC Digital Volume 4R */
	{ 0x0000032e, 0x0000 }, /* R814 (0x32E) - DMIC4R Control */
	{ 0x00000330, 0x0000 }, /* R816  - IN5L Control */
	{ 0x00000331, 0x0180 }, /* R817  - ADC Digital Volume 5L */
	{ 0x00000332, 0x0500 }, /* R818  - DMIC5L Control */
	{ 0x00000334, 0x0000 }, /* R820  - IN5R Control */
	{ 0x00000335, 0x0180 }, /* R821  - ADC Digital Volume 5R */
	{ 0x00000336, 0x0000 }, /* R822  - DMIC5R Control */
	{ 0x00000338, 0x0000 }, /* R824  - IN6L Control */
	{ 0x00000339, 0x0180 }, /* R825  - ADC Digital Volume 6L */
	{ 0x0000033A, 0x0500 }, /* R826  - DMIC6L Control */
	{ 0x0000033C, 0x0000 }, /* R828  - IN6R Control */
	{ 0x0000033D, 0x0180 }, /* R829  - ADC Digital Volume 6R */
	{ 0x0000033E, 0x0000 }, /* R830  - DMIC6R Control */
	{ 0x00000400, 0x0000 }, /* R1024 (0x400) - Output Enables 1 */
	{ 0x00000408, 0x0000 }, /* R1032 (0x408) - Output Rate 1 */
	{ 0x00000409, 0x0022 }, /* R1033 (0x409) - Output Volume Ramp */
	{ 0x00000410, 0x0080 }, /* R1040 (0x410) - Output Path Config 1L */
	{ 0x00000411, 0x0180 }, /* R1041 (0x411) - DAC Digital Volume 1L */
	{ 0x00000413, 0x0001 }, /* R1043 (0x413) - Noise Gate Select 1L */
	{ 0x00000414, 0x0080 }, /* R1044 (0x414) - Output Path Config 1R */
	{ 0x00000415, 0x0180 }, /* R1045 (0x415) - DAC Digital Volume 1R */
	{ 0x00000417, 0x0002 }, /* R1047 (0x417) - Noise Gate Select 1R */
	{ 0x00000418, 0x0080 }, /* R1048 (0x418) - Output Path Config 2L */
	{ 0x00000419, 0x0180 }, /* R1049 (0x419) - DAC Digital Volume 2L */
	{ 0x0000041b, 0x0004 }, /* R1051 (0x41B) - Noise Gate Select 2L */
	{ 0x0000041c, 0x0080 }, /* R1052 (0x41C) - Output Path Config 2R */
	{ 0x0000041d, 0x0180 }, /* R1053 (0x41D) - DAC Digital Volume 2R */
	{ 0x0000041f, 0x0008 }, /* R1055 (0x41F) - Noise Gate Select 2R */
	{ 0x00000420, 0x0080 }, /* R1056 (0x420) - Output Path Config 3L */
	{ 0x00000421, 0x0180 }, /* R1057 (0x421) - DAC Digital Volume 3L */
	{ 0x00000423, 0x0010 }, /* R1059 (0x423) - Noise Gate Select 3L */
	{ 0x00000424, 0x0080 }, /* R1060 (0x424) - Output Path Config 3R */
	{ 0x00000425, 0x0180 }, /* R1061 (0x425) - DAC Digital Volume 3R */
	{ 0x00000427, 0x0020 },
	{ 0x00000428, 0x0000 }, /* R1064 (0x428) - Output Path Config 4L */
	{ 0x00000429, 0x0180 }, /* R1065 (0x429) - DAC Digital Volume 4L */
	{ 0x0000042b, 0x0040 }, /* R1067 (0x42B) - Noise Gate Select 4L */
	{ 0x0000042c, 0x0000 }, /* R1068 (0x42C) - Output Path Config 4R */
	{ 0x0000042d, 0x0180 }, /* R1069 (0x42D) - DAC Digital Volume 4R */
	{ 0x0000042f, 0x0080 }, /* R1071 (0x42F) - Noise Gate Select 4R */
	{ 0x00000430, 0x0000 }, /* R1072 (0x430) - Output Path Config 5L */
	{ 0x00000431, 0x0180 }, /* R1073 (0x431) - DAC Digital Volume 5L */
	{ 0x00000433, 0x0100 }, /* R1075 (0x433) - Noise Gate Select 5L */
	{ 0x00000434, 0x0000 }, /* R1076 (0x434) - Output Path Config 5R */
	{ 0x00000435, 0x0180 }, /* R1077 (0x435) - DAC Digital Volume 5R */
	{ 0x00000437, 0x0200 }, /* R1079 (0x437) - Noise Gate Select 5R */
	{ 0x00000438, 0x0000 }, /* R1080 (0x438) - Output Path Config 6L */
	{ 0x00000439, 0x0180 }, /* R1081 (0x439) - DAC Digital Volume 6L */
	{ 0x0000043b, 0x0400 }, /* R1083 (0x43B) - Noise Gate Select 6L */
	{ 0x0000043c, 0x0000 }, /* R1084 (0x43C) - Output Path Config 6R */
	{ 0x0000043d, 0x0180 }, /* R1085 (0x43D) - DAC Digital Volume 6R */
	{ 0x0000043f, 0x0800 }, /* R1087 (0x43F) - Noise Gate Select 6R */
	{ 0x00000440, 0x003f }, /* R1088 (0x440) - DRE Enable */
	{ 0x00000441, 0xC050 }, /* R1089  - DRE Control 1 */
	{ 0x00000442, 0x0305 }, /* R1090 (0x442) - DRE Control 2 */
	{ 0x00000443, 0x5cfa }, /* R1091 (0x443) - DRE Control 3 */
	{ 0x00000448, 0x0fff }, /* R1096 (0x448) - eDRE Enable */
	{ 0x00000450, 0x0000 }, /* R1104 (0x450) - DAC AEC Control 1 */
	{ 0x00000451, 0x0000 },
	{ 0x00000458, 0x0000 }, /* R1112 (0x458) - Noise Gate Control */
	{ 0x00000490, 0x0069 }, /* R1168 (0x490) - PDM SPK1 CTRL 1 */
	{ 0x00000491, 0x0000 }, /* R1169 (0x491) - PDM SPK1 CTRL 2 */
	{ 0x00000492, 0x0069 }, /* R1170 (0x492) - PDM SPK2 CTRL 1 */
	{ 0x00000493, 0x0000 }, /* R1171 (0x493) - PDM SPK2 CTRL 2 */
	{ 0x000004A0, 0x3210 }, /* R1184  - HP1 Short Circuit Ctrl */
	{ 0x000004A1, 0x3200 }, /* R1185  - HP2 Short Circuit Ctrl */
	{ 0x000004A2, 0x3200 }, /* R1186  - HP3 Short Circuit Ctrl */
	{ 0x000004A8, 0x7020 }, /* R1192  - HP Test Ctrl 5 */
	{ 0x000004A9, 0x7020 }, /* R1193  - HP Test Ctrl 6 */
	{ 0x000004b5, 0x0000 },
	{ 0x000004b6, 0x8080 },
	{ 0x000004dc, 0x6000 }, /* R1244 (0x4DC) - DAC comp 1 */
	{ 0x000004de, 0x0000 }, /* R1246 (0x4DE) - DAC comp 3 */
	{ 0x000004df, 0x0000 }, /* R1247 (0x4DF) - DAC comp 4 */
	{ 0x00000500, 0x000c }, /* R1280 (0x500) - AIF1 BCLK Ctrl */
	{ 0x00000501, 0x0000 }, /* R1281 (0x501) - AIF1 Tx Pin Ctrl */
	{ 0x00000502, 0x0000 }, /* R1282 (0x502) - AIF1 Rx Pin Ctrl */
	{ 0x00000503, 0x0000 }, /* R1283 (0x503) - AIF1 Rate Ctrl */
	{ 0x00000504, 0x0000 }, /* R1284 (0x504) - AIF1 Format */
	{ 0x00000505, 0x0040 }, /* R1285 (0x505) - AIF1 Tx BCLK Rate */
	{ 0x00000506, 0x0040 }, /* R1286 (0x506) - AIF1 Rx BCLK Rate */
	{ 0x00000507, 0x1818 }, /* R1287 (0x507) - AIF1 Frame Ctrl 1 */
	{ 0x00000508, 0x1818 }, /* R1288 (0x508) - AIF1 Frame Ctrl 2 */
	{ 0x00000509, 0x0000 }, /* R1289 (0x509) - AIF1 Frame Ctrl 3 */
	{ 0x0000050a, 0x0001 }, /* R1290 (0x50A) - AIF1 Frame Ctrl 4 */
	{ 0x0000050b, 0x0002 }, /* R1291 (0x50B) - AIF1 Frame Ctrl 5 */
	{ 0x0000050c, 0x0003 }, /* R1292 (0x50C) - AIF1 Frame Ctrl 6 */
	{ 0x0000050d, 0x0004 }, /* R1293 (0x50D) - AIF1 Frame Ctrl 7 */
	{ 0x0000050e, 0x0005 }, /* R1294 (0x50E) - AIF1 Frame Ctrl 8 */
	{ 0x0000050f, 0x0006 }, /* R1295 (0x50F) - AIF1 Frame Ctrl 9 */
	{ 0x00000510, 0x0007 }, /* R1296 (0x510) - AIF1 Frame Ctrl 10 */
	{ 0x00000511, 0x0000 }, /* R1297 (0x511) - AIF1 Frame Ctrl 11 */
	{ 0x00000512, 0x0001 }, /* R1298 (0x512) - AIF1 Frame Ctrl 12 */
	{ 0x00000513, 0x0002 }, /* R1299 (0x513) - AIF1 Frame Ctrl 13 */
	{ 0x00000514, 0x0003 }, /* R1300 (0x514) - AIF1 Frame Ctrl 14 */
	{ 0x00000515, 0x0004 }, /* R1301 (0x515) - AIF1 Frame Ctrl 15 */
	{ 0x00000516, 0x0005 }, /* R1302 (0x516) - AIF1 Frame Ctrl 16 */
	{ 0x00000517, 0x0006 }, /* R1303 (0x517) - AIF1 Frame Ctrl 17 */
	{ 0x00000518, 0x0007 }, /* R1304 (0x518) - AIF1 Frame Ctrl 18 */
	{ 0x00000519, 0x0000 }, /* R1305 (0x519) - AIF1 Tx Enables */
	{ 0x0000051a, 0x0000 }, /* R1306 (0x51A) - AIF1 Rx Enables */
	{ 0x0000051b, 0x0000 }, /* R1307 (0x51B) - AIF1 Force Write */
	{ 0x00000540, 0x000c }, /* R1344 (0x540) - AIF2 BCLK Ctrl */
	{ 0x00000541, 0x0000 }, /* R1345 (0x541) - AIF2 Tx Pin Ctrl */
	{ 0x00000542, 0x0000 }, /* R1346 (0x542) - AIF2 Rx Pin Ctrl */
	{ 0x00000543, 0x0000 }, /* R1347 (0x543) - AIF2 Rate Ctrl */
	{ 0x00000544, 0x0000 }, /* R1348 (0x544) - AIF2 Format */
	{ 0x00000545, 0x0040 }, /* R1349 (0x545) - AIF2 Tx BCLK Rate */
	{ 0x00000546, 0x0040 }, /* R1350 (0x546) - AIF2 Rx BCLK Rate */
	{ 0x00000547, 0x1818 }, /* R1351 (0x547) - AIF2 Frame Ctrl 1 */
	{ 0x00000548, 0x1818 }, /* R1352 (0x548) - AIF2 Frame Ctrl 2 */
	{ 0x00000549, 0x0000 }, /* R1353 (0x549) - AIF2 Frame Ctrl 3 */
	{ 0x0000054a, 0x0001 }, /* R1354 (0x54A) - AIF2 Frame Ctrl 4 */
	{ 0x0000054b, 0x0002 }, /* R1355 (0x54B) - AIF2 Frame Ctrl 5 */
	{ 0x0000054c, 0x0003 }, /* R1356 (0x54C) - AIF2 Frame Ctrl 6 */
	{ 0x0000054d, 0x0004 }, /* R1357 (0x54D) - AIF2 Frame Ctrl 7 */
	{ 0x0000054e, 0x0005 }, /* R1358 (0x54E) - AIF2 Frame Ctrl 8 */
	{ 0x0000054F, 0x0006 }, /* R1359  - AIF2 Frame Ctrl 9 */
	{ 0x00000550, 0x0007 }, /* R1360  - AIF2 Frame Ctrl 10 */
	{ 0x00000551, 0x0000 }, /* R1361 (0x551) - AIF2 Frame Ctrl 11 */
	{ 0x00000552, 0x0001 }, /* R1362 (0x552) - AIF2 Frame Ctrl 12 */
	{ 0x00000553, 0x0002 }, /* R1363 (0x553) - AIF2 Frame Ctrl 13 */
	{ 0x00000554, 0x0003 }, /* R1364 (0x554) - AIF2 Frame Ctrl 14 */
	{ 0x00000555, 0x0004 }, /* R1365 (0x555) - AIF2 Frame Ctrl 15 */
	{ 0x00000556, 0x0005 }, /* R1366 (0x556) - AIF2 Frame Ctrl 16 */
	{ 0x00000557, 0x0006 }, /* R1367  - AIF2 Frame Ctrl 17 */
	{ 0x00000558, 0x0007 }, /* R1368  - AIF2 Frame Ctrl 18 */
	{ 0x00000559, 0x0000 }, /* R1369 (0x559) - AIF2 Tx Enables */
	{ 0x0000055a, 0x0000 }, /* R1370 (0x55A) - AIF2 Rx Enables */
	{ 0x0000055b, 0x0000 }, /* R1371 (0x55B) - AIF2 Force Write */
	{ 0x00000580, 0x000c }, /* R1408 (0x580) - AIF3 BCLK Ctrl */
	{ 0x00000581, 0x0000 }, /* R1409 (0x581) - AIF3 Tx Pin Ctrl */
	{ 0x00000582, 0x0000 }, /* R1410 (0x582) - AIF3 Rx Pin Ctrl */
	{ 0x00000583, 0x0000 }, /* R1411 (0x583) - AIF3 Rate Ctrl */
	{ 0x00000584, 0x0000 }, /* R1412 (0x584) - AIF3 Format */
	{ 0x00000585, 0x0040 }, /* R1413 (0x585) - AIF3 Tx BCLK Rate */
	{ 0x00000586, 0x0040 }, /* R1414 (0x586) - AIF3 Rx BCLK Rate */
	{ 0x00000587, 0x1818 }, /* R1415 (0x587) - AIF3 Frame Ctrl 1 */
	{ 0x00000588, 0x1818 }, /* R1416 (0x588) - AIF3 Frame Ctrl 2 */
	{ 0x00000589, 0x0000 }, /* R1417 (0x589) - AIF3 Frame Ctrl 3 */
	{ 0x0000058a, 0x0001 }, /* R1418 (0x58A) - AIF3 Frame Ctrl 4 */
	{ 0x00000591, 0x0000 }, /* R1425 (0x591) - AIF3 Frame Ctrl 11 */
	{ 0x00000592, 0x0001 }, /* R1426 (0x592) - AIF3 Frame Ctrl 12 */
	{ 0x00000599, 0x0000 }, /* R1433 (0x599) - AIF3 Tx Enables */
	{ 0x0000059a, 0x0000 }, /* R1434 (0x59A) - AIF3 Rx Enables */
	{ 0x0000059b, 0x0000 }, /* R1435 (0x59B) - AIF3 Force Write */
	{ 0x000005a0, 0x000c }, /* R1440  - AIF4 BCLK Ctrl */
	{ 0x000005a1, 0x0000 }, /* R1441  - AIF4 Tx Pin Ctrl */
	{ 0x000005a2, 0x0000 }, /* R1442  - AIF4 Rx Pin Ctrl */
	{ 0x000005a3, 0x0000 }, /* R1443  - AIF4 Rate Ctrl */
	{ 0x000005a4, 0x0000 }, /* R1444  - AIF4 Format */
	{ 0x000005a5, 0x0040 }, /* R1445  - AIF4 Tx BCLK Rate */
	{ 0x000005a6, 0x0040 }, /* R1446  - AIF4 Rx BCLK Rate */
	{ 0x000005a7, 0x1818 }, /* R1447  - AIF4 Frame Ctrl 1 */
	{ 0x000005a8, 0x1818 }, /* R1448  - AIF4 Frame Ctrl 2 */
	{ 0x000005a9, 0x0000 }, /* R1449  - AIF4 Frame Ctrl 3 */
	{ 0x000005aa, 0x0001 }, /* R1450  - AIF4 Frame Ctrl 4 */
	{ 0x000005b1, 0x0000 }, /* R1457  - AIF4 Frame Ctrl 11 */
	{ 0x000005b2, 0x0001 }, /* R1458  - AIF4 Frame Ctrl 12 */
	{ 0x000005b9, 0x0000 }, /* R1465  - AIF4 Tx Enables */
	{ 0x000005ba, 0x0000 }, /* R1466  - AIF4 Rx Enables */
	{ 0x000005bb, 0x0000 }, /* R1467  - AIF4 Force Write */
	{ 0x000005C2, 0x0000 }, /* R1474  - SPD1 TX Control */
	{ 0x000005e3, 0x0000 }, /* R1507 (0x5E3) - SLIMbus Framer Ref Gear */
	{ 0x000005e5, 0x0000 }, /* R1509 (0x5E5) - SLIMbus Rates 1 */
	{ 0x000005e6, 0x0000 }, /* R1510 (0x5E6) - SLIMbus Rates 2 */
	{ 0x000005e7, 0x0000 }, /* R1511 (0x5E7) - SLIMbus Rates 3 */
	{ 0x000005e8, 0x0000 }, /* R1512 (0x5E8) - SLIMbus Rates 4 */
	{ 0x000005e9, 0x0000 }, /* R1513 (0x5E9) - SLIMbus Rates 5 */
	{ 0x000005ea, 0x0000 }, /* R1514 (0x5EA) - SLIMbus Rates 6 */
	{ 0x000005eb, 0x0000 }, /* R1515 (0x5EB) - SLIMbus Rates 7 */
	{ 0x000005ec, 0x0000 }, /* R1516 (0x5EC) - SLIMbus Rates 8 */
	{ 0x000005f5, 0x0000 }, /* R1525 (0x5F5) - SLIMbus RX Channel Enable */
	{ 0x000005f6, 0x0000 }, /* R1526 (0x5F6) - SLIMbus TX Channel Enable */
	{ 0x00000640, 0x0000 },
	{ 0x00000641, 0x0080 },
	{ 0x00000642, 0x0000 },
	{ 0x00000643, 0x0080 },
	{ 0x00000644, 0x0000 },
	{ 0x00000645, 0x0080 },
	{ 0x00000646, 0x0000 },
	{ 0x00000647, 0x0080 },
	{ 0x00000648, 0x0000 },
	{ 0x00000649, 0x0080 },
	{ 0x0000064a, 0x0000 },
	{ 0x0000064b, 0x0080 },
	{ 0x0000064c, 0x0000 },
	{ 0x0000064d, 0x0080 },
	{ 0x0000064e, 0x0000 },
	{ 0x0000064f, 0x0080 },
	{ 0x00000680, 0x0000 },
	{ 0x00000681, 0x0080 },
	{ 0x00000682, 0x0000 },
	{ 0x00000683, 0x0080 },
	{ 0x00000684, 0x0000 },
	{ 0x00000685, 0x0080 },
	{ 0x00000686, 0x0000 },
	{ 0x00000687, 0x0080 },
	{ 0x00000688, 0x0000 },
	{ 0x00000689, 0x0080 },
	{ 0x0000068a, 0x0000 },
	{ 0x0000068b, 0x0080 },
	{ 0x0000068c, 0x0000 },
	{ 0x0000068d, 0x0080 },
	{ 0x0000068e, 0x0000 },
	{ 0x0000068f, 0x0080 },
	{ 0x00000690, 0x0000 },
	{ 0x00000691, 0x0080 },
	{ 0x00000692, 0x0000 },
	{ 0x00000693, 0x0080 },
	{ 0x00000694, 0x0000 },
	{ 0x00000695, 0x0080 },
	{ 0x00000696, 0x0000 },
	{ 0x00000697, 0x0080 },
	{ 0x00000698, 0x0000 },
	{ 0x00000699, 0x0080 },
	{ 0x0000069a, 0x0000 },
	{ 0x0000069b, 0x0080 },
	{ 0x0000069c, 0x0000 },
	{ 0x0000069d, 0x0080 },
	{ 0x0000069e, 0x0000 },
	{ 0x0000069f, 0x0080 },
	{ 0x000006a0, 0x0000 },
	{ 0x000006a1, 0x0080 },
	{ 0x000006a2, 0x0000 },
	{ 0x000006a3, 0x0080 },
	{ 0x000006a4, 0x0000 },
	{ 0x000006a5, 0x0080 },
	{ 0x000006a6, 0x0000 },
	{ 0x000006a7, 0x0080 },
	{ 0x000006a8, 0x0000 },
	{ 0x000006a9, 0x0080 },
	{ 0x000006aa, 0x0000 },
	{ 0x000006ab, 0x0080 },
	{ 0x000006ac, 0x0000 },
	{ 0x000006ad, 0x0080 },
	{ 0x000006ae, 0x0000 },
	{ 0x000006af, 0x0080 },
	{ 0x000006b0, 0x0000 },
	{ 0x000006b1, 0x0080 },
	{ 0x000006b2, 0x0000 },
	{ 0x000006b3, 0x0080 },
	{ 0x000006b4, 0x0000 },
	{ 0x000006b5, 0x0080 },
	{ 0x000006b6, 0x0000 },
	{ 0x000006b7, 0x0080 },
	{ 0x000006b8, 0x0000 },
	{ 0x000006b9, 0x0080 },
	{ 0x000006ba, 0x0000 },
	{ 0x000006bb, 0x0080 },
	{ 0x000006bc, 0x0000 },
	{ 0x000006bd, 0x0080 },
	{ 0x000006be, 0x0000 },
	{ 0x000006bf, 0x0080 },
	{ 0x000006c0, 0x0000 },
	{ 0x000006c1, 0x0080 },
	{ 0x000006c2, 0x0000 },
	{ 0x000006c3, 0x0080 },
	{ 0x000006c4, 0x0000 },
	{ 0x000006c5, 0x0080 },
	{ 0x000006c6, 0x0000 },
	{ 0x000006c7, 0x0080 },
	{ 0x000006c8, 0x0000 },
	{ 0x000006c9, 0x0080 },
	{ 0x000006ca, 0x0000 },
	{ 0x000006cb, 0x0080 },
	{ 0x000006cc, 0x0000 },
	{ 0x000006cd, 0x0080 },
	{ 0x000006ce, 0x0000 },
	{ 0x000006cf, 0x0080 },
	{ 0x000006d0, 0x0000 },
	{ 0x000006d1, 0x0080 },
	{ 0x000006d2, 0x0000 },
	{ 0x000006d3, 0x0080 },
	{ 0x000006d4, 0x0000 },
	{ 0x000006d5, 0x0080 },
	{ 0x000006d6, 0x0000 },
	{ 0x000006d7, 0x0080 },
	{ 0x000006d8, 0x0000 },
	{ 0x000006d9, 0x0080 },
	{ 0x000006da, 0x0000 },
	{ 0x000006db, 0x0080 },
	{ 0x000006dc, 0x0000 },
	{ 0x000006dd, 0x0080 },
	{ 0x000006de, 0x0000 },
	{ 0x000006df, 0x0080 },
	{ 0x00000700, 0x0000 },
	{ 0x00000701, 0x0080 },
	{ 0x00000702, 0x0000 },
	{ 0x00000703, 0x0080 },
	{ 0x00000704, 0x0000 },
	{ 0x00000705, 0x0080 },
	{ 0x00000706, 0x0000 },
	{ 0x00000707, 0x0080 },
	{ 0x00000708, 0x0000 },
	{ 0x00000709, 0x0080 },
	{ 0x0000070a, 0x0000 },
	{ 0x0000070b, 0x0080 },
	{ 0x0000070c, 0x0000 },
	{ 0x0000070d, 0x0080 },
	{ 0x0000070e, 0x0000 },
	{ 0x0000070f, 0x0080 },
	{ 0x00000710, 0x0000 },
	{ 0x00000711, 0x0080 },
	{ 0x00000712, 0x0000 },
	{ 0x00000713, 0x0080 },
	{ 0x00000714, 0x0000 },
	{ 0x00000715, 0x0080 },
	{ 0x00000716, 0x0000 },
	{ 0x00000717, 0x0080 },
	{ 0x00000718, 0x0000 },
	{ 0x00000719, 0x0080 },
	{ 0x0000071a, 0x0000 },
	{ 0x0000071b, 0x0080 },
	{ 0x0000071c, 0x0000 },
	{ 0x0000071d, 0x0080 },
	{ 0x0000071e, 0x0000 },
	{ 0x0000071f, 0x0080 },
	{ 0x00000720, 0x0000 },
	{ 0x00000721, 0x0080 },
	{ 0x00000722, 0x0000 },
	{ 0x00000723, 0x0080 },
	{ 0x00000724, 0x0000 },
	{ 0x00000725, 0x0080 },
	{ 0x00000726, 0x0000 },
	{ 0x00000727, 0x0080 },
	{ 0x00000728, 0x0000 },
	{ 0x00000729, 0x0080 },
	{ 0x0000072a, 0x0000 },
	{ 0x0000072b, 0x0080 },
	{ 0x0000072c, 0x0000 },
	{ 0x0000072d, 0x0080 },
	{ 0x0000072e, 0x0000 },
	{ 0x0000072f, 0x0080 },
	{ 0x00000730, 0x0000 },
	{ 0x00000731, 0x0080 },
	{ 0x00000732, 0x0000 },
	{ 0x00000733, 0x0080 },
	{ 0x00000734, 0x0000 },
	{ 0x00000735, 0x0080 },
	{ 0x00000736, 0x0000 },
	{ 0x00000737, 0x0080 },
	{ 0x00000738, 0x0000 },
	{ 0x00000739, 0x0080 },
	{ 0x0000073a, 0x0000 },
	{ 0x0000073b, 0x0080 },
	{ 0x0000073c, 0x0000 },
	{ 0x0000073d, 0x0080 },
	{ 0x0000073e, 0x0000 },
	{ 0x0000073f, 0x0080 },
	{ 0x00000740, 0x0000 },
	{ 0x00000741, 0x0080 },
	{ 0x00000742, 0x0000 },
	{ 0x00000743, 0x0080 },
	{ 0x00000744, 0x0000 },
	{ 0x00000745, 0x0080 },
	{ 0x00000746, 0x0000 },
	{ 0x00000747, 0x0080 },
	{ 0x00000748, 0x0000 },
	{ 0x00000749, 0x0080 },
	{ 0x0000074a, 0x0000 },
	{ 0x0000074b, 0x0080 },
	{ 0x0000074c, 0x0000 },
	{ 0x0000074d, 0x0080 },
	{ 0x0000074e, 0x0000 },
	{ 0x0000074f, 0x0080 },
	{ 0x00000750, 0x0000 },
	{ 0x00000751, 0x0080 },
	{ 0x00000752, 0x0000 },
	{ 0x00000753, 0x0080 },
	{ 0x00000754, 0x0000 },
	{ 0x00000755, 0x0080 },
	{ 0x00000756, 0x0000 },
	{ 0x00000757, 0x0080 },
	{ 0x00000758, 0x0000 },
	{ 0x00000759, 0x0080 },
	{ 0x0000075a, 0x0000 },
	{ 0x0000075b, 0x0080 },
	{ 0x0000075c, 0x0000 },
	{ 0x0000075d, 0x0080 },
	{ 0x0000075e, 0x0000 },
	{ 0x0000075f, 0x0080 },
	{ 0x00000760, 0x0000 },
	{ 0x00000761, 0x0080 },
	{ 0x00000762, 0x0000 },
	{ 0x00000763, 0x0080 },
	{ 0x00000764, 0x0000 },
	{ 0x00000765, 0x0080 },
	{ 0x00000766, 0x0000 },
	{ 0x00000767, 0x0080 },
	{ 0x00000768, 0x0000 },
	{ 0x00000769, 0x0080 },
	{ 0x0000076a, 0x0000 },
	{ 0x0000076b, 0x0080 },
	{ 0x0000076c, 0x0000 },
	{ 0x0000076d, 0x0080 },
	{ 0x0000076e, 0x0000 },
	{ 0x0000076f, 0x0080 },
	{ 0x00000770, 0x0000 }, /* R1904  - AIF2TX7MIX Input 1 Source */
	{ 0x00000771, 0x0080 }, /* R1905  - AIF2TX7MIX Input 1 Volume */
	{ 0x00000772, 0x0000 }, /* R1906  - AIF2TX7MIX Input 2 Source */
	{ 0x00000773, 0x0080 }, /* R1907  - AIF2TX7MIX Input 2 Volume */
	{ 0x00000774, 0x0000 }, /* R1908  - AIF2TX7MIX Input 3 Source */
	{ 0x00000775, 0x0080 }, /* R1909  - AIF2TX7MIX Input 3 Volume */
	{ 0x00000776, 0x0000 }, /* R1910  - AIF2TX7MIX Input 4 Source */
	{ 0x00000777, 0x0080 }, /* R1911  - AIF2TX7MIX Input 4 Volume */
	{ 0x00000778, 0x0000 }, /* R1912  - AIF2TX8MIX Input 1 Source */
	{ 0x00000779, 0x0080 }, /* R1913  - AIF2TX8MIX Input 1 Volume */
	{ 0x0000077a, 0x0000 }, /* R1914  - AIF2TX8MIX Input 2 Source */
	{ 0x0000077b, 0x0080 }, /* R1915  - AIF2TX8MIX Input 2 Volume */
	{ 0x0000077c, 0x0000 }, /* R1916  - AIF2TX8MIX Input 3 Source */
	{ 0x0000077d, 0x0080 }, /* R1917  - AIF2TX8MIX Input 3 Volume */
	{ 0x0000077e, 0x0000 }, /* R1918  - AIF2TX8MIX Input 4 Source */
	{ 0x0000077f, 0x0080 }, /* R1919  - AIF2TX8MIX Input 4 Volume */
	{ 0x00000780, 0x0000 },
	{ 0x00000781, 0x0080 },
	{ 0x00000782, 0x0000 },
	{ 0x00000783, 0x0080 },
	{ 0x00000784, 0x0000 },
	{ 0x00000785, 0x0080 },
	{ 0x00000786, 0x0000 },
	{ 0x00000787, 0x0080 },
	{ 0x00000788, 0x0000 },
	{ 0x00000789, 0x0080 },
	{ 0x0000078a, 0x0000 },
	{ 0x0000078b, 0x0080 },
	{ 0x0000078c, 0x0000 },
	{ 0x0000078d, 0x0080 },
	{ 0x0000078e, 0x0000 },
	{ 0x0000078f, 0x0080 },
	{ 0x000007a0, 0x0000 }, /* R1952 - AIF4TX1MIX Input 1 Source */
	{ 0x000007a1, 0x0080 }, /* R1953 - AIF4TX1MIX Input 1 Volume */
	{ 0x000007a2, 0x0000 }, /* R1954 - AIF4TX1MIX Input 2 Source */
	{ 0x000007a3, 0x0080 }, /* R1955 - AIF4TX1MIX Input 2 Volume */
	{ 0x000007a4, 0x0000 }, /* R1956 - AIF4TX1MIX Input 3 Source */
	{ 0x000007a5, 0x0080 }, /* R1957 - AIF4TX1MIX Input 3 Volume */
	{ 0x000007a6, 0x0000 }, /* R1958 - AIF4TX1MIX Input 4 Source */
	{ 0x000007a7, 0x0080 }, /* R1959 - AIF4TX1MIX Input 4 Volume */
	{ 0x000007a8, 0x0000 }, /* R1960 - AIF4TX2MIX Input 1 Source */
	{ 0x000007a9, 0x0080 }, /* R1961 - AIF4TX2MIX Input 1 Volume */
	{ 0x000007aa, 0x0000 }, /* R1962 - AIF4TX2MIX Input 2 Source */
	{ 0x000007ab, 0x0080 }, /* R1963 - AIF4TX2MIX Input 2 Volume */
	{ 0x000007ac, 0x0000 }, /* R1964 - AIF4TX2MIX Input 3 Source */
	{ 0x000007ad, 0x0080 }, /* R1965 - AIF4TX2MIX Input 3 Volume */
	{ 0x000007ae, 0x0000 }, /* R1966 - AIF4TX2MIX Input 4 Source */
	{ 0x000007af, 0x0080 }, /* R1967 - AIF4TX2MIX Input 4 Volume */
	{ 0x000007c0, 0x0000 },
	{ 0x000007c1, 0x0080 },
	{ 0x000007c2, 0x0000 },
	{ 0x000007c3, 0x0080 },
	{ 0x000007c4, 0x0000 },
	{ 0x000007c5, 0x0080 },
	{ 0x000007c6, 0x0000 },
	{ 0x000007c7, 0x0080 },
	{ 0x000007c8, 0x0000 },
	{ 0x000007c9, 0x0080 },
	{ 0x000007ca, 0x0000 },
	{ 0x000007cb, 0x0080 },
	{ 0x000007cc, 0x0000 },
	{ 0x000007cd, 0x0080 },
	{ 0x000007ce, 0x0000 },
	{ 0x000007cf, 0x0080 },
	{ 0x000007d0, 0x0000 },
	{ 0x000007d1, 0x0080 },
	{ 0x000007d2, 0x0000 },
	{ 0x000007d3, 0x0080 },
	{ 0x000007d4, 0x0000 },
	{ 0x000007d5, 0x0080 },
	{ 0x000007d6, 0x0000 },
	{ 0x000007d7, 0x0080 },
	{ 0x000007d8, 0x0000 },
	{ 0x000007d9, 0x0080 },
	{ 0x000007da, 0x0000 },
	{ 0x000007db, 0x0080 },
	{ 0x000007dc, 0x0000 },
	{ 0x000007dd, 0x0080 },
	{ 0x000007de, 0x0000 },
	{ 0x000007df, 0x0080 },
	{ 0x000007e0, 0x0000 },
	{ 0x000007e1, 0x0080 },
	{ 0x000007e2, 0x0000 },
	{ 0x000007e3, 0x0080 },
	{ 0x000007e4, 0x0000 },
	{ 0x000007e5, 0x0080 },
	{ 0x000007e6, 0x0000 },
	{ 0x000007e7, 0x0080 },
	{ 0x000007e8, 0x0000 },
	{ 0x000007e9, 0x0080 },
	{ 0x000007ea, 0x0000 },
	{ 0x000007eb, 0x0080 },
	{ 0x000007ec, 0x0000 },
	{ 0x000007ed, 0x0080 },
	{ 0x000007ee, 0x0000 },
	{ 0x000007ef, 0x0080 },
	{ 0x000007f0, 0x0000 },
	{ 0x000007f1, 0x0080 },
	{ 0x000007f2, 0x0000 },
	{ 0x000007f3, 0x0080 },
	{ 0x000007f4, 0x0000 },
	{ 0x000007f5, 0x0080 },
	{ 0x000007f6, 0x0000 },
	{ 0x000007f7, 0x0080 },
	{ 0x000007f8, 0x0000 },
	{ 0x000007f9, 0x0080 },
	{ 0x000007fa, 0x0000 },
	{ 0x000007fb, 0x0080 },
	{ 0x000007fc, 0x0000 },
	{ 0x000007fd, 0x0080 },
	{ 0x000007fe, 0x0000 },
	{ 0x000007ff, 0x0080 },
	{ 0x00000800, 0x0000 },
	{ 0x00000801, 0x0080 },
	{ 0x00000808, 0x0000 },
	{ 0x00000809, 0x0080 },
	{ 0x00000880, 0x0000 },
	{ 0x00000881, 0x0080 },
	{ 0x00000882, 0x0000 },
	{ 0x00000883, 0x0080 },
	{ 0x00000884, 0x0000 },
	{ 0x00000885, 0x0080 },
	{ 0x00000886, 0x0000 },
	{ 0x00000887, 0x0080 },
	{ 0x00000888, 0x0000 },
	{ 0x00000889, 0x0080 },
	{ 0x0000088a, 0x0000 },
	{ 0x0000088b, 0x0080 },
	{ 0x0000088c, 0x0000 },
	{ 0x0000088d, 0x0080 },
	{ 0x0000088e, 0x0000 },
	{ 0x0000088f, 0x0080 },
	{ 0x00000890, 0x0000 },
	{ 0x00000891, 0x0080 },
	{ 0x00000892, 0x0000 },
	{ 0x00000893, 0x0080 },
	{ 0x00000894, 0x0000 },
	{ 0x00000895, 0x0080 },
	{ 0x00000896, 0x0000 },
	{ 0x00000897, 0x0080 },
	{ 0x00000898, 0x0000 },
	{ 0x00000899, 0x0080 },
	{ 0x0000089a, 0x0000 },
	{ 0x0000089b, 0x0080 },
	{ 0x0000089c, 0x0000 },
	{ 0x0000089d, 0x0080 },
	{ 0x0000089e, 0x0000 },
	{ 0x0000089f, 0x0080 },
	{ 0x000008c0, 0x0000 },
	{ 0x000008c1, 0x0080 },
	{ 0x000008c2, 0x0000 },
	{ 0x000008c3, 0x0080 },
	{ 0x000008c4, 0x0000 },
	{ 0x000008c5, 0x0080 },
	{ 0x000008c6, 0x0000 },
	{ 0x000008c7, 0x0080 },
	{ 0x000008c8, 0x0000 },
	{ 0x000008c9, 0x0080 },
	{ 0x000008ca, 0x0000 },
	{ 0x000008cb, 0x0080 },
	{ 0x000008cc, 0x0000 },
	{ 0x000008cd, 0x0080 },
	{ 0x000008ce, 0x0000 },
	{ 0x000008cf, 0x0080 },
	{ 0x000008d0, 0x0000 },
	{ 0x000008d1, 0x0080 },
	{ 0x000008d2, 0x0000 },
	{ 0x000008d3, 0x0080 },
	{ 0x000008d4, 0x0000 },
	{ 0x000008d5, 0x0080 },
	{ 0x000008d6, 0x0000 },
	{ 0x000008d7, 0x0080 },
	{ 0x000008d8, 0x0000 },
	{ 0x000008d9, 0x0080 },
	{ 0x000008da, 0x0000 },
	{ 0x000008db, 0x0080 },
	{ 0x000008dc, 0x0000 },
	{ 0x000008dd, 0x0080 },
	{ 0x000008de, 0x0000 },
	{ 0x000008df, 0x0080 },
	{ 0x00000900, 0x0000 },
	{ 0x00000901, 0x0080 },
	{ 0x00000902, 0x0000 },
	{ 0x00000903, 0x0080 },
	{ 0x00000904, 0x0000 },
	{ 0x00000905, 0x0080 },
	{ 0x00000906, 0x0000 },
	{ 0x00000907, 0x0080 },
	{ 0x00000908, 0x0000 },
	{ 0x00000909, 0x0080 },
	{ 0x0000090a, 0x0000 },
	{ 0x0000090b, 0x0080 },
	{ 0x0000090c, 0x0000 },
	{ 0x0000090d, 0x0080 },
	{ 0x0000090e, 0x0000 },
	{ 0x0000090f, 0x0080 },
	{ 0x00000910, 0x0000 },
	{ 0x00000911, 0x0080 },
	{ 0x00000912, 0x0000 },
	{ 0x00000913, 0x0080 },
	{ 0x00000914, 0x0000 },
	{ 0x00000915, 0x0080 },
	{ 0x00000916, 0x0000 },
	{ 0x00000917, 0x0080 },
	{ 0x00000918, 0x0000 },
	{ 0x00000919, 0x0080 },
	{ 0x0000091a, 0x0000 },
	{ 0x0000091b, 0x0080 },
	{ 0x0000091c, 0x0000 },
	{ 0x0000091d, 0x0080 },
	{ 0x0000091e, 0x0000 },
	{ 0x0000091f, 0x0080 },
	{ 0x00000940, 0x0000 },
	{ 0x00000941, 0x0080 },
	{ 0x00000942, 0x0000 },
	{ 0x00000943, 0x0080 },
	{ 0x00000944, 0x0000 },
	{ 0x00000945, 0x0080 },
	{ 0x00000946, 0x0000 },
	{ 0x00000947, 0x0080 },
	{ 0x00000948, 0x0000 },
	{ 0x00000949, 0x0080 },
	{ 0x0000094a, 0x0000 },
	{ 0x0000094b, 0x0080 },
	{ 0x0000094c, 0x0000 },
	{ 0x0000094d, 0x0080 },
	{ 0x0000094e, 0x0000 },
	{ 0x0000094f, 0x0080 },
	{ 0x00000950, 0x0000 },
	{ 0x00000958, 0x0000 },
	{ 0x00000960, 0x0000 },
	{ 0x00000968, 0x0000 },
	{ 0x00000970, 0x0000 },
	{ 0x00000978, 0x0000 },
	{ 0x00000980, 0x0000 },
	{ 0x00000981, 0x0080 },
	{ 0x00000982, 0x0000 },
	{ 0x00000983, 0x0080 },
	{ 0x00000984, 0x0000 },
	{ 0x00000985, 0x0080 },
	{ 0x00000986, 0x0000 },
	{ 0x00000987, 0x0080 },
	{ 0x00000988, 0x0000 },
	{ 0x00000989, 0x0080 },
	{ 0x0000098a, 0x0000 },
	{ 0x0000098b, 0x0080 },
	{ 0x0000098c, 0x0000 },
	{ 0x0000098d, 0x0080 },
	{ 0x0000098e, 0x0000 },
	{ 0x0000098f, 0x0080 },
	{ 0x00000990, 0x0000 },
	{ 0x00000998, 0x0000 },
	{ 0x000009a0, 0x0000 },
	{ 0x000009a8, 0x0000 },
	{ 0x000009b0, 0x0000 },
	{ 0x000009b8, 0x0000 },
	{ 0x000009c0, 0x0000 },
	{ 0x000009c1, 0x0080 },
	{ 0x000009c2, 0x0000 },
	{ 0x000009c3, 0x0080 },
	{ 0x000009c4, 0x0000 },
	{ 0x000009c5, 0x0080 },
	{ 0x000009c6, 0x0000 },
	{ 0x000009c7, 0x0080 },
	{ 0x000009c8, 0x0000 },
	{ 0x000009c9, 0x0080 },
	{ 0x000009ca, 0x0000 },
	{ 0x000009cb, 0x0080 },
	{ 0x000009cc, 0x0000 },
	{ 0x000009cd, 0x0080 },
	{ 0x000009ce, 0x0000 },
	{ 0x000009cf, 0x0080 },
	{ 0x000009d0, 0x0000 },
	{ 0x000009d8, 0x0000 },
	{ 0x000009e0, 0x0000 },
	{ 0x000009e8, 0x0000 },
	{ 0x000009f0, 0x0000 },
	{ 0x000009f8, 0x0000 },
	{ 0x00000a00, 0x0000 },
	{ 0x00000a01, 0x0080 },
	{ 0x00000a02, 0x0000 },
	{ 0x00000a03, 0x0080 },
	{ 0x00000a04, 0x0000 },
	{ 0x00000a05, 0x0080 },
	{ 0x00000a06, 0x0000 },
	{ 0x00000a07, 0x0080 },
	{ 0x00000a08, 0x0000 },
	{ 0x00000a09, 0x0080 },
	{ 0x00000a0a, 0x0000 },
	{ 0x00000a0b, 0x0080 },
	{ 0x00000a0c, 0x0000 },
	{ 0x00000a0d, 0x0080 },
	{ 0x00000a0e, 0x0000 },
	{ 0x00000a0f, 0x0080 },
	{ 0x00000a10, 0x0000 },
	{ 0x00000a18, 0x0000 },
	{ 0x00000a20, 0x0000 },
	{ 0x00000a28, 0x0000 },
	{ 0x00000a30, 0x0000 },
	{ 0x00000a38, 0x0000 },
	{ 0x00000a40, 0x0000 },
	{ 0x00000a41, 0x0080 },
	{ 0x00000a42, 0x0000 },
	{ 0x00000a43, 0x0080 },
	{ 0x00000a44, 0x0000 },
	{ 0x00000a45, 0x0080 },
	{ 0x00000a46, 0x0000 },
	{ 0x00000a47, 0x0080 },
	{ 0x00000a48, 0x0000 },
	{ 0x00000a49, 0x0080 },
	{ 0x00000a4a, 0x0000 },
	{ 0x00000a4b, 0x0080 },
	{ 0x00000a4c, 0x0000 },
	{ 0x00000a4d, 0x0080 },
	{ 0x00000a4e, 0x0000 },
	{ 0x00000a4f, 0x0080 },
	{ 0x00000a50, 0x0000 },
	{ 0x00000a58, 0x0000 },
	{ 0x00000a60, 0x0000 },
	{ 0x00000a68, 0x0000 },
	{ 0x00000a70, 0x0000 },
	{ 0x00000a78, 0x0000 },
	{ 0x00000a80, 0x0000 },
	{ 0x00000a88, 0x0000 },
	{ 0x00000a90, 0x0000 },
	{ 0x00000a98, 0x0000 },
	{ 0x00000aa0, 0x0000 },
	{ 0x00000aa8, 0x0000 },
	{ 0x00000ab0, 0x0000 },
	{ 0x00000ab8, 0x0000 },
	{ 0x00000b00, 0x0000 },
	{ 0x00000b08, 0x0000 },
	{ 0x00000b10, 0x0000 },
	{ 0x00000b18, 0x0000 },
	{ 0x00000b20, 0x0000 },
	{ 0x00000b28, 0x0000 },
	{ 0x00000b30, 0x0000 },
	{ 0x00000b38, 0x0000 },
	{ 0x00000b40, 0x0000 },
	{ 0x00000b48, 0x0000 },
	{ 0x00000b50, 0x0000 },
	{ 0x00000b58, 0x0000 },
	{ 0x00000b60, 0x0000 },
	{ 0x00000b68, 0x0000 },
	{ 0x00000b70, 0x0000 },
	{ 0x00000b78, 0x0000 },
	{ 0x00000b80, 0x0000 },
	{ 0x00000b88, 0x0000 },
	{ 0x00000ba0, 0x0000 },
	{ 0x00000ba8, 0x0000 },
	{ 0x00000bc0, 0x0000 }, /* R3008  - ISRC4DEC1MIX Input 1 Source */
	{ 0x00000bc8, 0x0000 }, /* R3016  - ISRC4DEC2MIX Input 1 Source */
	{ 0x00000be0, 0x0000 }, /* R3040  - ISRC4INT1MIX Input 1 Source */
	{ 0x00000be8, 0x0000 }, /* R3048  - ISRC4INT2MIX Input 1 Source */
	{ 0x00000c00, 0x0000 },
	{ 0x00000c01, 0x0080 },
	{ 0x00000c02, 0x0000 },
	{ 0x00000c03, 0x0080 },
	{ 0x00000c04, 0x0000 },
	{ 0x00000c05, 0x0080 },
	{ 0x00000c06, 0x0000 },
	{ 0x00000c07, 0x0080 },
	{ 0x00000c08, 0x0000 },
	{ 0x00000c09, 0x0080 },
	{ 0x00000c0a, 0x0000 },
	{ 0x00000c0b, 0x0080 },
	{ 0x00000c0c, 0x0000 },
	{ 0x00000c0d, 0x0080 },
	{ 0x00000c0e, 0x0000 },
	{ 0x00000c0f, 0x0080 },
	{ 0x00000c10, 0x0000 }, /* R3088 (0xC10) - DSP6AUX1MIX Input 1 */
	{ 0x00000c18, 0x0000 }, /* R3088 (0xC18) - DSP6AUX2MIX Input 1 */
	{ 0x00000c20, 0x0000 }, /* R3088 (0xC20) - DSP6AUX3MIX Input 1 */
	{ 0x00000c28, 0x0000 }, /* R3088 (0xC28) - DSP6AUX4MIX Input 1 */
	{ 0x00000c30, 0x0000 }, /* R3088 (0xC30) - DSP6AUX5MIX Input 1 */
	{ 0x00000c38, 0x0000 }, /* R3088 (0xC38) - DSP6AUX6MIX Input 1 */
	{ 0x00000c40, 0x0000 },
	{ 0x00000c41, 0x0080 },
	{ 0x00000c42, 0x0000 },
	{ 0x00000c43, 0x0080 },
	{ 0x00000c44, 0x0000 },
	{ 0x00000c45, 0x0080 },
	{ 0x00000c46, 0x0000 },
	{ 0x00000c47, 0x0080 },
	{ 0x00000c48, 0x0000 },
	{ 0x00000c49, 0x0080 },
	{ 0x00000c4a, 0x0000 },
	{ 0x00000c4b, 0x0080 },
	{ 0x00000c4c, 0x0000 },
	{ 0x00000c4d, 0x0080 },
	{ 0x00000c4e, 0x0000 },
	{ 0x00000c4f, 0x0080 },
	{ 0x00000c50, 0x0000 },
	{ 0x00000c58, 0x0000 },
	{ 0x00000c60, 0x0000 },
	{ 0x00000c68, 0x0000 },
	{ 0x00000c70, 0x0000 },
	{ 0x00000c78, 0x0000 },
	{ 0x00000e00, 0x0000 }, /* R3584 (0xE00) - FX_Ctrl1 */
	{ 0x00000e10, 0x6318 }, /* R3600 (0xE10) - EQ1_1 */
	{ 0x00000e11, 0x6300 }, /* R3601 (0xE11) - EQ1_2 */
	{ 0x00000e12, 0x0fc8 }, /* R3602 (0xE12) - EQ1_3 */
	{ 0x00000e13, 0x03fe }, /* R3603 (0xE13) - EQ1_4 */
	{ 0x00000e14, 0x00e0 }, /* R3604 (0xE14) - EQ1_5 */
	{ 0x00000e15, 0x1ec4 }, /* R3605 (0xE15) - EQ1_6 */
	{ 0x00000e16, 0xf136 }, /* R3606 (0xE16) - EQ1_7 */
	{ 0x00000e17, 0x0409 }, /* R3607 (0xE17) - EQ1_8 */
	{ 0x00000e18, 0x04cc }, /* R3608 (0xE18) - EQ1_9 */
	{ 0x00000e19, 0x1c9b }, /* R3609 (0xE19) - EQ1_10 */
	{ 0x00000e1a, 0xf337 }, /* R3610 (0xE1A) - EQ1_11 */
	{ 0x00000e1b, 0x040b }, /* R3611 (0xE1B) - EQ1_12 */
	{ 0x00000e1c, 0x0cbb }, /* R3612 (0xE1C) - EQ1_13 */
	{ 0x00000e1d, 0x16f8 }, /* R3613 (0xE1D) - EQ1_14 */
	{ 0x00000e1e, 0xf7d9 }, /* R3614 (0xE1E) - EQ1_15 */
	{ 0x00000e1f, 0x040a }, /* R3615 (0xE1F) - EQ1_16 */
	{ 0x00000e20, 0x1f14 }, /* R3616 (0xE20) - EQ1_17 */
	{ 0x00000e21, 0x058c }, /* R3617 (0xE21) - EQ1_18 */
	{ 0x00000e22, 0x0563 }, /* R3618 (0xE22) - EQ1_19 */
	{ 0x00000e23, 0x4000 }, /* R3619 (0xE23) - EQ1_20 */
	{ 0x00000e24, 0x0b75 }, /* R3620 (0xE24) - EQ1_21 */
	{ 0x00000e26, 0x6318 }, /* R3622 (0xE26) - EQ2_1 */
	{ 0x00000e27, 0x6300 }, /* R3623 (0xE27) - EQ2_2 */
	{ 0x00000e28, 0x0fc8 }, /* R3624 (0xE28) - EQ2_3 */
	{ 0x00000e29, 0x03fe }, /* R3625 (0xE29) - EQ2_4 */
	{ 0x00000e2a, 0x00e0 }, /* R3626 (0xE2A) - EQ2_5 */
	{ 0x00000e2b, 0x1ec4 }, /* R3627 (0xE2B) - EQ2_6 */
	{ 0x00000e2c, 0xf136 }, /* R3628 (0xE2C) - EQ2_7 */
	{ 0x00000e2d, 0x0409 }, /* R3629 (0xE2D) - EQ2_8 */
	{ 0x00000e2e, 0x04cc }, /* R3630 (0xE2E) - EQ2_9 */
	{ 0x00000e2f, 0x1c9b }, /* R3631 (0xE2F) - EQ2_10 */
	{ 0x00000e30, 0xf337 }, /* R3632 (0xE30) - EQ2_11 */
	{ 0x00000e31, 0x040b }, /* R3633 (0xE31) - EQ2_12 */
	{ 0x00000e32, 0x0cbb }, /* R3634 (0xE32) - EQ2_13 */
	{ 0x00000e33, 0x16f8 }, /* R3635 (0xE33) - EQ2_14 */
	{ 0x00000e34, 0xf7d9 }, /* R3636 (0xE34) - EQ2_15 */
	{ 0x00000e35, 0x040a }, /* R3637 (0xE35) - EQ2_16 */
	{ 0x00000e36, 0x1f14 }, /* R3638 (0xE36) - EQ2_17 */
	{ 0x00000e37, 0x058c }, /* R3639 (0xE37) - EQ2_18 */
	{ 0x00000e38, 0x0563 }, /* R3640 (0xE38) - EQ2_19 */
	{ 0x00000e39, 0x4000 }, /* R3641 (0xE39) - EQ2_20 */
	{ 0x00000e3a, 0x0b75 }, /* R3642 (0xE3A) - EQ2_21 */
	{ 0x00000e3c, 0x6318 }, /* R3644 (0xE3C) - EQ3_1 */
	{ 0x00000e3d, 0x6300 }, /* R3645 (0xE3D) - EQ3_2 */
	{ 0x00000e3e, 0x0fc8 }, /* R3646 (0xE3E) - EQ3_3 */
	{ 0x00000e3f, 0x03fe }, /* R3647 (0xE3F) - EQ3_4 */
	{ 0x00000e40, 0x00e0 }, /* R3648 (0xE40) - EQ3_5 */
	{ 0x00000e41, 0x1ec4 }, /* R3649 (0xE41) - EQ3_6 */
	{ 0x00000e42, 0xf136 }, /* R3650 (0xE42) - EQ3_7 */
	{ 0x00000e43, 0x0409 }, /* R3651 (0xE43) - EQ3_8 */
	{ 0x00000e44, 0x04cc }, /* R3652 (0xE44) - EQ3_9 */
	{ 0x00000e45, 0x1c9b }, /* R3653 (0xE45) - EQ3_10 */
	{ 0x00000e46, 0xf337 }, /* R3654 (0xE46) - EQ3_11 */
	{ 0x00000e47, 0x040b }, /* R3655 (0xE47) - EQ3_12 */
	{ 0x00000e48, 0x0cbb }, /* R3656 (0xE48) - EQ3_13 */
	{ 0x00000e49, 0x16f8 }, /* R3657 (0xE49) - EQ3_14 */
	{ 0x00000e4a, 0xf7d9 }, /* R3658 (0xE4A) - EQ3_15 */
	{ 0x00000e4b, 0x040a }, /* R3659 (0xE4B) - EQ3_16 */
	{ 0x00000e4c, 0x1f14 }, /* R3660 (0xE4C) - EQ3_17 */
	{ 0x00000e4d, 0x058c }, /* R3661 (0xE4D) - EQ3_18 */
	{ 0x00000e4e, 0x0563 }, /* R3662 (0xE4E) - EQ3_19 */
	{ 0x00000e4f, 0x4000 }, /* R3663 (0xE4F) - EQ3_20 */
	{ 0x00000e50, 0x0b75 }, /* R3664 (0xE50) - EQ3_21 */
	{ 0x00000e52, 0x6318 }, /* R3666 (0xE52) - EQ4_1 */
	{ 0x00000e53, 0x6300 }, /* R3667 (0xE53) - EQ4_2 */
	{ 0x00000e54, 0x0fc8 }, /* R3668 (0xE54) - EQ4_3 */
	{ 0x00000e55, 0x03fe }, /* R3669 (0xE55) - EQ4_4 */
	{ 0x00000e56, 0x00e0 }, /* R3670 (0xE56) - EQ4_5 */
	{ 0x00000e57, 0x1ec4 }, /* R3671 (0xE57) - EQ4_6 */
	{ 0x00000e58, 0xf136 }, /* R3672 (0xE58) - EQ4_7 */
	{ 0x00000e59, 0x0409 }, /* R3673 (0xE59) - EQ4_8 */
	{ 0x00000e5a, 0x04cc }, /* R3674 (0xE5A) - EQ4_9 */
	{ 0x00000e5b, 0x1c9b }, /* R3675 (0xE5B) - EQ4_10 */
	{ 0x00000e5c, 0xf337 }, /* R3676 (0xE5C) - EQ4_11 */
	{ 0x00000e5d, 0x040b }, /* R3677 (0xE5D) - EQ4_12 */
	{ 0x00000e5e, 0x0cbb }, /* R3678 (0xE5E) - EQ4_13 */
	{ 0x00000e5f, 0x16f8 }, /* R3679 (0xE5F) - EQ4_14 */
	{ 0x00000e60, 0xf7d9 }, /* R3680 (0xE60) - EQ4_15 */
	{ 0x00000e61, 0x040a }, /* R3681 (0xE61) - EQ4_16 */
	{ 0x00000e62, 0x1f14 }, /* R3682 (0xE62) - EQ4_17 */
	{ 0x00000e63, 0x058c }, /* R3683 (0xE63) - EQ4_18 */
	{ 0x00000e64, 0x0563 }, /* R3684 (0xE64) - EQ4_19 */
	{ 0x00000e65, 0x4000 }, /* R3685 (0xE65) - EQ4_20 */
	{ 0x00000e66, 0x0b75 }, /* R3686 (0xE66) - EQ4_21 */
	{ 0x00000e80, 0x0018 }, /* R3712 (0xE80) - DRC1 ctrl1 */
	{ 0x00000e81, 0x0933 }, /* R3713 (0xE81) - DRC1 ctrl2 */
	{ 0x00000e82, 0x0018 }, /* R3714 (0xE82) - DRC1 ctrl3 */
	{ 0x00000e83, 0x0000 }, /* R3715 (0xE83) - DRC1 ctrl4 */
	{ 0x00000e84, 0x0000 }, /* R3716 (0xE84) - DRC1 ctrl5 */
	{ 0x00000e88, 0x0933 }, /* R3720 (0xE88) - DRC2 ctrl1 */
	{ 0x00000e89, 0x0018 }, /* R3721 (0xE89) - DRC2 ctrl2 */
	{ 0x00000e8a, 0x0000 }, /* R3722 (0xE8A) - DRC2 ctrl3 */
	{ 0x00000e8b, 0x0000 }, /* R3723 (0xE8B) - DRC2 ctrl4 */
	{ 0x00000e8c, 0x0040 }, /* R3724 (0xE8C) - DRC2 ctrl5 */
	{ 0x00000ec0, 0x0000 }, /* R3776 (0xEC0) - HPLPF1_1 */
	{ 0x00000ec1, 0x0000 }, /* R3777 (0xEC1) - HPLPF1_2 */
	{ 0x00000ec4, 0x0000 }, /* R3780 (0xEC4) - HPLPF2_1 */
	{ 0x00000ec5, 0x0000 }, /* R3781 (0xEC5) - HPLPF2_2 */
	{ 0x00000ec8, 0x0000 }, /* R3784 (0xEC8) - HPLPF3_1 */
	{ 0x00000ec9, 0x0000 }, /* R3785 (0xEC9) - HPLPF3_2 */
	{ 0x00000ecc, 0x0000 }, /* R3788 (0xECC) - HPLPF4_1 */
	{ 0x00000ecd, 0x0000 }, /* R3789 (0xECD) - HPLPF4_2 */
	{ 0x00000ed0, 0x0000 }, /* R3792 (0xED0) - ASRC2_ENABLE */
	{ 0x00000ed2, 0x0000 }, /* R3794 (0xED2) - ASRC2_RATE1 */
	{ 0x00000ed3, 0x4000 }, /* R3795 (0xED3) - ASRC2_RATE2 */
	{ 0x00000ee0, 0x0000 }, /* R3808 (0xEE0) - ASRC1_ENABLE */
	{ 0x00000ee2, 0x0000 }, /* R3810 (0xEE2) - ASRC1_RATE1 */
	{ 0x00000ee3, 0x4000 }, /* R3811 (0xEE3) - ASRC1_RATE2 */
	{ 0x00000ef0, 0x0000 }, /* R3824 (0xEF0) - ISRC 1 CTRL 1 */
	{ 0x00000ef1, 0x0001 }, /* R3825 (0xEF1) - ISRC 1 CTRL 2 */
	{ 0x00000ef2, 0x0000 }, /* R3826 (0xEF2) - ISRC 1 CTRL 3 */
	{ 0x00000ef3, 0x0000 }, /* R3827 (0xEF3) - ISRC 2 CTRL 1 */
	{ 0x00000ef4, 0x0001 }, /* R3828 (0xEF4) - ISRC 2 CTRL 2 */
	{ 0x00000ef5, 0x0000 }, /* R3829 (0xEF5) - ISRC 2 CTRL 3 */
	{ 0x00000ef6, 0x0000 }, /* R3830 (0xEF6) - ISRC 3 CTRL 1 */
	{ 0x00000ef7, 0x0001 }, /* R3831 (0xEF7) - ISRC 3 CTRL 2 */
	{ 0x00000ef8, 0x0000 }, /* R3832 (0xEF8) - ISRC 3 CTRL 3 */
	{ 0x00000ef9, 0x0000 }, /* R3833  - ISRC 4 CTRL 1 */
	{ 0x00000efa, 0x0001 }, /* R3834  - ISRC 4 CTRL 2 */
	{ 0x00000efb, 0x0000 }, /* R3835  - ISRC 4 CTRL 3 */
	{ 0x00000f00, 0x0000 }, /* R3840 (0xF00) - Clock Control */
	{ 0x00000F01, 0x0000 }, /* R3841  - ANC_SRC */
	{ 0x00000F02, 0x0000 }, /* R3842  - Arizona DSP Status */
	{ 0x00000F08, 0x001c }, /* R3848  - ANC Coefficient */
	{ 0x00000F09, 0x0000 }, /* R3849  - ANC Coefficient */
	{ 0x00000F0A, 0x0000 }, /* R3850  - ANC Coefficient */
	{ 0x00000F0B, 0x0000 }, /* R3851  - ANC Coefficient */
	{ 0x00000F0C, 0x0000 }, /* R3852  - ANC Coefficient */
	{ 0x00000F0D, 0x0000 }, /* R3853  - ANC Coefficient */
	{ 0x00000F0E, 0x0000 }, /* R3854  - ANC Coefficient */
	{ 0x00000F0F, 0x0000 }, /* R3855  - ANC Coefficient */
	{ 0x00000F10, 0x0000 }, /* R3856  - ANC Coefficient */
	{ 0x00000F11, 0x0000 }, /* R3857  - ANC Coefficient */
	{ 0x00000F12, 0x0000 }, /* R3858  - ANC Coefficient */
	{ 0x00000F15, 0x0000 }, /* R3861  - FCL Filter Control */
	{ 0x00000F17, 0x0004 }, /* R3863  - FCL ADC Reformatter Control */
	{ 0x00000F18, 0x0004 }, /* R3864  - ANC Coefficient */
	{ 0x00000F19, 0x0002 }, /* R3865  - ANC Coefficient */
	{ 0x00000F1A, 0x0000 }, /* R3866  - ANC Coefficient */
	{ 0x00000F1B, 0x0010 }, /* R3867  - ANC Coefficient */
	{ 0x00000F1C, 0x0000 }, /* R3868  - ANC Coefficient */
	{ 0x00000F1D, 0x0000 }, /* R3869  - ANC Coefficient */
	{ 0x00000F1E, 0x0000 }, /* R3870  - ANC Coefficient */
	{ 0x00000F1F, 0x0000 }, /* R3871  - ANC Coefficient */
	{ 0x00000F20, 0x0000 }, /* R3872  - ANC Coefficient */
	{ 0x00000F21, 0x0000 }, /* R3873  - ANC Coefficient */
	{ 0x00000F22, 0x0000 }, /* R3874  - ANC Coefficient */
	{ 0x00000F23, 0x0000 }, /* R3875  - ANC Coefficient */
	{ 0x00000F24, 0x0000 }, /* R3876  - ANC Coefficient */
	{ 0x00000F25, 0x0000 }, /* R3877  - ANC Coefficient */
	{ 0x00000F26, 0x0000 }, /* R3878  - ANC Coefficient */
	{ 0x00000F27, 0x0000 }, /* R3879  - ANC Coefficient */
	{ 0x00000F28, 0x0000 }, /* R3880  - ANC Coefficient */
	{ 0x00000F29, 0x0000 }, /* R3881  - ANC Coefficient */
	{ 0x00000F2A, 0x0000 }, /* R3882  - ANC Coefficient */
	{ 0x00000F2B, 0x0000 }, /* R3883  - ANC Coefficient */
	{ 0x00000F2C, 0x0000 }, /* R3884  - ANC Coefficient */
	{ 0x00000F2D, 0x0000 }, /* R3885  - ANC Coefficient */
	{ 0x00000F2E, 0x0000 }, /* R3886  - ANC Coefficient */
	{ 0x00000F2F, 0x0000 }, /* R3887  - ANC Coefficient */
	{ 0x00000F30, 0x0000 }, /* R3888  - ANC Coefficient */
	{ 0x00000F31, 0x0000 }, /* R3889  - ANC Coefficient */
	{ 0x00000F32, 0x0000 }, /* R3890  - ANC Coefficient */
	{ 0x00000F33, 0x0000 }, /* R3891  - ANC Coefficient */
	{ 0x00000F34, 0x0000 }, /* R3892  - ANC Coefficient */
	{ 0x00000F35, 0x0000 }, /* R3893  - ANC Coefficient */
	{ 0x00000F36, 0x0000 }, /* R3894  - ANC Coefficient */
	{ 0x00000F37, 0x0000 }, /* R3895  - ANC Coefficient */
	{ 0x00000F38, 0x0000 }, /* R3896  - ANC Coefficient */
	{ 0x00000F39, 0x0000 }, /* R3897  - ANC Coefficient */
	{ 0x00000F3A, 0x0000 }, /* R3898  - ANC Coefficient */
	{ 0x00000F3B, 0x0000 }, /* R3899  - ANC Coefficient */
	{ 0x00000F3C, 0x0000 }, /* R3900  - ANC Coefficient */
	{ 0x00000F3D, 0x0000 }, /* R3901  - ANC Coefficient */
	{ 0x00000F3E, 0x0000 }, /* R3902  - ANC Coefficient */
	{ 0x00000F3F, 0x0000 }, /* R3903  - ANC Coefficient */
	{ 0x00000F40, 0x0000 }, /* R3904  - ANC Coefficient */
	{ 0x00000F41, 0x0000 }, /* R3905  - ANC Coefficient */
	{ 0x00000F42, 0x0000 }, /* R3906  - ANC Coefficient */
	{ 0x00000F43, 0x0000 }, /* R3907  - ANC Coefficient */
	{ 0x00000F44, 0x0000 }, /* R3908  - ANC Coefficient */
	{ 0x00000F45, 0x0000 }, /* R3909  - ANC Coefficient */
	{ 0x00000F46, 0x0000 }, /* R3910  - ANC Coefficient */
	{ 0x00000F47, 0x0000 }, /* R3911  - ANC Coefficient */
	{ 0x00000F48, 0x0000 }, /* R3912  - ANC Coefficient */
	{ 0x00000F49, 0x0000 }, /* R3913  - ANC Coefficient */
	{ 0x00000F4A, 0x0000 }, /* R3914  - ANC Coefficient */
	{ 0x00000F4B, 0x0000 }, /* R3915  - ANC Coefficient */
	{ 0x00000F4C, 0x0000 }, /* R3916  - ANC Coefficient */
	{ 0x00000F4D, 0x0000 }, /* R3917  - ANC Coefficient */
	{ 0x00000F4E, 0x0000 }, /* R3918  - ANC Coefficient */
	{ 0x00000F4F, 0x0000 }, /* R3919  - ANC Coefficient */
	{ 0x00000F50, 0x0000 }, /* R3920  - ANC Coefficient */
	{ 0x00000F51, 0x0000 }, /* R3921  - ANC Coefficient */
	{ 0x00000F52, 0x0000 }, /* R3922  - ANC Coefficient */
	{ 0x00000F53, 0x0000 }, /* R3923  - ANC Coefficient */
	{ 0x00000F54, 0x0000 }, /* R3924  - ANC Coefficient */
	{ 0x00000F55, 0x0000 }, /* R3925  - ANC Coefficient */
	{ 0x00000F56, 0x0000 }, /* R3926  - ANC Coefficient */
	{ 0x00000F57, 0x0000 }, /* R3927  - ANC Coefficient */
	{ 0x00000F58, 0x0000 }, /* R3928  - ANC Coefficient */
	{ 0x00000F59, 0x0000 }, /* R3929  - ANC Coefficient */
	{ 0x00000F5A, 0x0000 }, /* R3930  - ANC Coefficient */
	{ 0x00000F5B, 0x0000 }, /* R3931  - ANC Coefficient */
	{ 0x00000F5C, 0x0000 }, /* R3932  - ANC Coefficient */
	{ 0x00000F5D, 0x0000 }, /* R3933  - ANC Coefficient */
	{ 0x00000F5E, 0x0000 }, /* R3934  - ANC Coefficient */
	{ 0x00000F5F, 0x0000 }, /* R3935  - ANC Coefficient */
	{ 0x00000F60, 0x0000 }, /* R3936  - ANC Coefficient */
	{ 0x00000F61, 0x0000 }, /* R3937  - ANC Coefficient */
	{ 0x00000F62, 0x0000 }, /* R3938  - ANC Coefficient */
	{ 0x00000F63, 0x0000 }, /* R3939  - ANC Coefficient */
	{ 0x00000F64, 0x0000 }, /* R3940  - ANC Coefficient */
	{ 0x00000F65, 0x0000 }, /* R3941  - ANC Coefficient */
	{ 0x00000F66, 0x0000 }, /* R3942  - ANC Coefficient */
	{ 0x00000F67, 0x0000 }, /* R3943  - ANC Coefficient */
	{ 0x00000F68, 0x0000 }, /* R3944  - ANC Coefficient */
	{ 0x00000F69, 0x0000 }, /* R3945  - ANC Coefficient */
	{ 0x00000F71, 0x0000 }, /* R3953  - FCR Filter Control */
	{ 0x00000F73, 0x0004 }, /* R3955  - FCR ADC Reformatter Control */
	{ 0x00000F74, 0x0004 }, /* R3956  - ANC Coefficient */
	{ 0x00000F75, 0x0002 }, /* R3957  - ANC Coefficient */
	{ 0x00000F76, 0x0000 }, /* R3958  - ANC Coefficient */
	{ 0x00000F77, 0x0010 }, /* R3959  - ANC Coefficient */
	{ 0x00000F78, 0x0000 }, /* R3960  - ANC Coefficient */
	{ 0x00000F79, 0x0000 }, /* R3961  - ANC Coefficient */
	{ 0x00000F7A, 0x0000 }, /* R3962  - ANC Coefficient */
	{ 0x00000F7B, 0x0000 }, /* R3963  - ANC Coefficient */
	{ 0x00000F7C, 0x0000 }, /* R3964  - ANC Coefficient */
	{ 0x00000F7D, 0x0000 }, /* R3965  - ANC Coefficient */
	{ 0x00000F7E, 0x0000 }, /* R3966  - ANC Coefficient */
	{ 0x00000F7F, 0x0000 }, /* R3967  - ANC Coefficient */
	{ 0x00000F80, 0x0000 }, /* R3968  - ANC Coefficient */
	{ 0x00000F81, 0x0000 }, /* R3969  - ANC Coefficient */
	{ 0x00000F82, 0x0000 }, /* R3970  - ANC Coefficient */
	{ 0x00000F83, 0x0000 }, /* R3971  - ANC Coefficient */
	{ 0x00000F84, 0x0000 }, /* R3972  - ANC Coefficient */
	{ 0x00000F85, 0x0000 }, /* R3973  - ANC Coefficient */
	{ 0x00000F86, 0x0000 }, /* R3974  - ANC Coefficient */
	{ 0x00000F87, 0x0000 }, /* R3975  - ANC Coefficient */
	{ 0x00000F88, 0x0000 }, /* R3976  - ANC Coefficient */
	{ 0x00000F89, 0x0000 }, /* R3977  - ANC Coefficient */
	{ 0x00000F8A, 0x0000 }, /* R3978  - ANC Coefficient */
	{ 0x00000F8B, 0x0000 }, /* R3979  - ANC Coefficient */
	{ 0x00000F8C, 0x0000 }, /* R3980  - ANC Coefficient */
	{ 0x00000F8D, 0x0000 }, /* R3981  - ANC Coefficient */
	{ 0x00000F8E, 0x0000 }, /* R3982  - ANC Coefficient */
	{ 0x00000F8F, 0x0000 }, /* R3983  - ANC Coefficient */
	{ 0x00000F90, 0x0000 }, /* R3984  - ANC Coefficient */
	{ 0x00000F91, 0x0000 }, /* R3985  - ANC Coefficient */
	{ 0x00000F92, 0x0000 }, /* R3986  - ANC Coefficient */
	{ 0x00000F93, 0x0000 }, /* R3987  - ANC Coefficient */
	{ 0x00000F94, 0x0000 }, /* R3988  - ANC Coefficient */
	{ 0x00000F95, 0x0000 }, /* R3989  - ANC Coefficient */
	{ 0x00000F96, 0x0000 }, /* R3990  - ANC Coefficient */
	{ 0x00000F97, 0x0000 }, /* R3991  - ANC Coefficient */
	{ 0x00000F98, 0x0000 }, /* R3992  - ANC Coefficient */
	{ 0x00000F99, 0x0000 }, /* R3993  - ANC Coefficient */
	{ 0x00000F9A, 0x0000 }, /* R3994  - ANC Coefficient */
	{ 0x00000F9B, 0x0000 }, /* R3995  - ANC Coefficient */
	{ 0x00000F9C, 0x0000 }, /* R3996  - ANC Coefficient */
	{ 0x00000F9D, 0x0000 }, /* R3997  - ANC Coefficient */
	{ 0x00000F9E, 0x0000 }, /* R3998  - ANC Coefficient */
	{ 0x00000F9F, 0x0000 }, /* R3999  - ANC Coefficient */
	{ 0x00000FA0, 0x0000 }, /* R4000  - ANC Coefficient */
	{ 0x00000FA1, 0x0000 }, /* R4001  - ANC Coefficient */
	{ 0x00000FA2, 0x0000 }, /* R4002  - ANC Coefficient */
	{ 0x00000FA3, 0x0000 }, /* R4003  - ANC Coefficient */
	{ 0x00000FA4, 0x0000 }, /* R4004  - ANC Coefficient */
	{ 0x00000FA5, 0x0000 }, /* R4005  - ANC Coefficient */
	{ 0x00000FA6, 0x0000 }, /* R4006  - ANC Coefficient */
	{ 0x00000FA7, 0x0000 }, /* R4007  - ANC Coefficient */
	{ 0x00000FA8, 0x0000 }, /* R4008  - ANC Coefficient */
	{ 0x00000FA9, 0x0000 }, /* R4009  - ANC Coefficient */
	{ 0x00000FAA, 0x0000 }, /* R4010  - ANC Coefficient */
	{ 0x00000FAB, 0x0000 }, /* R4011  - ANC Coefficient */
	{ 0x00000FAC, 0x0000 }, /* R4012  - ANC Coefficient */
	{ 0x00000FAD, 0x0000 }, /* R4013  - ANC Coefficient */
	{ 0x00000FAE, 0x0000 }, /* R4014  - ANC Coefficient */
	{ 0x00000FAF, 0x0000 }, /* R4015  - ANC Coefficient */
	{ 0x00000FB0, 0x0000 }, /* R4016  - ANC Coefficient */
	{ 0x00000FB1, 0x0000 }, /* R4017  - ANC Coefficient */
	{ 0x00000FB2, 0x0000 }, /* R4018  - ANC Coefficient */
	{ 0x00000FB3, 0x0000 }, /* R4019  - ANC Coefficient */
	{ 0x00000FB4, 0x0000 }, /* R4020  - ANC Coefficient */
	{ 0x00000FB5, 0x0000 }, /* R4021  - ANC Coefficient */
	{ 0x00000FB6, 0x0000 }, /* R4022  - ANC Coefficient */
	{ 0x00000FB7, 0x0000 }, /* R4023  - ANC Coefficient */
	{ 0x00000FB8, 0x0000 }, /* R4024  - ANC Coefficient */
	{ 0x00000FB9, 0x0000 }, /* R4025  - ANC Coefficient */
	{ 0x00000FBA, 0x0000 }, /* R4026  - ANC Coefficient */
	{ 0x00000FBB, 0x0000 }, /* R4027  - ANC Coefficient */
	{ 0x00000FBC, 0x0000 }, /* R4028  - ANC Coefficient */
	{ 0x00000FBD, 0x0000 }, /* R4029  - ANC Coefficient */
	{ 0x00000FBE, 0x0000 }, /* R4030  - ANC Coefficient */
	{ 0x00000FBF, 0x0000 }, /* R4031  - ANC Coefficient */
	{ 0x00000FC0, 0x0000 }, /* R4032  - ANC Coefficient */
	{ 0x00000FC1, 0x0000 }, /* R4033  - ANC Coefficient */
	{ 0x00000FC2, 0x0000 }, /* R4034  - ANC Coefficient */
	{ 0x00000FC3, 0x0000 }, /* R4035  - ANC Coefficient */
	{ 0x00000FC4, 0x0000 }, /* R4036  - ANC Coefficient */
	{ 0x00000FC5, 0x0000 }, /* R4037  - ANC Coefficient */
	{ 0x00001200, 0x0000 },
	{ 0x00001204, 0x0000 },
	{ 0x00001206, 0x0000 },
	{ 0x00001210, 0x0000 },
	{ 0x00001212, 0x0000 },
	{ 0x00001214, 0x0000 },
	{ 0x00001216, 0x0000 },
	{ 0x00001300, 0x0000 }, /* R4864  - DAC Comp 1 */
	{ 0x00001302, 0x0000 }, /* R4866  - DAC Comp 2 */
	{ 0x00001340, 0x0000 },
	{ 0x00001341, 0x0000 },
	{ 0x00001342, 0x0000 },
	{ 0x00001343, 0x0000 },
	{ 0x00001380, 0x0000 },
	{ 0x00001381, 0x0000 },
	{ 0x00001382, 0x0000 },
	{ 0x00001383, 0x0000 },
	{ 0x00001390, 0x0000 },
	{ 0x00001391, 0x0000 },
	{ 0x00001392, 0x0000 },
	{ 0x00001393, 0x0000 },
	{ 0x000013a0, 0x0000 },
	{ 0x000013a1, 0x0000 },
	{ 0x000013a2, 0x0000 },
	{ 0x000013a3, 0x0000 },
	{ 0x000013b0, 0x0000 },
	{ 0x000013b1, 0x0000 },
	{ 0x000013b2, 0x0000 },
	{ 0x000013b3, 0x0000 },
	{ 0x000013c0, 0x0000 },
	{ 0x000013c1, 0x0000 },
	{ 0x000013c2, 0x0000 },
	{ 0x000013c3, 0x0000 },
	{ 0x000013d0, 0x0000 },
	{ 0x000013d1, 0x0000 },
	{ 0x000013d2, 0x0000 },
	{ 0x000013d3, 0x0000 },
	{ 0x000013e0, 0x0000 },
	{ 0x000013e1, 0x0000 },
	{ 0x000013e2, 0x0000 },
	{ 0x000013e3, 0x0000 },
	{ 0x000013f0, 0x0000 },
	{ 0x000013f1, 0x0000 },
	{ 0x000013f2, 0x0000 },
	{ 0x000013f3, 0x0000 },
	{ 0x00001400, 0x0000 },
	{ 0x00001401, 0x0000 },
	{ 0x00001402, 0x0000 },
	{ 0x00001403, 0x0000 },
	{ 0x00001410, 0x0000 },
	{ 0x00001411, 0x0000 },
	{ 0x00001412, 0x0000 },
	{ 0x00001413, 0x0000 },
	{ 0x00001420, 0x0000 },
	{ 0x00001421, 0x0000 },
	{ 0x00001422, 0x0000 },
	{ 0x00001423, 0x0000 },
	{ 0x00001430, 0x0000 },
	{ 0x00001431, 0x0000 },
	{ 0x00001432, 0x0000 },
	{ 0x00001433, 0x0000 },
	{ 0x00001700, 0x2001 }, /* R5888  - GPIO1 Control 1 */
	{ 0x00001701, 0xE000 }, /* R5889  - GPIO1 Control 2 */
	{ 0x00001702, 0x2001 }, /* R5890  - GPIO2 Control 1 */
	{ 0x00001703, 0xE000 }, /* R5891  - GPIO2 Control 2 */
	{ 0x00001704, 0x2001 }, /* R5892  - GPIO3 Control 1 */
	{ 0x00001705, 0xE000 }, /* R5893  - GPIO3 Control 2 */
	{ 0x00001706, 0x2001 }, /* R5894  - GPIO4 Control 1 */
	{ 0x00001707, 0xE000 }, /* R5895  - GPIO4 Control 2 */
	{ 0x00001708, 0x2001 }, /* R5896  - GPIO5 Control 1 */
	{ 0x00001709, 0xE000 }, /* R5897  - GPIO5 Control 2 */
	{ 0x0000170A, 0x2001 }, /* R5898  - GPIO6 Control 1 */
	{ 0x0000170B, 0xE000 }, /* R5899  - GPIO6 Control 2 */
	{ 0x0000170C, 0x2001 }, /* R5900  - GPIO7 Control 1 */
	{ 0x0000170D, 0xE000 }, /* R5901  - GPIO7 Control 2 */
	{ 0x0000170E, 0x2001 }, /* R5902  - GPIO8 Control 1 */
	{ 0x0000170F, 0xE000 }, /* R5903  - GPIO8 Control 2 */
	{ 0x00001710, 0x2001 }, /* R5904  - GPIO9 Control 1 */
	{ 0x00001711, 0xE000 }, /* R5905  - GPIO9 Control 2 */
	{ 0x00001712, 0x2001 }, /* R5906  - GPIO10 Control 1 */
	{ 0x00001713, 0xE000 }, /* R5907  - GPIO10 Control 2 */
	{ 0x00001714, 0x2001 }, /* R5908  - GPIO11 Control 1 */
	{ 0x00001715, 0xE000 }, /* R5909  - GPIO11 Control 2 */
	{ 0x00001716, 0x2001 }, /* R5910  - GPIO12 Control 1 */
	{ 0x00001717, 0xE000 }, /* R5911  - GPIO12 Control 2 */
	{ 0x00001718, 0x2001 }, /* R5912  - GPIO13 Control 1 */
	{ 0x00001719, 0xE000 }, /* R5913  - GPIO13 Control 2 */
	{ 0x0000171A, 0x2001 }, /* R5914  - GPIO14 Control 1 */
	{ 0x0000171B, 0xE000 }, /* R5915  - GPIO14 Control 2 */
	{ 0x0000171C, 0x2001 }, /* R5916  - GPIO15 Control 1 */
	{ 0x0000171D, 0xE000 }, /* R5917  - GPIO15 Control 2 */
	{ 0x0000171E, 0x2001 }, /* R5918  - GPIO16 Control 1 */
	{ 0x0000171F, 0xE000 }, /* R5919  - GPIO16 Control 2 */
	{ 0x00001720, 0x2001 }, /* R5920  - GPIO17 Control 1 */
	{ 0x00001721, 0xE000 }, /* R5921  - GPIO17 Control 2 */
	{ 0x00001722, 0x2001 }, /* R5922  - GPIO18 Control 1 */
	{ 0x00001723, 0xE000 }, /* R5923  - GPIO18 Control 2 */
	{ 0x00001724, 0x2001 }, /* R5924  - GPIO19 Control 1 */
	{ 0x00001725, 0xE000 }, /* R5925  - GPIO19 Control 2 */
	{ 0x00001726, 0x2001 }, /* R5926  - GPIO20 Control 1 */
	{ 0x00001727, 0xE000 }, /* R5927  - GPIO20 Control 2 */
	{ 0x00001728, 0x2001 }, /* R5928  - GPIO21 Control 1 */
	{ 0x00001729, 0xE000 }, /* R5929  - GPIO21 Control 2 */
	{ 0x0000172A, 0x2001 }, /* R5930  - GPIO22 Control 1 */
	{ 0x0000172B, 0xE000 }, /* R5931  - GPIO22 Control 2 */
	{ 0x0000172C, 0x2001 }, /* R5932  - GPIO23 Control 1 */
	{ 0x0000172D, 0xE000 }, /* R5933  - GPIO23 Control 2 */
	{ 0x0000172E, 0x2001 }, /* R5934  - GPIO24 Control 1 */
	{ 0x0000172F, 0xE000 }, /* R5935  - GPIO24 Control 2 */
	{ 0x00001730, 0x2001 }, /* R5936  - GPIO25 Control 1 */
	{ 0x00001731, 0xE000 }, /* R5937  - GPIO25 Control 2 */
	{ 0x00001732, 0x2001 }, /* R5938  - GPIO26 Control 1 */
	{ 0x00001733, 0xE000 }, /* R5939  - GPIO26 Control 2 */
	{ 0x00001734, 0x2001 }, /* R5940  - GPIO27 Control 1 */
	{ 0x00001735, 0xE000 }, /* R5941  - GPIO27 Control 2 */
	{ 0x00001736, 0x2001 }, /* R5942  - GPIO28 Control 1 */
	{ 0x00001737, 0xE000 }, /* R5943  - GPIO28 Control 2 */
	{ 0x00001738, 0x2001 }, /* R5944  - GPIO29 Control 1 */
	{ 0x00001739, 0xE000 }, /* R5945  - GPIO29 Control 2 */
	{ 0x0000173A, 0x2001 }, /* R5946  - GPIO30 Control 1 */
	{ 0x0000173B, 0xE000 }, /* R5947  - GPIO30 Control 2 */
	{ 0x0000173C, 0x2001 }, /* R5948  - GPIO31 Control 1 */
	{ 0x0000173D, 0xE000 }, /* R5949  - GPIO31 Control 2 */
	{ 0x0000173E, 0x2001 }, /* R5950  - GPIO32 Control 1 */
	{ 0x0000173F, 0xE000 }, /* R5951  - GPIO32 Control 2 */
	{ 0x00001740, 0x2001 }, /* R5952  - GPIO33 Control 1 */
	{ 0x00001741, 0xE000 }, /* R5953  - GPIO33 Control 2 */
	{ 0x00001742, 0x2001 }, /* R5954  - GPIO34 Control 1 */
	{ 0x00001743, 0xE000 }, /* R5955  - GPIO34 Control 2 */
	{ 0x00001744, 0x2001 }, /* R5956  - GPIO35 Control 1 */
	{ 0x00001745, 0xE000 }, /* R5957  - GPIO35 Control 2 */
	{ 0x00001746, 0x2001 }, /* R5958  - GPIO36 Control 1 */
	{ 0x00001747, 0xE000 }, /* R5959  - GPIO36 Control 2 */
	{ 0x00001748, 0x2001 }, /* R5960  - GPIO37 Control 1 */
	{ 0x00001749, 0xE000 }, /* R5961  - GPIO37 Control 2 */
	{ 0x0000174A, 0x2001 }, /* R5962  - GPIO38 Control 1 */
	{ 0x0000174B, 0xE000 }, /* R5963  - GPIO38 Control 2 */
	{ 0x0000174C, 0x2001 }, /* R5964  - GPIO39 Control 1 */
	{ 0x0000174D, 0xE000 }, /* R5965  - GPIO39 Control 2 */
	{ 0x0000174E, 0x2001 }, /* R5966  - GPIO40 Control 1 */
	{ 0x0000174F, 0xE000 }, /* R5967  - GPIO40 Control 2 */
	{ 0x00001802, 0x0000 },
	{ 0x00001803, 0x0000 },
	{ 0x00001804, 0x0000 },
	{ 0x00001807, 0x0000 },
	{ 0x00001809, 0x0000 },
	{ 0x0000180F, 0x0000 },
	{ 0x00001813, 0x0000 },
	{ 0x00001819, 0x0000 },
	{ 0x0000181C, 0x0000 },
	{ 0x00001840, 0xFFFF }, /* R6208  - IRQ1 Mask 1 */
	{ 0x00001841, 0xFFFF }, /* R6209  - IRQ1 Mask 2 */
	{ 0x00001842, 0xFFFF }, /* R6210  - IRQ1 Mask 3 */
	{ 0x00001843, 0xFFFF }, /* R6211  - IRQ1 Mask 4 */
	{ 0x00001844, 0xFFFF }, /* R6212  - IRQ1 Mask 5 */
	{ 0x00001845, 0xFFFF }, /* R6213  - IRQ1 Mask 6 */
	{ 0x00001846, 0xFFFF }, /* R6214  - IRQ1 Mask 7 */
	{ 0x00001847, 0xFFFF }, /* R6215  - IRQ1 Mask 8 */
	{ 0x00001848, 0xFFFF }, /* R6216  - IRQ1 Mask 9 */
	{ 0x00001849, 0xFFFF }, /* R6217  - IRQ1 Mask 10 */
	{ 0x0000184A, 0xFFFF }, /* R6218  - IRQ1 Mask 11 */
	{ 0x0000184B, 0xFFFF }, /* R6219  - IRQ1 Mask 12 */
	{ 0x0000184C, 0xFFFF }, /* R6220  - IRQ1 Mask 13 */
	{ 0x0000184D, 0xFFFF }, /* R6221  - IRQ1 Mask 14 */
	{ 0x0000184E, 0xFFFF }, /* R6222  - IRQ1 Mask 15 */
	{ 0x00001948, 0xFFFF }, /* R6472  - IRQ2 Mask 9 */
	{ 0x00001A80, 0x4400 }, /* R6784  - IRQ1 CTRL */
};

static bool clearwater_is_adsp_memory(struct device *dev, unsigned int reg)
{
	if ((reg >= 0x080000 && reg <= 0x085ffe) ||
	    (reg >= 0x0a0000 && reg <= 0x0a7ffe) ||
	    (reg >= 0x0c0000 && reg <= 0x0c1ffe) ||
	    (reg >= 0x0e0000 && reg <= 0x0e1ffe) ||
	    (reg >= 0x100000 && reg <= 0x10effe) ||
	    (reg >= 0x120000 && reg <= 0x12bffe) ||
	    (reg >= 0x136000 && reg <= 0x137ffe) ||
	    (reg >= 0x140000 && reg <= 0x14bffe) ||
	    (reg >= 0x160000 && reg <= 0x161ffe) ||
	    (reg >= 0x180000 && reg <= 0x18effe) ||
	    (reg >= 0x1a0000 && reg <= 0x1b1ffe) ||
	    (reg >= 0x1b6000 && reg <= 0x1b7ffe) ||
	    (reg >= 0x1c0000 && reg <= 0x1cbffe) ||
	    (reg >= 0x1e0000 && reg <= 0x1e1ffe) ||
	    (reg >= 0x200000 && reg <= 0x208ffe) ||
	    (reg >= 0x220000 && reg <= 0x231ffe) ||
	    (reg >= 0x240000 && reg <= 0x24bffe) ||
	    (reg >= 0x260000 && reg <= 0x261ffe) ||
	    (reg >= 0x280000 && reg <= 0x288ffe) ||
	    (reg >= 0x2a0000 && reg <= 0x2a9ffe) ||
	    (reg >= 0x2c0000 && reg <= 0x2c3ffe) ||
	    (reg >= 0x2e0000 && reg <= 0x2e1ffe) ||
	    (reg >= 0x300000 && reg <= 0x305ffe) ||
	    (reg >= 0x320000 && reg <= 0x333ffe) ||
	    (reg >= 0x340000 && reg <= 0x34bffe) ||
	    (reg >= 0x360000 && reg <= 0x361ffe) ||
	    (reg >= 0x380000 && reg <= 0x388ffe) ||
	    (reg >= 0x3a0000 && reg <= 0x3a7ffe) ||
	    (reg >= 0x3c0000 && reg <= 0x3c1ffe) ||
	    (reg >= 0x3e0000 && reg <= 0x3e1ffe))
		return true;
	else
		return false;
}

static bool clearwater_16bit_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x2C2:
	case ARIZONA_SOFTWARE_RESET:
	case ARIZONA_DEVICE_REVISION:
	case ARIZONA_CTRL_IF_SPI_CFG_1:
	case ARIZONA_CTRL_IF_I2C1_CFG_1:
	case ARIZONA_CTRL_IF_I2C2_CFG_1:
	case ARIZONA_CTRL_IF_I2C1_CFG_2:
	case ARIZONA_CTRL_IF_I2C2_CFG_2:
	case ARIZONA_WRITE_SEQUENCER_CTRL_0:
	case ARIZONA_WRITE_SEQUENCER_CTRL_1:
	case ARIZONA_WRITE_SEQUENCER_CTRL_2:
	case ARIZONA_TONE_GENERATOR_1:
	case ARIZONA_TONE_GENERATOR_2:
	case ARIZONA_TONE_GENERATOR_3:
	case ARIZONA_TONE_GENERATOR_4:
	case ARIZONA_TONE_GENERATOR_5:
	case ARIZONA_PWM_DRIVE_1:
	case ARIZONA_PWM_DRIVE_2:
	case ARIZONA_PWM_DRIVE_3:
	case ARIZONA_WAKE_CONTROL:
	case ARIZONA_SEQUENCE_CONTROL:
	case ARIZONA_SAMPLE_RATE_SEQUENCE_SELECT_1:
	case ARIZONA_SAMPLE_RATE_SEQUENCE_SELECT_2:
	case ARIZONA_SAMPLE_RATE_SEQUENCE_SELECT_3:
	case ARIZONA_SAMPLE_RATE_SEQUENCE_SELECT_4:
	case ARIZONA_ALWAYS_ON_TRIGGERS_SEQUENCE_SELECT_1:
	case ARIZONA_ALWAYS_ON_TRIGGERS_SEQUENCE_SELECT_2:
	case ARIZONA_ALWAYS_ON_TRIGGERS_SEQUENCE_SELECT_3:
	case ARIZONA_ALWAYS_ON_TRIGGERS_SEQUENCE_SELECT_4:
	case ARIZONA_ALWAYS_ON_TRIGGERS_SEQUENCE_SELECT_5:
	case ARIZONA_ALWAYS_ON_TRIGGERS_SEQUENCE_SELECT_6:
	case ARIZONA_HAPTICS_CONTROL_1:
	case ARIZONA_HAPTICS_CONTROL_2:
	case ARIZONA_HAPTICS_PHASE_1_INTENSITY:
	case ARIZONA_HAPTICS_PHASE_1_DURATION:
	case ARIZONA_HAPTICS_PHASE_2_INTENSITY:
	case ARIZONA_HAPTICS_PHASE_2_DURATION:
	case ARIZONA_HAPTICS_PHASE_3_INTENSITY:
	case ARIZONA_HAPTICS_PHASE_3_DURATION:
	case ARIZONA_HAPTICS_STATUS:
	case CLEARWATER_COMFORT_NOISE_GENERATOR:
	case ARIZONA_CLOCK_32K_1:
	case ARIZONA_SYSTEM_CLOCK_1:
	case ARIZONA_SAMPLE_RATE_1:
	case ARIZONA_SAMPLE_RATE_2:
	case ARIZONA_SAMPLE_RATE_3:
	case ARIZONA_SAMPLE_RATE_1_STATUS:
	case ARIZONA_SAMPLE_RATE_2_STATUS:
	case ARIZONA_SAMPLE_RATE_3_STATUS:
	case ARIZONA_ASYNC_CLOCK_1:
	case ARIZONA_ASYNC_SAMPLE_RATE_1:
	case ARIZONA_ASYNC_SAMPLE_RATE_1_STATUS:
	case ARIZONA_ASYNC_SAMPLE_RATE_2:
	case ARIZONA_ASYNC_SAMPLE_RATE_2_STATUS:
	case CLEARWATER_DSP_CLOCK_1:
	case CLEARWATER_DSP_CLOCK_2:
	case ARIZONA_OUTPUT_SYSTEM_CLOCK:
	case ARIZONA_OUTPUT_ASYNC_CLOCK:
	case ARIZONA_RATE_ESTIMATOR_1:
	case ARIZONA_RATE_ESTIMATOR_2:
	case ARIZONA_RATE_ESTIMATOR_3:
	case ARIZONA_RATE_ESTIMATOR_4:
	case ARIZONA_RATE_ESTIMATOR_5:
	case ARIZONA_FLL1_CONTROL_1:
	case ARIZONA_FLL1_CONTROL_2:
	case ARIZONA_FLL1_CONTROL_3:
	case ARIZONA_FLL1_CONTROL_4:
	case ARIZONA_FLL1_CONTROL_5:
	case ARIZONA_FLL1_CONTROL_6:
	case ARIZONA_FLL1_CONTROL_7:
	case ARIZONA_FLL1_LOOP_FILTER_TEST_1:
	case ARIZONA_FLL1_NCO_TEST_0:
	case ARIZONA_FLL1_SYNCHRONISER_1:
	case ARIZONA_FLL1_SYNCHRONISER_2:
	case ARIZONA_FLL1_SYNCHRONISER_3:
	case ARIZONA_FLL1_SYNCHRONISER_4:
	case ARIZONA_FLL1_SYNCHRONISER_5:
	case ARIZONA_FLL1_SYNCHRONISER_6:
	case ARIZONA_FLL1_SYNCHRONISER_7:
	case ARIZONA_FLL1_SPREAD_SPECTRUM:
	case ARIZONA_FLL1_GPIO_CLOCK:
	case ARIZONA_FLL2_CONTROL_1:
	case ARIZONA_FLL2_CONTROL_2:
	case ARIZONA_FLL2_CONTROL_3:
	case ARIZONA_FLL2_CONTROL_4:
	case ARIZONA_FLL2_CONTROL_5:
	case ARIZONA_FLL2_CONTROL_6:
	case ARIZONA_FLL2_CONTROL_7:
	case ARIZONA_FLL2_LOOP_FILTER_TEST_1:
	case ARIZONA_FLL2_NCO_TEST_0:
	case ARIZONA_FLL2_SYNCHRONISER_1:
	case ARIZONA_FLL2_SYNCHRONISER_2:
	case ARIZONA_FLL2_SYNCHRONISER_3:
	case ARIZONA_FLL2_SYNCHRONISER_4:
	case ARIZONA_FLL2_SYNCHRONISER_5:
	case ARIZONA_FLL2_SYNCHRONISER_6:
	case ARIZONA_FLL2_SYNCHRONISER_7:
	case ARIZONA_FLL2_SPREAD_SPECTRUM:
	case ARIZONA_FLL2_GPIO_CLOCK:
	case ARIZONA_FLL3_CONTROL_1:
	case ARIZONA_FLL3_CONTROL_2:
	case ARIZONA_FLL3_CONTROL_3:
	case ARIZONA_FLL3_CONTROL_4:
	case ARIZONA_FLL3_CONTROL_5:
	case ARIZONA_FLL3_CONTROL_6:
	case ARIZONA_FLL3_CONTROL_7:
	case ARIZONA_FLL3_LOOP_FILTER_TEST_1:
	case ARIZONA_FLL3_NCO_TEST_0:
	case ARIZONA_FLL3_SYNCHRONISER_1:
	case ARIZONA_FLL3_SYNCHRONISER_2:
	case ARIZONA_FLL3_SYNCHRONISER_3:
	case ARIZONA_FLL3_SYNCHRONISER_4:
	case ARIZONA_FLL3_SYNCHRONISER_5:
	case ARIZONA_FLL3_SYNCHRONISER_6:
	case ARIZONA_FLL3_SYNCHRONISER_7:
	case ARIZONA_FLL3_SPREAD_SPECTRUM:
	case ARIZONA_FLL3_GPIO_CLOCK:
	case ARIZONA_MIC_CHARGE_PUMP_1:
	case CLEARWATER_CP_MODE:
	case ARIZONA_LDO1_CONTROL_1:
	case ARIZONA_LDO2_CONTROL_1:
	case ARIZONA_MIC_BIAS_CTRL_1:
	case ARIZONA_MIC_BIAS_CTRL_2:
	case ARIZONA_MIC_BIAS_CTRL_3:
	case ARIZONA_MIC_BIAS_CTRL_4:
	case ARIZONA_HP_CTRL_1L:
	case ARIZONA_HP_CTRL_1R:
	case ARIZONA_HP_CTRL_2L:
	case ARIZONA_HP_CTRL_2R:
	case ARIZONA_HP_CTRL_3L:
	case ARIZONA_HP_CTRL_3R:
	case ARIZONA_DCS_HP1L_CONTROL:
	case ARIZONA_DCS_HP1R_CONTROL:
	case CLEARWATER_EDRE_HP_STEREO_CONTROL:
	case ARIZONA_ACCESSORY_DETECT_MODE_1:
	case ARIZONA_HEADPHONE_DETECT_1:
	case ARIZONA_HEADPHONE_DETECT_2:
	case ARIZONA_HEADPHONE_DETECT_3:
	case ARIZONA_HP_DACVAL:
	case CLEARWATER_MICD_CLAMP_CONTROL:
	case ARIZONA_MIC_DETECT_1:
	case ARIZONA_MIC_DETECT_2:
	case ARIZONA_MIC_DETECT_3:
	case ARIZONA_MIC_DETECT_4:
	case ARIZONA_MIC_DETECT_LEVEL_1:
	case ARIZONA_MIC_DETECT_LEVEL_2:
	case ARIZONA_MIC_DETECT_LEVEL_3:
	case ARIZONA_MIC_DETECT_LEVEL_4:
	case ARIZONA_MIC_NOISE_MIX_CONTROL_1:
	case CLEARWATER_GP_SWITCH_1:
	case ARIZONA_JACK_DETECT_ANALOGUE:
	case ARIZONA_INPUT_ENABLES:
	case ARIZONA_INPUT_ENABLES_STATUS:
	case ARIZONA_INPUT_RATE:
	case ARIZONA_INPUT_VOLUME_RAMP:
	case ARIZONA_HPF_CONTROL:
	case ARIZONA_IN1L_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_1L:
	case ARIZONA_DMIC1L_CONTROL:
	case ARIZONA_IN1R_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_1R:
	case ARIZONA_DMIC1R_CONTROL:
	case ARIZONA_IN2L_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_2L:
	case ARIZONA_DMIC2L_CONTROL:
	case ARIZONA_IN2R_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_2R:
	case ARIZONA_DMIC2R_CONTROL:
	case ARIZONA_IN3L_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_3L:
	case ARIZONA_DMIC3L_CONTROL:
	case ARIZONA_IN3R_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_3R:
	case ARIZONA_DMIC3R_CONTROL:
	case ARIZONA_IN4L_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_4L:
	case ARIZONA_DMIC4L_CONTROL:
	case ARIZONA_IN4R_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_4R:
	case ARIZONA_DMIC4R_CONTROL:
	case ARIZONA_IN5L_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_5L:
	case ARIZONA_DMIC5L_CONTROL:
	case ARIZONA_IN5R_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_5R:
	case ARIZONA_DMIC5R_CONTROL:
	case ARIZONA_IN6L_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_6L:
	case ARIZONA_DMIC6L_CONTROL:
	case ARIZONA_IN6R_CONTROL:
	case ARIZONA_ADC_DIGITAL_VOLUME_6R:
	case ARIZONA_DMIC6R_CONTROL:
	case ARIZONA_OUTPUT_ENABLES_1:
	case ARIZONA_OUTPUT_STATUS_1:
	case ARIZONA_OUTPUT_STANDBY_1:
	case ARIZONA_RAW_OUTPUT_STATUS_1:
	case ARIZONA_OUTPUT_RATE_1:
	case ARIZONA_OUTPUT_VOLUME_RAMP:
	case ARIZONA_OUTPUT_PATH_CONFIG_1L:
	case ARIZONA_DAC_DIGITAL_VOLUME_1L:
	case ARIZONA_DAC_VOLUME_LIMIT_1L:
	case ARIZONA_NOISE_GATE_SELECT_1L:
	case ARIZONA_OUTPUT_PATH_CONFIG_1R:
	case ARIZONA_DAC_DIGITAL_VOLUME_1R:
	case ARIZONA_DAC_VOLUME_LIMIT_1R:
	case ARIZONA_NOISE_GATE_SELECT_1R:
	case ARIZONA_OUTPUT_PATH_CONFIG_2L:
	case ARIZONA_DAC_DIGITAL_VOLUME_2L:
	case ARIZONA_DAC_VOLUME_LIMIT_2L:
	case ARIZONA_NOISE_GATE_SELECT_2L:
	case ARIZONA_OUTPUT_PATH_CONFIG_2R:
	case ARIZONA_DAC_DIGITAL_VOLUME_2R:
	case ARIZONA_DAC_VOLUME_LIMIT_2R:
	case ARIZONA_NOISE_GATE_SELECT_2R:
	case ARIZONA_OUTPUT_PATH_CONFIG_3L:
	case ARIZONA_DAC_DIGITAL_VOLUME_3L:
	case ARIZONA_DAC_VOLUME_LIMIT_3L:
	case ARIZONA_NOISE_GATE_SELECT_3L:
	case ARIZONA_OUTPUT_PATH_CONFIG_3R:
	case ARIZONA_DAC_DIGITAL_VOLUME_3R:
	case ARIZONA_DAC_VOLUME_LIMIT_3R:
	case ARIZONA_NOISE_GATE_SELECT_3R:
	case ARIZONA_OUTPUT_PATH_CONFIG_4L:
	case ARIZONA_DAC_DIGITAL_VOLUME_4L:
	case ARIZONA_OUT_VOLUME_4L:
	case ARIZONA_NOISE_GATE_SELECT_4L:
	case ARIZONA_OUTPUT_PATH_CONFIG_4R:
	case ARIZONA_DAC_DIGITAL_VOLUME_4R:
	case ARIZONA_OUT_VOLUME_4R:
	case ARIZONA_NOISE_GATE_SELECT_4R:
	case ARIZONA_OUTPUT_PATH_CONFIG_5L:
	case ARIZONA_DAC_DIGITAL_VOLUME_5L:
	case ARIZONA_DAC_VOLUME_LIMIT_5L:
	case ARIZONA_NOISE_GATE_SELECT_5L:
	case ARIZONA_OUTPUT_PATH_CONFIG_5R:
	case ARIZONA_DAC_DIGITAL_VOLUME_5R:
	case ARIZONA_DAC_VOLUME_LIMIT_5R:
	case ARIZONA_NOISE_GATE_SELECT_5R:
	case ARIZONA_OUTPUT_PATH_CONFIG_6L:
	case ARIZONA_DAC_DIGITAL_VOLUME_6L:
	case ARIZONA_DAC_VOLUME_LIMIT_6L:
	case ARIZONA_NOISE_GATE_SELECT_6L:
	case ARIZONA_OUTPUT_PATH_CONFIG_6R:
	case ARIZONA_DAC_DIGITAL_VOLUME_6R:
	case ARIZONA_DAC_VOLUME_LIMIT_6R:
	case ARIZONA_NOISE_GATE_SELECT_6R:
	case ARIZONA_DRE_ENABLE:
	case CLEARWATER_EDRE_ENABLE:
	case ARIZONA_DAC_AEC_CONTROL_1:
	case ARIZONA_NOISE_GATE_CONTROL:
	case ARIZONA_PDM_SPK1_CTRL_1:
	case ARIZONA_PDM_SPK1_CTRL_2:
	case ARIZONA_PDM_SPK2_CTRL_1:
	case ARIZONA_PDM_SPK2_CTRL_2:
	case ARIZONA_HP1_SHORT_CIRCUIT_CTRL:
	case ARIZONA_HP2_SHORT_CIRCUIT_CTRL:
	case ARIZONA_HP3_SHORT_CIRCUIT_CTRL:
	case ARIZONA_HP_TEST_CTRL_5:
	case ARIZONA_HP_TEST_CTRL_6:
	case ARIZONA_SPK_CTRL_3:
	case ARIZONA_AIF1_BCLK_CTRL:
	case ARIZONA_AIF1_TX_PIN_CTRL:
	case ARIZONA_AIF1_RX_PIN_CTRL:
	case ARIZONA_AIF1_RATE_CTRL:
	case ARIZONA_AIF1_FORMAT:
	case ARIZONA_AIF1_TX_BCLK_RATE:
	case ARIZONA_AIF1_RX_BCLK_RATE:
	case ARIZONA_AIF1_FRAME_CTRL_1:
	case ARIZONA_AIF1_FRAME_CTRL_2:
	case ARIZONA_AIF1_FRAME_CTRL_3:
	case ARIZONA_AIF1_FRAME_CTRL_4:
	case ARIZONA_AIF1_FRAME_CTRL_5:
	case ARIZONA_AIF1_FRAME_CTRL_6:
	case ARIZONA_AIF1_FRAME_CTRL_7:
	case ARIZONA_AIF1_FRAME_CTRL_8:
	case ARIZONA_AIF1_FRAME_CTRL_9:
	case ARIZONA_AIF1_FRAME_CTRL_10:
	case ARIZONA_AIF1_FRAME_CTRL_11:
	case ARIZONA_AIF1_FRAME_CTRL_12:
	case ARIZONA_AIF1_FRAME_CTRL_13:
	case ARIZONA_AIF1_FRAME_CTRL_14:
	case ARIZONA_AIF1_FRAME_CTRL_15:
	case ARIZONA_AIF1_FRAME_CTRL_16:
	case ARIZONA_AIF1_FRAME_CTRL_17:
	case ARIZONA_AIF1_FRAME_CTRL_18:
	case ARIZONA_AIF1_TX_ENABLES:
	case ARIZONA_AIF1_RX_ENABLES:
	case ARIZONA_AIF2_BCLK_CTRL:
	case ARIZONA_AIF2_TX_PIN_CTRL:
	case ARIZONA_AIF2_RX_PIN_CTRL:
	case ARIZONA_AIF2_RATE_CTRL:
	case ARIZONA_AIF2_FORMAT:
	case ARIZONA_AIF2_TX_BCLK_RATE:
	case ARIZONA_AIF2_RX_BCLK_RATE:
	case ARIZONA_AIF2_FRAME_CTRL_1:
	case ARIZONA_AIF2_FRAME_CTRL_2:
	case ARIZONA_AIF2_FRAME_CTRL_3:
	case ARIZONA_AIF2_FRAME_CTRL_4:
	case ARIZONA_AIF2_FRAME_CTRL_5:
	case ARIZONA_AIF2_FRAME_CTRL_6:
	case ARIZONA_AIF2_FRAME_CTRL_7:
	case ARIZONA_AIF2_FRAME_CTRL_8:
	case ARIZONA_AIF2_FRAME_CTRL_9:
	case ARIZONA_AIF2_FRAME_CTRL_10:
	case ARIZONA_AIF2_FRAME_CTRL_11:
	case ARIZONA_AIF2_FRAME_CTRL_12:
	case ARIZONA_AIF2_FRAME_CTRL_13:
	case ARIZONA_AIF2_FRAME_CTRL_14:
	case ARIZONA_AIF2_FRAME_CTRL_15:
	case ARIZONA_AIF2_FRAME_CTRL_16:
	case ARIZONA_AIF2_FRAME_CTRL_17:
	case ARIZONA_AIF2_FRAME_CTRL_18:
	case ARIZONA_AIF2_TX_ENABLES:
	case ARIZONA_AIF2_RX_ENABLES:
	case ARIZONA_AIF3_BCLK_CTRL:
	case ARIZONA_AIF3_TX_PIN_CTRL:
	case ARIZONA_AIF3_RX_PIN_CTRL:
	case ARIZONA_AIF3_RATE_CTRL:
	case ARIZONA_AIF3_FORMAT:
	case ARIZONA_AIF3_TX_BCLK_RATE:
	case ARIZONA_AIF3_RX_BCLK_RATE:
	case ARIZONA_AIF3_FRAME_CTRL_1:
	case ARIZONA_AIF3_FRAME_CTRL_2:
	case ARIZONA_AIF3_FRAME_CTRL_3:
	case ARIZONA_AIF3_FRAME_CTRL_4:
	case ARIZONA_AIF3_FRAME_CTRL_11:
	case ARIZONA_AIF3_FRAME_CTRL_12:
	case ARIZONA_AIF3_TX_ENABLES:
	case ARIZONA_AIF3_RX_ENABLES:
	case ARIZONA_AIF4_BCLK_CTRL:
	case ARIZONA_AIF4_TX_PIN_CTRL:
	case ARIZONA_AIF4_RX_PIN_CTRL:
	case ARIZONA_AIF4_RATE_CTRL:
	case ARIZONA_AIF4_FORMAT:
	case ARIZONA_AIF4_TX_BCLK_RATE:
	case ARIZONA_AIF4_RX_BCLK_RATE:
	case ARIZONA_AIF4_FRAME_CTRL_1:
	case ARIZONA_AIF4_FRAME_CTRL_2:
	case ARIZONA_AIF4_FRAME_CTRL_3:
	case ARIZONA_AIF4_FRAME_CTRL_4:
	case ARIZONA_AIF4_FRAME_CTRL_11:
	case ARIZONA_AIF4_FRAME_CTRL_12:
	case ARIZONA_AIF4_TX_ENABLES:
	case ARIZONA_AIF4_RX_ENABLES:
	case ARIZONA_SPD1_TX_CONTROL:
	case ARIZONA_SPD1_TX_CHANNEL_STATUS_1:
	case ARIZONA_SPD1_TX_CHANNEL_STATUS_2:
	case ARIZONA_SPD1_TX_CHANNEL_STATUS_3:
	case ARIZONA_SLIMBUS_FRAMER_REF_GEAR:
	case ARIZONA_SLIMBUS_RATES_1:
	case ARIZONA_SLIMBUS_RATES_2:
	case ARIZONA_SLIMBUS_RATES_3:
	case ARIZONA_SLIMBUS_RATES_4:
	case ARIZONA_SLIMBUS_RATES_5:
	case ARIZONA_SLIMBUS_RATES_6:
	case ARIZONA_SLIMBUS_RATES_7:
	case ARIZONA_SLIMBUS_RATES_8:
	case ARIZONA_SLIMBUS_RX_CHANNEL_ENABLE:
	case ARIZONA_SLIMBUS_TX_CHANNEL_ENABLE:
	case ARIZONA_SLIMBUS_RX_PORT_STATUS:
	case ARIZONA_SLIMBUS_TX_PORT_STATUS:
	case ARIZONA_PWM1MIX_INPUT_1_SOURCE:
	case ARIZONA_PWM1MIX_INPUT_1_VOLUME:
	case ARIZONA_PWM1MIX_INPUT_2_SOURCE:
	case ARIZONA_PWM1MIX_INPUT_2_VOLUME:
	case ARIZONA_PWM1MIX_INPUT_3_SOURCE:
	case ARIZONA_PWM1MIX_INPUT_3_VOLUME:
	case ARIZONA_PWM1MIX_INPUT_4_SOURCE:
	case ARIZONA_PWM1MIX_INPUT_4_VOLUME:
	case ARIZONA_PWM2MIX_INPUT_1_SOURCE:
	case ARIZONA_PWM2MIX_INPUT_1_VOLUME:
	case ARIZONA_PWM2MIX_INPUT_2_SOURCE:
	case ARIZONA_PWM2MIX_INPUT_2_VOLUME:
	case ARIZONA_PWM2MIX_INPUT_3_SOURCE:
	case ARIZONA_PWM2MIX_INPUT_3_VOLUME:
	case ARIZONA_PWM2MIX_INPUT_4_SOURCE:
	case ARIZONA_PWM2MIX_INPUT_4_VOLUME:
	case ARIZONA_OUT1LMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT1LMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT1LMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT1LMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT1LMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT1LMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT1LMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT1LMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT1RMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT1RMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT1RMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT1RMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT1RMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT1RMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT1RMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT1RMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT2LMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT2LMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT2LMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT2LMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT2LMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT2LMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT2LMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT2LMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT2RMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT2RMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT2RMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT2RMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT2RMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT2RMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT2RMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT2RMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT3LMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT3LMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT3LMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT3LMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT3LMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT3LMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT3LMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT3LMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT3RMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT3RMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT3RMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT3RMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT3RMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT3RMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT3RMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT3RMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT4LMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT4LMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT4LMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT4LMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT4LMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT4LMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT4LMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT4LMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT4RMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT4RMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT4RMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT4RMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT4RMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT4RMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT4RMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT4RMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT5LMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT5LMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT5LMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT5LMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT5LMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT5LMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT5LMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT5LMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT5RMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT5RMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT5RMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT5RMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT5RMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT5RMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT5RMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT5RMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT6LMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT6LMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT6LMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT6LMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT6LMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT6LMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT6LMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT6LMIX_INPUT_4_VOLUME:
	case ARIZONA_OUT6RMIX_INPUT_1_SOURCE:
	case ARIZONA_OUT6RMIX_INPUT_1_VOLUME:
	case ARIZONA_OUT6RMIX_INPUT_2_SOURCE:
	case ARIZONA_OUT6RMIX_INPUT_2_VOLUME:
	case ARIZONA_OUT6RMIX_INPUT_3_SOURCE:
	case ARIZONA_OUT6RMIX_INPUT_3_VOLUME:
	case ARIZONA_OUT6RMIX_INPUT_4_SOURCE:
	case ARIZONA_OUT6RMIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX1MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX1MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX1MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX1MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX1MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX1MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX1MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX1MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX2MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX2MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX2MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX2MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX2MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX2MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX2MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX2MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX3MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX3MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX3MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX3MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX3MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX3MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX3MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX3MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX4MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX4MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX4MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX4MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX4MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX4MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX4MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX4MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX5MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX5MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX5MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX5MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX5MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX5MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX5MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX5MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX6MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX6MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX6MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX6MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX6MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX6MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX6MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX6MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX7MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX7MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX7MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX7MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX7MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX7MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX7MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX7MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF1TX8MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF1TX8MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF1TX8MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF1TX8MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF1TX8MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF1TX8MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF1TX8MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF1TX8MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF2TX1MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF2TX1MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF2TX1MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF2TX1MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF2TX1MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF2TX1MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF2TX1MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF2TX1MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF2TX2MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF2TX2MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF2TX2MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF2TX2MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF2TX2MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF2TX2MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF2TX2MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF2TX2MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF2TX3MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF2TX3MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF2TX3MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF2TX3MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF2TX3MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF2TX3MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF2TX3MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF2TX3MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF2TX4MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF2TX4MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF2TX4MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF2TX4MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF2TX4MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF2TX4MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF2TX4MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF2TX4MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF2TX5MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF2TX5MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF2TX5MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF2TX5MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF2TX5MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF2TX5MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF2TX5MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF2TX5MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF2TX6MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF2TX6MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF2TX6MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF2TX6MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF2TX6MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF2TX6MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF2TX6MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF2TX6MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF3TX1MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF3TX1MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF3TX1MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF3TX1MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF3TX1MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF3TX1MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF3TX1MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF3TX1MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF3TX2MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF3TX2MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF3TX2MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF3TX2MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF3TX2MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF3TX2MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF3TX2MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF3TX2MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF4TX1MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF4TX1MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF4TX1MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF4TX1MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF4TX1MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF4TX1MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF4TX1MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF4TX1MIX_INPUT_4_VOLUME:
	case ARIZONA_AIF4TX2MIX_INPUT_1_SOURCE:
	case ARIZONA_AIF4TX2MIX_INPUT_1_VOLUME:
	case ARIZONA_AIF4TX2MIX_INPUT_2_SOURCE:
	case ARIZONA_AIF4TX2MIX_INPUT_2_VOLUME:
	case ARIZONA_AIF4TX2MIX_INPUT_3_SOURCE:
	case ARIZONA_AIF4TX2MIX_INPUT_3_VOLUME:
	case ARIZONA_AIF4TX2MIX_INPUT_4_SOURCE:
	case ARIZONA_AIF4TX2MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX1MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX1MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX1MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX1MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX1MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX1MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX1MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX1MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX2MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX2MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX2MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX2MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX2MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX2MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX2MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX2MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX3MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX3MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX3MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX3MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX3MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX3MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX3MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX3MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX4MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX4MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX4MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX4MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX4MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX4MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX4MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX4MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX5MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX5MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX5MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX5MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX5MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX5MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX5MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX5MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX6MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX6MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX6MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX6MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX6MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX6MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX6MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX6MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX7MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX7MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX7MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX7MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX7MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX7MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX7MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX7MIX_INPUT_4_VOLUME:
	case ARIZONA_SLIMTX8MIX_INPUT_1_SOURCE:
	case ARIZONA_SLIMTX8MIX_INPUT_1_VOLUME:
	case ARIZONA_SLIMTX8MIX_INPUT_2_SOURCE:
	case ARIZONA_SLIMTX8MIX_INPUT_2_VOLUME:
	case ARIZONA_SLIMTX8MIX_INPUT_3_SOURCE:
	case ARIZONA_SLIMTX8MIX_INPUT_3_VOLUME:
	case ARIZONA_SLIMTX8MIX_INPUT_4_SOURCE:
	case ARIZONA_SLIMTX8MIX_INPUT_4_VOLUME:
	case ARIZONA_EQ1MIX_INPUT_1_SOURCE:
	case ARIZONA_EQ1MIX_INPUT_1_VOLUME:
	case ARIZONA_EQ1MIX_INPUT_2_SOURCE:
	case ARIZONA_EQ1MIX_INPUT_2_VOLUME:
	case ARIZONA_EQ1MIX_INPUT_3_SOURCE:
	case ARIZONA_EQ1MIX_INPUT_3_VOLUME:
	case ARIZONA_EQ1MIX_INPUT_4_SOURCE:
	case ARIZONA_EQ1MIX_INPUT_4_VOLUME:
	case ARIZONA_EQ2MIX_INPUT_1_SOURCE:
	case ARIZONA_EQ2MIX_INPUT_1_VOLUME:
	case ARIZONA_EQ2MIX_INPUT_2_SOURCE:
	case ARIZONA_EQ2MIX_INPUT_2_VOLUME:
	case ARIZONA_EQ2MIX_INPUT_3_SOURCE:
	case ARIZONA_EQ2MIX_INPUT_3_VOLUME:
	case ARIZONA_EQ2MIX_INPUT_4_SOURCE:
	case ARIZONA_EQ2MIX_INPUT_4_VOLUME:
	case ARIZONA_EQ3MIX_INPUT_1_SOURCE:
	case ARIZONA_EQ3MIX_INPUT_1_VOLUME:
	case ARIZONA_EQ3MIX_INPUT_2_SOURCE:
	case ARIZONA_EQ3MIX_INPUT_2_VOLUME:
	case ARIZONA_EQ3MIX_INPUT_3_SOURCE:
	case ARIZONA_EQ3MIX_INPUT_3_VOLUME:
	case ARIZONA_EQ3MIX_INPUT_4_SOURCE:
	case ARIZONA_EQ3MIX_INPUT_4_VOLUME:
	case ARIZONA_EQ4MIX_INPUT_1_SOURCE:
	case ARIZONA_EQ4MIX_INPUT_1_VOLUME:
	case ARIZONA_EQ4MIX_INPUT_2_SOURCE:
	case ARIZONA_EQ4MIX_INPUT_2_VOLUME:
	case ARIZONA_EQ4MIX_INPUT_3_SOURCE:
	case ARIZONA_EQ4MIX_INPUT_3_VOLUME:
	case ARIZONA_EQ4MIX_INPUT_4_SOURCE:
	case ARIZONA_EQ4MIX_INPUT_4_VOLUME:
	case ARIZONA_DRC1LMIX_INPUT_1_SOURCE:
	case ARIZONA_DRC1LMIX_INPUT_1_VOLUME:
	case ARIZONA_DRC1LMIX_INPUT_2_SOURCE:
	case ARIZONA_DRC1LMIX_INPUT_2_VOLUME:
	case ARIZONA_DRC1LMIX_INPUT_3_SOURCE:
	case ARIZONA_DRC1LMIX_INPUT_3_VOLUME:
	case ARIZONA_DRC1LMIX_INPUT_4_SOURCE:
	case ARIZONA_DRC1LMIX_INPUT_4_VOLUME:
	case ARIZONA_DRC1RMIX_INPUT_1_SOURCE:
	case ARIZONA_DRC1RMIX_INPUT_1_VOLUME:
	case ARIZONA_DRC1RMIX_INPUT_2_SOURCE:
	case ARIZONA_DRC1RMIX_INPUT_2_VOLUME:
	case ARIZONA_DRC1RMIX_INPUT_3_SOURCE:
	case ARIZONA_DRC1RMIX_INPUT_3_VOLUME:
	case ARIZONA_DRC1RMIX_INPUT_4_SOURCE:
	case ARIZONA_DRC1RMIX_INPUT_4_VOLUME:
	case ARIZONA_DRC2LMIX_INPUT_1_SOURCE:
	case ARIZONA_DRC2LMIX_INPUT_1_VOLUME:
	case ARIZONA_DRC2LMIX_INPUT_2_SOURCE:
	case ARIZONA_DRC2LMIX_INPUT_2_VOLUME:
	case ARIZONA_DRC2LMIX_INPUT_3_SOURCE:
	case ARIZONA_DRC2LMIX_INPUT_3_VOLUME:
	case ARIZONA_DRC2LMIX_INPUT_4_SOURCE:
	case ARIZONA_DRC2LMIX_INPUT_4_VOLUME:
	case ARIZONA_DRC2RMIX_INPUT_1_SOURCE:
	case ARIZONA_DRC2RMIX_INPUT_1_VOLUME:
	case ARIZONA_DRC2RMIX_INPUT_2_SOURCE:
	case ARIZONA_DRC2RMIX_INPUT_2_VOLUME:
	case ARIZONA_DRC2RMIX_INPUT_3_SOURCE:
	case ARIZONA_DRC2RMIX_INPUT_3_VOLUME:
	case ARIZONA_DRC2RMIX_INPUT_4_SOURCE:
	case ARIZONA_DRC2RMIX_INPUT_4_VOLUME:
	case ARIZONA_HPLP1MIX_INPUT_1_SOURCE:
	case ARIZONA_HPLP1MIX_INPUT_1_VOLUME:
	case ARIZONA_HPLP1MIX_INPUT_2_SOURCE:
	case ARIZONA_HPLP1MIX_INPUT_2_VOLUME:
	case ARIZONA_HPLP1MIX_INPUT_3_SOURCE:
	case ARIZONA_HPLP1MIX_INPUT_3_VOLUME:
	case ARIZONA_HPLP1MIX_INPUT_4_SOURCE:
	case ARIZONA_HPLP1MIX_INPUT_4_VOLUME:
	case ARIZONA_HPLP2MIX_INPUT_1_SOURCE:
	case ARIZONA_HPLP2MIX_INPUT_1_VOLUME:
	case ARIZONA_HPLP2MIX_INPUT_2_SOURCE:
	case ARIZONA_HPLP2MIX_INPUT_2_VOLUME:
	case ARIZONA_HPLP2MIX_INPUT_3_SOURCE:
	case ARIZONA_HPLP2MIX_INPUT_3_VOLUME:
	case ARIZONA_HPLP2MIX_INPUT_4_SOURCE:
	case ARIZONA_HPLP2MIX_INPUT_4_VOLUME:
	case ARIZONA_HPLP3MIX_INPUT_1_SOURCE:
	case ARIZONA_HPLP3MIX_INPUT_1_VOLUME:
	case ARIZONA_HPLP3MIX_INPUT_2_SOURCE:
	case ARIZONA_HPLP3MIX_INPUT_2_VOLUME:
	case ARIZONA_HPLP3MIX_INPUT_3_SOURCE:
	case ARIZONA_HPLP3MIX_INPUT_3_VOLUME:
	case ARIZONA_HPLP3MIX_INPUT_4_SOURCE:
	case ARIZONA_HPLP3MIX_INPUT_4_VOLUME:
	case ARIZONA_HPLP4MIX_INPUT_1_SOURCE:
	case ARIZONA_HPLP4MIX_INPUT_1_VOLUME:
	case ARIZONA_HPLP4MIX_INPUT_2_SOURCE:
	case ARIZONA_HPLP4MIX_INPUT_2_VOLUME:
	case ARIZONA_HPLP4MIX_INPUT_3_SOURCE:
	case ARIZONA_HPLP4MIX_INPUT_3_VOLUME:
	case ARIZONA_HPLP4MIX_INPUT_4_SOURCE:
	case ARIZONA_HPLP4MIX_INPUT_4_VOLUME:
	case ARIZONA_DSP1LMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1LMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP1LMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP1LMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP1LMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP1LMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP1LMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP1LMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP1RMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1RMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP1RMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP1RMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP1RMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP1RMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP1RMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP1RMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP1AUX1MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1AUX2MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1AUX3MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1AUX4MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1AUX5MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP1AUX6MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2LMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2LMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP2LMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP2LMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP2LMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP2LMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP2LMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP2LMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP2RMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2RMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP2RMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP2RMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP2RMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP2RMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP2RMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP2RMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP2AUX1MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2AUX2MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2AUX3MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2AUX4MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2AUX5MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP2AUX6MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3LMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3LMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP3LMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP3LMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP3LMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP3LMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP3LMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP3LMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP3RMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3RMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP3RMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP3RMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP3RMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP3RMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP3RMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP3RMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP3AUX1MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3AUX2MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3AUX3MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3AUX4MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3AUX5MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP3AUX6MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4LMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4LMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP4LMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP4LMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP4LMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP4LMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP4LMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP4LMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP4RMIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4RMIX_INPUT_1_VOLUME:
	case ARIZONA_DSP4RMIX_INPUT_2_SOURCE:
	case ARIZONA_DSP4RMIX_INPUT_2_VOLUME:
	case ARIZONA_DSP4RMIX_INPUT_3_SOURCE:
	case ARIZONA_DSP4RMIX_INPUT_3_VOLUME:
	case ARIZONA_DSP4RMIX_INPUT_4_SOURCE:
	case ARIZONA_DSP4RMIX_INPUT_4_VOLUME:
	case ARIZONA_DSP4AUX1MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4AUX2MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4AUX3MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4AUX4MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4AUX5MIX_INPUT_1_SOURCE:
	case ARIZONA_DSP4AUX6MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5LMIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5LMIX_INPUT_1_VOLUME:
	case CLEARWATER_DSP5LMIX_INPUT_2_SOURCE:
	case CLEARWATER_DSP5LMIX_INPUT_2_VOLUME:
	case CLEARWATER_DSP5LMIX_INPUT_3_SOURCE:
	case CLEARWATER_DSP5LMIX_INPUT_3_VOLUME:
	case CLEARWATER_DSP5LMIX_INPUT_4_SOURCE:
	case CLEARWATER_DSP5LMIX_INPUT_4_VOLUME:
	case CLEARWATER_DSP5RMIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5RMIX_INPUT_1_VOLUME:
	case CLEARWATER_DSP5RMIX_INPUT_2_SOURCE:
	case CLEARWATER_DSP5RMIX_INPUT_2_VOLUME:
	case CLEARWATER_DSP5RMIX_INPUT_3_SOURCE:
	case CLEARWATER_DSP5RMIX_INPUT_3_VOLUME:
	case CLEARWATER_DSP5RMIX_INPUT_4_SOURCE:
	case CLEARWATER_DSP5RMIX_INPUT_4_VOLUME:
	case CLEARWATER_DSP5AUX1MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5AUX2MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5AUX3MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5AUX4MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5AUX5MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP5AUX6MIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC1_1LMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC1_1RMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC1_2LMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC1_2RMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC2_1LMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC2_1RMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC2_2LMIX_INPUT_1_SOURCE:
	case CLEARWATER_ASRC2_2RMIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1DEC1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1DEC2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1DEC3MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1DEC4MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1INT1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1INT2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1INT3MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC1INT4MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2DEC1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2DEC2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2DEC3MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2DEC4MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2INT1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2INT2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2INT3MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC2INT4MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC3DEC1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC3DEC2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC3INT1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC3INT2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC4DEC1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC4DEC2MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC4INT1MIX_INPUT_1_SOURCE:
	case ARIZONA_ISRC4INT2MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6LMIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6LMIX_INPUT_1_VOLUME:
	case CLEARWATER_DSP6LMIX_INPUT_2_SOURCE:
	case CLEARWATER_DSP6LMIX_INPUT_2_VOLUME:
	case CLEARWATER_DSP6LMIX_INPUT_3_SOURCE:
	case CLEARWATER_DSP6LMIX_INPUT_3_VOLUME:
	case CLEARWATER_DSP6LMIX_INPUT_4_SOURCE:
	case CLEARWATER_DSP6LMIX_INPUT_4_VOLUME:
	case CLEARWATER_DSP6RMIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6RMIX_INPUT_1_VOLUME:
	case CLEARWATER_DSP6RMIX_INPUT_2_SOURCE:
	case CLEARWATER_DSP6RMIX_INPUT_2_VOLUME:
	case CLEARWATER_DSP6RMIX_INPUT_3_SOURCE:
	case CLEARWATER_DSP6RMIX_INPUT_3_VOLUME:
	case CLEARWATER_DSP6RMIX_INPUT_4_SOURCE:
	case CLEARWATER_DSP6RMIX_INPUT_4_VOLUME:
	case CLEARWATER_DSP6AUX1MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6AUX2MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6AUX3MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6AUX4MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6AUX5MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP6AUX6MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7LMIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7LMIX_INPUT_1_VOLUME:
	case CLEARWATER_DSP7LMIX_INPUT_2_SOURCE:
	case CLEARWATER_DSP7LMIX_INPUT_2_VOLUME:
	case CLEARWATER_DSP7LMIX_INPUT_3_SOURCE:
	case CLEARWATER_DSP7LMIX_INPUT_3_VOLUME:
	case CLEARWATER_DSP7LMIX_INPUT_4_SOURCE:
	case CLEARWATER_DSP7LMIX_INPUT_4_VOLUME:
	case CLEARWATER_DSP7RMIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7RMIX_INPUT_1_VOLUME:
	case CLEARWATER_DSP7RMIX_INPUT_2_SOURCE:
	case CLEARWATER_DSP7RMIX_INPUT_2_VOLUME:
	case CLEARWATER_DSP7RMIX_INPUT_3_SOURCE:
	case CLEARWATER_DSP7RMIX_INPUT_3_VOLUME:
	case CLEARWATER_DSP7RMIX_INPUT_4_SOURCE:
	case CLEARWATER_DSP7RMIX_INPUT_4_VOLUME:
	case CLEARWATER_DSP7AUX1MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7AUX2MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7AUX3MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7AUX4MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7AUX5MIX_INPUT_1_SOURCE:
	case CLEARWATER_DSP7AUX6MIX_INPUT_1_SOURCE:
	case ARIZONA_JACK_DETECT_DEBOUNCE:
	case ARIZONA_FX_CTRL1:
	case ARIZONA_FX_CTRL2:
	case ARIZONA_EQ1_1:
	case ARIZONA_EQ1_2:
	case ARIZONA_EQ1_3:
	case ARIZONA_EQ1_4:
	case ARIZONA_EQ1_5:
	case ARIZONA_EQ1_6:
	case ARIZONA_EQ1_7:
	case ARIZONA_EQ1_8:
	case ARIZONA_EQ1_9:
	case ARIZONA_EQ1_10:
	case ARIZONA_EQ1_11:
	case ARIZONA_EQ1_12:
	case ARIZONA_EQ1_13:
	case ARIZONA_EQ1_14:
	case ARIZONA_EQ1_15:
	case ARIZONA_EQ1_16:
	case ARIZONA_EQ1_17:
	case ARIZONA_EQ1_18:
	case ARIZONA_EQ1_19:
	case ARIZONA_EQ1_20:
	case ARIZONA_EQ1_21:
	case ARIZONA_EQ2_1:
	case ARIZONA_EQ2_2:
	case ARIZONA_EQ2_3:
	case ARIZONA_EQ2_4:
	case ARIZONA_EQ2_5:
	case ARIZONA_EQ2_6:
	case ARIZONA_EQ2_7:
	case ARIZONA_EQ2_8:
	case ARIZONA_EQ2_9:
	case ARIZONA_EQ2_10:
	case ARIZONA_EQ2_11:
	case ARIZONA_EQ2_12:
	case ARIZONA_EQ2_13:
	case ARIZONA_EQ2_14:
	case ARIZONA_EQ2_15:
	case ARIZONA_EQ2_16:
	case ARIZONA_EQ2_17:
	case ARIZONA_EQ2_18:
	case ARIZONA_EQ2_19:
	case ARIZONA_EQ2_20:
	case ARIZONA_EQ2_21:
	case ARIZONA_EQ3_1:
	case ARIZONA_EQ3_2:
	case ARIZONA_EQ3_3:
	case ARIZONA_EQ3_4:
	case ARIZONA_EQ3_5:
	case ARIZONA_EQ3_6:
	case ARIZONA_EQ3_7:
	case ARIZONA_EQ3_8:
	case ARIZONA_EQ3_9:
	case ARIZONA_EQ3_10:
	case ARIZONA_EQ3_11:
	case ARIZONA_EQ3_12:
	case ARIZONA_EQ3_13:
	case ARIZONA_EQ3_14:
	case ARIZONA_EQ3_15:
	case ARIZONA_EQ3_16:
	case ARIZONA_EQ3_17:
	case ARIZONA_EQ3_18:
	case ARIZONA_EQ3_19:
	case ARIZONA_EQ3_20:
	case ARIZONA_EQ3_21:
	case ARIZONA_EQ4_1:
	case ARIZONA_EQ4_2:
	case ARIZONA_EQ4_3:
	case ARIZONA_EQ4_4:
	case ARIZONA_EQ4_5:
	case ARIZONA_EQ4_6:
	case ARIZONA_EQ4_7:
	case ARIZONA_EQ4_8:
	case ARIZONA_EQ4_9:
	case ARIZONA_EQ4_10:
	case ARIZONA_EQ4_11:
	case ARIZONA_EQ4_12:
	case ARIZONA_EQ4_13:
	case ARIZONA_EQ4_14:
	case ARIZONA_EQ4_15:
	case ARIZONA_EQ4_16:
	case ARIZONA_EQ4_17:
	case ARIZONA_EQ4_18:
	case ARIZONA_EQ4_19:
	case ARIZONA_EQ4_20:
	case ARIZONA_EQ4_21:
	case ARIZONA_DRC1_CTRL1:
	case ARIZONA_DRC1_CTRL2:
	case ARIZONA_DRC1_CTRL3:
	case ARIZONA_DRC1_CTRL4:
	case ARIZONA_DRC1_CTRL5:
	case CLEARWATER_DRC2_CTRL1:
	case CLEARWATER_DRC2_CTRL2:
	case CLEARWATER_DRC2_CTRL3:
	case CLEARWATER_DRC2_CTRL4:
	case CLEARWATER_DRC2_CTRL5:
	case ARIZONA_HPLPF1_1:
	case ARIZONA_HPLPF1_2:
	case ARIZONA_HPLPF2_1:
	case ARIZONA_HPLPF2_2:
	case ARIZONA_HPLPF3_1:
	case ARIZONA_HPLPF3_2:
	case ARIZONA_HPLPF4_1:
	case ARIZONA_HPLPF4_2:
	case CLEARWATER_ASRC1_ENABLE:
	case CLEARWATER_ASRC1_STATUS:
	case CLEARWATER_ASRC1_RATE1:
	case CLEARWATER_ASRC1_RATE2:
	case CLEARWATER_ASRC2_ENABLE:
	case CLEARWATER_ASRC2_STATUS:
	case CLEARWATER_ASRC2_RATE1:
	case CLEARWATER_ASRC2_RATE2:
	case ARIZONA_ISRC_1_CTRL_1:
	case ARIZONA_ISRC_1_CTRL_2:
	case ARIZONA_ISRC_1_CTRL_3:
	case ARIZONA_ISRC_2_CTRL_1:
	case ARIZONA_ISRC_2_CTRL_2:
	case ARIZONA_ISRC_2_CTRL_3:
	case ARIZONA_ISRC_3_CTRL_1:
	case ARIZONA_ISRC_3_CTRL_2:
	case ARIZONA_ISRC_3_CTRL_3:
	case ARIZONA_ISRC_4_CTRL_1:
	case ARIZONA_ISRC_4_CTRL_2:
	case ARIZONA_ISRC_4_CTRL_3:
	case ARIZONA_CLOCK_CONTROL:
	case ARIZONA_ANC_SRC:
	case ARIZONA_DSP_STATUS:
	case ARIZONA_ANC_COEFF_START ... ARIZONA_ANC_COEFF_END:
	case ARIZONA_FCL_FILTER_CONTROL:
	case ARIZONA_FCL_ADC_REFORMATTER_CONTROL:
	case ARIZONA_FCL_COEFF_START ... ARIZONA_FCL_COEFF_END:
	case CLEARWATER_FCR_FILTER_CONTROL:
	case CLEARWATER_FCR_ADC_REFORMATTER_CONTROL:
	case CLEARWATER_FCR_COEFF_START ... CLEARWATER_FCR_COEFF_END:
	case CLEARWATER_DAC_COMP_1:
	case CLEARWATER_DAC_COMP_2:
	case CLEARWATER_FRF_COEFFICIENT_1L_1:
	case CLEARWATER_FRF_COEFFICIENT_1L_2:
	case CLEARWATER_FRF_COEFFICIENT_1L_3:
	case CLEARWATER_FRF_COEFFICIENT_1L_4:
	case CLEARWATER_FRF_COEFFICIENT_1R_1:
	case CLEARWATER_FRF_COEFFICIENT_1R_2:
	case CLEARWATER_FRF_COEFFICIENT_1R_3:
	case CLEARWATER_FRF_COEFFICIENT_1R_4:
	case CLEARWATER_FRF_COEFFICIENT_2L_1:
	case CLEARWATER_FRF_COEFFICIENT_2L_2:
	case CLEARWATER_FRF_COEFFICIENT_2L_3:
	case CLEARWATER_FRF_COEFFICIENT_2L_4:
	case CLEARWATER_FRF_COEFFICIENT_2R_1:
	case CLEARWATER_FRF_COEFFICIENT_2R_2:
	case CLEARWATER_FRF_COEFFICIENT_2R_3:
	case CLEARWATER_FRF_COEFFICIENT_2R_4:
	case CLEARWATER_FRF_COEFFICIENT_3L_1:
	case CLEARWATER_FRF_COEFFICIENT_3L_2:
	case CLEARWATER_FRF_COEFFICIENT_3L_3:
	case CLEARWATER_FRF_COEFFICIENT_3L_4:
	case CLEARWATER_FRF_COEFFICIENT_3R_1:
	case CLEARWATER_FRF_COEFFICIENT_3R_2:
	case CLEARWATER_FRF_COEFFICIENT_3R_3:
	case CLEARWATER_FRF_COEFFICIENT_3R_4:
	case CLEARWATER_FRF_COEFFICIENT_4L_1:
	case CLEARWATER_FRF_COEFFICIENT_4L_2:
	case CLEARWATER_FRF_COEFFICIENT_4L_3:
	case CLEARWATER_FRF_COEFFICIENT_4L_4:
	case CLEARWATER_FRF_COEFFICIENT_4R_1:
	case CLEARWATER_FRF_COEFFICIENT_4R_2:
	case CLEARWATER_FRF_COEFFICIENT_4R_3:
	case CLEARWATER_FRF_COEFFICIENT_4R_4:
	case CLEARWATER_FRF_COEFFICIENT_5L_1:
	case CLEARWATER_FRF_COEFFICIENT_5L_2:
	case CLEARWATER_FRF_COEFFICIENT_5L_3:
	case CLEARWATER_FRF_COEFFICIENT_5L_4:
	case CLEARWATER_FRF_COEFFICIENT_5R_1:
	case CLEARWATER_FRF_COEFFICIENT_5R_2:
	case CLEARWATER_FRF_COEFFICIENT_5R_3:
	case CLEARWATER_FRF_COEFFICIENT_5R_4:
	case CLEARWATER_FRF_COEFFICIENT_6L_1:
	case CLEARWATER_FRF_COEFFICIENT_6L_2:
	case CLEARWATER_FRF_COEFFICIENT_6L_3:
	case CLEARWATER_FRF_COEFFICIENT_6L_4:
	case CLEARWATER_FRF_COEFFICIENT_6R_1:
	case CLEARWATER_FRF_COEFFICIENT_6R_2:
	case CLEARWATER_FRF_COEFFICIENT_6R_3:
	case CLEARWATER_FRF_COEFFICIENT_6R_4:
	case CLEARWATER_GPIO1_CTRL_1:
	case CLEARWATER_GPIO1_CTRL_2:
	case CLEARWATER_GPIO2_CTRL_1:
	case CLEARWATER_GPIO2_CTRL_2:
	case CLEARWATER_GPIO3_CTRL_1:
	case CLEARWATER_GPIO3_CTRL_2:
	case CLEARWATER_GPIO4_CTRL_1:
	case CLEARWATER_GPIO4_CTRL_2:
	case CLEARWATER_GPIO5_CTRL_1:
	case CLEARWATER_GPIO5_CTRL_2:
	case CLEARWATER_GPIO6_CTRL_1:
	case CLEARWATER_GPIO6_CTRL_2:
	case CLEARWATER_GPIO7_CTRL_1:
	case CLEARWATER_GPIO7_CTRL_2:
	case CLEARWATER_GPIO8_CTRL_1:
	case CLEARWATER_GPIO8_CTRL_2:
	case CLEARWATER_GPIO9_CTRL_1:
	case CLEARWATER_GPIO9_CTRL_2:
	case CLEARWATER_GPIO10_CTRL_1:
	case CLEARWATER_GPIO10_CTRL_2:
	case CLEARWATER_GPIO11_CTRL_1:
	case CLEARWATER_GPIO11_CTRL_2:
	case CLEARWATER_GPIO12_CTRL_1:
	case CLEARWATER_GPIO12_CTRL_2:
	case CLEARWATER_GPIO13_CTRL_1:
	case CLEARWATER_GPIO13_CTRL_2:
	case CLEARWATER_GPIO14_CTRL_1:
	case CLEARWATER_GPIO14_CTRL_2:
	case CLEARWATER_GPIO15_CTRL_1:
	case CLEARWATER_GPIO15_CTRL_2:
	case CLEARWATER_GPIO16_CTRL_1:
	case CLEARWATER_GPIO16_CTRL_2:
	case CLEARWATER_GPIO17_CTRL_1:
	case CLEARWATER_GPIO17_CTRL_2:
	case CLEARWATER_GPIO18_CTRL_1:
	case CLEARWATER_GPIO18_CTRL_2:
	case CLEARWATER_GPIO19_CTRL_1:
	case CLEARWATER_GPIO19_CTRL_2:
	case CLEARWATER_GPIO20_CTRL_1:
	case CLEARWATER_GPIO20_CTRL_2:
	case CLEARWATER_GPIO21_CTRL_1:
	case CLEARWATER_GPIO21_CTRL_2:
	case CLEARWATER_GPIO22_CTRL_1:
	case CLEARWATER_GPIO22_CTRL_2:
	case CLEARWATER_GPIO23_CTRL_1:
	case CLEARWATER_GPIO23_CTRL_2:
	case CLEARWATER_GPIO24_CTRL_1:
	case CLEARWATER_GPIO24_CTRL_2:
	case CLEARWATER_GPIO25_CTRL_1:
	case CLEARWATER_GPIO25_CTRL_2:
	case CLEARWATER_GPIO26_CTRL_1:
	case CLEARWATER_GPIO26_CTRL_2:
	case CLEARWATER_GPIO27_CTRL_1:
	case CLEARWATER_GPIO27_CTRL_2:
	case CLEARWATER_GPIO28_CTRL_1:
	case CLEARWATER_GPIO28_CTRL_2:
	case CLEARWATER_GPIO29_CTRL_1:
	case CLEARWATER_GPIO29_CTRL_2:
	case CLEARWATER_GPIO30_CTRL_1:
	case CLEARWATER_GPIO30_CTRL_2:
	case CLEARWATER_GPIO31_CTRL_1:
	case CLEARWATER_GPIO31_CTRL_2:
	case CLEARWATER_GPIO32_CTRL_1:
	case CLEARWATER_GPIO32_CTRL_2:
	case CLEARWATER_GPIO33_CTRL_1:
	case CLEARWATER_GPIO33_CTRL_2:
	case CLEARWATER_GPIO34_CTRL_1:
	case CLEARWATER_GPIO34_CTRL_2:
	case CLEARWATER_GPIO35_CTRL_1:
	case CLEARWATER_GPIO35_CTRL_2:
	case CLEARWATER_GPIO36_CTRL_1:
	case CLEARWATER_GPIO36_CTRL_2:
	case CLEARWATER_GPIO37_CTRL_1:
	case CLEARWATER_GPIO37_CTRL_2:
	case CLEARWATER_GPIO38_CTRL_1:
	case CLEARWATER_GPIO38_CTRL_2:
	case CLEARWATER_GPIO39_CTRL_1:
	case CLEARWATER_GPIO39_CTRL_2:
	case CLEARWATER_GPIO40_CTRL_1:
	case CLEARWATER_GPIO40_CTRL_2:
	case CLEARWATER_IRQ1_STATUS_1:
	case CLEARWATER_IRQ1_STATUS_2:
	case CLEARWATER_IRQ1_STATUS_3:
	case CLEARWATER_IRQ1_STATUS_4:
	case CLEARWATER_IRQ1_STATUS_5:
	case CLEARWATER_IRQ1_STATUS_6:
	case CLEARWATER_IRQ1_STATUS_7:
	case CLEARWATER_IRQ1_STATUS_8:
	case CLEARWATER_IRQ1_STATUS_9:
	case CLEARWATER_IRQ1_STATUS_10:
	case CLEARWATER_IRQ1_STATUS_11:
	case CLEARWATER_IRQ1_STATUS_12:
	case CLEARWATER_IRQ1_STATUS_13:
	case CLEARWATER_IRQ1_STATUS_14:
	case CLEARWATER_IRQ1_STATUS_15:
	case CLEARWATER_IRQ1_STATUS_16:
	case CLEARWATER_IRQ1_STATUS_17:
	case CLEARWATER_IRQ1_STATUS_18:
	case CLEARWATER_IRQ1_STATUS_19:
	case CLEARWATER_IRQ1_STATUS_20:
	case CLEARWATER_IRQ1_STATUS_21:
	case CLEARWATER_IRQ1_STATUS_22:
	case CLEARWATER_IRQ1_STATUS_23:
	case CLEARWATER_IRQ1_STATUS_24:
	case CLEARWATER_IRQ1_STATUS_25:
	case CLEARWATER_IRQ1_STATUS_26:
	case CLEARWATER_IRQ1_STATUS_27:
	case CLEARWATER_IRQ1_STATUS_28:
	case CLEARWATER_IRQ1_STATUS_29:
	case CLEARWATER_IRQ1_STATUS_30:
	case CLEARWATER_IRQ1_STATUS_31:
	case CLEARWATER_IRQ1_STATUS_32:
	case CLEARWATER_IRQ1_MASK_1:
	case CLEARWATER_IRQ1_MASK_2:
	case CLEARWATER_IRQ1_MASK_3:
	case CLEARWATER_IRQ1_MASK_4:
	case CLEARWATER_IRQ1_MASK_5:
	case CLEARWATER_IRQ1_MASK_6:
	case CLEARWATER_IRQ1_MASK_7:
	case CLEARWATER_IRQ1_MASK_8:
	case CLEARWATER_IRQ1_MASK_9:
	case CLEARWATER_IRQ1_MASK_10:
	case CLEARWATER_IRQ1_MASK_11:
	case CLEARWATER_IRQ1_MASK_12:
	case CLEARWATER_IRQ1_MASK_13:
	case CLEARWATER_IRQ1_MASK_14:
	case CLEARWATER_IRQ1_MASK_15:
	case CLEARWATER_IRQ1_MASK_17:
	case CLEARWATER_IRQ1_MASK_18:
	case CLEARWATER_IRQ1_MASK_19:
	case CLEARWATER_IRQ1_MASK_21:
	case CLEARWATER_IRQ1_MASK_22:
	case CLEARWATER_IRQ1_MASK_23:
	case CLEARWATER_IRQ1_MASK_24:
	case CLEARWATER_IRQ1_MASK_25:
	case CLEARWATER_IRQ1_MASK_27:
	case CLEARWATER_IRQ1_MASK_28:
	case CLEARWATER_IRQ1_MASK_30:
	case CLEARWATER_IRQ1_MASK_31:
	case CLEARWATER_IRQ1_MASK_32:
	case CLEARWATER_IRQ1_RAW_STATUS_1:
	case CLEARWATER_IRQ1_RAW_STATUS_2:
	case CLEARWATER_IRQ1_RAW_STATUS_7:
	case CLEARWATER_IRQ1_RAW_STATUS_9:
	case CLEARWATER_IRQ1_RAW_STATUS_11:
	case CLEARWATER_IRQ1_RAW_STATUS_12:
	case CLEARWATER_IRQ1_RAW_STATUS_13:
	case CLEARWATER_IRQ1_RAW_STATUS_14:
	case CLEARWATER_IRQ1_RAW_STATUS_15:
	case CLEARWATER_IRQ1_RAW_STATUS_17:
	case CLEARWATER_IRQ1_RAW_STATUS_18:
	case CLEARWATER_IRQ1_RAW_STATUS_19:
	case CLEARWATER_IRQ1_RAW_STATUS_21:
	case CLEARWATER_IRQ1_RAW_STATUS_22:
	case CLEARWATER_IRQ1_RAW_STATUS_23:
	case CLEARWATER_IRQ1_RAW_STATUS_24:
	case CLEARWATER_IRQ1_RAW_STATUS_25:
	case CLEARWATER_IRQ1_RAW_STATUS_30:
	case CLEARWATER_IRQ1_RAW_STATUS_31:
	case CLEARWATER_IRQ1_RAW_STATUS_32:
	case CLEARWATER_IRQ2_STATUS_9:
	case CLEARWATER_IRQ2_MASK_9:
	case CLEARWATER_IRQ2_RAW_STATUS_9:
	case CLEARWATER_IRQ1_CTRL:
		return true;
	default:
		return false;
	}
}

static bool clearwater_16bit_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x2C2:
	case ARIZONA_SOFTWARE_RESET:
	case ARIZONA_DEVICE_REVISION:
	case ARIZONA_HAPTICS_STATUS:
	case ARIZONA_SAMPLE_RATE_1_STATUS:
	case ARIZONA_SAMPLE_RATE_2_STATUS:
	case ARIZONA_SAMPLE_RATE_3_STATUS:
	case ARIZONA_ASYNC_SAMPLE_RATE_1_STATUS:
	case ARIZONA_ASYNC_SAMPLE_RATE_2_STATUS:
	case ARIZONA_HP_CTRL_1L:
	case ARIZONA_HP_CTRL_1R:
	case ARIZONA_HP_CTRL_2L:
	case ARIZONA_HP_CTRL_2R:
	case ARIZONA_HP_CTRL_3L:
	case ARIZONA_HP_CTRL_3R:
	case ARIZONA_DCS_HP1L_CONTROL:
	case ARIZONA_DCS_HP1R_CONTROL:
	case ARIZONA_MIC_DETECT_3:
	case ARIZONA_MIC_DETECT_4:
	case ARIZONA_HEADPHONE_DETECT_2:
	case ARIZONA_HEADPHONE_DETECT_3:
	case ARIZONA_HP_DACVAL:
	case ARIZONA_INPUT_ENABLES_STATUS:
	case ARIZONA_OUTPUT_STATUS_1:
	case ARIZONA_RAW_OUTPUT_STATUS_1:
	case ARIZONA_ADC_VCO_CAL_4:
	case ARIZONA_ADC_VCO_CAL_5:
	case ARIZONA_ADC_VCO_CAL_6:
	case ARIZONA_ADC_VCO_CAL_7:
	case ARIZONA_ADC_VCO_CAL_8:
	case ARIZONA_ADC_VCO_CAL_9:
	case ARIZONA_SPD1_TX_CHANNEL_STATUS_1:
	case ARIZONA_SPD1_TX_CHANNEL_STATUS_2:
	case ARIZONA_SPD1_TX_CHANNEL_STATUS_3:
	case ARIZONA_SLIMBUS_RX_PORT_STATUS:
	case ARIZONA_SLIMBUS_TX_PORT_STATUS:
	case ARIZONA_FX_CTRL2:
	case CLEARWATER_ASRC2_STATUS:
	case CLEARWATER_ASRC1_STATUS:
	case CLEARWATER_IRQ1_STATUS_1:
	case CLEARWATER_IRQ1_STATUS_2:
	case CLEARWATER_IRQ1_STATUS_6:
	case CLEARWATER_IRQ1_STATUS_7:
	case CLEARWATER_IRQ1_STATUS_9:
	case CLEARWATER_IRQ1_STATUS_11:
	case CLEARWATER_IRQ1_STATUS_12:
	case CLEARWATER_IRQ1_STATUS_13:
	case CLEARWATER_IRQ1_STATUS_14:
	case CLEARWATER_IRQ1_STATUS_15:
	case CLEARWATER_IRQ1_STATUS_17:
	case CLEARWATER_IRQ1_STATUS_18:
	case CLEARWATER_IRQ1_STATUS_19:
	case CLEARWATER_IRQ1_STATUS_21:
	case CLEARWATER_IRQ1_STATUS_22:
	case CLEARWATER_IRQ1_STATUS_23:
	case CLEARWATER_IRQ1_STATUS_24:
	case CLEARWATER_IRQ1_STATUS_25:
	case CLEARWATER_IRQ1_STATUS_27:
	case CLEARWATER_IRQ1_STATUS_28:
	case CLEARWATER_IRQ1_STATUS_30:
	case CLEARWATER_IRQ1_STATUS_31:
	case CLEARWATER_IRQ1_STATUS_32:
	case CLEARWATER_IRQ1_RAW_STATUS_1:
	case CLEARWATER_IRQ1_RAW_STATUS_2:
	case CLEARWATER_IRQ1_RAW_STATUS_7:
	case CLEARWATER_IRQ1_RAW_STATUS_9:
	case CLEARWATER_IRQ1_RAW_STATUS_11:
	case CLEARWATER_IRQ1_RAW_STATUS_12:
	case CLEARWATER_IRQ1_RAW_STATUS_13:
	case CLEARWATER_IRQ1_RAW_STATUS_14:
	case CLEARWATER_IRQ1_RAW_STATUS_15:
	case CLEARWATER_IRQ1_RAW_STATUS_17:
	case CLEARWATER_IRQ1_RAW_STATUS_18:
	case CLEARWATER_IRQ1_RAW_STATUS_19:
	case CLEARWATER_IRQ1_RAW_STATUS_21:
	case CLEARWATER_IRQ1_RAW_STATUS_22:
	case CLEARWATER_IRQ1_RAW_STATUS_23:
	case CLEARWATER_IRQ1_RAW_STATUS_24:
	case CLEARWATER_IRQ1_RAW_STATUS_25:
	case CLEARWATER_IRQ1_RAW_STATUS_30:
	case CLEARWATER_IRQ1_RAW_STATUS_31:
	case CLEARWATER_IRQ1_RAW_STATUS_32:
	case CLEARWATER_IRQ2_STATUS_9:
	case CLEARWATER_IRQ2_RAW_STATUS_9:
		return true;
	default:
		return false;
	}
}

static bool clearwater_32bit_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ARIZONA_WSEQ_SEQUENCE_1 ... ARIZONA_WSEQ_SEQUENCE_508:
	case CLEARWATER_OTP_HPDET_CALIB_1 ... CLEARWATER_OTP_HPDET_CALIB_2:
	case CLEARWATER_DSP1_CONFIG ... CLEARWATER_DSP1_SCRATCH_2_3:
	case CLEARWATER_DSP2_CONFIG ... CLEARWATER_DSP2_SCRATCH_2_3:
	case CLEARWATER_DSP3_CONFIG ... CLEARWATER_DSP3_SCRATCH_2_3:
	case CLEARWATER_DSP4_CONFIG ... CLEARWATER_DSP4_SCRATCH_2_3:
	case CLEARWATER_DSP5_CONFIG ... CLEARWATER_DSP5_SCRATCH_2_3:
	case CLEARWATER_DSP6_CONFIG ... CLEARWATER_DSP6_SCRATCH_2_3:
	case CLEARWATER_DSP7_CONFIG ... CLEARWATER_DSP7_SCRATCH_2_3:
		return true;
	default:
		return clearwater_is_adsp_memory(dev, reg);
	}
}

static bool clearwater_32bit_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ARIZONA_WSEQ_SEQUENCE_1 ... ARIZONA_WSEQ_SEQUENCE_508:
	case CLEARWATER_OTP_HPDET_CALIB_1 ... CLEARWATER_OTP_HPDET_CALIB_2:
	case CLEARWATER_DSP1_CONFIG ... CLEARWATER_DSP1_SCRATCH_2_3:
	case CLEARWATER_DSP2_CONFIG ... CLEARWATER_DSP2_SCRATCH_2_3:
	case CLEARWATER_DSP3_CONFIG ... CLEARWATER_DSP3_SCRATCH_2_3:
	case CLEARWATER_DSP4_CONFIG ... CLEARWATER_DSP4_SCRATCH_2_3:
	case CLEARWATER_DSP5_CONFIG ... CLEARWATER_DSP5_SCRATCH_2_3:
	case CLEARWATER_DSP6_CONFIG ... CLEARWATER_DSP6_SCRATCH_2_3:
	case CLEARWATER_DSP7_CONFIG ... CLEARWATER_DSP7_SCRATCH_2_3:
		return true;
	default:
		return clearwater_is_adsp_memory(dev, reg);
	}
}

const struct regmap_config clearwater_16bit_spi_regmap = {
	.name = "clearwater_16bit",
	.reg_bits = 32,
	.pad_bits = 16,
	.val_bits = 16,

	.max_register = 0x2fff,
	.readable_reg = clearwater_16bit_readable_register,
	.volatile_reg = clearwater_16bit_volatile_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = clearwater_reg_default,
	.num_reg_defaults = ARRAY_SIZE(clearwater_reg_default),
};
EXPORT_SYMBOL_GPL(clearwater_16bit_spi_regmap);

const struct regmap_config clearwater_16bit_i2c_regmap = {
	.name = "clearwater_16bit",
	.reg_bits = 32,
	.val_bits = 16,

	.max_register = 0x2fff,
	.readable_reg = clearwater_16bit_readable_register,
	.volatile_reg = clearwater_16bit_volatile_register,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = clearwater_reg_default,
	.num_reg_defaults = ARRAY_SIZE(clearwater_reg_default),
};
EXPORT_SYMBOL_GPL(clearwater_16bit_i2c_regmap);

const struct regmap_config clearwater_32bit_spi_regmap = {
	.name = "clearwater_32bit",
	.reg_bits = 32,
	.reg_stride = 2,
	.pad_bits = 16,
	.val_bits = 32,

	.max_register = CLEARWATER_DSP7_SCRATCH_2_3,
	.readable_reg = clearwater_32bit_readable_register,
	.volatile_reg = clearwater_32bit_volatile_register,

	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL_GPL(clearwater_32bit_spi_regmap);

const struct regmap_config clearwater_32bit_i2c_regmap = {
	.name = "clearwater_32bit",
	.reg_bits = 32,
	.reg_stride = 2,
	.val_bits = 32,

	.max_register = CLEARWATER_DSP7_SCRATCH_2_3,
	.readable_reg = clearwater_32bit_readable_register,
	.volatile_reg = clearwater_32bit_volatile_register,

	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL_GPL(clearwater_32bit_i2c_regmap);
