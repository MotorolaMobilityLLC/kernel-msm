/*
 * Copyright (c) 2011-2020, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_modify_acl.c
 *
 * implementation for creating sysfs file modify_acl
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_sysfs.h"
#include "wlan_hdd_sysfs_modify_acl.h"

#define MAX_USER_COMMAND_SIZE_MODIFY_ACL 64

static ssize_t __hdd_sysfs_modify_acl_store(
		struct net_device *net_dev,
		const char *buf, size_t count)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	char buf_local[MAX_USER_COMMAND_SIZE_MODIFY_ACL + 1];
	char *sptr, *token;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	eSapACLType list_type;
	eSapACLCmdType cmd_type;
	int ret, i;
	QDF_STATUS qdf_status;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (adapter->device_mode != QDF_SAP_MODE) {
		hdd_err_rl("Command only allowed in sap mode");
		return -EINVAL;
	}

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	hdd_debug("modify_acl: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	sptr = buf_local;
	for (i = 0; i < QDF_MAC_ADDR_SIZE; i++) {
		token = strsep(&sptr, " ");
		if (!token)
			return -EINVAL;
		if (kstrtou8(token, 0, &peer_mac[i]))
			return -EINVAL;
	}

	/* Get list_type */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &list_type))
		return -EINVAL;

	/* Get cmd_type */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &cmd_type))
		return -EINVAL;

	hdd_debug("Modify ACL mac:" QDF_MAC_ADDR_FMT " type: %d cmd: %d",
		  QDF_MAC_ADDR_REF(peer_mac), list_type, cmd_type);

	qdf_status = wlansap_modify_acl(
		WLAN_HDD_GET_SAP_CTX_PTR(adapter),
		peer_mac, list_type, cmd_type);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Modify ACL failed");
		return -EIO;
	}

	return count;
}

static ssize_t hdd_sysfs_modify_acl_store(struct device *dev,
					  struct device_attribute *attr,
					  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_modify_acl_store(
				net_dev, buf, count);
	if (errno_size < 0)
		hdd_err_rl("errno_size %zd", errno_size);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(modify_acl, 0220,
		   NULL, hdd_sysfs_modify_acl_store);

int hdd_sysfs_modify_acl_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_modify_acl);
	if (error)
		hdd_err("could not create modify_acl sysfs file");

	return error;
}

void hdd_sysfs_modify_acl_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_modify_acl);
}
