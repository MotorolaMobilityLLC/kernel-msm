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
 * @file    ilitek_drv_mp_test.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/
/*INCLUDE FILE*/
/*=============================================================*/

#include "ilitek_drv_mp_test.h"

#ifdef CONFIG_ENABLE_ITO_MP_TEST
#include <linux/ctype.h>

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
/* The below .h file are included for MSG22xx */
/*Modify.*/
#include "msg22xx_open_test_RIU1_X.h"
#include "msg22xx_open_test_RIU2_X.h"
#include "msg22xx_open_test_RIU3_X.h"

#include "msg22xx_open_test_RIU1_Y.h"
#include "msg22xx_open_test_RIU2_Y.h"
#include "msg22xx_open_test_RIU3_Y.h"

/*Modify.*/
#include "msg22xx_short_test_RIU1_X.h"
#include "msg22xx_short_test_RIU2_X.h"
#include "msg22xx_short_test_RIU3_X.h"
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
#include "msg22xx_short_test_RIU4_X.h"
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

#include "msg22xx_short_test_RIU1_Y.h"
#include "msg22xx_short_test_RIU2_Y.h"
#include "msg22xx_short_test_RIU3_Y.h"
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
#include "msg22xx_short_test_RIU4_Y.h"
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
/* The below .h file are included for MSG28xx */
#include "msg28xx_mp_test_X.h"
#include "msg28xx_mp_test_Y.h"
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

/*=============================================================*/
/*PREPROCESSOR CONSTANT DEFINITION*/
/*=============================================================*/

/*Modify.*/
#define TP_TYPE_X    (2)
#define TP_TYPE_Y    (1)

/*=============================================================*/
/*EXTERN VARIABLE DECLARATION*/
/*=============================================================*/

extern u32 slave_i2c_id_db_bus;
extern u32 slave_i2c_id_dw_i2c;

extern u16 g_chip_type;
extern struct mutex g_mutex;

/*=============================================================*/
/*GLOBAL VARIABLE DEFINITION*/
/*=============================================================*/

u32 g_is_in_mp_test = 0;
u32 g_is_dynamic_code = 1;
/*=============================================================*/
/*LOCAL VARIABLE DEFINITION*/
/*=============================================================*/

static u32 g_test_retry_count = CTP_MP_TEST_RETRY_COUNT;
static ito_test_mode_e g_ito_test_mode = 0;

static s32 g_ctp_mp_test_status = ITO_TEST_UNDER_TESTING;

static u32 g_test_fail_channel_count = 0;

static struct work_struct g_ctp_ito_test_work;
static struct workqueue_struct *g_ctp_mp_test_work_queue = NULL;

ItoTest_result_e g_ito_test_result = ITO_TEST_OK;

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
/* The below variable are defined for MSG22xx */
static u8 g_self_ic_test_fail_channel[SELF_IC_MAX_CHANNEL_NUM] = { 0 };

