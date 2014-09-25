/*
 * Copyright (C) 2012-2014 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/m4sensorhub.h>
#include <linux/slab.h>

/* --------------- Global Declarations -------------- */

/* ------------ Local Function Prototypes ----------- */
static int m4sensorhub_jump_to_user(struct m4sensorhub_data *m4sensorhub);

/* --------------- Local Declarations -------------- */
/* The M4 Flash Memory Map
 * From the Flash Programming Manual:
  Sector  Start Address    Size              comments
  ------  ------------- ----------  ----------------------------
  0       0x08000000    16 Kbytes   reserved for M4 bootload
  1       0x08004000    16 Kbytes   first M4 firmware code page
  2       0x08008000    16 Kbytes
  3       0x0800C000    16 Kbytes
  4       0x08010000    64 Kbytes
  5       0x08020000    128 Kbytes  last M4 firmware code page
  6       0x08040000    128 Kbytes  first M4 file system page
  7       0x08060000    128 Kbytes
  8       0x08080000    128 Kbytes
  9       0x080A0000    128 Kbytes
  10      0x080C0000    128 Kbytes
  11      0x080E0000    128 Kbytes  last M4 file system page
 */
enum {
	M4_FLASH_SECTOR0 = 0x08000000,
	M4_FLASH_SECTOR1 = 0x08004000,
	M4_FLASH_SECTOR2 = 0x08008000,
	M4_FLASH_SECTOR3 = 0x0800C000,
	M4_FLASH_SECTOR4 = 0x08010000,
	M4_FLASH_SECTOR5 = 0x08020000,
	M4_FLASH_SECTOR6 = 0x08040000,
	M4_FLASH_SECTOR7 = 0x08060000,
	M4_FLASH_SECTOR8 = 0x08080000,
	M4_FLASH_SECTOR9 = 0x080A0000,
	M4_FLASH_SECTORA = 0x080C0000,
	M4_FLASH_SECTORB = 0x080E0000,
	M4_FLASH_END     = 0x08100000
};
#define USER_FLASH_FIRST_PAGE_ADDRESS	M4_FLASH_SECTOR1
#define USER_FLASH_FIRST_FILE_ADDRESS	M4_FLASH_SECTOR6
#define VERSION_OFFSET	0x200
#define VERSION_ADDRESS	(USER_FLASH_FIRST_PAGE_ADDRESS + VERSION_OFFSET)
#define BARKER_SIZE	4
#define BARKER_ADDRESS	(USER_FLASH_FIRST_FILE_ADDRESS - BARKER_SIZE)
#define BARKER_NUMBER	0xACEC0DE
/* The MAX_FILE_SIZE is the size of sectors 1-5 where the firmware code
 * will reside (minus the barker size).
 */
#define MAX_FILE_SIZE	(USER_FLASH_FIRST_FILE_ADDRESS \
			- USER_FLASH_FIRST_PAGE_ADDRESS \
			- BARKER_SIZE) /* bytes */
#define MAX_TRANSFER_SIZE	1024 /* bytes */
#define MAX_RETRIES	5
#define OPC_READ	(uint8_t)(0x03)
#define OPC_WREN	(uint8_t)(0x06)
#define OPC_ERPG	(uint8_t)(0x20)
#define OPC_ERUSM	(uint8_t)(0x60)
#define OPC_USRCD	(uint8_t)(0x77)

/* -------------- Local Data Structures ------------- */
#define NUM_FLASH_TO_ERASE	5
int flash_address[NUM_FLASH_TO_ERASE] = {
	M4_FLASH_SECTOR1,
	M4_FLASH_SECTOR2,
	M4_FLASH_SECTOR3,
	M4_FLASH_SECTOR4,
	M4_FLASH_SECTOR5
};
int flash_delay[NUM_FLASH_TO_ERASE] = {
	440, 440, 440, 1320, 4000
};

/* -------------- Global Functions ----------------- */

