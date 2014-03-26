/*
 * mms_ts.c - Touchscreen driver for Melfas MMS-series touch controllers
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *         Simon Wilson <simonwilson@google.com>
 *
 * ISP reflashing code based on original code from Melfas.
 * ISC reflashing code based on original code from Melfas.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define SHOW_COORD		1
#define ISC_DL_MODE		1
#define TOUCH_BOOSTER		0
#define SEC_TSP_FACTORY_TEST	1
#define TSP_TA_CALLBACK		1
#define TSP_ENABLE_SW_RESET	0
#define SHOW_TSP_DEBUG_MSG	1

#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <mach/gpio.h>
#include <linux/uaccess.h>
#include <linux/platform_data/mms_ts_w.h>
#include <asm/unaligned.h>
#include <linux/fb.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#if TOUCH_BOOSTER
#include <mach/cpufreq.h>
#include <mach/dev.h>
#endif

#define EVENT_SZ_8_BYTES	8
#define MAX_FINGERS		10
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_ANGLE		90
#define MIN_ANGLE		-90

/* Registers */
#define MMS_INPUT_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT0	0x10

#define MMS_TSP_REVISION	0xF0
#define MMS_HW_REVISION		0xF1
#define MMS_COMPAT_GROUP	0xF2
#define MMS_FW_VERSION_REG	0xF3

#define MMS_TA_REG	0x32
#define MMS_TA_ON	0x02
#define MMS_TA_OFF	0x03

#define MMS_NOISE_REG	0x30
#define MMS_NOISE_ON	0x01
#define MMS_NOISE_OFF	0x02

enum {
	COMPARE_UPDATE = 0,
	FORCED_UPDATE,
};

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

enum {
	TSP_STATE_RELEASE = 0,
	TSP_STATE_PRESS,
	TSP_STATE_MOVE,
};

#if TOUCH_BOOSTER
#define TOUCH_BOOSTER_CPU_CLK		800000
#define TOUCH_BOOSTER_BUS_CLK_266	267160
#define TOUCH_BOOSTER_BUS_CLK_400	400200
#define TOUCH_BOOSTER_OFF_TIME		100
#define TOUCH_BOOSTER_CHG_TIME		200
enum {
	TOUCH_BOOSTER_DELAY_OFF = 0,
	TOUCH_BOOSTER_ON,
	TOUCH_BOOSTER_QUICK_OFF,
};
#endif

#if ISC_DL_MODE	/* ISC_DL_MODE start */
/*
 * ISC_XFER_LEN	- ISC unit transfer length.
 * Give number of 2 power n, where  n is between 2 and 10
 * i.e. 4, 8, 16 ,,, 1024
 */
#define ISC_XFER_LEN		256

#define MMS_FLASH_PAGE_SZ	1024
#define ISC_BLOCK_NUM		(MMS_FLASH_PAGE_SZ / ISC_XFER_LEN)

#define FLASH_VERBOSE_DEBUG	1
#define MAX_SECTION_NUM		3

#define MAX_FINGER_NUM		5
#define FINGER_EVENT_SZ		8
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_LOG_LENGTH		128

/* Registers */
#define MMS_MODE_CONTROL	0x01
#define MMS_TX_NUM		0x0B
#define MMS_RX_NUM		0x0C
#define MMS_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT		0x10
#define MMS_UNIVERSAL_CMD	0xA0
#define MMS_UNIVERSAL_RESULT	0xAF
#define MMS_CMD_ENTER_ISC	0x5F
#define MMS_FW_VERSION		0xE1
#define MMS_ERASE_DEFEND	0xB0

/* Runtime config */
#define MMS_RUN_CONF_POINTER	0xA1
#define MMS_GET_RUN_CONF	0xA2
#define MMS_SET_RUN_CONF	0xA3

#define MMS_GET_CONF_VER	0x01

/* Universal commands */
#define MMS_CMD_SET_LOG_MODE	0x20
#define MMS_CMD_CONTROL		0x22
#define MMS_SUBCMD_START	0x80

/* Event types */
#define MMS_LOG_EVENT		0xD
#define MMS_NOTIFY_EVENT	0xE
#define MMS_ERROR_EVENT		0xF
#define MMS_TOUCH_KEY_EVENT	0x40

/* Firmware file name */
#define TSP_FW_NAME		"mms_ts_w.fw"
#define TSP_FW_CONFIG_NAME	"mms_ts_w_config.fw"
#define MAX_FW_PATH	255
#define TSP_FW_DIRECTORY	"tsp_melfas/w/"

/* Firmware Start Control */
#define RUN_START		0
#define RUN_STOP		1

/* mfsp offset */
#define MMS_MFSP_OFFSET		16

enum {
	SEC_NONE = -1,
	SEC_BOOT = 0,
	SEC_CORE,
	SEC_CONF,
	SEC_LIMIT
};

enum {
	GET_RX_NUM	= 1,
	GET_TX_NUM,
	GET_EVENT_DATA,
};

enum {
	LOG_TYPE_U08	= 2,
	LOG_TYPE_S08,
	LOG_TYPE_U16,
	LOG_TYPE_S16,
	LOG_TYPE_U32	= 8,
	LOG_TYPE_S32,
};

enum {
	ISC_ADDR		= 0xD5,

	ISC_CMD_READ_STATUS	= 0xD9,
	ISC_CMD_READ		= 0x4000,
	ISC_CMD_EXIT		= 0x8200,
	ISC_CMD_PAGE_ERASE	= 0xC000,

	ISC_PAGE_ERASE_DONE	= 0x10000,
	ISC_PAGE_ERASE_ENTER	= 0x20000,
};

enum {
	CONFIGID_COMMONCONF	= 0,
	CONFIGID_NORMALCONF	= 1,
	CONFIGID_TXCH		= 2,
	CONFIGID_RXCH,
	CONFIGID_KEYTXIDX,
	CONFIGID_KEYRXIDX,
	CONFIGID_DELAYOFFSET,
	CONFIGID_NOISECONF,
	CONFIGID_NORMALIZETABLE,
	CONFIGID_KEYNORMALIZETABLE,
};

enum {
	MMS_RUN_TYPE_SINGLE	= 1,
	MMS_RUN_TYPE_ARRAY,
	MMS_RUN_TYPE_END,
	MMS_RUN_TYPE_INFO,
	MMS_RUN_TYPE_UNKNOWN,
};

struct mms_config_item {
	u16	type;
	u16	category;
	u16	offset;
	u16	datasize;
	u16	data_blocksize;
	u16	reserved;

	u32	value;
} __packed;

struct mms_config_hdr {
	char	mark[4];

	char	tag[4];

	u32	core_version;
	u32	config_version;
	u32	data_offset;
	u32	data_count;

	u32	reserved0;
	u32	info_offset;
	u32	reserved2;
	u32	reserved3;
	u32	reserved4;
	u32	reserved5;

} __packed;

struct mms_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;
	u32	reserved1;

} __packed;

struct mms_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;

} __packed;

struct isc_packet {
	u8	cmd;
	u32	addr;
	u8	data[0];
} __packed;

#endif	/* ISC_DL_MODE end */

enum {
	ISP_MODE_FLASH_ERASE = 0x59F3,
	ISP_MODE_FLASH_WRITE = 0x62CD,
	ISP_MODE_FLASH_READ = 0x6AC9,
};

/* each address addresses 4-byte words */
#define ISP_MAX_FW_SIZE		(0x1F00 * 4)
#define ISP_IC_INFO_ADDR	0x1F00

#if SEC_TSP_FACTORY_TEST
#define TSP_BUF_SIZE	1024

/* self diagnostic */
#define ADDR_CH_NUM		0x0B
#define ADDR_UNIV_CMD		0xA0
#define CMD_ENTER_TEST		0x40
#define CMD_EXIT_TEST		0x4F
#define CMD_CM_DELTA		0x41
#define CMD_GET_DELTA		0x42
#define CMD_CM_ABS		0X43
#define CMD_GET_ABS		0X44
#define CMD_CM_JITTER		0X45
#define CMD_GET_JITTER		0X46
#define CMD_GET_INTEN		0x70
#define CMD_GET_INTEN_KEY	0x71
#define CMD_RESULT_SZ		0XAE
#define CMD_RESULT		0XAF

/* VSC(Vender Specific Command)  */
#define MMS_VSC_CMD			0xB0	/* vendor specific command */
#define MMS_VSC_MODE			0x1A	/* mode of vendor */

#define MMS_VSC_CMD_ENTER		0X01
#define MMS_VSC_CMD_CM_DELTA		0X02
#define MMS_VSC_CMD_CM_ABS		0X03
#define MMS_VSC_CMD_EXIT		0X05
#define MMS_VSC_CMD_INTENSITY		0X04
#define MMS_VSC_CMD_RAW			0X06
#define MMS_VSC_CMD_REFER		0X07

#define TSP_CMD_STR_LEN			32
#define TSP_CMD_RESULT_STR_LEN		512
#define TSP_CMD_PARAM_NUM		8

enum {
	FAIL_PWR_CONTROL = -1,
	SUCCESS_PWR_CONTROL = 0,
};

enum {	/* this is using by cmd_state valiable. */
	WAITING = 0,
	RUNNING,
	OK,
	FAIL,
	NOT_APPLICABLE,
};
#endif /* SEC_TSP_FACTORY_TEST */

struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};

