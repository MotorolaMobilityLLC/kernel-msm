/*
 * Copyright (c) 2013-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_DEBUGFS_H
#define _WLAN_HDD_DEBUGFS_H

#ifdef WLAN_DEBUGFS

#define HDD_DEBUGFS_FILE_NAME_MAX 24

/**
 * enum hdd_debugfs_file_id - Debugfs file Identifier
 * @HDD_DEBUFS_FILE_ID_ROAM_SCAN_STATS_INFO: roam_scan_stats file id
 * @HDD_DEBUFS_FILE_ID_OFFLOAD_INFO: offload_info file id
 * @HDD_DEBUGFS_FILE_ID_MAX: maximum id of csr debugfs file
 */
enum hdd_debugfs_file_id {
	HDD_DEBUFS_FILE_ID_ROAM_SCAN_STATS_INFO = 0,
	HDD_DEBUFS_FILE_ID_OFFLOAD_INFO = 1,

	HDD_DEBUGFS_FILE_ID_MAX,
};

/**
 * struct hdd_debugfs_file_info - Debugfs file info
 * @name: name of debugfs file
 * @id: id from enum hdd_debugfs_file_id used to identify file
 * @buf_max_size: max size of buffer from which debugfs file is updated
 * @entry: dentry pointer to debugfs file
 */
struct hdd_debugfs_file_info {
	uint8_t name[HDD_DEBUGFS_FILE_NAME_MAX];
	enum hdd_debugfs_file_id id;
	ssize_t buf_max_size;
	struct dentry *entry;
};

QDF_STATUS hdd_debugfs_init(struct hdd_adapter *adapter);
void hdd_debugfs_exit(struct hdd_adapter *adapter);

/**
 * hdd_wait_for_debugfs_threads_completion() - Wait for debugfs threads
 * completion before proceeding further to stop modules
 *
 * Return: true if there is no debugfs open
 *         false if there is at least one debugfs open
 */
bool hdd_wait_for_debugfs_threads_completion(void);

/**
 * hdd_return_debugfs_threads_count() - Return active debugfs threads
 *
 * Return: total number of active debugfs threads in driver
 */
int hdd_return_debugfs_threads_count(void);

/**
 * hdd_debugfs_thread_increment() - Increment debugfs thread count
 *
 * This function is used to increment and keep track of debugfs thread count.
 * This is invoked for every file open operation.
 *
 * Return: None
 */
void hdd_debugfs_thread_increment(void);

/**
 * hdd_debugfs_thread_decrement() - Decrement debugfs thread count
 *
 * This function is used to decrement and keep track of debugfs thread count.
 * This is invoked for every file release operation.
 *
 * Return: None
 */
void hdd_debugfs_thread_decrement(void);

#else
static inline QDF_STATUS hdd_debugfs_init(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}

static inline void hdd_debugfs_exit(struct hdd_adapter *adapter)
{
}

/**
 * hdd_wait_for_debugfs_threads_completion() - Wait for debugfs threads
 * completion before proceeding further to stop modules
 *
 * Return: true if there is no debugfs open
 *         false if there is at least one debugfs open
 */
static inline
bool hdd_wait_for_debugfs_threads_completion(void)
{
	return true;
}

/**
 * hdd_return_debugfs_threads_count() - Return active debugfs threads
 *
 * Return: total number of active debugfs threads in driver
 */
static inline
int hdd_return_debugfs_threads_count(void)
{
	return 0;
}

/**
 * hdd_debugfs_thread_increment() - Increment debugfs thread count
 *
 * This function is used to increment and keep track of debugfs thread count.
 * This is invoked for every file open operation.
 *
 * Return: None
 */
static inline
void hdd_debugfs_thread_increment(void)
{
}

/**
 * hdd_debugfs_thread_decrement() - Decrement debugfs thread count
 *
 * This function is used to decrement and keep track of debugfs thread count.
 * This is invoked for every file release operation.
 *
 * Return: None
 */
static inline
void hdd_debugfs_thread_decrement(void)
{
}

#endif /* #ifdef WLAN_DEBUGFS */
#endif /* #ifndef _WLAN_HDD_DEBUGFS_H */
