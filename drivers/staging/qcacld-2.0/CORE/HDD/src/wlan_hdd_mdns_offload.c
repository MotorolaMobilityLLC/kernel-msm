/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * wlan_hdd_mdns_offload.c - WLAN Host Device Driver implementation
 */


#include "wlan_hdd_main.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"
#include "sme_Api.h"
#include "wlan_hdd_mdns_offload.h"

#ifdef MDNS_OFFLOAD

#define MDNS_HEADER_LEN                           12
#define MDNS_FQDN_TYPE_GENERAL                    0
#define MDNS_FQDN_TYPE_UNIQUE                     1
#define MAX_NUM_FIELD_DOMAINNAME                  6
#define MAX_LEN_DOMAINNAME_FIELD                  64
#define MAX_MDNS_RESP_TYPE                        6
#define MDNS_TYPE_A                               1
#define MDNS_TYPE_TXT                             16
#define MDNS_TYPE_PTR                             12
#define MDNS_TYPE_PTR_DNAME                       13
#define MDNS_TYPE_SRV                             33
#define MDNS_TYPE_SRV_TARGET                      34
#define MDNS_CLASS                                1
#define MDNS_TTL                                  5

/* Offload struct */
struct hdd_mdns_resp_info {
	uint8_t	num_entries;
	uint8_t	*data;
	uint16_t	*offset;
};

struct hdd_mdns_resp_matched {
	uint8_t	num_matched;
	uint8_t	type;
};

/**
 * wlan_hdd_mdns_process_response_dname() - Process mDNS domain name
 * @response: Pointer to a struct hdd_mdns_resp_info
 * @resp_info: Pointer to a struct tSirMDNSResponseInfo
 *
 * This function will pack the whole domain name without compression. It will
 * add the leading len for each field and add zero length octet to terminate
 * the domain name.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_process_response_dname(
					struct hdd_mdns_resp_info *response,
					tpSirMDNSResponseInfo resp_info)
{
	uint8_t  num;
	uint16_t idx;
	uint8_t len = 0;

	if ((response == NULL) || (response->data == NULL) ||
		(response->offset == NULL)) {
		hddLog(LOGE, FL("Either data or offset in response is NULL!"));
		return FALSE;
	}

	if ((resp_info == NULL) ||
		(resp_info->resp_len >= MAX_MDNS_RESP_LEN)) {
		hddLog(LOGE, FL("resp_len exceeds %d!"), MAX_MDNS_RESP_LEN);
		return FALSE;
	}

	for (num = 0; num < response->num_entries; num++) {
		response->offset[num] =
					resp_info->resp_len + MDNS_HEADER_LEN;
		idx = num * MAX_LEN_DOMAINNAME_FIELD;
		len = strlen((char *)&response->data[idx]);
		if ((resp_info->resp_len + len + 1) >= MAX_MDNS_RESP_LEN) {
			hddLog(LOGE, FL("resp_len exceeds %d!"),
				MAX_MDNS_RESP_LEN);
			return FALSE;
                }
		resp_info->resp_data[resp_info->resp_len] = len;
		resp_info->resp_len++;
		vos_mem_copy(&resp_info->resp_data[resp_info->resp_len],
				&response->data[idx], len);
		resp_info->resp_len += len;
	}

	/* The domain name terminates with the zero length octet */
	if (num == response->num_entries) {
		if (resp_info->resp_len >= MAX_MDNS_RESP_LEN) {
			hddLog(LOGE, FL("resp_len exceeds %d!"),
				MAX_MDNS_RESP_LEN);
			return FALSE;
		}
		resp_info->resp_data[resp_info->resp_len] = 0;
		resp_info->resp_len++;
	}

	return TRUE;
}

/**
 * wlan_hdd_mdns_format_response_u16() - Form uint16_t response data
 * @value: The uint16_t value is formed to the struct tSirMDNSResponseInfo
 * @resp_info: Pointer to a struct tSirMDNSResponseInfo
 *
 * Return: None
 */
static void wlan_hdd_mdns_format_response_u16(uint16_t value,
					tpSirMDNSResponseInfo resp_info)
{
	uint8_t val_u8;

	if ((resp_info == NULL) || (resp_info->resp_data == NULL))
		return;
	val_u8 = (value & 0xff00) >> 8;
	resp_info->resp_data[resp_info->resp_len++] = val_u8;
	val_u8 = value & 0xff;
	resp_info->resp_data[resp_info->resp_len++] = val_u8;
}

/**
 * wlan_hdd_mdns_format_response_u32() - Form uint32_t response data
 * @value: The uint32_t value is formed to the struct tSirMDNSResponseInfo
 * @resp_info: Pointer to a struct tSirMDNSResponseInfo
 *
 * Return: None
 */
static void wlan_hdd_mdns_format_response_u32(uint32_t value,
					tpSirMDNSResponseInfo resp_info)
{
	uint8_t val_u8;

	if ((resp_info == NULL) || (resp_info->resp_data == NULL))
		return;
	val_u8 = (value & 0xff000000) >> 24;
	resp_info->resp_data[resp_info->resp_len++] = val_u8;
	val_u8 = (value & 0xff0000) >> 16;
	resp_info->resp_data[resp_info->resp_len++] = val_u8;
	val_u8 = (value & 0xff00) >> 8;
	resp_info->resp_data[resp_info->resp_len++] = val_u8;
	val_u8 = value & 0xff;
	resp_info->resp_data[resp_info->resp_len++] = val_u8;
}

/**
 * wlan_hdd_mdns_process_response_misc() - Process misc info in mDNS response
 * @resp_type: Response type for mDNS
 * @resp_info: Pointer to a struct tSirMDNSResponseInfo
 *
 * This function will pack the response type, class and TTL (Time To Live).
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_process_response_misc(uint16_t resp_type,
					tpSirMDNSResponseInfo resp_info)
{
	uint16_t len;

	if (resp_info == NULL) {
		hddLog(LOGE, FL("resp_info is NULL!"));
		return FALSE;
	}

	len = resp_info->resp_len + (3 * sizeof(uint16_t));
	if (len >= MAX_MDNS_RESP_LEN) {
		hddLog(LOGE, FL("resp_len exceeds %d!"), MAX_MDNS_RESP_LEN);
		return FALSE;
	}

	/* Fill Type, Class, TTL */
	wlan_hdd_mdns_format_response_u16(resp_type, resp_info);
	wlan_hdd_mdns_format_response_u16(MDNS_CLASS, resp_info);
	wlan_hdd_mdns_format_response_u32(MDNS_TTL, resp_info);

	return TRUE;
}

