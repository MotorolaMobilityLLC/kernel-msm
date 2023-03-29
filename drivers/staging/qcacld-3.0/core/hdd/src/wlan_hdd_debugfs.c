/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs.c
 *
 * This driver currently supports the following debugfs files:
 * wlan_wcnss/wow_enable to enable/disable WoWL.
 * wlan_wcnss/wow_pattern to configure WoWL patterns.
 * wlan_wcnss/pattern_gen to configure periodic TX patterns.
 */

#ifdef WLAN_OPEN_SOURCE
#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <wlan_hdd_debugfs.h>
#include <wlan_osif_request_manager.h>
#include <wlan_hdd_wowl.h>
#include <cds_sched.h>
#include <wlan_hdd_debugfs_llstat.h>
#include <wlan_hdd_debugfs_mibstat.h>
#include "wlan_hdd_debugfs_unit_test.h"


#define MAX_USER_COMMAND_SIZE_WOWL_ENABLE 8
#define MAX_USER_COMMAND_SIZE_WOWL_PATTERN 512
#define MAX_USER_COMMAND_SIZE_FRAME 4096

#define MAX_DEBUGFS_WAIT_ITERATIONS 20
#define DEBUGFS_WAIT_SLEEP_TIME 100

static bool hdd_periodic_pattern_map[MAXNUM_PERIODIC_TX_PTRNS];

static qdf_atomic_t debugfs_thread_count;

void hdd_debugfs_thread_increment(void)
{
	qdf_atomic_inc(&debugfs_thread_count);
}

void hdd_debugfs_thread_decrement(void)
{
	qdf_atomic_dec(&debugfs_thread_count);
}

int hdd_return_debugfs_threads_count(void)
{
	return qdf_atomic_read(&debugfs_thread_count);
}

bool hdd_wait_for_debugfs_threads_completion(void)
{
	int count = MAX_DEBUGFS_WAIT_ITERATIONS;
	int r;

	while (count) {
		r = hdd_return_debugfs_threads_count();
		if (!r)
			break;

		if (--count) {
			hdd_debug("Waiting for %d debugfs threads to exit", r);
			qdf_sleep(DEBUGFS_WAIT_SLEEP_TIME);
		}
	}

	/* at least one debugfs thread is executing */
	if (!count) {
		hdd_err("Timed-out waiting for debugfs threads");
		return false;
	}

	return true;
}

/**
 * __wcnss_wowpattern_write() - wow_pattern debugfs handler
 * @net_dev: net_device context used to register the debugfs file
 * @buf: text being written to the debugfs
 * @count: size of @buf
 * @ppos: (unused) offset into the virtual file system
 *
 * Return: number of bytes processed
 */
static ssize_t __wcnss_wowpattern_write(struct net_device *net_dev,
					const char __user *buf, size_t count,
					loff_t *ppos)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	char cmd[MAX_USER_COMMAND_SIZE_WOWL_PATTERN + 1];
	char *sptr, *token;
	uint8_t pattern_idx = 0;
	uint8_t pattern_offset = 0;
	char *pattern_buf;
	char *pattern_mask;
	int ret;

	hdd_enter();

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("Invalid adapter or adapter has invalid magic");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (!sme_is_feature_supported_by_fw(WOW)) {
		hdd_err("Wake-on-Wireless feature is not supported in firmware!");
		return -EINVAL;
	}

	if (count > MAX_USER_COMMAND_SIZE_WOWL_PATTERN) {
		hdd_err("Command length is larger than %d bytes",
			MAX_USER_COMMAND_SIZE_WOWL_PATTERN);
		return -EINVAL;
	}

	/* Get command from user */
	if (copy_from_user(cmd, buf, count))
		return -EFAULT;
	cmd[count] = '\0';
	sptr = cmd;

	/* Get pattern idx */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	if (kstrtou8(token, 0, &pattern_idx))
		return -EINVAL;

	/* Get pattern offset */
	token = strsep(&sptr, " ");

	/* Delete pattern if no further argument */
	if (!token) {
		hdd_del_wowl_ptrn_debugfs(adapter, pattern_idx);

		return count;
	}

	if (kstrtou8(token, 0, &pattern_offset))
		return -EINVAL;

	/* Get pattern */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	pattern_buf = token;

	/* Get pattern mask */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;

	pattern_mask = token;
	pattern_mask[strlen(pattern_mask) - 1] = '\0';

	hdd_add_wowl_ptrn_debugfs(adapter, pattern_idx, pattern_offset,
				  pattern_buf, pattern_mask);
	hdd_exit();
	return count;
}

/**
 * wcnss_wowpattern_write() - SSR wrapper for __wcnss_wowpattern_write
 * @file: file pointer
 * @buf: buffer
 * @count: count
 * @ppos: position pointer
 *
 * Return: 0 on success, error number otherwise
 */
