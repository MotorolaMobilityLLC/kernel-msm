/* Copyright (c) 2016-2018 The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/iio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/input/qpnp-power-on.h>
#include <linux/interrupt.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/pinctrl/consumer.h>
#include <soc/qcom/watchdog.h>
#include <linux/usb/usbpd.h>
#include <linux/pmic-voter.h>
#include "smb-lib.h"
#include "smb-reg.h"
#include "battery.h"
#include "step-chg-jeita.h"
#include "storm-watch.h"

#ifdef QCOM_BASE
#define smblib_err(chg, fmt, ...)		\
	pr_err("%s: %s: " fmt, chg->name,	\
		__func__, ##__VA_ARGS__)	\

#define smblib_dbg(chg, reason, fmt, ...)			\
	do {							\
		if (*chg->debug_mask & (reason))		\
			pr_info("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_debug("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
	} while (0)

#else
#define smblib_err(chg, fmt, ...)			\
	do {						\
		pr_err("%s: %s: " fmt, chg->name,	\
		       __func__, ##__VA_ARGS__);	\
		ipc_log_string(chg->ipc_log,		\
		"ERR:%s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)

#define smblib_dbg(chg, reason, fmt, ...)			\
	do {							\
		if (*chg->debug_mask & (reason))		\
			pr_info("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		else						\
			pr_debug("%s: %s: " fmt, chg->name,	\
				__func__, ##__VA_ARGS__);	\
		if ((PR_REGISTER) & (reason))			\
			ipc_log_string(chg->ipc_log_reg,	\
			"REG:%s: " fmt, __func__, ##__VA_ARGS__); \
		else						\
			ipc_log_string(chg->ipc_log,		\
			"INFO:%s: " fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#endif

#define QPNP_LOG_PAGES (50)

static bool is_secure(struct smb_charger *chg, int addr)
{
	if (addr == SHIP_MODE_REG || addr == FREQ_CLK_DIV_REG)
		return true;
	/* assume everything above 0xA0 is secure */
	return (bool)((addr & 0xFF) >= 0xA0);
}

int smblib_read(struct smb_charger *chg, u16 addr, u8 *val)
{
	unsigned int temp;
	int rc = 0;

	rc = regmap_read(chg->regmap, addr, &temp);
	if (rc >= 0)
		*val = (u8)temp;

	return rc;
}

int smblib_multibyte_read(struct smb_charger *chg, u16 addr, u8 *val,
				int count)
{
	return regmap_bulk_read(chg->regmap, addr, val, count);
}

int smblib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val)
{
	int rc = 0;

	mutex_lock(&chg->write_lock);
	if (is_secure(chg, addr)) {
		rc = regmap_write(chg->regmap, (addr & 0xFF00) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_update_bits(chg->regmap, addr, mask, val);

unlock:
	mutex_unlock(&chg->write_lock);
	return rc;
}

int smblib_write(struct smb_charger *chg, u16 addr, u8 val)
{
	int rc = 0;

	mutex_lock(&chg->write_lock);

	if (is_secure(chg, addr)) {
		rc = regmap_write(chg->regmap, (addr & ~(0xFF)) | 0xD0, 0xA5);
		if (rc < 0)
			goto unlock;
	}

	rc = regmap_write(chg->regmap, addr, val);

unlock:
	mutex_unlock(&chg->write_lock);
	return rc;
}

static int smblib_get_jeita_cc_delta(struct smb_charger *chg, int *cc_delta_ua)
{
	int rc, cc_minus_ua;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_2_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}

	if (!(stat & BAT_TEMP_STATUS_SOFT_LIMIT_MASK)) {
		*cc_delta_ua = 0;
		return 0;
	}

	rc = smblib_get_charge_param(chg, &chg->param.jeita_cc_comp,
					&cc_minus_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get jeita cc minus rc=%d\n", rc);
		return rc;
	}

	*cc_delta_ua = -cc_minus_ua;
	return 0;
}

int smblib_icl_override(struct smb_charger *chg, bool override)
{
	int rc;

	if (chg->mmi.factory_mode)
		override = true;

	rc = smblib_masked_write(chg, USBIN_LOAD_CFG_REG,
				ICL_OVERRIDE_AFTER_APSD_BIT,
				override ? ICL_OVERRIDE_AFTER_APSD_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't override ICL rc=%d\n", rc);

	return rc;
}

/********************
 * REGISTER GETTERS *
 ********************/

int smblib_get_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int *val_u)
{
	int rc = 0;
	u8 val_raw;

	rc = smblib_read(chg, param->reg, &val_raw);
	if (rc < 0) {
		smblib_err(chg, "%s: Couldn't read from 0x%04x rc=%d\n",
			param->name, param->reg, rc);
		return rc;
	}

	if (param->get_proc)
		*val_u = param->get_proc(param, val_raw);
	else
		*val_u = val_raw * param->step_u + param->min_u;
	smblib_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		   param->name, *val_u, val_raw);

	return rc;
}

int smblib_get_usb_suspend(struct smb_charger *chg, int *suspend)
{
	int rc = 0;
	u8 temp;

	rc = smblib_read(chg, USBIN_CMD_IL_REG, &temp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USBIN_CMD_IL rc=%d\n", rc);
		return rc;
	}
	*suspend = temp & USBIN_SUSPEND_BIT;

	return rc;
}

struct apsd_result {
	const char * const name;
	const u8 bit;
	const enum power_supply_type pst;
};

enum {
	UNKNOWN,
	SDP,
	CDP,
	DCP,
	OCP,
	FLOAT,
	HVDCP2,
	HVDCP3,
	MAX_TYPES
};

static const struct apsd_result const smblib_apsd_results[] = {
	[UNKNOWN] = {
		.name	= "UNKNOWN",
		.bit	= 0,
		.pst	= POWER_SUPPLY_TYPE_USB
	},
	[SDP] = {
		.name	= "SDP",
		.bit	= SDP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB
	},
	[CDP] = {
		.name	= "CDP",
		.bit	= CDP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_CDP
	},
	[DCP] = {
		.name	= "DCP",
		.bit	= DCP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_DCP
	},
	[OCP] = {
		.name	= "OCP",
		.bit	= OCP_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_DCP
	},
	[FLOAT] = {
		.name	= "FLOAT",
		.bit	= FLOAT_CHARGER_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_FLOAT
	},
	[HVDCP2] = {
		.name	= "HVDCP2",
		.bit	= DCP_CHARGER_BIT | QC_2P0_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_DCP
	},
	[HVDCP3] = {
		.name	= "HVDCP3",
		.bit	= DCP_CHARGER_BIT | QC_3P0_BIT,
		.pst	= POWER_SUPPLY_TYPE_USB_DCP,
	},
};

static const struct apsd_result *smblib_get_apsd_result(struct smb_charger *chg)
{
	int rc, i;
	u8 apsd_stat, stat;
	const struct apsd_result *result = &smblib_apsd_results[UNKNOWN];

	rc = smblib_read(chg, APSD_STATUS_REG, &apsd_stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return result;
	}
	smblib_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", apsd_stat);

	if (!(apsd_stat & APSD_DTC_STATUS_DONE_BIT))
		return result;

	rc = smblib_read(chg, APSD_RESULT_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_RESULT_STATUS rc=%d\n",
			rc);
		return result;
	}
	stat &= APSD_RESULT_STATUS_MASK;

	for (i = 0; i < ARRAY_SIZE(smblib_apsd_results); i++) {
		if (smblib_apsd_results[i].bit == stat)
			result = &smblib_apsd_results[i];
	}

	if (apsd_stat & QC_CHARGER_BIT) {
		/* since its a qc_charger, either return HVDCP3 or HVDCP2 */
		if (result != &smblib_apsd_results[HVDCP3])
			result = &smblib_apsd_results[HVDCP2];
	}

	return result;
}

/********************
 * REGISTER SETTERS *
 ********************/

static int chg_freq_list[] = {
	9600, 9600, 6400, 4800, 3800, 3200, 2700, 2400, 2100, 1900, 1700,
	1600, 1500, 1400, 1300, 1200,
};

int smblib_set_chg_freq(struct smb_chg_param *param,
				int val_u, u8 *val_raw)
{
	u8 i;

	if (val_u > param->max_u || val_u < param->min_u)
		return -EINVAL;

	/* Charger FSW is the configured freqency / 2 */
	val_u *= 2;
	for (i = 0; i < ARRAY_SIZE(chg_freq_list); i++) {
		if (chg_freq_list[i] == val_u)
			break;
	}
	if (i == ARRAY_SIZE(chg_freq_list)) {
		pr_err("Invalid frequency %d Hz\n", val_u / 2);
		return -EINVAL;
	}

	*val_raw = i;

	return 0;
}

static int smblib_set_opt_freq_buck(struct smb_charger *chg, int fsw_khz)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_buck, fsw_khz);
	if (rc < 0)
		dev_err(chg->dev, "Error in setting freq_buck rc=%d\n", rc);

	if (chg->mode == PARALLEL_MASTER && chg->pl.psy) {
		pval.intval = fsw_khz;
		/*
		 * Some parallel charging implementations may not have
		 * PROP_BUCK_FREQ property - they could be running
		 * with a fixed frequency
		 */
		power_supply_set_property(chg->pl.psy,
				POWER_SUPPLY_PROP_BUCK_FREQ, &pval);
	}

	return rc;
}

int smblib_set_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int val_u)
{
	int rc = 0;
	u8 val_raw;

	if (param->set_proc) {
		rc = param->set_proc(param, val_u, &val_raw);
		if (rc < 0)
			return -EINVAL;
	} else {
#ifdef QCOM_BASE
		if (val_u > param->max_u || val_u < param->min_u) {
			smblib_err(chg, "%s: %d is out of range [%d, %d]\n",
				param->name, val_u, param->min_u, param->max_u);
			return -EINVAL;
		}
#else
		if (val_u > param->max_u) {
			smblib_err(chg,
				   "%s: %d > [%d max], Limit imposed \n",
				   param->name, val_u, param->max_u);
			val_u = param->max_u;
		} else if (val_u < param->min_u) {
			smblib_err(chg, "%s: %d < [%d min], Limit imposed \n",
				param->name, val_u, param->min_u);
			val_u = param->min_u;
		}
#endif
		val_raw = (val_u - param->min_u) / param->step_u;
	}

	rc = smblib_write(chg, param->reg, val_raw);
	if (rc < 0) {
		smblib_err(chg, "%s: Couldn't write 0x%02x to 0x%04x rc=%d\n",
			param->name, val_raw, param->reg, rc);
		return rc;
	}

	smblib_dbg(chg, PR_REGISTER, "%s = %d (0x%02x)\n",
		   param->name, val_u, val_raw);

	return rc;
}

int smblib_set_usb_suspend(struct smb_charger *chg, bool suspend)
{
	int rc = 0;
	int irq = chg->irq_info[USBIN_ICL_CHANGE_IRQ].irq;

	if (suspend && irq) {
		if (chg->usb_icl_change_irq_enabled) {
			disable_irq_nosync(irq);
			chg->usb_icl_change_irq_enabled = false;
		}
	}

	rc = smblib_masked_write(chg, USBIN_CMD_IL_REG, USBIN_SUSPEND_BIT,
				 suspend ? USBIN_SUSPEND_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't write %s to USBIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	if (!suspend && irq) {
		if (!chg->usb_icl_change_irq_enabled) {
			enable_irq(irq);
			chg->usb_icl_change_irq_enabled = true;
		}
	}

	return rc;
}

int smblib_set_dc_suspend(struct smb_charger *chg, bool suspend)
{
	int rc = 0;

	rc = smblib_masked_write(chg, DCIN_CMD_IL_REG, DCIN_SUSPEND_BIT,
				 suspend ? DCIN_SUSPEND_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't write %s to DCIN_SUSPEND_BIT rc=%d\n",
			suspend ? "suspend" : "resume", rc);

	return rc;
}

static int smblib_set_adapter_allowance(struct smb_charger *chg,
					u8 allowed_voltage)
{
	int rc = 0;

	/* PM660 only support max. 9V */
	if (chg->smb_version == PM660_SUBTYPE) {
		switch (allowed_voltage) {
		case USBIN_ADAPTER_ALLOW_12V:
		case USBIN_ADAPTER_ALLOW_9V_TO_12V:
			allowed_voltage = USBIN_ADAPTER_ALLOW_9V;
			break;
		case USBIN_ADAPTER_ALLOW_5V_OR_12V:
		case USBIN_ADAPTER_ALLOW_5V_OR_9V_TO_12V:
			allowed_voltage = USBIN_ADAPTER_ALLOW_5V_OR_9V;
			break;
		case USBIN_ADAPTER_ALLOW_5V_TO_12V:
			allowed_voltage = USBIN_ADAPTER_ALLOW_5V_TO_9V;
			break;
		}
	}

	rc = smblib_write(chg, USBIN_ADAPTER_ALLOW_CFG_REG, allowed_voltage);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write 0x%02x to USBIN_ADAPTER_ALLOW_CFG rc=%d\n",
			allowed_voltage, rc);
		return rc;
	}

	return rc;
}

#define MICRO_5V	5000000
#define MICRO_9V	9000000
#define MICRO_12V	12000000
static int smblib_set_usb_pd_allowed_voltage(struct smb_charger *chg,
					int min_allowed_uv, int max_allowed_uv)
{
	int rc = 0;
#ifdef QCOM_BASE
	u8 allowed_voltage;

	if (min_allowed_uv == MICRO_5V && max_allowed_uv == MICRO_5V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V;
		smblib_set_opt_freq_buck(chg, chg->chg_freq.freq_5V);
	} else if (min_allowed_uv == MICRO_9V && max_allowed_uv == MICRO_9V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_9V;
		smblib_set_opt_freq_buck(chg, chg->chg_freq.freq_9V);
	} else if (min_allowed_uv == MICRO_12V && max_allowed_uv == MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_12V;
		smblib_set_opt_freq_buck(chg, chg->chg_freq.freq_12V);
	} else if (min_allowed_uv < MICRO_9V && max_allowed_uv <= MICRO_9V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V_TO_9V;
	} else if (min_allowed_uv < MICRO_9V && max_allowed_uv <= MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V_TO_12V;
	} else if (min_allowed_uv < MICRO_12V && max_allowed_uv <= MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_9V_TO_12V;
	} else {
		smblib_err(chg, "invalid allowed voltage [%d, %d]\n",
			min_allowed_uv, max_allowed_uv);
		return -EINVAL;
	}

	rc = smblib_set_adapter_allowance(chg, allowed_voltage);
	if (rc < 0) {
		smblib_err(chg, "Couldn't configure adapter allowance rc=%d\n",
				rc);
		return rc;
	}
#else
	if (chg->pd_active && (chg->pd_contract_uv <= 0)) {
		cancel_delayed_work(&chg->pd_contract_work);
		schedule_delayed_work(&chg->pd_contract_work,
				      msecs_to_jiffies(0));
	}
#endif
	return rc;
}

/********************
 * HELPER FUNCTIONS *
 ********************/
static int smblib_request_dpdm(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	/* fetch the DPDM regulator */
	if (!chg->dpdm_reg && of_get_property(chg->dev->of_node,
				"dpdm-supply", NULL)) {
		chg->dpdm_reg = devm_regulator_get(chg->dev, "dpdm");
		if (IS_ERR(chg->dpdm_reg)) {
			rc = PTR_ERR(chg->dpdm_reg);
			smblib_err(chg, "Couldn't get dpdm regulator rc=%d\n",
					rc);
			chg->dpdm_reg = NULL;
			return rc;
		}
	}

	if (enable) {
		if (chg->dpdm_reg && !regulator_is_enabled(chg->dpdm_reg)) {
			smblib_dbg(chg, PR_MISC, "enabling DPDM regulator\n");
			rc = regulator_enable(chg->dpdm_reg);
			if (rc < 0)
				smblib_err(chg,
					"Couldn't enable dpdm regulator rc=%d\n",
					rc);
		}
	} else {
		if (chg->dpdm_reg && regulator_is_enabled(chg->dpdm_reg)) {
			smblib_dbg(chg, PR_MISC, "disabling DPDM regulator\n");
			rc = regulator_disable(chg->dpdm_reg);
			if (rc < 0)
				smblib_err(chg,
					"Couldn't disable dpdm regulator rc=%d\n",
					rc);
		}
	}

	return rc;
}

static void smblib_rerun_apsd(struct smb_charger *chg)
{
	int rc;

	smblib_dbg(chg, PR_MISC, "re-running APSD\n");
	if (chg->wa_flags & QC_AUTH_INTERRUPT_WA_BIT) {
		rc = smblib_masked_write(chg,
				USBIN_SOURCE_CHANGE_INTRPT_ENB_REG,
				AUTH_IRQ_EN_CFG_BIT, AUTH_IRQ_EN_CFG_BIT);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable HVDCP auth IRQ rc=%d\n",
									rc);
	}

	rc = smblib_masked_write(chg, CMD_APSD_REG,
				APSD_RERUN_BIT, APSD_RERUN_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't re-run APSD rc=%d\n", rc);
}

static const struct apsd_result *smblib_update_usb_type(struct smb_charger *chg)
{
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);

	if (!strcmp(apsd_result->name, "HVDCP3"))
	    chg->mmi.hvdcp3_con = true;

	/* if PD is active, APSD is disabled so won't have a valid result */
	if (chg->pd_active) {
		chg->real_charger_type = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		/*
		 * Update real charger type only if its not FLOAT
		 * detected as as SDP
		 */
		if (!(apsd_result->pst == POWER_SUPPLY_TYPE_USB_FLOAT &&
			chg->real_charger_type == POWER_SUPPLY_TYPE_USB))
		chg->real_charger_type = apsd_result->pst;
	}

	smblib_dbg(chg, PR_MISC, "APSD=%s PD=%d\n",
					apsd_result->name, chg->pd_active);
	return apsd_result;
}

static int smblib_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct smb_charger *chg = container_of(nb, struct smb_charger, nb);

	if (!strcmp(psy->desc->name, "bms")) {
		if (!chg->bms_psy)
			chg->bms_psy = psy;
		if (ev == PSY_EVENT_PROP_CHANGED)
			schedule_work(&chg->bms_update_work);
	}

	if (!chg->pl.psy && !strcmp(psy->desc->name, "parallel"))
		chg->pl.psy = psy;

	return NOTIFY_OK;
}

static int smblib_register_notifier(struct smb_charger *chg)
{
	int rc;

	chg->nb.notifier_call = smblib_notifier_call;
	rc = power_supply_reg_notifier(&chg->nb);
	if (rc < 0) {
		smblib_err(chg, "Couldn't register psy notifier rc = %d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_mapping_soc_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw)
{
	if (val_u > param->max_u || val_u < param->min_u)
		return -EINVAL;

	*val_raw = val_u << 1;

	return 0;
}

int smblib_mapping_cc_delta_to_field_value(struct smb_chg_param *param,
					   u8 val_raw)
{
	int val_u  = val_raw * param->step_u + param->min_u;

	if (val_u > param->max_u)
		val_u -= param->max_u * 2;

	return val_u;
}

int smblib_mapping_cc_delta_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw)
{
	if (val_u > param->max_u || val_u < param->min_u - param->max_u)
		return -EINVAL;

	val_u += param->max_u * 2 - param->min_u;
	val_u %= param->max_u * 2;
	*val_raw = val_u / param->step_u;

	return 0;
}

static void smblib_uusb_removal(struct smb_charger *chg)
{
	int rc;
	struct smb_irq_data *data;
	struct storm_watch *wdata;

	cancel_delayed_work_sync(&chg->pl_enable_work);

	rc = smblib_request_dpdm(chg, false);
	if (rc < 0)
		smblib_err(chg, "Couldn't to disable DPDM rc=%d\n", rc);

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCH_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	/* reset both usbin current and voltage votes */
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->usb_icl_votable, SW_QC3_VOTER, false, 0);
	vote(chg->usb_icl_votable, USBIN_USBIN_BOOST_VOTER, false, 0);
	vote(chg->hvdcp_hw_inov_dis_votable, OV_VOTER, false, 0);

	cancel_delayed_work_sync(&chg->hvdcp_detect_work);

	if (chg->wa_flags & QC_AUTH_INTERRUPT_WA_BIT) {
		/* re-enable AUTH_IRQ_EN_CFG_BIT */
		rc = smblib_masked_write(chg,
				USBIN_SOURCE_CHANGE_INTRPT_ENB_REG,
				AUTH_IRQ_EN_CFG_BIT, AUTH_IRQ_EN_CFG_BIT);
		if (rc < 0)
			smblib_err(chg,
				"Couldn't enable QC auth setting rc=%d\n", rc);
	}

	/* reconfigure allowed voltage for HVDCP */
	rc = smblib_set_adapter_allowance(chg,
			USBIN_ADAPTER_ALLOW_5V_TO_9V);
	if (rc < 0)
		smblib_err(chg, "Couldn't set USBIN_ADAPTER_ALLOW_5V_TO_9V rc=%d\n",
			rc);
#ifdef QCOM_BASE
	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_5V;
#endif
	chg->pd_contract_uv = 0;
	chg->usb_icl_delta_ua = 0;
	chg->pulse_cnt = 0;
	chg->uusb_apsd_rerun_done = false;

	/* clear USB ICL vote for USB_PSY_VOTER */
	rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't un-vote for USB ICL rc=%d\n", rc);

	/* clear USB ICL vote for DCP_VOTER */
	rc = vote(chg->usb_icl_votable, DCP_VOTER, false, 0);
	if (rc < 0)
		smblib_err(chg,
			"Couldn't un-vote DCP from USB ICL rc=%d\n", rc);
}

void smblib_suspend_on_debug_battery(struct smb_charger *chg)
{
	int rc;
	union power_supply_propval val;

	if (!chg->suspend_input_on_debug_batt)
		return;

	rc = power_supply_get_property(chg->bms_psy,
			POWER_SUPPLY_PROP_DEBUG_BATTERY, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get debug battery prop rc=%d\n", rc);
		return;
	}

	vote(chg->usb_icl_votable, DEBUG_BOARD_VOTER, val.intval, 0);
	vote(chg->dc_suspend_votable, DEBUG_BOARD_VOTER, val.intval, 0);
	if (val.intval)
		pr_info("Input suspended: Fake battery\n");
}

int smblib_rerun_apsd_if_required(struct smb_charger *chg)
{
	union power_supply_propval val;
	int rc;

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc = %d\n", rc);
		return rc;
	}

	if (!val.intval)
		return 0;

	rc = smblib_request_dpdm(chg, true);
	if (rc < 0)
		smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);

	chg->uusb_apsd_rerun_done = true;
	smblib_rerun_apsd(chg);

	return 0;
}

static int smblib_get_hw_pulse_cnt(struct smb_charger *chg, int *count)
{
	int rc;
	u8 val[2];

	switch (chg->smb_version) {
	case PMI8998_SUBTYPE:
		rc = smblib_read(chg, QC_PULSE_COUNT_STATUS_REG, val);
		if (rc) {
			pr_err("failed to read QC_PULSE_COUNT_STATUS_REG rc=%d\n",
					rc);
			return rc;
		}
		*count = val[0] & QC_PULSE_COUNT_MASK;
		break;
	case PM660_SUBTYPE:
		rc = smblib_multibyte_read(chg,
				QC_PULSE_COUNT_STATUS_1_REG, val, 2);
		if (rc) {
			pr_err("failed to read QC_PULSE_COUNT_STATUS_1_REG rc=%d\n",
					rc);
			return rc;
		}
		*count = (val[1] << 8) | val[0];
		break;
	default:
		smblib_dbg(chg, PR_PARALLEL, "unknown SMB chip %d\n",
				chg->smb_version);
		return -EINVAL;
	}

	return 0;
}

static int smblib_get_pulse_cnt(struct smb_charger *chg, int *count)
{
	int rc;

	/* Use software based pulse count if HW INOV is disabled */
	if (get_effective_result(chg->hvdcp_hw_inov_dis_votable) > 0) {
		*count = chg->pulse_cnt;
		return 0;
	}

	/* Use h/w pulse count if autonomous mode is enabled */
	rc = smblib_get_hw_pulse_cnt(chg, count);
	if (rc < 0)
		smblib_err(chg, "failed to read h/w pulse count rc=%d\n", rc);

	return rc;
}

#define USBIN_25MA	25000
#define USBIN_100MA	100000
#define USBIN_150MA	150000
#define USBIN_500MA	500000
#define USBIN_900MA	900000

static int set_sdp_current(struct smb_charger *chg, int icl_ua)
{
	int rc;
	u8 icl_options;
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);

	/* power source is SDP */
	switch (icl_ua) {
	case USBIN_100MA:
		/* USB 2.0 100mA */
		icl_options = 0;
		break;
	case USBIN_150MA:
		/* USB 3.0 150mA */
		icl_options = CFG_USB3P0_SEL_BIT;
		break;
	case USBIN_500MA:
		/* USB 2.0 500mA */
		icl_options = USB51_MODE_BIT;
		break;
	case USBIN_900MA:
		/* USB 3.0 900mA */
		icl_options = CFG_USB3P0_SEL_BIT | USB51_MODE_BIT;
		break;
	default:
		smblib_err(chg, "ICL %duA isn't supported for SDP\n", icl_ua);
		return -EINVAL;
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB &&
		apsd_result->pst == POWER_SUPPLY_TYPE_USB_FLOAT) {
		/*
		 * change the float charger configuration to SDP, if this
		 * is the case of SDP being detected as FLOAT
		 */
		rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
			FORCE_FLOAT_SDP_CFG_BIT, FORCE_FLOAT_SDP_CFG_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set float ICL options rc=%d\n",
						rc);
			return rc;
		}
	}

	rc = smblib_masked_write(chg, USBIN_ICL_OPTIONS_REG,
		CFG_USB3P0_SEL_BIT | USB51_MODE_BIT, icl_options);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ICL options rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int get_sdp_current(struct smb_charger *chg, int *icl_ua)
{
	int rc;
	u8 icl_options;
	bool usb3 = false;

	rc = smblib_read(chg, USBIN_ICL_OPTIONS_REG, &icl_options);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get ICL options rc=%d\n", rc);
		return rc;
	}

	usb3 = (icl_options & CFG_USB3P0_SEL_BIT);

	if (icl_options & USB51_MODE_BIT)
		*icl_ua = usb3 ? USBIN_900MA : USBIN_500MA;
	else
		*icl_ua = usb3 ? USBIN_150MA : USBIN_100MA;

	return rc;
}

int smblib_set_icl_current(struct smb_charger *chg, int icl_ua)
{
	int rc = 0;
	bool override;
	int reg;
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);

	if (chg->mmi.factory_mode) {
		pr_err("USB ICL callback in Facory Mode! %d\n", icl_ua);
		return rc;
	}

	smblib_dbg(chg, PR_MISC, "%d uA\n", icl_ua);
	/* suspend and return if 25mA or less is requested */
	if (icl_ua < USBIN_25MA)
		return smblib_set_usb_suspend(chg, true);

	if (icl_ua == INT_MAX)
		goto override_suspend_config;

	if ((chg->hc_aicl_threshold_mv >= AICL_THRESHOLD_MIN_MV &&
	    chg->hc_aicl_threshold_mv <= AICL_THRESHOLD_MAX_MV) &&
	    (icl_ua > 1500000 || !strncmp(apsd_result->name, "HVDCP", 4))) {
		reg = (chg->hc_aicl_threshold_mv - AICL_THRESHOLD_MIN_MV) / 100;
		rc = smblib_masked_write(chg, USBIN_CONT_AICL_THRESHOLD_CFG_REG,
			USBIN_CONT_AICL_THRESHOLD_CFG_MASK, reg);
		if (rc < 0)
			smblib_err(chg, "Config CON AICL fails rc=%d\n", rc);
	} else if (chg->aicl_threshold_mv >= AICL_THRESHOLD_MIN_MV &&
	    chg->aicl_threshold_mv <= AICL_THRESHOLD_MAX_MV) {
		reg = (chg->aicl_threshold_mv - AICL_THRESHOLD_MIN_MV) / 100;
		rc = smblib_masked_write(chg, USBIN_CONT_AICL_THRESHOLD_CFG_REG,
			USBIN_CONT_AICL_THRESHOLD_CFG_MASK, reg);
		if (rc < 0)
			smblib_err(chg, "Config CON AICL fails rc=%d\n", rc);
	}

	/* configure current */
	if (chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
		&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB)) {
		rc = set_sdp_current(chg, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set SDP ICL rc=%d\n", rc);
			goto enable_icl_changed_interrupt;
		}
	} else {
		set_sdp_current(chg, 100000);
		rc = smblib_set_charge_param(chg, &chg->param.usb_icl, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set HC ICL rc=%d\n", rc);
			goto enable_icl_changed_interrupt;
		}
	}

override_suspend_config:
	/* determine if override needs to be enforced */
	override = true;
	if (icl_ua == INT_MAX) {
		/* remove override if no voters - hw defaults is desired */
		override = false;
	} else if (chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) {
		if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB)
			/* For std cable with type = SDP never override */
			override = false;
		else if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_CDP
			&& icl_ua == 1500000)
			/*
			 * For std cable with type = CDP override only if
			 * current is not 1500mA
			 */
			override = false;
	}

	/* enforce override */
	rc = smblib_masked_write(chg, USBIN_ICL_OPTIONS_REG,
		USBIN_MODE_CHG_BIT, override ? USBIN_MODE_CHG_BIT : 0);

	rc = smblib_icl_override(chg, override);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ICL override rc=%d\n", rc);
		goto enable_icl_changed_interrupt;
	}

	/* unsuspend after configuring current and override */
	rc = smblib_set_usb_suspend(chg, false);
	if (rc < 0) {
		smblib_err(chg, "Couldn't resume input rc=%d\n", rc);
		goto enable_icl_changed_interrupt;
	}

enable_icl_changed_interrupt:
	return rc;
}

