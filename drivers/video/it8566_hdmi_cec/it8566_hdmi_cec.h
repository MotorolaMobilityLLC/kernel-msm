/*
 * ITE it8566 HDMI CEC driver
 *
 * Copyright(c) 2014 ASUSTek COMPUTER INC. All rights reserved.
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

#ifndef _IT8566_HDMI_CEC_H
#define _IT8566_HDMI_CEC_H
extern struct i2c_client *it8566_get_cec_client(void);
extern struct dentry *it8566_get_debugfs_dir(void);
#endif
