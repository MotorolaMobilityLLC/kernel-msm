/* include/linux/input/proximity.h  (proximitySensor Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION All rights reserved.
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

/*
 * Definitions for GP2AP030A00F chip.
 */

/* ------------------------------------------------------------------------- */
/* SHARP PROXIMITY SENSOR DRIVER FOR KERNEL MODE                             */
/* ------------------------------------------------------------------------- */

#ifndef PROXIMITY_H
#define PROXIMITY_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include <linux/ioctl.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define PROX_DRIVER_NAME     "gp2a-prox"

#define SH_PROXIMITY_RESULT_FAILURE    -1
#define SH_PROXIMITY_RESULT_SUCCESS    0

#define SH_PROXIMITY_NEAR              0
#define SH_PROXIMITY_FAR               7

#define SH_PROXIMITY_ENABLE            1
#define SH_PROXIMITY_DISABLE           0

/* GP2AP030A00F register address */
#define GP2AP030_REG_COMMAND1               0x00
#define GP2AP030_REG_COMMAND2               0x01
#define GP2AP030_REG_COMMAND3               0x02
#define GP2AP030_REG_COMMAND4               0x03
#define GP2AP030_REG_LT_LSB                 0x08
#define GP2AP030_REG_LT_MSB                 0x09
#define GP2AP030_REG_HT_LSB                 0x0A
#define GP2AP030_REG_HT_MSB                 0x0B
#define GP2AP030_REG_D2_LSB                 0x10
#define GP2AP030_REG_D2_MSB                 0x11


/* IOCTLs for GP2AP030A00F */
#define GP2AP030IO                  0xA2

#define ECS_IOCTL_ENABLE            _IO(GP2AP030IO, 0x01)
#define ECS_IOCTL_DISABLE           _IO(GP2AP030IO, 0x02)
#define ECS_IOCTL_SET_CYCLE         _IOW(GP2AP030IO, 0x03, short)
#define ECS_IOCTL_GET_VO_DATA       _IOR(GP2AP030IO, 0x04, char)
#define ECS_IOCTL_LT_THRESHOLD_WRITE        _IOW(GP2AP030IO, 0x05, unsigned short)
#define ECS_IOCTL_HT_THRESHOLD_WRITE        _IOW(GP2AP030IO, 0x06, unsigned short)
#define ECS_IOCTL_GET_D2_DATA       _IOR(GP2AP030IO, 0x07, unsigned short)

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                    */
/* ------------------------------------------------------------------------- */
int PROX_recovery_end_func(void);
int PROX_recovery_start_func(void);

#endif /* PROXIMITY_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
