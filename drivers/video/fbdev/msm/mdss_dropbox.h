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
#ifndef MDSS_DROPBOX_H
#define MDSS_DROPBOX_H

#include <linux/ratelimit.h>

#define MDSS_DROPBOX_MSG_ESD "ESD event detected"
#define MDSS_DROPBOX_MSG_PWR_MODE_BLACK "PWR_MODE black screen detected"
#define MDSS_DROPBOX_MSG_PP_TO "Ping pong timeout"

extern struct ratelimit_state mdss_dropbox_global_rl;
void mdss_dropbox_report_event(char *msg, int count);
void mdss_dropbox_report_event_ratelimit(char *msg, int count,
					struct ratelimit_state *rs);

#endif /* MDSS_DROPBOX_H */
