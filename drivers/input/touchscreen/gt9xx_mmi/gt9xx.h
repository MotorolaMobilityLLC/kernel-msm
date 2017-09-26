/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2010 - 2016 Goodix. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 2.4.0.1
 * Release Date: 2016/10/26
 */

#ifndef _GOODIX_GT9XX_H_
#define _GOODIX_GT9XX_H_

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/usb.h>
#include <linux/power_supply.h>

#define MAX_BUTTONS 4
#define GTP_CONFIG_MAX_LENGTH 240
#define GTP_ADDR_LENGTH       2

/***************************PART1:ON/OFF define*******************************/
#define GTP_CUSTOM_CFG        0

#define GTP_DEBUG_ON          1
#define GTP_DEBUG_ARRAY_ON    0
#define GTP_DEBUG_FUNC_ON     0

struct goodix_modifiers {
	int	mods_num;
	struct semaphore list_sema;
	struct list_head mod_head;

};

struct config_modifier {
	const char *name;
	int id;
	bool effective;
	struct list_head link;
};

struct goodix_ts_platform_data {
	int irq_gpio;
	int rst_gpio;
	u32 rst_gpio_flags;
	u32 irq_gpio_flags;
	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;
	u32 panel_minx;
	u32 panel_miny;
	u32 panel_maxx;
	u32 panel_maxy;
	u32 button_map[MAX_BUTTONS];
	u8 num_button;
	u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH];
	bool force_update;
	bool i2c_pull_up;
	bool have_touch_key;
	bool driver_send_cfg;
	bool change_x2y;
	bool with_pen;
	bool slide_wakeup;
	bool dbl_clk_wakeup;
	bool auto_update;
	bool auto_update_cfg;
	bool esd_protect;
	bool wakeup_with_reset;
	bool ics_slot_report;
	bool create_wr_node;
	bool resume_in_workqueue;
	bool coordinate_scale;
	bool enable_int_as_output;
};

struct goodix_ts_data {
	spinlock_t irq_lock;
	spinlock_t esd_lock;
	struct i2c_client *client;
	struct input_dev  *input_dev;
	struct goodix_ts_platform_data *pdata;
	struct hrtimer timer;
	struct mutex lock;
	struct notifier_block ps_notif;
	struct input_dev *pen_dev;
	struct regulator *vdd_ana;
	struct regulator *vcc_i2c;
	struct goodix_modifiers modifiers;
#if defined(CONFIG_FB)
	struct notifier_block notifier;
	struct work_struct fb_notify_work;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	u8  max_touch_num;
	u8  int_trigger_type;
	u8  green_wake_mode;
	u8  enter_update;
	u8  gtp_rawdiff_mode;
	u8  fw_error;
	u8  pnl_init_error;
	u8  esd_running;
	u8  product_id[4];
	u16 abs_x_max;
	u16 abs_y_max;
	u16 version_info;
	s32 clk_tick_cnt;
	int  gtp_cfg_len;
	bool use_irq;
	bool fw_loading;
	bool force_update;
	bool ps_is_present;
	bool init_done;
	bool power_on;
	bool irq_enabled;
	bool gtp_suspended;
	/* moto TP modifiers */
	bool patching_enabled;
	bool charger_detection_enabled;
	struct work_struct ps_notify_work;
};

extern u16 show_len;
extern u16 total_len;

/************************* PART2:TODO define *******************************/
/* STEP_1(REQUIRED): Define Configuration Information Group(s)
 Sensor_ID Map:
	 sensor_opt1 sensor_opt2 Sensor_ID
		GND         GND          0
		VDDIO      GND          1
		NC           GND          2
		GND         NC/300K    3
		VDDIO      NC/300K    4
		NC           NC/300K    5
*/
/* TODO: define your own default or for Sensor_ID == 0 config here.
	 The predefined one is just a sample config,
	 which is not suitable for your tp in most cases. */
