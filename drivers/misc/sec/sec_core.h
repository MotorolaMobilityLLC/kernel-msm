/*
 * Copyright (C) 2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define SEC_PROC_ID_SIZE   16

#define SEC_IOCTL_WRITE_FUSE	_IOWR(0x99, 120, int*)
#define SEC_IOCTL_READ_FUSE	_IOWR(0x99, 121, int*)
#define SEC_IOCTL_READ_PROC_ID  _IOWR(0x99, 123, int*)
#define SEC_IOCTL_GET_TZ_VERSION _IOWR(0x99, 124, int*)
#define SEC_IOCTL_GET_TZ_CODES _IOWR(0x99, 125, int*)

struct SEC_EFUSE_PARM_T {
	unsigned int which_bank;
	unsigned int efuse_value;
};

enum SEC_KM_RESULT_T {
	SEC_KM_SUCCESS = 98,
	SEC_KM_FAIL = 99,
};
