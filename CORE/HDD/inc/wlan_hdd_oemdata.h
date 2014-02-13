/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifdef FEATURE_OEM_DATA_SUPPORT

/**===========================================================================
  
  \file  wlan_hdd_oemdata.h
  
  \brief Internal includes for the oem data
  
               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.
  
  ==========================================================================*/


#ifndef __WLAN_HDD_OEM_DATA_H__
#define __WLAN_HDD_OEM_DATA_H__

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 134
#endif

#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1968
#endif

struct iw_oem_data_req
{
    v_U8_t                  oemDataReq[OEM_DATA_REQ_SIZE];
};

int iw_set_oem_data_req(
        struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra);

int iw_get_oem_data_rsp(
        struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra);

struct iw_oem_data_rsp
{
    tANI_U8           oemDataRsp[OEM_DATA_RSP_SIZE];
};

#endif //__WLAN_HDD_OEM_DATA_H__

#endif //FEATURE_OEM_DATA_SUPPORT