static ssize_t wcnss_wowpattern_write(struct file *file,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	struct net_device *net_dev = file_inode(file)->i_private;
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __wcnss_wowpattern_write(net_dev, buf, count, ppos);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

/**
 * wcnss_patterngen_write() - pattern_gen debugfs handler
 * @net_dev: net_device context used to register the debugfs file
 * @buf: text being written to the debugfs
 * @count: size of @buf
 * @ppos: (unused) offset into the virtual file system
 *
 * Return: number of bytes processed
 */
static ssize_t __wcnss_patterngen_write(struct net_device *net_dev,
					const char __user *buf, size_t count,
					loff_t *ppos)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	tSirAddPeriodicTxPtrn *addPeriodicTxPtrnParams;
	tSirDelPeriodicTxPtrn *delPeriodicTxPtrnParams;

	char *cmd, *sptr, *token;
	uint8_t pattern_idx = 0;
	uint8_t pattern_duration = 0;
	char *pattern_buf;
	uint16_t pattern_len = 0;
	uint16_t i = 0;
	QDF_STATUS status;
	int ret;

	hdd_enter();

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("Invalid adapter or adapter has invalid magic");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	if (!sme_is_feature_supported_by_fw(WLAN_PERIODIC_TX_PTRN)) {
		hdd_err("Periodic Tx Pattern Offload feature is not supported in firmware!");
		return -EINVAL;
	}

	/* Get command from user */
	if (count <= MAX_USER_COMMAND_SIZE_FRAME)
		cmd = qdf_mem_malloc(count + 1);
	else {
		hdd_err("Command length is larger than %d bytes",
			MAX_USER_COMMAND_SIZE_FRAME);

		return -EINVAL;
	}

	if (!cmd) {
		hdd_err("Memory allocation for cmd failed!");
		return -ENOMEM;
	}

	if (copy_from_user(cmd, buf, count)) {
		qdf_mem_free(cmd);
		return -EFAULT;
	}
	cmd[count] = '\0';
	sptr = cmd;

	/* Get pattern idx */
	token = strsep(&sptr, " ");
	if (!token)
		goto failure;
	if (kstrtou8(token, 0, &pattern_idx))
		goto failure;

	if (pattern_idx > (MAXNUM_PERIODIC_TX_PTRNS - 1)) {
		hdd_err("Pattern index: %d is not in the range (0 ~ %d)",
			pattern_idx, MAXNUM_PERIODIC_TX_PTRNS - 1);

		goto failure;
	}

	/* Get pattern duration */
	token = strsep(&sptr, " ");
	if (!token)
		goto failure;
	if (kstrtou8(token, 0, &pattern_duration))
		goto failure;

	/* Delete pattern using index if duration is 0 */
	if (!pattern_duration) {
		if (!hdd_periodic_pattern_map[pattern_idx]) {
			hdd_debug_rl("WoW pattern %d is not in the table.",
				     pattern_idx);

			qdf_mem_free(cmd);
			return -EINVAL;
		}

		delPeriodicTxPtrnParams =
			qdf_mem_malloc(sizeof(tSirDelPeriodicTxPtrn));
		if (!delPeriodicTxPtrnParams) {
			qdf_mem_free(cmd);
			return -ENOMEM;
		}

		delPeriodicTxPtrnParams->ucPtrnId = pattern_idx;
		qdf_copy_macaddr(&delPeriodicTxPtrnParams->mac_address,
				 &adapter->mac_addr);

		/* Delete pattern */
		status = sme_del_periodic_tx_ptrn(hdd_ctx->mac_handle,
						  delPeriodicTxPtrnParams);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("sme_del_periodic_tx_ptrn() failed!");

			qdf_mem_free(delPeriodicTxPtrnParams);
			goto failure;
		}

		hdd_periodic_pattern_map[pattern_idx] = false;

		qdf_mem_free(cmd);
		qdf_mem_free(delPeriodicTxPtrnParams);
		return count;
	}

	/*
	 * In SAP mode allow configuration without any connection check
	 * In STA mode check if it's in connected state before adding
	 * patterns
	 */
	hdd_debug("device mode %d", adapter->device_mode);
	if ((QDF_STA_MODE == adapter->device_mode) &&
	    (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter)))) {
		hdd_err("Not in Connected state!");
		goto failure;
	}

	/* Get pattern */
	token = strsep(&sptr, " ");
	if (!token)
		goto failure;

	pattern_buf = token;
	pattern_buf[strlen(pattern_buf) - 1] = '\0';
	pattern_len = strlen(pattern_buf);

	/* Since the pattern is a hex string, 2 characters represent 1 byte. */
	if (pattern_len % 2) {
		hdd_err("Malformed pattern!");

		goto failure;
	} else
		pattern_len >>= 1;

	if (pattern_len < 14 || pattern_len > PERIODIC_TX_PTRN_MAX_SIZE) {
		hdd_err("Not an 802.3 frame!");

		goto failure;
	}

	addPeriodicTxPtrnParams = qdf_mem_malloc(sizeof(tSirAddPeriodicTxPtrn));
	if (!addPeriodicTxPtrnParams) {
		qdf_mem_free(cmd);
		return -ENOMEM;
	}

	addPeriodicTxPtrnParams->ucPtrnId = pattern_idx;
	addPeriodicTxPtrnParams->usPtrnIntervalMs = pattern_duration * 500;
	addPeriodicTxPtrnParams->ucPtrnSize = pattern_len;
	qdf_copy_macaddr(&addPeriodicTxPtrnParams->mac_address,
			 &adapter->mac_addr);

	/* Extract the pattern */
	for (i = 0; i < addPeriodicTxPtrnParams->ucPtrnSize; i++) {
		addPeriodicTxPtrnParams->ucPattern[i] =
			(hex_to_bin(pattern_buf[0]) << 4) +
			hex_to_bin(pattern_buf[1]);

		/* Skip to next byte */
		pattern_buf += 2;
	}

	/* Add pattern */
	status = sme_add_periodic_tx_ptrn(hdd_ctx->mac_handle,
					  addPeriodicTxPtrnParams);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("sme_add_periodic_tx_ptrn() failed!");

		qdf_mem_free(addPeriodicTxPtrnParams);
		goto failure;
	}

	if (!hdd_periodic_pattern_map[pattern_idx])
		hdd_periodic_pattern_map[pattern_idx] = true;

	qdf_mem_free(cmd);
	qdf_mem_free(addPeriodicTxPtrnParams);
	hdd_exit();
	return count;

