/*
*
* NFC Driver ( Toshiba )
*
* Copyright (C) 2015, TOSHIBA CORPORATION
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/
#ifndef __NFC_I2C_DRV_H__
#define __NFC_I2C_DRV_H__

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>


#define NFC_I2C_MAX_I2C_TRANSFER        0x0400

/* flag of kernel thread */
#define NFC_I2C_KTHD_NONE               0x00000000      /* initial flag */
#define NFC_I2C_KTHD_IRQHANDLER         0x00000001      /* interrupt flag position */
#define NFC_I2C_KTHD_END                0x80000000      /* end flag position */

/* kernel thread end flag  */
#define NFC_I2C_KTH_ENDFLG_OFF          0x00            /* in active */
#define NFC_I2C_KTH_ENDFLG_ON           0x01            /* not active */

/* general use */
#define NFC_I2C_OFF                     0x00            /* OFF */
#define NFC_I2C_ON                      0x01            /* ON  */

/* Write Ready Wait Check Time */
#define NFC_I2C_SIGNAL_WAIT              1              /* Signal Wait */

/* I2C Access Wait */
#define NFC_I2C_ACCESS_BE_WAIT_MIN        3000
#define NFC_I2C_ACCESS_BE_WAIT_MAX        6000
#define NFC_I2C_ACCESS_AF_WAIT_MIN        3000
#define NFC_I2C_ACCESS_AF_WAIT_MAX        6000
#define NFC_I2C_ACCESS_TO_WAIT_MIN        6000
#define NFC_I2C_ACCESS_TO_WAIT_MAX       10000
#define NFC_I2C_ACCESS_PENDING_WAIT      15000

#define NFC_I2C_ACCESS_CHECK_WAIT        5

/* I2C Read Check Wait */
#define NFC_I2C_ACCESS_NONE              0
#define NFC_I2C_ACCESS_READ              1
#define NFC_I2C_ACCESS_WRITE             2

int nfc_i2c_nint_active(void);
int nfc_i2c_nint_suspend(void);

#endif /* __NFC_I2C_DRV_H__ */
