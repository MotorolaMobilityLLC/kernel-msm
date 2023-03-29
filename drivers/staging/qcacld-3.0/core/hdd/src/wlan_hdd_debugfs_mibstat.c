/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs_mibstat.c
 *
 * WLAN Host Device Driver implementation to update
 * debugfs with MIB statistics
 */

#include <cds_sched.h>
#include "osif_sync.h"
#include <wlan_hdd_debugfs_mibstat.h>
#include <wlan_hdd_stats.h>
#include <wma_api.h>

struct mib_stats_buf {
	ssize_t len;
	uint8_t *result;
};

static struct mib_stats_buf mib_stats;

qdf_mutex_t mibstats_lock;

void hdd_debugfs_process_mib_stats(struct hdd_adapter *adapter,
				   struct stats_event *stats)
{
	ssize_t len = 0;
	uint8_t *buffer;

	hdd_enter();

	qdf_mutex_acquire(&mibstats_lock);
	if (!mib_stats.result) {
		qdf_mutex_release(&mibstats_lock);
		hdd_err("MIB statistics buffer is NULL");
		return;
	}

	buffer = mib_stats.result;
	buffer += mib_stats.len;

	len = scnprintf(buffer, DEBUGFS_MIBSTATS_BUF_SIZE - mib_stats.len,
			"dot11RTSSuccessCount %d "
			"\ndot11RTSFailureCount %d "
			"\ndot11QosFailedCount %d "
			"\ndot11QosRetryCount %d "
			"\ndot11QosTransmittedFrameCount %d "
			"\ndot11QosMPDUsReceivedCount %d "
			"\ndot11TransmittedAMPDUCount %d "
			"\ndot11QosACKFailureCount %d",
			stats->mib_stats->mib_mac_statistics.rts_success_cnt,
			stats->mib_stats->mib_mac_statistics.rts_fail_cnt,
			stats->mib_stats->mib_qos_counters.qos_failed_cnt,
			stats->mib_stats->mib_qos_counters.qos_retry_cnt,
			stats->mib_stats->mib_qos_counters.qos_tx_frame_cnt,
			stats->mib_stats->mib_qos_counters.qos_mpdu_rx_cnt,
			stats->mib_stats->mib_counters_group3.tx_ampdu_cnt,
			stats->mib_stats->
				mib_qos_counters.tx_qos_ack_fail_cnt_up
			);

	buffer += len;
	mib_stats.len += len;
	qdf_mutex_release(&mibstats_lock);

	hdd_exit();
}

static inline void wlan_hdd_mibstats_free_buf(void)
{
	qdf_mutex_acquire(&mibstats_lock);
	qdf_mem_free(mib_stats.result);
	mib_stats.result = NULL;
	mib_stats.len =  0;
	qdf_mutex_release(&mibstats_lock);
}

static int wlan_hdd_mibstats_alloc_buf(void)
{
	qdf_mutex_acquire(&mibstats_lock);
	if (mib_stats.result) {
		qdf_mutex_release(&mibstats_lock);
		hdd_err("Buffer is already allocated");
		return 0;
	}
	mib_stats.len = 0;
	mib_stats.result = qdf_mem_malloc(DEBUGFS_MIBSTATS_BUF_SIZE);
	if (!mib_stats.result) {
		qdf_mutex_release(&mibstats_lock);
		return -EINVAL;
	}
	qdf_mutex_release(&mibstats_lock);
	return 0;
}

/**
 * hdd_debugfs_mib_stats_update() - Update userspace with local stats buffer
 * @buf: userspace buffer (to which data is being copied into)
 * @count: max data that can be copied into buf in bytes
 * @pos: offset (where data should be copied into)
 *
 * This function copies mib statistics buffer into debugfs
 * entry.
 *
 * Return: number of characters copied; 0 on no-copy
 */
static ssize_t hdd_debugfs_mib_stats_update(char __user *buf,
					    size_t count, loff_t *pos)
{
	ssize_t ret_cnt;

	hdd_enter();
	qdf_mutex_acquire(&mibstats_lock);
	if (!mib_stats.result) {
		qdf_mutex_release(&mibstats_lock);
		hdd_err("Trying to read from NULL buffer");
		return 0;
	}

	ret_cnt = simple_read_from_buffer(buf, count, pos,
					  mib_stats.result,
					  mib_stats.len);
	qdf_mutex_release(&mibstats_lock);
	hdd_debug("mib stats read req: count: %zu, pos: %lld", count, *pos);

	hdd_exit();
	return ret_cnt;
}

