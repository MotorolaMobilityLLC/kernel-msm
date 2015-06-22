/* Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "SMBCHG: %s: " fmt, __func__

#include <linux/spmi.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/spmi.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/batterydata-lib.h>

/* Mask/Bit helpers */
#define _SMB_MASK(BITS, POS) \
	((unsigned char)(((1 << (BITS)) - 1) << (POS)))
#define SMB_MASK(LEFT_BIT_POS, RIGHT_BIT_POS) \
		_SMB_MASK((LEFT_BIT_POS) - (RIGHT_BIT_POS) + 1, \
				(RIGHT_BIT_POS))
/* Config registers */
struct smbchg_regulator {
	struct regulator_desc	rdesc;
	struct regulator_dev	*rdev;
};

struct parallel_usb_cfg {
	struct power_supply		*psy;
	int				min_current_thr_ma;
	int				min_9v_current_thr_ma;
	int				allowed_lowering_ma;
	int				current_max_ma;
	bool				avail;
	struct mutex			lock;
	int				initial_aicl_ma;
};

struct ilim_entry {
	int vmin_uv;
	int vmax_uv;
	int icl_pt_ma;
	int icl_lv_ma;
	int icl_hv_ma;
};

struct ilim_map {
	int			num;
	struct ilim_entry	*entries;
};

enum stepchg_state {
	STEP_MAX,
	STEP_ONE,
	STEP_TAPER,
	STEP_FULL,
	STEP_NONE = 0xFF,
};

struct pchg_current_map {
	int requested;
	int primary;
	int secondary;
};

struct smbchg_chip {
	struct device			*dev;
	struct spmi_device		*spmi;

	/* peripheral register address bases */
	u16				chgr_base;
	u16				bat_if_base;
	u16				usb_chgpth_base;
	u16				dc_chgpth_base;
	u16				otg_base;
	u16				misc_base;

	bool				test_mode;
	int				test_mode_soc;
	int				test_mode_temp;
	u8				revision[4];

	/* configuration parameters */
	int				iterm_ma;
	int				usb_max_current_ma;
	int				dc_max_current_ma;
	int				usb_target_current_ma;
	int				usb_tl_current_ma;
	int				dc_target_current_ma;
	int				prev_target_fastchg_current_ma;
	int				target_fastchg_current_ma;
	int				allowed_fastchg_current_ma;
	bool				update_allowed_fastchg_current_ma;
	int				fastchg_current_ma;
	unsigned int			pchg_current_map_len;
	struct pchg_current_map        *pchg_current_map_data;
	int				vfloat_mv;
	int				vfloat_parallel_mv;
	int				fastchg_current_comp;
	int				float_voltage_comp;
	int				resume_delta_mv;
	int				safety_time;
	int				prechg_safety_time;
	int				bmd_pin_src;
	int				jeita_temp_hard_limit;
	bool				use_vfloat_adjustments;
	bool				iterm_disabled;
	bool				bmd_algo_disabled;
	bool				soft_vfloat_comp_disabled;
	bool				chg_enabled;
	bool				low_icl_wa_on;
	bool				battery_unknown;
	bool				charge_unknown_battery;
	bool				chg_inhibit_en;
	bool				chg_inhibit_source_fg;
	bool				low_volt_dcin;
	bool				vbat_above_headroom;
	bool				force_aicl_rerun;
	u8				original_usbin_allowance;
	struct parallel_usb_cfg		parallel;
	struct delayed_work		parallel_en_work;
	struct dentry			*debug_root;

	/* wipower params */
	struct ilim_map			wipower_default;
	struct ilim_map			wipower_pt;
	struct ilim_map			wipower_div2;
	struct qpnp_vadc_chip		*vadc_dev;
	bool				wipower_dyn_icl_avail;
	struct ilim_entry		current_ilim;
	struct mutex			wipower_config;
	bool				wipower_configured;
	struct qpnp_adc_tm_btm_param	param;

	/* flash current prediction */
	int				rpara_uohm;
	int				rslow_uohm;

	/* vfloat adjustment */
	int				max_vbat_sample;
	int				n_vbat_samples;

	/* status variables */
	int				battchg_disabled;
	int				usb_suspended;
	int				dc_suspended;
	int				wake_reasons;
	int				previous_soc;
	int				usb_online;
	bool				dc_present;
	bool				usb_present;
	bool				batt_present;
	int				otg_retries;
	ktime_t				otg_enable_time;
	bool				aicl_deglitch_short;
	bool				sw_esr_pulse_en;
	bool				safety_timer_en;
	bool				aicl_complete;
	bool				usb_ov_det;

	/* jeita and temperature */
	bool				batt_hot;
	bool				batt_cold;
	bool				batt_warm;
	bool				batt_cool;
	unsigned int			chg_thermal_levels;
	unsigned int			chg_therm_lvl_sel;
	unsigned int			*chg_thermal_mitigation;

	unsigned int			thermal_levels;
	unsigned int			therm_lvl_sel;
	unsigned int			*thermal_mitigation;

	unsigned int			dc_thermal_levels;
	unsigned int			dc_therm_lvl_sel;
	unsigned int			*dc_thermal_mitigation;

	/* irqs */
	int				batt_hot_irq;
	int				batt_warm_irq;
	int				batt_cool_irq;
	int				batt_cold_irq;
	int				batt_missing_irq;
	int				vbat_low_irq;
	int				chg_hot_irq;
	int				chg_term_irq;
	int				taper_irq;
	bool				taper_irq_enabled;
	struct mutex			taper_irq_lock;
	int				recharge_irq;
	int				fastchg_irq;
	int				safety_timeout_irq;
#ifdef QCOM_BASE
	int				power_ok_irq;
#endif
	int				dcin_uv_irq;
	int				usbin_uv_irq;
	int				usbin_ov_irq;
	int				src_detect_irq;
	int				otg_fail_irq;
	int				otg_oc_irq;
	int				aicl_done_irq;
	int				usbid_change_irq;
	int				chg_error_irq;
	bool				enable_aicl_wake;

	/* psy */
	struct power_supply		*usb_psy;
	struct power_supply		batt_psy;
	struct power_supply		dc_psy;
	struct power_supply		*bms_psy;
	int				dc_psy_type;
	const char			*bms_psy_name;
	const char			*battery_psy_name;
	bool				psy_registered;

	struct smbchg_regulator		otg_vreg;
	struct smbchg_regulator		ext_otg_vreg;
	struct work_struct		usb_set_online_work;
	struct delayed_work		vfloat_adjust_work;
	struct delayed_work		hvdcp_det_work;
	spinlock_t			sec_access_lock;
	struct mutex			current_change_lock;
	struct mutex			usb_set_online_lock;
	struct mutex			usb_set_present_lock;
	struct mutex			dc_set_present_lock;
	struct mutex			battchg_disabled_lock;
	struct mutex			usb_en_lock;
	struct mutex			dc_en_lock;
	struct mutex			fcc_lock;
	struct mutex			pm_lock;
	/* aicl deglitch workaround */
	unsigned long			first_aicl_seconds;
	int				aicl_irq_count;
	bool				factory_mode;
	struct delayed_work		heartbeat_work;
	struct mutex			check_temp_lock;
	int				temp_state;
	int				hotspot_temp;
	int				hotspot_thrs_c;
	int				hot_temp_c;
	int				cold_temp_c;
	int				warm_temp_c;
	int				cool_temp_c;
	int				ext_high_temp;
	int				ext_temp_volt_mv;
	int				ext_temp_soc;
	int				stepchg_voltage_mv;
	int				stepchg_current_ma;
	int				stepchg_taper_ma;
	int				stepchg_iterm_ma;
	int				stepchg_max_voltage_mv;
	int				stepchg_max_current_ma;
	enum stepchg_state		stepchg_state;
	unsigned int			stepchg_state_holdoff;
	struct wakeup_source		smbchg_wake_source;
	struct delayed_work		usb_insertion_work;
	int				apsd_rerun_cnt;
	int				charger_rate;
	bool				usbid_disabled;
	bool				demo_mode;
	bool				batt_therm_wa;
	struct notifier_block		smb_reboot;
	int				aicl_wait_retries;
};

static struct smbchg_chip *the_chip;

enum print_reason {
	PR_REGISTER	= BIT(0),
	PR_INTERRUPT	= BIT(1),
	PR_STATUS	= BIT(2),
	PR_DUMP		= BIT(3),
	PR_PM		= BIT(4),
	PR_MISC		= BIT(5),
	PR_WIPOWER	= BIT(6),
};

enum wake_reason {
	PM_PARALLEL_CHECK = BIT(0),
	PM_REASON_VFLOAT_ADJUST = BIT(1),
	PM_ESR_PULSE = BIT(2),
	PM_HEARTBEAT = BIT(3),
	PM_CHARGER = BIT(4),
	PM_WIRELESS = BIT(5),
};

static void smbchg_rate_check(struct smbchg_chip *chip);
static void smbchg_set_temp_chgpath(struct smbchg_chip *chip, int prev_temp);
static int get_prop_batt_capacity(struct smbchg_chip *chip);

static int smbchg_debug_mask;
module_param_named(
	debug_mask, smbchg_debug_mask, int, S_IRUSR | S_IWUSR
);

static int smbchg_parallel_en;
module_param_named(
	parallel_en, smbchg_parallel_en, int, S_IRUSR | S_IWUSR
);

static int wipower_dyn_icl_en;
module_param_named(
	dynamic_icl_wipower_en, wipower_dyn_icl_en,
	int, S_IRUSR | S_IWUSR
);

static int wipower_dcin_interval = ADC_MEAS1_INTERVAL_2P0MS;
module_param_named(
	wipower_dcin_interval, wipower_dcin_interval,
	int, S_IRUSR | S_IWUSR
);

#define WIPOWER_DEFAULT_HYSTERISIS_UV	250000
static int wipower_dcin_hyst_uv = WIPOWER_DEFAULT_HYSTERISIS_UV;
module_param_named(
	wipower_dcin_hyst_uv, wipower_dcin_hyst_uv,
	int, S_IRUSR | S_IWUSR
);

static int smbchg_force_apsd(struct smbchg_chip *chip);

#define pr_smb(reason, fmt, ...)				\
	do {							\
		if (smbchg_debug_mask & (reason))		\
			pr_info(fmt, ##__VA_ARGS__);		\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);		\
	} while (0)

#define pr_smb_rt(reason, fmt, ...)					\
	do {								\
		if (smbchg_debug_mask & (reason))			\
			pr_info_ratelimited(fmt, ##__VA_ARGS__);	\
		else							\
			pr_debug_ratelimited(fmt, ##__VA_ARGS__);	\
	} while (0)

static int smbchg_read(struct smbchg_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		dev_err(chip->dev, "addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		dev_err(chip->dev, "spmi read failed addr=0x%02x sid=0x%02x rc=%d\n",
				addr, spmi->sid, rc);
		return rc;
	}
	return 0;
}

/*
 * Writes an arbitrary number of bytes to a specified register
 *
 * Do not use this function for register writes if possible. Instead use the
 * smbchg_masked_write function.
 *
 * The sec_access_lock must be held for all register writes and this function
 * does not do that. If this function is used, please hold the spinlock or
 * random secure access writes may fail.
 */
static int smbchg_write(struct smbchg_chip *chip, u8 *val,
			u16 addr, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		dev_err(chip->dev, "addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		dev_err(chip->dev, "write failed addr=0x%02x sid=0x%02x rc=%d\n",
			addr, spmi->sid, rc);
		return rc;
	}

	return 0;
}

/*
 * Writes a register to the specified by the base and limited by the bit mask
 *
 * Do not use this function for register writes if possible. Instead use the
 * smbchg_masked_write function.
 *
 * The sec_access_lock must be held for all register writes and this function
 * does not do that. If this function is used, please hold the spinlock or
 * random secure access writes may fail.
 */
static int smbchg_masked_write_raw(struct smbchg_chip *chip, u16 base, u8 mask,
									u8 val)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, base, 1);
	if (rc) {
		dev_err(chip->dev, "spmi read failed: addr=%03X, rc=%d\n",
				base, rc);
		return rc;
	}

	reg &= ~mask;
	reg |= val & mask;

	pr_smb(PR_REGISTER, "addr = 0x%x writing 0x%x\n", base, reg);

	rc = smbchg_write(chip, &reg, base, 1);
	if (rc) {
		dev_err(chip->dev, "spmi write failed: addr=%03X, rc=%d\n",
				base, rc);
		return rc;
	}

	return 0;
}

/*
 * Writes a register to the specified by the base and limited by the bit mask
 *
 * This function holds a spin lock to ensure secure access register writes goes
 * through. If the secure access unlock register is armed, any old register
 * write can unarm the secure access unlock, causing the next write to fail.
 *
 * Note: do not use this for sec_access registers. Instead use the function
 * below: smbchg_sec_masked_write
 */
static int smbchg_masked_write(struct smbchg_chip *chip, u16 base, u8 mask,
								u8 val)
{
	unsigned long flags;
	int rc;

	if (chip->factory_mode)
		return 0;

	spin_lock_irqsave(&chip->sec_access_lock, flags);
	rc = smbchg_masked_write_raw(chip, base, mask, val);
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);

	return rc;
}

static int smbchg_masked_write_fac(struct smbchg_chip *chip,
				   u16 base, u8 mask, u8 val)
{
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&chip->sec_access_lock, flags);
	rc = smbchg_masked_write_raw(chip, base, mask, val);
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);

	return rc;
}

/*
 * Unlocks sec access and writes to the register specified.
 *
 * This function holds a spin lock to exclude other register writes while
 * the two writes are taking place.
 */
#define SEC_ACCESS_OFFSET	0xD0
#define SEC_ACCESS_VALUE	0xA5
#define PERIPHERAL_MASK		0xFF
static int smbchg_sec_masked_write(struct smbchg_chip *chip, u16 base, u8 mask,
									u8 val)
{
	unsigned long flags;
	int rc;
	u16 peripheral_base = base & (~PERIPHERAL_MASK);

	if (chip->factory_mode)
		return 0;

	spin_lock_irqsave(&chip->sec_access_lock, flags);

	rc = smbchg_masked_write_raw(chip, peripheral_base + SEC_ACCESS_OFFSET,
				SEC_ACCESS_VALUE, SEC_ACCESS_VALUE);
	if (rc) {
		dev_err(chip->dev, "Unable to unlock sec_access: %d", rc);
		goto out;
	}

	rc = smbchg_masked_write_raw(chip, base, mask, val);

out:
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);
	return rc;
}

static int smbchg_sec_masked_write_fac(struct smbchg_chip *chip,
				       u16 base, u8 mask, u8 val)
{
	unsigned long flags;
	int rc;
	u16 peripheral_base = base & (~PERIPHERAL_MASK);

	spin_lock_irqsave(&chip->sec_access_lock, flags);

	rc = smbchg_masked_write_raw(chip, peripheral_base + SEC_ACCESS_OFFSET,
				SEC_ACCESS_VALUE, SEC_ACCESS_VALUE);
	if (rc) {
		dev_err(chip->dev, "Unable to unlock sec_access: %d", rc);
		goto out;
	}

	rc = smbchg_masked_write_raw(chip, base, mask, val);

out:
	spin_unlock_irqrestore(&chip->sec_access_lock, flags);
	return rc;
}

static void smbchg_stay_awake(struct smbchg_chip *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons | reason;
	if (reasons != 0 && chip->wake_reasons == 0) {
		pr_smb(PR_PM, "staying awake: 0x%02x (bit %d)\n",
				reasons, reason);
		__pm_stay_awake(&chip->smbchg_wake_source);
#ifdef QCOM_BASE
		pm_stay_awake(chip->dev);
#endif
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
}

static void smbchg_relax(struct smbchg_chip *chip, int reason)
{
	int reasons;

	mutex_lock(&chip->pm_lock);
	reasons = chip->wake_reasons & (~reason);
	if (reasons == 0 && chip->wake_reasons != 0) {
		pr_smb(PR_PM, "relaxing: 0x%02x (bit %d)\n",
				reasons, reason);
		__pm_relax(&chip->smbchg_wake_source);
#ifdef QCOM_BASE
		pm_relax(chip->dev);
#endif
	}
	chip->wake_reasons = reasons;
	mutex_unlock(&chip->pm_lock);
};

enum pwr_path_type {
	UNKNOWN = 0,
	PWR_PATH_BATTERY = 1,
	PWR_PATH_USB = 2,
	PWR_PATH_DC = 3,
};

#define PWR_PATH		0x08
#define PWR_PATH_MASK		0x03
static enum pwr_path_type smbchg_get_pwr_path(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + PWR_PATH, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read PWR_PATH rc = %d\n", rc);
		return PWR_PATH_BATTERY;
	}

	return reg & PWR_PATH_MASK;
}

#define RID_STS				0xB
#define RID_MASK			0xF
#define IDEV_STS			0x8
#define RT_STS				0x10
#define USBID_MSB			0xE
#define USBIN_UV_BIT			BIT(0)
#define USBIN_OV_BIT			BIT(1)
#define USBIN_SRC_DET_BIT		BIT(2)
#define FMB_STS_MASK			SMB_MASK(3, 0)
#define USBID_GND_THRESHOLD		0x495
static bool is_otg_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;
	u8 usbid_reg[2];
	u16 usbid_val;

	/*
	 * There is a problem with USBID conversions on PMI8994 revisions
	 * 2.0.0. As a workaround, check that the cable is not
	 * detected as factory test before enabling OTG.
	 */
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read IDEV_STS rc = %d\n", rc);
		return false;
	}

	if ((reg & FMB_STS_MASK) != 0) {
		pr_smb(PR_STATUS, "IDEV_STS = %02x, not ground\n", reg);
		return false;
	}

	rc = smbchg_read(chip, usbid_reg, chip->usb_chgpth_base + USBID_MSB, 2);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read USBID rc = %d\n", rc);
		return false;
	}
	usbid_val = (usbid_reg[0] << 8) | usbid_reg[1];

	if (usbid_val > USBID_GND_THRESHOLD) {
		pr_smb(PR_STATUS, "USBID = 0x%04x, too high to be ground\n",
				usbid_val);
		return false;
	}

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RID_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read usb rid status rc = %d\n", rc);
		return false;
	}

	pr_smb(PR_STATUS, "RID_STS = %02x\n", reg);

	return (reg & RID_MASK) == 0;
}

#define USBIN_9V			BIT(5)
#define USBIN_UNREG			BIT(4)
#define USBIN_LV			BIT(3)
#define DCIN_9V				BIT(2)
#define DCIN_UNREG			BIT(1)
#define DCIN_LV				BIT(0)
#define INPUT_STS			0x0D
#define DCIN_UV_BIT			BIT(0)
#define DCIN_OV_BIT			BIT(1)
static bool is_dc_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->dc_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read dc status rc = %d\n", rc);
		return false;
	}

	if ((reg & DCIN_UV_BIT) || (reg & DCIN_OV_BIT))
		return false;

	return true;
}

static bool is_usb_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb rt status rc = %d\n", rc);
		return false;
	}

	return !(reg & USBIN_UV_BIT) || !!(reg & USBIN_SRC_DET_BIT);
}

static char *usb_type_str[] = {
	"SDP",		/* bit 0 */
	"OTHER",	/* bit 1 */
	"DCP",		/* bit 2 */
	"CDP",		/* bit 3 */
	"NONE",		/* bit 4 error case */
};

#define N_TYPE_BITS		4
#define TYPE_BITS_OFFSET	4

static int get_type(u8 type_reg)
{
	unsigned long type = type_reg;
	type >>= TYPE_BITS_OFFSET;
	return find_first_bit(&type, N_TYPE_BITS);
}

/* helper to return the string of USB type */
static inline char *get_usb_type_name(int type)
{
	return usb_type_str[type];
}

static enum power_supply_type usb_type_enum[] = {
	POWER_SUPPLY_TYPE_USB,		/* bit 0 */
	POWER_SUPPLY_TYPE_UNKNOWN,	/* bit 1 */
	POWER_SUPPLY_TYPE_USB_DCP,	/* bit 2 */
	POWER_SUPPLY_TYPE_USB_CDP,	/* bit 3 */
	POWER_SUPPLY_TYPE_USB,		/* bit 4 error case, report SDP */
};

/* helper to return enum power_supply_type of USB type */
static inline enum power_supply_type get_usb_supply_type(int type)
{
	return usb_type_enum[type];
}

static enum power_supply_property smbchg_battery_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS,
	POWER_SUPPLY_PROP_FLASH_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_SAFETY_TIMER_ENABLE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
	POWER_SUPPLY_PROP_TEMP_HOTSPOT,
	POWER_SUPPLY_PROP_CHARGE_RATE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_AGE,
};

#define CHGR_STS			0x0E
#define BATT_LESS_THAN_2V		BIT(4)
#define CHG_HOLD_OFF_BIT		BIT(3)
#define CHG_TYPE_MASK			SMB_MASK(2, 1)
#define CHG_TYPE_SHIFT			1
#define BATT_NOT_CHG_VAL		0x0
#define BATT_PRE_CHG_VAL		0x1
#define BATT_FAST_CHG_VAL		0x2
#define BATT_TAPER_CHG_VAL		0x3
#define CHG_EN_BIT			BIT(0)
#define CHG_INHIBIT_BIT		BIT(1)
#define BAT_TCC_REACHED_BIT		BIT(7)
static int get_prop_batt_status(struct smbchg_chip *chip)
{
	int rc, status = POWER_SUPPLY_STATUS_DISCHARGING;
	u8 reg = 0, chg_type;
	bool charger_present, chg_inhibit;
	int batt_soc;

	batt_soc = get_prop_batt_capacity(chip);

	rc = smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read RT_STS rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if ((reg & BAT_TCC_REACHED_BIT) && !chip->demo_mode &&
	    (chip->temp_state == POWER_SUPPLY_HEALTH_GOOD))
		return POWER_SUPPLY_STATUS_FULL;

	if ((chip->stepchg_state == STEP_FULL) && !(batt_soc < 100) &&
	    !chip->demo_mode && (chip->temp_state == POWER_SUPPLY_HEALTH_GOOD))
		return POWER_SUPPLY_STATUS_FULL;

	charger_present = is_usb_present(chip) | is_dc_present(chip);
	if (!charger_present)
		return POWER_SUPPLY_STATUS_DISCHARGING;

	chg_inhibit = reg & CHG_INHIBIT_BIT;
	if (chg_inhibit)
		dev_err(chip->dev, "Charge Inhibit Set\n");

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return POWER_SUPPLY_STATUS_UNKNOWN;
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
	pr_smb_rt(PR_MISC, "CHGR_STS = 0x%02x\n", reg);
	return status;
}

#define BAT_PRES_STATUS			0x08
#define BAT_PRES_BIT			BIT(7)
static int get_prop_batt_present(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->bat_if_base + BAT_PRES_STATUS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
	}

	return !!(reg & BAT_PRES_BIT);
}

static int get_prop_charge_type(struct smbchg_chip *chip)
{
	int rc;
	u8 reg, chg_type;

	rc = smbchg_read(chip, &reg, chip->chgr_base + CHGR_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Unable to read CHGR_STS rc = %d\n", rc);
		return 0;
	}

	chg_type = (reg & CHG_TYPE_MASK) >> CHG_TYPE_SHIFT;

	if ((chg_type == BATT_TAPER_CHG_VAL) &&
	    ((chip->vfloat_mv != chip->stepchg_max_voltage_mv) ||
	     (chip->demo_mode)))
		chg_type = BATT_FAST_CHG_VAL;

	if (chg_type == BATT_NOT_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (chg_type == BATT_TAPER_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TAPER;
	else if (chg_type == BATT_FAST_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
	else if (chg_type == BATT_PRE_CHG_VAL)
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int set_property_on_fg(struct smbchg_chip *chip,
		enum power_supply_property prop, int val)
{
	int rc;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (!chip->bms_psy) {
		pr_smb(PR_STATUS, "no bms psy found\n");
		return -EINVAL;
	}

	ret.intval = val;
	rc = chip->bms_psy->set_property(chip->bms_psy, prop, &ret);
	if (rc)
		pr_smb(PR_STATUS,
			"bms psy does not allow updating prop %d rc = %d\n",
			prop, rc);

	return rc;
}

static int get_property_from_fg(struct smbchg_chip *chip,
		enum power_supply_property prop, int *val)
{
	int rc;
	union power_supply_propval ret = {0, };

	if (!chip->bms_psy && chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);
	if (!chip->bms_psy) {
		pr_smb(PR_STATUS, "no bms psy found\n");
		return -EINVAL;
	}

	rc = chip->bms_psy->get_property(chip->bms_psy, prop, &ret);
	if (rc) {
		pr_smb(PR_STATUS,
			"bms psy doesn't support reading prop %d rc = %d\n",
			prop, rc);
		return rc;
	}

	*val = ret.intval;
	return rc;
}

#define DEFAULT_BATT_CAPACITY	50
static int get_prop_batt_capacity(struct smbchg_chip *chip)
{
	int capacity, rc;

	if (chip->test_mode && !(chip->test_mode_soc < 0)
	    && !(chip->test_mode_soc > 100))
		return chip->test_mode_soc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_CAPACITY, &capacity);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get capacity rc = %d\n", rc);
		capacity = DEFAULT_BATT_CAPACITY;
	}
	return capacity;
}

#define DEFAULT_BATT_TEMP		200
#define GLITCH_BATT_TEMP		600
#define ERROR_BATT_TEMP 		597
static int get_prop_batt_temp(struct smbchg_chip *chip)
{
	int temp, rc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_TEMP, &temp);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get temperature rc = %d\n", rc);
		temp = DEFAULT_BATT_TEMP;
	}

	if ((chip->batt_therm_wa) && (temp > GLITCH_BATT_TEMP)) {
		dev_err(chip->dev, "GLITCH: Temperature Read %d \n", temp);
		temp = ERROR_BATT_TEMP;
	}

	return temp;
}

#define DEFAULT_BATT_CURRENT_NOW	0
static int get_prop_batt_current_now(struct smbchg_chip *chip)
{
	int ua, rc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_CURRENT_NOW, &ua);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get current rc = %d\n", rc);
		ua = DEFAULT_BATT_CURRENT_NOW;
	}
	return ua;
}

#define DEFAULT_CHARGE_COUNT	0
static int get_prop_charge_counter(struct smbchg_chip *chip)
{
	int uah, rc;

	rc = get_property_from_fg(chip,
				  POWER_SUPPLY_PROP_CHARGE_NOW_RAW, &uah);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get charge now raw rc = %d\n", rc);
		uah = DEFAULT_CHARGE_COUNT;
	}
	return uah;
}

#define DEFAULT_BATT_VOLTAGE_NOW	0
static int get_prop_batt_voltage_now(struct smbchg_chip *chip)
{
	int uv, rc;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_VOLTAGE_NOW, &uv);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get voltage rc = %d\n", rc);
		uv = DEFAULT_BATT_VOLTAGE_NOW;
	}
	return uv;
}

#define DEFAULT_BATT_VOLTAGE_MAX_DESIGN	4200000
static int get_prop_batt_voltage_max_design(struct smbchg_chip *chip)
{
	int uv, rc;

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &uv);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get voltage rc = %d\n", rc);
		uv = DEFAULT_BATT_VOLTAGE_MAX_DESIGN;
	}
	return uv;
}

#define DEFAULT_CHARGE_FULL	3000000
static int get_prop_charge_full(struct smbchg_chip *chip)
{
	int uah, rc;

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CHARGE_FULL, &uah);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get charge full rc = %d\n", rc);
		uah = DEFAULT_CHARGE_FULL;
	}
	return uah;
}

