/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
 * Copyright (c) 2013 LGE Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c/smb349.h>
#include <linux/power_supply.h>
#include <linux/of_gpio.h>

#define SMB349_MASK(BITS, POS)  ((unsigned char)(((1 << BITS) - 1) << POS))

/* Register definitions */
#define CHG_CURRENT_REG                         0x00
#define CHG_OTHER_CURRENT_REG                   0x01
#define VAR_FUNC_REG                            0x02
#define FLOAT_VOLTAGE_REG                       0x03
#define CHG_CTRL_REG                            0x04
#define STAT_TIMER_REG                          0x05
#define PIN_ENABLE_CTRL_REG                     0x06
#define THERM_CTRL_A_REG                        0x07
#define CTRL_FUNCTIONS_REG                      0x09
#define OTG_TLIM_THERM_CNTRL_REG                0x0A
#define HARD_SOFT_LIMIT_CELL_TEMP_MONITOR_REG   0x0B
#define FAULT_IRQ_REG                           0x0C
#define STATUS_IRQ_REG                          0x0D
#define SYSOK_USB3_SELECT_REG                   0x0E
#define FLEX_CHARGE_REG                         0x10
#define STATUS_INT_REG                          0x11
#define I2C_BUS_SLAVE_ADDR_REG                  0x12
#define CMD_A_REG                               0x30
#define CMD_B_REG                               0x31
#define CMD_C_REG                               0x33
#define IRQ_A_REG                               0x35
#define IRQ_B_REG                               0x36
#define IRQ_C_REG                               0x37
#define IRQ_D_REG                               0x38
#define IRQ_E_REG                               0x39
#define IRQ_F_REG                               0x3A
#define STATUS_A_REG                            0x3B
#define STATUS_B_REG                            0x3C
#define STATUS_C_REG                            0x3D
#define STATUS_D_REG                            0x3E
#define STATUS_E_REG                            0x3F

/* Status bits and masks */
#define CHG_STATUS_MASK                         SMB349_MASK(2, 1)
#define CHG_ENABLE_STATUS_BIT                   BIT(0)

/* Control bits and masks */
#define FAST_CHG_CURRENT_MASK                   SMB349_MASK(4, 4)
#define AC_INPUT_CURRENT_LIMIT_MASK             SMB349_MASK(4, 0)
#define PRE_CHG_CURRENT_MASK                    SMB349_MASK(3, 5)
#define TERMINATION_CURRENT_MASK                SMB349_MASK(3, 2)
#define PRE_CHG_TO_FAST_CHG_THRESH_MASK         SMB349_MASK(2, 6)
#define FLOAT_VOLTAGE_MASK                      SMB349_MASK(6, 0)
#define EN_PIN_CTRL_MASK                        SMB349_MASK(2, 5)
#define USB_OTG_EN_BIT                          BIT(4)
#define CHG_ENABLE_BIT                          BIT(1)
#define VOLATILE_W_PERM_BIT                     BIT(7)
#define USB_SELECTION_BIT                       BIT(1)
#define SYSTEM_FET_ENABLE_BIT                   BIT(7)
#define AUTOMATIC_INPUT_CURR_LIMIT_BIT          BIT(4)
#define AUTOMATIC_POWER_SOURCE_DETECTION_BIT    BIT(2)
#define BATT_OV_END_CHG_BIT                     BIT(1)
#define VCHG_FUNCTION                           BIT(0)
#define CURR_TERM_END_CHG_BIT                   BIT(6)
#define AUTO_RECHG_DIS_BIT                      BIT(7)
#define USB_2_3_SEL_BIT                         BIT(2)
#define USB_5_1_MODE_BIT                        BIT(1)
#define USB_HC_MODE_BIT                         BIT(0)
#define USB_CS_BIT                              BIT(4)
#define BATT_MISSING_STATUS_BIT                 BIT(4)
#define INOK_BIT                                BIT(2)
#define MISSING_BATT_BIT                        BIT(1)
#define FAST_CHARGE_SET_BIT                     BIT(6)
#define POWER_OK_BIT                            BIT(0)
#define PRE_CHG_TO_FAST_CHG_THRESH_BIT          BIT(1)
#define OTG_OC_LIMIT_STATUS_BIT                 BIT(6)
#define OTG_BATT_UV_STATUS_BIT                  BIT(4)
#define BOOST_BACK_PREVENTION_BIT               BIT(5)

#define SMB349_CHG_IUSB_MAX_MIN_100	100
#define SMB349_CHG_IUSB_MAX_MIN_MA	200

enum smb349_chg_status {
	SMB_CHG_STATUS_NONE		= 0,
	SMB_CHG_STATUS_PRE_CHARGE	= 1,
	SMB_CHG_STATUS_FAST_CHARGE	= 2,
	SMB_CHG_STATUS_TAPER_CHARGE	= 3,
	SMB_CHG_STATUS_EXCEPTION	= 4,
};

static const char * const smb349_chg_status[] = {
	"none",
	"pre-charge",
	"fast-charge",
	"taper-charge",
	"exception"
};

struct smb349_struct {
	struct i2c_client *client;
	struct dentry *dent;

	int chg_current_ma;
	int term_current_ma;
	int en_n_gpio;
	int chg_susp_gpio;
	int stat_gpio;
	int otg_en_gpio;
	int irq;
	int ext_pwr;
	int usb_present;
	int usb_online;
	bool ac_online;
	int chg_type;
	int charging_disabled;

	enum smb349_chg_status chg_status;

