/* Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/reboot.h>
#include <linux/qpnp/qpnp-adc.h>

#define SMB135X_BITS_PER_REG	8

/* Mask/Bit helpers */
#define _SMB135X_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB135X_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB135X_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))

/* Config registers */
#define CFG_3_REG			0x03
#define CHG_ITERM_50MA			0x08
#define CHG_ITERM_100MA			0x10
#define CHG_ITERM_150MA			0x18
#define CHG_ITERM_200MA			0x20
#define CHG_ITERM_250MA			0x28
#define CHG_ITERM_300MA			0x00
#define CHG_ITERM_500MA			0x30
#define CHG_ITERM_600MA			0x38
#define CHG_ITERM_MASK			SMB135X_MASK(5, 3)
#define CHG_IR_COMP_DIS			0x00
#define CHG_IR_COMP_25MV		0x01
#define CHG_IR_COMP_50MV		0x02
#define CHG_IR_COMP_75MV		0x03
#define CHG_IR_COMP_100MV		0x04
#define CHG_IR_COMP_125MV		0x05
#define CHG_IR_COMP_150MV		0x06
#define CHG_IR_COMP_175MV		0x07
#define CHG_IR_COMP_MASK		SMB135X_MASK(2, 0)

#define CFG_4_REG			0x04
#define CHG_INHIBIT_MASK		SMB135X_MASK(7, 6)
#define CHG_INHIBIT_50MV_VAL		0x00
#define CHG_INHIBIT_100MV_VAL		0x40
#define CHG_INHIBIT_200MV_VAL		0x80
#define CHG_INHIBIT_300MV_VAL		0xC0

#define CFG_5_REG			0x05
#define RECHARGE_200MV_BIT		BIT(2)
#define USB_2_3_BIT			BIT(5)

#define CFG_A_REG			0x0A
#define DCIN_VOLT_SEL			SMB135X_MASK(7, 5)
#define DCIN_INPUT_MASK			SMB135X_MASK(4, 0)
#define DCIN_INPUT_FAC                  0x06

#define CFG_B_REG			0x0B
#define DCIN_AICL_BIT			BIT(2)

#define CFG_C_REG			0x0C
#define USBIN_VOLT_MODE_5V		0x00
#define USBIN_VOLT_MODE_9V		0x60
#define USBIN_VOLT_MODE_5V_TO_9V	0x20
#define USBIN_VOLT_MODE_MASK		SMB135X_MASK(7, 5)
#define USBIN_INPUT_MASK		SMB135X_MASK(4, 0)

#define CFG_D_REG			0x0D
#define AICL_GLITCH			BIT(3)
#define AICL_ENABLE			BIT(2)

#define CFG_E_REG			0x0E
#define POLARITY_100_500_BIT		BIT(2)
#define USB_CTRL_BY_PIN_BIT		BIT(1)

#define CFG_11_REG			0x11
#define PRIORITY_BIT			BIT(7)
#define AUTO_SRC_DET_BIT		BIT(0)

#define USBIN_DCIN_CFG_REG		0x12
#define USBIN_SUSPEND_VIA_COMMAND_BIT	BIT(6)
#define OTG_CURRENT_LIMIT_1000MA        0x0C
#define OTG_CURRENT_LIMIT_MASK          SMB135X_MASK(3, 2)

#define CFG_14_REG			0x14
#define CHG_EN_BY_PIN_BIT			BIT(7)
#define CHG_EN_ACTIVE_LOW_BIT		BIT(6)
#define PRE_TO_FAST_REQ_CMD_BIT		BIT(5)
#define DISABLE_CURRENT_TERM_BIT	BIT(3)
#define DISABLE_AUTO_RECHARGE_BIT	BIT(2)
#define EN_CHG_INHIBIT_BIT		BIT(0)

#define CFG_16_REG			0x16
#define SAFETY_TIME_EN_BIT		BIT(4)
#define SAFETY_TIME_MINUTES_MASK	SMB135X_MASK(3, 2)
#define SAFETY_TIME_MINUTES_SHIFT	2

#define CFG_17_REG			0x17
#define CHG_STAT_DISABLE_BIT		BIT(0)
#define CHG_STAT_ACTIVE_HIGH_BIT	BIT(1)
#define CHG_STAT_IRQ_ONLY_BIT		BIT(4)

#define CFG_18_REG			0x18
#define DCIN_CMD_BIT			BIT(3)

#define CFG_19_REG			0x19
#define BATT_MISSING_ALGO_BIT		BIT(2)
#define BATT_MISSING_THERM_BIT		BIT(1)

#define CFG_1A_REG			0x1A
#define HOT_SOFT_VFLOAT_COMP_EN_BIT	BIT(3)
#define COLD_SOFT_VFLOAT_COMP_EN_BIT	BIT(2)

#define CFG_1C_REG			0x1C
#define BATT_CURR_MASK			SMB135X_MASK(4, 0)
#define TRICK_CURR_MASK			SMB135X_MASK(7, 5)

#define VFLOAT_REG			0x1E
#define VFLOAT_MASK			SMB135X_MASK(5, 0)

#define VERSION1_REG			0x2A
#define VERSION1_MASK			SMB135X_MASK(7,	6)
#define VERSION1_SHIFT			6
#define VERSION2_REG			0x32
#define VERSION2_MASK			SMB135X_MASK(1,	0)
#define VERSION3_REG			0x34

/* Irq Config registers */
#define IRQ_CFG_REG			0x07
#define IRQ_BAT_HOT_COLD_HARD_BIT	BIT(7)
#define IRQ_BAT_HOT_COLD_SOFT_BIT	BIT(6)
#define IRQ_OTG_OVER_CURRENT_BIT	BIT(4)
#define IRQ_USBIN_UV_BIT		BIT(2)
#define IRQ_INTERNAL_TEMPERATURE_BIT	BIT(0)

#define IRQ2_CFG_REG			0x08
#define IRQ2_SAFETY_TIMER_BIT		BIT(7)
#define IRQ2_CHG_ERR_BIT		BIT(6)
#define IRQ2_CHG_PHASE_CHANGE_BIT	BIT(4)
#define IRQ2_CHG_INHIBIT_BIT		BIT(3)
#define IRQ2_POWER_OK_BIT		BIT(2)
#define IRQ2_BATT_MISSING_BIT		BIT(1)
#define IRQ2_VBAT_LOW_BIT		BIT(0)

#define IRQ3_CFG_REG			0x09
#define IRQ3_SRC_DETECT_BIT		BIT(2)
#define IRQ3_DCIN_UV_BIT		BIT(0)

/* Command Registers */
#define CMD_I2C_REG			0x40
#define ALLOW_VOLATILE_BIT		BIT(6)

#define CMD_INPUT_LIMIT			0x41
#define USB_SHUTDOWN_BIT		BIT(6)
#define DC_SHUTDOWN_BIT			BIT(5)
#define USE_REGISTER_FOR_CURRENT	BIT(2)
#define USB_100_500_AC_MASK		SMB135X_MASK(1, 0)
#define USB_100_VAL			0x02
#define USB_500_VAL			0x00
#define USB_AC_VAL			0x01

#define CMD_CHG_REG			0x42
#define CMD_CHG_EN			BIT(1)
#define OTG_EN				BIT(0)

/* Status registers */
#define STATUS_0_REG			0x46
#define AICL_DONE_BIT			BIT(7)

#define STATUS_1_REG			0x47
#define USING_USB_BIT			BIT(1)
#define USING_DC_BIT			BIT(0)

#define STATUS_4_REG			0x4A
#define BATT_NET_CHG_CURRENT_BIT	BIT(7)
#define BATT_LESS_THAN_2V		BIT(4)
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			SMB135X_MASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3
#define CHG_EN_BIT			BIT(0)

#define STATUS_5_REG			0x4B
#define CDP_BIT				BIT(7)
#define DCP_BIT				BIT(6)
#define OTHER_BIT			BIT(5)
#define SDP_BIT				BIT(4)
#define ACA_A_BIT			BIT(3)
#define ACA_B_BIT			BIT(2)
#define ACA_C_BIT			BIT(1)
#define ACA_DOCK_BIT			BIT(0)

#define STATUS_6_REG			0x4D
#define HVDCP_BIT			BIT(4)

#define STATUS_8_REG			0x4E
#define USBIN_9V			BIT(5)
#define USBIN_UNREG			BIT(4)
#define USBIN_LV			BIT(3)
#define DCIN_9V				BIT(2)
#define DCIN_UNREG			BIT(1)
#define DCIN_LV				BIT(0)

#define STATUS_9_REG			0x4F
#define REV_MASK			SMB135X_MASK(3, 0)

/* Irq Status registers */
#define IRQ_A_REG			0x50
#define IRQ_A_HOT_HARD_BIT		BIT(6)
#define IRQ_A_COLD_HARD_BIT		BIT(4)
#define IRQ_A_HOT_SOFT_BIT		BIT(2)
#define IRQ_A_COLD_SOFT_BIT		BIT(0)

#define IRQ_B_REG			0x51
#define IRQ_B_BATT_TERMINAL_BIT		BIT(6)
#define IRQ_B_BATT_MISSING_BIT		BIT(4)
#define IRQ_B_VBAT_LOW_BIT		BIT(2)
#define IRQ_B_TEMPERATURE_BIT		BIT(0)

#define IRQ_C_REG			0x52
#define IRQ_C_TERM_BIT			BIT(0)
#define IRQ_C_FAST_CHG_BIT		BIT(6)

#define IRQ_D_REG			0x53
#define IRQ_D_AICL_DONE_BIT		BIT(4)
#define IRQ_D_TIMEOUT_BIT		BIT(2)

#define IRQ_E_REG			0x54
#define IRQ_E_DC_OV_BIT			BIT(6)
#define IRQ_E_DC_UV_BIT			BIT(4)
#define IRQ_E_USB_OV_BIT		BIT(2)
#define IRQ_E_USB_UV_BIT		BIT(0)

#define IRQ_F_REG			0x55
#define IRQ_F_OTG_OC_BIT                BIT(6)
#define IRQ_F_POWER_OK_BIT		BIT(0)

#define IRQ_G_REG			0x56
#define IRQ_G_SRC_DETECT_BIT		BIT(6)

static int ignore_disconnect;
module_param(ignore_disconnect, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ignore_disconnect, "Ignore Disconnects due to"
					" Under/Over Voltage");
enum {
	WRKARND_USB100_BIT = BIT(0),
	WRKARND_APSD_FAIL = BIT(1),
};

enum {
	REV_1 = 1,	/* Rev 1.0 */
	REV_1_1 = 2,	/* Rev 1.1 */
	REV_2 = 3,		/* Rev 2 */
	REV_2_1 = 5,	/* Rev 2.1 */
	REV_MAX,
};

static char *revision_str[] = {
	[REV_1] = "rev1",
	[REV_1_1] = "rev1.1",
	[REV_2] = "rev2",
	[REV_2_1] = "rev2.1",
};

enum {
	V_SMB1356,
	V_SMB1357,
	V_SMB1358,
	V_SMB1359,
	V_MAX,
};

static char *version_str[] = {
	[V_SMB1356] = "smb1356",
	[V_SMB1357] = "smb1357",
	[V_SMB1358] = "smb1358",
	[V_SMB1359] = "smb1359",
};

enum {
	USER = BIT(0),
	THERMAL = BIT(1),
	CURRENT = BIT(2),
};

enum path_type {
	USB,
	DC,
};

static int chg_time[] = {
	192,
	384,
	768,
	1536,
};

struct smb135x_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct smb_wakeup_source {
	struct wakeup_source    source;
	unsigned long           disabled;
};

struct smb135x_chg {
	struct i2c_client		*client;
	struct device			*dev;
	struct mutex			read_write_lock;

	u8				revision;
	int				version;

	bool				chg_enabled;

	bool				usb_present;
	bool				dc_present;
	bool				dc_ov;

	bool				bmd_algo_disabled;
	bool				iterm_disabled;
	int				iterm_ma;
	int				vfloat_mv;
	int				safety_time;
	int				resume_delta_mv;
	int				fake_battery_soc;
	struct dentry			*debug_root;
	int				usb_current_arr_size;
	int				*usb_current_table;
	int				dc_current_arr_size;
	int				*dc_current_table;
	u8				irq_cfg_mask[3];

	/* psy */
	struct power_supply		*usb_psy;
	int				usb_psy_ma;
	struct power_supply		batt_psy;
	struct power_supply		dc_psy;
	struct power_supply		*bms_psy;
	int				dc_psy_type;
	int				dc_psy_ma;
	const char			*bms_psy_name;

	/* status tracking */
	bool				chg_done_batt_full;
	bool				batt_present;
	bool				batt_hot;
	bool				batt_cold;
	bool				batt_warm;
	bool				batt_cool;

	bool				resume_completed;
	bool				irq_waiting;
	u32				usb_suspended;
	u32				dc_suspended;
	struct mutex			path_suspend_lock;

	u32				peek_poke_address;
	struct smb135x_regulator	otg_vreg;
	int				skip_writes;
	int				skip_reads;
	u32				workaround_flags;
	bool				soft_vfloat_comp_disabled;
	struct mutex			irq_complete;
	struct regulator		*therm_bias_vreg;
	struct delayed_work		wireless_insertion_work;
	struct delayed_work		usb_insertion_work;

	int				thermal_levels;
	int				dc_thermal_levels;
	int				therm_lvl_sel;
	int				dc_therm_lvl_sel;
	unsigned int			*thermal_mitigation;
	unsigned int			*dc_thermal_mitigation;
	struct mutex			current_change_lock;
	struct mutex                    batti_change_lock;
	bool				factory_mode;
	int				batt_current_ma;
	int				apsd_rerun_cnt;
	struct smb_wakeup_source        smb_wake_source;
	struct delayed_work		heartbeat_work;
	int				ext_temp_volt_mv;
	int				ext_temp_soc;
	int				ext_high_temp;
	int				temp_check;
	int				bms_check;
	unsigned long			float_charge_start_time;
	struct delayed_work		aicl_check_work;
	struct delayed_work		src_removal_work;
	struct delayed_work		ocp_clear_work;
	bool				aicl_disabled;
	bool				aicl_weak_detect;
	int				charger_rate;
	struct delayed_work		rate_check_work;
	int				rate_check_count;
	struct notifier_block		smb_reboot;
	int				ir_comp_mv;
	bool				invalid_battery;
	struct qpnp_adc_tm_btm_param	vbat_monitor_params;
	struct qpnp_adc_tm_chip		*adc_tm_dev;
	struct qpnp_vadc_chip		*vadc_dev;
	unsigned int			low_voltage_uv;
	unsigned int                    max_voltage_uv;
	bool				shutdown_voltage_tripped;
	bool				poll_fast;
	bool				hvdcp_powerup;
	int				prev_batt_health;
};

static struct smb135x_chg *the_chip;

static int smb135x_float_voltage_set(struct smb135x_chg *chip, int vfloat_mv);
static int handle_usb_removal(struct smb135x_chg *chip);
static int notify_usb_removal(struct smb135x_chg *chip);
static int smb135x_setup_vbat_monitoring(struct smb135x_chg *chip);

