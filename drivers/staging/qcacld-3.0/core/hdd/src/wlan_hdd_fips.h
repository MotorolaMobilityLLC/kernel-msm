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

/**
 * DOC: wlan_hdd_fips.h
 *
 * WLAN Host Device Driver FIPS Certification Feature
 */

#ifndef __WLAN_HDD_FIPS_H__
#define __WLAN_HDD_FIPS_H__

struct net_device;
struct iw_request_info;
union iwreq_data;
struct hdd_adapter;
void fips_test(struct hdd_adapter *adapter);

#define FIPS_KEY_LEN 32
struct iw_fips_test_request {
	uint32_t operation;
	uint32_t mode;
	uint32_t key_len;
	uint8_t  key[FIPS_KEY_LEN];
	uint32_t data_len;
	uint8_t  data[0];
};

struct iw_fips_test_response {
	uint32_t status;
	uint32_t data_len;
	uint8_t  data[0];
};


/**
 * hdd_fips_test() - Perform FIPS test
 * @dev: netdev upon which the FIPS test ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data structure
 * @extra: extra payload for wireless extensions ioctl
 *
 * This API implements the FIPS test interface. Upon entry the @extra
 * buffer will contain a FIPS test vector formated as a &struct
 * iw_fips_test_request. This vector will be sent to firmware where it
 * will be run through the appropriate hardware. The result of the
 * operation will be sent back to userspace via @extra encoded as a
 * &struct iw_fips_test_response.
 *
 * Return: 0 if the test vector was processed, otherwise a negative
 *         errno.
 */

int hdd_fips_test(struct net_device *dev,
		  struct iw_request_info *info,
		  union iwreq_data *wrqu, char *extra);

#endif /* __WLAN_HDD_FIPS_H__ */
