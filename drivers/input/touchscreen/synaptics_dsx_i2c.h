/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SYNAPTICS_DSX_RMI4_H_
#define _SYNAPTICS_DSX_RMI4_H_

#define SYNAPTICS_DSX_DRIVER_VERSION "DSX 1.1"

#include <linux/version.h>
#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
#include <mach/mmi_panel_notifier.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 38))
#define KERNEL_ABOVE_2_6_38
#endif

#ifdef KERNEL_ABOVE_2_6_38
#define sstrtoul(...) kstrtoul(__VA_ARGS__)
#else
#define sstrtoul(...) strict_strtoul(__VA_ARGS__)
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 7, 0))
#define KERNEL_ABOVE_3_7
#endif

#define PDT_PROPS (0x00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x000A)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1a)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)

#define SYNAPTICS_RMI4_PRODUCT_INFO_SIZE 2
#define SYNAPTICS_RMI4_PRODUCT_ID_SIZE 10
#define SYNAPTICS_RMI4_BUILD_ID_SIZE 3
#define SYNAPTICS_RMI4_SERIAL_SIZE 7
#define SYNAPTICS_RMI4_CONFIG_ID_SIZE 4
#define SYNAPTICS_RMI4_PACKAGE_ID_SIZE 4
#define SYNAPTICS_RMI4_FILENAME_SIZE 80

#define PACKAGE_ID_OFFSET 17
#define FW_VERSION_OFFSET 18
#define F34_PROPERTIES_OFFSET 1

#define MAX_NUMBER_OF_FINGERS 10
#define MAX_NUMBER_OF_BUTTONS 4
#define MAX_INTR_REGISTERS 4

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define MAX_NUMBER_TRACKED_RESUMES 10
/*
 * struct synaptics_rmi4_resume_info - information about a resume
 * @start: time when resume started
 * @finish: time when resume finished
 * @isr: time of the first isr after resume
 * @send_touch: time of the first send touch event after resume
 * @purge_off: time when events are no longer ignored after resume
 */
struct synaptics_rmi4_resume_info {
	struct timespec start;
	struct timespec finish;
	struct timespec isr;
	struct timespec send_touch;
	struct timespec purge_off;
	int    ignored_events;
};
#define MAX_NUMBER_TRACKED_IRQS 150
/*
 * struct synaptics_rmi4_irq_info - information about an interrupt
 * Using structure (instead of 1 variable) in case we want to add
 * more debugging information.
 * @irq_time: time when isr starts
 */
struct synaptics_rmi4_irq_info {
	struct timespec irq_time;
};
/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	unsigned char query_base_addr;
	unsigned char cmd_base_addr;
	unsigned char ctrl_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count;
	unsigned char fn_number;
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for data registers
 * @ctrl_base: 16-bit base address for command registers
 * @data_base: 16-bit base address for control registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

/*
 * struct synaptics_rmi4_fn - function handler data structure
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @size_of_data_register_block: data register block size
 * @data1_offset: offset to data1 register from data base address
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char size_of_data_register_block;
	unsigned char data1_offset;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: rmi protocol major version number
 * @version_minor: rmi protocol minor version number
 * @manufacturer_id: manufacturer id
 * @product_props: product properties information
 * @product_info: product info array
 * @date_code: device manufacture date
 * @tester_id: tester id array
 * @serial_number: device serial number
 * @product_id_string: device product id
 * @support_fn_list: linked list for function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	unsigned char serial[SYNAPTICS_RMI4_SERIAL_SIZE];
	unsigned char package_id[SYNAPTICS_RMI4_PACKAGE_ID_SIZE];
	unsigned char product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char build_id[SYNAPTICS_RMI4_BUILD_ID_SIZE];
	unsigned char config_id[SYNAPTICS_RMI4_CONFIG_ID_SIZE];
	struct list_head support_fn_list;
};

/*
 * struct synaptics_rmi4_data - rmi4 device instance data
 * @i2c_client: pointer to associated i2c client
 * @input_dev: pointer to associated input device
 * @board: constant pointer to platform data
 * @rmi4_mod_info: device information
 * @regulator: pointer to associated regulator
 * @rmi4_io_ctrl_mutex: mutex for i2c i/o control
 * @det_work: work thread instance for expansion function detection
 * @det_workqueue: pointer to work queue for work thread instance
 * @early_suspend: instance to support early suspend power management
 * @current_page: current page in sensor to acess
 * @button_0d_enabled: flag for 0d button support
 * @full_pm_cycle: flag for full power management cycle in early suspend stage
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f01
 * @f01_cmd_base_addr: command base address for f01
 * @f01_ctrl_base_addr: control base address for f01
 * @f01_data_base_addr: data base address for f01
 * @irq: attention interrupt
 * @sensor_max_x: sensor maximum x value
 * @sensor_max_y: sensor maximum y value
 * @irq_enabled: flag for indicating interrupt enable status
 * @touch_stopped: flag to stop interrupt thread processing
 * @fingers_on_2d: flag to indicate presence of fingers in 2d area
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @wait: wait queue for touch data polling in interrupt thread
 * @number_resumes: total number of remembered resumes
 * @last_resume: last resume's number (index of the location of resume)
 * @resume_info:  information about last few resumes
 * @number_irq: total number of remembered interrupt times
 * @last_irq: last interrup time's number (index of the location of interrupt)
 * @irq_info:  information about last few interrupt times
 * @i2c_read: pointer to i2c read function
 * @i2c_write: pointer to i2c write function
 * @irq_enable: pointer to irq enable function
 */
