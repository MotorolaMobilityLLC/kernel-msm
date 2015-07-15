/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef SPI_CONTEXHUB_H

#define SPI_CONTEXHUB_H

#define SPI_CPHA                0x01
#define SPI_CPOL                0x02

#define SPI_MODE_0              (0|0)
#define SPI_MODE_1              (0|SPI_CPHA)
#define SPI_MODE_2              (SPI_CPOL|0)
#define SPI_MODE_3              (SPI_CPOL|SPI_CPHA)

#define SPI_CS_HIGH             0x04
#define SPI_LSB_FIRST           0x08
#define SPI_3WIRE               0x10
#define SPI_LOOP                0x20
#define SPI_NO_CS               0x40
#define SPI_READY               0x80

#define SPI_IOC_MAGIC           'k'

struct spi_ioc_transfer {
	__u64 tx_buf;
	__u64 rx_buf;

	__u32 len;
	__u32 speed_hz;

	__u16 delay_usecs;
	__u8 bits_per_word;
	__u8 cs_change;
	__u32 pad;

	/* If the contents of 'struct spi_ioc_transfer' ever change
	 * incompatibly, then the ioctl number (currently 0) must change;
	 * ioctls with constant size fields get a bit more in the way of
	 * error checking than ones (like this) where that field varies.
	 *
	 * NOTE: struct layout is the same in 64bit and 32bit userspace.
	 */
};

/* not all platforms use <asm-generic/ioctl.h> or _IOC_TYPECHECK() ... */
#define SPI_MSGSIZE(N) \
	((((N)*(sizeof (struct spi_ioc_transfer))) < (1 << _IOC_SIZEBITS)) \
	? ((N)*(sizeof (struct spi_ioc_transfer))) : 0)

#define SPI_IOC_MESSAGE(N) _IOW(SPI_IOC_MAGIC, 0, char[SPI_MSGSIZE(N)])

/* Read / Write of SPI mode (SPI_MODE_0..SPI_MODE_3) */
#define SPI_IOC_RD_MODE                 _IOR(SPI_IOC_MAGIC, 1, __u8)
#define SPI_IOC_WR_MODE                 _IOW(SPI_IOC_MAGIC, 1, __u8)

/* Read / Write SPI bit justification */
#define SPI_IOC_RD_LSB_FIRST            _IOR(SPI_IOC_MAGIC, 2, __u8)
#define SPI_IOC_WR_LSB_FIRST            _IOW(SPI_IOC_MAGIC, 2, __u8)

/* Read / Write SPI device word length (1..N) */
#define SPI_IOC_RD_BITS_PER_WORD        _IOR(SPI_IOC_MAGIC, 3, __u8)
#define SPI_IOC_WR_BITS_PER_WORD        _IOW(SPI_IOC_MAGIC, 3, __u8)

/* Read / Write SPI device default max speed hz */
#define SPI_IOC_RD_MAX_SPEED_HZ         _IOR(SPI_IOC_MAGIC, 4, __u32)
#define SPI_IOC_WR_MAX_SPEED_HZ         _IOW(SPI_IOC_MAGIC, 4, __u32)

#define SPI_IOC_ENABLE_TIMESTAMPS       _IOW(SPI_IOC_MAGIC, 5, __u8)
#define SPI_IOC_RESET_HUB               _IOW(SPI_IOC_MAGIC, 6, __u8)
#define SPI_IOC_TXRX                    _IOW(SPI_IOC_MAGIC, 7, char[SPI_MSGSIZE(1)])
#define SPI_IOC_NOTIFY_HUB_SUSPENDED    _IOW(SPI_IOC_MAGIC, 8, __u8)

#endif // SPI_CONTEXHUB_H
