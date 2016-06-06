/* drivers/misc/ir2e71y/ir2e71y_priv.h  (Display Driver)
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

#ifndef IR2E71Y_KERL_PRIV_H
#define IR2E71Y_KERL_PRIV_H
/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */



#define IR2E71Y_MAIN_BKL_PARAM_OFF           (0)
#define IR2E71Y_MAIN_BKL_PARAM_WEAK          (1)
#define IR2E71Y_MAIN_BKL_PARAM_MIN           (0)
#define IR2E71Y_MAIN_BKL_PARAM_MAX           (255)
#define IR2E71Y_MAIN_BKL_PARAM_MIN_AUTO      (2)
#define IR2E71Y_MAIN_BKL_PARAM_MAX_AUTO      (255)


enum {
    IR2E71Y_MAIN_BKL_AUTO_OFF,
    IR2E71Y_MAIN_BKL_AUTO_ON,
    IR2E71Y_MAIN_BKL_AUTO_ECO_ON,
    NUM_IR2E71Y_MAIN_BKL_AUTO
};

enum {
    IR2E71Y_MAIN_BKL_DTV_OFF,
    IR2E71Y_MAIN_BKL_DTV_ON,
    NUM_IR2E71Y_MAIN_BKL_DTV
};

enum {
    IR2E71Y_MAIN_BKL_EMG_OFF,
    IR2E71Y_MAIN_BKL_EMG_ON_LEVEL0,
    IR2E71Y_MAIN_BKL_EMG_ON_LEVEL1,
    NUM_IR2E71Y_MAIN_BKL_EMG
};

enum {
    IR2E71Y_MAIN_BKL_LOWBKL_MODE_OFF,
    IR2E71Y_MAIN_BKL_LOWBKL_MODE_ON,
    NUM_IR2E71Y_MAIN_BKL_LOWBKL_MODE
};



enum {
    IR2E71Y_IRQ_NO_MASK,
    IR2E71Y_IRQ_MASK,
    NUM_IR2E71Y_IRQ_SWITCH
};

enum {
    IR2E71Y_MAIN_BKL_CHG_OFF,
    IR2E71Y_MAIN_BKL_CHG_ON,
    IR2E71Y_MAIN_BKL_CHG_ON_BRIGHT,
    NUM_IR2E71Y_MAIN_BKL_CHG
};


enum {
    IR2E71Y_TRV_PARAM_OFF,
    IR2E71Y_TRV_PARAM_ON
};

enum {
    ILLUMI_FRAME_FIRST = 0,
    ILLUMI_FRAME_SECOND,
    ILLUMI_FRAME_THIRD,
    ILLUMI_FRAME_MAX
};


enum {
    IR2E71Y_UPPER_UNIT_IS_NOT_CONNECTED,
    IR2E71Y_UPPER_UNIT_IS_CONNECTED,
    NUM_UPPER_UNIT_STATUS
};

enum {
    IR2E71Y_BDIC_IS_NOT_EXIST,
    IR2E71Y_BDIC_IS_EXIST,
    NUM_BDIC_EXIST_STATUS
};

enum {
    IR2E71Y_MAIN_BKL_MODE_OFF,
    IR2E71Y_MAIN_BKL_MODE_FIX,
    IR2E71Y_MAIN_BKL_MODE_AUTO,
    IR2E71Y_MAIN_BKL_MODE_DTV_OFF,
    IR2E71Y_MAIN_BKL_MODE_DTV_FIX,
    IR2E71Y_MAIN_BKL_MODE_DTV_AUTO,
    NUM_IR2E71Y_MAIN_BKL_MODE
};

enum {
    IR2E71Y_MAIN_DISP_ALS_RANGE_001 = 0,
    IR2E71Y_MAIN_DISP_ALS_RANGE_002,
    IR2E71Y_MAIN_DISP_ALS_RANGE_004,
    IR2E71Y_MAIN_DISP_ALS_RANGE_008,
    IR2E71Y_MAIN_DISP_ALS_RANGE_016,
    IR2E71Y_MAIN_DISP_ALS_RANGE_032,
    IR2E71Y_MAIN_DISP_ALS_RANGE_064,
    IR2E71Y_MAIN_DISP_ALS_RANGE_128,
    NUM_IR2E71Y_MAIN_DISP_ALS_RANGE
};

enum {
    IR2E71Y_TRI_LED_EXT_MODE_DISABLE,
    IR2E71Y_TRI_LED_EXT_MODE_ENABLE,
    NUM_IR2E71Y_TRI_LED_EXT_MODE
};