struct synaptics_rmi4_data {
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	const struct synaptics_dsx_platform_data *board;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct regulator *regulator;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex state_mutex;
#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
	struct mmi_notifier panel_nb;
#elif defined(CONFIG_FB)
	struct notifier_block panel_nb;
#endif
	atomic_t panel_off_flag;
	unsigned char current_page;
	unsigned char button_0d_enabled;
	unsigned char num_of_rx;
	unsigned char num_of_tx;
	unsigned char num_of_fingers;
	unsigned char f01_ctrl_register_0;
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned short num_of_intr_regs;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned int active_fn_intr_mask;
	int state;
	int irq;
	int sensor_max_x;
	int sensor_max_y;
	int normal_mode;
	bool irq_enabled;
	bool touch_stopped;
	bool fingers_on_2d;
	bool sensor_sleep;
	bool input_registered;
	bool in_bootloader;
	bool purge_enabled;
	bool reset_on_resume;
	bool hw_reset;
	bool one_touch_enabled;
	bool poweron;
	wait_queue_head_t wait;
	int (*i2c_read)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*i2c_write)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	void (*set_state)(struct synaptics_rmi4_data *rmi4_data, int state);
	int (*ready_state)(struct synaptics_rmi4_data *rmi4_data, bool standby);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char *f01_cmd_base_addr);
	int number_resumes;
	int last_resume;
	struct synaptics_rmi4_resume_info *resume_info;
	int number_irq;
	int last_irq;
	struct synaptics_rmi4_irq_info *irq_info;
};

struct f34_properties {
	union {
		struct {
			unsigned char regmap:1;
			unsigned char unlocked:1;
			unsigned char has_config_id:1;
			unsigned char has_perm_config:1;
			unsigned char has_bl_config:1;
			unsigned char has_display_config:1;
			unsigned char has_blob_config:1;
			unsigned char reserved:1;
		} __packed;
		unsigned char data[1];
	};
};

#define SYNAPTICS_DSX_STATES { \
	DSX(UNKNOWN), \
	DSX(ACTIVE), \
	DSX(STANDBY), \
	DSX(SUSPEND), \
	DSX(BL), \
	DSX(INIT), \
	DSX(FLASH), \
	DSX(INVALID) }

#define DSX(a)	STATE_##a
enum SYNAPTICS_DSX_STATES;
#undef DSX

enum ic_modes {
	IC_MODE_ANY = 0,
	IC_MODE_BL,
	IC_MODE_UI
};

enum exp_fn {
	RMI_DEV = 0,
	RMI_F34,
	RMI_F54,
	RMI_FW_UPDATER,
	RMI_LAST,
};

struct synaptics_rmi4_exp_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

void synaptics_rmi4_new_function(enum exp_fn fn_type, bool insert,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
				unsigned char intr_mask),
		enum ic_modes mode);

static inline ssize_t synaptics_rmi4_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_warn(dev, "%s Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_rmi4_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	dev_warn(dev, "%s Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}

static inline void batohui(unsigned int *dest, unsigned char *src, size_t size)
{
	int si = size;
	*dest = 0;
	for (; --si >= 0;)
		*dest += src[si] << (si << 3);
}

struct synaptics_rmi4_subpkt {
	unsigned char present;
	unsigned char expected;
	unsigned int size;
	void *data;
};

#define RMI4_SUBPKT(a)\
	{.present = 0, .expected = 1, .size = sizeof(a), .data = &a}
#define RMI4_SUBPKT_STATIC(a)\
	{.present = 1, .expected = 1, .size = sizeof(a), .data = &a}

struct synaptics_rmi4_packet_reg {
	int offset;
	int expected;
	unsigned int size;
	unsigned char nr_subpkts;
	struct synaptics_rmi4_subpkt *subpkt;
};

#define RMI4_NO_REG() {\
	.offset = -1, .expected = 0, .size = 0,\
	.nr_subpkts = 0, .subpkt = NULL}
#define RMI4_REG(r) {\
	.offset = -1, .expected = 1, .size = 0,\
	.nr_subpkts = ARRAY_SIZE(r), .subpkt = r}
#define RMI4_REG_STATIC(r, offs, sz) {\
	.offset = offs, .expected = 1, .size = sz,\
	.nr_subpkts = ARRAY_SIZE(r), .subpkt = r}

struct synaptics_rmi4_func_packet_regs {
	unsigned short base_addr;
	int nr_regs;
	struct synaptics_rmi4_packet_reg *regs;
};

int synaptics_rmi4_scan_packet_reg_info(
	struct synaptics_rmi4_data *rmi4_data,
	unsigned short query_addr,
	unsigned short regs_base_addr,
	struct synaptics_rmi4_func_packet_regs *regs);

int synaptics_rmi4_read_packet_reg(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs, unsigned char r);

int synaptics_rmi4_read_packet_regs(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs);
#endif
