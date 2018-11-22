/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/ipc_logging.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/pmic-voter.h>
#include <linux/of_batterydata.h>
#include <linux/alarmtimer.h>
#include "smb5-lib.h"
#include "smb5-reg.h"
#include "battery.h"
#include "schgm-flash.h"
#include "step-chg-jeita.h"
#include "storm-watch.h"
#include "schgm-flash.h"

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

#define typec_rp_med_high(chg, typec_mode)			\
	((typec_mode == POWER_SUPPLY_TYPEC_SOURCE_MEDIUM	\
	|| typec_mode == POWER_SUPPLY_TYPEC_SOURCE_HIGH)	\
	&& !chg->typec_legacy)

int smblib_read(struct smb_charger *chg, u16 addr, u8 *val)
{
	unsigned int value;
	int rc = 0;

	rc = regmap_read(chg->regmap, addr, &value);
	if (rc >= 0)
		*val = (u8)value;

	return rc;
}

int smblib_batch_read(struct smb_charger *chg, u16 addr, u8 *val,
			int count)
{
	return regmap_bulk_read(chg->regmap, addr, val, count);
}

int smblib_write(struct smb_charger *chg, u16 addr, u8 val)
{
	return regmap_write(chg->regmap, addr, val);
}

int smblib_batch_write(struct smb_charger *chg, u16 addr, u8 *val,
			int count)
{
	return regmap_bulk_write(chg->regmap, addr, val, count);
}

int smblib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val)
{
	return regmap_update_bits(chg->regmap, addr, mask, val);
}

int smblib_get_jeita_cc_delta(struct smb_charger *chg, int *cc_delta_ua)
{
	int rc, cc_minus_ua;
	u8 stat;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_7_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}

	if (stat & BAT_TEMP_STATUS_HOT_SOFT_BIT) {
		rc = smblib_get_charge_param(chg, &chg->param.jeita_cc_comp_hot,
					&cc_minus_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get jeita cc minus rc=%d\n",
					rc);
			return rc;
		}
	} else if (stat & BAT_TEMP_STATUS_COLD_SOFT_BIT) {
		rc = smblib_get_charge_param(chg,
					&chg->param.jeita_cc_comp_cold,
					&cc_minus_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get jeita cc minus rc=%d\n",
					rc);
			return rc;
		}
	} else {
		cc_minus_ua = 0;
	}

	*cc_delta_ua = -cc_minus_ua;

	return 0;
}

int smblib_icl_override(struct smb_charger *chg, bool override)
{
	int rc;

	rc = smblib_masked_write(chg, USBIN_LOAD_CFG_REG,
				ICL_OVERRIDE_AFTER_APSD_BIT,
				override ? ICL_OVERRIDE_AFTER_APSD_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't override ICL rc=%d\n", rc);

	return rc;
}

int smblib_stat_sw_override_cfg(struct smb_charger *chg, bool override)
{
	int rc = 0;

	/* override  = 1, SW STAT override; override = 0, HW auto mode */
	rc = smblib_masked_write(chg, MISC_SMB_EN_CMD_REG,
				SMB_EN_OVERRIDE_BIT,
				override ? SMB_EN_OVERRIDE_BIT : 0);
	if (rc < 0) {
		dev_err(chg->dev, "Couldn't configure SW STAT override rc=%d\n",
			rc);
		return rc;
	}

	return rc;
}

static void smblib_notify_extcon_props(struct smb_charger *chg, int id)
{
	union extcon_property_value val;
	union power_supply_propval prop_val;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC) {
		smblib_get_prop_typec_cc_orientation(chg, &prop_val);
		val.intval = ((prop_val.intval == 2) ? 1 : 0);
		extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_TYPEC_POLARITY, val);
	}

	val.intval = true;
	extcon_set_property(chg->extcon, id,
				EXTCON_PROP_USB_SS, val);
}

static void smblib_notify_device_mode(struct smb_charger *chg, bool enable)
{
	if (enable)
		smblib_notify_extcon_props(chg, EXTCON_USB);

	extcon_set_state_sync(chg->extcon, EXTCON_USB, enable);

	if (chg->dr_inst)
		dual_role_instance_changed(chg->dr_inst);
}

static void smblib_notify_usb_host(struct smb_charger *chg, bool enable)
{
	if (enable)
		smblib_notify_extcon_props(chg, EXTCON_USB_HOST);

	extcon_set_state_sync(chg->extcon, EXTCON_USB_HOST, enable);

	if (chg->dr_inst)
		dual_role_instance_changed(chg->dr_inst);
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

static const struct apsd_result smblib_apsd_results[] = {
	[UNKNOWN] = {
		.name	= "UNKNOWN",
		.bit	= 0,
		.pst	= POWER_SUPPLY_TYPE_UNKNOWN
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

#define AICL_RANGE2_MIN_MV		5600
#define AICL_RANGE2_STEP_DELTA_MV	200
#define AICL_RANGE2_OFFSET		16
int smblib_get_aicl_cont_threshold(struct smb_chg_param *param, u8 val_raw)
{
	int base = param->min_u;
	u8 reg = val_raw;
	int step = param->step_u;


	if (val_raw >= AICL_RANGE2_OFFSET) {
		reg = val_raw - AICL_RANGE2_OFFSET;
		base = AICL_RANGE2_MIN_MV;
		step = AICL_RANGE2_STEP_DELTA_MV;
	}

	return base + (reg * step);
}

/********************
 * REGISTER SETTERS *
 ********************/
static const struct buck_boost_freq chg_freq_list[] = {
	[0] = {
		.freq_khz	= 2400,
		.val		= 7,
	},
	[1] = {
		.freq_khz	= 2100,
		.val		= 8,
	},
	[2] = {
		.freq_khz	= 1600,
		.val		= 11,
	},
	[3] = {
		.freq_khz	= 1200,
		.val		= 15,
	},
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
		if (chg_freq_list[i].freq_khz == val_u)
			break;
	}
	if (i == ARRAY_SIZE(chg_freq_list)) {
		pr_err("Invalid frequency %d Hz\n", val_u / 2);
		return -EINVAL;
	}

	*val_raw = chg_freq_list[i].val;

	return 0;
}

int smblib_set_opt_switcher_freq(struct smb_charger *chg, int fsw_khz)
{
	union power_supply_propval pval = {0, };
	int rc = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher, fsw_khz);
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
		if (val_u > param->max_u || val_u < param->min_u)
			smblib_dbg(chg, PR_MISC,
				"%s: %d is out of range [%d, %d]\n",
				param->name, val_u, param->min_u, param->max_u);

		if (val_u > param->max_u)
			val_u = param->max_u;
		if (val_u < param->min_u)
			val_u = param->min_u;

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

	smblib_dbg(chg, PR_MISC, "%s\n", suspend ? "suspend" : "resume");
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

	/* PMI632 only support max. 9V */
	if (chg->smb_version == PMI632_SUBTYPE) {
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
	int rc;
	u8 allowed_voltage;

	if (min_allowed_uv == MICRO_5V && max_allowed_uv == MICRO_5V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_5V;
		smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_5V);
	} else if (min_allowed_uv == MICRO_9V && max_allowed_uv == MICRO_9V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_9V;
		smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_9V);
	} else if (min_allowed_uv == MICRO_12V && max_allowed_uv == MICRO_12V) {
		allowed_voltage = USBIN_ADAPTER_ALLOW_12V;
		smblib_set_opt_switcher_freq(chg, chg->chg_freq.freq_12V);
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

	return rc;
}

int smblib_set_aicl_cont_threshold(struct smb_chg_param *param,
				int val_u, u8 *val_raw)
{
	int base = param->min_u;
	int offset = 0;
	int step = param->step_u;

	if (val_u > param->max_u)
		val_u = param->max_u;
	if (val_u < param->min_u)
		val_u = param->min_u;

	if (val_u >= AICL_RANGE2_MIN_MV) {
		base = AICL_RANGE2_MIN_MV;
		step = AICL_RANGE2_STEP_DELTA_MV;
		offset = AICL_RANGE2_OFFSET;
	};

	*val_raw = ((val_u - base) / step) + offset;

	return 0;
}

/********************
 * HELPER FUNCTIONS *
 ********************/

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

int smblib_configure_hvdcp_apsd(struct smb_charger *chg, bool enable)
{
	int rc;
	u8 mask = HVDCP_EN_BIT | BC1P2_SRC_DETECT_BIT;

	if (chg->pd_not_supported)
		return 0;

	rc = smblib_masked_write(chg, USBIN_OPTIONS_1_CFG_REG, mask,
						enable ? mask : 0);
	if (rc < 0)
		smblib_err(chg, "failed to write USBIN_OPTIONS_1_CFG rc=%d\n",
				rc);

	return rc;
}

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
		if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT)
			chg->real_charger_type = POWER_SUPPLY_TYPE_USB;
	}

	smblib_dbg(chg, PR_MOTO, "APSD=%s PD=%d\n",
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

	if (!chg->jeita_configured)
		schedule_work(&chg->jeita_update_work);

	if (!chg->pl.psy && !strcmp(psy->desc->name, "parallel")) {
		chg->pl.psy = psy;
		schedule_work(&chg->pl_update_work);
	}

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

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
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
#ifdef QCOM_BASE
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
			is_flash_active(chg) ? SDP_CURRENT_UA : SDP_100_MA);
#endif
	vote(chg->usb_icl_votable, SW_QC3_VOTER, false, 0);

	/* reconfigure allowed voltage for HVDCP */
	rc = smblib_set_adapter_allowance(chg,
			USBIN_ADAPTER_ALLOW_5V_TO_9V);
	if (rc < 0)
		smblib_err(chg, "Couldn't set USBIN_ADAPTER_ALLOW_5V_TO_9V rc=%d\n",
			rc);

	/* reset USBOV votes and cancel work */
	cancel_delayed_work_sync(&chg->usbov_dbc_work);
	vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
	chg->dbc_usbov = false;

#ifdef QCOM_BASE
	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_5V;
#endif
	chg->usb_icl_delta_ua = 0;
	chg->pulse_cnt = 0;
	chg->uusb_apsd_rerun_done = false;

	/* write back the default FLOAT charger configuration */
	rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				(u8)FLOAT_OPTIONS_MASK, chg->float_cfg);
	if (rc < 0)
		smblib_err(chg, "Couldn't write float charger options rc=%d\n",
			rc);

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

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_DEBUG_BATTERY, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get debug battery prop rc=%d\n", rc);
		return;
	}
	if (chg->suspend_input_on_debug_batt) {
		vote(chg->usb_icl_votable, DEBUG_BOARD_VOTER, val.intval, 0);
		vote(chg->dc_suspend_votable, DEBUG_BOARD_VOTER, val.intval, 0);
		if (val.intval)
			pr_info("Input suspended: Fake battery\n");
	} else {
		vote(chg->chg_disable_votable, DEBUG_BOARD_VOTER,
					val.intval, 0);
	}
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

static int smblib_get_pulse_cnt(struct smb_charger *chg, int *count)
{
	*count = chg->pulse_cnt;
	return 0;
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
	bool hc_mode = false, override = false;

	if (chg->mmi.factory_mode) {
		pr_err("USB ICL callback in Facory Mode! %d\n", icl_ua);
		return rc;
	}

	smblib_dbg(chg, PR_MISC, "%d uA\n", icl_ua);
	/* suspend and return if 25mA or less is requested */
	if (icl_ua <= USBIN_25MA)
		return smblib_set_usb_suspend(chg, true);

	if (icl_ua == INT_MAX)
		goto set_mode;

	/* configure current */
	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB
		&& (chg->typec_legacy
		|| chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
		|| chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)) {
		rc = set_sdp_current(chg, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set SDP ICL rc=%d\n", rc);
			goto out;
		}
	} else {
		set_sdp_current(chg, 100000);
		rc = smblib_set_charge_param(chg, &chg->param.usb_icl, icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set HC ICL rc=%d\n", rc);
			goto out;
		}
		hc_mode = true;

		/*
		 * Micro USB mode follows ICL register independent of override
		 * bit, configure override only for typeC mode.
		 */
		if (chg->connector_type == POWER_SUPPLY_CONNECTOR_TYPEC)
			override = true;
	}

set_mode:
	rc = smblib_masked_write(chg, USBIN_ICL_OPTIONS_REG,
		USBIN_MODE_CHG_BIT, hc_mode ? USBIN_MODE_CHG_BIT : 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set USBIN_ICL_OPTIONS rc=%d\n", rc);
		goto out;
	}

	rc = smblib_icl_override(chg, override);
	if (rc < 0) {
		smblib_err(chg, "Couldn't set ICL override rc=%d\n", rc);
		goto out;
	}

	/* unsuspend after configuring current and override */
	rc = smblib_set_usb_suspend(chg, false);
	if (rc < 0) {
		smblib_err(chg, "Couldn't resume input rc=%d\n", rc);
		goto out;
	}

	/* Re-run AICL */
	if (chg->real_charger_type != POWER_SUPPLY_TYPE_USB)
		rc = smblib_run_aicl(chg, RERUN_AICL);
