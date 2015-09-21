/*
 * Copyright (C) 2013-2014 Motorola Mobility LLC.
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
#include <linux/m4sensorhub_stm32_bl_cmds.h>
#include <linux/m4sensorhub_notify.h>

/* --------------- Local Declarations -------------- */
/* We flash code in sector 0 to 255 */
/* From the Flash Programming Manual:
 All sectors are 2K
*/
#define USER_FLASH_FIRST_PAGE_ADDRESS	0x08000000
#define VERSION_OFFSET  0x200
#define VERSION_ADDRESS (USER_FLASH_FIRST_PAGE_ADDRESS + VERSION_OFFSET)


#define FLASH_SIZE (1024*1024) /* 1 MB */
#define FLASH_SIZE_FOR_SW (FLASH_SIZE / 2)
#define BARKER_SIZE 8
/* Sector 255 ends at 0x0807FFFF, and we put BARKER_SIZE number of bytes as barker */

#define BARKER_ADDRESS (USER_FLASH_FIRST_PAGE_ADDRESS + FLASH_SIZE_FOR_SW - BARKER_SIZE)

#define USER_FLASH_BANK2_ADDRESS	0x08080000
#define USER_FLASH_BANK2_LAST_SECTOR    255
#define SECTOR_SIZE  0x800
#define HOST_CONFIG_SIZE 8
#define HOST_CONFIG_ADDRESS \
	(USER_FLASH_BANK2_ADDRESS + \
	(USER_FLASH_BANK2_LAST_SECTOR * SECTOR_SIZE))
#define HOST_CONFIG_SECTOR	511

/* The MAX_FILE_SIZE is the size of sectors 0 to 255 where the firmware code
 * will reside (minus the barker size). */

#define MAX_FILE_SIZE (FLASH_SIZE_FOR_SW - BARKER_SIZE)
/* Transfer is done in 256 bytes */
/* L4 allows programming ONLY double words (2 * 32-bit data) */
#define MAX_TRANSFER_SIZE	256 /* bytes */

u8 barker_buffer[BARKER_SIZE] = {0xDE, 0xC0, 0xCE, 0x0A, 0x00, 0x00, 0x00, 0x00};

/* -------------- Local Functions ------------- */
static int m4sensorhub_update_host_config(struct m4sensorhub_data *m4sensorhub)
{
	int ret = -1;
	int host_config_sector = HOST_CONFIG_SECTOR;
	int num_of_sectors_host_config = 1;
	u64 host_config_from_device;
	if (m4sensorhub_bl_rm(m4sensorhub, HOST_CONFIG_ADDRESS,
			      (u8 *)&host_config_from_device,
				HOST_CONFIG_SIZE) != 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : error reading host_config\n",
		       __func__, __LINE__);
		return -EIO;
	}

	if (host_config_from_device == m4sensorhub->host_config) {
		KDEBUG(M4SH_NOTICE, "%s: host_config match\n", __func__);
		return 0;
	}

	KDEBUG(M4SH_NOTICE, "host config differ\n");
	if (m4sensorhub_bl_erase_sectors(
			m4sensorhub, &host_config_sector,
			num_of_sectors_host_config) != 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : error erasing sectors\n",
			       __func__, __LINE__);
	}

	if (m4sensorhub_bl_wm(m4sensorhub,
			      HOST_CONFIG_ADDRESS,
					(u8 *)&(m4sensorhub->host_config),
					HOST_CONFIG_SIZE) != 0) {
		KDEBUG(M4SH_ERROR,
		       "%s : %d: error writing host_config\n",
				__func__, __LINE__);
	}

	return ret;
}

static int m4sensorhub_bl_jump_to_user(struct m4sensorhub_data *m4sensorhub)
{
	int ret = -1;
	u8 barker_read_from_device[BARKER_SIZE];
	int loop;

	if (m4sensorhub_bl_rm(m4sensorhub, BARKER_ADDRESS,
			      barker_read_from_device, BARKER_SIZE) != 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : error reading from device\n",
		       __func__, __LINE__);
		KDEBUG(M4SH_ERROR, "*** Not executing M4 code ***\n");
		return -EIO;
	}

	if (memcmp(barker_read_from_device, barker_buffer, BARKER_SIZE) == 0) {
		/* check if host_config is properly written
		 * before letting M4 run */
		/* deliberately ignoring return value from this function */
		m4sensorhub_update_host_config(m4sensorhub);
		m4sensorhub_bl_go(m4sensorhub, USER_FLASH_FIRST_PAGE_ADDRESS);
		KDEBUG(M4SH_NOTICE, "Waiting for M4 setup\n");
		/* M4 takes about 1.47s to boot up with our binary running,
		adding some cushion and sleeping for 2.5s */
		msleep(2500);
		KDEBUG(M4SH_NOTICE, "Executing M4 code\n");
		ret = 0;
	} else {
		KDEBUG(M4SH_ERROR, "Wrong Barker Number read \n");
		for (loop = 0; loop < BARKER_SIZE; loop++) {
			KDEBUG(M4SH_ERROR, "read is  0x%02x ", barker_read_from_device[loop]);
			KDEBUG(M4SH_ERROR, "expected is  0x%02x ", barker_buffer[loop]);
		}
		KDEBUG(M4SH_ERROR, "\n");
		KDEBUG(M4SH_ERROR, "*** Not executing M4 code ***\n");
	}

	return ret;
}

