/* drivers/misc/ir2e71y/ir2e71y_led.c  (Display Driver)
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
#include "ir2e71y_led_ctrl.h"
#include "ir2e71y_main.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_BDIC_TRI_LED_MODE_OFF           (-1)
#define IR2E71Y_BDIC_TRI_LED_MODE_NORMAL         (0)
#define IR2E71Y_BDIC_TRI_LED_MODE_BLINK          (1)
#define IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY        (2)
#define IR2E71Y_BDIC_TRI_LED_MODE_HISPEED        (3)
#define IR2E71Y_BDIC_TRI_LED_MODE_STANDARD       (4)
#define IR2E71Y_BDIC_TRI_LED_MODE_BREATH         (5)
#define IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH    (6)
#define IR2E71Y_BDIC_TRI_LED_MODE_WAVE           (7)
#define IR2E71Y_BDIC_TRI_LED_MODE_FLASH          (8)
#define IR2E71Y_BDIC_TRI_LED_MODE_AURORA         (9)
#define IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW       (10)
#define IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN    (11)
#define IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR  (12)


#define NO_CURRENT_SET                          (0)
#define CURRENT_SET                             (1)

/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static void ir2e71y_led_status_init(void);
static int ir2e71y_bdic_seq_led_off(void);
static int ir2e71y_bdic_seq_led_normal_on(unsigned char color);
#ifdef IR2E71Y_SYSFS_LED
static int ir2e71y_bdic_seq_led_on(int no, struct ir2e71y_tri_led led);
static int ir2e71y_bdic_seq_leds_off(int no);
static void ir2e71y_bdic_seq_led_current_on(void);
#endif /* IR2E71Y_SYSFS_LED */
static void ir2e71y_bdic_seq_led_blink_on(unsigned char color, int ontime, int interval, int count);
static void ir2e71y_bdic_seq_led_firefly_on(unsigned char color, int ontime, int interval, int count);
#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
static void ir2e71y_bdic_seq_led_high_speed_on(unsigned char color, int interval, int count);
static void ir2e71y_bdic_seq_led_standard_on(unsigned char color, int interval, int count);
static void ir2e71y_bdic_seq_led_breath_on(unsigned char color, int interval, int count);
static void ir2e71y_bdic_seq_led_long_breath_on(unsigned char color, int interval, int count);
static void ir2e71y_bdic_seq_led_wave_on(unsigned char color, int interval, int count);
static void ir2e71y_bdic_seq_led_flash_on(unsigned char color, int interval, int count);
static void ir2e71y_bdic_seq_led_aurora_on(int interval, int count);
static void ir2e71y_bdic_seq_led_rainbow_on(int interval, int count);
#endif /* IR2E71Y_ILLUMI_COLOR_LED */
static void ir2e71y_bdic_seq_led_emopattern_on(int interval, int count);
#endif  /* IR2E71Y_ANIME_COLOR_LED */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
static void ir2e71y_bdic_seq_illumi_cancel_and_clear(void);
static void ir2e71y_bdic_seq_illumi_triple_color_on(struct ir2e71y_illumi_triple_color illumi_triple_color);
static void ir2e71y_bdic_seq_illumi_triple_color(unsigned char first_color);
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */
static void ir2e71y_bdic_LD_set_led_fix_on_table(int clr_vari, int color);
#ifdef IR2E71Y_KEY_LED
static void ir2e71y_bdic_seq_key_led_ctl(unsigned char dim, unsigned char index, int ontime, int interval);
#endif /* IR2E71Y_KEY_LED */
static void ir2e71y_bdic_LD_set_led_on_table(unsigned char *rgb_current);
#ifdef IR2E71Y_COLOR_LED_TWIN
static void ir2e71y_bdic_LD_set_led_on_table_twin(unsigned char *rgb_current);
#endif /* IR2E71Y_COLOR_LED_TWIN */

#ifdef IR2E71Y_SYSFS_LED
static bool ir2e71y_bdic_is_led_current_mode(void);
static bool ir2e71y_bdic_is_led_current_mode_no(int no);
static void ir2e71y_bdic_clear_current_param(void);
static unsigned char ir2e71y_bdic_LD_correction_led_fix_clrvari(unsigned char brightness, int color);
static void ir2e71y_bdic_LD_set_led_fix_current_table(unsigned char *rgb_current);
#ifdef IR2E71Y_COLOR_LED_TWIN
static void ir2e71y_bdic_LD_set_led_fix_current_table_twin(unsigned char *rgb_current);
#endif /* IR2E71Y_COLOR_LED_TWIN */
#endif /* IR2E71Y_SYSFS_LED */

static void ir2e71y_bdic_seq_bdic_active_for_led(int);
static void ir2e71y_bdic_seq_bdic_standby_for_led(int);
static unsigned char ir2e71y_bdic_get_color_index_and_reedit(struct ir2e71y_tri_led *tri_led);
static void ir2e71y_bdic_PD_TRI_LED_control(unsigned char request, int param);
#ifndef IR2E71Y_COLOR_LED_TWIN
static void ir2e71y_bdic_PD_TRI_LED_anime_start(void);
#endif  /* IR2E71Y_COLOR_LED_TWIN */
static void ir2e71y_bdic_PD_TRI_LED_set_anime(void);
static void ir2e71y_bdic_PD_TRI_LED_set_chdig(void);
#ifdef IR2E71Y_KEY_LED
static void ir2e71y_bdic_PD_KEY_LED_control(unsigned char dim, unsigned char index, int ontime, int interval);
static void ir2e71y_bdic_PD_KEY_LED_set_chdig(unsigned char dim, unsigned char index);
#endif /* IR2E71Y_KEY_LED */
static void ir2e71y_bdic_PD_TRI_LED_lposc_off(void);
static int ir2e71y_bdic_PD_TRI_LED_get_clrvari_index(int clrvari);
#ifdef IR2E71Y_COLOR_LED_TWIN
static void ir2e71y_bdic_PD_TRI_LED_control_twin(unsigned char request, int param);
static void ir2e71y_bdic_PD_TRI_LED_anime_start_twin(void);
static void ir2e71y_bdic_PD_TRI_LED_set_anime_twin(void);
static void ir2e71y_bdic_PD_TRI_LED_set_chdig_twin(void);
static void ir2e71y_bdic_LD_set_led_fix_on_table_twin(int clr_vari, int color);
#if defined(CONFIG_ANDROID_ENGINEERING)
static void ir2e71y_bdic_TRI_LED_INFO_output_twin(void);
#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
static void ir2e71y_bdic_illumi_triple_color_INFO_output(void);
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif /* IR2E71Y_COLOR_LED_TWIN */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
static void ir2e71y_bdic_cancel_illumi_work(void);
static void ir2e71y_bdic_clear_illumi_state(void);
static void ir2e71y_bdic_illumi_color_set_a2(void);
static void ir2e71y_workqueue_handler_illumi_set_b2(struct work_struct *work);
static void ir2e71y_workqueue_handler_illumi_set_c2(struct work_struct *work);
static void ir2e71y_workqueue_handler_illumi_set_a3(struct work_struct *work);
static void ir2e71y_workqueue_handler_illumi_set_anime_stop(struct work_struct *work);
static void ir2e71y_workqueue_handler_illumi_restart(struct work_struct *work);
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static struct ir2e71y_led_state_str led_state_str;

#ifdef IR2E71Y_KEY_LED
static unsigned char ir2e71y_bdic_key_led_index;
static unsigned char ir2e71y_bdic_key_led_before_index;
static unsigned char ir2e71y_bdic_key_led_dim;
static unsigned char ir2e71y_bdic_key_led_before_dim;

static int ir2e71y_bdic_key_led_ontime;
static int ir2e71y_bdic_key_led_interval;
#endif  /* IR2E71Y_KEY_LED */

static unsigned char ir2e71y_bdic_tri_led_color;
static int ir2e71y_bdic_tri_led_mode;
static int ir2e71y_bdic_tri_led_before_mode;
static int ir2e71y_bdic_tri_led_ontime;
static int ir2e71y_bdic_tri_led_interval;
static int ir2e71y_bdic_tri_led_count;
#ifdef IR2E71Y_COLOR_LED_TWIN
static int ir2e71y_bdic_tri_led_mode_twin;
#endif /* IR2E71Y_COLOR_LED_TWIN */

#ifdef IR2E71Y_SYSFS_LED
static unsigned char rgb_current1[IR2E71Y_RGB];
#ifdef IR2E71Y_COLOR_LED_TWIN
static unsigned char rgb_current2[IR2E71Y_RGB];
#endif /* IR2E71Y_COLOR_LED_TWIN */
#endif /* IR2E71Y_SYSFS_LED */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
struct ir2e71y_illumi_state illumi_state;
static int ir2e71y_illumi_delayed_times[ILLUMI_STATE_MAX] = {
     576730 + (1000000/HZ),
    1153460 + (1000000/HZ),
    1730190 + (1000000/HZ),
    3460380 + (1000000/HZ),
    3965900 + (1000000/HZ),
};
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */


/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_led_API_initialize                                                 */
/* ------------------------------------------------------------------------- */
int ir2e71y_led_API_initialize(struct ir2e71y_led_init_param *init_param)
{
    ir2e71y_led_status_init();

    led_state_str.bdic_chipver        = init_param->bdic_chipver;
    led_state_str.handset_color       = init_param->handset_color;
    led_state_str.bdic_clrvari_index  = ir2e71y_bdic_PD_TRI_LED_get_clrvari_index(init_param->handset_color);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_lposc_on                                                  */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_lposc_on(void)
{
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_lposc_enable);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_lposc_off                                                 */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_lposc_off(void)
{
    IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_lposc_disable);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_set_request                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_set_request(struct ir2e71y_tri_led *tmp)
{
    int color = 0x00;

    color = (tmp->blue << 2) | (tmp->green << 1) | tmp->red;

    ir2e71y_bdic_tri_led_mode        = tmp->led_mode;
    ir2e71y_bdic_tri_led_before_mode = tmp->led_mode;
    ir2e71y_bdic_tri_led_color       = color;
    ir2e71y_bdic_tri_led_ontime      = tmp->ontime;
    ir2e71y_bdic_tri_led_interval    = tmp->interval;
    ir2e71y_bdic_tri_led_count       = tmp->count;

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_tri_led_mode_twin        = tmp->led_mode;
#endif /* IR2E71Y_COLOR_LED_TWIN */
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_off                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_TRI_LED_off(void)
{
    int ret;
    ret = ir2e71y_bdic_seq_led_off();
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_get_color_index_and_reedit                        */
/* ------------------------------------------------------------------------- */
unsigned char ir2e71y_bdic_API_TRI_LED_get_color_index_and_reedit(struct ir2e71y_tri_led *tri_led)
{
    unsigned char color=0;

    color = ir2e71y_bdic_get_color_index_and_reedit(tri_led);

    return color;
}

#ifdef IR2E71Y_KEY_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_KEY_LED_get_color_index_and_reedit                        */
/* ------------------------------------------------------------------------- */
unsigned char ir2e71y_bdic_API_KEY_LED_get_color_index_and_reedit(struct ir2e71y_key_bkl_ctl *key_bkl_ctl )
{
    int i;
    unsigned char index = 0xFF;
    struct ir2e71y_key_bkl_ctl key_bkl;

    key_bkl = *key_bkl_ctl;

    if (key_bkl.key_left == IR2E71Y_KEY_BKL_DIM) {
        key_bkl.key_left = IR2E71Y_KEY_BKL_NORMAL;
    }
    if (key_bkl.key_center == IR2E71Y_KEY_BKL_DIM) {
        key_bkl.key_center = IR2E71Y_KEY_BKL_NORMAL;
    }
    if (key_bkl.key_right == IR2E71Y_KEY_BKL_DIM) {
       key_bkl.key_right = IR2E71Y_KEY_BKL_NORMAL;
    }

    for (i = 0; i < ARRAY_SIZE(ir2e71y_key_led_index_tbl); i++) {
        if ((ir2e71y_key_led_index_tbl[i].key_left== key_bkl.key_left) &&
            (ir2e71y_key_led_index_tbl[i].key_center== key_bkl.key_center) &&
            (ir2e71y_key_led_index_tbl[i].key_right== key_bkl.key_right)) {
            index = ir2e71y_key_led_index_tbl[i].index;
            break;
        }
    }

    if (index == 0xFF) {
        if (key_bkl.key_left > 1) {
            key_bkl.key_left = 1;
        }
        if (key_bkl.key_center > 1) {
            key_bkl.key_center = 1;
        }
        if (key_bkl.key_right > 1) {
            key_bkl.key_right = 1;
        }
        for (i = 0; i < ARRAY_SIZE(ir2e71y_key_led_index_tbl); i++) {
            if ((ir2e71y_key_led_index_tbl[i].key_left == key_bkl.key_left) &&
                (ir2e71y_key_led_index_tbl[i].key_center == key_bkl.key_center) &&
                (ir2e71y_key_led_index_tbl[i].key_right == key_bkl.key_right)) {
                index = ir2e71y_key_led_index_tbl[i].index;
                break;
            }
        }
        if (index == 0xFF) {
            index = 0;
        }
    }
    return index;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_KEY_LED_ctl                                               */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_KEY_LED_ctl(unsigned char dim, unsigned char index, int ontime, int interval)
{
    ir2e71y_bdic_seq_key_led_ctl(dim, index, ontime, interval);
    return;
}
#endif /* IR2E71Y_KEY_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_normal_on                                         */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_TRI_LED_normal_on(unsigned char color)
{
    int ret;
    ret = ir2e71y_bdic_seq_led_normal_on(color);
    return ret;
}

#ifdef IR2E71Y_SYSFS_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LED_blink_on                                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_LED_blink_on(int no, unsigned char color, struct ir2e71y_tri_led led)
{
    if (no == SYSFS_LED_SH_LED_1) {
        ir2e71y_bdic_seq_led_blink_on(color, led.ontime, led.interval, led.count);
    } else {
        /* not support SYSFS_LED_SH_LED_2 */
        return IR2E71Y_RESULT_FAILURE;
    }

    return IR2E71Y_RESULT_SUCCESS;
}
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LED_on                                                    */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_LED_on(int no, struct ir2e71y_tri_led led)
{
    int ret;
    ret = ir2e71y_bdic_seq_led_on(no, led);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LED_off                                                   */
/* ------------------------------------------------------------------------- */
int ir2e71y_bdic_API_LED_off(int no)
{
    int ret;

    ret = ir2e71y_bdic_seq_leds_off(no);
    return ret;
}
#endif /* IR2E71Y_SYSFS_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_blink_on                                          */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_blink_on(unsigned char color, int ontime, int interval, int count)
{
    ir2e71y_bdic_seq_led_blink_on(color, ontime, interval, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_firefly_on                                        */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_firefly_on(unsigned char color, int ontime, int interval, int count)
{
    ir2e71y_bdic_seq_led_firefly_on(color, ontime, interval, count);
    return;
}

#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_high_speed_on                                     */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_high_speed_on(unsigned char color, int interval, int count)
{
    ir2e71y_bdic_seq_led_high_speed_on(color, IR2E71Y_BDIC_TRI_LED_INTERVAL_HISPEED, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_standard_on                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_standard_on(unsigned char color, int interval, int count)
{
    ir2e71y_bdic_seq_led_standard_on(color, IR2E71Y_BDIC_TRI_LED_INTERVAL_STANDARD, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_breath_on                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_breath_on(unsigned char color, int interval, int count)
{
    ir2e71y_bdic_seq_led_breath_on(color, IR2E71Y_BDIC_TRI_LED_INTERVAL_BREATH, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_long_breath_on                                    */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_long_breath_on(unsigned char color, int interval, int count)
{
    ir2e71y_bdic_seq_led_long_breath_on(color, IR2E71Y_BDIC_TRI_LED_INTERVAL_LONG_BREATH, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_wave_on                                           */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_wave_on(unsigned char color, int interval, int count)
{
    ir2e71y_bdic_seq_led_wave_on(color, IR2E71Y_BDIC_TRI_LED_INTERVAL_WAVE, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_flash_on                                          */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_flash_on(unsigned char color, int interval, int count)
{
    ir2e71y_bdic_seq_led_flash_on(color, IR2E71Y_BDIC_TRI_LED_INTERVAL_FLASH, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_aurora_on                                         */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_aurora_on(int interval, int count)
{
    ir2e71y_bdic_seq_led_aurora_on(IR2E71Y_BDIC_TRI_LED_INTERVAL_AURORA, count);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_rainbow_on                                        */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_rainbow_on(int interval, int count)
{
    ir2e71y_bdic_seq_led_rainbow_on(IR2E71Y_BDIC_TRI_LED_INTERVAL_RAINBOW, count);
    return;
}
#endif /* IR2E71Y_ILLUMI_COLOR_LED */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LED_set_illumi_triple_color                               */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LED_set_illumi_triple_color(struct ir2e71y_illumi_triple_color illumi_triple_color)
{
    ir2e71y_bdic_seq_illumi_triple_color_on(illumi_triple_color);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LED_clear_illumi_triple_color                             */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_LED_clear_illumi_triple_color(void)
{
    ir2e71y_bdic_seq_illumi_cancel_and_clear();
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_LED_is_running_illumi_triple_color                        */
/* ------------------------------------------------------------------------- */
bool ir2e71y_bdic_API_LED_is_running_illumi_triple_color(void)
{
    return illumi_state.running_state;
}
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_emopattern_on                                     */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_emopattern_on(int interval, int count)
{
    ir2e71y_bdic_seq_led_emopattern_on(IR2E71Y_BDIC_TRI_LED_INTERVAL_EMOPATTERN, IR2E71Y_BDIC_TRI_LED_COUNT_EMOPATTERN);
    return;
}
#endif  /* IR2E71Y_ANIME_COLOR_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_exit                                              */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_exit(void)
{
#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
    if (illumi_state.workqueue) {
        destroy_workqueue(illumi_state.workqueue);
        illumi_state.workqueue = NULL;
    }
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */
}

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED_INFO_output                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;

    pbuf = (unsigned char *)kzalloc((BDIC_REG_CH2_C - BDIC_REG_SEQ_ANIME + 1), GFP_KERNEL);
    if (!pbuf) {
        IR2E71Y_ERR("kzalloc failed. size=%d", (BDIC_REG_CH2_C - BDIC_REG_SEQ_ANIME + 1));
        return;
    }
    p = pbuf;
    for (idx = BDIC_REG_SEQ_ANIME; idx <= BDIC_REG_CH2_C; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }

    printk("[BL71Y8] TRI-LED INFO ->>\n");
    printk("[BL71Y8] led_state_str.handset_color       = %d.\n", led_state_str.handset_color);
    printk("[BL71Y8] led_state_str.bdic_clrvari_index  = %d.\n", led_state_str.bdic_clrvari_index);
    printk("[BL71Y8] led_state_str.bdic_chipver        = %d.\n", led_state_str.bdic_chipver);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_color         = %d.\n", (int)ir2e71y_bdic_tri_led_color);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_mode          = %d.\n", ir2e71y_bdic_tri_led_mode);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_ontime        = %d.\n", ir2e71y_bdic_tri_led_ontime);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_interval      = %d.\n", ir2e71y_bdic_tri_led_interval);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_count         = %d.\n", ir2e71y_bdic_tri_led_count);

#ifdef IR2E71Y_SYSFS_LED
    printk("[BL71Y8] rgb_current1[0]                   = %d.\n", rgb_current1[0]);
    printk("[BL71Y8] rgb_current1[1]                   = %d.\n", rgb_current1[1]);
    printk("[BL71Y8] rgb_current1[2]                   = %d.\n", rgb_current1[2]);
#endif /* IR2E71Y_SYSFS_LED */

    p = pbuf;
    printk("[BL71Y8] BDIC_REG_TIMER_SETTING 0x%2X: %02x %02x %02x\n", BDIC_REG_SEQ_ANIME, *p, *(p + 1), *(p + 2));
    p += 3;
    printk("[BL71Y8] BDIC_REG_LED_SETTING   0x%2X: %02x %02x %02x %02x %02x %02x %02x\n",
                BDIC_REG_CH0_SET1, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6));
    p += 7;
    printk("[BL71Y8] BDIC_REG_LED_CURRENT   0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                BDIC_REG_CH0_A, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7), *(p + 8));

    kfree(pbuf);

    printk("[BL71Y8] TRI-LED INFO <<-\n");

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_TRI_LED_INFO_output_twin();
#endif /* IR2E71Y_COLOR_LED_TWIN */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
    ir2e71y_bdic_illumi_triple_color_INFO_output();
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_API_TRI_LED2_INFO_output                                      */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_API_TRI_LED2_INFO_output(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;
    unsigned short   ir2e71y_log_lv_bk;
    size_t  size;

    size  = (BDIC_REG_TIMER2 - BDIC_REG_SEQ_ANIME + 1);
    size += (BDIC_REG_CH5_C - BDIC_REG_CH3_SET1 + 1);

    pbuf = (unsigned char *)kzalloc(size, GFP_KERNEL);
    if (!pbuf) {
        IR2E71Y_ERR("kzalloc failed. size=%zd", size);
        return;
    }

    ir2e71y_log_lv_bk = ir2e71y_log_lv;
    ir2e71y_log_lv = IR2E71Y_LOG_LV_ERR;
    ir2e71y_bdic_API_IO_bank_set(0x00);

    p = pbuf;
    for (idx = BDIC_REG_SEQ_ANIME; idx <= BDIC_REG_TIMER2; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    for (idx = BDIC_REG_CH3_SET1; idx <= BDIC_REG_CH5_C; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    ir2e71y_log_lv = ir2e71y_log_lv_bk;

    printk("[BL71Y8] TRI-LED2 INFO ->>\n");
    printk("[BL71Y8] led_state_str.handset_color       = %d.\n", led_state_str.handset_color);
    printk("[BL71Y8] led_state_str.bdic_clrvari_index  = %d.\n", led_state_str.bdic_clrvari_index);
    printk("[BL71Y8] led_state_str.bdic_chipver        = %d.\n", led_state_str.bdic_chipver);

    p = pbuf;
    printk("[BL71Y8] BDIC_REG_TIMER_SETTING 0x%2X: %02x %02x %02x\n", BDIC_REG_SEQ_ANIME, *p, *(p + 1), *(p + 2));
    p += 3;
    printk("[BL71Y8] BDIC_REG_LED2_SETTING  0x%2X: %02x %02x %02x %02x %02x %02x %02x\n",
                BDIC_REG_CH3_SET1, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6));
    p += 7;
    printk("[BL71Y8] BDIC_REG_LED2_CURRENT  0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                BDIC_REG_CH3_A, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7), *(p + 8));

    kfree(pbuf);

    printk("[BL71Y8] TRI-LED2 INFO <<-\n");
    return;
}
#endif /* CONFIG_ANDROID_ENGINEERING */

/* ------------------------------------------------------------------------- */
/*ir2e71y_led_status_init                                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_led_status_init(void)
{
    IR2E71Y_TRACE("in")
    ir2e71y_bdic_tri_led_color    = 0;
    ir2e71y_bdic_tri_led_mode     = IR2E71Y_BDIC_TRI_LED_MODE_OFF;
    ir2e71y_bdic_tri_led_before_mode = IR2E71Y_BDIC_TRI_LED_MODE_OFF;
    ir2e71y_bdic_tri_led_ontime   = 0;
    ir2e71y_bdic_tri_led_interval = 0;
    ir2e71y_bdic_tri_led_count    = 0;
#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_tri_led_mode_twin           = IR2E71Y_BDIC_TRI_LED_MODE_OFF;
#endif /* IR2E71Y_COLOR_LED_TWIN */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
    memset(&illumi_state, 0, sizeof(illumi_state));
    illumi_state.illumi_state = ILLUMI_STATE_STOP;
    illumi_state.running_state = false;

    illumi_state.workqueue = create_singlethread_workqueue("ir2e71y_illumi");
    if (!illumi_state.workqueue) {
        IR2E71Y_ERR("create ir2e71y_illumi workqueue failed.");
    } else {
        INIT_DELAYED_WORK(&illumi_state.works[ILLUMI_STATE_WAIT_SET_B2_AREA],   ir2e71y_workqueue_handler_illumi_set_b2);
        INIT_DELAYED_WORK(&illumi_state.works[ILLUMI_STATE_WAIT_SET_C2_AREA],   ir2e71y_workqueue_handler_illumi_set_c2);
        INIT_DELAYED_WORK(&illumi_state.works[ILLUMI_STATE_WAIT_SET_A3_AREA],   ir2e71y_workqueue_handler_illumi_set_a3);
        INIT_DELAYED_WORK(&illumi_state.works[ILLUMI_STATE_WAIT_ANIME_BREAK],   ir2e71y_workqueue_handler_illumi_set_anime_stop);
        INIT_DELAYED_WORK(&illumi_state.works[ILLUMI_STATE_WAIT_RESTART],       ir2e71y_workqueue_handler_illumi_restart);
    }
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */
    IR2E71Y_TRACE("out")
    return;
}

#ifdef IR2E71Y_SYSFS_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_on                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_seq_led_on(int no, struct ir2e71y_tri_led led)
{
    bool is_led_current_mode;

    IR2E71Y_TRACE("in no:%d", no);

    is_led_current_mode = ir2e71y_bdic_is_led_current_mode();

    IR2E71Y_DEBUG("is_led_current_mode:%d", is_led_current_mode);

    if (no == SYSFS_LED_SH_LED_1) {
        rgb_current1[0] = ir2e71y_bdic_LD_correction_led_fix_clrvari(led.red,   SYSFS_LED_SH_RED);
        rgb_current1[1] = ir2e71y_bdic_LD_correction_led_fix_clrvari(led.green, SYSFS_LED_SH_GREEN);
        rgb_current1[2] = ir2e71y_bdic_LD_correction_led_fix_clrvari(led.blue,  SYSFS_LED_SH_BLUE);
#ifdef IR2E71Y_COLOR_LED_TWIN
    } else {
        rgb_current2[0] = ir2e71y_bdic_LD_correction_led_fix_clrvari(led.red,   SYSFS_LED_SH_RED);
        rgb_current2[1] = ir2e71y_bdic_LD_correction_led_fix_clrvari(led.green, SYSFS_LED_SH_GREEN);
        rgb_current2[2] = ir2e71y_bdic_LD_correction_led_fix_clrvari(led.blue,  SYSFS_LED_SH_BLUE);
#endif /* IR2E71Y_COLOR_LED_TWIN */
    }

    if (!is_led_current_mode) {
        ir2e71y_bdic_seq_led_current_on();
    } else {
        if (no == SYSFS_LED_SH_LED_1) {
            ir2e71y_bdic_LD_set_led_fix_current_table(rgb_current1);
#ifdef IR2E71Y_COLOR_LED_TWIN
        } else {
            ir2e71y_bdic_LD_set_led_fix_current_table_twin(rgb_current2);
#endif /* IR2E71Y_COLOR_LED_TWIN */
        }
    }
    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_leds_off                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_seq_leds_off(int no)
{
    IR2E71Y_TRACE("in no:%d", no);

    if (!ir2e71y_bdic_is_led_current_mode_no(no)) {
        ir2e71y_bdic_seq_led_off();
        ir2e71y_bdic_clear_current_param();
    } else {
        if (no == SYSFS_LED_SH_LED_1) {
            memset(rgb_current1, 0x00, sizeof(rgb_current1));
            ir2e71y_bdic_LD_set_led_fix_current_table(rgb_current1);
#ifdef IR2E71Y_COLOR_LED_TWIN
        } else {
            memset(rgb_current2, 0x00, sizeof(rgb_current2));
            ir2e71y_bdic_LD_set_led_fix_current_table_twin(rgb_current2);
#endif /* IR2E71Y_COLOR_LED_TWIN */
        }
    }

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_current_on                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_current_on(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL, 0);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, CURRENT_SET);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL, 0);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, CURRENT_SET);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    IR2E71Y_TRACE("out");
    return;
}

#endif /* IR2E71Y_SYSFS_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_off                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_seq_led_off(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_STOP, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_STOP, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_normal_on                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_seq_led_normal_on(unsigned char color)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_blink_on                                              */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_blink_on(unsigned char color, int ontime, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BLINK,   color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME,       ontime);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BLINK,   color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME,       ontime);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_firefly_on                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_firefly_on(unsigned char color, int ontime, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME,       ontime);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME,       ontime);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_high_speed_on                                         */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_high_speed_on(unsigned char color, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_HISPEED, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_HISPEED, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_standard_on                                           */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_standard_on(unsigned char color, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_STANDARD, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_STANDARD, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_breath_on                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_breath_on(unsigned char color, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BREATH, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BREATH, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_long_breath_on                                        */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_long_breath_on(unsigned char color, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_wave_on                                               */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_wave_on(unsigned char color, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_WAVE, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_WAVE, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_flash_on                                              */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_flash_on(unsigned char color, int interval, int count)
{
    IR2E71Y_TRACE("in color:%d", color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FLASH, color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FLASH, color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_aurora_on                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_aurora_on(int interval, int count)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_AURORA, IR2E71Y_BDIC_TRI_LED_COLOR_WHITE);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_AURORA, IR2E71Y_BDIC_TRI_LED_COLOR_WHITE);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_rainbow_on                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_rainbow_on(int interval, int count)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW, IR2E71Y_BDIC_TRI_LED_COLOR_WHITE);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW, IR2E71Y_BDIC_TRI_LED_COLOR_WHITE);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}
#endif /* IR2E71Y_ILLUMI_COLOR_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_led_emopattern_on                                         */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_led_emopattern_on(int interval, int count)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_EMOPATTERN, IR2E71Y_BDIC_TRI_LED_COLOR_MAGENTA);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_EMOPATTERN, IR2E71Y_BDIC_TRI_LED_COLOR_MAGENTA);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,     interval);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,        count);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (interval != 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
        }
    }
    IR2E71Y_TRACE("out");
}
#endif  /* IR2E71Y_ANIME_COLOR_LED */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_illumi_triple_color                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_illumi_triple_color(unsigned char first_color)
{
    IR2E71Y_TRACE("in color:%d", first_color);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_LED);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_ILLUMI_TRIPLE_COLOR, first_color);
    ir2e71y_bdic_PD_TRI_LED_control(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_ILLUMI_TRIPLE_COLOR, first_color);
    ir2e71y_bdic_PD_TRI_LED_control_twin(IR2E71Y_BDIC_REQ_TRI_LED_START, 0);
#endif /* IR2E71Y_COLOR_LED_TWIN */

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_illumi_triple_color_on                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_illumi_triple_color_on(struct ir2e71y_illumi_triple_color illumi_triple_color)
{
    struct ir2e71y_tri_led tri_led;
    int i;
    int work_count;

    IR2E71Y_TRACE("in");
    if (!illumi_state.workqueue) {
        IR2E71Y_ERR("illumi_state.workqueue does not exist err.");
        return;
    }

    for (i = 0; i != ILLUMI_FRAME_MAX; i++) {
        tri_led.red   = illumi_triple_color.colors[i].red;
        tri_led.green = illumi_triple_color.colors[i].green;
        tri_led.blue  = illumi_triple_color.colors[i].blue;
        illumi_state.colors[i] = ir2e71y_bdic_get_color_index_and_reedit(&tri_led);
    }
    illumi_state.count = illumi_triple_color.count;
    illumi_state.running_state = true;

    ir2e71y_bdic_seq_illumi_triple_color(illumi_state.colors[ILLUMI_FRAME_FIRST]);

    work_count = (illumi_state.count == IR2E71Y_TRI_LED_COUNT_NONE) ? ILLUMI_STATE_MAX : ILLUMI_STATE_MAX - 1;
    IR2E71Y_DEBUG("queue delay_works = %d isonshot = %d", work_count, illumi_state.count);
    for (i = 0; i != work_count; ++i) {
        queue_delayed_work(illumi_state.workqueue, &illumi_state.works[i], usecs_to_jiffies(ir2e71y_illumi_delayed_times[i]));
        IR2E71Y_DEBUG("delay_works[%d] was queued", i);
    }

    ir2e71y_IO_API_msleep(10);
    ir2e71y_bdic_illumi_color_set_a2();
    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_illumi_cancel_and_clear                                   */
/* ------------------------------------------------------------------------- */
void ir2e71y_bdic_seq_illumi_cancel_and_clear(void)
{
    IR2E71Y_TRACE("in");

    IR2E71Y_DEBUG("illumi_state.running_state = %d", illumi_state.running_state);
    if (illumi_state.running_state) {
        ir2e71y_API_semaphore_start();
        illumi_state.running_state = false;
        ir2e71y_API_semaphore_end();

        ir2e71y_bdic_cancel_illumi_work();

        ir2e71y_API_semaphore_start();
        ir2e71y_bdic_clear_illumi_state();
        ir2e71y_API_semaphore_end();
    }

    IR2E71Y_TRACE("out");
}
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_write_illumi_triple_color_top                                 */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_write_illumi_triple_color_top(char reg, int color_index)
{
    char color_rgb[3];
    unsigned char extend_color_index;
    int clrvari = led_state_str.bdic_clrvari_index;
#ifdef IR2E71Y_COLOR_LED_TWIN
    char reg_twin = 0;
    char color_rgb_twin[3];
#endif

#ifdef IR2E71Y_COLOR_LED_TWIN
    switch (reg) {
    case BDIC_REG_CH0_A:
       reg_twin = BDIC_REG_CH3_A;
       break;
    case BDIC_REG_CH0_B:
       reg_twin = BDIC_REG_CH3_B;
       break;
    case BDIC_REG_CH0_C:
    default:
       reg_twin = BDIC_REG_CH3_C;
    }
#endif

#ifdef IR2E71Y_EXTEND_COLOR_LED
    if (color_index > IR2E71Y_TRI_LED_COLOR_NUM) {
        extend_color_index = color_index - IR2E71Y_TRI_LED_COLOR_NUM;
        color_rgb[0] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][0];
        color_rgb[1] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][1];
        color_rgb[2] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][2];
#ifdef IR2E71Y_COLOR_LED_TWIN
        color_rgb_twin[0] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][0];
        color_rgb_twin[1] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][1];
        color_rgb_twin[2] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][2];
#endif
    } else {
        color_rgb[0] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][0];
        color_rgb[1] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][1];
        color_rgb[2] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][2];
#ifdef IR2E71Y_COLOR_LED_TWIN
        color_rgb_twin[0] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][0];
        color_rgb_twin[1] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][1];
        color_rgb_twin[2] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][2];
#endif
    }
#else /* IR2E71Y_EXTEND_COLOR_LED */
    color_rgb[0] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][0];
    color_rgb[1] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][1];
    color_rgb[2] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][2];