#define DEFAULT_CHARGE_FULL_DESIGN	3000000
static int get_prop_charge_full_design(struct smbchg_chip *chip)
{
	int uah, rc;

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &uah);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get full design rc = %d\n", rc);
		uah = DEFAULT_CHARGE_FULL_DESIGN;
	}
	return uah;
}

#define DEFAULT_CYCLE_COUNT	0
static int get_prop_cycle_count(struct smbchg_chip *chip)
{
	int count, rc;

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CYCLE_COUNT, &count);
	if (rc) {
		pr_smb(PR_STATUS, "Couldn't get cyclec count rc = %d\n", rc);
		count = DEFAULT_CYCLE_COUNT;
	}
	return count;
}

static int get_prop_batt_health(struct smbchg_chip *chip)
{
	if (chip->batt_hot)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (chip->batt_cold)
		return POWER_SUPPLY_HEALTH_COLD;
	else if (chip->batt_warm)
		return POWER_SUPPLY_HEALTH_WARM;
	else if (chip->batt_cool)
		return POWER_SUPPLY_HEALTH_COOL;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static const int usb_current_table[] = {
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

static const int dc_current_table[] = {
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

static const int fcc_comp_table[] = {
	250,
	700,
	900,
	1200,
};

static int calc_thermal_limited_current(struct smbchg_chip *chip,
						int current_ma)
{
	int therm_ma;

	if (chip->therm_lvl_sel > 0
			&& chip->therm_lvl_sel < (chip->thermal_levels - 1)) {
		/*
		 * consider thermal limit only when it is active and not at
		 * the highest level
		 */
		therm_ma = (int)chip->thermal_mitigation[chip->therm_lvl_sel];
		if (therm_ma < current_ma) {
			pr_smb(PR_STATUS,
				"Limiting current due to thermal: %d mA",
				therm_ma);
			return therm_ma;
		}
	}

	return current_ma;
}

#define CMD_CHG_REG	0x42
#define EN_BAT_CHG_BIT		BIT(1)
static int smbchg_charging_en(struct smbchg_chip *chip, bool en)
{
	/* The en bit is configured active low */
	return smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			EN_BAT_CHG_BIT, en ? 0 : EN_BAT_CHG_BIT);
}

#define CMD_IL			0x40
#define USBIN_SUSPEND_BIT	BIT(4)
#define CURRENT_100_MA		100
#define CURRENT_150_MA		150
#define CURRENT_500_MA		500
#define CURRENT_900_MA		900
#define SUSPEND_CURRENT_MA	2
static int smbchg_usb_suspend(struct smbchg_chip *chip, bool suspend)
{
	int rc;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			USBIN_SUSPEND_BIT, suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set usb suspend rc = %d\n", rc);
	return rc;
}

#define DCIN_SUSPEND_BIT	BIT(3)
static int smbchg_dc_suspend(struct smbchg_chip *chip, bool suspend)
{
	int rc = 0;

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			DCIN_SUSPEND_BIT, suspend ? DCIN_SUSPEND_BIT : 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't set dc suspend rc = %d\n", rc);
	return rc;
}

#define IL_CFG			0xF2
#define DCIN_INPUT_MASK	SMB_MASK(4, 0)
static int smbchg_set_dc_current_max(struct smbchg_chip *chip, int current_ma)
{
	int i;
	u8 dc_cur_val;

	for (i = ARRAY_SIZE(dc_current_table) - 1; i >= 0; i--) {
		if (current_ma >= dc_current_table[i])
			break;
	}

	if (i < 0) {
		dev_err(chip->dev, "Cannot find %dma current_table\n",
				current_ma);
		return -EINVAL;
	}

	chip->dc_max_current_ma = dc_current_table[i];
	dc_cur_val = i & DCIN_INPUT_MASK;

	pr_smb(PR_STATUS, "dc current set to %d mA\n",
			chip->dc_max_current_ma);
	return smbchg_sec_masked_write(chip, chip->dc_chgpth_base + IL_CFG,
				DCIN_INPUT_MASK, dc_cur_val);
}

enum enable_reason {
	/* userspace has suspended charging altogether */
	REASON_USER = BIT(0),
	/*
	 * this specific path has been suspended through the power supply
	 * framework
	 */
	REASON_POWER_SUPPLY = BIT(1),
	/*
	 * the usb driver has suspended this path by setting a current limit
	 * of < 2MA
	 */
	REASON_USB = BIT(2),
	/*
	 * when a wireless charger comes online,
	 * the dc path is suspended for a second
	 */
	REASON_WIRELESS = BIT(3),
	/*
	 * the thermal daemon can suspend a charge path when the system
	 * temperature levels rise
	 */
	REASON_THERMAL = BIT(4),
	/*
	 * an external OTG supply is being used, suspend charge path so the
	 * charger does not accidentally try to charge from the external supply.
	 */
	REASON_OTG = BIT(5),
	/*
	 * The Store DEMO App is running, ensure proper USB Suspend.
	 */
	REASON_DEMO = BIT(6),
};

enum battchg_enable_reason {
	/* userspace has disabled battery charging */
	REASON_BATTCHG_USER		= BIT(0),
	/* battery charging disabled while loading battery profiles */
	REASON_BATTCHG_LOADING_PROFILE	= BIT(1),
};

static struct power_supply *get_parallel_psy(struct smbchg_chip *chip)
{
	if (!chip->parallel.avail)
		return NULL;
	if (chip->parallel.psy)
		return chip->parallel.psy;
	chip->parallel.psy = power_supply_get_by_name("usb-parallel");
	if (!chip->parallel.psy)
		pr_smb(PR_STATUS, "parallel charger not found\n");
	return chip->parallel.psy;
}

static void smbchg_usb_update_online_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				usb_set_online_work);
	bool user_enabled = (chip->usb_suspended & REASON_USER) == 0;
	int online = user_enabled && chip->usb_present;

	mutex_lock(&chip->usb_set_online_lock);
	if (chip->usb_online != online) {
		power_supply_set_online(chip->usb_psy, online);
		chip->usb_online = online;
	}
	mutex_unlock(&chip->usb_set_online_lock);
}

static bool smbchg_primary_usb_is_en(struct smbchg_chip *chip,
		enum enable_reason reason)
{
	bool enabled;

	mutex_lock(&chip->usb_en_lock);
	enabled = (chip->usb_suspended & reason) == 0;
	mutex_unlock(&chip->usb_en_lock);

	return enabled;
}

static bool smcghg_is_battchg_en(struct smbchg_chip *chip,
		enum battchg_enable_reason reason)
{
	bool enabled;

	mutex_lock(&chip->battchg_disabled_lock);
	enabled = !(chip->battchg_disabled & reason);
	mutex_unlock(&chip->battchg_disabled_lock);

	return enabled;
}

static int smbchg_battchg_en(struct smbchg_chip *chip, bool enable,
		enum battchg_enable_reason reason, bool *changed)
{
	int rc = 0, battchg_disabled;

	pr_smb(PR_STATUS, "battchg %s, susp = %02x, en? = %d, reason = %02x\n",
			chip->battchg_disabled == 0 ? "enabled" : "disabled",
			chip->battchg_disabled, enable, reason);

	mutex_lock(&chip->battchg_disabled_lock);
	if (!enable)
		battchg_disabled = chip->battchg_disabled | reason;
	else
		battchg_disabled = chip->battchg_disabled & (~reason);

	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!battchg_disabled == !!chip->battchg_disabled) {
		*changed = false;
		goto out;
	}

	rc = smbchg_charging_en(chip, !battchg_disabled);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't configure batt chg: 0x%x rc = %d\n",
			battchg_disabled, rc);
		goto out;
	}
	*changed = true;

	pr_smb(PR_STATUS, "batt charging %s, battchg_disabled = %02x\n",
			battchg_disabled == 0 ? "enabled" : "disabled",
			battchg_disabled);
out:
	chip->battchg_disabled = battchg_disabled;
	mutex_unlock(&chip->battchg_disabled_lock);
	return rc;
}

static int smbchg_primary_usb_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason, bool *changed)
{
	int rc = 0, suspended;

	pr_smb(PR_STATUS, "usb %s, susp = %02x, en? = %d, reason = %02x\n",
			chip->usb_suspended == 0 ? "enabled"
			: "suspended", chip->usb_suspended, enable, reason);
	mutex_lock(&chip->usb_en_lock);
	if (!enable)
		suspended = chip->usb_suspended | reason;
	else
		suspended = chip->usb_suspended & (~reason);

	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!suspended == !!chip->usb_suspended) {
		*changed = false;
		goto out;
	}

	*changed = true;
	rc = smbchg_usb_suspend(chip, suspended != 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set usb suspend: %d rc = %d\n",
			suspended, rc);
		goto out;
	}

	pr_smb(PR_STATUS, "usb charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
out:
	chip->usb_suspended = suspended;
	mutex_unlock(&chip->usb_en_lock);
	return rc;
}

static int smbchg_dc_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason)
{
	int rc = 0, suspended;

	pr_smb(PR_STATUS, "dc %s, susp = %02x, en? = %d, reason = %02x\n",
			chip->dc_suspended == 0 ? "enabled"
			: "suspended", chip->dc_suspended, enable, reason);
	mutex_lock(&chip->dc_en_lock);
	if (!enable)
		suspended = chip->dc_suspended | reason;
	else
		suspended = chip->dc_suspended & ~reason;

	/* avoid unnecessary spmi interactions if nothing changed */
	if (!!suspended == !!chip->dc_suspended)
		goto out;

	rc = smbchg_dc_suspend(chip, suspended != 0);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't set dc suspend: %d rc = %d\n",
			suspended, rc);
		goto out;
	}

	if (chip->psy_registered)
		power_supply_changed(&chip->dc_psy);
	pr_smb(PR_STATUS, "dc charging %s, suspended = %02x\n",
			suspended == 0 ? "enabled"
			: "suspended", suspended);
out:
	chip->dc_suspended = suspended;
	mutex_unlock(&chip->dc_en_lock);
	return rc;
}

static int smbchg_get_pchg_current_map_index(struct smbchg_chip *chip)
{
	int i;

	for (i = 0; i < chip->pchg_current_map_len; i++) {
		if (chip->target_fastchg_current_ma >=
		    chip->pchg_current_map_data[i].requested) {
			break;
		}
	}

	if (i >= chip->pchg_current_map_len)
		i = (chip->pchg_current_map_len - 1);

	return i;
}

#define CHGPTH_CFG		0xF4
#define CFG_USB_2_3_SEL_BIT	BIT(7)
#define CFG_USB_2		0
#define CFG_USB_3		BIT(7)
#define USBIN_INPUT_MASK	SMB_MASK(4, 0)
#define USBIN_MODE_CHG_BIT	BIT(0)
#define USBIN_LIMITED_MODE	0
#define USBIN_HC_MODE		BIT(0)
#define USB51_MODE_BIT		BIT(1)
#define USB51_100MA		0
#define USB51_500MA		BIT(1)
static int smbchg_set_high_usb_chg_current(struct smbchg_chip *chip,
							int current_ma)
{
	int i, rc;
	u8 usb_cur_val;

	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_150_MA);

		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_150_MA, rc);
		else
			chip->usb_max_current_ma = 150;
		return rc;
	}

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + IL_CFG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
		return rc;
	}

	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
				USBIN_MODE_CHG_BIT, USBIN_HC_MODE);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write cfg 5 rc = %d\n", rc);
	chip->usb_max_current_ma = usb_current_table[i];
	return rc;
}

static int smbchg_set_usb_chg_current(struct smbchg_chip *chip,
							int current_ma)
{
	int i, rc;
	u8 usb_cur_val;

	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}

	/* Set minimum input limit if not found in the current table */
	if (i < 0)
		i = 0;

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + IL_CFG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0)
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
	return rc;
}

/* if APSD results are used
 *	if SDP is detected it will look at 500mA setting
 *		if set it will draw 500mA
 *		if unset it will draw 100mA
 *	if CDP/DCP it will look at 0x0C setting
 *		i.e. values in 0x41[1, 0] does not matter
 */
static int smbchg_set_usb_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	int rc;
	bool changed;

	if (!chip->batt_present) {
		pr_info_ratelimited("Ignoring usb current->%d, battery is absent\n",
				current_ma);
		return 0;
	}
	pr_smb(PR_STATUS, "USB current_ma = %d\n", current_ma);

	if (current_ma == SUSPEND_CURRENT_MA) {
		/* suspend the usb if current set to 2mA */
		rc = smbchg_primary_usb_en(chip, false, REASON_USB, &changed);
		chip->usb_max_current_ma = 0;
		goto out;
	} else {
		rc = smbchg_primary_usb_en(chip, true, REASON_USB, &changed);
	}

	if (chip->low_icl_wa_on) {
		chip->usb_max_current_ma = current_ma;
		pr_smb(PR_STATUS,
			"low_icl_wa on, ignoring the usb current setting\n");
		goto out;
	}
	if (current_ma < CURRENT_150_MA) {
		/* force 100mA */
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		chip->usb_max_current_ma = 100;
		smbchg_set_usb_chg_current(chip, chip->usb_max_current_ma);
		goto out;
	}
	/* specific current values */
	if (current_ma == CURRENT_150_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		chip->usb_max_current_ma = 150;
		smbchg_set_usb_chg_current(chip, chip->usb_max_current_ma);
		goto out;
	}
	if (current_ma == CURRENT_500_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_500MA);
		chip->usb_max_current_ma = 500;
		smbchg_set_usb_chg_current(chip, chip->usb_max_current_ma);
		goto out;
	}
	if (current_ma == CURRENT_900_MA) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_3);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_500MA);
		chip->usb_max_current_ma = 900;
		smbchg_set_usb_chg_current(chip, chip->usb_max_current_ma);
		goto out;
	}

	rc = smbchg_set_high_usb_chg_current(chip, current_ma);
out:
	pr_smb(PR_STATUS, "usb current set to %d mA\n",
			chip->usb_max_current_ma);
	if (rc < 0)
		dev_err(chip->dev,
			"Couldn't set %dmA rc = %d\n", current_ma, rc);
	return rc;
}

#define USBIN_HVDCP_STS			0x0C
#define USBIN_HVDCP_SEL_BIT		BIT(4)
#define USBIN_HVDCP_SEL_9V_BIT		BIT(1)
static int smbchg_get_min_parallel_current_ma(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + USBIN_HVDCP_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb status rc = %d\n", rc);
		return 0;
	}
	if ((reg & USBIN_HVDCP_SEL_BIT) && (reg & USBIN_HVDCP_SEL_9V_BIT))
		return chip->parallel.min_9v_current_thr_ma;
	return chip->parallel.min_current_thr_ma;
}

#define ICL_STS_1_REG			0x7
#define ICL_STS_2_REG			0x9
#define ICL_STS_MASK			0x1F
#define AICL_SUSP_BIT			BIT(6)
#define AICL_STS_BIT			BIT(5)
#define USBIN_SUSPEND_STS_BIT		BIT(3)
#define USBIN_ACTIVE_PWR_SRC_BIT	BIT(1)
#define DCIN_ACTIVE_PWR_SRC_BIT		BIT(0)
#define PARALLEL_MIN_BATT_VOLT_UV	3300000
static bool smbchg_is_parallel_usb_ok(struct smbchg_chip *chip)
{
	int min_current_thr_ma, rc, type, index;
	u8 reg;

	if (!smbchg_parallel_en) {
		pr_smb(PR_STATUS, "Parallel charging not enabled\n");
		return false;
	}

	if (get_prop_charge_type(chip) != POWER_SUPPLY_CHARGE_TYPE_FAST) {
		pr_smb(PR_STATUS, "Not in fast charge, skipping\n");
		return false;
	}
#ifdef QCOM_BASE
	if (get_prop_batt_health(chip) != POWER_SUPPLY_HEALTH_GOOD) {
		pr_smb(PR_STATUS, "JEITA active, skipping\n");
		return false;
	}

	if (get_prop_batt_voltage_now(chip) < PARALLEL_MIN_BATT_VOLT_UV) {
		pr_smb(PR_STATUS, "Battery Voltage below %d, skipping\n",
		       PARALLEL_MIN_BATT_VOLT_UV);
		return false;
	}
#endif

	if (chip->demo_mode) {
		pr_smb(PR_STATUS, "Demo Mode, skipping\n");
		return false;
	}

	if ((chip->stepchg_state == STEP_TAPER) ||
	    (chip->stepchg_state == STEP_FULL)) {
		pr_smb(PR_STATUS, "Battery Voltage High, skipping\n");
		return false;
	}

	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
		return false;
	}

	type = get_type(reg);
	if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB_CDP) {
		pr_smb(PR_STATUS, "CDP adapter, skipping\n");
		return false;
	}

	if (get_usb_supply_type(type) == POWER_SUPPLY_TYPE_USB) {
		pr_smb(PR_STATUS, "SDP adapter, skipping\n");
		return false;
	}

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_2_REG, 1);
	if (rc) {
		dev_err(chip->dev, "Could not read usb icl sts 2: %d\n", rc);
		return false;
	}

	/*
	 * If USBIN is suspended or not the active power source, do not enable
	 * parallel charging. The device may be charging off of DCIN.
	 */
	if (!!(reg & USBIN_SUSPEND_STS_BIT) ||
				!(reg & USBIN_ACTIVE_PWR_SRC_BIT)) {
		pr_smb(PR_STATUS, "USB not active power source: %02x\n", reg);
		return false;
	}

	min_current_thr_ma = smbchg_get_min_parallel_current_ma(chip);
	if (min_current_thr_ma <= 0) {
		pr_smb(PR_STATUS, "parallel charger unavailable for thr: %d\n",
				min_current_thr_ma);
		return false;
	}
	if (chip->usb_tl_current_ma < min_current_thr_ma) {
		pr_smb(PR_STATUS, "Weak USB chg skip enable: %d < %d\n",
			chip->usb_tl_current_ma, min_current_thr_ma);
		return false;
	}

	index = smbchg_get_pchg_current_map_index(chip);
	if (chip->pchg_current_map_data[index].secondary == 0)
		return false;

	return true;
}

#define FCC_CFG			0xF2
#define FCC_500MA_VAL		0x4
#define FCC_MASK		SMB_MASK(4, 0)
static int smbchg_set_fastchg_current_raw(struct smbchg_chip *chip,
							int current_ma)
{
	int i, rc;
	u8 cur_val;

	/* the fcc enumerations are the same as the usb currents */
	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_500_MA);

		rc = smbchg_sec_masked_write(chip, chip->chgr_base + FCC_CFG,
					FCC_MASK,
					FCC_500MA_VAL);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_500_MA, rc);
		else
			chip->fastchg_current_ma = 500;
		return rc;
	}

	cur_val = i & FCC_MASK;
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + FCC_CFG,
				FCC_MASK, cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to fcc cfg rc = %d\n", rc);
		return rc;
	}
	pr_smb(PR_STATUS, "fastcharge current set to %d\n",
			current_ma);

	chip->fastchg_current_ma = usb_current_table[i];
	return rc;
}

static int smbchg_set_fastchg_current(struct smbchg_chip *chip,
							int current_ma)
{
	int rc;

	mutex_lock(&chip->fcc_lock);
	if (chip->sw_esr_pulse_en)
		current_ma = 300;
	rc = smbchg_set_fastchg_current_raw(chip, current_ma);
	mutex_unlock(&chip->fcc_lock);
	return rc;
}

static int smbchg_set_parallel_vfloat(struct smbchg_chip *chip, int volt_mv)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };

	if (!parallel_psy)
		return 0;

	pval.intval = volt_mv * 1000;
	return parallel_psy->set_property(parallel_psy,
		POWER_SUPPLY_PROP_VOLTAGE_MAX, &pval);
}

static int smbchg_parallel_usb_charging_en(struct smbchg_chip *chip, bool en)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };

	if (!parallel_psy)
		return 0;

	pval.intval = en;
	return parallel_psy->set_property(parallel_psy,
		POWER_SUPPLY_PROP_CHARGING_ENABLED, &pval);
}

static int smbchg_sw_esr_pulse_en(struct smbchg_chip *chip, bool en)
{
	int rc;
	int index = 0;
	int target_current = chip->target_fastchg_current_ma;

	if (chip->parallel.current_max_ma != 0) {
		index = smbchg_get_pchg_current_map_index(chip);
		target_current = chip->pchg_current_map_data[index].primary;
	}

	chip->sw_esr_pulse_en = en;
	rc = smbchg_set_fastchg_current(chip, target_current);
	if (rc)
		return rc;
	if (chip->parallel.current_max_ma != 0)
		rc = smbchg_parallel_usb_charging_en(chip, !en);
	return rc;
}

#define USB_AICL_CFG				0xF3
#define AICL_EN_BIT				BIT(2)
static void smbchg_rerun_aicl(struct smbchg_chip *chip)
{
	pr_smb(PR_STATUS, "Rerunning AICL...\n");
	smbchg_sec_masked_write(chip, chip->usb_chgpth_base + USB_AICL_CFG,
			AICL_EN_BIT, 0);
	/* Add a delay so that AICL successfully clears */
	msleep(50);
	smbchg_sec_masked_write(chip, chip->usb_chgpth_base + USB_AICL_CFG,
			AICL_EN_BIT, AICL_EN_BIT);
}

static void taper_irq_en(struct smbchg_chip *chip, bool en)
{
	mutex_lock(&chip->taper_irq_lock);
	if ((chip->stepchg_state != STEP_ONE) &&
	    (chip->stepchg_state != STEP_TAPER) &&
	    (chip->stepchg_state != STEP_FULL))
		en = false;

	if (en != chip->taper_irq_enabled) {
		if (en) {
			enable_irq(chip->taper_irq);
			enable_irq_wake(chip->taper_irq);
		} else {
			disable_irq_wake(chip->taper_irq);
			disable_irq_nosync(chip->taper_irq);
		}
		chip->taper_irq_enabled = en;
	}
	mutex_unlock(&chip->taper_irq_lock);
}

#define HVDCP_ICL_MAX 2600
#define HVDCP_ICL_TAPER 1600
static void smbchg_parallel_usb_disable(struct smbchg_chip *chip)
{
	int current_max_ma;
	int pcurrent_max_ma = chip->parallel.current_max_ma;
	struct power_supply *parallel_psy = get_parallel_psy(chip);

	if (!parallel_psy)
		return;
	pr_smb(PR_STATUS, "disabling parallel charger\n");
	taper_irq_en(chip, false);
	chip->parallel.initial_aicl_ma = 0;
	chip->parallel.current_max_ma = 0;
	power_supply_set_current_limit(parallel_psy,
				SUSPEND_CURRENT_MA * 1000);
	power_supply_set_present(parallel_psy, false);

	current_max_ma = chip->stepchg_current_ma;
	current_max_ma =
		min(current_max_ma, chip->allowed_fastchg_current_ma);
	current_max_ma =
		min(current_max_ma,  chip->target_fastchg_current_ma);

	smbchg_set_fastchg_current(chip, current_max_ma);
	chip->usb_tl_current_ma =
		calc_thermal_limited_current(chip, chip->usb_target_current_ma);
	smbchg_set_usb_current_max(chip, chip->usb_tl_current_ma);

	if (pcurrent_max_ma != 0)
		smbchg_rerun_aicl(chip);
}

#define PARALLEL_TAPER_MAX_TRIES		3
#define PARALLEL_FCC_PERCENT_REDUCTION		75
#define MINIMUM_PARALLEL_FCC_MA			1100
#define CHG_ERROR_BIT		BIT(0)
#define BAT_TAPER_MODE_BIT	BIT(6)
static void smbchg_parallel_usb_taper(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };
	int parallel_fcc_ma, tries = 0;
	u8 reg = 0;

	if (!parallel_psy)
		return;

try_again:
	mutex_lock(&chip->parallel.lock);
	if (chip->parallel.current_max_ma == 0) {
		pr_smb(PR_STATUS, "Not parallel charging, skipping\n");
		goto done;
	}
	if ((get_prop_batt_voltage_now(chip) / 1000) <
	    chip->stepchg_max_voltage_mv) {
		pr_smb(PR_STATUS, "Not in Taper, skipping\n");
		goto done;
	}
	parallel_psy->get_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	tries += 1;
	parallel_fcc_ma = pval.intval / 1000;
	pr_smb(PR_STATUS, "try #%d parallel charger fcc = %d\n",
			tries, parallel_fcc_ma);
	if (parallel_fcc_ma < MINIMUM_PARALLEL_FCC_MA
				|| tries > PARALLEL_TAPER_MAX_TRIES) {
		smbchg_parallel_usb_disable(chip);
		goto done;
	}
	pval.intval = 1000 * ((parallel_fcc_ma
			* PARALLEL_FCC_PERCENT_REDUCTION) / 100);
	parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	/*
	 * sleep here for 100 ms in order to make sure the charger has a chance
	 * to go back into constant current charging
	 */
	mutex_unlock(&chip->parallel.lock);
	msleep(100);

	mutex_lock(&chip->parallel.lock);
	if (chip->parallel.current_max_ma == 0) {
		pr_smb(PR_STATUS, "Not parallel charging, skipping\n");
		goto done;
	}
	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	if (reg & BAT_TAPER_MODE_BIT) {
		mutex_unlock(&chip->parallel.lock);
		goto try_again;
	}
	taper_irq_en(chip, true);
done:
	mutex_unlock(&chip->parallel.lock);
}

static bool smbchg_is_aicl_complete(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_1_REG, 1);
	if (rc) {
		dev_err(chip->dev, "Could not read usb icl sts 1: %d\n", rc);
		return true;
	}
	return (reg & AICL_STS_BIT) != 0;
}

static int smbchg_get_aicl_level_ma(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_1_REG, 1);
	if (rc) {
		dev_err(chip->dev, "Could not read usb icl sts 1: %d\n", rc);
		return 0;
	}
	reg &= ICL_STS_MASK;
	if (reg & AICL_SUSP_BIT) {
		pr_warn("AICL suspended: %02x\n", reg);
		return 0;
	}
	if (reg >= ARRAY_SIZE(usb_current_table)) {
		pr_warn("invalid AICL value: %02x\n", reg);
		return 0;
	}
	return usb_current_table[reg];
}

