/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

#if !defined( WLAN_HDD_HOSTAPD_H )
#define WLAN_HDD_HOSTAPD_H

/**===========================================================================

  \file  WLAN_HDD_HOSTAPD_H.h

  \brief Linux HDD HOSTAPD include file

  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <vos_list.h>
#include <vos_types.h>

#include <wlan_qct_tl.h>
#include <wlan_hdd_main.h>

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/

/* max length of command string in hostapd ioctl */
#define HOSTAPD_IOCTL_COMMAND_STRLEN_MAX   8192

hdd_adapter_t* hdd_wlan_create_ap_dev( hdd_context_t *pHddCtx, tSirMacAddr macAddr, tANI_U8 *name);

VOS_STATUS hdd_register_hostapd(hdd_adapter_t *pAdapter, tANI_U8 rtnl_held);

VOS_STATUS hdd_unregister_hostapd(hdd_adapter_t *pAdapter, bool rtnl_held);

eCsrAuthType
hdd_TranslateRSNToCsrAuthType( u_int8_t auth_suite[4]);

eCsrEncryptionType
hdd_TranslateRSNToCsrEncryptionType(u_int8_t cipher_suite[4]);

eCsrEncryptionType
hdd_TranslateRSNToCsrEncryptionType(u_int8_t cipher_suite[4]);

eCsrAuthType
hdd_TranslateWPAToCsrAuthType(u_int8_t auth_suite[4]);

eCsrEncryptionType
hdd_TranslateWPAToCsrEncryptionType(u_int8_t cipher_suite[4]);

VOS_STATUS hdd_softap_sta_deauth(hdd_adapter_t*, struct tagCsrDelStaParams*);
void hdd_softap_sta_disassoc(hdd_adapter_t*, struct tagCsrDelStaParams*);
void hdd_softap_tkip_mic_fail_counter_measure(hdd_adapter_t*,v_BOOL_t);
int hdd_softap_unpackIE( tHalHandle halHandle,
                eCsrEncryptionType *pEncryptType,
                eCsrEncryptionType *mcEncryptType,
                eCsrAuthType *pAuthType,
                v_BOOL_t *pMFPCapable,
                v_BOOL_t *pMFPRequired,
                u_int16_t gen_ie_len,
                u_int8_t *gen_ie );

VOS_STATUS hdd_hostapd_SAPEventCB( tpSap_Event pSapEvent, v_PVOID_t usrDataForCallback);
VOS_STATUS hdd_init_ap_mode( hdd_adapter_t *pAdapter );
void hdd_set_ap_ops( struct net_device *pWlanHostapdDev );
int hdd_hostapd_stop (struct net_device *dev);
void hdd_hostapd_channel_wakelock_init(hdd_context_t *pHddCtx);
void hdd_hostapd_channel_wakelock_deinit(hdd_context_t *pHddCtx);
#ifdef FEATURE_WLAN_FORCE_SAP_SCC
void hdd_restart_softap (hdd_context_t *pHddCtx, hdd_adapter_t *pAdapter);
#endif /* FEATURE_WLAN_FORCE_SAP_SCC */
#ifdef QCA_HT_2040_COEX
VOS_STATUS hdd_set_sap_ht2040_mode(hdd_adapter_t *pHostapdAdapter,
                                   tANI_U8 channel_type);
#endif

#ifdef SAP_AUTH_OFFLOAD
void hdd_set_sap_auth_offload(hdd_adapter_t *pHostapdAdapter,
                              bool enabled);

int hdd_set_client_block_info(hdd_adapter_t *padapter);
#else
static inline int hdd_set_client_block_info(hdd_adapter_t *padapter)
{
	return 0;
}

static inline void
hdd_set_sap_auth_offload(hdd_adapter_t *pHostapdAdapter, bool enabled)
{
}
#endif /* SAP_AUTH_OFFLOAD */
int hdd_softap_set_channel_change(struct net_device *dev, int target_channel);

/**
 * hdd_is_sta_connection_pending() - This function will check if sta connection
 *                                   is pending or not.
 * @hdd_ctx: pointer to hdd context
 *
 * This function will return the status of flag is_sta_connection_pending
 *
 * Return: true or false
 */
static inline bool
hdd_is_sta_connection_pending(hdd_context_t *hdd_ctx)
{
    bool status;
    spin_lock(&hdd_ctx->sta_update_info_lock);
    status = hdd_ctx->is_sta_connection_pending;
    spin_unlock(&hdd_ctx->sta_update_info_lock);
    return status;
}

/**
 * hdd_change_sta_conn_pending_status() - This function will change the value
 *                                        of is_sta_connection_pending
 * @hdd_ctx: pointer to hdd context
 * @value: value to set
 *
 * This function will change the value of is_sta_connection_pending
 *
 * Return: none
 */
static inline void
hdd_change_sta_conn_pending_status(hdd_context_t *hdd_ctx,
                                   bool value)
{
    spin_lock(&hdd_ctx->sta_update_info_lock);
    hdd_ctx->is_sta_connection_pending = value;
    spin_unlock(&hdd_ctx->sta_update_info_lock);
}

/**
 * hdd_is_sap_restart_required() - This function will check if sap restart
 *                                 is pending or not.
 * @hdd_ctx: pointer to hdd context.
 *
 * This function will return the status of flag is_sap_restart_required.
 *
 * Return: true or false
 */
static inline bool
hdd_is_sap_restart_required(hdd_context_t *hdd_ctx)
{
    bool status;
    spin_lock(&hdd_ctx->sap_update_info_lock);
    status = hdd_ctx->is_sap_restart_required;
    spin_unlock(&hdd_ctx->sap_update_info_lock);
    return status;
}

/**
 * hdd_change_sap_restart_required_status() - This function will change the
 *                                            value of is_sap_restart_required
 * @hdd_ctx: pointer to hdd context
 * @value: value to set
 *
 * This function will change the value of is_sap_restart_required
 *
 * Return: none
 */
static inline void
hdd_change_sap_restart_required_status(hdd_context_t *hdd_ctx,
                                       bool value)
{
    spin_lock(&hdd_ctx->sap_update_info_lock);
    hdd_ctx->is_sap_restart_required = value;
    spin_unlock(&hdd_ctx->sap_update_info_lock);
}

#endif    // end #if !defined( WLAN_HDD_HOSTAPD_H )
