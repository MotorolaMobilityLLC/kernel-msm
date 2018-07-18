/*
 * Copyright (C) 2006-2017 ILITEK TECHNOLOGY CORP.
 *
 * Description: ILITEK I2C touchscreen driver for linux platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Johnson Yeh
 * Maintain: Luca Hsu, Tigers Huang, Dicky Chiang
 */

/**
 *
 * @file    ilitek_drv_update.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/
/*INCLUDE FILE*/
/*=============================================================*/

#include "ilitek_drv_common.h"

/*=============================================================*/
/*VARIABLE DECLARATION*/
/*=============================================================*/

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
/*
 * Note.
 * Please modify the name of the below .h depends on the vendor TP that you are using.
 */
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
#include "msg22xx_xxxx_update_bin.h"    /*for MSG22xx */
/*#include "msg22xx_yyyy_update_bin.h"*/

sw_id_data_t g_sw_id_data[] = {
    {MSG22XX_SW_ID_XXXX, msg22xx_xxxx_update_bin},
    /*{MSG22XX_SW_ID_YYYY, msg22xx_yyyy_update_bin},*/
};
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
#include "msg28xx_xxxx_update_bin.h"    /*for MSG28xx/MSG58xxA/ILI2117A/ILI2118A */
/*#include "msg28xx_yyyy_update_bin.h"*/

static sw_id_data_t g_sw_id_data[] = {
    {MSG28XX_SW_ID_XXXX, msg28xx_xxxx_update_bin},
    /*{MSG28XX_SW_ID_YYYY, msg28xx_yyyy_update_bin}, */
};
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_ILI21XX
#include "ilitek_fw.h"          /*for ILI21xx */
#endif /*CONFIG_ENABLE_CHIP_TYPE_ILI21XX */

static u32 g_update_retry_count = UPDATE_FIRMWARE_RETRY_COUNT;
static u32 g_is_update_info_block_first = 0;
static struct work_struct g_update_firmware_by_sw_id_work;
static struct workqueue_struct *g_update_firmware_by_sw_id_workQueue = NULL;
#endif /*CONFIG_UPDATE_FIRMWARE_BY_SW_ID */

static u8 g_tp_vendor_code[3] = { 0 };    /*for MSG22xx/MSG28xx */

static u8 g_one_dimen_fw_data[MSG28XX_FIRMWARE_WHOLE_SIZE * 1024] = { 0 }; /*for MSG22xx : array size = MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE*1024+MSG22XX_FIRMWARE_INFO_BLOCK_SIZE, for MSG28xx : array size = MSG28XX_FIRMWARE_WHOLE_SIZE*1024 */

static u8 g_fw_data_buf[MSG28XX_FIRMWARE_WHOLE_SIZE * 1024] = { 0 };  /*for update firmware(MSG22xx/MSG28xx/MSG58xxa) from SD card */

static u16 g_firmware_minor;
static u32 g_n_ilitek_ap_start_addr = 0;
static u32 g_n_ilitek_ap_end_addr = 0;
static u32 g_n_ilitek_ap_check_sum = 0;

static u16 g_sfr_addr3_byte0_1_value = 0x0000;  /*for MSG28xx */
static u16 g_sfr_addr3_byte2_3_value = 0x0000;

/*------------------------------------------------------------------------------//*/

extern struct i2c_client *g_i2c_client;

extern u32 slave_i2c_id_db_bus;
extern u32 slave_i2c_id_dw_i2c;

extern u8 is_force_to_update_firmware_enabled;
extern u8 g_system_update;

extern struct mutex g_mutex;

extern u8 g_fw_data[MAX_UPDATE_FIRMWARE_BUFFER_SIZE][1024];
extern u32 g_fw_data_count;
extern u8 g_fw_version_flag;
extern u16 g_chip_type;
extern u16 g_original_chip_type;

extern u8 g_is_update_firmware;
extern u8 g_msg22xx_chip_revision;

extern struct ilitek_tp_info_t g_ilitek_tp_info;
extern u8 g_ilitek_fw_data[ILITEK_MAX_UPDATE_FIRMWARE_BUFFER_SIZE * 1024];
extern u8 g_ilitek_fw_data_buf[ILITEK_ILI21XX_FIRMWARE_SIZE * 1024];   /*for update firmware(ILI21xx) from SD card */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u8 g_gesture_wakeup_flag;
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

/*=============================================================*/
/*FUNCTION DECLARATION*/
/*=============================================================*/

static s32 drv_msg22xx_update_firmware(u8 szFw_data[][1024], enum emem_type_e eEmemType);
static void drv_read_read_dq_mem_start(void);
static void drv_read_read_dq_mem_end(void);

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
static void drv_update_firmware_by_sw_id_do_work(struct work_struct *p_work);

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
static void drv_msg22xx_check_frimware_update_by_sw_id(void);
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static void drv_msg28xx_check_frimware_update_by_sw_id(void);
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#ifdef CONFIG_ENABLE_CHIP_TYPE_ILI21XX
static void drv_ilitek_check_frimware_update_by_sw_id(void);
#endif /*CONFIG_ENABLE_CHIP_TYPE_ILI21XX */
#endif /*CONFIG_UPDATE_FIRMWARE_BY_SW_ID */
void drv_msg28xx_read_eflash_start(u16 nStartAddr, enum emem_type_e eEmemType);

/*=============================================================*/
/*GLOBAL FUNCTION DEFINITION*/
/*=============================================================*/
u32 drv_msg22xx_retrieve_firmware_crc_from_eflash(enum emem_type_e eEmemType)
{
	u32 n_ret_val = 0;
	u16 nReg_data1 = 0, nReg_data2 = 0;

	DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
		eEmemType);

	db_bus_enter_serial_debug_mode();
	db_bus_stop_mcu();
	db_bus_iic_use_bus();
	db_bus_iic_reshape();

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
	/*Stop MCU */
	reg_set_l_byte_value(0x0FE6, 0x01);

	/*Exit flash low power mode */
	reg_set_l_byte_value(0x1619, BIT1);

	/*Change PIU clock to 48MHz */
	reg_set_l_byte_value(0x1E23, BIT6);

	/*Change mcu clock deglitch mux source */
	reg_set_l_byte_value(0x1E54, BIT0);
#else
	/*Stop MCU */
	 reg_set_l_byte_value(0x0FE6, 0x01);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

	/*RIU password */
	reg_set_16bit_value(0x161A, 0xABBA);

	if (eEmemType == EMEM_MAIN) {   /*Read main block CRC(48KB-4) from main block */
		reg_set_16bit_value(0x1600, 0xBFFC);   /*Set start address for main block CRC */
	} else if (eEmemType == EMEM_INFO) {    /*Read info block CRC(512Byte-4) from info block */
		reg_set_16bit_value(0x1600, 0xC1FC);   /*Set start address for info block CRC */
	}

	/*Enable burst mode */
	reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));

	reg_set_l_byte_value(0x160E, 0x01);

	nReg_data1 = reg_get16_bit_value(0x1604);
	nReg_data2 = reg_get16_bit_value(0x1606);

	n_ret_val = ((nReg_data2 >> 8) & 0xFF) << 24;
	n_ret_val |= (nReg_data2 & 0xFF) << 16;
	n_ret_val |= ((nReg_data1 >> 8) & 0xFF) << 8;
	n_ret_val |= (nReg_data1 & 0xFF);

	DBG(&g_i2c_client->dev, "CRC = 0x%x\n", n_ret_val);

	/*Clear burst mode */
	reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));

	reg_set_16bit_value(0x1600, 0x0000);

	/*Clear RIU password */
	reg_set_16bit_value(0x161A, 0x0000);

	db_bus_iic_not_use_bus();
	db_bus_not_stop_mcu();
	db_bus_exit_serial_debug_mode();

	return n_ret_val;
}

void drv_optimize_current_consumption(void)
{
    u32 i;
    u8 szDbBusTx_data[35] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "g_chip_type = 0x%x\n", g_chip_type);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_gesture_wakeup_flag == 1) {
        return;
    }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        mutex_lock(&g_mutex);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

        drv_touch_device_hw_reset();

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        reg_set_16bit_value(0x1618, (reg_get16_bit_value(0x1618) | 0x80));

        /*Enable burst mode */
        reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x11;
        szDbBusTx_data[2] = 0xA0;    /*bank:0x11, addr:h0050 */

        for (i = 0; i < 24; i++) {
            szDbBusTx_data[i + 3] = 0x11;
        }

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + 24);    /*Write 0x11 for reg 0x1150~0x115B */

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x11;
        szDbBusTx_data[2] = 0xB8;    /*bank:0x11, addr:h005C */

        for (i = 0; i < 6; i++) {
            szDbBusTx_data[i + 3] = 0xFF;
        }

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + 6); /*Write 0xFF for reg 0x115C~0x115E */

        /*Clear burst mode */
        reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        mutex_unlock(&g_mutex);
    } else if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A || g_chip_type == CHIP_TYPE_ILI2118A) {   /*CHIP_TYPE_MSG58XXA not need to execute the following code */
        mutex_lock(&g_mutex);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

        drv_touch_device_hw_reset();

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        /*Enable burst mode */
        reg_set_l_byte_value(0x1608, 0x21);

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x15;
        szDbBusTx_data[2] = 0x20;    /*bank:0x15, addr:h0010 */

        for (i = 0; i < 8; i++) {
            szDbBusTx_data[i + 3] = 0xFF;
        }

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + 8); /*Write 0xFF for reg 0x1510~0x1513 */

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x15;
        szDbBusTx_data[2] = 0x28;    /*bank:0x15, addr:h0014 */

        for (i = 0; i < 16; i++) {
            szDbBusTx_data[i + 3] = 0x00;
        }

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + 16);    /*Write 0x00 for reg 0x1514~0x151B */

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x21;
        szDbBusTx_data[2] = 0x40;    /*bank:0x21, addr:h0020 */

        for (i = 0; i < 8; i++) {
            szDbBusTx_data[i + 3] = 0xFF;
        }

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + 8); /*Write 0xFF for reg 0x2120~0x2123 */

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x21;
        szDbBusTx_data[2] = 0x20;    /*bank:0x21, addr:h0010 */

        for (i = 0; i < 32; i++) {
            szDbBusTx_data[i + 3] = 0xFF;
        }

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + 32);    /*Write 0xFF for reg 0x2110~0x211F */

        /*Clear burst mode */
        reg_set_l_byte_value(0x1608, 0x20);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        mutex_unlock(&g_mutex);
    }
}

int drv_ilitek_get_check_sum(u32 nStartAddr, u32 nEndAddr)
{
    u16 i = 0, n_in_time_count = 100;
    u8 sz_buf[64] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    /*get checksum */
    if ((g_chip_type == CHIP_TYPE_ILI2120) || (g_chip_type == CHIP_TYPE_ILI2121)) {
        out_write(0x4100B, 0x23, 1);
        out_write(0x41009, nEndAddr, 2);
        out_write(0x41000, 0x3B | (nStartAddr << 8), 4);
        out_write(0x041004, 0x66AA5500, 4);

        for (i = 0; i < n_in_time_count; i++) {
            sz_buf[0] = in_write_one_byte(0x41011);

            if ((sz_buf[0] & 0x01) == 0) {
                break;
            }
            mdelay(100);
        }

        return in_write(0x41018);
    }

    return -2;
}

int drv_ilitek_entry_ice_mode(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (out_write(0x181062, 0x0, 0) < 0) /*Entry ICE Mode */
        return -2;

    return 0;
}

int drv_ilitek_ice_mode_initial(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if ((g_chip_type == CHIP_TYPE_ILI2120) || (g_chip_type == CHIP_TYPE_ILI2121)
        || g_chip_type == 0) {
        /*close watch dog */
        if (out_write(0x5200C, 0x0000, 2) < 0)
            return -2;
        if (out_write(0x52020, 0x01, 1) < 0)
            return -2;
        if (out_write(0x52020, 0x00, 1) < 0)
            return -2;
        if (out_write(0x42000, 0x0F154900, 4) < 0)
            return -2;
        if (out_write(0x42014, 0x02, 1) < 0)
            return -2;
        if (out_write(0x42000, 0x00000000, 4) < 0)
            return -2;
        /*---------------------------------*/
        if (out_write(0x041000, 0xab, 1) < 0)
            return -2;
        if (out_write(0x041004, 0x66aa5500, 4) < 0)
            return -2;
        if (out_write(0x04100d, 0x00, 1) < 0)
            return -2;
        if (out_write(0x04100b, 0x03, 1) < 0)
            return -2;
        if (out_write(0x041009, 0x0000, 2) < 0)
            return -2;
    }
    return 0;
}

u16 drv_get_chip_type(void)
{
    s32 rc = 0;
    u32 nTemp = 0;
    u16 nChipType = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Check chip type by using DbBus for MSG22XX/MSG28XX/MSG58XX/MSG58XXA/ILI2117A/ILI2118A. */
    /*Erase TP Flash first */
    rc = db_bus_enter_serial_debug_mode();
    if (rc < 0) {
        DBG(&g_i2c_client->dev,
            "*** db_bus_enter_serial_debug_mode() failed, rc = %d ***\n", rc);

        /*Check chip type by using SmBus for ILI21XX. */
        if (0 == nChipType) {
            slave_i2c_id_dw_i2c = (0x82 >> 1);   /*0x41 */

            drv_ilitek_entry_ice_mode();
            drv_ilitek_ice_mode_initial();
            /*get PID (0xXX000000 is ILI212X) */
            nTemp = in_write(0x4009C);
            DBG(&g_i2c_client->dev, "nTemp=0x%x\n", nTemp);

            if ((nTemp & 0xFFFFFF00) == 0) {
                nChipType =
                    (vfice_reg_read(0xF001) << (8 * 1)) + (vfice_reg_read(0xF000));
                DBG(&g_i2c_client->dev, "nChipType=0x%x\n", nChipType);
                if (0xFFFF == nChipType) {
                    nChipType = CHIP_TYPE_ILI2120;
                }
            }

            exit_ice_mode();

            if ((nChipType != CHIP_TYPE_ILI2120) &&
                (nChipType != CHIP_TYPE_ILI2121)) {
                nChipType = 0;
            }

            g_original_chip_type = nChipType;
        }

        DBG(&g_i2c_client->dev, "*** g_original_chip_type = 0x%x ***\n",
            g_original_chip_type);
        DBG(&g_i2c_client->dev, "*** Chip Type = 0x%x ***\n", nChipType);

        if (0 == nChipType) {   /*If can not get chip type successfully, reset slave_i2c_id_dw_i2c to 0x26 as defalut setting. */
            slave_i2c_id_dw_i2c = (0x4C >> 1);   /*0x26 */
        }

        return nChipType;
    }
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K    /*for MSG22xx only */
    /*Exit flash low power mode */
    reg_set_l_byte_value(0x1619, BIT1);

    /*Change PIU clock to 48MHz */
    reg_set_l_byte_value(0x1E23, BIT6);

    /*Change mcu clock deglitch mux source */
    reg_set_l_byte_value(0x1E54, BIT0);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    /*Set Password */
    reg_set_l_byte_value(0x1616, 0xAA);
    reg_set_l_byte_value(0x1617, 0x55);
    reg_set_l_byte_value(0x1618, 0xA5);
    reg_set_l_byte_value(0x1619, 0x5A);

    /*disable cpu read, in,tial); read */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);
    reg_set_l_byte_value(0x1607, 0x00);

    /*set info block */
    reg_set_l_byte_value(0x1607, 0x08);
    /*set info double buffer */
    reg_set_l_byte_value(0x1604, 0x01);

    /*set eflash mode to read mode */
    reg_set_l_byte_value(0x1606, 0x01);
    reg_set_l_byte_value(0x1610, 0x01);
    reg_set_l_byte_value(0x1611, 0x00);

    /*set read address */
    reg_set_l_byte_value(0x1600, 0x05);
    reg_set_l_byte_value(0x1601, 0x00);

    nChipType = reg_get16_bit_value(0x160A) & 0xFFFF;

    if (nChipType == CHIP_TYPE_ILI2117A || nChipType == CHIP_TYPE_ILI2118A) {
        DBG(&g_i2c_client->dev,
            "----------------------ILI Chip Type=0x%x-------------------------\n",
            nChipType);
        g_original_chip_type = nChipType;
    } else {
        nChipType = reg_get16_bit_value(0x1ECC) & 0xFF;

        DBG(&g_i2c_client->dev,
            "----------------------MSG Chip Type=0x%x-------------------------\n",
            nChipType);
        g_original_chip_type = nChipType;

        if (nChipType != CHIP_TYPE_MSG21XX &&   /*(0x01) */
            nChipType != CHIP_TYPE_MSG21XXA &&  /*(0x02) */
            nChipType != CHIP_TYPE_MSG26XXM &&  /*(0x03) */
            nChipType != CHIP_TYPE_MSG22XX &&   /*(0x7A) */
            nChipType != CHIP_TYPE_MSG28XX &&   /*(0x85) */
            nChipType != CHIP_TYPE_MSG58XXA) {  /*(0xBF) */
            if (nChipType != 0) {
                nChipType = CHIP_TYPE_MSG58XXA;
            }
        }

        if (nChipType == CHIP_TYPE_MSG22XX) {   /*(0x7A) */
            g_msg22xx_chip_revision = reg_get_h_byte_value(0x1ECE);

            DBG(&g_i2c_client->dev, "*** g_msg22xx_chip_revision = 0x%x ***\n",
                g_msg22xx_chip_revision);
        }
    }
    DBG(&g_i2c_client->dev, "*** g_original_chip_type = 0x%x ***\n",
        g_original_chip_type);
    DBG(&g_i2c_client->dev, "*** Chip Type = 0x%x ***\n", nChipType);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return nChipType;
}

void drv_get_customer_firmware_version_by_db_bus(enum emem_type_e eEmemType, u16 *pMajor,
                                          u16 *pMinor, u8 **ppVersion)
{                               /*support MSG28xx only */
    u16 n_read_addr = 0;
    u8 szTmp_data[4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA ||
        g_chip_type == CHIP_TYPE_ILI2117A || g_chip_type == CHIP_TYPE_ILI2118A) {
        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        /*Stop mcu */
        reg_set_l_byte_value(0x0FE6, 0x01);

        if (eEmemType == EMEM_MAIN) {   /*Read SW ID from main block */
            drv_msg28xx_read_eflash_start(0x7FFD, EMEM_MAIN);
            n_read_addr = 0x7FFD;
        } else if (eEmemType == EMEM_INFO) {    /*Read SW ID from info block */
            drv_msg28xx_read_eflash_start(0x81FB, EMEM_INFO);
            n_read_addr = 0x81FB;
        }

        drv_msg28xx_read_eflash_do_read(n_read_addr, &szTmp_data[0]);

        drv_msg28xx_read_eflash_end();

        /*
         * Ex. Major in Main Block :
         * Major low byte at address 0x7FFD
         * 
         * Major in Info Block :
         * Major low byte at address 0x81FB
         */

        *pMajor = (szTmp_data[1] << 8);
        *pMajor |= szTmp_data[0];
        *pMinor = (szTmp_data[3] << 8);
        *pMinor |= szTmp_data[2];

        DBG(&g_i2c_client->dev, "*** Major = %d ***\n", *pMajor);
        DBG(&g_i2c_client->dev, "*** Minor = %d ***\n", *pMinor);

        if (*ppVersion == NULL) {
            *ppVersion = kzalloc(sizeof(u8) * 11, GFP_KERNEL);
        }

        sprintf(*ppVersion, "%05d.%05d", *pMajor, *pMinor);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();
    }
}

/*=============================================================*/
/*LOCAL FUNCTION DEFINITION*/
/*=============================================================*/

static void drv_msg22xx_get_tp_vender_code(u8 *pTpVendorCode)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        u16 nReg_data1, nReg_data2;

        drv_touch_device_hw_reset();

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        /*Stop mcu */
        reg_set_l_byte_value(0x0FE6, 0x01);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        /*Exit flash low power mode */
        reg_set_l_byte_value(0x1619, BIT1);

        /*Change PIU clock to 48MHz */
        reg_set_l_byte_value(0x1E23, BIT6);

        /*Change mcu clock deglitch mux source */
        reg_set_l_byte_value(0x1E54, BIT0);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        /*RIU password */
        reg_set_16bit_value(0x161A, 0xABBA);

        reg_set_16bit_value(0x1600, 0xC1E9);   /*Set start address for tp vendor code on info block(Actually, start reading from 0xC1E8) */

        /*Enable burst mode */
