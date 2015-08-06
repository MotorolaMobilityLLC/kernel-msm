/*
 * Copyright (C) 2011 NXP Semiconductors
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 */

#define PN544_MAGIC	0xE9

/*
 * PN544 power control via ioctl
 * PN544_SET_PWR(0): power off
 * PN544_SET_PWR(1): power on
 * PN544_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN544_SET_PWR	_IOW(PN544_MAGIC, 0x01, unsigned int)

#ifdef __KERNEL__
struct pn544_i2c_platform_data {
	/*
	 * WARNING: Device tree schema needs to be updated if the layout of
	 * this struct changes.
	 */
	unsigned int irq_gpio;
	unsigned int ven_gpio;
	unsigned int firmware_gpio;
	unsigned int ven_polarity;
	unsigned int discharge_delay;
	/*
	 * WARNING: Device tree schema needs to be updated if the layout of
	 * this struct changes.
	 */
};
#endif /* __KERNEL__ */
