/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare various struct, macros which shall be used in
 * pmo wow related features.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_WOW_PUBLIC_STRUCT_H_
#include "wlan_pmo_lphb_public_struct.h"

#define _WLAN_PMO_WOW_PUBLIC_STRUCT_H_

#ifndef PMO_WOW_FILTERS_MAX
#define PMO_WOW_FILTERS_MAX             22
#endif

#define PMO_WOWL_PTRN_MAX_SIZE          146
#define PMO_WOWL_PTRN_MASK_MAX_SIZE      19
#define PMO_WOWL_BCAST_PATTERN_MAX_SIZE 146

#define PMO_WOW_INTER_PTRN_TOKENIZER   ';'
#define PMO_WOW_INTRA_PTRN_TOKENIZER   ':'

#define PMO_WOW_PTRN_MASK_VALID     0xFF
#define PMO_NUM_BITS_IN_BYTE           8


/* Action frame categories */

#define PMO_MAC_ACTION_SPECTRUM_MGMT   0
#define PMO_MAC_ACTION_QOS_MGMT        1
#define PMO_MAC_ACTION_DLP             2
#define PMO_MAC_ACTION_BLKACK          3
#define PMO_MAC_ACTION_PUBLIC_USAGE    4
#define PMO_MAC_ACTION_RRM             5
#define PMO_MAC_ACTION_FAST_BSS_TRNST  6
#define PMO_MAC_ACTION_HT              7
#define PMO_MAC_ACTION_SA_QUERY        8
#define PMO_MAC_ACTION_PROT_DUAL_PUB   9
#define PMO_MAC_ACTION_WNM            10
#define PMO_MAC_ACTION_UNPROT_WNM     11
#define PMO_MAC_ACTION_TDLS           12
#define PMO_MAC_ACITON_MESH           13
#define PMO_MAC_ACTION_MHF            14
#define PMO_MAC_SELF_PROTECTED        15
#define PMO_MAC_ACTION_WME            17
#define PMO_MAC_ACTION_FST            18
#define PMO_MAC_ACTION_RVS            19
#define PMO_MAC_ACTION_VHT            21
#define PMO_MAC_ACTION_MAX            256

/*
 * ALLOWED_ACTION_FRAMES_BITMAP
 *
 * Bitmask is based on the below. The frames with 0's
 * set to their corresponding bit can be dropped in FW.
 *
 * -----------------------------+-----+-------+
 *         Type                 | Bit | Allow |
 * -----------------------------+-----+-------+
 * PMO_ACTION_SPECTRUM_MGMT    0      1
 * PMO_ACTION_QOS_MGMT         1      1
 * PMO_ACTION_DLP              2      0
 * PMO_ACTION_BLKACK           3      0
 * PMO_ACTION_PUBLIC_USAGE     4      1
 * PMO_ACTION_RRM              5      0
 * PMO_ACTION_FAST_BSS_TRNST   6      0
 * PMO_ACTION_HT               7      0
 * PMO_ACTION_SA_QUERY         8      1
 * PMO_ACTION_PROT_DUAL_PUB    9      1
 * PMO_ACTION_WNM             10      1
 * PMO_ACTION_UNPROT_WNM      11      0
 * PMO_ACTION_TDLS            12      0
 * PMO_ACITON_MESH            13      0
 * PMO_ACTION_MHF             14      0
 * PMO_SELF_PROTECTED         15      0
 * PMO_ACTION_WME             17      1
 * PMO_ACTION_FST             18      1
 * PMO_ACTION_RVS             19      1
 * PMO_ACTION_VHT             21      1
 * ----------------------------+------+-------+
 */
#define SYSTEM_SUSPEND_ALLOWED_ACTION_FRAMES_BITMAP0 \
			((1 << PMO_MAC_ACTION_SPECTRUM_MGMT) | \
			 (1 << PMO_MAC_ACTION_QOS_MGMT) | \
			 (1 << PMO_MAC_ACTION_PUBLIC_USAGE) | \
			 (1 << PMO_MAC_ACTION_SA_QUERY) | \
			 (1 << PMO_MAC_ACTION_PROT_DUAL_PUB) | \
			 (1 << PMO_MAC_ACTION_WNM) | \
			 (1 << PMO_MAC_ACTION_WME) | \
			 (1 << PMO_MAC_ACTION_FST) | \
			 (1 << PMO_MAC_ACTION_RVS) | \
			 (1 << PMO_MAC_ACTION_VHT))

