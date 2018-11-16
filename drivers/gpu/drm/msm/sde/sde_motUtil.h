/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MOTUTIL_H__
#define __MOTUTIL_H__

#include <linux/debugfs.h>
#include <linux/types.h>

/*****************************************************************************
 * /d/dri/0/debug/motUtil sysfs                                              *
 * - This is Mot debug sysfs to provide some tools to test panel and panel   *
 *   features. This tool only exist with the userdebug and Engineering build *
 * - Currently, this motUtil will provide 3 kind of tests and it can enhance *
 *   to test more if needed                                                  *
 *   - Test#1: DispUtil: This test is able to send any MIPI DSI command and  *
 *             it can read any data back from the panel                      *
 *   - Test#2: TeTest: This test will be used to test the TE trace between   *
 *             the panels and MDSS to make sure it works                     *
 *   - Test#3: kmsPropTest: This test will test the panel features such as   *
 *             HBM, ACL, CABC, etc. It can read these features configurations*
 *             in the KMS property and it can change these setting from KMS  *
 *             property.                                                     *
 * - Usage:                                                                  *
 *   - 1st byte will indicate which motUtil test will be run.                *
 *           0x00 = dispUtil, 0x01 = TeTest, 0x02 = kmsPropTest              *
 * --------------------------------------------------------------------------*
 *   - dispUtil:                                                             *
 *     1st byte: 0x00: dispUtil                                              *
 *     2nd byte: 0x00: primary panel and 0x01: Secondary panel               *
 *     3rd byte: 0x00: MIPI write cmd to panel and 0x01: MIPI read           *
 *     4th byte: 0x00: MIPI HS mode and 0x01: MIPI LP mode                   *
 *     5th byte: 0x00: MIPI DCS cmd and 0x01: MIPI generaic cmd              *
 *     6th byte: Indicate number of bytes to read from panel                 *
 *     7th and 8th: Indicate number of bytes for the payload cmd             *
 *     9th and .. :the payload cmd                                           *
 *     - Example:                                                            *
 *     echo "0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x01 0x28" > motUtil         *
 *       - This will send a mipi DSC command "0x28" to panel and             *
 *         which will request the panel to turn off the panel                *
 *     cat motUtil                                                           *
 *       - This will report a sending previous cmd status                    *
 *         0: send success or errno if it fails                              *
 *     echo "0x00 0x01 0x01 0x00 0x00 0x01 0x00 0x01 0x0a" > motUtil         *
 *       - This will send MIPI read cmd "0xa" to the panel.                  *
 *     cat motUtil                                                           *
 *       - number of reading bytes from previous command or errno if it fails*
 *---------------------------------------------------------------------------*
 *  - TeTest:                                                                *
 *    1st byte: TeTest                                                       *
 *    2nd byte: 0x00: Primary panel and 0x01: Secondary panel                *
 *    3rd byte: 0x00: Disable TE, 0x01: Enable TE                            *
 *              0x02: Start TeTest, 0x03 End TeTest                          *
 *    - Example:                                                             *
 *      echo "0x01 0x00 0x02" > motUtil                                      *
 *        - This STARTs the TeTest which will change pp_sync_config_height   *
 *          and enable the DSI clks                                          *
 *      echo "0x01 0x00 0x01" > motUtil                                      *
 *        - This will run the "TE enable testcase"                           *
 *      cat motUtil                                                          *
 *        - This will return number of TE signal the MDSS receive from panel *
 *      cat motUtil                                                          *
 *        - This will return the NEW number of TE signal                     *
 *      echo "0x01 0x00 0x00" > motUtil                                      *
 *        - This will run the "TE disable testcase"                          *
 *      cat motUtil                                                          *
 *        - This will return number of TE signal the MDSS receive from panel *
 *          which will not change before the "TE disable testcase"           *
 *      echo "0x01 0x00 0x03" > motUtil                                      *
 *        - This ENDs the TeTest which will change pp_sync_config_height back*
 *          and disable the DSI clks                                         *
 *---------------------------------------------------------------------------*
 *  - kmsPropTest                                                            *
 *    1st byte: kmsPropTest                                                  *
 *    2nd byte: 0x00: primary panel and 0x01: Secondary panel                *
 *    3rd byte: 0x00: get KMS property and 0x01: set KMS property            *
 *    4th byte: 0x00: HBM 0x01: ACL for the KMS property/panel feature       *
 *    5th byte: new data for a "set KMS property" operation                  *
 *    - Example                                                              *
 *    echo "0x02 0x00 0x010 0x00 0x00" > motUtil                             *
 *      - This will read the HBM setting of the primary panel in the KMS     *
 *        property.                                                          *
 *    cat motUtil                                                            *
 *     - This will return the status of previous cmd.                        *
 *         0 if success or errno if it fails                                 *
 *     echo "0x02 0x00 0x01 0x00 0x01" > motUtil                             *
 *      - This will turn on the HBM feature of the primary panel in the KMS  *
 *        property. The panel's backlight will be brighter when HBM is on    *
 *    cat motUtil                                                            *
 *     - This will return the status of previous cmd.                        *
 *         0 if success or errno if it fails                                 *
 *    echo "0x02 0x00 0x010 0x00 0x00" > motUtil                             *
 *     - This will read back the HBM setting in KMS property                 *
 *    echo "0x02 0x00 0x01 0x00 0x00" > motUtil                              *
 *      - This will turn off the HBM feature of the primary panel in the KMS *
 *        property. The panel's backlight will be dimmer when HBM is off     *
 *                                                                           *
 ****************************************************************************/

