/*
 * Copyright (c) 2013-2014, Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef DROPBOX_H
#define DROPBOX_H

#include <linux/types.h>
#include <linux/time.h>

#ifdef CONFIG_DROPBOX

extern void dropbox_register_trigger_callback(const char *name,
	void (*callback)(void *), void *data);
extern void dropbox_queue_event_binary(char *name,
	void *data, size_t size);
extern void dropbox_queue_event_text(char *name,
	void *data, size_t size);
extern void dropbox_queue_event_binaryfile(char *name, char *path);
extern void dropbox_queue_event_textfile(char *name, char *path);
extern void dropbox_queue_event_empty(char *name);

#else

static inline void dropbox_register_trigger_callback(const char *name,
	void (*callback)(void *), void *data) {}
static inline void dropbox_queue_event_binary(char *name,
	void *data, size_t size) {}
static inline void dropbox_queue_event_text(char *name,
	void *data, size_t size) {}
static inline void dropbox_queue_event_binaryfile(char *name, char *path) {}
static inline void dropbox_queue_event_textfile(char *name, char *path) {}
static inline void dropbox_queue_event_empty(char *name) {}

#endif

#endif /* DROPBOX_H */