/* m4sensorhub_load_firmware()

   Check firmware and load if different from what's already on the M4.
   Then jump to user code on M4.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/

int m4sensorhub_load_firmware(struct m4sensorhub_data *m4sensorhub,
	unsigned short force_upgrade,
	const struct firmware *fm)
{
	const struct firmware *firmware = fm;
	int i = MAX_RETRIES;
	int ret = 0;
	int bytes_left, bytes_to_write;
	int address_to_write;
	u8 *buf_to_read, *buf = NULL;
	u16 fw_version_file, fw_version_device;
	u32 barker_read_from_device;
	int j = 0;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		ret = -ENODATA;
		goto done;
	}

	m4sensorhub_hw_reset(m4sensorhub);

	buf = kzalloc(MAX_TRANSFER_SIZE+8, GFP_KERNEL);
	if (!buf) {
		KDEBUG(M4SH_ERROR, "%s: Failed to allocate buf\n", __func__);
		ret = -ENOMEM;
		goto done;
	}
	if (firmware == NULL) {
		ret = request_firmware(&firmware, m4sensorhub->filename,
			&m4sensorhub->i2c_client->dev);
	}
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "%s: request_firmware failed for %s\n",
			__func__, m4sensorhub->filename);
		KDEBUG(M4SH_ERROR, "Trying to run firmware already on hw.\n");
		ret = 0;
		goto done;
	}
	KDEBUG(M4SH_INFO, "%s: Firmware = %s\n",
		__func__, m4sensorhub->filename);

	if (firmware->size > MAX_FILE_SIZE) {
		KDEBUG(M4SH_ERROR, "%s: firmware file size is too big.\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	fw_version_file = *(u16 *) (firmware->data +
		VERSION_ADDRESS - USER_FLASH_FIRST_PAGE_ADDRESS);

	if (!force_upgrade) {
		/* Verify Barker number from device */
		buf[0] = OPC_READ;
		buf[1] = (BARKER_ADDRESS >> 24) & 0xFF;
		buf[2] = (BARKER_ADDRESS >> 16) & 0xFF;
		buf[3] = (BARKER_ADDRESS >> 8) & 0xFF;
		buf[4] = BARKER_ADDRESS & 0xFF;
		buf[5] = 0x00;
		buf[6] = 0x04;
		if (m4sensorhub_i2c_write_read(m4sensorhub,
				buf, 7, 0) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto done;
		}

		msleep(100);

		if (m4sensorhub_i2c_write_read(m4sensorhub,
			(u8 *) &barker_read_from_device, 0, BARKER_SIZE) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto done;
		}

		if (barker_read_from_device != BARKER_NUMBER) {
			KDEBUG(M4SH_NOTICE,
				"Barker Number read 0x%8x does not match 0x%8x\n",
				barker_read_from_device, BARKER_NUMBER);
			KDEBUG(M4SH_NOTICE,
				"forcing firmware update from file\n");
		} else {

			/* Read firmware version from device */
			buf[0] = OPC_READ;
			buf[1] = (VERSION_ADDRESS >> 24) & 0xFF;
			buf[2] = (VERSION_ADDRESS >> 16) & 0xFF;
			buf[3] = (VERSION_ADDRESS >> 8) & 0xFF;
			buf[4] = VERSION_ADDRESS & 0xFF;
			buf[5] = 0x00;
			buf[6] = 0x02;
			if (m4sensorhub_i2c_write_read(
					m4sensorhub, buf, 7, 0) < 0) {
				KDEBUG(M4SH_ERROR,
					"%s : %d : I2C transfer error\n",
					__func__, __LINE__);
				ret = -EINVAL;
				goto done;
			}

			msleep(100);

			if (m4sensorhub_i2c_write_read(m4sensorhub,
					(u8 *) &fw_version_device, 0, 2) < 0) {
				KDEBUG(M4SH_ERROR,
					"%s : %d : I2C transfer error\n",
					__func__, __LINE__);
				ret = -EINVAL;
				goto done;
			}

			if (fw_version_file == fw_version_device) {
				KDEBUG(M4SH_NOTICE,
					"Version of firmware on device is 0x%04x\n",
					fw_version_device);
				KDEBUG(M4SH_NOTICE,
					"Firmware on device same as file, not loading firmware.\n");
				goto done;
			}
			/* Print statement below isn't really an ERROR, but
			 * this ensures it is always printed */
			KDEBUG(M4SH_ERROR,
				"Version of firmware on device is 0x%04x\n",
				fw_version_device);
			KDEBUG(M4SH_ERROR,
				"Version of firmware on file is 0x%04x\n",
				fw_version_file);
			KDEBUG(M4SH_ERROR,
				"Firmware on device different from file,"
				"updating...\n");
		}
	} else {
		KDEBUG(M4SH_NOTICE, "Version of firmware on file is 0x%04x, "
			"updating...\n",
			fw_version_file);
	}

	/* The flash memory to update has to be erased before updating */
	for (j = 0; j < NUM_FLASH_TO_ERASE; j++) {
		buf[0] = OPC_ERPG;
		buf[1] = (flash_address[j] >> 24) & 0xFF;
		buf[2] = (flash_address[j] >> 16) & 0xFF;
		buf[3] = (flash_address[j] >> 8) & 0xFF;
		buf[4] = flash_address[j] & 0xFF;
		buf[5] = 0x00;
		buf[6] = 0x01;
		if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 7, 0) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			KDEBUG(M4SH_ERROR, "Erasing %8x address failed\n",
				flash_address[j]);
			ret = -EINVAL;
			goto done;
		}
		msleep(flash_delay[j]);
	}

	bytes_left = firmware->size;
	address_to_write = USER_FLASH_FIRST_PAGE_ADDRESS;
	buf_to_read = (u8 *) firmware->data;

	KDEBUG(M4SH_DEBUG, "%s: %d bytes to be written\n", __func__,
		firmware->size);

	while (bytes_left && i) {
		if (bytes_left > MAX_TRANSFER_SIZE)
			bytes_to_write = MAX_TRANSFER_SIZE;
		else
			bytes_to_write = bytes_left;

		buf[0] = OPC_WREN;
		buf[1] = (address_to_write >> 24) & 0xFF;
		buf[2] = (address_to_write >> 16) & 0xFF;
		buf[3] = (address_to_write >> 8) & 0xFF;
		buf[4] = address_to_write & 0xFF;
		buf[5] = (bytes_to_write >> 8) & 0xFF;
		buf[6] = bytes_to_write & 0xFF;
		buf[7] = 0xFF;
		memcpy(&buf[8], buf_to_read, bytes_to_write);
		if (m4sensorhub_i2c_write_read(m4sensorhub, buf,
				bytes_to_write+8, 0) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto done;
		}

		msleep(20);

		/* Read back the code that was written and validate it */
		buf[0] = OPC_READ;
		buf[1] = (address_to_write >> 24) & 0xFF;
		buf[2] = (address_to_write >> 16) & 0xFF;
		buf[3] = (address_to_write >> 8) & 0xFF;
		buf[4] = address_to_write & 0xFF;
		buf[5] = (bytes_to_write >> 8) & 0xFF;
		buf[6] = bytes_to_write & 0xFF;
		if (m4sensorhub_i2c_write_read(
				m4sensorhub, buf, 7, 0) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto done;
		}

		if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 0,
				bytes_to_write) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto done;
		}

		if (memcmp(buf, buf_to_read, bytes_to_write) != 0) {
			/* If memory write fails, try again */
			KDEBUG(M4SH_ERROR,
				"memory write to 0x%x of %d bytes failed, try again\n",
				address_to_write, bytes_to_write);
			i--;
		} else {
			address_to_write += bytes_to_write;
			buf_to_read += bytes_to_write;
			bytes_left -= bytes_to_write;
			/* Reset reter of retries */
			i = MAX_RETRIES;
		}
	}

	if (!i) {
		KDEBUG(M4SH_ERROR, "%s: firmware transfer failed\n", __func__);
		ret = -EINVAL;
	} else {
		/* Write barker number when firmware successfully written */
		buf[0] = OPC_WREN;
		buf[1] = (BARKER_ADDRESS >> 24) & 0xFF;
		buf[2] = (BARKER_ADDRESS >> 16) & 0xFF;
		buf[3] = (BARKER_ADDRESS >> 8) & 0xFF;
		buf[4] = BARKER_ADDRESS & 0xFF;
		buf[5] = 0x00;
		buf[6] = 0x04;
		buf[7] = 0xFF;
		buf[8] = BARKER_NUMBER & 0xFF;
		buf[9] = (BARKER_NUMBER >> 8) & 0xFF;
		buf[10] = (BARKER_NUMBER >> 16) & 0xFF;
		buf[11] = (BARKER_NUMBER >> 24) & 0xFF;
		if (m4sensorhub_i2c_write_read(m4sensorhub,
				buf, 12, 0) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto done;
		}

		KDEBUG(M4SH_NOTICE, "%s: %d bytes written successfully\n",
			__func__, firmware->size);
	}

