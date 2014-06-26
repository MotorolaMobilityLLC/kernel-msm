/*
 * logger_kernel.h - Kernel Logger Subsytem
 *
 *  Copyright (C) Intel 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_LOGGER_KERNEL_H
#define _LINUX_LOGGER_KERNEL_H

#include <linux/types.h>
#include "logger.h"

#define LOGGER_LOG_KERNEL	"log_kernel"   /* kernel-related messages */
#define LOGGER_LOG_KERNEL_BOT	"log_kern_bot" /* system/hardware events */

#endif /* _LINUX_LOGGER_KERNEL_H */