out:
	return rc;
}

int smblib_get_icl_current(struct smb_charger *chg, int *icl_ua)
{
	int rc = 0;
	u8 load_cfg;
	bool override;

	if ((chg->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_DEFAULT
		|| chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
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
		rc = smblib_get_charge_param(chg, &chg->param.icl_max_stat,
					icl_ua);
		if (rc < 0) {
			smblib_err(chg, "Couldn't get HC ICL rc=%d\n", rc);
			return rc;
		}
	}

	return 0;
}

/********************
 * Moisture Protection *
 ********************/
#define MICRO_USB_DETECTION_ON_TIME_20_MS 0x08
#define MICRO_USB_DETECTION_PERIOD_X_100 0x03
#define U_USB_STATUS_WATER_PRESENT 0x00
static int smblib_set_moisture_protection(struct smb_charger *chg,
				bool enable)
{
	int rc = 0;

	if (chg->moisture_present == enable) {
		smblib_dbg(chg, PR_MISC, "No change in moisture protection status\n");
		return rc;
	}

	if (enable) {
		chg->moisture_present = true;

		/* Disable uUSB factory mode detection */
		rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable uUSB factory mode detection rc=%d\n",
				rc);
			return rc;
		}

		/* Disable moisture detection and uUSB state change interrupt */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					TYPEC_WATER_DETECTION_INT_EN_BIT |
					MICRO_USB_STATE_CHANGE_INT_EN_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable moisture detection interrupt rc=%d\n",
			rc);
			return rc;
		}

		/* Set 1% duty cycle on ID detection */
		rc = smblib_masked_write(chg,
					TYPEC_U_USB_WATER_PROTECTION_CFG_REG,
					EN_MICRO_USB_WATER_PROTECTION_BIT |
					MICRO_USB_DETECTION_ON_TIME_CFG_MASK |
					MICRO_USB_DETECTION_PERIOD_CFG_MASK,
					EN_MICRO_USB_WATER_PROTECTION_BIT |
					MICRO_USB_DETECTION_ON_TIME_20_MS |
					MICRO_USB_DETECTION_PERIOD_X_100);
		if (rc < 0) {
			smblib_err(chg, "Couldn't set 1 percent CC_ID duty cycle rc=%d\n",
				rc);
			return rc;
		}

		vote(chg->usb_icl_votable, MOISTURE_VOTER, true, 0);
	} else {
		chg->moisture_present = false;
		vote(chg->usb_icl_votable, MOISTURE_VOTER, false, 0);

		/* Enable moisture detection and uUSB state change interrupt */
		rc = smblib_masked_write(chg, TYPE_C_INTERRUPT_EN_CFG_2_REG,
					TYPEC_WATER_DETECTION_INT_EN_BIT |
					MICRO_USB_STATE_CHANGE_INT_EN_BIT,
					TYPEC_WATER_DETECTION_INT_EN_BIT |
					MICRO_USB_STATE_CHANGE_INT_EN_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't enable moisture detection and uUSB state change interrupt rc=%d\n",
				rc);
			return rc;
		}

		/* Disable periodic monitoring of CC_ID pin */
		rc = smblib_write(chg, TYPEC_U_USB_WATER_PROTECTION_CFG_REG, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable 1 percent CC_ID duty cycle rc=%d\n",
				rc);
			return rc;
		}

		/* Enable uUSB factory mode detection */
		rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT,
					EN_MICRO_USB_FACTORY_MODE_BIT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't disable uUSB factory mode detection rc=%d\n",
				rc);
			return rc;
		}
	}

	smblib_dbg(chg, PR_MISC, "Moisture protection %s\n",
			chg->moisture_present ? "enabled" : "disabled");
	return rc;
}

/*********************
 * VOTABLE CALLBACKS *
 *********************/

static int smblib_dc_suspend_vote_callback(struct votable *votable, void *data,
			int suspend, const char *client)
{
	struct smb_charger *chg = data;

	if (chg->smb_version == PMI632_SUBTYPE)
		return 0;

	if (chg->mmi.factory_mode)
		return 0;

	/* resume input if suspend is invalid */
	if (suspend < 0)
		suspend = 0;

	return smblib_set_dc_suspend(chg, (bool)suspend);
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

	return 0;
}

static int smblib_usb_irq_enable_vote_callback(struct votable *votable,
				void *data, int enable, const char *client)
{
	struct smb_charger *chg = data;

	if (!chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].irq ||
				!chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq)
		return 0;

	if (enable) {
		enable_irq(chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].irq);
		enable_irq(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
	} else {
		disable_irq_nosync(
			chg->irq_info[INPUT_CURRENT_LIMITING_IRQ].irq);
		disable_irq_nosync(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
	}

	return 0;
}

/*******************
 * VCONN REGULATOR *
 * *****************/

int smblib_vconn_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 stat, orientation;

	smblib_dbg(chg, PR_OTG, "enabling VCONN\n");

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return rc;
	}

	/* VCONN orientation is opposite to that of CC */
	orientation =
		stat & TYPEC_CCOUT_VALUE_BIT ? 0 : VCONN_EN_ORIENTATION_BIT;
	rc = smblib_masked_write(chg, TYPE_C_VCONN_CONTROL_REG,
				VCONN_EN_VALUE_BIT | VCONN_EN_ORIENTATION_BIT,
				VCONN_EN_VALUE_BIT | orientation);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_CCOUT_CONTROL_REG rc=%d\n",
			rc);
		return rc;
	}

	return 0;
}

int smblib_vconn_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;

	smblib_dbg(chg, PR_OTG, "disabling VCONN\n");
	rc = smblib_masked_write(chg, TYPE_C_VCONN_CONTROL_REG,
				 VCONN_EN_VALUE_BIT, 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable vconn regulator rc=%d\n", rc);

	return 0;
}

int smblib_vconn_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;
	u8 cmd;

	rc = smblib_read(chg, TYPE_C_VCONN_CONTROL_REG, &cmd);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_INTRPT_ENB_SOFTWARE_CTRL rc=%d\n",
			rc);
		return rc;
	}

	return (cmd & VCONN_EN_VALUE_BIT) ? 1 : 0;
}

/*****************
 * OTG REGULATOR *
 *****************/

int smblib_vbus_regulator_enable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	smblib_dbg(chg, PR_OTG, "enabling OTG\n");

	rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, OTG_EN_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable OTG rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_vbus_regulator_disable(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc;

	smblib_dbg(chg, PR_OTG, "disabling OTG\n");

	rc = smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable OTG regulator rc=%d\n", rc);
		return rc;
	}

	return 0;
}

int smblib_vbus_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct smb_charger *chg = rdev_get_drvdata(rdev);
	int rc = 0;
	u8 cmd;

	rc = smblib_read(chg, DCDC_CMD_OTG_REG, &cmd);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read CMD_OTG rc=%d", rc);
		return rc;
	}

	return (cmd & OTG_EN_BIT) ? 1 : 0;
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

	rc = smblib_get_prop_from_bms(chg, POWER_SUPPLY_PROP_CAPACITY, val);

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
	bool usb_online, dc_online;
	u8 stat;
	int rc, suspend = 0;

	if (chg->dbc_usbov) {
		rc = smblib_get_prop_usb_present(chg, &pval);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't get usb present prop rc=%d\n", rc);
			return rc;
		}

		rc = smblib_get_usb_suspend(chg, &suspend);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't get usb suspend rc=%d\n", rc);
			return rc;
		}

		/*
		 * Report charging as long as USBOV is not debounced and
		 * charging path is un-suspended.
		 */
		if (pval.intval && !suspend) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			return 0;
		}
	}

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
	case PAUSE_CHARGE:
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

	if (!usb_online && dc_online
		&& chg->fake_batt_status == POWER_SUPPLY_STATUS_FULL) {
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return 0;
	}

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_5_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
				rc);
			return rc;
	}

	stat &= ENABLE_TRICKLE_BIT | ENABLE_PRE_CHARGING_BIT |
						ENABLE_FULLON_MODE_BIT;

	if (!stat)
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

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_7_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
			rc);
		return rc;
	}
	if (stat & BAT_TEMP_STATUS_TOO_COLD_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COLD;
	else if (stat & BAT_TEMP_STATUS_TOO_HOT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (stat & BAT_TEMP_STATUS_COLD_SOFT_BIT)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else if (stat & BAT_TEMP_STATUS_HOT_SOFT_BIT)
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

int smblib_get_prop_system_temp_level_max(struct smb_charger *chg,
				union power_supply_propval *val)
{
	val->intval = chg->thermal_levels;
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

	if (chg->mmi.pres_chrg_step == STEP_FULL)
		val->intval = 1;

	return 0;
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

int smblib_set_prop_batt_status(struct smb_charger *chg,
				  const union power_supply_propval *val)
{
	/* Faking battery full */
	if (val->intval == POWER_SUPPLY_STATUS_FULL)
		chg->fake_batt_status = val->intval;
	else
		chg->fake_batt_status = -EINVAL;

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

	vote(chg->chg_disable_votable, THERMAL_DAEMON_VOTER, false, 0);
	if (chg->system_temp_level == 0)
		return vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);

	vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			chg->thermal_mitigation[chg->system_temp_level] * 1000);
	return 0;
}

int smblib_set_prop_input_current_limited(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	chg->fake_input_current_limited = val->intval;
	return 0;
}

int smblib_run_aicl(struct smb_charger *chg, int type)
{
	int rc;
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

	stat = (type == RERUN_AICL) ? RERUN_AICL_BIT : RESTART_AICL_BIT;
	rc = smblib_masked_write(chg, AICL_CMD_REG, stat, stat);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to AICL_CMD_REG rc=%d\n",
				rc);
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

int smblib_force_vbus_voltage(struct smb_charger *chg, u8 val)
{
	int rc;

	rc = smblib_masked_write(chg, CMD_HVDCP_2_REG, val, val);
	if (rc < 0)
		smblib_err(chg, "Couldn't write to CMD_HVDCP_2_REG rc=%d\n",
				rc);

	return rc;
}

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
				smblib_err(chg, "Couldn't get max curr rc=%d\n",
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
		dev_err(chg->dev, "Couldn't configure s/w jeita rc=%d\n",
				rc);
		return rc;
	}
	return 0;
}

int smblib_configure_wdog(struct smb_charger *chg, bool enable)
{
	int rc;
	u8 val = 0;

	if (enable)
		val = WDOG_TIMER_EN_ON_PLUGIN_BIT | BARK_WDOG_INT_EN_BIT;

	/* enable WD BARK and enable it on plugin */
	rc = smblib_masked_write(chg, WD_CFG_REG,
				WATCHDOG_TRIGGER_AFP_EN_BIT |
				WDOG_TIMER_EN_ON_PLUGIN_BIT |
				BARK_WDOG_INT_EN_BIT, val);
	if (rc < 0) {
		pr_err("Couldn't configue WD config rc=%d\n", rc);
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

	val->intval = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);
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
		if (chg->smb_version == PMI632_SUBTYPE)
			val->intval = MICRO_9V;
		else
			val->intval = MICRO_12V;
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		val->intval = chg->voltage_max_uv;
		break;
	default:
		val->intval = MICRO_5V;
		break;
	}

	return 0;
}

int smblib_get_prop_usb_voltage_max_design(struct smb_charger *chg,
					union power_supply_propval *val)
{
	switch (chg->real_charger_type) {
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
	case POWER_SUPPLY_TYPE_USB_PD:
		if (chg->smb_version == PMI632_SUBTYPE)
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
	int rc = 0;

	rc = smblib_get_prop_usb_present(chg, val);
	if (rc < 0 || !val->intval)
		return rc;

	if (chg->usb_psy)
		rc = power_supply_get_property(chg->usb_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
	return rc;
}

int smblib_get_prop_typec_cc_orientation(struct smb_charger *chg,
					 union power_supply_propval *val)
{
	int rc = 0;
	u8 stat;

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n", rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_4 = 0x%02x\n", stat);

	if (stat & CC_ATTACHED_BIT)
		val->intval = (bool)(stat & CC_ORIENTATION_BIT) + 1;
	else
		val->intval = 0;

	return rc;
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
	int rc;
	u8 stat;

	rc = smblib_read(chg, TYPE_C_SNK_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_1 rc=%d\n", rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_STATUS_1 = 0x%02x\n", stat);

	switch (stat & DETECTED_SRC_TYPE_MASK) {
	case SNK_RP_STD_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	case SNK_RP_1P5_BIT:
		return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
	case SNK_RP_3P0_BIT:
		if(chg->mmi.mmi_hvdcp_disable) {
			smblib_dbg(chg, PR_MOTO, "hvdcp disabled, return SOURCE_MEDIUM");
			return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
		} else {
			return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
		}
	case SNK_RP_SHORT_BIT:
		return POWER_SUPPLY_TYPEC_NON_COMPLIANT;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblib_get_prop_dfp_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, TYPE_C_SRC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_SRC_STATUS_REG rc=%d\n",
				rc);
		return POWER_SUPPLY_TYPEC_NONE;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_SRC_STATUS_REG = 0x%02x\n", stat);

	switch (stat & DETECTED_SNK_TYPE_MASK) {
	case AUDIO_ACCESS_RA_RA_BIT:
		return POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
	case SRC_DEBUG_ACCESS_BIT:
		return POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY;
	case SRC_RD_RA_VCONN_BIT:
		return POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE;
	case SRC_RD_OPEN_BIT:
		return POWER_SUPPLY_TYPEC_SINK;
	default:
		break;
	}

	return POWER_SUPPLY_TYPEC_NONE;
}

static int smblib_get_prop_typec_mode(struct smb_charger *chg)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
				rc);
		return 0;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_MISC_STATUS_REG = 0x%02x\n", stat);

	if (stat & SNK_SRC_MODE_BIT)
		return smblib_get_prop_dfp_mode(chg);
	else
		return smblib_get_prop_ufp_mode(chg);
}

