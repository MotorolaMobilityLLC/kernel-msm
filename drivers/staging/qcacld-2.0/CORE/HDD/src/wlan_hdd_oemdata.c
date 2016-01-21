/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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

#ifdef FEATURE_OEM_DATA_SUPPORT

/*================================================================================
    \file wlan_hdd_oemdata.c

    \brief Linux Wireless Extensions for oem data req/rsp

    $Id: wlan_hdd_oemdata.c,v 1.34 2010/04/15 01:49:23 -- VINAY

================================================================================*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <wlan_hdd_includes.h>
#include <net/arp.h>
#include <vos_sched.h>
#include "qwlan_version.h"
#include "vos_utils.h"
#include "wma.h"
static struct hdd_context_s *pHddCtx;


/**---------------------------------------------------------------------------

  \brief iw_get_oem_data_cap()

  This function gets the capability information for OEM Data Request
  and Response.

  \param - dev  - Pointer to the net device
         - info - Pointer to the t_iw_oem_data_cap
         - wrqu - Pointer to the iwreq data
         - extra - Pointer to the data

  \return - 0 for success, non zero for failure

----------------------------------------------------------------------------*/
int iw_get_oem_data_cap(
        struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    eHalStatus status;
    t_iw_oem_data_cap oemDataCap;
    t_iw_oem_data_cap *pHddOemDataCap;
    hdd_adapter_t *pAdapter = netdev_priv(dev);
    hdd_context_t *pHddContext;
    hdd_config_t *pConfig;
    tANI_U32 numChannels;
    tANI_U8 chanList[OEM_CAP_MAX_NUM_CHANNELS];
    tANI_U32 i;
    int ret;

    ENTER();

    if (!pAdapter)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s:Invalid context, pAdapter is null", __func__);
       return -EINVAL;
    }

    pHddContext = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddContext);
    if (0 != ret)
      return ret;

    pConfig = pHddContext->cfg_ini;

    do
    {
       vos_mem_zero(&oemDataCap, sizeof(oemDataCap));
       strlcpy(oemDataCap.oem_target_signature, OEM_TARGET_SIGNATURE,
               OEM_TARGET_SIGNATURE_LEN);
       oemDataCap.oem_target_type = pHddContext->target_type;
       oemDataCap.oem_fw_version = pHddContext->target_fw_version;
       oemDataCap.driver_version.major = QWLAN_VERSION_MAJOR;
       oemDataCap.driver_version.minor = QWLAN_VERSION_MINOR;
       oemDataCap.driver_version.patch = QWLAN_VERSION_PATCH;
       oemDataCap.driver_version.build = QWLAN_VERSION_BUILD;
       oemDataCap.allowed_dwell_time_min = pConfig->nNeighborScanMinChanTime;
       oemDataCap.allowed_dwell_time_max = pConfig->nNeighborScanMaxChanTime;
       oemDataCap.curr_dwell_time_min =
               sme_getNeighborScanMinChanTime(pHddContext->hHal,
                                              pAdapter->sessionId);
       oemDataCap.curr_dwell_time_max =
               sme_getNeighborScanMaxChanTime(pHddContext->hHal,
                                              pAdapter->sessionId);
       oemDataCap.supported_bands = pConfig->nBandCapability;

       /* request for max num of channels */
       numChannels = WNI_CFG_VALID_CHANNEL_LIST_LEN;
       status = sme_GetCfgValidChannels(pHddContext->hHal,
                                        &chanList[0],
                                        &numChannels);
       if (eHAL_STATUS_SUCCESS != status)
       {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s:failed to get valid channel list", __func__);
         return -ENOENT;
       }
       else
       {
         /* make sure num channels is not more than chan list array */
         if (numChannels > OEM_CAP_MAX_NUM_CHANNELS)
         {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "%s:Num of channels(%d) more than length(%d) of chanlist",
                     __func__, numChannels, OEM_CAP_MAX_NUM_CHANNELS);
           return -ENOMEM;
         }

         oemDataCap.num_channels = numChannels;
         for (i = 0; i < numChannels; i++)
         {
           oemDataCap.channel_list[i] = chanList[i];
         }
       }

       pHddOemDataCap = (t_iw_oem_data_cap *)(extra);
       vos_mem_copy(pHddOemDataCap, &oemDataCap, sizeof(*pHddOemDataCap));
    } while (0);

    EXIT();
    return 0;
}