struct mms_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	char	phys[32];
	int	max_x;
	int	max_y;
	bool	invert_x;
	bool	invert_y;
	int	irq;
	int	(*power) (int on);
	void	(*input_event)(void *data);
	bool	enabled;
	u8	fw_ic_ver[3];
	int	finger_byte;
	const u8	*config_fw_version;
	unsigned char	finger_state[MAX_FINGERS];
	u16	mcount[MAX_FINGERS];

	struct melfas_tsi_platform_data *pdata;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;

	void (*register_cb)(void *);
	struct tsp_callbacks callbacks;
	bool	ta_status;
	bool	noise_mode;

	char	*fw_name;
	u8	*config_fw;

	bool	resume_done;

#if !TSP_ENABLE_SW_RESET
	struct delayed_work work_config_set;
#endif

#if TOUCH_BOOSTER
	struct delayed_work work_dvfs_off;
	struct delayed_work work_dvfs_chg;
	bool dvfs_lock_status;
	struct mutex dvfs_lock;
#endif	/* TOUCH_BOOSTER */

#if SEC_TSP_FACTORY_TEST
	struct list_head	cmd_list_head;
	u8			cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex		cmd_lock;
	bool			cmd_is_running;
	bool			ft_flag;
	unsigned int *reference;
	unsigned int *cm_abs;
	unsigned int *cm_delta;
	unsigned int *intensity;
#endif	/* SEC_TSP_FACTORY_TEST */
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
#endif

#if SEC_TSP_FACTORY_TEST
#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_cm_abs(void *device_data);
static void get_cm_delta(void *device_data);
static void get_intensity(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void run_reference_read(void *device_data);
static void run_cm_abs_read(void *device_data);
static void run_cm_delta_read(void *device_data);
static void run_intensity_read(void *device_data);
static void not_support_cmd(void *device_data);

struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("get_cm_delta", get_cm_delta),},
	{TSP_CMD("get_intensity", get_intensity),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};
#endif

#if ISC_DL_MODE
static int mms_read_config(struct i2c_client *client, u8 *buf,
				u8 *get_buf, int len);
static int mms_config_flash(struct mms_ts_info *info, const u8 *buf,
				const u8 len, char *text);
static int mms_config_start(struct mms_ts_info *info);
static int mms_config_finish(struct mms_ts_info *info);
static void mms_config_set(void *context);
static int mms_config_get(struct mms_ts_info *info, u8 mode);
static int melfas_power(struct mms_ts_info *info, int onoff);

static void mms_reboot(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
	i2c_smbus_write_byte_data(info->client, MMS_ERASE_DEFEND, 1);

	i2c_lock_adapter(adapter);

	melfas_power(info, 0);
	usleep_range(1000, 1100);
	melfas_power(info, 1);
	usleep_range(25000, 25100);

	i2c_unlock_adapter(adapter);
}

static int mms_verify_config(struct mms_ts_info *info, const u8 *buf,
				const u8 *tmp, int len)
{
	int count = 0;
	int ret = 0;
	if (memcmp(buf, tmp , len)) {
		dev_info(&info->client->dev, "Run-time config verify failed\n");
		mms_reboot(info);
		mms_config_set(info);
		ret = 1;
		count++;
	}
	if (count > 20) {
		mms_reboot(info);
		dev_info(&info->client->dev, "Run-time config failed\n");
		ret = 1;
	}
	return ret;
}

static int mms_read_config(struct i2c_client *client, u8 *buf,
				u8 *get_buf, int len)
{
	u8 cmd = MMS_GET_RUN_CONF;
	struct i2c_msg msg[3] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = buf,
			.len = 4,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = get_buf,
			.len = len,
		},
	};
	return (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
				!= ARRAY_SIZE(msg));
}

static int mms_config_flash(struct mms_ts_info *info, const u8 *buf,
				const u8 len, char *text)
{
	struct i2c_client *client;
	struct i2c_msg msg;
	int ret;
	client = info->client;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = (u8 *)buf;
	msg.len = len;

	if (i2c_transfer(client->adapter, &msg, 1) != 1) {
		dev_err(&client->dev, "failed to transfer %s data\n", text);
		mms_reboot(info);
		mms_config_set(info);
		return 0;
	} else {
		ret = 1;
	}
	return ret;
}

static void mms_config_set(void *context)
{
	struct mms_ts_info *info = context;
	struct i2c_client *client = info->client;
	struct mms_config_hdr	*conf_hdr;
	struct mms_config_item **conf_item;
	int offset;
	int offset_tmp = 0;
	int i;
	u8 config_flash_set[4];
	u8 config_flash_data[5];
	u8 *config_array;
	u8 flash_array[50];
	u8 cmp_data[30];

	if (info->config_fw == NULL) {
		dev_err(&client->dev, "failed to get config fw\n");
		return;
	}

	mms_config_start(info);

	conf_hdr = (struct mms_config_hdr *)info->config_fw;

	if ((conf_hdr->core_version & 0xff) != info->fw_ic_ver[1]) {
		mms_reboot(info);
		dev_err(&client->dev, "mfsp-version is not correct : 0x%x 0x%x :: 0x%x 0x%x\n",
			conf_hdr->core_version, conf_hdr->config_version & 0xff,
			info->fw_ic_ver[1], info->fw_ic_ver[2]);
		return;
	}

	if (conf_hdr->mark[3] != 0x02) {
		mms_reboot(info);
		dev_err(&client->dev, "failed to mfsp-version : %x\n",
			conf_hdr->mark[3]);
		return;
	}

	offset = conf_hdr->data_offset;
	conf_item = kzalloc(sizeof(*conf_item)*conf_hdr->data_count,
				GFP_KERNEL);

	for (i = 0;; i++, offset += MMS_MFSP_OFFSET) {
		conf_item[i] = (struct mms_config_item *)
					(info->config_fw + offset);

		if (i == MMS_GET_CONF_VER)
			dev_info(&info->client->dev, "Runtime Conf_Ver[%02x]\n",
							conf_item[i]->value);

		if (conf_item[i]->type == MMS_RUN_TYPE_INFO) {
			offset_tmp = conf_item[i]->data_blocksize;
			offset += offset_tmp;
		}

		if (conf_item[i]->type == MMS_RUN_TYPE_SINGLE) {
			config_flash_set[0] = MMS_RUN_CONF_POINTER;
			config_flash_set[1] = conf_item[i]->category;
			config_flash_set[2] = conf_item[i]->offset;
			config_flash_set[3] = conf_item[i]->datasize;

			mms_config_flash(info, config_flash_set,
				4, "config-set");
		}
		if (conf_item[i]->type == MMS_RUN_TYPE_ARRAY) {
			config_flash_set[0] = MMS_RUN_CONF_POINTER;
			config_flash_set[1] = conf_item[i]->category;
			config_flash_set[2] = conf_item[i]->offset;
			config_flash_set[3] = conf_item[i]->datasize;

			mms_config_flash(info, config_flash_set,
				4, "array-set");

			offset_tmp = conf_item[i]->data_blocksize;
			config_array = (u8 *)(info->config_fw +
					(offset + MMS_MFSP_OFFSET));
			offset += offset_tmp;

			flash_array[0] = MMS_SET_RUN_CONF;
			memcpy(&flash_array[1], config_array,
				conf_item[i]->datasize);

			mms_config_flash(info, flash_array,
				conf_item[i]->datasize + 1, "array_data");
			mms_read_config(client, config_flash_set, cmp_data,
				conf_item[i]->datasize);
			if (mms_verify_config(info, &flash_array[1], cmp_data,
				conf_item[i]->datasize) != 0)
				break;
		}

		config_flash_data[0] = MMS_SET_RUN_CONF;
		if (conf_item[i]->datasize == 1) {
			config_flash_data[1] = (u8)conf_item[i]->value;
			mms_config_flash(info, config_flash_data,
				2, "config-data1");
			mms_read_config(client, config_flash_set, cmp_data,
					conf_item[i]->datasize);

			if (mms_verify_config(info, &config_flash_data[1],
				cmp_data, 1) != 0)
				break;

		} else if (conf_item[i]->datasize == 2) {
			config_flash_data[1] =
				(u8)((conf_item[i]->value&0x00FF)>>0);
			config_flash_data[2] =
				(u8)((conf_item[i]->value&0xFF00)>>8);
			mms_config_flash(info, config_flash_data,
				3, "config-data2");
			mms_read_config(client, config_flash_set, cmp_data,
					conf_item[i]->datasize);
			if (mms_verify_config(info, &config_flash_data[1],
				cmp_data, 2) != 0)
				break;
		} else if (conf_item[i]->datasize == 4) {
			config_flash_data[1] =
				(u8)((conf_item[i]->value&0x000000FF)>>0);
			config_flash_data[2] =
				(u8)((conf_item[i]->value&0x0000FF00)>>8);
			config_flash_data[3] =
				(u8)((conf_item[i]->value&0x00FF0000)>>16);
			config_flash_data[4] =
				(u8)((conf_item[i]->value&0xFF000000)>>24);
			mms_config_flash(info, config_flash_data,
				5, "config-data4");
			mms_read_config(client, config_flash_set, cmp_data,
					conf_item[i]->datasize);

			if (mms_verify_config(info, &config_flash_data[1],
				cmp_data, 4) != 0)
				break;
		}
		if (conf_item[i]->type == MMS_RUN_TYPE_END) {
			mms_config_finish(info);
			break;
		}

	}

	kfree(conf_item);
	return;
}
static int mms_config_get(struct mms_ts_info *info, u8 mode)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	const struct firmware *fw;
	char fw_path[MAX_FW_PATH+1];

	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;

	if (mode == REQ_FW) {
		snprintf(fw_path, MAX_FW_PATH, "%s%s",
				TSP_FW_DIRECTORY, TSP_FW_CONFIG_NAME);

		ret = request_firmware(&fw, fw_path, &client->dev);
		if (ret) {
			dev_err(&client->dev,
				"fail config request_firmware[%s]\n", fw_path);
			release_firmware(fw);
			return (ret < 0) ? ret : -ret;
		}

		kfree(info->config_fw);
		info->config_fw = kzalloc((size_t)fw->size, GFP_KERNEL);
		memcpy((void *)info->config_fw, fw->data, fw->size);

		release_firmware(fw);
	} else if (mode == UMS) {
		struct filename f_name;

		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH,
				"/sdcard/%s", TSP_FW_CONFIG_NAME);
		f_name.name = fw_path;
		fp = file_open_name(&f_name, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			dev_err(&client->dev, "file %s open error:%d\n",
					fw_path, (s32)fp);
			return -EINVAL;
		} else {
			fsize = fp->f_path.dentry->d_inode->i_size;
			kfree(info->config_fw);
			info->config_fw = kzalloc((size_t)fsize, GFP_KERNEL);

			nread = vfs_read(fp, (char __user *)info->config_fw,
					fsize, &fp->f_pos);
			if (nread != fsize)
				dev_err(&client->dev, "nread != fsize error\n");

			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else {
		dev_err(&client->dev, "%s error mode[%d]\n", __func__, mode);
		return -EINVAL;
	}

	dev_info(&client->dev, "succeed to get runtime-config firmware\n");

	return 0;
}
static int mms_config_start(struct mms_ts_info *info)
{
	struct i2c_client *client;
	u8 mms_conf_buffer[4] = {MMS_UNIVERSAL_CMD, MMS_CMD_CONTROL,
				MMS_SUBCMD_START, RUN_START};
	client = info->client;

	dev_info(&client->dev, "runtime-config firmware update start!\n");
	usleep_range(40000, 40100);
	mms_config_flash(info, mms_conf_buffer, 4, "start-packit");
	return 0;
}
static int mms_config_finish(struct mms_ts_info *info)
{
	struct i2c_client *client;
	u8 mms_conf_buffer[4] = {MMS_UNIVERSAL_CMD, MMS_CMD_CONTROL,
				MMS_SUBCMD_START, RUN_STOP};
	client = info->client;

	mms_config_flash(info, mms_conf_buffer, 4, "finish-packit");
	dev_info(&client->dev, "succeed to runtime-config firmware update\n");
	return 0;
}
static int mms_isc_read_status(struct mms_ts_info *info, u32 val)
{
	struct i2c_client *client = info->client;
	u8 cmd = ISC_CMD_READ_STATUS;
	u32 result = 0;
	int cnt = 100;
	int ret = 0;

	do {
		i2c_smbus_read_i2c_block_data(client, cmd, 4, (u8 *)&result);
		if (result == val)
			break;
		usleep_range(1000, 1100);
	} while (--cnt);

	if (!cnt) {
		dev_err(&client->dev,
			"status read fail. cnt : %d, val : 0x%x != 0x%x\n",
			cnt, result, val);
	}

	return ret;
}

