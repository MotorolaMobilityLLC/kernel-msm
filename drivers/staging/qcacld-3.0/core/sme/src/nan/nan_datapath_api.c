/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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
 * DOC: nan_datapath_api.c
 *
 * SME NAN Data path API implementation
 */
#include <sir_common.h>
#include <ani_global.h>
#include "sme_api.h"
#include "sme_inside.h"
#include "csr_internal.h"
#include "sme_nan_datapath.h"

/**
 * csr_roam_start_ndi() - Start connection for NAN datapath
 * @mac_ctx: Global MAC context
 * @session: SME session ID
 * @profile: Configuration profile
 *
 * Return: Success or failure code
 */
QDF_STATUS csr_roam_start_ndi(struct mac_context *mac_ctx, uint32_t session,
			struct csr_roam_profile *profile)
{
	QDF_STATUS status;
	struct bss_config_param bss_cfg = {0};

	/* Build BSS configuration from profile */
	status = csr_roam_prepare_bss_config_from_profile(mac_ctx, profile,
						    &bss_cfg, NULL);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac_ctx->roam.roamSession[session].bssParams.uCfgDot11Mode
			= bss_cfg.uCfgDot11Mode;
		/* Copy profile parameters to PE session */
		csr_roam_prepare_bss_params(mac_ctx, session, profile, NULL,
			&bss_cfg, NULL);
		/*
		 * Following routine will eventually call
		 * csrRoamIssueStartBss through csrRoamCcmCfgSetCallback
		 */
		status = csr_roam_set_bss_config_cfg(mac_ctx, session, profile,
						NULL, &bss_cfg, NULL, false);
	}

	sme_debug("profile config validity: %d", status);

	return status;
}

/**
 * csr_roam_save_ndi_connected_info() - Save connected profile parameters
 * @mac_ctx: Global MAC context
 * @session_id: Session ID
 * @roam_profile: Profile given for starting BSS
 * @bssdesc: BSS description from start BSS response
 *
 * Saves NDI profile parameters into session's connected profile.
 *
 * Return: None
 */
void csr_roam_save_ndi_connected_info(struct mac_context *mac_ctx,
				      uint32_t session_id,
				      struct csr_roam_profile *roam_profile,
				      struct bss_description *bssdesc)
{
	struct csr_roam_session *roam_session;
	tCsrRoamConnectedProfile *connect_profile;

	roam_session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!roam_session) {
		sme_err("session %d not found", session_id);
		return;
	}

	connect_profile = &roam_session->connectedProfile;
	qdf_mem_zero(connect_profile, sizeof(*connect_profile));
	connect_profile->AuthType = roam_profile->negotiatedAuthType;
		connect_profile->AuthInfo = roam_profile->AuthType;
	connect_profile->EncryptionType =
		roam_profile->negotiatedUCEncryptionType;
		connect_profile->EncryptionInfo = roam_profile->EncryptionType;
	connect_profile->mcEncryptionType =
		roam_profile->negotiatedMCEncryptionType;
		connect_profile->mcEncryptionInfo =
			roam_profile->mcEncryptionType;
	connect_profile->BSSType = roam_profile->BSSType;
	connect_profile->modifyProfileFields.uapsd_mask =
		roam_profile->uapsd_mask;
	connect_profile->op_freq = bssdesc->chan_freq;
	connect_profile->beaconInterval = 0;
	qdf_mem_copy(&connect_profile->Keys, &roam_profile->Keys,
		     sizeof(roam_profile->Keys));
	csr_get_bss_id_bss_desc(bssdesc, &connect_profile->bssid);
	connect_profile->SSID.length = 0;
	csr_free_connect_bss_desc(mac_ctx, session_id);
	connect_profile->qap = false;
	connect_profile->qosConnection = false;
}

/**
 * csr_roam_update_ndp_return_params() - updates ndp return parameters
 * @mac_ctx: MAC context handle
 * @result: result of the roaming command
 * @roam_status: roam status returned to the roam command initiator
 * @roam_result: roam result returned to the roam command initiator
 * @roam_info: Roam info data structure to be updated
 *
 * Results: None
 */
void csr_roam_update_ndp_return_params(struct mac_context *mac_ctx,
					uint32_t result,
					uint32_t *roam_status,
					uint32_t *roam_result,
					struct csr_roam_info *roam_info)
{

	switch (result) {
	case eCsrStartBssSuccess:
		roam_info->ndp.ndi_create_params.reason = 0;
		roam_info->ndp.ndi_create_params.status =
					NDP_RSP_STATUS_SUCCESS;
		roam_info->ndp.ndi_create_params.sta_id = roam_info->staId;
		*roam_status = eCSR_ROAM_NDP_STATUS_UPDATE;
		*roam_result = eCSR_ROAM_RESULT_NDI_CREATE_RSP;
		break;
	case eCsrStartBssFailure:
		roam_info->ndp.ndi_create_params.status = NDP_RSP_STATUS_ERROR;
		roam_info->ndp.ndi_create_params.reason =
					NDP_NAN_DATA_IFACE_CREATE_FAILED;
		*roam_status = eCSR_ROAM_NDP_STATUS_UPDATE;
		*roam_result = eCSR_ROAM_RESULT_NDI_CREATE_RSP;
		break;
	case eCsrStopBssSuccess:
		roam_info->ndp.ndi_delete_params.reason = 0;
		roam_info->ndp.ndi_delete_params.status =
						NDP_RSP_STATUS_SUCCESS;
		*roam_status = eCSR_ROAM_NDP_STATUS_UPDATE;
		*roam_result = eCSR_ROAM_RESULT_NDI_DELETE_RSP;
		break;
	case eCsrStopBssFailure:
		roam_info->ndp.ndi_delete_params.status = NDP_RSP_STATUS_ERROR;
		roam_info->ndp.ndi_delete_params.reason =
					NDP_NAN_DATA_IFACE_DELETE_FAILED;
		*roam_status = eCSR_ROAM_NDP_STATUS_UPDATE;
		*roam_result = eCSR_ROAM_RESULT_NDI_DELETE_RSP;
		break;
	default:
		sme_err("Invalid CSR Roam result code: %d", result);
		break;
	}
}