int smblib_get_prop_typec_power_role(struct smb_charger *chg,
				     union power_supply_propval *val)
{
	int rc = 0;
	u8 ctrl;

	rc = smblib_read(chg, TYPE_C_MODE_CFG_REG, &ctrl);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_MODE_CFG_REG rc=%d\n",
			rc);
		return rc;
	}
	smblib_dbg(chg, PR_REGISTER, "TYPE_C_MODE_CFG_REG = 0x%02x\n",
		   ctrl);

	if (ctrl & TYPEC_DISABLE_CMD_BIT) {
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		return rc;
	}

	switch (ctrl & (EN_SRC_ONLY_BIT | EN_SNK_ONLY_BIT)) {
	case 0:
		val->intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		break;
	case EN_SRC_ONLY_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
		break;
	case EN_SNK_ONLY_BIT:
		val->intval = POWER_SUPPLY_TYPEC_PR_SINK;
		break;
	default:
		val->intval = POWER_SUPPLY_TYPEC_PR_NONE;
		smblib_err(chg, "unsupported power role 0x%02lx\n",
			ctrl & (EN_SRC_ONLY_BIT | EN_SNK_ONLY_BIT));
		return -EINVAL;
	}

	return rc;
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
	val->intval = chg->ok_to_pd;
	return 0;
}

int smblib_get_prop_die_health(struct smb_charger *chg,
						union power_supply_propval *val)
{
	int rc;
	u8 stat;

	rc = smblib_read(chg, MISC_TEMP_RANGE_STATUS_REG, &stat);
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


#ifdef QCOM_BASE
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
#endif

/*******************
 * USB PSY SETTERS *
 * *****************/

int smblib_set_prop_pd_current_max(struct smb_charger *chg,
				    const union power_supply_propval *val)
{
	int rc;

	if (chg->pd_active)
		rc = vote(chg->usb_icl_votable, PD_VOTER, true, val->intval);
	else
		rc = -EPERM;

	return rc;
}

#ifdef QCOM_BASE
static int smblib_handle_usb_current(struct smb_charger *chg,
					int usb_current)
{
	int rc = 0, rp_ua, typec_mode;
	union power_supply_propval val = {0, };

	if (chg->real_charger_type == POWER_SUPPLY_TYPE_USB_FLOAT) {
		if (usb_current == -ETIMEDOUT) {
			if ((chg->float_cfg & FLOAT_OPTIONS_MASK)
						== FORCE_FLOAT_SDP_CFG_BIT) {
				/*
				 * Confiugure USB500 mode if Float charger is
				 * configured for SDP.
				 */
				rc = set_sdp_current(chg, USBIN_500MA);
				if (rc < 0)
					smblib_err(chg,
						"Couldn't set SDP ICL rc=%d\n",
						rc);

				return rc;
			}

			if (chg->connector_type ==
					POWER_SUPPLY_CONNECTOR_TYPEC) {
				/*
				 * Valid FLOAT charger, report the current
				 * based of Rp.
				 */
				typec_mode = smblib_get_prop_typec_mode(chg);
				rp_ua = get_rp_based_dcp_current(chg,
								typec_mode);
				rc = vote(chg->usb_icl_votable,
					SW_ICL_MAX_VOTER, true, rp_ua);
				if (rc < 0)
					return rc;
			} else {
				rc = vote(chg->usb_icl_votable,
					SW_ICL_MAX_VOTER, true, DCP_CURRENT_UA);
				if (rc < 0)
					return rc;
			}
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
			rc = vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER,
							false, 0);
			if (rc < 0)
				return rc;
		}
	} else {
		rc = smblib_get_prop_usb_present(chg, &val);
		if (!rc && !val.intval)
			return 0;

		/* if flash is active force 500mA */
		if ((usb_current < SDP_CURRENT_UA) && is_flash_active(chg))
			usb_current = SDP_CURRENT_UA;

		rc = vote(chg->usb_icl_votable, USB_PSY_VOTER, true,
							usb_current);
		if (rc < 0) {
			pr_err("Couldn't vote ICL USB_PSY_VOTER rc=%d\n", rc);
			return rc;
		}

		rc = vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		if (rc < 0) {
			pr_err("Couldn't remove SW_ICL_MAX vote rc=%d\n", rc);
			return rc;
		}

	}

	return 0;
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

	if (0 >= val->intval)
		enable = false;

	if (!chg->pd_active &&
	    (smblib_get_prop_typec_mode(chg) ==
	     POWER_SUPPLY_TYPEC_SOURCE_DEFAULT)) {
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

	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher,
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

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	switch (val->intval) {
	case POWER_SUPPLY_TYPEC_PR_NONE:
		power_role = TYPEC_DISABLE_CMD_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_DUAL:
		power_role = 0;
		break;
	case POWER_SUPPLY_TYPEC_PR_SINK:
		power_role = EN_SNK_ONLY_BIT;
		break;
	case POWER_SUPPLY_TYPEC_PR_SOURCE:
		power_role = EN_SRC_ONLY_BIT;
		break;
	default:
		smblib_err(chg, "power role %d not supported\n", val->intval);
		return -EINVAL;
	}

	rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
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
	rc = smblib_set_usb_pd_allowed_voltage(chg, chg->voltage_min_uv,
					       max_uv);
	if (rc < 0) {
		smblib_err(chg, "invalid min voltage %duV rc=%d\n",
			val->intval, rc);
		return rc;
	}

	chg->voltage_max_uv = max_uv;
	power_supply_changed(chg->usb_main_psy);

	return rc;
}

int smblib_set_prop_pd_active(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	if (chg->mmi.factory_mode)
		return 0;

	chg->pd_active = val->intval;

	if (chg->pd_active) {
		vote(chg->usb_irq_enable_votable, PD_VOTER, true, 0);

		/*
		 * Enforce 500mA for PD until the real vote comes in later.
		 * It is guaranteed that pd_active is set prior to
		 * pd_current_max
		 */
		vote(chg->usb_icl_votable, PD_VOTER, true, USBIN_500MA);
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
	} else {
#ifdef QCOM_BASE
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, SDP_100_MA);
#endif
		vote(chg->usb_icl_votable, PD_VOTER, false, 0);
		vote(chg->usb_irq_enable_votable, PD_VOTER, false, 0);

		/* PD hard resets failed, rerun apsd */
		if (chg->ok_to_pd) {
			chg->ok_to_pd = false;
			rc = smblib_configure_hvdcp_apsd(chg, true);
			if (rc < 0) {
				dev_err(chg->dev,
					"Couldn't enable APSD rc=%d\n", rc);
				return rc;
			}
			smblib_rerun_apsd_if_required(chg);
		}
	}

	smblib_update_usb_type(chg);
	power_supply_changed(chg->usb_psy);
	return rc;
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

int smblib_set_prop_pd_in_hard_reset(struct smb_charger *chg,
				const union power_supply_propval *val)
{
	int rc = 0;

	if (chg->pd_hard_reset == val->intval)
		return rc;

	chg->pd_hard_reset = val->intval;
	rc = smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG,
			EXIT_SNK_BASED_ON_CC_BIT,
			(chg->pd_hard_reset) ? EXIT_SNK_BASED_ON_CC_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set EXIT_SNK_BASED_ON_CC rc=%d\n",
				rc);

	return rc;
}

static int smblib_recover_from_soft_jeita(struct smb_charger *chg)
{
	u8 stat1, stat7;
	int rc;

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_1_REG, &stat1);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_1 rc=%d\n",
				rc);
		return rc;
	}

	rc = smblib_read(chg, BATTERY_CHARGER_STATUS_7_REG, &stat7);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read BATTERY_CHARGER_STATUS_2 rc=%d\n",
				rc);
		return rc;
	}

	if ((chg->jeita_status && !(stat7 & BAT_TEMP_STATUS_SOFT_LIMIT_MASK) &&
		((stat1 & BATTERY_CHARGER_STATUS_MASK) == TERMINATE_CHARGE))) {
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

	chg->jeita_status = stat7 & BAT_TEMP_STATUS_SOFT_LIMIT_MASK;

	return 0;
}

/************************
 * USB MAIN PSY GETTERS *
 ************************/
int smblib_get_prop_fcc_delta(struct smb_charger *chg,
				union power_supply_propval *val)
{
	int rc, jeita_cc_delta_ua = 0;

	if (chg->sw_jeita_enabled) {
		val->intval = 0;
		return 0;
	}

	rc = smblib_get_jeita_cc_delta(chg, &jeita_cc_delta_ua);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get jeita cc delta rc=%d\n", rc);
		jeita_cc_delta_ua = 0;
	}

	val->intval = jeita_cc_delta_ua;
	return 0;
}

/************************
 * USB MAIN PSY SETTERS *
 ************************/
int smblib_get_charge_current(struct smb_charger *chg,
				int *total_current_ua)
{
	const struct apsd_result *apsd_result = smblib_get_apsd_result(chg);
	union power_supply_propval val = {0, };
	int rc = 0, typec_source_rd, current_ua;
	bool non_compliant;
	u8 stat;

	if (chg->pd_active) {
		*total_current_ua =
			get_client_vote_locked(chg->usb_icl_votable, PD_VOTER);
		return rc;
	}

	rc = smblib_read(chg, LEGACY_CABLE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_5 rc=%d\n", rc);
		return rc;
	}
	non_compliant = stat & TYPEC_NONCOMP_LEGACY_CABLE_STATUS_BIT;

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

/**********************
 * INTERRUPT HANDLERS *
 **********************/

irqreturn_t default_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	return IRQ_HANDLED;
}

irqreturn_t smblib_handle_usbin_collapse(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc;
	union power_supply_propval val = {0, };

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Couldn't get usb present rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	if (val.intval && !chg->reverse_boost) {
		cancel_delayed_work(&chg->mmi.heartbeat_work);
		schedule_delayed_work(&chg->mmi.heartbeat_work,
				      msecs_to_jiffies(100));
	}
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	return IRQ_HANDLED;
}

irqreturn_t chg_state_change_irq_handler(int irq, void *data)
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

irqreturn_t batt_temp_changed_irq_handler(int irq, void *data)
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

irqreturn_t batt_psy_changed_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);
	power_supply_changed(chg->batt_psy);
	return IRQ_HANDLED;
}

#define AICL_STEP_MV		200
#define MAX_AICL_THRESHOLD_MV	4800
irqreturn_t usbin_uv_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	if ((chg->wa_flags & WEAK_ADAPTER_WA)
			&& is_storming(&irq_data->storm_data)) {

		if (chg->aicl_max_reached) {
			smblib_dbg(chg, PR_MISC,
					"USBIN_UV storm at max AICL threshold\n");
			return IRQ_HANDLED;
		}

		smblib_dbg(chg, PR_MISC, "USBIN_UV storm at threshold %d\n",
				chg->aicl_5v_threshold_mv);

		/* suspend USBIN before updating AICL threshold */
		vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER, true, 0);

		/* delay for VASHDN deglitch */
		msleep(20);

		if (chg->aicl_5v_threshold_mv > MAX_AICL_THRESHOLD_MV) {
			/* reached max AICL threshold */
			chg->aicl_max_reached = true;
			goto unsuspend_input;
		}

		/* Increase AICL threshold by 200mV */
		rc = smblib_set_charge_param(chg, &chg->param.aicl_5v_threshold,
				chg->aicl_5v_threshold_mv + AICL_STEP_MV);
		if (rc < 0)
			dev_err(chg->dev,
				"Error in setting AICL threshold rc=%d\n", rc);
		else
			chg->aicl_5v_threshold_mv += AICL_STEP_MV;

		rc = smblib_set_charge_param(chg,
				&chg->param.aicl_cont_threshold,
				chg->aicl_cont_threshold_mv + AICL_STEP_MV);
		if (rc < 0)
			dev_err(chg->dev,
				"Error in setting AICL threshold rc=%d\n", rc);
		else
			chg->aicl_cont_threshold_mv += AICL_STEP_MV;

