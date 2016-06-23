/*
 *  ml630q790.c - Linux kernel modules for Sensor Hub 
 *
 *  Copyright (C) 2012-2014 LAPIS SEMICONDUCTOR CO., LTD.
 *
 *  This file is based on :
 *    ml610q792.c - Linux kernel modules for acceleration sensor
 *    http://android-dev.kyocera.co.jp/source/versionSelect_URBANO.html
 *    (C) 2012 KYOCERA Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* Version: 0x00000013 */

#ifdef NO_LINUX
#include "test/test.h"
#else
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/uaccess.h> 
//#include <asm/gpio.h>
#endif
#include "ml630q790.h"
#if 1
#include <linux/qpnp/pin.h>
#endif
//#include <sharp/sh_boot_manager.h>

#ifdef CONFIG_SHTERM
#include <sharp/shterm_k.h>
#endif /* CONFIG_SHTERM */

//#undef CONFIG_ML630Q790_DEBUG
#define CONFIG_ML630Q790_DEBUG
#ifdef CONFIG_ML630Q790_DEBUG
#define SHUB_SUSPEND
#define CONFIG_SET_AXIS_VAL

static char * shub_get_active_sensor_name(int32_t type)
{
    switch(type){
        case SHUB_ACTIVE_ACC               : return "ACTIVE_ACC";
        case SHUB_ACTIVE_PEDOM             : return "ACTIVE_PEDOM";
        case SHUB_ACTIVE_PEDOM_NO_NOTIFY   : return "ACTIVE_PEDOM_NO_NOTIFY";
        case SHUB_ACTIVE_GYRO              : return "ACTIVE_GYRO";
        case SHUB_ACTIVE_MAG               : return "ACTIVE_MAG";
        case SHUB_ACTIVE_BARO              : return "ACTIVE_BARO";
        case SHUB_ACTIVE_ORI               : return "ACTIVE_ORI";
        case SHUB_ACTIVE_GRAVITY           : return "ACTIVE_GRAVITY";
        case SHUB_ACTIVE_LACC              : return "ACTIVE_LACC";
        case SHUB_ACTIVE_RV                : return "ACTIVE_RV";
        case SHUB_ACTIVE_RV_NONMAG         : return "ACTIVE_RV_NONMAG";
        case SHUB_ACTIVE_RV_NONGYRO        : return "ACTIVE_RV_NONGYRO";
        case SHUB_ACTIVE_GYROUNC           : return "ACTIVE_GYROUNC";
        case SHUB_ACTIVE_MAGUNC            : return "ACTIVE_MAGUNC";
        case SHUB_ACTIVE_PEDODEC           : return "ACTIVE_PEDODEC";
        case SHUB_ACTIVE_PEDODEC_NO_NOTIFY : return "ACTIVE_PEDODEC_NO_NOTIFY";
        case SHUB_ACTIVE_SIGNIFICANT       : return "ACTIVE_SIGNIFICANT";
        case SHUB_ACTIVE_EXT_PEDOM         : return "ACTIVE_EXT_PEDOM";
        case SHUB_ACTIVE_ERROR             : return "ACTIVE_ERROR";
        case SHUB_ACTIVE_GDEC              : return "ACTIVE_GDEC";
        case SHUB_ACTIVE_MOTIONDEC         : return "ACTIVE_MOTIONDEC";
        case SHUB_ACTIVE_PICKUP            : return "ACTIVE_PICKUP";
#ifdef SHUB_SW_TWIST_DETECT
        case SHUB_ACTIVE_TWIST             : return "ACTIVE_TWIST";
#endif
    }
    return "NONE";
}

#define DBG_LV_PEDO    0x01
#define DBG_LV_INT     0x02
#define DBG_LV_TIME    0x04
#define DBG_LV_CMD     0x08
#define DBG_LV_INFO    0x10
#define DBG_LV_HOSTIF  0x20
#define DBG_LV_ERROR   0x40
#define DBG_LV_DATA    0x80 
#define DBG_LV_ALL     (0xffff)
//int32_t dbg_level = DBG_LV_ALL;
//int32_t dbg_level = DBG_LV_ERROR;
int dbg_level = DBG_LV_ERROR;
#define DBG(lv, msg, ...) {                            \
    if( lv & dbg_level ){                              \
        printk(KERN_ERR "[shub] " msg, ##__VA_ARGS__); \
    }                                                  \
}
#else
#define DBG(lv, msg, ...)
#endif

#ifdef CONFIG_ANDROID_ENGINEERING
module_param(dbg_level, int, 0600);
#endif

/*
   hostinterface register
 */
#define CFG      (0x00u)
#define INTMASK0 (0x02u)
#define INTMASK1 (0x03u)
#define STATUS   (0x09u)
#define ERROR0   (0x0Au)
#define ERROR1   (0x0Bu)
#define INTREQ0  (0x0Cu)
#define INTREQ1  (0x0Du)
#define PRM0     (0x20u)
#define CMD0     (0x30u)
#define CMD1     (0x31u)
#define CMDENTRY (0x32u)
#define FIFO     (0x10u)
#define RSLT00   (0x40u)
#define RSLT01   (0x41u)
#define RSLT02   (0x42u)
#define RSLT03   (0x43u)
#define RSLT04   (0x44u)
#define RSLT05   (0x45u)
#define RSLT06   (0x46u)
#define RSLT07   (0x47u)
#define RSLT08   (0x48u)
#define RSLT09   (0x49u)
#define RSLT0A   (0x4Au)
#define RSLT0B   (0x4Bu)
#define RSLT0C   (0x4Cu)
#define RSLT0D   (0x4Du)
#define RSLT0E   (0x4Eu)
#define RSLT0F   (0x4Fu)
#define RSLT10   (0x50u)
#define RSLT11   (0x51u)
#define RSLT12   (0x52u)
#define RSLT13   (0x53u)
#define RSLT14   (0x54u)
#define RSLT15   (0x55u)
#define RSLT16   (0x56u)
#define RSLT17   (0x57u)
#define RSLT18   (0x58u)
#define RSLT19   (0x59u)
#define RSLT1A   (0x5Au)
#define RSLT1B   (0x5Bu)
#define RSLT1C   (0x5Cu)
#define RSLT1D   (0x5Du)
#define RSLT1E   (0x5Eu)
#define RSLT1F   (0x5Fu)
#define RSLT20   (0x60u)
#define RSLT21   (0x61u)
#define RSLT22   (0x62u)
#define RSLT23   (0x63u)
#define RSLT24   (0x64u)
#define RSLT25   (0x65u)
#define RSLT26   (0x66u)
#define RSLT27   (0x67u)
#define RSLT28   (0x68u)
#define RSLT29   (0x69u)
#define RSLT2A   (0x6Au)
#define RSLT2B   (0x6Bu)
#define RSLT2C   (0x6Cu)
#define RSLT2D   (0x6Du)
#define RSLT2E   (0x6Eu)
#define RSLT2F   (0x6Fu)
#define RSLT30   (0x70u)
#define RSLT31   (0x71u)
#define RSLT32   (0x72u)
#define RSLT33   (0x73u)
#define RSLT34   (0x74u)
#define RSLT35   (0x75u)
#define RSLT36   (0x76u)
#define RSLT37   (0x77u)
#define RSLT38   (0x78u)
#define RSLT39   (0x79u)
#define RSLT3A   (0x7Au)
#define RSLT3B   (0x7Bu)
#define RSLT3C   (0x7Cu)
#define RSLT3D   (0x7Du)
#define RSLT3E   (0x7Eu)
#define RSLT3F   (0x7Fu)

/*
   INTREQ TYPE
 */
#define INTREQ_NONE      (0x0000u)
#define INTREQ_HOST_CMD  (0x0001u)
#define INTREQ_ACC       (0x0002u)
#define INTREQ_BARO      (0x0040u)
#define INTREQ_MAG       (0x0080u)
#define INTREQ_GYRO      (0x0100u)
#define INTREQ_FUSION    (0x4000u)
#define INTREQ_CUSTOMER  (0x8000u)
#define INTREQ_ERROR     (0xFFFEu)
#define INTREQ_MASK      (INTREQ_HOST_CMD | INTREQ_ACC | INTREQ_BARO | INTREQ_MAG | \
        INTREQ_GYRO | INTREQ_FUSION | INTREQ_CUSTOMER)

#define STATUS_IDLE      (0x00u)
#define STATUS_HOSTCMD   (0x01u)
#define STATUS_MEASURE   (0x02u)
#define STATUS_RUNAPP    (0x04u)
#define STATUS_FUSIONCAL (0x08u)
#define STATUS_INIT      (0xFEu)

/*
   INTREQ Detail
 */
#define ID_NONE                      (0x00u)
#define ID_ACC_PEDO_CNT              (0x02u)
#define ID_ACC_PEDO_MOVE             (0x04u)
#define ID_ACC_PEDO_STOP             (0x08u)
#define ID_ACC_PEDO_WALK_RUN         (0x10u)
#define ID_ACC_PEDO_PEDO_TRANS       (0x20u)
#define ID_ACC_MOVE_DETECT           (0x10u)
#define ID_ACC_XY_DETECT             (0x40u)
#define ID_ACC_Z_DETECT              (0x80u)
#define ID_ACC_PEDO_TIMER            (0x40u)
#define ID_ACC_SHAKE_GRAV            (0x02u)
#define ID_ACC_SHAKE_SNAP            (0x04u)
#define ID_ACC_SHAKE_DRAG            (0x08u)
#define ID_ACC_HW_TAP                (0x04u)
#define ID_ACC_SW_TAP                (0x80u)
#define ID_ELECT_TOUCH               (0x01u)
#define ID_ELECT_REREASE             (0x02u)
#define ID_PS_INBAG                  (0x04u)

#define ID_IDX0                      (0u)
#define ID_IDX1                      (8u)
#define ID_IDX2                      (16u)
#define ID_IDX3                      (24u)

/*
   HostCommand Number
 */
/* MCU */
#define HC_MCU_GET_VERSION           (0x0001u)
#define HC_MCU_SOFT_RESET            (0x0002u)
#define HC_MCU_GET_EX_SENSOR         (0x0003u)
#define HC_MCU_SET_PCON              (0x0004u)
#define HC_MCU_GET_PCON              (0x0005u)
#define HC_MCU_GET_INT_DETAIL        (0x0006u)
#define HC_MCU_SET_PORT_OUT          (0x0007u)
#define HC_MCU_ACCESS_SENSOR         (0x0009u)
#define HC_MCU_GET_PORT_OUT          (0x0008u)
#define HC_MCU_SELF_CHK_FW           (0x000Au)
#define HC_MCU_I2C_IO                (0x000Cu)

#define HC_GET_TSK_EXECUTE           (0x0010u)
#define HC_SENSOR_TSK_SET_CYCLE      (0x0011u)
#define HC_SENSOR_TSK_GET_CYCLE      (0x0012u)
#define HC_MCU_SET_PDIR              (0x0013u)
#define HC_MCU_GET_PDIR              (0x0014u)
#define HC_TSK_EXECUTE               (0x000fu)
#define HC_MCU_ASSERT_INT            (0x0702u)
#define HC_MCU_DEASSERT_INT          (0x0303u)
#define HC_MCU_SET_INT1              (0x0304u)

/* Firmware update */
#define HC_MCU_FUP_START             (0x0101u)
#define HC_MCU_FUP_ERASE             (0x0102u)
#define HC_MCU_FUP_WRITE             (0x0103u)
#define HC_MCU_FUP_END               (0x0104u)
#define HC_MCU_FUP_WRITE_FIFO        (0x0105u)
#define HC_MCU_FUP_SELFTEST          (0x000Au)

/* Accelerometer */
#define HC_ACC_SET_OFFSET            (0x1009u)
#define HC_ACC_GET_OFFSET            (0x100au)
#define HC_ACC_SET_POSITION          (0x105eu)
#define HC_ACC_GET_POSITION          (0x105fu)
#define HC_ACC_SET_OPERATION_MODE    (0x1060u)
#define HC_ACC_GET_OPERATION_MODE    (0x1061u)

/* Barometric pressure */
#define HC_BARO_SET_OFFSET           (0x6003u)
#define HC_BARO_GET_OFFSET           (0x6004u)
#define HC_BARO_SET_PARAM            (0x60f0u)
#define HC_BARO_GET_PARAM            (0x60f1u)

/* Magnetic */
#define HC_MAG_SET_OFFSET            (0x7005u)
#define HC_MAG_GET_OFFSET            (0x7006u)
#define HC_MAG_SET_CAL               (0x7007u)
#define HC_MAG_SET_POSITION          (0x7009u)
#define HC_MAG_GET_POSITION          (0x700au)
#define HC_MAG_SET_STATIC_MAT        (0x700du)

/* Gyro */
#define HC_GYRO_SET_OFFSET           (0x8002u)
#define HC_GYRO_GET_OFFSET           (0x8003u)
#define HC_GYRO_SET_CAL              (0x8004u)
#define HC_GYRO_SET_POSITION         (0x8009u)
#define HC_GYRO_GET_POSITION         (0x800au)
#define HC_GYRO_SET_FILTER           (0x800bu)
#define HC_GYRO_GET_FILTER           (0x800cu)
#define HC_GYRO_SET_CALIB            (0x800du)
#define HC_GYRO_GET_CALIB            (0x800eu)
#define HC_GYRO_SET_PARAM            (0x80f0u)
#define HC_GYRO_GET_PARAM            (0x80f1u)

/* G Detection */
#define HC_SET_GDETECTION_PARAM     (0x1180u)
#define HC_GET_GDETECTION_PARAM     (0x1181u)

/* Pickup */
#define HC_PICKUP_SET_ENABLE		(0x1900u)
#define HC_PICKUP_GET_ENABLE		(0x1901u)
#define HC_PICKUP_GET_ISR			(0x1902u)
#define HC_PICKUP_GET_INFO			(0x1903u)
#define HC_PICKUP_SET_PARAM1		(0x1904u)
#define HC_PICKUP_GET_PARAM1		(0x1905u)
#define HC_PICKUP_SET_PARAM2		(0x1906u)
#define HC_PICKUP_GET_PARAM2		(0x1907u)

#define HC_TWIST_SET_ENABLE         (0x1930u)
#define HC_TWIST_GET_ENABLE         (0x1931u)
#define HC_TWIST_GET_ISR            (0x1932u)
#define HC_TWIST_GET_INFO           (0x1933u)

/* LowPower Mode */
#define HC_SET_LPM_PARAM (0x0306u)
#define HC_GET_LPM_PARAM (0x0307u)
#define HC_GET_LPM_INFO  (0x0308u)

/* StepCounter/StepDetector/SignificantMotion */
#define HC_SET_PEDO_STEP_PARAM             (0x1100u+0x220)
#define HC_GET_PEDO_STEP_PARAM             (0x1101u+0x220)
#define HC_SET_PEDO_CALORIE_FACTOR         (0x1104u+0x220)
#define HC_GET_PEDO_CALORIE_FACTOR         (0x1105u+0x220)
#define HC_GET_PEDO_STEP_DATA              (0x1106u+0x220)
#define HC_CLR_PEDO_STEP_DATA              (0x1107u+0x220)
#define HC_SET_PEDO_RUN_DETECT_PARAM       (0x110eu+0x220)
#define HC_GET_PEDO_RUN_DETECT_PARAM       (0x110fu+0x220)
#define HC_GET_PEDO_RUN_DETECT_DATA        (0x1110u+0x220)
#define HC_CLR_PEDO_RUN_DETECT_DATA        (0x1111u+0x220)
#define HC_SET_ACTIVITY_DETECT_PARAM       (0x1112u+0x220)
#define HC_GET_ACTIVITY_DETECT_PARAM       (0x1113u+0x220)
#define HC_GET_ACTIVITY_DETECT_DATA        (0x1116u+0x220)
#define HC_CLR_ACTIVITY_DETECT_DATA        (0x1117u+0x220)
#define HC_GET_ACTIVITY_DETECT_TIME        (0x1118u+0x220)
#define HC_CLR_ACTIVITY_DETECT_TIME        (0x1119u+0x220)
#define HC_SET_ACTIVITY_TOTAL_DETECT_PARAM (0x111au+0x220)
#define HC_GET_ACTIVITY_TOTAL_DETECT_PARAM (0x111bu+0x220)
#define HC_GET_ACTIVITY_TOTAL_DETECT_DATA  (0x111cu+0x220)
#define HC_CLR_ACTIVITY_TOTAL_DETECT_CONT  (0x111fu+0x220)
#define HC_GET_ACTIVITY_TOTAL_DETECT_INFO  (0x1120u+0x220)
#define HC_SET_PEDO_STEP_PARAM2            (0x1126u+0x220)
#define HC_GET_PEDO_STEP_PARAM2            (0x1127u+0x220)

/* Activity detection (Extension Function) */
#define HC_SET_PEDO2_STEP_PARAM             (0x1100u)
#define HC_GET_PEDO2_STEP_PARAM             (0x1101u)
#define HC_SET_PEDO2_CALORIE_FACTOR         (0x1104u)
#define HC_GET_PEDO2_CALORIE_FACTOR         (0x1105u)
#define HC_GET_PEDO2_STEP_DATA              (0x1106u)
#define HC_CLR_PEDO2_STEP_DATA              (0x1107u)
#define HC_SET_PEDO2_RUN_DETECT_PARAM       (0x110eu)
#define HC_GET_PEDO2_RUN_DETECT_PARAM       (0x110fu)
#define HC_GET_PEDO2_RUN_DETECT_DATA        (0x1110u)
#define HC_CLR_PEDO2_RUN_DETECT_DATA        (0x1111u)
#define HC_SET_ACTIVITY2_DETECT_PARAM       (0x1112u)
#define HC_GET_ACTIVITY2_DETECT_PARAM       (0x1113u)
#define HC_GET_ACTIVITY2_DETECT_DATA        (0x1116u)
#define HC_CLR_ACTIVITY2_DETECT_DATA        (0x1117u)
#define HC_GET_ACTIVITY2_DETECT_TIME        (0x1118u)
#define HC_CLR_ACTIVITY2_DETECT_TIME        (0x1119u)
#define HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM (0x111au)
#define HC_GET_ACTIVITY2_TOTAL_DETECT_PARAM (0x111bu)
#define HC_GET_ACTIVITY2_TOTAL_DETECT_DATA  (0x111cu)
#define HC_CLR_ACTIVITY2_TOTAL_DETECT_CONT  (0x111fu)
#define HC_GET_ACTIVITY2_TOTAL_DETECT_INFO  (0x1120u)
#define HC_SET_ACTIVITY2_DETECT_PARAM2      (0x1121u)
#define HC_GET_ACTIVITY2_DETECT_PARAM2      (0x1122u)
#define HC_INIT_PEDO_AND_ACTIVITY_DETECT    (0x1123u)
#define HC_SET_ACTIVITY2_DETECT_PARAM3      (0x1124u)
#define HC_GET_ACTIVITY2_DETECT_PARAM3      (0x1125u)
#define HC_SET_PEDO2_STEP_PARAM2            (0x1126u)
#define HC_GET_PEDO2_STEP_PARAM2            (0x1127u)
#define HC_SET_ACTIVITY2_PAUSE_PARAM        (0x1128u)
#define HC_GET_ACTIVITY2_PAUSE_PARAM        (0x1129u)
#define HC_SET_ACTIVITY2_PAUSE_STATUS_PARAM (0x112au)
#define HC_GET_ACTIVITY2_PAUSE_STATUS_PARAM (0x112bu)
#define HC_CLR_ACTIVITY2_PAUSE_PARAM        (0x112cu)
#define HC_SET_RIDE_PEDO_PARAM               (0x112du)
#define HC_GET_RIDE_PEDO_PARAM               (0x112eu)
#define HC_SET_RIDE_PEDO2_PARAM              (0x112du+0x220)
#define HC_GET_RIDE_PEDO2_PARAM              (0x112eu+0x220)
#define HC_ACC_PEDO_SET_ALL_STATE_2ND       (0x1133u)
#define HC_ACC_PEDO_GET_ALL_STATE_2ND       (0x1134u)

/* Logging(batch mode) */
#define HC_LOGGING_SENSOR_SET_PARAM  (0xf001u)
#define HC_LOGGING_SENSOR_GET_PARAM  (0xf002u)
#define HC_LOGGING_SENSOR_SET_CYCLE  (0xf003u)
#define HC_LOGGING_SENSOR_GET_CYCLE  (0xf004u)
#define HC_LOGGING_FUSION_SET_PARAM  (0xf005u)
#define HC_LOGGING_FUSION_GET_PARAM  (0xf006u)
#define HC_LOGGING_FUSION_SET_CYCLE  (0xf007u)
#define HC_LOGGING_FUSION_GET_CYCLE  (0xf008u)
#define HC_LOGGING_GET_RESULT        (0xf009u)
#define HC_LOGGING_DELIMITER         (0xf00au)
#define HC_SENSOR_SET_PARAM          (0xf00bu)
#define HC_SENSOR_GET_PARAM          (0xf00cu)
#define HC_SENSOR_SET_CYCLE          (0xf00du)
#define HC_SENSOR_GET_RESULT         (0xf010u)
#define HC_LOGGING_SET_NOTIFY        (0xf01bu)
#define HC_LOGGING_GET_NOTIFY        (0xf01cu)
#define HC_LOGGING_SET_PEDO         (0xf022u)
#define HC_LOGGING_GET_PEDO         (0xf023u)
#define HC_LOGGING_SET_PEDO2        (0xf020u)
#define HC_LOGGING_GET_PEDO2        (0xf021u)
/* Fusion */
#define HC_SET_FUISON_PARAM          (0xf015u)
/* Motion Detection */
#define HC_SET_MOTDETECTION_EN             (0x1301u)
#define HC_GET_MOTDETECTION_EN             (0x1302u)
#define HC_SET_MOTDETECTION_PARAM          (0x1303u)
#define HC_GET_MOTDETECTION_PARAM          (0x1304u)
#define HC_SET_MOTDETECTION_INT            (0x1305u)
#define HC_GET_MOTDETECTION_INT            (0x1306u)
#define HC_GET_MOTDETECTION_SENS           (0x1307u)

#define HC_SET_LOW_POWER_PARAM              (0x1310u)
#define HC_GET_LOW_POWER_PARAM              (0x1311u)
#define HC_SET_LOW_POWER_DEV_PARAM          (0x1313u)
#define HC_GET_LOW_POWER_DEV_PARAM          (0x1314u)
#define HC_CLR_LOW_POWER_MODE               (0x1315u)
#define HC_SET_LOW_POWER_DEV_PARAM2          (0x1316u)
#define HC_GET_LOW_POWER_DEV_PARAM2          (0x1317u)

/*
   Exist sensor flag
 */
#define HC_INVALID                   (0x00u)
#define HC_ACC_VALID                 (0x01u)
#define HC_BARO_VALID                (0x20u)
#define HC_MAG_VALID                 (0x40u)
#define HC_GYRO_VALID                (0x80u)
#define HC_ORI_VALID                 (0x01u)
#define HC_GRAVITY_VALID             (0x02u)
#define HC_LACC_VALID                (0x04u)
#define HC_RV_VALID                  (0x08u)
#define HC_RV_NONMAG_VALID           (0x10u)
#define HC_RV_NONGYRO_VALID          (0x20u)
#define HC_ORI_MAG_VALID             (0x40u)

/*
   Logging Pedometer flag
 */
#define HC_FLG_LOGGING_PEDO                   (0x0001u)
#define HC_FLG_LOGGING_TOTAL_STATUS           (0x0002u)

/*
   Task execute command parameter
 */
#define TSK_SENSOR                   (0x01)
#define TSK_APP                      (0x02)
#define TSK_FUSION                   (0x04)

/*
   Host comannd execute type
 */
#define EXE_HOST_WAIT                (1)
#define EXE_HOST_RES                 (2)
#define EXE_HOST_ERR                 (4)
#define EXE_HOST_ALL                 (EXE_HOST_WAIT|EXE_HOST_RES|EXE_HOST_ERR)
#define EXE_HOST_EX_NO_RECOVER       (16)
#define EXE_HOST_RES_RSLT            (32)
#define EXE_HOST_RES_ONLY_FIFO_SIZE  (64)
#define EXE_HOST_SKIP_MUTEX_UNLOCK   (128)
#define EXE_HOST_ALL_RSLT            (EXE_HOST_WAIT|EXE_HOST_RES_RSLT|EXE_HOST_ERR)
#define EXE_HOST_CK_CONNECT          (8)
#define SHUB_CMD_RETRY_NUM           (3)
#define SHUB_SEND_CMD_CONNECT        (0x0001u)
#define SHUB_SEND_CMD_HOST           (0x0002u)

/*
   Firmware update command error code
 */
#define ERROR_FUP_MAXSIZE            (0x0011u)
#define ERROR_FUP_VERIFY             (0x0012u)
#define ERROR_FUP_CERTIFICATION      (0x0013u)
#define ERROR_FUP_ODDSIZE            (0x0014u)

/*
   Driver error code
 */
#define SHUB_RC_OK                 (0)
#define SHUB_RC_ERR                (-1)
#define SHUB_RC_ERR_TIMEOUT        (-2)

/*
   Timeout
 */
#define WAITEVENT_TIMEOUT            (2000)
#define WAIT_PEDOEVENT_TIMEOUT       (3000)
#define WAIT_CHECK_CONNECT           (200)


/*
   Sensor Power Flag
 */
#define POWER_DISABLE       false
#define POWER_ENABLE        true

/*
   Sensor active Flag
 */
#define ACTIVE_OFF          0x00
#define ACTIVE_ON           0x01

#define ACC_SPI_RETRY_NUM    5

#define ACC_WORK_QUEUE_NUM  11 

#define SHUB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SHUB_MAX(a, b) ((a) > (b) ? (a) : (b))

/*
   Task time 
 */
#define MEASURE_MAX_US            (400*1000)  /* (400000 -> 200000 -> 400000) */
#define SENSOR_TSK_DEFALUT_US     (5000)      /* (  3750 ->   5000) */
#define FUSION_TSK_DEFALUT_US     (20000)     /* ( 15000 ->  20000) */
#define APP_TSK_DEFALUT_US        (30000)

#define SENSOR_TSK_MIN_LOGGING_US (5000)      /* (  1875 ->   5000) */
#define FUSION_TSK_MIN_US         (20000)     /* ( 15000 ->  20000) */

#define SENSOR_ACC_MIN_DELAY      (5000)      /* (  7500 ->   5000) */

#define SENSOR_MAG_MIN_DELAY      (20000)     /* ( 15000 ->  20000) */
#define SENSOR_MAG_MAX_DELAY      (20000)     /* (       ->  20000) */
#define SENSOR_MAGUC_MIN_DELAY    (20000)     /* ( 15000 ->  20000) */
#define SENSOR_MAGUC_MAX_DELAY    (20000)     /* (       ->  20000) */

#define SENSOR_GYRO_MIN_DELAY     (5000)      /* (  3750 ->   5000) */
#define SENSOR_GYRO_MAX_DELAY     (20000)     /* (  7500 ->  20000) */
#define SENSOR_GYROUC_MIN_DELAY   (5000)      /* (  3750 ->   5000) */
#define SENSOR_GYROUC_MAX_DELAY   (20000)     /* (  7500 ->  20000) */

#define SENSOR_BARO_MIN_DELAY     (5000)      /* (  3750 ->   5000) */
#define SENSOR_BARO_MAX_DELAY     (1000000)
#define SENSOR_BARO_MID_DELAY     (200000)
#define SENSOR_BARO_LOW_DELAY     (100000)

#define FUSION_MIN_DELAY          (20000)     /* ( 15000 ->  20000) */
#define FUSION_ACC_DELAY          (5000)      /* (  7500 ->   5000) */
#define FUSION_GYRO_DELAY         (20000)     /* (  7500 ->  20000) */
#define FUSION_MAG_DELAY          (20000)     /* ( 15000 ->  20000) */
#define APP_MIN_DELAY             (30000)
#define GDEC_DEFALUT_US           (30000)
#define PICKUP_DEFALUT_US         (30000)
#define TWIST_DEFALUT_US          (30000)

// #define DELIMITER_LOGGING_SIZE    (458) /* xxxx - F101 */
// #define DELIMITER_LOGGING_SIZE    (454) /* F102 - F103 */
#define DELIMITER_LOGGING_SIZE    (414)    /* F104 - xxxx */
#define DELIMITER_TIMESTAMP_SIZE  (FIFO_SIZE - DELIMITER_LOGGING_SIZE)

#define LSI_ML630Q790             (0x0790)
#define LSI_ML630Q791             (0x0791)

#ifdef CONFIG_SET_AXIS_VAL
#define SHUB_ACC_AXIS_VAL           (0)
#define SHUB_MAG_AXIS_VAL           (0)
#define SHUB_GYRO_AXIS_VAL          (0)
#endif

/*
   GPIO
 */
#define USE_RESET_SIGNAL
#define USE_REMAP_SIGNAL

//#define SHUB_GPIO_INT0     (61)
#define SHUB_GPIO_INT0_NAME   "shub_hostif_int0"

#ifdef USE_RESET_SIGNAL
//#define SHUB_GPIO_RST        (120)
//#define SHUB_GPIO_RESET_NAME "shub_reset"
#endif

#ifdef USE_REMAP_SIGNAL
//#define SHUB_GPIO_REMP       (120)
//#define SHUB_GPIO_REMP_NAME  "shub_remap"
#endif

#define SHUB_RESET_PLUSE_WIDTH (1)

#ifdef CONFIG_ACC_U2DH
#define SHUB_RESET_TIME        (60)
#else
#define SHUB_RESET_TIME        (20)
#endif

//data opecord
#define DATA_OPECORD_ACC             0x01
#define DATA_OPECORD_MAG             0x02
#define DATA_OPECORD_GYRO            0x03
#define DATA_OPECORD_MAG_CAL_OFFSET  0x04
#define DATA_OPECORD_GYRO_CAL_OFFSET 0x05
#define DATA_OPECORD_ORI             0x10
#define DATA_OPECORD_GRAVITY         0x11
#define DATA_OPECORD_LINEARACC       0x12
#define DATA_OPECORD_RVECT           0x13
#define DATA_OPECORD_GAMERV          0x14
#define DATA_OPECORD_GEORV           0x15
#define DATA_OPECORD_PEDOCNT2        0x23
#define DATA_OPECORD_TOTAL_STATUS2   0x24
#define DATA_OPECORD_PEDOCNT         0x25
#define DATA_OPECORD_TOTAL_STATUS    0x26
#ifdef CONFIG_BARO_SENSOR
#define DATA_OPECORD_BARO            0x06
#endif

#define AXIS_X (0)
#define AXIS_Y (1)
#define AXIS_Z (2)
#define INTDETAIL_GDETECT            0x00000001
#define INTDETAIL2_PEDOM_CNT         0x00000002
#define INTDETAIL2_PEDOM_STOP        0x00000008
#define INTDETAIL2_PEDOM_RUN         0x00000010
#define INTDETAIL2_PEDOM_TRANS       0x00000020
#define INTDETAIL2_PEDOM_TOTAL_STATE 0x01000000
#define INTDETAIL2_PEDOM_RIDE_PAUSE  0x02000000
#define INTDETAIL2_PEDOM_SIGNIFICANT 0x80000000

#define INTDETAIL_PEDOM_CNT          0x00000001
#define INTDETAIL_PEDOM_STOP         0x00000004
#define INTDETAIL_PEDOM_RUN          0x00000008
#define INTDETAIL_PEDOM_TRANS        0x00000010
#define INTDETAIL_PEDOM_TOTAL_STATE  0x00000080
#define INTDETAIL_PEDOM_SIGNIFICANT  0x00000100
#define INTDETAIL2_PEDOM_PICKUP      0x00000400

#define INT_GYRO_CALIB              0x01
#define INT_GYRO_TWIST_RIGHT        0x02
#define INT_GYRO_TWIST_LEFT         0x04

#define APP_PEDOMETER               0x01
#define APP_CALORIE_FACTOR          0x02
#define APP_RUN_DETECTION           0x03
#define APP_VEICHLE_DETECTION       0x04
#define APP_TOTAL_STATUS_DETECTION  0x05
#define APP_GDETECTION              0x06
#define APP_MOTDTECTION             0x07
#define MCU_TASK_CYCLE              0x08
#define APP_TOTAL_STATUS_DETECTION_CLR_CONT_STEPS 0x09
#define APP_TOTAL_STATUS_DETECTION_CLR_CONT_STOP  0x0A
#define APP_NORMAL_PEDOMETER        0x0B
#define APP_LOW_POWER               0x0C
#define APP_VEICHLE_DETECTION2      0x0E
#define APP_CLEAR_PEDOM_AND_TOTAL_STATUS_DETECTION 0x0F
#define APP_VEICHLE_DETECTION3      0x10
#define APP_PEDOMETER2              0x11
#define APP_PEDOMETER2_2            0x12
#define APP_PICKUP_ENABLE           0x13
//#define APP_PICKUP_PARAM1           0x14
//#define APP_PICKUP_PARAM2           0x15
#define APP_PAUSE_PARAM             0x16
#define APP_PAUSE_STATUS_PARAM      0x17
#define APP_LPM_PARAM               0x18
#define APP_LPM_DEV_PARAM           0x19
#define APP_RIDE_PEDO_PARAM         0x20
#define APP_RIDE_PEDO2_PARAM        0x21
#define APP_LPM_DEV_PARAM2          0x22
#define APP_TOTAL_STATUS_DETECTION2 0x23
#define APP_CLEAR_PAUSE_PARAM       0x24
#define APP_TWIST_ENABLE            0x25
#define APP_PEDO_ALL_STATE_2ND      0x26

#define APP_PEDOMETER_N             (0x81)

#define SHUB_FACE_CHECK_X_MIN        (-50)
#define SHUB_FACE_CHECK_X_MAX        (50)
#define SHUB_FACE_CHECK_Y_MIN        (-50)
#define SHUB_FACE_CHECK_Y_MAX        (50)
#define SHUB_FACE_CHECK_Z_MIN_DOWN   (-1100)
#define SHUB_FACE_CHECK_Z_MAX_DOWN   (-900)
#define SHUB_FACE_CHECK_Z_MIN_UP     (900)
#define SHUB_FACE_CHECK_Z_MAX_UP     (1100)

#define SHUB_SIZE_PEDO_STEP_PRM      14

#define SHUB_PICKUP_ENABLE_PARAM_STILL  (0x10)

#define SHUB_BARO_BUFFER_SIZE       (5)

#ifdef CONFIG_ANDROID_ENGINEERING
#define SHUB_BARO_FILTER_NONE       (0)
#define SHUB_BARO_FILTER_MEDIAN     (1)
static int baro_filter = SHUB_BARO_FILTER_MEDIAN;
module_param(baro_filter, int, 0600);
#endif

///////////////////////////////////////
// union
///////////////////////////////////////
typedef union {
    uint16_t   u16;
    uint8_t    u8[2];
} Word;

typedef union {
    uint8_t    u8[PRM_SIZE];
    int8_t     s8[PRM_SIZE];
    uint16_t   u16[PRM_SIZE/2];
    int16_t    s16[PRM_SIZE/2];
    uint32_t   u32[PRM_SIZE/4];
    int32_t    s32[PRM_SIZE/4];
} HCParam;

typedef union {
    uint8_t    u8[FIFO_SIZE];
    int8_t     s8[FIFO_SIZE];
    uint16_t   u16[FIFO_SIZE/2];
    int16_t    s16[FIFO_SIZE/2];
    uint32_t   u32[FIFO_SIZE/4];
    int32_t    s32[FIFO_SIZE/4];
} HCRes;

///////////////////////////////////////
// struct
///////////////////////////////////////
typedef struct {
    Word       cmd;
    HCParam    prm;
} HostCmd;

typedef struct {
    HCRes      res;
    Word       err;
    int16_t    res_size;
} HostCmdRes;

typedef struct {
    uint32_t acc;
    uint32_t mag;
    uint32_t mag_uc;
    uint32_t gyro;
    uint32_t gyro_uc;
    uint32_t baro;
    uint32_t orien;
    uint32_t grav;
    uint32_t linear;
    uint32_t rot;
    uint32_t rot_gyro;
    uint32_t rot_mag;
    uint32_t pedocnt;
    uint32_t total_status;
    uint32_t pedocnt2;
    uint32_t total_status2;
}TotalOfDeletedTimestamp, SensorDelay;

typedef struct {
    ktime_t acc;
    ktime_t mag;
    ktime_t mag_uc;
    ktime_t gyro;
    ktime_t gyro_uc;
    ktime_t baro;
    ktime_t orien;
    ktime_t grav;
    ktime_t linear;
    ktime_t rot;
    ktime_t rot_gyro;
    ktime_t rot_mag;
    ktime_t pedocnt;
    ktime_t total_status;
    ktime_t pedocnt2;
    ktime_t total_status2;
}BaseTimestamp;

struct acceleration {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nAccuracy;
};

struct quaternion {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nS;
    int32_t nAccuracy;
};

struct gyroscope {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nXOffset;
    int32_t nYOffset;
    int32_t nZOffset;
    int32_t nAccuracy;
};

struct magnetic {
    int32_t nX;
    int32_t nY;
    int32_t nZ;
    int32_t nXOffset;
    int32_t nYOffset;
    int32_t nZOffset;
    int32_t nAccuracy;
};

struct barometric {
    uint32_t pressure;
    uint32_t buffer[SHUB_BARO_BUFFER_SIZE];
    uint8_t  pos;
    uint8_t  num;
};

struct orientation {
    int32_t pitch;
    int32_t roll;
    int32_t yaw;
    int32_t nAccuracy;
};

struct stepcount {
    uint64_t step;
    uint64_t stepOffset;
    uint64_t stepDis;
};

struct micon_hostcmd_param {
    uint8_t task[3];
    uint16_t task_cycle[3];
    uint16_t sensors;
    uint32_t s_cycle[4];
    uint8_t mag_cal;
    uint8_t gyro_cal;
    uint8_t gyro_filter;
    uint8_t fusion;
    uint16_t logg_sensors;
    uint16_t logg_fusion;
};

typedef struct t_SHUB_WorkQueue {
    struct work_struct  work;
    bool                status;
} SHUB_WorkQueue;


///////////////////////////////////////
// static
///////////////////////////////////////

static SHUB_WorkQueue  s_tAccWork[ACC_WORK_QUEUE_NUM];
static int32_t g_nIntIrqFlg;
static int32_t g_nIntIrqNo;
static int32_t s_nAccWorkCnt;
static uint16_t g_hostcmdErr;
static wait_queue_head_t s_tWaitInt;