static void smbchg_parallel_usb_enable(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);
	union power_supply_propval pval = {0, };
	int current_limit_ma, parallel_cl_ma, total_current_ma;
	int new_parallel_cl_ma, min_current_thr_ma;
	int primary_fastchg_current;
	int secondary_fastchg_current;
	int index;

	if (!parallel_psy)
		return;

	pr_smb(PR_STATUS, "Attempting to enable parallel charger\n");
	min_current_thr_ma = smbchg_get_min_parallel_current_ma(chip);
	if (min_current_thr_ma <= 0) {
		pr_warn("parallel charger unavailable for thr: %d\n",
				min_current_thr_ma);
		goto disable_parallel;
	}

	current_limit_ma = smbchg_get_aicl_level_ma(chip);
	if (current_limit_ma <= 0)
		goto disable_parallel;

	if (chip->parallel.initial_aicl_ma == 0) {
		if (current_limit_ma < min_current_thr_ma) {
			pr_warn("Initial AICL very low: %d < %d\n",
				current_limit_ma, min_current_thr_ma);
			goto disable_parallel;
		}
		chip->parallel.initial_aicl_ma = current_limit_ma;
	}

	/*
	 * Use the previous set current from the parallel charger.
	 * Treat 2mA as 0 because that is the suspend current setting
	 */
	parallel_cl_ma = chip->parallel.current_max_ma;
	if (parallel_cl_ma <= SUSPEND_CURRENT_MA)
		parallel_cl_ma = 0;

	/*
	 * Set the parallel charge path's input current limit (ICL)
	 * to the total current / 2
	 */
	total_current_ma = current_limit_ma + parallel_cl_ma;

	if (total_current_ma < chip->parallel.initial_aicl_ma
			- chip->parallel.allowed_lowering_ma) {
		pr_warn("Too little total current : %d (%d + %d) < %d - %d\n",
			total_current_ma,
			current_limit_ma, parallel_cl_ma,
			chip->parallel.initial_aicl_ma,
			chip->parallel.allowed_lowering_ma);
		goto disable_parallel;
	}

	new_parallel_cl_ma = total_current_ma / 2;

	if ((new_parallel_cl_ma == parallel_cl_ma) &&
	    (chip->target_fastchg_current_ma ==
	     chip->prev_target_fastchg_current_ma)) {
		pr_smb(PR_STATUS,
			"AICL at %d, old ICL: %d new ICL: %d, skipping\n",
			current_limit_ma, parallel_cl_ma, new_parallel_cl_ma);
		return;
	} else {
		pr_smb(PR_STATUS, "AICL at %d, old ICL: %d new ICL: %d\n",
			current_limit_ma, parallel_cl_ma, new_parallel_cl_ma);
	}

	index = smbchg_get_pchg_current_map_index(chip);
	primary_fastchg_current = chip->pchg_current_map_data[index].primary;
	secondary_fastchg_current =
		chip->pchg_current_map_data[index].secondary;

	pr_smb(PR_STATUS, "primary charge current=%d\n",
	       primary_fastchg_current);
	pr_smb(PR_STATUS, "secondary charge current=%d\n",
	       secondary_fastchg_current);

	taper_irq_en(chip, true);
	chip->parallel.current_max_ma = new_parallel_cl_ma;
	power_supply_set_present(parallel_psy, true);
	smbchg_set_fastchg_current(chip, primary_fastchg_current);
	pval.intval = (secondary_fastchg_current * 1000);
	pr_smb(PR_STATUS, "IBATT MAIN at %d mA, PARALLEL at %d mA\n",
	       primary_fastchg_current, secondary_fastchg_current);
	parallel_psy->set_property(parallel_psy,
			POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &pval);
	smbchg_set_usb_current_max(chip, chip->parallel.current_max_ma);
	power_supply_set_current_limit(parallel_psy,
				chip->parallel.current_max_ma * 1000);

	chip->prev_target_fastchg_current_ma = chip->target_fastchg_current_ma;
	return;

disable_parallel:
	pr_warn("disabling parallel charger\n");

	if (chip->usb_target_current_ma == HVDCP_ICL_MAX) {
		mutex_lock(&chip->current_change_lock);
		chip->usb_target_current_ma = HVDCP_ICL_TAPER;
		mutex_unlock(&chip->current_change_lock);
	}

	smbchg_parallel_usb_disable(chip);
}

static void smbchg_parallel_usb_en_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				parallel_en_work.work);

	smbchg_relax(chip, PM_PARALLEL_CHECK);
	mutex_lock(&chip->parallel.lock);
	if (smbchg_is_parallel_usb_ok(chip)) {
		smbchg_parallel_usb_enable(chip);
	} else {
		pr_smb(PR_STATUS, "parallel charging unavailable\n");
		smbchg_parallel_usb_disable(chip);
	}
	mutex_unlock(&chip->parallel.lock);
}

#define PARALLEL_CHARGER_EN_DELAY_MS	3500
static void smbchg_parallel_usb_check_ok(struct smbchg_chip *chip)
{
	struct power_supply *parallel_psy = get_parallel_psy(chip);

	if (!parallel_psy)
		return;
	mutex_lock(&chip->parallel.lock);
	if (smbchg_is_parallel_usb_ok(chip)) {
		smbchg_stay_awake(chip, PM_PARALLEL_CHECK);
		schedule_delayed_work(
			&chip->parallel_en_work,
			msecs_to_jiffies(PARALLEL_CHARGER_EN_DELAY_MS));
	} else {
		pr_smb(PR_STATUS, "parallel charging unavailable\n");
		smbchg_parallel_usb_disable(chip);
	}
	mutex_unlock(&chip->parallel.lock);
}

static int smbchg_usb_en(struct smbchg_chip *chip, bool enable,
		enum enable_reason reason)
{
	bool changed = false;
	int rc = smbchg_primary_usb_en(chip, enable, reason, &changed);

	if (changed)
		smbchg_parallel_usb_check_ok(chip);
	return rc;
}

static struct ilim_entry *smbchg_wipower_find_entry(struct smbchg_chip *chip,
				struct ilim_map *map, int uv)
{
	int i;
	struct ilim_entry *ret = &(chip->wipower_default.entries[0]);

	for (i = 0; i < map->num; i++) {
		if (is_between(map->entries[i].vmin_uv, map->entries[i].vmax_uv,
			uv))
			ret = &map->entries[i];
	}
	return ret;
}

static int ilim_ma_table[] = {
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

#define ZIN_ICL_PT	0xFC
#define ZIN_ICL_LV	0xFD
#define ZIN_ICL_HV	0xFE
#define ZIN_ICL_MASK	SMB_MASK(4, 0)
static int smbchg_dcin_ilim_config(struct smbchg_chip *chip, int offset, int ma)
{
	int i, rc;

	for (i = ARRAY_SIZE(ilim_ma_table) - 1; i >= 0; i--) {
		if (ma >= ilim_ma_table[i])
			break;
	}

	if (i < 0)
		i = 0;

	rc = smbchg_sec_masked_write(chip, chip->bat_if_base + offset,
			ZIN_ICL_MASK, i);
	if (rc)
		dev_err(chip->dev, "Couldn't write bat if offset %d value = %d rc = %d\n",
				offset, i, rc);
	return rc;
}

static int smbchg_wipower_ilim_config(struct smbchg_chip *chip,
						struct ilim_entry *ilim)
{
	int rc = 0;

	if (chip->current_ilim.icl_pt_ma != ilim->icl_pt_ma) {
		rc = smbchg_dcin_ilim_config(chip, ZIN_ICL_PT, ilim->icl_pt_ma);
		if (rc)
			dev_err(chip->dev, "failed to write batif offset %d %dma rc = %d\n",
					ZIN_ICL_PT, ilim->icl_pt_ma, rc);
		else
			chip->current_ilim.icl_pt_ma =  ilim->icl_pt_ma;
	}

	if (chip->current_ilim.icl_lv_ma !=  ilim->icl_lv_ma) {
		rc = smbchg_dcin_ilim_config(chip, ZIN_ICL_LV, ilim->icl_lv_ma);
		if (rc)
			dev_err(chip->dev, "failed to write batif offset %d %dma rc = %d\n",
					ZIN_ICL_LV, ilim->icl_lv_ma, rc);
		else
			chip->current_ilim.icl_lv_ma =  ilim->icl_lv_ma;
	}

	if (chip->current_ilim.icl_hv_ma !=  ilim->icl_hv_ma) {
		rc = smbchg_dcin_ilim_config(chip, ZIN_ICL_HV, ilim->icl_hv_ma);
		if (rc)
			dev_err(chip->dev, "failed to write batif offset %d %dma rc = %d\n",
					ZIN_ICL_HV, ilim->icl_hv_ma, rc);
		else
			chip->current_ilim.icl_hv_ma =  ilim->icl_hv_ma;
	}
	return rc;
}

static void btm_notify_dcin(enum qpnp_tm_state state, void *ctx);
static int smbchg_wipower_dcin_btm_configure(struct smbchg_chip *chip,
		struct ilim_entry *ilim)
{
	int rc;

	if (ilim->vmin_uv == chip->current_ilim.vmin_uv
			&& ilim->vmax_uv == chip->current_ilim.vmax_uv)
		return 0;

	chip->param.channel = DCIN;
	chip->param.btm_ctx = chip;
	if (wipower_dcin_interval < ADC_MEAS1_INTERVAL_0MS)
		wipower_dcin_interval = ADC_MEAS1_INTERVAL_0MS;

	if (wipower_dcin_interval > ADC_MEAS1_INTERVAL_16S)
		wipower_dcin_interval = ADC_MEAS1_INTERVAL_16S;

	chip->param.timer_interval = wipower_dcin_interval;
	chip->param.threshold_notification = &btm_notify_dcin;
	chip->param.high_thr = ilim->vmax_uv + wipower_dcin_hyst_uv;
	chip->param.low_thr = ilim->vmin_uv - wipower_dcin_hyst_uv;
	chip->param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	rc = qpnp_vadc_channel_monitor(chip->vadc_dev, &chip->param);
	if (rc) {
		dev_err(chip->dev, "Couldn't configure btm for dcin rc = %d\n",
				rc);
	} else {
		chip->current_ilim.vmin_uv = ilim->vmin_uv;
		chip->current_ilim.vmax_uv = ilim->vmax_uv;
		pr_smb(PR_STATUS, "btm ilim = (%duV %duV %dmA %dmA %dmA)\n",
			ilim->vmin_uv, ilim->vmax_uv,
			ilim->icl_pt_ma, ilim->icl_lv_ma, ilim->icl_hv_ma);
	}
	return rc;
}

static int smbchg_wipower_icl_configure(struct smbchg_chip *chip,
						int dcin_uv, bool div2)
{
	int rc = 0;
	struct ilim_map *map = div2 ? &chip->wipower_div2 : &chip->wipower_pt;
	struct ilim_entry *ilim = smbchg_wipower_find_entry(chip, map, dcin_uv);

	rc = smbchg_wipower_ilim_config(chip, ilim);
	if (rc) {
		dev_err(chip->dev, "failed to config ilim rc = %d, dcin_uv = %d , div2 = %d, ilim = (%duV %duV %dmA %dmA %dmA)\n",
			rc, dcin_uv, div2,
			ilim->vmin_uv, ilim->vmax_uv,
			ilim->icl_pt_ma, ilim->icl_lv_ma, ilim->icl_hv_ma);
		return rc;
	}

	rc = smbchg_wipower_dcin_btm_configure(chip, ilim);
	if (rc) {
		dev_err(chip->dev, "failed to config btm rc = %d, dcin_uv = %d , div2 = %d, ilim = (%duV %duV %dmA %dmA %dmA)\n",
			rc, dcin_uv, div2,
			ilim->vmin_uv, ilim->vmax_uv,
			ilim->icl_pt_ma, ilim->icl_lv_ma, ilim->icl_hv_ma);
		return rc;
	}
	chip->wipower_configured = true;
	return 0;
}

static void smbchg_wipower_icl_deconfigure(struct smbchg_chip *chip)
{
	int rc;
	struct ilim_entry *ilim = &(chip->wipower_default.entries[0]);

	if (!chip->wipower_configured)
		return;

	rc = smbchg_wipower_ilim_config(chip, ilim);
	if (rc)
		dev_err(chip->dev, "Couldn't config default ilim rc = %d\n",
				rc);

	rc = qpnp_vadc_end_channel_monitor(chip->vadc_dev);
	if (rc)
		dev_err(chip->dev, "Couldn't de configure btm for dcin rc = %d\n",
				rc);

	chip->wipower_configured = false;
	chip->current_ilim.vmin_uv = 0;
	chip->current_ilim.vmax_uv = 0;
	chip->current_ilim.icl_pt_ma = ilim->icl_pt_ma;
	chip->current_ilim.icl_lv_ma = ilim->icl_lv_ma;
	chip->current_ilim.icl_hv_ma = ilim->icl_hv_ma;
	pr_smb(PR_WIPOWER, "De config btm\n");
}

#define FV_STS		0x0C
#define DIV2_ACTIVE	BIT(7)
static void __smbchg_wipower_check(struct smbchg_chip *chip)
{
	int chg_type;
	bool usb_present, dc_present;
	int rc;
	int dcin_uv;
	bool div2;
	struct qpnp_vadc_result adc_result;
	u8 reg;

	if (!wipower_dyn_icl_en) {
		smbchg_wipower_icl_deconfigure(chip);
		return;
	}

	chg_type = get_prop_charge_type(chip);
	usb_present = is_usb_present(chip);
	dc_present = is_dc_present(chip);
	if (chg_type != POWER_SUPPLY_CHARGE_TYPE_NONE
			 && !usb_present
			&& dc_present
			&& chip->dc_psy_type == POWER_SUPPLY_TYPE_WIPOWER) {
		rc = qpnp_vadc_read(chip->vadc_dev, DCIN, &adc_result);
		if (rc) {
			pr_smb(PR_STATUS, "error DCIN read rc = %d\n", rc);
			return;
		}
		dcin_uv = adc_result.physical;

		/* check div_by_2 */
		rc = smbchg_read(chip, &reg, chip->chgr_base + FV_STS, 1);
		if (rc) {
			pr_smb(PR_STATUS, "error DCIN read rc = %d\n", rc);
			return;
		}
		div2 = !!(reg & DIV2_ACTIVE);

		pr_smb(PR_WIPOWER,
			"config ICL chg_type = %d usb = %d dc = %d dcin_uv(adc_code) = %d (0x%x) div2 = %d\n",
			chg_type, usb_present, dc_present, dcin_uv,
			adc_result.adc_code, div2);
		smbchg_wipower_icl_configure(chip, dcin_uv, div2);
	} else {
		pr_smb(PR_WIPOWER,
			"deconfig ICL chg_type = %d usb = %d dc = %d\n",
			chg_type, usb_present, dc_present);
		smbchg_wipower_icl_deconfigure(chip);
	}
}

static void smbchg_wipower_check(struct smbchg_chip *chip)
{
	if (!chip->wipower_dyn_icl_avail)
		return;

	mutex_lock(&chip->wipower_config);
	__smbchg_wipower_check(chip);
	mutex_unlock(&chip->wipower_config);
}

static void btm_notify_dcin(enum qpnp_tm_state state, void *ctx)
{
	struct smbchg_chip *chip = ctx;

	mutex_lock(&chip->wipower_config);
	pr_smb(PR_WIPOWER, "%s state\n",
			state  == ADC_TM_LOW_STATE ? "low" : "high");
	chip->current_ilim.vmin_uv = 0;
	chip->current_ilim.vmax_uv = 0;
	__smbchg_wipower_check(chip);
	mutex_unlock(&chip->wipower_config);
}

static int force_dcin_icl_write(void *data, u64 val)
{
	struct smbchg_chip *chip = data;

	smbchg_wipower_check(chip);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(force_dcin_icl_ops, NULL,
		force_dcin_icl_write, "0x%02llx\n");

/*
 * set the dc charge path's maximum allowed current draw
 * that may be limited by the system's thermal level
 */
static int smbchg_set_thermal_limited_dc_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	current_ma = calc_thermal_limited_current(chip, current_ma);
	return smbchg_set_dc_current_max(chip, current_ma);
}

/*
 * set the usb charge path's maximum allowed current draw
 * that may be limited by the system's thermal level
 */
static int smbchg_set_thermal_limited_usb_current_max(struct smbchg_chip *chip,
							int current_ma)
{
	int rc, aicl_ma;

	aicl_ma = smbchg_get_aicl_level_ma(chip);
	chip->usb_tl_current_ma =
		calc_thermal_limited_current(chip, current_ma);
	rc = smbchg_set_usb_current_max(chip, chip->usb_tl_current_ma);
	if (rc) {
		pr_err("Failed to set usb current max: %d\n", rc);
		return rc;
	}

	pr_smb(PR_STATUS, "AICL = %d, ICL = %d\n",
			aicl_ma, chip->usb_max_current_ma);
	if (chip->usb_max_current_ma > aicl_ma && smbchg_is_aicl_complete(chip))
		smbchg_rerun_aicl(chip);
	smbchg_parallel_usb_check_ok(chip);
	return rc;
}

static int smbchg_chg_system_temp_level_set(struct smbchg_chip *chip,
					    int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->chg_thermal_mitigation) {
		dev_err(chip->dev, "Charge thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		dev_err(chip->dev, "Unsupported charge level selected %d\n",
			lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->chg_thermal_levels) {
		dev_err(chip->dev,
			"Unsupported charge level selected %d forcing %d\n",
			lvl_sel, chip->chg_thermal_levels - 1);
		lvl_sel = chip->chg_thermal_levels - 1;
	}

	if (lvl_sel == chip->chg_therm_lvl_sel)
		return 0;

	mutex_lock(&chip->current_change_lock);
	prev_therm_lvl = chip->chg_therm_lvl_sel;
	chip->chg_therm_lvl_sel = lvl_sel;

	chip->allowed_fastchg_current_ma =
		chip->chg_thermal_mitigation[lvl_sel];
	chip->update_allowed_fastchg_current_ma = true;

	smbchg_stay_awake(chip, PM_HEARTBEAT);
	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work,
			      msecs_to_jiffies(0));
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int smbchg_system_temp_level_set(struct smbchg_chip *chip,
					int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->thermal_mitigation) {
		dev_err(chip->dev, "Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		dev_err(chip->dev, "Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->thermal_levels) {
		dev_err(chip->dev, "Unsupported USB level selected %d forcing %d\n",
				lvl_sel, chip->thermal_levels - 1);
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
		rc = smbchg_usb_en(chip, false, REASON_THERMAL);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
		goto out;
	}

	rc = smbchg_set_thermal_limited_usb_current_max(chip,
					chip->usb_target_current_ma);

	if (prev_therm_lvl == chip->thermal_levels - 1) {
		/*
		 * If previously highest value was selected charging must have
		 * been disabed. Enable charging by taking the USB path out of
		 * suspend.
		 */
		rc = smbchg_usb_en(chip, true, REASON_THERMAL);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set usb suspend rc %d\n", rc);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int smbchg_dc_system_temp_level_set(struct smbchg_chip *chip,
					   int lvl_sel)
{
	int rc = 0;
	int prev_therm_lvl;

	if (!chip->dc_thermal_mitigation) {
		dev_err(chip->dev, "Thermal mitigation not supported\n");
		return -EINVAL;
	}

	if (lvl_sel < 0) {
		dev_err(chip->dev, "Unsupported level selected %d\n", lvl_sel);
		return -EINVAL;
	}

	if (lvl_sel >= chip->dc_thermal_levels) {
		dev_err(chip->dev, "Unsupported DC level selected %d forcing %d\n",
				lvl_sel, chip->dc_thermal_levels - 1);
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
		 * setting the DC path in suspend
		 */
		rc = smbchg_dc_en(chip, false, REASON_THERMAL);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
		goto out;
	}

	rc = smbchg_set_thermal_limited_dc_current_max(chip,
					chip->dc_target_current_ma);

	if (prev_therm_lvl == chip->dc_thermal_levels - 1) {
		/*
		 * If previously highest value was selected charging must have
		 * been disabed. Enable charging by taking the DC path out of
		 * suspend.
		 */
		rc = smbchg_dc_en(chip, true, REASON_THERMAL);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set dc suspend rc %d\n", rc);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

#define UCONV			1000000LL
#define VIN_FLASH_UV		5500000LL
#define FLASH_V_THRESHOLD	3000000LL
#define BUCK_EFFICIENCY		800LL
static int smbchg_calc_max_flash_current(struct smbchg_chip *chip)
{
	int ocv_uv, ibat_ua, esr_uohm, rbatt_uohm, rc;
	int64_t ibat_flash_ua, total_flash_ua, total_flash_power_fw;

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_VOLTAGE_OCV, &ocv_uv);
	if (rc) {
		pr_smb(PR_STATUS, "bms psy does not support OCV\n");
		return 0;
	}

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CURRENT_NOW, &ibat_ua);
	if (rc) {
		pr_smb(PR_STATUS, "bms psy does not support current_now\n");
		return 0;
	}

	rc = get_property_from_fg(chip, POWER_SUPPLY_PROP_RESISTANCE,
			&esr_uohm);
	if (rc) {
		pr_smb(PR_STATUS, "bms psy does not support resistance\n");
		return 0;
	}

	rbatt_uohm = esr_uohm + chip->rpara_uohm + chip->rslow_uohm;
	ibat_flash_ua = (div_s64((ocv_uv - FLASH_V_THRESHOLD) * UCONV,
			rbatt_uohm)) - ibat_ua;
	total_flash_power_fw = FLASH_V_THRESHOLD * ibat_flash_ua
			* BUCK_EFFICIENCY;
	total_flash_ua = div64_s64(total_flash_power_fw, VIN_FLASH_UV * 1000LL);
	pr_smb(PR_MISC,
		"ibat_flash=%lld\n, ocv=%d, ibat=%d, rbatt=%d t_flash=%lld\n",
		ibat_flash_ua, ocv_uv, ibat_ua, rbatt_uohm, total_flash_ua);
	return (int)total_flash_ua;
}

#define FCC_CMP_CFG	0xF3
#define FCC_COMP_MASK	SMB_MASK(1, 0)
static int smbchg_fastchg_current_comp_set(struct smbchg_chip *chip,
					int comp_current)
{
	int rc;
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fcc_comp_table); i++)
		if (comp_current == fcc_comp_table[i])
			break;

	if (i >= ARRAY_SIZE(fcc_comp_table))
		return -EINVAL;

	rc = smbchg_sec_masked_write(chip, chip->chgr_base + FCC_CMP_CFG,
			FCC_COMP_MASK, i);

	if (rc)
		dev_err(chip->dev, "Couldn't set fastchg current comp rc = %d\n",
			rc);

	return rc;
}

#define FV_CMP_CFG	0xF5
#define FV_COMP_MASK	SMB_MASK(5, 0)
static int smbchg_float_voltage_comp_set(struct smbchg_chip *chip, int code)
{
	int rc;
	u8 val;

	val = code & FV_COMP_MASK;
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + FV_CMP_CFG,
			FV_COMP_MASK, val);

	if (rc)
		dev_err(chip->dev, "Couldn't set float voltage comp rc = %d\n",
			rc);

	return rc;
}

#define VFLOAT_CFG_REG			0xF4
#define MIN_FLOAT_MV			3600
#define MAX_FLOAT_MV			4500
#define VFLOAT_MASK			SMB_MASK(5, 0)

#define MID_RANGE_FLOAT_MV_MIN		3600
#define MID_RANGE_FLOAT_MIN_VAL		0x05
#define MID_RANGE_FLOAT_STEP_MV		20

#define HIGH_RANGE_FLOAT_MIN_MV		4340
#define HIGH_RANGE_FLOAT_MIN_VAL	0x2A
#define HIGH_RANGE_FLOAT_STEP_MV	10

#define VHIGH_RANGE_FLOAT_MIN_MV	4360
#define VHIGH_RANGE_FLOAT_MIN_VAL	0x2C
#define VHIGH_RANGE_FLOAT_STEP_MV	20
static int smbchg_float_voltage_set(struct smbchg_chip *chip, int vfloat_mv)
{
	int rc, delta;
	u8 temp;

	if ((vfloat_mv < MIN_FLOAT_MV) || (vfloat_mv > MAX_FLOAT_MV)) {
		dev_err(chip->dev, "bad float voltage mv =%d asked to set\n",
					vfloat_mv);
		return -EINVAL;
	}

	if (vfloat_mv <= HIGH_RANGE_FLOAT_MIN_MV) {
		/* mid range */
		delta = vfloat_mv - MID_RANGE_FLOAT_MV_MIN;
		temp = MID_RANGE_FLOAT_MIN_VAL + delta
				/ MID_RANGE_FLOAT_STEP_MV;
		vfloat_mv -= delta % MID_RANGE_FLOAT_STEP_MV;
	} else if (vfloat_mv <= VHIGH_RANGE_FLOAT_MIN_MV) {
		/* high range */
		delta = vfloat_mv - HIGH_RANGE_FLOAT_MIN_MV;
		temp = HIGH_RANGE_FLOAT_MIN_VAL + delta
				/ HIGH_RANGE_FLOAT_STEP_MV;
		vfloat_mv -= delta % HIGH_RANGE_FLOAT_STEP_MV;
	} else {
		/* very high range */
		delta = vfloat_mv - VHIGH_RANGE_FLOAT_MIN_MV;
		temp = VHIGH_RANGE_FLOAT_MIN_VAL + delta
				/ VHIGH_RANGE_FLOAT_STEP_MV;
		vfloat_mv -= delta % VHIGH_RANGE_FLOAT_STEP_MV;
	}

	rc = smbchg_sec_masked_write(chip, chip->chgr_base + VFLOAT_CFG_REG,
			VFLOAT_MASK, temp);

	if (rc)
		dev_err(chip->dev, "Couldn't set float voltage rc = %d\n", rc);
#ifdef QCOM_BASE
	else
		chip->vfloat_mv = vfloat_mv;
#endif
	return rc;
}

static int smbchg_float_voltage_get(struct smbchg_chip *chip)
{
	return chip->vfloat_mv;
}

#define SFT_CFG				0xFD
#define SFT_EN_MASK			SMB_MASK(5, 4)
#define SFT_TO_MASK			SMB_MASK(3, 2)
#define PRECHG_SFT_TO_MASK		SMB_MASK(1, 0)
#define SFT_TIMER_DISABLE_BIT		BIT(5)
#define PRECHG_SFT_TIMER_DISABLE_BIT	BIT(4)
#define SAFETY_TIME_MINUTES_SHIFT	2
static int smbchg_safety_timer_enable(struct smbchg_chip *chip, bool enable)
{
	int rc;
	u8 reg;

	if (!chip->safety_timer_en)
		return 0;

	if (enable)
		reg = 0;
	else
		reg = SFT_TIMER_DISABLE_BIT | PRECHG_SFT_TIMER_DISABLE_BIT;

	rc = smbchg_sec_masked_write(chip, chip->chgr_base + SFT_CFG,
			SFT_EN_MASK, reg);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't %s safety timer rc = %d\n",
			enable ? "enable" : "disable", rc);
		return rc;
	}
	chip->safety_timer_en = enable;
	return 0;
}