static int mms_isc_transfer_cmd(struct mms_ts_info *info, int cmd)
{
	struct i2c_client *client = info->client;
	struct isc_packet pkt = { ISC_ADDR, cmd };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(struct isc_packet),
		.buf = (u8 *)&pkt,
	};

	return (i2c_transfer(client->adapter, &msg, 1) != 1);
}

static int mms_isc_erase_page(struct mms_ts_info *info, int page)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_PAGE_ERASE | page) ||
		mms_isc_read_status(info, ISC_PAGE_ERASE_DONE |
					ISC_PAGE_ERASE_ENTER | page);
}

static int mms_isc_enter(struct mms_ts_info *info)
{
	return i2c_smbus_write_byte_data(info->client, MMS_CMD_ENTER_ISC, true);
}

static int mms_isc_exit(struct mms_ts_info *info)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_EXIT);
}

static int mms_flash_section(struct mms_ts_info *info,
				struct mms_fw_img *img, const u8 *data)
{
	struct i2c_client *client = info->client;
	struct isc_packet *isc_packet;
	int ret;
	int page, i;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = ISC_XFER_LEN,
		},
	};
	int ptr = img->offset;

	isc_packet = kzalloc(sizeof(*isc_packet) + ISC_XFER_LEN, GFP_KERNEL);
	isc_packet->cmd = ISC_ADDR;

	msg[0].buf = (u8 *)isc_packet;
	msg[1].buf = kzalloc(ISC_XFER_LEN, GFP_KERNEL);

	for (page = img->start_page; page <= img->end_page; page++) {
		mms_isc_erase_page(info, page);

		for (i = 0; i < ISC_BLOCK_NUM; i++, ptr += ISC_XFER_LEN) {
			/* flash firmware */
			u32 tmp = page * 256 + i * (ISC_XFER_LEN / 4);
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet) + ISC_XFER_LEN;

			memcpy(isc_packet->data, data + ptr, ISC_XFER_LEN);
			if (i2c_transfer(client->adapter, msg, 1) != 1)
				goto i2c_err;

			/* verify firmware */
			tmp |= ISC_CMD_READ;
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet);

			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
					!= ARRAY_SIZE(msg))
				goto i2c_err;

			if (memcmp(isc_packet->data, msg[1].buf,
					ISC_XFER_LEN)) {
#if FLASH_VERBOSE_DEBUG
				print_hex_dump(KERN_ERR, "mms fw wr : ",
						DUMP_PREFIX_OFFSET, 16, 1,
						isc_packet->data, ISC_XFER_LEN,
						false);

				print_hex_dump(KERN_ERR, "mms fw rd : ",
						DUMP_PREFIX_OFFSET, 16, 1,
						msg[1].buf, ISC_XFER_LEN,
						false);
#endif
				dev_err(&client->dev, "flash verify failed\n");
				ret = -1;
				goto out;
			}

		}
	}

	dev_info(&client->dev, "section [%d] update succeeded\n", img->type);

	ret = 0;
	goto out;

i2c_err:
	dev_err(&client->dev, "i2c failed @ %s\n", __func__);
	ret = -1;

out:
	kfree(isc_packet);
	kfree(msg[1].buf);

	return ret;
}

static int get_fw_version_ic(struct i2c_client *client, u8 *buf)
{
	u8 cmd = MMS_FW_VERSION;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = MAX_SECTION_NUM,
		},
	};

	return (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
				!= ARRAY_SIZE(msg));
}

static int mms_flash_fw(const u8 *fw_data, struct mms_ts_info *info,
				u8 update_mode)
{
	int ret = 0;
	int i;
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img **img;
	struct i2c_client *client = info->client;
	u8 ver[MAX_SECTION_NUM];
	u8 target[MAX_SECTION_NUM];
	int offset = sizeof(struct mms_bin_hdr);
	int retires = 3;
	bool update_flag = false;
	bool isc_enter_flag = false;

	if (fw_data == NULL) {
		dev_err(&client->dev, "mms_flash_fw fw_data is NULL!");
		return 1;
	}

	fw_hdr = (struct mms_bin_hdr *)fw_data;
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	while (retires--) {
		if (!get_fw_version_ic(client, ver))
			break;
		else
			mms_reboot(info);
	}

	if (retires < 0) {
		dev_warn(&client->dev, "failed to obtain ver. info\n");
		memset(ver, 0xff, sizeof(ver));
	}

	for (i = 0; i < MAX_SECTION_NUM; i++)
		info->fw_ic_ver[i] = ver[i];
	dev_info(&client->dev, "MMS-128S Before FW update : [0x%02x][0x%02x][0x%02x]",
				ver[SEC_BOOT], ver[SEC_CORE], ver[SEC_CONF]);

	for (i = 0; i < fw_hdr->section_num;
				i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw_data + offset);
		target[i] = img[i]->version;

		if (update_mode == COMPARE_UPDATE)
			info->pdata->fw_bin_ver[i] = target[i];

		if (update_mode == FORCED_UPDATE) {
			update_flag = true;
			dev_info(&client->dev, "section [%d] forced update mode. ver : 0x%x, bin : 0x%x\n",
					img[i]->type, ver[img[i]->type],
					target[i]);
		} else if (ver[img[i]->type] == 0xff) {
			update_flag = true;
			dev_info(&client->dev, "tsp ic fw error : section [%d] is need to be updated\n",
					img[i]->type);
		/* defence code for empty tsp ic */
		} else if (ver[img[i]->type] > 0x30) {
			update_flag = true;
			dev_info(&client->dev, "empty tsp ic : section [%d] is need to be updated\n",
					img[i]->type);
		} else if (ver[img[i]->type] < target[i]) {
			update_flag = true;
			dev_info(&client->dev, "section [%d] is need to be updated. ver : 0x%x, bin : 0x%x\n",
					img[i]->type, ver[img[i]->type],
					target[i]);
		} else {
			dev_info(&client->dev, "section [%d] is up to date, ver : 0x%x\n",
					i, target[i]);
		}

		if (update_flag) {
			if (!isc_enter_flag) {
				mms_isc_enter(info);
				isc_enter_flag = true;
			}

			ret = mms_flash_section(info, img[i],
					fw_data + fw_hdr->binary_offset);
			update_flag = false;

			if (ret) {
				dev_err(&client->dev,
					"failed to mms_flash_section[%d]\n", i);
				mms_reboot(info);
				goto out;
			}
		}
		info->fw_ic_ver[i] = target[i];
	}

	if (isc_enter_flag) {
		mms_isc_exit(info);
		mms_reboot(info);

		if (get_fw_version_ic(client, ver)) {
			dev_err(&client->dev, "failed to obtain version after flash\n");
			ret = -1;
			goto out;
		} else {
			for (i = 0; i < fw_hdr->section_num; i++) {
				if (ver[img[i]->type] != target[i]) {
					dev_info(&client->dev,
						"version mismatch after flash. [%d] 0x%02x != 0x%02x\n",
						i, ver[img[i]->type],
						target[i]);

					ret = -1;
					goto out;
				}
			}
		}
	}