/*reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));*/

        reg_set_l_byte_value(0x160E, 0x01);

        nReg_data1 = reg_get16_bit_value(0x1604);
        nReg_data2 = reg_get16_bit_value(0x1606);

        pTpVendorCode[0] = ((nReg_data1 >> 8) & 0xFF);
        pTpVendorCode[1] = (nReg_data2 & 0xFF);
        pTpVendorCode[2] = ((nReg_data2 >> 8) & 0xFF);

        DBG(&g_i2c_client->dev, "pTpVendorCode[0] = 0x%x , %c\n",
            pTpVendorCode[0], pTpVendorCode[0]);
        DBG(&g_i2c_client->dev, "pTpVendorCode[1] = 0x%x , %c\n",
            pTpVendorCode[1], pTpVendorCode[1]);
        DBG(&g_i2c_client->dev, "pTpVendorCode[2] = 0x%x , %c\n",
            pTpVendorCode[2], pTpVendorCode[2]);

        /*Clear burst mode */
/*reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));*/

        reg_set_16bit_value(0x1600, 0x0000);

        /*Clear RIU password */
        reg_set_16bit_value(0x161A, 0x0000);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        drv_touch_device_hw_reset();
    }
}

u32 drv_msg22xx_get_firmware_crc_by_hardware(enum emem_type_e eEmemType)
{
    u16 nCrcDown = 0;
    u32 nTimeOut = 0;
    u32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

    /*Exit flash low power mode */
    reg_set_l_byte_value(0x1619, BIT1);

    /*Change PIU clock to 48MHz */
    reg_set_l_byte_value(0x1E23, BIT6);

    /*Change mcu clock deglitch mux source */
    reg_set_l_byte_value(0x1E54, BIT0);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    /*RIU password */
    reg_set_16bit_value(0x161A, 0xABBA);

    /*Set PCE high */
    reg_set_l_byte_value(0x1618, 0x40);

    if (eEmemType == EMEM_MAIN) {
        /*Set start address and end address for main block */
        reg_set_16bit_value(0x1600, 0x0000);
        reg_set_16bit_value(0x1640, 0xBFF8);
    } else if (eEmemType == EMEM_INFO) {
        /*Set start address and end address for info block */
        reg_set_16bit_value(0x1600, 0xC000);
        reg_set_16bit_value(0x1640, 0xC1F8);
    }

    /*CRC reset */
    reg_set_16bit_value(0x164E, 0x0001);

    reg_set_16bit_value(0x164E, 0x0000);

    /*Trigger CRC check */
    reg_set_l_byte_value(0x160E, 0x20);
    mdelay(10);

    while (1) {
        DBG(&g_i2c_client->dev, "Wait CRC down\n");

        nCrcDown = reg_get16_bit_value(0x164E);
        if (nCrcDown == 2) {
            break;
        }
        mdelay(10);

        if ((nTimeOut++) > 30) {
            DBG(&g_i2c_client->dev, "Get CRC down failed. Timeout.\n");

            goto GetCRCEnd;
        }
    }

    n_ret_val = reg_get16_bit_value(0x1652);
    n_ret_val = (n_ret_val << 16) | reg_get16_bit_value(0x1650);

GetCRCEnd:

    DBG(&g_i2c_client->dev, "Hardware CRC = 0x%x\n", n_ret_val);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return n_ret_val;
}

static void drv_msg22xx_convert_fw_data_two_dimen_to_one_dimen(u8
                                                       szTwoDimenFw_data[][1024],
                                                       u8 *pOneDimenFw_data)
{
    u32 i, j;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < (MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE + 1); i++) {
        if (i < MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE) { /*i < 48 */
            for (j = 0; j < 1024; j++) {
                pOneDimenFw_data[i * 1024 + j] = szTwoDimenFw_data[i][j];
            }
        } else {                /*i == 48 */

            for (j = 0; j < 512; j++) {
                pOneDimenFw_data[i * 1024 + j] = szTwoDimenFw_data[i][j];
            }
        }
    }
}

static void drv_store_firmware_data(u8 *p_buf, u32 n_size)
{
    u32 n_count = n_size / 1024;
    u32 nRemainder = n_size % 1024;
    u32 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (n_count > 0) {           /*n_size >= 1024 */
        for (i = 0; i < n_count; i++) {
            memcpy(g_fw_data[g_fw_data_count], p_buf + (i * 1024), 1024);

            g_fw_data_count++;
        }

        if (nRemainder > 0) {   /*Handle special firmware size like MSG22XX(48.5KB) */
            DBG(&g_i2c_client->dev, "nRemainder = %d\n", nRemainder);

            memcpy(g_fw_data[g_fw_data_count], p_buf + (i * 1024), nRemainder);

            g_fw_data_count++;
        }
    } else {                    /*n_size < 1024 */

        if (n_size > 0) {
            memcpy(g_fw_data[g_fw_data_count], p_buf, n_size);

            g_fw_data_count++;
        }
    }

    DBG(&g_i2c_client->dev, "*** g_fw_data_count = %d ***\n", g_fw_data_count);

    if (p_buf != NULL) {
        DBG(&g_i2c_client->dev, "*** buf[0] = %c ***\n", p_buf[0]);
    }
}

static u16 drv_msg22xx_get_sw_id(enum emem_type_e eEmemType)
{
    u16 n_ret_val = 0;
    u16 nReg_data1 = 0;
    u16 nReg_data2 = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

	drv_touch_device_hw_reset();

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

    /*Exit flash low power mode */
    reg_set_l_byte_value(0x1619, BIT1);

    /*Change PIU clock to 48MHz */
    reg_set_l_byte_value(0x1E23, BIT6);

    /*Change mcu clock deglitch mux source */
    reg_set_l_byte_value(0x1E54, BIT0);
#else
    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    /*RIU password */
    reg_set_16bit_value(0x161A, 0xABBA);

    if (eEmemType == EMEM_MAIN) {   /*Read SW ID from main block */
        reg_set_16bit_value(0x1600, 0xBFF4);   /*Set start address for main block SW ID */
    } else if (eEmemType == EMEM_INFO) {    /*Read SW ID from info block */
        reg_set_16bit_value(0x1600, 0xC1EC);   /*Set start address for info block SW ID */
    }

    /*
     * Ex. SW ID in Main Block :
     * Major low byte at address 0xBFF4
     * Major high byte at address 0xBFF5
     * 
     * SW ID in Info Block :
     * Major low byte at address 0xC1EC
     * Major high byte at address 0xC1ED
     */

    /*Enable burst mode */
/*reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));*/

    reg_set_l_byte_value(0x160E, 0x01);

    nReg_data1 = reg_get16_bit_value(0x1604);
    nReg_data2 = reg_get16_bit_value(0x1606);

    n_ret_val = ((nReg_data1 >> 8) & 0xFF) << 8;
    n_ret_val |= (nReg_data1 & 0xFF);
    g_firmware_minor = ((nReg_data2 >> 8) & 0xFF) << 8;
    g_firmware_minor |= (nReg_data2 & 0xFF);

    /*Clear burst mode */
/*reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));*/

    reg_set_16bit_value(0x1600, 0x0000);

    /*Clear RIU password */
    reg_set_16bit_value(0x161A, 0x0000);

    DBG(&g_i2c_client->dev, "SW ID = 0x%x\n", n_ret_val);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return n_ret_val;
}

static void drv_msg28xx_convert_fw_data_two_dimen_to_one_dimen(u8
                                                       szTwoDimenFw_data[][1024],
                                                       u8 *pOneDimenFw_data)
{
    u32 i, j;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
        for (j = 0; j < 1024; j++) {
            pOneDimenFw_data[i * 1024 + j] = szTwoDimenFw_data[i][j];
        }
    }
}

static u32 drv_msg22xx_calculate_crc(u8 *pFw_data, u32 n_offset, u32 n_size)
{
    u32 i;
    u32 n_data = 0, nCrc = 0;
    u32 nCrcRule = 0x0C470C06;  /*0000 1100 0100 0111 0000 1100 0000 0110 */

    for (i = 0; i < n_size; i += 4) {
        n_data =
            (pFw_data[n_offset + i]) | (pFw_data[n_offset + i + 1] << 8) |
            (pFw_data[n_offset + i + 2] << 16) | (pFw_data[n_offset + i + 3] << 24);
        nCrc = (nCrc >> 1) ^ (nCrc << 1) ^ (nCrc & nCrcRule) ^ n_data;
    }

    return nCrc;
}

static void drv_msg22xx_access_eflash_init(void)
{
    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1606, 0x20);
    reg_set_l_byte_value(0x1608, 0x20);

    /*Clear PROGRAM erase password */
    reg_set_16bit_value(0x1618, 0xA55A);
}

static void drv_msg22xx_isp_burst_write_eflash_start(u16 nStartAddr, u8 *pFirst_data,
                                                u32 nBlockSize, u16 nPageNum,
                                                enum emem_type_e eEmemType)
{
    u16 nWriteAddr = nStartAddr / 4;
    u8 szDbBusTx_data[3] = { 0 };

    DBG(&g_i2c_client->dev,
        "*** %s() nStartAddr = 0x%x, nBlockSize = %d, nPageNum = %d, eEmemType = %d ***\n",
        __func__, nStartAddr, nBlockSize, nPageNum, eEmemType);

    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);

    /*Set e-flash mode to page write mode */
    reg_set_16bit_value(0x1606, 0x0080);

    /*Set data align */
    reg_set_l_byte_value(0x1640, 0x01);

    if (eEmemType == EMEM_INFO) {
        reg_set_l_byte_value(0x1607, 0x08);
    }

    /*Set double buffer */
    reg_set_l_byte_value(0x1604, 0x01);

    /*Set page write number */
    reg_set_16bit_value(0x161A, nPageNum);

    /*Set e-flash mode trigger(Trigger write mode) */
    reg_set_l_byte_value(0x1606, 0x81);

    /*Set init data */
    reg_set_l_byte_value(0x1602, pFirst_data[0]);
    reg_set_l_byte_value(0x1602, pFirst_data[1]);
    reg_set_l_byte_value(0x1602, pFirst_data[2]);
    reg_set_l_byte_value(0x1602, pFirst_data[3]);

    /*Set initial address(for latch SA, CA) */
    reg_set_16bit_value(0x1600, nWriteAddr);

    /*Set initial address(for latch PA) */
    reg_set_16bit_value(0x1600, nWriteAddr);

    /*Enable burst mode */
    reg_set_l_byte_value(0x1608, 0x21);

    szDbBusTx_data[0] = 0x10;
    szDbBusTx_data[1] = 0x16;
    szDbBusTx_data[2] = 0x02;
    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3);

    szDbBusTx_data[0] = 0x20;
/*szDbBusTx_data[1] = 0x00;*/
/*szDbBusTx_data[2] = 0x00;*/
    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);
}

static void drv_msg22xx_isp_burst_write_eflash_do_write(u8 *pBuffer_data, u32 n_length)
{
    u32 i;
    u8 szDbBusTx_data[3 + MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() n_length = %d ***\n", __func__, n_length);

    szDbBusTx_data[0] = 0x10;
    szDbBusTx_data[1] = 0x16;
    szDbBusTx_data[2] = 0x02;

    for (i = 0; i < n_length; i++) {
        szDbBusTx_data[3 + i] = pBuffer_data[i];
    }
    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3 + n_length);
}

static void drv_msg22xx_isp_burst_write_eflash_end(void)
{
    u8 szDbBusTx_data[1] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    szDbBusTx_data[0] = 0x21;
    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

    szDbBusTx_data[0] = 0x7E;
    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

    /*Clear burst mode */
    reg_set_l_byte_value(0x1608, 0x20);
}

static void drv_msg28xx_write_eflash_start(u16 nStartAddr, u8 *pFirst_data,
                                        enum emem_type_e eEmemType)
{
    u16 nWriteAddr = nStartAddr / 4;

    DBG(&g_i2c_client->dev, "*** %s() nStartAddr = 0x%x, eEmemType = %d ***\n",
        __func__, nStartAddr, eEmemType);

    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);

    /*Set e-flash mode to write mode */
    reg_set_16bit_value(0x1606, 0x0040);

    /*Set data align */
    reg_set_l_byte_value(0x1640, 0x01);

    if (eEmemType == EMEM_INFO) {
        reg_set_l_byte_value(0x1607, 0x08);
    }

    /*Set double buffer */
    reg_set_l_byte_value(0x1604, 0x01);

    /*Set e-flash mode trigger(Trigger write mode) */
    reg_set_l_byte_value(0x1606, 0x81);

    /*Set init data */
    reg_set_l_byte_value(0x1602, pFirst_data[0]);
    reg_set_l_byte_value(0x1602, pFirst_data[1]);
    reg_set_l_byte_value(0x1602, pFirst_data[2]);
    reg_set_l_byte_value(0x1602, pFirst_data[3]);

    /*Set initial address(for latch SA, CA) */
    reg_set_16bit_value(0x1600, nWriteAddr);

    /*Set initial address(for latch PA) */
    reg_set_16bit_value(0x1600, nWriteAddr);
}

static void drv_msg28xx_write_eflash_do_write(u16 nStartAddr, u8 *pBuffer_data)
{
    u16 nWriteAddr = nStartAddr / 4;

    DBG(&g_i2c_client->dev, "*** %s() nWriteAddr = %d ***\n", __func__,
        nWriteAddr);

    /*Write data */
    reg_set_l_byte_value(0x1602, pBuffer_data[0]);
    reg_set_l_byte_value(0x1602, pBuffer_data[1]);
    reg_set_l_byte_value(0x1602, pBuffer_data[2]);
    reg_set_l_byte_value(0x1602, pBuffer_data[3]);

    /*Set address */
    reg_set_16bit_value(0x1600, nWriteAddr);
}

static void drv_msg28xx_write_eflash_end(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Do nothing */
}

void drv_msg28xx_read_eflash_start(u16 nStartAddr, enum emem_type_e eEmemType)
{
    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);

    reg_set_l_byte_value(0x1606, 0x02);

    reg_set_16bit_value(0x1600, nStartAddr);

    if (eEmemType == EMEM_MAIN) {
        /*Set main block */
        reg_set_l_byte_value(0x1607, 0x00);

        /*Set main double buffer */
        reg_set_l_byte_value(0x1604, 0x01);

        /*Set e-flash mode to read mode for main */
        reg_set_16bit_value(0x1606, 0x0001);
    } else if (eEmemType == EMEM_INFO) {
        /*Set info block */
        reg_set_l_byte_value(0x1607, 0x08);

        /*Set info double buffer */
        reg_set_l_byte_value(0x1604, 0x01);

        /*Set e-flash mode to read mode for info */
        reg_set_16bit_value(0x1606, 0x0801);
    }
}

void drv_msg28xx_read_eflash_do_read(u16 n_read_addr, u8 *pRead_data)
{
    u16 nReg_data1 = 0, nReg_data2 = 0;

    DBG(&g_i2c_client->dev, "*** %s() n_read_addr = 0x%x ***\n", __func__,
        n_read_addr);

    /*Set read address */
    reg_set_16bit_value(0x1600, n_read_addr);

    /*Read 16+16 bits */
    nReg_data1 = reg_get16_bit_value(0x160A);
    nReg_data2 = reg_get16_bit_value(0x160C);

    pRead_data[0] = nReg_data1 & 0xFF;
    pRead_data[1] = (nReg_data1 >> 8) & 0xFF;
    pRead_data[2] = nReg_data2 & 0xFF;
    pRead_data[3] = (nReg_data2 >> 8) & 0xFF;
}

void drv_msg28xx_read_eflash_end(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Set read done */
    reg_set_l_byte_value(0x1606, 0x02);

    /*Unset info flag */
    reg_set_l_byte_value(0x1607, 0x00);

    /*Clear address */
    reg_set_16bit_value(0x1600, 0x0000);
}

static void drv_msg28xx_get_sfr_addr_3_value(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);

    /*Set e-flash mode to read mode */
    reg_set_l_byte_value(0x1606, 0x01);
    reg_set_l_byte_value(0x1610, 0x01);
    reg_set_l_byte_value(0x1607, 0x20);

    /*Set read address */
    reg_set_l_byte_value(0x1600, 0x03);
    reg_set_l_byte_value(0x1601, 0x00);

    g_sfr_addr3_byte0_1_value = reg_get16_bit_value(0x160A);
    g_sfr_addr3_byte2_3_value = reg_get16_bit_value(0x160C);

    DBG(&g_i2c_client->dev,
        "g_sfr_addr3_byte0_1_value = 0x%4X, g_sfr_addr3_byte2_3_value = 0x%4X\n",
        g_sfr_addr3_byte0_1_value, g_sfr_addr3_byte2_3_value);
}

static void drv_msg28xx_unset_protect_bit(void)
{
    u8 nB0, nB1, nB2, nB3;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_msg28xx_get_sfr_addr_3_value();

    nB0 = g_sfr_addr3_byte0_1_value & 0xFF;
    nB1 = (g_sfr_addr3_byte0_1_value & 0xFF00) >> 8;

    nB2 = g_sfr_addr3_byte2_3_value & 0xFF;
    nB3 = (g_sfr_addr3_byte2_3_value & 0xFF00) >> 8;

    DBG(&g_i2c_client->dev,
        "nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", nB0, nB1, nB2,
        nB3);

    nB2 = nB2 & 0xBF;           /*10111111 */
    nB3 = nB3 & 0xFC;           /*11111100 */

    DBG(&g_i2c_client->dev,
        "nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", nB0, nB1, nB2,
        nB3);

    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);
    reg_set_l_byte_value(0x1610, 0x80);
    reg_set_l_byte_value(0x1607, 0x10);

    /*Trigger SFR write */
    reg_set_l_byte_value(0x1606, 0x01);

    /*Set write data */
    reg_set_l_byte_value(0x1602, nB0);
    reg_set_l_byte_value(0x1602, nB1);
    reg_set_l_byte_value(0x1602, nB2);
    reg_set_l_byte_value(0x1602, nB3);

    /*Set write address */
    reg_set_l_byte_value(0x1600, 0x03);
    reg_set_l_byte_value(0x1601, 0x00);

    /*Set TM mode = 0 */
    reg_set_l_byte_value(0x1607, 0x00);

#ifdef CONFIG_ENABLE_HIGH_SPEED_ISP_MODE
    reg_set_l_byte_value(0x1606, 0x01);
    reg_set_l_byte_value(0x1606, 0x20);
#endif /*CONFIG_ENABLE_HIGH_SPEED_ISP_MODE */
}

void drv_msg_28xx_set_protect_bit(void)
{
    u8 nB0, nB1, nB2, nB3;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    nB0 = g_sfr_addr3_byte0_1_value & 0xFF;
    nB1 = (g_sfr_addr3_byte0_1_value & 0xFF00) >> 8;

    nB2 = g_sfr_addr3_byte2_3_value & 0xFF;
    nB3 = (g_sfr_addr3_byte2_3_value & 0xFF00) >> 8;

    DBG(&g_i2c_client->dev,
        "nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", nB0, nB1, nB2,
        nB3);

    /*Disable cpu read flash */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);
    reg_set_l_byte_value(0x1610, 0x80);
    reg_set_l_byte_value(0x1607, 0x10);

    /*Trigger SFR write */
    reg_set_l_byte_value(0x1606, 0x01);

    /*Set write data */
    reg_set_l_byte_value(0x1602, nB0);
    reg_set_l_byte_value(0x1602, nB1);
    reg_set_l_byte_value(0x1602, nB2);
    reg_set_l_byte_value(0x1602, nB3);

    /*Set write address */
    reg_set_l_byte_value(0x1600, 0x03);
    reg_set_l_byte_value(0x1601, 0x00);
    reg_set_l_byte_value(0x1606, 0x02);
}

