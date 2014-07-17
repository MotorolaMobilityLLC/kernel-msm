/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Copyright (C) 2011-2012 Atmel Corporation
 * Copyright (C) 2012 Google, Inc.
 *
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#define pr_fmt(fmt) "atmel_mxt_ts_mmi: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c/atmel_mxt_ts_mmi.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
#include "touchx.h"
#endif

#define ATMEL_MXT_STATES { \
	MXT_DEF(UNKNOWN), \
	MXT_DEF(ACTIVE), \
	MXT_DEF(STANDBY), \
	MXT_DEF(SUSPEND), \
	MXT_DEF(BL), \
	MXT_DEF(INIT), \
	MXT_DEF(FLASH), \
	MXT_DEF(QUERY), \
	MXT_DEF(INVALID) }

#define MXT_DEF(a)	STATE_##a
enum ATMEL_MXT_STATES;
#undef MXT_DEF

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>

static int fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data);
#endif

#define DRIVER_NAME "atmel_mxt_ts"
#define MXT_MAX_RETRIES		10
#define MXT_MAX_BLOCK_WRITE	256
#define MXT_MAX_BUTTONS		8
#define DEBUG_MSG_MAX		200

/* Configuration file */
#define MXT_CFG_MAGIC		"OBP_RAW V1"

/* Registers */
#define MXT_OBJECT_START	0x07
#define MXT_OBJECT_SIZE		6
#define MXT_INFO_CHECKSUM_SIZE	3
#define MXT_T37_REVISION	21

/* Object types */
#define MXT_DEBUG_DIAGNOSTIC_T37	37
#define MXT_GEN_MESSAGE_T5		5
#define MXT_GEN_COMMAND_T6		6
#define MXT_GEN_POWER_T7		7
#define MXT_GEN_ACQUIRE_T8		8
#define MXT_GEN_DATASOURCE_T53		53
#define MXT_TOUCH_MULTI_T9		9
#define MXT_TOUCH_KEYARRAY_T15		15
#define MXT_TOUCH_PROXIMITY_T23		23
#define MXT_TOUCH_PROXKEY_T52		52
#define MXT_PROCI_GRIPFACE_T20		20
#define MXT_PROCG_NOISE_T22		22
#define MXT_PROCI_ONETOUCH_T24		24
#define MXT_PROCI_TWOTOUCH_T27		27
#define MXT_PROCI_GRIP_T40		40
#define MXT_PROCI_PALM_T41		41
#define MXT_PROCI_TOUCHSUPPRESSION_T42	42
#define MXT_PROCI_STYLUS_T47		47
#define MXT_PROCG_NOISESUPPRESSION_T48	48
#define MXT_SPT_COMMSCONFIG_T18		18
#define MXT_SPT_GPIOPWM_T19		19
#define MXT_SPT_SELFTEST_T25		25
#define MXT_SPT_CTECONFIG_T28		28
#define MXT_SPT_USERDATA_T38		38
#define MXT_SPT_DIGITIZER_T43		43
#define MXT_SPT_MESSAGECOUNT_T44	44
#define MXT_SPT_CTECONFIG_T46		46
#define MXT_PROCI_ACTIVE_STYLUS_T63	63
#define MXT_PROCI_LENSEBENDING_T65	65
#define MXT_PROCG_NOISESUPPRESSION_T72	72
#define MXT_TOUCH_MULTITOUCHSCREEN_T100 100

/* MXT_GEN_MESSAGE_T5 object */
#define MXT_RPTID_NOMSG		0xff

/* MXT_GEN_COMMAND_T6 field */
#define MXT_COMMAND_RESET	0
#define MXT_COMMAND_BACKUPNV	1
#define MXT_COMMAND_CALIBRATE	2
#define MXT_COMMAND_REPORTALL	3
#define MXT_COMMAND_DIAGNOSTIC	5

/* Define for T6 status byte */
#define MXT_T6_STATUS_RESET	(1 << 7)
#define MXT_T6_STATUS_OFL	(1 << 6)
#define MXT_T6_STATUS_SIGERR	(1 << 5)
#define MXT_T6_STATUS_CAL	(1 << 4)
#define MXT_T6_STATUS_CFGERR	(1 << 3)
#define MXT_T6_STATUS_COMSERR	(1 << 2)

/* MXT_GEN_POWER_T7 field */
struct t7_config {
	u8 idle;
	u8 active;
} __packed;

#define MXT_POWER_CFG_RUN		0
#define MXT_POWER_CFG_DEEPSLEEP		1

/* MXT_TOUCH_MULTI_T9 field */
#define MXT_T9_ORIENT		9
#define MXT_T9_RANGE		18

/* MXT_TOUCH_MULTI_T9 status */
#define MXT_T9_UNGRIP		(1 << 0)
#define MXT_T9_SUPPRESS		(1 << 1)
#define MXT_T9_AMP		(1 << 2)
#define MXT_T9_VECTOR		(1 << 3)
#define MXT_T9_MOVE		(1 << 4)
#define MXT_T9_RELEASE		(1 << 5)
#define MXT_T9_PRESS		(1 << 6)
#define MXT_T9_DETECT		(1 << 7)

struct t9_range {
	u16 x;
	u16 y;
} __packed;

/* MXT_TOUCH_MULTI_T9 orient */
#define MXT_T9_ORIENT_SWITCH	(1 << 0)

/* MXT_SPT_COMMSCONFIG_T18 */
#define MXT_COMMS_CTRL		0
#define MXT_COMMS_CMD		1
#define MXT_COMMS_RETRIGEN      (1 << 6)

/* Define for MXT_GEN_COMMAND_T6 */
#define MXT_BOOT_VALUE		0xa5
#define MXT_RESET_VALUE		0x01
#define MXT_BACKUP_VALUE	0x55
#define MXT_IDENT_VALUE		0x80

/* Define for MXT_PROCI_TOUCHSUPPRESSION_T42 */
#define MXT_T42_MSG_TCHSUP	(1 << 0)

/* T47 Stylus */
#define MXT_TOUCH_MAJOR_T47_STYLUS	1

/* T63 Stylus */
#define MXT_T63_STYLUS_PRESS	(1 << 0)
#define MXT_T63_STYLUS_RELEASE	(1 << 1)
#define MXT_T63_STYLUS_MOVE		(1 << 2)
#define MXT_T63_STYLUS_SUPPRESS	(1 << 3)

#define MXT_T63_STYLUS_DETECT	(1 << 4)
#define MXT_T63_STYLUS_TIP		(1 << 5)
#define MXT_T63_STYLUS_ERASER	(1 << 6)
#define MXT_T63_STYLUS_BARREL	(1 << 7)

#define MXT_T63_STYLUS_PRESSURE_MASK	0x3F

/* T100 Multiple Touch Touchscreen */
#define MXT_T100_CTRL		0
#define MXT_T100_CFG1		1
#define MXT_T100_TCHAUX		3
#define MXT_T100_NUMTCH		6
#define MXT_T100_XRANGE		13
#define MXT_T100_YRANGE		24
#define MXT_T100_MOVHYSTI	47
#define MXT_T100_MOVHYSTN	49

#define MXT_T100_CFG_SWITCHXY	(1 << 5)

#define MXT_T100_TCHAUX_VECT	(1 << 0)
#define MXT_T100_TCHAUX_AMPL	(1 << 1)
#define MXT_T100_TCHAUX_AREA	(1 << 2)

#define MXT_T100_DETECT		(1 << 7)
#define MXT_T100_TYPE_MASK	0x70
#define MXT_T100_TYPE_STYLUS	0x20

/* Delay times */
#define MXT_BACKUP_TIME		50	/* msec */
#define MXT_RESET_TIME		200	/* msec */
#define MXT_RESET_TIMEOUT	3000	/* msec */
#define MXT_CRC_TIMEOUT		2000	/* msec */
#define MXT_FW_RESET_TIME	3500	/* msec */
#define MXT_FW_CHG_TIMEOUT	300	/* msec */
#define MXT_WAKEUP_TIME		25	/* msec */
#define MXT_REGULATOR_DELAY	150	/* msec */
#define MXT_POWERON_DELAY	150	/* msec */

/* Command to unlock bootloader */
#define MXT_UNLOCK_CMD_MSB	0xaa
#define MXT_UNLOCK_CMD_LSB	0xdc

/* Bootloader mode status */
#define MXT_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT_FRAME_CRC_CHECK	0x02
#define MXT_FRAME_CRC_FAIL	0x03
#define MXT_FRAME_CRC_PASS	0x04
#define MXT_APP_CRC_FAIL	0x40	/* valid 7 8 bit only */
#define MXT_BOOT_STATUS_MASK	0x3f
#define MXT_BOOT_EXTENDED_ID	(1 << 5)
#define MXT_BOOT_ID_MASK	0x1f

/* Touchscreen absolute values */
#define MXT_MAX_AREA		50
#define MXT_MAX_PRESSURE	26

#define MXT_PIXELS_PER_MM	20

struct mxt_object {
	u8 type;
	u16 start_address;
	u8 size_minus_one;
	u8 instances_minus_one;
	u8 num_report_ids;
} __packed;

struct mxt_tdat_section {
	const unsigned char *data;
	size_t size;
};

/* Each client has this additional data */
struct mxt_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	char phys[64];		/* device physical location */
	struct mxt_platform_data *pdata;
	struct mxt_object *object_table;
	struct mxt_info *info;
	void *raw_info_block;
	atomic_t state;
	unsigned int irq;
	unsigned int max_x;
	unsigned int max_y;
	bool in_bootloader;
	u16 mem_size;
	u32 config_id;
	u32 firmware_id;
	u8 t100_aux_ampl;
	u8 t100_aux_area;
	u8 t100_aux_vect;
	bool mem_access_created;
	struct bin_attribute mem_access_attr;
	bool debug_enabled;
	bool debug_v2_enabled;
	u8 *debug_msg_data;
	u16 debug_msg_count;
	struct bin_attribute debug_msg_attr;
	struct mutex debug_msg_lock;
	u8 revision_id;
	u8 max_reportid;
	u32 config_crc;
	u32 info_crc;
	u8 bootloader_addr;
	struct t7_config t7_cfg;
	u8 *msg_buf;
	u8 t6_status;
	bool update_input;
	u8 last_message_count;
	u8 num_touchids;
	u8 num_stylusids;
	unsigned long t15_keystatus;
	bool use_retrigen_workaround;
	bool use_regulator;
	bool poweron;
	bool input_registered;
	bool buttons_enabled;
	bool one_touch_enabled;
	bool sensor_sleep;
	bool irq_enabled;
	struct regulator *reg_vdd;
	struct regulator *reg_avdd;
	char *fw_name;
	char *cfg_name;
	u8 *T100_data;

	/* Cached parameters from object table */
	u16 T5_address;
	u8 T5_msg_size;
	u8 T6_reportid;
	u16 T6_address;
	u16 T7_address;
	u16 T38_address;
	u8 T9_reportid_min;
	u8 T9_reportid_max;
	u8 T15_reportid_min;
	u8 T15_reportid_max;
	u16 T18_address;
	u8 T19_reportid;
	u8 T42_reportid_min;
	u8 T42_reportid_max;
	u16 T44_address;
	u8 T48_reportid;
	u8 T63_reportid_min;
	u8 T63_reportid_max;
	u8 T100_reportid_min;
	u8 T100_reportid_max;

	/* for fw update in bootloader */
	struct completion bl_completion;

	/* for reset handling */
	struct completion reset_completion;

	/* for reset handling */
	struct completion crc_completion;

	/* Enable reporting of input events */
	bool enable_reporting;

	/* Indicates whether device is in suspend */
	bool suspended;

#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	struct mutex crit_section_lock;
	const struct firmware *tdat;
	struct mxt_tdat_section fw;
	struct mxt_tdat_section tsett;
};

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
struct touchxs touchxp;
EXPORT_SYMBOL(touchxp);
#endif

static int mxt_resume(struct device *dev);
static int mxt_get_sensor_state(struct mxt_data *data);
static int mxt_init_t7_power_cfg(struct mxt_data *data);
static void mxt_regulator_disable(struct mxt_data *data);
static void mxt_regulator_enable(struct mxt_data *data);
static void mxt_reset_slots(struct mxt_data *data);

struct debug_section {
	unsigned long j;
	unsigned int count;
};

static struct debug_section mxt_proc_t100_dbg;
static struct debug_section mxt_dump_message_dbg;

/* Function: throttle_dbgout
 *  Print no more than n messages in msec interval
 *  Return
 *  - false if printing is allowed
 *  - true if printing is NOT allowed
 */
bool throttle_dbgout(struct debug_section *dbg, int n, unsigned int msec)
{
	if (printk_timed_ratelimit(&dbg->j, msec))
		dbg->count = 0;

	if (dbg->count < n) {
		++dbg->count;
		return false;
	} else
		return true;
}

#define TOUCH_QUERY_STRING_SIZE 512
static char touch_setup_string[TOUCH_QUERY_STRING_SIZE];

struct obj_patch {
	u8 number;
	u8 offset;
	u8 value;
	struct list_head link;
};

static struct {
	int	cfg_num;
	struct list_head cfg_head;
}	mxt_cfg_data;

static __init int mxt_setup_query(char *str)
{
	if (strlen(str) >= TOUCH_QUERY_STRING_SIZE) {
		pr_warn("Atmel touch T100 boot param string too large\n");
		return 0;
	}
	strlcpy(touch_setup_string, str, TOUCH_QUERY_STRING_SIZE);
	INIT_LIST_HEAD(&mxt_cfg_data.cfg_head);
	return 1;
}

__setup("atmxt=", mxt_setup_query);

static char *mxt_find_patch(char *head, char *delimiters, char **next)
{
	char *patch = head;

	for (; patch; patch = *next) {
		*next = strpbrk(patch, delimiters);
		if (*next)
			*(*next)++ = '\0';

		patch = skip_spaces(patch);
		if (!patch || !*patch || *patch == '#')
			continue;
		else
			break;
	}

	return patch;
}

static int mxt_parse_patch(int object, char *query)
{
	int i, error;
	char *next, *value_p, *pair = query;
	long offset_v, value_v;
	struct obj_patch *patch;

	for (i = 0; pair; pair = next, i++) {
		pair = mxt_find_patch(pair, ",", &next);

		value_p = strpbrk(pair, "=");
		if (!value_p) {
			pr_err("T%d[%d]: invalid syntax '%s'\n",
				object, i, pair);
			continue;
		}

		/* make sure string is null terminated */
		*value_p = '\0';

		error = kstrtol(pair, 10, &offset_v);
		if (error) {
			pr_err("T%d[%d]: dec conversion error\n", object, i);
			continue;
		}

		error = kstrtol(++value_p, 16, &value_v);
		if (error) {
			pr_err("T%d[%d]: hex conversion error\n", object, i);
			continue;
		}

		/* primitive data validation */
		if (object <= 0 || object > 255 ||
		    offset_v < 0 || offset_v > 255) {
			pr_err("T%d[%d]: invalid values\n", object, i);
			continue;
		}

		patch = kzalloc(sizeof(*patch), GFP_KERNEL);
		if (!patch) {
			pr_err("failed to alloc mem\n");
			return -ENOMEM;
		}

		patch->number = (u8)object;
		patch->offset = (u8)offset_v;
		patch->value = (u8)value_v;

		pr_debug("T%d[%d], offset %d, value 0x%02x\n",
			patch->number, i, patch->offset, patch->value);
		mxt_cfg_data.cfg_num++;
		list_add_tail(&patch->link, &mxt_cfg_data.cfg_head);
	}

	return 0;
}

/*
 * Special settings can be passed to the driver via kernel cmd line
 * Example: atmxt="T100@0=1f,1=a0;T72@47=b2;"
 *   Where: T100 - decimal object number
 *          @    - delimits object number and following patch sets
 *	    0=1f - patch set decimal offset and hex value
 */
static void mxt_parse_setup_string(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	long number_v;
	char *config_p, *next, *patch_set = touch_setup_string;
	int i, error;

	for (i = 0; patch_set; patch_set = next) {
		patch_set = mxt_find_patch(patch_set, ";\n", &next);
		if (!patch_set)
			break;

		dev_dbg(dev, "patch set %d: \"%s\"\n", i, patch_set);

		config_p = strpbrk(patch_set, "@");

		if ((*patch_set != 'T' && *patch_set != 't') || !config_p) {
			dev_err(dev, "invalid syntax '%s'\n", patch_set);
			continue;
		}

		/* strip non digits */
		*config_p++ = '\0';

		error = kstrtol(++patch_set, 10, &number_v);
		if (error) {
			dev_err(dev, "kstrtol error %d\n", error);
			continue;
		}

		error = mxt_parse_patch((int)number_v, config_p);
		if (error < 0) {
			dev_err(dev, "invalid patch; parse error %d\n", error);
			continue;
		}

		i++;
	}

	if (mxt_cfg_data.cfg_num)
		dev_info(dev, "processed %d patch sets for %d objects\n",
			mxt_cfg_data.cfg_num, i);
	else
		dev_info(dev, "no valid patch sets found\n");
}

static inline size_t mxt_obj_size(const struct mxt_object *obj)
{
	return obj->size_minus_one + 1;
}

static inline size_t mxt_obj_instances(const struct mxt_object *obj)
{
	return obj->instances_minus_one + 1;
}

static bool mxt_object_readable(unsigned int type)
{
	switch (type) {
	case MXT_GEN_POWER_T7:
	case MXT_GEN_ACQUIRE_T8:
	case MXT_SPT_SELFTEST_T25:
	case MXT_SPT_USERDATA_T38:
	case MXT_SPT_CTECONFIG_T46:
	case MXT_PROCI_LENSEBENDING_T65:
	case MXT_PROCG_NOISESUPPRESSION_T72:
	case MXT_TOUCH_MULTITOUCHSCREEN_T100:
		return true;
	case MXT_GEN_DATASOURCE_T53:
	case MXT_TOUCH_MULTI_T9:
	case MXT_TOUCH_KEYARRAY_T15:
	case MXT_TOUCH_PROXIMITY_T23:
	case MXT_TOUCH_PROXKEY_T52:
	case MXT_PROCI_GRIPFACE_T20:
	case MXT_PROCG_NOISE_T22:
	case MXT_PROCI_ONETOUCH_T24:
	case MXT_PROCI_TWOTOUCH_T27:
	case MXT_PROCI_GRIP_T40:
	case MXT_PROCI_PALM_T41:
	case MXT_PROCI_TOUCHSUPPRESSION_T42:
	case MXT_PROCI_STYLUS_T47:
	case MXT_PROCG_NOISESUPPRESSION_T48:
	case MXT_SPT_COMMSCONFIG_T18:
	case MXT_SPT_GPIOPWM_T19:
	case MXT_SPT_CTECONFIG_T28:
	case MXT_SPT_DIGITIZER_T43:
	default:
		return false;
	}
}

static void mxt_dump_message(struct mxt_data *data, u8 *message)
{
	if (!throttle_dbgout(&mxt_dump_message_dbg, 20, 300000))
		print_hex_dump(KERN_DEBUG, "atmel_mxt_ts_mmi: MXT MSG:",
			DUMP_PREFIX_NONE, 16, 1,
			message, data->T5_msg_size, false);
}

static void mxt_debug_msg_enable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (data->debug_v2_enabled)
		return;

	mutex_lock(&data->debug_msg_lock);

	data->debug_msg_data = kcalloc(DEBUG_MSG_MAX,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->debug_msg_data) {
		mutex_unlock(&data->debug_msg_lock);
		dev_err(&data->client->dev, "Failed to allocate buffer\n");
		return;
	}

	data->debug_v2_enabled = true;
	mutex_unlock(&data->debug_msg_lock);

	dev_info(dev, "Enabled message output\n");
}

static void mxt_debug_msg_disable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;

	if (!data->debug_v2_enabled)
		return;

	dev_info(dev, "disabling message output\n");
	data->debug_v2_enabled = false;

	mutex_lock(&data->debug_msg_lock);
	kfree(data->debug_msg_data);
	data->debug_msg_data = NULL;
	data->debug_msg_count = 0;
	mutex_unlock(&data->debug_msg_lock);
	dev_info(dev, "Disabled message output\n");
}

static void mxt_debug_msg_add(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;

	mutex_lock(&data->debug_msg_lock);

	if (!data->debug_msg_data) {
		mutex_unlock(&data->debug_msg_lock);
		dev_err(dev, "No buffer!\n");
		return;
	}

	if (data->debug_msg_count < DEBUG_MSG_MAX) {
		memcpy(data->debug_msg_data +
		       data->debug_msg_count * data->T5_msg_size,
		       msg, data->T5_msg_size);
		data->debug_msg_count++;
	} else {
		dev_dbg(dev, "Discarding %u messages\n", data->debug_msg_count);
		data->debug_msg_count = 0;
	}

	mutex_unlock(&data->debug_msg_lock);

	sysfs_notify(&data->client->dev.kobj, NULL, "debug_notify");
}

static ssize_t mxt_debug_msg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	return -EIO;
}

static ssize_t mxt_debug_msg_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t bytes)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	size_t count, bytes_read;

	if (!data->debug_msg_data) {
		dev_err(dev, "No buffer!\n");
		return 0;
	}

	count = bytes / data->T5_msg_size;

	if (count > DEBUG_MSG_MAX)
		count = DEBUG_MSG_MAX;

	mutex_lock(&data->debug_msg_lock);

	if (count > data->debug_msg_count)
		count = data->debug_msg_count;

	bytes_read = count * data->T5_msg_size;

	memcpy(buf, data->debug_msg_data, bytes_read);
	data->debug_msg_count = 0;

	mutex_unlock(&data->debug_msg_lock);

	return bytes_read;
}

static int mxt_debug_msg_init(struct mxt_data *data)
{
	sysfs_bin_attr_init(&data->debug_msg_attr);
	data->debug_msg_attr.attr.name = "debug_msg";
	data->debug_msg_attr.attr.mode = S_IRUGO | S_IWUSR | S_IWGRP;
	data->debug_msg_attr.read = mxt_debug_msg_read;
	data->debug_msg_attr.write = mxt_debug_msg_write;
	data->debug_msg_attr.size = data->T5_msg_size * DEBUG_MSG_MAX;

	if (sysfs_create_bin_file(&data->client->dev.kobj,
				  &data->debug_msg_attr) < 0) {
		dev_err(&data->client->dev, "Failed to create %s\n",
			data->debug_msg_attr.attr.name);
		return -EINVAL;
	}

	return 0;
}

static void mxt_debug_msg_remove(struct mxt_data *data)
{
	if (data->debug_msg_attr.attr.name) {
		sysfs_remove_bin_file(&data->client->dev.kobj,
				      &data->debug_msg_attr);
		data->debug_msg_attr.attr.name = NULL;
	}
}

static int mxt_wait_for_completion(struct mxt_data *data,
			struct completion *comp, unsigned int timeout_ms)
{
	struct device *dev = &data->client->dev;
	unsigned long timeout = msecs_to_jiffies(timeout_ms);
	long ret;

	ret = wait_for_completion_interruptible_timeout(comp, timeout);
	if (ret < 0) {
		dev_err(dev, "Wait for completion interrupted.\n");
		return -EINTR;
	} else if (ret == 0) {
		dev_err(dev, "Wait for completion timed out.\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static int mxt_bootloader_read(struct mxt_data *data,
			       u8 *val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);

	if (ret == 1) {
		ret = 0;
	} else {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&data->client->dev, "%s: i2c recv failed (%d)\n",
			__func__, ret);
	}

	return ret;
}

static int mxt_bootloader_write(struct mxt_data *data,
				const u8 * const val, unsigned int count)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = data->bootloader_addr;
	msg.flags = data->client->flags & I2C_M_TEN;
	msg.len = count;
	msg.buf = (u8 *)val;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret == 1) {
		ret = 0;
	} else {
		ret = (ret < 0) ? ret : -EIO;
		dev_err(&data->client->dev, "%s: i2c send failed (%d)\n",
			__func__, ret);
	}

	return ret;
}

static int mxt_lookup_bootloader_address(struct mxt_data *data, bool retry)
{
	u8 appmode = data->client->addr;
	u8 bootloader;
	u8 family_id = 0;

	if (data->info)
		family_id = data->info->family_id;
	else
		family_id = data->pdata->dt_info.family_id;

	switch (appmode) {
	case 0x4a:
	case 0x4b:
		/* Chips after 1664S use different scheme */
		if (retry || family_id >= 0xa2) {
			bootloader = appmode - 0x24;
			break;
		}
		/* Fall through for normal case */
	case 0x4c:
	case 0x4d:
	case 0x5a:
	case 0x5b:
		bootloader = appmode - 0x26;
		break;
	default:
		dev_err(&data->client->dev,
			"Appmode i2c address 0x%02x not found\n",
			appmode);
		return -EINVAL;
	}

	data->bootloader_addr = bootloader;
	dev_dbg(&data->client->dev, "Bootloader address set to %x\n",
		data->bootloader_addr);

	return 0;
}

static int mxt_probe_bootloader(struct mxt_data *data, bool retry)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 val;
	bool crc_failure;

	ret = mxt_lookup_bootloader_address(data, retry);
	if (ret)
		return ret;

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	/* Check app crc fail mode */
	crc_failure = (val & ~MXT_BOOT_STATUS_MASK) == MXT_APP_CRC_FAIL;

	dev_err(dev, "Detected bootloader, status:%02X%s\n",
			val, crc_failure ? ", APP_CRC_FAIL" : "");

	return 0;
}

static u8 mxt_get_bootloader_version(struct mxt_data *data, u8 val)
{
	struct device *dev = &data->client->dev;
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (mxt_bootloader_read(data, &buf[0], 3) != 0) {
			dev_err(dev, "%s: i2c failure\n", __func__);
			return -EIO;
		}

		dev_info(dev, "Bootloader ID:%d Version:%d\n", buf[1], buf[2]);

		return buf[0];
	} else {
		dev_info(dev, "Bootloader ID:%d\n", val & MXT_BOOT_ID_MASK);

		return val;
	}
}

static int mxt_check_bootloader(struct mxt_data *data, unsigned int state,
				bool wait)
{
	struct device *dev = &data->client->dev;
	u8 val;
	int ret;

recheck:
	if (wait) {
		/*
		 * In application update mode, the interrupt
		 * line signals state transitions. We must wait for the
		 * CHG assertion before reading the status byte.
		 * Once the status byte has been read, the line is deasserted.
		 */
		ret = mxt_wait_for_completion(data, &data->bl_completion,
					      MXT_FW_CHG_TIMEOUT);
		if (ret) {
			/*
			 * TODO: handle -EINTR better by terminating fw update
			 * process before returning to userspace by writing
			 * length 0x000 to device (if we are in
			 * WAITING_FRAME_DATA state).
			 */
			dev_err(dev, "Update wait error %d\n", ret);
			return ret;
		}
	}

	ret = mxt_bootloader_read(data, &val, 1);
	if (ret)
		return ret;

	if (state == MXT_WAITING_BOOTLOAD_CMD)
		val = mxt_get_bootloader_version(data, val);

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK) {
			goto recheck;
		} else if (val == MXT_FRAME_CRC_FAIL) {
			dev_err(dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(dev, "Invalid bootloader state %02X != %02X\n",
			val, state);
		return -EINVAL;
	}

	return 0;
}

static int mxt_send_bootloader_cmd(struct mxt_data *data, bool unlock)
{
	int ret;
	u8 buf[2];

	if (unlock) {
		buf[0] = MXT_UNLOCK_CMD_LSB;
		buf[1] = MXT_UNLOCK_CMD_MSB;
	} else {
		buf[0] = 0x01;
		buf[1] = 0x01;
	}

	ret = mxt_bootloader_write(data, buf, 2);
	if (ret)
		return ret;

	return 0;
}

static int __mxt_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];
	int ret;
	int retry = MXT_MAX_RETRIES;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

retry_read:
	ret = i2c_transfer(client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		if (--retry) {
			dev_dbg(&client->dev, "%s: i2c retry %d\n",
				__func__, MXT_MAX_RETRIES - retry);
			msleep(MXT_WAKEUP_TIME);
			goto retry_read;
		} else {
			dev_err(&client->dev, "%s: i2c read @%d failed (%d)\n",
				__func__, reg, ret);
			return -EIO;
		}
	}

	return 0;
}

static int __mxt_write_reg(struct i2c_client *client, u16 reg, u16 len,
			   const void *val)
{
	u8 *buf;
	size_t count;
	int ret;
	int retry = MXT_MAX_RETRIES;

	count = len + 2;
	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	memcpy(&buf[2], val, len);

retry_write:
	ret = i2c_master_send(client, buf, count);
	if (ret != count) {
		if (--retry) {
			dev_dbg(&client->dev, "%s: i2c retry %d\n",
				__func__, MXT_MAX_RETRIES - retry);
			msleep(MXT_WAKEUP_TIME);
			goto retry_write;
		} else {
			dev_err(&client->dev, "%s: i2c write @%d failed (%d)\n",
				__func__, reg, ret);
			ret = -EIO;
		}
	} else {
		ret = 0;
	}

	kfree(buf);
	return ret;
}

static int mxt_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	return __mxt_write_reg(client, reg, 1, &val);
}

static struct mxt_object *mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_warn(&data->client->dev, "Invalid object type T%u\n", type);
	return NULL;
}

static void mxt_proc_t6_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];
	u32 crc = msg[2] | (msg[3] << 8) | (msg[4] << 16);

	if (crc != data->config_crc) {
		data->config_crc = crc;
		dev_dbg(dev, "T6 Config Checksum: 0x%06X\n", crc);
		complete(&data->crc_completion);
	}

	/* Detect transition out of reset */
	if ((data->t6_status & MXT_T6_STATUS_RESET) &&
	    !(status & MXT_T6_STATUS_RESET))
		complete(&data->reset_completion);

	/* Output debug if status has changed */
	if (status != data->t6_status)
		dev_dbg(dev, "T6 Status 0x%02X%s%s%s%s%s%s%s\n",
			status,
			(status == 0) ? " OK" : "",
			(status & MXT_T6_STATUS_RESET) ? " RESET" : "",
			(status & MXT_T6_STATUS_OFL) ? " OFL" : "",
			(status & MXT_T6_STATUS_SIGERR) ? " SIGERR" : "",
			(status & MXT_T6_STATUS_CAL) ? " CAL" : "",
			(status & MXT_T6_STATUS_CFGERR) ? " CFGERR" : "",
			(status & MXT_T6_STATUS_COMSERR) ? " COMSERR" : "");

	/* Save current status */
	data->t6_status = status;
}

static void mxt_input_button(struct mxt_data *data, u8 *message)
{
	struct input_dev *input = data->input_dev;
	const struct mxt_platform_data *pdata = data->pdata;
	bool button;
	int i;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	/* Active-low switch */
	for (i = 0; i < pdata->t19_num_keys; i++) {
		if (pdata->t19_keymap[i] == KEY_RESERVED)
			continue;
		button = !(message[1] & (1 << i));
		input_report_key(input, pdata->t19_keymap[i], button);
	}
}

static void mxt_input_sync(struct input_dev *input_dev)
{
	input_mt_report_pointer_emulation(input_dev, false);
	input_sync(input_dev);
}

static void mxt_proc_t9_message(struct mxt_data *data, u8 *message)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int area;
	int amplitude;
	u8 vector;
	int tool;
	struct timespec hw_time = ktime_to_timespec(ktime_get());

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	input_event(input_dev, EV_SYN, SYN_TIME_SEC, hw_time.tv_sec);
	input_event(input_dev, EV_SYN, SYN_TIME_NSEC, hw_time.tv_nsec);

	id = message[0] - data->T9_reportid_min;
	status = message[1];
	x = (message[2] << 4) | ((message[4] >> 4) & 0xf);
	y = (message[3] << 4) | ((message[4] & 0xf));

	/* Handle 10/12 bit switching */
	if (data->max_x < 1024)
		x >>= 2;
	if (data->max_y < 1024)
		y >>= 2;

	area = message[5];

	amplitude = message[6];
	vector = message[7];

	dev_dbg(dev,
		"[%u] %c%c%c%c%c%c%c%c x: %5u y: %5u area: %3u amp: %3u vector: %02X\n",
		id,
		(status & MXT_T9_DETECT) ? 'D' : '.',
		(status & MXT_T9_PRESS) ? 'P' : '.',
		(status & MXT_T9_RELEASE) ? 'R' : '.',
		(status & MXT_T9_MOVE) ? 'M' : '.',
		(status & MXT_T9_VECTOR) ? 'V' : '.',
		(status & MXT_T9_AMP) ? 'A' : '.',
		(status & MXT_T9_SUPPRESS) ? 'S' : '.',
		(status & MXT_T9_UNGRIP) ? 'U' : '.',
		x, y, area, amplitude, vector);

	input_mt_slot(input_dev, id);

	if (status & MXT_T9_DETECT) {
		/* Multiple bits may be set if the host is slow to read the
		 * status messages, indicating all the events that have
		 * happened */
		if (status & MXT_T9_RELEASE) {
			input_mt_report_slot_state(input_dev,
						   MT_TOOL_FINGER, 0);
			mxt_input_sync(input_dev);
		}

		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if (area == 0) {
			area = MXT_TOUCH_MAJOR_T47_STYLUS;
			tool = MT_TOOL_PEN;
		} else {
			tool = MT_TOOL_FINGER;
		}

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, amplitude);
		input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, area);
		input_report_abs(input_dev, ABS_MT_ORIENTATION, vector);
	} else {
		/* Touch no longer active, close out slot */
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	data->update_input = true;
}

static void mxt_proc_t100_message(struct mxt_data *data, u8 *message)
{
	struct input_dev *input_dev = data->input_dev;
	int id;
	u8 status;
	int x;
	int y;
	int tool;
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	unsigned char number_of_fingers_actually_touching;
	static char finger_register[32];
#endif
	struct timespec hw_time = ktime_to_timespec(ktime_get());

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	id = message[0] - data->T100_reportid_min - 2;

	/* ignore SCRSTATUS events */
	if (id < 0)
		return;

	input_event(input_dev, EV_SYN, SYN_TIME_SEC, hw_time.tv_sec);
	input_event(input_dev, EV_SYN, SYN_TIME_NSEC, hw_time.tv_nsec);

	status = message[1];
	x = (message[3] << 8) | message[2];
	y = (message[5] << 8) | message[4];

	if (!throttle_dbgout(&mxt_proc_t100_dbg, 20, 60000))
		dev_dbg(&data->client->dev,
		"[%u] status:%02X x:%u y:%u area:%02X amp:%02X vec:%02X\n",
		id,
		status,
		x, y,
		(data->t100_aux_area) ? message[data->t100_aux_area] : 0,
		(data->t100_aux_ampl) ? message[data->t100_aux_ampl] : 0,
		(data->t100_aux_vect) ? message[data->t100_aux_vect] : 0);

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (touchxp.touchx)
		mutex_lock(&touchxp.virtual_touch_mutex);

	finger_register[id < 32 ? id : 31] += 1;
#endif

	input_mt_slot(input_dev, id);

	if (status & MXT_T100_DETECT) {
		/* A reported size of zero indicates that the reported touch
		 * is a stylus from a linked Stylus T47 object. */
		if ((status & MXT_T100_TYPE_MASK) == MXT_T100_TYPE_STYLUS)
			tool = MT_TOOL_PEN;
		else
			tool = MT_TOOL_FINGER;

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
		if (touchxp.touchx) {
			int i;
			number_of_fingers_actually_touching = 0;
			for (i = 0; i < 32; i++) {
				if (finger_register[i] != 0)
					number_of_fingers_actually_touching++;
			}
			touchxp.touch_magic_dev = input_dev;
			touchxp.touchx(&x, &y, id,
				       number_of_fingers_actually_touching);
		}
#endif

		/* Touch active */
		input_mt_report_slot_state(input_dev, tool, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);

		if (data->t100_aux_ampl)
			input_report_abs(input_dev, ABS_MT_PRESSURE,
					 message[data->t100_aux_ampl]);

		if (data->t100_aux_area) {
			if (tool == MT_TOOL_PEN)
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
						 MXT_TOUCH_MAJOR_T47_STYLUS);
			else
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
						 message[data->t100_aux_area]);
		}

		if (data->t100_aux_vect)
			input_report_abs(input_dev, ABS_MT_ORIENTATION,
					 message[data->t100_aux_vect]);
	} else {
		/* Touch no longer active, close out slot */
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
		touchxp.finger_down = 0;
		finger_register[id < 32 ? id : 31] = 0;
#endif
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (touchxp.touchx)
		mutex_unlock(&touchxp.virtual_touch_mutex);
#endif

	data->update_input = true;
}

static void mxt_proc_t15_messages(struct mxt_data *data, u8 *msg)
{
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	int key;
	bool curr_state, new_state;
	bool sync = false;
	unsigned long keystates = le32_to_cpu(msg[2]);

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	for (key = 0; key < data->pdata->t15_num_keys; key++) {
		curr_state = test_bit(key, &data->t15_keystatus);
		new_state = test_bit(key, &keystates);

		if (!curr_state && new_state) {
			dev_dbg(dev, "T15 key press: %u\n", key);
			__set_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
				    data->pdata->t15_keymap[key], 1);
			sync = true;
		} else if (curr_state && !new_state) {
			dev_dbg(dev, "T15 key release: %u\n", key);
			__clear_bit(key, &data->t15_keystatus);
			input_event(input_dev, EV_KEY,
				    data->pdata->t15_keymap[key], 0);
			sync = true;
		}
	}

	if (sync)
		input_sync(input_dev);
}

static void mxt_proc_t42_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status = msg[1];

	if (status & MXT_T42_MSG_TCHSUP)
		dev_info(dev, "T42 suppress\n");
	else
		dev_info(dev, "T42 normal\n");
}

static int mxt_proc_t48_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	u8 status, state;

	status = msg[1];
	state  = msg[4];

	dev_dbg(dev, "T48 state %d status %02X %s%s%s%s%s\n",
			state,
			status,
			(status & 0x01) ? "FREQCHG " : "",
			(status & 0x02) ? "APXCHG " : "",
			(status & 0x04) ? "ALGOERR " : "",
			(status & 0x10) ? "STATCHG " : "",
			(status & 0x20) ? "NLVLCHG " : "");

	return 0;
}

static void mxt_proc_t63_messages(struct mxt_data *data, u8 *msg)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev = data->input_dev;
	u8 id;
	u16 x, y;
	u8 pressure;

	/* do not report events if input device not yet registered */
	if (!data->enable_reporting)
		return;

	/* stylus slots come after touch slots */
	id = data->num_touchids + (msg[0] - data->T63_reportid_min);

	if (id < 0 || id > (data->num_touchids + data->num_stylusids)) {
		dev_err(dev, "invalid stylus id %d, max slot is %d\n",
			id, data->num_stylusids);
		return;
	}

	x = msg[3] | (msg[4] << 8);
	y = msg[5] | (msg[6] << 8);
	pressure = msg[7] & MXT_T63_STYLUS_PRESSURE_MASK;

	dev_dbg(dev,
		"[%d] %c%c%c%c x: %d y: %d pressure: %d stylus:%c%c%c%c\n",
		id,
		(msg[1] & MXT_T63_STYLUS_SUPPRESS) ? 'S' : '.',
		(msg[1] & MXT_T63_STYLUS_MOVE)     ? 'M' : '.',
		(msg[1] & MXT_T63_STYLUS_RELEASE)  ? 'R' : '.',
		(msg[1] & MXT_T63_STYLUS_PRESS)    ? 'P' : '.',
		x, y, pressure,
		(msg[2] & MXT_T63_STYLUS_BARREL) ? 'B' : '.',
		(msg[2] & MXT_T63_STYLUS_ERASER) ? 'E' : '.',
		(msg[2] & MXT_T63_STYLUS_TIP)    ? 'T' : '.',
		(msg[2] & MXT_T63_STYLUS_DETECT) ? 'D' : '.');

	input_mt_slot(input_dev, id);

	if (msg[2] & MXT_T63_STYLUS_DETECT) {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 1);
		input_report_abs(input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
		input_report_abs(input_dev, ABS_MT_PRESSURE, pressure);
	} else {
		input_mt_report_slot_state(input_dev, MT_TOOL_PEN, 0);
	}

	input_report_key(input_dev, BTN_STYLUS,
			 (msg[2] & MXT_T63_STYLUS_ERASER));
	input_report_key(input_dev, BTN_STYLUS2,
			 (msg[2] & MXT_T63_STYLUS_BARREL));

	mxt_input_sync(input_dev);
}

static int mxt_proc_message(struct mxt_data *data, u8 *message)
{
	u8 report_id = message[0];
	bool dump = data->debug_enabled;

	if (report_id == MXT_RPTID_NOMSG)
		return 0;

	if (report_id == data->T6_reportid) {
		mxt_proc_t6_messages(data, message);
	} else if (report_id >= data->T9_reportid_min
	    && report_id <= data->T9_reportid_max) {
		mxt_proc_t9_message(data, message);
	} else if (report_id >= data->T100_reportid_min
	    && report_id <= data->T100_reportid_max) {
		mxt_proc_t100_message(data, message);
	} else if (report_id == data->T19_reportid) {
		mxt_input_button(data, message);
		data->update_input = true;
	} else if (report_id >= data->T63_reportid_min
		   && report_id <= data->T63_reportid_max) {
		mxt_proc_t63_messages(data, message);
	} else if (report_id >= data->T42_reportid_min
		   && report_id <= data->T42_reportid_max) {
		mxt_proc_t42_messages(data, message);
	} else if (report_id == data->T48_reportid) {
		mxt_proc_t48_messages(data, message);
	} else if (report_id >= data->T15_reportid_min
		   && report_id <= data->T15_reportid_max) {
		mxt_proc_t15_messages(data, message);
	} else {
		dump = true;
	}

	if (dump)
		mxt_dump_message(data, message);

	if (data->debug_v2_enabled)
		mxt_debug_msg_add(data, message);

	return 1;
}

static int mxt_read_and_process_messages(struct mxt_data *data, u8 count)
{
	struct device *dev = &data->client->dev;
	int ret;
	int i;
	u8 num_valid = 0;

	/* Safety check for msg_buf */
	if (count > data->max_reportid)
		return -EINVAL;

	/* Process remaining messages if necessary */
	ret = __mxt_read_reg(data->client, data->T5_address,
				data->T5_msg_size * count, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read %u messages (%d)\n", count, ret);
		return ret;
	}

	for (i = 0;  i < count; i++) {
		ret = mxt_proc_message(data,
			data->msg_buf + data->T5_msg_size * i);

		if (ret == 1)
			num_valid++;
	}

	/* return number of messages read */
	return num_valid;
}

static irqreturn_t mxt_process_messages_t44(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret;
	u8 count, num_left;

	if (!data->msg_buf) {
		dev_err(dev, "Message buffer not allocated!!!\n");
		return IRQ_NONE;
	}

	/* Read T44 and T5 together */
	ret = __mxt_read_reg(data->client, data->T44_address,
		data->T5_msg_size + 1, data->msg_buf);
	if (ret) {
		dev_err(dev, "Failed to read T44 and T5 (%d)\n", ret);
		return IRQ_NONE;
	}

	count = data->msg_buf[0];

	if (count == 0) {
		dev_warn(dev, "Interrupt triggered but zero messages\n");
		return IRQ_NONE;
	} else if (count > data->max_reportid) {
		dev_err(dev, "T44 count %d exceeded max report id\n", count);
		count = data->max_reportid;
	}

	/* Process first message */
	ret = mxt_proc_message(data, data->msg_buf + 1);
	if (ret < 0) {
		dev_warn(dev, "Unexpected invalid message\n");
		return IRQ_NONE;
	}

	num_left = count - 1;

	/* Process remaining messages if necessary */
	if (num_left) {
		ret = mxt_read_and_process_messages(data, num_left);
		if (ret < 0)
			goto end;
		else if (ret != num_left)
			dev_warn(dev, "Unexpected invalid message\n");
	}

end:
	if (data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static int mxt_process_messages_until_invalid(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int num_handled, processed = 0;
	bool pin_high;

	do {
		/* It appears host has to read "beyond" the message */
		/* with invalid id to ensure CHG line gets deasserted */
		num_handled = mxt_read_and_process_messages(data, 3);
		processed += num_handled;
	} while (num_handled > 0);

	if (processed)
		dev_dbg(dev, "processed %d messages\n", processed);

	pin_high = gpio_get_value(data->pdata->gpio_irq) == 1;
	if (!pin_high)
		dev_err(dev, "CHG pin still asserted\n");

	return 0;
}

static irqreturn_t mxt_process_messages(struct mxt_data *data)
{
	int total_handled, num_handled;
	u8 count = data->last_message_count;

	if (count < 1 || count > data->max_reportid)
		count = 1;

	/* include final invalid message */
	total_handled = mxt_read_and_process_messages(data, count + 1);
	if (total_handled < 0)
		return IRQ_NONE;
	/* if there were invalid messages, then we are done */
	else if (total_handled <= count)
		goto update_count;

	/* read two at a time until an invalid message or else we reach
	 * reportid limit */
	do {
		num_handled = mxt_read_and_process_messages(data, 2);
		if (num_handled < 0)
			return IRQ_NONE;

		total_handled += num_handled;

		if (num_handled < 2)
			break;
	} while (total_handled < data->num_touchids);

update_count:
	data->last_message_count = total_handled;

	if (data->enable_reporting && data->update_input) {
		mxt_input_sync(data->input_dev);
		data->update_input = false;
	}

	return IRQ_HANDLED;
}

static irqreturn_t mxt_interrupt(int irq, void *dev_id)
{
	struct mxt_data *data = dev_id;
	int state = mxt_get_sensor_state(data);

	if (data->in_bootloader  || data->suspended ||
		(state == STATE_UNKNOWN)) {
		/* bootloader state transition completion */
		complete(&data->bl_completion);
		return IRQ_HANDLED;
	}

	if (!data->object_table)
		return IRQ_NONE;

	if (data->T44_address)
		return mxt_process_messages_t44(data);
	else
		return mxt_process_messages(data);
}

static int mxt_t6_command(struct mxt_data *data, u16 cmd_offset,
			  u8 value, bool wait)
{
	u16 reg;
	u8 command_register;
	int timeout_counter = 0;
	int ret;

	reg = data->T6_address + cmd_offset;

	ret = mxt_write_reg(data->client, reg, value);
	if (ret)
		return ret;

	if (!wait)
		return 0;

	do {
		msleep(20);
		ret = __mxt_read_reg(data->client, reg, 1, &command_register);
		if (ret)
			return ret;
	} while ((command_register != 0) && (timeout_counter++ <= 100));

	if (timeout_counter > 100) {
		dev_err(&data->client->dev, "Command failed!\n");
		return -EIO;
	}

	return 0;
}

static int mxt_read_revision_id(struct mxt_data *data, u8 *revision)
{
	struct mxt_object *object;
	int error = -EINVAL;

	object = mxt_get_object(data, MXT_DEBUG_DIAGNOSTIC_T37);
	if (!object)
		goto err_exit;

	error = mxt_t6_command(data, MXT_COMMAND_DIAGNOSTIC,
				MXT_IDENT_VALUE, true);
	if (error)
		goto err_exit;

	error = __mxt_read_reg(data->client,
			       object->start_address + MXT_T37_REVISION,
			       sizeof(*revision), revision);
err_exit:
	return error;
}

static int mxt_soft_reset(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int ret = 0;

	dev_info(dev, "Resetting chip\n");

	INIT_COMPLETION(data->reset_completion);

	ret = mxt_t6_command(data, MXT_COMMAND_RESET, MXT_RESET_VALUE, false);
	if (ret)
		return ret;

	ret = mxt_wait_for_completion(data, &data->reset_completion,
				      MXT_RESET_TIMEOUT);
	if (ret)
		return ret;

	return 0;
}

static void mxt_update_crc(struct mxt_data *data, u8 cmd, u8 value)
{
	/* on failure, CRC is set to 0 and config will always be downloaded */
	data->config_crc = 0;
	INIT_COMPLETION(data->crc_completion);

	mxt_t6_command(data, cmd, value, true);

	/* Wait for crc message. On failure, CRC is set to 0 and config will
	 * always be downloaded */
	mxt_wait_for_completion(data, &data->crc_completion, MXT_CRC_TIMEOUT);
}

static void mxt_calc_crc24(u32 *crc, u8 firstbyte, u8 secondbyte)
{
	static const unsigned int crcpoly = 0x80001B;
	u32 result;
	u32 data_word;

	data_word = (secondbyte << 8) | firstbyte;
	result = ((*crc << 1) ^ data_word);

	if (result & 0x1000000)
		result ^= crcpoly;

	*crc = result;
}

static u32 mxt_calculate_crc(u8 *base, off_t start_off, off_t end_off)
{
	u32 crc = 0;
	u8 *ptr = base + start_off;
	u8 *last_val = base + end_off - 1;

	if (end_off < start_off)
		return -EINVAL;

	while (ptr < last_val) {
		mxt_calc_crc24(&crc, *ptr, *(ptr + 1));
		ptr += 2;
	}

	/* if len is odd, fill the last byte with 0 */
	if (ptr == last_val)
		mxt_calc_crc24(&crc, *ptr, 0);

	/* Mask to 24-bit */
	crc &= 0x00FFFFFF;

	return crc;
}

static int mxt_check_retrigen(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	int val;

	if (data->pdata->irqflags & IRQF_TRIGGER_LOW)
		return 0;

	if (data->T18_address) {
		error = __mxt_read_reg(client,
				       data->T18_address + MXT_COMMS_CTRL,
				       1, &val);
		if (error)
			return error;

		if (val & MXT_COMMS_RETRIGEN)
			return 0;
	}

	dev_warn(&client->dev, "Enabling RETRIGEN workaround\n");
	data->use_retrigen_workaround = true;
	return 0;
}

/*
 * mxt_check_reg_init - download configuration to chip
 *
 * Atmel Raw Config File Format
 *
 * The first four lines of the raw config file contain:
 *  1) Version
 *  2) Chip ID Information (first 7 bytes of device memory)
 *  3) Chip Information Block 24-bit CRC Checksum
 *  4) Chip Configuration 24-bit CRC Checksum
 *
 * The rest of the file consists of one line per object instance:
 *   <TYPE> <INSTANCE> <SIZE> <CONTENTS>
 *
 *   <TYPE> - 2-byte object type as hex
 *   <INSTANCE> - 2-byte object instance number as hex
 *   <SIZE> - 2-byte object size as hex
 *   <CONTENTS> - array of <SIZE> 1-byte hex values
 */
static int mxt_check_reg_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_info cfg_info;
	struct mxt_object *object;
	const struct firmware *cfg = NULL;
	int ret;
	int offset;
	int data_pos;
	int byte_offset;
	int i;
	int cfg_start_ofs;
	u32 info_crc, config_crc, calculated_crc;
	u8 *config_mem;
	size_t config_mem_size;
	unsigned int type, instance, size;
	u8 val;
	u16 reg;

	if (!data->cfg_name)
		return 0;

	ret = request_firmware(&cfg, data->cfg_name, dev);
	if (ret < 0) {
		dev_err(dev, "Failure to request config file %s\n",
			data->cfg_name);
		return 0;
	}

	mxt_update_crc(data, MXT_COMMAND_REPORTALL, 1);

	if (strncmp(cfg->data, MXT_CFG_MAGIC, strlen(MXT_CFG_MAGIC))) {
		dev_err(dev, "Unrecognised config file\n");
		ret = -EINVAL;
		goto release;
	}

	data_pos = strlen(MXT_CFG_MAGIC);

	/* Load information block and check */
	for (i = 0; i < sizeof(struct mxt_info); i++) {
		ret = sscanf(cfg->data + data_pos, "%hhx%n",
			     (unsigned char *)&cfg_info + i,
			     &offset);
		if (ret != 1) {
			dev_err(dev, "Bad format\n");
			ret = -EINVAL;
			goto release;
		}

		data_pos += offset;
	}

	if (cfg_info.family_id != data->info->family_id) {
		dev_err(dev, "Family ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	if (cfg_info.variant_id != data->info->variant_id) {
		dev_err(dev, "Variant ID mismatch!\n");
		ret = -EINVAL;
		goto release;
	}

	/* Read CRCs */
	ret = sscanf(cfg->data + data_pos, "%x%n", &info_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Info CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	ret = sscanf(cfg->data + data_pos, "%x%n", &config_crc, &offset);
	if (ret != 1) {
		dev_err(dev, "Bad format: failed to parse Config CRC\n");
		ret = -EINVAL;
		goto release;
	}
	data_pos += offset;

	/* The Info Block CRC is calculated over mxt_info and the object table
	 * If it does not match then we are trying to load the configuration
	 * from a different chip or firmware version, so the configuration CRC
	 * is invalid anyway. */
	if (info_crc == data->info_crc) {
		if (config_crc == 0 || data->config_crc == 0) {
			dev_info(dev, "CRC zero, attempting to apply config\n");
		} else if (config_crc == data->config_crc) {
			dev_info(dev, "Config CRC 0x%06X: OK\n",
				 data->config_crc);
			ret = 0;
			goto release;
		} else {
			dev_info(dev, "Config CRC 0x%06X: does not match file 0x%06X\n",
				 data->config_crc, config_crc);
		}
	} else {
		dev_warn(dev,
			 "Warning: Info CRC error - device=0x%06X file=0x%06X\n",
			data->info_crc, info_crc);
	}

	/* Malloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START
		+ data->info->object_num * sizeof(struct mxt_object)
		+ MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto release;
	}

	while (data_pos < cfg->size) {
		/* Read type, instance, length */
		ret = sscanf(cfg->data + data_pos, "%x %x %x%n",
			     &type, &instance, &size, &offset);
		if (ret == 0) {
			/* EOF */
			break;
		} else if (ret != 3) {
			dev_err(dev, "Bad format: failed to parse object\n");
			ret = -EINVAL;
			goto release_mem;
		}
		data_pos += offset;

		object = mxt_get_object(data, type);
		if (!object) {
			/* Skip object */
			for (i = 0; i < size; i++) {
				ret = sscanf(cfg->data + data_pos, "%hhx%n",
					     &val,
					     &offset);
				data_pos += offset;
			}
			continue;
		}

		if (size > mxt_obj_size(object)) {
			/* Either we are in fallback mode due to wrong
			 * config or config from a later fw version,
			 * or the file is corrupt or hand-edited */
			dev_warn(dev, "Discarding %u byte(s) in T%u\n",
				 size - mxt_obj_size(object), type);
		} else if (mxt_obj_size(object) > size) {
			/* If firmware is upgraded, new bytes may be added to
			 * end of objects. It is generally forward compatible
			 * to zero these bytes - previous behaviour will be
			 * retained. However this does invalidate the CRC and
			 * will force fallback mode until the configuration is
			 * updated. We warn here but do nothing else - the
			 * malloc has zeroed the entire configuration. */
			dev_warn(dev, "Zeroing %u byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}

		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		reg = object->start_address + mxt_obj_size(object) * instance;

		for (i = 0; i < size; i++) {
			ret = sscanf(cfg->data + data_pos, "%hhx%n",
				     &val,
				     &offset);
			if (ret != 1) {
				dev_err(dev, "Bad format in T%d\n", type);
				ret = -EINVAL;
				goto release_mem;
			}
			data_pos += offset;

			if (i > mxt_obj_size(object))
				continue;

			byte_offset = reg + i - cfg_start_ofs;

			if ((byte_offset >= 0)
			    && (byte_offset <= config_mem_size)) {
				*(config_mem + byte_offset) = val;
			} else {
				dev_err(dev, "Bad object: reg:%d, T%d, ofs=%d\n",
					reg, object->type, byte_offset);
				ret = -EINVAL;
				goto release_mem;
			}
		}
	}

	/* calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < cfg_start_ofs) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		ret = 0;
		goto release_mem;
	}

	calculated_crc = mxt_calculate_crc(config_mem,
					   data->T7_address - cfg_start_ofs,
					   config_mem_size);

	if (config_crc > 0 && (config_crc != calculated_crc))
		dev_warn(dev, "Config CRC error, calculated=%06X, file=%06X\n",
			 calculated_crc, config_crc);

	/* Write configuration as blocks */
	byte_offset = 0;
	while (byte_offset < config_mem_size) {
		size = config_mem_size - byte_offset;

		if (size > MXT_MAX_BLOCK_WRITE)
			size = MXT_MAX_BLOCK_WRITE;

		ret = __mxt_write_reg(data->client,
				      cfg_start_ofs + byte_offset,
				      size, config_mem + byte_offset);
		if (ret != 0) {
			dev_err(dev, "Config write error, ret=%d\n", ret);
			goto release_mem;
		}

		byte_offset += size;
	}

	mxt_update_crc(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);

	ret = mxt_check_retrigen(data);
	if (ret)
		goto release_mem;

	ret = mxt_soft_reset(data);
	if (ret)
		goto release_mem;

	dev_info(dev, "Config written\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);

release_mem:
	kfree(config_mem);
release:
	release_firmware(cfg);
	return ret;
}

static int mxt_set_t7_power_cfg(struct mxt_data *data, u8 sleep)
{
	struct device *dev = &data->client->dev;
	int error;
	struct t7_config *new_config;
	struct t7_config deepsleep = { .active = 0, .idle = 0 };

	if (sleep == MXT_POWER_CFG_DEEPSLEEP)
		new_config = &deepsleep;
	else
		new_config = &data->t7_cfg;

	error = __mxt_write_reg(data->client, data->T7_address,
			sizeof(data->t7_cfg),
			new_config);
	if (error)
		return error;

	dev_dbg(dev, "Set T7 ACTV:%d IDLE:%d\n",
		new_config->active, new_config->idle);

	return 0;
}

static int mxt_init_t7_power_cfg(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;
	bool retry = false;

recheck:
	error = __mxt_read_reg(data->client, data->T7_address,
				sizeof(data->t7_cfg), &data->t7_cfg);
	if (error)
		return error;

	if (data->t7_cfg.active == 0 || data->t7_cfg.idle == 0) {
		if (!retry) {
			dev_info(dev, "T7 cfg zero, resetting\n");
			mxt_soft_reset(data);
			retry = true;
			goto recheck;
		}

		dev_dbg(dev, "T7 cfg zero after reset, overriding\n");
		data->t7_cfg.active = 20;
		data->t7_cfg.idle = 100;
		return mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);
	}

	dev_info(dev, "Initialized power cfg: ACTV %d, IDLE %d\n",
				data->t7_cfg.active, data->t7_cfg.idle);
	return 0;
}

static bool mxt_t47_stylus_state(struct mxt_data *data)
{
	struct mxt_object *object;
	int error;
	u8 control = 0;

	object = mxt_get_object(data, MXT_PROCI_STYLUS_T47);
	if (!object)
		return false;

	error = __mxt_read_reg(data->client, object->start_address,
				sizeof(control), &control);
	if (error)
		dev_warn(&data->client->dev, "Unable to read T47\n");

	return control ? true : false;
}

static inline int mxt_acquire_irq(struct mxt_data *data)
{
	return mxt_process_messages_until_invalid(data);
}

static void mxt_irq_enable(struct mxt_data *data, bool enable)
{
	if (enable && !data->irq_enabled) {
		enable_irq(data->irq);
		data->irq_enabled = true;
	} else if (!enable && data->irq_enabled) {
		disable_irq(data->irq);
		data->irq_enabled = false;
	}
}

static void mxt_apply_patches(struct mxt_data *data)
{
	struct obj_patch *patch;
	struct mxt_object *object;
	int error;

	list_for_each_entry(patch, &mxt_cfg_data.cfg_head, link) {
		dev_dbg(&data->client->dev, "patching T%d@%d,0x%x\n",
				patch->number, patch->offset, patch->value);

		object = mxt_get_object(data, patch->number);
		if (!object)
			continue;

		if (patch->offset >= object->size_minus_one)
			continue;

		error = __mxt_write_reg(data->client,
				      object->start_address + patch->offset,
				      1, &patch->value);
		if (error)
			dev_warn(&data->client->dev, "patch failed\n");
	}
}

static void mxt_sensor_one_touch(struct mxt_data *data, bool enable)
{
	struct mxt_object *object;
	int error;
	u8 *t100_data_ptr;

	if (!data->T100_data) {
		dev_dbg(&data->client->dev, "T100: data not available\n");
		return;
	}

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!object)
		return;

	if (enable) {
		t100_data_ptr = kmalloc(mxt_obj_size(object), GFP_KERNEL);
		if (!t100_data_ptr) {
			dev_err(&data->client->dev,
				"T100: failed to allocate buffer\n");
			return;
		}

		memcpy(t100_data_ptr, data->T100_data, mxt_obj_size(object));

		/* make necessary changes here */
		*(t100_data_ptr + MXT_T100_NUMTCH) = 1;
		*(t100_data_ptr + MXT_T100_TCHAUX) &= ~(MXT_T100_TCHAUX_VECT |
				MXT_T100_TCHAUX_AMPL | MXT_T100_TCHAUX_AREA);
		*(t100_data_ptr + MXT_T100_MOVHYSTI) = 0x32;
		*(t100_data_ptr + MXT_T100_MOVHYSTN) = 0x32;

		/* if defined, reset panel resolution */
		if (data->pdata->res.x_max && data->pdata->res.y_max) {
			*((u16 *)(t100_data_ptr + MXT_T100_XRANGE)) =
					cpu_to_le16(data->pdata->res.x_max - 1);
			*((u16 *)(t100_data_ptr + MXT_T100_YRANGE)) =
					cpu_to_le16(data->pdata->res.y_max - 1);
		}
	} else
		t100_data_ptr = data->T100_data;

	error = __mxt_write_reg(data->client, object->start_address,
				mxt_obj_size(object), t100_data_ptr);
	if (error)
		dev_warn(&data->client->dev,
			"Failed to %s single-touch config\n",
			enable ? "enable" : "disable");
	else
		dev_dbg(&data->client->dev,
			"Successfully %s single-touch config\n",
			enable ? "enabled" : "disabled");

	if (t100_data_ptr != data->T100_data)
		kfree(t100_data_ptr);
}

static void mxt_wait_for_idle(struct mxt_data *data)
{
	int state, timeout_retries = 0;
	unsigned long start_wait_jiffies = jiffies;

	/* Reset completion indicated by asserting CHG  */
	/* Wait for CHG asserted or timeout after 200ms */
	do {
		state = gpio_get_value(data->pdata->gpio_irq);
		if (!state)
			break;
		usleep_range(1000, 1000);
	} while (++timeout_retries < 100);

	if (!state)
		dev_dbg(&data->client->dev, "reset took %ums\n",
			jiffies_to_msecs(jiffies - start_wait_jiffies));
	else
		dev_warn(&data->client->dev, "timeout waiting for idle %ums\n",
			jiffies_to_msecs(jiffies - start_wait_jiffies));
}

static void mxt_sensor_sleep(struct mxt_data *data)
{
	mxt_set_t7_power_cfg(data, MXT_POWER_CFG_DEEPSLEEP);
	data->sensor_sleep = true;
	dev_dbg(&data->client->dev, "LPM mode\n");
}

static void mxt_sensor_wake(struct mxt_data *data, bool calibrate)
{
	mxt_set_t7_power_cfg(data, MXT_POWER_CFG_RUN);

	if (calibrate) {
		/* Recalibrate since chip has been in deep sleep */
		mxt_t6_command(data, MXT_COMMAND_CALIBRATE, 1, false);
		dev_dbg(&data->client->dev, "calibrating\n");
	}

	data->sensor_sleep = false;
	dev_dbg(&data->client->dev, "Normal mode\n");
}

#define MXT_DEF(a)	(#a)
static const char * const mxt_state_names[] = ATMEL_MXT_STATES;
#undef MXT_DEF

static const char *mxt_state_name(int state)
{
	int index = state < 0 || state > STATE_INVALID ? STATE_INVALID : state;
	return mxt_state_names[index];
}

static int mxt_get_sensor_state(struct mxt_data *data)
{
	return atomic_read(&data->state);
}

static void mxt_set_sensor_state(struct mxt_data *data, int state)
{
	int current_state = mxt_get_sensor_state(data);

	if (current_state == state)
		return;

	switch (state) {
	case STATE_QUERY:
		/* drop flag to allow object specific message handling */
		if (data->in_bootloader)
			data->in_bootloader = false;
	case STATE_UNKNOWN:
	case STATE_FLASH:
		/* no special handling for these states */
			break;

	case STATE_SUSPEND:
		mxt_irq_enable(data, false);
		data->enable_reporting = false;

		if (data->one_touch_enabled)
			mxt_sensor_one_touch(data, true);
			break;

	case STATE_ACTIVE:
		if (!data->use_regulator && data->sensor_sleep)
			mxt_sensor_wake(data, false);

		if (data->one_touch_enabled)
			mxt_sensor_one_touch(data, false);

		if (mxt_cfg_data.cfg_num)
			mxt_apply_patches(data);

		mxt_irq_enable(data, true);
		data->enable_reporting = true;
			break;

	case STATE_STANDBY:
		mxt_irq_enable(data, false);
		/* put sensor to sleep to ensure the same */
		/* initial conditions apply to all */
		if (!data->use_regulator && !data->sensor_sleep)
			mxt_sensor_sleep(data);
			break;

	case STATE_BL:
		if (!data->in_bootloader)
			data->in_bootloader = true;

		mxt_irq_enable(data, false);
			break;

	case STATE_INIT:
		/* set flag to avoid object specific message handling */
		if (!data->in_bootloader)
			data->in_bootloader = true;
			break;
	}

	pr_info("state change %s -> %s\n", mxt_state_name(current_state),
			mxt_state_name(state));
	atomic_set(&data->state, state);
}

static void mxt_free_input_device(struct mxt_data *data)
{
	if (data->input_dev) {
		if (data->input_registered)
			input_unregister_device(data->input_dev);
		else
			input_free_device(data->input_dev);
		data->input_dev = NULL;
		data->input_registered = false;
	}
}

static void mxt_free_object_table(struct mxt_data *data)
{
	mxt_debug_msg_remove(data);

	kfree(data->raw_info_block);
	data->object_table = NULL;
	data->info = NULL;
	data->raw_info_block = NULL;
	kfree(data->msg_buf);
	data->msg_buf = NULL;
	kfree(data->T100_data);
	data->T100_data = NULL;

	mxt_free_input_device(data);

	data->enable_reporting = false;
	data->T5_address = 0;
	data->T5_msg_size = 0;
	data->T6_reportid = 0;
	data->T7_address = 0;
	data->T9_reportid_min = 0;
	data->T9_reportid_max = 0;
	data->T15_reportid_min = 0;
	data->T15_reportid_max = 0;
	data->T18_address = 0;
	data->T19_reportid = 0;
	data->T42_reportid_min = 0;
	data->T42_reportid_max = 0;
	data->T44_address = 0;
	data->T48_reportid = 0;
	data->T63_reportid_min = 0;
	data->T63_reportid_max = 0;
	data->T100_reportid_min = 0;
	data->T100_reportid_max = 0;
	data->max_reportid = 0;
	data->T38_address = 0;
}

static int mxt_parse_object_table(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int i;
	u8 reportid;
	u16 end_address;

	/* Valid Report IDs start counting from 1 */
	reportid = 1;
	data->mem_size = 0;
	for (i = 0; i < data->info->object_num; i++) {
		struct mxt_object *object = data->object_table + i;
		u8 min_id, max_id;

		le16_to_cpus(&object->start_address);

		if (object->num_report_ids) {
			min_id = reportid;
			reportid += object->num_report_ids *
					mxt_obj_instances(object);
			max_id = reportid - 1;
		} else {
			min_id = 0;
			max_id = 0;
		}

		dev_dbg(&data->client->dev,
			"T%-3u Start:%-4u Size:%-3u Instances:%u Report IDs:%u-%u\n",
			object->type, object->start_address,
			mxt_obj_size(object), mxt_obj_instances(object),
			min_id, max_id);

		switch (object->type) {
		case MXT_GEN_MESSAGE_T5:
			if (data->info->family_id == 0x80) {
				/* On mXT224 read and discard unused CRC byte
				 * otherwise DMA reads are misaligned */
				data->T5_msg_size = mxt_obj_size(object);
			} else {
				/* CRC not enabled, so skip last byte */
				data->T5_msg_size = mxt_obj_size(object) - 1;
			}
			data->T5_address = object->start_address;
		case MXT_GEN_COMMAND_T6:
			data->T6_reportid = min_id;
			data->T6_address = object->start_address;
			break;
		case MXT_GEN_POWER_T7:
			data->T7_address = object->start_address;
			break;
		case MXT_SPT_USERDATA_T38:
			data->T38_address = object->start_address;
			break;
		case MXT_TOUCH_MULTI_T9:
			/* Only handle messages from first T9 instance */
			data->T9_reportid_min = min_id;
			data->T9_reportid_max = min_id +
						object->num_report_ids - 1;
			data->num_touchids = object->num_report_ids;
			break;
		case MXT_TOUCH_KEYARRAY_T15:
			data->T15_reportid_min = min_id;
			data->T15_reportid_max = max_id;
			break;
		case MXT_SPT_COMMSCONFIG_T18:
			data->T18_address = object->start_address;
			break;
		case MXT_PROCI_TOUCHSUPPRESSION_T42:
			data->T42_reportid_min = min_id;
			data->T42_reportid_max = max_id;
			break;
		case MXT_SPT_MESSAGECOUNT_T44:
			data->T44_address = object->start_address;
			break;
		case MXT_SPT_GPIOPWM_T19:
			data->T19_reportid = min_id;
			break;
		case MXT_PROCG_NOISESUPPRESSION_T48:
			data->T48_reportid = min_id;
			break;
		case MXT_PROCI_ACTIVE_STYLUS_T63:
			/* Only handle messages from first T63 instance */
			data->T63_reportid_min = min_id;
			data->T63_reportid_max = min_id;
			data->num_stylusids = 1;
			break;
		case MXT_TOUCH_MULTITOUCHSCREEN_T100:
			data->T100_reportid_min = min_id;
			data->T100_reportid_max = max_id;
			/* first two report IDs reserved */
			data->num_touchids = object->num_report_ids - 2;
			break;
		}

		end_address = object->start_address
			+ mxt_obj_size(object) * mxt_obj_instances(object) - 1;

		if (end_address >= data->mem_size)
			data->mem_size = end_address + 1;
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;

	/* If T44 exists, T5 position has to be directly after */
	if (data->T44_address && (data->T5_address != data->T44_address + 1)) {
		dev_err(&client->dev, "Invalid T44 position\n");
		return -EINVAL;
	}

	data->msg_buf = kcalloc(data->max_reportid,
				data->T5_msg_size, GFP_KERNEL);
	if (!data->msg_buf) {
		dev_err(&client->dev, "Failed to allocate message buffer\n");
		return -ENOMEM;
	}

	return 0;
}

static u32 mxt_update_config_id(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	u8 userdata[4] = {0};
	u32 config_id = 0;
	int error;

	error = __mxt_read_reg(client, data->T38_address,
				sizeof(userdata), userdata);
	if (error)
		dev_warn(&client->dev, "Unable to read config id\n");
	else
		config_id = be32_to_cpu(*(unsigned int *)&userdata[0]);

	return config_id;
}

static int mxt_read_info_block(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	size_t size;
	void *buf;
	uint8_t num_objects;
	u32 calculated_crc;
	u8 *crc_ptr;
	u8 revision = 0;

	/* If info block already allocated, free it */
	if (data->raw_info_block != NULL)
		mxt_free_object_table(data);

	/* Read 7-byte ID information block starting at address 0 */
	size = sizeof(struct mxt_info);
	buf = kzalloc(size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	error = __mxt_read_reg(client, 0, size, buf);
	if (error) {
		dev_err(&client->dev, "error reading info block\n");
		goto err_free_mem;
	}

	/* Resize buffer to give space for rest of info block */
	num_objects = ((struct mxt_info *)buf)->object_num;
	size += (num_objects * sizeof(struct mxt_object))
		+ MXT_INFO_CHECKSUM_SIZE;

	buf = krealloc(buf, size, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Read rest of info block */
	error = __mxt_read_reg(client, MXT_OBJECT_START,
			       size - MXT_OBJECT_START,
			       buf + MXT_OBJECT_START);
	if (error) {
		dev_err(&client->dev, "error reading object table\n");
		goto err_free_mem;
	}

	/* Extract & calculate checksum */
	crc_ptr = buf + size - MXT_INFO_CHECKSUM_SIZE;
	data->info_crc = crc_ptr[0] | (crc_ptr[1] << 8) | (crc_ptr[2] << 16);

	calculated_crc = mxt_calculate_crc(buf, 0,
					   size - MXT_INFO_CHECKSUM_SIZE);

	/* CRC mismatch can be caused by data corruption due to I2C comms
	 * issue or else device is not using Object Based Protocol */
	if ((data->info_crc == 0) || (data->info_crc != calculated_crc)) {
		dev_err(&client->dev,
			"Info Block CRC error calculated=0x%06X read=0x%06X\n",
			data->info_crc, calculated_crc);
		return -EIO;
	}

	/* Save pointers in device data structure */
	data->raw_info_block = buf;
	data->info = (struct mxt_info *)buf;
	data->object_table = (struct mxt_object *)(buf + MXT_OBJECT_START);

	dev_info(&client->dev,
		 "Family: 0x%X Variant: 0x%X Firmware V%u.%u.%02X Objects: %u\n",
		 data->info->family_id, data->info->variant_id,
		 data->info->version >> 4, data->info->version & 0xf,
		 data->info->build, data->info->object_num);

	/* Parse object table information */
	error = mxt_parse_object_table(data);
	if (error) {
		dev_err(&client->dev, "Error %d reading object table\n", error);
		mxt_free_object_table(data);
		return error;
	}

	if (data->T38_address)
		data->config_id = mxt_update_config_id(data);

	error = mxt_read_revision_id(data, &revision);
	if (!error) {
		data->revision_id = revision;
		dev_info(&client->dev,
			"Revision ID: 0x%02x\n", data->revision_id);
	}

	return 0;

err_free_mem:
	kfree(buf);
	return error;
}

static int mxt_read_t9_resolution(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct t9_range range;
	unsigned char orient;
	struct mxt_object *object;

	object = mxt_get_object(data, MXT_TOUCH_MULTI_T9);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T9_RANGE,
			       sizeof(range), &range);
	if (error)
		return error;

	le16_to_cpus(range.x);
	le16_to_cpus(range.y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T9_ORIENT,
				1, &orient);
	if (error)
		return error;

	/* Handle default values */
	if (range.x == 0)
		range.x = 1023;

	if (range.y == 0)
		range.y = 1023;

	if (orient & MXT_T9_ORIENT_SWITCH) {
		data->max_x = range.y;
		data->max_y = range.x;
	} else {
		data->max_x = range.x;
		data->max_y = range.y;
	}

	dev_info(&client->dev,
		 "Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	return 0;
}

static void mxt_regulator_enable(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	gpio_set_value(data->pdata->gpio_reset, 0);

	error = regulator_enable(data->reg_vdd);
	if (error) {
		dev_err(dev, "Error %d enabling vdd regulator\n", error);
		return;
	}

	if (data->pdata->common_vdd_supply == 0) {
		error = regulator_enable(data->reg_avdd);
		if (error) {
			regulator_disable(data->reg_vdd);
			dev_err(dev, "Error %d enabling avdd regulator\n",
				error);
			return;
		}
	}

	msleep(MXT_WAKEUP_TIME);
	dev_dbg(&data->client->dev, "Regulator On\n");

	gpio_set_value(data->pdata->gpio_reset, 1);
	mxt_wait_for_idle(data);
}

static void mxt_regulator_disable(struct mxt_data *data)
{
	gpio_set_value(data->pdata->gpio_reset, 0);

	regulator_disable(data->reg_vdd);
	if (data->pdata->common_vdd_supply == 0)
		regulator_disable(data->reg_avdd);

	dev_dbg(&data->client->dev, "Regulator Off\n");
}

static int mxt_gpio_configure(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error = -EINVAL;

	/* According to maXTouch power sequencing specification, RESET line
	 * must be kept low until some time after regulators come up to
	 * voltage */
	if (gpio_is_valid(data->pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(data->pdata->gpio_reset, "mxt_reset_gpio");
		if (error) {
			dev_err(dev, "unable to request gpio [%u]\n",
				data->pdata->gpio_reset);
			goto fail;
		}

		error = gpio_direction_output(data->pdata->gpio_reset, 0);
		if (error) {
			dev_err(dev, "unable to set direction for gpio [%u]\n",
				data->pdata->gpio_reset);
			goto fail_release_reset_gpio;
		}
	} else {
		dev_err(dev, "reset gpio not provided\n");
		goto fail;
	}

	if (gpio_is_valid(data->pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(data->pdata->gpio_irq, "mxt_irq_gpio");
		if (error) {
			dev_err(dev, "unable to request gpio [%u]\n",
				data->pdata->gpio_irq);
			goto fail_release_reset_gpio;
		}
		error = gpio_direction_input(data->pdata->gpio_irq);
		if (error) {
			dev_err(dev, "unable to set direction for gpio [%u]\n",
				data->pdata->gpio_irq);
			goto fail_release_gpios;
		}
	} else {
		dev_err(dev, "irq gpio not provided\n");
		goto fail_release_reset_gpio;
	}

	return 0;

fail_release_gpios:
	gpio_free(data->pdata->gpio_irq);
fail_release_reset_gpio:
	gpio_free(data->pdata->gpio_reset);
fail:
	return error;
}

static void mxt_gpio_free(struct mxt_data *data)
{
	gpio_free(data->pdata->gpio_reset);
	gpio_free(data->pdata->gpio_irq);
}

static int mxt_power_init(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

#ifdef CONFIG_OF
	data->reg_vdd = regulator_get(&data->client->dev, "touch_vdd");
#else
	data->reg_vdd = regulator_get("vdd");
#endif
	if (IS_ERR(data->reg_vdd)) {
		error = PTR_ERR(data->reg_vdd);
		dev_err(dev, "Error %d getting vdd regulator\n", error);
		goto fail;
	}

	if (data->pdata->common_vdd_supply == 0) {
#ifdef CONFIG_OF
		data->reg_avdd = regulator_get(
					&data->client->dev, "touch_avdd");
#else
		data->reg_avdd = regulator_get("avdd");
#endif
		if (IS_ERR(data->reg_avdd)) {
			error = PTR_ERR(data->reg_avdd);
			dev_err(dev, "Error %d avdd regulator get\n", error);
			goto fail_release_vdd;
		}
	}

	return 0;

fail_release_vdd:
	regulator_put(data->reg_vdd);
fail:
	data->reg_vdd = NULL;
	data->reg_avdd = NULL;
	return error;
}

static int mxt_read_t100_config(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;
	struct mxt_object *object;
	u16 range_x, range_y;
	u8 cfg, tchaux;
	u8 aux;

	object = mxt_get_object(data, MXT_TOUCH_MULTITOUCHSCREEN_T100);
	if (!object)
		return -EINVAL;

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_XRANGE,
			       sizeof(range_x), &range_x);
	if (error)
		return error;

	le16_to_cpus(range_x);

	error = __mxt_read_reg(client,
			       object->start_address + MXT_T100_YRANGE,
			       sizeof(range_y), &range_y);
	if (error)
		return error;

	le16_to_cpus(range_y);

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_CFG1,
				1, &cfg);
	if (error)
		return error;

	error =  __mxt_read_reg(client,
				object->start_address + MXT_T100_TCHAUX,
				1, &tchaux);
	if (error)
		return error;

	/* Handle default values */
	if (range_x == 0)
		range_x = 1023;

	if (range_y == 0)
		range_y = 1023;

	if (cfg & MXT_T100_CFG_SWITCHXY) {
		data->max_x = range_y;
		data->max_y = range_x;
	} else {
		data->max_x = range_x;
		data->max_y = range_y;
	}

	/* allocate aux bytes */
	aux = 6;

	if (tchaux & MXT_T100_TCHAUX_VECT)
		data->t100_aux_vect = aux++;

	if (tchaux & MXT_T100_TCHAUX_AMPL)
		data->t100_aux_ampl = aux++;

	if (tchaux & MXT_T100_TCHAUX_AREA)
		data->t100_aux_area = aux++;

	dev_info(&client->dev,
		 "T100 Touchscreen size X%uY%u\n", data->max_x, data->max_y);

	/* T100 object size might change, thus free allocated buffer */
	kfree(data->T100_data);

	/* allocate memory to keep a copy of T100 */
	data->T100_data = kzalloc(mxt_obj_size(object), GFP_KERNEL);
	if (!data->T100_data)
		dev_warn(&client->dev, "Cannot allocate T100 data buffer\n");
	else {
		error = __mxt_read_reg(client, object->start_address,
				mxt_obj_size(object), data->T100_data);
		if (error) {
			dev_warn(&client->dev, "T100: failed to store data\n");
			kfree(data->T100_data);
			data->T100_data = NULL;
		} else
			dev_dbg(&client->dev, "T100: stored %d bytes\n",
				mxt_obj_size(object));
	}

	return 0;
}

static int mxt_initialize_t100_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct input_dev *input_dev;
	int i, error;

	error = mxt_read_t100_config(data);
	if (error)
		dev_warn(dev, "Failed to initialize T100 resolution\n");

	input_dev = data->input_dev;
#ifdef MXT_DISTINCT_INPUT_NAME
	input_dev->name = "Atmel maXTouch T100 Touchscreen";
#endif
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	/* For multi touch */
	input_mt_init_slots(input_dev, data->num_touchids);

	if (mxt_t47_stylus_state(data))
		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
					0, MT_TOOL_MAX, 0, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);

	if (data->t100_aux_area)
		input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				     0, MXT_MAX_AREA, 0, 0);

	if (data->t100_aux_ampl)
		input_set_abs_params(input_dev, ABS_MT_PRESSURE,
				     0, MXT_MAX_PRESSURE, 0, 0);

	if (data->t100_aux_vect)
		input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
				     0, 255, 0, 0);

	/* For T15 key array */
	if (data->T15_reportid_min) {
		data->t15_keystatus = 0;

		for (i = 0; i < data->pdata->t15_num_keys; i++)
			if (data->pdata->t15_keymap[i])
				input_set_capability(input_dev, EV_KEY,
					     data->pdata->t15_keymap[i]);
	}

	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data);
static int mxt_configure_objects(struct mxt_data *data);
static int mxt_apply_tdat_tsett(struct mxt_data *data);
static int mxt_alloc_input_device(struct mxt_data *data);

static void mxt_request_irq(struct mxt_data *data, unsigned long flags)
{
	int error;

	dev_dbg(&data->client->dev, "requesting IRQ, flags: %lu\n", flags);
	error = request_threaded_irq(data->irq, NULL, mxt_interrupt,
				     flags, data->client->name, data);
	/* no need to stay alive, touch is not functional */
	BUG_ON(error);
	data->irq_enabled = true;
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error, current_state = mxt_get_sensor_state(data);
	bool alt_bootloader_addr = false;
	bool retry = false;

retry_info:
	error = mxt_read_info_block(data);
	if (error) {
retry_bootloader:
		error = mxt_probe_bootloader(data, alt_bootloader_addr);
		if (error) {
			if (alt_bootloader_addr) {
				/* Chip is not in appmode or bootloader mode */
				return error;
			}

			dev_info(&client->dev, "Trying alternate bootloader address\n");
			alt_bootloader_addr = true;
			goto retry_bootloader;
		} else {
			if (retry) {
				dev_err(&client->dev,
						"Could not recover device from "
						"bootloader mode\n");
				/* this is not an error state, we can reflash
				 * from here */
				mxt_set_sensor_state(data, STATE_BL);
				return 0;
			}

			/* Attempt to exit bootloader into app mode */
			mxt_send_bootloader_cmd(data, false);
			msleep(MXT_FW_RESET_TIME);
			retry = true;
			goto retry_info;
		}
	}

	error = mxt_check_retrigen(data);
	if (error)
		return error;

	mxt_acquire_irq(data);

	error = mxt_configure_objects(data);
	if (error)
		return error;

	/* Touch IC is in UI mode, re-register to level */
	/* triggered IRQ mode per Atmel specification   */
	free_irq(data->irq, data);
	mxt_request_irq(data, data->pdata->irqflags);

	mxt_set_sensor_state(data, (current_state == STATE_UNKNOWN) ?
					STATE_STANDBY : STATE_ACTIVE);
	return 0;
}

static int mxt_configure_objects(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	int error;

	if (!data->input_dev) {
		error = mxt_alloc_input_device(data);
		if (error) {
			dev_err(&client->dev,
				"Failed to allocate input device\n");
			return error;
		}
	}

	error = mxt_debug_msg_init(data);
	if (error)
		return error;

	error = mxt_init_t7_power_cfg(data);
	if (error) {
		dev_err(&client->dev, "Failed to initialize power cfg\n");
		return error;
	}

	/* Check register init values */
	if (data->tdat)
		error = mxt_apply_tdat_tsett(data);
	else
		error = mxt_check_reg_init(data);
	if (error) {
		dev_err(&client->dev, "Error %d initializing configuration\n",
			error);
		return error;
	}

	if (data->T9_reportid_min) {
		error = mxt_initialize_t9_input_device(data);
		if (error)
			return error;
	} else if (data->T100_reportid_min) {
		error = mxt_initialize_t100_input_device(data);
		if (error)
			return error;
	} else
		dev_warn(&client->dev, "No touch object detected\n");

	if (!data->in_bootloader && !data->input_registered) {
		error = input_register_device(data->input_dev);
		if (error) {
			dev_err(&client->dev,
				"Error %d registering input device\n", error);
			return error;
		}
		data->input_registered = true;
	}

	return 0;
}

/* Firmware Version is returned as Major.Minor.Build */
static ssize_t mxt_fw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u.%u.%02X\n",
			 data->info->version >> 4, data->info->version & 0xf,
			 data->info->build);
}

/* Hardware Version is returned as FamilyID.VariantID */
static ssize_t mxt_hw_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%u.%u\n",
			data->info->family_id, data->info->variant_id);
}

/* Build Id is returned as FamilyID-ConfigID */
static ssize_t mxt_buildid_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_info *info = data->info ? data->info : &data->pdata->dt_info;
	return scnprintf(buf, PAGE_SIZE, "%02x%02x%02x-%08x\n",
			info->version >> 4,
			info->version & 0xf,
			info->build,
			data->config_id);
}

/* Product Id returned as FamilyID */
static ssize_t mxt_productinfo_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_info *info = data->info ? data->info : &data->pdata->dt_info;
	return scnprintf(buf, PAGE_SIZE, "%02x%02x\n",
				info->family_id, data->revision_id);
}

static ssize_t mxt_poweron_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", data->poweron);
}

static ssize_t mxt_ic_ver_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_info *info = data->info ? data->info : &data->pdata->dt_info;
	return scnprintf(buf, PAGE_SIZE,
			"%s%02x%02x(%02x)\n%s%02x%02x%02x\n%s%08x\n",
			"Product ID: ",
			info->family_id,
			data->revision_id,
			info->variant_id,
			"Build ID: ",
			info->version >> 4,
			info->version & 0xf,
			info->build,
			"Config ID: ",
			data->config_id);
}

static ssize_t mxt_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			data->irq_enabled ? "ENABLED" : "DISABLED");
}

static ssize_t mxt_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	unsigned long value = 0;
	int err = 0;

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		dev_err(dev, "%s: Failed to convert value.\n", __func__);
		return -EINVAL;
	}

	switch (value) {
	case 0: /* Disable irq */
		mxt_irq_enable(data, false);
		data->enable_reporting = false;
		break;
	case 1: /* Enable irq */
		mxt_irq_enable(data, true);
		data->enable_reporting = true;
		break;
	default:
		dev_err(dev, "%s: Invalid value\n", __func__);
		return -EINVAL;
	}
	return count;
}

static ssize_t mxt_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	switch (gpio_get_value(data->pdata->gpio_irq)) {
	case 0:
		return scnprintf(buf, PAGE_SIZE, "Low\n");
	case 1:
		return scnprintf(buf, PAGE_SIZE, "High\n");
	default:
		dev_err(dev, "%s: Failed to get GPIO for irq %d.\n",
				__func__,
				data->irq);
		return scnprintf(buf, PAGE_SIZE, "Unknown\n");
	}
}

static void mxt_hw_reset(struct mxt_data *data)
{
	gpio_set_value(data->pdata->gpio_reset, 0);
	udelay(1500);
	gpio_set_value(data->pdata->gpio_reset, 1);

	mxt_wait_for_idle(data);
	mxt_acquire_irq(data);
}

static ssize_t mxt_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int reset;
	struct mxt_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	if (data->suspended)
		mxt_resume(&data->client->dev);
	else {
		data->enable_reporting = false;
		mxt_hw_reset(data);
		data->enable_reporting = true;
	}

	return count;
}

static ssize_t mxt_show_instance(char *buf, int count,
				 struct mxt_object *object, int instance,
				 const u8 *val)
{
	int i;

	if (mxt_obj_instances(object) > 1)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				   "Instance %u\n", instance);

	for (i = 0; i < mxt_obj_size(object); i++)
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\t[%2u]: %02x (%d)\n", i, val[i], val[i]);
	count += scnprintf(buf + count, PAGE_SIZE - count, "\n");

	return count;
}

static ssize_t mxt_object_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	struct mxt_object *object;
	int count = 0;
	int i, j;
	int error;
	u8 *obuf;

	/* Pre-allocate buffer large enough to hold max sized object. */
	obuf = kmalloc(256, GFP_KERNEL);
	if (!obuf)
		return -ENOMEM;

	error = 0;
	for (i = 0; i < data->info->object_num; i++) {
		object = data->object_table + i;

		if (!mxt_object_readable(object->type))
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
				"T%u:\n", object->type);

		for (j = 0; j < mxt_obj_instances(object); j++) {
			u16 size = mxt_obj_size(object);
			u16 addr = object->start_address + j * size;

			error = __mxt_read_reg(data->client, addr, size, obuf);
			if (error)
				goto done;

			count = mxt_show_instance(buf, count, object, j, obuf);
		}
	}

done:
	kfree(obuf);
	return error ?: count;
}

static int mxt_check_firmware_format(struct device *dev,
				     const struct firmware *fw)
{
	unsigned int pos = 0;
	char c;

	while (pos < fw->size) {
		c = *(fw->data + pos);

		if (c < '0' || (c > '9' && c < 'A') || c > 'F')
			return 0;

		pos++;
	}

	/* To convert file try
	 * xxd -r -p mXTXXX__APP_VX-X-XX.enc > maxtouch.fw */
	dev_err(dev, "Aborting: firmware file must be in binary format\n");

	return -EINVAL;
}

static int mxt_load_fw(struct device *dev)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int ret;

	ret = request_firmware(&fw, data->fw_name, dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", data->fw_name);
		return ret;
	}

	/* Check for incorrect enc file */
	ret = mxt_check_firmware_format(dev, fw);
	if (ret)
		goto release_firmware;

	if (data->suspended)
		mxt_resume(&data->client->dev);

	if (!data->in_bootloader) {
		mxt_irq_enable(data, false);

		/* Change to the bootloader mode */
		ret = mxt_t6_command(data, MXT_COMMAND_RESET,
				     MXT_BOOT_VALUE, false);
		if (ret)
			goto release_firmware;

		msleep(MXT_RESET_TIME);

		/* At this stage, do not need to scan since we know
		 * family ID */
		ret = mxt_lookup_bootloader_address(data, 0);
		if (ret)
			goto release_firmware;

		mxt_irq_enable(data, true);
	}

	mxt_set_sensor_state(data, STATE_INIT);
	mxt_free_object_table(data);

	ret = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (ret) {
		/* Bootloader may still be unlocked from previous update
		 * attempt */
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, false);
		if (ret)
			goto release_firmware;
	} else {
		dev_info(dev, "Unlocking bootloader\n");
		/* Unlock bootloader */
		ret = mxt_send_bootloader_cmd(data, true);
		if (ret)
			goto release_firmware;
	}

	mxt_set_sensor_state(data, STATE_FLASH);
	while (pos < fw->size) {
		ret = mxt_check_bootloader(data, MXT_WAITING_FRAME_DATA, true);
		if (ret)
			goto release_firmware;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		ret = mxt_bootloader_write(data, fw->data + pos, frame_size);
		if (ret)
			goto release_firmware;

		ret = mxt_check_bootloader(data, MXT_FRAME_CRC_PASS, true);
		if (ret) {
			retry++;

			/* Back off by 20ms per retry */
			msleep(retry * 20);

			if (retry > 20) {
				dev_err(dev, "Retry count exceeded\n");
				goto release_firmware;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}

		if (frame % 50 == 0)
			dev_info(dev, "Sent %d frames, %d/%zd bytes\n",
				frame, pos, fw->size);
	}

	dev_info(dev, "Sent %d frames, %zd bytes\n", frame, pos);
	INIT_COMPLETION(data->bl_completion);
	/* Wait for flash. */
	ret = mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);
	if (ret)
		goto release_firmware;

	/* Wait for device to reset. Some bootloader versions do not assert
	 * the CHG line after bootloading has finished, so ignore error */
	mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);

release_firmware:
	release_firmware(fw);
	return ret;
}

static int mxt_update_file_name(struct device *dev, char **file_name,
				const char *buf, size_t count)
{
	char *file_name_tmp;

	/* Simple sanity check */
	if (count > 64) {
		dev_warn(dev, "File name too long\n");
		return -EINVAL;
	}

	file_name_tmp = krealloc(*file_name, count + 1, GFP_KERNEL);
	if (!file_name_tmp) {
		dev_warn(dev, "no memory\n");
		return -ENOMEM;
	}

	*file_name = file_name_tmp;
	memcpy(*file_name, buf, count);

	/* Echo into the sysfs entry may append newline at the end of buf */
	if (buf[count - 1] == '\n')
		(*file_name)[count - 1] = '\0';
	else
		(*file_name)[count] = '\0';

	return 0;
}

static ssize_t mxt_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int error;

	error = mxt_update_file_name(dev, &data->fw_name, buf, count);
	if (error)
		return error;

	error = mxt_load_fw(dev);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		mxt_set_sensor_state(data, STATE_QUERY);

		error = mxt_initialize(data);
		if (error)
			return error;
	}

	return count;
}

static ssize_t mxt_update_cfg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret;

	if (data->in_bootloader) {
		dev_err(dev, "Not in appmode\n");
		return -EINVAL;
	}

	ret = mxt_update_file_name(dev, &data->cfg_name, buf, count);
	if (ret)
		return ret;

	data->enable_reporting = false;
	mxt_free_input_device(data);

	if (data->suspended)
		mxt_resume(&data->client->dev);

	ret = mxt_configure_objects(data);
	if (ret)
		goto out;

	ret = count;
out:
	return ret;
}

static ssize_t mxt_debug_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	char c;

	c = data->debug_enabled ? '1' : '0';
	return scnprintf(buf, PAGE_SIZE, "%c\n", c);
}

static ssize_t mxt_debug_notify_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t mxt_debug_v2_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		if (i == 1)
			mxt_debug_msg_enable(data);
		else
			mxt_debug_msg_disable(data);

		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static ssize_t mxt_debug_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	int i;

	if (sscanf(buf, "%u", &i) == 1 && i < 2) {
		data->debug_enabled = (i == 1);

		dev_dbg(dev, "%s\n", i ? "debug enabled" : "debug disabled");
		return count;
	} else {
		dev_dbg(dev, "debug_enabled write error\n");
		return -EINVAL;
	}
}

static int mxt_check_mem_access_params(struct mxt_data *data, loff_t off,
				       size_t *count)
{
	if (off >= data->mem_size)
		return -EIO;

	if (off + *count > data->mem_size)
		*count = data->mem_size - off;

	if (*count > MXT_MAX_BLOCK_WRITE)
		*count = MXT_MAX_BLOCK_WRITE;

	return 0;
}

static ssize_t mxt_mem_access_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_read_reg(data->client, off, count, buf);

	return ret == 0 ? count : ret;
}

static ssize_t mxt_mem_access_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off,
	size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct mxt_data *data = dev_get_drvdata(dev);
	int ret = 0;

	ret = mxt_check_mem_access_params(data, off, &count);
	if (ret < 0)
		return ret;

	if (count > 0)
		ret = __mxt_write_reg(data->client, off, count, buf);

	return ret == 0 ? count : 0;
}

static int mxt_apply_tdat_tsett(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_object *object;
	int ret = 0;
	int offset = 0;
	int byte_offset;
	int cfg_start_ofs;
	u8 *config_mem;
	size_t config_mem_size;
	const unsigned char *cfg_data, *obj_data;
	unsigned int type, instance, size;
	u16 reg;

	pr_info("Applying TDAT settings\n");

	/* Alloc memory to store configuration */
	cfg_start_ofs = MXT_OBJECT_START
			+ data->info->object_num * sizeof(struct mxt_object)
			+ MXT_INFO_CHECKSUM_SIZE;
	config_mem_size = data->mem_size - cfg_start_ofs;
	config_mem = kzalloc(config_mem_size, GFP_KERNEL);
	if (!config_mem) {
		dev_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto just_leave;
	}
	dev_dbg(dev, "Config memory: size %u, start_ofs %u\n",
					config_mem_size, cfg_start_ofs);
	cfg_data = data->tsett.data;
	while (offset < data->tsett.size) {
		/* Read type, instance, length */
		type = (cfg_data[offset+1] << 8) | cfg_data[offset];
		instance = cfg_data[offset+2];
		size = (cfg_data[offset+4] << 8) | cfg_data[offset+3];
		obj_data = &cfg_data[offset+5];

		dev_dbg(dev, "T%u, instance: %u, size: %u, Tsett-offset: %d\n",
			type, instance, size, offset);
		offset += size + 5;

		object = mxt_get_object(data, type);
		if (!object)
			continue;

		if (size > mxt_obj_size(object)) {
			/* Either we are in fallback mode due to wrong
			 * config or config from a later fw version,
			 * or the file is corrupt or hand-edited */
			dev_warn(dev, "Discarding %u byte(s) in T%u\n",
				 size - mxt_obj_size(object), type);
		} else if (mxt_obj_size(object) > size) {
			/* If firmware is upgraded, new bytes may be added to
			 * end of objects. It is generally forward compatible
			 * to zero these bytes - previous behaviour will be
			 * retained. However this does invalidate the CRC and
			 * will force fallback mode until the configuration is
			 * updated. We warn here but do nothing else - the
			 * malloc has zeroed the entire configuration. */
			dev_warn(dev, "Zeroing %u byte(s) in T%d\n",
				 mxt_obj_size(object) - size, type);
		}

		if (instance >= mxt_obj_instances(object)) {
			dev_err(dev, "Object instances exceeded!\n");
			ret = -EINVAL;
			goto release_mem;
		}

		/* config data address in registers memory map */
		reg = object->start_address + mxt_obj_size(object) * instance;
		dev_dbg(dev, "T%u address: %u, instance[%u] data offset %u\n",
				type, object->start_address,
				instance, mxt_obj_size(object) * instance);

		/* copy new config data to config memory buffer */
		memcpy(config_mem + reg - cfg_start_ofs, obj_data, size);
		dev_dbg(dev, "Copied %u bytes at offset %u\n",
			size, (unsigned)(reg - cfg_start_ofs));
	}

	/* calculate crc of the received configs (not the raw config file) */
	if (data->T7_address < cfg_start_ofs) {
		dev_err(dev, "Bad T7 address, T7addr = %x, config offset %x\n",
			data->T7_address, cfg_start_ofs);
		goto release_mem;
	}

	/* Write configuration as blocks */
	byte_offset = 0;
	while (byte_offset < config_mem_size) {
		size = config_mem_size - byte_offset;

		if (size > MXT_MAX_BLOCK_WRITE)
			size = MXT_MAX_BLOCK_WRITE;

		dev_dbg(dev, "Writing config chunk size %u, at address %u\n",
			size, cfg_start_ofs + byte_offset);

		ret = __mxt_write_reg(data->client,
				      cfg_start_ofs + byte_offset,
				      size, config_mem + byte_offset);
		if (ret != 0) {
			dev_err(dev, "Config write error, ret=%d\n", ret);
			goto release_mem;
		}

		byte_offset += size;
	}

	mxt_update_crc(data, MXT_COMMAND_BACKUPNV, MXT_BACKUP_VALUE);

	ret = mxt_check_retrigen(data);
	if (ret)
		goto release_mem;

	mxt_soft_reset(data);
	dev_info(dev, "Config written\n");

	/* T7 config may have changed */
	mxt_init_t7_power_cfg(data);

	/* T38 may have changed */
	if (data->T38_address)
		data->config_id = mxt_update_config_id(data);

release_mem:
	kfree(config_mem);
just_leave:
	return ret;
}

static bool forcereflash;
static ssize_t mxt_forcereflash_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	forcereflash = true;

	return count;
}

static bool mxt_check_tdat_format(const unsigned char *image)
{
	return image[0] == 0x31;
}

static int mxt_get_tdat_config_id(const unsigned char *data, size_t size,
	u32 *config_id, u8 *revision)
{
	unsigned short id;
	int error = -EINVAL;
	size_t length, offset;

	for (offset = 0; offset < size; offset += length+5) {
		id = (data[offset+1] << 8) | data[offset];
		length = (data[offset+4] << 8) | data[offset+3];
		if (id == MXT_SPT_USERDATA_T38) {
			*config_id = be32_to_cpu(
					*(unsigned int *)&data[offset+5]);
			*revision = data[offset+9];
			error = 0;
			break;
		}
	}
	return error;
}

static int mxt_parse_tdat_image(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	u32 firmware_id;
	u8 *section, id;
	size_t ii, length, offset, header_sz, raw_size = data->tdat->size;
	const u8 *raw_image = data->tdat->data;
	int error;
	u32 config_id = 0;
	u8 revision_id = 0;

	pr_info("Start TDAT image processing\n");

	for (ii = 0, offset = 1; offset < raw_size; offset += length+4) {
		length = (raw_image[offset+3] << 16) |
			 (raw_image[offset+2] << 8) | raw_image[offset+1];

		dev_dbg(dev, "Record[%d]: length %u, offset %u\n",
				ii++, length, offset);

		if ((offset+length+4) > raw_size) {
			dev_err(dev, "Data overflow at offset %u (%u)\n",
				offset, raw_image[offset]);
			return -EINVAL;
		}
	}

	if (offset != raw_size) {
		dev_err(dev, "Data is misaligned\n");
		return -EINVAL;
	}

	for (offset = 1; offset < raw_size; offset += length+4) {
		/* byte 0	- section id     */
		/* byte 1-3	- section length */
		/* byte 4-...	- section data   */
		id = raw_image[offset];
		length = (raw_image[offset+3] << 16) |
			 (raw_image[offset+2] << 8) | raw_image[offset+1];
		section = (u8 *)&raw_image[offset+4];
		dev_dbg(dev, "Section [%u], size %u\n", id, length);

		switch (id) {
		case 1: /* config */
			error = mxt_get_tdat_config_id(section, length,
						&config_id, &revision_id);
			if (error)
				return -EINVAL;

			pr_info("TDAT: Config ID %x, revision 0x%02x\n",
						config_id, revision_id);
			data->tsett.data = section;
			data->tsett.size = length;
			break;

		case 2: /* firmware */
			firmware_id = be32_to_cpu(
						*(unsigned int *)(&section[1]));
			pr_info("TDAT: FW build ID %x\n", firmware_id);

			/* skip section header */
			header_sz = section[0] + 1;
			data->fw.data = section + header_sz;
			data->fw.size = length - header_sz;
			break;
		default:
			break;
		}
	}

	if (revision_id != data->revision_id) {
		dev_err(dev, "Incorrect firmware\n");
		return -EINVAL;
	}

	if ((config_id <= data->config_id) && !forcereflash) {
		dev_err(dev, "Firmware upgrade is not required\n");
		return -EINVAL;
	}

	data->config_id = config_id;
	data->firmware_id = firmware_id;
	data->revision_id = revision_id;

	return 0;
}

static ssize_t mxt_doreflash_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	unsigned int frame_size;
	unsigned int pos = 0;
	unsigned int retry = 0;
	unsigned int frame = 0;
	int error;

	error = mxt_update_file_name(dev, &data->fw_name, buf, count);
	if (error)
		return error;

	pr_info("TDAT file name: %s\n", data->fw_name);

	error = request_firmware(&data->tdat, data->fw_name, dev);
	if (error) {
		dev_err(dev, "Unable to load firmware %s\n", data->fw_name);
		return error;
	}

	if (mxt_check_tdat_format(data->tdat->data)) {
		error = mxt_parse_tdat_image(data);
		if (error)
			goto release_firmware;
		forcereflash = false;
	} else {
		dev_err(dev, "invalid TDAT format\n");
		goto release_firmware;
	}

	if (data->suspended)
		mxt_resume(&data->client->dev);

	mxt_irq_enable(data, false);

	if (!data->in_bootloader) {
		dev_dbg(dev, "Reboot to bootloader\n\n");
		/* Change to the bootloader mode */
		error = mxt_t6_command(data, MXT_COMMAND_RESET,
				     MXT_BOOT_VALUE, false);
		if (error)
			goto release_firmware;

		msleep(MXT_RESET_TIME);

		/* At this stage, no need to scan since family ID is known */
		error = mxt_lookup_bootloader_address(data, 0);
		if (error)
			goto release_firmware;

		/* Level triggered IRQ causes WD reset due to soft IRQ   */
		/* lockup, thus change to edge for the duration of flash */
		free_irq(data->irq, data);
		mxt_request_irq(data, IRQF_TRIGGER_FALLING | IRQF_ONESHOT);
	} else
		mxt_irq_enable(data, true);

	mxt_set_sensor_state(data, STATE_INIT);

	mutex_lock(&data->crit_section_lock);
	dev_dbg(dev, "critical section LOCK\n");

	mxt_free_object_table(data);

	dev_dbg(dev, "confirming bootloader mode\n");
	error = mxt_check_bootloader(data, MXT_WAITING_BOOTLOAD_CMD, false);
	if (error) {
		error = mxt_check_bootloader(data,
					MXT_WAITING_FRAME_DATA, false);
		if (error)
			goto flash_error;
	} else {
		dev_info(dev, "Unlocking bootloader\n");
		/* Unlock bootloader */
		error = mxt_send_bootloader_cmd(data, true);
		if (error)
			goto flash_error;
	}

	dev_dbg(dev, "start flashing frames\n");
	mxt_set_sensor_state(data, STATE_FLASH);
	while (pos < data->fw.size) {
		error = mxt_check_bootloader(data,
					MXT_WAITING_FRAME_DATA, true);
		if (error)
			goto flash_error;

		frame_size = ((*(data->fw.data + pos) << 8) |
				*(data->fw.data + pos + 1));
		/* Take account of CRC bytes */
		frame_size += 2;

		/* Write one frame to device */
		error = mxt_bootloader_write(data,
					data->fw.data + pos, frame_size);
		if (error)
			goto flash_error;

		error = mxt_check_bootloader(data,
					MXT_FRAME_CRC_PASS, true);
		if (error) {
			retry++;
			msleep(retry * 20);
			if (retry > 20) {
				dev_err(dev, "Retry count exceeded\n");
				goto flash_error;
			}
		} else {
			retry = 0;
			pos += frame_size;
			frame++;
		}

		if (frame % 50 == 0)
			dev_info(dev, "Sent %d frames, %u/%u bytes\n",
				frame, pos, data->fw.size);
	}

	dev_info(dev, "FW: sent %d frames, %u bytes\n", frame, pos);
	INIT_COMPLETION(data->bl_completion);
	error = mxt_wait_for_completion(data, &data->bl_completion,
				MXT_FW_RESET_TIME);
	if (error)
		goto flash_error;

	dev_info(dev, "The firmware update succeeded\n");
	goto initialize;

flash_error:
	mxt_soft_reset(data);

initialize:
	mxt_set_sensor_state(data, STATE_QUERY);
	error = mxt_initialize(data);
	if (error)
		dev_info(dev, "Init failed after firmware upgrade\n");

	mutex_unlock(&data->crit_section_lock);
	dev_dbg(dev, "critical section RELEASE\n");

	memset(&data->fw, 0, sizeof(data->fw));
	memset(&data->tsett, 0, sizeof(data->tsett));

release_firmware:
	release_firmware(data->tdat);
	data->tdat = NULL;
	return count;
}

static ssize_t mxt_flashprog_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct mxt_data *data = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "%d", data->in_bootloader);
}

static DEVICE_ATTR(fw_version, S_IRUGO, mxt_fw_version_show, NULL);
static DEVICE_ATTR(hw_version, S_IRUGO, mxt_hw_version_show, NULL);
static DEVICE_ATTR(buildid, S_IRUGO, mxt_buildid_show, NULL);
static DEVICE_ATTR(productinfo, S_IRUGO, mxt_productinfo_show, NULL);
static DEVICE_ATTR(object, S_IRUGO, mxt_object_show, NULL);
static DEVICE_ATTR(poweron, S_IRUGO, mxt_poweron_show, NULL);
static DEVICE_ATTR(ic_ver, S_IRUGO, mxt_ic_ver_show, NULL);
static DEVICE_ATTR(flashprog, S_IRUGO, mxt_flashprog_show, NULL);
static DEVICE_ATTR(update_fw, S_IWUSR, NULL, mxt_update_fw_store);
static DEVICE_ATTR(update_cfg, S_IWUSR, NULL, mxt_update_cfg_store);
static DEVICE_ATTR(doreflash, S_IWUSR, NULL, mxt_doreflash_store);
static DEVICE_ATTR(forcereflash, S_IWUSR, NULL, mxt_forcereflash_store);
static DEVICE_ATTR(hw_irqstat, S_IRUGO, mxt_hw_irqstat_show, NULL);
static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, mxt_reset_store);
static DEVICE_ATTR(drv_irq, S_IRUGO | S_IWUSR | S_IWGRP,
				mxt_drv_irq_show, mxt_drv_irq_store);
static DEVICE_ATTR(debug_v2_enable, S_IWUSR | S_IRUSR, NULL,
				mxt_debug_v2_enable_store);
static DEVICE_ATTR(debug_notify, S_IRUGO, mxt_debug_notify_show, NULL);
static DEVICE_ATTR(debug_enable, S_IWUSR | S_IRUSR, mxt_debug_enable_show,
				mxt_debug_enable_store);

static struct attribute *mxt_attrs[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_hw_version.attr,
	&dev_attr_object.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_update_cfg.attr,
	&dev_attr_ic_ver.attr,
	&dev_attr_doreflash.attr,
	&dev_attr_flashprog.attr,
	&dev_attr_buildid.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_poweron.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_drv_irq.attr,
	&dev_attr_hw_irqstat.attr,
	&dev_attr_reset.attr,
	&dev_attr_debug_enable.attr,
	&dev_attr_debug_v2_enable.attr,
	&dev_attr_debug_notify.attr,
	NULL
};

static const struct attribute_group mxt_attr_group = {
	.attrs = mxt_attrs,
};

static void mxt_reset_slots(struct mxt_data *data)
{
	struct input_dev *input_dev = data->input_dev;
	unsigned int num_mt_slots;
	int id;

	if (data->buttons_enabled)
		data->t15_keystatus = 0;

	if (!input_dev)
		return;

	num_mt_slots = data->num_touchids + data->num_stylusids;

	for (id = 0; id < num_mt_slots; id++) {
		input_mt_slot(input_dev, id);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}

	mxt_input_sync(input_dev);
}

#ifndef CONFIG_FB
static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	mxt_irq_enable(data, true);

	if (data->use_regulator) {
		mxt_regulator_enable(data);
		mxt_acquire_irq(data);
	} else if (data->sensor_sleep) {
		mxt_sensor_wake(data, true);
		mxt_hw_reset(hw);
	}

	mutex_unlock(&data->crit_section_lock);
	dev_dbg(&data->client->dev, "critical section RELEASE\n");

	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);

	mxt_reset_slots(data);
	mxt_irq_enable(data, false);

	if (data->use_regulator)
		mxt_regulator_disable(data);
	else if (!data->sensor_sleep)
		mxt_sensor_sleep(data);

	mutex_lock(&data->crit_section_lock);
	dev_dbg(&data->client->dev, "critical section LOCK\n");
}
#endif

#ifdef CONFIG_OF
static int mxt_parse_dt(struct mxt_data *data)
{
	unsigned resolution[2], key_codes[MXT_MAX_BUTTONS];
	struct device *dev = &data->client->dev;
	struct mxt_platform_data *pdata = data->pdata;
	int error = 0;
	u32 value;
	struct device_node *temp, *np = dev->of_node;

	pdata->common_vdd_supply = of_property_read_bool(np,
					"atmel,common-vdd-supply");
	data->use_regulator = !of_property_read_bool(np,
					"atmel,suspend-method-lpm");
	pr_info("using suspend method: %s\n", data->use_regulator ?
					"reset" : "lpm");
	data->one_touch_enabled = of_property_read_bool(np,
					"atmel,one-touch-enabled");
	if (data->one_touch_enabled)
		pr_info("using single touch in suspend\n");

	error = of_property_read_u32_array(np, "atmel,panel-resolution",
					resolution, 2);
	if (!error) {
		pdata->res.x_max = resolution[0];
		pdata->res.y_max = resolution[1];
		pr_debug("panel resolution X%d,Y%d\n",
					pdata->res.x_max, pdata->res.y_max);
	}

	/* reset, irq gpio info */
	pdata->gpio_irq = of_get_gpio(np, 0);
	pdata->gpio_reset = of_get_gpio(np, 1);

	/* defaults in case DTS is not populated */
	pdata->dt_info.family_id = 0xa4;
	pdata->dt_info.variant_id = 0x2;
	pdata->dt_info.version = 0x5;
	pdata->dt_info.build = 0x2;

	memset(key_codes, 0, sizeof(key_codes));
	error = of_property_read_u32_array(np, "atmel,key-buttons",
			 key_codes, MXT_MAX_BUTTONS);
	if (!error) {
		int i, keys;

		pdata->t15_num_keys = MXT_MAX_BUTTONS;
		pdata->t15_keymap = devm_kzalloc(dev,
			sizeof(unsigned) * pdata->t15_num_keys, GFP_KERNEL);
		if (!pdata->t15_keymap) {
			dev_err(dev, "T15 keymap allocation failure\n");
			goto exit_parser;
		}

		for (i = 0, keys = 0; i < pdata->t15_num_keys; i++)
			if (key_codes[i]) {
				*(pdata->t15_keymap + i) = key_codes[i];
				keys++;
			}

		data->buttons_enabled = true;
		pr_info("T15 has %d buttons\n", keys);
	}

	for_each_child_of_node(np, temp) {
		error = of_property_read_u32(temp, "atmel,family-id", &value);
		if (error) {
			dev_err(dev, "Unable to read family id\n");
			goto exit_parser;
		}
		pdata->dt_info.family_id = (u8)value;

		error = of_property_read_u32(temp, "atmel,variant-id", &value);
		if (error) {
			dev_err(dev, "Unable to read variant id\n");
			goto exit_parser;
		}
		pdata->dt_info.variant_id = (u8)value;

		error = of_property_read_u32(temp, "atmel,version", &value);
		if (error) {
			dev_err(dev, "Unable to read controller version\n");
			goto exit_parser;
		}
		pdata->dt_info.version = (u8)value;

		error = of_property_read_u32(temp, "atmel,build", &value);
		if (error) {
			dev_err(dev, "Unable to read build id\n");
			goto exit_parser;
		}
		pdata->dt_info.build = (u8)value;

		error = of_property_read_u32(temp, "atmel,revision-id", &value);
		if (error) {
			dev_err(dev, "Unable to read revision id\n");
			goto exit_parser;
		}
		data->revision_id = (u8)value;
	}

exit_parser:
	if (!error)
		dev_dbg(dev, "DT parsed successfully\n");
	return 0;
}
#else
static int mxt_parse_dt(struct device *dev, struct mxt_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int mxt_handle_pdata(struct mxt_data *data)
{
	int error;
	data->pdata = dev_get_platdata(&data->client->dev);

	/* Use provided platform data if present */
	if (data->pdata) {
		if (data->pdata->cfg_name)
			mxt_update_file_name(&data->client->dev,
					     &data->cfg_name,
					     data->pdata->cfg_name,
					     strlen(data->pdata->cfg_name));
		return 0;
	}

	data->pdata = kzalloc(sizeof(*data->pdata), GFP_KERNEL);
	if (!data->pdata) {
		dev_err(&data->client->dev, "Failed to allocate pdata\n");
		return -ENOMEM;
	}

	/* Set default parameters */
	data->pdata->irqflags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;

	if (data->client->dev.of_node) {
		error = mxt_parse_dt(data);
		if (error)
			return error;
	}

	return 0;
}

static int mxt_initialize_t9_input_device(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	const struct mxt_platform_data *pdata = data->pdata;
	struct input_dev *input_dev;
	int error;
	unsigned int num_mt_slots;
	int i;

	error = mxt_read_t9_resolution(data);
	if (error)
		dev_warn(dev, "Failed to initialize T9 resolution\n");

	input_dev = data->input_dev;
#ifdef MXT_DISTINCT_INPUT_NAME
	input_dev->name = "Atmel maXTouch T9 Touchscreen";
#endif
	if (pdata->t19_num_keys) {
		__set_bit(INPUT_PROP_BUTTONPAD, input_dev->propbit);

		for (i = 0; i < pdata->t19_num_keys; i++)
			if (pdata->t19_keymap[i] != KEY_RESERVED)
				input_set_capability(input_dev, EV_KEY,
						     pdata->t19_keymap[i]);

		input_abs_set_res(input_dev, ABS_X, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_Y, MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_X,
				  MXT_PIXELS_PER_MM);
		input_abs_set_res(input_dev, ABS_MT_POSITION_Y,
				  MXT_PIXELS_PER_MM);
#ifdef MXT_DISTINCT_INPUT_NAME
		input_dev->name = "Atmel maXTouch Touchpad";
#endif
	} else
		set_bit(INPUT_PROP_DIRECT, input_dev->propbit);


	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE,
			     0, 255, 0, 0);

	/* For multi touch */
	num_mt_slots = data->num_touchids + data->num_stylusids;
	input_mt_init_slots(input_dev, num_mt_slots);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, MXT_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, data->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, data->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
			     0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_ORIENTATION,
			     0, 255, 0, 0);

	/* For T63 active stylus */
	if (data->T63_reportid_min) {
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS);
		input_set_capability(input_dev, EV_KEY, BTN_STYLUS2);
		input_set_abs_params(input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
	}

	/* For T15 key array */
	if (data->T15_reportid_min) {
		data->t15_keystatus = 0;

		for (i = 0; i < data->pdata->t15_num_keys; i++)
			input_set_capability(input_dev, EV_KEY,
					     data->pdata->t15_keymap[i]);
	}

	return 0;
}

static int mxt_alloc_input_device(struct mxt_data *data)
{
	data->input_dev = input_allocate_device();
	if (IS_ERR_OR_NULL(data->input_dev))
		return -ENOMEM;

#ifndef MXT_DISTINCT_INPUT_NAME
	data->input_dev->name = DRIVER_NAME;
#endif
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &data->client->dev;
#ifndef CONFIG_FB
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;
#endif
	set_bit(EV_SYN, data->input_dev->evbit);
	set_bit(EV_ABS, data->input_dev->evbit);

	input_set_drvdata(data->input_dev, data);

	return 0;
}

static int mxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mxt_data *data;
	int error;

	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	snprintf(data->phys, sizeof(data->phys), "i2c-%u-%04x/input0",
		 client->adapter->nr, client->addr);

	data->client = client;
	data->irq = client->irq;
	i2c_set_clientdata(client, data);

	error = mxt_handle_pdata(data);
	if (error)
		goto err_free_mem;

	init_completion(&data->bl_completion);
	init_completion(&data->reset_completion);
	init_completion(&data->crc_completion);
	mutex_init(&data->debug_msg_lock);

	error = mxt_gpio_configure(data);
	if (error)
		goto err_free_pdata;

	/* Handle power up in edge triggered IRQ mode */
	mxt_request_irq(data, IRQF_TRIGGER_FALLING | IRQF_ONESHOT);

	error = mxt_power_init(data);
	if (error)
		goto err_free_irq;

	mxt_regulator_enable(data);

	/* check if runtime patches defined */
	mxt_parse_setup_string(data);

	error = mxt_initialize(data);
	if (error)
		goto err_disable_reg;

	mutex_init(&data->crit_section_lock);

#ifdef CONFIG_FB
	data->fb_notif.notifier_call = fb_notifier_callback;
	error = fb_register_client(&data->fb_notif);
	if (error)
		dev_err(&client->dev, "Error registering fb_notifier: %d\n",
			error);
#endif
	error = sysfs_create_group(&client->dev.kobj, &mxt_attr_group);
	if (error) {
		dev_err(&client->dev, "Failure %d creating sysfs group\n",
			error);
		goto err_free_object;
	}

	sysfs_bin_attr_init(&data->mem_access_attr);
	data->mem_access_attr.attr.name = "mem_access";
	data->mem_access_attr.attr.mode = S_IRUGO | S_IWUSR | S_IWGRP;
	data->mem_access_attr.read = mxt_mem_access_read;
	data->mem_access_attr.write = mxt_mem_access_write;
	data->mem_access_attr.size = data->mem_size;

	if (sysfs_create_bin_file(&client->dev.kobj,
				  &data->mem_access_attr) < 0) {
		dev_err(&client->dev, "Failed to create %s\n",
			data->mem_access_attr.attr.name);
		goto err_remove_sysfs_group;
	}

	data->mem_access_created = true;

	return 0;

err_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
err_free_object:
	mxt_free_object_table(data);
err_disable_reg:
	mxt_regulator_disable(data);
err_free_irq:
	free_irq(data->irq, data);
	mxt_gpio_free(data);
err_free_pdata:
	if (!dev_get_platdata(&data->client->dev))
		kfree(data->pdata);
err_free_mem:
	kfree(data);
	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	if (data->mem_access_created)
		sysfs_remove_bin_file(&client->dev.kobj,
				      &data->mem_access_attr);

	sysfs_remove_group(&client->dev.kobj, &mxt_attr_group);
	free_irq(data->irq, data);
	regulator_put(data->reg_avdd);
	if (data->pdata->common_vdd_supply == 0)
		regulator_put(data->reg_vdd);
	mxt_gpio_free(data);
	mxt_free_object_table(data);
	if (!dev_get_platdata(&data->client->dev))
		kfree(data->pdata);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);

	if (!data->suspended) {
		/* if driver is in critical section at the moment,
		 * mutex_lock can block until mutex gets released */
		mutex_lock(&data->crit_section_lock);
		dev_dbg(&data->client->dev, "critical section LOCK\n");

		mxt_set_sensor_state(data, STATE_SUSPEND);
		mxt_reset_slots(data);

		if (data->use_regulator)
			mxt_regulator_disable(data);
		else if (!data->sensor_sleep)
			mxt_sensor_sleep(data);
	}

	data->poweron = false;
	data->suspended = true;

	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	int state = mxt_get_sensor_state(data);

	if (data->suspended) {
		if (data->use_regulator) {
			mxt_regulator_enable(data);
			mxt_acquire_irq(data);
		} else if (data->sensor_sleep) {
			mxt_sensor_wake(data, false);
			mxt_hw_reset(data);
		}

		mutex_unlock(&data->crit_section_lock);
		dev_dbg(&data->client->dev, "critical section RELEASE\n");
	}

	if (data->in_bootloader)
		state = (state == STATE_INIT ||
			 state == STATE_FLASH) ? state : STATE_BL;
	else
		state = STATE_ACTIVE;

	mxt_set_sensor_state(data, state);
	data->poweron = true;
	data->suspended = false;

	return 0;
}

#ifdef CONFIG_FB
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct mxt_data *mxt_dev_data =
		container_of(self, struct mxt_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && mxt_dev_data &&
			mxt_dev_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			mxt_resume(&mxt_dev_data->client->dev);
			dev_dbg(&mxt_dev_data->client->dev, "DISPLAY-ON\n");
		} else if (*blank == FB_BLANK_POWERDOWN) {
			mxt_suspend(&mxt_dev_data->client->dev);
			dev_dbg(&mxt_dev_data->client->dev, "DISPLAY-OFF\n");
		}
	}

	return 0;
}
#endif

static const struct dev_pm_ops mxt_pm_ops = {
#if !defined(CONFIG_FB)
	.suspend	= mxt_suspend,
	.resume		= mxt_resume,
#endif
};
#endif

static void mxt_shutdown(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	disable_irq(data->irq);
}

static const struct i2c_device_id mxt_id[] = {
	{ "qt602240_ts", 0 },
	{ "atmel_mxt_ts", 0 },
	{ "atmel_mxt_tp", 0 },
	{ "mXT224", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mxt_id);

#ifdef CONFIG_OF
static struct of_device_id mxt_match_table[] = {
	{ .compatible = "atmel,mxt-ts",},
	{ },
};
#else
#define mxt_match_table NULL
#endif

static struct i2c_driver mxt_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = mxt_match_table,
#ifdef CONFIG_PM
		.pm	= &mxt_pm_ops,
#endif
	},
	.probe		= mxt_probe,
	.remove		= mxt_remove,
	.shutdown	= mxt_shutdown,
	.id_table	= mxt_id,
};

module_i2c_driver(mxt_driver);

/* Module information */
MODULE_AUTHOR("Joonyoung Shim <jy0922.shim@samsung.com>");
MODULE_DESCRIPTION("Atmel maXTouch Touchscreen driver");
MODULE_LICENSE("GPL");
