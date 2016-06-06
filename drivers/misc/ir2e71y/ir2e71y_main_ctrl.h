/* drivers/misc/ir2e71y/ir2e71y_main_ctrl.h  (Display Driver)
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

#ifndef IR2E71Y_MAIN_CTRL_H
#define IR2E71Y_MAIN_CTRL_H
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "ir2e71y_cmn.h"
#include "ir2e71y_data_default.h"

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_init1[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_STR,    0x80,                       0xFF,  10600},
     {BDIC_REG_SYSTEM3,             IR2E71Y_BDIC_CLR,    0x00,                       0x02,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_init12[] = {
     {BDIC_REG_GPIO_0,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GPIO_1,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
#ifndef IR2E71Y_MODEL_MID
     {BDIC_REG_GPIO_2,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GPIO_3,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GPIO_4,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
#endif /* IR2E71Y_MODEL_MID */
     {BDIC_REG_GPIO_5,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GPIO_6,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
#ifdef IR2E71Y_MODEL_MID
     {BDIC_REG_GPIO_ANSW,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
#else /* IR2E71Y_MODEL_MID */
     {BDIC_REG_GPIO_ANSW,           IR2E71Y_BDIC_STR,    0x1C,                       0xFF,      0},
#endif /* IR2E71Y_MODEL_MID */
     {BDIC_REG_GPIMSK0,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GPIMSK1,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GPIMSK2,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,      0},
     {BDIC_REG_INT_CTRL,            IR2E71Y_BDIC_STR,    0x02,                       0xFF,      0},
     {BDIC_REG_GPIO_OPD_PUSEL,      IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_init2[] = {
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_CLR,    0x00,                       0x7F,      0},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_STR,    0x20,                       0xFF,      0},
     {BDIC_REG_SYSTEM3,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_SYSTEM4,             IR2E71Y_BDIC_STR,    0x31,                       0xFF,      0},
     {BDIC_REG_SYSTEM5,             IR2E71Y_BDIC_CLR,    0x00,                       0xC0,      0},
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_STR,    0x28,                       0xFF,      0},
     {BDIC_REG_I2C_TIMER,           IR2E71Y_BDIC_STR,    0xFF,                       0xFF,      0},
     {BDIC_REG_I2C_SYS,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_I2C_DATA0,           IR2E71Y_BDIC_STR,    0x72,                       0xFF,      0},
     {BDIC_REG_XEN_TSD,             IR2E71Y_BDIC_STR,    0x01,                       0xFF,      0},

     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_STR,    0x01,                       0xFF,      0},
     {BDIC_REG_LDO,                 IR2E71Y_BDIC_STR,    0x01,                       0xFF,      0},
     {BDIC_REG_ALS_DATA0_SET,       IR2E71Y_BDIC_STR,    0x80,                       0xFF,      0},
     {BDIC_REG_ALS_DATA1_SET,       IR2E71Y_BDIC_STR,    0x21,                       0xFF,      0},

     {BDIC_REG_MODE_M1,             IR2E71Y_BDIC_STR,    0x03,                       0xFF,      0},
     {BDIC_REG_PSDATA_SET,          IR2E71Y_BDIC_STR,    0x02,                       0xFF,      0},
     {BDIC_REG_PS_HT_LSB,           IR2E71Y_BDIC_STR,    0x10,                       0xFF,      0},
     {BDIC_REG_PS_HT_MSB,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_PS_LT_LSB,           IR2E71Y_BDIC_STR,    0x0F,                       0xFF,      0},
     {BDIC_REG_PS_LT_MSB,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},

     {BDIC_REG_CPM1,                IR2E71Y_BDIC_STR,    0x05,                       0xFF,      0},
     {BDIC_REG_CPM2,                IR2E71Y_BDIC_STR,    0x31,                       0xFF,      0},
     {BDIC_REG_DCDC2_ERES2_1,       IR2E71Y_BDIC_STR,    0x33,                       0xFF,      0},
     {BDIC_REG_DCDC2_ERES2_2,       IR2E71Y_BDIC_STR,    0x05,                       0xFF,      0},
     {BDIC_REG_DCDC2_CLIMIT_1,      IR2E71Y_BDIC_STR,    0x06,                       0xFF,      0},
     {BDIC_REG_DCDC2_CLIMIT_2,      IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_DCDC2_GM,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_DCDC2_BF,            IR2E71Y_BDIC_STR,    0xC3,                       0xFF,      0},
     {BDIC_REG_DCDC2_SW,            IR2E71Y_BDIC_STR,    0xC3,                       0xFF,      0},
     {BDIC_REG_DCDC2_OSC_1,         IR2E71Y_BDIC_STR,    0xB1,                       0xFF,      0},
     {BDIC_REG_DCDC2_OSC_2,         IR2E71Y_BDIC_STR,    0x01,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_init3[] = {
     {BDIC_REG_VO2_PFML,            IR2E71Y_BDIC_STR,    BDIC_REG_VO2_PFML_VAL,      0xFF,      0},
     {BDIC_REG_VO2_PWML,            IR2E71Y_BDIC_STR,    BDIC_REG_VO2_PWML_VAL,      0xFF,      0},
     {BDIC_REG_VO2_PWMH,            IR2E71Y_BDIC_STR,    BDIC_REG_VO2_PWMH_VAL,      0xFF,      0}
    };

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_init4[] = {
     {BDIC_REG_DCCD2_MODE_SEL,      IR2E71Y_BDIC_STR,    0x85,                       0xFF,      0},
     {BDIC_REG_DCDC1_CUR_SEL_DRV,   IR2E71Y_BDIC_STR,    0x80,                       0xFF,      0},
     {BDIC_REG_DCDC2_TEST_57,       IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_DCDC1_CLIMT_1,       IR2E71Y_BDIC_STR,    0xF5,                       0xFF,      0},
     {BDIC_REG_DCDC1_CLIMT_2,       IR2E71Y_BDIC_STR,    BDIC_REG_DCDC1_CLIMT_2_VAL, 0xFF,      0},
     {BDIC_REG_DCDC1_GM,            IR2E71Y_BDIC_STR,    BDIC_REG_DCDC1_GM_VAL,      0xFF,      0},
     {BDIC_REG_DCDC1_BF,            IR2E71Y_BDIC_STR,    0xF3,                       0xFF,      0},
     {BDIC_REG_DCDC1_SW,            IR2E71Y_BDIC_STR,    0xF3,                       0xFF,      0},
     {BDIC_REG_DCDC1_OSC_1,         IR2E71Y_BDIC_STR,    0x31,                       0xFF,      0},
     {BDIC_REG_DCDC1_OSC_2,         IR2E71Y_BDIC_STR,    BDIC_REG_DCDC1_OSC_2_VAL,   0xFF,      0},
     {BDIC_REG_DCDC1_OVDETREF,      IR2E71Y_BDIC_STR,    0x84,                       0xFF,      0},
     {BDIC_REG_DCDC1_TIMER,         IR2E71Y_BDIC_STR,    0x04,                       0xFF,      0},
     {BDIC_REG_DCDC1_TH_LCUR,       IR2E71Y_BDIC_STR,    0x30,                       0xFF,      0},
     {BDIC_REG_DCDC1_TH_HCUR,       IR2E71Y_BDIC_STR,    0x34,                       0xFF,      0},
     {BDIC_REG_DCDC1_CUR_SOFT,      IR2E71Y_BDIC_STR,    0x04,                       0xFF,      0},
     {BDIC_REG_DCDC1_REF1_SOFT,     IR2E71Y_BDIC_STR,    0x22,                       0xFF,      0},
     {BDIC_REG_DCDC1_EAMPREF2,      IR2E71Y_BDIC_STR,    0x32,                       0xFF,      0},
     {BDIC_REG_DCDC1_PFMREF1,       IR2E71Y_BDIC_STR,    0x32,                       0xFF,      0},
     {BDIC_REG_DCDC1_PFMREF2,       IR2E71Y_BDIC_STR,    0x32,                       0xFF,      0},

     {BDIC_REG_M1LED,               IR2E71Y_BDIC_STR,    0xFF,                       0xFF,      0},
     {BDIC_REG_M2LED,               IR2E71Y_BDIC_STR,    0xFF,                       0xFF,      0},
     {BDIC_REG_PWMDC1,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_PWMDC2,              IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},

     {BDIC_REG_DETECTOR,            IR2E71Y_BDIC_STR,    0x02,                       0xFF,      0},
     {BDIC_REG_GIMR1,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMF1,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMR2,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMR3,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMR4,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMF3,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMF4,               IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_OPT_INT1,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_OPT_INT2,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},

     {BDIC_REG_AR_HI_TH_L_L,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_HI_TH_L_L_VAL,  0xFF,      0},
     {BDIC_REG_AR_HI_TH_L_H,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_HI_TH_L_H_VAL,  0xFF,      0},
     {BDIC_REG_AR_LO_TH_M_L,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_LO_TH_M_L_VAL,  0xFF,      0},
     {BDIC_REG_AR_LO_TH_M_H,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_LO_TH_M_H_VAL,  0xFF,      0},
     {BDIC_REG_AR_HI_TH_M_L,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_HI_TH_M_L_VAL,  0xFF,      0},
     {BDIC_REG_AR_HI_TH_M_H,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_HI_TH_M_H_VAL,  0xFF,      0},
     {BDIC_REG_AR_LO_TH_H_L,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_LO_TH_H_L_VAL,  0xFF,      0},
     {BDIC_REG_AR_LO_TH_H_H,        IR2E71Y_BDIC_STR,    BDIC_REG_AR_LO_TH_H_H_VAL,  0xFF,      0},
     {BDIC_REG_AR_RANGE_L,          IR2E71Y_BDIC_STR,    BDIC_REG_AR_RANGE_L_VAL,    0xFF,      0},
     {BDIC_REG_AR_RANGE_M,          IR2E71Y_BDIC_STR,    BDIC_REG_AR_RANGE_M_VAL,    0xFF,      0},
     {BDIC_REG_AR_RANGE_H,          IR2E71Y_BDIC_STR,    BDIC_REG_AR_RANGE_H_VAL,    0xFF,      0},
     {BDIC_REG_DATA_SHIFT_ML,       IR2E71Y_BDIC_STR,    BDIC_REG_DATA_SHIFT_M_VAL,  0xFF,      0},
     {BDIC_REG_DATA_SHIFT_H,        IR2E71Y_BDIC_STR,    BDIC_REG_DATA_SHIFT_H_VAL,  0xFF,      0},

     {BDIC_REG_CLR_IR_CMP_TH1,      IR2E71Y_BDIC_STR,    0x3E,                       0xFF,      0},
     {BDIC_REG_CLR_IR_CMP_TH2,      IR2E71Y_BDIC_STR,    0x3D,                       0xFF,      0},
     {BDIC_REG_CLR_IR_CMP_TH3,      IR2E71Y_BDIC_STR,    0x3C,                       0xFF,      0},
     {BDIC_REG_AR_DATA0,            IR2E71Y_BDIC_STR,    0x72,                       0xFF,      0},
     {BDIC_REG_AR_DATA1,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_AR_DATA2,            IR2E71Y_BDIC_STR,    0x5E,                       0xFF,      0},
     {BDIC_REG_AR_DATA3,            IR2E71Y_BDIC_STR,    0x01,                       0xFF,      0},
     {BDIC_REG_AR_DATA5,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xDE,                       0xFF,      0},
     {BDIC_REG_AR_RANGE_SEL,        IR2E71Y_BDIC_STR,    0x10,                       0xFF,      0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    BDIC_REG_AR_CTRL_VAL,       0xFF,      0},
     {BDIC_REG_SLOPE,               IR2E71Y_BDIC_STR,    0xFF,                       0xFF,      0},
     {BDIC_REG_TIMER_DIV,           IR2E71Y_BDIC_STR,    0x91,                       0xFF,      0},
     {BDIC_REG_I2C_TIMER,           IR2E71Y_BDIC_STR,    0xD0,                       0xFF,      0},

     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x01,                       0x00,  10000},
     {BDIC_REG_PS_DATA_L,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_PS_DATA_H,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_CLR_DATA_L,          IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_CLR_DATA_H,          IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_IR_DATA_L,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_IR_DATA_H,           IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},

     {BDIC_REG_ALS_ADJ0_0_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_0_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_0_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_0_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_0,         IR2E71Y_BDIC_STR,    BDIC_REG_ALS_SHIFT_DEFAULT_B,  0xFF,   0},
     {BDIC_REG_ALS_ADJ0_1_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_1_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_1_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_1_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_1,         IR2E71Y_BDIC_STR,    BDIC_REG_ALS_SHIFT_DEFAULT_B,  0xFF,   0},
     {BDIC_REG_ALS_ADJ0_2_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_2_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_2_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_2_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_2,         IR2E71Y_BDIC_STR,    BDIC_REG_ALS_SHIFT_DEFAULT_B,  0xFF,   0},
     {BDIC_REG_ALS_ADJ0_3_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_L_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_3_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ0_H_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_3_L,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_L_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_3_H,        IR2E71Y_BDIC_STR,    BDIC_REG_ALS_ADJ1_H_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_3,         IR2E71Y_BDIC_STR,    BDIC_REG_ALS_SHIFT_DEFAULT_A,  0xFF,   0},

     {BDIC_REG_OPT_TH_SHIFT_1_0,    IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_1_0_VAL,   0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_3_2,    IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_3_2_VAL,   0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_4_5,    IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_4_5_VAL,   0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_6_7,    IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_6_7_VAL,   0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_8_9,    IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_8_9_VAL,   0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_11_10,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_11_10_VAL, 0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_13_12,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_13_12_VAL, 0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_15_14,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_15_14_VAL, 0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_17_16,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_17_16_VAL, 0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_19_18,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_19_18_VAL, 0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_21_20,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_21_20_VAL, 0xFF, 0},
     {BDIC_REG_OPT_TH_SHIFT_23_22,  IR2E71Y_BDIC_STR,    BDIC_REG_OPT_TH_SHIFT_23_22_VAL, 0xFF, 0},

     {BDIC_REG_OPT0_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT0_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT0_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT0_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT0_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT0_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT0_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT0_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT1_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT1_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT1_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT1_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT1_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT1_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT1_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT1_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT2_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT2_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT2_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT2_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT2_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT2_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT2_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT2_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT3_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT3_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT3_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT3_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT3_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT3_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT3_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT3_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT4_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT4_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT4_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT4_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT4_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT4_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT4_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT4_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT5_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT5_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT5_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT5_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT5_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT5_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT5_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT5_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT6_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT6_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT6_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT6_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT6_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT6_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT6_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT6_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT7_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT7_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT7_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT7_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT7_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT7_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT7_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT7_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT8_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT8_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT8_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT8_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT8_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT8_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT8_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT8_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT9_LT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT9_LT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT9_LT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT9_LT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT9_HT_L,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT9_HT_L_VAL,     0xFF,      0},
     {BDIC_REG_OPT9_HT_H,           IR2E71Y_BDIC_STR,    BDIC_REG_OPT9_HT_H_VAL,     0xFF,      0},
     {BDIC_REG_OPT10_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT10_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT10_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT10_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT10_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT10_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT10_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT10_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT11_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT11_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT11_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT11_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT11_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT11_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT11_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT11_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT12_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT12_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT12_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT12_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT12_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT12_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT12_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT12_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT13_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT13_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT13_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT13_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT13_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT13_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT13_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT13_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT14_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT14_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT14_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT14_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT14_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT14_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT14_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT14_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT15_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT15_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT15_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT15_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT15_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT15_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT15_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT15_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT16_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT16_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT16_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT16_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT16_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT16_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT16_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT16_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT17_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT17_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT17_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT17_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT17_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT17_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT17_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT17_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT18_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT18_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT18_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT18_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT18_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT18_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT18_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT18_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT19_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT19_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT19_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT19_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT19_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT19_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT19_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT19_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT20_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT20_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT20_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT20_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT20_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT20_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT20_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT20_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT21_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT21_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT21_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT21_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT21_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT21_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT21_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT21_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT22_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT22_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT22_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT22_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT22_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT22_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT22_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT22_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT23_LT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT23_LT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT23_LT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT23_LT_H_VAL,    0xFF,      0},
     {BDIC_REG_OPT23_HT_L,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT23_HT_L_VAL,    0xFF,      0},
     {BDIC_REG_OPT23_HT_H,          IR2E71Y_BDIC_STR,    BDIC_REG_OPT23_HT_H_VAL,    0xFF,      0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_active[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_SET,    0x80,                       0x80,   1000},
     {BDIC_REG_SYSTEM3,             IR2E71Y_BDIC_SET,    0x02,                       0x02,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_standby[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM3,             IR2E71Y_BDIC_CLR,    0x00,                       0x02,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_CLR,    0x00,                       0x80,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_als_sensor_adjust[] = {
     {BDIC_REG_ALS_ADJ0_0_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_0_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_0_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_0_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_0,         IR2E71Y_BDIC_STR,   BDIC_REG_ALS_SHIFT_DEFAULT_B,  0xFF,   0},
     {BDIC_REG_ALS_ADJ0_1_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_1_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_1_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_1_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_1,         IR2E71Y_BDIC_STR,   BDIC_REG_ALS_SHIFT_DEFAULT_B,  0xFF,   0},
     {BDIC_REG_ALS_ADJ0_2_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_2_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_2_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_L_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_2_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_H_DEFAULT_B, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_2,         IR2E71Y_BDIC_STR,   BDIC_REG_ALS_SHIFT_DEFAULT_B,  0xFF,   0},
     {BDIC_REG_ALS_ADJ0_3_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_L_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_ADJ0_3_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ0_H_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_3_L,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_L_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_ADJ1_3_H,        IR2E71Y_BDIC_STR,   BDIC_REG_ALS_ADJ1_H_DEFAULT_A, 0xFF,   0},
     {BDIC_REG_ALS_SHIFT_3,         IR2E71Y_BDIC_STR,   BDIC_REG_ALS_SHIFT_DEFAULT_A,  0xFF,   0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_shutdown[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0}
};

#endif /* IR2E71Y_MAIN_CTRL_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */

