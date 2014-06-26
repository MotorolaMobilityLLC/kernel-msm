/*
 * max17042_battery.c - Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 * MyungJoo Ham <myungjoo.ham@samsung.com>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/miscdevice.h>
#include <linux/atomic.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>

/* Status register bits */
#define STATUS_MASK		0xFF0A
#define STATUS_POR_BIT		(1 << 1)
#define STATUS_BST_BIT		(1 << 3)
#define STATUS_VMN_BIT		(1 << 8)
#define STATUS_TMN_BIT		(1 << 9)
#define STATUS_SMN_BIT		(1 << 10)
#define STATUS_BI_BIT		(1 << 11)
#define STATUS_VMX_BIT		(1 << 12)
#define STATUS_TMX_BIT		(1 << 13)
#define STATUS_SMX_BIT		(1 << 14)
#define STATUS_BR_BIT		(1 << 15)

#define MAX17042_IC_VERSION	0x0092
#define MAX17050_IC_VERSION	0x00AC

/* Vmax disabled, Vmin disabled */
#define VOLT_DEF_MAX_MIN_THRLD  0xFF00

/* Vmax disabled, Vmin set to 3300mV */
#define VOLT_MIN_THRLD_ENBL	0xFFA5

/* Tmax disabled, Tmin disabled */
#define TEMP_DEF_MAX_MIN_THRLD  0x7F80

/* SoCmax disabled, SoCmin can be set to 15%, 4% and 1%.
 * INT will trigger when the thresholds are voilated.
 */
#define SOC_DEF_MAX_MIN1_THRLD	0xFF0E
#define SOC_DEF_MAX_MIN2_THRLD	0xFF04
#define SOC_DEF_MAX_MIN3_THRLD	0xFF01

/* SOC threshold for 1% interrupt */
#define SOC_INTR_S0_THR		1

#define MISCCFG_CONFIG_REPSOC	0x0000
#define MISCCFG_CONFIG_VFSOC	0x0003

/* low battery notification warning level */
#define SOC_WARNING_LEVEL1	14
#define SOC_WARNING_LEVEL2	4
#define SOC_SHUTDOWN_LEVEL	1

#define CONFIG_BER_BIT_ENBL	(1 << 0)
#define CONFIG_BEI_BIT_ENBL	(1 << 1)
#define CONFIG_ALRT_BIT_ENBL	(1 << 2)
#define CONFIG_VSTICKY_BIT_SET	(1 << 12)
#define CONFIG_TSTICKY_BIT_SET	(1 << 13)
#define CONFIG_SSTICKY_BIT_SET	(1 << 14)
#define CONFIG_ALP_BIT_ENBL	(1 << 11)
#define CONFIG_TEX_BIT_ENBL	(1 << 8)

#define VFSOC0_LOCK		0x0000
#define VFSOC0_UNLOCK		0x0080
#define FG_MODEL_UNLOCK1	0X0059
#define FG_MODEL_UNLOCK2	0X00C4
#define FG_MODEL_LOCK1		0X0000
#define FG_MODEL_LOCK2		0X0000

#define dQ_ACC_DIV	0x4
#define dP_ACC_100	0x1900
#define dP_ACC_200	0x3200

#define	NTC_47K_TGAIN	0xE4E4
#define	NTC_47K_TOFF	0x2F1D

#define BATT_CHRG_FULL_DES		1550000
#define MAX17042_VOLT_CONV_FCTR		625
#define MAX17042_CURR_CONV_FCTR		156
#define MAX17042_CHRG_CONV_FCTR		500

#define MAX17042_TEMP_SIGN_MASK		0x8000

#define MAX17042_MAX_MEM	(0xFF + 1)

#define MAX17042_MODEL_MUL_FACTOR(a, b)	((a * 100) / b)
#define MAX17042_MODEL_DIV_FACTOR(a, b)	((a * b) / 100)

#define CONSTANT_TEMP_IN_POWER_SUPPLY	350
#define POWER_SUPPLY_VOLT_MIN_THRESHOLD	3500000
#define BATTERY_VOLT_MIN_THRESHOLD	3400000

#define CYCLES_ROLLOVER_CUTOFF		0x00FF
#define MAX17042_DEF_RO_LRNCFG		0x0076

#define MAX17042_CGAIN_DISABLE		0x0000
#define MAX17042_EN_VOLT_FG		0x0007
#define MAX17042_CFG_INTR_SOCVF		0x0003

/* Vempty value set to 2500mV */
#define MAX17042_DEF_VEMPTY_VAL		0x7D5A

#define MAX17042_SIGN_INDICATOR		0x8000

#define SHUTDOWN_DEF_FG_MASK_BIT	(1 << 0)
#define SHUTDOWN_OCV_MASK_BIT		(1 << 1)
#define SHUTDOWN_LOWBATT_MASK_BIT	(1 << 2)

#define BYTE_VALUE			1
#define WORD_VALUE			0

/* Time interval to write temperature values from host, if needed (in milliseconds) */
#define TEMP_WRITE_INTERVAL		120000
enum max17042_register {
	MAX17042_STATUS		= 0x00,
	MAX17042_VALRT_Th	= 0x01,
	MAX17042_TALRT_Th	= 0x02,
	MAX17042_SALRT_Th	= 0x03,
	MAX17042_AtRate		= 0x04,
	MAX17042_RepCap		= 0x05,
	MAX17042_RepSOC		= 0x06,
	MAX17042_Age		= 0x07,
	MAX17042_TEMP		= 0x08,
	MAX17042_VCELL		= 0x09,
	MAX17042_Current	= 0x0A,
	MAX17042_AvgCurrent	= 0x0B,
	MAX17042_Qresidual	= 0x0C,
	MAX17042_SOC		= 0x0D,
	MAX17042_AvSOC		= 0x0E,
	MAX17042_RemCap		= 0x0F,
	MAX17042_FullCAP	= 0x10,
	MAX17042_TTE		= 0x11,
	MAX17042_V_empty	= 0x12,

	MAX17042_RSLOW		= 0x14,

	MAX17042_AvgTA		= 0x16,
	MAX17042_Cycles		= 0x17,
	MAX17042_DesignCap	= 0x18,
	MAX17042_AvgVCELL	= 0x19,
	MAX17042_MinMaxTemp	= 0x1A,
	MAX17042_MinMaxVolt	= 0x1B,
	MAX17042_MinMaxCurr	= 0x1C,
	MAX17042_CONFIG		= 0x1D,
	MAX17042_ICHGTerm	= 0x1E,
	MAX17042_AvCap		= 0x1F,
	MAX17042_ManName	= 0x20,
	MAX17042_DevName	= 0x21,
	MAX17042_DevChem	= 0x22,
	MAX17042_FullCAPNom	= 0x23,

	MAX17042_TempNom	= 0x24,
	MAX17042_TempCold	= 0x25,
	MAX17042_TempHot	= 0x26,
	MAX17042_AIN		= 0x27,
	MAX17042_LearnCFG	= 0x28,
	MAX17042_SHFTCFG	= 0x29,
	MAX17042_RelaxCFG	= 0x2A,
	MAX17042_MiscCFG	= 0x2B,
	MAX17042_TGAIN		= 0x2C,
	MAx17042_TOFF		= 0x2D,
	MAX17042_CGAIN		= 0x2E,
	MAX17042_COFF		= 0x2F,

	MAX17042_SOCempty	= 0x33,
	MAX17042_T_empty	= 0x34,
	MAX17042_FullCAP0	= 0x35,

	MAX17042_LAvg_empty	= 0x36,
	MAX17042_FCTC		= 0x37,
	MAX17042_RCOMP0		= 0x38,
	MAX17042_TempCo		= 0x39,
	MAX17042_ETC		= 0x3A,
	MAX17042_K_empty0	= 0x3B,
	MAX17042_TaskPeriod	= 0x3C,
	MAX17042_FSTAT		= 0x3D,

	MAX17042_SHDNTIMER	= 0x3F,

	MAX17042_dQacc		= 0x45,
	MAX17042_dPacc		= 0x46,
	MAX17042_VFSOC0         = 0x48,
	MAX17042_VFRemCap	= 0x4A,

	MAX17042_QH		= 0x4D,
	MAX17042_QL		= 0x4E,

	MAX17042_VFSOC0Enable	= 0x60,
	MAX17042_MLOCKReg1	= 0x62,
	MAX17042_MLOCKReg2	= 0x63,
	MAX17042_MODELChrTbl	= 0x80,
	MAX17042_OCV		= 0xEE,
	MAX17042_OCVInternal	= 0xFB,
	MAX17042_VFSOC		= 0xFF,

};

/* Registers specific to max17047/50 */
enum max17050_register {
	MAX17050_QRTbl00	= 0x12,
	MAX17050_FullSOCThr	= 0x13,
	MAX17050_QRTbl10	= 0x22,
	MAX17050_QRTbl20	= 0x32,
	MAX17050_V_empty	= 0x3A,
	MAX17050_QRTbl30	= 0x42,
};

#define DRV_NAME "max170xx_battery"

enum max170xx_chip_type {MAX17042, MAX17050};

/* No of times we should retry on -EAGAIN error */
#define NR_RETRY_CNT	3

/* No of times we should process interrupt reasons @irq handler */
/* Probably all values >1 are ok, Normally It just goes once thought
 * all bits and everything is handled. Also chips seems to limit
 * interrupts to ~3/s, so we have ~300ms to process, until we will
 * miss interrupt. What ever value it's, it doesn't have any
 * performance impact. */
#define NR_RETRY_INT	3

/* No of times we should reset I2C lines */
#define NR_I2C_RESET_CNT	8

#define VBATT_MAX 4200000	/* 4200mV */
#define VBATT_MIN 3400000	/* 3400mV */

#define VBATT_MIN_OFFSET	100 /* 100mV from VMMIN */
#define VBATT_MAX_OFFSET	50 /* 50mV from VMAX */
#define VALERT_VOLT_OFFSET	20 /* each bit corresponds to 20mV */