/**
 * __wlan_hdd_release_mib_stats_debugfs() - Function to free private
 *                                          memory on release
 * @net_dev: net_device context used to register the debugfs file
 *
 * Return: Errno
 */
static int __wlan_hdd_release_mib_stats_debugfs(struct net_device *net_dev)

{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_enter_dev(net_dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	wlan_hdd_mibstats_free_buf();

	hdd_exit();

	return 0;
}

/**
 * wlan_hdd_release_mib_stats_debugfs() - SSR wrapper function to free
 *                                        private memory on release
 * @inode: Pointer to inode structure
 * @file: file pointer
 *
 * Return: Errno
 */
static int wlan_hdd_release_mib_stats_debugfs(struct inode *inode,
					      struct file *file)
{
	struct net_device *net_dev = file_inode(file)->i_private;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_release_mib_stats_debugfs(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_read_mib_stats_debugfs() - mib_stats debugfs handler
 * @net_dev: net_device context used to register the debugfs file
 * @buf: text being written to the debugfs
 * @count: size of @buf
 * @pos: (unused) offset into the virtual file system
 *
 * Return: Number of bytes read on success, error number otherwise
 */
static ssize_t __wlan_hdd_read_mib_stats_debugfs(struct net_device *net_dev,
						 char __user *buf,
						 size_t count, loff_t *pos)

{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	ssize_t ret;

	hdd_enter_dev(net_dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		goto free_buf;

	if (*pos == 0) {
		ret = wlan_hdd_get_mib_stats(adapter);
		if (ret)
			goto free_buf;
	}

	/* All the events are received and buffer is populated */
	ret = hdd_debugfs_mib_stats_update(buf, count, pos);
	hdd_debug("%zu characters written into debugfs", ret);

	hdd_exit();

	return ret;

free_buf:
	wlan_hdd_mibstats_free_buf();

	hdd_exit();

	return ret;
}

/**
 * wlan_hdd_read_mib_stats_debugfs() - SSR wrapper function to read
 *                                     mib stats
 * @file: file pointer
 * @buf: buffer
 * @count: count
 * @pos: position pointer
 *
 * Return: Number of bytes read on success, error number otherwise
 */
static ssize_t wlan_hdd_read_mib_stats_debugfs(struct file *file,
				   char __user *buf, size_t count,
				   loff_t *pos)
{
	struct net_device *net_dev = file_inode(file)->i_private;
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __wlan_hdd_read_mib_stats_debugfs(net_dev, buf,
						     count, pos);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

/**
 * __wlan_hdd_open_mib_stats_debugfs() - Function to save private on open
 * @net_dev: net_device context used to register the debugfs file
 *
 * Return: Errno
 */
static int __wlan_hdd_open_mib_stats_debugfs(struct net_device *net_dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	int errno;

	hdd_enter_dev(net_dev);

	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = wlan_hdd_mibstats_alloc_buf();
	if (errno)
		return errno;

	hdd_exit();

	return 0;
}

/**
 * wlan_hdd_open_mib_stats_debugfs() - SSR wrapper to save private
 *                                     on open
 * @inode: Pointer to inode structure
 * @file: file pointer
 *
 * Return: Errno
 */
static int wlan_hdd_open_mib_stats_debugfs(struct inode *inode,
			       struct file *file)
{
	struct net_device *net_dev = inode->i_private;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_open_mib_stats_debugfs(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

static const struct file_operations fops_mib_stats = {
	.read = wlan_hdd_read_mib_stats_debugfs,
	.open = wlan_hdd_open_mib_stats_debugfs,
	.release = wlan_hdd_release_mib_stats_debugfs,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int wlan_hdd_create_mib_stats_file(struct hdd_adapter *adapter)
{
	if (!debugfs_create_file("mib_stats", 0444, adapter->debugfs_phy,
				 adapter->dev, &fops_mib_stats))
		return -EINVAL;

	return 0;
}

void wlan_hdd_create_mib_stats_lock(void)
{
	if (QDF_IS_STATUS_ERROR(qdf_mutex_create(
				&mibstats_lock)))
		hdd_err("mibstats lock init failed!");
}

void wlan_hdd_destroy_mib_stats_lock(void)
{
	qdf_mutex_destroy(&mibstats_lock);
}
