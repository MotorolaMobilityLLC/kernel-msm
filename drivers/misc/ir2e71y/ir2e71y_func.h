/* drivers/misc/ir2e71y/ir2e71y_func.h  (Display Driver)
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

#ifndef IR2E71Y_FUNC_H
#define IR2E71Y_FUNC_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#ifdef IR2E71Y_APPSBL
#include "ir2e71y_aboot_priv.h"
#endif /* IR2E71Y_APPSBL */

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_BDIC_GPIO_COG_RESET          (0)

#define IR2E71Y_BDIC_GPIO_LOW                (0)
#define IR2E71Y_BDIC_GPIO_HIGH               (1)

#define IR2E71Y_BDIC_GPIO_GPOD0              (0)
#define IR2E71Y_BDIC_GPIO_GPOD1              (1)
#define IR2E71Y_BDIC_GPIO_GPOD2              (2)
#define IR2E71Y_BDIC_GPIO_GPOD3              (3)
#define IR2E71Y_BDIC_GPIO_GPOD4              (4)
#define IR2E71Y_BDIC_GPIO_GPOD5              (5)

#define IR2E71Y_BDIC_I2C_SLAVE_ADDR          (0xA8)
#define IR2E71Y_BDIC_I2C_WBUF_MAX            (6)
#define IR2E71Y_BDIC_I2C_RBUF_MAX            (6)

#define IR2E71Y_BDIC_INT_GFAC_GFAC0          (0x00000001)
#define IR2E71Y_BDIC_INT_GFAC_GFAC1          (0x00000002)
#define IR2E71Y_BDIC_INT_GFAC_GFAC2          (0x00000004)
#define IR2E71Y_BDIC_INT_GFAC_PS             (0x00000008)
#define IR2E71Y_BDIC_INT_GFAC_GFAC4          (0x00000010)
#define IR2E71Y_BDIC_INT_GFAC_ALS            (0x00000100)
#define IR2E71Y_BDIC_INT_GFAC_PS2            (0x00000200)
#define IR2E71Y_BDIC_INT_GFAC_OPTON          (0x00000400)
#define IR2E71Y_BDIC_INT_GFAC_CPON           (0x00000800)
#define IR2E71Y_BDIC_INT_GFAC_ANIME          (0x00001000)
#define IR2E71Y_BDIC_INT_GFAC_TEST1          (0x00002000)
#define IR2E71Y_BDIC_INT_GFAC_DCDC2_ERR      (0x00004000)
#define IR2E71Y_BDIC_INT_GFAC_TSD            (0x00008000)
#define IR2E71Y_BDIC_INT_GFAC_TEST2          (0x00010000)
#define IR2E71Y_BDIC_INT_GFAC_TEST3          (0x00020000)
#define IR2E71Y_BDIC_INT_GFAC_DET            (0x00040000)
#define IR2E71Y_BDIC_INT_GFAC_I2C_ERR        (0x00080000)
#define IR2E71Y_BDIC_INT_GFAC_TEST4          (0x00100000)
#define IR2E71Y_BDIC_INT_GFAC_OPTSEL         (0x00200000)
#define IR2E71Y_BDIC_INT_GFAC_TEST5          (0x00400000)
#define IR2E71Y_BDIC_INT_GFAC_TEST6          (0x00800000)
#define IR2E71Y_BDIC_INT_GFAC_ALS_TRG1       (0x01000000)
#define IR2E71Y_BDIC_INT_GFAC_ALS_TRG2       (0x02000000)

#define IR2E71Y_BDIC_SENSOR_TYPE_PHOTO       (0x01)
#define IR2E71Y_BDIC_SENSOR_TYPE_PROX        (0x02)
#define IR2E71Y_BDIC_SENSOR_SLAVE_ADDR       (0x39)

#define IR2E71Y_OPT_CHANGE_WAIT_TIME         (150)

#define IR2E71Y_BDIC_GINF3_DCDC1_OVD         (0x20)

