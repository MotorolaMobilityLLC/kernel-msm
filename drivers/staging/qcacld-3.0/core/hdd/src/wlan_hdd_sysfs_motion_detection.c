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
 * DOC: wlan_hdd_sysfs_motion_detection.c
 *
 * implementation for creating sysfs file motion_detection
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_main.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_motion_detection.h>
#include "sme_api.h"

#define HDD_SYSFS_MT_CONFIG_UINT32_ARGS (10)
#define HDD_SYSFS_MT_CONFIG_UINT8_ARGS (5)
#define HDD_SYSFS_MT_CONFIG_NUM_ARGS (15)
#define HDD_SYSFS_MT_CONFIG_UINT8_INDEX (11)
#define MAX_SYSFS_MT_USER_COMMAND_SIZE_LENGTH (64)

static ssize_t
__hdd_sysfs_mt_bl_config_store(struct net_device *net_dev,
			       char const *buf, size_t count)
{
	struct sme_motion_det_base_line_cfg motion_det_base_line_cfg;
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	uint32_t bl_time_t, bl_packet_gap, bl_n, bl_num_meas;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
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

	sptr = buf_local;
	hdd_debug("mt_bl_config: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get val1 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &bl_time_t))
		return -EINVAL;

	/* Get val2 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &bl_packet_gap))
		return -EINVAL;

	/* Get val3 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &bl_n))
		return -EINVAL;

	/* Get val4 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &bl_num_meas))
		return -EINVAL;

	motion_det_base_line_cfg.vdev_id = adapter->vdev_id;
	motion_det_base_line_cfg.bl_time_t = bl_time_t;
	motion_det_base_line_cfg.bl_packet_gap = bl_packet_gap;
	motion_det_base_line_cfg.bl_n = bl_n;
	motion_det_base_line_cfg.bl_num_meas = bl_num_meas;
	sme_motion_det_base_line_config(hdd_ctx->mac_handle,
					&motion_det_base_line_cfg);

	return count;
}

static ssize_t
hdd_sysfs_mt_bl_config_store(struct device *dev,
			     struct device_attribute *attr,
			     char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_mt_bl_config_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_mt_bl_start_store(struct net_device *net_dev,
			      char const *buf, size_t count)
{
	struct sme_motion_det_base_line_en motion_det_base_line;
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	uint32_t value;
	QDF_STATUS status;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
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

	/* Do not send baselining start/stop during motion detection phase */
	if (adapter->motion_det_in_progress) {
		hdd_err("Motion detection still in progress, try later");
		return -EAGAIN;
	}

	sptr = buf_local;
	hdd_debug("mt_bl_start: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	if (value < 0 || value > 1) {
		hdd_err("Invalid value %d in mt_bl_start", value);
		return -EINVAL;
	}

	motion_det_base_line.vdev_id = adapter->vdev_id;
	motion_det_base_line.enable = value;
	status = sme_motion_det_base_line_enable(hdd_ctx->mac_handle,
						 &motion_det_base_line);

	if (status != QDF_STATUS_SUCCESS)
		hdd_err("can't enable md baselining msg to scheduler");

	return count;
}

static ssize_t
hdd_sysfs_mt_bl_start_store(struct device *dev,
			    struct device_attribute *attr,
			    char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_mt_bl_start_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_mt_config_store(struct net_device *net_dev,
			    char const *buf, size_t count)
{
	struct sme_motion_det_cfg motion_det_cfg;
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_MT_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	uint8_t val_8[HDD_SYSFS_MT_CONFIG_UINT8_ARGS];
	uint32_t val_32[HDD_SYSFS_MT_CONFIG_UINT32_ARGS];
	int i, ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
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

	sptr = buf_local;
	hdd_debug("mt_config: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	for (i = 0; i < HDD_SYSFS_MT_CONFIG_NUM_ARGS; i++) {
		token = strsep(&sptr, " ");
		if (!token) {
			hdd_err_rl("mt_config: not enough args(%d), expected args_num: 15",
				   i);
			return -EINVAL;
		}
		if ( i >= HDD_SYSFS_MT_CONFIG_UINT8_INDEX) {
			if (kstrtou8(token, 0,
				     &val_8[i - HDD_SYSFS_MT_CONFIG_UINT8_INDEX]))
				return -EINVAL;
		} else {
			if (kstrtou32(token, 0, &val_32[i]))
				return -EINVAL;
		}
	}

	motion_det_cfg.vdev_id = adapter->vdev_id;
	motion_det_cfg.time_t1 = val_32[0];
	motion_det_cfg.time_t2 = val_32[1];
	motion_det_cfg.n1 = val_32[2];
	motion_det_cfg.n2 = val_32[3];
	motion_det_cfg.time_t1_gap = val_32[4];
	motion_det_cfg.time_t2_gap = val_32[5];
	motion_det_cfg.coarse_K = val_32[6];
	motion_det_cfg.fine_K = val_32[7];
	motion_det_cfg.coarse_Q = val_32[8];
	motion_det_cfg.fine_Q = val_32[9];
	motion_det_cfg.md_coarse_thr_high = val_8[0];
	motion_det_cfg.md_fine_thr_high = val_8[1];
	motion_det_cfg.md_coarse_thr_low = val_8[2];
	motion_det_cfg.md_fine_thr_low = val_8[3];
	adapter->motion_detection_mode = val_8[4];
	sme_motion_det_config(hdd_ctx->mac_handle, &motion_det_cfg);
	adapter->motion_det_cfg =  true;

	return count;
}

static ssize_t
hdd_sysfs_mt_config_store(struct device *dev,
			  struct device_attribute *attr,
			  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_mt_config_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_mt_start_store(struct net_device *net_dev,
			   char const *buf, size_t count)
{
	struct sme_motion_det_en motion_det;
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	uint32_t value;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
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

	sptr = buf_local;
	hdd_debug("mt_start: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	if (value < 0 || value > 1) {
		hdd_err("Invalid value %d in mt_start", value);
		return -EINVAL;
	}

	if (!adapter->motion_det_cfg) {
		hdd_err("Motion Detection config values not availbale");
		return -EINVAL;
	}

	if (!adapter->motion_det_baseline_value) {
		hdd_err("Motion Detection Baselining not started/completed");
		return -EAGAIN;
	}

	motion_det.vdev_id = adapter->vdev_id;
	motion_det.enable = value;

	if (value) {
		/* For motion detection start, set motion_det_in_progress */
		adapter->motion_det_in_progress = true;
	} else {
		/* For motion detection stop, reset motion_det_in_progress */
		adapter->motion_det_in_progress = false;
		adapter->motion_detection_mode = 0;
	}

	sme_motion_det_enable(hdd_ctx->mac_handle, &motion_det);

	return count;
}

static ssize_t
hdd_sysfs_mt_start_store(struct device *dev,
			 struct device_attribute *attr,
			 char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_mt_start_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(mt_bl_config, 0220,
		   NULL, hdd_sysfs_mt_bl_config_store);

static DEVICE_ATTR(mt_bl_start, 0220,
		   NULL, hdd_sysfs_mt_bl_start_store);

static DEVICE_ATTR(mt_config, 0220,
		   NULL, hdd_sysfs_mt_config_store);

static DEVICE_ATTR(mt_start, 0220,
		   NULL, hdd_sysfs_mt_start_store);

static int hdd_sysfs_mt_bl_config_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_mt_bl_config);
	if (error)
		hdd_err("could not create mt_bl_config sysfs file");

	return error;
}

static void hdd_sysfs_mt_bl_config_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_mt_bl_config);
}

