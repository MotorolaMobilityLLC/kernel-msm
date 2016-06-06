/* drivers/misc/ir2e71y/ir2e71y_data_default.h  (Display Driver)
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

#ifndef IR2E71Y_DATA_DEFAULT_H
#define IR2E71Y_DATA_DEFAULT_H

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_BKL_FIX_TBL_NUM                  (256)
#define IR2E71Y_BKL_AUTO_STEP_NUM                (253)
#define IR2E71Y_BKL_AUTO_OPT_TBL_NUM             (24)
#define IR2E71Y_BKL_EMERGENCY_LIMIT_AUTO_LEVEL0  (595)
#define IR2E71Y_BKL_EMERGENCY_LIMIT_AUTO_LEVEL1  (99)
#define IR2E71Y_BKL_EMERGENCY_LIMIT_FIX_LEVEL0   (0x7D)
#define IR2E71Y_BKL_EMERGENCY_LIMIT_FIX_LEVEL1   (0x15)
#define IR2E71Y_BKL_PWM_LOWER_LIMIT              (0x000F)
#define IR2E71Y_BKL_PWM_LOWER_LIMIT_ZERO         (0x000F)
#define IR2E71Y_BKL_PWM_UPPER_LIMIT              (0x03E8)
#define IR2E71Y_BKL_CURRENT_LOWER_LIMIT          (0)
#define IR2E71Y_BKL_CURRENT_UPPER_LIMIT          (210)

#define IR2E71Y_INT_ENABLE_GFAC                  (IR2E71Y_BDIC_INT_GFAC_ALS | \
                                                 IR2E71Y_BDIC_INT_GFAC_DET | \
                                                 IR2E71Y_BDIC_INT_GFAC_I2C_ERR | \
                                                 IR2E71Y_BDIC_INT_GFAC_OPTSEL  | \
                                                 IR2E71Y_BDIC_INT_GFAC_ALS_TRG1 | \
                                                 IR2E71Y_BDIC_INT_GFAC_ALS_TRG2)

/* ------------------------------------------------------------------------- */
/* MACROS(Register Value)                                                    */
/* ------------------------------------------------------------------------- */
#define BDIC_REG_VO2_PFML_VAL                   (0x19)
#define BDIC_REG_VO2_PWML_VAL                   (0x19)
#define BDIC_REG_VO2_PWMH_VAL                   (0x19)
#define BDIC_REG_M1LED_VAL                      (0x5C)
#define BDIC_REG_M2LED_VAL                      (0x5C)
#define BDIC_REG_SYSTEM1_BKL                    (0x03)
#define BDIC_REG_DCDC1_CLIMT_2_VAL              (0xC0)
#define BDIC_REG_DCDC1_GM_VAL                   (0x28)
#define BDIC_REG_DCDC1_OSC_2_VAL                (0x11)

#define BDIC_REG_AR_HI_TH_L_L_VAL               (0xFF)
#define BDIC_REG_AR_HI_TH_L_H_VAL               (0xFF)
#define BDIC_REG_AR_LO_TH_M_L_VAL               (0xB8)
#define BDIC_REG_AR_LO_TH_M_H_VAL               (0x0B)
#define BDIC_REG_AR_HI_TH_M_L_VAL               (0xFF)
#define BDIC_REG_AR_HI_TH_M_H_VAL               (0xFF)
#define BDIC_REG_AR_LO_TH_H_L_VAL               (0xE0)
#define BDIC_REG_AR_LO_TH_H_H_VAL               (0x2E)
#define BDIC_REG_AR_RANGE_L_VAL                 (0x18)
#define BDIC_REG_AR_RANGE_M_VAL                 (0x1C)
#define BDIC_REG_AR_RANGE_H_VAL                 (0x1E)
#define BDIC_REG_AR_RANGE_L_VAL_PS_ON           (0x20)
#define BDIC_REG_AR_RANGE_M_VAL_PS_ON           (0x24)
#define BDIC_REG_AR_RANGE_H_VAL_PS_ON           (0x26)
#define BDIC_REG_DATA_SHIFT_M_VAL               (0x40)
#define BDIC_REG_DATA_SHIFT_H_VAL               (0x06)

