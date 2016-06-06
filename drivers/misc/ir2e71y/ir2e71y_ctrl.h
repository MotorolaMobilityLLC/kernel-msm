/* drivers/misc/ir2e71y/ir2e71y_ctrl.h  (Display Driver)
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

#ifndef IR2E71Y_CTRL_H
#define IR2E71Y_CTRL_H
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "ir2e71y_cmn.h"
#include "ir2e71y_data_default.h"

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_vsn_on_ts1[] = {
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_CLR,    0x00,                       0x20,      0},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_RMW,    0x10,                       0x50, 16 * 1000},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_SET,    0x20,                       0x20,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_vsn_on_ts2[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_RMW,    0x10,                       0x50,      20*1000}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_vsn_off[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_RMW,    0x00,                       0x50,   3000}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_vsp_on[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_DCDC2_TEST_57,       IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_SET,    0x01,                       0x01,   5000}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_vsp_off[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM2,             IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0},
     {BDIC_REG_DCDC2_TEST_57,       IR2E71Y_BDIC_STR,    0x00,                       0xFF,   1000}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_on[] = {
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_RMW,    0x01,                       0x55,      0},
     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_SET,    0x01,                       0x01,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_SET,    BDIC_REG_SYSTEM1_BKL,       0x03,   5000}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_auto_on[] = {
     {BDIC_REG_NONE,                IR2E71Y_BDIC_WAIT,   0x00,                       0x00, 22 * 1000},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0},
     {BDIC_REG_SLOPE,               IR2E71Y_BDIC_STR,    0xFF,                       0xFF,      0},
     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_fix_on[] = {
     {BDIC_REG_NONE,                IR2E71Y_BDIC_WAIT,   0x00,                       0x00, 25 * 1000},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0},
     {BDIC_REG_SLOPE,               IR2E71Y_BDIC_STR,    0x21,                       0xFF,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_fix[] = {
     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_SET,    0x01,                       0x01,      0},
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0},
     {BDIC_REG_SLOPE,               IR2E71Y_BDIC_STR,    0x21,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_auto[] = {
     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0},
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_CLR,    0x00,                       0x05,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_slope_fast[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SLOPE,               IR2E71Y_BDIC_STR,    0x21,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_slope_slow[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SLOPE,               IR2E71Y_BDIC_STR,    0xFF,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_opt_mode_off[] = {
     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_SET,    0x01,                       0x01,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_opt_mode_on[] = {
     {BDIC_REG_OPT_MODE,            IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_led_value[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_M1LED,               IR2E71Y_BDIC_STR,    BDIC_REG_M1LED_VAL,         0xFF,      0},
     {BDIC_REG_M2LED,               IR2E71Y_BDIC_STR,    BDIC_REG_M2LED_VAL,         0xFF,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_opt_value[] = {
     {BDIC_REG_OPT0,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT1,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT2,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT3,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT4,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT5,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT6,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT7,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT8,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT9,                IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT10,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT11,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT12,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT13,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT14,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT15,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT16,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT17,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT18,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT19,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT20,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT21,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT22,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0},
     {BDIC_REG_OPT23,               IR2E71Y_BDIC_STRM,    0x00,         0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_post_start[] = {
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_RMW,    0x01,                       0x05,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_bkl_off[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_CLR,    0x00,       BDIC_REG_SYSTEM1_BKL,  15000},
     {BDIC_REG_SYSTEM6,             IR2E71Y_BDIC_RMW,    0x01,                       0x05,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_sensor_power_on[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM4,             IR2E71Y_BDIC_SET,    0x02,                       0x02,   5000},
     {BDIC_REG_GIMR4,               IR2E71Y_BDIC_SET,    0x08,                       0x08,      0},
     {BDIC_REG_I2C_DATA6,           IR2E71Y_BDIC_STR,    0x0C,                       0xFF,      0},
     {BDIC_REG_I2C_SET,             IR2E71Y_BDIC_RMW,    0x50,                       0x70,      0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_sensor_power_off[] = {
     {BDIC_REG_SYSTEM4,             IR2E71Y_BDIC_CLR,    0x00,                       0x02,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_init_set_threshold[] = {
     {SENSOR_REG_PS_LT_LSB,         IR2E71Y_ALS_STRMS,   SENSOR_REG_PS_LT_LSB_VAL,   0xFF,       0},
     {SENSOR_REG_PS_LT_MSB,         IR2E71Y_ALS_STRM,    SENSOR_REG_PS_LT_MSB_VAL,   0xFF,       0},
     {SENSOR_REG_PS_HT_LSB,         IR2E71Y_ALS_STRM,    SENSOR_REG_PS_HT_LSB_VAL,   0xFF,       0},
     {SENSOR_REG_PS_HT_MSB,         IR2E71Y_ALS_STRM,    SENSOR_REG_PS_HT_MSB_VAL,   0xFF,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_psals_init[] = {
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STRMS,   0x00,                        0xFF,      0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_STRM,    SENSOR_REG_COMMAND2_INI_VAL, 0xFF,      0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STRM,    0x10,                        0xFF,      0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STRM,    0x08,                        0xFF,      0},
     {SENSOR_REG_INT_LT_LSB,        IR2E71Y_ALS_STRM,    0xFF,                        0xFF,      0},
     {SENSOR_REG_INT_LT_MSB,        IR2E71Y_ALS_STRM,    0xFF,                        0xFF,      0},
     {SENSOR_REG_INT_HT_LSB,        IR2E71Y_ALS_STRM,    0x00,                        0xFF,      0},
     {SENSOR_REG_INT_HT_MSB,        IR2E71Y_ALS_STRM,    0x00,                        0xFF,      0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x90,                        0xFF,      0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_init_als_off1[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STRMS,   0x00,                       0xFF,       0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_STRM,    0x00,                       0xFF,       0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STRM,    0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STRM,    0xEC,                       0xFF,       0}
};

#define ir2e71y_bdic_ps_init_als_off2    (ir2e71y_bdic_ps_init_set_threshold)

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_init_als_off3[] = {
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xE0,                       0xFF,       0},
     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xEC,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF, (35 * 1000)}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_deinit_als_off1[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,       0},
     {BDIC_REG_GIMR4,               IR2E71Y_BDIC_CLR,    0x00,                       0x08,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_deinit_als_off2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x80,                       0xFF,    1000},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x00,                       0xFF,       0},
     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_init_als_on2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x00,                       0xFF,       0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_RMW,     0x20,                       0xF8,       0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STRMS,   0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STRM,    0xEC,                       0xFF,       0}
};

#define ir2e71y_bdic_ps_init_als_on3     (ir2e71y_bdic_ps_init_set_threshold)

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_init_als_on4[] = {
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xCC,                           0xFF,       0},
     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xCC,                           0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                           0xFF,       0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x9E,                           0xFF,       0},

     {BDIC_REG_AR_HI_TH_L_L,        IR2E71Y_BDIC_STR,    0xFF,                           0xFF,       0},
     {BDIC_REG_AR_HI_TH_L_H,        IR2E71Y_BDIC_STR,    0x3F,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_L,        IR2E71Y_BDIC_STR,    0xEE,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_H,        IR2E71Y_BDIC_STR,    0x02,                           0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_L,        IR2E71Y_BDIC_STR,    0xFF,                           0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_H,        IR2E71Y_BDIC_STR,    0x3F,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_L,        IR2E71Y_BDIC_STR,    0xB8,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_H,        IR2E71Y_BDIC_STR,    0x0B,                           0xFF,       0},

     {BDIC_REG_AR_RANGE_L,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_L_VAL_PS_ON,  0xFF,       0},
     {BDIC_REG_AR_RANGE_M,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_M_VAL_PS_ON,  0xFF,       0},
     {BDIC_REG_AR_RANGE_H,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_H_VAL_PS_ON,  0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                           0xFF, (35 * 1000)}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_ps_deinit_als_on2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,       0},

     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x00,                       0xFF,       0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_RMW,     0x18,                       0xF8,       0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STRMS,   0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STRM,    0x08,                       0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xD0,                       0xFF,       0},

     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xD0,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x9E,                       0xFF,       0},

     {BDIC_REG_AR_HI_TH_L_L,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_HI_TH_L_H,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_L,        IR2E71Y_BDIC_STR,    0xB8,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_H,        IR2E71Y_BDIC_STR,    0x0B,                       0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_L,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_H,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_L,        IR2E71Y_BDIC_STR,    0xE0,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_H,        IR2E71Y_BDIC_STR,    0x2E,                       0xFF,       0},

     {BDIC_REG_AR_RANGE_L,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_L_VAL,    0xFF,       0},
     {BDIC_REG_AR_RANGE_M,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_M_VAL,    0xFF,       0},
     {BDIC_REG_AR_RANGE_H,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_H_VAL,    0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_als_init_ps_off2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                           0xFF,   0},

     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x00,                           0xFF,   0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_STR,     SENSOR_REG_COMMAND2_ALS_ON_VAL, 0xFF,   0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STRMS,   0x10,                           0xFF,   0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STRM,    0x08,                           0xFF,   0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xD0,                           0xFF,   0},

     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xD0,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x9E,                       0xFF,       0},

     {BDIC_REG_AR_HI_TH_L_L,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_HI_TH_L_H,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_L,        IR2E71Y_BDIC_STR,    0xB8,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_H,        IR2E71Y_BDIC_STR,    0x0B,                       0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_L,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_H,        IR2E71Y_BDIC_STR,    0xFF,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_L,        IR2E71Y_BDIC_STR,    0xE0,                       0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_H,        IR2E71Y_BDIC_STR,    0x2E,                       0xFF,       0},

     {BDIC_REG_AR_RANGE_L,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_L_VAL,    0xFF,       0},
     {BDIC_REG_AR_RANGE_M,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_M_VAL,    0xFF,       0},
     {BDIC_REG_AR_RANGE_H,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_H_VAL,    0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_SET,    0x04,                       0x04,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_als_init_ps_on2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                           0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x0C,                           0xFF,       0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_RMW,     0x20,                           0xF8,       0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STR,     0x10,                           0xFF,       0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STR,     0xEC,                           0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xCC,                           0xFF,       0},

     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xCC,                           0xFF,       0},
     {BDIC_REG_SYSTEM8  ,           IR2E71Y_BDIC_STR,    0x00,                           0xFF,       0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x9E,                           0xFF,       0},

     {BDIC_REG_AR_HI_TH_L_L,        IR2E71Y_BDIC_STR,    0xFF,                           0xFF,       0},
     {BDIC_REG_AR_HI_TH_L_H,        IR2E71Y_BDIC_STR,    0x3F,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_L,        IR2E71Y_BDIC_STR,    0xEE,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_M_H,        IR2E71Y_BDIC_STR,    0x02,                           0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_L,        IR2E71Y_BDIC_STR,    0xFF,                           0xFF,       0},
     {BDIC_REG_AR_HI_TH_M_H,        IR2E71Y_BDIC_STR,    0x3F,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_L,        IR2E71Y_BDIC_STR,    0xB8,                           0xFF,       0},
     {BDIC_REG_AR_LO_TH_H_H,        IR2E71Y_BDIC_STR,    0x0B,                           0xFF,       0},

     {BDIC_REG_AR_RANGE_L,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_L_VAL_PS_ON,  0xFF,       0},
     {BDIC_REG_AR_RANGE_M,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_M_VAL_PS_ON,  0xFF,       0},
     {BDIC_REG_AR_RANGE_H,          IR2E71Y_BDIC_STRM,   BDIC_REG_AR_RANGE_H_VAL_PS_ON,  0xFF,       0},
     {BDIC_REG_NONE,                IR2E71Y_BDIC_WAIT,   0x00,                           0x00,   45000},
     {BDIC_REG_SYSTEM8  ,           IR2E71Y_BDIC_STR,    0x04,                           0xFF,       0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_set_als_shift_ps_off[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x01,                       0x00,       0},
     {BDIC_REG_ALS_SHIFT_0,         IR2E71Y_BDIC_STR,    0x03,                       0xFF,       0},
     {BDIC_REG_ALS_SHIFT_1,         IR2E71Y_BDIC_STR,    0x03,                       0xFF,       0},
     {BDIC_REG_ALS_SHIFT_2,         IR2E71Y_BDIC_STR,    0x03,                       0xFF,       0},
     {BDIC_REG_ALS_SHIFT_3,         IR2E71Y_BDIC_STR,    0x03,                       0xFF,       0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,       0},
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_set_als_shift_ps_on[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x01,                       0x00,       0},
     {BDIC_REG_ALS_SHIFT_0,         IR2E71Y_BDIC_STR,    0x05,                       0xFF,       0},
     {BDIC_REG_ALS_SHIFT_1,         IR2E71Y_BDIC_STR,    0x05,                       0xFF,       0},
     {BDIC_REG_ALS_SHIFT_2,         IR2E71Y_BDIC_STR,    0x05,                       0xFF,       0},
     {BDIC_REG_ALS_SHIFT_3,         IR2E71Y_BDIC_STR,    0x05,                       0xFF,       0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,       0}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_als_auto_light_bdic[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_SET,    0x04,                       0x04,       0}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_als_auto_light_bdic_wait[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_SET,    0x04,                       0x04,       0},
     {BDIC_REG_NONE,                IR2E71Y_BDIC_WAIT,   0x00,                       0x00,   35000}
};

#define ir2e71y_bdic_als_deinit_ps_off1   (ir2e71y_bdic_ps_deinit_als_off1)
#define ir2e71y_bdic_als_deinit_ps_off2   (ir2e71y_bdic_ps_deinit_als_off2)

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_als_deinit_ps_on2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STRMS,   0x0C,                       0xFF,       0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_STRM,    0x00,                       0xFF,       0},
     {SENSOR_REG_COMMAND3,          IR2E71Y_ALS_STRM,    0x10,                       0xFF,       0},
     {SENSOR_REG_COMMAND4,          IR2E71Y_ALS_STRM,    0xEC,                       0xFF,       0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xEC,                       0xFF,       0},
     {BDIC_REG_AR_DATA6,            IR2E71Y_BDIC_STR,    0xEC,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,       0}
};


static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_i2ctimer_stop[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x08,                       0xFF,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_i2ctimer_start[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,       0}
};

static const ir2e71y_bdicRegSetting_t ir2e71y_bdic_dcdc1_err[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,       0},
     {BDIC_REG_SYSTEM1,             IR2E71Y_BDIC_CLR,    0x00,                       0x03,       0}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_reg_ar_ctrl[] = {
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x9A,                       0xFF,       0}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_i2c_throughmode_on[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,       0}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_i2c_throughmode_off[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,       0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,       0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_restore_regs_for_ave_ado_bdic[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_DATA_SHIFT_ML,       IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x01,                       0x00,      0},
     {BDIC_REG_OPT_TH_SHIFT_1_0,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_3_2,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_4_5,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_6_7,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_8_9,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_11_10,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_13_12,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_15_14,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_17_16,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_19_18,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_21_20,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_23_22,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_restore_regs_for_ave_ado_als[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,      0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_STR,     0x00,                       0xFF,      0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_ave_ado_param1[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x08,                       0xFF,   5000},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0x5E,                       0xFF,      0},
     {BDIC_REG_DATA_SHIFT_ML,       IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x01,                       0x00,      0},
     {BDIC_REG_OPT_TH_SHIFT_1_0,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_3_2,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_4_5,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_6_7,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_8_9,    IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_11_10,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_13_12,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_15_14,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_17_16,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_19_18,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_21_20,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_OPT_TH_SHIFT_23_22,  IR2E71Y_BDIC_STRM,   0x00,                       0xFF,      0},
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_STR,    0xDA,                       0xFF,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_ave_ado_param2[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x10,                       0xFF,      0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0x50,                       0xFF,      0},
     {SENSOR_REG_COMMAND2,          IR2E71Y_ALS_STR,     0x18,                       0xFF,      0},
     {SENSOR_REG_COMMAND1,          IR2E71Y_ALS_STR,     0xD0,                       0xFF,      0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_NONE,                IR2E71Y_BDIC_WAIT,   0x00,                       0x00, 100000}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_i2ctimer_stop_ave_ado[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x08,                       0xFF,   5000}
};

static ir2e71y_bdicRegSetting_t const ir2e71y_bdic_i2ctimer_start_ave_ado[] = {
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,   1000}
};

#if defined(CONFIG_ANDROID_ENGINEERING)
static ir2e71y_bdicRegSetting_t ir2e71y_bdic_restore_regs_for_bank1_dump[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,   1000}
};
#endif /* CONFIG_ANDROID_ENGINEERING */

