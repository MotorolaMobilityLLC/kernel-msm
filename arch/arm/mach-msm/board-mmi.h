/* Copyright (c) 2012, Motorola Mobility. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MMI_8960_H
#define __ARCH_ARM_MACH_MSM_BOARD_MMI_8960_H

/* from board-mmi-gsbi.c */
extern void mmi_init_gsbi_devices_from_dt(void);

/* from board-mmi-i2c.c */
extern void mmi_register_i2c_devices_from_dt(void);

/* from board-mmi-pmic.c */
extern void mmi_init_pm8921_gpio_mpp(void);
extern void mmi_pm8921_init(void *);

/* from board-mmi-keypad.c */
extern void mmi_pm8921_keypad_init(void *);

#endif

