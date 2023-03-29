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
 * DOC: sme_nan_datapath.h
 *
 * SME NAN Data path API specification
 */

#ifndef __SME_NAN_DATAPATH_H
#define __SME_NAN_DATAPATH_H

#include "csr_inside_api.h"

#ifdef WLAN_FEATURE_NAN
/* Start NDI BSS */
QDF_STATUS csr_roam_start_ndi(struct mac_context *mac_ctx, uint32_t session_id,
			      struct csr_roam_profile *profile);

void csr_roam_save_ndi_connected_info(struct mac_context *mac_ctx,
				      uint32_t session_id,
				      struct csr_roam_profile *roam_profile,
				      struct bss_description *bss_desc);

void csr_roam_update_ndp_return_params(struct mac_context *mac_ctx,
					uint32_t result,
					uint32_t *roam_status,
					uint32_t *roam_result,
					struct csr_roam_info *roam_info);

#else /* WLAN_FEATURE_NAN */
/* Start NDI BSS */
static inline QDF_STATUS csr_roam_start_ndi(struct mac_context *mac_ctx,
					uint32_t session_id,
					struct csr_roam_profile *profile)
{
	return QDF_STATUS_SUCCESS;
}

static inline void csr_roam_save_ndi_connected_info(struct mac_context *mac_ctx,
					uint32_t session_id,
					struct csr_roam_profile *roam_profile,
					struct bss_description *bss_desc)
{
}

static inline void csr_roam_update_ndp_return_params(struct mac_context *mac_ctx,
					uint32_t result,
					uint32_t *roam_status,
					uint32_t *roam_result,
					struct csr_roam_info *roam_info)
{
}

#endif /* WLAN_FEATURE_NAN */

#endif /* __SME_NAN_DATAPATH_H */