enum {
    IR2E71Y_KEY_BKL_OFF,
    IR2E71Y_KEY_BKL_NORMAL,
    IR2E71Y_KEY_BKL_BLINK,
    IR2E71Y_KEY_BKL_DIM,
    NUM_IR2E71Y_KEY_BKL_MODE
};

enum {
    IR2E71Y_KEY_BKL_LEFT,
    IR2E71Y_KEY_BKL_CENTER,
    IR2E71Y_KEY_BKL_RIGHT,
    NUM_IR2E71Y_KEY_SELECT
};

enum {
    IR2E71Y_TRI_LED_INTERVAL_NONE,
    IR2E71Y_TRI_LED_INTERVAL_1S,
    IR2E71Y_TRI_LED_INTERVAL_2S,
    IR2E71Y_TRI_LED_INTERVAL_3S,
    IR2E71Y_TRI_LED_INTERVAL_4S,
    IR2E71Y_TRI_LED_INTERVAL_5S,
    IR2E71Y_TRI_LED_INTERVAL_6S,
    IR2E71Y_TRI_LED_INTERVAL_7S,
    IR2E71Y_TRI_LED_INTERVAL_8S,
    IR2E71Y_TRI_LED_INTERVAL_9S,
    IR2E71Y_TRI_LED_INTERVAL_10S,
    IR2E71Y_TRI_LED_INTERVAL_11S,
    IR2E71Y_TRI_LED_INTERVAL_12S,
    IR2E71Y_TRI_LED_INTERVAL_13S,
    IR2E71Y_TRI_LED_INTERVAL_14S,
    IR2E71Y_TRI_LED_INTERVAL_15S,
    NUM_IR2E71Y_TRI_LED_INTERVAL
};

enum {
    IR2E71Y_KEY_BKL_INTERVAL_1S = 1,
    IR2E71Y_KEY_BKL_INTERVAL_2S,
    IR2E71Y_KEY_BKL_INTERVAL_3S,
    IR2E71Y_KEY_BKL_INTERVAL_4S,
    IR2E71Y_KEY_BKL_INTERVAL_5S,
    IR2E71Y_KEY_BKL_INTERVAL_6S,
    IR2E71Y_KEY_BKL_INTERVAL_7S,
    IR2E71Y_KEY_BKL_INTERVAL_8S,
    IR2E71Y_KEY_BKL_INTERVAL_9S,
    IR2E71Y_KEY_BKL_INTERVAL_10S,
    IR2E71Y_KEY_BKL_INTERVAL_11S,
    IR2E71Y_KEY_BKL_INTERVAL_12S,
    IR2E71Y_KEY_BKL_INTERVAL_13S,
    IR2E71Y_KEY_BKL_INTERVAL_14S,
    IR2E71Y_KEY_BKL_INTERVAL_15S,
    NUM_IR2E71Y_KEY_BKL_INTERVAL
};

enum {
    IR2E71Y_TRI_LED_MODE_OFF = -1,
    IR2E71Y_TRI_LED_MODE_NORMAL,
    IR2E71Y_TRI_LED_MODE_BLINK,
    IR2E71Y_TRI_LED_MODE_FIREFLY,
    IR2E71Y_TRI_LED_MODE_HISPEED,
    IR2E71Y_TRI_LED_MODE_STANDARD,
    IR2E71Y_TRI_LED_MODE_BREATH,
    IR2E71Y_TRI_LED_MODE_LONG_BREATH,
    IR2E71Y_TRI_LED_MODE_WAVE,
    IR2E71Y_TRI_LED_MODE_FLASH,
    IR2E71Y_TRI_LED_MODE_AURORA,
    IR2E71Y_TRI_LED_MODE_RAINBOW,
    IR2E71Y_TRI_LED_MODE_EMOPATTERN,
    IR2E71Y_TRI_LED_MODE_PATTERN1,
    IR2E71Y_TRI_LED_MODE_PATTERN2,
    IR2E71Y_TRI_LED_MODE_ILLUMI_TRIPLE_COLOR,
    NUM_IR2E71Y_TRI_LED_MODE
};

enum {
    IR2E71Y_TRI_LED_ONTIME_TYPE0,
    IR2E71Y_TRI_LED_ONTIME_TYPE1,
    IR2E71Y_TRI_LED_ONTIME_TYPE2,
    IR2E71Y_TRI_LED_ONTIME_TYPE3,
    IR2E71Y_TRI_LED_ONTIME_TYPE4,
    IR2E71Y_TRI_LED_ONTIME_TYPE5,
    IR2E71Y_TRI_LED_ONTIME_TYPE6,
    IR2E71Y_TRI_LED_ONTIME_TYPE7,
    IR2E71Y_TRI_LED_ONTIME_TYPE8,
    IR2E71Y_TRI_LED_ONTIME_TYPE9,
    IR2E71Y_TRI_LED_ONTIME_TYPE10,
    IR2E71Y_TRI_LED_ONTIME_TYPE11,
    IR2E71Y_TRI_LED_ONTIME_TYPE12,
    IR2E71Y_TRI_LED_ONTIME_TYPE13,
    NUM_IR2E71Y_TRI_LED_ONTIME
};