out:
	kfree(img);

	return ret;
}
#endif	/* ISC_DL_MODE */

#if !TSP_ENABLE_SW_RESET
static void work_mms_config_set(struct work_struct *work)
{
	struct mms_ts_info *info = container_of(work,
				struct mms_ts_info, work_config_set.work);
	mms_config_set(info);

#if TSP_TA_CALLBACK
	dev_info(&info->client->dev, "%s & TA %sconnect & noise mode %s!\n",
					__func__,
					info->ta_status ? "" : "dis",
					info->noise_mode ? "on" : "off");

	if (info->ta_status) {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_ON);
		if (info->noise_mode)
			i2c_smbus_write_byte_data(info->client,
					MMS_NOISE_REG, MMS_NOISE_ON);

	} else {
		if (info->noise_mode)
			info->noise_mode = false;
	}
#endif

	enable_irq(info->irq);

	info->enabled = true;
	info->resume_done = true;

}
#endif

#if TOUCH_BOOSTER
static void change_dvfs_lock(struct work_struct *work)
{
	struct mms_ts_info *info = container_of(work,
				struct mms_ts_info, work_dvfs_chg.work);
	int ret;

	mutex_lock(&info->dvfs_lock);

	ret = set_freq_limit(DVFS_TOUCH_ID, TOUCH_BOOSTER_BUS_CLK_266);
	if (ret < 0)
		dev_err(&info->client->dev,
				"%s dev change bud lock failed(%d)\n",
				__func__, __LINE__);
	else
		dev_dbg(&info->client->dev, "Change dvfs lock");

	mutex_unlock(&info->dvfs_lock);
}
static void set_dvfs_off(struct work_struct *work)
{

	struct mms_ts_info *info = container_of(work,
				struct mms_ts_info, work_dvfs_off.work);
	int ret;

	mutex_lock(&info->dvfs_lock);

	ret = set_freq_limit(DVFS_TOUCH_ID, -1);
	if (ret < 0)
		dev_err(&info->client->dev, " %s: cpu unlock failed(%d)\n",
				__func__, __LINE__);

	info->dvfs_lock_status = false;
	dev_dbg(&info->client->dev, "dvfs off!");

	mutex_unlock(&info->dvfs_lock);
}

static void set_dvfs_lock(struct mms_ts_info *info, uint32_t mode)
{
	int ret;

	mutex_lock(&info->dvfs_lock);

	if (mode == TOUCH_BOOSTER_DELAY_OFF) {
		if (info->dvfs_lock_status) {
			cancel_delayed_work(&info->work_dvfs_chg);
			schedule_delayed_work(&info->work_dvfs_off,
				msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
		}

	} else if (mode == TOUCH_BOOSTER_ON) {
		cancel_delayed_work(&info->work_dvfs_off);
		if (!info->dvfs_lock_status) {
			ret = set_freq_limit(DVFS_TOUCH_ID,
					TOUCH_BOOSTER_BUS_CLK_400);
			if (ret < 0) {
				dev_err(&info->client->dev,
						"%s: cpu lock failed(%d)",
						__func__, __LINE__);
			}

			schedule_delayed_work(&info->work_dvfs_chg,
				msecs_to_jiffies(TOUCH_BOOSTER_CHG_TIME));

			info->dvfs_lock_status = true;
			dev_dbg(&info->client->dev, "dvfs on");
		}
	} else if (mode == TOUCH_BOOSTER_QUICK_OFF) {
		cancel_delayed_work(&info->work_dvfs_off);
		cancel_delayed_work(&info->work_dvfs_chg);
		schedule_work(&info->work_dvfs_off.work);
	}
out:
	mutex_unlock(&info->dvfs_lock);
}
#endif

static void release_all_fingers(struct mms_ts_info *info)
{
	int i;

	dev_notice(&info->client->dev, "%s\n", __func__);

	for (i = 0; i < MAX_FINGERS; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER,
					   false);

		if (info->finger_state[i] != TSP_STATE_RELEASE) {
			dev_notice(&info->client->dev,
						"finger %d up(force)\n", i);
		}
		info->finger_state[i] = TSP_STATE_RELEASE;
		info->mcount[i] = 0;
	}
	input_report_key(info->input_dev,
				BTN_TOUCH, 0);
	input_report_key(info->input_dev,
			BTN_TOOL_FINGER, 0);

	input_sync(info->input_dev);

#if TOUCH_BOOSTER
	set_dvfs_lock(info, TOUCH_BOOSTER_QUICK_OFF);
	dev_notice(&info->client->dev, "dvfs lock free.\n");
#endif
}

static void reset_mms_ts(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retries_off = 3, retries_on = 3;

#if TSP_TA_CALLBACK
	dev_info(&client->dev,
			"%s & tsp %s & TA %sconnect & noise mode %s!\n",
			__func__,
			info->enabled ? "on" : "off",
			info->ta_status ? "" : "dis",
			info->noise_mode ? "on" : "off");
#else
	dev_info(&client->dev,
			"%s called & tap %s!\n", __func__,
			info->enabled ? "on" : "off");
#endif

	if (info->enabled == false)
		return;

	info->enabled = false;
	release_all_fingers(info);

	while (retries_off--) {
		if (!melfas_power(info, 0))
			break;
	}

	if (retries_off < 0) {
		dev_err(&info->client->dev,
				"%s : power off error!\n", __func__);
		info->enabled = true;
		return;
	}

	usleep_range(30000, 30100);


	while (retries_on--) {
		if (!melfas_power(info, 1))
			break;
	}

	if (retries_on < 0) {
		dev_err(&info->client->dev,
				"%s : power on error!\n", __func__);
		return;
	}

	usleep_range(120000, 120100);


	mms_config_set(info);

#if TSP_TA_CALLBACK
	if (info->ta_status) {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_ON);
		if (info->noise_mode)
			i2c_smbus_write_byte_data(info->client,
					MMS_NOISE_REG, MMS_NOISE_ON);
	} else
		info->noise_mode = false;
#endif
	info->enabled = true;
}

static void melfas_ta_cb(struct tsp_callbacks *cb, bool ta_status)
{
	struct mms_ts_info *info =
			container_of(cb, struct mms_ts_info, callbacks);
	struct i2c_client *client = info->client;

	info->ta_status = ta_status;

	dev_info(&client->dev, "%s & tsp %s & TA %sconnect & noise mode %s!\n",
					__func__,
					info->enabled ? "on" : "off",
					info->ta_status ? "" : "dis",
					info->noise_mode ? "on" : "off");

	if (info->enabled == false)
		return;

#if TSP_TA_CALLBACK
	if (info->ta_status) {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_ON);

		if (info->noise_mode) {
			i2c_smbus_write_byte_data(info->client,
					MMS_NOISE_REG, MMS_NOISE_ON);
			dev_err(&client->dev, "melfas_ta_cb & abnormal noise mode on!\n");
		}

	} else {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_OFF);

		if (info->noise_mode) {
			info->noise_mode = false;
			i2c_smbus_write_byte_data(info->client,
					MMS_NOISE_REG, MMS_NOISE_OFF);
			dev_info(&client->dev, "ta_cb & noise mode off!\n");
		}
	}
#endif
}

static irqreturn_t mms_ts_interrupt(int irq, void *dev_id)
{
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGERS * EVENT_SZ_8_BYTES] = { 0, };
	int ret, i, sz;
	int id, state, posX, posY, strenth, width;
	int palm, major_axis, minor_axis;
	int finger_event_sz;
	u8 *read_data;
	u8 reg = MMS_INPUT_EVENT0;
#if TOUCH_BOOSTER
	bool press_flag = false;