unsuspend_input:
		if (chg->smb_version == PMI632_SUBTYPE)
			schgm_flash_torch_priority(chg, TORCH_BOOST_MODE);

		if (chg->aicl_max_reached) {
			smblib_dbg(chg, PR_MISC,
				"Reached max AICL threshold resctricting ICL to 100mA\n");
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					true, USBIN_100MA);
			smblib_run_aicl(chg, RESTART_AICL);
		} else {
			smblib_run_aicl(chg, RESTART_AICL);
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					false, 0);
		}

		wdata = &chg->irq_info[USBIN_UV_IRQ].irq_data->storm_data;
		reset_storm_count(wdata);
	}

	if (!chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data)
		return IRQ_HANDLED;

	wdata = &chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data->storm_data;
	reset_storm_count(wdata);
	return IRQ_HANDLED;
}

#define USB_WEAK_INPUT_UA	1400000
#define ICL_CHANGE_DELAY_MS	1000
irqreturn_t icl_change_irq_handler(int irq, void *data)
{
	u8 stat;
	int rc, settled_ua, delay = ICL_CHANGE_DELAY_MS;
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->mode == PARALLEL_MASTER) {
		/*
		 * Ignore if change in ICL is due to DIE temp mitigation.
		 * This is to prevent any further ICL split.
		 */
		if (chg->hw_die_temp_mitigation) {
			rc = smblib_read(chg, MISC_DIE_TEMP_STATUS_REG, &stat);
			if (rc < 0) {
				smblib_err(chg,
					"Couldn't read DIE_TEMP rc=%d\n", rc);
				return IRQ_HANDLED;
			}
			if (stat & (DIE_TEMP_UB_BIT | DIE_TEMP_LB_BIT)) {
				smblib_dbg(chg, PR_PARALLEL,
					"skip ICL change DIE_TEMP %x\n", stat);
				return IRQ_HANDLED;
			}
		}

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
		if (settled_ua == get_effective_result(chg->usb_icl_votable))
			delay = 0;

		cancel_delayed_work_sync(&chg->icl_change_work);
		schedule_delayed_work(&chg->icl_change_work,
						msecs_to_jiffies(delay));
	}

	return IRQ_HANDLED;
}

static void smblib_micro_usb_plugin(struct smb_charger *chg, bool vbus_rising)
{
	if (!vbus_rising) {
		smblib_update_usb_type(chg);
		smblib_notify_device_mode(chg, false);
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

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		return;
	}

	vbus_rising = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	if (!vbus_rising) {
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
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

#define PL_DELAY_MS	30000
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
	smblib_set_opt_switcher_freq(chg, vbus_rising ? chg->chg_freq.freq_5V :
						chg->chg_freq.freq_removal);

	if (vbus_rising) {
		rc = smblib_request_dpdm(chg, true);
		if (rc < 0)
			smblib_err(chg, "Couldn't to enable DPDM rc=%d\n", rc);

		/* Schedule work to enable parallel charger */
		vote(chg->awake_votable, PL_DELAY_VOTER, true, 0);
		schedule_delayed_work(&chg->pl_enable_work,
					msecs_to_jiffies(PL_DELAY_MS));

		if (chg->mmi.factory_mode)
			chg->mmi.factory_kill_armed = true;
	} else {
		if (chg->wa_flags & BOOST_BACK_WA) {
			data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
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

		if (chg->wa_flags & WEAK_ADAPTER_WA) {
			chg->aicl_5v_threshold_mv =
					chg->default_aicl_5v_threshold_mv;
			chg->aicl_cont_threshold_mv =
					chg->default_aicl_cont_threshold_mv;

			smblib_set_charge_param(chg,
					&chg->param.aicl_5v_threshold,
					chg->aicl_5v_threshold_mv);
			smblib_set_charge_param(chg,
					&chg->param.aicl_cont_threshold,
					chg->aicl_cont_threshold_mv);
			chg->aicl_max_reached = false;

			if (chg->smb_version == PMI632_SUBTYPE)
				schgm_flash_torch_priority(chg,
						TORCH_BUCK_MODE);

			data = chg->irq_info[USBIN_UV_IRQ].irq_data;
			if (data) {
				wdata = &data->storm_data;
				reset_storm_count(wdata);
			}
			vote(chg->usb_icl_votable, AICL_THRESHOLD_VOTER,
					false, 0);
		}

		rc = smblib_request_dpdm(chg, false);
		if (rc < 0)
			smblib_err(chg, "Couldn't disable DPDM rc=%d\n", rc);

		smblib_update_usb_type(chg);
		if (chg->mmi.factory_kill_armed && !factory_kill_disable) {
			smblib_err(chg, "Factory kill power off\n");
			kernel_power_off();
		} else
			chg->mmi.factory_kill_armed = false;
	}

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		smblib_micro_usb_plugin(chg, vbus_rising);

	power_supply_changed(chg->usb_psy);
	__pm_stay_awake(&chg->mmi.smblib_mmi_hb_wake_source);
	cancel_delayed_work(&chg->mmi.heartbeat_work);
	schedule_delayed_work(&chg->mmi.heartbeat_work,
			      msecs_to_jiffies(0));
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: usbin-plugin %s\n",
					vbus_rising ? "attached" : "detached");
}

irqreturn_t usb_plugin_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->pd_hard_reset)
		smblib_usb_plugin_hard_reset_locked(chg);
	else
		smblib_usb_plugin_locked(chg);

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

#define QC3_PULSES_FOR_6V	5
#define QC3_PULSES_FOR_9V	20
#define QC3_PULSES_FOR_12V	35
static void smblib_hvdcp_adaptive_voltage_change(struct smb_charger *chg)
{
	int rc;
	u8 stat;
	int pulses;

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
			smblib_set_opt_switcher_freq(chg,
					chg->chg_freq.freq_5V);
			break;
		case QC_9V_BIT:
			smblib_set_opt_switcher_freq(chg,
					chg->chg_freq.freq_9V);
			break;
		case QC_12V_BIT:
			smblib_set_opt_switcher_freq(chg,
					chg->chg_freq.freq_12V);
			break;
		default:
			smblib_set_opt_switcher_freq(chg,
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
			smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_5V);
		else if (pulses < QC3_PULSES_FOR_9V)
			smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_6V_8V);
		else if (pulses < QC3_PULSES_FOR_12V)
			smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_9V);
		else
			smblib_set_opt_switcher_freq(chg,
				chg->chg_freq.freq_12V);
	}
}

/* triggers when HVDCP 3.0 authentication has finished */
static void smblib_handle_hvdcp_3p0_auth_done(struct smb_charger *chg,
					      bool rising)
{
	const struct apsd_result *apsd_result;

	if (!rising)
		return;

	if (chg->mode == PARALLEL_MASTER)
		vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, true, 0);

	/* the APSD done handler will set the USB supply type */
	apsd_result = smblib_get_apsd_result(chg);
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-3p0-auth-done rising; %s detected\n",
		   apsd_result->name);
}

static void smblib_handle_hvdcp_check_timeout(struct smb_charger *chg,
					      bool rising, bool qc_charger)
{
	if (rising) {

		if (qc_charger) {
			/* enable HDC and ICL irq for QC2/3 charger */
			vote(chg->usb_irq_enable_votable, QC_VOTER, true, 0);
			#ifdef QCOM_BASE
			vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
				HVDCP_CURRENT_UA);
			#endif
		} else {
			/* A plain DCP, enforce DCP ICL if specified */
			vote(chg->usb_icl_votable, DCP_VOTER,
				chg->dcp_icl_ua != -EINVAL, chg->dcp_icl_ua);
		}
	}

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s %s\n", __func__,
		   rising ? "rising" : "falling");
}

/* triggers when HVDCP is detected */
static void smblib_handle_hvdcp_detect_done(struct smb_charger *chg,
					    bool rising)
{
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: hvdcp-detect-done %s\n",
		   rising ? "rising" : "falling");
}

#ifdef QCOM_BASE
static void update_sw_icl_max(struct smb_charger *chg, int pst)
{
	int typec_mode;
	int rp_ua;

	/* while PD is active it should have complete ICL control */
	if (chg->pd_active)
		return;

	/*
	 * HVDCP 2/3, handled separately
	 * For UNKNOWN(input not present) return without updating ICL
	 */
	if (pst == POWER_SUPPLY_TYPE_USB_HVDCP
			|| pst == POWER_SUPPLY_TYPE_USB_HVDCP_3
			|| pst == POWER_SUPPLY_TYPE_UNKNOWN)
		return;

	/* TypeC rp med or high, use rp value */
	typec_mode = smblib_get_prop_typec_mode(chg);
	if (typec_rp_med_high(chg, typec_mode)) {
		rp_ua = get_rp_based_dcp_current(chg, typec_mode);
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true, rp_ua);
		return;
	}

	/* rp-std or legacy, USB BC 1.2 */
	switch (pst) {
	case POWER_SUPPLY_TYPE_USB:
		/*
		 * USB_PSY will vote to increase the current to 500/900mA once
		 * enumeration is done.
		 */
		if (!is_client_vote_enabled(chg->usb_icl_votable,
						USB_PSY_VOTER)) {
			/* if flash is active force 500mA */
			vote(chg->usb_icl_votable, USB_PSY_VOTER, true,
					is_flash_active(chg) ?
					SDP_CURRENT_UA : SDP_100_MA);
		}
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, false, 0);
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					CDP_CURRENT_UA);
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					DCP_CURRENT_UA);
		break;
	case POWER_SUPPLY_TYPE_USB_FLOAT:
		/*
		 * limit ICL to 100mA, the USB driver will enumerate to check
		 * if this is a SDP and appropriately set the current
		 */
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					SDP_100_MA);
		break;
	default:
		smblib_err(chg, "Unknown APSD %d; forcing 500mA\n", pst);
		vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
					SDP_CURRENT_UA);
		break;
	}
}
#endif

static void smblib_handle_apsd_done(struct smb_charger *chg, bool rising)
{
	const struct apsd_result *apsd_result;

	if (!rising)
		return;

	apsd_result = smblib_update_usb_type(chg);

#ifdef QCOM_BASE
	update_sw_icl_max(chg, apsd_result->pst);
#endif

	switch (apsd_result->bit) {
	case SDP_CHARGER_BIT:
	case CDP_CHARGER_BIT:
	case FLOAT_CHARGER_BIT:
		if ((chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
				|| chg->use_extcon)
			smblib_notify_device_mode(chg, true);
		break;
	case OCP_CHARGER_BIT:
	case DCP_CHARGER_BIT:
		break;
	default:
		break;
	}

	chg->mmi.apsd_done = true;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: apsd-done rising; %s detected\n",
		   apsd_result->name);
}