static void smb_stay_awake(struct smb_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void smb_relax(struct smb_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static int __smb135x_read(struct smb135x_chg *chip, int reg,
				u8 *val)
{
	s32 ret;
	int i;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	for (i = 0; ((ret < 0) && (i < 5)); i++) {
		mdelay(5);
		ret = i2c_smbus_read_byte_data(chip->client, reg);
	}
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int __smb135x_write(struct smb135x_chg *chip, int reg,
						u8 val)
{
	s32 ret;

	if (chip->factory_mode)
		return 0;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	pr_debug("Writing 0x%02x=0x%02x\n", reg, val);
	return 0;
}

static int __smb135x_write_fac(struct smb135x_chg *chip, int reg,
						u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		dev_err(chip->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	pr_debug("Writing 0x%02x=0x%02x\n", reg, val);
	return 0;
}

static int smb135x_read(struct smb135x_chg *chip, int reg,
				u8 *val)
{
	int rc;

	if (chip->skip_reads) {
		*val = 0;
		return 0;
	}
	mutex_lock(&chip->read_write_lock);
	rc = __smb135x_read(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int smb135x_write(struct smb135x_chg *chip, int reg,
						u8 val)
{
	int rc;

	if (chip->skip_writes)
		return 0;

	mutex_lock(&chip->read_write_lock);
	rc = __smb135x_write(chip, reg, val);
	mutex_unlock(&chip->read_write_lock);

	return rc;
}

static int smb135x_masked_write(struct smb135x_chg *chip, int reg,
						u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	if (chip->skip_writes || chip->skip_reads)
		return 0;

	mutex_lock(&chip->read_write_lock);
	rc = __smb135x_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __smb135x_write(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

static int smb135x_masked_write_fac(struct smb135x_chg *chip, int reg,
						u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	if (chip->skip_writes || chip->skip_reads)
		return 0;

	mutex_lock(&chip->read_write_lock);
	rc = __smb135x_read(chip, reg, &temp);
	if (rc < 0) {
		dev_err(chip->dev, "read failed: reg=%03X, rc=%d\n", reg, rc);
		goto out;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = __smb135x_write_fac(chip, reg, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"write failed: reg=%03X, rc=%d\n", reg, rc);
	}
out:
	mutex_unlock(&chip->read_write_lock);
	return rc;
}

static int is_usb_plugged_in(struct smb135x_chg *chip)
{
	int rc;
	u8 reg = 0;
	u8 reg2 = 0;

	rc = smb135x_read(chip, IRQ_E_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq E rc = %d\n", rc);
		return 0;
	}

	rc = smb135x_read(chip, IRQ_G_REG, &reg2);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq G rc = %d\n", rc);
		return 0;
	}

	return !(reg & IRQ_E_USB_UV_BIT) || !!(reg2 & IRQ_G_SRC_DETECT_BIT);
}

static int is_dc_plugged_in(struct smb135x_chg *chip)
{
	int rc;
	u8 reg = 0;

	rc = smb135x_read(chip, IRQ_E_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq E rc = %d\n", rc);
		return 0;
	}
	return !(reg & IRQ_E_DC_OV_BIT) && !(reg & IRQ_E_DC_UV_BIT);
}

static int read_revision(struct smb135x_chg *chip, u8 *revision)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, STATUS_9_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 9 rc = %d\n", rc);
		return rc;
	}
	*revision = (reg & REV_MASK);
	return 0;
}

static int read_version1(struct smb135x_chg *chip, u8 *version)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, VERSION1_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read version 1 rc = %d\n", rc);
		return rc;
	}
	*version = (reg & VERSION1_MASK) >> VERSION1_SHIFT;
	return 0;
}

static int read_version2(struct smb135x_chg *chip, u8 *version)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, VERSION2_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read version 2 rc = %d\n", rc);
		return rc;
	}
	*version = (reg & VERSION2_MASK);
	return 0;
}

static int read_version3(struct smb135x_chg *chip, u8 *version)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, VERSION3_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read version 3 rc = %d\n", rc);
		return rc;
	}
	*version = reg;
	return 0;
}

#define TRIM_23_REG		0x23
#define CHECK_USB100_GOOD_BIT	BIT(1)
static bool is_usb100_broken(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, TRIM_23_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 9 rc = %d\n", rc);
		return rc;
	}
	return !!(reg & CHECK_USB100_GOOD_BIT);
}

static char *usb_type_str[] = {
	"ACA_DOCK",	/* bit 0 */
	"ACA_C",	/* bit 1 */
	"ACA_B",	/* bit 2 */
	"ACA_A",	/* bit 3 */
	"SDP",		/* bit 4 */
	"OTHER",	/* bit 5 */
	"DCP",		/* bit 6 */
	"CDP",		/* bit 7 */
	"NONE",		/* bit 8  error case */
};

/* helper to return the string of USB type */
static char *get_usb_type_name(u8 stat_5)
{
	unsigned long stat = stat_5;

	return usb_type_str[find_first_bit(&stat, SMB135X_BITS_PER_REG)];
}

static enum power_supply_type usb_type_enum[] = {
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 0 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 1 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 2 */
	POWER_SUPPLY_TYPE_USB_ACA,	/* bit 3 */
	POWER_SUPPLY_TYPE_USB,		/* bit 4 */
	POWER_SUPPLY_TYPE_UNKNOWN,	/* bit 5 */
	POWER_SUPPLY_TYPE_USB_DCP,	/* bit 6 */
	POWER_SUPPLY_TYPE_USB_CDP,	/* bit 7 */
	POWER_SUPPLY_TYPE_UNKNOWN,	/* bit 8 error case, report UNKNWON */
};

/* helper to return enum power_supply_type of USB type */
static enum power_supply_type get_usb_supply_type(u8 stat_5)
{
	unsigned long stat = stat_5;

	return usb_type_enum[find_first_bit(&stat, SMB135X_BITS_PER_REG)];
}

static enum power_supply_property smb135x_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS,
	POWER_SUPPLY_PROP_CHARGE_RATE,
	/* Block from Fuel Gauge */
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_HOTSPOT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	/* Notification from Fuel Gauge */
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static int smb135x_force_apsd(struct smb135x_chg *chip)
{
	int rc;

	dev_info(chip->dev, "Start APSD Rerun!\n");
	rc = smb135x_masked_write_fac(chip, CFG_C_REG,
				      USBIN_VOLT_MODE_MASK,
				      USBIN_VOLT_MODE_5V);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg C rc = %d\n", rc);
		goto force_apsd_err;
	}

	rc = smb135x_masked_write_fac(chip, CFG_C_REG,
				      USBIN_VOLT_MODE_MASK,
				      USBIN_VOLT_MODE_9V);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg C rc = %d\n", rc);
		goto force_apsd_err;
	}

	rc = smb135x_masked_write_fac(chip, CFG_11_REG,
				      AUTO_SRC_DET_BIT,
				      0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg 11 rc = %d\n", rc);
		goto force_apsd_err;
	}

	rc = smb135x_masked_write_fac(chip, CFG_11_REG,
				      AUTO_SRC_DET_BIT,
				      AUTO_SRC_DET_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg 11 rc = %d\n", rc);
		goto force_apsd_err;
	}

	rc = smb135x_masked_write_fac(chip, CFG_C_REG,
				      USBIN_VOLT_MODE_MASK,
				      USBIN_VOLT_MODE_5V);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg C rc = %d\n", rc);
		goto force_apsd_err;
	}

	/* RESET to Default 5V to 9V */
	rc = smb135x_masked_write_fac(chip, CFG_C_REG,
				      USBIN_VOLT_MODE_MASK,
				      USBIN_VOLT_MODE_5V_TO_9V);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set cfg C rc = %d\n", rc);

force_apsd_err:

	return rc;
}

static int smb135x_reset_vbat_monitoring(struct smb135x_chg *chip)
{
	int rc = 0;

	chip->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_DISABLE;

	rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					 &chip->vbat_monitor_params);

	if (rc)
		dev_err(chip->dev, "tm disable failed: %d\n", rc);

	return rc;
}

static int smb135x_get_prop_batt_status(struct smb135x_chg *chip,
					int *batt_stat)
{
	int rc;
	int status = POWER_SUPPLY_STATUS_DISCHARGING;
	u8 reg = 0;
	u8 chg_type;

	if ((is_usb_plugged_in(chip) || is_dc_plugged_in(chip)) &&
	    (chip->chg_done_batt_full || chip->float_charge_start_time)) {
		*batt_stat = POWER_SUPPLY_STATUS_FULL;
		return 0;
	}

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read STATUS_4_REG rc = %d\n", rc);
		*batt_stat = POWER_SUPPLY_STATUS_UNKNOWN;
		return rc;
	}

	if (reg & CHG_HOLD_OFF_BIT) {
		/*
		 * when chg hold off happens the battery is
		 * not charging
		 */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		goto out;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

	if (chg_type == BATT_NOT_CHG_VAL)
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else
		status = POWER_SUPPLY_STATUS_CHARGING;
out:
	pr_debug("STATUS_4_REG=%x\n", reg);
	*batt_stat = status;
	return 0;
}

static int smb135x_get_prop_batt_present(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;
	bool prev_batt_status;

	prev_batt_status = chip->batt_present;

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0)
		return chip->batt_present;

	/* treat battery gone if less than 2V */
	if (reg & BATT_LESS_THAN_2V)
		chip->batt_present = false;
	else
		chip->batt_present = true;

	if ((prev_batt_status != chip->batt_present) &&
	    (!prev_batt_status))
		smb135x_reset_vbat_monitoring(chip);

	return chip->batt_present;
}

static int smb135x_get_prop_charge_type(struct smb135x_chg *chip,
					int *charge_type)
{
	int rc;
	u8 reg;
	u8 chg_type;

	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0) {
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		return rc;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;
	if (chg_type == BATT_NOT_CHG_VAL)
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (chg_type == BATT_FAST_CHG_VAL)
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (chg_type == BATT_PRE_CHG_VAL)
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	*charge_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
	return 0;
}

#define DEFAULT_BATT_CAPACITY	50
static int smb135x_get_prop_batt_capacity(struct smb135x_chg *chip,
					  int *batt_cap)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->fake_battery_soc >= 0) {
		*batt_cap = chip->fake_battery_soc;
		return rc;
	}

	if (chip->shutdown_voltage_tripped && !chip->factory_mode) {
		if ((chip->usb_psy) && (!chip->hvdcp_powerup) &&
		    chip->usb_present) {
			power_supply_set_present(chip->usb_psy, false);
			power_supply_set_online(chip->usb_psy, false);
			chip->usb_present = false;
		}
		*batt_cap = 0;
		return rc;
	}

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (rc < 0)
			ret.intval = DEFAULT_BATT_CAPACITY;

		if ((rc == 0) && (ret.intval == 0) && !chip->factory_mode) {
			chip->shutdown_voltage_tripped = true;
			if ((chip->usb_psy) && (!chip->hvdcp_powerup) &&
			    chip->usb_present) {
				power_supply_set_present(chip->usb_psy, false);
				power_supply_set_online(chip->usb_psy, false);
				chip->usb_present = false;
			}
		}
		*batt_cap = ret.intval;
		return rc;
	}
	*batt_cap = DEFAULT_BATT_CAPACITY;
	return rc;
}

static int smb135x_get_prop_batt_health(struct smb135x_chg *chip,
					int *batt_health)
{
	if (chip->invalid_battery)
		*batt_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	else if (chip->batt_hot)
		*batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		*batt_health = POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		*batt_health = POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		*batt_health = POWER_SUPPLY_HEALTH_COOL;
	else
		*batt_health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int smb135x_set_prop_batt_health(struct smb135x_chg *chip, int health)
{
	switch (health) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
		chip->batt_hot = true;
		chip->batt_cold = false;
		chip->batt_warm = false;
		chip->batt_cool = false;
		break;
	case POWER_SUPPLY_HEALTH_COLD:
		chip->batt_cold = true;
		chip->batt_hot = false;
		chip->batt_warm = false;
		chip->batt_cool = false;
		break;
	case POWER_SUPPLY_HEALTH_WARM:
		chip->batt_warm = true;
		chip->batt_hot = false;
		chip->batt_cold = false;
		chip->batt_cool = false;
		break;
	case POWER_SUPPLY_HEALTH_COOL:
		chip->batt_cool = true;
		chip->batt_hot = false;
		chip->batt_cold = false;
		chip->batt_warm = false;
		break;
	case POWER_SUPPLY_HEALTH_GOOD:
	default:
		chip->batt_hot = false;
		chip->batt_cold = false;
		chip->batt_warm = false;
		chip->batt_cool = false;
	}

	return 0;
}

#define DEFAULT_BATT_VOLT_MV	4000
static int smb135x_get_prop_batt_voltage_now(struct smb135x_chg *chip,
					     int *volt_mv)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &ret);
		if (rc < 0) {
			*volt_mv = DEFAULT_BATT_VOLT_MV;
			return rc;
		}

		*volt_mv = ret.intval / 1000;
		return 0;
	}
	*volt_mv = DEFAULT_BATT_VOLT_MV;
	return -EINVAL;
}

static int smb135x_enable_volatile_writes(struct smb135x_chg *chip)
{
	int rc;

	rc = smb135x_masked_write(chip, CMD_I2C_REG,
			ALLOW_VOLATILE_BIT, ALLOW_VOLATILE_BIT);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set VOLATILE_W_PERM_BIT rc=%d\n", rc);

	return rc;
}

static int usb_current_table_smb1356[] = {
	180,
	240,
	270,
	285,
	300,
	330,
	360,
	390,
	420,
	540,
	570,
	600,
	660,
	720,
	840,
	900,
	960,
	1080,
	1110,
	1128,
	1146,
	1170,
	1182,
	1200,
	1230,
	1260,
	1380,
	1440,
	1560,
	1620,
	1680,
	1800
};

static int usb_current_table_smb1357_smb1358[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
	2050,
	2100,
	2300,
	2400,
	2500,
	3000
};

static int usb_current_table_smb1359[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
	2050,
	2100,
	2300,
	2400,
	2500
};

static int dc_current_table_smb1356[] = {
	180,
	240,
	270,
	285,
	300,
	330,
	360,
	390,
	420,
	540,
	570,
	600,
	660,
	720,
	840,
	870,
	900,
	960,
	1080,
	1110,
	1128,
	1146,
	1158,
	1170,
	1182,
	1200,
};

static int dc_current_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
};

#define CURRENT_100_MA		100
#define CURRENT_150_MA		150
#define CURRENT_500_MA		500
#define CURRENT_900_MA		900
#define SUSPEND_CURRENT_MA	2

static int __smb135x_usb_suspend(struct smb135x_chg *chip, bool suspend)
{
	int rc;

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT,
			USB_SHUTDOWN_BIT, suspend ? USB_SHUTDOWN_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set cfg 11 rc = %d\n", rc);
	return rc;
}

static int __smb135x_dc_suspend(struct smb135x_chg *chip, bool suspend)
{
	int rc = 0;

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT,
			DC_SHUTDOWN_BIT, suspend ? DC_SHUTDOWN_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set cfg 11 rc = %d\n", rc);
	return rc;
}

static int smb135x_path_suspend(struct smb135x_chg *chip, enum path_type path,
						int reason, bool suspend)
{
	int rc = 0;
	int suspended;
	int *path_suspended;
	int (*func)(struct smb135x_chg *chip, bool suspend);

	mutex_lock(&chip->path_suspend_lock);
	if (path == USB) {
		suspended = chip->usb_suspended;
		path_suspended = &chip->usb_suspended;
		func = __smb135x_usb_suspend;
	} else {
		suspended = chip->dc_suspended;
		path_suspended = &chip->dc_suspended;
		func = __smb135x_dc_suspend;
	}

	if (suspend == false)
		suspended &= ~reason;
	else
		suspended |= reason;

	if (*path_suspended && !suspended)
		rc = func(chip, 0);
	if (!(*path_suspended) && suspended)
		rc = func(chip, 1);

	if (rc)
		dev_err(chip->dev, "Couldn't set/unset suspend for %s path rc = %d\n",
					path == USB ? "usb" : "dc",
					rc);
	else
		*path_suspended = suspended;

	mutex_unlock(&chip->path_suspend_lock);
	return rc;
}
static int smb135x_temp_charging(struct smb135x_chg *chip, int enable)
{
	int rc = 0;

	pr_debug("charging enable = %d\n", enable);

	if (chip->invalid_battery)
		enable = false;

	rc = smb135x_masked_write(chip, CMD_CHG_REG,
			CMD_CHG_EN, enable ? CMD_CHG_EN : 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set CHG_ENABLE_BIT enable = %d rc = %d\n",
			enable, rc);
		return rc;
	}
	chip->chg_enabled = enable;

	return 0;
}

static void smb135x_set_chrg_path_temp(struct smb135x_chg *chip)
{
	if (chip->batt_cool && !chip->ext_high_temp)
		smb135x_float_voltage_set(chip,
					  chip->ext_temp_volt_mv);
	else
		smb135x_float_voltage_set(chip, chip->vfloat_mv);

	if (chip->ext_high_temp ||
	    chip->batt_cold ||
	    chip->batt_hot ||
	    chip->chg_done_batt_full)
		smb135x_temp_charging(chip, 0);
	else
		smb135x_temp_charging(chip, 1);
}

static bool smb135x_is_max_thermal_level(struct smb135x_chg *chip)
{
	if (((chip->thermal_levels > 0) &&
	     ((chip->usb_present) &&
	      ((chip->therm_lvl_sel >= (chip->thermal_levels - 1)) ||
	       (chip->therm_lvl_sel == -EINVAL)))) ||
	    ((chip->dc_thermal_levels > 0) &&
	     (chip->dc_present) &&
	     ((chip->dc_therm_lvl_sel >= (chip->dc_thermal_levels - 1)) ||
	      (chip->dc_therm_lvl_sel == -EINVAL))))
		return true;
	else
		return false;
}

static int smb135x_check_temp_range(struct smb135x_chg *chip)
{
	int batt_volt;
	int batt_soc;
	int batt_health;
	int ext_high_temp = 0;

	if (smb135x_get_prop_batt_voltage_now(chip, &batt_volt))
		return 0;

	if (smb135x_get_prop_batt_capacity(chip, &batt_soc))
		return 0;

	if (smb135x_get_prop_batt_health(chip, &batt_health))
		return 0;

	if (((chip->batt_cool) &&
	     (batt_volt > chip->ext_temp_volt_mv)) ||
	    ((chip->batt_warm) &&
	     (batt_soc > chip->ext_temp_soc) &&
	     (smb135x_is_max_thermal_level(chip))))
		ext_high_temp = 1;

	if ((chip->prev_batt_health == POWER_SUPPLY_HEALTH_COOL) &&
	    (batt_health == POWER_SUPPLY_HEALTH_COOL) &&
	    !chip->ext_high_temp)
		ext_high_temp = 0;

	chip->prev_batt_health = batt_health;

	if (chip->ext_high_temp != ext_high_temp) {
		chip->ext_high_temp = ext_high_temp;

		dev_warn(chip->dev, "Ext High = %s\n",
			 chip->ext_high_temp ? "High" : "Low");

		return 1;
	}

	return 0;
}

