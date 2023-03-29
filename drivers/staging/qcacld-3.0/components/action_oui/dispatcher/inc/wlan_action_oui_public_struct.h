/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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

/**
 * DOC: Declare structs and macros which can be accessed by various
 * components and modules.
 */

#ifndef _WLAN_ACTION_OUI_PUBLIC_STRUCT_H_
#define _WLAN_ACTION_OUI_PUBLIC_STRUCT_H_

#include <wlan_cmn.h>
#include <qdf_status.h>
#include <qdf_types.h>

/*
 * Maximum ini string length of actions oui extensions,
 * (n * 83) + (n - 1) spaces + 1 (terminating character),
 * where n is the no of oui extensions
 * currently, max no of oui extensions is 10
 */
#define ACTION_OUI_MAX_STR_LEN 840

/*
 * Maximum number of action oui extensions supported in
 * each action oui category
 */
#define ACTION_OUI_MAX_EXTENSIONS 10

#define ACTION_OUI_MAX_OUI_LENGTH 5
#define ACTION_OUI_MAX_DATA_LENGTH 20
#define ACTION_OUI_MAX_DATA_MASK_LENGTH 3
#define ACTION_OUI_MAC_MASK_LENGTH 1
#define ACTION_OUI_MAX_CAPABILITY_LENGTH 1

/*
 * NSS Mask and NSS Offset to extract NSS info from
 * capability field of action oui extension
 */
#define ACTION_OUI_CAPABILITY_NSS_MASK 0x0f
#define ACTION_OUI_CAPABILITY_NSS_OFFSET 0
#define ACTION_OUI_CAPABILITY_NSS_MASK_1X1 1
#define ACTION_OUI_CAPABILITY_NSS_MASK_2X2 2
#define ACTION_OUI_CAPABILITY_NSS_MASK_3X3 4
#define ACTION_OUI_CAPABILITY_NSS_MASK_4X4 8

/*
 * Mask and offset to extract HT and VHT info from
 * capability field of action oui extension
 */
#define ACTION_OUI_CAPABILITY_HT_ENABLE_MASK 0x10
#define ACTION_OUI_CAPABILITY_HT_ENABLE_OFFSET 4
#define ACTION_OUI_CAPABILITY_VHT_ENABLE_MASK 0x20
#define ACTION_OUI_CAPABILITY_VHT_ENABLE_OFFSET 5

/*
 * Mask and offset to extract Band (2G and 5G) info from
 * capability field of action oui extension
 */
#define ACTION_OUI_CAPABILITY_BAND_MASK 0xC0
#define ACTION_OUI_CAPABILITY_BAND_OFFSET 6
#define ACTION_OUI_CAPABILITY_2G_BAND_MASK 0x40
#define ACTION_OUI_CAPABILITY_2G_BAND_OFFSET 6
#define ACTION_CAPABILITY_5G_BAND_MASK 0x80
#define ACTION_CAPABILITY_5G_BAND_OFFSET 7

/**
 * enum action_oui_id - to identify type of action oui
 * @ACTION_OUI_CONNECT_1X1: for 1x1 connection only
 * @ACTION_OUI_ITO_EXTENSION: for extending inactivity time of station
 * @ACTION_OUI_CCKM_1X1: for TX with CCKM 1x1 only
 * @ACTION_OUI_ITO_ALTERNATE: alternate ITO extensions used by firmware
 * @ACTION_OUI_SWITCH_TO_11N_MODE: connect in 11n
 * @ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN: connect in 1x1 & disable diversity gain
 * @ACTION_OUI_DISABLE_AGGRESSIVE_TX: disable aggressive TX in firmware
 * @ACTION_OUI_FORCE_MAX_NSS: Force Max NSS connection with few IOT APs
 * @ACTION_OUI_DISABLE_AGGRESSIVE_EDCA: disable aggressive EDCA with the ap
 * @ACTION_OUI_DISABLE_TWT: disable TWT with the ap
 * @ACTION_OUI_HOST_ONLY: host only action id start - placeholder.
 * New Firmware related "ACTION" needs to be added before this placeholder.
 * @ACTION_OUI_HOST_RECONN: reconnect to the same BSSID when wait for
 * association response timeout from AP
 * @ACTION_OUI_MAXIMUM_ID: maximum number of action oui types
 */
enum action_oui_id {
	ACTION_OUI_CONNECT_1X1 = 0,
	ACTION_OUI_ITO_EXTENSION = 1,
	ACTION_OUI_CCKM_1X1 = 2,
	ACTION_OUI_ITO_ALTERNATE = 3,
	ACTION_OUI_SWITCH_TO_11N_MODE = 4,
	ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN = 5,
	ACTION_OUI_DISABLE_AGGRESSIVE_TX = 6,
	ACTION_OUI_FORCE_MAX_NSS = 7,
	ACTION_OUI_DISABLE_AGGRESSIVE_EDCA = 8,
	ACTION_OUI_DISABLE_TWT = 9,
	ACTION_OUI_HOST_ONLY,
	ACTION_OUI_HOST_RECONN = ACTION_OUI_HOST_ONLY,
	ACTION_OUI_MAXIMUM_ID
};