#ifdef IR2E71Y_COLOR_LED_TWIN
    color_rgb_twin[0] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][0];
    color_rgb_twin[1] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][1];
    color_rgb_twin[2] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][2];
#endif
#endif /* IR2E71Y_EXTEND_COLOR_LED */

    ir2e71y_bdic_API_IO_multi_write_reg(reg, color_rgb, 3);
#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_API_IO_multi_write_reg(reg_twin, color_rgb_twin, 3);
#endif
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_write_illumi_triple_color_bottom                              */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_write_illumi_triple_color_bottom(char reg, int color_index)
{
    char color_rgb[3];
#ifdef IR2E71Y_COLOR_LED_TWIN
    char reg_twin = 0;
#endif
    memset(color_rgb, 0, sizeof(color_rgb));

#ifdef IR2E71Y_COLOR_LED_TWIN
    switch (reg) {
    case BDIC_REG_CH0_A:
       reg_twin = BDIC_REG_CH3_A;
       break;
    case BDIC_REG_CH0_B:
       reg_twin = BDIC_REG_CH3_B;
       break;
    case BDIC_REG_CH0_C:
    default:
       reg_twin = BDIC_REG_CH3_C;
    }
#endif

    ir2e71y_bdic_API_IO_multi_write_reg(reg, color_rgb, 3);
#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_API_IO_multi_write_reg(reg_twin, color_rgb, 3);
#endif
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_cancel_illumi_work                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_cancel_illumi_work(void)
{

    int i;
    int queued_tail;
    int queued_head = illumi_state.illumi_state;

    if (queued_head == ILLUMI_STATE_STOP) {
        return;
    }

    queued_tail = (illumi_state.count == IR2E71Y_TRI_LED_COUNT_NONE) ? ILLUMI_STATE_MAX : ILLUMI_STATE_MAX -1;
    i = illumi_state.illumi_state;

    for (; i != queued_tail; i++) {
        IR2E71Y_DEBUG("cancel work[%d]", i);
        cancel_delayed_work_sync(&illumi_state.works[i]);
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_clear_illumi_state                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_clear_illumi_state(void)
{
    illumi_state.colors[ILLUMI_FRAME_FIRST] = illumi_state.colors[ILLUMI_FRAME_SECOND] = illumi_state.colors[ILLUMI_FRAME_THIRD] = 0;
    illumi_state.count = IR2E71Y_TRI_LED_COUNT_NONE;
    illumi_state.illumi_state = ILLUMI_STATE_STOP;
    illumi_state.running_state = false;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_illumi_color_set_a2                                           */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_illumi_color_set_a2(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_write_illumi_triple_color_top(BDIC_REG_CH0_A, illumi_state.colors[ILLUMI_FRAME_SECOND]);
    illumi_state.illumi_state = ILLUMI_STATE_WAIT_SET_B2_AREA;

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_illumi_set_b2                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_illumi_set_b2(struct work_struct *work)
{
    IR2E71Y_TRACE("in");

    ir2e71y_API_semaphore_start();

    if (!illumi_state.running_state) {
        IR2E71Y_DEBUG("out running_state = %d", illumi_state.running_state);
        ir2e71y_API_semaphore_end();
        return;
    }

    ir2e71y_bdic_write_illumi_triple_color_bottom(BDIC_REG_CH0_B, ILLUMI_FRAME_SECOND);

    illumi_state.illumi_state = ILLUMI_STATE_WAIT_SET_C2_AREA;

    ir2e71y_API_semaphore_end();

    IR2E71Y_TRACE("out");


}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_illumi_set_c2                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_illumi_set_c2(struct work_struct *work)
{
    IR2E71Y_TRACE("in");

    ir2e71y_API_semaphore_start();

    if (!illumi_state.running_state) {
        IR2E71Y_DEBUG("out running_state = %d", illumi_state.running_state);
        ir2e71y_API_semaphore_end();
        return;
    }

    ir2e71y_bdic_write_illumi_triple_color_top(BDIC_REG_CH0_C, illumi_state.colors[ILLUMI_FRAME_THIRD]);

    illumi_state.illumi_state = ILLUMI_STATE_WAIT_SET_A3_AREA;

    ir2e71y_API_semaphore_end();

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_illumi_set_a3                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_illumi_set_a3(struct work_struct *work)
{
    IR2E71Y_TRACE("in");

    ir2e71y_API_semaphore_start();

    if (!illumi_state.running_state) {
        IR2E71Y_DEBUG("out running_state = %d", illumi_state.running_state);
        ir2e71y_API_semaphore_end();
        return;
    }

    ir2e71y_bdic_write_illumi_triple_color_bottom(BDIC_REG_CH0_A, ILLUMI_FRAME_THIRD);

    illumi_state.illumi_state = ILLUMI_STATE_WAIT_ANIME_BREAK;

    ir2e71y_API_semaphore_end();

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_illumi_set_anime_stop                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_illumi_set_anime_stop(struct work_struct *work)
{
    IR2E71Y_TRACE("in");

    ir2e71y_API_semaphore_start();

    if (!illumi_state.running_state) {
        IR2E71Y_DEBUG("out running_state = %d", illumi_state.running_state);
        ir2e71y_API_semaphore_end();
        return;
    }

    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_SYSTEM7, 0x00);

    if (illumi_state.count == IR2E71Y_TRI_LED_COUNT_1) {
        ir2e71y_bdic_clear_illumi_state();
        ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_LED);
    } else {
        illumi_state.illumi_state = ILLUMI_STATE_WAIT_RESTART;
    }

    ir2e71y_API_semaphore_end();

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_illumi_restart                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_illumi_restart(struct work_struct *work)
{
    int clrvari = led_state_str.bdic_clrvari_index;
    int i;
    int work_count;
    unsigned char color_rgb[9];
#ifdef IR2E71Y_COLOR_LED_TWIN
    unsigned char color_rgb_twin[9];
#endif
    unsigned char color_index;

    IR2E71Y_TRACE("in");

    ir2e71y_API_semaphore_start();

    if (!illumi_state.running_state) {
        IR2E71Y_DEBUG("out running_state = %d", illumi_state.running_state);
        ir2e71y_API_semaphore_end();
        return;
    }

#ifdef IR2E71Y_EXTEND_COLOR_LED
    if (illumi_state.colors[ILLUMI_FRAME_FIRST] > IR2E71Y_TRI_LED_COLOR_NUM) {
        color_index = illumi_state.colors[ILLUMI_FRAME_FIRST] - IR2E71Y_TRI_LED_COLOR_NUM;
        color_rgb[0] = 0;
        color_rgb[1] = 0;
        color_rgb[2] = 0;
        color_rgb[3] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][color_index][0];
        color_rgb[4] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][color_index][1];
        color_rgb[5] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][color_index][2];
        color_rgb[6] = 0;
        color_rgb[7] = 0;
        color_rgb[8] = 0;
#ifdef IR2E71Y_COLOR_LED_TWIN
        color_rgb_twin[0] = 0;
        color_rgb_twin[1] = 0;
        color_rgb_twin[2] = 0;
        color_rgb_twin[3] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][color_index][0];
        color_rgb_twin[4] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][color_index][1];
        color_rgb_twin[5] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][color_index][2];
        color_rgb_twin[6] = 0;
        color_rgb_twin[7] = 0;
        color_rgb_twin[8] = 0;
#endif
    } else {
        color_index = illumi_state.colors[ILLUMI_FRAME_FIRST];
        color_rgb[0] = 0;
        color_rgb[1] = 0;
        color_rgb[2] = 0;
        color_rgb[3] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][0];
        color_rgb[4] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][1];
        color_rgb[5] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][2];
        color_rgb[6] = 0;
        color_rgb[7] = 0;
        color_rgb[8] = 0;
