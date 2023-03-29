/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

#include <wlan_hdd_includes.h>
#include <wlan_hdd_sysfs.h>
#include <osif_psoc_sync.h>
#include <wlan_hdd_sysfs_swlm.h>

static ssize_t
__hdd_sysfs_dp_swlm_show(struct hdd_context *hdd_ctx,
			 struct kobj_attribute *attr, char *buf)
{
	ol_txrx_soc_handle soc_hdl = cds_get_context(QDF_MODULE_ID_SOC);

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "dp_swlm enable: %d\n",
			 cdp_soc_is_swlm_enabled(soc_hdl));
}

static ssize_t hdd_sysfs_dp_swlm_show(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      char *buf)
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

	errno_size = __hdd_sysfs_dp_swlm_show(hdd_ctx, attr, buf);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dp_swlm_store(struct hdd_context *hdd_ctx,
			  struct kobj_attribute *attr, const char *buf,
			  size_t count)
{
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	uint32_t value;
	int ret;
	ol_txrx_soc_handle dp_soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!wlan_hdd_validate_modules_state(hdd_ctx) || !dp_soc)
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	hdd_debug("dp_swlm: %d", value);

	cdp_soc_set_swlm_enable(dp_soc, value);

	return count;
}

static ssize_t
hdd_sysfs_dp_swlm_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			char const *buf, size_t count)
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

	errno_size = __hdd_sysfs_dp_swlm_store(hdd_ctx, attr,
					       buf, count);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static struct kobj_attribute dp_swlm_attribute =
	__ATTR(dp_swlm, 0664, hdd_sysfs_dp_swlm_show,
	       hdd_sysfs_dp_swlm_store);

int hdd_sysfs_dp_swlm_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &dp_swlm_attribute.attr);
	if (error)
		hdd_err("could not create dp_swlm sysfs file");

	return error;
}

void hdd_sysfs_dp_swlm_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}

	sysfs_remove_file(driver_kobject, &dp_swlm_attribute.attr);
}
