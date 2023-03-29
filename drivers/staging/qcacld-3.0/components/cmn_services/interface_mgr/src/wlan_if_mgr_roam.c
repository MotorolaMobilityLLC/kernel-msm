/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DOC: contains interface manager roam public api
 */
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_cm_roam_api.h"
#include "wlan_if_mgr_main.h"
#include "wlan_p2p_ucfg_api.h"
#include "cds_api.h"
#include "sme_api.h"
#include "wlan_vdev_mgr_utils_api.h"
#include "wni_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"

static void if_mgr_enable_roaming_on_vdev(struct wlan_objmgr_pdev *pdev,
					  void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct change_roam_state_arg *roam_arg = arg;
	uint8_t vdev_id, curr_vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);
	curr_vdev_id = roam_arg->curr_vdev_id;

	if (curr_vdev_id != vdev_id &&
	    vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE &&
	    vdev->vdev_mlme.mlme_state == WLAN_VDEV_S_UP) {
		ifmgr_debug("Roaming enabled on vdev_id %d", vdev_id);
		wlan_cm_enable_rso(pdev, vdev_id,
				   roam_arg->requestor,
				   REASON_DRIVER_ENABLED);
	}
}

QDF_STATUS if_mgr_enable_roaming(struct wlan_objmgr_pdev *pdev,
				 struct wlan_objmgr_vdev *vdev,
				 enum wlan_cm_rso_control_requestor requestor)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct change_roam_state_arg roam_arg;
	uint8_t vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);

	roam_arg.requestor = requestor;
	roam_arg.curr_vdev_id = vdev_id;

	status = wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
						if_mgr_enable_roaming_on_vdev,
						&roam_arg, 0,
						WLAN_IF_MGR_ID);

	return status;
}

static void if_mgr_disable_roaming_on_vdev(struct wlan_objmgr_pdev *pdev,
					   void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct change_roam_state_arg *roam_arg = arg;
	uint8_t vdev_id, curr_vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);
	curr_vdev_id = roam_arg->curr_vdev_id;

	if (curr_vdev_id != vdev_id &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE &&
	    vdev->vdev_mlme.mlme_state == WLAN_VDEV_S_UP) {
		/* IFMGR Verification: Temporary call to sme_stop_roaming api,
		 * will be replaced by converged roaming api
		 * once roaming testing is complete.
		 */
		ifmgr_debug("Roaming disabled on vdev_id %d", vdev_id);
		wlan_cm_disable_rso(pdev, vdev_id,
				    roam_arg->requestor,
				    REASON_DRIVER_DISABLED);
	}
}

QDF_STATUS if_mgr_disable_roaming(struct wlan_objmgr_pdev *pdev,
				  struct wlan_objmgr_vdev *vdev,
				  enum wlan_cm_rso_control_requestor requestor)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct change_roam_state_arg roam_arg;
	uint8_t vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);

	roam_arg.requestor = requestor;
	roam_arg.curr_vdev_id = vdev_id;

	status = wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
						if_mgr_disable_roaming_on_vdev,
						&roam_arg, 0,
						WLAN_IF_MGR_ID);

	return status;
}

QDF_STATUS
if_mgr_enable_roaming_on_connected_sta(struct wlan_objmgr_pdev *pdev,
				       struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	vdev_id = wlan_vdev_get_id(vdev);

	if (policy_mgr_is_sta_active_connection_exists(psoc) &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		ifmgr_debug("Enable roaming on connected sta for vdev_id %d", vdev_id);
		wlan_cm_enable_roaming_on_connected_sta(pdev, vdev_id);
		policy_mgr_set_pcl_for_connected_vdev(psoc, vdev_id, true);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS if_mgr_enable_roaming_after_p2p_disconnect(
				struct wlan_objmgr_pdev *pdev,
				struct wlan_objmgr_vdev *vdev,
				enum wlan_cm_rso_control_requestor requestor)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc;
	struct change_roam_state_arg roam_arg;
	uint8_t vdev_id;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	vdev_id = wlan_vdev_get_id(vdev);

	roam_arg.requestor = requestor;
	roam_arg.curr_vdev_id = vdev_id;

	/*
	 * Due to audio share glitch with P2P clients due
	 * to roam scan on concurrent interface, disable
	 * roaming if "p2p_disable_roam" ini is enabled.
	 * Re-enable roaming again once the p2p client
	 * gets disconnected.
	 */
	if (ucfg_p2p_is_roam_config_disabled(psoc) &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_CLIENT_MODE) {
		ifmgr_debug("P2P client disconnected, enable roam");
		status = wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					      if_mgr_enable_roaming_on_vdev,
					      &roam_arg, 0,
					      WLAN_IF_MGR_ID);
	}

	return status;
}

/**
 * if_mgr_calculate_mcc_beacon_interval() - Calculates the new beacon interval
 * @sta_bi: station beacon interval
 * @go_given_bi: P2P GO's given beacon interval
 *
 * This function has 3 stages. First it modifies the input go_given_bi to be
 * within 100 to 199. Then it checks if the sta_bi and go_given_bi are multiples
 * of each other. If they are, that means the 2 values are compatible, and just
 * return as is, otherwise, find new compatible BI for P2P GO
 *
 * Return: valid beacon interval value
 */
static uint16_t if_mgr_calculate_mcc_beacon_interval(uint16_t sta_bi,
						     uint16_t go_given_bi)
{
	uint8_t num_beacons, is_multiple;
	uint16_t go_calculated_bi, go_final_bi, sta_calculated_bi;

	/* ensure BI ranges between 100 and 200 */
	if (go_given_bi < 100)
		go_calculated_bi = 100;
	else
		go_calculated_bi = 100 + (go_given_bi % 100);

	if (sta_bi == 0) {
		/* There is possibility to receive zero as value.
		 * Which will cause divide by zero. Hence initialise with 100
		 */
		sta_bi = 100;
		ifmgr_warn("sta_bi 2nd parameter is zero, initialize to %d",
			   sta_bi);
	}
	/* check, if either one is multiple of another */
	if (sta_bi > go_calculated_bi)
		is_multiple = !(sta_bi % go_calculated_bi);
	else
		is_multiple = !(go_calculated_bi % sta_bi);

	/* if it is multiple, then accept GO's beacon interval
	 * range [100,199] as it is
	 */
	if (is_multiple)
		return go_calculated_bi;

	/* else , if it is not multiple, then then check for number of beacons
	 * to be inserted based on sta BI
	 */
	num_beacons = sta_bi / 100;
	if (num_beacons) {
		/* GO's final beacon interval will be aligned to sta beacon
		 * interval, but in the range of [100, 199].
		 */
		sta_calculated_bi = sta_bi / num_beacons;
		go_final_bi = sta_calculated_bi;
	} else {
		/* if STA beacon interval is less than 100, use GO's change
		 * beacon interval instead of updating to STA's beacon interval.
		 */
		go_final_bi = go_calculated_bi;
	}

	return go_final_bi;
}

static QDF_STATUS
if_mgr_send_chng_mcc_beacon_interval(struct wlan_objmgr_vdev *vdev,
				     struct beacon_interval_arg *bss_arg)
{
	struct scheduler_msg msg = {0};
	struct wlan_change_bi *p_msg;
	uint16_t len = 0;
	QDF_STATUS status;
	uint8_t *mac_addr;

	if (!bss_arg->update_beacon_interval)
		return QDF_STATUS_SUCCESS;

	bss_arg->update_beacon_interval = false;

	len = sizeof(*p_msg);
	p_msg = qdf_mem_malloc(len);
	if (!p_msg)
		return QDF_STATUS_E_NOMEM;

	p_msg->message_type = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
	p_msg->length = len;

	mac_addr = wlan_vdev_get_hw_macaddr(vdev);
	qdf_mem_copy(&p_msg->bssid, mac_addr, QDF_MAC_ADDR_SIZE);
	ifmgr_debug(QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(mac_addr));
	p_msg->session_id = wlan_vdev_get_id(vdev);
	ifmgr_debug("session %d BeaconInterval %d", p_msg->session_id,
			bss_arg->bss_beacon_interval);
	p_msg->beacon_interval = bss_arg->bss_beacon_interval;

	msg.type = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
	msg.bodyval = 0;
	msg.bodyptr = p_msg;

	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);

	if (status != QDF_STATUS_SUCCESS)
		qdf_mem_free(p_msg);

	return status;
}

