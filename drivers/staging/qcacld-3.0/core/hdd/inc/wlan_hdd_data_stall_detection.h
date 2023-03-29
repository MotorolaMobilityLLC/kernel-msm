/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_DATA_STALL_DETECTION_H
#define __WLAN_HDD_DATA_STALL_DETECTION_H

/**
 * DOC: wlan_hdd_data_stall_detection.h
 *
 * WLAN Host Device Driver data stall detection API specification
 */

/**
 * hdd_register_data_stall_detect_cb() - register data stall callback
 *
 * Return: 0 for success or Error code for failure
 */
int hdd_register_data_stall_detect_cb(void);

/**
 * hdd_deregister_data_stall_detect_cb() - de-register data stall callback
 *
 * Return: 0 for success or Error code for failure
 */
int hdd_deregister_data_stall_detect_cb(void);
#endif /* __WLAN_HDD_DATA_STALL_DETECTION_H */