static int m4sensorhub_bl_erase_fw(struct m4sensorhub_data *m4sensorhub)
{
	return m4sensorhub_bl_erase_bank(m4sensorhub, BANK1);
}
/* -------------- Global Functions ----------------- */

/* m4sensorhub_l4_load_firmware()
   This function uses I2C bootloader version 1.1 on L4
   Check firmware and load if different from what's already on the L4.
   Then jump to user code on L4.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/

#define FLASHING_RETRIES 5
int m4sensorhub_l4_load_firmware(struct m4sensorhub_data *m4sensorhub,
	unsigned short force_upgrade, const struct firmware *fm)
{
	const struct firmware *firmware = fm;
	int ret = -EINVAL;
	int bytes_left, bytes_to_write;
	int address_to_write;
	u8 *buf_to_read, *buf = NULL;
	u8 *write_buf;
	u16 fw_version_file, fw_version_device;
	u8 barker_read_from_device[BARKER_SIZE];
	int loop;
	int iter;
	bool writefailed = false;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		ret = -ENODATA;
		goto done;
	}

	/* Always write MAX_TRANSFER_SIZE, even for the last chunk*/
	buf = kzalloc(MAX_TRANSFER_SIZE, GFP_KERNEL);
	if (!buf) {
		KDEBUG(M4SH_ERROR, "%s: Failed to allocate buf\n", __func__);
		ret = -ENOMEM;
		goto done;
	}

	if (firmware == NULL) {
		ret = request_firmware(&firmware, m4sensorhub->filename,
			&m4sensorhub->i2c_client->dev);
		if (ret < 0) {
			KDEBUG(M4SH_ERROR, "%s: req fw failed %s, %d\n",
			       __func__, m4sensorhub->filename, ret);
			KDEBUG(M4SH_ERROR, "run fw already on hw.\n");
			ret = 0;
			goto done;
		}
	}

	KDEBUG(M4SH_INFO, "%s: Filename = %s", __func__, m4sensorhub->filename);

	if (firmware->size > MAX_FILE_SIZE) {
		KDEBUG(M4SH_ERROR, "%s: firmware file size is too big.\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	fw_version_file = *(u16 *)(firmware->data +
		VERSION_ADDRESS - USER_FLASH_FIRST_PAGE_ADDRESS);

	for (iter = 0; iter < FLASHING_RETRIES; iter++) {
		m4sensorhub_hw_reset(m4sensorhub);
		KDEBUG(M4SH_INFO, "%s: iter = %d\n", __func__, iter);
		if (!force_upgrade) {
			/* Verify Barker number from device */
			if (m4sensorhub_bl_rm(m4sensorhub, BARKER_ADDRESS,
					      barker_read_from_device,
					BARKER_SIZE) != 0) {
				KDEBUG(M4SH_ERROR, "%s : %d : Barker fail\n",
				       __func__, __LINE__);
				continue;
			}

			if (memcmp(barker_read_from_device,
				   barker_buffer, BARKER_SIZE) != 0) {
				KDEBUG(M4SH_NOTICE, "Barker does not match\n");
				for (loop = 0; loop < BARKER_SIZE; loop++) {
					KDEBUG(M4SH_ERROR, "read 0x%02x ",
					       barker_read_from_device[loop]);
					KDEBUG(M4SH_ERROR, "expected 0x%02x ",
					       barker_buffer[loop]);
				}
				KDEBUG(M4SH_ERROR, "\n");


				KDEBUG(M4SH_NOTICE,
					"forcing firmware update from file\n");
			} else {
				/* Read firmware version from device */
				if (m4sensorhub_bl_rm(m4sensorhub,
						      VERSION_ADDRESS,
						(u8 *)&fw_version_device,
					sizeof(fw_version_device)) != 0) {
					KDEBUG(M4SH_ERROR, "Read err at ");
					KDEBUG(M4SH_ERROR, "%s : %d :\n",
					       __func__, __LINE__);
					continue;
				}

				if (fw_version_file == fw_version_device) {
					KDEBUG(M4SH_NOTICE,
					       "Version of firmware on device is 0x%04x\n",
						fw_version_device);
					KDEBUG(M4SH_NOTICE,
					       "Firmware on device same as file, not loading firmware.\n");
					ret = 0;
					goto done;
				}
				/* Print statement below isn't
				* really an ERROR, but
				* this ensures it is always printed */
				KDEBUG(M4SH_ERROR,
				       "Version of firmware on device is 0x%04x\n",
					fw_version_device);
				KDEBUG(M4SH_ERROR,
				       "Version of firmware on file is 0x%04x\n",
					fw_version_file);
				KDEBUG(M4SH_ERROR,
				       "Firmware on device different from file, updating...\n");
			}
		} else {
			KDEBUG(M4SH_NOTICE, "firmware from file is 0x%04x\n",
			       fw_version_file);
		}

		/* The flash memory to update has
		* to be erased before updating */
		ret = m4sensorhub_bl_erase_fw(m4sensorhub);
		if (ret < 0) {
			KDEBUG(M4SH_ERROR, "%s: erase failed\n", __func__);
			/* TODO : Not sure if this is critical error
			goto done; */
		}

		bytes_left = firmware->size;
		address_to_write = USER_FLASH_FIRST_PAGE_ADDRESS;
		buf_to_read = (u8 *)firmware->data;

		KDEBUG(M4SH_DEBUG, "%s: %d bytes to be written\n", __func__,
		       (int)firmware->size);

		while (bytes_left) {
			if (bytes_left > MAX_TRANSFER_SIZE) {
				bytes_to_write = MAX_TRANSFER_SIZE;
				write_buf = buf_to_read;
			} else {
				bytes_to_write = bytes_left;
				memcpy(buf, buf_to_read, bytes_to_write);
				write_buf = buf;
			}

			writefailed = false;
			if (m4sensorhub_bl_wm(m4sensorhub, address_to_write,
					      write_buf,
						  MAX_TRANSFER_SIZE) != 0) {
				KDEBUG(M4SH_ERROR, "write err addr 0x%08x at ",
				       address_to_write);
				KDEBUG(M4SH_ERROR, " %s : %d\n",
				       __func__, __LINE__);
				writefailed = true;
				break;
			}

			/* Relying on CRC to take care of errors */
			address_to_write += bytes_to_write;
			buf_to_read += bytes_to_write;
			bytes_left -= bytes_to_write;
		}

		if (writefailed == true)
			continue;

		/* Write barker number when firmware successfully written */
		if (m4sensorhub_bl_wm(m4sensorhub, BARKER_ADDRESS,
				      barker_buffer, BARKER_SIZE) != 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : error writing\n",
			       __func__, __LINE__);
			continue;
		}

		KDEBUG(M4SH_NOTICE, "%s: %d bytes written successfully\n",
		       __func__, (int)firmware->size);

		ret = 0;
		break;
	}