static int smb135x_set_high_usb_chg_current(struct smb135x_chg *chip,
							int current_ma)
{
	int i, rc;
	u8 usb_cur_val;

	for (i = chip->usb_current_arr_size - 1; i >= 0; i--) {
		if (current_ma >= chip->usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_150_MA);
		rc = smb135x_masked_write(chip, CFG_5_REG,
						USB_2_3_BIT, USB_2_3_BIT);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_100_VAL);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_150_MA, rc);
		return rc;
	}

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smb135x_masked_write(chip, CFG_C_REG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
		return rc;
	}

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT,
					USB_100_500_AC_MASK, USB_AC_VAL);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write cfg 5 rc = %d\n", rc);
	return rc;
}

#define MAX_VERSION			0xF
#define USB_100_PROBLEM_VERSION		0x2
/* if APSD results are used
 *	if SDP is detected it will look at 500mA setting
 *		if set it will draw 500mA
 *		if unset it will draw 100mA
 *	if CDP/DCP it will look at 0x0C setting
 *		i.e. values in 0x41[1, 0] does not matter
 */
static int smb135x_set_usb_chg_current(struct smb135x_chg *chip,
							int current_ma)
{
	int rc;

	pr_debug("USB current_ma = %d\n", current_ma);

	if (chip->workaround_flags & WRKARND_USB100_BIT) {
		pr_info("USB requested = %dmA using %dmA\n", current_ma,
						CURRENT_500_MA);
		current_ma = CURRENT_500_MA;
	}

	if (current_ma == 0)
		/* choose the lowest available value of 100mA */
		current_ma = CURRENT_100_MA;

	if (current_ma == SUSPEND_CURRENT_MA) {
		/* force suspend bit */
		rc = smb135x_path_suspend(chip, USB, CURRENT, true);
		goto out;
	}
	if (current_ma < CURRENT_150_MA) {
		/* force 100mA */
		rc = smb135x_masked_write(chip, CFG_5_REG, USB_2_3_BIT, 0);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_100_VAL);
		rc |= smb135x_path_suspend(chip, USB, CURRENT, false);
		goto out;
	}
	/* specific current values */
	if (current_ma == CURRENT_150_MA) {
		rc = smb135x_masked_write(chip, CFG_5_REG,
						USB_2_3_BIT, USB_2_3_BIT);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_100_VAL);
		rc |= smb135x_path_suspend(chip, USB, CURRENT, false);
		goto out;
	}
	if (current_ma == CURRENT_500_MA) {
		rc = smb135x_masked_write(chip, CFG_5_REG, USB_2_3_BIT, 0);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_500_VAL);
		rc |= smb135x_path_suspend(chip, USB, CURRENT, false);
		goto out;
	}
	if (current_ma == CURRENT_900_MA) {
		rc = smb135x_masked_write(chip, CFG_5_REG,
						USB_2_3_BIT, USB_2_3_BIT);
		rc |= smb135x_masked_write(chip, CMD_INPUT_LIMIT,
				USB_100_500_AC_MASK, USB_500_VAL);
		rc |= smb135x_path_suspend(chip, USB, CURRENT, false);
		goto out;
	}

	rc = smb135x_set_high_usb_chg_current(chip, current_ma);
	rc |= smb135x_path_suspend(chip, USB, CURRENT, false);
out:
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set %dmA rc = %d\n", current_ma, rc);
	return rc;
}

static int smb135x_set_dc_chg_current(struct smb135x_chg *chip,
							int current_ma)
{
	int i, rc;
	u8 dc_cur_val;

	for (i = chip->dc_current_arr_size - 1; i >= 0; i--) {
		if (current_ma >= chip->dc_current_table[i])
			break;
	}
	dc_cur_val = i & DCIN_INPUT_MASK;
	rc = smb135x_masked_write(chip, CFG_A_REG,
				DCIN_INPUT_MASK, dc_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set dc charge current rc = %d\n",
				rc);
		return rc;
	}
	return 0;
}

static int smb135x_set_appropriate_current(struct smb135x_chg *chip,
						enum path_type path)
{
	int therm_ma, current_ma;
	int path_current = (path == USB) ? chip->usb_psy_ma : chip->dc_psy_ma;
	int (*func)(struct smb135x_chg *chip, int current_ma);
	int rc = 0;

	/*
	 * If battery is absent do not modify the current at all, these
	 * would be some appropriate values set by the bootloader or default
	 * configuration and since it is the only source of power we should
	 * not change it
	 */
	if (!chip->batt_present) {
		pr_debug("ignoring current request since battery is absent\n");
		return 0;
	}

	if (path == USB) {
		path_current = chip->usb_psy_ma;
		func = smb135x_set_usb_chg_current;
	} else {
		path_current = chip->dc_psy_ma;
		func = smb135x_set_dc_chg_current;
		if (chip->dc_psy_type == -EINVAL)
			func = NULL;
	}

	if ((path == DC) &&
	    (chip->dc_thermal_mitigation) &&
	    (chip->dc_therm_lvl_sel > 0) &&
	    (chip->dc_therm_lvl_sel < chip->dc_thermal_levels))
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		therm_ma = chip->dc_thermal_mitigation[chip->dc_therm_lvl_sel];
	else
		therm_ma = path_current;

	current_ma = min(therm_ma, path_current);
	if (func != NULL)
		rc = func(chip, current_ma);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set %s current to min(%d, %d)rc = %d\n",
				path == USB ? "usb" : "dc",
				therm_ma, path_current,
				rc);
	return rc;
}

int batt_current_table[] = {
	300,
	400,
	450,
	475,
	500,
	550,
	600,
	650,
	700,
	900,
	950,
	1000,
	1100,
	1200,
	1400,
	1450,
	1500,
	1600,
	1800,
	1850,
	1880,
	1910,
	1930,
	1950,
	1970,
	2000,
	2050,
	2100,
	2300,
	2400,
	2500
};

static int smb135x_set_batt_current(struct smb135x_chg *chip,
				   int current_ma)
{
	int i, rc;
	u8 batt_cur_val;

	for (i = ARRAY_SIZE(batt_current_table) - 1; i >= 0; i--) {
		if (current_ma >= batt_current_table[i])
			break;
	}
	batt_cur_val = i & BATT_CURR_MASK;
	rc = smb135x_masked_write(chip, CFG_1C_REG,
				  BATT_CURR_MASK, batt_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set Battery current rc = %d\n",
			rc);
		return rc;
	}
	return 0;
}

static int __smb135x_charging(struct smb135x_chg *chip, int enable)
{
	int rc = 0;

	pr_debug("charging enable = %d\n", enable);

	if (chip->invalid_battery)
		enable = false;

	rc = smb135x_masked_write(chip, CMD_CHG_REG,
			CMD_CHG_EN, enable ? CMD_CHG_EN : 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set CHG_ENABLE_BIT enable = %d rc = %d\n",
			enable, rc);
		return rc;
	}
	chip->chg_enabled = enable;

	/* set the suspended status */
	rc = smb135x_path_suspend(chip, DC, USER, !enable);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set dc suspend to %d rc = %d\n",
			enable, rc);
		return rc;
	}
	rc = smb135x_path_suspend(chip, USB, USER, !enable);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set usb suspend to %d rc = %d\n",
			enable, rc);
		return rc;
	}

	pr_debug("charging %s\n",
			enable ?  "enabled" : "disabled running from batt");
	return rc;
}

static int smb135x_charging(struct smb135x_chg *chip, int enable)
{
	int rc = 0;

	pr_debug("charging enable = %d\n", enable);

	__smb135x_charging(chip, enable);

	if (chip->usb_psy) {
		pr_debug("usb psy changed\n");
		power_supply_changed(chip->usb_psy);
	}
	if (chip->dc_psy_type != -EINVAL) {
		pr_debug("dc psy changed\n");
		power_supply_changed(&chip->dc_psy);
	}
	pr_debug("charging %s\n",
			enable ?  "enabled" : "disabled running from batt");
	return rc;
}

static int smb135x_system_temp_level_set(struct smb135x_chg *chip,
								int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->thermal_mitigation) {
		pr_err("Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		pr_err("Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->thermal_levels) {
		pr_err("Unsupported level selected %d forcing %d\n", lvl_sel,
				chip->thermal_levels - 1);
		lvl_sel = chip->thermal_levels - 1;
	}

	if (lvl_sel == chip->therm_lvl_sel)
		return 0;

	mutex_lock(&chip->current_change_lock);
	prev_therm_lvl = chip->therm_lvl_sel;
	chip->therm_lvl_sel = lvl_sel;
	if ((chip->therm_lvl_sel == (chip->thermal_levels - 1)) &&
	    (chip->thermal_mitigation[lvl_sel] == 0)) {
		/*
		 * Disable charging if highest value selected by
		 * setting the USB path in suspend
		 */
		rc = smb135x_path_suspend(chip, USB, THERMAL, true);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
		goto out;
	}

	rc = smb135x_set_batt_current(chip, chip->thermal_mitigation[lvl_sel]);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set batt current rc %d\n", rc);

	if (prev_therm_lvl == chip->thermal_levels - 1) {
		/*
		 * If previously highest value was selected charging must have
		 * been disabed. Enable charging by taking the USB path out of
		 * suspend.
		 */
		rc = smb135x_path_suspend(chip, USB, THERMAL, false);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int smb135x_dc_system_temp_level_set(struct smb135x_chg *chip,
					    int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->dc_thermal_mitigation) {
		pr_err("Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		pr_err("Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->dc_thermal_levels) {
		pr_err("Unsupported DC level selected %d forcing %d\n", lvl_sel,
				chip->dc_thermal_levels - 1);
		lvl_sel = chip->dc_thermal_levels - 1;
	}

	if (lvl_sel == chip->dc_therm_lvl_sel)
		return 0;

	mutex_lock(&chip->current_change_lock);
	prev_therm_lvl = chip->dc_therm_lvl_sel;
	chip->dc_therm_lvl_sel = lvl_sel;
	if ((chip->dc_therm_lvl_sel == (chip->dc_thermal_levels - 1)) &&
	    (chip->dc_thermal_mitigation[lvl_sel] == 0)) {
		/*
		 * Disable charging if highest value selected by
		 * setting the DC and USB path in suspend
		 */
		rc = smb135x_path_suspend(chip, DC, THERMAL, true);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
		goto out;
	}

	smb135x_set_appropriate_current(chip, DC);

	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set dc current rc %d\n", rc);

	if (prev_therm_lvl == chip->dc_thermal_levels - 1) {
		/*
		 * If previously highest value was selected charging must have
		 * been disabed. Enable charging by taking the DC out of
		 * suspend.
		 */
		rc = smb135x_path_suspend(chip, DC, THERMAL, false);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int smb135x_bms_set_property(struct smb135x_chg *chip,
				    enum power_supply_property prop,
				    const union power_supply_propval *val)
{
	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		chip->bms_psy->set_property(chip->bms_psy,
					    prop, val);
		return 0;
	}

	return -EINVAL;
}
static int smb135x_battery_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smb135x_charging(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		chip->fake_battery_soc = val->intval;
		power_supply_changed(&chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		smb135x_system_temp_level_set(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		smb_stay_awake(&chip->smb_wake_source);
		chip->bms_check = 1;
		cancel_delayed_work(&chip->heartbeat_work);
		schedule_delayed_work(&chip->heartbeat_work,
			msecs_to_jiffies(0));
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		smb_stay_awake(&chip->smb_wake_source);
		smb135x_set_prop_batt_health(chip, val->intval);
		smb135x_check_temp_range(chip);
		smb135x_set_chrg_path_temp(chip);
		chip->temp_check = 1;
		cancel_delayed_work(&chip->heartbeat_work);
		schedule_delayed_work(&chip->heartbeat_work,
			msecs_to_jiffies(0));
		break;
	/* Block from Fuel Gauge */
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		smb135x_bms_set_property(chip, prop, val);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb135x_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int smb135x_bms_get_property(struct smb135x_chg *chip,
				    enum power_supply_property prop,
				    int *bms_prop)
{
	int rc;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	if (chip->bms_psy) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
						 prop, &ret);
		*bms_prop = ret.intval;
		return rc;
	}

	return -EINVAL;
}

static int smb135x_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	int stat_val;
	int rc;
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = smb135x_get_prop_batt_status(chip, &stat_val);
		val->intval = stat_val;
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb135x_get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = smb135x_get_prop_charge_type(chip, &stat_val);
		val->intval = stat_val;
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = smb135x_get_prop_batt_capacity(chip, &stat_val);
		val->intval = stat_val;
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = smb135x_get_prop_batt_health(chip, &stat_val);
		val->intval = stat_val;
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = chip->therm_lvl_sel;
		break;
	case POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS:
		val->intval = chip->thermal_levels;
		break;
	case POWER_SUPPLY_PROP_CHARGE_RATE:
		val->intval = chip->charger_rate;
		break;
	/* Block from Fuel Gauge */
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		rc = smb135x_bms_get_property(chip, prop, &stat_val);
		val->intval = stat_val;
		if (rc < 0)
			return rc;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smb135x_dc_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS,
};

static int smb135x_dc_set_property(struct power_supply *psy,
				   enum power_supply_property prop,
				   const union power_supply_propval *val)
{
	struct smb135x_chg *chip = container_of(psy,
						struct smb135x_chg, dc_psy);
	switch (prop) {
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		smb135x_dc_system_temp_level_set(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smb135x_dc_is_writeable(struct power_supply *psy,
				   enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int smb135x_dc_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->dc_present;
		if (chip->shutdown_voltage_tripped && !chip->factory_mode)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->chg_enabled ? chip->dc_present : 0;
		if (chip->shutdown_voltage_tripped && !chip->factory_mode)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->dc_present;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		val->intval = chip->dc_therm_lvl_sel;
		break;
	case POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS:
		val->intval = chip->dc_thermal_levels;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void smb135x_external_power_changed(struct power_supply *psy)
{
	struct smb135x_chg *chip = container_of(psy,
				struct smb135x_chg, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0;

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	pr_debug("current_limit = %d\n", current_limit);

	if (chip->usb_psy_ma != current_limit) {
		mutex_lock(&chip->current_change_lock);
		chip->usb_psy_ma = current_limit;
		rc = smb135x_set_appropriate_current(chip, USB);
		mutex_unlock(&chip->current_change_lock);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set usb current rc = %d\n",
					rc);
	}
}

#define MIN_FLOAT_MV	3600
#define MAX_FLOAT_MV	4400

#define MID_RANGE_FLOAT_MV_MIN		3600
#define MID_RANGE_FLOAT_MIN_VAL		0x05
#define MID_RANGE_FLOAT_STEP_MV		20

#define HIGH_RANGE_FLOAT_MIN_MV		4340
#define HIGH_RANGE_FLOAT_MIN_VAL	0x2A
#define HIGH_RANGE_FLOAT_STEP_MV	10

#define VHIGH_RANGE_FLOAT_MIN_MV	4400
#define VHIGH_RANGE_FLOAT_MIN_VAL	0x2E
#define VHIGH_RANGE_FLOAT_STEP_MV	20
static int smb135x_float_voltage_set(struct smb135x_chg *chip, int vfloat_mv)
{
	u8 temp;
	int rc;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		vfloat_mv = 4350;
	}

	if (vfloat_mv <= HIGH_RANGE_FLOAT_MIN_MV) {
		/* mid range */
		temp = MID_RANGE_FLOAT_MIN_VAL
			+ (vfloat_mv - MID_RANGE_FLOAT_MV_MIN)
				/ MID_RANGE_FLOAT_STEP_MV;
	} else if (vfloat_mv <= VHIGH_RANGE_FLOAT_MIN_MV) {
		/* high range */
		temp = HIGH_RANGE_FLOAT_MIN_VAL
			+ (vfloat_mv - HIGH_RANGE_FLOAT_MIN_MV)
				/ HIGH_RANGE_FLOAT_STEP_MV;
	} else {
		/* very high range */
		temp = VHIGH_RANGE_FLOAT_MIN_VAL
			+ (vfloat_mv - VHIGH_RANGE_FLOAT_MIN_MV)
				/ VHIGH_RANGE_FLOAT_STEP_MV;
	}

	rc = smb135x_write(chip, VFLOAT_REG, temp);
	if (rc < 0)
		rc = smb135x_write(chip, VFLOAT_REG, temp);
	temp = 0;
	rc = smb135x_read(chip, VFLOAT_REG, &temp);
	if ((rc >= 0) && ((temp & VFLOAT_MASK) > VHIGH_RANGE_FLOAT_MIN_VAL)) {
		dev_err(chip->dev, "bad float voltage set mv =%d\n",
			temp);
		chip->invalid_battery = true;
		smb135x_temp_charging(chip, 0);
	}

	return rc;
}

static bool elapsed_msec_greater(struct timeval *start_time,
				struct timeval *end_time, int ms)
{
	int msec_elapsed;

	msec_elapsed = (end_time->tv_sec - start_time->tv_sec) * 1000 +
		DIV_ROUND_UP(end_time->tv_usec - start_time->tv_usec, 1000);

	return (msec_elapsed > ms);
}

#define MAX_STEP_MS		10
static int smb135x_chg_otg_enable(struct smb135x_chg *chip)
{
	int rc = 0;
	int restart_count = 0;
	struct timeval time_a, time_b, time_c, time_d;

	/*
	 * Workaround for a hardware bug where the OTG needs to be enabled
	 * disabled and enabled for it to be actually enabled. The time between
	 * each step should be atmost MAX_STEP_MS
	 *
	 * Note that if enable-disable executes within the timeframe
	 * but the final enable takes more than MAX_STEP_ME, we treat it as
	 * the first enable and try disabling again. We don't want
	 * to issue enable back to back.
	 *
	 * Notice the instances when time is captured and the successive
	 * steps.
	 * timeA-enable-timeC-disable-timeB-enable-timeD.
	 * When
	 * (timeB - timeA) < MAX_STEP_MS AND (timeC - timeD) < MAX_STEP_MS
	 * then it is guaranteed that the successive steps
	 * must have executed within MAX_STEP_MS
	 */
	do_gettimeofday(&time_a);
restart_from_enable:
	/* first step - enable otg */
	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, OTG_EN);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d\n", rc);
		return rc;
	}

restart_from_disable:
	/* second step - disable otg */
	do_gettimeofday(&time_c);
	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d\n", rc);
		return rc;
	}
	do_gettimeofday(&time_b);

	if (elapsed_msec_greater(&time_a, &time_b, MAX_STEP_MS)) {
		restart_count++;
		if (restart_count > 10) {
			dev_err(chip->dev,
				"Couldn't enable OTG restart_count=%d\n",
				restart_count);
			return -EAGAIN;
		}
		time_a = time_b;
		pr_debug("restarting from first enable\n");
		goto restart_from_enable;
	}

	/* third step (first step in case of a failure) - enable otg */
	time_a = time_b;
	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, OTG_EN);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d\n", rc);
		return rc;
	}
	do_gettimeofday(&time_d);

	if (elapsed_msec_greater(&time_c, &time_d, MAX_STEP_MS)) {
		restart_count++;
		if (restart_count > 10) {
			dev_err(chip->dev,
				"Couldn't enable OTG restart_count=%d\n",
				restart_count);
			return -EAGAIN;
		}
		pr_debug("restarting from disable\n");
		goto restart_from_disable;
	}
	return rc;
}

static int smb135x_chg_otg_regulator_enable(struct regulator_dev *rdev)
{
	struct smb135x_chg *chip = rdev_get_drvdata(rdev);
	return smb135x_chg_otg_enable(chip);
}

static void ocp_clear_work(struct work_struct *work)
{
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
				ocp_clear_work.work);
	smb135x_chg_otg_enable(chip);
}