/* default fuel gauge cell data for debug purpose only */
static uint16_t cell_char_tbl[] = {
	/* Data to be written from 0x80h */
	0xA250, 0xB720, 0xB800, 0xB880, 0xB920, 0xBA00, 0xBA60, 0xBBF0,
	0xBCF0, 0xBE50, 0xC060, 0xC2D0, 0xC520, 0xC750, 0xCA00, 0xD090,
	/* Data to be written from 0x90h */
	0x0120, 0x1C80, 0x0470, 0x0440, 0x0100, 0x5500, 0x0960, 0x2410,
	0x2250, 0x15F0, 0x0BD0, 0x0D00, 0x0B00, 0x0BB0, 0x08A0, 0x08A0,
	/* Data to be written from 0xA0h */
	0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
	0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
};

struct max17042_chip {
	struct i2c_client *client;
	enum max170xx_chip_type chip_type;
	struct power_supply battery;
	struct max17042_platform_data *pdata;
	struct mutex batt_lock;
	struct mutex init_lock;

	int present;
	int status;
	int health;
	int technology;
	int charge_full_des;

	struct work_struct	init_worker;
	struct work_struct	evt_worker;
	struct delayed_work	temp_worker;

	bool plat_rebooting;
	/*
	 * user space can disable default shutdown
	 * methods set by platform.
	 */
	int	disable_shdwn_methods;

	/*
	 * user space can set this variable to report constant
	 * batery temperature for conformence testing.
	 */
	bool	enable_fake_temp;
	int	extra_resv_cap;
	int	voltage_max;
	int	model_algo_factor;
};

/* Sysfs entry for disable shutdown methods from user space */
static ssize_t override_shutdown_methods(struct device *device,
			       struct device_attribute *attr, const char *buf,
			       size_t count);
static ssize_t get_shutdown_methods(struct device *device,
			       struct device_attribute *attr, char *buf);
static DEVICE_ATTR(disable_shutdown_methods, S_IRUGO | S_IWUSR,
	get_shutdown_methods, override_shutdown_methods);

/* Sysfs entry to enter shutdown voltage from user space */
static int shutdown_volt;
static ssize_t set_shutdown_voltage(struct device *device,
				struct device_attribute *attr, const char *buf,
				size_t count);
static ssize_t get_shutdown_voltage_set_by_user(struct device *device,
				struct device_attribute *attr, char *buf);
static DEVICE_ATTR(shutdown_voltage, S_IRUGO | S_IWUSR,
	get_shutdown_voltage_set_by_user, set_shutdown_voltage);

/*
 * Sysfs entry to report fake battery temperature. This
 * interface is needed to support conformence testing
 */
static ssize_t set_fake_temp_enable(struct device *device,
			       struct device_attribute *attr, const char *buf,
			       size_t count);
static ssize_t get_fake_temp_enable(struct device *device,
			       struct device_attribute *attr, char *buf);
static DEVICE_ATTR(enable_fake_temp, S_IRUGO | S_IWUSR,
	get_fake_temp_enable, set_fake_temp_enable);

#ifdef CONFIG_DEBUG_FS
static struct dentry *max17042_dbgfs_root;
static char max17042_dbg_regs[MAX17042_MAX_MEM][4];
#endif

static int max17042_reboot_callback(struct notifier_block *nfb,
					unsigned long event, void *data);

static struct notifier_block max17042_reboot_notifier_block = {
	.notifier_call = max17042_reboot_callback,
	.priority = 0,
};

static bool is_battery_online(struct max17042_chip *chip);
static void configure_interrupts(struct max17042_chip *chip);
/* Set SOC threshold in S3 state */
static void set_soc_intr_thresholds_s3(struct max17042_chip *chip);
/* Set SOC threshold to offset percentage in S0 state */
static void set_soc_intr_thresholds_s0(struct max17042_chip *chip, int offset);
static void save_runtime_params(struct max17042_chip *chip);
static void set_chip_config(struct max17042_chip *chip);
static u16 fg_vfSoc;
static bool fake_batt_full;
static struct max17042_config_data *fg_conf_data;
static struct i2c_client *max17042_client;


atomic_t fopen_count;

static void update_runtime_params(struct max17042_chip *chip);
static int read_batt_pack_temp(struct max17042_chip *chip, int *temp);

/* Voltage-Capacity lookup function to get
 * capacity value against a given voltage */
static unsigned int voltage_capacity_lookup(unsigned int val)
{
	unsigned int max = VBATT_MAX / 1000;
	unsigned int min = VBATT_MIN / 1000;
	unsigned int capacity;
	unsigned int total_diff;
	unsigned int val_diff;

	if (val > max)
		return 100;

	if (val < min)
		return 0;

	total_diff = max - min;
	val_diff = max - val;

	capacity = (total_diff - val_diff) * 100 / total_diff;

	return capacity;
}

static int max17042_property_is_privileged_read(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		return 1;
	default:
		break;
	}
	return 0;
}

static int dev_file_open(struct inode *i, struct file *f)
{
	if (atomic_read(&fopen_count))
		return -EBUSY;
	atomic_inc(&fopen_count);
	return 0;
}

static int dev_file_close(struct inode *i, struct file *f)
{
	atomic_dec(&fopen_count);
	return 0;
}

static ssize_t dev_file_read(struct file *f, char __user *buf,
			size_t len, loff_t *off)
{
	struct max17042_chip *chip = i2c_get_clientdata(max17042_client);
	int ret;

	if (!chip->pdata->is_init_done) {
		dev_err(&max17042_client->dev,
			"MAX17042 is not initialized.\n");
		return -ECANCELED;
	}

	update_runtime_params(chip);

	if (sizeof(*fg_conf_data) > len)
		return -EINVAL;

	ret = copy_to_user(buf, fg_conf_data, sizeof(*fg_conf_data));
	if (!ret)
		return sizeof(*fg_conf_data);

	return -EINVAL;
}

static ssize_t dev_file_write(struct file *f, const char __user *buf,
			size_t len, loff_t *off)
{
	struct max17042_chip *chip = i2c_get_clientdata(max17042_client);

	if (chip->pdata->is_init_done) {
		dev_err(&max17042_client->dev,
			"Already initialized.So ignoring new set of data\n");
		return -ECANCELED;
	}

	if (len > sizeof(*fg_conf_data))
		return -EINVAL;

	if (copy_from_user(fg_conf_data, buf, len))
		return -EINVAL;

	set_chip_config(chip);

	if (chip->pdata->is_init_done) {
		dev_info(&max17042_client->dev,
				"MAX17042 initialized successfully\n");
		fg_conf_data->config_init = 0x1;
	}

	/* Return no. of bytes written */
	return len;
}

static const struct file_operations helper_fops = {
	.owner = THIS_MODULE,
	.open = &dev_file_open,
	.release = &dev_file_close,
	.read = &dev_file_read,
	.write = &dev_file_write,
};

static struct miscdevice fg_helper = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "max170xx",
	.fops = &helper_fops,
};

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

static int max17042_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret, i;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	if (chip->plat_rebooting) {
		dev_warn(&client->dev, "rebooting is in progress\n");
		return -EINVAL;
	}

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_write_word_data(client, reg, value);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Write error:%d\n", ret);

	return ret;
}

