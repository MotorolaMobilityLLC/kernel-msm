/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_unit_test.c
 *
 * WLAN Host Device Driver implementation to create sysfs
 * unit_test_target
 */
#include "wlan_hdd_main.h"
#include "osif_psoc_sync.h"
#include "osif_vdev_sync.h"
#include "wlan_dsc_test.h"
#include "wlan_hdd_sysfs.h"
#include "wlan_hdd_sysfs_unit_test.h"
#include "wlan_module_ids.h"
#include "wma.h"

#define MAX_USER_COMMAND_SIZE_UNIT_TEST_TARGET 256

static ssize_t __hdd_sysfs_unit_test_target_store(
		struct net_device *net_dev,
		const char __user *buf, size_t count)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	char buf_local[MAX_USER_COMMAND_SIZE_UNIT_TEST_TARGET + 1];
	char *sptr, *token;
	uint32_t apps_args[WMA_MAX_NUM_ARGS];
	int module_id, args_num, ret, i;
	QDF_STATUS status;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (!hdd_ctx->config->is_unit_test_framework_enabled) {
		hdd_warn_rl("UT framework is disabled");
		return -EINVAL;
	}

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	hdd_nofl_info("unit_test_target: count %zu buf_local:(%s) net_devname %s",
		      count, buf_local, net_dev->name);

	sptr = buf_local;
	/* Get module_id */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &module_id))
		return -EINVAL;

	/* Get args_num */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &args_num))
		return -EINVAL;

	if (module_id < WLAN_MODULE_ID_MIN ||
	    module_id >= WLAN_MODULE_ID_MAX) {
		hdd_err_rl("Invalid MODULE ID %d", module_id);
		return -EINVAL;
	}
	if (args_num > WMA_MAX_NUM_ARGS) {
		hdd_err_rl("Too many args %d", args_num);
		return -EINVAL;
	}

	for (i = 0; i < args_num; i++) {
		token = strsep(&sptr, " ");
		if (!token) {
			hdd_err_rl("not enough args(%d), expected args_num:%d",
				   i, args_num);
			return -EINVAL;
		}
		if (kstrtou32(token, 0, &apps_args[i]))
			return -EINVAL;
	}

	status = sme_send_unit_test_cmd(adapter->vdev_id,
					module_id,
					args_num,
					&apps_args[0]);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err_rl("sme_send_unit_test_cmd returned %d", status);
		return -EINVAL;
	}

	return count;
}

static ssize_t hdd_sysfs_unit_test_target_store(struct device *dev,
						struct device_attribute *attr,
						char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_unit_test_target_store(
				net_dev, buf, count);
	if (errno_size < 0)
		hdd_err_rl("errno_size %zd", errno_size);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(unit_test_target, 0220,
		   NULL, hdd_sysfs_unit_test_target_store);

int hdd_sysfs_unit_test_target_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_unit_test_target);
	if (error)
		hdd_err("could not create unit_test_target sysfs file");

	return error;
}

void hdd_sysfs_unit_test_target_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_unit_test_target);
}