/**---------------------------------------------------------------------------

  \brief send_oem_reg_rsp_nlink_msg() - send oem registration response

  This function sends oem message to registered application process

  \param -
     - none

  \return - none

  --------------------------------------------------------------------------*/
static void send_oem_reg_rsp_nlink_msg(void)
{
   struct sk_buff *skb;
   struct nlmsghdr *nlh;
   tAniMsgHdr *aniHdr;
   tANI_U8 *buf;
   tANI_U8 *numInterfaces;
   tANI_U8 *deviceMode;
   tANI_U8 *vdevId;
   hdd_adapter_list_node_t *pAdapterNode = NULL;
   hdd_adapter_list_node_t *pNext = NULL;
   hdd_adapter_t *pAdapter = NULL;
   VOS_STATUS status = 0;

   /* OEM message is always to a specific process and cannot be a broadcast */
   if (pHddCtx->oem_pid == 0)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid dest pid", __func__);
      return;
   }

   skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), GFP_KERNEL);
   if (skb == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: alloc_skb failed", __func__);
      return;
   }

   nlh = (struct nlmsghdr *)skb->data;
   nlh->nlmsg_pid = 0;  /* from kernel */
   nlh->nlmsg_flags = 0;
   nlh->nlmsg_seq = 0;
   nlh->nlmsg_type = WLAN_NL_MSG_OEM;
   aniHdr = NLMSG_DATA(nlh);
   aniHdr->type = ANI_MSG_APP_REG_RSP;

   /* Fill message body:
    *   First byte will be number of interfaces, followed by
    *   two bytes for each interfaces
    *     - one byte for device mode
    *     - one byte for vdev id
    */
   buf = (char *) ((char *)aniHdr + sizeof(tAniMsgHdr));
   numInterfaces = buf++;
   *numInterfaces = 0;

   /* Iterate through each of the adapters and fill device mode and vdev id */
   status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);
   while ((VOS_STATUS_SUCCESS == status) && pAdapterNode)
   {
     pAdapter = pAdapterNode->pAdapter;
     if (pAdapter)
     {
       deviceMode = buf++;
       vdevId = buf++;
       *deviceMode = pAdapter->device_mode;
       *vdevId = pAdapter->sessionId;
       (*numInterfaces)++;
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "%s: numInterfaces: %d, deviceMode: %d, vdevId: %d",
                 __func__, *numInterfaces, *deviceMode, *vdevId);
     }
     status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
     pAdapterNode = pNext;
   }

   aniHdr->length = sizeof(tANI_U8) + (*numInterfaces) * 2 * sizeof(tANI_U8);
   nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));

   skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s: sending App Reg Response length (%d) to process pid (%d)",
             __func__, aniHdr->length, pHddCtx->oem_pid);

   (void)nl_srv_ucast(skb, pHddCtx->oem_pid, MSG_DONTWAIT);

   return;
}

/**---------------------------------------------------------------------------

  \brief send_oem_err_rsp_nlink_msg() - send oem error response

  This function sends error response to oem app

  \param -
     - app_pid - PID of oem application process

  \return - none

  --------------------------------------------------------------------------*/