#define BDIC_REG_AR_CTRL_VAL                    (0xDE)
#define BDIC_REG_GPIO_2_VAL                     (0x00)

#define BDIC_REG_ALS_ADJ0_L_DEFAULT_A           (0x5C)
#define BDIC_REG_ALS_ADJ0_H_DEFAULT_A           (0x3F)
#define BDIC_REG_ALS_ADJ1_L_DEFAULT_A           (0x5C)
#define BDIC_REG_ALS_ADJ1_H_DEFAULT_A           (0x3F)
#define BDIC_REG_ALS_SHIFT_DEFAULT_A            (0x03)
#define BDIC_REG_CLEAR_OFFSET_DEFAULT_A         (0x00)
#define BDIC_REG_IR_OFFSET_DEFAULT_A            (0x00)

#define BDIC_REG_ALS_ADJ0_L_DEFAULT_B           (0xAC)
#define BDIC_REG_ALS_ADJ0_H_DEFAULT_B           (0x0C)
#define BDIC_REG_ALS_ADJ1_L_DEFAULT_B           (0x00)
#define BDIC_REG_ALS_ADJ1_H_DEFAULT_B           (0x00)
#define BDIC_REG_ALS_SHIFT_DEFAULT_B            (0x03)
#define BDIC_REG_CLEAR_OFFSET_DEFAULT_B         (0x00)
#define BDIC_REG_IR_OFFSET_DEFAULT_B            (0x00)

#define BDIC_REG_OPT_TH_SHIFT_1_0_VAL           (0x00)
#define BDIC_REG_OPT_TH_SHIFT_3_2_VAL           (0x00)
#define BDIC_REG_OPT_TH_SHIFT_4_5_VAL           (0x00)
#define BDIC_REG_OPT_TH_SHIFT_6_7_VAL           (0x00)
#define BDIC_REG_OPT_TH_SHIFT_8_9_VAL           (0x00)
#define BDIC_REG_OPT_TH_SHIFT_11_10_VAL         (0x00)
#define BDIC_REG_OPT_TH_SHIFT_13_12_VAL         (0x00)
#define BDIC_REG_OPT_TH_SHIFT_15_14_VAL         (0x00)
#define BDIC_REG_OPT_TH_SHIFT_17_16_VAL         (0x00)
#define BDIC_REG_OPT_TH_SHIFT_19_18_VAL         (0x44)
#define BDIC_REG_OPT_TH_SHIFT_21_20_VAL         (0x44)
#define BDIC_REG_OPT_TH_SHIFT_23_22_VAL         (0x44)

