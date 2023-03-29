/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_fw_mode_config.c
 *
 * Implementation for creating sysfs file fw_mode_config
 */

#include <wlan_hdd_includes.h>
#include "osif_psoc_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_fw_mode_config.h>
#include "wlan_policy_mgr_ucfg.h"

static ssize_t
__wlan_hdd_store_fw_mode_config_sysfs(struct hdd_context *hdd_ctx,
				      struct kobj_attribute *attr,
				       const char *buf,
				       size_t count)
{
	uint8_t dual_mac_feature = DISABLE_DBS_CXN_AND_SCAN;
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	uint32_t val1, val2;
	QDF_STATUS status;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	hdd_debug("set_fw_mode_cfg: count %zu buf_local:(%s)",
		  count, buf_local);

	sptr = buf_local;
	/* Get val1 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val1))
		return -EINVAL;

	/* Get val2 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val2))
		return -EINVAL;

	hdd_debug("Sysfs to set dual fw mode config");
	status = ucfg_policy_mgr_get_dual_mac_feature(hdd_ctx->psoc,
						      &dual_mac_feature);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err_rl("can't get dual mac feature val, use def");
	if (dual_mac_feature == DISABLE_DBS_CXN_AND_SCAN) {
		hdd_err_rl("Dual mac feature is disabled from INI");
		return -EPERM;
	}
	hdd_debug("%d %d", val1, val2);
	policy_mgr_set_dual_mac_fw_mode_config(hdd_ctx->psoc,
					       val1, val2);

	return count;
}

static ssize_t
wlan_hdd_store_fw_mode_config_sysfs(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf,
				    size_t count)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	ssize_t errno_size;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	errno_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					     &psoc_sync);
	if (errno_size)
		return errno_size;

	errno_size = __wlan_hdd_store_fw_mode_config_sysfs(hdd_ctx, attr,
							   buf, count);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static struct kobj_attribute set_fw_mode_cfg_attribute =
	__ATTR(fw_mode_config, 0220, NULL,
	       wlan_hdd_store_fw_mode_config_sysfs);

int hdd_sysfs_fw_mode_config_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &set_fw_mode_cfg_attribute.attr);
	if (error)
		hdd_err("could not create fw_mode_config sysfs file");

	return error;
}

void
hdd_sysfs_fw_mode_config_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}
	sysfs_remove_file(driver_kobject, &set_fw_mode_cfg_attribute.attr);
}