	struct delayed_work irq_work;
	struct wake_lock chg_wake_lock;
	struct power_supply ac_psy;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *wlc_psy;
	struct wake_lock uevent_wake_lock;
	int wlc_support;
	int ext_ovp_otg_ctrl;
};

static struct smb349_struct *the_smb349_chg;

struct debug_reg {
	char	*name;
	u8	reg;
};

#define SMB349_DEBUG_REG(x, y) {#x#y, y##_REG}

static struct debug_reg smb349_debug_regs[] = {
	SMB349_DEBUG_REG(00_, CHG_CURRENT),
	SMB349_DEBUG_REG(01_, CHG_OTHER_CURRENT),
	SMB349_DEBUG_REG(02_, VAR_FUNC),
	SMB349_DEBUG_REG(03_, FLOAT_VOLTAGE),
	SMB349_DEBUG_REG(04_, CHG_CTRL),
	SMB349_DEBUG_REG(05_, STAT_TIMER),
	SMB349_DEBUG_REG(06_, PIN_ENABLE_CTRL),
	SMB349_DEBUG_REG(07_, THERM_CTRL_A),
	SMB349_DEBUG_REG(09_, CTRL_FUNCTIONS),
	SMB349_DEBUG_REG(0A_, OTG_TLIM_THERM_CNTRL),
	SMB349_DEBUG_REG(0B_, HARD_SOFT_LIMIT_CELL_TEMP_MONITOR),
	SMB349_DEBUG_REG(0C_, FAULT_IRQ),
	SMB349_DEBUG_REG(0D_, STATUS_IRQ),
	SMB349_DEBUG_REG(0E_, SYSOK_USB3_SELECT),
	SMB349_DEBUG_REG(10_, FLEX_CHARGE),
	SMB349_DEBUG_REG(11_, STATUS_INT),
	SMB349_DEBUG_REG(12_, I2C_BUS_SLAVE_ADDR),
	SMB349_DEBUG_REG(30_, CMD_A),
	SMB349_DEBUG_REG(31_, CMD_B),
	SMB349_DEBUG_REG(33_, CMD_C),
	SMB349_DEBUG_REG(35_, IRQ_A),
	SMB349_DEBUG_REG(36_, IRQ_B),
	SMB349_DEBUG_REG(37_, IRQ_C),
	SMB349_DEBUG_REG(38_, IRQ_D),
	SMB349_DEBUG_REG(39_, IRQ_E),
	SMB349_DEBUG_REG(3A_, IRQ_F),
	SMB349_DEBUG_REG(3B_, STATUS_A),
	SMB349_DEBUG_REG(3C_, STATUS_B),
	SMB349_DEBUG_REG(3D_, STATUS_C),
	SMB349_DEBUG_REG(3E_, STATUS_D),
	SMB349_DEBUG_REG(3F_, STATUS_E),
};

int32_t smb349_is_ready(void)
{
	struct smb349_struct *smb349_chg = the_smb349_chg;

	if (!smb349_chg)
		return -EPROBE_DEFER;
	return 0;
}
EXPORT_SYMBOL(smb349_is_ready);

static int smb349_read_reg(struct i2c_client *client, int reg,
			   u8 *val)
{
	s32 ret;
	struct smb349_struct *smb349_chg;

	smb349_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_read_byte_data(smb349_chg->client, reg);
	if (ret < 0) {
		dev_err(&smb349_chg->client->dev,
			"i2c read fail: can't read from %02x: %d\n", reg, ret);
		return ret;
	} else {
		*val = ret;
	}

	return 0;
}

