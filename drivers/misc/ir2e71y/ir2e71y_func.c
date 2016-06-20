/* drivers/misc/ir2e71y/ir2e71y_func.c  (Display Driver)
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

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "ir2e71y_define.h"
#include <linux/ir2e71y.h>
#include "ir2e71y_priv.h"

#include "ir2e71y_io.h"
#include "ir2e71y_main.h"
#include "ir2e71y_led.h"
#include "ir2e71y_func.h"
#include "ir2e71y_dbg.h"
#include "ir2e71y_pm.h"
#include "ir2e71y_ctrl.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_BDIC_BKL_DTV_OFF                 (0)
#define IR2E71Y_BDIC_BKL_DTV_ON                  (1)

#define IR2E71Y_BDIC_BKL_LOWBKL_OFF              (0)
#define IR2E71Y_BDIC_BKL_LOWBKL_ON               (1)
#define IR2E71Y_BDIC_BKL_LOWBKL_EXE              (2)

#define IR2E71Y_BDIC_BKL_CHG_OFF                 (0)
#define IR2E71Y_BDIC_BKL_CHG_ON                  (1)

#define IR2E71Y_BDIC_BKL_AUTO_FIX_PARAM_MIN_ADO  (0)

#define IR2E71Y_BDIC_GET_ADO_RETRY_TIMES         (4)
#define IR2E71Y_BDIC_MAX_ADO_VALUE          (0xFFFF)
#define IR2E71Y_BDIC_RATIO_OF_ALS0          (64 / 4)
#define IR2E71Y_BDIC_RATIO_OF_ALS1          (60 / 4)

#define IR2E71Y_BDIC_LUX_TABLE_ARRAY_SIZE    (ARRAY_SIZE(ir2e71y_bdic_bkl_ado_tbl))
#define IR2E71Y_BDIC_LUX_DIVIDE_COFF         (100)

#define IR2E71Y_BDIC_REGSET(x)               (ir2e71y_bdic_API_seq_regset(x, ARRAY_SIZE(x)))
#define IR2E71Y_BDIC_BACKUP_REGS_BDIC(x)     (ir2e71y_bdic_seq_backup_bdic_regs(x, ARRAY_SIZE(x)))
#define IR2E71Y_BDIC_BACKUP_REGS_ALS(x)      (ir2e71y_bdic_seq_backup_als_regs(x, ARRAY_SIZE(x)))
#define IR2E71Y_BDIC_RESTORE_REGS(x)         (ir2e71y_bdic_API_seq_regset(x, ARRAY_SIZE(x)))

#define IR2E71Y_BDIC_AVE_ADO_READ_TIMES          (5)
#define IR2E71Y_BDIC_INVALID_ADO                 (-1)
#define IR2E71Y_BDIC_INVALID_RANGE               (-1)
/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    IR2E71Y_BDIC_BKL_SLOPE_MODE_NONE,
    IR2E71Y_BDIC_BKL_SLOPE_MODE_FAST,
    IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW,
};

enum {
    IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_NONE,
    IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_EMG,
#ifdef IR2E71Y_LOWBKL
    IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_LOWBKL,
#endif /* IR2E71Y_LOWBKL */
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_GPIO_control(unsigned char symbol, unsigned char status);
static void ir2e71y_bdic_seq_backlight_off(void);
static void ir2e71y_bdic_seq_backlight_fix_on(int param);
static void ir2e71y_bdic_seq_backlight_auto_on(int param);
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_lux(unsigned short *ado, unsigned int *lux, unsigned short *clear, unsigned short *ir);
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_raw_als(unsigned short *clear, unsigned short *ir);
#ifdef IR2E71Y_ALS_INT
static int ir2e71y_bdic_LD_PHOTO_SENSOR_set_alsint(struct ir2e71y_photo_sensor_int_trigger *value);
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_alsint(struct ir2e71y_photo_sensor_int_trigger *value);
static int ir2e71y_bdic_LD_set_als_int(struct ir2e71y_photo_sensor_int_trigger *value);
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_lightinfo(struct ir2e71y_light_info *value);
#endif /* IR2E71Y_ALS_INT */
#ifdef IR2E71Y_TRV_NM2
static int ir2e71y_bdic_LD_LCD_BKL_trv_param(struct ir2e71y_trv_param param);
static int ir2e71y_bdic_LD_LCD_BKL_trv_level_calculation(int level, int l_limit);
#endif /* IR2E71Y_TRV_NM2 */
#ifdef IR2E71Y_LOWBKL
static void ir2e71y_bdic_LD_LCD_BKL_lowbkl_on(void);
static void ir2e71y_bdic_LD_LCD_BKL_lowbkl_off(void);
#endif /* IR2E71Y_LOWBKL */
static void ir2e71y_bdic_LD_LCD_BKL_dtv_on(void);
static void ir2e71y_bdic_LD_LCD_BKL_dtv_off(void);
static void ir2e71y_bdic_LD_LCD_BKL_set_emg_mode(int emg_mode, bool force);
static void ir2e71y_bdic_LD_LCD_BKL_get_mode(int *mode);
static void ir2e71y_bdic_LD_LCD_BKL_get_fix_param(int mode, int level, unsigned char *value);
static void ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(int mode, int level, unsigned char *opt_val);
static void ir2e71y_bdic_LD_LCD_BKL_chg_on(void);
static void ir2e71y_bdic_LD_LCD_BKL_chg_off(void);

static void ir2e71y_bdic_PD_LCD_POS_PWR_on(void);
static void ir2e71y_bdic_PD_LCD_POS_PWR_off(void);
static void ir2e71y_bdic_PD_LCD_NEG_PWR_on(void);
static void ir2e71y_bdic_PD_LCD_NEG_PWR_off(void);
static int ir2e71y_bdic_seq_backup_bdic_regs(ir2e71y_bdicRegSetting_t *regs, int size);
static int ir2e71y_bdic_seq_backup_als_regs(ir2e71y_bdicRegSetting_t *regs, int size);
static void ir2e71y_bdic_PD_BKL_control(unsigned char request, int param);
static void ir2e71y_bdic_PD_GPIO_control(unsigned char port, unsigned char status);
static unsigned char ir2e71y_bdic_PD_opt_th_shift(int index);
static void ir2e71y_bdic_PD_BKL_update_led_value(void);
static void ir2e71y_bdic_PD_BKL_set_led_value(void);
static void ir2e71y_bdic_PD_BKL_set_opt_value(void);
static int ir2e71y_bdic_PD_get_sensor_state(void);
static int ir2e71y_bdic_PD_wait4i2ctimer_stop(void);
static int ir2e71y_bdic_PD_psals_power_on(void);
static int ir2e71y_bdic_PD_psals_power_off(void);
static int ir2e71y_bdic_PD_psals_ps_init_als_off(void);
static int ir2e71y_bdic_PD_psals_ps_init_als_on(void);
static int ir2e71y_bdic_PD_psals_ps_deinit_als_off(void);
static int ir2e71y_bdic_PD_psals_ps_deinit_als_on(void);
static int ir2e71y_bdic_PD_psals_als_init_ps_off(void);
static int ir2e71y_bdic_PD_psals_als_init_ps_on(void);
static int ir2e71y_bdic_PD_psals_als_deinit_ps_off(void);
static int ir2e71y_bdic_PD_psals_als_deinit_ps_on(void);
static int ir2e71y_bdic_PD_get_ave_ado(struct ir2e71y_ave_ado *ave_ado);

static int ir2e71y_bdic_PD_REG_ADO_get_opt(unsigned short *ado, unsigned short *clear, unsigned short *ir);
static int ir2e71y_bdic_PD_REG_RAW_DATA_get_opt(unsigned short *clear, unsigned short *ir);
#ifdef IR2E71Y_ALS_INT
static int ir2e71y_bdic_PD_REG_int_setting(struct ir2e71y_photo_sensor_int_trigger *trigger);
static int ir2e71y_bdic_chk_als_trigger(struct ir2e71y_photo_sensor_trigger *trigger);
#endif /* IR2E71Y_ALS_INT */
#ifdef IR2E71Y_LED_INT
static int ir2e71y_bdic_led_auto_low_process(void);
static int ir2e71y_bdic_led_auto_low_enable(void);
static int ir2e71y_bdic_led_auto_low_disable(void);
#endif /* IR2E71Y_LED_INT */

static int ir2e71y_bdic_PD_psals_write_threshold(struct ir2e71y_prox_params *prox_params);

static int ir2e71y_bdic_PD_i2c_throughmode_ctrl(bool ctrl);

static void ir2e71y_bdic_als_shift_ps_on_table_adjust(struct ir2e71y_photo_sensor_adj *adj);
static void ir2e71y_bdic_als_shift_ps_off_table_adjust(struct ir2e71y_photo_sensor_adj *adj);

static int ir2e71y_bdic_als_user_to_devtype(int type);
#ifdef IR2E71Y_LOWBKL
static int ir2e71y_bdic_set_lowbkl_mode(int onoff);
#endif /* IR2E71Y_LOWBKL */
static int ir2e71y_bdic_set_opt_value_slow(void);
static int ir2e71y_bdic_set_opt_value_fast(void);

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static struct ir2e71y_bdic_state_str s_state_str;

static int ir2e71y_bdic_bkl_mode;
static int ir2e71y_bdic_bkl_param;
static int ir2e71y_bdic_bkl_param_auto;
static int ir2e71y_bdic_bkl_before_mode;
static int ir2e71y_bdic_bkl_slope_slow_allow;

static int ir2e71y_bdic_dtv;

static int ir2e71y_bdic_emg;
static int ir2e71y_bdic_emg_before;
static bool ir2e71y_bdic_emg_off_pending;

static int ir2e71y_bdic_chg;

static struct ir2e71y_main_bkl_ctl ir2e71y_bkl_priority_table[NUM_IR2E71Y_MAIN_BKL_DEV_TYPE] = {
    { IR2E71Y_MAIN_BKL_MODE_OFF      , IR2E71Y_MAIN_BKL_PARAM_OFF },
    { IR2E71Y_MAIN_BKL_MODE_AUTO     , IR2E71Y_MAIN_BKL_PARAM_OFF }
};


static unsigned int ir2e71y_bdic_irq_fac = 0;
static unsigned int ir2e71y_bdic_irq_fac_exe = 0;

static int  ir2e71y_bdic_irq_prioriy[IR2E71Y_IRQ_MAX_KIND];

static unsigned char ir2e71y_backup_irq_photo_req[3];

static int ir2e71y_bdic_irq_det_flag = 0;

#ifdef IR2E71Y_TRV_NM2
static int  l_limit_m = 24;
static int  l_limit_a = 85;
#endif /* IR2E71Y_TRV_NM2 */

#ifdef IR2E71Y_LOWBKL
static int ir2e71y_bdic_lowbkl;
static int ir2e71y_bdic_lowbkl_before;
#endif /* IR2E71Y_LOWBKL */

static int psals_recovery_flag = IR2E71Y_BDIC_PSALS_RECOVERY_NONE;

static int mled_delay_ms1   = 400;
static int slope_fast       = 0xD8;

#if defined(CONFIG_ANDROID_ENGINEERING)
module_param(mled_delay_ms1, int, 0600);
module_param(slope_fast, int, 0600);
#ifdef IR2E71Y_TRV_NM2
module_param(l_limit_m, int, 0600);
module_param(l_limit_a, int, 0600);
#endif /* IR2E71Y_TRV_NM2 */
#endif /* CONFIG_ANDROID_ENGINEERING */

#ifdef IR2E71Y_ALS_INT
static struct ir2e71y_photo_sensor_int_trigger sensor_int_trigger;
#endif /* IR2E71Y_ALS_INT */

static int ir2e71y_bdic_ado_for_brightness;
static signed char ir2e71y_bdic_before_range;
static unsigned short ir2e71y_bdic_before_ado;

#define IR2E71Y_MAIN_BKL_LOWBKL_AUTO_DIVIDE           (3)
#define IR2E71Y_MAIN_BKL_LOWBKL_AUTO_VAL              (0x14)
#define IR2E71Y_MAIN_BKL_LOWBKL_AUTO_PARAM_THRESHOLD  (0x14)
#define IR2E71Y_MAIN_BKL_LOWBKL_AUTO_DIVIDE_THRESHOLD (0x44)
#define IR2E71Y_MAIN_BKL_LOWBKL_FIX_DIVIDE            (5)
#define IR2E71Y_MAIN_BKL_LOWBKL_FIX_VAL               (0x40)
#define IR2E71Y_MAIN_BKL_LOWBKL_FIX_PARAM_THRESHOLD   (0x40)
#define IR2E71Y_MAIN_BKL_LOWBKL_FIX_DIVIDE_THRESHOLD  (0x80)