static s16 g_self_ic_raw_data1[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s16 g_self_ic_raw_data2[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s16 g_self_ic_raw_data3[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s16 g_self_ic_raw_data4[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s8 g_self_ic_data_flag1[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s8 g_self_ic_data_flag2[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s8 g_self_ic_data_flag3[SELF_IC_MAX_CHANNEL_NUM] = { 0 };
static s8 g_self_ic_data_flag4[SELF_IC_MAX_CHANNEL_NUM] = { 0 };

static u8 g_self_ici_to_test_key_num = 0;
static u8 g_self_ici_to_test_dummy_num = 0;
static u8 g_self_ici_to_triangle_num = 0;
static u8 g_self_ici_enable_2r = 0;

static u8 *g_self_ic_map1 = NULL;
static u8 *g_self_ic_map2 = NULL;
static u8 *g_self_ic_map3 = NULL;
static u8 *g_self_ic_map40_1 = NULL;
static u8 *g_self_ic_map40_2 = NULL;
static u8 *g_self_ic_map41_1 = NULL;
static u8 *g_self_ic_map41_2 = NULL;

static u8 *g_self_ic_short_map1 = NULL;
static u8 *g_self_ic_short_map2 = NULL;
static u8 *g_self_ic_short_map3 = NULL;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
static u8 *g_self_ic_map40_3 = NULL;
static u8 *g_self_ic_map40_4 = NULL;
static u8 *g_self_ic_map41_3 = NULL;
static u8 *g_self_ic_map41_4 = NULL;

static u8 *g_self_ic_short_map4 = NULL;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

static u8 g_is_old_firmware_version = 0;

/*g_msg22xx_open_riu1~g_msg22xx_open_riu3 are for MSG22XX*/
static u32 *g_msg22xx_open_riu1 = NULL;
static u32 *g_msg22xx_open_riu2 = NULL;
static u32 *g_msg22xx_open_riu3 = NULL;

/*g_msg22xx_short_riu1~g_msg22xx_short_riu4 are for MSG22XX*/
static u32 *g_msg22xx_short_riu1 = NULL;
static u32 *g_msg22xx_short_riu2 = NULL;
static u32 *g_msg22xx_short_riu3 = NULL;

/*g_msg22xx_open_sub_frame_num1~g_msg22xx_short_sub_frame_num4 are for MSG22XX*/
static u8 g_msg22xx_open_sub_frame_num1 = 0;
static u8 g_msg22xx_open_sub_frame_num2 = 0;
static u8 g_msg22xx_open_sub_frame_num3 = 0;
static u8 g_msg22xx_short_sub_frame_num1 = 0;
static u8 g_msg22xx_short_sub_frame_num2 = 0;
static u8 g_msg22xx_short_sub_frame_num3 = 0;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
static u32 *g_msg22xx_short_riu4 = NULL;
static u8 g_msg22xx_short_sub_frame_num4 = 0;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
/* The below variable are defined for MSG28xx */
test_scope_info_t g_test_scope_info = { 0 };    /*Used for MSG28xx */

static u16 g_mutual_ic_sense_line_num = 0;
static u16 g_mutual_ic_drive_line_num = 0;
static u16 g_mutual_ic_water_proof_num = 0;

static u8 g_mutual_ic_test_fail_channel[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };

static s32 g_mutual_ic_delta_c[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_mutual_ic_delta_cva[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_mutual_ic_result[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_mutual_ic_result_water[12] = { 0 };

/*static u8 g_mutua_iicMode[MUTUAL_IC_MAX_MUTUAL_NUM] = {0};*/
static s32 g_mutual_ic_sense_r[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_mutual_ic_drive_r[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };

u16 g_msg28xx_mux_mem_20_3e_0_settings[16] = { 0 };
u16 g_msg28xx_mux_mem_20_3e_1_settings[16] = { 0 };
u16 g_msg28xx_mux_mem_20_3e_2_settings[16] = { 0 };
u16 g_msg28xx_mux_mem_20_3e_3_settings[16] = { 0 };
u16 g_msg28xx_mux_mem_20_3e_4_settings[16] = { 0 };
u16 g_msg28xx_mux_mem_20_3e_5_settings[16] = { 0 };
u16 g_msg28xx_mux_mem_20_3e_6_settings[16] = { 0 };

static s32 g_mutual_ic_on_cell_open_test_result[2] = { 0 };
static s32 g_mutual_ic_on_cell_open_test_result_data[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_mutual_ic_on_cell_open_test_result_ratio_data[MUTUAL_IC_MAX_MUTUAL_NUM] =
    { 0 };
static s32 g_mutual_ic_on_cell_open_test_golden_channel[MUTUAL_IC_MAX_MUTUAL_NUM] =
    { 0 };
static s32 g_mutual_ic_on_cell_open_test_golden_channel_max[MUTUAL_IC_MAX_MUTUAL_NUM] =
    { 0 };
static s32 g_mutual_ic_on_cell_open_test_golden_channel_min[MUTUAL_IC_MAX_MUTUAL_NUM] =
    { 0 };

static u16 g_normal_test_fail_check_deltac[MUTUAL_IC_MAX_MUTUAL_NUM];
static u16 g_normal_test_fail_check_ratio1000[MUTUAL_IC_MAX_MUTUAL_NUM];
static s32 g_mutual_ic_on_cell_open_test_ratio1000[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_mutual_ic_on_cell_open_test_ratio_border1000[MUTUAL_IC_MAX_MUTUAL_NUM] =
    { 0 };
static s32 g_mutual_ic_on_cell_open_test_ratio_move1000[MUTUAL_IC_MAX_MUTUAL_NUM] =
    { 0 };
static s32 g_mutual_ic_on_cell_open_test_ratio_border_move1000[MUTUAL_IC_MAX_MUTUAL_NUM]
= { 0 };
static u16 g_normal_test_fail_check_deltac[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static u16 g_normal_test_fail_check_ratio[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_mutual_ic_delta_c2[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 };
static s32 g_ito_short_r_data[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_ito_short_result_data[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_ito_short_fail_channel[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_ic_pin_short_result_data[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static u16 g_ic_pin_short_sence_r[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_ic_pin_short_fail_channel[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_mutual_ic_deltac_water[12] = { 0 };
static s32 g_mutual_ic_grr[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
static s32 g_key_array[3] = { 0 };

/*Used for MSG28xx TP type definition*/
static u16 g_msg28xx_sense_num = 0;
static u16 g_msg28xx_drive_num = 0;
static u16 g_msg28xx_key_num = 0;
static u16 g_msg28xx_key_line = 0;
static u16 g_msg28xx_gr_num = 0;
static u16 g_msg28xx_csub_ref = 0;
static u16 g_msg28xx_cfb_ref = 0;
static u16 g_msg28xx_sense_mutual_scan_num = 0;
static u16 g_msg28xx_mutual_key = 0;
static u16 g_msg28xx_pattern_type = 0;
static u16 g_msg28xx_dc_range = 0;
static u16 g_msg28xx_dc_range_up = 0;
static u16 g_msg28xx_dc_ratio_1000 = 0;
static u16 g_msg28xx_dc_ratio_up_1000 = 0;
static u16 g_msg28xx_dc_border_ratio_1000 = 0;
static u16 g_msg28xx_dc_border_ratio_up_1000 = 0;
static u16 g_msg28xx_pattern_model = 0;
static u16 g_msg28xx_sensor_key_ch = 0;
static s16 g_ic_pin_short_check_fail = 0;
static s16 g_ito_short_checK_fail = 0;

static u16 g_msg28xx_short_n1_test_number = 0;
static u16 g_msg28xx_short_n2_test_number = 0;
static u16 g_msg28xx_short_s1_test_number = 0;
static u16 g_msg28xx_short_s2_test_number = 0;
static u16 g_msg28xx_short_test_5_type = 0;
static u16 g_msg28xx_short_x_test_number = 0;

static u16 *g_msg28xx_short_n1_test_pin = NULL;
static u16 *g_msg28xx_short_n1_mux_mem_20_3e = NULL;
static u16 *g_msg28xx_short_n2_test_pin = NULL;
static u16 *g_msg28xx_short_n2_muc_mem_20_3e = NULL;
static u16 *g_msg28xx_short_s1_test_pin = NULL;
static u16 *g_msg28xx_short_s1_mux_mem_20_3e = NULL;
static u16 *g_msg28xx_short_s2_test_pin = NULL;
static u16 *g_msg28xx_short_s2_mux_mem_20_3e = NULL;
static u16 *g_msg28xx_short_x_test_pin = NULL;
static u16 *g_msg28xx_short_x_mux_mem_20_3e = NULL;
static u16 *g_msg28xx_short_gr_test_pin = NULL;
static u16 *g_msg28xx_short_gr_mux_mem_20_3e = NULL;

static u16 *g_msg28xx_short_ic_pin_mux_mem_1_settings = NULL;
static u16 *g_msg28xx_short_ic_pin_mux_mem_2_settings = NULL;
static u16 *g_msg28xx_short_ic_pin_mux_mem_3_settings = NULL;
static u16 *g_msg28xx_short_ic_pin_mux_mem_4_settings = NULL;
static u16 *g_msg28xx_short_ic_pin_mux_mem_5_settings = NULL;

static u16 *g_msg28xx_pad_table_drive = NULL;
static u16 *g_msg28xx_pad_table_sense = NULL;
static u16 *g_msg28xx_pad_table_gr = NULL;
static u16 *g_msg28xx_sense_pad_pin_mapping = NULL;
static u8 *g_msg28xx_key_sen = NULL;
static u8 *g_msg28xx_key_drv = NULL;

static const u8 *g_msg28xx_map_va_mutual = NULL;
static u8 *g_msg28xx_AnaGen_Version = NULL;

static u16 g_msg28xx_tp_type = 0;
static s8 g_msg28xx_deep_stand_by = 0;
static u16 g_msg28xx_deep_stand_by_time_out = 0;
static u16 g_msg28xx_short_value = 0;
static u16 g_msg28xx_ic_pin_short = 0;
static u16 g_msg28xx_support_ic = 0;
static u16 g_msg28xx_open_mode = 0;
static u16 g_msg28xx_charge_pump = 0;
static u16 g_msg28xx_key_drv_o = 0;
static u16 g_msg28xx_sub_frame = 0;
static s32 g_msg28xx_fixed_carrier = 0;
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

/*=============================================================*/
/*EXTERN FUNCTION DECLARATION*/
/*=============================================================*/

/*=============================================================*/
/*LOCAL FUNCTION DECLARATION*/
/*=============================================================*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
static u16 drv_mp_test_ito_test_self_ic_get_tp_type(void);
static u16 drv_mp_test_ito_test_self_ic_choose_tp_type(void);
static void drv_mp_test_ito_test_self_ic_ana_sw_reset(void);
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static u8 drv_mp_test_mutual_ic_check_value_range(s32 n_value, s32 nMax, s32 nMin);
static void drv_mp_test_mutual_ic_debug_show_array(void *p_buf, u16 nLen,
                                             int n_dataType, int nCarry,
                                             int nChangeLine);
/*static void _DrvMpTestMutualICDebugShowS32Array(s32 *p_buf, u16 nRow, u16 nCol);*/
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

/*=============================================================*/
/*LOCAL FUNCTION DEFINITION*/
/*=============================================================*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
static u16 drv_mp_test_ito_test_msg22xx_get_data_out(s16 *p_raw_data, u16 nSubFrameNum)
{
    u32 i;
    u16 szRaw_data[SELF_IC_MAX_CHANNEL_NUM * 2] = { 0 };
    u16 nRegInt = 0x0000;
    u16 n_size = nSubFrameNum * 4;   /*1SF 4AFE */
    u8 szDbBusTx_data[8] = { 0 };
    u8 szDbBusRx_data[SELF_IC_MAX_CHANNEL_NUM * 4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    reg_set_16Bit_value_off(0x120A, BIT1);  /*one-shot mode */
    reg_set_16Bit_value_off(0x3D08, BIT8);  /*SELF_IC_FIQ_E_FRAME_READY_MASK */
    /*reg_set_16bit_value(0x130C, BIT15); //MCU read done */
    reg_set_16bit_value_on(0x120A, BIT0);   /*trigger one-shot */

    DBG(&g_i2c_client->dev, "polling start\n");

    /*Polling frame-ready interrupt status */
    i = 0;
    do {
        i++;
        nRegInt = reg_get16_bit_value(0x3D18); /*bank:intr_ctrl, addr:h000c */
        DBG("*** nRegInt=  %x ***\n", nRegInt);

        DBG("*** try count i=  %d ***\n", i);

        if (i > 20) {           /*sway 20151219 */
            error_flag = 0;
            return 0;
        }
    } while ((nRegInt & SELF_IC_FIQ_E_FRAME_READY_MASK) == 0x0000);

    DBG(&g_i2c_client->dev, "polling end\n");

    reg_set_16Bit_value_off(0x3D18, BIT8);  /*Clear frame-ready interrupt status */

    /*ReadRegBurst start */
    szDbBusTx_data[0] = 0x10;
    szDbBusTx_data[1] = 0x15;    /*bank:fout, addr:h0000 */
    szDbBusTx_data[2] = 0x00;
    iic_write_data(slave_i2c_id_db_bus, &szDbBusTx_data[0], 3);
    mdelay(20);
    iic_read_data(slave_i2c_id_db_bus, &szDbBusRx_data[0], (nSubFrameNum * 4 * 2));
    mdelay(100);

    for (i = 0; i < (nSubFrameNum * 4 * 2); i++) {
        DBG(&g_i2c_client->dev, "szDbBusRx_data[%d] = %d\n", i, szDbBusRx_data[i]);   /*add for debug */
    }
    /*ReadRegBurst stop */

    reg_set_16bit_value_on(0x3D08, BIT8);   /*SELF_IC_FIQ_E_FRAME_READY_MASK */

    for (i = 0; i < n_size; i++) {
        szRaw_data[i] = (szDbBusRx_data[2 * i + 1] << 8) | (szDbBusRx_data[2 * i]);
        p_raw_data[i] = (s16) szRaw_data[i];
    }

    return n_size;
}

static void drv_mp_test_ito_test_msg22xx_send_data_in(u8 nStep, u16 nRiuWriteLength)
{
    u32 i;
    u32 *pType = NULL;

    DBG(&g_i2c_client->dev, "*** %s() nStep = %d, nRiuWriteLength = %d ***\n",
        __func__, nStep, nRiuWriteLength);

    if (nStep == 1) {           /*39-1 */
        pType = &g_msg22xx_short_riu1[0];
    } else if (nStep == 2) {    /*39-2 */
        pType = &g_msg22xx_short_riu2[0];
    } else if (nStep == 3) {    /*39-3 */
        pType = &g_msg22xx_short_riu3[0];
    }
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
    else if (nStep == 0) {      /*39-4 (2R) */
        pType = &g_msg22xx_short_riu4[0];
    }
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
    else if (nStep == 4) {
        pType = &g_msg22xx_open_riu1[0];
    } else if (nStep == 5) {
        pType = &g_msg22xx_open_riu2[0];
    } else if (nStep == 6) {
        pType = &g_msg22xx_open_riu3[0];
    }

    reg_set_16bit_value_on(0x1192, BIT4);   /*force on enable sensor mux and csub sel sram clock */
    reg_set_16Bit_value_off(0x1192, BIT5);  /*mem clk sel */
    reg_set_16Bit_value_off(0x1100, BIT3);  /*tgen soft rst */
    reg_set_16bit_value(0x1180, MSG22XX_RIU_BASE_ADDR);    /*sensor mux sram read/write base address */
    reg_set_16bit_value(0x1182, nRiuWriteLength);  /*sensor mux sram write length */
    reg_set_16bit_value_on(0x1188, BIT0);   /*reg_mem0_w_start */

    for (i = MSG22XX_RIU_BASE_ADDR;
         i < (MSG22XX_RIU_BASE_ADDR + nRiuWriteLength); i++) {
        reg_set_16bit_value(0x118A, (u16) (pType[i]));
        reg_set_16bit_value(0x118C, (u16) (pType[i] >> 16));
    }
}

static void drv_mp_test_ito_test_msg22xx_setc(u8 nCSubStep)
{
    u32 i;
    u16 nRegVal;
    u32 nCSubNew;

    DBG(&g_i2c_client->dev, "*** %s() nCSubStep = %d ***\n", __func__,
        nCSubStep);

    nCSubNew = (nCSubStep > MSG22XX_CSUB_REF_MAX) ? MSG22XX_CSUB_REF_MAX : nCSubStep;   /*6 bits */
    nCSubNew =
        (nCSubNew | (nCSubNew << 8) | (nCSubNew << 16) | (nCSubNew << 24));

    nRegVal = reg_get16_bit_value(0x11C8); /*csub sel overwrite enable, will referance value of 11C0 */

    if (nRegVal == 0x000F) {
        reg_set_16bit_value(0x11C0, nCSubNew); /*prs 0 */
        reg_set_16bit_value(0x11C2, (nCSubNew >> 16)); /*prs 0 */
        reg_set_16bit_value(0x11C4, nCSubNew); /*prs 1 */
        reg_set_16bit_value(0x11C6, (nCSubNew >> 16)); /*prs 1 */
    } else {
        reg_set_16bit_value_on(0x1192, BIT4);   /*force on enable sensor mux  and csub sel sram clock */
        reg_set_16Bit_value_off(0x1192, BIT5);  /*mem clk sel */
        reg_set_16Bit_value_off(0x1100, BIT3);  /*tgen soft rst */
        reg_set_16bit_value(0x1184, 0);    /*n_addr */
        reg_set_16bit_value(0x1186, SELF_IC_MAX_CHANNEL_NUM);  /*nLen */
        reg_set_16bit_value_on(0x1188, BIT2);   /*reg_mem0_w_start */

        for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
            reg_set_16bit_value(0x118E, nCSubNew);
            reg_set_16bit_value(0x1190, (nCSubNew >> 16));
        }
    }
}

static void drv_mp_test_ito_test_msg22xx_register_reset(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    reg_set_16bit_value_on(0x1102, (BIT3 | BIT4 | BIT5 | BIT6 | BIT7));
    reg_set_16Bit_value_off(0x1130, (BIT0 | BIT1 | BIT2 | BIT3 | BIT8));

    reg_set_16bit_value_on(0x1312, BIT15);
    reg_set_16Bit_value_off(0x1312, BIT13);

    reg_mask_16bit_value(0x1250, 0x007F, ((5 << 0) & 0x007F), ADDRESS_MODE_8BIT);

    reg_mask_16bit_value(0x1250, 0x7F00, ((1 << 8) & 0x7F00), ADDRESS_MODE_8BIT);

    reg_set_16Bit_value_off(0x1250, 0x8080);

    reg_set_16Bit_value_off(0x1312, (BIT12 | BIT14));

    reg_set_16bit_value_on(0x1300, BIT15);
    reg_set_16Bit_value_off(0x1300, (BIT10 | BIT11 | BIT12 | BIT13 | BIT14));

    reg_set_16bit_value_on(0x1130, BIT9);

    reg_set_16bit_value_on(0x1318, (BIT12 | BIT13));

    reg_set_16bit_value(0x121A, ((8 - 1) & 0x01FF));   /*sampling number group A */
    reg_set_16bit_value(0x121C, ((8 - 1) & 0x01FF));   /*sampling number group B */

    reg_mask_16bit_value(0x131A, 0x3FFF, 0x2000, ADDRESS_MODE_8BIT);

    reg_mask_16bit_value(0x131C, (BIT8 | BIT9 | BIT10), (2 << 8),
                      ADDRESS_MODE_8BIT);

    reg_set_16Bit_value_off(0x1174, 0x0F00);

    reg_set_16Bit_value_off(0x1240, 0xFFFF);    /*mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense */
    reg_set_16Bit_value_off(0x1242, 0x00FF);    /*mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense */

    reg_set_16bit_value_on(0x1212, 0xFFFF); /*timing group A/B selection for total 24 subframes, 0: group A, 1: group B */
    reg_set_16bit_value_on(0x1214, 0x00FF); /*timing group A/B selection for total 24 subframes, 0: group A, 1: group B */

    reg_set_16bit_value_on(0x121E, 0xFFFF); /*sample number group A/B selection for total 24 subframes, 0: group A, 1: group B */
    reg_set_16bit_value_on(0x1220, 0x00FF); /*sample number group A/B selection for total 24 subframes, 0: group A, 1: group B */

    reg_set_16Bit_value_off(0x120E, 0xFFFF);    /*noise sense mode selection for total 24 subframes */
    reg_set_16Bit_value_off(0x1210, 0x00FF);    /*noise sense mode selection for total 24 subframes */

    reg_set_16bit_value(0x128C, 0x0F); /*ADC afe gain correction bypass */
    reg_set_16Bit_value_off(0x1104, BIT12);
}

static void drv_mp_test_ito_test_msg22xx_register_reset_patch(void)
{
    DBG(&g_i2c_client->dev, "*** %s() *** g_is_old_firmware_version = %d\n",
        __func__, g_is_old_firmware_version);

    if (g_is_old_firmware_version == 0) {  /*Only need to patch register reset for MSG22XX firmware older than V01.005.02; */
        return;
    }

    reg_mask_16bit_value(0x1101, 0xFFFF, 0x501E, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1102, 0x7FFF, 0x06FF, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1104, 0x0FFF, 0x0772, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1105, 0x0FFF, 0x0770, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1106, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1107, 0x0003, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1108, 0x0073, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1109, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x110A, 0x7FFF, 0x1087, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x110E, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x110F, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1117, 0xFF0F, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1118, 0xFFFF, 0x0200, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1119, 0x00FF, 0x000E, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x111E, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x111F, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1133, 0x000F, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x113A, 0x0F37, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x113B, 0x0077, 0x0077, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x113C, 0xFF00, 0xA000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x113D, 0x0077, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x113E, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);

    reg_mask_16bit_value(0x1204, 0x0006, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1205, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1207, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1208, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1209, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x120A, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x120B, 0x003F, 0x002E, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x120D, 0x001F, 0x0005, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x120E, 0x001F, 0x0005, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x120F, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1210, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1211, 0x0FFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1212, 0x1F87, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1213, 0x0F7F, 0x0014, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1214, 0xFF9F, 0x090A, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1215, 0x0F7F, 0x0F0C, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1216, 0x0FFF, 0x0700, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1217, 0xFF1F, 0x5C0A, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1218, 0x1F7F, 0x0A14, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1219, 0xFFFF, 0x218C, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x121E, 0x1F1F, 0x0712, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x121F, 0x3F3F, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1220, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1221, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1223, 0x3F3F, 0x0002, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1224, 0x003F, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1228, 0xFFFF, 0x8183, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x122D, 0x0001, 0x0001, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1250, 0xFFFF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1251, 0x00FF, 0x0000, ADDRESS_MODE_16BIT);
    reg_mask_16bit_value(0x1270, 0x0003, 0x0003, ADDRESS_MODE_16BIT);

    reg_set_16bit_value_by_address_mode(0x115C, 0x0000, ADDRESS_MODE_16BIT);  /*sensor ov enable */
    reg_set_16bit_value_by_address_mode(0x115D, 0x0000, ADDRESS_MODE_16BIT);  /*sensor ov enable */
    reg_set_16bit_value_by_address_mode(0x115E, 0x0000, ADDRESS_MODE_16BIT);  /*sensor and gr ov enable */

    reg_set_16bit_value_by_address_mode(0x124E, 0x000F, ADDRESS_MODE_16BIT);  /*bypass mode */
}

static void drv_mp_test_ito_test_msg22xx_get_charge_dump_time(u16 nMode,
                                                      u16 *pChargeTime,
                                                      u16 *pDumpTime)
{
    u16 nChargeTime = 0, nDumpTime = 0;
    u16 nMinChargeTime = 0xFFFF, nMinDumpTime = 0xFFFF, nMaxChargeTime =
        0x0000, nMaxDumpTime = 0x0000;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    nChargeTime = reg_get16_bit_value(0x1226);
    nDumpTime = reg_get16_bit_value(0x122A);

    if (nMinChargeTime > nChargeTime) {
        nMinChargeTime = nChargeTime;
    }

    if (nMaxChargeTime < nChargeTime) {
        nMaxChargeTime = nChargeTime;
    }

    if (nMinDumpTime > nDumpTime) {
        nMinDumpTime = nDumpTime;
    }

    if (nMaxDumpTime < nDumpTime) {
        nMaxDumpTime = nDumpTime;
    }

    DBG(&g_i2c_client->dev, "nChargeTime = %d, nDumpTime = %d\n", nChargeTime,
        nDumpTime);

    if (nMode == 1) {
        *pChargeTime = nMaxChargeTime;
        *pDumpTime = nMaxDumpTime;
    } else {
        *pChargeTime = nMinChargeTime;
        *pDumpTime = nMinDumpTime;
    }
}

static void drv_mp_test_ito_open_test_msg22xx_first(u8 nItemId, s16 *p_raw_data,
                                              s8 *p_dataFlag)
{
    u32 i;
    s16 szTmpRaw_data[SELF_IC_MAX_CHANNEL_NUM * 2] = { 0 };
    u16 nSubFrameNum = 0;
    u16 nChargeTime, nDumpTime;
    u8 *pMapping = NULL;

    DBG(&g_i2c_client->dev, "*** %s() nItemId = %d ***\n", __func__, nItemId);

    if (nItemId == 40) {
        /*Stop cpu */
        reg_set_16bit_value(0x0FE6, 0x0001);   /*bank:mheg5, addr:h0073 */

        drv_mp_test_ito_test_msg22xx_register_reset_patch();
        drv_mp_test_ito_test_msg22xx_register_reset();
    }

    switch (nItemId) {
    case 40:
        pMapping = &g_self_ic_map1[0];
        nSubFrameNum = g_msg22xx_open_sub_frame_num1;
        break;
    case 41:
        pMapping = &g_self_ic_map2[0];
        nSubFrameNum = g_msg22xx_open_sub_frame_num2;
        break;
    case 42:
        pMapping = &g_self_ic_map3[0];
        nSubFrameNum = g_msg22xx_open_sub_frame_num3;
        break;
    }

    if (nSubFrameNum > 24) {    /*SELF_IC_MAX_CHANNEL_NUM/2 */
        nSubFrameNum = 24;
    }

    drv_mp_test_ito_test_msg22xx_send_data_in(nItemId - 36, nSubFrameNum * 6);
    reg_set_16bit_value(0x1216, (nSubFrameNum - 1) << 1);  /*subframe numbers, 0:1subframe, 1:2subframe */

    if (nItemId == 40) {
        if (1) {
            reg_set_16bit_value(0x1110, 0x0060);   /*2.4V -> 1.2V */
        } else {
            reg_set_16bit_value(0x1110, 0x0020);   /*3.0V -> 0.6V */
        }

        reg_set_16bit_value(0x11C8, 0x000F);   /*csub sel overwrite enable, will referance value of 11C0 */
        reg_set_16bit_value(0x1174, 0x0F06);   /*1 : sel idel driver for sensor pad that connected to AFE */
        reg_set_16bit_value(0x1208, 0x0006);   /*PRS1 */
        reg_set_16bit_value(0x1240, 0xFFFF);   /*mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense */
        reg_set_16bit_value(0x1242, 0x00FF);   /*mutual cap mode selection for total 24 subframes, 0: normal sense, 1: mutual cap sense */

        drv_mp_test_ito_test_msg22xx_get_charge_dump_time(1, &nChargeTime, &nDumpTime);

        reg_set_16bit_value(0x1226, nChargeTime);
        reg_set_16bit_value(0x122A, nDumpTime);

        drv_mp_test_ito_test_msg22xx_setc(MSG22XX_CSUB_REF);
        drv_mp_test_ito_test_self_ic_ana_sw_reset();
    }

    drv_mp_test_ito_test_msg22xx_get_data_out(szTmpRaw_data, nSubFrameNum);

    for (i = 0; i < nSubFrameNum; i++) {
/*DBG(&g_i2c_client->dev, "szTmpRaw_data[%d * 4] >> 3 = %d\n", i, szTmpRaw_data[i * 4] >> 3); // add for debug*/
/*DBG(&g_i2c_client->dev, "pMapping[%d] = %d\n", i, pMapping[i]); // add for debug*/

        p_raw_data[pMapping[i]] = (szTmpRaw_data[i * 4] >> 3); /*Filter to ADC */
        p_dataFlag[pMapping[i]] = 1;
    }
}

static void drv_mp_test_ito_open_test_msg22xx_first(u8 nItemId, s16 *p_raw_data,
                                               s8 *p_dataFlag)
{
    u32 i, j;
    s16 szIIR1[MSG22XX_MAX_SUBFRAME_NUM * MSG22XX_MAX_AFE_NUM] = { 32767 };
    s16 szIIR2[MSG22XX_MAX_SUBFRAME_NUM * MSG22XX_MAX_AFE_NUM] = { 32767 };
    s16 szIIRTmp[MSG22XX_MAX_SUBFRAME_NUM * MSG22XX_MAX_AFE_NUM] = { 32767 };
    u16 nSensorNum = 0, nSubFrameNum = 0;
    u8 *pMapping = NULL;

    DBG(&g_i2c_client->dev, "*** %s() nItemId = %d ***\n", __func__, nItemId);

    if ((nItemId == 0 && g_self_ici_enable_2r == 1) ||
        (nItemId == 1 && g_self_ici_enable_2r == 0)) {
        /*Stop cpu */
        reg_set_16bit_value(0x0FE6, 0x0001);   /*bank:mheg5, addr:h0073 */

        drv_mp_test_ito_test_msg22xx_register_reset_patch();
        drv_mp_test_ito_test_msg22xx_register_reset();
    }

    switch (nItemId) {
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
    case 0:                    /*39-4 (2R) */
        pMapping = &g_self_ic_short_map4[0];
        nSensorNum = g_msg22xx_short_sub_frame_num4;
        break;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
    case 1:                    /*39-1 */
        pMapping = &g_self_ic_short_map1[0];
        nSensorNum = g_msg22xx_short_sub_frame_num1;
        break;
    case 2:                    /*39-2 */
        pMapping = &g_self_ic_short_map2[0];
        nSensorNum = g_msg22xx_short_sub_frame_num2;
        break;
    case 3:                    /*39-3 */
        pMapping = &g_self_ic_short_map3[0];
        nSensorNum = g_msg22xx_short_sub_frame_num3;
        break;
    }

    if (nSensorNum > 24) {      /*SELF_IC_MAX_CHANNEL_NUM/2 */
        nSubFrameNum = 24;
    } else {
        nSubFrameNum = nSensorNum;
    }

    drv_mp_test_ito_test_msg22xx_send_data_in(nItemId, nSubFrameNum * 6);
    reg_set_16bit_value(0x1216, (nSubFrameNum - 1) << 1);  /*subframe numbers, 0:1subframe, 1:2subframe */

    if ((nItemId == 0 && g_self_ici_enable_2r == 1) ||
        (nItemId == 1 && g_self_ici_enable_2r == 0)) {
        reg_set_16bit_value(0x1110, 0x0030);   /*[6:4}  011 : 2.1V -> 1.5V */
/*reg_set_16bit_value(0x1110, 0x0060); // 2.4V -> 1.2V*/
        reg_set_16bit_value(0x11C8, 0x000F);   /*csub sel overwrite enable, will referance value of 11C0 */
        reg_set_16bit_value(0x1174, 0x0000);   /*[11:8] 000 : sel active driver for sensor pad that connected to AFE */
        reg_set_16bit_value(0x1208, 0x0006);   /*PRS 1 */
        reg_set_16bit_value_on(0x1104, BIT14);  /*R mode */
        reg_set_16bit_value(0x1176, 0x0000);   /*CFB 10p */

        drv_mp_test_ito_test_msg22xx_setc(MSG22XX_CSUB_REF);

        reg_mask_16bit_value(0x1213, 0x007F, 0x003B, ADDRESS_MODE_16BIT);  /*Charge 20ns (group A) */
        reg_mask_16bit_value(0x1218, 0x007F, 0x003B, ADDRESS_MODE_16BIT);  /*Charge 20ns (group B) */
    }

    reg_mask_16bit_value(0x1215, 0x007F, 0x000B, ADDRESS_MODE_16BIT);  /*Dump 4ns (group A) */
    reg_mask_16bit_value(0x1219, 0x007F, 0x000B, ADDRESS_MODE_16BIT);  /*Dump 4ns (group B) */

    drv_mp_test_ito_test_self_ic_ana_sw_reset();
    drv_mp_test_ito_test_msg22xx_get_data_out(szIIRTmp, nSubFrameNum);

    if (nSensorNum > MSG22XX_MAX_SUBFRAME_NUM) {
        j = 0;

        for (i = 0; i < MSG22XX_MAX_SUBFRAME_NUM; i++) {
            szIIR1[j] = (szIIRTmp[i * 4] >> 3);
            j++;

            if ((nSensorNum - MSG22XX_MAX_SUBFRAME_NUM) > i) {
                szIIR1[j] = (szIIRTmp[i * 4 + 1] >> 3);
                j++;
            }
        }
    } else {
        for (i = 0; i < nSensorNum; i++) {
            szIIR1[i] = (szIIRTmp[i * 4 + 1] >> 3);
        }
    }

    reg_mask_16bit_value(0x1215, 0x007F, 0x0017, ADDRESS_MODE_16BIT);  /*Dump 8ns (group A) */
    reg_mask_16bit_value(0x1219, 0x007F, 0x0017, ADDRESS_MODE_16BIT);  /*Dump 8ns (group B) */

    drv_mp_test_ito_test_self_ic_ana_sw_reset();
    drv_mp_test_ito_test_msg22xx_get_data_out(szIIRTmp, nSubFrameNum);

    if (nSensorNum > MSG22XX_MAX_SUBFRAME_NUM) {
        j = 0;

        for (i = 0; i < MSG22XX_MAX_SUBFRAME_NUM; i++) {
            szIIR2[j] = (szIIRTmp[i * 4] >> 3);
            j++;

            if ((nSensorNum - MSG22XX_MAX_SUBFRAME_NUM) > i) {
                szIIR2[j] = (szIIRTmp[i * 4 + 1] >> 3);
                j++;
            }
        }
    } else {
        for (i = 0; i < nSensorNum; i++) {
            szIIR2[i] = (szIIRTmp[i * 4 + 1] >> 3);
        }
    }

    for (i = 0; i < nSensorNum; i++) {
        if ((abs(szIIR1[i]) > 4000) || (abs(szIIR2[i]) > 4000)) {
            p_raw_data[pMapping[i]] = 8192;
        } else {
            p_raw_data[pMapping[i]] = ((szIIR2[i] - szIIR1[i]) * 4);
        }
        p_dataFlag[pMapping[i]] = 1;

/*DBG(&g_i2c_client->dev, "szIIR1[%d] = %d, szIIR2[%d] = %d\n", i, szIIR1[i], i, szIIR2[i]); // add for debug*/
/*DBG(&g_i2c_client->dev, "p_raw_data[%d] = %d\n", pMapping[i], p_raw_data[pMapping[i]]); // add for debug*/
/*DBG(&g_i2c_client->dev, "pMapping[%d] = %d\n", i, pMapping[i]); // add for debug*/
    }
}

static u8 drv_mp_test_msg22xx_check_firmware_version(void)
{                               /*Only MSG22XX support platform firmware version */
    u32 i;
    s32 nDiff;
    u16 nReg_data1, nReg_data2;
    u8 szDbBusRx_data[12] = { 0 };
    u8 szCompareFwVersion[10] = { 0x56, 0x30, 0x31, 0x2E, 0x30, 0x30, 0x35, 0x2E, 0x30, 0x32 }; /*{'V', '0', '1', '.', '0', '0', '5', '.', '0', '2'} */
    u8 nIsOldFirmwareVersion = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    mutex_lock(&g_mutex);

    drv_touch_device_hw_reset();

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop mcu */
    reg_set_l_byte_value(0x0FE6, 0x01);

    /*RIU password */
    reg_set_16bit_value(0x161A, 0xABBA);

    reg_set_16bit_value(0x1600, 0xC1F2);   /*Set start address for platform firmware version on info block(Actually, start reading from 0xC1F0) */

    /*Enable burst mode */
    reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));

    for (i = 0; i < 3; i++) {
        reg_set_l_byte_value(0x160E, 0x01);

        nReg_data1 = reg_get16_bit_value(0x1604);
        nReg_data2 = reg_get16_bit_value(0x1606);

        szDbBusRx_data[i * 4 + 0] = (nReg_data1 & 0xFF);
        szDbBusRx_data[i * 4 + 1] = ((nReg_data1 >> 8) & 0xFF);

        DBG(&g_i2c_client->dev, "szDbBusRx_data[%d] = 0x%x , %c\n", i * 4 + 0, szDbBusRx_data[i * 4 + 0], szDbBusRx_data[i * 4 + 0]); /*add for debug */
        DBG(&g_i2c_client->dev, "szDbBusRx_data[%d] = 0x%x , %c\n", i * 4 + 1, szDbBusRx_data[i * 4 + 1], szDbBusRx_data[i * 4 + 1]); /*add for debug */

        szDbBusRx_data[i * 4 + 2] = (nReg_data2 & 0xFF);
        szDbBusRx_data[i * 4 + 3] = ((nReg_data2 >> 8) & 0xFF);

        DBG(&g_i2c_client->dev, "szDbBusRx_data[%d] = 0x%x , %c\n", i * 4 + 2, szDbBusRx_data[i * 4 + 2], szDbBusRx_data[i * 4 + 2]); /*add for debug */
        DBG(&g_i2c_client->dev, "szDbBusRx_data[%d] = 0x%x , %c\n", i * 4 + 3, szDbBusRx_data[i * 4 + 3], szDbBusRx_data[i * 4 + 3]); /*add for debug */
    }

    /*Clear burst mode */
    reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));

    reg_set_16bit_value(0x1600, 0x0000);

    /*Clear RIU password */
    reg_set_16bit_value(0x161A, 0x0000);

    for (i = 0; i < 10; i++) {
        nDiff = szDbBusRx_data[2 + i] - szCompareFwVersion[i];

        DBG(&g_i2c_client->dev, "i = %d, nDiff = %d\n", i, nDiff);

        if (nDiff < 0) {
            nIsOldFirmwareVersion = 1;
            break;
        } else if (nDiff > 0) {
            nIsOldFirmwareVersion = 0;
            break;
        }
    }

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    drv_touch_device_hw_reset();

    mutex_unlock(&g_mutex);

    return nIsOldFirmwareVersion;
}

static u16 drv_mp_test_ito_test_self_ic_get_tp_type(void)
{
    u16 nMajor = 0, nMinor = 0;

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

        /*RIU password */
        reg_set_16bit_value(0x161A, 0xABBA);

        reg_set_16bit_value(0x1600, 0xBFF4);   /*Set start address for customer firmware version on main block */

        /*Enable burst mode */
/*reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));*/

        reg_set_l_byte_value(0x160E, 0x01);

        nReg_data1 = reg_get16_bit_value(0x1604);
        nReg_data2 = reg_get16_bit_value(0x1606);

        nMajor = (((nReg_data1 >> 8) & 0xFF) << 8) + (nReg_data1 & 0xFF);
        nMinor = (((nReg_data2 >> 8) & 0xFF) << 8) + (nReg_data2 & 0xFF);

        /*Clear burst mode */
/*reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));*/

        reg_set_16bit_value(0x1600, 0x0000);

        /*Clear RIU password */
        reg_set_16bit_value(0x161A, 0x0000);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        drv_touch_device_hw_reset();
        mdelay(100);
    }

    DBG(&g_i2c_client->dev, "*** major = %d ***\n", nMajor);
    DBG(&g_i2c_client->dev, "*** minor = %d ***\n", nMinor);

    return nMajor;
}

static u16 drv_mp_test_ito_test_self_ic_choose_tp_type(void)
{
    u16 nTpType = 0;
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        /*g_msg22xx_open_riu1~g_msg22xx_open_riu3 are for MSG22XX*/
        g_msg22xx_open_riu1 = NULL;
        g_msg22xx_open_riu2 = NULL;
        g_msg22xx_open_riu3 = NULL;

        /*g_msg22xx_short_riu1~g_msg22xx_short_riu4 are for MSG22XX*/
        g_msg22xx_short_riu1 = NULL;
        g_msg22xx_short_riu2 = NULL;
        g_msg22xx_short_riu3 = NULL;

        g_msg22xx_open_sub_frame_num1 = 0;
        g_msg22xx_open_sub_frame_num2 = 0;
        g_msg22xx_open_sub_frame_num3 = 0;
        g_msg22xx_short_sub_frame_num1 = 0;
        g_msg22xx_short_sub_frame_num2 = 0;
        g_msg22xx_short_sub_frame_num3 = 0;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
        g_msg22xx_short_riu4 = NULL;
        g_msg22xx_short_sub_frame_num4 = 0;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
    }

    g_self_ic_map1 = NULL;
    g_self_ic_map2 = NULL;
    g_self_ic_map3 = NULL;
    g_self_ic_map40_1 = NULL;
    g_self_ic_map40_2 = NULL;
    g_self_ic_map41_1 = NULL;
    g_self_ic_map41_2 = NULL;

    g_self_ic_short_map1 = NULL;
    g_self_ic_short_map2 = NULL;
    g_self_ic_short_map3 = NULL;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
    g_self_ic_map40_3 = NULL;
    g_self_ic_map40_4 = NULL;
    g_self_ic_map41_3 = NULL;
    g_self_ic_map41_4 = NULL;

    g_self_ic_short_map4 = NULL;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

    g_self_ici_to_test_key_num = 0;
    g_self_ici_to_test_dummy_num = 0;
    g_self_ici_to_triangle_num = 0;
    g_self_ici_enable_2r = 0;

    for (i = 0; i < 10; i++) {
        nTpType = drv_mp_test_ito_test_self_ic_get_tp_type();
        DBG(&g_i2c_client->dev, "nTpType = %d, i = %d\n", nTpType, i);

        if (TP_TYPE_X == nTpType || TP_TYPE_Y == nTpType) { /*Modify. */
            break;
        } else if (i < 5) {
            mdelay(100);
        } else {
            drv_touch_device_hw_reset();
        }
    }

    if (TP_TYPE_X == nTpType) { /*Modify. */
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            g_msg22xx_open_riu1 = MSG22XX_open_1_X;
            g_msg22xx_open_riu2 = MSG22XX_open_2_X;
            g_msg22xx_open_riu3 = MSG22XX_open_3_X;

            g_msg22xx_short_riu1 = MSG22XX_short_1_X;
            g_msg22xx_short_riu2 = MSG22XX_short_2_X;
            g_msg22xx_short_riu3 = MSG22XX_short_3_X;

            g_msg22xx_open_sub_frame_num1 = MSG22XX_NUM_OPEN_1_SENSOR_X;
            g_msg22xx_open_sub_frame_num2 = MSG22XX_NUM_OPEN_2_SENSOR_X;
            g_msg22xx_open_sub_frame_num3 = MSG22XX_NUM_OPEN_3_SENSOR_X;
            g_msg22xx_short_sub_frame_num1 = MSG22XX_NUM_SHORT_1_SENSOR_X;
            g_msg22xx_short_sub_frame_num2 = MSG22XX_NUM_SHORT_2_SENSOR_X;
            g_msg22xx_short_sub_frame_num3 = MSG22XX_NUM_SHORT_3_SENSOR_X;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
            g_msg22xx_short_riu4 = MSG22XX_short_4_X;
            g_msg22xx_short_sub_frame_num4 = MSG22XX_NUM_SHORT_4_SENSOR_X;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

            g_self_ic_map1 = MSG22XX_MAP1_X;
            g_self_ic_map2 = MSG22XX_MAP2_X;
            g_self_ic_map3 = MSG22XX_MAP3_X;
            g_self_ic_map40_1 = MSG22XX_MAP40_1_X;
            g_self_ic_map40_2 = MSG22XX_MAP40_2_X;
            g_self_ic_map41_1 = MSG22XX_MAP41_1_X;
            g_self_ic_map41_2 = MSG22XX_MAP41_2_X;

            g_self_ic_short_map1 = MSG22XX_SHORT_MAP1_X;
            g_self_ic_short_map2 = MSG22XX_SHORT_MAP2_X;
            g_self_ic_short_map3 = MSG22XX_SHORT_MAP3_X;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
            g_self_ic_map40_3 = MSG22XX_MAP40_3_X;
            g_self_ic_map40_4 = MSG22XX_MAP40_4_X;
            g_self_ic_map41_3 = MSG22XX_MAP41_3_X;
            g_self_ic_map41_4 = MSG22XX_MAP41_4_X;

            g_self_ic_short_map4 = MSG22XX_SHORT_MAP4_X;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

            g_self_ici_to_test_key_num = MSG22XX_NUM_KEY_X;
            g_self_ici_to_test_dummy_num = MSG22XX_NUM_DUMMY_X;
            g_self_ici_to_triangle_num = MSG22XX_NUM_SENSOR_X;
            g_self_ici_enable_2r = MSG22XX_ENABLE_2R_X;
        }
    } else if (TP_TYPE_Y == nTpType) {  /*Modify. */
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            g_msg22xx_open_riu1 = MSG22XX_open_1_Y;
            g_msg22xx_open_riu2 = MSG22XX_open_2_Y;
            g_msg22xx_open_riu3 = MSG22XX_open_3_Y;

            g_msg22xx_short_riu1 = MSG22XX_short_1_Y;
            g_msg22xx_short_riu2 = MSG22XX_short_2_Y;
            g_msg22xx_short_riu3 = MSG22XX_short_3_Y;

            g_msg22xx_open_sub_frame_num1 = MSG22XX_NUM_OPEN_1_SENSOR_Y;
            g_msg22xx_open_sub_frame_num2 = MSG22XX_NUM_OPEN_2_SENSOR_Y;
            g_msg22xx_open_sub_frame_num3 = MSG22XX_NUM_OPEN_3_SENSOR_Y;
            g_msg22xx_short_sub_frame_num1 = MSG22XX_NUM_SHORT_1_SENSOR_Y;
            g_msg22xx_short_sub_frame_num2 = MSG22XX_NUM_SHORT_2_SENSOR_Y;
            g_msg22xx_short_sub_frame_num3 = MSG22XX_NUM_SHORT_3_SENSOR_Y;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
            g_msg22xx_short_riu4 = MSG22XX_short_4_Y;
            g_msg22xx_short_sub_frame_num4 = MSG22XX_NUM_SHORT_4_SENSOR_Y;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

            g_self_ic_map1 = MSG22XX_MAP1_Y;
            g_self_ic_map2 = MSG22XX_MAP2_Y;
            g_self_ic_map3 = MSG22XX_MAP3_Y;
            g_self_ic_map40_1 = MSG22XX_MAP40_1_Y;
            g_self_ic_map40_2 = MSG22XX_MAP40_2_Y;
            g_self_ic_map41_1 = MSG22XX_MAP41_1_Y;
            g_self_ic_map41_2 = MSG22XX_MAP41_2_Y;

            g_self_ic_short_map1 = MSG22XX_SHORT_MAP1_Y;
            g_self_ic_short_map2 = MSG22XX_SHORT_MAP2_Y;
            g_self_ic_short_map3 = MSG22XX_SHORT_MAP3_Y;

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
            g_self_ic_map40_3 = MSG22XX_MAP40_3_Y;
            g_self_ic_map40_4 = MSG22XX_MAP40_4_Y;
            g_self_ic_map41_3 = MSG22XX_MAP41_3_Y;
            g_self_ic_map41_4 = MSG22XX_MAP41_4_Y;

            g_self_ic_short_map4 = MSG22XX_SHORT_MAP4_Y;
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

            g_self_ici_to_test_key_num = MSG22XX_NUM_KEY_Y;
            g_self_ici_to_test_dummy_num = MSG22XX_NUM_DUMMY_Y;
            g_self_ici_to_triangle_num = MSG22XX_NUM_SENSOR_Y;
            g_self_ici_enable_2r = MSG22XX_ENABLE_2R_Y;
        }
    } else {
        nTpType = 0;
    }

    return nTpType;
}

static void drv_mp_test_ito_test_self_ic_ana_sw_reset(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    reg_set_16bit_value(0x1100, 0xFFFF);   /*reset ANA - analog */

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        reg_set_16bit_value_on(0x130C, BIT1);   /*reset Filter - digital */
    }

    reg_set_16bit_value(0x1100, 0x0000);
    mdelay(15);
}

static ItoTest_result_e drv_mp_test_ito_open_test_selfIc_second(u8 nItemId)
{
    ItoTest_result_e n_ret_val = ITO_TEST_OK;
    u32 i;
    s32 nTmpRaw_dataJg1 = 0;
    s32 nTmpRaw_dataJg2 = 0;
    s32 nTmpJgAvgThMax1 = 0;
    s32 nTmpJgAvgThMin1 = 0;
    s32 nTmpJgAvgThMax2 = 0;
    s32 nTmpJgAvgThMin2 = 0;

    DBG(&g_i2c_client->dev, "*** %s() nItemId = %d ***\n", __func__, nItemId);

    if (nItemId == 40) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 2) - 2; i++) {
            nTmpRaw_dataJg1 += g_self_ic_raw_data1[g_self_ic_map40_1[i]];
        }

        for (i = 0; i < 2; i++) {
            nTmpRaw_dataJg2 += g_self_ic_raw_data1[g_self_ic_map40_2[i]];
        }
    } else if (nItemId == 41) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 2) - 2; i++) {
            nTmpRaw_dataJg1 += g_self_ic_raw_data2[g_self_ic_map41_1[i]];
        }

        for (i = 0; i < 2; i++) {
            nTmpRaw_dataJg2 += g_self_ic_raw_data2[g_self_ic_map41_2[i]];
        }
    }

    nTmpJgAvgThMax1 =
        (nTmpRaw_dataJg1 / ((g_self_ici_to_triangle_num / 2) - 2)) * (100 +
                                                                     SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD)
        / 100;
    nTmpJgAvgThMin1 =
        (nTmpRaw_dataJg1 / ((g_self_ici_to_triangle_num / 2) - 2)) * (100 -
                                                                     SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD)
        / 100;
    nTmpJgAvgThMax2 =
        (nTmpRaw_dataJg2 / 2) * (100 +
                                SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin2 =
        (nTmpRaw_dataJg2 / 2) * (100 -
                                SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;

    DBG(&g_i2c_client->dev,
        "nItemId = %d, nTmpRaw_dataJg1 = %d, nTmpJgAvgThMax1 = %d, nTmpJgAvgThMin1 = %d, nTmpRaw_dataJg2 = %d, nTmpJgAvgThMax2 = %d, nTmpJgAvgThMin2 = %d\n",
        nItemId, nTmpRaw_dataJg1, nTmpJgAvgThMax1, nTmpJgAvgThMin1,
        nTmpRaw_dataJg2, nTmpJgAvgThMax2, nTmpJgAvgThMin2);

    if (nItemId == 40) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 2) - 2; i++) {
            if (g_self_ic_raw_data1[g_self_ic_map40_1[i]] > nTmpJgAvgThMax1 ||
                g_self_ic_raw_data1[g_self_ic_map40_1[i]] < nTmpJgAvgThMin1) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map40_1[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (g_self_ic_raw_data1[g_self_ic_map40_2[i]] > nTmpJgAvgThMax2 ||
                g_self_ic_raw_data1[g_self_ic_map40_2[i]] < nTmpJgAvgThMin2) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map40_2[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    } else if (nItemId == 41) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 2) - 2; i++) {
            if (g_self_ic_raw_data2[g_self_ic_map41_1[i]] > nTmpJgAvgThMax1 ||
                g_self_ic_raw_data2[g_self_ic_map41_1[i]] < nTmpJgAvgThMin1) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map41_1[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (g_self_ic_raw_data2[g_self_ic_map41_2[i]] > nTmpJgAvgThMax2 ||
                g_self_ic_raw_data2[g_self_ic_map41_2[i]] < nTmpJgAvgThMin2) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map41_2[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    }

    return n_ret_val;
}

#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
static ItoTest_result_e drv_mp_test_ito_open_test_selfIc_second2r(u8 nItemId)
{
    ItoTest_result_e n_ret_val = ITO_TEST_OK;
    u32 i;
    s32 nTmpRaw_dataJg1 = 0;
    s32 nTmpRaw_dataJg2 = 0;
    s32 nTmpRaw_dataJg3 = 0;
    s32 nTmpRaw_dataJg4 = 0;
    s32 nTmpJgAvgThMax1 = 0;
    s32 nTmpJgAvgThMin1 = 0;
    s32 nTmpJgAvgThMax2 = 0;
    s32 nTmpJgAvgThMin2 = 0;
    s32 nTmpJgAvgThMax3 = 0;
    s32 nTmpJgAvgThMin3 = 0;
    s32 nTmpJgAvgThMax4 = 0;
    s32 nTmpJgAvgThMin4 = 0;

    DBG(&g_i2c_client->dev, "*** %s() nItemId = %d ***\n", __func__, nItemId);

    if (nItemId == 40) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            nTmpRaw_dataJg1 += g_self_ic_raw_data1[g_self_ic_map40_1[i]];    /*first region: non-border */
        }

        for (i = 0; i < 2; i++) {
            nTmpRaw_dataJg2 += g_self_ic_raw_data1[g_self_ic_map40_2[i]];    /*first region: border */
        }

        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            nTmpRaw_dataJg3 += g_self_ic_raw_data1[g_self_ic_map40_3[i]];    /*second region: non-border */
        }

        for (i = 0; i < 2; i++) {
            nTmpRaw_dataJg4 += g_self_ic_raw_data1[g_self_ic_map40_4[i]];    /*second region: border */
        }
    } else if (nItemId == 41) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            nTmpRaw_dataJg1 += g_self_ic_raw_data2[g_self_ic_map41_1[i]];    /*first region: non-border */
        }

        for (i = 0; i < 2; i++) {
            nTmpRaw_dataJg2 += g_self_ic_raw_data2[g_self_ic_map41_2[i]];    /*first region: border */
        }

        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            nTmpRaw_dataJg3 += g_self_ic_raw_data2[g_self_ic_map41_3[i]];    /*second region: non-border */
        }

        for (i = 0; i < 2; i++) {
            nTmpRaw_dataJg4 += g_self_ic_raw_data2[g_self_ic_map41_4[i]];    /*second region: border */
        }
    }

    nTmpJgAvgThMax1 =
        (nTmpRaw_dataJg1 / ((g_self_ici_to_triangle_num / 4) - 2)) * (100 +
                                                                     SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD)
        / 100;
    nTmpJgAvgThMin1 =
        (nTmpRaw_dataJg1 / ((g_self_ici_to_triangle_num / 4) - 2)) * (100 -
                                                                     SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD)
        / 100;
    nTmpJgAvgThMax2 =
        (nTmpRaw_dataJg2 / 2) * (100 +
                                SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin2 =
        (nTmpRaw_dataJg2 / 2) * (100 -
                                SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMax3 =
        (nTmpRaw_dataJg3 / ((g_self_ici_to_triangle_num / 4) - 2)) * (100 +
                                                                     SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD)
        / 100;
    nTmpJgAvgThMin3 =
        (nTmpRaw_dataJg3 / ((g_self_ici_to_triangle_num / 4) - 2)) * (100 -
                                                                     SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD)
        / 100;
    nTmpJgAvgThMax4 =
        (nTmpRaw_dataJg4 / 2) * (100 +
                                SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;
    nTmpJgAvgThMin4 =
        (nTmpRaw_dataJg4 / 2) * (100 -
                                SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD) / 100;

    DBG(&g_i2c_client->dev,
        "nItemId = %d, nTmpRaw_dataJg1 = %d, nTmpJgAvgThMax1 = %d, nTmpJgAvgThMin1 = %d, nTmpRaw_dataJg2 = %d, nTmpJgAvgThMax2 = %d, nTmpJgAvgThMin2 = %d\n",
        nItemId, nTmpRaw_dataJg1, nTmpJgAvgThMax1, nTmpJgAvgThMin1,
        nTmpRaw_dataJg2, nTmpJgAvgThMax2, nTmpJgAvgThMin2);
    DBG(&g_i2c_client->dev,
        "nTmpRaw_dataJg3 = %d, nTmpJgAvgThMax3 = %d, nTmpJgAvgThMin3 = %d, nTmpRaw_dataJg4 = %d, nTmpJgAvgThMax4 = %d, nTmpJgAvgThMin4 = %d\n",
        nTmpRaw_dataJg3, nTmpJgAvgThMax3, nTmpJgAvgThMin3, nTmpRaw_dataJg4,
        nTmpJgAvgThMax4, nTmpJgAvgThMin4);

    if (nItemId == 40) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            if (g_self_ic_raw_data1[g_self_ic_map40_1[i]] > nTmpJgAvgThMax1 ||
                g_self_ic_raw_data1[g_self_ic_map40_1[i]] < nTmpJgAvgThMin1) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map40_1[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (g_self_ic_raw_data1[g_self_ic_map40_2[i]] > nTmpJgAvgThMax2 ||
                g_self_ic_raw_data1[g_self_ic_map40_2[i]] < nTmpJgAvgThMin2) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map40_2[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            if (g_self_ic_raw_data1[g_self_ic_map40_3[i]] > nTmpJgAvgThMax3 ||
                g_self_ic_raw_data1[g_self_ic_map40_3[i]] < nTmpJgAvgThMin3) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map40_3[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (g_self_ic_raw_data1[g_self_ic_map40_4[i]] > nTmpJgAvgThMax4 ||
                g_self_ic_raw_data1[g_self_ic_map40_4[i]] < nTmpJgAvgThMin4) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map40_4[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    } else if (nItemId == 41) {
        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            if (g_self_ic_raw_data2[g_self_ic_map41_1[i]] > nTmpJgAvgThMax1 ||
                g_self_ic_raw_data2[g_self_ic_map41_1[i]] < nTmpJgAvgThMin1) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map41_1[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (g_self_ic_raw_data2[g_self_ic_map41_2[i]] > nTmpJgAvgThMax2 ||
                g_self_ic_raw_data2[g_self_ic_map41_2[i]] < nTmpJgAvgThMin2) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map41_2[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < (g_self_ici_to_triangle_num / 4) - 2; i++) {
            if (g_self_ic_raw_data2[g_self_ic_map41_3[i]] > nTmpJgAvgThMax3 ||
                g_self_ic_raw_data2[g_self_ic_map41_3[i]] < nTmpJgAvgThMin3) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map41_3[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (g_self_ic_raw_data2[g_self_ic_map41_4[i]] > nTmpJgAvgThMax4 ||
                g_self_ic_raw_data2[g_self_ic_map41_4[i]] < nTmpJgAvgThMin4) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_map41_4[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    }

    return n_ret_val;
}
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

static ItoTest_result_e drv_mp_test_ito_short_test_selfIc_second(u8 nItemId)
{
    ItoTest_result_e n_ret_val = ITO_TEST_OK;
    u32 i;
    u8 nSensorCount = 0;

    DBG(&g_i2c_client->dev, "*** %s() nItemId = %d ***\n", __func__, nItemId);

    if (nItemId == 1) {         /*39-1 */
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            nSensorCount = g_msg22xx_short_sub_frame_num1;
        }

        for (i = 0; i < nSensorCount; i++) {
            if (g_self_ic_raw_data1[g_self_ic_short_map1[i]] >
                SELF_IC_SHORT_TEST_THRESHOLD) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_short_map1[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    } else if (nItemId == 2) {  /*39-2 */
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            nSensorCount = g_msg22xx_short_sub_frame_num2;
        }

        for (i = 0; i < nSensorCount; i++) {
            if (g_self_ic_raw_data2[g_self_ic_short_map2[i]] >
                SELF_IC_SHORT_TEST_THRESHOLD) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_short_map2[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    } else if (nItemId == 3) {  /*39-3 */
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            nSensorCount = g_msg22xx_short_sub_frame_num3;
        }

        for (i = 0; i < nSensorCount; i++) {
            if (g_self_ic_raw_data3[g_self_ic_short_map3[i]] >
                SELF_IC_SHORT_TEST_THRESHOLD) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_short_map3[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    }
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
    else if (nItemId == 0) {    /*39-4 (2R) */
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            nSensorCount = g_msg22xx_short_sub_frame_num4;
        }

        for (i = 0; i < nSensorCount; i++) {
            if (g_self_ic_raw_data4[g_self_ic_short_map4[i]] >
                SELF_IC_SHORT_TEST_THRESHOLD) {
                g_self_ic_test_fail_channel[g_test_fail_channel_count] =
                    g_self_ic_short_map4[i];
                g_test_fail_channel_count++;
                n_ret_val = ITO_TEST_FAIL;
            }
        }
    }
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */

    DBG(&g_i2c_client->dev, "nSensorCount = %d\n", nSensorCount);

    return n_ret_val;
}

static ItoTest_result_e drv_mp_test_self_ic_ito_open_test_entry(void)
{
    ItoTest_result_e nRetVal1 = ITO_TEST_OK, nRetVal2 = ITO_TEST_OK, nRetVal3 =
        ITO_TEST_OK;
    u32 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    DBG(&g_i2c_client->dev, "open test start\n");

    drv_set_iic_data_rate(g_i2c_client, 50000); /*50 KHz */

    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    if (!drv_mp_test_ito_test_self_ic_choose_tp_type()) {
        DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
        nRetVal1 = ITO_TEST_GET_TP_TYPE_ERROR;
        goto ITO_TEST_END;
    }

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop cpu */
    reg_set_16bit_value(0x0FE6, 0x0001);   /*bank:mheg5, addr:h0073 */
    mdelay(50);

    for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
        g_self_ic_raw_data1[i] = 0;
        g_self_ic_raw_data2[i] = 0;
        g_self_ic_raw_data3[i] = 0;
        g_self_ic_data_flag1[i] = 0;
        g_self_ic_data_flag2[i] = 0;
        g_self_ic_data_flag3[i] = 0;
        g_self_ic_test_fail_channel[i] = 0;
    }

    g_test_fail_channel_count = 0; /*Reset g_test_fail_channel_count to 0 before test start */

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        drv_mp_test_ito_open_test_msg22xx_first(40, g_self_ic_raw_data1,
                                          g_self_ic_data_flag1);

        if (g_self_ici_enable_2r) {
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
            nRetVal2 = drv_mp_test_ito_open_test_selfIc_second2r(40);
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
        } else {
            nRetVal2 = drv_mp_test_ito_open_test_selfIc_second(40);
        }

        drv_mp_test_ito_open_test_msg22xx_first(41, g_self_ic_raw_data2,
                                          g_self_ic_data_flag2);

        if (g_self_ici_enable_2r) {
#ifdef CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE
            nRetVal3 = drv_mp_test_ito_open_test_selfIc_second2r(41);
#endif /*CONFIG_ENABLE_MP_TEST_ITEM_FOR_2R_TRIANGLE */
        } else {
            nRetVal3 = drv_mp_test_ito_open_test_selfIc_second(41);
        }
    }

ITO_TEST_END:

    drv_set_iic_data_rate(g_i2c_client, 100000);    /*100 KHz */

    drv_touch_device_hw_reset();
    drv_enable_finger_touch_report();

    DBG(&g_i2c_client->dev, "open test end\n");

    if ((nRetVal1 != ITO_TEST_OK) && (nRetVal2 == ITO_TEST_OK) &&
        (nRetVal3 == ITO_TEST_OK)) {
        return ITO_TEST_GET_TP_TYPE_ERROR;
    } else if ((nRetVal1 == ITO_TEST_OK) &&
               ((nRetVal2 != ITO_TEST_OK) || (nRetVal3 != ITO_TEST_OK))) {
        return ITO_TEST_FAIL;
    } else {
        return ITO_TEST_OK;
    }
}

static ItoTest_result_e drv_mp_test_self_ic_ito_short_test_entry(void)
{
    ItoTest_result_e nRetVal1 = ITO_TEST_OK, nRetVal2 = ITO_TEST_OK, nRetVal3 =
        ITO_TEST_OK, nRetVal4 = ITO_TEST_OK, nRetVal5 = ITO_TEST_OK;
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    DBG(&g_i2c_client->dev, "short test start\n");

    drv_set_iic_data_rate(g_i2c_client, 50000); /*50 KHz */

    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    if (!drv_mp_test_ito_test_self_ic_choose_tp_type()) {
        DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
        nRetVal1 = ITO_TEST_GET_TP_TYPE_ERROR;
        goto ITO_TEST_END;
    }

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop cpu */
    reg_set_16bit_value(0x0FE6, 0x0001);   /*bank:mheg5, addr:h0073 */
    mdelay(50);

    for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
        g_self_ic_raw_data1[i] = 0;
        g_self_ic_raw_data2[i] = 0;
        g_self_ic_raw_data3[i] = 0;
        g_self_ic_raw_data4[i] = 0;
        g_self_ic_data_flag1[i] = 0;
        g_self_ic_data_flag2[i] = 0;
        g_self_ic_data_flag3[i] = 0;
        g_self_ic_data_flag4[i] = 0;
        g_self_ic_test_fail_channel[i] = 0;
    }

    g_test_fail_channel_count = 0; /*Reset g_test_fail_channel_count to 0 before test start */

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        if (g_self_ici_enable_2r) {
            drv_mp_test_ito_open_test_msg22xx_first(0, g_self_ic_raw_data4,
                                               g_self_ic_data_flag4);
            nRetVal2 = drv_mp_test_ito_short_test_selfIc_second(0);
        }

        drv_mp_test_ito_open_test_msg22xx_first(1, g_self_ic_raw_data1,
                                           g_self_ic_data_flag1);
        nRetVal3 = drv_mp_test_ito_short_test_selfIc_second(1);

        drv_mp_test_ito_open_test_msg22xx_first(2, g_self_ic_raw_data2,
                                           g_self_ic_data_flag2);
        nRetVal4 = drv_mp_test_ito_short_test_selfIc_second(2);

        drv_mp_test_ito_open_test_msg22xx_first(3, g_self_ic_raw_data3,
                                           g_self_ic_data_flag3);
        nRetVal5 = drv_mp_test_ito_short_test_selfIc_second(3);
    }

ITO_TEST_END:

    drv_set_iic_data_rate(g_i2c_client, 100000);    /*100 KHz */

    drv_touch_device_hw_reset();
    drv_enable_finger_touch_report();

    DBG(&g_i2c_client->dev, "short test end\n");

    if ((nRetVal1 != ITO_TEST_OK) && (nRetVal2 == ITO_TEST_OK) &&
        (nRetVal3 == ITO_TEST_OK) && (nRetVal4 == ITO_TEST_OK)
        && (nRetVal5 == ITO_TEST_OK)) {
        return ITO_TEST_GET_TP_TYPE_ERROR;
    } else if ((nRetVal1 == ITO_TEST_OK) &&
               ((nRetVal2 != ITO_TEST_OK) || (nRetVal3 != ITO_TEST_OK) ||
                (nRetVal4 != ITO_TEST_OK) || (nRetVal5 != ITO_TEST_OK))) {
        return ITO_TEST_FAIL;
    } else {
        return ITO_TEST_OK;
    }
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

/*------------------------------------------------------------------------------//*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static void drv_mp_test_ito_open_test_msg28xx_set_mutual_csub_via_db_bus(s16 nCSub)
{
    u8 nBaseLen = 6;
    u16 nFilter = 0x3F;
    u16 nLastFilter = 0xFFF;
    u8 nBasePattern = nCSub & nFilter;
    u8 nPattern;
    u16 n16BitsPattern;
    u16 nCSub16Bits[5] = { 0 };
    int i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < 5; i++) {
        if (i == 0) {
            nPattern = nBasePattern;    /*Patn => Pattern */
        }

        n16BitsPattern =
            ((nPattern & 0xF) << nBaseLen *
             2) | (nPattern << nBaseLen) | nPattern;

        if (i == 4) {
            nCSub16Bits[i] = n16BitsPattern & nLastFilter;
        } else {
            nCSub16Bits[i] = n16BitsPattern;
        }
        nPattern = (u8) ((n16BitsPattern >> 4) & nFilter);
    }

    reg_set_16bit_value(0x215C, 0x1FFF);

    for (i = 0; i < 5; i++) {
        reg_set_16bit_value(0x2148 + 2 * i, nCSub16Bits[i]);
        reg_set_16bit_value(0x2152 + 2 * i, nCSub16Bits[i]);
    }
}

/*
static void drv_mp_test_ito_open_test_msg28xx_afe_gain_one(void)
{
    u8 nReg_data = 0;
    u16 nAFECoef = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*AFE gain = 1X
reg_set_16bit_value(0x1318, 0x4440);
reg_set_16bit_value(0x131A, 0x4444);
reg_set_16bit_value(0x13D6, 0x2000);

reg_set_16bit_value(0x2160, 0x0040);
reg_set_16bit_value(0x2162, 0x0040);
reg_set_16bit_value(0x2164, 0x0040);
reg_set_16bit_value(0x2166, 0x0040);
reg_set_16bit_value(0x2168, 0x0040);
reg_set_16bit_value(0x216A, 0x0040);
reg_set_16bit_value(0x216C, 0x0040);
reg_set_16bit_value(0x216E, 0x0040);
reg_set_16bit_value(0x2170, 0x0040);
reg_set_16bit_value(0x2172, 0x0040);
reg_set_16bit_value(0x2174, 0x0040);
reg_set_16bit_value(0x2176, 0x0040);
reg_set_16bit_value(0x2178, 0x0040);
reg_set_16bit_value(0x217A, 0x1FFF);
reg_set_16bit_value(0x217C, 0x1FFF);

    /*reg_hvbuf_sel_gain 
reg_set_16bit_value(0x1564, 0x0077);

    /*all AFE cfb use defalt (50p) 
reg_set_16bit_value(0x1508, 0x1FFF);   /*all AFE cfb: SW control */
reg_set_16bit_value(0x1550, 0x0000);   /*all AFE cfb use defalt (50p) */

    /*ADC: AFE Gain bypass 
     * reg_set_16bit_value(0x1260, 0x1FFF);
     * 
     * /*AFE coef 
     * nReg_data = reg_get_l_byte_value(0x101A);
     * nAFECoef = (u16) (0x10000 / nReg_data);
     * reg_set_16bit_value(0x13D6, nAFECoef);
     * }
     * 
     */
static void drv_mp_test_ito_open_test_msg28xx_afe_gain_one(void)
{
    /*AFE gain = 1X */
    u16 nAfeGain = 0;
    u16 nDriOpening = 0;
    u8 nReg_data = 0;
    u16 nAfeCoef = 0;
    u16 i = 0;

    /*proto.MstarReadReg(loopDevice, (uint)0x1312, ref regdata); //get dri num */
    nReg_data = reg_get_l_byte_value(0x1312);
    nDriOpening = nReg_data;

    /*filter unit gain */
    if (nDriOpening == 11 || nDriOpening == 15 || nDriOpening == 4) {
        reg_set_16bit_value(0x1318, 0x4470);
    } else if (nDriOpening == 7) {
        reg_set_16bit_value(0x1318, 0x4460);
    }

    /*proto.MstarWriteReg_16(loopDevice, 0x131A, 0x4444); */
    reg_set_16bit_value(0x131A, 0x4444);

    /*AFE coef */
    /*proto.MstarReadReg(loopDevice, (uint)0x101A, ref regdata); */
    nReg_data = reg_get_l_byte_value(0x101A);
    nAfeCoef = 0x10000 / nReg_data;
    /*proto.MstarWriteReg_16(loopDevice, (uint)0x13D6, AFE_coef); */
    reg_set_16bit_value(0x13D6, nAfeCoef);

    /*AFE gain */
    if (nDriOpening == 7 || nDriOpening == 15 || nDriOpening == 4 ||
        nDriOpening == 1) {
        nAfeGain = 0x0040;
    } else if (nDriOpening == 11) {
        nAfeGain = 0x0055;
    }

    for (i = 0; i < 13; i++) {
        reg_set_16bit_value(0x2160 + 2 * i, nAfeGain);
    }

    /*AFE gain: over write enable */
    reg_set_16bit_value(0x217A, 0x1FFF);
    reg_set_16bit_value(0x217C, 0x1FFF);

    /*all AFE cfb use defalt (50p) */
    reg_set_16bit_value(0x1508, 0x1FFF);   /*all AFE cfb: SW control */
    /*if ((g_msg28xx_pattern_type == 5) && (g_msg28xx_pattern_model == 1)) */
    if (g_msg28xx_pattern_type == 5) {
        switch (g_msg28xx_cfb_ref) {
        case 0:
        case 2:
            set_cfb(_20p);
            break;
        case 1:
            set_cfb(_10p);
            break;
        case 3:
            set_cfb(_30p);
            break;
        case 4:
            set_cfb(_40p);
            break;
        case 5:
            set_cfb(_50p);
            break;
        }

        DBG(&g_i2c_client->dev, "set_cfb : %d", g_msg28xx_cfb_ref);
    } else {
        reg_set_16bit_value(0x1550, 0x0000);   /*all AFE cfb use defalt (50p) */
    }

    /*reg_hvbuf_sel_gain */
    reg_set_16bit_value(0x1564, 0x0077);

    /*ADC: AFE Gain bypass */
    reg_set_16bit_value(0x1260, 0x1FFF);
}

static void drv_mp_test_ito_open_test_msg28xx_calibrate_mutual_csub(s16 nCSub)
{
    u8 nChipVer;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    nChipVer = reg_get_l_byte_value(0x1ECE);
    DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# Chip ID = %d ***\n",
        nChipVer);

    if (nChipVer != 0)
        reg_set_16bit_value(0x10F0, 0x0004);   /*bit2 */

    drv_mp_test_ito_open_test_msg28xx_set_mutual_csub_via_db_bus(nCSub);
    drv_mp_test_ito_open_test_msg28xx_afe_gain_one();
}

static void drv_mp_test_ito_test_db_bus_read_dq_mem_start(void)
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

/*
static void drv_mp_test_ito_test_db_bus_read_dq_mem_startAddr24(void)
{
    u8 nParCmdSelUseCfg = 0x7F;
/*u8 nParCmdAdByteEn0 = 0x50;
/*u8 nParCmdAdByteEn1 = 0x51;
u8 nParCmdAdByteEn2 = 0x52;
/*u8 nParCmdDaByteEn0 = 0x54;
u8 nParCmdUSetSelB0 = 0x80;
u8 nParCmdUSetSelB1 = 0x82;
u8 nParCmdSetSelB2 = 0x85;
u8 nParCmdIicUse = 0x35;
    /*u8 nParCmdWr        = 0x10; 

DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

iic_write_data(slave_i2c_id_db_bus, &nParCmdSelUseCfg, 1);
    /*iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn0, 1); 
    /*iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn1, 1); 
iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn2, 1);
    /*iic_write_data(slave_i2c_id_db_bus, &nParCmdDaByteEn0, 1); 
iic_write_data(slave_i2c_id_db_bus, &nParCmdUSetSelB0, 1);
iic_write_data(slave_i2c_id_db_bus, &nParCmdUSetSelB1, 1);
iic_write_data(slave_i2c_id_db_bus, &nParCmdSetSelB2, 1);
iic_write_data(slave_i2c_id_db_bus, &nParCmdIicUse, 1);
}

*/

static void drv_mp_test_ito_test_db_bus_read_dq_mem_end(void)
{
    u8 nParCmdNSelUseCfg = 0x7E;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    iic_write_data(slave_i2c_id_db_bus, &nParCmdNSelUseCfg, 1);
}

/*
static void drv_mp_test_ito_test_db_bus_read_dq_mem_endAddr24(void)
{
    u8 nParCmdSelUseCfg  = 0x7F;
    u8 nParCmdAdByteEn1  = 0x51;
    u8 nParCmdSetSelB0   = 0x81;
    u8 nParCmdSetSelB1   = 0x83;
    u8 nParCmdNSetSelB2  = 0x84;
    u8 nParCmdIicUse     = 0x35;
    u8 nParCmdNSelUseCfg = 0x7E;
    u8 nParCmdNIicUse    = 0x34;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    reg_set_l_byte_value(0, 0);

    iic_write_data(slave_i2c_id_db_bus, &nParCmdSelUseCfg, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn1, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdSetSelB0, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdSetSelB1, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdNSetSelB2, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdIicUse, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdNSelUseCfg, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdNIicUse,  1);
}
*/

static void drv_mp_test_ito_test_msg28xx_enable_adc_one_shot(void)
{
    reg_set_16bit_value_on(0x100a, BIT0);

    return;
}

static s32 drv_mp_test_ito_test_msg28xx_trigger_mutual_one_shot(s16 *p_result_data,
                                                        u16 *pSenNum,
                                                        u16 *pDrvNum)
{
    u16 n_addr = 0x5000, nAddrNextSF = 0x1A4;
    u16 nSF = 0, nAfeOpening = 0, nDriOpening = 0;
    u16 nMax_dataNumOfOneSF = 0;
    u16 nDriMode = 0;
    int n_dataShift = -1;
    u16 i, j, k;
    u8 nReg_data = 0;
    u8 aShot_data[392] = { 0 };  /*13*15*2 */
    u16 nReg_dataU16 = 0;
    s16 *pShot_dataAll = NULL;
    u8 nParCmdIicUse = 0x35;
    u16 nReg_data2 = 0, nSwcap = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    iic_write_data(slave_i2c_id_db_bus, &nParCmdIicUse, 1);
    nReg_data = reg_get_l_byte_value(0x130A);
    nSF = nReg_data >> 4;
    nAfeOpening = nReg_data & 0x0f;

    if (nSF == 0) {
        return -1;
    }

    nReg_data = reg_get_l_byte_value(0x100B);
    nDriMode = nReg_data;

    nReg_data = reg_get_l_byte_value(0x1312);
    nDriOpening = nReg_data;

    DBG(&g_i2c_client->dev,
        "*** Msg28xx MP Test# TriggerMutualOneShot nSF=%d, nAfeOpening=%d, nDriMode=%d, nDriOpening=%d. ***\n",
        nSF, nAfeOpening, nDriMode, nDriOpening);

    nMax_dataNumOfOneSF = nAfeOpening * nDriOpening;

    pShot_dataAll = kzalloc(sizeof(s16) * nSF * nMax_dataNumOfOneSF, GFP_KERNEL);

    reg_set_16Bit_value_off(0x3D08, BIT8);  /*FIQ_E_FRAME_READY_MASK */

    nReg_data2 = reg_get_16bit_value_by_address_mode(0x1301, ADDRESS_MODE_16BIT);
    nSwcap = (nReg_data2 & 0x0800);
    if (nSwcap) {
        drv_mp_test_ito_test_msg28xx_enable_adc_one_shot();
    } else {                    /*sine mode */
        reg_mask_16bit_value(0x1E2A, BIT0, 0x0000, ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1110, BIT1, BIT1, ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1E2A, BIT0, BIT0, ADDRESS_MODE_16BIT);
    }

    /*polling frame-ready interrupt status */
    while (0x0000 == (nReg_dataU16 & BIT8)) {
        nReg_dataU16 = reg_get16_bit_value(0x3D18);
    }

    if (nDriMode == 2) {        /*for short test */
        if (nAfeOpening % 2 == 0)
            n_dataShift = -1;
        else
            n_dataShift = 0;     /*special case */
        /*s16 nShort_result_data[nSF][nAfeOpening]; */

        /*get ALL raw data */
        for (i = 0; i < nSF; i++) {
            memset(aShot_data, 0, sizeof(aShot_data));
            drv_mp_test_ito_test_db_bus_read_dq_mem_start();
            reg_get_x_bit_value(n_addr + i * nAddrNextSF, aShot_data, 28,
                            MAX_I2C_TRANSACTION_LENGTH_LIMIT);
            drv_mp_test_ito_test_db_bus_read_dq_mem_end();

            /*drv_mp_test_mutual_ic_debug_show_array(aShot_data, 26, 8, 16, 16);*/
            if (g_msg28xx_pattern_type == 5) {
                for (j = 0; j < nAfeOpening; j++) {
                    DBG(&g_i2c_client->dev, "*** i : %d, j : %d ***\n", i, j);
                    p_result_data[i * nAfeOpening + j] =
                        (s16) (aShot_data[2 * j] | aShot_data[2 * j + 1] << 8);
                    DBG(&g_i2c_client->dev,
                        "*** drv_mp_test_ito_test_msg28xx_trigger_mutual_one_shot_2 : %02X, %02X, %04X ***\n",
                        aShot_data[2 * j + 1], aShot_data[2 * j],
                        p_result_data[i * nAfeOpening + j]);
                    if (n_dataShift == 0 && (j == nAfeOpening - 1)) {
                        p_result_data[i * nAfeOpening + j] =
                            (s16) (aShot_data[2 * (j + 1)] |
                                   aShot_data[2 * (j + 1) + 1] << 8);
                    }
                }
            } else {
                for (j = 0; j < nAfeOpening; j++) {
                    p_result_data[i * MUTUAL_IC_MAX_CHANNEL_DRV + j] =
                        (s16) (aShot_data[2 * j] | aShot_data[2 * j + 1] << 8);

                    if (n_dataShift == 0 && (j == nAfeOpening - 1)) {
                        p_result_data[i * MUTUAL_IC_MAX_CHANNEL_DRV + j] =
                            (s16) (aShot_data[2 * (j + 1)] |
                                   aShot_data[2 * (j + 1) + 1] << 8);
                    }
                }
            }
        }

        *pSenNum = nSF;
        *pDrvNum = nAfeOpening;
    } else {                    /*for open test */

        /*s16 nOpen_result_data[nSF * nAfeOpening][nDriOpening]; */

        if (nAfeOpening % 2 == 0 || nDriOpening % 2 == 0)
            n_dataShift = -1;
        else
            n_dataShift = 0;     /*special case */

        /*get ALL raw data, combine and handle datashift. */
        for (i = 0; i < nSF; i++) {
            memset(aShot_data, 0, sizeof(aShot_data));
            drv_mp_test_ito_test_db_bus_read_dq_mem_start();
            reg_get_x_bit_value(n_addr + i * nAddrNextSF, aShot_data, 392,
                            MAX_I2C_TRANSACTION_LENGTH_LIMIT);
            drv_mp_test_ito_test_db_bus_read_dq_mem_end();

            /*drv_mp_test_mutual_ic_debug_show_array(aShot_data, 390, 8, 10, 16);*/
            for (j = 0; j < nMax_dataNumOfOneSF; j++) {
                DBG(&g_i2c_client->dev, "*** i : %d, j : %d ***\n", i, j);
                pShot_dataAll[i * nMax_dataNumOfOneSF + j] =
                    (s16) (aShot_data[2 * j] | aShot_data[2 * j + 1] << 8);
                DBG(&g_i2c_client->dev,
                    "*** drv_mp_test_ito_test_msg28xx_trigger_mutual_one_shot_1 : %02X, %02X, %04X ***\n",
                    aShot_data[2 * j + 1], aShot_data[2 * j],
                    pShot_dataAll[i * nMax_dataNumOfOneSF + j]);

                if (n_dataShift == 0 && j == (nMax_dataNumOfOneSF - 1)) {
                    pShot_dataAll[i * nMax_dataNumOfOneSF + j] =
                        (s16) (aShot_data[2 * (j + 1)] |
                               aShot_data[2 * (j + 1) + 1] << 8);
                    DBG(&g_i2c_client->dev,
                        "*** drv_mp_test_ito_test_msg28xx_trigger_mutual_one_shot_2 : %02X, %02X, %04X ***\n",
                        aShot_data[2 * (j + 1) + 1], aShot_data[2 * (j + 1)],
                        pShot_dataAll[i * nMax_dataNumOfOneSF + j]);
                }
            }
            DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# aShot_data ***\n");
            drv_mp_test_mutual_ic_debug_show_array(aShot_data,
                                             g_mutual_ic_sense_line_num *
                                             g_mutual_ic_drive_line_num, 8, 16,
                                             g_mutual_ic_drive_line_num);
        }

        DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# pShot_dataAll ***\n");
        drv_mp_test_mutual_ic_debug_show_array(pShot_dataAll,
                                         g_mutual_ic_sense_line_num *
                                         g_mutual_ic_drive_line_num, -16, 16,
                                         g_mutual_ic_drive_line_num);

        DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# pShot_dataAll ***\n");
        drv_mp_test_mutual_ic_debug_show_array(pShot_dataAll,
                                         g_mutual_ic_sense_line_num *
                                         g_mutual_ic_drive_line_num, -16, 10,
                                         g_mutual_ic_drive_line_num);

        /*problem here */
        for (k = 0; k < nSF; k++) {
            for (i = k * nAfeOpening; i < nAfeOpening * (k + 1); i++) { /*Sen */
                for (j = 0; j < nDriOpening; j++) { /*Dri */
                    p_result_data[i * MUTUAL_IC_MAX_CHANNEL_DRV + j] = pShot_dataAll[k * nMax_dataNumOfOneSF + (j + (i - nAfeOpening * k) * nDriOpening)];  /*result_data[Sen, Dri] */
                }
            }
        }

        *pSenNum = nSF * nAfeOpening;
        *pDrvNum = nDriOpening;
    }
    reg_set_16bit_value_on(0x3D08, BIT8);   /*FIQ_E_FRAME_READY_MASK */
    reg_set_16bit_value_on(0x3D08, BIT4);   /*FIQ_E_TIMER0_MASK */

    kfree(pShot_dataAll);

    return 0;
}

static s32 drv_mp_test_ito_test_msg28xx_get_mutual_one_shot_raw_iir(s16 * n_result_data,
                                                          u16 *pSenNum,
                                                          u16 *pDrvNum)
{
    return drv_mp_test_ito_test_msg28xx_trigger_mutual_one_shot(n_result_data, pSenNum,
                                                        pDrvNum);
}

static s32 drv_mp_test_ito_test_msg28xx_get_deltac(s32 *pDeltaC)
{
    s16 *p_raw_data = NULL;
    s16 aRaw_dataOverlapDone[g_msg28xx_sense_num][g_msg28xx_drive_num];
    u16 nDrvPos = 0, nSenPos = 0, nShift = 0;
    u16 nSenNumBak = 0;
    u16 nDrvNumBak = 0;
    s16 i, j;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    p_raw_data =
        kzalloc(sizeof(s16) * MUTUAL_IC_MAX_CHANNEL_SEN * 2 *
                MUTUAL_IC_MAX_CHANNEL_DRV, GFP_KERNEL);

    if (drv_mp_test_ito_test_msg28xx_get_mutual_one_shot_raw_iir
        (p_raw_data, &nSenNumBak, &nDrvNumBak) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# GetMutualOneShotRawIIR failed! ***\n");
        return -1;
    }

    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# nSenNumBak=%d nDrvNumBak=%d ***\n", nSenNumBak,
        nDrvNumBak);

    DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# p_raw_data ***\n");
    drv_mp_test_mutual_ic_debug_show_array(p_raw_data,
                                     g_mutual_ic_sense_line_num *
                                     g_mutual_ic_drive_line_num, -16, 10,
                                     g_mutual_ic_sense_line_num);

    if (g_msg28xx_pattern_type == 5) {
        for (i = 0; i < g_msg28xx_sense_num; i++) {
            for (j = 0; j < g_msg28xx_drive_num; j++) {
                aRaw_dataOverlapDone[i][j] = MSG28XX_UN_USE_SENSOR;
            }
        }
    }

    for (i = 0; i < nSenNumBak; i++) {
        for (j = 0; j < nDrvNumBak; j++) {
            nShift = (u16) (i * nDrvNumBak + j);

            if (g_msg28xx_tp_type == TP_TYPE_X) {
                if (g_is_dynamic_code && (g_msg28xx_AnaGen_Version[2] > 16) &&
                    (g_msg28xx_map_va_mutual != NULL)) {
                    nDrvPos = *(g_msg28xx_map_va_mutual + nShift * 2 + 1);
                    nSenPos = *(g_msg28xx_map_va_mutual + nShift * 2 + 0);
                } else {
                    nDrvPos = g_MapVaMutual_X[nShift][1];
                    nSenPos = g_MapVaMutual_X[nShift][0];
                }
            } else if (g_msg28xx_tp_type == TP_TYPE_Y) {
                if (g_is_dynamic_code && (g_msg28xx_AnaGen_Version[2] > 16) &&
                    (g_msg28xx_map_va_mutual != NULL)) {
                    nDrvPos = *(g_msg28xx_map_va_mutual + nShift * 2 + 1);
                    nSenPos = *(g_msg28xx_map_va_mutual + nShift * 2 + 0);
                } else {
                    nDrvPos = g_MapVaMutual_Y[nShift][1];
                    nSenPos = g_MapVaMutual_Y[nShift][0];
                }
            }
            if (nDrvPos >= g_mutual_ic_drive_line_num ||
                nSenPos >= g_mutual_ic_sense_line_num)
                continue;

/*DBG(&g_i2c_client->dev, "*** nDrvPos=%d nSenPos=%d ***\n", nDrvPos, nSenPos);*/
            if (nDrvPos != 0xFF && nSenPos != 0xFF) {
                aRaw_dataOverlapDone[nSenPos][nDrvPos] =
                    p_raw_data[i * MUTUAL_IC_MAX_CHANNEL_DRV + j];
            }
        }
    }

    DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# aRaw_dataOverlapDone ***\n");
    drv_mp_test_mutual_ic_debug_show_array(aRaw_dataOverlapDone,
                                     g_mutual_ic_sense_line_num *
                                     g_mutual_ic_drive_line_num, -16, 10,
                                     g_mutual_ic_sense_line_num);

    for (i = 0; i < g_mutual_ic_sense_line_num; i++) {
        for (j = 0; j < g_mutual_ic_drive_line_num; j++) {
            nShift = (u16) (i * g_mutual_ic_drive_line_num + j);
            pDeltaC[nShift] = (s32) aRaw_dataOverlapDone[i][j];
        }
    }

    DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# gDeltaC ***\n");
    drv_mp_test_mutual_ic_debug_show_array(pDeltaC,
                                     g_mutual_ic_sense_line_num *
                                     g_mutual_ic_drive_line_num, -32, 10,
                                     g_mutual_ic_sense_line_num);

    kfree(p_raw_data);

    return 0;
}

static void drv_mp_test_ito_test_msg28xx_ana_sw_reset(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*reset ANA */
    reg_set_16bit_value_on(0x1002, (BIT0 | BIT1 | BIT2 | BIT3));    /*reg_tgen_soft_rst: 1 to reset */
    reg_set_16Bit_value_off(0x1002, (BIT0 | BIT1 | BIT2 | BIT3));

    /*delay */
    mdelay(20);
}

static void drv_mp_test_ito_test_msg28xx_ana_change_cd_time(u16 nTime1, u16 nTime2)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    reg_set_16bit_value(0x1026, nTime1);
    reg_set_16bit_value(0x1030, nTime2);
}

static void drv_mp_test_ito_open_test_msg28xx_on_cell_ana_enable_charge_pump(u8
                                                                  enable_charge_pump)
{
    u16 val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (!enable_charge_pump) {
        reg_set_16Bit_value_off(0x1560, BIT0 | BIT1);   /*high v buf en, close */
        val = reg_get16_bit_value(0x1566); /*set */
        val |= BIT12;
        reg_set_16bit_value(0x1566, val);  /*LV on */
        reg_set_16Bit_value_off(0x1566, BIT8 | BIT9 | BIT10 | BIT11);
        reg_set_16Bit_value_off(0x1570, BIT8 | BIT10);
        reg_set_16Bit_value_off(0x1580, BIT0 | BIT4);

        val = reg_get16_bit_value(0x1464); /*set */
        val |= BIT2 | BIT3;
        reg_set_16bit_value(0x1464, val);
    } else {
        val = reg_get16_bit_value(0x1560); /*set */
        val |= 0x03;
        reg_set_16bit_value(0x1560, val);  /*high v buf en, close */
        reg_set_16Bit_value_off(0x1566, BIT12); /*LV on */
        reg_set_16Bit_value_off(0x1566, BIT8 | BIT9 | BIT10 | BIT11);   /*LV on */

        val = reg_get16_bit_value(0x1566); /*set */
        val |= BIT8 | BIT10;
        reg_set_16bit_value(0x1566, val);

        val = reg_get16_bit_value(0x1570); /*cset */
        val |= BIT10;
        reg_set_16bit_value(0x1570, val);

        val = reg_get16_bit_value(0x1580); /*set */
        val |= BIT0 | BIT1;
        reg_set_16bit_value(0x1580, val);
        reg_set_16Bit_value_off(0x1464, BIT2 | BIT3);
    }
}

static void drv_mp_test_ito_open_test_msg28xx_on_cell_update_ana_charge_dump_setting(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    reg_set_16bit_value_by_address_mode(0x1018, 0x001F, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1019, 0x003f, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x101a, 0x0028, ADDRESS_MODE_16BIT);  /*post idle time in sub0 */
    reg_set_16bit_value_by_address_mode(0x101b, 0x0028, ADDRESS_MODE_16BIT);  /*post idle time in sub1 */
    reg_set_16bit_value_by_address_mode(0x101c, 0x0028, ADDRESS_MODE_16BIT);  /*post idle time in sub2 */
    reg_set_16bit_value_by_address_mode(0x101d, 0x0028, ADDRESS_MODE_16BIT);  /*post idle time in sub3 */
    reg_set_16bit_value_by_address_mode(0x101e, 0x0028, ADDRESS_MODE_16BIT);  /*post idle time in sub4 */
    reg_set_16bit_value_by_address_mode(0x101f, 0x0000, ADDRESS_MODE_16BIT);

    reg_set_16bit_value_by_address_mode(0x100d, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1103, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1104, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1302, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x136b, 0x10000 / 0x20, ADDRESS_MODE_16BIT);  /*AFE_coef, set value by 0x10000 dividing register 0x100d value */
    reg_set_16bit_value_by_address_mode(0x1b30, 0x0020, ADDRESS_MODE_16BIT);
}

static void drv_mp_test_ito_open_test_msg28xx_open_sine_mode_setting(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_mp_test_ito_open_test_msg28xx_on_cell_ana_enable_charge_pump
        (g_msg28xx_charge_pump);

    switch (g_msg28xx_cfb_ref) {
    case 0:
    case 2:
        set_cfb(_20p);
        break;
    case 1:
        set_cfb(_10p);
        break;
    case 3:
        set_cfb(_30p);
        break;
    case 4:
        set_cfb(_40p);
        break;
    case 5:
        set_cfb(_50p);
        break;
    }
    DBG(&g_i2c_client->dev, "set_cfb : %d", g_msg28xx_cfb_ref);
}

static s32 drv_mp_test_ito_open_test_msg28xx_obtain_open_value_keys(s32 *pkeyarray)
{
    u16 k;
    u32 nShift = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    set_cfb(_50p);
    drv_mp_test_ito_open_test_msg28xx_on_cell_ana_enable_charge_pump(DISABLE);
    if (drv_mp_test_ito_test_msg28xx_get_deltac(g_mutual_ic_delta_c) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# GetDeltaC failed! ***\n");
        return -1;
    }

    for (k = 0; k < g_msg28xx_key_num; k++) {
        nShift =
            (g_msg28xx_key_sen[k] - 1) * g_msg28xx_key_drv_o +
            g_msg28xx_key_drv_o - 1;
        pkeyarray[k] = g_mutual_ic_delta_c[nShift];
    }

    return 0;
}

static void drv_mp_test_ito_open_test_msg28xx_db_bus_re_enter(void)
{
    u8 nParCmdSelUseCfg = 0x7F;
    u8 nParCmdAdByteEn1 = 0x51;
    u8 nPar_N_CmdUSetSelB0 = 0x80;
    u8 nPar_N_CmdUSetSelB1 = 0x82;
    u8 nPar_N_CmdSetSelB2 = 0x84;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    iic_write_data(slave_i2c_id_db_bus, &nParCmdSelUseCfg, 1);
    iic_write_data(slave_i2c_id_db_bus, &nParCmdAdByteEn1, 1);
    iic_write_data(slave_i2c_id_db_bus, &nPar_N_CmdUSetSelB0, 1);
    iic_write_data(slave_i2c_id_db_bus, &nPar_N_CmdUSetSelB1, 1);
    iic_write_data(slave_i2c_id_db_bus, &nPar_N_CmdSetSelB2, 1);
}

static s32 drv_mp_test_ito_test_msg28xx_check_switch_status(void)
{
    u32 nReg_data = 0;
    int nTimeOut = 280;
    int nT = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    do {
        nReg_data = reg_get16_bit_value(0x1402);
        mdelay(20);
        nT++;
        if (nT > nTimeOut) {
            return -1;
        }

    } while (nReg_data != 0x7447);

    return 0;
}

static s32 drv_mp_test_ito_open_test_msg28xx_re_enter_mutual_mode(u16 nFMode)
{
    DBG(&g_i2c_client->dev, "*** %s() nFMode = 0x%x***\n", __func__, nFMode);

    /*Start mcu */
    reg_set_l_byte_value(0x0FE6, 0x00);

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    drv_mp_test_ito_open_test_msg28xx_db_bus_re_enter();

    mdelay(50);

    reg_set_16bit_value(0x1402, nFMode);
    /*
     * switch (nFMode)
     * {
     * case MUTUAL_MODE:
     * reg_set_16bit_value(0x1402, 0x5705);
     * break;
     * case MUTUAL_SINE:
     * reg_set_16bit_value(0x1402, 0x5706);
     * break;
     * }
     */
    if (drv_mp_test_ito_test_msg28xx_check_switch_status() < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx MP Test# CheckSwitchStatus failed! ***\n");
        return -1;
    }

    /*Stop mcu */
    reg_set_16bit_value(0x0FE6, 0x0001);
    reg_set_16bit_value(0x3D08, 0xFEFF);   /*open timer */

    return 0;
}

void drv_mp_test_ito_open_test_msg28xx_open_swcap_mode_setting(void)
{
    u16 nChargeT = 0x0C, nDumpT = 0x06;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);

    drv_mp_test_ito_open_test_msg28xx_calibrate_mutual_csub(g_msg28xx_csub_ref);
    drv_mp_test_ito_test_msg28xx_ana_change_cd_time(nChargeT, nDumpT);
    drv_mp_test_ito_open_test_msg28xx_on_cell_ana_enable_charge_pump(DISABLE);
    reg_set_16bit_value(0x156A, 0x000A);   /*DAC com voltage */
    drv_mp_test_ito_open_test_msg28xx_on_cell_update_ana_charge_dump_setting();
    drv_mp_test_ito_test_msg28xx_ana_sw_reset();
}

void drv_mp_test_ito_test_msg28xx_sram_enter_access_mode(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*change to R2 mode */
    reg_mask_16bit_value(0x2149, BIT5, BIT5, ADDRESS_MODE_16BIT);
    /*SRAM using MCU clock */
    reg_mask_16bit_value(0x1E11, BIT13, BIT13, ADDRESS_MODE_16BIT);
}

void drv_mp_test_ito_test_msg28xx_sram_exit_access_mode(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*change to R2 mode */
    reg_mask_16bit_value(0x2149, BIT5, 0, ADDRESS_MODE_16BIT);
    /*SRAM using MCU clock */
    reg_mask_16bit_value(0x1E11, BIT13, 0, ADDRESS_MODE_16BIT);
}

void drv_mp_test_ito_test_msg28xx__reg_get16bit_byte_value_buf(u16 n_addr, u8 *p_buf,
                                                     u16 nLen)
{
    u16 i;
    u8 aTx_data[3] = { 0x10, (n_addr >> 8) & 0xFF, n_addr & 0xFF };
    u8 *p_rx_data = (u8 *) kzalloc(nLen, sizeof(u8));

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    iic_write_data(slave_i2c_id_db_bus, &aTx_data[0], 3);
    iic_read_data(slave_i2c_id_db_bus, p_rx_data, nLen);

    for (i = 0; i < nLen; i++) {
        p_buf[i] = p_rx_data[i];
    }

    kfree(p_rx_data);
}

void drv_mp_test_ito_test_msg28xx_write_dq_mem_8bit(u16 addr, u8 data)
{
    u8 aReadBuf[4] = { 0 };
    u16 nHigh16, nLow16 = 0;
    u16 read_addr = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    read_addr = addr - (addr % 4);

    drv_mp_test_ito_test_msg28xx__reg_get16bit_byte_value_buf(read_addr, aReadBuf,
                                                    sizeof(aReadBuf) /
                                                    sizeof(aReadBuf[0]));

    aReadBuf[addr % 4] = data;
    DBG(&g_i2c_client->dev, "read_buf:0x%02x 0x%02x 0x%02x  0x%02x",
        aReadBuf[0], aReadBuf[1], aReadBuf[2], aReadBuf[3]);
    nLow16 = (((u16) aReadBuf[1] << 8) | aReadBuf[0]);
    nHigh16 = (((u16) aReadBuf[3] << 8) | aReadBuf[2]);

    reg_set_16bit_value(read_addr + 2, nLow16);
    reg_set_16bit_value(read_addr, nHigh16);
}

s32 drv_mp_test_ito_open_test_msg28xx_on_cell_open_va_value(void)
{
    int i, nIsf = 0;
    u8 aShot_data[16], nReg_data = 0;
    u16 n_addr = 0x6410, nSF = 0;
    u16 nKeySen = g_msg28xx_pad_table_drive[g_msg28xx_key_drv_o - 1];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    nReg_data = reg_get_l_byte_value(0x130A);
    nSF = nReg_data >> 4;

    drv_mp_test_ito_test_msg28xx_sram_enter_access_mode();

    for (nIsf = 0; nIsf < nSF; nIsf++) {
        memset(aShot_data, 0, sizeof(aShot_data) / sizeof(aShot_data[0]));
        drv_mp_test_ito_test_db_bus_read_dq_mem_start();
        /*RegGetXByteValue(u8Shot_data, addr + 0x20 * isf, sizeof(u8Shot_data) / sizeof(u8Shot_data[0])); */
        reg_get_x_bit_value(n_addr + 0x20 * nIsf, aShot_data,
                        sizeof(aShot_data) / sizeof(aShot_data[0]),
                        MAX_I2C_TRANSACTION_LENGTH_LIMIT);
        DBG(&g_i2c_client->dev, "*** %s() _ Before***\n", __func__);
        drv_mp_test_mutual_ic_debug_show_array(aShot_data,
                                         sizeof(aShot_data) /
                                         sizeof(aShot_data[0]), 8, 16, 10);
        /*Find key index, then replace key sensor by assign sensor 56(0x38). */
        for (i = 0; i < (sizeof(aShot_data) / sizeof(u8)); i++) {
            if (aShot_data[i] == nKeySen) {
                drv_mp_test_ito_test_msg28xx_write_dq_mem_8bit(n_addr + nIsf * 0x20 + i,
                                                        0x38);
            }
        }
        /*RegGetXByteValue(u8Shot_data, addr + 0x20 * isf, sizeof(u8Shot_data) / sizeof(u8Shot_data[0])); */
        reg_get_x_bit_value(n_addr + 0x20 * nIsf, aShot_data,
                        sizeof(aShot_data) / sizeof(aShot_data[0]),
                        MAX_I2C_TRANSACTION_LENGTH_LIMIT);
        DBG(&g_i2c_client->dev, "*** %s() _ After***\n", __func__);
        drv_mp_test_mutual_ic_debug_show_array(aShot_data,
                                         sizeof(aShot_data) /
                                         sizeof(aShot_data[0]), 8, 16, 10);
        drv_mp_test_ito_test_db_bus_read_dq_mem_end();
    }
    drv_mp_test_ito_test_msg28xx_sram_exit_access_mode();
    drv_mp_test_ito_test_msg28xx_ana_sw_reset();
    if (drv_mp_test_ito_test_msg28xx_get_deltac(g_mutual_ic_delta_c) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# GetDeltaC failed! ***\n");
        return -1;
    }
    return 0;
}

static s32 drv_mp_test_msg28xx_ito_open_test(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);

    drv_mp_test_ito_open_test_msg28xx_calibrate_mutual_csub(g_msg28xx_csub_ref);
    reg_set_16bit_value(0x156A, 0x000A);   /*DAC com voltage */
    drv_mp_test_ito_test_msg28xx_ana_sw_reset();

    if (drv_mp_test_ito_test_msg28xx_get_deltac(g_mutual_ic_delta_c) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# GetDeltaC failed! ***\n");
        return -1;
    }

    return 0;
}

static s32 drv_mp_test_ito_open_test_msg28xx_open_judge(u16 nItemID,
                                                 s8 pNormalTest_result[][2],
                                                 u16
                                                 pNormalTest_resultCheck[][13]
                                                 /*, u16 nDriOpening */
    )
{
    s32 n_ret_val = 0;
    u16 nCSub = g_msg28xx_csub_ref;
    u16 nRowNum = 0, nColumnNum = 0;
    u32 nSum = 0, nAvg = 0, nDelta = 0, nPrev = 0;
    u16 i, j, k;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < g_mutual_ic_sense_line_num * g_mutual_ic_drive_line_num; i++) {
        /*deltaC_result[i] = deltaC[i]; */
        /*g_mutual_ic_result[i] = 1673 * nCSub - g_mutual_ic_delta_c[i] * 2 / (nDriOpening + 1);*/
        if (g_mutual_ic_delta_c[i] > 31000) {
            return -1;
        }

        g_mutual_ic_result[i] = 1673 * nCSub - g_mutual_ic_delta_c[i];

        /*For mutual key, last column if not be used, show number "one". */
        if ((g_msg28xx_mutual_key == 1 || g_msg28xx_mutual_key == 2) &&
            (g_msg28xx_key_num != 0)) {
            if ((g_mutual_ic_sense_line_num < g_mutual_ic_drive_line_num) &&
                ((i + 1) % g_mutual_ic_drive_line_num == 0)) {
                g_mutual_ic_result[i] = -32000;
                for (k = 0; k < g_msg28xx_key_num; k++) {
                    if ((i + 1) / g_mutual_ic_drive_line_num == g_msg28xx_key_sen[k]) {
                        /*g_mutual_ic_result[i] = 1673 * nCSub - g_mutual_ic_delta_c[i] * 2 / (nDriOpening + 1);*/
                        g_mutual_ic_result[i] =
                            1673 * nCSub - g_mutual_ic_delta_c[i];
                    }
                }
            }

            if ((g_mutual_ic_sense_line_num > g_mutual_ic_drive_line_num) &&
                (i >
                 (g_mutual_ic_sense_line_num - 1) * g_mutual_ic_drive_line_num - 1)) {
                g_mutual_ic_result[i] = -32000;
                for (k = 0; k < g_msg28xx_key_num; k++) {
                    if (((i + 1) -
                         (g_mutual_ic_sense_line_num -
                          1) * g_mutual_ic_drive_line_num) == g_msg28xx_key_sen[k]) {
                        /*g_mutual_ic_result[i] = 1673 * nCSub - g_mutual_ic_delta_c[i] * 2 / (nDriOpening + 1);*/
                        g_mutual_ic_result[i] =
                            1673 * nCSub - g_mutual_ic_delta_c[i];
                    }
                }
            }
        }
    }

    if (g_msg28xx_key_num > 0) {
        if (g_mutual_ic_drive_line_num >= g_mutual_ic_sense_line_num) {
            nRowNum = g_mutual_ic_drive_line_num - 1;
            nColumnNum = g_mutual_ic_sense_line_num;
        } else {
            nRowNum = g_mutual_ic_drive_line_num;
            nColumnNum = g_mutual_ic_sense_line_num - 1;
        }
    } else {
        nRowNum = g_mutual_ic_drive_line_num;
        nColumnNum = g_mutual_ic_sense_line_num;
    }

    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# Show g_mutual_ic_result ***\n");
    /*drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_result, nRowNum*nColumnNum, -32, 10, nColumnNum);*/
    for (j = 0; j < g_mutual_ic_drive_line_num; j++) {
        for (i = 0; i < g_mutual_ic_sense_line_num; i++) {
            DBG(&g_i2c_client->dev, "%d  ",
                g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j]);
        }
        DBG(&g_i2c_client->dev, "\n");
    }

    for (j = 0; j < nRowNum; j++) {
        nSum = 0;
        for (i = 0; i < nColumnNum; i++) {
            nSum = nSum + g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j];
        }

        nAvg = nSum / nColumnNum;
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# OpenJudge average=%d ***\n", nAvg);
        for (i = 0; i < nColumnNum; i++) {
            if (0 ==
                drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result
                                                    [i *
                                                     g_mutual_ic_drive_line_num +
                                                     j],
                                                    (s32) (nAvg +
                                                           nAvg *
                                                           MSG28XX_DC_RANGE /
                                                           100),
                                                    (s32) (nAvg -
                                                           nAvg *
                                                           MSG28XX_DC_RANGE /
                                                           100))) {
                g_mutual_ic_test_fail_channel[i * g_mutual_ic_drive_line_num + j] = 1;
                g_test_fail_channel_count++;
                n_ret_val = -1;
            }

            if (i > 0) {
                nDelta =
                    g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j] >
                    nPrev ? (g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j] -
                             nPrev) : (nPrev -
                                       g_mutual_ic_result[i *
                                                        g_mutual_ic_drive_line_num +
                                                        j]);
                if (nDelta > nPrev * MUTUAL_IC_FIR_RATIO / 100) {
                    if (0 == g_mutual_ic_test_fail_channel[i * g_mutual_ic_drive_line_num + j]) {   /*for avoid g_test_fail_channel_count to be added twice */
                        g_mutual_ic_test_fail_channel[i * g_mutual_ic_drive_line_num +
                                                  j] = 1;
                        g_test_fail_channel_count++;
                    }
                    n_ret_val = -1;
                    DBG(&g_i2c_client->dev,
                        "\nSense%d, Drive%d, MAX_Ratio = %d,%d\t", i, j, nDelta,
                        nPrev);
                }
            }
            nPrev = g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j];
        }
    }

    /*DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# OpenJudge g_test_fail_channel_count=%d ***\n", g_test_fail_channel_count); */
    return n_ret_val;
}

