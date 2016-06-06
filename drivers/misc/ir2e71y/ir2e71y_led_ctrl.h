/* drivers/misc/ir2e71y/ir2e71y_led_ctrl.h  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef IR2E71Y_LED_CTRL_H
#define IR2E71Y_LED_CTRL_H
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "ir2e71y_cmn.h"
#ifdef IR2E71Y_MODEL_MID
#else /* IR2E71Y_MODEL_MID */
#include "ir2e71y_led_default.h"
#endif /* IR2E71Y_MODEL_MID */

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_LED_FIX_ONR        (1)
#define IR2E71Y_LED_FIX_ONG        (2)
#define IR2E71Y_LED_FIX_ONB        (3)

#ifdef IR2E71Y_KEY_LED
static ir2e71y_bdicRegSetting_t ir2e71y_bdic_key_led_fix_on[] = {
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x38,                       0x38,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_key_led_ani_on[] = {
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x38,                       0x38,      0}
    ,{BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_RMW,    0x04,                       0xFC,   6000}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_key_led_off[] = {
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_CLR,    0x00,                       0xFC,      0}
    ,{BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0x38,   5500}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_key_led_off_system7[] = {
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_CLR,    0x00,                       0xFC,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_key_led_off_fix[] = {
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0x38,   5500}
};
#endif /* IR2E71Y_KEY_LED */

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_fix_on[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_CH0_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH1_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH2_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH0_B,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH1_B,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH2_B,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH0_C,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH1_C,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH2_C,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_SET,    0x20,                       0x20,      0},
     {BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_SET,    0x20,                       0x20,      0},
     {BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_SET,    0x20,                       0x20,      0},
#ifndef IR2E71Y_COLOR_LED_TWIN
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x07,                       0x07,      0}
#endif /* IR2E71Y_COLOR_LED_TWIN */
};

#ifndef IR2E71Y_COLOR_LED_TWIN
static ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_ani_on[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_TIMER,               IR2E71Y_BDIC_RMW,    0xF7,                       0xF7,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x07,                       0x07,      0},
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_RMW,    0x01,                       0xF3,   6000}
};

#ifdef IR2E71Y_ANIME_COLOR_LED
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_ani_on_triple_color[] = {
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x07,                       0x07,      0},
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_RMW,    0x01,                       0xF3,   6000}
};
#endif  /* IR2E71Y_ANIME_COLOR_LED */

#endif /* IR2E71Y_COLOR_LED_TWIN */

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_lposc_enable[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_SET,    0x10,                       0x10,      0}
};

#ifdef IR2E71Y_ANIME_COLOR_LED
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_high_speed_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x41,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x30,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x41,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x30,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x41,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x30,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_standard_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x42,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x22,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x42,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x22,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x42,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x22,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_breath_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_long_breath_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_wave_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_flash_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_aurora_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x31,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_rainbow_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x46,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x31,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_emopattern_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_illumi_triple_color_1st[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_RMW,    0x02,                       0x6F,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_RMW,    0x17,                       0xF7,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_RMW,    0x02,                       0x6F,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_RMW,    0x17,                       0xF7,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_RMW,    0x02,                       0x6F,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_RMW,    0x17,                       0xF7,      0}
    ,{BDIC_REG_TIMER,               IR2E71Y_BDIC_RMW,    0x30,                       0xF0,      0}
};

#ifdef IR2E71Y_EXTEND_COLOR_LED
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_pattern1_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_pattern2_on[] = {
     {BDIC_REG_CH0_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH0_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH1_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET1,            IR2E71Y_BDIC_STRM,   0x46,                       0xFF,      0}
    ,{BDIC_REG_CH2_SET2,            IR2E71Y_BDIC_STRM,   0x31,                       0xFF,      0}
};
#endif  /* IR2E71Y_EXTEND_COLOR_LED */