extern int ir2e71y_dbg_API_get_recovery_check_error(void);
extern int ir2e71y_dbg_API_update_recovery_check_error(int flag);

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_initialize                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_initialize(struct ir2e71y_bdic_state_str *state_str)
{
    struct ir2e71y_led_init_param init_param;

    s_state_str.bdic_chipver                    = state_str->bdic_chipver;

    ir2e71y_bdic_bkl_mode        = IR2E71Y_BDIC_BKL_MODE_OFF;
    ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
    ir2e71y_bdic_bkl_param       = IR2E71Y_MAIN_BKL_PARAM_OFF;
    ir2e71y_bdic_bkl_param_auto  = IR2E71Y_MAIN_BKL_PARAM_OFF;
    ir2e71y_bdic_bkl_slope_slow_allow = IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_NONE;

    ir2e71y_bdic_dtv             = IR2E71Y_BDIC_BKL_DTV_OFF;

    ir2e71y_bdic_emg             = IR2E71Y_BDIC_BKL_EMG_OFF;
    ir2e71y_bdic_emg_before      = IR2E71Y_BDIC_BKL_EMG_OFF;
    ir2e71y_bdic_emg_off_pending = false;

    ir2e71y_bdic_chg             = IR2E71Y_BDIC_BKL_CHG_OFF;

    ir2e71y_bdic_ado_for_brightness      = IR2E71Y_BDIC_INVALID_ADO;
    ir2e71y_bdic_before_range    = IR2E71Y_BDIC_INVALID_RANGE;
    ir2e71y_bdic_before_ado      = 0;

    memcpy(&(s_state_str.photo_sensor_adj),
                                &(state_str->photo_sensor_adj), sizeof(struct ir2e71y_photo_sensor_adj));

    if (s_state_str.photo_sensor_adj.status == IR2E71Y_ALS_SENSOR_ADJUST_STATUS_COMPLETED) {
        ir2e71y_bdic_als_shift_ps_on_table_adjust(&(s_state_str.photo_sensor_adj));
        ir2e71y_bdic_als_shift_ps_off_table_adjust(&(s_state_str.photo_sensor_adj));
    }
    init_param.handset_color                   = state_str->handset_color;
    init_param.bdic_chipver                    = state_str->bdic_chipver;
    ir2e71y_led_API_initialize(&init_param);

#ifdef IR2E71Y_ALS_INT
    memset(&sensor_int_trigger, 0x00, sizeof(struct ir2e71y_photo_sensor_int_trigger));
#endif /* IR2E71Y_ALS_INT */
#ifdef IR2E71Y_TRV_NM2
    memset(&s_state_str.trv_param, 0x00, sizeof(struct ir2e71y_trv_param));
#endif /* IR2E71Y_TRV_NM2 */
#ifdef IR2E71Y_LOWBKL
    ir2e71y_bdic_lowbkl          = IR2E71Y_BDIC_BKL_LOWBKL_OFF;
    ir2e71y_bdic_lowbkl_before   = IR2E71Y_BDIC_BKL_LOWBKL_OFF;
#endif /* IR2E71Y_LOWBKL */
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_set_prox_sensor_param                                     */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_set_prox_sensor_param(struct ir2e71y_prox_params *prox_params)
{
    ir2e71y_bdic_PD_psals_write_threshold(prox_params);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_release_hw_reset                                      */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_release_hw_reset(void)
{
    ir2e71y_bdic_LD_GPIO_control(IR2E71Y_BDIC_GPIO_COG_RESET, IR2E71Y_BDIC_GPIO_HIGH);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_set_hw_reset                                          */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_set_hw_reset(void)
{
    ir2e71y_bdic_LD_GPIO_control(IR2E71Y_BDIC_GPIO_COG_RESET, IR2E71Y_BDIC_GPIO_LOW);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_power_on                                              */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_power_on(void)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_PD_LCD_POS_PWR_on();
    IR2E71Y_TRACE("out")

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_power_off                                             */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_power_off(void)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_PD_LCD_POS_PWR_off();
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_m_power_on                                            */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_m_power_on(void)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_PD_LCD_NEG_PWR_on();
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_m_power_off                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_m_power_off(void)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_PD_LCD_NEG_PWR_off();
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_off                                               */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_off(void)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_seq_backlight_off();
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_fix_on                                            */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_fix_on(int param)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_seq_backlight_fix_on(param);
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_auto_on                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_auto_on(int param)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_seq_backlight_auto_on(param);
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_get_param                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_get_param(struct ir2e71y_bdic_bkl_info *bkl_info)
{
    int mode = 0;
    unsigned char value;

    switch (ir2e71y_bdic_bkl_mode) {
    case IR2E71Y_BDIC_BKL_MODE_FIX:
        bkl_info->mode = IR2E71Y_BDIC_BKL_MODE_FIX;
        bkl_info->param = ir2e71y_bdic_bkl_param;
        ir2e71y_bdic_LD_LCD_BKL_get_mode(&mode);
        ir2e71y_bdic_LD_LCD_BKL_get_fix_param(mode, ir2e71y_bdic_bkl_param, &value);
        bkl_info->value = value;
        break;

    case IR2E71Y_BDIC_BKL_MODE_AUTO:
        bkl_info->mode = IR2E71Y_BDIC_BKL_MODE_AUTO;
        bkl_info->param = ir2e71y_bdic_bkl_param_auto;
        bkl_info->value = 0x100;
        break;

    default:
        bkl_info->mode = IR2E71Y_BDIC_BKL_MODE_OFF;
        bkl_info->param = 0;
        bkl_info->value = 0;
        break;
    }
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_set_request                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_set_request(int type, struct ir2e71y_main_bkl_ctl *tmp)
{
    ir2e71y_bkl_priority_table[type].mode  = tmp->mode;
    ir2e71y_bkl_priority_table[type].param = tmp->param;

    ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
    ir2e71y_bdic_bkl_mode   = tmp->mode;
    ir2e71y_bdic_bkl_param  = tmp->param;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_get_request                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_get_request(int type, struct ir2e71y_main_bkl_ctl *tmp, struct ir2e71y_main_bkl_ctl *req)
{
    ir2e71y_bkl_priority_table[type].mode  = tmp->mode;
    ir2e71y_bkl_priority_table[type].param = tmp->param;


    IR2E71Y_TRACE("in. tmp->mode %d, tmp->param %d ", tmp->mode, tmp->param);

    if (ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].mode == IR2E71Y_MAIN_BKL_MODE_OFF) {
        req->mode  = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].mode;
        req->param = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].param;
    } else if ((ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].mode == IR2E71Y_MAIN_BKL_MODE_FIX) &&
               (ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].param == IR2E71Y_MAIN_BKL_PARAM_WEAK)) {
        req->mode  = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].mode;
        req->param = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].param;
    } else if (ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP_AUTO].param != IR2E71Y_MAIN_BKL_PARAM_OFF) {
        req->mode  = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP_AUTO].mode;
        req->param = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP_AUTO].param;
    } else {
        req->mode  = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].mode;
        req->param = ir2e71y_bkl_priority_table[IR2E71Y_MAIN_BKL_DEV_TYPE_APP].param;
    }
    IR2E71Y_TRACE("out. req->mode %d, req->param %d ", req->mode, req->param);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_dtv_on                                            */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_dtv_on(void)
{
    ir2e71y_bdic_LD_LCD_BKL_dtv_on();

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_dtv_off                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_dtv_off(void)
{
    ir2e71y_bdic_LD_LCD_BKL_dtv_off();

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_set_emg_mode                                      */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_set_emg_mode(int emg_mode)
{
    ir2e71y_bdic_LD_LCD_BKL_set_emg_mode(emg_mode, false);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_chg_on                                            */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_chg_on(void)
{
    ir2e71y_bdic_LD_LCD_BKL_chg_on();

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_chg_off                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_chg_off(void)
{
    ir2e71y_bdic_LD_LCD_BKL_chg_off();

    return;
}

#ifdef IR2E71Y_LOWBKL
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_lowbkl_on                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_lowbkl_on(void)
{
    ir2e71y_bdic_LD_LCD_BKL_lowbkl_on();

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_lowbkl_off                                        */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LCD_BKL_lowbkl_off(void)
{
    ir2e71y_bdic_LD_LCD_BKL_lowbkl_off();

    return;
}
#endif /* IR2E71Y_LOWBKL */

#ifdef IR2E71Y_TRV_NM2
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LCD_BKL_trv_param                                         */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_LCD_BKL_trv_param(struct ir2e71y_trv_param param)
{
    int ret;

    ret = ir2e71y_bdic_LD_LCD_BKL_trv_param(param);

    return ret;
}
#endif /* IR2E71Y_TRV_NM2 */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_PHOTO_SENSOR_get_lux                                      */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_PHOTO_SENSOR_get_lux(unsigned short *value, unsigned int *lux)
{
    int ret;

    ret = ir2e71y_bdic_LD_PHOTO_SENSOR_get_lux(value, lux, NULL, NULL);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_PHOTO_SENSOR_get_raw_als                                  */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_PHOTO_SENSOR_get_raw_als(unsigned short *clear, unsigned short *ir)
{
    int ret;

    ret = ir2e71y_bdic_LD_PHOTO_SENSOR_get_raw_als(clear, ir);

    return ret;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_PHOTO_SENSOR_set_alsint                                   */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_PHOTO_SENSOR_set_alsint(struct ir2e71y_photo_sensor_int_trigger *value)
{
    int ret;

    ret = ir2e71y_bdic_LD_PHOTO_SENSOR_set_alsint(value);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_PHOTO_SENSOR_get_alsint                                   */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_PHOTO_SENSOR_get_alsint(struct ir2e71y_photo_sensor_int_trigger *value)
{
    int ret;

    ret = ir2e71y_bdic_LD_PHOTO_SENSOR_get_alsint(value);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_PHOTO_SENSOR_get_light_info                                  */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_PHOTO_SENSOR_get_light_info(struct ir2e71y_light_info *value)
{
    int ret;

    ret = ir2e71y_bdic_LD_PHOTO_SENSOR_get_lightinfo(value);

    return ret;
}
#endif /* IR2E71Y_ALS_INT */

#ifdef IR2E71Y_LED_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_led_auto_low_enable                                       */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_led_auto_low_enable(bool enable)
{
    int ret;

    if (enable) {
        ret = ir2e71y_bdic_led_auto_low_enable();
    } else {
        ret = ir2e71y_bdic_led_auto_low_disable();
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_led_auto_low_process                                      */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_led_auto_low_process(void)
{
    int ret;

    ret = ir2e71y_bdic_led_auto_low_process();

    return ret;
}
#endif /* IR2E71Y_LED_INT */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_i2c_transfer                                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_i2c_transfer(struct ir2e71y_bdic_i2c_msg *msg)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    int reg = 0;

    if (msg == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> msg.");
        return IR2E71Y_RESULT_FAILURE;
    }

    reg = (int)msg->wbuf[0];

    if (msg->mode >= NUM_IR2E71Y_BDIC_I2C_M) {
        IR2E71Y_ERR("<INVALID_VALUE> msg->mode(%d).", msg->mode);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (reg < SENSOR_REG_COMMAND1 || reg > SENSOR_REG_D2_MSB) {
        IR2E71Y_ERR("<RESULT_FAILURE> Register address out of range.");
        return IR2E71Y_RESULT_FAILURE;
    }


    ir2e71y_bdic_PD_i2c_throughmode_ctrl(true);
    ret = ir2e71y_bdic_API_IO_i2c_transfer(msg);
    ir2e71y_bdic_PD_i2c_throughmode_ctrl(false);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_DIAG_write_reg                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_DIAG_write_reg(unsigned char reg, unsigned char val)
{
    int ret = 0;

    ret = ir2e71y_bdic_API_IO_write_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_DIAG_read_reg                                             */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_DIAG_read_reg(unsigned char reg, unsigned char *val)
{
    int ret = 0;

    ret = ir2e71y_bdic_API_IO_read_reg(reg, val);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_DIAG_multi_read_reg                                       */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_DIAG_multi_read_reg(unsigned char reg, unsigned char *val, int size)
{
    int ret = 0;

    ret = ir2e71y_bdic_API_IO_multi_read_reg(reg, val, size);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_RECOVERY_check_restoration                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_RECOVERY_check_restoration(void)
{
    unsigned char dummy = 0;

    ir2e71y_bdic_API_IO_bank_set(0x00);
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GINF4, &dummy);

    if (dummy & 0x04) {
        return IR2E71Y_RESULT_SUCCESS;
    } else {
        return IR2E71Y_RESULT_FAILURE;
    }
}

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_DBG_INFO_output                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_DBG_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;
    unsigned short   ir2e71y_log_lv_bk;

    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_DEBUG, IR2E71Y_DEV_REQ_ON);
    pbuf = kzalloc(256 * 2, GFP_KERNEL);
    if (!pbuf) {
        IR2E71Y_ERR("kzalloc failed. size=%d", 256 * 2);
        return;
    }
    IR2E71Y_BDIC_BACKUP_REGS_BDIC(ir2e71y_bdic_restore_regs_for_bank1_dump);

    ir2e71y_log_lv_bk = ir2e71y_log_lv;
    ir2e71y_log_lv = IR2E71Y_LOG_LV_ERR;

    ir2e71y_bdic_PD_wait4i2ctimer_stop();

    p = pbuf;
    ir2e71y_bdic_API_IO_bank_set(0x00);
    for (idx = 0x00; idx <= 0xFF; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    ir2e71y_bdic_API_IO_bank_set(0x01);
    for (idx = 0x00; idx <= 0xFF; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    ir2e71y_bdic_API_IO_bank_set(0x00);
    ir2e71y_log_lv = ir2e71y_log_lv_bk;

    IR2E71Y_BDIC_RESTORE_REGS(ir2e71y_bdic_restore_regs_for_bank1_dump);
    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_DEBUG, IR2E71Y_DEV_REQ_OFF);

    printk("[BL71Y8] BDIC INFO ->>\n");
    printk("[BL71Y8] s_state_str.bdic_chipver               = %d.\n", s_state_str.bdic_chipver);
    printk("[BL71Y8] ir2e71y_bdic_bkl_mode                   = %d.\n", ir2e71y_bdic_bkl_mode);
    printk("[BL71Y8] ir2e71y_bdic_bkl_param                  = %d.\n", ir2e71y_bdic_bkl_param);
    printk("[BL71Y8] ir2e71y_bdic_bkl_param_auto             = %d.\n", ir2e71y_bdic_bkl_param_auto);
    printk("[BL71Y8] ir2e71y_bdic_dtv                        = %d.\n", ir2e71y_bdic_dtv);
    printk("[BL71Y8] ir2e71y_bdic_emg                        = %d.\n", ir2e71y_bdic_emg);
    printk("[BL71Y8] ir2e71y_bdic_emg_off_pending            = %d.\n", ir2e71y_bdic_emg_off_pending);
#ifdef IR2E71Y_LOWBKL
    printk("[BL71Y8] ir2e71y_bdic_lowbkl                     = %d.\n", ir2e71y_bdic_lowbkl);
#endif /* IR2E71Y_LOWBKL */
    printk("[BL71Y8] ir2e71y_bdic_chg                        = %d.\n", ir2e71y_bdic_chg);
#ifdef IR2E71Y_TRV_NM2
    printk("[BL71Y8] trv_param.request                      = %d.\n", s_state_str.trv_param.request);
    printk("[BL71Y8] trv_param.strength                     = %d.\n", s_state_str.trv_param.strength);
    printk("[BL71Y8] trv_param.adjust                       = %d.\n", s_state_str.trv_param.adjust);
#endif /* IR2E71Y_TRV_NM2 */

    printk("[BL71Y8] photo_sensor_adj.status                = 0x%02X.\n", s_state_str.photo_sensor_adj.status);
    printk("[BL71Y8] photo_sensor_adj.als_adj0              = 0x%04X.\n", s_state_str.photo_sensor_adj.als_adjust[0].als_adj0);
    printk("[BL71Y8] photo_sensor_adj.als_adj1              = 0x%04X.\n", s_state_str.photo_sensor_adj.als_adjust[0].als_adj1);
    printk("[BL71Y8] photo_sensor_adj.als_shift             = 0x%02X.\n", s_state_str.photo_sensor_adj.als_adjust[0].als_shift);
    printk("[BL71Y8] photo_sensor_adj.clear_offset          = 0x%02X.\n", s_state_str.photo_sensor_adj.als_adjust[0].clear_offset);
    printk("[BL71Y8] photo_sensor_adj.ir_offset             = 0x%02X.\n", s_state_str.photo_sensor_adj.als_adjust[0].ir_offset);
    printk("[BL71Y8] photo_sensor_adj.als_adj0              = 0x%04X.\n", s_state_str.photo_sensor_adj.als_adjust[1].als_adj0);
    printk("[BL71Y8] photo_sensor_adj.als_adj1              = 0x%04X.\n", s_state_str.photo_sensor_adj.als_adjust[1].als_adj1);
    printk("[BL71Y8] photo_sensor_adj.als_shift             = 0x%02X.\n", s_state_str.photo_sensor_adj.als_adjust[1].als_shift);
    printk("[BL71Y8] photo_sensor_adj.clear_offset          = 0x%02X.\n", s_state_str.photo_sensor_adj.als_adjust[1].clear_offset);
    printk("[BL71Y8] photo_sensor_adj.ir_offset             = 0x%02X.\n", s_state_str.photo_sensor_adj.als_adjust[1].ir_offset);
    printk("[BL71Y8] photo_sensor_adj.chksum                = 0x%06X.\n", s_state_str.photo_sensor_adj.chksum);

    for (idx = 0; idx < NUM_IR2E71Y_MAIN_BKL_DEV_TYPE; idx++) {
        printk("[BL71Y8] ir2e71y_bkl_priority_table[%d]       = (mode:%d, param:%d).\n",
                                    idx, ir2e71y_bkl_priority_table[idx].mode, ir2e71y_bkl_priority_table[idx].param);
    }

    p = pbuf;
    for (idx = 0x00; idx < 0xFF; idx += 8) {
        printk("[BL71Y8] BDIC_REG_BANK0 0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                    idx, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
        p += 8;
    }
    for (idx = 0x00; idx < 0xFF; idx += 8) {
        printk("[BL71Y8] BDIC_REG_BANK1 0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                    idx, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
        p += 8;
    }
    printk("[BL71Y8] BDIC INFO <<-\n");
    kfree(pbuf);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_OPT_INFO_output                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_OPT_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;
    unsigned short   ir2e71y_log_lv_bk;

    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_DEBUG, IR2E71Y_DEV_REQ_ON);
    pbuf = kzalloc((BDIC_REG_OPT23 - BDIC_REG_OPT0 + 1), GFP_KERNEL);
    if (!pbuf) {
        IR2E71Y_ERR("kzalloc failed. size=%d", (BDIC_REG_OPT23 - BDIC_REG_OPT0 + 1));
        return;
    }
    IR2E71Y_BDIC_BACKUP_REGS_BDIC(ir2e71y_bdic_restore_regs_for_bank1_dump);

    ir2e71y_log_lv_bk = ir2e71y_log_lv;
    ir2e71y_log_lv = IR2E71Y_LOG_LV_ERR;

    ir2e71y_bdic_PD_wait4i2ctimer_stop();

    p = pbuf;
    ir2e71y_bdic_API_IO_bank_set(0x01);
    for (idx = BDIC_REG_OPT0; idx <= BDIC_REG_OPT23; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    ir2e71y_bdic_API_IO_bank_set(0x00);
    ir2e71y_log_lv = ir2e71y_log_lv_bk;

    IR2E71Y_BDIC_RESTORE_REGS(ir2e71y_bdic_restore_regs_for_bank1_dump);
    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_DEBUG, IR2E71Y_DEV_REQ_OFF);

    printk("[BL71Y8] BDIC_REG_OPT INFO ->>\n");
    printk("[BL71Y8] ir2e71y_bdic_bkl_mode                   = %d.\n", ir2e71y_bdic_bkl_mode);
    printk("[BL71Y8] ir2e71y_bdic_bkl_param                  = %d.\n", ir2e71y_bdic_bkl_param);
    printk("[BL71Y8] ir2e71y_bdic_bkl_param_auto             = %d.\n", ir2e71y_bdic_bkl_param_auto);
    printk("[BL71Y8] ir2e71y_bdic_emg                        = %d.\n", ir2e71y_bdic_emg);
    printk("[BL71Y8] ir2e71y_bdic_emg_off_pending            = %d.\n", ir2e71y_bdic_emg_off_pending);
    printk("[BL71Y8] ir2e71y_bdic_chg                        = %d.\n", ir2e71y_bdic_chg);

    p = pbuf;
    printk("[BL71Y8] BDIC_REG_OPT0  0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                        BDIC_REG_OPT0, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
    p += 8;
    printk("[BL71Y8] BDIC_REG_OPT8  0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                        BDIC_REG_OPT8, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
    p += 8;
    printk("[BL71Y8] BDIC_REG_OPT16 0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                        BDIC_REG_OPT16, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));

    printk("[BL71Y8] BDIC_REG_OPT INFO <<-\n");
    kfree(pbuf);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_PSALS_INFO_output                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_PSALS_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;

    printk("[BL71Y8] in PSALS SENSOR INFO ->>\n");
    (void)ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_DEBUG, IR2E71Y_DEV_REQ_INIT);
    pbuf = kzalloc((((SENSOR_REG_D2_MSB + 7) / 8) * 8), GFP_KERNEL);
    if (!pbuf) {
        IR2E71Y_ERR("kzalloc failed. size=%d", (((SENSOR_REG_D2_MSB + 7) / 8) * 8));
        return;
    }

    ir2e71y_IO_API_delay_us(1000 * 1000);

    ir2e71y_bdic_PD_i2c_throughmode_ctrl(true);

    p = pbuf;
    for (idx = SENSOR_REG_COMMAND1; idx <= SENSOR_REG_D2_MSB; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_IO_psals_read_reg(idx, p);
        p++;
    }
    p = pbuf;
    printk("[BL71Y8] SENSOR_REG_DUMP 0x00: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                        *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
    p += 8;
    printk("[BL71Y8] SENSOR_REG_DUMP 0x08: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                        *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
    p += 8;
    printk("[BL71Y8] SENSOR_REG_DUMP 0x10: %02x %02x                              \n", *p, *(p + 1));
    kfree(pbuf);

    ir2e71y_bdic_PD_i2c_throughmode_ctrl(false);

    (void)ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_DEBUG, IR2E71Y_DEV_REQ_OFF);

    printk("[BL71Y8] out PSALS SENSOR INFO <<-\n");
    return;
}
#endif /* CONFIG_ANDROID_ENGINEERING */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_check_type                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IRQ_check_type(int irq_type)
{
    if ((irq_type < IR2E71Y_IRQ_TYPE_ALS) || (irq_type >= NUM_IR2E71Y_IRQ_TYPE)) {
        return IR2E71Y_RESULT_FAILURE;
    }
    if (irq_type == IR2E71Y_IRQ_TYPE_DET) {
        if (!(IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_DET)) {
           return IR2E71Y_RESULT_FAILURE;
        }
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_save_fac                                              */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_save_fac(void)
{
    unsigned char value1 = 0, value2 = 0, value3 = 0;
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GFAC1, &value1);
    value1 &= 0x7F;
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GFAC3, &value2);
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GFAC4, &value3);
    IR2E71Y_DEBUG("GFAC4=%02x GFAC3=%02x GFAC1=%02x", value3, value2, value1);

    ir2e71y_bdic_irq_fac = (unsigned int)value1 | ((unsigned int)value2 << 8) | ((unsigned int)value3 << 16);

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_DET) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF4, 0x04);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR1, 0x08);
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF1, 0x08);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS2) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF3, 0x02);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_I2C_ERR) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR4, 0x08);
        IR2E71Y_ERR("ps_als error : INT_I2C_ERR_REQ(GFAC4[3]) detect");
    }

    if (ir2e71y_bdic_irq_fac & (IR2E71Y_BDIC_INT_GFAC_DET | IR2E71Y_BDIC_INT_GFAC_PS2 | IR2E71Y_BDIC_INT_GFAC_I2C_ERR | IR2E71Y_BDIC_INT_GFAC_ALS)) {
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GIMR3, &ir2e71y_backup_irq_photo_req[0]);
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GIMF3, &ir2e71y_backup_irq_photo_req[1]);
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GIMR4, &ir2e71y_backup_irq_photo_req[2]);
    }

    if ((ir2e71y_bdic_irq_fac & (IR2E71Y_BDIC_INT_GFAC_ALS | IR2E71Y_BDIC_INT_GFAC_OPTSEL)) != 0) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR3, 0x01);
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF3, 0x01);
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR4, 0x20);
    }

    if (ir2e71y_bdic_irq_det_flag == 1) {
        ir2e71y_bdic_irq_fac |= IR2E71Y_BDIC_INT_GFAC_DET;
        ir2e71y_bdic_irq_det_flag = 2;
    }

#ifdef IR2E71Y_ALS_INT
    value1 = 0;
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GFAC2, &value1);
    value1 &= 0x30;
    IR2E71Y_DEBUG("GFAC2=%02x ", value1);
    ir2e71y_bdic_irq_fac = ir2e71y_bdic_irq_fac | ((unsigned int)value1 << 20);
