/* < DTS2013062605264 sunlibin 20130702 begin */
/* add cypress new driver ttda-02.03.01.476713 */
/*
 * cyttsp4_test_device_access_api.c
 * Cypress TrueTouch(TM) Standard Product V4 Device Access API test module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
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

#include <linux/module.h>
#include <linux/cyttsp4_device_access-api.h>

#define CONFIG_VER_OFFSET	8
#define CONFIG_VER_SIZE		2

#define BUFFER_SIZE		256

static u8 buffer[BUFFER_SIZE];
static int active_refresh_interval;

static int __init cyttsp4_test_device_access_api_init(void)
{
	u16 config_ver;
	int i;
	int j;
	int value;
	int rc;

	pr_info("%s: Enter\n", __func__);

	/*
	 * CASE 1 - Get CONFIG_VER and update it
	 */

	/*
	 * Get CONFIG_VER
	 * Group 6 read requires to fetch from offset to the end of row
	 * The return buffer should be at least read config block command
	 * return size + config row size bytes
	 */
	rc = cyttsp4_device_access_read_command(NULL,
			GRPNUM_TOUCH_CONFIG, CONFIG_VER_OFFSET,
			buffer, BUFFER_SIZE);
	if (rc < 0) {
		pr_err("%s: cyttsp4_device_access_read_command failed, rc=%d\n",
			__func__, rc);
		goto exit;
	}

	pr_info("%s: cyttsp4_device_access_read_command returned %d bytes\n",
		__func__, rc);

	/* Calculate CONFIG_VER (Little Endian) */
	config_ver = buffer[0] + (buffer[1] << 8);

	pr_info("%s: Old CONFIG_VER:%04X New CONFIG_VER:%04X\n", __func__,
		config_ver, config_ver + 1);

	config_ver++;

	/* Store CONFIG_VER (Little Endian) */
	buffer[0] = config_ver & 0xFF;
	buffer[1] = config_ver >> 8;

	/*
	 * Set CONFIG_VER
	 * Group 6 write supports writing arbitrary number of bytes
	 */
	rc = cyttsp4_device_access_write_command(NULL,
			GRPNUM_TOUCH_CONFIG, CONFIG_VER_OFFSET,
			buffer, CONFIG_VER_SIZE);
	if (rc < 0) {
		pr_err("%s: cyttsp4_device_access_write_command failed, rc=%d\n",
			__func__, rc);
		goto exit;
	}

	/*
	 * CASE 2 - Get Operational mode parameters
	 */
	for (i = OP_PARAM_ACTIVE_DISTANCE;
			i <= OP_PARAM_ACTIVE_LOOK_FOR_TOUCH_INTERVAL; i++) {
		buffer[0] = OP_CMD_GET_PARAMETER;
		buffer[1] = i;

		rc = cyttsp4_device_access_write_command(NULL,
				GRPNUM_OP_COMMAND, 0, buffer, 2);
		if (rc < 0) {
			pr_err("%s: cyttsp4_device_access_write_command failed, rc=%d\n",
				__func__, rc);
			goto exit;
		}

		/*
		 * The return buffer should be at least
		 * number of command data registers + 1
		 */
		rc = cyttsp4_device_access_read_command(NULL,
				GRPNUM_OP_COMMAND, 0, buffer, 7);
		if (rc < 0) {
			pr_err("%s: cyttsp4_device_access_read_command failed, rc=%d\n",
				__func__, rc);
			goto exit;
		}

		if (buffer[0] != OP_CMD_GET_PARAMETER || buffer[1] != i) {
			pr_err("%s: Invalid response\n", __func__);
			rc = -EINVAL;
			goto exit;
		}

		/*
		 * Get value stored starting at &buffer[3] whose
		 * size (in bytes) is specified at buffer[2]
		 */
		value = 0;
		j = 0;
		while (buffer[2]--)
			value += buffer[3 + j++] << (8 * buffer[2]);

		/* Store Active mode refresh interval to restore */
		if (i == OP_PARAM_REFRESH_INTERVAL)
			active_refresh_interval = value;

		pr_info("%s: Parameter %02X: %d\n", __func__, i, value);
	}

	/*
	 * CASE 3 - Set Active mode refresh interval to 200 ms
	 */
	buffer[0] = OP_CMD_SET_PARAMETER; /* Set Parameter */
	buffer[1] = OP_PARAM_REFRESH_INTERVAL; /* Refresh Interval parameter */
	buffer[2] = 1; /* Parameter length - 1 byte */
	buffer[3] = 200; /* 200 ms */

	rc = cyttsp4_device_access_write_command(NULL,
			GRPNUM_OP_COMMAND, 0, buffer, 4);
	if (rc < 0) {
		pr_err("%s: cyttsp4_device_access_write_command failed, rc=%d\n",
			__func__, rc);
		goto exit;
	}

exit:
	return 0;
}
module_init(cyttsp4_test_device_access_api_init);

static void __exit cyttsp4_test_device_access_api_exit(void)
{
	int rc;

	pr_info("%s: Exit\n", __func__);

	/*
	 * CASE 4 - Restore Active mode refresh interval to original
	 */
	if (active_refresh_interval) {
		buffer[0] = OP_CMD_SET_PARAMETER; /* Set Parameter */
		buffer[1] = OP_PARAM_REFRESH_INTERVAL;
					/* Refresh Interval parameter */
		buffer[2] = 1; /* Parameter length - 1 byte */
		buffer[3] = (u8)active_refresh_interval;

		rc = cyttsp4_device_access_write_command(NULL,
				GRPNUM_OP_COMMAND, 0, buffer, 4);
		if (rc < 0) {
			pr_err("%s: cyttsp4_device_access_write_command failed, rc=%d\n",
				__func__, rc);
		}
	}
}
module_exit(cyttsp4_test_device_access_api_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen device access API tester");
MODULE_AUTHOR("Cypress Semiconductor");

/* DTS2013062605264 sunlibin 20130702 end > */
