/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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

#ifndef CSR_HOST_SCAN_ROAM_H
#define CSR_HOST_SCAN_ROAM_H

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * csr_roam_issue_reassociate() - Issue Reassociate
 * @mac: Global MAC Context
 * @vdev_id: SME vdev ID
 * @bss_desc: BSS Descriptor
 * @ies: Pointer to the IE's
 * @roam_profile: Roaming profile
 *
 * Return: Success or Failure
 */
QDF_STATUS
csr_roam_issue_reassociate(struct mac_context *mac, uint32_t vdev_id,
			   struct bss_description *bss_desc,
			   tDot11fBeaconIEs *bcn_ies,
			   struct csr_roam_profile *roam_profile);

/**
 * csr_roam_issue_reassociate_cmd() - Issue the reassociate command
 * @mac: Global MAC Context
 * @vdev_id: SME vdev ID
 *
 * Return: Success or Failure status
 */
QDF_STATUS
csr_roam_issue_reassociate_cmd(struct mac_context *mac, uint32_t vdev_id);

/**
 * csr_neighbor_roam_process_scan_results() - build roaming candidate list
 *
 * @mac_ctx: The handle returned by mac_open.
 * @sessionid: Session information
 * @scan_results_list: List obtained from csr_scan_get_result()
 *
 * This function applies various candidate checks like LFR, 11r, preauth, ESE
 * and builds a roamable AP list. It applies age limit only if no suitable
 * recent candidates are found.
 *
 * Output list is built in mac_ctx->roam.neighborRoamInfo[sessionid].
 *
 * Return: void
 */
void
csr_neighbor_roam_process_scan_results(struct mac_context *mac_ctx,
				       uint8_t vdev_id,
				       tScanResultHandle *scan_results_list);
/**
 * csr_neighbor_roam_trigger_handoff() - Start roaming
 * @mac_ctx: Global MAC Context
 * @vdev_id: SME vdev ID
 *
 * Return: None
 */
void csr_neighbor_roam_trigger_handoff(struct mac_context *mac_ctx,
				       uint8_t vdev_id);
/**
 * csr_neighbor_roam_process_scan_complete() - Post process the scan results
 * @mac: Global MAC Context
 * @vdev_id: SME vdev ID
 *
 * Return: Success or Failure
 */
QDF_STATUS csr_neighbor_roam_process_scan_complete(struct mac_context *mac,
						   uint8_t vdev_id);

/**
 * csr_neighbor_roam_candidate_found_ind_hdlr() - Handle roam candidate found
 * indication from firmware
 * @mac_ctx: Pointer to Global MAC structure
 * @msg_buf: pointer to msg buff
 *
 * This function is called by CSR as soon as TL posts the candidate
 * found indication to SME via MC thread
 *
 * Return: QDF_STATUS_SUCCESS on success, corresponding error code otherwise
 */
QDF_STATUS csr_neighbor_roam_candidate_found_ind_hdlr(struct mac_context *mac,
						      void *msg_buf);

/**
 * csr_neighbor_roam_free_roamable_bss_list() - Frees roamable APs list
 * @mac_ctx: The handle returned by mac_open.
 * @llist: Neighbor Roam BSS List to be emptied
 *
 * Empties and frees all the nodes in the roamable AP list
 *
 * Return: none
 */
void csr_neighbor_roam_free_roamable_bss_list(struct mac_context *mac_ctx,
					      tDblLinkList *llist);

/**
 * csr_neighbor_roam_remove_roamable_ap_list_entry() - Remove roamable
 * candidate APs from list
 * @mac_ctx: Pointer to Global MAC structure
 * @list: The list from which the entry should be removed
 * @neigh_entry: Neighbor Roam BSS Node to be removed
 *
 * This function removes a given entry from the given list
 *
 * Return: true if successfully removed, else false
 */
bool csr_neighbor_roam_remove_roamable_ap_list_entry(struct mac_context *mac,
		tDblLinkList *list, tpCsrNeighborRoamBSSInfo neigh_entry);

/**
 * csr_neighbor_roam_next_roamable_ap() - Get next AP from roamable AP list
 * @mac_ctx - The handle returned by mac_open.
 * @plist - The list from which the entry should be returned
 * @neighbor_entry - Neighbor Roam BSS Node whose next entry should be returned
 *
 * Gets the entry next to passed entry. If NULL is passed, return the entry
 * in the head of the list
 *
 * Return: Neighbor Roam BSS Node to be returned
 */
