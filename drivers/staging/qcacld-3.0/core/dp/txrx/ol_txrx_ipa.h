/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#ifndef _OL_TXRX_IPA_H_
#define _OL_TXRX_IPA_H_

#ifdef IPA_OFFLOAD

#include <cdp_txrx_cmn.h>       /* ol_txrx_vdev_t, etc. */
#include <ol_txrx_types.h>

/**
 * struct frag_header - fragment header type registered to IPA hardware
 * @length:    fragment length
 * @reserved1: Reserved not used
 * @reserved2: Reserved not used
 *
 */
#ifdef QCA_WIFI_3_0
struct frag_header {
	uint16_t length;
	uint32_t reserved1;
	uint32_t reserved2;
} __packed;
#else
struct frag_header {
	uint32_t
		length:16,
		reserved16:16;
	uint32_t reserved2;
} __packed;
#endif

/**
 * struct ipa_header - ipa header type registered to IPA hardware
 * @vdev_id:  vdev id
 * @reserved: Reserved not used
 *
 */
struct ipa_header {
	uint32_t
		vdev_id:8,      /* vdev_id field is LSB of IPA DESC */
		reserved:24;
} __packed;

/**
 * struct ol_txrx_ipa_uc_tx_hdr - full tx header registered to IPA hardware
 * @frag_hd: fragment header
 * @ipa_hd:  ipa header
 * @eth:     ether II header
 *
 */
struct ol_txrx_ipa_uc_tx_hdr {
	struct frag_header frag_hd;
	struct ipa_header ipa_hd;
	struct ethhdr eth;
} __packed;

/**
 * struct ol_txrx_ipa_uc_rx_hdr - full rx header registered to IPA hardware
 * @eth:     ether II header
 *
 */
struct ol_txrx_ipa_uc_rx_hdr {
	struct ethhdr eth;
} __packed;

#define OL_TXRX_IPA_UC_WLAN_8023_HDR_SIZE      14

#define OL_TXRX_IPA_IPV4_NAME_EXT              "_ipv4"
#define OL_TXRX_IPA_IPV6_NAME_EXT              "_ipv6"

#define OL_TXRX_IPA_MAX_IFACE                  MAX_IPA_IFACE

#define OL_TXRX_IPA_WLAN_FRAG_HEADER        sizeof(struct frag_header)
#define OL_TXRX_IPA_WLAN_IPA_HEADER         sizeof(struct ipa_header)
#define OL_TXRX_IPA_UC_WLAN_TX_HDR_LEN      sizeof(struct ol_txrx_ipa_uc_tx_hdr)
#define OL_TXRX_IPA_UC_WLAN_RX_HDR_LEN      sizeof(struct ol_txrx_ipa_uc_rx_hdr)
#define OL_TXRX_IPA_UC_WLAN_HDR_DES_MAC_OFFSET \
	(OL_TXRX_IPA_WLAN_FRAG_HEADER + OL_TXRX_IPA_WLAN_IPA_HEADER)

#if defined(QCA_WIFI_3_0) && defined(CONFIG_IPA3)
#define OL_TXRX_IPA_WDI2_SET(pipe_in, ipa_res, osdev) \
	do { \
		QDF_IPA_PIPE_IN_UL_RDY_RING_RP_VA(pipe_in) = \
			ipa_res->rx_proc_done_idx->vaddr; \
		QDF_IPA_PIPE_IN_UL_RDY_COMP_RING(pipe_in) = \
			qdf_mem_get_dma_addr(osdev, \
				&ipa_res->rx2_rdy_ring->mem_info);\
		QDF_IPA_PIPE_IN_UL_RDY_COMP_RING_SIZE(pipe_in) = \
			ipa_res->rx2_rdy_ring->mem_info.size; \
		QDF_IPA_PIPE_IN_UL_RDY_COMP_RING_WP_PA(pipe_in) = \
			qdf_mem_get_dma_addr(osdev, \
				&ipa_res->rx2_proc_done_idx->mem_info); \
		QDF_IPA_PIPE_IN_UL_RDY_COMP_RING_WP_VA(pipe_in) = \
			ipa_res->rx2_proc_done_idx->vaddr; \
	} while (0)