static int smb349_write_reg(struct i2c_client *client, int reg,
			    u8 val)
{
	s32 ret;
	struct smb349_struct *smb349_chg;

	smb349_chg = i2c_get_clientdata(client);
	ret = i2c_smbus_write_byte_data(smb349_chg->client, reg, val);
	if (ret < 0) {
		dev_err(&smb349_chg->client->dev,
			"i2c write fail: can't write %02x to %02x: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int smb349_masked_write(struct i2c_client *client, int reg,
			       u8 mask, u8 val)
{
	s32 rc;
	u8 temp;

	rc = smb349_read_reg(client, reg, &temp);
	if (rc) {
		pr_err("smb349_read_reg failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	temp &= ~mask;
	temp |= val & mask;
	rc = smb349_write_reg(client, reg, temp);
	if (rc) {
		pr_err("smb349_write failed: reg=%03X, rc=%d\n", reg, rc);
		return rc;
	}
	return 0;
}

static bool smb349_is_charger_present(struct smb349_struct *smb349_chg)
{
	u8 temp;
	bool chg_online;
	int ret;

	ret = smb349_read_reg(smb349_chg->client, IRQ_F_REG, &temp);
	if (ret) {
		pr_err("failed to read IRQ_STATUS_F_REG rc=%d\n", ret);
		return false;
	}

	chg_online = temp & 0x01;
	pr_debug("charger present = %d\n", chg_online);

	return chg_online;
}

#define SMB349_CHG_ENABLE_SHIFT	1
static int smb349_enable_charging(struct smb349_struct *smb349_chg, bool enable)
{
	int ret;
	u8 val = (u8)(!!enable << SMB349_CHG_ENABLE_SHIFT);

	pr_info("enable=%d\n", enable);

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
						CHG_ENABLE_BIT, val);
	if (ret) {
		pr_err("failed to set CHG_ENABLE_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/* When the status bit of a certain condition is read,
 * the corresponding IRQ signal is cleared.
 */
static int smb349_clear_irq(struct smb349_struct *smb349_chg)
{
	u8 val[6] = {0,};
	int ret;

	ret = smb349_read_reg(smb349_chg->client, IRQ_A_REG, &val[0]);
	if (ret < 0) {
		pr_err("failed to read IRQ_A_REG rc=%d\n", ret);
		return ret;
	}

	ret = smb349_read_reg(smb349_chg->client, IRQ_B_REG, &val[1]);
	if (ret < 0) {
		pr_err("failed to read IRQ_B_REG rc=%d\n", ret);
		return ret;
	}

	ret = smb349_read_reg(smb349_chg->client, IRQ_C_REG, &val[2]);
	if (ret < 0) {
		pr_err("Failed to read IRQ_C_REG rc=%d\n", ret);
		return ret;
	}

	ret = smb349_read_reg(smb349_chg->client, IRQ_D_REG, &val[3]);
	if (ret < 0) {
		pr_err("Failed to read IRQ_D_REG rc=%d\n", ret);
		return ret;
	}

	ret = smb349_read_reg(smb349_chg->client, IRQ_E_REG, &val[4]);
	if (ret < 0) {
		pr_err("Failed to read IRQ_E_REG rc=%d\n", ret);
		return ret;
	}

	ret = smb349_read_reg(smb349_chg->client, IRQ_F_REG, &val[5]);
	if (ret < 0) {
		pr_err("Failed to read IRQ_F_REG rc=%d\n", ret);
		return ret;
	}

	pr_debug("irq status: A[0x%02x], B[0x%02x], C[0x%02x], "\
			"D[0x%02x], E[0x%02x], F[0x%02x]\n",
			val[0], val[1], val[2], val[3], val[4], val[5]);
	return 0;
}

static void smb349_irq_worker(struct work_struct *work)
{
	struct smb349_struct *smb349_chg =
		container_of(work, struct smb349_struct, irq_work.work);
	union power_supply_propval ret = {0,};
	int rc = 0;
	bool ext_pwr;
	int wlc_pwr = 0;

	rc = smb349_clear_irq(smb349_chg);
	ext_pwr = smb349_is_charger_present(smb349_chg);
	if (smb349_chg->wlc_psy) {
		smb349_chg->wlc_psy->get_property(smb349_chg->wlc_psy,
					POWER_SUPPLY_PROP_ONLINE, &ret);
		wlc_pwr = ret.intval;
		pr_info("wireless charger is = %d\n", wlc_pwr);
	}

	if ((smb349_chg->ext_pwr ^ ext_pwr) && !wlc_pwr) {
		wake_lock_timeout(&smb349_chg->uevent_wake_lock, HZ*2);
		smb349_chg->ext_pwr = ext_pwr;
		pr_info("notify vbus to usb otg ext_pwr = %d\n", ext_pwr);
		power_supply_set_present(smb349_chg->usb_psy,
			smb349_chg->ext_pwr);
	}
	power_supply_changed(&smb349_chg->ac_psy);
}

/*
 * The STAT pin is low when charging and high when not charging.
 * Interrupt is triggered on both rising or falling edge.
 */
static irqreturn_t smb349_irq(int irq, void *dev_id)
{
	struct smb349_struct *smb349_chg = dev_id;

	schedule_delayed_work(&smb349_chg->irq_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

#define SMB349_FAST_CHG_MIN_MA	1000
#define SMB349_FAST_CHG_STEP_MA	200
#define SMB349_FAST_CHG_MAX_MA	4000
#define SMB349_FAST_CHG_SHIFT	4
static int smb349_chg_current_set(struct smb349_struct *smb349_chg)
{
	u8 temp;

	if ((smb349_chg->chg_current_ma < SMB349_FAST_CHG_MIN_MA) ||
		(smb349_chg->chg_current_ma >  SMB349_FAST_CHG_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", smb349_chg->chg_current_ma);
		return -EINVAL;
	}

	temp = (smb349_chg->chg_current_ma - SMB349_FAST_CHG_MIN_MA)
			/ SMB349_FAST_CHG_STEP_MA;

	temp = temp << SMB349_FAST_CHG_SHIFT;
	pr_debug("fast chg limit = %d setting = %02x\n",
				smb349_chg->chg_current_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
			FAST_CHG_CURRENT_MASK, temp);
}

#define SMB349_TERM_CURR_MIN_MA		100
#define SMB349_TERM_CURR_STEP_MA	100
#define SMB349_TERM_CURR_MAX_MA		700
#define SMB349_TERM_CURR_SHIFT		2
static int smb349_term_current_set(struct smb349_struct *smb349_chg)
{
	u8 temp;

	if ((smb349_chg->term_current_ma < SMB349_TERM_CURR_MIN_MA) ||
		(smb349_chg->term_current_ma >  SMB349_TERM_CURR_MAX_MA)) {
		pr_err("bad mA=%d asked to set\n", smb349_chg->term_current_ma);
		return -EINVAL;
	}

	temp = (smb349_chg->term_current_ma - SMB349_TERM_CURR_MIN_MA)
			/ SMB349_TERM_CURR_STEP_MA;

	temp -= 1;
	if (temp == 255)
		temp = 6;

	temp = temp << SMB349_TERM_CURR_SHIFT;
	pr_debug("chg termination current = %d setting = %02x\n",
				smb349_chg->term_current_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_OTHER_CURRENT_REG,
			TERMINATION_CURRENT_MASK, temp);
}

/*
 * val must be EN_PIN_CTRL_MASK or 0.
 * EN_PIN_CTRL_MASK - pin control, active low
 * 0 - I2C control, active high
 */
static int smb349_set_pin_control(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("smb349_set_pin_control, val = %d\n", val);

	if (val != EN_PIN_CTRL_MASK)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, PIN_ENABLE_CTRL_REG,
						EN_PIN_CTRL_MASK, val);
	if (ret) {
		pr_err("failed to set EN_PIN_CTRL_MASK rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * val must be USB_CS_BIT or 0.
 * USB_CS_BIT - pin control
 * 0 - register control
 */
static int smb349_set_usbcs_control(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("smb349_set_usbcs_control, val = %d\n", val);

	if (val != USB_CS_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, PIN_ENABLE_CTRL_REG,
						USB_CS_BIT, val);
	if (ret) {
		pr_err("failed to set USB_CS_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_set_float_voltage(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_debug("smb349_set_float_voltage, val = %d\n", val);

	if (val < 0x0 || val > 0x3f) {
		pr_err("invalid setting value, val = %d\n", val);
		return -EINVAL;
	}

	ret = smb349_masked_write(smb349_chg->client, FLOAT_VOLTAGE_REG,
						FLOAT_VOLTAGE_MASK, val);
	if (ret) {
		pr_err("failed to set FLOAT_VOLTAGE_REG rc=%d\n", ret);
		return ret;
	}

	return 0;
}

#define SMB349_USB_5_1_MODE_SHIFT	1
static int smb349_set_usb_5_1_mode(struct smb349_struct *smb349_chg, u8 usb5)
{
	int ret;

	pr_debug("smb349_set_usb_5_1_mode, usb5 = %d\n", usb5);

	usb5 = usb5 << SMB349_USB_5_1_MODE_SHIFT;

	ret = smb349_masked_write(smb349_chg->client, CMD_B_REG,
				USB_5_1_MODE_BIT, usb5);
	if (ret) {
		pr_err("failed to set USB_5_1_MODE_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_set_irq(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_masked_write(smb349_chg->client, STATUS_IRQ_REG,
				INOK_BIT | MISSING_BATT_BIT,
				INOK_BIT | MISSING_BATT_BIT);

	if (ret) {
		pr_err("failed to set INOK | MISSING_BATT rc = %d\n", ret);
		return ret;
	}

	return 0;
}

static int set_reg(void *data, u64 val)
{
	int addr = (int)data;
	int ret;
	u8 temp;

	if (!the_smb349_chg) {
		pr_err("the_smb349_chg is not initialized\n");
		return -EAGAIN;
	}

	temp = (u16) val;
	ret = smb349_write_reg(the_smb349_chg->client, addr, temp);

	if (ret) {
		pr_err("smb349_write_reg to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}
	return 0;
}

static int get_reg(void *data, u64 *val)
{
	int addr = (int)data;
	int ret;
	u8 temp = 0;

	if (!the_smb349_chg) {
		pr_err("the_smb349_chg is not initialized\n");
		return -EAGAIN;
	}

	ret = smb349_read_reg(the_smb349_chg->client, addr, &temp);
	if (ret) {
		pr_err("smb349_read_reg to %x value = %d errored = %d\n",
			addr, temp, ret);
		return -EAGAIN;
	}

	*val = temp;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

/*
 * mode must be USB_HC_MODE_BIT or 0.
 * USB_HC_MODE_BIT - high current mode.
 * 0 - usb mode.
 */
static int smb349_usb_hc_mode(struct smb349_struct *smb349_chg, u8 mode)
{
	int ret;

	pr_debug("smb349_usb_hc_mode mode = %d\n", mode);

	if (mode != USB_HC_MODE_BIT)
		mode = 0;

	ret = smb349_masked_write(smb349_chg->client, CMD_B_REG,
				USB_HC_MODE_BIT, mode);
	if (ret) {
		pr_err("failed to set USB_HC_MODE_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * FAST_CHARGE_SET_BIT - Allow fast-charge current settings.
 * 0 - Force pre-charge current settings.
 */
static int smb349_set_fast_charge(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	if (val != FAST_CHARGE_SET_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
				FAST_CHARGE_SET_BIT, val);
	if (ret) {
		pr_err("failed to set FAST_CHARGE_SET_BIT rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int
smb349_enbable_fast_chg_thresh(struct smb349_struct *smb349_chg, u8 val)
{
	int ret;

	pr_info("val = %d\n", val);

	if (val != PRE_CHG_TO_FAST_CHG_THRESH_BIT)
		val = 0;

	ret = smb349_masked_write(smb349_chg->client, SYSOK_USB3_SELECT_REG,
				PRE_CHG_TO_FAST_CHG_THRESH_BIT, val);
	if (ret) {
		pr_err("failed to set PRE_CHG_TO_FAST_CHG_THRESH_BIT\n");
		return ret;
	}

	return 0;
}

struct input_current_ma_limit_entry {
	int	icl_ma;
	u8	value;
};

static struct input_current_ma_limit_entry icl_ma_table[] = {
	{500, 0x0},
	{900, 0x1},
	{1000, 0x2},
	{1100, 0x3},
	{1200, 0x4},
	{1300, 0x5},
	{1500, 0x6},
	{1600, 0x7},
	{1700, 0x8},
	{1800, 0x9},
	{2000, 0xA},
	{2200, 0xB},
	{2400, 0xC},
	{2500, 0xD},
	{3000, 0xE},
	{3500, 0xF},
};

#define SMB349_INPUT_CURRENT_LIMIT_MIN_MA	500
#define SMB349_INPUT_CURRENT_LIMIT_MAX_MA	3500
static int
smb349_input_current_limit_set(struct smb349_struct *smb349_chg, int icl_ma)
{
	int i;
	u8 temp;

	if ((icl_ma < SMB349_INPUT_CURRENT_LIMIT_MIN_MA) ||
		(icl_ma >  SMB349_INPUT_CURRENT_LIMIT_MAX_MA)) {
		pr_err("bad mA = %d asked to set\n", icl_ma);
		return -EINVAL;
	}

	for (i = ARRAY_SIZE(icl_ma_table) - 1; i >= 0; i--) {
		if (icl_ma_table[i].icl_ma == icl_ma)
			break;
	}

	if (i < 0) {
		pr_err("can't find %d in icl_ma_table. Use min.\n", icl_ma);
		i = 0;
	}

	temp = icl_ma_table[i].value;

	pr_debug("input current limit=%d setting %02x\n", icl_ma, temp);
	return smb349_masked_write(smb349_chg->client, CHG_CURRENT_REG,
			AC_INPUT_CURRENT_LIMIT_MASK, temp);
}

#define HC_INPUT_CURR_LIMIT_DEFAULT 1800
#define HC_INPUT_CURR_LIMIT_FACTORY 1500

static int smb349_chg_is_otg_active(struct smb349_struct *smb349_chg)
{
	u8 val;
	int ret;

	ret = smb349_read_reg(smb349_chg->client, CMD_A_REG, &val);
	if (ret) {
		pr_err("failed to read CMD_A_REG = %d\n", ret);
		return 0;
	}

	if (val & USB_OTG_EN_BIT) {
		ret = smb349_read_reg(smb349_chg->client, IRQ_F_REG, &val);
		if (ret) {
			pr_err("failed to read IRQ_F_REG, rc=%d\n", ret);
			return 0;
		}

		if ((val & OTG_OC_LIMIT_STATUS_BIT) ||
			(val & OTG_BATT_UV_STATUS_BIT)) {
			pr_debug("IRQ Status F = 0x%x.\n", val);
			return 0;
		}

		return 1;
	}

	pr_info("usb otg disabled.\n");
	return 0;
}

static int smb349_switch_usb_to_charge_mode(struct smb349_struct *smb349_chg)
{
	int ret;

	pr_info("switch to charge mode\n");

	/* enable usb otg */
	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
						USB_OTG_EN_BIT, 0);
	if (ret) {
		pr_err("failed to turn on usb otg = %d\n", ret);
		return ret;
	}

	smb349_enable_charging(smb349_chg, true);

	if (smb349_chg->otg_en_gpio)
		gpio_set_value(smb349_chg->otg_en_gpio, 0);

	return 0;
}

static int smb349_switch_usb_to_host_mode(struct smb349_struct *smb349_chg)
{
	int ret;

	pr_info("switch to host mode\n");

	smb349_enable_charging(smb349_chg, false);

	/* force usb otg */
	ret = smb349_masked_write(smb349_chg->client, CMD_A_REG,
					USB_OTG_EN_BIT, USB_OTG_EN_BIT);
	if (ret) {
		pr_err("failed to turn off usb otg = %d\n", ret);
		return ret;
	}

	if (smb349_chg->otg_en_gpio)
		gpio_set_value(smb349_chg->otg_en_gpio, 1);

	return 0;
}

static void pm_external_power_changed(struct power_supply *psy)
{
	struct smb349_struct *smb349_chg = container_of(psy,
					struct smb349_struct, ac_psy);
	union power_supply_propval ret = {0,};
	int wlc_online = 0;
	int wlc_chg_current_ma = 0;

	smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
			  POWER_SUPPLY_PROP_ONLINE, &ret);
	smb349_chg->usb_online = ret.intval;

	if(smb349_chg->wlc_support) {
		smb349_chg->wlc_psy->get_property(smb349_chg->wlc_psy,
				  POWER_SUPPLY_PROP_ONLINE, &ret);
		wlc_online = ret.intval;

		smb349_chg->wlc_psy->get_property(smb349_chg->wlc_psy,
				  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
		wlc_chg_current_ma = ret.intval / 1000;
		pr_info(" wlc_online = %d current = %d\n", wlc_online,
						wlc_chg_current_ma);
	}

	if (smb349_chg->usb_online &&
			smb349_is_charger_present(smb349_chg)) {
		smb349_usb_hc_mode(smb349_chg, 0);
		smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
				  POWER_SUPPLY_PROP_CURRENT_MAX, &ret);
		smb349_chg->chg_current_ma = ret.intval / 1000;
		smb349_input_current_limit_set(smb349_chg,
					smb349_chg->chg_current_ma);

		pr_info("usb is online! i_limit = %d\n",
				smb349_chg->chg_current_ma);
	} else if (smb349_chg->ac_online &&
			smb349_is_charger_present(smb349_chg)) {
		smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		smb349_set_fast_charge(smb349_chg, FAST_CHARGE_SET_BIT);
		smb349_input_current_limit_set(smb349_chg,
					HC_INPUT_CURR_LIMIT_DEFAULT);
		smb349_chg_current_set(smb349_chg);

		pr_info("ac is online! i_limit = %d\n",
				smb349_chg->chg_current_ma);
	} else if (wlc_online && smb349_is_charger_present(smb349_chg)) {
		smb349_usb_hc_mode(smb349_chg, USB_HC_MODE_BIT);
		smb349_set_fast_charge(smb349_chg, FAST_CHARGE_SET_BIT);
		smb349_input_current_limit_set(smb349_chg,
					wlc_chg_current_ma);
		smb349_chg_current_set(smb349_chg);
		pr_info("wlc is online! i_limit = %d\n", wlc_chg_current_ma);
	}

	smb349_chg->usb_psy->get_property(smb349_chg->usb_psy,
			  POWER_SUPPLY_PROP_SCOPE, &ret);
	if (ret.intval) {
		pr_info("usb host mode = %d\n", ret.intval);
		if ((ret.intval == POWER_SUPPLY_SCOPE_SYSTEM)
				&& !smb349_chg_is_otg_active(smb349_chg))
			smb349_switch_usb_to_host_mode(smb349_chg);
		else if ((ret.intval == POWER_SUPPLY_SCOPE_DEVICE)
				&& smb349_chg_is_otg_active(smb349_chg))
			smb349_switch_usb_to_charge_mode(smb349_chg);
	}

	power_supply_changed(&smb349_chg->ac_psy);
}

static int smb349_get_prop_charge_type(struct smb349_struct *smb349_chg)
{
	u8 status_c;
	enum smb349_chg_status status;
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	bool chg_enabled;
	bool charger_err;
	int ret;

	ret = smb349_read_reg(smb349_chg->client, STATUS_C_REG, &status_c);
	if (ret) {
		pr_err("failed to read STATUS_C_REG.\n");
		chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		goto exception_handling;
	}

	chg_enabled = (bool) (status_c & 0x01);
	charger_err = (bool) (status_c & (1<<6));

	if (!chg_enabled) {
		pr_debug("charging not enabled.\n");
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		goto exception_handling;
	}

	if (charger_err) {
		pr_err("charger error detected.\n");
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		goto exception_handling;
	}

	status = (status_c >> 1) & 0x3;

	switch (status) {
	case SMB_CHG_STATUS_NONE:
		chg_type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case SMB_CHG_STATUS_FAST_CHARGE:
	case SMB_CHG_STATUS_TAPER_CHARGE:
		chg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case SMB_CHG_STATUS_PRE_CHARGE:
		chg_type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
	default:
		break;
	}

	pr_info("charge status = %s\n", smb349_chg_status[status]);

	if (smb349_chg->chg_status != status) {
		if (status == SMB_CHG_STATUS_NONE) {
			pr_info("charging stopped.\n");
			wake_unlock(&smb349_chg->chg_wake_lock);
		} else {
			pr_info("charging started.\n");
			wake_lock(&smb349_chg->chg_wake_lock);
		}
		smb349_chg->chg_status = status;
	}

	return chg_type;

exception_handling:
	smb349_chg->chg_status = SMB_CHG_STATUS_EXCEPTION;
	if (wake_lock_active(&smb349_chg->chg_wake_lock)) {
		pr_err("exception_handling : unlock chg_wake_lock.\n");
		wake_unlock(&smb349_chg->chg_wake_lock);
	}
	return chg_type;
}

#define FAIL_DEFAULT_SOC 50
static int smb349_get_soc_from_batt_psy(struct smb349_struct *smb349_chg)
{
	union power_supply_propval ret = {0,};
	int soc = 0;

	if (!smb349_chg->batt_psy)
		smb349_chg->batt_psy = power_supply_get_by_name("battery");

	if (smb349_chg->batt_psy) {
		smb349_chg->batt_psy->get_property(smb349_chg->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		soc = ret.intval;
	} else {
		pr_warn("battery power supply is not registered yet\n");
		soc = FAIL_DEFAULT_SOC;
	}

	return soc;
}

static int smb349_get_prop_chg_status(struct smb349_struct *smb349_chg)
{
	int chg_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	int soc = 0;

	soc = smb349_get_soc_from_batt_psy(smb349_chg);
	chg_type = smb349_get_prop_charge_type(smb349_chg);

	switch (chg_type) {
	case POWER_SUPPLY_CHARGE_TYPE_NONE:
		if (smb349_is_charger_present(smb349_chg)) {
			if (soc >= 100)
				chg_status = POWER_SUPPLY_STATUS_FULL;
			else
				chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	case POWER_SUPPLY_CHARGE_TYPE_TRICKLE:
	case POWER_SUPPLY_CHARGE_TYPE_FAST:
		chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		break;
	}

	pr_debug("chg status = %d soc = %d\n", chg_status, soc);
	return chg_status;
}


static enum power_supply_property pm_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct,
						ac_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb349_get_prop_chg_status(smb349_chg);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_UNKNOWN; //FIXME
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = smb349_chg->chg_current_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb349_chg->ac_online;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = smb349_get_prop_charge_type(smb349_chg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int pm_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct smb349_struct *smb349_chg = container_of(psy,
						struct smb349_struct,
						ac_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		smb349_chg->ac_online = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		smb349_chg->chg_current_ma = val->intval / 1000;
		break;
	default:
		return -EINVAL;
	}
	power_supply_changed(&smb349_chg->ac_psy);
	return 0;
}

static int smb349_create_debugfs_entries(struct smb349_struct *smb349_chg)
{
	int i;

	smb349_chg->dent = debugfs_create_dir(SMB349_NAME, NULL);
	if (IS_ERR(smb349_chg->dent)) {
		pr_err("smb349 driver couldn't create debugfs dir\n");
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(smb349_debug_regs) ; i++) {
		char *name = smb349_debug_regs[i].name;
		u32 reg = smb349_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, smb349_chg->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("debugfs_create_file %s failed.\n", name);
			return -EFAULT;
		}
	}

	return 0;
}

static void remove_debugfs_entries(struct smb349_struct *smb349_chg)
{
	if (smb349_chg->dent)
		debugfs_remove_recursive(smb349_chg->dent);
}

static int smb349_hwinit(struct smb349_struct *smb349_chg)
{
	int ret;

	ret = smb349_write_reg(smb349_chg->client, CMD_A_REG,
				VOLATILE_W_PERM_BIT);
	if (ret) {
		pr_err("failed to set VOLATILE_W_PERM_BIT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_masked_write(smb349_chg->client, CHG_CTRL_REG,
				CURR_TERM_END_CHG_BIT|AUTO_RECHG_DIS_BIT,
				AUTO_RECHG_DIS_BIT);
	if (ret) {
		pr_err("failed to set CURR_TERM_END_CHG_BIT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_chg_current_set(smb349_chg);
	if (ret) {
		pr_err("failed to set FAST_CHG_CURRENT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_term_current_set(smb349_chg);
	if (ret) {
		pr_err("failed to set TERM_CURRENT rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_usb_5_1_mode(smb349_chg, 1);
	if (ret) {
		pr_err("failed to set USB_5_1_MODE rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_irq(smb349_chg);
	if (ret) {
		pr_err("failed to set smb349_set_irq rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_pin_control(smb349_chg, 0);
	if (ret) {
		pr_err("failed to set pin control rc=%d\n", ret);
		return ret;
	}

	ret = smb349_set_usbcs_control(smb349_chg, 0);
	if (ret) {
		pr_err("failed to set usbcs control rc=%d\n", ret);
		return ret;
	}

	/*FIXME - get from dt */
	/* Set Floating Voltage to 4.35v */
	ret = smb349_set_float_voltage(smb349_chg, 0x2d);
	if (ret) {
		pr_err("failed to set floating voltage rc=%d\n", ret);
		return ret;
	}

	ret = smb349_enbable_fast_chg_thresh(smb349_chg, 0);
	if (ret) {
		pr_err("failed to set floating voltage rc=%d\n", ret);
		return ret;
	}

	ret = smb349_masked_write(smb349_chg->client, THERM_CTRL_A_REG,
				BOOST_BACK_PREVENTION_BIT, 0);
	if (ret) {
		pr_err("failed to set boost_back_prevnetion bit rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_init_ac_psy(struct smb349_struct *smb349_chg)
{
	int ret;

	smb349_chg->ac_psy.name = "ac";
	smb349_chg->ac_psy.type = POWER_SUPPLY_TYPE_MAINS;
	smb349_chg->ac_psy.supplied_to = pm_power_supplied_to;
	smb349_chg->ac_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to);
	smb349_chg->ac_psy.properties = pm_power_props;
	smb349_chg->ac_psy.num_properties = ARRAY_SIZE(pm_power_props);
	smb349_chg->ac_psy.get_property = pm_power_get_property;
	smb349_chg->ac_psy.set_property = pm_power_set_property;
	smb349_chg->ac_psy.external_power_changed =
				pm_external_power_changed;

	ret = power_supply_register(&smb349_chg->client->dev,
				&smb349_chg->ac_psy);
	if (ret) {
		pr_err("failed to register power_supply. ret=%d.\n", ret);
		return ret;
	}

	return 0;
}

static int smb349_parse_dt(struct device_node *dev_node,
			   struct smb349_struct *smb349_chg)
{
	int ret = 0;

	smb349_chg->stat_gpio =
		of_get_named_gpio(dev_node, "summit,stat-gpio", 0);
	if (smb349_chg->stat_gpio < 0) {
		pr_err("failed to get stat-gpio.\n");
		ret = smb349_chg->stat_gpio;
		goto out;
	}

	ret = of_property_read_u32(dev_node, "summit,chg-current-ma",
				   &(smb349_chg->chg_current_ma));
	if (ret) {
		pr_err("failed to get chg_current_ma.\n");
		goto out;
	}

	ret = of_property_read_u32(dev_node, "summit,term-current-ma",
				   &(smb349_chg->term_current_ma));
	if (ret) {
		pr_err("failed to get term_current_ma.\n");
		goto  out;
	}

	smb349_chg->wlc_support =
			of_property_read_bool(dev_node,
					"summit,wlc-support");

	smb349_chg->ext_ovp_otg_ctrl =
			of_property_read_bool(dev_node,
					"summit,ext-ovp-otg-ctrl");
	if (smb349_chg->ext_ovp_otg_ctrl) {
		smb349_chg->otg_en_gpio =
			of_get_named_gpio(dev_node, "summit,otg-en-gpio", 0);
		if (smb349_chg->otg_en_gpio < 0) {
			pr_err("Unable to get named gpio for otg_en_gpio.\n");
			return smb349_chg->otg_en_gpio;
		}
	}

	pr_info("chg_i_ma = %d term_i_ma = %d\n",
			smb349_chg->chg_current_ma,
			smb349_chg->term_current_ma);
out:
	return ret;
}

static int smb349_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	const struct smb349_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct smb349_struct *smb349_chg;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("smb349 i2c func fail.\n");
		return -EIO;
	}

	smb349_chg = kzalloc(sizeof(*smb349_chg), GFP_KERNEL);
	if (!smb349_chg) {
		pr_err("smb349 alloc fail.\n");
		return -ENOMEM;
	}

	smb349_chg->client = client;

	smb349_chg->usb_psy = power_supply_get_by_name("usb");
	if (!smb349_chg->usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		ret = -EPROBE_DEFER;
		goto err_stat_gpio;
	}

	if (dev_node) {
		ret = smb349_parse_dt(dev_node, smb349_chg);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto err_stat_gpio;
		}

	} else {
		pdata = client->dev.platform_data;

		if (pdata == NULL) {
			pr_err("smb349 no platform data.\n");
			goto err_stat_gpio;
		}

		smb349_chg->stat_gpio = pdata->stat_gpio;
		smb349_chg->chg_current_ma = pdata->chg_current_ma;
		smb349_chg->term_current_ma = pdata->term_current_ma;
		smb349_chg->wlc_support = pdata->wlc_support;
		smb349_chg->ext_ovp_otg_ctrl = pdata->ext_ovp_otg_ctrl;
		if (smb349_chg->ext_ovp_otg_ctrl)
			smb349_chg->otg_en_gpio = pdata->otg_en_gpio;

	}

	if (smb349_chg->wlc_support) {
		smb349_chg->wlc_psy = power_supply_get_by_name("wireless");
		if (!smb349_chg->wlc_psy) {
			pr_err("wireless supply not found deferring probe\n");
			ret = -EPROBE_DEFER;
			goto err_stat_gpio;
		}
	}

	if (smb349_chg->stat_gpio) {
		ret = gpio_request(smb349_chg->stat_gpio, "smb349_stat");
		if (ret) {
			pr_err("stat_gpio gpio_request failed for %d ret=%d\n",
			       smb349_chg->stat_gpio, ret);
			goto err_stat_gpio;
		}
		smb349_chg->irq = gpio_to_irq(smb349_chg->stat_gpio);
	}

	if (smb349_chg->otg_en_gpio) {
		ret = gpio_request_one(smb349_chg->otg_en_gpio,
				GPIOF_OUT_INIT_LOW, "otg_en");
		if (ret) {
			pr_err("otg_en_gpio request failed for %d ret=%d\n",
					smb349_chg->otg_en_gpio, ret);
			goto err_otg_en_gpio;
		}
	}

	i2c_set_clientdata(client, smb349_chg);

	ret = smb349_hwinit(smb349_chg);
	if (ret) {
		pr_err("smb349_hwinit failed. ret=%d\n", ret);
		goto err_hw_init;
	}

	wake_lock_init(&smb349_chg->chg_wake_lock,
		       WAKE_LOCK_SUSPEND, SMB349_NAME);
	wake_lock_init(&smb349_chg->uevent_wake_lock,
		       WAKE_LOCK_SUSPEND, "smb349_chg_uevent");

	ret = smb349_init_ac_psy(smb349_chg);
	if (ret) {
		pr_err("smb349_init_ac_psy failed. ret=%d\n", ret);
		goto err_reg_psy;
	}

	the_smb349_chg = smb349_chg;

	ret = smb349_create_debugfs_entries(smb349_chg);
	if (ret) {
		pr_err("smb349_create_debugfs_entries failed.ret=%d\n", ret);
		goto err_debugfs;
	}

	INIT_DELAYED_WORK(&smb349_chg->irq_work, smb349_irq_worker);

	if (smb349_chg->irq) {
		ret = request_irq(smb349_chg->irq, smb349_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"smb349_irq", smb349_chg);
		if (ret) {
			pr_err("request_irq %d failed.ret=%d\n", smb349_chg->irq, ret);
			goto err_irq;
		}

		enable_irq_wake(smb349_chg->irq);
	}

	smb349_enable_charging(smb349_chg, true);

	pr_info("probe done\n");
	return 0;

err_irq:
err_debugfs:
	power_supply_unregister(&smb349_chg->ac_psy);
	remove_debugfs_entries(smb349_chg);
err_reg_psy:
err_hw_init:
	if (smb349_chg->otg_en_gpio)
		   gpio_free(smb349_chg->otg_en_gpio);
err_otg_en_gpio:
	if (smb349_chg->stat_gpio)
		gpio_free(smb349_chg->stat_gpio);
err_stat_gpio:
	kfree(smb349_chg);
	pr_info("fail to probe\n");
	return ret;
}

static int smb349_remove(struct i2c_client *client)
{
	struct smb349_struct *smb349_chg = i2c_get_clientdata(client);

	power_supply_unregister(&smb349_chg->ac_psy);
	if (smb349_chg->stat_gpio)
		gpio_free(smb349_chg->stat_gpio);
	if (smb349_chg->otg_en_gpio)
		gpio_free(smb349_chg->otg_en_gpio);
	if (smb349_chg->irq)
		free_irq(smb349_chg->irq, smb349_chg);
	remove_debugfs_entries(smb349_chg);

	wake_lock_destroy(&smb349_chg->uevent_wake_lock);

	kfree(smb349_chg);
	return 0;
}

static const struct i2c_device_id smb349_id[] = {
	{SMB349_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, smb349_id);

static const struct of_device_id smb349_match[] = {
	{ .compatible = "summit,smb349-charger", },
	{ },
};

static struct i2c_driver smb349_driver = {
	.driver	= {
		   .name	= SMB349_NAME,
		   .owner	= THIS_MODULE,
		   .of_match_table = of_match_ptr(smb349_match),
	},
	.probe		= smb349_probe,
	.remove		= smb349_remove,
	.id_table	= smb349_id,
};

static int __init smb349_init(void)
{
	return i2c_add_driver(&smb349_driver);
}
module_init(smb349_init);

static void __exit smb349_exit(void)
{
	return i2c_del_driver(&smb349_driver);
}
module_exit(smb349_exit);

MODULE_DESCRIPTION("Driver for SMB349 charger chip");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:" SMB349_NAME);