static int max17042_read_reg(struct i2c_client *client, u8 reg)
{
	int ret, i;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	if (chip->plat_rebooting) {
		dev_warn(&client->dev, "rebooting is in progress\n");
		return -EINVAL;
	}

	for (i = 0; i < NR_RETRY_CNT; i++) {
		ret = i2c_smbus_read_word_data(client, reg);
		if (ret == -EAGAIN || ret == -ETIMEDOUT)
			continue;
		else
			break;
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C SMbus Read error:%d\n", ret);

	return ret;
}

/*
 * max17042 chip has few registers which could get modified by the
 * chip as well during its fuel gauge learning process. So we need
 * to do a write verify on those registers and if the write fails
 * then we have to retry.
 */
static int max17042_write_verify_reg(struct i2c_client *client,
						u8 reg, u16 value)
{
	int ret, i;

	for (i = 0; i < NR_RETRY_CNT; i++) {
		/* Write the value to register */
		ret = max17042_write_reg(client, reg, value);
		if (ret < 0)
			continue;
		/* Read the value from register */
		ret = max17042_read_reg(client, reg);
		if (ret < 0)
			continue;
		/* compare the both the values */
		if (value != ret)
			dev_err(&client->dev,
				"write verify failed on Register:0x%x\n", reg);
		else
			break;
	}

	return ret;
}

static int max17042_reg_read_modify(struct i2c_client *client, u8 reg,
							u16 val, int bit_set)
{
	int ret;

	ret = max17042_read_reg(client, reg);
	if (ret < 0)
		return ret;

	if (bit_set)
		ret |= val;
	else
		ret &= (~val);

	ret = max17042_write_reg(client, reg, ret);
	return ret;
}

static irqreturn_t max17042_intr_handler(int id, void *dev)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t max17042_thread_handler(int id, void *dev)
{
	struct max17042_chip *chip = dev;
	struct device *device = &chip->client->dev;
	int stat, temp, val, count = 0;
	u16 processed, ignored, config;

	pm_runtime_get_sync(device);

	/* read current configuration */
	val = max17042_read_reg(chip->client, MAX17042_CONFIG);
	if (val < 0)
		config = fg_conf_data->cfg;
	else
		config = val;

	stat = max17042_read_reg(chip->client, MAX17042_STATUS);
	do {
		dev_dbg(device, "%s: Status-val: 0x%x\n", __func__, stat);
		if (stat < 0) {
			dev_err(device,
				"max17042-INTR: status read failed:%d\n", stat);
			pm_runtime_put_sync(device);
			return IRQ_HANDLED;
		}

		processed = 0;
		ignored = 0;

		if ((stat & STATUS_VMN_BIT) || (stat & STATUS_VMX_BIT)) {
			dev_info(device, "VOLT threshold INTR\n");
			/* nothing yet */
			if (stat & STATUS_VMN_BIT) {
				if (config & CONFIG_VSTICKY_BIT_SET)
					processed |= STATUS_VMN_BIT;
				else
					ignored |= STATUS_VMN_BIT;
			}
			if (stat & STATUS_VMX_BIT) {
				if (config & CONFIG_VSTICKY_BIT_SET)
					processed |= STATUS_VMX_BIT;
				else
					ignored |= STATUS_VMX_BIT;
			}
		}

		if ((stat & STATUS_SMN_BIT) || (stat & STATUS_SMX_BIT)) {
			dev_info(device, "SOC threshold INTR\n");
			/* Actual processing is done in evt_worker */
			/* so we might get interrupt again or miss */
			if (stat & STATUS_SMN_BIT) {
				if (config & CONFIG_SSTICKY_BIT_SET)
					processed |= STATUS_SMN_BIT;
				else
					ignored |= STATUS_SMN_BIT;
			}
			if (stat & STATUS_SMX_BIT) {
				if (config & CONFIG_SSTICKY_BIT_SET)
					processed |= STATUS_SMX_BIT;
				else
					ignored |= STATUS_SMX_BIT;
			}
		}

		if (stat & STATUS_BR_BIT) {
			dev_info(device, "Battery removed INTR\n");
			if ((config & CONFIG_BER_BIT_ENBL) &&
			    (stat & STATUS_BST_BIT)) {
				dev_warn(device, "battery unplugged\n");
				mutex_lock(&chip->batt_lock);
				chip->present = 0;
				mutex_unlock(&chip->batt_lock);
				kernel_power_off();
			}
			processed |= STATUS_BR_BIT;
		}

		if ((stat & STATUS_TMN_BIT) || (stat & STATUS_TMX_BIT)) {
			val = read_batt_pack_temp(chip, &temp);
			if (val) {
				dev_warn(device, "Can't read temp: %d\n", val);
			} else {
				val = max17042_read_reg(chip->client,
							MAX17042_TALRT_Th);
				dev_info(device,
					"Thermal threshold INTR: %d (%d, %d)\n",
					 temp, (int8_t)(val & 0xff),
					 (int8_t)(val >> 8));
			}
			if (stat & STATUS_TMN_BIT) {
				if (config & CONFIG_TSTICKY_BIT_SET)
					processed |= STATUS_TMN_BIT;
				else
					ignored |= STATUS_TMN_BIT;
			}
			if (stat & STATUS_TMX_BIT) {
				if (config & CONFIG_TSTICKY_BIT_SET)
					processed |= STATUS_TMX_BIT;
				else
					ignored |= STATUS_TMX_BIT;
			}
		}

		if (stat & STATUS_POR_BIT) {
			dev_info(device, "Power On Reset event\n");
			ignored |= STATUS_POR_BIT;
		}

		if (stat & STATUS_BST_BIT)
			ignored |= STATUS_BST_BIT;

		if (stat & STATUS_BI_BIT) {
			dev_info(device, "Battery Insert INTR\n");
			/* nothing yet */
			processed |= STATUS_BI_BIT;
		}

		/* clear int */
		max17042_reg_read_modify(chip->client, MAX17042_STATUS,
					 processed, 0);

		stat = max17042_read_reg(chip->client, MAX17042_STATUS);
	} while ((stat & STATUS_MASK & ~ignored) && (count++ < NR_RETRY_INT));

	/* update battery status and health */
	schedule_work(&chip->evt_worker);
	pm_runtime_put_sync(device);
	if (count >= NR_RETRY_INT) {
		dev_err(device, "%s: can't process all IRQ reasons: 0x%x\n",
			__func__, stat);
		/* desperate */
		max17042_write_reg(max17042_client, MAX17042_STATUS, 0x0000);
	}
	return IRQ_HANDLED;
}

static short adjust_sign_value(int value, int is_byte)
{
	short result, temp = (short)value;
	if (temp & MAX17042_SIGN_INDICATOR) {

		if (is_byte) {
			result = (~temp) >> 8;
			result &= 0xff;
		} else {
			result = ~temp;
		}

		result++;
		result *= -1;
	} else {
		if (is_byte)
			result = temp >> 8;
		else
			result = temp;
	}

	return result;
}

static int read_batt_pack_temp(struct max17042_chip *chip, int *temp)
{
	int ret;
	u16 val;

	/* Read battery pack temperature */
	if (chip->pdata->battery_pack_temp) {
		ret = chip->pdata->battery_pack_temp(temp);
		if (ret < 0)
			goto temp_read_err;

		/* Convert the temperature to 2's complement form.
		 * Most significant byte contains the decimal
		 * equivalent of the data */
		if (fg_conf_data->cfg & CONFIG_TEX_BIT_ENBL) {
			if (*temp < 0) {
				val = (*temp + 0xff + 1);
				val <<= 8;
			} else {
				val = *temp;
				val <<= 8;
			}
			ret = max17042_write_reg(chip->client,
							MAX17042_TEMP, val);
			if (ret < 0)
				dev_err(&chip->client->dev,
					"Temp write to maxim failed:%d", ret);
		}
	} else {
		ret = max17042_read_reg(chip->client, MAX17042_TEMP);
		if (ret < 0)
			goto temp_read_err;

		/* MAX17042_TEMP register gives the signed
		 * value and we are ignoring the lower byte
		 * which represents the decimal point */

		*temp = adjust_sign_value(ret, BYTE_VALUE);
	}
	return 0;

temp_read_err:
	dev_err(&chip->client->dev, "BP Temp read error:%d", ret);
	return ret;
}

static int max17042_set_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);
	int ret = 0;
	int8_t temp;

	mutex_lock(&chip->batt_lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		chip->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = max17042_read_reg(chip->client, MAX17042_TALRT_Th);
		if (ret < 0)
			break;
		temp = val->intval / 10; /* 0.1C prop to 1C reg */
		/* Force that min is under max */
		if (temp >= (int8_t)(ret >> 8))
			temp = (int8_t)(ret >> 8) - 1;
		ret = (ret & 0xff00) + (uint8_t)temp;
		ret = max17042_write_reg(chip->client, MAX17042_TALRT_Th, ret);
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = max17042_read_reg(chip->client, MAX17042_TALRT_Th);
		if (ret < 0)
			break;
		temp = val->intval / 10; /* 0.1C prop to 1C reg */
		/* Force that max is over min */
		if (temp <= (int8_t)(ret & 0xff))
			temp = (int8_t)(ret & 0xff) + 1;
		ret = (temp << 8) + (ret & 0xff);
		ret = max17042_write_reg(chip->client, MAX17042_TALRT_Th, ret);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&chip->batt_lock);

	return ret;
}

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17042_chip *chip = container_of(psy,
				struct max17042_chip, battery);
	short int cur;
	int volt_ocv, ret, batt_temp, batt_vmin;

	mutex_lock(&chip->batt_lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		/*
		 * status is being read from external
		 * module so check for error case before
		 * assigning to intval.
		 */
		if (chip->status < 0) {
			ret = chip->status;
			goto ps_prop_read_err;
		} else {
			val->intval = chip->status;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		/*
		 * health is being read from external
		 * module so check for error case before
		 * assigning to intval.
		 */
		if (chip->health < 0) {
			ret = chip->health;
			goto ps_prop_read_err;
		} else {
			val->intval = chip->health;
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = chip->technology;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->charge_full_des;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = max17042_read_reg(chip->client, MAX17042_RepCap);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = ret * MAX17042_CHRG_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = max17042_read_reg(chip->client, MAX17042_FullCAP);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = ret * MAX17042_CHRG_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = max17042_read_reg(chip->client, MAX17042_QH);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = ret * MAX17042_CHRG_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = max17042_read_reg(chip->client, MAX17042_Current);
		if (ret < 0)
			goto ps_prop_read_err;

		cur = adjust_sign_value(ret, WORD_VALUE);

		if (fg_conf_data->rsense)
			val->intval = (cur * MAX17042_CURR_CONV_FCTR)
						/ fg_conf_data->rsense;
		else
			val->intval = cur * MAX17042_CURR_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = max17042_read_reg(chip->client, MAX17042_AvgCurrent);
		if (ret < 0)
			goto ps_prop_read_err;

		cur = adjust_sign_value(ret, WORD_VALUE);

		if (fg_conf_data->rsense)
			val->intval = (cur * MAX17042_CURR_CONV_FCTR)
						/ fg_conf_data->rsense;
		else
			val->intval = cur * MAX17042_CURR_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (!chip->pdata->enable_current_sense ||
				chip->enable_fake_temp) {
			val->intval = CONSTANT_TEMP_IN_POWER_SUPPLY;
			break;
		}
		ret = read_batt_pack_temp(chip, &batt_temp);
		if (ret < 0)
			goto ps_prop_read_err;
		/*
		 * Temperature is measured in units of degrees celcius, the
		 * power_supply class measures temperature in tenths of degrees
		 * celsius.
		 */
		val->intval = batt_temp * 10;
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = max17042_read_reg(chip->client, MAX17042_TALRT_Th);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = ((int8_t)(ret & 0xff)) * 10; /* 0.1C */
		break;
	case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
		ret = max17042_read_reg(chip->client, MAX17042_TALRT_Th);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = ((int8_t)(ret >> 8)) * 10; /* 0.1C */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = max17042_read_reg(chip->client, MAX17042_VCELL);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = (ret >> 3) * MAX17042_VOLT_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = max17042_read_reg(chip->client, MAX17042_AvgVCELL);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = (ret >> 3) * MAX17042_VOLT_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = max17042_read_reg(chip->client, MAX17042_OCVInternal);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = (ret >> 3) * MAX17042_VOLT_CONV_FCTR;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		if (chip->chip_type == MAX17042)
			ret = max17042_read_reg(chip->client, MAX17042_V_empty);
		else
			ret = max17042_read_reg(chip->client, MAX17050_V_empty);
		if (ret < 0)
			goto ps_prop_read_err;
		val->intval = (ret >> 7) * 10000; /* Units of LSB = 10mV */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->voltage_max;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/*
		 * WA added to support power supply voltage
		 * variations b/w supply and FG readings.
		 */
		if (fake_batt_full) {
			val->intval = 100;
			break;
		}

		/* Voltage Based shutdown method to avoid modem crash */
		if (chip->pdata->is_volt_shutdown) {
			ret = max17042_read_reg(chip->client,
						MAX17042_OCVInternal);
			if (ret < 0)
				goto ps_prop_read_err;
			volt_ocv = (ret >> 3) * MAX17042_VOLT_CONV_FCTR;

			/* Get the minimum voltage thereshold */
			if (shutdown_volt)
				batt_vmin = shutdown_volt;
			else if (chip->pdata->get_vmin_threshold)
				batt_vmin = chip->pdata->get_vmin_threshold();
			else
				batt_vmin = BATTERY_VOLT_MIN_THRESHOLD;

			if (volt_ocv <= batt_vmin) {
				/* if user disables OCV shutdown method
				 * report 1% capcity so that platform
				 * will not get shutdown.
				 */
				if (chip->disable_shdwn_methods &
						SHUTDOWN_OCV_MASK_BIT)
					val->intval = 1;
				else
					val->intval = 0;
				break;
			}

		}

		/* Check for LOW Battery Shutdown mechanism is enabled */
		if (chip->pdata->is_lowbatt_shutdown &&
			(chip->health == POWER_SUPPLY_HEALTH_DEAD)) {
			/* if user disables LOWBATT INT shutdown method
			 * report 1% capcity so that platform
			 * will not get shutdown.
			 */
			if (chip->disable_shdwn_methods &
					SHUTDOWN_LOWBATT_MASK_BIT)
				val->intval = 1;
			else
				val->intval = 0;
			break;
		}

		/* If current sensing is not enabled then read the
		 * voltage based fuel gauge register for SOC */
		if (chip->pdata->enable_current_sense) {
			ret = max17042_read_reg(chip->client, MAX17042_RepSOC);
			if (ret < 0)
				goto ps_prop_read_err;
			val->intval = ret >> 8;
			/* Check if MSB of lower byte is set
			 * then round off the SOC to higher digit
			 */
			if ((ret & 0x80) && val->intval)
				val->intval += 1;
		} else {
			ret = max17042_read_reg(chip->client, MAX17042_VCELL);
			if (ret < 0)
				goto ps_prop_read_err;

			ret = (ret >> 3) * MAX17042_VOLT_CONV_FCTR / 1000;
			val->intval = voltage_capacity_lookup(ret);
		}

		if (val->intval > 100)
			val->intval = 100;

		/* if user disables default FG shutdown method
		 * report 1% capcity so that platform
		 * will not get shutdown.
		 */
		if ((val->intval == 0) && (chip->disable_shdwn_methods &
				SHUTDOWN_DEF_FG_MASK_BIT))
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		if (!strncmp(chip->pdata->battid, "UNKNOWNB", 8))
			val->strval = chip->pdata->battid;
		else
			val->strval = chip->pdata->model_name;
		break;
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		val->strval = chip->pdata->serial_num;
		break;
	default:
		mutex_unlock(&chip->batt_lock);
		return -EINVAL;
	}

	mutex_unlock(&chip->batt_lock);
	return 0;

ps_prop_read_err:
	mutex_unlock(&chip->batt_lock);
	return ret;
}

static void dump_fg_conf_data(struct max17042_chip *chip)
{
	int i;

	dev_info(&chip->client->dev, "size:%x\n", fg_conf_data->size);
	dev_info(&chip->client->dev, "table_type:%x\n",
					fg_conf_data->table_type);
	dev_info(&chip->client->dev, "config_init:%x\n",
					fg_conf_data->config_init);
	dev_info(&chip->client->dev, "rcomp0:%x\n", fg_conf_data->rcomp0);
	dev_info(&chip->client->dev, "tempCo:%x\n", fg_conf_data->tempCo);
	dev_info(&chip->client->dev, "kempty0:%x\n", fg_conf_data->kempty0);
	dev_info(&chip->client->dev, "full_cap:%x\n", fg_conf_data->full_cap);
	dev_info(&chip->client->dev, "cycles:%x\n", fg_conf_data->cycles);
	dev_info(&chip->client->dev, "full_capnom:%x\n",
						fg_conf_data->full_capnom);
	dev_info(&chip->client->dev, "qrtbl00:%x\n", fg_conf_data->qrtbl00);
	dev_info(&chip->client->dev, "qrtbl10:%x\n", fg_conf_data->qrtbl10);
	dev_info(&chip->client->dev, "qrtbl20:%x\n", fg_conf_data->qrtbl20);
	dev_info(&chip->client->dev, "qrtbl30:%x\n", fg_conf_data->qrtbl30);
	dev_info(&chip->client->dev, "full_soc_thr:%x\n",
						fg_conf_data->full_soc_thr);
	dev_info(&chip->client->dev, "vempty:%x\n", fg_conf_data->vempty);

	dev_info(&chip->client->dev, "soc_empty:%x\n",
						fg_conf_data->soc_empty);
	dev_info(&chip->client->dev, "ichgt_term:%x\n",
						fg_conf_data->ichgt_term);
	dev_info(&chip->client->dev, "design_cap:%x\n",
						fg_conf_data->design_cap);
	dev_info(&chip->client->dev, "etc:%x\n", fg_conf_data->etc);
	dev_info(&chip->client->dev, "rsense:%x\n", fg_conf_data->rsense);
	dev_info(&chip->client->dev, "cfg:%x\n", fg_conf_data->cfg);
	dev_info(&chip->client->dev, "learn_cfg:%x\n",
						fg_conf_data->learn_cfg);
	dev_info(&chip->client->dev, "filter_cfg:%x\n",
						fg_conf_data->filter_cfg);
	dev_info(&chip->client->dev, "relax_cfg:%x\n", fg_conf_data->relax_cfg);

	for (i = 0; i < CELL_CHAR_TBL_SAMPLES; i++)
		dev_info(&chip->client->dev, "%x, ",
				fg_conf_data->cell_char_tbl[i]);
	dev_info(&chip->client->dev, "\n");
}

static void enable_soft_POR(struct max17042_chip *chip)
{
	u16 val = 0x0000;

	max17042_write_reg(chip->client, MAX17042_MLOCKReg1, val);
	max17042_write_reg(chip->client, MAX17042_MLOCKReg2, val);
	max17042_write_reg(chip->client, MAX17042_STATUS, val);

	val = max17042_read_reg(chip->client, MAX17042_MLOCKReg1);
	if (val)
		dev_err(&chip->client->dev, "MLOCKReg1 read failed\n");

	val = max17042_read_reg(chip->client, MAX17042_MLOCKReg2);
	if (val)
		dev_err(&chip->client->dev, "MLOCKReg2 read failed\n");

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	if (val)
		dev_err(&chip->client->dev, "STATUS read failed\n");

	/* send POR command */
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, 0x000F);
	mdelay(2);

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	if (val & STATUS_POR_BIT)
		dev_info(&chip->client->dev, "SoftPOR done!\n");
	else
		dev_err(&chip->client->dev, "SoftPOR failed\n");
}