#ifdef IR2E71Y_COLOR_LED_TWIN
        color_rgb_twin[0] = 0;
        color_rgb_twin[1] = 0;
        color_rgb_twin[2] = 0;
        color_rgb_twin[3] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][0];
        color_rgb_twin[4] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][1];
        color_rgb_twin[5] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][2];
        color_rgb_twin[6] = 0;
        color_rgb_twin[7] = 0;
        color_rgb_twin[8] = 0;
#endif
    }
#else /* IR2E71Y_EXTEND_COLOR_LED */
    color_index = illumi_state.colors[ILLUMI_FRAME_FIRST];
    color_rgb[0] = 0;
    color_rgb[1] = 0;
    color_rgb[2] = 0;
    color_rgb[3] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][0];
    color_rgb[4] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][1];
    color_rgb[5] = ir2e71y_triple_led_anime_tbl[clrvari][1][color_index][2];
    color_rgb[6] = 0;
    color_rgb[7] = 0;
    color_rgb[8] = 0;
#ifdef IR2E71Y_COLOR_LED_TWIN
    color_rgb_twin[0] = 0;
    color_rgb_twin[1] = 0;
    color_rgb_twin[2] = 0;
    color_rgb_twin[3] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][0];
    color_rgb_twin[4] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][1];
    color_rgb_twin[5] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][color_index][2];
    color_rgb_twin[6] = 0;
    color_rgb_twin[7] = 0;
    color_rgb_twin[8] = 0;
#endif
#endif /* IR2E71Y_EXTEND_COLOR_LED */

    ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH0_A, color_rgb, 9);
#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH3_A, color_rgb_twin, 9);
#endif

#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_SYSTEM7, 0x05);
#else
    ir2e71y_bdic_API_IO_write_reg(BDIC_REG_SYSTEM7, 0x01);
#endif

    work_count = (illumi_state.count == IR2E71Y_TRI_LED_COUNT_NONE) ? ILLUMI_STATE_MAX : ILLUMI_STATE_MAX - 1;
    IR2E71Y_DEBUG("restart queue delay_works = %d isonshot = %d", work_count, illumi_state.count);
    for (i = 0; i != work_count; ++i) {
        queue_delayed_work(illumi_state.workqueue, &illumi_state.works[i], usecs_to_jiffies(ir2e71y_illumi_delayed_times[i]));
        IR2E71Y_DEBUG("delay_works[%d] was queued", i);
    }

    ir2e71y_IO_API_msleep(10);
    ir2e71y_bdic_illumi_color_set_a2();

    illumi_state.illumi_state = ILLUMI_STATE_WAIT_SET_B2_AREA;

    ir2e71y_API_semaphore_end();

    IR2E71Y_TRACE("out");
}
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_led_fix_on_table                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_set_led_fix_on_table(int clr_vari, int color)
{
    unsigned char *pTriLed;

#ifdef IR2E71Y_EXTEND_COLOR_LED
    unsigned char extend_color_index;

    if (color > IR2E71Y_TRI_LED_COLOR_NUM) {
        extend_color_index = color - IR2E71Y_TRI_LED_COLOR_NUM;
        pTriLed = (unsigned char *)(&(ir2e71y_triple_led_extend_tbl[clr_vari][extend_color_index]));
    } else {
        pTriLed = (unsigned char *)(&(ir2e71y_triple_led_tbl[clr_vari][color]));
    }
#else /* IR2E71Y_EXTEND_COLOR_LED */
    pTriLed = (unsigned char *)(&(ir2e71y_triple_led_tbl[clr_vari][color]));
#endif /* IR2E71Y_EXTEND_COLOR_LED */

    ir2e71y_bdic_LD_set_led_on_table(pTriLed);
}
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_key_led_ctl                                               */
/* ------------------------------------------------------------------------- */
#ifdef IR2E71Y_KEY_LED
static void ir2e71y_bdic_seq_key_led_ctl(unsigned char dim, unsigned char index, int ontime, int interval)
{
    IR2E71Y_TRACE("in index:%d\n", index);

    ir2e71y_bdic_seq_bdic_active_for_led(IR2E71Y_DEV_TYPE_KEYLED);
    ir2e71y_bdic_PD_KEY_LED_control(dim, index, ontime, interval);

    if (led_state_str.bdic_chipver == IR2E71Y_BDIC_CHIPVER_0) {
        if (index == 0) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_KEYLED);
        }
    } else if (led_state_str.bdic_chipver >= IR2E71Y_BDIC_CHIPVER_1) {
        if (index == 0 || ((8 <= index) && (index <= 14))) {
            ir2e71y_bdic_seq_bdic_standby_for_led(IR2E71Y_DEV_TYPE_KEYLED);
        }
    } else {
    }

    IR2E71Y_TRACE("out\n");

}
#endif  /* IR2E71Y_KEY_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_led_on_table                                           */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_set_led_on_table(unsigned char *rgb_current)
{

    unsigned char *pTriLed;
    ir2e71y_bdicRegSetting_t led_fix_on[ARRAY_SIZE(ir2e71y_bdic_led_fix_on)];

    memcpy(led_fix_on, ir2e71y_bdic_led_fix_on, sizeof(ir2e71y_bdic_led_fix_on));

    pTriLed = (unsigned char *)rgb_current;

    led_fix_on[IR2E71Y_LED_FIX_ONR].data = *(pTriLed + 0);
    led_fix_on[IR2E71Y_LED_FIX_ONG].data = *(pTriLed + 1);
    led_fix_on[IR2E71Y_LED_FIX_ONB].data = *(pTriLed + 2);
    IR2E71Y_BDIC_REGSET(led_fix_on);
}

