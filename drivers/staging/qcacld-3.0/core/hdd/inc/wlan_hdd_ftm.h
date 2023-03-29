/*
 * Copyright (c) 2013-2018 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_HDD_FTM_H
#define WLAN_HDD_FTM_H

/**
 * DOC: wlan_hdd_ftm.h
 *
 * WLAN Host Device Driver Factory Test Mode header file
 */

#include "qdf_status.h"
#include "scheduler_api.h"
#include "cds_api.h"
#include "qdf_types.h"
#include <wlan_ptt_sock_svc.h>

struct hdd_context;

#if  defined(QCA_WIFI_FTM)
int wlan_hdd_qcmbr_unified_ioctl(struct hdd_adapter *adapter,
				 struct ifreq *ifr);
int hdd_update_cds_config_ftm(struct hdd_context *hdd_ctx);
#else
static inline int hdd_update_cds_config_ftm(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif /* QCA_WIFI_FTM */
#endif /* WLAN_HDD_FTM_H */
