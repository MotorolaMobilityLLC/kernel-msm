/*
 * Copyright (c) 2015-2019 The Linux Foundation. All rights reserved.
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

#if !defined(__SME_POWER_SAVE_H)
#define __SME_POWER_SAVE_H
#include "qdf_lock.h"
#include "qdf_trace.h"
#include "qdf_mem.h"
#include "qdf_types.h"
#include "ani_system_defs.h"
#include "sir_api.h"

/*
 * Auto Ps Entry User default timeout value, used instead of negative timeouts
 * from user space - 5000ms
 */
#define AUTO_PS_ENTRY_USER_TIMER_DEFAULT_VALUE 5000
#define AUTO_PS_ENTRY_TIMER_DEFAULT_VALUE 1000
#define AUTO_PS_DEFER_TIMEOUT_MS 1500

/**
 * enum ps_state - State of the power save
 * @FULL_POWER_MODE: for Full power mode
 * @LEGACY_POWER_SAVE_MODE: For Legacy Power Save mode
 * @UAPSD_MODE: for UAPSD power save
 */

enum ps_state {
	FULL_POWER_MODE,
	LEGACY_POWER_SAVE_MODE,
	UAPSD_MODE
};

/**
 * struct ps_params - maintain power save state and USAPD params
 * @mac_ctx: mac_ctx
 * @session_id: Session Id.
 * @ps_state : State of the power save
 * @uapsd_per_ac_trigger_enable_mask: dynamic UPASD mask setting
 *		derived from AddTS Rsp and DelTS frame.
 *		If a particular AC bit is set, it means AC is trigger  enabled.
 * @uapsd_per_ac_delivery_enable_mask: dynamic UPASD mask setting
 *		derived from AddTS Rsp and DelTs frame.
 *		If a particular AC bit is set, it means AC is delivery enabled.
 * @ac_admit_mask: used for AC downgrade. This is a dynamic mask
 *		setting which keep tracks of ACs being admitted.
 *		If bit is set to 0: That particular AC is not admitted
 *		If bit is set to 1: That particular AC is admitted
 * @uapsd_per_ac_bit_mask: This is a static UAPSD mask setting
 *		derived from SME_JOIN_REQ and SME_REASSOC_REQ.
 *		If a particular AC bit is set, it means the AC is both
 *		trigger enabled and delivery enabled.
 * @auto_ps_enable_timer: Upon expiration of this timer	Power Save Offload
 *		module will try to enable sta mode ps
 */

struct ps_params {
	void *mac_ctx;
	uint32_t     session_id;
	enum    ps_state ps_state;
	uint8_t uapsd_per_ac_trigger_enable_mask;
	uint8_t uapsd_per_ac_delivery_enable_mask;
	uint8_t ac_admit_mask[SIR_MAC_DIRECTION_DIRECT];
	uint8_t uapsd_per_ac_bit_mask;
	qdf_mc_timer_t auto_ps_enable_timer;

};

/**
 * struct ps_global_info - global struct for Power save information
 * @ps_params: maintain power save state and USAPD params
 * @remain_in_power_active_till_dhcp: remain in Power active till DHCP completes
 */
struct ps_global_info {
	struct ps_params ps_params[WLAN_MAX_VDEVS];
	bool remain_in_power_active_till_dhcp;
};

/**
 * enum sme_ps_cmd: power save message to send WMA
 * @SME_PS_ENABLE: For power save enable.
 * @SME_PS_DISABLE: for Power save disable.
 * @SME_PS_UAPSD_ENABLE; for UAPSD enable.
 * @SME_PS_UAPSD_DISABLE: for UAPSD disable.
 * @SME_PS_WOWL_ENTER: for WOWL Enter.
 * @SME_PS_WOWL_EXIT: for WOWL Exit.
 * @SME_PS_WOWL_ADD_BCAST_PTRN: Add bcst WOWL pattern.
 * @SME_PS_WOWL_DEL_BCAST_PTRN: Del Bcsr Wowl Pattern.
 */
enum sme_ps_cmd {
	SME_PS_ENABLE = 0,
	SME_PS_DISABLE,
	SME_PS_UAPSD_ENABLE,
	SME_PS_UAPSD_DISABLE,
	SME_PS_WOWL_ENTER,
	SME_PS_WOWL_EXIT,
	SME_PS_WOWL_ADD_BCAST_PTRN,
	SME_PS_WOWL_DEL_BCAST_PTRN,
};

#endif