static void if_mgr_update_beacon_interval(struct wlan_objmgr_pdev *pdev,
					  void *object, void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	uint8_t allow_mcc_go_diff_bi;
	struct wlan_objmgr_peer *peer;
	enum wlan_peer_type bss_persona;
	struct beacon_interval_arg *bss_arg = arg;
	struct wlan_objmgr_vdev *vdev = object;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return;

	policy_mgr_get_allow_mcc_go_diff_bi(psoc, &allow_mcc_go_diff_bi);

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_IF_MGR_ID);
	if (!peer)
		return;

	bss_persona = wlan_peer_get_peer_type(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_IF_MGR_ID);

	/*
	 * If GO in MCC support different beacon interval,
	 * change the BI of the P2P-GO
	 */
	if (bss_persona == WLAN_PEER_P2P_GO)
		return;
	/*
	 * Handle different BI scenario based on the
	 * configuration set. If Config is not set to 0x04 then
	 * Disconnect all the P2P clients associated. If config
	 * is set to 0x04 then update the BI without
	 * disconnecting all the clients
	 */
	if (allow_mcc_go_diff_bi == ALLOW_MCC_GO_DIFF_BI_NO_DISCONNECT &&
	    bss_arg->update_beacon_interval) {
		bss_arg->status =
			if_mgr_send_chng_mcc_beacon_interval(vdev, bss_arg);
		return;
	} else if (bss_arg->update_beacon_interval) {
		/*
		 * If the configuration of fAllowMCCGODiffBI is set to
		 * other than 0x04
		 */
		bss_arg->status = wlan_sap_disconnect_all_p2p_client(vdev_id);
		return;
	}
}

static QDF_STATUS
if_mgr_update_mcc_p2p_beacon_interval(struct wlan_objmgr_vdev *vdev,
				      struct beacon_interval_arg *bss_arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	uint8_t enable_mcc_mode;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/* If MCC is not supported just break and return SUCCESS */
	wlan_mlme_get_mcc_feature(psoc, &enable_mcc_mode);
	if (!enable_mcc_mode)
		return QDF_STATUS_E_FAILURE;

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  if_mgr_update_beacon_interval,
					  bss_arg, 0, WLAN_IF_MGR_ID);

	return bss_arg->status;
}

static bool if_mgr_validate_sta_bcn_intrvl(struct wlan_objmgr_vdev *vdev,
					   struct beacon_interval_arg *bss_arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct vdev_mlme_obj *vdev_mlme;
	struct wlan_objmgr_peer *peer;
	uint16_t new_bcn_interval;
	uint32_t beacon_interval;
	struct wlan_channel *chan;
	enum QDF_OPMODE curr_persona;
	enum wlan_peer_type bss_persona;
	uint8_t allow_mcc_go_diff_bi;
	uint8_t conc_rule1 = 0, conc_rule2 = 0;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return false;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_IF_MGR_ID);
	if (!peer)
		return false;

	curr_persona = wlan_vdev_mlme_get_opmode(vdev);
	bss_persona = wlan_peer_get_peer_type(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_IF_MGR_ID);

	if (curr_persona == QDF_P2P_CLIENT_MODE) {
		ifmgr_debug("Bcn Intrvl validation not require for STA/CLIENT");
		return false;
	}

	chan = wlan_vdev_get_active_channel(vdev);
	if (!chan) {
		ifmgr_err("failed to get active channel");
		return false;
	}

	vdev_mlme =
		wlan_objmgr_vdev_get_comp_private_obj(vdev,
						      WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		QDF_ASSERT(0);
		return false;
	}

	wlan_util_vdev_mlme_get_param(vdev_mlme, WLAN_MLME_CFG_BEACON_INTERVAL,
				      &beacon_interval);

	if (bss_persona == WLAN_PEER_AP &&
	    (chan->ch_cfreq1 != bss_arg->ch_freq ||
	     chan->ch_cfreq2 != bss_arg->ch_freq)) {
		ifmgr_debug("*** MCC with SAP+STA sessions ****");
		bss_arg->status = QDF_STATUS_SUCCESS;
		return true;
	}

	if (bss_persona == WLAN_PEER_P2P_GO &&
	    (chan->ch_cfreq1 != bss_arg->ch_freq ||
	     chan->ch_cfreq2 != bss_arg->ch_freq) &&
	    beacon_interval != bss_arg->bss_beacon_interval) {
		policy_mgr_get_allow_mcc_go_diff_bi(psoc,
						    &allow_mcc_go_diff_bi);

		switch (allow_mcc_go_diff_bi) {
		case ALLOW_MCC_GO_DIFF_BI_WFA_CERT:
			bss_arg->status = QDF_STATUS_SUCCESS;
			return true;
		case ALLOW_MCC_GO_DIFF_BI_WORKAROUND:
			/* fall through */
		case ALLOW_MCC_GO_DIFF_BI_NO_DISCONNECT:
			policy_mgr_get_conc_rule1(psoc, &conc_rule1);
			policy_mgr_get_conc_rule2(psoc, &conc_rule2);
			if (conc_rule1 || conc_rule2)
				new_bcn_interval = CUSTOM_CONC_GO_BI;
			else
				new_bcn_interval =
					if_mgr_calculate_mcc_beacon_interval(
						bss_arg->bss_beacon_interval,
						beacon_interval);

			ifmgr_debug("Peer AP BI : %d, new Beacon Interval: %d",
				    bss_arg->bss_beacon_interval,
				    new_bcn_interval);

			/* Update the beacon interval */
			if (new_bcn_interval != beacon_interval) {
				ifmgr_err("Beacon Interval got changed config used: %d",
					  allow_mcc_go_diff_bi);
				bss_arg->bss_beacon_interval = new_bcn_interval;
				bss_arg->update_beacon_interval = true;
				bss_arg->status =
					if_mgr_update_mcc_p2p_beacon_interval(
								vdev,
								bss_arg);
				return true;
			}
			bss_arg->status = QDF_STATUS_SUCCESS;
			return true;
		case ALLOW_MCC_GO_DIFF_BI_TEAR_DOWN:
			bss_arg->update_beacon_interval = false;
			bss_arg->status = wlan_sap_stop_bss(vdev_id);
			return true;
		default:
			ifmgr_err("BcnIntrvl is diff can't connect to preferred AP");
			bss_arg->status = QDF_STATUS_E_FAILURE;
			return true;
		}
	}
	return false;
}