#endif

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .buf = &reg,
		 .len = 1,
		 }, {
		     .addr = client->addr,
		     .flags = I2C_M_RD,
		     .buf = buf,
		     },
	};
	finger_event_sz = EVENT_SZ_8_BYTES;

	sz = i2c_smbus_read_byte_data(client, MMS_INPUT_EVENT_PKT_SZ);

	if (sz < 0) {
		dev_err(&client->dev, "%s bytes=%d\n", __func__, sz);
		for (i = 0; i < 50; i++) {
			sz = i2c_smbus_read_byte_data(client,
						      MMS_INPUT_EVENT_PKT_SZ);
			if (sz > 0)
				break;
		}

		if (i == 50) {
			dev_dbg(&client->dev, "i2c failed... reset!!\n");
			reset_mms_ts(info);
			return IRQ_HANDLED;
		}
		dev_err(&client->dev, "success read touch info data\n");
	}
	if (sz == 0)
		return IRQ_HANDLED;

	if (sz > MAX_FINGERS*finger_event_sz || sz%finger_event_sz) {
		dev_err(&client->dev, "abnormal data inputed & reset IC[%d]\n",
									sz);
		reset_mms_ts(info);
		return IRQ_HANDLED;
	}

	msg[1].len = sz;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

	if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev,
			"failed to read %d bytes of touch data (%d)\n",
			sz, ret);

		for (i = 0; i < 5; i++) {
			ret = i2c_transfer(client->adapter, msg,
							ARRAY_SIZE(msg));
			if (ret == ARRAY_SIZE(msg))
				break;
		}

		if (i == 5) {
			dev_err(&client->dev,
				"failed to read touch data & reset IC[%d]\n",
									ret);
			reset_mms_ts(info);
			return IRQ_HANDLED;
		}
		dev_err(&client->dev, "success read touch data\n");
	}

	if (buf[0] == MMS_NOTIFY_EVENT) {	/* MMS_NOTIFY_EVENT */
		dev_info(&client->dev, "TSP noise mode enter!(%d)\n", buf[1]);
		i2c_smbus_write_byte_data(info->client,
					MMS_NOISE_REG, MMS_NOISE_ON);
		if (!info->ta_status)
			dev_err(&client->dev, "TSP noise mode enter but ta off!\n");
		info->noise_mode = true;
		return IRQ_HANDLED;
	}

	if (buf[0] == MMS_ERROR_EVENT) { /* MMS_ERROR_EVENT*/
		dev_err(&client->dev, "Error detected, restarting TSP\n");
		reset_mms_ts(info);
		return IRQ_HANDLED;
	}

	for (i = 0; i < sz; i += finger_event_sz) {
		read_data = &buf[i];
		id = (read_data[0] & 0xf) - 1;
		state = read_data[0] & 0x80;
		posX = read_data[2] | ((read_data[1] & 0xf) << 8);
		if (posX >= info->max_x)
			posX = info->max_x-1;
		posY = read_data[3] | (((read_data[1] >> 4) & 0xf) << 8);
		if (posY >= info->max_y)
			posY = info->max_y-1;
		width = read_data[4];
		strenth = read_data[5];
		palm = (read_data[0] & 0x10) >> 4;
		major_axis = read_data[6];
		minor_axis = read_data[7];

		if (info->invert_x) {
			posX = info->max_x - posX;
			if (posX < 0)
				posX = 0;
		}
		if (info->invert_y) {
			posY = info->max_y - posY;
			if (posY < 0)
				posY = 0;
		}
		if (id >= MAX_FINGERS) {
			dev_notice(&client->dev,
				"finger id error [%d]\n", id);
			reset_mms_ts(info);
			return IRQ_HANDLED;
		}

		if (state == TSP_STATE_RELEASE) {
			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, false);
#if SHOW_COORD
			dev_notice(&client->dev, "R [%2d],([%4d],[%3d])[%d][%d]",
				id, posX, posY,	palm, info->mcount[id]);
#else
			dev_notice(&client->dev, "R [%2d][%d]", id,
					info->mcount[id]);
#endif
			info->finger_state[id] = TSP_STATE_RELEASE;
			info->mcount[id] = 0;
		} else {
			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, true);
			input_report_key(info->input_dev,
						BTN_TOUCH, 1);
			input_report_key(info->input_dev,
					BTN_TOOL_FINGER, 1);

			input_report_abs(info->input_dev,
						ABS_MT_POSITION_X, posX);
			input_report_abs(info->input_dev,
						ABS_MT_POSITION_Y, posY);

			input_report_abs(info->input_dev,
						ABS_MT_WIDTH_MAJOR, width);
			input_report_abs(info->input_dev,
						ABS_MT_PRESSURE, strenth);
			input_report_abs(info->input_dev,
						ABS_MT_TOUCH_MAJOR, major_axis);
			input_report_abs(info->input_dev,
						ABS_MT_TOUCH_MINOR, minor_axis);
/*
			input_report_abs(info->input_dev,
						ABS_MT_PALM, palm);
*/
			info->mcount[id] += 1;

			if (info->finger_state[id] == TSP_STATE_RELEASE) {
				info->finger_state[id] = TSP_STATE_PRESS;
#if SHOW_COORD
				dev_notice(&client->dev,
					"P [%2d],([%4d],[%3d]),P:%d W:%d S:%3d Mj_a:%d Mi_a:%d",
					id, posX, posY, palm, width, strenth,
					major_axis, minor_axis);
#else
				dev_notice(&client->dev, "P [%2d]", id);
#endif
			} else
				info->finger_state[id] = TSP_STATE_MOVE;
		}
	}
	input_sync(info->input_dev);

#if TOUCH_BOOSTER
	for (i = 0; i < MAX_FINGERS; i++) {
		if (info->finger_state[i] == TSP_STATE_PRESS
			|| info->finger_state[i] == TSP_STATE_MOVE) {
			press_flag = TOUCH_BOOSTER_ON;
			break;
		}
	}

	set_dvfs_lock(info, press_flag);
#endif

	return IRQ_HANDLED;
}

static int mms_ts_fw_load(struct mms_ts_info *info)
{

	struct i2c_client *client = info->client;
	int ret = 0;

	const struct firmware *fw;
	char fw_path[MAX_FW_PATH+1];
	int retries = 3;

	if (!info->pdata) {
		dev_err(&client->dev,
			"fw cannot be updated, missing platform data\n");
		return 1;
	}

	/* firmware update */
	snprintf(fw_path, MAX_FW_PATH, "%s%s", TSP_FW_DIRECTORY, TSP_FW_NAME);
	dev_info(&client->dev, "tsp fw path : %s\n", fw_path);

	ret = request_firmware(&fw, fw_path, &client->dev);
	if (ret)
		dev_err(&client->dev, "fail request_firmware[%s]\n", fw_path);

	do {
		ret = mms_flash_fw(fw->data, info, COMPARE_UPDATE);
	} while (ret && --retries);
	release_firmware(fw);

	if (!retries) {
		dev_err(&info->client->dev, "failed to flash firmware after retires\n");
	} else {
		/* Runtime config setting*/
		mms_config_get(info, REQ_FW);
		mms_config_set(info);
	}

	return ret;
}

#if SEC_TSP_FACTORY_TEST
static void set_cmd_result(struct mms_ts_info *info, char *buff, int len)
{
	strlcat(info->cmd_result, buff, TSP_CMD_RESULT_STR_LEN);
}

static int get_data(struct mms_ts_info *info, u8 addr, u8 size, u8 *array)
{
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	u8 reg = addr;
	unsigned char buf[size];
	int ret;

	msg.addr = client->addr;
	msg.flags = 0x00;
	msg.len = 1;
	msg.buf = &reg;

	ret = i2c_transfer(adapter, &msg, 1);

	if (ret >= 0) {
		msg.addr = client->addr;
		msg.flags = I2C_M_RD;
		msg.len = size;
		msg.buf = buf;

		ret = i2c_transfer(adapter, &msg, 1);
	}
	if (ret < 0) {
		pr_err("[TSP] : read error : [%d]", ret);
		return ret;
	}

	memcpy(array, &buf, size);
	return size;
}

static void get_intensity_data(struct mms_ts_info *info)
{
	u8 w_buf[4];
	u8 r_buf;
	u8 read_buffer[60] = {0};
	int i, j;
	int ret;
	u16 max_value = 0, min_value = 0;
	u16 raw_data;
	char buff[TSP_CMD_STR_LEN] = {0};
	int tx_num = info->pdata->tsp_tx;
	int rx_num = info->pdata->tsp_rx;

	disable_irq(info->irq);

	w_buf[0] = ADDR_UNIV_CMD;
	w_buf[1] = CMD_GET_INTEN;
	w_buf[2] = 0xFF;
	for (i = 0; i < rx_num; i++) {
		w_buf[3] = i;

		ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 3, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;
		usleep_range(1, 5);

		ret = i2c_smbus_read_i2c_block_data(info->client,
			CMD_RESULT_SZ, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		ret = get_data(info, CMD_RESULT, r_buf, read_buffer);
		if (ret < 0)
			goto err_i2c;

		for (j = 0; j < r_buf/2; j++) {
			raw_data = read_buffer[2*j] | (read_buffer[2*j+1] << 8);
			if (raw_data > 32767)
				raw_data = 0;
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}
			info->intensity[i * tx_num + j] = raw_data;
			dev_dbg(&info->client->dev,
				"[TSP] intensity[%d][%d] = %d\n", j, i,
				info->intensity[i * tx_num + j]);
		}
	}

#if SHOW_TSP_DEBUG_MSG
	pr_info("\n");
	for (i = 0; i < rx_num; i++) {
		pr_info("melfas-ts data :");
		for (j = 0; j < tx_num; j++)
			pr_info("[%d]", info->intensity[i * tx_num + j]);
		pr_info("\n");
	}
#endif

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->irq);

	return;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
		__func__, MMS_VSC_CMD_INTENSITY);
}