static struct workqueue_struct *accsns_wq_int;
static DEFINE_SPINLOCK(acc_lock);

static struct acceleration s_tLatestAccData;
static struct gyroscope s_tLatestGyroData;
static struct magnetic s_tLatestMagData;
static struct barometric s_tLatestBaroData;
static struct barometric s_tLoggingBaroData;
static struct orientation s_tLatestOriData;
static struct acceleration s_tLatestGravityData;
static struct acceleration s_tLatestLinearAccData;
static struct quaternion s_tLatestRVectData;
static struct quaternion s_tLatestGameRVData;
static struct quaternion s_tLatestGeoRVData;
static struct stepcount s_tLatestStepCountData;

static struct micon_hostcmd_param s_micon_param;

static atomic_t g_CurrentSensorEnable;
static atomic_t g_CurrentLoggingSensorEnable;

static atomic_t g_bIsIntIrqEnable;
static atomic_t g_WakeupSensor;
static atomic_t g_FWUpdateStatus;

struct work_struct fusion_irq_work;
struct work_struct acc_irq_work;
struct work_struct significant_work;
struct work_struct gyro_irq_work;
struct work_struct mag_irq_work;
struct work_struct baro_irq_work;
struct work_struct customer_irq_work;

static BaseTimestamp s_beseTime;
static BaseTimestamp s_pending_baseTime;
static SensorDelay s_sensor_delay_us;
static SensorDelay s_logging_delay_us;
static SensorDelay shub_logging_cycle;
static int32_t s_sensor_task_delay_us;
static bool s_enable_notify_step;
static bool shub_connect_flg = false;
static bool shub_fw_write_flg = false;
static uint32_t s_lsi_id;
static uint32_t s_exist_sensor = 0;

static int32_t shub_basetime_req = 0;
static ktime_t shub_baseTime;
static int32_t shub_gyro_entime_flg = 0;
static ktime_t s_gyro_enable_time_s;
static ktime_t s_gyro_enable_time_e;

static uint8_t shub_lowpower_mode = 0;
static uint8_t shub_operation_mode = 1;
static uint16_t shub_failed_init_param = 0;
static uint32_t shub_err_code = SHUB_FUP_NO_COMMAND;

static bool shub_mot_still_enable_flag = false;

#ifdef SHUB_SUSPEND
static bool s_is_suspend;
#endif

static int32_t shub_sensor_info[NUM_SHUB_SAME_NOTIFY_KIND] = {0};
int32_t shub_get_sensor_activate_info(int kind)
{
    return shub_sensor_info[kind];
}

static int32_t shub_sensor_first_measure_info[NUM_SHUB_SAME_NOTIFY_KIND] = {0};
int32_t shub_get_sensor_first_measure_info(int kind)
{
    return shub_sensor_first_measure_info[kind];
}

static int shub_sensor_same_delay_flg[NUM_SHUB_SAME_NOTIFY_KIND] = {0};
int shub_get_sensor_same_delay_flg(int kind)
{
    return shub_sensor_same_delay_flg[kind];
}


///////////////////////////////////////
// Mutex
///////////////////////////////////////

static DEFINE_MUTEX(userReqMutex);
static DEFINE_MUTEX(s_tDataMutex);
static DEFINE_MUTEX(s_hostCmdMutex);
static DEFINE_SPINLOCK(s_intreqData);

#include <linux/wakelock.h>

static int shub_wakelock_timeout = HZ;
#ifdef CONFIG_ANDROID_ENGINEERING
module_param(shub_wakelock_timeout, int, 0600);
#endif
static struct wake_lock shub_irq_wake_lock;
static struct wake_lock shub_int_wake_lock;
static struct wake_lock shub_acc_wake_lock;
static struct wake_lock shub_gyro_wake_lock;
static struct wake_lock shub_mag_wake_lock;

#ifdef CONFIG_BARO_SENSOR
static struct wake_lock shub_baro_wake_lock;
#endif

static struct wake_lock shub_customer_wake_lock;
static struct wake_lock shub_fusion_wake_lock;
static struct wake_lock shub_timer_wake_lock;
static spinlock_t shub_wake_spinlock;
static bool shub_suspend_call_flg = false;

#include <linux/pm_qos.h>
//#include <mach/cpuidle.h>

#define SHUB_PM_QOS_LATENCY_VALUE 1
static struct pm_qos_request shub_qos_cpu_dma_latency;
static DEFINE_MUTEX(qosMutex);
static int shub_wake_lock_num = 0;

static int16_t shub_acc_offset[3] = {0};
static int16_t shub_mag_axis_interfrence[9] = {0};

#define ENABLE_IRQ {                                                         \
    if((g_nIntIrqNo != -1) && (atomic_read(&g_bIsIntIrqEnable) == false)){   \
        atomic_set(&g_bIsIntIrqEnable,true);                                 \
        enable_irq(g_nIntIrqNo);                                             \
    }                                                                        \
}
#define DISABLE_IRQ {                                                        \
    if((g_nIntIrqNo != -1) && (atomic_read(&g_bIsIntIrqEnable) == true)){    \
        disable_irq_nosync(g_nIntIrqNo);                                     \
        atomic_set(&g_bIsIntIrqEnable,false);                                \
    }                                                                        \
}

#define ERR_WAKEUP {                                                \
    atomic_set(&g_WakeupSensor, SHUB_ACTIVE_ERROR);                 \
}

#define SUB_FW_VERSION(data) ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]))

#define U8_TO_S16(data) ((int16_t)(((uint32_t)*data ) | ((uint32_t)(*(data+1) << 8))))
#define U8_TO_U16(data) ((uint16_t)(((uint32_t)*data ) | ((uint32_t)(*(data+1) << 8))))
#define U8_TO_U24(data) (((uint32_t)*data ) | ((uint32_t)(*(data+1) << 8))| ((uint32_t)(*(data+2) << 16)))

#define RESU8_TO_X32(res, pos)  \
    ((res).res.u8[(pos+3)] << 24 | \
     (res).res.u8[(pos+2)] << 16 | \
     (res).res.u8[(pos+1)] << 8  | \
     (res).res.u8[(pos+0)])

#define RESU8_TO_X16(res, pos)  \
    ((res).res.u8[(pos+1)] << 8  | \
     (res).res.u8[(pos+0)])

#define RESU8_TO_X8(res, pos) (res).res.u8[(pos)]

#define RESU8_SHUB_ERR_CODE(cmd, res, pos) ((cmd.cmd.u16 << 16) | (res.res.u8[(pos)]))
#define RESU16_SHUB_ERR_CODE(cmd, res) ((cmd.cmd.u16 << 16) | (res.err.u16))


///////////////////////////////////////
// extern function 
///////////////////////////////////////
extern int32_t hostif_write_proc(uint8_t adr, const uint8_t *data, uint16_t size);
extern int32_t hostif_read_proc(uint8_t adr, uint8_t *data, uint16_t size);
extern int32_t initBatchingProc(void);
extern int32_t suspendBatchingProc(void);
extern int32_t resumeBatchingProc(void);

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// STATIC SYMBOL
//
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// static void pending_base_time(int32_t arg_iSensType);
static void pending_base_time(int32_t arg_iSensType, ktime_t *time);
static int32_t update_base_time(int32_t arg_iSensType, ktime_t *time );
static struct timespec event_time_to_offset(int32_t arg_iSensType,int32_t eventOffsetTime);
static int32_t sensor_get_logging_data(uint8_t* buf, int32_t size);
static int32_t logging_flush_exec(void);
static int32_t shub_read_sensor_data(int32_t arg_iSensType);
static int32_t shub_hostcmd(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size);
static int32_t shub_waitcmd(uint16_t intBit);
static irqreturn_t shub_irq_handler(int32_t irq, void *dev_id);
static void shub_int_work_func(struct work_struct *work);
static void shub_int_acc_work_func(struct work_struct *work);
static void shub_int_mag_work_func(struct work_struct *work);
static void shub_int_customer_work_func(struct work_struct *work);
static void shub_int_gyro_work_func(struct work_struct *work);
static void shub_int_fusion_work_func(struct work_struct *work);
static int32_t shub_set_delay_exec(int32_t arg_iSensType, int32_t arg_iLoggingType);
static int32_t shub_activate_exec(int32_t arg_iSensType, int32_t arg_iEnable, int mode);
static int32_t shub_activate_logging_exec(int32_t arg_iSensType, int32_t arg_iEnable);
static int32_t shub_gpio_init(void);
static void shub_workqueue_init(void);
static int32_t shub_workqueue_create( struct workqueue_struct *queue, void (*func)(struct work_struct *) );
static void shub_workqueue_delete(struct work_struct *work);
static int32_t hostif_write(uint8_t adr, const uint8_t *data, uint16_t size);
static int32_t hostif_read(uint8_t adr, uint8_t *data, uint16_t size);
static int32_t shub_get_data_app_exec(int32_t arg_iType,int32_t *data);
static int32_t shub_clear_data_app_exec(int32_t type);
static int32_t shub_get_param_exec(int32_t type,int32_t *data);
static int32_t shub_set_param_exec(int32_t type,int32_t *data);
static int32_t shub_activate_significant_exec(int32_t arg_iSensType, int32_t arg_iEnable, uint8_t * notify);
static int32_t shub_activate_pedom_exec(int32_t arg_iSensType, int32_t arg_iEnable);
static int32_t shub_init_app_exec(int32_t type);
static struct timespec shub_get_timestamp(void);
#ifdef NO_LINUX
#include "test/test.c"
#endif

static void shub_wake_lock_init(void)
{
    spin_lock_init(&shub_wake_spinlock);
    wake_lock_init(&shub_irq_wake_lock,      WAKE_LOCK_SUSPEND, "shub_irq_wake_lock");
    wake_lock_init(&shub_int_wake_lock,      WAKE_LOCK_SUSPEND, "shub_int_wake_lock");
    wake_lock_init(&shub_acc_wake_lock,      WAKE_LOCK_SUSPEND, "shub_acc_wake_lock");
    wake_lock_init(&shub_gyro_wake_lock,     WAKE_LOCK_SUSPEND, "shub_gyro_wake_lock");
    wake_lock_init(&shub_mag_wake_lock,      WAKE_LOCK_SUSPEND, "shub_mag_wake_lock");
#ifdef CONFIG_BARO_SENSOR
    wake_lock_init(&shub_baro_wake_lock,     WAKE_LOCK_SUSPEND, "shub_baro_wake_lock");
#endif
    wake_lock_init(&shub_customer_wake_lock, WAKE_LOCK_SUSPEND, "shub_customer_wake_lock");
    wake_lock_init(&shub_fusion_wake_lock,   WAKE_LOCK_SUSPEND, "shub_fusion_wake_lock");
    wake_lock_init(&shub_timer_wake_lock,    WAKE_LOCK_SUSPEND, "shub_timer_wake_lock");
    return;
}

static void shub_wake_lock_destroy(void)
{

//    unsigned long flags;

//    spin_lock_irqsave(&shub_wake_spinlock, flags);

    if (wake_lock_active(&shub_irq_wake_lock)){
        wake_unlock(&shub_irq_wake_lock);
    }
    if (wake_lock_active(&shub_int_wake_lock)){
        wake_unlock(&shub_int_wake_lock);
    }
    if (wake_lock_active(&shub_acc_wake_lock)){
        wake_unlock(&shub_acc_wake_lock);
    }
    if (wake_lock_active(&shub_gyro_wake_lock)){
        wake_unlock(&shub_gyro_wake_lock);
    }
    if (wake_lock_active(&shub_mag_wake_lock)){
        wake_unlock(&shub_mag_wake_lock);
    }
#ifdef CONFIG_BARO_SENSOR
    if (wake_lock_active(&shub_baro_wake_lock)){
        wake_unlock(&shub_baro_wake_lock);
    }
#endif
    if (wake_lock_active(&shub_customer_wake_lock)){
        wake_unlock(&shub_customer_wake_lock);
    }
    if (wake_lock_active(&shub_fusion_wake_lock)){
        wake_unlock(&shub_fusion_wake_lock);
    }
    if (wake_lock_active(&shub_timer_wake_lock)){
        wake_unlock(&shub_timer_wake_lock);
    }
    wake_lock_destroy(&shub_irq_wake_lock);
    wake_lock_destroy(&shub_int_wake_lock);
    wake_lock_destroy(&shub_acc_wake_lock);
    wake_lock_destroy(&shub_gyro_wake_lock);
    wake_lock_destroy(&shub_mag_wake_lock);
#ifdef CONFIG_BARO_SENSOR
    wake_lock_destroy(&shub_baro_wake_lock);
#endif
    wake_lock_destroy(&shub_customer_wake_lock);
    wake_lock_destroy(&shub_fusion_wake_lock);
    wake_lock_destroy(&shub_timer_wake_lock);

//    spin_unlock_irqrestore(&shub_wake_spinlock, flags);
    return;
}

static void shub_wake_lock_start(struct wake_lock *wl)
{
    unsigned long flags;

    spin_lock_irqsave(&shub_wake_spinlock, flags);


    if(!shub_suspend_call_flg
      || (strcmp(wl->ws.name,"shub_irq_wake_lock")
      &&  strcmp(wl->ws.name,"shub_int_wake_lock"))){
        if (!wake_lock_active(wl)){
            wake_lock(wl);
        }
    }

    spin_unlock_irqrestore(&shub_wake_spinlock, flags);

    return;
}

static void shub_wake_lock_end(struct wake_lock *wl)
{
    unsigned long flags;

    spin_lock_irqsave(&shub_wake_spinlock, flags);

    if(!shub_suspend_call_flg
      || (strcmp(wl->ws.name,"shub_irq_wake_lock")
      &&  strcmp(wl->ws.name,"shub_int_wake_lock"))){
        if (wake_lock_active(wl)){
            wake_unlock(wl);
        }
    }

    spin_unlock_irqrestore(&shub_wake_spinlock, flags);

    return;
}

static void shub_timer_wake_lock_start(void)
{
    unsigned long flags;

    spin_lock_irqsave(&shub_wake_spinlock, flags);

    if (wake_lock_active(&shub_timer_wake_lock)){
        wake_unlock(&shub_timer_wake_lock);
    }
    wake_lock_timeout(&shub_timer_wake_lock, shub_wakelock_timeout);

    spin_unlock_irqrestore(&shub_wake_spinlock, flags);

    return;
}

static void shub_qos_init(void)
{
    shub_wake_lock_num = 0;
    mutex_init(&qosMutex);
	shub_qos_cpu_dma_latency.type = PM_QOS_REQ_AFFINE_CORES;
	shub_qos_cpu_dma_latency.cpus_affine.bits[0] = 0x0f; /* little cluster */
 	pm_qos_add_request(&shub_qos_cpu_dma_latency, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
    return;
}

static void shub_qos_destroy(void)
{
    shub_wake_lock_num = 0;
    pm_qos_remove_request(&shub_qos_cpu_dma_latency);
    return;
}

void shub_qos_start(void)
{

    mutex_lock(&qosMutex);
    if (!shub_wake_lock_num) {
        pm_qos_update_request(&shub_qos_cpu_dma_latency, SHUB_PM_QOS_LATENCY_VALUE);
    }
    shub_wake_lock_num++;
    mutex_unlock(&qosMutex);

    return;
}

void shub_qos_end(void)
{

    mutex_lock(&qosMutex);
    shub_wake_lock_num--;
    if (!shub_wake_lock_num) {
        pm_qos_update_request(&shub_qos_cpu_dma_latency, PM_QOS_DEFAULT_VALUE);
    }
    if (shub_wake_lock_num < 0) {
        shub_wake_lock_num = 0;
    }
    mutex_unlock(&qosMutex);

    return;
}

static uint64_t shub_dbg_cnt_irq;
static uint64_t shub_dbg_cnt_cmd;
static uint64_t shub_dbg_cnt_acc;
static uint64_t shub_dbg_cnt_baro;
static uint64_t shub_dbg_cnt_mag;
static uint64_t shub_dbg_cnt_gyro;
static uint64_t shub_dbg_cnt_fusion;
static uint64_t shub_dbg_cnt_cust;
static uint64_t shub_dbg_cnt_other;

static void shub_dbg_collect_irq_log(uint16_t intreq)
{
    if(intreq & INTREQ_HOST_CMD) {
        shub_dbg_cnt_cmd++;
    }
    if(intreq & INTREQ_ACC) {
        shub_dbg_cnt_acc++;
    }
//#ifdef CONFIG_BARO_SENSOR
    if(intreq & INTREQ_BARO) {
        shub_dbg_cnt_baro++;
    }
//#endif
    if(intreq & INTREQ_GYRO) {
        shub_dbg_cnt_gyro++;
    }
    if(intreq & INTREQ_MAG) {
        shub_dbg_cnt_mag++;
    }
    if(intreq & INTREQ_CUSTOMER) {
        shub_dbg_cnt_cust++;
    }
    if(intreq & INTREQ_FUSION) {
        shub_dbg_cnt_fusion++;
    }
    if(intreq & ~INTREQ_MASK) {
        shub_dbg_cnt_other++;
    }
}

static void shub_dbg_clr_irq_log(void)
{
    shub_dbg_cnt_irq = 0;
    shub_dbg_cnt_cmd = 0;
    shub_dbg_cnt_acc = 0;
    shub_dbg_cnt_baro = 0;
    shub_dbg_cnt_mag = 0;
    shub_dbg_cnt_gyro = 0;
    shub_dbg_cnt_fusion = 0;
    shub_dbg_cnt_cust = 0;
    shub_dbg_cnt_other = 0;
}

static void shub_dbg_out_irq_log(void)
{

//#ifdef CONFIG_BARO_SENSOR
    printk("[shub][dbg] irq=%lld,cmd=%lld,a=%lld,b=%lld,m=%lld,g=%lld,f=%lld,c=%lld,o=%lld,en=%08x(%08x)\n",
            shub_dbg_cnt_irq, shub_dbg_cnt_cmd, shub_dbg_cnt_acc, shub_dbg_cnt_baro, shub_dbg_cnt_mag,
//#else
//    printk("[shub][dbg] irq=%lld,cmd=%lld,a=%lld,m=%lld,g=%lld,f=%lld,c=%lld,o=%lld,en=%08x(%08x)\n",
//            shub_dbg_cnt_irq, shub_dbg_cnt_cmd, shub_dbg_cnt_acc, shub_dbg_cnt_mag,
//#endif
            shub_dbg_cnt_gyro, shub_dbg_cnt_fusion, shub_dbg_cnt_cust, shub_dbg_cnt_other,
            atomic_read(&g_CurrentSensorEnable), atomic_read(&g_CurrentLoggingSensorEnable));
}

static int32_t shub_check_access(void);
static int32_t shub_boot_fw_check(void);
static int32_t shub_user_fw_check(void);

bool shub_fw_update_check(void)
{
    bool ret = true;
    if(shub_fw_write_flg == false){
        ret = false;
    }
    return ret;
}

bool shub_connect_check(void)
{
    bool ret = true;
    if(shub_connect_flg == false){
        ret = false;
    }
    return ret;
}

#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_spi_log = 0;
module_param(shub_spi_log, int, 0600);

void shub_wbuf_printk( uint8_t adr, const uint8_t *data, uint16_t size )
{
    int i, t;
    int log_size = size;
    const uint8_t *p = data;
    
    printk("write addr=0x%x, size=%d\n", adr, size);
//  if(log_size > 16){
//      log_size = 16;
//  }
    for(i=0; i<log_size; i+=8){
        t = log_size - i;
        if(t > 8){
            t = 8;
        }
        if(t == 1){
            printk("----> wBuf[%02d   ] : %02x\n", i,*p);
        }else if(t == 2){
            printk("----> wBuf[%02d-%02d] : %02x,%02x\n", i,i+t-1,*p,*(p+1));
        }else if(t == 3){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2));
        }else if(t == 4){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3));
        }else if(t == 5){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4));
        }else if(t == 6){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5));
        }else if(t == 7){
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6));
        }else{
            printk("----> wBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7));
        }
        p += t;
    }
}

void shub_rbuf_printk( uint8_t adr, uint8_t *data, uint16_t size )
{
    int i, t;
    int log_size = size;
    uint8_t *p = data;
    
    printk("Read  addr=0x%x, size=%d\n", adr, size);
//  if(log_size > 16){
//      log_size = 16;
//  }
    for(i=0; i<log_size; i+=8){
        t = log_size - i;
        if(t > 8){
            t = 8;
        }
        if(t == 1){
            printk("----> rBuf[%02d   ] : %02x\n", i,*p);
        }else if(t == 2){
            printk("----> rBuf[%02d-%02d] : %02x,%02x\n", i,i+t-1,*p,*(p+1));
        }else if(t == 3){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2));
        }else if(t == 4){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3));
        }else if(t == 5){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4));
        }else if(t == 6){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5));
        }else if(t == 7){
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6));
        }else{
            printk("----> rBuf[%02d-%02d] : %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", i,i+t-1,*p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7));
        }
        p += t;
    }
}

#define SHUB_DBG_SPIW(adr, data, size) {    \
    if(shub_spi_log)                        \
        shub_wbuf_printk(adr, data, size);  \
}

#define SHUB_DBG_SPIR(adr, data, size) {    \
    if(shub_spi_log)                        \
        shub_rbuf_printk(adr, data, size);  \
}
#else
#define SHUB_DBG_SPIW(adr, data, size)
#define SHUB_DBG_SPIR(adr, data, size)
#endif

int32_t shub_adjust_value(int32_t min,int32_t max,int32_t value)
{
    if(min > value) return min;
    if(max < value) return max;
    return value;
}

static struct timespec shub_get_timestamp(void)
{
    struct timespec ts;
#ifndef NO_LINUX
    ktime_get_ts(&ts);
    monotonic_to_bootbased(&ts);
#endif
    return ts;
}

static int32_t hostif_write(uint8_t adr, const uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

    SHUB_DBG_SPIW(adr, data, size);

    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_write_proc(adr, data, size);
        if(ret == 0){
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "write EBUSY error(Retry:%d)\n", i);
            usleep(100 * 1000);

        }else{
            DBG(DBG_LV_ERROR, "write Other error (H/W Reset ON) \n");
            break;
        }
    }

    return ret;
}

static int32_t hostif_read(uint8_t adr, uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_read_proc(adr, data, size);
        if(ret == 0){
            SHUB_DBG_SPIR(adr, data, size);
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "read EBUSY error(Retry:%d)\n", i);
            usleep(100 * 1000);

        }else{
            DBG(DBG_LV_ERROR, "read Other error (H/W Reset ON) \n");
            break;
        }
    }

    return ret;
}

#ifndef NO_LINUX

#if 1
static void pending_base_time(int32_t arg_iSensType, ktime_t *time)
{
    ktime_t pending_baseTime;
    if(time == NULL){
        pending_baseTime = timespec_to_ktime(shub_get_timestamp());
    }else{
        pending_baseTime = *time;
    }

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        s_pending_baseTime.acc = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_GYRO){
        s_pending_baseTime.gyro = pending_baseTime;
        DBG(DBG_LV_TIME, "[time] s_pending_baseTime( gyro=%lld )\n", s_pending_baseTime.gyro.tv64);
    }
    if(arg_iSensType & SHUB_ACTIVE_MAG){
        s_pending_baseTime.mag = pending_baseTime;
    }
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_pending_baseTime.baro = pending_baseTime;
    }
#endif
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_pending_baseTime.orien = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_pending_baseTime.grav = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_pending_baseTime.linear = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV){
        s_pending_baseTime.rot = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_pending_baseTime.rot_gyro = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_pending_baseTime.rot_mag = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        s_pending_baseTime.pedocnt = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
        s_pending_baseTime.pedocnt2 = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
        s_pending_baseTime.total_status2 = pending_baseTime;
    }
}
#else
static void pending_base_time(int32_t arg_iSensType)
{
    ktime_t pending_baseTime;
    pending_baseTime = timespec_to_ktime(shub_get_timestamp());

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        s_pending_baseTime.acc = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_GYRO){
        s_pending_baseTime.gyro = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_MAG){
        s_pending_baseTime.mag = pending_baseTime;
    }
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_pending_baseTime.baro = pending_baseTime;
    }
#endif
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_pending_baseTime.orien = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_pending_baseTime.grav = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_pending_baseTime.linear = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV){
        s_pending_baseTime.rot = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_pending_baseTime.rot_gyro = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_pending_baseTime.rot_mag = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        s_pending_baseTime.pedocnt = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
        s_pending_baseTime.pedocnt2 = pending_baseTime;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
        s_pending_baseTime.total_status2 = pending_baseTime;
    }
}
#endif

static int32_t shub_compare_gyroEnable_time(void)
{
    int ret;
    
    if(shub_gyro_entime_flg == 0){
        return -1;
    }
    ret = ktime_compare( s_beseTime.gyro, s_gyro_enable_time_s);
    if(ret < 0){
        return -1;
    }
    ret = ktime_compare( s_beseTime.gyro, s_gyro_enable_time_e);
    if(ret > 0){
        shub_gyro_entime_flg = 0;
        DBG(DBG_LV_TIME, "[time] %s : clear time = %lld\n", __func__, s_gyro_enable_time_s.tv64);
        return -1;
    }
    return 1;
}

static void shub_set_gyroEnable_time(ktime_t *time)
{
    shub_gyro_entime_flg = 1;
    s_gyro_enable_time_s = *time;
    s_gyro_enable_time_e = ktime_add_us(s_gyro_enable_time_s, 130 * 1000);
    DBG(DBG_LV_TIME, "[time] %s : %lld --> %lld\n", __func__, s_gyro_enable_time_s.tv64, s_gyro_enable_time_e.tv64);
}

static int32_t update_base_time(int32_t arg_iSensType, ktime_t *time )
{
    if(time == NULL){
        if(arg_iSensType & SHUB_ACTIVE_ACC){
            s_beseTime.acc = s_pending_baseTime.acc;
        }
        if(arg_iSensType & SHUB_ACTIVE_GYRO){
            s_beseTime.gyro = s_pending_baseTime.gyro;
            DBG(DBG_LV_TIME, "[time] s_beseTime null( gyro=%lld )\n", s_beseTime.gyro.tv64);
        }
        if(arg_iSensType & SHUB_ACTIVE_MAG){
            s_beseTime.mag = s_pending_baseTime.mag;
        }
#ifdef CONFIG_BARO_SENSOR
        if(arg_iSensType & SHUB_ACTIVE_BARO){
            s_beseTime.baro = s_pending_baseTime.baro;
        }
#endif
        if(arg_iSensType & SHUB_ACTIVE_ORI){
            s_beseTime.orien = s_pending_baseTime.orien;
        }
        if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
            s_beseTime.grav = s_pending_baseTime.grav;
        }
        if(arg_iSensType & SHUB_ACTIVE_LACC){
            s_beseTime.linear = s_pending_baseTime.linear;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV){
            s_beseTime.rot = s_pending_baseTime.rot;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
            s_beseTime.rot_gyro = s_pending_baseTime.rot_gyro;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
            s_beseTime.rot_mag = s_pending_baseTime.rot_mag;
        }
        if(arg_iSensType & SHUB_ACTIVE_PEDOM){
            s_beseTime.pedocnt = s_pending_baseTime.pedocnt;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
            s_beseTime.pedocnt2 = s_pending_baseTime.pedocnt2;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
            s_beseTime.total_status2 = s_pending_baseTime.total_status2;
        }
    }else{
        if(arg_iSensType & SHUB_ACTIVE_ACC){
            s_beseTime.acc = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_GYRO){
            s_beseTime.gyro = *time;
            DBG(DBG_LV_TIME, "[time] s_beseTime addr( gyro=%lld )\n", s_beseTime.gyro.tv64);
        }
        if(arg_iSensType & SHUB_ACTIVE_MAG){
            s_beseTime.mag = *time;
        }
#ifdef CONFIG_BARO_SENSOR
        if(arg_iSensType & SHUB_ACTIVE_BARO){
            s_beseTime.baro = *time;
        }
#endif
        if(arg_iSensType & SHUB_ACTIVE_ORI){
            s_beseTime.orien = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
            s_beseTime.grav = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_LACC){
            s_beseTime.linear = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV){
            s_beseTime.rot = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
            s_beseTime.rot_gyro = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
            s_beseTime.rot_mag = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_PEDOM){
            s_beseTime.pedocnt = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
            s_beseTime.pedocnt2 = *time;
        }
        if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
            s_beseTime.total_status2 = *time;
        }
    }
    return SHUB_RC_OK;
}

static uint64_t shub_do_div(uint64_t n, uint64_t base)
{
    uint64_t temp = n;
    do_div(temp, base);
    return temp;
}

static int32_t shub_update_time_flash_exe(ktime_t *time)
{
    int ret;
    int32_t result = 0;
    int32_t iCurrentLoggingEnable;
    uint64_t min_delay = MEASURE_MAX_US;
    uint64_t task_delay;
    uint64_t tmp;
    ktime_t oldTime;
    ktime_t newTime;
    ktime_t cngTime;
    ktime_t sub_time;
    
    task_delay = (uint64_t)(s_micon_param.task_cycle[0] * 10);
    iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    
    if(iCurrentLoggingEnable == 0){
        return result;
    }
    
    if(iCurrentLoggingEnable & SHUB_ACTIVE_ACC){
        tmp = s_logging_delay_us.acc;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.acc;
            newTime = s_pending_baseTime.acc;
        }
    }
    if(iCurrentLoggingEnable & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){
        tmp = s_logging_delay_us.gyro;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.gyro;
            newTime = s_pending_baseTime.gyro;
        }
    }
    if(iCurrentLoggingEnable & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){
        tmp = s_logging_delay_us.mag;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.mag;
            newTime = s_pending_baseTime.mag;
        }
    }
#ifdef CONFIG_BARO_SENSOR
    if(iCurrentLoggingEnable & SHUB_ACTIVE_BARO){
        tmp = s_logging_delay_us.baro;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.baro;
            newTime = s_pending_baseTime.baro;
        }
    }
#endif
    if(iCurrentLoggingEnable & SHUB_ACTIVE_ORI){
        tmp = s_logging_delay_us.orien;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.orien;
            newTime = s_pending_baseTime.orien;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_GRAVITY){
        tmp = s_logging_delay_us.grav;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.grav;
            newTime = s_pending_baseTime.grav;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_LACC){
        tmp = s_logging_delay_us.linear;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.linear;
            newTime = s_pending_baseTime.linear;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_RV){
        tmp = s_logging_delay_us.rot;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.rot;
            newTime = s_pending_baseTime.rot;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONMAG){
        tmp = s_logging_delay_us.rot_gyro;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.rot_gyro;
            newTime = s_pending_baseTime.rot_gyro;
        }
    }
    if(iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONGYRO){
        tmp = s_logging_delay_us.rot_mag;
        if(min_delay > tmp){
            min_delay = tmp;
            oldTime = s_beseTime.rot_mag;
            newTime = s_pending_baseTime.rot_mag;
        }
    }
    
    if(task_delay != min_delay){
        return result;
    }
    
    ret = ktime_compare(oldTime, newTime);
    if(ret > 0){
        sub_time = ktime_sub(oldTime, newTime);
    }else{
        sub_time = ktime_sub(newTime, oldTime);
    }
    
    min_delay = (uint64_t)(min_delay * 1000);
    tmp = (uint64_t)(shub_do_div(min_delay, 10) * 8);
    if(sub_time.tv64 < (min_delay + tmp)){
        if(ret > 0){
            cngTime = ktime_add_ns(newTime, shub_do_div(sub_time.tv64, 2));
        }else{
            cngTime = ktime_sub_ns(newTime, shub_do_div(sub_time.tv64, 2));
        }
        *time = cngTime;
        DBG(DBG_LV_TIME, "[time] flash_update : %lld --> %lld( %lld ), d=%lld, sub=%lld\n", oldTime.tv64, cngTime.tv64, newTime.tv64, task_delay, shub_do_div(sub_time.tv64, 1000));
        result = 1;
    }else{
        DBG(DBG_LV_TIME, "[time][over] flash_update : %lld --> %lld, d=%lld, sub=%lld\n", oldTime.tv64, newTime.tv64, task_delay, shub_do_div(sub_time.tv64, 1000));
    }
    return result;
}

static void shub_update_time_flash(int32_t arg_iSensType)
{
    int32_t ret;
    ktime_t newTime;
    
    ret = shub_update_time_flash_exe(&newTime);
    if(ret == 0){
        update_base_time(arg_iSensType, NULL);
    }else{
        update_base_time(arg_iSensType, &newTime);
    }
}

static struct timespec event_time_to_offset(int32_t arg_iSensType,int32_t eventOffsetTime)
{
    ktime_t baseTime = timespec_to_ktime(shub_get_timestamp());
    ktime_t eventTime ;
    struct timespec ts;
    uint64_t sensorTaskCycle_ns;
    uint64_t micom_timer_base_clock_ns;

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        baseTime = s_beseTime.acc;
    }
    if(arg_iSensType & SHUB_ACTIVE_GYRO){
        baseTime =  s_beseTime.gyro;
    }
    if(arg_iSensType & SHUB_ACTIVE_MAG){
        baseTime =  s_beseTime.mag;
    }
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        baseTime = s_beseTime.baro;
    }
#endif
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        baseTime =  s_beseTime.orien;
    }
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        baseTime =  s_beseTime.grav;
    }
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        baseTime =  s_beseTime.linear;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV){
        baseTime =  s_beseTime.rot;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        baseTime = s_beseTime.rot_gyro;
    }
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        baseTime = s_beseTime.rot_mag;
    }
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        baseTime = s_beseTime.pedocnt;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_PEDOM){
        baseTime = s_beseTime.pedocnt2;
    }
    if(arg_iSensType & SHUB_ACTIVE_EXT_TOTAL_STATUS){
        baseTime = s_beseTime.total_status2;
    }

    s_sensor_task_delay_us = (s_sensor_task_delay_us / 10) * 10; 
    sensorTaskCycle_ns = s_sensor_task_delay_us * 1000;
    if(s_sensor_task_delay_us <= 7500){
        micom_timer_base_clock_ns = 30517;
    }else{
        micom_timer_base_clock_ns = 488281;
    }
    do_div(sensorTaskCycle_ns, micom_timer_base_clock_ns);
    sensorTaskCycle_ns*= micom_timer_base_clock_ns;

    eventTime  = ktime_add_ns(baseTime, eventOffsetTime * sensorTaskCycle_ns);
//  if(arg_iSensType & SHUB_ACTIVE_GYRO){
//      DBG(DBG_LV_TIME, "[shub][time] Base=%lld, Cycle=%llu, Cnt=%d\n", baseTime.tv64, sensorTaskCycle_ns, eventOffsetTime);
//  }
    update_base_time(arg_iSensType, &eventTime);
    ts = ns_to_timespec(eventTime.tv64);

    return ts;
}

#endif

#ifdef CONFIG_BARO_SENSOR
static int32_t shub_baro_filter( struct barometric* baroData, uint32_t pressure ) {
    int i, j;
    uint32_t tmp = 0;
    uint32_t tmpBuf[SHUB_BARO_BUFFER_SIZE];

    baroData->buffer[baroData->pos] = pressure;
    if( baroData->num < SHUB_BARO_BUFFER_SIZE ) {
        baroData->num++;
    }
    baroData->pos++;
    if( baroData->pos >= SHUB_BARO_BUFFER_SIZE ) {
        baroData->pos = 0;
    }

    for (i = 0; i < baroData->num; ++i) {
        tmpBuf[i] = baroData->buffer[i];
    }
    for (i = 0; i < baroData->num; ++i) {
        for (j = i+1; j < baroData->num; ++j) {
            if (tmpBuf[i] > tmpBuf[j]) {
                tmp = tmpBuf[i];
                tmpBuf[i] = tmpBuf[j];
                tmpBuf[j] = tmp;
            }
        }
    }
#ifdef CONFIG_ANDROID_ENGINEERING
    if(baro_filter){
        baroData->pressure = tmpBuf[baroData->num/2];
    } else {
        baroData->pressure = pressure;
    }
#else
    baroData->pressure = tmpBuf[baroData->num/2];
#endif
    return SHUB_RC_OK;
}
#endif

static int32_t sensor_get_logging_data(uint8_t* buf, int32_t size)
{
    int32_t    ret;
    HostCmd    cmd;
    HostCmdRes res;
    uint8_t    ucBuff[2];
    int32_t    logsize = size;
    int32_t    readsize;
    int32_t    fifosize;
    uint8_t    *wp;

    wp = buf;
    do{
        if(logsize > FIFO_SIZE){
            readsize = FIFO_SIZE;
        } else {
            readsize = logsize;
        }

        cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
        cmd.prm.u8[0] = 0x00;
        cmd.prm.u8[1] = readsize & 0xFF;
        cmd.prm.u8[2] = (readsize >> 0x08) & 0xFF;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 3);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_GET_RESULT err (%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        ret =  hostif_read(RSLT3E, ucBuff, 2);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "RSLT3E/3F read err\n");
            return SHUB_RC_ERR;
        }
        fifosize =(ucBuff[0] | ucBuff[1] << 0x08);
        //size check
        if(readsize != fifosize){
            DBG(DBG_LV_ERROR, "Get readsize error. [%d != %d] \n",readsize,fifosize);
            return SHUB_RC_ERR;
        }

        ret = hostif_read(FIFO, wp, readsize);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "FIFO read err. remain = 0x%x\n", logsize);
            return SHUB_RC_ERR;
        }
        wp += readsize;
        logsize -= readsize;
    }while(logsize > 0);

    return SHUB_RC_OK;
}


static int32_t logging_flush_exec(void)
{
    int32_t ret = SHUB_RC_OK;
    int32_t size, size_buff, cmdsize;
    uint8_t *buf, *buf_tmp;
    int32_t iCurrentEnable;
    uint8_t op_buf;
    uint8_t data_tmp[12];
    HostCmd    cmd;
    HostCmdRes res;
    TotalOfDeletedTimestamp delTimestamp;
    uint32_t tm_tmp; 
    int32_t data[10];
    struct timespec tstamp;
    uint8_t gyro_ofs_first=1;
    uint8_t mag_ofs_first=1;
    int32_t aRet;

    size = LOGGING_RAM_SIZE;

    iCurrentEnable = atomic_read(&g_CurrentLoggingSensorEnable);

    buf = (uint8_t *)kmalloc(size, GFP_KERNEL );
    if(buf == NULL){
        DBG(DBG_LV_ERROR, "error(kmalloc) : %s\n", __FUNCTION__);
        update_base_time(ACTIVE_FUNC_MASK,NULL);
        return -ENOMEM;
    }else{
        // for "kfree"
        buf_tmp = buf;
    }

    memset(buf, 0xFF, size);

    cmd.cmd.u16 = HC_LOGGING_DELIMITER;
    shub_basetime_req = 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_SKIP_MUTEX_UNLOCK |
            EXE_HOST_RES_ONLY_FIFO_SIZE | 
            EXE_HOST_WAIT |
            EXE_HOST_ERR , 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_DELIMITER err (%x)\n", res.err.u16);
        shub_basetime_req = 0;
        update_base_time(ACTIVE_FUNC_MASK,NULL);
        kfree(buf_tmp);
        mutex_unlock(&s_hostCmdMutex);
        return SHUB_RC_ERR;
    }