int smblib_get_icl_current(struct smb_charger *chg, int *icl_ua)
{
	int rc = 0;
	u8 load_cfg;
	bool override;

	if ((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
		|| chg->micro_usb_mode)
		&& (chg->real_charger_type == POWER_SUPPLY_TYPE_USB)) {
		rc = get_sdp_current(chg, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get SDP ICL rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = smblib_read(chg, USBIN_LOAD_CFG_REG, &load_cfg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get load cfg rc=%d\n", rc);
			return rc;
		}
		override = load_cfg & ICL_OVERRIDE_AFTER_APSD_BIT;
		if (!override)
			return INT_MAX;

		/* override is set */
		rc = smblib_get_charge_param(chg, &chg->param.usb_icl, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get HC ICL rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

/*********************
 * VOTABLE CALLBACKS *
 *********************/

static int smblib_dc_suspend_vote_callback(struct votable *votable, void *data,
			int suspend, const char *client)
{
	struct smb_charger *chg = data;

	if (chg->mmi.factory_mode)
		return 0;

	/* resume input if suspend is invalid */
	if (suspend < 0)
		suspend = 0;

	return smblib_set_dc_suspend(chg, (bool)suspend);
}

static int smblib_dc_icl_vote_callback(struct votable *votable, void *data,
			int icl_ua, const char *client)
{
	struct smb_charger *chg = data;
	int rc = 0;
	bool suspend;

	if (chg->mmi.factory_mode)
		return 0;

	if (icl_ua < 0) {
		smblib_dbg(chg, PR_MISC, "No Voter hence suspending\n");
		icl_ua = 0;
	}

	suspend = (icl_ua < USBIN_25MA);
	if (suspend)
		goto suspend;

	rc = smblib_set_charge_param(chg, &chg->param.dc_icl, icl_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set DC input current limit rc=%d\n",
			rc);
		return rc;
	}

suspend:
	rc = vote(chg->dc_suspend_votable, USER_VOTER, suspend, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't vote to %s DC rc=%d\n",
			suspend ? "suspend" : "resume", rc);
		return rc;
	}
	return rc;
}

static int smblib_pd_disallowed_votable_indirect_callback(
	struct votable *votable, void *data, int disallowed, const char *client)
{
	struct smb_charger *chg = data;
	int rc;

	rc = vote(chg->pd_allowed_votable, PD_DISALLOWED_INDIRECT_VOTER,
		!disallowed, 0);

	return rc;
}

static int smblib_awake_vote_callback(struct votable *votable, void *data,
			int awake, const char *client)
{
	struct smb_charger *chg = data;

	if (awake)
		pm_stay_awake(chg->dev);
	else
		pm_relax(chg->dev);

	return 0;
}

static int smblib_chg_disable_vote_callback(struct votable *votable, void *data,
			int chg_disable, const char *client)
{
	struct smb_charger *chg = data;
	int rc;

	if (chg->mmi.factory_mode)
		return 0;

	rc = smblib_masked_write(chg, CHARGING_ENABLE_CMD_REG,
				 CHARGING_ENABLE_CMD_BIT,
				 chg_disable ? 0 : CHARGING_ENABLE_CMD_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't %s charging rc=%d\n",
			chg_disable ? "disable" : "enable", rc);
		return rc;
	}
	smblib_rerun_aicl(chg);

	return 0;
}

static int smblib_hvdcp_enable_vote_callback(struct votable *votable,
			void *data,
			int hvdcp_enable, const char *client)
{
	struct smb_charger *chg = data;
	int rc;
	u8 val = HVDCP_AUTH_ALG_EN_CFG_BIT | HVDCP_EN_BIT;
	u8 stat;

#ifdef QCOM_BASE
	/* vote to enable/disable HW autonomous INOV */
	vote(chg->hvdcp_hw_inov_dis_votable, client, !hvdcp_enable, 0);
#endif
	/*
	 * Disable the autonomous bit and auth bit for disabling hvdcp.
	 * This ensures only qc 2.0 detection runs but no vbus
	 * negotiation happens.
	 */
#ifdef QCOM_BASE
	if (!hvdcp_enable)
		val = HVDCP_EN_BIT;
#endif
	rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
#ifdef QCOM_BASE
				 HVDCP_EN_BIT | HVDCP_AUTH_ALG_EN_CFG_BIT,
#else
				 HVDCP_EN_BIT | HVDCP_AUTH_ALG_EN_CFG_BIT |
				 HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT,
#endif
				 val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't %s hvdcp rc=%d\n",
			hvdcp_enable ? "enable" : "disable", rc);
		return rc;
	}

	rc = smblib_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD status rc=%d\n", rc);
		return rc;
	}

	/* re-run APSD if HVDCP was detected */
	if (stat & QC_CHARGER_BIT)
		smblib_rerun_apsd(chg);

	return 0;
}

static int smblib_hvdcp_disable_indirect_vote_callback(struct votable *votable,
			void *data, int hvdcp_disable, const char *client)
{
	struct smb_charger *chg = data;

	vote(chg->hvdcp_enable_votable, HVDCP_INDIRECT_VOTER,
			!hvdcp_disable, 0);

	return 0;
}

static int smblib_apsd_disable_vote_callback(struct votable *votable,
			void *data,
			int apsd_disable, const char *client)
{
	struct smb_charger *chg = data;
	int rc;

	if (apsd_disable) {
		rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
							AUTO_SRC_DETECT_BIT,
							0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable APSD rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
							AUTO_SRC_DETECT_BIT,
							AUTO_SRC_DETECT_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable APSD rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

static int smblib_hvdcp_hw_inov_dis_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;
	int rc;

	if (disable) {
		/*
		 * the pulse count register get zeroed when autonomous mode is
		 * disabled. Track that in variables before disabling
		 */
		rc = smblib_get_hw_pulse_cnt(chg, &chg->pulse_cnt);
		if (rc < 0) {
			pr_err("failed to read QC_PULSE_COUNT_STATUS_REG rc=%d\n",
					rc);
			return rc;
		}
	}
#ifdef QCOM_BASE
	rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG,
			HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT,
			disable ? 0 : HVDCP_AUTONOMOUS_MODE_EN_CFG_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't %s hvdcp rc=%d\n",
				disable ? "disable" : "enable", rc);
		return rc;
	}

	return rc;
#else
	return 0;
#endif
}

static int smblib_usb_irq_enable_vote_callback(struct votable *votable,
				void *data, int enable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[INPUT_CURRENT_LIMIT_IRQ].irq ||
				!chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(chg->irq_info[INPUT_CURRENT_LIMIT_IRQ].irq);
		enable_irq(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
	} else {
		disable_irq(chg->irq_info[INPUT_CURRENT_LIMIT_IRQ].irq);
		disable_irq(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
	}

	return 0;
}

static int smblib_typec_irq_disable_vote_callback(struct votable *votable,
				void *data, int disable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[TYPE_C_CHANGE_IRQ].irq)
		return 0;

	if (disable)
		disable_irq_nosync(chg->irq_info[TYPE_C_CHANGE_IRQ].irq);
	else
		enable_irq(chg->irq_info[TYPE_C_CHANGE_IRQ].irq);

	return 0;
}

/*******************
 * VCONN REGULATOR *
 * *****************/

#define MAX_OTG_SS_TRIES 2
static int _smblib_vconn_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 val;

	/*
	 * When enabling VCONN using the command register the CC pin must be
	 * selected. VCONN should be supplied to the inactive CC pin hence using
	 * the opposite of the CC_ORIENTATION_BIT.
	 */
	smblib_dbg(chg, PR_OTG, "enabling VCONN\n");
	val = chg->typec_status[3] &
			CC_ORIENTATION_BIT ? 0 : VCONN_EN_ORIENTATION_BIT;
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 VCONN_EN_VALUE_BIT | VCONN_EN_ORIENTATION_BIT,
				 VCONN_EN_VALUE_BIT | val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable vconn setting rc=%d\n", rc);
		return rc;
	}

	return rc;
}

int smblib_vconn_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&chg->vconn_oc_lock);
	if (chg->vconn_en)
		goto unlock;

	rc = _smblib_vconn_regulator_enable(rdev);
	if (rc >= 0)
		chg->vconn_en = true;

unlock:
	mutex_unlock(&chg->vconn_oc_lock);
	return rc;
}

static int _smblib_vconn_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	smblib_dbg(chg, PR_OTG, "disabling VCONN\n");
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 VCONN_EN_VALUE_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable vconn regulator rc=%d\n", rc);

	return rc;
}

int smblib_vconn_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&chg->vconn_oc_lock);
	if (!chg->vconn_en)
		goto unlock;

	rc = _smblib_vconn_regulator_disable(rdev);
	if (rc >= 0)
		chg->vconn_en = false;

unlock:
	mutex_unlock(&chg->vconn_oc_lock);
	return rc;
}

int smblib_vconn_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int ret;

	mutex_lock(&chg->vconn_oc_lock);
	ret = chg->vconn_en;
	mutex_unlock(&chg->vconn_oc_lock);
	return ret;
}

/*******************
 * MICNRS REGULATOR *
 * *****************/

int smblib_micnrs_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&chg->micnrs_oc_lock);
	if (chg->micnrs_en)
		goto unlock;

	/* BOB Mode set to PWM to prevent Audible Range */
	if (chg->bob_reg)
		rc = regulator_set_mode(chg->bob_reg,
					REGULATOR_MODE_FAST);
	if (rc)
		dev_err(chg->dev,
			"Failed to set Bob Mode: %d\n", rc);
	else
		chg->micnrs_en = true;

unlock:
	mutex_unlock(&chg->micnrs_oc_lock);
	return rc;
}

int smblib_micnrs_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&chg->micnrs_oc_lock);
	if (!chg->micnrs_en)
		goto unlock;
	/* BOB Mode set to AUTO to save current drain */
	if (chg->bob_reg)
		rc = regulator_set_mode(chg->bob_reg,
					REGULATOR_MODE_NORMAL);
	if (rc)
		dev_err(chg->dev,
				"Failed to set Bob Mode: %d\n", rc);
	else
		chg->micnrs_en = false;
unlock:
	mutex_unlock(&chg->micnrs_oc_lock);
	return rc;
}

int smblib_micnrs_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int ret;

	mutex_lock(&chg->micnrs_oc_lock);
	ret = chg->micnrs_en;
	mutex_unlock(&chg->micnrs_oc_lock);
	return ret;
}

/*****************
 * OTG REGULATOR *
 *****************/
#define MAX_RETRY		15
#define MIN_DELAY_US		2000
#define MAX_DELAY_US		9000
static int otg_current[] = {250000, 500000, 1000000, 1500000};
static int smblib_enable_otg_wa(struct smb_charger *chg)
{
	u8 stat;
	int rc, i, retry_count = 0, min_delay = MIN_DELAY_US;

	for (i = 0; i < ARRAY_SIZE(otg_current); i++) {
		smblib_dbg(chg, PR_OTG, "enabling OTG with %duA\n",
						otg_current[i]);
		rc = smblib_set_charge_param(chg, &chg->param.otg_cl,
						otg_current[i]);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set otg limit rc=%d\n", rc);
			return rc;
		}

		rc = smblib_write(chg, CMD_OTG_REG, OTG_EN_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable OTG rc=%d\n", rc);
			return rc;
		}

		retry_count = 0;
		min_delay = MIN_DELAY_US;
		do {
			usleep_range(min_delay, min_delay + 100);
			rc = smblib_read(chg, OTG_STATUS_REG, &stat);
			if (rc < 0) {
				smblib_err(chg, "Couldn't read OTG status rc=%d\n",
							rc);
				goto out;
			}

			if (stat & BOOST_SOFTSTART_DONE_BIT) {
				rc = smblib_set_charge_param(chg,
					&chg->param.otg_cl, chg->otg_cl_ua);
				if (rc < 0) {
					smblib_err(chg, "Couldn't set otg limit rc=%d\n",
							rc);
					goto out;
				}
				break;
			}
			/* increase the delay for following iterations */
			if (retry_count > 5)
				min_delay = MAX_DELAY_US;

		} while (retry_count++ < MAX_RETRY);

		if (retry_count >= MAX_RETRY) {
			smblib_dbg(chg, PR_OTG, "OTG enable failed with %duA\n",
								otg_current[i]);
			rc = smblib_write(chg, CMD_OTG_REG, 0);
			if (rc < 0) {
				smblib_err(chg, "disable OTG rc=%d\n", rc);
				goto out;
			}
		} else {
			smblib_dbg(chg, PR_OTG, "OTG enabled\n");
			return 0;
		}
	}

	if (i == ARRAY_SIZE(otg_current)) {
		rc = -EINVAL;
		goto out;
	}

	return 0;
out:
	smblib_write(chg, CMD_OTG_REG, 0);
	return rc;
}

static int _smblib_vbus_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	smblib_dbg(chg, PR_OTG, "halt 1 in 8 mode\n");
	rc = smblib_masked_write(chg, OTG_ENG_OTG_CFG_REG,
				 ENG_BUCKBOOST_HALT1_8_MODE_BIT,
				 ENG_BUCKBOOST_HALT1_8_MODE_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set OTG_ENG_OTG_CFG_REG rc=%d\n",
			rc);
		return rc;
	}

	smblib_dbg(chg, PR_OTG, "enabling OTG\n");

	if (chg->wa_flags & OTG_WA) {
		rc = smblib_enable_otg_wa(chg);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable OTG rc=%d\n", rc);
	} else {
		rc = smblib_write(chg, CMD_OTG_REG, OTG_EN_BIT);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable OTG rc=%d\n", rc);
	}

	return rc;
}

int smblib_vbus_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&chg->otg_oc_lock);
	if (chg->otg_en)
		goto unlock;

	if (!chg->usb_icl_votable) {
		chg->usb_icl_votable = find_votable("USB_ICL");

		if (!chg->usb_icl_votable)
			return -EINVAL;
	}
	vote(chg->usb_icl_votable, USBIN_USBIN_BOOST_VOTER, true, 0);

	rc = _smblib_vbus_regulator_enable(rdev);
	if (rc >= 0)
		chg->otg_en = true;
	else
		vote(chg->usb_icl_votable, USBIN_USBIN_BOOST_VOTER, false, 0);

unlock:
	mutex_unlock(&chg->otg_oc_lock);
	return rc;
}

static int _smblib_vbus_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	if (chg->wa_flags & OTG_WA) {
		/* set OTG current limit to minimum value */
		rc = smblib_set_charge_param(chg, &chg->param.otg_cl,
						chg->param.otg_cl.min_u);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't set otg current limit rc=%d\n", rc);
			return rc;
		}
	}

	smblib_dbg(chg, PR_OTG, "disabling OTG\n");
	rc = smblib_write(chg, CMD_OTG_REG, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable OTG regulator rc=%d\n", rc);
		return rc;
	}

	smblib_dbg(chg, PR_OTG, "start 1 in 8 mode\n");
	rc = smblib_masked_write(chg, OTG_ENG_OTG_CFG_REG,
				 ENG_BUCKBOOST_HALT1_8_MODE_BIT, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set OTG_ENG_OTG_CFG_REG rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_vbus_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	mutex_lock(&chg->otg_oc_lock);
	if (!chg->otg_en)
		goto unlock;

	rc = _smblib_vbus_regulator_disable(rdev);
	if (rc >= 0)
		chg->otg_en = false;

	if (chg->usb_icl_votable)
		vote(chg->usb_icl_votable, USBIN_USBIN_BOOST_VOTER, false, 0);
unlock:
	mutex_unlock(&chg->otg_oc_lock);
	return rc;
}

int smblib_vbus_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int ret;

	mutex_lock(&chg->otg_oc_lock);
	ret = chg->otg_en;
	mutex_unlock(&chg->otg_oc_lock);
	return ret;
}

/********************
 * EXTERNAL OTG REG *
 ********************/
int smblib_ext_vbus_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	if (chg->otg_en)
		return rc;
	smblib_dbg(chg, PR_OTG, "Enable Ext VBUS\n");
	rc = vote(chg->usb_icl_votable, OTG_VOTER, true, 0);
	if (rc < 0) {
		smblib_err(chg, "Failed to vote for USB Suspend - %d\n", rc);
		return rc;
	}

	chg->otg_en = true;
	return rc;
}

int smblib_ext_vbus_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	if (!chg->otg_en)
		return rc;
	smblib_dbg(chg, PR_OTG, "Disable Ext VBUS\n");
	rc = vote(chg->usb_icl_votable, OTG_VOTER, false, 0);
	if (rc < 0) {
		smblib_err(chg, "Failed to vote for USB Suspend - %d\n", rc);
		return rc;
	}

	chg->otg_en = false;
	return rc;
}

int smblib_ext_vbus_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = chg->otg_en;
	return rc;
}

/********************
 * BATT PSY GETTERS *
 ********************/

int smblib_get_prop_input_suspend(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	val->intval
		= (get_client_vote(chg->usb_icl_votable, USER_VOTER) == 0)
		 && get_client_vote(chg->dc_suspend_votable, USER_VOTER);
	return 0;
}

int smblib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, BATIF_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATIF_INT_RT_STS rc=%d\n", rc);
		return rc;
	}

	val->intval = !(stat & (BAT_THERM_OR_ID_MISSING_RT_STS_BIT
					| BAT_TERMINAL_MISSING_RT_STS_BIT));

	return rc;
}

int smblib_get_prop_batt_capacity(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	int rc = -EINVAL;

	if (chg->fake_capacity >= 0) {
		val->intval = chg->fake_capacity;
		return 0;
	}

	if (chg->bms_psy)
		rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_CAPACITY, val);
	return rc;
}

int smblib_get_prop_batt_age(struct smb_charger *chg,
			     union power_supply_propval *val)
{
	int age;
	int rc = -EINVAL;
	union power_supply_propval pval = {0, };

	if (chg->bms_psy) {
		rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
		if (rc < 0) {
			smblib_err(chg, "Fail: get charge full rc=%d\n",
				   rc);
			return rc;
		}

		age = pval.intval * 100;

		rc = power_supply_get_property(chg->bms_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
		if (rc < 0) {
			smblib_err(chg, "Fail: get chrg full design rc=%d\n",
				   rc);
			return rc;
		}

		val->intval = age / pval.intval;
	}
	return rc;
}

int smblib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val)
{
	union power_supply_propval pval = {0, };
	bool usb_online, dc_online, qnovo_en;
	u8 stat, pt_en_cmd;
	int rc;

	rc = smblib_get_prop_usb_online(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb online property rc=%d\n",
			rc);
		return rc;
	}
	usb_online = (bool)pval.intval;

	rc = smblib_get_prop_dc_online(chg, &pval);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get dc online property rc=%d\n",
			rc);
		return rc;
	}
	dc_online = (bool)pval.intval;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}
	stat = stat & BATTERY_CHARGER_STATUS_MASK;

	if (!usb_online && !dc_online) {
		switch (stat) {
		case TERMINATE_CHARGE:
		case INHIBIT_CHARGE:
#ifdef QCOM_BASE
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
#endif
		default:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		return rc;
	}

	switch (stat) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
	case FAST_CHARGE:
	case FULLON_CHARGE:
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case TERMINATE_CHARGE:
	case INHIBIT_CHARGE:
#ifdef QCOM_BASE
		val->intval = POWER_SUPPLY_STATUS_FULL;
		break;
#endif
	case DISABLE_CHARGE:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	default:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	if (chg->mmi.pres_chrg_step == STEP_FULL)
		val->intval = POWER_SUPPLY_STATUS_FULL;

	if (val->intval != POWER_SUPPLY_STATUS_CHARGING)
		return 0;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_7_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
				rc);
			return rc;
	}

	stat &= ENABLE_TRICKLE_BIT | ENABLE_PRE_CHARGING_BIT |
		 ENABLE_FAST_CHARGING_BIT | ENABLE_FULLON_MODE_BIT;

	rc = smblib_read(chg, QNOVO_PT_ENABLE_CMD_REG, &pt_en_cmd);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read QNOVO_PT_ENABLE_CMD_REG rc=%d\n",
				rc);
		return rc;
	}

	qnovo_en = (bool)(pt_en_cmd & QNOVO_PT_ENABLE_CMD_BIT);

	/* ignore stat7 when qnovo is enabled */
	if (!qnovo_en && !stat)
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return 0;
}

bool smblib_charge_halted(struct smb_charger *chg)
{
	u8 stat;
	int rc;
	bool flag = false;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return flag;
	}
	stat = stat & BATTERY_CHARGER_STATUS_MASK;

	switch (stat) {
	case TERMINATE_CHARGE:
	case INHIBIT_CHARGE:
		flag = true;
		break;
	default:
		break;
	}

	return flag;
}

int smblib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}

	switch (stat & BATTERY_CHARGER_STATUS_MASK) {
	case TRICKLE_CHARGE:
	case PRE_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case FAST_CHARGE:
	case FULLON_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case TAPER_CHARGE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_TAPER;
		break;
	default:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return rc;
}

int smblib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val)
{
	union power_supply_propval pval;
	int rc;
	int effective_fv_uv;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_2_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "BATTERY_CHARGER_STATUS_2 = 0x%02x\n",
		   stat);

	if (stat & CHARGER_ERROR_STATUS_BAT_OV_BIT) {
		rc = smblib_get_prop_from_bms(chg,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (!rc) {
			/*
			 * If Vbatt is within 40mV above Vfloat, then don't
			 * treat it as overvoltage.
			 */
			effective_fv_uv = get_effective_result(chg->fv_votable);
			if (pval.intval >= effective_fv_uv + 40000) {
				val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				smblib_err(chg, "battery over-voltage vbat_fg = %duV, fv = %duV\n",
						pval.intval, effective_fv_uv);
				goto done;
			}
		}
	}

	if (stat & BAT_TEMP_STATUS_TOO_COLD_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COLD;
	else if (stat & BAT_TEMP_STATUS_TOO_HOT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (stat & BAT_TEMP_STATUS_COLD_SOFT_LIMIT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else if (stat & BAT_TEMP_STATUS_HOT_SOFT_LIMIT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_WARM;
	else
		val->intval = POWER_SUPPLY_HEALTH_GOOD;

	val->intval = chg->mmi.batt_health;
done:
	return rc;
}

int smblib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->system_temp_level;
	return 0;
}

int smblib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val)
{
	u8 stat;
	int rc;

	if (chg->fake_input_current_limited >= 0) {
		val->intval = chg->fake_input_current_limited;
		return 0;
	}

	rc = smblib_read(chg, AICL_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read AICL_STATUS rc=%d\n", rc);
		return rc;
	}
	val->intval = (stat & SOFT_ILIMIT_BIT) || chg->is_hdc;
	return 0;
}

int smblib_get_prop_batt_charge_done(struct smb_charger *chg,
					union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
			rc);
		return rc;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	val->intval = (stat == TERMINATE_CHARGE);

	if ((chg->mmi.pres_chrg_step == STEP_FULL) ||
	    (chg->mmi.pres_chrg_step == STEP_EB))
		val->intval = 1;

	return 0;
}

int smblib_get_prop_charge_qnovo_enable(struct smb_charger *chg,
				  union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, QNOVO_PT_ENABLE_CMD_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read QNOVO_PT_ENABLE_CMD rc=%d\n",
			rc);
		return rc;
	}

	val->intval = (bool)(stat & QNOVO_PT_ENABLE_CMD_BIT);
	return 0;
}

int smblib_get_prop_from_bms(struct smb_charger *chg,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int rc;

	if (!chg->bms_psy)
		return -EINVAL;

	rc = power_supply_get_property(chg->bms_psy, psp, val);

	return rc;
}

/***********************
 * BATTERY PSY SETTERS *
 ***********************/

int smblib_set_prop_input_suspend(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	int rc;

	/* vote 0mA when suspended */
	rc = vote(chg->usb_icl_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't vote to %s USB rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	rc = vote(chg->dc_suspend_votable, USER_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't vote to %s DC rc=%d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	power_supply_changed(chg->batt_psy);
	return rc;
}

int smblib_set_prop_batt_capacity(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	chg->fake_capacity = val->intval;

	power_supply_changed(chg->batt_psy);

	return 0;
}

int smblib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	if (val->intval < 0)
		return -EINVAL;

	if (chg->thermal_levels <= 0)
		return -EINVAL;

	if (val->intval >= chg->thermal_levels)
		chg->system_temp_level = chg->thermal_levels - 1;
	else
		chg->system_temp_level = val->intval;
	/* disable parallel charge in case of system temp level */
	vote(chg->pl_disable_votable, THERMAL_DAEMON_VOTER,
			chg->system_temp_level ? true : false, 0);

	vote(chg->chg_disable_votable, THERMAL_DAEMON_VOTER, false, 0);
	if (chg->system_temp_level == 0)
		return vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);

	vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			chg->thermal_mitigation[chg->system_temp_level] * 1000);
	return 0;
}

int smblib_set_prop_charge_qnovo_enable(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	int rc = 0;

	rc = smblib_masked_write(chg, QNOVO_PT_ENABLE_CMD_REG,
			QNOVO_PT_ENABLE_CMD_BIT,
			val->intval ? QNOVO_PT_ENABLE_CMD_BIT : 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't enable qnovo rc=%d\n", rc);
		return rc;
	}

	return rc;
}

int smblib_set_prop_input_current_limited(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	chg->fake_input_current_limited = val->intval;
	return 0;
}

int smblib_rerun_aicl(struct smb_charger *chg)
{
	int rc, settled_icl_ua;
	u8 stat;

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
								rc);
		return rc;
	}

	/* USB is suspended so skip re-running AICL */
	if (stat & USBIN_SUSPEND_STS_BIT)
		return rc;

	smblib_dbg(chg, PR_MISC, "re-running AICL\n");
	rc = smblib_get_charge_param(chg, &chg->param.icl_stat,
			&settled_icl_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get settled ICL rc=%d\n", rc);
		return rc;
	}

	vote(chg->usb_icl_votable, AICL_RERUN_VOTER, true,
			max(settled_icl_ua - chg->param.usb_icl.step_u,
				chg->param.usb_icl.step_u));
	vote(chg->usb_icl_votable, AICL_RERUN_VOTER, false, 0);

	return 0;
}

static int smblib_dp_pulse(struct smb_charger *chg)
{
	int rc;

	/* QC 3.0 increment */
	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, SINGLE_INCREMENT_BIT,
			SINGLE_INCREMENT_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}
#ifdef QCOM_BASE
static int smblib_dm_pulse(struct smb_charger *chg)
{
	int rc;

	/* QC 3.0 decrement */
	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, SINGLE_DECREMENT_BIT,
			SINGLE_DECREMENT_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}

static int smblib_force_vbus_voltage(struct smb_charger *chg, u8 val)
{
	int rc;

	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, val, val);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}
#endif

int smblib_dp_dm(struct smb_charger *chg, int val)
{
#ifdef QCOM_BASE
	int target_icl_ua, rc = 0;
	union power_supply_propval pval;

	switch (val) {
	case POWER_SUPPLY_DP_DM_DP_PULSE:
		rc = smblib_dp_pulse(chg);
		if (!rc)
			chg->pulse_cnt++;
		smblib_dbg(chg, PR_PARALLEL, "DP_DM_DP_PULSE rc=%d cnt=%d\n",
				rc, chg->pulse_cnt);
		break;
	case POWER_SUPPLY_DP_DM_DM_PULSE:
		rc = smblib_dm_pulse(chg);
		if (!rc && chg->pulse_cnt)
			chg->pulse_cnt--;
		smblib_dbg(chg, PR_PARALLEL, "DP_DM_DM_PULSE rc=%d cnt=%d\n",
				rc, chg->pulse_cnt);
		break;
	case POWER_SUPPLY_DP_DM_ICL_DOWN:
		target_icl_ua = get_effective_result(chg->usb_icl_votable);
		if (target_icl_ua < 0) {
			/* no client vote, get the ICL from charger */
			rc = power_supply_get_property(chg->usb_psy,
					POWER_SUPPLY_PROP_HW_CURRENT_MAX,
					&pval);
			if (rc < 0) {
				smblib_err(chg,
					"Couldn't get max current rc=%d\n",
					rc);
				return rc;
			}
			target_icl_ua = pval.intval;
		}

		/*
		 * Check if any other voter voted on USB_ICL in case of
		 * voter other than SW_QC3_VOTER reset and restart reduction
		 * again.
		 */
		if (target_icl_ua != get_client_vote(chg->usb_icl_votable,
							SW_QC3_VOTER))
			chg->usb_icl_delta_ua = 0;

		chg->usb_icl_delta_ua += 100000;
		vote(chg->usb_icl_votable, SW_QC3_VOTER, true,
						target_icl_ua - 100000);
		smblib_dbg(chg, PR_PARALLEL, "ICL DOWN ICL=%d reduction=%d\n",
				target_icl_ua, chg->usb_icl_delta_ua);
		break;
	case POWER_SUPPLY_DP_DM_FORCE_5V:
		rc = smblib_force_vbus_voltage(chg, FORCE_5V_BIT);
		if (rc < 0)
			pr_err("Failed to force 5V\n");
		break;
	case POWER_SUPPLY_DP_DM_FORCE_9V:
		rc = smblib_force_vbus_voltage(chg, FORCE_9V_BIT);
		if (rc < 0)
			pr_err("Failed to force 9V\n");
		break;
	case POWER_SUPPLY_DP_DM_FORCE_12V:
		rc = smblib_force_vbus_voltage(chg, FORCE_12V_BIT);
		if (rc < 0)
			pr_err("Failed to force 12V\n");
		break;
	case POWER_SUPPLY_DP_DM_ICL_UP:
	default:
		break;
	}

	return rc;
#else
	return 0;
#endif
}

int smblib_disable_hw_jeita(struct smb_charger *chg, bool disable)
{
	int rc;
	u8 mask;

	/*
	 * Disable h/w base JEITA compensation if s/w JEITA is enabled
	 */
	mask = JEITA_EN_COLD_SL_FCV_BIT
		| JEITA_EN_HOT_SL_FCV_BIT
		| JEITA_EN_HOT_SL_CCC_BIT
		| JEITA_EN_COLD_SL_CCC_BIT,
	rc = smblib_masked_write(chg, JEITA_EN_CFG_REG, mask,
			disable ? 0 : mask);
	if (rc < 0) {
		dev_err(chg->dev,
			"Couldn't configure s/w jeita rc=%d\n",
			rc);
		return rc;
	}
	return 0;
}

/*******************
 * DC PSY GETTERS *
 *******************/

int smblib_get_prop_dc_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, DCIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read DCIN_RT_STS rc=%d\n", rc);
		return rc;
	}

	val->intval = (bool)(stat & DCIN_PLUGIN_RT_STS_BIT);
	return 0;
}

int smblib_get_prop_dc_online(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	int rc = 0;
	u8 stat;

	if (get_client_vote(chg->dc_suspend_votable, USER_VOTER)) {
		val->intval = false;
		return rc;
	}

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "POWER_PATH_STATUS = 0x%02x\n",
		   stat);

	val->intval = (stat & USE_DCIN_BIT) &&
		      (stat & VALID_INPUT_POWER_SOURCE_STS_BIT);

	return rc;
}

int smblib_get_prop_dc_current_max(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	val->intval = get_effective_result_locked(chg->dc_icl_votable);
	return 0;
}

