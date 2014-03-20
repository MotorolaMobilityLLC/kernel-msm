/*
 * Copyright (C) 2014 Motorola Mobility LLC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _CCI_INTF_H_
#define _CCI_INTF_H_

#include <linux/ioctl.h>
#include <linux/videodev2.h>

#define MSM_CCI_INTF_MAX_XFER 256

struct msm_cci_intf_register {
	unsigned short width; /* 1 or 2 */
	unsigned short addr;
};

struct msm_cci_intf_data {
	unsigned short count; /* between 1 and MSM_CCI_INTF_MAX_XFER */
	unsigned char  buf[MSM_CCI_INTF_MAX_XFER];
};

struct msm_cci_intf_xfer {
	unsigned short cci_bus;     /* 0 = MASTER_0, 1 = MASTER_1 */
	unsigned short slave_addr;  /* 7-bit addr of intended device */
	struct msm_cci_intf_register  reg;
	struct msm_cci_intf_data      data;
};

#define MSM_CCI_INTF_READ \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 50, struct msm_cci_intf_xfer)
#define MSM_CCI_INTF_READ32 \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 50, struct msm_cci_intf_xfer)

#define MSM_CCI_INTF_WRITE \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 51, struct msm_cci_intf_xfer)
#define MSM_CCI_INTF_WRITE32 \
	_IOWR('X', BASE_VIDIOC_PRIVATE + 51, struct msm_cci_intf_xfer)

#endif
