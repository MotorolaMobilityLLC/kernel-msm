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

#ifndef __NFC_I2C_DRV_CONFIG_H__
#define __NFC_I2C_DRV_CONFIG_H__

/******************************************************************************
 * data
 ******************************************************************************/
/* device driver file name */
#define NFC_I2C_CONFIG_DRIVER_NAME      "nfc_i2c_drv"

/* I2C slave address of device register bank */
#define NFC_I2C_SLAVE_ADR               0x55            /* CLF Chip Slave Address   */

/* kernel thread priority  */
#define NFC_I2C_SCHED_CHG
#undef  NFC_I2C_SCHED_CHG

#ifdef  NFC_I2C_SCHED_CHG
#define NFC_I2C_KTHD_PRI                99              /* priority (0-99)         */
#endif  /* NFC_I2C_SCHED_CHG */

/* I2C Read Buffer */
#define NFC_I2C_BUF_SIZE                258              /* I2C Buffer Size          		*/
#define NFC_I2C_READ_BUFFER_SIZE        2*4096           /* I2C READ BUFFER SIZE */

#endif/* __NFC_I2C_DRV_CONFIG_H__ */