//  pending_base_time(ACTIVE_FUNC_MASK);
    pending_base_time(ACTIVE_FUNC_MASK, &shub_baseTime);
    if(res.res_size != 0){
        ret = hostif_read(FIFO, res.res.u8, res.res_size);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_DELIMITER err (%x)\n", res.err.u16);
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            kfree(buf_tmp);
            mutex_unlock(&s_hostCmdMutex);
            return SHUB_RC_ERR;
        }
    }
    mutex_unlock(&s_hostCmdMutex);
    size = res.res.u16[0];

    if(size > DELIMITER_LOGGING_SIZE){
        memcpy(buf, (uint8_t *)&(res.res.u8[DELIMITER_TIMESTAMP_SIZE]), DELIMITER_LOGGING_SIZE);
    }else{
        memcpy(buf, (uint8_t *)&(res.res.u8[DELIMITER_TIMESTAMP_SIZE]), size);
    }
    delTimestamp.acc        = (uint32_t)RESU8_TO_X32(res,2 );
    delTimestamp.mag        = (uint32_t)RESU8_TO_X32(res,6 );
    delTimestamp.gyro       = (uint32_t)RESU8_TO_X32(res,10);
    delTimestamp.orien      = (uint32_t)RESU8_TO_X32(res,14);
    delTimestamp.grav       = (uint32_t)RESU8_TO_X32(res,18);
    delTimestamp.linear     = (uint32_t)RESU8_TO_X32(res,22);
    delTimestamp.rot        = (uint32_t)RESU8_TO_X32(res,26);
    delTimestamp.rot_gyro   = (uint32_t)RESU8_TO_X32(res,30);
    delTimestamp.rot_mag    = (uint32_t)RESU8_TO_X32(res,34);
    delTimestamp.pedocnt    = (uint32_t)RESU8_TO_X32(res,38);
    delTimestamp.total_status = (uint32_t)RESU8_TO_X32(res,42);
    delTimestamp.pedocnt2   = (uint32_t)RESU8_TO_X32(res,46);
    delTimestamp.total_status2= (uint32_t)RESU8_TO_X32(res,50);

#ifdef CONFIG_BARO_SENSOR
    delTimestamp.baro       = (uint32_t)RESU8_TO_X32(res,54);
#endif

    if(size > DELIMITER_LOGGING_SIZE){
        ret = sensor_get_logging_data(&buf[DELIMITER_LOGGING_SIZE], size-DELIMITER_LOGGING_SIZE);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "sensor_get_logging_data err!!\n" );
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            kfree(buf_tmp);
            return SHUB_RC_ERR;
        }
    }

    size_buff = size;
    DBG(DBG_LV_TIME, "%s : size_buff=%d\n", __func__, size_buff);

    while (size_buff)
    {
        memset(data_tmp, 0, sizeof(data_tmp));
        cmdsize = 0xFF;
        op_buf = *buf;
        buf++;
        size_buff--;

        /* Length Check */
        switch(op_buf)
        {
            case DATA_OPECORD_ACC:
                cmdsize=DATA_SIZE_ACC;
                break;
            case DATA_OPECORD_MAG:
                cmdsize=DATA_SIZE_MAG;
                break;
            case DATA_OPECORD_GYRO:
                cmdsize=DATA_SIZE_GYRO;
                break;
            case DATA_OPECORD_MAG_CAL_OFFSET:
                cmdsize=DATA_SIZE_MAG_CAL_OFFSET;
                break;
            case DATA_OPECORD_GYRO_CAL_OFFSET:
                cmdsize=DATA_SIZE_GYRO_CAL_OFFSET;
                break;
            case DATA_OPECORD_ORI:
                cmdsize=DATA_SIZE_ORI;
                break;
            case DATA_OPECORD_GRAVITY:
                cmdsize=DATA_SIZE_GRAVITY;
                break;
            case DATA_OPECORD_LINEARACC:
                cmdsize=DATA_SIZE_LINEARACC;
                break;
            case DATA_OPECORD_RVECT:
                cmdsize=DATA_SIZE_RVECT;
                break;
            case DATA_OPECORD_GAMERV:
                cmdsize=DATA_SIZE_GAMERV;
                break;
            case DATA_OPECORD_GEORV:
                cmdsize=DATA_SIZE_GEORV;
                break;
            case DATA_OPECORD_PEDOCNT:
                cmdsize=DATA_SIZE_PEDOCNT;
                break;
            case DATA_OPECORD_TOTAL_STATUS:
                cmdsize=DATA_SIZE_TOTALSTATUS;
                break;
            case DATA_OPECORD_PEDOCNT2:
                cmdsize=DATA_SIZE_PEDOCNT;
                break;
            case DATA_OPECORD_TOTAL_STATUS2:
                cmdsize=DATA_SIZE_TOTALSTATUS;
                break;
#ifdef CONFIG_BARO_SENSOR
            case DATA_OPECORD_BARO:
                cmdsize=DATA_SIZE_BARO;
                break;
#endif
            default:
                DBG(DBG_LV_ERROR, "logging data type non [0x%02X]\n", op_buf);
                cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
                cmd.prm.u16[0] = 1;
                shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);

                kfree(buf_tmp);
                return SHUB_RC_ERR;
                break;
        }

        if (cmdsize == 0xff)
        {
            DBG(DBG_LV_ERROR, "logging format error [%x]", op_buf);
            cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
            cmd.prm.u16[0] = 1;
            shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);

            kfree(buf_tmp);
            return SHUB_RC_ERR;
        }

        //check buffer over flow
        if (size_buff < cmdsize)
        {
            /* data non */
            DBG(DBG_LV_ERROR, "buffer over flow");
            kfree(buf_tmp);
            update_base_time(ACTIVE_FUNC_MASK,NULL);
            return SHUB_RC_ERR;
        }

        switch(op_buf)
        {
            case DATA_OPECORD_ACC:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_ACC);
                    buf+=DATA_SIZE_ACC;
                    size_buff-=DATA_SIZE_ACC;
                    tm_tmp = data_tmp[DATA_SIZE_ACC - 7] + delTimestamp.acc;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_ACC, tm_tmp);
                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ACC - 6]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ACC - 4]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ACC - 2]);
                    data[3] = tstamp.tv_sec;
                    data[4] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_ACC){
                        shub_input_report_acc(data);
                    }
                }
                break;
            case DATA_OPECORD_MAG:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_MAG);
                    buf+=DATA_SIZE_MAG;
                    size_buff-=DATA_SIZE_MAG;

                    tm_tmp = data_tmp[DATA_SIZE_MAG - 8] + delTimestamp.mag;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_MAG, tm_tmp);
                    if(iCurrentEnable & SHUB_ACTIVE_MAG){
                        data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 7]);
                        data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 5]);
                        data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 3]);
                        data[3] = (int32_t)data_tmp[DATA_SIZE_MAG - 1];
                        data[4] = tstamp.tv_sec;
                        data[5] = tstamp.tv_nsec;
                        shub_input_report_mag(data);
                    }

                    if(iCurrentEnable & SHUB_ACTIVE_MAGUNC){
                        data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 7]) + s_tLatestMagData.nXOffset;
                        data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 5]) + s_tLatestMagData.nYOffset;
                        data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG - 3]) + s_tLatestMagData.nZOffset;
                        data[3] = s_tLatestMagData.nXOffset;
                        data[4] = s_tLatestMagData.nYOffset;
                        data[5] = s_tLatestMagData.nZOffset;
                        data[6] = (int32_t)data_tmp[DATA_SIZE_MAG - 1];
                        data[7] = tstamp.tv_sec;
                        data[8] = tstamp.tv_nsec;
                        shub_input_report_mag_uncal(data);
                    }
                }
                break;
            case DATA_OPECORD_GYRO:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GYRO);
                    buf+=DATA_SIZE_GYRO;
                    size_buff-=DATA_SIZE_GYRO;
                    tm_tmp = data_tmp[DATA_SIZE_GYRO - 7] + delTimestamp.gyro;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_GYRO, tm_tmp);
                    aRet = shub_compare_gyroEnable_time();
                    if(aRet > 0){
                        DBG(DBG_LV_INFO, "[time] %s : Sec=%d, nanoSec=%d\n", __func__, (int)tstamp.tv_sec, (int)tstamp.tv_nsec);
                    }else{
                        if(iCurrentEnable & SHUB_ACTIVE_GYRO){
                            data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 6]);
                            data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 4]);
                            data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 2]);
                            data[3] = s_tLatestGyroData.nAccuracy;
                            data[4] = tstamp.tv_sec;
                            data[5] = tstamp.tv_nsec;
                            shub_input_report_gyro(data);
                            //DBG(DBG_LV_INFO, "gyro sec= %d nsec=%d cnt=%d sum=%d\n"
                            //        ,tstamp.tv_sec,tstamp.tv_nsec,tm_tmp,delTimestamp.gyro);
                        }
                        if(iCurrentEnable & SHUB_ACTIVE_GYROUNC){
                            data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 6]) + s_tLatestGyroData.nXOffset;
                            data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 4]) + s_tLatestGyroData.nYOffset;
                            data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO - 2]) + s_tLatestGyroData.nZOffset;
                            data[3] = s_tLatestGyroData.nAccuracy;
                            data[4] = s_tLatestGyroData.nXOffset;
                            data[5] = s_tLatestGyroData.nYOffset;
                            data[6] = s_tLatestGyroData.nZOffset;
                            data[7] = tstamp.tv_sec;
                            data[8] = tstamp.tv_nsec;
                            shub_input_report_gyro_uncal(data);
                        }
                    }
                }
                break;
            case DATA_OPECORD_MAG_CAL_OFFSET:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_MAG_CAL_OFFSET);
                    buf+=DATA_SIZE_MAG_CAL_OFFSET;
                    size_buff-=DATA_SIZE_MAG_CAL_OFFSET;
                    mag_ofs_first=0;
                    s_tLatestMagData.nXOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG_CAL_OFFSET - 6]);
                    s_tLatestMagData.nYOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG_CAL_OFFSET - 4]);
                    s_tLatestMagData.nZOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_MAG_CAL_OFFSET - 2]);
                }
                break;
            case DATA_OPECORD_GYRO_CAL_OFFSET:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GYRO_CAL_OFFSET);
                    buf+=DATA_SIZE_GYRO_CAL_OFFSET;
                    size_buff-=DATA_SIZE_GYRO_CAL_OFFSET;
                    if(gyro_ofs_first == 0){
                        s_tLatestGyroData.nAccuracy = 3;
                    }else{
                        gyro_ofs_first=0;
                    }
                    s_tLatestGyroData.nXOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO_CAL_OFFSET - 6]);
                    s_tLatestGyroData.nYOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO_CAL_OFFSET - 4]);
                    s_tLatestGyroData.nZOffset = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GYRO_CAL_OFFSET - 2]);
                }
                break;
            case DATA_OPECORD_ORI:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_ORI);
                    buf+=DATA_SIZE_ORI;
                    size_buff-=DATA_SIZE_ORI;

                    tm_tmp = data_tmp[DATA_SIZE_ORI - 8] + delTimestamp.orien;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_ORI, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ORI - 7]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ORI - 5]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_ORI - 3]);
                    data[3] = (int32_t)data_tmp[DATA_SIZE_ORI - 1];
                    data[4] = tstamp.tv_sec;
                    data[5] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_ORI){
                        shub_input_report_orien(data);
                    }
                }
                break;
            case DATA_OPECORD_GRAVITY:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GRAVITY);
                    buf+=DATA_SIZE_GRAVITY;
                    size_buff-=DATA_SIZE_GRAVITY;

                    tm_tmp = data_tmp[DATA_SIZE_GRAVITY - 7] + delTimestamp.grav;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_GRAVITY, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GRAVITY - 6]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GRAVITY - 4]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GRAVITY - 2]);
                    data[3] = tstamp.tv_sec;
                    data[4] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_GRAVITY){
                        shub_input_report_grav(data);
                    }
                }
                break;
            case DATA_OPECORD_LINEARACC:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_LINEARACC);
                    buf+=DATA_SIZE_LINEARACC;
                    size_buff-=DATA_SIZE_LINEARACC;

                    tm_tmp = data_tmp[DATA_SIZE_LINEARACC - 7] + delTimestamp.linear;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_LACC, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_LINEARACC - 6]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_LINEARACC - 4]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_LINEARACC - 2]);
                    data[3] = tstamp.tv_sec;
                    data[4] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_LACC){
                        shub_input_report_linear(data);
                    }
                }
                break;
            case DATA_OPECORD_RVECT:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_RVECT);
                    buf+=DATA_SIZE_RVECT;
                    size_buff-=DATA_SIZE_RVECT;

                    tm_tmp = data_tmp[DATA_SIZE_RVECT - 9] + delTimestamp.rot;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_RV, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 8]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 6]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 4]);
                    data[3] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_RVECT - 2]);
                    data[4] = -1; 
                    data[5] = tstamp.tv_sec;
                    data[6] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_RV){
                        shub_input_report_rot(data);
                    }
                }
                break;
            case DATA_OPECORD_GAMERV:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GAMERV);
                    buf+=DATA_SIZE_GAMERV;
                    size_buff-=DATA_SIZE_GAMERV;

                    tm_tmp = data_tmp[DATA_SIZE_GAMERV - 9] + delTimestamp.rot_gyro;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_RV_NONMAG, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 8]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 6]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 4]);
                    data[3] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GAMERV - 2]);
                    data[4] = tstamp.tv_sec;
                    data[5] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_RV_NONMAG){
                        shub_input_report_rot_gyro(data);
                    }
                }
                break;
            case DATA_OPECORD_GEORV:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_GEORV);
                    buf+=DATA_SIZE_GEORV;
                    size_buff-=DATA_SIZE_GEORV;

                    tm_tmp = data_tmp[DATA_SIZE_GEORV - 10] + delTimestamp.rot_mag;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_RV_NONGYRO, tm_tmp);

                    data[0] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 9]);
                    data[1] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 7]);
                    data[2] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 5]);
                    data[3] = (int32_t)U8_TO_S16(&data_tmp[DATA_SIZE_GEORV - 3]);
                    data[4] = (int32_t)data_tmp[DATA_SIZE_GEORV - 1];
                    data[5] = tstamp.tv_sec;
                    data[6] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_RV_NONGYRO){
                        shub_input_report_rot_mag(data);
                    }
                }
                break;
            case DATA_OPECORD_PEDOCNT:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_PEDOCNT);
                    buf+=DATA_SIZE_PEDOCNT;
                    size_buff-=DATA_SIZE_PEDOCNT;

                    tm_tmp = (int32_t)(((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 10]) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 9] & 0xFF) << 8)  |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 8] & 0xFF) << 16) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 7] & 0xFF) << 24));
                    tm_tmp += delTimestamp.pedocnt;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_PEDOM, tm_tmp);
                    data[0] = (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 6]) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 5] & 0xFF) << 8)  |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 4] & 0xFF) << 16) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 3] & 0xFF) << 24));
                    data[1] = tstamp.tv_sec;
                    data[2] = tstamp.tv_nsec;
                    if(iCurrentEnable & (SHUB_ACTIVE_PEDOM | SHUB_ACTIVE_PEDOM_NO_NOTIFY)){
                        //DBG(DBG_LV_INFO, "pedom sec= %d nsec=%d cnt=%d sum=%d steps %d\n"
                        //        ,tstamp.tv_sec,tstamp.tv_nsec,tm_tmp, delTimestamp.pedocnt, data[0]);
                        s_tLatestStepCountData.step = (uint64_t)data[0]; 
//                        data[0] -= (int32_t)s_tLatestStepCountData.stepOffset;
                        shub_input_report_stepcnt(data);
                    }
                    if(iCurrentEnable & (SHUB_ACTIVE_PEDODEC | SHUB_ACTIVE_PEDODEC_NO_NOTIFY)){
                        //DBG(DBG_LV_INFO, "pedom sec= %d nsec=%d cnt=%d sum=%d steps %d\n"
                        //        ,tstamp.tv_sec,tstamp.tv_nsec,tm_tmp, delTimestamp.pedocnt, data[0]);
                        shub_input_report_stepdetect(data);
                    }
                    break;
                }
            case DATA_OPECORD_PEDOCNT2:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_PEDOCNT);
                    buf+=DATA_SIZE_PEDOCNT;
                    size_buff-=DATA_SIZE_PEDOCNT;

                    tm_tmp = (int32_t)(((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 10]) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 9] & 0xFF) << 8)  |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 8] & 0xFF) << 16) |
                            (((int32_t)data_tmp[DATA_SIZE_PEDOCNT - 7] & 0xFF) << 24));
                    tm_tmp += delTimestamp.pedocnt2;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_EXT_PEDOM, tm_tmp);
                    data[0] = (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 6]) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 5] & 0xFF) << 8)  |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 4] & 0xFF) << 16) |
                            (((uint32_t)data_tmp[DATA_SIZE_PEDOCNT - 3] & 0xFF) << 24));
                    data[1] = (int32_t)U8_TO_U16(&data_tmp[DATA_SIZE_PEDOCNT - 2]);
                    data[2] = tstamp.tv_sec;
                    data[3] = tstamp.tv_nsec;
                    /*
                       DBG(DBG_LV_ERROR, "Steps = %d, Calorie = %d Sec=%d nanoSec=%d\n",data[0],data[1],data[2],data[3]);
                     */
                    break;
                }
            case DATA_OPECORD_TOTAL_STATUS2 :
                {
                    memcpy(data_tmp , buf, DATA_SIZE_TOTALSTATUS);
                    buf+=DATA_SIZE_TOTALSTATUS;
                    size_buff-=DATA_SIZE_TOTALSTATUS;

                    //timestamp
                    tm_tmp = (int32_t)(((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 9]) |
                            (((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 8] & 0xFF) << 8)  |
                            (((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 7] & 0xFF) << 16) |
                            (((uint32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 6] & 0xFF) << 24));
                    tm_tmp += delTimestamp.total_status2;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_EXT_TOTAL_STATUS, tm_tmp);
                    //total status
                    data[0] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 5];
                    //bycicle rate
                    data[1] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 4];
                    //car rate
                    data[2] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 3];
                    //train rate
                    data[3] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 2];
                    //no ride rate
                    data[4] = (int32_t)data_tmp[DATA_SIZE_TOTALSTATUS - 1];
                    //timestamp [sec]
                    data[5] = tstamp.tv_sec;
                    //timestamp [nsec]
                    data[6] = tstamp.tv_nsec;
                    /*
                       DBG(DBG_LV_ERROR, "Status  = %x \n",data[0]);
                       DBG(DBG_LV_ERROR, "Bycicle = %x \n",data[1]);
                       DBG(DBG_LV_ERROR, "Car     = %x \n",data[2]);
                       DBG(DBG_LV_ERROR, "Train   = %x \n",data[3]);
                       DBG(DBG_LV_ERROR, "No Ride = %x \n",data[4]);
                       DBG(DBG_LV_ERROR, "Sec     = %d \n",data[5]);
                       DBG(DBG_LV_ERROR, "nanoSec = %d \n",data[6]);
                     */
                    break;
                }
#ifdef CONFIG_BARO_SENSOR
            case DATA_OPECORD_BARO:
                {
                    memcpy(data_tmp , buf, DATA_SIZE_BARO);
                    buf+=DATA_SIZE_BARO;
                    size_buff-=DATA_SIZE_BARO;
                    tm_tmp = data_tmp[DATA_SIZE_BARO - 4] + delTimestamp.baro;
                    tstamp = event_time_to_offset(SHUB_ACTIVE_BARO, tm_tmp);
                    //data[0] = U8_TO_U24(&data_tmp[DATA_SIZE_BARO - 3]);
                    shub_baro_filter( &s_tLoggingBaroData, U8_TO_U24(&data_tmp[DATA_SIZE_BARO - 3]) );
                    data[0] = s_tLoggingBaroData.pressure;
                    data[1] = tstamp.tv_sec;
                    data[2] = tstamp.tv_nsec;

                    if(iCurrentEnable & SHUB_ACTIVE_BARO){
                        shub_input_report_baro(data);
                    }
                }
                break;
#endif
            default:
                DBG(DBG_LV_ERROR, "logging data type non [[0x%02X]]\n", op_buf);
                break;
        }
        delTimestamp.acc      = 0;
        delTimestamp.mag      = 0;
        delTimestamp.gyro     = 0;
        delTimestamp.orien    = 0;
        delTimestamp.grav     = 0;
        delTimestamp.linear   = 0;
        delTimestamp.rot      = 0;
        delTimestamp.rot_gyro = 0;
        delTimestamp.rot_mag  = 0;
        delTimestamp.pedocnt  = 0;
        delTimestamp.pedocnt2 = 0;
        delTimestamp.total_status2  = 0;
        delTimestamp.total_status   = 0;
#ifdef CONFIG_BARO_SENSOR
        delTimestamp.baro     = 0;
#endif
    }

    if(mag_ofs_first == 0){
        cmd.cmd.u16 = HC_MAG_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_MAG_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestMagData.nXOffset = 0;
            s_tLatestMagData.nYOffset = 0;
            s_tLatestMagData.nZOffset = 0;
        }else{
            s_tLatestMagData.nXOffset = res.res.s16[0];
            s_tLatestMagData.nYOffset = res.res.s16[1];
            s_tLatestMagData.nZOffset = res.res.s16[2];
        }
    }

    if(gyro_ofs_first == 0){
        cmd.cmd.u16 = HC_GYRO_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GYRO_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestGyroData.nXOffset = 0;
            s_tLatestGyroData.nYOffset = 0;
            s_tLatestGyroData.nZOffset = 0;
        }else{
            s_tLatestGyroData.nXOffset = res.res.s16[0];
            s_tLatestGyroData.nYOffset = res.res.s16[1];
            s_tLatestGyroData.nZOffset = res.res.s16[2];
            s_tLatestGyroData.nAccuracy = 3;
        }
    }
    kfree(buf_tmp);
    
    if(size == 0){
        update_base_time(ACTIVE_FUNC_MASK,NULL);
    }else{
        shub_update_time_flash(ACTIVE_FUNC_MASK);
    }
    
    return ret; 
}

static int32_t shub_read_sensor_data(int32_t arg_iSensType)
{
    int32_t  ret = SHUB_RC_OK;
    HostCmd cmd;
    HostCmdRes res;
    uint8_t data_tmp[16];
    int32_t X,Y,Z,S,ACC = 0;
    int32_t param[] = {0,0};

    mutex_lock(&s_tDataMutex);

    /*get ACC sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_ACC){

        ret = hostif_read(RSLT00, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestAccData.nX = X;
        s_tLatestAccData.nY = Y;
        s_tLatestAccData.nZ = Z;

        if(DBG_LV_PEDO & dbg_level){
            cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
            cmd.prm.u32[0x00] = 0x0000201;
            cmd.prm.u32[0x01] = 0x0000000;
            cmd.prm.u8[0x08] = 0x00;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
            param[0] =(int32_t)(uint32_t)RESU8_TO_X32(res,0);
            param[1] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        }

        DBG(DBG_LV_DATA, "get ACC data X[%d] Y[%d] Z[%d] stab[%d] instab[%d]\n",X, Y, Z, param[0], param[1]);
    }

    /*get GYRO sensor data*/
    if(arg_iSensType & (SHUB_ACTIVE_GYRO | SHUB_ACTIVE_GYROUNC)){

        ret = hostif_read(RSLT06, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestGyroData.nX = X;
        s_tLatestGyroData.nY = Y;
        s_tLatestGyroData.nZ = Z;
        DBG(DBG_LV_DATA, "get GYRO data X[%d] Y[%d] Z[%d] ACC[%d]\n", X, Y, Z, s_tLatestGyroData.nAccuracy);
    }

    /*get MAG sensor data*/
    if(arg_iSensType & (SHUB_ACTIVE_MAG | SHUB_ACTIVE_MAGUNC)){
        ret = hostif_read(RSLT0C, data_tmp, 8);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);
        ACC = data_tmp[6];

        s_tLatestMagData.nAccuracy = ACC;
        s_tLatestMagData.nX = X;
        s_tLatestMagData.nY = Y;
        s_tLatestMagData.nZ = Z;

        DBG(DBG_LV_DATA, "get MAG data X[%d] Y[%d] Z[%d] ACC[%d]\n", X, Y, Z, ACC);
    }

    /*get ORIENTATION sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_ORI){
        ret = hostif_read(RSLT13, data_tmp, 7);
        ACC = data_tmp[0];
        X = (int32_t)(int16_t)((uint32_t)data_tmp[1] | ((uint32_t)data_tmp[2] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[3] | ((uint32_t)data_tmp[4] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[5] | ((uint32_t)data_tmp[6] & 0xFF) << 8);

        s_tLatestOriData.pitch = X;
        s_tLatestOriData.roll = Y;
        s_tLatestOriData.yaw = Z;
        s_tLatestOriData.nAccuracy = ACC;
        DBG(DBG_LV_DATA, "get ORIENTATION data pitch[%d] roll[%d] yaw[%d] ACC[%d]\n", X, Y, Z, ACC);
    }

    /*get GRAVITY sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        ret = hostif_read(RSLT1A, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestGravityData.nX = X;
        s_tLatestGravityData.nY = Y;
        s_tLatestGravityData.nZ = Z;

        DBG(DBG_LV_DATA, "get GRAVITY data X[%d] Y[%d] Z[%d]\n", X, Y, Z);
    }

    /*get LINEACC sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_LACC){
        ret = hostif_read(RSLT20, data_tmp, 6);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);

        s_tLatestLinearAccData.nX = X;
        s_tLatestLinearAccData.nY = Y;
        s_tLatestLinearAccData.nZ = Z;

        DBG(DBG_LV_DATA, "get LINEACC data X[%d] Y[%d] Z[%d]\n", X, Y, Z);
    }

    /*get ROTATION_VECTOR sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_RV){
        ret = hostif_read(RSLT26, data_tmp, 7);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);
        ACC = (int32_t)(int8_t)data_tmp[6];

        s_tLatestRVectData.nX = X;
        s_tLatestRVectData.nY = Y;
        s_tLatestRVectData.nZ = Z;
        s_tLatestRVectData.nS = 0;
        s_tLatestRVectData.nAccuracy = ACC;

        DBG(DBG_LV_DATA, "get ROTATION_VECTOR data X[%d] Y[%d] Z[%d] ACC[%d]\n", X, Y, Z, ACC);
    }

    /*get MAGROT sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        ret = hostif_read(RSLT2D, data_tmp, 9);

        ACC = data_tmp[0];
        X = (int32_t)(int16_t)((uint32_t)data_tmp[1] | ((uint32_t)data_tmp[2] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[3] | ((uint32_t)data_tmp[4] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[5] | ((uint32_t)data_tmp[6] & 0xFF) << 8);
        S = (int32_t)(int16_t)((uint32_t)data_tmp[7] | ((uint32_t)data_tmp[8] & 0xFF) << 8);

        s_tLatestGeoRVData.nX = X;
        s_tLatestGeoRVData.nY = Y;
        s_tLatestGeoRVData.nZ = Z;
        s_tLatestGeoRVData.nS = S;
        s_tLatestGeoRVData.nAccuracy = ACC; 

        DBG(DBG_LV_DATA, "get MAGROT data X[%d] Y[%d] Z[%d] S[%d] ACC[%d]\n", X, Y, Z, S, ACC);
    }

    /*get GAMEROT sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        ret = hostif_read(RSLT36, data_tmp, 8);
        X = (int32_t)(int16_t)((uint32_t)data_tmp[0] | ((uint32_t)data_tmp[1] & 0xFF) << 8);
        Y = (int32_t)(int16_t)((uint32_t)data_tmp[2] | ((uint32_t)data_tmp[3] & 0xFF) << 8);
        Z = (int32_t)(int16_t)((uint32_t)data_tmp[4] | ((uint32_t)data_tmp[5] & 0xFF) << 8);
        S = (int32_t)(int16_t)((uint32_t)data_tmp[6] | ((uint32_t)data_tmp[7] & 0xFF) << 8);

        s_tLatestGameRVData.nX = X;
        s_tLatestGameRVData.nY = Y;
        s_tLatestGameRVData.nZ = Z;
        s_tLatestGameRVData.nS = S;

        DBG(DBG_LV_DATA, "get GAMEROT data X[%d] Y[%d] Z[%d] S[%d]\n", X, Y, Z, S);
    }

    /*get PEDO sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_PEDOM){
        /*get data*/
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0] = 1;
        cmd.prm.u32[1] = 0;
        cmd.prm.u8[0x08] = 0x00;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
        }else{
            s_tLatestStepCountData.step = (uint64_t)res.res.u32[0];
            DBG(DBG_LV_DATA, "get PEDO data step[%lld]\n", s_tLatestStepCountData.step);
        }
    }

#ifdef CONFIG_BARO_SENSOR
    /*get Barometric pressure sensor data*/
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        /*get data*/
        cmd.cmd.u16 = HC_SENSOR_GET_RESULT;
        cmd.prm.u16[0] = HC_BARO_VALID;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_GET_RESULT err(%x)\n", res.err.u16);
        }else{
            // s_tLatestBaroData.pressure = res.res.u32[0];
            // /*DBG(DBG_LV_DATA, "get BARO data[%d]\n", s_tLatestBaroData.pressure);*/
            // DBG(DBG_LV_DATA, "get BARO data baro[%d], temp[%d]\n", s_tLatestBaroData.pressure, RESU8_TO_X16(res,4));
            shub_baro_filter( &s_tLatestBaroData, res.res.u32[0] );
            DBG(DBG_LV_DATA, "get BARO data baro[%d], filter[%d], temp[%d]\n", res.res.u32[0], s_tLatestBaroData.pressure, RESU8_TO_X16(res,4));
        }
    }
#endif

    mutex_unlock(&s_tDataMutex);

    return ret;
}

#ifndef SIM_HOST
//static int32_t shub_hostcmd(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size)
static int32_t shub_hostcmd_exe(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size)
{
    int32_t ret;
    uint8_t reg[20];
    uint8_t i;

    g_hostcmdErr=0;
#ifdef NO_HOST
    res->err.u16= 0;
    return 0;
#endif

    mutex_lock(&s_hostCmdMutex);
    res->res_size=0;
    memset(reg, 0, sizeof(reg));
    if(size > sizeof(prm->prm.u8)){
        size = sizeof(prm->prm.u8);
    }

    reg[0] = prm->cmd.u8[0];
    reg[1] = prm->cmd.u8[1];
    for(i = 0;i < size;i++){
        reg[2 + sizeof(prm->prm.u8) - i - 1] = prm->prm.u8[i];
    }
    reg[19] = 1;

    spin_lock( &s_intreqData );
    g_nIntIrqFlg = 0;
    spin_unlock( &s_intreqData );

    ret =  hostif_write(CMD0, reg, 2);
    ret |= hostif_write(PRM0, &reg[2], 0x10);
    ret |= hostif_write(CMDENTRY, &reg[19], 1);
    if(ret != SHUB_RC_OK){
        DBG(DBG_LV_ERROR, "HostCmd error(ret %x)", ret);
        goto ERROR;
    }

    if((mode & EXE_HOST_WAIT) == EXE_HOST_WAIT){
        if((mode & EXE_HOST_CK_CONNECT) == EXE_HOST_CK_CONNECT){
            ret = shub_waitcmd(SHUB_SEND_CMD_CONNECT);
        }else{
            ret = shub_waitcmd(SHUB_SEND_CMD_HOST);
        }
        if(ret != SHUB_RC_OK) {
            if(((mode & EXE_HOST_EX_NO_RECOVER) == EXE_HOST_EX_NO_RECOVER) && 
                    (ret == SHUB_RC_ERR_TIMEOUT)){
                goto ERROR;
            }
            DBG(DBG_LV_ERROR, "hostcmd timout error\n");
            ERR_WAKEUP;
            goto ERROR;
        }
    }

    if((mode & EXE_HOST_RES_ONLY_FIFO_SIZE) == EXE_HOST_RES_ONLY_FIFO_SIZE){
        uint8_t tmp[2];
        ret = hostif_read(RSLT3E, tmp, 2);
        if(ret != SHUB_RC_OK) {
            goto ERROR;
        }
        res->res_size = (tmp[0] | (tmp[1] << 0x08));
        if(res->res_size > sizeof(res->res.u8)){
            res->res_size = sizeof(res->res.u8);
        }
    }

    if((mode & EXE_HOST_RES) == EXE_HOST_RES){
        uint8_t tmp[2];
        ret = hostif_read(RSLT3E, tmp, 2);
        if(ret != SHUB_RC_OK) {
            goto ERROR;
        }
        res->res_size = (tmp[0] | (tmp[1] << 0x08));
        if(res->res_size > sizeof(res->res.u8)){
            res->res_size = sizeof(res->res.u8);
        }
        if(res->res_size != 0){
            ret = hostif_read(FIFO, res->res.u8, res->res_size);
            if(ret != SHUB_RC_OK) {
                goto ERROR;
            }
        }
    }

    if((mode & EXE_HOST_RES_RSLT) == EXE_HOST_RES_RSLT){
        res->res_size = 64;
        ret = hostif_read(RSLT00, res->res.u8, 64);
        if(ret != SHUB_RC_OK) {
            goto ERROR;
        }
    }

    if((mode & EXE_HOST_ERR) == EXE_HOST_ERR){
        // shub_int_work_func
        res->err.u16= g_hostcmdErr;
    }

ERROR:
    if((mode & EXE_HOST_SKIP_MUTEX_UNLOCK) != EXE_HOST_SKIP_MUTEX_UNLOCK){
        mutex_unlock(&s_hostCmdMutex);
    }
    return ret;
}

static int32_t shub_hostcmd(const HostCmd *prm, HostCmdRes *res, uint8_t mode, uint8_t size)
{
    int32_t ret;
    int32_t i;
    
    DBG(DBG_LV_CMD, "%s : cmd=0x%04x \n", __func__, prm->cmd.u16);
    
    for(i=0; i<SHUB_CMD_RETRY_NUM; i++)
    {
        ret = shub_hostcmd_exe(prm, res, mode, size);
        if(((mode & EXE_HOST_ERR) == EXE_HOST_ERR) && (res->err.u16 != 0)) {
            if(res->err.u16 == 0x0FFE) {
                if(i >= (SHUB_CMD_RETRY_NUM - 1)){
                    DBG(DBG_LV_ERROR, "%s : RetryOver!![%d](ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                        __func__, i, ret, res->err.u16, prm->cmd.u16, mode, size);
                }else{
                    DBG(DBG_LV_ERROR, "%s : Retry[%d](ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                        __func__, i, ret, res->err.u16, prm->cmd.u16, mode, size);
                    usleep(1 * 1000);
                }
            }else{
                DBG(DBG_LV_ERROR, "%s : Error(ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                    __func__, ret, res->err.u16, prm->cmd.u16, mode, size);
                i = SHUB_CMD_RETRY_NUM;
//              break;
            }
        }else if(ret == SHUB_RC_OK) {
            return ret;
            
        }else{
            DBG(DBG_LV_ERROR, "%s : Error(ret=0x%x, err=0x%x, cmd=0x%x, mode=0x%x, size=%d)\n",
                __func__, ret, res->err.u16, prm->cmd.u16, mode, size);
            i = SHUB_CMD_RETRY_NUM;
//          break;
        }
    }
    return ret;
}

#endif

static int32_t shub_waitcmd(uint16_t intBit)
{
    int32_t ret = SHUB_RC_ERR_TIMEOUT;
    int32_t result = 0;
    long timeout;
    int32_t retry = 300;

    if(intBit == SHUB_SEND_CMD_CONNECT){
        timeout = msecs_to_jiffies(WAIT_CHECK_CONNECT);
    }else{
        timeout = msecs_to_jiffies(WAITEVENT_TIMEOUT);
    }

    while(retry){

        result = wait_event_interruptible_timeout(s_tWaitInt, (g_nIntIrqFlg & (INTREQ_HOST_CMD | INTREQ_ERROR)), timeout);
        if( g_nIntIrqFlg & INTREQ_ERROR ){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg &= ~INTREQ_ERROR;
            spin_unlock( &s_intreqData );

            DBG(DBG_LV_INT, "INTREQ0/1 -Error- \n");
            ret = SHUB_RC_ERR;
            break;

        }else if( g_nIntIrqFlg & INTREQ_HOST_CMD ){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg &= ~INTREQ_HOST_CMD;
            spin_unlock( &s_intreqData );

            ret = SHUB_RC_OK;
            DBG(DBG_LV_HOSTIF, "Wakeup Event... \n");
            break;
        }

        if( result == -ERESTARTSYS ) {
            DBG(DBG_LV_HOSTIF, "wait event signal received. retry = %d, g_nIntIrqFlg = %x \n", retry, g_nIntIrqFlg);
            usleep(10 * 1000);
        }

        if( result == 0 ){
            ret = SHUB_RC_ERR_TIMEOUT;
            DBG(DBG_LV_ERROR, "wait event timeout... %x \n", g_nIntIrqFlg);
            break;
        }
        retry--;
    }
    return ret;
}

// PA6 hostif interrupt
static irqreturn_t shub_irq_handler(int32_t irq, void *dev_id)
{
    shub_wake_lock_start(&shub_irq_wake_lock);
    if( irq != g_nIntIrqNo ){
        shub_wake_lock_end(&shub_irq_wake_lock);
        return IRQ_NONE;
    }

    if(shub_basetime_req == 1){
        shub_baseTime = timespec_to_ktime(shub_get_timestamp());
        shub_basetime_req = 0;
    }

    DISABLE_IRQ;
    shub_wake_lock_start(&shub_int_wake_lock);
    if( shub_workqueue_create(accsns_wq_int, shub_int_work_func) != SHUB_RC_OK){
        shub_wake_lock_end(&shub_int_wake_lock);
        ENABLE_IRQ;
    }
    shub_dbg_cnt_irq++;

    shub_wake_lock_end(&shub_irq_wake_lock);
    return IRQ_HANDLED;
}

#ifndef SIM_HOST
// PA6 hostif interrupt substance
static void shub_int_work_func(struct work_struct *work)
{
    uint8_t    cmd_reg_clear[2]={0};
    uint8_t    err_intreq[4];
    uint16_t   intreq;
    int32_t    iCurrentEnable;

    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);

    hostif_read(ERROR0, err_intreq, 4);
    g_hostcmdErr = (uint16_t)((uint16_t)err_intreq[0] | (uint16_t)(err_intreq[1] << 8));

    intreq = (uint16_t)(err_intreq[2] | (err_intreq[3] << 8));

    shub_dbg_collect_irq_log(intreq);

    if(intreq == 0 || 
            (((intreq & ~INTREQ_MASK) != 0) && 
             ( intreq != INTREQ_ERROR))
      ){
        DBG(DBG_LV_INT, "### shub_int_work_func : Error1 %x\n", intreq);
        goto ERROR;
    }

    DBG(DBG_LV_INT, "### INTREQ0/1=%x iCurrentEnable=%x \n", intreq, iCurrentEnable);
    if(intreq == INTREQ_ERROR){
        DBG(DBG_LV_ERROR, "### shub_int_work_func Error %x\n", intreq);
        spin_lock( &s_intreqData );
        g_nIntIrqFlg |= INTREQ_ERROR;
        spin_unlock( &s_intreqData );
        wake_up_interruptible(&s_tWaitInt);
        goto ERROR;
    }

    if(intreq & INTREQ_HOST_CMD){
        //DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_HOST_CMD g_nIntIrqFlg:%x \n", g_nIntIrqFlg);
        if(!(g_nIntIrqFlg & INTREQ_HOST_CMD)){
            spin_lock( &s_intreqData );
            g_nIntIrqFlg |= INTREQ_HOST_CMD;
            spin_unlock( &s_intreqData );
            // DBG(DBG_LV_ERROR, "CMD:%04x\n",s_lsi_id);
            if(s_lsi_id != LSI_ML630Q791){
                hostif_write(CMD0, cmd_reg_clear, sizeof(cmd_reg_clear));
            }
            wake_up_interruptible(&s_tWaitInt);
        }
    }

    if(intreq & INTREQ_ACC){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_ACC iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_acc_wake_lock);
        schedule_work(&acc_irq_work);
    }

    if(intreq & INTREQ_GYRO){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_GYRO iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_gyro_wake_lock);
        schedule_work(&gyro_irq_work);
    }

    if(intreq & INTREQ_MAG){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_MAG iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_mag_wake_lock);
        schedule_work(&mag_irq_work);
    }

    if(intreq & INTREQ_BARO){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_BARO iCurrentEnable:%x \n", iCurrentEnable);
    }

    if(intreq & INTREQ_CUSTOMER){
        DBG(DBG_LV_INT, "### shub_int_work_func INTREQ_CUSTOMER iCurrentEnable:%x \n", iCurrentEnable);
        shub_wake_lock_start(&shub_customer_wake_lock);
        schedule_work(&customer_irq_work);
    }

    if((intreq & INTREQ_FUSION) == INTREQ_FUSION){
        shub_wake_lock_start(&shub_fusion_wake_lock);
        schedule_work(&fusion_irq_work);
    }

