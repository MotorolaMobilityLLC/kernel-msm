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
/* --------------- Global Declarations -------------- */

/* ------------ Local Function Prototypes ----------- */
static int m4sensorhub_bl_ack(struct m4sensorhub_data *m4sensorhub);
static int m4sensorhub_bl_rm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len);
static int m4sensorhub_bl_go(struct m4sensorhub_data *m4sensorhub,
	int start_address);
static int m4sensorhub_bl_wm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len);
static int m4sensorhub_bl_erase_fw(struct m4sensorhub_data *m4sensorhub);
static int m4sensorhub_jump_to_user(struct m4sensorhub_data *m4sensorhub);

/* --------------- Local Declarations -------------- */

#define ACK		((uint8_t) (0x79))
#define NACK	((uint8_t) (0x1F))
#define BUSY    ((uint8_t) (0x76))

/* LIST OF OPCODES FOR BOOTLOADER */
/* opcode_get */
#define OPC_GET	((uint8_t) (0x00))
/* opcode_get_version */
#define OPC_GV	((uint8_t) (0x01))
/* opcode_get_id */
#define OPC_GID	((uint8_t) (0x02))
/* opcode_read_memory */
#define OPC_RM	((uint8_t) (0x11))
/* opcode_go */
#define OPC_GO	((uint8_t) (0x21))
/* opcode_write_memory */
#define OPC_WM	((uint8_t) (0x31))
/* opcode_no_stretch_write_memory */
#define OPC_NO_STRETCH_WM	((uint8_t) (0x32))
/* opcode_erase */
#define OPC_ER	((uint8_t) (0x44))
/* opcode_no_stretch_erase */
#define OPC_NO_STRETCH_ER	((uint8_t) (0x45))
/* opcode_write_protect */
#define OPC_WP	((uint8_t) (0x63))
/* opcode_write_unprotect */
#define OPC_WU	((uint8_t) (0x73))
/* opcode_readout_protect */
#define OPC_RP	((uint8_t) (0x82))
/* opcode_readout_unprotect */
#define OPC_RU	((uint8_t) (0x92))
/* opcode_no_stretch_readout_unprotect */
#define OPC_NO_STRETCH_RU	((uint8_t) (0x93))

/* We flash code in sector 6 and sector 7 */
#define USER_FLASH_FIRST_PAGE_ADDRESS	0x08040000
#define VERSION_OFFSET  0x200
#define VERSION_ADDRESS (USER_FLASH_FIRST_PAGE_ADDRESS + VERSION_OFFSET)

/* Sector 7 ends at 0x0807FFFF, and we put 4 byte barker */
/* at the end of Sector 7, giving BARKER_ADDRESS as (0x0807FFFF - 0x4 + 0x1) */
#define BARKER_ADDRESS	0x0807FFFC
#define BARKER_NUMBER	0xACEC0DE
/* The MAX_FILE_SIZE is the size of sectors 6 and 7  where the firmware code
 * will reside (minus the barker size).

 * From the Flash Programming Manual:
  Sector  Start Address End Address Size
  ------  ------------- ----------------
  0       0x0800 0000   0x0800 3FFF 16 Kbytes
  1       0x0800 4000   0x0800 7FFF 16 Kbytes
  2       0x0800 8000   0x8000 BFFF 16 Kbytes
  3       0x0800 C000   0x0800 FFFF 16 Kbytes
  4       0x0801 0000   0x0801 FFFF 64 Kbytes
  5       0x0802 0000   0x0803 FFFF 128 Kbytes
  6       0x0804 0000   0x0805 FFFF 128 Kbytes
  7       0x0806 0000   0x0807 FFFF 128 Kbytes

 */