done:
	/* If ret is invalid, then we don't try to jump to user code */
	if (ret >= 0 && m4sensorhub_bl_jump_to_user(m4sensorhub) < 0)
		/* If jump to user code fails, return failure */
		ret = -EINVAL;

	KDEBUG(M4SH_INFO, "%s: enabling peripherals\n", __func__);
	m4sensorhub_notify_subscriber(ENABLE_PERIPHERAL);
	release_firmware(firmware);
	kfree(buf);

	/* irrespective of whether we upgraded FW or not,
	the firmware version will be the same as one in the file*/
	m4sensorhub->fw_version = fw_version_file;

	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_l4_load_firmware);

/* TODO: Restructure reflash code so that this function can go in the core */
int m4sensorhub_test_m4_reboot(struct m4sensorhub_data *m4, bool reboot_first)
{
	int err = 0;
	int i;
	unsigned char buf[2];

	if (m4 == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is missing.\n", __func__);
		err = -ENODATA;
		goto m4sensorhub_test_m4_reboot_exit;
	}

	for (i = 0; i < 3; i++) {
		if ((i > 0) || (reboot_first)) {
			m4sensorhub_hw_reset(m4);
			err = m4sensorhub_bl_jump_to_user(m4);
			if (err < 0) {
				KDEBUG(M4SH_ERROR,
					   "%s: M4 reboot failed (retries=%d)\n",
					   __func__, i);
				continue;
			}
		}

		/* Wait progressively longer for M4 to become ready */
		if (i > 0)
			msleep(i * 100);

		/* Read M4 register to test if M4 is ready */
		buf[0] = 0x00; /* Bank */
		buf[1] = 0x00; /* Offset */
		err = m4sensorhub_i2c_write_read(m4, &(buf[0]), 2, 1);
		if (err < 0) {
			KDEBUG(M4SH_ERROR, "%s: %s (retries=%d).\n", __func__,
				   "Failed initial I2C read", i);
			continue;
		} else {
			/* No failure so break loop */
			err = 0;
			break;
		}
	}

	if (err < 0)
		panic("%s: M4 has failed--forcing panic...\n", __func__);

m4sensorhub_test_m4_reboot_exit:
	return err;
}
EXPORT_SYMBOL_GPL(m4sensorhub_test_m4_reboot);