static int smb135x_chg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smb135x_chg *chip = rdev_get_drvdata(rdev);

	cancel_delayed_work_sync(&chip->ocp_clear_work);
	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);
	return rc;
}

static int smb135x_chg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smb135x_chg *chip = rdev_get_drvdata(rdev);

	rc = smb135x_read(chip, CMD_CHG_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return  (reg & OTG_EN) ? 1 : 0;
}

struct regulator_ops smb135x_chg_otg_reg_ops = {
	.enable		= smb135x_chg_otg_regulator_enable,
	.disable	= smb135x_chg_otg_regulator_disable,
	.is_enabled	= smb135x_chg_otg_regulator_is_enable,
};

#define SMB1356_VERSION3_BIT	BIT(7)
#define SMB1357_VERSION1_VAL	0x01
#define SMB1358_VERSION1_VAL	0x02
#define SMB1359_VERSION1_VAL	0x00
#define SMB1357_VERSION2_VAL	0x01
#define SMB1358_VERSION2_VAL	0x02
#define SMB1359_VERSION2_VAL	0x00
static int smb135x_chip_version_and_revision(struct smb135x_chg *chip)
{
	int rc;
	u8 version1, version2, version3;

	/* read the revision */
	rc = read_revision(chip, &chip->revision);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read revision rc = %d\n", rc);
		return rc;
	}

	if (chip->revision >= REV_MAX || revision_str[chip->revision] == NULL) {
		dev_err(chip->dev, "Bad revision found = %d\n", chip->revision);
		return -EINVAL;
	}

	/* check if it is smb1356 */
	rc = read_version3(chip, &version3);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read version3 rc = %d\n", rc);
		return rc;
	}

	if (version3 & SMB1356_VERSION3_BIT) {
		chip->version = V_SMB1356;
		goto wrkarnd_and_input_current_values;
	}

	/* check if it is smb1357, smb1358 or smb1359 based on revision */
	if (chip->revision <= REV_1_1) {
		rc = read_version1(chip, &version1);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't read version 1 rc = %d\n", rc);
			return rc;
		}
		switch (version1) {
		case SMB1357_VERSION1_VAL:
			chip->version = V_SMB1357;
			break;
		case SMB1358_VERSION1_VAL:
			chip->version = V_SMB1358;
			break;
		case SMB1359_VERSION1_VAL:
			chip->version = V_SMB1359;
			break;
		default:
			dev_err(chip->dev,
				"Unknown version 1 = 0x%02x rc = %d\n",
				version1, rc);
			return rc;
		}
	} else {
		rc = read_version2(chip, &version2);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't read version 2 rc = %d\n", rc);
			return rc;
		}
		switch (version2) {
		case SMB1357_VERSION2_VAL:
			chip->version = V_SMB1357;
			break;
		case SMB1358_VERSION2_VAL:
			chip->version = V_SMB1358;
			break;
		case SMB1359_VERSION2_VAL:
			chip->version = V_SMB1359;
			break;
		default:
			dev_err(chip->dev,
					"Unknown version 2 = 0x%02x rc = %d\n",
					version2, rc);
			return rc;
		}
	}

wrkarnd_and_input_current_values:
	if (is_usb100_broken(chip))
		chip->workaround_flags |= WRKARND_USB100_BIT;
	/*
	 * Rev v1.0 and v1.1 of SMB135x fails charger type detection
	 * (apsd) due to interference on the D+/- lines by the USB phy.
	 * Set the workaround flag to disable charger type reporting
	 * for this revision.
	 */
	if (chip->revision <= REV_1_1)
		chip->workaround_flags |= WRKARND_APSD_FAIL;

	pr_debug("workaround_flags = %x\n", chip->workaround_flags);

	switch (chip->version) {
	case V_SMB1356:
		chip->usb_current_table = usb_current_table_smb1356;
		chip->usb_current_arr_size
			= ARRAY_SIZE(usb_current_table_smb1356);
		chip->dc_current_table = dc_current_table_smb1356;
		chip->dc_current_arr_size
			= ARRAY_SIZE(dc_current_table_smb1356);
		break;
	case V_SMB1357:
		chip->usb_current_table = usb_current_table_smb1357_smb1358;
		chip->usb_current_arr_size
			= ARRAY_SIZE(usb_current_table_smb1357_smb1358);
		chip->dc_current_table = dc_current_table;
		chip->dc_current_arr_size = ARRAY_SIZE(dc_current_table);
		break;
	case V_SMB1358:
		chip->usb_current_table = usb_current_table_smb1357_smb1358;
		chip->usb_current_arr_size
			= ARRAY_SIZE(usb_current_table_smb1357_smb1358);
		chip->dc_current_table = dc_current_table;
		chip->dc_current_arr_size = ARRAY_SIZE(dc_current_table);
		break;
	case V_SMB1359:
		chip->usb_current_table = usb_current_table_smb1359;
		chip->usb_current_arr_size
			= ARRAY_SIZE(usb_current_table_smb1359);
		chip->dc_current_table = dc_current_table;
		chip->dc_current_arr_size = ARRAY_SIZE(dc_current_table);
		break;
	}

	return 0;
}

static int smb135x_regulator_init(struct smb135x_chg *chip)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};

	init_data = of_get_regulator_init_data(chip->dev, chip->dev->of_node);
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &smb135x_chg_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = chip->dev->of_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->otg_vreg.rdev = regulator_register(
						&chip->otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->otg_vreg.rdev)) {
			rc = PTR_ERR(chip->otg_vreg.rdev);
			chip->otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"OTG reg failed, rc=%d\n", rc);
		}
	}

	return rc;
}

static void smb135x_regulator_deinit(struct smb135x_chg *chip)
{
	if (chip->otg_vreg.rdev)
		regulator_unregister(chip->otg_vreg.rdev);
}

static void wireless_insertion_work(struct work_struct *work)
{
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
				wireless_insertion_work.work);

	/* unsuspend dc */
	smb135x_path_suspend(chip, DC, CURRENT, false);
}

static int drop_usbin_rate(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	rc = smb135x_read(chip, CFG_C_REG, &reg);
	if (rc < 0)
		dev_err(chip->dev, "Failed to Read CFG C\n");

	reg = reg & USBIN_INPUT_MASK;
	if (reg > 0)
		reg--;
	dev_warn(chip->dev, "Input current Set 0x%x\n", reg);
	rc = smb135x_masked_write(chip,
				  CFG_C_REG, USBIN_INPUT_MASK,
				  reg);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't Lower Input Rate\n");

	return reg;
}

static void aicl_check_work(struct work_struct *work)
{
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
				aicl_check_work.work);
	int rc;

	dev_dbg(chip->dev, "Drop Rate!\n");

	rc = drop_usbin_rate(chip);
	if (!chip->aicl_weak_detect && (rc < 0x02))
		chip->aicl_weak_detect = true;

	cancel_delayed_work(&chip->src_removal_work);
	schedule_delayed_work(&chip->src_removal_work,
		msecs_to_jiffies(3000));
	if (!rc) {
		dev_dbg(chip->dev, "Reached Bottom IC!\n");
		return;
	}

	rc = smb135x_masked_write(chip,
				  IRQ_CFG_REG,
				  IRQ_USBIN_UV_BIT,
				  IRQ_USBIN_UV_BIT);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't Unmask USBIN UV IRQ\n");
}

static void src_removal_work(struct work_struct *work)
{
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
				src_removal_work.work);
	bool usb_present = is_usb_plugged_in(chip);
	if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		if (!usb_present && !is_dc_plugged_in(chip)) {
			chip->chg_done_batt_full = false;
			chip->float_charge_start_time = 0;
		}
		notify_usb_removal(chip);
	}
}

static int smb135x_get_charge_rate(struct smb135x_chg *chip)
{
	u8 reg;
	int rc;

	if (!is_usb_plugged_in(chip))
		return POWER_SUPPLY_CHARGE_RATE_NONE;

	if (chip->aicl_weak_detect)
		return POWER_SUPPLY_CHARGE_RATE_WEAK;

	rc = smb135x_read(chip, STATUS_6_REG, &reg);
	if (rc < 0)
		return POWER_SUPPLY_CHARGE_RATE_NORMAL;

	if (reg & HVDCP_BIT)
		return POWER_SUPPLY_CHARGE_RATE_TURBO;

	return POWER_SUPPLY_CHARGE_RATE_NORMAL;
}

#define HVDCP_INPUT_CURRENT_MAX 1600
static void rate_check_work(struct work_struct *work)
{
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
			     rate_check_work.work);

	chip->charger_rate = smb135x_get_charge_rate(chip);

	if (chip->charger_rate > POWER_SUPPLY_CHARGE_RATE_NORMAL) {
		pr_warn("Charger Rate = %d\n", chip->charger_rate);

		if (chip->charger_rate ==
		    POWER_SUPPLY_CHARGE_RATE_TURBO) {
			mutex_lock(&chip->current_change_lock);
			chip->usb_psy_ma = HVDCP_INPUT_CURRENT_MAX;
			smb135x_set_appropriate_current(chip, USB);
			mutex_unlock(&chip->current_change_lock);
		}

		chip->rate_check_count = 0;
		power_supply_changed(&chip->batt_psy);
		return;
	}

	chip->rate_check_count++;
	if (chip->rate_check_count < 6)
		schedule_delayed_work(&chip->rate_check_work,
				      msecs_to_jiffies(500));
}

static void usb_insertion_work(struct work_struct *work)
{
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
				usb_insertion_work.work);
	int rc;

	rc = smb135x_force_apsd(chip);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't rerun apsd rc = %d\n", rc);

	smb_relax(&chip->smb_wake_source);
}

static void toggle_usbin_aicl(struct smb135x_chg *chip)
{
	int rc;

	/* Set AICL OFF */
	rc = smb135x_masked_write(chip, CFG_D_REG, AICL_ENABLE, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable AICL\n");

	/* Set AICL ON */
	rc = smb135x_masked_write(chip, CFG_D_REG, AICL_ENABLE, AICL_ENABLE);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't enable AICL\n");
}

#define FLOAT_CHG_TIME_SECS 1800
#define INPUT_CURR_CHECK_THRES 0x0C /*  1100 mA */
static void heartbeat_work(struct work_struct *work)
{
	u8 reg;
	int rc;
	struct timespec bootup_time;
	unsigned long float_timestamp;
	bool usb_present;
	bool dc_present;
	int batt_health;
	int batt_soc;
	struct smb135x_chg *chip =
		container_of(work, struct smb135x_chg,
				heartbeat_work.work);
	bool poll_status = chip->poll_fast;

	if (smb135x_get_prop_batt_capacity(chip, &batt_soc) ||
	    smb135x_get_prop_batt_health(chip, &batt_health)) {
		schedule_delayed_work(&chip->heartbeat_work,
				      msecs_to_jiffies(1000));
		return;
	}

	dev_dbg(chip->dev, "HB Pound!\n");
	cancel_delayed_work(&chip->src_removal_work);

	get_monotonic_boottime(&bootup_time);
	float_timestamp = bootup_time.tv_sec;

	if (batt_soc < 20)
		chip->poll_fast = true;
	else
		chip->poll_fast = false;

	if (poll_status != chip->poll_fast)
		smb135x_setup_vbat_monitoring(chip);

	usb_present = is_usb_plugged_in(chip);
	dc_present = is_dc_plugged_in(chip);

	if (chip->usb_present && !usb_present) {
		dev_warn(chip->dev, "HB Caught Removal!\n");
		/* USB removed */
		chip->usb_present = usb_present;
		if (!is_dc_plugged_in(chip)) {
			chip->chg_done_batt_full = false;
			chip->float_charge_start_time = 0;
		}
		notify_usb_removal(chip);
	} else if (usb_present) {
		if (!chip->aicl_disabled) {
			rc = smb135x_read(chip, STATUS_0_REG, &reg);
			if (rc < 0) {
				pr_info("Failed to Read Status 0x46\n");
			} else if (!reg) {
				dev_warn(chip->dev, "HB Caught Low Rate!\n");
				toggle_usbin_aicl(chip);
			}
		} else {
			rc = smb135x_read(chip, CFG_C_REG, &reg);
			if (rc < 0)
				dev_err(chip->dev, "Failed to Read CFG C\n");

			reg = reg & USBIN_INPUT_MASK;
			if (reg < INPUT_CURR_CHECK_THRES) {
				dev_warn(chip->dev, "Increase Input Rate\n");
				rc = smb135x_masked_write(chip,
							  IRQ_CFG_REG,
							  IRQ_USBIN_UV_BIT,
							  IRQ_USBIN_UV_BIT);
				if (rc < 0)
					dev_err(chip->dev,
						"Failed to Write CFG Reg\n");
				smb135x_set_appropriate_current(chip, USB);
			}
		}

		if (!chip->chg_done_batt_full &&
		    !chip->float_charge_start_time &&
		    chip->iterm_disabled &&
		    (batt_soc >= 100)) {
			chip->float_charge_start_time = float_timestamp;
			dev_warn(chip->dev, "Float Start!\n");
		} else if (chip->float_charge_start_time &&
			   ((float_timestamp - chip->float_charge_start_time)
			    >= FLOAT_CHG_TIME_SECS)) {
			chip->float_charge_start_time = 0;
			chip->chg_done_batt_full = true;
			dev_warn(chip->dev, "Float Done!\n");
		} else if (chip->chg_done_batt_full && (batt_soc < 100)) {
			chip->chg_done_batt_full = false;
			dev_warn(chip->dev, "SOC dropped,  Charge Resume!\n");
		}
	}

	if ((batt_health == POWER_SUPPLY_HEALTH_WARM) ||
	    (batt_health == POWER_SUPPLY_HEALTH_COOL) ||
	    (batt_health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
	    (batt_health == POWER_SUPPLY_HEALTH_COLD))
		chip->temp_check = smb135x_check_temp_range(chip);

	smb135x_set_chrg_path_temp(chip);

	if (chip->temp_check ||
	    chip->bms_check ||
	    chip->chg_done_batt_full ||
	    chip->float_charge_start_time) {
		chip->temp_check = 0;
		chip->bms_check = 0;
	}

	power_supply_changed(&chip->batt_psy);

	schedule_delayed_work(&chip->heartbeat_work,
			      msecs_to_jiffies(60000));

	if (!usb_present && !dc_present)
		smb_relax(&chip->smb_wake_source);
}

static int hot_hard_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_hot = !!rt_stat;
	return 0;
}
static int cold_hard_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_cold = !!rt_stat;
	return 0;
}
static int hot_soft_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_warm = !!rt_stat;
	return 0;
}
static int cold_soft_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_cool = !!rt_stat;
	return 0;
}
static int battery_missing_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	chip->batt_present = !rt_stat;
	return 0;
}
static int vbat_low_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_warn("vbat low\n");
	return 0;
}
static int chg_hot_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_warn("chg hot\n");
	return 0;
}
static int chg_term_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	if (!chip->iterm_disabled)
		chip->chg_done_batt_full = !!rt_stat;
	return 0;
}

