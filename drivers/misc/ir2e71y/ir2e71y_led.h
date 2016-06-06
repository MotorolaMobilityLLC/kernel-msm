/* drivers/misc/ir2e71y/ir2e71y_led.h  (Display Driver)
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

#ifndef IR2E71Y_LED_H
#define IR2E71Y_LED_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#ifdef IR2E71Y_SYSFS_LED
#define SYSFS_LED_SH_LED_1                          (0)
#define SYSFS_LED_SH_LED_2                          (1)


#define SYSFS_LED_SH_RED                            (0)
#define SYSFS_LED_SH_GREEN                          (1)
#define SYSFS_LED_SH_BLUE                           (2)
#endif /* IR2E71Y_SYSFS_LED */
/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
enum {
    IR2E71Y_BDIC_REQ_TRI_LED_NONE = 0,
    IR2E71Y_BDIC_REQ_TRI_LED_ACTIVE,
    IR2E71Y_BDIC_REQ_TRI_LED_STANDBY,
    IR2E71Y_BDIC_REQ_TRI_LED_STOP,
    IR2E71Y_BDIC_REQ_TRI_LED_START,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_NORMAL,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BLINK,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FIREFLY,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_HISPEED,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_STANDARD,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_BREATH,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_LONG_BREATH,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_WAVE,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_FLASH,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_AURORA,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_RAINBOW,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_MODE_EMOPATTERN,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_ONTIME,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_INTERVAL,
    IR2E71Y_BDIC_REQ_TRI_LED_SET_COUNT,
    IR2E71Y_BDIC_REQ_ILLUMI_TRIPLE_COLOR,
};

struct ir2e71y_led_init_param {
    int handset_color;
    int bdic_chipver;
};

struct ir2e71y_led_state_str {
    int handset_color;
    int bdic_chipver;
    int bdic_clrvari_index;
};

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
enum {
    ILLUMI_STATE_STOP  = -1,
    ILLUMI_STATE_WAIT_SET_B2_AREA,
    ILLUMI_STATE_WAIT_SET_C2_AREA,
    ILLUMI_STATE_WAIT_SET_A3_AREA,
    ILLUMI_STATE_WAIT_ANIME_BREAK,
    ILLUMI_STATE_WAIT_RESTART,
    ILLUMI_STATE_MAX,
};

struct ir2e71y_illumi_state {
    char colors[ILLUMI_FRAME_MAX];
    int  count;
    bool running_state;
    int  illumi_state;
    struct workqueue_struct  * workqueue;
    struct delayed_work works[ILLUMI_STATE_MAX];
};
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

struct ir2e71y_bdic_led_color_index {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char color;
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
int  ir2e71y_led_API_initialize(struct ir2e71y_led_init_param *init_param);
void ir2e71y_bdic_API_TRI_LED_set_request(struct ir2e71y_tri_led *tmp);
void ir2e71y_bdic_API_lposc_on(void);
void ir2e71y_bdic_API_lposc_off(void);

int  ir2e71y_bdic_API_TRI_LED_off(void);
unsigned char ir2e71y_bdic_API_TRI_LED_get_color_index_and_reedit(struct ir2e71y_tri_led *tri_led );
#ifdef IR2E71Y_KEY_LED
unsigned char ir2e71y_bdic_API_KEY_LED_get_color_index_and_reedit(struct ir2e71y_key_bkl_ctl *key_bkl_ctl );
void ir2e71y_bdic_API_KEY_LED_ctl(unsigned char dim, unsigned char index, int ontime, int interval);
#endif /* IR2E71Y_KEY_LED */
int  ir2e71y_bdic_API_TRI_LED_normal_on(unsigned char color);
#ifdef IR2E71Y_SYSFS_LED
int ir2e71y_bdic_API_LED_blink_on(int no, unsigned char color, struct ir2e71y_tri_led led);
int ir2e71y_bdic_API_LED_on(int no, struct ir2e71y_tri_led led);
int ir2e71y_bdic_API_LED_off(int no);
#endif /* IR2E71Y_SYSFS_LED */
void ir2e71y_bdic_API_TRI_LED_blink_on(unsigned char color, int ontime, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_firefly_on(unsigned char color, int ontime, int interval, int count);
#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
void ir2e71y_bdic_API_TRI_LED_high_speed_on(unsigned char color, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_standard_on(unsigned char color, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_breath_on(unsigned char color, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_long_breath_on(unsigned char color, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_wave_on(unsigned char color, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_flash_on(unsigned char color, int interval, int count);
void ir2e71y_bdic_API_TRI_LED_aurora_on(int interval, int count);
void ir2e71y_bdic_API_TRI_LED_rainbow_on(int interval, int count);
#endif /* IR2E71Y_ILLUMI_COLOR_LED */
void ir2e71y_bdic_API_TRI_LED_emopattern_on(int interval, int count);
#endif  /* IR2E71Y_ANIME_COLOR_LED */

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
void ir2e71y_bdic_API_LED_set_illumi_triple_color(struct ir2e71y_illumi_triple_color illumi_triple_color);
void ir2e71y_bdic_API_LED_clear_illumi_triple_color(void);
bool ir2e71y_bdic_API_LED_is_running_illumi_triple_color(void);
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

void ir2e71y_bdic_API_TRI_LED_exit(void);
#if defined(CONFIG_ANDROID_ENGINEERING)
void ir2e71y_bdic_API_TRI_LED_INFO_output(void);
void ir2e71y_bdic_API_TRI_LED2_INFO_output(void);
#endif /* CONFIG_ANDROID_ENGINEERING */

#endif  /* IR2E71Y_LED_H */
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