static int write_characterization_data(struct max17042_chip *chip)
{
	uint16_t cell_data[CELL_CHAR_TBL_SAMPLES];
	uint16_t temp_data[CELL_CHAR_TBL_SAMPLES];
	int i;
	u8 addr;

	memset(cell_data, 0x0, sizeof(cell_data));
	/* Unlock model access */
	max17042_write_reg(chip->client, MAX17042_MLOCKReg1, FG_MODEL_UNLOCK1);
	max17042_write_reg(chip->client, MAX17042_MLOCKReg2, FG_MODEL_UNLOCK2);
	addr = MAX17042_MODELChrTbl;

	/* write the 48 words */
	for (i = 0; i < CELL_CHAR_TBL_SAMPLES; i++)
		max17042_write_reg(chip->client, addr + i,
				fg_conf_data->cell_char_tbl[i]);

	/* read the 48 words */
	for (i = 0; i < CELL_CHAR_TBL_SAMPLES; i++)
		cell_data[i] = max17042_read_reg(chip->client, addr + i);

	/* compare the data */
	if (memcmp(cell_data, fg_conf_data->cell_char_tbl, sizeof(cell_data))) {
		dev_err(&chip->client->dev, "%s write failed\n", __func__);
		for (i = 0; i < CELL_CHAR_TBL_SAMPLES; i++)
			dev_err(&chip->client->dev, "0x%x,0x%x\n", cell_data[i],
						fg_conf_data->cell_char_tbl[i]);
		/* Lock Model access regs */
		max17042_write_reg(chip->client, MAX17042_MLOCKReg1,
								FG_MODEL_LOCK1);
		max17042_write_reg(chip->client, MAX17042_MLOCKReg2,
								FG_MODEL_LOCK2);
		return -EIO;
	}

	memset(temp_data, 0x0, sizeof(temp_data));
	/* Lock Model access regs */
	max17042_write_reg(chip->client, MAX17042_MLOCKReg1, FG_MODEL_LOCK1);
	max17042_write_reg(chip->client, MAX17042_MLOCKReg2, FG_MODEL_LOCK2);

	/* read the 48 words */
	for (i = 0; i < CELL_CHAR_TBL_SAMPLES; i++)
		cell_data[i] = max17042_read_reg(chip->client, addr + i);

	/* compare the data */
	if (memcmp(cell_data, temp_data, sizeof(temp_data))) {
		dev_err(&chip->client->dev, "%s verify failed\n", __func__);
		for (i = 0; i < CELL_CHAR_TBL_SAMPLES; i++)
			dev_err(&chip->client->dev, "0x%x, ", cell_data[i]);
		dev_err(&chip->client->dev, "\n");
		return -EIO;
	}

	return 0;
}