ERROR:
    shub_workqueue_delete(work);
    ENABLE_IRQ;
    shub_wake_lock_end(&shub_int_wake_lock);
    return;
}
#endif

static void shub_significant_work_func(struct work_struct *work)
{
    int32_t iCurrentEnable;
    uint8_t notify=0;
    int32_t data[3]={0};
    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);

    if(iCurrentEnable & SHUB_ACTIVE_SIGNIFICANT){
        DBG(DBG_LV_INT, "### SIGNIFICANT\n");
        shub_activate_significant_exec(SHUB_ACTIVE_SIGNIFICANT, 0, &notify);
        shub_get_sensors_data(SHUB_ACTIVE_SIGNIFICANT, data);
        shub_input_report_significant(data);
    }
}

static void shub_int_acc_work_func(struct work_struct *work)
{
    int32_t ret;
    int32_t iCurrentEnable;
    int32_t iCurrentLoggingEnable;
    int32_t iWakeupSensor;
    HostCmd cmd;
    HostCmdRes res;
    int32_t data[3]={0};
    uint32_t intdetail=0;
    uint32_t intdetail2=0;
    uint8_t notify=0;
    int32_t rideInfo[7];
    bool sendInfo;
    uint16_t rdata[2];

    DBG(DBG_LV_INT, "### shub_int_acc_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_ACC >> 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_acc_wake_lock);
        return;
    }
    iCurrentEnable = atomic_read(&g_CurrentSensorEnable);
    iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);

    intdetail = res.res.u8[0] |
        ((res.res.u8[1] << 8 ) & 0x0000ff00) | 
        ((res.res.u8[2] << 16) & 0x00ff0000) | 
        ((res.res.u8[3] << 24) & 0xff000000); 
    intdetail2 = res.res.u8[4] |
        ((res.res.u8[5] << 8 ) & 0x0000ff00);

    DBG(DBG_LV_INT, "### !! Acc Event !! %x %x\n",intdetail, intdetail2);
    iWakeupSensor = atomic_read(&g_WakeupSensor);
    atomic_set(&g_WakeupSensor,iWakeupSensor | SHUB_ACTIVE_PEDOM);

    if((iCurrentEnable & SHUB_ACTIVE_PEDOM) && 
            (iCurrentLoggingEnable & SHUB_ACTIVE_PEDOM) == 0){
        if(intdetail2 & INTDETAIL_PEDOM_CNT){
            shub_get_sensors_data(SHUB_ACTIVE_PEDOM, data);
            shub_input_report_stepcnt(data);
            shub_timer_wake_lock_start();
        }
    }

    if((iCurrentEnable & SHUB_ACTIVE_PEDODEC) && 
            (iCurrentLoggingEnable & SHUB_ACTIVE_PEDODEC) == 0){
        if(intdetail2 & INTDETAIL_PEDOM_CNT){
            shub_get_sensors_data(SHUB_ACTIVE_PEDODEC, data);
            shub_input_report_stepdetect(data);
            shub_timer_wake_lock_start();
        }
    }

    if(iCurrentEnable & SHUB_ACTIVE_SIGNIFICANT){
        int32_t enable=0;
        if(intdetail2 & INTDETAIL_PEDOM_SIGNIFICANT){
            enable=1;
        }else if(intdetail2 & INTDETAIL_PEDOM_TOTAL_STATE){
            cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_DATA;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,0);
            if(ret != SHUB_RC_OK) {
                DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_DATA err(%x)\n", res.err.u16);
            }
            if(res.res.u8[0] == 1){
                //the continuation of steps
                enable=1;
            }
        }
        if(enable == 1){
            shub_activate_significant_exec(SHUB_ACTIVE_SIGNIFICANT, 0, &notify);
            shub_get_sensors_data(SHUB_ACTIVE_SIGNIFICANT, data);
            shub_input_report_significant(data);
            shub_timer_wake_lock_start();
        }
    }

    if(intdetail2 & INTDETAIL2_PEDOM_PICKUP){
        DBG(DBG_LV_INT, "### !! PickUp !!\n");
        
        cmd.cmd.u16 = HC_PICKUP_GET_ISR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,8);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_PICKUP_GET_ISR err(%x)\n", res.err.u16);
            shub_wake_lock_end(&shub_fusion_wake_lock);
        }else{
            rdata[0] = (uint16_t)(res.res.u8[4] | (res.res.u8[5] & 0xFF) << 8);
            rdata[1] = (uint16_t)(res.res.u8[6] | (res.res.u8[7] & 0xFF) << 8);
            DBG(DBG_LV_INT, "PickUp isr=%02x, en=%d, pk=%d, lv=%d, cnt=%d, rost=%d\n",
                res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],rdata[0],rdata[1]);
        }
        shub_input_report_exif_pickup_det();
        shub_timer_wake_lock_start();
    }

    sendInfo = !(intdetail & (INTDETAIL2_PEDOM_TOTAL_STATE | INTDETAIL2_PEDOM_RIDE_PAUSE));
    if(intdetail & INTDETAIL_GDETECT){
        DBG(DBG_LV_INT, "### !! G detection !!\n");

        shub_input_report_exif_grav_det(sendInfo);

        shub_timer_wake_lock_start();
    }

    sendInfo = !(intdetail & INTDETAIL2_PEDOM_TOTAL_STATE);
    if(intdetail & INTDETAIL2_PEDOM_RIDE_PAUSE) {
        mutex_lock(&userReqMutex);
        ret = shub_get_data_app_exec(APP_VEICHLE_DETECTION, rideInfo);
        mutex_unlock(&userReqMutex);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "INTDETAIL2_PEDOM_RIDE_PAUSE notify info get err.\n");
        } else {
            shub_input_report_exif_ride_pause_det(sendInfo, rideInfo[6]);
            shub_timer_wake_lock_start();
        }
    }

    if(intdetail & INTDETAIL2_PEDOM_TOTAL_STATE){
        shub_input_report_exif_judge();
        shub_timer_wake_lock_start();
    }
    shub_workqueue_delete(work);
    shub_wake_lock_end(&shub_acc_wake_lock);
    return;
}

static void shub_int_mag_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INT, "### shub_int_mag_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_MAG >> 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_mag_wake_lock);
        return;
    }

    if(res.res.u8[0] == 0x01){
        cmd.cmd.u16 = HC_MAG_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        mutex_lock(&s_tDataMutex);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_MAG_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestMagData.nXOffset = 0;
            s_tLatestMagData.nYOffset = 0;
            s_tLatestMagData.nZOffset = 0;
        }else{
            s_tLatestMagData.nXOffset = res.res.s16[0];
            s_tLatestMagData.nYOffset = res.res.s16[1];
            s_tLatestMagData.nZOffset = res.res.s16[2];

            DBG(DBG_LV_INFO, "get MAG Cal data X[%d] Y[%d] Z[%d]\n",
                    s_tLatestMagData.nXOffset,
                    s_tLatestMagData.nYOffset, 
                    s_tLatestMagData.nZOffset);
        }
        mutex_unlock(&s_tDataMutex);
        DBG(DBG_LV_INFO, "[DBG] Update Mag offset ");
    }else{
        DBG(DBG_LV_ERROR, "[DBG] other Mag IRQ detail(%x)\n", res.res.u8[0]);
    }
    shub_wake_lock_end(&shub_mag_wake_lock);
} 

static void shub_int_customer_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INT, "### shub_int_customer_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_CUSTOMER;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_customer_wake_lock);
        return;
    }
    if(res.res.u8[1] & 0x03) {
        shub_input_report_exif_mot_det(res.res.u8[1]);
        shub_timer_wake_lock_start();
    }

    DBG(DBG_LV_INT, "### !! Motion detection !! %x %x\n", res.res.u8[0], res.res.u8[1]);
    shub_wake_lock_end(&shub_customer_wake_lock);
} 

static void shub_int_gyro_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;
#ifdef SHUB_SW_TWIST_DETECT
    uint16_t rdata[3];
    uint8_t info;
#endif

    DBG(DBG_LV_INT, "### shub_int_gyro_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_GYRO >> 1;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_gyro_wake_lock);
        return;
    }

    if(res.res.u8[0] & INT_GYRO_CALIB){
        cmd.cmd.u16 = HC_GYRO_GET_OFFSET;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        mutex_lock(&s_tDataMutex);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GYRO_GET_OFFSET err(%x)\n", res.err.u16);
            s_tLatestGyroData.nXOffset = 0;
            s_tLatestGyroData.nYOffset = 0;
            s_tLatestGyroData.nZOffset = 0;
        }else{
            s_tLatestGyroData.nXOffset = res.res.s16[0];
            s_tLatestGyroData.nYOffset = res.res.s16[1];
            s_tLatestGyroData.nZOffset = res.res.s16[2];
            s_tLatestGyroData.nAccuracy = 3;
            DBG(DBG_LV_INFO, "get GYRO Cal data X[%d] Y[%d] Z[%d] Acc[%d]\n",
                    s_tLatestGyroData.nXOffset,
                    s_tLatestGyroData.nYOffset, 
                    s_tLatestGyroData.nZOffset,
                    s_tLatestGyroData.nAccuracy
               );
        }
        mutex_unlock(&s_tDataMutex);
        DBG(DBG_LV_INFO, "[DBG] Update Gyro offset\n");
#ifdef SHUB_SW_TWIST_DETECT
    }else if(res.res.u8[0] & (INT_GYRO_TWIST_LEFT | INT_GYRO_TWIST_RIGHT)) {
        info = (res.res.u8[0] & (INT_GYRO_TWIST_LEFT | INT_GYRO_TWIST_RIGHT));
        DBG(DBG_LV_INT, "### !! Twist isr=%d !!\n", res.res.u8[0]);
        
        cmd.cmd.u16 = HC_TWIST_GET_ISR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,9);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "HC_TWIST_GET_ISR err(%x)\n", res.err.u16);
            shub_wake_lock_end(&shub_fusion_wake_lock);
        }else{
            rdata[0] = (uint16_t)(res.res.u8[2] | (res.res.u8[3] & 0xFF) << 8);
            rdata[1] = (uint16_t)(res.res.u8[4] | (res.res.u8[5] & 0xFF) << 8);
            rdata[2] = (uint16_t)(res.res.u8[6] | (res.res.u8[7] & 0xFF) << 8);
            DBG(DBG_LV_INT, "Twist isr=%02x, en=%d, Left=%d, Right=%d, rost=%d, stat=%d\n",
                res.res.u8[0],res.res.u8[1],rdata[0],rdata[1],rdata[2],res.res.u8[8]);
        }
        shub_input_report_exif_twist_det(info);
        shub_timer_wake_lock_start();
#endif
    }else{
        DBG(DBG_LV_INFO, "[DBG] other Gyro IRQ detail(%x)\n", res.res.u8[0]);
    }
    shub_wake_lock_end(&shub_gyro_wake_lock);
} 

static void shub_int_fusion_work_func(struct work_struct *work)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    DBG(DBG_LV_INT, "### shub_int_fusion_work_func In \n");

    cmd.cmd.u16 = HC_MCU_GET_INT_DETAIL;
    cmd.prm.u16[0] = INTREQ_FUSION;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "HC_MCU_GET_INT_DETAIL err(%x)\n", res.err.u16);
        shub_wake_lock_end(&shub_fusion_wake_lock);
        return;
    }

    if((res.res.u8[0] == 0x01) || (res.res.u8[0] == 0x02) || (res.res.u8[0] == 0x03)){
        DBG(DBG_LV_INFO, "[DBG]shub_logging_flush by fifo full start\n");
        suspendBatchingProc();
        shub_logging_flush();
        resumeBatchingProc();
        shub_timer_wake_lock_start();
        DBG(DBG_LV_INFO, "[DBG]shub_logging_flush by fifo full end\n");
    }else{
        DBG(DBG_LV_ERROR, "[DBG] other Fusion IRQ detail(%x)\n", res.res.u8[0]);
    }
    shub_wake_lock_end(&shub_fusion_wake_lock);
}

static int32_t shub_calc_sensortask_period_us(int32_t period_us)
{
//  if(period_us <= 7*1000){ //0-7
//      return 1875;
//  }
//  if(period_us <= 14*1000){ //8-14
//      return 3750;
//  }
//  if(period_us <= 29*1000){//15-29
//      return 7500;
//  }
//  if(period_us <= 59*1000){//30-59
//      return 30000;
//  }
//  if(period_us <= 99*1000){//60-99
//      return 60000;
//  }
//  return 100000;
    if(period_us <= 29*1000){  // 0-29
        return 5000;
    }
    if(period_us < 100*1000){ // 30-100
        return (period_us / 5000) * 5000;
    }
    return 100000;
}

static int32_t shub_calc_fusion_period_us(int32_t period_us)
{
//  if(period_us <= 39*1000){
//      return 15000;
//  }
    return 20000;
}

static uint8_t shub_calc_sensorcnt(int32_t sensor_us , int32_t task_us)
{
    int32_t tmp_cnt;

    if(task_us == 0){
        return 1;
    }

    tmp_cnt = sensor_us / task_us;

    if(tmp_cnt > 255){
        tmp_cnt=255;
    }
    if(tmp_cnt <= 0){
        tmp_cnt = 1;
    }
    return (uint8_t)tmp_cnt;
}

static uint8_t shub_calc_sensorcnt_even(int32_t sensor_us , int32_t task_us)
{
    int32_t tmp_cnt = shub_calc_sensorcnt(sensor_us, task_us);
//  if((tmp_cnt > 1) && ((tmp_cnt % 2) != 0)){
//      tmp_cnt--;
//  }
    return (uint8_t)tmp_cnt;
}

static int32_t shub_set_delay_exec(int32_t arg_iSensType, int32_t arg_iLoggingType)
{
    HostCmd cmd;
    HostCmdRes res;
    uint32_t sensorTaskDelay_us=MEASURE_MAX_US;
    uint32_t fusionTaskDelay_us=MEASURE_MAX_US;
    uint32_t mag_delay=MEASURE_MAX_US;
    uint32_t gyro_delay=MEASURE_MAX_US;
    uint32_t gyro_filter_delay=MEASURE_MAX_US;
    uint32_t acc_delay=MEASURE_MAX_US;
#ifdef CONFIG_BARO_SENSOR
    uint32_t baro_delay=SENSOR_BARO_MAX_DELAY;
#endif
    uint32_t mag_logging_delay=MEASURE_MAX_US;
    uint32_t gyro_logging_delay=MEASURE_MAX_US;
    int32_t ret;
    int32_t sensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t loggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentSensorEnable = arg_iSensType |sensorEnable;
    int32_t iCurrentLoggingEnable = arg_iLoggingType | loggingEnable;
    SensorDelay sensor_delay_us = s_sensor_delay_us;
    SensorDelay logging_delay_us = s_logging_delay_us;

    DBG(DBG_LV_INFO, "####%s sensor=%s logging=%s\n",
            __FUNCTION__,
            shub_get_active_sensor_name(arg_iSensType),
            shub_get_active_sensor_name(arg_iLoggingType));

//    if((iCurrentSensorEnable & SHUB_ACTIVE_ACC) == 0){
    if((iCurrentSensorEnable & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC)) == 0){
        sensor_delay_us.acc  = MEASURE_MAX_US;
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_MAG) == 0){
        sensor_delay_us.mag  = MEASURE_MAX_US;
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_GYRO) == 0){
        sensor_delay_us.gyro = MEASURE_MAX_US;
    }
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentSensorEnable & SHUB_ACTIVE_BARO) == 0){
        sensor_delay_us.baro = SENSOR_BARO_MAX_DELAY;
    }
#endif

    if((iCurrentLoggingEnable & SHUB_ACTIVE_ACC) == 0){
        logging_delay_us.acc  = MEASURE_MAX_US;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_MAG) == 0){
        logging_delay_us.mag  = MEASURE_MAX_US;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_GYRO) == 0){
        logging_delay_us.gyro = MEASURE_MAX_US;
    }
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentLoggingEnable & SHUB_ACTIVE_BARO) == 0){
        logging_delay_us.baro = SENSOR_BARO_MAX_DELAY;
    }
#endif

    //Fusion TASK Cycle
    // for sensor measure
    if((iCurrentSensorEnable & SHUB_ACTIVE_ORI) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.orien   , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_GRAVITY) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.grav    , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_LACC) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.linear  , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_RV) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.rot     , fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_RV_NONMAG) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.rot_gyro, fusionTaskDelay_us);
    }
    if((iCurrentSensorEnable & SHUB_ACTIVE_RV_NONGYRO) != 0){
        fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.rot_mag , fusionTaskDelay_us);
    }

    // for logging
    if((iCurrentLoggingEnable & SHUB_ACTIVE_ORI) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.orien   , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_GRAVITY) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.grav    , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_LACC) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.linear  , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.rot     , fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONMAG) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.rot_gyro, fusionTaskDelay_us);
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONGYRO) != 0){
        fusionTaskDelay_us = SHUB_MIN(logging_delay_us.rot_mag , fusionTaskDelay_us);
    }
    fusionTaskDelay_us = SHUB_MAX(FUSION_TSK_DEFALUT_US, fusionTaskDelay_us);

//  sensor_delay_us.mag /= 2;
//  sensor_delay_us.mag_uc /= 2;

    //Fusion task enable condition
    if((iCurrentSensorEnable & FUSION_9AXIS_GROUP_MASK) | 
            (iCurrentLoggingEnable & FUSION_9AXIS_GROUP_MASK)){
        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC )
            sensor_delay_us.acc  = SHUB_MIN(FUSION_ACC_DELAY, sensor_delay_us.acc);
        else
            sensor_delay_us.acc  = FUSION_ACC_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAG)
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag);
        else
            sensor_delay_us.mag  = FUSION_MAG_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC)
            sensor_delay_us.mag_uc  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag_uc);
        else
            sensor_delay_us.mag_uc  = FUSION_MAG_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            sensor_delay_us.gyro = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro);
        }else{
            sensor_delay_us.gyro = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensor_delay_us.gyro_uc = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro_uc);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro_uc);
        }else{
            sensor_delay_us.gyro_uc  = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }
    }else if((iCurrentSensorEnable & FUSION_6AXIS_ACC_MAG_MASK) | 
            (iCurrentLoggingEnable & FUSION_6AXIS_ACC_MAG_MASK)){

        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC)
            sensor_delay_us.acc  = SHUB_MIN(FUSION_ACC_DELAY, sensor_delay_us.acc);
        else
            sensor_delay_us.acc  = FUSION_ACC_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAG)
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag);
        else
            sensor_delay_us.mag  = FUSION_MAG_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC)
            sensor_delay_us.mag_uc  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.mag_uc);
        else
            sensor_delay_us.mag_uc  = FUSION_MAG_DELAY;
    }else if((iCurrentSensorEnable & FUSION_6AXIS_ACC_GYRO_MASK) | 
            (iCurrentLoggingEnable & FUSION_6AXIS_ACC_GYRO_MASK)){

        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC)
            sensor_delay_us.acc  = SHUB_MIN(FUSION_ACC_DELAY, sensor_delay_us.acc);
        else
            sensor_delay_us.acc  = FUSION_ACC_DELAY;

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            sensor_delay_us.gyro = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro);
        }else{
            sensor_delay_us.gyro = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }

        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensor_delay_us.gyro_uc = SHUB_MIN(FUSION_GYRO_DELAY, sensor_delay_us.gyro_uc);
            /* for gyro calibrataion */
            sensor_delay_us.mag  = SHUB_MIN(FUSION_MAG_DELAY, sensor_delay_us.gyro_uc);
        }else{
            sensor_delay_us.gyro_uc  = FUSION_GYRO_DELAY;
            sensor_delay_us.mag  = FUSION_MAG_DELAY;
        }
    }else{
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            sensor_delay_us.mag  = SHUB_MIN(sensor_delay_us.mag, sensor_delay_us.gyro);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensor_delay_us.mag  = SHUB_MIN(sensor_delay_us.mag, sensor_delay_us.gyro_uc);
        }
    }

    // Sensor min cycle
    sensor_delay_us.acc     = SHUB_MAX(SENSOR_ACC_MIN_DELAY      , sensor_delay_us.acc);
    sensor_delay_us.mag     = SHUB_MAX(SENSOR_MAG_MIN_DELAY      , sensor_delay_us.mag);
    sensor_delay_us.mag     = SHUB_MIN(SENSOR_MAG_MAX_DELAY      , sensor_delay_us.mag);
    sensor_delay_us.gyro    = SHUB_MAX(SENSOR_GYRO_MIN_DELAY     , sensor_delay_us.gyro);
    sensor_delay_us.gyro    = SHUB_MIN(SENSOR_GYRO_MAX_DELAY     , sensor_delay_us.gyro);
    sensor_delay_us.mag_uc  = SHUB_MAX(SENSOR_MAGUC_MIN_DELAY    , sensor_delay_us.mag_uc );
    sensor_delay_us.mag_uc  = SHUB_MIN(SENSOR_MAGUC_MAX_DELAY    , sensor_delay_us.mag_uc );
    sensor_delay_us.gyro_uc = SHUB_MAX(SENSOR_GYROUC_MIN_DELAY   , sensor_delay_us.gyro_uc);
    sensor_delay_us.gyro_uc = SHUB_MIN(SENSOR_GYROUC_MAX_DELAY   , sensor_delay_us.gyro_uc);
    sensor_delay_us.baro    = SHUB_MAX(SENSOR_BARO_MIN_DELAY     , sensor_delay_us.baro);

    // Sensor TASK Cycle
    if(iCurrentLoggingEnable == 0){
        sensorTaskDelay_us=fusionTaskDelay_us;
//      if(iCurrentSensorEnable & (SHUB_ACTIVE_ACC | FUSION_GROUP_MASK)){
        if(iCurrentSensorEnable & (ACC_GROUP_MASK | FUSION_GROUP_ACC_MASK)){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc    , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & (SHUB_ACTIVE_MAG | FUSION_GROUP_MAG_MASK)){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.mag    , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.mag_uc , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & (SHUB_ACTIVE_GYRO | FUSION_GROUP_GYRO_MASK) ){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro   , sensorTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro_uc, sensorTaskDelay_us);
        }
#ifdef CONFIG_BARO_SENSOR
        if(iCurrentSensorEnable & SHUB_ACTIVE_BARO){
            sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.baro , sensorTaskDelay_us);
        }
#endif
        if(iCurrentSensorEnable & (~APPTASK_GROUP_ACC_MASK)){
            sensorTaskDelay_us = SHUB_MIN(SENSOR_TSK_DEFALUT_US, sensorTaskDelay_us);
        }

        if(iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.mag_uc, fusionTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_MAG){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.mag, fusionTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYRO){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro , fusionTaskDelay_us);
        }
        if(iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC){
            fusionTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro_uc , fusionTaskDelay_us);
        }
    }else{
        sensorTaskDelay_us = SENSOR_TSK_MIN_LOGGING_US ;

        if(iCurrentLoggingEnable & SHUB_ACTIVE_MAGUNC){
//          fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag_uc / 2, fusionTaskDelay_us);
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag_uc, fusionTaskDelay_us);
        }
        if(iCurrentLoggingEnable & SHUB_ACTIVE_MAG){
//          fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag / 2, fusionTaskDelay_us);
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.mag, fusionTaskDelay_us);
        }
        if(iCurrentLoggingEnable & SHUB_ACTIVE_GYRO){
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.gyro , fusionTaskDelay_us);
        }
        if(iCurrentLoggingEnable & SHUB_ACTIVE_GYROUNC){
            fusionTaskDelay_us = SHUB_MIN(logging_delay_us.gyro_uc , fusionTaskDelay_us);
        }
    }

    //App task enable condition
    if((iCurrentLoggingEnable | iCurrentSensorEnable) & APPTASK_GROUP_ACC_MASK){ 
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, APP_TSK_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    } 
    //G detection enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_GDEC){
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, GDEC_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    }

    // pickup enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_PICKUP){
        sensor_delay_us.acc = SHUB_MIN(sensor_delay_us.acc, PICKUP_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.acc , sensorTaskDelay_us);
    }

#ifdef SHUB_SW_TWIST_DETECT
    // twist enable condition
    if(iCurrentSensorEnable & SHUB_ACTIVE_TWIST){
        sensor_delay_us.gyro = SHUB_MIN(sensor_delay_us.gyro, TWIST_DEFALUT_US);
        sensorTaskDelay_us = SHUB_MIN(sensor_delay_us.gyro , sensorTaskDelay_us);
    }
#endif

    sensorTaskDelay_us = shub_calc_sensortask_period_us(sensorTaskDelay_us);
    fusionTaskDelay_us = shub_calc_fusion_period_us(fusionTaskDelay_us);
    s_sensor_task_delay_us = sensorTaskDelay_us;
    
#ifdef CONFIG_ACC_U2DH
    /***** Set Freerun *****/
    if(sensorTaskDelay_us < 8000){
        cmd.cmd.u16 = HC_ACC_GET_OPERATION_MODE;
        cmd.prm.u8[0] = 0;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "HC_ACC_GET_OPERATION_MODE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        if(res.res.u8[0] == 1){
            cmd.cmd.u16 = HC_ACC_SET_OPERATION_MODE;
            cmd.prm.u8[0] = 0;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_ACC_SET_OPERATION_MODE err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
            shub_operation_mode = 0;
        }
    }
#endif
    
    /*set task cycle*/
    cmd.cmd.u16 = HC_SENSOR_TSK_SET_CYCLE;
    cmd.prm.u16[0] = (uint16_t)(sensorTaskDelay_us / 10);
    cmd.prm.u16[1] = (uint16_t)(APP_TSK_DEFALUT_US / 10);
    cmd.prm.u16[2] = (uint16_t)(fusionTaskDelay_us / 10);
    DBG(DBG_LV_INFO, "Task Period:sens=%d app=%d fusion=%d\n"
            ,sensorTaskDelay_us,APP_TSK_DEFALUT_US,fusionTaskDelay_us);
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL,6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SENSOR_TSK_SET_CYCLE err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    s_micon_param.task_cycle[0] = cmd.prm.u16[0];
    s_micon_param.task_cycle[1] = cmd.prm.u16[1];
    s_micon_param.task_cycle[2] = cmd.prm.u16[2];

    if((iCurrentSensorEnable & MAG_GROUP_MASK) == MAG_GROUP_MASK){
        mag_delay = SHUB_MIN(sensor_delay_us.mag, sensor_delay_us.mag_uc);
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_MAGUNC) == SHUB_ACTIVE_MAGUNC ){
        mag_delay = sensor_delay_us.mag_uc;
    }
    mag_delay = SHUB_MIN(mag_delay, sensor_delay_us.mag);

    if((iCurrentSensorEnable & GYRO_GROUP_MASK) == GYRO_GROUP_MASK){
        gyro_delay = SHUB_MIN(sensor_delay_us.gyro, sensor_delay_us.gyro_uc);
        gyro_filter_delay = SHUB_MIN(s_sensor_delay_us.gyro, s_sensor_delay_us.gyro_uc);
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_GYROUNC) == SHUB_ACTIVE_GYROUNC ){
        gyro_delay = sensor_delay_us.gyro_uc;
        gyro_filter_delay = s_sensor_delay_us.gyro_uc;
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_ORI) == SHUB_ACTIVE_ORI ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_sensor_delay_us.orien);
    }else if((iCurrentSensorEnable & SHUB_ACTIVE_GYRO) == SHUB_ACTIVE_GYRO ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_sensor_delay_us.gyro);
    }

    gyro_delay = SHUB_MIN(gyro_delay, sensor_delay_us.gyro);


    if((iCurrentLoggingEnable & MAG_GROUP_MASK) == MAG_GROUP_MASK){
        mag_logging_delay = SHUB_MIN(logging_delay_us.mag, logging_delay_us.mag_uc);
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_MAGUNC) == SHUB_ACTIVE_MAGUNC ){
        mag_logging_delay = logging_delay_us.mag_uc;
    }
    mag_logging_delay = SHUB_MIN(mag_logging_delay, logging_delay_us.mag);

    if((iCurrentLoggingEnable & GYRO_GROUP_MASK) == GYRO_GROUP_MASK){
        gyro_logging_delay = SHUB_MIN(logging_delay_us.gyro, logging_delay_us.gyro_uc);
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro);
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro_uc);
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_GYROUNC) == SHUB_ACTIVE_GYROUNC ){
        gyro_logging_delay = logging_delay_us.gyro_uc;
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro_uc);
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_ORI) == SHUB_ACTIVE_ORI ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.orien);
    }else if((iCurrentLoggingEnable & SHUB_ACTIVE_GYRO) == SHUB_ACTIVE_GYRO ){
        gyro_filter_delay = SHUB_MIN(gyro_filter_delay, s_logging_delay_us.gyro);
    }
    gyro_logging_delay = SHUB_MIN(gyro_logging_delay, logging_delay_us.gyro);
    gyro_delay = SHUB_MIN(gyro_delay, gyro_logging_delay);

    acc_delay = sensor_delay_us.acc;
    /* lsm6ds only */
    if(iCurrentLoggingEnable & (FUSION_GROUP_ACC_MASK | SHUB_ACTIVE_ACC | APPTASK_GROUP_ACC_MASK )){ 
        acc_delay = SHUB_MIN(logging_delay_us.acc, sensor_delay_us.acc);
        if(((iCurrentLoggingEnable|iCurrentSensorEnable) & (APPTASK_GROUP_ACC_MASK|SHUB_ACTIVE_GDEC)) 
                && (logging_delay_us.acc < sensor_delay_us.acc)){ 
            acc_delay =SENSOR_ACC_MIN_DELAY; 
        } 
    }

#ifdef CONFIG_BARO_SENSOR
    baro_delay = sensor_delay_us.baro;
    if(iCurrentLoggingEnable & SHUB_ACTIVE_BARO){
        baro_delay = SHUB_MIN(logging_delay_us.baro, sensor_delay_us.baro);
    }
    if(baro_delay >= SENSOR_BARO_MAX_DELAY) {
        baro_delay = SENSOR_BARO_MAX_DELAY;
    } else if(baro_delay >= SENSOR_BARO_MID_DELAY) {
        baro_delay = SENSOR_BARO_MID_DELAY;
    } else if(baro_delay >= SENSOR_BARO_LOW_DELAY) {
        baro_delay = SENSOR_BARO_LOW_DELAY;
    }
#endif

#ifdef SHUB_SW_GYRO_ENABLE
    cmd.cmd.u16 = HC_GYRO_SET_FILTER;
