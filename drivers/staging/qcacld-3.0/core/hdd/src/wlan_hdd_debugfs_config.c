/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs_config.c
 *
 * WLAN Host Device Driver implementation to update
 * debugfs with ini configs
 */

#include "wlan_hdd_main.h"
#include "osif_psoc_sync.h"
#include "cfg_ucfg_api.h"
#include "wlan_hdd_debugfs_config.h"

#define DEBUGFS_CONFIG_BUF_SIZE (4096 * 8)

/**
 * struct ini_config_buf - the buffer struct to save ini configs
 * @len: buffer len
 * @result: the pointer to buffer
 */
struct ini_config_buf {
	ssize_t len;
	uint8_t result[DEBUGFS_CONFIG_BUF_SIZE];
};

/**
 * wlan_hdd_config_update() - Update userspace with local statistics buffer
 * @buf: userspace buffer (to which data is being copied into)
 * @count: max data that can be copied into buf
 * @pos: offset (where data should be copied into)
 * @ini_config: buffer structure for ini config info
 *
 * This function should copies ini configs into debugfs entry.
 *
 * Return: number of characters copied; 0 on no-copy
 */
static ssize_t wlan_hdd_config_update(char __user *buf, size_t count,
				      loff_t *pos,
				      struct ini_config_buf *ini_config)
{
	ssize_t ret_cnt;

	ret_cnt = simple_read_from_buffer(buf, count, pos, ini_config->result,
					  ini_config->len);
	hdd_debug("ini config read req: count: %zu, pos: %lld", count, *pos);

	return ret_cnt;
}

/**
 * wlan_hdd_config_get() - Function to save ini config to buffer
 * @hdd_ctx: hdd context used to register the debugfs file
 * @ini_config: buffer structure for ini config info
 *
 * Return: Errno
 */
static int wlan_hdd_config_get(struct hdd_context *hdd_ctx,
			       struct ini_config_buf *ini_config)
{
	QDF_STATUS status;

	status = ucfg_cfg_ini_config_print(hdd_ctx->psoc, ini_config->result,
					   &ini_config->len,
					   DEBUGFS_CONFIG_BUF_SIZE);
	return qdf_status_to_os_return(status);
}

/**
 * __wlan_hdd_read_config_debugfs() - function to get ini conifg
 * @file: file pointer
 * @buf: buffer
 * @count: count
 * @pos: position pointer
 *
 * Return: Number of bytes read on success, error number otherwise
 */
static ssize_t __wlan_hdd_read_config_debugfs(struct file *file,
					      char __user *buf, size_t count,
					      loff_t *pos)
{
	struct ini_config_buf *ini_config;
	ssize_t err_size;

	ini_config = (struct ini_config_buf *)file->private_data;
	if (!ini_config)
		return -ENOMEM;

	err_size = wlan_hdd_config_update(buf, count, pos, ini_config);

	return err_size;
}

/**
 * wlan_hdd_read_config_debugfs() - wrapper function to get ini conifg
 * @file: file pointer
 * @buf: buffer
 * @count: count
 * @pos: position pointer
 *
 * Return: Number of bytes read on success, error number otherwise
 */
static ssize_t wlan_hdd_read_config_debugfs(struct file *file,
					    char __user *buf, size_t count,
					    loff_t *pos)
{
	struct hdd_context *hdd_ctx = file_inode(file)->i_private;
	struct osif_psoc_sync *psoc_sync;
	ssize_t err_size;

	err_size = wlan_hdd_validate_context(hdd_ctx);
	if (err_size)
		return err_size;

	err_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					   &psoc_sync);
	if (err_size)
		return err_size;

	err_size = __wlan_hdd_read_config_debugfs(file, buf, count, pos);

	osif_psoc_sync_op_stop(psoc_sync);

	return err_size;
}

/**
 * __wlan_hdd_open_config_debugfs() - function to open config debugfs
 * @inode: Pointer to inode structure
 * @file: file pointer
 *
 * Return: Errno
 */
static int __wlan_hdd_open_config_debugfs(struct inode *inode,
					  struct file *file)
{
	struct hdd_context *hdd_ctx = file_inode(file)->i_private;
	struct ini_config_buf *ini_config;
	ssize_t errno;
	void *buf;

	buf = qdf_mem_malloc(sizeof(*ini_config));
	if (!buf)
		return -ENOMEM;

	ini_config = (struct ini_config_buf *)buf;
	errno = wlan_hdd_config_get(hdd_ctx, ini_config);
	if (errno) {
		qdf_mem_free(buf);
		return errno;
	}

	file->private_data = buf;
	return 0;
}

/**
 * wlan_hdd_open_config_debugfs() - wrapper function to open config debugfs
 * @inode: Pointer to inode structure
 * @file: file pointer
 *
 * Return: Errno
 */
static int wlan_hdd_open_config_debugfs(struct inode *inode, struct file *file)
{
	struct hdd_context *hdd_ctx = file_inode(file)->i_private;
	struct osif_psoc_sync *psoc_sync;
	ssize_t errno;

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					&psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_open_config_debugfs(inode, file);

	osif_psoc_sync_op_stop(psoc_sync);
	return errno;
}

/**
 * wlan_hdd_release_config_debugfs() - wrapper to release
 * @inode: Pointer to inode structure
 * @file: file pointer
 *
 * Return: Errno
 */
static int wlan_hdd_release_config_debugfs(struct inode *inode,
					   struct file *file)
{
	qdf_mem_free(file->private_data);
	file->private_data = NULL;

	return 0;
}

static const struct file_operations fops_config_debugfs = {
	.read = wlan_hdd_read_config_debugfs,
	.open = wlan_hdd_open_config_debugfs,
	.release = wlan_hdd_release_config_debugfs,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int hdd_debugfs_ini_config_init(struct hdd_context *hdd_ctx)
{
	if (!debugfs_create_file("ini_config", 0444, qdf_debugfs_get_root(),
				 hdd_ctx, &fops_config_debugfs))
		return -EINVAL;

	return 0;
}

void hdd_debugfs_ini_config_deinit(struct hdd_context *hdd_ctx)
{
	/*
	 * Config ini doesn't have a directory it is removed
	 * as part of qdf remove
	 */
}