#else
/* Do nothing */
#define OL_TXRX_IPA_WDI2_SET(pipe_in, ipa_res, osdev)
#endif /* IPA3 */

/**
 * ol_txrx_ipa_uc_get_resource() - Client request resource information
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 *  OL client will request IPA UC related resource information
 *  Resource information will be distributted to IPA module
 *  All of the required resources should be pre-allocated
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_uc_get_resource(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id);

/**
 * ol_txrx_ipa_uc_set_doorbell_paddr() - Client set IPA UC doorbell register
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 *  IPA UC let know doorbell register physical address
 *  WLAN firmware will use this physical address to notify IPA UC
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_uc_set_doorbell_paddr(struct cdp_soc_t *soc_hdl,
					     uint8_t pdev_id);

/**
 * ol_txrx_ipa_uc_set_active() - Client notify IPA UC data path active or not
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 * @uc_active: WDI UC path enable or not
 * @is_tx: TX path or RX path
 *
 *  IPA UC let know doorbell register physical address
 *  WLAN firmware will use this physical address to notify IPA UC
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_uc_set_active(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				     bool uc_active, bool is_tx);

/**
 * ol_txrx_ipa_uc_op_response() - Handle OP command response from firmware
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 * @op_msg: op response message from firmware
 *
 * Return: none
 */
QDF_STATUS ol_txrx_ipa_uc_op_response(struct cdp_soc_t *soc_hdl,
				      uint8_t pdev_id, uint8_t *op_msg);

/**
 * ol_txrx_ipa_uc_register_op_cb() - Register OP handler function
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 * @op_cb: handler function pointer
 *
 * Return: none
 */
QDF_STATUS ol_txrx_ipa_uc_register_op_cb(struct cdp_soc_t *soc_hdl,
					 uint8_t pdev_id,
					 ipa_uc_op_cb_type op_cb,
					 void *usr_ctxt);

/**
 * ol_txrx_ipa_uc_get_stat() - Get firmware wdi status
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 * Return: none
 */
