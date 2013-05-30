/*
 * Copyright (C) 2012 LGE.
 *
 * Writer: yehan.ahn@lge.com, 	hyesung.shin@lge.com
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

#ifndef LGE_TOUCH_SYNAPTICS_H
#define LGE_TOUCH_SYNAPTICS_H

#define MAX_FINGER                      10

/* Debug Mask setting */
#define TOUCH_DEBUG_PRINT (0)
#define TOUCH_ERROR_PRINT (1)
#define TOUCH_INFO_PRINT (1)

#if TOUCH_INFO_PRINT
#define TOUCH_INFO_MSG(fmt, args...) \
		printk(KERN_INFO "[Touch] " fmt, ##args)
#else
#define TOUCH_INFO_MSG(fmt, args...)
#endif

#if TOUCH_ERROR_PRINT
#define TOUCH_ERR_MSG(fmt, args...) \
	printk(KERN_ERR "[Touch E] [%s %d] "\
				fmt, __FUNCTION__, __LINE__, ##args)
#else
#define TOUCH_ERR_MSG(fmt, args...)
#endif

#if TOUCH_DEBUG_PRINT
#define TOUCH_DEBUG_MSG(fmt, args...) \
		printk(KERN_INFO "[Touch D] [%s %d] " \
				fmt, __FUNCTION__, __LINE__, ##args)
#else
#define TOUCH_DEBUG_MSG(fmt, args...)
#endif

#define DESCRIPTION_TABLE_START 0xe9
#define PAGE_SELECT_REG	        0xFF        /* Button exists Page 02 */
#define PAGE_MAX_NUM            4           /* number of page register */

struct touch_platform_data {
	u32             max_id;
	u32             max_major;
	u32             max_pres;
	u32             max_x;
	u32             max_y;
	u32             lcd_x;
	u32             lcd_y;
	int             reset_gpio;
	u32             reset_gpio_flags;
	int             irq_gpio;
	u32             irq_gpio_flags;
	int             type;		/* Used when choosing firmware */
};

struct fw_upgrade_info {
	char            fw_path[256];
	u8              fw_force_upgrade;
	volatile        u8 is_downloading;
};

struct synaptics_ts_fw_info {
	u8              fw_rev;
	u8              fw_image_rev;
	u8              manufacturer_id;
	u8              product_id[11];
	u8              fw_image_product_id[11];
	u8              config_id[5];
	u8              image_config_id[5];
	unsigned char   *fw_start;
	unsigned long   fw_size;
	struct fw_upgrade_info fw_upgrade;
	u8              ic_fw_identifier[31];   /* String */
};

struct t_data {
	u16             state;
	u16             x_position;
	u16             y_position;
	u16             width_major;
	u16             width_minor;
	u16             pressure;
};

struct touch_data {
	u8              device_status_reg;	/* DEVICE_STATUS_REG */
	u8              interrupt_status_reg;

	u8              total_num;
	u8              prev_total_num;
	u8              state;
	u8              palm;
	struct t_data   curr_data[MAX_FINGER];
};

struct touch_device_driver {
	int (*probe)(struct i2c_client *client);
	void (*remove)(struct i2c_client *client);
	int (*init)(struct i2c_client *client,
				struct synaptics_ts_fw_info* info);
	int (*data)(struct i2c_client *client, struct touch_data* data);
	int (*power)(struct i2c_client *client, int power_ctrl);
	int (*ic_ctrl)(struct i2c_client *client, u8 code, u16 value);
	int (*fw_upgrade)(struct i2c_client *client,
				struct synaptics_ts_fw_info* info);
};

struct function_descriptor {
	u8              query_base;
	u8              command_base;
	u8              control_base;
	u8              data_base;
	u8              int_source_count;
	u8              id;
};

struct ts_ic_function {
	struct function_descriptor dsc;
	u8              function_page;
};

struct synaptics_ts_data {
	u8                      is_probed;
	struct i2c_client       *client;
	struct touch_platform_data *pdata;
	struct ts_ic_function   common_fc;
	struct ts_ic_function   finger_fc;
	struct ts_ic_function   button_fc;
	struct ts_ic_function   analog_fc;	/* FIXME: not used in ClearPad3000 serise */
	struct ts_ic_function   flash_fc;
	struct touch_data       ts_data;
	struct synaptics_ts_fw_info	fw_info;

	void *h_touch;
	atomic_t                device_init;
	u8                      work_sync_err_cnt;
	u8                      ic_init_err_cnt;
	volatile int            curr_pwr_state;
	int                     curr_resume_state;
	int                     int_pin_state;
	struct input_dev        *input_dev;
	struct delayed_work     work_init;
	struct work_struct      work_fw_upgrade;
	struct work_struct      work_recover;
	struct kobject          lge_touch_kobj;
	struct notifier_block   notif;
};

enum{
	POLLING_MODE = 0,
	INTERRUPT_MODE,
	HYBRIDE_MODE
};

enum{
	POWER_OFF = 0,
	POWER_ON,
	POWER_SLEEP,
	POWER_WAKE
};

enum{
	CONTINUOUS_REPORT_MODE = 0,
	REDUCED_REPORT_MODE,
};

enum{
	DOWNLOAD_COMPLETE = 0,
	UNDER_DOWNLOADING,
};

enum {
	DO_NOT_ANYTHING = 0,
	ABS_PRESS,
	ABS_RELEASE,
};

enum {
	FINGER_STATE_NO_PRESENT = 0,
	FINGER_STATE_PRESENT_VALID,
	FINGER_STATE_PRESENT_NOVALID,
	FINGER_STATE_RESERVED
};

enum{
	BASELINE_OPEN = 0,
	BASELINE_FIX,
	BASELINE_REBASE,
};

enum{
	IC_CTRL_CODE_NONE = 0,
	IC_CTRL_BASELINE,
	IC_CTRL_READ,
	IC_CTRL_WRITE,
	IC_CTRL_RESET_CMD,
};

enum{
	DEBUG_NONE              = 0,
	DEBUG_BASE_INFO         = (1U << 0),
	DEBUG_TRACE             = (1U << 1),
	DEBUG_GET_DATA          = (1U << 2),
	DEBUG_ABS               = (1U << 3),
	DEBUG_BUTTON            = (1U << 4),
	DEBUG_FW_UPGRADE        = (1U << 5),
	DEBUG_GHOST             = (1U << 6),
	DEBUG_IRQ_HANDLE        = (1U << 7),
	DEBUG_POWER             = (1U << 8),
	DEBUG_JITTER            = (1U << 9),
	DEBUG_ACCURACY          = (1U << 10),
};

enum{
	WORK_POST_COMPLETE = 0,
	WORK_POST_OUT,
	WORK_POST_ERR_RETRY,
	WORK_POST_ERR_CIRTICAL,
	WORK_POST_MAX,
};

#endif