/* Max file size is 128Kb + 128 Kb - 4 bytes for barker */
#define MAX_FILE_SIZE	(128*1024+128*1024-4) /* bytes */
/* Transfer is done in 256 bytes, or if data is less than 256 bytes, then
data is 16 bit aligned and sent out */
#define MAX_TRANSFER_SIZE	256 /* bytes */
/* If M4 returns back BUSY, then we give it 60 chances to execute the command
each attempt to check status is delayed by 100 ms, thus 60 attempts give it a
window of about 6 seconds. */
#define MAX_ATTEMPTS 60
/* We are flashing/erasing 2 sectors */
#define NUM_FLASH_TO_ERASE 2
#define SECTOR_TO_ERASE_1 6
#define SECTOR_TO_ERASE_2 7
int page_to_erase[2] = {SECTOR_TO_ERASE_1, SECTOR_TO_ERASE_2};
/* -------------- Local Data Structures ------------- */


/* -------------- Global Functions ----------------- */

/* m4sensorhub_401_load_firmware()
   This function uses I2C bootloader version 1.1 on M4
   Check firmware and load if different from what's already on the M4.
   Then jump to user code on M4.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/

int m4sensorhub_401_load_firmware(struct m4sensorhub_data *m4sensorhub,
	unsigned short force_upgrade, const struct firmware *fm)
{
	const struct firmware *firmware = fm;
	int ret = 0;
	int bytes_left, bytes_to_write;
	int address_to_write;
	u8 *buf_to_read, *buf = NULL;
	u16 fw_version_file, fw_version_device;
	u32 barker_read_from_device;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		ret = -ENODATA;
		goto done;
	}

	m4sensorhub_hw_reset(m4sensorhub);

	buf = kzalloc(MAX_TRANSFER_SIZE, GFP_KERNEL);
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

	KDEBUG(M4SH_INFO, "%s: Filename = %s", __func__, m4sensorhub->filename);

	if (firmware->size > MAX_FILE_SIZE) {
		KDEBUG(M4SH_ERROR, "%s: firmware file size is too big.\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	fw_version_file = *(u16 *)(firmware->data +
		VERSION_ADDRESS - USER_FLASH_FIRST_PAGE_ADDRESS);

	if (!force_upgrade) {
		/* Verify Barker number from device */
		if (m4sensorhub_bl_rm(m4sensorhub, BARKER_ADDRESS,
				(u8 *)&barker_read_from_device,
				sizeof(barker_read_from_device)) != 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : error reading Barker\n",
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
			/*
			 * This is likely a blank flash factory case, so
			 * skip doing any driver pre-flash callbacks.
			 */
			goto m4sensorhub_401_load_firmware_erase_flash;
		} else {
			/* Read firmware version from device */
			if (m4sensorhub_bl_rm(m4sensorhub, VERSION_ADDRESS,
					(u8 *)&fw_version_device,
					sizeof(fw_version_device)) != 0) {
				KDEBUG(M4SH_ERROR, "%s : %d : Read err\n",
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
				"Firmware on device different from file, updating...\n");
		}
	} else {
		KDEBUG(M4SH_NOTICE, "Version of firmware on file is 0x%04x\n",
			fw_version_file);
		/*
		 * Currently no code uses the force_upgrade path,
		 * but in case it is used for some recovery (and because
		 * no numbers or error checking is done), we will skip
		 * trying to call any driver pre-flash callbacks.
		 */
		goto m4sensorhub_401_load_firmware_erase_flash;
	}

	/* Boot M4, execute any pre-flash callbacks, then reset for BL mode */
	if (m4sensorhub_preflash_callbacks_exist()) {
		KDEBUG(M4SH_ERROR, "%s: Booting M4 to execute callbacks...\n",
			__func__); /* Not an error (see similar above) */
		ret = m4sensorhub_jump_to_user(m4sensorhub);
		if (ret < 0) {
			KDEBUG(M4SH_ERROR, "%s: %s %s %d.\n", __func__,
				"Failed to boot M4 for callbacks",
				"with error code", ret);
			/*
			 * Since we don't know the status of M4 at this point,
			 * we will skip any callbacks, reset the IC to a known
			 * state, and go ahead with a reflash.
			 */
			m4sensorhub_hw_reset(m4sensorhub);
			goto m4sensorhub_401_load_firmware_erase_flash;
		}

		m4sensorhub_call_preflash_callbacks();
		KDEBUG(M4SH_ERROR, "%s: Callbacks complete, flashing M4...\n",
			__func__); /* Not an error (see similar above) */
		m4sensorhub_hw_reset(m4sensorhub);
	}

	/* The flash memory to update has to be erased before updating */
