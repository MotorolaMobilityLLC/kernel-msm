/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/input/focaltech_mmi.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#if defined(CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_MMI)
#include "focaltech_flash.h"
#endif


#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
/* Early-suspend level */
#define FT_SUSPEND_LEVEL 1
#endif

#if defined(CONFIG_TOUCHSCREEN_FOCALTECH_SECURE_TOUCH_MMI)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
static irqreturn_t ft_ts_interrupt(int irq, void *data);
#endif

#include <linux/usb.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>

#define FT_DRIVER_VERSION	0x02

#define FT_META_REGS		3
#define FT_ONE_TCH_LEN		6
#define FT_TCH_LEN(x)		(FT_META_REGS + FT_ONE_TCH_LEN * x)

#define FT_PRESS		0x7F
#define FT_MAX_ID		0x0F
#define FT_TOUCH_X_H_POS	3
#define FT_TOUCH_X_L_POS	4
#define FT_TOUCH_Y_H_POS	5
#define FT_TOUCH_Y_L_POS	6
#define FT_TD_STATUS		2
#define FT_TOUCH_EVENT_POS	3
#define FT_TOUCH_ID_POS		5
#define FT_TOUCH_DOWN		0
#define FT_TOUCH_CONTACT	2

/* register address*/
#define FT_REG_DEV_MODE		0x00
#define FT_DEV_MODE_REG_CAL	0x02
#define FT_REG_FW_ID		0xA1
#define FT_REG_ID		0xA3
#define FT_REG_PMODE		0xA5
#define FT_REG_FW_VER		0xA6
#define FT_REG_FW_VENDOR_ID	0xA8
#define FT_REG_POINT_RATE	0x88
#define FT_REG_THGROUP		0x80
#define FT_REG_ECC		0xCC
#define FT_REG_RESET_FW		0x07
#define FT_REG_FW_MIN_VER	0xB2
#define FT_REG_FW_SUB_MIN_VER	0xB3
#define FT_REG_I2C_MODE		0xEB
#define FT_REG_FW_LEN		0xB0
#define FT_REG_CHG_STATE	0x8B

/* i2c mode register value */
#define FT_VAL_I2C_MODE		0xAA
#define FT_VAL_I2C_MODE_STD	0x09
#define FT_VAL_I2C_MODE_STD_CFM	0x08

/* gesture register address*/
#define FT_REG_GESTURE_ENABLE	0xD0
#define FT_REG_GESTURE_OUTPUT	0xD3

/* gesture register bits*/
#define FT_GESTURE_DOUBLECLICK_COORD_X		100
#define FT_GESTURE_DOUBLECLICK_COORD_Y		100
#define FT_GESTURE_WAKEUP_TIMEOUT		500
#define FT_GESTURE_DEFAULT_TRACKING_ID		0x0A
#define FT_GESTURE_DOUBLECLICK_ID		0x24
#define FT_GESTURE_POINTER_NUM_MAX		128
#define FT_GESTURE_POINTER_SIZEOF		4
#define FT_GESTURE_ID_FLAG_SIZE			1
#define FT_GESTURE_POINTER_NUM_FLAG_SIZE	1
/* 6 bytes are taken to mark which gesture is supported in firmware */
#define FT_GESTURE_SET_FLAG_SIZE		6
#define I2C_TRANSFER_MAX_BYTE			255
#define FT_GESTURE_DATA_HEADER	(FT_GESTURE_ID_FLAG_SIZE + \
				FT_GESTURE_POINTER_NUM_FLAG_SIZE + \
				FT_GESTURE_SET_FLAG_SIZE)

/* power register bits*/
#define FT_PMODE_ACTIVE		0x00
#define FT_PMODE_MONITOR	0x01
#define FT_PMODE_STANDBY	0x02
#define FT_PMODE_HIBERNATE	0x03
#define FT_FACTORYMODE_VALUE	0x40
#define FT_WORKMODE_VALUE	0x00
#define FT_RST_CMD_REG1		0xFC
#define FT_RST_CMD_REG2		0xBC
#define FT_READ_ID_REG		0x90
#define FT_ERASE_APP_REG	0x61
#define FT_ERASE_PANEL_REG	0x63
#define FT_FW_START_REG		0xBF

#define FT_STATUS_NUM_TP_MASK	0x0F

#define FT_VTG_MIN_UV		2600000
#define FT_VTG_MAX_UV		3300000
#define FT_I2C_VTG_MIN_UV	1800000
#define FT_I2C_VTG_MAX_UV	1800000

#define FT_COORDS_ARR_SIZE	4
#define MAX_BUTTONS		4

#define FT_8BIT_SHIFT		8
#define FT_4BIT_SHIFT		4
#define FT_FW_NAME_MAX_LEN	50

#define FT5316_ID		0x0A
#define FT5306I_ID		0x55
#define FT6X06_ID		0x06
#define FT6X36_ID		0x36

#define FT_UPGRADE_AA		0xAA
#define FT_UPGRADE_55		0x55

#define FT_FW_MIN_SIZE		8
#define FT_FW_MAX_SIZE		65536

/* Firmware file is not supporting minor and sub minor so use 0 */
#define FT_FW_FILE_MAJ_VER(x)	((x)->data[(x)->size - 2])
#define FT_FW_FILE_MIN_VER(x)	0
#define FT_FW_FILE_SUB_MIN_VER(x) 0
#define FT_FW_FILE_VENDOR_ID(x)	((x)->data[(x)->size - 1])

#define FT_FW_FILE_MAJ_VER_FT6X36(x)	((x)->data[0x10a])
#define FT_FW_FILE_VENDOR_ID_FT6X36(x)	((x)->data[0x108])

/**
* Application data verification will be run before upgrade flow.
* Firmware image stores some flags with negative and positive value
* in corresponding addresses, we need pick them out do some check to
* make sure the application data is valid.
*/
#define FT_FW_CHECK(x, ts_data) \
	(ts_data->family_id == FT6X36_ID ? \
	(((x)->data[0x104] ^ (x)->data[0x105]) == 0xFF \
	&& ((x)->data[0x106] ^ (x)->data[0x107]) == 0xFF) : \
	(((x)->data[(x)->size - 8] ^ (x)->data[(x)->size - 6]) == 0xFF \
	&& ((x)->data[(x)->size - 7] ^ (x)->data[(x)->size - 5]) == 0xFF \
	&& ((x)->data[(x)->size - 3] ^ (x)->data[(x)->size - 4]) == 0xFF))

#define FT_MAX_TRIES		5
#define FT_RETRY_DLY		20

#define FT_MAX_WR_BUF		10
#define FT_MAX_RD_BUF		3
#define FT_FW_PKT_LEN		128
#define FT_FW_PKT_META_LEN	6
#define FT_FW_PKT_DLY_MS	20
#define FT_FW_LAST_PKT		0x6ffa
#define FT_EARSE_DLY_MS		100
#define FT_55_AA_DLY_NS		5000

#define FT_UPGRADE_LOOP		30
#define FT_CAL_START		0x04
#define FT_CAL_FIN		0x00
#define FT_CAL_STORE		0x05
#define FT_CAL_RETRY		100
#define FT_REG_CAL		0x00
#define FT_CAL_MASK		0x70

#define FT_INFO_MAX_LEN		512

#define FT_BLOADER_SIZE_OFF	12
#define FT_BLOADER_NEW_SIZE	30
#define FT_DATA_LEN_OFF_OLD_FW	8
#define FT_DATA_LEN_OFF_NEW_FW	14
#define FT_FINISHING_PKT_LEN_OLD_FW	6
#define FT_FINISHING_PKT_LEN_NEW_FW	12
#define FT_MAGIC_BLOADER_Z7	0x7bfa
#define FT_MAGIC_BLOADER_LZ4	0x6ffa
#define FT_MAGIC_BLOADER_GZF_30	0x7ff4
#define FT_MAGIC_BLOADER_GZF	0x7bf4

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

#define EXP_FN_DET_INTERVAL 1000 /* ms */

static irqreturn_t ft_ts_interrupt(int irq, void *data);

enum {
	FT_BLOADER_VERSION_LZ4 = 0,
	FT_BLOADER_VERSION_Z7 = 1,
	FT_BLOADER_VERSION_GZF = 2,
};

enum {
	FT_FT5336_FAMILY_ID_0x11 = 0x11,
	FT_FT5336_FAMILY_ID_0x12 = 0x12,
	FT_FT5336_FAMILY_ID_0x13 = 0x13,
	FT_FT5336_FAMILY_ID_0x14 = 0x14,
};

#define FT_STORE_TS_INFO(buf, id, fw_maj, fw_min, fw_sub_min) \
			snprintf(buf, FT_INFO_MAX_LEN, \
				"vendor name = Focaltech\n" \
				"model = 0x%x\n" \
				"fw_version = %d.%d.%d\n", \
				id, fw_maj, fw_min, fw_sub_min)
#define FT_TS_INFO_SYSFS_DIR_NAME "ts_info"
static char *ts_info_buff;

#define FT_STORE_TS_DBG_INFO(buf, id, name, max_tch, group_id, \
			fw_vkey_support, fw_name, fw_maj, fw_min, fw_sub_min) \
			snprintf(buf, FT_INFO_MAX_LEN, \
				"controller\t= focaltech\n" \
				"model\t\t= 0x%x\n" \
				"name\t\t= %s\n" \
				"max_touches\t= %d\n" \
				"drv_ver\t\t= 0x%x\n" \
				"group_id\t= 0x%x\n" \
				"fw_vkey_support\t= %s\n" \
				"fw_name\t\t= %s\n" \
				"fw_ver\t\t= %d.%d.%d\n", id, name, \
				max_tch, FT_DRIVER_VERSION, group_id, \
				fw_vkey_support, fw_name, fw_maj, fw_min, \
				fw_sub_min)

#define FT_DEBUG_DIR_NAME	"ts_debug"
#define FT_FW_ID_LEN		2

#define FT_PROC_DIR_NAME	"ftxxxx-debug"

enum {
	PROC_UPGRADE		= 0,
	PROC_READ_REGISTER	= 1,
	PROC_WRITE_REGISTER	= 2,
	PROC_WRITE_DATA		= 6,
	PROC_READ_DATA		= 7
};

struct ft_clip_area {
	unsigned xul_clip, yul_clip, xbr_clip, ybr_clip;
	unsigned inversion; /* clip inside (when 1) or ouside otherwise */
};

enum {
	FT_MOD_CHARGER,
	FT_MOD_FPS,
	FT_MOD_MAX
};

struct ft_modifiers {
	int	mods_num;
	struct semaphore list_sema;
	struct list_head mod_head;

};

struct config_modifier {
	const char *name;
	int id;
	bool effective;
	struct ft_clip_area *clipa;
	struct list_head link;
};

struct ft_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct ft_ts_platform_data *pdata;
	struct ft_gesture_platform_data *gesture_pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	bool regulator_en;
	struct mutex ft_clk_io_ctrl_mutex;
	char fw_name[FT_FW_NAME_MAX_LEN];
	bool loading_fw;
	u8 family_id;
	struct dentry *dir;
	u16 addr;
	bool suspended;
	char *ts_info;
	u8 *tch_data;
	u32 tch_data_len;
	u8 fw_ver[3];
	u8 fw_vendor_id;
	const char *panel_supplier;
	struct kobject *ts_info_kobj;
#if defined(CONFIG_FB)
	struct work_struct fb_notify_work;
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
#if defined(CONFIG_TOUCHSCREEN_FOCALTECH_SECURE_TOUCH_MMI)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_irq_processed;
	bool st_initialized;
	struct clk *core_clk;
	struct clk *iface_clk;
#endif
	bool flash_enabled;
	bool force_reflash;
	u8 fw_id[FT_FW_ID_LEN];
	bool irq_enabled;
	struct proc_dir_entry *proc_entry;
	u8 proc_operate_mode;
	struct notifier_block ft_reboot;
	/* moto TP modifiers */
	bool patching_enabled;
	struct ft_modifiers modifiers;
	bool charger_detection_enabled;
	struct work_struct ps_notify_work;
	struct notifier_block ps_notif;
	bool ps_is_present;

	bool fps_detection_enabled;
	bool is_fps_registered;	/* FPS notif registration might be delayed */
	struct notifier_block fps_notif;

	bool clipping_on;
	struct ft_clip_area *clipa;
};

struct ft_exp_fn {
	bool inserted;
	int (*func_init)(struct ft_ts_data *data);
	void (*func_remove)(struct ft_ts_data *data);
	void (*func_attn)(struct ft_ts_data *data, unsigned char intr_mask);
	struct list_head link;
};

struct ft_exp_fn_ctrl {
	bool inited;
	struct mutex list_mutex;
	struct list_head fn_list;
	struct delayed_work det_work;
	struct workqueue_struct *det_workqueue;
	struct ft_ts_data *data_ptr;
};
static DEFINE_MUTEX(exp_fn_ctrl_mutex);
static struct ft_exp_fn_ctrl exp_fn_ctrl;


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

static struct touch_up_down display_ud[20];
static struct touch_area_stats display_ud_stats = {
	.ud = display_ud,
	.ud_len = ARRAY_SIZE(display_ud),
	.name = "ts"
};

__attribute__((weak))
int FPS_register_notifier(struct notifier_block *nb, unsigned long stype,
			bool report)
{
	return -ENODEV;
}

__attribute__((weak))
int FPS_unregister_notifier(struct notifier_block *nb, unsigned long stype)
{
	return -ENODEV;
}

static void ud_set_id(struct touch_area_stats *tas, int id)
{
	tas->ud_id = id;
}

static void ud_log_status(struct touch_area_stats *tas, bool down)
{
	struct touch_up_down *ud = tas->ud;
	ssize_t id = tas->ud_id;

	if (id >= tas->ud_len)
		tas->unknown_counter++;

	if (!down) { /* up */
		if (ud[id].up_down == 0x10) {
			pr_debug("%s UP[%zu]\n", tas->name, id);
			ud[id].up_down |= 1;
			ud[id].mismatch--;
		}
	} else if (down) { /* down */
		if (ud[id].up_down == 0) {
			ud[id].up_down |= (1 << 4);
			pr_debug("%s DOWN[%zu]\n", tas->name, id);
			ud[id].mismatch++;
		} else if (ud[id].up_down == 0x10)
			return;
	}

	if (ud[id].up_down == 0x11) {
		pr_debug("%s CLEAR[%zu]\n", tas->name, id);
		ud[id].up_down = 0;
		ud[id].counter++;
	}
}

static void TSI_state(struct input_dev *dev, unsigned int tool, bool status)
{
	ud_log_status(&display_ud_stats, status);
	input_mt_report_slot_state(dev, tool, status);
}

static void TSI_id(struct input_dev *dev, int id)
{
	ud_set_id(&display_ud_stats, id);
	input_mt_slot(dev, id);
}

#define input_mt_report_slot_state TSI_state
#define input_mt_slot TSI_id

static int ft_ts_start(struct device *dev);
static int ft_ts_stop(struct device *dev);
static void ft_resume_ps_chg_cm_state(struct ft_ts_data *data);
static struct config_modifier *ft_modifier_by_id(
	struct ft_ts_data *data, int id);

#if defined(CONFIG_TOUCHSCREEN_FOCALTECH_SECURE_TOUCH_MMI)
static void ft_secure_touch_init(struct ft_ts_data *data)
{
	data->st_initialized = 0;

	init_completion(&data->st_powerdown);
	init_completion(&data->st_irq_processed);

	/* Get clocks */
	data->core_clk = devm_clk_get(&data->client->dev, "core_clk");
	if (IS_ERR(data->core_clk)) {
		data->core_clk = NULL;
		dev_warn(&data->client->dev,
			"%s: core_clk is not defined\n", __func__);
	}

	data->iface_clk = devm_clk_get(&data->client->dev, "iface_clk");
	if (IS_ERR(data->iface_clk)) {
		data->iface_clk = NULL;
		dev_warn(&data->client->dev,
			"%s: iface_clk is not defined", __func__);
	}
	data->st_initialized = 1;
}