static void configure_learncfg(struct max17042_chip *chip)
{

	u16 cycles;

	/*assigning cycles value from restored data*/
	cycles = fg_conf_data->cycles;
	if ((cycles >= CYCLES_ROLLOVER_CUTOFF) &&
				(chip->chip_type == MAX17042))
		max17042_write_verify_reg(chip->client, MAX17042_LearnCFG,
						MAX17042_DEF_RO_LRNCFG);
	else
		max17042_write_reg(chip->client, MAX17042_LearnCFG,
						fg_conf_data->learn_cfg);
}

static void write_config_regs(struct max17042_chip *chip)
{
	max17042_write_reg(chip->client, MAX17042_CONFIG, fg_conf_data->cfg);
	configure_learncfg(chip);

	max17042_write_reg(chip->client, MAX17042_SHFTCFG,
						fg_conf_data->filter_cfg);
	max17042_write_reg(chip->client, MAX17042_RelaxCFG,
						fg_conf_data->relax_cfg);
	if (chip->chip_type == MAX17050)
		max17042_write_reg(chip->client, MAX17050_FullSOCThr,
					fg_conf_data->full_soc_thr);
}

static void write_custom_regs(struct max17042_chip *chip)
{
	max17042_write_verify_reg(chip->client, MAX17042_RCOMP0,
						fg_conf_data->rcomp0);
	max17042_write_verify_reg(chip->client, MAX17042_TempCo,
						fg_conf_data->tempCo);
	max17042_write_verify_reg(chip->client, MAX17042_ICHGTerm,
						fg_conf_data->ichgt_term);
	/* adjust Temperature gain and offset */
	max17042_write_reg(chip->client,
			MAX17042_TGAIN, chip->pdata->tgain);
	max17042_write_reg(chip->client,
			MAx17042_TOFF, chip->pdata->toff);

	if (chip->chip_type == MAX17042) {
		max17042_write_reg(chip->client, MAX17042_ETC,
						fg_conf_data->etc);
		max17042_write_verify_reg(chip->client, MAX17042_K_empty0,
						fg_conf_data->kempty0);
		max17042_write_verify_reg(chip->client, MAX17042_SOCempty,
						fg_conf_data->soc_empty);
		max17042_write_verify_reg(chip->client, MAX17042_V_empty,
						MAX17042_DEF_VEMPTY_VAL);

	} else {	/* chip type max17050 */
		max17042_write_verify_reg(chip->client, MAX17050_V_empty,
							fg_conf_data->vempty);
		max17042_write_verify_reg(chip->client, MAX17050_QRTbl00,
			fg_conf_data->qrtbl00 + chip->extra_resv_cap);
		max17042_write_verify_reg(chip->client, MAX17050_QRTbl10,
			fg_conf_data->qrtbl10 + chip->extra_resv_cap);
		max17042_write_verify_reg(chip->client, MAX17050_QRTbl20,
			fg_conf_data->qrtbl20 + chip->extra_resv_cap);
		max17042_write_verify_reg(chip->client, MAX17050_QRTbl30,
			fg_conf_data->qrtbl30 + chip->extra_resv_cap);
	}
}

static void update_capacity_regs(struct max17042_chip *chip)
{
	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
			MAX17042_MODEL_MUL_FACTOR(fg_conf_data->full_cap,
				chip->model_algo_factor)
					* fg_conf_data->rsense);
	max17042_write_verify_reg(chip->client, MAX17042_FullCAPNom,
			MAX17042_MODEL_MUL_FACTOR(fg_conf_data->full_cap,
				chip->model_algo_factor)
					* fg_conf_data->rsense);
	max17042_write_reg(chip->client, MAX17042_DesignCap,
			MAX17042_MODEL_MUL_FACTOR(fg_conf_data->full_cap,
				chip->model_algo_factor)
					* fg_conf_data->rsense);
}

static void reset_vfsoc0_reg(struct max17042_chip *chip)
{
	fg_vfSoc = max17042_read_reg(chip->client, MAX17042_VFSOC);
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, VFSOC0_UNLOCK);
	max17042_write_verify_reg(chip->client, MAX17042_VFSOC0, fg_vfSoc);
	max17042_write_reg(chip->client, MAX17042_VFSOC0Enable, VFSOC0_LOCK);
}

static void load_new_capacity_params(struct max17042_chip *chip, bool is_por)
{
	u16 rem_cap, rep_cap, dq_acc;

	if (is_por) {
		/* fg_vfSoc needs to shifted by 8 bits to get the
		 * perc in 1% accuracy, to get the right rem_cap multiply
		 * full_cap by model multiplication factor,fg_vfSoc
		 * and divide by 100
		 */
		rem_cap = ((fg_vfSoc >> 8) *
			(u32)(MAX17042_MODEL_MUL_FACTOR
				(fg_conf_data->full_cap,
					chip->model_algo_factor))) / 100;

		max17042_write_verify_reg(chip->client,
					MAX17042_RemCap, rem_cap);

		rep_cap = rem_cap;

		max17042_write_verify_reg(chip->client,
					MAX17042_RepCap, rep_cap);
	}

	/* Write dQ_acc to 200% of Capacity and dP_acc to 200% */
	dq_acc = MAX17042_MODEL_MUL_FACTOR(fg_conf_data->full_cap,
			chip->model_algo_factor) / dQ_ACC_DIV;
	max17042_write_verify_reg(chip->client, MAX17042_dQacc, dq_acc);
	max17042_write_verify_reg(chip->client, MAX17042_dPacc, dP_ACC_200);

	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
			fg_conf_data->full_cap
			* fg_conf_data->rsense);
	max17042_write_reg(chip->client, MAX17042_DesignCap,
			MAX17042_MODEL_MUL_FACTOR(fg_conf_data->full_cap,
			chip->model_algo_factor)
			* fg_conf_data->rsense);
	max17042_write_verify_reg(chip->client, MAX17042_FullCAPNom,
			MAX17042_MODEL_MUL_FACTOR(fg_conf_data->full_cap,
			chip->model_algo_factor)
			* fg_conf_data->rsense);
	/* Update SOC register with new SOC */
	max17042_write_reg(chip->client, MAX17042_RepSOC, fg_vfSoc);
}

static void update_runtime_params(struct max17042_chip *chip)
{

	fg_conf_data->rcomp0 = max17042_read_reg(chip->client,
							MAX17042_RCOMP0);
	fg_conf_data->tempCo = max17042_read_reg(chip->client,
							MAX17042_TempCo);
	/*
	 * Save only the original qrtbl register values ignoring the
	 * additionally reserved capacity. We deal with reserved
	 * capacity while restoring.
	 */
	if (chip->chip_type == MAX17050) {
		fg_conf_data->qrtbl00 = max17042_read_reg(chip->client,
			MAX17050_QRTbl00) - chip->extra_resv_cap;
		fg_conf_data->qrtbl10 = max17042_read_reg(chip->client,
			MAX17050_QRTbl10) - chip->extra_resv_cap;
		fg_conf_data->qrtbl20 = max17042_read_reg(chip->client,
			MAX17050_QRTbl20) - chip->extra_resv_cap;
		fg_conf_data->qrtbl30 = max17042_read_reg(chip->client,
			MAX17050_QRTbl30) - chip->extra_resv_cap;
	}

	fg_conf_data->full_capnom = max17042_read_reg(chip->client,
							MAX17042_FullCAPNom);
	fg_conf_data->full_cap = max17042_read_reg(chip->client,
							MAX17042_FullCAP);
	if (fg_conf_data->rsense) {
		fg_conf_data->full_capnom = MAX17042_MODEL_DIV_FACTOR(
			fg_conf_data->full_capnom, chip->model_algo_factor)
					/ fg_conf_data->rsense;

		fg_conf_data->full_cap /= fg_conf_data->rsense;
	}
	fg_conf_data->cycles = max17042_read_reg(chip->client,
							MAX17042_Cycles);

	/* Dump data before saving */
	dump_fg_conf_data(chip);
}

static void save_runtime_params(struct max17042_chip *chip)
{
	int size, retval;

	dev_dbg(&chip->client->dev, "%s\n", __func__);

	if (!chip->pdata->save_config_data || !chip->pdata->is_init_done)
		return ;

	update_runtime_params(chip);

	size = sizeof(*fg_conf_data) - sizeof(fg_conf_data->cell_char_tbl);
	retval = chip->pdata->save_config_data(DRV_NAME, fg_conf_data, size);
	if (retval < 0) {
		dev_err(&chip->client->dev, "%s failed\n", __func__);
		return ;
	}

}

static int init_max17042_chip(struct max17042_chip *chip)
{
	int ret = 0, val;
	bool is_por;

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	dev_info(&chip->client->dev, "Status reg: %x\n", val);

	if (val & STATUS_POR_BIT)
		is_por = true;
	else
		is_por = false;

	/* Initialize configuration */
	write_config_regs(chip);

	/* write cell characterization data */
	ret = write_characterization_data(chip);
	if (ret < 0)
		return ret;

	/* write custom parameters */
	write_custom_regs(chip);

	/* update capacity params */
	update_capacity_regs(chip);

	/* delay must be atleast 350mS to allow VFSOC
	 * to be calculated from the new configuration
	 */
	msleep(350);

	/* reset vfsoc0 reg */
	reset_vfsoc0_reg(chip);

	/* advance to coulomb counter mode */
	max17042_write_verify_reg(chip->client,
			MAX17042_Cycles, fg_conf_data->cycles);

	/* load new capacity params */
	load_new_capacity_params(chip, is_por);

	if (is_por) {
		/* Init complete, Clear the POR bit */
		val = max17042_read_reg(chip->client, MAX17042_STATUS);
		max17042_write_reg(chip->client, MAX17042_STATUS,
						val & (~STATUS_POR_BIT));
	}

	/* reset FullCap to non inflated value */
	max17042_write_verify_reg(chip->client, MAX17042_FullCAP,
			fg_conf_data->full_cap * fg_conf_data->rsense);

	return ret;
}