irqreturn_t usb_source_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int rc = 0;
	u8 stat;

	/*
	 * Prepared to run PD or PD is active. At this moment, APSD is disabled,
	 * but there still can be irq on apsd_done from previously unfinished
	 * APSD run, skip it.
	 */
	if (chg->ok_to_pd)
		return IRQ_HANDLED;

	rc = smblib_read(chg, APSD_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read APSD_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}
	smblib_dbg(chg, PR_REGISTER, "APSD_STATUS = 0x%02x\n", stat);

	if ((chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		&& (stat & APSD_DTC_STATUS_DONE_BIT)
		&& !chg->uusb_apsd_rerun_done) {
		/*
		 * Force re-run APSD to handle slow insertion related
		 * charger-mis-detection.
		 */
		chg->uusb_apsd_rerun_done = true;
		smblib_rerun_apsd_if_required(chg);
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

static void typec_sink_insertion(struct smb_charger *chg)
{
	vote(chg->usb_icl_votable, OTG_VOTER, true, 0);

	if (chg->use_extcon) {
		smblib_notify_usb_host(chg, true);
		chg->otg_present = true;
	}

	if (!chg->pr_swap_in_progress)
		chg->ok_to_pd = (!(*chg->pd_disabled) || chg->early_usb_attach)
					&& !chg->pd_not_supported;
}

static void typec_src_insertion(struct smb_charger *chg)
{
	int rc = 0;
	u8 stat;

	if (chg->pr_swap_in_progress)
		return;

	rc = smblib_read(chg, LEGACY_CABLE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATE_MACHINE_STATUS_REG rc=%d\n",
			rc);
		return;
	}

	chg->typec_legacy = stat & TYPEC_LEGACY_CABLE_STATUS_BIT;
	chg->ok_to_pd = (!(chg->typec_legacy || *chg->pd_disabled)
			|| chg->early_usb_attach) && !chg->pd_not_supported;
	if (!chg->ok_to_pd) {
		rc = smblib_configure_hvdcp_apsd(chg, true);
		if (rc < 0) {
			dev_err(chg->dev,
				"Couldn't enable APSD rc=%d\n", rc);
			return;
		}
		smblib_rerun_apsd_if_required(chg);
	}
}

static void typec_sink_removal(struct smb_charger *chg)
{
	vote(chg->usb_icl_votable, OTG_VOTER, false, 0);

	if (chg->use_extcon) {
		if (chg->otg_present)
			smblib_notify_usb_host(chg, false);
		chg->otg_present = false;
	}
}

static void typec_src_removal(struct smb_charger *chg)
{
	int rc;
	struct smb_irq_data *data;
	struct storm_watch *wdata;
	union power_supply_propval val;

	/* disable apsd */
	rc = smblib_configure_hvdcp_apsd(chg, false);
	if (rc < 0)
		smblib_err(chg, "Couldn't disable APSD rc=%d\n", rc);

	smblib_update_usb_type(chg);

	if (chg->wa_flags & BOOST_BACK_WA) {
		data = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq_data;
		if (data) {
			wdata = &data->storm_data;
			update_storm_count(wdata, WEAK_CHG_STORM_COUNT);
			vote(chg->usb_icl_votable, BOOST_BACK_VOTER, false, 0);
			vote(chg->usb_icl_votable, WEAK_CHARGER_VOTER,
					false, 0);
		}
	}

	cancel_delayed_work_sync(&chg->pl_enable_work);

	/* reset input current limit voters */
#ifdef QCOM_BASE
	vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER, true,
			is_flash_active(chg) ? SDP_CURRENT_UA : SDP_100_MA);
#endif
	vote(chg->usb_icl_votable, PD_VOTER, false, 0);
	vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);
	vote(chg->usb_icl_votable, DCP_VOTER, false, 0);
	vote(chg->usb_icl_votable, PL_USBIN_USBIN_VOTER, false, 0);
	vote(chg->usb_icl_votable, SW_QC3_VOTER, false, 0);
	vote(chg->usb_icl_votable, OTG_VOTER, false, 0);
	vote(chg->usb_icl_votable, CTM_VOTER, false, 0);

	rc = smblib_get_prop_usb_present(chg, &val);
	if (rc < 0) {
		smblib_err(chg, "Error getting USB Present rc = %d\n", rc);
		chg->mmi.apsd_done = false;
		chg->mmi.charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;
	} else if (!val.intval) {
		chg->mmi.apsd_done = false;
		chg->mmi.charger_rate = POWER_SUPPLY_CHARGE_RATE_NONE;
	}

	/* reset usb irq voters */
	vote(chg->usb_irq_enable_votable, PD_VOTER, false, 0);
	vote(chg->usb_irq_enable_votable, QC_VOTER, false, 0);

	/* reset parallel voters */
	vote(chg->pl_disable_votable, PL_DELAY_VOTER, true, 0);
	vote(chg->pl_disable_votable, PL_FCC_LOW_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_I_VOTER, false, 0);
	vote(chg->pl_enable_votable_indirect, USBIN_V_VOTER, false, 0);
	vote(chg->awake_votable, PL_DELAY_VOTER, false, 0);

	/* reset USBOV votes and cancel work */
	cancel_delayed_work_sync(&chg->usbov_dbc_work);
	vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
	chg->dbc_usbov = false;

	chg->pulse_cnt = 0;
	chg->usb_icl_delta_ua = 0;
	chg->voltage_min_uv = MICRO_5V;
	chg->voltage_max_uv = MICRO_9V;

	/* write back the default FLOAT charger configuration */
	rc = smblib_masked_write(chg, USBIN_OPTIONS_2_CFG_REG,
				(u8)FLOAT_OPTIONS_MASK, chg->float_cfg);
	if (rc < 0)
		smblib_err(chg, "Couldn't write float charger options rc=%d\n",
			rc);

	/* reconfigure allowed voltage for HVDCP */
	rc = smblib_set_adapter_allowance(chg,
			USBIN_ADAPTER_ALLOW_5V_TO_9V);
	if (rc < 0)
		smblib_err(chg, "Couldn't set USBIN_ADAPTER_ALLOW_5V_TO_9V rc=%d\n",
			rc);

	if (chg->use_extcon)
		smblib_notify_device_mode(chg, false);

	chg->typec_legacy = false;

	chg->mmi.charging_limit_modes = CHARGING_LIMIT_OFF;
	chg->mmi.hvdcp3_con = false;
	chg->mmi.vbus_inc_cnt = 0;
	vote(chg->awake_votable, HEARTBEAT_VOTER, true, true);
	cancel_delayed_work(&chg->mmi.heartbeat_work);
	schedule_delayed_work(&chg->mmi.heartbeat_work,
			      msecs_to_jiffies(0));
}

#ifdef QCOM_BASE
static void smblib_handle_rp_change(struct smb_charger *chg, int typec_mode)
{
	const struct apsd_result *apsd = smblib_get_apsd_result(chg);

	/*
	 * We want the ICL vote @ 100mA for a FLOAT charger
	 * until the detection by the USB stack is complete.
	 * Ignore the Rp changes unless there is a
	 * pre-existing valid vote or FLOAT is configured for
	 * SDP current.
	 */
	if (apsd->pst == POWER_SUPPLY_TYPE_USB_FLOAT) {
		if (get_client_vote(chg->usb_icl_votable, SW_ICL_MAX_VOTER)
					<= USBIN_100MA
			|| (chg->float_cfg & FLOAT_OPTIONS_MASK)
					== FORCE_FLOAT_SDP_CFG_BIT)
		return;
	}

	update_sw_icl_max(chg, apsd->pst);

	smblib_dbg(chg, PR_MISC, "CC change old_mode=%d new_mode=%d\n",
						chg->typec_mode, typec_mode);
}
#endif

irqreturn_t typec_or_rid_detection_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		if (chg->moisture_protection_enabled &&
				(chg->wa_flags & MOISTURE_PROTECTION_WA)) {
			/*
			 * Adding pm_stay_awake as because pm_relax is called
			 * on exit path from the work routine.
			 */
			pm_stay_awake(chg->dev);
			schedule_work(&chg->moisture_protection_work);
		}

		cancel_delayed_work_sync(&chg->uusb_otg_work);
		/*
		 * Skip OTG enablement if RID interrupt triggers with moisture
		 * protection still enabled.
		 */
		if (!chg->moisture_present) {
			vote(chg->awake_votable, OTG_DELAY_VOTER, true, 0);
			smblib_dbg(chg, PR_INTERRUPT, "Scheduling OTG work\n");
			schedule_delayed_work(&chg->uusb_otg_work,
					msecs_to_jiffies(chg->otg_delay_ms));
		}

		return IRQ_HANDLED;
	}

	return IRQ_HANDLED;
}

irqreturn_t typec_state_change_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	int typec_mode, icl_vote_ma;
	bool typec_chg = false;

	if (chg->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		smblib_dbg(chg, PR_INTERRUPT,
				"Ignoring for micro USB\n");
		return IRQ_HANDLED;
	}

	typec_mode = smblib_get_prop_typec_mode(chg);
	if (chg->sink_src_mode != UNATTACHED_MODE
			&& (typec_mode != chg->typec_mode))
#ifdef QCOM_BASE
		smblib_handle_rp_change(chg, typec_mode);
#else
		typec_chg = true;
#endif

	chg->typec_mode = typec_mode;

	if (typec_chg) {
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

	if (chg->dr_inst)
		dual_role_instance_changed(chg->dr_inst);

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: cc-state-change; Type-C %s detected\n",
				smblib_typec_mode_name[chg->typec_mode]);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

irqreturn_t typec_attach_detach_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	int rc;

	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s\n", irq_data->name);

	rc = smblib_read(chg, TYPE_C_STATE_MACHINE_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATE_MACHINE_STATUS_REG rc=%d\n",
			rc);
		return IRQ_HANDLED;
	}

	if (stat & TYPEC_ATTACH_DETACH_STATE_BIT) {
		rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_MISC_STATUS_REG rc=%d\n",
				rc);
			return IRQ_HANDLED;
		}

		if (stat & SNK_SRC_MODE_BIT) {
			chg->sink_src_mode = SRC_MODE;
			typec_sink_insertion(chg);
		} else {
			chg->sink_src_mode = SINK_MODE;
			typec_src_insertion(chg);
		}

	} else {
		switch (chg->sink_src_mode) {
		case SRC_MODE:
			typec_sink_removal(chg);
			break;
		case SINK_MODE:
			typec_src_removal(chg);
			break;
		default:
			break;
		}

		if (!chg->pr_swap_in_progress) {
			chg->ok_to_pd = false;
			chg->sink_src_mode = UNATTACHED_MODE;
			chg->early_usb_attach = false;
		}
	}

	if (chg->dr_inst)
		dual_role_instance_changed(chg->dr_inst);

	power_supply_changed(chg->usb_psy);

	return IRQ_HANDLED;
}

irqreturn_t dc_plugin_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;

	power_supply_changed(chg->dc_psy);
	return IRQ_HANDLED;
}

irqreturn_t high_duty_cycle_irq_handler(int irq, void *data)
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
irqreturn_t switcher_power_ok_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	struct storm_watch *wdata = &irq_data->storm_data;
	int pok_irq = chg->irq_info[SWITCHER_POWER_OK_IRQ].irq;
	int rc;
	u8 stat, susp;

	if (!(chg->wa_flags & BOOST_BACK_WA))
		return IRQ_HANDLED;

	rc = smblib_read(chg, POWER_PATH_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read POWER_PATH_STATUS rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	rc = smblib_read(chg, USBIN_CMD_IL_REG, &susp);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USBIN_CMD_IL rc=%d\n", rc);
		return IRQ_HANDLED;
	}

	/* skip suspending input if its already suspended by some other voter */
	if ((stat & USE_USBIN_BIT) && (susp & USBIN_SUSPEND_BIT))
		return IRQ_HANDLED;

	if (stat & USE_DCIN_BIT)
		return IRQ_HANDLED;

	if (is_storming(&irq_data->storm_data)) {
		chg->reverse_boost = true;
		stat = 0;
		rc = smblib_read(chg, TYPE_C_SNK_STATUS_REG, &stat);
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

irqreturn_t wdog_bark_irq_handler(int irq, void *data)
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

static void smblib_usbov_dbc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						usbov_dbc_work.work);

	smblib_dbg(chg, PR_MISC, "Resetting USBOV debounce\n");
	chg->dbc_usbov = false;
	vote(chg->awake_votable, USBOV_DBC_VOTER, false, 0);
}

irqreturn_t usbin_ov_irq_handler(int irq, void *data)
{
	struct smb_irq_data *irq_data = data;
	struct smb_charger *chg = irq_data->parent_data;
	u8 stat;
	int rc;

	if (!(chg->wa_flags & USBIN_OV_WA))
		goto out;

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		goto out;
	}
	smblib_dbg(chg, PR_INTERRUPT, "IRQ: %s stat=%x\n", irq_data->name,
				!!stat);

	if (stat & USBIN_OV_RT_STS_BIT) {
		chg->dbc_usbov = true;
		vote(chg->awake_votable, USBOV_DBC_VOTER, true, 0);
		schedule_delayed_work(&chg->usbov_dbc_work,
				msecs_to_jiffies(1000));
	} else {
		cancel_delayed_work_sync(&chg->usbov_dbc_work);
		chg->dbc_usbov = false;
		vote(chg->awake_votable, USBOV_DBC_VOTER, true, 0);
	}