static void send_oem_err_rsp_nlink_msg(v_SINT_t app_pid, tANI_U8 error_code)
{
   struct sk_buff *skb;
   struct nlmsghdr *nlh;
   tAniMsgHdr *aniHdr;
   tANI_U8 *buf;

   skb = alloc_skb(NLMSG_SPACE(WLAN_NL_MAX_PAYLOAD), GFP_KERNEL);
   if (skb == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: alloc_skb failed", __func__);
      return;
   }

   nlh = (struct nlmsghdr *)skb->data;
   nlh->nlmsg_pid = 0;  /* from kernel */
   nlh->nlmsg_flags = 0;
   nlh->nlmsg_seq = 0;
   nlh->nlmsg_type = WLAN_NL_MSG_OEM;
   aniHdr = NLMSG_DATA(nlh);
   aniHdr->type = ANI_MSG_OEM_ERROR;
   aniHdr->length = sizeof(tANI_U8);
   nlh->nlmsg_len = NLMSG_LENGTH(sizeof(tAniMsgHdr) + aniHdr->length);

   /* message body will contain one byte of error code */
   buf = (char *) ((char *) aniHdr + sizeof(tAniMsgHdr));
   *buf = error_code;

   skb_put(skb, NLMSG_SPACE(sizeof(tAniMsgHdr) + aniHdr->length));

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s: sending oem error response to process pid (%d)",
             __func__, app_pid);

   (void)nl_srv_ucast(skb, app_pid, MSG_DONTWAIT);

   return;
}

/**---------------------------------------------------------------------------

  \brief send_oem_data_rsp_msg() - send oem data response

  This function sends oem data rsp message to registered application process
  over the netlink socket.

  \param -
     - oemDataRsp - Pointer to OEM Data Response struct

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
void send_oem_data_rsp_msg(int length, tANI_U8 *oemDataRsp)
{
   struct sk_buff *skb;
   struct nlmsghdr *nlh;
   tAniMsgHdr *aniHdr;
   tANI_U8 *oemData;

   /* OEM message is always to a specific process and cannot be a broadcast */
   if (pHddCtx->oem_pid == 0)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid dest pid", __func__);
      return;
   }

   if (length > OEM_DATA_RSP_SIZE)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid length of Oem Data response", __func__);
      return;
   }

   skb = alloc_skb(NLMSG_SPACE(sizeof(tAniMsgHdr) + OEM_DATA_RSP_SIZE),
                   GFP_KERNEL);
   if (skb == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: alloc_skb failed", __func__);
      return;
   }

   nlh = (struct nlmsghdr *)skb->data;
   nlh->nlmsg_pid = 0;  /* from kernel */
   nlh->nlmsg_flags = 0;
   nlh->nlmsg_seq = 0;
   nlh->nlmsg_type = WLAN_NL_MSG_OEM;
   aniHdr = NLMSG_DATA(nlh);
   aniHdr->type = ANI_MSG_OEM_DATA_RSP;

   aniHdr->length = length;
   nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));
   oemData = (tANI_U8 *) ((char *)aniHdr + sizeof(tAniMsgHdr));
   vos_mem_copy(oemData, oemDataRsp, length);

   skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s: sending Oem Data Response of len (%d) to process pid (%d)",
             __func__, length, pHddCtx->oem_pid);

   (void)nl_srv_ucast(skb, pHddCtx->oem_pid, MSG_DONTWAIT);

   return;
}

/**---------------------------------------------------------------------------

  \brief oem_process_data_req_msg() - process oem data request

  This function sends oem message to SME

  \param -
     - oemDataLen - Length to OEM Data buffer
     - oemData - Pointer to OEM Data buffer

  \return - eHalStatus enumeration

  --------------------------------------------------------------------------*/
static eHalStatus oem_process_data_req_msg(int oemDataLen, char *oemData)
{
   hdd_adapter_t *pAdapter = NULL;
   tOemDataReqConfig oemDataReqConfig;
   tANI_U32 oemDataReqID = 0;
   eHalStatus status = eHAL_STATUS_SUCCESS;

   /* for now, STA interface only */
   pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
   if (!pAdapter)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: No adapter for STA mode", __func__);
      return eHAL_STATUS_FAILURE;
   }

   if (!oemData)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: oemData is null", __func__);
      return eHAL_STATUS_FAILURE;
   }

   vos_mem_zero(&oemDataReqConfig, sizeof(tOemDataReqConfig));

   oemDataReqConfig.data = vos_mem_malloc(oemDataLen);
   if (!oemDataReqConfig.data) {
      hddLog(LOGE, FL("malloc failed for data req buffer"));
      return eHAL_STATUS_FAILED_ALLOC;
   }

   oemDataReqConfig.data_len = oemDataLen;
   vos_mem_copy(oemDataReqConfig.data, oemData, oemDataLen);

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s: calling sme_OemDataReq", __func__);

   status = sme_OemDataReq(pHddCtx->hHal,
                           pAdapter->sessionId,
                           &oemDataReqConfig,
                           &oemDataReqID);

   vos_mem_free(oemDataReqConfig.data);
   return status;
}

