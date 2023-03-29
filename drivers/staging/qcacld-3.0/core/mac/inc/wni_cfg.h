/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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

#ifndef __WNICFG_H
#define __WNICFG_H

/*
 * String parameter lengths
 */

#define WNI_CFG_VALID_CHANNEL_LIST_LEN    100
#define WNI_CFG_COUNTRY_CODE_LEN    3
#define WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN    255
#define WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN    255
#define WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN    255
#define WNI_CFG_WPS_UUID_LEN    16
#define WNI_CFG_HE_PPET_LEN     25

/*
 * Integer parameter min/max/default values
 */
#define WNI_CFG_PHY_MODE_11A    0
#define WNI_CFG_PHY_MODE_11B    1
#define WNI_CFG_PHY_MODE_11G    2
#define WNI_CFG_PHY_MODE_NONE    3

#define WNI_CFG_CURRENT_CHANNEL_STAMIN    0
#define WNI_CFG_CURRENT_CHANNEL_STAMAX    173

#define WNI_CFG_EDCA_PROFILE_ANI    0
#define WNI_CFG_EDCA_PROFILE_WMM    1
#define WNI_CFG_EDCA_PROFILE_ETSI_EUROPE   3
#define WNI_CFG_EDCA_PROFILE_MAX    4

#define WNI_CFG_ADMIT_POLICY_ADMIT_ALL    0
#define WNI_CFG_ADMIT_POLICY_REJECT_ALL    1
#define WNI_CFG_ADMIT_POLICY_BW_FACTOR    2

#define WNI_CFG_CHANNEL_BONDING_MODE_DISABLE    0
#define WNI_CFG_CHANNEL_BONDING_MODE_ENABLE    1

#define WNI_CFG_BLOCK_ACK_ENABLED_DELAYED    0
#define WNI_CFG_BLOCK_ACK_ENABLED_IMMEDIATE    1

#define WNI_CFG_HT_CAP_INFO_ADVANCE_CODING    0
#define WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ    5
#define WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ    6
#define WNI_CFG_HT_CAP_INFO_TX_STBC    7
#define WNI_CFG_HT_CAP_INFO_RX_STBC    8

#define WNI_CFG_GREENFIELD_CAPABILITY_DISABLE    0

/*
 * WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF + 1 is
 * assumed to be the default fw supported BF antennas, if fw
 * says it supports 8 antennas in rx ready event and if
 * gTxBFCsnValue INI value is configured above 3, set
 * the same to WNI_CFG_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED.
 * Otherwise, fall back and set fw default value[3].
 */

#define WNI_CFG_WPS_ENABLE_AP    1

#define WNI_CFG_ASSOC_STA_LIMIT_STAMIN    1
#define WNI_CFG_ASSOC_STA_LIMIT_STAMAX    32

#define WNI_CFG_REMOVE_TIME_SYNC_CMD_STAMIN 0
#define WNI_CFG_REMOVE_TIME_SYNC_CMD_STAMAX 1
#define WNI_CFG_REMOVE_TIME_SYNC_CMD_STADEF 0

#define CFG_STA_MAGIC_DWORD    0xbeefbeef

#endif
