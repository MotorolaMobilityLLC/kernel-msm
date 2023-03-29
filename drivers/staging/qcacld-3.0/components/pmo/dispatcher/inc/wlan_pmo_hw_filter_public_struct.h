/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: This file shall contain all public parameter (struct/macro/enum)
 * definitions to support hardware filtering configuration. No APIs, or
 * implememtations of APIs, shall be contained within.
 */

#ifndef _WLAN_PMO_HW_FILTER_PUBLIC_STRUCT_H
#define _WLAN_PMO_HW_FILTER_PUBLIC_STRUCT_H

/**
 * pmo_hw_filter_mode - bitmap for enabled hardware filters
 * @HW_FILTER_DISABLED: hardware filter is completely disabled
 * @HW_FILTER_NON_ARP_BC: drop all broadcast frames, except ARP
 * @HW_FILTER_NON_ICMPV6_MC: drop all multicast frames, except ICMPv6
 *
 * The hardware filter is only effective in DTIM mode. Use this configuration
 * to blanket drop broadcast/multicast packets at the hardware level, without
 * waking up the firmware.
 */
enum pmo_hw_filter_mode {
	PMO_HW_FILTER_DISABLED		= 0,
	PMO_HW_FILTER_NON_ARP_BC	= (1 << 0),
	PMO_HW_FILTER_NON_ICMPV6_MC	= (1 << 1),
};

/**
 * struct pmo_hw_filter_params - hardware filter configuration parameters
 * @vdev_id: Id of the virtual device to configure
 * @enable: Enable/Disable the given hw filter modes
 * @mode_bitmap: the hardware filter mode bitmap to configure
 */
struct pmo_hw_filter_params {
	uint8_t vdev_id;
	bool enable;
	enum pmo_hw_filter_mode mode_bitmap;
};

#endif /* _WLAN_PMO_HW_FILTER_PUBLIC_STRUCT_H */
