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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _SYNAPTICS_DSX_RMI4_H_
#define _SYNAPTICS_DSX_RMI4_H_

#define SYNAPTICS_DSX_DRIVER_VERSION "DSX 1.1"

/* define to enable USB charger detection */
#undef USB_CHARGER_DETECTION

#include <linux/version.h>
#include <linux/ktime.h>
#include <linux/semaphore.h>
#if defined(USB_CHARGER_DETECTION)
#include <linux/usb.h>
#include <linux/power_supply.h>
#endif
#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
#include <linux/mmi_panel_notifier.h>
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
#define SYNAPTICS_RMI4_F35 (0x35)
#define SYNAPTICS_RMI4_F38 (0x38)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)
#define SYNAPTICS_RMI4_FDB (0xdb)

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
#define F34_PROPERTIES_OFFSET_V2 0

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

#define PRODUCT_INFO_SIZE 2
#define PRODUCT_ID_SIZE 10

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
	unsigned char intr_src_count:3;
	unsigned char reserved_1:2;
	unsigned char fn_version:2;
	unsigned char reserved_2:1;
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

struct touch_up_down {
	int mismatch;
	unsigned char up_down;
	unsigned int counter;
};

struct touch_area_stats {
	struct touch_up_down *ud;
	ssize_t ud_len;
	ssize_t ud_id;
	ssize_t unknown_counter;
	const char *name;
};

struct synaptics_dsx_func_patch {
	unsigned short func;
	unsigned char regstr;
	unsigned char subpkt;
	unsigned char size;
	unsigned char bitmask;
	unsigned char *data;
	struct list_head link;
};

struct synaptics_clip_area {
	unsigned xul_clip, yul_clip, xbr_clip, ybr_clip;
	unsigned inversion; /* clip inside (when 1) or ouside otherwise */
};

enum {
	SYNA_MOD_AOD,
	SYNA_MOD_STATS,
	SYNA_MOD_FOLIO,
	SYNA_MOD_CHARGER,
	SYNA_MOD_WAKEUP,
	SYNA_MOD_FPS,
	SYNA_MOD_QUERY,	/* run time query; active only */
	SYNA_MOD_RT,	/* run time patch; active only; always effective */
	SYNA_MOD_MAX
};

#define FLAG_FORCE_UPDATE	1
#define FLAG_WAKEABLE		2
#define FLAG_POWER_SLEEP	4

struct synaptics_dsx_patch {
	const char *name;
	int	cfg_num;
	unsigned int flags;
	struct semaphore list_sema;
	struct list_head cfg_head;
};

struct config_modifier {
	const char *name;
	int id;
	bool effective;
	struct synaptics_dsx_patch *active;
	struct synaptics_dsx_patch *suspended;
	struct synaptics_clip_area *clipa;
	struct list_head link;
};

struct synaptics_dsx_modifiers {
	int	mods_num;
	struct semaphore list_sema;
	struct list_head mod_head;

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

struct f34_properties_v2 {
	union {
		struct {
			unsigned char query0_subpkt1_size:3;
			unsigned char has_config_id:1;
			unsigned char reserved:1;
			unsigned char has_thqa:1;
			unsigned char reserved2:2;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_control_95n {
	union {
		struct {
			/* byte 0 - flags*/
			unsigned char c95_filter_bw:3;
			unsigned char c95_byte0_b3_b6:4;
			unsigned char c95_disable:1;

			/* bytes 1 - 10 */
			unsigned char c95_first_burst_length_lsb;
			unsigned char c95_first_burst_length_msb;
			unsigned char c95_addl_burst_length_lsb;
			unsigned char c95_addl_burst_length_msb;
			unsigned char c95_i_stretch;
			unsigned char c95_r_stretch;
			unsigned char c95_noise_control1;
			unsigned char c95_noise_control2;
			unsigned char c95_noise_control3;
			unsigned char c95_noise_control4;
		} __packed;
		struct {
			unsigned char data[11];
		} __packed;
	};
};

enum {
	STATE_UNKNOWN,
	STATE_ACTIVE,
	STATE_SUSPEND,
	STATE_STANDBY = 4,
	STATE_BL,
	STATE_INIT,
	STATE_FLASH,
	STATE_QUERY,
	STATE_INVALID
};

#define STATE_UI       (STATE_ACTIVE | STATE_SUSPEND)

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
	RMI_CTRL_ACCESS_BLK,
	RMI_LAST,
};

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
	bool present;
	bool expected;
	short offset;
	unsigned int size;
	void *data;
};

#define RMI4_SUBPKT(a)\
	{.present = 0, .expected = 1, .size = sizeof(a), .data = &a}
#define RMI4_SUBPKT_STATIC(o, a)\
	{.present = 1, .expected = 1,\
			.offset = o, .size = sizeof(a), .data = &a}

struct synaptics_rmi4_packet_reg {
	unsigned short r_number;
	bool updated;	/* indicate that value in *data has been updated */
	bool modified;	/* indicate that value in *data has been modified */
	bool expected;
	short offset;
	unsigned int size;
	unsigned char *data;
	unsigned char nr_subpkts;
	struct synaptics_rmi4_subpkt *subpkt;
};

#define RMI4_NO_REG(r) {\
	.r_number = r, .offset = -1, .updated = 0, .modified = 0,\
	.expected = 0, .size = 0, .data = NULL,\
	.nr_subpkts = 0, .subpkt = NULL}
#define RMI4_REG(r, s) {\
	.r_number = r, .offset = -1, .updated = 0, .modified = 0,\
	.expected = 1, .size = 0, .data = NULL,\
	.nr_subpkts = ARRAY_SIZE(s), .subpkt = s}
#define RMI4_REG_STATIC(r, s, sz) {\
	.r_number = r, .offset = r, .updated = 0, .modified = 0,\
	.expected = 1, .size = sz, .data = NULL,\
	.nr_subpkts = ARRAY_SIZE(s), .subpkt = s}

struct synaptics_rmi4_func_packet_regs {
	unsigned short f_number;
	unsigned short base_addr;
	unsigned short query_offset;
	int nr_regs;
	struct synaptics_rmi4_packet_reg *regs;
};

/*
 * struct synaptics_rmi4_data - rmi4 device instance data
 * @i2c_client: pointer to associated i2c client
 * @input_dev: pointer to associated input device
 * @board: constant pointer to platform data
 * @rmi4_mod_info: device information
 * @regulator: pointer to associated regulator
 * @vdd_quirk: pointer to associated regulator for 'quirk' config
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
 * @touch_stopped: touch is in suspend state
 * @flash_enabled: allow flashing once transition to active state is complete
 * @ic_on: touch ic power state
 * @fingers_on_2d: flag to indicate presence of fingers in 2d area
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
	struct regulator *vdd_quirk;
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
	unsigned char aod_mt;
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
	bool irq_enabled;
	atomic_t touch_stopped;
	bool flash_enabled;
	bool ic_on;
	bool fingers_on_2d;
	bool input_registered;
	bool in_bootloader;
	wait_queue_head_t wait;
	int (*i2c_read)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*i2c_write)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	void (*set_state)(struct synaptics_rmi4_data *rmi4_data, int state);
	int (*ready_state)(struct synaptics_rmi4_data *rmi4_data, bool standby);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data);
	int number_resumes;
	int last_resume;
	struct synaptics_rmi4_resume_info *resume_info;
	int number_irq;
	int last_irq;
	struct synaptics_rmi4_irq_info *irq_info;

	/* features enablers */
	bool charger_detection_enabled;
	bool folio_detection_enabled;
	bool fps_detection_enabled;
	bool wakeup_detection_enabled;
	bool patching_enabled;
	bool purge_enabled;

	bool suspend_is_wakeable;
	bool clipping_on;
	struct synaptics_clip_area *clipa;
	struct synaptics_dsx_modifiers modifiers;

	struct work_struct resume_work;
	struct synaptics_rmi4_func_packet_regs *f12_data_registers_ptr;
	struct notifier_block rmi_reboot;
#if defined(USB_CHARGER_DETECTION)
	struct power_supply psy;
#endif
#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
	struct notifier_block folio_notif;
#endif
	bool is_fps_registered;	/* FPS notif registration might be delayed */
	struct notifier_block fps_notif;

	struct mutex rmi4_exp_init_mutex;
};

struct time_keeping {
	int id;
	unsigned long long duration;
} __packed;

struct statistics {
	int		active;	/* current index within keeper array */
	int		max;	/* keeper array dimension */
	bool		clk_run;/* flag to indicate whether clock is ticking */
	unsigned int	flags;	/* time keeping flags */
	unsigned char	abbr;	/* abbreviation letter */
	ktime_t		start;	/* time notch */
	struct time_keeping keeper[0];
} __packed;

enum {
	FREQ_DURATION,		/* scanning frequencies duration */
	FREQ_HOPPING,		/* scanning frequencies hopping */
	MODE_NMS,		/* noise mitigation mode */
	MODE_METAL_PLATE,	/* metal plate detection */
	STATISTICS_MAX
};

struct synaptics_dsx_stats {
	bool enabled;
	ktime_t	uptime;
	bool uptime_run;
	unsigned long long uptime_dur;
	struct statistics *stats[STATISTICS_MAX];
#define dur stats[FREQ_DURATION]
#define hop stats[FREQ_HOPPING]
#define nms stats[MODE_NMS]
#define mpd stats[MODE_METAL_PLATE]
} __packed;

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

int synaptics_rmi4_scan_packet_reg_info(
	struct synaptics_rmi4_data *rmi4_data,
	unsigned short query_addr,
	unsigned short regs_base_addr,
	struct synaptics_rmi4_func_packet_regs *regs);

int synaptics_rmi4_read_packet_reg(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs, int idx);

int synaptics_rmi4_read_packet_regs(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs);

static inline int secure_memcpy(unsigned char *dest, unsigned int dest_size,
		const unsigned char *src, unsigned int src_size,
		unsigned int count)
{
	if (dest == NULL || src == NULL)
		return -EINVAL;

	if (count > dest_size || count > src_size)
		return -EINVAL;

	memcpy((void *)dest, (const void *)src, count);

	return 0;
}

static inline int synaptics_rmi4_reg_read(
		struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr,
		unsigned char *data,
		unsigned short len)
{
	return rmi4_data->i2c_read(rmi4_data, addr, data, len);
}

static inline int synaptics_rmi4_reg_write(
		struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr,
		unsigned char *data,
		unsigned short len)
{
	return rmi4_data->i2c_write(rmi4_data, addr, data, len);
}

extern int FPS_register_notifier(struct notifier_block *nb,
				unsigned long stype, bool report);
extern int FPS_unregister_notifier(struct notifier_block *nb,
				unsigned long stype);

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_TEST_REPORTING)
int synaptics_rmi4_scan_f54_ctrl_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs);

int synaptics_rmi4_scan_f54_cmd_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs);

int synaptics_rmi4_scan_f54_data_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs);

int synaptics_rmi4_scan_f54_query_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs);
#else
static inline int synaptics_rmi4_scan_f54_ctrl_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs) {
	return -ENOSYS;
}

static inline int synaptics_rmi4_scan_f54_cmd_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs) {
	return -ENOSYS;
}

static inline int synaptics_rmi4_scan_f54_data_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs) {
	return -ENOSYS;
}

static inline int synaptics_rmi4_scan_f54_query_reg_info(
	struct synaptics_rmi4_func_packet_regs *regs) {
	return -ENOSYS;
}
#endif
#endif