static int smbchg_battery_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0;
	bool unused;
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		smbchg_battchg_en(chip, val->intval,
				REASON_BATTCHG_USER, &unused);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		smbchg_usb_en(chip, val->intval, REASON_USER);
		smbchg_dc_en(chip, val->intval, REASON_USER);
		chip->chg_enabled = val->intval;
		schedule_work(&chip->usb_set_online_work);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (chip->test_mode)
			chip->test_mode_soc = val->intval;
		power_supply_changed(&chip->batt_psy);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		/* Make charge rate and USB input throttling mutually
		   exclusive for now */
		if (chip->chg_thermal_mitigation)
			smbchg_chg_system_temp_level_set(chip, val->intval);
		else
			smbchg_system_temp_level_set(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		rc = smbchg_set_fastchg_current(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		rc = smbchg_float_voltage_set(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_SAFETY_TIMER_ENABLE:
		rc = smbchg_safety_timer_enable(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		chip->hotspot_temp = val->intval;
		smbchg_stay_awake(chip, PM_HEARTBEAT);
		cancel_delayed_work(&chip->heartbeat_work);
		schedule_delayed_work(&chip->heartbeat_work,
				      msecs_to_jiffies(0));
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chip->test_mode)
			chip->test_mode_temp = val->intval;
		cancel_delayed_work(&chip->heartbeat_work);
		schedule_delayed_work(&chip->heartbeat_work,
					msecs_to_jiffies(0));
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smbchg_battery_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_SAFETY_TIMER_ENABLE:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

static int smbchg_battery_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = get_prop_batt_status(chip);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = get_prop_batt_present(chip);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		val->intval = smcghg_is_battchg_en(chip, REASON_BATTCHG_USER);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->chg_enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = get_prop_charge_type(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = smbchg_float_voltage_get(chip);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = get_prop_batt_health(chip);
		val->intval = chip->temp_state;
		if (val->intval ==  POWER_SUPPLY_HEALTH_WARM) {
			if (chip->ext_high_temp)
				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			else
				val->intval = POWER_SUPPLY_HEALTH_GOOD;
		}

		if (val->intval ==  POWER_SUPPLY_HEALTH_COOL) {
			if (chip->ext_high_temp)
				val->intval = POWER_SUPPLY_HEALTH_COLD;
			else
				val->intval = POWER_SUPPLY_HEALTH_GOOD;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_FLASH_CURRENT_MAX:
		val->intval = smbchg_calc_max_flash_current(chip);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		val->intval = chip->fastchg_current_ma * 1000;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		/* Make charge rate and USB input throttling mutually
		   exclusive for now */
		if (chip->chg_thermal_mitigation)
			val->intval = chip->chg_therm_lvl_sel;
		else
			val->intval = chip->therm_lvl_sel;
		break;
	case POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS:
		/* Make charge rate and USB input throttling mutually
		   exclusive for now */
		if (chip->chg_thermal_mitigation)
			val->intval = chip->chg_thermal_levels;
		else
			val->intval = chip->thermal_levels;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_MAX:
		val->intval = smbchg_get_aicl_level_ma(chip) * 1000;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED:
		val->intval = (int)chip->aicl_complete;
		break;
	case POWER_SUPPLY_PROP_TEMP_HOTSPOT:
		val->intval = chip->hotspot_temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_RATE:
		smbchg_rate_check(chip);
		val->intval = chip->charger_rate;
		break;
	/* properties from fg */
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_batt_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_batt_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = get_prop_charge_counter(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_prop_batt_voltage_now(chip);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (chip->test_mode && !(chip->test_mode_temp < -350)
		    && !(chip->test_mode_temp > 1250))
			val->intval = chip->test_mode_temp;
		else
			val->intval = get_prop_batt_temp(chip);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = get_prop_batt_voltage_max_design(chip);
		break;
	case POWER_SUPPLY_PROP_SAFETY_TIMER_ENABLE:
		val->intval = chip->safety_timer_en;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = get_prop_cycle_count(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = get_prop_charge_full(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = get_prop_charge_full_design(chip);
		break;
	case POWER_SUPPLY_PROP_AGE:
		val->intval = ((get_prop_charge_full(chip) / 10) /
			       (get_prop_charge_full_design(chip) / 1000));
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property smbchg_dc_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_NUM_SYSTEM_TEMP_LEVELS,
};

static int smbchg_dc_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	int rc = 0;
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		rc = smbchg_dc_en(chip, val->intval, REASON_POWER_SUPPLY);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		rc = smbchg_set_dc_current_max(chip, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		smbchg_dc_system_temp_level_set(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int smbchg_dc_get_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       union power_supply_propval *val)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, dc_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		if ((chip->temp_state == POWER_SUPPLY_HEALTH_OVERHEAT) ||
		    (chip->temp_state == POWER_SUPPLY_HEALTH_COLD))
			val->intval = 0;
		else
			val->intval = is_dc_present(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = chip->dc_suspended == 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* return if dc is charging the battery */
		if ((chip->temp_state == POWER_SUPPLY_HEALTH_OVERHEAT) ||
		    (chip->temp_state == POWER_SUPPLY_HEALTH_COLD))
			val->intval = 0;
		else
			val->intval = (smbchg_get_pwr_path(chip) == PWR_PATH_DC)
				&& (get_prop_batt_status(chip)
				    == POWER_SUPPLY_STATUS_CHARGING);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chip->dc_max_current_ma * 1000;
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

static int smbchg_dc_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

#define USBIN_SUSPEND_SRC_BIT		BIT(6)
#ifdef QCOM_BASE
static void smbchg_unknown_battery_en(struct smbchg_chip *chip, bool en)
{
	int rc;

	if (en == chip->battery_unknown || chip->charge_unknown_battery)
		return;

	chip->battery_unknown = en;
	rc = smbchg_sec_masked_write(chip,
		chip->usb_chgpth_base + CHGPTH_CFG,
		USBIN_SUSPEND_SRC_BIT, en ? 0 : USBIN_SUSPEND_SRC_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usb_chgpth cfg rc=%d\n", rc);
		return;
	}
}
#endif

static void smbchg_vfloat_adjust_check(struct smbchg_chip *chip)
{
	if (!chip->use_vfloat_adjustments)
		return;

	smbchg_stay_awake(chip, PM_REASON_VFLOAT_ADJUST);
	pr_smb(PR_STATUS, "Starting vfloat adjustments\n");
	schedule_delayed_work(&chip->vfloat_adjust_work, 0);
}

#define FV_STS_REG			0xC
#define AICL_INPUT_STS_BIT		BIT(6)
static bool smbchg_is_input_current_limited(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->chgr_base + FV_STS_REG, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read FV_STS rc=%d\n", rc);
		return false;
	}

	return !!(reg & AICL_INPUT_STS_BIT);
}

#define SW_ESR_PULSE_MS			1500
static void smbchg_cc_esr_wa_check(struct smbchg_chip *chip)
{
	int rc, esr_count;

	if (!is_usb_present(chip) && !is_dc_present(chip)) {
		pr_smb(PR_STATUS, "No inputs present, skipping\n");
		return;
	}

	if (get_prop_charge_type(chip) != POWER_SUPPLY_CHARGE_TYPE_FAST) {
		pr_smb(PR_STATUS, "Not in fast charge, skipping\n");
		return;
	}

	if (!smbchg_is_input_current_limited(chip)) {
		pr_smb(PR_STATUS, "Not input current limited, skipping\n");
		return;
	}

	set_property_on_fg(chip, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_ESR_COUNT, &esr_count);
	if (rc) {
		pr_smb(PR_STATUS,
			"could not read ESR counter rc = %d\n", rc);
		return;
	}

	/*
	 * The esr_count is counting down the number of fuel gauge cycles
	 * before a ESR pulse is needed.
	 *
	 * After a successful ESR pulse, this count is reset to some
	 * high number like 28. If this reaches 0, then the fuel gauge
	 * hardware should force a ESR pulse.
	 *
	 * However, if the device is in constant current charge mode while
	 * being input current limited, the ESR pulse will not affect the
	 * battery current, so the measurement will fail.
	 *
	 * As a failsafe, force a manual ESR pulse if this value is read as
	 * 0.
	 */
	if (esr_count != 0) {
		pr_smb(PR_STATUS, "ESR count is not zero, skipping\n");
		return;
	}

	pr_smb(PR_STATUS, "Lowering charge current for ESR pulse\n");
	smbchg_stay_awake(chip, PM_ESR_PULSE);
	smbchg_sw_esr_pulse_en(chip, true);
	msleep(SW_ESR_PULSE_MS);
	pr_smb(PR_STATUS, "Raising charge current for ESR pulse\n");
	smbchg_relax(chip, PM_ESR_PULSE);
	smbchg_sw_esr_pulse_en(chip, false);
}

static void smbchg_soc_changed(struct smbchg_chip *chip)
{
	smbchg_cc_esr_wa_check(chip);
}

#define DC_AICL_CFG			0xF3
#define MISC_TRIM_OPT_15_8		0xF5
#define USB_AICL_DEGLITCH_MASK		(BIT(5) | BIT(4) | BIT(3))
#define USB_AICL_DEGLITCH_SHORT		(BIT(5) | BIT(4) | BIT(3))
#define USB_AICL_DEGLITCH_LONG		0
#define DC_AICL_DEGLITCH_MASK		(BIT(5) | BIT(4) | BIT(3))
#define DC_AICL_DEGLITCH_SHORT		(BIT(5) | BIT(4) | BIT(3))
#define DC_AICL_DEGLITCH_LONG		0
#define AICL_RERUN_MASK			(BIT(5) | BIT(4))
#define AICL_RERUN_ON			(BIT(5) | BIT(4))
#define AICL_RERUN_OFF			0

static int smbchg_hw_aicl_rerun_en(struct smbchg_chip *chip, bool en)
{
	int rc = 0;

	rc = smbchg_sec_masked_write(chip,
		chip->misc_base + MISC_TRIM_OPT_15_8,
		AICL_RERUN_MASK, en ? AICL_RERUN_ON : AICL_RERUN_OFF);
	if (rc)
		pr_err("Couldn't write to MISC_TRIM_OPTIONS_15_8 rc=%d\n",
			rc);
	return rc;
}

static int smbchg_aicl_config(struct smbchg_chip *chip)
{
	int rc = 0;

	rc = smbchg_sec_masked_write(chip,
		chip->usb_chgpth_base + USB_AICL_CFG,
		USB_AICL_DEGLITCH_MASK, USB_AICL_DEGLITCH_LONG);
	if (rc) {
		pr_err("Couldn't write to USB_AICL_CFG rc=%d\n", rc);
		return rc;
	}
	rc = smbchg_sec_masked_write(chip,
		chip->dc_chgpth_base + DC_AICL_CFG,
		DC_AICL_DEGLITCH_MASK, DC_AICL_DEGLITCH_LONG);
	if (rc) {
		pr_err("Couldn't write to DC_AICL_CFG rc=%d\n", rc);
		return rc;
	}
	rc = smbchg_hw_aicl_rerun_en(chip, true);
	if (rc)
		pr_err("Couldn't enable AICL rerun rc= %d\n", rc);
	return rc;
}

static void smbchg_aicl_deglitch_wa_en(struct smbchg_chip *chip, bool en)
{
	int rc;

	if (chip->force_aicl_rerun)
		return;

	if (en && !chip->aicl_deglitch_short) {
		rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + USB_AICL_CFG,
			USB_AICL_DEGLITCH_MASK, USB_AICL_DEGLITCH_SHORT);
		if (rc) {
			pr_err("Couldn't write to USB_AICL_CFG rc=%d\n", rc);
			return;
		}
		rc = smbchg_sec_masked_write(chip,
			chip->dc_chgpth_base + DC_AICL_CFG,
			DC_AICL_DEGLITCH_MASK, DC_AICL_DEGLITCH_SHORT);
		if (rc) {
			pr_err("Couldn't write to DC_AICL_CFG rc=%d\n", rc);
			return;
		}
		rc = smbchg_hw_aicl_rerun_en(chip, true);
		if (rc) {
			pr_err("Couldn't enable AICL rerun rc=%d\n", rc);
			return;
		}
		pr_smb(PR_STATUS, "AICL deglitch set to short\n");
	} else if (!en && chip->aicl_deglitch_short) {
		rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + USB_AICL_CFG,
			USB_AICL_DEGLITCH_MASK, USB_AICL_DEGLITCH_LONG);
		if (rc) {
			pr_err("Couldn't write to USB_AICL_CFG rc=%d\n", rc);
			return;
		}
		rc = smbchg_sec_masked_write(chip,
			chip->dc_chgpth_base + DC_AICL_CFG,
			DC_AICL_DEGLITCH_MASK, DC_AICL_DEGLITCH_LONG);
		if (rc) {
			pr_err("Couldn't write to DC_AICL_CFG rc=%d\n", rc);
			return;
		}
		smbchg_hw_aicl_rerun_en(chip, false);
		if (rc) {
			pr_err("Couldn't disable AICL rerun rc=%d\n", rc);
			return;
		}
		pr_smb(PR_STATUS, "AICL deglitch set to normal\n");
	}
	chip->aicl_deglitch_short = en;
}

static void smbchg_aicl_deglitch_wa_check(struct smbchg_chip *chip)
{
	union power_supply_propval prop = {0,};
	int rc;
	u8 reg;
	bool low_volt_chgr = true;

	if (!is_usb_present(chip) && !is_dc_present(chip)) {
		pr_smb(PR_STATUS, "Charger removed\n");
		smbchg_aicl_deglitch_wa_en(chip, false);
		return;
	}

	if (!chip->bms_psy)
		return;

	if (is_usb_present(chip)) {
		rc = smbchg_read(chip, &reg,
				chip->usb_chgpth_base + USBIN_HVDCP_STS, 1);
		if (rc < 0) {
			pr_err("Couldn't read hvdcp status rc = %d\n", rc);
			return;
		}
		if (reg & USBIN_HVDCP_SEL_BIT)
			low_volt_chgr = false;
	} else if (is_dc_present(chip)) {
		if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIPOWER)
			low_volt_chgr = false;
		else
			low_volt_chgr = chip->low_volt_dcin;
	}

	if (!low_volt_chgr) {
		pr_smb(PR_STATUS, "High volt charger! Don't set deglitch\n");
		smbchg_aicl_deglitch_wa_en(chip, false);
		return;
	}

	/* It is possible that battery voltage went high above threshold
	 * when the charger is inserted and can go low because of system
	 * load. We shouldn't be reconfiguring AICL deglitch when this
	 * happens as it will lead to oscillation again which is being
	 * fixed here. Do it once when the battery voltage crosses the
	 * threshold (e.g. 4.2 V) and clear it only when the charger
	 * is removed.
	 */
	if (!chip->vbat_above_headroom) {
		rc = chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_MIN, &prop);
		if (rc < 0) {
			pr_err("could not read voltage_min, rc=%d\n", rc);
			return;
		}
		chip->vbat_above_headroom = !prop.intval;
	}
	smbchg_aicl_deglitch_wa_en(chip, chip->vbat_above_headroom);
}

static bool smbchg_hvdcp_det_check(struct smbchg_chip *chip)
{
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + USBIN_HVDCP_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read hvdcp status rc = %d\n", rc);
		return false;
	}

	/*
	 * If a valid HVDCP is detected, notify true only
	 * if USB is still present.
	 */
	if ((reg & USBIN_HVDCP_SEL_BIT) && is_usb_present(chip))
		return true;

	return false;
}

#define WEAK_CHRG_THRSH 450
static void smbchg_rate_check(struct smbchg_chip *chip)
{
	union power_supply_propval prop = {0,};
	int prev_chg_rate = chip->charger_rate;
	char *charge_rate[] = {
		"None", "Normal", "Weak", "Turbo"
	};

	if (!is_usb_present(chip)) {
		chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;
		return;
	}

	if (chip->usb_psy &&
	    !chip->usb_psy->get_property(chip->usb_psy,
					 POWER_SUPPLY_PROP_TYPE,
					 &prop)) {
		if ((prop.intval == POWER_SUPPLY_TYPE_USB_CDP) ||
		    (prop.intval == POWER_SUPPLY_TYPE_USB))
			chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_NORMAL;
		else if (smbchg_hvdcp_det_check(chip))
			chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_TURBO;
		else if (smbchg_get_aicl_level_ma(chip) < WEAK_CHRG_THRSH)
			chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_WEAK;
		else
			chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_NORMAL;
	} else
		chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_NORMAL;

	if (prev_chg_rate != chip->charger_rate)
		dev_err(chip->dev, "%s Charger Detected!\n",
			charge_rate[chip->charger_rate]);

}

#define UNKNOWN_BATT_TYPE	"Unknown Battery"
#define LOADING_BATT_TYPE	"Loading Battery Data"
static void smbchg_external_power_changed(struct power_supply *psy)
{
	struct smbchg_chip *chip = container_of(psy,
				struct smbchg_chip, batt_psy);
	union power_supply_propval prop = {0,};
	int rc, current_limit = 0, soc;
#ifdef QCOM_BASE
	bool en;
	bool unused;
#endif

	if (chip->bms_psy_name)
		chip->bms_psy =
			power_supply_get_by_name((char *)chip->bms_psy_name);

	smbchg_aicl_deglitch_wa_check(chip);
	if (chip->bms_psy) {
#ifdef QCOM_BASE
		chip->bms_psy->get_property(chip->bms_psy,
				POWER_SUPPLY_PROP_BATTERY_TYPE, &prop);
		en = strcmp(prop.strval, UNKNOWN_BATT_TYPE) != 0;
		smbchg_unknown_battery_en(chip, en);
		en = strcmp(prop.strval, LOADING_BATT_TYPE) != 0;
		smbchg_battchg_en(chip, en, REASON_BATTCHG_LOADING_PROFILE,
				&unused);
#endif
		soc = get_prop_batt_capacity(chip);
		if (chip->previous_soc != soc) {
			chip->previous_soc = soc;
			smbchg_soc_changed(chip);
		}
	}

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &prop);
	if (rc < 0)
		pr_smb(PR_MISC, "could not read USB charge_en, rc=%d\n",
				rc);
	else
		smbchg_usb_en(chip, prop.intval, REASON_POWER_SUPPLY);

	rc = chip->usb_psy->get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
	if (rc < 0)
		dev_err(chip->dev,
			"could not read USB current_max property, rc=%d\n", rc);
	else
		current_limit = prop.intval / 1000;

	if (smbchg_hvdcp_det_check(chip)) {
		if ((chip->stepchg_state == STEP_TAPER) ||
		    (chip->stepchg_state == STEP_FULL) ||
		    (chip->demo_mode))
			current_limit = HVDCP_ICL_TAPER;
		else
			current_limit = HVDCP_ICL_MAX;
	}

	pr_smb(PR_MISC, "current_limit = %d\n", current_limit);
	mutex_lock(&chip->current_change_lock);
	if (current_limit != chip->usb_target_current_ma) {
		pr_smb(PR_STATUS, "changed current_limit = %d\n",
				current_limit);
		chip->usb_target_current_ma = current_limit;
		rc = smbchg_set_thermal_limited_usb_current_max(chip,
				current_limit);
		if (rc < 0)
			dev_err(chip->dev,
				"Couldn't set usb current rc = %d\n", rc);
		smbchg_rerun_aicl(chip);
	}
	mutex_unlock(&chip->current_change_lock);

	smbchg_vfloat_adjust_check(chip);

	power_supply_changed(&chip->batt_psy);
}

#define OTG_EN		BIT(0)
static int smbchg_otg_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	chip->otg_retries = 0;
	rc = smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			OTG_EN, OTG_EN);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't enable OTG mode rc=%d\n", rc);
	else
		chip->otg_enable_time = ktime_get();
	pr_smb(PR_STATUS, "Enabling OTG Boost\n");
	return rc;
}

static int smbchg_otg_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
			OTG_EN, 0);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't disable OTG mode rc=%d\n", rc);
	pr_smb(PR_STATUS, "Disabling OTG Boost\n");
	return rc;
}

static int smbchg_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 reg = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_read(chip, &reg, chip->bat_if_base + CMD_CHG_REG, 1);
	if (rc < 0) {
		dev_err(chip->dev,
				"Couldn't read OTG enable bit rc=%d\n", rc);
		return rc;
	}

	return (reg & OTG_EN) ? 1 : 0;
}

struct regulator_ops smbchg_otg_reg_ops = {
	.enable		= smbchg_otg_regulator_enable,
	.disable	= smbchg_otg_regulator_disable,
	.is_enabled	= smbchg_otg_regulator_is_enable,
};

#define USBIN_CHGR_CFG			0xF1
#define USBIN_ADAPTER_9V		0x3
#define HVDCP_EN_BIT			BIT(3)
static int smbchg_external_otg_regulator_enable(struct regulator_dev *rdev)
{
	bool changed;
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_primary_usb_en(chip, false, REASON_OTG, &changed);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't suspend charger rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_read(chip, &chip->original_usbin_allowance,
			chip->usb_chgpth_base + USBIN_CHGR_CFG, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb allowance rc=%d\n", rc);
		return rc;
	}

	/*
	 * To disallow source detect and usbin_uv interrupts, set the adapter
	 * allowance to 9V, so that the audio boost operating in reverse never
	 * gets detected as a valid input
	 */
	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + CHGPTH_CFG,
				HVDCP_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable HVDCP rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + USBIN_CHGR_CFG,
				0xFF, USBIN_ADAPTER_9V);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write usb allowance rc=%d\n", rc);
		return rc;
	}

	pr_smb(PR_STATUS, "Enabling OTG Boost\n");
	return rc;
}

static int smbchg_external_otg_regulator_disable(struct regulator_dev *rdev)
{
	bool changed;
	int rc = 0;
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	rc = smbchg_primary_usb_en(chip, true, REASON_OTG, &changed);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't unsuspend charger rc=%d\n", rc);
		return rc;
	}

	/*
	 * Reenable HVDCP and set the adapter allowance back to the original
	 * value in order to allow normal USBs to be recognized as a valid
	 * input.
	 */
	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + CHGPTH_CFG,
				HVDCP_EN_BIT, HVDCP_EN_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't enable HVDCP rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip,
				chip->usb_chgpth_base + USBIN_CHGR_CFG,
				0xFF, chip->original_usbin_allowance);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write usb allowance rc=%d\n", rc);
		return rc;
	}

	pr_smb(PR_STATUS, "Disabling OTG Boost\n");
	return rc;
}

static int smbchg_external_otg_regulator_is_enable(struct regulator_dev *rdev)
{
	struct smbchg_chip *chip = rdev_get_drvdata(rdev);

	return !smbchg_primary_usb_is_en(chip, REASON_OTG);
}

struct regulator_ops smbchg_external_otg_reg_ops = {
	.enable		= smbchg_external_otg_regulator_enable,
	.disable	= smbchg_external_otg_regulator_disable,
	.is_enabled	= smbchg_external_otg_regulator_is_enable,
};

static int smbchg_regulator_init(struct smbchg_chip *chip)
{
	int rc = 0;
	struct regulator_init_data *init_data;
	struct regulator_config cfg = {};
	struct device_node *regulator_node;

	regulator_node = of_get_child_by_name(chip->dev->of_node,
			"qcom,smbcharger-boost-otg");

	init_data = of_get_regulator_init_data(chip->dev, regulator_node);
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		chip->otg_vreg.rdesc.owner = THIS_MODULE;
		chip->otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->otg_vreg.rdesc.ops = &smbchg_otg_reg_ops;
		chip->otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = regulator_node;

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

	if (rc)
		return rc;

	regulator_node = of_get_child_by_name(chip->dev->of_node,
			"qcom,smbcharger-external-otg");
	init_data = of_get_regulator_init_data(chip->dev, regulator_node);
	if (!init_data) {
		dev_err(chip->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		if (of_get_property(chip->dev->of_node,
					"otg-parent-supply", NULL))
			init_data->supply_regulator = "otg-parent";
		chip->ext_otg_vreg.rdesc.owner = THIS_MODULE;
		chip->ext_otg_vreg.rdesc.type = REGULATOR_VOLTAGE;
		chip->ext_otg_vreg.rdesc.ops = &smbchg_external_otg_reg_ops;
		chip->ext_otg_vreg.rdesc.name = init_data->constraints.name;

		cfg.dev = chip->dev;
		cfg.init_data = init_data;
		cfg.driver_data = chip;
		cfg.of_node = regulator_node;

		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		chip->ext_otg_vreg.rdev = regulator_register(
					&chip->ext_otg_vreg.rdesc, &cfg);
		if (IS_ERR(chip->ext_otg_vreg.rdev)) {
			rc = PTR_ERR(chip->ext_otg_vreg.rdev);
			chip->ext_otg_vreg.rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev,
					"external OTG reg failed, rc=%d\n", rc);
		}
	}

	return rc;
}

static void smbchg_regulator_deinit(struct smbchg_chip *chip)
{
	if (chip->otg_vreg.rdev)
		regulator_unregister(chip->otg_vreg.rdev);
	if (chip->ext_otg_vreg.rdev)
		regulator_unregister(chip->ext_otg_vreg.rdev);
}

#define REVISION1_REG			0x0
#define DIG_MINOR			0
#define DIG_MAJOR			1
#define ANA_MINOR			2
#define ANA_MAJOR			3
static int smbchg_low_icl_wa_check(struct smbchg_chip *chip)
{
	int rc = 0;
	bool enable = (get_prop_batt_status(chip)
		!= POWER_SUPPLY_STATUS_CHARGING);

	/* only execute workaround if the charger is version 1.x */
	if (chip->revision[DIG_MAJOR] > 1)
		return 0;

	mutex_lock(&chip->current_change_lock);
	pr_smb(PR_STATUS, "low icl %s -> %s\n",
			chip->low_icl_wa_on ? "on" : "off",
			enable ? "on" : "off");
	if (enable == chip->low_icl_wa_on)
		goto out;

	chip->low_icl_wa_on = enable;
	if (enable) {
		rc = smbchg_sec_masked_write(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
					USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					USBIN_LIMITED_MODE | USB51_100MA);
		if (rc)
			dev_err(chip->dev,
				"could not set low current limit: %d\n", rc);
	} else {
		rc = smbchg_set_thermal_limited_usb_current_max(chip,
						chip->usb_target_current_ma);
		if (rc)
			dev_err(chip->dev,
				"could not set current limit: %d\n", rc);
	}
out:
	mutex_unlock(&chip->current_change_lock);
	return rc;
}

static int vf_adjust_low_threshold = 5;
module_param(vf_adjust_low_threshold, int, 0644);

static int vf_adjust_high_threshold = 7;
module_param(vf_adjust_high_threshold, int, 0644);

static int vf_adjust_n_samples = 10;
module_param(vf_adjust_n_samples, int, 0644);

static int vf_adjust_max_delta_mv = 40;
module_param(vf_adjust_max_delta_mv, int, 0644);

static int vf_adjust_trim_steps_per_adjust = 1;
module_param(vf_adjust_trim_steps_per_adjust, int, 0644);

#define CENTER_TRIM_CODE		7
#define MAX_LIN_CODE			14
#define MAX_TRIM_CODE			15
#define SCALE_SHIFT			4
#define VF_TRIM_OFFSET_MASK		SMB_MASK(3, 0)
#define VF_STEP_SIZE_MV			10
#define SCALE_LSB_MV			17

static void set_max_allowed_current_ma(struct smbchg_chip *chip,
				       int current_ma)
{
	if (!chip->usb_present) {
		pr_smb(PR_STATUS, "NO allowed current, No USB\n");
		return;
	}

	chip->target_fastchg_current_ma =
		min(current_ma, chip->allowed_fastchg_current_ma);
	pr_smb(PR_STATUS, "requested=%d: allowed=%d: result=%d\n",
	       current_ma, chip->allowed_fastchg_current_ma,
	       chip->target_fastchg_current_ma);
}