out:
	smblib_dbg(chg, PR_MISC, "USBOV debounce status %d\n",
				chg->dbc_usbov);
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
	u8 stat = 0, orientation;

	chg->pr_swap_in_progress = val->intval;

	rc = smblib_masked_write(chg, TYPE_C_DEBOUNCE_OPTION_REG,
			REDUCE_TCCDEBOUNCE_TO_2MS_BIT,
			val->intval ? REDUCE_TCCDEBOUNCE_TO_2MS_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set tCC debounce rc=%d\n", rc);

	rc = smblib_masked_write(chg, TYPE_C_EXIT_STATE_CFG_REG,
			BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT,
			val->intval ? BYPASS_VSAFE0V_DURING_ROLE_SWAP_BIT : 0);
	if (rc < 0)
		smblib_err(chg, "Couldn't set exit state cfg rc=%d\n", rc);

	if (chg->pr_swap_in_progress) {
		rc = smblib_read(chg, TYPE_C_MISC_STATUS_REG, &stat);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_STATUS_4 rc=%d\n",
				rc);
		}

		orientation =
			stat & CC_ORIENTATION_BIT ? TYPEC_CCOUT_VALUE_BIT : 0;
		rc = smblib_masked_write(chg, TYPE_C_CCOUT_CONTROL_REG,
			TYPEC_CCOUT_SRC_BIT | TYPEC_CCOUT_BUFFER_EN_BIT
					| TYPEC_CCOUT_VALUE_BIT,
			TYPEC_CCOUT_SRC_BIT | TYPEC_CCOUT_BUFFER_EN_BIT
					| orientation);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_CCOUT_CONTROL_REG rc=%d\n",
				rc);
		}
	} else {
		rc = smblib_masked_write(chg, TYPE_C_CCOUT_CONTROL_REG,
			TYPEC_CCOUT_SRC_BIT, 0);
		if (rc < 0) {
			smblib_err(chg, "Couldn't read TYPE_C_CCOUT_CONTROL_REG rc=%d\n",
				rc);
		}

		/* enable DRP */
		rc = smblib_masked_write(chg, TYPE_C_MODE_CFG_REG,
				 TYPEC_POWER_ROLE_CMD_MASK, 0);
		if (rc < 0)
			smblib_err(chg, "Couldn't enable DRP rc=%d\n", rc);
	}

	return 0;
}

/***************
 * Work Queues *
 ***************/
static void smblib_uusb_otg_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						uusb_otg_work.work);
	int rc;
	u8 stat;
	bool otg;

	rc = smblib_read(chg, TYPEC_U_USB_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_STATUS_3 rc=%d\n", rc);
		goto out;
	}
	otg = !!(stat & U_USB_GROUND_NOVBUS_BIT);
	if (chg->otg_present != otg)
		smblib_notify_usb_host(chg, otg);
	else
		goto out;

	chg->otg_present = otg;
	if (!otg)
		chg->boost_current_ua = 0;

	rc = smblib_set_charge_param(chg, &chg->param.freq_switcher,
				otg ? chg->chg_freq.freq_below_otg_threshold
					: chg->chg_freq.freq_removal);
	if (rc < 0)
		dev_err(chg->dev, "Error in setting freq_boost rc=%d\n", rc);

	smblib_dbg(chg, PR_REGISTER, "TYPE_C_U_USB_STATUS = 0x%02x OTG=%d\n",
			stat, otg);
	power_supply_changed(chg->usb_psy);

out:
	vote(chg->awake_votable, OTG_DELAY_VOTER, false, 0);
}

static void bms_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						bms_update_work);

	smblib_suspend_on_debug_battery(chg);

	if (chg->batt_psy)
		power_supply_changed(chg->batt_psy);
}

static void pl_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						pl_update_work);

	smblib_stat_sw_override_cfg(chg, false);
}

static void clear_hdc_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						clear_hdc_work.work);

	chg->is_hdc = 0;
	if (chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq)
		enable_irq(chg->irq_info[HIGH_DUTY_CYCLE_IRQ].irq);
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

#define MOISTURE_PROTECTION_CHECK_DELAY_MS 300000		/* 5 mins */
static void smblib_moisture_protection_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						moisture_protection_work);
	int rc;
	bool usb_plugged_in;
	u8 stat;

	/*
	 * Disable 1% duty cycle on CC_ID pin and enable uUSB factory mode
	 * detection to track any change on RID, as interrupts are disable.
	 */
	rc = smblib_write(chg, TYPEC_U_USB_WATER_PROTECTION_CFG_REG, 0);
	if (rc < 0) {
		smblib_err(chg, "Couldn't disable periodic monitoring of CC_ID rc=%d\n",
			rc);
		goto out;
	}

	rc = smblib_masked_write(chg, TYPEC_U_USB_CFG_REG,
					EN_MICRO_USB_FACTORY_MODE_BIT,
					EN_MICRO_USB_FACTORY_MODE_BIT);
	if (rc < 0) {
		smblib_err(chg, "Couldn't enable uUSB factory mode detection rc=%d\n",
			rc);
		goto out;
	}

	/*
	 * Add a delay of 100ms to allow change in rid to reflect on
	 * status registers.
	 */
	msleep(100);

	rc = smblib_read(chg, USBIN_BASE + INT_RT_STS_OFFSET, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read USB_INT_RT_STS rc=%d\n", rc);
		goto out;
	}
	usb_plugged_in = (bool)(stat & USBIN_PLUGIN_RT_STS_BIT);

	/* Check uUSB status for moisture presence */
	rc = smblib_read(chg, TYPEC_U_USB_STATUS_REG, &stat);
	if (rc < 0) {
		smblib_err(chg, "Couldn't read TYPE_C_U_USB_STATUS_REG rc=%d\n",
				rc);
		goto out;
	}

	/*
	 * Factory mode detection happens in case of USB plugged-in by using
	 * a different current source of 2uA which can hamper moisture
	 * detection. Since factory mode is not supported in kernel, factory
	 * mode detection can be considered as equivalent to presence of
	 * moisture.
	 */
	if (stat == U_USB_STATUS_WATER_PRESENT || stat == U_USB_FMB1_BIT ||
			stat == U_USB_FMB2_BIT || (usb_plugged_in &&
			stat == U_USB_FLOAT1_BIT)) {
		smblib_set_moisture_protection(chg, true);
		alarm_start_relative(&chg->moisture_protection_alarm,
			ms_to_ktime(MOISTURE_PROTECTION_CHECK_DELAY_MS));
	} else {
		smblib_set_moisture_protection(chg, false);
		rc = alarm_cancel(&chg->moisture_protection_alarm);
		if (rc < 0)
			smblib_err(chg, "Couldn't cancel moisture protection alarm\n");
	}

out:
	pm_relax(chg->dev);
}

static enum alarmtimer_restart moisture_protection_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct smb_charger *chg = container_of(alarm, struct smb_charger,
					moisture_protection_alarm);

	smblib_dbg(chg, PR_MISC, "moisture Protection Alarm Triggered %lld\n",
			ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chg->dev);
	schedule_work(&chg->moisture_protection_work);

	return ALARMTIMER_NORESTART;
}

#define JEITA_SOFT			0
#define JEITA_HARD			1
static int smblib_update_jeita(struct smb_charger *chg, u32 *thresholds,
								int type)
{
	int rc;
	u16 temp, base;

	base = CHGR_JEITA_THRESHOLD_BASE_REG(type);

	temp = thresholds[1] & 0xFFFF;
	temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
	rc = smblib_batch_write(chg, base, (u8 *)&temp, 2);
	if (rc < 0) {
		smblib_err(chg,
			"Couldn't configure Jeita %s hot threshold rc=%d\n",
			(type == JEITA_SOFT) ? "Soft" : "Hard", rc);
		return rc;
	}

	temp = thresholds[0] & 0xFFFF;
	temp = ((temp & 0xFF00) >> 8) | ((temp & 0xFF) << 8);
	rc = smblib_batch_write(chg, base + 2, (u8 *)&temp, 2);
	if (rc < 0) {
		smblib_err(chg,
			"Couldn't configure Jeita %s cold threshold rc=%d\n",
			(type == JEITA_SOFT) ? "Soft" : "Hard", rc);
		return rc;
	}

	smblib_dbg(chg, PR_MISC, "%s Jeita threshold configured\n",
				(type == JEITA_SOFT) ? "Soft" : "Hard");

	return 0;
}

