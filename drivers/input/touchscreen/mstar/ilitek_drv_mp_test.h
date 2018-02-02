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
 * @file    ilitek_drv_mp_test.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __ILITEK_DRV_MP_TEST_H__
#define __ILITEK_DRV_MP_TEST_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE                                                             */
/*--------------------------------------------------------------------------*/

#include "ilitek_drv_common.h"

#ifdef CONFIG_ENABLE_ITO_MP_TEST

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION                                         */
/*--------------------------------------------------------------------------*/

#define CTP_MP_TEST_RETRY_COUNT (3)

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG22XX
/*--------- Constant defined for MSG22xx ---------*/
#define SELF_IC_OPEN_TEST_NON_BORDER_AREA_THRESHOLD (35) /*range : 25~60*/
#define SELF_IC_OPEN_TEST_BORDER_AREA_THRESHOLD     (40) /*range : 25~60*/

#define	SELF_IC_SHORT_TEST_THRESHOLD                (3500)
#define SELF_IC_MAX_CHANNEL_NUM   (48)
#define SELF_IC_FIQ_E_FRAME_READY_MASK      (1 << 8)

#define MSG22XX_RIU_BASE_ADDR       (0)
#define MSG22XX_RIU_WRITE_LENGTH    (144)
#define MSG22XX_CSUB_REF            (0) /*(18)*/
#define MSG22XX_CSUB_REF_MAX        (0x3F)

#define MSG22XX_MAX_SUBFRAME_NUM    (24)
#define MSG22XX_MAX_AFE_NUM         (4)
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG22XX*/

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
/*--------- Constant defined for MSG28xx ---------*/
#define MUTUAL_IC_MAX_CHANNEL_NUM  60 /*38*/
#define MUTUAL_IC_MAX_MUTUAL_NUM  (MUTUAL_IC_MAX_CHANNEL_DRV * MUTUAL_IC_MAX_CHANNEL_SEN)

#define MUTUAL_IC_FIR_RATIO    50 /*25*/
#define MUTUAL_IC_IIR_MAX		32600

#define MUTUAL_IC_MAX_CHANNEL_DRV  30
#define MUTUAL_IC_MAX_CHANNEL_SEN  20

#define MSG28XX_SHORT_VALUE  3000
#define MSG28XX_WATER_VALUE  20000
#define MSG28XX_DC_RANGE  30
#define MSG28XX_DC_RATIO  10
#define MSG28XX_UN_USE_SENSOR (0x5AA5)
#define MSG28XX_UN_USE_PIN (0xABCD)
#define MSG28XX_NULL_DATA (-3240)
#define MSG28XX_PIN_NO_ERROR (0xFFFF)
#define MSG28XX_IIR_MAX 32600
#define MSG28XX_TEST_ITEM_NUM 8
#define	MSG28XX_KEY_SEPERATE	0x5566
#define	MSG28XX_KEY_COMBINE		0x7788
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX*/

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR MACRO DEFINITION                                            */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* DATA TYPE DEFINITION                                                     */
/*--------------------------------------------------------------------------*/

#if defined(CONFIG_ENABLE_CHIP_TYPE_MSG28XX)
struct
{
    u8 n_my;
    u8 n_mx;
    u8 n_key_num;
} test_scope_info_t;
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX*/

/*--------------------------------------------------------------------------*/
/* GLOBAL VARIABLE DEFINITION                                               */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION                                              */
/*--------------------------------------------------------------------------*/

#if defined(CONFIG_ENABLE_CHIP_TYPE_MSG28XX)
extern void drv_mp_test_get_test_scope(test_scope_info_t *p_info);
extern void drv_mp_test_get_test_log_all(u8 *p_data_log, u32 *p_length);
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX*/
extern void drv_mp_test_create_mp_test_work_queue(void);
extern void drv_mp_test_get_test_data_log(ito_test_mode_e e_ito_test_mode, u8 *p_data_log, u32 *p_length);
extern void drv_mp_test_get_test_fail_channel(ito_test_mode_e e_ito_test_mode, u8 *pFailChannel, u32 *pFailChannelCount);
extern s32 drv_mp_test_get_test_result(void);
extern void drv_mp_test_schedule_mp_test_work(ito_test_mode_e e_ito_test_mode);

#endif /*CONFIG_ENABLE_ITO_MP_TEST*/

#endif  /* __ILITEK_DRV_MP_TEST_H__ */