#ifdef IR2E71Y_SYSFS_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_correction_led_fix_clrvari                                 */
/* ------------------------------------------------------------------------- */
static unsigned char ir2e71y_bdic_LD_correction_led_fix_clrvari(unsigned char brightness, int color)
{
    unsigned int data = 0;
    unsigned char factor_value = ir2e71y_clrvari_correction_led_tbl[led_state_str.bdic_clrvari_index][color];

    if (brightness == 0) {
        return 0;
    }

    data = (unsigned int)brightness * (unsigned int)factor_value;
    if (data < 255) {
        data = 255;
    }
    data = data / 255;
    IR2E71Y_DEBUG("brightness:%d to %d", brightness, data);

    return (unsigned char)data;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_led_fix_current_table                                  */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_set_led_fix_current_table(unsigned char *rgb_current)
{
    unsigned char *pTriLed;
    ir2e71y_bdicRegSetting_t led_current[ARRAY_SIZE(ir2e71y_bdic_led_current)];

    memcpy(led_current, ir2e71y_bdic_led_current, sizeof(ir2e71y_bdic_led_current));

    pTriLed = (unsigned char *)rgb_current;

    led_current[0].data = *(pTriLed + 0);
    led_current[1].data = *(pTriLed + 1);
    led_current[2].data = *(pTriLed + 2);
    IR2E71Y_BDIC_REGSET(led_current);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_is_led_current_mode                                           */
/* ------------------------------------------------------------------------- */
static bool ir2e71y_bdic_is_led_current_mode(void)
{
    int i;

    for (i = 0; i < IR2E71Y_RGB; i++) {
#ifdef IR2E71Y_COLOR_LED_TWIN
        if (rgb_current1[i] || rgb_current2[i]) {
#else /* IR2E71Y_COLOR_LED_TWIN */
        if (rgb_current1[i]) {
#endif /* IR2E71Y_COLOR_LED_TWIN */
            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_is_led_current_mode_no                                        */
/* ------------------------------------------------------------------------- */
static bool ir2e71y_bdic_is_led_current_mode_no(int no)
{
#ifdef IR2E71Y_COLOR_LED_TWIN
    int i;
    unsigned char *chk_current = ((no == SYSFS_LED_SH_LED_1) ? rgb_current2 : rgb_current1);

    for (i = 0; i < IR2E71Y_RGB; i++) {
        if (chk_current[i]) {
            return true;
        }
    }
#else /* IR2E71Y_COLOR_LED_TWIN */
    if (no != SYSFS_LED_SH_LED_1)
        return true;
#endif /* IR2E71Y_COLOR_LED_TWIN */
    return false;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_clear_current_param                                           */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_clear_current_param(void)
{
    IR2E71Y_TRACE("in");
    memset(rgb_current1, 0x00, sizeof(rgb_current1));
#ifdef IR2E71Y_COLOR_LED_TWIN
    memset(rgb_current2, 0x00, sizeof(rgb_current2));
#endif /* IR2E71Y_COLOR_LED_TWIN */
    IR2E71Y_TRACE("out");
    return;
}
#endif /* IR2E71Y_SYSFS_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_bdic_active_for_led                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_bdic_active_for_led(int dev_type)
{
    IR2E71Y_TRACE("in dev_type:%d", dev_type);
    (void)ir2e71y_pm_API_bdic_power_manager(dev_type, IR2E71Y_DEV_REQ_ON);
    IR2E71Y_TRACE("out");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_seq_bdic_standby_for_led                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_seq_bdic_standby_for_led(int dev_type)
{
    (void)ir2e71y_pm_API_bdic_power_manager(dev_type, IR2E71Y_DEV_REQ_OFF);
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_get_color_index_and_reedit                                    */
/* ------------------------------------------------------------------------- */
static unsigned char ir2e71y_bdic_get_color_index_and_reedit(struct ir2e71y_tri_led *tri_led)
{
    unsigned int i;
    unsigned char color = 0xFF;

#ifdef IR2E71Y_EXTEND_COLOR_LED
    struct ir2e71y_bdic_led_color_index extend_cloler_index[IR2E71Y_TRI_LED_EXTEND_COLOR_TBL_NUM];

    if ((tri_led->red <= 5) && (tri_led->green <= 5) && (tri_led->blue <= 5)) {
        if ((tri_led->red >= 3) || (tri_led->green >= 3) || (tri_led->blue >= 3)) {
            if (tri_led->red == 0x00) {
                memcpy(extend_cloler_index, ir2e71y_triple_led_extend_color_index_tbl0, sizeof(extend_cloler_index));
            } else if (tri_led->red == 0x03) {
                memcpy(extend_cloler_index, ir2e71y_triple_led_extend_color_index_tbl3, sizeof(extend_cloler_index));
            } else if (tri_led->red == 0x04) {
                memcpy(extend_cloler_index, ir2e71y_triple_led_extend_color_index_tbl4, sizeof(extend_cloler_index));
            } else if (tri_led->red == 0x05) {
                memcpy(extend_cloler_index, ir2e71y_triple_led_extend_color_index_tbl5, sizeof(extend_cloler_index));
            }

            for (i = 0; i < ARRAY_SIZE(extend_cloler_index); i++) {
                if (extend_cloler_index[i].green == tri_led->green   &&
                    extend_cloler_index[i].blue  == tri_led->blue) {
                    color = extend_cloler_index[i].color;
                    break;
                }
            }
        } else {
            for (i = 0; i < ARRAY_SIZE(ir2e71y_triple_led_color_index_tbl); i++) {
                if (ir2e71y_triple_led_color_index_tbl[i].red   == tri_led->red     &&
                    ir2e71y_triple_led_color_index_tbl[i].green == tri_led->green   &&
                    ir2e71y_triple_led_color_index_tbl[i].blue  == tri_led->blue) {
                    color = ir2e71y_triple_led_color_index_tbl[i].color;
                    break;
                }
            }
        }
    }
#else /* IR2E71Y_EXTEND_COLOR_LED */
    for (i = 0; i < ARRAY_SIZE(ir2e71y_triple_led_color_index_tbl); i++) {
        if (ir2e71y_triple_led_color_index_tbl[i].red   == tri_led->red     &&
            ir2e71y_triple_led_color_index_tbl[i].green == tri_led->green   &&
            ir2e71y_triple_led_color_index_tbl[i].blue  == tri_led->blue) {
            color = ir2e71y_triple_led_color_index_tbl[i].color;
            break;
        }
    }
#endif /* IR2E71Y_EXTEND_COLOR_LED */

    if (color == 0xFF) {
        if (tri_led->red > 1) {
            tri_led->red = 1;
        }
        if (tri_led->green > 1) {
            tri_led->green = 1;
        }
        if (tri_led->blue > 1) {
            tri_led->blue = 1;
        }
        for (i = 0; i < ARRAY_SIZE(ir2e71y_triple_led_color_index_tbl); i++) {
            if (ir2e71y_triple_led_color_index_tbl[i].red   == tri_led->red     &&
                ir2e71y_triple_led_color_index_tbl[i].green == tri_led->green   &&
                ir2e71y_triple_led_color_index_tbl[i].blue  == tri_led->blue) {
                color = ir2e71y_triple_led_color_index_tbl[i].color;
                break;
            }
        }
        if (color == 0xFF) {
            color = 0;
        }
    }
    return color;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_control                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_control(unsigned char request, int param)
{
    switch (request) {
    case IR2E71Y_BDIC_REQ_TRI_LED_ACTIVE:
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_STANDBY:
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_START:
        IR2E71Y_DEBUG("IR2E71Y_BDIC_REQ_TRI_LED_START tri_led_mode=%d, led_before_mode=%d"
                       , ir2e71y_bdic_tri_led_mode, ir2e71y_bdic_tri_led_before_mode);
#ifdef IR2E71Y_SYSFS_LED
        if (param == NO_CURRENT_SET) {
            ir2e71y_bdic_clear_current_param();
        }
#endif /* IR2E71Y_SYSFS_LED */
        switch (ir2e71y_bdic_tri_led_before_mode) {
        case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
            if (ir2e71y_bdic_tri_led_mode != IR2E71Y_BDIC_TRI_LED_MODE_NORMAL) {
                IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_off_fix);
            }
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_OFF:
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
        case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
        case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
        case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
        case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
        case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
        case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
        case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
        case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
        case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
        case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
        default:
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_off);
            ir2e71y_bdic_PD_TRI_LED_lposc_off();
            break;
        }

        if (ir2e71y_bdic_tri_led_mode == IR2E71Y_BDIC_TRI_LED_MODE_NORMAL) {
#ifdef IR2E71Y_SYSFS_LED
            if (param == CURRENT_SET) {
                ir2e71y_bdic_LD_set_led_on_table(rgb_current1);
            } else
#endif /* IR2E71Y_SYSFS_LED */
            ir2e71y_bdic_LD_set_led_fix_on_table(led_state_str.bdic_clrvari_index, ir2e71y_bdic_tri_led_color);
        } else {
            ir2e71y_pm_API_lpsoc_power_manager(IR2E71Y_DEV_TYPE_LED, IR2E71Y_DEV_REQ_ON);
            ir2e71y_bdic_PD_TRI_LED_set_chdig();
            ir2e71y_bdic_PD_TRI_LED_set_anime();

#ifndef IR2E71Y_COLOR_LED_TWIN
            ir2e71y_bdic_PD_TRI_LED_anime_start();
#endif  /* IR2E71Y_COLOR_LED_TWIN */
        }
        ir2e71y_bdic_tri_led_before_mode = ir2e71y_bdic_tri_led_mode;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_STOP:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_off);
        ir2e71y_bdic_tri_led_mode        = IR2E71Y_BDIC_TRI_LED_MODE_NORMAL;
        ir2e71y_bdic_tri_led_before_mode = IR2E71Y_BDIC_TRI_LED_MODE_OFF;
        ir2e71y_bdic_tri_led_color       = 0;
        ir2e71y_bdic_tri_led_ontime      = 0;
        ir2e71y_bdic_tri_led_interval    = 0;
        ir2e71y_bdic_tri_led_count       = 0;
        ir2e71y_bdic_PD_TRI_LED_lposc_off();
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_NORMAL;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BLINK:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_BLINK;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_HISPEED:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_HISPEED;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_STANDARD:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_STANDARD;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BREATH:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_BREATH;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_WAVE:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_WAVE;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FLASH:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_FLASH;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_EMOPATTERN:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_AURORA:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_AURORA;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_ILLUMI_TRIPLE_COLOR:
        ir2e71y_bdic_tri_led_mode  = IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

#endif  /* IR2E71Y_ANIME_COLOR_LED */

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME:
        ir2e71y_bdic_tri_led_ontime = param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL:
        ir2e71y_bdic_tri_led_interval = param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT:
        ir2e71y_bdic_tri_led_count = param;
        break;

    default:
        break;
    }

    return;
}

#ifndef IR2E71Y_COLOR_LED_TWIN
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_anime_start                                        */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_anime_start(void)
{
    unsigned char timer_val = 0;

    switch (ir2e71y_bdic_tri_led_mode) {
    case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
    case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
    case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
    case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
    case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
    case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
    case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
#endif  /* IR2E71Y_ANIME_COLOR_LED */
        timer_val  = (unsigned char)(ir2e71y_bdic_tri_led_interval << 4);
        timer_val |= (unsigned char)(ir2e71y_bdic_tri_led_count & 0x07);
        ir2e71y_bdic_led_ani_on[1].data  = timer_val;
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_ani_on);
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_ani_on_triple_color);
        break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */

    default:
        break;
    }

    return;
}
#endif  /* IR2E71Y_COLOR_LED_TWIN */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_set_anime                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_set_anime(void)
{
    unsigned char ch_set1_val;
    unsigned char ch_set2_val;

    switch (ir2e71y_bdic_tri_led_mode) {
    case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
        ch_set1_val = 0x4E;
        if (ir2e71y_bdic_tri_led_ontime > IR2E71Y_TRI_LED_ONTIME_TYPE7) {
            ch_set2_val = IR2E71Y_TRI_LED_ONTIME_TYPE1 + (ir2e71y_bdic_tri_led_ontime - IR2E71Y_TRI_LED_ONTIME_TYPE7);
        } else {
            ch_set2_val = (unsigned char)(ir2e71y_bdic_tri_led_ontime);
        }
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH0_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH0_SET2, ch_set2_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH1_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH1_SET2, ch_set2_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH2_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH2_SET2, ch_set2_val);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
        ch_set1_val = 0x06;
        ch_set2_val = (unsigned char)(ir2e71y_bdic_tri_led_ontime);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH0_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH0_SET2, ch_set2_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH1_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH1_SET2, ch_set2_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH2_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH2_SET2, ch_set2_val);
        break;
#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_high_speed_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_standard_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_breath_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_long_breath_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_wave_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_flash_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_emopattern_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_aurora_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_rainbow_on);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_illumi_triple_color_1st);
        break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */

    default:
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_set_chdig                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_set_chdig(void)
{
    int clrvari = led_state_str.bdic_clrvari_index;
    unsigned char wBuf[9];
#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
    unsigned char anime_tbl1[IR2E71Y_COL_VARI_KIND][IR2E71Y_TRI_LED_ANIME_3PAGE][IR2E71Y_TRI_LED_COLOR_TBL_NUM][3];
#endif /* IR2E71Y_ILLUMI_COLOR_LED */
    unsigned char anime_tbl2[IR2E71Y_COL_VARI_KIND][IR2E71Y_TRI_LED_ANIME_3PAGE][3];
#ifdef IR2E71Y_EXTEND_COLOR_LED
    unsigned char extend_color_index;
#endif /* IR2E71Y_EXTEND_COLOR_LED */
#endif  /* IR2E71Y_ANIME_COLOR_LED */

    memset(wBuf, 0, sizeof(wBuf));
    ir2e71y_bdic_API_IO_bank_set(0x00);

    switch (ir2e71y_bdic_tri_led_mode) {
    case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
    case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
    case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
#ifdef IR2E71Y_EXTEND_COLOR_LED
        if (ir2e71y_bdic_tri_led_color > IR2E71Y_TRI_LED_COLOR_NUM) {
            extend_color_index = ir2e71y_bdic_tri_led_color - IR2E71Y_TRI_LED_COLOR_NUM;
            wBuf[0] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][0];
            wBuf[1] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][1];
            wBuf[2] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][2];
            wBuf[3] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][0];
            wBuf[4] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][1];
            wBuf[5] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][2];
            wBuf[6] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][0];
            wBuf[7] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][1];
            wBuf[8] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][2];
        } else {
            wBuf[0] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[1] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[2] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][2];
            wBuf[3] = ir2e71y_triple_led_anime_tbl[clrvari][1][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = ir2e71y_triple_led_anime_tbl[clrvari][1][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = ir2e71y_triple_led_anime_tbl[clrvari][1][ir2e71y_bdic_tri_led_color][2];
            wBuf[6] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[7] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[8] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][2];
        }