QDF_STATUS ol_txrx_ipa_uc_get_stat(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * ol_txrx_ipa_enable_autonomy() - Enable autonomy RX path
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 * Set all RX packet route to IPA
 * Return: none
 */
QDF_STATUS ol_txrx_ipa_enable_autonomy(struct cdp_soc_t *soc_hdl,
				       uint8_t pdev_id);

/**
 * ol_txrx_ipa_disable_autonomy() - Disable autonomy RX path
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 * Disable RX packet route to host
 * Return: none
 */
QDF_STATUS ol_txrx_ipa_disable_autonomy(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id);

/**
 * ol_txrx_ipa_tx_buf_smmu_mapping() - Create SMMU mappings for IPA
 *				       allocated TX buffers
 * @soc_hdl: handle to the soc
 * @pdev_id: pdev id number, to get the handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_tx_buf_smmu_mapping(struct cdp_soc_t *soc_hdl,
					   uint8_t pdev_id);

/**
 * ol_txrx_ipa_tx_buf_smmu_unmapping() - Release SMMU mappings for IPA
 *					 allocated TX buffers
 * @soc_hdl: handle to the soc
 * @pdev_id: pdev id number, to get the handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_tx_buf_smmu_unmapping(struct cdp_soc_t *soc_hdl,
					     uint8_t pdev_id);

#ifdef CONFIG_IPA_WDI_UNIFIED_API
/**
 * ol_txrx_ipa_setup() - Setup and connect IPA pipes
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 * @ipa_i2w_cb: IPA to WLAN callback
 * @ipa_w2i_cb: WLAN to IPA callback
 * @ipa_wdi_meter_notifier_cb: IPA WDI metering callback
 * @ipa_desc_size: IPA descriptor size
 * @ipa_priv: handle to the HTT instance
 * @is_rm_enabled: Is IPA RM enabled or not
 * @p_tx_pipe_handle: pointer to Tx pipe handle
 * @p_rx_pipe_handle: pointer to Rx pipe handle
 * @is_smmu_enabled: Is SMMU enabled or not
 * @sys_in: parameters to setup sys pipe in mcc mode
 * @over_gsi: is ipa ver gsi fw
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_setup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			     void *ipa_i2w_cb, void *ipa_w2i_cb,
			     void *ipa_wdi_meter_notifier_cb,
			     uint32_t ipa_desc_size,
			     void *ipa_priv, bool is_rm_enabled,
			     uint32_t *tx_pipe_handle, uint32_t *rx_pipe_handle,
			     bool is_smmu_enabled,
			     qdf_ipa_sys_connect_params_t *sys_in,
			     bool over_gsi);
#else /* CONFIG_IPA_WDI_UNIFIED_API */
/**
 * ol_txrx_ipa_setup() - Setup and connect IPA pipes
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 * @ipa_i2w_cb: IPA to WLAN callback
 * @ipa_w2i_cb: WLAN to IPA callback
 * @ipa_wdi_meter_notifier_cb: IPA WDI metering callback
 * @ipa_desc_size: IPA descriptor size
 * @ipa_priv: handle to the HTT instance
 * @is_rm_enabled: Is IPA RM enabled or not
 * @p_tx_pipe_handle: pointer to Tx pipe handle
 * @p_rx_pipe_handle: pointer to Rx pipe handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_setup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			     void *ipa_i2w_cb, void *ipa_w2i_cb,
			     void *ipa_wdi_meter_notifier_cb,
			     uint32_t ipa_desc_size, void *ipa_priv,
			     bool is_rm_enabled, uint32_t *tx_pipe_handle,
			     uint32_t *rx_pipe_handle);
#endif /* CONFIG_IPA_WDI_UNIFIED_API */
QDF_STATUS ol_txrx_ipa_cleanup(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			       uint32_t tx_pipe_handle,
			       uint32_t rx_pipe_handle);
QDF_STATUS ol_txrx_ipa_setup_iface(char *ifname, uint8_t *mac_addr,
		qdf_ipa_client_type_t prod_client,
		qdf_ipa_client_type_t cons_client,
		uint8_t session_id, bool is_ipv6_enabled);
QDF_STATUS ol_txrx_ipa_cleanup_iface(char *ifname, bool is_ipv6_enabled);

/**
 * ol_txrx_ipa_enable_pipes() - Enable and resume traffic on Tx/Rx pipes
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_enable_pipes(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * ol_txrx_ipa_disable_pipes() – Suspend traffic and disable Tx/Rx pipes
 * @soc_hdl: data path soc handle
 * @pdev_id: device instance id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_disable_pipes(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id);
QDF_STATUS ol_txrx_ipa_set_perf_level(int client,
		uint32_t max_supported_bw_mbps);
#ifdef FEATURE_METERING
/**
 * ol_txrx_ipa_uc_get_share_stats() - get Tx/Rx byte stats from FW
 * @soc_hdl: data path soc handle
 * @pdev_id: physical device instance id
 * @value: reset stats
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_uc_get_share_stats(struct cdp_soc_t *soc_hdl,
					  uint8_t pdev_id, uint8_t reset_stats);
/**
 * ol_txrx_ipa_uc_set_quota() - set quota limit to FW
 * @soc_hdl: data path soc handle
 * @pdev_id: physical device instance number
 * @value: quota limit bytes
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ol_txrx_ipa_uc_set_quota(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				    uint64_t quota_bytes);
#endif
#endif
#endif /* _OL_TXRX_IPA_H_*/