#endif /* IR2E71Y_ALS_INT */

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_check_DET                                             */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IRQ_check_DET(void)
{
    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_DET) {
        return IR2E71Y_BDIC_IRQ_TYPE_DET;
    } else {
        return IR2E71Y_BDIC_IRQ_TYPE_NONE;
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_check_I2C_ERR                                         */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IRQ_check_I2C_ERR(void)
{
    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_I2C_ERR) {
        return IR2E71Y_BDIC_IRQ_TYPE_I2C_ERR;
    } else {
        return IR2E71Y_BDIC_IRQ_TYPE_NONE;
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_check_fac                                             */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IRQ_check_fac(void)
{
    int i;

    if (ir2e71y_bdic_irq_fac == 0) {
        return IR2E71Y_RESULT_FAILURE;
    }

    ir2e71y_bdic_irq_fac_exe = (ir2e71y_bdic_irq_fac & IR2E71Y_INT_ENABLE_GFAC);
    if (ir2e71y_bdic_irq_fac_exe == 0) {
        return IR2E71Y_RESULT_FAILURE;
    }

    for (i = 0; i < IR2E71Y_IRQ_MAX_KIND; i++) {
        ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_NONE;
    }

    i = 0;
    if ((ir2e71y_bdic_irq_fac_exe &
        (IR2E71Y_BDIC_INT_GFAC_PS | IR2E71Y_BDIC_INT_GFAC_I2C_ERR | IR2E71Y_BDIC_INT_GFAC_PS2))
        == IR2E71Y_BDIC_INT_GFAC_PS) {
        ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_PS;
        i++;
    }

    if ((ir2e71y_bdic_irq_fac_exe & IR2E71Y_BDIC_INT_GFAC_DET) != 0) {
        ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_DET;
        i++;
    } else if (((ir2e71y_bdic_irq_fac_exe & IR2E71Y_BDIC_INT_GFAC_I2C_ERR) != 0)) {
        ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_I2C_ERR;
        i++;
    } else if ((ir2e71y_bdic_irq_fac_exe & (IR2E71Y_BDIC_INT_GFAC_ALS | IR2E71Y_BDIC_INT_GFAC_OPTSEL)) != 0) {
        ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_ALS;
        i++;
#ifdef IR2E71Y_ALS_INT
    } else if ((ir2e71y_bdic_irq_fac_exe & (IR2E71Y_BDIC_INT_GFAC_ALS_TRG1 | IR2E71Y_BDIC_INT_GFAC_ALS_TRG2)) != 0) {
        if ((ir2e71y_bdic_irq_fac_exe & IR2E71Y_BDIC_INT_GFAC_ALS_TRG1) != 0) {
            ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER1;
        }
        if ((ir2e71y_bdic_irq_fac_exe & IR2E71Y_BDIC_INT_GFAC_ALS_TRG2) != 0) {
            ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER2;
        }
        if ((ir2e71y_bdic_irq_fac_exe & (IR2E71Y_BDIC_INT_GFAC_ALS_TRG1 | IR2E71Y_BDIC_INT_GFAC_ALS_TRG2))
            == (IR2E71Y_BDIC_INT_GFAC_ALS_TRG1 | IR2E71Y_BDIC_INT_GFAC_ALS_TRG2)) {
            ir2e71y_bdic_irq_prioriy[i] = IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER;
        }
        i++;
#endif /* IR2E71Y_ALS_INT */
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_get_fac                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_IRQ_get_fac(int iQueFac)
{
    if (iQueFac >= IR2E71Y_IRQ_MAX_KIND) {
        return IR2E71Y_BDIC_IRQ_TYPE_NONE;
    }
    return ir2e71y_bdic_irq_prioriy[iQueFac];
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_Clear                                                 */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_Clear(void)
{
    unsigned char out1, out2, out3;

    if (ir2e71y_bdic_irq_fac == 0) {
        return;
    }

    out1 = (unsigned char)(ir2e71y_bdic_irq_fac & 0x000000FF);
    out2 = (unsigned char)((ir2e71y_bdic_irq_fac >>  8) & 0x000000FF);
    out3 = (unsigned char)((ir2e71y_bdic_irq_fac >> 16) & 0x000000FF);

    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR1, out1);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR3, out2);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, out3);

    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR1, 0x00);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR3, 0x00);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, 0x00);

    if ((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS) && (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_PS)) {
        ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMR1, 0x08);
        ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMF1, 0x08);
    }

    if ((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS2) && (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_PS2)) {
        ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMF3, 0x02);
    }


    if (((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_DET) &&
         (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_DET)) ||
        ((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS) &&
         (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_ALS)) ||
        ((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS2) &&
         (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_PS2)) ||
        ((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_I2C_ERR) &&
         (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_I2C_ERR))) {
        if (ir2e71y_backup_irq_photo_req[0] & 0x01) {
            ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMR3, 0x01);
        }
        if (ir2e71y_backup_irq_photo_req[1] & 0x01) {
            ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMF3, 0x01);
        }
        if (ir2e71y_backup_irq_photo_req[2] & 0x20) {
            ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMR4, 0x20);
        }
    }

#ifdef IR2E71Y_ALS_INT
    if ((ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS_TRG1) || (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS_TRG2)) {
        out1 = 0;
        if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS_TRG1) {
            out1 = out1 | IR2E71Y_BDIC_OPT_TABLE1_IMR;
        }
        if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS_TRG2) {
            out1 = out1 | IR2E71Y_BDIC_OPT_TABLE2_IMR;
        }
        ir2e71y_bdic_reg_als_int_clear1[1].mask = out1;
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_als_int_clear1);

        ir2e71y_bdic_PD_wait4i2ctimer_stop();

        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_als_int_clear2);

        out1 = 0;
        if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS_TRG1) {
            out1 = out1 | IR2E71Y_BDIC_OPT_TABLE1_IMR;
            ir2e71y_bdic_API_IO_write_reg(BDIC_REG_OPT_INT1, 0x00);
            memset(&(sensor_int_trigger.trigger1), 0x00, sizeof(struct ir2e71y_photo_sensor_trigger));
            IR2E71Y_DEBUG("<OTHER>TRIGGER1 clear");
        }
        if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS_TRG2) {
            out1 = out1 | IR2E71Y_BDIC_OPT_TABLE2_IMR;
            ir2e71y_bdic_API_IO_write_reg(BDIC_REG_OPT_INT2, 0x00);
            memset(&(sensor_int_trigger.trigger2), 0x00, sizeof(struct ir2e71y_photo_sensor_trigger));
            IR2E71Y_DEBUG("<OTHER>TRIGGER2 clear");
        }
        ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GSCR2, out1);
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GSCR2, out1);
    }