static s32 drv_mp_test_ito_open_test_msg28xx_on_cell_open_judge(u16 nItemID,
                                                       s8 pNormalTest_result[],
                                                       u16
                                                       pNormalTest_resultCheck[]
                                                       /*, u16 nDriOpening */
    )
{
    s32 n_ret_val = 0;
    u16 nCSub = g_msg28xx_csub_ref;
    u16 nRowNum = 0, nColumnNum = 0;
    u16 i, j, k;
    s32 bg_per_csub = 0;
    u16 nCfb = 0;
    s32 ratioAvg1000 = 0, ratioAvgMax1000 = 0, ratioAvgMin1000 = 0, passCount =
        0;
    s32 ratioAvgBorder1000 = 0, ratioAvgBorderMax1000 =
        0, ratioAvgBorderMin1000 = 0, passCount1 = 0;
    s32 ratioAvgMove1000 = 0, ratioAvgBorderMove1000 = 0;

    memset(g_normal_test_fail_check_deltac, 0xFFFF, MUTUAL_IC_MAX_MUTUAL_NUM);
    memset(g_normal_test_fail_check_ratio1000, 0xFFFF, MUTUAL_IC_MAX_MUTUAL_NUM);
    memset(g_mutual_ic_on_cell_open_test_ratio1000, 0,
           sizeof(g_mutual_ic_on_cell_open_test_ratio1000));
    memset(g_mutual_ic_on_cell_open_test_ratio_border1000, 0,
           sizeof(g_mutual_ic_on_cell_open_test_ratio_border1000));
    memset(g_mutual_ic_on_cell_open_test_ratio_move1000, 0,
           sizeof(g_mutual_ic_on_cell_open_test_ratio_move1000));
    memset(g_mutual_ic_on_cell_open_test_ratio_border_move1000, 0,
           sizeof(g_mutual_ic_on_cell_open_test_ratio_border_move1000));

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_msg28xx_open_mode == 1) { /*if open mode = 1 (sine mode), Csub must be zero. */
        nCSub = 0;
    }

    if (!g_msg28xx_cfb_ref) {
        nCfb = 2;
    } else {
        nCfb = g_msg28xx_cfb_ref;
    }

    bg_per_csub = (int)8365 / nCfb;
    /*bg_per_csub = (int)(2.4 * 1.17 * 32768 / (11 * nCfb)); */

    for (i = 0; i < g_mutual_ic_sense_line_num * g_mutual_ic_drive_line_num; i++) {
        if (g_mutual_ic_delta_c[i] > 31000) {
            return -1;
        }

        if (g_mutual_ic_delta_c[i] != MSG28XX_UN_USE_SENSOR) {
            if ((g_msg28xx_pattern_type == 5) && (g_msg28xx_pattern_model == 1)) {
                g_mutual_ic_result[i] = bg_per_csub * nCSub - g_mutual_ic_delta_c[i];
            } else {
                g_mutual_ic_result[i] = 1673 * nCSub - g_mutual_ic_delta_c[i];
            }
        } else {
            g_mutual_ic_result[i] = MSG28XX_NULL_DATA;
        }
        /*For mutual key, last column if not be used, show number "one". */
        if ((g_msg28xx_mutual_key == 1 || g_msg28xx_mutual_key == 2) &&
            (g_msg28xx_key_num != 0)) {
            if (g_msg28xx_pattern_type == 5) {
                if (g_msg28xx_sensor_key_ch != g_msg28xx_key_num) {
                    if (!((i + 1) % g_mutual_ic_drive_line_num)) {
                        g_mutual_ic_result[i] = MSG28XX_NULL_DATA;

                        for (k = 0; k < g_msg28xx_key_num; k++) {
                            if ((i + 1) / g_mutual_ic_drive_line_num ==
                                g_msg28xx_key_sen[k]) {
                                if (g_msg28xx_pattern_model == 1) {
                                    g_mutual_ic_result[i] =
                                        bg_per_csub * nCSub -
                                        g_mutual_ic_delta_c[i];
                                } else {
                                    g_mutual_ic_result[i] =
                                        1673 * nCSub - g_mutual_ic_delta_c[i];
                                }
                            }
                        }
                    }
                } else {
                    if (i >
                        ((g_msg28xx_sense_num - 1) * g_msg28xx_drive_num - 1)) {
                        g_mutual_ic_result[i] = MSG28XX_NULL_DATA;

                        for (k = 0; k < g_msg28xx_key_num; k++) {
                            if (((i + 1) -
                                 (g_mutual_ic_sense_line_num -
                                  1) * g_mutual_ic_drive_line_num) ==
                                g_msg28xx_key_sen[k]) {
                                if (g_msg28xx_pattern_model == 1) {
                                    g_mutual_ic_result[i] =
                                        bg_per_csub * nCSub -
                                        g_mutual_ic_delta_c[i];
                                } else {
                                    g_mutual_ic_result[i] =
                                        1673 * nCSub - g_mutual_ic_delta_c[i];
                                }
                            }
                        }
                    }
                }
            } else {
                if ((g_mutual_ic_sense_line_num < g_mutual_ic_drive_line_num) &&
                    ((i + 1) % g_mutual_ic_drive_line_num == 0)) {
                    g_mutual_ic_result[i] = MSG28XX_NULL_DATA;
                    for (k = 0; k < g_msg28xx_key_num; k++) {
                        if ((i + 1) / g_mutual_ic_drive_line_num ==
                            g_msg28xx_key_sen[k]) {
                            g_mutual_ic_result[i] =
                                1673 * nCSub - g_mutual_ic_delta_c[i];
                        }
                    }
                }

                if ((g_mutual_ic_sense_line_num > g_mutual_ic_drive_line_num) &&
                    (i >
                     (g_mutual_ic_sense_line_num - 1) * g_mutual_ic_drive_line_num -
                     1)) {
                    g_mutual_ic_result[i] = MSG28XX_NULL_DATA;
                    for (k = 0; k < g_msg28xx_key_num; k++) {
                        if (((i + 1) -
                             (g_mutual_ic_sense_line_num -
                              1) * g_mutual_ic_drive_line_num) ==
                            g_msg28xx_key_sen[k]) {
                            g_mutual_ic_result[i] =
                                1673 * nCSub - g_mutual_ic_delta_c[i];
                        }
                    }
                }
            }
        }
    }

