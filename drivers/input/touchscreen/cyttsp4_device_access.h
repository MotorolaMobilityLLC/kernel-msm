/* add cypress new driver ttda-02.03.01.476713 */

/*
 * cyttsp4_device_access.h
 * Cypress TrueTouch(TM) Standard Product V4 Device Access module.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_DEVICE_ACCESS_H
#define _LINUX_CYTTSP4_DEVICE_ACCESS_H

#define CYTTSP4_DEVICE_ACCESS_NAME "cyttsp4_device_access"

#define CYTTSP4_INPUT_ELEM_SZ (sizeof("0xHH") + 1)
#define CYTTSP4_TCH_PARAM_SIZE_BLK_SZ 128

/* Timeout values in ms. */
#define CY_DA_REQUEST_EXCLUSIVE_TIMEOUT	5000

struct cyttsp4_device_access_platform_data {
	char const *device_access_dev_name;
};

#define CY_CMD_IN_DATA_OFFSET_VALUE 0
#define CY_CMD_LCL_IDAC_OFFSET 1

#define CY_CMD_OUT_STATUS_OFFSET 0
#define CY_CMD_RET_PNL_OUT_ELMNT_SZ_OFFS_H 2
#define CY_CMD_RET_PNL_OUT_ELMNT_SZ_OFFS_L 3
#define CY_CMD_RET_PNL_OUT_DATA_FORMAT_OFFS 4

#define CY_CMD_RET_PANEL_ELMNT_SZ_MASK 0x07
#define I2C_BUF_MAX_SIZE 250

enum cyttsp4_scan_data_type {
	CY_MUT_RAW,
	CY_MUT_BASE,
	CY_MUT_DIFF,
	CY_SELF_RAW,
	CY_SELF_BASE,
	CY_SELF_DIFF,
	CY_BAL_RAW,
	CY_BAL_BASE,
	CY_BAL_DIFF,
};

enum check_data_type {
	CY_CHK_MUT_RAW,
	CY_CHK_SELF_RAW,
	CY_CHK_MUT_IDAC,
	CY_CHK_SELF_IDAC,
};
typedef enum {
	CY_TMD445 = 0,
	CY_TMD463 = 1,
	CY_TMDUNKNOW
} cy_tp_ic_version;
/* tp capacitance infomation */
/* add a element of self capcitance number */
typedef struct cypress4_tp_cap_info_ {
	int min_mut_cap;
	int max_mut_cap;
	int min_self_cap;
	int max_self_cap;
	int tx_lines;
	int rx_lines;
	int self_cap_num;
	bool data_ok;
	u16 *ignore_list_self_cap;
	u16 *ignore_list_mut_cap;
	int ignore_list_size_self_cap;
	int ignore_list_size_mut_cap;
	int data_start_byte;
	cy_tp_ic_version tp_ic_version;
} cypress4_tp_cap_info;

#define CYP_CAP_NUM_MAX     20

/*move to hw_tp_common.c*/


#endif /* _LINUX_CYTTSP4_DEVICE_ACCESS_H */