#ifdef IR2E71Y_ALS_INT
#define IR2E71Y_BDIC_OPT_L_EDGE_EN           (0x20)
#define IR2E71Y_BDIC_OPT_H_EDGE_EN           (0x40)
#define IR2E71Y_BDIC_OPT_TH_SIDE             (0x80)

#define IR2E71Y_BDIC_OPT_TABLE1_IMR          (0x10)
#define IR2E71Y_BDIC_OPT_TABLE2_IMR          (0x20)
#endif /* IR2E71Y_ALS_INT */

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

enum {
    IR2E71Y_BDIC_REQ_NONE = 0,
    IR2E71Y_BDIC_REQ_ACTIVE,
    IR2E71Y_BDIC_REQ_STANDBY,
    IR2E71Y_BDIC_REQ_STOP,
    IR2E71Y_BDIC_REQ_START,
    IR2E71Y_BDIC_REQ_BKL_SET_MODE_OFF,
    IR2E71Y_BDIC_REQ_BKL_SET_MODE_FIX,
    IR2E71Y_BDIC_REQ_BKL_SET_MODE_AUTO,
    IR2E71Y_BDIC_REQ_PHOTO_SENSOR_CONFIG,
    IR2E71Y_BDIC_REQ_BKL_DTV_OFF,
    IR2E71Y_BDIC_REQ_BKL_DTV_ON,
    IR2E71Y_BDIC_REQ_BKL_SET_EMG_MODE,
#ifdef IR2E71Y_LOWBKL
    IR2E71Y_BDIC_REQ_BKL_LOWBKL_OFF,
    IR2E71Y_BDIC_REQ_BKL_LOWBKL_ON,
#endif /* IR2E71Y_LOWBKL */
    IR2E71Y_BDIC_REQ_BKL_CHG_OFF,
    IR2E71Y_BDIC_REQ_BKL_CHG_ON,
#ifdef IR2E71Y_TRV_NM2
    IR2E71Y_BDIC_REQ_BKL_TRV_REQUEST,
#endif /* IR2E71Y_TRV_NM2 */
    IR2E71Y_BDIC_REQ_BKL_ON,
    IR2E71Y_BDIC_REQ_BKL_FIX_START,
    IR2E71Y_BDIC_REQ_BKL_AUTO_START,
    IR2E71Y_BDIC_REQ_BKL_SET_LED_VALUE,
    IR2E71Y_BDIC_REQ_BKL_SET_OPT_VALUE
};

enum {
    IR2E71Y_BDIC_PWR_STATUS_OFF,
    IR2E71Y_BDIC_PWR_STATUS_STANDBY,
    IR2E71Y_BDIC_PWR_STATUS_ACTIVE,
    NUM_IR2E71Y_BDIC_PWR_STATUS
};

enum {
    IR2E71Y_BDIC_DEV_TYPE_LCD_BKL,
    IR2E71Y_BDIC_DEV_TYPE_LCD_PWR,
    IR2E71Y_BDIC_DEV_TYPE_TRI_LED,
    IR2E71Y_BDIC_DEV_TYPE_TRI_LED_ANIME,
    IR2E71Y_BDIC_DEV_TYPE_PHOTO_SENSOR_APP,
    IR2E71Y_BDIC_DEV_TYPE_PHOTO_SENSOR_LUX,
    IR2E71Y_BDIC_DEV_TYPE_PHOTO_SENSOR_BKL,
    IR2E71Y_BDIC_DEV_TYPE_PROX_SENSOR,
    NUM_IR2E71Y_BDIC_DEV_TYPE
};

enum {
    IR2E71Y_BDIC_DEV_PWR_OFF,
    IR2E71Y_BDIC_DEV_PWR_ON,
    NUM_IR2E71Y_BDIC_DEV_PWR
};