static int smbchg_trim_add_steps(int prev_trim, int delta_steps)
{
	int scale_steps;
	int linear_offset, linear_scale;
	int offset_code = prev_trim & VF_TRIM_OFFSET_MASK;
	int scale_code = (prev_trim & ~VF_TRIM_OFFSET_MASK) >> SCALE_SHIFT;

	if (abs(delta_steps) > 1) {
		pr_smb(PR_STATUS,
			"Cant trim multiple steps delta_steps = %d\n",
			delta_steps);
		return prev_trim;
	}
	if (offset_code <= CENTER_TRIM_CODE)
		linear_offset = offset_code + CENTER_TRIM_CODE;
	else if (offset_code > CENTER_TRIM_CODE)
		linear_offset = MAX_TRIM_CODE - offset_code;

	if (scale_code <= CENTER_TRIM_CODE)
		linear_scale = scale_code + CENTER_TRIM_CODE;
	else if (scale_code > CENTER_TRIM_CODE)
		linear_scale = scale_code - (CENTER_TRIM_CODE + 1);

	/* check if we can accomodate delta steps with just the offset */
	if (linear_offset + delta_steps >= 0
			&& linear_offset + delta_steps <= MAX_LIN_CODE) {
		linear_offset += delta_steps;

		if (linear_offset > CENTER_TRIM_CODE)
			offset_code = linear_offset - CENTER_TRIM_CODE;
		else
			offset_code = MAX_TRIM_CODE - linear_offset;

		return (prev_trim & ~VF_TRIM_OFFSET_MASK) | offset_code;
	}

	/* changing offset cannot satisfy delta steps, change the scale bits */
	scale_steps = delta_steps > 0 ? 1 : -1;

	if (linear_scale + scale_steps < 0
			|| linear_scale + scale_steps > MAX_LIN_CODE) {
		pr_smb(PR_STATUS,
			"Cant trim scale_steps = %d delta_steps = %d\n",
			scale_steps, delta_steps);
		return prev_trim;
	}

	linear_scale += scale_steps;

	if (linear_scale > CENTER_TRIM_CODE)
		scale_code = linear_scale - CENTER_TRIM_CODE;
	else
		scale_code = linear_scale + (CENTER_TRIM_CODE + 1);
	prev_trim = (prev_trim & VF_TRIM_OFFSET_MASK)
		| scale_code << SCALE_SHIFT;

	/*
	 * now that we have changed scale which is a 17mV jump, change the
	 * offset bits (10mV) too so the effective change is just 7mV
	 */
	delta_steps = -1 * delta_steps;

	linear_offset = clamp(linear_offset + delta_steps, 0, MAX_LIN_CODE);
	if (linear_offset > CENTER_TRIM_CODE)
		offset_code = linear_offset - CENTER_TRIM_CODE;
	else
		offset_code = MAX_TRIM_CODE - linear_offset;

	return (prev_trim & ~VF_TRIM_OFFSET_MASK) | offset_code;
}

#define TRIM_14		0xFE
#define VF_TRIM_MASK	0xFF
static int smbchg_adjust_vfloat_mv_trim(struct smbchg_chip *chip,
						int delta_mv)
{
	int sign, delta_steps, rc = 0;
	u8 prev_trim, new_trim;
	int i;

	sign = delta_mv > 0 ? 1 : -1;
	delta_steps = (delta_mv + sign * VF_STEP_SIZE_MV / 2)
			/ VF_STEP_SIZE_MV;

	rc = smbchg_read(chip, &prev_trim, chip->misc_base + TRIM_14, 1);
	if (rc) {
		dev_err(chip->dev, "Unable to read trim 14: %d\n", rc);
		return rc;
	}

	for (i = 1; i <= abs(delta_steps)
			&& i <= vf_adjust_trim_steps_per_adjust; i++) {
		new_trim = (u8)smbchg_trim_add_steps(prev_trim,
				delta_steps > 0 ? 1 : -1);
		if (new_trim == prev_trim) {
			pr_smb(PR_STATUS,
				"VFloat trim unchanged from %02x\n", prev_trim);
			/* treat no trim change as an error */
			return -EINVAL;
		}

		rc = smbchg_sec_masked_write(chip, chip->misc_base + TRIM_14,
				VF_TRIM_MASK, new_trim);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't change vfloat trim rc=%d\n", rc);
		}
		pr_smb(PR_STATUS,
			"VFlt trim %02x to %02x, delta steps: %d\n",
			prev_trim, new_trim, delta_steps);
		prev_trim = new_trim;
	}

	return rc;
}

static void smbchg_vfloat_adjust_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				vfloat_adjust_work.work);
	int vbat_uv, vbat_mv, ibat_ua, rc, delta_vfloat_mv;
	bool taper, enable;

start:
	taper = (get_prop_charge_type(chip)
		== POWER_SUPPLY_CHARGE_TYPE_TAPER);
	enable = taper && (chip->parallel.current_max_ma == 0);

	if (!enable) {
		pr_smb(PR_MISC,
			"Stopping vfloat adj taper=%d parallel_ma = %d\n",
			taper, chip->parallel.current_max_ma);
		goto stop;
	}

	set_property_on_fg(chip, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &vbat_uv);
	if (rc) {
		pr_smb(PR_STATUS,
			"bms psy does not support voltage rc = %d\n", rc);
		goto stop;
	}
	vbat_mv = vbat_uv / 1000;

	if ((vbat_mv - chip->vfloat_mv) < -1 * vf_adjust_max_delta_mv) {
		pr_smb(PR_STATUS, "Skip vbat out of range: %d\n", vbat_mv);
		goto start;
	}

	rc = get_property_from_fg(chip,
			POWER_SUPPLY_PROP_CURRENT_NOW, &ibat_ua);
	if (rc) {
		pr_smb(PR_STATUS,
			"bms psy does not support current_now rc = %d\n", rc);
		goto stop;
	}

	if (ibat_ua / 1000 > -chip->iterm_ma) {
		pr_smb(PR_STATUS, "Skip ibat too high: %d\n", ibat_ua);
		goto start;
	}

	pr_smb(PR_STATUS, "sample number = %d vbat_mv = %d ibat_ua = %d\n",
		chip->n_vbat_samples,
		vbat_mv,
		ibat_ua);

	chip->max_vbat_sample = max(chip->max_vbat_sample, vbat_mv);
	chip->n_vbat_samples += 1;
	if (chip->n_vbat_samples < vf_adjust_n_samples) {
		pr_smb(PR_STATUS, "Skip %d samples; max = %d\n",
			chip->n_vbat_samples, chip->max_vbat_sample);
		goto start;
	}
	/* if max vbat > target vfloat, delta_vfloat_mv could be negative */
	delta_vfloat_mv = chip->vfloat_mv - chip->max_vbat_sample;
	pr_smb(PR_STATUS, "delta_vfloat_mv = %d, samples = %d, mvbat = %d\n",
		delta_vfloat_mv, chip->n_vbat_samples, chip->max_vbat_sample);
	/*
	 * enough valid samples has been collected, adjust trim codes
	 * based on maximum of collected vbat samples if necessary
	 */
	if (delta_vfloat_mv > vf_adjust_high_threshold
			|| delta_vfloat_mv < -1 * vf_adjust_low_threshold) {
		rc = smbchg_adjust_vfloat_mv_trim(chip, delta_vfloat_mv);
		if (rc) {
			pr_smb(PR_STATUS,
				"Stopping vfloat adj after trim adj rc = %d\n",
				 rc);
			goto stop;
		}
		chip->max_vbat_sample = 0;
		chip->n_vbat_samples = 0;
		goto start;
	}

stop:
	chip->max_vbat_sample = 0;
	chip->n_vbat_samples = 0;
	smbchg_relax(chip, PM_REASON_VFLOAT_ADJUST);
	return;
}

static int smbchg_charging_status_change(struct smbchg_chip *chip)
{
	smbchg_low_icl_wa_check(chip);
	smbchg_vfloat_adjust_check(chip);
	set_property_on_fg(chip, POWER_SUPPLY_PROP_STATUS,
			get_prop_batt_status(chip));
	return 0;
}

#define HOT_BAT_HARD_BIT	BIT(0)
#define HOT_BAT_SOFT_BIT	BIT(1)
#define COLD_BAT_HARD_BIT	BIT(2)
#define COLD_BAT_SOFT_BIT	BIT(3)
#define BAT_OV_BIT		BIT(4)
#define BAT_LOW_BIT		BIT(5)
#define BAT_MISSING_BIT		BIT(6)
#define BAT_TERM_MISSING_BIT	BIT(7)
static irqreturn_t batt_hot_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_hot = !!(reg & HOT_BAT_HARD_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t batt_cold_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_cold = !!(reg & COLD_BAT_HARD_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t batt_warm_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_warm = !!(reg & HOT_BAT_SOFT_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_cool_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_cool = !!(reg & COLD_BAT_SOFT_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	return IRQ_HANDLED;
}

static irqreturn_t batt_pres_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->bat_if_base + RT_STS, 1);
	chip->batt_present = !(reg & BAT_MISSING_BIT);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	return IRQ_HANDLED;
}

static irqreturn_t vbat_low_handler(int irq, void *_chip)
{
	pr_warn_ratelimited("vbat low\n");
	return IRQ_HANDLED;
}

static irqreturn_t chg_error_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_smb(PR_INTERRUPT, "chg-error triggered\n");
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t fastchg_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;

	pr_smb(PR_INTERRUPT, "p2f triggered\n");
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t chg_hot_handler(int irq, void *_chip)
{
	pr_warn_ratelimited("chg hot\n");
	smbchg_wipower_check(_chip);
	return IRQ_HANDLED;
}

static void smbchg_hvdcp_det_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
				struct smbchg_chip,
				hvdcp_det_work.work);
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + USBIN_HVDCP_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read hvdcp status rc = %d\n", rc);
		return;
	}

	/*
	 * If a valid HVDCP is detected, notify it to the usb_psy only
	 * if USB is still present.
	 */
	if ((reg & USBIN_HVDCP_SEL_BIT) && is_usb_present(chip)) {
		power_supply_set_supply_type(chip->usb_psy,
				POWER_SUPPLY_TYPE_USB_HVDCP);
		smbchg_aicl_deglitch_wa_check(chip);
	}
	smbchg_stay_awake(chip, PM_HEARTBEAT);
	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work, msecs_to_jiffies(0));
}

static irqreturn_t chg_term_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	return IRQ_HANDLED;
}

static irqreturn_t taper_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	taper_irq_en(chip, false);
	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_taper(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	smbchg_wipower_check(chip);
	return IRQ_HANDLED;
}

static irqreturn_t recharge_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->chgr_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	smbchg_parallel_usb_check_ok(chip);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	return IRQ_HANDLED;
}

static irqreturn_t safety_timeout_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->misc_base + RT_STS, 1);
	pr_warn_ratelimited("safety timeout rt_stat = 0x%02x\n", reg);
	if (chip->psy_registered)
		power_supply_changed(&chip->batt_psy);
	smbchg_charging_status_change(chip);
	return IRQ_HANDLED;
}

#ifdef QCOM_BASE
/**
 * power_ok_handler() - called when the switcher turns on or turns off
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating switcher turning on or off
 */
static irqreturn_t power_ok_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	u8 reg = 0;

	smbchg_read(chip, &reg, chip->misc_base + RT_STS, 1);
	pr_smb(PR_INTERRUPT, "triggered: 0x%02x\n", reg);
	return IRQ_HANDLED;
}
#endif

static int handle_dc_removal(struct smbchg_chip *chip)
{
	if (chip->dc_psy_type != -EINVAL)
		power_supply_set_online(&chip->dc_psy, chip->dc_present);

	smbchg_relax(chip, PM_WIRELESS);
	return 0;
}

static int handle_dc_insertion(struct smbchg_chip *chip)
{
	if (chip->dc_psy_type != -EINVAL)
		power_supply_set_online(&chip->dc_psy,
						chip->dc_present);
	smbchg_stay_awake(chip, PM_WIRELESS);
	return 0;
}

/**
 * dcin_uv_handler() - called when the dc voltage crosses the uv threshold
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating whether dc voltage is uv
 */
#define DCIN_UNSUSPEND_DELAY_MS		1000
static irqreturn_t dcin_uv_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool dc_present = is_dc_present(chip);

	pr_smb(PR_STATUS, "chip->dc_present = %d dc_present = %d\n",
			chip->dc_present, dc_present);

#ifdef QCOM_BASE
	if (chip->dc_present != dc_present) {
		/* dc changed */
		chip->dc_present = dc_present;
		if (chip->psy_registered)
			power_supply_changed(&chip->dc_psy);
		smbchg_charging_status_change(chip);
		smbchg_aicl_deglitch_wa_check(chip);
		chip->vbat_above_headroom = false;
	}

	smbchg_wipower_check(chip);
#endif
	mutex_lock(&chip->dc_set_present_lock);
	if (!chip->dc_present && dc_present) {
		/* dc inserted */
		chip->dc_present = dc_present;
		handle_dc_insertion(chip);
	} else if (chip->dc_present && !dc_present) {
		/* dc removed */
		chip->dc_present = dc_present;
		handle_dc_removal(chip);
	}
	mutex_unlock(&chip->dc_set_present_lock);

	return IRQ_HANDLED;
}

static void usb_insertion_work(struct work_struct *work)
{
	struct smbchg_chip *chip =
		container_of(work, struct smbchg_chip,
			     usb_insertion_work.work);
	int rc;

	rc = smbchg_force_apsd(chip);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't rerun apsd rc = %d\n", rc);

	smbchg_relax(chip, PM_CHARGER);
}

static void handle_usb_removal(struct smbchg_chip *chip)
{
	int rc;

	cancel_delayed_work(&chip->usb_insertion_work);
	chip->apsd_rerun_cnt = 0;

	smbchg_aicl_deglitch_wa_check(chip);
	if (chip->force_aicl_rerun) {
		rc = smbchg_hw_aicl_rerun_en(chip, true);
		if (rc)
			pr_err("Error enabling AICL rerun rc= %d\n",
				rc);
	}

	/* Clear the OV detected status set before */
	if (chip->usb_ov_det)
		chip->usb_ov_det = false;
	if (chip->usb_psy) {
		pr_smb(PR_MISC, "setting usb psy type = %d\n",
				POWER_SUPPLY_TYPE_UNKNOWN);
		pr_smb(PR_MISC, "setting usb psy present = %d\n",
				chip->usb_present);
		power_supply_set_supply_type(chip->usb_psy,
				POWER_SUPPLY_TYPE_UNKNOWN);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		schedule_work(&chip->usb_set_online_work);
		rc = power_supply_set_health_state(chip->usb_psy,
				POWER_SUPPLY_HEALTH_UNKNOWN);
		if (rc)
			pr_smb(PR_STATUS,
				"usb psy does not allow updating prop %d rc = %d\n",
				POWER_SUPPLY_HEALTH_UNKNOWN, rc);
	}

	if (chip->parallel.avail && chip->aicl_done_irq
			&& chip->enable_aicl_wake) {
		disable_irq_wake(chip->aicl_done_irq);
		chip->enable_aicl_wake = false;
	}
	chip->charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;
	chip->vbat_above_headroom = false;
	chip->stepchg_state = STEP_NONE;
	chip->vfloat_mv = chip->stepchg_max_voltage_mv;
	chip->aicl_wait_retries = 0;
	set_max_allowed_current_ma(chip, chip->stepchg_current_ma);
	smbchg_set_temp_chgpath(chip, chip->temp_state);
	smbchg_stay_awake(chip, PM_HEARTBEAT);
	smbchg_relax(chip, PM_CHARGER);
	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work, msecs_to_jiffies(0));
}

#define HVDCP_NOTIFY_MS		2500
static void handle_usb_insertion(struct smbchg_chip *chip)
{
	enum power_supply_type usb_supply_type;
	int rc, type;
	char *usb_type_name = "null";
	u8 reg = 0;

	smbchg_stay_awake(chip, PM_CHARGER);

	cancel_delayed_work(&chip->usb_insertion_work);

	/* usb inserted */
	rc = smbchg_read(chip, &reg, chip->misc_base + IDEV_STS, 1);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't read status 5 rc = %d\n", rc);
	type = get_type(reg);
	usb_type_name = get_usb_type_name(type);
	usb_supply_type = get_usb_supply_type(type);

	/* Rerun APSD 1 sec later */
	if ((usb_supply_type == POWER_SUPPLY_TYPE_USB) &&
	    !chip->apsd_rerun_cnt && !chip->factory_mode) {
		dev_info(chip->dev, "HW Detected SDP!\n");
		chip->apsd_rerun_cnt++;
		chip->usb_present = 0;
		schedule_delayed_work(&chip->usb_insertion_work,
				      msecs_to_jiffies(1000));
		return;
	}

	chip->apsd_rerun_cnt = 0;

	pr_smb(PR_STATUS, "inserted %s, usb psy type = %d stat_5 = 0x%02x\n",
			usb_type_name, usb_supply_type, reg);
	smbchg_aicl_deglitch_wa_check(chip);
	if (chip->usb_psy) {
		pr_smb(PR_MISC, "setting usb psy type = %d\n",
				usb_supply_type);
		power_supply_set_supply_type(chip->usb_psy, usb_supply_type);
		pr_smb(PR_MISC, "setting usb psy present = %d\n",
				chip->usb_present);
		power_supply_set_present(chip->usb_psy, chip->usb_present);
		/* Notify the USB psy if OV condition is not present */
		if (!chip->usb_ov_det) {
			rc = power_supply_set_health_state(chip->usb_psy,
					POWER_SUPPLY_HEALTH_GOOD);
			if (rc)
				pr_smb(PR_STATUS,
					"usb psy does not allow updating prop %d rc = %d\n",
					POWER_SUPPLY_HEALTH_GOOD, rc);
		}
		schedule_work(&chip->usb_set_online_work);
		schedule_delayed_work(&chip->hvdcp_det_work,
					msecs_to_jiffies(HVDCP_NOTIFY_MS));
	}

	chip->charger_rate =  POWER_SUPPLY_CHARGE_RATE_NORMAL;

	if (chip->parallel.avail && chip->aicl_done_irq
			&& !chip->enable_aicl_wake) {
		rc = enable_irq_wake(chip->aicl_done_irq);
		chip->enable_aicl_wake = true;
	}
	smbchg_stay_awake(chip, PM_HEARTBEAT);
	cancel_delayed_work(&chip->heartbeat_work);
	schedule_delayed_work(&chip->heartbeat_work, msecs_to_jiffies(0));
}

/**
 * usbin_ov_handler() - this is called when an overvoltage condition occurs
 * @chip: pointer to smbchg_chip chip
 */
static irqreturn_t usbin_ov_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	int rc;
	u8 reg;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb rt status rc = %d\n", rc);
		goto out;
	}

	/* OV condition is detected. Notify it to USB psy */
	if (reg & USBIN_OV_BIT) {
		chip->usb_ov_det = true;
		if (chip->usb_psy) {
			rc = power_supply_set_health_state(chip->usb_psy,
					POWER_SUPPLY_HEALTH_OVERVOLTAGE);
			if (rc)
				pr_smb(PR_STATUS,
					"usb psy does not allow updating prop %d rc = %d\n",
					POWER_SUPPLY_HEALTH_OVERVOLTAGE, rc);
		}
	} else {
		chip->usb_ov_det = false;
		/* If USB is present, then handle the USB insertion */
		if (is_usb_present(chip)) {
			mutex_lock(&chip->usb_set_present_lock);
			handle_usb_insertion(chip);
			mutex_unlock(&chip->usb_set_present_lock);
		}
	}
out:
	return IRQ_HANDLED;
}

/**
 * usbin_uv_handler() - this is called when USB charger is removed
 * @chip: pointer to smbchg_chip chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static irqreturn_t usbin_uv_handler(int irq, void *_chip)
{
#ifdef QCOM_BASE
	int rc;
	u8 reg;
	union power_supply_propval prop = {0, };
#endif
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);

	pr_smb(PR_STATUS, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);
#ifdef QCOM_BASE
	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb rt status rc = %d\n", rc);
		goto out;
	}
	reg &= USBIN_UV_BIT;

	if (reg && chip->usb_psy && !chip->usb_psy->get_property(chip->usb_psy,
							POWER_SUPPLY_PROP_TYPE,
							&prop)) {
		if ((prop.intval == POWER_SUPPLY_TYPE_USB_HVDCP) ||
			(prop.intval == POWER_SUPPLY_TYPE_USB_DCP)) {
			if (chip->usb_present && !usb_present) {
				/* DCP or HVDCP removed */
				chip->usb_present = usb_present;
				handle_usb_removal(chip);
				chip->aicl_irq_count = 0;
			}
		}
	}
	smbchg_wipower_check(chip);
out:
#endif
	return IRQ_HANDLED;
}

/**
 * src_detect_handler() - this is called on rising edge when USB charger type
 *			is detected and on falling edge when USB voltage falls
 *			below the coarse detect voltage(1V), use it for
 *			handling USB charger insertion and CDP or SDP removal
 * @chip: pointer to smbchg_chip
 * @rt_stat: the status bit indicating chg insertion/removal
 */
static irqreturn_t src_detect_handler(int irq, void *_chip)
{
	int rc;
	u8 reg;

	struct smbchg_chip *chip = _chip;
	bool usb_present;

	pr_smb(PR_STATUS, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev,
			"Couldn't read src_det rt status rc = %d\n", rc);
		return IRQ_HANDLED;
	}

	usb_present = !!(reg & USBIN_SRC_DET_BIT);

	pr_smb(PR_STATUS, "chip->usb_present = %d usb_present = %d\n",
			chip->usb_present, usb_present);

	mutex_lock(&chip->usb_set_present_lock);
	if (!chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
			handle_usb_insertion(chip);
	} else if (chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		handle_usb_removal(chip);
	}
	mutex_unlock(&chip->usb_set_present_lock);
	return IRQ_HANDLED;
}

/**
 * otg_oc_handler() - called when the usb otg goes over current
 */
#define NUM_OTG_RETRIES			5
#define OTG_OC_RETRY_DELAY_US		50000
static irqreturn_t otg_oc_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	s64 elapsed_us = ktime_us_delta(ktime_get(), chip->otg_enable_time);

	if (elapsed_us > OTG_OC_RETRY_DELAY_US)
		chip->otg_retries = 0;

	pr_smb(PR_INTERRUPT, "triggered\n");
	/*
	 * Due to a HW bug in the PMI8994 charger, the current inrush that
	 * occurs when connecting certain OTG devices can cause the OTG
	 * overcurrent protection to trip.
	 *
	 * The work around is to try reenabling the OTG when getting an
	 * overcurrent interrupt once.
	 */
	if (chip->otg_retries < NUM_OTG_RETRIES) {
		chip->otg_retries += 1;
		pr_smb(PR_STATUS,
			"Retrying OTG enable. Try #%d, elapsed_us %lld\n",
						chip->otg_retries, elapsed_us);
		smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
							OTG_EN, 0);
		msleep(20);
		smbchg_masked_write(chip, chip->bat_if_base + CMD_CHG_REG,
							OTG_EN, OTG_EN);
		chip->otg_enable_time = ktime_get();
	}
	return IRQ_HANDLED;
}

/**
 * otg_fail_handler() - called when the usb otg fails
 * (when vbat < OTG UVLO threshold)
 */
static irqreturn_t otg_fail_handler(int irq, void *_chip)
{
	pr_smb(PR_INTERRUPT, "triggered\n");
	return IRQ_HANDLED;
}

static int get_current_time(unsigned long *now_tm_sec)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int rc;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
			__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Error reading rtc device (%s) : %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
			CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, now_tm_sec);

close_time:
	rtc_class_close(rtc);
	return rc;
}

#define AICL_IRQ_LIMIT_SECONDS	60
#define AICL_IRQ_LIMIT_COUNT	25
/**
 * aicl_done_handler() - called when the usb AICL algorithm is finished
 *			and a current is set.
 */
static irqreturn_t aicl_done_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool usb_present = is_usb_present(chip);
	bool bad_charger = false;
	int rc;
	u8 reg;
	long elapsed_seconds;
	unsigned long now_seconds;

	pr_smb(PR_INTERRUPT, "Aicl triggered icl:%d c:%d dgltch:%d first:%ld\n",
			smbchg_get_aicl_level_ma(chip),
			chip->aicl_irq_count, chip->aicl_deglitch_short,
			chip->first_aicl_seconds);

	rc = smbchg_read(chip, &reg,
			chip->usb_chgpth_base + ICL_STS_1_REG, 1);
	if (!rc)
		chip->aicl_complete = reg & AICL_STS_BIT;
	else
		chip->aicl_complete = false;

	if (chip->aicl_deglitch_short || chip->force_aicl_rerun) {
		if (!chip->aicl_irq_count)
			get_current_time(&chip->first_aicl_seconds);

		chip->aicl_irq_count++;

		if (chip->aicl_irq_count > AICL_IRQ_LIMIT_COUNT) {
			get_current_time(&now_seconds);
			elapsed_seconds = now_seconds
					- chip->first_aicl_seconds;
			pr_smb(PR_INTERRUPT, "elp:%ld first:%ld now:%ld c=%d\n",
				elapsed_seconds, chip->first_aicl_seconds,
				now_seconds, chip->aicl_irq_count);
			if (elapsed_seconds <= AICL_IRQ_LIMIT_SECONDS) {
				pr_smb(PR_INTERRUPT, "Disable AICL rerun\n");
				/*
				 * Disable AICL rerun since many interrupts were
				 * triggered in a short time
				 */
				rc = smbchg_hw_aicl_rerun_en(chip, false);
				if (rc)
					pr_err("Couldn't turn off AICL rerun rc:%d\n",
						rc);
				bad_charger = true;
			}
			chip->aicl_irq_count = 0;
		} else if ((get_prop_charge_type(chip) ==
				POWER_SUPPLY_CHARGE_TYPE_FAST) &&
					(reg & AICL_SUSP_BIT)) {
			bad_charger = true;
		}
		if (bad_charger) {
			rc = power_supply_set_health_state(chip->usb_psy,
					POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
			if (rc)
				pr_err("Couldn't set health on usb psy rc:%d\n",
					rc);
		}
	}

	if (usb_present)
		smbchg_parallel_usb_check_ok(chip);

	if (chip->aicl_complete) {
		smbchg_rate_check(chip);
		power_supply_changed(&chip->batt_psy);
	}

	return IRQ_HANDLED;
}

/**
 * usbid_change_handler() - called when the usb RID changes.
 * This is used mostly for detecting OTG
 */
static irqreturn_t usbid_change_handler(int irq, void *_chip)
{
	struct smbchg_chip *chip = _chip;
	bool otg_present;

	if (chip->usbid_disabled)
		return IRQ_HANDLED;

	pr_smb(PR_INTERRUPT, "triggered\n");

	/*
	 * After the falling edge of the usbid change interrupt occurs,
	 * there may still be some time before the ADC conversion for USB RID
	 * finishes in the fuel gauge. In the worst case, this could be up to
	 * 15 ms.
	 *
	 * Sleep for 20 ms (minimum msleep time) to wait for the conversion to
	 * finish and the USB RID status register to be updated before trying
	 * to detect OTG insertions.
	 */
	msleep(20);
	otg_present = is_otg_present(chip);
	if (chip->usb_psy)
		power_supply_set_usb_otg(chip->usb_psy, otg_present ? 1 : 0);
	if (otg_present)
		pr_smb(PR_STATUS, "OTG detected\n");

	return IRQ_HANDLED;
}

static int determine_initial_status(struct smbchg_chip *chip)
{
	/*
	 * It is okay to read the interrupt status here since
	 * interrupts aren't requested. reading interrupt status
	 * clears the interrupt so be careful to read interrupt
	 * status only in interrupt handling code
	 */

	batt_pres_handler(0, chip);
	batt_hot_handler(0, chip);
	batt_warm_handler(0, chip);
	batt_cool_handler(0, chip);
	batt_cold_handler(0, chip);
	chg_term_handler(0, chip);
	usbid_change_handler(0, chip);
	src_detect_handler(0, chip);
	smbchg_charging_en(chip, 0);
#ifdef QCOM_BASE
	mutex_lock(&chip->usb_set_present_lock);
	chip->usb_present = is_usb_present(chip);
	if (chip->usb_present)
		handle_usb_insertion(chip);
	else
		handle_usb_removal(chip);
	mutex_unlock(&chip->usb_set_present_lock);
#endif
	mutex_lock(&chip->dc_set_present_lock);
	chip->dc_present = is_dc_present(chip);
	if (chip->dc_present)
		handle_dc_insertion(chip);
	else
		handle_dc_removal(chip);
	mutex_unlock(&chip->dc_set_present_lock);

	return 0;
}