/*nRowNum = g_mutual_ic_drive_line_num;*/
/*nColumnNum = g_mutual_ic_sense_line_num;*/
    nRowNum = g_mutual_ic_sense_line_num;
    nColumnNum = g_mutual_ic_drive_line_num;

    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# Show g_mutual_ic_result ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_result, nRowNum * nColumnNum,
                                     -32, 10, nColumnNum);
    memcpy(g_mutual_ic_on_cell_open_test_result_data, g_mutual_ic_result,
           sizeof(g_mutual_ic_on_cell_open_test_result_data));
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# Show g_mutual_ic_on_cell_open_test_golden_channel ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_on_cell_open_test_golden_channel,
                                     nRowNum * nColumnNum, -32, 10, nColumnNum);
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# Show g_mutual_ic_on_cell_open_test_golden_channel_max ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_on_cell_open_test_golden_channel_max,
                                     nRowNum * nColumnNum, -32, 10, nColumnNum);
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# Show g_mutual_ic_on_cell_open_test_golden_channel_min ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_on_cell_open_test_golden_channel_min,
                                     nRowNum * nColumnNum, -32, 10, nColumnNum);

    for (k = 0; k < (sizeof(g_mutual_ic_delta_c) / sizeof(g_mutual_ic_delta_c[0]));
         k++) {
        if (0 == g_mutual_ic_on_cell_open_test_golden_channel[k]) {
            if (k == 0) {
                pNormalTest_result[0] = 1;   /*no golden sample */
            }
            break;
        }

        if (g_mutual_ic_result[k] != MSG28XX_NULL_DATA) {
            g_mutual_ic_on_cell_open_test_ratio1000[k] =
                g_mutual_ic_result[k] * 1000 /
                g_mutual_ic_on_cell_open_test_golden_channel[k];

            if (0 ==
                drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[k],
                                                    g_mutual_ic_on_cell_open_test_golden_channel_max
                                                    [k],
                                                    g_mutual_ic_on_cell_open_test_golden_channel_min
                                                    [k])) {
                DBG(&g_i2c_client->dev,
                    "*** Msg28xx Open Test# Golden FAIL: %d ***\n", k);
                DBG(&g_i2c_client->dev,
                    "*** Msg28xx Open Test# g_mutual_ic_result: %d ***\n",
                    g_mutual_ic_result[k]);
                DBG(&g_i2c_client->dev,
                    "*** Msg28xx Open Test# g_mutual_ic_on_cell_open_test_golden_channel_max: %d ***\n",
                    g_mutual_ic_on_cell_open_test_golden_channel_max[k]);
                DBG(&g_i2c_client->dev,
                    "*** Msg28xx Open Test# g_mutual_ic_on_cell_open_test_golden_channel_min: %d ***\n",
                    g_mutual_ic_on_cell_open_test_golden_channel_min[k]);
                pNormalTest_result[0] = 1;
                pNormalTest_resultCheck[k] =
                    (u16) (((k / g_mutual_ic_drive_line_num) + 1) * 100 +
                           ((k % g_mutual_ic_drive_line_num) + 1));
            } else {
                pNormalTest_resultCheck[k] = MSG28XX_PIN_NO_ERROR;

                if ((g_msg28xx_pattern_type == 3) && (g_msg28xx_key_num == 0) &&
                    ((k % g_mutual_ic_drive_line_num == 0) ||
                     ((k + 1) % g_mutual_ic_drive_line_num == 0))) {
                    ratioAvgBorder1000 += g_mutual_ic_on_cell_open_test_ratio1000[k];
                    passCount1 += 1;
                } else if ((g_msg28xx_pattern_type == 3) &&
                           (g_msg28xx_key_num != 0) &&
                           ((k % g_mutual_ic_drive_line_num == 0) ||
                            ((k + 2) % g_mutual_ic_drive_line_num == 0))) {
                    ratioAvgBorder1000 += g_mutual_ic_on_cell_open_test_ratio1000[k];
                    passCount1 += 1;
                } else {
                    ratioAvg1000 += g_mutual_ic_on_cell_open_test_ratio1000[k];
                    passCount += 1;
                }
            }
        } else {
            pNormalTest_resultCheck[k] = MSG28XX_PIN_NO_ERROR;
        }

        g_normal_test_fail_check_deltac[k] = pNormalTest_resultCheck[k];

    }

    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# Show  g_mutual_ic_on_cell_open_test_ratio1000 ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_on_cell_open_test_ratio1000,
                                     nRowNum * nColumnNum, -32, 10, nColumnNum);
    ratioAvgMax1000 =
        (s32) (100000 + g_msg28xx_dc_ratio_1000 +
               g_msg28xx_dc_ratio_up_1000) / 100;
    ratioAvgMin1000 = (s32) (100000 - g_msg28xx_dc_ratio_1000) / 100;
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# g_msg28xx_dc_ratio_1000: %d ***\n",
        g_msg28xx_dc_ratio_1000);
    DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# ratioAvgMax1000: %d ***\n",
        ratioAvgMax1000);
    DBG(&g_i2c_client->dev, "*** Msg28xx Open Test# ratioAvgMin1000: %d ***\n",
        ratioAvgMin1000);
    ratioAvgBorderMax1000 =
        (s32) (100000 + g_msg28xx_dc_border_ratio_1000 +
               g_msg28xx_dc_border_ratio_up_1000) / 100;
    ratioAvgBorderMin1000 =
        (s32) (100000 - g_msg28xx_dc_border_ratio_1000) / 100;
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# g_msg28xx_dc_border_ratio_1000: %d ***\n",
        g_msg28xx_dc_border_ratio_1000);
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# ratioAvgBorderMax1000: %d ***\n",
        ratioAvgBorderMax1000);
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# ratioAvgBorderMin1000: %d ***\n",
        ratioAvgBorderMin1000);

    if (passCount != 0) {
        if (passCount1 != 0) {
            ratioAvgBorderMove1000 = ratioAvgBorder1000 / passCount1;
            ratioAvgMove1000 = ratioAvg1000 / passCount;

            for (i = 0;
                 i <
                 sizeof(g_mutual_ic_on_cell_open_test_ratio1000) /
                 sizeof(g_mutual_ic_on_cell_open_test_ratio1000[0]); i++) {
                if ((g_msg28xx_key_num == 0) &&
                    ((i % g_mutual_ic_drive_line_num == 0) ||
                     ((i + 1) % g_mutual_ic_drive_line_num == 0))) {
                    g_mutual_ic_on_cell_open_test_ratio_move1000[i] =
                        g_mutual_ic_on_cell_open_test_ratio1000[i] -
                        ratioAvgBorderMove1000 + 1;
                } else if ((g_msg28xx_key_num != 0) &&
                           ((i % g_mutual_ic_drive_line_num == 0) ||
                            ((i + 2) % g_mutual_ic_drive_line_num == 0))) {
                    g_mutual_ic_on_cell_open_test_ratio_move1000[i] =
                        g_mutual_ic_on_cell_open_test_ratio1000[i] -
                        ratioAvgBorderMove1000 + 1;
                } else {
                    g_mutual_ic_on_cell_open_test_ratio_move1000[i] =
                        g_mutual_ic_on_cell_open_test_ratio1000[i] -
                        ratioAvgMove1000 + 1;
                }
            }
        } else {
            ratioAvgMove1000 = ratioAvg1000 / passCount;
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Open Test# ratioAvgMove1000: %d ***\n",
                ratioAvgMove1000);

            for (i = 0;
                 i <
                 sizeof(g_mutual_ic_on_cell_open_test_ratio1000) /
                 sizeof(g_mutual_ic_on_cell_open_test_ratio1000[0]); i++) {
                g_mutual_ic_on_cell_open_test_ratio_move1000[i] =
                    g_mutual_ic_on_cell_open_test_ratio1000[i] - ratioAvgMove1000 +
                    1000;
            }
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Open Test# Show g_mutual_ic_on_cell_open_test_ratio_move1000 ***\n");
            drv_mp_test_mutual_ic_debug_show_array
                (g_mutual_ic_on_cell_open_test_ratio_move1000, nRowNum * nColumnNum,
                 -32, 10, nColumnNum);
            memcpy(g_mutual_ic_on_cell_open_test_result_ratio_data,
                   g_mutual_ic_on_cell_open_test_ratio_move1000,
                   sizeof(g_mutual_ic_on_cell_open_test_result_ratio_data));
        }
    } else {
        memcpy(g_mutual_ic_on_cell_open_test_ratio1000,
               g_mutual_ic_on_cell_open_test_ratio_move1000,
               sizeof(g_mutual_ic_on_cell_open_test_ratio1000));
    }

    for (j = 0; j < (sizeof(g_mutual_ic_delta_c) / sizeof(g_mutual_ic_delta_c[0]));
         j++) {
        if (0 == g_mutual_ic_on_cell_open_test_golden_channel[j]) {
            if (j == 0) {
                pNormalTest_result[1] = 1;   /*no golden sample */
            }
            break;
        }

        if (MSG28XX_PIN_NO_ERROR == pNormalTest_resultCheck[j]) {

            if (g_mutual_ic_result[j] != MSG28XX_NULL_DATA) {
                if ((g_msg28xx_pattern_type == 3) && (g_msg28xx_key_num == 0) &&
                    ((j % g_mutual_ic_drive_line_num == 0) ||
                     ((j + 1) % g_mutual_ic_drive_line_num == 0))) {
                    if (0 ==
                        drv_mp_test_mutual_ic_check_value_range
                        (g_mutual_ic_on_cell_open_test_ratio_move1000[j],
                         ratioAvgBorderMax1000, ratioAvgBorderMin1000)) {
                        DBG(&g_i2c_client->dev,
                            "*** Msg28xx Open Test# FAIL: %d ***\n", j);
                        pNormalTest_result[1] = 1;
                        pNormalTest_resultCheck[j] =
                            (u16) (((j / g_mutual_ic_drive_line_num) + 1) * 100 +
                                   ((j % g_mutual_ic_drive_line_num) + 1));
                    } else {
                        pNormalTest_resultCheck[j] = MSG28XX_PIN_NO_ERROR;
                    }
                } else if ((g_msg28xx_pattern_type == 3) &&
                           (g_msg28xx_key_num != 0) &&
                           ((j % g_mutual_ic_drive_line_num == 0) ||
                            ((j + 2) % g_mutual_ic_drive_line_num == 0))) {
                    if (0 ==
                        drv_mp_test_mutual_ic_check_value_range
                        (g_mutual_ic_on_cell_open_test_ratio_move1000[j],
                         ratioAvgBorderMax1000, ratioAvgBorderMin1000)) {
                        pNormalTest_result[1] = 1;
                        pNormalTest_resultCheck[j] =
                            (u16) (((j / g_mutual_ic_drive_line_num) + 1) * 100 +
                                   ((j % g_mutual_ic_drive_line_num) + 1));
                    } else {
                        pNormalTest_resultCheck[j] = MSG28XX_PIN_NO_ERROR;
                    }
                } else {
                    if (0 ==
                        drv_mp_test_mutual_ic_check_value_range
                        (g_mutual_ic_on_cell_open_test_ratio_move1000[j],
                         ratioAvgMax1000, ratioAvgMin1000)) {
                        DBG(&g_i2c_client->dev,
                            "*** Msg28xx Open Test# Ratio FAIL: %d ***\n", j);
                        pNormalTest_result[1] = 1;
                        pNormalTest_resultCheck[j] =
                            (u16) (((j / g_mutual_ic_drive_line_num) + 1) * 100 +
                                   ((j % g_mutual_ic_drive_line_num) + 1));
                        DBG(&g_i2c_client->dev,
                            "*** g_mutual_ic_drive_line_num: %d ***\n",
                            g_mutual_ic_drive_line_num);
                    } else {
                        pNormalTest_resultCheck[j] = MSG28XX_PIN_NO_ERROR;
                    }
                }
            } else {
                g_normal_test_fail_check_ratio1000[j] = MSG28XX_PIN_NO_ERROR;
            }
        } else {
            g_normal_test_fail_check_ratio[j] = pNormalTest_resultCheck[j];
            continue;
        }

        g_normal_test_fail_check_ratio[j] = pNormalTest_resultCheck[j];
    }

    for (k = 0; k < MUTUAL_IC_MAX_MUTUAL_NUM; k++) {
        if (0 == g_mutual_ic_on_cell_open_test_golden_channel[k]) {
            pNormalTest_resultCheck[k] = MSG28XX_PIN_NO_ERROR;
            g_normal_test_fail_check_deltac[k] = MSG28XX_PIN_NO_ERROR;
            g_normal_test_fail_check_ratio[k] = MSG28XX_PIN_NO_ERROR;
        } else {
            continue;
        }
    }

    if ((pNormalTest_result[0] != 0) || (pNormalTest_result[1] != 0))
        n_ret_val = -1;

    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# pNormalTest_result[0]: %d ***\n",
        pNormalTest_result[0]);
    g_mutual_ic_on_cell_open_test_result[0] = pNormalTest_result[0];
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# pNormalTest_result[1]: %d ***\n",
        pNormalTest_result[1]);
    g_mutual_ic_on_cell_open_test_result[1] = pNormalTest_result[1];

    return n_ret_val;
}

static s32 drv_mp_test_msg28xx_ito_test_switch_fw_mode(u16 *pFMode)
{
    u8 nFreq = g_msg28xx_fixed_carrier, nReg_data;
    u8 aCmd[3] = { 0x0B, 0x01, 0x00 };
    u32 nFreq_data = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*MCU_stop */
    mdelay(100);

    reg_set_16bit_value(0X3D08, 0xFFFF);
    reg_set_16bit_value(0X3D18, 0xFFFF);

    retry_reg_set_16bit_value(0x1402, 0x7474);

    retry_reg_set_16bit_value(0x1E06, 0x0000);
    retry_reg_set_16bit_value(0x1E06, 0x0001);
    /*reg_set_16bit_value((uint)0x1E04, (uint)0x7D60); */
    /*reg_set_16bit_value(0x1E04, 0x829F); */
    retry_reg_set_16bit_value(0x0FE6, 0x0000);

    if (g_msg28xx_pattern_type == 5) {
        mdelay(300);
    } else {
        mdelay(150);
    }

    if (drv_mp_test_ito_test_msg28xx_check_switch_status() < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx MP Test# CheckSwitchStatus failed! Enter MP mode failed ***\n");
        return -1;
    }

    if (g_msg28xx_pattern_type == 5) {
        if (g_msg28xx_deep_stand_by == 1) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx MP Test# enter deep standby! ***\n");

            reg_set_16bit_value(0x1402, 0x6179);
            mdelay(g_msg28xx_deep_stand_by_time_out);

            db_bus_enter_serial_debug_mode();
            /*db_bus_wait_mcu(); */
            db_bus_stop_mcu();
            db_bus_iic_use_bus();
            db_bus_iic_reshape();

            if (drv_mp_test_ito_test_msg28xx_check_switch_status() < 0) {
                g_msg28xx_deep_stand_by = 0;
                DBG(&g_i2c_client->dev,
                    "*** Msg28xx MP Test# Deep standby fail, fw not support DEEP STANDBY ***\n");
                return -1;
            }
        }
    } else {
        if (g_msg28xx_deep_stand_by == 0) {
            /*deep satndby mode */
            reg_set_16bit_value(0x1402, 0x6179);
            mdelay(600);

            db_bus_enter_serial_debug_mode();
            db_bus_wait_mcu();
            db_bus_iic_use_bus();
            db_bus_iic_reshape();

            if (drv_mp_test_ito_test_msg28xx_check_switch_status() < 0) {
                g_msg28xx_deep_stand_by = -1;
                DBG(&g_i2c_client->dev,
                    "*** Msg28xx MP Test# Deep standby fail, fw not support DEEP STANDBY ***\n");
                return -1;
            }
        }
    }

    if (*pFMode == MUTUAL_SINE) {
        nFreq = (g_msg28xx_fixed_carrier > 30) ? g_msg28xx_fixed_carrier : 30;
        nFreq = (g_msg28xx_fixed_carrier < 140) ? g_msg28xx_fixed_carrier : 140;
        aCmd[2] = nFreq;

        DBG(&g_i2c_client->dev, "*** nFreq = %d  ***\n", nFreq);
        iic_write_data(slave_i2c_id_dw_i2c, &aCmd[0], sizeof(aCmd) / sizeof(u8));
    }

    DBG(&g_i2c_client->dev, "*** nFMode = 0x%x  ***\n", *pFMode);

    switch (*pFMode) {
    case MUTUAL_MODE:
        reg_set_16bit_value(0x1402, 0x5705);
        break;

    case MUTUAL_SINE:
        reg_set_16bit_value(0x1402, 0x5706);
        break;

    case SELF:
        reg_set_16bit_value(0x1402, 0x6278);
        break;

    case WATERPROOF:
        reg_set_16bit_value(0x1402, 0x7992);
        break;

    case MUTUAL_SINGLE_DRIVE:
        reg_set_16bit_value(0x1402, 0x0158);
        break;

    default:
        return -1;
    }

    if (drv_mp_test_ito_test_msg28xx_check_switch_status() < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx MP Test# CheckSwitchStatus failed! Enter FW mode failed  ***\n");
        return -1;
    }

    if (*pFMode == MUTUAL_SINE) {
        nReg_data = reg_get_16bit_value_by_address_mode(0x2003, ADDRESS_MODE_16BIT);
        nFreq_data = nReg_data * (13000000 / 16384) / 1000;   /*khz */

        if (abs(nFreq_data - g_msg28xx_fixed_carrier) >= 2) {
            DBG(&g_i2c_client->dev,
                "Fixed carrier failed, current frequency = %d khz, need fixed frequency = %d khz",
                nFreq_data, g_msg28xx_fixed_carrier);
            return -1;
        }
    }

    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*stop mcu */
    reg_set_16bit_value(0x3D08, 0xFEFF);   /*open timer */

    return 0;
}

static u16 drv_mp_test_msg28xx_ito_test_get_tp_type(void)
{
    u16 nMajor = 0, nMinor = 0;
    u8 sz_tx_data[3] = { 0 };
    u8 sz_rx_data[4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    sz_tx_data[0] = 0x03;

    iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
    iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 4);

    nMajor = (sz_rx_data[1] << 8) + sz_rx_data[0];
    nMinor = (sz_rx_data[3] << 8) + sz_rx_data[2];

    DBG(&g_i2c_client->dev, "*** major = %d ***\n", nMajor);
    DBG(&g_i2c_client->dev, "*** minor = %d ***\n", nMinor);

    return nMajor;
}