void drv_msg28xx_erase_emem(enum emem_type_e eEmemType)
{
    u32 nInfoAddr = 0x20;
    u32 nTimeOut = 0;
    u8 nReg_data = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    drv_touch_device_hw_reset();

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    DBG(&g_i2c_client->dev, "Erase start\n");

    drv_msg22xx_access_eflash_init();

    /*Stop mcu */
    reg_set_l_byte_value(0x0FE6, 0x01);

    /*Set PROGRAM erase password */
    reg_set_16bit_value(0x1618, 0x5AA5);

    drv_msg28xx_unset_protect_bit();

    if (eEmemType == EMEM_MAIN) {   /*128KB */
        DBG(&g_i2c_client->dev, "Erase main block\n");

        /*Set main block */
        reg_set_l_byte_value(0x1607, 0x00);

        /*Set e-flash mode to erase mode */
        reg_set_l_byte_value(0x1606, 0xC0);

        /*Set page erase main */
        reg_set_l_byte_value(0x1607, 0x03);

        /*e-flash mode trigger */
        reg_set_l_byte_value(0x1606, 0xC1);

        nTimeOut = 0;
        while (1) {             /*Wait erase done */
            nReg_data = reg_get_l_byte_value(0x160E);
            nReg_data = (nReg_data & BIT3);

            DBG(&g_i2c_client->dev, "Wait erase done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT3) {
                break;
            }

            mdelay(10);

            if ((nTimeOut++) > 10) {
                DBG(&g_i2c_client->dev, "Erase main block failed. Timeout.\n");

                goto EraseEnd;
            }
        }
    } else if (eEmemType == EMEM_INFO) {    /*2KB */
        DBG(&g_i2c_client->dev, "Erase info block\n");

        /*Set info block */
        reg_set_l_byte_value(0x1607, 0x08);

        /*Set info double buffer */
        reg_set_l_byte_value(0x1604, 0x01);

        /*Set e-flash mode to erase mode */
        reg_set_l_byte_value(0x1606, 0xC0);

        /*Set page erase info */
        reg_set_l_byte_value(0x1607, 0x09);

        for (nInfoAddr = 0x20; nInfoAddr <= MSG28XX_EMEM_INFO_MAX_ADDR;
             nInfoAddr += 0x20) {
            DBG(&g_i2c_client->dev, "nInfoAddr = 0x%x\n", nInfoAddr);   /*add for debug */

            /*Set address */
            reg_set_16bit_value(0x1600, nInfoAddr);

            /*e-flash mode trigger */
            reg_set_l_byte_value(0x1606, 0xC1);

            nTimeOut = 0;
            while (1) {         /*Wait erase done */
                nReg_data = reg_get_l_byte_value(0x160E);
                nReg_data = (nReg_data & BIT3);

                DBG(&g_i2c_client->dev,
                    "Wait erase done flag nReg_data = 0x%x\n", nReg_data);

                if (nReg_data == BIT3) {
                    break;
                }

                mdelay(10);

                if ((nTimeOut++) > 10) {
                    DBG(&g_i2c_client->dev,
                        "Erase info block failed. Timeout.\n");

                    /*Set main block */
                    reg_set_l_byte_value(0x1607, 0x00);

                    goto EraseEnd;
                }
            }
        }

        /*Set main block */
        reg_set_l_byte_value(0x1607, 0x00);
    }

EraseEnd:

    drv_msg_28xx_set_protect_bit();

    reg_set_l_byte_value(0x1606, 0x00);
    reg_set_l_byte_value(0x1607, 0x00);

    /*Clear PROGRAM erase password */
    reg_set_16bit_value(0x1618, 0xA55A);

    DBG(&g_i2c_client->dev, "Erase end\n");

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();
}

static void drv_msg28xx_program_emem(enum emem_type_e eEmemType)
{
    u32 i, j;
#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME) || defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
    u32 k;
#endif
    u32 nPageNum = 0, n_length = 0, nIndex = 0, nWordNum = 0;
    u32 nRetryTime = 0;
    u8 nReg_data = 0;
    u8 szFirst_data[MSG28XX_EMEM_SIZE_BYTES_ONE_WORD] = { 0 };
    u8 szBuffer_data[MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE] = { 0 };
#ifdef CONFIG_ENABLE_HIGH_SPEED_ISP_MODE
    u8 szWrite_data[3 + MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE * 2] = { 0 };
#endif /*CONFIG_ENABLE_HIGH_SPEED_ISP_MODE */

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    drv_touch_device_hw_reset();

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    DBG(&g_i2c_client->dev, "Program start\n");

    drv_msg22xx_access_eflash_init();

    /*Stop mcu */
    reg_set_l_byte_value(0x0FE6, 0x01);

    /*Set PROGRAM erase password */
    reg_set_16bit_value(0x1618, 0x5AA5);

    drv_msg28xx_unset_protect_bit();

    if (eEmemType == EMEM_MAIN) {   /*Program main block */
        DBG(&g_i2c_client->dev, "Program main block\n");

#ifdef CONFIG_ENABLE_HIGH_SPEED_ISP_MODE
        nPageNum = (MSG28XX_FW_MAIN_BLOCK_SIZE * 1024) / MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;    /*128*1024/128=1024 */

        /*Set ISP mode */
        reg_set_16bit_value_on(0x1EBE, BIT15);

        reg_set_l_byte_value(0x1604, 0x01);

        reg_set_16bit_value(0x161A, nPageNum);
        reg_set_16bit_value(0x1600, 0x0000);   /*Set initial address */
        reg_set_16bit_value_on(0x3C00, BIT0);   /*Disable INT GPIO mode */
        reg_set_16bit_value_on(0x1EA0, BIT1);   /*Set ISP INT enable */
        reg_set_16bit_value(0x1E34, 0x0000);   /*Set DQMem start address */

        drv_read_read_dq_mem_start();

        szWrite_data[0] = 0x10;
        szWrite_data[1] = 0x00;
        szWrite_data[2] = 0x00;

        n_length = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE * 2; /*128*2=256 */

        for (j = 0; j < n_length; j++) {
            szWrite_data[3 + j] = g_fw_data[0][j];
        }
        iic_write_data(slave_i2c_id_db_bus, &szWrite_data[0], 3 + n_length); /*Write the first two pages(page 0 & page 1) */

        drv_read_read_dq_mem_end();

        reg_set_16bit_value_on(0x1EBE, BIT15);  /*Set ISP mode */
        reg_set_16bit_value_on(0x1608, BIT0);   /*Set burst mode */
        reg_set_16bit_value_on(0x161A, BIT13);  /*Set ISP trig */

        udelay(2000);           /*delay about 2ms */

        drv_read_read_dq_mem_start();

        n_length = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE; /*128 */

        for (i = 2; i < nPageNum; i++) {
            if (i == 2) {
                szWrite_data[0] = 0x10;
                szWrite_data[1] = 0x00;
                szWrite_data[2] = 0x00;

                for (j = 0; j < n_length; j++) {
                    szWrite_data[3 + j] =
                        g_fw_data[i / 8][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE *
                                        (i - (8 * (i / 8))) + j];
                }

                iic_write_data(slave_i2c_id_db_bus, &szWrite_data[0], 3 + n_length);
            } else if (i == (nPageNum - 1)) {
                szWrite_data[0] = 0x10;
                szWrite_data[1] = 0x00;
                szWrite_data[2] = 0x80;

                for (j = 0; j < n_length; j++) {
                    szWrite_data[3 + j] =
                        g_fw_data[i / 8][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE *
                                        (i - (8 * (i / 8))) + j];
                }

                szWrite_data[3 + 128] = 0xFF;
                szWrite_data[3 + 129] = 0xFF;
                szWrite_data[3 + 130] = 0xFF;
                szWrite_data[3 + 131] = 0xFF;

                iic_write_data(slave_i2c_id_db_bus, &szWrite_data[0],
                             3 + n_length + 4);
            } else {
/*szWrite_data[0] = 0x10;*/
/*szWrite_data[1] = 0x00;*/
                if (szWrite_data[2] == 0x00) {
                    szWrite_data[2] = 0x80;
                } else {        /*szWrite_data[2] == 0x80 */

                    szWrite_data[2] = 0x00;
                }

                for (j = 0; j < n_length; j++) {
                    szWrite_data[3 + j] =
                        g_fw_data[i / 8][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE *
                                        (i - (8 * (i / 8))) + j];
                }

                iic_write_data(slave_i2c_id_db_bus, &szWrite_data[0], 3 + n_length);
            }
        }

        drv_read_read_dq_mem_end();

#else

#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME)
        nPageNum = (MSG28XX_FW_MAIN_BLOCK_SIZE * 1024) / 8;   /*128*1024/8=16384 */
#elif defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
        nPageNum = (MSG28XX_FW_MAIN_BLOCK_SIZE * 1024) / 32;  /*128*1024/32=4096 */
#else /*UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
        nPageNum = (MSG28XX_FW_MAIN_BLOCK_SIZE * 1024) / MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE; // 128*1024/128=1024
#endif

        nIndex = 0;

        for (i = 0; i < nPageNum; i++) {
            if (i == 0) {
                /*Read first data 4 bytes */
                n_length = MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

                szFirst_data[0] = g_fw_data[0][0];
                szFirst_data[1] = g_fw_data[0][1];
                szFirst_data[2] = g_fw_data[0][2];
                szFirst_data[3] = g_fw_data[0][3];

                drv_msg22xx_isp_burst_write_eflash_start(nIndex, &szFirst_data[0],
                                                    MSG28XX_FW_MAIN_BLOCK_SIZE
                                                    * 1024, nPageNum,
                                                    EMEM_MAIN);

                nIndex += n_length;

#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME)
                n_length = 8 - MSG28XX_EMEM_SIZE_BYTES_ONE_WORD; /*4 = 8 - 4 */
#elif defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
                n_length = 32 - MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;    /*28 = 32 - 4 */
#else /*UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                n_length = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE - MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;  /*124 = 128 - 4 */
#endif

                for (j = 0; j < n_length; j++) {
                    szBuffer_data[j] = g_fw_data[0][4 + j];
                }
            } else {
#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME)
                n_length = 8;
#elif defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
                n_length = 32;
#else /*UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                n_length = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE; /*128 */
#endif

                for (j = 0; j < n_length; j++) {
#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME)
                    szBuffer_data[j] =
                        g_fw_data[i / 128][8 * (i - (128 * (i / 128))) + j];
#elif defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
                    szBuffer_data[j] =
                        g_fw_data[i / 32][32 * (i - (32 * (i / 32))) + j];
#else /*UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                    szBuffer_data[j] =
                        g_fw_data[i / 8][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE *
                                        (i - (8 * (i / 8))) + j];
#endif
                }
            }

            drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0], n_length);

            udelay(2000);       /*delay about 2ms */

            nIndex += n_length;
        }

        drv_msg22xx_isp_burst_write_eflash_end();

        /*Set write done */
        reg_set_16bit_value_on(0x1606, BIT2);

        /*Check RBB */
        nReg_data = reg_get_l_byte_value(0x160E);
        nRetryTime = 0;

        while ((nReg_data & BIT3) != BIT3) {
            mdelay(10);

            nReg_data = reg_get_l_byte_value(0x160E);

            if (nRetryTime++ > 100) {
                DBG(&g_i2c_client->dev,
                    "main block can't wait write to done.\n");

                goto ProgramEnd;
            }
        }
#endif /*CONFIG_ENABLE_HIGH_SPEED_ISP_MODE */
    } else if (eEmemType == EMEM_INFO) {    /*Program info block */
        DBG(&g_i2c_client->dev, "Program info block\n");

        nPageNum = (MSG28XX_FIRMWARE_INFO_BLOCK_SIZE * 1024) / MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;    /*2*1024/128=16 */

        nIndex = 0;
        nIndex += MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE; /*128 */

        /*Skip firt page(page 0) & Update page 1~14 by isp burst write mode */
        for (i = 1; i < (nPageNum - 1); i++) {  /*Skip the first 128 byte and the last 128 byte of info block */
            if (i == 1) {
                /*Read first data 4 bytes */
                n_length = MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

                szFirst_data[0] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                    [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE];
                szFirst_data[1] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                    [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 1];
                szFirst_data[2] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                    [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 2];
                szFirst_data[3] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                    [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 3];

                drv_msg22xx_isp_burst_write_eflash_start(nIndex, &szFirst_data[0],
                                                    MSG28XX_FIRMWARE_INFO_BLOCK_SIZE
                                                    * 1024, nPageNum - 1,
                                                    EMEM_INFO);

                nIndex += n_length;

                n_length = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE - MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;  /*124 = 128 - 4 */

#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME)
                for (j = 0; j < (n_length / 8); j++) {   /*124/8 = 15 */
                    for (k = 0; k < 8; k++) {
                        szBuffer_data[k] =
                            g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                            [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 4 +
                             (8 * j) + k];
                    }

                    drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0], 8);

                    udelay(2000);   /*delay about 2ms */
                }

                for (k = 0; k < (n_length % 8); k++) {   /*124%8 = 4 */
                    szBuffer_data[k] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                        [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 4 + (8 * j) +
                         k];
                }

                drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0], (n_length % 8)); /*124%8 = 4 */

                udelay(2000);   /*delay about 2ms */
#elif defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
                for (j = 0; j < (n_length / 32); j++) {  /*124/8 = 3 */
                    for (k = 0; k < 32; k++) {
                        szBuffer_data[k] =
                            g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                            [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 4 +
                             (32 * j) + k];
                    }

                    drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0], 32);

                    udelay(2000);   /*delay about 2ms */
                }

                for (k = 0; k < (n_length % 32); k++) {  /*124%32 = 28 */
                    szBuffer_data[k] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                        [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 4 + (32 * j) +
                         k];
                }

                drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0], (n_length % 32));    /*124%8 = 28 */

                udelay(2000);   /*delay about 2ms */
#else /*UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                for (j = 0; j < n_length; j++) {
                    szBuffer_data[j] = g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                        [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 4 + j];
                }
#endif
            } else {
                n_length = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE; /*128 */

#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME)
                if (i < 8) {    /*1 < i < 8 */
                    for (j = 0; j < (n_length / 8); j++) {   /*128/8 = 16 */
                        for (k = 0; k < 8; k++) {
                            szBuffer_data[k] =
                                g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                                [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE * i +
                                 (8 * j) + k];
                        }

                        drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0],
                                                              8);

                        udelay(2000);   /*delay about 2ms */
                    }
                } else {        /*i >= 8 */

                    for (j = 0; j < (n_length / 8); j++) {   /*128/8 = 16 */
                        for (k = 0; k < 8; k++) {
                            szBuffer_data[k] =
                                g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                                         1][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE
                                            * (i - 8) + (8 * j) + k];
                        }

                        drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0],
                                                              8);

                        udelay(2000);   /*delay about 2ms */
                    }
                }
#elif defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
                if (i < 8) {    /*1 < i < 8 */
                    for (j = 0; j < (n_length / 32); j++) {  /*128/32 = 4 */
                        for (k = 0; k < 32; k++) {
                            szBuffer_data[k] =
                                g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                                [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE * i +
                                 (32 * j) + k];
                        }

                        drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0],
                                                              32);

                        udelay(2000);   /*delay about 2ms */
                    }
                } else {        /*i >= 8 */

                    for (j = 0; j < (n_length / 32); j++) {  /*128/32 = 4 */
                        for (k = 0; k < 32; k++) {
                            szBuffer_data[k] =
                                g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                                         1][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE
                                            * (i - 8) + (32 * j) + k];
                        }

                        drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0],
                                                              32);

                        udelay(2000);   /*delay about 2ms */
                    }
                }
#else /*UPDATE FIRMWARE WITH 128 BYTE EACH TIME */
                if (i < 8) {    /*1 < i < 8 */
                    for (j = 0; j < n_length; j++) {
                        szBuffer_data[j] =
                            g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE]
                            [MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE * i + j];
                    }
                } else {        /*i >= 8 */

                    for (j = 0; j < n_length; j++) {
                        szBuffer_data[j] =
                            g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                                     1][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE *
                                        (i - 8) + j];
                    }
                }
#endif
            }

#if defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_8_BYTE_EACH_TIME) || defined(CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_32_BYTE_EACH_TIME)
            /*Do nothing here */
#else
            drv_msg22xx_isp_burst_write_eflash_do_write(&szBuffer_data[0], n_length);

            udelay(2000);       /*delay about 2ms */
#endif
            nIndex += n_length;
        }

        drv_msg22xx_isp_burst_write_eflash_end();

        /*Set write done */
        reg_set_16bit_value_on(0x1606, BIT2);

        /*Check RBB */
        nReg_data = reg_get_l_byte_value(0x160E);
        nRetryTime = 0;

        while ((nReg_data & BIT3) != BIT3) {
            mdelay(10);

            nReg_data = reg_get_l_byte_value(0x160E);

            if (nRetryTime++ > 100) {
                DBG(&g_i2c_client->dev,
                    "Info block page 1~14 can't wait write to done.\n");

                goto ProgramEnd;
            }
        }

        reg_set_16Bit_value_off(0x1EBE, BIT15);

        /*Update page 15 by write mode */
        nIndex = 15 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;
        nWordNum = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE / MSG28XX_EMEM_SIZE_BYTES_ONE_WORD; /*128/4=32 */
        n_length = MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

        for (i = 0; i < nWordNum; i++) {
            if (i == 0) {
                szFirst_data[0] =
                    g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                             1][7 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE];
                szFirst_data[1] =
                    g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                             1][7 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 1];
                szFirst_data[2] =
                    g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                             1][7 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 2];
                szFirst_data[3] =
                    g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                             1][7 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE + 3];

                drv_msg28xx_write_eflash_start(nIndex, &szFirst_data[0], EMEM_INFO);
            } else {
                for (j = 0; j < n_length; j++) {
                    szFirst_data[j] =
                        g_fw_data[MSG28XX_FW_MAIN_BLOCK_SIZE +
                                 1][7 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE +
                                    (4 * i) + j];
                }

                drv_msg28xx_write_eflash_do_write(nIndex, &szFirst_data[0]);
            }

            udelay(2000);       /*delay about 2ms */

            nIndex += n_length;
        }

        drv_msg28xx_write_eflash_end();

        /*Set write done */
        reg_set_16bit_value_on(0x1606, BIT2);

        /*Check RBB */
        nReg_data = reg_get_l_byte_value(0x160E);
        nRetryTime = 0;

        while ((nReg_data & BIT3) != BIT3) {
            mdelay(10);

            nReg_data = reg_get_l_byte_value(0x160E);

            if (nRetryTime++ > 100) {
                DBG(&g_i2c_client->dev,
                    "Info block page 15 can't wait write to done.\n");

                goto ProgramEnd;
            }
        }
    }

ProgramEnd:

    drv_msg_28xx_set_protect_bit();

    /*Clear PROGRAM erase password */
    reg_set_16bit_value(0x1618, 0xA55A);

    DBG(&g_i2c_client->dev, "Program end\n");

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();
}