/**
 * wlan_hdd_mdns_compress_data() - Compress the domain name in mDNS response
 * @resp_info: Pointer to a struct tSirMDNSResponseInfo
 * @response_dst: The response which domain name is compressed.
 * @response_src: The response which domain name is matched with response_dst.
 *                Its offset is used for data compression.
 * @num_matched: The number of matched entries between response_dst and
 *               response_src
 *
 * This function will form the different fields of domain name in response_dst
 * if any. Then use the offset of the matched domain name in response_src to
 * compress the matched domain name.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_compress_data(tpSirMDNSResponseInfo resp_info,
				struct hdd_mdns_resp_info *response_dst,
				struct hdd_mdns_resp_info *response_src,
				uint8_t num_matched)
{
	uint8_t  num, num_diff;
	uint16_t value, idx;
	uint8_t len = 0;

	if ((response_src == NULL) || (response_dst == NULL) ||
		(resp_info == NULL)) {
		hddLog(LOGE, FL("response info is NULL!"));
		return FALSE;
	}

	if (response_dst->num_entries < num_matched) {
		hddLog(LOGE, FL("num_entries is less than num_matched!"));
		return FALSE;
	}

	if (resp_info->resp_len >= MAX_MDNS_RESP_LEN) {
		hddLog(LOGE, FL("resp_len exceeds %d!"), MAX_MDNS_RESP_LEN);
		return FALSE;
	}

	num_diff = response_dst->num_entries - num_matched;
	if ((num_diff > 0) && (response_dst->data == NULL)) {
		hddLog(LOGE, FL("response_dst->data is NULL!"));
		return FALSE;
	}

	/*
	* Handle the unmatched string at the beginning
	* Store the length of octets and the octets
	*/
	for (num = 0; num < num_diff; num++) {
		response_dst->offset[num] =
			resp_info->resp_len + MDNS_HEADER_LEN;
		idx = num * MAX_LEN_DOMAINNAME_FIELD;
		len = strlen((char *)&response_dst->data[idx]);
		if ((resp_info->resp_len + len + 1) >= MAX_MDNS_RESP_LEN) {
			hddLog(LOGE, FL("resp_len exceeds %d!"),
				MAX_MDNS_RESP_LEN);
			return FALSE;
		}
		resp_info->resp_data[resp_info->resp_len] = len;
		resp_info->resp_len++;
		vos_mem_copy(&resp_info->resp_data[resp_info->resp_len],
			&response_dst->data[idx], len);
		resp_info->resp_len += len;
	}
	/*
	* Handle the matched string from the end
	* Just keep the offset and mask the leading two bit
	*/
	if (response_src->num_entries >= num_matched) {
		num_diff = response_src->num_entries - num_matched;
		value =	response_src->offset[num_diff];
		if (value > 0) {
			value |= 0xc000;
			if ((resp_info->resp_len + sizeof(uint16_t)) >=
				MAX_MDNS_RESP_LEN) {
				hddLog(LOGE, FL("resp_len exceeds %d!"),
					MAX_MDNS_RESP_LEN);
				return FALSE;
			}
			wlan_hdd_mdns_format_response_u16(value, resp_info);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * wlan_hdd_mdns_reset_response() - Reset the response info
 * @response: The response which info is reset.
 *
 * Return: None
 */
static void wlan_hdd_mdns_reset_response(struct hdd_mdns_resp_info *response)
{
	if (response == NULL)
		return;
	response->num_entries = 0;
	response->data = NULL;
	response->offset = NULL;
}

/**
 * wlan_hdd_mdns_init_response() - Initialize the response info
 * @response: The response which info is initiatized.
 * @resp_dname: The domain name string which might be tokenized.
 *
 * This function will allocate the memory for both response->data and
 * response->offset. Besides, it will also tokenize the domain name to some
 * entries and fill response->num_entries with the num of entries.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_init_response(
					struct hdd_mdns_resp_info *response,
					uint8_t *resp_dname, char separator)
{
	uint16_t size;

	if ((resp_dname == NULL) || (response == NULL)) {
		hddLog(LOGE, FL("resp_dname or response is NULL!"));
		return FALSE;
	}

	size = MAX_NUM_FIELD_DOMAINNAME * MAX_LEN_DOMAINNAME_FIELD;
	response->data = vos_mem_malloc(size);
	if (response->data) {
		vos_mem_zero(response->data, size);
		if (VOS_STATUS_SUCCESS !=
			hdd_string_to_string_array((char *)resp_dname,
						response->data,
						separator,
						&response->num_entries,
						MAX_NUM_FIELD_DOMAINNAME,
						MAX_LEN_DOMAINNAME_FIELD)) {
			hddLog(LOGE, FL("hdd_string_to_string_array fail!"));
			goto err_init_resp;
		}

		if ((response->num_entries > 0) &&
			(strlen((char *)&response->data[0]) > 0)) {
			size = sizeof(uint16_t) * response->num_entries;
			response->offset = vos_mem_malloc(size);
			if (response->offset) {
				vos_mem_zero(response->offset, size);
				return TRUE;
			}
		}
	}

err_init_resp:
	if (response->data)
		vos_mem_free(response->data);
	wlan_hdd_mdns_reset_response(response);
	return FALSE;
}

/**
 * wlan_hdd_mdns_find_entries_from_end() - Find the matched entries
 * @response1: The response info is used to be compared.
 * @response2: The response info is used to be compared.
 *
 * This function will find the matched entries from the end.
 *
 * Return: Return the number of the matched entries.
 */
static uint8_t wlan_hdd_mdns_find_entries_from_end(
					struct hdd_mdns_resp_info *response1,
					struct hdd_mdns_resp_info *response2)
{
	uint8_t  min, len1, i;
	uint16_t  num1, num2;
	uint8_t num_matched = 0;

	min = VOS_MIN(response1->num_entries, response2->num_entries);

	for (i = 1; i <= min; i++) {
		num1 = (response1->num_entries - i);
		num1 *= MAX_LEN_DOMAINNAME_FIELD;
		num2 = (response2->num_entries - i);
		num2 *= MAX_LEN_DOMAINNAME_FIELD;
		len1 = strlen((char *)&response1->data[num1]);

		if ((len1 == 0) ||
			(len1 != strlen((char *)&response2->data[num2])))
			break;
		if (memcmp(&response1->data[num1],
			&response2->data[num2], len1))
			break;
		else
			num_matched++;
	}

	return num_matched;
}

/**
 * wlan_hdd_mdns_find_max() - Find the maximum number of the matched entries
 * @matchedlist: Pointer to the array of struct hdd_mdns_resp_matched
 * @numlist: The number of the elements in the array matchedlist.
 *
 * Find the max number of the matched entries among the array matchedlist.
 *
 * Return: None
 */
static void wlan_hdd_mdns_find_max(struct hdd_mdns_resp_matched *matchedlist,
				uint8_t numlist)
{
	int j;
	struct hdd_mdns_resp_matched tmp;

	/* At least two values are used for sorting */
	if ((numlist < 2) || (matchedlist == NULL)) {
		hddLog(LOGE, FL("At least two values are used for sorting!"));
		return;
	}

	for (j = 0; j < numlist-1; j++) {
		if (matchedlist[j].num_matched >
			matchedlist[j+1].num_matched) {
			vos_mem_copy(&tmp, &matchedlist[j],
					sizeof(struct hdd_mdns_resp_matched));
			vos_mem_copy(&matchedlist[j], &matchedlist[j+1],
					sizeof(struct hdd_mdns_resp_matched));
			vos_mem_copy(&matchedlist[j+1], &tmp,
					sizeof(struct hdd_mdns_resp_matched));
		}
	}
}

/**
 * wlan_hdd_mdns_pack_response_type_a() - Pack Type A response
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 * @resptype_a: Pointer to the struct hdd_mdns_resp_info of Type A
 *
 * Type A response include QName, response type, class, TTL and Ipv4.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response_type_a(hdd_config_t *ini_config,
					tpSirMDNSResponseInfo resp_info,
					struct hdd_mdns_resp_info *resptype_a)
{
	uint16_t value;
	uint32_t len;

	if ((ini_config == NULL) || (resp_info == NULL) ||
		(resptype_a == NULL)) {
		hddLog(LOGE, FL("ini_config or response info is NULL!"));
		return FALSE;
	}

	/* No Type A response */
	if (strlen((char *)ini_config->mdns_resp_type_a) <= 0)
		return TRUE;

	/* Wrong response is assigned, just ignore this response */
	if (!wlan_hdd_mdns_init_response(resptype_a,
					ini_config->mdns_resp_type_a, '.'))
		return TRUE;

	/* Process response domain name */
	if (!wlan_hdd_mdns_process_response_dname(resptype_a, resp_info)) {
		hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
			MDNS_TYPE_A);
		return FALSE;
	}

	/* Process response Type, Class, TTL */
	if (!wlan_hdd_mdns_process_response_misc(MDNS_TYPE_A, resp_info)) {
		hddLog(LOGE, FL("Fail to process mDNS misc response (%d)!"),
			MDNS_TYPE_A);
		return FALSE;
	}

	/* Process response RDLength, RData */
	len = sizeof(uint16_t) + sizeof(uint32_t);
	len += resp_info->resp_len;
	if (len >= MAX_MDNS_RESP_LEN) {
		hddLog(LOGE, FL("resp_len exceeds %d!"), MAX_MDNS_RESP_LEN);
		return FALSE;
	}
	value = sizeof(uint32_t);
	wlan_hdd_mdns_format_response_u16(value, resp_info);
	wlan_hdd_mdns_format_response_u32(ini_config->mdns_resp_type_a_ipv4,
					resp_info);

	return TRUE;
}