#endif /* IR2E71Y_ALS_INT */
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_i2c_error_Clear                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_i2c_error_Clear(void)
{
    unsigned char out2 = 0, out3 = 0;

    if (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_I2C_ERR) {
        out3 = 0x08;
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, out3);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, 0x00);
    }

    if (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_PS2) {
        out2 = 0x02;
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR3, out2);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR3, 0x00);
    }
    psals_recovery_flag = IR2E71Y_BDIC_PSALS_RECOVERY_DURING;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_det_fac_Clear                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_det_fac_Clear(void)
{
    unsigned char out3 = 0;

    if (IR2E71Y_INT_ENABLE_GFAC & IR2E71Y_BDIC_INT_GFAC_DET) {
        out3 = 0x04;
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, out3);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, 0x00);
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_det_irq_ctrl                                          */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_det_irq_ctrl(int ctrl)
{
    ir2e71y_bdic_API_IO_bank_set(0x00);
    if (ctrl) {
        ir2e71y_bdic_API_IO_set_bit_reg(BDIC_REG_GIMF4, 0x04);
    } else {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF4, 0x04);
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_dbg_Clear_All                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_dbg_Clear_All(void)
{
    unsigned char out1, out2, out3;

    out1 = (unsigned char)(IR2E71Y_INT_ENABLE_GFAC & 0x000000FF);
    out2 = (unsigned char)((IR2E71Y_INT_ENABLE_GFAC >>  8) & 0x000000FF);
    out3 = (unsigned char)((IR2E71Y_INT_ENABLE_GFAC >> 16) & 0x000000FF);

    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR1, out1);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR3, out2);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, out3);

    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR1, 0x00);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR3, 0x00);
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, 0x00);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_IRQ_dbg_set_fac                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_IRQ_dbg_set_fac(unsigned int nGFAC)
{
    ir2e71y_bdic_irq_fac = nGFAC;

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_DET) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF4, 0x04);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR1, 0x08);
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF1, 0x08);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_ALS) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR3, 0x01);
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF3, 0x01);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_PS2) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMF3, 0x02);
    }

    if (ir2e71y_bdic_irq_fac & IR2E71Y_BDIC_INT_GFAC_OPTSEL) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR4, 0x20);
    }
}


/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_als_sensor_pow_ctl                                        */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_als_sensor_pow_ctl(int dev_type, int power_mode)
{
    unsigned long type = 0;
    int param_chk = 0;

    type = ir2e71y_bdic_als_user_to_devtype(dev_type);
    if (type == NUM_IR2E71Y_PHOTO_SENSOR_TYPE) {
        param_chk = 1;
        IR2E71Y_ERR("<INVALID_VALUE> ctl->type(%d).", dev_type);
    }

    switch (power_mode) {
    case IR2E71Y_PHOTO_SENSOR_DISABLE:
        ir2e71y_pm_API_als_user_manager(type, IR2E71Y_DEV_REQ_OFF);
        break;
    case IR2E71Y_PHOTO_SENSOR_ENABLE:
        ir2e71y_pm_API_als_user_manager(type, IR2E71Y_DEV_REQ_ON);
        break;
    default:
        param_chk = 1;
        IR2E71Y_ERR("<INVALID_VALUE> ctl->power(%d).", power_mode);
        break;
    }

    if (param_chk == 1) {
        return IR2E71Y_RESULT_FAILURE;
    }

#ifdef IR2E71Y_ALS_INT
    if ((dev_type == sensor_int_trigger.type) && (power_mode == IR2E71Y_PHOTO_SENSOR_DISABLE)) {
        memset(&sensor_int_trigger, 0x00, sizeof(struct ir2e71y_photo_sensor_int_trigger));
        ir2e71y_bdic_PD_REG_int_setting(&sensor_int_trigger);

        return IR2E71Y_RESULT_ALS_INT_OFF;
    }
#endif /* IR2E71Y_ALS_INT */

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_power_on                                            */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_power_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_power_on();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_power_off                                           */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_power_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_power_off();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_ps_init_als_off                                     */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_ps_init_als_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_ps_init_als_off();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_ps_init_als_on                                      */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_ps_init_als_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_ps_init_als_on();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_ps_deinit_als_off                                   */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_ps_deinit_als_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_ps_deinit_als_off();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_ps_deinit_als_on                                    */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_ps_deinit_als_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_ps_deinit_als_on();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_als_init_ps_off                                     */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_als_init_ps_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_als_init_ps_off();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_als_init_ps_on                                      */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_als_init_ps_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_als_init_ps_on();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_als_deinit_ps_off                                   */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_als_deinit_ps_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_als_deinit_ps_off();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_als_deinit_ps_on                                    */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_als_deinit_ps_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_psals_als_deinit_ps_on();
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_psals_is_recovery_successful                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_psals_is_recovery_successful(void)
{
    if (psals_recovery_flag == IR2E71Y_BDIC_PSALS_RECOVERY_RETRY_OVER) {
        ir2e71y_bdic_API_IO_clr_bit_reg(BDIC_REG_GIMR4, 0x08);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, 0x08);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_GSCR4, 0x00);
        return IR2E71Y_RESULT_FAILURE;
    }

    psals_recovery_flag = IR2E71Y_BDIC_PSALS_RECOVERY_NONE;
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_get_ave_ado                                               */
/* ------------------------------------------------------------------------- */
int  ir2e71y_bdic_API_get_ave_ado(struct ir2e71y_ave_ado *ave_ado)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = ir2e71y_bdic_PD_get_ave_ado(ave_ado);
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_update_led_value                                          */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_update_led_value(void)
{
    IR2E71Y_TRACE("in");
    ir2e71y_bdic_PD_BKL_update_led_value();
    IR2E71Y_TRACE("out");
    return;
}

/* ------------------------------------------------------------------------- */
/* Logical Driver                                                            */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_GPIO_control                                               */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_GPIO_control(unsigned char symbol, unsigned char status)
{
    unsigned char port;

    switch (symbol) {
    case IR2E71Y_BDIC_GPIO_COG_RESET:
        port = IR2E71Y_BDIC_GPIO_GPOD4;
        break;

    default:
        return;;
    }

    ir2e71y_bdic_PD_GPIO_control(port, status);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_backlight_off                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_backlight_off(void)
{
    (void)ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_BKL, IR2E71Y_DEV_REQ_OFF);

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_MODE_OFF, 0);
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_STOP, 0);

    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_BKL, IR2E71Y_DEV_REQ_OFF);

    if (ir2e71y_bdic_emg_off_pending) {
        ir2e71y_bdic_LD_LCD_BKL_set_emg_mode(IR2E71Y_BDIC_BKL_EMG_OFF, true);
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_backlight_fix_on                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_backlight_fix_on(int param)
{
    IR2E71Y_TRACE("in param:%d", param);

    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_BKL, IR2E71Y_DEV_REQ_ON);

    IR2E71Y_PERFORMANCE("RESUME BDIC TURN-ON BACKLIGHT 0020 START");
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_MODE_FIX, param);
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_LED_VALUE, 0);
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_ON, 0);
    IR2E71Y_PERFORMANCE("RESUME BDIC TURN-ON BACKLIGHT 0020 END");

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_FIX_START, 0);

    (void)ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_BKL, IR2E71Y_DEV_REQ_ON);

    IR2E71Y_TRACE("out");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_backlight_auto_on                                         */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_backlight_auto_on(int param)
{
    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_BKL, IR2E71Y_DEV_REQ_ON);
    (void)ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_BKL, IR2E71Y_DEV_REQ_ON);

    IR2E71Y_PERFORMANCE("RESUME BDIC TURN-ON BACKLIGHT 0010 START");
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_MODE_AUTO, param);
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_LED_VALUE, 0);
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_ON, 0);
    IR2E71Y_PERFORMANCE("RESUME BDIC TURN-ON BACKLIGHT 0010 END");

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_OPT_VALUE, 0);
    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_AUTO_START, 0);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_PHOTO_SENSOR_get_lux                                       */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_lux(unsigned short *ado, unsigned int *lux, unsigned short *clear, unsigned short *ir)
{
    int ret;
    unsigned int i;
    unsigned int ret_lux;
    unsigned short ado_tmp;

    IR2E71Y_TRACE("in");

    if (ir2e71y_pm_API_is_als_active() != IR2E71Y_DEV_STATE_ON) {
        IR2E71Y_ERR("<OTHER> photo sensor user none.");
        return IR2E71Y_RESULT_FAILURE;
    }

    ret = ir2e71y_bdic_PD_REG_ADO_get_opt(&ado_tmp, clear, ir);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        return ret;
    }

    ret_lux = 0;
    if (ado_tmp != 0) {
        for (i = 0; i < IR2E71Y_BDIC_LUX_TABLE_ARRAY_SIZE; i++) {
            if ((ado_tmp >= ir2e71y_bdic_bkl_ado_tbl[i].range_low) &&
                (ado_tmp < ir2e71y_bdic_bkl_ado_tbl[i].range_high)) {
                ret_lux  = ((unsigned int)ado_tmp) * ir2e71y_bdic_bkl_ado_tbl[i].param_a;
                ret_lux += ir2e71y_bdic_bkl_ado_tbl[i].param_b;
                ret_lux += (IR2E71Y_BDIC_LUX_DIVIDE_COFF / 2);
                ret_lux /= IR2E71Y_BDIC_LUX_DIVIDE_COFF;
                break;
            }
        }
    }

    *lux = ret_lux;
    if (ado) {
        *ado = ado_tmp;
    }

    IR2E71Y_TRACE("out ado=0x%04X, lux=%u", ado_tmp, ret_lux);
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_PHOTO_SENSOR_get_raw_als                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_raw_als(unsigned short *clear, unsigned short *ir)
{
    IR2E71Y_TRACE("in");

    if (ir2e71y_pm_API_is_als_active() != IR2E71Y_DEV_STATE_ON) {
        IR2E71Y_ERR("<OTHER> photo sensor user none.");
        return IR2E71Y_RESULT_FAILURE;
    }

    ir2e71y_bdic_PD_REG_RAW_DATA_get_opt(clear, ir);

    IR2E71Y_TRACE("out clear=0x%04X, ir=0x%04X", (unsigned int)*clear, (unsigned int)*ir);
    return IR2E71Y_RESULT_SUCCESS;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_PHOTO_SENSOR_set_alsint                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_PHOTO_SENSOR_set_alsint(struct ir2e71y_photo_sensor_int_trigger *value)
{
    unsigned int type = 0;
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_chk_als_trigger(&(value->trigger1));
    if (ret == IR2E71Y_RESULT_FAILURE) {
        IR2E71Y_ERR("<INVALID_VALUE> trigger1.");
        value->result = IR2E71Y_RESULT_FAILURE;
        return IR2E71Y_RESULT_FAILURE;
    }
    ret = ir2e71y_bdic_chk_als_trigger(&(value->trigger2));
    if (ret == IR2E71Y_RESULT_FAILURE) {
        IR2E71Y_ERR("<INVALID_VALUE> trigger2.");
        value->result = IR2E71Y_RESULT_FAILURE;
        return IR2E71Y_RESULT_FAILURE;
    }
    type = ir2e71y_bdic_als_user_to_devtype(value->type);
    if (type == NUM_IR2E71Y_PHOTO_SENSOR_TYPE) {
        IR2E71Y_ERR("<INVALID_VALUE> type(%d).", value->type);
        value->result = IR2E71Y_RESULT_FAILURE;
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_pm_API_is_active_als_user(type) != IR2E71Y_DEV_STATE_ON) {
        IR2E71Y_ERR("<OTHER> photo sensor user none.");
        value->result = IR2E71Y_RESULT_FAILURE;
        return IR2E71Y_RESULT_SUCCESS;
    }

    if ((sensor_int_trigger.trigger1.enable != 0) || (sensor_int_trigger.trigger2.enable != 0)) {
        if (sensor_int_trigger.type != value->type) {
            IR2E71Y_WARN("request from other user.");
        }
    }

    ir2e71y_bdic_LD_set_als_int(value);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_PHOTO_SENSOR_get_alsint                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_alsint(struct ir2e71y_photo_sensor_int_trigger *value)
{
    IR2E71Y_TRACE("in");

    memcpy(value, &sensor_int_trigger, sizeof(struct ir2e71y_photo_sensor_int_trigger));
    value->result = IR2E71Y_RESULT_SUCCESS;

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_als_int                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_set_als_int(struct ir2e71y_photo_sensor_int_trigger *value)
{
    int ret;

    IR2E71Y_TRACE("in");

    if (value->trigger1.enable == 0) {
        IR2E71Y_DEBUG("trigger1 disable");
        memset(&value->trigger1, 0x00, sizeof(struct ir2e71y_photo_sensor_trigger));
    }
    if (value->trigger2.enable == 0) {
        IR2E71Y_DEBUG("trigger2 disable");
        memset(&value->trigger2, 0x00, sizeof(struct ir2e71y_photo_sensor_trigger));
    }

    memcpy(&sensor_int_trigger, value, sizeof(struct ir2e71y_photo_sensor_int_trigger));
    value->result = IR2E71Y_RESULT_SUCCESS;

    ret = ir2e71y_bdic_PD_REG_int_setting(&sensor_int_trigger);

    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_PHOTO_SENSOR_get_lightinfo                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_PHOTO_SENSOR_get_lightinfo(struct ir2e71y_light_info *value)
{
    int ret;
    unsigned int lux = 0;
    unsigned short clear = 0, ir = 0;
    unsigned char level = 0;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_LD_PHOTO_SENSOR_get_lux(NULL, &lux, &clear, &ir);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        value->result = IR2E71Y_RESULT_FAILURE;
        return IR2E71Y_RESULT_FAILURE;
    }
    IR2E71Y_DEBUG("lux=%d clear=%d ir=%d", lux, clear, ir);
    value->lux = lux;
    if (clear < 1) {
        IR2E71Y_DEBUG("[caution]clear is zero");
        value->clear_ir_rate = 0;
    } else {
        value->clear_ir_rate = (((unsigned int)ir * 1000) / (unsigned int)clear + 5) / 10;
    }
    ir2e71y_bdic_API_IO_bank_set(0x00);
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_ADO_INDEX, &level);
    value->level = (unsigned short)(level & 0x1F);
    IR2E71Y_DEBUG("level=%d", value->level);

    value->result = IR2E71Y_RESULT_SUCCESS;

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}
#endif /* IR2E71Y_ALS_INT */

#ifdef IR2E71Y_LED_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_led_auto_low_process                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_led_auto_low_process(void)
{
    unsigned char ginf3 = 0;

    IR2E71Y_TRACE("in");

    ir2e71y_bdic_API_IO_bank_set(0x00);
    ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GINF3, &ginf3);

    if (ginf3 & 0x01) {
        IR2E71Y_DEBUG("normal.");
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_led_cur_norm);
    } else {
        IR2E71Y_DEBUG("low.");
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_led_cur_low);
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_led_auto_low_enable                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_led_auto_low_enable(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_LED_AUTO_LOW, IR2E71Y_DEV_REQ_ON);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_als_req_on);
    ir2e71y_bdic_led_auto_low_process();

    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_led_auto_low_disable                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_led_auto_low_disable(void)
{
    IR2E71Y_TRACE("in");

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_als_req_off);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_led_cur_norm);
    ir2e71y_pm_API_als_user_manager(IR2E71Y_DEV_TYPE_LED_AUTO_LOW, IR2E71Y_DEV_REQ_OFF);

    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}