static void ft_secure_touch_notify(struct ft_ts_data *data)
{
	sysfs_notify(&data->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t ft_filter_interrupt(struct ft_ts_data *data)
{
	if (atomic_read(&data->st_enabled)) {
		if (atomic_cmpxchg(&data->st_pending_irqs, 0, 1) == 0) {
			reinit_completion(&data->st_irq_processed);
			ft_secure_touch_notify(data);
			wait_for_completion_interruptible(
						&data->st_irq_processed);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

/*
 * 'blocking' variable will have value 'true' when we want to prevent the driver
 * from accessing the xPU/SMMU protected HW resources while the session is
 * active.
 */
static void ft_secure_touch_stop(struct ft_ts_data *data, bool blocking)
{
	if (atomic_read(&data->st_enabled)) {
		atomic_set(&data->st_pending_irqs, -1);
		ft_secure_touch_notify(data);
		if (blocking)
			wait_for_completion_interruptible(
						&data->st_powerdown);
	}
}

static int ft_clk_prepare_enable(struct ft_ts_data *data)
{
	int ret;

	ret = clk_prepare_enable(data->iface_clk);
	if (ret) {
		dev_err(&data->client->dev,
			"error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(data->core_clk);
	if (ret) {
		clk_disable_unprepare(data->iface_clk);
		dev_err(&data->client->dev,
			"error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void ft_clk_disable_unprepare(struct ft_ts_data *data)
{
	clk_disable_unprepare(data->core_clk);
	clk_disable_unprepare(data->iface_clk);
}

static int ft_bus_get(struct ft_ts_data *data)
{
	int retval;

	mutex_lock(&data->ft_clk_io_ctrl_mutex);
	retval = pm_runtime_get_sync(data->client->adapter->dev.parent);
	if (retval >= 0 &&  data->core_clk != NULL && data->iface_clk != NULL) {
		retval = ft_clk_prepare_enable(data);
		if (retval)
			pm_runtime_put_sync(data->client->adapter->dev.parent);
	}
	mutex_unlock(&data->ft_clk_io_ctrl_mutex);
	return retval;
}

static void ft_bus_put(struct ft_ts_data *data)
{
	mutex_lock(&data->ft_clk_io_ctrl_mutex);
	if (data->core_clk != NULL && data->iface_clk != NULL)
		ft_clk_disable_unprepare(data);
	pm_runtime_put_sync(data->client->adapter->dev.parent);
	mutex_unlock(&data->ft_clk_io_ctrl_mutex);
}

static ssize_t ft_secure_touch_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&data->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t ft_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;
	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!data->st_initialized)
		return -EIO;

	err = count;
	switch (value) {
	case 0:
		if (atomic_read(&data->st_enabled) == 0)
			break;
		ft_bus_put(data);
		atomic_set(&data->st_enabled, 0);
		ft_secure_touch_notify(data);
		complete(&data->st_irq_processed);
		ft_ts_interrupt(data->client->irq, data);
		complete(&data->st_powerdown);
		break;

	case 1:
		if (atomic_read(&data->st_enabled)) {
			err = -EBUSY;
			break;
		}
		synchronize_irq(data->client->irq);
		if (ft_bus_get(data) < 0) {
			dev_err(&data->client->dev, "ft_bus_get failed\n");
			err = -EIO;
			break;
		}
		reinit_completion(&data->st_powerdown);
		reinit_completion(&data->st_irq_processed);
		atomic_set(&data->st_enabled, 1);
		atomic_set(&data->st_pending_irqs,  0);
		break;

	default:
		dev_err(&data->client->dev, "unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

/*
 * This function returns whether there are pending interrupts, or
 * other error conditions that need to be signaled to the userspace library,
 * according tot he following logic:
 * - st_enabled is 0 if secure touch is not enabled, returning -EBADF
 * - st_pending_irqs is -1 to signal that secure touch is in being stopped,
 *   returning -EINVAL
 * - st_pending_irqs is 1 to signal that there is a pending irq, returning
 *   the value "1" to the sysfs read operation
 * - st_pending_irqs is 0 (only remaining case left) if the pending interrupt
 *   has been processed, so the interrupt handler can be allowed to continue.
 */
static ssize_t ft_secure_touch_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&data->st_enabled) == 0)
		return -EBADF;
	if (atomic_cmpxchg(&data->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;
	if (atomic_cmpxchg(&data->st_pending_irqs, 1, 0) == 1)
		val = 1;
	else
		complete(&data->st_irq_processed);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
#else
static void ft_secure_touch_init(struct ft_ts_data *data)
{
}
static irqreturn_t ft_filter_interrupt(struct ft_ts_data *data)
{
	return IRQ_NONE;
}
static void ft_secure_touch_stop(struct ft_ts_data *data, bool blocking)
{
}
#endif

static struct device_attribute st_attrs[] = {
#if defined(CONFIG_TOUCHSCREEN_FOCALTECH_SECURE_TOUCH_MMI)
		__ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
				ft_secure_touch_enable_show,
				ft_secure_touch_enable_store),
		__ATTR(secure_touch, S_IRUGO,
				ft_secure_touch_show, NULL),
#endif
};

static int ft_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int ft_i2c_write(struct i2c_client *client, char *writebuf,
			    int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

static int ft_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return ft_i2c_write(client, buf, sizeof(buf));
}

static int ft_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return ft_i2c_read(client, &addr, 1, val, 1);
}

static void ft_irq_disable(struct ft_ts_data *data)
{
	if (data->irq_enabled) {
		disable_irq(data->client->irq);
		data->irq_enabled = false;
	}
}

static void ft_irq_enable(struct ft_ts_data *data)
{
	if (!data->irq_enabled) {
		enable_irq(data->client->irq);
		data->irq_enabled = true;
	}
}

static const char *ft_find_vendor_name(
	const struct ft_ts_platform_data *pdata, u8 id)
{
	int i;

	if (pdata->num_vendor_ids == 0)
		return NULL;

	for (i = 0; i < pdata->num_vendor_ids; i++)
		if (id == pdata->vendor_ids[i]) {
			pr_info("vendor id 0x%02x panel supplier is %s\n",
				id, pdata->vendor_names[i]);
			return pdata->vendor_names[i];
		}

	return NULL;
}

static void ft_update_fw_vendor_id(struct ft_ts_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr;
	int err;

	reg_addr = FT_REG_FW_VENDOR_ID;
	err = ft_i2c_read(client, &reg_addr, 1, &data->fw_vendor_id, 1);
	if (err < 0)
		dev_err(&client->dev, "fw vendor id read failed");
	data->panel_supplier = ft_find_vendor_name(data->pdata,
		data->fw_vendor_id);
}

static void ft_update_fw_ver(struct ft_ts_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr;
	int err;

	reg_addr = FT_REG_FW_VER;
	err = ft_i2c_read(client, &reg_addr, 1, &data->fw_ver[0], 1);
	if (err < 0)
		dev_err(&client->dev, "fw major version read failed");

	reg_addr = FT_REG_FW_MIN_VER;
	err = ft_i2c_read(client, &reg_addr, 1, &data->fw_ver[1], 1);
	if (err < 0)
		dev_err(&client->dev, "fw minor version read failed");

	reg_addr = FT_REG_FW_SUB_MIN_VER;
	err = ft_i2c_read(client, &reg_addr, 1, &data->fw_ver[2], 1);
	if (err < 0)
		dev_err(&client->dev, "fw sub minor version read failed");

	dev_info(&client->dev, "Firmware version = %d.%d.%d\n",
		data->fw_ver[0], data->fw_ver[1], data->fw_ver[2]);
}

static void ft_update_fw_id(struct ft_ts_data *data)
{
	struct i2c_client *client = data->client;
	u8 reg_addr;
	int err;

	reg_addr = FT_REG_FW_ID;
	err = ft_i2c_read(client, &reg_addr, 1,
		&data->fw_id[0], FT_FW_ID_LEN);
	if (err < 0)
		dev_err(&client->dev, "fw id read failed");

	dev_info(&client->dev, "Firmware id = 0x%02x%02x\n",
		data->fw_id[0], data->fw_id[1]);
}

static void ft_ud_fix_mismatch(struct ft_ts_data *data)
{
	int i;
	struct input_dev *ip_dev = data->input_dev;

	for (i = 0; i < display_ud_stats.ud_len; i++) {
		if (display_ud[i].mismatch) {
			pr_debug("%s MISMATCH[%d]\n", display_ud_stats.name, i);
			input_mt_slot(ip_dev, i);
			input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
		}
	}
}

static irqreturn_t ft_ts_interrupt(int irq, void *dev_id)
{
	struct ft_ts_data *data = dev_id;
	struct input_dev *ip_dev;
	int rc, i;
	u32 id, x, y, status, num_touches;
	u8 reg, *buf;
	bool update_input = false;

	if (!data) {
		pr_err("%s: Invalid data\n", __func__);
		return IRQ_HANDLED;
	}

	if (ft_filter_interrupt(data) == IRQ_HANDLED)
		return IRQ_HANDLED;

	ip_dev = data->input_dev;
	buf = data->tch_data;

	/*
	 * Read touch data start from register FT_REG_DEV_MODE.
	 * The touch x/y value start from FT_TOUCH_X_H/L_POS and
	 * FT_TOUCH_Y_H/L_POS in buf.
	 */
	reg = FT_REG_DEV_MODE;
	rc = ft_i2c_read(data->client, &reg, 1, buf, data->tch_data_len);
	if (rc < 0) {
		dev_err(&data->client->dev, "%s: read data fail\n", __func__);
		return IRQ_HANDLED;
	}

	for (i = 0; i < data->pdata->num_max_touches; i++) {
		/*
		 * Getting the finger ID of the touch event incase of
		 * multiple touch events
		 */
		id = (buf[FT_TOUCH_ID_POS + FT_ONE_TCH_LEN * i]) >> 4;
		if (id >= FT_MAX_ID)
			break;

		update_input = true;

		x = (buf[FT_TOUCH_X_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
			(buf[FT_TOUCH_X_L_POS + FT_ONE_TCH_LEN * i]);
		y = (buf[FT_TOUCH_Y_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
			(buf[FT_TOUCH_Y_L_POS + FT_ONE_TCH_LEN * i]);

		if (data->pdata->x_flip)
			x = data->pdata->x_max - x;
		if (data->pdata->y_flip)
			y = data->pdata->y_max - y;

		status = buf[FT_TOUCH_EVENT_POS + FT_ONE_TCH_LEN * i] >> 6;

		num_touches = buf[FT_TD_STATUS] & FT_STATUS_NUM_TP_MASK;

		/* invalid combination */
		if (!num_touches && !status && !id)
			break;

		if (data->clipping_on && data->clipa) {
			bool inside;

			inside = (x >= data->clipa->xul_clip) &&
				(x <= data->clipa->xbr_clip) &&
				(y >= data->clipa->yul_clip) &&
				(y <= data->clipa->ybr_clip);

			if (inside == data->clipa->inversion) {
				/* Touch might still be active, but we're */
				/* sending release anyway to avoid touch  */
				/* stuck at the last reported position.   */
				/* Driver will suppress reporting to UI   */
				/* touch events from within clipping area */
				status = 1;

				dev_dbg(&data->client->dev,
					"finger id-%d (%d,%d) within clipping area\n",
					id, x, y);
			}
		}

		input_mt_slot(ip_dev, id);
		if (status == FT_TOUCH_DOWN || status == FT_TOUCH_CONTACT) {
			input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 1);
			input_report_abs(ip_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ip_dev, ABS_MT_POSITION_Y, y);
		} else {
			input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
		}
	}

	/* report up event if FW missed to do */
	if (num_touches == 0)
		ft_ud_fix_mismatch(data);

	if (update_input) {
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
	}

	return IRQ_HANDLED;
}

static int ft_gpio_configure(struct ft_ts_data *data, bool on)
{
	int err = 0;

	if (on) {
		if (gpio_is_valid(data->pdata->irq_gpio)) {
			err = gpio_request(data->pdata->irq_gpio,
						"ft_irq_gpio");
			if (err) {
				dev_err(&data->client->dev,
					"irq gpio request failed");
				goto err_irq_gpio_req;
			}

			err = gpio_direction_input(data->pdata->irq_gpio);
			if (err) {
				dev_err(&data->client->dev,
					"set_direction for irq gpio failed\n");
				goto err_irq_gpio_dir;
			}
		}

		if (gpio_is_valid(data->pdata->reset_gpio)) {
			err = gpio_request(data->pdata->reset_gpio,
						"ft_reset_gpio");
			if (err) {
				dev_err(&data->client->dev,
					"reset gpio request failed");
				goto err_irq_gpio_dir;
			}

			err = gpio_direction_output(data->pdata->reset_gpio, 0);
			if (err) {
				dev_err(&data->client->dev,
				"set_direction for reset gpio failed\n");
				goto err_reset_gpio_dir;
			}
			msleep(data->pdata->hard_rst_dly);
			gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
		}

		return 0;
	}
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

		return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	return err;
}

static int ft_power_on(struct ft_ts_data *data, bool on)
{
	int rc;

	if (!on)
		goto power_off;

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}

	return rc;

power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
		}
	}

	return rc;
}

static int ft_power_init(struct ft_ts_data *data, bool on)
{
	int rc;

	if (!on)
		goto pwr_deinit;

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->client->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FT_VTG_MIN_UV,
					   FT_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		dev_err(&data->client->dev,
			"Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FT_I2C_VTG_MIN_UV,
					   FT_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
			"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;

pwr_deinit:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);

	regulator_put(data->vdd);

	if (regulator_count_voltages(data->vcc_i2c) > 0)
		regulator_set_voltage(data->vcc_i2c, 0, FT_I2C_VTG_MAX_UV);

	regulator_put(data->vcc_i2c);
	return 0;
}

static int ft_ts_pinctrl_init(struct ft_ts_data *data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	data->ts_pinctrl = devm_pinctrl_get(&(data->client->dev));
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		retval = PTR_ERR(data->ts_pinctrl);
		dev_dbg(&data->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	data->pinctrl_state_active
		= pinctrl_lookup_state(data->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_active)) {
		retval = PTR_ERR(data->pinctrl_state_active);
		dev_err(&data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_suspend
		= pinctrl_lookup_state(data->ts_pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(data->pinctrl_state_suspend)) {
		retval = PTR_ERR(data->pinctrl_state_suspend);
		dev_err(&data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_release
		= pinctrl_lookup_state(data->ts_pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
		retval = PTR_ERR(data->pinctrl_state_release);
		dev_dbg(&data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(data->ts_pinctrl);
err_pinctrl_get:
	data->ts_pinctrl = NULL;
	return retval;
}

static ssize_t ft_ud_stat(struct ft_ts_data *data, char *buf, ssize_t size)
{
	int i;
	ssize_t total = 0;

	total += scnprintf(buf + total, size - total, "screen: ");
	for (i = 0; i < display_ud_stats.ud_len; i++)
		if (display_ud[i].mismatch)
			total += scnprintf(buf + total, size - total,
				"%d)%u,%d ", i,
				display_ud[i].counter,
				display_ud[i].mismatch);
		else if (display_ud[i].counter)
			total += scnprintf(buf + total, size - total,
				"%d)%u ", i,
				display_ud[i].counter);
	return total;
}

#ifdef CONFIG_PM
static int ft_ts_start(struct device *dev)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	int err;

	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
				data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Cannot get active pinctrl state\n");
	}

	err = ft_gpio_configure(data, true);
	if (err < 0) {
		dev_err(&data->client->dev,
			"failed to put gpios in resue state\n");
		goto err_gpio_configuration;
	}

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		usleep_range(800, 1000);
		if (data->regulator_en) {
			if (data->pdata->power_on) {
				err = data->pdata->power_on(true);
				if (err) {
					dev_err(dev, "power on failed");
					return err;
				}
			} else {
				err = ft_power_on(data, true);
				if (err) {
					dev_err(dev, "power on failed");
					return err;
				}
			}
		}
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}

	msleep(data->pdata->soft_rst_dly);

	ft_irq_enable(data);
	data->suspended = false;

	return 0;

err_gpio_configuration:
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_suspend);
		if (err < 0)
			dev_err(dev, "Cannot get suspend pinctrl state\n");
	}
	return err;
}

static int ft_ts_stop(struct device *dev)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	char txbuf[2];
	static char ud_stats[PAGE_SIZE];
	int i, err;

	ft_irq_disable(data);

	/* print UD statistics */
	ft_ud_stat(data, ud_stats, sizeof(ud_stats));
	pr_info("%s\n", ud_stats);
	/* release all touches */
	for (i = 0; i < data->pdata->num_max_touches; i++) {
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
	}
	input_mt_report_pointer_emulation(data->input_dev, false);
	input_sync(data->input_dev);

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		txbuf[0] = FT_REG_PMODE;
		txbuf[1] = FT_PMODE_HIBERNATE;
		ft_i2c_write(data->client, txbuf, sizeof(txbuf));
	}

	if (data->regulator_en) {
		if (data->pdata->power_on) {
			err = data->pdata->power_on(false);
			if (err) {
				dev_err(dev, "power off failed");
				goto pwr_off_fail;
			}
		} else {
			err = ft_power_on(data, false);
			if (err) {
				dev_err(dev, "power off failed");
				goto pwr_off_fail;
			}
		}
	}

	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_suspend);
		if (err < 0)
			dev_err(dev, "Cannot get suspend pinctrl state\n");
	}

	err = ft_gpio_configure(data, false);
	if (err < 0) {
		dev_err(&data->client->dev,
			"failed to put gpios in suspend state\n");
		goto gpio_configure_fail;
	}

	data->suspended = true;

	return 0;

gpio_configure_fail:
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Cannot get active pinctrl state\n");
	}
	if (data->regulator_en) {
		if (data->pdata->power_on) {
			err = data->pdata->power_on(true);
			if (err)
				dev_err(dev, "power on failed");
		} else {
			err = ft_power_on(data, true);
			if (err)
				dev_err(dev, "power on failed");
		}
	}
pwr_off_fail:
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}
	ft_irq_enable(data);
	return err;
}

static int ft_ts_suspend(struct device *dev)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	if (data->loading_fw) {
		dev_info(dev, "Firmware loading in process...\n");
		return 0;
	}

	if (data->suspended) {
		dev_info(dev, "Already in suspend state\n");
		return 0;
	}

	data->flash_enabled = false;
	ft_secure_touch_stop(data, true);

	return ft_ts_stop(dev);
}

static int ft_ts_resume(struct device *dev)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	int err;

	if (!data->suspended) {
		dev_dbg(dev, "Already in awake state\n");
		return 0;
	}

	data->flash_enabled = true;
	ft_secure_touch_stop(data, true);

	err = ft_ts_start(dev);
	if (err < 0)
		return err;

	if (data->charger_detection_enabled)
		ft_resume_ps_chg_cm_state(data);

	return 0;
}

static const struct dev_pm_ops ft_ts_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = ft_ts_suspend,
	.resume = ft_ts_resume,
#endif
};

#else
static int ft_ts_suspend(struct device *dev)
{
	return 0;
}

static int ft_ts_resume(struct device *dev)
{
	return 0;
}

#endif

#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct ft_ts_data *data =
		container_of(work, struct ft_ts_data, fb_notify_work);
	ft_ts_resume(&data->client->dev);
}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *d)
{
	struct fb_event *evdata = d;
	int *blank;
	struct ft_ts_data *data =
		container_of(self, struct ft_ts_data, fb_notif);

	if ((event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK) &&
		evdata && evdata->data && data && data->client) {
		blank = evdata->data;
		pr_debug("fb notification: event = %lu blank = %d\n", event,
			*blank);
		if (data->pdata->resume_in_workqueue) {
			if (event == FB_EARLY_EVENT_BLANK) {
				if (*blank != FB_BLANK_POWERDOWN)
					return 0;
				flush_work(&data->fb_notify_work);
				ft_ts_suspend(&data->client->dev);
			} else if (*blank == FB_BLANK_UNBLANK ||
				(*blank == FB_BLANK_NORMAL &&
				data->suspended)) {
				schedule_work(&data->fb_notify_work);
			}
		} else {
			if (event == FB_EARLY_EVENT_BLANK) {
				if (*blank != FB_BLANK_POWERDOWN)
					return 0;
				ft_ts_suspend(&data->client->dev);
			} else if (*blank == FB_BLANK_UNBLANK ||
				(*blank == FB_BLANK_NORMAL &&
				data->suspended)) {
				ft_ts_resume(&data->client->dev);
			}
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ft_ts_early_suspend(struct early_suspend *handler)
{
	struct ft_ts_data *data = container_of(handler,
						   struct ft_ts_data,
						   early_suspend);

	/*
	 * During early suspend/late resume, the driver doesn't access xPU/SMMU
	 * protected HW resources. So, there is no compelling need to block,
	 * but notifying the userspace that a power event has occurred is
	 * enough. Hence 'blocking' variable can be set to false.
	 */
	ft_secure_touch_stop(data, false);
	ft_ts_suspend(&data->client->dev);
}

static void ft_ts_late_resume(struct early_suspend *handler)
{
	struct ft_ts_data *data = container_of(handler,
						   struct ft_ts_data,
						   early_suspend);

	ft_secure_touch_stop(data, false);
	ft_ts_resume(&data->client->dev);
}
#endif

static int ft_set_charger_state(struct ft_ts_data *data,
					bool enable)
{
	int retval;
	struct device *dev = &data->client->dev;

	/* disable IRQ to set CHARGER_STATE reg */
	ft_irq_disable(data);
	if (enable) {
		retval = ft_write_reg(data->client,
			FT_REG_CHG_STATE, 1);
		if (retval < 0)
			dev_err(dev, "enable chg state failed(%d)\n",
				retval);
		else
			dev_info(dev, "set chg state\n");
	} else {
		retval = ft_write_reg(data->client,
			FT_REG_CHG_STATE, 0);
		if (retval < 0)
			dev_err(dev, "enable chg state failed(%d)\n",
				retval);
		else
			dev_info(dev, "unset chg state\n");
	}
	ft_irq_enable(data);

	return (retval > 0) ? 0 : retval;
}

static int ps_get_state(struct ft_ts_data *data,
			struct power_supply *psy, bool *present)
{
	struct device *dev = &data->client->dev;
	union power_supply_propval pval = {0};
	int retval;

	retval = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &pval);

	if (retval) {
		dev_err(dev, "%s psy get property failed\n", psy->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	dev_dbg(dev, "%s is %s\n", psy->name,
			(*present) ? "present" : "not present");
	return 0;
}

static void ft_update_ps_chg_cm_state(
				struct ft_ts_data *data, bool present)
{
	struct config_modifier *cm = NULL;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		if (cm->id == FT_MOD_CHARGER) {
			cm->effective = present;
			break;
		}
	}
	up(&data->modifiers.list_sema);
}

static void ft_resume_ps_chg_cm_state(struct ft_ts_data *data)
{
	struct config_modifier *cm = NULL;
	bool is_active = false;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		if (cm->id == FT_MOD_CHARGER) {
			is_active = cm->effective;
			break;
		}
	}
	up(&data->modifiers.list_sema);

	/* FT_REG_CHG_STATE bit is 0 when powerup
	 * So only need set this bit when need to be set as 1
	 */
	if (is_active)
		ft_set_charger_state(data, true);
}

static void ps_notify_callback_work(struct work_struct *work)
{
	struct ft_ts_data *data =
		container_of(work, struct ft_ts_data, ps_notify_work);
	bool present = data->ps_is_present;
	struct device *dev = &data->client->dev;
	int retval;

	/* enable IC if it in suspend state */
	if (data->suspended) {
		dev_dbg(dev, "charger resumes tp ic\n");
		ft_ts_resume(&data->client->dev);
	}

	retval = ft_set_charger_state(data, present);
	if (retval) {
		dev_err(dev, "set charger state failed rc=%d\n", retval);
		return;
	}

	ft_update_ps_chg_cm_state(data, present);
}

static int ps_notify_callback(struct notifier_block *self,
				 unsigned long event, void *p)
{
	struct ft_ts_data *data =
		container_of(self, struct ft_ts_data, ps_notif);
	struct power_supply *psy = p;
	struct device *dev = NULL;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_ADDED || event == PSY_EVENT_PROP_CHANGED)
		&& psy && psy->get_property && psy->name &&
		!strncmp(psy->name, "usb", sizeof("usb")) && data) {
		dev = &data->client->dev;
		dev_dbg(dev, "ps notification: event = %lu\n", event);
		retval = ps_get_state(data, psy, &present);
		if (retval) {
			dev_err(dev, "psy get property failed\n");
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (data->ps_is_present == present) {
				dev_info(dev, "ps present state not change\n");
				return 0;
			}
		}
		data->ps_is_present = present;
		schedule_work(&data->ps_notify_work);
	}

	return 0;
}