enum sde_motUtil_type {
        MOTUTIL_DISP_UTIL = 0,
        MOTUTIL_TE_TEST,
	MOTUTIL_KMS_PROP_TEST,
};

enum sde_motUtil_disp_cmd {
	DISPUTIL_TYPE = 0,
	DISPUTIL_PANEL_TYPE,
	DISPUTIL_CMD_TYPE,
	DISPUTIL_CMD_XFER_MODE,
	DISPUTIL_MIPI_CMD_TYPE,
	DISPUTIL_NUM_BYTE_RD,
	DISPUTIL_PAYLOAD_LEN_U,
	DISPUTIL_PAYLOAD_LEN_L,
	DISPUTIL_PAYLOAD,
};

enum sde_motUtil_teTest_cmd {
	TETEST_TYPE,
	TETEST_PANEL_TYPE,
	TETEST_TE_TEST_TYPE,
};

enum sde_motutil_teTest_type {
	TETEST_TE_ENABLE = 0,
	TETEST_TE_DISABLE,
	TETEST_TE_TEST_START,
	TETEST_TE_TEST_END,
};

enum sde_motUtil_kmsPropTest_cmd {
	KMSPROPTEST_TYPE,
	KMSPROPTEST_PANEL_TYPE,
	KMSPROPTEST_PROP_TYPE,
	KMSPROPTEST_PROP_INDEX,
	KMSPROPTEST_NEW_VAL,
};

enum sde_motUtil_kmsPropTest_ConnPropType {
	KMSPROPTEST_TYPE_HBM,
	KMSPROPTEST_TYPE_ACL,
	KMSPROPTEST_TYPE_CABC,
};

struct motUtil {
	struct mutex lock;
	enum sde_motUtil_type motUtil_type;
	bool read_cmd;
	int cmd_status;
	u16 read_len;
	u8 *rd_buf;
	int val;
	bool te_enable;
};

#define DISPUTIL_DSI_WRITE      0
#define DISPUTIL_DSI_READ       1

#define DISPUTIL_DSI_DCS        0
#define DISPUTIL_DSI_GENERIC    1

#define DISPUTIL_DSI_HS_MODE    0
#define DISPUTIL_DSI_LP_MODE    1

#define KMSPROPTEST_GETPROP	0
#define KMSPROPTEST_SETPROP	1

#define MOTUTIL_MAIN_DISP	0
#define MOTUTIL_CLI_DISP	1

int sde_debugfs_mot_util_init(struct sde_kms *sde_kms,
			struct dentry *parent);

#endif /* __MOTUTIL_H__ */
