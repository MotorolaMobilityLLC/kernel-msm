/*
 * Copyright (C) 2010-2013 Motorola Mobility LLC
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

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/stm401.h>


#define STM401_MAXDATA_LENGTH		250
#define STM401_DELAY_USEC		10
#define STM401_RESPONSE_MSG		0x3b
#define STM401_RESPONSE_MSG_SUCCESS	0x00
#define STM401_CRC_SEED			0xffff
#define STM401_COMMAND_HEADER		0x80
#define STM401_CRC_LENGTH		2
#define STM401_OPCODE_LENGTH		1
#define STM401_ADDRESS_LENGTH		3
#define STM401_CORECOMMAND_LENGTH	(STM401_OPCODE_LENGTH +\
					STM401_MAXDATA_LENGTH +\
					STM401_ADDRESS_LENGTH)
#define STM401_HEADER_LENGTH		1
#define STM401_CMDLENGTH_BYTES		2
#define G_MAX				0x7FFF
#define STM401_CLIENT_MASK		0xF0
#define STM401_RESET_DELAY	400
#define CRC_RESPONSE_LENGTH	9
#define CRC_PACKET_LENGTH	11

/* Store crc value */
unsigned short datacrc;

enum stm_opcode {
	PASSWORD_OPCODE = 0x11,
	MASSERASE_OPCODE = 0x15,
	RXDATA_OPCODE = 0x10,
	CRC_OPCODE = 0x16,
};