static u16 drv_msg28xx_get_sw_id(enum emem_type_e eEmemType)
{
    u16 n_ret_val = 0;
    u16 n_read_addr = 0;
    u8 szTmp_data[4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

    if (eEmemType == EMEM_MAIN) {   /*Read SW ID from main block */
        drv_msg28xx_read_eflash_start(0x7FFD, EMEM_MAIN);
        n_read_addr = 0x7FFD;
    } else if (eEmemType == EMEM_INFO) {    /*Read SW ID from info block */
        drv_msg28xx_read_eflash_start(0x81FB, EMEM_INFO);
        n_read_addr = 0x81FB;
    }

    drv_msg28xx_read_eflash_do_read(n_read_addr, &szTmp_data[0]);

    drv_msg28xx_read_eflash_end();

    /*
     * Ex. SW ID in Main Block :
     * Major low byte at address 0x7FFD
     * 
     * SW ID in Info Block :
     * Major low byte at address 0x81FB
     */

    n_ret_val = (szTmp_data[1] << 8);
    n_ret_val |= szTmp_data[0];

    DBG(&g_i2c_client->dev, "SW ID = 0x%x\n", n_ret_val);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return n_ret_val;
}

static u32 drv_msg28xx_get_firmware_crc_by_hardware(enum emem_type_e eEmemType)
{
    u32 n_ret_val = 0;
    u32 nRetryTime = 0;
    u32 nCrcEndAddr = 0;
    u16 nCrcDown = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    drv_touch_device_hw_reset();

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    drv_msg22xx_access_eflash_init();

    if (eEmemType == EMEM_MAIN) {
        /*Disable cpu read flash */
        reg_set_l_byte_value(0x1608, 0x20);
        reg_set_l_byte_value(0x1606, 0x20);

        /*Set read flag */
        reg_set_16bit_value(0x1610, 0x0001);

        /*Mode reset main block */
        reg_set_16bit_value(0x1606, 0x0000);

        /*CRC reset */
        reg_set_16bit_value(0x1620, 0x0002);

        reg_set_16bit_value(0x1620, 0x0000);

        /*Set CRC e-flash block start address => Main Block : 0x0000 ~ 0x7FFE */
        reg_set_16bit_value(0x1600, 0x0000);

        nCrcEndAddr = (MSG28XX_FW_MAIN_BLOCK_SIZE * 1024) / 4 - 2;

        reg_set_16bit_value(0x1622, nCrcEndAddr);

        /*Trigger CRC check */
        reg_set_16bit_value(0x1620, 0x0001);

        nCrcDown = reg_get16_bit_value(0x1620);

        nRetryTime = 0;
        while ((nCrcDown >> 15) == 0) {
            mdelay(10);

            nCrcDown = reg_get16_bit_value(0x1620);
            nRetryTime++;

            if (nRetryTime > 30) {
                DBG(&g_i2c_client->dev, "Wait main block nCrcDown failed.\n");
                break;
            }
        }

        n_ret_val = reg_get16_bit_value(0x1626);
        n_ret_val = (n_ret_val << 16) | reg_get16_bit_value(0x1624);
    } else if (eEmemType == EMEM_INFO) {
        /*Disable cpu read flash */
        reg_set_l_byte_value(0x1608, 0x20);
        reg_set_l_byte_value(0x1606, 0x20);

        /*Set read flag */
        reg_set_16bit_value(0x1610, 0x0001);

        /*Mode reset info block */
        reg_set_16bit_value(0x1606, 0x0800);

        reg_set_l_byte_value(0x1604, 0x01);

        /*CRC reset */
        reg_set_16bit_value(0x1620, 0x0002);

        reg_set_16bit_value(0x1620, 0x0000);

        /*Set CRC e-flash block start address => Info Block : 0x0020 ~ 0x01FE */
        reg_set_16bit_value(0x1600, 0x0020);
        reg_set_16bit_value(0x1622, 0x01FE);

        /*Trigger CRC check */
        reg_set_16bit_value(0x1620, 0x0001);

        nCrcDown = reg_get16_bit_value(0x1620);

        nRetryTime = 0;
        while ((nCrcDown >> 15) == 0) {
            mdelay(10);

            nCrcDown = reg_get16_bit_value(0x1620);
            nRetryTime++;

            if (nRetryTime > 30) {
                DBG(&g_i2c_client->dev, "Wait info block nCrcDown failed.\n");
                break;
            }
        }

        n_ret_val = reg_get16_bit_value(0x1626);
        n_ret_val = (n_ret_val << 16) | reg_get16_bit_value(0x1624);
    }

    DBG(&g_i2c_client->dev, "Hardware CRC = 0x%x\n", n_ret_val);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return n_ret_val;
}

static u32 drv_msg28xx_retrieve_firmware_crc_from_eflash(enum emem_type_e eEmemType)
{
    u32 n_ret_val = 0;
    u16 n_read_addr = 0;
    u8 szTmp_data[4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

    if (eEmemType == EMEM_MAIN) {   /*Read main block CRC(128KB-4) from main block */
        drv_msg28xx_read_eflash_start(0x7FFF, EMEM_MAIN);
        n_read_addr = 0x7FFF;
    } else if (eEmemType == EMEM_INFO) {    /*Read info block CRC(2KB-MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE-4) from info block */
        drv_msg28xx_read_eflash_start(0x81FF, EMEM_INFO);
        n_read_addr = 0x81FF;
    }

    drv_msg28xx_read_eflash_do_read(n_read_addr, &szTmp_data[0]);

    DBG(&g_i2c_client->dev, "szTmp_data[0] = 0x%x\n", szTmp_data[0]); /*add for debug */
    DBG(&g_i2c_client->dev, "szTmp_data[1] = 0x%x\n", szTmp_data[1]); /*add for debug */
    DBG(&g_i2c_client->dev, "szTmp_data[2] = 0x%x\n", szTmp_data[2]); /*add for debug */
    DBG(&g_i2c_client->dev, "szTmp_data[3] = 0x%x\n", szTmp_data[3]); /*add for debug */

    drv_msg28xx_read_eflash_end();

    n_ret_val = (szTmp_data[3] << 24);
    n_ret_val |= (szTmp_data[2] << 16);
    n_ret_val |= (szTmp_data[1] << 8);
    n_ret_val |= szTmp_data[0];

    DBG(&g_i2c_client->dev, "CRC = 0x%x\n", n_ret_val);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return n_ret_val;
}

static u32 drv_msg28xx_retrieve_firmware_crc_from_bin_file(u8 szTmpBuf[][1024],
                                                     enum emem_type_e eEmemType)
{
    u32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    if (szTmpBuf != NULL) {
        if (eEmemType == EMEM_MAIN) {
            n_ret_val = szTmpBuf[127][1023];
            n_ret_val = (n_ret_val << 8) | szTmpBuf[127][1022];
            n_ret_val = (n_ret_val << 8) | szTmpBuf[127][1021];
            n_ret_val = (n_ret_val << 8) | szTmpBuf[127][1020];
        } else if (eEmemType == EMEM_INFO) {
            n_ret_val = szTmpBuf[129][1023];
            n_ret_val = (n_ret_val << 8) | szTmpBuf[129][1022];
            n_ret_val = (n_ret_val << 8) | szTmpBuf[129][1021];
            n_ret_val = (n_ret_val << 8) | szTmpBuf[129][1020];
        }
    }

    return n_ret_val;
}

static s32 drv_msg28xx_check_firmware_bin_integrity(u8 szFw_data[][1024])
{
    u32 nCrcMain = 0, nCrcMainBin = 0;
    u32 nCrcInfo = 0, nCrcInfoBin = 0;
    u32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_msg28xx_convert_fw_data_two_dimen_to_one_dimen(szFw_data, g_one_dimen_fw_data);

    /* Calculate main block CRC & info block CRC by device driver itself */
    nCrcMain =
        drv_msg22xx_calculate_crc(g_one_dimen_fw_data, 0,
                                MSG28XX_FW_MAIN_BLOCK_SIZE * 1024 -
                                MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);
    nCrcInfo =
        drv_msg22xx_calculate_crc(g_one_dimen_fw_data,
                                MSG28XX_FW_MAIN_BLOCK_SIZE * 1024 +
                                MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE,
                                MSG28XX_FIRMWARE_INFO_BLOCK_SIZE * 1024 -
                                MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE -
                                MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);

    /* Read main block CRC & info block CRC from firmware bin file */
    nCrcMainBin =
        drv_msg28xx_retrieve_firmware_crc_from_bin_file(szFw_data, EMEM_MAIN);
    nCrcInfoBin =
        drv_msg28xx_retrieve_firmware_crc_from_bin_file(szFw_data, EMEM_INFO);

    DBG(&g_i2c_client->dev,
        "nCrcMain=0x%x, nCrcInfo=0x%x, nCrcMainBin=0x%x, nCrcInfoBin=0x%x\n",
        nCrcMain, nCrcInfo, nCrcMainBin, nCrcInfoBin);

    if ((nCrcMainBin != nCrcMain) || (nCrcInfoBin != nCrcInfo)) {
        DBG(&g_i2c_client->dev,
            "CHECK FIRMWARE BIN FILE INTEGRITY FAILED. CANCEL UPDATE FIRMWARE.\n");

        n_ret_val = -1;
    } else {
        DBG(&g_i2c_client->dev,
            "CHECK FIRMWARE BIN FILE INTEGRITY SUCCESS. PROCEED UPDATE FIRMWARE.\n");

        n_ret_val = 0;
    }

    return n_ret_val;
}

static s32 drv_msg28xx_update_firmware(u8 szFw_data[][1024], enum emem_type_e eEmemType)
{
    u32 nCrcMain = 0, nCrcMainHardware = 0, nCrcMainEflash = 0;
    u32 nCrcInfo = 0, nCrcInfoHardware = 0, nCrcInfoEflash = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    if (drv_msg28xx_check_firmware_bin_integrity(szFw_data) < 0) {
        DBG(&g_i2c_client->dev,
            "CHECK FIRMWARE BIN FILE INTEGRITY FAILED. CANCEL UPDATE FIRMWARE.\n");

        g_fw_data_count = 0;      /*Reset g_fw_data_count to 0 */

        drv_touch_device_hw_reset();

        return -1;
    }

    g_is_update_firmware = 0x01;  /*Set flag to 0x01 for indicating update firmware is processing */

     /**/
        /*Erase */
         /**/ if (eEmemType == EMEM_ALL) {
        drv_msg28xx_erase_emem(EMEM_MAIN);
        drv_msg28xx_erase_emem(EMEM_INFO);
    } else if (eEmemType == EMEM_MAIN) {
        drv_msg28xx_erase_emem(EMEM_MAIN);
    } else if (eEmemType == EMEM_INFO) {
        drv_msg28xx_erase_emem(EMEM_INFO);
    }

    DBG(&g_i2c_client->dev, "erase OK\n");

     /**/
        /*Program */
         /**/ if (eEmemType == EMEM_ALL) {
        drv_msg28xx_program_emem(EMEM_MAIN);
        drv_msg28xx_program_emem(EMEM_INFO);
    } else if (eEmemType == EMEM_MAIN) {
        drv_msg28xx_program_emem(EMEM_MAIN);
    } else if (eEmemType == EMEM_INFO) {
        drv_msg28xx_program_emem(EMEM_INFO);
    }

    DBG(&g_i2c_client->dev, "program OK\n");

    /* Calculate main block CRC & info block CRC by device driver itself */
    drv_msg28xx_convert_fw_data_two_dimen_to_one_dimen(szFw_data, g_one_dimen_fw_data);

    /* Read main block CRC & info block CRC from TP */
    if (eEmemType == EMEM_ALL) {
        nCrcMain =
            drv_msg22xx_calculate_crc(g_one_dimen_fw_data, 0,
                                    MSG28XX_FW_MAIN_BLOCK_SIZE * 1024 -
                                    MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);
        nCrcInfo =
            drv_msg22xx_calculate_crc(g_one_dimen_fw_data,
                                    MSG28XX_FW_MAIN_BLOCK_SIZE * 1024 +
                                    MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE,
                                    MSG28XX_FIRMWARE_INFO_BLOCK_SIZE * 1024 -
                                    MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE -
                                    MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);

        nCrcMainHardware = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);
        nCrcInfoHardware = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_INFO);

        nCrcMainEflash = drv_msg28xx_retrieve_firmware_crc_from_eflash(EMEM_MAIN);
        nCrcInfoEflash = drv_msg28xx_retrieve_firmware_crc_from_eflash(EMEM_INFO);
    } else if (eEmemType == EMEM_MAIN) {
        nCrcMain =
            drv_msg22xx_calculate_crc(g_one_dimen_fw_data, 0,
                                    MSG28XX_FW_MAIN_BLOCK_SIZE * 1024 -
                                    MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);
        nCrcMainHardware = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);
        nCrcMainEflash = drv_msg28xx_retrieve_firmware_crc_from_eflash(EMEM_MAIN);
    } else if (eEmemType == EMEM_INFO) {
        nCrcInfo =
            drv_msg22xx_calculate_crc(g_one_dimen_fw_data,
                                    MSG28XX_FW_MAIN_BLOCK_SIZE * 1024 +
                                    MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE,
                                    MSG28XX_FIRMWARE_INFO_BLOCK_SIZE * 1024 -
                                    MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE -
                                    MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);
        nCrcInfoHardware = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_INFO);
        nCrcInfoEflash = drv_msg28xx_retrieve_firmware_crc_from_eflash(EMEM_INFO);
    }

    DBG(&g_i2c_client->dev,
        "nCrcMain=0x%x, nCrcInfo=0x%x, nCrcMainHardware=0x%x, nCrcInfoHardware=0x%x, nCrcMainEflash=0x%x, nCrcInfoEflash=0x%x\n",
        nCrcMain, nCrcInfo, nCrcMainHardware, nCrcInfoHardware, nCrcMainEflash,
        nCrcInfoEflash);

    g_fw_data_count = 0;          /*Reset g_fw_data_count to 0 after update firmware */

    drv_touch_device_hw_reset();
    mdelay(300);

    g_is_update_firmware = 0x00;  /*Set flag to 0x00 for indicating update firmware is finished */

    if (eEmemType == EMEM_ALL) {
        if ((nCrcMainHardware != nCrcMain) || (nCrcInfoHardware != nCrcInfo) ||
            (nCrcMainEflash != nCrcMain) || (nCrcInfoEflash != nCrcInfo)) {
            DBG(&g_i2c_client->dev, "Update FAILED\n");

            return -1;
        }
    } else if (eEmemType == EMEM_MAIN) {
        if ((nCrcMainHardware != nCrcMain) || (nCrcMainEflash != nCrcMain)) {
            DBG(&g_i2c_client->dev, "Update FAILED\n");

            return -1;
        }
    } else if (eEmemType == EMEM_INFO) {
        if ((nCrcInfoHardware != nCrcInfo) || (nCrcInfoEflash != nCrcInfo)) {
            DBG(&g_i2c_client->dev, "Update FAILED\n");

            return -1;
        }
    }

    DBG(&g_i2c_client->dev, "Update SUCCESS\n");

    return 0;
}

s32 drv_update_firmware_cash(u8 szFw_data[][1024],enum emem_type_e eEmemType)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "g_chip_type = 0x%x\n", g_chip_type);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {  /*(0x7A) */
        drv_msg22xx_get_tp_vender_code(g_tp_vendor_code);

        if (g_tp_vendor_code[0] == 'C' && g_tp_vendor_code[1] == 'N' && g_tp_vendor_code[2] == 'T') { /*for specific TP vendor which store some important information in info block, only update firmware for main block, do not update firmware for info block. */
            return drv_msg22xx_update_firmware(szFw_data, EMEM_MAIN);
        } else {
            return drv_msg22xx_update_firmware(szFw_data, EMEM_ALL);
        }
    } else if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2117A || g_chip_type == CHIP_TYPE_ILI2118A) {   /*(0x85) || (0xBF) || (0x2117) || (0x2118) */
        DBG(&g_i2c_client->dev, "is_force_to_update_firmware_enabled = %d\n",
            is_force_to_update_firmware_enabled);

        if (is_force_to_update_firmware_enabled) {  /*Force to update firmware, do not check whether the vendor id of the update firmware bin file is equal to the vendor id on e-flash. */
            return drv_msg28xx_update_firmware(szFw_data, EMEM_MAIN);
        } else {
            u16 eSwId = 0x0000;
            u16 eVendorId = 0x0000;

            eVendorId = szFw_data[129][1005] << 8 | szFw_data[129][1004]; /*Retrieve major from info block */
            eSwId = drv_msg28xx_get_sw_id(EMEM_INFO);

            DBG(&g_i2c_client->dev, "eVendorId = 0x%x, eSwId = 0x%x\n",
                eVendorId, eSwId);

            /*Check if the vendor id of the update firmware bin file is equal to the vendor id on e-flash. YES => allow update, NO => not allow update */
            if (eSwId != eVendorId) {
                drv_touch_device_hw_reset();    /*Reset HW here to avoid touch may be not worked after get sw id. */

                DBG(&g_i2c_client->dev,
                    "The vendor id of the update firmware bin file is different from the vendor id on e-flash. Not allow to update.\n");
                g_fw_data_count = 0;  /*Reset g_fw_data_count to 0 */

                return -1;
            } else {
                return drv_msg28xx_update_firmware(szFw_data, EMEM_MAIN);
            }
        }
    } else {
        DBG(&g_i2c_client->dev, "Unsupported chip type = 0x%x\n", g_chip_type);
        g_fw_data_count = 0;      /*Reset g_fw_data_count to 0 */

        return -1;
    }
}

s32 drv_update_firmware_by_sd_card(const char *p_file_path)
{
    s32 n_ret_val = 0;
    struct file *pfile = NULL;
    struct inode *inode;
    s32 fsize = 0;
    mm_segment_t old_fs;
    loff_t pos;
    u16 eSwId = 0x0000;
    u16 eVendorId = 0x0000;
    u16 minor = 0x0000;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    pfile = filp_open(p_file_path, O_RDONLY, 0);
    if (IS_ERR(pfile)) {
        DBG(&g_i2c_client->dev, "Error occurred while opening file %s.\n",
            p_file_path);
        return -1;
    }
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 18, 0)
    inode = pfile->f_dentry->d_inode;
#else
    inode = pfile->f_path.dentry->d_inode;
#endif

    fsize = inode->i_size;

    DBG(&g_i2c_client->dev, "fsize = %d\n", fsize);

    if (fsize <= 0) {
        filp_close(pfile, NULL);
        return -1;
    }

    /*read firmware */
    memset(g_fw_data_buf, 0, MSG28XX_FIRMWARE_WHOLE_SIZE * 1024);

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    pos = 0;
    vfs_read(pfile, g_fw_data_buf, fsize, &pos);

    filp_close(pfile, NULL);
    set_fs(old_fs);

    drv_store_firmware_data(g_fw_data_buf, fsize);

    drv_disable_finger_touch_report();

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        eVendorId = g_fw_data[47][1013] << 8 | g_fw_data[47][1012];
		minor = g_fw_data[47][1015] << 8 | g_fw_data[47][1014];
        eSwId = drv_msg22xx_get_sw_id(EMEM_MAIN);
    } else if (g_chip_type == CHIP_TYPE_MSG28XX ||
               g_chip_type == CHIP_TYPE_MSG58XXA ||
               g_chip_type == CHIP_TYPE_ILI2117A ||
               g_chip_type == CHIP_TYPE_ILI2118A) {
        eVendorId = g_fw_data[129][1005] << 8 | g_fw_data[129][1004]; /*Retrieve major from info block */
        eSwId = drv_msg28xx_get_sw_id(EMEM_INFO);
    }

    DBG(&g_i2c_client->dev, "eVendorId = 0x%x, eSwId = 0x%x\n", eVendorId,
        eSwId);
    DBG(&g_i2c_client->dev, "is_force_to_update_firmware_enabled = %d\n",
        is_force_to_update_firmware_enabled);

    if ((eSwId == eVendorId) || (is_force_to_update_firmware_enabled)) {
		if ((!g_system_update) || ((g_system_update && (minor != g_firmware_minor))) || (is_force_to_update_firmware_enabled)) {
        if ((g_chip_type == CHIP_TYPE_MSG22XX && fsize == 49664 /* 48.5KB */ )) {
            n_ret_val = drv_update_firmware_cash(g_fw_data, EMEM_ALL);
			g_system_update = 0;
        } else
            if ((g_chip_type == CHIP_TYPE_MSG28XX ||
                 g_chip_type == CHIP_TYPE_MSG58XXA ||
                 g_chip_type == CHIP_TYPE_ILI2117A ||
                 g_chip_type == CHIP_TYPE_ILI2118A) &&
                fsize == 133120 /* 130KB */ ) {
            n_ret_val = drv_update_firmware_cash(g_fw_data, EMEM_MAIN);   /*For MSG28xx sine mode requirement, update main block only, do not update info block. */
			g_system_update = 0;
        } else {
            drv_touch_device_hw_reset();

            DBG(&g_i2c_client->dev,
                "The file size of the update firmware bin file is not supported, fsize = %d\n",
                fsize);
            n_ret_val = -1;
        }
		} else {
			DBG(&g_i2c_client->dev, "do not need update\n");
		}
    } else {
        drv_touch_device_hw_reset();

        DBG(&g_i2c_client->dev,
            "The vendor id of the update firmware bin file is different from the vendor id on e-flash. Not allow to update.\n");
        n_ret_val = -1;
    }

    g_fw_data_count = 0;          /*Reset g_fw_data_count to 0 after update firmware */

    drv_enable_finger_touch_report();

    return n_ret_val;
}

/*------------------------------------------------------------------------------//*/

static void drv_read_read_dq_mem_start(void)
{
    u8 nParCmdSelUseCfg = 0x7F;
    u8 nParCmdAdByteEn0 = 0x50;
    u8 nParCmdAdByteEn1 = 0x51;
    u8 nParCmdDaByteEn0 = 0x54;
    u8 nParCmdUSetSelB0 = 0x80;
    u8 nParCmdUSetSelB1 = 0x82;
    u8 nParCmdSetSelB2 = 0x85;
    u8 nParCmdIicUse = 0x35;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    iic_write_data(slave_i2c_id_db_bus, &nParCmdSelUseCfg, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn0, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn1, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdDaByteEn0, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdUSetSelB0, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdUSetSelB1, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdSetSelB2, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdIicUse, 1);
}

static void drv_read_read_dq_mem_end(void)
{
    u8 nParCmdNSelUseCfg = 0x7E;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    iic_write_data(slave_i2c_id_db_bus, &nParCmdNSelUseCfg, 1);
}

u32 drv_read_dq_mem_value(u16 n_addr)
{
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA ||
        g_chip_type == CHIP_TYPE_ILI2117A || g_chip_type == CHIP_TYPE_ILI2118A) {
        u8 tx_data[3] = { 0x10, (n_addr >> 8) & 0xFF, n_addr & 0xFF };
        u8 rx_data[4] = { 0 };

        DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

        DBG(&g_i2c_client->dev, "DQMem Addr = 0x%x\n", n_addr);

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        /*Stop mcu */
        reg_set_l_byte_value(0x0FE6, 0x01); /*bank:mheg5, addr:h0073 */
        mdelay(100);

        drv_read_read_dq_mem_start();

        iic_write_data(slave_i2c_id_db_bus, &tx_data[0], 3);
        iic_read_data(slave_i2c_id_db_bus, &rx_data[0], 4);

        drv_read_read_dq_mem_end();

        /*Start mcu */
        reg_set_l_byte_value(0x0FE6, 0x00); /*bank:mheg5, addr:h0073 */

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        return (rx_data[3] << 24 | rx_data[2] << 16 | rx_data[1] << 8 |
                rx_data[0]);
    } else {
        DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

        /*TODO : not support yet */

        return 0;
    }
}

