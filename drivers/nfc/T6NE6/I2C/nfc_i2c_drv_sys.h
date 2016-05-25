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

#ifndef __NFC_I2C_DRV_SYS_H__
#define __NFC_I2C_DRV_SYS_H__

#include <linux/ioctl.h>
/******************************************************************************
 * define
 ******************************************************************************/
/* IOCTL parameters ( I2C ) */
#define NFC_I2C_IOC_MAGIC 'd'
#define NFC_I2C_IOCTL_SET_PON_L    _IOW ( NFC_I2C_IOC_MAGIC, \
                                          0x02, \
                                          struct _nfc_i2c_data_rw)

#define NFC_I2C_IOCTL_SET_PON_H    _IOW ( NFC_I2C_IOC_MAGIC, \
                                          0x01, \
                                          struct _nfc_i2c_data_rw)

/******************************************************************************
 * data
 ******************************************************************************/
/* structure for register Read/Write I2C */
/*typedef struct _nfc_i2c_data_rw { */
/*    unsigned short slave_adr;     */           /* I2C slave address  */
/*    unsigned short adr;           */           /* reg. address       */
/*    unsigned short sbit;          */           /* start bit position */
/*    unsigned short ebit;          */           /* end bit position   */
/*    unsigned short param;         */           /* write/read value   */
/*    unsigned short enabit;        */           /* enable bit mask    */
/*} NFC_I2C_DATA_RW;                */
typedef struct _nfc_i2c_data_rw {
    unsigned short data;                         /* enable bit mask    */
} NFC_I2C_DATA_RW;

#endif  /* __NFC_I2C_DRV_SYS_H__ */
