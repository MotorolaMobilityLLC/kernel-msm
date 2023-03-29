/*
 * Copyright (c) 2012-2014,2016-2017,2019 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_HDD_MISC_H
#define WLAN_HDD_MISC_H
/*
 * To prevent name conflicts when loading different instances of the driver:
 *
 * If DYNAMIC_SINGLE_CHIP is defined, which means there are multiple possible
 * drivers, but only one instance of driver at a time(WLAN dynamic detect),
 * prepend DYNAMIC_SINGLE_CHIP to the filenames.
 *
 * Otherwise, if MULTI_IF_NAME is defined, which means there are multiple
 * instances of the driver with different module names, prepend MULTI_IF_NAME
 * to the filenames.
 */
#ifdef DYNAMIC_SINGLE_CHIP
#define PREFIX DYNAMIC_SINGLE_CHIP "/"
#else

#ifdef MULTI_IF_NAME
#define PREFIX MULTI_IF_NAME "/"
#else
#define PREFIX ""
#endif

#endif

#ifdef MSM_PLATFORM
#define WLAN_INI_FILE              "wlan/qca_cld/" PREFIX "WCNSS_qcom_cfg.ini"
#define WLAN_MAC_FILE              "wlan/qca_cld/" PREFIX "wlan_mac.bin"
// BEGIN IKSWR-45692, support loading moto specific configurations
#define WLAN_MOT_INI_FILE              "wlan/qca_cld/" PREFIX "WCNSS_mot_cfg.ini"
// END IKSWR-45692
#else
#define WLAN_INI_FILE              "wlan/" PREFIX "qcom_cfg.ini"
#define WLAN_MAC_FILE              "wlan/" PREFIX "wlan_mac.bin"
#endif /* MSM_PLATFORM */

#endif /* WLAN_HDD_MISC_H */