#ifdef IR2E71Y_ALS_INT
static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_als_int_setting[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_OPT_INT1,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_OPT_INT2,            IR2E71Y_BDIC_STR,    0x00,                       0xFF,      0},
     {BDIC_REG_GIMR2,               IR2E71Y_BDIC_SET,    0x30,                       0x30,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_als_int_clear1[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_GIMR2,               IR2E71Y_BDIC_CLR,    0x00,                       0x30,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_als_int_clear2[] = {
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_CLR,    0x00,                       0x04,      0},
     {BDIC_REG_SYSTEM8,             IR2E71Y_BDIC_STR,    0x04,                       0xFF,   3000},
     {BDIC_REG_AR_CTRL,             IR2E71Y_BDIC_SET,    0x04,                       0x04,      0}
};
#endif /* IR2E71Y_ALS_INT */

#ifdef IR2E71Y_LED_INT
static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_als_req_on[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_ALS_INT,             IR2E71Y_BDIC_RMW,    BDIC_REG_INT_ALS_SEL_VAL,   0x1F,      0},
     {BDIC_REG_GIMR3,               IR2E71Y_BDIC_SET,    0x01,                       0x01,      0},
     {BDIC_REG_GIMF3,               IR2E71Y_BDIC_SET,    0x01,                       0x01,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_als_req_off[] = {
     {BDIC_REG_BANKSEL,             IR2E71Y_BDIC_BANK,   0x00,                       0x00,      0},
     {BDIC_REG_GIMR3,               IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0},
     {BDIC_REG_GIMF3,               IR2E71Y_BDIC_CLR,    0x00,                       0x01,      0}
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_led_cur_low[] = {
     {BDIC_REG_CH_CUR_SEL,          IR2E71Y_BDIC_RMW,    0x3F,                       0x3F,      0},
     {BDIC_REG_CH_CUR_SEL2,         IR2E71Y_BDIC_RMW,    0x3F,                       0x3F,      0},
};

static ir2e71y_bdicRegSetting_t ir2e71y_bdic_reg_led_cur_norm[] = {
     {BDIC_REG_CH_CUR_SEL,          IR2E71Y_BDIC_RMW,    0x00,                       0x3F,      0},
     {BDIC_REG_CH_CUR_SEL2,         IR2E71Y_BDIC_RMW,    0x00,                       0x3F,      0},
};
#endif /* IR2E71Y_LED_INT */
#endif /* IR2E71Y_CTRL_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