/**
 * wlan_hdd_mdns_pack_response_type_txt() - Pack Type Txt response
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 * @resptype_txt: Pointer to the struct hdd_mdns_resp_info of Type txt
 * @resptype_a: Pointer to the struct hdd_mdns_resp_info of Type A
 *
 * Type Txt response include QName, response type, class, TTL and text content.
 * Also, it will find the matched QName from resptype_A and compress the data.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response_type_txt(hdd_config_t *ini_config,
				tpSirMDNSResponseInfo resp_info,
				struct hdd_mdns_resp_info *resptype_txt,
				struct hdd_mdns_resp_info *resptype_a)
{
	uint8_t num_matched;
	uint8_t num;
	uint16_t idx;
	uint16_t value = 0;
	uint32_t len;
	uint32_t total_len;
	bool status;
	struct hdd_mdns_resp_info resptype_content;

	if ((ini_config == NULL) || (resp_info == NULL) ||
		(resptype_txt == NULL)) {
		hddLog(LOGE, FL("ini_config or response info is NULL!"));
		return FALSE;
	}

	/* No Type Txt response */
	if (strlen((char *)ini_config->mdns_resp_type_txt) <= 0)
		return TRUE;

	/* Wrong response is assigned, just ignore this response */
	if (!wlan_hdd_mdns_init_response(resptype_txt,
				ini_config->mdns_resp_type_txt, '.'))
		return TRUE;

	/*
	* For data compression
	* Check if any strings are matched with Type A response
	*/
	if (resptype_a && (resptype_a->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_txt,
								resptype_a);
		if (num_matched > 0) {
			if (!wlan_hdd_mdns_compress_data(resp_info,
				resptype_txt, resptype_a, num_matched)) {
				hddLog(LOGE, FL("Fail to compress mDNS "
					"response (%d)!"), MDNS_TYPE_TXT);
				return FALSE;
			}
		} else {
			/*
			* num_matched is zero. Error!
			* At least ".local" is needed.
			*/
			hddLog(LOGE, FL("No matched string! Fail to pack mDNS "
					"response (%d)!"), MDNS_TYPE_TXT);
			return FALSE;
		}
	} else {
		/* no TypeA response, so show the whole data */
		if (!wlan_hdd_mdns_process_response_dname(resptype_txt,
							resp_info)) {
			hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
				MDNS_TYPE_TXT);
			return FALSE;
		}
	}

	/* Process response Type, Class, TTL */
	if (!wlan_hdd_mdns_process_response_misc(MDNS_TYPE_TXT, resp_info)) {
		hddLog(LOGE, FL("Fail to process mDNS misc response (%d)!"),
			MDNS_TYPE_TXT);
		return FALSE;
	}

	/*
	* Process response RDLength, RData.
	* TypeTxt RData include len.
	*/
	status = wlan_hdd_mdns_init_response(&resptype_content,
				ini_config->mdns_resp_type_txt_content,
				'/');
	if (status == FALSE) {
		hddLog(LOGE, FL("wlan_hdd_mdns_init_response FAIL"));
		return FALSE;
	}

	for (num = 0; num < resptype_content.num_entries; num++) {
		idx = num * MAX_LEN_DOMAINNAME_FIELD;
		value += strlen((char *)&resptype_content.data[idx]);
	}

	/* content len is uint16_t */
	total_len = sizeof(uint16_t);
	total_len += resp_info->resp_len + value + resptype_content.num_entries;

	if (total_len >= MAX_MDNS_RESP_LEN) {
		hddLog(LOGE, FL("resp_len exceeds %d!"), MAX_MDNS_RESP_LEN);
		return FALSE;
	}
	wlan_hdd_mdns_format_response_u16(value + resptype_content.num_entries,
						resp_info);

	for (num = 0; num < resptype_content.num_entries; num++) {
		idx = num * MAX_LEN_DOMAINNAME_FIELD;
		len = strlen((char *)&resptype_content.data[idx]);
		resp_info->resp_data[resp_info->resp_len] = len;
		resp_info->resp_len++;

		vos_mem_copy(&resp_info->resp_data[resp_info->resp_len],
			&resptype_content.data[idx], len);

		resp_info->resp_len += len;
		hddLog(LOGE, FL("index = %d, len = %d, str = %s"),
			num, len, &resptype_content.data[idx]);
	}

	return TRUE;
}