static void jeita_update_work(struct work_struct *work)
{
	struct smb_charger *chg = container_of(work, struct smb_charger,
						jeita_update_work);
	struct device_node *node = chg->dev->of_node;
	struct device_node *batt_node, *pnode;
	union power_supply_propval val;
	int rc;
	u32 jeita_thresholds[2];

	batt_node = of_find_node_by_name(node, "qcom,battery-data");
	if (!batt_node) {
		smblib_err(chg, "Batterydata not available\n");
		goto out;
	}

	/* if BMS is not ready, defer the work */
	if (!chg->bms_psy)
		return;

	rc = smblib_get_prop_from_bms(chg,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &val);
	if (rc < 0) {
		smblib_err(chg, "Failed to get batt-id rc=%d\n", rc);
		goto out;
	}

	/* if BMS hasn't read out the batt_id yet, defer the work */
	if (val.intval <= 0)
		return;

	pnode = of_batterydata_get_best_profile(batt_node,
					val.intval / 1000, NULL);
	if (IS_ERR(pnode)) {
		rc = PTR_ERR(pnode);
		smblib_err(chg, "Failed to detect valid battery profile %d\n",
				rc);
		goto out;
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-hard-thresholds",
				jeita_thresholds, 2);
	if (!rc) {
		rc = smblib_update_jeita(chg, jeita_thresholds, JEITA_HARD);
		if (rc < 0) {
			smblib_err(chg, "Couldn't configure Hard Jeita rc=%d\n",
					rc);
			goto out;
		}
	}

	rc = of_property_read_u32_array(pnode, "qcom,jeita-soft-thresholds",
				jeita_thresholds, 2);
	if (!rc) {
		rc = smblib_update_jeita(chg, jeita_thresholds, JEITA_SOFT);
		if (rc < 0) {
			smblib_err(chg, "Couldn't configure Soft Jeita rc=%d\n",
					rc);
			goto out;
		}
	}

out:
	chg->jeita_configured = true;
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
	if (chg->usb_icl_votable == NULL) {
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
		chg->dc_suspend_votable = NULL;
		return rc;
	}

	chg->awake_votable = create_votable("AWAKE", VOTE_SET_ANY,
					smblib_awake_vote_callback,
					chg);
	if (IS_ERR(chg->awake_votable)) {
		rc = PTR_ERR(chg->awake_votable);
		chg->awake_votable = NULL;
		return rc;
	}

	chg->chg_disable_votable = create_votable("CHG_DISABLE", VOTE_SET_ANY,
					smblib_chg_disable_vote_callback,
					chg);
	if (IS_ERR(chg->chg_disable_votable)) {
		rc = PTR_ERR(chg->chg_disable_votable);
		chg->chg_disable_votable = NULL;
		return rc;
	}

	chg->usb_irq_enable_votable = create_votable("USB_IRQ_DISABLE",
					VOTE_SET_ANY,
					smblib_usb_irq_enable_vote_callback,
					chg);
	if (IS_ERR(chg->usb_irq_enable_votable)) {
		rc = PTR_ERR(chg->usb_irq_enable_votable);
		chg->usb_irq_enable_votable = NULL;
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
	if (chg->awake_votable)
		destroy_votable(chg->awake_votable);
	if (chg->chg_disable_votable)
		destroy_votable(chg->chg_disable_votable);
}

int smblib_init(struct smb_charger *chg)
{
	int rc = 0;

	device_init_wakeup(chg->dev, true);
	mutex_init(&chg->lock);
	INIT_WORK(&chg->bms_update_work, bms_update_work);
	INIT_WORK(&chg->pl_update_work, pl_update_work);
	INIT_WORK(&chg->jeita_update_work, jeita_update_work);
	INIT_DELAYED_WORK(&chg->clear_hdc_work, clear_hdc_work);
	INIT_DELAYED_WORK(&chg->icl_change_work, smblib_icl_change_work);
	INIT_DELAYED_WORK(&chg->pl_enable_work, smblib_pl_enable_work);
	INIT_DELAYED_WORK(&chg->uusb_otg_work, smblib_uusb_otg_work);
	INIT_DELAYED_WORK(&chg->bb_removal_work, smblib_bb_removal_work);
	INIT_DELAYED_WORK(&chg->usbov_dbc_work, smblib_usbov_dbc_work);

	if (chg->moisture_protection_enabled &&
				(chg->wa_flags & MOISTURE_PROTECTION_WA)) {
		INIT_WORK(&chg->moisture_protection_work,
					smblib_moisture_protection_work);

		if (alarmtimer_get_rtcdev()) {
			alarm_init(&chg->moisture_protection_alarm,
				ALARM_BOOTTIME, moisture_protection_alarm_cb);
		} else {
			smblib_err(chg, "Failed to initialize moisture protection alarm\n");
			return -ENODEV;
		}
	}

	chg->fake_capacity = -EINVAL;
	chg->fake_input_current_limited = -EINVAL;
	chg->fake_batt_status = -EINVAL;
	chg->jeita_configured = false;
	chg->sink_src_mode = UNATTACHED_MODE;

	switch (chg->mode) {
	case PARALLEL_MASTER:
		rc = qcom_batt_init(chg->smb_version);
		if (rc < 0) {
			smblib_err(chg, "Couldn't init qcom_batt_init rc=%d\n",
				rc);
			return rc;
		}

		rc = qcom_step_chg_init(chg->dev, chg->step_chg_enabled,
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

		chg->bms_psy = power_supply_get_by_name("bms");
		chg->pl.psy = power_supply_get_by_name("parallel");
		if (chg->pl.psy) {
			rc = smblib_stat_sw_override_cfg(chg, false);
			if (rc < 0) {
				smblib_err(chg,
					"Couldn't config stat sw rc=%d\n", rc);
				return rc;
			}
		}
		rc = smblib_register_notifier(chg);
		if (rc < 0) {
			smblib_err(chg,
				"Couldn't register notifier rc=%d\n", rc);
			return rc;
		}
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
		if (chg->moisture_protection_enabled &&
				(chg->wa_flags & MOISTURE_PROTECTION_WA)) {
			alarm_cancel(&chg->moisture_protection_alarm);
			cancel_work_sync(&chg->moisture_protection_work);
		}
		cancel_work_sync(&chg->bms_update_work);
		cancel_work_sync(&chg->jeita_update_work);
		cancel_work_sync(&chg->pl_update_work);
		cancel_delayed_work_sync(&chg->clear_hdc_work);
		cancel_delayed_work_sync(&chg->icl_change_work);
		cancel_delayed_work_sync(&chg->pl_enable_work);
		cancel_delayed_work_sync(&chg->uusb_otg_work);
		cancel_delayed_work_sync(&chg->bb_removal_work);
		cancel_delayed_work_sync(&chg->usbov_dbc_work);
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

	return 0;
}

/*********************
 * MMI Functionality *
 *********************/

static struct smb_charger *mmi_chip;

static char *stepchg_str[] = {
	[STEP_MAX]		= "MAX",
	[STEP_NORM]		= "NORMAL",
	[STEP_FULL]		= "FULL",
	[STEP_FLOAT]		= "FLOAT",
	[STEP_DEMO]		= "DEMO",
	[STEP_STOP]		= "STOP",
	[STEP_NONE]		= "NONE",
};

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
		if (temp_c <= max_temp - HYSTERISIS_DEGC)
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
#define TAPER_DROP_MA 100
static bool mmi_has_current_tapered(struct smb_charger *chip,
				    int batt_ma, int taper_ma)
{
	bool change_state = false;
	int allowed_fcc, target_ma;

	if (!chip) {
		smblib_dbg(chip, PR_MOTO, "called before chip valid!\n");
		return false;
	}

	allowed_fcc = get_effective_result(chip->fcc_votable) / 1000;

	if (allowed_fcc >= taper_ma)
		target_ma = taper_ma;
	else
		target_ma = allowed_fcc - TAPER_DROP_MA;

	if (batt_ma < 0) {
		batt_ma *= -1;
		if (batt_ma <= target_ma)
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

	if (chip->typec_mode == POWER_SUPPLY_TYPEC_SOURCE_HIGH  ||
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
			smblib_err(chip, "Error getting AICL Settled rc = %d\n", rc);
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
#define HEARTBEAT_DELAY_MS 60000
#define HEARTBEAT_HOLDOFF_MS 10000
#define HEARTBEAT_EB_WAIT_MS 1000
#define HYST_STEP_MV 50
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
#define REV_BST_MA -10
static void mmi_heartbeat_work(struct work_struct *work)
{
	struct smb_charger *chip = container_of(work,
						struct smb_charger,
						mmi.heartbeat_work.work);
	int rc;
	int batt_mv;
	int batt_ma;
	int batt_soc;
	int batt_temp;
	int usb_mv;
	u8 apsd_reg;
	bool icl_override;
	static int vbus_inc_mv = VBUS_INPUT_VOLTAGE_TARGET;
	bool vbus_inc_now = false;
	int vbus_present = 0;
	int charger_present = 0;
	int cl_usb = -EINVAL;
	int cl_pd = -EINVAL;
	int cl_cc = -EINVAL;

	int prev_step;
	int hb_resch_time;

	union power_supply_propval val;
	struct mmi_params *mmi = &chip->mmi;
	struct mmi_temp_zone *zone;
	int max_fv_mv = -EINVAL;
	int target_usb = -EINVAL;
	int target_fcc = -EINVAL;
	int target_fv = -EINVAL;
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
			typec_src_removal(chip);
	} else if (chip->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		charger_present = 1;
		mmi->charger_debounce_cnt = 0;
	} else if (mmi->charger_debounce_cnt < CHARGER_DETECTION_DONE) {
		mmi->charger_debounce_cnt++;
		/* Set the USB CL to 500 only if pd is not active to avoid
		 * PD Compliance issues */
		if (!chip->pd_active)
			cl_usb = 500;
		if ((chip->typec_mode == POWER_SUPPLY_TYPEC_NONE) ||
		    (chip->typec_mode ==
		     POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER)) {
			mmi->charger_debounce_cnt = CHARGER_DETECTION_DONE;
			charger_present = 1;
			mmi->apsd_done = true;
		}
	} else if (mmi->charger_debounce_cnt == CHARGER_DETECTION_DONE)
		charger_present = 1;

	rc = smblib_get_prop_from_bms(chip,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting Batt Voltage rc = %d\n", rc);
		goto end_hb;
	} else
		batt_mv = val.intval / 1000;

	rc = smblib_get_prop_from_bms(chip,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
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

	rc = smblib_get_prop_from_bms(chip,
				POWER_SUPPLY_PROP_TEMP, &val);
	if (rc < 0) {
		smblib_err(chip, "Error getting Batt Temperature rc = %d\n", rc);
		goto end_hb;
	} else
		batt_temp = val.intval / 10;

	rc = smblib_get_prop_usb_voltage_now(chip, &val);
	if (rc < 0) {
		pr_err("Error getting USB Voltage rc = %d\n", rc);
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
		icl_override = !!(apsd_reg & APSD_RESULT_STATUS_7_BIT);
		apsd_reg &= APSD_RESULT_STATUS_MASK;
	}

	smblib_dbg(chip, PR_MISC, "batt=%d mV, %d mA, %d C, USB= %d mV\n",
		batt_mv, batt_ma, batt_temp, usb_mv);

	pok_irq = chip->irq_info[SWITCHER_POWER_OK_IRQ].irq;
	if (chip->reverse_boost || (batt_mv >= usb_mv && vbus_present)) {
		if (((usb_mv < REV_BST_THRESH) &&
		    ((prev_vbus_mv - REV_BST_DROP) > usb_mv)) ||
		    (batt_ma > REV_BST_MA)) {
			smblib_err(chip,
				   "Reverse Boosted: Clear, USB Suspend\n");
			if (chip->mmi.factory_mode)
				smblib_set_usb_suspend(chip, true);
			else
				vote(chip->usb_icl_votable, BOOST_BACK_VOTER,
				     true, 0);
			msleep(50);
			if (chip->mmi.factory_mode)
				smblib_set_usb_suspend(chip,
					chip->mmi.force_chg_suspend);
			else
				vote(chip->usb_icl_votable, BOOST_BACK_VOTER,
				     false, 0);
			if (chip->reverse_boost) {
				chip->reverse_boost = false;
				enable_irq(pok_irq);
			}
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
			if (vbus_present) {
				if (mmi->hvdcp3_con)
					cl_cc = 3000;
				else
					cl_cc = 500;
			} else
				cl_cc = 0;

			if (vbus_present &&
			    !icl_override &&
			    (apsd_reg == 0)) {
				rc = smblib_masked_write(chip, CMD_ICL_OVERRIDE,
							 ICL_OVERRIDE_BIT,
							 ICL_OVERRIDE_BIT);
				if (rc < 0)
					smblib_err(chip,
						   "Fail ICL Over rc%d\n", rc);
			}
			break;
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
			if (apsd_reg & OCP_CHARGER_BIT)
				cl_usb = 1000;
			else
#ifdef CONFIG_DCP_2A_SUPPORT
				cl_usb = 1800;
#else
				cl_usb = 1500;
#endif
		else if (chip->real_charger_type ==
			 POWER_SUPPLY_TYPE_USB_CDP)
			cl_usb = 1500;
		else if ((cl_cc == 500) &&
			 (cl_usb <= 500))
			cl_usb = cl_cc;
		else if (cl_usb <= 0)
			cl_usb = 500;

		if (cl_usb >= TURBO_CHRG_THRSH)
			mmi->charger_rate = POWER_SUPPLY_CHARGE_RATE_TURBO;
	}

	if (!mmi->temp_zones) {
		vote(chip->usb_icl_votable, HEARTBEAT_VOTER, cl_usb >= 0,
					(cl_usb * 1000));
		pr_warn("EFFECTIVE: FV = %d, CDIS = %d, FCC = %d, "
		   "USBICL = %d, USBCICL = %d\n",
		   get_effective_result(chip->fv_votable),
		   get_effective_result(chip->chg_disable_votable),
		   get_effective_result(chip->fcc_votable),
		   get_effective_result(chip->usb_icl_votable),
		   cl_cc);
		goto end_hb;
	}

	if (mmi->enable_charging_limit && mmi->is_factory_image)
		update_charging_limit_modes(chip, batt_soc);

	mmi_find_temp_zone(chip, batt_temp);
	zone = &mmi->temp_zones[mmi->pres_temp_zone];

	if (mmi->base_fv_mv == 0) {
		mmi->base_fv_mv = get_client_vote(chip->fv_votable,
						  BATT_PROFILE_VOTER) / 1000;
		vote(chip->fv_votable,
		     BATT_PROFILE_VOTER, false, 0);
	}
	max_fv_mv = mmi->base_fv_mv;

	/* Determine Next State */
	prev_step = chip->mmi.pres_chrg_step;

	if (mmi->charging_limit_modes == CHARGING_LIMIT_RUN)
		pr_warn("Factory Mode/Image so Limiting Charging!!!\n");

	if (!charger_present) {
		mmi->pres_chrg_step = STEP_NONE;
	} else if ((mmi->pres_temp_zone == ZONE_HOT) ||
		   (mmi->pres_temp_zone == ZONE_COLD) ||
		   (mmi->charging_limit_modes == CHARGING_LIMIT_RUN)) {
		chip->mmi.pres_chrg_step = STEP_STOP;
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
			vote(chip->usb_icl_votable, DEMO_VOTER, false, 0);
			vote(chip->dc_suspend_votable, DEMO_VOTER, false, 0);
			usb_suspend = 0;
			mmi->chrg_taper_cnt = 0;
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
			mmi->pres_chrg_step = STEP_FULL;
		}
	} else if (mmi->pres_chrg_step == STEP_FULL) {
		if (batt_soc <= 99) {
			mmi->chrg_taper_cnt = 0;
			mmi->pres_chrg_step = STEP_NORM;
		}
	} else if (mmi->pres_chrg_step == STEP_FLOAT) {
		if ((zone->fcc_norm_ma) ||
		    ((batt_mv + HYST_STEP_MV) < zone->norm_mv))
			mmi->pres_chrg_step = STEP_MAX;

	}

	/* Take State actions */
	switch (mmi->pres_chrg_step) {
	case STEP_FLOAT:
	case STEP_MAX:
		if (!zone->norm_mv)
			target_fv = max_fv_mv;
		else
			target_fv = zone->norm_mv;
		target_fcc = zone->fcc_max_ma;
		target_usb = cl_usb;
		break;
	case STEP_FULL:
		target_fv = max_fv_mv;
		target_fcc = -EINVAL;
		target_usb = cl_usb;
		break;
	case STEP_NORM:
		target_fv = max_fv_mv + mmi->vfloat_comp_mv;
		target_fcc = zone->fcc_norm_ma;
		target_usb = cl_usb;
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
		target_usb = cl_usb;
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

	if (charger_present && mmi->hvdcp3_con && !chip->pd_active &&
	    (vbus_inc_mv > VBUS_INPUT_VOLTAGE_NOM)) {
		if ((usb_mv < vbus_inc_mv) &&
		    (usb_mv >= VBUS_INPUT_VOLTAGE_MIN) &&
		    (mmi->vbus_inc_cnt < VBUS_INPUT_MAX_COUNT)) {
			pr_warn("HVDCP Input %d mV Low, Increase\n", usb_mv);
			smblib_dp_pulse(chip);
			vbus_inc_now = true;
			mmi->vbus_inc_cnt++;
		} else if ((usb_mv > VBUS_INPUT_VOLTAGE_MAX)
				&& (mmi->vbus_inc_cnt > 0)) {
			smblib_dbg(chip, PR_MOTO,
				   "HVDCP Input %d mV High, Decrease\n",
				   usb_mv);
			vbus_inc_mv -= 50;
			smblib_dm_pulse(chip);
			vbus_inc_now = true;
			mmi->vbus_inc_cnt--;
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
		"PMI Input %d mA, USBC CL %d mA, FV %d mV, FCC %d mA\n",
		 target_usb, cl_cc, target_fv, target_fcc);
	smblib_dbg(chip, PR_MOTO, "Step State = %s\n",
		stepchg_str[(int)mmi->pres_chrg_step]);
	pr_warn("EFFECTIVE: FV = %d, CDIS = %d, FCC = %d, "
		"USBICL = %d\n",
		get_effective_result(chip->fv_votable),
		get_effective_result(chip->chg_disable_votable),
		get_effective_result(chip->fcc_votable),
		get_effective_result(chip->usb_icl_votable));
end_hb:
	if (chip->batt_psy)
		power_supply_changed(chip->batt_psy);

	hb_resch_time = HEARTBEAT_HOLDOFF_MS;

	if (!chip->mmi.chrg_taper_cnt && (rc >= 0))
		hb_resch_time = HEARTBEAT_DELAY_MS;

	if ((!charger_present && mmi->charger_debounce_cnt) ||
	    vbus_inc_now)
		hb_resch_time = HEARTBEAT_EB_WAIT_MS;

	schedule_delayed_work(&mmi->heartbeat_work,
			      msecs_to_jiffies(hb_resch_time));

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
	smblib_dbg(chg, PR_MISC, "SMB Reboot\n");
	if (!chg) {
		smblib_dbg(chg, PR_MOTO, "called before chip valid!\n");
		return NOTIFY_DONE;
	}

	atomic_set(&chg->mmi.hb_ready, 0);
	cancel_delayed_work_sync(&chg->mmi.heartbeat_work);

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

			smblib_dbg(chg, PR_MOTO,"VBUS UV wait 1 sec!\n");
			/* Delay 1 sec to allow more VBUS decay */
			msleep(1000);
			/* configure power role for UFP */
			smblib_masked_write(chg,
				TYPE_C_MODE_CFG_REG,
				TYPEC_POWER_ROLE_CMD_MASK,
				EN_SNK_ONLY_BIT);
			break;
		default:
			break;
		}
	}

	return NOTIFY_DONE;
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

static ssize_t factory_charge_upper_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int state;

	if (!mmi_chip) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	state = mmi_chip->mmi.upper_limit_capacity;

	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(factory_charge_upper, 0444,
		factory_charge_upper_show,
		NULL);

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
		smblib_err(mmi_chip,"chip not valid\n");
		return -ENODEV;
	}
	mmi_chip->mmi.force_chg_suspend = (bool)mode;
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
		smblib_err(mmi_chip,"chip not valid\n");
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
		pr_err("chip not valid\n");
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
		smblib_err(mmi_chip, "Invalid ibatt value = %lu\n", chg_current);
		return -EINVAL;
	}

	if (!mmi_chip) {
		smblib_err(mmi_chip, "chip not valid\n");
		return -ENODEV;
	}

	chg_current *= 1000; /* Convert to uA */
	r = smblib_set_charge_param(mmi_chip, &mmi_chip->param.fcc, chg_current);
	if (r < 0) {
		smblib_err(mmi_chip, "Factory Couldn't set master fcc = %d rc=%d\n",
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
		smblib_err(mmi_chip, "Factory Couldn't get master fcc rc=%d\n", (int)ret);
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
		smblib_err(mmi_chip, "Factory Couldn't set usb icl = %d rc=%d\n",
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
		smblib_err(mmi_chip, "Factory Couldn't get usb_icl rc=%d\n", (int)r);
		return r;
	}
	state /= 1000; /* Convert to mA */
end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_iusb, 0664,
		force_chg_iusb_show,
		force_chg_iusb_store);

#define PRE_CHARGE_CONV_MV 50
#define PRE_CHARGE_MAX 0x7
static ssize_t force_chg_itrick_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned long r;
	unsigned long chg_current;
	u8 value;

	r = kstrtoul(buf, 0, &chg_current);
	if (r) {
		smblib_err(mmi_chip, "Invalid pre-charge value = %lu\n", chg_current);
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

	r = smblib_masked_write(mmi_chip, CHGR_PRE_CHARGE_CURRENT_CFG_REG,
				CHGR_PRE_CHARGE_CURRENT_SETTING_MASK,
				value);
	if (r < 0) {
		smblib_err(mmi_chip, "Factory Couldn't set ITRICK %d  mV rc=%d\n",
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

	ret = smblib_read(mmi_chip, CHGR_PRE_CHARGE_CURRENT_CFG_REG, &value);
	if (ret) {
		smblib_err(mmi_chip, "Pre Chg ITrick failed ret = %d\n", ret);
		state = -EFAULT;
		goto end;
	}

	value &= CHGR_PRE_CHARGE_CURRENT_SETTING_MASK;

	state = value * PRE_CHARGE_CONV_MV;

end:
	return scnprintf(buf, CHG_SHOW_MAX_SIZE, "%d\n", state);
}

static DEVICE_ATTR(force_chg_itrick, 0664,
		   force_chg_itrick_show,
		   force_chg_itrick_store);

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

enum dual_role_property smblib_typec_dr_properties[] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

static int smblib_typec_dr_is_writeable(
			struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop)
{
	int rc = 0;
	struct smb_charger *chip = dual_role_get_drvdata(dual_role);

	if (!chip || (chip->typec_mode == POWER_SUPPLY_TYPEC_NONE))
		return 0;

	if (chip->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB)
		return 0;

	/* FIXME:
	 * How to decide the connected devices support the role swap ?
	 */
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
	case DUAL_ROLE_PROP_PR:
		rc = 0;
		break;
	default:
		rc = 0;
	}
	return rc;
}

static int smblib_typec_dr_set_property(
			struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop,
			const unsigned int *val)
{
	int rc = 0;
	union power_supply_propval propval = {0,};
	struct smb_charger *chip = dual_role_get_drvdata(dual_role);

	if (!chip || (chip->typec_mode == POWER_SUPPLY_TYPEC_NONE))
		return -EINVAL;

	/* FIXME:
	 * Need dual role state sync during swap
	 */
	switch (prop) {
	case DUAL_ROLE_PROP_PR:
		switch (*val) {
		case DUAL_ROLE_PROP_PR_SRC:
			propval.intval = POWER_SUPPLY_TYPEC_PR_SOURCE;
			break;
		case DUAL_ROLE_PROP_PR_SNK:
			propval.intval = POWER_SUPPLY_TYPEC_PR_SINK;
			break;
		default:
			propval.intval = POWER_SUPPLY_TYPEC_PR_DUAL;
		}
		rc = smblib_set_prop_typec_power_role(chip, &propval);
		break;
	default:
		pr_debug("Invalid DUAL ROLE request %d\n", prop);
		rc = -EINVAL;
	}

	return rc;
}

static int smblib_typec_dr_get_property(
			struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop,
			unsigned int *val)
{
	int rc = 0;
	struct smb_charger *chip = dual_role_get_drvdata(dual_role);
	unsigned int mode, power_role, data_role;

	if (!chip)
		return -EINVAL;

	if (chip->connector_type == POWER_SUPPLY_CONNECTOR_MICRO_USB) {
		union power_supply_propval val;

		rc = smblib_get_prop_usb_present(chip, &val);
		if (rc < 0) {
			smblib_err(chip, "Error getting USB Present rc = %d\n",
				rc);
			return rc;
		}

		if (chip->otg_present) {
			mode = DUAL_ROLE_PROP_MODE_DFP;
			power_role = DUAL_ROLE_PROP_PR_SRC;
			data_role = DUAL_ROLE_PROP_DR_HOST;
		} else if (val.intval > 0) {
			mode = DUAL_ROLE_PROP_MODE_UFP;
			power_role = DUAL_ROLE_PROP_PR_SNK;
			data_role = DUAL_ROLE_PROP_DR_DEVICE;
		} else {
			mode = DUAL_ROLE_PROP_MODE_NONE;
			power_role = DUAL_ROLE_PROP_PR_NONE;
			data_role = DUAL_ROLE_PROP_DR_NONE;
		}
	} else {
		switch (chip->typec_mode) {
		case POWER_SUPPLY_TYPEC_SINK:
		case POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE:
		case POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY:
		case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		case POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY:
			mode = DUAL_ROLE_PROP_MODE_DFP;
			power_role = DUAL_ROLE_PROP_PR_SRC;
			data_role = DUAL_ROLE_PROP_DR_HOST;
			break;
		case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
		case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
		case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		case POWER_SUPPLY_TYPEC_NON_COMPLIANT:
			mode = DUAL_ROLE_PROP_MODE_UFP;
			power_role = DUAL_ROLE_PROP_PR_SNK;
			data_role = DUAL_ROLE_PROP_DR_DEVICE;
			break;
		default:
			mode = DUAL_ROLE_PROP_MODE_NONE;
			power_role = DUAL_ROLE_PROP_PR_NONE;
			data_role = DUAL_ROLE_PROP_DR_NONE;
		}
	}

	switch (prop) {
	case DUAL_ROLE_PROP_SUPPORTED_MODES:
		*val = chip->dr_desc.supported_modes;
		break;
	case DUAL_ROLE_PROP_MODE:
		*val = mode;
		break;
	case DUAL_ROLE_PROP_PR:
		*val = power_role;
		break;
	case DUAL_ROLE_PROP_DR:
		*val = data_role;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int smblib_typec_dual_role_init(struct smb_charger *chip)
{
	int rc = 0;

	/* Register for android TypeC dual role framework */
	chip->dr_desc.name		= "otg_default";
	chip->dr_desc.properties	= smblib_typec_dr_properties;
	chip->dr_desc.get_property	= smblib_typec_dr_get_property;
	chip->dr_desc.set_property	= smblib_typec_dr_set_property;
	chip->dr_desc.property_is_writeable =
				smblib_typec_dr_is_writeable;
	chip->dr_desc.supported_modes	=
				DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	chip->dr_desc.num_properties	=
				ARRAY_SIZE(smblib_typec_dr_properties);

	chip->dr_inst = devm_dual_role_instance_register(chip->dev,
				&chip->dr_desc);
	if (IS_ERR(chip->dr_inst)) {
		pr_err("Failed to initialize dual role\n");
		rc = PTR_ERR(chip->dr_inst);
	} else {
		chip->dr_inst->drv_data = chip;
	}

	return rc;
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
				"Couldn't read mmi temp zones rc = %d\n", rc);
			return rc;
		}
		smblib_err(chg,
			"mmi temp zones: Num: %d\n", chg->mmi.num_temp_zones);
		for (i = 0; i < chg->mmi.num_temp_zones; i++) {
			smblib_err(chg,
				"mmi temp zones: Zone %d, Temp %d C, "
				"Step Volt %d mV, Full Rate %d mA, "
				"Taper Rate %d mA\n", i,
				chg->mmi.temp_zones[i].temp_c,
				chg->mmi.temp_zones[i].norm_mv,
				chg->mmi.temp_zones[i].fcc_max_ma,
				chg->mmi.temp_zones[i].fcc_norm_ma);
		}
		chg->mmi.pres_temp_zone = ZONE_NONE;
	} else {
		chg->mmi.temp_zones = NULL;
		chg->mmi.num_temp_zones = 0;
		dev_err(chg->dev, "mmi temp zones is not set\n");
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
				"Couldn't read usb therm limits rc = %d\n", rc);
			return rc;
		}
	} else {
		chg->mmi.usb_thermal_mitigation = NULL;
		chg->mmi.usb_thermal_levels = 0;
		dev_err(chg->dev, "usb-thermal-mitigation is not set\n");
	}

	rc = of_property_read_u32(node, "qcom,iterm-ma",
				  &chg->mmi.chrg_iterm);
	if (rc)
		chg->mmi.chrg_iterm = 150;

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

	INIT_DELAYED_WORK(&chg->mmi.heartbeat_work, mmi_heartbeat_work);
	wakeup_source_init(&chg->mmi.smblib_mmi_hb_wake_source,
			   "smblib_mmi_hb_wake");
	alarm_init(&chg->mmi.heartbeat_alarm, ALARM_BOOTTIME,
		   mmi_heartbeat_alarm_cb);

	rc = parse_mmi_dt(chg);
	if (rc < 0)
		smblib_err(chg, "Error getting mmi dt items\n");

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

	rc = device_create_file(chg->dev,
				&dev_attr_factory_charge_upper);
	if (rc)
		smblib_err(chg, "couldn't create factory_charge_upper\n");

	if (chg->mmi.factory_mode) {
		mmi_chip = chg;
		smblib_err(chg, "Entering Factory Mode SMB!\n");

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_usb_suspend);
		if (rc) {
			smblib_err(chg, "couldn't create force_chg_usb_suspend\n");
		}

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_fail_clear);
		if (rc) {
			smblib_err(chg, "couldn't create force_chg_fail_clear\n");
		}

		rc = device_create_file(chg->dev,
					&dev_attr_force_chg_auto_enable);
		if (rc) {
			smblib_err(chg, "couldn't create force_chg_auto_enable\n");
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

	/* Turn Jeita OFF */
	rc = smblib_masked_write(chg, JEITA_EN_CFG_REG,
				 0x3F,
				 0x00);
	if (rc)
		smblib_err(chg, "couldn't set JEITA CFG\n");

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
}