void drv_write_dq_mem_value(u16 n_addr, u32 n_data)
{
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA ||
        g_chip_type == CHIP_TYPE_ILI2117A || g_chip_type == CHIP_TYPE_ILI2118A) {
        u8 szDbBusTx_data[7] = { 0 };

        DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

        DBG(&g_i2c_client->dev, "DQMem Addr = 0x%x\n", n_addr);

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        /*Stop mcu */
        reg_set_l_byte_value(0x0FE6, 0x01); /*bank:mheg5, addr:h0073 */
        mdelay(100);

        drv_read_read_dq_mem_start();

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = ((n_addr >> 8) & 0xff);
        szDbBusTx_data[2] = (n_addr & 0xff);
        szDbBusTx_data[3] = n_data & 0x000000FF;
        szDbBusTx_data[4] = ((n_data & 0x0000FF00) >> 8);
        szDbBusTx_data[5] = ((n_data & 0x00FF0000) >> 16);
        szDbBusTx_data[6] = ((n_data & 0xFF000000) >> 24);
        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 7);

        drv_read_read_dq_mem_end();

        /*Start mcu */
        reg_set_l_byte_value(0x0FE6, 0x00); /*bank:mheg5, addr:h0073 */
        mdelay(100);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();
    } else {
        DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

        /*TODO : not support yet */
    }
}

/*------------------------------------------------------------------------------//*/

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
void drv_check_firmware_update_by_sw_id(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
        drv_msg22xx_check_frimware_update_by_sw_id();
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */
    } else if (g_chip_type == CHIP_TYPE_MSG28XX ||
               g_chip_type == CHIP_TYPE_MSG58XXA ||
               g_chip_type == CHIP_TYPE_ILI2117A ||
               g_chip_type == CHIP_TYPE_ILI2118A) {
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
        drv_msg28xx_check_frimware_update_by_sw_id();
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
#ifdef CONFIG_ENABLE_CHIP_TYPE_ILI21XX
        drv_ilitek_check_frimware_update_by_sw_id();
#endif /*CONFIG_ENABLE_CHIP_TYPE_ILI21XX */
    } else {
        DBG(&g_i2c_client->dev,
            "This chip type (0x%x) does not support update firmware by sw id\n",
            g_chip_type);
    }
}

/*-------------------------Start of SW ID for ILI21XX----------------------------//*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_ILI21XX
static void drv_ilitek_check_frimware_update_by_sw_id(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_disable_finger_touch_report();

    g_update_firmware_by_sw_id_workQueue =
        create_singlethread_workqueue("update_firmware_by_sw_id");
    INIT_WORK(&g_update_firmware_by_sw_id_work, drv_update_firmware_by_sw_id_do_work);

    g_update_retry_count = 1;
    g_is_update_firmware = 0x01;  /*Set flag to 0x01 for indicating update firmware is processing */

    queue_work(g_update_firmware_by_sw_id_workQueue, &g_update_firmware_by_sw_id_work);

    /*Note : Since drv_touch_device_hw_reset() and drv_enable_finger_touch_report() shall be called at drv_update_firmware_by_sw_id_do_work() later, can not call the two functions here. */
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_ILI21XX */

/*-------------------------End of SW ID for ILI21XX----------------------------//*/

/*-------------------------Start of SW ID for MSG22XX----------------------------//*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
static void drv_msg22xx_erase_emem(enum emem_type_e eEmemType)
{
    u32 i = 0;
    u32 nTimeOut = 0;
    u16 nReg_data = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    DBG(&g_i2c_client->dev, "Erase start\n");

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    /*Exit flash low power mode */
    reg_set_l_byte_value(0x1619, BIT1);

    /*Change PIU clock to 48MHz */
    reg_set_l_byte_value(0x1E23, BIT6);

    /*Change mcu clock deglitch mux source */
    reg_set_l_byte_value(0x1E54, BIT0);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    /*Set PROGRAM password */
    reg_set_l_byte_value(0x161A, 0xBA);
    reg_set_l_byte_value(0x161B, 0xAB);

    if (eEmemType == EMEM_ALL) {    /*48KB + 512Byte */
        DBG(&g_i2c_client->dev, "Erase all block\n");

        /*Clear pce */
        reg_set_l_byte_value(0x1618, 0x80);
        mdelay(100);

        /*Chip erase */
        reg_set_16bit_value(0x160E, BIT3);

        DBG(&g_i2c_client->dev, "Wait erase done flag\n");

        while (1) {             /*Wait erase done flag */
            nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
            nReg_data = nReg_data & BIT1;

            DBG(&g_i2c_client->dev, "Wait erase done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT1) {
                break;
            }

            mdelay(50);

            if ((nTimeOut++) > 30) {
                DBG(&g_i2c_client->dev, "Erase all block failed. Timeout.\n");

                goto EraseEnd;
            }
        }
    } else if (eEmemType == EMEM_MAIN) {    /*48KB (32+8+8) */
        DBG(&g_i2c_client->dev, "Erase main block\n");

        for (i = 0; i < 3; i++) {
            /*Clear pce */
            reg_set_l_byte_value(0x1618, 0x80);
            mdelay(10);

            if (i == 0) {
                reg_set_16bit_value(0x1600, 0x0000);
            } else if (i == 1) {
                reg_set_16bit_value(0x1600, 0x8000);
            } else if (i == 2) {
                reg_set_16bit_value(0x1600, 0xA000);
            }

            /*Sector erase */
            reg_set_16bit_value(0x160E, (reg_get16_bit_value(0x160E) | BIT2));

            DBG(&g_i2c_client->dev, "Wait erase done flag\n");

            nReg_data = 0;
            nTimeOut = 0;

            while (1) {         /*Wait erase done flag */
                nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
                nReg_data = nReg_data & BIT1;

                DBG(&g_i2c_client->dev,
                    "Wait erase done flag nReg_data = 0x%x\n", nReg_data);

                if (nReg_data == BIT1) {
                    break;
                }
                mdelay(50);

                if ((nTimeOut++) > 30) {
                    DBG(&g_i2c_client->dev,
                        "Erase main block failed. Timeout.\n");

                    goto EraseEnd;
                }
            }
        }
    } else if (eEmemType == EMEM_INFO) {    /*512Byte */
        DBG(&g_i2c_client->dev, "Erase info block\n");

        /*Clear pce */
        reg_set_l_byte_value(0x1618, 0x80);
        mdelay(10);

        reg_set_16bit_value(0x1600, 0xC000);

        /*Sector erase */
        reg_set_16bit_value(0x160E, (reg_get16_bit_value(0x160E) | BIT2));

        DBG(&g_i2c_client->dev, "Wait erase done flag\n");

        while (1) {             /*Wait erase done flag */
            nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
            nReg_data = nReg_data & BIT1;

            DBG(&g_i2c_client->dev, "Wait erase done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT1) {
                break;
            }
            mdelay(50);

            if ((nTimeOut++) > 30) {
                DBG(&g_i2c_client->dev, "Erase info block failed. Timeout.\n");

                goto EraseEnd;
            }
        }
    }

EraseEnd:

    DBG(&g_i2c_client->dev, "Erase end\n");

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();
}

static void drv_msg22xx_program_emem(enum emem_type_e eEmemType)
{
    u32 i, j;
    u32 nRemainSize = 0, nBlockSize = 0, n_size = 0, index = 0;
    u32 nTimeOut = 0;
    u16 nReg_data = 0;
    s32 rc = 0;
#if defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    u8 szDbBusTx_data[128] = { 0 };
#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    u32 nSizePerWrite = 1;
#else
    u32 nSizePerWrite = 125;
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */
#elif defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
    u8 szDbBusTx_data[1024] = { 0 };
#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    u32 nSizePerWrite = 1;
#else
    u32 nSizePerWrite = 1021;
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */
#endif

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
#if defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
        nSizePerWrite = 41;     /*123/3=41 */
#elif defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
        nSizePerWrite = 340;    /*1020/3=340 */
#endif
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
#if defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
        nSizePerWrite = 31;     /*124/4=31 */
#elif defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
        nSizePerWrite = 255;    /*1020/4=255 */
#endif
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
    }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Hold reset pin before program */
    reg_set_l_byte_value(0x1E06, 0x00);

    DBG(&g_i2c_client->dev, "Program start\n");

    reg_set_16bit_value(0x161A, 0xABBA);
    reg_set_16bit_value(0x1618, (reg_get16_bit_value(0x1618) | 0x80));

    if (eEmemType == EMEM_MAIN) {
        DBG(&g_i2c_client->dev, "Program main block\n");

        reg_set_16bit_value(0x1600, 0x0000);   /*Set start address of main block */
        nRemainSize = MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE * 1024;  /*48KB */
        index = 0;
    } else if (eEmemType == EMEM_INFO) {
        DBG(&g_i2c_client->dev, "Program info block\n");

        reg_set_16bit_value(0x1600, 0xC000);   /*Set start address of info block */
        nRemainSize = MSG22XX_FIRMWARE_INFO_BLOCK_SIZE; /*512Byte */
        index = MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE * 1024;
    } else {
        DBG(&g_i2c_client->dev,
            "eEmemType = %d is not supported for program e-memory.\n",
            eEmemType);
        return;
    }

    reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));    /*Enable burst mode */

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
        reg_set_16bit_value_on(0x160A, BIT0);   /*Set Bit0 = 1 for enable I2C 400KHz burst write mode, let e-flash discard the last 2 dummy byte. */
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
        reg_set_16bit_value_on(0x160A, BIT0);
        reg_set_16bit_value_on(0x160A, BIT1);   /*Set Bit0 = 1, Bit1 = 1 for enable I2C 400KHz burst write mode, let e-flash discard the last 3 dummy byte. */
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
    }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    /*Program start */
    szDbBusTx_data[0] = 0x10;
    szDbBusTx_data[1] = 0x16;
    szDbBusTx_data[2] = 0x02;

    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3);

    szDbBusTx_data[0] = 0x20;

    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

    i = 0;

    while (nRemainSize > 0) {
        if (nRemainSize > nSizePerWrite) {
            nBlockSize = nSizePerWrite;
        } else {
            nBlockSize = nRemainSize;
        }

        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x16;
        szDbBusTx_data[2] = 0x02;

        n_size = 3;

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
            u32 k = 0;

            for (j = 0; j < nBlockSize; j++) {
                for (k = 0; k < 3; k++) {
                    szDbBusTx_data[3 + (j * 3) + k] =
                        g_one_dimen_fw_data[index + (i * nSizePerWrite) + j];
                }
                n_size = n_size + 3;
            }
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
            u32 k = 0;

            for (j = 0; j < nBlockSize; j++) {
                for (k = 0; k < 4; k++) {
                    szDbBusTx_data[3 + (j * 4) + k] =
                        g_one_dimen_fw_data[index + (i * nSizePerWrite) + j];
                }
                n_size = n_size + 4;
            }
#else /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_C */
            for (j = 0; j < nBlockSize; j++) {
                szDbBusTx_data[3 + j] =
                    g_one_dimen_fw_data[index + (i * nSizePerWrite) + j];
                n_size++;
            }
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
        } else {
            for (j = 0; j < nBlockSize; j++) {
                szDbBusTx_data[3 + j] =
                    g_one_dimen_fw_data[index + (i * nSizePerWrite) + j];
                n_size++;
            }
        }
#else
        for (j = 0; j < nBlockSize; j++) {
            szDbBusTx_data[3 + j] =
                g_one_dimen_fw_data[index + (i * nSizePerWrite) + j];
            n_size++;
        }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        i++;

        rc = iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], n_size);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "Write firmware data failed, rc = %d. Exit program procedure.\n",
                rc);

            goto ProgramEnd;
        }

        nRemainSize = nRemainSize - nBlockSize;
    }

    /*Program end */
    szDbBusTx_data[0] = 0x21;

    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
        reg_set_16Bit_value_off(0x160A, BIT0);  /*Set Bit0 = 0 for disable I2C 400KHz burst write mode, let e-flash discard the last 2 dummy byte. */
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
        reg_set_16Bit_value_off(0x160A, BIT0);
        reg_set_16Bit_value_off(0x160A, BIT1);  /*Set Bit0 = 0, Bit1 = 0 for disable I2C 400KHz burst write mode, let e-flash discard the last 3 dummy byte. */
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
    }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));   /*Clear burst mode */

    DBG(&g_i2c_client->dev, "Wait write done flag\n");

    while (1) {                 /*Wait write done flag */
        /*Polling 0x1610 is 0x0002 */
        nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
        nReg_data = nReg_data & BIT1;

        DBG(&g_i2c_client->dev, "Wait write done flag nReg_data = 0x%x\n",
            nReg_data);

        if (nReg_data == BIT1) {
            break;
        }
        mdelay(10);

        if ((nTimeOut++) > 30) {
            DBG(&g_i2c_client->dev, "Write failed. Timeout.\n");

            goto ProgramEnd;
        }
    }

ProgramEnd:

    DBG(&g_i2c_client->dev, "Program end\n");

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();
}



static u32 drv_msg22xx_retrieve_firmware_crc_from_bin_file(u8 szTmpBuf[],
                                                     enum emem_type_e eEmemType)
{
    u32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() eEmemType = %d ***\n", __func__,
        eEmemType);

    if (szTmpBuf != NULL) {
        if (eEmemType == EMEM_MAIN) {   /*Read main block CRC(48KB-4) from bin file */
            n_ret_val = szTmpBuf[0xBFFF] << 24;
            n_ret_val |= szTmpBuf[0xBFFE] << 16;
            n_ret_val |= szTmpBuf[0xBFFD] << 8;
            n_ret_val |= szTmpBuf[0xBFFC];
        } else if (eEmemType == EMEM_INFO) {    /*Read info block CRC(512Byte-4) from bin file */
            n_ret_val = szTmpBuf[0xC1FF] << 24;
            n_ret_val |= szTmpBuf[0xC1FE] << 16;
            n_ret_val |= szTmpBuf[0xC1FD] << 8;
            n_ret_val |= szTmpBuf[0xC1FC];
        }
    }

    return n_ret_val;
}

static s32 drv_msg22xx_update_firmware_by_sw_id(void)
{
    s32 n_ret_val = -1;
    u32 nCrcInfoA = 0, nCrcInfoB = 0, nCrcMainA = 0, nCrcMainB = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev,
        "g_is_update_info_block_first = %d, g_is_update_firmware = 0x%x\n",
        g_is_update_info_block_first, g_is_update_firmware);

    drv_msg22xx_convert_fw_data_two_dimen_to_one_dimen(g_fw_data, g_one_dimen_fw_data);

    if (g_is_update_info_block_first == 1) {
        if ((g_is_update_firmware & 0x10) == 0x10) {
            drv_msg22xx_erase_emem(EMEM_INFO);
            drv_msg22xx_program_emem(EMEM_INFO);

            nCrcInfoA =
                drv_msg22xx_retrieve_firmware_crc_from_bin_file(g_one_dimen_fw_data,
                                                          EMEM_INFO);
            nCrcInfoB = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_INFO);

            DBG(&g_i2c_client->dev, "nCrcInfoA = 0x%x, nCrcInfoB = 0x%x\n",
                nCrcInfoA, nCrcInfoB);

            if (nCrcInfoA == nCrcInfoB) {
                drv_msg22xx_erase_emem(EMEM_MAIN);
                drv_msg22xx_program_emem(EMEM_MAIN);

                nCrcMainA =
                    drv_msg22xx_retrieve_firmware_crc_from_bin_file(g_one_dimen_fw_data,
                                                              EMEM_MAIN);
                nCrcMainB = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_MAIN);

                DBG(&g_i2c_client->dev, "nCrcMainA = 0x%x, nCrcMainB = 0x%x\n",
                    nCrcMainA, nCrcMainB);

                if (nCrcMainA == nCrcMainB) {
                    g_is_update_firmware = 0x00;
                    n_ret_val = 0;
                } else {
                    g_is_update_firmware = 0x01;
                }
            } else {
                g_is_update_firmware = 0x11;
            }
        } else if ((g_is_update_firmware & 0x01) == 0x01) {
            drv_msg22xx_erase_emem(EMEM_MAIN);
            drv_msg22xx_program_emem(EMEM_MAIN);

            nCrcMainA =
                drv_msg22xx_retrieve_firmware_crc_from_bin_file(g_one_dimen_fw_data,
                                                          EMEM_MAIN);
            nCrcMainB = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_MAIN);

            DBG(&g_i2c_client->dev, "nCrcMainA=0x%x, nCrcMainB=0x%x\n",
                nCrcMainA, nCrcMainB);

            if (nCrcMainA == nCrcMainB) {
                g_is_update_firmware = 0x00;
                n_ret_val = 0;
            } else {
                g_is_update_firmware = 0x01;
            }
        }
    } else {
/*g_is_update_info_block_first == 0*/

        if ((g_is_update_firmware & 0x10) == 0x10) {
            drv_msg22xx_erase_emem(EMEM_MAIN);
            drv_msg22xx_program_emem(EMEM_MAIN);

            nCrcMainA =
                drv_msg22xx_retrieve_firmware_crc_from_bin_file(g_one_dimen_fw_data,
                                                          EMEM_MAIN);
            nCrcMainB = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_MAIN);

            DBG(&g_i2c_client->dev, "nCrcMainA=0x%x, nCrcMainB=0x%x\n",
                nCrcMainA, nCrcMainB);

            if (nCrcMainA == nCrcMainB) {
                drv_msg22xx_erase_emem(EMEM_INFO);
                drv_msg22xx_program_emem(EMEM_INFO);

                nCrcInfoA =
                    drv_msg22xx_retrieve_firmware_crc_from_bin_file(g_one_dimen_fw_data,
                                                              EMEM_INFO);
                nCrcInfoB = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_INFO);

                DBG(&g_i2c_client->dev, "nCrcInfoA=0x%x, nCrcInfoB=0x%x\n",
                    nCrcInfoA, nCrcInfoB);

                if (nCrcInfoA == nCrcInfoB) {
                    g_is_update_firmware = 0x00;
                    n_ret_val = 0;
                } else {
                    g_is_update_firmware = 0x01;
                }
            } else {
                g_is_update_firmware = 0x11;
            }
        } else if ((g_is_update_firmware & 0x01) == 0x01) {
            drv_msg22xx_erase_emem(EMEM_INFO);
            drv_msg22xx_program_emem(EMEM_INFO);

            nCrcInfoA =
                drv_msg22xx_retrieve_firmware_crc_from_bin_file(g_one_dimen_fw_data,
                                                          EMEM_INFO);
            nCrcInfoB = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_INFO);

            DBG(&g_i2c_client->dev, "nCrcInfoA=0x%x, nCrcInfoB=0x%x\n",
                nCrcInfoA, nCrcInfoB);

            if (nCrcInfoA == nCrcInfoB) {
                g_is_update_firmware = 0x00;
                n_ret_val = 0;
            } else {
                g_is_update_firmware = 0x01;
            }
        }
    }

    return n_ret_val;
}

