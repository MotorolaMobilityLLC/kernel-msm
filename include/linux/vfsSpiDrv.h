/*! @file vfsSpiDrv.h
*******************************************************************************
*  SPI Driver Interface Functions
*
*  This file contains the SPI driver interface functions.
*
*  Copyright (C) 2011-2013 Validity Sensors, Inc.
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,
*  Boston, MA  02110-1301, USA.
*
*/

#ifndef VFSSPIDRV_H_
#define VFSSPIDRV_H_

#define SLOW_BAUD_RATE      9600000
#define MAX_BAUD_RATE       17143000
#define BAUD_RATE_COEF      1000
#define DRDY_TIMEOUT_MS     40
#define VFSSPI_DRDY_PIN     143
#define VFSSPI_SLEEP_PIN    127
#define VFSSPI_CS_PIN       29
#define DO_CHIP_SELECT      1
#define DRDY_ACTIVE_STATUS  1
#define BITS_PER_WORD       8
#define DRDY_IRQ_FLAG       IRQF_TRIGGER_RISING
#define DEFAULT_BUFFER_SIZE (4096 * 6)
#define DRDY_IRQ_ENABLE     1
#define DRDY_IRQ_DISABLE    0
#define VFSSPI_DRDY_NOTIFY_TYPE_SIGNAL      0x00000001
#define VFSSPI_DRDY_NOTIFY_TYPE_EVENTFD     0x00000002

/* IOCTL commands definitions */

/*
 * Magic number of IOCTL command
 */
#define VFSSPI_IOCTL_MAGIC    'k'
/*
 * Transmit data to the device and retrieve data from it simultaneously
 */
#define VFSSPI_IOCTL_RW_SPI_MESSAGE         _IOWR(VFSSPI_IOCTL_MAGIC,	\
							 1, unsigned int)
/*
 * Hard reset the device
 */
#define VFSSPI_IOCTL_DEVICE_RESET           _IO(VFSSPI_IOCTL_MAGIC,   2)
/*
 * Set the baud rate of SPI master clock
 */
#define VFSSPI_IOCTL_SET_CLK                _IOW(VFSSPI_IOCTL_MAGIC,	\
							 3, unsigned int)
/*
 * Get level state of DRDY GPIO
 */
#define VFSSPI_IOCTL_CHECK_DRDY             _IO(VFSSPI_IOCTL_MAGIC,   4)
/*
 * Register DRDY eventfd. It is used by SPI driver for indicating host that
 * DRDY signal is asserted.
 */
#define VFSSPI_IOCTL_REGISTER_DRDY_EVENTFD   _IOW(VFSSPI_IOCTL_MAGIC,	\
							 5, unsigned int)
/*
 * Enable/disable DRDY interrupt handling in the SPI driver
 */
#define VFSSPI_IOCTL_SET_DRDY_INT           _IOW(VFSSPI_IOCTL_MAGIC,	\
							8, unsigned int)
/*
 * Put device in suspend state
 */
#define VFSSPI_IOCTL_DEVICE_SUSPEND         _IO(VFSSPI_IOCTL_MAGIC,   9)
/*
 * Indicate the fingerprint buffer size for read
 */
#define VFSSPI_IOCTL_STREAM_READ_START      _IOW(VFSSPI_IOCTL_MAGIC,	\
						 10, unsigned int)
/*
 * Indicate that fingerprint acquisition is completed
 */
#define VFSSPI_IOCTL_STREAM_READ_STOP       _IO(VFSSPI_IOCTL_MAGIC,   11)

/*
 * Inform user service of suported notification method
 */
#define VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE   _IOWR(VFSSPI_IOCTL_MAGIC,	\
							 21, unsigned int)
/*
 * Used by IOCTL command:
 *         VFSSPI_IOCTL_RW_SPI_MESSAGE
 *
 * @rx_buffer:pointer to retrieved data
 * @tx_buffer:pointer to transmitted data
 * @len:transmitted/retrieved data size
 */
struct vfsspi_ioctl_transfer {
	unsigned char *rx_buffer;
	unsigned char *tx_buffer;
	unsigned int len;
};

/*
 * Used by IOCTL command:
 *         VFSSPI_IOCTL_REGISTER_DRDY_EVENTFD
 *
 * @user_pid:Process ID to which SPI driver sends signal indicating that DRDY
 *			is asserted
 * @eventfd:eventfd
*/
struct vfsspi_ioctl_register_eventfd {
	int user_pid;
	unsigned int eventfd;
};

/*
 * Used by IOCTL command:
 *         VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE
 *
 * @supported_types: indicated types supported by driver
 * @selected_type: inform service which notification type to use
*/
struct vfsspi_ioc_select_drdy_ntf_type {
	unsigned int supported_types;
	unsigned int selected_type;
};

#endif /* VFSSPIDRV_H_ */
