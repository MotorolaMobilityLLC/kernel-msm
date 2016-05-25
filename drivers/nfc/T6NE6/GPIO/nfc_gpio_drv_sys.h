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

#ifndef __NFC_GPIO_DRV_SYS_H__
#define __NFC_GPIO_DRV_SYS_H__

#include <linux/ioctl.h>
/******************************************************************************
 * define
 ******************************************************************************/
/* IOCTL parameters */
#define NFC_GPIO_IOC_MAGIC 'd'
#define NFC_GPIO_IOCTL_SET_PON_H     _IOW ( NFC_GPIO_IOC_MAGIC, \
                                            0x01, \
                                            struct _nfc_gpio_data_rw )

#define NFC_GPIO_IOCTL_SET_PON_L     _IOW ( NFC_GPIO_IOC_MAGIC, \
                                            0x02, \
                                            struct _nfc_gpio_data_rw )

#define NFC_GPIO_IOCTL_SET_PON_P     _IOW ( NFC_GPIO_IOC_MAGIC, \
                                            0x03, \
                                            struct _nfc_gpio_data_rw )

#define NFC_GPIO_IOCTL_GET_PON       _IOR ( NFC_GPIO_IOC_MAGIC, \
                                            0x04, \
                                            struct _nfc_gpio_data_rw )

#define NFC_GPIO_IOCTL_GET_NINT      _IOR ( NFC_GPIO_IOC_MAGIC, \
                                            0x15, \
                                            unsigned int             )
#define NFC_GPIO_IOCTL_GET_STATUS    _IOR ( NFC_GPIO_IOC_MAGIC, \
                                            0x16, \
                                            unsigned int             )
#define NFC_GPIO_IOCTL_REQ_CHIPRESET _IO  ( NFC_GPIO_IOC_MAGIC, \
                                            0x17                     )

/* #define NFC_GPIO_IOCTL_GET_PON is Define */
#define NFC_GPIO_IOCTL_GET_PON_LOW  0
#define NFC_GPIO_IOCTL_GET_PON_HIGH 1

/******************************************************************************
 * data
 ******************************************************************************/
/* structure for register Read/Write */
typedef struct _nfc_gpio_data_rw {
    unsigned short data;                          /* enable bit mask    */
} NFC_GPIO_DATA_RW;

#endif  /* __NFC_GPIO_DRV_SYS_H__ */