#define BDIC_REG_OPT0_LT_L_VAL                  (0x00)
#define BDIC_REG_OPT0_LT_H_VAL                  (0x00)
#define BDIC_REG_OPT0_HT_L_VAL                  (0x0B)
#define BDIC_REG_OPT0_HT_H_VAL                  (0x00)
#define BDIC_REG_OPT1_LT_L_VAL                  (0x05)
#define BDIC_REG_OPT1_LT_H_VAL                  (0x00)
#define BDIC_REG_OPT1_HT_L_VAL                  (0x25)
#define BDIC_REG_OPT1_HT_H_VAL                  (0x00)
#define BDIC_REG_OPT2_LT_L_VAL                  (0x1B)
#define BDIC_REG_OPT2_LT_H_VAL                  (0x00)
#define BDIC_REG_OPT2_HT_L_VAL                  (0x45)
#define BDIC_REG_OPT2_HT_H_VAL                  (0x00)
#define BDIC_REG_OPT3_LT_L_VAL                  (0x3B)
#define BDIC_REG_OPT3_LT_H_VAL                  (0x00)
#define BDIC_REG_OPT3_HT_L_VAL                  (0x7B)
#define BDIC_REG_OPT3_HT_H_VAL                  (0x00)
#define BDIC_REG_OPT4_LT_L_VAL                  (0x65)
#define BDIC_REG_OPT4_LT_H_VAL                  (0x00)
#define BDIC_REG_OPT4_HT_L_VAL                  (0xC5)
#define BDIC_REG_OPT4_HT_H_VAL                  (0x00)
#define BDIC_REG_OPT5_LT_L_VAL                  (0xAB)
#define BDIC_REG_OPT5_LT_H_VAL                  (0x00)
#define BDIC_REG_OPT5_HT_L_VAL                  (0x35)
#define BDIC_REG_OPT5_HT_H_VAL                  (0x01)
#define BDIC_REG_OPT6_LT_L_VAL                  (0x0B)
#define BDIC_REG_OPT6_LT_H_VAL                  (0x01)
#define BDIC_REG_OPT6_HT_L_VAL                  (0xF5)
#define BDIC_REG_OPT6_HT_H_VAL                  (0x01)
#define BDIC_REG_OPT7_LT_L_VAL                  (0xAB)
#define BDIC_REG_OPT7_LT_H_VAL                  (0x01)
#define BDIC_REG_OPT7_HT_L_VAL                  (0xEB)
#define BDIC_REG_OPT7_HT_H_VAL                  (0x02)
#define BDIC_REG_OPT8_LT_L_VAL                  (0x95)
#define BDIC_REG_OPT8_LT_H_VAL                  (0x02)
#define BDIC_REG_OPT8_HT_L_VAL                  (0xA0)
#define BDIC_REG_OPT8_HT_H_VAL                  (0x04)
#define BDIC_REG_OPT9_LT_L_VAL                  (0xF0)
#define BDIC_REG_OPT9_LT_H_VAL                  (0x03)
#define BDIC_REG_OPT9_HT_L_VAL                  (0x65)
#define BDIC_REG_OPT9_HT_H_VAL                  (0x08)
#define BDIC_REG_OPT10_LT_L_VAL                 (0xDB)
#define BDIC_REG_OPT10_LT_H_VAL                 (0x06)
#define BDIC_REG_OPT10_HT_L_VAL                 (0x45)
#define BDIC_REG_OPT10_HT_H_VAL                 (0x0D)
#define BDIC_REG_OPT11_LT_L_VAL                 (0x9B)
#define BDIC_REG_OPT11_LT_H_VAL                 (0x0B)
#define BDIC_REG_OPT11_HT_L_VAL                 (0x65)
#define BDIC_REG_OPT11_HT_H_VAL                 (0x14)
#define BDIC_REG_OPT12_LT_L_VAL                 (0xAB)
#define BDIC_REG_OPT12_LT_H_VAL                 (0x11)
#define BDIC_REG_OPT12_HT_L_VAL                 (0x0B)
#define BDIC_REG_OPT12_HT_H_VAL                 (0x21)
#define BDIC_REG_OPT13_LT_L_VAL                 (0x15)
#define BDIC_REG_OPT13_LT_H_VAL                 (0x1C)
#define BDIC_REG_OPT13_HT_L_VAL                 (0xEB)
#define BDIC_REG_OPT13_HT_H_VAL                 (0x2F)
#define BDIC_REG_OPT14_LT_L_VAL                 (0xF5)
#define BDIC_REG_OPT14_LT_H_VAL                 (0x2A)
#define BDIC_REG_OPT14_HT_L_VAL                 (0x80)
#define BDIC_REG_OPT14_HT_H_VAL                 (0x48)
#define BDIC_REG_OPT15_LT_L_VAL                 (0xB0)
#define BDIC_REG_OPT15_LT_H_VAL                 (0x3E)
#define BDIC_REG_OPT15_HT_L_VAL                 (0x85)
#define BDIC_REG_OPT15_HT_H_VAL                 (0x72)
#define BDIC_REG_OPT16_LT_L_VAL                 (0x6B)
#define BDIC_REG_OPT16_LT_H_VAL                 (0x62)
#define BDIC_REG_OPT16_HT_L_VAL                 (0x95)
#define BDIC_REG_OPT16_HT_H_VAL                 (0x9D)
#define BDIC_REG_OPT17_LT_L_VAL                 (0x1B)
#define BDIC_REG_OPT17_LT_H_VAL                 (0x90)
#define BDIC_REG_OPT17_HT_L_VAL                 (0x25)
#define BDIC_REG_OPT17_HT_H_VAL                 (0xDC)
#define BDIC_REG_OPT18_LT_L_VAL                 (0x39)
#define BDIC_REG_OPT18_LT_H_VAL                 (0x0C)
#define BDIC_REG_OPT18_HT_L_VAL                 (0x69)
#define BDIC_REG_OPT18_HT_H_VAL                 (0x14)
#define BDIC_REG_OPT19_LT_L_VAL                 (0xDA)
#define BDIC_REG_OPT19_LT_H_VAL                 (0x11)
#define BDIC_REG_OPT19_HT_L_VAL                 (0x16)
#define BDIC_REG_OPT19_HT_H_VAL                 (0x1C)
#define BDIC_REG_OPT20_LT_L_VAL                 (0x87)
#define BDIC_REG_OPT20_LT_H_VAL                 (0x19)
#define BDIC_REG_OPT20_HT_L_VAL                 (0xE3)
#define BDIC_REG_OPT20_HT_H_VAL                 (0x28)
#define BDIC_REG_OPT21_LT_L_VAL                 (0xC4)
#define BDIC_REG_OPT21_LT_H_VAL                 (0x23)
#define BDIC_REG_OPT21_HT_L_VAL                 (0x7C)
#define BDIC_REG_OPT21_HT_H_VAL                 (0x42)
#define BDIC_REG_OPT22_LT_L_VAL                 (0x3F)
#define BDIC_REG_OPT22_LT_H_VAL                 (0x38)
#define BDIC_REG_OPT22_HT_L_VAL                 (0x9F)
#define BDIC_REG_OPT22_HT_H_VAL                 (0x5C)
#define BDIC_REG_OPT23_LT_L_VAL                 (0xAC)
#define BDIC_REG_OPT23_LT_H_VAL                 (0x54)
#define BDIC_REG_OPT23_HT_L_VAL                 (0xFF)
#define BDIC_REG_OPT23_HT_H_VAL                 (0xFF)