#endif /* IR2E71Y_LED_INT */

#ifdef IR2E71Y_TRV_NM2
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_trv_param                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_LCD_BKL_trv_param(struct ir2e71y_trv_param param)
{
    IR2E71Y_TRACE("in");

    if ((param.request != IR2E71Y_TRV_PARAM_ON) && (param.request != IR2E71Y_TRV_PARAM_OFF)) {
        return IR2E71Y_RESULT_FAILURE;
    }

    if (s_state_str.trv_param.request == param.request) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_TRV_REQUEST, param.request);
    IR2E71Y_DEBUG("strength(%d), adjust(%d)", param.strength, param.adjust);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_trv_level_calculation                              */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_LD_LCD_BKL_trv_level_calculation(int level, int l_limit)
{
    long long_level = 0;

    long_level = (long)((level * (255 - l_limit) * 2 + 255) / (255 * 2));
    long_level = long_level + (long)l_limit;

    IR2E71Y_DEBUG("TRV_NM2: before level(%d), after level(%d)", level, (int)long_level);

    return (int)long_level;
}
#endif /* IR2E71Y_TRV_NM2 */

#ifdef IR2E71Y_LOWBKL
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_lowbkl_on                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_lowbkl_on(void)
{
    if ((ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_ON) || (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_EXE)) {
        return;
    }

    ir2e71y_bdic_bkl_slope_slow_allow = IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_LOWBKL;

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_LOWBKL_ON, 0);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }

    ir2e71y_bdic_bkl_slope_slow_allow = IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_NONE;

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_lowbkl_off                                         */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_lowbkl_off(void)
{
    if (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_OFF) {
        return;
    }

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_LOWBKL_OFF, 0);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }
    return;
}
#endif /* IR2E71Y_LOWBKL */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_dtv_on                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_dtv_on(void)
{
    if (ir2e71y_bdic_dtv == IR2E71Y_BDIC_BKL_DTV_ON) {
        return;
    }

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_DTV_ON, 0);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_dtv_off                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_dtv_off(void)
{
    if (ir2e71y_bdic_dtv == IR2E71Y_BDIC_BKL_DTV_OFF) {
        return;
    }

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_DTV_OFF, 0);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_set_emg_mode                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_set_emg_mode(int emg_mode, bool force)
{
    IR2E71Y_TRACE("in emg_mode=%d force=%d", emg_mode, force);

    ir2e71y_bdic_emg_off_pending = false;

    if (emg_mode == ir2e71y_bdic_emg) {
        IR2E71Y_DEBUG("out: same request.");
        return;
    }

    if (!force &&
        (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) &&
        (ir2e71y_bdic_emg == IR2E71Y_BDIC_BKL_EMG_ON_LEVEL0) &&
        (emg_mode == IR2E71Y_BDIC_BKL_EMG_OFF)) {
        IR2E71Y_DEBUG("out: off pending.");
        ir2e71y_bdic_emg_off_pending = true;
        return;
    }

    ir2e71y_bdic_bkl_slope_slow_allow = IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_EMG;

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_SET_EMG_MODE, emg_mode);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }

    ir2e71y_bdic_bkl_slope_slow_allow = IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_NONE;
    ir2e71y_bdic_emg_before = ir2e71y_bdic_emg;

    IR2E71Y_TRACE("out");

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_get_mode                                           */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_get_mode(int *mode)
{

    *mode = IR2E71Y_BKL_TBL_MODE_NORMAL;

    switch (ir2e71y_bdic_emg) {
    case IR2E71Y_BDIC_BKL_EMG_ON_LEVEL0:
        *mode = IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL0;
        break;
    case IR2E71Y_BDIC_BKL_EMG_ON_LEVEL1:
        *mode = IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL1;
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_get_fix_param                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_get_fix_param(int mode, int level, unsigned char *param)
{
    unsigned char value;

    if (param == NULL) {
        return;
    }

#ifdef IR2E71Y_TRV_NM2
    if (s_state_str.trv_param.request == IR2E71Y_TRV_PARAM_ON) {
        level = ir2e71y_bdic_LD_LCD_BKL_trv_level_calculation(level, l_limit_m);
    }
#endif /* IR2E71Y_TRV_NM2 */

#ifdef IR2E71Y_LOWBKL
    if (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_EXE) {
        if (level > IR2E71Y_MAIN_BKL_LOWBKL_FIX_DIVIDE_THRESHOLD) {
            level = ((level * IR2E71Y_MAIN_BKL_LOWBKL_FIX_DIVIDE) / 10);
            if (!level) {
                level++;
            }
        } else {
            level = IR2E71Y_MAIN_BKL_LOWBKL_FIX_VAL;
        }
    }
#endif /* IR2E71Y_LOWBKL */
    value = ir2e71y_main_bkl_tbl[level];

    switch (mode) {
    case IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL0:
        if (value > IR2E71Y_BKL_EMERGENCY_LIMIT_FIX_LEVEL0) {
            value = IR2E71Y_BKL_EMERGENCY_LIMIT_FIX_LEVEL0;
        }
        break;
    case IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL1:
        if (value > IR2E71Y_BKL_EMERGENCY_LIMIT_FIX_LEVEL1) {
            value = IR2E71Y_BKL_EMERGENCY_LIMIT_FIX_LEVEL1;
        }
        break;
    }

    *param = value;
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_chg_on                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_chg_on(void)
{
    if (ir2e71y_bdic_chg == IR2E71Y_BDIC_BKL_CHG_ON) {
        return;
    }

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_CHG_ON, 0);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_chg_off                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_chg_off(void)
{
    if (ir2e71y_bdic_chg == IR2E71Y_BDIC_BKL_CHG_OFF) {
        return;
    }

    ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_BKL_CHG_OFF, 0);

    if (ir2e71y_bdic_bkl_mode != IR2E71Y_BDIC_BKL_MODE_OFF) {
        ir2e71y_bdic_PD_BKL_control(IR2E71Y_BDIC_REQ_START, 0);
    }
    return;
}

/* ------------------------------------------------------------------------- */
/* Phygical Driver                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_LCD_POS_PWR_on                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_LCD_POS_PWR_on(void)
{
    IR2E71Y_TRACE("in")
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_vsp_on);
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_LCD_POS_PWR_off                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_LCD_POS_PWR_off(void)
{
    IR2E71Y_TRACE("in")
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_vsp_off);
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_LCD_NEG_PWR_on                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_LCD_NEG_PWR_on(void)
{
    IR2E71Y_TRACE("in")
    if (s_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        IR2E71Y_DEBUG("VSN ON(TS2) chipver=%d", s_state_str.bdic_chipver);
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_vsn_on_ts2);
    } else {
        IR2E71Y_DEBUG("VSN ON(TS1) chipver=%d", s_state_str.bdic_chipver);
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_vsn_on_ts1);
    }
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_LCD_NEG_PWR_off                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_LCD_NEG_PWR_off(void)
{
    IR2E71Y_TRACE("in")
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_vsn_off);
    IR2E71Y_TRACE("out")
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_backup_bdic_regs                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_seq_backup_bdic_regs(ir2e71y_bdicRegSetting_t *regs, int size)
{
    int i;
    int ret = IR2E71Y_RESULT_SUCCESS;
    ir2e71y_bdicRegSetting_t *tbl;

    tbl = (ir2e71y_bdicRegSetting_t *)regs;
    for (i = 0; i < size; i++) {
        switch (tbl->flg) {
        case IR2E71Y_BDIC_STR:
        case IR2E71Y_BDIC_STRM:
            ret = ir2e71y_bdic_API_IO_read_reg(tbl->addr, &(tbl->data));
            break;
        case IR2E71Y_BDIC_SET:
        case IR2E71Y_BDIC_CLR:
        case IR2E71Y_BDIC_RMW:
            break;
        case IR2E71Y_BDIC_BANK:
            ret = ir2e71y_bdic_API_IO_bank_set(tbl->data);
            break;
        case IR2E71Y_BDIC_WAIT:
            ir2e71y_IO_API_delay_us(tbl->wait);
            ret = IR2E71Y_RESULT_SUCCESS;
            break;
        case IR2E71Y_ALS_STR:
        case IR2E71Y_ALS_STRM:
        case IR2E71Y_ALS_STRMS:
        case IR2E71Y_ALS_RMW:
            break;
        default:
            break;
        }
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_ERR("bdic Read Error addr=%02X, data=%02X, mask=%02X", tbl->addr, tbl->data, tbl->mask);
            continue;
        }
        tbl++;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_backup_als_regs                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_seq_backup_als_regs(ir2e71y_bdicRegSetting_t *regs, int size)
{
    int i;
    int ret = IR2E71Y_RESULT_SUCCESS;
    ir2e71y_bdicRegSetting_t *tbl;

    tbl = (ir2e71y_bdicRegSetting_t *)regs;
    for (i = 0; i < size; i++) {
        switch (tbl->flg) {
        case IR2E71Y_BDIC_STR:
        case IR2E71Y_BDIC_STRM:
            ret = ir2e71y_bdic_API_IO_write_reg(tbl->addr, tbl->data);
            break;
        case IR2E71Y_BDIC_SET:
        case IR2E71Y_BDIC_CLR:
        case IR2E71Y_BDIC_RMW:
            break;
        case IR2E71Y_BDIC_BANK:
            ret = ir2e71y_bdic_API_IO_bank_set(tbl->data);
            break;
        case IR2E71Y_BDIC_WAIT:
            ir2e71y_IO_API_delay_us(tbl->wait);
            ret = IR2E71Y_RESULT_SUCCESS;
            break;
        case IR2E71Y_ALS_STR:
        case IR2E71Y_ALS_STRM:
        case IR2E71Y_ALS_STRMS:
            ret = ir2e71y_bdic_API_IO_psals_read_reg(tbl->addr, &(tbl->data));
            break;
        case IR2E71Y_ALS_RMW:
            break;
        default:
            break;
        }
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_ERR("bdic Read Error addr=%02X, data=%02X, mask=%02X", tbl->addr, tbl->data, tbl->mask);
            continue;
        }
        tbl++;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_BKL_control                                                */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_BKL_control(unsigned char request, int param)
{
    int ret;
    unsigned char val = 0x00;

    switch (request) {
    case IR2E71Y_BDIC_REQ_ACTIVE:
        break;

    case IR2E71Y_BDIC_REQ_STANDBY:
        break;

    case IR2E71Y_BDIC_REQ_BKL_ON:
        if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_OFF) {
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_on);
            ir2e71y_bdic_API_IO_bank_set(0x00);
            ret = ir2e71y_bdic_API_IO_read_reg(BDIC_REG_GINF3, &val);
            IR2E71Y_DEBUG("DCDC1 Err Chk. ret=%d. val=0x%02x", ret, val);
            if (ret == IR2E71Y_RESULT_SUCCESS) {
#if defined(CONFIG_ANDROID_ENGINEERING)
                if (ir2e71y_dbg_API_get_recovery_check_error() == IR2E71Y_DBG_BDIC_ERROR_DCDC1) {
                    val = val | IR2E71Y_BDIC_GINF3_DCDC1_OVD;
                    IR2E71Y_DEBUG("DEBUG DCDC1 Err set. val=0x%02x", val);
                }
#endif /* defined (CONFIG_ANDROID_ENGINEERING) */
                if ((val & IR2E71Y_BDIC_GINF3_DCDC1_OVD) == IR2E71Y_BDIC_GINF3_DCDC1_OVD) {
                    IR2E71Y_ERR("DCDC1_OVD bit ON.");
                    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_dcdc1_err);
                }
            } else {
                IR2E71Y_ERR("BDIC GINF3 read error.");
            }
        }
        break;

    case IR2E71Y_BDIC_REQ_BKL_FIX_START:
        if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_OFF) {
            ir2e71y_bdic_bkl_fix_on[2].data  = (unsigned char)slope_fast;
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_fix_on);

        } else if (ir2e71y_bdic_bkl_before_mode != IR2E71Y_BDIC_BKL_MODE_FIX) {
            ir2e71y_bdic_bkl_fix[2].data  = (unsigned char)slope_fast;
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_fix);
        }
        break;

    case IR2E71Y_BDIC_REQ_BKL_AUTO_START:
        if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_OFF) {
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_auto_on);

        } else if (ir2e71y_bdic_bkl_before_mode != IR2E71Y_BDIC_BKL_MODE_AUTO) {
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_auto);
        }
        break;

    case IR2E71Y_BDIC_REQ_BKL_SET_LED_VALUE:
        if (ir2e71y_bdic_bkl_mode == IR2E71Y_BDIC_BKL_MODE_FIX) {
            ir2e71y_bdic_PD_BKL_set_led_value();
        } else if (ir2e71y_bdic_bkl_mode == IR2E71Y_BDIC_BKL_MODE_AUTO) {
            if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_OFF) {
                ir2e71y_bdic_PD_BKL_set_led_value();
            }
        }
        break;

    case IR2E71Y_BDIC_REQ_BKL_SET_OPT_VALUE:
        ir2e71y_bdic_PD_BKL_set_opt_value();
        break;

    case IR2E71Y_BDIC_REQ_START:
        if (ir2e71y_bdic_bkl_mode == IR2E71Y_BDIC_BKL_MODE_FIX) {
            ir2e71y_bdic_PD_BKL_set_led_value();
        } else if (ir2e71y_bdic_bkl_mode == IR2E71Y_BDIC_BKL_MODE_AUTO) {
            ir2e71y_bdic_PD_BKL_set_opt_value();
        }
        break;

    case IR2E71Y_BDIC_REQ_STOP:
        ir2e71y_bdic_bkl_mode  = IR2E71Y_BDIC_BKL_MODE_OFF;
        ir2e71y_bdic_bkl_param = IR2E71Y_MAIN_BKL_PARAM_OFF;
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_off);
        break;

    case IR2E71Y_BDIC_REQ_BKL_SET_MODE_OFF:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_bkl_mode  = IR2E71Y_BDIC_BKL_MODE_OFF;
        ir2e71y_bdic_bkl_param = param;
        break;

    case IR2E71Y_BDIC_REQ_BKL_SET_MODE_FIX:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_bkl_mode  = IR2E71Y_BDIC_BKL_MODE_FIX;
        ir2e71y_bdic_bkl_param = param;
#ifdef IR2E71Y_LOWBKL
        if (ir2e71y_bdic_lowbkl != IR2E71Y_BDIC_BKL_LOWBKL_OFF) {
            ir2e71y_bdic_set_lowbkl_mode(true);
        }
