/*
 * Copyright (C) 2010 MEMSIC, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * Definitions for mmc3x30 magnetic sensor chip.
 */
#ifndef __MMC3X30_H__
#define __MMC3X30_H__

#include <linux/ioctl.h>


#define MMC3X30_I2C_NAME		   "mmc3x30"

#define MMC3X30_I2C_ADDR		    0x30

//MEMSIC MMC3X30 Data
#define MMC3X30_REG_DATA         	0x00

//MEMSIC MMC35XX REG
#define MMC35XX_REG_STATUS     	    0x06
#define MMC35XX_REG_CTRL0 	    	0x07
#define MMC35XX_REG_CTRL1        	0x08
#define MMC35XX_REG_ID              0x20
#define MMC35XX_REG_OTP         	0x1B

//MEMSIC MMC36XX REG
#define MMC36XX_REG_TMPT     		0x06
#define MMC36XX_REG_STATUS		0x07
#define MMC36XX_REG_CTRL0 	    	0x08
#define MMC36XX_REG_CTRL1        	0x09
#define MMC36XX_REG_CTRL2        	0x0A
#define MMC36XX_REG_X_THLD        	0x0B
#define MMC36XX_REG_Y_THLD        	0x0C
#define MMC36XX_REG_Z_THLD        	0x0D
#define MMC36XX_REG_SELF_TEST       0x0E
#define MMC36XX_REG_OTP_GATE0       0x0F
#define MMC36XX_REG_OTP_GATE1       0x12
#define MMC36XX_REG_OTP_GATE2       0x13
#define MMC36XX_REG_OTP             0x2A
#define MMC36XX_REG_ID              0x2F

//MEMSIC MMC35XX CMD
#define MMC35XX_SINGLE_TM		0x01
#define MMC35XX_BW_16BIT_SLOW		0x00
#define MMC35XX_BW_16BIT_FAST		0x01
#define MMC35XX_BW_14BIT		0x02
#define MMC35XX_CONT_MODE		0x02
#define MMC35XX_CONT_FREQ_50HZ		0x00
#define MMC35XX_CONT_FREQ_25HZ		0x04
#define MMC35XX_CONT_FREQ_12HZ		0x08
#define MMC35XX_CONT_FREQ_1_5HZ		0x0C

//MEMSIC MMC36XX CMD
#define MMC36XX_OTP_OPEN0		0xE1
#define MMC36XX_OTP_OPEN1		0x11
#define MMC36XX_OTP_OPEN2		0x80
#define MMC36XX_SINGLE_TM		0x01
#define MMC36XX_SINGLE_TMPT		0x02
#define MMC36XX_BW_16BIT_100HZ		0x00
#define MMC36XX_BW_16BIT_200HZ		0x01
#define MMC36XX_BW_16BIT_400HZ		0x02
#define MMC36XX_BW_16BIT_600HZ		0x03

/*MMC35XX RESET*/
#define MMC35XX_FILLCAP			0x80
#define MMC35XX_RESET			0x40
#define MMC35XX_SET			0x20
#define MMC35XX_NOBOOST			0x10
#define MMC35XX_ST_XYZ			0x20
/*MEMSIC MMC35XX  Status */
#define MMC35XX_STATUS_MEASURE_DONE	0x01
#define MMC35XX_STATUS_CHARGE_PUMP	0x02
#define MMC35XX_STATUS_ST_XYZ		0x08

/*MMC36XX RESET*/
#define MMC36XX_FILLCAP                 0x20
#define MMC36XX_RESET			0x10
#define MMC36XX_SET			0x08
#define MMC36XX_ST_POST			0x02
#define MMC36XX_ST_NEGT			0x04
#define MMC36XX_ST_DEFAULT		0x00
#define MMC36XX_ST_DELTA_VALUE		100
/*MEMSIC MMC36XX  Status */
#define MMC36XX_STATUS_MEASURE_DONE     0x01
#define MMC36XX_STATUS_CHARGE_PUMP      0x08

/*DEVICE ID*/
#define MMC3524_DEVICE_ID               0x08
#define MMC3530_DEVICE_ID               0x09
#define MMC3630_DEVICE_ID               0x0A


/* Use 'm' as magic number */
#define MMC3X30_IOM			'm'
/* IOCTLs for MMC3X30 device */
#define MMC3X30_IOC_TM		_IO(MMC3X30_IOM, 0x00)
#define MMC3X30_IOC_SET		_IO(MMC3X30_IOM, 0x01)
#define MMC3X30_IOC_READ	_IOR(MMC3X30_IOM, 0x02, int[3])
#define MMC3X30_IOC_READXYZ	_IOR(MMC3X30_IOM, 0x03, int[3])
#define MMC3X30_IOC_RESET	_IO(MMC3X30_IOM, 0x04)
#define MMC3X30_IOC_NOBOOST	_IO(MMC3X30_IOM, 0x05)
#define MMC3X30_IOC_ID		_IOR(MMC3X30_IOM, 0x06, short)
#define MMC3X30_IOC_DIAG	_IOR(MMC3X30_IOM, 0x14, int[1])
#define MMC3X30_IOC_READ_REG	_IOWR(MMC3X30_IOM, 0x15, unsigned char)
#define MMC3X30_IOC_WRITE_REG	_IOW(MMC3X30_IOM, 0x16, unsigned char[2])
#define MMC3X30_IOC_READ_REGS	_IOWR(MMC3X30_IOM, 0x17, unsigned char[10])
#define MMC3X30_IOC_WRITE_REGS	_IOW(MMC3X30_IOM, 0x18, unsigned char[10])

#ifdef CONFIG_COMPAT
/* IOCTLs for MMC3X30 device */
#define COMPAT_MMC3X30_IOC_TM		_IO(MMC3X30_IOM, 0x00)
#define COMPAT_MMC3X30_IOC_SET		_IO(MMC3X30_IOM, 0x01)
#define COMPAT_MMC3X30_IOC_READ		_IOR(MMC3X30_IOM, 0x02, int[3])
#define COMPAT_MMC3X30_IOC_READXYZ	_IOR(MMC3X30_IOM, 0x03, int[3])
#define COMPAT_MMC3X30_IOC_RESET        _IO(MMC3X30_IOM, 0x04)
#define COMPAT_MMC3X30_IOC_NOBOOST      _IO(MMC3X30_IOM, 0x05)
#define COMPAT_MMC3X30_IOC_ID           _IOR(MMC3X30_IOM, 0x06, short)
#define COMPAT_MMC3X30_IOC_DIAG         _IOR(MMC3X30_IOM, 0x14, int[1])
#define COMPAT_MMC3X30_IOC_READ_REG	_IOWR(MMC3X30_IOM, 0x15, unsigned char)
#define COMPAT_MMC3X30_IOC_WRITE_REG	_IOW(MMC3X30_IOM, 0x16, unsigned char[2])
#define COMPAT_MMC3X30_IOC_READ_REGS	_IOWR(MMC3X30_IOM, 0x17, unsigned char[10])
#define COMPAT_MMC3X30_IOC_WRITE_REGS	_IOW(MMC3X30_IOM, 0x18, unsigned char[10])
#endif

#endif /* __MMC3X30_H__ */

