/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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

#ifndef __WMA_HE_H
#define __WMA_HE_H

#include "wma.h"
#include "sir_api.h"
#include "target_if.h"

/* Number of bits to shift on HE MCS 12 13 MAP to get the desired map */
#define WMA_MCS_12_13_MAP_L80 16
#define WMA_MCS_12_13_MAP_G80 8

/* Mask to fill tx and rx mcs rate maps to be sent to the FW */
#define WMA_MCS_12_13_PEER_RATE_MAP 0x00ff0000

#ifdef WLAN_FEATURE_11AX
/**
 * wma_print_he_cap() - Print HE capabilities
 * @he_cap: pointer to HE Capability
 *
 * Received HE capabilities are converted into dot11f structure.
 * This function will print all the HE capabilities as stored
 * in the dot11f structure.
 *
 * Return: None
 */
void wma_print_he_cap(tDot11fIEhe_cap *he_cap);

/**
 * wma_print_he_ppet() - Prints HE PPE Threshold
 * @he_ppet: PPE Threshold
 *
 * This function prints HE PPE Threshold as received from FW.
 * Refer to the definition of wmi_ppe_threshold to understand
 * how PPE thresholds are packed by FW for a given NSS and RU.
 *
 * Return: none
 */
void wma_print_he_ppet(void *ppet);

/**
 * wma_print_he_phy_cap() - Print HE PHY Capability
 * @phy_cap: pointer to PHY Capability
 *
 * This function prints HE PHY Capability received from FW.
 *
 * Return: none
 */
void wma_print_he_phy_cap(uint32_t *phy_cap);

/**
 * wma_print_he_mac_cap_w1() - Print HE MAC Capability
 * @mac_cap: MAC Capability
 *
 * This function prints HE MAC Capability received from FW.
 *
 * Return: none
 */
void wma_print_he_mac_cap_w1(uint32_t mac_cap);

/**
 * wma_print_he_mac_cap_w2() - Print HE MAC Capability
 * @mac_cap: MAC Capability
 *
 * This function prints HE MAC Capability received from FW.
 *
 * Return: none
 */
void wma_print_he_mac_cap_w2(uint32_t mac_cap);

/**
 * wma_print_he_op() - Print HE Operation
 * @he_cap: pointer to HE Operation
 *
 * Print HE operation stored as dot11f structure
 *
 * Return: None
 */
void wma_print_he_op(tDot11fIEhe_op *he_ops);

/**
 * wma_update_target_ext_he_cap() - Update HE caps with given extended cap
 * @tgt_hdl: target psoc information
 * @tgt_cfg: Target config
 *
 * This function loop through each hardware mode and for each hardware mode
 * again it loop through each MAC/PHY and pull the caps 2G and 5G specific
 * HE caps and derives the final cap.
 *
 * Return: None
 */
void wma_update_target_ext_he_cap(struct target_psoc_info *tgt_hdl,
				  struct wma_tgt_cfg *tgt_cfg);

/*
 * wma_he_update_tgt_services() - update tgt cfg to indicate 11ax support
 * @wmi_handle: pointer to WMI handle
 * @cfg: pointer to WMA target services
 *
 * Based on WMI SERVICES information, enable 11ax support and set DOT11AX bit
 * in feature caps bitmap.
 *
 * Return: None
 */
void wma_he_update_tgt_services(struct wmi_unified *wmi_handle,
				struct wma_tgt_services *cfg);

/**
 * wma_populate_peer_he_cap() - populate peer HE capabilities in peer assoc cmd
 * @peer: pointer to peer assoc params
 * @params: pointer to ADD STA params
 *
 * Return: None
 */
void wma_populate_peer_he_cap(struct peer_assoc_params *peer,
			      tpAddStaParams params);

/**
 * wma_update_vdev_he_ops() - update he ops in vdev start request
 * @he_ops: target he ops
 * @he_op: source he ops
 *
 * Return: None
 */
void wma_update_vdev_he_ops(uint32_t *he_ops, tDot11fIEhe_op *he_op);

#define DOT11AX_HEMU_MODE 0x30
#define HE_SUBFEE 0
#define HE_SUBFER 1
#define HE_MUBFEE 2
#define HE_MUBFER 3

/**
 * wma_set_he_txbf_params() - set HE Tx beamforming params to FW
 * @vdev_id: VDEV id
 * @su bfer: SU beamformer capability
 * @su bfee: SU beamformee capability
 * @mu bfer: MU beamformer capability
 *
 * Return: None
 */
void wma_set_he_txbf_params(uint8_t vdev_id, bool su_bfer,
			    bool su_bfee, bool mu_bfer);


/**
 * wma_set_he_txbf_cfg() - set HE Tx beamforming mlme cfg to FW
 * @mac: Global MAC context
 * @vdev_id: VDEV id
 *
 * Return: None
 */