static void drv_mp_test_ito_open_test_msg28xx_on_cell_cal_golden_range(void)
{
    u32 i = 0, value = 0, value_up = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < MUTUAL_IC_MAX_MUTUAL_NUM; i++) {
        value =
            (s32) g_msg28xx_dc_range
            * abs(g_mutual_ic_on_cell_open_test_golden_channel[i]) / 100;
        value_up =
            (s32) g_msg28xx_dc_range_up
            * abs(g_mutual_ic_on_cell_open_test_golden_channel[i]) / 100;
        g_mutual_ic_on_cell_open_test_golden_channel_max[i] =
            g_mutual_ic_on_cell_open_test_golden_channel[i] + value + value_up;
        g_mutual_ic_on_cell_open_test_golden_channel_min[i] =
            g_mutual_ic_on_cell_open_test_golden_channel[i] - value;
    }
}

static u16 drv_mp_test_msg28xx_ito_test_choose_tp_type(void)
{
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_msg28xx_sense_num = 0;
    g_msg28xx_drive_num = 0;
    g_msg28xx_key_num = 0;
    g_msg28xx_key_line = 0;
    g_msg28xx_gr_num = 0;
    g_msg28xx_csub_ref = 0;
    g_msg28xx_cfb_ref = 0;
    g_msg28xx_sense_mutual_scan_num = 0;
    g_msg28xx_mutual_key = 0;
    g_msg28xx_dc_range = 0;
    g_msg28xx_dc_range_up = 0;
    g_msg28xx_dc_ratio_1000 = 0;
    g_msg28xx_dc_ratio_up_1000 = 0;
    g_msg28xx_dc_border_ratio_1000 = 0;
    g_msg28xx_dc_border_ratio_up_1000 = 0;
    g_msg28xx_pattern_model = 0;
    g_msg28xx_sensor_key_ch = 0;
    g_msg28xx_deep_stand_by = 0;
    g_msg28xx_deep_stand_by_time_out = 0;
    g_msg28xx_short_value = 0;
    g_msg28xx_ic_pin_short = 0;
    g_msg28xx_support_ic = 0;
    g_msg28xx_open_mode = 0;
    g_msg28xx_charge_pump = 0;
    g_msg28xx_key_drv_o = 0;
    g_msg28xx_sub_frame = 0;
    g_msg28xx_fixed_carrier = 0;

    g_msg28xx_short_n1_test_number = 0;
    g_msg28xx_short_n2_test_number = 0;
    g_msg28xx_short_s1_test_number = 0;
    g_msg28xx_short_s2_test_number = 0;
    g_msg28xx_short_test_5_type = 0;
    g_msg28xx_short_x_test_number = 0;

    g_msg28xx_short_n1_test_pin = NULL;
    g_msg28xx_short_n1_mux_mem_20_3e = NULL;
    g_msg28xx_short_n2_test_pin = NULL;
    g_msg28xx_short_n2_muc_mem_20_3e = NULL;
    g_msg28xx_short_s1_test_pin = NULL;
    g_msg28xx_short_s1_mux_mem_20_3e = NULL;
    g_msg28xx_short_s2_test_pin = NULL;
    g_msg28xx_short_s2_mux_mem_20_3e = NULL;
    g_msg28xx_short_x_test_pin = NULL;
    g_msg28xx_short_x_mux_mem_20_3e = NULL;
    g_msg28xx_short_gr_test_pin = NULL;
    g_msg28xx_short_gr_mux_mem_20_3e = NULL;
    g_msg28xx_pad_table_drive = NULL;
    g_msg28xx_pad_table_sense = NULL;
    g_msg28xx_pad_table_gr = NULL;

    g_msg28xx_key_sen = NULL;
    g_msg28xx_key_drv = NULL;

    g_msg28xx_map_va_mutual = NULL;

    for (i = 0; i < 10; i++) {
        g_msg28xx_tp_type = drv_mp_test_msg28xx_ito_test_get_tp_type();
        DBG(&g_i2c_client->dev, "TP Type = %d, i = %d\n", g_msg28xx_tp_type, i);

        if (TP_TYPE_X == g_msg28xx_tp_type || TP_TYPE_Y == g_msg28xx_tp_type) { /*Modify. */
            break;
        } else if (i < 5) {
            mdelay(100);
        } else {
            drv_touch_device_hw_reset();
        }
    }

    if (TP_TYPE_X == g_msg28xx_tp_type) { /*Modify. */
        DBG(&g_i2c_client->dev, "*** Choose Tp Type X ***\n");

        g_msg28xx_sense_num = SENSE_NUM_X;
        g_msg28xx_drive_num = DRIVE_NUM_X;
        g_msg28xx_key_num = KEY_NUM_X;
        g_msg28xx_key_line = KEY_LINE_X;
        g_msg28xx_gr_num = GR_NUM_X;
        g_msg28xx_csub_ref = CSUB_REF_X;
        g_msg28xx_sense_mutual_scan_num = SENSE_MUTUAL_SCAN_NUM_X;
        g_msg28xx_mutual_key = MUTUAL_KEY_X;
        g_msg28xx_pattern_type = PATTERN_TYPE_X;

        g_msg28xx_short_n1_test_number = SHORT_N1_TEST_NUMBER_X;
        g_msg28xx_short_n2_test_number = SHORT_n2_test_number_X;
        g_msg28xx_short_s1_test_number = SHORT_s1_test_number_X;
        g_msg28xx_short_s2_test_number = SHORT_s2_test_number_X;
        g_msg28xx_short_test_5_type = SHORT_TEST_5_TYPE_X;
        g_msg28xx_short_x_test_number = SHORT_X_TEST_NUMBER_X;

        g_msg28xx_short_n1_test_pin = MSG28XX_SHORT_N1_TEST_PIN_X;
        g_msg28xx_short_n1_mux_mem_20_3e = SHORT_N1_mux_mem_20_3e_X;
        g_msg28xx_short_n2_test_pin = MSG28XX_SHORT_N2_TEST_PIN_X;
        g_msg28xx_short_n2_muc_mem_20_3e = SHORT_N2_mux_mem_20_3e_X;
        g_msg28xx_short_s1_test_pin = MSG28XX_SHORT_S1_TEST_PIN_X;
        g_msg28xx_short_s1_mux_mem_20_3e = SHORT_S1_mux_mem_20_3e_X;
        g_msg28xx_short_s2_test_pin = MSG28XX_SHORT_S2_TEST_PIN_X;
        g_msg28xx_short_s2_mux_mem_20_3e = SHORT_S2_mux_mem_20_3e_X;
        g_msg28xx_short_x_test_pin = MSG28XX_SHORT_X_TEST_PIN_X;
        g_msg28xx_short_x_mux_mem_20_3e = SHORT_X_mux_mem_20_3e_X;

        g_msg28xx_pad_table_drive = PAD_TABLE_DRIVE_X;
        g_msg28xx_pad_table_sense = PAD_TABLE_SENSE_X;
        g_msg28xx_pad_table_gr = PAD_TABLE_GR_X;

        g_msg28xx_key_sen = KEYSEN_X;
        g_msg28xx_key_drv = KEYDRV_X;
        g_msg28xx_AnaGen_Version = ANAGEN_VER_X;

        if (g_msg28xx_pattern_type == 5) {
            g_msg28xx_cfb_ref = CFB_REF_X;
            g_msg28xx_dc_range = DC_RANGE_X;
            g_msg28xx_dc_range_up = DC_Range_UP_X;
            g_msg28xx_dc_ratio_1000 = DC_Ratio_X * 1000;
            g_msg28xx_dc_ratio_up_1000 = DC_Ratio_UP_X * 1000;
            g_msg28xx_dc_border_ratio_1000 = DC_Border_Ratio_X * 1000;
            g_msg28xx_dc_border_ratio_up_1000 = DC_Border_Ratio_UP_X * 1000;
            g_msg28xx_pattern_model = TP_1T2R_MODEL_X;
            g_msg28xx_sensor_key_ch = sensor_key_ch_X;
            g_msg28xx_deep_stand_by = DEEP_STANDBY_X;
            g_msg28xx_deep_stand_by_time_out = DEEP_STANDBY_TIMEOUT_X;
            g_msg28xx_short_value = SHORT_VALUE_X;
            g_msg28xx_ic_pin_short = ICPIN_SHORT_X;
            g_msg28xx_support_ic = SupportIC_X;
            g_msg28xx_open_mode = OPEN_MODE_X;
            g_msg28xx_charge_pump = CHARGE_PUMP_X;
            g_msg28xx_key_drv_o = KEYDRV_O_X;
            g_msg28xx_sub_frame = SUB_FRAME_X;
            g_msg28xx_fixed_carrier = FIXED_CARRIER_X;

            g_msg28xx_short_x_test_number = SHORT_GR_TEST_NUMBER_X;
            g_msg28xx_short_gr_test_pin = MSG28XX_SHORT_GR_TEST_PIN_X;
            g_msg28xx_short_gr_mux_mem_20_3e = SHORT_GR_mux_mem_20_3e_X;

            g_msg28xx_short_ic_pin_mux_mem_1_settings =
                ICPIN_MUX_MEM_1_settings_X;
            g_msg28xx_short_ic_pin_mux_mem_2_settings =
                ICPIN_MUX_MEM_2_settings_X;
            g_msg28xx_short_ic_pin_mux_mem_3_settings =
                ICPIN_MUX_MEM_3_settings_X;
            g_msg28xx_short_ic_pin_mux_mem_4_settings =
                ICPIN_MUX_MEM_4_settings_X;
            g_msg28xx_short_ic_pin_mux_mem_5_settings =
                ICPIN_MUX_MEM_5_settings_X;

            g_msg28xx_sense_pad_pin_mapping = SENSE_PAD_PIN_MAPPING_X;

            memcpy(g_mutual_ic_on_cell_open_test_golden_channel, g_GoldenChannel_X,
                   sizeof(g_mutual_ic_on_cell_open_test_golden_channel));

            drv_mp_test_ito_open_test_msg28xx_on_cell_cal_golden_range();
        }
        /*g_Msg28xxMapVaMutual = g_MapVaMutual_X; */
    } else if (TP_TYPE_Y == g_msg28xx_tp_type) {  /*Modify. */
        DBG(&g_i2c_client->dev, "*** Choose Tp Type Y ***\n");

        g_msg28xx_sense_num = SENSE_NUM_Y;
        g_msg28xx_drive_num = DRIVE_NUM_Y;
        g_msg28xx_key_num = KEY_NUM_Y;
        g_msg28xx_key_line = KEY_LINE_Y;
        g_msg28xx_gr_num = GR_NUM_Y;
        g_msg28xx_csub_ref = CSUB_REF_Y;
        g_msg28xx_sense_mutual_scan_num = SENSE_MUTUAL_SCAN_NUM_Y;
        g_msg28xx_mutual_key = MUTUAL_KEY_Y;
        g_msg28xx_pattern_type = PATTERN_TYPE_Y;

        g_msg28xx_short_n1_test_number = SHORT_N1_TEST_NUMBER_Y;
        g_msg28xx_short_n2_test_number = SHORT_n2_test_number_Y;
        g_msg28xx_short_s1_test_number = SHORT_s1_test_number_Y;
        g_msg28xx_short_s2_test_number = SHORT_s2_test_number_Y;
        g_msg28xx_short_test_5_type = SHORT_TEST_5_TYPE_Y;
        g_msg28xx_short_x_test_number = SHORT_X_TEST_NUMBER_Y;

        g_msg28xx_short_n1_test_pin = MSG28XX_SHORT_N1_TEST_PIN_Y;
        g_msg28xx_short_n1_mux_mem_20_3e = SHORT_N1_mux_mem_20_3e_Y;
        g_msg28xx_short_n2_test_pin = MSG28XX_SHORT_N2_TEST_PIN_Y;
        g_msg28xx_short_n2_muc_mem_20_3e = SHORT_N2_mux_mem_20_3e_Y;
        g_msg28xx_short_s1_test_pin = MSG28XX_SHORT_S1_TEST_PIN_Y;
        g_msg28xx_short_s1_mux_mem_20_3e = SHORT_S1_mux_mem_20_3e_Y;
        g_msg28xx_short_s2_test_pin = MSG28XX_SHORT_S2_TEST_PIN_Y;
        g_msg28xx_short_s2_mux_mem_20_3e = SHORT_S2_mux_mem_20_3e_Y;
        g_msg28xx_short_x_test_pin = MSG28XX_SHORT_X_TEST_PIN_Y;
        g_msg28xx_short_x_mux_mem_20_3e = SHORT_X_mux_mem_20_3e_Y;

        g_msg28xx_pad_table_drive = PAD_TABLE_DRIVE_Y;
        g_msg28xx_pad_table_sense = PAD_TABLE_SENSE_Y;
        g_msg28xx_pad_table_gr = PAD_TABLE_GR_Y;

        g_msg28xx_key_sen = KEYSEN_Y;
        g_msg28xx_key_drv = KEYDRV_Y;

        g_msg28xx_AnaGen_Version = ANAGEN_VER_Y;

        if ((g_msg28xx_pattern_type == 5)) {
            g_msg28xx_cfb_ref = CFB_REF_Y;
            g_msg28xx_dc_range = DC_RANGE_Y;
            g_msg28xx_dc_range_up = DC_Range_UP_X;
            g_msg28xx_dc_ratio_1000 = DC_Ratio_Y * 1000;
            g_msg28xx_dc_ratio_up_1000 = DC_Ratio_UP_Y * 1000;
            g_msg28xx_dc_border_ratio_1000 = DC_Border_Ratio_Y * 1000;
            g_msg28xx_dc_border_ratio_up_1000 = DC_Border_Ratio_UP_Y * 1000;
            g_msg28xx_pattern_model = TP_1T2R_MODEL_Y;
            g_msg28xx_sensor_key_ch = sensor_key_ch_Y;
            g_msg28xx_deep_stand_by = DEEP_STANDBY_Y;
            g_msg28xx_deep_stand_by_time_out = DEEP_STANDBY_TIMEOUT_Y;
            g_msg28xx_short_value = SHORT_VALUE_Y;
            g_msg28xx_ic_pin_short = ICPIN_SHORT_Y;
            g_msg28xx_support_ic = SupportIC_Y;
            g_msg28xx_open_mode = OPEN_MODE_Y;
            g_msg28xx_charge_pump = CHARGE_PUMP_Y;
            g_msg28xx_key_drv_o = KEYDRV_O_Y;
            g_msg28xx_sub_frame = SUB_FRAME_Y;
            g_msg28xx_fixed_carrier = FIXED_CARRIER_Y;

            g_msg28xx_short_x_test_number = SHORT_GR_TEST_NUMBER_Y;
            g_msg28xx_short_gr_test_pin = MSG28XX_SHORT_GR_TEST_PIN_Y;
            g_msg28xx_short_gr_mux_mem_20_3e = SHORT_GR_mux_mem_20_3e_Y;

            g_msg28xx_short_ic_pin_mux_mem_1_settings =
                ICPIN_MUX_MEM_1_settings_Y;
            g_msg28xx_short_ic_pin_mux_mem_2_settings =
                ICPIN_MUX_MEM_2_settings_Y;
            g_msg28xx_short_ic_pin_mux_mem_3_settings =
                ICPIN_MUX_MEM_3_settings_Y;
            g_msg28xx_short_ic_pin_mux_mem_4_settings =
                ICPIN_MUX_MEM_4_settings_Y;
            g_msg28xx_short_ic_pin_mux_mem_5_settings =
                ICPIN_MUX_MEM_5_settings_Y;

            g_msg28xx_sense_pad_pin_mapping = SENSE_PAD_PIN_MAPPING_Y;

            memcpy(g_mutual_ic_on_cell_open_test_golden_channel, g_GoldenChannel_Y,
                   sizeof(g_mutual_ic_on_cell_open_test_golden_channel));

            drv_mp_test_ito_open_test_msg28xx_on_cell_cal_golden_range();
        }
        /*g_Msg28xxMapVaMutual = g_MapVaMutual_Y; */
    } else {
        g_msg28xx_tp_type = 0;
    }

    return g_msg28xx_tp_type;
}

static s32 drv_mp_test_ito_open_test_msg228xx_get_value_r(s32 *pTarget)
{
    s16 *p_raw_data = NULL;
    u16 nSenNumBak = 0;
    u16 nDrvNumBak = 0;
    u16 nShift = 0;
    s16 i, j;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_msg28xx_pattern_type == 5) {
        p_raw_data = kzalloc(sizeof(s16) * 1170, GFP_KERNEL);
    } else {
        p_raw_data =
            kzalloc(sizeof(s16) * MUTUAL_IC_MAX_CHANNEL_SEN * 2 *
                    MUTUAL_IC_MAX_CHANNEL_DRV, GFP_KERNEL);
    }

    if (drv_mp_test_ito_test_msg28xx_get_mutual_one_shot_raw_iir
        (p_raw_data, &nSenNumBak, &nDrvNumBak) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# GetMutualOneShotRawIIR failed! ***\n");
        return -1;
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 13; j++) {
            nShift = (u16) (j + 13 * i);
            if (g_msg28xx_pattern_type == 5) {
                pTarget[nShift] = p_raw_data[i * 13 + j];
            } else {
                pTarget[nShift] = p_raw_data[i * MUTUAL_IC_MAX_CHANNEL_DRV + j];
            }
        }
    }

    kfree(p_raw_data);
    return 0;
}

int drv_mp_test_msg28xx_ascii_to_hex(char ch)
{
    char ch_tmp;
    int hex_val = -1;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    ch_tmp = tolower(ch);

    if ((ch_tmp >= '0') && (ch_tmp <= '9')) {
        hex_val = ch_tmp - '0';
    } else if ((ch_tmp >= 'a') && (ch_tmp <= 'f')) {
        hex_val = ch_tmp - 'a' + 10;
    }

    return hex_val;
}

int drv_mp_test_msg28xx_str_to_hex(char *hex_str)
{
    int i, len;
    int hex_tmp, hex_val;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    len = strlen(hex_str);
    hex_val = 0;
    for (i = 0; i < len; i++) {
        hex_tmp = drv_mp_test_msg28xx_ascii_to_hex(hex_str[i]);

        if (hex_tmp == -1) {
            return -1;
        }

        hex_val = (hex_val) * 16 + hex_tmp;
    }
    return hex_val;
}

static s16 drv_mp_test_msg28xx_ito_test_choose_va_mapping(u16 nScanMode)
{
    s16 n_ret_val = 0;
    u8 nDrvOpening = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    nDrvOpening = reg_get_l_byte_value(0x1312);
    DBG(&g_i2c_client->dev, "*** nDrvOpening = %d ***\n", nDrvOpening);
    if (TP_TYPE_X == g_msg28xx_tp_type) {
        switch (nDrvOpening) {
        case 4:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker4_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker4_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker4_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker4_Key_X ***\n");
            }
            break;
        case 5:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker5_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker5_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker5_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker5_Key_X ***\n");
            }
            break;
        case 6:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker6_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker6_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker6_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker6_Key_X ***\n");
            }
            break;
        case 7:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker7_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker7_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker7_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker7_Key_X ***\n");
            }
            break;
        case 8:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker8_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker8_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker8_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker8_Key_X ***\n");
            }
            break;
        case 9:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker9_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker9_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker9_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker9_Key_X ***\n");
            }
            break;
        case 10:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker10_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker10_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker10_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker10_Key_X ***\n");
            }
            break;
        case 11:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker11_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker11_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker11_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker11_Key_X ***\n");
            }
            break;
        case 12:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker12_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker12_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker12_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker12_Key_X ***\n");
            }
            break;
        case 13:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker13_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker13_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker13_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker13_Key_X ***\n");
            }
            break;
        case 14:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker14_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker14_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker14_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker14_Key_X ***\n");
            }
            break;
        case 15:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker15_X[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker15_X ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker15_Key_X[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker15_Key_X ***\n");
            }
            break;
        default:
            DBG(&g_i2c_client->dev, "*** Unknown VA Mapping! ***\n");
            n_ret_val = -1;
            break;
        }
    } else if (TP_TYPE_Y == g_msg28xx_tp_type) {
        switch (nDrvOpening) {
        case 4:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker4_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker4_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker4_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker4_Key_Y ***\n");
            }
            break;
        case 5:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker5_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker5_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker5_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker5_Key_Y ***\n");
            }
            break;
        case 6:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker6_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker6_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker6_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker6_Key_Y ***\n");
            }
            break;
        case 7:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker7_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker7_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker7_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker7_Key_Y ***\n");
            }
            break;
        case 8:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker8_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker8_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker8_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker8_Key_Y ***\n");
            }
            break;
        case 9:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker9_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker9_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker9_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker9_Key_Y ***\n");
            }
            break;
        case 10:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker10_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker10_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker10_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker10_Key_Y ***\n");
            }
            break;
        case 11:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker11_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker11_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker11_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker11_Key_Y ***\n");
            }
            break;
        case 12:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker12_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker12_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker12_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker12_Key_Y ***\n");
            }
            break;
        case 13:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker13_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker13_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker13_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker13_Key_Y ***\n");
            }
            break;
        case 14:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker14_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker14_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker14_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker14_Key_Y ***\n");
            }
            break;
        case 15:
            if (nScanMode == MSG28XX_KEY_SEPERATE) {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker15_Y[0][0];
                DBG(&g_i2c_client->dev, "*** g_MapVaMutual_Barker15_Y ***\n");
            } else {
                g_msg28xx_map_va_mutual = &g_MapVaMutual_Barker15_Key_Y[0][0];
                DBG(&g_i2c_client->dev,
                    "*** g_MapVaMutual_Barker15_Key_Y ***\n");
            }
            break;
        default:
            DBG(&g_i2c_client->dev, "*** Unknown VA Mapping! ***\n");
            n_ret_val = -1;
            break;
        }
    }

    return n_ret_val;
}

static u16 drv_mp_test_msg28xx_get_firmware_officia_version_on_flash(void)
{
    u16 n_ret_val = 0;
    u16 n_read_addr = 0;
    u8 szTmp_data[4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);

    drv_msg28xx_read_eflash_Start(0x81FE, EMEM_INFO);
    n_read_addr = 0x81FE;

    drv_msg28xx_read_eflash_do_read(n_read_addr, &szTmp_data[0]);

    drv_msg28xx_read_eflash_end();

    szTmp_data[1] = 0;           /*truncate useless char to transfer hex */
    n_ret_val = drv_mp_test_msg28xx_str_to_hex(szTmp_data);

    DBG(&g_i2c_client->dev,
        "SW ID = 0x%x, szTmp_data[0] = 0x%x, szTmp_data[1] = 0x%x, szTmp_data[2] = 0x%x, szTmp_data[3] = 0x%x\n",
        n_ret_val, szTmp_data[0], szTmp_data[1], szTmp_data[2], szTmp_data[3]);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    return n_ret_val;
}

static s32 drv_mp_test_msg28xx_obtain_open_value_va_fw_v1007(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (drv_mp_test_ito_test_msg28xx_get_deltac(g_mutual_ic_delta_c) < 0) {
        DBG(&g_i2c_client->dev,
            "*** drv_mp_test_ito_test_msg28xx_get_deltac failed! ***\n");
        return -1;
    }
    return 0;
}

static s32 drv_mp_test_msg28xx_obtain_open_value_keys_fw_v1007(int *pkeyarray)
{
    int k;
    u16 numKey = g_msg28xx_key_num;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    set_cfb(_50p);
    drv_mp_test_ito_open_test_msg28xx_on_cell_ana_enable_charge_pump(DISABLE);
    if (drv_mp_test_ito_open_test_msg228xx_get_value_r(g_mutual_ic_delta_cva) < 0) {
        DBG(&g_i2c_client->dev,
            "*** drv_mp_test_ito_open_test_msg228xx_get_value_r failed! ***\n");
        return -1;
    }

    if (MAX(numKey, 3) > 3)
        numKey = 3;

    for (k = 0; k < numKey; k++) {
        pkeyarray[k] = g_mutual_ic_delta_cva[k];
    }
    return 0;
}

static s32 drv_mp_test_msg28xx_open_previous_fw_v1007(u16 nFMode)
{
    s32 k;
    u16 nShift = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    reg_set_16bit_value(0x0FE6, 0x0001);

    if (nFMode == MUTUAL_SINE)
        drv_mp_test_ito_open_test_msg28xx_open_sine_mode_setting();
    else
        drv_mp_test_ito_open_test_msg28xx_open_swcap_mode_setting();

    if ((g_msg28xx_pattern_type == 5) && (g_msg28xx_pattern_model == 1) &&
        (g_msg28xx_key_num)) {
        if (drv_mp_test_ito_open_test_msg28xx_obtain_open_value_keys(g_key_array) < 0) {
            DBG(&g_i2c_client->dev,
                "*** drv_mp_test_ito_open_test_msg28xx_obtain_open_value_keys failed ***\n");
            return -1;
        }

        if (drv_mp_test_ito_open_test_msg28xx_re_enter_mutual_mode(nFMode)) {
            DBG(&g_i2c_client->dev,
                "*** drv_mp_test_ito_open_test_msg28xx_re_enter_mutual_mode failed ***\n");
            return -1;
        }

        if (nFMode == MUTUAL_SINE)
            drv_mp_test_ito_open_test_msg28xx_open_sine_mode_setting();
        else
            drv_mp_test_ito_open_test_msg28xx_open_swcap_mode_setting();

        if (drv_mp_test_ito_open_test_msg28xx_on_cell_open_va_value() < 0) {
            DBG(&g_i2c_client->dev,
                "*** drv_mp_test_ito_open_test_msg28xx_on_cell_open_va_value failed ***\n");
            return -1;
        }

        for (k = 0; k < g_msg28xx_key_num; k++) {
            nShift =
                (g_msg28xx_key_sen[k] - 1) * g_msg28xx_key_drv_o +
                g_msg28xx_key_drv_o - 1;
            g_mutual_ic_delta_c[nShift] = g_key_array[k];
        }
    } else {
        if (drv_mp_test_ito_test_msg28xx_get_deltac(g_mutual_ic_delta_c) < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Open Test# GetDeltaC failed! ***\n");
            return -1;
        }
    }

    return 0;
}

static s32 drv_mp_test_msg28xx_open_latter_fw_v1007(u16 nFMode)
{                               /*modifing */
    int k, numKey = 0;
    u16 nShift = 0, fmodeKey;;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop MCU */
    reg_set_l_byte_value(0x0FE6, 0x01);
    DBG(&g_i2c_client->dev, "*** nFMode = 0x%x ***", nFMode);

    numKey = g_msg28xx_key_num;

    if (nFMode == MUTUAL_SINE)
        drv_mp_test_ito_open_test_msg28xx_open_sine_mode_setting();
    else
        drv_mp_test_ito_open_test_msg28xx_open_swcap_mode_setting();

    if (MAX(numKey, 3) > 3)
        numKey = 3;
    if (g_msg28xx_mutual_key != 0) {

        if (nFMode == MUTUAL_SINE)
            fmodeKey = MUTUAL_SINE_KEY;
        else
            fmodeKey = MUTUAL_KEY;

        if (drv_mp_test_msg28xx_obtain_open_value_va_fw_v1007() < 0) {
            DBG(&g_i2c_client->dev,
                "*** drv_mp_test_msg28xx_obtain_open_value_va_fw_v1007 failed ***\n");
            return -1;
        }

        if (nFMode == MUTUAL_SINE)
            drv_mp_test_ito_open_test_msg28xx_open_sine_mode_setting();
        else
            drv_mp_test_ito_open_test_msg28xx_open_swcap_mode_setting();

        if (drv_mp_test_ito_open_test_msg28xx_re_enter_mutual_mode(fmodeKey) < 0) {
            DBG(&g_i2c_client->dev,
                "*** drv_mp_test_ito_open_test_msg28xx_re_enter_mutual_mode failed ***\n");
            return -1;
        }

        if (drv_mp_test_msg28xx_obtain_open_value_keys_fw_v1007(g_key_array) < 0) {
            DBG(&g_i2c_client->dev,
                "*** drv_mp_test_msg28xx_obtain_open_value_keys_fw_v1007 failed ***\n");
            return -1;
        }

        for (k = 0; k < numKey; k++) {
            nShift =
                (g_msg28xx_key_sen[k] - 1) * g_msg28xx_key_drv_o +
                g_msg28xx_key_drv_o - 1;
            g_mutual_ic_delta_c[nShift] = g_key_array[k];
        }

    } else {
        if (drv_mp_test_ito_test_msg28xx_get_deltac(g_mutual_ic_delta_c) < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Open Test# GetDeltaC failed! ***\n");
            return -1;
        }
    }

    return 0;
}

static s32 drv_mp_test_msg28xx_ito_open_test_entry(void)
{
    u16 nFwMode = MUTUAL_MODE;
    s32 n_ret_val = 0;
/*u8 nDrvOpening = 0;*/
    /*u16 nCheckState = 0; */
    u16 nTime = 0;
    s8 aNormalTest_result[8][2] = { {0} };   /*0:golden    1:ratio */
    u16 aNormalTest_resultCheck[6][13] = { {0} };    /*6:max subframe    13:max afe */
    s8 aOnCellNormalTest_result[2] = { 0 };  /*0:golden    1:ratio */
    u16 aOnCellNormalTest_resultCheck[MUTUAL_IC_MAX_MUTUAL_NUM] = { 0 }; /*6:max subframe    13:max afe */
    u16 nFwVer = 0;
    u16 nScanMode = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    if (g_msg28xx_pattern_type != 5) {
        g_msg28xx_deep_stand_by = 0;
    }
    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    if (!drv_mp_test_msg28xx_ito_test_choose_tp_type()) {
        DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
        n_ret_val = -2;
        goto ITO_TEST_END;
    }

    nFwVer = drv_mp_test_msg28xx_get_firmware_officia_version_on_flash();
    DBG(&g_i2c_client->dev, "*** nFwVer = 0x%02x ***", nFwVer);

    g_mutual_ic_sense_line_num = g_msg28xx_sense_num;
    g_mutual_ic_drive_line_num = g_msg28xx_drive_num;

_RETRY_OPEN:
    drv_touch_device_hw_reset();

    /*reset only */
    db_bus_reset_slave();
    db_bus_enter_serial_debug_mode();
    /*db_bus_wait_mcu(); */
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    mdelay(100);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);

    if (g_msg28xx_pattern_type == 5) {
        switch (g_msg28xx_open_mode) {
        case 0:
            nFwMode = MUTUAL_MODE;
            break;
        case 1:
            nFwMode = MUTUAL_SINE;
            break;
        }
    }

    if (drv_mp_test_msg28xx_ito_test_switch_fw_mode(&nFwMode) < 0) {
        nTime++;
        if (nTime < 5) {
            goto _RETRY_OPEN;
        }
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# Switch Fw Mode failed! ***\n");
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    nScanMode = reg_get_16bit_value_by_address_mode(0x136E, ADDRESS_MODE_16BIT);
    DBG(&g_i2c_client->dev, "*** nScanMode = 0x%x ***\n", nScanMode);
    g_is_dynamic_code = 1;

    if (g_msg28xx_pattern_type == 5) {
        if (nFwVer == 0x0007) {
            if ((nScanMode != MSG28XX_KEY_SEPERATE) &&
                (nScanMode != MSG28XX_KEY_COMBINE)) {
                nScanMode = MSG28XX_KEY_SEPERATE;
                g_is_dynamic_code = 0;
            }
        } else if (nFwVer < 0x0007) {
            nScanMode = MSG28XX_KEY_COMBINE;
            g_is_dynamic_code = 0;
        }
        if (g_is_dynamic_code && (g_msg28xx_AnaGen_Version[2] > 16)) {
            if (drv_mp_test_msg28xx_ito_test_choose_va_mapping(nScanMode) < 0) {
                DBG(&g_i2c_client->dev,
                    "*** drv_mp_test_msg28xx_ito_test_choose_va_mapping failed! ***\n");
                n_ret_val = -1;
                goto ITO_TEST_END;
            }
        }
        DBG(&g_i2c_client->dev,
            "*** g_is_dynamic_code = %d , g_msg28xx_AnaGen_Version = %d.%d.%d.%d ***\n",
            g_is_dynamic_code, g_msg28xx_AnaGen_Version[0],
            g_msg28xx_AnaGen_Version[1], g_msg28xx_AnaGen_Version[2],
            g_msg28xx_AnaGen_Version[3]);
        if (nScanMode == MSG28XX_KEY_SEPERATE) {
            DBG(&g_i2c_client->dev,
                "*** nFwMode = 0x%x , Msg28xxopen_latter_FW_v1007 ***",
                nFwMode);
            if (drv_mp_test_msg28xx_open_latter_fw_v1007(nFwMode) < 0) {
                DBG(&g_i2c_client->dev,
                    "*** Msg28xxopen_latter_FW_v1007 failed! ***\n");
                n_ret_val = -1;
                goto ITO_TEST_END;
            }
        } else {
            DBG(&g_i2c_client->dev,
                "*** nFwMode = 0x%x , Msg28xxopen_previous_FW_v1007 ***",
                nFwMode);
            if (drv_mp_test_msg28xx_open_previous_fw_v1007(nFwMode) < 0) {
                DBG(&g_i2c_client->dev,
                    "*** Msg28xxopen_previous_FW_v1007 failed! ***\n");
                n_ret_val = -1;
                goto ITO_TEST_END;
            }
        }
    } else {
        if (drv_mp_test_msg28xx_ito_open_test() < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Open Test# OpenTest failed! ***\n");
            n_ret_val = -1;
            goto ITO_TEST_END;
        }
    }

    mdelay(10);

    if (g_msg28xx_pattern_type == 5) {
        n_ret_val =
            drv_mp_test_ito_open_test_msg28xx_on_cell_open_judge(0,
                                                        aOnCellNormalTest_result,
                                                        aOnCellNormalTest_resultCheck
                                                        /*, nDrvOpening */ );
    } else {
        n_ret_val =
            drv_mp_test_ito_open_test_msg28xx_open_judge(0, aNormalTest_result,
                                                  aNormalTest_resultCheck
                                                  /*, nDrvOpening */ );
    }
    DBG(&g_i2c_client->dev,
        "*** Msg28xx Open Test# OpenTestOpenJudge return value = %d ***\n",
        n_ret_val);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

ITO_TEST_END:

    drv_touch_device_hw_reset();
    mdelay(300);

    drv_enable_finger_touch_report();

    return n_ret_val;
}