#endif /* IR2E71Y_LOWBKL */
        break;

    case IR2E71Y_BDIC_REQ_BKL_SET_MODE_AUTO:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_bkl_mode  = IR2E71Y_BDIC_BKL_MODE_AUTO;
        ir2e71y_bdic_bkl_param_auto = param;
#ifdef IR2E71Y_LOWBKL
        if (ir2e71y_bdic_lowbkl != IR2E71Y_BDIC_BKL_LOWBKL_OFF) {
            ir2e71y_bdic_set_lowbkl_mode(true);
        }
#endif /* IR2E71Y_LOWBKL */
        break;

    case IR2E71Y_BDIC_REQ_BKL_DTV_OFF:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_dtv = IR2E71Y_BDIC_BKL_DTV_OFF;
        break;

    case IR2E71Y_BDIC_REQ_BKL_DTV_ON:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_dtv = IR2E71Y_BDIC_BKL_DTV_ON;
        break;

    case IR2E71Y_BDIC_REQ_BKL_SET_EMG_MODE:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_emg = param;
        break;

#ifdef IR2E71Y_LOWBKL
    case IR2E71Y_BDIC_REQ_BKL_LOWBKL_OFF:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_set_lowbkl_mode(false);
        break;

    case IR2E71Y_BDIC_REQ_BKL_LOWBKL_ON:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_set_lowbkl_mode(true);
        break;
#endif /* IR2E71Y_LOWBKL */

    case IR2E71Y_BDIC_REQ_BKL_CHG_OFF:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_chg = IR2E71Y_BDIC_BKL_CHG_OFF;
        break;

    case IR2E71Y_BDIC_REQ_BKL_CHG_ON:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        ir2e71y_bdic_chg = IR2E71Y_BDIC_BKL_CHG_ON;
        break;

#ifdef IR2E71Y_TRV_NM2
    case IR2E71Y_BDIC_REQ_BKL_TRV_REQUEST:
        ir2e71y_bdic_bkl_before_mode = ir2e71y_bdic_bkl_mode;
        s_state_str.trv_param.request = param;
#ifdef IR2E71Y_LOWBKL
        ir2e71y_bdic_set_lowbkl_mode(ir2e71y_bdic_lowbkl);
#endif /* IR2E71Y_LOWBKL */
        break;