/**
 * wlan_hdd_mdns_pack_response_type_ptr_dname() - Pack Type PTR domain name
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 * @resptype_ptr_dn: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 *                     domain name
 * @resptype_ptr: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 * @resptype_txt: Pointer to the struct hdd_mdns_resp_info of Type Txt
 * @resptype_a: Pointer to the struct hdd_mdns_resp_info of Type A
 *
 * The Type Ptr response include Type PTR domain name in its data field.
 * Also, it will find the matched QName from the existing resptype_ptr,
 * resptype_txt, resptype_a and then compress the data.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response_type_ptr_dname(
				hdd_config_t *ini_config,
				tpSirMDNSResponseInfo resp_info,
				struct hdd_mdns_resp_info *resptype_ptr_dn,
				struct hdd_mdns_resp_info *resptype_ptr,
				struct hdd_mdns_resp_info *resptype_txt,
				struct hdd_mdns_resp_info *resptype_a)
{
	uint8_t  num_matched, numlist, size;
	struct hdd_mdns_resp_matched matchedlist[MAX_MDNS_RESP_TYPE-1];
	struct hdd_mdns_resp_info *resp;

	if ((ini_config == NULL) || (resp_info == NULL) ||
		(resptype_ptr == NULL) || (resptype_ptr_dn == NULL)) {
		hddLog(LOGE, FL("ini_config or response info is NULL!"));
		return FALSE;
	}

	/* No Type Ptr domain name response */
	if (strlen((char *)ini_config->mdns_resp_type_ptr_dname) <= 0)
		return TRUE;

	/* Wrong response is assigned, just ignore this response */
	if (!wlan_hdd_mdns_init_response(resptype_ptr_dn,
				ini_config->mdns_resp_type_ptr_dname, '.'))
		return TRUE;

	/*
	* For data compression
	* Check if any strings are matched with previous
	* response.
	*/
	numlist = 0;
	size = (MAX_MDNS_RESP_TYPE-1);
	size *= sizeof(struct hdd_mdns_resp_matched);
	vos_mem_zero(matchedlist, size);
	num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_ptr_dn,
							resptype_ptr);
	if (num_matched > 0) {
		matchedlist[numlist].num_matched = num_matched;
		matchedlist[numlist].type = MDNS_TYPE_PTR;
		numlist++;
	}
	if (resptype_txt && (resptype_txt->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(
					resptype_ptr_dn, resptype_txt);
		if (num_matched > 0) {
			matchedlist[numlist].num_matched = num_matched;
			matchedlist[numlist].type = MDNS_TYPE_TXT;
			numlist++;
		}
	}
	if (resptype_a && (resptype_a->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(
					resptype_ptr_dn,resptype_a);
		if (num_matched > 0) {
			matchedlist[numlist].num_matched = num_matched;
			matchedlist[numlist].type = MDNS_TYPE_A;
			numlist++;
		}
	}
	if (numlist > 0) {
		if (numlist > 1)
			wlan_hdd_mdns_find_max(matchedlist, numlist);
		resp = NULL;
		switch (matchedlist[numlist-1].type) {
		case MDNS_TYPE_A:
			resp = resptype_a;
			break;
		case MDNS_TYPE_TXT:
			resp = resptype_txt;
			break;
		case MDNS_TYPE_PTR:
			resp = resptype_ptr;
			break;
		default:
			hddLog(LOGE, FL("Fail to compress mDNS response "
					"(%d)!"), MDNS_TYPE_PTR_DNAME);
			return FALSE;
		}
		num_matched = matchedlist[numlist-1].num_matched;
		if (!wlan_hdd_mdns_compress_data(resp_info, resptype_ptr_dn,
						resp, num_matched)) {
			hddLog(LOGE, FL("Fail to compress mDNS response "
					"(%d)!"), MDNS_TYPE_PTR_DNAME);
			return FALSE;
		}
	} else {
		/* num = 0 -> no matched string */
		if (!wlan_hdd_mdns_process_response_dname(resptype_ptr_dn,
							resp_info)) {
			hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
					MDNS_TYPE_PTR_DNAME);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * wlan_hdd_mdns_pack_response_type_ptr() - Pack Type PTR response
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 * @resptype_ptr: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 * @resptype_ptr_dn: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 *                     domain name
 * @resptype_txt: Pointer to the struct hdd_mdns_resp_info of Type Txt
 * @resptype_a: Pointer to the struct hdd_mdns_resp_info of Type A
 *
 * The Type Ptr response include QName, response type, class, TTL and
 * Type PTR domain name. Also, it will find the matched QName from the
 * existing resptype_txt, resptype_a and then compress the data.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response_type_ptr(hdd_config_t *ini_config,
				tpSirMDNSResponseInfo resp_info,
				struct hdd_mdns_resp_info *resptype_ptr,
				struct hdd_mdns_resp_info *resptype_ptr_dn,
				struct hdd_mdns_resp_info *resptype_txt,
				struct hdd_mdns_resp_info *resptype_a)
{
	uint8_t num_matched, num_matched1;
	uint16_t value;
	uint8_t val_u8;
	uint32_t offset_data_len, len;

	if ((ini_config == NULL) || (resp_info == NULL) ||
		(resptype_ptr == NULL) || (resptype_ptr_dn == NULL)) {
		hddLog(LOGE, FL("ini_config or response info is NULL!"));
		return FALSE;
	}

	/* No Type Ptr response */
	if (strlen((char *)ini_config->mdns_resp_type_ptr) <= 0)
		return TRUE;

	/* Wrong response is assigned, just ignore this response */
	if (!wlan_hdd_mdns_init_response(resptype_ptr,
					ini_config->mdns_resp_type_ptr, '.'))
		return TRUE;

	/*
	* For data compression
	* Check if any strings are matched with Type A response
	*/
	num_matched  = 0;
	num_matched1 = 0;
	if (resptype_a && (resptype_a->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_ptr,
								resptype_a);
	}
	if (resptype_txt && (resptype_txt->num_entries > 0)) {
		num_matched1 = wlan_hdd_mdns_find_entries_from_end(
						resptype_ptr, resptype_txt);
	}
	if ((num_matched != num_matched1) ||
		((num_matched > 0) && (num_matched1 > 0))) {
		if (num_matched >= num_matched1) {
			if (!wlan_hdd_mdns_compress_data(resp_info,
				resptype_ptr, resptype_a, num_matched)) {
				hddLog(LOGE, FL("Fail to compress mDNS "
					"response (%d)!"), MDNS_TYPE_PTR);
				return FALSE;
			}
		} else {
			/* num_matched is less than num_matched1 */
			if (!wlan_hdd_mdns_compress_data(resp_info,
				resptype_ptr, resptype_txt, num_matched1)) {
				hddLog(LOGE, FL("Fail to compress mDNS "
					"response (%d)!"), MDNS_TYPE_PTR);
				return FALSE;
			}
		}
	} else {
		/*
		* Both num_matched and num_matched1 are zero.
		* no TypeA & TypeTxt
		*/
		if (!wlan_hdd_mdns_process_response_dname(resptype_ptr,
							resp_info)) {
			hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
				MDNS_TYPE_PTR);
			return FALSE;
		}
	}

	/* Process response Type, Class, TTL */
	if (!wlan_hdd_mdns_process_response_misc(MDNS_TYPE_PTR,	resp_info)) {
		hddLog(LOGE, FL("Fail to process mDNS misc response (%d)!"),
			MDNS_TYPE_PTR);
		return FALSE;
	}

	/*
	* Process response RDLength, RData (Ptr domain name)
	* Save the offset of RData length
	*/
	offset_data_len = resp_info->resp_len;
	resp_info->resp_len += sizeof(uint16_t);

	if (!wlan_hdd_mdns_pack_response_type_ptr_dname(ini_config, resp_info,
						resptype_ptr_dn, resptype_ptr,
						resptype_txt, resptype_a)) {
		return FALSE;
	}
	/* Set the RData length */
	len = offset_data_len + sizeof(uint16_t);
	if ((resptype_ptr_dn->num_entries > 0) &&
		(resp_info->resp_len > len)) {
		value = resp_info->resp_len - len;
		val_u8 = (value & 0xff00) >> 8;
		resp_info->resp_data[offset_data_len] = val_u8;
		val_u8 = value & 0xff;
		resp_info->resp_data[offset_data_len+1] = val_u8;
	} else {
		hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
			MDNS_TYPE_PTR);
		return FALSE;
	}

	return TRUE;
}

/**
 * wlan_hdd_mdns_pack_response_type_srv_target()- Pack Type Service Target
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 * @resptype_srv_tgt: Pointer to the struct hdd_mdns_resp_info of Type Srv
 *                      target
 * @resptype_srv: Pointer to the struct hdd_mdns_resp_info of Type Srv
 * @resptype_ptr: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 * @resptype_ptr_dn: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 *                     domain name
 * @resptype_txt: Pointer to the struct hdd_mdns_resp_info of Type Txt
 * @resptype_a: Pointer to the struct hdd_mdns_resp_info of Type A
 *
 * The Type service target is one of the data field in the Type SRV response.
 * Also, it will find the matched QName from the existing resptype_srv,
 * resptype_ptr, resptype_ptr_dn, resptype_txt, resptype_a and then compress
 * the data.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response_type_srv_target(
				hdd_config_t *ini_config,
				tpSirMDNSResponseInfo resp_info,
				struct hdd_mdns_resp_info *resptype_srv_tgt,
				struct hdd_mdns_resp_info *resptype_srv,
				struct hdd_mdns_resp_info *resptype_ptr,
				struct hdd_mdns_resp_info *resptype_ptr_dn,
				struct hdd_mdns_resp_info *resptype_txt,
				struct hdd_mdns_resp_info *resptype_a)
{
	uint8_t  num_matched, num, size;
	struct hdd_mdns_resp_matched matchedlist[MAX_MDNS_RESP_TYPE-1];
	struct hdd_mdns_resp_info *resp;

	if ((ini_config == NULL) || (resp_info == NULL) ||
		(resptype_srv == NULL) || (resptype_srv_tgt == NULL)) {
		hddLog(LOGE, FL("ini_config or response info is NULL!"));
		return FALSE;
	}

	/* No Type Srv Target response */
	if (strlen((char *)ini_config->mdns_resp_type_srv_target) <= 0)
		return TRUE;

	/* Wrong response is assigned, just ignore this response */
	if (!wlan_hdd_mdns_init_response(resptype_srv_tgt,
				ini_config->mdns_resp_type_srv_target, '.'))
		return TRUE;

	/*
	* For data compression
	* Check if any strings are matched with previous response.
	*/
	num = 0;
	size = (MAX_MDNS_RESP_TYPE-1);
	size *= sizeof(struct hdd_mdns_resp_matched);
	vos_mem_zero(matchedlist, size);
	num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_srv_tgt,
							resptype_srv);
	if (num_matched > 0) {
		matchedlist[num].num_matched = num_matched;
		matchedlist[num].type = MDNS_TYPE_SRV;
		num++;
	}
	if (resptype_ptr && (resptype_ptr->num_entries > 0)) {
		if (resptype_ptr_dn && (resptype_ptr_dn->num_entries > 0)) {
			num_matched = wlan_hdd_mdns_find_entries_from_end(
					resptype_srv_tgt, resptype_ptr_dn);
			if (num_matched > 0) {
				matchedlist[num].num_matched = num_matched;
				matchedlist[num].type = MDNS_TYPE_PTR_DNAME;
				num++;
			}
		}
		num_matched = wlan_hdd_mdns_find_entries_from_end(
					resptype_srv_tgt, resptype_ptr);
		if (num_matched > 0) {
			matchedlist[num].num_matched = num_matched;
			matchedlist[num].type = MDNS_TYPE_PTR;
			num++;
		}
	}
	if (resptype_txt && (resptype_txt->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(
					resptype_srv_tgt, resptype_txt);
		if (num_matched > 0) {
			matchedlist[num].num_matched = num_matched;
			matchedlist[num].type = MDNS_TYPE_TXT;
			num++;
		}
	}
	if (resptype_a && (resptype_a->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(
					resptype_srv_tgt, resptype_a);
		if (num_matched > 0) {
			matchedlist[num].num_matched = num_matched;
			matchedlist[num].type = MDNS_TYPE_A;
			num++;
		}
	}
	if (num > 0) {
		if (num > 1)
			wlan_hdd_mdns_find_max(matchedlist, num);
		resp = NULL;
		switch (matchedlist[num-1].type) {
		case MDNS_TYPE_A:
			resp = resptype_a;
			break;
		case MDNS_TYPE_TXT:
			resp = resptype_txt;
			break;
		case MDNS_TYPE_PTR:
			resp = resptype_ptr;
			break;
		case MDNS_TYPE_PTR_DNAME:
			resp = resptype_ptr_dn;
			break;
		case MDNS_TYPE_SRV:
			resp = resptype_srv;
			break;
		default:
			hddLog(LOGE, FL("Fail to compress mDNS response "
					"(%d)!"), MDNS_TYPE_SRV_TARGET);
			return FALSE;
		}
		num_matched = matchedlist[num-1].num_matched;
		if (!wlan_hdd_mdns_compress_data(resp_info, resptype_srv_tgt,
						resp, num_matched)) {
			hddLog(LOGE, FL("Fail to compress mDNS response "
					"(%d)!"), MDNS_TYPE_SRV_TARGET);
			return FALSE;
		}
	} else {
		/* num = 0 -> no matched string */
		if (!wlan_hdd_mdns_process_response_dname(resptype_srv_tgt,
							resp_info)) {
			hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
					MDNS_TYPE_SRV_TARGET);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * wlan_hdd_mdns_pack_response_type_srv()- Pack Type Service response
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 * @resptype_srv: Pointer to the struct hdd_mdns_resp_info of Type Srv
 * @resptype_srv_tgt: Pointer to the struct hdd_mdns_resp_info of Type Srv
 *                      target
 * @resptype_ptr: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 * @resptype_ptr_dn: Pointer to the struct hdd_mdns_resp_info of Type Ptr
 *                     domain name
 * @resptype_txt: Pointer to the struct hdd_mdns_resp_info of Type Txt
 * @resptype_a: Pointer to the struct hdd_mdns_resp_info of Type A
 *
 * The Type SRV (Service) response include QName, response type, class, TTL
 * and four kinds of data fields. Also, it will find the matched QName from
 * the existing resptype_ptr, resptype_ptr_dn, resptype_txt, resptype_a and
 * then compress the data.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response_type_srv(hdd_config_t *ini_config,
				tpSirMDNSResponseInfo resp_info,
				struct hdd_mdns_resp_info *resptype_srv,
				struct hdd_mdns_resp_info *resptype_srv_tgt,
				struct hdd_mdns_resp_info *resptype_ptr,
				struct hdd_mdns_resp_info *resptype_ptr_dn,
				struct hdd_mdns_resp_info *resptype_txt,
				struct hdd_mdns_resp_info *resptype_a)
{
	uint8_t num_matched, num, size;
	uint16_t value;
	uint8_t val_u8;
	uint32_t offset_data_len, len;
	struct hdd_mdns_resp_info *resp;
	struct hdd_mdns_resp_matched matchedlist[MAX_MDNS_RESP_TYPE-1];

	if ((ini_config == NULL) || (resp_info == NULL) ||
		(resptype_srv == NULL) || (resptype_srv_tgt == NULL)) {
		hddLog(LOGE, FL("ini_config or response info is NULL!"));
		return FALSE;
	}

	/* No Type Srv response */
	if (strlen((char *)ini_config->mdns_resp_type_srv) <= 0)
		return TRUE;

	/* Wrong response is assigned, just ignore this response */
	if (!wlan_hdd_mdns_init_response(resptype_srv,
					ini_config->mdns_resp_type_srv, '.'))
		return TRUE;

	/*
	* For data compression
	* Check if any strings are matched with Type A response
	*/
	num = 0;
	size = (MAX_MDNS_RESP_TYPE-1);
	size *= sizeof(struct hdd_mdns_resp_matched);
	vos_mem_zero(matchedlist, size);
	if (resptype_ptr && (resptype_ptr->num_entries > 0)) {
		if (resptype_ptr_dn && (resptype_ptr_dn->num_entries > 0)) {
			num_matched = wlan_hdd_mdns_find_entries_from_end(
							resptype_srv,
							resptype_ptr_dn);
			if (num_matched > 0) {
				matchedlist[num].num_matched = num_matched;
				matchedlist[num].type =	MDNS_TYPE_PTR_DNAME;
				num++;
			}
		}
		num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_srv,
								resptype_ptr);
		if (num_matched > 0) {
			matchedlist[num].num_matched = num_matched;
			matchedlist[num].type = MDNS_TYPE_PTR;
			num++;
		}
	}
	if (resptype_txt && (resptype_txt->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_srv,
								resptype_txt);
		if (num_matched > 0) {
			matchedlist[num].num_matched =num_matched;
			matchedlist[num].type = MDNS_TYPE_TXT;
			num++;
		}
	}
	if (resptype_a && (resptype_a->num_entries > 0)) {
		num_matched = wlan_hdd_mdns_find_entries_from_end(resptype_srv,
								resptype_a);
		if (num_matched > 0) {
			matchedlist[num].num_matched = num_matched;
			matchedlist[num].type = MDNS_TYPE_A;
			num++;
		}
	}
	if (num > 0) {
		if (num > 1)
			wlan_hdd_mdns_find_max(matchedlist, num);
		resp = NULL;
		switch (matchedlist[num-1].type) {
		case MDNS_TYPE_A:
			resp = resptype_a;
			break;
		case MDNS_TYPE_TXT:
			resp = resptype_txt;
			break;
		case MDNS_TYPE_PTR:
			resp = resptype_ptr;
			break;
		case MDNS_TYPE_PTR_DNAME:
			resp = resptype_ptr_dn;
			break;
		default:
			hddLog(LOGE, FL("Fail to compress mDNS response "
					"(%d)!"), MDNS_TYPE_SRV);
			return FALSE;
		}
		num_matched = matchedlist[num-1].num_matched;
		if (!wlan_hdd_mdns_compress_data(resp_info, resptype_srv,
						resp, num_matched)) {
			hddLog(LOGE, FL("Fail to compress mDNS response "
					"(%d)!"), MDNS_TYPE_SRV);
			return FALSE;
		}
	} else {
		/* num = 0 -> no matched string */
		if (!wlan_hdd_mdns_process_response_dname(resptype_srv,
							resp_info)) {
			hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
					MDNS_TYPE_SRV);
			return FALSE;
		}
	}

	/* Process response Type, Class, TTL */
	if (!wlan_hdd_mdns_process_response_misc(MDNS_TYPE_SRV,	resp_info)) {
		hddLog(LOGE, FL("Fail to process mDNS misc response (%d)!"),
					MDNS_TYPE_SRV);
		return FALSE;
	}

	/*
	* Process response RDLength, RData (Srv target name)
	* Save the offset of RData length
	*/
	offset_data_len = resp_info->resp_len;
	resp_info->resp_len += sizeof(uint16_t);

	len = resp_info->resp_len + (3 * sizeof(uint16_t));
	if (len >= MAX_MDNS_RESP_LEN) {
		hddLog(LOGE, FL("resp_len exceeds %d!"), MAX_MDNS_RESP_LEN);
		return FALSE;
	}

	/* set Srv Priority */
	value = ini_config->mdns_resp_type_srv_priority;
	wlan_hdd_mdns_format_response_u16(value, resp_info);
	/* set Srv Weight */
	value = ini_config->mdns_resp_type_srv_weight;
	wlan_hdd_mdns_format_response_u16(value, resp_info);
	/* set Srv Port */
	value = ini_config->mdns_resp_type_srv_port;
	wlan_hdd_mdns_format_response_u16(value, resp_info);

	if (!wlan_hdd_mdns_pack_response_type_srv_target(ini_config, resp_info,
						resptype_srv_tgt, resptype_srv,
						resptype_ptr, resptype_ptr_dn,
						resptype_txt, resptype_a)) {
		return FALSE;
	}
	/* Set the RData length */
	len = offset_data_len + sizeof(uint16_t);
	if ((resptype_srv_tgt->num_entries > 0) &&
		(resp_info->resp_len > len)) {
		value = resp_info->resp_len - len;
		val_u8 = (value & 0xff00) >> 8;
		resp_info->resp_data[offset_data_len] = val_u8;
		val_u8 = value & 0xff;
		resp_info->resp_data[offset_data_len+1] = val_u8;
	} else {
		hddLog(LOGE, FL("Fail to process mDNS response (%d)!"),
			MDNS_TYPE_SRV);
		return FALSE;
	}

	return TRUE;
}

/**
 * wlan_hdd_mdns_free_mem() - Free the allocated memory
 * @response: Pointer to the struct hdd_mdns_resp_info
 *
 * Return: None
 */
static void wlan_hdd_mdns_free_mem(struct hdd_mdns_resp_info *response)
{
	if (response && response->data)
		vos_mem_free(response->data);
	if (response && response->offset)
		vos_mem_free(response->offset);
}

/**
 * wlan_hdd_mdns_pack_response() - Pack mDNS response
 * @ini_config: Pointer to the struct hdd_config_t
 * @resp_info: Pointer to the struct tSirMDNSResponseInfo
 *
 * This function will pack four types of responses (Type A, Type Txt, Type Ptr
 * and Type Service). Each response contains QName, response type, class, TTL
 * and data fields.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
static bool wlan_hdd_mdns_pack_response(hdd_config_t *ini_config,
					tpSirMDNSResponseInfo resp_info)
{
	struct hdd_mdns_resp_info resptype_a, resptype_txt;
	struct hdd_mdns_resp_info resptype_ptr, resptype_ptr_dn;
	struct hdd_mdns_resp_info resptype_srv, resptype_srv_tgt;
	uint32_t num_res_records = 0;
	bool status = FALSE;

	wlan_hdd_mdns_reset_response(&resptype_a);
	wlan_hdd_mdns_reset_response(&resptype_txt);
	wlan_hdd_mdns_reset_response(&resptype_ptr);
	wlan_hdd_mdns_reset_response(&resptype_ptr_dn);
	wlan_hdd_mdns_reset_response(&resptype_srv);
	wlan_hdd_mdns_reset_response(&resptype_srv_tgt);

	resp_info->resp_len = 0;

	/* Process Type A response */
	if (!wlan_hdd_mdns_pack_response_type_a(ini_config, resp_info,
						&resptype_a))
		goto err_resptype_a;

	if ((resptype_a.num_entries > 0) &&
		(strlen((char *)&resptype_a.data[0]) > 0))
		num_res_records++;

	/* Process Type TXT response */
	if (!wlan_hdd_mdns_pack_response_type_txt(ini_config, resp_info,
					&resptype_txt, &resptype_a))
		goto err_resptype_txt;

	if ((resptype_txt.num_entries > 0) &&
		(strlen((char *)&resptype_txt.data[0]) > 0))
		num_res_records++;

	/* Process Type PTR response */
	if (!wlan_hdd_mdns_pack_response_type_ptr(ini_config, resp_info,
					&resptype_ptr, &resptype_ptr_dn,
					&resptype_txt, &resptype_a))
		goto err_resptype_ptr;

	if ((resptype_ptr.num_entries > 0) &&
		(strlen((char *)&resptype_ptr.data[0]) > 0))
		num_res_records++;

	/* Process Type SRV response */
	if (!wlan_hdd_mdns_pack_response_type_srv(ini_config, resp_info,
				&resptype_srv, &resptype_srv_tgt,
				&resptype_ptr, &resptype_ptr_dn,
				&resptype_txt, &resptype_a))
		goto err_resptype_srv;

	if ((resptype_srv.num_entries > 0) &&
		(strlen((char *)&resptype_srv.data[0]) > 0))
		num_res_records++;

	resp_info->resourceRecord_count = num_res_records;
	hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
		"%s: Pack mDNS response data successfully!", __func__);
	status = TRUE;