/**---------------------------------------------------------------------------

  \brief oem_process_channel_info_req_msg() - process oem channel_info request

  This function responds with channel info to oem process

  \param -
     - numOfChannels - number of channels
     - chanList - channel list

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
static int oem_process_channel_info_req_msg(int numOfChannels, char *chanList)
{
   struct sk_buff *skb;
   struct nlmsghdr *nlh;
   tAniMsgHdr *aniHdr;
   tHddChannelInfo *pHddChanInfo;
   tHddChannelInfo hddChanInfo;
   tANI_U8 chanId;
   tANI_U32 reg_info_1;
   tANI_U32 reg_info_2;
   eHalStatus status = eHAL_STATUS_FAILURE;
   int i;
   tANI_U8 *buf;

   /* OEM message is always to a specific process and cannot be a broadcast */
   if (pHddCtx->oem_pid == 0)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid dest pid", __func__);
      return -1;
   }

   skb = alloc_skb(NLMSG_SPACE(sizeof(tAniMsgHdr) + sizeof(tANI_U8) +
                   numOfChannels * sizeof(tHddChannelInfo)), GFP_KERNEL);
   if (skb == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: alloc_skb failed", __func__);
      return -1;
   }

   nlh = (struct nlmsghdr *)skb->data;
   nlh->nlmsg_pid = 0;  /* from kernel */
   nlh->nlmsg_flags = 0;
   nlh->nlmsg_seq = 0;
   nlh->nlmsg_type = WLAN_NL_MSG_OEM;
   aniHdr = NLMSG_DATA(nlh);
   aniHdr->type = ANI_MSG_CHANNEL_INFO_RSP;

   aniHdr->length = sizeof(tANI_U8) + numOfChannels * sizeof(tHddChannelInfo);
   nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));

   /* First byte of message body will have num of channels */
   buf = (char *) ((char *)aniHdr + sizeof(tAniMsgHdr));
   *buf++ = numOfChannels;

   /* Next follows channel info struct for each channel id.
    * If chan id is wrong or SME returns failure for a channel
    * then fill in 0 in channel info for that particular channel
    */
   for (i = 0 ; i < numOfChannels; i++)
   {
      pHddChanInfo = (tHddChannelInfo *) ((char *) buf +
                                        i * sizeof(tHddChannelInfo));

      chanId = chanList[i];
      status = sme_getRegInfo(pHddCtx->hHal, chanId,
                              &reg_info_1, &reg_info_2);
      if (eHAL_STATUS_SUCCESS == status)
      {
         /* band center freq1, and freq2 depends on peer's capability
          * and at this time we might not be associated on the given channel,
          * so fill freq1=mhz, and freq2=0
          */
         hddChanInfo.chan_id = chanId;
         hddChanInfo.reserved0 = 0;
         hddChanInfo.mhz = vos_chan_to_freq(chanId);
         hddChanInfo.band_center_freq1 = hddChanInfo.mhz;
         hddChanInfo.band_center_freq2 = 0;

         /* set only DFS flag in info, rest of the fields will be filled in
          *  by the OEM App
          */
         hddChanInfo.info = 0;
         if (NV_CHANNEL_DFS == vos_nv_getChannelEnabledState(chanId))
             WMI_SET_CHANNEL_FLAG(&hddChanInfo, WMI_CHAN_FLAG_DFS);

         hddChanInfo.reg_info_1 = reg_info_1;
         hddChanInfo.reg_info_2 = reg_info_2;
      }
      else
      {
         /* chanId passed to sme_getRegInfo is not valid, fill in zeros
          * in channel info struct
          */
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: sme_getRegInfo failed for chan (%d), return info 0",
                   __func__, chanId);
         hddChanInfo.chan_id = chanId;
         hddChanInfo.reserved0 = 0;
         hddChanInfo.mhz = 0;
         hddChanInfo.band_center_freq1 = 0;
         hddChanInfo.band_center_freq2 = 0;
         hddChanInfo.info = 0;
         hddChanInfo.reg_info_1 = 0;
         hddChanInfo.reg_info_2 = 0;
      }
      vos_mem_copy(pHddChanInfo, &hddChanInfo, sizeof(tHddChannelInfo));
   }

   skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "%s: sending channel info resp for num channels (%d) to pid (%d)",
             __func__, numOfChannels, pHddCtx->oem_pid);

   (void)nl_srv_ucast(skb, pHddCtx->oem_pid, MSG_DONTWAIT);

   return 0;
}