#define SENSOR_REG_COMMAND2_INI_VAL             (0x28)
#define SENSOR_REG_COMMAND2_ALS_ON_VAL          (0x18)

#define SENSOR_REG_PS_LT_LSB_VAL                (0xFF)
#define SENSOR_REG_PS_LT_MSB_VAL                (0xFF)
#define SENSOR_REG_PS_HT_LSB_VAL                (0xFF)
#define SENSOR_REG_PS_HT_MSB_VAL                (0xFF)

#ifdef IR2E71Y_LED_INT
#define BDIC_REG_INT_ALS_SEL_VAL                (0x05)
#endif /* IR2E71Y_LED_INT */

#define BDIC_WAIT_TIMER_START                   (1 * 1000)
#define BDIC_WAIT_TIMER_STOP                    (3 * 1000)

#if defined(USE_LINUX) || defined(IR2E71Y_APPSBL)
/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static const unsigned char ir2e71y_main_bkl_tbl[IR2E71Y_BKL_FIX_TBL_NUM] = {
     0x00
    ,0x04
    ,0x04
    ,0x04
    ,0x04
    ,0x04
    ,0x04
    ,0x04
    ,0x04
    ,0x05
    ,0x05
    ,0x05
    ,0x05
    ,0x05
    ,0x05
    ,0x06
    ,0x06
    ,0x06
    ,0x06
    ,0x06
    ,0x06
    ,0x07
    ,0x07
    ,0x07
    ,0x07
    ,0x07
    ,0x08
    ,0x08
    ,0x08
    ,0x08
    ,0x08
    ,0x09
    ,0x09
    ,0x09
    ,0x09
    ,0x09
    ,0x0A
    ,0x0A
    ,0x0A
    ,0x0A
    ,0x0B
    ,0x0B
    ,0x0B
    ,0x0B
    ,0x0B
    ,0x0C
    ,0x0C
    ,0x0C
    ,0x0C
    ,0x0D
    ,0x0D
    ,0x0D
    ,0x0D
    ,0x0E
    ,0x0E
    ,0x0E
    ,0x0E
    ,0x0F
    ,0x0F
    ,0x0F
    ,0x0F
    ,0x10
    ,0x10
    ,0x10
    ,0x10
    ,0x11
    ,0x11
    ,0x11
    ,0x12
    ,0x12
    ,0x12
    ,0x12
    ,0x13
    ,0x13
    ,0x13
    ,0x14
    ,0x14
    ,0x14
    ,0x15
    ,0x15
    ,0x15
    ,0x15
    ,0x16
    ,0x16
    ,0x16
    ,0x17
    ,0x17
    ,0x17
    ,0x18
    ,0x18
    ,0x19
    ,0x19
    ,0x19
    ,0x1A
    ,0x1A
    ,0x1A
    ,0x1B
    ,0x1B
    ,0x1B
    ,0x1C
    ,0x1C
    ,0x1D
    ,0x1D
    ,0x1D
    ,0x1E
    ,0x1E
    ,0x1F
    ,0x1F
    ,0x1F
    ,0x20
    ,0x20
    ,0x21
    ,0x21
    ,0x21
    ,0x22
    ,0x22
    ,0x23
    ,0x23
    ,0x24
    ,0x24
    ,0x25
    ,0x25
    ,0x26
    ,0x26
    ,0x26
    ,0x27
    ,0x27
    ,0x28
    ,0x28
    ,0x29
    ,0x29
    ,0x2A
    ,0x2A
    ,0x2B
    ,0x2B
    ,0x2C
    ,0x2C
    ,0x2D
    ,0x2D
    ,0x2E
    ,0x2F
    ,0x2F
    ,0x30
    ,0x30
    ,0x31
    ,0x31
    ,0x32
    ,0x33
    ,0x33
    ,0x34
    ,0x34
    ,0x35
    ,0x36
    ,0x36
    ,0x37
    ,0x38
    ,0x38
    ,0x39
    ,0x3A
    ,0x3A
    ,0x3B
    ,0x3C
    ,0x3D
    ,0x3D
    ,0x3E
    ,0x3F
    ,0x40
    ,0x41
    ,0x41
    ,0x42
    ,0x43
    ,0x44
    ,0x45
    ,0x46
    ,0x47
    ,0x48
    ,0x49
    ,0x4A
    ,0x4B
    ,0x4C
    ,0x4D
    ,0x4E
    ,0x4F
    ,0x50
    ,0x51
    ,0x52
    ,0x53
    ,0x54
    ,0x55
    ,0x57
    ,0x58
    ,0x59
    ,0x5A
    ,0x5C
    ,0x5D
    ,0x5E
    ,0x60
    ,0x61
    ,0x62
    ,0x64
    ,0x65
    ,0x66
    ,0x67
    ,0x6A
    ,0x6B
    ,0x6D
    ,0x6E
    ,0x70
    ,0x71
    ,0x73
    ,0x75
    ,0x76
    ,0x78
    ,0x7A
    ,0x7C
    ,0x7D
    ,0x7F
    ,0x81
    ,0x83
    ,0x85
    ,0x87
    ,0x89
    ,0x8A
    ,0x8C
    ,0x8E
    ,0x90
    ,0x92
    ,0x94
    ,0x96
    ,0x98
    ,0x9A
    ,0x9C
    ,0x9F
    ,0xA1
    ,0xA3
    ,0xA5
    ,0xA7
    ,0xA9
    ,0xAB
    ,0xAE
    ,0xB0
    ,0xB2
    ,0xB4
    ,0xB7
    ,0xB9
    ,0xBB
    ,0xBD
    ,0xC0
    ,0xC2
    ,0xC4
    ,0xC7
    ,0xC9
    ,0xCB
    ,0xCE
    ,0xD0
    ,0xD2
};

