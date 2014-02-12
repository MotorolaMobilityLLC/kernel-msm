/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( HDD_INCLUDES_H__ )
#define HDD_INCLUDES_H__

/**===========================================================================
  
  \file  wlan_hdd_includes.h
  
  \brief Internal includes for the Linux HDD 
  
               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.
  
  ==========================================================================*/
  
/* $HEADER$ */
  
/*--------------------------------------------------------------------------- 
  Include files
  -------------------------------------------------------------------------*/ 

// throw all the includes in here f to get the .c files  in the HDD to compile.

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <linux/if_arp.h>


#include <vos_api.h>

#include <sme_Api.h>
#include <wlan_qct_tl.h>

#include "wlan_hdd_assoc.h"
#include "wlan_hdd_dp_utils.h"
#include "wlan_hdd_mib.h"
#include "wlan_hdd_wext.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_tx_rx.h"

#ifdef FEATURE_OEM_DATA_SUPPORT
/*include for oem data req specific structures*/
/*and function declarations*/
#include "wlan_hdd_oemdata.h" 
#endif

#endif    // end #if !defined( HDD_INCLUDES_H__ )