enum {
    IR2E71Y_MAIN_BKL_DEV_TYPE_APP,
    IR2E71Y_MAIN_BKL_DEV_TYPE_APP_AUTO,
    NUM_IR2E71Y_MAIN_BKL_DEV_TYPE
};

enum {
    IR2E71Y_BDIC_IRQ_TYPE_NONE,
    IR2E71Y_BDIC_IRQ_TYPE_ALS,
    IR2E71Y_BDIC_IRQ_TYPE_PS,
    IR2E71Y_BDIC_IRQ_TYPE_DET,
    IR2E71Y_BDIC_IRQ_TYPE_I2C_ERR,
#ifdef IR2E71Y_ALS_INT
    IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER,
    IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER1,
    IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER2,
#endif /* IR2E71Y_ALS_INT */
    NUM_IR2E71Y_BDIC_IRQ_TYPE
};

enum {
    IR2E71Y_MAIN_BKL_ADJ_RETRY0,
    IR2E71Y_MAIN_BKL_ADJ_RETRY1,
    IR2E71Y_MAIN_BKL_ADJ_RETRY2,
    IR2E71Y_MAIN_BKL_ADJ_RETRY3,
    NUM_IR2E71Y_MAIN_BKL_ADJ
};

enum {
    IR2E71Y_BDIC_MAIN_BKL_OPT_LOW,
    IR2E71Y_BDIC_MAIN_BKL_OPT_HIGH,
    NUM_IR2E71Y_BDIC_MAIN_BKL_OPT_MODE
};

enum {
    IR2E71Y_BDIC_PHOTO_LUX_TIMER_ON,
    IR2E71Y_BDIC_PHOTO_LUX_TIMER_OFF,
    NUM_IR2E71Y_BDIC_PHOTO_LUX_TIMER_SWITCH
};

enum {
    IR2E71Y_BDIC_LUX_JUDGE_IN,
    IR2E71Y_BDIC_LUX_JUDGE_IN_CONTI,
    IR2E71Y_BDIC_LUX_JUDGE_OUT,
    IR2E71Y_BDIC_LUX_JUDGE_OUT_CONTI,
    IR2E71Y_BDIC_LUX_JUDGE_ERROR,
    NUM_IR2E71Y_BDIC_LUX_JUDGE
};

enum {
    IR2E71Y_BDIC_BL_PARAM_WRITE = 0,
    IR2E71Y_BDIC_BL_PARAM_READ,
    IR2E71Y_BDIC_BL_MODE_SET,
    IR2E71Y_BDIC_ALS_SET,
    IR2E71Y_BDIC_ALS_PARAM_WRITE,
    IR2E71Y_BDIC_ALS_PARAM_READ,
    IR2E71Y_BDIC_ALS_PARAM_SET,
    IR2E71Y_BDIC_CABC_CTL,
    IR2E71Y_BDIC_CABC_CTL_TIME_SET,
    IR2E71Y_BDIC_DEVICE_SET
};

enum {
    IR2E71Y_BDIC_BL_PWM_FIX_PARAM = 0,
    IR2E71Y_BDIC_BL_PWM_AUTO_PARAM
};

enum {
    IR2E71Y_BDIC_PSALS_RECOVERY_NONE = 0,
    IR2E71Y_BDIC_PSALS_RECOVERY_DURING,
    IR2E71Y_BDIC_PSALS_RECOVERY_RETRY_OVER
};

enum {
    IR2E71Y_BDIC_BKL_MODE_OFF = 0,
    IR2E71Y_BDIC_BKL_MODE_FIX,
    IR2E71Y_BDIC_BKL_MODE_AUTO
};

enum {
    IR2E71Y_BDIC_BKL_EMG_OFF,
    IR2E71Y_BDIC_BKL_EMG_ON_LEVEL0,
    IR2E71Y_BDIC_BKL_EMG_ON_LEVEL1,
};

enum {
    IR2E71Y_BKL_TBL_MODE_NORMAL,
    IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL0,
    IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL1,
    NUM_IR2E71Y_BKL_TBL_MODE
};