/*******************
 * DC PSY SETTERS *
 * *****************/

int smblib_set_prop_dc_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc;

	rc = vote(chg->dc_icl_votable, USER_VOTER, true, val->intval);
	return rc;
}

/*******************
 * USB PSY GETTERS *
 *******************/

int smblib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USBIN_RT_STS rc=%d\n", rc);
		return rc;
	}

	if (!chg->otg_en)
		val->intval = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
	else
		val->intval = 0;
	return 0;
}

int smblib_get_prop_usb_online(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	int rc = 0;
#ifdef QCOM_BASE
	u8 stat;

	if (get_client_vote_locked(chg->usb_icl_votable, USER_VOTER) == 0) {
		val->intval = false;
		return rc;
	}

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "POWER_PATH_STATUS = 0x%02x\n",
		   stat);

	val->intval = (stat & USE_USBIN_BIT) &&
		      (stat & VALID_INPUT_POWER_SOURCE_STS_BIT);
#else
	if (chg->mmi.apsd_done)
		rc = smblib_get_prop_usb_present(chg, val);
	else
		val->intval = 0;
#endif
	return rc;
}

int smblib_get_prop_usb_voltage_max(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_PD:
		if (chg->smb_version == PM660_SUBTYPE)
			val->intval = MICRO_9V;
		else
			val->intval = MICRO_12V;
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

int smblib_get_prop_usb_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	if (!chg->iio.usbin_v_chan ||
		PTR_ERR(chg->iio.usbin_v_chan) == -EPROBE_DEFER)
		chg->iio.usbin_v_chan = iio_channel_get(chg->dev, "usbin_v");

	if (IS_ERR(chg->iio.usbin_v_chan))
		return PTR_ERR(chg->iio.usbin_v_chan);

	return iio_read_channel_processed(chg->iio.usbin_v_chan, &val->intval);
}

int smblib_get_prop_usb_current_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc = 0;

	rc = smblib_get_prop_usb_present(chg, val);
	if (rc < 0 || !val->intval)
		return rc;

	if (!chg->iio.usbin_i_chan ||
		PTR_ERR(chg->iio.usbin_i_chan) == -EPROBE_DEFER)
		chg->iio.usbin_i_chan = iio_channel_get(chg->dev, "usbin_i");

	if (IS_ERR(chg->iio.usbin_i_chan))
		return PTR_ERR(chg->iio.usbin_i_chan);

	return iio_read_channel_processed(chg->iio.usbin_i_chan, &val->intval);
}

int smblib_get_prop_charger_temp(struct smb_charger *chg,
				 union power_supply_propval *val)
{
	int rc;

	if (!chg->iio.temp_chan ||
		PTR_ERR(chg->iio.temp_chan) == -EPROBE_DEFER)
		chg->iio.temp_chan = iio_channel_get(chg->dev, "charger_temp");

	if (IS_ERR(chg->iio.temp_chan))
		return PTR_ERR(chg->iio.temp_chan);

	rc = iio_read_channel_processed(chg->iio.temp_chan, &val->intval);
	val->intval /= 100;
	return rc;
}

int smblib_get_prop_charger_temp_max(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc;

	if (!chg->iio.temp_max_chan ||
		PTR_ERR(chg->iio.temp_max_chan) == -EPROBE_DEFER)
		chg->iio.temp_max_chan = iio_channel_get(chg->dev,
							 "charger_temp_max");
	if (IS_ERR(chg->iio.temp_max_chan))
		return PTR_ERR(chg->iio.temp_max_chan);

	rc = iio_read_channel_processed(chg->iio.temp_max_chan, &val->intval);
	val->intval /= 100;
	return rc;
}

int smblib_get_prop_typec_cc_orientation(struct smb_charger *chg,
					 union power_supply_propval *val)
{
	if (chg->typec_status[3] & CC_ATTACHED_BIT)
		val->intval =
			(bool)(chg->typec_status[3] & CC_ORIENTATION_BIT) + 1;
	else
		val->intval = 0;

	return 0;
}

static const char * const smblib_typec_mode_name[] = {
	[POWER_SUPPLY_TYPEC_NONE]		  = "NONE",
	[POWER_SUPPLY_TYPEC_SOURCE_DEFAULT]	  = "SOURCE_DEFAULT",
	[POWER_SUPPLY_TYPEC_SOURCE_MEDIUM]	  = "SOURCE_MEDIUM",
	[POWER_SUPPLY_TYPEC_SOURCE_HIGH]	  = "SOURCE_HIGH",
	[POWER_SUPPLY_TYPEC_NON_COMPLIANT]	  = "NON_COMPLIANT",
	[POWER_SUPPLY_TYPEC_SINK]		  = "SINK",
	[POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE]   = "SINK_POWERED_CABLE",
	[POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY] = "SINK_DEBUG_ACCESSORY",
	[POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER]   = "SINK_AUDIO_ADAPTER",
	[POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY]   = "POWERED_CABLE_ONLY",
};

static int smblib_get_prop_ufp_mode(struct smb_charger *chg)
{
	switch (chg->typec_status[0]) {
	case UFP_TYPEC_RDSTD_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	case UFP_TYPEC_RD1P5_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case UFP_TYPEC_RD3P0_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblib_get_prop_dfp_mode(struct smb_charger *chg)
{
	switch (chg->typec_status[1] & DFP_TYPEC_MASK) {
	case DFP_RA_RA_BIT:
		return POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
	case DFP_RD_RD_BIT:
		return POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY;
	case DFP_RD_RA_VCONN_BIT:
		return POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE;
	case DFP_RD_OPEN_BIT:
		return POWER_SUPPLY_TYPEC_SINK;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblib_get_prop_typec_mode(struct smb_charger *chg)
{
	if (chg->typec_status[3] & UFP_DFP_MODE_STATUS_BIT)
		return smblib_get_prop_dfp_mode(chg);
	else
		return smblib_get_prop_ufp_mode(chg);
}

int smblib_get_prop_typec_power_role(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc = 0;
	u8 ctrl;

	rc = smblib_read(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG, &ctrl);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_INTRPT_ENB_SOFTWARE_CTRL = 0x%02x\n",
		   ctrl);

	if (ctrl & TYPEC_DISABLE_CMD_BIT) {
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		return rc;
	}

	switch (ctrl & (DFP_EN_CMD_BIT | UFP_EN_CMD_BIT)) {
	case 0:
		val->intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		break;
	case DFP_EN_CMD_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
		break;
	case UFP_EN_CMD_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SINK;
		break;
	default:
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		smblib_err(chg, "unsupported power role 0x%02lx\n",
			ctrl & (DFP_EN_CMD_BIT | UFP_EN_CMD_BIT));
		return -EINVAL;
	}

	return rc;
}

int smblib_get_prop_pd_allowed(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	val->intval = get_effective_result(chg->pd_allowed_votable);
	return 0;
}

int smblib_get_prop_input_current_settled(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	return smblib_get_charge_param(chg, &chg->param.icl_stat, &val->intval);
}

#define HVDCP3_STEP_UV	200000
int smblib_get_prop_input_voltage_settled(struct smb_charger *chg,
						union power_supply_propval *val)
{
	int rc, pulses;

	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		rc = smblib_get_pulse_cnt(chg, &pulses);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't read QC_PULSE_COUNT rc=%d\n", rc);
			return 0;
		}
		val->intval = MICRO_5V + HVDCP3_STEP_UV * pulses;
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		val->intval = chg->voltage_min_uv;
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

int smblib_get_prop_pd_in_hard_reset(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	val->intval = chg->pd_hard_reset;
	return 0;
}

int smblib_get_pe_start(struct smb_charger *chg,
			       union power_supply_propval *val)
{
	/*
	 * hvdcp timeout voter is the last one to allow pd. Use its vote
	 * to indicate start of pe engine
	 */
	val->intval
		= !get_client_vote_locked(chg->pd_disallowed_votable_indirect,
			HVDCP_TIMEOUT_VOTER);
	return 0;
}

int smblib_get_prop_die_health(struct smb_charger *chg,
						union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, TEMP_RANGE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TEMP_RANGE_STATUS_REG rc=%d\n",
									rc);
		return rc;
	}

	/* TEMP_RANGE bits are mutually exclusive */
	switch (stat & TEMP_RANGE_MASK) {
	case TEMP_BELOW_RANGE_BIT:
		val->intval = POWER_SUPPLY_HEALTH_COOL;
		break;
	case TEMP_WITHIN_RANGE_BIT:
		val->intval = POWER_SUPPLY_HEALTH_WARM;
		break;
	case TEMP_ABOVE_RANGE_BIT:
		val->intval = POWER_SUPPLY_HEALTH_HOT;
		break;
	case ALERT_LEVEL_BIT:
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	default:
		val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	return 0;
}

#define SDP_CURRENT_UA			500000
#define CDP_CURRENT_UA			1500000
#define DCP_CURRENT_UA			1500000
#define HVDCP_CURRENT_UA		3000000
#define TYPEC_DEFAULT_CURRENT_UA	900000
#define TYPEC_MEDIUM_CURRENT_UA		1500000
#define TYPEC_HIGH_CURRENT_UA		3000000
static int get_rp_based_dcp_current(struct smb_charger *chg, int typec_mode)
{
	int rp_ua;

	switch (typec_mode) {
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		rp_ua = TYPEC_HIGH_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
	/* fall through */
	default:
		rp_ua = DCP_CURRENT_UA;
	}

	return rp_ua;
}

/*******************
 * USB PSY SETTERS *
 * *****************/

int smblib_set_prop_pd_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc;

#ifdef QCOM_BASE
	if (chg->pd_active)
		rc = vote(chg->usb_icl_votable, PD_VOTER, true, val->intval);
	else
		rc = -EPERM;
#else
	bool enable = true;

	if (0 == val->intval)
		enable = false;

	if (chg->pd_active)
		rc = vote(chg->usb_icl_votable, PD_VOTER, enable, val->intval);
	else
		rc = -EPERM;
#endif
	return rc;
}

#ifdef QCOM_BASE
static int smblib_handle_usb_current(struct smb_charger *chg,
					int usb_current)
{
	int rc = 0, rp_ua, typec_mode;

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		if (usb_current == -ETIMEDOUT) {
			/*
			 * Valid FLOAT charger, report the current based
			 * of Rp
			 */
			typec_mode = smblib_get_prop_typec_mode(chg);
			rp_ua = get_rp_based_dcp_current(chg, typec_mode);
			rc = vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER,
								true, rp_ua);
			if (rc < 0)
				return rc;
		} else {
			/*
			 * FLOAT charger detected as SDP by USB driver,
			 * charge with the requested current and update the
			 * real_charger_type
			 */
			chg->real_charger_type = POWER_SUPPLY_TYPE_USB;
			rc = vote(chg->usb_icl_votable, USB_PSY_VOTER,
						true, usb_current);
			if (rc < 0)
				return rc;
			rc = vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER,
							false, 0);
			if (rc < 0)
				return rc;
		}
	} else {
		rc = vote(chg->usb_icl_votable, USB_PSY_VOTER,
					true, usb_current);
	}

	return rc;
}
#endif

int smblib_set_prop_sdp_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc = 0;

#ifdef QCOM_BASE
	if (!chg->pd_active) {
		rc = smblib_handle_usb_current(chg, val->intval);
	} else if (chg->system_suspend_supported) {
		if (val->intval <= USBIN_25MA)
			rc = vote(chg->usb_icl_votable,
				PD_SUSPEND_SUPPORTED_VOTER, true, val->intval);
		else
			rc = vote(chg->usb_icl_votable,
				PD_SUSPEND_SUPPORTED_VOTER, false, 0);
	}
#else
	bool enable = true;

	if (0 == val->intval)
		enable = false;

	if (!chg->pd_active) {
		rc = vote(chg->usb_icl_votable, USB_PSY_VOTER,
				enable, val->intval);
	} else if (chg->system_suspend_supported) {
		if (val->intval <= USBIN_25MA)
			rc = vote(chg->usb_icl_votable, USB_PSY_VOTER,
					enable, val->intval);
		else
			rc = vote(chg->usb_icl_votable, USB_PSY_VOTER,
					false, 0);
	}
#endif
	return rc;
}

int smblib_set_prop_boost_current(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_boost,
				val->intval <= chg->boost_threshold_ua ?
				chg->chg_freq.freq_below_otg_threshold :
				chg->chg_freq.freq_above_otg_threshold);
	if (rc < 0) {
		dev_err(chg->dev, "Error in setting freq_boost rc=%d\n", rc);
		return rc;
	}

	chg->boost_current_ua = val->intval;
	return rc;
}

int smblib_set_prop_typec_power_role(struct smb_charger *chg,
				     const union power_supply_propval *val)
{
	int rc = 0;
	u8 power_role;

	switch (val->intval) {
	case POWER_SUPPLY_TYPEC_PR_NONE:
		power_role = TYPEC_DISABLE_CMD_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_DUAL:
		power_role = 0;
		break;
	case POWER_SUPPLY_TYPEC_PR_SINK:
		power_role = UFP_EN_CMD_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_SOURCE:
		power_role = DFP_EN_CMD_BIT;
		break;
	default:
		smblib_err(chg, "power role %d not supported\n", val->intval);
		return -EINVAL;
	}

	if (chg->wa_flags & TYPEC_PBS_WA_BIT) {
		if (power_role == UFP_EN_CMD_BIT) {
			/* disable PBS workaround when forcing sink mode */
			rc = smblib_write(chg, TM_IO_DTEST4_SEL, 0x0);
			if (rc < 0) {
				smblib_err(chg, "Couldn't write to TM_IO_DTEST4_SEL rc=%d\n",
					rc);
			}
		} else {
			/* restore it back to 0xA5 */
			rc = smblib_write(chg, TM_IO_DTEST4_SEL, 0xA5);
			if (rc < 0) {
				smblib_err(chg, "Couldn't write to TM_IO_DTEST4_SEL rc=%d\n",
					rc);
			}
		}
#ifndef QCOM_BASE
		/* increase VCONN softstart and advertise default current*/
		rc = smblib_masked_write(chg, TYPE_C_CFG_2_REG,
				VCONN_SOFTSTART_CFG_MASK | EN_80UA_180UA_CUR_SOURCE_BIT,
				VCONN_SOFTSTART_CFG_MASK);
		if (rc < 0) {
			smblib_err(chg, "Couldn't write to TYPE_C_CFG_2_REG rc=%d\n",
				rc);
		}
#endif
	}

	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 TYPEC_POWER_ROLE_CMD_MASK, power_role);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write 0x%02x to TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
			power_role, rc);
		return rc;
	}

	return rc;
}

int smblib_set_prop_pd_voltage_min(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc, min_uv;

	min_uv = min(val->intval, chg->voltage_max_uv);
	rc = smblib_set_usb_pd_allowed_voltage(chg, min_uv,
					       chg->voltage_max_uv);
	if (rc < 0) {
		smblib_err(chg, "invalid max voltage %duV rc=%d\n",
			val->intval, rc);
		return rc;
	}

	chg->voltage_min_uv = min_uv;
	power_supply_changed(chg->usb_main_psy);
	return rc;
}

int smblib_set_prop_pd_voltage_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc, max_uv;

	max_uv = max(val->intval, chg->voltage_min_uv);
	if ((chg->pd_active) && (max_uv != chg->voltage_max_uv))
		chg->pd_contract_uv = 0;

	rc = smblib_set_usb_pd_allowed_voltage(chg, chg->voltage_min_uv,
					       max_uv);
	if (rc < 0) {
		smblib_err(chg, "invalid max voltage %duV rc=%d\n",
			val->intval, rc);
		return rc;
	}

	chg->voltage_max_uv = max_uv;
	return rc;
}

static int __smblib_set_prop_pd_active(struct smb_charger *chg, bool pd_active)
{
	int rc;
	bool orientation, sink_attached, hvdcp;
	u8 stat;

	if (chg->mmi.factory_mode)
		return 0;

	chg->pd_active = pd_active;
	if (chg->pd_active) {
#ifndef QCOM_BASE
		/*
		 * increase VCONN softstart and advertise 1.5A current
		 * to comply with the Type-C Specification. While an explicit
		 * USB PD contract is in place, the provider shall advertise
		 * a USB Type-C Current of either 1.5 A or 3.0 A.*/
		rc = smblib_masked_write(chg, TYPE_C_CFG_2_REG,
				VCONN_SOFTSTART_CFG_MASK | EN_80UA_180UA_CUR_SOURCE_BIT,
				VCONN_SOFTSTART_CFG_MASK | EN_80UA_180UA_CUR_SOURCE_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't write to TYPE_C_CFG_2_REG rc=%d\n",
				rc);
		}
#endif
		vote(chg->apsd_disable_votable, PD_VOTER, true, 0);
		vote(chg->pd_allowed_votable, PD_VOTER, true, 0);
		vote(chg->usb_irq_enable_votable, PD_VOTER, true, 0);

		/*
		 * VCONN_EN_ORIENTATION_BIT controls whether to use CC1 or CC2
		 * line when TYPEC_SPARE_CFG_BIT (CC pin selection s/w override)
		 * is set or when VCONN_EN_VALUE_BIT is set.
		 */
		orientation = chg->typec_status[3] & CC_ORIENTATION_BIT;
		rc = smblib_masked_write(chg,
				TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				VCONN_EN_ORIENTATION_BIT,
				orientation ? 0 : VCONN_EN_ORIENTATION_BIT);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't enable vconn on CC line rc=%d\n", rc);
			return rc;
		}

		if (chg->pd_contract_uv <= 0) {
			cancel_delayed_work(&chg->pd_contract_work);
			schedule_delayed_work(&chg->pd_contract_work,
					      msecs_to_jiffies(0));
		}

		/* SW controlled CC_OUT */
		rc = smblib_masked_write(chg, TAPER_TIMER_SEL_CFG_REG,
				TYPEC_SPARE_CFG_BIT, TYPEC_SPARE_CFG_BIT);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable SW cc_out rc=%d\n",
									rc);

		/*
		 * Enforce 500mA for PD until the real vote comes in later.
		 * It is guaranteed that pd_active is set prior to
		 * pd_current_max
		 */
		rc = vote(chg->usb_icl_votable, PD_VOTER, true, USBIN_500MA);
		if (rc < 0)
			smblib_err(chg, "Couldn't vote for USB ICL rc=%d\n",
									rc);

		/* since PD was found the cable must be non-legacy */
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, false, 0);

		/* clear USB ICL vote for DCP_VOTER */
		rc = vote(chg->usb_icl_votable, DCP_VOTER, false, 0);
		if (rc < 0)
			smblib_err(chg, "Couldn't un-vote DCP from USB ICL rc=%d\n",
									rc);

		/* remove USB_PSY_VOTER */
		rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
		if (rc < 0)
			smblib_err(chg, "Couldn't unvote USB_PSY rc=%d\n", rc);
	} else {
#ifndef QCOM_BASE
		/* increase VCONN softstart and advertise default current*/
		rc = smblib_masked_write(chg, TYPE_C_CFG_2_REG,
				VCONN_SOFTSTART_CFG_MASK | EN_80UA_180UA_CUR_SOURCE_BIT,
				VCONN_SOFTSTART_CFG_MASK);
		if (rc < 0) {
			smblib_err(chg, "Couldn't write to TYPE_C_CFG_2_REG rc=%d\n",
				rc);
		}
#endif
		rc = smblib_read(chg, APSD_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read APSD status rc=%d\n",
									rc);
			return rc;
		}

		hvdcp = stat & QC_CHARGER_BIT;
		vote(chg->apsd_disable_votable, PD_VOTER, false, 0);
		vote(chg->pd_allowed_votable, PD_VOTER, true, 0);
		vote(chg->usb_irq_enable_votable, PD_VOTER, false, 0);
#ifdef QCOM_BASE
		vote(chg->hvdcp_disable_votable_indirect, PD_INACTIVE_VOTER,
								false, 0);
#endif
		/* HW controlled CC_OUT */
		rc = smblib_masked_write(chg, TAPER_TIMER_SEL_CFG_REG,
							TYPEC_SPARE_CFG_BIT, 0);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable HW cc_out rc=%d\n",
									rc);

		/*
		 * This WA should only run for HVDCP. Non-legacy SDP/CDP could
		 * draw more, but this WA will remove Rd causing VBUS to drop,
		 * and data could be interrupted. Non-legacy DCP could also draw
		 * more, but it may impact compliance.
		 */
		sink_attached = chg->typec_status[3] & UFP_DFP_MODE_STATUS_BIT;
#ifdef QCOM_BASE
		if (!chg->typec_legacy_valid && !sink_attached && hvdcp)
			schedule_work(&chg->legacy_detection_work);
#else
		/* re-run APSD if HVDCP was detected */
		if (hvdcp && !chg->mmi.hvdcp3_con) {
			smblib_rerun_apsd(chg);
			chg->mmi.vbus_inc_cnt = 0;
			chg->mmi.hvdcp3_con = false;
		}
#endif
	}

	smblib_update_usb_type(chg);
	power_supply_changed(chg->usb_psy);
	return rc;
}

int smblib_set_prop_pd_active(struct smb_charger *chg,
			      const union power_supply_propval *val)
{
	if (!get_effective_result(chg->pd_allowed_votable))
		return -EINVAL;

	return __smblib_set_prop_pd_active(chg, val->intval);
}

int smblib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;

	smblib_dbg(chg, PR_MISC, "Set ship mode: %d!!\n", !!val->intval);

	rc = smblib_masked_write(chg, SHIP_MODE_REG, SHIP_MODE_EN_BIT,
			!!val->intval ? SHIP_MODE_EN_BIT : 0);
	if (rc < 0)
		dev_err(chg->dev, "Couldn't %s ship mode, rc=%d\n",
				!!val->intval ? "enable" : "disable", rc);

	return rc;
}

int smblib_reg_block_update(struct smb_charger *chg,
				struct reg_info *entry)
{
	int rc = 0;

	while (entry && entry->reg) {
		rc = smblib_read(chg, entry->reg, &entry->bak);
		if (rc < 0) {
			dev_err(chg->dev, "Error in reading %s rc=%d\n",
				entry->desc, rc);
			break;
		}
		entry->bak &= entry->mask;

		rc = smblib_masked_write(chg, entry->reg,
					 entry->mask, entry->val);
		if (rc < 0) {
			dev_err(chg->dev, "Error in writing %s rc=%d\n",
				entry->desc, rc);
			break;
		}
		entry++;
	}

	return rc;
}

int smblib_reg_block_restore(struct smb_charger *chg,
				struct reg_info *entry)
{
	int rc = 0;

	while (entry && entry->reg) {
		rc = smblib_masked_write(chg, entry->reg,
					 entry->mask, entry->bak);
		if (rc < 0) {
			dev_err(chg->dev, "Error in writing %s rc=%d\n",
				entry->desc, rc);
			break;
		}
		entry++;
	}

	return rc;
}

static struct reg_info cc2_detach_settings[] = {
	{
		.reg	= TYPE_C_CFG_2_REG,
		.mask	= TYPE_C_UFP_MODE_BIT | EN_TRY_SOURCE_MODE_BIT,
		.val	= TYPE_C_UFP_MODE_BIT,
		.desc	= "TYPE_C_CFG_2_REG",
	},
	{
		.reg	= TYPE_C_CFG_3_REG,
		.mask	= EN_TRYSINK_MODE_BIT,
		.val	= 0,
		.desc	= "TYPE_C_CFG_3_REG",
	},
	{
		.reg	= TAPER_TIMER_SEL_CFG_REG,
		.mask	= TYPEC_SPARE_CFG_BIT,
		.val	= TYPEC_SPARE_CFG_BIT,
		.desc	= "TAPER_TIMER_SEL_CFG_REG",
	},
	{
		.reg	= TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
		.mask	= VCONN_EN_ORIENTATION_BIT,
		.val	= 0,
		.desc	= "TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG",
	},
	{
		.reg	= MISC_CFG_REG,
		.mask	= TCC_DEBOUNCE_20MS_BIT,
		.val	= TCC_DEBOUNCE_20MS_BIT,
		.desc	= "Tccdebounce time"
	},
	{
	},
};

static int smblib_cc2_sink_removal_enter(struct smb_charger *chg)
{
	int rc, ccout, ufp_mode;
	u8 stat;

	if ((chg->wa_flags & TYPEC_CC2_REMOVAL_WA_BIT) == 0)
		return 0;

	if (chg->cc2_detach_wa_active)
		return 0;

	rc = smblib_read(chg, TYPE_C_STATUS_4_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return rc;
	}

	ccout = (stat & CC_ATTACHED_BIT) ?
					(!!(stat & CC_ORIENTATION_BIT) + 1) : 0;
	ufp_mode = (stat & TYPEC_DEBOUNCE_DONE_STATUS_BIT) ?
					!(stat & UFP_DFP_MODE_STATUS_BIT) : 0;

	if (ccout != 2)
		return 0;

	if (!ufp_mode)
		return 0;

	chg->cc2_detach_wa_active = true;
	/* The CC2 removal WA will cause a type-c-change IRQ storm */
	smblib_reg_block_update(chg, cc2_detach_settings);
	schedule_work(&chg->rdstd_cc2_detach_work);
	return rc;
}

static int smblib_cc2_sink_removal_exit(struct smb_charger *chg)
{
	if ((chg->wa_flags & TYPEC_CC2_REMOVAL_WA_BIT) == 0)
		return 0;

	if (!chg->cc2_detach_wa_active)
		return 0;

	chg->cc2_detach_wa_active = false;
	cancel_work_sync(&chg->rdstd_cc2_detach_work);
	smblib_reg_block_restore(chg, cc2_detach_settings);
	return 0;
}

int smblib_set_prop_pd_in_hard_reset(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	if (chg->pd_hard_reset == val->intval)
		return rc;

	chg->pd_hard_reset = val->intval;
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
			EXIT_SNK_BASED_ON_CC_BIT,
			(chg->pd_hard_reset) ? EXIT_SNK_BASED_ON_CC_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set EXIT_SNK_BASED_ON_CC rc=%d\n",
				rc);

	vote(chg->apsd_disable_votable, PD_HARD_RESET_VOTER,
							chg->pd_hard_reset, 0);

	return rc;
}

static int smblib_recover_from_soft_jeita(struct smb_charger *chg)
{
	u8 stat_1, stat_2;
	int rc;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat_1);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
				rc);
		return rc;
	}

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_2_REG, &stat_2);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
				rc);
		return rc;
	}

	if ((chg->jeita_status && !(stat_2 & BAT_TEMP_STATUS_SOFT_LIMIT_MASK) &&
		((stat_1 & BATTERY_CHARGER_STATUS_MASK) == TERMINATE_CHARGE))) {
		/*
		 * We are moving from JEITA soft -> Normal and charging
		 * is terminated
		 */
		rc = smblib_write(chg, CHARGING_ENABLE_CMD_REG, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable charging rc=%d\n",
						rc);
			return rc;
		}
		rc = smblib_write(chg, CHARGING_ENABLE_CMD_REG,
						CHARGING_ENABLE_CMD_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable charging rc=%d\n",
						rc);
			return rc;
		}
	}

	chg->jeita_status = stat_2 & BAT_TEMP_STATUS_SOFT_LIMIT_MASK;

	return 0;
}

/***********************
* USB MAIN PSY GETTERS *
*************************/
int smblib_get_prop_fcc_delta(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc, jeita_cc_delta_ua = 0;

	rc = smblib_get_jeita_cc_delta(chg, &jeita_cc_delta_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get jeita cc delta rc=%d\n", rc);
		jeita_cc_delta_ua = 0;
	}

	val->intval = jeita_cc_delta_ua;
	return 0;
}

/***********************
* USB MAIN PSY SETTERS *
*************************/
int smblib_get_charge_current(struct smb_charger *chg,
				int *total_current_ua)
{
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);
	union power_supply_propval val = {0, };
	int rc = 0, typec_source_rd, current_ua;
	bool non_compliant;
	u8 stat5;

	if (chg->pd_active) {
		*total_current_ua =
			get_client_vote_locked(chg->usb_icl_votable, PD_VOTER);
		return rc;
	}

	rc = smblib_read(chg, TYPE_C_STATUS_5_REG, &stat5);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_5 rc=%d\n", rc);
		return rc;
	}
	non_compliant = stat5 & TYPEC_NONCOMP_LEGACY_CABLE_STATUS_BIT;

	/* get settled ICL */
	rc = smblib_get_prop_input_current_settled(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get settled ICL rc=%d\n", rc);
		return rc;
	}

	typec_source_rd = smblib_get_prop_ufp_mode(chg);

	/* QC 2.0/3.0 adapter */
	if (apsd_result->bit & (QC_3P0_BIT | QC_2P0_BIT)) {
		*total_current_ua = HVDCP_CURRENT_UA;
		return 0;
	}

	if (non_compliant) {
		switch (apsd_result->bit) {
		case CDP_CHARGER_BIT:
			current_ua = CDP_CURRENT_UA;
			break;
		case DCP_CHARGER_BIT:
		case OCP_CHARGER_BIT:
		case FLOAT_CHARGER_BIT:
			current_ua = DCP_CURRENT_UA;
			break;
		default:
			current_ua = 0;
			break;
		}

		*total_current_ua = max(current_ua, val.intval);
		return 0;
	}

	switch (typec_source_rd) {
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
		switch (apsd_result->bit) {
		case CDP_CHARGER_BIT:
			current_ua = CDP_CURRENT_UA;
			break;
		case DCP_CHARGER_BIT:
		case OCP_CHARGER_BIT:
		case FLOAT_CHARGER_BIT:
			current_ua = chg->default_icl_ua;
			break;
		default:
			current_ua = 0;
			break;
		}
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
		current_ua = TYPEC_MEDIUM_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		current_ua = TYPEC_HIGH_CURRENT_UA;
		break;
	case POWER_SUPPLY_TYPEC_NON_COMPLIANT:
	case POWER_SUPPLY_TYPEC_NONE:
	default:
		current_ua = 0;
		break;
	}

	*total_current_ua = max(current_ua, val.intval);
	return 0;
}

/************************
 * PARALLEL PSY GETTERS *
 ************************/

int smblib_get_prop_slave_current_now(struct smb_charger *chg,
		union power_supply_propval *pval)
{
	if (IS_ERR_OR_NULL(chg->iio.batt_i_chan))
		chg->iio.batt_i_chan = iio_channel_get(chg->dev, "batt_i");

	if (IS_ERR(chg->iio.batt_i_chan))
		return PTR_ERR(chg->iio.batt_i_chan);

	return iio_read_channel_processed(chg->iio.batt_i_chan, &pval->intval);
}

