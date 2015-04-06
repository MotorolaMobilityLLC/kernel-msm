/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef WLAN_HDD_FTM_H
#define WLAN_HDD_FTM_H
#include "vos_status.h"
#include "vos_mq.h"
#include "vos_api.h"
#include "msg.h"
#include "halTypes.h"
#include "vos_types.h"
#include <wlan_ptt_sock_svc.h>

#define WLAN_FTM_SUCCESS   0
#define WLAN_FTM_FAILURE   1

typedef enum {
    WLAN_FTM_INITIALIZED,
    WLAN_FTM_STOPPED,
    WLAN_FTM_STARTED,
} wlan_hdd_ftm_state;
typedef struct wlan_hdd_ftm_status_s
{
    v_U8_t ftm_state;
        /**vos event */
    vos_event_t  ftm_vos_event;

   /** completion variable for ftm command to complete*/
    struct completion ftm_comp_var;
    v_BOOL_t  IsCmdPending;
} wlan_hdd_ftm_status_t;

int wlan_hdd_ftm_open(hdd_context_t *pHddCtx);
int wlan_hdd_ftm_close(hdd_context_t *pHddCtx);

#if  defined(QCA_WIFI_FTM)
VOS_STATUS wlan_hdd_ftm_testmode_cmd(void *data, int len);
int wlan_hdd_qcmbr_unified_ioctl(hdd_adapter_t *pAdapter, struct ifreq *ifr);
#endif

#endif