void wma_set_he_txbf_cfg(struct mac_context *mac, uint8_t vdev_id);
/**
 * wma_vdev_set_he_bss_params() - set HE OPs in vdev start
 * @wma: pointer to wma handle
 * @vdev_id: VDEV id
 * @he_info: pointer to he info
 *
 * Return: None
 */
void wma_vdev_set_he_bss_params(tp_wma_handle wma, uint8_t vdev_id,
				struct vdev_mlme_he_ops_info *he_info);

/**
 * wma_vdev_set_he_config() - set HE Config in vdev start
 * @wma: pointer to wma handle
 * @vdev_id: VDEV id
 * @add_bss: BSS params
 *
 * Return: None
 */
void wma_vdev_set_he_config(tp_wma_handle wma, uint8_t vdev_id,
				struct bss_params *add_bss);

static inline bool wma_is_peer_he_capable(tpAddStaParams params)
{
	return params->he_capable;
}

/**
 * wma_update_he_ops_ie() - update the HE OPS IE to firmware
 * @wma: pointer to wma context
 * @vdev_id: vdev id
 * @he_ops: 32bit value of HE ops
 *
 * This API is used to send updated HE operational IE to firmware, so that
 * firmware can be in sync with host
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_update_he_ops_ie(tp_wma_handle wma, uint8_t vdev_id,
				tDot11fIEhe_op *he_ops);

/**
 * wma_get_he_capabilities() - Get HE capabilities from WMA
 * @he_cap: Pointer to HE capabilities
 *
 * Currently HE capabilities are not updated in wma_handle. This
 * is an interface for upper layer to query capabilities from WMA.
 * When the real use case arise, update wma_handle with HE capabilities
 * as required.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_get_he_capabilities(struct he_capability *he_cap);

/**
 * wma_set_he_vdev_param() - update he vdev param in wma
 * @intr: pointer to wma_txrx_node
 * @param_id: vdev param id
 * @value: value of vdev param
 *
 * Result: None
 */
void wma_set_he_vdev_param(struct wma_txrx_node *intr, WMI_VDEV_PARAM param_id,
			   uint32_t value);

/**
 * wma_get_he_vdev_param() - retrieve he vdev param from wma
 * @intr: pointer to wma_txrx_node
 * @param_id: vdev param id
 *
 * Result: param value
 */
uint32_t wma_get_he_vdev_param(struct wma_txrx_node *intr,
			       WMI_VDEV_PARAM param_id);

#else
static inline void wma_print_he_cap(tDot11fIEhe_cap *he_cap)
{
}

static inline void wma_print_he_ppet(void *ppet)
{
}

static inline void wma_print_he_phy_cap(uint32_t *phy_cap)
{
}

static inline void wma_print_he_mac_cap_w1(uint32_t mac_cap)
{
}

static inline void wma_print_he_mac_cap_w2(uint32_t mac_cap)
{
}

static inline void wma_print_he_op(tDot11fIEhe_op *he_ops)
{
}

static inline void wma_update_target_ext_he_cap(struct
						target_psoc_info *tgt_hdl,
						struct wma_tgt_cfg *tgt_cfg)
{
}

static inline void wma_he_update_tgt_services(struct wmi_unified *wmi_handle,
					      struct wma_tgt_services *cfg)
{
	cfg->en_11ax = false;
	return;
}

static inline void wma_populate_peer_he_cap(struct peer_assoc_params *peer,
					    tpAddStaParams params)
{
}

static inline
void wma_update_vdev_he_ops(uint32_t *he_ops, tDot11fIEhe_op *he_op)
{
}

static inline void wma_set_he_txbf_params(uint8_t vdev_id, bool su_bfer,
					  bool su_bfee, bool mu_bfer)
{
}

static inline void wma_set_he_txbf_cfg(struct mac_context *mac, uint8_t vdev_id)
{
}

static inline  QDF_STATUS wma_update_he_ops_ie(tp_wma_handle wma,
			uint8_t vdev_id, tDot11fIEhe_op *he_ops)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void wma_vdev_set_he_bss_params(tp_wma_handle wma, uint8_t vdev_id,
				struct vdev_mlme_he_ops_info *he_info)
{
}

static inline void wma_vdev_set_he_config(tp_wma_handle wma, uint8_t vdev_id,
					struct bss_params *add_bss)
{
}

static inline bool wma_is_peer_he_capable(tpAddStaParams params)
{
	return false;
}

static inline void wma_set_he_vdev_param(struct wma_txrx_node *intr,
			WMI_VDEV_PARAM param_id, uint32_t value)
{
}

static inline uint32_t wma_get_he_vdev_param(struct wma_txrx_node *intr,
					     WMI_VDEV_PARAM param_id)
{
	return 0;
}

#endif

#endif /* __WMA_HE_H */
