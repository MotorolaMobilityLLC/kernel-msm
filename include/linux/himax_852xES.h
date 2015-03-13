/* Himax Android Driver Sample Code Ver 0.3 for HMX852xES chipset
*
* Copyright (C) 2014 Himax Corporation.
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

#ifndef HIMAX852xES_H
#define HIMAX852xES_H

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/firmware.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/wakelock.h>
#include <linux/himax_platform.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>

#endif
#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
#include <linux/touch_psensor.h>
#endif

#define HIMAX852xes_NAME "Himax852xes"
#define HIMAX852xes_FINGER_SUPPORT_NUM		10
#define HIMAX_I2C_ADDR				0x48
#define INPUT_DEV_NAME	"himax-touchscreen"
#define FLASH_DUMP_FILE "/data/user/Flash_Dump.bin"
#define DIAG_COORDINATE_FILE "/sdcard/Coordinate_Dump.csv"

#define CONFIG_TOUCHSCREEN_HIMAX_DEBUG
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#define D(x...) printk(KERN_DEBUG "[HXTP] " x)
#define I(x...) printk(KERN_INFO "[HXTP] " x)
#define W(x...) printk(KERN_WARNING "[HXTP][WARNING] " x)
#define E(x...) printk(KERN_ERR "[HXTP][ERROR] " x)
#define DIF(x...) \
	if (debug_flag) \
	printk(KERN_DEBUG "[HXTP][DEBUG] " x) \
} while(0)

#define HX_TP_SYS_DIAG
//#define HX_TP_SYS_DEBUG
#define HX_TP_SYS_RESET
#define HX_TP_SYS_HITOUCH

//#define HX_EN_SEL_BUTTON	// Support Self Virtual key,default is close
#define HX_EN_MUT_BUTTON	// Support Mutual Virtual Key,default is close

#else
#define D(x...)
#define I(x...)
#define W(x...)
#define E(x...)
#define DIF(x...)
#endif
//===========Himax Option function=============
#define HX_RST_PIN_FUNC
//#define HX_SMART_WAKEUP
//#define HX_DOT_VIEW
//#define HX_PALM_REPORT
#define HX_ESD_WORKAROUND
//#define HX_USB_DETECT

#define HX_85XX_A_SERIES_PWON		1
#define HX_85XX_B_SERIES_PWON		2
#define HX_85XX_C_SERIES_PWON		3
#define HX_85XX_D_SERIES_PWON		4
#define HX_85XX_E_SERIES_PWON		5
#define HX_85XX_ES_SERIES_PWON		6

#define HX_TP_BIN_CHECKSUM_SW		1
#define HX_TP_BIN_CHECKSUM_HW		2
#define HX_TP_BIN_CHECKSUM_CRC		3

#define HX_KEY_MAX_COUNT		4
#define DEFAULT_RETRY_CNT		3

#define HX_VKEY_0   KEY_BACK
#define HX_VKEY_1   KEY_HOME
#define HX_VKEY_2   KEY_RESERVED
#define HX_VKEY_3   KEY_RESERVED
#define HX_KEY_ARRAY    {HX_VKEY_0, HX_VKEY_1, HX_VKEY_2, HX_VKEY_3}

#define SHIFTBITS			5
#define FLASH_SIZE			32768

struct himax_virtual_key {
	int index;
	int keycode;
	int x_range_min;
	int x_range_max;
	int y_range_min;
	int y_range_max;
};

struct himax_config {
	uint8_t  default_cfg;
	uint8_t  sensor_id;
	uint8_t  fw_ver;
	uint16_t length;
	uint32_t tw_x_min;
	uint32_t tw_x_max;
	uint32_t tw_y_min;
	uint32_t tw_y_max;
	uint32_t pl_x_min;
	uint32_t pl_x_max;
	uint32_t pl_y_min;
	uint32_t pl_y_max;
	uint8_t c1[11];
	uint8_t c2[11];
	uint8_t c3[11];
	uint8_t c4[11];
	uint8_t c5[11];
	uint8_t c6[11];
	uint8_t c7[11];
	uint8_t c8[11];
	uint8_t c9[11];
	uint8_t c10[11];
	uint8_t c11[11];
	uint8_t c12[11];
	uint8_t c13[11];
	uint8_t c14[11];
	uint8_t c15[11];
	uint8_t c16[11];
	uint8_t c17[11];
	uint8_t c18[17];
	uint8_t c19[15];
	uint8_t c20[5];
	uint8_t c21[11];
	uint8_t c22[4];
	uint8_t c23[3];
	uint8_t c24[3];
	uint8_t c25[4];
	uint8_t c26[2];
	uint8_t c27[2];
	uint8_t c28[2];
	uint8_t c29[2];
	uint8_t c30[2];
	uint8_t c31[2];
	uint8_t c32[2];
	uint8_t c33[2];
	uint8_t c34[2];
	uint8_t c35[3];
	uint8_t c36[5];
	uint8_t c37[5];
	uint8_t c38[9];
	uint8_t c39[14];
	uint8_t c40[159];
	uint8_t c41[99];
};

struct himax_ts_data {
	bool suspended;
	atomic_t suspend_mode;
	uint8_t x_channel;
	uint8_t y_channel;
	uint8_t useScreenRes;
	uint8_t diag_command;

	uint8_t protocol_type;
	uint8_t first_pressed;
	uint8_t coord_data_size;
	uint8_t area_data_size;
	uint8_t raw_data_frame_size;
	uint8_t raw_data_nframes;
	uint8_t nFinger_support;
	uint8_t irq_enabled;
	uint8_t diag_self[50];

	uint16_t finger_pressed;
	uint16_t last_slot;
	uint16_t pre_finger_mask;

	uint32_t debug_log_level;
	uint32_t widthFactor;
	uint32_t heightFactor;
	uint32_t tw_x_min;
	uint32_t tw_x_max;
	uint32_t tw_y_min;
	uint32_t tw_y_max;
	uint32_t pl_x_min;
	uint32_t pl_x_max;
	uint32_t pl_y_min;
	uint32_t pl_y_max;

	int use_irq;
	int vendor_fw_ver;
	int vendor_config_ver;
	int vendor_sensor_id;
	int (*power)(int on);
	int pre_finger_data[10][2];

	struct device *dev;
	struct workqueue_struct *himax_wq;
	struct work_struct work;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct i2c_client *client;
	struct himax_i2c_platform_data *pdata;
	struct himax_virtual_key *button;

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
	struct workqueue_struct *himax_att_wq;
	struct delayed_work work_att;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
#if defined(CONFIG_TOUCHSCREEN_PROXIMITY)
	struct wake_lock ts_wake_lock;
#endif
#ifdef HX_RST_PIN_FUNC
	int rst_gpio;
#endif

#ifdef HX_SMART_WAKEUP
	uint8_t SMWP_enable;
	struct wake_lock ts_SMWP_wake_lock;
#endif
#ifdef HX_DOT_VIEW
	uint8_t cover_enable;
#endif
#ifdef HX_USB_DETECT
	uint8_t usb_connected;
	uint8_t *cable_config;
#endif
};

static struct himax_ts_data *private_ts;

#define HX_CMD_NOP					 0x00
#define HX_CMD_SETMICROOFF			 0x35
#define HX_CMD_SETROMRDY			 0x36
#define HX_CMD_TSSLPIN				 0x80
#define HX_CMD_TSSLPOUT 			 0x81
#define HX_CMD_TSSOFF				 0x82
#define HX_CMD_TSSON				 0x83
#define HX_CMD_ROE					 0x85
#define HX_CMD_RAE					 0x86
#define HX_CMD_RLE					 0x87
#define HX_CMD_CLRES				 0x88
#define HX_CMD_TSSWRESET			 0x9E
#define HX_CMD_SETDEEPSTB			 0xD7
#define HX_CMD_SET_CACHE_FUN		 0xDD
#define HX_CMD_SETIDLE				 0xF2
#define HX_CMD_SETIDLEDELAY 		 0xF3
#define HX_CMD_SELFTEST_BUFFER		 0x8D
#define HX_CMD_MANUALMODE			 0x42
#define HX_CMD_FLASH_ENABLE 		 0x43
#define HX_CMD_FLASH_SET_ADDRESS	 0x44
#define HX_CMD_FLASH_WRITE_REGISTER  0x45
#define HX_CMD_FLASH_SET_COMMAND	 0x47
#define HX_CMD_FLASH_WRITE_BUFFER	 0x48
#define HX_CMD_FLASH_PAGE_ERASE 	 0x4D
#define HX_CMD_FLASH_SECTOR_ERASE	 0x4E
#define HX_CMD_CB					 0xCB
#define HX_CMD_EA					 0xEA
#define HX_CMD_4A					 0x4A
#define HX_CMD_4F					 0x4F
#define HX_CMD_B9					 0xB9
#define HX_CMD_76					 0x76

enum input_protocol_type {
	PROTOCOL_TYPE_A	= 0x00,
	PROTOCOL_TYPE_B	= 0x01,
};

enum handshaking_result {
	RESULT_RUN = 0,
	RESULT_STOP,
	I2C_FAIL,
};

#ifdef HX_ESD_WORKAROUND
	static u8 		ESD_RESET_ACTIVATE 	= 1;
	static u8 		ESD_COUNTER 		= 0;
	//static int 		ESD_COUNTER_SETTING = 3;
	static u8		ESD_R36_FAIL		= 0;
#endif

#ifdef HX_TP_SYS_DIAG
	static uint8_t x_channel 		= 0;
	static uint8_t y_channel 		= 0;
	static uint8_t *diag_mutual		= NULL;

	static int diag_command = 0;
	static uint8_t diag_coor[128];// = {0xFF};
	static uint8_t diag_self[100] = {0};

	static uint8_t *getMutualBuffer(void);
	static uint8_t *getSelfBuffer(void);
	static uint8_t 	getDiagCommand(void);
	static uint8_t 	getXChannel(void);
	static uint8_t 	getYChannel(void);

	static void 	setMutualBuffer(void);
	static void 	setXChannel(uint8_t x);
	static void 	setYChannel(uint8_t y);

	static uint8_t	coordinate_dump_enable = 0;
	struct file	*coordinate_fn;
#endif

#ifdef HX_TP_SYS_DEBUG
	static bool	fw_update_complete = false;
	static int handshaking_result = 0;
	static unsigned char debug_level_cmd = 0;
	static unsigned char upgrade_fw[32*1024];
#endif

#ifdef HX_RST_PIN_FUNC
	void himax_HW_reset(uint8_t loadconfig,uint8_t int_off);
#endif

#ifdef HX_TP_SYS_HITOUCH
	static int	hitouch_command			= 0;
	static bool hitouch_is_connect	= false;
#endif

#ifdef HX_SMART_WAKEUP
	static bool FAKE_POWER_KEY_SEND = false;
#endif

#ifdef HX_DOT_VIEW
	#include <linux/hall_sensor.h>
#endif

#ifdef HX_AUTO_UPDATE_CONFIG
static int CFB_START_ADDR			= 0;
static int CFB_LENGTH				= 0;
static int CFB_INFO_LENGTH 			= 0;
#endif

extern int irq_enable_count;

#endif
