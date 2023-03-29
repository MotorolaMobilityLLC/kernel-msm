/*
 * Copyright (c) 2014-2017, 2019-2020 The Linux Foundation. All rights reserved.
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

#ifndef __CDS_REG_SERVICE_H
#define __CDS_REG_SERVICE_H

/**=========================================================================

   \file  cds_reg_service.h

   \brief Connectivity driver services (CDS): Non-Volatile storage API

   ========================================================================*/

#include "qdf_status.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>

#define CDS_COUNTRY_CODE_LEN  2
#define CDS_MAC_ADDRESS_LEN 6
#define HT40PLUS_2G_FCC_CH_END       7
#define HT40PLUS_2G_EURJAP_CH_END    9
#define HT40MINUS_2G_CH_START        5
#define HT40MINUS_2G_CH_END          13

/**
 * cds_get_vendor_reg_flags() - This API returns vendor specific regulatory
 * channel flags
 * @pdev: pdev pointer
 * @freq: channel frequency
 * @bandwidth: channel BW
 * @is_ht_enabled: HT enabled/disabled flag
 * @is_vht_enabled: VHT enabled/disabled flag
 * @sub_20_channel_width: Sub 20 channel bandwidth
 * Return: channel flags
 */
uint32_t cds_get_vendor_reg_flags(struct wlan_objmgr_pdev *pdev,
				  qdf_freq_t freq,
				  uint16_t bandwidth,
				  bool is_ht_enabled, bool is_vht_enabled,
				  uint8_t sub_20_channel_width);
#endif /* __CDS_REG_SERVICE_H */
