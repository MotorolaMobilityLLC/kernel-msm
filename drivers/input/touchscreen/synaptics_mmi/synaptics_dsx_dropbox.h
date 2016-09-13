/* Copyright (c) 2014 - 2015, Motorola Mobility LLC. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#ifndef SYNAPTICS_DROPBOX_H
#define SYNAPTICS_DROPBOX_H

#include <linux/ratelimit.h>

#define SYNAPTICS_DROPBOX_MSG_I2C "Touch I2C communication error detected"
#define SYNAPTICS_DROPBOX_MSG_RESET_TIMEOUT "Touch reset time out detected"

#if defined(CONFIG_DROPBOX)
void synaptics_dropbox_report_event(char *msg, int count);
void synaptics_dropbox_report_event_ratelimit(char *msg, int count);
#else
static inline void synaptics_dropbox_report_event(char *msg, int count) {}
static inline void synaptics_dropbox_report_event_ratelimit(char *msg, int count) {}
#endif
#endif /* SYNAPTICS_DROPBOX_H */