#endif /* IR2E71Y_TRV_NM2 */

    default:
        break;
    }
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_GPIO_control                                               */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_GPIO_control(unsigned char port, unsigned char status)
{
    unsigned char reg;
    unsigned char bit;

    switch (port) {
    case IR2E71Y_BDIC_GPIO_GPOD0:
        reg = BDIC_REG_GPIO_0;
        bit = 0x01;
        break;
    case IR2E71Y_BDIC_GPIO_GPOD1:
        reg = BDIC_REG_GPIO_1;
        bit = 0x01;
        break;
    case IR2E71Y_BDIC_GPIO_GPOD2:
        reg = BDIC_REG_GPIO_2;
        bit = 0x01;
        break;
    case IR2E71Y_BDIC_GPIO_GPOD3:
        reg = BDIC_REG_GPIO_3;
        bit = 0x01;
        break;
    case IR2E71Y_BDIC_GPIO_GPOD4:
        reg = BDIC_REG_GPIO_4;
        bit = 0x01;
        break;
    case IR2E71Y_BDIC_GPIO_GPOD5:
        reg = BDIC_REG_GPIO_5;
        bit = 0x01;
        break;
    default:
        return;
    }
    if (status == IR2E71Y_BDIC_GPIO_HIGH) {
        ir2e71y_bdic_API_IO_set_bit_reg(reg, bit);
    } else {
        ir2e71y_bdic_API_IO_clr_bit_reg(reg, bit);
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_opt_th_shift                                               */
/* ------------------------------------------------------------------------- */
static unsigned char ir2e71y_bdic_PD_opt_th_shift(int index)
{
    unsigned char opt_th_shift[IR2E71Y_BKL_AUTO_OPT_TBL_NUM];
    opt_th_shift[0] = (0x07 & BDIC_REG_OPT_TH_SHIFT_1_0_VAL);
    opt_th_shift[1] = (0x70 & BDIC_REG_OPT_TH_SHIFT_1_0_VAL) >> 4;
    opt_th_shift[2] = (0x07 & BDIC_REG_OPT_TH_SHIFT_3_2_VAL);
    opt_th_shift[3] = (0x70 & BDIC_REG_OPT_TH_SHIFT_3_2_VAL) >> 4;
    opt_th_shift[4] = (0x07 & BDIC_REG_OPT_TH_SHIFT_4_5_VAL);
    opt_th_shift[5] = (0x70 & BDIC_REG_OPT_TH_SHIFT_4_5_VAL) >> 4;
    opt_th_shift[6] = (0x07 & BDIC_REG_OPT_TH_SHIFT_6_7_VAL);
    opt_th_shift[7] = (0x70 & BDIC_REG_OPT_TH_SHIFT_6_7_VAL) >> 4;
    opt_th_shift[8] = (0x07 & BDIC_REG_OPT_TH_SHIFT_8_9_VAL);
    opt_th_shift[9] = (0x70 & BDIC_REG_OPT_TH_SHIFT_8_9_VAL) >> 4;
    opt_th_shift[10] = (0x07 & BDIC_REG_OPT_TH_SHIFT_11_10_VAL);
    opt_th_shift[11] = (0x70 & BDIC_REG_OPT_TH_SHIFT_11_10_VAL) >> 4;
    opt_th_shift[12] = (0x07 & BDIC_REG_OPT_TH_SHIFT_13_12_VAL);
    opt_th_shift[13] = (0x70 & BDIC_REG_OPT_TH_SHIFT_13_12_VAL) >> 4;
    opt_th_shift[14] = (0x07 & BDIC_REG_OPT_TH_SHIFT_15_14_VAL);
    opt_th_shift[15] = (0x70 & BDIC_REG_OPT_TH_SHIFT_15_14_VAL) >> 4;
    opt_th_shift[16] = (0x07 & BDIC_REG_OPT_TH_SHIFT_17_16_VAL);
    opt_th_shift[17] = (0x70 & BDIC_REG_OPT_TH_SHIFT_17_16_VAL) >> 4;
    opt_th_shift[18] = (0x07 & BDIC_REG_OPT_TH_SHIFT_19_18_VAL);
    opt_th_shift[19] = (0x70 & BDIC_REG_OPT_TH_SHIFT_19_18_VAL) >> 4;
    opt_th_shift[20] = (0x07 & BDIC_REG_OPT_TH_SHIFT_21_20_VAL);
    opt_th_shift[21] = (0x70 & BDIC_REG_OPT_TH_SHIFT_21_20_VAL) >> 4;
    opt_th_shift[22] = (0x07 & BDIC_REG_OPT_TH_SHIFT_23_22_VAL);
    opt_th_shift[23] = (0x70 & BDIC_REG_OPT_TH_SHIFT_23_22_VAL) >> 4;
    if (index >= IR2E71Y_BKL_AUTO_OPT_TBL_NUM || index < 0) {
        IR2E71Y_ERR("invalid index");
    }
    return opt_th_shift[index];
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_BKL_update_led_value                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_BKL_update_led_value(void)
{
    int ret;
    unsigned short ado_tmp;

    ir2e71y_bdic_ado_for_brightness = IR2E71Y_BDIC_INVALID_ADO;
    ret = ir2e71y_bdic_PD_REG_ADO_get_opt(&ado_tmp, NULL, NULL);
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        ir2e71y_bdic_ado_for_brightness = (int)ado_tmp;
    }
    IR2E71Y_DEBUG("ado=0x%08X", (unsigned int)ir2e71y_bdic_ado_for_brightness);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_BKL_set_led_value                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_BKL_set_led_value(void)
{
    int i = 0, mode = 0;
    unsigned short ado_tmp;
    unsigned short ado;
    int slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_NONE;

    IR2E71Y_TRACE("in ir2e71y_bdic_bkl_mode=%d->%d", ir2e71y_bdic_bkl_before_mode, ir2e71y_bdic_bkl_mode);

    if (ir2e71y_bdic_bkl_mode == IR2E71Y_BDIC_BKL_MODE_OFF) {
        IR2E71Y_DEBUG("out: blk_off.");
        return;
    }

    ir2e71y_bdic_LD_LCD_BKL_get_mode(&mode);

    switch (ir2e71y_bdic_bkl_mode) {
    case IR2E71Y_BDIC_BKL_MODE_FIX:
        ir2e71y_bdic_LD_LCD_BKL_get_fix_param(mode, ir2e71y_bdic_bkl_param, &ir2e71y_bdic_bkl_led_value[1].data);
        ir2e71y_bdic_LD_LCD_BKL_get_fix_param(mode, ir2e71y_bdic_bkl_param, &ir2e71y_bdic_bkl_led_value[2].data);
        if (ir2e71y_bdic_bkl_before_mode != IR2E71Y_BDIC_BKL_MODE_FIX) {
            slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_NONE;
        } else {
            slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_FAST;
        }
#ifdef IR2E71Y_LOWBKL
        if ((ir2e71y_bdic_bkl_slope_slow_allow == IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_LOWBKL) &&
            (ir2e71y_bdic_lowbkl_before != IR2E71Y_BDIC_BKL_LOWBKL_EXE) &&
            (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_EXE)) {
            slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW;
        }
        if (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_OFF) {
            ir2e71y_bdic_lowbkl_before = IR2E71Y_BDIC_BKL_LOWBKL_OFF;
        }
#endif /* IR2E71Y_LOWBKL */
        if ((ir2e71y_bdic_bkl_slope_slow_allow == IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_EMG) &&
            (ir2e71y_bdic_emg_before == IR2E71Y_BDIC_BKL_EMG_OFF) &&
            (ir2e71y_bdic_emg == IR2E71Y_BDIC_BKL_EMG_ON_LEVEL0)) {
            slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW;
        }
        switch (slope_mode) {
        case IR2E71Y_BDIC_BKL_SLOPE_MODE_FAST:
            ir2e71y_bdic_bkl_slope_fast[1].data  = (unsigned char)slope_fast;
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_slope_fast);
            break;
        case IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW:
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_slope_slow);
            break;
        }
        break;
    case IR2E71Y_BDIC_BKL_MODE_AUTO:
        if (ir2e71y_bdic_ado_for_brightness >= 0) {
            ado_tmp = (unsigned short)ir2e71y_bdic_ado_for_brightness;
            ir2e71y_bdic_ado_for_brightness = IR2E71Y_BDIC_INVALID_ADO;
        } else {
            ir2e71y_bdic_PD_REG_ADO_get_opt(&ado_tmp, NULL, NULL);
        }
        ado = (unsigned long)ado_tmp << 4;
        if (ado <= IR2E71Y_BDIC_BKL_AUTO_FIX_PARAM_MIN_ADO) {
            ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(mode, 0, &ir2e71y_bdic_bkl_led_value[1].data);
            ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(mode, 0, &ir2e71y_bdic_bkl_led_value[2].data);
        } else {
            for (i = 0; i < IR2E71Y_BKL_AUTO_OPT_TBL_NUM; i++) {
                if ((ado >> ir2e71y_bdic_PD_opt_th_shift(i)) <= ir2e71y_bdic_bkl_ado_index[i]) {
                    ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(mode, i, &ir2e71y_bdic_bkl_led_value[1].data);
                    ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(mode, i, &ir2e71y_bdic_bkl_led_value[2].data);
                    break;
                }
            }
        }
        IR2E71Y_DEBUG("ado=0x%04X, current=%02x, index=%02x", (unsigned int)ado, ir2e71y_bdic_bkl_led_value[1].data, i);
        break;
    default:

        return;
    }

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_led_value);

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_BKL_set_opt_value                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_BKL_set_opt_value(void)
{
    int i;
    int mode = 0;
    int slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_FAST;

    IR2E71Y_TRACE("in ir2e71y_bdic_bkl_mode=%d->%d", ir2e71y_bdic_bkl_before_mode, ir2e71y_bdic_bkl_mode);

    ir2e71y_bdic_LD_LCD_BKL_get_mode(&mode);

    switch (ir2e71y_bdic_bkl_mode) {
    case IR2E71Y_BDIC_BKL_MODE_OFF:
        break;

    case IR2E71Y_BDIC_BKL_MODE_FIX:
        break;

    case IR2E71Y_BDIC_BKL_MODE_AUTO:
        for (i = 0; i < IR2E71Y_BKL_AUTO_OPT_TBL_NUM; i++) {
            ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(mode, i, &(ir2e71y_bdic_bkl_opt_value[i].data));
        }
#ifdef IR2E71Y_LOWBKL
        if ((ir2e71y_bdic_bkl_slope_slow_allow == IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_LOWBKL) &&
            (ir2e71y_bdic_lowbkl_before != IR2E71Y_BDIC_BKL_LOWBKL_EXE) &&
            (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_EXE)) {
            slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW;
        }
        if (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_OFF) {
            ir2e71y_bdic_lowbkl_before = IR2E71Y_BDIC_BKL_LOWBKL_OFF;
        }
#endif /* IR2E71Y_LOWBKL */
        if ((ir2e71y_bdic_bkl_slope_slow_allow == IR2E71Y_BDIC_BKL_SLOPE_SLOW_ALLOW_EMG) &&
            (ir2e71y_bdic_emg_before == IR2E71Y_BDIC_BKL_EMG_OFF) &&
            (ir2e71y_bdic_emg == IR2E71Y_BDIC_BKL_EMG_ON_LEVEL0)) {
            slope_mode = IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW;
        }
        switch (slope_mode) {
        case IR2E71Y_BDIC_BKL_SLOPE_MODE_FAST:
            ir2e71y_bdic_set_opt_value_fast();
            break;
        case IR2E71Y_BDIC_BKL_SLOPE_MODE_SLOW:
            ir2e71y_bdic_set_opt_value_slow();
            break;
        }
        break;

    default:
        break;
    }

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_LCD_BKL_get_pwm_param                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_LCD_BKL_get_pwm_param(int mode, int level, unsigned char *opt_val)
{
    unsigned long  pwm_val;
    unsigned short min_val;
    unsigned short max_val;
    int bkl_param_auto = ir2e71y_bdic_bkl_param_auto;

#ifdef IR2E71Y_TRV_NM2
    if (s_state_str.trv_param.request == IR2E71Y_TRV_PARAM_ON) {
        bkl_param_auto = ir2e71y_bdic_LD_LCD_BKL_trv_level_calculation(bkl_param_auto, l_limit_a);
    }
#endif /* IR2E71Y_TRV_NM2 */

    min_val = ir2e71y_main_bkl_min_tbl[level];
    max_val = ir2e71y_main_bkl_max_tbl[level];

    switch (mode) {
    case IR2E71Y_BKL_TBL_MODE_NORMAL:
    case IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL0:
    case IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL1:
#ifdef IR2E71Y_LOWBKL
        if (ir2e71y_bdic_lowbkl == IR2E71Y_BDIC_BKL_LOWBKL_EXE) {
            if (bkl_param_auto > IR2E71Y_MAIN_BKL_LOWBKL_AUTO_DIVIDE_THRESHOLD) {
                bkl_param_auto = ((bkl_param_auto * IR2E71Y_MAIN_BKL_LOWBKL_AUTO_DIVIDE) / 10);
                if (bkl_param_auto < IR2E71Y_MAIN_BKL_PARAM_MIN_AUTO) {
                    bkl_param_auto = IR2E71Y_MAIN_BKL_PARAM_MIN_AUTO;
                }
            } else {
                bkl_param_auto = IR2E71Y_MAIN_BKL_LOWBKL_AUTO_VAL;
            }
        }
#endif /* IR2E71Y_LOWBKL */
        if (bkl_param_auto <= IR2E71Y_MAIN_BKL_PARAM_MIN_AUTO) {
            pwm_val = (unsigned long)min_val;
        } else if (bkl_param_auto >= IR2E71Y_MAIN_BKL_PARAM_MAX_AUTO) {
            pwm_val = (unsigned long)max_val;
        } else {
            pwm_val = (unsigned long)((max_val - min_val) * (bkl_param_auto - 2));
            pwm_val /= (unsigned char)IR2E71Y_BKL_AUTO_STEP_NUM;
            pwm_val += (unsigned long)min_val;
        }
        switch (mode) {
        case IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL0:
            if (pwm_val > IR2E71Y_BKL_EMERGENCY_LIMIT_AUTO_LEVEL0) {
                pwm_val = IR2E71Y_BKL_EMERGENCY_LIMIT_AUTO_LEVEL0;
            }
            break;
        case IR2E71Y_BKL_TBL_MODE_EMERGENCY_LEVEL1:
            if (pwm_val > IR2E71Y_BKL_EMERGENCY_LIMIT_AUTO_LEVEL1) {
                pwm_val = IR2E71Y_BKL_EMERGENCY_LIMIT_AUTO_LEVEL1;
            }
            break;
        }
        break;
    default:
        if (level == 0) {
            pwm_val = (unsigned long)IR2E71Y_BKL_PWM_LOWER_LIMIT_ZERO;
        } else {
            pwm_val = (unsigned long)IR2E71Y_BKL_PWM_LOWER_LIMIT;
        }
        break;
    }

    if (pwm_val < (unsigned long)IR2E71Y_BKL_PWM_LOWER_LIMIT) {
        if (level == 0) {
            if (pwm_val < (unsigned long)IR2E71Y_BKL_PWM_LOWER_LIMIT_ZERO) {
                pwm_val = (unsigned long)IR2E71Y_BKL_PWM_LOWER_LIMIT_ZERO;
            }
        } else {
            pwm_val = (unsigned long)IR2E71Y_BKL_PWM_LOWER_LIMIT;
        }
    } else if (pwm_val > (unsigned long)IR2E71Y_BKL_PWM_UPPER_LIMIT) {
        pwm_val = (unsigned long)IR2E71Y_BKL_PWM_UPPER_LIMIT;
    } else {
        ;
    }

    pwm_val *= (unsigned char)IR2E71Y_BKL_CURRENT_UPPER_LIMIT;
    pwm_val /= (unsigned short)IR2E71Y_BKL_PWM_UPPER_LIMIT;
    *opt_val = (unsigned char)pwm_val;
    IR2E71Y_INFO("mode=%d, param=%d, pwm=0x%2lX", mode, bkl_param_auto, pwm_val);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_get_sensor_state                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_get_sensor_state(void)
{
    int ret;
    unsigned char reg = SENSOR_REG_COMMAND1;
    unsigned char val = 0x00;
    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_API_IO_psals_read_reg(reg, &val);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        val = 0x00;
    }

#if defined(CONFIG_ANDROID_ENGINEERING)
    if (ir2e71y_dbg_API_get_recovery_check_error() == IR2E71Y_DBG_RECOVERY_ERROR_PSALS) {
        ir2e71y_dbg_API_update_recovery_check_error(IR2E71Y_DBG_RECOVERY_ERROR_NONE);
        IR2E71Y_DEBUG("force sensor state error.");
        val = 0x00;
    }
#endif /* CONFIG_ANDROID_ENGINEERING */

    IR2E71Y_TRACE("out");
    return val;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_wait4i2ctimer_stop                                         */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_wait4i2ctimer_stop(void)
{
    int waitcnt;
    int retry_cnt = 3;
    int ret;
    unsigned char reg = BDIC_REG_SYSTEM8;
    unsigned char val = 0x00;

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_stop);

    for (waitcnt = 0; waitcnt < retry_cnt; waitcnt++) {
        ir2e71y_IO_API_delay_us(BDIC_WAIT_TIMER_STOP);
        ret = ir2e71y_bdic_API_IO_read_reg(reg, &val);

        if ((ret == IR2E71Y_RESULT_SUCCESS) &&
            (val == 0xA0)) {
                break;
        }

        IR2E71Y_WARN("retry(%d)!! SYSTEM8 = 0x%02x", waitcnt, val);

    }

    if (waitcnt == retry_cnt) {
        IR2E71Y_ERR("i2ctimer wait failed.");
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_i2c_throughmode_ctrl                                       */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_i2c_throughmode_ctrl(bool ctrl)
{

    if (ctrl) {
        ir2e71y_bdic_PD_wait4i2ctimer_stop();
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2c_throughmode_on);
    } else {
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2c_throughmode_off);
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_power_on                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_power_on(void)
{
    int sensor_state;
    int i;

    IR2E71Y_TRACE("in");

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_sensor_power_on);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_psals_init);

    sensor_state = ir2e71y_bdic_PD_get_sensor_state();
    if (sensor_state == 0x00) {
        IR2E71Y_ERR("psals poweron(first) failed!! sensor_state = %02x", sensor_state);

        for (i = 0; i < 3; i++) {
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_psals_init);

            sensor_state = ir2e71y_bdic_PD_get_sensor_state();
            if (sensor_state != 0x00) {
                break;
            }
            IR2E71Y_WARN("try psals poweron failed(%d)!! sensor_state = %02x", i + 1, sensor_state);
        }

        if (i == 3) {
            IR2E71Y_ERR("psals poweron retry over!!");
            if (psals_recovery_flag == IR2E71Y_BDIC_PSALS_RECOVERY_DURING) {
                psals_recovery_flag = IR2E71Y_BDIC_PSALS_RECOVERY_RETRY_OVER;
            }
        }
    }

    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_power_off                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_power_off(void)
{
    IR2E71Y_TRACE("in");
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_sensor_power_off);
    psals_recovery_flag = IR2E71Y_BDIC_PSALS_RECOVERY_NONE;
    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_ps_init_als_off                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_ps_init_als_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_init_als_off1);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out1");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_init_als_off2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_init_als_off3);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_ps_init_als_on                                       */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_ps_init_als_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_set_als_shift_ps_on);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_init_als_on2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out4");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_init_als_on3);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out5");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_init_als_on4);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out6");
        return ret;
    }

    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_ps_deinit_als_off                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_ps_deinit_als_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_deinit_als_off1);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out1");
        return ret;
    }

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_deinit_als_off2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_ps_deinit_als_on                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_ps_deinit_als_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_set_als_shift_ps_off);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ps_deinit_als_on2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out4");
        return ret;
    }
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_als_init_ps_off                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_als_init_ps_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_set_als_shift_ps_off);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_als_init_ps_off2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out43");
        return ret;
    }
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_als_init_ps_on                                       */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_als_init_ps_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_set_als_shift_ps_on);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_als_init_ps_on2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out4");
        return ret;
    }
    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_als_deinit_ps_off                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_als_deinit_ps_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_als_deinit_ps_off1);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out1");
        return ret;
    }

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_als_deinit_ps_off2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    ir2e71y_bdic_before_range    = IR2E71Y_BDIC_INVALID_RANGE;
    ir2e71y_bdic_before_ado      = 0;

    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_als_deinit_ps_on                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_als_deinit_ps_on(void)
{
    int ret;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out2");
        return ret;
    }

    ret = IR2E71Y_BDIC_REGSET(ir2e71y_bdic_als_deinit_ps_on2);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        return ret;
    }

    ir2e71y_bdic_before_range    = IR2E71Y_BDIC_INVALID_RANGE;
    ir2e71y_bdic_before_ado      = 0;

    IR2E71Y_TRACE("out");
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_psals_write_threshold                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_psals_write_threshold(struct ir2e71y_prox_params *prox_params)
{
    if (!prox_params) {
        return IR2E71Y_RESULT_FAILURE;
    }
    ir2e71y_bdic_ps_init_set_threshold[0].data = (unsigned char)(prox_params->threshold_low & 0x00FF);
    ir2e71y_bdic_ps_init_set_threshold[1].data = (unsigned char)((prox_params->threshold_low >> 8) & 0x00FF);
    ir2e71y_bdic_ps_init_set_threshold[2].data = (unsigned char)(prox_params->threshold_high & 0x00FF);
    ir2e71y_bdic_ps_init_set_threshold[3].data = (unsigned char)((prox_params->threshold_high >> 8) & 0x00FF);
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_REG_ADO_get_opt                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_REG_ADO_get_opt(unsigned short *ado, unsigned short *clear, unsigned short *ir)
{
    int ret, shift_tmp;
    uint64_t ado0, ado1;
    uint64_t als0, als1;
    unsigned short alpha, beta, gmm;
    unsigned char rval[(SENSOR_REG_D1_MSB + 1) - SENSOR_REG_D0_LSB];
    signed char range0, res;
    unsigned int retry, flag_a;

    IR2E71Y_TRACE("in");

    ir2e71y_bdic_PD_i2c_throughmode_ctrl(true);
    ret = ir2e71y_bdic_API_IO_psals_read_reg(SENSOR_REG_COMMAND2, rval);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out1");
        goto i2c_err;
    }
    range0 = rval[0] & 0x07;
    res    = (rval[0] & 0x38) >> 3;
    IR2E71Y_DEBUG("range0=%d, res=%d", range0, res);

    if (res != 0x05) {
        for (retry = 0; retry < IR2E71Y_BDIC_GET_ADO_RETRY_TIMES; retry++) {
            ret = ir2e71y_bdic_API_IO_psals_read_reg(SENSOR_REG_COMMAND1, rval);
            if (ret != IR2E71Y_RESULT_SUCCESS) {
                IR2E71Y_ERR("out2");
                goto i2c_err;
            }
            flag_a = (rval[0] & 0x02) >> 1;
            if (flag_a == 1) {
                break;
            }

            ir2e71y_IO_API_delay_us(25 * 1000);
        }
        IR2E71Y_DEBUG("retry=%d", retry);
        if (retry >= IR2E71Y_BDIC_GET_ADO_RETRY_TIMES) {
            IR2E71Y_WARN("als flag_a check retry over");
        }
    }

    ret = ir2e71y_bdic_API_IO_psals_burst_read_reg(SENSOR_REG_D0_LSB, rval, sizeof(rval));
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        goto i2c_err;
    }
    als0 = (rval[1] << 8 | rval[0]);
    als1 = (rval[3] << 8 | rval[2]);
    IR2E71Y_DEBUG("als1*16=%lld, als0*15=%lld", als1 * IR2E71Y_BDIC_RATIO_OF_ALS0, als0 * IR2E71Y_BDIC_RATIO_OF_ALS1);

    if ((als1 * IR2E71Y_BDIC_RATIO_OF_ALS0) > (als0 * IR2E71Y_BDIC_RATIO_OF_ALS1)) {
        alpha = s_state_str.photo_sensor_adj.als_adjust[1].als_adj0;
        beta  = s_state_str.photo_sensor_adj.als_adjust[1].als_adj1;
        gmm = s_state_str.photo_sensor_adj.als_adjust[1].als_shift;
        if (gmm < 16) {
            ado0 = (((als0 * alpha) - (als1 * beta)) << gmm) >> 15;
        } else {
            ado0 = (((als0 * alpha) - (als1 * beta)) >> (32 - gmm)) >> 15;
        }
        IR2E71Y_DEBUG("ROUTE-1 als0=%04llX, als1=%04llX, alpha=%04X, beta=%04X, gmm=%02x, ado0=%08llx", als0, als1, alpha, beta, gmm, ado0);
    } else {
        alpha = s_state_str.photo_sensor_adj.als_adjust[0].als_adj0;
        beta  = s_state_str.photo_sensor_adj.als_adjust[0].als_adj1;
        gmm = s_state_str.photo_sensor_adj.als_adjust[0].als_shift;
        if (gmm < 16) {
            ado0 = (((als0 * alpha) - (als1 * beta)) << gmm) >> 15;
        } else {
            ado0 = (((als0 * alpha) - (als1 * beta)) >> (32 - gmm)) >> 15;
        }
        IR2E71Y_DEBUG("ROUTE-2 als0=%04llX, als1=%04llX, alpha=%04X, beta=%04X, gmm=%02x, ado0=%08llx", als0, als1, alpha, beta, gmm, ado0);
    }

    if (res < 3) {
        shift_tmp = (res - 3) + (range0 - 4);
    } else {
        shift_tmp = ((res - 3) * 2) + (range0 - 4);
    }

    if (shift_tmp < 0) {
        shift_tmp = (-1) * shift_tmp;
        ado1 = ado0 >> shift_tmp;
    } else {
        ado1 = ado0 << shift_tmp;
    }

    if (ado1 > IR2E71Y_BDIC_MAX_ADO_VALUE) {
        ado1 = IR2E71Y_BDIC_MAX_ADO_VALUE;
    }
    if (ado) {
        *ado = (unsigned short)ado1;
    }
    if (clear) {
        *clear = (unsigned short)als0;
    }
    if (ir) {
        *ir = (unsigned short)als1;
    }

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_ar_ctrl);
    ir2e71y_bdic_PD_i2c_throughmode_ctrl(false);
    ir2e71y_IO_API_delay_us(1000);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;

i2c_err:
    ir2e71y_bdic_PD_i2c_throughmode_ctrl(false);
    return IR2E71Y_RESULT_FAILURE;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_REG_RAW_DATA_get_opt                                       */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_REG_RAW_DATA_get_opt(unsigned short *clear, unsigned short *ir)
{
    int retry, ret;
    unsigned short als0, als1;
    unsigned char rval[(SENSOR_REG_D1_MSB + 1) - SENSOR_REG_D0_LSB];
    unsigned char flag_a;

    IR2E71Y_TRACE("in");

    ir2e71y_bdic_PD_i2c_throughmode_ctrl(true);

    for (retry = 0; retry < IR2E71Y_BDIC_GET_ADO_RETRY_TIMES; retry++) {
        ret = ir2e71y_bdic_API_IO_psals_read_reg(SENSOR_REG_COMMAND1, rval);
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_ERR("out2");
            goto i2c_err;
        }
        flag_a = (rval[0] & 0x02) >> 1;
        if (flag_a == 1) {
            break;
        }
        ir2e71y_IO_API_delay_us(25 * 1000);
    }
    IR2E71Y_DEBUG("retry=%d", retry);

    ret = ir2e71y_bdic_API_IO_psals_burst_read_reg(SENSOR_REG_D0_LSB, rval, sizeof(rval));
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out3");
        goto i2c_err;
    }
    als0 = (rval[1] << 8 | rval[0]);
    als1 = (rval[3] << 8 | rval[2]);

    ir2e71y_bdic_PD_i2c_throughmode_ctrl(false);
    ir2e71y_IO_API_delay_us(1000);
    *clear = als0;
    *ir = als1;

    IR2E71Y_DEBUG("als0=0x%04X, als1=0x%04x", (unsigned int)als0, (unsigned int)als1);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;

