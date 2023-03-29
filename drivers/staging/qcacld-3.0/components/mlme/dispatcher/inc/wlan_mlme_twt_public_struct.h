/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains definitions for MLME TWT functionality.
 */

#ifndef _WLAN_MLME_TWT_STRUCT_H_
#define _WLAN_MLME_TWT_STRUCT_H_

#define TWT_ALL_SESSIONS_DIALOG_ID 255

/**
 * enum wlan_twt_commands  - TWT commands
 * @WLAN_TWT_NONE: Indicates none of the TWT commands are active.
 * @WLAN_TWT_SETUP: TWT setup
 * @WLAN_TWT_TERMINATE: TWT terminate
 * @WLAN_TWT_SUSPEND: TWT suspend
 * @WLAN_TWT_RESUME: TWT resume
 * @WLAN_TWT_NUDGE: TWT nudge
 * @WLAN_TWT_STATISTICS: TWT statistics
 * @WLAN_TWT_CLEAR_STATISTICS: TWT clear statistics
 * @WLAN_TWT_ANY: Indicates one of the commands is in progress.
 */
enum wlan_twt_commands {
	WLAN_TWT_NONE             = 0,
	WLAN_TWT_SETUP            = BIT(0),
	WLAN_TWT_TERMINATE        = BIT(1),
	WLAN_TWT_SUSPEND          = BIT(2),
	WLAN_TWT_RESUME           = BIT(3),
	WLAN_TWT_NUDGE            = BIT(4),
	WLAN_TWT_STATISTICS       = BIT(5),
	WLAN_TWT_CLEAR_STATISTICS = BIT(6),
	WLAN_TWT_ANY              = 0xFF,
};

/**
 * enum wlan_twt_capabilities  - Represents the Bitmap of TWT capabilities
 * supported by device and peer.
 * @WLAN_TWT_CAPA_REQUESTOR: TWT requestor support is advertised by TWT
 * non-scheduling STA.
 * @WLAN_TWT_CAPA_RESPONDER: TWT responder support is advertised by TWT
 * AP.
 * @WLAN_TWT_CAPA_BROADCAST: This indicates support for the role of broadcast
 * TWT scheduling/receiving functionality.
 * @WLAN_TWT_CAPA_FLEXIBLE: Device supports flexible TWT schedule.
 * @WLAN_TWT_CAPA_REQUIRED: The TWT Required is advertised by AP to indicate
 * that it mandates the associated HE STAs to support TWT.
 */
enum wlan_twt_capabilities {
	WLAN_TWT_CAPA_REQUESTOR = BIT(0),
	WLAN_TWT_CAPA_RESPONDER = BIT(1),
	WLAN_TWT_CAPA_BROADCAST = BIT(2),
	WLAN_TWT_CAPA_FLEXIBLE =  BIT(3),
	WLAN_TWT_CAPA_REQUIRED =  BIT(4),
};

/**
 * enum wlan_twt_session_state  - TWT session state for a dialog id
 * @WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED: Session doesn't exist
 * @WLAN_TWT_SETUP_STATE_ACTIVE: TWT session is active
 * @WLAN_TWT_SETUP_STATE_SUSPEND: TWT session is suspended
 */
enum wlan_twt_session_state {
	WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED = 0,
	WLAN_TWT_SETUP_STATE_ACTIVE = 1,
	WLAN_TWT_SETUP_STATE_SUSPEND = 2,
};

#define WLAN_ALL_SESSIONS_DIALOG_ID 255

#ifdef WLAN_SUPPORT_TWT
#define WLAN_MAX_TWT_SESSIONS_PER_PEER 1

/**
 * struct twt_session_info  - TWT session related parameters
 * @dialog_id: TWT session dialog id
 * @state: TWT session state
 * @setup_done: TWT session setup is complete
 * @active_cmd: bitmap to indicate which command is
 * in progress. Bits are provided by enum wlan_twt_commands.
 */
struct twt_session_info {
	uint8_t dialog_id;
	uint8_t state;
	bool setup_done;
	enum wlan_twt_commands active_cmd;
};

/**
 * struct twt_context  - TWT context
 * @peer_capability: TWT peer capability bitmap. Refer enum
 * wlan_twt_capabilities for representation.
 * @num_twt_sessions: Maximum supported TWT sessions.
 * @session_info: TWT session related parameters for each session
 */
struct twt_context {
	uint8_t peer_capability;
	uint8_t num_twt_sessions;
	struct twt_session_info session_info[WLAN_MAX_TWT_SESSIONS_PER_PEER];
};
#endif /* WLAN_SUPPORT_TWT */
#endif /* _WLAN_MLME_TWT_STRUCT_H_ */