/**********************
 * INTERRUPT HANDLERS *
 **********************/

irqreturn_t smblib_handle_debug(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_otg_overcurrent(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;
	u8 stat;

	rc = smblib_read(chg, OTG_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't read OTG_INT_RT_STS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	if (chg->wa_flags & OTG_WA) {
		if (stat & OTG_OC_DIS_SW_STS_RT_STS_BIT)
			smblib_err(chg, "OTG disabled by hw\n");

		/* not handling software based hiccups for PM660 */
		return IRQ_HANDLED;
	}

	if (stat & OTG_OVERCURRENT_RT_STS_BIT)
		schedule_work(&chg->otg_oc_work);

	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_chg_state_change(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
				rc);
		return IRQ_HANDLED;
	}

	stat = stat & BATTERY_CHARGER_STATUS_MASK;
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_batt_temp_changed(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;

	rc = smblib_recover_from_soft_jeita(chg);
	if (rc < 0) {
		smblib_err(chg, "Couldn't recover chg from soft jeita rc=%d\n",
				rc);
		return IRQ_HANDLED;
	}

	rerun_election(chg->fcc_votable);
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_batt_psy_changed(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_usb_psy_changed(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	power_supply_changed(chg->usb_psy);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_usbin_uv(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	if (!chg->irq_info[SWITCH_POWER_OK_IRQ].irq_data)
		return IRQ_HANDLED;

	wdata = &chg->irq_info[SWITCH_POWER_OK_IRQ].irq_data->storm_data;
	reset_storm_count(wdata);
	return IRQ_HANDLED;
}

static void smblib_micro_usb_plugin(struct smb_charger *chg, bool vbus_rising)
{
	if (vbus_rising) {
		/* use the typec flag even though its not typec */
		chg->typec_present = 1;
	} else {
		chg->typec_present = 0;
		smblib_update_usb_type(chg);
		extcon_set_cable_state_(chg->extcon, EXTCON_USB, false);
		smblib_uusb_removal(chg);
	}
}

void smblib_usb_plugin_hard_reset_locked(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	bool vbus_rising;
	struct smb_irq_data *data;
	struct storm_watch *wdata;

	if (chg->otg_en) {
		smblib_dbg(chg, PR_INTERRUPT, "OTG enabled, do nothing\n");
		return;
	}

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return;
	}

	vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	if (vbus_rising) {
		smblib_cc2_sink_removal_exit(chg);
	} else {
		smblib_cc2_sink_removal_enter(chg);
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCH_POWER_OK_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				update_storm_count(wdata,
						WEAK_CHG_STORM_COUNT);
				chg->reverse_boost = false;
				vote(chg->usb_icl_votable, BOOST_BACK_VOTER,
						false, 0);
				vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
						false, 0);
			}
		}
	}

	power_supply_changed(chg->usb_psy);
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: usbin-plugin %s\n",
					vbus_rising ? "attached" : "detached");
}

#define PL_DELAY_MS			30000
static int factory_kill_disable;
module_param(factory_kill_disable, int, 0644);
void smblib_usb_plugin_locked(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	bool vbus_rising;
	struct smb_irq_data *data;
	struct storm_watch *wdata;

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return;
	}

	vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
	smblib_set_opt_freq_buck(chg, vbus_rising ? chg->chg_freq.freq_5V :
						chg->chg_freq.freq_removal);

	if (vbus_rising) {
		rc = smblib_request_dpdm(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);
		if (chg->fcc_stepper_mode)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER, false, 0);
		/* Schedule work to enable parallel charger */
		vote(chg->awake_votable, PL_DELAY_VOTER, true, 0);
		schedule_delayed_work(&chg->pl_enable_work,
					msecs_to_jiffies(PL_DELAY_MS));

		if (chg->mmi.factory_mode)
			chg->mmi.factory_kill_armed = true;
	} else {
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCH_POWER_OK_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				update_storm_count(wdata,
						WEAK_CHG_STORM_COUNT);
				vote(chg->usb_icl_votable, BOOST_BACK_VOTER,
						false, 0);
				vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
						false, 0);
			}
		}

		/* Force 1500mA FCC on removal */
		if (chg->fcc_stepper_mode)
			vote(chg->fcc_votable, FCC_STEPPER_VOTER,
						true, 1500000);

		rc = smblib_request_dpdm(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't disable DPDM rc=%d\n", rc);

		if (chg->mmi.factory_kill_armed && !factory_kill_disable) {
			smblib_err(chg, "Factory kill power off\n");
			kernel_power_off();
		} else
			chg->mmi.factory_kill_armed = false;
	}

	if (chg->micro_usb_mode)
		smblib_micro_usb_plugin(chg, vbus_rising);

	power_supply_changed(chg->usb_psy);

	__pm_stay_awake(&chg->mmi.smblib_mmi_hb_wake_source);
	cancel_delayed_work(&chg->mmi.heartbeat_work);
	schedule_delayed_work(&chg->mmi.heartbeat_work,
			      msecs_to_jiffies(0));
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: usbin-plugin %s\n",
					vbus_rising ? "attached" : "detached");
}

irqreturn_t smblib_handle_usb_plugin(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	mutex_lock(&chg->lock);
	if (chg->pd_hard_reset)
		smblib_usb_plugin_hard_reset_locked(chg);
	else
		smblib_usb_plugin_locked(chg);
	mutex_unlock(&chg->lock);
	return IRQ_HANDLED;
}

#define USB_WEAK_INPUT_UA	1400000
#define ICL_CHANGE_DELAY_MS	1000
irqreturn_t smblib_handle_icl_change(int irq, void *data)
{
	u8 stat;
	int rc, settled_ua, delay = ICL_CHANGE_DELAY_MS;
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->mode == PARALLEL_MASTER) {
		rc = smblib_read(chg, AICL_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read AICL_STATUS rc=%d\n",
					rc);
			return IRQ_HANDLED;
		}

		rc = smblib_get_charge_param(chg, &chg->param.icl_stat,
				&settled_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get ICL status rc=%d\n", rc);
			return IRQ_HANDLED;
		}

		/* If AICL settled then schedule work now */
		if ((settled_ua == get_effective_result(chg->usb_icl_votable))
				|| (stat & AICL_DONE_BIT))
			delay = 0;

		cancel_delayed_work_sync(&chg->icl_change_work);
		schedule_delayed_work(&chg->icl_change_work,
						msecs_to_jiffies(delay));
	}

	return IRQ_HANDLED;
}

static void smblib_handle_slow_plugin_timeout(struct smb_charger *chg,
					      bool rising)
{
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: slow-plugin-timeout %s\n",
		   rising ? "rising" : "falling");
}

static void smblib_handle_sdp_enumeration_done(struct smb_charger *chg,
					       bool rising)
{
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: sdp-enumeration-done %s\n",
		   rising ? "rising" : "falling");
}

#define MICRO_10P3V	10300000
static void smblib_check_ov_condition(struct smb_charger *chg)
{
	union power_supply_propval pval = {0, };
	int rc;

	if (chg->wa_flags & OV_IRQ_WA_BIT) {
		rc = power_supply_get_property(chg->usb_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get current voltage, rc=%d\n",
				rc);
			return;
		}

		if (pval.intval > MICRO_10P3V) {
			smblib_err(chg, "USBIN OV detected\n");
			vote(chg->hvdcp_hw_inov_dis_votable, OV_VOTER, true,
				0);
			pval.intval = POWER_SUPPLY_DP_DM_FORCE_5V;
			rc = power_supply_set_property(chg->batt_psy,
				POWER_SUPPLY_PROP_DP_DM, &pval);
			return;
		}
	}
}

#define QC3_PULSES_FOR_6V	5
#define QC3_PULSES_FOR_9V	20
#define QC3_PULSES_FOR_12V	35
static void smblib_hvdcp_adaptive_voltage_change(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	int pulses;

	smblib_check_ov_condition(chg);
	power_supply_changed(chg->usb_main_psy);
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP) {
		rc = smblib_read(chg, QC_CHANGE_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't read QC_CHANGE_STATUS rc=%d\n", rc);
			return;
		}

		switch (stat & QC_2P0_STATUS_MASK) {
		case QC_5V_BIT:
			smblib_set_opt_freq_buck(chg,
					chg->chg_freq.freq_5V);
			break;
		case QC_9V_BIT:
			smblib_set_opt_freq_buck(chg,
					chg->chg_freq.freq_9V);
			break;
		case QC_12V_BIT:
			smblib_set_opt_freq_buck(chg,
					chg->chg_freq.freq_12V);
			break;
		default:
			smblib_set_opt_freq_buck(chg,
					chg->chg_freq.freq_removal);
			break;
		}
	}

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
		rc = smblib_get_pulse_cnt(chg, &pulses);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't read QC_PULSE_COUNT rc=%d\n", rc);
			return;
		}

		if (pulses < QC3_PULSES_FOR_6V)
			smblib_set_opt_freq_buck(chg,
				chg->chg_freq.freq_5V);
		else if (pulses < QC3_PULSES_FOR_9V)
			smblib_set_opt_freq_buck(chg,
				chg->chg_freq.freq_6V_8V);
		else if (pulses < QC3_PULSES_FOR_12V)
			smblib_set_opt_freq_buck(chg,
				chg->chg_freq.freq_9V);
		else
			smblib_set_opt_freq_buck(chg,
				chg->chg_freq.freq_12V);
	}
}

/* triggers when HVDCP 3.0 authentication has finished */
static void smblib_handle_hvdcp_3p0_auth_done(struct smb_charger *chg,
					      bool rising)
{
	const struct apsd_result *apsd_result;
	int rc;

	if (!rising)
		return;

	if (chg->wa_flags & QC_AUTH_INTERRUPT_WA_BIT) {
		/*
		 * Disable AUTH_IRQ_EN_CFG_BIT to receive adapter voltage
		 * change interrupt.
		 */
		rc = smblib_masked_write(chg,
				USBIN_SOURCE_CHANGE_INTRPT_ENB_REG,
				AUTH_IRQ_EN_CFG_BIT, 0);
		if (rc < 0)
			smblib_err(chg,
				"Couldn't enable QC auth setting rc=%d\n", rc);
	}

	if (chg->mode == PARALLEL_MASTER)
		vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, true, 0);

	/* the APSD done handler will set the USB supply type */
	apsd_result = smblib_get_apsd_result(chg);
#ifdef QCOM_BASE
	if (get_effective_result(chg->hvdcp_hw_inov_dis_votable)) {
		if (apsd_result->pst == POWER_SUPPLY_TYPE_USB_HVDCP) {
			/* force HVDCP2 to 9V if INOV is disabled */
			rc = smblib_masked_write(chg, CMD_HVDCP_2_REG,
					FORCE_9V_BIT, FORCE_9V_BIT);
			if (rc < 0)
				smblib_err(chg,
					"Couldn't force 9V HVDCP rc=%d\n", rc);
		}
	}
#endif

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-3p0-auth-done rising; %s detected\n",
		   apsd_result->name);
}

static void smblib_handle_hvdcp_check_timeout(struct smb_charger *chg,
					      bool rising, bool qc_charger)
{
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);

	/* Hold off PD only until hvdcp 2.0 detection timeout */
	if (rising) {
#ifdef QCOM_BASE
		vote(chg->pd_disallowed_votable_indirect, HVDCP_TIMEOUT_VOTER,
								false, 0);
#else
		cancel_delayed_work_sync(&chg->hvdcp_detect_work);
		schedule_delayed_work(&chg->hvdcp_detect_work,
				      msecs_to_jiffies(1000));
#endif
		/* enable HDC and ICL irq for QC2/3 charger */
		if (qc_charger)
			vote(chg->usb_irq_enable_votable, QC_VOTER, true, 0);

		/*
		 * HVDCP detection timeout done
		 * If adapter is not QC2.0/QC3.0 - it is a plain old DCP.
		 */
		if (!qc_charger && (apsd_result->bit & DCP_CHARGER_BIT))
			/* enforce DCP ICL if specified */
			vote(chg->usb_icl_votable, DCP_VOTER,
				chg->dcp_icl_ua != -EINVAL, chg->dcp_icl_ua);

		/*
		 * if pd is not allowed, then set pd_active = false right here,
		 * so that it starts the hvdcp engine
		 */
		if (!get_effective_result(chg->pd_allowed_votable) &&
				!chg->micro_usb_mode)
			__smblib_set_prop_pd_active(chg, 0);
	}

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: smblib_handle_hvdcp_check_timeout %s\n",
		   rising ? "rising" : "falling");
}

/* triggers when HVDCP is detected */
static void smblib_handle_hvdcp_detect_done(struct smb_charger *chg,
					    bool rising)
{
	if (!rising)
		return;

	/* the APSD done handler will set the USB supply type */
#ifdef QCOM_BASE
	cancel_delayed_work_sync(&chg->hvdcp_detect_work);
#endif
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-detect-done %s\n",
		   rising ? "rising" : "falling");
}

#ifdef QCOM_BASE
static void smblib_force_legacy_icl(struct smb_charger *chg, int pst)
{
	int typec_mode;
	int rp_ua;

	/* while PD is active it should have complete ICL control */
	if (chg->pd_active)
		return;

	switch (pst) {
	case POWER_SUPPLY_TYPE_USB:
		/*
		 * USB_PSY will vote to increase the current to 500/900mA once
		 * enumeration is done. Ensure that USB_PSY has at least voted
		 * for 100mA before releasing the LEGACY_UNKNOWN vote
		 */
		if (!is_client_vote_enabled(chg->usb_icl_votable,
								USB_PSY_VOTER))
			vote(chg->usb_icl_votable, USB_PSY_VOTER, true, 100000);
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, false, 0);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, 1500000);
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		typec_mode = smblib_get_prop_typec_mode(chg);
		rp_ua = get_rp_based_dcp_current(chg, typec_mode);
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, rp_ua);
		break;
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		/*
		 * limit ICL to 100mA, the USB driver will enumerate to check
		 * if this is a SDP and appropriately set the current
		 */
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, 100000);
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, 3000000);
		break;
	default:
		smblib_err(chg, "Unknown APSD %d; forcing 500mA\n", pst);
		vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, 500000);
		break;
	}
}

#endif
static void smblib_notify_extcon_props(struct smb_charger *chg)
{
	union power_supply_propval val;

	smblib_get_prop_typec_cc_orientation(chg, &val);
	extcon_set_cable_state_(chg->extcon, EXTCON_USB_CC,
					(val.intval == 2) ? 1 : 0);
	extcon_set_cable_state_(chg->extcon, EXTCON_USB_SPEED, true);
}

static void smblib_notify_device_mode(struct smb_charger *chg, bool enable)
{
	if (enable)
		smblib_notify_extcon_props(chg);

	extcon_set_cable_state_(chg->extcon, EXTCON_USB, enable);
}

static void smblib_notify_usb_host(struct smb_charger *chg, bool enable)
{
	if (enable)
		smblib_notify_extcon_props(chg);

	extcon_set_cable_state_(chg->extcon, EXTCON_USB_HOST, enable);
}

#define HVDCP_DET_MS 2500
static void smblib_handle_apsd_done(struct smb_charger *chg, bool rising)
{
	const struct apsd_result *apsd_result;

	if (!rising)
		return;

	apsd_result = smblib_update_usb_type(chg);
#ifdef QCOM_BASE
	if (!chg->typec_legacy_valid)
		smblib_force_legacy_icl(chg, apsd_result->pst);
#endif
	switch (apsd_result->bit) {
	case SDP_CHARGER_BIT:
	case CDP_CHARGER_BIT:
		if (chg->micro_usb_mode)
			extcon_set_cable_state_(chg->extcon, EXTCON_USB,
					true);
		if (chg->use_extcon)
			smblib_notify_device_mode(chg, true);
	case OCP_CHARGER_BIT:
	case FLOAT_CHARGER_BIT:
		/* if not DCP then no hvdcp timeout happens, Enable pd here. */
		vote(chg->pd_disallowed_votable_indirect, HVDCP_TIMEOUT_VOTER,
				false, 0);
		break;
	case DCP_CHARGER_BIT:
		if (chg->wa_flags & QC_CHARGER_DETECTION_WA_BIT)
			schedule_delayed_work(&chg->hvdcp_detect_work,
					      msecs_to_jiffies(HVDCP_DET_MS));
		break;
	default:
		break;
	}

	chg->mmi.apsd_done = true;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: apsd-done rising; %s detected\n",
		   apsd_result->name);
}

irqreturn_t smblib_handle_usb_source_change(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc = 0;
	u8 stat;

	rc = smblib_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smblib_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", stat);

	if (chg->micro_usb_mode && (stat & APSD_DTC_STATUS_DONE_BIT)
			&& !chg->uusb_apsd_rerun_done) {
		/*
		 * Force re-run APSD to handle slow insertion related
		 * charger-mis-detection.
		 */
		chg->uusb_apsd_rerun_done = true;
		smblib_rerun_apsd(chg);
		return IRQ_HANDLED;
	}

	smblib_handle_apsd_done(chg,
		(bool)(stat & APSD_DTC_STATUS_DONE_BIT));

	smblib_handle_hvdcp_detect_done(chg,
		(bool)(stat & QC_CHARGER_BIT));

	smblib_handle_hvdcp_check_timeout(chg,
		(bool)(stat & HVDCP_CHECK_TIMEOUT_BIT),
		(bool)(stat & QC_CHARGER_BIT));

	smblib_handle_hvdcp_3p0_auth_done(chg,
		(bool)(stat & QC_AUTH_DONE_STATUS_BIT));

	smblib_handle_sdp_enumeration_done(chg,
		(bool)(stat & ENUMERATION_DONE_BIT));

	smblib_handle_slow_plugin_timeout(chg,
		(bool)(stat & SLOW_PLUGIN_TIMEOUT_BIT));

	smblib_hvdcp_adaptive_voltage_change(chg);

	power_supply_changed(chg->usb_psy);

	rc = smblib_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smblib_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", stat);

	vote(chg->awake_votable, HEARTBEAT_VOTER, true, true);
	cancel_delayed_work(&chg->mmi.heartbeat_work);
	schedule_delayed_work(&chg->mmi.heartbeat_work,
			      msecs_to_jiffies(0));
	return IRQ_HANDLED;
}

static int typec_try_sink(struct smb_charger *chg)
{
	union power_supply_propval val;
	bool debounce_done, vbus_detected, sink;
	u8 stat;
	int exit_mode = ATTACHED_SRC, rc;

	/* ignore typec interrupt while try.snk WIP */
	chg->try_sink_active = true;

	/* force SNK mode */
	val.intval = POWER_SUPPLY_TYPEC_PR_SINK;
	rc = smblib_set_prop_typec_power_role(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set UFP mode rc=%d\n", rc);
		goto try_sink_exit;
	}

	/* reduce Tccdebounce time to ~20ms */
	rc = smblib_masked_write(chg, MISC_CFG_REG,
			TCC_DEBOUNCE_20MS_BIT, TCC_DEBOUNCE_20MS_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set MISC_CFG_REG rc=%d\n", rc);
		goto try_sink_exit;
	}

	/*
	 * give opportunity to the other side to be a SRC,
	 * for tDRPTRY + Tccdebounce time
	 */
	msleep(120);

	rc = smblib_read(chg, TYPE_C_STATUS_4_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n",
				rc);
		goto try_sink_exit;
	}

	debounce_done = stat & TYPEC_DEBOUNCE_DONE_STATUS_BIT;

	if (!debounce_done)
		/*
		 * The other side didn't switch to source, either it
		 * is an adamant sink or is removed go back to showing Rp
		 */
		goto try_wait_src;

	/*
	 * We are in force sink mode and the other side has switched to
	 * showing Rp. Config DRP in case the other side removes Rp so we
	 * can quickly (20ms) switch to showing our Rp. Note that the spec
	 * needs us to show Rp for 80mS while the drp DFP residency is just
	 * 54mS. But 54mS is plenty time for us to react and force Rp for
	 * the remaining 26mS.
	 */
	val.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
	rc = smblib_set_prop_typec_power_role(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set DFP mode rc=%d\n",
				rc);
		goto try_sink_exit;
	}

	/*
	 * while other side is Rp, wait for VBUS from it; exit if other side
	 * removes Rp
	 */
	do {
		rc = smblib_read(chg, TYPE_C_STATUS_4_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n",
					rc);
			goto try_sink_exit;
		}

		debounce_done = stat & TYPEC_DEBOUNCE_DONE_STATUS_BIT;
		vbus_detected = stat & TYPEC_VBUS_STATUS_BIT;

		/* Successfully transitioned to ATTACHED.SNK */
		if (vbus_detected && debounce_done) {
			exit_mode = ATTACHED_SINK;
			goto try_sink_exit;
		}

		/*
		 * Ensure sink since drp may put us in source if other
		 * side switches back to Rd
		 */
		sink = !(stat &  UFP_DFP_MODE_STATUS_BIT);

		usleep_range(1000, 2000);
	} while (debounce_done && sink);

try_wait_src:
	/*
	 * Transition to trywait.SRC state. check if other side still wants
	 * to be SNK or has been removed.
	 */
	val.intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
	rc = smblib_set_prop_typec_power_role(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set UFP mode rc=%d\n", rc);
		goto try_sink_exit;
	}

	/* Need to be in this state for tDRPTRY time, 75ms~150ms */
	msleep(80);

	rc = smblib_read(chg, TYPE_C_STATUS_4_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		goto try_sink_exit;
	}

	debounce_done = stat & TYPEC_DEBOUNCE_DONE_STATUS_BIT;

	if (debounce_done)
		/* the other side wants to be a sink */
		exit_mode = ATTACHED_SRC;
	else
		/* the other side is detached */
		exit_mode = UNATTACHED_SINK;

try_sink_exit:
	/* release forcing of SRC/SNK mode */
	val.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
	rc = smblib_set_prop_typec_power_role(chg, &val);
	if (rc < 0)
		smblib_err(chg, "Couldn't set DFP mode rc=%d\n", rc);

	/* revert Tccdebounce time back to ~120ms */
	rc = smblib_masked_write(chg, MISC_CFG_REG, TCC_DEBOUNCE_20MS_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set MISC_CFG_REG rc=%d\n", rc);

	chg->try_sink_active = false;

	return exit_mode;
}

static void typec_sink_insertion(struct smb_charger *chg)
{
	int exit_mode;

	/*
	 * Try.SNK entry status - ATTACHWAIT.SRC state and detected Rd-open
	 * or RD-Ra for TccDebounce time.
	 */

	if (*chg->try_sink_enabled) {
		exit_mode = typec_try_sink(chg);

		if (exit_mode != ATTACHED_SRC) {
			smblib_usb_typec_change(chg);
			return;
		}
	}

	/* when a sink is inserted we should not wait on hvdcp timeout to
	 * enable pd
	 */
	vote(chg->pd_disallowed_votable_indirect, HVDCP_TIMEOUT_VOTER,
			false, 0);
	if (chg->use_extcon) {
		smblib_notify_usb_host(chg, true);
		chg->otg_present = true;
	}
}

static void typec_sink_removal(struct smb_charger *chg)
{
	smblib_set_charge_param(chg, &chg->param.freq_boost,
			chg->chg_freq.freq_above_otg_threshold);
	chg->boost_current_ua = 0;
}

static void smblib_handle_typec_removal(struct smb_charger *chg)
{
	int rc;
	struct smb_irq_data *data;
	struct storm_watch *wdata;
	union power_supply_propval val;

	chg->cc2_detach_wa_active = false;

	rc = smblib_request_dpdm(chg, false);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable DPDM rc=%d\n", rc);

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCH_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}

	/* reset APSD voters */
	vote(chg->apsd_disable_votable, PD_HARD_RESET_VOTER, false, 0);
	vote(chg->apsd_disable_votable, PD_VOTER, false, 0);

	cancel_delayed_work_sync(&chg->pl_enable_work);
	cancel_delayed_work_sync(&chg->hvdcp_detect_work);

	/* reset input current limit voters */
#ifdef QCOM_BASE
	vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, 100000);
#endif
	vote(chg->usb_icl_votable, PD_VOTER, false, 0);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	vote(chg->usb_icl_votable, DCP_VOTER, false, 0);
	vote(chg->usb_icl_votable, PL_USBIN_USBIN_VOTER, false, 0);
	vote(chg->usb_icl_votable, SW_QC3_VOTER, false, 0);

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Error getting USB Present rc = %d\n", rc);
		chg->mmi.apsd_done = false;
		chg->mmi.charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;
	} else if (!val.intval) {
		chg->mmi.apsd_done = false;
		chg->mmi.charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;
	}
	/* reset hvdcp voters */
#ifdef QCOM_BASE
	vote(chg->hvdcp_disable_votable_indirect, VBUS_CC_SHORT_VOTER, true, 0);
	vote(chg->hvdcp_disable_votable_indirect, PD_INACTIVE_VOTER, true, 0);
#endif
	vote(chg->hvdcp_hw_inov_dis_votable, OV_VOTER, false, 0);

	/* reset power delivery voters */
	vote(chg->pd_allowed_votable, PD_VOTER, false, 0);
	vote(chg->pd_disallowed_votable_indirect, CC_DETACHED_VOTER, true, 0);
	vote(chg->pd_disallowed_votable_indirect, HVDCP_TIMEOUT_VOTER, true, 0);

	/* reset usb irq voters */
	vote(chg->usb_irq_enable_votable, PD_VOTER, false, 0);
	vote(chg->usb_irq_enable_votable, QC_VOTER, false, 0);

	/* reset parallel voters */
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	vote(chg->usb_icl_votable, USBIN_USBIN_BOOST_VOTER, false, 0);
	chg->vconn_attempts = 0;
	chg->otg_attempts = 0;
	chg->pulse_cnt = 0;
	chg->usb_icl_delta_ua = 0;
	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_5V;
	chg->pd_active = 0;
	chg->pd_hard_reset = 0;
	chg->pd_contract_uv = 0;
	chg->typec_legacy_valid = false;

	/* write back the default FLOAT charger configuration */
	rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				(u8)FLOAT_OPTIONS_MASK, chg->float_cfg);
	if (rc < 0)
		smblib_err(chg, "Couldn't write float charger options rc=%d\n",
			rc);

	/* reset back to 120mS tCC debounce */
	rc = smblib_masked_write(chg, MISC_CFG_REG, TCC_DEBOUNCE_20MS_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set 120mS tCC debounce rc=%d\n", rc);

	/* enable APSD CC trigger for next insertion */
	rc = smblib_masked_write(chg, TYPE_C_CFG_REG,
				APSD_START_ON_CC_BIT, APSD_START_ON_CC_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't enable APSD_START_ON_CC rc=%d\n", rc);

	if (chg->wa_flags & QC_AUTH_INTERRUPT_WA_BIT) {
		/* re-enable AUTH_IRQ_EN_CFG_BIT */
		rc = smblib_masked_write(chg,
				USBIN_SOURCE_CHANGE_INTRPT_ENB_REG,
				AUTH_IRQ_EN_CFG_BIT, AUTH_IRQ_EN_CFG_BIT);
		if (rc < 0)
			smblib_err(chg,
				"Couldn't enable QC auth setting rc=%d\n", rc);
	}

	/* reconfigure allowed voltage for HVDCP */
	rc = smblib_set_adapter_allowance(chg,
			USBIN_ADAPTER_ALLOW_5V_OR_9V_TO_12V);
	if (rc < 0)
		smblib_err(chg, "Couldn't set USBIN_ADAPTER_ALLOW_5V_OR_9V_TO_12V rc=%d\n",
			rc);

	/* enable DRP */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 TYPEC_POWER_ROLE_CMD_MASK, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't enable DRP rc=%d\n", rc);

	/* HW controlled CC_OUT */
	rc = smblib_masked_write(chg, TAPER_TIMER_SEL_CFG_REG,
							TYPEC_SPARE_CFG_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't enable HW cc_out rc=%d\n", rc);

	/* restore crude sensor if PM660/PMI8998 */
	if (chg->wa_flags & TYPEC_PBS_WA_BIT) {
		rc = smblib_write(chg, TM_IO_DTEST4_SEL, 0xA5);
		if (rc < 0)
			smblib_err(chg, "Couldn't restore crude sensor rc=%d\n",
				rc);
	}

	mutex_lock(&chg->vconn_oc_lock);
	if (!chg->vconn_en)
		goto unlock;

	smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 VCONN_EN_VALUE_BIT, 0);
	chg->vconn_en = false;

unlock:
	mutex_unlock(&chg->vconn_oc_lock);

	/* clear exit sink based on cc */
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
						EXIT_SNK_BASED_ON_CC_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't clear exit_sink_based_on_cc rc=%d\n",
				rc);

	typec_sink_removal(chg);
	smblib_update_usb_type(chg);

	if (chg->use_extcon) {
		if (chg->otg_present)
			smblib_notify_usb_host(chg, false);
		else
			smblib_notify_device_mode(chg, false);
	}
	chg->otg_present = false;

	chg->mmi.charging_limit_modes = CHARGING_LIMIT_OFF;
	chg->mmi.hvdcp3_con = false;
	chg->mmi.vbus_inc_cnt = 0;
	vote(chg->awake_votable, HEARTBEAT_VOTER, true, true);
	cancel_delayed_work(&chg->mmi.heartbeat_work);
	schedule_delayed_work(&chg->mmi.heartbeat_work,
			      msecs_to_jiffies(0));
}

static void smblib_handle_typec_insertion(struct smb_charger *chg)
{
	int rc;

	vote(chg->pd_disallowed_votable_indirect, CC_DETACHED_VOTER, false, 0);

	/* disable APSD CC trigger since CC is attached */
	rc = smblib_masked_write(chg, TYPE_C_CFG_REG, APSD_START_ON_CC_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable APSD_START_ON_CC rc=%d\n",
									rc);

	if (chg->typec_status[3] & UFP_DFP_MODE_STATUS_BIT) {
		typec_sink_insertion(chg);
	} else {
		rc = smblib_request_dpdm(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);
		typec_sink_removal(chg);
	}
}