enum {
    IR2E71Y_KEY_BKL_ONTIME_TYPE0,
    IR2E71Y_KEY_BKL_ONTIME_TYPE1,
    IR2E71Y_KEY_BKL_ONTIME_TYPE2,
    IR2E71Y_KEY_BKL_ONTIME_TYPE3,
    IR2E71Y_KEY_BKL_ONTIME_TYPE4,
    IR2E71Y_KEY_BKL_ONTIME_TYPE5,
    IR2E71Y_KEY_BKL_ONTIME_TYPE6,
    IR2E71Y_KEY_BKL_ONTIME_TYPE7,
    NUM_IR2E71Y_KEY_BKL_ONTIME
};

enum {
    IR2E71Y_TRI_LED_COUNT_NONE,
    IR2E71Y_TRI_LED_COUNT_1,
    IR2E71Y_TRI_LED_COUNT_2,
    IR2E71Y_TRI_LED_COUNT_3,
    IR2E71Y_TRI_LED_COUNT_4,
    IR2E71Y_TRI_LED_COUNT_5,
    IR2E71Y_TRI_LED_COUNT_6,
    IR2E71Y_TRI_LED_COUNT_7,
    NUM_IR2E71Y_TRI_LED_COUNT
};

enum {
    IR2E71Y_LEDC_ONCOUNT_REPEAT,
    IR2E71Y_LEDC_ONCOUNT_1SHOT,
    NUM_IR2E71Y_LEDC_ONCOUNT
};

enum {
    IR2E71Y_LEDC_IS_NOT_EXIST,
    IR2E71Y_LEDC_IS_EXIST,
    NUM_LEDC_EXIST_STATUS
};


enum {
    IR2E71Y_LEDC_RGB_MODE_NORMAL,
    IR2E71Y_LEDC_RGB_MODE_PATTERN1,
    IR2E71Y_LEDC_RGB_MODE_PATTERN2,
    IR2E71Y_LEDC_RGB_MODE_PATTERN3,
    IR2E71Y_LEDC_RGB_MODE_PATTERN4,
    IR2E71Y_LEDC_RGB_MODE_PATTERN5,
    IR2E71Y_LEDC_RGB_MODE_PATTERN6,
    IR2E71Y_LEDC_RGB_MODE_PATTERN7,
    IR2E71Y_LEDC_RGB_MODE_PATTERN8,
    IR2E71Y_LEDC_RGB_MODE_PATTERN9,
    IR2E71Y_LEDC_RGB_MODE_PATTERN10,
    IR2E71Y_LEDC_RGB_MODE_PATTERN11,
    IR2E71Y_LEDC_RGB_MODE_PATTERN12,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION1,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION2,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION3,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION4,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION5,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION6,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION7,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION8,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION9,
    IR2E71Y_LEDC_RGB_MODE_ANIMATION10,
    NUM_IR2E71Y_LEDC_RGB_MODE
};

enum {
    IR2E71Y_PRE_SHUTDOWN,
    IR2E71Y_POST_SHUTDOWN,
};


struct ir2e71y_ledc_rgb {
    unsigned int mode;
    unsigned int red[2];
    unsigned int green[2];
    unsigned int blue[2];
};





struct ir2e71y_bdic_status {
    int power_status;
    unsigned int users;
};

struct ir2e71y_psals_status {
    int power_status;
    unsigned int als_users;
    int ps_um_status;
    int als_um_status;
};


struct ir2e71y_main_bkl_auto {
    int mode;
    int param;
};

struct ir2e71y_ave_ado {
    unsigned char  als_range;
    unsigned short ave_als0;
    unsigned short ave_als1;
    unsigned short ave_ado;
};


struct ir2e71y_trv_param {
    int            request;
    int            strength;
    int            adjust;
};

struct ir2e71y_rgb {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
};

struct ir2e71y_illumi_triple_color {
    struct ir2e71y_rgb colors[ILLUMI_FRAME_MAX];
    int count;
};


struct ir2e71y_main_bkl_ctl {
    int mode;
    int param;
};

struct ir2e71y_tri_led {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    int ext_mode;
    int led_mode;
    int ontime;
    int interval;
    int count;
};

struct ir2e71y_key_bkl_ctl {
    unsigned char key_left;
    unsigned char key_center;
    unsigned char key_right;
    int ontime;
    int interval;
};