static void get_raw_data(struct mms_ts_info *info, u8 cmd)
{
	u8 w_buf[4];
	u8 r_buf = 0;
	u8 read_buffer[60] = {0};
	int ret;
	int i, j;
	int max_value = 0, min_value = 0;
	int raw_data;
	int retry;
	char buff[TSP_CMD_STR_LEN] = {0};
	int gpio = info->pdata->gpio_int;
	int tx_num = info->pdata->tsp_tx;
	int rx_num = info->pdata->tsp_rx;

	disable_irq(info->irq);

	ret = i2c_smbus_write_byte_data(info->client,
		ADDR_UNIV_CMD, CMD_ENTER_TEST);
	if (ret < 0)
		goto err_i2c;

	/* event type check */
	retry = 1;
	while (retry) {
		while (gpio_get_value(gpio))
			udelay(100);

		ret = i2c_smbus_read_i2c_block_data(info->client,
			0x0F, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		ret = i2c_smbus_read_i2c_block_data(info->client,
			0x10, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		dev_info(&info->client->dev, "event type = 0x%x\n", r_buf);
		if (r_buf == 0x0C)
			retry = 0;
	}

	ret = i2c_smbus_read_i2c_block_data(info->client,
			CMD_RESULT_SZ, 1, &r_buf);
	if (ret < 0)
		goto err_i2c;

	ret = get_data(info, CMD_RESULT, r_buf, read_buffer);
	if (ret < 0)
		goto err_i2c;

	dev_info(&info->client->dev, "get_data : r_buf = 0x%x\n", r_buf);

	w_buf[0] = ADDR_UNIV_CMD;
	if (cmd == MMS_VSC_CMD_CM_DELTA)
		w_buf[1] = CMD_CM_DELTA;
	else
		w_buf[1] = CMD_CM_ABS;
	ret = i2c_smbus_write_i2c_block_data(info->client,
		 w_buf[0], 1, &w_buf[1]);
	if (ret < 0)
		goto err_i2c;

	while (gpio_get_value(gpio))
		udelay(100);

	ret = i2c_smbus_read_i2c_block_data(info->client,
		CMD_RESULT_SZ, 1, &r_buf);
	if (ret < 0)
		goto err_i2c;
	ret = i2c_smbus_read_i2c_block_data(info->client,
		CMD_RESULT, 1, &r_buf);
	if (ret < 0)
		goto err_i2c;

	if (r_buf == 1)
		dev_info(&info->client->dev, "PASS\n");
	else
		dev_info(&info->client->dev, "FAIL\n");

	if (cmd == MMS_VSC_CMD_CM_DELTA)
		w_buf[1] = CMD_GET_DELTA;
	else
		w_buf[1] = CMD_GET_ABS;
	w_buf[2] = 0xFF;

	for (i = 0; i < rx_num; i++) {
		w_buf[3] = i;

		ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 3, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;

		while (gpio_get_value(gpio))
			udelay(100);

		ret = i2c_smbus_read_i2c_block_data(info->client,
			CMD_RESULT_SZ, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		ret = get_data(info, CMD_RESULT, r_buf, read_buffer);
		if (ret < 0)
			goto err_i2c;

		for (j = 0; j < tx_num; j++) {
			raw_data = read_buffer[2*j] | (read_buffer[2*j+1] << 8);
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}

			if (cmd == MMS_VSC_CMD_CM_DELTA) {
				info->cm_delta[i * tx_num + j] =
					raw_data;
				dev_dbg(&info->client->dev,
					"[TSP] delta[%d][%d] = %d\n", j, i,
					info->cm_delta[i * tx_num + j]);
			} else if (cmd == MMS_VSC_CMD_CM_ABS) {
				info->cm_abs[i * tx_num + j] =
					raw_data;
				dev_dbg(&info->client->dev,
					"[TSP] raw[%d][%d] = %d\n", j, i,
					info->cm_abs[i * tx_num + j]);
			} else if (cmd == MMS_VSC_CMD_REFER) {
				info->reference[i * tx_num + j] =
					raw_data;
				dev_dbg(&info->client->dev,
					"[TSP] reference[%d][%d] = %d\n", j, i,
					info->reference[i * tx_num + j]);
			}
		}
	}

	ret = i2c_smbus_write_byte_data(info->client,
		ADDR_UNIV_CMD, CMD_EXIT_TEST);

#if SHOW_TSP_DEBUG_MSG
	pr_info("\n");
	for (i = 0; i < rx_num; i++) {
		dev_info(&info->client->dev, "melfas-ts data :");
		for (j = 0; j < tx_num; j++) {
			if (cmd == MMS_VSC_CMD_CM_DELTA)
				dev_info(&info->client->dev, "[%d]",
					info->cm_delta[i * tx_num + j]);
			else if (cmd == MMS_VSC_CMD_CM_ABS)
				dev_info(&info->client->dev, "[%d]",
					info->cm_abs[i * tx_num + j]);
			else if (cmd == MMS_VSC_CMD_REFER)
				dev_info(&info->client->dev, "[%d]",
					info->reference[i * tx_num + j]);
		}
		pr_info("\n");
	}
#endif
	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->irq);

	return;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
		__func__, cmd);
}


static void get_raw_data_all(struct mms_ts_info *info, u8 cmd)
{
	u8 w_buf[6];
	u8 read_buffer[2];	/* 52 */
	int gpio = info->pdata->gpio_int;
	int ret;
	int i, j;
	u32 max_value = 0, min_value = 0;
	u32 raw_data;
	char buff[TSP_CMD_STR_LEN] = {0};
	int tx_num = info->pdata->tsp_tx;
	int rx_num = info->pdata->tsp_rx;

	disable_irq(info->irq);

	w_buf[0] = MMS_VSC_CMD;	/* vendor specific command id */
	w_buf[1] = MMS_VSC_MODE;	/* mode of vendor */
	w_buf[2] = 0;		/* tx line */
	w_buf[3] = 0;		/* rx line */
	w_buf[4] = 0;		/* reserved */
	w_buf[5] = 0;		/* sub command */

	if (cmd == MMS_VSC_CMD_EXIT) {
		w_buf[5] = MMS_VSC_CMD_EXIT;	/* exit test mode */

		ret = i2c_smbus_write_i2c_block_data(info->client,
						     w_buf[0], 5, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;
		enable_irq(info->irq);
		usleep_range(200000, 200100);
		return;
	}

	/* MMS_VSC_CMD_CM_DELTA or MMS_VSC_CMD_CM_ABS
	 * this two mode need to enter the test mode
	 * exit command must be followed by testing.
	 */
	if (cmd == MMS_VSC_CMD_CM_DELTA || cmd == MMS_VSC_CMD_CM_ABS) {
		/* enter the debug mode */
		w_buf[2] = 0x0;	/* tx */
		w_buf[3] = 0x0;	/* rx */
		w_buf[5] = MMS_VSC_CMD_ENTER;

		ret = i2c_smbus_write_i2c_block_data(info->client,
						     w_buf[0], 5, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;

		/* wating for the interrupt */
		while (gpio_get_value(gpio))
			udelay(100);
	}

	for (i = 0; i < rx_num; i++) {
		for (j = 0; j < tx_num; j++) {

			w_buf[2] = j;	/* tx */
			w_buf[3] = i;	/* rx */
			w_buf[5] = cmd;

			ret = i2c_smbus_write_i2c_block_data(info->client,
					w_buf[0], 5, &w_buf[1]);
			if (ret < 0)
				goto err_i2c;

			usleep_range(1, 5);

			ret = i2c_smbus_read_i2c_block_data(info->client, 0xBF,
							    2, read_buffer);
			if (ret < 0)
				goto err_i2c;

			raw_data = ((u16) read_buffer[1] << 8) | read_buffer[0];
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}

			if (cmd == MMS_VSC_CMD_INTENSITY) {
				info->intensity[j * rx_num + i] = raw_data;
				dev_info(&info->client->dev, "[TSP] intensity[%d][%d] = %d\n",
					i, j, info->intensity[j * rx_num + i]);
			} else if (cmd == MMS_VSC_CMD_CM_DELTA) {
				info->cm_delta[j * rx_num + i] = raw_data;
				dev_info(&info->client->dev, "[TSP] delta[%d][%d] = %d\n",
					i, j, info->cm_delta[j * rx_num + i]);
			} else if (cmd == MMS_VSC_CMD_CM_ABS) {
				info->cm_abs[j * rx_num + i] = raw_data;
				dev_info(&info->client->dev, "[TSP] raw[%d][%d] = %d\n",
					i, j, info->cm_abs[j * rx_num + i]);
			} else if (cmd == MMS_VSC_CMD_REFER) {
				info->reference[j * rx_num + i] =
						raw_data >> 3;
				dev_info(&info->client->dev, "[TSP] reference[%d][%d] = %d\n",
					i, j, info->reference[j * rx_num + i]);
			}
		}

	}

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->irq);
	return;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
		__func__, cmd);
}

static ssize_t show_close_tsp_test(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);

	get_raw_data_all(info, MMS_VSC_CMD_EXIT);
	info->ft_flag = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}

static void set_default_result(struct mms_ts_info *info)
{
	char delim = ':';

	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strlen(info->cmd));
	strlcat(info->cmd_result, &delim, TSP_CMD_RESULT_STR_LEN);
}

static int check_rx_tx_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[TSP_CMD_STR_LEN] = {0};
	int node;

	if (info->cmd_param[0] < 0 ||
			info->cmd_param[0] >= info->pdata->tsp_tx ||
			info->cmd_param[1] < 0 ||
			info->cmd_param[1] >= info->pdata->tsp_rx) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_info(&info->client->dev, "%s: parameter error: %u,%u\n",
				__func__, info->cmd_param[0],
				info->cmd_param[1]);
		node = -1;
		return node;
	}
	/* Model dependency */
	node = info->cmd_param[0] * info->pdata->tsp_rx + info->cmd_param[1];
	dev_info(&info->client->dev, "%s: node = %d\n", __func__,
			node);
	return node;

}

