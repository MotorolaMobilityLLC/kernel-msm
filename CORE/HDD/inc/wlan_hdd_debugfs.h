/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _WLAN_HDD_DEBUGFS_H
#define _WLAN_HDD_DEBUGFS_H

#ifdef WLAN_OPEN_SOURCE
VOS_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter);
void hdd_debugfs_exit(hdd_context_t *pHddCtx);
#else
inline VOS_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter)
{
    return VOS_STATUS_SUCCESS;
}
inline void hdd_debugfs_exit(hdd_context_t *pHddCtx)
{
}
#endif /* #ifdef WLAN_OPEN_SOURCE */
#endif /* #ifndef _WLAN_HDD_DEBUGFS_H */