static void drv_msg22xx_check_frimware_update_by_sw_id(void)
{
    u32 nCrcMainA, nCrcInfoA, nCrcMainB, nCrcInfoB;
    u32 i, j;
    u32 n_swIdListNum = 0;
    u16 nUpdateBinMajor = 0, nUpdateBinMinor = 0;
    u16 nMajor = 0, nMinor = 0;
    u8 nIsSwIdValid = 0;
    u8 nFinalIndex = 0;
    u8 *pVersion = NULL;
    u16 eSwId = MSG22XX_SW_ID_UNDEFINED;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "*** g_msg22xx_chip_revision = 0x%x ***\n",
        g_msg22xx_chip_revision);

    drv_disable_finger_touch_report();

    n_swIdListNum = sizeof(g_sw_id_data) / sizeof(sw_id_data_t);
    DBG(&g_i2c_client->dev,
        "*** sizeof(g_sw_id_data)=%d, sizeof(sw_id_data_t)=%d, n_swIdListNum=%d ***\n",
        (int)sizeof(g_sw_id_data), (int)sizeof(sw_id_data_t), (int)n_swIdListNum);

    nCrcMainA = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_MAIN);
    nCrcMainB = drv_msg22xx_retrieve_firmware_crc_from_eflash(EMEM_MAIN);

    nCrcInfoA = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_INFO);
    nCrcInfoB = drv_msg22xx_retrieve_firmware_crc_from_eflash(EMEM_INFO);

    g_update_firmware_by_sw_id_workQueue =
        create_singlethread_workqueue("update_firmware_by_sw_id");
    INIT_WORK(&g_update_firmware_by_sw_id_work, drv_update_firmware_by_sw_id_do_work);

    DBG(&g_i2c_client->dev,
        "nCrcMainA=0x%x, nCrcInfoA=0x%x, nCrcMainB=0x%x, nCrcInfoB=0x%x\n",
        nCrcMainA, nCrcInfoA, nCrcMainB, nCrcInfoB);

    if (nCrcMainA == nCrcMainB && nCrcInfoA == nCrcInfoB) { /*Case 1. Main Block:OK, Info Block:OK */
        eSwId = drv_msg22xx_get_sw_id(EMEM_MAIN);

        nIsSwIdValid = 0;

        for (j = 0; j < n_swIdListNum; j++) {
            DBG(&g_i2c_client->dev, "g_sw_id_data[%d].n_swId = 0x%x\n", j, g_sw_id_data[j].n_swId);   /*TODO : add for debug */

            if (eSwId == g_sw_id_data[j].n_swId) {
                nUpdateBinMajor =
                    (*(g_sw_id_data[j].p_update_bin + 0xBFF5)) << 8 |
                    (*(g_sw_id_data[j].p_update_bin + 0xBFF4));
                nUpdateBinMinor =
                    (*(g_sw_id_data[j].p_update_bin + 0xBFF7)) << 8 |
                    (*(g_sw_id_data[j].p_update_bin + 0xBFF6));
                nIsSwIdValid = 1;
                nFinalIndex = j;

                break;
            }
        }

        if (0 == nIsSwIdValid) {
            DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                eSwId);

            eSwId = MSG22XX_SW_ID_UNDEFINED;
            nUpdateBinMajor = 0;
            nUpdateBinMinor = 0;
        }

        drv_get_customer_firmware_version(&nMajor, &nMinor, &pVersion);

        DBG(&g_i2c_client->dev,
            "eSwId=0x%x, nMajor=%d, nMinor=%d, nUpdateBinMajor=%d, nUpdateBinMinor=%d\n",
            eSwId, nMajor, nMinor, nUpdateBinMajor, nUpdateBinMinor);

        if (nUpdateBinMinor > nMinor) {
            if (1 == nIsSwIdValid) {
                DBG(&g_i2c_client->dev, "nFinalIndex = %d\n", nFinalIndex); /*TODO : add for debug */

                for (i = 0; i < (MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE + 1); i++) {
                    if (i < MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE) { /*i < 48 */
                        drv_store_firmware_data((g_sw_id_data
                                               [nFinalIndex].p_update_bin +
                                               i * 1024), 1024);
                    } else {    /*i == 48 */

                        drv_store_firmware_data((g_sw_id_data
                                               [nFinalIndex].p_update_bin +
                                               i * 1024), 512);
                    }
                }
            } else {
                DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                    eSwId);

                eSwId = MSG22XX_SW_ID_UNDEFINED;
            }

            if (eSwId < MSG22XX_SW_ID_UNDEFINED && eSwId != 0x0000 &&
                eSwId != 0xFFFF) {
                g_fw_data_count = 0;  /*Reset g_fw_data_count to 0 after copying update firmware data to temp buffer */

                g_update_retry_count = UPDATE_FIRMWARE_RETRY_COUNT;
                g_is_update_info_block_first = 1;   /*Set 1 for indicating main block is complete */
                g_is_update_firmware = 0x11;
                queue_work(g_update_firmware_by_sw_id_workQueue,
                           &g_update_firmware_by_sw_id_work);
                return;
            } else {
                DBG(&g_i2c_client->dev, "The sw id is invalid.\n");
                DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
            }
        } else {
            DBG(&g_i2c_client->dev,
                "The update bin version is older than or equal to the current firmware version on e-flash.\n");
            DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
        }
    } else if (nCrcMainA == nCrcMainB && nCrcInfoA != nCrcInfoB) {  /*Case 2. Main Block:OK, Info Block:FAIL */
        eSwId = drv_msg22xx_get_sw_id(EMEM_MAIN);

        DBG(&g_i2c_client->dev, "eSwId=0x%x\n", eSwId);

        nIsSwIdValid = 0;

        for (j = 0; j < n_swIdListNum; j++) {
            DBG(&g_i2c_client->dev, "g_sw_id_data[%d].n_swId = 0x%x\n", j, g_sw_id_data[j].n_swId);   /*TODO : add for debug */

            if (eSwId == g_sw_id_data[j].n_swId) {
                for (i = 0; i < (MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE + 1); i++) {
                    if (i < MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE) { /*i < 48 */
                        drv_store_firmware_data((g_sw_id_data[j].p_update_bin +
                                               i * 1024), 1024);
                    } else {    /*i == 48 */

                        drv_store_firmware_data((g_sw_id_data[j].p_update_bin +
                                               i * 1024), 512);
                    }
                }
                nIsSwIdValid = 1;

                break;
            }
        }

        if (0 == nIsSwIdValid) {
            DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                eSwId);

            eSwId = MSG22XX_SW_ID_UNDEFINED;
        }

        if (eSwId < MSG22XX_SW_ID_UNDEFINED && eSwId != 0x0000 &&
            eSwId != 0xFFFF) {
            g_fw_data_count = 0;  /*Reset g_fw_data_count to 0 after copying update firmware data to temp buffer */

            g_update_retry_count = UPDATE_FIRMWARE_RETRY_COUNT;
            g_is_update_info_block_first = 1;   /*Set 1 for indicating main block is complete */
            g_is_update_firmware = 0x11;
            queue_work(g_update_firmware_by_sw_id_workQueue,
                       &g_update_firmware_by_sw_id_work);
            return;
        } else {
            DBG(&g_i2c_client->dev, "The sw id is invalid.\n");
            DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
        }
    } else if (nCrcMainA != nCrcMainB && nCrcInfoA == nCrcInfoB) {  /*Case 3. Main Block:FAIL, Info Block:OK */
        eSwId = drv_msg22xx_get_sw_id(EMEM_INFO);

        DBG(&g_i2c_client->dev, "eSwId=0x%x\n", eSwId);

        nIsSwIdValid = 0;

        for (j = 0; j < n_swIdListNum; j++) {
            DBG(&g_i2c_client->dev, "g_sw_id_data[%d].n_swId = 0x%x\n", j, g_sw_id_data[j].n_swId);   /*TODO : add for debug */

            if (eSwId == g_sw_id_data[j].n_swId) {
                for (i = 0; i < (MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE + 1); i++) {
                    if (i < MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE) { /*i < 48 */
                        drv_store_firmware_data((g_sw_id_data[j].p_update_bin +
                                               i * 1024), 1024);
                    } else {    /*i == 48 */

                        drv_store_firmware_data((g_sw_id_data[j].p_update_bin +
                                               i * 1024), 512);
                    }
                }
                nIsSwIdValid = 1;

                break;
            }
        }

        if (0 == nIsSwIdValid) {
            DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                eSwId);

            eSwId = MSG22XX_SW_ID_UNDEFINED;
        }

        if (eSwId < MSG22XX_SW_ID_UNDEFINED && eSwId != 0x0000 &&
            eSwId != 0xFFFF) {
            g_fw_data_count = 0;  /*Reset g_fw_data_count to 0 after copying update firmware data to temp buffer */

            g_update_retry_count = UPDATE_FIRMWARE_RETRY_COUNT;
            g_is_update_info_block_first = 0;   /*Set 0 for indicating main block is broken */
            g_is_update_firmware = 0x11;
            queue_work(g_update_firmware_by_sw_id_workQueue,
                       &g_update_firmware_by_sw_id_work);
            return;
        } else {
            DBG(&g_i2c_client->dev, "The sw id is invalid.\n");
            DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
        }
    } else {                    /*Case 4. Main Block:FAIL, Info Block:FAIL */

        DBG(&g_i2c_client->dev, "Main block and Info block are broken.\n");
        DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
    }

    drv_touch_device_hw_reset();

    drv_enable_finger_touch_report();
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

/*-------------------------End of SW ID for MSG22XX----------------------------//*/

/*-------------------------Start of SW ID for MSG28XX----------------------------//*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
/*
static s32 drv_msg28xx_update_firmwareBySwId(void)
{
    s32 n_ret_val = -1;
    u32 nCrcInfoA = 0, nCrcInfoB = 0, nCrcMainA = 0, nCrcMainB = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "g_is_update_info_block_first = %d, g_is_update_firmware = 0x%x\n", g_is_update_info_block_first, g_is_update_firmware);

    if (g_is_update_info_block_first == 1)
    {
        if ((g_is_update_firmware & 0x10) == 0x10)
        {
            drv_msg28xx_erase_emem(EMEM_INFO);
            drv_msg28xx_program_emem(EMEM_INFO);

            nCrcInfoA = drv_msg28xx_retrieve_firmware_crc_from_bin_file(g_fw_data, EMEM_INFO);
            nCrcInfoB = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_INFO);

            DBG(&g_i2c_client->dev, "nCrcInfoA = 0x%x, nCrcInfoB = 0x%x\n", nCrcInfoA, nCrcInfoB);

            if (nCrcInfoA == nCrcInfoB)
            {
                drv_msg28xx_erase_emem(EMEM_MAIN);
                drv_msg28xx_program_emem(EMEM_MAIN);

                nCrcMainA = drv_msg28xx_retrieve_firmware_crc_from_bin_file(g_fw_data, EMEM_MAIN);
                nCrcMainB = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);

                DBG(&g_i2c_client->dev, "nCrcMainA = 0x%x, nCrcMainB = 0x%x\n", nCrcMainA, nCrcMainB);

                if (nCrcMainA == nCrcMainB)
                {
                    g_is_update_firmware = 0x00;
                    n_ret_val = 0;
                }
                else
                {
                    g_is_update_firmware = 0x01;
                }
            }
            else
            {
                g_is_update_firmware = 0x11;
            }
        }
        else if ((g_is_update_firmware & 0x01) == 0x01)
        {
            drv_msg28xx_erase_emem(EMEM_MAIN);
            drv_msg28xx_program_emem(EMEM_MAIN);

            nCrcMainA = drv_msg28xx_retrieve_firmware_crc_from_bin_file(g_fw_data, EMEM_MAIN);
            nCrcMainB = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);

            DBG(&g_i2c_client->dev, "nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA, nCrcMainB);

            if (nCrcMainA == nCrcMainB)
            {
                g_is_update_firmware = 0x00;
                n_ret_val = 0;
            }
            else
            {
                g_is_update_firmware = 0x01;
            }
        }
    }
    else g_is_update_info_block_first == 0
{
    if ((g_is_update_firmware & 0x10) == 0x10) {
        drv_msg28xx_erase_emem(EMEM_MAIN);
        drv_msg28xx_program_emem(EMEM_MAIN);

        nCrcMainA =
            drv_msg28xx_retrieve_firmware_crc_from_bin_file(g_fw_data, EMEM_MAIN);
        nCrcMainB = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);

        DBG(&g_i2c_client->dev, "nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA,
            nCrcMainB);

        if (nCrcMainA == nCrcMainB) {
            drv_msg28xx_erase_emem(EMEM_INFO);
            drv_msg28xx_program_emem(EMEM_INFO);

            nCrcInfoA =
                drv_msg28xx_retrieve_firmware_crc_from_bin_file(g_fw_data, EMEM_INFO);
            nCrcInfoB = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_INFO);

            DBG(&g_i2c_client->dev, "nCrcInfoA=0x%x, nCrcInfoB=0x%x\n",
                nCrcInfoA, nCrcInfoB);

            if (nCrcInfoA == nCrcInfoB) {
                g_is_update_firmware = 0x00;
                n_ret_val = 0;
            } else {
                g_is_update_firmware = 0x01;
            }
        } else {
            g_is_update_firmware = 0x11;
        }
    } else if ((g_is_update_firmware & 0x01) == 0x01) {
        drv_msg28xx_erase_emem(EMEM_INFO);
        drv_msg28xx_program_emem(EMEM_INFO);

        nCrcInfoA =
            drv_msg28xx_retrieve_firmware_crc_from_bin_file(g_fw_data, EMEM_INFO);
        nCrcInfoB = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_INFO);

        DBG(&g_i2c_client->dev, "nCrcInfoA=0x%x, nCrcInfoB=0x%x\n", nCrcInfoA,
            nCrcInfoB);

        if (nCrcInfoA == nCrcInfoB) {
            g_is_update_firmware = 0x00;
            n_ret_val = 0;
        } else {
            g_is_update_firmware = 0x01;
        }
    }
}

return n_ret_val;
}

*/
static void drv_msg28xx_check_frimware_update_by_sw_id(void)
{
    u32 nCrcMainA, nCrcMainB;
    u32 i, j;
    u32 n_swIdListNum = 0;
    u16 nUpdateBinMajor = 0, nUpdateBinMinor = 0;
    u16 nMajor = 0, nMinor = 0;
    u8 nIsSwIdValid = 0;
    u8 nFinalIndex = 0;
    u8 *pVersion = NULL;
    msg28xx_sw_id_e eMain_swId = MSG28XX_SW_ID_UNDEFINED, eSwId =
        MSG28XX_SW_ID_UNDEFINED;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_disable_finger_touch_report();

    n_swIdListNum = sizeof(g_sw_id_data) / sizeof(sw_id_data_t);
    DBG(&g_i2c_client->dev,
        "*** sizeof(g_sw_id_data)=%d, sizeof(sw_id_data_t)=%d, n_swIdListNum=%d ***\n",
        (int)sizeof(g_sw_id_data), (int)sizeof(sw_id_data_t), (int)n_swIdListNum);

    nCrcMainA = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);
    nCrcMainB = drv_msg28xx_retrieve_firmware_crc_from_eflash(EMEM_MAIN);

    DBG(&g_i2c_client->dev, "nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA,
        nCrcMainB);

#ifdef CONFIG_ENABLE_CODE_FOR_DEBUG /*TODO : add for debug */
    if (nCrcMainA != nCrcMainB) {
        for (i = 0; i < 5; i++) {
            nCrcMainA = drv_msg28xx_get_firmware_crc_by_hardware(EMEM_MAIN);
            nCrcMainB = drv_msg28xx_retrieve_firmware_crc_from_eflash(EMEM_MAIN);

            DBG(&g_i2c_client->dev,
                "*** Retry[%d] : nCrcMainA=0x%x, nCrcMainB=0x%x ***\n", i,
                nCrcMainA, nCrcMainB);

            if (nCrcMainA == nCrcMainB) {
                break;
            }

            mdelay(50);
        }
    }
#endif /*CONFIG_ENABLE_CODE_FOR_DEBUG */

    g_update_firmware_by_sw_id_workQueue =
        create_singlethread_workqueue("update_firmware_by_sw_id");
    INIT_WORK(&g_update_firmware_by_sw_id_work, drv_update_firmware_by_sw_id_do_work);

    if (nCrcMainA == nCrcMainB) {
        eMain_swId = drv_msg28xx_get_sw_id(EMEM_MAIN);

        DBG(&g_i2c_client->dev, "eMain_swId=0x%x\n", eMain_swId);

        eSwId = eMain_swId;

        nIsSwIdValid = 0;

        for (j = 0; j < n_swIdListNum; j++) {
            DBG(&g_i2c_client->dev, "g_sw_id_data[%d].n_swId = 0x%x\n", j, g_sw_id_data[j].n_swId);   /*TODO : add for debug */

            if (eSwId == g_sw_id_data[j].n_swId) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY  /*By two dimensional array */
                nUpdateBinMajor =
                    ((*(g_sw_id_data[j].p_update_bin + 127))[1013]) << 8 |
                    ((*(g_sw_id_data[j].p_update_bin + 127))[1012]);
                nUpdateBinMinor =
                    ((*(g_sw_id_data[j].p_update_bin + 127))[1015]) << 8 |
                    ((*(g_sw_id_data[j].p_update_bin + 127))[1014]);
#else /*By one dimensional array */
                nUpdateBinMajor =
                    (*(g_sw_id_data[j].p_update_bin + 0x1FFF5)) << 8 |
                    (*(g_sw_id_data[j].p_update_bin + 0x1FFF4));
                nUpdateBinMinor =
                    (*(g_sw_id_data[j].p_update_bin + 0x1FFF7)) << 8 |
                    (*(g_sw_id_data[j].p_update_bin + 0x1FFF6));
#endif /*CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY */
                nIsSwIdValid = 1;
                nFinalIndex = j;

                break;
            }
        }

        if (0 == nIsSwIdValid) {
            DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                eSwId);

            eSwId = MSG28XX_SW_ID_UNDEFINED;
            nUpdateBinMajor = 0;
            nUpdateBinMinor = 0;
        }

        drv_get_customer_firmware_version_by_db_bus(EMEM_MAIN, &nMajor, &nMinor,
                                             &pVersion);
        if ((nUpdateBinMinor & 0xFF) > (nMinor & 0xFF)) {
            if (g_fw_version_flag) {
                DBG(&g_i2c_client->dev,
                    "eSwId=0x%x, nMajor=%u, nMinor=%u.%u, nUpdateBinMajor=%u, nUpdateBinMinor=%u.%u\n",
                    eSwId, nMajor, nMinor & 0xFF, (nMinor & 0xFF00) >> 8,
                    nUpdateBinMajor, nUpdateBinMinor & 0xFF,
                    (nUpdateBinMinor & 0xFF00) >> 8);
            } else {
                DBG(&g_i2c_client->dev,
                    "eSwId=0x%x, nMajor=%d, nMinor=%d, nUpdateBinMajor=%u, nUpdateBinMinor=%u\n",
                    eSwId, nMajor, nMinor, nUpdateBinMajor, nUpdateBinMinor);
            }
            if (1 == nIsSwIdValid) {
                DBG(&g_i2c_client->dev, "nFinalIndex = %d\n", nFinalIndex); /*TODO : add for debug */

                for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY  /*By two dimensional array */
                    drv_store_firmware_data(*
                                          (g_sw_id_data[nFinalIndex].p_update_bin +
                                           i), 1024);
#else /*By one dimensional array */
                    drv_store_firmware_data((g_sw_id_data[nFinalIndex].p_update_bin +
                                           i * 1024), 1024);
#endif /*CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY */
                }
            } else {
                DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                    eSwId);

                eSwId = MSG28XX_SW_ID_UNDEFINED;
            }

            if (eSwId < MSG28XX_SW_ID_UNDEFINED && eSwId != 0x0000 &&
                eSwId != 0xFFFF) {
                g_fw_data_count = 0;  /*Reset g_fw_data_count to 0 after copying update firmware data to temp buffer */

                g_update_retry_count = UPDATE_FIRMWARE_RETRY_COUNT;
                g_is_update_info_block_first = 1;   /*Set 1 for indicating main block is complete */
                g_is_update_firmware = 0x11;
                queue_work(g_update_firmware_by_sw_id_workQueue,
                           &g_update_firmware_by_sw_id_work);
                return;
            } else {
                DBG(&g_i2c_client->dev, "The sw id is invalid.\n");
                DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
            }
        } else {
            DBG(&g_i2c_client->dev,
                "The update bin version is older than or equal to the current firmware version on e-flash.\n");
            DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
        }
    } else {
        eSwId = drv_msg28xx_get_sw_id(EMEM_INFO);

        DBG(&g_i2c_client->dev, "eSwId=0x%x\n", eSwId);

        nIsSwIdValid = 0;

        for (j = 0; j < n_swIdListNum; j++) {
            DBG(&g_i2c_client->dev, "g_sw_id_data[%d].n_swId = 0x%x\n", j, g_sw_id_data[j].n_swId);   /*TODO : add for debug */

            if (eSwId == g_sw_id_data[j].n_swId) {
                for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY  /*By two dimensional array */
                    drv_store_firmware_data(*(g_sw_id_data[j].p_update_bin + i),
                                          1024);
#else /*By one dimensional array */
                    drv_store_firmware_data((g_sw_id_data[j].p_update_bin + i * 1024),
                                          1024);
#endif /*CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY */
                }
                nIsSwIdValid = 1;

                break;
            }
        }

        if (0 == nIsSwIdValid) {
            DBG(&g_i2c_client->dev, "eSwId = 0x%x is an undefined SW ID.\n",
                eSwId);

            eSwId = MSG28XX_SW_ID_UNDEFINED;
        }

        if (eSwId < MSG28XX_SW_ID_UNDEFINED && eSwId != 0x0000 &&
            eSwId != 0xFFFF) {
            g_fw_data_count = 0;  /*Reset g_fw_data_count to 0 after copying update firmware data to temp buffer */

            g_update_retry_count = UPDATE_FIRMWARE_RETRY_COUNT;
            g_is_update_info_block_first = 0;   /*Set 0 for indicating main block is broken */
            g_is_update_firmware = 0x11;
            queue_work(g_update_firmware_by_sw_id_workQueue,
                       &g_update_firmware_by_sw_id_work);
            return;
        } else {
            DBG(&g_i2c_client->dev, "The sw id is invalid.\n");
            DBG(&g_i2c_client->dev, "Go to normal boot up process.\n");
        }
    }

    drv_touch_device_hw_reset();

    drv_enable_finger_touch_report();
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

