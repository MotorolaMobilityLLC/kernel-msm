/* Copyright (c) 2014, Motorola Mobility LLC. All rights reserved.
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

#define MDSS_DROPBOX_MSG_ESD "ESD event detected"
#define MDSS_DROPBOX_MSG_PWR_MODE_BLACK "PWR_MODE black screen detected"
#define MDSS_DROPBOX_MSG_PWR_MODE_FAIL "PWR_MODE read failure"
#define MDSS_DROPBOX_MSG_PWR_MODE_INVALID "PWR_MODE invalid mode detected"
#define MDSS_DROPBOX_MSG_PWR_MODE_MISMATCH "PWR_MODE mis-match sensorhub reported"
#define MDSS_DROPBOX_MSG_ESD_SENSORHUB_DROPBOX "ESD sensorhub detected"

void mdss_dropbox_report_event(char *msg, int count);
#endif /* MDSS_DROPBOX_H */
