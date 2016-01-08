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

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "himax_platform.h"

#define HIMAX852XES_NAME		"Himax852xes"
#define HIMAX_I2C_ADDR			0x48
#define INPUT_DEV_NAME			"himax-touchscreen"
#define FLASH_DUMP_FILE			"/data/user/Flash_Dump.bin"
#define DIAG_COORDINATE_FILE	"/sdcard/Coordinate_Dump.csv"

#define CONFIG_TOUCHSCREEN_HIMAX_DEBUG
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
#if 0
#define D(x...) printk(KERN_DEBUG "HXTP: " x)
#define I(x...) printk(KERN_INFO "HXTP: " x)
#define W(x...) printk(KERN_WARNING "HXTP: " x)
#define E(x...) printk(KERN_ERR "HXTP: " x)
#define DIF(x...) \
	if (debug_flag) \
	printk(KERN_DEBUG "[HXTP][DEBUG] " x) \
} while(0)
#else
#define D(x...) 
#define I(x...) 
#define W(x...) printk(KERN_WARNING "HXTP: " x)
#define E(x...) printk(KERN_ERR "HXTP: " x)
#define DIF(x...)
#endif

#define HX_TP_SYS_DIAG
#define HX_TP_SYS_SELF_TEST
#define HX_TP_SYS_RESET
//#define HX_TP_SYS_2T2R
#define HX_TP_SYS_DEBUG

/* Support Self Virtual key, default is close */
//#define HX_EN_SEL_BUTTON

/* Support Mutual Virtual Key, default is close */
#define HX_EN_MUT_BUTTON 

#else
#define D(x...) 
#define I(x...) 
#define W(x...) 
#define E(x...) 
#define DIF(x...)
#endif

//#define HX_LOADIN_CONFIG
/* if enable HX_AUTO_UPDATE_FW, need to disable HX_LOADIN_CONFIG */
#define HX_AUTO_UPDATE_FW
//#define HX_SMART_WAKEUP
//#define HX_PALM_REPORT
#define HX_ESD_WORKAROUND

#ifdef HX_ESD_WORKAROUND
#define HX_ESD_MONITOR
#define HX_ESD_MONITOR_INTERVAL		2000
#endif

#define HX_85XX_A_SERIES_PWON		1
#define HX_85XX_B_SERIES_PWON		2
#define HX_85XX_C_SERIES_PWON		3
#define HX_85XX_D_SERIES_PWON		4
#define HX_85XX_E_SERIES_PWON		5
#define HX_85XX_ES_SERIES_PWON		6

#define HX_TP_BIN_CHECKSUM_SW		1
#define HX_TP_BIN_CHECKSUM_HW		2
#define HX_TP_BIN_CHECKSUM_CRC		3
	
#define HX_KEY_MAX_COUNT			4			
#define DEFAULT_RETRY_CNT			3

#define HX_VKEY_0   KEY_BACK
#define HX_VKEY_1   KEY_HOME
#define HX_VKEY_2   KEY_RESERVED
#define HX_VKEY_3   KEY_RESERVED
#define HX_KEY_ARRAY    {HX_VKEY_0, HX_VKEY_1, HX_VKEY_2, HX_VKEY_3}

#define SHIFTBITS	5
#define FLASH_SIZE	32768

struct himax_virtual_key {
	int index;
	int keycode;
	int x_range_min;
	int x_range_max;
	int y_range_min;
	int y_range_max;
};