static void ft_detection_work(struct work_struct *work)
{
	struct ft_exp_fn *exp_fhandler, *next_list_entry;
	struct ft_ts_data *data;
	int error;
	bool det_again = false;

	mutex_lock(&exp_fn_ctrl_mutex);
	data = exp_fn_ctrl.data_ptr;
	mutex_unlock(&exp_fn_ctrl_mutex);

	if (data == NULL) {
		if (exp_fn_ctrl.det_workqueue)
			queue_delayed_work(exp_fn_ctrl.det_workqueue,
				&exp_fn_ctrl.det_work,
				msecs_to_jiffies(EXP_FN_DET_INTERVAL));
		return;
	}

	mutex_lock(&exp_fn_ctrl.list_mutex);
	if (list_empty(&exp_fn_ctrl.fn_list))
		goto release_mutex;

	list_for_each_entry_safe(exp_fhandler,
				next_list_entry,
				&exp_fn_ctrl.fn_list,
				link) {
		if (exp_fhandler->func_init == NULL) {
			if (exp_fhandler->inserted == true) {
				if (exp_fhandler->func_init != NULL)
					exp_fhandler->func_remove(data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
			continue;
		}

		if (exp_fhandler->inserted == true)
			continue;

		error = exp_fhandler->func_init(data);
		if (error < 0) {
			pr_err("Failed to init exp function. err=%d\n", error);
			det_again = true;
			continue;
		}
		exp_fhandler->inserted = true;
	}

release_mutex:
	mutex_unlock(&exp_fn_ctrl.list_mutex);
	if (det_again) {
		if (exp_fn_ctrl.det_workqueue)
			queue_delayed_work(exp_fn_ctrl.det_workqueue,
				&exp_fn_ctrl.det_work,
				msecs_to_jiffies(EXP_FN_DET_INTERVAL));
	}
}

void ft_new_function(bool insert,
		int (*func_init)(struct ft_ts_data *data),
		void (*func_remove)(struct ft_ts_data *data),
		void (*func_attn)(struct ft_ts_data *data,
					unsigned char intr_mask))
{
	struct ft_exp_fn *exp_fhandler;

	mutex_lock(&exp_fn_ctrl_mutex);
	if (!exp_fn_ctrl.inited) {
		mutex_init(&exp_fn_ctrl.list_mutex);
		INIT_LIST_HEAD(&exp_fn_ctrl.fn_list);
		exp_fn_ctrl.det_workqueue =
			create_singlethread_workqueue("rmi_det_workqueue");
		if (IS_ERR_OR_NULL(exp_fn_ctrl.det_workqueue))
			pr_err("unable to create a workqueue\n");
		INIT_DELAYED_WORK(&exp_fn_ctrl.det_work,
			ft_detection_work);
		exp_fn_ctrl.inited = true;
	}
	mutex_unlock(&exp_fn_ctrl_mutex);

	mutex_lock(&exp_fn_ctrl.list_mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
		if (!exp_fhandler) {
			pr_err("failed to alloc mem for expansion function\n");
			goto exit;
		}
		exp_fhandler->func_init = func_init;
		exp_fhandler->func_attn = func_attn;
		exp_fhandler->func_remove = func_remove;
		exp_fhandler->inserted = false;
		list_add_tail(&exp_fhandler->link, &exp_fn_ctrl.fn_list);
	} else {
		list_for_each_entry(exp_fhandler, &exp_fn_ctrl.fn_list, link) {
			if (exp_fhandler->func_init == func_init) {
				exp_fhandler->inserted = false;
				exp_fhandler->func_init = NULL;
				exp_fhandler->func_attn = NULL;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&exp_fn_ctrl.list_mutex);
	if (exp_fn_ctrl.det_workqueue)
		queue_delayed_work(exp_fn_ctrl.det_workqueue,
					&exp_fn_ctrl.det_work, 0);
}
EXPORT_SYMBOL(ft_new_function);

static int fps_notifier_callback(struct notifier_block *self,
				unsigned long event, void *p)
{
	int fps_state = *(int *)p;
	struct ft_ts_data *data =
		container_of(self, struct ft_ts_data, fps_notif);

	if (data && event == 0xBEEF && data->client) {
		struct config_modifier *cm =
			ft_modifier_by_id(data, FT_MOD_FPS);
		if (!cm) {
			dev_err(&data->client->dev,
				"No FPS modifier found\n");
			goto done;
		}

		if (fps_state) {/* on */
			data->clipping_on = true;
			data->clipa = cm->clipa;
			cm->effective = true;
		} else {/* off */
			data->clipping_on = false;
			data->clipa = NULL;
			cm->effective = false;
		}
		pr_info("FPS: clipping is %s\n",
			data->clipping_on ? "ON" : "OFF");
	}
done:
	return 0;
}

static int ft_fps_register_init(struct ft_ts_data *data)
{
	int retval;

	retval = FPS_register_notifier(&data->fps_notif, 0xBEEF, false);
	if (retval < 0) {
		dev_err(&data->client->dev,
			"Failed to register fps_notifier: %d\n", retval);
		return retval;
	}
	data->is_fps_registered = true;
	dev_info(&data->client->dev,
			"Register fps_notifier OK\n");

	return 0;
}

#if !defined(CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_MMI)
static int ft_auto_cal(struct i2c_client *client)
{
	struct ft_ts_data *data = i2c_get_clientdata(client);
	u8 temp = 0, i;

	/* set to factory mode */
	msleep(2 * data->pdata->soft_rst_dly);
	ft_write_reg(client, FT_REG_DEV_MODE, FT_FACTORYMODE_VALUE);
	msleep(data->pdata->soft_rst_dly);

	/* start calibration */
	ft_write_reg(client, FT_DEV_MODE_REG_CAL, FT_CAL_START);
	msleep(2 * data->pdata->soft_rst_dly);
	for (i = 0; i < FT_CAL_RETRY; i++) {
		ft_read_reg(client, FT_REG_CAL, &temp);
		/* return to normal mode, calibration finish */
		if (((temp & FT_CAL_MASK) >> FT_4BIT_SHIFT) == FT_CAL_FIN)
			break;
	}

	/*calibration OK */
	msleep(2 * data->pdata->soft_rst_dly);
	ft_write_reg(client, FT_REG_DEV_MODE, FT_FACTORYMODE_VALUE);
	msleep(data->pdata->soft_rst_dly);

	/* store calibration data */
	ft_write_reg(client, FT_DEV_MODE_REG_CAL, FT_CAL_STORE);
	msleep(2 * data->pdata->soft_rst_dly);

	/* set to normal mode */
	ft_write_reg(client, FT_REG_DEV_MODE, FT_WORKMODE_VALUE);
	msleep(2 * data->pdata->soft_rst_dly);

	return 0;
}

static int ft_fw_upgrade_start(struct i2c_client *client,
			const u8 *data, u32 data_len)
{
	struct ft_ts_data *ts_data = i2c_get_clientdata(client);
	struct fw_upgrade_info info = ts_data->pdata->info;
	u8 reset_reg;
	u8 w_buf[FT_MAX_WR_BUF] = {0}, r_buf[FT_MAX_RD_BUF] = {0};
	u8 pkt_buf[FT_FW_PKT_LEN + FT_FW_PKT_META_LEN];
	int i, j, temp;
	u32 pkt_num, pkt_len;
	u8 is_5336_new_bootloader = false;
	u8 is_5336_fwsize_30 = false;
	u8 fw_ecc;

	/* determine firmware size */
	if (*(data + data_len - FT_BLOADER_SIZE_OFF) == FT_BLOADER_NEW_SIZE)
		is_5336_fwsize_30 = true;
	else
		is_5336_fwsize_30 = false;

	for (i = 0, j = 0; i < FT_UPGRADE_LOOP; i++) {
		msleep(FT_EARSE_DLY_MS);
		/* reset - write 0xaa and 0x55 to reset register */
		if (ts_data->family_id == FT6X06_ID
			|| ts_data->family_id == FT6X36_ID)
			reset_reg = FT_RST_CMD_REG2;
		else
			reset_reg = FT_RST_CMD_REG1;

		ft_write_reg(client, reset_reg, FT_UPGRADE_AA);
		msleep(info.delay_aa);

		ft_write_reg(client, reset_reg, FT_UPGRADE_55);
		if (i <= (FT_UPGRADE_LOOP / 2))
			msleep(info.delay_55 + i * 3);
		else
			msleep(info.delay_55 - (i - (FT_UPGRADE_LOOP / 2)) * 2);

		/* Set i2c to std i2c mode */
		w_buf[0] = FT_REG_I2C_MODE;
		w_buf[1] = FT_VAL_I2C_MODE;
		w_buf[2] = FT_VAL_I2C_MODE_STD;
		ft_i2c_read(client, w_buf, 3, r_buf, 3);
		if (r_buf[0] != FT_REG_I2C_MODE ||
			r_buf[1] != FT_VAL_I2C_MODE ||
			r_buf[2] != FT_VAL_I2C_MODE_STD_CFM) {
			dev_err(&client->dev,
				"set std i2c error. r_val = 0x%02x%02x%02x\n",
				r_buf[0], r_buf[1], r_buf[2]);
			continue;
		}

		/* Enter upgrade mode */
		w_buf[0] = FT_UPGRADE_55;
		ft_i2c_write(client, w_buf, 1);
		usleep_range(FT_55_AA_DLY_NS, FT_55_AA_DLY_NS + 1);
		w_buf[0] = FT_UPGRADE_AA;
		ft_i2c_write(client, w_buf, 1);

		/* check READ_ID */
		msleep(info.delay_readid);
		w_buf[0] = FT_READ_ID_REG;
		w_buf[1] = 0x00;
		w_buf[2] = 0x00;
		w_buf[3] = 0x00;

		ft_i2c_read(client, w_buf, 4, r_buf, 2);

		if (r_buf[0] != info.upgrade_id_1
			|| r_buf[1] != info.upgrade_id_2) {
			dev_err(&client->dev, "Upgrade ID mismatch(%d), IC=0x%x 0x%x, info=0x%x 0x%x\n",
				i, r_buf[0], r_buf[1],
				info.upgrade_id_1, info.upgrade_id_2);
		} else
			break;
	}

	if (i >= FT_UPGRADE_LOOP) {
		dev_err(&client->dev, "Abort upgrade\n");
		return -EIO;
	}

	w_buf[0] = 0xcd;
	ft_i2c_read(client, w_buf, 1, r_buf, 1);

	if (r_buf[0] <= 4)
		is_5336_new_bootloader = FT_BLOADER_VERSION_LZ4;
	else if (r_buf[0] == 7)
		is_5336_new_bootloader = FT_BLOADER_VERSION_Z7;
	else if (r_buf[0] >= 0x0f &&
		((ts_data->family_id == FT_FT5336_FAMILY_ID_0x11) ||
		(ts_data->family_id == FT_FT5336_FAMILY_ID_0x12) ||
		(ts_data->family_id == FT_FT5336_FAMILY_ID_0x13) ||
		(ts_data->family_id == FT_FT5336_FAMILY_ID_0x14)))
		is_5336_new_bootloader = FT_BLOADER_VERSION_GZF;
	else
		is_5336_new_bootloader = FT_BLOADER_VERSION_LZ4;

	dev_dbg(&client->dev, "bootloader type=%d, r_buf=0x%x, family_id=0x%x\n",
		is_5336_new_bootloader, r_buf[0], ts_data->family_id);
	/* is_5336_new_bootloader = FT_BLOADER_VERSION_GZF; */

	/* erase app and panel paramenter area */
	w_buf[0] = FT_ERASE_APP_REG;
	ft_i2c_write(client, w_buf, 1);
	msleep(info.delay_erase_flash);

	if (is_5336_fwsize_30) {
		w_buf[0] = FT_ERASE_PANEL_REG;
		ft_i2c_write(client, w_buf, 1);
	}
	msleep(FT_EARSE_DLY_MS);

	w_buf[0] = FT_REG_FW_LEN;
	w_buf[1] = (u8)((data_len>>16) & 0xff);
	w_buf[2] = (u8)((data_len>>8) & 0xff);
	w_buf[3] = (u8)((data_len) & 0xff);
	ft_i2c_write(client, w_buf, 4);

	pkt_num = (data_len) / FT_FW_PKT_LEN;
	pkt_len = FT_FW_PKT_LEN;
	pkt_buf[0] = FT_FW_START_REG;
	pkt_buf[1] = 0x00;
	fw_ecc = 0;

	for (i = 0; i < pkt_num; i++) {
		temp = i * FT_FW_PKT_LEN;
		pkt_buf[2] = (u8) (temp >> FT_8BIT_SHIFT);
		pkt_buf[3] = (u8) temp;
		pkt_buf[4] = (u8) (pkt_len >> FT_8BIT_SHIFT);
		pkt_buf[5] = (u8) pkt_len;

		for (j = 0; j < FT_FW_PKT_LEN; j++) {
			pkt_buf[6 + j] = data[i * FT_FW_PKT_LEN + j];
			fw_ecc ^= pkt_buf[6 + j];
		}

		ft_i2c_write(client, pkt_buf,
				FT_FW_PKT_LEN + FT_FW_PKT_META_LEN);
		msleep(FT_FW_PKT_DLY_MS);
	}

	/* send remaining bytes */
	if ((data_len) % FT_FW_PKT_LEN > 0) {
		temp = pkt_num * FT_FW_PKT_LEN;
		pkt_buf[2] = (u8) (temp >> FT_8BIT_SHIFT);
		pkt_buf[3] = (u8) temp;
		temp = (data_len) % FT_FW_PKT_LEN;
		pkt_buf[4] = (u8) (temp >> FT_8BIT_SHIFT);
		pkt_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) {
			pkt_buf[6 + i] = data[pkt_num * FT_FW_PKT_LEN + i];
			fw_ecc ^= pkt_buf[6 + i];
		}

		ft_i2c_write(client, pkt_buf, temp + FT_FW_PKT_META_LEN);
		msleep(FT_FW_PKT_DLY_MS);
	}

	/* send the finishing packet */
	if (is_5336_new_bootloader == FT_BLOADER_VERSION_LZ4 ||
		is_5336_new_bootloader == FT_BLOADER_VERSION_Z7) {
		for (i = 0; i < FT_FINISHING_PKT_LEN_OLD_FW; i++) {
			if (is_5336_new_bootloader  == FT_BLOADER_VERSION_Z7)
				temp = FT_MAGIC_BLOADER_Z7 + i;
			else if (is_5336_new_bootloader ==
						FT_BLOADER_VERSION_LZ4)
				temp = FT_MAGIC_BLOADER_LZ4 + i;
			pkt_buf[2] = (u8)(temp >> 8);
			pkt_buf[3] = (u8)temp;
			temp = 1;
			pkt_buf[4] = (u8)(temp >> 8);
			pkt_buf[5] = (u8)temp;
			pkt_buf[6] = data[data_len + i];
			fw_ecc ^= pkt_buf[6];

			ft_i2c_write(client,
				pkt_buf, temp + FT_FW_PKT_META_LEN);
			msleep(FT_FW_PKT_DLY_MS);
		}
	} else if (is_5336_new_bootloader == FT_BLOADER_VERSION_GZF) {
		for (i = 0; i < FT_FINISHING_PKT_LEN_NEW_FW; i++) {
			if (is_5336_fwsize_30)
				temp = FT_MAGIC_BLOADER_GZF_30 + i;
			else
				temp = FT_MAGIC_BLOADER_GZF + i;
			pkt_buf[2] = (u8)(temp >> 8);
			pkt_buf[3] = (u8)temp;
			temp = 1;
			pkt_buf[4] = (u8)(temp >> 8);
			pkt_buf[5] = (u8)temp;
			pkt_buf[6] = data[data_len + i];
			fw_ecc ^= pkt_buf[6];

			ft_i2c_write(client,
				pkt_buf, temp + FT_FW_PKT_META_LEN);
			msleep(FT_FW_PKT_DLY_MS);

		}
	}

	/* verify checksum */
	w_buf[0] = FT_REG_ECC;
	ft_i2c_read(client, w_buf, 1, r_buf, 1);
	if (r_buf[0] != fw_ecc) {
		dev_err(&client->dev, "ECC error! dev_ecc=%02x fw_ecc=%02x\n",
					r_buf[0], fw_ecc);
		return -EIO;
	}

	/* reset */
	w_buf[0] = FT_REG_RESET_FW;
	ft_i2c_write(client, w_buf, 1);
	msleep(ts_data->pdata->soft_rst_dly);

	dev_info(&client->dev, "Firmware upgrade successful\n");

	return 0;
}

static int ft_fw_upgrade(struct device *dev, bool force)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	int rc;
	u8 fw_file_maj, fw_file_min, fw_file_sub_min, fw_file_vendor_id;
	bool fw_upgrade = false;

	if (data->suspended) {
		dev_err(dev, "Device is in suspend state: Exit FW upgrade\n");
		return -EBUSY;
	}

	rc = request_firmware(&fw, data->fw_name, dev);
	if (rc < 0) {
		dev_err(dev, "Request firmware failed - %s (%d)\n",
						data->fw_name, rc);
		return rc;
	}

	if (fw->size < FT_FW_MIN_SIZE || fw->size > FT_FW_MAX_SIZE) {
		dev_err(dev, "Invalid firmware size (%zu)\n", fw->size);
		rc = -EIO;
		goto rel_fw;
	}

	if (data->family_id == FT6X36_ID) {
		fw_file_maj = FT_FW_FILE_MAJ_VER_FT6X36(fw);
		fw_file_vendor_id = FT_FW_FILE_VENDOR_ID_FT6X36(fw);
	} else {
		fw_file_maj = FT_FW_FILE_MAJ_VER(fw);
		fw_file_vendor_id = FT_FW_FILE_VENDOR_ID(fw);
	}
	fw_file_min = FT_FW_FILE_MIN_VER(fw);
	fw_file_sub_min = FT_FW_FILE_SUB_MIN_VER(fw);
	fw_file_vendor_id = FT_FW_FILE_VENDOR_ID(fw);

	dev_info(dev, "Current firmware: %d.%d.%d", data->fw_ver[0],
				data->fw_ver[1], data->fw_ver[2]);
	dev_info(dev, "New firmware: %d.%d.%d", fw_file_maj,
				fw_file_min, fw_file_sub_min);

	if (force)
		fw_upgrade = true;
	else if ((data->fw_ver[0] < fw_file_maj) &&
		data->fw_vendor_id == fw_file_vendor_id)
		fw_upgrade = true;

	if (!fw_upgrade) {
		dev_info(dev, "Exiting fw upgrade...\n");
		rc = -EFAULT;
		goto rel_fw;
	}

	/* start firmware upgrade */
	if (FT_FW_CHECK(fw, data)) {
		rc = ft_fw_upgrade_start(data->client, fw->data, fw->size);
		if (rc < 0)
			dev_err(dev, "update failed (%d). try later...\n", rc);
		else if (data->pdata->info.auto_cal)
			ft_auto_cal(data->client);
	} else {
		dev_err(dev, "FW format error\n");
		rc = -EIO;
	}

	ft_update_fw_ver(data);
	ft_update_fw_id(data);

	FT_STORE_TS_DBG_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);
	FT_STORE_TS_INFO(ts_info_buff, data->family_id, data->fw_ver[0],
			 data->fw_ver[1], data->fw_ver[2]);
rel_fw:
	release_firmware(fw);
	return rc;
}
#endif

static ssize_t ft_poweron_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	bool val;

	mutex_lock(&data->input_dev->mutex);
	val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		val == false &&
		data->flash_enabled);
}

