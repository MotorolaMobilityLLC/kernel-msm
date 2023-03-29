/*
 * Copyright (c) 2011-2014, 2016, 2018-2021 The Linux Foundation. All rights reserved.
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
 *
 * This file lim_prop_exts_utils.h contains the definitions
 * used by all LIM modules to support proprietary features.
 * Author:        Chandra Modumudi
 * Date:          12/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __LIM_PROP_EXTS_UTILS_H
#define __LIM_PROP_EXTS_UTILS_H

#define LIM_ADAPTIVE_11R_OUI      "\x00\x40\x96\x2C"
#define LIM_ADAPTIVE_11R_OUI_SIZE 4

/**
 * lim_extract_ap_capability() - extract AP's HCF/WME/WSM capability
 * @mac_ctx: Pointer to Global MAC structure
 * @p_ie: Pointer to starting IE in Beacon/Probe Response
 * @ie_len: Length of all IEs combined
 * @qos_cap: Bits are set according to capabilities
 * @uapsd: pointer to uapsd
 * @local_constraint: Pointer to local power constraint.
 * @session: A pointer to session entry.
 * @is_pwr_constraint: Check for Power constraint bit in beacon
 *
 * This function is called to extract AP's HCF/WME/WSM capability
 * from the IEs received from it in Beacon/Probe Response frames
 *
 * Return: None
 */
void
lim_extract_ap_capability(struct mac_context *mac_ctx, uint8_t *p_ie,
			  uint16_t ie_len, uint8_t *qos_cap, uint8_t *uapsd,
			  int8_t *local_constraint, struct pe_session *session,
			  bool *is_pwr_constraint);

ePhyChanBondState lim_get_htcb_state(ePhyChanBondState aniCBMode);

/**
 * lim_objmgr_update_vdev_nss() - update nss in vdev object
 * @psoc: Pointer to Global MAC structure
 * @vdev_id: vdev id
 * @nss: nss
 *
 * Return: None
 */
void lim_objmgr_update_vdev_nss(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint8_t nss);

/**
 * lim_update_he_mcs_12_13_map() - update he_mcs_12_13_map in vdev object
 * @psoc: Pointer to Global MAC structure
 * @vdev_id: vdev id
 * @he_mcs_12_13_map: he mcs 12/13 map
 *
 * Return: None
 */
void lim_update_he_mcs_12_13_map(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint16_t he_mcs_12_13_map);
#endif /* __LIM_PROP_EXTS_UTILS_H */