//  cmd.prm.u8[0] = 1;
//  if(gyro_filter_delay >= 200*1000){
//      cmd.prm.u8[0] = 16; /*120ms*/
//  }else if(gyro_filter_delay >= 60*1000){
//      cmd.prm.u8[0] = 7; /*52.5ms*/
//  }else if(gyro_filter_delay >= 20*1000){
//      cmd.prm.u8[0] = 2; /*15ms*/
//  }else if(gyro_filter_delay >= 5*1000){
//      cmd.prm.u8[0] = 1; /*3.75ms*/
//  }
    if((iCurrentLoggingEnable | iCurrentSensorEnable) & (GYRO_GROUP_MASK | SHUB_ACTIVE_ORI)){ 
        cmd.prm.u8[0] = shub_calc_sensorcnt_even(gyro_filter_delay, gyro_delay);
        if(cmd.prm.u8[0] > 16){
            cmd.prm.u8[0] = 16;
        }
    }else{
        cmd.prm.u8[0] = 1;
    }
    if(s_micon_param.gyro_filter != cmd.prm.u8[0]){
        DBG(DBG_LV_INFO, "GyroFilter=%d\n", cmd.prm.u8[0]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_FILTER err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
    s_micon_param.gyro_filter = cmd.prm.u8[0];
#endif

    /*set sensor measure cycle */
    cmd.cmd.u16 = HC_SENSOR_SET_CYCLE;
    cmd.prm.u8[0] = shub_calc_sensorcnt_even(acc_delay, sensorTaskDelay_us);
#ifdef CONFIG_BARO_SENSOR
    cmd.prm.u8[1] = shub_calc_sensorcnt_even(baro_delay, sensorTaskDelay_us);
#else
    cmd.prm.u8[1] = 12; /** !! not implement !! */
#endif
    cmd.prm.u8[2] = shub_calc_sensorcnt_even(mag_delay , sensorTaskDelay_us);
    cmd.prm.u8[3] = shub_calc_sensorcnt_even(gyro_delay , sensorTaskDelay_us);

#ifdef CONFIG_BARO_SENSOR
    DBG(DBG_LV_INFO, "Poll Period:acc=%d baro=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[1],cmd.prm.u8[2],cmd.prm.u8[3]);
#else
    DBG(DBG_LV_INFO, "Poll Period:acc=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[2],cmd.prm.u8[3]);
#endif
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_SET_CYCLE err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    s_micon_param.s_cycle[0] = cmd.prm.u8[0]*sensorTaskDelay_us;
    s_micon_param.s_cycle[1] = cmd.prm.u8[1]*sensorTaskDelay_us;
    s_micon_param.s_cycle[2] = cmd.prm.u8[2]*sensorTaskDelay_us;
    s_micon_param.s_cycle[3] = cmd.prm.u8[3]*sensorTaskDelay_us;

    /*set logging sensor cycle */
    cmd.cmd.u16 = HC_LOGGING_SENSOR_SET_CYCLE;
    cmd.prm.u8[0] = shub_calc_sensorcnt(logging_delay_us.acc , sensorTaskDelay_us);
#ifdef CONFIG_BARO_SENSOR
    cmd.prm.u8[1] = shub_calc_sensorcnt(logging_delay_us.baro, sensorTaskDelay_us);
#else
    cmd.prm.u8[1] = 12; /** !! not implement !! */
#endif
    cmd.prm.u8[2] = shub_calc_sensorcnt(mag_logging_delay  , sensorTaskDelay_us);
    cmd.prm.u8[3] = shub_calc_sensorcnt(gyro_logging_delay , sensorTaskDelay_us);

#ifdef CONFIG_BARO_SENSOR
    DBG(DBG_LV_INFO, "LogS Period:acc=%d baro=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[1],cmd.prm.u8[2],cmd.prm.u8[3]);
#else
    DBG(DBG_LV_INFO, "LogS Period:acc=%d mag=%d gyro=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[2],cmd.prm.u8[3]);
#endif
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
    shub_logging_cycle.acc  = cmd.prm.u8[0]*sensorTaskDelay_us;
    shub_logging_cycle.baro = cmd.prm.u8[1]*sensorTaskDelay_us;
    shub_logging_cycle.mag  = cmd.prm.u8[2]*sensorTaskDelay_us;
    shub_logging_cycle.gyro = cmd.prm.u8[3]*sensorTaskDelay_us;

    /*set loggin fusion cycle */
    cmd.cmd.u16 = HC_LOGGING_FUSION_SET_CYCLE;
//  cmd.prm.u8[0] = shub_calc_sensorcnt(logging_delay_us.orien    ,fusionTaskDelay_us);
//  cmd.prm.u8[1] = shub_calc_sensorcnt(logging_delay_us.grav     ,fusionTaskDelay_us);
//  cmd.prm.u8[2] = shub_calc_sensorcnt(logging_delay_us.linear   ,fusionTaskDelay_us);
//  cmd.prm.u8[3] = shub_calc_sensorcnt(logging_delay_us.rot      ,fusionTaskDelay_us);
//  cmd.prm.u8[4] = shub_calc_sensorcnt(logging_delay_us.rot_gyro ,fusionTaskDelay_us);
//  cmd.prm.u8[5] = shub_calc_sensorcnt(logging_delay_us.rot_mag  ,fusionTaskDelay_us);
    cmd.prm.u8[0] = shub_calc_sensorcnt(logging_delay_us.orien    ,sensorTaskDelay_us);
    cmd.prm.u8[1] = shub_calc_sensorcnt(logging_delay_us.grav     ,sensorTaskDelay_us);
    cmd.prm.u8[2] = shub_calc_sensorcnt(logging_delay_us.linear   ,sensorTaskDelay_us);
    cmd.prm.u8[3] = shub_calc_sensorcnt(logging_delay_us.rot      ,sensorTaskDelay_us);
    cmd.prm.u8[4] = shub_calc_sensorcnt(logging_delay_us.rot_gyro ,sensorTaskDelay_us);
    cmd.prm.u8[5] = shub_calc_sensorcnt(logging_delay_us.rot_mag  ,sensorTaskDelay_us);

    DBG(DBG_LV_INFO , "LogF Period:ori=%d grav=%d linear=%d rot=%d game=%d magrot=%d\n",
            cmd.prm.u8[0], cmd.prm.u8[1], cmd.prm.u8[2],
            cmd.prm.u8[3] , cmd.prm.u8[4], cmd.prm.u8[5]);
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "FUSION_HC_LOGGING_SET_CYCLE err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    shub_logging_cycle.orien    = cmd.prm.u8[0]*sensorTaskDelay_us;
    shub_logging_cycle.grav     = cmd.prm.u8[1]*sensorTaskDelay_us;
    shub_logging_cycle.linear   = cmd.prm.u8[2]*sensorTaskDelay_us;
    shub_logging_cycle.rot      = cmd.prm.u8[3]*sensorTaskDelay_us;
    shub_logging_cycle.rot_gyro = cmd.prm.u8[4]*sensorTaskDelay_us;
    shub_logging_cycle.rot_mag  = cmd.prm.u8[5]*sensorTaskDelay_us;

#ifdef CONFIG_BARO_SENSOR
    cmd.cmd.u16 = HC_BARO_GET_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_BARO_GET_PARAM err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_BARO_SET_PARAM;
        cmd.prm.u8[0] = 0;
        cmd.prm.u8[1] = res.res.u8[1];
        cmd.prm.u8[2] = 0;
        if(baro_delay == SENSOR_BARO_MAX_DELAY){
            cmd.prm.u8[3] = 1;
        } else {
            cmd.prm.u8[3] = 2;
        }
        cmd.prm.u8[4] = res.res.u8[4];
        cmd.prm.u8[5] = res.res.u8[5];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_BARO_SET_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
#endif

    return SHUB_RC_OK;
}

static uint8_t oldParam9 = 0;

static int32_t shub_set_param_exec(int32_t type , int32_t *param)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;

    if(type == APP_PEDOMETER){
        cmd.prm.u8[0x00] = (uint8_t)param[0x00];
        cmd.prm.u8[0x01] = (uint8_t)param[0x01];
        cmd.prm.u8[0x02] = (uint8_t)param[0x02];
        cmd.prm.u8[0x03] = (uint8_t)param[0x03];
        cmd.prm.u8[0x04] = (uint8_t)param[0x04];
        cmd.prm.u8[0x05] = (uint8_t)param[0x05];
        cmd.prm.u8[0x06] = (uint8_t)param[0x06];
        cmd.prm.u8[0x07] = (uint8_t)param[0x07];
        cmd.prm.u8[0x08] = (uint8_t)param[0x08];
        cmd.prm.u8[0x09] = (uint8_t)param[0x09];
        cmd.prm.u8[0x0a] = (uint8_t)param[0x0a];
        cmd.prm.u8[0x0b] = (uint8_t)param[0x0b];
        cmd.prm.u8[0x0c] = (uint8_t)param[0x0c];
        cmd.prm.u8[0x0d] = (uint8_t)param[0x0d];

        ret = shub_set_delay_exec(SHUB_ACTIVE_EXT_PEDOM, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_EXT_PEDOM, param[0], 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }

        cmd.cmd.u16 = HC_SET_PEDO2_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_PEDOMETER_N){
        cmd.prm.u8[0x00] = (uint8_t)param[0x00];
        cmd.prm.u8[0x01] = (uint8_t)param[0x01];
        cmd.prm.u8[0x02] = (uint8_t)param[0x02];
        cmd.prm.u8[0x03] = (uint8_t)param[0x03];
        cmd.prm.u8[0x04] = (uint8_t)param[0x04];
        cmd.prm.u8[0x05] = (uint8_t)param[0x05];
        cmd.prm.u8[0x06] = (uint8_t)param[0x06];
        cmd.prm.u8[0x07] = (uint8_t)param[0x07];
        cmd.prm.u8[0x08] = (uint8_t)param[0x08];
        cmd.prm.u8[0x09] = (uint8_t)param[0x09];
        cmd.prm.u8[0x0a] = (uint8_t)param[0x0a];
        cmd.prm.u8[0x0b] = (uint8_t)param[0x0b];
        cmd.prm.u8[0x0c] = (uint8_t)param[0x0c];
        cmd.prm.u8[0x0d] = (uint8_t)param[0x0d];

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_CALORIE_FACTOR){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];

        cmd.cmd.u16 = HC_SET_PEDO2_CALORIE_FACTOR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_CALORIE_FACTOR err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_RUN_DETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_PEDO2_RUN_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_RUN_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

    }else if(type == APP_VEICHLE_DETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)param[3];
        cmd.prm.u8[0x05] = (uint8_t)param[4];
        cmd.prm.u8[0x06] = (uint8_t)param[5];
        cmd.prm.u8[0x07] = (uint8_t)param[6];
        cmd.prm.u8[0x08] = (uint8_t)param[7];
        cmd.prm.u8[0x09] = (uint8_t)param[8];
        cmd.prm.u8[0x0A] = (uint8_t)param[9];
        cmd.prm.u8[0x0B] = (uint8_t)param[10];
        cmd.prm.u8[0x0C] = (uint8_t)param[11];
        cmd.prm.u8[0x0D] = (uint8_t)param[12];

        cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_VEICHLE_DETECTION2){
        cmd.prm.u8[0x00] = (uint8_t)(param[0] & 0xff);
        cmd.prm.u8[0x01] = (uint8_t)((param[0] >> 8) & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)((param[2] >> 8) & 0xff);
//      cmd.prm.u8[0x06] = (uint8_t)param[3];
//      cmd.prm.u8[0x07] = (uint8_t)param[4];
//      cmd.prm.u8[0x08] = (uint8_t)(param[5] & 0xff);
//      cmd.prm.u8[0x09] = (uint8_t)((param[5] >> 8) & 0xff);
//      cmd.prm.u8[0x0A] = (uint8_t)param[6];
//      cmd.prm.u8[0x0B] = (uint8_t)param[7];
//      cmd.prm.u8[0x0C] = (uint8_t)(param[8] & 0xff);
//      cmd.prm.u8[0x0D] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM2;
//      ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_DETECT_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_VEICHLE_DETECTION3){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)param[3];
        cmd.prm.u8[0x06] = (uint8_t)param[4];
        cmd.prm.u8[0x07] = (uint8_t)param[5];
        cmd.prm.s8[0x08] = (int8_t)param[6];

        cmd.cmd.u16 = HC_SET_ACTIVITY2_DETECT_PARAM3;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_DETECT_PARAM3 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

    }else if(type == APP_TOTAL_STATUS_DETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)(param[3] & 0xff);
        cmd.prm.u8[0x06] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)param[4];
        cmd.prm.u8[0x08] = (uint8_t)param[5];
        cmd.prm.u8[0x09] = (uint8_t)param[6];
        cmd.prm.u8[0x0A] = (uint8_t)param[7];
        cmd.prm.u8[0x0B] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0C] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0D] = (uint8_t)param[9];
        cmd.prm.u8[0x0E] = (uint8_t)param[10];

        cmd.cmd.u16 = HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

    }else if(type == APP_PEDO_ALL_STATE_2ND){
        cmd.prm.u8[0x00] = (uint8_t)(param[0] & 0xff);
        cmd.prm.u8[0x01] = (uint8_t)((param[0] >> 8) & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[0] >>16) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[0] >>24) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)param[1];

        cmd.cmd.u16 = HC_ACC_PEDO_SET_ALL_STATE_2ND;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 5);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_PEDO_SET_ALL_STATE_2ND err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_GDETECTION){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)(param[3] & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x06] = (uint8_t)param[4];
        cmd.prm.u8[0x07] = (uint8_t)param[5];
        cmd.prm.u8[0x08] = (uint8_t)param[6];

        ret = shub_set_delay_exec(SHUB_ACTIVE_GDEC, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_GDEC, param[0], 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_SET_GDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_GDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_MOTDTECTION){
        shub_set_param_exif_md_mode(param);
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        if(shub_get_already_md_flg() & 0x02) {
            param[0] = 1;
            cmd.prm.u8[0x00] = 1;
        }
        cmd.cmd.u16 = HC_SET_MOTDETECTION_EN;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_MOTDETECTION_EN err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        cmd.prm.u8[0x00] = (uint8_t)param[1];
        cmd.prm.u8[0x01] = (uint8_t)param[2];
        cmd.prm.u8[0x02] = (uint8_t)param[3];
        cmd.prm.u8[0x03] = (uint8_t)param[4];
        cmd.prm.u8[0x04] = (uint8_t)param[5];
        cmd.prm.u8[0x05] = (uint8_t)param[6];
        cmd.prm.u8[0x06] = (uint8_t)param[7];
        cmd.prm.u8[0x07] = (uint8_t)param[8];
        cmd.cmd.u16 = HC_SET_MOTDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 8);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_MOTDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        if (param[9] & 0x01) {
            shub_mot_still_enable_flag = true;
        } else {
            shub_mot_still_enable_flag = false;
        }
        ret = shub_set_delay_exec(SHUB_ACTIVE_MOTIONDEC, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_MOTIONDEC, param[0], 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
//        cmd.prm.u8[0x00] = (uint8_t)param[9];
        cmd.prm.u8[0x00] = (uint8_t)(param[9] & 0x03);
        if(shub_get_already_md_flg() & 0x02) {
            cmd.prm.u8[0x00] |= 0x02;
        }
        cmd.cmd.u16 = HC_SET_MOTDETECTION_INT;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_MOTDETECTION_INT err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        if(param[9] & 0x02) {
            if((oldParam9 & 0x01) == (param[9] & 0x01)) {
                if(!(param[9] & 0x80)) {
                    shub_set_already_md_flg(1);
                }
            }
        }
        else if(!(param[9] & 0x02)) {
            if((oldParam9 & 0x01) == (param[9] & 0x01)) {
                shub_clr_already_md_flg(1);
            }
        }
//        oldParam9 = param[9];
        oldParam9 = param[9] & 0x03;
    }else if(type == APP_LOW_POWER){
        cmd.cmd.u16 = HC_SET_LPM_PARAM;

        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)param[3];
        cmd.prm.u8[0x06] = (uint8_t)param[4];
        cmd.prm.u8[0x07] = (uint8_t)(param[5] & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)((param[5] >> 8) & 0xff);

        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LPM_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_PEDOMETER2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)(param[6] & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)((param[6] >> 8) & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)(param[7] & 0xff);
        cmd.prm.u8[0x09] = (uint8_t)((param[7] >> 8) & 0xff);
        cmd.prm.u8[0x0a] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0c] = (uint8_t)(param[9] & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[9] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_PEDO2_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO2_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_PEDOMETER2_2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)(param[6] & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)((param[6] >> 8) & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)(param[7] & 0xff);
        cmd.prm.u8[0x09] = (uint8_t)((param[7] >> 8) & 0xff);
        cmd.prm.u8[0x0a] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0c] = (uint8_t)(param[9] & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[9] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 14);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
#ifdef SHUB_SW_PICKUP_DETECT
    }else if(type == APP_PICKUP_ENABLE){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
//      cmd.prm.u8[0x01] = SHUB_PICKUP_ENABLE_PARAM;
#ifdef CONFIG_PICKUP_PROX
        cmd.prm.u8[0x01] |= SHUB_PICKUP_ENABLE_PARAM_STILL;
#endif

        ret = shub_set_delay_exec(SHUB_ACTIVE_PICKUP, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_PICKUP, param[0], 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_PICKUP_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_PICKUP_SET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
//  }else if(type == APP_PICKUP_PARAM1){
//  }else if(type == APP_PICKUP_PARAM2){
#endif
#ifdef SHUB_SW_TWIST_DETECT
    }else if(type == APP_TWIST_ENABLE){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        ret = shub_set_delay_exec(SHUB_ACTIVE_TWIST, 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        ret = shub_activate_exec(SHUB_ACTIVE_TWIST, param[0], 0);
        if(ret != SHUB_RC_OK) {
            return SHUB_RC_ERR;
        }
        cmd.cmd.u16 = HC_TWIST_SET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TWIST_SET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
#endif
    }else if(type == APP_PAUSE_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];

        cmd.cmd.u16 = HC_SET_ACTIVITY2_PAUSE_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_PAUSE_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_PAUSE_STATUS_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)param[6];
        cmd.prm.u8[0x07] = (uint8_t)param[7];
        cmd.prm.u8[0x08] = (uint8_t)param[8];
        cmd.prm.u8[0x09] = (uint8_t)((param[9] >> 0) & 0xff);
        cmd.prm.u8[0x0a] = (uint8_t)((param[9] >> 8) & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)param[10];
        cmd.prm.u8[0x0c] = (uint8_t)((param[11] >> 0) & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[11] >> 8) & 0xff);
        cmd.prm.u8[0x0e] = (uint8_t)((param[12] >> 0) & 0xff);
        cmd.prm.u8[0x0f] = (uint8_t)((param[12] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_ACTIVITY2_PAUSE_STATUS_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 16);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY2_PAUSE_STATUS_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_LPM_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)((param[3] >> 0) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)param[4];
        cmd.prm.u8[0x06] = (uint8_t)((param[5] >> 0) & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)((param[5] >> 8) & 0xff);
        cmd.prm.u8[0x08] = (uint8_t)param[6];
        cmd.prm.u8[0x09] = (uint8_t)param[7];
        cmd.prm.u8[0x0a] = (uint8_t)((param[8] >> 0) & 0xff);
        cmd.prm.u8[0x0b] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0c] = (uint8_t)((param[9] >> 0) & 0xff);
        cmd.prm.u8[0x0d] = (uint8_t)((param[9] >> 8) & 0xff);
        cmd.prm.u8[0x0e] = (uint8_t)((param[10] >> 0) & 0xff);
        cmd.prm.u8[0x0f] = (uint8_t)((param[10] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_LOW_POWER_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 16);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LOW_POWER_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        shub_lowpower_mode = (uint8_t)param[0];
    }else if(type == APP_LPM_DEV_PARAM){
        cmd.cmd.u16 = HC_SET_LOW_POWER_DEV_PARAM;
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)param[2];
        cmd.prm.u8[0x03] = (uint8_t)param[3];

#ifdef CONFIG_ACC_U2DH
        cmd.prm.u8[0x04] = (uint8_t)param[4];
        cmd.prm.u8[0x05] = (uint8_t)param[5];
        cmd.prm.u8[0x06] = (uint8_t)param[6];
        cmd.prm.u8[0x07] = (uint8_t)param[7];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 8);
#else
//      ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
#endif
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LOW_POWER_DEV_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_RIDE_PEDO_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];

        cmd.cmd.u16 = HC_SET_RIDE_PEDO_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_RIDE_PEDO_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_RIDE_PEDO2_PARAM){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];

        cmd.cmd.u16 = HC_SET_RIDE_PEDO2_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_RIDE_PEDO2_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_LPM_DEV_PARAM2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)param[1];
        cmd.prm.u8[0x02] = (uint8_t)((param[2] >> 0) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[3] >> 0) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)((param[3] >> 8) & 0xff);

        cmd.cmd.u16 = HC_SET_LOW_POWER_DEV_PARAM2;
//        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_LOW_POWER_DEV_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else if(type == APP_TOTAL_STATUS_DETECTION2){
        cmd.prm.u8[0x00] = (uint8_t)param[0];
        cmd.prm.u8[0x01] = (uint8_t)(param[1] & 0xff);
        cmd.prm.u8[0x02] = (uint8_t)((param[1] >> 8) & 0xff);
        cmd.prm.u8[0x03] = (uint8_t)(param[2] & 0xff);
        cmd.prm.u8[0x04] = (uint8_t)((param[2] >> 8) & 0xff);
        cmd.prm.u8[0x05] = (uint8_t)(param[3] & 0xff);
        cmd.prm.u8[0x06] = (uint8_t)((param[3] >> 8) & 0xff);
        cmd.prm.u8[0x07] = (uint8_t)param[4];
        cmd.prm.u8[0x08] = (uint8_t)param[5];
        cmd.prm.u8[0x09] = (uint8_t)param[6];
        cmd.prm.u8[0x0A] = (uint8_t)param[7];
        cmd.prm.u8[0x0B] = (uint8_t)(param[8] & 0xff);
        cmd.prm.u8[0x0C] = (uint8_t)((param[8] >> 8) & 0xff);
        cmd.prm.u8[0x0D] = (uint8_t)param[9];
        cmd.prm.u8[0x0E] = (uint8_t)param[10];

        cmd.cmd.u16 = HC_SET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }else{
        return SHUB_RC_ERR;
    }
    return SHUB_RC_OK;
}

static int32_t shub_get_param_exec(int32_t type ,int32_t *param)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;

    if(type == APP_PEDOMETER){
        cmd.cmd.u16 = HC_GET_PEDO2_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)res.res.u8[0x00];
        param[1] = (int32_t)res.res.u8[0x01];
        param[2] = (int32_t)res.res.u8[0x02];
        param[3] = (int32_t)res.res.u8[0x03];
        param[4] = (int32_t)res.res.u8[0x04];
        param[5] = (int32_t)res.res.u8[0x05];
        param[6] = (int32_t)res.res.u8[0x06];
        param[7] = (int32_t)res.res.u8[0x07];
        param[8] = (int32_t)res.res.u8[0x08];
        param[9] = (int32_t)res.res.u8[0x09];
        param[10] = (int32_t)res.res.u8[0x0a];
        param[11] = (int32_t)res.res.u8[0x0b];
        param[12] = (int32_t)res.res.u8[0x0c];
        param[13] = (int32_t)res.res.u8[0x0d];
    }else if(type == APP_PEDOMETER_N){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)res.res.u8[0x00];
        param[1] = (int32_t)res.res.u8[0x01];
        param[2] = (int32_t)res.res.u8[0x02];
        param[3] = (int32_t)res.res.u8[0x03];
        param[4] = (int32_t)res.res.u8[0x04];
        param[5] = (int32_t)res.res.u8[0x05];
        param[6] = (int32_t)res.res.u8[0x06];
        param[7] = (int32_t)res.res.u8[0x07];
        param[8] = (int32_t)res.res.u8[0x08];
        param[9] = (int32_t)res.res.u8[0x09];
        param[10] = (int32_t)res.res.u8[0x0a];
        param[11] = (int32_t)res.res.u8[0x0b];
        param[12] = (int32_t)res.res.u8[0x0c];
        param[13] = (int32_t)res.res.u8[0x0d];
    }else if(type == APP_CALORIE_FACTOR){
        cmd.cmd.u16 = HC_GET_PEDO2_CALORIE_FACTOR;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_CALORIE_FACTOR err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)res.res.u8[0x00];
        param[1] = (int32_t)res.res.u8[0x01];
        param[2] = (int32_t)res.res.u8[0x02];
        param[3] = (int32_t)res.res.u8[0x03];
        param[4] = (int32_t)res.res.u8[0x04];
        param[5] = (int32_t)res.res.u8[0x05];
    }else if(type == APP_RUN_DETECTION){
        cmd.cmd.u16 = HC_GET_PEDO2_RUN_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_RUN_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
    }else if(type == APP_VEICHLE_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[8] = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[9] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
        param[10] = (int32_t)(uint8_t)RESU8_TO_X8(res,11);
        param[11] = (int32_t)(uint8_t)RESU8_TO_X8(res,12);
        param[12] = (int32_t)(uint8_t)RESU8_TO_X8(res,13);
    }else if(type == APP_TOTAL_STATUS_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint32_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] = (int32_t)(uint16_t)RESU8_TO_X16(res,5);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
        param[8] = (int32_t)(uint16_t)RESU8_TO_X16(res,11);
        param[9] = (int32_t)(uint16_t)RESU8_TO_X8(res,13);
        param[10] = (int32_t)(uint16_t)RESU8_TO_X8(res,14);

    }else if(type == APP_PEDO_ALL_STATE_2ND){
        cmd.cmd.u16 = HC_ACC_PEDO_GET_ALL_STATE_2ND;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_PEDO_GET_ALL_STATE_2ND err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint32_t)RESU8_TO_X32(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
    }else if(type == APP_VEICHLE_DETECTION2){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint16_t)RESU8_TO_X16(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,4);
//      param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
//      param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
//      param[5] = (int32_t)(uint16_t)RESU8_TO_X16(res,8);
//      param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
//      param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,11);
//      param[8] = (int32_t)(uint16_t)RESU8_TO_X16(res,12);
    }else if(type == APP_VEICHLE_DETECTION3){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_PARAM3;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_DETECT_PARAM3 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[6] = (int32_t)(int8_t)RESU8_TO_X8(res,8);
    }else if(type == APP_GDETECTION){
        cmd.cmd.u16 = HC_GET_GDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_GDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3] = (int32_t)(uint16_t)RESU8_TO_X16(res,4);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);

    }else if(type == APP_MOTDTECTION){
        cmd.cmd.u16 = HC_GET_MOTDETECTION_EN;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_EN err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);

        cmd.cmd.u16 = HC_GET_MOTDETECTION_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[2] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[3] = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[8] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);

        cmd.cmd.u16 = HC_GET_MOTDETECTION_INT;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_INT err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[9] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        if (param[9] & 0x01) {
            shub_mot_still_enable_flag = true;
        } else {
            shub_mot_still_enable_flag = false;
        }
    }else if(type == MCU_TASK_CYCLE){
        cmd.cmd.u16 = HC_SENSOR_TSK_GET_CYCLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_TSK_GET_CYCLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        param[0] = (int32_t)res.res.u16[0] * 10;
        param[1] = (int32_t)res.res.u16[1] * 10;
        param[2] = (int32_t)res.res.u16[2] * 10;
    }else if(type == APP_LOW_POWER){
        cmd.cmd.u16 = HC_GET_LPM_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LPM_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,7);
    }else if(type == APP_PEDOMETER2){
        cmd.cmd.u16 = HC_GET_PEDO2_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] =(int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,6);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[8] =(int32_t)(uint16_t)RESU8_TO_X16(res,10);
        param[9] =(int32_t)(uint16_t)RESU8_TO_X16(res,12);
    }else if(type == APP_PEDOMETER2_2){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] =(int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,6);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[8] =(int32_t)(uint16_t)RESU8_TO_X16(res,10);
        param[9] =(int32_t)(uint16_t)RESU8_TO_X16(res,12);
#ifdef SHUB_SW_PICKUP_DETECT
    }else if(type == APP_PICKUP_ENABLE){
        cmd.cmd.u16 = HC_PICKUP_GET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_PICKUP_GET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
#ifdef CONFIG_PICKUP_PROX
        param[1] &= ~SHUB_PICKUP_ENABLE_PARAM_STILL;
#endif
//  }else if(type == APP_PICKUP_PARAM1){
//  }else if(type == APP_PICKUP_PARAM2){
#endif
#ifdef SHUB_SW_TWIST_DETECT
    }else if(type == APP_TWIST_ENABLE){
        cmd.cmd.u16 = HC_TWIST_GET_ENABLE;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TWIST_GET_ENABLE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
#endif
    }else if(type == APP_PAUSE_PARAM){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_PAUSE_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_PAUSE_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
    }else if(type == APP_PAUSE_STATUS_PARAM){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_PAUSE_STATUS_PARAM;
        cmd.prm.u8[0x00] = param[0];
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY2_PAUSE_STATUS_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3]  = (int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4]  = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5]  = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6]  = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[7]  = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[8]  = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[9]  = (int32_t)(uint16_t)RESU8_TO_X16(res,9);
        param[10] = (int32_t)(uint8_t)RESU8_TO_X8(res,11);
        param[11] = (int32_t)(uint16_t)RESU8_TO_X16(res,12);
        param[12] = (int32_t)(uint16_t)RESU8_TO_X16(res,14);
    }else if(type == APP_LPM_PARAM){
        cmd.cmd.u16 = HC_GET_LOW_POWER_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LOW_POWER_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3]  = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[4]  = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[5]  = (int32_t)(uint16_t)RESU8_TO_X16(res,6);
        param[6]  = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[7]  = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[8]  = (int32_t)(uint16_t)RESU8_TO_X16(res,10);
        param[9]  = (int32_t)(uint16_t)RESU8_TO_X16(res,12);
        param[10] = (int32_t)(uint16_t)RESU8_TO_X16(res,14);
    }else if(type == APP_LPM_DEV_PARAM){
        cmd.cmd.u16 = HC_GET_LOW_POWER_DEV_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LOW_POWER_DEV_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3]  = (int32_t)(uint8_t)RESU8_TO_X8(res,3);
#ifdef CONFIG_ACC_U2DH
        param[4]  = (int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5]  = (int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6]  = (int32_t)(uint8_t)RESU8_TO_X8(res,6);
        param[7]  = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
#endif
    }else if(type == APP_RIDE_PEDO_PARAM){
        cmd.cmd.u16 = HC_GET_RIDE_PEDO_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_RIDE_PEDO_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
    }else if(type == APP_RIDE_PEDO2_PARAM){
        cmd.cmd.u16 = HC_GET_RIDE_PEDO2_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_RIDE_PEDO2_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
    }else if(type == APP_LPM_DEV_PARAM2){
        cmd.cmd.u16 = HC_GET_LOW_POWER_DEV_PARAM2;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LOW_POWER_DEV_PARAM2 err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0]  = (int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1]  = (int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2]  = (int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3]  = (int32_t)(uint16_t)RESU8_TO_X16(res,4);
    }else if(type == APP_TOTAL_STATUS_DETECTION2){
        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] = (int32_t)(uint32_t)RESU8_TO_X8(res,0);
        param[1] = (int32_t)(uint16_t)RESU8_TO_X16(res,1);
        param[2] = (int32_t)(uint16_t)RESU8_TO_X16(res,3);
        param[3] = (int32_t)(uint16_t)RESU8_TO_X16(res,5);
        param[4] = (int32_t)(uint8_t)RESU8_TO_X8(res,7);
        param[5] = (int32_t)(uint8_t)RESU8_TO_X8(res,8);
        param[6] = (int32_t)(uint8_t)RESU8_TO_X8(res,9);
        param[7] = (int32_t)(uint8_t)RESU8_TO_X8(res,10);
        param[8] = (int32_t)(uint16_t)RESU8_TO_X16(res,11);
        param[9] = (int32_t)(uint8_t)RESU8_TO_X8(res,13);
        param[10] = (int32_t)(uint8_t)RESU8_TO_X8(res,14);
    }else{
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_clear_data_app_exec(int32_t type)
{
    HostCmd cmd;
    HostCmdRes res;
    uint8_t cmd_len=0;
    int32_t ret;

    if(type == APP_PEDOMETER){
        cmd.cmd.u16 = HC_CLR_PEDO2_STEP_DATA;
    }else if(type == APP_RUN_DETECTION){
        cmd.cmd.u16 = HC_CLR_PEDO2_RUN_DETECT_DATA;
    }else if(type == APP_VEICHLE_DETECTION){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_DETECT_DATA;
    }else if(type == APP_TOTAL_STATUS_DETECTION_CLR_CONT_STEPS){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_TOTAL_DETECT_CONT;
        cmd_len=1;
        cmd.prm.u8[0]= 0x01;
    }else if(type == APP_TOTAL_STATUS_DETECTION_CLR_CONT_STOP){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_TOTAL_DETECT_CONT;
        cmd_len=1;
        cmd.prm.u8[0]= 0x02;
    }else if(type == APP_CLEAR_PAUSE_PARAM){
        cmd.cmd.u16 = HC_CLR_ACTIVITY2_PAUSE_PARAM;
    }else{
        return SHUB_RC_ERR;
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, cmd_len);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_CLR_XXX err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_init_app_exec(int32_t type)
{
    HostCmd cmd;
    HostCmdRes res;
    uint8_t cmd_len=0;
    int32_t ret;

    if(type == APP_CLEAR_PEDOM_AND_TOTAL_STATUS_DETECTION){
        cmd.cmd.u16 = HC_INIT_PEDO_AND_ACTIVITY_DETECT;
    }else{
        return SHUB_RC_ERR;
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, cmd_len);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_INIT_XXX err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_get_data_app_exec(int32_t arg_iType, int32_t *param)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;

    if(arg_iType == APP_PEDOMETER){
        cmd.cmd.u16 = HC_GET_PEDO2_STEP_DATA;
        cmd.prm.u32[0x00] = 0x0000ffff;
        cmd.prm.u32[0x01] = 0x01ffffff;
        cmd.prm.u8[0x08] = 0x00;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO2_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint32_t)RESU8_TO_X32(res,0);
        param[1] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[3] =(int32_t)(uint32_t)RESU8_TO_X32(res,10);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,14);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,15);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,17);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,19);
        param[8] =(int32_t)(uint32_t)RESU8_TO_X32(res,21);
        param[9] =(int32_t)(uint32_t)RESU8_TO_X32(res,25);
        param[10] =(int32_t)(uint32_t)RESU8_TO_X32(res,29);
        param[11] =(int32_t)(uint16_t)RESU8_TO_X16(res,33);
        param[12] =(int32_t)(uint32_t)RESU8_TO_X32(res,35);
        param[13] =(int32_t)(uint32_t)RESU8_TO_X32(res,39);
        param[14] =(int32_t)(uint32_t)RESU8_TO_X32(res,43);
        param[15] =(int32_t)(uint8_t)RESU8_TO_X8(res,47);
        param[16] =(int32_t)(uint32_t)RESU8_TO_X32(res,48);
        param[17] =(int32_t)(uint32_t)RESU8_TO_X32(res,52);
        param[18] =(int32_t)(uint32_t)RESU8_TO_X32(res,56);
        param[19] =(int32_t)(uint32_t)RESU8_TO_X32(res,60);
        param[20] =(int32_t)(uint32_t)RESU8_TO_X32(res,64);
        param[21] =(int32_t)(uint32_t)RESU8_TO_X32(res,68);
        param[22] =(int32_t)(uint32_t)RESU8_TO_X32(res,72);
        param[23] =(int32_t)(uint32_t)RESU8_TO_X32(res,76);
        param[24] =(int32_t)(uint32_t)RESU8_TO_X32(res,80);
        param[25] =(int32_t)(uint32_t)RESU8_TO_X32(res,84);
        param[26] =(int32_t)(uint32_t)RESU8_TO_X32(res,88);
        param[27] =(int32_t)(uint32_t)RESU8_TO_X32(res,92);
        param[28] =(int32_t)(uint32_t)RESU8_TO_X32(res,96);
        param[29] =(int32_t)(uint16_t)RESU8_TO_X16(res,100);
        param[30] =(int32_t)(uint16_t)RESU8_TO_X16(res,102);
        param[31] =(int32_t)(uint16_t)RESU8_TO_X16(res,104);
        param[32] =(int32_t)(uint16_t)RESU8_TO_X16(res,106);
        param[33] =(int32_t)(uint16_t)RESU8_TO_X16(res,108);
        param[34] =(int32_t)(uint16_t)RESU8_TO_X16(res,110);
        param[35] =(int32_t)(uint32_t)RESU8_TO_X32(res,112);
        param[36] =(int32_t)(uint32_t)RESU8_TO_X32(res,116);
        param[37] =(int32_t)(uint32_t)RESU8_TO_X32(res,120);
        param[38] =(int32_t)(uint32_t)RESU8_TO_X32(res,124);
        param[39] =(int32_t)(uint32_t)RESU8_TO_X32(res,128);
        param[40] =(int32_t)(uint32_t)RESU8_TO_X32(res,132);

    }else if(arg_iType == APP_NORMAL_PEDOMETER){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0x00] = 0x0000ffff;
        cmd.prm.u32[0x01] = 0x01ffffff;
        cmd.prm.u8[0x08] = 0x00;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint32_t)RESU8_TO_X32(res,0);
        param[1] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,8);
        param[3] =(int32_t)(uint32_t)RESU8_TO_X32(res,10);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,14);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,15);
        param[6] =(int32_t)(uint16_t)RESU8_TO_X16(res,17);
        param[7] =(int32_t)(uint16_t)RESU8_TO_X16(res,19);
        param[8] =(int32_t)(uint32_t)RESU8_TO_X32(res,21);
        param[9] =(int32_t)(uint32_t)RESU8_TO_X32(res,25);
        param[10] =(int32_t)(uint32_t)RESU8_TO_X32(res,29);
        param[11] =(int32_t)(uint16_t)RESU8_TO_X16(res,33);
        param[12] =(int32_t)(uint32_t)RESU8_TO_X32(res,35);
        param[13] =(int32_t)(uint32_t)RESU8_TO_X32(res,39);
        param[14] =(int32_t)(uint32_t)RESU8_TO_X32(res,43);
        param[15] =(int32_t)(uint8_t)RESU8_TO_X8(res,47);
        param[16] =(int32_t)(uint32_t)RESU8_TO_X32(res,48);
        param[17] =(int32_t)(uint32_t)RESU8_TO_X32(res,52);
        param[18] =(int32_t)(uint32_t)RESU8_TO_X32(res,56);
        param[19] =(int32_t)(uint32_t)RESU8_TO_X32(res,60);
        param[20] =(int32_t)(uint32_t)RESU8_TO_X32(res,64);
        param[21] =(int32_t)(uint32_t)RESU8_TO_X32(res,68);
        param[22] =(int32_t)(uint32_t)RESU8_TO_X32(res,72);
        param[23] =(int32_t)(uint32_t)RESU8_TO_X32(res,76);
        param[24] =(int32_t)(uint32_t)RESU8_TO_X32(res,80);
        param[25] =(int32_t)(uint32_t)RESU8_TO_X32(res,84);
        param[26] =(int32_t)(uint32_t)RESU8_TO_X32(res,88);
        param[27] =(int32_t)(uint32_t)RESU8_TO_X32(res,92);
        param[28] =(int32_t)(uint32_t)RESU8_TO_X32(res,96);
        param[29] =(int32_t)(uint16_t)RESU8_TO_X16(res,100);
        param[30] =(int32_t)(uint16_t)RESU8_TO_X16(res,102);
        param[31] =(int32_t)(uint16_t)RESU8_TO_X16(res,104);
        param[32] =(int32_t)(uint16_t)RESU8_TO_X16(res,106);
        param[33] =(int32_t)(uint16_t)RESU8_TO_X16(res,108);
        param[34] =(int32_t)(uint16_t)RESU8_TO_X16(res,110);
        param[35] =(int32_t)(uint32_t)RESU8_TO_X32(res,112);
        param[36] =(int32_t)(uint32_t)RESU8_TO_X32(res,116);
        param[37] =(int32_t)(uint32_t)RESU8_TO_X32(res,120);
        param[38] =(int32_t)(uint32_t)RESU8_TO_X32(res,124);
        param[39] =(int32_t)(uint32_t)RESU8_TO_X32(res,128);
        param[40] =(int32_t)(uint32_t)RESU8_TO_X32(res,132);
    }else if(arg_iType == APP_RUN_DETECTION){
        cmd.cmd.u16 = HC_GET_PEDO2_RUN_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_RUN_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint32_t)RESU8_TO_X8(res,0);
    }else if(arg_iType == APP_VEICHLE_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
        param[1] =(int32_t)(uint8_t)RESU8_TO_X8(res,1);
        param[2] =(int32_t)(uint8_t)RESU8_TO_X8(res,2);
        param[3] =(int32_t)(uint8_t)RESU8_TO_X8(res,3);
        param[4] =(int32_t)(uint8_t)RESU8_TO_X8(res,4);
        param[5] =(int32_t)(uint8_t)RESU8_TO_X8(res,5);
        param[6] =(int32_t)(uint8_t)RESU8_TO_X8(res,6);
    }else if(arg_iType == APP_TOTAL_STATUS_DETECTION){
        cmd.cmd.u16 = HC_GET_ACTIVITY2_TOTAL_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);

        cmd.cmd.u16 = HC_GET_ACTIVITY2_TOTAL_DETECT_INFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_INFO err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[1] =(int32_t)(uint16_t)RESU8_TO_X16(res,0);
        param[2] =(int32_t)(uint16_t)RESU8_TO_X16(res,2);
        param[3] =(int32_t)(uint32_t)RESU8_TO_X32(res,4);
        param[4] =(int32_t)(uint32_t)RESU8_TO_X32(res,8);
        param[5] =(int32_t)(uint16_t)RESU8_TO_X16(res,12);
    }else if(arg_iType == APP_MOTDTECTION){
        cmd.cmd.u16 = HC_GET_MOTDETECTION_SENS;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_MOTDETECTION_SENS err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(int16_t)RESU8_TO_X16(res,0);
        param[1] =(int32_t)(int16_t)RESU8_TO_X16(res,2);
        param[2] =(int32_t)(int16_t)RESU8_TO_X16(res,4);

    }else if(arg_iType == APP_LOW_POWER){
        cmd.cmd.u16 = HC_GET_LPM_INFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_LPM_INFO err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        param[0] =(int32_t)(uint8_t)RESU8_TO_X8(res,0);
    }else{
        return SHUB_RC_ERR;
    }

    return SHUB_RC_OK;
}