static DEVICE_ATTR(poweron, 0444, ft_poweron_show, NULL);

static ssize_t ft_productinfo_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		data->pdata->name);
}

static DEVICE_ATTR(productinfo, 0444, ft_productinfo_show, NULL);

static ssize_t ft_force_reflash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	unsigned int input;

	if (kstrtouint(buf, 10, &input) != 1)
		return -EINVAL;

	data->force_reflash = (input == 0) ? false : true;

	return count;
}

static DEVICE_ATTR(forcereflash, 0220, NULL, ft_force_reflash_store);

static ssize_t ft_flashprog_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		(data->loading_fw) ? 1 : 0);
}

static DEVICE_ATTR(flashprog, 0444, ft_flashprog_show, NULL);

static ssize_t ft_do_reflash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	int retval;
	char prefix[FT_FW_NAME_MAX_LEN] = "focaltech";
	char template[FT_FW_NAME_MAX_LEN];

	if (count > FT_FW_NAME_MAX_LEN) {
		dev_err(&data->client->dev,
			"%s: FW filename is too long\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (data->suspended) {
		dev_err(&data->client->dev,
			"%s: In suspend state, try again later\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (data->loading_fw) {
		dev_err(&data->client->dev,
			"%s: In FW flashing state, try again later\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (!data->force_reflash) {
		if (strncmp(buf, prefix,
			strnlen(prefix, sizeof(prefix)))) {
			dev_err(&data->client->dev,
				"%s: FW does not belong to Focaltech\n",
				__func__);
			retval = -EINVAL;
			goto exit;
		}

		snprintf(template, sizeof(template), "-%s-",
						data->pdata->name);
		if (!strnstr(buf + strnlen(prefix, sizeof(prefix)), template,
			count)) {
			dev_err(&data->client->dev,
				"%s: FW does not belong to %s\n",
				__func__,
				data->pdata->name);
			retval = -EINVAL;
			goto exit;
		}
	}

	strlcpy(data->fw_name, buf, count);
	dev_dbg(&data->client->dev,
			"%s: FW filename: %s\n",
			__func__,
			data->fw_name);

	mutex_lock(&data->input_dev->mutex);
	ft_irq_disable(data);
	data->loading_fw = true;
#if defined(CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_MMI)
		retval = fts_ctpm_auto_upgrade(data->client,
				data->fw_name,
				data->pdata);
#else
		retval = ft_fw_upgrade(dev, true);
#endif
	if (retval)
		dev_err(&data->client->dev,
				"%s: FW %s upgrade failed\n",
				__func__,
				data->pdata->name);
	ft_irq_enable(data);
	data->loading_fw = false;
	mutex_unlock(&data->input_dev->mutex);

	retval = count;
exit:
	data->fw_name[0] = 0;
	return retval;
}

static DEVICE_ATTR(doreflash, 0220, NULL, ft_do_reflash_store);

static ssize_t ft_build_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%02x%02x-%02x\n",
		data->fw_id[0], data->fw_id[1], data->fw_ver[0]);
}

