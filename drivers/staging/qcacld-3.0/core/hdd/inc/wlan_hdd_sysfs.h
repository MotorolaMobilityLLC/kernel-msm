/*
 * Copyright (c) 2017-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_SYSFS_H_
#define _WLAN_HDD_SYSFS_H_

#ifdef WLAN_SYSFS

#define MAX_SYSFS_USER_COMMAND_SIZE_LENGTH (32)

/**
 * hdd_sys_validate_and_copy_buf() - validate sysfs input buf and copy into
 *                                   destination buffer
 * @dest_buf - pointer to destination buffer where data should be copied
 * @dest_buf_size - size of destination buffer
 * @src_buf - pointer to constant sysfs source buffer
 * @src_buf_size - size of source buffer
 *
 * Return: 0 for success and error code for failure
 */
int
hdd_sysfs_validate_and_copy_buf(char *dest_buf, size_t dest_buf_size,
				char const *src_buf, size_t src_buf_size);

/**
 * hdd_create_sysfs_files() - create sysfs files
 * @hdd_ctx: pointer to hdd context
 *
 * Return: none
 */
void hdd_create_sysfs_files(struct hdd_context *hdd_ctx);

/**
 * hdd_destroy_sysfs_files() - destroy sysfs files
 *
 * Return: none
 */
void hdd_destroy_sysfs_files(void);

/**
 * hdd_create_adapter_sysfs_files - create adapter sysfs files
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_create_adapter_sysfs_files(struct hdd_adapter *adapter);

/**
 * hdd_destroy_adapter_sysfs_files - destroy adapter sysfs files
 * @adapter: pointer to adapter
 *
 * Return: none
 */
void hdd_destroy_adapter_sysfs_files(struct hdd_adapter *adapter);
#else
static inline int
hdd_sysfs_validate_and_copy_buf(char *dest_buf, size_t dest_buf_size,
				char const *src_buf, size_t src_buf_size)
{
	return -EPERM;
}

static void hdd_create_sysfs_files(struct hdd_context *hdd_ctx)
{
}

static void hdd_destroy_sysfs_files(void)
{
}

static inline
void hdd_sysfs_create_adapter_root_obj(struct hdd_adapter *adapter)
{
}

static inline
void hdd_sysfs_destroy_adapter_root_obj(struct hdd_adapter *adapter)
{
}

static void hdd_create_adapter_sysfs_files(struct hdd_adapter *adapter)
{
}

static void hdd_destroy_adapter_sysfs_files(struct hdd_adapter *adapter)
{
}
#endif /* End of WLAN SYSFS*/

#endif /* End of _WLAN_HDD_SYSFS_H_ */