static void reset_max17042(struct max17042_chip *chip)
{
	/* do soft power reset */
	enable_soft_POR(chip);

	/* After Power up, the MAX17042 requires 500mS in order
	 * to perform signal debouncing and initial SOC reporting
	 */
	msleep(500);

	max17042_write_reg(chip->client, MAX17042_CONFIG, 0x2210);

	/* adjust Temperature gain and offset */
	max17042_write_reg(chip->client, MAX17042_TGAIN, NTC_47K_TGAIN);
	max17042_write_reg(chip->client, MAx17042_TOFF, NTC_47K_TOFF);
}

static void max17042_restore_conf_data(struct max17042_chip *chip)
{
	int retval = 0, size;

	/* return if lock already acquired */
	if (!mutex_trylock(&chip->init_lock))
		return;

	if (!chip->pdata->is_init_done && chip->pdata->restore_config_data) {
		retval = chip->pdata->restore_config_data(DRV_NAME,
					fg_conf_data, sizeof(*fg_conf_data));

		if (retval == -ENXIO) {		/* no device found */
			dev_err(&chip->client->dev, "device not found\n");
			chip->pdata->is_init_done = 1;
			chip->pdata->save_config_data = NULL;
		} else if (retval < 0) {	/* device not ready */
			dev_warn(&chip->client->dev, "device not ready\n");
		} else {			/* device ready */
			set_chip_config(chip);
			/* mark the dirty byte in non-volatile memory */
			if (!fg_conf_data->config_init && retval >= 0) {
				fg_conf_data->config_init = 0x1;
				size = sizeof(*fg_conf_data) -
					sizeof(fg_conf_data->cell_char_tbl);
				retval = chip->pdata->save_config_data(
					DRV_NAME, fg_conf_data, size);
				if (retval < 0)
					dev_err(&chip->client->dev,
						"%s failed\n", __func__);
			}
		}
	}
			if (chip->pdata->is_volt_shutdown_enabled)
				chip->pdata->is_volt_shutdown =
				chip->pdata->is_volt_shutdown_enabled();

			if (chip->pdata->is_lowbatt_shutdown_enabled)
				chip->pdata->is_lowbatt_shutdown =
				chip->pdata->is_lowbatt_shutdown_enabled();

	mutex_unlock(&chip->init_lock);
}

static void set_chip_config(struct max17042_chip *chip)
{
	int val, retval;

	/* Dump data after restoring */
	dump_fg_conf_data(chip);

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	dev_info(&chip->client->dev, "Status reg: %x\n", val);
	if (!fg_conf_data->config_init || (val & STATUS_POR_BIT)) {
		dev_info(&chip->client->dev, "Config data should be loaded\n");
		if (chip->pdata->reset_chip)
			reset_max17042(chip);
		retval = init_max17042_chip(chip);
		if (retval < 0) {
			dev_err(&chip->client->dev, "maxim chip init failed\n");
			reset_max17042(chip);
			chip->pdata->save_config_data = NULL;
		}
	}
	if (fg_conf_data->cfg & CONFIG_TEX_BIT_ENBL)
		schedule_delayed_work(&chip->temp_worker, 0);
	chip->pdata->is_init_done = 1;
	configure_interrupts(chip);

	/* multiply with 1000 to align with linux power supply sub system */
	chip->charge_full_des = (fg_conf_data->design_cap / 2) * 1000;
}

static void max17042_init_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
				struct max17042_chip, init_worker);

	dev_info(&chip->client->dev, "%s\n", __func__);
	max17042_restore_conf_data(chip);
}

static void max17042_temp_worker(struct work_struct *w)
{
	struct delayed_work *work = to_delayed_work(w);
	struct max17042_chip *chip = container_of(work,
				struct max17042_chip, temp_worker);
	int temp;
	read_batt_pack_temp(chip, &temp);
	schedule_delayed_work(&chip->temp_worker, TEMP_WRITE_INTERVAL);
}

/* Set the SOC threshold interrupt to offset percentage in S0 state */
static void set_soc_intr_thresholds_s0(struct max17042_chip *chip, int offset)
{
	u16 soc_tr;
	int soc, ret;

	/* program interrupt thesholds such that we should
	 * get interrupt for every 'offset' perc change in the soc
	 */
	ret = max17042_read_reg(chip->client, MAX17042_RepSOC);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"maxim RepSOC read failed:%d\n", ret);
		return ;
	}
	soc = ret >> 8;

	/* if upper threshold exceeds 100% then stop
	 * the interrupt for upper thresholds */
	 if ((soc + offset) > 100)
		soc_tr = 0xff << 8;
	else
		soc_tr = (soc + offset) << 8;

	/* if lower threshold falls
	 * below 1% limit it to 1% */
	if ((soc - offset) < 1)
		soc_tr |= 1;
	else
		soc_tr |= soc;

	dev_info(&chip->client->dev,
		"soc perc: soc: %d, offset: %d\n", soc, offset);
	ret = max17042_write_reg(chip->client, MAX17042_SALRT_Th, soc_tr);
	if (ret < 0)
		dev_err(&chip->client->dev,
			"SOC threshold write to maxim fail:%d", ret);
}

static void set_soc_intr_thresholds_s3(struct max17042_chip *chip)
{
	int ret, val, soc;

	if (chip->pdata->enable_current_sense)
		ret = max17042_read_reg(chip->client, MAX17042_RepSOC);
	else
		ret = max17042_read_reg(chip->client, MAX17042_VFSOC);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"maxim RepSOC read failed:%d\n", ret);
		return ;
	}
	val = ret;
	soc = val >> 8;
	/* Check if MSB of lower byte is set
	 * then round off the SOC to higher digit
	 */
	if (val & 0x80)
		soc += 1;

	/* If soc > 15% set the alert threshold to 15%
	 * else if soc > 4% set the threshold to 4%
	 * else set it to 1%
	 */
	if (soc > SOC_WARNING_LEVEL1)
		val = SOC_DEF_MAX_MIN1_THRLD;
	else if (soc > SOC_WARNING_LEVEL2)
		val = SOC_DEF_MAX_MIN2_THRLD;
	else
		val = SOC_DEF_MAX_MIN3_THRLD;

	max17042_write_reg(chip->client, MAX17042_SALRT_Th, val);
}

static int max17042_get_batt_health(void)
{
	struct max17042_chip *chip = i2c_get_clientdata(max17042_client);
	int vavg, temp, ret;
	int stat;

	if (!chip->pdata->valid_battery) {
		dev_err(&chip->client->dev, "Invalid battery detected");
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	ret = read_batt_pack_temp(chip, &temp);
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"battery pack temp read fail:%d", ret);
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	if ((temp <= chip->pdata->temp_min_lim) ||
			(temp >= chip->pdata->temp_max_lim)) {
		dev_info(&chip->client->dev,
			"Battery Over Temp condition Detected:%d\n", temp);
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	}

	stat = max17042_read_reg(chip->client, MAX17042_STATUS);
	if (stat < 0) {
		dev_err(&chip->client->dev, "error reading status register");
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}

	ret = max17042_read_reg(chip->client, MAX17042_AvgVCELL);
	if (ret < 0) {
		dev_err(&chip->client->dev, "Vavg read fail:%d", ret);
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	}
	/* get the voltage to milli volts */
	vavg = ((ret >> 3) * MAX17042_VOLT_CONV_FCTR) / 1000;
	if (vavg < chip->pdata->volt_min_lim) {
		dev_info(&chip->client->dev,
			"Low Battery condition Detected:%d\n", vavg);
		return POWER_SUPPLY_HEALTH_DEAD;
	}
	if (vavg > chip->pdata->volt_max_lim + VBATT_MAX_OFFSET) {
		dev_info(&chip->client->dev,
			"Battery Over Voltage condition Detected:%d\n", vavg);
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	if (stat & STATUS_VMX_BIT) {
		dev_info(&chip->client->dev,
			"Battery Over Voltage condition Detected:%d\n", vavg);
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}

	return POWER_SUPPLY_HEALTH_GOOD;
}

static void max17042_evt_worker(struct work_struct *work)
{
	struct max17042_chip *chip = container_of(work,
			  struct max17042_chip, evt_worker);
	int status = 0, health;

	pm_runtime_get_sync(&chip->client->dev);

	/* get the battery status */
	if (chip->pdata->battery_status)
		status = chip->pdata->battery_status();

	/* get the battery health */
	if (chip->pdata->battery_health)
		health = chip->pdata->battery_health();
	else
		health = max17042_get_batt_health();

	mutex_lock(&chip->batt_lock);
	if (chip->pdata->battery_status)
		chip->status = status;
	chip->health = health;
	mutex_unlock(&chip->batt_lock);

	/* Init maxim chip if it is not already initialized */
	if (!chip->pdata->is_init_done &&
	!chip->pdata->file_sys_storage_enabled)
		schedule_work(&chip->init_worker);

	power_supply_changed(&chip->battery);
	/* If charging is stopped and there is a sudden drop in SOC below
	 * minimum threshold currently set, we'll not get further interrupts.
	 * This call to set thresholds, will take care of this scenario.
	 */
	if (chip->pdata->soc_intr_mode_enabled)
		set_soc_intr_thresholds_s0(chip, SOC_INTR_S0_THR);
	pm_runtime_put_sync(&chip->client->dev);
}

static void max17042_external_power_changed(struct power_supply *psy)
{
	struct max17042_chip *chip = container_of(psy,
			struct max17042_chip, battery);
	schedule_work(&chip->evt_worker);
}

static bool is_battery_online(struct max17042_chip *chip)
{
	int val;
	bool online = false;

	val = max17042_read_reg(chip->client, MAX17042_STATUS);
	if (val < 0) {
		dev_info(&chip->client->dev, "i2c read error\n");
		return online;
	}

	/* check battery present bit */
	if (val & STATUS_BST_BIT)
		online = false;
	else
		online = true;

	return online;
}

static void init_battery_props(struct max17042_chip *chip)
{
	chip->present = 1;
	chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
	chip->health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chip->technology = chip->pdata->technology;
	chip->charge_full_des = BATT_CHRG_FULL_DES;
}

#ifdef CONFIG_DEBUG_FS
/**
 * max17042_show - debugfs: show the state of an endpoint.
 * @seq: The seq_file to write data to.
 * @unused: not used
 *
 * This debugfs entry shows the content of the register
 * given in the data parameter.
*/
static int max17042_show(struct seq_file *seq, void *unused)
{
	u16 val;
	long addr;

	if (kstrtol((char *)seq->private, 16, &addr))
		return -EINVAL;

	val = max17042_read_reg(max17042_client, addr);
	seq_printf(seq, "%x\n", val);

	return 0;
}

static int max17042_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max17042_show, inode->i_private);
}

