/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
/**
 * DOC: Declare API's for wow pattern addition and deletion in fwr
 */

#ifndef _WLAN_PMO_WOW_H_
#define _WLAN_PMO_WOW_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_main.h"
#include "wlan_pmo_wow_public_struct.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

/**
 * DOC: wlan_pmo_wowl
 *
 * This module houses all the logic for WOW(wake on wireless) in
 * PMO(Power Management and Offload).
 *
 * It provides the following APIs
 *
 * - Ability to enable/disable following WoWL modes
 *  1) Magic packet (MP) mode
 *  2) Pattern Byte Matching (PBM) mode
 * - Ability to add/remove patterns for PBM
 *
 * A Magic Packet is a packet that contains 6 0xFFs followed by 16
 * contiguous copies of the receiving NIC's Ethernet address. There is
 * no API to configure Magic Packet Pattern.
 *
 * Wakeup pattern (used for PBM) is defined as following:
 * struct
 * {
 *  U8  PatternSize;                  // Non-Zero pattern size
 *  U8  PatternMaskSize;              // Non-zero pattern mask size
 *  U8  PatternMask[PatternMaskSize]; // Pattern mask
 *  U8  Pattern[PatternSize];         // Pattern
 * } hdd_wowl_ptrn_t;
 *
 * PatternSize and PatternMaskSize indicate size of the variable
 * length Pattern and PatternMask. PatternMask indicates which bytes
 * of an incoming packet should be compared with corresponding bytes
 * in the pattern.
 *
 * Maximum allowed pattern size is 128 bytes. Maximum allowed
 * PatternMaskSize is 16 bytes.
 *
 * Maximum number of patterns that can be configured is 8
 *
 * PMO will add following 2 commonly used patterns for PBM by default:
 *  1) ARP Broadcast Pattern
 *  2) Unicast Pattern
 *
 * However note that WoWL will not be enabled by default by PMO. WoWL
 * needs to enabled explcitly by exercising the iwpriv command.
 *
 * PMO will expose an API that accepts patterns as Hex string in the
 * following format:
 * "PatternSize:PatternMaskSize:PatternMask:Pattern"
 *
 * Multiple patterns can be specified by deleimiting each pattern with
 * the ';' token:
 * "PatternSize1:PatternMaskSize1:PatternMask1:Pattern1;PatternSize2:..."
 *
 * Patterns can be configured dynamically via iwpriv cmd or statically
 * via qcom_cfg.ini file
 *
 * PBM (when enabled) can perform filtering on unicast data or
 * broadcast data or both. These configurations are part of factory
 * default (cfg.dat) and the default behavior is to perform filtering
 * on both unicast and data frames.
 *
 * MP filtering (when enabled) is performed ALWAYS on both unicast and
 * broadcast data frames.
 *
 * Management frames are not subjected to WoWL filtering and are
 * discarded when WoWL is enabled.
 *
 * Whenever a patern match succeeds, RX path is restored and packets
 * (both management and data) will be pushed to the host from that
 * point onwards.  Therefore, exit from WoWL is implicit and happens
 * automatically when the first packet match succeeds.
 *
 * WoWL works on top of BMPS. So when WoWL is requested, SME will
 * attempt to put the device in BMPS mode (if not already in BMPS). If
 * attempt to BMPS fails, request for WoWL will be rejected.
 */

#define PMO_WOW_MAX_EVENT_BM_LEN 4

#define PMO_WOW_FILTERS_ARP_NS		2
#define PMO_WOW_FILTERS_PKT_OR_APF	5

/**
 * pmo_get_and_increment_wow_default_ptrn() -Get and increment wow default ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to get and increment wow default ptrn
 *
 * Return: current wow default ptrn count
 */
static inline uint8_t pmo_get_and_increment_wow_default_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	uint8_t count;

	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		count = vdev_ctx->num_wow_default_patterns++;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		count = vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_def++;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}

	return count;
}

/**
 * pmo_increment_wow_default_ptrn() -increment wow default ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to increment wow default ptrn
 *
 * Return: None
 */
static inline void pmo_increment_wow_default_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		vdev_ctx->num_wow_default_patterns++;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_def++;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}
}

/**
 * pmo_decrement_wow_default_ptrn() -decrement wow default ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to decrement wow default ptrn
 *
 * Return: None
 */