static int taper_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int fast_chg_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	if (rt_stat & IRQ_C_FAST_CHG_BIT) {
		chip->chg_done_batt_full = false;
		chip->float_charge_start_time = 0;
	}
	power_supply_changed(&chip->batt_psy);
	return 0;
}

static int recharge_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int safety_timeout_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_warn("safety timeout rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int aicl_done_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	if (rt_stat & IRQ_D_AICL_DONE_BIT)
		pr_warn("aicl done rt_stat = 0x%02x Rate = %d\n", rt_stat,
			chip->charger_rate);

	return 0;
}

/**
 * power_ok_handler() - called when the switcher turns on or turns off
 * @chip: pointer to smb135x_chg chip
 * @rt_stat: the status bit indicating switcher turning on or off
 */
static int power_ok_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

#define MAX_OTG_RETRY	3
static int otg_oc_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	if (!(rt_stat & IRQ_F_OTG_OC_BIT)) {
		pr_err("Spurious OTG OC irq\n");
		return 0;
	}

	schedule_delayed_work(&chip->ocp_clear_work,
		msecs_to_jiffies(0));

	pr_err("rt_stat = 0x%02x\n", rt_stat);
	return 0;
}

static int handle_dc_removal(struct smb135x_chg *chip)
{
	if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIRELESS) {
		cancel_delayed_work_sync(&chip->wireless_insertion_work);
		smb135x_path_suspend(chip, DC, CURRENT, true);
	}

	if (chip->dc_psy_type != -EINVAL)
		power_supply_set_online(&chip->dc_psy, chip->dc_present);

	smb_relax(&chip->smb_wake_source);
	return 0;
}

#define DCIN_UNSUSPEND_DELAY_MS		1000
static int handle_dc_insertion(struct smb135x_chg *chip)
{
	if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIRELESS)
		schedule_delayed_work(&chip->wireless_insertion_work,
			msecs_to_jiffies(DCIN_UNSUSPEND_DELAY_MS));
	if (chip->dc_psy_type != -EINVAL)
		power_supply_set_online(&chip->dc_psy,
						chip->dc_present);

	smb_stay_awake(&chip->smb_wake_source);
	return 0;
}
/**
 * dcin_uv_handler() - called when the dc voltage crosses the uv threshold
 * @chip: pointer to smb135x_chg chip
 * @rt_stat: the status bit indicating whether dc voltage is uv
 */
static int dcin_uv_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	/*
	 * rt_stat indicates if dc is undervolted. If so dc_present
	 * should be marked removed
	 */
	bool dc_present = !rt_stat;

	pr_debug("chip->dc_present = %d dc_present = %d\n",
			chip->dc_present, dc_present);

	if (chip->dc_present && !dc_present) {
		/* dc removed */
		chip->dc_present = dc_present;
		if (!dc_present && !is_usb_plugged_in(chip)) {
			chip->chg_done_batt_full = false;
			chip->float_charge_start_time = 0;
		}
		handle_dc_removal(chip);
	}

	if (!chip->dc_present && dc_present) {
		/* dc inserted */
		chip->dc_present = dc_present;
		handle_dc_insertion(chip);
	}

	return 0;
}

static int dcin_ov_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	/*
	 * rt_stat indicates if dc is overvolted. If so dc_present
	 * should be marked removed
	 */
	bool dc_present = !rt_stat;

	pr_debug("chip->dc_present = %d dc_present = %d\n",
			chip->dc_present, dc_present);

	chip->dc_ov = !!rt_stat;

	if (chip->dc_present && !dc_present) {
		/* dc removed */
		chip->dc_present = dc_present;
		handle_dc_removal(chip);
	}

	if (!chip->dc_present && dc_present) {
		/* dc inserted */
		chip->dc_present = dc_present;
		handle_dc_insertion(chip);
	}
	return 0;
}

static int handle_usb_removal(struct smb135x_chg *chip)
{
	int rc;
	cancel_delayed_work(&chip->usb_insertion_work);
	cancel_delayed_work(&chip->aicl_check_work);
	cancel_delayed_work(&chip->rate_check_work);
	chip->apsd_rerun_cnt = 0;
	chip->aicl_weak_detect = false;
	chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;

	if (chip->hvdcp_powerup)
		chip->hvdcp_powerup = false;

	rc = smb135x_masked_write(chip,
				  IRQ_CFG_REG,
				  IRQ_USBIN_UV_BIT,
				  IRQ_USBIN_UV_BIT);
	if (rc < 0)
		dev_err(chip->dev,
			"Failed to Write CFG Reg\n");

	if (!chip->aicl_disabled) {
		/* Set AICL Glich to 20ms */
		rc = smb135x_masked_write(chip, CFG_D_REG, AICL_GLITCH, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set 20 ms AICL glitch\n");
			return rc;
		}
	}

	if (chip->usb_psy) {
		pr_debug("setting usb psy type = %d\n",
				POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_supply_type(chip->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);
		pr_debug("setting usb psy present = %d\n", chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
	}

	smb_relax(&chip->smb_wake_source);
	return 0;
}

static int notify_usb_removal(struct smb135x_chg *chip)
{
	int rc;

	rc = handle_usb_removal(chip);

	if (rc >= 0) {
		if (chip->usb_psy)
			power_supply_changed(chip->usb_psy);
		power_supply_changed(&chip->batt_psy);
	}

	return rc;
}

static int handle_usb_insertion(struct smb135x_chg *chip)
{
	u8 reg;
	int rc;
	char *usb_type_name = "null";
	enum power_supply_type usb_supply_type;

	/* usb inserted */
	rc = smb135x_read(chip, STATUS_5_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return rc;
	}

	cancel_delayed_work(&chip->usb_insertion_work);

	/* Rerun APSD 1 sec later */
	if ((reg & SDP_BIT) && !chip->apsd_rerun_cnt) {
		dev_info(chip->dev, "HW Detected SDP!\n");
		smb_stay_awake(&chip->smb_wake_source);
		chip->apsd_rerun_cnt++;
		chip->usb_present = 0;
		schedule_delayed_work(&chip->usb_insertion_work,
				      msecs_to_jiffies(1000));
		return 0;
	}

	chip->apsd_rerun_cnt = 0;

	if (!chip->aicl_disabled) {
		/* Set AICL Glich to 15 us */
		rc = smb135x_masked_write(chip,
					  CFG_D_REG,
					  AICL_GLITCH,
					  AICL_GLITCH);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set 15 us AICL glitch\n");
	}

	/*
	 * Report the charger type as UNKNOWN if the
	 * apsd-fail flag is set. This nofifies the USB driver
	 * to initiate a s/w based charger type detection.
	 */
	if (chip->workaround_flags & WRKARND_APSD_FAIL)
		reg = 0;

	chip->aicl_weak_detect = false;
	usb_type_name = get_usb_type_name(reg);
	usb_supply_type = get_usb_supply_type(reg);
	pr_debug("inserted %s, usb psy type = %d stat_5 = 0x%02x\n",
			usb_type_name, usb_supply_type, reg);
	if (chip->usb_psy) {
		pr_debug("setting usb psy type = %d\n", usb_supply_type);
		power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
		pr_debug("setting usb psy present = %d\n", chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
	}
	smb_stay_awake(&chip->smb_wake_source);

	chip->charger_rate =  POWER_SUPPLY_CHARGE_RATE_NORMAL;
	chip->rate_check_count = 0;
	cancel_delayed_work(&chip->rate_check_work);
	schedule_delayed_work(&chip->rate_check_work,
			      msecs_to_jiffies(500));
	return 0;
}

/**
 * usbin_uv_handler() - this is called when USB charger is removed
 * @chip: pointer to smb135x_chg chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static int usbin_uv_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	u8 reg;
	int rc;
	/*
	 * rt_stat indicates if usb is undervolted. If so usb_present
	 * should be marked removed
	 */
	bool usb_present = !rt_stat;

	pr_debug("chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	if (ignore_disconnect) {
		pr_info("Ignore usbin_uv - usb_present = %d\n", usb_present);
		return 0;
	}

	if (is_usb_plugged_in(chip)) {
		if (!chip->aicl_disabled) {
			rc = smb135x_read(chip, STATUS_0_REG, &reg);
			if (rc < 0)
				pr_err("Failed to Read Status 0x46\n");
			else if (!reg)
				toggle_usbin_aicl(chip);
		} else if (!usb_present) {
			rc = smb135x_masked_write(chip,
						  IRQ_CFG_REG,
						  IRQ_USBIN_UV_BIT, 0);
			if (rc < 0)
				pr_err("Failed to Disable USBIN UV IRQ\n");
			cancel_delayed_work(&chip->aicl_check_work);
			schedule_delayed_work(&chip->aicl_check_work,
					      msecs_to_jiffies(0));
		}
		return 0;
	}

	if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		if (!usb_present && !is_dc_plugged_in(chip)) {
			chip->chg_done_batt_full = false;
			chip->float_charge_start_time = 0;
		}
		notify_usb_removal(chip);
	}
	return 0;
}

static int usbin_ov_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	/*
	 * rt_stat indicates if usb is overvolted. If so usb_present
	 * should be marked removed
	 */
	bool usb_present = !rt_stat;
	int health;

	pr_info("chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	if (ignore_disconnect) {
		pr_info("Ignore usbin_ov - usb_present = %d\n", usb_present);
		return 0;
	}

	if (is_usb_plugged_in(chip) && !chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		handle_usb_insertion(chip);
	} else if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		if (!is_dc_plugged_in(chip)) {
			chip->chg_done_batt_full = false;
			chip->float_charge_start_time = 0;
		}
		notify_usb_removal(chip);
	}

	if (chip->usb_psy) {
		health = rt_stat ? POWER_SUPPLY_HEALTH_OVERVOLTAGE
					: POWER_SUPPLY_HEALTH_GOOD;
		power_supply_set_health_state(chip->usb_psy, health);
	}

	return 0;
}

static void smb135x_notify_vbat(enum qpnp_tm_state state, void *ctx)
{
	struct smb135x_chg *chip = ctx;
	struct qpnp_vadc_result result;
	int batt_volt;
	int rc;

	pr_err("shutdown voltage tripped\n");

	rc = qpnp_vadc_read(chip->vadc_dev, VSYS, &result);
	pr_info("vbat = %lld, raw = 0x%x\n",
		result.physical, result.adc_code);

	smb135x_get_prop_batt_voltage_now(chip, &batt_volt);
	pr_info("vbat is at %d, state is at %d\n",
		 batt_volt, state);

	if (state == ADC_TM_LOW_STATE)
		chip->shutdown_voltage_tripped = true;
	else
		qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
					    &chip->vbat_monitor_params);

	power_supply_changed(&chip->batt_psy);
}

static int smb135x_setup_vbat_monitoring(struct smb135x_chg *chip)
{
	int rc;

	chip->vbat_monitor_params.low_thr = chip->low_voltage_uv;
	chip->vbat_monitor_params.high_thr = chip->max_voltage_uv * 2;

	chip->vbat_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	chip->vbat_monitor_params.channel = VSYS;
	chip->vbat_monitor_params.btm_ctx = (void *)chip;

	if (chip->poll_fast) { /* the adc polling rate is higher*/
		chip->vbat_monitor_params.timer_interval =
			ADC_MEAS1_INTERVAL_31P3MS;
	} else /* adc polling rate is default*/ {
		chip->vbat_monitor_params.timer_interval =
			ADC_MEAS1_INTERVAL_1S;
	}

	chip->vbat_monitor_params.threshold_notification = &smb135x_notify_vbat;
	dev_info(chip->dev, "set low thr to %d and high to %d\n",
		 chip->vbat_monitor_params.low_thr,
		 chip->vbat_monitor_params.high_thr);

	if (!smb135x_get_prop_batt_present(chip)) {
		pr_info("no battery inserted, vbat monitoring disabled\n");
		chip->vbat_monitor_params.state_request =
			ADC_TM_HIGH_LOW_THR_DISABLE;
	} else {
		rc = qpnp_adc_tm_channel_measure(chip->adc_tm_dev,
						 &chip->vbat_monitor_params);
		if (rc) {
			dev_err(chip->dev, "tm setup failed: %d\n", rc);
			return rc;
		}
	}

	pr_info("vbat monitoring setup complete\n");
	return 0;
}

/**
 * src_detect_handler() - this is called when USB charger type is detected, use
 *			it for handling USB charger insertion/removal
 * @chip: pointer to smb135x_chg chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static int src_detect_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	bool usb_present = !!rt_stat;

	pr_info("chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		handle_usb_insertion(chip);
	} else if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		if (!is_dc_plugged_in(chip))
			chip->chg_done_batt_full = false;
		notify_usb_removal(chip);
	}

	return 0;
}

static int chg_inhibit_handler(struct smb135x_chg *chip, u8 rt_stat)
{
	/*
	 * charger is inserted when the battery voltage is high
	 * so h/w won't start charging just yet. Treat this as
	 * battery full
	 */
	pr_debug("rt_stat = 0x%02x\n", rt_stat);
	if (!chip->iterm_disabled)
		chip->chg_done_batt_full = !!rt_stat;
	return 0;
}

struct smb_irq_info {
	const char		*name;
	int			(*smb_irq)(struct smb135x_chg *chip,
							u8 rt_stat);
	int			high;
	int			low;
};

struct irq_handler_info {
	u8			stat_reg;
	u8			val;
	u8			prev_val;
	struct smb_irq_info	irq_info[4];
};

static struct irq_handler_info handlers[] = {
	{IRQ_A_REG, 0, 0,
		{
			{
				.name		= "cold_soft",
				.smb_irq	= cold_soft_handler,
			},
			{
				.name		= "hot_soft",
				.smb_irq	= hot_soft_handler,
			},
			{
				.name		= "cold_hard",
				.smb_irq	= cold_hard_handler,
			},
			{
				.name		= "hot_hard",
				.smb_irq	= hot_hard_handler,
			},
		},
	},
	{IRQ_B_REG, 0, 0,
		{
			{
				.name		= "chg_hot",
				.smb_irq	= chg_hot_handler,
			},
			{
				.name		= "vbat_low",
				.smb_irq	= vbat_low_handler,
			},
			{
				.name		= "battery_missing",
				.smb_irq	= battery_missing_handler,
			},
			{
				.name		= "battery_missing",
				.smb_irq	= battery_missing_handler,
			},
		},
	},
	{IRQ_C_REG, 0, 0,
		{
			{
				.name		= "chg_term",
				.smb_irq	= chg_term_handler,
			},
			{
				.name		= "taper",
				.smb_irq	= taper_handler,
			},
			{
				.name		= "recharge",
				.smb_irq	= recharge_handler,
			},
			{
				.name		= "fast_chg",
				.smb_irq	= fast_chg_handler,
			},
		},
	},
	{IRQ_D_REG, 0, 0,
		{
			{
				.name		= "prechg_timeout",
			},
			{
				.name		= "safety_timeout",
				.smb_irq	= safety_timeout_handler,
			},
			{
				.name		= "aicl_done",
				.smb_irq	= aicl_done_handler,
			},
			{
				.name		= "battery_ov",
			},
		},
	},
	{IRQ_E_REG, 0, 0,
		{
			{
				.name		= "usbin_uv",
				.smb_irq	= usbin_uv_handler,
			},
			{
				.name		= "usbin_ov",
				.smb_irq	= usbin_ov_handler,
			},
			{
				.name		= "dcin_uv",
				.smb_irq	= dcin_uv_handler,
			},
			{
				.name		= "dcin_ov",
				.smb_irq	= dcin_ov_handler,
			},
		},
	},
	{IRQ_F_REG, 0, 0,
		{
			{
				.name		= "power_ok",
				.smb_irq	= power_ok_handler,
			},
			{
				.name		= "unused",
			},
			{
				.name		= "otg_fail",
			},
			{
				.name		= "otg_oc",
				.smb_irq	= otg_oc_handler,
			},
		},
	},
	{IRQ_G_REG, 0, 0,
		{
			{
				.name		= "chg_inhibit",
				.smb_irq	= chg_inhibit_handler,
			},
			{
				.name		= "chg_error",
			},
			{
				.name		= "wd_timeout",
			},
			{
				.name		= "src_detect",
				.smb_irq	= src_detect_handler,
			},
		},
	},
};