failure:
	hdd_err("Invalid input. Input format is: ptrn_idx duration pattern");
	qdf_mem_free(cmd);
	return -EINVAL;
}

/**
 * wcnss_patterngen_write() - SSR wrapper for __wcnss_patterngen_write
 * @file: file pointer
 * @buf: buffer
 * @count: count
 * @ppos: position pointer
 *
 * Return: 0 on success, error number otherwise
 */
static ssize_t wcnss_patterngen_write(struct file *file,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	struct net_device *net_dev = file_inode(file)->i_private;
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __wcnss_patterngen_write(net_dev, buf, count, ppos);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

/**
 * __wcnss_debugfs_open() - Generic debugfs open() handler
 * @net_dev: net_device context used to register the debugfs file
 *
 * Return: Errno
 */
static int __wcnss_debugfs_open(struct net_device *net_dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_enter();

	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("Invalid adapter or adapter has invalid magic");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);

	hdd_exit();

	return errno;
}

/**
 * wcnss_debugfs_open() - SSR wrapper for __wcnss_debugfs_open
 * @inode: inode pointer
 * @file: file pointer
 *
 * Return: 0 on success, error number otherwise
 */
static int wcnss_debugfs_open(struct inode *inode, struct file *file)
{
	struct net_device *net_dev = inode->i_private;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wcnss_debugfs_open(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static const struct file_operations fops_wowpattern = {
	.write = wcnss_wowpattern_write,
	.open = wcnss_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_patterngen = {
	.write = wcnss_patterngen_write,
	.open = wcnss_debugfs_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

/**
 * hdd_debugfs_init() - Initialize debugfs interface
 * @adapter: interface adapter pointer
 *
 * Register support for the debugfs files supported by the driver.
 *
 * NB: The current implementation only supports debugfs operations
 * on the primary interface, i.e. wlan0
 *
 * Return: QDF_STATUS_SUCCESS if all files registered,
 *	   QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS hdd_debugfs_init(struct hdd_adapter *adapter)
{
	struct net_device *net_dev = adapter->dev;

	adapter->debugfs_phy = debugfs_create_dir(net_dev->name, 0);

	if (!adapter->debugfs_phy) {
		hdd_err("debugfs: create folder %s fail", net_dev->name);
		return QDF_STATUS_E_FAILURE;
	}

	if (!debugfs_create_file("wow_pattern", 00400 | 00200,
					adapter->debugfs_phy, net_dev,
					&fops_wowpattern))
		return QDF_STATUS_E_FAILURE;

	if (!debugfs_create_file("pattern_gen", 00400 | 00200,
					adapter->debugfs_phy, net_dev,
					&fops_patterngen))
		return QDF_STATUS_E_FAILURE;

	if (wlan_hdd_create_mib_stats_file(adapter))
		return QDF_STATUS_E_FAILURE;

	if (wlan_hdd_create_ll_stats_file(adapter))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_debugfs_exit() - Shutdown debugfs interface
 * @adapter: interface adapter pointer
 *
 * Unregister support for the debugfs files supported by the driver.
 *
 * Return: None
 */
void hdd_debugfs_exit(struct hdd_adapter *adapter)
{
	debugfs_remove_recursive(adapter->debugfs_phy);
}
#endif /* #ifdef WLAN_OPEN_SOURCE */
