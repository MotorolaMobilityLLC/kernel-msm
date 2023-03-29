/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

#ifndef _CFG_PKT_CAPTURE_H_
#define _CFG_PKT_CAPTURE_H_

#ifdef WLAN_FEATURE_PKT_CAPTURE

#define CFG_PKT_CAPTURE_MODE_DEFAULT	(0)
#define CFG_PKT_CAPTURE_MODE_MGMT_PKT	BIT(0)
#define CFG_PKT_CAPTURE_MODE_DATA_PKT	BIT(1)
#define CFG_PKT_CAPTURE_MODE_MAX	(CFG_PKT_CAPTURE_MODE_MGMT_PKT | \
					 CFG_PKT_CAPTURE_MODE_DATA_PKT)

/*
 * <ini>
 * packet_capture_mode - Packet capture mode
 * @Min: 0
 * @Max: 3
 * Default: 0 - Capture no packets
 *
 * This ini is used to decide packet capture mode
 *
 * packet_capture_mode = 0 - Capture no packets
 * packet_capture_mode = 1 - Capture management packets only
 * packet_capture_mode = 2 - Capture data packets only
 * packet_capture_mode = 3 - Capture both data and management packets
 *
 * Supported Feature: packet capture
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_PKT_CAPTURE_MODE \
			CFG_INI_UINT("packet_capture_mode", \
			0, \
			CFG_PKT_CAPTURE_MODE_MAX, \
			CFG_PKT_CAPTURE_MODE_DEFAULT, \
			CFG_VALUE_OR_DEFAULT, \
			"Value for packet capture mode")

#define CFG_PKT_CAPTURE_MODE_ALL \
	CFG(CFG_PKT_CAPTURE_MODE)
#else
#define CFG_PKT_CAPTURE_MODE_ALL
#endif /* WLAN_FEATURE_PKT_CAPTURE */
#endif /* _CFG_PKT_CAPTURE_H_ */
