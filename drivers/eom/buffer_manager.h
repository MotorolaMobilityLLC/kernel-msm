/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>

struct buffer_page {
	char *data;
	size_t size;
	struct list_head list;
};

struct eom_buffer {
	struct list_head page_list;
	size_t read_offset;
	size_t total_size;
	struct mutex mutex;
};

void eom_buffer_init(struct eom_buffer *buf);
void eom_buffer_free(struct eom_buffer *buf);
ssize_t eom_buffer_write(struct eom_buffer *buf, const char *data, size_t size);
ssize_t eom_buffer_read(struct eom_buffer *buf, char __user *user_buf, size_t count);

#endif