static inline void pmo_decrement_wow_default_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		vdev_ctx->num_wow_default_patterns--;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_def--;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}
}

/**
 * pmo_get_wow_default_ptrn() -Get wow default ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to get wow default ptrn
 *
 * Return: current wow default ptrn count
 */
static inline uint8_t pmo_get_wow_default_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	uint8_t count;

	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		count = vdev_ctx->num_wow_default_patterns;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		count = vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_def;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}

	return count;
}

/**
 * pmo_get_wow_default_ptrn() -Set wow default ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to set wow default ptrn
 *
 * Return: Set wow default ptrn count
 */
static inline void pmo_set_wow_default_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx, uint8_t value)
{
	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		vdev_ctx->num_wow_default_patterns = value;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_def = value;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}
}

/**
 * pmo_increment_wow_user_ptrn() -increment wow user ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to increment wow user ptrn
 *
 * Return: None
 */
static inline void pmo_increment_wow_user_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		vdev_ctx->num_wow_user_patterns++;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_usr++;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}
}

/**
 * pmo_decrement_wow_user_ptrn() -decrement wow user ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to decrement wow user ptrn
 *
 * Return: None
 */
static inline void pmo_decrement_wow_user_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		vdev_ctx->num_wow_user_patterns--;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_usr--;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}
}

/**
 * pmo_get_wow_user_ptrn() -Get wow user ptrn
 * @vdev_ctx: pmo vdev priv ctx
 *
 * API to Get wow user ptrn
 *
 * Return: None
 */
static inline uint8_t pmo_get_wow_user_ptrn(
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	uint8_t count;

	if (vdev_ctx->pmo_psoc_ctx->caps.unified_wow) {
		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		count = vdev_ctx->num_wow_user_patterns;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	} else {
		qdf_spin_lock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
		count = vdev_ctx->pmo_psoc_ctx->wow.ptrn_id_usr;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_psoc_ctx->lock);
	}

	return count;
}

/**
 * pmo_core_del_wow_pattern() - Function which will delete the WoWL pattern
 * @vdev: pointer to the vdev
 *
 * This function deletes all the user WoWl patterns and default WoWl patterns
 *
 * Return: error if any errors encountered, QDF_STATUS_SUCCESS otherwise
 */

