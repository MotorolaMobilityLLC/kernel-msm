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
#ifndef __NFC_GPIO_DRV_H__
#define __NFC_GPIO_DRV_H__

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>

/* flag of kernel thread */
#define NFC_GPIO_KTHD_NONE          0x00000000       /* initial flag */
#define NFC_GPIO_KTHD_IRQHANDLER    0x00000001       /* interrupt flag position */
#define NFC_GPIO_KTHD_END           0x80000000       /* end flag position */

/* kernel thread end flag  */
#define NFC_GPIO_KTHD_ENDFLG_OFF          0x00       /* in active */
#define NFC_GPIO_KTHD_ENDFLG_ON           0x01       /* not active */

int nfc_gpio_status_active(void);
int nfc_gpio_status_suspend(void);
void nfc_gpio_pon_low(void);
#endif /* __NFC_GPIO_DRV_H__ */
