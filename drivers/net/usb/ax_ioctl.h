/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 *     Copyright (c) 2022    ASIX Electronic Corporation    All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 ******************************************************************************/
#ifndef __ASIX_IOCTL_H
#define __ASIX_IOCTL_H
// CHANGE NETWORK INTERFACE WAY
// DEFAULT_SCAN   : scan "eth0" - "eth255"
// INTERFACE_SCAN : scan all available network interfaces
#define NET_INTERFACE	DEFAULT_SCAN
#define DEFAULT_SCAN	0x00
#define INTERFACE_SCAN	0x01

#define AX88179_SIGNATURE	"AX88179_178A"
#define AX88179_DRV_NAME	"AX88179_178A"
#define AX88179A_SIGNATURE	"AX88179A_772D"
#define AX88179A_DRV_NAME	"AX88179A_772D"

#define AX_PRIVATE		SIOCDEVPRIVATE

#define AX_SIGNATURE			0
#define AX_USB_COMMAND			1
#define AX88179_READ_EEPROM		2
#define AX88179_WRITE_EEPROM		3
#define AX88179A_READ_VERSION		2
#define AX88179A_WRITE_FLASH		3
#define AX88179A_ROOT_2_ROM		4
#define AX88179A_ERASE_FLASH		5
#define AX88179A_SW_RESET		6
#define AX88179A_READ_FLASH		7
#define AX88179A_PROGRAM_EFUSE		8
#define AX88179A_DUMP_EFUSE		9

#define ERR_FALSH_WRITE_EN		1
#define ERR_FALSH_WRITE_DIS		2
#define ERR_FALSH_ERASE_ALL		3
#define ERR_FALSH_WRITE			4
#define ERR_FALSH_READ			5
#define ERR_EFUSE_READ			6
#define ERR_EFUSE_WRITE			6

#define USB_READ_OPS			0
#define USB_WRITE_OPS			1

#define SCAN_DEV_MAX_RETRY		10

struct _ax_usb_command {
	unsigned char	ops;
	unsigned char	cmd;
	unsigned short	value;
	unsigned short	index;
	unsigned short	size;
	unsigned char	*data;
	unsigned long	cmd_data;
};

struct _ax88179a_flash {
	int status;
	int length;
	int offset;
	unsigned char *buf;
};

struct _ax88179a_version {
	unsigned char version[16];
};

struct _ax_ioctl_command {
	unsigned short	ioctl_cmd;
	unsigned char	sig[16];
	unsigned char	type;
	unsigned short *buf;
	unsigned short size;
	unsigned char delay;
	union {
		struct _ax88179a_flash		flash;
		struct _ax88179a_version	version;
		struct _ax_usb_command		usb_cmd;
	};
};

#define SWAP_32(val)	(((val >> 24) & 0x000000FF) | \
			 ((val >>  8) & 0x0000FF00) | \
			 ((val <<  8) & 0x00FF0000) | \
			 ((val << 24) & 0xFF000000))
struct ax_device;
typedef int (*IOCTRL_TABLE)(struct ax_device *axdev,
			    struct _ax_ioctl_command *info);

struct ax_command_info {
	int inet_sock;
	struct ifreq *ifr;
	int argc;
	char **argv;
	unsigned short ioctl_cmd;
	const char *help_ins;
	const char *help_desc;
};

typedef void (*MENU_FUNC)(struct ax_command_info *info);

struct _command_list {
	char *cmd;
	unsigned short ioctl_cmd;
	MENU_FUNC OptFunc;
	const char *help_ins;
	const char *help_desc;
};
#endif /* __ASIX_IOCTL_H */