m4sensorhub_401_load_firmware_erase_flash:
	ret = m4sensorhub_bl_erase_fw(m4sensorhub);
	if (ret < 0) {
		pr_err("%s: erase failed\n", __func__);
		/* TODO : Not sure if this is critical error
		goto done; */
	}

	bytes_left = firmware->size;
	address_to_write = USER_FLASH_FIRST_PAGE_ADDRESS;
	buf_to_read = (u8 *)firmware->data;

	KDEBUG(M4SH_DEBUG, "%s: %d bytes to be written\n", __func__,
		firmware->size);

	while (bytes_left) {
		if (bytes_left > MAX_TRANSFER_SIZE)
			bytes_to_write = MAX_TRANSFER_SIZE;
		else
			bytes_to_write = bytes_left;

		if (m4sensorhub_bl_wm(m4sensorhub, address_to_write,
				buf_to_read, bytes_to_write) != 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : error writing %d\n",
				__func__, __LINE__, address_to_write);
			ret = -EINVAL;
			goto done;
		}

		/* Relying on CRC to take care of errors */
		address_to_write += bytes_to_write;
		buf_to_read += bytes_to_write;
		bytes_left -= bytes_to_write;
	}

	/* Write barker number when firmware successfully written */
	buf[0] = BARKER_NUMBER & 0xFF;
	buf[1] = (BARKER_NUMBER >> 8) & 0xFF;
	buf[2] = (BARKER_NUMBER >> 16) & 0xFF;
	buf[3] = (BARKER_NUMBER >> 24) & 0xFF;
	if (m4sensorhub_bl_wm(m4sensorhub, BARKER_ADDRESS,
		buf, 4) != 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : error writing\n",
			__func__, __LINE__);
		ret = -EINVAL;
		goto done;
	}

	KDEBUG(M4SH_NOTICE, "%s: %d bytes written successfully\n",
		__func__, firmware->size);

	ret = 0;

done:
	release_firmware(firmware);

	/* irrespective of whether we upgraded FW or not,
	the firmware version will be the same as one in the file*/
	m4sensorhub->fw_version = fw_version_file;

	/* If ret is invalid, then we don't try to jump to user code */
	if (ret >= 0 && m4sensorhub_jump_to_user(m4sensorhub) < 0)
		/* If jump to user code fails, return failure */
		ret = -EINVAL;

	kfree(buf);

	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_401_load_firmware);

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
			err = m4sensorhub_jump_to_user(m4);
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

/* -------------- Local Functions ----------------- */
/* m4sensorhub_bl_ack()

   Receive next byte from M4 and wait to get either NACK or ACK.
   if M4 sent back ACK, then return 1, else return 0.

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
static int m4sensorhub_bl_ack(struct m4sensorhub_data *m4sensorhub)
{
	u8 buf_ack = 0;
	int ret = 0;
	int timeout = 0;

	for (timeout = 0; timeout <= MAX_ATTEMPTS; timeout++) {
		if (m4sensorhub_i2c_write_read(m4sensorhub,
				&buf_ack, 0, 1) < 0) {
			KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
				__func__, __LINE__);
			break;
		}
		/* Poll for ACK or NACK if M4 says its BUSY */
		if (buf_ack == BUSY) {
			KDEBUG(M4SH_DEBUG, "%s : %d : got BUSY from M4\n",
				__func__, __LINE__);
			msleep(100);
		} else if (buf_ack == ACK) {
			KDEBUG(M4SH_DEBUG, "%s : %d : got ACK from M4\n",
				__func__, __LINE__);
			ret = 1;
			break;
		} else if (buf_ack == NACK) {
			break;
		} else {
			KDEBUG(M4SH_ERROR, "%s : %d : unexpected response\n",
				__func__, __LINE__);
			break;
		}
	}

	return ret;
}

