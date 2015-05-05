/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_H
#define LINUX_SPI_FPC1020_H

typedef enum {
	FPC1020_MODE_IDLE = 0,
	FPC1020_MODE_WAIT_AND_CAPTURE = 1,
	FPC1020_MODE_SINGLE_CAPTURE = 2,
	FPC1020_MODE_CHECKERBOARD_TEST_NORM = 3,
	FPC1020_MODE_CHECKERBOARD_TEST_INV = 4,
	FPC1020_MODE_BOARD_TEST_ONE = 5,
	FPC1020_MODE_BOARD_TEST_ZERO = 6,
	FPC1020_MODE_WAIT_FINGER_DOWN = 7,
	FPC1020_MODE_WAIT_FINGER_UP = 8,
	FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE = 9,

} fpc1020_capture_mode_t;
#endif