static int smb135x_irq_read(struct smb135x_chg *chip)
{
	int rc, i;

	/*
	 * When dcin path is suspended the irq triggered status is not cleared
	 * causing a storm. To prevent this situation unsuspend dcin path while
	 * reading interrupts and restore its status back.
	 */
	mutex_lock(&chip->path_suspend_lock);

	if (chip->dc_suspended)
		__smb135x_dc_suspend(chip, false);

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		rc = smb135x_read(chip, handlers[i].stat_reg,
						&handlers[i].val);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't read %d rc = %d\n",
					handlers[i].stat_reg, rc);
			handlers[i].val = 0;
			continue;
		}
	}

	if (chip->dc_suspended)
		__smb135x_dc_suspend(chip, true);

	mutex_unlock(&chip->path_suspend_lock);

	return rc;
}
#define IRQ_LATCHED_MASK	0x02
#define IRQ_STATUS_MASK		0x01
#define BITS_PER_IRQ		2
static irqreturn_t smb135x_chg_stat_handler(int irq, void *dev_id)
{
	struct smb135x_chg *chip = dev_id;
	int i, j;
	u8 triggered;
	u8 changed;
	u8 rt_stat, prev_rt_stat;
	int rc;
	int handler_count = 0;

	mutex_lock(&chip->irq_complete);
	chip->irq_waiting = true;
	if (!chip->resume_completed) {
		dev_dbg(chip->dev, "IRQ triggered before device-resume\n");
		disable_irq_nosync(irq);
		mutex_unlock(&chip->irq_complete);
		return IRQ_HANDLED;
	}
	chip->irq_waiting = false;

	smb135x_irq_read(chip);
	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		for (j = 0; j < ARRAY_SIZE(handlers[i].irq_info); j++) {
			triggered = handlers[i].val
			       & (IRQ_LATCHED_MASK << (j * BITS_PER_IRQ));
			rt_stat = handlers[i].val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			prev_rt_stat = handlers[i].prev_val
				& (IRQ_STATUS_MASK << (j * BITS_PER_IRQ));
			changed = prev_rt_stat ^ rt_stat;

			if (triggered || changed)
				rt_stat ? handlers[i].irq_info[j].high++ :
						handlers[i].irq_info[j].low++;

			if ((triggered || changed)
				&& handlers[i].irq_info[j].smb_irq != NULL) {
				handler_count++;
				rc = handlers[i].irq_info[j].smb_irq(chip,
								rt_stat);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't handle %d irq for reg 0x%02x rc = %d\n",
						j, handlers[i].stat_reg, rc);
			}
		}
		handlers[i].prev_val = handlers[i].val;
	}

	pr_debug("handler count = %d\n", handler_count);

	if (handler_count && !chip->aicl_disabled) {
		pr_debug("batt psy changed\n");
		power_supply_changed(&chip->batt_psy);
		if (chip->usb_psy) {
			pr_debug("usb psy changed\n");
			power_supply_changed(chip->usb_psy);
		}
		if (chip->dc_psy_type != -EINVAL) {
			pr_debug("dc psy changed\n");
			power_supply_changed(&chip->dc_psy);
		}
	}

	mutex_unlock(&chip->irq_complete);

	return IRQ_HANDLED;
}

#define LAST_CNFG_REG	0x1F
static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct smb135x_chg *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_cnfg_regs, chip);
}

static const struct file_operations cnfg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cnfg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_CMD_REG	0x40
#define LAST_CMD_REG	0x42
static int show_cmd_regs(struct seq_file *m, void *data)
{
	struct smb135x_chg *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int cmd_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_cmd_regs, chip);
}

static const struct file_operations cmd_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cmd_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#define FIRST_STATUS_REG	0x46
#define LAST_STATUS_REG		0x56
static int show_status_regs(struct seq_file *m, void *data)
{
	struct smb135x_chg *chip = m->private;
	int rc;
	u8 reg;
	u8 addr;

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (!rc)
			seq_printf(m, "0x%02x = 0x%02x\n", addr, reg);
	}

	return 0;
}

static int status_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_status_regs, chip);
}

static const struct file_operations status_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= status_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_irq_count(struct seq_file *m, void *data)
{
	int i, j, total = 0;

	for (i = 0; i < ARRAY_SIZE(handlers); i++)
		for (j = 0; j < 4; j++) {
			seq_printf(m, "%s=%d\t(high=%d low=%d)\n",
						handlers[i].irq_info[j].name,
						handlers[i].irq_info[j].high
						+ handlers[i].irq_info[j].low,
						handlers[i].irq_info[j].high,
						handlers[i].irq_info[j].low);
			total += (handlers[i].irq_info[j].high
					+ handlers[i].irq_info[j].low);
		}

	seq_printf(m, "\n\tTotal = %d\n", total);

	return 0;
}

static int irq_count_debugfs_open(struct inode *inode, struct file *file)
{
	struct smb135x_chg *chip = inode->i_private;

	return single_open(file, show_irq_count, chip);
}

static const struct file_operations irq_count_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= irq_count_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int get_reg(void *data, u64 *val)
{
	struct smb135x_chg *chip = data;
	int rc;
	u8 temp;

	rc = smb135x_read(chip, chip->peek_poke_address, &temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't read reg %x rc = %d\n",
			chip->peek_poke_address, rc);
		return -EAGAIN;
	}
	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct smb135x_chg *chip = data;
	int rc;
	u8 temp;

	temp = (u8) val;
	rc = __smb135x_write_fac(chip, chip->peek_poke_address, temp);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't write 0x%02x to 0x%02x rc= %d\n",
			chip->peek_poke_address, temp, rc);
		return -EAGAIN;
	}
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int force_irq_set(void *data, u64 val)
{
	struct smb135x_chg *chip = data;

	smb135x_chg_stat_handler(chip->client->irq, data);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_irq_ops, NULL, force_irq_set, "0x%02llx\n");

static int force_rechg_set(void *data, u64 val)
{
	int rc;
	struct smb135x_chg *chip = data;

	if (!chip->chg_enabled) {
		pr_debug("Charging Disabled force recharge not allowed\n");
		return -EINVAL;
	}

	rc = smb135x_masked_write(chip, CFG_14_REG, EN_CHG_INHIBIT_BIT, 0);
	if (rc)
		dev_err(chip->dev,
			"Couldn't disable charge-inhibit rc=%d\n", rc);
	/* delay for charge-inhibit to take affect */
	msleep(500);
	rc |= smb135x_charging(chip, false);
	rc |= smb135x_charging(chip, true);
	rc |= smb135x_masked_write(chip, CFG_14_REG, EN_CHG_INHIBIT_BIT,
						EN_CHG_INHIBIT_BIT);
	if (rc)
		dev_err(chip->dev,
			"Couldn't enable charge-inhibit rc=%d\n", rc);

	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(force_rechg_ops, NULL, force_rechg_set, "0x%02llx\n");

#ifdef DEBUG
static void dump_regs(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;
	u8 addr;

	for (addr = 0; addr <= LAST_CNFG_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_STATUS_REG; addr <= LAST_STATUS_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}

	for (addr = FIRST_CMD_REG; addr <= LAST_CMD_REG; addr++) {
		rc = smb135x_read(chip, addr, &reg);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't read 0x%02x rc = %d\n",
					addr, rc);
		else
			pr_debug("0x%02x = 0x%02x\n", addr, reg);
	}
}
#else
static void dump_regs(struct smb135x_chg *chip)
{
}
#endif
static int determine_initial_status(struct smb135x_chg *chip)
{
	int rc;
	u8 reg;

	/*
	 * It is okay to read the interrupt status here since
	 * interrupts aren't requested. reading interrupt status
	 * clears the interrupt so be careful to read interrupt
	 * status only in interrupt handling code
	 */

	chip->batt_present = true;
	rc = smb135x_read(chip, IRQ_B_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq b rc = %d\n", rc);
		return rc;
	}
	if (reg & IRQ_B_BATT_TERMINAL_BIT || reg & IRQ_B_BATT_MISSING_BIT)
		chip->batt_present = false;
	rc = smb135x_read(chip, STATUS_4_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 4 rc = %d\n", rc);
		return rc;
	}
	/* treat battery gone if less than 2V */
	if (reg & BATT_LESS_THAN_2V)
		chip->batt_present = false;

	rc = smb135x_read(chip, IRQ_A_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq A rc = %d\n", rc);
		return rc;
	}

	if (reg & IRQ_A_HOT_HARD_BIT)
		chip->batt_hot = true;
	if (reg & IRQ_A_COLD_HARD_BIT)
		chip->batt_cold = true;
	if (reg & IRQ_A_HOT_SOFT_BIT)
		chip->batt_warm = true;
	if (reg & IRQ_A_COLD_SOFT_BIT)
		chip->batt_cool = true;

	rc = smb135x_read(chip, IRQ_C_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq A rc = %d\n", rc);
		return rc;
	}
	if (reg & IRQ_C_TERM_BIT)
		chip->chg_done_batt_full = true;

	rc = smb135x_read(chip, IRQ_E_REG, &reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read irq E rc = %d\n", rc);
		return rc;
	}
	chip->usb_present = !(reg & IRQ_E_USB_OV_BIT)
				&& !(reg & IRQ_E_USB_UV_BIT);
	chip->dc_present = !(reg & IRQ_E_DC_OV_BIT) && !(reg & IRQ_E_DC_UV_BIT);

	if (chip->usb_present) {
		handle_usb_insertion(chip);
		if (smb135x_get_charge_rate(chip) ==
		    POWER_SUPPLY_CHARGE_RATE_TURBO)
			chip->hvdcp_powerup = true;
	} else
		handle_usb_removal(chip);

	if (chip->dc_psy_type != -EINVAL) {
		if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIRELESS) {
			/*
			 * put the dc path in suspend state if it is powered
			 * by wireless charger
			 */
			if (chip->dc_present)
				smb135x_path_suspend(chip, DC, CURRENT, false);
			else
				smb135x_path_suspend(chip, DC, CURRENT, true);
		}
	}
	return 0;
}

static int smb135x_hw_init(struct smb135x_chg *chip)
{
	int rc;
	int i;
	u8 reg, mask;

	if (chip->therm_bias_vreg) {
		rc = regulator_enable(chip->therm_bias_vreg);
		if (rc) {
			pr_err("Couldn't enable therm-bias rc = %d\n", rc);
			return rc;
		}
	}

	rc = smb135x_enable_volatile_writes(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't configure for volatile rc = %d\n",
				rc);
		return rc;
	}

	/*
	 * force using current from the register i.e. ignore auto
	 * power source detect (APSD) mA ratings
	 */
	mask = USE_REGISTER_FOR_CURRENT;

	if (chip->workaround_flags & WRKARND_USB100_BIT)
		reg = 0;
	else
		/* this ignores APSD results */
		reg = USE_REGISTER_FOR_CURRENT;

	if (chip->aicl_disabled) {
		/* Disable AICL */
		rc = smb135x_masked_write(chip, CFG_D_REG, AICL_ENABLE, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't Disable AICL\n");
			return rc;
		}
	} else {
		/* Set AICL Glich to 20 ms */
		rc = smb135x_masked_write(chip, CFG_D_REG, AICL_GLITCH, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set 20 ms AICL glitch\n");
			return rc;
		}
	}

	rc = smb135x_masked_write(chip, CMD_INPUT_LIMIT, mask, reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set input limit cmd rc=%d\n", rc);
		return rc;
	}

	/* set bit 0 = 100mA bit 1 = 500mA and set register control */
	rc = smb135x_masked_write(chip, CFG_E_REG,
			POLARITY_100_500_BIT | USB_CTRL_BY_PIN_BIT,
			POLARITY_100_500_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usbin cfg rc=%d\n", rc);
		return rc;
	}

	/*
	 * set chg en by cmd register, set chg en by writing bit 1,
	 * enable auto pre to fast, enable current termination, enable
	 * auto recharge, disable chg inhibition
	 */
	rc = smb135x_masked_write(chip, CFG_14_REG,
			CHG_EN_BY_PIN_BIT | CHG_EN_ACTIVE_LOW_BIT
			| PRE_TO_FAST_REQ_CMD_BIT | DISABLE_AUTO_RECHARGE_BIT
			| EN_CHG_INHIBIT_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set cfg 14 rc=%d\n", rc);
		return rc;
	}

	/* control USB suspend via command bits */
	rc = smb135x_masked_write(chip, USBIN_DCIN_CFG_REG,
		USBIN_SUSPEND_VIA_COMMAND_BIT, USBIN_SUSPEND_VIA_COMMAND_BIT);

	/* Set the OTG Current Limit */
	rc = smb135x_masked_write(chip, USBIN_DCIN_CFG_REG,
		OTG_CURRENT_LIMIT_MASK, OTG_CURRENT_LIMIT_1000MA);

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smb135x_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
	}

	/* set Battery Current */
	if (chip->batt_current_ma != -EINVAL) {
		rc = smb135x_set_batt_current(chip, chip->batt_current_ma);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set Battery Current rc = %d\n", rc);
			return rc;
		}
	}

	/* set iterm */
	if (chip->iterm_ma != -EINVAL) {
		if (chip->iterm_disabled) {
			dev_err(chip->dev, "Error: Both iterm_disabled and iterm_ma set\n");
			return -EINVAL;
		} else {
			if (chip->iterm_ma <= 50)
				reg = CHG_ITERM_50MA;
			else if (chip->iterm_ma <= 100)
				reg = CHG_ITERM_100MA;
			else if (chip->iterm_ma <= 150)
				reg = CHG_ITERM_150MA;
			else if (chip->iterm_ma <= 200)
				reg = CHG_ITERM_200MA;
			else if (chip->iterm_ma <= 250)
				reg = CHG_ITERM_250MA;
			else if (chip->iterm_ma <= 300)
				reg = CHG_ITERM_300MA;
			else if (chip->iterm_ma <= 500)
				reg = CHG_ITERM_500MA;
			else
				reg = CHG_ITERM_600MA;

			rc = smb135x_masked_write(chip, CFG_3_REG,
							CHG_ITERM_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set iterm rc = %d\n", rc);
				return rc;
			}

			rc = smb135x_masked_write(chip, CFG_14_REG,
						DISABLE_CURRENT_TERM_BIT, 0);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't enable iterm rc = %d\n", rc);
				return rc;
			}
		}
	} else  if (chip->iterm_disabled) {
		rc = smb135x_masked_write(chip, CFG_14_REG,
					DISABLE_CURRENT_TERM_BIT,
					DISABLE_CURRENT_TERM_BIT);
		if (rc) {
			dev_err(chip->dev, "Couldn't set iterm rc = %d\n",
								rc);
			return rc;
		}
	}

	/* set ir comp */
	if (chip->ir_comp_mv != -EINVAL) {
		if (chip->ir_comp_mv == 0)
			reg = CHG_IR_COMP_DIS;
		else if (chip->ir_comp_mv <= 25)
			reg = CHG_IR_COMP_25MV;
		else if (chip->ir_comp_mv <= 50)
			reg = CHG_IR_COMP_50MV;
		else if (chip->ir_comp_mv <= 75)
			reg = CHG_IR_COMP_75MV;
		else if (chip->ir_comp_mv <= 100)
			reg = CHG_IR_COMP_100MV;
		else if (chip->ir_comp_mv <= 125)
			reg = CHG_IR_COMP_125MV;
		else if (chip->ir_comp_mv <= 150)
			reg = CHG_IR_COMP_150MV;
		else
			reg = CHG_IR_COMP_175MV;

		rc = smb135x_masked_write(chip, CFG_3_REG,
					  CHG_IR_COMP_MASK, reg);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't set ir comp rc = %d\n", rc);
			return rc;
		}
	}

	/* set the safety time voltage */
	if (chip->safety_time != -EINVAL) {
		if (chip->safety_time == 0) {
			/* safety timer disabled */
			rc = smb135x_masked_write(chip, CFG_16_REG,
							SAFETY_TIME_EN_BIT, 0);
			if (rc < 0) {
				dev_err(chip->dev,
				"Couldn't disable safety timer rc = %d\n",
				rc);
				return rc;
			}
		} else {
			for (i = 0; i < ARRAY_SIZE(chg_time); i++) {
				if (chip->safety_time <= chg_time[i]) {
					reg = i << SAFETY_TIME_MINUTES_SHIFT;
					break;
				}
			}
			rc = smb135x_masked_write(chip, CFG_16_REG,
				SAFETY_TIME_EN_BIT | SAFETY_TIME_MINUTES_MASK,
				SAFETY_TIME_EN_BIT | reg);
			if (rc < 0) {
				dev_err(chip->dev,
					"Couldn't set safety timer rc = %d\n",
					rc);
				return rc;
			}
		}
	}

	/* battery missing detection */
	rc = smb135x_masked_write(chip, CFG_19_REG,
			BATT_MISSING_ALGO_BIT | BATT_MISSING_THERM_BIT,
			chip->bmd_algo_disabled ? BATT_MISSING_THERM_BIT :
						BATT_MISSING_ALGO_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set batt_missing config = %d\n",
									rc);
		return rc;
	}

	__smb135x_charging(chip, chip->chg_enabled);

	/* interrupt enabling - active low */
	if (chip->client->irq) {
		mask = CHG_STAT_IRQ_ONLY_BIT | CHG_STAT_ACTIVE_HIGH_BIT
			| CHG_STAT_DISABLE_BIT;
		reg = CHG_STAT_IRQ_ONLY_BIT;
		rc = smb135x_masked_write(chip, CFG_17_REG, mask, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set irq config rc = %d\n",
					rc);
			return rc;
		}

		/* enabling only interesting interrupts */
		rc = smb135x_write(chip, IRQ_CFG_REG,
			IRQ_BAT_HOT_COLD_HARD_BIT
			| IRQ_BAT_HOT_COLD_SOFT_BIT
			| IRQ_OTG_OVER_CURRENT_BIT
			| IRQ_INTERNAL_TEMPERATURE_BIT
			| IRQ_USBIN_UV_BIT);

		rc |= smb135x_write(chip, IRQ2_CFG_REG,
			IRQ2_SAFETY_TIMER_BIT
			| IRQ2_CHG_ERR_BIT
			| IRQ2_CHG_PHASE_CHANGE_BIT
			| IRQ2_BATT_MISSING_BIT
			| IRQ2_VBAT_LOW_BIT);

		rc |= smb135x_write(chip, IRQ3_CFG_REG, IRQ3_SRC_DETECT_BIT
				| IRQ3_DCIN_UV_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set irq enable rc = %d\n",
					rc);
			return rc;
		}
	}

	/* resume threshold */
	if (chip->resume_delta_mv != -EINVAL) {
		if (chip->resume_delta_mv < 100)
			reg = CHG_INHIBIT_50MV_VAL;
		else if (chip->resume_delta_mv < 200)
			reg = CHG_INHIBIT_100MV_VAL;
		else if (chip->resume_delta_mv < 300)
			reg = CHG_INHIBIT_200MV_VAL;
		else
			reg = CHG_INHIBIT_300MV_VAL;

		rc = smb135x_masked_write(chip, CFG_4_REG,
						CHG_INHIBIT_MASK, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set inhibit val rc = %d\n",
					rc);
			return rc;
		}

		if (chip->resume_delta_mv < 200)
			reg = 0;
		else
			 reg = RECHARGE_200MV_BIT;

		rc = smb135x_masked_write(chip, CFG_5_REG,
						RECHARGE_200MV_BIT, reg);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set recharge  rc = %d\n",
					rc);
			return rc;
		}
	}

	/* DC path current settings */
	if (chip->dc_psy_type != -EINVAL) {
		rc = smb135x_set_dc_chg_current(chip, chip->dc_psy_ma);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set dc charge current rc = %d\n",
					rc);
			return rc;
		}

		/* Configure Command mode for DCIN */
		rc = smb135x_masked_write(chip, CFG_18_REG,
					  DCIN_CMD_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc command mode rc = %d\n",
				rc);
			return rc;
		}

		/* Configure DCIN 5V to 9V */
		rc = smb135x_masked_write(chip, CFG_A_REG,
					  DCIN_VOLT_SEL, 0x40);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc voltage mode rc = %d\n",
				rc);
			return rc;
		}

		/* Configure DCIN AICL */
		rc = smb135x_masked_write(chip, CFG_B_REG,
					  DCIN_AICL_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set dc AICL rc = %d\n",
				rc);
			return rc;
		}
	}

	/*
	 * on some devices the battery is powered via external sources which
	 * could raise its voltage above the float voltage. smb135x chips go
	 * in to reverse boost in such a situation and the workaround is to
	 * disable float voltage compensation (note that the battery will appear
	 * hot/cold when powered via external source).
	 */

	if (chip->soft_vfloat_comp_disabled) {
		mask = HOT_SOFT_VFLOAT_COMP_EN_BIT
				| COLD_SOFT_VFLOAT_COMP_EN_BIT;
		rc = smb135x_masked_write(chip, CFG_1A_REG, mask, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't disable soft vfloat rc = %d\n",
					rc);
			return rc;
		}
	}

	return rc;
}

