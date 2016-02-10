/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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

/*===========================================================================
  \file wlan_nlink_common.h

  Exports and types for the Netlink Service interface. This header file contains
  message types and definitions that is shared between the user space service
  (e.g. BTC service) and WLAN kernel module.

===========================================================================*/

#ifndef WLAN_NLINK_COMMON_H__
#define WLAN_NLINK_COMMON_H__

#include <linux/netlink.h>
#ifdef QCA_FEATURE_RPS
#include <linux/if.h>
#endif
/*---------------------------------------------------------------------------
 * External Functions
 *-------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *-------------------------------------------------------------------------*/
#define WLAN_NL_MAX_PAYLOAD   256     /* maximum size for netlink message*/
#define WLAN_NLINK_PROTO_FAMILY  NETLINK_USERSOCK
#define WLAN_NLINK_MCAST_GRP_ID  0x01

/*---------------------------------------------------------------------------
 * Type Declarations
 *-------------------------------------------------------------------------*/

/*
 * The following enum defines the target service within WLAN driver for which the
 * message is intended for. Each service along with its counterpart
 * in the user space, define a set of messages they recognize.
 * Each of this message will have an header of type tAniMsgHdr defined below.
 * Each Netlink message to/from a kernel module will contain only one
 * message which is preceded by a tAniMsgHdr. The maximun size (in bytes) of
 * a netlink message is assumed to be MAX_PAYLOAD bytes.
 *
 *         +------------+-------+----------+----------+
 *         |Netlink hdr | Align |tAniMsgHdr| msg body |
 *         +------------+-------+----------|----------+
 */

// Message Types
#define WLAN_BTC_QUERY_STATE_REQ    0x01  // BTC  --> WLAN
#define WLAN_BTC_BT_EVENT_IND       0x02  // BTC  --> WLAN
#define WLAN_BTC_QUERY_STATE_RSP    0x03  // WLAN -->  BTC
#define WLAN_MODULE_UP_IND          0x04  // WLAN -->  BTC
#define WLAN_MODULE_DOWN_IND        0x05  // WLAN -->  BTC
#define WLAN_STA_ASSOC_DONE_IND     0x06  // WLAN -->  BTC
#define WLAN_STA_DISASSOC_DONE_IND  0x07  // WLAN -->  BTC

// Special Message Type used by AMP, intercepted by send_btc_nlink_msg() and
// replaced by WLAN_STA_ASSOC_DONE_IND or WLAN_STA_DISASSOC_DONE_IND
#define WLAN_AMP_ASSOC_DONE_IND     0x10

// Special Message Type used by SoftAP, intercepted by send_btc_nlink_msg() and
// replaced by WLAN_STA_ASSOC_DONE_IND
#define WLAN_BTC_SOFTAP_BSS_START   0x11
#define WLAN_SVC_FW_CRASHED_IND     0x100
#define WLAN_SVC_LTE_COEX_IND       0x101
#define WLAN_SVC_WLAN_AUTO_SHUTDOWN_IND 0x102
#define WLAN_SVC_DFS_CAC_START_IND      0x103
#define WLAN_SVC_DFS_CAC_END_IND        0x104
#define WLAN_SVC_DFS_RADAR_DETECT_IND   0x105
#define WLAN_SVC_WLAN_STATUS_IND    0x106
#define WLAN_SVC_WLAN_VERSION_IND   0x107
#define WLAN_SVC_DFS_ALL_CHANNEL_UNAVAIL_IND 0x108
#define WLAN_SVC_WLAN_TP_IND        0x109
#define WLAN_SVC_RPS_ENABLE_IND     0x10A
#define WLAN_SVC_WLAN_TP_TX_IND     0x10B

#define WLAN_SVC_MAX_SSID_LEN    32
#define WLAN_SVC_MAX_BSSID_LEN   6
#define WLAN_SVC_MAX_STR_LEN     16
#define WLAN_SVC_MAX_NUM_CHAN    128
#define WLAN_SVC_COUNTRY_CODE_LEN 3

/*
 * Maximim number of queues supported by WLAN driver. Setting an upper
 * limit. Actual number of queues may be smaller than this value.
 */
#define WLAN_SVC_IFACE_NUM_QUEUES 6