#ifdef IR2E71Y_COLOR_LED_TWIN
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_high_speed_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x41,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x30,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x41,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x30,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x41,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x30,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_standard_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x42,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x22,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x42,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x22,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x42,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x22,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_breath_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_long_breath_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_wave_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_flash_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_aurora_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x31,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_rainbow_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x02,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x06,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x07,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x46,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x31,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_emopattern_on_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_STRM,   0x03,                       0xFF,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_STRM,   0x05,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_illumi_triple_color_1st_twin[] = {
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_RMW,    0x02,                       0x6F,      0}
    ,{BDIC_REG_CH3_SET2,            IR2E71Y_BDIC_RMW,    0x17,                       0xF7,      0}
    ,{BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_RMW,    0x02,                       0x6F,      0}
    ,{BDIC_REG_CH4_SET2,            IR2E71Y_BDIC_RMW,    0x17,                       0xF7,      0}
    ,{BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_RMW,    0x02,                       0x6F,      0}
    ,{BDIC_REG_CH5_SET2,            IR2E71Y_BDIC_RMW,    0x17,                       0xF7,      0}
    ,{BDIC_REG_TIMER2,              IR2E71Y_BDIC_RMW,    0x30,                       0xF0,      0}
};
#endif  /* IR2E71Y_COLOR_LED_TWIN */
#endif  /* IR2E71Y_ANIME_COLOR_LED */

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_off[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
#ifdef IR2E71Y_COLOR_LED_TWIN
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0x3F,   5500}
#else  /* IR2E71Y_COLOR_LED_TWIN */
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_CLR,    0x00,                       0xF3,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0x07,   5500}
#endif /* IR2E71Y_COLOR_LED_TWIN */
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_lposc_disable[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_CLR,    0x00,                       0x10,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_off_fix[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
#ifdef IR2E71Y_COLOR_LED_TWIN
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0x3F,   5500}
#else  /* IR2E71Y_COLOR_LED_TWIN */
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0x07,   5500}
#endif /* IR2E71Y_COLOR_LED_TWIN */
};

#ifdef IR2E71Y_COLOR_LED_TWIN
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_fix_on_twin[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_CH3_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH4_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH5_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH3_B,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH4_B,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH5_B,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH3_C,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH4_C,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH5_C,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH3_SET1,            IR2E71Y_BDIC_SET,    0x20,                       0x20,      0},
     {BDIC_REG_CH4_SET1,            IR2E71Y_BDIC_SET,    0x20,                       0x20,      0},
     {BDIC_REG_CH5_SET1,            IR2E71Y_BDIC_SET,    0x20,                       0x20,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x3F,                       0x3F,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_ani_on_twin[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_TIMER,               IR2E71Y_BDIC_RMW,    0xF7,                       0xF7,      0},
     {BDIC_REG_TIMER2,              IR2E71Y_BDIC_RMW,    0xF7,                       0xF7,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x3F,                       0x3F,      0},
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_STR,    0x05,                       0xFF,   6000}
};

#ifdef IR2E71Y_ANIME_COLOR_LED
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_ani_on_triple_color_twin[] = {
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_SET,    0x3F,                       0x3F,      0},
     {BDIC_REG_SYSTEM7,             IR2E71Y_BDIC_STR,    0x05,                       0xFF,   6000}
};
#endif  /* IR2E71Y_ANIME_COLOR_LED */
#endif /* IR2E71Y_COLOR_LED_TWIN */


#ifdef IR2E71Y_SYSFS_LED
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_current[] = {
     {BDIC_REG_CH0_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH1_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH2_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
};

#ifdef IR2E71Y_COLOR_LED_TWIN
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_led_current_twin[] = {
     {BDIC_REG_CH3_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH4_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_CH5_A,               IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
};
#endif /* IR2E71Y_COLOR_LED_TWIN */
#endif /* IR2E71Y_SYSFS_LED */

#endif  /* IR2E71Y_LED_CTRL_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