static void drv_mp_test_ito_open_test_msg228xx_set_noise_sensor_mode(u8 n_enable)
{
    s16 j;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (n_enable) {
        reg_set_16bit_value_on(0x1546, BIT4);
        for (j = 0; j < 10; j++) {
            reg_set_16bit_value(0x2148 + 2 * j, 0x0000);
        }
        reg_set_16bit_value(0x215C, 0x1FFF);
    }
}

static void drv_mp_test_ito_open_test_msg228xx_ana_fix_prs(u16 nOption)
{
    u16 nReg_data = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    retry_reg_set_16bit_value(0x0FE6, 0x0001);

    nReg_data = reg_get16_bit_value(0x1008);
    nReg_data &= 0x00F1;
    nReg_data |= (u16) ((nOption << 1) & 0x000E);
    reg_set_16bit_value(0x1008, nReg_data);
}

static void drv_mp_test_ito_open_test_msg228xx_change_ana_setting(void)
{
    int i, nMappingItem;
    u8 nChipVer;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*bank:mheg5, addr:h0073 */

    nChipVer = reg_get_l_byte_value(0x1ECE);
    /*uint chip_ver = Convert.ToUInt16((uint)regdata[0]); //for U01 (SF shift). */

    if (nChipVer != 0)
        reg_set_l_byte_value(0x131E, 0x01);

    for (nMappingItem = 0; nMappingItem < 6; nMappingItem++) {
        /*sensor mux sram read/write base address / write length */
        reg_set_l_byte_value(0x2192, 0x00);
        reg_set_l_byte_value(0x2102, 0x01);
        reg_set_l_byte_value(0x2102, 0x00);
        reg_set_l_byte_value(0x2182, 0x08);
        reg_set_l_byte_value(0x2180, 0x08 * nMappingItem);
        reg_set_l_byte_value(0x2188, 0x01);

        for (i = 0; i < 8; i++) {
            if (nMappingItem == 0 && nChipVer == 0x0) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_0_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_0_settings[2 * i + 1]);
            }
            if ((nMappingItem == 1 && nChipVer == 0x0) ||
                (nMappingItem == 0 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_1_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_1_settings[2 * i + 1]);
            }
            if ((nMappingItem == 2 && nChipVer == 0x0) ||
                (nMappingItem == 1 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_2_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_2_settings[2 * i + 1]);
            }
            if ((nMappingItem == 3 && nChipVer == 0x0) ||
                (nMappingItem == 2 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_3_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_3_settings[2 * i + 1]);
            }
            if ((nMappingItem == 4 && nChipVer == 0x0) ||
                (nMappingItem == 3 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_4_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_4_settings[2 * i + 1]);
            }
            if ((nMappingItem == 5 && nChipVer == 0x0) ||
                (nMappingItem == 4 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_5_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_5_settings[2 * i + 1]);
            }
            if (nMappingItem == 5 && nChipVer != 0x0) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_6_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_6_settings[2 * i + 1]);
            }
        }
    }
}

static void drv_mp_test_msg28xx_ito_read_setting(u16 *pPad2Sense, u16 *pPad2Drive,
                                            u16 *pPad2GR)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    memcpy(g_msg28xx_mux_mem_20_3e_1_settings, g_msg28xx_short_n1_mux_mem_20_3e,
           sizeof(u16) * 16);
    memcpy(g_msg28xx_mux_mem_20_3e_2_settings, g_msg28xx_short_n2_muc_mem_20_3e,
           sizeof(u16) * 16);
    memcpy(g_msg28xx_mux_mem_20_3e_3_settings, g_msg28xx_short_s1_mux_mem_20_3e,
           sizeof(u16) * 16);
    memcpy(g_msg28xx_mux_mem_20_3e_4_settings, g_msg28xx_short_s2_mux_mem_20_3e,
           sizeof(u16) * 16);

    if (g_msg28xx_short_test_5_type != 0) {
        if (g_msg28xx_pattern_type == 5) {
            memcpy(g_msg28xx_mux_mem_20_3e_5_settings,
                   g_msg28xx_short_gr_mux_mem_20_3e, sizeof(u16) * 16);
        } else {
            memcpy(g_msg28xx_mux_mem_20_3e_5_settings,
                   g_msg28xx_short_x_mux_mem_20_3e, sizeof(u16) * 16);
        }
    }

    memcpy(pPad2Sense, g_msg28xx_pad_table_sense,
           sizeof(u16) * g_mutual_ic_sense_line_num);
    memcpy(pPad2Drive, g_msg28xx_pad_table_drive,
           sizeof(u16) * g_mutual_ic_drive_line_num);

    if (g_msg28xx_gr_num != 0) {
        memcpy(pPad2GR, g_msg28xx_pad_table_gr, sizeof(u16) * g_msg28xx_gr_num);
    }
}

static void drv_mp_test_ito_open_test_msg228xx_on_cell_set_sensor_pad_state(u16 state)
{
    u16 value = 0, i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < 8; i++) {
        value |= (u16) (state << (i * 2));
    }

    for (i = 0; i < 8; i++) {
        reg_set_16bit_value_by_address_mode(0x1514 + i, value, ADDRESS_MODE_16BIT);
    }

    for (i = 0; i < 4; i++) {
        reg_set_16bit_value_by_address_mode(0x1510 + i, 0xFFFF, ADDRESS_MODE_16BIT);  /*After DAC overwrite, output DC */
    }
}

static void
drv_mp_test_ito_open_test_msg228xx_on_cell_patch_fw_ana_setting_for_short_test(void)
{
    u16 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*overwrite sensor PAD , restore to default state */
    for (i = 0; i < 8; i++) {
        reg_set_16bit_value_by_address_mode(0x1e33 + i, 0x0000, ADDRESS_MODE_16BIT);
    }

    /*overwrite PAD gpio , restore to default state */
    reg_set_16bit_value_by_address_mode(0x1e30, 0x000f, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1e31, 0x0000, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1e32, 0xffff, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1e3b, 0xffff, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1e3c, 0xffff, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1e3d, 0x003f, ADDRESS_MODE_16BIT);

    for (i = 0; i < 20; i++) {
        reg_set_16bit_value_by_address_mode(0x2110 + i, 0x0000, ADDRESS_MODE_16BIT);
    }

    for (i = 0; i < 16; i++) {
        reg_set_16bit_value_by_address_mode(0x2160 + i, 0x0000, ADDRESS_MODE_16BIT);
    }

    /*post idle for */
    reg_set_16bit_value_by_address_mode(0x101a, 0x0028, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x101b, 0x0028, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x101c, 0x0028, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x101d, 0x0028, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x101e, 0x0028, ADDRESS_MODE_16BIT);
}

static s32 drv_mp_test_msg28xx_ito_short_test(u8 nItemID)
{
    s16 i;
    u8 nReg_data = 0;
    u16 nAfeCoef = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*bank:mheg5, addr:h0073 */

    /*set Subframe = 6 ; Sensor = 13 */
    reg_set_l_byte_value(0x130A, 0x6D);
    reg_set_l_byte_value(0x1103, 0x06);
    reg_set_l_byte_value(0x1016, 0x0C);

    reg_set_l_byte_value(0x1104, 0x0C);
    reg_set_l_byte_value(0x100C, 0x0C);
    reg_set_l_byte_value(0x1B10, 0x0C);

    /*adc analog+digital pipe delay, 60= 13 AFE. */
    reg_set_l_byte_value(0x102F, 0x60);

    /*trim: Fout 52M &  1.2V */
    reg_set_16bit_value(0x1420, 0xA55A);   /*password */
    reg_set_16bit_value(0x1428, 0xA55A);   /*password */
    reg_set_16bit_value(0x1422, 0xFC4C);   /*go */

    drv_mp_test_ito_open_test_msg228xx_set_noise_sensor_mode(1);
    drv_mp_test_ito_open_test_msg228xx_ana_fix_prs(3);
    drv_mp_test_ito_test_msg28xx_ana_change_cd_time(0x007E, 0x0006);

    if (g_msg28xx_pattern_type != 5) {
        /*DAC overwrite */
        reg_set_16bit_value(0x150C, 0x80A2);   /*bit15 //AE:3.5v for test */
        reg_set_16bit_value(0x1520, 0xFFFF);   /*After DAC overwrite, output DC */
        reg_set_16bit_value(0x1522, 0xFFFF);
        reg_set_16bit_value(0x1524, 0xFFFF);
        reg_set_16bit_value(0x1526, 0xFFFF);
    }

    /*all AFE cfb use defalt (50p) */
    reg_set_16bit_value(0x1508, 0x1FFF);   /*all AFE cfb: SW control */
    reg_set_16bit_value(0x1550, 0x0000);   /*all AFE cfb use defalt (50p) */

    /*reg_afe_icmp disenable */
    reg_set_16bit_value(0x1552, 0x0000);

    /*reg_hvbuf_sel_gain */
    reg_set_16bit_value(0x1564, 0x0077);

    /*ADC: AFE Gain bypass */
    reg_set_16bit_value(0x1260, 0x1FFF);

    /*reg_sel_ros disenable */
    reg_set_16bit_value(0x156A, 0x0000);

    /*reg_adc_desp_invert disenable */
    reg_set_l_byte_value(0x1221, 0x00);

    if (g_msg28xx_pattern_type != 5) {
        /*AFE coef */
        nReg_data = reg_get_l_byte_value(0x101A);
        nAfeCoef = 0x10000 / nReg_data;
        reg_set_16bit_value(0x13D6, nAfeCoef);
    }

    /*AFE gain = 1X */
    /*reg_set_16bit_value(0x1318, 0x4440); */
    /*reg_set_16bit_value(0x131A, 0x4444); */
    /*reg_set_16bit_value(0x13D6, 0x2000); */

    if (g_msg28xx_pattern_type == 5) {
        /*AFE gain = 1X */
        reg_set_16bit_value(0x1318, 0x4440);
        reg_set_16bit_value(0x131A, 0x4444);

        reg_set_16bit_value_by_address_mode(0x1012, 0x0680, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1022, 0x0000, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x110A, 0x0104, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1310, 0x04F1, ADDRESS_MODE_16BIT);

        reg_set_16bit_value_by_address_mode(0x1317, 0x04F1, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1432, 0x0000, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1435, 0x0C00, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1538, 0x0400, ADDRESS_MODE_16BIT);

        reg_set_16bit_value_by_address_mode(0x1540, 0x0012, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1530, 0x0133, ADDRESS_MODE_16BIT);  /*HI v buf enable */
        /*reg_set_16bit_value_by_address_mode(0x1533, 0x0522, ADDRESS_MODE_16BIT);//low v buf gain */
        reg_set_16bit_value_by_address_mode(0x1533, 0x0000, ADDRESS_MODE_16BIT);  /*low v buf gain */
        reg_set_16bit_value_by_address_mode(0x1E11, 0x8000, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x2003, 0x007E, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x2006, 0x137F, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x213E, 0x1FFF, ADDRESS_MODE_16BIT);

        /*re-set sample and coefficient */
        reg_set_16bit_value_by_address_mode(0x100D, 0x0020, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1103, 0x0020, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1104, 0x0020, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1302, 0x0020, ADDRESS_MODE_16BIT);
        reg_set_16bit_value_by_address_mode(0x1B30, 0x0020, ADDRESS_MODE_16BIT);

        /*coefficient */
        reg_set_16bit_value_by_address_mode(0x136B, 0x10000 / 0x0020, ADDRESS_MODE_16BIT);    /*65536/ sample */

        drv_mp_test_ito_open_test_msg228xx_on_cell_set_sensor_pad_state(POS_PULSE);

        /*DAC overwrite */
        reg_set_16bit_value(0x150C, 0x8040);   /*bit15 //AFE:1.3v for test */
    }

    drv_mp_test_ito_open_test_msg228xx_change_ana_setting();
    drv_mp_test_ito_test_msg28xx_ana_sw_reset();

    if (g_msg28xx_pattern_type == 5) {
        drv_mp_test_ito_open_test_msg228xx_on_cell_patch_fw_ana_setting_for_short_test();
        memset(g_mutual_ic_delta_c, 0, sizeof(g_mutual_ic_delta_c));
    }
    if (drv_mp_test_ito_open_test_msg228xx_get_value_r(g_mutual_ic_delta_c) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# GetValueR failed! ***\n");
        return -1;
    }
    if (g_msg28xx_pattern_type == 5) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Ito Short Test# GetValueR 1.3v! ***\n");
        drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c, 65, -32, 10, 13);

        memset(g_mutual_ic_delta_c2, 0, sizeof(g_mutual_ic_delta_c2));
        reg_set_16bit_value(0x150C, 0x8083);   /*bit15 //AFE:3.72v for test */
        if (drv_mp_test_ito_open_test_msg228xx_get_value_r(g_mutual_ic_delta_c2) < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# GetValueR failed! ***\n");
            return -1;
        }
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Ito Short Test# GetValueR 3.72v! ***\n");
        drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c2, 65, -32, 10, 13);
    } else {
        drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c, 128, -32, 10, 8);
    }

    for (i = 0; i < 65; i++) {  /*13 AFE * 5 subframe */
        if (g_msg28xx_pattern_type == 5) {
            if ((abs(g_mutual_ic_delta_c2[i]) < MSG28XX_IIR_MAX) &&
                (abs(g_mutual_ic_delta_c[i]) < MSG28XX_IIR_MAX)) {
                g_mutual_ic_delta_c[i] =
                    g_mutual_ic_delta_c2[i] - g_mutual_ic_delta_c[i];
            } else {
                g_mutual_ic_delta_c[i] = g_mutual_ic_delta_c2[i];
            }

/*if (g_mutual_ic_delta_c[i] <= -1000 || g_mutual_ic_delta_c[i] >= (MSG28XX_IIR_MAX))*/
            if (g_mutual_ic_delta_c[i] >= (MSG28XX_IIR_MAX)) {
                g_mutual_ic_delta_c[i] = 0x7FFF;
            } else {
                g_mutual_ic_delta_c[i] = abs(g_mutual_ic_delta_c[i]);
            }
        } else {
            if (g_mutual_ic_delta_c[i] <= -1000 ||
                g_mutual_ic_delta_c[i] >= (MUTUAL_IC_IIR_MAX)) {
                g_mutual_ic_delta_c[i] = 0x7FFF;
            } else {
                g_mutual_ic_delta_c[i] = abs(g_mutual_ic_delta_c[i]);
            }
        }
    }

    if (g_msg28xx_pattern_type == 5) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Ito Short Test# GetValueR 3.72v - 1.3v ! ***\n");
        drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c, 65, -32, 10, 13);
    }

    return 0;
}

static s32 drv_mp_test_ito_open_test_msg228xx_read_test_pins(u8 nItemID,
                                                     u16 *pTestPins)
{
    u16 n_count = 0;
    s16 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    switch (nItemID) {
    case 1:
    case 11:
        n_count = g_msg28xx_short_n1_test_number;
        memcpy(pTestPins, g_msg28xx_short_n1_test_pin, sizeof(u16) * n_count);
        break;
    case 2:
    case 12:
        n_count = g_msg28xx_short_n2_test_number;
        memcpy(pTestPins, g_msg28xx_short_n2_test_pin, sizeof(u16) * n_count);
        break;
    case 3:
    case 13:
        n_count = g_msg28xx_short_s1_test_number;
        memcpy(pTestPins, g_msg28xx_short_s1_test_pin, sizeof(u16) * n_count);
        break;
    case 4:
    case 14:
        n_count = g_msg28xx_short_s2_test_number;
        memcpy(pTestPins, g_msg28xx_short_s2_test_pin, sizeof(u16) * n_count);
        break;

    case 5:
    case 15:
        if (g_msg28xx_short_test_5_type != 0) {
            n_count = g_msg28xx_short_x_test_number;
            if (g_msg28xx_pattern_type == 5) {
                memcpy(pTestPins, g_msg28xx_short_gr_test_pin,
                       sizeof(u16) * n_count);
            } else {
                memcpy(pTestPins, g_msg28xx_short_x_test_pin,
                       sizeof(u16) * n_count);
            }
        }
        break;

    case 0:
    default:
        return 0;
    }

    for (i = n_count; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
        pTestPins[i] = MSG28XX_PIN_NO_ERROR;    /*PIN_NO_ERROR */
    }

    return n_count;
}

static s32 drv_mp_test_ito_open_test_msg228xx_judge(u8 nItemID,
                                              /*s8 pNormalTest_result[][2], */
                                              u16 pTestPinMap[][13],
                                              u16 *pTestPin_count)
{
    s32 n_ret_val = 0;
    u16 nTestPins[MUTUAL_IC_MAX_CHANNEL_NUM];
    s16 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    *pTestPin_count =
        drv_mp_test_ito_open_test_msg228xx_read_test_pins(nItemID, nTestPins);
    /*drv_mp_test_mutual_ic_debug_show_array(nTestPins, *pTestPin_count, 16, 10, 8);*/
    if (*pTestPin_count == 0) {
        if (nItemID == 5 && g_msg28xx_short_test_5_type == 0) {

        } else {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# TestPin_count = 0 ***\n");
            return -1;
        }
    }

    /*
     * u16 n_countTestPin = 0;
     * for (i = 0; i < testPins.Length; i++)
     * {
     * if (pTestPins[i] != 0xFFFF)
     * n_countTestPin++;
     * }
     */

    for (i = (nItemID - 1) * 13; i < (13 * nItemID); i++) {
        g_mutual_ic_result[i] = g_mutual_ic_delta_c[i];
    }

    for (i = 0; i < *pTestPin_count; i++) {
        pTestPinMap[nItemID][i] = nTestPins[i];
        if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nItemID - 1) * 13], MSG28XX_SHORT_VALUE, -1000)) {   /*0: false   1: true */
            /*pNormalTest_result[nItemID][0] = -1;    //-1: failed   0: success */
            /*0: golden   1: ratio */
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# ShortTestMsg28xxJudge failed! ***\n");
            n_ret_val = -1;
        }
    }

    DBG(&g_i2c_client->dev, "*** Msg28xx Short Test# nItemID = %d ***\n",
        nItemID);
    /*drv_mp_test_mutual_ic_debug_show_array(pTestPinMap[nItemID], *pTestPin_count, 16, 10, 8);*/

    return n_ret_val;
}

static s32 drv_mp_test_ito_open_test_msg228xx_on_cell_judge(u8 nItemID,
                                                    /*s8 pNormalTest_result[][2], */
                                                    u16 pTestPinMap[][13],
                                                    s8 * TestFail,
                                                    u16 *pTestPin_count)
{
    s32 n_ret_val = 0;
    u16 aTestPins[MUTUAL_IC_MAX_CHANNEL_NUM];
    s16 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    *pTestPin_count =
        drv_mp_test_ito_open_test_msg228xx_read_test_pins(nItemID, aTestPins);
    /*drv_mp_test_mutual_ic_debug_show_array(aTestPins, *pTestPin_count, 16, 10, 8);*/
    if (*pTestPin_count == 0) {
        if (nItemID == 5 && g_msg28xx_short_test_5_type == 0) {

        } else {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# TestPin_count = 0 ***\n");
            return -1;
        }
    }

    for (i = (nItemID - 1) * 13; i < (13 * nItemID); i++) {
        g_mutual_ic_result[i] = g_mutual_ic_delta_c[i];
    }

    for (i = 0; i < *pTestPin_count; i++) {
        pTestPinMap[nItemID][i] = aTestPins[i];

/*if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nItemID - 1) * 13], MSG28XX_SHORT_VALUE, -1000))    //0: false   1: true*/
/*if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nItemID - 1) * 13], g_msg28xx_short_value, -1000))    //0: false   1: true*/
        if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nItemID - 1) * 13], g_msg28xx_short_value, -g_msg28xx_short_value)) {    /*0: false   1: true */
            /*pNormalTest_result[nItemID][0] = -1;    //-1: failed   0: success */
            /*0: golden   1: ratio */
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# ShortTestMsg28xxJudge failed! ***\n");
            TestFail[nItemID] = 1;
            n_ret_val = -1;
        }
    }

    DBG(&g_i2c_client->dev, "*** Msg28xx Short Test# nItemID = %d ***\n",
        nItemID);
    /*drv_mp_test_mutual_ic_debug_show_array(pTestPinMap[nItemID], *pTestPin_count, 16, 10, 8);*/

    return n_ret_val;
}

static s32 drv_mp_test_ito_open_test_msg228xx_covert_r_value(s32 n_value)
{
    if (n_value >= MUTUAL_IC_IIR_MAX) {
        return 0;
    }

    /*return ((3.53 - 1.3) * 10 / (50 * (((float)n_value - 0 ) / 32768 * 1.1))); */
    return 223 * 32768 / (n_value * 550);
}

static s32 drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value(s32 deltaR)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (deltaR >= MUTUAL_IC_IIR_MAX) {
        return 0;
    }

    if (deltaR == 0) {
        deltaR = 1;
    }

/*return ((3.72 - 1.3) * 2.15 * 32768 * 1.1 / (50 * (((double)deltaR - 0))));*/
    return 187541 / (50 * deltaR);
}

static void drv_mp_test_ic_pin_short_test_msg28xx_on_cell_prepare_ana(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*bank:mheg5, addr:h0073 */

    /*set Subframe = 6 ; Sensor = 13 */
    reg_set_l_byte_value(0x130A, 0x6D);
    reg_set_l_byte_value(0x1103, 0x06);
    reg_set_l_byte_value(0x1016, 0x0C);

    reg_set_l_byte_value(0x1104, 0x0C);
    reg_set_l_byte_value(0x100C, 0x0C);
    reg_set_l_byte_value(0x1B10, 0x0C);

    /*adc analog+digital pipe delay, 60= 13 AFE. */
    reg_set_l_byte_value(0x102F, 0x60);

    /*trim: Fout 52M &  1.2V */
    reg_set_16bit_value(0x1420, 0xA55A);   /*password */
    reg_set_16bit_value(0x1428, 0xA55A);   /*password */
    reg_set_16bit_value(0x1422, 0xFC4C);   /*go */

    drv_mp_test_ito_open_test_msg228xx_set_noise_sensor_mode(1);
    drv_mp_test_ito_open_test_msg228xx_ana_fix_prs(3);
    drv_mp_test_ito_test_msg28xx_ana_change_cd_time(0x007E, 0x0006);

    /*all AFE cfb use defalt (50p) */
    reg_set_16bit_value(0x1508, 0x1FFF);   /*all AFE cfb: SW control */
    reg_set_16bit_value(0x1550, 0x0000);   /*all AFE cfb use defalt (50p) */

    /*reg_afe_icmp disenable */
    reg_set_16bit_value(0x1552, 0x0000);

    /*reg_hvbuf_sel_gain */
    reg_set_16bit_value(0x1564, 0x0077);

    /*ADC: AFE Gain bypass */
    reg_set_16bit_value(0x1260, 0x1FFF);

    /*reg_sel_ros disenable */
    reg_set_16bit_value(0x156A, 0x0000);

    /*reg_adc_desp_invert disenable */
    reg_set_l_byte_value(0x1221, 0x00);

    /*AFE gain = 1X */
    reg_set_16bit_value(0x1318, 0x4440);
    reg_set_16bit_value(0x131A, 0x4444);

    reg_set_16bit_value_by_address_mode(0x1012, 0x0680, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1022, 0x0000, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x110A, 0x0104, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1310, 0x04F1, ADDRESS_MODE_16BIT);

    reg_set_16bit_value_by_address_mode(0x1317, 0x04F1, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1432, 0x0000, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1435, 0x0C00, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1538, 0x0400, ADDRESS_MODE_16BIT);

    reg_set_16bit_value_by_address_mode(0x1540, 0x0012, ADDRESS_MODE_16BIT);
     /**/ reg_set_16bit_value_by_address_mode(0x1530, 0x0133, ADDRESS_MODE_16BIT);    /*HI v buf enable */
/*reg_set_16bit_value_by_address_mode(0x1533, 0x0522, ADDRESS_MODE_16BIT);//low v buf gain*/
    reg_set_16bit_value_by_address_mode(0x1533, 0x0000, ADDRESS_MODE_16BIT);  /*low v buf gain */
    reg_set_16bit_value_by_address_mode(0x1E11, 0x8000, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x2003, 0x007E, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x2006, 0x137F, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x213E, 0x1FFF, ADDRESS_MODE_16BIT);

    /*re-set sample and coefficient */
    reg_set_16bit_value_by_address_mode(0x100D, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1103, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1104, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1302, 0x0020, ADDRESS_MODE_16BIT);
    reg_set_16bit_value_by_address_mode(0x1B30, 0x0020, ADDRESS_MODE_16BIT);

    /*coefficient */
    reg_set_16bit_value_by_address_mode(0x136B, 0x10000 / 0x0020, ADDRESS_MODE_16BIT);    /*65536/ sample */
}

static void drv_mp_test_ic_pin_short_test_msg28xx_on_cell_change_ana_setting(void)
{
    int i, nMappingItem;
    u8 nChipVer;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);

    nChipVer = reg_get_l_byte_value(0x1ECE);

    if (nChipVer != 0) {
        reg_set_l_byte_value(0x131E, 0x01);
    }

    memcpy(g_msg28xx_mux_mem_20_3e_1_settings,
           g_msg28xx_short_ic_pin_mux_mem_1_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_1_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_2_settings,
           g_msg28xx_short_ic_pin_mux_mem_2_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_2_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_3_settings,
           g_msg28xx_short_ic_pin_mux_mem_3_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_3_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_4_settings,
           g_msg28xx_short_ic_pin_mux_mem_4_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_4_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_5_settings,
           g_msg28xx_short_ic_pin_mux_mem_5_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_5_settings));

    for (nMappingItem = 0; nMappingItem < 6; nMappingItem++) {
        /*sensor mux sram read/write base address / write length */
        reg_set_l_byte_value(0x2192, 0x00);
        reg_set_l_byte_value(0x2102, 0x01);
        reg_set_l_byte_value(0x2102, 0x00);
        reg_set_l_byte_value(0x2182, 0x08);
        reg_set_l_byte_value(0x2180, 0x08 * nMappingItem);
        reg_set_l_byte_value(0x2188, 0x01);

        for (i = 0; i < 8; i++) {
            if (nMappingItem == 0 && nChipVer == 0x0) {
                memset(g_msg28xx_mux_mem_20_3e_0_settings, 0,
                       sizeof(g_msg28xx_mux_mem_20_3e_0_settings));
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_0_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_0_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_0_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_0_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_0_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_0_settings[2 * i + 1]);
            }

            if ((nMappingItem == 1 && nChipVer == 0x0) ||
                (nMappingItem == 0 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_1_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_1_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_1_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_1_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_1_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_1_settings[2 * i + 1]);
            }

            if ((nMappingItem == 2 && nChipVer == 0x0) ||
                (nMappingItem == 1 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_2_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_2_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_2_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_2_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_2_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_2_settings[2 * i + 1]);
            }

            if ((nMappingItem == 3 && nChipVer == 0x0) ||
                (nMappingItem == 2 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_3_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_3_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_3_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_3_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_3_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_3_settings[2 * i + 1]);
            }

            if ((nMappingItem == 4 && nChipVer == 0x0) ||
                (nMappingItem == 3 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_4_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_4_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_4_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_4_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_4_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_4_settings[2 * i + 1]);
            }

            if ((nMappingItem == 5 && nChipVer == 0x0) ||
                (nMappingItem == 4 && nChipVer != 0x0)) {
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_5_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_5_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_5_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_5_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_5_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_5_settings[2 * i + 1]);
            }

            if (nMappingItem == 5 && nChipVer != 0x0) {
                memset(g_msg28xx_mux_mem_20_3e_6_settings, 0,
                       sizeof(g_msg28xx_mux_mem_20_3e_6_settings));
                reg_set_16bit_value(0x218A,
                                 g_msg28xx_mux_mem_20_3e_6_settings[2 * i]);
                reg_set_16bit_value(0x218C,
                                 g_msg28xx_mux_mem_20_3e_6_settings[2 * i + 1]);
                DBG(&g_i2c_client->dev,
                    "g_msg28xx_mux_mem_20_3e_6_settings[%d] = %x, g_msg28xx_mux_mem_20_3e_6_settings[%d] = %x\n",
                    2 * i, g_msg28xx_mux_mem_20_3e_6_settings[2 * i], 2 * i + 1,
                    g_msg28xx_mux_mem_20_3e_6_settings[2 * i + 1]);
            }
        }
    }
}

static void drv_mp_test_ic_pin_short_test_msg28xx_on_cell_ana_load_setting(void)
{
    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);
    drv_mp_test_ic_pin_short_test_msg28xx_on_cell_change_ana_setting();
}

static s32 drv_mp_test_ic_pin_short_test_msg28xx_on_cellIc_pin_short(void)
{
    s16 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_mp_test_ic_pin_short_test_msg28xx_on_cell_prepare_ana();
    drv_mp_test_ito_open_test_msg228xx_on_cell_set_sensor_pad_state(POS_PULSE);

    /*DAC overwrite */
    reg_set_16bit_value(0x150C, 0x8040);   /*bit15 //AFE:1.3v for test */

    drv_mp_test_ic_pin_short_test_msg28xx_on_cell_ana_load_setting();
    drv_mp_test_ito_test_msg28xx_ana_sw_reset();
    drv_mp_test_ito_open_test_msg228xx_on_cell_patch_fw_ana_setting_for_short_test();

    memset(g_mutual_ic_delta_c, 0, sizeof(g_mutual_ic_delta_c));

    if (drv_mp_test_ito_open_test_msg228xx_get_value_r(g_mutual_ic_delta_c) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx IC Pin Short Test# GetValueR failed! ***\n");
        return -1;
    }

    DBG(&g_i2c_client->dev,
        "*** Msg28xx IC Pin Short Test# GetValueR 1.3v! ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c, 65, -32, 10, 13);

    memset(g_mutual_ic_delta_c2, 0, sizeof(g_mutual_ic_delta_c2));

    /*DAC overwrite */
    reg_set_16bit_value(0x150C, 0x8083);   /*bit15 //AFE:3.72v for test */

    if (drv_mp_test_ito_open_test_msg228xx_get_value_r(g_mutual_ic_delta_c2) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# GetValueR failed! ***\n");
        return -1;
    }

    DBG(&g_i2c_client->dev,
        "*** Msg28xx IC Pin Short Test# GetValueR 3.72v! ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c2, 65, -32, 10, 13);

    for (i = 0; i < 65; i++) {  /*13 AFE * 5 subframe */
        if ((abs(g_mutual_ic_delta_c2[i]) < MSG28XX_IIR_MAX) &&
            (abs(g_mutual_ic_delta_c[i]) < MSG28XX_IIR_MAX)) {
            g_mutual_ic_delta_c[i] = g_mutual_ic_delta_c2[i] - g_mutual_ic_delta_c[i];
        } else {
            g_mutual_ic_delta_c[i] = g_mutual_ic_delta_c2[i];
        }

/*if (g_mutual_ic_delta_c[i] <= -1000 || g_mutual_ic_delta_c[i] >= (MSG28XX_IIR_MAX)) // -1000 is fout bias issue*/
        if (g_mutual_ic_delta_c[i] >= (MSG28XX_IIR_MAX)) {
            g_mutual_ic_delta_c[i] = 0x7FFF;
        } else {
            g_mutual_ic_delta_c[i] = abs(g_mutual_ic_delta_c[i]);
        }
    }

    DBG(&g_i2c_client->dev,
        "*** Msg28xx IC Pin Short Test# GetValueR 3.72v - 1.3v ! ***\n");
    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_delta_c, 65, -32, 10, 13);

    return 0;
}

static s32 drv_mp_test_ic_pin_short_test_msg28xx_on_cell_read_mapping(u16
                                                            testPin_data[][13])
{
    int i, j, nMappingItem;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    memcpy(g_msg28xx_mux_mem_20_3e_1_settings,
           g_msg28xx_short_ic_pin_mux_mem_1_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_1_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_2_settings,
           g_msg28xx_short_ic_pin_mux_mem_2_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_2_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_3_settings,
           g_msg28xx_short_ic_pin_mux_mem_3_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_3_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_4_settings,
           g_msg28xx_short_ic_pin_mux_mem_4_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_4_settings));
    memcpy(g_msg28xx_mux_mem_20_3e_5_settings,
           g_msg28xx_short_ic_pin_mux_mem_5_settings,
           sizeof(g_msg28xx_mux_mem_20_3e_5_settings));

    for (nMappingItem = 1; nMappingItem < 6; nMappingItem++) {
        u16 testpin = 1;
        u16 index = 0;

        for (i = 0;
             i <
             sizeof(g_msg28xx_mux_mem_20_3e_1_settings) /
             sizeof(g_msg28xx_mux_mem_20_3e_1_settings[0]); i++) {
            for (j = 0; j < 4; j++) {
                if (nMappingItem == 1) {
                    if (((g_msg28xx_mux_mem_20_3e_1_settings[i] >> (4 * j)) &
                         0x0F) != 0) {
                        testPin_data[nMappingItem][index] = testpin;
                        index++;
                    }
                } else if (nMappingItem == 2) {
                    if (((g_msg28xx_mux_mem_20_3e_2_settings[i] >> (4 * j)) &
                         0x0F) != 0) {
                        testPin_data[nMappingItem][index] = testpin;
                        index++;
                    }
                } else if (nMappingItem == 3) {
                    if (((g_msg28xx_mux_mem_20_3e_3_settings[i] >> (4 * j)) &
                         0x0F) != 0) {
                        testPin_data[nMappingItem][index] = testpin;
                        index++;
                    }
                } else if (nMappingItem == 4) {
                    if (((g_msg28xx_mux_mem_20_3e_4_settings[i] >> (4 * j)) &
                         0x0F) != 0) {
                        testPin_data[nMappingItem][index] = testpin;
                        index++;
                    }
                } else if (nMappingItem == 5) {
                    if (((g_msg28xx_mux_mem_20_3e_5_settings[i] >> (4 * j)) &
                         0x0F) != 0) {
                        testPin_data[nMappingItem][index] = testpin;
                        index++;
                    }
                }
                testpin++;
            }                   /*for (j = 0; j < 4; j++) */
        }                       /*for (i = 0; i < MUX_MEM_1_settings.Length; i++) */
    }                           /*for (nMappingItem = 1; nMappingItem < 6; nMappingItem++) */

    return 1;
}

