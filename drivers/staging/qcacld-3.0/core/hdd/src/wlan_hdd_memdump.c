/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_memdump.c
 *
 * WLAN Host Device Driver file for dumping firmware memory
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <linux/uaccess.h> /* for copy_to_user */
#include "osif_sync.h"
#include <sme_api.h>
#include <wlan_hdd_includes.h>

#ifdef MULTI_IF_NAME
#define PROCFS_DRIVER_DUMP_DIR "debugdriver" MULTI_IF_NAME
#else
#define PROCFS_DRIVER_DUMP_DIR "debugdriver"
#endif
#define PROCFS_DRIVER_DUMP_NAME "driverdump"
#define PROCFS_DRIVER_DUMP_PERM 0444

static struct proc_dir_entry *proc_file_driver, *proc_dir_driver;

/** memdump_get_file_data() - get data available in proc file
 *
 * @file - handle for the proc file.
 *
 * This function is used to retrieve the data passed while
 * creating proc file entry.
 *
 * Return: void pointer to hdd_context
 */
static void *memdump_get_file_data(struct file *file)
{
	void *hdd_ctx;

	hdd_ctx = PDE_DATA(file_inode(file));
	return hdd_ctx;
}

void hdd_driver_mem_cleanup(void)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return;
	}

	if (hdd_ctx->driver_dump_mem) {
		qdf_mem_free(hdd_ctx->driver_dump_mem);
		hdd_ctx->driver_dump_mem = NULL;
	}
}


/**
 * __hdd_driver_memdump_read() - perform read operation in driver
 * memory dump proc file
 * @file  - handle for the proc file.
 * @buf   - pointer to user space buffer.
 * @count - number of bytes to be read.
 * @pos   - offset in the from buffer.
 *
 * This function performs read operation for the driver memory dump proc file.
 *
 * Return: number of bytes read on success
 *         negative error code in case of failure
 *         0 in case of no more data
 */
static ssize_t __hdd_driver_memdump_read(struct file *file, char __user *buf,
					 size_t count, loff_t *pos)
{
	int status;
	QDF_STATUS qdf_status;
	struct hdd_context *hdd_ctx;
	size_t no_of_bytes_read = 0;

	hdd_ctx = memdump_get_file_data(file);

	hdd_debug("Read req for size:%zu pos:%llu", count, *pos);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (status != 0)
		return -EINVAL;

	mutex_lock(&hdd_ctx->memdump_lock);
	if (*pos < 0) {
		hdd_err("Invalid start offset for memdump read");
		mutex_unlock(&hdd_ctx->memdump_lock);
		return -EINVAL;
	}

	if (!count ||
	    (hdd_ctx->driver_dump_size && *pos >= hdd_ctx->driver_dump_size)) {
		mutex_unlock(&hdd_ctx->memdump_lock);
		hdd_debug("No more data to copy");
		return 0;
	}

	if (*pos == 0 || !hdd_ctx->driver_dump_mem) {
		/* Allocate memory for Driver memory dump */
		if (!hdd_ctx->driver_dump_mem) {
			hdd_ctx->driver_dump_mem =
				qdf_mem_malloc(DRIVER_MEM_DUMP_SIZE);
			if (!hdd_ctx->driver_dump_mem) {
				mutex_unlock(&hdd_ctx->memdump_lock);
				return -ENOMEM;
			}
		}

		qdf_status = qdf_state_info_dump_all(hdd_ctx->driver_dump_mem,
						DRIVER_MEM_DUMP_SIZE,
						&hdd_ctx->driver_dump_size);
		/*
		 * If qdf_status is QDF_STATUS_E_NOMEM, then memory allocated is
		 * insufficient to dump driver information. This print can give
		 * information to allocate more memory if more information from
		 * each layer is added in future.
		 */
		if (qdf_status != QDF_STATUS_SUCCESS)
			hdd_err("Error in dump driver information, status %d",
				qdf_status);
		hdd_debug("driver_dump_size: %d", hdd_ctx->driver_dump_size);
	}

	if (count > hdd_ctx->driver_dump_size - *pos)
		no_of_bytes_read = hdd_ctx->driver_dump_size - *pos;
	else
		no_of_bytes_read = count;

	if (copy_to_user(buf, hdd_ctx->driver_dump_mem + *pos,
					no_of_bytes_read)) {
		hdd_err("copy to user space failed");
		mutex_unlock(&hdd_ctx->memdump_lock);
		return -EFAULT;
	}

	/* offset(pos) should be updated here based on the copy done */
	*pos += no_of_bytes_read;

	/* Entire driver memory dump copy completed */
	if (*pos >= hdd_ctx->driver_dump_size)
		hdd_driver_mem_cleanup();

	mutex_unlock(&hdd_ctx->memdump_lock);

	return no_of_bytes_read;
}