#define ALLOWED_ACTION_FRAMES_BITMAP1   0x0
#define ALLOWED_ACTION_FRAMES_BITMAP2   0x0
#define ALLOWED_ACTION_FRAMES_BITMAP3   0x0
#define ALLOWED_ACTION_FRAMES_BITMAP4   0x0
#define ALLOWED_ACTION_FRAMES_BITMAP5   0x0
#define ALLOWED_ACTION_FRAMES_BITMAP6   0x0
#define ALLOWED_ACTION_FRAMES_BITMAP7   0x0

#define ALLOWED_ACTION_FRAME_MAP_WORDS (PMO_MAC_ACTION_MAX / 32)

#define RUNTIME_PM_ALLOWED_ACTION_FRAMES_BITMAP0 \
		((1 << PMO_MAC_ACTION_SPECTRUM_MGMT) | \
		 (1 << PMO_MAC_ACTION_QOS_MGMT) | \
		 (1 << PMO_MAC_ACTION_PUBLIC_USAGE) | \
		 (1 << PMO_MAC_ACTION_RRM) | \
		 (1 << PMO_MAC_ACTION_SA_QUERY) | \
		 (1 << PMO_MAC_ACTION_PROT_DUAL_PUB) | \
		 (1 << PMO_MAC_ACTION_WNM) | \
		 (1 << PMO_MAC_ACTION_WME) | \
		 (1 << PMO_MAC_ACTION_FST) | \
		 (1 << PMO_MAC_ACTION_RVS) | \
		 (1 << PMO_MAC_ACTION_VHT))

/* Public Action for 20/40 BSS Coexistence */
#define PMO_MAC_ACTION_MEASUREMENT_PILOT    7

#define DROP_PUBLIC_ACTION_FRAME_BITMAP \
		(1 << PMO_MAC_ACTION_MEASUREMENT_PILOT)

#ifndef ANI_SUPPORT_11H
/*
 * DROP_SPEC_MGMT_ACTION_FRAME_BITMAP
 *
 * Bitmask is based on the below. The frames with 1's
 * set to their corresponding bit can be dropped in FW.
 *
 * ----------------------------------+-----+------+
 *         Type                      | Bit | Drop |
 * ----------------------------------+-----+------+
 * ACTION_SPCT_MSR_REQ                  0     1
 * ACTION_SPCT_TPC_REQ                  2     1
 * ----------------------------------+-----+------+
 */
#define DROP_SPEC_MGMT_ACTION_FRAME_BITMAP \
		((1 << ACTION_SPCT_MSR_REQ) |\
		 (1 << ACTION_SPCT_TPC_REQ))
#else
/*
 * If 11H support is defined, dont drop the above action category of
 * spectrum mgmt action frames as host driver is processing them.
 */
#define DROP_SPEC_MGMT_ACTION_FRAME_BITMAP 0
#endif /* ANI_SUPPORT_11H */

#define PMO_SUPPORTED_ACTION_CATE           256
#define PMO_SUPPORTED_ACTION_CATE_ELE_LIST (PMO_SUPPORTED_ACTION_CATE/32)

/**
 * struct pmo_action_wakeup_set_params - action wakeup set params
 * @vdev_id: virtual device id
 * @operation: 0 reset to fw default, 1 set the bits,
 *    2 add the setting bits, 3 delete the setting bits
 * @action_category_map: bit mapping.
 * @action_per_category: bitmap per action category
 */
struct pmo_action_wakeup_set_params {
	uint32_t vdev_id;
	uint32_t operation;
	uint32_t action_category_map[PMO_SUPPORTED_ACTION_CATE_ELE_LIST];
	uint32_t action_per_category[PMO_SUPPORTED_ACTION_CATE];
};

/**
 * enum pmo_wow_action_wakeup_opertion: describe action wakeup operation
 * @pmo_action_wakeup_reset: reset
 * @pmo_action_wakeup_set: set
 * @pmo_action_wakeup_add_set: add and set
 * @pmo_action_wakeup_del_set: delete and set
 */
