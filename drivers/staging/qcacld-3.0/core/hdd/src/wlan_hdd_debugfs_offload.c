/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs_offload.c
 *
 * WLAN Host Device Driver implementation to update
 * debugfs with offload information
 */

#include <wlan_hdd_debugfs_csr.h>
#include <wlan_hdd_main.h>
#include <cds_sched.h>
#include <wma_api.h>
#include "qwlan_version.h"
#include "wmi_unified_param.h"
#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_ns_public_struct.h"
#include "wlan_pmo_mc_addr_filtering_public_struct.h"
#include "wlan_pmo_ucfg_api.h"
#include "wlan_hdd_object_manager.h"

/* IPv6 address string */
#define IPV6_MAC_ADDRESS_STR_LEN 47  /* Including null terminator */

/**
 * wlan_hdd_mc_addr_list_info_debugfs() - Populate mc addr list info
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 * @buf: output buffer to hold mc addr list info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_mc_addr_list_info_debugfs(struct hdd_context *hdd_ctx,
				   struct hdd_adapter *adapter, uint8_t *buf,
				   ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret;
	uint8_t i;
	struct pmo_mc_addr_list mc_addr_list = {0};
	QDF_STATUS status;

	if (!ucfg_pmo_is_mc_addr_list_enabled(hdd_ctx->psoc)) {
		ret = scnprintf(buf, buf_avail_len,
				"\nMC addr ini is disabled\n");
		if (ret > 0)
			length = ret;
		return length;
	}

	status = ucfg_pmo_get_mc_addr_list(hdd_ctx->psoc,
					   adapter->vdev_id,
					   &mc_addr_list);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		ret = scnprintf(buf, buf_avail_len,
				"\nMC addr list query is failed\n");
		if (ret > 0)
			length = ret;
		return length;
	}

	if (mc_addr_list.mc_cnt == 0) {
		ret = scnprintf(buf, buf_avail_len,
				"\nMC addr list is empty\n");
		if (ret > 0)
			length = ret;
		return length;
	}

	ret = scnprintf(buf, buf_avail_len,
			"\nMC ADDR LIST DETAILS (mc_cnt = %u)\n",
			mc_addr_list.mc_cnt);
	if (ret <= 0)
		return length;
	length += ret;

	for (i = 0; i < mc_addr_list.mc_cnt; i++) {
		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			return buf_avail_len;
		}

		ret = scnprintf(buf + length, buf_avail_len - length,
				QDF_FULL_MAC_FMT "\n",
				QDF_FULL_MAC_REF(mc_addr_list.mc_addr[i].bytes));
		if (ret <= 0)
			return length;
		length += ret;
	}

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	ret = scnprintf(buf + length, buf_avail_len - length,
			"mc_filter_applied = %u\n",
			mc_addr_list.is_filter_applied);
	if (ret <= 0)
		return length;

	length += ret;
	return length;
}

/**
 * wlan_hdd_arp_offload_info_debugfs() - Populate arp offload info
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 * @buf: output buffer to hold arp offload info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_arp_offload_info_debugfs(struct hdd_context *hdd_ctx,
				  struct hdd_adapter *adapter, uint8_t *buf,
				  ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret_val;
	struct pmo_arp_offload_params info = {0};
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return 0;

	status = ucfg_pmo_get_arp_offload_params(vdev, &info);
	hdd_objmgr_put_vdev(vdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		ret_val = scnprintf(buf, buf_avail_len,
				    "\nARP OFFLOAD QUERY FAILED\n");
		goto len_adj;
	}

	if (!info.is_offload_applied)
		ret_val = scnprintf(buf, buf_avail_len,
				    "\nARP OFFLOAD: DISABLED\n");
	else
		ret_val = scnprintf(buf, buf_avail_len,
				    "\nARP OFFLOAD: ENABLED (%u.%u.%u.%u)\n",
				    info.host_ipv4_addr[0],
				    info.host_ipv4_addr[1],
				    info.host_ipv4_addr[2],
				    info.host_ipv4_addr[3]);
len_adj:
	if (ret_val <= 0)
		return length;
	length = ret_val;

	return length;
}

#ifdef WLAN_NS_OFFLOAD
/**
 * ipv6_addr_string() - Get IPv6 addr string from array of octets
 * @buffer: output buffer to hold string, caller should ensure size of
 *          buffer is atleast IPV6_MAC_ADDRESS_STR_LEN
 * @ipv6_addr: IPv6 address array
 *
 * Return: None
 */
static void ipv6_addr_string(uint8_t *buffer, uint8_t *ipv6_addr)
{
	uint8_t *a = ipv6_addr;

	scnprintf(buffer, IPV6_MAC_ADDRESS_STR_LEN,
		  "%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x::%02x%02x",
		  (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6],
		  (a)[7], (a)[8], (a)[9], (a)[10], (a)[11], (a)[12], (a)[13],
		  (a)[14], (a)[15]);
}

/**
 * hdd_ipv6_scope_str() - Get string for PMO NS (IPv6) Addr scope
 * @scope: scope id from enum pmo_ns_addr_scope
 *
 * Return: Meaningful string for enum pmo_ns_addr_scope
 */