/**
 * hdd_driver_memdump_read() - perform read operation in driver
 * memory dump proc file
 * @file  - handle for the proc file.
 * @buf   - pointer to user space buffer.
 * @count - number of bytes to be read.
 * @pos   - offset in the from buffer.
 *
 * This function performs read operation for the driver memory dump proc file.
 *
 * Return: number of bytes read on success
 *         negative error code in case of failure
 *         0 in case of no more data
 */
static ssize_t hdd_driver_memdump_read(struct file *file, char __user *buf,
				       size_t count, loff_t *pos)
{
	struct osif_driver_sync *driver_sync;
	ssize_t err_size;

	err_size = osif_driver_sync_op_start(&driver_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_driver_memdump_read(file, buf, count, pos);

	osif_driver_sync_op_stop(driver_sync);

	return err_size;
}

/**
 * struct driver_dump_fops - file operations for driver dump feature
 * @read - read function for driver dump operation.
 *
 * This structure initialize the file operation handle for memory
 * dump feature
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops driver_dump_fops = {
	.proc_read = hdd_driver_memdump_read,
};
#else
static const struct file_operations driver_dump_fops = {
	.read = hdd_driver_memdump_read,
};
#endif

/**
 * hdd_driver_memdump_procfs_init() - Initialize procfs for driver memory dump
 * @hdd_ctx Pointer to hdd context
 *
 * This function create file under proc file system to be used later for
 * processing driver memory dump
 *
 * Return:   0 on success, error code otherwise.
 */
static int hdd_driver_memdump_procfs_init(struct hdd_context *hdd_ctx)
{
	proc_dir_driver = proc_mkdir(PROCFS_DRIVER_DUMP_DIR, NULL);
	if (!proc_dir_driver) {
		pr_debug("Could not initialize /proc/%s\n",
			 PROCFS_DRIVER_DUMP_DIR);
		return -ENOMEM;
	}

	proc_file_driver = proc_create_data(PROCFS_DRIVER_DUMP_NAME,
				     PROCFS_DRIVER_DUMP_PERM, proc_dir_driver,
				     &driver_dump_fops, hdd_ctx);
	if (!proc_file_driver) {
		remove_proc_entry(PROCFS_DRIVER_DUMP_NAME, proc_dir_driver);
		pr_debug("Could not initialize /proc/%s\n",
			  PROCFS_DRIVER_DUMP_NAME);
		return -ENOMEM;
	}

	pr_debug("/proc/%s/%s created\n", PROCFS_DRIVER_DUMP_DIR,
		 PROCFS_DRIVER_DUMP_NAME);
	return 0;
}

/**
 * hdd_driver_memdump_procfs_remove() - Remove file/dir under procfs
 * for driver memory dump
 *
 * This function removes file/dir under proc file system that was
 * processing driver memory dump
 *
 * Return:  None
 */
static void hdd_driver_memdump_procfs_remove(void)
{
	remove_proc_entry(PROCFS_DRIVER_DUMP_NAME, proc_dir_driver);
	pr_debug("/proc/%s/%s removed\n", PROCFS_DRIVER_DUMP_DIR,
					  PROCFS_DRIVER_DUMP_NAME);
	remove_proc_entry(PROCFS_DRIVER_DUMP_DIR, NULL);
	pr_debug("/proc/%s removed\n", PROCFS_DRIVER_DUMP_DIR);
}

/**
 * hdd_driver_memdump_init() - Intialization function for driver
 * memory dump feature
 *
 * This function creates proc file for driver memdump feature
 *
 * Return - 0 on success, error otherwise
 */
int hdd_driver_memdump_init(void)
{
	int status;
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return -EINVAL;
	}

	mutex_init(&hdd_ctx->memdump_lock);

	status = hdd_driver_memdump_procfs_init(hdd_ctx);
	if (status) {
		hdd_err("Failed to create proc file");
		return status;
	}

	return 0;
}

/**
 * hdd_driver_memdump_deinit() - De initialize driver memdump feature
 *
 * This function removes proc file created for driver memdump feature.
 *
 * Return: None
 */
void hdd_driver_memdump_deinit(void)
{
	hdd_driver_memdump_procfs_remove();
}