static int prechg_time[] = {
	24,
	48,
	96,
	192,
};
static int chg_time[] = {
	192,
	384,
	768,
	1536,
};

enum bpd_type {
	BPD_TYPE_BAT_NONE,
	BPD_TYPE_BAT_ID,
	BPD_TYPE_BAT_THM,
	BPD_TYPE_BAT_THM_BAT_ID,
	BPD_TYPE_DEFAULT,
};

static const char * const bpd_label[] = {
	[BPD_TYPE_BAT_NONE]		= "bpd_none",
	[BPD_TYPE_BAT_ID]		= "bpd_id",
	[BPD_TYPE_BAT_THM]		= "bpd_thm",
	[BPD_TYPE_BAT_THM_BAT_ID]	= "bpd_thm_id",
};

static inline int get_bpd(const char *name)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(bpd_label); i++) {
		if (strcmp(bpd_label[i], name) == 0)
			return i;
	}
	return -EINVAL;
}

#define DC_CHGR_CFG			0xF1
#define CCMP_CFG			0xFA
#define CCMP_CFG_MASK			0xFF
#define CCMP_CFG_DIS_ALL_TEMP		0x20
#define CHGR_CFG1			0xFB
#define RECHG_THRESHOLD_SRC_BIT		BIT(1)
#define TERM_I_SRC_BIT			BIT(2)
#define TERM_SRC_FG			BIT(2)
#define CHGR_CFG2			0xFC
#define CHG_INHIB_CFG_REG		0xF7
#define CHG_INHIBIT_50MV_VAL		0x00
#define CHG_INHIBIT_100MV_VAL		0x01
#define CHG_INHIBIT_200MV_VAL		0x02
#define CHG_INHIBIT_300MV_VAL		0x03
#define CHG_INHIBIT_MASK		0x03
#define USE_REGISTER_FOR_CURRENT	BIT(2)
#define CHG_EN_SRC_BIT			BIT(7)
#define CHG_EN_COMMAND_BIT		BIT(6)
#define P2F_CHG_TRAN			BIT(5)
#define I_TERM_BIT			BIT(3)
#define AUTO_RECHG_BIT			BIT(2)
#define CHARGER_INHIBIT_BIT		BIT(0)
#define CFG_TCC_REG			0xF9
#define CHG_ITERM_50MA			0x1
#define CHG_ITERM_100MA			0x2
#define CHG_ITERM_150MA			0x3
#define CHG_ITERM_200MA			0x4
#define CHG_ITERM_250MA			0x5
#define CHG_ITERM_300MA			0x0
#define CHG_ITERM_500MA			0x6
#define CHG_ITERM_600MA			0x7
#define CHG_ITERM_MASK			SMB_MASK(2, 0)
#define USB51_COMMAND_POL		BIT(2)
#define USB51AC_CTRL			BIT(1)
#define TR_8OR32B			0xFE
#define BUCK_8_16_FREQ_BIT		BIT(0)
#define BM_CFG				0xF3
#define BATT_MISSING_ALGO_BIT		BIT(2)
#define BMD_PIN_SRC_MASK		SMB_MASK(1, 0)
#define PIN_SRC_SHIFT			0
#define CHGR_CFG			0xFF
#define RCHG_LVL_BIT			BIT(0)
#define CFG_AFVC			0xF6
#define VFLOAT_COMP_ENABLE_MASK		SMB_MASK(2, 0)
#define TR_RID_REG			0xFA
#define FG_INPUT_FET_DELAY_BIT		BIT(3)
#define TRIM_OPTIONS_7_0		0xF6
#define INPUT_MISSING_POLLER_EN_BIT	BIT(3)
#define AICL_WL_SEL_CFG			0xF5
#define AICL_WL_SEL_MASK		SMB_MASK(1, 0)
#define AICL_WL_SEL_45S		0
#define CHGR_CCMP_CFG			0xFA
#define JEITA_TEMP_HARD_LIMIT_BIT	BIT(5)
#define USBIN_ALLOW_MASK		SMB_MASK(2, 0)
#define USBIN_ALLOW_5V			0x0
#define USBIN_ALLOW_5V_9V		0x1
#define USBIN_ALLOW_5V_TO_9V		0x2
#define USBIN_ALLOW_9V			0x3
#define USBIN_ALLOW_5V_UNREG		0x4
#define USBIN_ALLOW_5V_9V_UNREG		0x5
#define DCIN_ALLOW_5V_TO_9V		0x2
static int smbchg_hw_init(struct smbchg_chip *chip)
{
	int rc, i;
	u8 reg, mask;

	rc = smbchg_read(chip, chip->revision,
			chip->misc_base + REVISION1_REG, 4);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read revision rc=%d\n",
				rc);
		return rc;
	}
	pr_smb(PR_STATUS, "Charger Revision DIG: %d.%d; ANA: %d.%d\n",
			chip->revision[DIG_MAJOR], chip->revision[DIG_MINOR],
			chip->revision[ANA_MAJOR], chip->revision[ANA_MINOR]);

	rc = smbchg_sec_masked_write(chip,
			chip->dc_chgpth_base + AICL_WL_SEL_CFG,
			AICL_WL_SEL_MASK, AICL_WL_SEL_45S);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set AICL rerun timer rc=%d\n",
				rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + TR_RID_REG,
			FG_INPUT_FET_DELAY_BIT, FG_INPUT_FET_DELAY_BIT);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable fg input fet delay rc=%d\n",
				rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip, chip->misc_base + TRIM_OPTIONS_7_0,
			INPUT_MISSING_POLLER_EN_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't disable input missing poller rc=%d\n",
				rc);
		return rc;
	}

	/*
	 * Do not force using current from the register i.e. use auto
	 * power source detect (APSD) mA ratings for the initial current values.
	 *
	 * If this is set, AICL will not rerun at 9V for HVDCPs
	 */
	rc = smbchg_masked_write(chip, chip->usb_chgpth_base + CMD_IL,
			USE_REGISTER_FOR_CURRENT, 0);

	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set input limit cmd rc=%d\n", rc);
		return rc;
	}

	/*
	 * set chg en by cmd register, set chg en by writing bit 1,
	 * enable auto pre to fast, enable auto recharge by default.
	 * enable current termination and charge inhibition based on
	 * the device tree configuration.
	 */
	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG2,
			CHG_EN_SRC_BIT | CHG_EN_COMMAND_BIT | P2F_CHG_TRAN
			| I_TERM_BIT | AUTO_RECHG_BIT | CHARGER_INHIBIT_BIT,
			CHG_EN_COMMAND_BIT
			| (chip->chg_inhibit_en ? CHARGER_INHIBIT_BIT : 0)
			| (chip->iterm_disabled ? I_TERM_BIT : 0));
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
		return rc;
	}
	chip->battchg_disabled = 0;

	/*
	 * Based on the configuration, use the analog sensors or the fuelgauge
	 * adc for recharge threshold source.
	 */

	if (chip->chg_inhibit_source_fg)
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG1,
			TERM_I_SRC_BIT | RECHG_THRESHOLD_SRC_BIT,
			TERM_SRC_FG | RECHG_THRESHOLD_SRC_BIT);
	else
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CHGR_CFG1,
			TERM_I_SRC_BIT | RECHG_THRESHOLD_SRC_BIT, 0);

	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set chgr_cfg2 rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip, chip->chgr_base + CCMP_CFG,
				     CCMP_CFG_MASK, CCMP_CFG_DIS_ALL_TEMP);

	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set ccmp_cfg rc=%d\n", rc);
		return rc;
	}

	/*
	 * control USB suspend via command bits and set correct 100/500mA
	 * polarity on the usb current
	 */
	rc = smbchg_sec_masked_write(chip, chip->usb_chgpth_base + CHGPTH_CFG,
		USBIN_SUSPEND_SRC_BIT | USB51_COMMAND_POL | USB51AC_CTRL,
		(chip->charge_unknown_battery ? 0 : USBIN_SUSPEND_SRC_BIT));
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set usb_chgpth cfg rc=%d\n", rc);
		return rc;
	}

	/* set the float voltage */
	if (chip->vfloat_mv != -EINVAL) {
		rc = smbchg_float_voltage_set(chip, chip->vfloat_mv);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set float voltage rc = %d\n", rc);
			return rc;
		}
		pr_smb(PR_STATUS, "set vfloat to %d\n", chip->vfloat_mv);
	}

	/* set the fast charge current compensation */
	if (chip->fastchg_current_comp != -EINVAL) {
		rc = smbchg_fastchg_current_comp_set(chip,
			chip->fastchg_current_comp);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set fastchg current comp rc = %d\n",
				rc);
			return rc;
		}
		pr_smb(PR_STATUS, "set fastchg current comp to %d\n",
			chip->fastchg_current_comp);
	}

	/* set the float voltage compensation */
	if (chip->float_voltage_comp != -EINVAL) {
		rc = smbchg_float_voltage_comp_set(chip,
			chip->float_voltage_comp);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set float voltage comp rc = %d\n",
				rc);
			return rc;
		}
		pr_smb(PR_STATUS, "set float voltage comp to %d\n",
			chip->float_voltage_comp);
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

			rc = smbchg_sec_masked_write(chip,
					chip->chgr_base + CFG_TCC_REG,
					CHG_ITERM_MASK, reg);
			if (rc) {
				dev_err(chip->dev,
					"Couldn't set iterm rc = %d\n", rc);
				return rc;
			}
			pr_smb(PR_STATUS, "set tcc (%d) to 0x%02x\n",
					chip->iterm_ma, reg);
		}
	}

	/* set the safety time voltage */
	if (chip->safety_time != -EINVAL) {
		reg = (chip->safety_time > 0 ? 0 : SFT_TIMER_DISABLE_BIT) |
			(chip->prechg_safety_time > 0
			? 0 : PRECHG_SFT_TIMER_DISABLE_BIT);

		for (i = 0; i < ARRAY_SIZE(chg_time); i++) {
			if (chip->safety_time <= chg_time[i]) {
				if (chip->safety_time == 0)
					reg |= (ARRAY_SIZE(chg_time) - 1)
						<< SAFETY_TIME_MINUTES_SHIFT;
				else
					reg |= i << SAFETY_TIME_MINUTES_SHIFT;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(prechg_time); i++) {
			if (chip->prechg_safety_time <= prechg_time[i]) {
				if (chip->prechg_safety_time == 0)
					reg |= ARRAY_SIZE(prechg_time) - 1;
				else
					reg |= i;
				break;
			}
		}

		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + SFT_CFG,
				SFT_EN_MASK | SFT_TO_MASK |
				(chip->prechg_safety_time > 0
				? PRECHG_SFT_TO_MASK : 0), reg);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set safety timer rc = %d\n",
				rc);
			return rc;
		}

		if ((chip->safety_time == 0) &&
		    (chip->prechg_safety_time == 0))
			chip->safety_timer_en = false;
		else
			chip->safety_timer_en = true;
	} else {
		rc = smbchg_read(chip, &reg, chip->chgr_base + SFT_CFG, 1);
		if (rc < 0)
			dev_err(chip->dev, "Unable to read SFT_CFG rc = %d\n",
				rc);
		else if (!(reg & SFT_EN_MASK))
			chip->safety_timer_en = true;
	}

	/* configure jeita temperature hard limit */
	if (chip->jeita_temp_hard_limit >= 0) {
		rc = smbchg_sec_masked_write(chip,
			chip->chgr_base + CHGR_CCMP_CFG,
			JEITA_TEMP_HARD_LIMIT_BIT,
			chip->jeita_temp_hard_limit
			? 0 : JEITA_TEMP_HARD_LIMIT_BIT);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set jeita temp hard limit rc = %d\n",
				rc);
			return rc;
		}
	}

	/* make the buck switch faster to prevent some vbus oscillation */
	rc = smbchg_sec_masked_write(chip,
			chip->usb_chgpth_base + TR_8OR32B,
			BUCK_8_16_FREQ_BIT, 0);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set buck frequency rc = %d\n", rc);
		return rc;
	}

	/* battery missing detection */
	mask =  BATT_MISSING_ALGO_BIT;
	reg = chip->bmd_algo_disabled ? 0 : BATT_MISSING_ALGO_BIT;
	if (chip->bmd_pin_src < BPD_TYPE_DEFAULT) {
		mask |= BMD_PIN_SRC_MASK;
		reg |= chip->bmd_pin_src << PIN_SRC_SHIFT;
	}
	rc = smbchg_sec_masked_write(chip,
			chip->bat_if_base + BM_CFG, mask, reg);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set batt_missing config = %d\n",
									rc);
		return rc;
	}

	smbchg_charging_status_change(chip);

	/*
	 * The charger needs 20 milliseconds to go into battery supplementary
	 * mode. Sleep here until we are sure it takes into effect.
	 */
	msleep(20);
	smbchg_usb_en(chip, chip->chg_enabled, REASON_USER);
	smbchg_dc_en(chip, chip->chg_enabled, REASON_USER);
	/* resume threshold */
	if (chip->resume_delta_mv != -EINVAL) {

		/*
		 * Configure only if the recharge threshold source is not
		 * fuel gauge ADC.
		 */
		if (!chip->chg_inhibit_source_fg) {
			if (chip->resume_delta_mv < 100)
				reg = CHG_INHIBIT_50MV_VAL;
			else if (chip->resume_delta_mv < 200)
				reg = CHG_INHIBIT_100MV_VAL;
			else if (chip->resume_delta_mv < 300)
				reg = CHG_INHIBIT_200MV_VAL;
			else
				reg = CHG_INHIBIT_300MV_VAL;

			rc = smbchg_sec_masked_write(chip,
					chip->chgr_base + CHG_INHIB_CFG_REG,
					CHG_INHIBIT_MASK, reg);
			if (rc < 0) {
				dev_err(chip->dev, "Couldn't set inhibit val rc = %d\n",
						rc);
				return rc;
			}
		}

		rc = smbchg_sec_masked_write(chip,
				chip->chgr_base + CHGR_CFG,
				RCHG_LVL_BIT, (chip->resume_delta_mv < 200)
				? 0 : RCHG_LVL_BIT);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set recharge rc = %d\n",
					rc);
			return rc;
		}
	}

	/* DC path current settings */
	if (chip->dc_psy_type != -EINVAL) {
		rc = smbchg_sec_masked_write(chip,
				     chip->dc_chgpth_base + DC_CHGR_CFG,
				     0xFF, DCIN_ALLOW_5V_TO_9V);
		if (rc < 0) {
			dev_err(chip->dev,
				"Couldn't set DCIN CHGR CONFIG rc = %d\n", rc);
			return rc;
		}

		rc = smbchg_set_thermal_limited_dc_current_max(chip,
						chip->dc_target_current_ma);
		if (rc < 0) {
			dev_err(chip->dev, "can't set dc current: %d\n", rc);
			return rc;
		}
	}


	/*
	 * on some devices the battery is powered via external sources which
	 * could raise its voltage above the float voltage. smbchargers go
	 * in to reverse boost in such a situation and the workaround is to
	 * disable float voltage compensation (note that the battery will appear
	 * hot/cold when powered via external source).
	 */
	if (chip->soft_vfloat_comp_disabled) {
		rc = smbchg_sec_masked_write(chip, chip->chgr_base + CFG_AFVC,
				VFLOAT_COMP_ENABLE_MASK, 0);
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't disable soft vfloat rc = %d\n",
					rc);
			return rc;
		}
	}

	rc = smbchg_set_fastchg_current(chip, chip->target_fastchg_current_ma);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't set fastchg current = %d\n", rc);
		return rc;
	}

	rc = smbchg_sec_masked_write(chip,
				     chip->usb_chgpth_base + USBIN_CHGR_CFG,
				     USBIN_ALLOW_MASK, USBIN_ALLOW_5V_TO_9V);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't write usb allowance rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_read(chip, &chip->original_usbin_allowance,
			chip->usb_chgpth_base + USBIN_CHGR_CFG, 1);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't read usb allowance rc=%d\n", rc);

	if (chip->wipower_dyn_icl_avail) {
		rc = smbchg_wipower_ilim_config(chip,
				&(chip->wipower_default.entries[0]));
		if (rc < 0) {
			dev_err(chip->dev, "Couldn't set default wipower ilim = %d\n",
				rc);
			return rc;
		}
	}

	if (chip->force_aicl_rerun)
		rc = smbchg_aicl_config(chip);

	return rc;
}

static int smbchg_force_apsd(struct smbchg_chip *chip)
{
	int rc;

	dev_info(chip->dev, "Start APSD Rerun!\n");
	rc = smbchg_sec_masked_write(chip,
				     chip->usb_chgpth_base + USBIN_CHGR_CFG,
				     USBIN_ALLOW_MASK, USBIN_ALLOW_9V);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write usb allowance %d rc=%d\n",
			USBIN_ALLOW_9V, rc);
	msleep(10);

	/* RESET to Default 5V to 9V */
	rc = smbchg_sec_masked_write(chip,
				     chip->usb_chgpth_base + USBIN_CHGR_CFG,
				     USBIN_ALLOW_MASK, USBIN_ALLOW_5V_TO_9V);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write usb allowance %d rc=%d\n",
			USBIN_ALLOW_5V_TO_9V, rc);

	return rc;
}

static struct of_device_id smbchg_match_table[] = {
	{
		.compatible	= "qcom,qpnp-smbcharger",
		.data		= (void *)ARRAY_SIZE(usb_current_table),
	},
	{ },
};

#define DC_MA_MIN 300
#define DC_MA_MAX 2000
#define OF_PROP_READ(chip, prop, dt_property, retval, optional)		\
do {									\
	if (retval)							\
		break;							\
	if (optional)							\
		prop = -EINVAL;						\
									\
	retval = of_property_read_u32(chip->spmi->dev.of_node,		\
					"qcom," dt_property	,	\
					&prop);				\
									\
	if ((retval == -EINVAL) && optional)				\
		retval = 0;						\
	else if (retval)						\
		dev_err(chip->dev, "Error reading " #dt_property	\
				" property rc = %d\n", rc);		\
} while (0)

#define ILIM_ENTRIES		3
#define VOLTAGE_RANGE_ENTRIES	2
#define RANGE_ENTRY		(ILIM_ENTRIES + VOLTAGE_RANGE_ENTRIES)
static int smb_parse_wipower_map_dt(struct smbchg_chip *chip,
		struct ilim_map *map, char *property)
{
	struct device_node *node = chip->dev->of_node;
	int total_elements, size;
	struct property *prop;
	const __be32 *data;
	int num, i;

	prop = of_find_property(node, property, &size);
	if (!prop) {
		dev_err(chip->dev, "%s missing\n", property);
		return -EINVAL;
	}

	total_elements = size / sizeof(int);
	if (total_elements % RANGE_ENTRY) {
		dev_err(chip->dev, "%s table not in multiple of %d, total elements = %d\n",
				property, RANGE_ENTRY, total_elements);
		return -EINVAL;
	}

	data = prop->value;
	num = total_elements / RANGE_ENTRY;
	map->entries = devm_kzalloc(chip->dev,
			num * sizeof(struct ilim_entry), GFP_KERNEL);
	if (!map->entries) {
		dev_err(chip->dev, "kzalloc failed for default ilim\n");
		return -ENOMEM;
	}
	for (i = 0; i < num; i++) {
		map->entries[i].vmin_uv =  be32_to_cpup(data++);
		map->entries[i].vmax_uv =  be32_to_cpup(data++);
		map->entries[i].icl_pt_ma =  be32_to_cpup(data++);
		map->entries[i].icl_lv_ma =  be32_to_cpup(data++);
		map->entries[i].icl_hv_ma =  be32_to_cpup(data++);
	}
	map->num = num;
	return 0;
}

static int smb_parse_wipower_dt(struct smbchg_chip *chip)
{
	int rc = 0;

	chip->wipower_dyn_icl_avail = false;

	if (!chip->vadc_dev)
		goto err;

	rc = smb_parse_wipower_map_dt(chip, &chip->wipower_default,
					"qcom,wipower-default-ilim-map");
	if (rc) {
		dev_err(chip->dev, "failed to parse wipower-pt-ilim-map rc = %d\n",
				rc);
		goto err;
	}

	rc = smb_parse_wipower_map_dt(chip, &chip->wipower_pt,
					"qcom,wipower-pt-ilim-map");
	if (rc) {
		dev_err(chip->dev, "failed to parse wipower-pt-ilim-map rc = %d\n",
				rc);
		goto err;
	}

	rc = smb_parse_wipower_map_dt(chip, &chip->wipower_div2,
					"qcom,wipower-div2-ilim-map");
	if (rc) {
		dev_err(chip->dev, "failed to parse wipower-div2-ilim-map rc = %d\n",
				rc);
		goto err;
	}
	chip->wipower_dyn_icl_avail = true;
	return 0;
err:
	chip->wipower_default.num = 0;
	chip->wipower_pt.num = 0;
	chip->wipower_default.num = 0;
	if (chip->wipower_default.entries)
		devm_kfree(chip->dev, chip->wipower_default.entries);
	if (chip->wipower_pt.entries)
		devm_kfree(chip->dev, chip->wipower_pt.entries);
	if (chip->wipower_div2.entries)
		devm_kfree(chip->dev, chip->wipower_div2.entries);
	chip->wipower_default.entries = NULL;
	chip->wipower_pt.entries = NULL;
	chip->wipower_div2.entries = NULL;
	chip->vadc_dev = NULL;
	return rc;
}

static int parse_dt_pchg_current_map(const u32 *arr,
				     struct pchg_current_map *current_map,
				     int count)
{
	u32 len = 0;
	u32 requested;
	u32 primary;
	u32 secondary;
	int i;

	if (!arr)
		return 0;

	for (i = 0; i < count*3; i += 3) {
		requested = be32_to_cpu(arr[i]);
		primary = be32_to_cpu(arr[i + 1]);
		secondary = be32_to_cpu(arr[i + 2]);
		current_map->requested = requested;
		current_map->primary = primary;
		current_map->secondary = secondary;
		len++;
		current_map++;
	}
	return len;
}