static const unsigned short ir2e71y_main_bkl_min_tbl[IR2E71Y_BKL_AUTO_OPT_TBL_NUM] = {
    6
   ,6
   ,11
   ,13
   ,13
   ,14
   ,17
   ,19
   ,21
   ,24
   ,30
   ,33
   ,37
   ,41
   ,44
   ,49
   ,54
   ,58
   ,62
   ,73
   ,95
   ,151
   ,297
   ,461
};

static const unsigned short ir2e71y_main_bkl_max_tbl[IR2E71Y_BKL_AUTO_OPT_TBL_NUM] = {
    59
   ,59
   ,78
   ,104
   ,158
   ,205
   ,269
   ,341
   ,388
   ,477
   ,630
   ,721
   ,831
   ,972
   ,1074
   ,1225
   ,1408
   ,1540
   ,1699
   ,1871
   ,1964
   ,2112
   ,2370
   ,2505
};

static unsigned short const ir2e71y_bdic_bkl_ado_index[IR2E71Y_BKL_AUTO_OPT_TBL_NUM] = {
    ((BDIC_REG_OPT0_HT_H_VAL << 8)  | BDIC_REG_OPT0_HT_L_VAL),
    ((BDIC_REG_OPT1_HT_H_VAL << 8)  | BDIC_REG_OPT1_HT_L_VAL),
    ((BDIC_REG_OPT2_HT_H_VAL << 8)  | BDIC_REG_OPT2_HT_L_VAL),
    ((BDIC_REG_OPT3_HT_H_VAL << 8)  | BDIC_REG_OPT3_HT_L_VAL),
    ((BDIC_REG_OPT4_HT_H_VAL << 8)  | BDIC_REG_OPT4_HT_L_VAL),
    ((BDIC_REG_OPT5_HT_H_VAL << 8)  | BDIC_REG_OPT5_HT_L_VAL),
    ((BDIC_REG_OPT6_HT_H_VAL << 8)  | BDIC_REG_OPT6_HT_L_VAL),
    ((BDIC_REG_OPT7_HT_H_VAL << 8)  | BDIC_REG_OPT7_HT_L_VAL),
    ((BDIC_REG_OPT8_HT_H_VAL << 8)  | BDIC_REG_OPT8_HT_L_VAL),
    ((BDIC_REG_OPT9_HT_H_VAL << 8)  | BDIC_REG_OPT9_HT_L_VAL),
    ((BDIC_REG_OPT10_HT_H_VAL << 8) | BDIC_REG_OPT10_HT_L_VAL),
    ((BDIC_REG_OPT11_HT_H_VAL << 8) | BDIC_REG_OPT11_HT_L_VAL),
    ((BDIC_REG_OPT12_HT_H_VAL << 8) | BDIC_REG_OPT12_HT_L_VAL),
    ((BDIC_REG_OPT13_HT_H_VAL << 8) | BDIC_REG_OPT13_HT_L_VAL),
    ((BDIC_REG_OPT14_HT_H_VAL << 8) | BDIC_REG_OPT14_HT_L_VAL),
    ((BDIC_REG_OPT15_HT_H_VAL << 8) | BDIC_REG_OPT15_HT_L_VAL),
    ((BDIC_REG_OPT16_HT_H_VAL << 8) | BDIC_REG_OPT16_HT_L_VAL),
    ((BDIC_REG_OPT17_HT_H_VAL << 8) | BDIC_REG_OPT17_HT_L_VAL),
    ((BDIC_REG_OPT18_HT_H_VAL << 8) | BDIC_REG_OPT18_HT_L_VAL),
    ((BDIC_REG_OPT19_HT_H_VAL << 8) | BDIC_REG_OPT19_HT_L_VAL),
    ((BDIC_REG_OPT20_HT_H_VAL << 8) | BDIC_REG_OPT20_HT_L_VAL),
    ((BDIC_REG_OPT21_HT_H_VAL << 8) | BDIC_REG_OPT21_HT_L_VAL),
    ((BDIC_REG_OPT22_HT_H_VAL << 8) | BDIC_REG_OPT22_HT_L_VAL),
    ((BDIC_REG_OPT23_HT_H_VAL << 8) | BDIC_REG_OPT23_HT_L_VAL)
};

static const struct ir2e71y_bdic_bkl_ado_tbl ir2e71y_bdic_bkl_ado_tbl[5] = {
    {    0,      1,   756,      65},
    {    1,     22,   448,     596},
    {   22,    370,   267,    5896},
    {  370,   3915,   256,    -980},
    { 3915,  65536,   254,    3642}
};
#endif /* defined(USE_LINUX) || defined(IR2E71Y_APPSBL) */

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */

#endif /* IR2E71Y_IR2E71Y_DATA_DEFAULT_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