static bool if_mgr_validate_p2pcli_bcn_intrvl(struct wlan_objmgr_vdev *vdev,
				       struct beacon_interval_arg *bss_arg)
{
	enum QDF_OPMODE curr_persona;
	enum wlan_peer_type bss_persona;
	uint32_t beacon_interval;
	struct wlan_channel *chan;
	struct wlan_objmgr_peer *peer;
	struct vdev_mlme_obj *vdev_mlme;

	curr_persona = wlan_vdev_mlme_get_opmode(vdev);

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_IF_MGR_ID);
	if (!peer)
		return false;

	bss_persona = wlan_peer_get_peer_type(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_IF_MGR_ID);

	chan = wlan_vdev_get_active_channel(vdev);
	if (!chan) {
		ifmgr_err("failed to get active channel");
		return false;
	}

	vdev_mlme =
		wlan_objmgr_vdev_get_comp_private_obj(vdev,
						      WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		QDF_ASSERT(0);
		return false;
	}

	wlan_util_vdev_mlme_get_param(vdev_mlme, WLAN_MLME_CFG_BEACON_INTERVAL,
				      &beacon_interval);

	if (curr_persona == QDF_STA_MODE) {
		ifmgr_debug("Ignore Beacon Interval Validation...");
	} else if (bss_persona == WLAN_PEER_P2P_GO) {
		if ((chan->ch_cfreq1 != bss_arg->ch_freq ||
		     chan->ch_cfreq2 != bss_arg->ch_freq) &&
		    beacon_interval != bss_arg->bss_beacon_interval) {
			ifmgr_err("BcnIntrvl is diff can't connect to P2P_GO network");
			bss_arg->status = QDF_STATUS_E_FAILURE;
			return true;
		}
	}

	return false;
}