static int smb_parse_dt(struct smbchg_chip *chip)
{
	int rc = 0;
	struct device_node *node = chip->dev->of_node;
	const char *dc_psy_type, *bpd;
	const u32 *current_map;

	if (!node) {
		dev_err(chip->dev, "device tree info. missing\n");
		return -EINVAL;
	}

	/* read optional u32 properties */
	OF_PROP_READ(chip, chip->iterm_ma, "iterm-ma", rc, 1);
	OF_PROP_READ(chip, chip->target_fastchg_current_ma,
			"fastchg-current-ma", rc, 1);
	OF_PROP_READ(chip, chip->vfloat_mv, "float-voltage-mv", rc, 1);
	OF_PROP_READ(chip, chip->safety_time, "charging-timeout-mins", rc, 1);
	OF_PROP_READ(chip, chip->rpara_uohm, "rparasitic-uohm", rc, 1);
	OF_PROP_READ(chip, chip->prechg_safety_time, "precharging-timeout-mins",
			rc, 1);
	OF_PROP_READ(chip, chip->fastchg_current_comp, "fastchg-current-comp",
			rc, 1);
	OF_PROP_READ(chip, chip->float_voltage_comp, "float-voltage-comp",
			rc, 1);
	if (chip->safety_time != -EINVAL &&
		(chip->safety_time > chg_time[ARRAY_SIZE(chg_time) - 1])) {
		dev_err(chip->dev, "Bad charging-timeout-mins %d\n",
						chip->safety_time);
		return -EINVAL;
	}
	if (chip->prechg_safety_time != -EINVAL &&
		(chip->prechg_safety_time >
		 prechg_time[ARRAY_SIZE(prechg_time) - 1])) {
		dev_err(chip->dev, "Bad precharging-timeout-mins %d\n",
						chip->prechg_safety_time);
		return -EINVAL;
	}
	OF_PROP_READ(chip, chip->resume_delta_mv, "resume-delta-mv", rc, 1);
	OF_PROP_READ(chip, chip->parallel.min_current_thr_ma,
			"parallel-usb-min-current-ma", rc, 1);
	OF_PROP_READ(chip, chip->parallel.min_9v_current_thr_ma,
			"parallel-usb-9v-min-current-ma", rc, 1);
	OF_PROP_READ(chip, chip->parallel.allowed_lowering_ma,
			"parallel-allowed-lowering-ma", rc, 1);
	if (chip->parallel.min_current_thr_ma != -EINVAL
			&& chip->parallel.min_9v_current_thr_ma != -EINVAL)
		chip->parallel.avail = true;
	pr_smb(PR_STATUS, "parallel usb thr: %d, 9v thr: %d\n",
			chip->parallel.min_current_thr_ma,
			chip->parallel.min_9v_current_thr_ma);
	OF_PROP_READ(chip, chip->jeita_temp_hard_limit,
			"jeita-temp-hard-limit", rc, 1);

	OF_PROP_READ(chip, chip->hot_temp_c,
		     "hot-temp-c", rc, 1);
	if (chip->hot_temp_c == -EINVAL)
		chip->hot_temp_c = 60;

	OF_PROP_READ(chip, chip->cold_temp_c,
		     "cold-temp-c", rc, 1);
	if (chip->cold_temp_c == -EINVAL)
		chip->cold_temp_c = -20;

	OF_PROP_READ(chip, chip->warm_temp_c,
		     "warm-temp-c", rc, 1);
	if (chip->warm_temp_c == -EINVAL)
		chip->warm_temp_c = 45;

	OF_PROP_READ(chip, chip->cool_temp_c,
		     "cool-temp-c", rc, 1);
	if (chip->cool_temp_c == -EINVAL)
		chip->cool_temp_c = 0;

	OF_PROP_READ(chip, chip->ext_temp_volt_mv,
		     "ext-temp-volt-mv", rc, 1);
	if (chip->ext_temp_volt_mv == -EINVAL)
		chip->ext_temp_volt_mv = 4000;

	OF_PROP_READ(chip, chip->ext_temp_soc,
		     "ext-temp-soc", rc, 1);
	if (chip->ext_temp_soc == -EINVAL)
		chip->ext_temp_soc = 65;

	OF_PROP_READ(chip, chip->hotspot_thrs_c,
		     "hotspot-thrs-c", rc, 1);
	if (chip->hotspot_thrs_c == -EINVAL)
		chip->hotspot_thrs_c = 50;

	OF_PROP_READ(chip, chip->stepchg_voltage_mv,
			"stepchg-voltage-mv", rc, 1);

	OF_PROP_READ(chip, chip->stepchg_current_ma,
			"stepchg-current-ma", rc, 1);

	OF_PROP_READ(chip, chip->stepchg_taper_ma,
			"stepchg-taper-ma", rc, 1);

	OF_PROP_READ(chip, chip->stepchg_iterm_ma,
			"stepchg-iterm-ma", rc, 1);
	if ((chip->stepchg_current_ma != -EINVAL) &&
	    (chip->stepchg_voltage_mv != -EINVAL) &&
	    (chip->stepchg_taper_ma != -EINVAL) &&
	    (chip->stepchg_iterm_ma != -EINVAL)) {
		chip->stepchg_max_current_ma = chip->target_fastchg_current_ma;
		chip->allowed_fastchg_current_ma =
			chip->target_fastchg_current_ma;
		chip->stepchg_max_voltage_mv = chip->vfloat_mv;
	}

	/* read boolean configuration properties */
	chip->use_vfloat_adjustments = of_property_read_bool(node,
						"qcom,autoadjust-vfloat");
	chip->bmd_algo_disabled = of_property_read_bool(node,
						"qcom,bmd-algo-disabled");
	chip->iterm_disabled = of_property_read_bool(node,
						"qcom,iterm-disabled");
	chip->soft_vfloat_comp_disabled = of_property_read_bool(node,
					"qcom,soft-vfloat-comp-disabled");
	chip->chg_enabled = !(of_property_read_bool(node,
						"qcom,charging-disabled"));
	chip->charge_unknown_battery = of_property_read_bool(node,
						"qcom,charge-unknown-battery");
	chip->chg_inhibit_en = of_property_read_bool(node,
					"qcom,chg-inhibit-en");
	chip->chg_inhibit_source_fg = of_property_read_bool(node,
						"qcom,chg-inhibit-fg");
	chip->low_volt_dcin = of_property_read_bool(node,
					"qcom,low-volt-dcin");
	chip->usbid_disabled = of_property_read_bool(node,
						"qcom,usbid-disabled");
	chip->batt_therm_wa = of_property_read_bool(node,
					       "qcom,batt-therm-wa-enabled");
	if (chip->batt_therm_wa)
		dev_info(chip->dev, "batt_therm workaround is enabled!\n");
	chip->force_aicl_rerun = of_property_read_bool(node,
					"qcom,force-aicl-rerun");

	/* parse the battery missing detection pin source */
	rc = of_property_read_string(chip->spmi->dev.of_node,
		"qcom,bmd-pin-src", &bpd);
	if (rc) {
		/* Select BAT_THM as default BPD scheme */
		chip->bmd_pin_src = BPD_TYPE_DEFAULT;
		rc = 0;
	} else {
		chip->bmd_pin_src = get_bpd(bpd);
		if (chip->bmd_pin_src < 0) {
			dev_err(chip->dev,
				"failed to determine bpd schema %d\n", rc);
			return rc;
		}
	}

	/* parse the dc power supply configuration */
	rc = of_property_read_string(node, "qcom,dc-psy-type", &dc_psy_type);
	if (rc) {
		chip->dc_psy_type = -EINVAL;
		rc = 0;
	} else {
		if (strcmp(dc_psy_type, "Mains") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_MAINS;
		else if (strcmp(dc_psy_type, "Wireless") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIRELESS;
		else if (strcmp(dc_psy_type, "Wipower") == 0)
			chip->dc_psy_type = POWER_SUPPLY_TYPE_WIPOWER;
	}
	if (chip->dc_psy_type != -EINVAL) {
		OF_PROP_READ(chip, chip->dc_target_current_ma,
				"dc-psy-ma", rc, 0);
		if (rc)
			return rc;
		if (chip->dc_target_current_ma < DC_MA_MIN
				|| chip->dc_target_current_ma > DC_MA_MAX) {
			dev_err(chip->dev, "Bad dc mA %d\n",
					chip->dc_target_current_ma);
			return -EINVAL;
		}
	}

	if (chip->dc_psy_type == POWER_SUPPLY_TYPE_WIPOWER)
		smb_parse_wipower_dt(chip);

	/* read the bms power supply name */
	rc = of_property_read_string(node, "qcom,bms-psy-name",
						&chip->bms_psy_name);
	if (rc)
		chip->bms_psy_name = NULL;

	/* read the bms power supply name */
	rc = of_property_read_string(node, "qcom,battery-psy-name",
						&chip->battery_psy_name);
	if (rc)
		chip->battery_psy_name = "battery";

	current_map = of_get_property(node, "qcom,parallel-charge-current-map",
				      &chip->pchg_current_map_len);
	if ((!current_map) || (chip->pchg_current_map_len <= 0))
		dev_err(chip->dev, "No parallel charge current map defined\n");
	else {
		chip->pchg_current_map_len /= 3 * sizeof(u32);
		dev_err(chip->dev, "length=%d\n", chip->pchg_current_map_len);
		if (chip->pchg_current_map_len > 30)
			chip->pchg_current_map_len = 30;

		chip->pchg_current_map_data =
			devm_kzalloc(chip->dev,
				     (sizeof(struct pchg_current_map) *
				      chip->pchg_current_map_len),
				     GFP_KERNEL);
		if (chip->pchg_current_map_data == NULL) {
			dev_err(chip->dev,
			 "Failed to kzalloc memory for parallel charge map.\n");
			return -ENOMEM;
		}

		chip->pchg_current_map_len =
			parse_dt_pchg_current_map(current_map,
						  chip->pchg_current_map_data,
						  chip->pchg_current_map_len);

		if (chip->pchg_current_map_len <= 0) {
			dev_err(chip->dev,
			"Couldn't read parallel charge currents rc = %d\n", rc);
			return rc;
		}
		dev_err(chip->dev, "num parallel charge entries=%d\n",
			chip->pchg_current_map_len);
	}

	if (of_find_property(node, "qcom,chg-thermal-mitigation",
					&chip->chg_thermal_levels)) {
		chip->chg_thermal_mitigation = devm_kzalloc(chip->dev,
			chip->chg_thermal_levels,
			GFP_KERNEL);

		if (chip->chg_thermal_mitigation == NULL) {
			dev_err(chip->dev,
				"thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->chg_thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
						"qcom,chg-thermal-mitigation",
						chip->chg_thermal_mitigation,
						chip->chg_thermal_levels);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't read therm limits rc = %d\n", rc);
			return rc;
		}
	} else
		chip->chg_thermal_levels = 0;

	if (of_find_property(node, "qcom,thermal-mitigation",
					&chip->thermal_levels)) {
		chip->thermal_mitigation = devm_kzalloc(chip->dev,
			chip->thermal_levels,
			GFP_KERNEL);

		if (chip->thermal_mitigation == NULL) {
			dev_err(chip->dev, "thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
				"qcom,thermal-mitigation",
				chip->thermal_mitigation, chip->thermal_levels);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't read therm limits rc = %d\n", rc);
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
			dev_err(chip->dev,
				"DC thermal mitigation kzalloc() failed.\n");
			return -ENOMEM;
		}

		chip->dc_thermal_levels /= sizeof(int);
		rc = of_property_read_u32_array(node,
				"qcom,dc-thermal-mitigation",
				chip->dc_thermal_mitigation,
				chip->dc_thermal_levels);
		if (rc) {
			dev_err(chip->dev,
				"Couldn't read DC therm limits rc = %d\n", rc);
			return rc;
		}
	} else
		chip->dc_thermal_levels = 0;

	return 0;
}

#define SUBTYPE_REG			0x5
#define SMBCHG_CHGR_SUBTYPE		0x1
#define SMBCHG_OTG_SUBTYPE		0x8
#define SMBCHG_BAT_IF_SUBTYPE		0x3
#define SMBCHG_USB_CHGPTH_SUBTYPE	0x4
#define SMBCHG_DC_CHGPTH_SUBTYPE	0x5
#define SMBCHG_MISC_SUBTYPE		0x7
#define REQUEST_IRQ(chip, resource, irq_num, irq_name, irq_handler, flags, rc)\
do {									\
	irq_num = spmi_get_irq_byname(chip->spmi,			\
					resource, irq_name);		\
	if (irq_num < 0) {						\
		dev_err(chip->dev, "Unable to get " irq_name " irq\n");	\
		return -ENXIO;						\
	}								\
	rc = devm_request_threaded_irq(chip->dev,			\
			irq_num, NULL, irq_handler, flags, irq_name,	\
			chip);						\
	if (rc < 0) {							\
		dev_err(chip->dev, "Unable to request " irq_name " irq: %d\n",\
				rc);					\
		return -ENXIO;						\
	}								\
} while (0)

static int smbchg_request_irqs(struct smbchg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
							| IRQF_ONESHOT;

	spmi_for_each_container_dev(spmi_resource, chip->spmi) {
		if (!spmi_resource) {
				dev_err(chip->dev, "spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			dev_err(chip->dev, "node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		rc = smbchg_read(chip, &subtype,
				resource->start + SUBTYPE_REG, 1);
		if (rc) {
			dev_err(chip->dev, "Peripheral subtype read failed rc=%d\n",
					rc);
			return rc;
		}

		switch (subtype) {
		case SMBCHG_CHGR_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->chg_error_irq,
				"chg-error", chg_error_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->taper_irq,
				"chg-taper-thr", taper_handler,
				(IRQF_TRIGGER_RISING | IRQF_ONESHOT), rc);
			disable_irq_nosync(chip->taper_irq);
			REQUEST_IRQ(chip, spmi_resource, chip->chg_term_irq,
				"chg-tcc-thr", chg_term_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->recharge_irq,
				"chg-rechg-thr", recharge_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->fastchg_irq,
				"chg-p2f-thr", fastchg_handler, flags, rc);
			enable_irq_wake(chip->chg_term_irq);
			enable_irq_wake(chip->chg_error_irq);
			enable_irq_wake(chip->fastchg_irq);
			break;
		case SMBCHG_BAT_IF_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->batt_hot_irq,
				"batt-hot", batt_hot_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_warm_irq,
				"batt-warm", batt_warm_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_cool_irq,
				"batt-cool", batt_cool_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_cold_irq,
				"batt-cold", batt_cold_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->batt_missing_irq,
				"batt-missing", batt_pres_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->vbat_low_irq,
				"batt-low", vbat_low_handler, flags, rc);
			enable_irq_wake(chip->batt_hot_irq);
			enable_irq_wake(chip->batt_warm_irq);
			enable_irq_wake(chip->batt_cool_irq);
			enable_irq_wake(chip->batt_cold_irq);
			enable_irq_wake(chip->batt_missing_irq);
			enable_irq_wake(chip->vbat_low_irq);
			break;
		case SMBCHG_USB_CHGPTH_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->usbin_uv_irq,
				"usbin-uv", usbin_uv_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->usbin_ov_irq,
				"usbin-ov", usbin_ov_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->src_detect_irq,
				"usbin-src-det",
				src_detect_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->otg_fail_irq,
				"otg-fail", otg_fail_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource, chip->otg_oc_irq,
				"otg-oc", otg_oc_handler,
				(IRQF_TRIGGER_RISING | IRQF_ONESHOT), rc);
			REQUEST_IRQ(chip, spmi_resource, chip->aicl_done_irq,
				"aicl-done",
				aicl_done_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource,
				chip->usbid_change_irq, "usbid-change",
				usbid_change_handler,
				(IRQF_TRIGGER_FALLING | IRQF_ONESHOT), rc);
			enable_irq_wake(chip->usbin_uv_irq);
			enable_irq_wake(chip->usbin_ov_irq);
			enable_irq_wake(chip->src_detect_irq);
			enable_irq_wake(chip->otg_fail_irq);
			enable_irq_wake(chip->otg_oc_irq);
			enable_irq_wake(chip->usbid_change_irq);
			if (chip->parallel.avail && chip->usb_present) {
				rc = enable_irq_wake(chip->aicl_done_irq);
				chip->enable_aicl_wake = true;
			}
			break;
		case SMBCHG_DC_CHGPTH_SUBTYPE:
			REQUEST_IRQ(chip, spmi_resource, chip->dcin_uv_irq,
				"dcin-uv", dcin_uv_handler, flags, rc);
			enable_irq_wake(chip->dcin_uv_irq);
			break;
		case SMBCHG_MISC_SUBTYPE:
#ifdef QCOM_BASE
			REQUEST_IRQ(chip, spmi_resource, chip->power_ok_irq,
				"power-ok", power_ok_handler, flags, rc);
#endif
			REQUEST_IRQ(chip, spmi_resource, chip->chg_hot_irq,
				"temp-shutdown", chg_hot_handler, flags, rc);
			REQUEST_IRQ(chip, spmi_resource,
				chip->safety_timeout_irq,
				"safety-timeout",
				safety_timeout_handler, flags, rc);
			enable_irq_wake(chip->chg_hot_irq);
			enable_irq_wake(chip->safety_timeout_irq);
			break;
		case SMBCHG_OTG_SUBTYPE:
			break;
		}
	}

	return rc;
}

#define REQUIRE_BASE(chip, base, rc)					\
do {									\
	if (!rc && !chip->base) {					\
		dev_err(chip->dev, "Missing " #base "\n");		\
		rc = -EINVAL;						\
	}								\
} while (0)

static int smbchg_parse_peripherals(struct smbchg_chip *chip)
{
	int rc = 0;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	u8 subtype;
	struct spmi_device *spmi = chip->spmi;

	spmi_for_each_container_dev(spmi_resource, chip->spmi) {
		if (!spmi_resource) {
				dev_err(chip->dev, "spmi resource absent\n");
			return rc;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
						IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			dev_err(chip->dev, "node %s IO resource absent!\n",
				spmi->dev.of_node->full_name);
			return rc;
		}

		rc = smbchg_read(chip, &subtype,
				resource->start + SUBTYPE_REG, 1);
		if (rc) {
			dev_err(chip->dev, "Peripheral subtype read failed rc=%d\n",
					rc);
			return rc;
		}

		switch (subtype) {
		case SMBCHG_CHGR_SUBTYPE:
			chip->chgr_base = resource->start;
			break;
		case SMBCHG_BAT_IF_SUBTYPE:
			chip->bat_if_base = resource->start;
			break;
		case SMBCHG_USB_CHGPTH_SUBTYPE:
			chip->usb_chgpth_base = resource->start;
			break;
		case SMBCHG_DC_CHGPTH_SUBTYPE:
			chip->dc_chgpth_base = resource->start;
			break;
		case SMBCHG_MISC_SUBTYPE:
			chip->misc_base = resource->start;
			break;
		case SMBCHG_OTG_SUBTYPE:
			chip->otg_base = resource->start;
			break;
		}
	}

	REQUIRE_BASE(chip, chgr_base, rc);
	REQUIRE_BASE(chip, bat_if_base, rc);
	REQUIRE_BASE(chip, usb_chgpth_base, rc);
	REQUIRE_BASE(chip, dc_chgpth_base, rc);
	REQUIRE_BASE(chip, misc_base, rc);

	return rc;
}

static inline void dump_reg(struct smbchg_chip *chip, u16 addr,
		const char *name)
{
	u8 reg;

	smbchg_read(chip, &reg, addr, 1);
	pr_smb(PR_DUMP, "%s - %04X = %02X\n", name, addr, reg);
}

/* dumps useful registers for debug */
static void dump_regs(struct smbchg_chip *chip)
{
	u16 addr;

	/* charger peripheral */
	for (addr = 0xB; addr <= 0x10; addr++)
		dump_reg(chip, chip->chgr_base + addr, "CHGR Status");
	for (addr = 0xF0; addr <= 0xFF; addr++)
		dump_reg(chip, chip->chgr_base + addr, "CHGR Config");
	/* battery interface peripheral */
	dump_reg(chip, chip->bat_if_base + RT_STS, "BAT_IF Status");
	dump_reg(chip, chip->bat_if_base + CMD_CHG_REG, "BAT_IF Command");
	for (addr = 0xF0; addr <= 0xFB; addr++)
		dump_reg(chip, chip->bat_if_base + addr, "BAT_IF Config");
	/* usb charge path peripheral */
	for (addr = 0x7; addr <= 0x10; addr++)
		dump_reg(chip, chip->usb_chgpth_base + addr, "USB Status");
	dump_reg(chip, chip->usb_chgpth_base + CMD_IL, "USB Command");
	for (addr = 0xF0; addr <= 0xF5; addr++)
		dump_reg(chip, chip->usb_chgpth_base + addr, "USB Config");
	/* dc charge path peripheral */
	dump_reg(chip, chip->dc_chgpth_base + RT_STS, "DC Status");
	for (addr = 0xF0; addr <= 0xF6; addr++)
		dump_reg(chip, chip->dc_chgpth_base + addr, "DC Config");
	/* misc peripheral */
	dump_reg(chip, chip->misc_base + IDEV_STS, "MISC Status");
	dump_reg(chip, chip->misc_base + RT_STS, "MISC Status");
	for (addr = 0xF0; addr <= 0xF3; addr++)
		dump_reg(chip, chip->misc_base + addr, "MISC CFG");
}

static inline void dump_reg_dbfs(struct smbchg_chip *chip, u16 addr,
				 const char *name, struct seq_file *m)
{
	u8 reg;

	smbchg_read(chip, &reg, addr, 1);
	seq_printf(m, "%s - %04X = %02X\n", name, addr, reg);
}

/* dumps useful registers for debug */
static void dump_regs_dbfs(struct smbchg_chip *chip, struct seq_file *m)
{
	u16 addr;

	/* charger peripheral */
	for (addr = 0xB; addr <= 0x10; addr++)
		dump_reg_dbfs(chip, chip->chgr_base + addr, "CHGR Status", m);
	for (addr = 0xF0; addr <= 0xFF; addr++)
		dump_reg_dbfs(chip, chip->chgr_base + addr, "CHGR Config", m);
	/* battery interface peripheral */
	dump_reg_dbfs(chip, chip->bat_if_base + RT_STS, "BAT_IF Status", m);
	dump_reg_dbfs(chip, chip->bat_if_base + CMD_CHG_REG,
		      "BAT_IF Command", m);
	for (addr = 0xF0; addr <= 0xFB; addr++)
		dump_reg_dbfs(chip, chip->bat_if_base + addr,
			      "BAT_IF Config", m);
	/* usb charge path peripheral */
	for (addr = 0x7; addr <= 0x10; addr++)
		dump_reg_dbfs(chip, chip->usb_chgpth_base + addr,
			      "USB Status", m);
	dump_reg_dbfs(chip, chip->usb_chgpth_base + CMD_IL, "USB Command", m);
	for (addr = 0xF0; addr <= 0xF5; addr++)
		dump_reg_dbfs(chip, chip->usb_chgpth_base + addr,
			      "USB Config", m);
	/* dc charge path peripheral */
	dump_reg_dbfs(chip, chip->dc_chgpth_base + RT_STS, "DC Status", m);
	for (addr = 0xF0; addr <= 0xF6; addr++)
		dump_reg_dbfs(chip, chip->dc_chgpth_base + addr,
			      "DC Config", m);
	/* misc peripheral */
	dump_reg_dbfs(chip, chip->misc_base + IDEV_STS, "MISC Status", m);
	dump_reg_dbfs(chip, chip->misc_base + RT_STS, "MISC Status", m);
	for (addr = 0xF0; addr <= 0xF3; addr++)
		dump_reg_dbfs(chip, chip->misc_base + addr, "MISC CFG", m);
}

static int show_cnfg_regs(struct seq_file *m, void *data)
{
	struct smbchg_chip *chip = m->private;

	dump_regs_dbfs(chip, m);
	return 0;
}

static int cnfg_debugfs_open(struct inode *inode, struct file *file)
{
	struct smbchg_chip *chip = inode->i_private;

	return single_open(file, show_cnfg_regs, chip);
}

static const struct file_operations cnfg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= cnfg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct smbchg_chip *chip)
{
	struct dentry *ent;

	chip->debug_root = debugfs_create_dir("qpnp-smbcharger", NULL);
	if (!chip->debug_root) {
		dev_err(chip->dev, "Couldn't create debug dir\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("force_dcin_icl_check",
				  S_IFREG | S_IWUSR | S_IRUGO,
				  chip->debug_root, chip,
				  &force_dcin_icl_ops);
	if (!ent) {
		dev_err(chip->dev,
			"Couldn't create force dcin icl check file\n");
		return -EINVAL;
	}

	ent = debugfs_create_file("register_dump",
				  S_IFREG | S_IWUSR | S_IRUGO,
				  chip->debug_root, chip,
				  &cnfg_debugfs_ops);
	if (!ent) {
		dev_err(chip->dev,
			"Couldn't create force dump file\n");
		return -EINVAL;
	}
	return 0;
}

#define CHG_SHOW_MAX_SIZE 50
static ssize_t force_demo_mode_store(struct device *dev,
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

	the_chip->demo_mode = (mode) ? true : false;

	return r ? r : count;
}

static ssize_t force_demo_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	state = (the_chip->demo_mode) ? 1 : 0;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_demo_mode, 0644,
		force_demo_mode_show,
		force_demo_mode_store);

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

	r = smbchg_masked_write_fac(the_chip,
				    the_chip->usb_chgpth_base + CMD_IL,
				    USBIN_SUSPEND_BIT,
				    mode ? USBIN_SUSPEND_BIT : 0);

	return r ? r : count;
}

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

	ret = smbchg_read(the_chip,
			  &value, the_chip->usb_chgpth_base + CMD_IL, 1);
	if (ret) {
		pr_err("USBIN_SUSPEND_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (USBIN_SUSPEND_BIT & value) ? 1 : 0;

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

	/* do nothing for SMBCHG */
	r = 0;

	return r ? r : count;
}

static ssize_t force_chg_fail_clear_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/* do nothing for SMBCHG */
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

	r = smbchg_masked_write_fac(the_chip,
				    the_chip->bat_if_base + CMD_CHG_REG,
				    EN_BAT_CHG_BIT,
				    mode ? 0 : EN_BAT_CHG_BIT);
	if (r < 0) {
		dev_err(the_chip->dev,
			"Couldn't set EN_BAT_CHG_BIT enable = %d r = %d\n",
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

	ret = smbchg_read(the_chip,
			  &value, the_chip->bat_if_base + CMD_CHG_REG, 1);
	if (ret) {
		pr_err("CHG_EN_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (EN_BAT_CHG_BIT & value) ? 0 : 1;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_auto_enable, 0664,
		force_chg_auto_enable_show,
		force_chg_auto_enable_store);

static int smbchg_set_fastchg_current_fac(struct smbchg_chip *chip,
					  int current_ma)
{
	int i, rc;
	u8 cur_val;

	/* the fcc enumerations are the same as the usb currents */
	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_500_MA);

		rc = smbchg_sec_masked_write_fac(chip,
						 chip->chgr_base + FCC_CFG,
						 FCC_MASK,
						 FCC_500MA_VAL);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
				CURRENT_500_MA, rc);

		return rc;
	}

	cur_val = i & FCC_MASK;
	rc = smbchg_sec_masked_write_fac(chip, chip->chgr_base + FCC_CFG,
				FCC_MASK, cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to fcc cfg rc = %d\n", rc);
		return rc;
	}

	return rc;
}

static ssize_t force_chg_ibatt_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		pr_err("Invalid ibatt value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	r = smbchg_set_fastchg_current_fac(the_chip, chg_current);
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

	ret = smbchg_read(the_chip, &value, the_chip->chgr_base + FCC_CFG, 1);
	if (ret) {
		pr_err("Read Fast Charge Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = usb_current_table[value & FCC_MASK];

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_ibatt, 0664,
		force_chg_ibatt_show,
		force_chg_ibatt_store);

static int smbchg_set_high_usb_chg_current_fac(struct smbchg_chip *chip,
					       int current_ma)
{
	int i, rc;
	u8 usb_cur_val;

	for (i = ARRAY_SIZE(usb_current_table) - 1; i >= 0; i--) {
		if (current_ma >= usb_current_table[i])
			break;
	}
	if (i < 0) {
		dev_err(chip->dev,
			"Cannot find %dma current_table using %d\n",
			current_ma, CURRENT_150_MA);

		rc = smbchg_sec_masked_write_fac(chip,
					chip->usb_chgpth_base + CHGPTH_CFG,
					CFG_USB_2_3_SEL_BIT, CFG_USB_2);
		rc |= smbchg_masked_write_fac(chip,
					      chip->usb_chgpth_base + CMD_IL,
					   USBIN_MODE_CHG_BIT | USB51_MODE_BIT,
					   USBIN_LIMITED_MODE | USB51_100MA);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't set %dmA rc=%d\n",
					CURRENT_150_MA, rc);

		return rc;
	}

	usb_cur_val = i & USBIN_INPUT_MASK;
	rc = smbchg_sec_masked_write_fac(chip, chip->usb_chgpth_base + IL_CFG,
				USBIN_INPUT_MASK, usb_cur_val);
	if (rc < 0) {
		dev_err(chip->dev, "cannot write to config c rc = %d\n", rc);
		return rc;
	}

	rc = smbchg_masked_write_fac(chip, chip->usb_chgpth_base + CMD_IL,
				USBIN_MODE_CHG_BIT, USBIN_HC_MODE);
	if (rc < 0)
		dev_err(chip->dev, "Couldn't write cfg 5 rc = %d\n", rc);

	return rc;
}

static ssize_t force_chg_iusb_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long usb_curr;

	r = kstrtoul(buf, 0, &usb_curr);
	if (r) {
		pr_err("Invalid iusb value = %lu\n", usb_curr);
		return -EINVAL;
	}

	if (!the_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	r = smbchg_set_high_usb_chg_current_fac(the_chip,
						usb_curr);
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

	ret = smbchg_read(the_chip, &value,
			  the_chip->usb_chgpth_base + IL_CFG, 1);
	if (ret) {
		pr_err("USBIN Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = usb_current_table[(value & USBIN_INPUT_MASK)];
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_iusb, 0664,
		force_chg_iusb_show,
		force_chg_iusb_store);


#define PRECHG_OFFSET 100
#define PRECHG_STEP 50
#define PRECHG_TOP 250
#define PRECHG_REG_SHIFT 5
#define PRECHG_MASK 0x7
#define PRECHG_CFG 0xF1
#define PRECHG_MAX 550
#define PRECHG_MAX_LVL 0x4
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

	if (chg_current >= PRECHG_MAX) {
		i = PRECHG_MAX_LVL;
		goto prechg_write;
	}

	for (i = PRECHG_TOP; i > PRECHG_OFFSET; i = i - PRECHG_STEP) {
		if (chg_current >= i)
			break;
	}

	i = (i - PRECHG_OFFSET) / PRECHG_STEP;

	i = i & PRECHG_MASK;

prechg_write:
	r = smbchg_sec_masked_write_fac(the_chip,
					the_chip->chgr_base + PRECHG_CFG,
					PRECHG_MASK, i);
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

	ret = smbchg_read(the_chip,
			  &value, the_chip->chgr_base + PRECHG_CFG, 1);
	if (ret) {
		pr_err("Pre-Charge Current failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = value & PRECHG_MASK;

	if (state >= PRECHG_MAX_LVL)
		state = PRECHG_MAX;
	else
		state = (state * PRECHG_STEP) + PRECHG_OFFSET;
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_itrick, 0664,
		   force_chg_itrick_show,
		   force_chg_itrick_store);

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

	if (mode)
		r = smbchg_masked_write_fac(the_chip,
					the_chip->bat_if_base + CMD_CHG_REG,
					(EN_BAT_CHG_BIT | OTG_EN),
					(EN_BAT_CHG_BIT | OTG_EN));
	else
		r = smbchg_masked_write_fac(the_chip,
					the_chip->bat_if_base + CMD_CHG_REG,
					(EN_BAT_CHG_BIT | OTG_EN),
					0);

	if (r < 0)
		dev_err(the_chip->dev,
			"Couldn't set OTG mode = %d r = %d\n",
			(int)mode, (int)r);

	return r ? r : count;
}

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

	ret = smbchg_read(the_chip,
			  &value,
			  the_chip->bat_if_base + CMD_CHG_REG,
			  1);
	if (ret) {
		pr_err("OTG_EN failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (OTG_EN & value) ? 1 : 0;
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_usb_otg_ctl, 0664,
		   force_chg_usb_otg_ctl_show,
		   force_chg_usb_otg_ctl_store);

static bool smbchg_is_max_thermal_level(struct smbchg_chip *chip)
{
	if ((chip->chg_thermal_levels == 0) ||
	    ((chip->chg_thermal_levels > 0) &&
	     ((chip->usb_present) &&
	      ((chip->chg_therm_lvl_sel >= (chip->chg_thermal_levels - 1)) ||
	       (chip->chg_therm_lvl_sel == -EINVAL)))))
		return true;
	else
		return false;
}

static char *smb_health_text[] = {
	"Unknown", "Good", "Overheat", "Warm", "Dead", "Over voltage",
	"Unspecified failure", "Cold", "Cool", "Watchdog timer expire",
	"Safety timer expire"
};

static int smbchg_check_temp_range(struct smbchg_chip *chip,
				   int batt_volt,
				   int batt_soc,
				   int batt_health,
				   int prev_batt_health)
{
	int ext_high_temp = 0;

	if (((batt_health == POWER_SUPPLY_HEALTH_COOL) &&
	     (batt_volt > chip->ext_temp_volt_mv)) ||
	    ((batt_health == POWER_SUPPLY_HEALTH_WARM) &&
	     (batt_soc > chip->ext_temp_soc) &&
	     (smbchg_is_max_thermal_level(chip))))
		ext_high_temp = 1;

	if ((prev_batt_health == POWER_SUPPLY_HEALTH_COOL) &&
	    (batt_health == POWER_SUPPLY_HEALTH_COOL) &&
	    !chip->ext_high_temp)
		ext_high_temp = 0;

	if (chip->ext_high_temp != ext_high_temp) {
		chip->ext_high_temp = ext_high_temp;
		dev_err(chip->dev, "Ext High = %s\n",
			chip->ext_high_temp ? "High" : "Low");

		return 1;
	}

	return 0;
}

#define HYSTERISIS_DEGC 2
static void smbchg_check_temp_state(struct smbchg_chip *chip, int batt_temp)
{
	int hotspot;
	int temp_state = POWER_SUPPLY_HEALTH_GOOD;

	if (!chip)
		return;

	mutex_lock(&chip->check_temp_lock);

	/* Convert to Degrees C */
	hotspot = chip->hotspot_temp / 1000;

	/* Override batt_temp if battery hot spot condition
	   is active */
	if ((batt_temp > chip->cool_temp_c) &&
	    (hotspot > batt_temp) &&
	    (hotspot >= chip->hotspot_thrs_c)) {
		batt_temp = hotspot;
	}

	if (chip->temp_state == POWER_SUPPLY_HEALTH_WARM) {
		if (batt_temp >= chip->hot_temp_c)
			/* Warm to Hot */
			temp_state = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (batt_temp <=
			 chip->warm_temp_c - HYSTERISIS_DEGC)
			/* Warm to Normal */
			temp_state = POWER_SUPPLY_HEALTH_GOOD;
		else
			/* Stay Warm */
			temp_state = POWER_SUPPLY_HEALTH_WARM;
	} else if ((chip->temp_state == POWER_SUPPLY_HEALTH_GOOD) ||
		   (chip->temp_state == POWER_SUPPLY_HEALTH_UNKNOWN)) {
		if (batt_temp >= chip->warm_temp_c)
			/* Normal to Warm */
			temp_state = POWER_SUPPLY_HEALTH_WARM;
		else if (batt_temp <= chip->cool_temp_c)
			/* Normal to Cool */
			temp_state = POWER_SUPPLY_HEALTH_COOL;
		else
			/* Stay Normal */
			temp_state = POWER_SUPPLY_HEALTH_GOOD;
	} else if (chip->temp_state == POWER_SUPPLY_HEALTH_COOL) {
		if (batt_temp >=
		    chip->cool_temp_c + HYSTERISIS_DEGC)
			/* Cool to Normal */
			temp_state = POWER_SUPPLY_HEALTH_GOOD;
		else if (batt_temp <= chip->cold_temp_c)
			/* Cool to Cold */
			temp_state = POWER_SUPPLY_HEALTH_COLD;
		else
			/* Stay Cool */
			temp_state = POWER_SUPPLY_HEALTH_COOL;
	} else if (chip->temp_state == POWER_SUPPLY_HEALTH_COLD) {
		if (batt_temp >=
		    chip->cold_temp_c + HYSTERISIS_DEGC)
			/* Cold to Cool */
			temp_state = POWER_SUPPLY_HEALTH_COOL;
		else
			/* Stay Cold */
			temp_state = POWER_SUPPLY_HEALTH_COLD;
	} else if (chip->temp_state == POWER_SUPPLY_HEALTH_OVERHEAT) {
		if (batt_temp <=
		    chip->hot_temp_c - HYSTERISIS_DEGC)
			/* Hot to Warm */
			temp_state = POWER_SUPPLY_HEALTH_WARM;
		else
			/* Stay Hot */
			temp_state = POWER_SUPPLY_HEALTH_OVERHEAT;
	}

	if (chip->temp_state != temp_state) {
		chip->temp_state = temp_state;
		dev_err(chip->dev, "Battery Temp State = %s\n",
			smb_health_text[chip->temp_state]);
	}
	mutex_unlock(&chip->check_temp_lock);

	return;
}

static void smbchg_set_temp_chgpath(struct smbchg_chip *chip, int prev_temp)
{
	if (chip->factory_mode)
		return;

	if (((chip->temp_state == POWER_SUPPLY_HEALTH_COOL) &&
	     !chip->ext_high_temp) || chip->demo_mode)
		smbchg_float_voltage_set(chip,
					  chip->ext_temp_volt_mv);
	else {
		smbchg_float_voltage_set(chip, chip->vfloat_mv);
		if (is_usb_present(chip))
			smbchg_set_parallel_vfloat(chip,
						   chip->vfloat_parallel_mv);
	}

	if (chip->ext_high_temp ||
	    (chip->temp_state == POWER_SUPPLY_HEALTH_COLD) ||
	    (chip->temp_state == POWER_SUPPLY_HEALTH_OVERHEAT) ||
	    (chip->stepchg_state == STEP_FULL))
		smbchg_charging_en(chip, 0);
	else {
		if ((prev_temp == POWER_SUPPLY_HEALTH_COOL) &&
		    (chip->temp_state == POWER_SUPPLY_HEALTH_GOOD)) {
			smbchg_charging_en(chip, 0);
			mdelay(10);
		}
		smbchg_charging_en(chip, 1);
	}
}

static void smbchg_sync_accy_property_status(struct smbchg_chip *chip)
{
	bool usb_present = is_usb_present(chip);
	bool dc_present = is_dc_present(chip);
	u8 reg = 0;
	int rc;

	if (chip->apsd_rerun_cnt)
		return;

	rc = smbchg_read(chip, &reg, chip->usb_chgpth_base + RT_STS, 1);
	if (rc < 0) {
		dev_err(chip->dev, "Couldn't read usb rt status rc = %d\n", rc);
		return;
	}
	reg &= USBIN_SRC_DET_BIT;

	mutex_lock(&chip->usb_set_present_lock);
	if (reg && !chip->usb_present && usb_present) {
		/* USB inserted */
		chip->usb_present = usb_present;
		handle_usb_insertion(chip);
	} else if (!reg && chip->usb_present && !usb_present) {
		/* USB removed */
		chip->usb_present = usb_present;
		handle_usb_removal(chip);
	}
	mutex_unlock(&chip->usb_set_present_lock);

	mutex_lock(&chip->dc_set_present_lock);
	if (!chip->dc_present && dc_present) {
		/* dc inserted */
		chip->dc_present = dc_present;
		handle_dc_insertion(chip);
	} else if (chip->dc_present && !dc_present) {
		/* dc removed */
		chip->dc_present = dc_present;
		handle_dc_removal(chip);
	}
	mutex_unlock(&chip->dc_set_present_lock);

	/* DCIN online status is determined based on GPIO status. */
	if (chip->usb_present != chip->usb_online)
		schedule_work(&chip->usb_set_online_work);
}

#define AICL_WAIT_COUNT 3
static bool smbchg_check_and_kick_aicl(struct smbchg_chip *chip)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (chip->factory_mode || chip->demo_mode)
		return false;

	if (chip->usb_psy && !chip->usb_psy->get_property(chip->usb_psy,
							POWER_SUPPLY_PROP_TYPE,
							&prop)) {
		if ((prop.intval != POWER_SUPPLY_TYPE_USB_HVDCP) &&
			(prop.intval != POWER_SUPPLY_TYPE_USB_DCP)) {
			chip->aicl_wait_retries = 0;
			return false;
		}
	}

	if (!is_usb_present(chip) || smbchg_is_aicl_complete(chip)) {
		chip->aicl_wait_retries = 0;
		return false;
	}

	if (++chip->aicl_wait_retries > AICL_WAIT_COUNT) {
		chip->aicl_wait_retries = 0;
		dev_err(chip->dev, "AICL Stuck, Re-run APSD\n");
		rc = smbchg_force_apsd(chip);
		if (rc < 0)
			dev_err(chip->dev, "Couldn't rerun apsd rc = %d\n", rc);
		return true;
	} else
		return false;
}

#define HEARTBEAT_DELAY_MS 60000
#define HEARTBEAT_HOLDOFF_MS 10000
#define STEPCHG_MAX_FV_COMP 60
#define STEPCHG_ONE_FV_COMP 40
#define STEPCHG_FULL_FV_COMP 100
#define STEPCHG_CURR_ADJ 200
#define DEMO_MODE_MAX_SOC 35
#define DEMO_MODE_HYS_SOC 5
#define HYST_STEP_MV 50
static void smbchg_heartbeat_work(struct work_struct *work)
{
	struct smbchg_chip *chip = container_of(work,
						struct smbchg_chip,
						heartbeat_work.work);
	int batt_mv;
	int batt_ma;
	int batt_soc;
	int batt_temp;
	int prev_batt_health;
	int prev_ext_lvl;
	int prev_step;
	int index;


	smbchg_stay_awake(chip, PM_HEARTBEAT);
	if (smbchg_check_and_kick_aicl(chip))
		goto end_hb;
	set_property_on_fg(chip, POWER_SUPPLY_PROP_UPDATE_NOW, 1);
	batt_mv = get_prop_batt_voltage_now(chip) / 1000;
	batt_ma = get_prop_batt_current_now(chip) / 1000;
	batt_soc = get_prop_batt_capacity(chip);

	batt_temp = get_prop_batt_temp(chip) / 10;
	dev_dbg(chip->dev, "batt=%d mV, %d mA, %d C\n",
		batt_mv, batt_ma, batt_temp);
	smbchg_sync_accy_property_status(chip);

	prev_step = chip->stepchg_state;

	if (chip->demo_mode) {
		chip->stepchg_state = STEP_NONE;
		dev_warn(chip->dev, "Battery in Demo Mode charging Limited\n");
		if ((!!!(chip->usb_suspended & REASON_DEMO)) &&
		    (batt_soc >= DEMO_MODE_MAX_SOC)) {
			smbchg_usb_en(chip, false, REASON_DEMO);
			smbchg_dc_en(chip, false, REASON_DEMO);
		} else if (!!(chip->usb_suspended & REASON_DEMO) &&
			(batt_soc <=
			 (DEMO_MODE_MAX_SOC - DEMO_MODE_HYS_SOC))) {
			smbchg_usb_en(chip, true, REASON_DEMO);
			smbchg_dc_en(chip, true, REASON_DEMO);
		}
	} else if ((chip->stepchg_state == STEP_NONE) && (chip->usb_present)) {
		if (batt_mv >= chip->stepchg_voltage_mv)
			chip->stepchg_state = STEP_ONE;
		else
			chip->stepchg_state = STEP_MAX;
		chip->stepchg_state_holdoff = 0;
	} else if ((chip->stepchg_state == STEP_MAX) &&
		   (batt_ma < 0) && (chip->usb_present) &&
		   ((batt_mv + HYST_STEP_MV) >= chip->stepchg_voltage_mv)) {
		batt_ma *= -1;

		index = smbchg_get_pchg_current_map_index(chip);
		if (chip->pchg_current_map_data[index].primary ==
		    chip->stepchg_current_ma)
			batt_ma -= STEPCHG_CURR_ADJ;

		if ((batt_ma <= chip->stepchg_current_ma) &&
		    (chip->allowed_fastchg_current_ma >=
		     chip->stepchg_current_ma))
			if (chip->stepchg_state_holdoff >= 2) {
				chip->stepchg_state = STEP_ONE;
				chip->stepchg_state_holdoff = 0;
			} else
				chip->stepchg_state_holdoff++;
		else
			chip->stepchg_state_holdoff = 0;
	} else if ((chip->stepchg_state == STEP_ONE) &&
		   (batt_ma < 0) && (chip->usb_present) &&
		   ((batt_mv + HYST_STEP_MV) >=
		    chip->stepchg_max_voltage_mv)) {
		batt_ma *= -1;
		if ((batt_ma <= chip->stepchg_taper_ma) &&
		    (chip->allowed_fastchg_current_ma >=
		     chip->stepchg_taper_ma))
			if (chip->stepchg_state_holdoff >= 2) {
				chip->stepchg_state = STEP_TAPER;
				chip->stepchg_state_holdoff = 0;
			} else
				chip->stepchg_state_holdoff++;
		else
			chip->stepchg_state_holdoff = 0;
	} else if ((chip->stepchg_state == STEP_TAPER) &&
		   (batt_ma < 0) && (chip->usb_present)) {
		batt_ma *= -1;
		if ((batt_ma <= chip->stepchg_iterm_ma) &&
		    (chip->allowed_fastchg_current_ma >=
		     chip->stepchg_iterm_ma))
			if (chip->stepchg_state_holdoff >= 2) {
				chip->stepchg_state = STEP_FULL;
				chip->stepchg_state_holdoff = 0;
			} else
				chip->stepchg_state_holdoff++;
		else
			chip->stepchg_state_holdoff = 0;
	}  else if ((chip->stepchg_state == STEP_FULL) &&
		    (chip->usb_present) && (batt_soc < 100)) {
		chip->stepchg_state = STEP_TAPER;
	} else if (!chip->usb_present) {
		chip->stepchg_state = STEP_NONE;
		chip->stepchg_state_holdoff = 0;
	} else
		chip->stepchg_state_holdoff = 0;

	switch (chip->stepchg_state) {
	case STEP_FULL:
	case STEP_TAPER:
		if (smbchg_hvdcp_det_check(chip) &&
		    (chip->usb_target_current_ma != HVDCP_ICL_TAPER)) {
			mutex_lock(&chip->current_change_lock);
			chip->usb_target_current_ma = HVDCP_ICL_TAPER;
			mutex_unlock(&chip->current_change_lock);
		}
		chip->vfloat_mv = chip->stepchg_max_voltage_mv;
		chip->vfloat_parallel_mv =
			chip->stepchg_max_voltage_mv - STEPCHG_FULL_FV_COMP;
		set_max_allowed_current_ma(chip, chip->stepchg_current_ma);
		break;
	case STEP_ONE:
	case STEP_NONE:
		chip->vfloat_mv =
			chip->stepchg_max_voltage_mv + STEPCHG_ONE_FV_COMP;
		chip->vfloat_parallel_mv = chip->stepchg_max_voltage_mv;
		set_max_allowed_current_ma(chip, chip->stepchg_current_ma);
		break;
	case STEP_MAX:
		chip->vfloat_mv =
			chip->stepchg_max_voltage_mv + STEPCHG_MAX_FV_COMP;
		chip->vfloat_parallel_mv =
			chip->stepchg_voltage_mv + STEPCHG_MAX_FV_COMP;
		set_max_allowed_current_ma(chip, chip->stepchg_max_current_ma);
		break;
	default:
		break;
	}

	dev_dbg(chip->dev, "Step State = %d\n",
		(int)chip->stepchg_state);

	prev_batt_health = chip->temp_state;
	smbchg_check_temp_state(chip, batt_temp);
	prev_ext_lvl = chip->ext_high_temp;
	smbchg_check_temp_range(chip, batt_mv, batt_soc,
				chip->temp_state, prev_batt_health);
	if ((prev_batt_health != chip->temp_state) ||
	    (prev_ext_lvl != chip->ext_high_temp) ||
	    (prev_step != chip->stepchg_state) ||
	    (chip->update_allowed_fastchg_current_ma)) {
		smbchg_set_temp_chgpath(chip, prev_batt_health);
		if (chip->usb_present) {
			smbchg_parallel_usb_check_ok(chip);
			chip->update_allowed_fastchg_current_ma = false;
		} else if (chip->dc_present) {
			smbchg_set_fastchg_current(chip,
					      chip->target_fastchg_current_ma);
		}
	}

end_hb:
	power_supply_changed(&chip->batt_psy);

	if (!chip->stepchg_state_holdoff && !chip->aicl_wait_retries)
		schedule_delayed_work(&chip->heartbeat_work,
				      msecs_to_jiffies(HEARTBEAT_DELAY_MS));
	else
		schedule_delayed_work(&chip->heartbeat_work,
				      msecs_to_jiffies(HEARTBEAT_HOLDOFF_MS));

	smbchg_relax(chip, PM_HEARTBEAT);
}

static bool smbchg_charger_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}

static bool qpnp_smbcharger_test_mode(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	const char *mode;
	int rc;
	bool test = false;

	if (!np)
		return test;

	rc = of_property_read_string(np, "mmi,battery", &mode);
	if ((rc >= 0) && mode) {
		if (strcmp(mode, "test") == 0)
			test = true;
	}
	of_node_put(np);

	return test;
}

static int smbchg_reboot(struct notifier_block *nb,
			 unsigned long event, void *unused)
{
	struct smbchg_chip *chip =
			container_of(nb, struct smbchg_chip, smb_reboot);

	dev_dbg(chip->dev, "SMB Reboot\n");
	if (!chip) {
		dev_warn(chip->dev, "called before chip valid!\n");
		return NOTIFY_DONE;
	}

	if (chip->factory_mode) {
		switch (event) {
		case SYS_POWER_OFF:
			/* Disable Charging */
			smbchg_charging_en(chip, 0);

			/* Suspend USB and DC */
			smbchg_usb_suspend(chip, true);
			smbchg_dc_suspend(chip, true);

			while (is_usb_present(chip))
				msleep(100);
			dev_warn(chip->dev, "VBUS UV wait 1 sec!\n");
			/* Delay 1 sec to allow more VBUS decay */
			msleep(1000);
			break;
		default:
			break;
		}
	}
	return NOTIFY_DONE;
}

#define DEFAULT_TEST_MODE_SOC  52
#define DEFAULT_TEST_MODE_TEMP  225
static int smbchg_probe(struct spmi_device *spmi)
{
	int rc;
	struct smbchg_chip *chip;
	struct power_supply *usb_psy;
	struct qpnp_vadc_chip *vadc_dev;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_smb(PR_STATUS, "USB supply not found, deferring probe\n");
		return -EPROBE_DEFER;
	}

	if (of_find_property(spmi->dev.of_node, "qcom,dcin-vadc", NULL)) {
		vadc_dev = qpnp_get_vadc(&spmi->dev, "dcin");
		if (IS_ERR(vadc_dev)) {
			rc = PTR_ERR(vadc_dev);
			if (rc != -EPROBE_DEFER)
				dev_err(&spmi->dev, "Couldn't get vadc rc=%d\n",
						rc);
			return rc;
		}
	}

	chip = devm_kzalloc(&spmi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	chip->factory_mode = smbchg_charger_mmi_factory();
	if (chip->factory_mode) {
		dev_warn(&spmi->dev,
			 "Entering Factory Mode SMB Writes Disabled\n");
		chip->apsd_rerun_cnt = 1;
	}

	wakeup_source_init(&chip->smbchg_wake_source, "smbchg_wake");
	INIT_WORK(&chip->usb_set_online_work, smbchg_usb_update_online_work);
	INIT_DELAYED_WORK(&chip->parallel_en_work,
			smbchg_parallel_usb_en_work);
	INIT_DELAYED_WORK(&chip->vfloat_adjust_work, smbchg_vfloat_adjust_work);
	INIT_DELAYED_WORK(&chip->hvdcp_det_work, smbchg_hvdcp_det_work);
	INIT_DELAYED_WORK(&chip->heartbeat_work,
			  smbchg_heartbeat_work);
	INIT_DELAYED_WORK(&chip->usb_insertion_work,
			  usb_insertion_work);
	chip->vadc_dev = vadc_dev;
	chip->spmi = spmi;
	chip->dev = &spmi->dev;
	chip->usb_psy = usb_psy;
	chip->demo_mode = false;
	chip->test_mode_soc = DEFAULT_TEST_MODE_SOC;
	chip->test_mode_temp = DEFAULT_TEST_MODE_TEMP;
	chip->test_mode = qpnp_smbcharger_test_mode();
	if (chip->test_mode)
		dev_warn(&spmi->dev, "Test Mode Enabled\n");

	chip->usb_online = -EINVAL;
	chip->stepchg_state = STEP_NONE;
	smbchg_parallel_en = 1;
	chip->charger_rate =  POWER_SUPPLY_CHARGE_RATE_NONE;
	dev_set_drvdata(&spmi->dev, chip);

	spin_lock_init(&chip->sec_access_lock);
	mutex_init(&chip->fcc_lock);
	mutex_init(&chip->current_change_lock);
	mutex_init(&chip->usb_set_online_lock);
	mutex_init(&chip->usb_set_present_lock);
	mutex_init(&chip->dc_set_present_lock);
	mutex_init(&chip->battchg_disabled_lock);
	mutex_init(&chip->usb_en_lock);
	mutex_init(&chip->dc_en_lock);
	mutex_init(&chip->parallel.lock);
	mutex_init(&chip->taper_irq_lock);
	mutex_init(&chip->pm_lock);
	mutex_init(&chip->wipower_config);
	mutex_init(&chip->check_temp_lock);

	rc = smbchg_parse_peripherals(chip);
	if (rc) {
		dev_err(chip->dev, "Error parsing DT peripherals: %d\n", rc);
		return rc;
	}
	rc = smb_parse_dt(chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "Unable to parse DT nodes: %d\n", rc);
		return rc;
	}

	rc = smbchg_regulator_init(chip);
	if (rc) {
		dev_err(&spmi->dev,
			"Couldn't initialize regulator rc=%d\n", rc);
		return rc;
	}

	rc = smbchg_hw_init(chip);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to intialize hardware rc = %d\n", rc);
		goto free_regulator;
	}

	rc = determine_initial_status(chip);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to determine init status rc = %d\n", rc);
		goto free_regulator;
	}

	chip->previous_soc = -EINVAL;
	chip->batt_psy.name		= chip->battery_psy_name;
	chip->batt_psy.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->batt_psy.get_property	= smbchg_battery_get_property;
	chip->batt_psy.set_property	= smbchg_battery_set_property;
	chip->batt_psy.properties	= smbchg_battery_properties;
	chip->batt_psy.num_properties	= ARRAY_SIZE(smbchg_battery_properties);
	chip->batt_psy.external_power_changed = smbchg_external_power_changed;
	chip->batt_psy.property_is_writeable = smbchg_battery_is_writeable;

	rc = power_supply_register(chip->dev, &chip->batt_psy);
	if (rc < 0) {
		dev_err(&spmi->dev,
			"Unable to register batt_psy rc = %d\n", rc);
		goto free_regulator;
	}

	if (chip->dc_psy_type != -EINVAL) {
		chip->dc_psy.name		= "dc";
		chip->dc_psy.type		= chip->dc_psy_type;
		chip->dc_psy.get_property	= smbchg_dc_get_property;
		chip->dc_psy.set_property	= smbchg_dc_set_property;
		chip->dc_psy.property_is_writeable = smbchg_dc_is_writeable;
		chip->dc_psy.properties		= smbchg_dc_properties;
		chip->dc_psy.num_properties = ARRAY_SIZE(smbchg_dc_properties);
		rc = power_supply_register(chip->dev, &chip->dc_psy);
		if (rc < 0) {
			dev_err(&spmi->dev,
				"Unable to register dc_psy rc = %d\n", rc);
			goto unregister_batt_psy;
		}
	}
	chip->psy_registered = true;

	rc = smbchg_request_irqs(chip);
	if (rc < 0) {
		dev_err(&spmi->dev, "Unable to request irqs rc = %d\n", rc);
		goto unregister_dc_psy;
	}

	power_supply_set_present(chip->usb_psy, chip->usb_present);

	chip->smb_reboot.notifier_call = smbchg_reboot;
	chip->smb_reboot.next = NULL;
	chip->smb_reboot.priority = 1;
	rc = register_reboot_notifier(&chip->smb_reboot);
	if (rc)
		dev_err(chip->dev, "register for reboot failed\n");

	dump_regs(chip);
	create_debugfs_entries(chip);

	the_chip = chip;

	rc = device_create_file(chip->dev,
				&dev_attr_force_demo_mode);
	if (rc) {
		pr_err("couldn't create force_demo_mode\n");
		goto unregister_dc_psy;
	}

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
	smbchg_stay_awake(chip, PM_HEARTBEAT);
	schedule_delayed_work(&chip->heartbeat_work,
			      msecs_to_jiffies(0));

	dev_info(chip->dev, "SMBCHG successfully probed batt=%d dc = %d usb = %d\n",
			get_prop_batt_present(chip),
			chip->dc_present, chip->usb_present);
	return 0;

unregister_dc_psy:
	power_supply_unregister(&chip->dc_psy);
unregister_batt_psy:
	power_supply_unregister(&chip->batt_psy);
free_regulator:
	smbchg_regulator_deinit(chip);
	mutex_lock(&chip->usb_set_present_lock);
	handle_usb_removal(chip);
	mutex_unlock(&chip->usb_set_present_lock);
	mutex_lock(&chip->dc_set_present_lock);
	handle_dc_removal(chip);
	mutex_unlock(&chip->dc_set_present_lock);
	wakeup_source_trash(&chip->smbchg_wake_source);
	return rc;
}