#define CTP_CFG_GROUP0 {\
	0x41, 0xD0, 0x02, 0x00, 0x05, 0x0A, 0x34, \
	0x00, 0x01, 0x08, 0x28, 0x05, 0x50, 0x32, \
	0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x17, 0x19, 0x1E, 0x14, 0x8C, \
	0x2D, 0x0E, 0x3C, 0x3E, 0x82, 0x0A, 0x82, \
	0x0A, 0x00, 0x99, 0x33, 0x1D, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x2B, 0x19, 0x64, 0x94, 0xC0, 0x02, \
	0x08, 0x00, 0x00, 0x04,	0xF2, 0x1C, 0x00, \
	0xB9, 0x26, 0x00, 0x93, 0x32, 0x00, 0x77, \
	0x42, 0x00, 0x62, 0x57, 0x00, 0x62, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0xFF, 0x65, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x19, 0x46, 0x00, 0x00, 0x00, 0x00, 0x32, \
	0x1C, 0x1A, 0x18, 0x16, 0x14, 0x12, 0x10, \
	0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x02,	0x04, 0x06, 0x08, \
	0x0A, 0x0C, 0x0F, 0x10, 0x12, 0x13, 0x14, \
	0x18, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, \
	0x22, 0x24, 0x26, 0x28, 0x29, 0x2A, 0xFF, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	0x00, 0x00, 0xB8, 0x01\
}

/* TODO: define your config for Sensor_ID == 1 here, if needed */
#define CTP_CFG_GROUP1 {\
}

/* TODO: define your config for Sensor_ID == 2 here, if needed */
#define CTP_CFG_GROUP2 {\
}

/* TODO: define your config for Sensor_ID == 3 here, if needed */
#define CTP_CFG_GROUP3 {\
}
/* TODO: define your config for Sensor_ID == 4 here, if needed */
#define CTP_CFG_GROUP4 {\
}

/* TODO: define your config for Sensor_ID == 5 here, if needed */
#define CTP_CFG_GROUP5 {\
}

/* STEP_2(REQUIRED): Customize your I/O ports & I/O operations */
#define GTP_RST_PORT    64 /* EXYNOS4_GPX2(0) */
#define GTP_INT_PORT    65 /* EXYNOS4_GPX2(1) */

#define GTP_GPIO_AS_INPUT(pin)          (gpio_direction_input(pin))
#define GTP_GPIO_AS_INT(pin)            (GTP_GPIO_AS_INPUT(pin))
#define GTP_GPIO_GET_VALUE(pin)         gpio_get_value(pin)
#define GTP_GPIO_OUTPUT(pin, level)      gpio_direction_output(pin, level)
#define GTP_GPIO_REQUEST(pin, label)    gpio_request(pin, label)
#define GTP_GPIO_FREE(pin)              gpio_free(pin)

/* STEP_3(optional): Specify your special config info if needed */
#if GTP_CUSTOM_CFG
	#define GTP_MAX_HEIGHT   800
	#define GTP_MAX_WIDTH    480
	#define GTP_INT_TRIGGER  0            /* 0: Rising 1: Falling */
#else
	#define GTP_MAX_HEIGHT   4096
	#define GTP_MAX_WIDTH    4096
	#define GTP_INT_TRIGGER  1
#endif
#define GTP_MAX_TOUCH         10

/* STEP_4(optional): If keys are available and reported as keys,
config your key info here */
#define GTP_KEY_TAB {KEY_MENU, KEY_HOME, KEY_BACK, KEY_HOMEPAGE, \
	KEY_F1, KEY_F2, KEY_F3}

/**************************PART3:OTHER define*******************************/
#define GTP_DRIVER_VERSION	"V2.4.0.1<2016/10/26>"
#define GTP_I2C_NAME		"Goodix-TS"
#define GT91XX_CONFIG_PROC_FILE	"gt9xx_config"
#define GTP_POLL_TIME		10
#define GTP_CONFIG_MIN_LENGTH	186
#define GTP_ESD_CHECK_VALUE	0xAA
#define RETRY_MAX_TIMES		5
#define PEN_TRACK_ID		9
#define MASK_BIT_8		0x80
#define FAIL			0
#define SUCCESS			1
#define SWITCH_OFF		0
#define SWITCH_ON		1