static int32_t shub_activate_pedom_exec(int32_t arg_iSensType, int32_t arg_iEnable)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentEnable =iCurrentSensorEnable|iCurrentLoggingEnable;
    int32_t iCurrentEnable_old =iCurrentSensorEnable|iCurrentLoggingEnable;

    bool setpCounterOn2OffFlg;
    bool setpCounterOff2OnFlg;
    bool setpCounterBatchOn2OffFlg;
    bool setpCounterBatchOff2OnFlg;

    if((arg_iSensType & (STEPCOUNT_GROUP_MASK|STEPDETECT_GROUP_MASK)) == 0){
        return SHUB_RC_OK;
    }

    if(arg_iEnable == POWER_ENABLE){
        iCurrentEnable |= arg_iSensType;
    }else{
        iCurrentEnable &= ~arg_iSensType;
    }

    setpCounterOn2OffFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM) != 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM) != 0 && 
        (arg_iEnable == POWER_DISABLE);

    setpCounterBatchOn2OffFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM_NO_NOTIFY) != 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM_NO_NOTIFY) != 0 && 
        (arg_iEnable == POWER_DISABLE);

    setpCounterOff2OnFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM) == 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM) != 0 && 
        (arg_iEnable == POWER_ENABLE);

    setpCounterBatchOff2OnFlg = (iCurrentEnable_old & SHUB_ACTIVE_PEDOM_NO_NOTIFY) == 0 && 
        (arg_iSensType & SHUB_ACTIVE_PEDOM_NO_NOTIFY) != 0 && 
        (arg_iEnable == POWER_ENABLE);


    //StepCounter ON->OFF 
    if(setpCounterOn2OffFlg || setpCounterBatchOn2OffFlg) {
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0] = 1;
        cmd.prm.u32[1] = 0;
        cmd.prm.u8[0x08] = 0x00;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        //Update offset
        s_tLatestStepCountData.stepDis = res.res.u32[0];
    }

    //StepCounter OFF->ON 
    if( setpCounterOff2OnFlg || setpCounterBatchOff2OnFlg ){
        cmd.cmd.u16 = HC_GET_PEDO_STEP_DATA;
        cmd.prm.u32[0] = 1;
        cmd.prm.u32[1] = 0;
        cmd.prm.u8[0x08] = 0x00;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 9);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        //Update offset
        s_tLatestStepCountData.stepOffset += (res.res.u32[0]-s_tLatestStepCountData.stepDis);
    }

    /* Enable/Disable */
    cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM);

    /* CountStep */
    if((iCurrentEnable & (STEPCOUNT_GROUP_MASK |
                    STEPDETECT_GROUP_MASK |
                    SHUB_ACTIVE_SIGNIFICANT)) != 0){
        cmd.prm.u8[0] = 1;
    }else{
        cmd.prm.u8[0] = 0;
    }

    /* NotifyStep */
    if(iCurrentEnable & (SHUB_ACTIVE_PEDODEC | SHUB_ACTIVE_PEDOM)){
        cmd.prm.u8[3] = 1;
        s_enable_notify_step = true;
    }else{
        cmd.prm.u8[3] = 0;
        s_enable_notify_step = false;
    }

    cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
    DBG(DBG_LV_INFO, "Enable PEDO en=%d onstep=%d\n" ,cmd.prm.u8[0],cmd.prm.u8[3]);
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable);
#ifdef CONFIG_SHTERM
    if(iCurrentEnable != iCurrentEnable_old){
        shterm_k_set_info(SHTERM_INFO_PEDOMETER, (cmd.prm.u8[0] || cmd.prm.u8[3] )? 1 : 0 );
    }
#endif /* CONFIG_SHTERM */
    return SHUB_RC_OK;
}

static int32_t shub_activate_significant_exec(int32_t arg_iSensType, int32_t arg_iEnable, uint8_t * notify)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentEnable_old =iCurrentSensorEnable;

    if((arg_iSensType & SHUB_ACTIVE_SIGNIFICANT) == 0){
        return SHUB_RC_OK;
    }

    if(arg_iEnable == POWER_ENABLE){
        iCurrentSensorEnable |= SHUB_ACTIVE_SIGNIFICANT;
    }else{
        iCurrentSensorEnable &= ~SHUB_ACTIVE_SIGNIFICANT;
    }

    /* Disable Step*/
    cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM);

    cmd.prm.u8[0] = 0;
    cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    /* Enable */
    if((iCurrentEnable_old & SHUB_ACTIVE_SIGNIFICANT) == 0 && 
            (arg_iSensType & SHUB_ACTIVE_SIGNIFICANT) != 0 && 
            (arg_iEnable == POWER_ENABLE)){
        /* Enable */
        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, 15);
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0xf5;
        cmd.prm.u8[2] = 0x03;
        cmd.prm.u8[3] = 21;
        cmd.prm.u8[4] = 0;

        cmd.cmd.u16 = HC_SET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_DATA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_DATA err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        //DBG(DBG_LV_ERROR, "status(%x)\n", res.res.u8[0]);
        if(res.res.u8[0] != 0){
            if(((iCurrentLoggingEnable | iCurrentEnable_old) & PEDOM_GROUP_MASK) != 0){
                *notify=1;
            }
        }
        //DBG(DBG_LV_ERROR, "notify(%x)\n", *notify);

        if(*notify == 0 || res.res.u8[0] == 0){
            /* Clear Run Detection */
            cmd.cmd.u16 = HC_CLR_PEDO_RUN_DETECT_DATA;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_CLR_PEDO_RUN_DETECT_DATA err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }

            /* Clear Transport Detection */
            cmd.cmd.u16 = HC_CLR_ACTIVITY_DETECT_DATA;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_CLR_ACTIVITY_DETECT_DATA err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
    }

    /* Disable */
    if((iCurrentEnable_old & SHUB_ACTIVE_SIGNIFICANT) != 0 && 
            (arg_iSensType & SHUB_ACTIVE_SIGNIFICANT) != 0 && 
            (arg_iEnable == POWER_DISABLE)){
        /* Disable */
        cmd.cmd.u16 = HC_GET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, 15);
        cmd.prm.u8[0] = 0;

        cmd.cmd.u16 = HC_SET_ACTIVITY_TOTAL_DETECT_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 15);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_ACTIVITY_TOTAL_DETECT_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }

    /* Enable Step*/
    cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM);

    if(((iCurrentSensorEnable | iCurrentLoggingEnable) & (STEPCOUNT_GROUP_MASK |
                    STEPDETECT_GROUP_MASK |
                    SHUB_ACTIVE_SIGNIFICANT)) != 0){
        cmd.prm.u8[0] = 1;
    }else{
        cmd.prm.u8[0] = 0;
    }

    cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
        return SHUB_RC_ERR;
    }

    atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable);
#ifdef CONFIG_SHTERM
    if(iCurrentSensorEnable != iCurrentEnable_old){
        shterm_k_set_info(SHTERM_INFO_PEDOMETER, (cmd.prm.u8[0])? 1 : 0 );
    }
#endif /* CONFIG_SHTERM */
    return SHUB_RC_OK;
}

#ifdef SHUB_SUSPEND
static int32_t shub_suspend_sensor_exec(void)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    uint8_t iEnableSensor = 0;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    uint32_t enable_sensor;

    enable_sensor=iCurrentSensorEnable | iCurrentLoggingEnable;

    //check enable logging
    if(iCurrentLoggingEnable & (FUSION_GROUP_ACC_MASK | SHUB_ACTIVE_ACC | APPTASK_GROUP_ACC_MASK)){
        iEnableSensor |= HC_ACC_VALID;
    }

    //check enable sensor measure 
    //(stepcounter stepdetector singnificant motiondec gdetect extpedom)
    if(iCurrentSensorEnable & (APPTASK_GROUP_ACC_MASK | SHUB_ACTIVE_GDEC)){
        iEnableSensor |= HC_ACC_VALID;
    }

    if(iCurrentSensorEnable & SHUB_ACTIVE_TWIST){
        iEnableSensor |= HC_MAG_VALID;
        iEnableSensor |= HC_GYRO_VALID;
    }

    //check enable logging mag 
    if(iCurrentLoggingEnable & ACTIVE_MAG_GROUP_MASK){
        iEnableSensor |= HC_MAG_VALID;
    }

    //check enable logging gyro 
    if(iCurrentLoggingEnable & ACTIVE_GYRO_GROUP_MASK){
        iEnableSensor |= HC_GYRO_VALID;
    }

#ifdef CONFIG_BARO_SENSOR
    //check enable logging baro
    if(iCurrentLoggingEnable & BARO_GROUP_MASK){
        iEnableSensor |= HC_BARO_VALID;
    }
#endif

    /***** Set sensor *****/
    cmd.cmd.u16 = HC_SENSOR_SET_PARAM;
    cmd.prm.u8[0] = iEnableSensor & s_exist_sensor;
    cmd.prm.u8[1] = 0;
    if(s_micon_param.sensors != cmd.prm.u8[0]){ 
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_SET_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
    s_micon_param.sensors = cmd.prm.u8[0];

    /***** Set Task *****/
    cmd.cmd.u16 = HC_TSK_EXECUTE;
    cmd.prm.u8[0] =  (iEnableSensor != 0)?1:0;
    //check stepcounter stepdetector singnificant motiondec extpedom 
    cmd.prm.u8[1] = ((enable_sensor & APPTASK_GROUP_MASK) != 0)?1:0;
    // logging only
    cmd.prm.u8[2] = ((iCurrentLoggingEnable & ACTIVE_MAG_GROUP_MASK) != 0)?1:0;
    if(s_micon_param.task[0] != cmd.prm.u8[0] ||
            s_micon_param.task[1] != cmd.prm.u8[1] || 
            s_micon_param.task[2] != cmd.prm.u8[2] ){
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TSK_EXECUTE err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
    s_micon_param.task[0] = cmd.prm.u8[0];
    s_micon_param.task[1] = cmd.prm.u8[1];
    s_micon_param.task[2] = cmd.prm.u8[2];

    return SHUB_RC_OK;
}
#endif

static int32_t shub_activate_exec(int32_t arg_iSensType, int32_t arg_iEnable, int mode)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    uint8_t iEnableSensor = 0;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
    int32_t iCurrentSensorEnable_tmp = iCurrentSensorEnable;
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    uint32_t enable_sensor;
    uint32_t enable_sensor_acc;
    uint8_t setCal = 0;
    uint32_t enableGyro = 0;
    uint32_t waitFusion = 0;
    int32_t lpParam[16];
    uint8_t newDeepSleepMode = 0;
    uint8_t newAccMode = 0;

    DBG(DBG_LV_INFO, "####%s %s[%x] en=%d [logging=%x] [sensor=%x]\n",
            __FUNCTION__,
            shub_get_active_sensor_name(arg_iSensType),
            arg_iSensType, 
            arg_iEnable,
            iCurrentLoggingEnable,
            iCurrentSensorEnable
       );

    if(arg_iEnable == POWER_ENABLE){
        iCurrentSensorEnable |= arg_iSensType;
    }else{
        iCurrentSensorEnable &= ~arg_iSensType;
    }
    enable_sensor=iCurrentSensorEnable | iCurrentLoggingEnable;

    enable_sensor_acc = enable_sensor & (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC);
    if(enable_sensor_acc == (SHUB_ACTIVE_ACC | SHUB_ACTIVE_SHEX_ACC)) {
        s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, s_sensor_delay_us.acc);
        s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, s_sensor_delay_us.acc);
    }
    else if(enable_sensor_acc == SHUB_ACTIVE_ACC) {
        if(arg_iSensType == SHUB_ACTIVE_SHEX_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, MEASURE_MAX_US);
        }
        else {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, s_sensor_delay_us.acc);
        }
    }
    else if(enable_sensor_acc == SHUB_ACTIVE_SHEX_ACC) {
        if(arg_iSensType == SHUB_ACTIVE_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, MEASURE_MAX_US);
        }
        else {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, s_sensor_delay_us.acc);
        }
    }
    else {
        s_sensor_delay_us.acc = MEASURE_MAX_US;
    }

    if(enable_sensor & ACTIVE_ACC_GROUP_MASK){
        iEnableSensor |= HC_ACC_VALID;
    }
    if(enable_sensor & ACTIVE_MAG_GROUP_MASK){
        //gyro calibrataion needs magnetic field
        iEnableSensor |= HC_MAG_VALID;
    }
    if(enable_sensor & ACTIVE_GYRO_GROUP_MASK){
        iEnableSensor |= HC_GYRO_VALID;
    }
#ifdef CONFIG_BARO_SENSOR
    if(enable_sensor & BARO_GROUP_MASK){
        iEnableSensor |= HC_BARO_VALID;
    }
#endif

    if(iCurrentSensorEnable & ACTIVE_MAG_GROUP_MASK){
        setCal |= HC_MAG_VALID;
    }
    if(iCurrentSensorEnable & ACTIVE_GYRO_GROUP_MASK){
        setCal |= HC_GYRO_VALID;
    }
#ifdef CONFIG_BARO_SENSOR
    if(iCurrentSensorEnable & BARO_GROUP_MASK){
        setCal |= HC_BARO_VALID;
    }
#endif

    if(arg_iEnable == POWER_ENABLE && iEnableSensor == 0){
        /*none*/
        DBG(DBG_LV_ERROR, "error sensor measure_normal_report unkonw sensor type[%x]\n", arg_iSensType);
        return SHUB_RC_ERR;
    }

    atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable);
    /* update period */
    ret = shub_set_delay_exec(0,0);
    if(ret != SHUB_RC_OK) {
        atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
        return SHUB_RC_ERR;
    }

    /***** Set Mag calibration *****/
    if((s_exist_sensor & HC_MAG_VALID) != 0){
        cmd.cmd.u16 = HC_MAG_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] =(setCal & HC_MAG_VALID) != 0 ? 1 : 0; 
        if(s_micon_param.mag_cal != cmd.prm.u8[1]){
            DBG(DBG_LV_INFO, "notify calib MAG=%d ", cmd.prm.u8[1]);
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_MAG_SET_CAL err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
        }
        s_micon_param.mag_cal = cmd.prm.u8[1];
    }

    /***** Set Gyro calibration *****/
    if((s_exist_sensor & HC_GYRO_VALID) != 0){
        cmd.cmd.u16 = HC_GYRO_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = (setCal & HC_GYRO_VALID) != 0 ? 1 : 0;
        if(s_micon_param.gyro_cal != cmd.prm.u8[1] ){
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
            DBG(DBG_LV_INFO, "GYRO=%d\n", cmd.prm.u8[1]);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_GYRO_SET_CAL err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
        }
        s_micon_param.gyro_cal = cmd.prm.u8[1] ;
    }
#ifdef CONFIG_BARO_SENSOR
    if((enable_sensor & SHUB_ACTIVE_BARO) != 0 && (iCurrentSensorEnable_tmp & SHUB_ACTIVE_BARO) == 0) {
        memset(&s_tLatestBaroData, 0x00 , sizeof(s_tLatestBaroData));
    }
#endif

#ifdef CONFIG_BARO_SENSOR
    if ( enable_sensor &
        ( SHUB_ACTIVE_ACC | GYRO_GROUP_MASK | MAG_GROUP_MASK | SHUB_ACTIVE_BARO | SHUB_ACTIVE_RV_NONGYRO | SHUB_ACTIVE_RV_NONMAG | FUSION_9AXIS_GROUP_MASK ) ||
        ((enable_sensor & SHUB_ACTIVE_MOTIONDEC) && shub_mot_still_enable_flag) ) {
#else
    if ( enable_sensor &
        ( SHUB_ACTIVE_ACC | GYRO_GROUP_MASK | MAG_GROUP_MASK | SHUB_ACTIVE_RV_NONGYRO | SHUB_ACTIVE_RV_NONMAG | FUSION_9AXIS_GROUP_MASK ) ||
        ((enable_sensor & SHUB_ACTIVE_MOTIONDEC) && shub_mot_still_enable_flag) ) {
#endif
        newAccMode = 0;
        newDeepSleepMode = 0;
    } else {
        newAccMode = 1;
        newDeepSleepMode = 1;
    }
    
    /***** Set DeepSleep Disable *****/
    if(newDeepSleepMode == 0){
        memset(lpParam, 0, sizeof(lpParam));
        ret = shub_get_param_exec(APP_LPM_PARAM, lpParam);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "HC_GET_LPM_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        if(lpParam[0] != newDeepSleepMode){
            lpParam[0] = newDeepSleepMode;
            ret = shub_set_param_exec(APP_LPM_PARAM, lpParam);
            if(SHUB_RC_OK != ret){
                DBG(DBG_LV_ERROR, "HC_SET_LPM_PARAM err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
            cmd.cmd.u16 = HC_CLR_LOW_POWER_MODE;
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                DBG(DBG_LV_ERROR, "HC_CLR_LOW_POWER_MODE err(%x)\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
    }
    /***** Set Freerun *****/
    cmd.cmd.u16 = HC_ACC_GET_OPERATION_MODE;
    cmd.prm.u8[0] = 0;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if(SHUB_RC_OK != ret){
        DBG(DBG_LV_ERROR, "HC_ACC_GET_OPERATION_MODE err(%x)\n", res.err.u16);
        atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
        return SHUB_RC_ERR;
    }
    if(res.res.u8[0] != newAccMode){
        cmd.cmd.u16 = HC_ACC_SET_OPERATION_MODE;
        cmd.prm.u8[0] = newAccMode;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_ACC_SET_OPERATION_MODE err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        shub_operation_mode = newAccMode;
    }

    /***** Set sensor *****/
    cmd.cmd.u16 = HC_SENSOR_SET_PARAM;
    cmd.prm.u8[0] = iEnableSensor & s_exist_sensor;
    cmd.prm.u8[1] = 0;
    if(s_micon_param.sensors != cmd.prm.u8[0]){ 
        DBG(DBG_LV_INFO, "Enable Sens=%x\n" ,cmd.prm.u8[0]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SENSOR_SET_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        if(((s_micon_param.sensors & HC_GYRO_VALID) == 0) && 
                ((cmd.prm.u8[0] & HC_GYRO_VALID) == HC_GYRO_VALID)){
            //Gyro Off->On
            enableGyro = 1;
        }
    }
    s_micon_param.sensors = cmd.prm.u8[0];

    {
        uint32_t using_gyro = ACTIVE_GYRO_GROUP_MASK;
        bool gyroSensorOnFlg = 
            ((arg_iSensType & using_gyro) != 0)
            &&
            (arg_iEnable == POWER_ENABLE);

        bool gyroSensorEnabledFlg =
            ((iCurrentLoggingEnable | iCurrentSensorEnable_tmp) & using_gyro) == 0;

        if(gyroSensorOnFlg && gyroSensorEnabledFlg){
            DBG(DBG_LV_INFO,  "gyroscope 130ms wait(activate_exec)\n");
            usleep(130 * 1000);//gyroscope 130ms wait
        }
        else if((mode == 1) && (enableGyro == 1)){
            DBG(DBG_LV_INFO,  "gyroscope 130ms wait resume(activate_exec)\n");
            usleep(130 * 1000); // gyroscope 130ms wait
        }
    }

    /***** Set Fusion *****/
    cmd.cmd.u16 = HC_SET_FUISON_PARAM;
    //  cmd.prm.u8[0] = ((enable_sensor & FUSION_GROUP_MASK) != 0)?1:0;
    cmd.prm.u8[0] = 0;
    if((enable_sensor & SHUB_ACTIVE_ORI ) != 0) {
        cmd.prm.u8[0] |= HC_ORI_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_GRAVITY) != 0){
        cmd.prm.u8[0] |= HC_GRAVITY_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_LACC) != 0){
        cmd.prm.u8[0] |= HC_LACC_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_RV) != 0){
        cmd.prm.u8[0] |= HC_RV_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_RV_NONMAG) != 0){
        cmd.prm.u8[0] |= HC_RV_NONMAG_VALID;
    }
    if((enable_sensor & SHUB_ACTIVE_RV_NONGYRO) != 0){
        cmd.prm.u8[0] |= HC_RV_NONGYRO_VALID;
    }
    if(s_micon_param.fusion != cmd.prm.u8[0]){  
        DBG(DBG_LV_INFO, "Enable fusion=%x\n",cmd.prm.u8[0]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_FUISON_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }

        //if(cmd.prm.u8[0]== 1){
        if((cmd.prm.u8[0] | s_micon_param.fusion) != s_micon_param.fusion){
            waitFusion = 1;
        }
    }

    //if((s_micon_param.fusion == 1) && (cmd.prm.u8[0] == 1) && (enableGyro == 1)){
    //    //Change Fusion mode
    //    //Enable 9-Axis
    //    if(((arg_iSensType & GYRO_GROUP_MASK) == 0) && (arg_iEnable == POWER_ENABLE)){
    //        waitFusion = 1;
    //    }
    //}
    s_micon_param.fusion = cmd.prm.u8[0];

    /***** Set Task *****/
    cmd.cmd.u16 = HC_TSK_EXECUTE;
    cmd.prm.u8[0] =  (enable_sensor != 0)?1:0;
    cmd.prm.u8[1] = ((enable_sensor & APPTASK_GROUP_MASK) != 0)?1:0;
    cmd.prm.u8[2] = ((enable_sensor & ACTIVE_MAG_GROUP_MASK) != 0)?1:0;
    if(s_micon_param.task[0] != cmd.prm.u8[0] ||
            s_micon_param.task[1] != cmd.prm.u8[1] || 
            s_micon_param.task[2] != cmd.prm.u8[2] ){
        DBG(DBG_LV_INFO, "Enable TASK sens=%x app=%x fusion=%x\n" ,cmd.prm.u8[0],cmd.prm.u8[1],cmd.prm.u8[2]);
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 3);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_TSK_EXECUTE err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
    }
    s_micon_param.task[0] = cmd.prm.u8[0];
    s_micon_param.task[1] = cmd.prm.u8[1];
    s_micon_param.task[2] = cmd.prm.u8[2];

    if(waitFusion == 1){
        usleep(80 * 1000);//Fusion 80ms wait
        DBG(DBG_LV_INFO,  "Fusion 80ms wait\n");
    }

    /***** Set DeepSleep Enable *****/
    if(newDeepSleepMode == 1){
        memset(lpParam, 0, sizeof(lpParam));
        ret = shub_get_param_exec(APP_LPM_PARAM, lpParam);
        if(SHUB_RC_OK != ret){
            DBG(DBG_LV_ERROR, "HC_GET_LPM_PARAM err(%x)\n", res.err.u16);
            atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
            return SHUB_RC_ERR;
        }
        if(lpParam[0] != newDeepSleepMode){
            lpParam[0] = newDeepSleepMode;
            ret = shub_set_param_exec(APP_LPM_PARAM, lpParam);
            if(SHUB_RC_OK != ret) {
                DBG(DBG_LV_ERROR, "HC_SET_LPM_PARAM err(%x)\n", res.err.u16);
                atomic_set(&g_CurrentSensorEnable, iCurrentSensorEnable_tmp);
                return SHUB_RC_ERR;
            }
        }
    }

#ifdef CONFIG_SHTERM
    if(enable_sensor & ACTIVE_ACC_GROUP_MASK){
        shterm_k_set_info(SHTERM_INFO_ACCELE, 1);
    }else{
        shterm_k_set_info(SHTERM_INFO_ACCELE, 0);
    }

    if(enable_sensor & ACTIVE_MAG_GROUP_MASK){
        shterm_k_set_info(SHTERM_INFO_COMPS, 1);
    }else{
        shterm_k_set_info(SHTERM_INFO_COMPS, 0);
    }

    if(enable_sensor & ACTIVE_GYRO_GROUP_MASK){
        shterm_k_set_info(SHTERM_INFO_GYRO, 1);
    }else{
        shterm_k_set_info(SHTERM_INFO_GYRO, 0);
    }
#endif /* CONFIG_SHTERM */
    return SHUB_RC_OK;
}

static int32_t shub_activate_logging_exec(int32_t arg_iSensType, int32_t arg_iEnable)
{
    int32_t ret;
    int32_t set_time_flag = 0;
    HostCmd cmd;
    HostCmdRes res;
    uint16_t hcSensor;
    uint16_t hcFusion;
    int32_t iCurrentLoggingEnable = atomic_read(&g_CurrentLoggingSensorEnable);
    int32_t iCurrentLoggingEnable_tmp = iCurrentLoggingEnable;
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);

    DBG(DBG_LV_INFO, "####%s %s[%x] en=%d [logging=%x]\n",
            __FUNCTION__,
            shub_get_active_sensor_name(arg_iSensType),
            arg_iSensType, 
            arg_iEnable,
            iCurrentLoggingEnable
       );

    if(arg_iEnable == POWER_ENABLE){
        iCurrentLoggingEnable |= arg_iSensType;
    }else{
        iCurrentLoggingEnable &= ~arg_iSensType;
    }

    hcSensor = 0;
    hcFusion = 0;
    if((iCurrentLoggingEnable & SHUB_ACTIVE_ACC) != 0){
        hcSensor |= HC_ACC_VALID;
    }
    if((iCurrentLoggingEnable & MAG_GROUP_MASK) != 0){
        hcSensor |= HC_MAG_VALID;
    }
    if((iCurrentLoggingEnable & GYRO_GROUP_MASK) != 0){
        hcSensor |= HC_GYRO_VALID;
    }
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentLoggingEnable & BARO_GROUP_MASK) != 0){
        hcSensor |= HC_BARO_VALID;
    }
#endif
    if((iCurrentLoggingEnable & SHUB_ACTIVE_ORI) != 0){
        hcFusion |= HC_ORI_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_GRAVITY) != 0){
        hcFusion |= HC_GRAVITY_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_LACC) != 0){
        hcFusion |= HC_LACC_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV) != 0){
        hcFusion |= HC_RV_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONMAG) != 0){
        hcFusion |= HC_RV_NONMAG_VALID;
    }
    if((iCurrentLoggingEnable & SHUB_ACTIVE_RV_NONGYRO) != 0){
        hcFusion |= HC_RV_NONGYRO_VALID;
    }

    atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable);

    ret = shub_activate_exec(0, 0, 0);//update sensor measure cycle
    if(SHUB_RC_OK != ret){
        DBG(DBG_LV_ERROR, "shub_activate_exec\n");
        atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
        return SHUB_RC_ERR;
    }
    
#ifdef CONFIG_BARO_SENSOR
    if((iCurrentLoggingEnable & SHUB_ACTIVE_BARO) != 0 && (iCurrentLoggingEnable_tmp & SHUB_ACTIVE_BARO) == 0) {
        memset(&s_tLoggingBaroData, 0x00 , sizeof(s_tLoggingBaroData));
    }
#endif

    cmd.cmd.u16 = HC_LOGGING_SENSOR_SET_PARAM;
    cmd.prm.u16[0] = hcSensor & s_exist_sensor;
    if(s_micon_param.logg_sensors != cmd.prm.u16[0]){ 
        DBG(DBG_LV_INFO, "Enable Logging Sens=%x " ,cmd.prm.u16[0]);
        shub_basetime_req = 1;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_SENSOR_SET_PARAM err(%x)\n", res.err.u16);
            shub_basetime_req = 0;
            atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
            return SHUB_RC_ERR;
        }
        set_time_flag = 1;
    }
    s_micon_param.logg_sensors = cmd.prm.u16[0];

    {
        uint32_t using_gyro = ACTIVE_GYRO_GROUP_MASK;
        bool gyroSensorOnFlg = 
            ((arg_iSensType & using_gyro) != 0)
            &&
            (arg_iEnable == POWER_ENABLE);

        bool gyroSensorEnabledFlg =
            ((iCurrentLoggingEnable_tmp | iCurrentSensorEnable) & using_gyro) == 0;

        if(gyroSensorOnFlg && gyroSensorEnabledFlg){
//          DBG(DBG_LV_INFO,  "gyroscope 130ms wait(logging_exec)\n");
//          usleep(130 * 1000);//gyroscope 130ms wait
            shub_set_gyroEnable_time(&shub_baseTime);
        }
    }

    cmd.cmd.u16 = HC_LOGGING_FUSION_SET_PARAM;
    cmd.prm.u16[0] = hcFusion;
    if(s_micon_param.logg_fusion != cmd.prm.u16[0]){ 
        DBG(DBG_LV_INFO, " Fusion=%x\n" ,cmd.prm.u16[0]);
        shub_basetime_req = 1;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_LOGGING_FUSION_SET_PARAM err(%x)\n", res.err.u16);
            shub_basetime_req = 0;
            atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
            return SHUB_RC_ERR;
        }
        set_time_flag = 1;
    }
    s_micon_param.logg_fusion = cmd.prm.u16[0];

    cmd.cmd.u16 = HC_LOGGING_GET_PEDO;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_GET_PEDO err(%x)\n", res.err.u16);
        update_base_time(ACTIVE_FUNC_MASK,NULL);
        atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
        return SHUB_RC_ERR;
    }
    memcpy(cmd.prm.u8, res.res.u8, 2);

    if((iCurrentLoggingEnable & PEDOM_GROUP_MASK )!= 0){
        cmd.prm.u16[0] |= HC_FLG_LOGGING_PEDO;
    }else{
        cmd.prm.u16[0] &= ~HC_FLG_LOGGING_PEDO;
    }
    cmd.cmd.u16 = HC_LOGGING_SET_PEDO;
    if(set_time_flag == 0){
        shub_basetime_req = 1;
    }
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_SET_PEDO err(%x)\n", res.err.u16);
        shub_basetime_req = 0;
        update_base_time(ACTIVE_FUNC_MASK,NULL);
        atomic_set(&g_CurrentLoggingSensorEnable, iCurrentLoggingEnable_tmp);
        return SHUB_RC_ERR;
    }

    if(arg_iEnable == POWER_ENABLE && 
            (iCurrentLoggingEnable_tmp & arg_iEnable) == 0 ){
//      pending_base_time(arg_iSensType);
        pending_base_time(arg_iSensType, &shub_baseTime);
        update_base_time(arg_iSensType,NULL);
    }

    logging_flush_exec();
    return SHUB_RC_OK;
}

#ifndef NO_LINUX
static int32_t shub_gpio_init(void)
{
    int32_t ret;

#ifndef SHUB_SW_GPIO_PMIC
#ifdef USE_RESET_SIGNAL   
    ret = shub_gpio_request(SHUB_GPIO_PIN_RESET);
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        return ret;
    }

#ifndef SHUB_SW_PINCTRL 
    ret = shub_gpio_direction_output(SHUB_GPIO_PIN_RESET, 1);
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        goto ERROR;
    }
#endif /* SHUB_SW_PINCTRL */
#endif
#endif /* SHUB_SW_GPIO_PMIC */

#ifdef USE_REMAP_SIGNAL
    ret = shub_gpio_request(SHUB_GPIO_PIN_BRMP);
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        return ret;
    }

#ifndef SHUB_SW_PINCTRL 
    ret = shub_gpio_direction_output(SHUB_GPIO_PIN_BRMP, 1);
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        goto ERROR;
    }
#endif
#endif

    g_nIntIrqNo = shub_gpio_to_irq(SHUB_GPIO_PIN_INT0);
    atomic_set(&g_bIsIntIrqEnable, true);
    ret = shub_gpio_request(SHUB_GPIO_PIN_INT0);
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_request ret=%d\n",ret);
        goto ERROR;
    }
    ret = shub_gpio_direction_input(SHUB_GPIO_PIN_INT0);
    if (ret < 0){
        DBG(DBG_LV_ERROR, "failed to gpio_direction_input ret=%d\n",ret);
        goto ERROR;
    }

    ret = request_any_context_irq(g_nIntIrqNo, shub_irq_handler, IRQF_TRIGGER_LOW, SHUB_GPIO_INT0_NAME, NULL);
    if(ret < 0) {
        DBG(DBG_LV_ERROR, "Failed request_any_context_irq. ret=%x\n", ret);
        goto ERROR;
    }

    return SHUB_RC_OK;

ERROR:
    shub_gpio_free(SHUB_GPIO_PIN_INT0);

#ifndef SHUB_SW_GPIO_PMIC
#ifdef USE_RESET_SIGNAL   
    shub_gpio_free(SHUB_GPIO_PIN_RESET);
#endif
#endif

#ifdef USE_REMAP_SIGNAL
    shub_gpio_free(SHUB_GPIO_PIN_BRMP);
#endif
    return -ENODEV;
}
#endif

static void shub_workqueue_init(void)
{
    int32_t i;

    for(i=0; i<ACC_WORK_QUEUE_NUM; i++){
        cancel_work_sync(&s_tAccWork[i].work);
        s_tAccWork[i].status = false;
    }
    s_nAccWorkCnt = 0;
}

static int32_t shub_workqueue_create( struct workqueue_struct *queue, void (*func)(struct work_struct *) )
{
    int32_t ret = SHUB_RC_ERR;
    unsigned long flags;

    if((queue == NULL) || (func == NULL)){
        return SHUB_RC_ERR;
    }

    spin_lock_irqsave(&acc_lock, flags);

    if(s_tAccWork[s_nAccWorkCnt].status == false){

        INIT_WORK( &s_tAccWork[s_nAccWorkCnt].work, func );

        ret = queue_work( queue, &s_tAccWork[s_nAccWorkCnt].work );

        if (ret == 1) {
            s_tAccWork[s_nAccWorkCnt].status = true;

            if(++s_nAccWorkCnt >= ACC_WORK_QUEUE_NUM){
                s_nAccWorkCnt = 0;
            }
            ret = SHUB_RC_OK;

        }else{
            DBG(DBG_LV_ERROR, "ACC %s[%d] queue_work Non Create(%d) \n",__FUNCTION__, __LINE__, ret);
        }

    }else{
        DBG(DBG_LV_ERROR, "ACC queue_work[%d] used!! \n", s_nAccWorkCnt);
    }

    spin_unlock_irqrestore(&acc_lock, flags);

    return ret;
}

static void shub_workqueue_delete(struct work_struct *work)
{
    int32_t i;
    unsigned long flags;

    spin_lock_irqsave(&acc_lock, flags);

    for(i=0; i<ACC_WORK_QUEUE_NUM; i++){

        if(&s_tAccWork[i].work == work){
            s_tAccWork[i].status = false;
            break;
        }
    }

    spin_unlock_irqrestore(&acc_lock, flags);
    return ;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// PUBLIC SYMBOL
//
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
static HostCmdRes direct_cmdres;

int32_t shub_direct_sendcmd(uint16_t cmd, const uint8_t *prm)
{
    int i;
    int32_t ret;
    HostCmd hcmd;

    if(prm == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i = 0; i < 9; i++){
        direct_cmdres.res.u8[i] = 0;
    }
    direct_cmdres.err.u16 = 0;
    
    hcmd.cmd.u16 = cmd;
    memcpy(hcmd.prm.u8, prm, 16);

    ret = shub_hostcmd(&hcmd, &direct_cmdres, EXE_HOST_ALL, 16);

    return ret;
}

int32_t shub_noreturn_sendcmd(uint16_t cmd, const uint8_t *prm)
{
    int i;
    int32_t ret;
    HostCmd hcmd;

    if(prm == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i = 0; i < 9; i++){
        direct_cmdres.res.u8[i] = 0;
    }
    direct_cmdres.err.u16 = 0;
    
    hcmd.cmd.u16 = cmd;
    memcpy(hcmd.prm.u8, prm, 16);

    ret = shub_hostcmd(&hcmd, &direct_cmdres, 0, 16);

    return ret;
}

int32_t shub_direct_get_error(uint16_t *error)
{
    if(error == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    *error = direct_cmdres.err.u16;
    return 0;
}

int32_t shub_direct_get_result(uint8_t *result)
{
    int i;

    if(result == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i = 0; i < 2; i++){
        result[i] = direct_cmdres.res.u8[i];
    }
    return 0;
}

int32_t shub_direct_multi_get_result(uint8_t *result)
{
    int i;

    if(result == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i = 0; i < 9; i++){
        result[i] = direct_cmdres.res.u8[i];
    }
    return 0;
}

int32_t shub_hostif_write(uint8_t adr, const uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }
    SHUB_DBG_SPIW(adr, data, size);
    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_write_proc(adr, data, size);
        if(ret == 0){
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "write EBUSY error(Retry:%d)\n", i);
            usleep(100 * 1000);

        }else{
            DBG(DBG_LV_ERROR, "write Other error (H/W Reset ON) \n");
            break;
        }
    }

    return ret;
}

int32_t shub_hostif_read(uint8_t adr, uint8_t *data, uint16_t size)
{
    int32_t i;
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i=0; i<ACC_SPI_RETRY_NUM; i++)
    {
        ret = hostif_read_proc(adr, data, size);
        if(ret == 0){
            SHUB_DBG_SPIR(adr, data, size);
            return 0;

        }else if(ret == -EBUSY){
            DBG(DBG_LV_ERROR, "read EBUSY error(Retry:%d)\n", i);
            usleep(100 * 1000);

        }else{
            DBG(DBG_LV_ERROR, "read Other error (H/W Reset ON) \n");
            break;
        }
    }

    return ret;
}

int32_t shub_cmd_wite(struct IoctlDiagCmdReq *arg_cmd)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t i;
    uint8_t flag = EXE_HOST_ALL;
    uint8_t err_intreq[4];
    
    if(arg_cmd == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }
    
    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));
    
    cmd.cmd.u16 = (uint16_t)arg_cmd->m_Cmd;
    for(i=0; i<arg_cmd->m_req_size; i++) {
        cmd.prm.u8[i] = arg_cmd->m_buf[i];
    }
    
    if(cmd.cmd.u16 == HC_MCU_ASSERT_INT){
        flag = 0;
        DISABLE_IRQ;
    }
    
    if((cmd.cmd.u16 == HC_MCU_DEASSERT_INT) || (cmd.cmd.u16 == HC_MCU_SOFT_RESET)){
        flag = 0;
    }
    
    ret = shub_hostcmd(&cmd, &res, flag, arg_cmd->m_req_size);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "error Command Write %x\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    
    if(cmd.cmd.u16 == HC_MCU_DEASSERT_INT){
        hostif_read(ERROR0, err_intreq, 4);
        ENABLE_IRQ;
    }
    
    return ret;
}

int32_t shub_cmd_read(struct IoctlDiagCmdReq *arg_cmd)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;
    int32_t i;
    
    if(arg_cmd == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }
    
    memset(&cmd, 0x00, sizeof(cmd));
    memset(&res, 0x00, sizeof(res));
    
    cmd.cmd.u16 = (uint16_t)arg_cmd->m_Cmd;
    for(i=0; i<arg_cmd->m_req_size; i++) {
        cmd.prm.u8[i] = arg_cmd->m_buf[i];
    }

    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, arg_cmd->m_req_size);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "error Command Read %x\n", res.err.u16);
        return SHUB_RC_ERR;
    }
    
    arg_cmd->m_res_size = res.res_size;
    
    for(i=0; i<res.res_size; i++) {
        arg_cmd->m_buf[i] = res.res.u8[i];
    }
    return ret;
}