static uint8_t *hdd_ipv6_scope_str(enum pmo_ns_addr_scope scope)
{
	switch (scope) {
	case PMO_NS_ADDR_SCOPE_NODELOCAL:
		return "Node Local";
	case PMO_NS_ADDR_SCOPE_LINKLOCAL:
		return "Link Local";
	case PMO_NS_ADDR_SCOPE_SITELOCAL:
		return "Site Local";
	case PMO_NS_ADDR_SCOPE_ORGLOCAL:
		return "Org Local";
	case PMO_NS_ADDR_SCOPE_GLOBAL:
		return "Global";
	default:
		return "Invalid";
	}
}

/**
 * wlan_hdd_ns_offload_info_debugfs() - Populate ns offload info
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 * @buf: output buffer to hold ns offload info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_ns_offload_info_debugfs(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *adapter, uint8_t *buf,
				 ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret;
	struct pmo_ns_offload_params info = {0};
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;
	uint32_t i;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return 0;

	status = ucfg_pmo_get_ns_offload_params(vdev, &info);
	hdd_objmgr_put_vdev(vdev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		ret = scnprintf(buf, buf_avail_len,
				"\nNS OFFLOAD QUERY FAILED\n");
		if (ret <= 0)
			return length;
		length += ret;

		return length;
	}

	ret = scnprintf(buf, buf_avail_len,
			"\nNS OFFLOAD DETAILS\n");
	if (ret <= 0)
		return length;
	length += ret;

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	if (!info.is_offload_applied) {
		ret = scnprintf(buf + length, buf_avail_len - length,
				"NS offload is not enabled\n");
		if (ret <= 0)
			return length;
		length += ret;

		return length;
	}

	ret = scnprintf(buf + length, buf_avail_len - length,
			"NS offload enabled, %u ns addresses offloaded\n",
			info.num_ns_offload_count);
	if (ret <= 0)
		return length;
	length += ret;

	for (i = 0; i < info.num_ns_offload_count; i++) {
		uint8_t ipv6_str[IPV6_MAC_ADDRESS_STR_LEN];
		uint8_t cast_string[12];
		uint8_t *scope_string;

		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			return buf_avail_len;
		}

		ipv6_addr_string(ipv6_str, info.target_ipv6_addr[i]);
		scope_string = hdd_ipv6_scope_str(info.scope[i]);

		if (info.target_ipv6_addr_ac_type[i] ==
		    PMO_IPV6_ADDR_AC_TYPE)
			strlcpy(cast_string, "(ANY CAST)", 12);
		else
			strlcpy(cast_string, "(UNI CAST)", 12);

		ret = scnprintf(buf + length, buf_avail_len - length,
				"%u. %s %s and scope is: %s\n",
				(i + 1), ipv6_str, cast_string,
				scope_string);
		if (ret <= 0)
			return length;
		length += ret;
	}

	return length;
}
#else
/**
 * wlan_hdd_ns_offload_info_debugfs() - Populate ns offload info
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 * @buf: output buffer to hold ns offload info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_ns_offload_info_debugfs(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *adapter, uint8_t *buf,
				 ssize_t buf_avail_len)
{
	return 0;
}
#endif

/**
 * wlan_hdd_apf_info_debugfs() - Populate apf offload info
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 * @buf: output buffer to hold apf offload info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
wlan_hdd_apf_info_debugfs(struct hdd_context *hdd_ctx,
			  struct hdd_adapter *adapter, uint8_t *buf,
			  ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret_val;
	bool apf_enabled;

	if (hdd_ctx->apf_version > 2)
		apf_enabled = adapter->apf_context.apf_enabled;
	else
		apf_enabled = hdd_ctx->apf_enabled_v2;

	ret_val = scnprintf(buf, buf_avail_len,
			    "\nAPF OFFLOAD DETAILS, offload_applied: %u\n\n",
			    apf_enabled);
	if (ret_val <= 0)
		return length;
	length = ret_val;

	return length;
}

ssize_t
wlan_hdd_debugfs_update_filters_info(struct hdd_context *hdd_ctx,
				     struct hdd_adapter *adapter,
				     uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t len;
	int ret_val;
	struct hdd_station_ctx *hdd_sta_ctx;

	hdd_enter();

	len = wlan_hdd_current_time_info_debugfs(buf, buf_avail_len);

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}

	if (adapter->device_mode != QDF_STA_MODE) {
		ret_val = scnprintf(buf + len, buf_avail_len - len,
				    "Interface is not operating in STA mode\n");
		if (ret_val <= 0)
			return len;

		len += ret_val;
		return len;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		ret_val = scnprintf(buf + len, buf_avail_len - len,
				    "\nSTA is not connected\n");
		if (ret_val <= 0)
			return len;

		len += ret_val;
		return len;
	}

	len += wlan_hdd_mc_addr_list_info_debugfs(hdd_ctx, adapter, buf + len,
						  buf_avail_len - len);

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	len += wlan_hdd_arp_offload_info_debugfs(hdd_ctx, adapter, buf + len,
						 buf_avail_len - len);

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	len += wlan_hdd_ns_offload_info_debugfs(hdd_ctx, adapter, buf + len,
						buf_avail_len - len);

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	len += wlan_hdd_apf_info_debugfs(hdd_ctx, adapter, buf + len,
					 buf_avail_len - len);

	hdd_exit();

	return len;
}
