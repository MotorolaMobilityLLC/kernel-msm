/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

/**===========================================================================

  \file  wlan_hdd_oemdata.h

  \brief Internal includes for the oem data

  ==========================================================================*/


#ifndef __WLAN_HDD_OEM_DATA_H__
#define __WLAN_HDD_OEM_DATA_H__

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 280
#endif

#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1724
#endif

#define OEM_APP_SIGNATURE_LEN      16
#define OEM_APP_SIGNATURE_STR      "QUALCOMM-OEM-APP"

#define OEM_TARGET_SIGNATURE_LEN   8
#define OEM_TARGET_SIGNATURE       "QUALCOMM"

#define OEM_CAP_MAX_NUM_CHANNELS   128

typedef enum
{
  /* Error null context */
  OEM_ERR_NULL_CONTEXT = 1,

  /* OEM App is not registered */
  OEM_ERR_APP_NOT_REGISTERED,

  /* Invalid signature */
  OEM_ERR_INVALID_SIGNATURE,

  /* Invalid message type */
  OEM_ERR_NULL_MESSAGE_HEADER,

  /* Invalid message type */
  OEM_ERR_INVALID_MESSAGE_TYPE,

  /* Invalid length in message body */
  OEM_ERR_INVALID_MESSAGE_LENGTH
} eOemErrorCode;

int oem_activate_service(void *pAdapter);

int iw_get_oem_data_cap(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra);

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U8 major;
  tANI_U8 minor;
  tANI_U8 patch;
  tANI_U8 build;
} tDriverVersion;

typedef PACKED_PRE struct PACKED_POST
{
    /* Signature of chip set vendor, e.g. QUALCOMM */
    tANI_U8 oem_target_signature[OEM_TARGET_SIGNATURE_LEN];
    tANI_U32 oem_target_type;         /* Chip type */
    tANI_U32 oem_fw_version;          /* FW version */
    tDriverVersion driver_version;    /* CLD version */
    tANI_U16 allowed_dwell_time_min;  /* Channel dwell time - allowed min */
    tANI_U16 allowed_dwell_time_max;  /* Channel dwell time - allowed max */
    tANI_U16 curr_dwell_time_min;     /* Channel dwell time - current min */
    tANI_U16 curr_dwell_time_max;     /* Channel dwell time - current max */
    tANI_U16 supported_bands;          /* 2.4G or 5G Hz */
    tANI_U16 num_channels;             /* Num of channels IDs to follow */
    tANI_U8 channel_list[OEM_CAP_MAX_NUM_CHANNELS];  /* List of channel IDs */
} t_iw_oem_data_cap;

typedef PACKED_PRE struct PACKED_POST
{
    /* channel id */
    tANI_U32 chan_id;

    /* reserved0 */
    tANI_U32 reserved0;

    /* Primary 20 MHz channel frequency in MHz */
    tANI_U32 mhz;

    /* Center frequency 1 in MHz */
    tANI_U32 band_center_freq1;

    /* Center frequency 2 in MHz - valid only for 11acvht 80plus80 mode */
    tANI_U32 band_center_freq2;

    /* channel info described below */
    tANI_U32 info;

    /* contains min power, max power, reg power and reg class id */
    tANI_U32 reg_info_1;

    /* contains antennamax */
    tANI_U32 reg_info_2;
} tHddChannelInfo;

typedef PACKED_PRE struct PACKED_POST
{
    /* peer mac address */
    tANI_U8 peer_mac_addr[6];

    /* peer status: 1: CONNECTED, 2: DISCONNECTED */
    tANI_U8 peer_status;

    /* vdev_id for the peer mac */
    tANI_U8 vdev_id;

    /* peer capability:
     * 0: RTT/RTT2
     * 1: RTT3(timing Meas Capability)
     * 2: RTT3(fine timing Meas Capability)
     * Default is 0
     */
    tANI_U32 peer_capability;

    /* reserved0 */
    tANI_U32 reserved0;

    /* channel info on which peer is connected */
    tHddChannelInfo peer_chan_info;
} tPeerStatusInfo;

#endif //__WLAN_HDD_OEM_DATA_H__

#endif //FEATURE_OEM_DATA_SUPPORT