static void smblib_handle_rp_change(struct smb_charger *chg, int typec_mode)
{
	int rp_ua;
	const struct apsd_result *apsd = smblib_get_apsd_result(chg);

	if ((apsd->pst != POWER_SUPPLY_TYPE_USB_DCP)
		&& (apsd->pst != POWER_SUPPLY_TYPE_USB_FLOAT))
		return;

	/*
	 * if APSD indicates FLOAT and the USB stack had detected SDP,
	 * do not respond to Rp changes as we do not confirm that its
	 * a legacy cable
	 */
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB)
		return;
	/*
	 * We want the ICL vote @ 100mA for a FLOAT charger
	 * until the detection by the USB stack is complete.
	 * Ignore the Rp changes unless there is a
	 * pre-existing valid vote.
	 */
	if (apsd->pst == POWER_SUPPLY_TYPE_USB_FLOAT &&
		get_client_vote(chg->usb_icl_votable,
			LEGACY_UNKNOWN_VOTER) <= 100000)
		return;

	/*
	 * handle Rp change for DCP/FLOAT/OCP.
	 * Update the current only if the Rp is different from
	 * the last Rp value.
	 */
	smblib_dbg(chg, PR_MISC, "CC change old_mode=%d new_mode=%d\n",
						chg->typec_mode, typec_mode);

	rp_ua = get_rp_based_dcp_current(chg, typec_mode);
#ifdef QCOM_BASE
	vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, true, rp_ua);
#endif
}

static void smblib_handle_typec_cc_state_change(struct smb_charger *chg)
{
	int typec_mode, icl_vote_ma;
	bool typec_chg = false;

	if (chg->pr_swap_in_progress)
		return;

	typec_mode = smblib_get_prop_typec_mode(chg);
	if (chg->typec_present && (typec_mode != chg->typec_mode))
		smblib_handle_rp_change(chg, typec_mode);

	chg->typec_mode = typec_mode;

	if (!chg->typec_present && chg->typec_mode != POWER_SUPPLY_TYPEC_NONE) {
		chg->typec_present = true;
		smblib_dbg(chg, PR_MISC, "TypeC %s insertion\n",
			smblib_typec_mode_name[chg->typec_mode]);
		smblib_handle_typec_insertion(chg);
	} else if (chg->typec_present &&
				chg->typec_mode == POWER_SUPPLY_TYPEC_NONE) {
		chg->typec_present = false;
		smblib_dbg(chg, PR_MISC, "TypeC removal\n");
		smblib_handle_typec_removal(chg);
	} else if (typec_chg) {
		icl_vote_ma = get_client_vote(chg->usb_icl_votable,
					      HEARTBEAT_VOTER) / 1000;
		if ((typec_mode == POWER_SUPPLY_TYPEC_SOURCE_MEDIUM) &&
		    (icl_vote_ma > 1500))
			vote(chg->usb_icl_votable, HEARTBEAT_VOTER,
			     true, 1500000);
		else if ((typec_mode ==  POWER_SUPPLY_TYPEC_SOURCE_DEFAULT) &&
			 (icl_vote_ma > 500))
			vote(chg->usb_icl_votable, HEARTBEAT_VOTER,
			     true, 500000);
		vote(chg->awake_votable, HEARTBEAT_VOTER, true, true);
		cancel_delayed_work(&chg->mmi.heartbeat_work);
		schedule_delayed_work(&chg->mmi.heartbeat_work,
				      msecs_to_jiffies(0));
	}
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: cc-state-change; Type-C %s detected\n",
				smblib_typec_mode_name[chg->typec_mode]);
}

void smblib_usb_typec_change(struct smb_charger *chg)
{
	int rc;

	rc = smblib_multibyte_read(chg, TYPE_C_STATUS_1_REG,
							chg->typec_status, 5);
	if (rc < 0) {
		smblib_err(chg, "Couldn't cache USB Type-C status rc=%d\n", rc);
		return;
	}

	smblib_handle_typec_cc_state_change(chg);

	if (chg->typec_status[3] & TYPEC_VBUS_ERROR_STATUS_BIT)
		smblib_dbg(chg, PR_INTERRUPT, "IRQ: vbus-error\n");

	if (chg->typec_status[3] & TYPEC_VCONN_OVERCURR_STATUS_BIT)
		schedule_work(&chg->vconn_oc_work);

	power_supply_changed(chg->usb_psy);
}

irqreturn_t smblib_handle_usb_typec_change(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->micro_usb_mode) {
		cancel_delayed_work_sync(&chg->uusb_otg_work);
		vote(chg->awake_votable, OTG_DELAY_VOTER, true, 0);
		smblib_dbg(chg, PR_INTERRUPT, "Scheduling OTG work\n");
		schedule_delayed_work(&chg->uusb_otg_work,
				msecs_to_jiffies(chg->otg_delay_ms));
		return IRQ_HANDLED;
	}

	if (chg->cc2_detach_wa_active || chg->typec_en_dis_active ||
					 chg->try_sink_active) {
		smblib_dbg(chg, PR_MISC | PR_INTERRUPT, "Ignoring since %s active\n",
			chg->cc2_detach_wa_active ?
			"cc2_detach_wa" : "typec_en_dis");
		return IRQ_HANDLED;
	}

	mutex_lock(&chg->lock);
	smblib_usb_typec_change(chg);
	mutex_unlock(&chg->lock);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_dc_plugin(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	power_supply_changed(chg->dc_psy);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_high_duty_cycle(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	chg->is_hdc = true;
	/*
	 * Disable usb IRQs after the flag set and re-enable IRQs after
	 * the flag cleared in the delayed work queue, to avoid any IRQ
	 * storming during the delays
	 */
	if (chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq)
		disable_irq_nosync(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);

	schedule_delayed_work(&chg->clear_hdc_work, msecs_to_jiffies(60));

	return IRQ_HANDLED;
}

static void smblib_bb_removal_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bb_removal_work.work);

	vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
	vote(chg->awake_votable, BOOST_BACK_VOTER, false, 0);
}

#define BOOST_BACK_UNVOTE_DELAY_MS		750
#define BOOST_BACK_STORM_COUNT			3
#define WEAK_CHG_STORM_COUNT			8
irqreturn_t smblib_handle_switcher_power_ok(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata = &irq_data->storm_data;
	int pok_irq = chg->irq_info[SWITCH_POWER_OK_IRQ].irq;
	int rc, usb_icl;
	u8 stat;

	if (!(chg->wa_flags & BOOST_BACK_WA))
		return IRQ_HANDLED;

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	/* skip suspending input if its already suspended by some other voter */
	usb_icl = get_effective_result(chg->usb_icl_votable);
	if ((stat & USE_USBIN_BIT) && usb_icl >= 0 && usb_icl < USBIN_25MA)
		return IRQ_HANDLED;

	if (stat & USE_DCIN_BIT)
		return IRQ_HANDLED;

	if (is_storming(&irq_data->storm_data)) {
		chg->reverse_boost = true;
		stat = 0;
		rc = smblib_read(chg, TYPE_C_STATUS_1_REG, &stat);
		if (rc < 0) {
			smblib_err(chg,
				   "Error getting USB Pres rc = %d\n", rc);
		} else if (stat) {
			smblib_err(chg, "USB Present, Disable Power OK IRQ\n");
			disable_irq_nosync(pok_irq);
			cancel_delayed_work(&chg->mmi.heartbeat_work);
			schedule_delayed_work(&chg->mmi.heartbeat_work,
					      msecs_to_jiffies(100));
			return IRQ_HANDLED;
		}
		/* This could be a weak charger reduce ICL */
		if (!is_client_vote_enabled(chg->usb_icl_votable,
						WEAK_CHARGER_VOTER)) {
			smblib_err(chg,
				"Weak charger detected: voting %dmA ICL\n",
				*chg->weak_chg_icl_ua / 1000);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					true, *chg->weak_chg_icl_ua);
			/*
			 * reset storm data and set the storm threshold
			 * to 3 for reverse boost detection.
			 */
			update_storm_count(wdata, BOOST_BACK_STORM_COUNT);
		} else {
			smblib_err(chg,
				"Reverse boost detected: voting 0mA to suspend input\n");
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, true, 0);
			vote(chg->awake_votable, BOOST_BACK_VOTER, true, 0);
			/*
			 * Remove the boost-back vote after a delay, to avoid
			 * permanently suspending the input if the boost-back
			 * condition is unintentionally hit.
			 */
			schedule_delayed_work(&chg->bb_removal_work,
				msecs_to_jiffies(BOOST_BACK_UNVOTE_DELAY_MS));
		}
	}

	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_wdog_bark(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblib_write(chg, BARK_BITE_WDOG_PET_REG, BARK_BITE_WDOG_PET_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't pet the dog rc=%d\n", rc);

	if (chg->step_chg_enabled || chg->sw_jeita_enabled)
		power_supply_changed(chg->batt_psy);

	return IRQ_HANDLED;
}

/**************
 * Additional USB PSY getters/setters
 * that call interrupt functions
***************/

int smblib_get_prop_pr_swap_in_progress(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->pr_swap_in_progress;
	return 0;
}

int smblib_set_prop_pr_swap_in_progress(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc;

	chg->pr_swap_in_progress = val->intval;
	/*
	 * call the cc changed irq to handle real removals while
	 * PR_SWAP was in progress
	 */
	smblib_usb_typec_change(chg);
	rc = smblib_masked_write(chg, MISC_CFG_REG, TCC_DEBOUNCE_20MS_BIT,
			val->intval ? TCC_DEBOUNCE_20MS_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set tCC debounce rc=%d\n", rc);
	return 0;
}

/***************
 * Work Queues *
 ***************/
#define MAX_INPUT_PWR_UW 18000000
static void smblib_pd_contract_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
					       pd_contract_work.work);
	int rc, max_ua;

	if (!chg->pd) {
		chg->pd = devm_usbpd_get_by_phandle(chg->dev,
						    "qcom,usbpd-phandle");
		if (IS_ERR_OR_NULL(chg->pd)) {
			pr_err("Error getting the pd phandle %ld\n",
				PTR_ERR(chg->pd));
			chg->pd = NULL;
		}
	}
	if (!chg->pd || !chg->pd_active)
		return;

	chg->pd_contract_uv = usbpd_select_pdo_match(chg->pd);

	if (chg->pd_contract_uv == -ENOTSUPP)
		return;

	if (chg->pd_contract_uv >= MICRO_9V)
		smblib_set_opt_freq_buck(chg, chg->chg_freq.freq_9V);
	else
		smblib_set_opt_freq_buck(chg, chg->chg_freq.freq_5V);

	max_ua = (MAX_INPUT_PWR_UW / (chg->pd_contract_uv / 1000)) * 1000;
	smblib_dbg(chg, PR_MISC, "smblib_pd_contract_work: %d uV, %d uA\n",
		   chg->pd_contract_uv, max_ua);

	rc = vote(chg->usb_icl_votable, ICL_LIMIT_VOTER, true, max_ua);
	if (rc < 0)
		smblib_err(chg, "Error setting %d uA rc=%d\n", max_ua, rc);

	if (chg->pd_contract_uv <= 0)
		schedule_delayed_work(&chg->pd_contract_work,
				      msecs_to_jiffies(100));
}

static void smblib_uusb_otg_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						uusb_otg_work.work);
	int rc;
	u8 stat;
	bool otg;

	rc = smblib_read(chg, TYPE_C_STATUS_3_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_3 rc=%d\n", rc);
		goto out;
	}

	otg = !!(stat & (U_USB_GND_NOVBUS_BIT | U_USB_GND_BIT));
	extcon_set_cable_state_(chg->extcon, EXTCON_USB_HOST, otg);
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_3 = 0x%02x OTG=%d\n",
			stat, otg);
	power_supply_changed(chg->usb_psy);

out:
	vote(chg->awake_votable, OTG_DELAY_VOTER, false, 0);
}


static void smblib_hvdcp_detect_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
					       hvdcp_detect_work.work);

	vote(chg->pd_disallowed_votable_indirect, HVDCP_TIMEOUT_VOTER,
				false, 0);
	power_supply_changed(chg->usb_psy);
}

static void bms_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bms_update_work);

	smblib_suspend_on_debug_battery(chg);

	if (chg->batt_psy)
		power_supply_changed(chg->batt_psy);
}

static void clear_hdc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						clear_hdc_work.work);

	chg->is_hdc = 0;
	if (chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq)
		enable_irq(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
}

static void rdstd_cc2_detach_work(struct work_struct *work)
{
	int rc;
	u8 stat4, stat5;
	struct smb_charger *chg = container_of(work, struct smb_charger,
						rdstd_cc2_detach_work);

	if (!chg->cc2_detach_wa_active)
		return;

	/*
	 * WA steps -
	 * 1. Enable both UFP and DFP, wait for 10ms.
	 * 2. Disable DFP, wait for 30ms.
	 * 3. Removal detected if both TYPEC_DEBOUNCE_DONE_STATUS
	 *    and TIMER_STAGE bits are gone, otherwise repeat all by
	 *    work rescheduling.
	 * Note, work will be cancelled when USB_PLUGIN rises.
	 */

	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 UFP_EN_CMD_BIT | DFP_EN_CMD_BIT,
				 UFP_EN_CMD_BIT | DFP_EN_CMD_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write TYPE_C_CTRL_REG rc=%d\n", rc);
		return;
	}

	usleep_range(10000, 11000);

	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				 UFP_EN_CMD_BIT | DFP_EN_CMD_BIT,
				 UFP_EN_CMD_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't write TYPE_C_CTRL_REG rc=%d\n", rc);
		return;
	}

	usleep_range(30000, 31000);

	rc = smblib_read(chg, TYPE_C_STATUS_4_REG, &stat4);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return;
	}

	rc = smblib_read(chg, TYPE_C_STATUS_5_REG, &stat5);
	if (rc < 0) {
		smblib_err(chg,
			"Couldn't read TYPE_C_STATUS_5_REG rc=%d\n", rc);
		return;
	}

	if ((stat4 & TYPEC_DEBOUNCE_DONE_STATUS_BIT)
			|| (stat5 & TIMER_STAGE_2_BIT)) {
		smblib_dbg(chg, PR_MISC, "rerunning DD=%d TS2BIT=%d\n",
				(int)(stat4 & TYPEC_DEBOUNCE_DONE_STATUS_BIT),
				(int)(stat5 & TIMER_STAGE_2_BIT));
		goto rerun;
	}

	smblib_dbg(chg, PR_MISC, "Bingo CC2 Removal detected\n");
	chg->cc2_detach_wa_active = false;
	rc = smblib_masked_write(chg, TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
						EXIT_SNK_BASED_ON_CC_BIT, 0);
	smblib_reg_block_restore(chg, cc2_detach_settings);
	mutex_lock(&chg->lock);
	smblib_usb_typec_change(chg);
	mutex_unlock(&chg->lock);
	return;

rerun:
	schedule_work(&chg->rdstd_cc2_detach_work);
}

static void smblib_otg_oc_exit(struct smb_charger *chg, bool success)
{
	int rc;

	chg->otg_attempts = 0;
	if (!success) {
		smblib_err(chg, "OTG soft start failed\n");
		chg->otg_en = false;
	}

	smblib_dbg(chg, PR_OTG, "enabling VBUS < 1V check\n");
	rc = smblib_masked_write(chg, OTG_CFG_REG,
					QUICKSTART_OTG_FASTROLESWAP_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't enable VBUS < 1V check rc=%d\n", rc);
}

#define MAX_OC_FALLING_TRIES 10
static void smblib_otg_oc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
								otg_oc_work);
	int rc, i;
	u8 stat;

	if (!chg->vbus_vreg || !chg->vbus_vreg->rdev)
		return;

	smblib_err(chg, "over-current detected on VBUS\n");
	mutex_lock(&chg->otg_oc_lock);
	if (!chg->otg_en)
		goto unlock;

	smblib_dbg(chg, PR_OTG, "disabling VBUS < 1V check\n");
	smblib_masked_write(chg, OTG_CFG_REG,
					QUICKSTART_OTG_FASTROLESWAP_BIT,
					QUICKSTART_OTG_FASTROLESWAP_BIT);

	/*
	 * If 500ms has passed and another over-current interrupt has not
	 * triggered then it is likely that the software based soft start was
	 * successful and the VBUS < 1V restriction should be re-enabled.
	 */
	schedule_delayed_work(&chg->otg_ss_done_work, msecs_to_jiffies(500));

	rc = _smblib_vbus_regulator_disable(chg->vbus_vreg->rdev);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable VBUS rc=%d\n", rc);
		goto unlock;
	}

	if (++chg->otg_attempts > OTG_MAX_ATTEMPTS) {
		cancel_delayed_work_sync(&chg->otg_ss_done_work);
		smblib_err(chg, "OTG failed to enable after %d attempts\n",
			   chg->otg_attempts - 1);
		smblib_otg_oc_exit(chg, false);
		goto unlock;
	}

	/*
	 * The real time status should go low within 10ms. Poll every 1-2ms to
	 * minimize the delay when re-enabling OTG.
	 */
	for (i = 0; i < MAX_OC_FALLING_TRIES; ++i) {
		usleep_range(1000, 2000);
		rc = smblib_read(chg, OTG_BASE + INT_RT_STS_OFFSET, &stat);
		if (rc >= 0 && !(stat & OTG_OVERCURRENT_RT_STS_BIT))
			break;
	}

	if (i >= MAX_OC_FALLING_TRIES) {
		cancel_delayed_work_sync(&chg->otg_ss_done_work);
		smblib_err(chg, "OTG OC did not fall after %dms\n",
						2 * MAX_OC_FALLING_TRIES);
		smblib_otg_oc_exit(chg, false);
		goto unlock;
	}

	smblib_dbg(chg, PR_OTG, "OTG OC fell after %dms\n", 2 * i + 1);
	rc = _smblib_vbus_regulator_enable(chg->vbus_vreg->rdev);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable VBUS rc=%d\n", rc);
		goto unlock;
	}

unlock:
	mutex_unlock(&chg->otg_oc_lock);
}

static void smblib_vconn_oc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
								vconn_oc_work);
	int rc, i;
	u8 stat;

	if (chg->micro_usb_mode)
		return;

	smblib_err(chg, "over-current detected on VCONN\n");
	if (!chg->vconn_vreg || !chg->vconn_vreg->rdev)
		return;

	mutex_lock(&chg->vconn_oc_lock);
	rc = _smblib_vconn_regulator_disable(chg->vconn_vreg->rdev);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable VCONN rc=%d\n", rc);
		goto unlock;
	}

	if (++chg->vconn_attempts > VCONN_MAX_ATTEMPTS) {
		smblib_err(chg, "VCONN failed to enable after %d attempts\n",
			   chg->otg_attempts - 1);
		chg->vconn_en = false;
		chg->vconn_attempts = 0;
		goto unlock;
	}

	/*
	 * The real time status should go low within 10ms. Poll every 1-2ms to
	 * minimize the delay when re-enabling OTG.
	 */
	for (i = 0; i < MAX_OC_FALLING_TRIES; ++i) {
		usleep_range(1000, 2000);
		rc = smblib_read(chg, TYPE_C_STATUS_4_REG, &stat);
		if (rc >= 0 && !(stat & TYPEC_VCONN_OVERCURR_STATUS_BIT))
			break;
	}

	if (i >= MAX_OC_FALLING_TRIES) {
		smblib_err(chg, "VCONN OC did not fall after %dms\n",
						2 * MAX_OC_FALLING_TRIES);
		chg->vconn_en = false;
		chg->vconn_attempts = 0;
		goto unlock;
	}

	smblib_dbg(chg, PR_OTG, "VCONN OC fell after %dms\n", 2 * i + 1);
	if (++chg->vconn_attempts > VCONN_MAX_ATTEMPTS) {
		smblib_err(chg, "VCONN failed to enable after %d attempts\n",
			   chg->vconn_attempts - 1);
		chg->vconn_en = false;
		goto unlock;
	}

	rc = _smblib_vconn_regulator_enable(chg->vconn_vreg->rdev);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable VCONN rc=%d\n", rc);
		goto unlock;
	}

unlock:
	mutex_unlock(&chg->vconn_oc_lock);
}

static void smblib_otg_ss_done_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							otg_ss_done_work.work);
	int rc;
	bool success = false;
	u8 stat;

	mutex_lock(&chg->otg_oc_lock);
	rc = smblib_read(chg, OTG_STATUS_REG, &stat);
	if (rc < 0)
		smblib_err(chg, "Couldn't read OTG status rc=%d\n", rc);
	else if (stat & BOOST_SOFTSTART_DONE_BIT)
		success = true;

	smblib_otg_oc_exit(chg, success);
	mutex_unlock(&chg->otg_oc_lock);
}

static void smblib_icl_change_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							icl_change_work.work);
	int rc, settled_ua;

	rc = smblib_get_charge_param(chg, &chg->param.icl_stat, &settled_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get ICL status rc=%d\n", rc);
		return;
	}

	power_supply_changed(chg->usb_main_psy);

	smblib_dbg(chg, PR_INTERRUPT, "icl_settled=%d\n", settled_ua);
}

static void smblib_pl_enable_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							pl_enable_work.work);

	smblib_dbg(chg, PR_PARALLEL, "timer expired, enabling parallel\n");
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);
}

static void smblib_legacy_detection_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
							legacy_detection_work);
	int rc;
	u8 stat;
	bool legacy, rp_high;

	mutex_lock(&chg->lock);
	chg->typec_en_dis_active = 1;
	smblib_dbg(chg, PR_MISC, "running legacy unknown workaround\n");
	rc = smblib_masked_write(chg,
				TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				TYPEC_DISABLE_CMD_BIT,
				TYPEC_DISABLE_CMD_BIT);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable type-c rc=%d\n", rc);

	/* wait for the adapter to turn off VBUS */
	msleep(1000);

	smblib_dbg(chg, PR_MISC, "legacy workaround enabling typec\n");

	rc = smblib_masked_write(chg,
				TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				TYPEC_DISABLE_CMD_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't enable type-c rc=%d\n", rc);

	/* wait for type-c detection to complete */
	msleep(400);

	rc = smblib_read(chg, TYPE_C_STATUS_5_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read typec stat5 rc = %d\n", rc);
		goto unlock;
	}

	chg->typec_legacy_valid = true;
	vote(chg->usb_icl_votable, LEGACY_UNKNOWN_VOTER, false, 0);
	legacy = stat & TYPEC_LEGACY_CABLE_STATUS_BIT;
	rp_high = chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_HIGH;
	smblib_dbg(chg, PR_MISC, "legacy workaround done legacy = %d rp_high = %d\n",
			legacy, rp_high);
	if (!legacy || !rp_high)
		vote(chg->hvdcp_disable_votable_indirect, VBUS_CC_SHORT_VOTER,
								false, 0);

unlock:
	chg->typec_en_dis_active = 0;
	smblib_usb_typec_change(chg);
	mutex_unlock(&chg->lock);
}

static int smblib_create_votables(struct smb_charger *chg)
{
	int rc = 0;

	chg->fcc_votable = find_votable("FCC");
	if (chg->fcc_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find FCC votable rc=%d\n", rc);
		return rc;
	}

	chg->fv_votable = find_votable("FV");
	if (chg->fv_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find FV votable rc=%d\n", rc);
		return rc;
	}

	chg->usb_icl_votable = find_votable("USB_ICL");
	if (!chg->usb_icl_votable) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find USB_ICL votable rc=%d\n", rc);
		return rc;
	}

	chg->pl_disable_votable = find_votable("PL_DISABLE");
	if (chg->pl_disable_votable == NULL) {
		rc = -EINVAL;
		smblib_err(chg, "Couldn't find votable PL_DISABLE rc=%d\n", rc);
		return rc;
	}

	chg->pl_enable_votable_indirect = find_votable("PL_ENABLE_INDIRECT");
	if (chg->pl_enable_votable_indirect == NULL) {
		rc = -EINVAL;
		smblib_err(chg,
			"Couldn't find votable PL_ENABLE_INDIRECT rc=%d\n",
			rc);
		return rc;
	}

	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);

	chg->dc_suspend_votable = create_votable("DC_SUSPEND", VOTE_SET_ANY,
					smblib_dc_suspend_vote_callback,
					chg);
	if (IS_ERR(chg->dc_suspend_votable)) {
		rc = PTR_ERR(chg->dc_suspend_votable);
		return rc;
	}

	chg->dc_icl_votable = create_votable("DC_ICL", VOTE_MIN,
					smblib_dc_icl_vote_callback,
					chg);
	if (IS_ERR(chg->dc_icl_votable)) {
		rc = PTR_ERR(chg->dc_icl_votable);
		return rc;
	}

	chg->pd_disallowed_votable_indirect
		= create_votable("PD_DISALLOWED_INDIRECT", VOTE_SET_ANY,
			smblib_pd_disallowed_votable_indirect_callback, chg);
	if (IS_ERR(chg->pd_disallowed_votable_indirect)) {
		rc = PTR_ERR(chg->pd_disallowed_votable_indirect);
		return rc;
	}

	chg->pd_allowed_votable = create_votable("PD_ALLOWED",
					VOTE_SET_ANY, NULL, NULL);
	if (IS_ERR(chg->pd_allowed_votable)) {
		rc = PTR_ERR(chg->pd_allowed_votable);
		return rc;
	}

	chg->awake_votable = create_votable("AWAKE", VOTE_SET_ANY,
					smblib_awake_vote_callback,
					chg);
	if (IS_ERR(chg->awake_votable)) {
		rc = PTR_ERR(chg->awake_votable);
		return rc;
	}

	chg->chg_disable_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					smblib_chg_disable_vote_callback,
					chg);
	if (IS_ERR(chg->chg_disable_votable)) {
		rc = PTR_ERR(chg->chg_disable_votable);
		return rc;
	}


	chg->hvdcp_disable_votable_indirect = create_votable(
				"HVDCP_DISABLE_INDIRECT",
				VOTE_SET_ANY,
				smblib_hvdcp_disable_indirect_vote_callback,
				chg);
	if (IS_ERR(chg->hvdcp_disable_votable_indirect)) {
		rc = PTR_ERR(chg->hvdcp_disable_votable_indirect);
		return rc;
	}

	chg->hvdcp_enable_votable = create_votable("HVDCP_ENABLE",
					VOTE_SET_ANY,
					smblib_hvdcp_enable_vote_callback,
					chg);
	if (IS_ERR(chg->hvdcp_enable_votable)) {
		rc = PTR_ERR(chg->hvdcp_enable_votable);
		return rc;
	}

	chg->apsd_disable_votable = create_votable("APSD_DISABLE",
					VOTE_SET_ANY,
					smblib_apsd_disable_vote_callback,
					chg);
	if (IS_ERR(chg->apsd_disable_votable)) {
		rc = PTR_ERR(chg->apsd_disable_votable);
		return rc;
	}

	chg->hvdcp_hw_inov_dis_votable = create_votable("HVDCP_HW_INOV_DIS",
					VOTE_SET_ANY,
					smblib_hvdcp_hw_inov_dis_vote_callback,
					chg);
	if (IS_ERR(chg->hvdcp_hw_inov_dis_votable)) {
		rc = PTR_ERR(chg->hvdcp_hw_inov_dis_votable);
		return rc;
	}

	chg->usb_irq_enable_votable = create_votable("USB_IRQ_DISABLE",
					VOTE_SET_ANY,
					smblib_usb_irq_enable_vote_callback,
					chg);
	if (IS_ERR(chg->usb_irq_enable_votable)) {
		rc = PTR_ERR(chg->usb_irq_enable_votable);
		return rc;
	}

	chg->typec_irq_disable_votable = create_votable("TYPEC_IRQ_DISABLE",
					VOTE_SET_ANY,
					smblib_typec_irq_disable_vote_callback,
					chg);
	if (IS_ERR(chg->typec_irq_disable_votable)) {
		rc = PTR_ERR(chg->typec_irq_disable_votable);
		return rc;
	}

	return rc;
}

static void smblib_destroy_votables(struct smb_charger *chg)
{
	if (chg->dc_suspend_votable)
		destroy_votable(chg->dc_suspend_votable);
	if (chg->usb_icl_votable)
		destroy_votable(chg->usb_icl_votable);
	if (chg->dc_icl_votable)
		destroy_votable(chg->dc_icl_votable);
	if (chg->pd_disallowed_votable_indirect)
		destroy_votable(chg->pd_disallowed_votable_indirect);
	if (chg->pd_allowed_votable)
		destroy_votable(chg->pd_allowed_votable);
	if (chg->awake_votable)
		destroy_votable(chg->awake_votable);
	if (chg->chg_disable_votable)
		destroy_votable(chg->chg_disable_votable);
	if (chg->apsd_disable_votable)
		destroy_votable(chg->apsd_disable_votable);
	if (chg->hvdcp_hw_inov_dis_votable)
		destroy_votable(chg->hvdcp_hw_inov_dis_votable);
	if (chg->typec_irq_disable_votable)
		destroy_votable(chg->typec_irq_disable_votable);
}

static void smblib_iio_deinit(struct smb_charger *chg)
{
	if (!IS_ERR_OR_NULL(chg->iio.temp_chan))
		iio_channel_release(chg->iio.temp_chan);
	if (!IS_ERR_OR_NULL(chg->iio.temp_max_chan))
		iio_channel_release(chg->iio.temp_max_chan);
	if (!IS_ERR_OR_NULL(chg->iio.usbin_i_chan))
		iio_channel_release(chg->iio.usbin_i_chan);
	if (!IS_ERR_OR_NULL(chg->iio.usbin_v_chan))
		iio_channel_release(chg->iio.usbin_v_chan);
	if (!IS_ERR_OR_NULL(chg->iio.batt_i_chan))
		iio_channel_release(chg->iio.batt_i_chan);
	if (!IS_ERR_OR_NULL(chg->iio.dcin_v_chan))
		iio_channel_release(chg->iio.dcin_v_chan);
}