i2c_err:
    ir2e71y_bdic_PD_i2c_throughmode_ctrl(false);
    return IR2E71Y_RESULT_FAILURE;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_REG_int_setting                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_REG_int_setting(struct ir2e71y_photo_sensor_int_trigger *trigger)
{
    unsigned char opt_int_val1 = 0;
    unsigned char opt_int_val2 = 0;
    unsigned char gimr2_val = 0;

    if (trigger->trigger1.enable) {
        IR2E71Y_DEBUG("trigger1 level(%d) side(%d) hi_edge(%d) lo_edge(%d)", trigger->trigger1.level, trigger->trigger1.side,
                                                                            trigger->trigger1.en_hi_edge, trigger->trigger1.en_lo_edge);
        gimr2_val = gimr2_val | IR2E71Y_BDIC_OPT_TABLE1_IMR;
        opt_int_val1 = trigger->trigger1.level;
        if (trigger->trigger1.side) {
            opt_int_val1 = opt_int_val1 | IR2E71Y_BDIC_OPT_TH_SIDE;
        }
        if (trigger->trigger1.en_hi_edge) {
            opt_int_val1 = opt_int_val1 | IR2E71Y_BDIC_OPT_H_EDGE_EN;
        }
        if (trigger->trigger1.en_lo_edge) {
            opt_int_val1 = opt_int_val1 | IR2E71Y_BDIC_OPT_L_EDGE_EN;
        }
    }
    if (trigger->trigger2.enable) {
        IR2E71Y_DEBUG("trigger2 level(%d) side(%d) hi_edge(%d) lo_edge(%d)", trigger->trigger2.level, trigger->trigger2.side,
                                                                            trigger->trigger2.en_hi_edge, trigger->trigger2.en_lo_edge);
        gimr2_val = gimr2_val | IR2E71Y_BDIC_OPT_TABLE2_IMR;
        opt_int_val2 = trigger->trigger2.level;
        if (trigger->trigger2.side) {
            opt_int_val2 = opt_int_val2 | IR2E71Y_BDIC_OPT_TH_SIDE;
        }
        if (trigger->trigger2.en_hi_edge) {
            opt_int_val2 = opt_int_val2 | IR2E71Y_BDIC_OPT_H_EDGE_EN;
        }
        if (trigger->trigger2.en_lo_edge) {
            opt_int_val2 = opt_int_val2 | IR2E71Y_BDIC_OPT_L_EDGE_EN;
        }
    }
    IR2E71Y_DEBUG("write OPT_INT1(0x%x) OPT_INT2(0x%x) GIMR2(0x%x)", opt_int_val1, opt_int_val2, gimr2_val);
    ir2e71y_bdic_reg_als_int_setting[1].data = opt_int_val1;
    ir2e71y_bdic_reg_als_int_setting[2].data = opt_int_val2;
    ir2e71y_bdic_reg_als_int_setting[3].data = gimr2_val;

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_reg_als_int_setting);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_chk_als_trigger                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_chk_als_trigger(struct ir2e71y_photo_sensor_trigger *trigger)
{
    int ret = IR2E71Y_RESULT_SUCCESS;

    if ((trigger->level < 0) || (trigger->level >= IR2E71Y_BKL_AUTO_OPT_TBL_NUM)) {
        ret = IR2E71Y_RESULT_FAILURE;
    }
    if ((trigger->side < 0) || (trigger->side > 1)) {
        ret = IR2E71Y_RESULT_FAILURE;
    }
    if ((trigger->en_hi_edge < 0) || (trigger->en_hi_edge > 1)) {
        ret = IR2E71Y_RESULT_FAILURE;
    }
    if ((trigger->en_lo_edge < 0) || (trigger->en_lo_edge > 1)) {
        ret = IR2E71Y_RESULT_FAILURE;
    }
    if ((trigger->enable < 0) || (trigger->enable > 1)) {
        ret = IR2E71Y_RESULT_FAILURE;
    }

    return ret;
}
#endif /* IR2E71Y_ALS_INT */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_get_ave_ado                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_get_ave_ado(struct ir2e71y_ave_ado *ave_ado)
{
    int i, ret;
    unsigned long als0 = 0;
    unsigned long als1 = 0;
    unsigned long ado  = 0;
    unsigned char rvalL, rvalH;

    IR2E71Y_TRACE("in");

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out1");
        return ret;
    }

    IR2E71Y_BDIC_BACKUP_REGS_BDIC(ir2e71y_bdic_restore_regs_for_ave_ado_bdic);
    IR2E71Y_BDIC_BACKUP_REGS_ALS(ir2e71y_bdic_restore_regs_for_ave_ado_als);

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ave_ado_param1);
    ir2e71y_bdic_ave_ado_param2[2].data = (0x18 | (ave_ado->als_range & 0x07));
    if (ir2e71y_bdic_restore_regs_for_ave_ado_als[1].data != ir2e71y_bdic_ave_ado_param2[2].data) {
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_ave_ado_param2);
    }
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_start_ave_ado);

    for (i = 0; i < IR2E71Y_BDIC_AVE_ADO_READ_TIMES; i++) {
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_stop_ave_ado);

        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_I2C_RDATA0, &rvalL);
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_I2C_RDATA1, &rvalH);
        als0 += (unsigned long)((rvalH << 8) | rvalL);

        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_I2C_RDATA2, &rvalL);
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_I2C_RDATA3, &rvalH);
        als1 += (unsigned long)((rvalH << 8) | rvalL);

        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_ADOL, &rvalL);
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_ADOH, &rvalH);
        ado  += (unsigned long)((rvalH << 8) | rvalL);

        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_start_ave_ado);
    }

    ret = ir2e71y_bdic_PD_wait4i2ctimer_stop();
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("out1");
        return ret;
    }

    IR2E71Y_BDIC_RESTORE_REGS(ir2e71y_bdic_restore_regs_for_ave_ado_bdic);
    IR2E71Y_BDIC_RESTORE_REGS(ir2e71y_bdic_restore_regs_for_ave_ado_als);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_start_ave_ado);

    ave_ado->ave_als0 = (unsigned short)(als0 / IR2E71Y_BDIC_AVE_ADO_READ_TIMES);
    ave_ado->ave_als1 = (unsigned short)(als1 / IR2E71Y_BDIC_AVE_ADO_READ_TIMES);
    ave_ado->ave_ado  = (unsigned short)(ado  / IR2E71Y_BDIC_AVE_ADO_READ_TIMES);
    IR2E71Y_DEBUG("ave_als0:0x%04X ave_als1:0x%04X ave_ado:0x%04x",
                            ave_ado->ave_als0, ave_ado->ave_als1, ave_ado->ave_ado);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_als_shift_ps_on_table_adjust                                  */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_als_shift_ps_on_table_adjust(struct ir2e71y_photo_sensor_adj *adj)
{
    unsigned char als_shift1;
    unsigned char als_shift2;

    als_shift1 = (adj->als_adjust[1].als_shift & 0x1F);
    if ((als_shift1 == 0x0E) || (als_shift1 == 0x0F)) {
        als_shift1 = 0x0F;
    } else {
        als_shift1 = (als_shift1 + 0x02) & 0x1F;
    }

    als_shift2 = (adj->als_adjust[0].als_shift & 0x1F);
    if ((als_shift2 == 0x0E) || (als_shift2 == 0x0F)) {
        als_shift2 = 0x0F;
    } else {
        als_shift2 = (als_shift2 + 0x02) & 0x1F;
    }

    ir2e71y_bdic_set_als_shift_ps_on[1].data = als_shift1;
    ir2e71y_bdic_set_als_shift_ps_on[2].data = als_shift1;
    ir2e71y_bdic_set_als_shift_ps_on[3].data = als_shift1;
    ir2e71y_bdic_set_als_shift_ps_on[4].data = als_shift2;

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_als_shift_ps_off_table_adjust                                 */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_als_shift_ps_off_table_adjust(struct ir2e71y_photo_sensor_adj *adj)
{
    unsigned char als_shift1;
    unsigned char als_shift2;

    als_shift1 = (adj->als_adjust[1].als_shift & 0x1F);
    als_shift2 = (adj->als_adjust[0].als_shift & 0x1F);

    ir2e71y_bdic_set_als_shift_ps_off[1].data = als_shift1;
    ir2e71y_bdic_set_als_shift_ps_off[2].data = als_shift1;
    ir2e71y_bdic_set_als_shift_ps_off[3].data = als_shift1;
    ir2e71y_bdic_set_als_shift_ps_off[4].data = als_shift2;

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_als_user_to_devtype                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_als_user_to_devtype(int sensor_type)
{
    int dev_type = NUM_IR2E71Y_PHOTO_SENSOR_TYPE;

    switch (sensor_type) {
    case IR2E71Y_PHOTO_SENSOR_TYPE_APP:
        dev_type = IR2E71Y_DEV_TYPE_ALS_APP;
        break;
    case IR2E71Y_PHOTO_SENSOR_TYPE_CAMERA:
        dev_type = IR2E71Y_DEV_TYPE_CAMERA;
        break;
    case IR2E71Y_PHOTO_SENSOR_TYPE_KEYLED:
        dev_type = IR2E71Y_DEV_TYPE_KEYLED;
        break;
    case IR2E71Y_PHOTO_SENSOR_TYPE_DIAG:
        dev_type = IR2E71Y_DEV_TYPE_DIAG;
        break;
    case IR2E71Y_PHOTO_SENSOR_TYPE_SENSORHUB:
        dev_type = IR2E71Y_DEV_TYPE_SENSORHUB;
        break;
    default:
        break;
    }

    return dev_type;
}

#ifdef IR2E71Y_LOWBKL
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_set_lowbkl_mode                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_set_lowbkl_mode(int onoff)
{
    ir2e71y_bdic_lowbkl_before = ir2e71y_bdic_lowbkl;

    if (onoff) {
        switch (ir2e71y_bdic_bkl_mode) {
        case IR2E71Y_BDIC_BKL_MODE_AUTO:
            if (ir2e71y_bdic_bkl_param_auto > IR2E71Y_MAIN_BKL_LOWBKL_AUTO_PARAM_THRESHOLD) {
                ir2e71y_bdic_lowbkl = IR2E71Y_BDIC_BKL_LOWBKL_EXE;
            } else {
                ir2e71y_bdic_lowbkl = IR2E71Y_BDIC_BKL_LOWBKL_ON;
            }
            break;
        case IR2E71Y_BDIC_BKL_MODE_FIX:
            if (ir2e71y_bdic_bkl_param > IR2E71Y_MAIN_BKL_LOWBKL_FIX_PARAM_THRESHOLD) {
                ir2e71y_bdic_lowbkl = IR2E71Y_BDIC_BKL_LOWBKL_EXE;
            } else {
                ir2e71y_bdic_lowbkl = IR2E71Y_BDIC_BKL_LOWBKL_ON;
            }
            break;
        }
#ifdef IR2E71Y_TRV_NM2
        if (s_state_str.trv_param.request == IR2E71Y_TRV_PARAM_ON) {
            ir2e71y_bdic_lowbkl = IR2E71Y_BDIC_BKL_LOWBKL_ON;
        }
#endif /* IR2E71Y_TRV_NM2 */
    } else {
        ir2e71y_bdic_lowbkl = IR2E71Y_BDIC_BKL_LOWBKL_OFF;
    }
    if (ir2e71y_bdic_lowbkl_before != ir2e71y_bdic_lowbkl) {
        IR2E71Y_DEBUG("Change lowbkl mode. before=%d. after=%d", ir2e71y_bdic_lowbkl_before, ir2e71y_bdic_lowbkl);
    }
    return IR2E71Y_RESULT_SUCCESS;
}
#endif /* IR2E71Y_LOWBKL */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_set_opt_value_slow                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_set_opt_value_slow(void)
{
    int ret = 0;
    unsigned char val = 0x00;

    IR2E71Y_TRACE("in");

    ir2e71y_bdic_PD_wait4i2ctimer_stop();

    ir2e71y_bdic_API_IO_bank_set(0x00);
    ret = ir2e71y_bdic_API_IO_read_reg(BDIC_REG_NLED1H, &val);
    IR2E71Y_DEBUG("NLED1H read. ret=%d. val=0x%02x", ret, val);
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        ir2e71y_bdic_bkl_led_value[1].data = val;
        ir2e71y_bdic_bkl_led_value[2].data = val;
    } else {
        IR2E71Y_ERR("BDIC NLED1H read error.");
    }
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_led_value);

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_opt_mode_off);

    ir2e71y_bdic_API_IO_bank_set(0x01);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_opt_value);
    ir2e71y_bdic_API_IO_bank_set(0x00);

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_opt_mode_on);

    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_start);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_set_opt_value_fast                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_set_opt_value_fast(void)
{
    unsigned char ado_idx = 0x00;

    IR2E71Y_TRACE("in");

    ir2e71y_bdic_PD_wait4i2ctimer_stop();

    if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_AUTO || ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_FIX) {
        ir2e71y_bdic_bkl_slope_fast[1].data  = (unsigned char)slope_fast;
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_slope_fast);
        ir2e71y_bdic_API_IO_read_reg(BDIC_REG_ADO_INDEX, &ado_idx);
        ado_idx = ado_idx & 0x1F;
        if (ado_idx >= IR2E71Y_BKL_AUTO_OPT_TBL_NUM) {
            ado_idx = IR2E71Y_BKL_AUTO_OPT_TBL_NUM - 1;
        }
        ir2e71y_bdic_bkl_led_value[1].data   = ir2e71y_bdic_bkl_opt_value[ado_idx].data;
        ir2e71y_bdic_bkl_led_value[2].data   = ir2e71y_bdic_bkl_opt_value[ado_idx].data;
    }

    ir2e71y_bdic_API_IO_bank_set(0x01);
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_opt_value);
    ir2e71y_bdic_API_IO_bank_set(0x00);

    if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_AUTO || ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_FIX) {
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_opt_mode_off);
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_led_value);
        ir2e71y_IO_API_delay_us(1000 * mled_delay_ms1);
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_opt_mode_on);
    }
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_i2ctimer_start);
    if (ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_AUTO || ir2e71y_bdic_bkl_before_mode == IR2E71Y_BDIC_BKL_MODE_FIX) {
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_bkl_slope_slow);
    }

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
