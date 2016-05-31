 /*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MOT_DBA_H
#define _MOT_DBA_H

#include <video/msm_dba.h>
#include <linux/mod_display.h>
#include "msm_dba_internal.h"

struct mot_dba_device {
	struct msm_dba_device_info *dev;
	int mod_display_type;
	struct list_head list;
	bool enable;
};

int mot_dba_add_device(struct msm_dba_device_info *dev,
					int mod_display_type);
int mot_dba_device_enable(int mod_display_type);
int mot_dba_device_disable(int mod_display_type);

void mot_dba_notify_clients(enum msm_dba_callback_event event);

#endif /*_MOT_DBA_H */