/* m4sensorhub_bl_rm()

   Read data from M4 FLASH.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     start_address - address to read data from
     data - pointer of buffer to write read data
     len - length of data to read
*/
static int m4sensorhub_bl_rm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len)
{
	u8 buf[5] = {0};
	int num_bytes = 0;

	if ((data == NULL) || (len < 0) || (len > MAX_TRANSFER_SIZE)) {
		KDEBUG(M4SH_ERROR, "%s : %d : Invalid inputs\n",
			__func__, __LINE__);
		return -1;
	}

	buf[num_bytes++] = OPC_RM;
	buf[num_bytes++] = ~(OPC_RM);

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
			__func__, __LINE__);
		return -1;
	}

	num_bytes = 0;
	buf[num_bytes++] = (start_address >> 24) & 0xFF;
	buf[num_bytes++] = (start_address >> 16) & 0xFF;
	buf[num_bytes++] = (start_address >> 8) & 0xFF;
	buf[num_bytes++] = start_address & 0xFF;
	buf[num_bytes++] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, address invalid\n",
			__func__, __LINE__);
		return -1;
	}

	num_bytes = 0;
	buf[num_bytes++] = (len - 1);
	buf[num_bytes++] = (len - 1) ^ 0xFF;

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received\n",
			__func__, __LINE__);
		return -1;
	}

	if (m4sensorhub_i2c_write_read(m4sensorhub, data, 0, len) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	return 0;
}

/* m4sensorhub_bl_go()

   Cause M4 to jump to new address and start execution fresh.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     start_address - address to jump M4 code to
*/
static int m4sensorhub_bl_go(struct m4sensorhub_data *m4sensorhub,
	int start_address)
{
	u8 buf[5] = {0};
	int num_bytes = 0;

	buf[num_bytes++] = OPC_GO;
	buf[num_bytes++] = ~(OPC_GO);

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
			__func__, __LINE__);
		return -1;
	}

	num_bytes = 0;
	buf[num_bytes++] = (start_address >> 24) & 0xFF;
	buf[num_bytes++] = (start_address >> 16) & 0xFF;
	buf[num_bytes++] = (start_address >> 8) & 0xFF;
	buf[num_bytes++] = start_address & 0xFF;
	buf[num_bytes++] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, address invalid\n",
			__func__, __LINE__);
		return -1;
	}

	return 0;
}

/* m4sensorhub_bl_wm()

   Write data to M4 FLASH.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     start_address - address to write data to
     data - pointer of data to write
     len - length of data to write, must be 16-bit aligned
*/
static int m4sensorhub_bl_wm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len)
{
	u8 buf[5] = {0};
	u8 *temp_buf = NULL;
	u8 checksum = 0;
	int i, retval = -1;
	int malloc_size = 0;
	int num_bytes = 0;

	if ((data == NULL) || (len < 0) || (len > MAX_TRANSFER_SIZE)) {
		KDEBUG(M4SH_ERROR, "%s : %d : Invalid inputs\n",
			__func__, __LINE__);
		goto done;
	}

	/* add 2 bytes for data, one for length and last for checksum
	as per spec, data is 16 bit aligned, padding of 0xFF if needed */
	if (len % 2)
		malloc_size = len + 1 + 2;
	else
		malloc_size = len + 2;


	temp_buf = kzalloc(malloc_size, GFP_KERNEL);
	if (!temp_buf) {
		pr_err("Can't allocate memory %s\n", __func__);
		retval = -ENOMEM;
		goto done;
	}

	buf[num_bytes++] = OPC_NO_STRETCH_WM;
	buf[num_bytes++] = ~(OPC_NO_STRETCH_WM);

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		goto done;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
			__func__, __LINE__);
		goto done;
	}

	num_bytes = 0;
	checksum = 0;

	buf[num_bytes++] = (start_address >> 24) & 0xFF;
	checksum = checksum ^ ((start_address >> 24) & 0xFF);
	buf[num_bytes++] = (start_address >> 16) & 0xFF;
	checksum = checksum ^ ((start_address >> 16) & 0xFF);
	buf[num_bytes++] = (start_address >> 8) & 0xFF;
	checksum = checksum ^ ((start_address >> 8) & 0xFF);
	buf[num_bytes++] = start_address & 0xFF;
	checksum = checksum ^ ((start_address) & 0xFF);
	buf[num_bytes++] = checksum;

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		goto done;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, address invalid\n",
			__func__, __LINE__);
		goto done;
	}


	checksum = 0;
	num_bytes = 0;
	/* (malloc_size -2 - 1) is the number of data bytes -1 to be sent */
	checksum = checksum ^ (malloc_size - 2 - 1);
	/* Calculate checksum of data */
	for (i = 0; i < len; i++)
		checksum ^= data[i];

	if (len % 2)
		checksum ^= 0xFF;

	temp_buf[num_bytes++] = malloc_size - 2 - 1;
	memcpy(&temp_buf[num_bytes], data, len);
	if (len % 2)
		temp_buf[len + 1] = 0xFF;
	temp_buf[malloc_size-1] = checksum;

	if (m4sensorhub_i2c_write_read(m4sensorhub, temp_buf,
			malloc_size, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		goto done;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, data invalid\n",
			__func__, __LINE__);
		goto done;
	}

	retval = 0;