static int32_t shub_int_enable(int enable)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];
    
    memset(prm, 0, sizeof(prm));
    
    if((enable != 0) && (enable != 1)){
        DBG(DBG_LV_ERROR, "%s error enable=%d\n", __func__, enable);
        return -1;
    }
    
    if(enable == 1){
        // GPIO66(PU -> PD)
        shub_gpio_tlmm_config(SHUB_GPIO_PIN_INT0, SHUB_GPIO_PULL_DOWN);
        
        prm[0] = 1;
        ret = shub_direct_sendcmd(HC_MCU_SET_INT1, prm);
        shub_direct_get_error(&errRes);
        if((ret != 0) || (errRes != 0)) {
            DBG(DBG_LV_ERROR, "%s(1) error ret=%d,errRes=%d\n", __func__, ret, errRes);
            return -1;
        }
    }else{
        // GPIO66(PD -> PU)
        shub_gpio_tlmm_config(SHUB_GPIO_PIN_INT0, SHUB_GPIO_PULL_UP);
        
        prm[0] = 0;
        ret = shub_direct_sendcmd(HC_MCU_SET_INT1, prm);
        shub_direct_get_error(&errRes);
        if((ret != 0) || (errRes != 0)) {
            printk("[shub]%s(0) error ret=%d,errRes=%d\n", __func__, ret, errRes);
            return -1;
        }
    }
    return ret;
}

#ifdef CONFIG_ACC_U2DH
static int32_t shub_lis2dh_low_power(void)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];
    
    memset(prm, 0, sizeof(prm));
    prm[0] = 0x00;
    prm[1] = 0x01;
    prm[2] = 0x1E;
    prm[3] = 0x90;
    ret = shub_direct_sendcmd(HC_MCU_ACCESS_SENSOR, prm);
    shub_direct_get_error(&errRes);
    if((ret != 0) || (errRes != 0)) {
        DBG(DBG_LV_ERROR, "%s error ret=%d,errRes=%d\n", __func__, ret, errRes);
        return -1;
    }

    return 0;
}
#endif

int32_t shub_get_int_state(int *state)
{
    int32_t ret;
    int32_t ret_val = 0;
    int gpio_state;
    uint8_t err_intreq[4];
    
    if(state == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    ret = shub_int_enable(1);
    if(ret != 0) {
        ret_val = -1;
    }
    
    usleep(1 * 1000);
    
#ifdef SHUB_SW_GPIO_INT1
    gpio_state = shub_gpio_get_value(SHUB_GPIO_PIN_INT0) | (shub_gpio_get_value(SHUB_GPIO_PIN_INT1) << 1);
#else
    gpio_state = shub_gpio_get_value(SHUB_GPIO_PIN_INT0) | 0x02;
#endif
    
    ret = shub_int_enable(0);
    if(ret != 0) {
        ret_val = -1;
    }
    
    hostif_read(ERROR0, err_intreq, 4);
    
    *state = gpio_state;
    
    return ret_val;
}

int32_t shub_init_common( void )
{
    uint8_t reg = 0xFF;
    int32_t cnt = 0;
    int32_t iWakeupSensor;
    Word sreg;

    DBG(DBG_LV_INFO, "%s : register_init\n", __FUNCTION__);

    DISABLE_IRQ;

    atomic_set(&g_CurrentSensorEnable,ACTIVE_OFF);
    atomic_set(&g_CurrentLoggingSensorEnable,ACTIVE_OFF);
    iWakeupSensor = atomic_read(&g_WakeupSensor);
    atomic_set(&g_WakeupSensor,iWakeupSensor & ~ACTIVE_FUNC_MASK);
    s_lsi_id = LSI_ML630Q790;
    memset(&s_micon_param, 0x00, sizeof(s_micon_param));
    s_micon_param.gyro_filter=1;

#ifdef SHUB_SUSPEND
    s_is_suspend = false;
#endif
    mutex_lock(&s_tDataMutex);
    memset(&s_tLatestAccData       , 0x00 , sizeof(s_tLatestAccData));
    memset(&s_tLatestGyroData      , 0x00 , sizeof(s_tLatestGyroData));
    memset(&s_tLatestMagData       , 0x00 , sizeof(s_tLatestMagData));
    memset(&s_tLatestBaroData      , 0x00 , sizeof(s_tLatestBaroData));
    memset(&s_tLoggingBaroData     , 0x00 , sizeof(s_tLoggingBaroData));
    memset(&s_tLatestOriData       , 0x00 , sizeof(s_tLatestOriData));
    memset(&s_tLatestGravityData   , 0x00 , sizeof(s_tLatestGravityData));
    memset(&s_tLatestLinearAccData , 0x00 , sizeof(s_tLatestLinearAccData));
    memset(&s_tLatestRVectData     , 0x00 , sizeof(s_tLatestRVectData));
    memset(&s_tLatestGameRVData    , 0x00 , sizeof(s_tLatestGameRVData));
    memset(&s_tLatestGeoRVData     , 0x00 , sizeof(s_tLatestGeoRVData));
    memset(&s_tLatestStepCountData   , 0x00 , sizeof(s_tLatestStepCountData));
    mutex_unlock(&s_tDataMutex);
    s_enable_notify_step= false;

#ifdef USE_RESET_SIGNAL
#ifdef USE_REMAP_SIGNAL
    shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 0);
#endif
    shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0);
    usleep(SHUB_RESET_PLUSE_WIDTH * 1000);
    shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1);

    usleep(SHUB_RESET_TIME * 1000);
#endif

#ifndef NO_HOST
    while(1) {
        hostif_read(STATUS, &reg, sizeof(reg));
        if(reg == 0x00) {
            DBG(DBG_LV_INFO, "STATUS OK!!\n");
            break;
        }

        usleep(SHUB_RESET_TIME * 1000);
        if(cnt++ > 10) {
            DBG(DBG_LV_ERROR, "shub_initialize:STATUS read TimeOut. reg=%x \n", reg);
            return SHUB_RC_ERR_TIMEOUT;
        }
    }
#endif
    reg = 0x04;
    hostif_write(CFG, &reg, sizeof(reg));

    hostif_read(INTREQ0, sreg.u8, 2);

    reg = 0x00;
    hostif_write(INTMASK0, &reg, sizeof(reg));
    hostif_write(INTMASK1, &reg, sizeof(reg));

    ENABLE_IRQ;

    return SHUB_RC_OK;
}

int32_t shub_init_param( void )
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    // Get Sensor Version
    cmd.cmd.u16 = HC_MCU_GET_VERSION;
    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_CK_CONNECT |EXE_HOST_EX_NO_RECOVER), 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "FW Version Get Error HC_MCU_GET_VERSION(%d) err %x\n",cmd.prm.u8[0], res.err.u16);
        return SHUB_RC_ERR;
    }
    s_lsi_id = (uint32_t)RESU8_TO_X16(res, 6);
    DBG(DBG_LV_ERROR, "Sensor User FW Version %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n",
            res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],
            res.res.u8[4],res.res.u8[5],res.res.u8[6],res.res.u8[7]);
    DBG(DBG_LV_ERROR, "Sensor Id = 0x%04x\n", s_lsi_id);

    // Get Sensor Info
    cmd.cmd.u16 = HC_MCU_GET_EX_SENSOR;
    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_EX_NO_RECOVER), 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "%s : Get Sensor Info Error(%d) err %x\n", __FUNCTION__, cmd.prm.u8[0], res.err.u16);
        return SHUB_RC_ERR;
    }
    s_exist_sensor = res.res.u8[0]; 
    DBG(DBG_LV_ERROR, "Sensor Info = 0x%04x\n", s_exist_sensor);

#ifdef CONFIG_ACC_U2DH
    ret = shub_lis2dh_low_power();
    if(SHUB_RC_OK != ret) {
        DBG(DBG_LV_ERROR, "%s : lis2dh_low_power Error\n", __FUNCTION__);
        return SHUB_RC_ERR;
    }
#endif

#ifdef CONFIG_PICKUP_PROX
    cmd.cmd.u16 = HC_MCU_GET_PDIR;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_MCU_GET_PDIR err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_MCU_SET_PDIR;
        memcpy(cmd.prm.u8, res.res.u8, 7);
        cmd.prm.u8[3] = 1;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MCU_SET_PDIR err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
#endif

#ifdef CONFIG_SET_AXIS_VAL

//    if(sh_boot_get_handset() == 1) {
        cmd.cmd.u16 = HC_ACC_SET_POSITION;
//      cmd.prm.u8[0] = SHUB_ACC_AXIS_VAL;
        cmd.prm.u8[0] = shub_get_acc_axis_val();
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "error enable HC_ACC_SET_POSITION 0x%x\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        if((s_exist_sensor & HC_MAG_VALID) != 0){
            cmd.cmd.u16 = HC_MAG_SET_POSITION;
//          cmd.prm.u8[0] = SHUB_MAG_AXIS_VAL;
            cmd.prm.u8[0] = shub_get_mag_axis_val();
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
                DBG(DBG_LV_ERROR, "error HC_MAG_SET_POSITION 0x%x\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
        if((s_exist_sensor & HC_GYRO_VALID) != 0){
            cmd.cmd.u16 = HC_GYRO_SET_POSITION;
//          cmd.prm.u8[0] = SHUB_GYRO_AXIS_VAL;
            cmd.prm.u8[0] = shub_get_gyro_axis_val();
            ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
            if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
                shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
                DBG(DBG_LV_ERROR, "error HC_GYRO_SET_POSITION 0x%x\n", res.err.u16);
                return SHUB_RC_ERR;
            }
        }
//    }
#endif

    /***** Set Mag calibration *****/
    if((s_exist_sensor & HC_MAG_VALID) != 0){
        cmd.cmd.u16 = HC_MAG_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0; 
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_MAG_SET_CAL err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        s_micon_param.mag_cal = cmd.prm.u8[1];
    }

    /***** Set Gyro calibration *****/
    if((s_exist_sensor & HC_GYRO_VALID) != 0){
        cmd.cmd.u16 = HC_GYRO_SET_CAL;
        cmd.prm.u8[0] = 1;
        cmd.prm.u8[1] = 0;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 2);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_CAL err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        s_micon_param.gyro_cal = cmd.prm.u8[1] ;
    }

#ifdef SHUB_SW_GYRO_ENABLE
    cmd.cmd.u16 = HC_GYRO_GET_CALIB;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_GYRO_GET_CALIB err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_GYRO_SET_CALIB;
        cmd.prm.u8[0] = 0x4C;
        cmd.prm.u8[1] = 0x1D;
        cmd.prm.u8[2] = 0x04;
        cmd.prm.u8[3] = 0x29;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 4);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_CALIB err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
#endif

#ifdef SHUB_SW_GYRO_ENABLE
    cmd.cmd.u16 = HC_GYRO_GET_PARAM;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
        DBG(DBG_LV_ERROR, "HC_GYRO_GET_PARAM err(%x)\n", res.err.u16);
    } else {
        cmd.cmd.u16 = HC_GYRO_SET_PARAM;
        cmd.prm.u8[0] = res.res.u8[0];
        cmd.prm.u8[1] = res.res.u8[1];
        cmd.prm.u8[2] = res.res.u8[2];
        cmd.prm.u8[3] = res.res.u8[3];
        cmd.prm.u8[4] = 0;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 5);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
            DBG(DBG_LV_ERROR, "HC_GYRO_SET_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
    }
#endif

//#ifdef CONFIG_BARO_SENSOR
//    cmd.cmd.u16 = HC_BARO_GET_PARAM;
//    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
//    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
//        shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
//        DBG(DBG_LV_ERROR, "HC_BARO_GET_PARAM err(%x)\n", res.err.u16);
//    } else {
//        cmd.cmd.u16 = HC_BARO_SET_PARAM;
//        cmd.prm.u8[0] = 1;
//        cmd.prm.u8[1] = 1;
//        cmd.prm.u8[2] = 0;
//        cmd.prm.u8[3] = 5;
//        cmd.prm.u8[4] = res.res.u8[4];
//        cmd.prm.u8[5] = res.res.u8[5];
//        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 6);
//        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
//            shub_set_error_code(RESU16_SHUB_ERR_CODE(cmd,res));
//            DBG(DBG_LV_ERROR, "HC_BARO_SET_PARAM err(%x)\n", res.err.u16);
//            return SHUB_RC_ERR;
//        }
//    }
//#endif


    ret = shub_set_default_parameter();
    if(ret != 0) {
        shub_set_error_code(SHUB_FUP_ERR_SET_PARAM);
        DBG(DBG_LV_ERROR, "Failed shub_set_default_parameter. ret=%x\n", ret);
        return SHUB_RC_ERR;
    }

    
    shub_dbg_clr_irq_log();

    shub_mot_still_enable_flag = false;

    return SHUB_RC_OK;
}

void shub_set_error_code(uint32_t mode){
    shub_err_code = mode;
    return;
}

uint32_t shub_get_error_code(void){
    return shub_err_code;
}

int32_t shub_initialize( void )
{
    int32_t ret;

    // Init Common
    ret = shub_init_common();
    if(ret != SHUB_RC_OK){
        return ret;
    }

#ifndef NO_HOST
    // check access
    ret = shub_check_access();
    if(ret != SHUB_RC_OK) {
        return ret;
    }
    shub_connect_flg = true;


    // Init Param
    ret = shub_init_param();
    if(ret != SHUB_RC_OK) {
        shub_failed_init_param++;
        DBG(DBG_LV_ERROR, "Failed shub_init_param. ret=%x, cnt=%d\n", ret, shub_failed_init_param);
        //return ret;
    }
#endif

    shub_exif_input_val_init();

    return SHUB_RC_OK;
}

static int32_t shub_update_firmware(bool boot, uint8_t *arg_iDataPage1, uint8_t *arg_iDataPage2, uint32_t arg_iLen)
{
    uint8_t reg = 0xFF;
    Word    sreg;
    uint32_t i;
    int32_t ret;
    int32_t remain_size = arg_iLen;
    uint32_t write_size;
    uint32_t write_pos;
    HostCmd cmd;
    HostCmdRes res;
    uint32_t chksum;

    DBG(DBG_LV_INFO, "### Start Firmware Update ### boot=%d, DataPage1=%lx DataPage2=%lx Len=%d \n"
            , boot, (long)arg_iDataPage1, (long)arg_iDataPage2, arg_iLen);
    atomic_set(&g_FWUpdateStatus,true);

    //  if((arg_iDataPage1 == NULL) || (arg_iLen == 0)){
    //      DBG(DBG_LV_ERROR, "arg error\n");
    //      ret = SHUB_RC_ERR;
    //      goto ERROR;
    //  }

    if(boot){
#ifdef USE_RESET_SIGNAL
#ifdef USE_REMAP_SIGNAL
        shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 1);
#endif
        shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0);
        usleep(SHUB_RESET_PLUSE_WIDTH * 1000);
        shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1);
#endif
        usleep(SHUB_RESET_TIME * 1000);
    }else{
        ENABLE_IRQ;

        cmd.cmd.u16 = HC_MCU_FUP_START;
        cmd.prm.u8[0] = 0x55;
        cmd.prm.u8[1] = 0xAA;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 2);
        if(ret != SHUB_RC_OK) {
            DBG(DBG_LV_ERROR, "Communication Error!\n");
            ret = SHUB_RC_ERR;
            goto ERROR;
        }
        if(res.err.u16 == ERROR_FUP_CERTIFICATION) {
            DBG(DBG_LV_ERROR, "Certification Error!\n");
            ret = SHUB_RC_ERR;
            goto ERROR;
        }
        usleep(SHUB_RESET_TIME * 1000);
    } 
    reg = 0x04;
    hostif_write(CFG, &reg, sizeof(reg));

    hostif_read(INTREQ0, sreg.u8, 2);

    reg = 0x00;
    hostif_write(INTMASK0, &reg, sizeof(reg));
    hostif_write(INTMASK1, &reg, sizeof(reg));

    ENABLE_IRQ;

    DBG(DBG_LV_INFO, "Check Firmware Mode.\n");
    cmd.cmd.u16 = HC_MCU_GET_VERSION;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL_RSLT, 0);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }
    if(res.res.u8[2] != 0x01){
        DBG(DBG_LV_ERROR, "Version check Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    DBG(DBG_LV_INFO, "Flash Clear.\n");
    cmd.cmd.u16 = HC_MCU_FUP_ERASE;
    cmd.prm.u8[0] = 0xAA;
    cmd.prm.u8[1] = 0x55;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 2);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }
    if(res.err.u16 == ERROR_FUP_CERTIFICATION) {
        DBG(DBG_LV_ERROR, "Certification Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    write_pos=0;
    while(remain_size > 0){
        if(remain_size > FIFO_SIZE){
            write_size = FIFO_SIZE;
            remain_size -= FIFO_SIZE;
        }else{
            write_size = remain_size;
            remain_size = 0;
        }
        if(write_pos < (64*1024)){
            ret = hostif_write(FIFO, &arg_iDataPage1[write_pos], write_size);
        }else{
            ret = hostif_write(FIFO, &arg_iDataPage2[write_pos - (64*1024)], write_size);
        }
        write_pos+= write_size;

        cmd.cmd.u16 = HC_MCU_FUP_WRITE_FIFO;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 0);
        if(ret != SHUB_RC_OK || res.err.u16 != 0) {
            DBG(DBG_LV_ERROR, "HC_MCU_FUP_WRITE_FIFO err(%x)\n", res.err.u16);
            ret = SHUB_RC_ERR;
            goto ERROR;
        }
    }


    chksum=0;
    for(i= 0; i < arg_iLen; i++){
        if(i < (64*1024)){
            chksum += (uint32_t)arg_iDataPage1[i];
        }else{
            chksum += (uint32_t)arg_iDataPage2[i - (64*1024)];
        }
    }
    DBG(DBG_LV_INFO, "Write CheckSum Value=[0x%08X].\n", chksum);
    hostif_write(FIFO, (uint8_t *)&chksum, (int32_t)sizeof(chksum));
    cmd.cmd.u16 = HC_MCU_FUP_WRITE_FIFO;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 0);
    if(ret != SHUB_RC_OK || res.err.u16 != 0) {
        DBG(DBG_LV_ERROR, "Fifo Write Cmd Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    DBG(DBG_LV_INFO, "SelfTest.\n");
    cmd.cmd.u16 = HC_MCU_FUP_SELFTEST;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_RES|EXE_HOST_WAIT, 0);
    if(ret != SHUB_RC_OK || res.res.u8[0] == 0x01) {
        DBG(DBG_LV_ERROR, "SelfTest Error! [Firmware Update Mode]\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    DISABLE_IRQ;

    DBG(DBG_LV_INFO, "End Firmware Update.\n");
    cmd.cmd.u16 = HC_MCU_FUP_END;
    shub_hostcmd(&cmd, &res, 0, 0);

    usleep(SHUB_RESET_TIME * 1000);

    DBG(DBG_LV_INFO, "Initialize.\n");
    reg = 0x04;
    hostif_write(CFG, &reg, sizeof(reg));

    hostif_read(INTREQ0, sreg.u8, 2);

    reg = 0x00;
    hostif_write(INTMASK0, &reg, sizeof(reg));
    hostif_write(INTMASK1, &reg, sizeof(reg));

    ENABLE_IRQ;

    DBG(DBG_LV_INFO, "Check User program mode.\n");
    cmd.cmd.u16 = HC_MCU_GET_VERSION;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }

    if(res.res.u8[2] != 0x00){
        DBG(DBG_LV_ERROR, "Version check Error!");
        ret = SHUB_RC_ERR;
        goto ERROR;
    }
    ret = SHUB_RC_OK;
ERROR:
    atomic_set(&g_FWUpdateStatus,false);

    return ret;
}

int32_t shub_update_fw(bool boot, uint8_t *arg_iDataPage1, uint8_t *arg_iDataPage2, uint32_t arg_iLen)
{
    int32_t ret;
    int32_t i;
    int32_t retry_count = 0;
    
    //    if((arg_iDataPage1 == NULL) || (arg_iLen == 0)){
    if((arg_iDataPage1 == NULL) || (arg_iDataPage2 == NULL) || (arg_iLen == 0)){

        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    for(i=0; i<SHUB_CMD_RETRY_NUM; i++) {
        ret = shub_update_firmware(boot, arg_iDataPage1, arg_iDataPage2, arg_iLen);
        if(SHUB_RC_OK != ret) {
            DBG(DBG_LV_ERROR, "%s : FW update Error(%d) Retry=%d\n", __FUNCTION__, ret, i);
            ret = SHUB_RC_ERR;
        }else{
            retry_count = i;
            i = SHUB_CMD_RETRY_NUM;
            DBG(DBG_LV_ERROR, "%s : FW update OK!!( Retry=%d )\n", __FUNCTION__, retry_count);
            ret = SHUB_RC_OK;
        }
    }
    if(ret != SHUB_RC_OK) {
        shub_set_error_code(SHUB_FUP_ERR_WRITE);
        DBG(DBG_LV_ERROR, "%s : FW update Retry over!!( Retry=%d, ret=%d )\n", __FUNCTION__, retry_count, ret);
        return ret;
    }

    ret = shub_initialize();
    if(ret != SHUB_RC_OK) {
        if(ret == SHUB_RC_ERR){
            shub_set_error_code(SHUB_FUP_ERR_CHECK_ACCESS);
            DBG(DBG_LV_ERROR, "%s : shub_initialize(check_access) Error( ret=%d )\n", __FUNCTION__, ret);
        } else{
            shub_set_error_code(SHUB_FUP_ERR_INIT_COMMON);
            DBG(DBG_LV_ERROR, "%s : shub_initialize(init_common) Error( ret=%d )\n", __FUNCTION__, ret);
        }
        return SHUB_RC_ERR;
    }
    shub_fw_write_flg = false;
    return ret;
}

static int32_t shub_check_access(void)
{
    int32_t ret;

    // user FW check
    ret = shub_user_fw_check();
    if(ret == SHUB_RC_OK) {
        return SHUB_RC_OK;
    }

    // boot FW check
    ret = shub_boot_fw_check();
    if(ret == SHUB_RC_OK){
        DBG(DBG_LV_ERROR, "%s : Boot FW Update Flag On!!\n", __FUNCTION__);
        shub_fw_write_flg = true;
        shub_connect_flg = true;
    }
    return SHUB_RC_ERR;
}

static int32_t shub_boot_fw_check(void)
{
    uint8_t reg = 0xFF;
    Word    sreg;
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;
    int32_t i;
    int32_t retry_count = 0;

    DBG(DBG_LV_INFO, "boot start\n");
    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

#ifdef USE_RESET_SIGNAL
#ifdef USE_REMAP_SIGNAL
    shub_gpio_set_value(SHUB_GPIO_PIN_BRMP, 1);
#endif
    shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 0);
    usleep(SHUB_RESET_PLUSE_WIDTH * 1000);
    shub_gpio_set_value(SHUB_GPIO_PIN_RESET, 1);
#endif
    usleep(SHUB_RESET_TIME * 1000);

    reg = 0x04;
    hostif_write(CFG, &reg, sizeof(reg));

    hostif_read(INTREQ0, sreg.u8, 2);

    reg = 0x00;
    hostif_write(INTMASK0, &reg, sizeof(reg));
    hostif_write(INTMASK1, &reg, sizeof(reg));

    ENABLE_IRQ;

    // check boot version
    for(i=0; i<SHUB_CMD_RETRY_NUM; i++) {
        cmd.cmd.u16 = HC_MCU_GET_VERSION;
        ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL_RSLT | EXE_HOST_CK_CONNECT), 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : FW Version Get Error(ret=%d, err=0x%x, cnt=%d)\n", __FUNCTION__, ret, res.err.u16, i);
            ret = SHUB_RC_ERR;
        }else{
            retry_count = i;
            i = SHUB_CMD_RETRY_NUM;
            DBG(DBG_LV_ERROR, "Sensor Boot FW Version(%d) %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", retry_count,
                    res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],
                    res.res.u8[4],res.res.u8[5],res.res.u8[6],res.res.u8[7]);
            ret = SHUB_RC_OK;
        }
    }
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "%s : Boot FW Version Retry over!!( Retry=%d, ret=%d )\n", __FUNCTION__, i, ret);
    }

    shub_init_common();
    return ret;
}

static int32_t shub_user_fw_check(void)
{
    int32_t i;
    int32_t retry_count = 0;
    int32_t ret = SHUB_RC_OK;
    HostCmd cmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }
    // check version
    for(i=0; i<SHUB_CMD_RETRY_NUM; i++) {
        cmd.cmd.u16 = HC_MCU_GET_VERSION;
        ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_CK_CONNECT |EXE_HOST_EX_NO_RECOVER), 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "%s : FW Version Get Error(ret=%d, err=0x%x, cnt=%d)\n", __FUNCTION__, ret, res.err.u16, i);
            ret = SHUB_RC_ERR;
        }else{
            retry_count = i;
            i = SHUB_CMD_RETRY_NUM;
            DBG(DBG_LV_INFO, "Sensor User FW Version(%d) %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x\n", retry_count,
                    res.res.u8[0],res.res.u8[1],res.res.u8[2],res.res.u8[3],
                    res.res.u8[4],res.res.u8[5],res.res.u8[6],res.res.u8[7]);
            ret = SHUB_RC_OK;
        }
    }
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "%s : FW Version Retry over!!( Retry=%d, ret=%d )\n", __FUNCTION__, i, ret);
        return SHUB_RC_ERR;
    }
    s_lsi_id = (uint32_t)RESU8_TO_X16(res, 6);
    // check sum
#ifndef NO_LINUX
    cmd.cmd.u16 = HC_MCU_FUP_SELFTEST;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_RES|EXE_HOST_WAIT, 0);
    if(ret != SHUB_RC_OK || res.res.u8[0] == 0x01) {
        DBG(DBG_LV_ERROR, "%s : SelfTest Error(ret=%d)\n", __FUNCTION__, ret);
        return SHUB_RC_ERR;
    }
#endif
    return SHUB_RC_OK;
}

int32_t shub_get_fw_version(uint8_t *arg_iData)
{
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret = SHUB_RC_OK;

    if(arg_iData == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    cmd.cmd.u16 = HC_MCU_GET_VERSION;

    ret = shub_hostcmd(&cmd, &res, (EXE_HOST_ALL | EXE_HOST_EX_NO_RECOVER), 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "FW Version Get Error HC_MCU_GET_VERSION(%d) err %x\n",cmd.prm.u8[0], res.err.u16);
        return SHUB_RC_ERR;
    }

    arg_iData[0] = res.res.u8[0];
    arg_iData[1] = res.res.u8[1];
    arg_iData[2] = res.res.u8[2];
    arg_iData[3] = res.res.u8[3];

    DBG(DBG_LV_INFO, "FW Version=%02x %02x %02x %02x\n", arg_iData[0], arg_iData[1], arg_iData[2], arg_iData[3]);
    return SHUB_RC_OK;
}

int32_t shub_get_sensors_data(int32_t type, int32_t* data)
{
    int32_t ret = SHUB_RC_OK;
    struct timespec ts;
    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

#ifdef SHUB_SUSPEND
    if(s_is_suspend){
        return SHUB_RC_OK;
    }
#endif
    mutex_lock(&userReqMutex);
    ret = shub_read_sensor_data(type);
    ts = shub_get_timestamp();
    mutex_unlock(&userReqMutex);

    mutex_lock(&s_tDataMutex);
    switch(type){
        case SHUB_ACTIVE_ACC:
            data[0] = s_tLatestAccData.nX;
            data[1] = s_tLatestAccData.nY;
            data[2] = s_tLatestAccData.nZ;
            data[3] = ts.tv_sec;
            data[4] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_GYRO:
            data[0] = s_tLatestGyroData.nX;
            data[1] = s_tLatestGyroData.nY;
            data[2] = s_tLatestGyroData.nZ;
            data[3] = s_tLatestGyroData.nAccuracy;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_GYROUNC:
            data[0] = s_tLatestGyroData.nX + s_tLatestGyroData.nXOffset;
            data[1] = s_tLatestGyroData.nY + s_tLatestGyroData.nYOffset;
            data[2] = s_tLatestGyroData.nZ + s_tLatestGyroData.nZOffset;
            data[3] = s_tLatestGyroData.nAccuracy;
            data[4] = s_tLatestGyroData.nXOffset;
            data[5] = s_tLatestGyroData.nYOffset;
            data[6] = s_tLatestGyroData.nZOffset;
            data[7] = ts.tv_sec;
            data[8] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_MAG:
            data[0] = s_tLatestMagData.nX;
            data[1] = s_tLatestMagData.nY;
            data[2] = s_tLatestMagData.nZ;
            data[3] = s_tLatestMagData.nAccuracy;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_MAGUNC:
            data[0] = s_tLatestMagData.nX + s_tLatestMagData.nXOffset;
            data[1] = s_tLatestMagData.nY + s_tLatestMagData.nYOffset;
            data[2] = s_tLatestMagData.nZ + s_tLatestMagData.nZOffset;
            data[3] = s_tLatestMagData.nXOffset;
            data[4] = s_tLatestMagData.nYOffset;
            data[5] = s_tLatestMagData.nZOffset;
            data[6] = s_tLatestMagData.nAccuracy;
            data[7] = ts.tv_sec;
            data[8] = ts.tv_nsec;
            break;


#ifdef CONFIG_BARO_SENSOR
        case SHUB_ACTIVE_BARO:
            data[0] = (int32_t)(s_tLatestBaroData.pressure);
            data[1] = ts.tv_sec;
            data[2] = ts.tv_nsec;
            break;
#endif


        case SHUB_ACTIVE_ORI:
            data[0] = s_tLatestOriData.pitch;
            data[1] = s_tLatestOriData.roll;
            data[2] = s_tLatestOriData.yaw;
            data[3] = s_tLatestOriData.nAccuracy;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_GRAVITY:
            data[0] = s_tLatestGravityData.nX;
            data[1] = s_tLatestGravityData.nY;
            data[2] = s_tLatestGravityData.nZ;
            data[3] = ts.tv_sec;
            data[4] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_LACC:
            data[0] = s_tLatestLinearAccData.nX;
            data[1] = s_tLatestLinearAccData.nY;
            data[2] = s_tLatestLinearAccData.nZ;
            data[3] = ts.tv_sec;
            data[4] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_RV:
            data[0] = s_tLatestRVectData.nX;
            data[1] = s_tLatestRVectData.nY;
            data[2] = s_tLatestRVectData.nZ;
            data[3] = s_tLatestRVectData.nS;
            data[4] = s_tLatestRVectData.nAccuracy;
            data[5] = ts.tv_sec;
            data[6] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_RV_NONMAG:
            data[0] = s_tLatestGameRVData.nX;
            data[1] = s_tLatestGameRVData.nY;
            data[2] = s_tLatestGameRVData.nZ;
            data[3] = s_tLatestGameRVData.nS;
            data[4] = ts.tv_sec;
            data[5] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_RV_NONGYRO:
            data[0] = s_tLatestGeoRVData.nX;
            data[1] = s_tLatestGeoRVData.nY;
            data[2] = s_tLatestGeoRVData.nZ;
            data[3] = s_tLatestGeoRVData.nS;
            data[4] = s_tLatestGeoRVData.nAccuracy;
            data[5] = ts.tv_sec;
            data[6] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_PEDOM:
        case SHUB_ACTIVE_PEDOM_NO_NOTIFY:
        case SHUB_ACTIVE_PEDODEC:
        case SHUB_ACTIVE_PEDODEC_NO_NOTIFY:
//            data[0] = (uint32_t)(s_tLatestStepCountData.step - s_tLatestStepCountData.stepOffset);
            data[0] = (uint32_t)(s_tLatestStepCountData.step);
            data[1] = ts.tv_sec;
            data[2] = ts.tv_nsec;
            break;

        case SHUB_ACTIVE_SIGNIFICANT:
            data[0] = ts.tv_sec;
            data[1] = ts.tv_nsec;
            break;
        default:
            ret =  SHUB_RC_OK;
            break;
    }
    mutex_unlock(&s_tDataMutex);

    return ret;

}

int32_t shub_activate(int32_t arg_iSensType, int32_t arg_iEnable)
{
    int32_t ret;
    uint8_t notify=0;
    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }

    mutex_lock(&userReqMutex);

    ret = shub_activate_pedom_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    ret = shub_activate_significant_exec(arg_iSensType, arg_iEnable, &notify);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    ret = shub_activate_exec(arg_iSensType, arg_iEnable, 0);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    shub_sensor_info[SHUB_SAME_NOTIFY_GYRO] = atomic_read(&g_CurrentSensorEnable) & GYRO_GROUP_MASK;
    if(shub_sensor_info[SHUB_SAME_NOTIFY_GYRO] != GYRO_GROUP_MASK){
        if(shub_sensor_info[SHUB_SAME_NOTIFY_GYRO] == 0){
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_GYRO] = 0;
        }
        else {
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_GYRO] = shub_sensor_info[SHUB_SAME_NOTIFY_GYRO];
        }
    }
    shub_sensor_info[SHUB_SAME_NOTIFY_MAG] = atomic_read(&g_CurrentSensorEnable) & MAG_GROUP_MASK;
    if(shub_sensor_info[SHUB_SAME_NOTIFY_MAG] != MAG_GROUP_MASK){
        if(shub_sensor_info[SHUB_SAME_NOTIFY_MAG] == 0){
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_MAG] = 0;
        }
        else {
            shub_sensor_first_measure_info[SHUB_SAME_NOTIFY_MAG] = shub_sensor_info[SHUB_SAME_NOTIFY_MAG];
        }
    }

    if(notify==1){
#ifdef NO_LINUX
        shub_significant_work_func(NULL);
#else
        schedule_work(&significant_work);
#endif
    }

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_activate_logging(int32_t arg_iSensType, int32_t arg_iEnable)
{
    int32_t ret;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return 0;
    }

    mutex_lock(&userReqMutex);
    ret = shub_activate_pedom_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }

    ret = shub_activate_logging_exec(arg_iSensType, arg_iEnable);
    if(SHUB_RC_OK != ret) {
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);

    return ret;
}

int32_t shub_set_delay(int32_t arg_iSensType, int32_t delay)
{
    int32_t ret = SHUB_RC_OK;
    int32_t max_delay = MEASURE_MAX_US;

    delay *= 1000;

    if(delay < 0){
        delay = 0; 
    }

#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        max_delay = SENSOR_BARO_MAX_DELAY;
    }
#endif

    if(delay > max_delay){
        delay = max_delay; 
    }

    mutex_lock(&userReqMutex);

    DBG(DBG_LV_INFO, "####%s [%s](%dus)\n", __FUNCTION__, shub_get_active_sensor_name (arg_iSensType), delay);

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
        if(iCurrentSensorEnable & SHUB_ACTIVE_SHEX_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, delay);
        }
        else {
            s_sensor_delay_us.acc = delay;
        }
//        s_sensor_delay_us.acc = delay;
//        s_sensor_delay_us.acc = SHUB_MIN(shub_get_exif_delay_ms() * 1000, delay);
    }else if(arg_iSensType & SHUB_ACTIVE_GYRO){
        s_sensor_delay_us.gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAG){
        s_sensor_delay_us.mag = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GYROUNC){
        s_sensor_delay_us.gyro_uc = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAGUNC){
        s_sensor_delay_us.mag_uc = delay;
#ifdef CONFIG_BARO_SENSOR
    }else if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_sensor_delay_us.baro = delay;
#endif
    }else if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_sensor_delay_us.orien = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_sensor_delay_us.grav = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_sensor_delay_us.linear = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV){
        s_sensor_delay_us.rot = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_sensor_delay_us.rot_gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_sensor_delay_us.rot_mag = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_SHEX_ACC){
        int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);
        if(iCurrentSensorEnable & SHUB_ACTIVE_ACC) {
            s_sensor_delay_us.acc = SHUB_MIN(shub_get_acc_delay_ms() * 1000, delay);
        }
        else {
            s_sensor_delay_us.acc = delay;
        }
//        s_sensor_delay_us.acc = delay;
//        s_sensor_delay_us.acc = SHUB_MIN(delay, s_sensor_delay_us.acc);
    }

    if(s_sensor_delay_us.gyro == s_sensor_delay_us.gyro_uc){
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_GYRO] = 1;
    }
    else {
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_GYRO] = 0;
    }
    if(s_sensor_delay_us.mag == s_sensor_delay_us.mag_uc){
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_MAG] = 1;
    }
    else {
        shub_sensor_same_delay_flg[SHUB_SAME_NOTIFY_MAG] = 0;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    ret = shub_set_delay_exec(arg_iSensType,0);
    if(ret != SHUB_RC_OK) {
        goto ERROR;
    }

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}