/*-------------------------End of SW ID for MSG28XX----------------------------//*/

static void drv_update_firmware_by_sw_id_do_work(struct work_struct *p_work)
{
    s32 n_ret_val = -1;

    DBG(&g_i2c_client->dev, "*** %s() g_update_retry_count = %d ***\n", __func__,
        g_update_retry_count);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
        drv_msg22xx_get_tp_vender_code(g_tp_vendor_code);

        if (g_tp_vendor_code[0] == 'C' && g_tp_vendor_code[1] == 'N' && g_tp_vendor_code[2] == 'T') { /*for specific TP vendor which store some important information in info block, only update firmware for main block, do not update firmware for info block. */
            n_ret_val = drv_msg22xx_update_firmware(g_fw_data, EMEM_MAIN);
        } else {
            n_ret_val = drv_msg22xx_update_firmware_by_sw_id();
        }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */
    } else if (g_chip_type == CHIP_TYPE_MSG28XX ||
               g_chip_type == CHIP_TYPE_MSG58XXA ||
               g_chip_type == CHIP_TYPE_ILI2117A ||
               g_chip_type == CHIP_TYPE_ILI2118A) {
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
        n_ret_val = drv_msg28xx_update_firmware(g_fw_data, EMEM_MAIN);
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
#ifdef CONFIG_ENABLE_CHIP_TYPE_ILI21XX
        n_ret_val = drv_ilitek_update_firmware(CTPM_FW, true);
#endif /*CONFIG_ENABLE_CHIP_TYPE_ILI21XX */
    } else {
        DBG(&g_i2c_client->dev,
            "This chip type (0x%x) does not support update firmware by sw id\n",
            g_chip_type);

        drv_touch_device_hw_reset();

        drv_enable_finger_touch_report();

        n_ret_val = -1;
        return;
    }

    DBG(&g_i2c_client->dev, "*** Update firmware by sw id result = %d ***\n",
        n_ret_val);

    if (n_ret_val == 0) {
        DBG(&g_i2c_client->dev, "Update firmware by sw id success\n");

        drv_touch_device_hw_reset();

        drv_enable_finger_touch_report();

        if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
            || g_chip_type == CHIP_TYPE_MSG58XXA ||
            g_chip_type == CHIP_TYPE_ILI2117A ||
            g_chip_type == CHIP_TYPE_ILI2118A) {
            g_is_update_info_block_first = 0;
            g_is_update_firmware = 0x00;
        } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
                   g_chip_type == CHIP_TYPE_ILI2121) {
            g_is_update_firmware = 0x00;  /*Set flag to 0x00 for indicating update firmware is finished */

            drv_ilitek_read_touch_panelInfo(&g_ilitek_tp_info);   /*Update TP panel info for ILI21xx */
        }
    } else {                    /*n_ret_val == -1 for MSG22xx/MSG28xx/MSG58xxa or n_ret_val == 2/3/4 for ILI21xx */

        g_update_retry_count--;
        if (g_update_retry_count > 0) {
            DBG(&g_i2c_client->dev, "g_update_retry_count = %d\n",
                g_update_retry_count);
            queue_work(g_update_firmware_by_sw_id_workQueue,
                       &g_update_firmware_by_sw_id_work);
        } else {
            DBG(&g_i2c_client->dev, "Update firmware by sw id failed\n");

            drv_touch_device_hw_reset();

            drv_enable_finger_touch_report();

            if (g_chip_type == CHIP_TYPE_MSG22XX ||
                g_chip_type == CHIP_TYPE_MSG28XX ||
                g_chip_type == CHIP_TYPE_MSG58XXA ||
                g_chip_type == CHIP_TYPE_ILI2117A ||
                g_chip_type == CHIP_TYPE_ILI2118A) {
                g_is_update_info_block_first = 0;
                g_is_update_firmware = 0x00;
            } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
                       g_chip_type == CHIP_TYPE_ILI2121) {
                g_is_update_firmware = 0x00;  /*Set flag to 0x00 for indicating update firmware is finished */

/*drv_ilitek_read_touch_panelInfo(&g_ilitek_tp_info); // TODO : add for debug*/

/*
                if (n_ret_val == 1)
                {
                    DBG(&g_i2c_client->dev, "%s() : File error\n", __func__);
                }
*/
                if (n_ret_val == 2) {
                    DBG(&g_i2c_client->dev, "%s() : I2C transfer error\n",
                        __func__);
                } else if (n_ret_val == 3) {
                    DBG(&g_i2c_client->dev, "%s() : Upgrade FW failed\n",
                        __func__);
                } else if (n_ret_val == 4) {
                    DBG(&g_i2c_client->dev, "%s() : No need to upgrade FW\n",
                        __func__);
                }
/*
                else if (n_ret_val == 5)
                {
                    DBG(&g_i2c_client->dev, "%s() : System is suspend\n", __func__);
                }
*/
            }
        }
    }
}

#endif /*CONFIG_UPDATE_FIRMWARE_BY_SW_ID */

/*------------------------------------------------------------------------------//*/

static s32 drv_msg22xx_update_firmware(u8 szFw_data[][1024], enum emem_type_e eEmemType)
{
    u32 i = 0, index = 0;
    u32 nCrcMain = 0, nCrcMainTp = 0;
    u32 nCrcInfo = 0, nCrcInfoTp = 0;
    u32 nRemainSize, nBlockSize, n_size;
    u32 nTimeOut = 0;
    u16 nReg_data = 0;
    s32 rc = 0;
#if defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    u8 szDbBusTx_data[128] = { 0 };
#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    u32 nSizePerWrite = 1;
#else
    u32 nSizePerWrite = 125;
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */
#elif defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
    u8 szDbBusTx_data[1024] = { 0 };
#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    u32 nSizePerWrite = 1;
#else
    u32 nSizePerWrite = 1021;
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */
#endif

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    DBG(&g_i2c_client->dev, "*** g_msg22xx_chip_revision = 0x%x ***\n",
        g_msg22xx_chip_revision);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
#if defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
        nSizePerWrite = 41;     /*123/3=41 */
#elif defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
        nSizePerWrite = 340;    /*1020/3=340 */
#endif
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
#if defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
        nSizePerWrite = 31;     /*124/4=31 */
#elif defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
        nSizePerWrite = 255;    /*1020/4=255 */
#endif
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
    }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    g_is_update_firmware = 0x01;  /*Set flag to 0x01 for indicating update firmware is processing */

    drv_msg22xx_convert_fw_data_two_dimen_to_one_dimen(szFw_data, g_one_dimen_fw_data);

    drv_touch_device_hw_reset();

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    DBG(&g_i2c_client->dev, "Erase start\n");

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
    /*Exit flash low power mode */
    reg_set_l_byte_value(0x1619, BIT1);

    /*Change PIU clock to 48MHz */
    reg_set_l_byte_value(0x1E23, BIT6);

    /*Change mcu clock deglitch mux source */
    reg_set_l_byte_value(0x1E54, BIT0);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

    /*Set PROGRAM password */
    reg_set_l_byte_value(0x161A, 0xBA);
    reg_set_l_byte_value(0x161B, 0xAB);

    if (eEmemType == EMEM_ALL) {    /*48KB + 512Byte */
        DBG(&g_i2c_client->dev, "Erase all block\n");

        /*Clear pce */
        reg_set_l_byte_value(0x1618, 0x80);
        mdelay(100);

        /*Chip erase */
        reg_set_16bit_value(0x160E, BIT3);

        DBG(&g_i2c_client->dev, "Wait erase done flag\n");

        while (1) {             /*Wait erase done flag */
            nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
            nReg_data = nReg_data & BIT1;

            DBG(&g_i2c_client->dev, "Wait erase done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT1) {
                break;
            }

            mdelay(50);

            if ((nTimeOut++) > 30) {
                DBG(&g_i2c_client->dev, "Erase all block failed. Timeout.\n");

                goto UpdateEnd;
            }
        }
    } else if (eEmemType == EMEM_MAIN) {    /*48KB (32+8+8) */
        DBG(&g_i2c_client->dev, "Erase main block\n");

        for (i = 0; i < 3; i++) {
            /*Clear pce */
            reg_set_l_byte_value(0x1618, 0x80);
            mdelay(10);

            if (i == 0) {
                reg_set_16bit_value(0x1600, 0x0000);
            } else if (i == 1) {
                reg_set_16bit_value(0x1600, 0x8000);
            } else if (i == 2) {
                reg_set_16bit_value(0x1600, 0xA000);
            }

            /*Sector erase */
            reg_set_16bit_value(0x160E, (reg_get16_bit_value(0x160E) | BIT2));

            DBG(&g_i2c_client->dev, "Wait erase done flag\n");

            nReg_data = 0;
            nTimeOut = 0;

            while (1) {         /*Wait erase done flag */
                nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
                nReg_data = nReg_data & BIT1;

                DBG(&g_i2c_client->dev,
                    "Wait erase done flag nReg_data = 0x%x\n", nReg_data);

                if (nReg_data == BIT1) {
                    break;
                }
                mdelay(50);

                if ((nTimeOut++) > 30) {
                    DBG(&g_i2c_client->dev,
                        "Erase main block failed. Timeout.\n");

                    goto UpdateEnd;
                }
            }
        }
    } else if (eEmemType == EMEM_INFO) {    /*512Byte */
        DBG(&g_i2c_client->dev, "Erase info block\n");

        /*Clear pce */
        reg_set_l_byte_value(0x1618, 0x80);
        mdelay(10);

        reg_set_16bit_value(0x1600, 0xC000);

        /*Sector erase */
        reg_set_16bit_value(0x160E, (reg_get16_bit_value(0x160E) | BIT2));

        DBG(&g_i2c_client->dev, "Wait erase done flag\n");

        while (1) {             /*Wait erase done flag */
            nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
            nReg_data = nReg_data & BIT1;

            DBG(&g_i2c_client->dev, "Wait erase done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT1) {
                break;
            }
            mdelay(50);

            if ((nTimeOut++) > 30) {
                DBG(&g_i2c_client->dev, "Erase info block failed. Timeout.\n");

                goto UpdateEnd;
            }
        }
    }

    DBG(&g_i2c_client->dev, "Erase end\n");

    /*Hold reset pin before program */
    reg_set_l_byte_value(0x1E06, 0x00);

     /**/
        /*Program */
         /**/ if (eEmemType == EMEM_ALL || eEmemType == EMEM_MAIN) {    /*48KB */
        DBG(&g_i2c_client->dev, "Program main block start\n");

        /*Program main block */
        reg_set_16bit_value(0x161A, 0xABBA);
        reg_set_16bit_value(0x1618, (reg_get16_bit_value(0x1618) | 0x80));

        reg_set_16bit_value(0x1600, 0x0000);   /*Set start address of main block */

        reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));    /*Enable burst mode */

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
            reg_set_16bit_value_on(0x160A, BIT0);   /*Set Bit0 = 1 for enable I2C 400KHz burst write mode, let e-flash discard the last 2 dummy byte. */
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
            reg_set_16bit_value_on(0x160A, BIT0);
            reg_set_16bit_value_on(0x160A, BIT1);   /*Set Bit0 = 1, Bit1 = 1 for enable I2C 400KHz burst write mode, let e-flash discard the last 3 dummy byte. */
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
        }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        /*Program start */
        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x16;
        szDbBusTx_data[2] = 0x02;

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3);

        szDbBusTx_data[0] = 0x20;

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

        nRemainSize = MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE * 1024;  /*48KB */
        index = 0;

        while (nRemainSize > 0) {
            if (nRemainSize > nSizePerWrite) {
                nBlockSize = nSizePerWrite;
            } else {
                nBlockSize = nRemainSize;
            }

            szDbBusTx_data[0] = 0x10;
            szDbBusTx_data[1] = 0x16;
            szDbBusTx_data[2] = 0x02;

            n_size = 3;

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
            if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
                u32 j = 0;

                for (i = 0; i < nBlockSize; i++) {
                    for (j = 0; j < 3; j++) {
                        szDbBusTx_data[3 + (i * 3) + j] =
                            g_one_dimen_fw_data[index * nSizePerWrite + i];
                    }
                    n_size = n_size + 3;
                }
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
                u32 j = 0;

                for (i = 0; i < nBlockSize; i++) {
                    for (j = 0; j < 4; j++) {
                        szDbBusTx_data[3 + (i * 4) + j] =
                            g_one_dimen_fw_data[index * nSizePerWrite + i];
                    }
                    n_size = n_size + 4;
                }
#else /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_C */
                for (i = 0; i < nBlockSize; i++) {
                    szDbBusTx_data[3 + i] =
                        g_one_dimen_fw_data[index * nSizePerWrite + i];
                    n_size++;
                }
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
            } else {
                for (i = 0; i < nBlockSize; i++) {
                    szDbBusTx_data[3 + i] =
                        g_one_dimen_fw_data[index * nSizePerWrite + i];
                    n_size++;
                }
            }
#else
            for (i = 0; i < nBlockSize; i++) {
                szDbBusTx_data[3 + i] =
                    g_one_dimen_fw_data[index * nSizePerWrite + i];
                n_size++;
            }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

            index++;

            rc = iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], n_size);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
                    "Write firmware data failed, rc = %d. Exit program procedure.\n",
                    rc);

                goto UpdateEnd;
            }

            nRemainSize = nRemainSize - nBlockSize;
        }

        /*Program end */
        szDbBusTx_data[0] = 0x21;

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
            reg_set_16Bit_value_off(0x160A, BIT0);  /*Set Bit0 = 0 for disable I2C 400KHz burst write mode, let e-flash discard the last 2 dummy byte. */
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
            reg_set_16Bit_value_off(0x160A, BIT0);
            reg_set_16Bit_value_off(0x160A, BIT1);  /*Set Bit0 = 0, Bit1 = 0 for disable I2C 400KHz burst write mode, let e-flash discard the last 3 dummy byte. */
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
        }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));   /*Clear burst mode */

        DBG(&g_i2c_client->dev, "Wait main block write done flag\n");

        nReg_data = 0;
        nTimeOut = 0;

        while (1) {             /*Wait write done flag */
            /*Polling 0x1610 is 0x0002 */
            nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
            nReg_data = nReg_data & BIT1;

            DBG(&g_i2c_client->dev, "Wait write done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT1) {
                break;
            }
            mdelay(10);

            if ((nTimeOut++) > 30) {
                DBG(&g_i2c_client->dev, "Write failed. Timeout.\n");

                goto UpdateEnd;
            }
        }

        DBG(&g_i2c_client->dev, "Program main block end\n");
    }

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_INFO) {  /*512 Byte */
        DBG(&g_i2c_client->dev, "Program info block start\n");

        /*Program info block */
        reg_set_16bit_value(0x161A, 0xABBA);
        reg_set_16bit_value(0x1618, (reg_get16_bit_value(0x1618) | 0x80));

        reg_set_16bit_value(0x1600, 0xC000);   /*Set start address of info block */

        reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));    /*Enable burst mode */

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
            reg_set_16bit_value_on(0x160A, BIT0);   /*Set Bit0 = 1 for enable I2C 400KHz burst write mode, let e-flash discard the last 2 dummy byte. */
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
            reg_set_16bit_value_on(0x160A, BIT0);
            reg_set_16bit_value_on(0x160A, BIT1);   /*Set Bit0 = 1, Bit1 = 1 for enable I2C 400KHz burst write mode, let e-flash discard the last 3 dummy byte. */
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
        }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        /*Program start */
        szDbBusTx_data[0] = 0x10;
        szDbBusTx_data[1] = 0x16;
        szDbBusTx_data[2] = 0x02;

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3);

        szDbBusTx_data[0] = 0x20;

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

        nRemainSize = MSG22XX_FIRMWARE_INFO_BLOCK_SIZE; /*512Byte */
        index = 0;

        while (nRemainSize > 0) {
            if (nRemainSize > nSizePerWrite) {
                nBlockSize = nSizePerWrite;
            } else {
                nBlockSize = nRemainSize;
            }

            szDbBusTx_data[0] = 0x10;
            szDbBusTx_data[1] = 0x16;
            szDbBusTx_data[2] = 0x02;

            n_size = 3;

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
            if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
                u32 j = 0;

                for (i = 0; i < nBlockSize; i++) {
                    for (j = 0; j < 3; j++) {
                        szDbBusTx_data[3 + (i * 3) + j] =
                            g_one_dimen_fw_data[(MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE *
                                              1024) + (index * nSizePerWrite) +
                                             i];
                    }
                    n_size = n_size + 3;
                }
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
                u32 j = 0;

                for (i = 0; i < nBlockSize; i++) {
                    for (j = 0; j < 4; j++) {
                        szDbBusTx_data[3 + (i * 4) + j] =
                            g_one_dimen_fw_data[(MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE *
                                              1024) + (index * nSizePerWrite) +
                                             i];
                    }
                    n_size = n_size + 4;
                }
#else /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_C */
                for (i = 0; i < nBlockSize; i++) {
                    szDbBusTx_data[3 + i] =
                        g_one_dimen_fw_data[(MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE *
                                          1024) + (index * nSizePerWrite) + i];
                    n_size++;
                }
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
            } else {
                for (i = 0; i < nBlockSize; i++) {
                    szDbBusTx_data[3 + i] =
                        g_one_dimen_fw_data[(MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE *
                                          1024) + (index * nSizePerWrite) + i];
                    n_size++;
                }
            }
#else
            for (i = 0; i < nBlockSize; i++) {
                szDbBusTx_data[3 + i] =
                    g_one_dimen_fw_data[(MSG22XX_FIRMWARE_MAIN_BLOCK_SIZE * 1024) +
                                     (index * nSizePerWrite) + i];
                n_size++;
            }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

            index++;

            iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], n_size);

            nRemainSize = nRemainSize - nBlockSize;
        }

        /*Program end */
        szDbBusTx_data[0] = 0x21;

        iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 1);

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        if (g_msg22xx_chip_revision >= CHIP_TYPE_MSG22XX_REVISION_U05) {
#if defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A)
            reg_set_16Bit_value_off(0x160A, BIT0);  /*Set Bit0 = 0 for disable I2C 400KHz burst write mode, let e-flash discard the last 2 dummy byte. */
#elif defined(CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_B)
            reg_set_16Bit_value_off(0x160A, BIT0);
            reg_set_16Bit_value_off(0x160A, BIT1);  /*Set Bit0 = 0, Bit1 = 0 for disable I2C 400KHz burst write mode, let e-flash discard the last 3 dummy byte. */
#endif /*CONFIG_ENABLE_UPDATE_FW_I2C_SPEED_400K_METHOD_A */
        }
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));   /*Clear burst mode */

        DBG(&g_i2c_client->dev, "Wait info block write done flag\n");

        nReg_data = 0;
        nTimeOut = 0;

        while (1) {             /*Wait write done flag */
            /*Polling 0x1610 is 0x0002 */
            nReg_data = reg_get16_bit_value(0x1610);    /*Memory status */
            nReg_data = nReg_data & BIT1;

            DBG(&g_i2c_client->dev, "Wait write done flag nReg_data = 0x%x\n",
                nReg_data);

            if (nReg_data == BIT1) {
                break;
            }
            mdelay(10);

            if ((nTimeOut++) > 30) {
                DBG(&g_i2c_client->dev, "Write failed. Timeout.\n");

                goto UpdateEnd;
            }
        }

        DBG(&g_i2c_client->dev, "Program info block end\n");
    }

UpdateEnd:

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_MAIN) {
        /*Get CRC 32 from updated firmware bin file */
        nCrcMain = g_one_dimen_fw_data[0xBFFF] << 24;
        nCrcMain |= g_one_dimen_fw_data[0xBFFE] << 16;
        nCrcMain |= g_one_dimen_fw_data[0xBFFD] << 8;
        nCrcMain |= g_one_dimen_fw_data[0xBFFC];

        /*CRC Main from TP */
        DBG(&g_i2c_client->dev, "Get Main CRC from TP\n");

        nCrcMainTp = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_MAIN);

        DBG(&g_i2c_client->dev, "nCrcMain=0x%x, nCrcMainTp=0x%x\n", nCrcMain,
            nCrcMainTp);
    }

    if (eEmemType == EMEM_ALL || eEmemType == EMEM_INFO) {
        nCrcInfo = g_one_dimen_fw_data[0xC1FF] << 24;
        nCrcInfo |= g_one_dimen_fw_data[0xC1FE] << 16;
        nCrcInfo |= g_one_dimen_fw_data[0xC1FD] << 8;
        nCrcInfo |= g_one_dimen_fw_data[0xC1FC];

        /*CRC Info from TP */
        DBG(&g_i2c_client->dev, "Get Info CRC from TP\n");

        nCrcInfoTp = drv_msg22xx_get_firmware_crc_by_hardware(EMEM_INFO);

        DBG(&g_i2c_client->dev, "nCrcInfo=0x%x, nCrcInfoTp=0x%x\n", nCrcInfo,
            nCrcInfoTp);
    }

    g_fw_data_count = 0;          /*Reset g_fw_data_count to 0 after update firmware */

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    drv_touch_device_hw_reset();

    g_is_update_firmware = 0x00;  /*Set flag to 0x00 for indicating update firmware is finished */

    if (eEmemType == EMEM_ALL) {
        if ((nCrcMainTp != nCrcMain) || (nCrcInfoTp != nCrcInfo)) {
            DBG(&g_i2c_client->dev, "Update FAILED\n");

            return -1;
        }
    } else if (eEmemType == EMEM_MAIN) {
        if (nCrcMainTp != nCrcMain) {
            DBG(&g_i2c_client->dev, "Update FAILED\n");

            return -1;
        }
    } else if (eEmemType == EMEM_INFO) {
        if (nCrcInfoTp != nCrcInfo) {
            DBG(&g_i2c_client->dev, "Update FAILED\n");

            return -1;
        }
    }

    DBG(&g_i2c_client->dev, "Update SUCCESS\n");

    return 0;
}

