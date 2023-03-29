/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file lim_process_cfg_updates.cc contains the utility functions
 * to handle various CFG parameter update events
 * Author:        Chandra Modumudi
 * Date:          01/20/03
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "ani_global.h"

#include "wni_cfg.h"
#include "sir_mac_prot_def.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_prop_exts_utils.h"
#include "sch_api.h"
#include "rrm_api.h"

static void lim_update_config(struct mac_context *mac, struct pe_session *pe_session);

void lim_set_cfg_protection(struct mac_context *mac, struct pe_session *pesessionEntry)
{
	uint32_t val = 0;
	struct wlan_mlme_cfg *mlme_cfg = mac->mlme_cfg;

	if (pesessionEntry && LIM_IS_AP_ROLE(pesessionEntry)) {
		if (pesessionEntry->gLimProtectionControl ==
		    MLME_FORCE_POLICY_PROTECTION_DISABLE)
			qdf_mem_zero((void *)&pesessionEntry->cfgProtection,
				    sizeof(tCfgProtection));
		else {
			pe_debug("frm11a = %d, from11b = %d, frm11g = %d, "
				   "ht20 = %d, nongf = %d, lsigTxop = %d, "
				   "rifs = %d, obss = %d",
				pesessionEntry->cfgProtection.fromlla,
				pesessionEntry->cfgProtection.fromllb,
				pesessionEntry->cfgProtection.fromllg,
				pesessionEntry->cfgProtection.ht20,
				pesessionEntry->cfgProtection.nonGf,
				pesessionEntry->cfgProtection.lsigTxop,
				pesessionEntry->cfgProtection.rifs,
				pesessionEntry->cfgProtection.obss);
		}
	} else {
		mac->lim.gLimProtectionControl =
			mlme_cfg->sap_protection_cfg.protection_force_policy;


		if (mac->lim.gLimProtectionControl ==
		    MLME_FORCE_POLICY_PROTECTION_DISABLE)
			qdf_mem_zero((void *)&mac->lim.cfgProtection,
				    sizeof(tCfgProtection));
		else {
			val = mlme_cfg->sap_protection_cfg.protection_enabled;

			mac->lim.cfgProtection.fromlla =
				(val >> MLME_PROTECTION_ENABLED_FROM_llA) & 1;
			mac->lim.cfgProtection.fromllb =
				(val >> MLME_PROTECTION_ENABLED_FROM_llB) & 1;
			mac->lim.cfgProtection.fromllg =
				(val >> MLME_PROTECTION_ENABLED_FROM_llG) & 1;
			mac->lim.cfgProtection.ht20 =
				(val >> MLME_PROTECTION_ENABLED_HT_20) & 1;
			mac->lim.cfgProtection.nonGf =
				(val >> MLME_PROTECTION_ENABLED_NON_GF) & 1;
			mac->lim.cfgProtection.lsigTxop =
				(val >> MLME_PROTECTION_ENABLED_LSIG_TXOP) & 1;
			mac->lim.cfgProtection.rifs =
				(val >> MLME_PROTECTION_ENABLED_RIFS) & 1;
			mac->lim.cfgProtection.obss =
				(val >> MLME_PROTECTION_ENABLED_OBSS) & 1;

		}
	}
}

/**
 * lim_handle_param_update()
 *
 ***FUNCTION:
 * This function is use to post a message whenever need indicate
 * there is update of config parameter.
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  mac  - Pointer to Global MAC structure
 * @param  cfgId - ID of CFG parameter that got updated
 * @return None
 */
void lim_handle_param_update(struct mac_context *mac, eUpdateIEsType cfgId)
{
	struct scheduler_msg msg = { 0 };
	QDF_STATUS status;

	pe_debug("Handling CFG parameter id %X update", cfgId);

	switch (cfgId) {
	case eUPDATE_IE_PROBE_BCN:
	{
		msg.type = SIR_LIM_UPDATE_BEACON;
		status = lim_post_msg_api(mac, &msg);

		if (status != QDF_STATUS_SUCCESS)
			pe_err("Failed lim_post_msg_api %u", status);
			break;
	}
	default:
		break;
	}
}

/**
 * lim_apply_configuration()
 *
 ***FUNCTION:
 * This function is called to apply the configured parameters
 * before joining or reassociating with a BSS or starting a BSS.
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  mac  - Pointer to Global MAC structure
 * @return None
 */

void lim_apply_configuration(struct mac_context *mac, struct pe_session *pe_session)
{
	uint32_t phyMode;

	pe_session->limSentCapsChangeNtf = false;

	lim_get_phy_mode(mac, &phyMode, pe_session);

	lim_update_config(mac, pe_session);

	lim_get_short_slot_from_phy_mode(mac, pe_session, phyMode,
					 &pe_session->shortSlotTimeSupported);

	lim_set_cfg_protection(mac, pe_session);

	/* Added for BT - AMP Support */
	if (LIM_IS_AP_ROLE(pe_session)) {
		/* This check is required to ensure the beacon generation is not done
		   as a part of join request for a BT-AMP station */

		if (pe_session->statypeForBss == STA_ENTRY_SELF) {
			sch_set_beacon_interval(mac, pe_session);
			sch_set_fixed_beacon_fields(mac, pe_session);
		}
	}
} /*** end lim_apply_configuration() ***/

/**
 * lim_update_config
 *
 * FUNCTION:
 * Update the local state from CFG database
 * (This used to be dphUpdateConfig)
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

static void lim_update_config(struct mac_context *mac, struct pe_session *pe_session)
{
	bool enabled;

	pe_session->beaconParams.fShortPreamble =
					mac->mlme_cfg->ht_caps.short_preamble;

	/* In STA case this parameter is filled during the join request */
	if (LIM_IS_AP_ROLE(pe_session)) {
		enabled = mac->mlme_cfg->wmm_params.wme_enabled;
		pe_session->limWmeEnabled = enabled;
	}
	enabled = mac->mlme_cfg->wmm_params.wsm_enabled;
	pe_session->limWsmEnabled = enabled;

	if ((!pe_session->limWmeEnabled) && (pe_session->limWsmEnabled)) {
		pe_err("Can't enable WSM without WME");
		pe_session->limWsmEnabled = 0;
	}
	/* In STA , this parameter is filled during the join request */
	if (LIM_IS_AP_ROLE(pe_session)) {
		enabled = mac->mlme_cfg->wmm_params.qos_enabled;
		pe_session->limQosEnabled = enabled;
	}
	pe_session->limHcfEnabled = mac->mlme_cfg->feature_flags.enable_hcf;

	/* AP: WSM should enable HCF as well, for STA enable WSM only after */
	/* association response is received */
	if (pe_session->limWsmEnabled && LIM_IS_AP_ROLE(pe_session))
		pe_session->limHcfEnabled = 1;

	pe_debug("Updated Lim shadow state based on CFG");
}