int32_t shub_set_delay_logging(int32_t arg_iSensType, int32_t delay)
{
    int32_t ret = SHUB_RC_OK;
    int32_t max_delay = MEASURE_MAX_US;

    mutex_lock(&userReqMutex);

    if(delay < 0){
        delay = 0; 
    }

    delay /= 1000;
#ifdef CONFIG_BARO_SENSOR
    if(arg_iSensType & SHUB_ACTIVE_BARO){
        max_delay = SENSOR_BARO_MAX_DELAY;
    }
#endif

    if(delay > max_delay){
        delay = max_delay; 
    }

    DBG(DBG_LV_INFO, "####%s(0x%x, %dus)\n", __FUNCTION__, arg_iSensType, delay);

    if(arg_iSensType & SHUB_ACTIVE_ACC){
        s_logging_delay_us.acc = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GYRO){
        s_logging_delay_us.gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAG){
        s_logging_delay_us.mag = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GYROUNC){
        s_logging_delay_us.gyro_uc = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_MAGUNC){
        s_logging_delay_us.mag_uc = delay;
#ifdef CONFIG_BARO_SENSOR
    }else if(arg_iSensType & SHUB_ACTIVE_BARO){
        s_logging_delay_us.baro = delay;
#endif
    }else if(arg_iSensType & SHUB_ACTIVE_ORI){
        s_logging_delay_us.orien = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_GRAVITY){
        s_logging_delay_us.grav = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_LACC){
        s_logging_delay_us.linear = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV){
        s_logging_delay_us.rot = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONMAG){
        s_logging_delay_us.rot_gyro = delay;
    }else if(arg_iSensType & SHUB_ACTIVE_RV_NONGYRO){
        s_logging_delay_us.rot_mag = delay;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        ret =  SHUB_RC_ERR;
        goto ERROR;
    }

    ret = shub_set_delay_exec(0, arg_iSensType);
    if(ret != SHUB_RC_OK) {
        goto ERROR;
    }

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_current_active(void)
{
    int32_t iCurrentEnable;

    iCurrentEnable = atomic_read(&g_CurrentSensorEnable) ;

    return (iCurrentEnable & ACTIVE_FUNC_MASK);
}

int32_t shub_get_current_active_logging(void)
{
    int32_t iCurrentEnable;

    iCurrentEnable = atomic_read(&g_CurrentLoggingSensorEnable) ;

    return (iCurrentEnable & ACTIVE_FUNC_MASK);
}

int32_t shub_set_param(int32_t type,int32_t* data)
{
    int32_t ret = SHUB_RC_OK;
    IoCtlParam local_param;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    memcpy(local_param.m_iParam, data, sizeof(local_param.m_iParam));

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }
    mutex_lock(&userReqMutex);
    shub_set_param_check_exif(type, local_param.m_iParam);
    ret = shub_set_param_exec(type, local_param.m_iParam);
    if((ret == SHUB_RC_OK) && (type == APP_PEDOMETER)) {
        shub_set_enable_ped_exif_flg(local_param.m_iParam[0]);
    }
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_param(int32_t type,int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_param_exec(type,data);
//  shub_get_param_check_exif(type, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_clear_data(int32_t type)
{
    int32_t ret = SHUB_RC_OK;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_clear_data_app_exec(type);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_init_app(int32_t type)
{
    int32_t ret = SHUB_RC_OK;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_init_app_exec(type);
    mutex_unlock(&userReqMutex);
    
    if(ret == SHUB_RC_OK) {
        //ret = shub_set_default_parameter();
        ret = shub_set_default_ped_parameter();
        if(ret != 0) {
            //DBG(DBG_LV_ERROR, "Failed shub_set_default_parameter. ret=%x\n", ret);
            DBG(DBG_LV_ERROR, "Failed shub_set_default_ped_parameter. ret=%x\n", ret);
            return SHUB_RC_ERR;
        }
    }

    return ret;
}


int32_t shub_get_data_pedometer(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_PEDOMETER, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_normal_pedometer(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_NORMAL_PEDOMETER, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_motion(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_MOTDTECTION, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_low_power(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_LOW_POWER, data);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_data_act_detection(int32_t* data)
{
    int32_t ret = SHUB_RC_OK;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);
    ret = shub_get_data_app_exec(APP_RUN_DETECTION, data);
    ret = shub_get_data_app_exec(APP_TOTAL_STATUS_DETECTION, &data[1]);
    ret = shub_get_data_app_exec(APP_VEICHLE_DETECTION, &data[7]);
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_direct_hostcmd(uint16_t cmd, const uint8_t *prm, uint8_t *rslt)
{
    HostCmd hcmd;
    HostCmdRes res;
    int32_t ret;

    if((prm == NULL) || (rslt == NULL)) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_OK;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    hcmd.cmd.u16 = cmd;
    memcpy(hcmd.prm.u8, prm, 16);

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 16);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, " err(%x)\n", res.err.u16);
        return g_hostcmdErr;
    }

    memcpy(rslt, res.res.u8, res.res_size);

    return g_hostcmdErr;
}


int32_t shub_logging_flush(void)
{
    int32_t ret;
    mutex_lock(&userReqMutex);
    ret = logging_flush_exec();
    mutex_unlock(&userReqMutex);
    return ret;
}

void shub_logging_clear(void)
{
    int32_t ret;
    HostCmd cmd;
    HostCmdRes res;

    mutex_lock(&userReqMutex);
    DBG(DBG_LV_INFO, "[DBG]FIFO clear start\n");
    cmd.cmd.u16 = HC_LOGGING_GET_RESULT;
    cmd.prm.u8[0] = 0x01;
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        DBG(DBG_LV_ERROR, "HC_LOGGING_GET_RESULT err(%x)\n", res.err.u16);
        mutex_unlock(&userReqMutex);
        return;
    }
    DBG(DBG_LV_INFO, "[DBG] FIFO clear end\n");
    mutex_unlock(&userReqMutex);
}

int32_t shub_write_sensor(uint8_t type, uint8_t addr ,uint8_t data)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MCU_ACCESS_SENSOR;
    hcmd.prm.u8[0] = type;
    hcmd.prm.u8[1] = 1;//write/8bit 
    hcmd.prm.u8[2] = addr;
    hcmd.prm.u8[3] = data;
    hcmd.prm.u8[4] = 0;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 5);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)|| (0 != res.res.u8[0])) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_read_sensor(uint8_t type, uint8_t addr ,uint8_t *data)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(data == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MCU_ACCESS_SENSOR;
    hcmd.prm.u8[0] = type;
    hcmd.prm.u8[1] = 0;//write/8bit 
    hcmd.prm.u8[2] = addr;
    hcmd.prm.u8[3] = 0;
    hcmd.prm.u8[4] = 0;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 5);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
    *data= res.res.u8[0];
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

void shub_change_acc_axis(int32_t* offsets)
{
    int acc_axis;
    int32_t temp;

    acc_axis = shub_get_acc_axis_val();
    DBG(DBG_LV_INFO, "%s : x=%d, y=%d, z=%d, axis=%d\n", __FUNCTION__, offsets[0], offsets[1], offsets[2], acc_axis);

    switch(acc_axis){
    case 1:
        offsets[2] *= -1;              // z
        offsets[0] *= -1;              // x
        break;
    case 2:
        offsets[2] *= -1;              // z
        temp        = offsets[0];      // 
        offsets[0]  = offsets[1];      // x
        offsets[1]  = temp;            // y
        break;
    case 3:
        offsets[2] *= -1;              // z
        offsets[1] *= -1;              // y
        break;
    case 4:
        offsets[2] *= -1;              // z
        temp        = offsets[0] * -1; // 
        offsets[0]  = offsets[1] * -1; // x
        offsets[1]  = temp;             // y
        break;
    default:
        break;
    }
    DBG(DBG_LV_INFO, "%s : x=%d, y=%d, z=%d\n", __FUNCTION__, offsets[0], offsets[1], offsets[2]);
}

int32_t shub_set_acc_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    shub_change_acc_axis(offsets);

    hcmd.cmd.u16 = HC_ACC_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offsets[0];
    hcmd.prm.s16[1] = (int16_t)offsets[1];
    hcmd.prm.s16[2] = (int16_t)offsets[2];

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    shub_acc_offset[0] = hcmd.prm.s16[0];
    shub_acc_offset[1] = hcmd.prm.s16[1];
    shub_acc_offset[2] = hcmd.prm.s16[2];

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_acc_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_ACC_GET_OFFSET;
    ret =  shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
    offsets[1] = (int32_t)res.res.s16[1];
    offsets[2] = (int32_t)res.res.s16[2];

    shub_change_acc_axis(offsets);

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_acc_position(int32_t type)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_ACC_SET_POSITION;
    hcmd.prm.u8[0] = (uint8_t)type;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_mag_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MAG_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offsets[0];
    hcmd.prm.s16[1] = (int16_t)offsets[1];
    hcmd.prm.s16[2] = (int16_t)offsets[2];

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_mag_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MAG_GET_OFFSET;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
    offsets[1] = (int32_t)res.res.s16[1];
    offsets[2] = (int32_t)res.res.s16[2];
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_mag_position(int32_t type)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_MAG_SET_POSITION;
    hcmd.prm.u8[0] = (uint8_t)type;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_cal_mag_axis_interfrence(int32_t* mat)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    if(mat == NULL){
        hcmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        hcmd.prm.u8[0] = 0;
        ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            ret= SHUB_RC_ERR;
            goto ERROR;
        }

    }else{
        hcmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        hcmd.prm.u8[0] = 1;
        hcmd.prm.u8[1] = 0;
        hcmd.prm.s16[1] = mat[0];
        hcmd.prm.s16[2] = mat[1];
        hcmd.prm.s16[3] = mat[2];
        hcmd.prm.s16[4] = mat[3];
        hcmd.prm.s16[5] = mat[4];
        ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            ret= SHUB_RC_ERR;
            goto ERROR;
        }

        shub_mag_axis_interfrence[0] = hcmd.prm.s16[1];
        shub_mag_axis_interfrence[1] = hcmd.prm.s16[2];
        shub_mag_axis_interfrence[2] = hcmd.prm.s16[3];
        shub_mag_axis_interfrence[3] = hcmd.prm.s16[4];
        shub_mag_axis_interfrence[4] = hcmd.prm.s16[5];

        hcmd.cmd.u16 = HC_MAG_SET_STATIC_MAT;
        hcmd.prm.u8[0] = 1;
        hcmd.prm.u8[1] = 1;
        hcmd.prm.s16[1] = mat[5];
        hcmd.prm.s16[2] = mat[6];
        hcmd.prm.s16[3] = mat[7];
        hcmd.prm.s16[4] = mat[8];
        ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 12);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            ret= SHUB_RC_ERR;
            goto ERROR;
        }
        
        shub_mag_axis_interfrence[5] = hcmd.prm.s16[1];
        shub_mag_axis_interfrence[6] = hcmd.prm.s16[2];
        shub_mag_axis_interfrence[7] = hcmd.prm.s16[3];
        shub_mag_axis_interfrence[8] = hcmd.prm.s16[4];
        
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_gyro_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_GYRO_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offsets[0];
    hcmd.prm.s16[1] = (int16_t)offsets[1];
    hcmd.prm.s16[2] = (int16_t)offsets[2];

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 6);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_gyro_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_GYRO_GET_OFFSET;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
    offsets[1] = (int32_t)res.res.s16[1];
    offsets[2] = (int32_t)res.res.s16[2];

ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_set_gyro_position(int32_t type)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_GYRO_SET_POSITION;
    hcmd.prm.u8[0] = (uint8_t)type;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 1);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        mutex_unlock(&userReqMutex);
        return SHUB_RC_ERR;
    }

    mutex_unlock(&userReqMutex);
    return ret;
}

#ifdef CONFIG_BARO_SENSOR
int32_t shub_set_baro_offset(int32_t offset)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_BARO_SET_OFFSET;
    hcmd.prm.s16[0] = (int16_t)offset;

    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 2);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

int32_t shub_get_baro_offset(int32_t* offsets)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(offsets == NULL) {
        DBG(DBG_LV_ERROR, "arg error\n");
        return SHUB_RC_ERR;
    }

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_BARO_GET_OFFSET;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    offsets[0] = (int32_t)res.res.s16[0];
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}
#endif

int32_t shub_get_task_cycle(int32_t* cycle)
{
    int32_t ret=0;
    HostCmd hcmd;
    HostCmdRes res;

    if(atomic_read(&g_FWUpdateStatus)){
        DBG(DBG_LV_ERROR, "FW Update or Recovery Now:%s\n", __FUNCTION__);
        return SHUB_RC_OK;
    }

    mutex_lock(&userReqMutex);

    hcmd.cmd.u16 = HC_SENSOR_TSK_GET_CYCLE;
    ret = shub_hostcmd(&hcmd, &res, EXE_HOST_ALL, 0);
    if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
        ret= SHUB_RC_ERR;
        goto ERROR;
    }

    cycle[0] = (int32_t)res.res.u16[0] * 10;
    cycle[1] = (int32_t)res.res.u16[1] * 10;
    cycle[2] = (int32_t)res.res.u16[2] * 10;
ERROR:
    mutex_unlock(&userReqMutex);
    return ret;
}

void shub_debug_level_chg(int32_t lv)
{
#ifdef CONFIG_ML630Q790_DEBUG
    dbg_level = lv;
    printk("shub_debug_level_chg level:%x\n", lv);
#endif
}

#ifndef NO_LINUX
int32_t shub_suspend(void)
{
    SHUB_DBG_TIME_INIT
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret=0;
    shub_suspend_call_flg = true;

    SHUB_DBG_TIME_START
    suspendBatchingProc();
#ifdef SHUB_SUSPEND
    shub_suspend_acc();
    shub_suspend_mag();
    shub_suspend_mag_uncal();
    shub_suspend_gyro();
    shub_suspend_gyro_uncal();
    shub_suspend_orien();
    shub_suspend_grav();
    shub_suspend_linear();
    shub_suspend_rot();
    shub_suspend_rot_gyro();
    shub_suspend_rot_mag();
    shub_suspend_exif();
#ifdef CONFIG_BARO_SENSOR
    shub_suspend_baro();
#endif
    s_is_suspend = true;
#endif 
    cmd.cmd.u16 = HC_LOGGING_SET_NOTIFY;
    cmd.prm.u8[0] = 0; // Disable
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 1);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
        goto ERROR;
    }

    if(s_enable_notify_step){
        /* Enable/Disable */
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            goto ERROR;
        }

        memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM);
        cmd.prm.u8[3] = 0;

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            goto ERROR;
        }
    }
ERROR:
#ifdef SHUB_SUSPEND
    shub_suspend_sensor_exec();
#endif
    disable_irq(g_nIntIrqNo);
    enable_irq_wake(g_nIntIrqNo);
    shub_dbg_out_irq_log();
    shub_dbg_clr_irq_log();
    shub_suspend_call_flg = false;
    SHUB_DBG_TIME_END((SHUBIO<<8)+9999)
    return ret;
}

int32_t shub_resume(void)
{
    SHUB_DBG_TIME_INIT
    HostCmd cmd;
    HostCmdRes res;
    int32_t ret;
    int32_t data[3]={0};
    int32_t iCurrentSensorEnable = atomic_read(&g_CurrentSensorEnable);

    SHUB_DBG_TIME_START
    disable_irq_wake(g_nIntIrqNo);
    enable_irq(g_nIntIrqNo);

#ifdef SHUB_SUSPEND
    shub_resume_acc();
    shub_resume_mag();
    shub_resume_mag_uncal();
    shub_resume_gyro();
    shub_resume_gyro_uncal();
    shub_resume_orien();
    shub_resume_grav();
    shub_resume_linear();
    shub_resume_rot();
    shub_resume_rot_gyro();
    shub_resume_rot_mag();
    shub_resume_exif();
#ifdef CONFIG_BARO_SENSOR
    shub_resume_baro();
#endif
    s_is_suspend = false;
#endif

    cmd.cmd.u16 = HC_LOGGING_SET_NOTIFY;
    cmd.prm.u8[0] = 1; // Enable
    ret = shub_hostcmd(&cmd, &res, EXE_HOST_WAIT|EXE_HOST_ERR, 1);
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Communication Error!\n");
    }

    if(s_enable_notify_step){
        /* Enable/Disable */
        cmd.cmd.u16 = HC_GET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, 0);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_GET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }

        memcpy(cmd.prm.u8,res.res.u8, SHUB_SIZE_PEDO_STEP_PRM);
        cmd.prm.u8[3] = 1;

        cmd.cmd.u16 = HC_SET_PEDO_STEP_PARAM;
        ret = shub_hostcmd(&cmd, &res, EXE_HOST_ALL, SHUB_SIZE_PEDO_STEP_PRM);
        if((SHUB_RC_OK != ret) || (0 != res.err.u16)) {
            DBG(DBG_LV_ERROR, "HC_SET_PEDO_STEP_PARAM err(%x)\n", res.err.u16);
            return SHUB_RC_ERR;
        }
        if((iCurrentSensorEnable & SHUB_ACTIVE_PEDOM) != 0){
            shub_get_sensors_data(SHUB_ACTIVE_PEDOM, data);
            shub_input_report_stepcnt(data);
        }
    }

#ifdef SHUB_SUSPEND
    shub_activate_exec(0, 0, 1);//update
#endif
    resumeBatchingProc();

    SHUB_DBG_TIME_END((SHUBIO<<8)+9998)
    return SHUB_RC_OK;
}
#endif

static int sensor_rep_show(struct seq_file *s, void *what)
{
    seq_printf(s, "[ml630q790 ]SensorEnable=0x%08x, Logging=0x%08x\n", atomic_read(&g_CurrentSensorEnable), atomic_read(&g_CurrentLoggingSensorEnable));
    seq_printf(s, "[ml630q790 ][Task]");
    seq_printf(s, "Sensor=%s(%dus), ", ((s_micon_param.task[0] != 0)?"on":"off"), ((s_micon_param.task[0] != 0)?(s_micon_param.task_cycle[0]*10):0));
    seq_printf(s, "App=%s(%dus), ",    ((s_micon_param.task[1] != 0)?"on":"off"), ((s_micon_param.task[1] != 0)?(s_micon_param.task_cycle[1]*10):0));
    seq_printf(s, "Fusion=%s(%dus)\n", ((s_micon_param.task[2] != 0)?"on":"off"), ((s_micon_param.task[2] != 0)?(s_micon_param.task_cycle[2]*10):0));
    seq_printf(s, "[ml630q790 ][NoBatch]");
    seq_printf(s, "Acc=%s(%dus), ",  (((s_micon_param.sensors & HC_ACC_VALID)  != 0)?"on":"off"), (((s_micon_param.sensors & HC_ACC_VALID)  != 0)?s_micon_param.s_cycle[0]:0));
    seq_printf(s, "Mag=%s(%dus), ",  (((s_micon_param.sensors & HC_MAG_VALID)  != 0)?"on":"off"), (((s_micon_param.sensors & HC_MAG_VALID)  != 0)?s_micon_param.s_cycle[2]:0));
    seq_printf(s, "Gyro=%s(%dus), ", (((s_micon_param.sensors & HC_GYRO_VALID) != 0)?"on":"off"), (((s_micon_param.sensors & HC_GYRO_VALID) != 0)?s_micon_param.s_cycle[3]:0));
    seq_printf(s, "Baro=%s(%dus)\n", (((s_micon_param.sensors & HC_BARO_VALID) != 0)?"on":"off"), (((s_micon_param.sensors & HC_BARO_VALID) != 0)?s_micon_param.s_cycle[1]:0));
    seq_printf(s, "[ml630q790 ]");
    seq_printf(s, "fusion=%d\n",     s_micon_param.fusion);
    seq_printf(s, "[ml630q790 ][Logging]");
    seq_printf(s, "Acc=%s(%dus), ",  (((s_micon_param.logg_sensors & HC_ACC_VALID)  != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_ACC_VALID)  != 0)?shub_logging_cycle.acc :0));
    seq_printf(s, "Mag=%s(%dus), ",  (((s_micon_param.logg_sensors & HC_MAG_VALID)  != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_MAG_VALID)  != 0)?shub_logging_cycle.mag :0));
    seq_printf(s, "Gyro=%s(%dus), ", (((s_micon_param.logg_sensors & HC_GYRO_VALID) != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_GYRO_VALID) != 0)?shub_logging_cycle.gyro:0));
    seq_printf(s, "Baro=%s(%dus)\n", (((s_micon_param.logg_sensors & HC_BARO_VALID) != 0)?"on":"off"), (((s_micon_param.logg_sensors & HC_BARO_VALID) != 0)?shub_logging_cycle.baro:0));
    seq_printf(s, "[ml630q790 ][Logging]");
    seq_printf(s, "orien=%s(%dus), ",    (((s_micon_param.logg_fusion & HC_ORI_VALID)        != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_ORI_VALID)        != 0)?shub_logging_cycle.orien   :0));
    seq_printf(s, "grav=%s(%dus), ",     (((s_micon_param.logg_fusion & HC_GRAVITY_VALID)    != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_GRAVITY_VALID)    != 0)?shub_logging_cycle.grav    :0));
    seq_printf(s, "linear=%s(%dus), ",   (((s_micon_param.logg_fusion & HC_LACC_VALID)       != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_LACC_VALID)       != 0)?shub_logging_cycle.linear  :0));
    seq_printf(s, "rot=%s(%dus), ",      (((s_micon_param.logg_fusion & HC_RV_VALID)         != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_RV_VALID)         != 0)?shub_logging_cycle.rot     :0));
    seq_printf(s, "rot_gyro=%s(%dus), ", (((s_micon_param.logg_fusion & HC_RV_NONMAG_VALID)  != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_RV_NONMAG_VALID)  != 0)?shub_logging_cycle.rot_gyro:0));
    seq_printf(s, "rot_mag=%s(%dus)\n",  (((s_micon_param.logg_fusion & HC_RV_NONGYRO_VALID) != 0)?"on":"off"),  (((s_micon_param.logg_fusion & HC_RV_NONGYRO_VALID) != 0)?shub_logging_cycle.rot_mag :0));
    seq_printf(s, "[ml630q790 ][Calibration]");
    seq_printf(s, "mag_cal=%d, ",     s_micon_param.mag_cal);
    seq_printf(s, "gyro_cal=%d, ",    s_micon_param.gyro_cal);
    seq_printf(s, "gyro_filter=%d\n", s_micon_param.gyro_filter);
    seq_printf(s, "[ml630q790 ]g_bIsIntIrqEnable=%d\n",           atomic_read(&g_bIsIntIrqEnable));
//  seq_printf(s, "[ml630q790 ]g_WakeupSensor=%x\n",              atomic_read(&g_WakeupSensor));
    seq_printf(s, "[ml630q790 ]g_FWUpdateStatus=%d\n",            atomic_read(&g_FWUpdateStatus));
    seq_printf(s, "[ml630q790 ]LowPowerMode=%d, ",                shub_lowpower_mode);
    seq_printf(s, "OperationMode=%d\n",                           shub_operation_mode);
    seq_printf(s, "[ml630q790 ]connect_flg=%d, ",                 shub_connect_flg);
    seq_printf(s, "fw_write_flg=%d, ",                            shub_fw_write_flg);
    seq_printf(s, "FailedInitParam=%d, ",                         shub_failed_init_param);
    seq_printf(s, "err_cmd=0x%x, ",                               (shub_err_code >> 16) & 0xFFFF);
    seq_printf(s, "err_code=0x%x\n",                              (shub_err_code & 0xFFFF));


    seq_printf(s, "[ml630q790 ]s_sensor_delay_us ");
    seq_printf(s, "acc=%d, ",          s_sensor_delay_us.acc);
    seq_printf(s, "mag=%d, ",          s_sensor_delay_us.mag);
    seq_printf(s, "mag_uc=%d, ",       s_sensor_delay_us.mag_uc);
    seq_printf(s, "gyro=%d, ",         s_sensor_delay_us.gyro);
    seq_printf(s, "gyro_uc=%d, ",      s_sensor_delay_us.gyro_uc);
    seq_printf(s, "baro=%d, ",         s_sensor_delay_us.baro);
    seq_printf(s, "orien=%d, ",        s_sensor_delay_us.orien);
    seq_printf(s, "grav=%d, ",         s_sensor_delay_us.grav);
    seq_printf(s, "linear=%d, ",       s_sensor_delay_us.linear);
    seq_printf(s, "rot=%d, ",          s_sensor_delay_us.rot);
    seq_printf(s, "rot_gyro=%d, ",     s_sensor_delay_us.rot_gyro);
    seq_printf(s, "rot_mag=%d, ",      s_sensor_delay_us.rot_mag);
    seq_printf(s, "pedocnt=%d, ",      s_sensor_delay_us.pedocnt);
    seq_printf(s, "total_status=%d, ", s_sensor_delay_us.total_status);
    seq_printf(s, "pedocnt2=%d, ",     s_sensor_delay_us.pedocnt2);
    seq_printf(s, "total_status2=%d\n",s_sensor_delay_us.total_status2);
    
    seq_printf(s, "[ml630q790 ]s_logging_delay_us ");
    seq_printf(s, "acc=%d, ",          s_logging_delay_us.acc);
    seq_printf(s, "mag=%d, ",          s_logging_delay_us.mag);
    seq_printf(s, "mag_uc=%d, ",       s_logging_delay_us.mag_uc);
    seq_printf(s, "gyro=%d, ",         s_logging_delay_us.gyro);
    seq_printf(s, "gyro_uc=%d, ",      s_logging_delay_us.gyro_uc);
    seq_printf(s, "baro=%d, ",         s_logging_delay_us.baro);
    seq_printf(s, "orien=%d, ",        s_logging_delay_us.orien);
    seq_printf(s, "grav=%d, ",         s_logging_delay_us.grav);
    seq_printf(s, "linear=%d, ",       s_logging_delay_us.linear);
    seq_printf(s, "rot=%d, ",          s_logging_delay_us.rot);
    seq_printf(s, "rot_gyro=%d, ",     s_logging_delay_us.rot_gyro);
    seq_printf(s, "rot_mag=%d, ",      s_logging_delay_us.rot_mag);
    seq_printf(s, "pedocnt=%d, ",      s_logging_delay_us.pedocnt);
    seq_printf(s, "total_status=%d, ", s_logging_delay_us.total_status);
    seq_printf(s, "pedocnt2=%d, ",     s_logging_delay_us.pedocnt2);
    seq_printf(s, "total_status2=%d\n",s_logging_delay_us.total_status2);
    
    seq_printf(s, "[ml630q790 ]shub_dbg_cnt ");
    seq_printf(s, "irq=%lld, ",        shub_dbg_cnt_irq);
    seq_printf(s, "cmd=%lld, ",        shub_dbg_cnt_cmd);
    seq_printf(s, "acc=%lld, ",        shub_dbg_cnt_acc);
    seq_printf(s, "mag=%lld, ",        shub_dbg_cnt_mag);
    seq_printf(s, "gyro=%lld, ",       shub_dbg_cnt_gyro);
    seq_printf(s, "baro=%lld, ",       shub_dbg_cnt_baro);
    seq_printf(s, "fusion=%lld, ",     shub_dbg_cnt_fusion);
    seq_printf(s, "cust=%lld, ",       shub_dbg_cnt_cust);
    seq_printf(s, "other=%lld\n",      shub_dbg_cnt_other);

    seq_printf(s, "[ml630q790 ]shub_acc_offset ");
    seq_printf(s, "[0]=%d, ",          shub_acc_offset[0]);
    seq_printf(s, "[1]=%d, ",          shub_acc_offset[1]);
    seq_printf(s, "[2]=%d\n",          shub_acc_offset[2]);

    seq_printf(s, "[ml630q790 ]shub_mag_axis_interfrence ");
    seq_printf(s, "[0]=%d, ",          shub_mag_axis_interfrence[0]);
    seq_printf(s, "[1]=%d, ",          shub_mag_axis_interfrence[1]);
    seq_printf(s, "[2]=%d, ",          shub_mag_axis_interfrence[2]);
    seq_printf(s, "[3]=%d, ",          shub_mag_axis_interfrence[3]);
    seq_printf(s, "[4]=%d, ",          shub_mag_axis_interfrence[4]);
    seq_printf(s, "[5]=%d, ",          shub_mag_axis_interfrence[5]);
    seq_printf(s, "[6]=%d, ",          shub_mag_axis_interfrence[6]);
    seq_printf(s, "[7]=%d, ",          shub_mag_axis_interfrence[7]);
    seq_printf(s, "[8]=%d\n",          shub_mag_axis_interfrence[8]);

    seq_printf(s, "[ml630q790 ]LatestACC  ");
    seq_printf(s, "X=%d, ",            s_tLatestAccData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestAccData.nY);
    seq_printf(s, "Z=%d\n",            s_tLatestAccData.nZ);
    
    seq_printf(s, "[ml630q790 ]LatestGYRO ");
    seq_printf(s, "X=%d, ",            s_tLatestGyroData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestGyroData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestGyroData.nZ);
    seq_printf(s, "OX=%d, ",           s_tLatestGyroData.nXOffset);
    seq_printf(s, "OY=%d, ",           s_tLatestGyroData.nYOffset);
    seq_printf(s, "OZ=%d, ",           s_tLatestGyroData.nZOffset);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestGyroData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestMAG  ");
    seq_printf(s, "X=%d, ",            s_tLatestMagData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestMagData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestMagData.nZ);
    seq_printf(s, "OX=%d, ",           s_tLatestMagData.nXOffset);
    seq_printf(s, "OY=%d, ",           s_tLatestMagData.nYOffset);
    seq_printf(s, "OZ=%d, ",           s_tLatestMagData.nZOffset);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestMagData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestBARO ");
    seq_printf(s, "Pressure=%d\n",     s_tLatestBaroData.pressure);
    
    seq_printf(s, "[ml630q790 ]LatestORI  ");
    seq_printf(s, "P=%d, ",            s_tLatestOriData.pitch);
    seq_printf(s, "R=%d, ",            s_tLatestOriData.roll);
    seq_printf(s, "Y=%d, ",            s_tLatestOriData.yaw);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestOriData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestGRV  ");
    seq_printf(s, "X=%d, ",            s_tLatestGravityData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestGravityData.nY);
    seq_printf(s, "Z=%d\n",            s_tLatestGravityData.nZ);

    seq_printf(s, "[ml630q790 ]LatestLACC ");
    seq_printf(s, "X=%d, ",            s_tLatestLinearAccData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestLinearAccData.nY);
    seq_printf(s, "Z=%d\n",            s_tLatestLinearAccData.nZ);

    seq_printf(s, "[ml630q790 ]LatestRv   ");
    seq_printf(s, "X=%d, ",            s_tLatestRVectData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestRVectData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestRVectData.nZ);
    seq_printf(s, "S=%d, ",            s_tLatestRVectData.nS);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestRVectData.nAccuracy);

    seq_printf(s, "[ml630q790 ]LatestGRV  ");
    seq_printf(s, "X=%d, ",            s_tLatestGeoRVData.nX);
    seq_printf(s, "Y=%d, ",            s_tLatestGeoRVData.nY);
    seq_printf(s, "Z=%d, ",            s_tLatestGeoRVData.nZ);
    seq_printf(s, "S=%d, ",            s_tLatestGeoRVData.nS);
    seq_printf(s, "Accuracy=%d\n",     s_tLatestGeoRVData.nAccuracy);
    
    seq_printf(s, "[ml630q790 ]LatestSTEP ");
    seq_printf(s, "S=%llu, ",          s_tLatestStepCountData.step);
    seq_printf(s, "OS=%llu\n",         s_tLatestStepCountData.stepOffset);

    shub_sensor_rep_input_exif(s);
    shub_sensor_rep_input_acc(s);
    shub_sensor_rep_input_gyro(s);
    shub_sensor_rep_input_gyro_uncal(s);
    shub_sensor_rep_input_mag(s);
    shub_sensor_rep_input_mag_uncal(s);
    shub_sensor_rep_input_orien(s);
    shub_sensor_rep_input_grav(s);
    shub_sensor_rep_input_linear(s);
    shub_sensor_rep_input_rot(s);
    shub_sensor_rep_input_game_rot_gyro(s);
    shub_sensor_rep_input_rot_mag(s);
    shub_sensor_rep_input_pedo(s);
    shub_sensor_rep_input_pedodect(s);
    shub_sensor_rep_input_mcu(s);
#ifdef CONFIG_BARO_SENSOR
    shub_sensor_rep_input_baro(s);
#endif
#ifdef CONFIG_HOSTIF_I2C
#endif
#ifdef CONFIG_HOSTIF_SPI
    shub_sensor_rep_spi(s);
#endif

    return 0;
}

static int sensor_rep_open(struct inode *inode, struct file *file)
{
    return single_open(file, sensor_rep_show, NULL);
}

static const struct file_operations sensor_rep_ops = {
    .open       = sensor_rep_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

int32_t shub_probe(void)
{
    SHUB_DBG_TIME_INIT
    int32_t ret;
    struct proc_dir_entry *entry;

    SHUB_DBG_TIME_START
    shub_wake_lock_init();
    shub_qos_init();
    accsns_wq_int = create_singlethread_workqueue("accsns_wq_int");
    if(!accsns_wq_int)
    {
        DBG(DBG_LV_ERROR, "can't create interrupt queue : accsns_wq_int \n");
        goto REGIST_ERR;
    }
    
    initBatchingProc();

    entry = proc_create("driver/sensor_rep", S_IRUGO, NULL, &sensor_rep_ops);
    if (!entry)
        DBG(DBG_LV_ERROR, "shub proc_create error\n");

    SHUB_DBG_TIME_END((SHUBIO<<8)+9997)
    
    g_nIntIrqNo = -1;
    g_nIntIrqFlg = 0;

    s_sensor_delay_us.acc       = SENSOR_ACC_MIN_DELAY;
    s_sensor_delay_us.mag       = SENSOR_MAG_MIN_DELAY;
    s_sensor_delay_us.mag_uc    = SENSOR_MAGUC_MIN_DELAY;
    s_sensor_delay_us.gyro      = SENSOR_GYRO_MIN_DELAY;
    s_sensor_delay_us.gyro_uc   = SENSOR_GYROUC_MIN_DELAY;
    s_sensor_delay_us.baro      = SENSOR_BARO_MIN_DELAY;
    s_sensor_delay_us.orien     = FUSION_MIN_DELAY;
    s_sensor_delay_us.grav      = FUSION_MIN_DELAY;
    s_sensor_delay_us.linear    = FUSION_MIN_DELAY;
    s_sensor_delay_us.rot       = FUSION_MIN_DELAY;
    s_sensor_delay_us.rot_gyro  = FUSION_MIN_DELAY;
    s_sensor_delay_us.rot_mag   = FUSION_MIN_DELAY;
    s_sensor_delay_us.pedocnt   = APP_MIN_DELAY;

    s_logging_delay_us.acc      = SENSOR_ACC_MIN_DELAY;
    s_logging_delay_us.mag      = SENSOR_MAG_MIN_DELAY;
    s_logging_delay_us.mag_uc   = SENSOR_MAGUC_MIN_DELAY;
    s_logging_delay_us.gyro     = SENSOR_GYRO_MIN_DELAY;
    s_logging_delay_us.gyro_uc  = SENSOR_GYROUC_MIN_DELAY;
    s_logging_delay_us.baro     = SENSOR_BARO_MIN_DELAY;
    s_logging_delay_us.orien    = FUSION_MIN_DELAY;
    s_logging_delay_us.grav     = FUSION_MIN_DELAY;
    s_logging_delay_us.linear   = FUSION_MIN_DELAY;
    s_logging_delay_us.rot      = FUSION_MIN_DELAY;
    s_logging_delay_us.rot_gyro = FUSION_MIN_DELAY;
    s_logging_delay_us.rot_mag  = FUSION_MIN_DELAY;
    s_logging_delay_us.pedocnt  = APP_MIN_DELAY;
    shub_logging_cycle = s_logging_delay_us;

    s_sensor_task_delay_us = SENSOR_TSK_DEFALUT_US;

    atomic_set(&g_CurrentSensorEnable,ACTIVE_OFF);
    atomic_set(&g_CurrentLoggingSensorEnable,ACTIVE_OFF);
    atomic_set(&g_WakeupSensor,ACTIVE_OFF);
    atomic_set(&g_FWUpdateStatus,false);

    memset(&s_tLatestAccData       , 0x00 , sizeof(s_tLatestAccData));
    memset(&s_tLatestGyroData      , 0x00 , sizeof(s_tLatestGyroData));
    memset(&s_tLatestMagData       , 0x00 , sizeof(s_tLatestMagData));
    memset(&s_tLatestBaroData      , 0x00 , sizeof(s_tLatestBaroData));
    memset(&s_tLoggingBaroData     , 0x00 , sizeof(s_tLoggingBaroData));
    memset(&s_tLatestOriData       , 0x00 , sizeof(s_tLatestOriData));
    memset(&s_tLatestGravityData   , 0x00 , sizeof(s_tLatestGravityData));
    memset(&s_tLatestLinearAccData , 0x00 , sizeof(s_tLatestLinearAccData));
    memset(&s_tLatestRVectData     , 0x00 , sizeof(s_tLatestRVectData));
    memset(&s_tLatestGameRVData    , 0x00 , sizeof(s_tLatestGameRVData));
    memset(&s_tLatestGeoRVData     , 0x00 , sizeof(s_tLatestGeoRVData));
    memset(&s_tLatestStepCountData   , 0x00 , sizeof(s_tLatestStepCountData));

    init_waitqueue_head(&s_tWaitInt);
    shub_workqueue_init();
    INIT_WORK(&acc_irq_work, shub_int_acc_work_func);
    INIT_WORK(&significant_work, shub_significant_work_func);
    INIT_WORK(&mag_irq_work, shub_int_mag_work_func);
    INIT_WORK(&customer_irq_work, shub_int_customer_work_func);
    INIT_WORK(&gyro_irq_work, shub_int_gyro_work_func);
    INIT_WORK(&fusion_irq_work, shub_int_fusion_work_func);

    mutex_init(&s_tDataMutex);
    mutex_init(&s_hostCmdMutex);
    mutex_init(&userReqMutex);

    spin_lock_init( &s_intreqData );

#ifndef NO_LINUX
    if(g_nIntIrqNo == -1){
        ret = shub_gpio_init();
        if(ret != SHUB_RC_OK){
            DBG(DBG_LV_ERROR, "Failed shub_gpio_init. ret=%x\n", ret);
            goto REGIST_ERR;
        }
        DISABLE_IRQ;
    }
#endif

    shub_qos_start();
    ret = shub_initialize();
    shub_qos_end();
    if(ret != SHUB_RC_OK) {
        DBG(DBG_LV_ERROR, "Failed shub_initialize. ret=%x\n", ret);
        return -ENODEV;
    }

    DBG(DBG_LV_INFO, "Init Complete!!!\n");
    return 0;
    
REGIST_ERR:
    if(accsns_wq_int != NULL){
        flush_workqueue(accsns_wq_int);
        destroy_workqueue(accsns_wq_int);
        accsns_wq_int = NULL;
    }
    
    return 0;
}

int32_t shub_remove(void)
{
    DISABLE_IRQ;
    
    shub_wake_lock_destroy();
    shub_qos_destroy();

    cancel_work_sync(&acc_irq_work);
    cancel_work_sync(&significant_work);
    cancel_work_sync(&fusion_irq_work);
    cancel_work_sync(&gyro_irq_work);
    cancel_work_sync(&mag_irq_work);
    cancel_work_sync(&customer_irq_work);

    if(accsns_wq_int != NULL){
        flush_workqueue(accsns_wq_int);
        destroy_workqueue(accsns_wq_int);
        accsns_wq_int = NULL;
    }

    shub_gpio_free(SHUB_GPIO_PIN_INT0);

#ifndef SHUB_SW_GPIO_PMIC
#ifdef USE_RESET_SIGNAL   
    shub_gpio_free(SHUB_GPIO_PIN_RESET);
#endif
#endif

#ifdef USE_REMAP_SIGNAL
    shub_gpio_free(SHUB_GPIO_PIN_BRMP);
#endif
    
    remove_proc_entry("sensor_rep", NULL);
    return 0;
}

#ifndef NO_LINUX
MODULE_AUTHOR("LAPIS SEMICONDUCTOR");
MODULE_DESCRIPTION("SensorHub Driver");
MODULE_LICENSE("GPL v2");
#endif

