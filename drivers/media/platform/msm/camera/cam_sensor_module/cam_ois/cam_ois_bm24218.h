/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#ifndef _CAM_OIS_BM24218_H_
#define _CAM_OIS_BM24218_H_

#include "cam_ois_dev.h"

int cam_ois_bm24218_fw_download(struct cam_ois_ctrl_t *o_ctrl);
int cam_ois_bm24218_after_cal_data_dl(struct cam_ois_ctrl_t *o_ctrl);

#endif