int smblib_init(struct smb_charger *chg)
{
	int rc = 0;

	device_init_wakeup(chg->dev, true);
	mutex_init(&chg->lock);
	mutex_init(&chg->write_lock);
	mutex_init(&chg->otg_oc_lock);
	mutex_init(&chg->vconn_oc_lock);
	mutex_init(&chg->micnrs_oc_lock);
	INIT_WORK(&chg->bms_update_work, bms_update_work);
	INIT_WORK(&chg->rdstd_cc2_detach_work, rdstd_cc2_detach_work);
	INIT_DELAYED_WORK(&chg->hvdcp_detect_work, smblib_hvdcp_detect_work);
	INIT_DELAYED_WORK(&chg->clear_hdc_work, clear_hdc_work);
	INIT_WORK(&chg->otg_oc_work, smblib_otg_oc_work);
	INIT_WORK(&chg->vconn_oc_work, smblib_vconn_oc_work);
	INIT_DELAYED_WORK(&chg->otg_ss_done_work, smblib_otg_ss_done_work);
	INIT_DELAYED_WORK(&chg->icl_change_work, smblib_icl_change_work);
	INIT_DELAYED_WORK(&chg->pl_enable_work, smblib_pl_enable_work);
	INIT_DELAYED_WORK(&chg->pd_contract_work, smblib_pd_contract_work);
	INIT_WORK(&chg->legacy_detection_work, smblib_legacy_detection_work);
	INIT_DELAYED_WORK(&chg->uusb_otg_work, smblib_uusb_otg_work);
	INIT_DELAYED_WORK(&chg->bb_removal_work, smblib_bb_removal_work);
	chg->fake_capacity = -EINVAL;
	chg->fake_input_current_limited = -EINVAL;

	switch (chg->mode) {
	case PARALLEL_MASTER:
		rc = qcom_batt_init();
		if (rc < 0) {
			smblib_err(chg, "Couldn't init qcom_batt_init rc=%d\n",
				rc);
			return rc;
		}

		rc = qcom_step_chg_init(chg->step_chg_enabled,
						chg->sw_jeita_enabled);
		if (rc < 0) {
			smblib_err(chg, "Couldn't init qcom_step_chg_init rc=%d\n",
				rc);
			return rc;
		}

		rc = smblib_create_votables(chg);
		if (rc < 0) {
			smblib_err(chg, "Couldn't create votables rc=%d\n",
				rc);
			return rc;
		}

		rc = smblib_register_notifier(chg);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't register notifier rc=%d\n", rc);
			return rc;
		}

		chg->bms_psy = power_supply_get_by_name("bms");
		chg->pl.psy = power_supply_get_by_name("parallel");
		break;
	case PARALLEL_SLAVE:
		break;
	default:
		smblib_err(chg, "Unsupported mode %d\n", chg->mode);
		return -EINVAL;
	}

	mmi_init(chg);

	return rc;
}

int smblib_deinit(struct smb_charger *chg)
{
	mmi_deinit(chg);

	switch (chg->mode) {
	case PARALLEL_MASTER:
		cancel_work_sync(&chg->bms_update_work);
		cancel_work_sync(&chg->rdstd_cc2_detach_work);
		cancel_delayed_work_sync(&chg->hvdcp_detect_work);
		cancel_delayed_work_sync(&chg->clear_hdc_work);
		cancel_work_sync(&chg->otg_oc_work);
		cancel_work_sync(&chg->vconn_oc_work);
		cancel_delayed_work_sync(&chg->otg_ss_done_work);
		cancel_delayed_work_sync(&chg->icl_change_work);
		cancel_delayed_work_sync(&chg->pl_enable_work);
		cancel_work_sync(&chg->legacy_detection_work);
		cancel_delayed_work_sync(&chg->uusb_otg_work);
		cancel_delayed_work_sync(&chg->bb_removal_work);
		power_supply_unreg_notifier(&chg->nb);
		smblib_destroy_votables(chg);
		qcom_step_chg_deinit();
		qcom_batt_deinit();
		break;
	case PARALLEL_SLAVE:
		break;
	default:
		smblib_err(chg, "Unsupported mode %d\n", chg->mode);
		return -EINVAL;
	}

	smblib_iio_deinit(chg);

	return 0;
}

/*********************
 * MMI Functionality *
 *********************/

struct smb_charger *mmi_chip;

static char *stepchg_str[] = {
	[STEP_MAX]		= "MAX",
	[STEP_NORM]		= "NORMAL",
	[STEP_EB]		= "EXTERNAL BATT",
	[STEP_FULL]		= "FULL",
	[STEP_FLOAT]		= "FLOAT",
	[STEP_DEMO]		= "DEMO",
	[STEP_STOP]		= "STOP",
	[STEP_NONE]		= "NONE",
};

static char *ebchg_str[] = {
	[EB_DISCONN]		= "DISCONN",
	[EB_SINK]		= "SINK",
	[EB_SRC]		= "SOURCE",
	[EB_OFF]		= "OFF",
};

int smblib_enable_dc_aicl(struct smb_charger *chg, bool enable)
{
	int rc = 0;

	if (!chg)
		return rc;

	rc = smblib_masked_write(chg, DCIN_AICL_OPTIONS_CFG_REG,
				 DCIN_AICL_EN_BIT,
				 enable ? DCIN_AICL_EN_BIT : 0);

	return rc;

}

int smblib_get_prop_dc_voltage_now(struct smb_charger *chg,
				    union power_supply_propval *val)
{
	int rc = 0;

	rc = smblib_get_prop_dc_present(chg, val);
	if (rc < 0 || !val->intval)
		return rc;

	if (!chg->iio.dcin_v_chan ||
		PTR_ERR(chg->iio.dcin_v_chan) == -EPROBE_DEFER)
		chg->iio.dcin_v_chan = iio_channel_get(chg->dev, "dcin_v");

	if (IS_ERR(chg->iio.dcin_v_chan))
		return PTR_ERR(chg->iio.dcin_v_chan);

	return iio_read_channel_processed(chg->iio.dcin_v_chan, &val->intval);
}

#define MIN_TEMP_C -20
#define MAX_TEMP_C 60
#define MIN_MAX_TEMP_C 47
#define HYSTERISIS_DEGC 2
static bool mmi_find_temp_zone(struct smb_charger *chip, int temp_c)
{
	int prev_zone, num_zones;
	struct mmi_temp_zone *zones;
	int hotter_t, hotter_fcc;
	int colder_t, colder_fcc;
	int i;
	int max_temp;

	if (!chip) {
		smblib_dbg(chip, PR_MOTO, "called before chip valid!\n");
		return false;
	}

	zones = chip->mmi.temp_zones;
	num_zones = chip->mmi.num_temp_zones;
	prev_zone = chip->mmi.pres_temp_zone;

	if (chip->mmi.max_chrg_temp >= MIN_MAX_TEMP_C)
		max_temp = chip->mmi.max_chrg_temp;
	else
		max_temp = zones[num_zones - 1].temp_c;

	if (prev_zone == ZONE_NONE) {
		for (i = num_zones - 1; i >= 0; i--) {
			if (temp_c >= zones[i].temp_c) {
				if (i == num_zones - 1)
					chip->mmi.pres_temp_zone = ZONE_HOT;
				else
					chip->mmi.pres_temp_zone = i + 1;
				return true;
			}
		}
		chip->mmi.pres_temp_zone = ZONE_COLD;
		return true;
	}

	if (prev_zone == ZONE_COLD) {
		if (temp_c >= MIN_TEMP_C + HYSTERISIS_DEGC)
			chip->mmi.pres_temp_zone = ZONE_FIRST;
	} else if (prev_zone == ZONE_HOT) {
		if (temp_c <=  max_temp - HYSTERISIS_DEGC)
			chip->mmi.pres_temp_zone = num_zones - 1;
	} else {
		if (prev_zone == ZONE_FIRST) {
			hotter_fcc = zones[prev_zone + 1].fcc_max_ma;
			colder_fcc = 0;
			hotter_t = zones[prev_zone].temp_c;
			colder_t = MIN_TEMP_C;
		} else if (prev_zone == num_zones - 1) {
			hotter_fcc = 0;
			colder_fcc = zones[prev_zone - 1].fcc_max_ma;
			hotter_t = zones[prev_zone].temp_c;
			colder_t = zones[prev_zone - 1].temp_c;
		} else {
			hotter_fcc = zones[prev_zone + 1].fcc_max_ma;
			colder_fcc = zones[prev_zone - 1].fcc_max_ma;
			hotter_t = zones[prev_zone].temp_c;
			colder_t = zones[prev_zone - 1].temp_c;
		}

		if (zones[prev_zone].fcc_max_ma < hotter_fcc)
			hotter_t += HYSTERISIS_DEGC;

		if (zones[prev_zone].fcc_max_ma < colder_fcc)
			colder_t -= HYSTERISIS_DEGC;

		if (temp_c < MIN_TEMP_C)
			chip->mmi.pres_temp_zone = ZONE_COLD;
		else if (temp_c >= max_temp)
			chip->mmi.pres_temp_zone = ZONE_HOT;
		else if (temp_c >= hotter_t)
			chip->mmi.pres_temp_zone++;
		else if (temp_c < colder_t)
			chip->mmi.pres_temp_zone--;
	}

	if (prev_zone != chip->mmi.pres_temp_zone) {
		smblib_dbg(chip, PR_MOTO,
			   "Entered Temp Zone %d!\n",
			   chip->mmi.pres_temp_zone);
		return true;
	}

	return false;
}

#define TAPER_COUNT 2
static bool mmi_has_current_tapered(struct smb_charger *chip,
				    int batt_ma, int taper_ma)
{
	bool change_state = false;
	int allowed_fcc;

	if (!chip) {
		smblib_dbg(chip, PR_MOTO, "called before chip valid!\n");
		return false;
	}

	allowed_fcc = get_effective_result(chip->fcc_votable) / 1000;

	if (batt_ma < 0) {
		batt_ma *= -1;
		if ((batt_ma <= taper_ma) && (allowed_fcc >= taper_ma))
			if (chip->mmi.chrg_taper_cnt >= TAPER_COUNT) {
				change_state = true;
				chip->mmi.chrg_taper_cnt = 0;
			} else
				chip->mmi.chrg_taper_cnt++;
		else
			chip->mmi.chrg_taper_cnt = 0;
	} else {
		if (chip->mmi.chrg_taper_cnt >= TAPER_COUNT) {
			change_state = true;
			chip->mmi.chrg_taper_cnt = 0;
		} else
			chip->mmi.chrg_taper_cnt++;
	}

	return change_state;
}

static bool is_wls_present(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	const struct power_supply_desc *wls;

	if (chip->mmi.wls_psy && chip->mmi.wls_psy->desc)
		wls = chip->mmi.wls_psy->desc;
	else
		return false;

	if (!wls->get_property)
		return false;

	rc = wls->get_property(chip->mmi.wls_psy,
			       POWER_SUPPLY_PROP_PRESENT,
			       &ret);
	if (rc < 0) {
		smblib_err(chip, "Couldn't get wls status\n");
		return false;
	}

	return ret.intval ? true : false;
}

static bool is_usbeb_present(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	const struct power_supply_desc *usbeb;

	if (chip->mmi.usbeb_psy && chip->mmi.usbeb_psy->desc)
		usbeb = chip->mmi.usbeb_psy->desc;
	else
		return false;

	if (!usbeb->get_property)
		return false;

	rc = power_supply_get_property(chip->mmi.usbeb_psy,
				       POWER_SUPPLY_PROP_PRESENT,
				       &ret);
	if (rc < 0) {
		smblib_err(chip, "Couldn't get usbeb status\n");
		return false;
	}

	return ret.intval ? true : false;
}

static int get_eb_pwr_prop(struct smb_charger *chip,
			   enum power_supply_property prop)
{
	union power_supply_propval ret = {0, };
	int eb_prop;
	int rc;
	struct power_supply *eb_pwr_psy;

	eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);
	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->get_property)
		return -ENODEV;

	rc = power_supply_get_property(eb_pwr_psy, prop, &ret);
	if (rc) {
		smblib_dbg(chip, PR_MISC,
			   "eb pwr error reading Prop %d rc = %d\n", prop, rc);
		ret.intval = -EINVAL;
	}
	eb_prop = ret.intval;

	power_supply_put(eb_pwr_psy);

	return eb_prop;
}

static int get_eb_prop(struct smb_charger *chip,
		       enum power_supply_property prop)
{
	union power_supply_propval ret = {0, };
	int eb_prop;
	int rc;
	struct power_supply *eb_batt_psy;

	eb_batt_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_batt_psy_name);
	if (!eb_batt_psy || !eb_batt_psy->desc ||
	    !eb_batt_psy->desc->get_property)
		return -ENODEV;

	rc = power_supply_get_property(eb_batt_psy, prop, &ret);
	if (rc) {
		smblib_dbg(chip, PR_MISC,
			   "eb batt error reading Prop %d rc = %d\n",
			   prop, rc);
		ret.intval = -EINVAL;
	}
	eb_prop = ret.intval;

	power_supply_put(eb_batt_psy);

	return eb_prop;
}

#define EB_RCV_NEVER BIT(7)
#define EB_SND_EXT BIT(2)
#define EB_SND_LOW BIT(1)
#define EB_SND_NEVER BIT(0)
static void mmi_check_extbat_ability(struct smb_charger *chip, char *able)
{
	int rc;
	const struct power_supply_desc *eb_pwr_d;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy;

	if (!able)
		return;

	eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);

	*able = 0;

	if (!eb_pwr_psy || !eb_pwr_psy->desc) {
		*able |= (EB_RCV_NEVER | EB_SND_NEVER);
		return;
	}

	eb_pwr_d = eb_pwr_psy->desc;
	rc = eb_pwr_d->get_property(eb_pwr_psy,
				    POWER_SUPPLY_PROP_PTP_INTERNAL_RECEIVE,
				    &ret);
	if (rc) {
		smblib_err(chip,
			   "Could not read Receive Params rc = %d\n", rc);
		*able |= EB_RCV_NEVER;
	} else if ((ret.intval == POWER_SUPPLY_PTP_INT_RCV_NEVER) ||
		   (ret.intval == POWER_SUPPLY_PTP_INT_RCV_UNKNOWN))
		*able |= EB_RCV_NEVER;

	rc = eb_pwr_d->get_property(eb_pwr_psy,
				    POWER_SUPPLY_PROP_PTP_POWER_REQUIRED,
				    &ret);
	if (rc) {
		smblib_err(chip,
			   "Could not read Power Required rc = %d\n", rc);
		*able |= EB_RCV_NEVER;
	} else if ((ret.intval == POWER_SUPPLY_PTP_POWER_NOT_REQUIRED) ||
		   (ret.intval == POWER_SUPPLY_PTP_POWER_REQUIREMENTS_UNKNOWN))
		*able |= EB_RCV_NEVER;

	rc = eb_pwr_d->get_property(eb_pwr_psy,
				    POWER_SUPPLY_PROP_PTP_POWER_AVAILABLE,
				    &ret);
	if (rc) {
		smblib_err(chip,
			   "Could not read Power Available rc = %d\n", rc);
		*able |= EB_SND_NEVER;
	} else if ((ret.intval == POWER_SUPPLY_PTP_POWER_NOT_AVAILABLE) ||
		   (ret.intval == POWER_SUPPLY_PTP_POWER_AVAILABILITY_UNKNOWN))
		*able |= EB_SND_NEVER;
	else if (ret.intval == POWER_SUPPLY_PTP_POWER_AVAILABLE_INTERNAL) {
		rc = eb_pwr_d->get_property(eb_pwr_psy,
					    POWER_SUPPLY_PROP_PTP_INTERNAL_SEND,
					    &ret);
		if (rc) {
			smblib_err(chip,
				   "Could not read Send Params rc = %d\n", rc);
			*able |= EB_SND_NEVER;
		} else if ((ret.intval == POWER_SUPPLY_PTP_INT_SND_NEVER) ||
			   (ret.intval == POWER_SUPPLY_PTP_INT_SND_UNKNOWN))
			*able |= EB_SND_NEVER;
		else if (ret.intval == POWER_SUPPLY_PTP_INT_SND_LOW_BATT_SAVER)
			*able |= EB_SND_LOW;
	} else if (ret.intval == POWER_SUPPLY_PTP_POWER_AVAILABLE_EXTERNAL)
		*able |= EB_SND_EXT;

	power_supply_put(eb_pwr_psy);
}

static void mmi_set_extbat_in_cl(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->set_property)
		return;

	ret.intval = chip->mmi.cl_ebchg * 1000;

	smblib_dbg(chip, PR_MISC, "Set EB In Current %d uA\n", ret.intval);
	rc = power_supply_set_property(eb_pwr_psy,
				       POWER_SUPPLY_PROP_PTP_MAX_INPUT_CURRENT,
				       &ret);
	if (rc)
		smblib_err(chip, "Failed to set EB Input Current %d mA",
		       ret.intval / 1000);

	power_supply_put(eb_pwr_psy);
}

static void mmi_get_extbat_out_cl(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);
	int prev_cl_ebsrc = chip->mmi.cl_ebsrc;
	bool ebsrc_max, wls_src_max;

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->get_property) {
		chip->mmi.cl_ebsrc = 0;
		return;
	}

	if (chip->mmi.turbo_pwr_ebsrc == TURBO_EBSRC_UNKNOWN) {
		rc = power_supply_get_property(eb_pwr_psy,
					 POWER_SUPPLY_PROP_PTP_POWER_SOURCE,
					      &ret);

		switch (ret.intval) {
		case POWER_SUPPLY_PTP_POWER_SOURCE_NONE_TURBO:
		case POWER_SUPPLY_PTP_POWER_SOURCE_BATTERY_TURBO:
		case POWER_SUPPLY_PTP_POWER_SOURCE_WIRED_TURBO:
		case POWER_SUPPLY_PTP_POWER_SOURCE_WIRELESS_TURBO:
			chip->mmi.turbo_pwr_ebsrc = TURBO_EBSRC_VALID;
			break;
		default:
			chip->mmi.turbo_pwr_ebsrc = TURBO_EBSRC_NOT_SUPPORTED;
			break;
		}
	}

	rc = power_supply_get_property(eb_pwr_psy,
				       POWER_SUPPLY_PROP_PTP_MAX_OUTPUT_CURRENT,
				       &ret);
	if (rc)
		smblib_err(chip, "Failed to get EB Output Current");
	else {
		smblib_dbg(chip, PR_MISC,
			   "Get EB Out Current %d uA\n", ret.intval);
		ret.intval /= 1000;

		wls_src_max = (is_wls_present(chip) &&
			       (chip->mmi.vo_ebsrc > (MICRO_5V / 1000)));
		ebsrc_max =
			((chip->mmi.turbo_pwr_ebsrc == TURBO_EBSRC_VALID) &&
			 ((eb_attach_stop_soc == 100) ||
			  is_wls_present(chip)));

		if (ret.intval == 0)
			chip->mmi.cl_ebsrc = 0;
		else if ((ret.intval < chip->mmi.dc_ebmax_current_ma) ||
			 ebsrc_max ||
			 wls_src_max ||
			 is_usbeb_present(chip))
			chip->mmi.cl_ebsrc = ret.intval;
		else
			chip->mmi.cl_ebsrc = chip->mmi.dc_ebmax_current_ma;
	}

	if (prev_cl_ebsrc != chip->mmi.cl_ebsrc)
		smblib_dbg(chip, PR_MISC, "cl_ebsrc %d mA, retval %d mA\n",
			 chip->mmi.cl_ebsrc, ret.intval);

	power_supply_put(eb_pwr_psy);
}

static void mmi_get_extbat_in_vl(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);
	int prev_vi_ebsrc = chip->mmi.vi_ebsrc;

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->get_property) {
		chip->mmi.vi_ebsrc = 0;
		return;
	}

	rc = power_supply_get_property(eb_pwr_psy,
				       POWER_SUPPLY_PROP_PTP_MAX_INPUT_VOLTAGE,
				       &ret);
	if (rc) {
		smblib_err(chip, "Failed to get EB Input Voltage");
		chip->mmi.vi_ebsrc = MICRO_5V;
	} else {
		smblib_dbg(chip, PR_MISC,
			   "Get EB In Voltage %d uV\n", ret.intval);
		chip->mmi.vi_ebsrc = ret.intval;
	}

	if (prev_vi_ebsrc != chip->mmi.vi_ebsrc)
		smblib_dbg(chip, PR_MISC, "vi_ebsrc %d uV, retval %d uV\n",
			 chip->mmi.vi_ebsrc, ret.intval);

	power_supply_put(eb_pwr_psy);
}

#define USBIN_TOLER 500000
static void mmi_set_extbat_in_volt(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->set_property)
		return;

	rc = smblib_get_prop_usb_voltage_now(chip, &ret);
	if (rc < 0)
		ret.intval = MICRO_5V;
	else if (ret.intval >= (MICRO_9V - USBIN_TOLER))
		ret.intval = MICRO_9V;
	else
		ret.intval = MICRO_5V;

	smblib_dbg(chip, PR_MISC, "Set EB In Voltage %d uV\n", ret.intval);
	rc = power_supply_set_property(eb_pwr_psy,
				       POWER_SUPPLY_PROP_PTP_INPUT_VOLTAGE,
				       &ret);
	if (rc)
		smblib_err(chip, "Failed to set EB Input Voltage %d mV",
		       ret.intval / 1000);

	power_supply_put(eb_pwr_psy);
}

static void mmi_get_extbat_out_volt(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);
	int prev_vo_ebsrc = chip->mmi.vo_ebsrc;

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->get_property) {
		chip->mmi.vo_ebsrc = 0;
		return;
	}

	rc = power_supply_get_property(eb_pwr_psy,
				       POWER_SUPPLY_PROP_PTP_OUTPUT_VOLTAGE,
				       &ret);
	if (rc)
		smblib_err(chip, "Failed to get EB Output Voltage");
	else {
		smblib_dbg(chip, PR_MISC, "Get EB Out Voltage %d uV\n",
			   ret.intval);
		ret.intval /= 1000;
		chip->mmi.vo_ebsrc = ret.intval;
	}

	if (prev_vo_ebsrc != chip->mmi.vo_ebsrc)
		smblib_dbg(chip, PR_MISC, "vo_ebsrc %d mV, retval %d mV\n",
			 chip->mmi.vo_ebsrc, ret.intval);

	power_supply_put(eb_pwr_psy);
}

static void mmi_set_extbat_out_vl(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->set_property)
		return;

	ret.intval = chip->mmi.vl_ebsrc;

	smblib_dbg(chip, PR_MISC, "Set EB Out Voltage %d uV\n", ret.intval);
	rc = power_supply_set_property(eb_pwr_psy,
				      POWER_SUPPLY_PROP_PTP_MAX_OUTPUT_VOLTAGE,
				      &ret);
	if (rc)
		smblib_err(chip, "Failed to set EB Output Voltage %d mV",
		       ret.intval / 1000);

	power_supply_put(eb_pwr_psy);
}

static int mmi_get_extbat_state(struct smb_charger *chip, int *state)
{
	int rc;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->get_property) {
		*state = EB_DISCONN;
		chip->mmi.turbo_pwr_ebsrc = TURBO_EBSRC_UNKNOWN;
		return 0;
	}

	rc = power_supply_get_property(eb_pwr_psy,
				       POWER_SUPPLY_PROP_PTP_CURRENT_FLOW,
				       &ret);
	power_supply_put(eb_pwr_psy);

	if (rc) {
		smblib_dbg(chip, PR_MISC, "Failed to get EB State");
		return -EINVAL;
	}

	smblib_dbg(chip, PR_MISC, "Get EB State %d\n", ret.intval);
	switch (ret.intval) {
	case POWER_SUPPLY_PTP_CURRENT_FROM_PHONE:
		*state = EB_SINK;
		break;
	case POWER_SUPPLY_PTP_CURRENT_TO_PHONE:
		*state = EB_SRC;
		break;
	case POWER_SUPPLY_PTP_CURRENT_OFF:
	default:
		*state = EB_OFF;
		break;
	}

	return 0;
}

#define MAX_PINCTRL_TRIES 5
static void mmi_set_extbat_state(struct smb_charger *chip,
				 enum ebchg_state state,
				 bool force)
{
	int rc = 0;
	char ability = 0;
	int i = 0;
	int dcin_mv = 0;
	int dcin_high_cnt = 0;
	static unsigned char mmi_pinctrl_tries;
	union power_supply_propval ret = {0, };
	struct power_supply *eb_pwr_psy =
		power_supply_get_by_name((char *)chip->mmi.eb_pwr_psy_name);

	if (mmi_pinctrl_tries < MAX_PINCTRL_TRIES) {
		chip->mmi.smb_pinctrl =
			pinctrl_get_select(chip->dev, "eb_active");
		if (!chip->mmi.smb_pinctrl)
			smblib_err(chip,
				   "Could not get/set pinctrl state active\n");
		chip->mmi.smb_pinctrl = pinctrl_get_select_default(chip->dev);
		if (!chip->mmi.smb_pinctrl)
			smblib_err(chip, "Could not get/set pinctrl state\n");

		mmi_pinctrl_tries++;
	}

	if ((state == chip->mmi.ebchg_state) && !force) {
		if (eb_pwr_psy)
			goto set_eb_done;
		else
			return;
	}

	if (!eb_pwr_psy || !eb_pwr_psy->desc ||
	    !eb_pwr_psy->desc->get_property) {
		/* Remove USB Suspend Vote */
		vote(chip->usb_icl_votable, EB_VOTER,
		     false, 0);
		vote(chip->dc_suspend_votable, EB_VOTER,
		     true, 1);

		if (gpio_is_valid(chip->mmi.ebchg_gpio.gpio))
			gpio_set_value(chip->mmi.ebchg_gpio.gpio, 0);
		chip->mmi.cl_ebsrc = 0;

		ret.intval = MICRO_9V;
		rc = smblib_set_prop_pd_voltage_max(chip, &ret);
		if (rc < 0)
			smblib_err(chip,
				   "Couldn't set 9V USBC Voltage rc=%d\n", rc);
		chip->mmi.turbo_pwr_ebsrc = TURBO_EBSRC_UNKNOWN;
		chip->mmi.ebchg_state = EB_DISCONN;
		return;
	}

	mmi_check_extbat_ability(chip, &ability);

	if ((state == EB_SINK) && (ability & EB_RCV_NEVER) && !force) {
		smblib_err(chip,
			   "Setting Sink State not Allowed on RVC Never EB\n");
		goto set_eb_done;
	}

	if ((state == EB_SRC) && (ability & EB_SND_NEVER) &&
	    !chip->mmi.usbeb_present && !force) {
		smblib_err(chip,
			   "Setting SRC State not Allowed on SND Never EB\n");
		goto set_eb_done;
	}

	smblib_dbg(chip, PR_MOTO,
		   "EB State is %d setting %d\n",
		   chip->mmi.ebchg_state, state);

	switch (state) {
	case EB_SINK:
		mmi_get_extbat_in_vl(chip);

		if (chip->mmi.vi_ebsrc < MICRO_9V) {
			ret.intval = MICRO_5V;
			rc = smblib_set_prop_pd_voltage_max(chip, &ret);
			if (rc < 0)
				smblib_err(chip,
					"Couldn't set 5V USBC Voltage rc=%d\n",
					rc);
			msleep(500);
			rc = smblib_get_prop_usb_voltage_now(chip, &ret);
			if (rc < 0) {
				smblib_err(chip,
					"Couldn't get 5V USBC Voltage rc=%d\n",
					rc);
				goto set_eb_done;
			}
			if (ret.intval > (MICRO_5V + USBIN_TOLER)) {
				smblib_err(chip,
				       "Voltage too high for EB_SINK %d uV\n",
				       ret.intval);
				goto set_eb_done;
			}
		}

		mmi_set_extbat_in_volt(chip);

		ret.intval = POWER_SUPPLY_PTP_CURRENT_FROM_PHONE;
		rc = power_supply_set_property(eb_pwr_psy,
					       POWER_SUPPLY_PROP_PTP_CURRENT_FLOW,
					       &ret);
		if (!rc) {
			chip->mmi.ebchg_state = state;

			vote(chip->usb_icl_votable, EB_VOTER,
			     false, 0);
			vote(chip->dc_suspend_votable, EB_VOTER,
			     true, 1);
			gpio_set_value(chip->mmi.ebchg_gpio.gpio, 1);
		}
		break;
	case EB_SRC:
		if (!(ability & EB_SND_EXT) && (eb_attach_stop_soc != 100))
			chip->mmi.vl_ebsrc = MICRO_5V;
		else
			chip->mmi.vl_ebsrc = MICRO_9V;

		mmi_set_extbat_out_vl(chip);
		ret.intval = POWER_SUPPLY_PTP_CURRENT_TO_PHONE;
		rc = power_supply_set_property(eb_pwr_psy,
					       POWER_SUPPLY_PROP_PTP_CURRENT_FLOW,
					       &ret);
		if (!rc) {
			for (i = 0; i < 100; i++) {
				rc = smblib_get_prop_dc_voltage_now(chip,
								    &ret);
				if (rc == -EINVAL)
					break;
				dcin_mv = ret.intval / 1000;

				if (dcin_mv > 4500)
					dcin_high_cnt++;
				else
					dcin_high_cnt = 0;

				if (dcin_high_cnt > 10) {
					/* Total required delay is 200 msec
					 * for power mux transition timing.
					 */
					msleep(100);
					break;
				}
				msleep(20);
			}

			chip->mmi.ebchg_state = state;
			vote(chip->usb_icl_votable, EB_VOTER,
			     true, 0);
			vote(chip->dc_suspend_votable, EB_VOTER,
			     false, 1);
			gpio_set_value(chip->mmi.ebchg_gpio.gpio, 0);

			ret.intval = MICRO_9V;
			rc = smblib_set_prop_pd_voltage_max(chip, &ret);
			if (rc < 0)
				smblib_err(chip,
					"Couldn't set 9V USBC Voltage rc=%d\n",
					rc);
		}
		break;
	case EB_OFF:
		ret.intval = POWER_SUPPLY_PTP_CURRENT_OFF;
		rc = power_supply_set_property(eb_pwr_psy,
					    POWER_SUPPLY_PROP_PTP_CURRENT_FLOW,
					    &ret);
		if (!rc) {
			chip->mmi.ebchg_state = state;
			gpio_set_value(chip->mmi.ebchg_gpio.gpio, 0);
			vote(chip->usb_icl_votable, EB_VOTER,
			     false, 0);
			vote(chip->dc_suspend_votable, EB_VOTER,
			     true, 1);

			ret.intval = MICRO_9V;
			rc = smblib_set_prop_pd_voltage_max(chip, &ret);
			if (rc < 0)
				smblib_err(chip,
					"Couldn't set 9V USBC Voltage rc=%d\n",
					rc);
		}

		break;
	default:
		break;
	}

set_eb_done:
	power_supply_put(eb_pwr_psy);
}

