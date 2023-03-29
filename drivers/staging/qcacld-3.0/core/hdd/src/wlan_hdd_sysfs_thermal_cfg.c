/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_thermal_cfg.c
 *
 * implementation for creating sysfs file thermal_cfg
 */

#include <wlan_hdd_includes.h>
#include "osif_psoc_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_thermal_cfg.h>
#include "qdf_trace.h"
#include "sme_api.h"
#include "qdf_status.h"
#include <wlan_fw_offload_main.h>
#include "wlan_hdd_thermal.h"
#include <wlan_fwol_ucfg_api.h>

#ifdef FW_THERMAL_THROTTLE_SUPPORT
#ifndef QCN7605_SUPPORT
static QDF_STATUS hdd_send_thermal_mgmt_cmd(mac_handle_t mac_handle,
					    uint16_t lower_thresh_deg,
					    uint16_t higher_thresh_deg)
{
	return sme_set_thermal_mgmt(mac_handle, lower_thresh_deg,
				    higher_thresh_deg);
}
#else
static QDF_STATUS hdd_send_thermal_mgmt_cmd(mac_handle_t mac_handle,
					    uint16_t lower_thresh_deg,
					    uint16_t higher_thresh_deg)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif /* FW_THERMAL_THROTTLE_SUPPORT */

static ssize_t
__hdd_sysfs_thermal_cfg_store(struct hdd_context *hdd_ctx,
			      struct kobj_attribute *attr,
			      const char *buf,
			      size_t count)
{
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	uint32_t val1, val2, val3, val4, val7;
	uint16_t val5, val6;
	QDF_STATUS status;
	int ret;
	struct thermal_mitigation_params therm_cfg_params;
	struct wlan_fwol_thermal_temp thermal_temp = {0};

	status = ucfg_fwol_get_thermal_temp(hdd_ctx->psoc, &thermal_temp);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("Failed to get fwol thermal obj");
		return status;
	}

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	hdd_debug("thermal_cfg: count %zu buf_local:(%s)",
		  count, buf_local);

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

	/* Get val3 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val3))
		return -EINVAL;

	/* Get val4 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val4))
		return -EINVAL;

	/* Get val5 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou16(token, 0, &val5))
		return -EINVAL;

	/* Get val6 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou16(token, 0, &val6))
		return -EINVAL;

	/* Get val7 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val7))
		return -EINVAL;

	/* Check for valid inputs */
	if (val1 < 0 || val1 > 1 || val2 < 0 || val3 < 0 || val3 > 100 ||
	    val4 < 0 || val4 > 3 ||  val5 < 0 || val6 < 0 || val7 < 0 ||
	    val6 <= val5)
		return -EINVAL;

	therm_cfg_params.enable = val1;
	therm_cfg_params.dc = val2;
	therm_cfg_params.levelconf[0].dcoffpercent = val3;
	therm_cfg_params.levelconf[0].priority = val4;
	therm_cfg_params.levelconf[0].tmplwm = val7;
	hdd_thermal_fill_clientid_priority(THERMAL_MONITOR_APPS,
					   thermal_temp.priority_apps,
					   thermal_temp.priority_wpps,
					   &therm_cfg_params);

	status = sme_set_thermal_throttle_cfg(hdd_ctx->mac_handle,
					      &therm_cfg_params);

	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	if (!val7) {
		status = hdd_send_thermal_mgmt_cmd(hdd_ctx->mac_handle,
						   val5, val6);

		if (QDF_IS_STATUS_ERROR(status))
			return qdf_status_to_os_return(status);
	}

	return count;
}

static ssize_t
hdd_sysfs_thermal_cfg_store(struct kobject *kobj,
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

	errno_size = __hdd_sysfs_thermal_cfg_store(hdd_ctx, attr,
						   buf, count);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static struct kobj_attribute thermal_cfg_attribute =
	__ATTR(thermal_cfg, 0220, NULL,
	       hdd_sysfs_thermal_cfg_store);

int hdd_sysfs_thermal_cfg_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &thermal_cfg_attribute.attr);
	if (error)
		hdd_err("could not create thermal_cfg sysfs file");

	return error;
}

void
hdd_sysfs_thermal_cfg_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}
	sysfs_remove_file(driver_kobject, &thermal_cfg_attribute.attr);
}