static int smb135x_hw_init_fac(struct smb135x_chg *chip)
{
	int rc;

	pr_info("Factory Mode I2C Writes Disabled!\n");
	rc = smb135x_masked_write_fac(chip, CMD_I2C_REG,
				      ALLOW_VOLATILE_BIT,
				      ALLOW_VOLATILE_BIT);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't configure for volatile rc = %d\n",
			rc);
	/* interrupt enabling - active low */
	if (chip->client->irq) {
		rc = smb135x_masked_write_fac(chip, CFG_17_REG,
					      CHG_STAT_IRQ_ONLY_BIT |
					      CHG_STAT_ACTIVE_HIGH_BIT
					      | CHG_STAT_DISABLE_BIT,
					      CHG_STAT_IRQ_ONLY_BIT);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set irq config rc = %d\n",
				rc);
			return rc;
		}

		/* enabling only interesting interrupts */
		rc = __smb135x_write_fac(chip, IRQ_CFG_REG,
					 IRQ_BAT_HOT_COLD_HARD_BIT
					 | IRQ_BAT_HOT_COLD_SOFT_BIT
					 | IRQ_INTERNAL_TEMPERATURE_BIT
					 | IRQ_USBIN_UV_BIT);

		rc |= __smb135x_write_fac(chip, IRQ2_CFG_REG,
					  IRQ2_SAFETY_TIMER_BIT
					  | IRQ2_CHG_ERR_BIT
					  | IRQ2_CHG_PHASE_CHANGE_BIT
					  | IRQ2_BATT_MISSING_BIT
					  | IRQ2_VBAT_LOW_BIT);

		rc |= __smb135x_write_fac(chip, IRQ3_CFG_REG,
					  IRQ3_SRC_DETECT_BIT
					  | IRQ3_DCIN_UV_BIT);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set irq enable rc = %d\n",
				rc);
			return rc;
		}
	}

	if (chip->dc_psy_type != -EINVAL) {
		rc = smb135x_masked_write_fac(chip, CFG_A_REG,
					      DCIN_INPUT_MASK,
					      DCIN_INPUT_FAC);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set dc current rc = %d\n",
				rc);
			return rc;
		}

		/* Configure Command mode for DCIN */
		rc = smb135x_masked_write_fac(chip, CFG_18_REG,
					      DCIN_CMD_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc command mode rc = %d\n",
				rc);
			return rc;
		}

		/* Configure DCIN 5V to 9V */
		rc = smb135x_masked_write_fac(chip, CFG_A_REG,
					      DCIN_VOLT_SEL, 0x40);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set dc voltage mode rc = %d\n",
				rc);
			return rc;
		}

		/* Configure DCIN AICL */
		rc = smb135x_masked_write_fac(chip, CFG_B_REG,
					      DCIN_AICL_BIT, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set dc AICL rc = %d\n",
				rc);
			return rc;
		}
	}
	return rc;
}

static struct of_device_id smb135x_match_table[] = {
	{ .compatible = "qcom,smb1356-charger", },
	{ .compatible = "qcom,smb1357-charger", },
	{ .compatible = "qcom,smb1358-charger", },
	{ .compatible = "qcom,smb1359-charger", },
	{ },
};

#define DC_MA_MIN 300
#define DC_MA_MAX 2000
static int smb_parse_dt(struct smb135x_chg *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;
	const struct of_device_id *match;
	const char *dc_psy_type;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	match = of_match_node(smb135x_match_table, node);
	if (match == NULL) {
		dev_err(chip->dev, "device tree match not found\n");
		return -EINVAL;
	}

	chip->usb_current_arr_size = (int)match->data;

	rc = of_property_read_u32(node, "qcom,float-voltage-mv",
						&chip->vfloat_mv);
	if (rc < 0)
		chip->vfloat_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,max-batt-curr-ma",
				  &chip->batt_current_ma);
	if (rc < 0)
		chip->batt_current_ma = -EINVAL;

	rc = of_property_read_u32(node, "qcom,charging-timeout",
						&chip->safety_time);
	if (rc < 0)
		chip->safety_time = -EINVAL;

	if (!rc &&
		(chip->safety_time > chg_time[ARRAY_SIZE(chg_time) - 1])) {
		dev_err(chip->dev, "Bad charging-timeout %d\n",
						chip->safety_time);
		return -EINVAL;
	}

	chip->bmd_algo_disabled = of_property_read_bool(node,
						"qcom,bmd-algo-disabled");

	chip->dc_psy_type = -EINVAL;
	dc_psy_type = of_get_property(node, "qcom,dc-psy-type", NULL);
	if (dc_psy_type) {
		if (strcmp(dc_psy_type, "Mains") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_MAINS;
		else if (strcmp(dc_psy_type, "Wireless") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIRELESS;
	}

	if (chip->dc_psy_type != -EINVAL) {
		rc = of_property_read_u32(node, "qcom,dc-psy-ma",
							&chip->dc_psy_ma);
		if (rc < 0) {
			dev_err(chip->dev,
					"no mA current for dc rc = %d\n", rc);
			return rc;
		}

		if (chip->dc_psy_ma < DC_MA_MIN
				|| chip->dc_psy_ma > DC_MA_MAX) {
			dev_err(chip->dev, "Bad dc mA %d\n", chip->dc_psy_ma);
			return -EINVAL;
		}
	}

	rc = of_property_read_u32(node, "qcom,recharge-thresh-mv",
						&chip->resume_delta_mv);
	if (rc < 0)
		chip->resume_delta_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,iterm-ma", &chip->iterm_ma);
	if (rc < 0)
		chip->iterm_ma = -EINVAL;

	rc = of_property_read_u32(node, "qcom,ir-comp-mv", &chip->ir_comp_mv);
	if (rc < 0)
		chip->ir_comp_mv = -EINVAL;

	rc = of_property_read_u32(node, "qcom,ext-temp-volt-mv",
				  &chip->ext_temp_volt_mv);

	rc = of_property_read_u32(node, "qcom,ext-temp-volt-mv",
				  &chip->ext_temp_volt_mv);
	if (rc < 0)
		chip->ext_temp_volt_mv = 0;

	rc = of_property_read_u32(node, "qcom,ext-temp-soc",
				  &chip->ext_temp_soc);
	if (rc < 0)
		chip->ext_temp_soc = 0;

	chip->aicl_disabled = of_property_read_bool(node,
						    "qcom,aicl-disabled");

	rc = of_property_read_u32(node, "qcom,low-voltage-uv",
				  &chip->low_voltage_uv);
	if (rc < 0)
		chip->low_voltage_uv = 2750000;

	rc = of_property_read_u32(node, "qcom,max-voltage-uv",
				  &chip->max_voltage_uv);
	if (rc < 0)
		chip->max_voltage_uv = 4350000;

	chip->iterm_disabled = of_property_read_bool(node,
						"qcom,iterm-disabled");

	chip->chg_enabled = !(of_property_read_bool(node,
						"qcom,charging-disabled"));

	rc = of_property_read_string(node, "qcom,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	chip->soft_vfloat_comp_disabled = of_property_read_bool(node,
					"qcom,soft-vfloat-comp-disabled");

	if (of_find_property(node, "therm-bias-supply", NULL)) {
		/* get the thermistor bias regulator */
		chip->therm_bias_vreg = devm_regulator_get(chip->dev,
							"therm-bias");
		if (IS_ERR(chip->therm_bias_vreg))
			return PTR_ERR(chip->therm_bias_vreg);
	}

	if (of_find_property(node, "qcom,thermal-mitigation",
					&chip->thermal_levels)) {
		chip->thermal_mitigation = devm_kzalloc(chip->dev,
			chip->thermal_levels,
			GFP_KERNEL);

		if (chip->thermal_mitigation == NULL) {
			pr_err("thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chip->thermal_mitigation, chip->thermal_levels);
		if (rc) {
			pr_err("Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	} else
		chip->thermal_levels = 0;

	if (of_find_property(node, "qcom,dc-thermal-mitigation",
					&chip->dc_thermal_levels)) {
		chip->dc_thermal_mitigation = devm_kzalloc(chip->dev,
			chip->dc_thermal_levels,
			GFP_KERNEL);

		if (chip->dc_thermal_mitigation == NULL) {
			pr_err("DC thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->dc_thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
				"qcom,dc-thermal-mitigation",
				chip->dc_thermal_mitigation,
				chip->dc_thermal_levels);
		if (rc) {
			pr_err("Couldn't read DC therm limits rc = %d\n", rc);
			return rc;
		}
	} else
		chip->dc_thermal_levels = 0;

	return 0;
}

#define CHG_SHOW_MAX_SIZE 50
#define USB_SUSPEND_BIT BIT(4)
static ssize_t force_chg_usb_suspend_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid usb suspend mode value = %lu\n", mode);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	r = smb135x_masked_write_fac(the_chip, CFG_11_REG,
				     USB_SUSPEND_BIT,
				     mode ? USB_SUSPEND_BIT : 0);

	return r ? r : count;
}

#define USB_SUSPEND_STATUS_BIT BIT(3)
static ssize_t force_chg_usb_suspend_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	ret = smb135x_read(the_chip, STATUS_1_REG, &value);
	if (ret) {
		pr_err("USB_SUSPEND_STATUS_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (USB_SUSPEND_STATUS_BIT & value) ? 1 : 0;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_usb_suspend, 0664,
		force_chg_usb_suspend_show,
		force_chg_usb_suspend_store);

static ssize_t force_chg_fail_clear_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid chg fail mode value = %lu\n", mode);
		return -EINVAL;
	}

	/* do nothing for SMB135X */
	r = 0;

	return r ? r : count;
}

static ssize_t force_chg_fail_clear_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/* do nothing for SMB135X */
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "0\n");
}

static DEVICE_ATTR(force_chg_fail_clear, 0664,
		force_chg_fail_clear_show,
		force_chg_fail_clear_store);

static ssize_t force_chg_auto_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid chrg enable value = %lu\n", mode);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	r = smb135x_masked_write_fac(the_chip, CMD_CHG_REG,
				     CMD_CHG_EN, mode ? 0 : CMD_CHG_EN);
	if (r < 0) {
		dev_err(the_chip->dev,
			"Couldn't set CHG_ENABLE_BIT enable = %d r = %d\n",
			(int)mode, (int)r);
		return r;
	}

	return r ? r : count;
}

static ssize_t force_chg_auto_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smb135x_read(the_chip, STATUS_4_REG, &value);
	if (ret) {
		pr_err("CHG_EN_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (CHG_EN_BIT & value) ? 1 : 0;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_auto_enable, 0664,
		force_chg_auto_enable_show,
		force_chg_auto_enable_store);

#define MAX_IBATT_LEVELS 31
static ssize_t force_chg_ibatt_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;
	int i;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		pr_err("Invalid ibatt value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	for (i = MAX_IBATT_LEVELS - 1; i >= 0; i--) {
		if (chg_current >= batt_current_table[i])
			break;
	}

	r = smb135x_masked_write_fac(the_chip, CFG_1C_REG,
				     BATT_CURR_MASK, i);
	if (r < 0) {
		dev_err(the_chip->dev,
			"Couldn't set Fast Charge Current = %d r = %d\n",
			(int)chg_current, (int)r);
		return r;
	}

	return r ? r : count;
}

