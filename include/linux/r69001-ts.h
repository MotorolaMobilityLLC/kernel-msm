/*
 * R69001 Touchscreen Controller Driver
 * Header file
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef _R69001_TS_H_
#define _R69001_TS_H_

#ifdef CONFIG_R69001_DEBUG_SUPPORT
#include <linux/r69001-ts-debug.h>
#endif

/* Spec */
#define MIN_X                       0
#define MIN_Y                       0
#define MIN_Z                       0
#ifdef CONFIG_R69001_RESOLUTION_WIDTH
#define MAX_X                       CONFIG_R69001_RESOLUTION_WIDTH
#else
#define MAX_X                       720
#endif
#ifdef CONFIG_R69001_RESOLUTION_HEIGHT
#define MAX_Y                       CONFIG_R69001_RESOLUTION_HEIGHT
#else
#define MAX_Y                       1280
#endif
#define MAX_Z                       255
#define MIN_AREA                    0
#define MAX_AREA                    10
#ifdef CONFIG_R69001_MAX_FINGERS
#define MAX_FINGERS                 CONFIG_R69001_MAX_FINGERS
#else
#define MAX_FINGERS                 10
#endif

/* Scan cycle */
#ifdef CONFIG_R69001_SCAN_TIME
#define SCAN_TIME                   CONFIG_R69001_SCAN_TIME
#else
#define SCAN_TIME                   16
#endif

/* Polling interval */
#ifdef CONFIG_R69001_POLLING_TIME
#define POLL_INTERVAL               CONFIG_R69001_POLLING_TIME
#else
#define POLL_INTERVAL               2
#endif
#define POLL_INTERVAL_MAX           1000

/* Number of RX/TX */
#ifdef CONFIG_R69001_NUM_RX
#define NUM_RX                      CONFIG_R69001_NUM_RX
#else
#define NUM_RX                      21
#endif
#ifdef CONFIG_R69001_NUM_TX
#define NUM_TX                      CONFIG_R69001_NUM_TX
#else
#define NUM_TX                      13
#endif

/* Scan Mode */
#define R69001_SCAN_MODE_STOP           0x00
#define R69001_SCAN_MODE_LOW_POWER      0x01
#define R69001_SCAN_MODE_FULL_SCAN      0x02
#define R69001_SCAN_MODE_CALIBRATION    0x03

/* Interrupt/Polling mode */
#define R69001_TS_INTERRUPT_MODE        0
#define R69001_TS_POLLING_MODE          1
#define R69001_TS_POLLING_LOW_EDGE_MODE 2
#define R69001_TS_CALIBRATION_INTERRUPT_MODE 3

struct io_mode {
	u8 scan;
	u8 mode;
	u16 poll_time;
};

struct io_rxtx_num {
	u8 rx;
	u8 tx;
};

struct io_resolution {
	u16 x;
	u16 y;
};

struct r69001_io_data {
	struct io_mode mode;
	struct io_rxtx_num rxtx_num;
	struct io_resolution resolution;
};

struct r69001_platform_data {
	int irq_type;
	int gpio;
};

#define R69001_IO   'R'

/* R69001 ioctl commands */
#define RSP_IOCTL_SET_INT_POLL_MODE _IOW(R69001_IO, 0x01, struct r69001_io_data)
#define RSP_IOCTL_GET_RXTX_NUM      _IOR(R69001_IO, 0x02, struct r69001_io_data)
#define RSP_IOCTL_GET_RESOLUTION    _IOR(R69001_IO, 0x03, struct r69001_io_data)
#define RSP_IOCTL_SET_SCAN_MODE     _IOW(R69001_IO, 0x04, struct r69001_io_data)

#define RSP_IOCTL_SUSPEND           _IOW(R69001_IO, 0x50, struct r69001_io_data)
#define RSP_IOCTL_RESUME            _IOW(R69001_IO, 0x51, struct r69001_io_data)

#endif /* _R69001_TS_H_ */