static const struct file_operations max17042_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= max17042_dbgfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void max17042_create_debugfs(struct max17042_chip *chip)
{
	int i;
	struct dentry *entry;

	max17042_dbgfs_root = debugfs_create_dir(DRV_NAME, NULL);
	if (IS_ERR(max17042_dbgfs_root)) {
		dev_warn(&chip->client->dev, "DEBUGFS DIR create failed\n");
		return ;
	}

	for (i = 0; i < MAX17042_MAX_MEM; i++) {
		sprintf((char *)&max17042_dbg_regs[i], "%x", i);
		entry = debugfs_create_file(
					(const char *)&max17042_dbg_regs[i],
					S_IRUGO,
					max17042_dbgfs_root,
					&max17042_dbg_regs[i],
					&max17042_dbgfs_fops);
		if (IS_ERR(entry)) {
			debugfs_remove_recursive(max17042_dbgfs_root);
			max17042_dbgfs_root = NULL;
			dev_warn(&chip->client->dev,
					"DEBUGFS entry Create failed\n");
			return ;
		}
	}
}
static inline void max17042_remove_debugfs(struct max17042_chip *chip)
{
	if (max17042_dbgfs_root)
		debugfs_remove_recursive(max17042_dbgfs_root);
}
#else
static inline void max17042_create_debugfs(struct max17042_chip *chip)
{
}
static inline void max17042_remove_debugfs(struct max17042_chip *chip)
{
}
#endif
/**
 * override_shutdown_methods - sysfs to set disable_shdwn_methods
 * Parameter as define by sysfs interface
 * Context: can sleep
 *
 */
static ssize_t override_shutdown_methods(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	if (value > (SHUTDOWN_DEF_FG_MASK_BIT |
			SHUTDOWN_OCV_MASK_BIT |
			SHUTDOWN_LOWBATT_MASK_BIT))
		return -EINVAL;

	chip->disable_shdwn_methods = value;
	return count;
}

/**
 * get_shutdown_methods - sysfs get disable_shdwn_methods
 * Parameter as define by sysfs interface
 * Context: can sleep
 *
 */
static ssize_t get_shutdown_methods(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", chip->disable_shdwn_methods);
}

/**
 * get_shutdown_voltage_set_by_user - get function for sysfs shutdown_voltage
 * Parameters as defined by sysfs interface
 */
static ssize_t get_shutdown_voltage_set_by_user(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", shutdown_volt);
}

/**
 * set_shutdown_voltage - set function for sysfs shutdown_voltage
 * Parameters as defined by sysfs interface
 * shutdown_volt can take the values between 3.4V to 4.2V
 */
static ssize_t set_shutdown_voltage(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned long value;
	if (kstrtoul(buf, 10, &value))
		return -EINVAL;
	if ((value < VBATT_MIN) || (value > VBATT_MAX))
		return -EINVAL;
	shutdown_volt = value;
	return count;
}

/**
 * set_fake_temp_enable - sysfs to set enable_fake_temp
 * Parameter as define by sysfs interface
 */
static ssize_t set_fake_temp_enable(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 10, &value))
		return -EINVAL;

	/* allow only 0 or 1 */
	if (value > 1)
		return -EINVAL;

	if (value)
		chip->enable_fake_temp = true;
	else
		chip->enable_fake_temp = false;

	return count;
}

/**
 * get_fake_temp_enable - sysfs get enable_fake_temp
 * Parameter as define by sysfs interface
 * Context: can sleep
 */
static ssize_t get_fake_temp_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", chip->enable_fake_temp);
}

static void configure_interrupts(struct max17042_chip *chip)
{
	int ret;
	unsigned int edge_type;
	int vmax, vmin, reg_val;

	/* set SOC-alert threshold sholds to lowest value */
	max17042_write_reg(chip->client, MAX17042_SALRT_Th,
					SOC_DEF_MAX_MIN3_THRLD);

	/* enable Alerts for SOCRep */
	if (chip->pdata->enable_current_sense)
		max17042_write_reg(chip->client, MAX17042_MiscCFG,
						MISCCFG_CONFIG_REPSOC);
	else
		max17042_write_reg(chip->client, MAX17042_MiscCFG,
						MISCCFG_CONFIG_VFSOC);

	/* disable the T-alert sticky bit */
	max17042_reg_read_modify(chip->client, MAX17042_CONFIG,
					CONFIG_TSTICKY_BIT_SET, 0);

	/* Setting V-alrt threshold register to default values */
	if (chip->pdata->en_vmax_intr) {
		vmax = chip->pdata->volt_max_lim + VBATT_MAX_OFFSET;
		vmin = chip->pdata->volt_min_lim - VBATT_MIN_OFFSET;
		reg_val = ((vmax / VALERT_VOLT_OFFSET) << 8) |
				(vmin / VALERT_VOLT_OFFSET);
		max17042_write_reg(chip->client, MAX17042_VALRT_Th, reg_val);
	} else {
		max17042_write_reg(chip->client, MAX17042_VALRT_Th,
					VOLT_DEF_MAX_MIN_THRLD);
	}

	/* Setting T-alrt threshold register to default values */
	max17042_write_reg(chip->client, MAX17042_TALRT_Th,
					TEMP_DEF_MAX_MIN_THRLD);

	/* clear BI bit */
	max17042_reg_read_modify(chip->client, MAX17042_STATUS,
						STATUS_BI_BIT, 0);
	/* clear BR bit */
	max17042_reg_read_modify(chip->client, MAX17042_STATUS,
						STATUS_BR_BIT, 0);

	/* get interrupt edge type from ALP pin */
	if (fg_conf_data->cfg & CONFIG_ALP_BIT_ENBL)
		edge_type = IRQF_TRIGGER_RISING;
	else
		edge_type = IRQF_TRIGGER_FALLING;

	/* register interrupt */
	ret = request_threaded_irq(chip->client->irq,
					max17042_intr_handler,
					max17042_thread_handler,
					edge_type,
					DRV_NAME, chip);
	if (ret) {
		dev_warn(&chip->client->dev,
			"cannot get IRQ:%d\n", chip->client->irq);
		chip->client->irq = -1;
	} else {
		dev_info(&chip->client->dev, "IRQ No:%d\n", chip->client->irq);
	}

	/* enable interrupts */
	max17042_reg_read_modify(chip->client, MAX17042_CONFIG,
						CONFIG_ALRT_BIT_ENBL, 1);

	/* set the Interrupt threshold register for soc */
	if (chip->pdata->soc_intr_mode_enabled)
		set_soc_intr_thresholds_s0(chip, SOC_INTR_S0_THR);

	/*
	 * recheckthe battery present status to
	 * make sure we didn't miss any battery
	 * removal event and power off if battery
	 * is removed/unplugged.
	 */
	if ((fg_conf_data->cfg & CONFIG_BER_BIT_ENBL) &&
		!is_battery_online(chip)) {
		dev_warn(&chip->client->dev, "battery NOT present\n");
		mutex_lock(&chip->batt_lock);
		chip->present = 0;
		mutex_unlock(&chip->batt_lock);
		kernel_power_off();
	}
}

#ifdef CONFIG_ACPI
extern void *max17042_platform_data(void *info);
#endif
static int max17042_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17042_chip *chip;
	int ret, i, gpio;
	struct acpi_gpio_info gpio_info;

#ifdef CONFIG_XEN
	return -ENODEV;
#endif

#ifdef CONFIG_ACPI
	client->dev.platform_data = max17042_platform_data(NULL);
	gpio = acpi_get_gpio_by_index(&client->dev, 0, &gpio_info);
	client->irq = gpio_to_irq(gpio);
	ret = gpio_request_one(gpio, GPIOF_IN, client->name);
	if (ret < 0) {
		dev_warn(&client->dev, "gpio request failed.");
		return -EIO;
	}
