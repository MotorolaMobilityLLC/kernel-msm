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
 * DOC: wlan_hdd_sysfs_dl_modes.c
 *
 * implementation for creating sysfs file dl_modes
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_main.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_dl_modes.h>
#include "wmi_unified_param.h"
#include "wma_api.h"

static int hdd_sysfs_set_dbg(struct hdd_adapter *adapter,
			     int id,
			     const char *id_string,
			     int value)
{
	int errno;

	hdd_debug("%s %d", id_string, value);
	errno = wma_cli_set_command(adapter->vdev_id, id, value, DBG_CMD);
	if (errno)
		hdd_err("Failed to set firmware, errno %d", errno);

	return errno;
}

#define hdd_sysfs_set_dbg(adapter, id, value) \
			  hdd_sysfs_set_dbg(adapter, id, #id, value)

static ssize_t
__hdd_sysfs_dl_loglevel_store(struct net_device *net_dev,
			      char const *buf, size_t count)
{
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
	hdd_debug("dl_loglevel: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_LOG_LEVEL, value);

	if (ret) {
		hdd_err_rl("failed dl_loglevel: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_loglevel_store(struct device *dev,
			    struct device_attribute *attr,
			    char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_loglevel_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_mod_loglevel_store(struct net_device *net_dev,
				  char const *buf, size_t count)
{
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
	hdd_debug("dl_mod_loglevel: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_MOD_LOG_LEVEL, value);

	if (ret) {
		hdd_err_rl("failed dl_mod_loglevel: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_mod_loglevel_store(struct device *dev,
				struct device_attribute *attr,
				char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_mod_loglevel_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_modoff_store(struct net_device *net_dev,
			    char const *buf, size_t count)
{
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
	hdd_debug("dl_modoff: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_MODULE_DISABLE, value);

	if (ret) {
		hdd_err_rl("failed dl_modoff: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_modoff_store(struct device *dev,
			  struct device_attribute *attr,
			  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_modoff_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_modon_store(struct net_device *net_dev,
			   char const *buf, size_t count)
{
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
	hdd_debug("dl_modon: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_MODULE_ENABLE, value);

	if (ret) {
		hdd_err_rl("failed dl_modon: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_modon_store(struct device *dev,
			 struct device_attribute *attr,
			 char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_modon_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_report_store(struct net_device *net_dev,
			    char const *buf, size_t count)
{
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
	hdd_debug("dl_report: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_REPORT_ENABLE, value);

	if (ret) {
		hdd_err_rl("failed dl_report: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_report_store(struct device *dev,
			  struct device_attribute *attr,
			  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_report_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_type_store(struct net_device *net_dev,
			  char const *buf, size_t count)
{
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
	hdd_debug("dl_type: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_TYPE, value);

	if (ret) {
		hdd_err_rl("failed dl_type: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_type_store(struct device *dev,
			struct device_attribute *attr,
			char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_type_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_vapoff_store(struct net_device *net_dev,
			    char const *buf, size_t count)
{
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
	hdd_debug("dl_vapoff: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_VAP_DISABLE, value);

	if (ret) {
		hdd_err_rl("failed dl_vapoff: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_vapoff_store(struct device *dev,
			  struct device_attribute *attr,
			  char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_vapoff_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_dl_vapon_store(struct net_device *net_dev,
			   char const *buf, size_t count)
{
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
	hdd_debug("dl_vapon: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_dbg(adapter, WMI_DBGLOG_VAP_ENABLE, value);

	if (ret) {
		hdd_err_rl("failed dl_vapon: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_dl_vapon_store(struct device *dev,
			 struct device_attribute *attr,
			 char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_dl_vapon_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(dl_loglevel, 0220,
		   NULL, hdd_sysfs_dl_loglevel_store);

static DEVICE_ATTR(dl_mod_loglevel, 0220,
		   NULL, hdd_sysfs_dl_mod_loglevel_store);

static DEVICE_ATTR(dl_modoff, 0220,
		   NULL, hdd_sysfs_dl_modoff_store);

static DEVICE_ATTR(dl_modon, 0220,
		   NULL, hdd_sysfs_dl_modon_store);

static DEVICE_ATTR(dl_report, 0220,
		   NULL, hdd_sysfs_dl_report_store);

static DEVICE_ATTR(dl_type, 0220,
		   NULL, hdd_sysfs_dl_type_store);

static DEVICE_ATTR(dl_vapoff, 0220,
		   NULL, hdd_sysfs_dl_vapoff_store);

static DEVICE_ATTR(dl_vapon, 0220,
		   NULL, hdd_sysfs_dl_vapon_store);

static int hdd_sysfs_dl_loglevel_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_loglevel);
	if (error)
		hdd_err("could not create dl_loglevel sysfs file");

	return error;
}

static void hdd_sysfs_dl_loglevel_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_loglevel);
}

static int hdd_sysfs_dl_mod_loglevel_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_dl_mod_loglevel);
	if (error)
		hdd_err("could not create dl_mod_loglevel sysfs file");

	return error;
}

static void hdd_sysfs_dl_mod_loglevel_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_mod_loglevel);
}

static int hdd_sysfs_dl_modoff_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_modoff);
	if (error)
		hdd_err("could not create dl_modoff sysfs file");

	return error;
}

static void hdd_sysfs_dl_modoff_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_modoff);
}

static int hdd_sysfs_dl_modon_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_modon);
	if (error)
		hdd_err("could not create dl_modon sysfs file");

	return error;
}

static void hdd_sysfs_dl_modon_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_modon);
}

static int hdd_sysfs_dl_report_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_report);
	if (error)
		hdd_err("could not create dl_report sysfs file");

	return error;
}

static void hdd_sysfs_dl_report_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_report);
}

static int hdd_sysfs_dl_type_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_type);
	if (error)
		hdd_err("could not create dl_type sysfs file");

	return error;
}

static void hdd_sysfs_dl_type_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_type);
}

static int hdd_sysfs_dl_vapoff_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_vapoff);
	if (error)
		hdd_err("could not create dl_vapoff sysfs file");

	return error;
}

static void hdd_sysfs_dl_vapoff_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_vapoff);
}

static int hdd_sysfs_dl_vapon_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_dl_vapon);
	if (error)
		hdd_err("could not create dl_vapon sysfs file");

	return error;
}

static void hdd_sysfs_dl_vapon_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_dl_vapon);
}

void hdd_sysfs_dl_modes_create(struct hdd_adapter *adapter)
{
	hdd_sysfs_dl_loglevel_create(adapter);
	hdd_sysfs_dl_mod_loglevel_create(adapter);
	hdd_sysfs_dl_modoff_create(adapter);
	hdd_sysfs_dl_modon_create(adapter);
	hdd_sysfs_dl_report_create(adapter);
	hdd_sysfs_dl_type_create(adapter);
	hdd_sysfs_dl_vapoff_create(adapter);
	hdd_sysfs_dl_vapon_create(adapter);
}

void hdd_sysfs_dl_modes_destroy(struct hdd_adapter *adapter)
{
	hdd_sysfs_dl_vapon_destroy(adapter);
	hdd_sysfs_dl_vapoff_destroy(adapter);
	hdd_sysfs_dl_type_destroy(adapter);
	hdd_sysfs_dl_report_destroy(adapter);
	hdd_sysfs_dl_modon_destroy(adapter);
	hdd_sysfs_dl_modoff_destroy(adapter);
	hdd_sysfs_dl_mod_loglevel_destroy(adapter);
	hdd_sysfs_dl_loglevel_destroy(adapter);
}