static bool
if_mgr_validate_p2pgo_bcn_intrvl(struct wlan_objmgr_vdev *vdev,
				 struct beacon_interval_arg *bss_arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct vdev_mlme_obj *vdev_mlme;
	enum QDF_OPMODE curr_persona;
	uint32_t beacon_interval;
	struct wlan_channel *chan;
	uint8_t conc_rule1 = 0, conc_rule2 = 0;
	uint16_t new_bcn_interval;

	curr_persona = wlan_vdev_mlme_get_opmode(vdev);

	chan = wlan_vdev_get_active_channel(vdev);
	if (!chan) {
		ifmgr_err("failed to get active channel");
		return false;
	}

	vdev_mlme =
		wlan_objmgr_vdev_get_comp_private_obj(vdev,
						      WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		QDF_ASSERT(0);
		return false;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		return false;

	wlan_util_vdev_mlme_get_param(vdev_mlme, WLAN_MLME_CFG_BEACON_INTERVAL,
				      &beacon_interval);

	if ((curr_persona == QDF_P2P_CLIENT_MODE) ||
	    (curr_persona == QDF_STA_MODE)) {
		/* check for P2P_client scenario */
		if ((chan->ch_cfreq1 == 0) && (chan->ch_cfreq2 == 0) &&
		    (beacon_interval == 0))
			return false;

		if (wlan_vdev_mlme_get_state(vdev) == WLAN_VDEV_S_UP &&
		    (chan->ch_cfreq1 != bss_arg->ch_freq ||
		     chan->ch_cfreq2 != bss_arg->ch_freq) &&
		    beacon_interval != bss_arg->bss_beacon_interval) {
			/*
			 * Updated beaconInterval should be used only when
			 * we are starting a new BSS not incase of
			 * client or STA case
			 */
			policy_mgr_get_conc_rule1(psoc, &conc_rule1);
			policy_mgr_get_conc_rule2(psoc, &conc_rule2);

			/* Calculate beacon Interval for P2P-GO incase of MCC */
			if (conc_rule1 || conc_rule2) {
				new_bcn_interval = CUSTOM_CONC_GO_BI;
			} else {
				new_bcn_interval =
					if_mgr_calculate_mcc_beacon_interval(
						beacon_interval,
						bss_arg->bss_beacon_interval);
			}
			if (new_bcn_interval != bss_arg->bss_beacon_interval)
				bss_arg->bss_beacon_interval = new_bcn_interval;
			bss_arg->status = QDF_STATUS_SUCCESS;
			return true;
		}
	}
	return false;
}

static void if_mgr_validate_beacon_interval(struct wlan_objmgr_pdev *pdev,
					    void *object, void *arg)
{
	struct beacon_interval_arg *bss_arg = arg;
	struct wlan_objmgr_vdev *vdev = object;
	uint8_t iter_vdev_id = wlan_vdev_get_id(vdev);
	bool is_done = false;

	if (iter_vdev_id == bss_arg->curr_vdev_id)
		return;

	if (wlan_vdev_mlme_get_state(vdev) != WLAN_VDEV_S_UP)
		return;

	if (bss_arg->is_done)
		return;

	switch (bss_arg->curr_bss_opmode) {
	case QDF_STA_MODE:
		is_done = if_mgr_validate_sta_bcn_intrvl(vdev, bss_arg);
		break;
	case QDF_P2P_CLIENT_MODE:
		is_done = if_mgr_validate_p2pcli_bcn_intrvl(vdev, bss_arg);
		break;
	case QDF_SAP_MODE:
	case QDF_IBSS_MODE:
		break;
	case QDF_P2P_GO_MODE:
		is_done = if_mgr_validate_p2pgo_bcn_intrvl(vdev, bss_arg);
		break;
	default:
		ifmgr_err("BSS opmode not supported: %d",
			  bss_arg->curr_bss_opmode);
	}

	if (is_done)
		bss_arg->is_done = is_done;
}

bool if_mgr_is_beacon_interval_valid(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id,
				     struct validate_bss_data *candidate)
{
	struct wlan_objmgr_psoc *psoc;
	struct beacon_interval_arg bss_arg;
	uint8_t enable_mcc_mode;
	struct wlan_objmgr_vdev *vdev;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	wlan_mlme_get_mcc_feature(psoc, &enable_mcc_mode);
	if (!enable_mcc_mode)
		return false;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_IF_MGR_ID);
	if (!vdev)
		return false;

	bss_arg.curr_vdev_id = vdev_id;
	bss_arg.curr_bss_opmode = wlan_vdev_mlme_get_opmode(vdev);
	bss_arg.ch_freq = candidate->chan_freq;
	bss_arg.bss_beacon_interval = candidate->beacon_interval;
	bss_arg.is_done = false;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_IF_MGR_ID);

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  if_mgr_validate_beacon_interval,
					  &bss_arg, 0,
					  WLAN_IF_MGR_ID);

	if (!bss_arg.is_done)
		return true;

	if (bss_arg.bss_beacon_interval != candidate->beacon_interval) {
		candidate->beacon_interval = bss_arg.bss_beacon_interval;
		if (bss_arg.status == QDF_STATUS_SUCCESS)
			return true;
	}

	return false;
}