/**---------------------------------------------------------------------------

  \brief hdd_SendPeerStatusIndToOemApp()

  This function sends peer status indication to registered oem application

  \param -
     - peerMac : MAC address of peer
     - peerStatus : ePeerConnected or ePeerDisconnected
     - peerTimingMeasCap : 0: RTT/RTT2, 1: RTT3. Default is 0
     - sessionId : SME session id, i.e. vdev_id
     - chanId: operating channel id

  \return - None

  --------------------------------------------------------------------------*/
void hdd_SendPeerStatusIndToOemApp(v_MACADDR_t *peerMac,
                                   tANI_U8 peerStatus,
                                   tANI_U8 peerTimingMeasCap,
                                   tANI_U8 sessionId,
                                   tSirSmeChanInfo *chan_info)
{
   struct sk_buff *skb;
   struct nlmsghdr *nlh;
   tAniMsgHdr *aniHdr;
   tPeerStatusInfo *pPeerInfo;


   if (!pHddCtx || !pHddCtx->hHal)
   {
     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: Either HDD Ctx is null or Hal Ctx is null", __func__);
     return;
   }

   /* check if oem app has registered and pid is valid */
   if ((!pHddCtx->oem_app_registered) || (pHddCtx->oem_pid == 0))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: OEM app is not registered(%d) or pid is invalid(%d)",
                __func__, pHddCtx->oem_app_registered, pHddCtx->oem_pid);
      return;
   }

   skb = alloc_skb(NLMSG_SPACE(sizeof(tAniMsgHdr) + sizeof(tPeerStatusInfo)),
                   GFP_KERNEL);
   if (skb == NULL)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: alloc_skb failed", __func__);
      return;
   }

   nlh = (struct nlmsghdr *)skb->data;
   nlh->nlmsg_pid = 0;  /* from kernel */
   nlh->nlmsg_flags = 0;
   nlh->nlmsg_seq = 0;
   nlh->nlmsg_type = WLAN_NL_MSG_OEM;
   aniHdr = NLMSG_DATA(nlh);
   aniHdr->type = ANI_MSG_PEER_STATUS_IND;

   aniHdr->length = sizeof(tPeerStatusInfo);
   nlh->nlmsg_len = NLMSG_LENGTH((sizeof(tAniMsgHdr) + aniHdr->length));

   pPeerInfo = (tPeerStatusInfo *) ((char *)aniHdr + sizeof(tAniMsgHdr));

   vos_mem_copy(pPeerInfo->peer_mac_addr, peerMac->bytes,
                sizeof(peerMac->bytes));
   pPeerInfo->peer_status = peerStatus;
   pPeerInfo->vdev_id = sessionId;
   /* peerTimingMeasCap - bit mask for timing and fine timing Meas Cap */
   pPeerInfo->peer_capability = peerTimingMeasCap;
   pPeerInfo->reserved0 = 0;

   if (chan_info) {
       pPeerInfo->peer_chan_info.chan_id = chan_info->chan_id;
       pPeerInfo->peer_chan_info.reserved0 = 0;
       pPeerInfo->peer_chan_info.mhz = chan_info->mhz;
       pPeerInfo->peer_chan_info.band_center_freq1 =
                                      chan_info->band_center_freq1;
       pPeerInfo->peer_chan_info.band_center_freq2 =
                                      chan_info->band_center_freq2;
       pPeerInfo->peer_chan_info.info = chan_info->info;
       pPeerInfo->peer_chan_info.reg_info_1 = chan_info->reg_info_1;
       pPeerInfo->peer_chan_info.reg_info_2 = chan_info->reg_info_2;
   } else {
       pPeerInfo->peer_chan_info.chan_id = 0;
       pPeerInfo->peer_chan_info.reserved0 = 0;
       pPeerInfo->peer_chan_info.mhz = 0;
       pPeerInfo->peer_chan_info.band_center_freq1 = 0;
       pPeerInfo->peer_chan_info.band_center_freq2 = 0;
       pPeerInfo->peer_chan_info.info = 0;
       pPeerInfo->peer_chan_info.reg_info_1 = 0;
       pPeerInfo->peer_chan_info.reg_info_2 = 0;
   }
   skb_put(skb, NLMSG_SPACE((sizeof(tAniMsgHdr) + aniHdr->length)));

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
            "%s: sending peer "MAC_ADDRESS_STR
            " status(%d), peerTimingMeasCap(%d), vdevId(%d), chanId(%d)"
            " to oem app pid(%d), center freq 1 (%d), center freq 2 (%d),"
            " info (0x%x), frequency (%d),reg info 1 (0x%x),"
            " reg info 2 (0x%x)",__func__, MAC_ADDR_ARRAY(peerMac->bytes),
             peerStatus, peerTimingMeasCap, sessionId,
             pPeerInfo->peer_chan_info.chan_id, pHddCtx->oem_pid,
             pPeerInfo->peer_chan_info.band_center_freq1,
             pPeerInfo->peer_chan_info.band_center_freq2,
             pPeerInfo->peer_chan_info.info,
             pPeerInfo->peer_chan_info.mhz,
             pPeerInfo->peer_chan_info.reg_info_1,
             pPeerInfo->peer_chan_info.reg_info_2);

   (void)nl_srv_ucast(skb, pHddCtx->oem_pid, MSG_DONTWAIT);

   return;
}

