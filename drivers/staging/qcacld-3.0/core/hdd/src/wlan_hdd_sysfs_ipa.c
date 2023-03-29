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

#include <wlan_hdd_main.h>
#include <wlan_hdd_sysfs_ipa.h>
#include <wlan_ipa_ucfg_api.h>
#include <wlan_hdd_sysfs.h>
#include "osif_sync.h"

#define MAX_USER_COMMAND_SIZE_IPAUCSTAT 4

static ssize_t __hdd_sysfs_ipaucstate_store(struct net_device *net_dev,
					    const char __user *buf,
					    size_t count)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	uint8_t cmd[MAX_USER_COMMAND_SIZE_IPAUCSTAT] = {0};
	int ret;
	char *sptr, *token;
	uint8_t set_value = 0;

	hdd_enter();

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (!ucfg_ipa_is_enabled())
		return -EINVAL;

	if (adapter->device_mode != QDF_SAP_MODE)
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(cmd, sizeof(cmd),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = cmd;
	/* Get set_value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou8(token, 0, &set_value))
		return -EINVAL;

	/* If input value is non-zero get stats */
	switch (set_value) {
	case IPA_UC_STAT:
		ucfg_ipa_uc_stat(hdd_ctx->pdev);
		break;
	case IPA_UC_INFO:
		ucfg_ipa_uc_info(hdd_ctx->pdev);
		break;
	case IPA_UC_RT_DEBUG_HOST_DUMP:
		ucfg_ipa_uc_rt_debug_host_dump(hdd_ctx->pdev);
		break;
	case IPA_DUMP_INFO:
		ucfg_ipa_dump_info(hdd_ctx->pdev);
		break;
	default:
		/* place holder for stats clean up
		 * Stats clean not implemented yet on FW and IPA
		 */
		break;
	}
	hdd_exit();
	return count;
}

static ssize_t hdd_sysfs_ipaucstate_store(struct device *dev,
					  struct device_attribute *attr,
					  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_ipaucstate_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);
	return err_size;
}

static DEVICE_ATTR(ipaucstat, 0220,
		   NULL, hdd_sysfs_ipaucstate_store);

void hdd_sysfs_ipa_create(struct hdd_adapter *adapter)
{
	if (device_create_file(&adapter->dev->dev, &dev_attr_ipaucstat))
		hdd_err("could not create wlan sysfs file");
}

void hdd_sysfs_ipa_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_ipaucstat);
}