struct ir2e71y_bdic_state_str {
    int handset_color;
    int bdic_chipver;
    int bdic_clrvari_index;
    struct ir2e71y_photo_sensor_adj photo_sensor_adj;
#ifdef IR2E71Y_TRV_NM2
    struct ir2e71y_trv_param trv_param;
#endif /* IR2E71Y_TRV_NM2 */
};

struct ir2e71y_bdic_bkl_ado_tbl {
    unsigned long range_low;
    unsigned long range_high;
    unsigned long param_a;
    long param_b;
};

struct ir2e71y_bdic_bkl_info {
    int mode;
    int param;
    int value;
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_initialize(struct ir2e71y_bdic_state_str *state_str);
void ir2e71y_bdic_API_LCD_release_hw_reset(void);
void ir2e71y_bdic_API_LCD_set_hw_reset(void);
void ir2e71y_bdic_API_LCD_power_on(void);
void ir2e71y_bdic_API_LCD_power_off(void);
void ir2e71y_bdic_API_LCD_m_power_on(void);
void ir2e71y_bdic_API_LCD_m_power_off(void);
void ir2e71y_bdic_API_LCD_BKL_off(void);
void ir2e71y_bdic_API_LCD_BKL_fix_on(int param);
void ir2e71y_bdic_API_LCD_BKL_auto_on(int param);
void ir2e71y_bdic_API_LCD_BKL_get_param(struct ir2e71y_bdic_bkl_info *bkl_info);
void ir2e71y_bdic_API_LCD_BKL_set_request(int type, struct ir2e71y_main_bkl_ctl *tmp);
void ir2e71y_bdic_API_LCD_BKL_get_request(int type, struct ir2e71y_main_bkl_ctl *tmp, struct ir2e71y_main_bkl_ctl *req);
void ir2e71y_bdic_API_LCD_BKL_dtv_on(void);
void ir2e71y_bdic_API_LCD_BKL_dtv_off(void);
void ir2e71y_bdic_API_LCD_BKL_set_emg_mode(int emg_mode);
void ir2e71y_bdic_API_LCD_BKL_chg_on(void);
void ir2e71y_bdic_API_LCD_BKL_chg_off(void);
#ifdef IR2E71Y_LOWBKL
void ir2e71y_bdic_API_LCD_BKL_lowbkl_on(void);
void ir2e71y_bdic_API_LCD_BKL_lowbkl_off(void);
#endif /* IR2E71Y_LOWBKL */
#ifdef IR2E71Y_TRV_NM2
int ir2e71y_bdic_API_LCD_BKL_trv_param(struct ir2e71y_trv_param param);
#endif /* IR2E71Y_TRV_NM2 */

int  ir2e71y_bdic_API_PHOTO_SENSOR_get_lux(unsigned short *value, unsigned int *lux);
int  ir2e71y_bdic_API_PHOTO_SENSOR_get_raw_als(unsigned short *clear, unsigned short *ir);
#ifdef IR2E71Y_ALS_INT
int  ir2e71y_bdic_API_PHOTO_SENSOR_set_alsint(struct ir2e71y_photo_sensor_int_trigger *value);
int  ir2e71y_bdic_API_PHOTO_SENSOR_get_alsint(struct ir2e71y_photo_sensor_int_trigger *value);
#endif /* IR2E71Y_ALS_INT */
#ifdef IR2E71Y_LED_INT
int ir2e71y_bdic_API_led_auto_low_enable(bool enable);
int ir2e71y_bdic_API_led_auto_low_process(void);
#endif /* IR2E71Y_LED_INT */
int ir2e71y_bdic_API_PHOTO_SENSOR_get_light_info(struct ir2e71y_light_info *value);
int ir2e71y_bdic_API_i2c_transfer(struct ir2e71y_bdic_i2c_msg *msg);
unsigned char ir2e71y_bdic_API_I2C_start_judge(void);
void ir2e71y_bdic_API_I2C_start_ctl(int flg);

int  ir2e71y_bdic_API_DIAG_write_reg(unsigned char reg, unsigned char val);
int  ir2e71y_bdic_API_DIAG_read_reg(unsigned char reg, unsigned char *val);
int  ir2e71y_bdic_API_DIAG_multi_read_reg(unsigned char reg, unsigned char *val, int size);
int  ir2e71y_bdic_API_RECOVERY_check_restoration(void);
int  ir2e71y_bdic_API_RECOVERY_check_bdic_practical(void);
#if defined(CONFIG_ANDROID_ENGINEERING)
void ir2e71y_bdic_API_DBG_INFO_output(void);
void ir2e71y_bdic_API_OPT_INFO_output(void);
void ir2e71y_bdic_API_PSALS_INFO_output(void);
#endif /* CONFIG_ANDROID_ENGINEERING */

int  ir2e71y_bdic_API_IRQ_check_type(int irq_type);
void ir2e71y_bdic_API_IRQ_save_fac(void);
int  ir2e71y_bdic_API_IRQ_check_DET(void);
int  ir2e71y_bdic_API_IRQ_check_I2C_ERR(void);
int  ir2e71y_bdic_API_IRQ_check_fac(void);
int  ir2e71y_bdic_API_IRQ_get_fac(int iQueFac);
void ir2e71y_bdic_API_IRQ_Clear(void);
void ir2e71y_bdic_API_IRQ_i2c_error_Clear(void);
void ir2e71y_bdic_API_IRQ_det_fac_Clear(void);
void ir2e71y_bdic_API_IRQ_det_irq_ctrl(int ctrl);
void ir2e71y_bdic_API_IRQ_dbg_Clear_All(void);
void ir2e71y_bdic_API_IRQ_dbg_set_fac(unsigned int nGFAC);
int  ir2e71y_bdic_API_als_sensor_pow_ctl(int dev_type, int power_mode);
int  ir2e71y_bdic_API_psals_power_on(void);
int  ir2e71y_bdic_API_psals_power_off(void);
int  ir2e71y_bdic_API_psals_ps_init_als_off(void);
int  ir2e71y_bdic_API_psals_ps_init_als_on(void);
int  ir2e71y_bdic_API_psals_ps_deinit_als_off(void);
int  ir2e71y_bdic_API_psals_ps_deinit_als_on(void);
int  ir2e71y_bdic_API_psals_als_init_ps_off(void);
int  ir2e71y_bdic_API_psals_als_init_ps_on(void);
int  ir2e71y_bdic_API_psals_als_deinit_ps_off(void);
int  ir2e71y_bdic_API_psals_als_deinit_ps_on(void);
int  ir2e71y_bdic_API_psals_is_recovery_successful(void);
void ir2e71y_bdic_API_set_prox_sensor_param(struct ir2e71y_prox_params *prox_params);
int  ir2e71y_bdic_API_get_lux_data(void);
void ir2e71y_bdic_API_set_bkl_mode(unsigned char bkl_mode, unsigned char data, unsigned char msk);
void ir2e71y_bdic_API_set_lux_mode(unsigned char lux_mode, unsigned char data, unsigned char msk);
void ir2e71y_bdic_API_set_lux_mode_modify(unsigned char data, unsigned char msk);
int  ir2e71y_bdic_API_get_sensor_state(void);
void ir2e71y_bdic_API_RECOVERY_lux_data_backup(void);
void ir2e71y_bdic_API_RECOVERY_lux_data_restore(void);
void ir2e71y_bdic_API_ps_background(unsigned long state);
void ir2e71y_bdic_API_set_device_code(void);
int  ir2e71y_bdic_API_get_ave_ado(struct ir2e71y_ave_ado *ave_ado);
void ir2e71y_bdic_API_update_led_value(void);

#endif  /* IR2E71Y_FUNC_H */
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