struct ir2e71y_als_adjust {
    unsigned short als_adj0;
    unsigned short als_adj1;
    unsigned char als_shift;
    unsigned char clear_offset;
    unsigned char ir_offset;
};

struct ir2e71y_photo_sensor_adj {
    unsigned char status;
    unsigned char key_backlight;
    unsigned int chksum;
    struct ir2e71y_als_adjust als_adjust[2];
};

struct ir2e71y_ledc_req {
    unsigned int red[2];
    unsigned int green[2];
    unsigned int blue[2];
    int led_mode;
    int on_count;
};



int ir2e71y_API_get_bdic_is_exist(void);
void ir2e71y_API_psals_recovery_subscribe( void );
void ir2e71y_API_psals_recovery_unsubscribe(void);
int ir2e71y_API_check_upper_unit(void);
void ir2e71y_API_semaphore_start(void);
void ir2e71y_API_semaphore_end(void);

/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
struct ir2e71y_queue_data_t {
    int                 irq_GFAC;
    struct list_head    list;
};

enum {
    IR2E71Y_SUBSCRIBE_TYPE_INT,
    IR2E71Y_SUBSCRIBE_TYPE_POL,
    NUM_IR2E71Y_SUBSCRIBE_TYPE
};

enum {
    IR2E71Y_DEBUG_PROCESS_STATE_OUTPUT,
    IR2E71Y_DEBUG_TRACE_LOG_SWITCH,
    IR2E71Y_DEBUG_BDIC_I2C_WRITE,
    IR2E71Y_DEBUG_BDIC_I2C_READ,
    IR2E71Y_DEBUG_PROX_SENSOR_CTL,
    IR2E71Y_DEBUG_BKL_CTL,
    IR2E71Y_DEBUG_LIGHT_SENSOR_CTL,
    IR2E71Y_DEBUG_IRQ_LOGIC_CHK = 10,
    IR2E71Y_DEBUG_BDIC_IRQ_ALL_CLEAR = 11,
    IR2E71Y_DEBUG_BDIC_IRQ_CLEAR = 12,
    IR2E71Y_DEBUG_BDIC_WRITE = 20,
    IR2E71Y_DEBUG_BDIC_READ = 21,
    IR2E71Y_DEBUG_RGB_LED = 25,
    IR2E71Y_DEBUG_LED_REG_DUMP = 26,
    IR2E71Y_DEBUG_BDIC_RESTART = 27,
    IR2E71Y_DEBUG_RECOVERY_NG = 60,
    IR2E71Y_DEBUG_ALS_REPORT = 70,
};

enum {
    IR2E71Y_DEBUG_INFO_TYPE_BOOT,
    IR2E71Y_DEBUG_INFO_TYPE_KERNEL,
    IR2E71Y_DEBUG_INFO_TYPE_BDIC,
    IR2E71Y_DEBUG_INFO_TYPE_SENSOR,
    IR2E71Y_DEBUG_INFO_TYPE_POWERON,
    IR2E71Y_DEBUG_INFO_TYPE_PANEL,
    IR2E71Y_DEBUG_INFO_TYPE_PM,
    IR2E71Y_DEBUG_INFO_TYPE_BDIC_OPT,
    NUM_IR2E71Y_DEBUG_INFO_TYPE
};

enum {
    IR2E71Y_BKL_MODE_OFF,
    IR2E71Y_BKL_MODE_ON,
    IR2E71Y_BKL_MODE_AUTO,
    NUM_IR2E71Y_BKL_MODE
};

enum {
    IR2E71Y_ALS_IRQ_SUBSCRIBE_TYPE_BKL_CTRL,
    IR2E71Y_ALS_IRQ_SUBSCRIBE_TYPE_DBC_IOCTL,
    NUM_IR2E71Y_ALS_IRQ_SUBSCRIBE_TYPE
};

enum {
    IR2E71Y_DRIVER_IS_NOT_INITIALIZED,
    IR2E71Y_DRIVER_IS_INITIALIZED,
    NUM_IR2E71Y_DRIVER_STATUS
};

enum {
    IR2E71Y_IRQ_TYPE_ALS,
    IR2E71Y_IRQ_TYPE_PS,
    IR2E71Y_IRQ_TYPE_DET,
    IR2E71Y_IRQ_TYPE_I2CERR,
    IR2E71Y_IRQ_TYPE_ALS_REQ,
    NUM_IR2E71Y_IRQ_TYPE
};

struct ir2e71y_procfs {
    int id;
    int par[4];
};

struct ir2e71y_subscribe {
    int   irq_type;
    void (*callback)(void);
};

#endif /* IR2E71Y_KERL_PRIV_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