#define WEAK_CHRG_THRSH 450
#define TURBO_CHRG_THRSH 2500
void mmi_chrg_rate_check(struct smb_charger *chip)
{
	int rc;
	union power_supply_propval val;
	struct mmi_params *mmi = &chip->mmi;
	int prev_chg_rate = mmi->charger_rate;
	char *charge_rate[] = {
		"None", "Normal", "Weak", "Turbo"
	};

	rc = smblib_get_prop_usb_present(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting USB Present rc = %d\n", rc);
		return;
	} else if (!val.intval) {
		if ((mmi->ebchg_state != EB_DISCONN) &&
		    (mmi->turbo_pwr_ebsrc == TURBO_EBSRC_VALID))
			mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_TURBO;
		else if (is_usbeb_present(chip)) {
			if (mmi->cl_ebsrc >= TURBO_CHRG_THRSH)
				mmi->charger_rate =
					POWER_SUPPLY_CHARGE_RATE_TURBO;
			else
				mmi->charger_rate =
					POWER_SUPPLY_CHARGE_RATE_NORMAL;
		} else if (is_wls_present(chip))
			mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_NORMAL;
		else
			mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;

		goto end_rate_check;
	}

	if (mmi->charger_rate == POWER_SUPPLY_CHARGE_RATE_TURBO)
		goto end_rate_check;

	val.intval = get_client_vote(chip->usb_icl_votable, PD_VOTER);
	if ((val.intval / 1000) >= TURBO_CHRG_THRSH) {
		mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_TURBO;
		goto end_rate_check;
	}

	if (chip->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_HIGH ||
		 mmi->hvdcp3_con) {
		mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_TURBO;
		goto end_rate_check;
	}

	rc = smblib_get_prop_input_current_limited(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting AICL Limited rc = %d\n", rc);
	} else if (val.intval) {
		rc = smblib_get_prop_input_current_settled(chip, &val);
		if (rc < 0) {
			smblib_err(chip,
				   "Error getting AICL Settled rc = %d\n", rc);
		} else if (val.intval < WEAK_CHRG_THRSH) {
			mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_WEAK;
			goto end_rate_check;
		}
	}

	mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_NORMAL;

end_rate_check:
	if (prev_chg_rate != mmi->charger_rate)
		smblib_err(chip, "%s Charger Detected\n",
		       charge_rate[mmi->charger_rate]);

}

static int mmi_psy_notifier_call(struct notifier_block *nb, unsigned long val,
				 void *v)
{
	struct smb_charger *chip = container_of(nb,
				struct smb_charger, mmi.mmi_psy_notifier);
	struct power_supply *psy = v;

	if (!chip) {
		smblib_dbg(chip, PR_MOTO, "called before chip valid!\n");
		return NOTIFY_DONE;
	}

	if ((val == PSY_EVENT_PROP_ADDED) ||
	    (val == PSY_EVENT_PROP_REMOVED)) {
		smblib_dbg(chip, PR_MISC, "PSY Added/Removed run HB!\n");
		cancel_delayed_work(&chip->mmi.heartbeat_work);
		schedule_delayed_work(&chip->mmi.heartbeat_work,
				      msecs_to_jiffies(0));
		return NOTIFY_OK;
	}

	if (val != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (psy &&
	    (strcmp(psy->desc->name,
		    (char *)chip->mmi.eb_pwr_psy_name) == 0)) {
		smblib_dbg(chip, PR_MISC, "PSY changed on PTP\n");
		cancel_delayed_work(&chip->mmi.heartbeat_work);
		schedule_delayed_work(&chip->mmi.heartbeat_work,
				      msecs_to_jiffies(100));
		return NOTIFY_OK;
	}

	return NOTIFY_OK;
}

void update_charging_limit_modes(struct smb_charger *chip, int batt_soc)
{
	enum charging_limit_modes charging_limit_modes;

	charging_limit_modes = chip->mmi.charging_limit_modes;
	if ((charging_limit_modes != CHARGING_LIMIT_RUN)
	    && (batt_soc >= chip->mmi.upper_limit_capacity))
		charging_limit_modes = CHARGING_LIMIT_RUN;
	else if ((charging_limit_modes != CHARGING_LIMIT_OFF)
		   && (batt_soc <= chip->mmi.lower_limit_capacity))
		charging_limit_modes = CHARGING_LIMIT_OFF;

	if (charging_limit_modes != chip->mmi.charging_limit_modes)
		chip->mmi.charging_limit_modes = charging_limit_modes;
}

#define CHARGER_DETECTION_DONE 7
#define SMBCHG_HEARTBEAT_INTERVAL_NS	70000000000
#define HEARTBEAT_DELAY_MS 60000
#define HEARTBEAT_HOLDOFF_MS 10000
#define HEARTBEAT_EB_WAIT_MS 1000
#define HYST_STEP_MV 50
#define EB_SPLIT_MA 500
#define DEMO_MODE_HYS_SOC 5
#define DEMO_MODE_VOLTAGE 4000
#define VBUS_INPUT_VOLTAGE_TARGET 5200
#define VBUS_INPUT_VOLTAGE_NOM ((VBUS_INPUT_VOLTAGE_TARGET) - 200)
#define VBUS_INPUT_VOLTAGE_MAX ((VBUS_INPUT_VOLTAGE_TARGET) + 200)
#define VBUS_INPUT_VOLTAGE_MIN 4000
#define VBUS_INPUT_MAX_COUNT 4
#define WARM_TEMP 45
#define COOL_TEMP 0
#define REV_BST_THRESH 4700
#define REV_BST_DROP 150
static void mmi_heartbeat_work(struct work_struct *work)
{
	struct smb_charger *chip = container_of(work,
						struct smb_charger,
						mmi.heartbeat_work.work);
	int rc;
	int batt_mv;
	int batt_ma;
	int batt_soc;
	int eb_soc;
	int batt_temp;
	int usb_mv;
	int dcin_mv;
	u8 apsd_reg;
	bool icl_override;
	static int vbus_inc_mv = VBUS_INPUT_VOLTAGE_TARGET;
	bool vbus_inc_now = false;
	int vbus_present = 0;
	int charger_present = 0;
	int cl_usb = -EINVAL;
	int cl_pd = -EINVAL;
	int cl_cc = -EINVAL;
	bool prev_usbeb_pres = chip->mmi.usbeb_present;

	int prev_step;
	char eb_able = 0;
	int hb_resch_time;
	int ebparams_cnt = chip->mmi.update_eb_params;
	bool eb_chrg_allowed;

	int eb_state = 0;

	int pwr_ext;
	bool eb_sink_to_off = false;

	union power_supply_propval val;
	struct mmi_params *mmi = &chip->mmi;
	struct mmi_temp_zone *zone;
	int max_fv_mv;
	int target_usb;
	int target_dc;
	int target_fcc;
	int target_fv;
	int prev_vl_ebsrc = chip->mmi.vl_ebsrc;
	int pok_irq;
	static int prev_vbus_mv = -1;

	if (!atomic_read(&chip->mmi.hb_ready))
		return;

	/* Have not been resumed so wait another 100 ms */
	if (chip->suspended) {
		smblib_err(chip, "SMB HB running before Resume\n");
		schedule_delayed_work(&mmi->heartbeat_work,
				      msecs_to_jiffies(100));
		return;
	}

	vote(chip->awake_votable, HEARTBEAT_VOTER, true, true);

	alarm_try_to_cancel(&chip->mmi.heartbeat_alarm);

	/* Collect External Battery Information */
	eb_soc = get_eb_prop(chip, POWER_SUPPLY_PROP_CAPACITY);
	pwr_ext = get_eb_pwr_prop(chip, POWER_SUPPLY_PROP_PTP_EXTERNAL);
	mmi_check_extbat_ability(chip, &eb_able);

	if (eb_soc == -EINVAL)
		eb_soc = 0;

	if ((eb_soc == -ENODEV) && (pwr_ext == -ENODEV))
		mmi_set_extbat_state(chip, EB_DISCONN, false);
	else if (mmi->ebchg_state == EB_DISCONN)
		mmi_set_extbat_state(chip, EB_OFF, false);
	else {
		rc = mmi_get_extbat_state(chip, &eb_state);
		if ((rc >= 0) && (mmi->ebchg_state != eb_state))
			mmi_set_extbat_state(chip, EB_OFF, true);
	}

	if (pwr_ext == POWER_SUPPLY_PTP_EXT_SUPPORTED) {
		mmi->usbeb_present = is_usbeb_present(chip);
		mmi->wls_present = is_wls_present(chip);
	} else {
		mmi->usbeb_present = false;
		mmi->wls_present = false;
	}
	/* Hold Wakelock if External Battery has External Power */
	if ((mmi->wls_present || mmi->usbeb_present) &&
	    (mmi->ebchg_state == EB_SRC))
		vote(chip->awake_votable, WIRELESS_VOTER, true, 0);
	else
		vote(chip->awake_votable, WIRELESS_VOTER, false, 0);

	if ((chip->mmi.ebchg_state == EB_SRC) && chip->mmi.check_ebsrc_vl) {
		if (!(eb_able & EB_SND_EXT) && (eb_attach_stop_soc != 100))
			chip->mmi.vl_ebsrc = MICRO_5V;
		else
			chip->mmi.vl_ebsrc = MICRO_9V;

		if (prev_vl_ebsrc != chip->mmi.vl_ebsrc)
			mmi_set_extbat_out_vl(chip);
	}
	chip->mmi.check_ebsrc_vl = false;

	/* Collect Current Information */
	rc = smblib_get_prop_usb_present(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting USB Present rc = %d\n", rc);
		goto end_hb;
	} else
		vbus_present = val.intval;

	if (!vbus_present) {
		vbus_inc_mv = VBUS_INPUT_VOLTAGE_TARGET;
		charger_present = 0;
		mmi->charger_debounce_cnt = 0;
		if (mmi->apsd_done &&
		    chip->typec_mode == POWER_SUPPLY_TYPEC_NONE)
			smblib_handle_typec_removal(chip);
	} else if (mmi->charger_debounce_cnt < CHARGER_DETECTION_DONE) {
		mmi->charger_debounce_cnt++;
		/* Set the USB CL to 500 only if pd is not active to avoid
		 * PD Compliance issues */
		if (!chip->pd_active)
			cl_usb = 500;
		if ((chip->typec_mode == POWER_SUPPLY_TYPEC_NONE) ||
		    (chip->typec_mode ==
		     POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) ||
		    (chip->pd_active && (chip->pd_contract_uv > 0))) {
			mmi->charger_debounce_cnt = CHARGER_DETECTION_DONE;
			charger_present = 1;
			mmi->apsd_done = true;
		}
	} else if (mmi->charger_debounce_cnt == CHARGER_DETECTION_DONE)
		charger_present = 1;

	if (vbus_present || mmi->wls_present || !mmi->usbeb_present)
		smblib_enable_dc_aicl(chip, false);
	else if (mmi->usbeb_present && !prev_usbeb_pres)
		smblib_enable_dc_aicl(chip, true);

	rc = smblib_get_prop_batt_voltage_now(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting Batt Voltage rc = %d\n", rc);
		goto end_hb;
	} else
		batt_mv = val.intval / 1000;

	rc = smblib_get_prop_batt_current_now(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting Batt Current rc = %d\n", rc);
		goto end_hb;
	} else
		batt_ma = val.intval / 1000;

	rc = smblib_get_prop_batt_capacity(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting Batt Capacity rc = %d\n", rc);
		goto end_hb;
	} else
		batt_soc = val.intval;

	rc = smblib_get_prop_batt_temp(chip, &val);
	if (rc < 0) {
		smblib_err(chip,
			   "Error getting Batt Temperature rc = %d\n", rc);
		goto end_hb;
	} else
		batt_temp = val.intval / 10;

	rc = smblib_get_prop_usb_voltage_now(chip, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting USB Voltage rc = %d\n", rc);
		goto end_hb;
	} else
		usb_mv = val.intval / 1000;

	if (prev_vbus_mv == -1)
		prev_vbus_mv = usb_mv;

	rc = smblib_read(chip, APSD_RESULT_STATUS_REG, &apsd_reg);
	if (rc < 0) {
		smblib_err(chip, "Couldn't read APSD_RESULT_STATUS rc=%d\n",
			   rc);
		icl_override = false;
		apsd_reg = 0;
	} else {
		icl_override = !!(apsd_reg & ICL_OVERRIDE_LATCH_BIT);
		apsd_reg &= APSD_RESULT_STATUS_MASK;
	}

	smblib_dbg(chip, PR_MISC, "batt=%d mV, %d mA, %d C, USB= %d mV\n",
		 batt_mv, batt_ma, batt_temp, usb_mv);

	pok_irq = chip->irq_info[SWITCH_POWER_OK_IRQ].irq;
	if (chip->reverse_boost) {
		if ((usb_mv < REV_BST_THRESH) &&
		    ((prev_vbus_mv - REV_BST_DROP) > usb_mv)) {
			smblib_err(chip,
				   "Reverse Boosted: Clear, USB Suspend\n");
			chip->reverse_boost = false;
			vote(chip->usb_icl_votable, BOOST_BACK_VOTER,
			     true, 0);
			msleep(50);
			vote(chip->usb_icl_votable, BOOST_BACK_VOTER,
			     false, 0);
			enable_irq(pok_irq);
		} else {
			smblib_err(chip,
				   "Reverse Boosted: USB %d mV PUSB %d mV\n",
				   usb_mv, prev_vbus_mv);
		}
	}

	prev_vbus_mv = usb_mv;

	if (charger_present) {
		val.intval = get_client_vote(chip->usb_icl_votable,
					     USB_PSY_VOTER);
		cl_usb = val.intval / 1000;

		val.intval = get_client_vote(chip->usb_icl_votable,
					     PD_VOTER);
		cl_pd = val.intval / 1000;

		switch (chip->typec_mode) {
		case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
			if (mmi->hvdcp3_con)
				cl_cc = 3000;
			else
				cl_cc = 500;
			break;
		case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
			cl_cc = 1500;
			break;
		case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
			cl_cc = 3000;
			break;
		case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
			cl_cc = 500;
			break;
		case POWER_SUPPLY_TYPEC_NONE:
			if (vbus_present)
				cl_cc = 500;
			if (vbus_present &&
			    !icl_override &&
			    (apsd_reg == 0)) {
				rc = smblib_masked_write(chip, CMD_APSD_REG,
							 ICL_OVERRIDE_BIT,
							 ICL_OVERRIDE_BIT);
				if (rc < 0)
					smblib_err(chip,
						   "Fail ICL Over rc%d\n", rc);
			}
		default:
			cl_cc = 0;
			break;
		}

		if (cl_pd > 0)
			cl_usb = cl_pd;
		else if ((cl_cc > 500) ||
			 (chip->typec_mode ==
			  POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER))
			cl_usb = cl_cc;
		else if (chip->real_charger_type ==
			 POWER_SUPPLY_TYPE_USB_DCP)
			cl_usb = 1500;
		else if (chip->real_charger_type ==
			 POWER_SUPPLY_TYPE_USB_CDP)
			cl_usb = 1500;
		else if (cl_cc == 500)
			cl_usb = cl_cc;
		else if (cl_usb <= 0)
			cl_usb = 500;

		if (cl_usb >= TURBO_CHRG_THRSH)
			mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_TURBO;
	}


	/* Determine External Battery Next State */
	eb_chrg_allowed = (!(eb_able & EB_RCV_NEVER) &&
			   !mmi->usbeb_present &&
			   !mmi->wls_present);

	mmi_get_extbat_out_volt(chip);
	mmi_get_extbat_out_cl(chip);

	if (mmi->enable_charging_limit && mmi->is_factory_image)
		update_charging_limit_modes(chip, batt_soc);

	if (!vbus_present) {
		switch (mmi->ebchg_state) {
		case EB_SRC:
			if (mmi->wls_present || mmi->usbeb_present) {
				mmi->eb_rechrg = false;
				if (batt_soc == 100)
					mmi_set_extbat_state(chip, EB_OFF,
								false);
			} else if ((eb_able & EB_SND_NEVER) ||
				   (eb_on_sw == 0)) {
				mmi_set_extbat_state(chip, EB_OFF, false);
				mmi->eb_rechrg = true;
			}

			break;
		case EB_SINK:
			mmi_set_extbat_state(chip, EB_OFF, false);
			eb_sink_to_off = true;
			break;
		case EB_OFF:
			if (mmi->wls_present || mmi->usbeb_present) {
				mmi->eb_rechrg = false;
				if (batt_soc < 100)
					mmi_set_extbat_state(chip, EB_SRC,
							     false);
			} else if ((eb_able & EB_SND_NEVER) ||
				   (eb_able & EB_SND_EXT) ||
				   (eb_on_sw == 0))
				mmi_set_extbat_state(chip, EB_OFF, false);
			else if (eb_able & EB_SND_LOW) {
				if (batt_soc <= eb_low_start_soc)
					mmi_set_extbat_state(chip, EB_SRC,
								false);
				else
					mmi_set_extbat_state(chip, EB_OFF,
								false);
			} else
				mmi_set_extbat_state(chip, EB_SRC, false);

			if ((mmi->ebchg_state == EB_SRC) && (batt_soc < 75))
				mmi->eb_rechrg = false;

			break;
		case EB_DISCONN:
		default:
			break;
		}
	} else if (vbus_present ||
		   mmi->wls_present ||
		   mmi->usbeb_present) {
		mmi->eb_rechrg = false;
	}

	if (mmi->eb_rechrg)
		target_dc = mmi->dc_eff_current_ma;
	else if (mmi->cl_ebsrc)
		target_dc = mmi->cl_ebsrc;
	else
		target_dc = mmi->dc_ebmax_current_ma;

	mmi_find_temp_zone(chip, batt_temp);
	zone = &mmi->temp_zones[mmi->pres_temp_zone];

	if (mmi->base_fv_mv == 0) {
		mmi->base_fv_mv = get_client_vote(chip->fv_votable,
						  BATT_PROFILE_VOTER) / 1000;
		vote(chip->fv_votable,
		     BATT_PROFILE_VOTER, false, 0);
	}
	max_fv_mv = mmi->base_fv_mv;

	rc = smblib_get_prop_dc_voltage_now(chip, &val);
	if (rc == -EINVAL) {
		dcin_mv = 0;
		smblib_err(chip, "Failed to get DCIN\n");
	} else
		dcin_mv = val.intval / 1000;

	/* Determine Next State */
	prev_step = chip->mmi.pres_chrg_step;

	if (mmi->charging_limit_modes == CHARGING_LIMIT_RUN)
		pr_warn("Factory Mode/Image so Limiting Charging!!!\n");

	if (!charger_present && (mmi->ebchg_state != EB_SRC)) {
		mmi->pres_chrg_step = STEP_NONE;
	} else if ((mmi->pres_temp_zone == ZONE_HOT) ||
		   (mmi->pres_temp_zone == ZONE_COLD) ||
		   (mmi->charging_limit_modes == CHARGING_LIMIT_RUN)) {
		chip->mmi.pres_chrg_step = STEP_STOP;
	} else if (mmi->force_eb_chrg && !(eb_able & EB_RCV_NEVER) &&
		   (mmi->ebchg_state != EB_DISCONN)) {
		mmi->pres_chrg_step = STEP_EB;
	} else if (mmi->demo_mode) {
		bool voltage_full;
		static int demo_full_soc = 100;
		int usb_suspend = get_client_vote(chip->usb_icl_votable,
					      DEMO_VOTER);
		if (usb_suspend == -EINVAL)
			usb_suspend = 0;
		else if(usb_suspend == 0)
			usb_suspend = 1;

		mmi->pres_chrg_step = STEP_DEMO;
		smblib_dbg(chip, PR_MOTO,
			   "Battery in Demo Mode charging Limited %dper\n",
			   mmi->demo_mode);

		voltage_full = ((usb_suspend == 0) &&
			((batt_mv + HYST_STEP_MV) >= DEMO_MODE_VOLTAGE) &&
			mmi_has_current_tapered(chip, batt_ma,
						mmi->chrg_iterm));

		if ((usb_suspend == 0) &&
		    ((batt_soc >= mmi->demo_mode) ||
		     voltage_full)) {
			demo_full_soc = batt_soc;
			vote(chip->usb_icl_votable, DEMO_VOTER, true, 0);
			vote(chip->dc_suspend_votable, DEMO_VOTER, true, 0);
			usb_suspend = 1;
		} else if (usb_suspend &&
			   (batt_soc <=
			    (demo_full_soc - DEMO_MODE_HYS_SOC))) {
			if (chip->mmi.ebchg_state == EB_SINK) {
				mmi_set_extbat_state(chip, EB_OFF, false);
				mmi->cl_ebchg = 0;
				mmi_set_extbat_in_cl(chip);
			}	
			vote(chip->usb_icl_votable, DEMO_VOTER, false, 0);
			vote(chip->dc_suspend_votable, DEMO_VOTER, false, 0);
			usb_suspend = 0;
			mmi->chrg_taper_cnt = 0;
		}

		if (vbus_present &&
		    ((eb_soc >= mmi->demo_mode) ||
		     (eb_able & EB_RCV_NEVER))) {
			mmi_set_extbat_state(chip, EB_OFF, false);
			mmi->cl_ebchg = 0;
			mmi_set_extbat_in_cl(chip);
		} else if (vbus_present && usb_suspend &&
			   (eb_soc <=
			    (mmi->demo_mode - DEMO_MODE_HYS_SOC))) {
			mmi->cl_ebchg = cl_usb;
			mmi_set_extbat_in_cl(chip);
			mmi_set_extbat_state(chip, EB_SINK, false);
		}
	} else if ((mmi->pres_chrg_step == STEP_NONE) ||
		   (mmi->pres_chrg_step == STEP_STOP)) {
		if (zone->norm_mv && (batt_mv >= zone->norm_mv)) {
			if (zone->fcc_norm_ma)
				mmi->pres_chrg_step = STEP_NORM;
			else
				mmi->pres_chrg_step = STEP_STOP;
		} else
			mmi->pres_chrg_step = STEP_MAX;
	} else if (mmi->pres_chrg_step == STEP_MAX) {
		if (!zone->norm_mv) {
			/* No Step in this Zone */
			mmi->chrg_taper_cnt = 0;
			if ((batt_mv + HYST_STEP_MV) >= max_fv_mv)
				mmi->pres_chrg_step = STEP_NORM;
			else
				mmi->pres_chrg_step = STEP_MAX;
		} else if ((batt_mv + HYST_STEP_MV) < zone->norm_mv) {
			mmi->chrg_taper_cnt = 0;
			mmi->pres_chrg_step = STEP_MAX;
		} else if (!zone->fcc_norm_ma)
			mmi->pres_chrg_step = STEP_FLOAT;
		else if (mmi_has_current_tapered(chip, batt_ma,
						 zone->fcc_norm_ma)) {
			mmi->chrg_taper_cnt = 0;
			if (smblib_charge_halted(chip)) {
				vote(chip->chg_disable_votable,
				     HEARTBEAT_VOTER, true, 0);
				smblib_err(chip, "Charge Halt..Toggle\n");
				msleep(50);
			}
			mmi->pres_chrg_step = STEP_NORM;
		}
	} else if (mmi->pres_chrg_step == STEP_NORM) {
		if (!zone->fcc_norm_ma)
			mmi->pres_chrg_step = STEP_STOP;
		else if ((batt_soc < 100) ||
			 (batt_mv + HYST_STEP_MV) < max_fv_mv) {
			mmi->chrg_taper_cnt = 0;
			mmi->pres_chrg_step = STEP_NORM;
		} else if (mmi_has_current_tapered(chip, batt_ma,
						   mmi->chrg_iterm)) {
			if (eb_chrg_allowed)
				mmi->pres_chrg_step = STEP_EB;
			else
				mmi->pres_chrg_step = STEP_FULL;
		}
	} else if (mmi->pres_chrg_step == STEP_EB) {
		if ((batt_soc < 95) || !eb_chrg_allowed) {
			mmi->chrg_taper_cnt = 0;
			mmi->pres_chrg_step = STEP_NORM;
		}
	} else if (mmi->pres_chrg_step == STEP_FULL) {
		if (batt_soc <= 99) {
			mmi->chrg_taper_cnt = 0;
			mmi->pres_chrg_step = STEP_NORM;
		} else if (eb_chrg_allowed) {
			mmi->pres_chrg_step = STEP_EB;
		}
	} else if (mmi->pres_chrg_step == STEP_FLOAT) {
		if ((zone->fcc_norm_ma) ||
		    ((batt_mv + HYST_STEP_MV) < zone->norm_mv))
			mmi->pres_chrg_step = STEP_MAX;

	}

	/* Take State actions */
	switch (mmi->pres_chrg_step) {
	case STEP_EB:
		target_fv = max_fv_mv;
		if (batt_soc < 100)
			target_fcc = zone->fcc_norm_ma;
		else
			target_fcc = -EINVAL;

		if (cl_usb > 1500) {
			mmi->cl_ebchg = cl_usb - EB_SPLIT_MA;
			target_usb = EB_SPLIT_MA;
		} else {
			mmi->cl_ebchg = cl_usb;
			target_usb = -EINVAL;
		}

		break;
	case STEP_FLOAT:
	case STEP_MAX:
		if (!zone->norm_mv)
			target_fv = max_fv_mv;
		else
			target_fv = zone->norm_mv;
		target_fcc = zone->fcc_max_ma;
		mmi->cl_ebchg = 0;

		if (((cl_usb - EB_SPLIT_MA) >= target_fcc) && eb_chrg_allowed)
			mmi->cl_ebchg = EB_SPLIT_MA;

		target_usb = cl_usb - mmi->cl_ebchg;
		break;
	case STEP_FULL:
		target_fv = max_fv_mv;
		target_fcc = -EINVAL;
		target_usb = cl_usb;
		break;
	case STEP_NORM:
		target_fv = max_fv_mv + mmi->vfloat_comp_mv;
		target_fcc = zone->fcc_norm_ma;
		mmi->cl_ebchg = 0;

		if (((cl_usb - EB_SPLIT_MA) >= target_fcc) && eb_chrg_allowed)
			mmi->cl_ebchg = EB_SPLIT_MA;

		target_usb = cl_usb - mmi->cl_ebchg;
		break;
	case STEP_NONE:
		target_fv = max_fv_mv;
		target_fcc = zone->fcc_norm_ma;
		target_usb = cl_usb;
		break;
	case STEP_STOP:
		target_fv = max_fv_mv;
		target_fcc = -EINVAL;
		target_usb = cl_usb;
		break;
	case STEP_DEMO:
		target_fv = DEMO_MODE_VOLTAGE;
		target_fcc = zone->fcc_max_ma;
		target_usb = cl_usb - mmi->cl_ebchg;
		break;
	default:
		break;
	}

	/* Votes for State */
	vote(chip->fv_votable, HEARTBEAT_VOTER, true, target_fv * 1000);

	vote(chip->chg_disable_votable, HEARTBEAT_VOTER,
	     (target_fcc < 0), 0);

	vote(chip->fcc_votable, HEARTBEAT_VOTER,
	     true, (target_fcc >= 0) ? (target_fcc * 1000) : 0);

	rc = vote(chip->usb_icl_votable, HEARTBEAT_VOTER,
		  true, (target_usb >= 0) ? (target_usb * 1000) : 0);
	if (rc < 0) {
		smblib_err(chip, "Problem setting USB ICL %d\n", target_usb);
		goto end_hb;
	}
	if (vbus_present) {
		if (mmi->cl_ebchg > 0) {
			mmi_set_extbat_in_cl(chip);
			mmi_set_extbat_state(chip, EB_SINK, false);
		} else if (mmi->ebchg_state != EB_DISCONN)
			mmi_set_extbat_state(chip, EB_OFF, false);
	}

	if ((target_dc > 1600) &&
	    (chip->mmi.vl_ebsrc >= MICRO_9V) &&
	    (dcin_mv > ((MICRO_5V / 1000) + 500)))
		target_dc = 1600;

	vote(chip->dc_icl_votable, HEARTBEAT_VOTER,
	     true, (target_dc >= 0) ? (target_dc * 1000) : 0);

	if (charger_present && mmi->hvdcp3_con && !chip->pd_active &&
	    (vbus_inc_mv > VBUS_INPUT_VOLTAGE_NOM)) {
		if ((usb_mv < vbus_inc_mv) &&
		    (usb_mv >= VBUS_INPUT_VOLTAGE_MIN) &&
		    (mmi->vbus_inc_cnt < VBUS_INPUT_MAX_COUNT)) {
			smblib_dbg(chip, PR_MOTO,
				   "HVDCP Input %d mV Low, Increase\n",
				   usb_mv);
			smblib_dp_pulse(chip);
			vbus_inc_now = true;
			mmi->vbus_inc_cnt++;
		} else if (usb_mv > VBUS_INPUT_VOLTAGE_MAX) {
			smblib_dbg(chip, PR_MOTO,
				   "HVDCP Input %d mV High force 5V\n",
				   usb_mv);
			vbus_inc_mv -= 50;
			smblib_write(chip, CMD_HVDCP_2_REG,
				     FORCE_5V_BIT);
			vbus_inc_now = true;
			mmi->vbus_inc_cnt = 0;
		}

	}

	if (chip->mmi.pres_temp_zone == ZONE_HOT) {
		chip->mmi.batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (chip->mmi.pres_temp_zone == ZONE_COLD) {
		chip->mmi.batt_health = POWER_SUPPLY_HEALTH_COLD;
	} else if (batt_temp >= WARM_TEMP) {
		if (chip->mmi.pres_chrg_step == STEP_STOP)
			chip->mmi.batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			chip->mmi.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (batt_temp <= COOL_TEMP) {
		if (chip->mmi.pres_chrg_step == STEP_STOP)
			chip->mmi.batt_health = POWER_SUPPLY_HEALTH_COLD;
		else
			chip->mmi.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	} else
		chip->mmi.batt_health = POWER_SUPPLY_HEALTH_GOOD;

	smblib_dbg(chip, PR_MOTO,
		"EB Input %d mA, PMI Input %d mA, USBC CL %d mA, DC %d mA\n",
		   mmi->cl_ebchg, target_usb, cl_cc,
		   target_dc);
	smblib_dbg(chip, PR_MOTO, "Step State = %s, EB State %s\n",
		   stepchg_str[(int)mmi->pres_chrg_step],
		   ebchg_str[(int)mmi->ebchg_state]);
	smblib_dbg(chip, PR_MOTO,
		   "EFFECTIVE: FV = %d, CDIS = %d, FCC = %d, "
		   "USBICL = %d, DCICL = %d\n",
		   get_effective_result(chip->fv_votable),
		   get_effective_result(chip->chg_disable_votable),
		   get_effective_result(chip->fcc_votable),
		   get_effective_result(chip->usb_icl_votable),
		   get_effective_result(chip->dc_icl_votable));
end_hb:
	if (chip->batt_psy)
		power_supply_changed(chip->batt_psy);

	hb_resch_time = HEARTBEAT_HOLDOFF_MS;

	if (!chip->mmi.chrg_taper_cnt && (rc >= 0))
		hb_resch_time = HEARTBEAT_DELAY_MS;

	if (ebparams_cnt != mmi->update_eb_params)
		hb_resch_time = HEARTBEAT_EB_MS;
	else
		mmi->update_eb_params = 0;

	if (eb_sink_to_off ||
	    (!charger_present && mmi->charger_debounce_cnt) ||
	    vbus_inc_now)
		hb_resch_time = HEARTBEAT_EB_WAIT_MS;

	schedule_delayed_work(&mmi->heartbeat_work,
			      msecs_to_jiffies(hb_resch_time));

	if (mmi->ebchg_state == EB_SRC) {
		alarm_start_relative(&mmi->heartbeat_alarm,
				ns_to_ktime(SMBCHG_HEARTBEAT_INTERVAL_NS));
	}

	__pm_relax(&mmi->smblib_mmi_hb_wake_source);
	if (!vbus_present)
		vote(chip->awake_votable, HEARTBEAT_VOTER, false, 0);
}

static int smbchg_reboot(struct notifier_block *nb,
			 unsigned long event, void *unused)
{
	struct smb_charger *chg = container_of(nb, struct smb_charger,
						mmi.smb_reboot);
	union power_supply_propval val;
	int rc;
	char eb_able;
	int usb_present = -EINVAL, batt_soc = -EINVAL;
	int soc_max = 99;

	smblib_dbg(chg, PR_MISC, "SMB Reboot\n");
	if (!chg) {
		smblib_dbg(chg, PR_MOTO, "called before chip valid!\n");
		return NOTIFY_DONE;
	}

	mmi_check_extbat_ability(chg, &eb_able);
	if (eb_able & EB_SND_NEVER)
		soc_max = 0;
	else if (eb_able & EB_SND_LOW)
		soc_max = eb_low_start_soc;

	atomic_set(&chg->mmi.hb_ready, 0);
	cancel_delayed_work_sync(&chg->mmi.heartbeat_work);

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc >= 0)
		usb_present = val.intval;

	val.intval = 0;
	rc = smblib_get_prop_batt_capacity(chg, &val);
	if (rc >= 0)
		batt_soc = val.intval;

	if (chg->mmi.factory_mode) {
		switch (event) {
		case SYS_POWER_OFF:
			/* Disable Factory Kill */
			factory_kill_disable = true;
			/* Disable Charging */
			smblib_masked_write(chg, CHARGING_ENABLE_CMD_REG,
					    CHARGING_ENABLE_CMD_BIT,
					    0);

			/* Suspend USB and DC */
			smblib_set_usb_suspend(chg, true);
			smblib_set_dc_suspend(chg, true);

			rc = smblib_get_prop_usb_present(chg, &val);
			while (rc >= 0 && val.intval) {
				msleep(100);
				rc = smblib_get_prop_usb_present(chg, &val);
				smblib_dbg(chg, PR_MOTO,
					   "Wait for VBUS to decay\n");
			}

			smblib_dbg(chg, PR_MOTO, "VBUS UV wait 1 sec!\n");
			/* Delay 1 sec to allow more VBUS decay */
			msleep(1000);
			/* configure power role for UFP */
			smblib_masked_write(chg,
				TYPE_C_INTRPT_ENB_SOFTWARE_CTRL_REG,
				TYPEC_POWER_ROLE_CMD_MASK, UFP_EN_CMD_BIT);
			break;
		default:
			break;
		}
	} else if ((batt_soc >= soc_max)  ||
		   (get_eb_prop(chg, POWER_SUPPLY_PROP_CAPACITY) <= 0)) {
		/* Turn off any Ext batt charging */
		smblib_err(chg, "Attempt to Shutdown EB!\n");
		mmi_set_extbat_state(chg, EB_OFF, false);
		if (gpio_is_valid(chg->mmi.ebchg_gpio.gpio)) {
			gpio_set_value(chg->mmi.ebchg_gpio.gpio, 0);
			gpio_free(chg->mmi.ebchg_gpio.gpio);
		}
	} else if ((chg->mmi.ebchg_state == EB_OFF) && (usb_present == 0)) {
		smblib_err(chg, "Attempt to Turn EB ON!\n");
		mmi_set_extbat_state(chg, EB_SRC, false);
	}

	return NOTIFY_DONE;
}

static int usr_rst_sw_disable;
module_param(usr_rst_sw_disable, int, 0644);
static void warn_irq_w(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						mmi.warn_irq_work.work);

	int warn_line = gpio_get_value(chg->mmi.warn_gpio.gpio);
	u8 stat;
	bool vbus_rising;

	if (!warn_line) {
		smblib_dbg(chg, PR_MOTO, "HW User Reset! 2 sec to Reset\n");

		/* trigger wdog if resin key pressed */
		if (qpnp_pon_key_status & QPNP_PON_KEY_RESIN_BIT) {
			smblib_dbg(chg, PR_MOTO,
				   "User triggered watchdog reset\n");
			msm_trigger_wdog_bite();
			return;
		}

		vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
		if (usr_rst_sw_disable <= 0) {
			/* Configure hardware reset before halt
			 * The new KUNGKOW circuit will not disconnect the
			 * battery if usb/dc is connected. But because the
			 * kernel is halted, a watchdog reset will be reported
			 * instead of hardware reset. In this case, we need to
			 * clear the KUNPOW reset bit to let BL detect it as a
			 * hardware reset.
			 * A pmic hard reset is necessary to report the powerup
			 * reason to BL correctly.
			*/
			qpnp_pon_store_extra_reset_info(RESET_EXTRA_RESET_KUNPOW_REASON, 0);
			qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
			/*
			 * pre-empt a battery pull by rebooting if a usb
			 * charger is present. otherwise halt the kernel
			 */
			if (vbus_rising)
				kernel_restart(NULL);
			else
				kernel_halt();
		} else
			smblib_dbg(chg, PR_MOTO, "SW HALT Disabled!\n");
		return;
	}
}

#define WARN_IRQ_DELAY  5 /* 5msec */
static irqreturn_t warn_irq_handler(int irq, void *_chip)
{
	struct smb_charger *chg = _chip;

	/*schedule delayed work for 5msec for line state to settle*/
	schedule_delayed_work(&chg->mmi.warn_irq_work,
				msecs_to_jiffies(WARN_IRQ_DELAY));

	return IRQ_HANDLED;
}

#define CHG_SHOW_MAX_SIZE 50
static ssize_t factory_image_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		pr_err("Invalid factory image mode value = %lu\n", mode);
		return -EINVAL;
	}

	if (!mmi_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	mmi_chip->mmi.is_factory_image = (mode) ? true : false;

	return r ? r : count;
}

