/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
 * Copyright (c) 2013 LGE Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful;
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SMB349_H__
#define __SMB349_H__

#define SMB349_NAME	"smb349"

/**
 * struct smb349_platform_data
 * structure to pass board specific information to the smb349 charger driver
 * @chg_current_ma:     maximum fast charge current in mA
 * @en_n_gpio:          gpio to enable or disable charging
 * @chg_susp_gpio:      put active low to allow chip to suspend and disable I2C
 * @stat_gpio:          STAT pin, active low, '0' when charging.
 * @term_current_ma:    charging termination current in milliamps.
 *                      valid values are 100/200/300/400/500/600/700.
 * @wlc_support:        wireless power supply support
 */
struct smb349_platform_data {
	int en_n_gpio;
	int chg_susp_gpio;
	int chg_current_ma;
	int stat_gpio;
	int otg_en_gpio;
	int term_current_ma;
	int wlc_support;
	int ext_ovp_otg_ctrl;
};

#endif