static void not_support_cmd(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buff[16] = {0};

	set_default_result(info);
	snprintf(buff, sizeof(buff), "%s", "NA");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 4;
	dev_info(&info->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}

static void fw_update(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	struct i2c_client *client = info->client;

	char fw_path[MAX_FW_PATH+1];
	u8 *fw_data = NULL;
	u8 ver[MAX_SECTION_NUM];
	int retries = 3;
	int ret = 0, i = 0;
	bool fw_read_flag = false;

	const struct firmware *fw;

	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;

	char result[16] = {0};
	set_default_result(info);

	dev_info(&client->dev,
		"fw_ic_ver = 0x%02x, fw_bin_ver = 0x%02x\n",
		info->fw_ic_ver[SEC_CONF], info->pdata->fw_bin_ver[SEC_CONF]);

	switch (info->cmd_param[0]) {
	case BUILT_IN:
		info->cmd_param[0] = REQ_FW;
		dev_info(&client->dev, "BUILT_IN=>REQ_FW update mode!\n");
		break;

	case UMS:
		dev_info(&client->dev, "UMS update mode!\n");
		break;

	case REQ_FW:
		dev_info(&client->dev, "REQ_FW update mode!\n");
		break;

	default:
		dev_err(&client->dev, "invalid cmd_param[%d]\n",
					info->cmd_param[0]);
		goto not_support;
	}

	disable_irq(info->irq);

	if (info->cmd_param[0] == REQ_FW) {
		snprintf(fw_path, MAX_FW_PATH, "%s%s",
				TSP_FW_DIRECTORY, TSP_FW_NAME);
		ret = request_firmware(&fw, fw_path, &client->dev);
		if (ret) {
			dev_err(&client->dev,
				"fail request_firmware[%s]\n", fw_path);
		} else {
			fw_data = kzalloc((size_t)fw->size, GFP_KERNEL);
			memcpy((void *)fw_data, fw->data, fw->size);
			release_firmware(fw);
			fw_read_flag = true;
		}

	} else if (info->cmd_param[0] == UMS) {
		struct filename f_name;
		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH, "/sdcard/%s", TSP_FW_NAME);
		f_name.name = fw_path;
		fp = file_open_name(&f_name, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			dev_err(&client->dev, "file %s open error:%d\n",
							fw_path, (s32)fp);
		} else {
			fsize = fp->f_path.dentry->d_inode->i_size;
			fw_data = kzalloc((size_t)fsize, GFP_KERNEL);
			nread = vfs_read(fp, (char __user *)fw_data,
					fsize, &fp->f_pos);
			if (nread != fsize)
				dev_err(&client->dev, "nread != fsize error\n");
			filp_close(fp, current->files);
			fw_read_flag = true;
		}
		set_fs(old_fs);
	}

	for (i = 0; fw_read_flag == true && i < retries; i++) {
		ret = mms_flash_fw(fw_data, info, FORCED_UPDATE);
		if (!ret)
			break;
	}
	kfree(fw_data);

	if (!fw_read_flag || i == retries)
		dev_err(&client->dev, "fail fw update\n");

	ret = mms_config_get(info, info->cmd_param[0]);
	if (ret) {
		dev_err(&client->dev, "fail mms_config_get\n");
		mms_config_get(info, REQ_FW);
	}
	mms_config_set(info);

	get_fw_version_ic(client, ver);
	for (i = 0; i < MAX_SECTION_NUM; i++)
		info->fw_ic_ver[i] = ver[i];

	dev_info(&client->dev, "After FW update : [0x%02x]\n", ver[SEC_CONF]);

	enable_irq(info->irq);

	if (fw_read_flag && !ret) {
		snprintf(result, sizeof(result) , "%s", "OK");
		set_cmd_result(info, result, strnlen(result, sizeof(result)));
		info->cmd_state = OK;
	} else {
		snprintf(result, sizeof(result) , "%s", "NG");
		set_cmd_result(info, result, strnlen(result, sizeof(result)));
		info->cmd_state = FAIL;
	}

	return;

not_support:
	snprintf(result, sizeof(result) , "%s", "NG");
	set_cmd_result(info, result, strnlen(result, sizeof(result)));
	info->cmd_state = FAIL;
	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);
	snprintf(buff, sizeof(buff), "ME00%02x%02x",
					info->pdata->fw_bin_ver[SEC_CORE],
					info->pdata->fw_bin_ver[SEC_CONF]);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "ME00%02x%02x",
			info->fw_ic_ver[SEC_CORE], info->fw_ic_ver[SEC_CONF]);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[20] = {0};
	u8 config_flash_set[4];
	u8 config_ver;
	int ret;

	config_flash_set[0] = MMS_RUN_CONF_POINTER;
	config_flash_set[1] = 0;	/* COMMONCONF */
	config_flash_set[2] = 8;	/* offset */
	config_flash_set[3] = 1;	/* length */

	ret = mms_config_flash(info, config_flash_set, 4,
				"config-set-conf-ver");
	if (!ret)
		dev_err(&info->client->dev, "mms_config_flash fail!\n");

	config_flash_set[0] = MMS_GET_RUN_CONF;
	mms_read_config(info->client, config_flash_set, &config_ver, 1);
	dev_info(&info->client->dev, "Runtime config_ver [%02x]\n", config_ver);

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", info->config_fw_version);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_threshold(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	int threshold;

	set_default_result(info);

	threshold = i2c_smbus_read_byte_data(info->client, 0x05);
	if (threshold < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = FAIL;
		return;
}
	snprintf(buff, sizeof(buff), "%d", threshold);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void module_off_master(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[3] = {0};

	mutex_lock(&info->lock);
	if (info->enabled) {
		disable_irq(info->irq);
		info->enabled = false;
	}
	mutex_unlock(&info->lock);

	if (melfas_power(info, 0) == SUCCESS_PWR_CONTROL)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		info->cmd_state = OK;
	else
		info->cmd_state = FAIL;

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[3] = {0};

	if (melfas_power(info, 1) == SUCCESS_PWR_CONTROL) {
		snprintf(buff, sizeof(buff), "%s", "OK");

		/* Set runtime config */
		usleep_range(25000, 25100);
		mms_config_set(info);

		mutex_lock(&info->lock);
		if (!info->enabled) {
			enable_irq(info->irq);
			info->enabled = true;
		}
		mutex_unlock(&info->lock);
	} else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		info->cmd_state = OK;
	else
		info->cmd_state = FAIL;

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);

}

static void get_chip_vendor(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", info->pdata->tsp_vendor);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", info->pdata->tsp_ic);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_reference(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->reference[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	info->cmd_state = OK;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

}

static void get_cm_abs(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->cm_abs[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_cm_delta(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->cm_delta[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_intensity(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->intensity[node];

	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_x_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	if (info->enabled)
		dev_info(&info->client->dev, "%s = [%d] from ic\n", __func__,
				i2c_smbus_read_byte_data(info->client, 0xEF));

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%d", info->pdata->tsp_tx);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buff[16] = {0};

	if (info->enabled)
		dev_info(&info->client->dev, "%s = [%d] from ic\n", __func__,
				i2c_smbus_read_byte_data(info->client, 0xEE));

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%d", info->pdata->tsp_rx);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = OK;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void run_reference_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	get_raw_data(info, MMS_VSC_CMD_REFER);
	info->cmd_state = OK;
}

static void run_cm_abs_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	get_raw_data(info, MMS_VSC_CMD_CM_ABS);
	info->cmd_state = OK;
}

static void run_cm_delta_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	get_raw_data(info, MMS_VSC_CMD_CM_DELTA);
	info->cmd_state = OK;
}

static void run_intensity_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	get_intensity_data(info);
	info->cmd_state = OK;
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
				  *devattr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;
	int ret;

	if (info->cmd_is_running == true) {
		dev_err(&info->client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}


	/* check lock  */
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = true;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = RUNNING;

	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
		info->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);

	cur = strnchr(buf, count, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				ret = kstrtoint(buff, 10,
						info->cmd_param + param_cnt);
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
							info->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(info);


err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	char buff[16] = {0};

	dev_info(&info->client->dev, "tsp cmd: status:%d\n",
			info->cmd_state);

	if (info->cmd_state == WAITING)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (info->cmd_state == RUNNING)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (info->cmd_state == OK)
		snprintf(buff, sizeof(buff), "OK");

	else if (info->cmd_state == FAIL)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (info->cmd_state == NOT_APPLICABLE)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
				    *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);

	dev_info(&info->client->dev, "tsp cmd: result: %s\n", info->cmd_result);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = WAITING;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}

static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_close_tsp_test.attr,
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};
#endif /* SEC_TSP_FACTORY_TEST */


static int melfas_power(struct mms_ts_info *info, int onoff)
{
	int rc;

	static struct regulator *vddo_vreg;
	static struct regulator *avdd_vreg;

	dev_info(&info->client->dev,
		"[TSP] %s called with ON= %d\n", __func__, onoff);

	if (!vddo_vreg) {
		vddo_vreg = regulator_get(&info->client->dev, "vddo");
		if (IS_ERR(vddo_vreg)) {
			vddo_vreg = NULL;
			dev_err(&info->client->dev,
				"[TSP] failed to get vddo regulator\n");
			return -ENODEV;
		}
	}

	if (!avdd_vreg) {
		avdd_vreg = regulator_get(&info->client->dev, "avdd");
		if (IS_ERR(avdd_vreg)) {
			avdd_vreg = NULL;
			dev_err(&info->client->dev,
				"[TSP] failed to get avdd regulator\n");
			return -ENODEV;
		}
		rc = regulator_set_voltage(avdd_vreg, 3300000, 3300000);
		if (rc) {
			dev_err(&info->client->dev,
				"[TSP] unable to set voltage for avdd_vreg\n");
			return rc;
		}
	}

	if (onoff) {
		if (regulator_is_enabled(vddo_vreg)) {
			dev_err(&info->client->dev,
				"[TSP] vddo is already enabled\n");
		} else {
			rc = regulator_enable(vddo_vreg);
			if (rc) {
				dev_err(&info->client->dev,
					"[TSP] unable to enable vddo\n");
				return rc;
			}
		}
		if (regulator_is_enabled(avdd_vreg)) {
			dev_err(&info->client->dev,
				"[TSP] avdd is already enabled\n");
		} else {
			rc = regulator_enable(avdd_vreg);
			if (rc) {
				dev_err(&info->client->dev,
					"[TSP] unable to enable avdd\n");
				return rc;
			}
		}
	} else {
		if (regulator_is_enabled(vddo_vreg)) {
			rc = regulator_disable(vddo_vreg);
			if (rc) {
				dev_err(&info->client->dev,
					"[TSP] unable to disable vddo\n");
				return rc;
			}
		} else {
			dev_err(&info->client->dev,
				"[TSP] vddo is already enabled\n");
		}
		if (regulator_is_enabled(vddo_vreg)) {
			rc = regulator_disable(avdd_vreg);
			if (rc) {
				dev_err(&info->client->dev,
					"[TSP] unable to disable avdd\n");
				return rc;
			}
		} else {
			dev_err(&info->client->dev,
				"[TSP] avdd is already enabled\n");
		}
	}
	usleep_range(100000, 100100);
	return 0;
}

static int melfas_parse_dt(struct device *dev,
			struct melfas_tsi_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	of_property_read_u32(np, "melfas,max_x", &pdata->max_x);
	of_property_read_u32(np, "melfas,max_y", &pdata->max_y);
	pdata->invert_x = of_property_read_bool(np, "melfas,invert_x");
	pdata->invert_y = of_property_read_bool(np, "melfas,invert_y");
	pdata->gpio_int = of_get_named_gpio(np, "melfas,gpio_int", 0);

	of_property_read_string(np, "melfas,tsp_vendor", &pdata->tsp_vendor);
	of_property_read_string(np, "melfas,tsp_ic", &pdata->tsp_ic);
	of_property_read_u32(np, "melfas,tsp_tx", &pdata->tsp_tx);
	of_property_read_u32(np, "melfas,tsp_rx", &pdata->tsp_rx);
	of_property_read_string(np, "melfas,config_fw_version",
		&pdata->config_fw_version);
	return 0;
}

void melfas_register_callback(void *cbv)
{
	pr_debug("[TSP] melfas_register_callback not implemented yet\n");
}

static int mms_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;

#if SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
	int rx_num;
	int tx_num;
#endif

	pr_info("[TSP] %s\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	info = kzalloc(sizeof(struct mms_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;

	if (client->dev.of_node) {
		info->pdata = devm_kzalloc(&client->dev,
			sizeof(struct melfas_tsi_platform_data), GFP_KERNEL);
		if (!info->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = melfas_parse_dt(&client->dev, info->pdata);
		if (ret) {
			dev_err(&client->dev, "Error parsing dt %d\n", ret);
			return ret;
		}

		info->pdata->register_cb = melfas_register_callback;
	} else {
		info->pdata = client->dev.platform_data;
	}

	if (NULL == info->pdata) {
		dev_err(&client->dev, "failed to get platform data\n");
		goto err_config;
	}

	info->irq = -1;
	mutex_init(&info->lock);

	if (info->pdata) {
		info->max_x = info->pdata->max_x;
		info->max_y = info->pdata->max_y;
		info->invert_x = info->pdata->invert_x;
		info->invert_y = info->pdata->invert_y;
		info->config_fw_version = info->pdata->config_fw_version;
		info->input_event = info->pdata->input_event;
		info->register_cb = info->pdata->register_cb;
	} else {
		info->max_x = 720;
		info->max_y = 1280;
	}
	for (i = 0; i < MAX_FINGERS; i++) {
		info->finger_state[i] = TSP_STATE_RELEASE;
		info->mcount[i] = 0;
	}

	melfas_power(info, 1);
	usleep_range(25000, 25100);

	i2c_set_clientdata(client, info);

	ret = mms_ts_fw_load(info);

	if (ret) {
		dev_err(&client->dev, "failed to initialize (%d)\n", ret);
		goto err_fw_init;
	}

	info->enabled = true;
	info->resume_done = true;

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;


	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);

	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, MAX_FINGERS, INPUT_MT_DIRECT);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, (info->max_x)-1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				0, (info->max_y)-1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,
				0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR,
				0, MAX_PRESSURE, 0, 0);