struct himax_config {
	uint8_t default_cfg;
	uint8_t sensor_id;
	uint8_t fw_ver;
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
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct himax_i2c_platform_data *pdata;	
	struct himax_virtual_key *button;
#ifdef HX_ESD_MONITOR
	struct delayed_work esd_dwork;
#endif
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	int rst_gpio;
#ifdef HX_SMART_WAKEUP
	bool smwake_enable;
	struct wake_lock smwake_wake_lock;
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

static unsigned char E_IrefTable_1[16][2] = { /* 1uA */
	{0x20,0x0F},{0x20,0x1F},{0x20,0x2F},{0x20,0x3F},
	{0x20,0x4F},{0x20,0x5F},{0x20,0x6F},{0x20,0x7F},
	{0x20,0x8F},{0x20,0x9F},{0x20,0xAF},{0x20,0xBF},
	{0x20,0xCF},{0x20,0xDF},{0x20,0xEF},{0x20,0xFF}
};

static unsigned char E_IrefTable_2[16][2] = { /* 2uA */
	{0xA0,0x0E},{0xA0,0x1E},{0xA0,0x2E},{0xA0,0x3E},
	{0xA0,0x4E},{0xA0,0x5E},{0xA0,0x6E},{0xA0,0x7E},
	{0xA0,0x8E},{0xA0,0x9E},{0xA0,0xAE},{0xA0,0xBE},
	{0xA0,0xCE},{0xA0,0xDE},{0xA0,0xEE},{0xA0,0xFE}
};													

static unsigned char E_IrefTable_3[16][2] = { /* 3uA */
	{0x20,0x0E},{0x20,0x1E},{0x20,0x2E},{0x20,0x3E},
	{0x20,0x4E},{0x20,0x5E},{0x20,0x6E},{0x20,0x7E},
	{0x20,0x8E},{0x20,0x9E},{0x20,0xAE},{0x20,0xBE},
	{0x20,0xCE},{0x20,0xDE},{0x20,0xEE},{0x20,0xFE}
};			

static unsigned char E_IrefTable_4[16][2] = { /* 4uA */
	{0xA0,0x0D},{0xA0,0x1D},{0xA0,0x2D},{0xA0,0x3D},
	{0xA0,0x4D},{0xA0,0x5D},{0xA0,0x6D},{0xA0,0x7D},
	{0xA0,0x8D},{0xA0,0x9D},{0xA0,0xAD},{0xA0,0xBD},
	{0xA0,0xCD},{0xA0,0xDD},{0xA0,0xED},{0xA0,0xFD}
};	 

static unsigned char E_IrefTable_5[16][2] = { /* 5uA */
	{0x20,0x0D},{0x20,0x1D},{0x20,0x2D},{0x20,0x3D},
	{0x20,0x4D},{0x20,0x5D},{0x20,0x6D},{0x20,0x7D},
	{0x20,0x8D},{0x20,0x9D},{0x20,0xAD},{0x20,0xBD},
	{0x20,0xCD},{0x20,0xDD},{0x20,0xED},{0x20,0xFD}
};	 

static unsigned char E_IrefTable_6[16][2] = { /* 6uA */
	{0xA0,0x0C},{0xA0,0x1C},{0xA0,0x2C},{0xA0,0x3C},
	{0xA0,0x4C},{0xA0,0x5C},{0xA0,0x6C},{0xA0,0x7C},
	{0xA0,0x8C},{0xA0,0x9C},{0xA0,0xAC},{0xA0,0xBC},
	{0xA0,0xCC},{0xA0,0xDC},{0xA0,0xEC},{0xA0,0xFC}
};	 

static unsigned char E_IrefTable_7[16][2] = { /* 7uA */
	{0x20,0x0C},{0x20,0x1C},{0x20,0x2C},{0x20,0x3C},
	{0x20,0x4C},{0x20,0x5C},{0x20,0x6C},{0x20,0x7C},
	{0x20,0x8C},{0x20,0x9C},{0x20,0xAC},{0x20,0xBC},
	{0x20,0xCC},{0x20,0xDC},{0x20,0xEC},{0x20,0xFC}
};									

#ifdef HX_TP_SYS_DIAG
#ifdef HX_TP_SYS_2T2R
	static bool Is_2T2R = false;
	static uint8_t x_channel_2 = 0;
	static uint8_t y_channel_2 = 0;
	static uint8_t *diag_mutual_2 = NULL;

	static uint8_t *getMutualBuffer_2(void);
	static uint8_t 	getXChannel_2(void);
	static uint8_t 	getYChannel_2(void);

	static void 	setMutualBuffer_2(void);
	static void 	setXChannel_2(uint8_t x);
	static void 	setYChannel_2(uint8_t y);
#endif
	static uint8_t x_channel 		= 0;
	static uint8_t y_channel 		= 0;
	static uint8_t *diag_mutual = NULL;

	static int diag_command = 0;
	static uint8_t diag_coor[128];
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

static void himax_chip_reset(bool loadconfig, bool int_off);

#ifdef HX_TP_SYS_SELF_TEST
static ssize_t himax_chip_self_test_function(struct device *dev,
	struct device_attribute *attr, char *buf);
static int himax_chip_self_test(void);
#endif

#ifdef HX_SMART_WAKEUP
	static bool FAKE_POWER_KEY_SEND = false;
#endif

extern int irq_enable_count;
#endif