err_resptype_srv:
	wlan_hdd_mdns_free_mem(&resptype_srv);
	wlan_hdd_mdns_free_mem(&resptype_srv_tgt);

err_resptype_ptr:
	wlan_hdd_mdns_free_mem(&resptype_ptr);
	wlan_hdd_mdns_free_mem(&resptype_ptr_dn);

err_resptype_txt:
	wlan_hdd_mdns_free_mem(&resptype_txt);

err_resptype_a:
	wlan_hdd_mdns_free_mem(&resptype_a);

	return status;
}

/**
 * wlan_hdd_set_mdns_offload() - Enable mDNS offload
 * @hostapd_adapter: Pointer to the struct hdd_adapter_t
 *
 * This function will set FQDN/unique FQDN (full qualified domain name)
 * and the mDNS response. Then send them to SME.
 *
 * Return: Return boolean. TRUE for success, FALSE for fail.
 */
bool wlan_hdd_set_mdns_offload(hdd_adapter_t *hostapd_adapter)
{
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(hostapd_adapter);
	tpSirMDNSOffloadInfo mdns_offload_info;
	tpSirMDNSFqdnInfo mdns_fqdn_info;
	tpSirMDNSResponseInfo mdns_resp_info;
	uint32_t fqdn_len, ufqdn_len;

	if (pHddCtx->cfg_ini->enable_mdns_offload !=
					CFG_MDNS_OFFLOAD_SUPPORT_ENABLE)
		return FALSE;

	/* 1. Prepare the MDNS fqdn request to send to SME */
	fqdn_len = strlen(pHddCtx->cfg_ini->mdns_fqdn);
	ufqdn_len = strlen(pHddCtx->cfg_ini->mdns_uniquefqdn);
	if ((fqdn_len == 0) && (ufqdn_len == 0)) {
		hddLog(LOGE, FL("No mDNS FQDN or UFQDN is assigned!"));
		return FALSE;
	}

	mdns_fqdn_info = vos_mem_malloc(sizeof(*mdns_fqdn_info));
	if (NULL == mdns_fqdn_info) {
		hddLog(LOGE, FL("could not allocate tSirMDNSFqdnInfo!"));
		return FALSE;
	}
	/* MDNS fqdn request */
	if (fqdn_len > 0) {
		vos_mem_zero(mdns_fqdn_info, sizeof(*mdns_fqdn_info));
		mdns_fqdn_info->vdev_id = hostapd_adapter->sessionId;
		mdns_fqdn_info->fqdn_type = MDNS_FQDN_TYPE_GENERAL;
		mdns_fqdn_info->fqdn_len = fqdn_len;
		vos_mem_copy(mdns_fqdn_info->fqdn_data,
			pHddCtx->cfg_ini->mdns_fqdn,
			mdns_fqdn_info->fqdn_len);

		if (eHAL_STATUS_SUCCESS !=
			sme_setMDNSFqdn(pHddCtx->hHal, mdns_fqdn_info)) {
			hddLog(LOGE, FL("sme_setMDNSFqdn fail!"));
			vos_mem_free(mdns_fqdn_info);
			return FALSE;
		}
	}
	/* MDNS unique fqdn request */
	if (ufqdn_len > 0) {
		vos_mem_zero(mdns_fqdn_info, sizeof(*mdns_fqdn_info));
		mdns_fqdn_info->vdev_id = hostapd_adapter->sessionId;
		mdns_fqdn_info->fqdn_type = MDNS_FQDN_TYPE_UNIQUE;
		mdns_fqdn_info->fqdn_len = ufqdn_len;
		vos_mem_copy(mdns_fqdn_info->fqdn_data,
			pHddCtx->cfg_ini->mdns_uniquefqdn,
			mdns_fqdn_info->fqdn_len);

		if (eHAL_STATUS_SUCCESS !=
			sme_setMDNSFqdn(pHddCtx->hHal, mdns_fqdn_info)) {
			hddLog(LOGE, FL("sme_setMDNSFqdn fail!"));
			vos_mem_free(mdns_fqdn_info);
			return FALSE;
		}
	}
	vos_mem_free(mdns_fqdn_info);

	/* 2. Prepare the MDNS response request to send to SME */
	mdns_resp_info = vos_mem_malloc(sizeof(*mdns_resp_info));
	if (NULL == mdns_resp_info) {
		hddLog(LOGE, FL("could not allocate tSirMDNSResponseInfo!"));
		return FALSE;
	}

	vos_mem_zero(mdns_resp_info, sizeof(*mdns_resp_info));
	mdns_resp_info->vdev_id = hostapd_adapter->sessionId;

	if (!wlan_hdd_mdns_pack_response(pHddCtx->cfg_ini, mdns_resp_info)) {
		hddLog(LOGE, FL("wlan_hdd_pack_mdns_response fail!"));
		vos_mem_free(mdns_resp_info);
		return FALSE;
	}

	if (eHAL_STATUS_SUCCESS !=
			sme_setMDNSResponse(pHddCtx->hHal, mdns_resp_info)) {
		hddLog(LOGE, FL("sme_setMDNSResponse fail!"));
		vos_mem_free(mdns_resp_info);
		return FALSE;
	}

	vos_mem_free(mdns_resp_info);

	/* 3. Prepare the MDNS Enable request to send to SME */
	mdns_offload_info = vos_mem_malloc(sizeof(*mdns_offload_info));
	if (NULL == mdns_offload_info) {
		hddLog(LOGE, FL("could not allocate tSirMDNSOffloadInfo!"));
		return FALSE;
	}

	vos_mem_zero(mdns_offload_info, sizeof(*mdns_offload_info));
	mdns_offload_info->vdev_id = hostapd_adapter->sessionId;
	mdns_offload_info->mDNSOffloadEnabled =
					pHddCtx->cfg_ini->enable_mdns_offload;

	if (eHAL_STATUS_SUCCESS !=
			sme_setMDNSOffload(pHddCtx->hHal, mdns_offload_info)) {
		hddLog(LOGE, FL("sme_setMDNSOffload fail!"));
		vos_mem_free(mdns_offload_info);
		return FALSE;
	}

	vos_mem_free(mdns_offload_info);
	hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
		"%s: enable mDNS offload successfully!", __func__);
	return TRUE;
}
#endif /* MDNS_OFFLOAD */