/*
	input_set_abs_params(input_dev, ABS_MT_ANGLE,
		     MIN_ANGLE, MAX_ANGLE, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PALM,
			0, 1, 0, 0);
*/
	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev (%d)\n",
			ret);
		goto err_reg_input_dev;
	}

#if !TSP_ENABLE_SW_RESET
	INIT_DELAYED_WORK(&info->work_config_set, work_mms_config_set);
#endif

#if TOUCH_BOOSTER
	mutex_init(&info->dvfs_lock);
	INIT_DELAYED_WORK(&info->work_dvfs_off, set_dvfs_off);
	INIT_DELAYED_WORK(&info->work_dvfs_chg, change_dvfs_lock);
	info->dvfs_lock_status = false;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	info->early_suspend.suspend = mms_ts_early_suspend;
	info->early_suspend.resume = mms_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	info->callbacks.inform_charger = melfas_ta_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);

	ret = request_threaded_irq(client->irq, NULL, mms_ts_interrupt,
				   IRQF_TRIGGER_LOW  | IRQF_ONESHOT,
				   MELFAS_TS_NAME, info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_reg_input_dev;
	}

	info->irq = client->irq;

	barrier();

	dev_info(&client->dev,
			"Melfas MMS-series touch controller initialized\n");

#if SEC_TSP_FACTORY_TEST
	rx_num = info->pdata->tsp_rx;
	tx_num = info->pdata->tsp_tx;

	info->reference = kzalloc(sizeof(int) * rx_num * tx_num, GFP_KERNEL);
	info->cm_abs = kzalloc(sizeof(int) * rx_num * tx_num, GFP_KERNEL);
	info->cm_delta = kzalloc(sizeof(int) * rx_num * tx_num, GFP_KERNEL);
	info->intensity = kzalloc(sizeof(int) * rx_num * tx_num, GFP_KERNEL);
	if (unlikely(info->reference == NULL ||
				info->cm_abs == NULL ||
				info->cm_delta == NULL ||
				info->intensity == NULL)) {
		ret = -ENOMEM;
		goto err_alloc_node_data_failed;
	}


	INIT_LIST_HEAD(&info->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &info->cmd_list_head);

	mutex_init(&info->cmd_lock);
	info->cmd_is_running = false;

	fac_dev_ts = device_create(sec_class,
			NULL, 0, info, "tsp");
	if (IS_ERR(fac_dev_ts))
		dev_err(&client->dev, "Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&fac_dev_ts->kobj,
				 &sec_touch_factory_attr_group);
	if (ret)
		dev_err(&client->dev, "Failed to create sysfs group\n");
#endif	/* SEC_TSP_FACTORY_TEST */

	pr_info("[TSP] %s done\n", __func__);

	return 0;

#if SEC_TSP_FACTORY_TEST
err_alloc_node_data_failed:
	dev_err(&client->dev, "Err_alloc_node_data failed\n");
	kfree(info->reference);
	kfree(info->cm_abs);
	kfree(info->cm_delta);
	kfree(info->intensity);
#endif

err_reg_input_dev:
	input_unregister_device(input_dev);
	input_free_device(input_dev);
err_fw_init:
	melfas_power(info, 0);
err_config:
err_input_alloc:
err_alloc:
	kfree(info);
	pr_err("[TSP] %s failed\n", __func__);
	return ret;

}

static int mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);

#if SEC_TSP_FACTORY_TEST
	dev_err(&client->dev, "Err_alloc_node_data failed\n");
	kfree(info->reference);
	kfree(info->cm_abs);
	kfree(info->cm_delta);
	kfree(info->intensity);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif
	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int mms_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	int retries = 50;

	dev_info(&info->client->dev, "%s %s\n", __func__,
		info->enabled ? "ON" : "OFF");

	while (!info->resume_done) {
		if (!retries--)
			break;
		usleep_range(10000, 10100);
	}

	if (!info->enabled)
		return 0;

	disable_irq(info->irq);

	info->enabled = false;
	release_all_fingers(info);

#if TSP_ENABLE_SW_RESET
	/* sw_power off */
	i2c_smbus_write_byte_data(info->client, MMS_MODE_CONTROL, 0);
	usleep_range(10000, 12000);
#else
	melfas_power(info, 0);
#endif

	return 0;
}

static int mms_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	dev_info(&info->client->dev, "%s %s\n", __func__,
		info->enabled ? "ON" : "OFF");

	if (info->enabled)
		return 0;

#if TSP_ENABLE_SW_RESET
	/* sw_power on */
	gpio_direction_output(info->pdata->gpio_int, 0);
	usleep_range(1000, 1100);
	gpio_direction_input(info->pdata->gpio_int);
#else
	melfas_power(info, 1);
	info->resume_done = false;
	schedule_delayed_work(&info->work_config_set,
					msecs_to_jiffies(40));
#endif

#if TSP_ENABLE_SW_RESET
	enable_irq(info->irq);
	info->enabled = true;
#endif

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_suspend(&info->client->dev);
}

static void mms_ts_late_resume(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops mms_ts_pm_ops = {
	.suspend = mms_ts_suspend,
	.resume = mms_ts_resume,
};
#endif

static const struct i2c_device_id mms_ts_id[] = {
	{MELFAS_TS_NAME, 0},
};

MODULE_DEVICE_TABLE(i2c, mms_ts_id);

#ifdef CONFIG_OF
static struct of_device_id melfas_match_table[] = {
	{.compatible = "melfas,mms128s",},
};
#else
#define melfas_match_table NULL
#endif

static struct i2c_driver mms_ts_driver = {
	.probe = mms_ts_probe,
	.remove = mms_ts_remove,
	.driver = {
		.name = MELFAS_TS_NAME,
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm = &mms_ts_pm_ops,
#endif
		.of_match_table = melfas_match_table,
		},
	.id_table = mms_ts_id,
};

static int __init mms_ts_init(void)
{
	pr_info("[TSP] %s\n", __func__);
	return i2c_add_driver(&mms_ts_driver);

}

static void __exit mms_ts_exit(void)
{
	i2c_del_driver(&mms_ts_driver);
}

module_init(mms_ts_init);
module_exit(mms_ts_exit);

/* Module information */
MODULE_DESCRIPTION("Touchscreen driver for Melfas MMS-series controllers");
MODULE_LICENSE("GPL");
