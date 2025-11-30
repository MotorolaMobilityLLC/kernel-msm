// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include "buffer_manager.h"
#include <linux/uaccess.h>

void eom_buffer_init(struct eom_buffer *buf)
{
	INIT_LIST_HEAD(&buf->page_list);
	buf->read_offset = 0;
	buf->total_size = 0;
	mutex_init(&buf->mutex);
}

void eom_buffer_free(struct eom_buffer *buf)
{
	struct buffer_page *page, *tmp;

	mutex_lock(&buf->mutex);
	list_for_each_entry_safe(page, tmp, &buf->page_list, list) {
		list_del(&page->list);
		kfree(page->data);
		kfree(page);
	}
	buf->total_size = 0;
	buf->read_offset = 0;
	mutex_unlock(&buf->mutex);
}

/**
 * eom_buffer_write - Write data to the buffer
 * @buf: Pointer to the eom_buffer structure representing the buffer
 * @data: Pointer to the data to be written to the buffer
 * @size: Size of the data to be written
 *
 * Writes data to the buffer, allocating new pages as needed. The write operation
 * is sequential, meaning that new data is always appended to the tail of the
 * buffer. Returns the number of bytes successfully written. If the buffer is NULL
 * or data is NULL, returns -EINVAL. If memory allocation fails, returns -ENOMEM.
 *
 * Note: The buffer does not support random writes or overwriting existing data.
 * All writes are sequential and append to the end of the buffer. For EOM,
 * The data is stored in the eom_entry structure as tuple, which contains the x and y
 * coordinates and the error count. this tuple is appended for each sample point.
 *
 * Return: Number of bytes written to the buffer, or a negative error code.
 */
ssize_t eom_buffer_write(struct eom_buffer *buf, const char *data, size_t size)
{
	size_t copied = 0;

	if (buf == NULL || data == NULL)
		return -EINVAL;

	mutex_lock(&buf->mutex);

	while (size > 0) {
		size_t to_copy;
		struct buffer_page *last_page = NULL;

		if (!list_empty(&buf->page_list))
			last_page = list_last_entry(&buf->page_list, struct buffer_page, list);

		/* Allocate a new page if needed or if the last page is full */
		if (!last_page || last_page->size >= PAGE_SIZE) {
			struct buffer_page *new_page = kmalloc(sizeof(*new_page), GFP_KERNEL);

			if (!new_page) {
				mutex_unlock(&buf->mutex);
				return -ENOMEM;
			}

			new_page->data = kmalloc(PAGE_SIZE, GFP_KERNEL);
			if (!new_page->data) {
				kfree(new_page);
				mutex_unlock(&buf->mutex);
				return -ENOMEM;
			}

			new_page->size = 0;
			INIT_LIST_HEAD(&new_page->list);
			list_add_tail(&new_page->list, &buf->page_list);
			last_page = new_page;
		}

		/* Copy data into current tail page */
		to_copy = min(size, PAGE_SIZE - last_page->size);
		memcpy(last_page->data + last_page->size, data + copied, to_copy);

		last_page->size += to_copy;
		buf->total_size += to_copy;
		copied += to_copy;
		size -= to_copy;
	}

	mutex_unlock(&buf->mutex);

	return copied;
}

/**
 * eom_buffer_read - Read data from the buffer
 * @buf: Pointer to the eom_buffer structure representing the buffer
 * @user_buf: Pointer to the user-space buffer where the data will be copied
 * @count: Maximum number of bytes to read from the buffer
 *
 * Reads data from the buffer, copying it to the user-space buffer. The read
 * operation starts from the current read pointer and moves forward. The read
 * pointer is updated after each read operation. Returns the number of bytes
 * successfully read. If the user buffer is NULL or the buffer is NULL, returns
 * -EINVAL. If the buffer is empty, returns 0.
 *
 * Note: The buffer does not support random reads or seeking. All reads are
 * sequential and start from the current read pointer.
 *
 * Return: Number of bytes read from the buffer, or a negative error code.
 */
ssize_t eom_buffer_read(struct eom_buffer *buf, char __user *user_buf, size_t count)
{
	size_t copied = 0;

	if (user_buf == NULL || buf == NULL)
		return -EINVAL;

	mutex_lock(&buf->mutex);

	while (count > 0) {
		struct buffer_page *page;

		/* Get the first page with data */
		if (list_empty(&buf->page_list))
			break;

		page = list_first_entry(&buf->page_list, struct buffer_page, list);

		size_t available = page->size - buf->read_offset;
		size_t to_copy = min(count, available);

		if (copy_to_user(user_buf + copied, page->data + buf->read_offset, to_copy)) {
			/* If copy_to_user fails, return bytes successfully copied so far.
			 * The buffer state (read_offset, etc.) is NOT updated for this failed copy.
			 */
			mutex_unlock(&buf->mutex);
			return copied;
		}

		buf->read_offset += to_copy;
		copied += to_copy;
		count -= to_copy;

		/* Move to next page if finished reading this one */
		if (buf->read_offset >= page->size) {
			list_del(&page->list);
			buf->total_size -= page->size;
			kfree(page->data);
			kfree(page);
			buf->read_offset = 0;
		}
	}
	mutex_unlock(&buf->mutex);

	return copied;
}