static ssize_t factory_image_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!mmi_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	state = (mmi_chip->mmi.is_factory_image) ? 1 : 0;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(factory_image_mode, 0644,
		factory_image_mode_show,
		factory_image_mode_store);

static ssize_t force_demo_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		smblib_err(mmi_chip, "Invalid demo  mode value = %lu\n", mode);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}
	mmi_chip->mmi.chrg_taper_cnt = 0;

	if ((mode >= 35) && (mode <= 80))
		mmi_chip->mmi.demo_mode = mode;
	else
		mmi_chip->mmi.demo_mode = 35;

	return r ? r : count;
}

static ssize_t force_demo_mode_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	state = mmi_chip->mmi.demo_mode;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_demo_mode, 0644,
		force_demo_mode_show,
		force_demo_mode_store);

static ssize_t force_max_chrg_temp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		smblib_err(mmi_chip, "Invalid max temp value = %lu\n", mode);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	if ((mode >= MIN_MAX_TEMP_C) && (mode <= MAX_TEMP_C))
		mmi_chip->mmi.max_chrg_temp = mode;
	else
		mmi_chip->mmi.max_chrg_temp = MAX_TEMP_C;

	return r ? r : count;
}

static ssize_t force_max_chrg_temp_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	state = mmi_chip->mmi.max_chrg_temp;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_max_chrg_temp, 0644,
		force_max_chrg_temp_show,
		force_max_chrg_temp_store);

static ssize_t force_chg_usb_suspend_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long mode;

	r = kstrtoul(buf, 0, &mode);
	if (r) {
		smblib_err(mmi_chip,
			   "Invalid usb suspend mode value = %lu\n", mode);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}
	r = smblib_set_usb_suspend(mmi_chip, (bool)mode);

	return r ? r : count;
}

static ssize_t force_chg_usb_suspend_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int state;
	int ret;

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}
	ret = smblib_get_usb_suspend(mmi_chip, &state);
	if (ret) {
		smblib_err(mmi_chip,
			   "USBIN_SUSPEND_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

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
		smblib_err(mmi_chip,
			   "Invalid chg fail mode value = %lu\n", mode);
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
		smblib_err(mmi_chip,
			   "Invalid chrg enable value = %lu\n", mode);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	r = smblib_masked_write(mmi_chip, CHARGING_ENABLE_CMD_REG,
				CHARGING_ENABLE_CMD_BIT,
				mode ? CHARGING_ENABLE_CMD_BIT : 0);
	if (r < 0) {
		smblib_err(mmi_chip, "Factory Couldn't %s charging rc=%d\n",
			   mode ? "enable" : "disable", (int)r);
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

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smblib_read(mmi_chip, CHARGING_ENABLE_CMD_REG, &value);
	if (ret) {
		smblib_err(mmi_chip, "CHG_EN_BIT failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	state = (CHARGING_ENABLE_CMD_BIT & value) ? 1 : 0;
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_auto_enable, 0664,
		force_chg_auto_enable_show,
		force_chg_auto_enable_store);

static ssize_t force_chg_ibatt_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		smblib_err(mmi_chip,
			   "Invalid ibatt value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	chg_current *= 1000; /* Convert to uA */
	r = smblib_set_charge_param(mmi_chip, &mmi_chip->param.fcc, chg_current);
	if (r < 0) {
		smblib_err(mmi_chip,
			   "Factory Couldn't set masterfcc = %d rc=%d\n",
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

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smblib_get_charge_param(mmi_chip, &mmi_chip->param.fcc, &state);
	if (ret < 0) {
		smblib_err(mmi_chip,
			   "Factory Couldn't get master fcc rc=%d\n",
			   (int)ret);
		return ret;
	}

	state /= 1000; /* Convert to mA */
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

	r = kstrtoul(buf, 0, &usb_curr);
	if (r) {
		smblib_err(mmi_chip, "Invalid iusb value = %lu\n", usb_curr);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}
	usb_curr *= 1000; /* Convert to uA */
	r = smblib_set_charge_param(mmi_chip, &mmi_chip->param.usb_icl, usb_curr);
	if (r < 0) {
		smblib_err(mmi_chip,
			   "Factory Couldn't set usb icl = %d rc=%d\n",
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
	int r;

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		r = -ENODEV;
		goto end;
	}

	r = smblib_get_charge_param(mmi_chip, &mmi_chip->param.usb_icl, &state);
	if (r < 0) {
		smblib_err(mmi_chip,
			   "Factory Couldn't get usb_icl rc=%d\n", (int)r);
		return r;
	}
	state /= 1000; /* Convert to mA */
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_iusb, 0664,
		force_chg_iusb_show,
		force_chg_iusb_store);

#define PRE_CHARGE_CONV_MV 25
#define PRE_CHARGE_MAX 0x3F
static ssize_t force_chg_itrick_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;
	u8 value;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		smblib_err(mmi_chip,
			   "Invalid pre-charge value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	chg_current /= PRE_CHARGE_CONV_MV;

	if (chg_current > PRE_CHARGE_MAX)
		value = PRE_CHARGE_MAX;
	else
		value = (u8)chg_current;

	r = smblib_masked_write(mmi_chip, PRE_CHARGE_CURRENT_CFG_REG,
				PRE_CHARGE_CURRENT_SETTING_MASK,
				value);
	if (r < 0) {
		smblib_err(mmi_chip,
			   "Factory Couldn't set ITRICK %d  mV rc=%d\n",
			   (int)value, (int)r);
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

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		state = -ENODEV;
		goto end;
	}

	ret = smblib_read(mmi_chip, PRE_CHARGE_CURRENT_CFG_REG, &value);
	if (ret) {
		smblib_err(mmi_chip, "Pre Chg ITrick failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	value &= PRE_CHARGE_CURRENT_SETTING_MASK;

	state = value * PRE_CHARGE_CONV_MV;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_itrick, 0664,
		   force_chg_itrick_show,
		   force_chg_itrick_store);

int smblib_get_prop_dc_system_temp_level(struct smb_charger *chg,
					 union power_supply_propval *val)
{
	val->intval = chg->mmi.dc_system_temp_level;
	return 0;
}

int smblib_set_prop_dc_system_temp_level(struct smb_charger *chg,
					 const union power_supply_propval *val)
{
	if (val->intval < 0)
		return -EINVAL;

	if (chg->mmi.dc_thermal_levels <= 0)
		return -EINVAL;

	if (val->intval >= chg->mmi.dc_thermal_levels)
		chg->mmi.dc_system_temp_level = chg->mmi.dc_thermal_levels - 1;
	else
		chg->mmi.dc_system_temp_level = val->intval;

	if (chg->mmi.dc_system_temp_level == 0)
		return vote(chg->dc_icl_votable, THERMAL_DAEMON_VOTER, false, 0);

	vote(chg->dc_icl_votable, THERMAL_DAEMON_VOTER, true,
	     chg->mmi.dc_thermal_mitigation[chg->mmi.dc_system_temp_level] * 1000);

	return 0;
}

int smblib_get_prop_usb_system_temp_level(struct smb_charger *chg,
					  union power_supply_propval *val)
{
	val->intval = chg->mmi.usb_system_temp_level;
	return 0;
}

int smblib_set_prop_usb_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	if (val->intval < 0)
		return -EINVAL;

	if (chg->mmi.usb_thermal_levels <= 0)
		return -EINVAL;

	if (chg->mmi.usb_system_temp_level  >= chg->mmi.usb_thermal_levels)
		chg->mmi.usb_system_temp_level = chg->mmi.usb_thermal_levels - 1;
	else
		chg->mmi.usb_system_temp_level = val->intval;

	if (chg->mmi.usb_system_temp_level == 0)
		return vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);

	vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true,
	     chg->mmi.usb_thermal_mitigation[chg->mmi.usb_system_temp_level] * 1000);

	return 0;
}

static bool mmi_factory_check(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	bool factory = false;

	if (np)
		factory = of_property_read_bool(np, "mmi,factory-cable");

	of_node_put(np);

	return factory;
}

static void parse_mmi_dt_gpio(struct smb_charger *chg)
{
	struct device_node *node = chg->dev->of_node;
	enum of_gpio_flags flags;
	int rc;

	chg->mmi.ebchg_gpio.gpio = -EINVAL;
	chg->mmi.warn_gpio.gpio = -EINVAL;
	chg->mmi.togl_rst_gpio.gpio = -EINVAL;

	if (!node) {
		smblib_err(chg, "gpio dtree info. missing\n");
		return;
	}

	if (of_gpio_count(node) < 0) {
		smblib_err(chg, "No GPIOS defined.\n");
		return;
	}

	chg->mmi.ebchg_gpio.gpio = of_get_gpio_flags(node, 0, &flags);
	chg->mmi.ebchg_gpio.flags = flags;
	of_property_read_string_index(node, "gpio-names", 0,
				      &chg->mmi.ebchg_gpio.label);

	rc = gpio_request_one(chg->mmi.ebchg_gpio.gpio,
			      chg->mmi.ebchg_gpio.flags,
			      chg->mmi.ebchg_gpio.label);
	if (rc) {
		smblib_err(chg, "failed to request eb GPIO\n");
		return;
	}

	rc = gpio_export(chg->mmi.ebchg_gpio.gpio, 1);
	if (rc) {
		smblib_err(chg, "Failed to export eb GPIO %s: %d\n",
			   chg->mmi.ebchg_gpio.label,
			   chg->mmi.ebchg_gpio.gpio);
		return;
	}

	rc = gpio_export_link(chg->dev, chg->mmi.ebchg_gpio.label,
			      chg->mmi.ebchg_gpio.gpio);
	if (rc)
		smblib_err(chg, "Failed to eb link GPIO %s: %d\n",
			   chg->mmi.ebchg_gpio.label,
			   chg->mmi.ebchg_gpio.gpio);

	chg->mmi.warn_gpio.gpio = of_get_gpio_flags(node, 1, &flags);
	chg->mmi.warn_gpio.flags = flags;
	of_property_read_string_index(node, "gpio-names", 1,
				      &chg->mmi.warn_gpio.label);

	rc = gpio_request_one(chg->mmi.warn_gpio.gpio,
			      chg->mmi.warn_gpio.flags,
			      chg->mmi.warn_gpio.label);
	if (rc) {
		smblib_err(chg, "failed to request warn GPIO\n");
		return;
	}

	rc = gpio_export(chg->mmi.warn_gpio.gpio, 1);
	if (rc) {
		smblib_err(chg, "Failed to warn export GPIO %s: %d\n",
			   chg->mmi.warn_gpio.label,
			   chg->mmi.warn_gpio.gpio);
		return;
	}

	rc = gpio_export_link(chg->dev, chg->mmi.warn_gpio.label,
			      chg->mmi.warn_gpio.gpio);
	if (rc)
		smblib_err(chg, "Failed to link warn GPIO %s: %d\n",
			   chg->mmi.warn_gpio.label,
			   chg->mmi.warn_gpio.gpio);
	else
		chg->mmi.warn_irq = gpio_to_irq(chg->mmi.warn_gpio.gpio);

	chg->mmi.togl_rst_gpio.gpio = of_get_gpio_flags(node, 2, &flags);
	chg->mmi.togl_rst_gpio.flags = flags;
	of_property_read_string_index(node, "gpio-names", 2,
				      &chg->mmi.togl_rst_gpio.label);

	rc = gpio_request_one(chg->mmi.togl_rst_gpio.gpio,
			      chg->mmi.togl_rst_gpio.flags,
			      chg->mmi.togl_rst_gpio.label);
	if (rc) {
		smblib_err(chg, "failed to request GPIO %s: %d\n",
			   chg->mmi.togl_rst_gpio.label,
			   chg->mmi.togl_rst_gpio.gpio);
		return;
	}

	rc = gpio_export(chg->mmi.togl_rst_gpio.gpio, 1);
	if (rc) {
		smblib_err(chg, "Failed to export GPIO %s: %d\n",
			   chg->mmi.togl_rst_gpio.label,
			   chg->mmi.togl_rst_gpio.gpio);
		return;
	}

	rc = gpio_export_link(chg->dev, chg->mmi.togl_rst_gpio.label,
			      chg->mmi.togl_rst_gpio.gpio);

	if (rc)
		smblib_err(chg, "Failed to link GPIO %s: %d\n",
			   chg->mmi.togl_rst_gpio.label,
			   chg->mmi.togl_rst_gpio.gpio);
}

static int parse_mmi_dt(struct smb_charger *chg)
{
	struct device_node *node = chg->dev->of_node;
	int rc = 0;
	int byte_len;
	int i;

	if (!node) {
		smblib_err(chg, "mmi dtree info. missing\n");
		return -ENODEV;
	}


	if (of_find_property(node, "qcom,mmi-temp-zones", &byte_len)) {
		if ((byte_len / sizeof(u32)) % 4) {
			smblib_err(chg,
				   "DT error wrong mmi temp zones\n");
			return -ENODEV;
		}

		chg->mmi.temp_zones = (struct mmi_temp_zone *)
			devm_kzalloc(chg->dev, byte_len, GFP_KERNEL);

		if (chg->mmi.temp_zones == NULL)
			return -ENOMEM;

		chg->mmi.num_temp_zones =
			byte_len / sizeof(struct mmi_temp_zone);

		rc = of_property_read_u32_array(node,
				"qcom,mmi-temp-zones",
				(u32 *)chg->mmi.temp_zones,
				byte_len / sizeof(u32));
		if (rc < 0) {
			smblib_err(chg,
				   "Couldn't read mmi temp zones rc = %d\n",
				   rc);
			return rc;
		}
		smblib_err(chg, "mmi temp zones: Num: %d\n",
			   chg->mmi.num_temp_zones);
		for (i = 0; i < chg->mmi.num_temp_zones; i++) {
			smblib_err(chg,
				   "mmi temp zones: Zone %d, Temp %d C, " \
				   "Step Volt %d mV, Full Rate %d mA, " \
				   "Taper Rate %d mA\n", i,
				   chg->mmi.temp_zones[i].temp_c,
				   chg->mmi.temp_zones[i].norm_mv,
				   chg->mmi.temp_zones[i].fcc_max_ma,
				   chg->mmi.temp_zones[i].fcc_norm_ma);
		}
		chg->mmi.pres_temp_zone = ZONE_NONE;
	}

	if (of_find_property(node, "qcom,usb-thermal-mitigation", &byte_len)) {
		chg->mmi.usb_thermal_mitigation =
			devm_kzalloc(chg->dev, byte_len, GFP_KERNEL);

		if (chg->mmi.usb_thermal_mitigation == NULL)
			return -ENOMEM;

		chg->mmi.usb_thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,usb-thermal-mitigation",
				chg->mmi.usb_thermal_mitigation,
				chg->mmi.usb_thermal_levels);
		if (rc < 0) {
			smblib_err(chg,
				   "Couldn't read usb therm limits rc = %d\n",
				   rc);
			return rc;
		}
	}

	if (of_find_property(node, "qcom,dc-thermal-mitigation", &byte_len)) {
		chg->mmi.dc_thermal_mitigation =
			devm_kzalloc(chg->dev, byte_len, GFP_KERNEL);

		if (chg->mmi.dc_thermal_mitigation == NULL)
			return -ENOMEM;

		chg->mmi.dc_thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(node,
				"qcom,dc-thermal-mitigation",
				chg->mmi.dc_thermal_mitigation,
				chg->mmi.dc_thermal_levels);
		if (rc < 0) {
			smblib_err(chg,
				   "Couldn't read usb therm limits rc = %d\n",
				   rc);
			return rc;
		}
	}

	rc = of_property_read_u32(node, "qcom,iterm-ma",
				  &chg->mmi.chrg_iterm);
	if (rc)
		chg->mmi.chrg_iterm = 150;

	rc = of_property_read_u32(node, "qcom,dc-eb-icl-ma",
				  &chg->mmi.dc_ebmax_current_ma);
	if (rc)
		chg->mmi.dc_ebmax_current_ma = 900;

	rc = of_property_read_u32(node, "qcom,dc-eb-icl-eff-ma",
				  &chg->mmi.dc_eff_current_ma);
	if (rc)
		chg->mmi.dc_eff_current_ma = 500;

	/* read the external battery power supply name */
	rc = of_property_read_string(node, "qcom,eb-batt-psy-name",
				     &chg->mmi.eb_batt_psy_name);
	if (rc)
		chg->mmi.eb_batt_psy_name = "gb_battery";

	/* read the external power control power supply name */
	rc = of_property_read_string(node, "qcom,eb-pwr-psy-name",
				     &chg->mmi.eb_pwr_psy_name);
	if (rc)
		chg->mmi.eb_pwr_psy_name = "gb_ptp";

	chg->mmi.vl_ebsrc = MICRO_9V;
	chg->mmi.check_ebsrc_vl = false;

	chg->mmi.enable_charging_limit =
		of_property_read_bool(node, "qcom,enable-charging-limit");

	rc = of_property_read_u32(node, "qcom,upper-limit-capacity",
				  &chg->mmi.upper_limit_capacity);
	if (rc)
		chg->mmi.upper_limit_capacity = 100;

	rc = of_property_read_u32(node, "qcom,lower-limit-capacity",
				  &chg->mmi.lower_limit_capacity);
	if (rc)
		chg->mmi.lower_limit_capacity = 0;

	rc = of_property_read_u32(node, "qcom,vfloat-comp-uv",
				  &chg->mmi.vfloat_comp_mv);
	if (rc)
		chg->mmi.vfloat_comp_mv = 0;
	chg->mmi.vfloat_comp_mv /= 1000;

	return rc;
}

static enum alarmtimer_restart mmi_heartbeat_alarm_cb(struct alarm *alarm,
							 ktime_t now)
{
	struct smb_charger *chip = container_of(alarm, struct smb_charger,
						mmi.heartbeat_alarm);

	smblib_dbg(chip, PR_MOTO, "SMB: HB alarm fired\n");

	__pm_stay_awake(&chip->mmi.smblib_mmi_hb_wake_source);
	cancel_delayed_work(&chip->mmi.heartbeat_work);
	/* Delay by 500 ms to allow devices to resume. */
	schedule_delayed_work(&chip->mmi.heartbeat_work,
			      msecs_to_jiffies(500));

	return ALARMTIMER_NORESTART;
}

void mmi_init(struct smb_charger *chg)
{
	int rc;

	if (!chg)
		return;

	chg->ipc_log = ipc_log_context_create(QPNP_LOG_PAGES,
						"charger", 0);
	if (chg->ipc_log == NULL)
		pr_err("%s: failed to create charger IPC log\n",
						__func__);
	else
		smblib_dbg(chg, PR_MISC,
			   "IPC logging is enabled for charger\n");

	chg->ipc_log_reg = ipc_log_context_create(QPNP_LOG_PAGES,
						"charger_reg", 0);
	if (chg->ipc_log_reg == NULL)
		pr_err("%s: failed to create register IPC log\n",
						__func__);
	else
		smblib_dbg(chg, PR_MISC,
			   "IPC logging is enabled for charger\n");

	mmi_chip = chg;
	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_9V;
	chg->mmi.factory_mode = mmi_factory_check();
	chg->mmi.is_factory_image = false;
	chg->mmi.charging_limit_modes = CHARGING_LIMIT_UNKNOWN;
	chg->mmi.charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;

	INIT_DELAYED_WORK(&chg->mmi.warn_irq_work, warn_irq_w);
	INIT_DELAYED_WORK(&chg->mmi.heartbeat_work, mmi_heartbeat_work);
	wakeup_source_init(&chg->mmi.smblib_mmi_hb_wake_source,
			   "smblib_mmi_hb_wake");
	alarm_init(&chg->mmi.heartbeat_alarm, ALARM_BOOTTIME,
		   mmi_heartbeat_alarm_cb);

	rc = parse_mmi_dt(chg);
	if (rc < 0)
		smblib_err(chg, "Error getting mmi dt items\n");

	parse_mmi_dt_gpio(chg);

	if (chg->mmi.warn_irq) {
		rc = request_irq(chg->mmi.warn_irq,
				 warn_irq_handler,
				 IRQF_TRIGGER_FALLING,
				 "mmi_factory_warn", chg);
		if (rc) {
			smblib_err(chg, "request irq failed for Warn\n");
		}
	} else
		smblib_err(chg, "IRQ for Warn doesn't exist\n");

	chg->mmi.smb_reboot.notifier_call = smbchg_reboot;
	chg->mmi.smb_reboot.next = NULL;
	chg->mmi.smb_reboot.priority = 1;
	rc = register_reboot_notifier(&chg->mmi.smb_reboot);
	if (rc)
		smblib_err(chg, "SMB register for reboot failed\n");

	rc = device_create_file(chg->dev,
				&dev_attr_force_demo_mode);
	if (rc) {
		smblib_err(chg, "couldn't create force_demo_mode\n");
	}

	rc = device_create_file(chg->dev,
				&dev_attr_force_max_chrg_temp);
	if (rc) {
		smblib_err(chg, "couldn't create force_max_chrg_temp\n");
	}

	rc = device_create_file(chg->dev,
				&dev_attr_factory_image_mode);
	if (rc)
		smblib_err(chg, "couldn't create factory_image_mode\n");

	if (chg->mmi.factory_mode) {
		mmi_chip = chg;
		smblib_err(chg, "Entering Factory Mode SMB!\n");

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_usb_suspend);
		if (rc) {
			smblib_err(chg,
				   "couldn't create force_chg_usb_suspend\n");
		}

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_fail_clear);
		if (rc) {
			smblib_err(chg,
				   "couldn't create force_chg_fail_clear\n");
		}

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_auto_enable);
		if (rc) {
			smblib_err(chg,
				   "couldn't create force_chg_auto_enable\n");
		}

		rc = device_create_file(chg->dev,
				&dev_attr_force_chg_ibatt);
		if (rc) {
			smblib_err(chg, "couldn't create force_chg_ibatt\n");
		}

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_iusb);
		if (rc) {
			smblib_err(chg, "couldn't create force_chg_iusb\n");
		}

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_itrick);
		if (rc) {
			smblib_err(chg, "couldn't create force_chg_itrick\n");
		}
	}

	/* reconfigure allowed voltage for HVDCP */
	rc = smblib_set_adapter_allowance(chg,
			USBIN_ADAPTER_ALLOW_5V_TO_9V);

	/* Disable all WIPWR Feature */
	rc = smblib_masked_write(chg, WI_PWR_OPTIONS_REG, 0xFF, 0x00);
	if (rc)
		smblib_err(chg, "couldn't disable Wipwr settings\n");

	/* Set 4.0 V for DCIN AICL Threshold */
	rc = smblib_masked_write(chg, DCIN_AICL_REF_SEL_CFG_REG,
				 DCIN_CONT_AICL_THRESHOLD_CFG_MASK,
				 0x00);
	if (rc)
		smblib_err(chg, "couldn't set DCIN AICL Threshold\n");

	/* Turn Jeita OFF */
	rc = smblib_masked_write(chg, JEITA_EN_CFG_REG,
				 0x3F,
				 0x00);
	if (rc)
		smblib_err(chg, "couldn't set JEITA CFG\n");

	/* Register the notifier for the psy updates*/
	chg->mmi.mmi_psy_notifier.notifier_call = mmi_psy_notifier_call;
	rc = power_supply_reg_notifier(&chg->mmi.mmi_psy_notifier);
	if (rc)
		smblib_err(chg, "failed to reg notifier: %d\n", rc);

	chg->mmi.init_done = true;
}

void mmi_deinit(struct smb_charger *chg)
{
	if (!chg || !chg->mmi.init_done)
		return;

	device_remove_file(chg->dev,
			   &dev_attr_force_demo_mode);
	device_remove_file(chg->dev,
			   &dev_attr_force_max_chrg_temp);

	if (chg->mmi.factory_mode) {
		device_remove_file(chg->dev,
				   &dev_attr_force_chg_usb_suspend);
		device_remove_file(chg->dev,
				   &dev_attr_force_chg_fail_clear);
		device_remove_file(chg->dev,
				   &dev_attr_force_chg_auto_enable);
		device_remove_file(chg->dev,
				   &dev_attr_force_chg_ibatt);
		device_remove_file(chg->dev,
				   &dev_attr_force_chg_iusb);
		device_remove_file(chg->dev,
				   &dev_attr_force_chg_itrick);
	}
	wakeup_source_trash(&chg->mmi.smblib_mmi_hb_wake_source);
	power_supply_unreg_notifier(&chg->mmi.mmi_psy_notifier);
}
