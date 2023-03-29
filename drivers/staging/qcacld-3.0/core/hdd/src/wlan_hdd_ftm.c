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

/**
 * DOC: wlan_hdd_ftm.c
 *
 * This file contains the WLAN factory test mode implementation
 */

#include "cds_sched.h"
#include <cds_api.h>
#include "sir_types.h"
#include "qdf_types.h"
#include "sir_api.h"
#include "sir_mac_prot_def.h"
#include "sme_api.h"
#include "mac_init_api.h"
#include "wlan_qct_sys.h"
#include "wlan_hdd_misc.h"
#include "i_cds_packet.h"
#include "cds_reg_service.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_lpass.h"
#include "qwlan_version.h"
#include "wma_types.h"

#ifdef QCA_WIFI_FTM

#include "wlan_hdd_cfg80211.h"
#include "hif.h"
#include <wlan_ioctl_ftm.h>
#include <wlan_cfg80211_ftm.h>

/**
 * hdd_update_cds_config_ftm() - API to update cds configuration parameters
 * for FTM mode.
 * @hdd_ctx: HDD Context
 *
 * Return: 0 on success; errno on failure
 */

int hdd_update_cds_config_ftm(struct hdd_context *hdd_ctx)
{
	struct cds_config_info *cds_cfg;
	QDF_STATUS status;
	bool self_recovery;

	cds_cfg = qdf_mem_malloc(sizeof(*cds_cfg));
	if (!cds_cfg)
		return -ENOMEM;

	status = ucfg_mlme_get_self_recovery(hdd_ctx->psoc, &self_recovery);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get self recovery ini config");
		return -EIO;
	}

	cds_cfg->driver_type = QDF_DRIVER_TYPE_MFG;
	hdd_lpass_populate_cds_config(cds_cfg, hdd_ctx);
	cds_cfg->sub_20_channel_width = WLAN_SUB_20_CH_WIDTH_NONE;
	cds_cfg->self_recovery_enabled = self_recovery;
	cds_cfg->num_vdevs = hdd_ctx->config->num_vdevs;
	cds_init_ini_config(cds_cfg);

	return 0;
}

#ifdef LINUX_QCMBR

/**
 * wlan_hdd_qcmbr_ioctl() - Standard QCMBR ioctl handler
 * @adapter: adapter upon which the ioctl was received
 * @ifr: the ioctl request
 *
 * Return: 0 on success, non-zero on error
 */
static int wlan_hdd_qcmbr_ioctl(struct hdd_adapter *adapter, struct ifreq *ifr)
{
	int ret, cmd;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (get_user(cmd, (int *)ifr->ifr_data) != 0)
		return QDF_STATUS_E_FAILURE;

	ret = wlan_ioctl_ftm_testmode_cmd(hdd_ctx->pdev, cmd,
				(uint8_t *)ifr->ifr_data + sizeof(cmd));

	return ret;
}

/**
 * wlan_hdd_qcmbr_unified_ioctl() - Unified QCMBR ioctl handler
 * @adapter: adapter upon which the ioctl was received
 * @ifr: the ioctl request
 *
 * Return: 0 on success, non-zero on error
 */
int wlan_hdd_qcmbr_unified_ioctl(struct hdd_adapter *adapter,
				 struct ifreq *ifr)
{
	int ret;

	ret = wlan_hdd_qcmbr_ioctl(adapter, ifr);

	return ret;
}

#endif /* LINUX_QCMBR */
#endif /* QCA_WIFI_FTM */