/* Registers define */
#define GTP_REG_VERSION		0x8140
#define GTP_REG_ESD_CHECK	0x8141
#define GTP_REG_SENSOR_ID	0x814A
#define GTP_REG_DOZE_BUF	0x814B
#define GTP_READ_COOR_ADDR	0x814E
#define GTP_REG_SLEEP		0x8040
#define GTP_REG_COMMAND		0x8040
#define GTP_REG_COMMAND_CHECK	0x8046
#define GTP_REG_CONFIG_DATA	0x8047

/* Sleep time define */
#define GTP_1_DLY_MS		1
#define GTP_2_DLY_MS		2
#define GTP_10_DLY_MS		10
#define GTP_20_DLY_MS		20
#define GTP_50_DLY_MS		50
#define GTP_58_DLY_MS		58
#define GTP_100_DLY_MS		100
#define GTP_500_DLY_MS		500
#define GTP_1000_DLY_MS		1000
#define GTP_3000_DLY_MS		3000

#define RESOLUTION_LOC        3
#define TRIGGER_LOC           8

#define CFG_GROUP_LEN(p_cfg_grp)  (sizeof(p_cfg_grp) / sizeof(p_cfg_grp[0]))
/* Log define */
#define GTP_DEBUG(fmt, arg...) \
do { \
	if (GTP_DEBUG_ON) {\
		pr_info("<<-GTP-DEBUG->> [%d]"fmt"\n", __LINE__, ##arg);\
	} \
} while (0)
#define GTP_DEBUG_ARRAY(array, num) \
do { \
	s32 i;\
	u8 *a = array;\
	if (GTP_DEBUG_ARRAY_ON) {\
		pr_warn("<<-GTP-DEBUG-ARRAY->>\n");\
		for (i = 0; i < (num); i++) {\
			pr_warn("%02x  ", (a)[i]);\
			if ((i + 1) % 10 == 0) {\
				pr_warn("\n");\
			} \
		} \
		pr_warn("\n");\
	} \
} while (0)
#define GTP_DEBUG_FUNC() \
do {\
	if (GTP_DEBUG_FUNC_ON) {\
		pr_warn("<<-GTP-FUNC->>  Func:%s@Line:%d\n", \
		__func__, __LINE__);\
	} \
} while (0)
#define GTP_SWAP(x, y) \
do {\
	typeof(x) z = x;\
	x = y;\
	y = z;\
} while (0)

/******************************End of Part III********************************/
#ifdef CONFIG_OF
extern int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid);
#endif

extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern void gtp_int_sync(struct goodix_ts_data *ts, s32 ms);
extern void gtp_esd_switch(struct i2c_client *, s32);
extern int gtp_irq_control_enable(struct goodix_ts_data *ts, bool enable);
extern u8 gup_init_update_proc(struct goodix_ts_data *);
extern s32 gtp_send_cfg(struct i2c_client *client);
extern s32 gup_update_proc(void *dir);
extern void goodix_resume_ps_chg_cm_state(struct goodix_ts_data *data);

extern s32 init_wr_node(struct i2c_client *);
extern void uninit_wr_node(void);

/*********** For gt9xx_update Start *********/
extern struct i2c_client *i2c_connect_client;
extern void gtp_reset_guitar(struct i2c_client *client, s32 ms);
extern s32 gtp_send_cfg(struct i2c_client *client);
extern s32 gtp_read_version(struct i2c_client *, u16*, u8*);
extern s32 gtp_i2c_read_dbl_check(struct i2c_client *, u16, u8 *, int);
extern s32 gtp_fw_startup(struct i2c_client *client);
/*********** For gt9xx_update End *********/

/*********** For goodix_tool Start *********/
#define UPDATE_FUNCTIONS
#ifdef UPDATE_FUNCTIONS
extern s32 gup_enter_update_mode(struct i2c_client *client);
extern s32 gup_update_proc(void *dir);
extern void gup_leave_update_mode(struct i2c_client *client);
#endif
/*********** For goodix_tool End *********/

#endif /* _GOODIX_GT9XX_H_ */