static DEVICE_ATTR(buildid, 0444, ft_build_id_show, NULL);

static ssize_t ft_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	struct ft_ts_data *data = dev_get_drvdata(dev);

	if ('1' != buf[0]) {
		pr_err("Invalid argument for reset\n");
		return -EINVAL;
	}

	mutex_lock(&data->input_dev->mutex);
	ft_irq_disable(data);
	retval = gpio_direction_output(data->pdata->reset_gpio, 0);
	if (retval == 0) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	} else {
		dev_err(&data->client->dev,
			"set direction for reset gpio failed\n");
	}
	msleep(100);
	ft_irq_enable(data);
	mutex_unlock(&data->input_dev->mutex);
	return count;
}

static DEVICE_ATTR(reset, 0220, NULL, ft_reset_store);

static ssize_t ft_drv_irq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		data->irq_enabled ? "ENABLED" : "DISABLED");
}

static ssize_t ft_panel_supplier_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);

	if (data->panel_supplier)
		return scnprintf(buf, PAGE_SIZE, "%s\n",
			data->panel_supplier);
	return 0;
}
static DEVICE_ATTR(panel_supplier, 0444, ft_panel_supplier_show, NULL);

static ssize_t ft_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value = 0;
	int err = 0;
	struct ft_ts_data *data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		pr_err("Failed to convert value\n");
		return -EINVAL;
	}

	switch (value) {
	case 0:
		/* Disable irq */
		ft_irq_disable(data);
		break;
	case 1:
		/* Enable irq */
		ft_irq_enable(data);
		break;
	default:
		pr_err("Invalid value\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(drv_irq, 0664, ft_drv_irq_show, ft_drv_irq_store);

static ssize_t ft_ud_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t total = 0;

	total += scnprintf(buf + total, PAGE_SIZE - total, "display:\n");
	for (i = 0; i < display_ud_stats.ud_len; i++)
		total += scnprintf(buf + total, PAGE_SIZE - total,
				"[%d]: full cycles-%u, mismatch-%d\n", i,
				display_ud[i].counter,
				display_ud[i].mismatch);

	return total;
}

static DEVICE_ATTR(tsi, 0444, ft_ud_show, NULL);

static const struct attribute *ft_attrs[] = {
	&dev_attr_poweron.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_flashprog.attr,
	&dev_attr_doreflash.attr,
	&dev_attr_buildid.attr,
	&dev_attr_reset.attr,
	&dev_attr_panel_supplier.attr,
	&dev_attr_drv_irq.attr,
	&dev_attr_tsi.attr,
	NULL
};

static ssize_t ts_info_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	strlcpy(buf, ts_info_buff, FT_INFO_MAX_LEN);
	return strnlen(buf, FT_INFO_MAX_LEN);
}
static struct kobj_attribute ts_info_attr = __ATTR_RO(ts_info);

#include <linux/major.h>
#include <linux/kdev_t.h>

/* Attribute: path (RO) */
static ssize_t path_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ft_ts_data *data = dev_get_drvdata(dev);
	ssize_t blen;
	const char *path;

	if (!data) {
		pr_err("cannot get ft_data pointer\n");
		return (ssize_t)0;
	}
	path = kobject_get_path(&data->client->dev.kobj, GFP_KERNEL);
	blen = scnprintf(buf, PAGE_SIZE, "%s", path ? path : "na");
	kfree(path);
	return blen;
}

/* Attribute: vendor (RO) */
static ssize_t vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "focaltech");
}

static struct device_attribute touchscreen_attributes[] = {
	__ATTR_RO(path),
	__ATTR_RO(vendor),
	__ATTR_NULL
};

#define TSDEV_MINOR_BASE 128
#define TSDEV_MINOR_MAX 32

static int ft_ts_sysfs_class(void *_data, bool create)
{
	struct ft_ts_data *data = _data;
	const struct ft_ts_platform_data *pdata = data->pdata;
	struct device_attribute *attrs = touchscreen_attributes;
	int i, error = 0;
	static struct class *touchscreen_class;
	static struct device *ts_class_dev;
	static int minor;

	if (create) {
		minor = input_get_new_minor(data->client->addr,
						1, false);
		if (minor < 0)
			minor = input_get_new_minor(TSDEV_MINOR_BASE,
					TSDEV_MINOR_MAX, true);
		pr_info("assigned minor %d\n", minor);

		touchscreen_class = class_create(THIS_MODULE, "touchscreen");
		if (IS_ERR(touchscreen_class)) {
			error = PTR_ERR(touchscreen_class);
			touchscreen_class = NULL;
			return error;
		}

		ts_class_dev = device_create(touchscreen_class, NULL,
				MKDEV(INPUT_MAJOR, minor),
				data, pdata->name);
		if (IS_ERR(ts_class_dev)) {
			error = PTR_ERR(ts_class_dev);
			ts_class_dev = NULL;
			return error;
		}

		for (i = 0; attrs[i].attr.name != NULL; ++i) {
			error = device_create_file(ts_class_dev, &attrs[i]);
			if (error)
				break;
		}

		if (error)
			goto device_destroy;
	} else {
		if (!touchscreen_class || !ts_class_dev)
			return -ENODEV;

		for (i = 0; attrs[i].attr.name != NULL; ++i)
			device_remove_file(ts_class_dev, &attrs[i]);

		device_unregister(ts_class_dev);
		class_unregister(touchscreen_class);
	}

	return 0;

device_destroy:
	for (--i; i >= 0; --i)
		device_remove_file(ts_class_dev, &attrs[i]);
	device_destroy(touchscreen_class, MKDEV(INPUT_MAJOR, minor));
	ts_class_dev = NULL;
	class_unregister(touchscreen_class);
	pr_err("error creating touchscreen class\n");

	return -ENODEV;
}

static bool ft_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0xFF) {
		pr_err("FT reg address is invalid: 0x%x\n", addr);
		return false;
	}

	return true;
}

static int ft_debug_data_set(void *_data, u64 val)
{
	struct ft_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft_debug_addr_is_valid(data->addr))
		dev_info(&data->client->dev,
			"Writing into FT registers not supported\n");

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft_debug_data_get(void *_data, u64 *val)
{
	struct ft_ts_data *data = _data;
	int rc;
	u8 reg = 0;

	mutex_lock(&data->input_dev->mutex);

	if (ft_debug_addr_is_valid(data->addr)) {
		rc = ft_read_reg(data->client, data->addr, &reg);
		if (rc < 0)
			dev_err(&data->client->dev,
				"FT read register 0x%x failed (%d)\n",
				data->addr, rc);
		else
			*val = reg;
	}

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, ft_debug_data_get,
			ft_debug_data_set, "0x%02llX\n");

static int ft_debug_addr_set(void *_data, u64 val)
{
	struct ft_ts_data *data = _data;

	if (ft_debug_addr_is_valid(val)) {
		mutex_lock(&data->input_dev->mutex);
		data->addr = val;
		mutex_unlock(&data->input_dev->mutex);
	}

	return 0;
}

static int ft_debug_addr_get(void *_data, u64 *val)
{
	struct ft_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft_debug_addr_is_valid(data->addr))
		*val = data->addr;

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, ft_debug_addr_get,
			ft_debug_addr_set, "0x%02llX\n");

static int ft_debug_suspend_set(void *_data, u64 val)
{
	struct ft_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (val)
		ft_ts_suspend(&data->client->dev);
	else
		ft_ts_resume(&data->client->dev);

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft_debug_suspend_get(void *_data, u64 *val)
{
	struct ft_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);
	*val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, ft_debug_suspend_get,
			ft_debug_suspend_set, "%lld\n");

static int ft_debug_dump_info(struct seq_file *m, void *v)
{
	struct ft_ts_data *data = m->private;

	seq_printf(m, "%s\n", data->ts_info);

	return 0;
}

static int debugfs_dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ft_debug_dump_info, inode->i_private);
}

static const struct file_operations debug_dump_info_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_dump_info_open,
	.read		= seq_read,
	.release	= single_release,
};

static ssize_t ft_proc_read(struct file *filp, char __user *buff,
				size_t count, loff_t *ppos)
{
	unsigned char *read_buf = NULL;
	size_t read_buf_len = (count > PAGE_SIZE) ? count : PAGE_SIZE;
	u8 regval = 0x00;
	u8 regaddr = 0x00;
	struct ft_ts_data *data = PDE_DATA(filp->f_inode);
	int num_read_chars = 0;
	int retval;

	if (data == NULL) {
		pr_err("%s: cannot get ts data\n", __func__);
		return -EFAULT;
	}
	read_buf = kmalloc(read_buf_len, GFP_KERNEL);
	if (read_buf == NULL)
		return -ENOMEM;

	mutex_lock(&data->input_dev->mutex);
	switch (data->proc_operate_mode) {
	case PROC_UPGRADE:
		regaddr = FT_REG_FW_VER;
		retval = ft_read_reg(data->client,
					regaddr, &regval);
		if (retval < 0)
			num_read_chars = snprintf(read_buf, read_buf_len,
					"get fw version failed.\n");
		else
			num_read_chars = snprintf(read_buf, read_buf_len,
					"current fw version:0x%02x\n", regval);
		break;
	case PROC_READ_REGISTER:
		retval = ft_i2c_read(data->client, NULL, 0, read_buf, 1);
		if (retval < 0)
			dev_err(&data->client->dev,
					"%s: read I2C error\n", __func__);
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		retval = ft_i2c_read(data->client,
					NULL, 0, read_buf, count);
		if (retval < 0)
			dev_err(&data->client->dev,
					"%s: read I2C error\n", __func__);
		num_read_chars = count;
		break;
	default:
		break;
	}

	mutex_unlock(&data->input_dev->mutex);

	if (copy_to_user(buff, read_buf, num_read_chars)) {
		dev_err(&data->client->dev,
			"%s: copy to user error\n", __func__);
		kfree(read_buf);
		return -EFAULT;
	}
	kfree(read_buf);

	return num_read_chars;
}

static ssize_t ft_proc_write(struct file *filp, const char __user *buff,
				size_t count, loff_t *ppos)
{
	unsigned char *write_buf = NULL;
	size_t write_buf_len = count;
	struct ft_ts_data *data = PDE_DATA(filp->f_inode);
	int writelen = 0;
	int retval;

	if (data == NULL) {
		pr_err("%s: cannot get ts data\n", __func__);
		return -EFAULT;
	}
	write_buf = kmalloc(write_buf_len, GFP_KERNEL);
	if (write_buf == NULL)
		return -ENOMEM;

	if (copy_from_user(write_buf, buff, count)) {
		dev_err(&data->client->dev,
			"%s: copy from user error\n", __func__);
		kfree(write_buf);
		return -EFAULT;
	}

	mutex_lock(&data->input_dev->mutex);
	data->proc_operate_mode = write_buf[0];
	switch (data->proc_operate_mode) {
	case PROC_READ_REGISTER:
		retval = ft_i2c_write(data->client, write_buf + 1, 1);
		if (retval < 0)
			dev_err(&data->client->dev,
				"%s: write I2C error\n", __func__);
		break;
	case PROC_WRITE_REGISTER:
		retval = ft_i2c_write(data->client, write_buf + 1, 2);
		if (retval < 0)
			dev_err(&data->client->dev,
				"%s: write I2C error\n", __func__);
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = count - 1;
		if (writelen > 0) {
			retval = ft_i2c_write(data->client,
					write_buf + 1, writelen);
			if (retval < 0)
				dev_err(&data->client->dev,
					"%s: write I2C error\n", __func__);
		}
		break;
	default:
		dev_err(&data->client->dev,
				"%s: unsupport opt %d\n",
				__func__, data->proc_operate_mode);
		break;
	}

	mutex_unlock(&data->input_dev->mutex);
	kfree(write_buf);

	return count;
}