done:
	release_firmware(firmware);

	/* If ret is invalid, then we don't try to jump to user code */
	if (ret >= 0 && m4sensorhub_jump_to_user(m4sensorhub) < 0)
		/* If jump to user code fails, return failure */
		ret = -EINVAL;

	kfree(buf);

	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_load_firmware);


/* -------------- Local Functions ----------------- */

static int m4sensorhub_jump_to_user(struct m4sensorhub_data *m4sensorhub)
{
	int ret = -1;
	u8 buf[7] = {0};
	u32 barker_read_from_device = 0;

	buf[0] = OPC_READ;
	buf[1] = (BARKER_ADDRESS >> 24) & 0xFF;
	buf[2] = (BARKER_ADDRESS >> 16) & 0xFF;
	buf[3] = (BARKER_ADDRESS >> 8) & 0xFF;
	buf[4] = BARKER_ADDRESS & 0xFF;
	buf[5] = 0x00;
	buf[6] = 0x04;
	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 7, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return ret;
	}

	msleep(100);

	if (m4sensorhub_i2c_write_read(m4sensorhub,
			(u8 *) &barker_read_from_device, 0, BARKER_SIZE) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return ret;
	}

	if (barker_read_from_device == BARKER_NUMBER) {
		buf[0] = OPC_USRCD;
		if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 1, 0) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			return ret;
		}
		KDEBUG(M4SH_NOTICE, "Executing M4 code\n");
		msleep(5000); /* 5 secs delay */
		ret = 0;
	} else {
		KDEBUG(M4SH_ERROR,
			"Barker Number read 0x%8x does not match 0x%8x\n",
			barker_read_from_device, BARKER_NUMBER);
		KDEBUG(M4SH_ERROR, "*** Not executing M4 code ***\n");
	}

	return ret;
}
