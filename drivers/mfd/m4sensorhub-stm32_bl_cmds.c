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

/* --------------- Global Declarations -------------- */


/* If M4 returns back BUSY, then we give it 60 chances to execute the command
each attempt to check status is delayed by 100 ms, thus 60 attempts give it a
window of about 6 seconds. */
#define MAX_ATTEMPTS 60
#define CHIP_DETAILS_LEN 20
#define CHIP_ID_RSP_LEN 3
/* -------------- Local Data Structures ------------- */

/* -------------- Local Functions ----------------- */
/* m4sensorhub_bl_ack()

	Receive next byte from M4 and wait to get either NACK or ACK.
	if M4 sent back ACK, then return 1, else return 0.

	m4sensorhub - pointer to the main m4sensorhub data struct
*/
int m4sensorhub_bl_ack(struct m4sensorhub_data *m4sensorhub)
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
EXPORT_SYMBOL_GPL(m4sensorhub_bl_ack);

/* m4sensorhub_bl_rm()

   Read data from M4 FLASH.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     start_address - address to read data from
     data - pointer of buffer to write read data
     len - length of data to read
*/
int m4sensorhub_bl_rm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len)
{
	u8 buf[5] = {0};
	int num_bytes = 0;

	if ((data == NULL) || (len < 0)) {
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
EXPORT_SYMBOL_GPL(m4sensorhub_bl_rm);

/* m4sensorhub_bl_go()

   Cause M4 to jump to new address and start execution fresh.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     start_address - address to jump M4 code to
*/
int m4sensorhub_bl_go(struct m4sensorhub_data *m4sensorhub,
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
EXPORT_SYMBOL_GPL(m4sensorhub_bl_go);

/* m4sensorhub_bl_wm()

   Write data to M4 FLASH.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     start_address - address to write data to
     data - pointer of data to write
     len - length of data to write, must be 16-bit aligned
*/
int m4sensorhub_bl_wm(struct m4sensorhub_data *m4sensorhub,
	int start_address, u8 *data, int len)
{
	u8 buf[5] = {0};
	u8 *temp_buf = NULL;
	u8 checksum = 0;
	int i, retval = -1;
	int malloc_size = 0;
	int num_bytes = 0;

	if ((data == NULL) || (len < 0)) {
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
EXPORT_SYMBOL_GPL(m4sensorhub_bl_wm);

/* m4sensorhub_bl_erase_sectors()

   Erases the M4 FLASH that is to be used for the firmware.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
int m4sensorhub_bl_erase_sectors(struct m4sensorhub_data *m4sensorhub,
					int *sector_array, int numsectors)
{
	u8 *buf;
	u8 checksum = 0;
	int i;
	int num_bytes = 0;
	int ret = 0;

	buf = kzalloc(2*numsectors + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	buf[num_bytes++] = OPC_NO_STRETCH_ER;
	buf[num_bytes++] = ~(OPC_NO_STRETCH_ER);

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	num_bytes = 0;
	buf[num_bytes++] = ((numsectors - 1) >> 8) & 0xFF;
	buf[num_bytes++] = ((numsectors - 1) & 0xFF);
	buf[num_bytes++] = buf[0] ^ buf[1];

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes,
				       0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	checksum = 0;
	for (i = 0; i < numsectors; i++) {
		buf[2*i] = (sector_array[i] >> 8) & 0xFF;
		buf[2*i+1] = sector_array[i] & 0xFF;
		checksum ^= buf[2*i];
		checksum ^= buf[2*i+1];
	}
	buf[2*numsectors] = checksum;

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf,
				       (2*numsectors + 1), 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

err:
	kfree(buf);

	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_bl_erase_sectors);

int m4sensorhub_bl_get_chipdetails(struct m4sensorhub_data *m4sensorhub)
{
	int num_bytes = 0;
	int ret = 0;
	int i;
	u8 buf[CHIP_DETAILS_LEN] = {0};
	buf[num_bytes++] = OPC_GET;
	buf[num_bytes++] = ~(OPC_GET);
	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 0, CHIP_DETAILS_LEN) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	pr_info("%s: Response from L4 for opcode OPC_GET is: \n", __func__);
	for (i = 0; i < CHIP_DETAILS_LEN; i++) {
		pr_info("buf[%d] = 0x%x\n", i, buf[i]);
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_bl_get_chipdetails);

int m4sensorhub_bl_get_bootloader_version(struct m4sensorhub_data *m4sensorhub)
{
	u8 buf[2] = {0};
	int num_bytes = 0;
	int ret, i;
	buf[num_bytes++] = OPC_GV;
	buf[num_bytes++] = ~(OPC_GV);
	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	num_bytes = 1;
	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 0, num_bytes) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	pr_info("%s: Response from L4 for opcode OPC_GV is: \n", __func__);
	for (i = 0; i < num_bytes; i++) {
		pr_info("data[%d] = 0x%x\n", i, buf[i]);
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_bl_get_bootloader_version);

int m4sensorhub_bl_get_chip_id(struct m4sensorhub_data *m4sensorhub)
{
	u8 buf[CHIP_ID_RSP_LEN] = {0};
	int num_bytes = 0;
	int ret, i;
	buf[num_bytes++] = OPC_GID;
	buf[num_bytes++] = ~(OPC_GID);
	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, 0, CHIP_ID_RSP_LEN) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}
	pr_info("%s: Response from L4 for opcode OPC_GID is: \n", __func__);
	for (i = 0; i < CHIP_ID_RSP_LEN; i++) {
		pr_info("data[%d] = 0x%x\n", i, buf[i]);
	}
err:
	return ret;
}
EXPORT_SYMBOL_GPL(m4sensorhub_bl_get_chip_id);

int m4sensorhub_bl_erase_bank(struct m4sensorhub_data *m4sensorhub,
    int bank_id)
{
	int ret = 0;
	u8 buf[3];
	int num_bytes = 0;
	u8 special_erase_low_byte;

	switch(bank_id) {
		case BANK1:
			special_erase_low_byte = SPECIAL_ERASE_LOW_BYTE_CMD_BANK1;
		break;
		case BANK2:
			special_erase_low_byte = SPECIAL_ERASE_LOW_BYTE_CMD_BANK2;
		break;
		case BANK_ALL:
			special_erase_low_byte = SPECIAL_ERASE_LOW_BYTE_CMD_BANK_ALL;
		break;
		default:
			ret = -EINVAL;
			goto err;
	}

	buf[num_bytes++] = OPC_NO_STRETCH_ER;
	buf[num_bytes++] = ~(OPC_NO_STRETCH_ER);

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes, 0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	num_bytes = 0;
	buf[num_bytes++] = SPECIAL_ERASE_HIGH_BYTE;
	buf[num_bytes++] = special_erase_low_byte;
	buf[num_bytes++] = SPECIAL_ERASE_HIGH_BYTE ^ special_erase_low_byte;

	if (m4sensorhub_i2c_write_read(m4sensorhub, buf, num_bytes,
				       0) < 0) {
		KDEBUG(M4SH_ERROR, "%s : %d : I2C transfer error\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

	if (!m4sensorhub_bl_ack(m4sensorhub)) {
		KDEBUG(M4SH_ERROR, "%s : %d : NACK received, command invalid\n",
		       __func__, __LINE__);
		ret = -1;
		goto err;
	}

err:
	return ret;

}
EXPORT_SYMBOL_GPL(m4sensorhub_bl_erase_bank);