tpCsrNeighborRoamBSSInfo
csr_neighbor_roam_next_roamable_ap(struct mac_context *mac_ctx,
				   tDblLinkList *llist,
				   tpCsrNeighborRoamBSSInfo neighbor_entry);

/**
 * csr_neighbor_roam_request_handoff() - Handoff to a different AP
 * @mac_ctx: Pointer to Global MAC structure
 * @vdev_id: vdev id
 *
 * This function triggers actual switching from one AP to the new AP.
 * It issues disassociate with reason code as Handoff and CSR as a part of
 * handling disassoc rsp, issues reassociate to the new AP
 *
 * Return: none
 */
void
csr_neighbor_roam_request_handoff(struct mac_context *mac, uint8_t vdev_id);

/**
 * csr_neighbor_roam_get_handoff_ap_info - Identifies the best AP for roaming
 * @mac:            mac context
 * @session_id:     vdev Id
 * @hand_off_node:  AP node that is the handoff candidate returned
 *
 * This function returns the best possible AP for handoff. For 11R case, it
 * returns the 1st entry from pre-auth done list. For non-11r case, it returns
 * the 1st entry from roamable AP list
 *
 * Return: true if able find handoff AP, false otherwise
 */
bool
csr_neighbor_roam_get_handoff_ap_info(struct mac_context *mac,
				      tpCsrNeighborRoamBSSInfo hand_off_node,
				      uint8_t vdev_id);

/**
 * csr_neighbor_roam_is_handoff_in_progress() - Check whether roam handoff is
 * in progress
 * @mac_ctx: Pointer to Global MAC structure
 * @vdev_id: vdev id
 *
 * This function returns whether handoff is in progress or not based on
 * the current neighbor roam state
 *
 * Return: true if reassoc in progress, false otherwise
 */
bool csr_neighbor_roam_is_handoff_in_progress(struct mac_context *mac,
					      uint8_t vdev_id);

/**
 * csr_neighbor_roam_free_neighbor_roam_bss_node() - Free Roam BSS node
 * @mac_ctx: Pointer to Global MAC structure
 * @roam_bss_node: Neighbor Roam BSS Node to be freed
 *
 * This function frees all the internal pointers CSR NeighborRoam BSS Info
 * and also frees the node itself
 *
 * Return: None
 */
void
csr_neighbor_roam_free_neighbor_roam_bss_node(struct mac_context *mac,
				tpCsrNeighborRoamBSSInfo roam_bss_node);

#else
static inline QDF_STATUS
csr_roam_issue_reassociate(struct mac_context *mac, uint32_t vdev_id,
			   struct bss_description *bss_desc,
			   tDot11fBeaconIEs *bcn_ies,
			   struct csr_roam_profile *roam_profile)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
csr_neighbor_roam_process_scan_results(struct mac_context *mac_ctx,
				       uint8_t vdev_id,
				       tScanResultHandle *scan_results_list)
{}

static inline void
csr_neighbor_roam_trigger_handoff(struct mac_context *mac_ctx, uint8_t vdev_id)
{}

static inline void
csr_neighbor_roam_request_handoff(struct mac_context *mac, uint8_t vdev_id)
{}

static inline bool
csr_neighbor_roam_get_handoff_ap_info(struct mac_context *mac,
				      tpCsrNeighborRoamBSSInfo pHandoffNode,
				      uint8_t vdev_id)
{
	return false;
}

static inline void
csr_neighbor_roam_free_roamable_bss_list(struct mac_context *mac_ctx,
					 tDblLinkList *llist)
{}

static inline QDF_STATUS
csr_neighbor_roam_process_scan_complete(struct mac_context *mac,
					uint8_t vdev_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
csr_roam_issue_reassociate_cmd(struct mac_context *mac, uint32_t vdev_id)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline bool
csr_neighbor_roam_is_handoff_in_progress(struct mac_context *mac,
					 uint8_t vdev_id)
{
	return false;
}

static inline QDF_STATUS
csr_neighbor_roam_candidate_found_ind_hdlr(struct mac_context *mac,
					   void *msg_buf)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_FEATURE_HOST_ROAM */
#endif /* CSR_HOST_SCAN_ROAM_H */
