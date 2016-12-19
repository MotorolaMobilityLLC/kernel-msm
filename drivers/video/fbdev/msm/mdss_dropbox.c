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
#include <linux/dropbox.h>
#include <linux/string.h>

#include "mdss_dropbox.h"

#define DROPBOX_DISPLAY_ISSUE "display_issue"

DEFINE_RATELIMIT_STATE(mdss_dropbox_global_rl, 3600 * HZ, 1);

void mdss_dropbox_report_event(char *msg, int count)
{
	char dropbox_entry[256];

	snprintf(dropbox_entry, sizeof(dropbox_entry), "%s\ncount: %d",
		msg, count);

	pr_err("%s: display issue detected[count: %d][%s], reporting dropbox\n",
		__func__, count, msg);
	dropbox_queue_event_text(DROPBOX_DISPLAY_ISSUE,
				dropbox_entry, strlen(dropbox_entry));
}

void mdss_dropbox_report_event_ratelimit(char *msg, int count,
					struct ratelimit_state *rs)
{
	if (!__ratelimit(rs))
		return;

	mdss_dropbox_report_event(msg, count);
}