static int smbchg_remove(struct spmi_device *spmi)
{
	struct smbchg_chip *chip = dev_get_drvdata(&spmi->dev);

	debugfs_remove_recursive(chip->debug_root);

	if (chip->dc_psy_type != -EINVAL)
		power_supply_unregister(&chip->dc_psy);

	device_remove_file(chip->dev,
			   &dev_attr_force_demo_mode);
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

	power_supply_unregister(&chip->batt_psy);
	smbchg_regulator_deinit(chip);
	wakeup_source_trash(&chip->smbchg_wake_source);

	return 0;
}

static int smbchg_suspend(struct device *dev)
{
	struct smbchg_chip *chip = dev_get_drvdata(dev);

	if (chip->wake_reasons)
		return -EAGAIN;

	return 0;
}

static int smbchg_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops smbchg_pm_ops = {
	.resume		= smbchg_resume,
	.suspend	= smbchg_suspend,
};

MODULE_DEVICE_TABLE(spmi, smbchg_id);

static struct spmi_driver smbchg_driver = {
	.driver		= {
		.name		= "qpnp-smbcharger",
		.owner		= THIS_MODULE,
		.of_match_table	= smbchg_match_table,
		.pm		= &smbchg_pm_ops,
	},
	.probe		= smbchg_probe,
	.remove		= smbchg_remove,
};

static int __init smbchg_init(void)
{
	return spmi_driver_register(&smbchg_driver);
}

static void __exit smbchg_exit(void)
{
	return spmi_driver_unregister(&smbchg_driver);
}

module_init(smbchg_init);
module_exit(smbchg_exit);

MODULE_DESCRIPTION("QPNP SMB Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:qpnp-smbcharger");