QDF_STATUS pmo_core_del_wow_pattern(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_add_wow_user_pattern() - Function which will add the WoWL pattern
 *			 to be used when PBM filtering is enabled
 * @vdev: pointer to the vdev
 * @ptrn: pointer to the pattern string to be added
 *
 * Return: false if any errors encountered, QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS pmo_core_add_wow_user_pattern(struct wlan_objmgr_vdev *vdev,
			struct pmo_wow_add_pattern *ptrn);

/**
 * pmo_core_del_wow_user_pattern() - Function which will delete the WoWL pattern
 * @vdev: pointer to the vdev
 * @ptrn: pointer to the pattern string to be delete
 *
 * Return: error if any errors encountered, QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS pmo_core_del_wow_user_pattern(struct wlan_objmgr_vdev *vdev,
			uint8_t pattern_id);

/**
 * pmo_core_enable_wakeup_event() -  enable wow wakeup events
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 * @wow_event: wow event to enable
 *
 * Return: none
 */
void pmo_core_enable_wakeup_event(struct wlan_objmgr_psoc *psoc,
				  uint32_t vdev_id,
				  WOW_WAKE_EVENT_TYPE wow_event);

/**
 * pmo_core_disable_wakeup_event() -  disable wow wakeup events
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 * @wow_event: wow event to disable
 *
 * Return: none
 */
void pmo_core_disable_wakeup_event(struct wlan_objmgr_psoc *psoc,
				   uint32_t vdev_id,
				   WOW_WAKE_EVENT_TYPE wow_event);

/**
 * pmo_is_wow_applicable(): should enable wow
 * @psoc: objmgr psoc object
 *
 *  Enable WOW if any one of the condition meets,
 *  1) Is any one of vdev in beaconning mode (in AP mode) ?
 *  2) Is any one of vdev in connected state (in STA mode) ?
 *  3) Is PNO in progress in any one of vdev ?
 *  4) Is Extscan in progress in any one of vdev ?
 *  5) Is P2P listen offload in any one of vdev?
 *  6) Is any vdev in NAN data mode? BSS is already started at the
 *     the time of device creation. It is ready to accept data
 *     requests.
 *  7) If LPASS feature is enabled
 *  8) If NaN feature is enabled
 *  If none of above conditions is true then return false
 *
 * Return: true if wma needs to configure wow false otherwise.
 */
bool pmo_core_is_wow_applicable(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_update_wow_enable() - update wow enable flag
 * @psoc_ctx: Pointer to objmgr psoc handle
 * @value: true if wow mode enable else false
 *
 * Return: None
 */
static inline
void pmo_core_update_wow_enable(struct pmo_psoc_priv_obj *psoc_ctx,
	bool value)
{
	qdf_spin_lock_bh(&psoc_ctx->lock);
	psoc_ctx->wow.wow_enable = value;
	qdf_spin_unlock_bh(&psoc_ctx->lock);
}

/**
 * pmo_core_is_wow_mode_enabled() - check if wow needs to be enabled in fw
 * @psoc_ctx: Pointer to objmgr psoc handle
 *
 * API to check if wow mode is enabled in fwr as part of apps suspend or not
 *
 * Return: true is wow mode is enabled else false
 */
static inline
bool pmo_core_is_wow_enabled(struct pmo_psoc_priv_obj *psoc_ctx)
{
	bool value;

	if (!psoc_ctx) {
		pmo_err("psoc_ctx is null");
		return false;
	}

	qdf_spin_lock_bh(&psoc_ctx->lock);
	value = psoc_ctx->wow.wow_enable;
	qdf_spin_unlock_bh(&psoc_ctx->lock);
	pmo_debug("WoW enable %d", value);

	return value;
}

/**
 * pmo_core_set_wow_nack() - Set wow nack flag
 * @psoc_ctx: Pointer to objmgr psoc handle
 * @value: true if received wow nack from else false
 *
 * Return: None
 */
static inline
void pmo_core_set_wow_nack(struct pmo_psoc_priv_obj *psoc_ctx, bool value)
{
	qdf_spin_lock_bh(&psoc_ctx->lock);
	psoc_ctx->wow.wow_nack = value;
	qdf_spin_unlock_bh(&psoc_ctx->lock);
}

/**
 * pmo_core_get_wow_nack() - Get wow nack flag
 * @psoc_ctx: Pointer to objmgr psoc handle
 *
 * Return: wow nack flag
 */
static inline
bool pmo_core_get_wow_nack(struct pmo_psoc_priv_obj *psoc_ctx)
{
	bool value;

	qdf_spin_lock_bh(&psoc_ctx->lock);
	value = psoc_ctx->wow.wow_nack;
	qdf_spin_unlock_bh(&psoc_ctx->lock);

	return value;
}
/**
 * pmo_core_update_wow_enable_cmd_sent() - update wow enable cmd sent flag
 * @psoc_ctx: Pointer to objmgr psoc handle
 * @value: true if wow enable cmd sent else false
 *
 * Return: None
 */
static inline
void pmo_core_update_wow_enable_cmd_sent(struct pmo_psoc_priv_obj *psoc_ctx,
	bool value)
{
	qdf_spin_lock_bh(&psoc_ctx->lock);
	psoc_ctx->wow.wow_enable_cmd_sent = value;
	qdf_spin_unlock_bh(&psoc_ctx->lock);
}

/**
 * pmo_core_get_wow_enable_cmd_sent() - Get wow enable cmd sent flag
 * @psoc_ctx: Pointer to objmgr psoc handle
 *
 * Return: return true if wow enable cmd sent else false
 */
static inline
bool pmo_core_get_wow_enable_cmd_sent(struct pmo_psoc_priv_obj *psoc_ctx)
{
	bool value;

	qdf_spin_lock_bh(&psoc_ctx->lock);
	value = psoc_ctx->wow.wow_enable_cmd_sent;
	qdf_spin_unlock_bh(&psoc_ctx->lock);

	return value;
}

/**
 * pmo_core_update_wow_initial_wake_up() - update wow initial wake up
 * @psoc_ctx: Pointer to objmgr psoc handle
 * @value: set to 1 if wow initial wake up is received;
 *         if clean state, reset it to 0;
 *
 * Return: None
 */
static inline
void pmo_core_update_wow_initial_wake_up(struct pmo_psoc_priv_obj *psoc_ctx,
	int value)
{
	qdf_atomic_set(&psoc_ctx->wow.wow_initial_wake_up, value);
}

/**
 * pmo_core_get_wow_initial_wake_up() - Get wow initial wake up
 * @psoc_ctx: Pointer to objmgr psoc handle
 *
 * Return:  1 if wow initial wake up is received;
 *          0 if wow iniital wake up is not received;
 */
static inline
int pmo_core_get_wow_initial_wake_up(struct pmo_psoc_priv_obj *psoc_ctx)
{
	return qdf_atomic_read(&psoc_ctx->wow.wow_initial_wake_up);
}

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * pmo_core_is_extscan_in_progress(): check if a extscan is in progress
 * @vdev: objmgr vdev handle
 *
 * Return: TRUE/FALSE
 */
static inline
bool pmo_core_is_extscan_in_progress(struct wlan_objmgr_vdev *vdev)
{
	bool extscan_in_progress;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	extscan_in_progress = vdev_ctx->extscan_in_progress;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return extscan_in_progress;
}

/**
 * pmo_core_update_extscan_in_progress(): update extscan is in progress flags
 * @vdev: objmgr vdev handle
 * @value:true if extscan is in progress else false
 *
 * Return: TRUE/FALSE
 */
static inline
void pmo_core_update_extscan_in_progress(struct wlan_objmgr_vdev *vdev,
	bool value)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->extscan_in_progress = value;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
}
#else
static inline
bool pmo_core_is_extscan_in_progress(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
void pmo_core_update_extscan_in_progress(struct wlan_objmgr_vdev *vdev,
	bool value)
{
}
#endif

/**
 * pmo_core_is_p2plo_in_progress(): check if p2plo is in progress
 * @vdev: objmgr vdev handle
 *
 * Return: TRUE/FALSE
 */
static inline
bool pmo_core_is_p2plo_in_progress(struct wlan_objmgr_vdev *vdev)
{
	bool p2plo_in_progress;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	p2plo_in_progress = vdev_ctx->p2plo_in_progress;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return p2plo_in_progress;
}

/**
 * pmo_core_update_p2plo_in_progress(): update p2plo is in progress flags
 * @vdev: objmgr vdev handle
 * @value:true if p2plo is in progress else false
 *
 * Return: TRUE/FALSE
 */
static inline
void pmo_core_update_p2plo_in_progress(struct wlan_objmgr_vdev *vdev,
	bool value)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->p2plo_in_progress = value;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
}

#ifdef WLAN_FEATURE_LPSS
/**
 * pmo_core_is_lpass_enabled() - check if lpass is enabled
 * @posc: objmgr psoc object
 *
 * WoW is needed if LPASS or NaN feature is enabled in INI because
 * target can't wake up itself if its put in PDEV suspend when LPASS
 * or NaN features are supported
 *
 * Return: true if lpass is enabled else false
 */
static inline
bool pmo_core_is_lpass_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.lpass_enable;
}
#else
static inline
bool pmo_core_is_lpass_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * pmo_get_event_bitmap_idx() - get indices for extended wow bitmaps
 * @event: wow event
 * @wow_bitmap_size: WOW bitmap size
 * @bit_idx: bit index
 * @idx: byte index
 *
 * Return: none
 */
static inline void pmo_get_event_bitmap_idx(WOW_WAKE_EVENT_TYPE event,
			      uint32_t wow_bitmap_size,
			      uint32_t *bit_idx,
			      uint32_t *idx)
{

	if (!bit_idx || !idx || wow_bitmap_size == 0) {
		pmo_err("bit_idx:%pK idx:%pK wow_bitmap_size:%u",
			 bit_idx, idx, wow_bitmap_size);
		return;
	}
	if (event == 0) {
		*idx = *bit_idx = 0;
	} else {
		*idx = event / (wow_bitmap_size * 8);
		*bit_idx = event % (wow_bitmap_size * 8);
	}
}

/**
 * pmo_get_num_wow_filters() - get the supported number of WoW filters
 * @psoc: the psoc to query
 *
 * Return: number of WoW filters supported
 */
uint8_t pmo_get_num_wow_filters(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_get_wow_state() - Get wow state
 * @pmo_ctx: Pointer to pmo priv object
 *
 * Return:  current WoW status(none/D0/D3)
 */
static inline
enum pmo_wow_state pmo_core_get_wow_state(struct pmo_psoc_priv_obj *pmo_ctx)
{
	return pmo_ctx->wow.wow_state;
}
#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_WOW_H_ */