done:
	kfree(temp_buf);

	return retval;
}

/* m4sensorhub_bl_erase_fw()

   Erases the M4 FLASH that is to be used for the firmware.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
static int m4sensorhub_bl_erase_fw(struct m4sensorhub_data *m4sensorhub)
{
	u8 buf[2*NUM_FLASH_TO_ERASE + 1] = {0};
	u8 checksum = 0;
	int i;
	int num_bytes = 0;

	buf[num_bytes++] = OPC_NO_STRETCH_ER;
	buf[num_bytes++] = ~(OPC_NO_STRETCH_ER);

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
			__func__, __LINE__);
		return -1;
	}

	num_bytes = 0;
	buf[num_bytes++] = ((NUM_FLASH_TO_ERASE - 1) >> 8) & 0xFF;
	buf[num_bytes++] = ((NUM_FLASH_TO_ERASE - 1) & 0xFF);
	buf[num_bytes++] = buf[0] ^ buf[1];

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes,
			0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
			__func__, __LINE__);
		return -1;
	}

	checksum = 0;
	for (i = 0; i < NUM_FLASH_TO_ERASE; i++) {
		buf[2*i] = (page_to_erase[i] >> 8) & 0xFF;
		buf[2*i+1] = page_to_erase[i] & 0xFF;
		checksum ^= buf[2*i];
		checksum ^= buf[2*i+1];
	}
	buf[2*NUM_FLASH_TO_ERASE] = checksum;

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf,
			(2*NUM_FLASH_TO_ERASE + 1), 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
			__func__, __LINE__);
		return -1;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
			__func__, __LINE__);
		return -1;
	}

	return 0;
}

/* m4sensorhub_jump_to_user()

   Jump to user code loaded in M4 FLASH.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
static int m4sensorhub_jump_to_user(struct m4sensorhub_data *m4sensorhub)
{
	int ret = -1;
	u32 barker_read_from_device = 0;

	if (m4sensorhub_bl_rm(m4sensorhub, BARKER_ADDRESS,
	    (u8 *)&barker_read_from_device, 4) != 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : error reading from device\n",
		       __func__, __LINE__);
		KDEBUG(M4SH_ERROR, "*** Not executing M4 code ***\n");
		return -EIO;
	}

	if (barker_read_from_device == BARKER_NUMBER) {
		m4sensorhub_bl_go(m4sensorhub, USER_FLASH_FIRST_PAGE_ADDRESS);
		KDEBUG(M4SH_NOTICE, "Waiting for M4 setup\n");
		msleep(100);
		KDEBUG(M4SH_NOTICE, "Executing M4 code\n");
		ret = 0;
	} else {
		KDEBUG(M4SH_ERROR, "Wrong Barker Number read 0x%8x\n",
		       barker_read_from_device);
		KDEBUG(M4SH_ERROR, "*** Not executing M4 code ***\n");
	}

	return ret;
}