static const unsigned short crc_table[256] = {
0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5,
0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b,
0xc18c, 0xd1ad, 0xe1ce, 0xf1ef, 0x1231, 0x0210,
0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c,
0xf3ff, 0xe3de, 0x2462, 0x3443, 0x0420, 0x1401,
0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a, 0xb54b,
0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6,
0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738,
0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5,
0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969,
0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96,
0x1a71, 0x0a50, 0x3a33, 0x2a12, 0xdbfd, 0xcbdc,
0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03,
0x0c60, 0x1c41, 0xedae, 0xfd8f, 0xcdec, 0xddcd,
0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97, 0x6eb6,
0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca, 0xa1eb,
0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1,
0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c,
0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2,
0x4235, 0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb,
0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447,
0x5424, 0x4405, 0xa7db, 0xb7fa, 0x8799, 0x97b8,
0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3, 0x36f2,
0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9,
0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827,
0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c,
0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0,
0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d,
0xbdaa, 0xad8b, 0x9de8, 0x8dc9, 0x7c26, 0x6c07,
0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba,
0x8fd9, 0x9ff8, 0x6e17, 0x7e36, 0x4e55, 0x5e74,
0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

static int stm401_misc_open(struct inode *inode, struct file *file)
{
	int err = 0;
	dev_dbg(&stm401_misc_data->client->dev, "stm401_misc_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = stm401_misc_data;

	err = stm401_enable(stm401_misc_data);

	return err;
}

unsigned short stm401_calculate_crc(unsigned char *data, size_t length)
{
	size_t count;
	unsigned int crc = STM401_CRC_SEED;
	unsigned int temp;
	for (count = 0; count < length; ++count) {
		temp = (*data++ ^ (crc >> 8)) & 0xff;
		crc = crc_table[temp] ^ (crc << 8);
	}
	return (unsigned short)(crc);
}

/* the caller function is resposible to free mem allocated in this function. */
void stm401_build_command(enum stm_commands cmd,
		const char *inbuff, unsigned int *length)
{
	unsigned int index = 0, i, len = *length;
	unsigned short corecmdlen;
	unsigned short crc;

	struct stm401_data *ps_stm = stm401_misc_data;

	/*allocate for the stm command */
	stm401_cmdbuff[index++] = STM401_COMMAND_HEADER; /* header */
	switch (cmd) {
	case PASSWORD_RESET:
		stm401_cmdbuff[index++] = 0x21; /* len LSB */
		stm401_cmdbuff[index++] = 0x00; /* len MSB */
		stm401_cmdbuff[index++] = PASSWORD_OPCODE; /* opcode */
		for (i = 4; i < 36; i++) /* followed by FFs */
			stm401_cmdbuff[index++] = 0xff;
		stm401_cmdbuff[index++] = 0x9E; /* CRC LSB */
		stm401_cmdbuff[index++] = 0xE6; /* CRC MSB */
		break;
	case MASS_ERASE:
		stm401_cmdbuff[index++] = 0x01; /* len LSB */
		stm401_cmdbuff[index++] = 0x00; /* len MSB */
		stm401_cmdbuff[index++] = MASSERASE_OPCODE; /* opcode */
		stm401_cmdbuff[index++] = 0x64; /* crc LSB */
		stm401_cmdbuff[index++] = 0xa3; /* crc MSB */
		break;
	case CRC_CHECK:
		corecmdlen = STM401_CRC_LENGTH +
			STM401_OPCODE_LENGTH + STM401_ADDRESS_LENGTH;
		stm401_cmdbuff[index++] =
			(unsigned char)(corecmdlen & 0xff); /* LSB len */
		stm401_cmdbuff[index++] =
			(unsigned char)((corecmdlen >> 8) & 0xff);/*MSB len*/
		stm401_cmdbuff[index++] = CRC_OPCODE; /* opcode */
		/* LSB of write addr on STM */
		stm401_cmdbuff[index++] =
			(unsigned char)(ps_stm->current_addr & 0xff);
		/* middle byte of write addr */
		stm401_cmdbuff[index++] =
			(unsigned char)((ps_stm->current_addr >> 8) & 0xff);
		/* MSB of write addr on STM */
		stm401_cmdbuff[index++] =
			(unsigned char)((ps_stm->current_addr >> 16) & 0xff);
		/* LSB of length of data to perform CRC */
		stm401_cmdbuff[index++] =
			(unsigned char)(len & 0xff);
		/* MSB of length of data to perform CRC */
		stm401_cmdbuff[index++] =
			(unsigned char)((len >> 8) & 0xff);
		crc = stm401_calculate_crc(stm401_cmdbuff+3, corecmdlen);
		/* crc LSB */
		stm401_cmdbuff[index++] = (unsigned char)(crc & 0xff);
		/* crc MSB */
		stm401_cmdbuff[index++] = (unsigned char)((crc >> 8)&0xff);
		break;
	case PROGRAM_CODE:
		/*code length */
		dev_dbg(&stm401_misc_data->client->dev,
			"No of bytes got from user = %d", len);
		corecmdlen = len + STM401_OPCODE_LENGTH + STM401_ADDRESS_LENGTH;
		stm401_cmdbuff[index++] =
			(unsigned char)(corecmdlen & 0xff); /* LSB len */
		stm401_cmdbuff[index++] =
			(unsigned char)((corecmdlen >> 8) & 0xff);/*MSB len*/
		stm401_cmdbuff[index++] = RXDATA_OPCODE; /* opcode */
		/* LSB of write addr on STM */
		stm401_cmdbuff[index++] =
			(unsigned char)(ps_stm->current_addr & 0xff);
		/* middle byte of write addr */
		stm401_cmdbuff[index++] =
			(unsigned char)((ps_stm->current_addr >> 8) & 0xff);
		/* MSB of write addr on STM */
		stm401_cmdbuff[index++] =
			(unsigned char)((ps_stm->current_addr >> 16) & 0xff);
		/* copy data from user to kernel space */
		if (copy_from_user(stm401_cmdbuff+index, inbuff, len)) {
			dev_err(&stm401_misc_data->client->dev,
				"Copy from user returned error\n");
			index = 0;
		} else {
			index += len; /*increment index with data len*/
			crc = stm401_calculate_crc(stm401_cmdbuff+3, len+1+3);
			datacrc = stm401_calculate_crc(stm401_cmdbuff+7, len);
			dev_dbg(&stm401_misc_data->client->dev,
				"data CRC - %02x\n", datacrc);
			/* crc LSB */
			stm401_cmdbuff[index++] = (unsigned char)(crc & 0xff);
			/* crc MSB */
			stm401_cmdbuff[index++]
				= (unsigned char)((crc >> 8)&0xff);
		}
		break;
	case END_FIRMWARE:
		stm401_cmdbuff[index++] = 0x06; /* len LSB */
		stm401_cmdbuff[index++] = 0x00; /* len MSB */
		stm401_cmdbuff[index++] = RXDATA_OPCODE; /* opcode */
		stm401_cmdbuff[index++] = 0xfe;
		stm401_cmdbuff[index++] = 0xff;
		stm401_cmdbuff[index++] = 0x00;
		stm401_cmdbuff[index++] = 0x00;
		stm401_cmdbuff[index++] = 0x44;
		stm401_cmdbuff[index++] = 0x89; /* crc LSB */
		stm401_cmdbuff[index++] = 0xa7; /* crc MSB */
		break;
	case PASSWORD_RESET_DEFAULT:
		stm401_cmdbuff[index++] = 0x21; /* len LSB */
		stm401_cmdbuff[index++] = 0x00; /* len MSB */
		stm401_cmdbuff[index++] = PASSWORD_OPCODE; /* opcode */
		for (i = 0; i < 32; i++) /* followed by 30 FFs */
			stm401_cmdbuff[index++] = 0xff;
		stm401_cmdbuff[index++] = 0x9e; /* CRC LSB */
		stm401_cmdbuff[index++] = 0xE6; /* CRC MSB */
		break;
	default:
		dev_err(&stm401_misc_data->client->dev,
			"Invalid stm401 cmd\n");
		index = 0;
		break;
	}
	/*command length */
	*length = index;
}

ssize_t stm401_misc_write(struct file *file, const char __user *buff,
				 size_t count, loff_t *ppos)
{
	int err = 0;
	struct stm401_data *ps_stm401;
	unsigned int len = (unsigned int)count;
	unsigned int crclen = (unsigned int)count;

	ps_stm401 = stm401_misc_data;
	dev_dbg(&ps_stm401->client->dev, "stm401_misc_write\n");
	if (len > STM401_MAXDATA_LENGTH || len == 0) {
		dev_err(&ps_stm401->client->dev,
			"Packet size >STM401_MAXDATA_LENGTH or 0\n");
		err = -EINVAL;
		return err;
	}
	dev_dbg(&ps_stm401->client->dev, "Leng = %d", len); /* debug */

	mutex_lock(&ps_stm401->lock);

	if (ps_stm401->mode == BOOTMODE) {
		dev_dbg(&ps_stm401->client->dev,
			"stm401_misc_write: boot mode\n");
		/* build the stm401 command to program code */
		stm401_build_command(PROGRAM_CODE, buff, &len);
		err = stm401_i2c_write_read_no_reset(ps_stm401, stm401_cmdbuff,
				 len, I2C_RESPONSE_LENGTH);

		if (err == 0) {
			stm401_build_command(CRC_CHECK, NULL, &crclen);
			err = stm401_i2c_write_read_no_reset(ps_stm401,
				stm401_cmdbuff, CRC_PACKET_LENGTH,
				CRC_RESPONSE_LENGTH);

			if (err == 0) {
				if (datacrc !=
					((read_cmdbuff[6] << 8)
					+ read_cmdbuff[5])) {
					dev_err(&ps_stm401->client->dev,
						"CRC validation failed\n");
					err = -EIO;
				}

				/* increment the current
				STM write addr by count */
				if (err == 0) {
					stm401_misc_data->current_addr += count;
					/* return the number of
					bytes successfully written */
					err = len;
				}
			}
		}
	} else {
		dev_dbg(&ps_stm401->client->dev,
			"stm401_misc_write: normal mode\n");
		if (copy_from_user(stm401_cmdbuff, buff, count)) {
			dev_err(&ps_stm401->client->dev,
				"Copy from user returned error\n");
			err = -EINVAL;
		}
		if (err == 0)
			err = stm401_i2c_write_no_reset(ps_stm401,
				stm401_cmdbuff, count);
		if (err == 0)
			err = len;
	}
	mutex_unlock(&ps_stm401->lock);
	return err;
}

/* gpio toggling to switch modes(boot mode,normal mode)on STM401 */
int switch_stm401_mode(enum stm_mode mode)
{
	struct stm401_platform_data *pdata;
	unsigned int bslen_pin_active_value =
		stm401_misc_data->pdata->bslen_pin_active_value;

	pdata = stm401_misc_data->pdata;
	stm401_misc_data->mode = mode;

	/* bootloader mode */
	if (mode == BOOTMODE) {
		gpio_set_value(pdata->gpio_bslen,
				(bslen_pin_active_value));
		dev_dbg(&stm401_misc_data->client->dev,
			"Toggling to switch to boot mode\n");
		stm401_reset(pdata);
	} else {
		/*normal mode */
		gpio_set_value(pdata->gpio_bslen,
				!(bslen_pin_active_value));
		dev_dbg(&stm401_misc_data->client->dev,
			"Toggling to normal or factory mode\n");
		stm401_reset_and_init();
	}

	return 0;
}

int stm401_get_version(struct stm401_data *ps_stm401)
{
	int err = 0;
	if (ps_stm401->mode == BOOTMODE) {
		dev_dbg(&ps_stm401->client->dev,
			"Switch to normal to get version\n");
		switch_stm401_mode(NORMALMODE);
		msleep_interruptible(stm401_i2c_retry_delay);
	}
	dev_dbg(&ps_stm401->client->dev, "STM software version: ");
	stm401_cmdbuff[0] = REV_ID;
	err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 1);
	if (err >= 0) {
		err = (int)read_cmdbuff[0];
		dev_err(&ps_stm401->client->dev, "STM401 version %02x",
			read_cmdbuff[0]);
	}
	return err;
}

int stm401_bootloadermode(struct stm401_data *ps_stm401)
{
	int err = 0;
	unsigned int cmdlen = 0;
	/* switch STM to bootloader mode */
	dev_dbg(&ps_stm401->client->dev, "Switching to bootloader mode\n");
	err = switch_stm401_mode(BOOTMODE);
	/* send password reset command to unlock STM	 */
	dev_dbg(&ps_stm401->client->dev, "Password reset for reset vector\n");
	stm401_build_command(PASSWORD_RESET, NULL, &cmdlen);
	err = stm401_i2c_write_read_no_reset(ps_stm401, stm401_cmdbuff,
				cmdlen, I2C_RESPONSE_LENGTH);
	/* password reset for reset vector failed */
	if (err < 0) {
		/* password for blank reset vector */
		stm401_build_command(PASSWORD_RESET_DEFAULT, NULL, &cmdlen);
		err = stm401_i2c_write_read_no_reset(ps_stm401, stm401_cmdbuff,
			cmdlen, I2C_RESPONSE_LENGTH);
	}
	return err;
}

const struct file_operations stm401_misc_fops = {
	.owner = THIS_MODULE,
	.open = stm401_misc_open,
	.unlocked_ioctl = stm401_misc_ioctl,
	.write = stm401_misc_write,
};
