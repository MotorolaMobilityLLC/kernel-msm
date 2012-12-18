/*
 * Copyright (C) 2008 Google, Inc.
 *
 * Based on, but no longer compatible with, the original
 * OpenBinder.org binder driver interface, which is:
 *
 * Copyright (c) 2005 Palmsource, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_BINDER_H
#define _LINUX_BINDER_H

#ifdef CONFIG_ANDROID_BINDER_IPC_32BIT
#define BINDER_IPC_32BIT 1
#endif
struct binder_get_peer {
	pid_t	self;
	pid_t	remote;
};

#define BINDER_GET_PEER_PID		_IOWR('b', 10, struct binder_get_peer)

#include "uapi/binder.h"

#endif /* _LINUX_BINDER_H */