static void if_mgr_get_vdev_id_from_bssid(struct wlan_objmgr_pdev *pdev,
					  void *object, void *arg)
{
	struct bssid_search_arg *bssid_arg = arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct wlan_objmgr_peer *peer;

	if (!(wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE ||
	      wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_CLIENT_MODE))
		return;

	/* Need to check the connection manager state when that becomes
	 * available
	 */
	if (wlan_vdev_mlme_get_state(vdev) != WLAN_VDEV_S_UP)
		return;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_IF_MGR_ID);
	if (!peer)
		return;

	if (WLAN_ADDR_EQ(bssid_arg->peer_addr.bytes,
			 wlan_peer_get_macaddr(peer)) == QDF_STATUS_SUCCESS)
		bssid_arg->vdev_id = wlan_vdev_get_id(vdev);

	wlan_objmgr_peer_release_ref(peer, WLAN_IF_MGR_ID);
}

QDF_STATUS if_mgr_validate_candidate(struct wlan_objmgr_vdev *vdev,
				     struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	enum QDF_OPMODE op_mode;
	enum policy_mgr_con_mode mode;
	struct bssid_search_arg bssid_arg;
	struct validate_bss_data *candidate_info =
		&event_data->validate_bss_info;
	uint32_t chan_freq = candidate_info->chan_freq;
	uint32_t conc_freq = 0;

	op_mode = wlan_vdev_mlme_get_opmode(vdev);

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Ignore the BSS if any other vdev is already connected to it.
	 */
	qdf_copy_macaddr(&bssid_arg.peer_addr,
			 &candidate_info->peer_addr);
	bssid_arg.vdev_id = WLAN_INVALID_VDEV_ID;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  if_mgr_get_vdev_id_from_bssid,
					  &bssid_arg, 0,
					  WLAN_IF_MGR_ID);

	if (bssid_arg.vdev_id != WLAN_INVALID_VDEV_ID) {
		ifmgr_info("vdev_id %d already connected to "QDF_MAC_ADDR_FMT". select next bss for vdev_id %d",
			   bssid_arg.vdev_id,
			   QDF_MAC_ADDR_REF(bssid_arg.peer_addr.bytes),
			   wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * If concurrency enabled take the concurrent connected channel first.
	 * Valid multichannel concurrent sessions exempted
	 */
	mode = policy_mgr_convert_device_mode_to_qdf_type(op_mode);
	/* If concurrency is not allowed select next bss */
	if (!policy_mgr_is_concurrency_allowed(psoc, mode, chan_freq,
					       HW_MODE_20_MHZ)) {
		ifmgr_info("Concurrency not allowed for this channel freq %d bssid "QDF_MAC_ADDR_FMT", selecting next",
			   chan_freq,
			   QDF_MAC_ADDR_REF(bssid_arg.peer_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * check if channel is allowed for current hw mode, if not fetch
	 * next BSS.
	 */
	if (!policy_mgr_is_hwmode_set_for_given_chnl(psoc, chan_freq)) {
		ifmgr_info("HW mode isn't properly set, freq %d BSSID "QDF_MAC_ADDR_FMT,
			   chan_freq,
			   QDF_MAC_ADDR_REF(bssid_arg.peer_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	/* validate beacon interval */
	if (policy_mgr_concurrent_open_sessions_running(psoc) &&
	    !if_mgr_is_beacon_interval_valid(pdev, wlan_vdev_get_id(vdev),
					     candidate_info)) {
		conc_freq = wlan_get_conc_freq();
		ifmgr_debug("csr Conc Channel freq: %d",
			    conc_freq);

		if (conc_freq) {
			if ((conc_freq == chan_freq) ||
			    (policy_mgr_is_hw_dbs_capable(psoc) &&
			    !wlan_reg_is_same_band_freqs(conc_freq,
							 chan_freq))) {
				/*
				 * make this 0 because we do not want the below
				 * check to pass as we don't want to connect on
				 * other channel
				 */
				ifmgr_debug("Conc chnl freq match: %d",
					    conc_freq);
				conc_freq = 0;
			}
		}
	}

	if (conc_freq)
		return QDF_STATUS_E_INVAL;

	return QDF_STATUS_SUCCESS;
}