enum pmo_wow_action_wakeup_opertion {
	pmo_action_wakeup_reset = 0,
	pmo_action_wakeup_set,
	pmo_action_wakeup_add_set,
	pmo_action_wakeup_del_set,
};

/**
 * enum pmo_wow_state: enumeration of wow state
 * @pmo_wow_state_none: not in wow state
 * @pmo_wow_state_legacy_d0: in d0 wow state trigger by legacy d0 wow command
 * @pmo_wow_state_unified_d0: in d0 wow state trigger by unified wow command
 * @pmo_wow_state_unified_d3: in d3 wow state trigger by unified wow command
 */
enum pmo_wow_state {
	pmo_wow_state_none = 0,
	pmo_wow_state_legacy_d0,
	pmo_wow_state_unified_d0,
	pmo_wow_state_unified_d3,
};

/**
 * struct pmo_wow - store wow patterns
 * @wow_enable: wow enable/disable
 * @wow_enable_cmd_sent: is wow enable command sent to fw
 * @is_wow_bus_suspended: true if bus is suspended
 * @wow_state: state of wow
 * @target_suspend: target suspend event
 * @target_resume: target resume event
 * @wow_nack: wow negative ack flag
 * @wow_initial_wake_up: target initial wake up is received
 * @wow_wake_lock: wow wake lock
 * @lphb_cache: lphb cache
 * @lphb_cb_ctx: callback context for lphb, kept as void* as
 *                        osif structures are opaque to pmo.
 * @pmo_lphb_callback: registered os if calllback function
 * @ptrn_id_def: default pattern id counter for legacy firmware
 * @ptrn_id_usr: user pattern id counter for legacy firmware
 * @txrx_suspended: flag to determine if TX/RX is suspended
 *		    during WoW
 *
 * This structure stores wow patterns and
 * wow related parameters in host.
 */
struct pmo_wow {
	bool wow_enable;
	bool wow_enable_cmd_sent;
	bool is_wow_bus_suspended;
	enum pmo_wow_state wow_state;
	qdf_event_t target_suspend;
	qdf_event_t target_resume;
	int wow_nack;
	atomic_t wow_initial_wake_up;
	qdf_wake_lock_t wow_wake_lock;
	/*
	 * currently supports only vdev 0.
	 * cache has two entries: one for TCP and one for UDP.
	 */
	struct pmo_lphb_req lphb_cache[2];
	void *lphb_cb_ctx;
	pmo_lphb_callback lphb_cb;

	uint8_t ptrn_id_def;
	uint8_t ptrn_id_usr;
	bool txrx_suspended;
};

/* WOW related structures */
/**
 * struct pmo_wow_add_pattern - wow pattern add structure
 * @pattern_id: pattern id
 * @pattern_byte_offset: pattern byte offset from beginning of the 802.11
 *			 packet to start of the wake-up pattern
 * @pattern_size: pattern size
 * @pattern: pattern byte stream
 * @pattern_mask_size: pattern mask size
 * @pattern_mask: pattern mask
 */
struct pmo_wow_add_pattern {
	uint8_t pattern_id;
	uint8_t pattern_byte_offset;
	uint8_t pattern_size;
	uint8_t pattern[PMO_WOWL_BCAST_PATTERN_MAX_SIZE];
	uint8_t pattern_mask_size;
	uint8_t pattern_mask[PMO_WOWL_BCAST_PATTERN_MAX_SIZE];
};

/**
 * struct pmo_wow_cmd_params - wow cmd parameter
 * @enable: wow enable or disable flag
 * @can_suspend_link: flag to indicate if link can be suspended
 * @pause_iface_config: interface config
 */
struct pmo_wow_cmd_params {
	bool enable;
	bool can_suspend_link;
	uint8_t pause_iface_config;
	uint32_t flags;
};

/**
 * struct pmo_suspend_params - suspend cmd parameter
 * @disable_target_intr: disable target interrupt
 */
struct pmo_suspend_params {
	uint8_t disable_target_intr;
};

#endif /* end  of _WLAN_PMO_WOW_PUBLIC_STRUCT_H_ */
