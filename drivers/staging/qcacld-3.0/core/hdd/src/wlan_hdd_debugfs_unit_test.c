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

/**
 * DOC: wlan_hdd_debugfs_unit_test.c
 *
 * WLAN Host Device Driver implementation to create debugfs
 * unit_test_host/unit_test_target/wlan_suspend/wlan_resume
 */
#include "wlan_hdd_main.h"
#include "osif_psoc_sync.h"
#include "osif_vdev_sync.h"
#include "wlan_dsc_test.h"
#include "wlan_hdd_unit_test.h"
#include "wlan_hdd_debugfs_unit_test.h"
#include "wma.h"

#ifdef WLAN_UNIT_TEST
/* strlen("all") + 1(\n) */
#define MIN_USER_COMMAND_SIZE_UNIT_TEST_HOST 4
#define MAX_USER_COMMAND_SIZE_UNIT_TEST_HOST 32

/**
 * __wlan_hdd_write_unit_test_host_debugfs()
 *    - host unit test debugfs handler
 *
 * @net_dev: net_device context used to register the debugfs file
 * @buf: text being written to the debugfs
 * @count: size of @buf
 * @ppos: (unused) offset into the virtual file system
 *
 * Return: number of bytes processed or errno
 */
static ssize_t __wlan_hdd_write_unit_test_host_debugfs(
		struct hdd_context *hdd_ctx,
		const char __user *buf, size_t count,
		loff_t *ppos)
{
	char name[MAX_USER_COMMAND_SIZE_UNIT_TEST_HOST + 1];
	int ret;

	if (count < MIN_USER_COMMAND_SIZE_UNIT_TEST_HOST ||
	    count > MAX_USER_COMMAND_SIZE_UNIT_TEST_HOST) {
		hdd_err_rl("Command length (%zu) is invalid, expected [%d, %d]",
			   count,
			   MIN_USER_COMMAND_SIZE_UNIT_TEST_HOST,
			   MAX_USER_COMMAND_SIZE_UNIT_TEST_HOST);
		return -EINVAL;
	}

	/* Get command from user */
	if (copy_from_user(name, buf, count))
		return -EFAULT;
	/* default 'echo' cmd takes new line character to here*/
	if (name[count - 1] == '\n')
		name[count - 1] = '\0';
	else
		name[count] = '\0';

	hdd_nofl_info("unit_test: count %zu name: %s", count, name);

	ret = wlan_hdd_unit_test(hdd_ctx, name);
	if (ret != 0)
		return ret;

	return count;
}

/**
 * wlan_hdd_write_unit_test_host_debugfs()
 *    - wrapper for __wlan_hdd_write_unit_test_host_debugfs
 *
 * @file: file pointer
 * @buf: buffer
 * @count: count
 * @ppos: position pointer
 *
 * Return: number of bytes processed or errno
 */
static ssize_t wlan_hdd_write_unit_test_host_debugfs(
		struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct hdd_context *hdd_ctx = file_inode(file)->i_private;
	struct osif_psoc_sync *psoc_sync;
	ssize_t errno_size;

	errno_size = wlan_hdd_validate_context(hdd_ctx);
	if (errno_size)
		return errno_size;

	errno_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					     &psoc_sync);
	if (errno_size)
		return errno_size;

	errno_size = __wlan_hdd_write_unit_test_host_debugfs(
				hdd_ctx, buf, count, ppos);
	if (errno_size < 0)
		hdd_err_rl("err_size %zd", errno_size);

	osif_psoc_sync_op_stop(psoc_sync);
	return errno_size;
}

static const struct file_operations fops_unit_test_host_debugfs = {
	.write = wlan_hdd_write_unit_test_host_debugfs,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int wlan_hdd_debugfs_unit_test_host_create(struct hdd_context *hdd_ctx)
{
	if (!debugfs_create_file("unit_test_host", 00400 | 00200,
				 qdf_debugfs_get_root(),
				 hdd_ctx, &fops_unit_test_host_debugfs))
		return -EINVAL;

	return 0;
}
#endif /* WLAN_UNIT_TEST */