// Event data for WLAN_BTC_QUERY_STATE_RSP & WLAN_STA_ASSOC_DONE_IND
typedef struct
{
   unsigned char channel;  // 0 implies STA not associated to AP
} tWlanAssocData;

#define ANI_NL_MSG_BASE     0x10    /* Some arbitrary base */

typedef enum eAniNlModuleTypes {
   ANI_NL_MSG_PUMAC = ANI_NL_MSG_BASE + 0x01,// PTT Socket App
   ANI_NL_MSG_PTT   = ANI_NL_MSG_BASE + 0x07,// Quarky GUI
   WLAN_NL_MSG_BTC,
   WLAN_NL_MSG_OEM,
   WLAN_NL_MSG_SVC,
   WLAN_NL_MSG_CNSS_DIAG = ANI_NL_MSG_BASE + 0x0B,//Value needs to be 27
   ANI_NL_MSG_LOG,
   ANI_NL_MSG_MAX
} tAniNlModTypes, tWlanNlModTypes;

#define WLAN_NL_MSG_BASE ANI_NL_MSG_BASE
#define WLAN_NL_MSG_MAX  ANI_NL_MSG_MAX

//All Netlink messages must contain this header
typedef struct sAniHdr {
   unsigned short type;
   unsigned short length;
} tAniHdr, tAniMsgHdr;

struct wlan_status_data {
   uint8_t lpss_support;
   uint8_t is_on;
   uint8_t vdev_id;
   uint8_t is_connected;
   int8_t rssi;
   uint8_t ssid_len;
   uint8_t country_code[WLAN_SVC_COUNTRY_CODE_LEN];
   uint32_t vdev_mode;
   uint32_t freq;
   uint32_t numChannels;
   uint8_t channel_list[WLAN_SVC_MAX_NUM_CHAN];
   uint8_t ssid[WLAN_SVC_MAX_SSID_LEN];
   uint8_t bssid[WLAN_SVC_MAX_BSSID_LEN];
};

struct wlan_version_data {
   uint32_t chip_id;
   char chip_name[WLAN_SVC_MAX_STR_LEN];
   char chip_from[WLAN_SVC_MAX_STR_LEN];
   char host_version[WLAN_SVC_MAX_STR_LEN];
   char fw_version[WLAN_SVC_MAX_STR_LEN];
};

#ifdef QCA_FEATURE_RPS
/**
 * struct wlan_rps_data - structure to send RPS info to cnss-daemon
 * @ifname:         interface name for which the RPS data belongs to
 * @num_queues:     number of rx queues for which RPS data is being sent
 * @cpu_map_list:   array of cpu maps for different rx queues supported by
 *                  the wlan driver
 *
 * The structure specifies the format of data exchanged between wlan
 * driver and cnss-daemon. On receipt of the data, cnss-daemon is expected
 * to apply the 'cpu_map' for each rx queue belonging to the interface 'ifname'
 */
struct wlan_rps_data {
	char ifname[IFNAMSIZ];
	uint16_t num_queues;
	uint16_t cpu_map_list[WLAN_SVC_IFACE_NUM_QUEUES];
};
#endif

struct wlan_dfs_info {
   uint16_t channel;
   uint8_t country_code[WLAN_SVC_COUNTRY_CODE_LEN];
};

/**
 * enum wlan_tp_level - indicates wlan throughput level
 *
 * The different throughput levels are determined on the basis of # of tx and
 * rx packets and other threshold values. For example, if the # of total packets
 * sent or received by the driver is greater than 500 in the last 100ms, the
 * driver has a high throughput requirement. The driver may tweak certain system
 * parameters based on the throughput level.
 *
 * @WLAN_SVC_TP_NONE - used for initialization
 * @WLAN_SVC_TP_LOW - used to identify low throughput level
 * @WLAN_SVC_TP_MEDIUM - used to identify medium throughput level
 * @WLAN_SVC_TP_HIGH - used to identify high throughput level
 */
enum wlan_tp_level {
        WLAN_SVC_TP_NONE,
        WLAN_SVC_TP_LOW,
        WLAN_SVC_TP_MEDIUM,
        WLAN_SVC_TP_HIGH,
};

#endif //WLAN_NLINK_COMMON_H__