static ssize_t force_chg_ibatt_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smb135x_read(the_chip, 0x49, &value);
	if (ret ||
	    ((value & SMB135X_MASK(4, 0)) > (MAX_IBATT_LEVELS - 1))) {
		pr_err("Fast Charge Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = batt_current_table[(value & SMB135X_MASK(4, 0))];

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_ibatt, 0664,
		force_chg_ibatt_show,
		force_chg_ibatt_store);

static ssize_t force_chg_iusb_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long usb_curr;
	int i;

	r = kstrtoul(buf, 0, &usb_curr);
	if (r) {
		pr_err("Invalid iusb value = %lu\n", usb_curr);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	for (i = the_chip->usb_current_arr_size - 1; i >= 0; i--) {
		if (usb_curr >= the_chip->usb_current_table[i])
			break;
	}

	r = smb135x_masked_write_fac(the_chip, CFG_C_REG,
				     USBIN_INPUT_MASK, i);
	if (r < 0) {
		dev_err(the_chip->dev,
			"Couldn't set USBIN Current = %d r = %d\n",
			(int)usb_curr, (int)r);
		return r;
	}
	return r ? r : count;
}

static ssize_t force_chg_iusb_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		ret = -ENODEV;
		goto end;
	}

	ret = smb135x_read(the_chip, 0x46, &value);
	if (ret ||
	    ((value & USBIN_INPUT_MASK) >
	     (the_chip->usb_current_arr_size - 1))) {
		pr_err("USBIN Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = the_chip->usb_current_table[(value & USBIN_INPUT_MASK)];
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_iusb, 0664,
		force_chg_iusb_show,
		force_chg_iusb_store);


#define PRECHG_OFFSET 100
#define PRECHG_STEP 50
#define PRECHG_MAX 250
#define PRECHG_REG_SHIFT 5
static ssize_t force_chg_itrick_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;
	int i;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		pr_err("Invalid pre-charge value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	for (i = PRECHG_MAX; i > PRECHG_OFFSET; i = i - PRECHG_STEP) {
		if (chg_current >= i)
			break;
	}

	i = (i - PRECHG_OFFSET) / PRECHG_STEP;

	i = (i << PRECHG_REG_SHIFT) & SMB135X_MASK(7, 5);

	r = smb135x_masked_write_fac(the_chip, CFG_1C_REG,
				     BATT_CURR_MASK, i);
	if (r < 0) {
		dev_err(the_chip->dev,
			"Couldn't set Pre-Charge Current = %d r = %d\n",
			(int)chg_current, (int)r);
		return r;
	}


	return r ? r : count;
}

static ssize_t force_chg_itrick_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smb135x_read(the_chip, 0x49, &value);
	if (ret) {
		pr_err("Pre-Charge Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (value & SMB135X_MASK(7, 5)) >> PRECHG_REG_SHIFT;

	state = (state * PRECHG_STEP) + PRECHG_OFFSET;
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_itrick, 0664,
		   force_chg_itrick_show,
		   force_chg_itrick_store);

#define OTG_EN_BIT BIT(0)
static ssize_t force_chg_usb_otg_ctl_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int state;
	int ret;
	u8 value;

	if (!the_chip) {
		pr_err("chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smb135x_read(the_chip, CMD_CHG_REG, &value);
	if (ret) {
		pr_err("OTG_EN_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (OTG_EN_BIT & value) ? 1 : 0;
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static ssize_t force_chg_usb_otg_ctl_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid otg ctl value = %lu\n", mode);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	if (mode) {
		r = smb135x_masked_write_fac(the_chip, CMD_CHG_REG,
					     (CMD_CHG_EN | OTG_EN_BIT),
					     (CMD_CHG_EN | OTG_EN_BIT));
	} else {
		r = smb135x_masked_write_fac(the_chip, CMD_CHG_REG,
					     (CMD_CHG_EN | OTG_EN_BIT),
					     0);
	}

	if (r < 0)
		dev_err(the_chip->dev,
			"Couldn't set OTG mode = %d r = %d\n",
			(int)mode, (int)r);

	return r ? r : count;
}

static DEVICE_ATTR(force_chg_usb_otg_ctl, 0664,
		   force_chg_usb_otg_ctl_show,
		   force_chg_usb_otg_ctl_store);

static bool smb135x_charger_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}

static int smb135x_charger_reboot(struct notifier_block *nb,
				unsigned long event, void *unused)
{
	struct smb135x_chg *chip =
			container_of(nb, struct smb135x_chg, smb_reboot);
	int rc = 0;

	dev_dbg(chip->dev, "SMB Reboot\n");

	rc = smb135x_masked_write(chip, CMD_CHG_REG, OTG_EN, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);

	/* force usb/dc shutdown on halt */
	if (event == SYS_HALT) {
		__smb135x_usb_suspend(chip, true);
		__smb135x_dc_suspend(chip, true);
	}

	return NOTIFY_DONE;
}

static int smb135x_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int rc;
	struct smb135x_chg *chip;
	struct power_supply *usb_psy;
	u8 reg = 0;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&client->dev, "USB supply not found; defer probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	chip->factory_mode = smb135x_charger_mmi_factory();
	if (chip->factory_mode)
		chip->apsd_rerun_cnt = 1;

	chip->client = client;
	chip->dev = &client->dev;
	chip->usb_psy = usb_psy;
	chip->fake_battery_soc = -EINVAL;
	chip->charger_rate =  POWER_SUPPLY_CHARGE_RATE_NONE;
	chip->aicl_weak_detect = false;
	chip->invalid_battery = false;
	chip->shutdown_voltage_tripped = false;
	chip->poll_fast = false;
	chip->prev_batt_health = POWER_SUPPLY_HEALTH_GOOD;
	chip->therm_lvl_sel = -EINVAL;
	chip->dc_therm_lvl_sel = -EINVAL;
	chip->hvdcp_powerup = false;


	wakeup_source_init(&chip->smb_wake_source.source, "smb135x_wake");
	INIT_DELAYED_WORK(&chip->wireless_insertion_work,
					wireless_insertion_work);
	INIT_DELAYED_WORK(&chip->usb_insertion_work,
					usb_insertion_work);
	INIT_DELAYED_WORK(&chip->heartbeat_work,
					heartbeat_work);
	INIT_DELAYED_WORK(&chip->aicl_check_work,
					aicl_check_work);
	INIT_DELAYED_WORK(&chip->src_removal_work,
					src_removal_work);
	INIT_DELAYED_WORK(&chip->rate_check_work,
					rate_check_work);
	INIT_DELAYED_WORK(&chip->ocp_clear_work,
					ocp_clear_work);

	mutex_init(&chip->path_suspend_lock);
	mutex_init(&chip->current_change_lock);
	mutex_init(&chip->read_write_lock);
	/* probe the device to check if its actually connected */
	rc = smb135x_read(chip, CMD_INPUT_LIMIT, &reg);
	if (rc) {
		pr_err("Failed to detect SMB135x, device may be absent\n");
		return -ENODEV;
	} else {
		dev_info(&client->dev, " CMD_IL=%x\n", reg);
		if (reg & USB_SHUTDOWN_BIT)
			chip->usb_suspended = 0x1; /* USER bit */
		if (reg & DC_SHUTDOWN_BIT)
			chip->dc_suspended = 0x1; /* USER bit */
	}

	rc = smb_parse_dt(chip);
	if (rc < 0) {
		dev_err(&client->dev, "Unable to parse DT nodes\n");
		return rc;
	}

	i2c_set_clientdata(client, chip);

	rc = smb135x_chip_version_and_revision(chip);
	if (rc) {
		dev_err(&client->dev,
			"Couldn't detect version/revision rc=%d\n", rc);
		return rc;
	}

	dump_regs(chip);

	chip->vadc_dev = qpnp_get_vadc(chip->dev, "smb135x");
	if (IS_ERR(chip->vadc_dev)) {
		rc = PTR_ERR(chip->vadc_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("vadc not ready, defer probe\n");
		wakeup_source_trash(&chip->smb_wake_source.source);
		return rc;
	}

	chip->adc_tm_dev = qpnp_get_adc_tm(chip->dev, "smb135x");
	if (IS_ERR(chip->adc_tm_dev)) {
		rc = PTR_ERR(chip->adc_tm_dev);
		if (rc == -EPROBE_DEFER)
			pr_err("adc-tm not ready, defer probe\n");
		wakeup_source_trash(&chip->smb_wake_source.source);
		return rc;
	}

	rc = smb135x_regulator_init(chip);
	if  (rc) {
		dev_err(&client->dev,
			"Couldn't initialize regulator rc=%d\n", rc);
		return rc;
	}

	rc = smb135x_hw_init(chip);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to intialize hardware rc = %d\n", rc);
		goto free_regulator;
	}

	rc = determine_initial_status(chip);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to determine init status rc = %d\n", rc);
		goto free_regulator;
	}

	if (chip->factory_mode) {
		rc = smb135x_hw_init_fac(chip);
		if (rc < 0) {
			dev_err(&client->dev,
			 "Unable to init hardware for factory status rc = %d\n",
				rc);
			goto free_regulator;
		}
	}

	chip->batt_psy.name		= "battery";
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smb135x_battery_get_property;
	chip->batt_psy.set_property	= smb135x_battery_set_property;
	chip->batt_psy.properties	= smb135x_battery_properties;
	chip->batt_psy.num_properties  = ARRAY_SIZE(smb135x_battery_properties);
	chip->batt_psy.external_power_changed = smb135x_external_power_changed;
	chip->batt_psy.property_is_writeable = smb135x_battery_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&client->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		goto free_regulator;
	}

	if (chip->dc_psy_type != -EINVAL) {
		chip->dc_psy.name		= "dc";
		chip->dc_psy.type		= chip->dc_psy_type;
		chip->dc_psy.get_property	= smb135x_dc_get_property;
		chip->dc_psy.set_property       = smb135x_dc_set_property;
		chip->dc_psy.properties		= smb135x_dc_properties;
		chip->dc_psy.num_properties = ARRAY_SIZE(smb135x_dc_properties);
		chip->dc_psy.property_is_writeable = smb135x_dc_is_writeable;

		rc = power_supply_register(chip->dev, &chip->dc_psy);
		if (rc < 0) {
			dev_err(&client->dev,
				"Unable to register dc_psy rc = %d\n", rc);
			goto unregister_batt_psy;
		}
	}

	chip->resume_completed = true;
	mutex_init(&chip->irq_complete);

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				smb135x_chg_stat_handler,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"smb135x_chg_stat_irq", chip);
		if (rc < 0) {
			dev_err(&client->dev,
				"request_irq for irq=%d  failed rc = %d\n",
				client->irq, rc);
			goto unregister_dc_psy;
		}
		enable_irq_wake(client->irq);
	}

	chip->smb_reboot.notifier_call = smb135x_charger_reboot;
	chip->smb_reboot.next = NULL;
	chip->smb_reboot.priority = 1;
	rc = register_reboot_notifier(&chip->smb_reboot);
	if (rc)
		dev_err(chip->dev, "register for reboot failed\n");

	chip->debug_root = debugfs_create_dir("smb135x", NULL);
	if (!chip->debug_root)
		dev_err(chip->dev, "Couldn't create debug dir\n");

	if (chip->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_file("config_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cnfg_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create cnfg debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("status_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &status_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create status debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("cmd_registers", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &cmd_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create cmd debug file rc = %d\n",
				rc);

		ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->peek_poke_address));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create address debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &poke_poke_debug_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("force_irq",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &force_irq_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_x32("skip_writes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->skip_writes));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_x32("skip_reads",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->skip_reads));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create data debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("irq_count", S_IFREG | S_IRUGO,
					  chip->debug_root, chip,
					  &irq_count_debugfs_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create count debug file rc = %d\n",
				rc);

		ent = debugfs_create_file("force_recharge",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &force_rechg_ops);
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create recharge debug file rc = %d\n",
				rc);

		ent = debugfs_create_x32("usb_suspend_votes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->usb_suspended));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create usb vote file rc = %d\n",
				rc);

		ent = debugfs_create_x32("dc_suspend_votes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root,
					  &(chip->dc_suspended));
		if (!ent)
			dev_err(chip->dev,
				"Couldn't create dc vote file rc = %d\n",
				rc);
		}

	the_chip = chip;

	if (chip->factory_mode) {
		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_usb_suspend);
		if (rc) {
			pr_err("couldn't create force_chg_usb_suspend\n");
			goto unregister_dc_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_fail_clear);
		if (rc) {
			pr_err("couldn't create force_chg_fail_clear\n");
			goto unregister_dc_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_auto_enable);
		if (rc) {
			pr_err("couldn't create force_chg_auto_enable\n");
			goto unregister_dc_psy;
		}

		rc = device_create_file(chip->dev,
				&dev_attr_force_chg_ibatt);
		if (rc) {
			pr_err("couldn't create force_chg_ibatt\n");
			goto unregister_dc_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_iusb);
		if (rc) {
			pr_err("couldn't create force_chg_iusb\n");
			goto unregister_dc_psy;
		}

		rc = device_create_file(chip->dev,
					&dev_attr_force_chg_itrick);
		if (rc) {
			pr_err("couldn't create force_chg_itrick\n");
			goto unregister_dc_psy;
		}

		rc = device_create_file(chip->dev,
				&dev_attr_force_chg_usb_otg_ctl);
		if (rc) {
			pr_err("couldn't create force_chg_usb_otg_ctl\n");
			goto unregister_dc_psy;
		}

	}

	rc = smb135x_setup_vbat_monitoring(chip);
	if (rc < 0)
		pr_err("failed to set up voltage notifications: %d\n", rc);

	schedule_delayed_work(&chip->heartbeat_work,
			      msecs_to_jiffies(60000));

	dev_info(chip->dev, "SMB135X version = %s revision = %s successfully probed batt=%d dc = %d usb = %d\n",
			version_str[chip->version],
			revision_str[chip->revision],
			smb135x_get_prop_batt_present(chip),
			chip->dc_present, chip->usb_present);
	return 0;

unregister_dc_psy:
	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);
unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);
free_regulator:
	smb135x_regulator_deinit(chip);
	wakeup_source_trash(&chip->smb_wake_source.source);
	return rc;
}

static int smb135x_charger_remove(struct i2c_client *client)
{
	int rc;
	struct smb135x_chg *chip = i2c_get_clientdata(client);

	if (chip->therm_bias_vreg) {
		rc = regulator_disable(chip->therm_bias_vreg);
		if (rc)
			pr_err("Couldn't disable therm-bias rc = %d\n", rc);
	}

	unregister_reboot_notifier(&chip->smb_reboot);
	debugfs_remove_recursive(chip->debug_root);

	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);

	power_supply_unregister(&chip->batt_psy);

	mutex_destroy(&chip->irq_complete);
	if (chip->factory_mode) {
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_usb_suspend);
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_fail_clear);
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_auto_enable);
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_ibatt);
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_iusb);
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_itrick);
		device_remove_file(chip->dev,
				   &dev_attr_force_chg_usb_otg_ctl);
	}

	smb135x_regulator_deinit(chip);
	wakeup_source_trash(&chip->smb_wake_source.source);

	return 0;
}

static int smb135x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb135x_chg *chip = i2c_get_clientdata(client);
	int i, rc;

	/* Save the current IRQ config */
	for (i = 0; i < 3; i++) {
		rc = smb135x_read(chip, IRQ_CFG_REG + i,
					&chip->irq_cfg_mask[i]);
		if (rc)
			dev_err(chip->dev,
				"Couldn't save irq cfg regs rc=%d\n", rc);
	}

	/* enable only important IRQs */
	rc = smb135x_write(chip, IRQ_CFG_REG, IRQ_USBIN_UV_BIT);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set irq_cfg rc = %d\n", rc);

	rc = smb135x_write(chip, IRQ2_CFG_REG, IRQ2_BATT_MISSING_BIT
						| IRQ2_VBAT_LOW_BIT);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set irq2_cfg rc = %d\n", rc);

	rc = smb135x_write(chip, IRQ3_CFG_REG, IRQ3_SRC_DETECT_BIT
			| IRQ3_DCIN_UV_BIT);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set irq3_cfg rc = %d\n", rc);

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = false;
	mutex_unlock(&chip->irq_complete);

	return 0;
}

static int smb135x_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb135x_chg *chip = i2c_get_clientdata(client);

	if (chip->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int smb135x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct smb135x_chg *chip = i2c_get_clientdata(client);
	int i, rc;

	/* Restore the IRQ config */
	for (i = 0; i < 3; i++) {
		rc = smb135x_write(chip, IRQ_CFG_REG + i,
					chip->irq_cfg_mask[i]);
		if (rc)
			dev_err(chip->dev,
				"Couldn't restore irq cfg regs rc=%d\n", rc);
	}
	mutex_lock(&chip->irq_complete);
	chip->resume_completed = true;
	if (chip->irq_waiting) {
		mutex_unlock(&chip->irq_complete);
		smb135x_chg_stat_handler(client->irq, chip);
		enable_irq(client->irq);
	} else {
		mutex_unlock(&chip->irq_complete);
	}
	return 0;
}

static const struct dev_pm_ops smb135x_pm_ops = {
	.resume		= smb135x_resume,
	.suspend_noirq	= smb135x_suspend_noirq,
	.suspend	= smb135x_suspend,
};

static const struct i2c_device_id smb135x_charger_id[] = {
	{"smb135x-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb135x_charger_id);

static struct i2c_driver smb135x_charger_driver = {
	.driver		= {
		.name		= "smb135x-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= smb135x_match_table,
		.pm		= &smb135x_pm_ops,
	},
	.probe		= smb135x_charger_probe,
	.remove		= smb135x_charger_remove,
	.id_table	= smb135x_charger_id,
};

module_i2c_driver(smb135x_charger_driver);

MODULE_DESCRIPTION("SMB135x Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:smb135x-charger");