#else /* IR2E71Y_EXTEND_COLOR_LED */
        wBuf[0] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][0];
        wBuf[1] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][1];
        wBuf[2] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][2];
        wBuf[3] = ir2e71y_triple_led_anime_tbl[clrvari][1][ir2e71y_bdic_tri_led_color][0];
        wBuf[4] = ir2e71y_triple_led_anime_tbl[clrvari][1][ir2e71y_bdic_tri_led_color][1];
        wBuf[5] = ir2e71y_triple_led_anime_tbl[clrvari][1][ir2e71y_bdic_tri_led_color][2];
        wBuf[6] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][0];
        wBuf[7] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][1];
        wBuf[8] = ir2e71y_triple_led_anime_tbl[clrvari][0][ir2e71y_bdic_tri_led_color][2];
#endif /* IR2E71Y_EXTEND_COLOR_LED */
        if ((ir2e71y_bdic_tri_led_mode == IR2E71Y_BDIC_TRI_LED_MODE_BLINK) &&
            (ir2e71y_bdic_tri_led_ontime > IR2E71Y_TRI_LED_ONTIME_TYPE7)) {
            wBuf[0] = wBuf[6] = wBuf[3];
            wBuf[1] = wBuf[7] = wBuf[4];
            wBuf[2] = wBuf[8] = wBuf[5];
        }

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH0_A, wBuf, 9);
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
    case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
#ifdef IR2E71Y_EXTEND_COLOR_LED
        if (ir2e71y_bdic_tri_led_color > IR2E71Y_TRI_LED_COLOR_NUM) {
            extend_color_index = ir2e71y_bdic_tri_led_color - IR2E71Y_TRI_LED_COLOR_NUM;
            wBuf[3] = ir2e71y_triple_led_extend_anime_high_speed_tbl[clrvari][extend_color_index][0];
            wBuf[4] = ir2e71y_triple_led_extend_anime_high_speed_tbl[clrvari][extend_color_index][1];
            wBuf[5] = ir2e71y_triple_led_extend_anime_high_speed_tbl[clrvari][extend_color_index][2];
        } else {
            wBuf[3] = ir2e71y_triple_led_anime_high_speed_tbl[clrvari][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = ir2e71y_triple_led_anime_high_speed_tbl[clrvari][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = ir2e71y_triple_led_anime_high_speed_tbl[clrvari][ir2e71y_bdic_tri_led_color][2];
        }
#else /* IR2E71Y_EXTEND_COLOR_LED */
        wBuf[3] = ir2e71y_triple_led_anime_high_speed_tbl[clrvari][ir2e71y_bdic_tri_led_color][0];
        wBuf[4] = ir2e71y_triple_led_anime_high_speed_tbl[clrvari][ir2e71y_bdic_tri_led_color][1];
        wBuf[5] = ir2e71y_triple_led_anime_high_speed_tbl[clrvari][ir2e71y_bdic_tri_led_color][2];
#endif /* IR2E71Y_EXTEND_COLOR_LED */

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH0_A, wBuf, 9);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
    case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
#ifdef IR2E71Y_EXTEND_COLOR_LED
        if (ir2e71y_bdic_tri_led_color > IR2E71Y_TRI_LED_COLOR_NUM) {
            extend_color_index = ir2e71y_bdic_tri_led_color - IR2E71Y_TRI_LED_COLOR_NUM;
            switch (ir2e71y_bdic_tri_led_mode) {
            case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
                wBuf[0] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][2][extend_color_index][2];
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
                wBuf[0] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][2][extend_color_index][2];
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
                wBuf[0] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][2][extend_color_index][2];
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
                wBuf[0] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][2][extend_color_index][2];
                break;
            }
        } else {
            switch (ir2e71y_bdic_tri_led_mode) {
            case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_breath_tbl, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_long_breath_tbl, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_wave_tbl, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_flash_tbl, sizeof(anime_tbl1));
                break;
            }
            wBuf[0] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[1] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[2] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][2];
            wBuf[3] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][2];
            wBuf[6] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][0];
            wBuf[7] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][1];
            wBuf[8] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][2];
        }
#else /* IR2E71Y_EXTEND_COLOR_LED */
        switch (ir2e71y_bdic_tri_led_mode) {
        case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
            memcpy(anime_tbl1, ir2e71y_triple_led_anime_breath_tbl, sizeof(anime_tbl1));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
            memcpy(anime_tbl1, ir2e71y_triple_led_anime_long_breath_tbl, sizeof(anime_tbl1));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
            memcpy(anime_tbl1, ir2e71y_triple_led_anime_wave_tbl, sizeof(anime_tbl1));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
            memcpy(anime_tbl1, ir2e71y_triple_led_anime_flash_tbl, sizeof(anime_tbl1));
            break;
        }
        wBuf[0] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][0];
        wBuf[1] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][1];
        wBuf[2] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][2];
        wBuf[3] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][0];
        wBuf[4] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][1];
        wBuf[5] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][2];
        wBuf[6] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][0];
        wBuf[7] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][1];
        wBuf[8] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][2];
#endif /* IR2E71Y_EXTEND_COLOR_LED */

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH0_A, wBuf, 9);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
    case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        switch (ir2e71y_bdic_tri_led_mode) {
        case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
            memcpy(anime_tbl2, ir2e71y_triple_led_anime_aurora_tbl, sizeof(anime_tbl2));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
            memcpy(anime_tbl2, ir2e71y_triple_led_anime_rainbow_tbl, sizeof(anime_tbl2));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
            memcpy(anime_tbl2, ir2e71y_triple_led_anime_emopattern_tbl, sizeof(anime_tbl2));
            break;
        }
#else /* IR2E71Y_ILLUMI_COLOR_LED */
    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        memcpy(anime_tbl2, ir2e71y_triple_led_anime_emopattern_tbl, sizeof(anime_tbl2));
#endif /* IR2E71Y_ILLUMI_COLOR_LED */
        wBuf[0] = anime_tbl2[clrvari][0][0];
        wBuf[1] = anime_tbl2[clrvari][0][1];
        wBuf[2] = anime_tbl2[clrvari][0][2];
        wBuf[3] = anime_tbl2[clrvari][1][0];
        wBuf[4] = anime_tbl2[clrvari][1][1];
        wBuf[5] = anime_tbl2[clrvari][1][2];
        wBuf[6] = anime_tbl2[clrvari][2][0];
        wBuf[7] = anime_tbl2[clrvari][2][1];
        wBuf[8] = anime_tbl2[clrvari][2][2];

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH0_A, wBuf, 9);
        break;