static s32 drv_mp_test_ic_pin_short_test_msg28xx_on_cell_short_test_judge(u16 nItemID,
                                                               u16 pTestPinMap[]
                                                               [13],
                                                               s8 * TestFail)
{
    int n_ret = 1, i, count_test_pin = 0, j;
    u16 GRPins[13] = { 0 }, GR_Id[13] = {
    0};
    int found_gr = 0, count = 0, BypassGR = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (!drv_mp_test_ic_pin_short_test_msg28xx_on_cell_read_mapping(pTestPinMap)) {
        return 0;
    }

    if (g_msg28xx_gr_num == 0) {
        BypassGR = 1;
    }

    for (i = 0; i < sizeof(GRPins) / sizeof(u16); i++) {
        GRPins[i] = 0xFFFF;
    }

    for (i = 0; i < g_msg28xx_short_x_test_number; i++) {
        GRPins[i] = g_msg28xx_short_gr_test_pin[i];
    }
    DBG(&g_i2c_client->dev, "GRPins[0]= %d\n", GRPins[0]);

    if (g_msg28xx_short_x_test_number) {
        DBG(&g_i2c_client->dev, "*** %s()_1 ***\n", __func__);
        for (j = 0; j < sizeof(GRPins) / sizeof(u16); j++) {
            DBG(&g_i2c_client->dev, "*** %s()_2 ***\n", __func__);

            if (GRPins[j] == 0xFFFF) {
                continue;
            }

            for (i = 0; i < 13; i++) {
                DBG(&g_i2c_client->dev, "pTestPinMap[%d][%d] = %d\n", nItemID,
                    i, pTestPinMap[nItemID][i]);

                if ((pTestPinMap[nItemID][i] != MSG28XX_UN_USE_PIN) &&
                    (pTestPinMap[nItemID][i] != 0)) {
                    DBG(&g_i2c_client->dev, "pTestPinMap[%d][%d] = %d\n",
                        nItemID, i, pTestPinMap[nItemID][i]);
                    if (pTestPinMap[nItemID][i] == (GRPins[j] + 1)) {
                        DBG(&g_i2c_client->dev, "pTestPinMap[%d][%d] = %d\n",
                            nItemID, i, pTestPinMap[nItemID][i]);
                        GR_Id[count] = i;
                        found_gr = 1;

                        if (count < sizeof(GR_Id) / sizeof(u16)) {
                            count++;
                        }
                    }
                } else {
                    break;
                }
            }
        }
    }

    count = 0;

    for (i = (nItemID - 1) * 13; i < (13 * nItemID); i++) {
        if ((found_gr == 1) && (i == GR_Id[count] + ((nItemID - 1) * 13)) &&
            (BypassGR == 1)) {
            g_mutual_ic_delta_c[i] = 1;

            if (count < sizeof(GR_Id) / sizeof(u16)) {
                count++;
            }
        }
    }

    for (i = 0; i < sizeof(pTestPinMap) / sizeof(pTestPinMap[0]); i++) {
        if (pTestPinMap[nItemID][i] != MSG28XX_PIN_NO_ERROR) {
            count_test_pin++;
        }
    }

    for (i = 0; i < count_test_pin; i++) {
/*if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_delta_c[i + (nItemID - 1) * 13], g_msg28xx_ic_pin_short, -1000))*/
        if (0 ==
            drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_delta_c
                                                [i + (nItemID - 1) * 13],
                                                g_msg28xx_ic_pin_short,
                                                -g_msg28xx_ic_pin_short)) {
            TestFail[nItemID] = 1;
        }
    }

    return n_ret;
}

static s32 drv_mp_test_ic_pin_short_test_msg28xx_on_cell_result_prepare(s32 thrs,
                                                              u16 * senseR)
{
    u16 count = 0, i, n_ret = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
        g_ic_pin_short_fail_channel[i] = 0;

        if (g_msg28xx_sense_pad_pin_mapping[i] != 0) {
            DBG(&g_i2c_client->dev,
                "pICPinChannel[%d] = %d, IC Pin P[%d] = %dM, deltaC = %d\n", i,
                g_msg28xx_sense_pad_pin_mapping[i],
                g_msg28xx_sense_pad_pin_mapping[i], senseR[i],
                g_ic_pin_short_result_data[i]);

            if (senseR[i] < thrs) {
                g_ic_pin_short_fail_channel[i] = g_msg28xx_sense_pad_pin_mapping[i];
                DBG(&g_i2c_client->dev, "IC Pin Fail P%d = %dM",
                    g_msg28xx_sense_pad_pin_mapping[i], senseR[i]);
                count++;
                n_ret = -1;
            }
        }
    }                           /*for (int i = 0; i < senseR.Length; i++) */

    return n_ret;
}

static s32 drv_mp_test_ic_pin_short_test_msg28xx_on_cellIc_pin_short_test_entry(void)
{
    u16 nFwMode = MUTUAL_SINGLE_DRIVE;
    s16 i = 0, j = 0, count_test_pin = 0;
    u16 *pPad2Drive = NULL;
    u16 *pPad2Sense = NULL;
    u16 *pDriveR = NULL;
    u16 *pSenseR = NULL;
    u16 *pGRR = NULL;
    u16 nTime = 0;
    u16 nTestItemLoop = 6;
    u16 nTestItem = 0;
    u16 aTestPinMap[6][13] = { {0} };   /*6:max subframe    13:max afe */
    u32 n_ret_val = 0;
    s32 thrs = 0;
    s8 aNormalTestFail[MSG28XX_TEST_ITEM_NUM] = { 0 };  /*0:golden    1:ratio */

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    if (!drv_mp_test_msg28xx_ito_test_choose_tp_type()) {
        DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
        drv_touch_device_hw_reset();
        drv_enable_finger_touch_report();
        return -2;
    }

    pPad2Drive = kzalloc(sizeof(s16) * g_msg28xx_drive_num, GFP_KERNEL);
    pPad2Sense = kzalloc(sizeof(s16) * g_msg28xx_sense_num, GFP_KERNEL);
    pDriveR = kzalloc(sizeof(s16) * g_msg28xx_drive_num, GFP_KERNEL);
    pSenseR = kzalloc(sizeof(s16) * g_msg28xx_sense_num, GFP_KERNEL);
    pGRR = kzalloc(sizeof(s16) * g_msg28xx_gr_num, GFP_KERNEL);

    g_mutual_ic_sense_line_num = g_msg28xx_sense_num;
    g_mutual_ic_drive_line_num = g_msg28xx_drive_num;

_RETRY_SHORT:
    drv_touch_device_hw_reset();

    /*reset only */
/*db_bus_reset_slave();*/
    db_bus_enter_serial_debug_mode();
/*db_bus_wait_mcu();*/
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    mdelay(100);

    /*Start mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0000);

    if (drv_mp_test_msg28xx_ito_test_switch_fw_mode(&nFwMode) < 0) {
        nTime++;
        if (nTime < 5) {
            goto _RETRY_SHORT;
        }
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# Switch Fw Mode failed! ***\n");
        g_ic_pin_short_check_fail = 1;
        n_ret_val = -1;

        goto ITO_TEST_END;
    }

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*bank:mheg5, addr:h0073 */

    for (i = 0; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
        pSenseR[i] = MSG28XX_PIN_NO_ERROR;
    }

    thrs =
        drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value(g_msg28xx_ic_pin_short);

    for (i = 1; i < 6; i++) {   /*max 6 subframe */
        for (j = 0; j < 13; j++) {  /*max 13 AFE */
            if (((i - 1) * 13 + j) < MUTUAL_IC_MAX_CHANNEL_NUM) {   /*prevent heap corruption detected */
                g_ic_pin_short_fail_channel[(i - 1) * 13 + j] = MSG28XX_UN_USE_PIN;
            }

            aTestPinMap[i][j] = MSG28XX_UN_USE_PIN;
        }
    }

    if (drv_mp_test_ic_pin_short_test_msg28xx_on_cellIc_pin_short() < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# Get DeltaC failed! ***\n");
        g_ic_pin_short_check_fail = 1;
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    for (nTestItem = 1; nTestItem < nTestItemLoop; nTestItem++) {
        DBG(&g_i2c_client->dev, "*** Short test item %d ***\n", nTestItem);
        if (drv_mp_test_ic_pin_short_test_msg28xx_on_cell_short_test_judge
            (nTestItem, /*aNormalTest_result, */ aTestPinMap,
             aNormalTestFail) < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# Item %d is failed! ***\n", nTestItem);
            g_ic_pin_short_check_fail = 1;
            n_ret_val = -1;
            goto ITO_TEST_END;
        }

        count_test_pin = 0;

        for (i = 0; i < 13; i++) {
            if (aTestPinMap[nTestItem][i] != MSG28XX_UN_USE_PIN) {
                /*DEBUG("normalTestFail_check[%d, %d] = %d", nTestItem, i, testPin_data[nTestItem][i]); */
                count_test_pin++;
            }
        }

        if ((nTestItem > 0) && (nTestItem < 6)) {
            for (i = 0; i < count_test_pin; i++) {
                if ((aTestPinMap[nTestItem][i] > 0) &&
                    (aTestPinMap[nTestItem][i] <= MUTUAL_IC_MAX_CHANNEL_NUM)) {
                    pSenseR[aTestPinMap[nTestItem][i] - 1] =
                        drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value
                        (g_mutual_ic_delta_c[i + (nTestItem - 1) * 13]);
                    g_ic_pin_short_result_data[aTestPinMap[nTestItem][i] - 1] =
                        g_mutual_ic_delta_c[i + (nTestItem - 1) * 13];
                    DBG(&g_i2c_client->dev,
                        "pSenseR[%d]={%d}, _gDeltaC[{%d}]={%d}",
                        aTestPinMap[nTestItem][i] - 1,
                        pSenseR[aTestPinMap[nTestItem][i] - 1],
                        i + (nTestItem - 1) * 13,
                        g_mutual_ic_delta_c[i + (nTestItem - 1) * 13]);
                } else {
                    n_ret_val = -1;
                    g_ic_pin_short_check_fail = 1;  /*ic pin short fail */
                    goto ITO_TEST_END;
                }
            }
        }                       /*if ((nTestItem > 0) && (nTestItem < 6)) */
    }

    if (drv_mp_test_ic_pin_short_test_msg28xx_on_cell_result_prepare(thrs, pSenseR) < 0) {
        g_ic_pin_short_check_fail = 1;  /*ic pin short fail */
        n_ret_val = -1;
    }

    memcpy(g_ic_pin_short_sence_r, pSenseR, sizeof(g_ic_pin_short_sence_r));

    DBG(&g_i2c_client->dev, "*** %s()g_ic_pin_short_fail_channel:  ***\n",
        __func__);
    drv_mp_test_mutual_ic_debug_show_array(g_ic_pin_short_fail_channel, 60, -32, 10, 10);
    DBG(&g_i2c_client->dev, "*** %s()g_ic_pin_short_result_data:  ***\n", __func__);
    drv_mp_test_mutual_ic_debug_show_array(g_ic_pin_short_result_data, 60, -32, 10, 10);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

ITO_TEST_END:

    drv_touch_device_hw_reset();
    mdelay(300);

    drv_enable_finger_touch_report();
    kfree(pPad2Sense);
    kfree(pPad2Drive);

    return n_ret_val;
}

static s32 drv_mp_test_ito_open_test_msg228xx_on_cellI_to_short_test_entry(void)
{
    u16 nFwMode = MUTUAL_SINGLE_DRIVE;
    s16 i = 0, j = 0;
    u16 *pPad2Drive = NULL;
    u16 *pPad2Sense = NULL;
    u16 *pDriveR = NULL;
    u16 *pSenseR = NULL;
    u16 *pGRR = NULL;
    u16 nTime = 0;
    u16 aPad2GR[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };

    u16 nTestItemLoop = 6;
    u16 nTestItem = 0;
    u16 aTestPinMap[6][13] = { {0} };   /*6:max subframe    13:max afe */
    u16 nTestPinNum = 0;
    u32 n_ret_val = 0;
    s32 thrs = 0;
    s8 aNormalTestFail[MSG28XX_TEST_ITEM_NUM] = { 0 };  /*0:golden    1:ratio */

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    if (!drv_mp_test_msg28xx_ito_test_choose_tp_type()) {
        DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
        drv_touch_device_hw_reset();
        drv_enable_finger_touch_report();
        return -2;
    }

    pPad2Drive = kzalloc(sizeof(s16) * g_msg28xx_drive_num, GFP_KERNEL);
    pPad2Sense = kzalloc(sizeof(s16) * g_msg28xx_sense_num, GFP_KERNEL);
    pDriveR = kzalloc(sizeof(s16) * g_msg28xx_drive_num, GFP_KERNEL);
    pSenseR = kzalloc(sizeof(s16) * g_msg28xx_sense_num, GFP_KERNEL);
    pGRR = kzalloc(sizeof(s16) * g_msg28xx_gr_num, GFP_KERNEL);

    g_mutual_ic_sense_line_num = g_msg28xx_sense_num;
    g_mutual_ic_drive_line_num = g_msg28xx_drive_num;

_RETRY_SHORT:
    drv_touch_device_hw_reset();

    /*reset only */
/*db_bus_reset_slave();*/
    db_bus_enter_serial_debug_mode();
/*db_bus_wait_mcu();*/
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    mdelay(100);

    /*Start mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0000);  /*bank:mheg5, addr:h0073 */

    if (drv_mp_test_msg28xx_ito_test_switch_fw_mode(&nFwMode) < 0) {
        nTime++;
        if (nTime < 5) {
            goto _RETRY_SHORT;
        }
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# Switch Fw Mode failed! ***\n");
        g_ito_short_checK_fail = 1;
        n_ret_val = -1;

        goto ITO_TEST_END;
    }

    db_bus_stop_mcu();

    drv_mp_test_msg28xx_ito_read_setting(pPad2Sense, pPad2Drive, aPad2GR);

    thrs =
        drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value(g_msg28xx_short_value);

    for (i = 0; i < g_mutual_ic_sense_line_num; i++) {
        pSenseR[i] = thrs;
    }
    for (i = 0; i < g_mutual_ic_drive_line_num; i++) {
        pDriveR[i] = thrs;
    }
    for (i = 0; i < g_msg28xx_gr_num; i++) {
        pGRR[i] = thrs;
    }

    for (i = 1; i < 6; i++) {   /*max 6 subframe */
        for (j = 0; j < 13; j++) {  /*max 13 AFE */
            if (((i - 1) * 13 + j) < MUTUAL_IC_MAX_CHANNEL_NUM) {
                g_ito_short_fail_channel[(i - 1) * 13 + j] =
                    (u32) MSG28XX_UN_USE_PIN;
            }
            aTestPinMap[i][j] = MSG28XX_UN_USE_PIN;
        }
    }

    /*N1_ShortTest */
    if (drv_mp_test_msg28xx_ito_short_test(1) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# Get DeltaC failed! ***\n");
        g_ito_short_checK_fail = 1;
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    for (nTestItem = 1; nTestItem < nTestItemLoop; nTestItem++) {
        DBG(&g_i2c_client->dev, "*** Short test item %d ***\n", nTestItem);
        if (drv_mp_test_ito_open_test_msg228xx_on_cell_judge
            (nTestItem, /*aNormalTest_result, */ aTestPinMap, aNormalTestFail,
             &nTestPinNum) < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# Item %d is failed! ***\n", nTestItem);
            n_ret_val = -1;
            goto ITO_TEST_END;
        }

        if (nTestItem == 1 || nTestItem == 2 ||
            (nTestItem == 5 && g_msg28xx_short_test_5_type == 1)) {
            DBG(&g_i2c_client->dev, "SHORT_TEST_N3");
            for (i = 0; i < nTestPinNum; i++) {
                for (j = 0; j < g_mutual_ic_sense_line_num; j++) {
                    if (aTestPinMap[nTestItem][i] == pPad2Sense[j]) {
                        /*g_mutual_ic_sense_r[j] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(g_mutual_ic_result[i + (nTestItem - 1) * 13]);*/
/*g_mutual_ic_sense_r[j] = g_mutual_ic_result[i + (nTestItem - 1) * 13];    //change comparison way because float computing in driver is prohibited*/

                        pSenseR[j] =
                            drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value
                            (g_mutual_ic_result[i + (nTestItem - 1) * 13]);
                        DBG(&g_i2c_client->dev,
                            "senseR[%d] = %d, _gDeltaC[%d] = %d\n", j,
                            pSenseR[j], i + (nTestItem - 1) * 13,
                            g_mutual_ic_result[i + (nTestItem - 1) * 13]);

                        g_ito_short_r_data[j] = pSenseR[j];
                        g_ito_short_result_data[j] =
                            g_mutual_ic_result[i + (nTestItem - 1) * 13];

                        if (pSenseR[j] >= 10) {
                            g_ito_short_r_data[j] = 10;
                        }

/*if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nTestItem - 1) * 13], g_msg28xx_short_value, -1000))*/
                        if (0 ==
                            drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result
                                                                [i +
                                                                 (nTestItem -
                                                                  1) * 13],
                                                                g_msg28xx_short_value,
                                                                -g_msg28xx_short_value))
                        {
                            g_ito_short_fail_channel[j] =
                                (u32) aTestPinMap[nTestItem][i];
                            /*DEBUG("Ito Short senseR, count_test_pin = %d, normalTestFail_check[%d][%d] = %d, pShortFailChannel[%d] = %d, _gDeltaC[%d] = %d", count_test_pin, nTestItem, i, normalTestFail_check[nTestItem][i], j, ptMutualMpTest_result->pShortFailChannel[j], i + (nTestItem - 1) * 13, _gDeltaC[i + (nTestItem - 1) * 13]); */
                        }
                    }
                }
            }
        }

        if (nTestItem == 3 || nTestItem == 4 ||
            (nTestItem == 5 && g_msg28xx_short_test_5_type == 2)) {
            DBG(&g_i2c_client->dev, "SHORT_TEST_S3");
            for (i = 0; i < nTestPinNum; i++) {
                for (j = 0; j < g_mutual_ic_drive_line_num; j++) {
                    if (aTestPinMap[nTestItem][i] == pPad2Drive[j]) {
                        /*g_mutual_ic_drive_r[j] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(g_mutual_ic_result[i + (nTestItem - 1) * 13]);*/
/*g_mutual_ic_drive_r[j] = g_mutual_ic_result[i + (nTestItem - 1) * 13];    //change comparison way because float computing in driver is prohibited*/

                        pDriveR[j] =
                            drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value
                            (g_mutual_ic_result[i + (nTestItem - 1) * 13]);
                        DBG(&g_i2c_client->dev,
                            "driveR[%d] = %d, _gDeltaC[%d] = %d\n", j,
                            pDriveR[j], i + (nTestItem - 1) * 13,
                            g_mutual_ic_result[i + (nTestItem - 1) * 13]);
                        g_ito_short_r_data[g_mutual_ic_sense_line_num + j] =
                            pDriveR[j];
                        g_ito_short_result_data[g_mutual_ic_sense_line_num + j] =
                            g_mutual_ic_result[i + (nTestItem - 1) * 13];

                        if (pDriveR[j] >= 10) {
                            g_ito_short_r_data[g_mutual_ic_sense_line_num + j] = 10;
                        }

/*if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nTestItem - 1) * 13], g_msg28xx_short_value, -1000))*/
                        if (0 ==
                            drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result
                                                                [i +
                                                                 (nTestItem -
                                                                  1) * 13],
                                                                g_msg28xx_short_value,
                                                                -g_msg28xx_short_value))
                        {
                            g_ito_short_fail_channel[g_mutual_ic_sense_line_num + j] =
                                (u32) aTestPinMap[nTestItem][i];

                            /*DEBUG("Ito Short driveR, count_test_pin = %d, normalTestFail_check[%d][%d] = %d, pShortFailChannel[%d] = %d, _gDeltaC[%d] = %d", count_test_pin, nTestItem, i, normalTestFail_check[nTestItem][i], _gSenseNum+j, ptMutualMpTest_result->pShortFailChannel[_gSenseNum+j], i + (nTestItem - 1) * 13, _gDeltaC[i + (nTestItem - 1) * 13]); */
                        }
                    }
                }
            }
        }

        if (nTestItem == 5 && g_msg28xx_short_test_5_type == 3) {
            for (i = 0; i < nTestPinNum; i++) {
                for (j = 0; j < g_msg28xx_gr_num; j++) {
                    if (aTestPinMap[nTestItem][i] == aPad2GR[j]) {
                        /*g_mutual_ic_grr[j] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(g_mutual_ic_result[i + (nTestItem - 1) * 13]);*/
/*g_mutual_ic_grr[j] = g_mutual_ic_result[i + (nTestItem - 1) * 13];    //change comparison way because float computing in driver is prohibited*/

                        pGRR[j] =
                            drv_mp_test_ito_open_test_msg228xx_on_cell_covert_r_value
                            (g_mutual_ic_result[i + (nTestItem - 1) * 13]);
                        g_ito_short_r_data[g_mutual_ic_sense_line_num +
                                        g_mutual_ic_drive_line_num + j] = pGRR[j];
                        g_ito_short_result_data[g_mutual_ic_sense_line_num +
                                             g_mutual_ic_drive_line_num + j] =
                            g_mutual_ic_result[i + (nTestItem - 1) * 13];

                        if (pGRR[j] >= 10) {
                            g_ito_short_r_data[g_mutual_ic_sense_line_num +
                                            g_mutual_ic_drive_line_num + j] = 10.0;
                        }

/*if (0 == drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result[i + (nTestItem - 1) * 13], g_msg28xx_short_value, -1000))*/
                        if (0 ==
                            drv_mp_test_mutual_ic_check_value_range(g_mutual_ic_result
                                                                [i +
                                                                 (nTestItem -
                                                                  1) * 13],
                                                                g_msg28xx_short_value,
                                                                -g_msg28xx_short_value))
                        {
                            g_ito_short_fail_channel[g_mutual_ic_sense_line_num +
                                                  g_mutual_ic_drive_line_num + j] =
                                (u32) aTestPinMap[nTestItem][i];
                            /*DEBUG("Ito Short GRR, count_test_pin = %d, normalTestFail_check[%d][%d] = %d, pShortFailChannel[%d] = %d, _gDeltaC[%d] = %d", count_test_pin, nTestItem, i, normalTestFail_check[nTestItem][i], _gSenseNum+j, ptMutualMpTest_result->pShortFailChannel[_gSenseNum+j], i + (nTestItem - 1) * 13, _gDeltaC[i + (nTestItem - 1) * 13]); */
                        }
                    }
                }
            }
        }

        if (aNormalTestFail[nTestItem]) {
            g_ito_short_checK_fail = aNormalTestFail[nTestItem];   /*ito short fail */
            n_ret_val = -1;
        }
    }

    DBG(&g_i2c_client->dev, "*** %s()g_ito_short_fail_channel:  ***\n", __func__);
    drv_mp_test_mutual_ic_debug_show_array(g_ito_short_fail_channel, 60, -32, 10, 10);
    DBG(&g_i2c_client->dev, "*** %s()g_ito_short_result_data:  ***\n", __func__);
    drv_mp_test_mutual_ic_debug_show_array(g_ito_short_result_data, 60, -32, 10, 10);

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

ITO_TEST_END:

    drv_touch_device_hw_reset();
    mdelay(300);

    drv_enable_finger_touch_report();
    kfree(pPad2Sense);
    kfree(pPad2Drive);

    return n_ret_val;
}

static s32 drv_mp_test_msg28xx_ito_short_test_entry(void)
{
    u16 nFwMode = MUTUAL_SINGLE_DRIVE;
    /*ItoTest_result_e nRetVal1 = ITO_TEST_OK, nRetVal2 = ITO_TEST_OK, nRetVal3 = ITO_TEST_OK, nRetVal4 = ITO_TEST_OK, nRetVal5 = ITO_TEST_OK; */
    s16 i = 0, j = 0;
    /*u16 nTestPin_count = 0; */
    /*s32 nShortThreshold = 0; */
    u16 *pPad2Drive = NULL;
    u16 *pPad2Sense = NULL;
    u16 nTime = 0;
    u16 aPad2GR[MUTUAL_IC_MAX_CHANNEL_NUM] = { 0 };
    s32 a_resultTemp[(MUTUAL_IC_MAX_CHANNEL_SEN +
                     MUTUAL_IC_MAX_CHANNEL_DRV) * 2] = { 0 };

    /*short test1 to 5. */
    /*u16 nTestPin_count = 0; */
    u16 nTestItemLoop = 6;
    u16 nTestItem = 0;
    /*s8 aNormalTest_result[8][2] = {0};    //0:golden    1:ratio */
    u16 aTestPinMap[6][13] = { {0} };   /*6:max subframe    13:max afe */
    u16 nTestPinNum = 0;
/*s32 nThrs = 0;*/
    u32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    g_msg28xx_deep_stand_by = 0;
    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    if (!drv_mp_test_msg28xx_ito_test_choose_tp_type()) {
        DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
        drv_touch_device_hw_reset();
        drv_enable_finger_touch_report();
        return -2;
    }

    pPad2Drive = kzalloc(sizeof(s16) * g_msg28xx_drive_num, GFP_KERNEL);
    pPad2Sense = kzalloc(sizeof(s16) * g_msg28xx_sense_num, GFP_KERNEL);
    g_mutual_ic_sense_line_num = g_msg28xx_sense_num;
    g_mutual_ic_drive_line_num = g_msg28xx_drive_num;

_RETRY_SHORT:
    drv_touch_device_hw_reset();

    /*reset only */
    db_bus_reset_slave();
    db_bus_enter_serial_debug_mode();
    db_bus_wait_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    mdelay(100);

    if (drv_mp_test_msg28xx_ito_test_switch_fw_mode(&nFwMode) < 0) {
        nTime++;
        if (nTime < 5) {
            goto _RETRY_SHORT;
        }
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# Switch Fw Mode failed! ***\n");
        n_ret_val = -1;

        goto ITO_TEST_END;
    }

    drv_mp_test_msg28xx_ito_read_setting(pPad2Sense, pPad2Drive, aPad2GR);

    /*N1_ShortTest */
    if (drv_mp_test_msg28xx_ito_short_test(1) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Short Test# Get DeltaC failed! ***\n");
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    for (nTestItem = 1; nTestItem < nTestItemLoop; nTestItem++) {
        DBG(&g_i2c_client->dev, "*** Short test item %d ***\n", nTestItem);
        if (drv_mp_test_ito_open_test_msg228xx_judge
            (nTestItem, /*aNormalTest_result, */ aTestPinMap,
             &nTestPinNum) < 0) {
            DBG(&g_i2c_client->dev,
                "*** Msg28xx Short Test# Item %d is failed! ***\n", nTestItem);
            n_ret_val = -1;
            goto ITO_TEST_END;
        }

        if (nTestItem == 1 || nTestItem == 2 ||
            (nTestItem == 5 && g_msg28xx_short_test_5_type == 1)) {
            for (i = 0; i < nTestPinNum; i++) {
                for (j = 0; j < g_mutual_ic_sense_line_num; j++) {
                    if (aTestPinMap[nTestItem][i] == pPad2Sense[j]) {
                        /*g_mutual_ic_sense_r[j] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(g_mutual_ic_result[i + (nTestItem - 1) * 13]);*/
                        g_mutual_ic_sense_r[j] = g_mutual_ic_result[i + (nTestItem - 1) * 13];   /*change comparison way because float computing in driver is prohibited */
                    }
                }
            }
        }

        if (nTestItem == 3 || nTestItem == 4 ||
            (nTestItem == 5 && g_msg28xx_short_test_5_type == 2)) {
            for (i = 0; i < nTestPinNum; i++) {
                for (j = 0; j < g_mutual_ic_drive_line_num; j++) {
                    if (aTestPinMap[nTestItem][i] == pPad2Drive[j]) {
                        /*g_mutual_ic_drive_r[j] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(g_mutual_ic_result[i + (nTestItem - 1) * 13]);*/
                        g_mutual_ic_drive_r[j] = g_mutual_ic_result[i + (nTestItem - 1) * 13];   /*change comparison way because float computing in driver is prohibited */
                    }
                }
            }
        }

        if (nTestItem == 5 && g_msg28xx_short_test_5_type == 3) {
            for (i = 0; i < nTestPinNum; i++) {
                for (j = 0; j < g_msg28xx_gr_num; j++) {
                    if (aTestPinMap[nTestItem][i] == aPad2GR[j]) {
                        /*g_mutual_ic_grr[j] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(g_mutual_ic_result[i + (nTestItem - 1) * 13]);*/
                        g_mutual_ic_grr[j] = g_mutual_ic_result[i + (nTestItem - 1) * 13];  /*change comparison way because float computing in driver is prohibited */
                    }
                }
            }
        }
    }

    for (i = 0; i < g_mutual_ic_sense_line_num; i++) {
        a_resultTemp[i] = g_mutual_ic_sense_r[i];
    }

    /*for (i = 0; i < g_mutual_ic_drive_line_num - 1; i++) */
    for (i = 0; i < g_mutual_ic_drive_line_num; i++) {
        a_resultTemp[i + g_mutual_ic_sense_line_num] = g_mutual_ic_drive_r[i];
    }

    /*nThrs = drv_mp_test_ito_open_test_msg228xx_covert_r_value(MSG28XX_SHORT_VALUE); */
    for (i = 0; i < g_mutual_ic_sense_line_num + g_mutual_ic_drive_line_num; i++) {
        if (a_resultTemp[i] == 0) {
            g_mutual_ic_result[i] = drv_mp_test_ito_open_test_msg228xx_covert_r_value(1);
        } else {
            g_mutual_ic_result[i] =
                drv_mp_test_ito_open_test_msg228xx_covert_r_value(a_resultTemp[i]);
        }
    }

    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_result,
                                     g_mutual_ic_sense_line_num +
                                     g_mutual_ic_drive_line_num, -32, 10, 8);
    /*for (i = 0; i < (g_mutual_ic_sense_line_num + g_mutual_ic_drive_line_num - 1); i++) */
    for (i = 0; i < (g_mutual_ic_sense_line_num + g_mutual_ic_drive_line_num); i++) {
        if (a_resultTemp[i] > MSG28XX_SHORT_VALUE) { /*change comparison way because float computing in driver is prohibited */
            g_mutual_ic_test_fail_channel[i] = 1;
            g_test_fail_channel_count++;
            n_ret_val = -1;
        } else {
            g_mutual_ic_test_fail_channel[i] = 0;
        }
    }

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

ITO_TEST_END:

    drv_touch_device_hw_reset();
    mdelay(300);

    drv_enable_finger_touch_report();
    kfree(pPad2Sense);
    kfree(pPad2Drive);

    return n_ret_val;
}

static s32 drv_mp_test_ito_water_proof_test_msg28xx_trigger_water_proof_one_shot(s16 *
                                                                      p_result_data,
                                                                      u32
                                                                      nDelay)
{
    u16 n_addr = 0x5000, nAddrNextSF = 0x1A4;
    u16 nSF = 0, nAfeOpening = 0, nDriOpening = 0;
    u16 nMax_dataNumOfOneSF = 0;
    u16 nDriMode = 0;
    u16 i;
    u8 nReg_data = 0;
    u8 aShot_data[390] = { 0 };  /*13*15*2 */
    u16 nReg_dataU16 = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    nReg_data = reg_get_l_byte_value(0x130A);
    nSF = nReg_data >> 4;
    nAfeOpening = nReg_data & 0x0f;

    if (nSF == 0) {
        return -1;
    }

    nReg_data = reg_get_l_byte_value(0x100B);
    nDriMode = nReg_data;

    nReg_data = reg_get_l_byte_value(0x1312);
    nDriOpening = nReg_data;

    DBG(&g_i2c_client->dev,
        "*** Msg28xx WaterProof Test# TriggerWaterProofOneShot nSF=%d, nAfeOpening=%d, nDriMode=%d, nDriOpening=%d. ***\n",
        nSF, nAfeOpening, nDriMode, nDriOpening);

    nMax_dataNumOfOneSF = nAfeOpening * nDriOpening;

    reg_set_16Bit_value_off(0x3D08, BIT8);  /*FIQ_E_FRAME_READY_MASK */

    /*polling frame-ready interrupt status */
    drv_mp_test_ito_test_msg28xx_enable_adc_one_shot();

    while (0x0000 == (nReg_dataU16 & BIT8)) {
        nReg_dataU16 = reg_get16_bit_value(0x3D18);
    }

    reg_set_16bit_value_on(0x3D08, BIT8);   /*FIQ_E_FRAME_READY_MASK */
    reg_set_16bit_value_on(0x3D08, BIT4);   /*FIQ_E_TIMER0_MASK */

    if (g_msg28xx_pattern_type == 1) {  /*for short test */
        /*s16 nShort_result_data[nSF][nAfeOpening]; */

        /*get ALL raw data */
        drv_mp_test_ito_test_db_bus_read_dq_mem_start();
        reg_get_x_bit_value(n_addr, aShot_data, 16, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
        drv_mp_test_ito_test_db_bus_read_dq_mem_end();

        /*drv_mp_test_mutual_ic_debug_show_array(aShot_data, 26, 8, 16, 16);*/
        for (i = 0; i < 8; i++) {
            p_result_data[i] =
                (s16) (aShot_data[2 * i] | aShot_data[2 * i + 1] << 8);
        }
    } else if (g_msg28xx_pattern_type == 3 || g_msg28xx_pattern_type == 4) {    /*for open test */
        /*s16 nOpen_result_data[nSF * nAfeOpening][nDriOpening]; */

        if (nSF > 4) {
            nSF = 4;
        }

        /*get ALL raw data, combine and handle datashift. */
        for (i = 0; i < nSF; i++) {
            drv_mp_test_ito_test_db_bus_read_dq_mem_start();
            reg_get_x_bit_value(n_addr + i * nAddrNextSF, aShot_data, 16,
                            MAX_I2C_TRANSACTION_LENGTH_LIMIT);
            drv_mp_test_ito_test_db_bus_read_dq_mem_end();

            /*drv_mp_test_mutual_ic_debug_show_array(aShot_data, 390, 8, 10, 16);*/
            p_result_data[2 * i] =
                (s16) (aShot_data[4 * i] | aShot_data[4 * i + 1] << 8);
            p_result_data[2 * i + 1] =
                (s16) (aShot_data[4 * i + 2] | aShot_data[4 * i + 3] << 8);
        }
    } else {
        return -1;
    }

    return 0;
}

static s32 drv_mp_test_ito_water_proof_test_msg28xx_get_water_proof_one_shot_raw_iir(s16 *
                                                                        pRaw_dataWP,
                                                                        u32
                                                                        nDelay)
{
    return
        drv_mp_test_ito_water_proof_test_msg28xx_trigger_water_proof_one_shot(pRaw_dataWP,
                                                                   nDelay);
}

static s32 drv_mp_test_ito_water_proof_test_msg28xx_get_deltac_wp(s32 *pTarget,
                                                         s8 nSwap, u32 nDelay)
{
    s16 nRaw_dataWP[12] = { 0 };
    s16 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (drv_mp_test_ito_water_proof_test_msg28xx_get_water_proof_one_shot_raw_iir
        (nRaw_dataWP, nDelay) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx Open Test# GetMutualOneShotRawIIR failed! ***\n");
        return -1;
    }

    for (i = 0; i < g_mutual_ic_water_proof_num; i++) {
        pTarget[i] = nRaw_dataWP[i];
    }

    return 0;
}

static s32 drv_mp_test_msg28xx_ito_water_proof_test(u32 nDelay)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*Stop mcu */
    retry_reg_set_16bit_value(0x0FE6, 0x0001);  /*bank:mheg5, addr:h0073 */
    drv_mp_test_ito_test_msg28xx_ana_sw_reset();

    if (drv_mp_test_ito_water_proof_test_msg28xx_get_deltac_wp
        (g_mutual_ic_deltac_water, -1, nDelay) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx WaterProof Test# GetDeltaCWP failed! ***\n");
        return -1;
    }

    drv_mp_test_mutual_ic_debug_show_array(g_mutual_ic_deltac_water, 12, -32, 10, 16);

    return 0;
}