/*------------------------------------------------------------------------------//*/

static u32 hex_to_dec(char *pHex, s32 n_length)
{
    u32 n_ret_val = 0, nTemp = 0, i;
    s32 nShift = (n_length - 1) * 4;

    for (i = 0; i < n_length; nShift -= 4, i++) {
        if ((pHex[i] >= '0') && (pHex[i] <= '9')) {
            nTemp = pHex[i] - '0';
        } else if ((pHex[i] >= 'a') && (pHex[i] <= 'f')) {
            nTemp = (pHex[i] - 'a') + 10;
        } else if ((pHex[i] >= 'A') && (pHex[i] <= 'F')) {
            nTemp = (pHex[i] - 'A') + 10;
        } else {
            return -1;
        }

        n_ret_val |= (nTemp << nShift);
    }

    return n_ret_val;
}

s32 drv_ilitek_convert_firmware_data(u8 *p_buf, u32 n_size)
{                               /*for ILI21xx */
    u32 i = 0, j = 0, k = 0;
    u32 nApStartAddr = 0xFFFF, nDfStartAddr = 0xFFFF, nStartAddr =
        0xFFFF, nExAddr = 0;
    u32 nApEndAddr = 0x0, nDfEndAddr = 0x0, nEndAddr = 0x0;
    u32 nApChecksum = 0x0, nDfChecksum = 0x0, nChecksum = 0x0, n_length =
        0, n_addr = 0, n_type = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_n_ilitek_ap_start_addr = 0;   /*Reset g_n_ilitek_ap_start_addr and g_n_ilitek_ap_end_addr and g_n_ilitek_ap_check_sum to 0 */
    g_n_ilitek_ap_end_addr = 0;
    g_n_ilitek_ap_check_sum = 0;

    if (0 == n_size) {
        DBG(&g_i2c_client->dev, "n_size is 0. Can not convert firmware data.\n");
        return -1;
    }

    memset(g_ilitek_fw_data, 0xFF, ILITEK_MAX_UPDATE_FIRMWARE_BUFFER_SIZE * 1024);

    for (i = 0; i < n_size;) {
        s32 n_offset;

        n_length = hex_to_dec(&p_buf[i + 1], 2);
        n_addr = hex_to_dec(&p_buf[i + 3], 4);
        n_type = hex_to_dec(&p_buf[i + 7], 2);
/*DBG(&g_i2c_client->dev, "n_length = %d(0x%02X), n_addr = %d(0x%04X), n_type = %d\n", n_length, n_length, n_addr, n_addr, n_type);*/

        /*calculate checksum */
        for (j = 8; j < (2 + 4 + 2 + (n_length * 2)); j += 2) {
            if (n_type == 0x00) {
                /*for ice mode write method */
                nChecksum = nChecksum + hex_to_dec(&p_buf[i + 1 + j], 2);

                if (n_addr + (j - 8) / 2 < nDfStartAddr) {
                    nApChecksum = nApChecksum + hex_to_dec(&p_buf[i + 1 + j], 2);
/*DBG(&g_i2c_client->dev, "n_addr = 0x%04X, nApChecksum = 0x%06X, _data = 0x%02X\n", n_addr + (j - 8)/2, nApChecksum, hex_to_dec(&p_buf[i + 1 + j], 2));*/
                } else {
                    nDfChecksum = nDfChecksum + hex_to_dec(&p_buf[i + 1 + j], 2);
/*DBG(&g_i2c_client->dev, "n_addr = 0x%04X, nDfChecksum = 0x%06X, _data = 0x%02X\n", n_addr + (j - 8)/2, nDfChecksum, hex_to_dec(&p_buf[i + 1 + j], 2));*/
                }
            }
        }

        if (n_type == 0x04) {
            nExAddr = hex_to_dec(&p_buf[i + 9], 4);
        }
        n_addr = n_addr + (nExAddr << 16);

        if (p_buf[i + 1 + j + 2] == 0x0D) {
            n_offset = 2;
        } else {
            n_offset = 1;
        }

        if (n_type == 0x00) {
            if (n_addr > (ILITEK_MAX_UPDATE_FIRMWARE_BUFFER_SIZE * 1024)) {
                DBG(&g_i2c_client->dev, "Invalid hex format!\n");

                return -1;
            }

            if (n_addr < nStartAddr) {
                nStartAddr = n_addr;
            }
            if ((n_addr + n_length) > nEndAddr) {
                nEndAddr = n_addr + n_length;
            }

            /*for Bl protocol 1.4+, nApStartAddr and nApEndAddr */
            if (n_addr < nApStartAddr) {
                nApStartAddr = n_addr;
            }

            if ((n_addr + n_length) > nApEndAddr && (n_addr < nDfStartAddr)) {
                nApEndAddr = n_addr + n_length - 1;

                if (nApEndAddr > nDfStartAddr) {
                    nApEndAddr = nDfStartAddr - 1;
                }
            }

            /*for Bl protocol 1.4+, bl_end_addr */
            if ((n_addr + n_length) > nDfEndAddr && (n_addr >= nDfStartAddr)) {
                nDfEndAddr = n_addr + n_length;
            }

            /*fill data */
            for (j = 0, k = 0; j < (n_length * 2); j += 2, k++) {
                g_ilitek_fw_data[n_addr + k] = hex_to_dec(&p_buf[i + 9 + j], 2);
            }
        }

        i += 1 + 2 + 4 + 2 + (n_length * 2) + 2 + n_offset;
    }

    g_n_ilitek_ap_start_addr = nApStartAddr;    /*Assign g_n_ilitek_ap_start_addr and g_n_ilitek_ap_end_addr and g_n_ilitek_ap_check_sum here for upgrade firmware by MTPTool APK later */
    g_n_ilitek_ap_end_addr = nApEndAddr;
    g_n_ilitek_ap_check_sum = nApChecksum;

    DBG(&g_i2c_client->dev,
        "nStartAddr = 0x%06X, nEndAddr = 0x%06X, nChecksum = 0x%06X\n",
        nStartAddr, nEndAddr, nChecksum);
    DBG(&g_i2c_client->dev,
        "nApStartAddr = 0x%06X, nApEndAddr = 0x%06X, nApChecksum = 0x%06X\n",
        nApStartAddr, nApEndAddr, nApChecksum);
    DBG(&g_i2c_client->dev,
        "nDfStartAddr = 0x%06X, nDfEndAddr = 0x%06X, nDfChecksum = 0x%06X\n",
        nDfStartAddr, nDfEndAddr, nDfChecksum);

    return 0;
}

s32 drv_ilitek_update_firmware(u8 *pszFw_data, bool bIsUpdateFirmwareBySwId)
{                               /*for ILI21xx */
    s32 rc = 0, nUpdateRetryCount = 0, nUpgradeStatus = 0, nUpdateLength =
        0, nCheckFwFlag = 0, nChecksum = 0, i = 0, j = 0, k = 0;
    u8 sz_fw_version[4] = { 0 };
    u8 sz_buf[512] = { 0 };
    u8 szCmd[2] = { 0 };
    u32 nApStartAddr = 0, nDfStartAddr = 0, nApEndAddr = 0, nDfEndAddr =
        0, nApChecksum = 0, nDfChecksum = 0, nTemp = 0, nIcChecksum = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    g_fw_data_count = 0;          /*Reset g_fw_data_count to 0 after update firmware */

    if (true == bIsUpdateFirmwareBySwId) {
        nApStartAddr =
            (pszFw_data[0] << 16) + (pszFw_data[1] << 8) + pszFw_data[2];
        nApEndAddr = (pszFw_data[3] << 16) + (pszFw_data[4] << 8) + pszFw_data[5];
        nApChecksum = (pszFw_data[6] << 16) + (pszFw_data[7] << 8) + pszFw_data[8];
        nDfStartAddr =
            (pszFw_data[9] << 16) + (pszFw_data[10] << 8) + pszFw_data[11];
        nDfEndAddr =
            (pszFw_data[12] << 16) + (pszFw_data[13] << 8) + pszFw_data[14];
        nDfChecksum =
            (pszFw_data[15] << 16) + (pszFw_data[16] << 8) + pszFw_data[17];
        sz_fw_version[0] = pszFw_data[18];
        sz_fw_version[1] = pszFw_data[19];
        sz_fw_version[2] = pszFw_data[20];
        sz_fw_version[3] = pszFw_data[21];
        nUpdateLength =
            ((pszFw_data[26] << 16) + (pszFw_data[27] << 8) + pszFw_data[28]) +
            ((pszFw_data[29] << 16) + (pszFw_data[30] << 8) + pszFw_data[31]);

        DBG(&g_i2c_client->dev,
            "nApStartAddr=0x%06X, nApEndAddr=0x%06X, nApChecksum=0x%06X\n",
            nApStartAddr, nApEndAddr, nApChecksum);
        DBG(&g_i2c_client->dev,
            "nDfStartAddr=0x%06X, nDfEndAddr=0x%06X, nDfChecksum=0x%06X\n",
            nDfStartAddr, nDfEndAddr, nDfChecksum);
        DBG(&g_i2c_client->dev, "nUpdateLength=%d\n", nUpdateLength);

        /*Check if FW is ready */
        szCmd[0] = ILITEK_TP_CMD_READ_DATA;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &szCmd[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_CMD_READ_DATA : iic_write_data() failed, rc = %d\n",
                rc);
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_buf[0], 3);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_CMD_READ_DATA : iic_read_data() failed, rc = %d\n",
                rc);
        }

        DBG(&g_i2c_client->dev, "sz_buf[0][1][2] = 0x%X, 0x%X, 0x%X\n", sz_buf[0],
            sz_buf[1], sz_buf[2]);
        if (sz_buf[1] >= 0x80) {
            DBG(&g_i2c_client->dev,
                "FW is ready. Continue to check FW version.\n");

            for (i = 0; i < 4; i++) {
                DBG(&g_i2c_client->dev,
                    "g_ilitek_tp_info.sz_fw_version[%d] = %d, pszFw_data[%d] = %d\n",
                    i, g_ilitek_tp_info.sz_fw_version[i], i, pszFw_data[i + 18]);

                if (g_ilitek_tp_info.sz_fw_version[i] == 0xFF) {
                    DBG(&g_i2c_client->dev, "FW version is 0xFF\n");
                    break;
                } else if (g_ilitek_tp_info.sz_fw_version[i] != pszFw_data[i + 18]) {
                    break;
                } else if ((i == 3) &&
                           (g_ilitek_tp_info.sz_fw_version[3] ==
                            pszFw_data[3 + 18])) {
                    nCheckFwFlag = 1;
                }
            }
        } else {
            DBG(&g_i2c_client->dev,
                "FW is not ready. Start to upgrade FW directly.\n");
        }
    } else {
        nApStartAddr = g_n_ilitek_ap_start_addr;
        nApEndAddr = g_n_ilitek_ap_end_addr;
        nUpdateLength = g_n_ilitek_ap_end_addr;
        nApChecksum = g_n_ilitek_ap_check_sum;

        DBG(&g_i2c_client->dev,
            "g_n_ilitek_ap_start_addr=0x%06X, g_n_ilitek_ap_end_addr=0x%06X, g_n_ilitek_ap_check_sum = 0x%06X\n",
            g_n_ilitek_ap_start_addr, g_n_ilitek_ap_end_addr, g_n_ilitek_ap_check_sum);
    }

UPDATE_RETRY:

    if (drv_ilitek_entry_ice_mode())    /*Entry ICE Mode */
        return 2;
    if (drv_ilitek_ice_mode_initial())
        return 2;

    if (true == bIsUpdateFirmwareBySwId) {
        nIcChecksum = drv_ilitek_get_check_sum(nApStartAddr, nApEndAddr);
        if (nIcChecksum == nApChecksum) {
            nCheckFwFlag = 1;
        }

        if (nChecksum != 0xFFFFFFFF && nCheckFwFlag == 1) {
            DBG(&g_i2c_client->dev,
                "drv_ilitek_update_firmware() do not need update\n");
            if (exit_ice_mode() < 0) {
                return 2;
            } else {
                return 4;
            }
        }
    }
    mdelay(5);

    for (i = 0; i <= 0xd000; i += 0x1000) {
/*DBG(&g_i2c_client->dev, "%s, i = %X\n", __func__, i);*/
        if (out_write(0x041000, 0x06, 1) < 0)
            return 2;
        mdelay(3);

        if (out_write(0x041004, 0x66aa5500, 4) < 0)
            return 2;
        mdelay(3);

        nTemp = (i << 8) + 0x20;
        if (out_write(0x041000, nTemp, 4) < 0)
            return 2;
        mdelay(3);

        if (out_write(0x041004, 0x66aa5500, 4) < 0)
            return 2;
        mdelay(20);

        for (j = 0; j < 50; j++) {
            if (out_write(0x041000, 0x05, 1) < 0)
                return 2;
            if (out_write(0x041004, 0x66aa5500, 4) < 0)
                return 2;
            mdelay(1);

            sz_buf[0] = in_write(0x041013);
/*DBG(&g_i2c_client->dev, "%s, sz_buf[0] = %X\n", __func__, sz_buf[0]);*/
            if (sz_buf[0] == 0) {
                break;
            } else {
                mdelay(2);
            }
        }
    }
    mdelay(100);

    DBG(&g_i2c_client->dev, "ILITEK_UPDATE_FIRMWARE_PAGE_LENGTH:%d\n",
        ILITEK_UPDATE_FIRMWARE_PAGE_LENGTH);

    for (i = nApStartAddr; i < nApEndAddr;
         i += ILITEK_UPDATE_FIRMWARE_PAGE_LENGTH) {
/*DBG(&g_i2c_client->dev, "%s, i = %X\n", __func__, i);*/
        if (out_write(0x041000, 0x06, 1) < 0)
            return 2;
        if (out_write(0x041004, 0x66aa5500, 4) < 0)
            return 2;
        nTemp = (i << 8) + 0x02;
        if (out_write(0x041000, nTemp, 4) < 0)
            return 2;
        if (out_write
            (0x041004, 0x66aa5500 + ILITEK_UPDATE_FIRMWARE_PAGE_LENGTH - 1,
             4) < 0)
            return 2;

        sz_buf[0] = 0x25;
        sz_buf[3] = (char)((0x041020 & 0x00FF0000) >> 16);
        sz_buf[2] = (char)((0x041020 & 0x0000FF00) >> 8);
        sz_buf[1] = (char)((0x041020 & 0x000000FF));

        for (k = 0; k < ILITEK_UPDATE_FIRMWARE_PAGE_LENGTH; k++) {
            if (true == bIsUpdateFirmwareBySwId) {
                sz_buf[4 + k] = pszFw_data[i + 32 + k];
            } else {
                sz_buf[4 + k] = pszFw_data[i + k];
            }
        }

        if (iic_write_data
            (slave_i2c_id_dw_i2c, sz_buf,
             ILITEK_UPDATE_FIRMWARE_PAGE_LENGTH + 4) < 0) {
            DBG(&g_i2c_client->dev,
                "iic_write_data() error, address = 0x%X, start_addr = 0x%X, end_addr = 0x%X\n",
                (int)i, (int)nApStartAddr, (int)nApEndAddr);
            return 2;
        }

        nUpgradeStatus = (i * 100) / nUpdateLength;
        DBG(&g_i2c_client->dev, "%cupgrade firmware(ap code), %02d%c\n", 0x0D,
            nUpgradeStatus, '%');
        mdelay(3);
    }

    nIcChecksum = 0;
    nIcChecksum = drv_ilitek_get_check_sum(nApStartAddr, nApEndAddr);
    DBG(&g_i2c_client->dev, "nIcChecksum = 0x%X, nApChecksum = 0x%X\n",
        nIcChecksum, nApChecksum);

    if (nIcChecksum != nApChecksum) {
        if (nUpdateRetryCount < 2) {
            nUpdateRetryCount++;
            goto UPDATE_RETRY;
        } else {
            exit_ice_mode();
            DBG(&g_i2c_client->dev, "Upgrade FW Failed\n");
            return 3;
        }
    }

    DBG(&g_i2c_client->dev, "%cupgrade firmware(ap code), %02d%c\n", 0x0D,
        nUpgradeStatus, '%');
    if (exit_ice_mode() < 0)
        return 2;

    DBG(&g_i2c_client->dev, "Upgrade ILITEK_IOCTL_I2C_RESET end\n");

    szCmd[0] = ILITEK_TP_CMD_READ_DATA;
    rc = iic_write_data(slave_i2c_id_dw_i2c, &szCmd[0], 1);
    if (rc < 0) {
        DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_READ_DATA : iic_write_data() failed, rc = %d\n", rc);
    }

    mdelay(10);

    rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_buf[0], 3);
    if (rc < 0) {
        DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_READ_DATA : iic_read_data() failed, rc = %d\n", rc);
    }

    DBG(&g_i2c_client->dev, "sz_buf[0][1][2] = 0x%X, 0x%X, 0x%X\n", sz_buf[0],
        sz_buf[1], sz_buf[2]);
    if (sz_buf[1] >= 0x80) {
        DBG(&g_i2c_client->dev, "Upgrade FW Success\n");
    } else {
        if (nUpdateRetryCount < 2) {
            nUpdateRetryCount++;
            goto UPDATE_RETRY;
        } else {
            DBG(&g_i2c_client->dev, "Upgrade FW Failed\n");
            return 3;
        }
    }
    mdelay(100);

    return 0;
}

s32 drv_ilitek_update_firmware_by_sd_card(const char *p_file_path)
{                               /*for ILI21xx */
    s32 n_ret_val = 0;
    struct file *pfile = NULL;
    struct inode *inode;
    s32 fsize = 0;
    mm_segment_t old_fs;
    loff_t pos;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    pfile = filp_open(p_file_path, O_RDONLY, 0);
    if (IS_ERR(pfile)) {
        DBG(&g_i2c_client->dev, "Error occurred while opening file %s.\n",
            p_file_path);
        return -1;
    }
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 18, 0)
    inode = pfile->f_dentry->d_inode;
#else
    inode = pfile->f_path.dentry->d_inode;
#endif

    fsize = inode->i_size;

    DBG(&g_i2c_client->dev, "fsize = %d\n", fsize);

    if (fsize <= 0) {
        filp_close(pfile, NULL);
        return -1;
    }

    /*read firmware */
    memset(g_ilitek_fw_data_buf, 0, ILITEK_ILI21XX_FIRMWARE_SIZE * 1024);

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    pos = 0;
    vfs_read(pfile, g_ilitek_fw_data_buf, fsize, &pos);

    filp_close(pfile, NULL);
    set_fs(old_fs);

    drv_disable_finger_touch_report();

    if ((g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) &&
        (fsize < (ILITEK_ILI21XX_FIRMWARE_SIZE * 1024))) {
        n_ret_val = drv_ilitek_convert_firmware_data(g_ilitek_fw_data_buf, fsize);   /*convert _gIliTekFw_dataBuf to g_ilitek_fw_data */

        if (0 == n_ret_val) {
            n_ret_val = drv_ilitek_update_firmware(g_ilitek_fw_data, false);

            if (0 == n_ret_val) {
                drv_ilitek_read_touch_panelInfo(&g_ilitek_tp_info);   /*Update TP panel info for ILI21xx */
            }
        } else {
            DBG(&g_i2c_client->dev, "Invalid hex file for upgrade firmware!\n");
        }
    } else {
        DBG(&g_i2c_client->dev, "Unsupported chip type = 0x%x\n", g_chip_type);
        n_ret_val = -1;
    }

    drv_enable_finger_touch_report();

    return n_ret_val;
}

/*------------------------------------------------------------------------------//*/