#endif /* IR2E71Y_ANIME_COLOR_LED */

    default:
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_KEY_LED_control                                            */
/* ------------------------------------------------------------------------- */
#ifdef IR2E71Y_KEY_LED
static void ir2e71y_bdic_PD_KEY_LED_control(unsigned char dim, unsigned char index, int ontime, int interval)
{
    ir2e71y_bdic_key_led_index = index;
    ir2e71y_bdic_key_led_dim = dim;

    IR2E71Y_DEBUG("in before_index:%d, current_index:%d; before_dim:%d,current_dim:%d.\n"
                                                             ,ir2e71y_bdic_key_led_before_index, ir2e71y_bdic_key_led_index
                                                             ,ir2e71y_bdic_key_led_before_dim, ir2e71y_bdic_key_led_dim);

    if(ir2e71y_bdic_key_led_index > 7) {
        ir2e71y_bdic_key_led_ontime   = ontime;
        ir2e71y_bdic_key_led_interval = interval;
    } else {
        ir2e71y_bdic_key_led_ontime   = 0;
        ir2e71y_bdic_key_led_interval = 0;
    }

    if((ir2e71y_bdic_key_led_before_index == ir2e71y_bdic_key_led_index) &&
        (ir2e71y_bdic_key_led_before_dim == ir2e71y_bdic_key_led_dim)) {
        return;
    }

    if(ir2e71y_bdic_key_led_index == 0) {
        ir2e71y_bdic_API_IO_bank_set(0x00);
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_key_led_off);
        if (ir2e71y_bdic_tri_led_mode != IR2E71Y_BDIC_TRI_LED_MODE_BLINK) {
            ir2e71y_pm_API_lpsoc_power_manager(IR2E71Y_DEV_TYPE_KEYLED, IR2E71Y_DEV_REQ_OFF);
        }
    } else {
        ir2e71y_bdic_API_IO_bank_set(0x00);
        if(ir2e71y_bdic_key_led_before_index == 0) {
        } else if (ir2e71y_bdic_key_led_before_index <=7 && ir2e71y_bdic_key_led_index > 7) {
        } else {
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_key_led_off_system7);
            if (ir2e71y_bdic_tri_led_mode != IR2E71Y_BDIC_TRI_LED_MODE_BLINK) {
                ir2e71y_pm_API_lpsoc_power_manager(IR2E71Y_DEV_TYPE_KEYLED, IR2E71Y_DEV_REQ_OFF);
            }
        }

        if(ir2e71y_bdic_key_led_index <= 7) {
            ir2e71y_bdic_PD_KEY_LED_set_chdig(dim, ir2e71y_bdic_key_led_index);
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_key_led_fix_on);
        } else {
            ir2e71y_pm_API_lpsoc_power_manager(IR2E71Y_DEV_TYPE_KEYLED, IR2E71Y_DEV_REQ_ON);
            ir2e71y_bdic_PD_KEY_LED_set_chdig(dim, ir2e71y_bdic_key_led_index);
            IR2E71Y_BDIC_REGSET(ir2e71y_bdic_key_led_ani_on);
        }
    }

    ir2e71y_bdic_key_led_before_index = ir2e71y_bdic_key_led_index;
    ir2e71y_bdic_key_led_before_dim = ir2e71y_bdic_key_led_dim;

    return;
}
#endif /* IR2E71Y_KEY_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_KEY_LED_set_chdig                                          */
/* ------------------------------------------------------------------------- */
#ifdef IR2E71Y_KEY_LED
static void ir2e71y_bdic_PD_KEY_LED_set_chdig(unsigned char dim, unsigned char index)
{
    unsigned char key_left_mode = ir2e71y_key_led_index_tbl[index].key_left;
    unsigned char key_center_mode = ir2e71y_key_led_index_tbl[index].key_center;
    unsigned char key_right_mode = ir2e71y_key_led_index_tbl[index].key_right;
    unsigned char wBuf[9];
    unsigned char timer1_val = 0;
    unsigned char ch_set1_val = 0;
    unsigned char ch_set2_val = 0;

    if (dim & (1<<IR2E71Y_KEY_BKL_LEFT)) {
        key_left_mode = IR2E71Y_KEY_BKL_DIM;
    }
    if (dim & (1<<IR2E71Y_KEY_BKL_CENTER)) {
        key_center_mode = IR2E71Y_KEY_BKL_DIM;
    }
    if (dim & (1<<IR2E71Y_KEY_BKL_RIGHT)) {
        key_right_mode = IR2E71Y_KEY_BKL_DIM;
    }

    memset( wBuf, 0, sizeof( wBuf ));

    if(key_left_mode == IR2E71Y_KEY_BKL_OFF) {
        wBuf[0] = 0;
        wBuf[3] = 0;
        wBuf[6] = 0;
    } else if (key_left_mode == IR2E71Y_KEY_BKL_NORMAL) {
        wBuf[0] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_LEFT][IR2E71Y_KEY_BKL_NORMAL];
        wBuf[3] = 0;
        wBuf[6] = 0;
    } else if (key_left_mode == IR2E71Y_KEY_BKL_BLINK) {
        wBuf[0] = 0;
        wBuf[3] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_LEFT][IR2E71Y_KEY_BKL_BLINK];
        wBuf[6] = 0;
    } else { /* key_left_mode = IR2E71Y_KEY_BKL_DIM */
        wBuf[0] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_LEFT][IR2E71Y_KEY_BKL_DIM];
        wBuf[3] = 0;
        wBuf[6] = 0;
    }

    if(key_center_mode == IR2E71Y_KEY_BKL_OFF) {
        wBuf[1] = 0;
        wBuf[4] = 0;
        wBuf[7] = 0;
    } else if (key_center_mode == IR2E71Y_KEY_BKL_NORMAL) {
        wBuf[1] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_CENTER][IR2E71Y_KEY_BKL_NORMAL];
        wBuf[4] = 0;
        wBuf[7] = 0;
    } else if (key_center_mode == IR2E71Y_KEY_BKL_BLINK) {
        wBuf[1] = 0;
        wBuf[4] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_CENTER][IR2E71Y_KEY_BKL_BLINK];
        wBuf[7] = 0;
    } else { /* key_center_mode = IR2E71Y_KEY_BKL_DIM */
        wBuf[1] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_CENTER][IR2E71Y_KEY_BKL_DIM];
        wBuf[4] = 0;
        wBuf[7] = 0;
    }

    if(key_right_mode == IR2E71Y_KEY_BKL_OFF) {
        wBuf[2] = 0;
        wBuf[5] = 0;
        wBuf[8] = 0;
    } else if (key_right_mode == IR2E71Y_KEY_BKL_NORMAL) {
        wBuf[2] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_RIGHT][IR2E71Y_KEY_BKL_NORMAL];
        wBuf[5] = 0;
        wBuf[8] = 0;
    } else if (key_right_mode == IR2E71Y_KEY_BKL_BLINK) {
        wBuf[2] = 0;
        wBuf[5] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_RIGHT][IR2E71Y_KEY_BKL_BLINK];
        wBuf[8] = 0;
    } else { /* key_right_mode = IR2E71Y_KEY_BKL_DIM */
        wBuf[2] = ir2e71y_key_led_tbl[IR2E71Y_KEY_BKL_RIGHT][IR2E71Y_KEY_BKL_DIM];
        wBuf[5] = 0;
        wBuf[8] = 0;
    }

    ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH3_A, wBuf, 9);

    if(index > 7) {
        timer1_val  = (unsigned char)(ir2e71y_bdic_key_led_interval << 4);
        timer1_val |= (unsigned char)(0x00 & 0x07);
        ch_set1_val = 0x46;
        ch_set2_val = (unsigned char)(ir2e71y_bdic_key_led_ontime);
    }

    if(key_left_mode == IR2E71Y_KEY_BKL_BLINK) {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH3_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH3_SET2, ch_set2_val);
    }  else {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH3_SET1, 0x20, 0x20);
    }

    if(key_center_mode == IR2E71Y_KEY_BKL_BLINK) {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH4_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH4_SET2, ch_set2_val);
    } else {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH4_SET1, 0x20, 0x20);
    }

    if(key_right_mode == IR2E71Y_KEY_BKL_BLINK) {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH5_SET1, ch_set1_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH5_SET2, ch_set2_val);
    } else {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH5_SET1, 0x20, 0x20);
    }

    if(index > 7) {
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_TIMER2, (unsigned char)timer1_val, 0xF7);
    }

    return;
}
#endif /* IR2E71Y_KEY_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_lposc_off                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_lposc_off(void)
{
    ir2e71y_pm_API_lpsoc_power_manager(IR2E71Y_DEV_TYPE_LED, IR2E71Y_DEV_REQ_OFF);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_get_clrvari_index                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_PD_TRI_LED_get_clrvari_index(int clrvari)
{
    int i = 0;

    for (i = 0; i < IR2E71Y_COL_VARI_KIND; i++) {
        if ((int)ir2e71y_clrvari_index[i] == clrvari) {
            break;
        }
    }
    if (i >= IR2E71Y_COL_VARI_KIND) {
        clrvari = IR2E71Y_COL_VARI_DEFAULT;
        for (i = 0; i < IR2E71Y_COL_VARI_KIND; i++) {
            if ((int)ir2e71y_clrvari_index[i] == clrvari) {
                break;
            }
        }
    }
    return i;
}

#ifdef IR2E71Y_COLOR_LED_TWIN
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_control_twin                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_control_twin(unsigned char request, int param)
{
    switch (request) {
    case IR2E71Y_BDIC_REQ_TRI_LED_ACTIVE:
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_STANDBY:
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_START:
        IR2E71Y_DEBUG("IR2E71Y_BDIC_REQ_TRI_LED_START tri_led_mode_twin=%d"
                       , ir2e71y_bdic_tri_led_mode_twin);

        if (ir2e71y_bdic_tri_led_mode_twin == IR2E71Y_BDIC_TRI_LED_MODE_NORMAL) {
#ifdef IR2E71Y_SYSFS_LED
            if (param == CURRENT_SET) {
                ir2e71y_bdic_LD_set_led_on_table_twin(rgb_current2);
            } else
#endif /* IR2E71Y_SYSFS_LED */
            ir2e71y_bdic_LD_set_led_fix_on_table_twin(led_state_str.bdic_clrvari_index, ir2e71y_bdic_tri_led_color);
        } else {
            ir2e71y_pm_API_lpsoc_power_manager(IR2E71Y_DEV_TYPE_LED, IR2E71Y_DEV_REQ_ON);
            ir2e71y_bdic_PD_TRI_LED_set_chdig_twin();
            ir2e71y_bdic_PD_TRI_LED_set_anime_twin();

            ir2e71y_bdic_PD_TRI_LED_anime_start_twin();
        }
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_STOP:
        ir2e71y_bdic_tri_led_mode_twin        = IR2E71Y_BDIC_TRI_LED_MODE_NORMAL;
        ir2e71y_bdic_tri_led_color       = 0;
        ir2e71y_bdic_tri_led_ontime      = 0;
        ir2e71y_bdic_tri_led_interval    = 0;
        ir2e71y_bdic_tri_led_count       = 0;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_NORMAL;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BLINK:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_BLINK;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_HISPEED:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_HISPEED;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_STANDARD:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_STANDARD;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BREATH:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_BREATH;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_WAVE:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_WAVE;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FLASH:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_FLASH;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_EMOPATTERN:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_AURORA:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_AURORA;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
    case IR2E71Y_BDIC_REQ_ILLUMI_TRIPLE_COLOR:
        ir2e71y_bdic_tri_led_mode_twin  = IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR;
        ir2e71y_bdic_tri_led_color = (unsigned char)param;
        break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */
    case IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME:
        ir2e71y_bdic_tri_led_ontime = param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL:
        ir2e71y_bdic_tri_led_interval = param;
        break;

    case IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT:
        ir2e71y_bdic_tri_led_count = param;
        break;

    default:
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_anime_start_twin                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_anime_start_twin(void)
{
    unsigned char timer_val = 0;

    switch (ir2e71y_bdic_tri_led_mode_twin) {
    case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
    case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
    case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
    case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
    case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
    case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
    case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
#endif  /* IR2E71Y_ANIME_COLOR_LED */
        timer_val  = (unsigned char)(ir2e71y_bdic_tri_led_interval << 4);
        timer_val |= (unsigned char)(ir2e71y_bdic_tri_led_count & 0x07);
        ir2e71y_bdic_led_ani_on_twin[1].data  = timer_val;
        ir2e71y_bdic_led_ani_on_twin[2].data  = timer_val;
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_ani_on_twin);
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_ani_on_triple_color_twin);
        break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */

    default:
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_set_anime_twin                                     */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_set_anime_twin(void)
{
    unsigned char ch_set3_val;
    unsigned char ch_set4_val;

    switch (ir2e71y_bdic_tri_led_mode_twin) {
    case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
        ch_set3_val = 0x4E;
        if (ir2e71y_bdic_tri_led_ontime > IR2E71Y_TRI_LED_ONTIME_TYPE7) {
            ch_set4_val = IR2E71Y_TRI_LED_ONTIME_TYPE1 + (ir2e71y_bdic_tri_led_ontime - IR2E71Y_TRI_LED_ONTIME_TYPE7);
        } else {
            ch_set4_val = (unsigned char)(ir2e71y_bdic_tri_led_ontime);
        }
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH3_SET1, ch_set3_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH3_SET2, ch_set4_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH4_SET1, ch_set3_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH4_SET2, ch_set4_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH5_SET1, ch_set3_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH5_SET2, ch_set4_val);

        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
        ch_set3_val = 0x06;
        ch_set4_val = (unsigned char)(ir2e71y_bdic_tri_led_ontime);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH3_SET1, ch_set3_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH3_SET2, ch_set4_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH4_SET1, ch_set3_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH4_SET2, ch_set4_val);
        ir2e71y_bdic_API_IO_msk_bit_reg(BDIC_REG_CH5_SET1, ch_set3_val, 0x6F);
        ir2e71y_bdic_API_IO_write_reg(BDIC_REG_CH5_SET2, ch_set4_val);
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_high_speed_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_standard_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_breath_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_long_breath_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_wave_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_flash_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_emopattern_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_aurora_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_led_rainbow_on_twin);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
        IR2E71Y_BDIC_REGSET(ir2e71y_bdic_illumi_triple_color_1st_twin);
        break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */
    default:
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_PD_TRI_LED_set_chdig_twin                                     */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_PD_TRI_LED_set_chdig_twin(void)
{
    int clrvari = led_state_str.bdic_clrvari_index;
    unsigned char wBuf[9];
#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
    unsigned char anime_tbl1[IR2E71Y_COL_VARI_KIND][IR2E71Y_TRI_LED_ANIME_3PAGE][IR2E71Y_TRI_LED_COLOR_TBL_NUM][3];
#endif /* IR2E71Y_ILLUMI_COLOR_LED */
    unsigned char anime_tbl2[IR2E71Y_COL_VARI_KIND][IR2E71Y_TRI_LED_ANIME_3PAGE][3];
#ifdef IR2E71Y_EXTEND_COLOR_LED
    unsigned char extend_color_index;
#endif /* IR2E71Y_EXTEND_COLOR_LED */
#endif  /* IR2E71Y_ANIME_COLOR_LED */

    memset(wBuf, 0, sizeof(wBuf));
    ir2e71y_bdic_API_IO_bank_set(0x00);

    switch (ir2e71y_bdic_tri_led_mode_twin) {
    case IR2E71Y_BDIC_TRI_LED_MODE_NORMAL:
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BLINK:
    case IR2E71Y_BDIC_TRI_LED_MODE_FIREFLY:
    case IR2E71Y_BDIC_TRI_LED_MODE_TRIPLE_COLOR:
#ifdef IR2E71Y_EXTEND_COLOR_LED
        if (ir2e71y_bdic_tri_led_color > IR2E71Y_TRI_LED_COLOR_NUM) {
            extend_color_index = ir2e71y_bdic_tri_led_color - IR2E71Y_TRI_LED_COLOR_NUM;
            wBuf[0] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][0];
            wBuf[1] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][1];
            wBuf[2] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][2];
            wBuf[3] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][0];
            wBuf[4] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][1];
            wBuf[5] = ir2e71y_triple_led_extend_anime_tbl[clrvari][1][extend_color_index][2];
            wBuf[6] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][0];
            wBuf[7] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][1];
            wBuf[8] = ir2e71y_triple_led_extend_anime_tbl[clrvari][0][extend_color_index][2];
        } else {
            wBuf[0] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[1] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[2] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][2];
            wBuf[3] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][ir2e71y_bdic_tri_led_color][2];
            wBuf[6] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[7] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[8] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][2];
        }
#else /* IR2E71Y_EXTEND_COLOR_LED */
        wBuf[0] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][0];
        wBuf[1] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][1];
        wBuf[2] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][2];
        wBuf[3] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][ir2e71y_bdic_tri_led_color][0];
        wBuf[4] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][ir2e71y_bdic_tri_led_color][1];
        wBuf[5] = ir2e71y_triple_led_anime_tbl_twin[clrvari][1][ir2e71y_bdic_tri_led_color][2];
        wBuf[6] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][0];
        wBuf[7] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][1];
        wBuf[8] = ir2e71y_triple_led_anime_tbl_twin[clrvari][0][ir2e71y_bdic_tri_led_color][2];
#endif /* IR2E71Y_EXTEND_COLOR_LED */
        if ((ir2e71y_bdic_tri_led_mode_twin == IR2E71Y_BDIC_TRI_LED_MODE_BLINK) &&
            (ir2e71y_bdic_tri_led_ontime > IR2E71Y_TRI_LED_ONTIME_TYPE7)) {
            wBuf[0] = wBuf[6] = wBuf[3];
            wBuf[1] = wBuf[7] = wBuf[4];
            wBuf[2] = wBuf[8] = wBuf[5];
        }

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH3_A, wBuf, 9);
        break;

#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
    case IR2E71Y_BDIC_TRI_LED_MODE_HISPEED:
    case IR2E71Y_BDIC_TRI_LED_MODE_STANDARD:
#ifdef IR2E71Y_EXTEND_COLOR_LED
        if (ir2e71y_bdic_tri_led_color > IR2E71Y_TRI_LED_COLOR_NUM) {
            extend_color_index = ir2e71y_bdic_tri_led_color - IR2E71Y_TRI_LED_COLOR_NUM;
            wBuf[3] = ir2e71y_triple_led_extend_anime_high_speed_tbl[clrvari][extend_color_index][0];
            wBuf[4] = ir2e71y_triple_led_extend_anime_high_speed_tbl[clrvari][extend_color_index][1];
            wBuf[5] = ir2e71y_triple_led_extend_anime_high_speed_tbl[clrvari][extend_color_index][2];
        } else {
            wBuf[3] = ir2e71y_triple_led_anime_high_speed_tbl_twin[clrvari][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = ir2e71y_triple_led_anime_high_speed_tbl_twin[clrvari][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = ir2e71y_triple_led_anime_high_speed_tbl_twin[clrvari][ir2e71y_bdic_tri_led_color][2];
        }
#else /* IR2E71Y_EXTEND_COLOR_LED */
        wBuf[3] = ir2e71y_triple_led_anime_high_speed_tbl_twin[clrvari][ir2e71y_bdic_tri_led_color][0];
        wBuf[4] = ir2e71y_triple_led_anime_high_speed_tbl_twin[clrvari][ir2e71y_bdic_tri_led_color][1];
        wBuf[5] = ir2e71y_triple_led_anime_high_speed_tbl_twin[clrvari][ir2e71y_bdic_tri_led_color][2];
#endif /* IR2E71Y_EXTEND_COLOR_LED */

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH3_A, wBuf, 9);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
    case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
    case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
#ifdef IR2E71Y_EXTEND_COLOR_LED
        if (ir2e71y_bdic_tri_led_color > IR2E71Y_TRI_LED_COLOR_NUM) {
            extend_color_index = ir2e71y_bdic_tri_led_color - IR2E71Y_TRI_LED_COLOR_NUM;
            switch (ir2e71y_bdic_tri_led_mode_twin) {
            case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
                wBuf[0] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_breath_tbl[clrvari][2][extend_color_index][2];
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
                wBuf[0] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_long_breath_tbl[clrvari][2][extend_color_index][2];
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
                wBuf[0] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_wave_tbl[clrvari][2][extend_color_index][2];
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
                wBuf[0] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][0][extend_color_index][0];
                wBuf[1] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][0][extend_color_index][1];
                wBuf[2] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][0][extend_color_index][2];
                wBuf[3] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][1][extend_color_index][0];
                wBuf[4] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][1][extend_color_index][1];
                wBuf[5] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][1][extend_color_index][2];
                wBuf[6] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][2][extend_color_index][0];
                wBuf[7] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][2][extend_color_index][1];
                wBuf[8] = ir2e71y_triple_led_extend_anime_flash_tbl[clrvari][2][extend_color_index][2];
                break;
            }
        } else {
            switch (ir2e71y_bdic_tri_led_mode_twin) {
            case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_breath_tbl_twin, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_long_breath_tbl_twin, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_wave_tbl_twin, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_flash_tbl_twin, sizeof(anime_tbl1));
                break;
            }
            wBuf[0] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[1] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[2] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][2];
            wBuf[3] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][2];
            wBuf[6] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][0];
            wBuf[7] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][1];
            wBuf[8] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][2];
          }
#else /* IR2E71Y_EXTEND_COLOR_LED */
            switch (ir2e71y_bdic_tri_led_mode_twin) {
            case IR2E71Y_BDIC_TRI_LED_MODE_BREATH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_breath_tbl_twin, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_LONG_BREATH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_long_breath_tbl_twin, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_WAVE:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_wave_tbl_twin, sizeof(anime_tbl1));
                break;
            case IR2E71Y_BDIC_TRI_LED_MODE_FLASH:
                memcpy(anime_tbl1, ir2e71y_triple_led_anime_flash_tbl_twin, sizeof(anime_tbl1));
                break;
            }
            wBuf[0] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][0];
            wBuf[1] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][1];
            wBuf[2] = anime_tbl1[clrvari][0][ir2e71y_bdic_tri_led_color][2];
            wBuf[3] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][0];
            wBuf[4] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][1];
            wBuf[5] = anime_tbl1[clrvari][1][ir2e71y_bdic_tri_led_color][2];
            wBuf[6] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][0];
            wBuf[7] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][1];
            wBuf[8] = anime_tbl1[clrvari][2][ir2e71y_bdic_tri_led_color][2];
#endif /* IR2E71Y_EXTEND_COLOR_LED */

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH3_A, wBuf, 9);
        break;

    case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
    case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        switch (ir2e71y_bdic_tri_led_mode_twin) {
        case IR2E71Y_BDIC_TRI_LED_MODE_AURORA:
            memcpy(anime_tbl2, ir2e71y_triple_led_anime_aurora_tbl_twin, sizeof(anime_tbl2));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_RAINBOW:
            memcpy(anime_tbl2, ir2e71y_triple_led_anime_rainbow_tbl_twin, sizeof(anime_tbl2));
            break;
        case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
            memcpy(anime_tbl2, ir2e71y_triple_led_anime_emopattern_tbl_twin, sizeof(anime_tbl2));
            break;
        }
#else /* IR2E71Y_ILLUMI_COLOR_LED */
    case IR2E71Y_BDIC_TRI_LED_MODE_EMOPATTERN:
        memcpy(anime_tbl2, ir2e71y_triple_led_anime_emopattern_tbl_twin, sizeof(anime_tbl2));
#endif /* IR2E71Y_ILLUMI_COLOR_LED */
        wBuf[0] = anime_tbl2[clrvari][0][0];
        wBuf[1] = anime_tbl2[clrvari][0][1];
        wBuf[2] = anime_tbl2[clrvari][0][2];
        wBuf[3] = anime_tbl2[clrvari][1][0];
        wBuf[4] = anime_tbl2[clrvari][1][1];
        wBuf[5] = anime_tbl2[clrvari][1][2];
        wBuf[6] = anime_tbl2[clrvari][2][0];
        wBuf[7] = anime_tbl2[clrvari][2][1];
        wBuf[8] = anime_tbl2[clrvari][2][2];

        ir2e71y_bdic_API_IO_multi_write_reg(BDIC_REG_CH3_A, wBuf, 9);
        break;
#endif /* IR2E71Y_ANIME_COLOR_LED */

    default:
        break;
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_led_fix_on_table_twin                                  */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_set_led_fix_on_table_twin(int clr_vari, int color)
{
    unsigned char *pTriLed;

#ifdef IR2E71Y_EXTEND_COLOR_LED
    unsigned char extend_color_index;

    if (color > IR2E71Y_TRI_LED_COLOR_NUM) {
        extend_color_index = color - IR2E71Y_TRI_LED_COLOR_NUM;
        pTriLed = (unsigned char *)(&(ir2e71y_triple_led_extend_tbl[clr_vari][extend_color_index]));
    } else {
        pTriLed = (unsigned char *)(&(ir2e71y_triple_led_tbl_twin[clr_vari][color]));
    }
#else /* IR2E71Y_EXTEND_COLOR_LED */
    pTriLed = (unsigned char *)(&(ir2e71y_triple_led_tbl_twin[clr_vari][color]));
#endif /* IR2E71Y_EXTEND_COLOR_LED */

    ir2e71y_bdic_LD_set_led_on_table_twin(pTriLed);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_led_on_table_twin                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_set_led_on_table_twin(unsigned char *rgb_current)
{

    unsigned char *pTriLed;
    ir2e71y_bdicRegSetting_t led_fix_on[ARRAY_SIZE(ir2e71y_bdic_led_fix_on_twin)];

    memcpy(led_fix_on, ir2e71y_bdic_led_fix_on_twin, sizeof(ir2e71y_bdic_led_fix_on_twin));

    pTriLed = (unsigned char *)rgb_current;

    led_fix_on[IR2E71Y_LED_FIX_ONR].data = *(pTriLed + 0);
    led_fix_on[IR2E71Y_LED_FIX_ONG].data = *(pTriLed + 1);
    led_fix_on[IR2E71Y_LED_FIX_ONB].data = *(pTriLed + 2);
    IR2E71Y_BDIC_REGSET(led_fix_on);
}

#ifdef IR2E71Y_SYSFS_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_LD_set_led_fix_current_table_twin                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_LD_set_led_fix_current_table_twin(unsigned char *rgb_current)
{
    unsigned char *pTriLed;
    ir2e71y_bdicRegSetting_t led_current[ARRAY_SIZE(ir2e71y_bdic_led_current_twin)];

    memcpy(led_current, ir2e71y_bdic_led_current_twin, sizeof(ir2e71y_bdic_led_current_twin));

    pTriLed = (unsigned char *)rgb_current;

    led_current[0].data = *(pTriLed + 0);
    led_current[1].data = *(pTriLed + 1);
    led_current[2].data = *(pTriLed + 2);
    IR2E71Y_BDIC_REGSET(led_current);
}
#endif /* IR2E71Y_SYSFS_LED */

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_TRI_LED_INFO_output_twin                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_TRI_LED_INFO_output_twin(void)
{
    int idx;
    unsigned char   *p;
    unsigned char   *pbuf;
    unsigned short   ir2e71y_log_lv_bk;
    size_t  size;

    size  = (BDIC_REG_TIMER2 - BDIC_REG_SEQ_ANIME + 1);
    size += (BDIC_REG_CH5_C - BDIC_REG_CH3_SET1 + 1);

    pbuf = (unsigned char *)kzalloc(size, GFP_KERNEL);
    if (!pbuf) {
        IR2E71Y_ERR("kzalloc failed. size=%ld", size);
        return;
    }

    ir2e71y_log_lv_bk = ir2e71y_log_lv;
    ir2e71y_log_lv = IR2E71Y_LOG_LV_ERR;
    ir2e71y_bdic_API_IO_bank_set(0x00);

    p = pbuf;
    for (idx = BDIC_REG_SEQ_ANIME; idx <= BDIC_REG_TIMER2; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    for (idx = BDIC_REG_CH3_SET1; idx <= BDIC_REG_CH5_C; idx++) {
        *p = 0x00;
        ir2e71y_bdic_API_DIAG_read_reg(idx, p);
        p++;
    }
    ir2e71y_log_lv = ir2e71y_log_lv_bk;

    printk("[BL71Y8] TRI-LED-TWIN INFO ->>\n");
    printk("[BL71Y8] led_state_str.handset_color       = %d.\n", led_state_str.handset_color);
    printk("[BL71Y8] led_state_str.bdic_clrvari_index  = %d.\n", led_state_str.bdic_clrvari_index);
    printk("[BL71Y8] led_state_str.bdic_chipver        = %d.\n", led_state_str.bdic_chipver);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_color         = %d.\n", (int)ir2e71y_bdic_tri_led_color);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_mode_twin     = %d.\n", ir2e71y_bdic_tri_led_mode_twin);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_ontime        = %d.\n", ir2e71y_bdic_tri_led_ontime);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_interval      = %d.\n", ir2e71y_bdic_tri_led_interval);
    printk("[BL71Y8] ir2e71y_bdic_tri_led_count         = %d.\n", ir2e71y_bdic_tri_led_count);

#ifdef IR2E71Y_SYSFS_LED
    printk("[BL71Y8] rgb_current2[0]                   = %d.\n", rgb_current2[0]);
    printk("[BL71Y8] rgb_current2[1]                   = %d.\n", rgb_current2[1]);
    printk("[BL71Y8] rgb_current2[2]                   = %d.\n", rgb_current2[2]);
#endif /* IR2E71Y_SYSFS_LED */

    p = pbuf;
    printk("[BL71Y8] BDIC_REG_TIMER_SETTING 0x%2X: %02x %02x %02x\n", BDIC_REG_SEQ_ANIME, *p, *(p + 1), *(p + 2));
    p += 3;
    printk("[BL71Y8] BDIC_REG_LED_TWIN_SETTING  0x%2X: %02x %02x %02x %02x %02x %02x %02x\n",
                BDIC_REG_CH3_SET1, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6));
    p += 7;
    printk("[BL71Y8] BDIC_REG_LED_TWIN_CURRENT  0x%2X: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                BDIC_REG_CH3_A, *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7), *(p + 8));

    kfree(pbuf);

    printk("[BL71Y8] TRI-LED-TWIN INFO <<-\n");
    return;
}

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_illumi_triple_color_INFO_output                               */
/* ------------------------------------------------------------------------- */
static void ir2e71y_bdic_illumi_triple_color_INFO_output(void)
{
    int i;

    printk("[BL71Y8] illumi-triple-color INFO ->>\n");

    printk("[BL71Y8] illumi_state.running_state        = %d.\n", illumi_state.running_state);
    printk("[BL71Y8] illumi_state.illumi_state         = %d.\n", illumi_state.illumi_state);

    for (i = ILLUMI_FRAME_FIRST; i != ILLUMI_FRAME_MAX; i++) {
        printk("[BL71Y8] illumi_state.colors[%d]            = %d.\n", i, illumi_state.colors[i]);
    }

    printk("[BL71Y8] illumi_state.count                = %d.\n", illumi_state.count);

    printk("[BL71Y8] illumi-triple-color INFO <<-\n");
}
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */
#endif /* CONFIG_ANDROID_ENGINEERING */
#endif /* IR2E71Y_COLOR_LED_TWIN */
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