static void drv_mp_test_msg28xx_ito_water_proof_test_msg_judge(void)
{
    int i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    for (i = 0; i < g_mutual_ic_water_proof_num; i++) {
        g_mutual_ic_result_water[i] = abs(g_mutual_ic_deltac_water[i]);
    }
}

static s32 drv_mp_test_msg28xx_ito_water_proof_test_entry(void)
{
    u16 nFwMode = WATERPROOF;
    s16 i = 0;
    u32 n_ret_val = 0;
    u16 nReg_dataWP = 0;
    u32 nDelay = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    g_mutual_ic_water_proof_num = 12;

    drv_disable_finger_touch_report();
    drv_touch_device_hw_reset();

    /*reset only */
    db_bus_reset_slave();
    db_bus_enter_serial_debug_mode();
    db_bus_wait_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    mdelay(100);

    if (drv_mp_test_msg28xx_ito_test_switch_fw_mode(&nFwMode) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx WaterProof Test# Switch FW mode failed! ***\n");
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    nReg_dataWP = reg_get16_bit_value(0x1402);
    if (nReg_dataWP == 0x8BBD) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx WaterProof Test# FW don't support waterproof! ***\n");
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    if (drv_mp_test_msg28xx_ito_water_proof_test(nDelay) < 0) {
        DBG(&g_i2c_client->dev,
            "*** Msg28xx WaterProof Test# Get DeltaC failed! ***\n");
        n_ret_val = -1;
        goto ITO_TEST_END;
    }

    drv_mp_test_msg28xx_ito_water_proof_test_msg_judge();

    for (i = 0; i < g_mutual_ic_water_proof_num; i++) {
        if (g_mutual_ic_result_water[i] > MSG28XX_WATER_VALUE) {   /*change comparison way because float computing in driver is prohibited */
            g_mutual_ic_test_fail_channel[i] = 1;
            g_test_fail_channel_count++;
            n_ret_val = -1;
        } else {
            g_mutual_ic_test_fail_channel[i] = 0;
        }
    }

    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

ITO_TEST_END:

    drv_touch_device_hw_reset();
    mdelay(300);

    drv_enable_finger_touch_report();

    return n_ret_val;
}

static u8 drv_mp_test_mutual_ic_check_value_range(s32 n_value, s32 nMax, s32 nMin)
{
    if (n_value <= nMax && n_value >= nMin) {
        return 1;
    } else {
        return 0;
    }
}

static void drv_mp_test_mutual_ic_debug_show_array(void *p_buf, u16 nLen,
                                             int n_dataType, int nCarry,
                                             int nChangeLine)
{
    u8 *pU8Buf = NULL;
    s8 *pS8Buf = NULL;
    u16 *pU16Buf = NULL;
    s16 *pS16Buf = NULL;
    u32 *pU32Buf = NULL;
    s32 *pS32Buf = NULL;
    int i;

    if (n_dataType == 8)
        pU8Buf = (u8 *) p_buf;
    else if (n_dataType == -8)
        pS8Buf = (s8 *) p_buf;
    else if (n_dataType == 16)
        pU16Buf = (u16 *) p_buf;
    else if (n_dataType == -16)
        pS16Buf = (s16 *) p_buf;
    else if (n_dataType == 32)
        pU32Buf = (u32 *) p_buf;
    else if (n_dataType == -32)
        pS32Buf = (s32 *) p_buf;

    for (i = 0; i < nLen; i++) {
        if (nCarry == 16) {
            if (n_dataType == 8)
                DBG(&g_i2c_client->dev, "%02X ", pU8Buf[i]);
            else if (n_dataType == -8)
                DBG(&g_i2c_client->dev, "%02X ", pS8Buf[i]);
            else if (n_dataType == 16)
                DBG(&g_i2c_client->dev, "%04X ", pU16Buf[i]);
            else if (n_dataType == -16)
                DBG(&g_i2c_client->dev, "%04X ", pS16Buf[i]);
            else if (n_dataType == 32)
                DBG(&g_i2c_client->dev, "%08X ", pU32Buf[i]);
            else if (n_dataType == -32)
                DBG(&g_i2c_client->dev, "%08X ", pS32Buf[i]);
        } else if (nCarry == 10) {
            if (n_dataType == 8)
                DBG(&g_i2c_client->dev, "%6d ", pU8Buf[i]);
            else if (n_dataType == -8)
                DBG(&g_i2c_client->dev, "%6d ", pS8Buf[i]);
            else if (n_dataType == 16)
                DBG(&g_i2c_client->dev, "%6d ", pU16Buf[i]);
            else if (n_dataType == -16)
                DBG(&g_i2c_client->dev, "%6d ", pS16Buf[i]);
            else if (n_dataType == 32)
                DBG(&g_i2c_client->dev, "%6d ", pU32Buf[i]);
            else if (n_dataType == -32)
                DBG(&g_i2c_client->dev, "%6d ", pS32Buf[i]);
        }

        if (i % nChangeLine == nChangeLine - 1) {
            DBG(&g_i2c_client->dev, "\n");
        }
    }
    DBG(&g_i2c_client->dev, "\n");
}

/*
static void _DrvMpTestMutualICDebugShowS32Array(s32 *p_buf, u16 nRow, u16 nCol)
{
    int i, j;

    for(j=0; j < nRow; j++)
    {
        for(i=0; i < nCol; i++)
        {
            DBG(&g_i2c_client->dev, "%4d ", p_buf[i * nRow + j]);
        }
        DBG(&g_i2c_client->dev, "\n");
    }
    DBG(&g_i2c_client->dev, "\n");
}
*/

static s32 drv_mp_test_ito_water_proof_test(void)
{
    s32 n_ret_val = -1;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type ==
        CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) {
        return drv_mp_test_msg28xx_ito_water_proof_test_entry();
    }

    return n_ret_val;
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

/*------------------------------------------------------------------------------//*/

static s32 drv_mp_test_ito_open_test(void)
{
    s32 n_ret_val = -1;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        n_ret_val = drv_mp_test_self_ic_ito_open_test_entry();
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type ==
        CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) {
        n_ret_val = drv_mp_test_msg28xx_ito_open_test_entry();
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

    return n_ret_val;
}

static s32 drv_mp_test_ito_short_test(void)
{
    s32 n_ret_val = -1;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        n_ret_val = drv_mp_test_self_ic_ito_short_test_entry();
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type ==
        CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) {
        if (!drv_mp_test_msg28xx_ito_test_choose_tp_type()) {
            DBG(&g_i2c_client->dev, "Choose Tp Type failed\n");
            n_ret_val = -1;
        }

        if (g_msg28xx_pattern_type == 5) {
            s32 nRetVal1 = -1, nRetVal2 = -1;

            nRetVal1 = drv_mp_test_ito_open_test_msg228xx_on_cellI_to_short_test_entry();
            nRetVal2 =
                drv_mp_test_ic_pin_short_test_msg28xx_on_cellIc_pin_short_test_entry();

            if ((nRetVal1 == -1) || (nRetVal2 == -1)) {
                n_ret_val = -1;
            } else {
                n_ret_val = 0;
            }
        } else {
            n_ret_val = drv_mp_test_msg28xx_ito_short_test_entry();
        }
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

    return n_ret_val;
}

static void drv_mp_test_ito_test_do_work(struct work_struct *p_work)
{
    s32 n_ret_val = ITO_TEST_OK;

    DBG(&g_i2c_client->dev,
        "*** %s() g_is_in_mp_test = %d, g_test_retry_count = %d ***\n", __func__,
        g_is_in_mp_test, g_test_retry_count);

    if (g_ito_test_mode == ITO_TEST_MODE_OPEN_TEST) {
        n_ret_val = drv_mp_test_ito_open_test();
    } else if (g_ito_test_mode == ITO_TEST_MODE_SHORT_TEST) {
        n_ret_val = drv_mp_test_ito_short_test();
    }
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    else if (g_ito_test_mode == ITO_TEST_MODE_WATERPROOF_TEST) {
        n_ret_val = drv_mp_test_ito_water_proof_test();
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
    else {
        DBG(&g_i2c_client->dev, "*** Undefined Mp Test Mode = %d ***\n",
            g_ito_test_mode);
        return;
    }

    DBG(&g_i2c_client->dev, "*** ctp mp test result = %d ***\n", n_ret_val);

    if (n_ret_val == ITO_TEST_OK) {   /*n_ret_val == 0 */
        g_ctp_mp_test_status = ITO_TEST_OK;
         /*PASS*/ mutex_lock(&g_mutex);
        g_is_in_mp_test = 0;
        mutex_unlock(&g_mutex);
        DBG(&g_i2c_client->dev, "mp test success\n");
    } else {
        g_test_retry_count--;
        if (g_test_retry_count > 0) {
            DBG(&g_i2c_client->dev, "g_test_retry_count = %d\n",
                g_test_retry_count);
            queue_work(g_ctp_mp_test_work_queue, &g_ctp_ito_test_work);
        } else {
            if (((g_chip_type == CHIP_TYPE_MSG22XX) &&
                 (n_ret_val == ITO_TEST_FAIL))
                ||
                ((g_chip_type == CHIP_TYPE_MSG28XX ||
                  g_chip_type == CHIP_TYPE_ILI2117A ||
                  g_chip_type ==
                  CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ )
                 && (n_ret_val == -1))) {
                g_ctp_mp_test_status = ITO_TEST_FAIL;
            } else
                if (((g_chip_type == CHIP_TYPE_MSG22XX) &&
                     (n_ret_val == ITO_TEST_GET_TP_TYPE_ERROR))
                    ||
                    ((g_chip_type == CHIP_TYPE_MSG28XX ||
                      g_chip_type == CHIP_TYPE_ILI2117A ||
                      g_chip_type == CHIP_TYPE_ILI2118A
                      /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) &&
                     (n_ret_val == -2))) {
                g_ctp_mp_test_status = ITO_TEST_GET_TP_TYPE_ERROR;
            } else {
                g_ctp_mp_test_status = ITO_TEST_UNDEFINED_ERROR;
            }

            mutex_lock(&g_mutex);
            g_is_in_mp_test = 0;
            mutex_unlock(&g_mutex);
            DBG(&g_i2c_client->dev, "mp test failed\n");
        }
    }
}

/*=============================================================*/
/*GLOBAL FUNCTION DEFINITION*/
/*=============================================================*/

s32 drv_mp_test_get_test_result(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "g_ctp_mp_test_status = %d\n", g_ctp_mp_test_status);

    return g_ctp_mp_test_status;
}

void drv_mp_test_get_test_fail_channel(ito_test_mode_e e_ito_test_mode,
                                 u8 *pFailChannel, u32 *pFailChannelCount)
{
#if defined(CONFIG_ENABLE_CHIP_TYPE_MSG22XX) || defined(CONFIG_ENABLE_CHIP_TYPE_MSG28XX)
    u32 i;
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX || CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "g_test_fail_channel_count = %d\n",
        g_test_fail_channel_count);

#if defined(CONFIG_ENABLE_CHIP_TYPE_MSG22XX)
    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        for (i = 0; i < g_test_fail_channel_count; i++) {
            pFailChannel[i] = g_self_ic_test_fail_channel[i];
        }

        *pFailChannelCount = g_test_fail_channel_count;
    } else {
        DBG(&g_i2c_client->dev,
            "g_chip_type = 0x%x is an undefined chip type.\n", g_chip_type);
        *pFailChannelCount = 0;
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type ==
        CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) {
        for (i = 0; i < MUTUAL_IC_MAX_MUTUAL_NUM; i++) {
            pFailChannel[i] = g_mutual_ic_test_fail_channel[i];
        }

        *pFailChannelCount = MUTUAL_IC_MAX_MUTUAL_NUM;  /*Return the test result of all channels, APK will filter out the fail channels. */
    } else {
        DBG(&g_i2c_client->dev,
            "g_chip_type = 0x%x is an undefined chip type.\n", g_chip_type);
        *pFailChannelCount = 0;
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
}

void drv_mp_test_get_test_data_log(ito_test_mode_e e_ito_test_mode, u8 *p_data_log,
                             u32 *p_length)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_ENABLE_CHIP_TYPE_MSG22XX)
    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        u32 i;
        u8 nHighByte, nLowByte;

        if (e_ito_test_mode == ITO_TEST_MODE_OPEN_TEST) {
            for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
                nHighByte = (g_self_ic_raw_data1[i] >> 8) & 0xFF;
                nLowByte = (g_self_ic_raw_data1[i]) & 0xFF;

                if (g_self_ic_data_flag1[i] == 1) {
                    p_data_log[i * 4] = 1;    /*indicate it is a on-use channel number */
                } else {
                    p_data_log[i * 4] = 0;    /*indicate it is a non-use channel number */
                }

                if (g_self_ic_raw_data1[i] >= 0) {
                    p_data_log[i * 4 + 1] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[i * 4 + 1] = 1;
                                         /*- : a negative number*/
                }

                p_data_log[i * 4 + 2] = nHighByte;
                p_data_log[i * 4 + 3] = nLowByte;
            }

            for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
                nHighByte = (g_self_ic_raw_data2[i] >> 8) & 0xFF;
                nLowByte = (g_self_ic_raw_data2[i]) & 0xFF;

                if (g_self_ic_data_flag2[i] == 1) {
                    p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 4] = 1;  /*indicate it is a on-use channel number */
                } else {
                    p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 4] = 0;  /*indicate it is a non-use channel number */
                }

                if (g_self_ic_raw_data2[i] >= 0) {
                    p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 4] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 4] = 1;
                                                                     /*- : a negative number*/
                }

                p_data_log[(i * 4 + 2) + SELF_IC_MAX_CHANNEL_NUM * 4] = nHighByte;
                p_data_log[(i * 4 + 3) + SELF_IC_MAX_CHANNEL_NUM * 4] = nLowByte;
            }

            *p_length = SELF_IC_MAX_CHANNEL_NUM * 8;
        } else if (e_ito_test_mode == ITO_TEST_MODE_SHORT_TEST) {
            for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
                nHighByte = (g_self_ic_raw_data1[i] >> 8) & 0xFF;
                nLowByte = (g_self_ic_raw_data1[i]) & 0xFF;

                if (g_self_ic_data_flag1[i] == 1) {
                    p_data_log[i * 4] = 1;    /*indicate it is a on-use channel number */
                } else {
                    p_data_log[i * 4] = 0;    /*indicate it is a non-use channel number */
                }

                if (g_self_ic_raw_data1[i] >= 0) {
                    p_data_log[i * 4 + 1] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[i * 4 + 1] = 1;
                                         /*- : a negative number*/
                }

                p_data_log[i * 4 + 2] = nHighByte;
                p_data_log[i * 4 + 3] = nLowByte;
            }

            for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
                nHighByte = (g_self_ic_raw_data2[i] >> 8) & 0xFF;
                nLowByte = (g_self_ic_raw_data2[i]) & 0xFF;

                if (g_self_ic_data_flag2[i] == 1) {
                    p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 4] = 1;  /*indicate it is a on-use channel number */
                } else {
                    p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 4] = 0;  /*indicate it is a non-use channel number */
                }

                if (g_self_ic_raw_data2[i] >= 0) {
                    p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 4] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 4] = 1;
                                                                     /*- : a negative number*/
                }

                p_data_log[i * 4 + 2 + SELF_IC_MAX_CHANNEL_NUM * 4] = nHighByte;
                p_data_log[i * 4 + 3 + SELF_IC_MAX_CHANNEL_NUM * 4] = nLowByte;
            }

            for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
                nHighByte = (g_self_ic_raw_data3[i] >> 8) & 0xFF;
                nLowByte = (g_self_ic_raw_data3[i]) & 0xFF;

                if (g_self_ic_data_flag3[i] == 1) {
                    p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 8] = 1;  /*indicate it is a on-use channel number */
                } else {
                    p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 8] = 0;  /*indicate it is a non-use channel number */
                }

                if (g_self_ic_raw_data3[i] >= 0) {
                    p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 8] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 8] = 1;
                                                                     /*- : a negative number*/
                }

                p_data_log[(i * 4 + 2) + SELF_IC_MAX_CHANNEL_NUM * 8] = nHighByte;
                p_data_log[(i * 4 + 3) + SELF_IC_MAX_CHANNEL_NUM * 8] = nLowByte;
            }

            if (g_self_ici_enable_2r) {
                for (i = 0; i < SELF_IC_MAX_CHANNEL_NUM; i++) {
                    nHighByte = (g_self_ic_raw_data4[i] >> 8) & 0xFF;
                    nLowByte = (g_self_ic_raw_data4[i]) & 0xFF;

                    if (g_self_ic_data_flag4[i] == 1) {
                        p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 12] = 1; /*indicate it is a on-use channel number */
                    } else {
                        p_data_log[i * 4 + SELF_IC_MAX_CHANNEL_NUM * 12] = 0; /*indicate it is a non-use channel number */
                    }

                    if (g_self_ic_raw_data4[i] >= 0) {
                        p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 12] = 0;   /*+ : a positive number */
                    } else {
                        p_data_log[(i * 4 + 1) + SELF_IC_MAX_CHANNEL_NUM * 12] =
                            1;
                                                                          /*- : a negative number*/
                    }

                    p_data_log[(i * 4 + 2) + SELF_IC_MAX_CHANNEL_NUM * 12] =
                        nHighByte;
                    p_data_log[(i * 4 + 3) + SELF_IC_MAX_CHANNEL_NUM * 12] =
                        nLowByte;
                }
            }

            *p_length = SELF_IC_MAX_CHANNEL_NUM * 16;
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined MP Test Mode ***\n");
        }
    } else {
        DBG(&g_i2c_client->dev,
            "g_chip_type = 0x%x is an undefined chip type.\n", g_chip_type);
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type ==
        CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) {
        u32 i, j, k;

        if (e_ito_test_mode == ITO_TEST_MODE_OPEN_TEST) {
            k = 0;

            for (j = 0; j < g_mutual_ic_drive_line_num; j++)
/*for (j = 0; j < (g_mutual_ic_drive_line_num-1); j ++)*/
            {
                for (i = 0; i < g_mutual_ic_sense_line_num; i++) {
/*DBG(&g_i2c_client->dev, "\nDrive%d, Sense%d, Value = %d\t", j, i, g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j]); // add for debug*/

                    if (g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j] >= 0) {
                        p_data_log[k * 5] = 0;    /*+ : a positive number */
                    } else {
                        p_data_log[k * 5] = 1;
                                           /*- : a negative number*/
                    }

                    p_data_log[k * 5 + 1] =
                        (g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j] >> 24)
                        & 0xFF;
                    p_data_log[k * 5 + 2] =
                        (g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j] >> 16)
                        & 0xFF;
                    p_data_log[k * 5 + 3] =
                        (g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j] >> 8)
                        & 0xFF;
                    p_data_log[k * 5 + 4] =
                        (g_mutual_ic_result[i * g_mutual_ic_drive_line_num + j]) &
                        0xFF;

                    k++;
                }
            }

            DBG(&g_i2c_client->dev, "\nk = %d\n", k);

            *p_length = k * 5;
        } else if (e_ito_test_mode == ITO_TEST_MODE_SHORT_TEST) {
            k = 0;

            for (i = 0;
                 i < (g_mutual_ic_drive_line_num - 1 + g_mutual_ic_sense_line_num);
                 i++) {
                if (g_mutual_ic_result[i] >= 0) {
                    p_data_log[k * 5] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[k * 5] = 1;
                                       /*- : a negative number*/
                }

                p_data_log[k * 5 + 1] = (g_mutual_ic_result[i] >> 24) & 0xFF;
                p_data_log[k * 5 + 2] = (g_mutual_ic_result[i] >> 16) & 0xFF;
                p_data_log[k * 5 + 3] = (g_mutual_ic_result[i] >> 8) & 0xFF;
                p_data_log[k * 5 + 4] = (g_mutual_ic_result[i]) & 0xFF;
                k++;
            }

            DBG(&g_i2c_client->dev, "\nk = %d\n", k);

            *p_length = k * 5;
        } else if (e_ito_test_mode == ITO_TEST_MODE_WATERPROOF_TEST) {
            k = 0;

            for (i = 0; i < g_mutual_ic_water_proof_num; i++) {
                if (g_mutual_ic_result_water[i] >= 0) {
                    p_data_log[k * 5] = 0;    /*+ : a positive number */
                } else {
                    p_data_log[k * 5] = 1;
                                       /*- : a negative number*/
                }

                p_data_log[k * 5 + 1] = (g_mutual_ic_result_water[i] >> 24) & 0xFF;
                p_data_log[k * 5 + 2] = (g_mutual_ic_result_water[i] >> 16) & 0xFF;
                p_data_log[k * 5 + 3] = (g_mutual_ic_result_water[i] >> 8) & 0xFF;
                p_data_log[k * 5 + 4] = (g_mutual_ic_result_water[i]) & 0xFF;
                k++;
            }

            DBG(&g_i2c_client->dev, "\nk = %d\n", k);

            *p_length = k * 5;
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined MP Test Mode ***\n");
        }
    } else {
        DBG(&g_i2c_client->dev,
            "g_chip_type = 0x%x is an undefined chip type.\n", g_chip_type);
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
}

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
void drv_mp_test_get_test_scope(test_scope_info_t *p_info)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type ==
        CHIP_TYPE_ILI2118A /* || g_chip_type == CHIP_TYPE_MSG58XXA */ ) {
        p_info->n_my = g_mutual_ic_drive_line_num;
        p_info->n_mx = g_mutual_ic_sense_line_num;
        p_info->n_key_num = g_msg28xx_key_num;

        DBG(&g_i2c_client->dev, "*** My = %d ***\n", p_info->n_my);
        DBG(&g_i2c_client->dev, "*** Mx = %d ***\n", p_info->n_mx);
        DBG(&g_i2c_client->dev, "*** KeyNum = %d ***\n", p_info->n_key_num);
    }
}

void drv_mp_test_get_test_log_all(u8 *p_data_log, u32 *p_length)
{
    u16 i = 0;
    u8 *pRowStr = "D", *pColStr = "S", *pPinStr = "P", *pItem = NULL;
    u8 *pVersion = NULL;
    u16 nMajor = 0, nMinor = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_msg28xx_pattern_type == 5) {
        DBG(&g_i2c_client->dev, "g_msg28xx_pattern_type = 5\n");

        if (g_is_in_mp_test != 0) {
            *p_length =
                *p_length + sprintf(p_data_log + *p_length,
                                   "Test is still running!!!\n");
        } else {
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            *p_length =
                *p_length + sprintf(p_data_log + *p_length,
                                   "Device Driver Version : %s\n",
                                   DEVICE_DRIVER_RELEASE_VERSION);
            drv_get_customer_firmware_version(&nMajor, &nMinor, &pVersion);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length,
                                   "Main Block FW Version : %d.%03d\n", nMajor,
                                   nMinor);
            drv_get_customer_firmware_version_by_db_bus(EMEM_MAIN, &nMajor, &nMinor,
                                                 &pVersion);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length,
                                   "Main Block FW Version : %d.%03d\n", nMajor,
                                   nMinor);
            drv_get_customer_firmware_version_by_db_bus(EMEM_INFO, &nMajor, &nMinor,
                                                 &pVersion);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length,
                                   "Info Block FW Version : %d.%03d\n", nMajor,
                                   nMinor);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length, "SupportIC : %d\n",
                                   g_msg28xx_support_ic);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length, "DC_Range=%d\n",
                                   g_msg28xx_dc_range);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length, "DC_Ratio_1000=%d\n",
                                   g_msg28xx_dc_ratio_1000);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length,
                                   "DC_Border_Ratio_1000=%d\n",
                                   g_msg28xx_dc_border_ratio_1000);
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "Golden";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%5s%2d", pRowStr,
                                       i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                if ((i % g_msg28xx_drive_num) == 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr,
                                           (i / g_msg28xx_drive_num) + 1);
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_golden_channel
                                           [i]);
                } else if ((i % g_msg28xx_drive_num) ==
                           (g_msg28xx_drive_num - 1)) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d\n",
                                           g_mutual_ic_on_cell_open_test_golden_channel
                                           [i]);
                } else {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_golden_channel
                                           [i]);
                }
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "Golden_Max";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%5s%2d", pRowStr,
                                       i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                if ((i % g_msg28xx_drive_num) == 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr,
                                           (i / g_msg28xx_drive_num) + 1);
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_golden_channel_max
                                           [i]);
                } else if ((i % g_msg28xx_drive_num) ==
                           (g_msg28xx_drive_num - 1)) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d\n",
                                           g_mutual_ic_on_cell_open_test_golden_channel_max
                                           [i]);
                } else {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_golden_channel_max
                                           [i]);
                }
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "Golden_Min";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%5s%2d", pRowStr,
                                       i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                if ((i % g_msg28xx_drive_num) == 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr,
                                           (i / g_msg28xx_drive_num) + 1);
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_golden_channel_min
                                           [i]);
                } else if ((i % g_msg28xx_drive_num) ==
                           (g_msg28xx_drive_num - 1)) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d\n",
                                           g_mutual_ic_on_cell_open_test_golden_channel_min
                                           [i]);
                } else {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_golden_channel_min
                                           [i]);
                }
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "DeltaC";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%5s%2d", pRowStr,
                                       i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                if ((i % g_msg28xx_drive_num) == 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr,
                                           (i / g_msg28xx_drive_num) + 1);
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_result_data
                                           [i]);
                } else if ((i % g_msg28xx_drive_num) ==
                           (g_msg28xx_drive_num - 1)) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d\n",
                                           g_mutual_ic_on_cell_open_test_result_data
                                           [i]);
                } else {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_result_data
                                           [i]);
                }
            }
            if (g_mutual_ic_on_cell_open_test_result[0] == 0) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "DeltaC__result:PASS\n");
            } else {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "DeltaC__result:FAIL\n");
                pItem = "Fail Channel:  ";
                *p_length = *p_length + sprintf(p_data_log + *p_length, "%s", pItem);
                for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                    if (g_normal_test_fail_check_deltac[i] == MSG28XX_PIN_NO_ERROR) {
                        continue;
                    }
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "D%d.S%d",
                                           g_normal_test_fail_check_deltac[i] % 100,
                                           g_normal_test_fail_check_deltac[i] /
                                           100);
                    *p_length = *p_length + sprintf(p_data_log + *p_length, "    ");
                }
                *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "Ratio";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%5s%2d", pRowStr,
                                       i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                if ((i % g_msg28xx_drive_num) == 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr,
                                           (i / g_msg28xx_drive_num) + 1);
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_result_ratio_data
                                           [i]);
                } else if ((i % g_msg28xx_drive_num) ==
                           (g_msg28xx_drive_num - 1)) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d\n",
                                           g_mutual_ic_on_cell_open_test_result_ratio_data
                                           [i]);
                } else {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_mutual_ic_on_cell_open_test_result_ratio_data
                                           [i]);
                }
            }
            if (g_mutual_ic_on_cell_open_test_result[1] == 0) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "Ratio__result:PASS\n");
            } else {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "Ratio__result:FAIL\n");
                pItem = "Fail Channel:  ";
                *p_length = *p_length + sprintf(p_data_log + *p_length, "%s", pItem);
                for (i = 0; i < g_msg28xx_sense_num * g_msg28xx_drive_num; i++) {
                    if (g_normal_test_fail_check_ratio[i] == MSG28XX_PIN_NO_ERROR) {
                        continue;
                    }
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "D%d.S%d",
                                           g_normal_test_fail_check_ratio[i] % 100,
                                           g_normal_test_fail_check_ratio[i] / 100);
                    *p_length = *p_length + sprintf(p_data_log + *p_length, "    ");
                }
                *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            *p_length =
                *p_length + sprintf(p_data_log + *p_length, "ShortValue=%d\n",
                                   g_msg28xx_short_value);
            *p_length =
                *p_length + sprintf(p_data_log + *p_length, "ICPinShort=%d\n",
                                   g_msg28xx_ic_pin_short);
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "Pin Number";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
                if (g_msg28xx_sense_pad_pin_mapping[i] != 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%5s%2d",
                                           pPinStr,
                                           g_msg28xx_sense_pad_pin_mapping[i]);
                }
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "deltaR";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
                if (g_msg28xx_sense_pad_pin_mapping[i] != 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%6dM",
                                           g_ic_pin_short_sence_r[i]);
                }
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "result_data";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
                if (g_msg28xx_sense_pad_pin_mapping[i] != 0) {
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%7d",
                                           g_ic_pin_short_result_data[i]);
                }
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            if (g_ic_pin_short_check_fail == 0) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "ICPin Short Test:PASS\n");
            } else {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "ICPin Short Test:FAIL\n");
                pItem = "Fail Channel:  ";
                *p_length = *p_length + sprintf(p_data_log + *p_length, "%s", pItem);
                for (i = 0; i < MUTUAL_IC_MAX_CHANNEL_NUM; i++) {
                    if (g_ic_pin_short_fail_channel[i] != 0) {
                        *p_length =
                            *p_length + sprintf(p_data_log + *p_length, "P%d",
                                               g_ic_pin_short_fail_channel[i]);
                        *p_length =
                            *p_length + sprintf(p_data_log + *p_length, "    ");
                    }
                }
                *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "deltaR";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < 10; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%7d", i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num; i++) {
                if ((i % 10) == 0) {
                    if (i != 0) {
                        *p_length =
                            *p_length + sprintf(p_data_log + *p_length, "\n");
                    }
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr, i);
                }

                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%6dM",
                                       g_ito_short_r_data[i]);
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                if ((i % 10) == 0) {
                    if (i != 0) {
                        *p_length =
                            *p_length + sprintf(p_data_log + *p_length, "\n");
                    }
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pRowStr, i);
                }

                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%6dM",
                                       g_ito_short_r_data[i +
                                                       g_msg28xx_sense_num]);
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            pItem = "result_data";
            *p_length = *p_length + sprintf(p_data_log + *p_length, "%10s", pItem);
            for (i = 0; i < 10; i++) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%7d", i + 1);
            }

            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            for (i = 0; i < g_msg28xx_sense_num; i++) {
                if ((i % 10) == 0) {
                    if (i != 0) {
                        *p_length =
                            *p_length + sprintf(p_data_log + *p_length, "\n");
                    }
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pColStr, i);
                }

                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%7d",
                                       g_ito_short_result_data[i]);
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");
            for (i = 0; i < g_msg28xx_drive_num; i++) {
                if ((i % 10) == 0) {
                    if (i != 0) {
                        *p_length =
                            *p_length + sprintf(p_data_log + *p_length, "\n");
                    }
                    *p_length =
                        *p_length + sprintf(p_data_log + *p_length, "%8s%2d",
                                           pRowStr, i);
                }

                *p_length =
                    *p_length + sprintf(p_data_log + *p_length, "%7d",
                                       g_ito_short_result_data[i +
                                                            g_msg28xx_sense_num]);
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            if (g_ito_short_checK_fail == 0) {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "ITO Short Test:PASS\n");
            } else {
                *p_length =
                    *p_length + sprintf(p_data_log + *p_length,
                                       "ITO Short Test:FAIL\n");
            }
            *p_length = *p_length + sprintf(p_data_log + *p_length, "\n");

            DBG(&g_i2c_client->dev, "***p_data_log: %d ***\n", p_data_log[0]);
            DBG(&g_i2c_client->dev, "*** *p_length: %d ***\n", *p_length);
        }
    } else {
        *p_length = *p_length + sprintf(p_data_log + *p_length, "No Logs!!!\n");
    }
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

void drv_mp_test_schedule_mp_test_work(ito_test_mode_e e_ito_test_mode)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "*** g_is_in_mp_test = %d ***\n", g_is_in_mp_test);

    if (g_is_in_mp_test == 0) {
        DBG(&g_i2c_client->dev, "ctp mp test start\n");

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            g_is_old_firmware_version = drv_mp_test_msg22xx_check_firmware_version();
        }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX */

        g_ito_test_mode = e_ito_test_mode;

        mutex_lock(&g_mutex);
        g_is_in_mp_test = 1;
        mutex_unlock(&g_mutex);

        g_test_retry_count = CTP_MP_TEST_RETRY_COUNT;
        g_ctp_mp_test_status = ITO_TEST_UNDER_TESTING;

        queue_work(g_ctp_mp_test_work_queue, &g_ctp_ito_test_work);
    }
}

void drv_mp_test_create_mp_test_work_queue(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_ctp_mp_test_work_queue = create_singlethread_workqueue("ctp_mp_test");
    INIT_WORK(&g_ctp_ito_test_work, drv_mp_test_ito_test_do_work);
}

#endif /*CONFIG_ENABLE_ITO_MP_TEST */