static const struct file_operations ft_proc_ops = {
	.owner		= THIS_MODULE,
	.read		= ft_proc_read,
	.write		= ft_proc_write
};

static int ft_create_proc_entry(struct ft_ts_data *data)
{
	struct proc_dir_entry *entry = data->proc_entry;
	struct i2c_client *client = data->client;

	entry = proc_create_data(FT_PROC_DIR_NAME,
			0664,
			NULL,
			&ft_proc_ops,
			data);
	if (entry == NULL) {
		dev_err(&client->dev, "Couldn't create proc entry\n");
		return -ENOMEM;
	}
	dev_info(&client->dev, "Create proc entry success\n");

	return 0;
}

static void ft_remove_proc_entry(struct ft_ts_data *data)
{
	proc_remove(data->proc_entry);
}

#ifdef CONFIG_OF
static int ft_get_dt_coords(struct device *dev, char *name,
				struct ft_ts_platform_data *pdata)
{
	u32 coords[FT_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FT_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "focaltech,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "focaltech,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

/* ASCII names order MUST match enum */
static const char const *ascii_names[] = {"charger", "fps", "na"};

static int ft_modifier_name2id(const char *name)
{
	int i, len2cmp, chosen = -1;

	for (i = 0; i < FT_MOD_MAX; i++) {
		len2cmp = min_t(int, strlen(name), strlen(ascii_names[i]));
		if (!strncmp(name, ascii_names[i], len2cmp)) {
			chosen = i;
			break;
		}
	}
	return chosen;
}

static struct config_modifier *ft_modifier_by_id(
	struct ft_ts_data *data, int id)
{
	struct config_modifier *cm, *found = NULL;

	down(&data->modifiers.list_sema);
	list_for_each_entry(cm, &data->modifiers.mod_head, link) {
		pr_debug("walk-thru: ptr=%p modifier[%s] id=%d\n",
					cm, cm->name, cm->id);
		if (cm->id == id) {
			found = cm;
			break;
		}
	}
	up(&data->modifiers.list_sema);
	pr_debug("returning modifier id=%d[%d]\n", found ? found->id : -1, id);
	return found;
}

static void ft_dt_parse_modifier(struct ft_ts_data *data,
		struct device_node *parent, struct config_modifier *config,
		const char *modifier_name, bool active)
{
	struct device *dev = &data->client->dev;
	char node_name[64];
	struct ft_clip_area clipa;
	struct device_node *np_config;
	int err;

	scnprintf(node_name, 63, "%s-%s", modifier_name,
			active ? "active" : "suspended");
	np_config = of_find_node_by_name(parent, node_name);
	if (!np_config) {
		dev_dbg(dev, "%s: node does not exist\n", node_name);
		return;
	}

	err = of_property_read_u32_array(np_config, "touch-clip-area",
		(unsigned int *)&clipa, sizeof(clipa)/sizeof(unsigned int));
	if (!err) {
		config->clipa = kzalloc(sizeof(clipa), GFP_KERNEL);
		if (!config->clipa) {
			dev_err(dev, "clip area allocation failure\n");
			return;
		}
		memcpy(config->clipa, &clipa, sizeof(clipa));
		pr_notice("using touch clip area in %s\n", node_name);
	}

	of_node_put(np_config);
}

static int ft_dt_parse_modifiers(struct ft_ts_data *data)
{
	struct device *dev = &data->client->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_mod;
	int i, num_names, ret = 0;
	char node_name[64];
	static const char **modifiers_names;

	sema_init(&data->modifiers.list_sema, 1);
	INIT_LIST_HEAD(&data->modifiers.mod_head);
	data->modifiers.mods_num = 0;

	num_names = of_property_count_strings(np, "config_modifier-names");
	if (num_names < 0) {
		dev_err(dev, "Cannot parse config_modifier-names: %d\n",
			num_names);
		return -ENODEV;
	}

	modifiers_names = devm_kzalloc(dev,
			sizeof(*modifiers_names) * num_names, GFP_KERNEL);
	if (!modifiers_names)
		return -ENOMEM;

	for (i = 0; i < num_names; i++) {
		ret = of_property_read_string_index(np, "config_modifier-names",
			i, &modifiers_names[i]);
		if (ret < 0) {
			dev_err(dev, "Cannot parse modifier-names: %d\n", ret);
			return ret;
		}
	}

	data->modifiers.mods_num = num_names;

	for (i = 0; i < num_names; i++) {
		int id;
		struct config_modifier *cm, *config;

		scnprintf(node_name, 63, "config_modifier-%s",
				modifiers_names[i]);
		np_mod = of_find_node_by_name(np, node_name);
		if (!np_mod) {
			dev_warn(dev, "cannot find modifier node %s\n",
				node_name);
			continue;
		}

		/* check for duplicate nodes in devtree */
		id = ft_modifier_name2id(modifiers_names[i]);
		dev_info(dev, "processing modifier %s[%d]\n", node_name, id);
		list_for_each_entry(cm, &data->modifiers.mod_head, link) {
			if (cm->id == id) {
				dev_err(dev, "duplicate modifier node %s\n",
					node_name);
				return -EFAULT;
			}
		}
		/* allocate modifier's structure */
		config = devm_kzalloc(dev, sizeof(*config), GFP_KERNEL);
		if (!config)
			return -ENOMEM;

		list_add_tail(&config->link, &data->modifiers.mod_head);
		config->name = modifiers_names[i];
		config->id = id;

		if (of_property_read_bool(np_mod, "enable-notification")) {
			switch (id) {
			case FT_MOD_CHARGER:
				pr_notice("using charger detection\n");
				data->charger_detection_enabled = true;
				break;
			case FT_MOD_FPS:
				pr_notice("using fingerprint sensor detection\n");
				data->fps_detection_enabled = true;
				break;
			default:
				pr_notice("no notification found\n");
				break;
			}
		} else {
			config->effective = true;
			dev_dbg(dev, "modifier %s enabled unconditionally\n",
					node_name);
		}

		dev_dbg(dev, "processing modifier %s[%d]\n",
				node_name, config->id);

		ft_dt_parse_modifier(data, np_mod, config,
				modifiers_names[i], true);
		ft_dt_parse_modifier(data, np_mod, config,
				modifiers_names[i], false);

		of_node_put(np_mod);
	}

	return 0;
}

static int ft_parse_dt(struct device *dev,
			struct ft_ts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val, num_buttons;
	u32 button_map[MAX_BUTTONS];
	u32 num_vendor_ids, i;

	pdata->name = "focaltech";
	rc = of_property_read_string(np, "focaltech,name", &pdata->name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read name\n");
		return rc;
	}

	rc = ft_get_dt_coords(dev, "focaltech,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = ft_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (rc)
		return rc;

	pdata->i2c_pull_up = of_property_read_bool(np,
						"focaltech,i2c-pull-up");

	pdata->no_force_update = of_property_read_bool(np,
						"focaltech,no-force-update");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	pdata->fw_name = "ft_fw.bin";
	rc = of_property_read_string(np, "focaltech,fw-name", &pdata->fw_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
		return rc;
	}

	rc = of_property_read_u32(np, "focaltech,group-id", &temp_val);
	if (!rc)
		pdata->group_id = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,hard-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->hard_rst_dly = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,soft-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->soft_rst_dly = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,num-max-touches", &temp_val);
	if (!rc)
		pdata->num_max_touches = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,fw-delay-aa-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay aa\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_aa =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-55-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay 55\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_55 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-upgrade-id1", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw upgrade id1\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.upgrade_id_1 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-upgrade-id2", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw upgrade id2\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.upgrade_id_2 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-readid-ms",
							&temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay read id\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_readid =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-era-flsh-ms",
							&temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay erase flash\n");
		return rc;
	} else if (rc != -EINVAL)
		pdata->info.delay_erase_flash =  temp_val;

	pdata->info.auto_cal = of_property_read_bool(np,
					"focaltech,fw-auto-cal");

	pdata->fw_vkey_support = of_property_read_bool(np,
						"focaltech,fw-vkey-support");

	pdata->ignore_id_check = of_property_read_bool(np,
						"focaltech,ignore-id-check");

	pdata->gesture_support = of_property_read_bool(np,
						"focaltech,gesture-support");

	pdata->resume_in_workqueue = of_property_read_bool(np,
					"focaltech,resume-in-workqueue");

	if (of_property_read_bool(np, "focaltech,x-flip")) {
		pr_notice("using flipped X axis\n");
		pdata->x_flip = true;
	}

	if (of_property_read_bool(np, "focaltech,y-flip")) {
		pr_notice("using flipped Y axis\n");
		pdata->y_flip = true;
	}

	rc = of_property_read_u32(np, "focaltech,family-id", &temp_val);
	if (!rc)
		pdata->family_id = temp_val;
	else
		return rc;

	prop = of_find_property(np, "focaltech,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);
		if (num_buttons > MAX_BUTTONS)
			return -EINVAL;

		rc = of_property_read_u32_array(np,
			"focaltech,button-map", button_map,
			num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
			return rc;
		}
	}

	num_vendor_ids = of_property_count_elems_of_size(np,
		"focaltech,vendor_ids", sizeof(u32));
	if ((num_vendor_ids > 0) && (num_vendor_ids < MAX_PANEL_SUPPLIERS)) {
		const char *name;

		rc = of_property_read_u32_array(np,
			"focaltech,vendor_ids", pdata->vendor_ids,
			num_vendor_ids);
		if (rc) {
			dev_err(dev, "Unable to read vendor ids\n");
			return rc;
		}
		prop = of_find_property(np, "focaltech,vendor_names", NULL);
		if (!prop) {
			dev_err(dev, "Unable to read vendor names\n");
			return rc;
		}
		for (name = of_prop_next_string(prop, NULL), i = 0;
			i < num_vendor_ids;
			name = of_prop_next_string(prop, name), i++)
			pdata->vendor_names[i] = name;

		pdata->num_vendor_ids = num_vendor_ids;
	}

	return 0;
}
#else
static int ft_parse_dt(struct device *dev,
			struct ft_ts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int ft_reboot(struct notifier_block *nb,
			unsigned long event,
			void *unused)
{
	struct ft_ts_data *data =
		container_of(nb, struct ft_ts_data, ft_reboot);

	dev_info(&data->client->dev, "touch shutdown\n");

	if (data->is_fps_registered) {
		FPS_unregister_notifier(&data->fps_notif, 0xBEEF);
		data->is_fps_registered = false;
	}

	if (data->charger_detection_enabled) {
		power_supply_unreg_notifier(&data->ps_notif);
		data->charger_detection_enabled = false;
	}

#if defined(CONFIG_FB)
	fb_unregister_client(&data->fb_notif);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif

	if (data->irq_enabled) {
		ft_irq_disable(data);
		free_irq(data->client->irq, data);
	}

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_direction_output(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_free(data->pdata->reset_gpio);
	}
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	if (data->regulator_en) {
		if (data->pdata->power_on)
			data->pdata->power_on(false);
		else
			ft_power_on(data, false);
		if (data->pdata->power_init)
			data->pdata->power_init(false);
		else
			ft_power_init(data, false);
		data->regulator_en = false;
	}

	return NOTIFY_DONE;
}

static int ft_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ft_ts_platform_data *pdata;
	struct ft_ts_data *data;
	struct input_dev *input_dev;
	struct dentry *temp;
	u8 reg_value = 0;
	u8 reg_addr;
	int err, retval, attr_count;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ft_ts_platform_data), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		err = ft_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "DT parsing failed\n");
			return err;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev,
			sizeof(struct ft_ts_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->tch_data_len = FT_TCH_LEN(pdata->num_max_touches);
	data->tch_data = devm_kzalloc(&client->dev,
				data->tch_data_len, GFP_KERNEL);
	if (!data->tch_data)
		return -ENOMEM;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	data->input_dev = input_dev;
	data->client = client;
	data->pdata = pdata;
	data->force_reflash = (data->pdata->no_force_update) ? false : true;
	data->flash_enabled = true;
	data->irq_enabled = false;

	data->patching_enabled = true;
	err = ft_dt_parse_modifiers(data);
	if (err)
		data->patching_enabled = false;

	input_dev->name = "focaltech_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, pdata->num_max_touches, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min,
			     pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min,
			     pdata->y_max, 0, 0);

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev, "Input device registration failed\n");
		goto input_register_device_err;
	}

	if (pdata->power_init) {
		err = pdata->power_init(true);
		if (err)
			dev_warn(&client->dev, "power init failed");
		else
			data->regulator_en = true;
	} else {
		err = ft_power_init(data, true);
		if (err)
			dev_warn(&client->dev, "power init failed");
		else
			data->regulator_en = true;
	}

	if (data->regulator_en) {
		if (pdata->power_on) {
			err = pdata->power_on(true);
			if (err) {
				dev_err(&client->dev, "power on failed");
				goto regulator_en_err;
			}
		} else {
			err = ft_power_on(data, true);
			if (err) {
				dev_err(&client->dev, "power on failed");
				goto regulator_en_err;
			}
		}
	}

	err = ft_ts_pinctrl_init(data);
	if (!err && data->ts_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0) {
			dev_err(&client->dev,
				"failed to select pin to active state");
		}
	}

	err = ft_gpio_configure(data, true);
	if (err < 0) {
		dev_err(&client->dev,
			"Failed to configure the gpios\n");
		goto gpio_config_err;
	}

	/* make sure CTP already finish startup process */
	msleep(data->pdata->soft_rst_dly);

	/* check the controller id */
	reg_addr = FT_REG_ID;
	err = ft_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0) {
		dev_err(&client->dev, "version read failed");
		goto free_gpio;
	}

	dev_info(&client->dev, "Device ID = 0x%x\n", reg_value);

	if ((pdata->family_id != reg_value) && (!pdata->ignore_id_check)) {
		dev_err(&client->dev, "%s:Unsupported controller\n", __func__);
		goto free_gpio;
	}

	data->family_id = pdata->family_id;

	/*get some register information */
	reg_addr = FT_REG_POINT_RATE;
	ft_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0)
		dev_err(&client->dev, "report rate read failed");

	dev_info(&client->dev, "report rate = %dHz\n", reg_value * 10);

	reg_addr = FT_REG_THGROUP;
	err = ft_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0)
		dev_err(&client->dev, "threshold read failed");

	dev_dbg(&client->dev, "touch threshold = %d\n", reg_value * 4);

	err = request_threaded_irq(client->irq, NULL,
				ft_ts_interrupt,
	/*
	 * the interrupt trigger mode will be set in Device Tree with property
	 * "interrupts", so here we just need to set the flag IRQF_ONESHOT
	 */
				IRQF_ONESHOT,
				client->dev.driver->name, data);
	if (err) {
		dev_err(&client->dev, "request irq failed\n");
		goto free_gpio;
	}

	/* request_threaded_irq enable irq,so set the flag irq_enabled */
	data->irq_enabled = true;

	err = ft_ts_sysfs_class(data, true);
	if (err) {
		dev_err(&client->dev, "sys class files creation failed\n");
		goto sysfs_class_err;
	}

	err = sysfs_create_files(&client->dev.kobj, ft_attrs);
	if (err) {
		dev_err(&client->dev, "sys files creation failed\n");
		goto sysfs_create_files_err;
	}

	data->dir = debugfs_create_dir(FT_DEBUG_DIR_NAME, NULL);
	if (data->dir == NULL || IS_ERR(data->dir)) {
		pr_err("debugfs_create_dir failed(%ld)\n", PTR_ERR(data->dir));
		err = PTR_ERR(data->dir);
		goto debugfs_create_dir_err;
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("dump_info", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_dump_info_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	err = ft_create_proc_entry(data);
	if (err) {
		dev_err(&client->dev, "create proc entry failed");
		goto free_debug_dir;
	}

	data->ts_info = devm_kzalloc(&client->dev,
				FT_INFO_MAX_LEN, GFP_KERNEL);
	if (!data->ts_info)
		goto free_proc_entry;

	/*creation touch panel info kobj*/
	data->ts_info_kobj = kobject_create_and_add(FT_TS_INFO_SYSFS_DIR_NAME,
					kernel_kobj);
	if (!data->ts_info_kobj) {
		dev_err(&client->dev, "kobject creation failed.\n");
	} else {
		err = sysfs_create_file(data->ts_info_kobj, &ts_info_attr.attr);
		if (err) {
			kobject_put(data->ts_info_kobj);
			dev_err(&client->dev, "sysfs creation failed.\n");
		} else {
			ts_info_buff = devm_kzalloc(&client->dev,
						 FT_INFO_MAX_LEN, GFP_KERNEL);
			if (!ts_info_buff)
				goto free_ts_info;
		}
	}
	FT_STORE_TS_DBG_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);
	FT_STORE_TS_INFO(ts_info_buff, data->family_id, data->fw_ver[0],
			 data->fw_ver[1], data->fw_ver[2]);

	/*Initialize secure touch */
	ft_secure_touch_init(data);
	ft_secure_touch_stop(data, true);
	mutex_init(&(data->ft_clk_io_ctrl_mutex));

	/* Creation of secure touch sysfs files */
	for (attr_count = 0; attr_count < ARRAY_SIZE(st_attrs); attr_count++) {
		retval = sysfs_create_file(&data->input_dev->dev.kobj,
						&st_attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&client->dev,
				"%s: Failed to create sysfs attributes\n",
							__func__);
			goto free_secure_touch_sysfs;
		}
	}

	ft_update_fw_ver(data);
	ft_update_fw_vendor_id(data);
	ft_update_fw_id(data);