static int hdd_sysfs_mt_bl_start_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_mt_bl_start);
	if (error)
		hdd_err("could not create mt_bl_start sysfs file");

	return error;
}

static void hdd_sysfs_mt_bl_start_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_mt_bl_start);
}

static int hdd_sysfs_mt_config_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_mt_config);
	if (error)
		hdd_err("could not create mt_config sysfs file");

	return error;
}

static void hdd_sysfs_mt_config_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_mt_config);
}

static int hdd_sysfs_mt_start_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_mt_start);
	if (error)
		hdd_err("could not create mt_start sysfs file");

	return error;
}

static void hdd_sysfs_mt_start_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_mt_start);
}

void hdd_sysfs_motion_detection_create(struct hdd_adapter *adapter)
{
	hdd_sysfs_mt_bl_config_create(adapter);
	hdd_sysfs_mt_bl_start_create(adapter);
	hdd_sysfs_mt_config_create(adapter);
	hdd_sysfs_mt_start_create(adapter);
}

void hdd_sysfs_motion_detection_destroy(struct hdd_adapter *adapter)
{
	hdd_sysfs_mt_start_destroy(adapter);
	hdd_sysfs_mt_config_destroy(adapter);
	hdd_sysfs_mt_bl_start_destroy(adapter);
	hdd_sysfs_mt_bl_config_destroy(adapter);
}