/**
 * enum action_oui_info - to indicate presence of various action OUI
 * fields in action oui extension, following identifiers are to be set in
 * the info mask field of action oui extension
 * @ACTION_OUI_INFO_OUI: to indicate presence of OUI string
 * @ACTION_OUI_INFO_MAC_ADDRESS: to indicate presence of mac address
 * @ACTION_OUI_INFO_AP_CAPABILITY_NSS: to indicate presence of nss info
 * @ACTION_OUI_INFO_AP_CAPABILITY_HT: to indicate presence of HT cap
 * @ACTION_OUI_INFO_AP_CAPABILITY_VHT: to indicate presence of VHT cap
 * @ACTION_OUI_INFO_AP_CAPABILITY_BAND: to indicate presence of band info
 */
enum action_oui_info {
	/*
	 * OUI centric parsing, expect OUI in each action OUI extension,
	 * hence, ACTION_OUI_INFO_OUI is dummy
	 */
	ACTION_OUI_INFO_OUI = 1 << 0,
	ACTION_OUI_INFO_MAC_ADDRESS = 1 << 1,
	ACTION_OUI_INFO_AP_CAPABILITY_NSS = 1 << 2,
	ACTION_OUI_INFO_AP_CAPABILITY_HT = 1 << 3,
	ACTION_OUI_INFO_AP_CAPABILITY_VHT = 1 << 4,
	ACTION_OUI_INFO_AP_CAPABILITY_BAND = 1 << 5,
};

/* Total mask of all enum action_oui_info IDs */
#define ACTION_OUI_INFO_MASK 0x3F

/**
 * struct action_oui_extension - action oui extension contents
 * @info_mask: info mask
 * @oui_length: length of the oui, either 3 or 5 bytes
 * @data_length: length of the oui data
 * @data_mask_length: length of the data mask
 * @mac_addr_length: length of the mac addr
 * @mac_mask_length: length of the mac mask
 * @capability_length: length of the capability
 * @oui: oui value
 * @data: data buffer
 * @data_mask: data mask buffer
 * @mac_addr: mac addr
 * @mac_mask: mac mask
 * @capability: capability buffer
 */
struct action_oui_extension {
	uint32_t info_mask;
	uint32_t oui_length;
	uint32_t data_length;
	uint32_t data_mask_length;
	uint32_t mac_addr_length;
	uint32_t mac_mask_length;
	uint32_t capability_length;
	uint8_t oui[ACTION_OUI_MAX_OUI_LENGTH];
	uint8_t data[ACTION_OUI_MAX_DATA_LENGTH];
	uint8_t data_mask[ACTION_OUI_MAX_DATA_MASK_LENGTH];
	uint8_t mac_addr[QDF_MAC_ADDR_SIZE];
	uint8_t mac_mask[ACTION_OUI_MAC_MASK_LENGTH];
	uint8_t capability[ACTION_OUI_MAX_CAPABILITY_LENGTH];
};

/**
 * struct action_oui_request - Contains specific action oui information
 * @action_id: type of action from enum action_oui_info
 * @no_oui_extensions: number of action oui extensions of type @action_id
 * @total_no_oui_extensions: total no of oui extensions from all
 * action oui types, this is just a total count needed by firmware
 * @extension: pointer to zero length array, to indicate this structure is
 * followed by a array of @no_oui_extensions structures of
 * type struct action_oui_extension
 */
struct action_oui_request {
	enum action_oui_id action_id;
	uint32_t no_oui_extensions;
	uint32_t total_no_oui_extensions;
	struct action_oui_extension extension[0];
};

/**
 * struct action_oui_search_attr - Used to check against action_oui ini input
 *
 * @ie_data: beacon ie data
 * @ie_length: length of ie data
 * @mac_addr: bssid of access point
 * @nss: AP spatial stream info
 * @ht_cap: Whether AP is HT capable
 * @vht_cap: Whether AP is VHT capable
 * @enable_2g: Whether 2.4GHz band is enabled in AP
 * @enable_5g: Whether 5GHz band is enabled in AP
 */
struct action_oui_search_attr {
	uint8_t *ie_data;
	uint32_t ie_length;
	uint8_t *mac_addr;
	uint32_t nss;
	bool ht_cap;
	bool vht_cap;
	bool enable_2g;
	bool enable_5g;
};

#endif /* _WLAN_ACTION_OUI_PUBLIC_STRUCT_H_ */
