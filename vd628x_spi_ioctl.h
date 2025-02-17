/******************** (C) COPYRIGHT 2025 STMicroelectronics ********************
*
* File Name          : vd628x_spi_ioctl.h
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
*******************************************************************************/

#ifndef __VD628x_ADAPTER_IOCTL__
#define __VD628x_ADAPTER_IOCTL__ 1

#define VD628x_IOCTL_REG_WR		_IOW('r', 0x01, struct vd628x_reg)
#define VD628x_IOCTL_REG_RD		_IOWR('r', 0x02, struct vd628x_reg)

#define VD628x_IOCTL_GET_SPI_INFO	_IOWR('r', 0x01, struct vd628x_spi_info)
#define VD628x_IOCTL_SET_SPI_PARAMS	_IOW('r', 0x02, struct vd628x_spi_params)
#define VD628x_IOCTL_GET_CHUNK_SAMPLES	_IOWR('r', 0x03, __u16)

struct vd628x_reg {
	__u8 index;
	__u8 data;
};

struct vd628x_spi_info {
	__u32 chunk_size;
	__u32 spi_max_frequency;
};

struct vd628x_spi_params {
	__u32 speed_hz;
	__u16 samples_nb_per_chunk;
	__u16 pdm_data_sample_width_in_bytes;
};


#endif