/*
 * Callback function invoked by Netlink service for all netlink
 * messages (from user space) addressed to WLAN_NL_MSG_OEM
 */

/**
 * oem_msg_callback() - callback invoked by netlink service
 * @skb:    skb with netlink message
 *
 * This function gets invoked by netlink service when a message
 * is received from user space addressed to WLAN_NL_MSG_OEM
 *
 * Return: zero on success
 *         On error, error number will be returned.
 */
static int oem_msg_callback(struct sk_buff *skb)
{
   struct nlmsghdr *nlh;
   tAniMsgHdr *msg_hdr;
   int ret;
   char *sign_str = NULL;
   nlh = (struct nlmsghdr *)skb->data;

   if (!nlh) {
       hddLog(LOGE, FL("Netlink header null"));
       return -EPERM;
   }

   ret = wlan_hdd_validate_context(pHddCtx);
   if (0 != ret) {
       hddLog(LOGE, FL("HDD context is not valid"));
       return ret;
   }

   msg_hdr = NLMSG_DATA(nlh);

   if (!msg_hdr) {
       hddLog(LOGE, FL("Message header null"));
       send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid, OEM_ERR_NULL_MESSAGE_HEADER);
       return -EPERM;
   }

   if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(tAniMsgHdr) + msg_hdr->length)) {
       hddLog(LOGE, FL("Invalid nl msg len, nlh->nlmsg_len (%d), msg_hdr->len (%d)"),
              nlh->nlmsg_len, msg_hdr->length);
       send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                  OEM_ERR_INVALID_MESSAGE_LENGTH);
       return -EPERM;
   }

   switch (msg_hdr->type) {
   case ANI_MSG_APP_REG_REQ:
      /* Registration request is only allowed for Qualcomm Application */
      hddLog(LOG1, FL("Received App Req Req from App process pid(%d), len(%d)"),
                   nlh->nlmsg_pid, msg_hdr->length);

      sign_str = (char *)((char *)msg_hdr + sizeof(tAniMsgHdr));
      if ((OEM_APP_SIGNATURE_LEN == msg_hdr->length) &&
          (0 == strncmp(sign_str, OEM_APP_SIGNATURE_STR,
                        OEM_APP_SIGNATURE_LEN))) {
          hddLog(LOG1, FL("Valid App Req Req from oem app process pid(%d)"),
                       nlh->nlmsg_pid);

          pHddCtx->oem_app_registered = TRUE;
          pHddCtx->oem_pid = nlh->nlmsg_pid;
          send_oem_reg_rsp_nlink_msg();
      } else {
          hddLog(LOGE, FL("Invalid signature in App Reg Request from pid(%d)"),
                 nlh->nlmsg_pid);
          send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                       OEM_ERR_INVALID_SIGNATURE);
          return -EPERM;
      }
      break;

   case ANI_MSG_OEM_DATA_REQ:
      hddLog(LOG1, FL("Received Oem Data Request length(%d) from pid: %d"),
                   msg_hdr->length, nlh->nlmsg_pid);

      if ((!pHddCtx->oem_app_registered) ||
          (nlh->nlmsg_pid != pHddCtx->oem_pid)) {
          /* either oem app is not registered yet or pid is different */
          hddLog(LOGE, FL("OEM DataReq: app not registered(%d) or incorrect pid(%d)"),
                 pHddCtx->oem_app_registered, nlh->nlmsg_pid);
          send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                       OEM_ERR_APP_NOT_REGISTERED);
          return -EPERM;
      }

      if ((!msg_hdr->length) || (OEM_DATA_REQ_SIZE < msg_hdr->length)) {
          hddLog(LOGE, FL("Invalid length (%d) in Oem Data Request"),
                       msg_hdr->length);
          send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                       OEM_ERR_INVALID_MESSAGE_LENGTH);
          return -EPERM;
      }
      oem_process_data_req_msg(msg_hdr->length,
                              (char *) ((char *)msg_hdr +
                              sizeof(tAniMsgHdr)));
      break;

   case ANI_MSG_CHANNEL_INFO_REQ:
      hddLog(LOG1,
             FL("Received channel info request, num channel(%d) from pid: %d"),
             msg_hdr->length, nlh->nlmsg_pid);

      if ((!pHddCtx->oem_app_registered) ||
          (nlh->nlmsg_pid != pHddCtx->oem_pid)) {
          /* either oem app is not registered yet or pid is different */
          hddLog(LOGE,
                 FL("Chan InfoReq: app not registered(%d) or incorrect pid(%d)"),
                 pHddCtx->oem_app_registered, nlh->nlmsg_pid);
          send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                     OEM_ERR_APP_NOT_REGISTERED);
          return -EPERM;
      }

      /* message length contains list of channel ids */
      if ((!msg_hdr->length) ||
          (WNI_CFG_VALID_CHANNEL_LIST_LEN < msg_hdr->length)) {
          hddLog(LOGE,
                 FL("Invalid length (%d) in channel info request"),
                 msg_hdr->length);
          send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                    OEM_ERR_INVALID_MESSAGE_LENGTH);
          return -EPERM;
      }
      oem_process_channel_info_req_msg(msg_hdr->length,
                            (char *)((char*)msg_hdr + sizeof(tAniMsgHdr)));
      break;

   default:
      hddLog(LOGE,
             FL("Received Invalid message type (%d), length (%d)"),
             msg_hdr->type, msg_hdr->length);
      send_oem_err_rsp_nlink_msg(nlh->nlmsg_pid,
                                 OEM_ERR_INVALID_MESSAGE_TYPE);
      return -EPERM;
   }
   return 0;
}

static int __oem_msg_callback(struct sk_buff *skb)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = oem_msg_callback(skb);
    vos_ssr_unprotect(__func__);

    return ret;
}

/**---------------------------------------------------------------------------

  \brief oem_activate_service() - Activate oem message handler

  This function registers a handler to receive netlink message from
  an OEM application process.

  \param -
     - pAdapter - pointer to HDD adapter

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/
int oem_activate_service(void *pAdapter)
{
   pHddCtx = (struct hdd_context_s*) pAdapter;

   /* Register the msg handler for msgs addressed to WLAN_NL_MSG_OEM */
   nl_srv_register(WLAN_NL_MSG_OEM, __oem_msg_callback);
   return 0;
}



#endif