#if defined(CONFIG_FB)
	INIT_WORK(&data->fb_notify_work, fb_notify_resume_work);
	data->fb_notif.notifier_call = fb_notifier_callback;

	err = fb_register_client(&data->fb_notif);
	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
			err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						    FT_SUSPEND_LEVEL;
	data->early_suspend.suspend = ft_ts_early_suspend;
	data->early_suspend.resume = ft_ts_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	mutex_lock(&exp_fn_ctrl_mutex);
	if (!exp_fn_ctrl.inited) {
		mutex_init(&exp_fn_ctrl.list_mutex);
		INIT_LIST_HEAD(&exp_fn_ctrl.fn_list);
		exp_fn_ctrl.det_workqueue =
			create_singlethread_workqueue("rmi_det_workqueue");
		if (IS_ERR_OR_NULL(exp_fn_ctrl.det_workqueue))
			pr_err("unable to create a workqueue\n");
		INIT_DELAYED_WORK(&exp_fn_ctrl.det_work,
			ft_detection_work);
		exp_fn_ctrl.inited = true;
	}
	exp_fn_ctrl.data_ptr = data;
	mutex_unlock(&exp_fn_ctrl_mutex);

	if (data->charger_detection_enabled) {
		struct power_supply *psy = NULL;

		INIT_WORK(&data->ps_notify_work, ps_notify_callback_work);
		data->ps_notif.notifier_call = ps_notify_callback;
		err = power_supply_reg_notifier(&data->ps_notif);
		if (err) {
			dev_err(&client->dev,
				"Unable to register ps_notifier: %d\n", err);
			goto free_fb_notifier;
		}

		/* if power supply supplier registered brfore TP
		 * ps_notify_callback will not receive PSY_EVENT_PROP_ADDED
		 * event, and will cause miss to set TP into charger state.
		 * So check PS state in probe.
		 */
		psy = power_supply_get_by_name("usb");
		if (psy) {
			retval = ps_get_state(data, psy, &data->ps_is_present);
			if (retval) {
				dev_err(&client->dev,
					"psy get property failed rc=%d\n",
					retval);
				goto free_fb_notifier;
			}

			retval = ft_set_charger_state(data,
							data->ps_is_present);
			if (retval) {
				dev_err(&client->dev,
					"set charger state failed rc=%d\n",
					retval);
				goto free_fb_notifier;
			}

			ft_update_ps_chg_cm_state(data,
						data->ps_is_present);
		}
	}

	if (data->fps_detection_enabled) {
		data->fps_notif.notifier_call = fps_notifier_callback;
		dev_dbg(&client->dev, "registering FPS notifier\n");
		ft_new_function(true,
			ft_fps_register_init,
			NULL,
			NULL);
	}

	data->ft_reboot.notifier_call = ft_reboot;
	data->ft_reboot.next = NULL;
	data->ft_reboot.priority = 1;
	err = register_reboot_notifier(&data->ft_reboot);
	if (err) {
		dev_err(&client->dev, "register for reboot failed\n");
		goto reboot_register_err;
	}

	return 0;

reboot_register_err:
	if (data->is_fps_registered)
		FPS_unregister_notifier(&data->fps_notif, 0xBEEF);
	if (data->charger_detection_enabled)
		power_supply_unreg_notifier(&data->ps_notif);
free_fb_notifier:
#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif
free_secure_touch_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&data->input_dev->dev.kobj,
					&st_attrs[attr_count].attr);
	}
free_ts_info:
	kobject_put(data->ts_info_kobj);
free_proc_entry:
	ft_remove_proc_entry(data);
free_debug_dir:
	debugfs_remove_recursive(data->dir);
debugfs_create_dir_err:
	sysfs_remove_files(&client->dev.kobj, ft_attrs);
sysfs_create_files_err:
	ft_ts_sysfs_class(data, false);
sysfs_class_err:
	free_irq(client->irq, data);
	data->irq_enabled = false;
free_gpio:
	ft_gpio_configure(data, false);
gpio_config_err:
	if (data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->ts_pinctrl);
			data->ts_pinctrl = NULL;
		} else {
			err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_release);
			if (err)
				pr_err("failed to select relase pinctrl state\n");
		}
	}

	if (data->regulator_en) {
		if (pdata->power_on)
			pdata->power_on(false);
		else
			ft_power_on(data, false);
	}
regulator_en_err:
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft_power_init(data, false);
	input_unregister_device(input_dev);
input_register_device_err:
	input_free_device(input_dev);
	return err;
}

static int ft_ts_remove(struct i2c_client *client)
{
	struct ft_ts_data *data = i2c_get_clientdata(client);
	int retval, attr_count;

	if (exp_fn_ctrl.inited) {
		cancel_delayed_work_sync(&exp_fn_ctrl.det_work);
		flush_workqueue(exp_fn_ctrl.det_workqueue);
		destroy_workqueue(exp_fn_ctrl.det_workqueue);
	}

	if (data->is_fps_registered)
		FPS_unregister_notifier(&data->fps_notif, 0xBEEF);

	unregister_reboot_notifier(&data->ft_reboot);

	if (data->charger_detection_enabled)
		power_supply_unreg_notifier(&data->ps_notif);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif

	for (attr_count = 0; attr_count < ARRAY_SIZE(st_attrs); attr_count++) {
		sysfs_remove_file(&data->input_dev->dev.kobj,
					&st_attrs[attr_count].attr);
	}

	ft_remove_proc_entry(data);
	debugfs_remove_recursive(data->dir);
	sysfs_remove_files(&client->dev.kobj, ft_attrs);
	ft_ts_sysfs_class(data, false);

	free_irq(client->irq, data);
	data->irq_enabled = false;

	ft_gpio_configure(data, false);

	if (data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->ts_pinctrl);
			data->ts_pinctrl = NULL;
		} else {
			retval = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_release);
			if (retval < 0)
				pr_err("failed to select release pinctrl state\n");
		}
	}

	if (data->regulator_en) {
		if (data->pdata->power_on)
			data->pdata->power_on(false);
		else
			ft_power_on(data, false);
	}

	if (data->pdata->power_init)
		data->pdata->power_init(false);
	else
		ft_power_init(data, false);

	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	kobject_put(data->ts_info_kobj);
	return 0;
}

static const struct i2c_device_id ft_ts_id[] = {
	{"focaltech_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ft_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id ft_match_table[] = {
	{ .compatible = "focaltech,mmi",},
	{ },
};
#else
#define ft_match_table NULL
#endif

static struct i2c_driver ft_ts_driver = {
	.probe = ft_ts_probe,
	.remove = ft_ts_remove,
	.driver = {
		.name = "focaltech_ts",
		.owner = THIS_MODULE,
		.of_match_table = ft_match_table,
#ifdef CONFIG_PM
		.pm = &ft_ts_pm_ops,
#endif
		},
	.id_table = ft_ts_id,
};

static int __init ft_ts_init(void)
{
	return i2c_add_driver(&ft_ts_driver);
}
module_init(ft_ts_init);

static void __exit ft_ts_exit(void)
{
	i2c_del_driver(&ft_ts_driver);
}
module_exit(ft_ts_exit);

MODULE_DESCRIPTION("FocalTech mmi TouchScreen driver");
MODULE_LICENSE("GPL v2");