#endif
	if (!client->dev.platform_data) {
		dev_err(&client->dev, "Platform Data is NULL");
		return -EFAULT;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev,
				"SM bus doesn't support DWORD transactions\n");
		return -EIO;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	fg_conf_data = kzalloc(sizeof(*fg_conf_data), GFP_KERNEL);
	if (!fg_conf_data) {
		dev_err(&client->dev, "mem alloc failed\n");
		kfree(chip);
		return -ENOMEM;
	}
	chip->client = client;
	chip->pdata = client->dev.platform_data;
	/* LSB offset for qrtbl registers is 0.25%
	 * ie, 0x04 = 1% reserved capacity
	 */
	chip->extra_resv_cap = 4 * chip->pdata->resv_cap;

	if (chip->pdata->get_vmax_threshold)
		chip->voltage_max = chip->pdata->get_vmax_threshold();
	else
		chip->voltage_max = VBATT_MAX;

	if (chip->pdata->fg_algo_model)
		chip->model_algo_factor = chip->pdata->fg_algo_model;
	else
		chip->model_algo_factor = 100;

	i2c_set_clientdata(client, chip);
	max17042_client = client;

	ret = max17042_read_reg(chip->client, MAX17042_DevName);
	if (ret < 0 && chip->pdata->reset_i2c_lines) {
		dev_warn(&client->dev, "reset i2c device:%d\n", ret);
		for (i = 0; i < NR_I2C_RESET_CNT; i++) {
			chip->pdata->reset_i2c_lines();
			ret = max17042_read_reg(chip->client, MAX17042_DevName);
			if (ret < 0)
				dev_warn(&client->dev,
						"reset i2c device:%d\n", ret);
			else
				break;
		}
	}

	if (ret == MAX17042_IC_VERSION) {
		dev_info(&client->dev, "chip type max17042 detected\n");
		chip->chip_type = MAX17042;
	} else if (ret == MAX17050_IC_VERSION) {
		dev_info(&client->dev, "chip type max17047/50 detected\n");
		chip->chip_type = MAX17050;
	} else {
		dev_err(&client->dev, "device version mismatch: %x\n", ret);
		kfree(chip);
		kfree(fg_conf_data);
		return -EIO;
	}

	/* init battery properties */
	init_battery_props(chip);
	INIT_WORK(&chip->init_worker, max17042_init_worker);
	INIT_WORK(&chip->evt_worker, max17042_evt_worker);
	INIT_DEFERRABLE_WORK(&chip->temp_worker, max17042_temp_worker);

	mutex_init(&chip->batt_lock);
	mutex_init(&chip->init_lock);

	/* disable the Alert pin before setting thresholds */
	max17042_reg_read_modify(client, MAX17042_CONFIG,
						CONFIG_ALRT_BIT_ENBL, 0);

	if (chip->pdata->enable_current_sense) {
		dev_info(&chip->client->dev, "current sensing enabled\n");
		/* Initialize the chip with battery config data */
		max17042_restore_conf_data(chip);
	} else {

		dev_info(&chip->client->dev, "current sensing NOT enabled\n");
		/* incase of invalid battery no need to init the FG chip */
		chip->pdata->is_init_done = 1;
		/* disable coulomb counter based fuel gauging */
		max17042_write_reg(chip->client, MAX17042_CGAIN,
						MAX17042_CGAIN_DISABLE);
		/* Enable voltage based Fuel Gauging */
		max17042_write_reg(chip->client, MAX17042_LearnCFG,
						MAX17042_EN_VOLT_FG);
		/* configure interrupts for SOCvf */
		max17042_write_reg(chip->client, MAX17042_MiscCFG,
						MAX17042_CFG_INTR_SOCVF);
	}

	chip->technology = chip->pdata->technology;

	if (chip->chip_type == MAX17042)
		chip->battery.name = "max17042_battery";
	else
		chip->battery.name = "max17047_battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.set_property	= max17042_set_property;
	chip->battery.property_is_privileged_read =
					max17042_property_is_privileged_read;
	chip->battery.external_power_changed = max17042_external_power_changed;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		kfree(fg_conf_data);
		return ret;
	}

	/* Init Runtime PM State */
	pm_runtime_put_noidle(&chip->client->dev);
	pm_schedule_suspend(&chip->client->dev, MSEC_PER_SEC);

	/* In case of power supply register INT now
	 * else the INT will registered after chip init.
	 */
	if (!chip->pdata->enable_current_sense)
		configure_interrupts(chip);

	if (chip->pdata->file_sys_storage_enabled)
		misc_register(&fg_helper);

	/* Create debugfs for maxim registers */
	max17042_create_debugfs(chip);

	/* create sysfs file to disable shutdown methods */
	ret = device_create_file(&client->dev,
			&dev_attr_disable_shutdown_methods);
	if (ret)
		dev_warn(&client->dev, "cannot create sysfs entry\n");

	/* create sysfs file to enter shutdown voltage */
	ret = device_create_file(&client->dev,
			&dev_attr_shutdown_voltage);
	if (ret)
		dev_warn(&client->dev, "cannot create sysfs entry\n");

	/* create sysfs file to enable fake battery temperature */
	ret = device_create_file(&client->dev,
			&dev_attr_enable_fake_temp);
	if (ret)
		dev_warn(&client->dev, "cannot create sysfs entry\n");

	/* Register reboot notifier callback */
	if (!chip->pdata->file_sys_storage_enabled)
		register_reboot_notifier(&max17042_reboot_notifier_block);
	schedule_work(&chip->evt_worker);

	return 0;
}

static int max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	if (chip->pdata->file_sys_storage_enabled)
		misc_deregister(&fg_helper);
	else
		unregister_reboot_notifier(&max17042_reboot_notifier_block);
	device_remove_file(&client->dev, &dev_attr_disable_shutdown_methods);
	device_remove_file(&client->dev, &dev_attr_shutdown_voltage);
	device_remove_file(&client->dev, &dev_attr_enable_fake_temp);
	max17042_remove_debugfs(chip);
	if (client->irq > 0)
		free_irq(client->irq, chip);
	power_supply_unregister(&chip->battery);
	pm_runtime_get_noresume(&chip->client->dev);
	kfree(chip);
	kfree(fg_conf_data);
	return 0;
}

#ifdef CONFIG_PM
static int max17042_suspend(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);

	/*
	 * disable irq here doesn't mean max17042 interrupt
	 * can't wake up system. max17042 interrupt is triggered
	 * by GPIO pin, which is always active.
	 * When resume callback calls enable_irq, kernel
	 * would deliver the buffered interrupt (if it has) to
	 * driver.
	 */
	if (chip->client->irq > 0) {
		/* set SOC alert thresholds */
		set_soc_intr_thresholds_s3(chip);
		/* setting Vmin(3300mV) threshold to wake the
		 * platfrom in under low battery conditions */
		max17042_write_reg(chip->client, MAX17042_VALRT_Th,
					VOLT_MIN_THRLD_ENBL);
		disable_irq(chip->client->irq);
		enable_irq_wake(chip->client->irq);
	}
	if (fg_conf_data->cfg & CONFIG_TEX_BIT_ENBL)
		cancel_delayed_work_sync(&chip->temp_worker);

	/* max17042 IC automatically goes into shutdown mode
	 * if the SCL and SDA were held low for more than
	 * timeout of SHDNTIMER register value
	 */
	dev_dbg(&chip->client->dev, "max17042 suspend\n");

	return 0;
}

static int max17042_resume(struct device *dev)
{
	struct max17042_chip *chip = dev_get_drvdata(dev);
	int vmax, vmin, reg_val;

	if (chip->client->irq > 0) {
		/* Setting V-alrt threshold register to default values */
		if (chip->pdata->en_vmax_intr) {
			vmax = chip->pdata->volt_max_lim +
					VBATT_MAX_OFFSET;
			vmin = chip->pdata->volt_min_lim -
					VBATT_MIN_OFFSET;
			reg_val = ((vmax / VALERT_VOLT_OFFSET) << 8) |
					(vmin / VALERT_VOLT_OFFSET);
			max17042_write_reg(chip->client, MAX17042_VALRT_Th,
						reg_val);
		} else {
			max17042_write_reg(chip->client, MAX17042_VALRT_Th,
					VOLT_DEF_MAX_MIN_THRLD);
		}
		/* set SOC-alert threshold sholds to lowest value */
		max17042_write_reg(chip->client, MAX17042_SALRT_Th,
					SOC_DEF_MAX_MIN3_THRLD);
		enable_irq(chip->client->irq);
		disable_irq_wake(chip->client->irq);
	}
	/* update battery status and health */
	schedule_work(&chip->evt_worker);
	if (fg_conf_data->cfg & CONFIG_TEX_BIT_ENBL)
		schedule_delayed_work(&chip->temp_worker, 0);

	/* max17042 IC automatically wakes up if any edge
	 * on SDCl or SDA if we set I2CSH of CONFG reg
	 */
	dev_dbg(&chip->client->dev, "max17042 resume\n");

	return 0;
}
#else
#define max17042_suspend NULL
#define max17042_resume NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int max17042_runtime_suspend(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int max17042_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int max17042_runtime_idle(struct device *dev)
{

	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#else
#define max17042_runtime_suspend	NULL
#define max17042_runtime_resume		NULL
#define max17042_runtime_idle		NULL
#endif

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ "max17047", 1 },
	{ "max17050", 2 },
	{ "MAX17042", 0 },
	{ "MAX17047", 1 },
	{ "MAX17050", 2 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static const struct dev_pm_ops max17042_pm_ops = {
	.suspend		= max17042_suspend,
	.resume			= max17042_resume,
	.runtime_suspend	= max17042_runtime_suspend,
	.runtime_resume		= max17042_runtime_resume,
	.runtime_idle		= max17042_runtime_idle,
};

#ifdef CONFIG_ACPI
static struct acpi_device_id max17042_acpi_match[] = {
	{"MAX17047", 0},
	{}
};
MODULE_DEVICE_TABLE(acpi, max17042_acpi_match);

#endif

static struct i2c_driver max17042_i2c_driver = {
	.driver	= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &max17042_pm_ops,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(max17042_acpi_match),
#endif
	},
	.probe		= max17042_probe,
	.remove		= max17042_remove,
	.id_table	= max17042_id,
};

static int max17042_reboot_callback(struct notifier_block *nfb,
					unsigned long event, void *data)
{
	struct max17042_chip *chip = i2c_get_clientdata(max17042_client);

	if (chip->pdata->enable_current_sense)
		save_runtime_params(chip);

	/* if the shutdown or reboot sequence started
	 * then block the access to maxim registers as chip
	 * cannot be recovered from broken i2c transactions
	 */
	mutex_lock(&chip->batt_lock);
	chip->plat_rebooting = true;
	mutex_unlock(&chip->batt_lock);

	return NOTIFY_OK;
}

static int __init max17042_init(void)
{
	return i2c_add_driver(&max17042_i2c_driver);
}
#ifdef CONFIG_ACPI
late_initcall(max17042_init);
#else
module_init(max17042_init);
#endif

static void __exit max17042_exit(void)
{
	i2c_del_driver(&max17042_i2c_driver);
}
module_exit(max17042_exit);

int __init set_fake_batt_full(char *p)
{
	fake_batt_full = true;
	return 0;
}

early_param("fake_batt_full", set_fake_batt_full);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");
