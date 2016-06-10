/* shub-input_diag.c
 *
 * Copyright (C) 2014 Sharp Corporation
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input-polldev.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/poll.h>

#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <asm/uaccess.h> 
#include <soc/qcom/sh_smem.h>

#include "shub_io.h"
#include "ml630q790.h"

static int shub_diag_acc_self_test_initialize(void);
static int shub_diag_acc_self_test_check(int16_t* accData);
static int shub_diag_acc_self_test_average(int16_t data[][3]);

static int shub_diag_gyro_self_test_initialize(void);
static int shub_diag_gyro_self_test_check(int16_t* gyroData);
static int shub_diag_gyro_self_test_average(int16_t data[][3]);

#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_diag_log = 0;
module_param(shub_diag_log, int, 0600);
#define DBG_DIAG_IO(msg, ...) {                      \
    if(shub_diag_log)                                \
        printk("[shub][diag] " msg, ##__VA_ARGS__);  \
}
#else
#define DBG_DIAG_IO(msg, ...)
#endif

#define SHUB_DIAG_SHUB   0x01
#define SHUB_DIAG_6AXIS  0x02
#define SHUB_DIAG_MAG    0x03
#define SHUB_DIAG_BARO   0x04

#define HC_MCU_GET_VERSION              (0x0001u)
#define HC_MCU_ACC_SENSOR_REG           (0x0009u)
#define HC_MCU_GET_PORT_OUT             (0x0008u)
#define HC_MCU_SELF_TEST                (0x000Au)
#define HC_MCU_I2C_IO                   (0x000Cu)


#define SHUB_ABS(a)     ((a) > 0 ? (a) : -(a))
#define SHUB_INT_PIN_CHECK_RETRY_MAX    (10)
#define SHUB_INT_PIN_CHECK_TIME_US      (10000)

#define YAS537_REG_DIDR                 (0x80)
#define YAS537_REG_CMDR                 (0x81)
#define YAS537_REG_CONFR                (0x82)
#define YAS537_REG_INTRVLR              (0x83)
#define YAS537_REG_OXR                  (0x84)
#define YAS537_REG_OY1R                 (0x85)
#define YAS537_REG_OY2R                 (0x86)
#define YAS537_REG_AVRR                 (0x87)
#define YAS537_REG_HCKR                 (0x88)
#define YAS537_REG_LCKR                 (0x89)
#define YAS537_REG_SRSTR                (0x90)
#define YAS537_REG_ADCCALR              (0x91)
#define YAS537_REG_MTCR                 (0x93)
#define YAS537_REG_OCR                  (0x9e)
#define YAS537_REG_TRMR                 (0x9f)
#define YAS537_REG_DATAR                (0xb0)
#define YAS537_REG_CALR                 (0xc0)

#define YAS537_DATA_UNDERFLOW           (0)
#define YAS537_DATA_OVERFLOW            (16383)
#define YAS537_MAG_RCOIL_TIME           (65)
#define YAS537_SLAVE_ADDRESS            (0x2Eu)
#define YAS537_BUSY_RETRY_MAX           (10)
#define YAS537_RCOIL_RETRY_MAX          (1)
#define YAS537_RCOIL_RETRY_TIME_US      (1000000)

#define YAS_CAL_REG_NUM                 (17)
#define YAS_PCB_MEASURE_DATA_REG_NUM    (8)
#define YAS_PCB_MEASURE_COMMAND_START   (0x01)
#define YAS_PCB_MEASURE_COMMAND_LDTC    (0x02)
#define YAS_PCB_MEASURE_COMMAND_FORS    (0x04)

#define LSM6DS3_ACC_GYRO_INT1_CTRL      (0x0D)
#define LSM6DS3_ACC_GYRO_INT2_CTRL      (0x0E)
#define LSM6DS3_ACC_GYRO_CTRL3_C        (0x12)
#define LSM6DS3_ACC_GYRO_CTRL4_C        (0x13)

#define LSM6DS3_ACC_GYRO_CTRL1_XL       (0x10)
#define LSM6DS3_ACC_GYRO_CTRL8_XL       (0x17)
#define LSM6DS3_ACC_GYRO_CTRL9_XL       (0x18)
#define LSM6DS3_ACC_GYRO_TAP_CFG        (0x58)
#define LSM6DS3_ACC_GYRO_OUT_X_L_XL     (0x28)

#define LSM6DS3_ACC_GYRO_CTRL2_G        (0x11)
#define LSM6DS3_ACC_GYRO_CTRL6_G        (0x15)
#define LSM6DS3_ACC_GYRO_CTRL7_G        (0x16)
#define LSM6DS3_ACC_GYRO_CTR10_C        (0x19)
#define LSM6DS3_ACC_GYRO_OUT_X_L_G      (0x22) 

#define LSM6DS3_SLAVE_ADDRESS           (0x6Au)

#define LSM6DS3_ACC_GYRO_CTRL5_C        (0x14)
#define LSM6DS3_ACC_GYRO_OUT_X_H_XL     (0x29)
#define LSM6DS3_ACC_GYRO_OUT_Y_L_XL     (0x2A)
#define LSM6DS3_ACC_GYRO_OUT_Y_H_XL     (0x2B)
#define LSM6DS3_ACC_GYRO_OUT_Z_L_XL     (0x2C)
#define LSM6DS3_ACC_GYRO_OUT_Z_H_XL     (0x2D)
#define LSM6DS3_ACC_GYRO_STATUS_REG     (0x1E)
#define SELF_TEST_ACC_READTIMES (5)
#define SELF_TEST_GYRO_READTIMES (5)
#define CHECK_ACC_REG (1)
#define CHECK_GYRO_REG (2)
#define ACC_MIN_ST (1470)
#define ACC_MAX_ST (27870)
#define GYRO_MIN_ST (3570)
#define GYRO_MAX_ST (10000)

#define MAG_ERROR_DEFAULT           (0)
#define MAG_ERROR_ANOTHER           (-1)
#define MAG_ERROR_COIL_SETTING      (-2)
#define MAG_ERROR_MEASURE           (-3)
#define MAG_ERROR_CAL_VER           (-4)
#define MAG_ERROR_RETRY_OVER        (-5)
#define MAG_ERROR_INITIALIZE_ERROR  (-6)
#define MAG_ERROR_INITIALIZE2_ERROR (-7)

#define LPS22HB_INTERRUPT_CFG    (0x0B)
#define LPS22HB_CTRL_REG1        (0x10)
#define LPS22HB_CTRL_REG2        (0x11)
#define LPS22HB_CTRL_REG3        (0x12)
#define LPS22HB_FIFO_CTRL        (0x14)
#define LPS22HB_RES_CONF         (0x1A)
#define LPS22HB_PRESS_OUT_XL     (0x28)

#define LPS22HB_SLAVE_ADDRESS    (0x5Cu)

#ifdef CONFIG_ACC_U2DH
#define LIS2DH_OUT_X_L                  (0x28)
#define LIS2DH_OUT_X_H                  (0x29)
#define LIS2DH_OUT_Y_L                  (0x2A)
#define LIS2DH_OUT_Y_H                  (0x2B)
#define LIS2DH_OUT_Z_L                  (0x2C)
#define LIS2DH_OUT_Z_H                  (0x2D)
#endif

#define SHUB_ACC_SHIFT_VAL     2

struct yas_cal {
    int8_t a2, a3, a4, a6, a7, a8;
    int16_t a5, a9, cxy1y2[3];
    uint8_t k, ver;
};

static struct yas_cal ysaCalData;
static int16_t yas_overflow[3];
static int16_t yas_underflow[3];
static uint16_t ysa_last_after_rcoil[3];
static int ysa_rcoil_first_set_flg = 0;

static struct platform_device *pdev;
static struct input_dev *shub_idev;

static int read_cal_init_flg = 0;
static uint8_t calRegData[YAS_CAL_REG_NUM];
static int gyro_first_read_wait_flg = 1;
static int initial_gyro_flg = 0;
static int initial_acc_flg = 0;
static int initial_mag_flg = 0;
static int initial_baro_flg = 0;

static int32_t shub_probe_diag(struct platform_device *pfdev);
static int32_t shub_remove_diag(struct platform_device *pfdev);

#ifdef CONFIG_OF
    static struct of_device_id shub_of_match_tb_diag[] = {
        { .compatible = "sharp,shub_diag" ,},
        {}
    };
#else
    #define shub_of_match_tb_diag NULL
#endif

/* mag sensor read */
static int shub_diag_magSensorReadSendCmd(uint8_t addr, uint8_t* resData)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];

    memset(prm, 0, sizeof(prm));
    prm[0] = 0x06;
    prm[1] = 0x00;
    prm[2] = addr;
    ret = shub_direct_sendcmd(HC_MCU_ACC_SENSOR_REG, prm);
    shub_direct_get_error(&errRes);
    if((ret == 0) && (errRes == 0)) {
        shub_direct_get_result(resData);
    }else{
        printk("[shub]%s err.addr=%02x ret=%d errRes=%d\n", __func__, addr, ret, errRes);
        return -1;
    }

    return 0;
}

/* mag sensor write */
static int shub_diag_magSensorWriteSendCmd(uint8_t addr, uint8_t data)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];

    memset(prm, 0, sizeof(prm));
    prm[0] = 0x06;
    prm[1] = 0x01;
    prm[2] = addr;
    prm[3] = data;
    ret = shub_direct_sendcmd(HC_MCU_ACC_SENSOR_REG, prm);
    shub_direct_get_error(&errRes);
    if((ret != 0) || (errRes != 0)) {
        printk("[shub]%s err.addr=%02x data=%02x ret=%d errRes=%d\n", __func__, addr, data, ret, errRes);
        return -1;
    }

    return 0;
}

/* acc gyro sensor read */
static int shub_diag_accGyroSensorReadSendCmd(uint8_t addr, uint8_t* resData)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];

    memset(prm, 0, sizeof(prm));
    prm[0] = 0x00;
    prm[1] = 0x00;
    prm[2] = addr;
    ret = shub_direct_sendcmd(HC_MCU_ACC_SENSOR_REG, prm);
    shub_direct_get_error(&errRes);
    if((ret == 0) && (errRes == 0)) {
        shub_direct_get_result(resData);
    }else{
        printk("[shub]%s err.addr=%02x ret=%d errRes=%d\n", __func__, addr, ret, errRes);
        return -1;
    }

    return 0;
}

/* acc gyro sensor write */
static int shub_diag_accGyroSensorWriteSendCmd(uint8_t addr, uint8_t data)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];

    memset(prm, 0, sizeof(prm));
    prm[0] = 0x00;
    prm[1] = 0x01;
    prm[2] = addr;
    prm[3] = data;
    ret = shub_direct_sendcmd(HC_MCU_ACC_SENSOR_REG, prm);
    shub_direct_get_error(&errRes);
    if((ret != 0) || (errRes != 0)) {
        printk("[shub]%s err.addr=%02x data=%02x ret=%d errRes=%d\n", __func__, addr, data, ret, errRes);
        return -1;
    }

    return 0;
}

/* barometric pressure sensor read */
static int shub_diag_baroSensorReadSendCmd(uint8_t addr, uint8_t* resData)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[12];
    uint8_t Data[2];

    memset(prm, 0, sizeof(prm));
    prm[0] = LPS22HB_SLAVE_ADDRESS;
    prm[1] = 0x00;
    prm[2] = addr;
    prm[3] = 0x01;
    ret = shub_direct_sendcmd(HC_MCU_I2C_IO, prm);
    shub_direct_get_error(&errRes);
    if((ret == 0) && (errRes == 0)) {
//        shub_direct_get_result(resData);
        shub_direct_get_result(&Data[0]);
        resData[0] = Data[1];
    }else{
        printk("[shub]%s err.addr=%02x ret=%d errRes=%d\n", __func__, addr, ret, errRes);
        return -1;
    }

    return 0;
}

/* barometric pressure sensor write */
static int shub_diag_baroSensorWriteSendCmd(uint8_t addr, uint8_t data)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[12];

    memset(prm, 0, sizeof(prm));
    prm[0] = LPS22HB_SLAVE_ADDRESS;
    prm[1] = 0x01;
    prm[2] = addr;
    prm[3] = 1;
    prm[4] = data;
    ret = shub_direct_sendcmd(HC_MCU_I2C_IO, prm);
    shub_direct_get_error(&errRes);
    if((ret != 0) || (errRes != 0)) {
        printk("[shub]%s err.addr=%02x data=%02x ret=%d errRes=%d\n", __func__, addr, data, ret, errRes);
        return -1;
    }

    return 0;
}

/* sensor multi read */
static int shub_diag_sensorMultiReadSendCmd(uint8_t addr, uint8_t* resData, uint8_t slaveAddr, uint8_t size)
{
    int32_t ret;
    uint16_t errRes;
    uint8_t prm[16];
    uint8_t readSize = size;

    memset(prm, 0, sizeof(prm));
    prm[0] = slaveAddr;
    prm[1] = 0x00;
    prm[2] = addr;
    prm[3] = readSize;
    ret = shub_direct_sendcmd(HC_MCU_I2C_IO, prm);
    shub_direct_get_error(&errRes);
    if((ret == 0) && (errRes == 0)) {
        shub_direct_multi_get_result(resData);
    }
    else{
        printk("[shub]%s set err.addr=%02x ret=%d errRes=%d\n", __func__, addr, ret, errRes);
        return -1;
    }

    return 0;
}

/* mag initial setting */
static int shub_diag_mag_initialize(void)
{
    int ret;
    uint8_t setData;

    /* SRSTR Soft reset */
    setData = 0x02;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_SRSTR, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

/* coil data setting */
static int shub_diag_mag_set_coil(void)
{
    int ret;
    uint8_t setData = 0;
    
    /* CONFR Config set */
    setData = 0x09;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_CONFR, setData);
    if(ret != 0) {
        return -1;
    }
    usleep(YAS537_MAG_RCOIL_TIME);
    return 0;
}

/* mag initial2 setting */
static int shub_diag_mag_initialize_2(void)
{
    int ret;
    uint8_t setData;

    /* ADCCALR A/D rev */
    setData = 0x03;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_ADCCALR, setData);
    if(ret != 0) {
        return -1;
    }
    setData = 0xF8;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_ADCCALR + 1, setData);
    if(ret != 0) {
        return -1;
    }
    /* TRMR Temperature rev */
    setData = 0xFF;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_TRMR, setData);
    if(ret != 0) {
        return -1;
    }
    /* CONFR coil data setting */
    ret = shub_diag_mag_set_coil();
    if(ret != 0) {
        return -1;
    }
    /* INTRVLR Interval */
    setData = 0x00;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_INTRVLR, setData);
    if(ret != 0) {
        return -1;
    }
    /* AVRR Average filter */
    setData = 0x70;
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_AVRR, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

/* acc initial setting */
static int shub_diag_acc_initialize(void)
{
    int ret;
    uint8_t setData;

    /* INT1_CTRL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_INT1_CTRL, setData);
    if(ret != 0) {
        return -1;
    }
    /* INT2_CTRL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_INT2_CTRL, setData);
    if(ret != 0) {
        return -1;
    }
    /* Accelerometer axis output enable */
    setData = 0x38;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL9_XL, setData);
    if(ret != 0) {
        return -1;
    }
    /* HR mode HPF disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_TAP_CFG, setData);
    if(ret != 0) {
        return -1;
    }
    /* HP & LPF disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL8_XL, setData);
    if(ret != 0) {
        return -1;
    }
	/* Block data update enable */
    setData = 0x44;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL3_C, setData);
    if(ret != 0) {
        return -1;
    }
	/* DA timer disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL4_C, setData);
    if(ret != 0) {
        return -1;
    }
    
    return 0;
}

static int shub_diag_acc_self_test_initialize(void)
{
    int ret;
    uint8_t setData;

	/* CTRL1 setting */
	setData = 0x30;
	ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL1_XL, setData);
	if(ret != 0) {
        return -1;
    }
        
    /* CTRL2 setting */
	setData = 0x00;
	ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL2_G, setData);
	if(ret != 0) {
        return -1;
    }
    
	/* Block data update enable */
    setData = 0x44;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL3_C, setData);
    if(ret != 0) {
        return -1;
    }
    
	/* DA timer disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL4_C, setData);
    if(ret != 0) {
        return -1;
    }
    
	/* LSM6DS3_ACC_GYRO_CTRL5_C disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL5_C, setData);
    if(ret != 0) {
        return -1;
    }
    
	/* LSM6DS3_ACC_GYRO_CTRL6_G disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL6_G, setData);
    if(ret != 0) {
        return -1;
    }
    
	/* LSM6DS3_ACC_GYRO_CTRL7_G disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL7_G, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* HP & LPF disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL8_XL, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* Accelerometer axis output enable */
    setData = 0x38;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL9_XL, setData);
    if(ret != 0) {
        return -1;
    }
    
	/* LSM6DS3_ACC_GYRO_CTR10_C disable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTR10_C, setData);
    if(ret != 0) {
        return -1;
    }

    return 0;
}

/* gyro initial setting */
static int shub_diag_gyro_initialize(void)
{
    int ret;
    uint8_t setData;

    gyro_first_read_wait_flg = 1;

    /* INT1_CTRL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_INT1_CTRL, setData);
    if(ret != 0) {
        return -1;
    }
    /* INT2_CTRL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_INT2_CTRL, setData);
    if(ret != 0) {
        return -1;
    }
    /* Sensitive setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL6_G, setData);
    if(ret != 0) {
        return -1;
    }
    /* HPF enable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL7_G, setData);
    if(ret != 0) {
        return -1;
    }
    /* Gyroscope axis output enable */
    setData = 0x38;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTR10_C, setData);
    if(ret != 0) {
        return -1;
    }
    /* Block data update enable */
    setData = 0x44;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL3_C, setData);
    if(ret != 0) {
        return -1;
    }
    /* DA timer enabled */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL4_C, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

static int shub_diag_gyro_self_test_initialize(void)
{
    int ret;
    uint8_t setData;

    gyro_first_read_wait_flg = 1;

    /* CTRL1_XL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL1_XL, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* CTRL2_G setting */
    setData = 0x5C;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL2_G, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* Block data update enable */
    setData = 0x44;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL3_C, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* DA timer enabled */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL4_C, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* CTRL5_C setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL5_C, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* Sensitive setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL6_G, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* HPF enable */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL7_G, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* CTRL8_XL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL8_XL, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* CTRL9_XL setting */
    setData = 0x00;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL9_XL, setData);
    if(ret != 0) {
        return -1;
    }
    
    /* Gyroscope axis output enable */
    setData = 0x38;
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTR10_C, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

/* baro initial setting */
static int shub_diag_baro_initialize(void)
{
    int ret;
    uint8_t setData;

    /* INTERRUPT_CFG setting */
    setData = 0x00;
    ret = shub_diag_baroSensorWriteSendCmd(LPS22HB_INTERRUPT_CFG, setData);
    if(ret != 0) {
        return -1;
    }
    /* CTRL_REG2 setting */
    setData = 0x10;
    ret = shub_diag_baroSensorWriteSendCmd(LPS22HB_CTRL_REG2, setData);
    if(ret != 0) {
        return -1;
    }
    /* CTRL_REG3 setting */
    setData = 0x38;
    ret = shub_diag_baroSensorWriteSendCmd(LPS22HB_CTRL_REG3, setData);
    if(ret != 0) {
        return -1;
    }
    /* FIFO_CTRL setting */
    setData = 0x00;
    ret = shub_diag_baroSensorWriteSendCmd(LPS22HB_FIFO_CTRL, setData);
    if(ret != 0) {
        return -1;
    }
    /* RES_CONF setting */
    setData = 0x00;
    ret = shub_diag_baroSensorWriteSendCmd(LPS22HB_RES_CONF, setData);
    if(ret != 0) {
        return -1;
    }
    
    return 0;
}

/* cal data set */
static int shub_diag_mag_calc_setting(const uint8_t *pu08Data)
{
    int i;
    int ret;

    for (i = 0; i < 3; i++) {
        ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_MTCR + i, pu08Data[i]);
        if(ret != 0) {
            return -1;
        }
        ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_OXR + i, pu08Data[i + 12]);
        if(ret != 0) {
            return -1;
        }
    }
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_MTCR + i, (pu08Data[i] & 0xe0) | 0x10);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_HCKR, (pu08Data[15] >> 3) & 0x1e);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_LCKR, (pu08Data[15] << 1) & 0x1e);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_OCR, pu08Data[16] & 0x3f);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

/* cal data read */
static void shub_diag_mag_calc_correction(const uint8_t *pu08Data)
{
    int i;
//    int16_t a[3];

    ysaCalData.cxy1y2[0] = ((pu08Data[0] << 1) | (pu08Data[1] >> 7)) - 256;
    ysaCalData.cxy1y2[1] = (((pu08Data[1] << 2) & 0x1fc) | (pu08Data[2] >> 6)) - 256;
    ysaCalData.cxy1y2[2] = (((pu08Data[2] << 3) & 0x1f8) | (pu08Data[3] >> 5)) - 256;
    ysaCalData.a2 = (((pu08Data[3] << 2) & 0x7c) | (pu08Data[4] >> 6)) - 64;
    ysaCalData.a3 = (((pu08Data[4] << 1) & 0x7e) | (pu08Data[5] >> 7)) - 64;
    ysaCalData.a4 = (((pu08Data[5] << 1) & 0xfe) | (pu08Data[6] >> 7)) - 128;
    ysaCalData.a5 = (((pu08Data[6] << 2) & 0x1fc)| (pu08Data[7] >> 6)) - 112;
    ysaCalData.a6 = (((pu08Data[7] << 1) & 0x7e) | (pu08Data[8] >> 7)) - 64;
    ysaCalData.a7 = (((pu08Data[8] << 1) & 0xfe) | (pu08Data[9] >> 7)) - 128;
    ysaCalData.a8 = (pu08Data[9] & 0x7f) - 64;
    ysaCalData.a9 = (((pu08Data[10] << 1) & 0x1fe) | (pu08Data[11] >> 7)) - 112;
    ysaCalData.k = pu08Data[11] & 0x7f;
//    a[0] = 128;
//    a[1] = ysaCalData.a5;
//    a[2] = ysaCalData.a9;
    for (i = 0; i < 3; i++) {
//        yas_overflow[i] = 8192 + ysaCalData.k * a[i] * (8192 - SHUB_ABS(ysaCalData.cxy1y2[i]) * 325 / 16 - 192) / 8192;
//        yas_underflow[i] = 8192 - ysaCalData.k * a[i] * (8192 - SHUB_ABS(ysaCalData.cxy1y2[i]) * 325 / 16 - 192) / 8192;
//        if (YAS537_DATA_OVERFLOW < yas_overflow[i]) {
            yas_overflow[i] = YAS537_DATA_OVERFLOW;
//        }
//        if (yas_underflow[i] < YAS537_DATA_UNDERFLOW) {
            yas_underflow[i] = YAS537_DATA_UNDERFLOW;
//        }
    }
}

/* cal data read setting */
static int shub_diag_mag_read_cal(void)
{
    int i;
    int ret;
    int cal_valid = 1;
    uint8_t resData[2];

    if(read_cal_init_flg == 0) {
        cal_valid = 0;
        memset(calRegData, 0, sizeof(calRegData));
        memset(resData, 0, sizeof(resData));
        for(i = 0; i < YAS_CAL_REG_NUM; i++)
        {
            ret = shub_diag_magSensorReadSendCmd(YAS537_REG_CALR + i, resData);
            if(ret != 0) {
                printk("[shub]%s read err.\n", __func__);
                return -1;
            }
            calRegData[i] = resData[0];
        }

        /* cal register check */
        for (i = 0; i < YAS_CAL_REG_NUM; i++) {
            if(i < YAS_CAL_REG_NUM - 1) {
                if (calRegData[i] != 0x00) {
                    cal_valid = 1;
                }
            }
            else {
                if ((calRegData[i] & 0x3F) != 0x00) {
                    cal_valid = 1;
                }
            }
        }
        /* ysaCalData set */
        if (cal_valid) {
            ysaCalData.ver = calRegData[16] >> 6;
            if(ysaCalData.ver == 1) {
                shub_diag_mag_calc_correction(calRegData);
                read_cal_init_flg = 1;
            }
        }
    }

    /* cal register set */
    if (cal_valid) {
        if(ysaCalData.ver == 1) {
            ret = shub_diag_mag_calc_setting(calRegData);
            if(ret == 0) {
                return 0;
            }
        }
    }

    printk("[shub]%s check err. cal_valid=%d cal_ver=%d\n", __func__, cal_valid, ysaCalData.ver);
    return -1;
}

#ifndef CONFIG_ACC_U2DH
/* Accelerometer mode setting */
static int shub_diag_acc_mode_change(int mode)
{
    int ret;
    uint8_t setData;

    if(mode){
        /* power mode(Accelerometer data out) */
        setData = 0x7C;
    }
    else{
        /* power down mode */
        setData = 0x00;
    }
    /* Accelerometer mode setting */
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL1_XL, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}
#endif

/* Gyroscope mode setting */
static int shub_diag_gyro_mode_change(int mode)
{
    int ret;
    uint8_t setData;

    if(mode){
        /* power mode(Gyroscope data out) */
        setData = 0x7C;
    }
    else{
        /* power down mode */
        setData = 0x00;
        gyro_first_read_wait_flg = 1;
    }
    /* Gyroscope mode setting */
    ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL2_G, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

/* acc measure one time */
static int shub_diag_acc_measure(int16_t* accData)
{
    int ret;
    uint8_t mesData[9];

    memset(mesData, 0, sizeof(mesData));
    ret = shub_diag_sensorMultiReadSendCmd(LSM6DS3_ACC_GYRO_OUT_X_L_XL, mesData, LSM6DS3_SLAVE_ADDRESS, 6);
    if((ret != 0) || (mesData[0] != 0)) {
        printk("[shub]%s measurement read err.ret=%d %d\n", __func__, ret, mesData[0]);
        return -1;
    }
    accData[0] = (int16_t)( ((uint16_t)mesData[2] << 8) | (uint16_t)mesData[1]);
    accData[1] = (int16_t)( ((uint16_t)mesData[4] << 8) | (uint16_t)mesData[3]);
    accData[2] = (int16_t)( ((uint16_t)mesData[6] << 8) | (uint16_t)mesData[5]);

    return 0;
}

#ifdef CONFIG_ACC_U2DH
static int shub_diag_lis2dh_acc_measure(int16_t* accData)
{
    int ret;
    uint8_t mesData_xl[2];
    uint8_t mesData_xh[2];
    uint8_t mesData_yl[2];
    uint8_t mesData_yh[2];
    uint8_t mesData_zl[2];
    uint8_t mesData_zh[2];

    ret = shub_diag_accGyroSensorReadSendCmd(LIS2DH_OUT_X_L, mesData_xl);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_accGyroSensorReadSendCmd(LIS2DH_OUT_X_H, mesData_xh);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_accGyroSensorReadSendCmd(LIS2DH_OUT_Y_L, mesData_yl);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_accGyroSensorReadSendCmd(LIS2DH_OUT_Y_H, mesData_yh);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_accGyroSensorReadSendCmd(LIS2DH_OUT_Z_L, mesData_zl);
    if(ret != 0) {
        return -1;
    }
    ret = shub_diag_accGyroSensorReadSendCmd(LIS2DH_OUT_Z_H, mesData_zh);
    if(ret != 0) {
        return -1;
    }

    accData[0] = (int16_t) ( ((uint16_t)mesData_xh[0] << 8) | (uint16_t)mesData_xl[0]);
    accData[1] = (int16_t) ( ((uint16_t)mesData_yh[0] << 8) | (uint16_t)mesData_yl[0]);
    accData[2] = (int16_t) ( ((uint16_t)mesData_zh[0] << 8) | (uint16_t)mesData_zl[0]);

    return 0;
}
#endif

/* gyro measure one time */
static int shub_diag_gyro_measure(int16_t* gyroData)
{
    int ret;
    uint8_t mesData[9];

    memset(mesData, 0, sizeof(mesData));
    ret = shub_diag_sensorMultiReadSendCmd(LSM6DS3_ACC_GYRO_OUT_X_L_G, mesData, LSM6DS3_SLAVE_ADDRESS, 6);
    if((ret != 0) || (mesData[0] != 0)) {
        printk("[shub]%s measurement read err.ret=%d %d\n", __func__, ret, mesData[0]);
        return -1;
    }
    gyroData[0] = (int16_t)( ((uint16_t)mesData[2] << 8) | (uint16_t)mesData[1]);
    gyroData[1] = (int16_t)( ((uint16_t)mesData[4] << 8) | (uint16_t)mesData[3]);
    gyroData[2] = (int16_t)( ((uint16_t)mesData[6] << 8) | (uint16_t)mesData[5]);

    return 0;
}

/* collect mag initial setting */
static int shub_diag_mag_initialize_collect(void)
{
    int ret;
    /* mag initial setting */
    ret = shub_diag_mag_initialize();
    if(ret != 0) {
        return MAG_ERROR_INITIALIZE_ERROR;
    }
    /* cal data read setting */
    ret = shub_diag_mag_read_cal();
    if(ret != 0) {
        return MAG_ERROR_CAL_VER;
    }
    /* mag initial2 setting */
    ret = shub_diag_mag_initialize_2();
    if(ret != 0) {
        return MAG_ERROR_INITIALIZE2_ERROR;
    }
    return 0;
}

/* get_port_pa2_val */
static int shub_diag_mag_get_port_out_pa2(uint8_t *port_val)
{
    int32_t ret;
    uint8_t prm[16];
    uint8_t resData[8];

    memset(prm, 0, sizeof(prm));
    ret = shub_direct_sendcmd(HC_MCU_GET_PORT_OUT, prm);
    shub_direct_multi_get_result(resData);
    if(ret != 0) {
        printk("[shub]%s error ret=%d\n", __func__, ret);
        return -1;
    }
    *port_val = resData[2];
    DBG_DIAG_IO("shub_diag_mag_get_port_out_pa2 A0=%d,A1=%d,A2=%d,A3=%d\n", resData[0], resData[1], resData[2], resData[3]);
    return 0;
}

/* mag measure start */
static int shub_diag_mag_measure(uint8_t* mesData, uint8_t cmd, int portCheck)
{
    int ret;
    int busy;
    int busy_cnt;
    int port_cnt;
    uint8_t resData[2];
    uint8_t get_pa2_val;

    ret = shub_diag_magSensorWriteSendCmd(YAS537_REG_CMDR, cmd);
    if(ret != 0) {
        printk("[shub]%s measure start err.\n", __func__);
        return -1;
    }
    /* wait 2ms */
    usleep(2000);
    busy_cnt = 0;
    do {
        if(busy_cnt >= YAS537_BUSY_RETRY_MAX) {
            printk("[shub]%s measure busy retry over.\n", __func__);
            return -1;
        }
        if(portCheck) {
            /* INT PIN check "L" */
            port_cnt = 0;
            do {
                if(port_cnt >= SHUB_INT_PIN_CHECK_RETRY_MAX) {
                    printk("[shub]%s INT PIN is not LOW. retry out.\n", __func__);
                    return -1;
                }
                else {
                    if(port_cnt > 0) {
                        usleep(SHUB_INT_PIN_CHECK_TIME_US);
                    }
                    port_cnt++;
                }
                get_pa2_val = 255;
                ret = shub_diag_mag_get_port_out_pa2(&get_pa2_val);
                if(ret != 0) {
                    printk("[shub]%s get port out err.\n", __func__);
                    return -1;
                }
            } while(get_pa2_val != 0);
        }
        /* F_MB reg read */
        memset(resData, 0, sizeof(resData));
        ret = shub_diag_magSensorReadSendCmd(YAS537_REG_DATAR + 2, resData);
        if(ret != 0) {
            printk("[shub]%s F_MB reg read err.ret=%d\n", __func__, ret);
            return -1;
        }
        busy = resData[0] >> 7;
        busy_cnt++;
    } while(busy != 0);
    
    /* measure read */
    memset(mesData, 0, YAS_PCB_MEASURE_DATA_REG_NUM + 1);
    ret = shub_diag_sensorMultiReadSendCmd(YAS537_REG_DATAR, mesData, YAS537_SLAVE_ADDRESS, YAS_PCB_MEASURE_DATA_REG_NUM);
    if((ret != 0) || (mesData[0] != 0)) {
        printk("[shub]%s measurement read err.ret=%d %d\n", __func__, ret, mesData[0]);
        return -1;
    }

    return 0;
}

/* mag measure rcoil data check */
static int invalid_magnetic_field(uint16_t *cur, uint16_t *last)
{
    int16_t invalid_thresh[] = {1500, 1500, 1500};
    int i;
    int ret = 0;

    for (i = 0; i < 3; i++) {
        if(ysa_rcoil_first_set_flg) {
            if (invalid_thresh[i] < SHUB_ABS(cur[i] - last[i])) {
                DBG_DIAG_IO("invalid_magnetic_field i=%d,cur=%d,last%d,div=%d\n", i, cur[i], last[i], SHUB_ABS(cur[i] - last[i]));
                ret = 1;
            }
        }
        last[i] = cur[i];
    }
    ysa_rcoil_first_set_flg = 1;
    return ret;
}

/* magnetic field data measure */
static int shub_diag_mag_data_measure(int32_t* revData)
{
    int mes_cnt;
    int i;
    int ret;
    int rcoil;
    int ouflow_flg;
    int rcoil_setting = 0;
    uint8_t mesData[YAS_PCB_MEASURE_DATA_REG_NUM + 1];
//    uint16_t temperature;
    uint16_t xy1y2[3];
    int32_t h[3];
    int32_t s[3];
    
    for(mes_cnt = 0; mes_cnt <= YAS537_RCOIL_RETRY_MAX; mes_cnt++) {
        if(rcoil_setting) {
            DBG_DIAG_IO("---Mag measurement retry[%d](cause=%d)\n", mes_cnt, rcoil_setting);
            /* wait */
            usleep(YAS537_RCOIL_RETRY_TIME_US);
            /* coil data setting */
            ret = shub_diag_mag_set_coil();
            if(ret != 0) {
                printk("[shub]%s coil data setting err.\n", __func__);
                return MAG_ERROR_COIL_SETTING;
            }
            rcoil_setting = 0;
        }
        /* mag measure start */
        ret = shub_diag_mag_measure(mesData, YAS_PCB_MEASURE_COMMAND_START, 0);
        if(ret != 0) {
            return MAG_ERROR_MEASURE;
        }
        rcoil = (mesData[2+1] >> 6) & 0x01;
//        temperature = (uint16_t)((mesData[0+1] << 8) | mesData[1+1]);
        xy1y2[0] = (uint16_t)(((mesData[2+1] & 0x3f) << 8) | mesData[3+1]);
        xy1y2[1] = (uint16_t)((mesData[4+1] << 8) | mesData[5+1]);
        xy1y2[2] = (uint16_t)((mesData[6+1] << 8) | mesData[7+1]);
        /* rcoil check */
        if(rcoil == 0) {
            if(invalid_magnetic_field(xy1y2, ysa_last_after_rcoil)) {
                rcoil_setting = 1;
                continue ;
            }
        }
        /* raw data flow check */
        ouflow_flg = 0;
        for (i = 0; i < 3; i++) {
            if (yas_overflow[i] <= xy1y2[i]) {
                ouflow_flg = 1;
                break;
            }
            if (xy1y2[i] <= yas_underflow[i]) {
                ouflow_flg = 2;
                break;
            }
        }
        if(ouflow_flg == 1) {
            rcoil_setting = 2;
            continue ;
        }
        else if(ouflow_flg == 2) {
            rcoil_setting = 3;
            continue ;
        }
        DBG_DIAG_IO("Mag mes(raw   ),x=%d,y1=%d,y2=%d\n", xy1y2[0], xy1y2[1], xy1y2[2]);
        if (ysaCalData.ver == 1) {
            /* coordinate change  */
            for (i = 0; i < 3; i++) {
                s[i] = xy1y2[i] - 8192;
            }
            h[0] = (ysaCalData.k * (128*s[0] + ysaCalData.a2*s[1] + ysaCalData.a3*s[2])) / 8192;
            h[1] = (ysaCalData.k * (ysaCalData.a4*s[0] + ysaCalData.a5*s[1] + ysaCalData.a6*s[2])) / 8192;
            h[2] = (ysaCalData.k * (ysaCalData.a7*s[0] + ysaCalData.a8*s[1] + ysaCalData.a9*s[2])) / 8192;
            /* change data flow check */
            for (i = 0; i < 3; i++) {
                if (h[i] < -8192) {
                    h[i] = -8192;
                }
                if (8191 < h[i]) {
                    h[i] = 8191;
                }
                xy1y2[i] = h[i] + 8192;
            }
            /* xy1y2 to xyz(uT) change */
            revData[0] = ((xy1y2[0]) * 300) / 1000;
            revData[1] = ((xy1y2[1] - xy1y2[2]) * 1732 / 10) / 1000;
            revData[2] = ((-xy1y2[1] - xy1y2[2]) * 300) / 1000;

            DBG_DIAG_IO("Mag mes(rawchg),x=%d,y1=%d,y2=%d, mes(uT),x=%d,y=%d,z=%d\n", xy1y2[0], xy1y2[1], xy1y2[2], revData[0], revData[1], revData[2]);
            return 0;
        }
        else {
            printk("[shub]%s cal_ver=%d err.\n", __func__, ysaCalData.ver);
            return MAG_ERROR_CAL_VER;
        }
    }

    printk("[shub]%s Mag measure retry over.\n", __func__);
    return MAG_ERROR_RETRY_OVER;
}

/* acc data measure */
static int shub_diag_acc_data_measure(int16_t* revData)
{
    int ret;

#ifdef CONFIG_ACC_U2DH
    /* acc measure one time */
    ret = shub_diag_lis2dh_acc_measure(revData);
    if(ret != 0) {
        return -1;
    }
#else
    /* Accelerometer mode setting */
    ret = shub_diag_acc_mode_change(1);
    if(ret != 0) {
        return -1;
    }
    
    usleep(3500);
    
    /* acc measure one time */
    ret = shub_diag_acc_measure(revData);
    if(ret != 0) {
        return -1;
    }
    
    /* Accelerometer mode setting(power down) */
    ret = shub_diag_acc_mode_change(0);
    if(ret != 0) {
        return -1;
    }
#endif
    return 0;
}

/* gyro data measure */
static int shub_diag_gyro_data_measure(int16_t* revData)
{
    int ret;

    /* Gyroscope mode setting */
    ret = shub_diag_gyro_mode_change(1);
    if(ret != 0) {
        return -1;
    }
    
    if(gyro_first_read_wait_flg){
        gyro_first_read_wait_flg = 0;
        usleep(100000);
    }
    
    /* gyro measure one time */
    ret = shub_diag_gyro_measure(revData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

static int shub_diag_acc_self_test_check(int16_t* accData)
{
	int i;
	int ret;
	int dis_ret;
	int16_t out_nost[SELF_TEST_ACC_READTIMES + 1][3];
	int16_t out_st[SELF_TEST_ACC_READTIMES + 1][3];
	uint8_t setData;
	
	memset(out_nost, 0, sizeof(out_nost));
	memset(out_st, 0, sizeof(out_st));
	
	ret = shub_diag_acc_self_test_initialize();
	if(ret){
		printk("[%d][shub]%s  initialize  err\n", __LINE__, __func__);
		return -1;
	}
	
	msleep(200);	
	
	ret = shub_diag_acc_self_test_average(out_nost);
	if(ret){
		printk("[%d][shub]%s  test_average  nost  error\n", __LINE__, __func__);
		return -1;
	}
		
	setData = 0x01;
	ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL5_C, setData);
	if(ret != 0) {
        return -1;
    }
    
    msleep(200);
		
	ret = shub_diag_acc_self_test_average(out_st);
	if(ret){
		printk("[%d][shub]%s  test_average  st  error\n", __LINE__, __func__);
		return -1;
	}

	for(i=0;i < SELF_TEST_ACC_READTIMES; i++ ) {
		DBG_DIAG_IO("[%d]%s  out_nost[%d][0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, i, out_nost[i][0], out_nost[i][1], out_nost[i][2]);
	}
	for( i=0; i < SELF_TEST_ACC_READTIMES + 1; i++ ) {
		if( i == SELF_TEST_ACC_READTIMES) {
			DBG_DIAG_IO("[%d]%s  AVERAGE out_nost[0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, out_nost[i][0], out_nost[i][1], out_nost[i][2]);
			DBG_DIAG_IO("[%d]%s  AVERAGE out_st[0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, out_st[i][0], out_st[i][1], out_st[i][2]);
		} else{
		DBG_DIAG_IO("[%d]%s  out_st[%d][0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, i, out_st[i][0], out_st[i][1], out_st[i][2]);
		}
	}
	

	for( i = 0; i < 3; i++ ) {
		accData[i] = abs( out_st[SELF_TEST_ACC_READTIMES][i] - out_nost[SELF_TEST_ACC_READTIMES][i] );
		DBG_DIAG_IO("[%d]%s  accData[%d]:%d\n", __LINE__, __func__, i, accData[i]);
	}
		
	if (( ( ACC_MIN_ST <= accData[0] ) && ( ACC_MAX_ST >= accData[0] ) ) &&
		( ( ACC_MIN_ST <= accData[1] ) && ( ACC_MAX_ST >= accData[1] ) ) &&
		( ( ACC_MIN_ST <= accData[2] ) && ( ACC_MAX_ST >= accData[2] ) ) ) {
		/* OK */
        printk("[%d][shub]%s  self  test  OK.\n", __LINE__, __func__ );
        ret = 0;
	} else {
		/* NG */
        printk("[%d][shub]%s  self  test  NG.\n", __LINE__, __func__ );
        ret = -2;
	}

	DBG_DIAG_IO("%s min(ST_X)=%d, |OUTX_ST-OUTX_NOST|=%d, max(ST_X)=%d\n", __func__, ACC_MIN_ST, accData[0], ACC_MAX_ST );
	DBG_DIAG_IO("%s min(ST_Y)=%d, |OUTY_ST-OUTY_NOST|=%d, max(ST_Y)=%d\n", __func__, ACC_MIN_ST, accData[1], ACC_MAX_ST );
	DBG_DIAG_IO("%s min(ST_Z)=%d, |OUTZ_ST-OUTZ_NOST|=%d, max(ST_Z)=%d\n", __func__, ACC_MIN_ST, accData[2], ACC_MAX_ST );
	
	/* CTRL1 setting */
	dis_ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL1_XL, 0x00 );
	if(dis_ret != 0) {
		printk("[%d][shub]%s  CTRL1  disable_error\n", __LINE__, __func__);
        return dis_ret;
    }

	/* LSM6DS3_ACC_GYRO_CTRL5_C disable */
    dis_ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL5_C, 0x00);
    if(dis_ret != 0) {
        printk("[%d][shub]%s CTRL5 disable_error\n", __LINE__, __func__);
        return dis_ret;
    }
    
    dis_ret = shub_diag_acc_initialize();
	if(dis_ret != 0){
		printk("[%d][shub]%s  end_initialize  err\n", __LINE__, __func__);
		return dis_ret;
	}
    return ret;
}

static int shub_diag_acc_self_test_average(int16_t data[][3])
{
	int acc_cnt = 0;
	int ret;
	uint8_t readData[2];
	int32_t sumData[3];
	uint8_t check_init_flg = 1;
	
	memset(sumData, 0, sizeof(sumData));
		
	while(check_init_flg){
		
		ret = shub_diag_accGyroSensorReadSendCmd(LSM6DS3_ACC_GYRO_STATUS_REG,readData);
		
		if(CHECK_ACC_REG & readData[0]){
			ret = shub_diag_acc_measure(data[acc_cnt]);
			if(ret){
				printk("[%d][shub]%s  acc_measure  err\n", __LINE__, __func__);
				return -1;
			}
			check_init_flg = 0;
		}
	}
	
	while(acc_cnt != SELF_TEST_ACC_READTIMES){
		
		ret = shub_diag_accGyroSensorReadSendCmd(LSM6DS3_ACC_GYRO_STATUS_REG,readData);
		
		if(CHECK_ACC_REG & readData[0]){
			ret = shub_diag_acc_measure(data[acc_cnt]);
			if(ret){
				printk("[%d][shub]%s  acc_measure  err\n", __LINE__, __func__);
				return -1;
			}
			
			sumData[0] += data[acc_cnt][0];
			sumData[1] += data[acc_cnt][1];
			sumData[2] += data[acc_cnt][2];
			acc_cnt++;
		}
	}
	
	data[SELF_TEST_ACC_READTIMES][0] = sumData[0] / SELF_TEST_ACC_READTIMES;
	data[SELF_TEST_ACC_READTIMES][1] = sumData[1] / SELF_TEST_ACC_READTIMES;
	data[SELF_TEST_ACC_READTIMES][2] = sumData[2] / SELF_TEST_ACC_READTIMES;
	
	return 0;
}

static int shub_diag_gyro_self_test_check(int16_t* gyroData)
{
	int i;
	int ret;
	int dis_ret;
	int16_t out_nost[SELF_TEST_GYRO_READTIMES + 1][3];
	int16_t out_st[SELF_TEST_GYRO_READTIMES + 1][3];
	uint8_t setData;
	
	memset(out_nost, 0, sizeof(out_nost));
	memset(out_st, 0, sizeof(out_st));
	
	ret = shub_diag_gyro_self_test_initialize();
	if(ret){
		printk("[%d][shub]%s  initialize  err\n", __LINE__, __func__);
		return -1;
	}
	
	msleep(800);
	
	ret = shub_diag_gyro_self_test_average(out_nost);
	if(ret){
		printk("[%d][shub]%s  test_average  nost  error\n", __LINE__, __func__);
		return -1;
	}
	
	setData = 0x04;
	ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL5_C, setData);
	if(ret != 0) {
        return -1;
    }
    
    msleep(60);
	
	ret = shub_diag_gyro_self_test_average(out_st);
	if(ret){
		printk("[%d][shub]%s  test_average  error\n", __LINE__, __func__);
		return -1;
	}

	for(i=0;i < SELF_TEST_GYRO_READTIMES; i++ ) {
		DBG_DIAG_IO("[%d]%s  out_nost[%d][0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, i, out_nost[i][0], out_nost[i][1], out_nost[i][2]);
	}
	for(i=0;i < SELF_TEST_GYRO_READTIMES + 1; i++ ) {
		if(SELF_TEST_GYRO_READTIMES) {
			DBG_DIAG_IO("[%d]%s  AVERAGE out_nost[0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, out_nost[i][0], out_nost[i][1], out_nost[i][2]);
			DBG_DIAG_IO("[%d]%s  AVERAGE out_st[0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, out_st[i][0], out_st[i][1], out_st[i][2]);
		} else{
		DBG_DIAG_IO("[%d]%s  out_st[%d][0]:%d  [1]:%d  [2]:%d\n", __LINE__, __func__, i, out_st[i][0], out_st[i][1], out_st[i][2]);
		}
	}
	
	
	for( i = 0; i < 3; i++ ) {
		gyroData[i] = abs( out_st[SELF_TEST_GYRO_READTIMES][i] - out_nost[SELF_TEST_GYRO_READTIMES][i] );
		DBG_DIAG_IO("[%d]%s  gyroData[%d]:%d\n", __LINE__, __func__, i, gyroData[i]);
	}
	
	if (( ( GYRO_MIN_ST <= gyroData[0] ) && ( GYRO_MAX_ST >= gyroData[0] ) ) &&
		( ( GYRO_MIN_ST <= gyroData[1] ) && ( GYRO_MAX_ST >= gyroData[1] ) ) &&
		( ( GYRO_MIN_ST <= gyroData[2] ) && ( GYRO_MAX_ST >= gyroData[2] ) ) ) {
		/* OK */
        printk("[%d][shub]%s self test OK.\n", __LINE__, __func__ );
        ret = 0;
	} else {
		/* NG */
        printk("[%d][shub]%s self test NG.\n", __LINE__, __func__ );
        ret = -2;
	}
	
	DBG_DIAG_IO("%s min(ST_X)=%d, |OUTX_ST-OUTX_NOST|=%d, max(ST_X)=%d\n", __func__, GYRO_MIN_ST, gyroData[0], GYRO_MAX_ST );
	DBG_DIAG_IO("%s min(ST_Y)=%d, |OUTY_ST-OUTY_NOST|=%d, max(ST_Y)=%d\n", __func__, GYRO_MIN_ST, gyroData[1], GYRO_MAX_ST );
	DBG_DIAG_IO("%s min(ST_Z)=%d, |OUTZ_ST-OUTZ_NOST|=%d, max(ST_Z)=%d\n", __func__, GYRO_MIN_ST, gyroData[2], GYRO_MAX_ST );
		
	/* CTRL2 setting */
	dis_ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL2_G, 0x00 );
	if(dis_ret != 0) {
        printk("[%d][shub]%s  CTRL2  disable_error\n", __LINE__, __func__);
        return dis_ret;
    }
    
	/* LSM6DS3_ACC_GYRO_CTRL5_C disable */
    dis_ret = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL5_C, 0x00);
    if(dis_ret != 0) {
        printk("[%d][shub]%s  CTRL5  disable_error\n", __LINE__, __func__);
        return dis_ret;
    }
    
    dis_ret = shub_diag_gyro_initialize();
	if(dis_ret){
		printk("[%d][shub]%s  end_initialize  err\n", __LINE__, __func__);
		return dis_ret;
	}
    return ret;
}

static int shub_diag_gyro_self_test_average(int16_t data[][3])
{
	int gyro_cnt = 0;
	int ret;
	uint8_t readData[2];
	int32_t sumData[3];
	uint8_t check_init_flg = 1;
	
	memset(sumData, 0, sizeof(sumData));
		
	while(check_init_flg){
		
	ret = shub_diag_accGyroSensorReadSendCmd(LSM6DS3_ACC_GYRO_STATUS_REG,readData);
		
		if(CHECK_GYRO_REG & readData[0]){
			ret = shub_diag_gyro_measure(data[gyro_cnt]);
			if(ret){
				printk("[%d][shub]%s  gyro_measure  err\n", __LINE__, __func__);
				return -1;
			}
			check_init_flg = 0;
		}
	}
	
	while(gyro_cnt != SELF_TEST_GYRO_READTIMES){
		
		ret = shub_diag_accGyroSensorReadSendCmd(LSM6DS3_ACC_GYRO_STATUS_REG,readData);
		
		if(CHECK_GYRO_REG & readData[0]){
			ret = shub_diag_gyro_measure(data[gyro_cnt]);
			if(ret){
				printk("[%d][shub]%s  acc_measure  err\n", __LINE__, __func__);
				return -1;
			}
			
			sumData[0] += data[gyro_cnt][0];
			sumData[1] += data[gyro_cnt][1];
			sumData[2] += data[gyro_cnt][2];
			gyro_cnt++;
		}
	}
	
	data[SELF_TEST_GYRO_READTIMES][0] = sumData[0] / SELF_TEST_GYRO_READTIMES;
	data[SELF_TEST_GYRO_READTIMES][1] = sumData[1] / SELF_TEST_GYRO_READTIMES;
	data[SELF_TEST_GYRO_READTIMES][2] = sumData[2] / SELF_TEST_GYRO_READTIMES;
	
	return 0;
}

/* self test check */
static int shub_diag_mag_self_test_check(int32_t *data)
{
    int ret;
    int sx = 0;
    int sy = 0;
    int port_cnt;
    uint8_t get_pa2_val;
    uint8_t cmd;
    uint8_t mesData[YAS_PCB_MEASURE_DATA_REG_NUM + 1];
    int32_t dataP[3];
    int32_t dataM[3];

    /* mag initial setting */
    ret = shub_diag_mag_initialize();
    if(ret != 0) {
        return -1;
    }
    /* cal data read setting */
    ret = shub_diag_mag_read_cal();
    if(ret != 0) {
        return -1;
    }
    /* mag initial2 setting */
    ret = shub_diag_mag_initialize_2();
    if(ret != 0) {
        return -1;
    }
    /* INT PIN check "H" */
    port_cnt = 0;
    do {
        if(port_cnt >= SHUB_INT_PIN_CHECK_RETRY_MAX) {
            printk("[shub]%s INT PIN is not HIGH. retry out.\n", __func__);
            return -1;
        }
        else {
            if(port_cnt > 0) {
                usleep(SHUB_INT_PIN_CHECK_TIME_US);
            }
            port_cnt++;
        }
        get_pa2_val = 255;
        ret = shub_diag_mag_get_port_out_pa2(&get_pa2_val);
        if(ret != 0) {
            printk("[shub]%s get port out err.\n", __func__);
            return -1;
        }
    } while(get_pa2_val != 1);

    /* mag measure plus */
    cmd = YAS_PCB_MEASURE_COMMAND_START | YAS_PCB_MEASURE_COMMAND_LDTC;
    ret = shub_diag_mag_measure(mesData, cmd, 1);
    if(ret != 0) {
        return -1;
    }
    dataP[0] = (uint16_t)(((mesData[2+1] & 0x3f) << 8) | mesData[3+1]);
    dataP[1] = (uint16_t)((mesData[4+1] << 8) | mesData[5+1]);
    dataP[2] = (uint16_t)((mesData[6+1] << 8) | mesData[7+1]);
    /* mag measure minus */
    cmd = YAS_PCB_MEASURE_COMMAND_START | YAS_PCB_MEASURE_COMMAND_LDTC | YAS_PCB_MEASURE_COMMAND_FORS;
    ret = shub_diag_mag_measure(mesData, cmd, 0);
    if(ret != 0) {
        return -1;
    }
    dataM[0] = (uint16_t)(((mesData[2+1] & 0x3f) << 8) | mesData[3+1]);
    dataM[1] = (uint16_t)((mesData[4+1] << 8) | mesData[5+1]);
    dataM[2] = (uint16_t)((mesData[6+1] << 8) | mesData[7+1]);
    
    if (ysaCalData.ver == 1) {
        /* MHx = 0.3 x (k/8192) x 128 x (Xp-Xm) */
        /* NHX = MHx / 1.8 */
        sx = ysaCalData.k * 128 * (dataP[0] - dataM[0]) / 8192 * 300;
        sx /= 1000;
        sx = (sx * 10) / 18;
        /* MHy = 0.3 x (k/8192) x (a5 x (Y1p-Y1m)-a9 x (Y2p-Y2m))/sqrt(3) */
        /* NHY = MHy / 1.8 */
        sy = ysaCalData.k * (ysaCalData.a5 * (dataP[1] - dataM[1]) - ysaCalData.a9 * (dataP[2] - dataM[2])) / 8192 * 1732 / 10;
        sy /= 1000;
        sy = (sy * 10) / 18;
    }
    else {
        printk("[shub]%s cal_ver=%d err.\n", __func__, ysaCalData.ver);
    }
    /* 24<=NHX 31<=NHY */
    if((sx >= 24) && (sy >= 31)){
        /* OK */
        printk("[shub]%s self test OK. sx=%d sy=%d\n", __func__, sx, sy);
        ret = 0;
    }
    else{
        /* NG */
        printk("[shub]%s self test NG. sx=%d sy=%d\n", __func__, sx, sy);
        ret = -2;
    }
    data[0] = sx;
    data[1] = sy;
    return ret;
}

/* Barometric pressure mode setting */
static int shub_diag_baro_mode_change(int mode)
{
    int ret;
    uint8_t setData;

    if(mode){
        /* normal mode */
        setData = 0x32;
    }
    else{
        /* power down mode */
        setData = 0x02;
    }
    /* Barometric pressure mode setting */
    ret = shub_diag_baroSensorWriteSendCmd(LPS22HB_CTRL_REG1, setData);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

/* baro measure one time */
static int shub_diag_baro_measure(int32_t* baroData)
{
    int ret;
    uint8_t mesData[9];

    memset(mesData, 0, sizeof(mesData));
    ret = shub_diag_sensorMultiReadSendCmd(LPS22HB_PRESS_OUT_XL, mesData, LPS22HB_SLAVE_ADDRESS, 5);
    if((ret != 0) || (mesData[0] != 0)) {
        printk("[shub]%s measurement read err.ret=%d %d\n", __func__, ret, mesData[0]);
        return -1;
    }
    baroData[0] = (int32_t)((mesData[3] << 16) | (mesData[2] << 8) | mesData[1]);
    baroData[1] = (int32_t)((mesData[5] << 8) | mesData[4]);

    return 0;
}

/* baro data measure */
static int shub_diag_baro_data_measure(int32_t* revData)
{
    int ret;

    /* Barometric pressure mode setting */
    ret = shub_diag_baro_mode_change(1);
    if(ret != 0) {
        return -1;
    }

    usleep(15000);

    /* baro measure one time */
    ret = shub_diag_baro_measure(revData);
    if(ret != 0) {
        return -1;
    }

    /* Barometric mode setting(power down) */
    ret = shub_diag_baro_mode_change(0);
    if(ret != 0) {
        return -1;
    }
    return 0;
}

static int shub_diag_baro_hwtest_data_measure(int32_t* revData)
{
    int ret;

    /* baro measure one time */
    ret = shub_diag_baro_measure(revData);
    if(ret != 0) {
        return -1;
    }

    return 0;
}

/* fw_get_version */
static int shub_diag_fw_get_version(uint8_t *arg_iData)
{
    int32_t ret;
    uint8_t prm[16];
    uint8_t resData[8];

    memset(prm, 0, sizeof(prm));
    ret = shub_direct_sendcmd(HC_MCU_GET_VERSION, prm);
    shub_direct_multi_get_result(resData);
    if(ret != 0) {
        printk("[shub]%s error ret=%d\n", __func__, ret);
        return -1;
    }

    arg_iData[0] = resData[0];
    arg_iData[1] = resData[1];
    arg_iData[2] = resData[2];
    arg_iData[3] = resData[3];
    arg_iData[4] = resData[4];
    arg_iData[5] = resData[5];
    arg_iData[6] = resData[6];
    arg_iData[7] = resData[7];

    printk("FW Version=%02x %02x %02x %02x %02x %02x %02x %02x\n",
        arg_iData[0], arg_iData[1], arg_iData[2], arg_iData[3], arg_iData[4], arg_iData[5], arg_iData[6], arg_iData[7]);
    return 0;
}

/* fw_checksum */
static int shub_diag_fw_checksum(void)
{
    int32_t ret;
    uint8_t prm[16];
    uint8_t resData[2];

    memset(prm, 0, sizeof(prm));
    ret = shub_direct_sendcmd(HC_MCU_SELF_TEST, prm);
    shub_direct_get_result(resData);
    if(ret != 0) {
        printk("[shub]%s error ret=%d\n", __func__, ret);
        ret = -1;
    }
    else if(resData[0] == 0x01) {
        printk("[shub]%s Checksum error.\n", __func__);
        ret = -2;
    }

    return ret;
}

static long shub_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int32_t ret = -1;
    
//    printk("[shub]shub_ioctl cmd=%d\n", cmd);
    switch (cmd) {
        case SHUB_DIAG_GET_REG:
            {
                uint8_t readData[2];
                struct IoctlDiagRes res;
                
                ret = copy_from_user(&res,argp,sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_GET_REG)\n");
                    return -EFAULT;
                }
                if(res.addr == SHUB_DIAG_6AXIS){
                    res.rtn = shub_diag_accGyroSensorReadSendCmd(res.reg, readData);
                }else if(res.addr == SHUB_DIAG_MAG){
                    res.rtn = shub_diag_magSensorReadSendCmd(res.reg, readData);
                }else if(res.addr == SHUB_DIAG_BARO){
                    res.rtn = shub_diag_baroSensorReadSendCmd(res.reg, readData);
                }else if(res.addr == SHUB_DIAG_SHUB){
                    res.rtn = shub_hostif_read(res.reg, readData, 1);
                }else{
                    res.rtn = -10;
                }
                res.data = readData[0];
                
                DBG_DIAG_IO("ioctl(cmd = Get_Reg) : addr=%d, data=...\n", res.addr);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_GET_REG)\n");
                    return -EFAULT;
                }
            }
            break;

        case SHUB_DIAG_SET_REG:
            {
                struct IoctlDiagRes res;
                
                ret = copy_from_user(&res,argp,sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_SET_REG)\n");
                    return -EFAULT;
                }

                DBG_DIAG_IO("ioctl(cmd = Set_Reg) : addr=%d, data=...\n", res.addr);
                if(res.addr == SHUB_DIAG_6AXIS){
                    res.rtn = shub_diag_accGyroSensorWriteSendCmd(res.reg, res.data);
                }else if(res.addr == SHUB_DIAG_MAG){
                    res.rtn = shub_diag_magSensorWriteSendCmd(res.reg, res.data);
                }else if(res.addr == SHUB_DIAG_BARO){
                    res.rtn = shub_diag_baroSensorWriteSendCmd(res.reg, res.data);
                }else if(res.addr == SHUB_DIAG_SHUB){
                    res.rtn = shub_hostif_write(res.reg, &res.data, 1);
                }else{
                    res.rtn = -10;
                }
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_SET_REG)\n");
                    return -EFAULT;
                }
            }
            break;
            
        case SHUB_DIAG_GET_PORT_STATE:
            {
                int gpio_state = 0xFF;
                
                ret = shub_get_int_state(&gpio_state);
                if(ret != 0) {
                    return -EFAULT;
                }
                DBG_DIAG_IO("ioctl(cmd = Get_Port_State) : state=%d\n", gpio_state);
                ret = copy_to_user(argp, &gpio_state, sizeof(gpio_state));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_GET_PORT_STATE)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_GYRO:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                res.rtn = shub_diag_gyro_initialize();
                if(res.rtn == 0){
                    res.rtn = shub_diag_gyro_data_measure(&res.accGyroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Gyro) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0],res.accGyroData[1],res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_GYRO)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_ACC:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                
                res.rtn = shub_diag_acc_initialize();
                if(res.rtn == 0){
                    res.rtn = shub_diag_acc_data_measure(&res.accGyroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Acc) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0],res.accGyroData[1],res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_ACC)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_MAG:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                
                res.rtn = shub_diag_mag_initialize_collect();
                if(res.rtn == 0){
                    res.rtn = shub_diag_mag_data_measure(&res.magData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Mag) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.magData[0],res.magData[1],res.magData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_MAG)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_MAG_SELF_TEST:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                
                res.rtn = shub_diag_mag_self_test_check(&res.magData[0]);
                DBG_DIAG_IO("ioctl(cmd = Mag_Self_Test) : ret=%d, val=0x%04x, 0x%04x\n", res.rtn, res.magData[0], res.magData[1]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_MAG)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_GYRO_CONT:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_gyro_flg){
                    res.rtn = shub_diag_gyro_initialize();
                    if(res.rtn == 0){
                        initial_gyro_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_gyro_data_measure(&res.accGyroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Gyro_Cont) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0],res.accGyroData[1],res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_GYRO_CONT)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_ACC_CONT:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
#ifndef CONFIG_ACC_U2DH
                if(!initial_acc_flg){
                    res.rtn = shub_diag_acc_initialize();
                    if(res.rtn == 0){
                        initial_acc_flg = 1;
                    }
                }
#endif
                if(res.rtn == 0){
                    res.rtn = shub_diag_acc_data_measure(&res.accGyroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Acc_Cont) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0],res.accGyroData[1],res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_ACC_CONT)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_MAG_CONT:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_mag_flg){
                    res.rtn = shub_diag_mag_initialize_collect();
                    if(res.rtn == 0){
                        initial_mag_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_mag_data_measure(&res.magData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Mag_Cont) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.magData[0],res.magData[1],res.magData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_MAG_CONT)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_STOP_GYRO:
            {
                struct IoctlDiagRes res;
                
                DBG_DIAG_IO("ioctl(cmd = Stop_Gyro)\n"); 
                initial_gyro_flg = 0;
                gyro_first_read_wait_flg = 1;
                
                res.rtn = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL2_G, 0x00);
                
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_STOP_GYRO)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_STOP_ACC:
            {
                struct IoctlDiagRes res;
                
                DBG_DIAG_IO("ioctl(cmd = Stop_Acc)\n"); 
                initial_acc_flg = 0;
                
                res.rtn = shub_diag_accGyroSensorWriteSendCmd(LSM6DS3_ACC_GYRO_CTRL1_XL, 0x00);
                
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_STOP_ACC)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_STOP_BARO:
            {
                struct IoctlDiagRes res;
                
                DBG_DIAG_IO("ioctl(cmd = Stop_Baro)\n"); 
                initial_baro_flg = 0;
                
                res.rtn = shub_diag_baroSensorWriteSendCmd(LPS22HB_CTRL_REG1, 0x02);
                
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_STOP_BARO)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_SENSOR_INIT:
            {
                struct IoctlDiagRes res;
                DBG_DIAG_IO("ioctl(cmd = Sensor_Init)\n");
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                ysa_rcoil_first_set_flg = 0;
                gyro_first_read_wait_flg = 1;
                read_cal_init_flg = 0;
                initial_gyro_flg = 0;
                initial_acc_flg = 0;
                initial_mag_flg = 0;
                initial_baro_flg = 0;
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_SENSOR_INIT)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MAG_READ_CAL:
            {
                struct IoctlDiagMagCalData res;
                memset(&res, 0, sizeof(struct IoctlDiagMagCalData));
                if(read_cal_init_flg == 0){
                    res.rtn = shub_diag_mag_initialize();
                    if(res.rtn == 0){
                        res.rtn = shub_diag_mag_read_cal();
                    }
                }
                if(res.rtn == 0){
                    res.s32Cx = ysaCalData.cxy1y2[0];
                    res.s32Cy1 = ysaCalData.cxy1y2[1];
                    res.s32Cy2 = ysaCalData.cxy1y2[2];
                    res.s32A2 = ysaCalData.a2;
                    res.s32A3 = ysaCalData.a3;
                    res.s32A4 = ysaCalData.a4;
                    res.s32A5 = ysaCalData.a5;
                    res.s32A6 = ysaCalData.a6;
                    res.s32A7 = ysaCalData.a7;
                    res.s32A8 = ysaCalData.a8;
                    res.s32A9 = ysaCalData.a9;
                    res.s32K = ysaCalData.k;
                }
                DBG_DIAG_IO("ioctl(cmd = Mag_Read_Cal) : ret=%d, data=...\n", res.rtn);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagMagCalData));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_SENSOR_INIT)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_FW_GET_VERSION:
            {
                struct IoctlDiagFirmwareVersion res;
                memset(&res, 0, sizeof(struct IoctlDiagFirmwareVersion));
                
                res.rtn = shub_diag_fw_get_version(&res.data[0]);
                DBG_DIAG_IO("ioctl(cmd = FW_Get_Version) : ret=%d, data=...\n", res.rtn);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagFirmwareVersion));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_FW_GET_VERSION)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_FW_CHECKSUM:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                
                res.rtn = shub_diag_fw_checksum();
                DBG_DIAG_IO("ioctl(cmd = FW_CheckSum) : ret=%d\n", res.rtn);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_FW_CHECKSUM)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_ONLY_GYRO:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_gyro_flg){
                    res.rtn = shub_diag_gyro_initialize();
                    if(res.rtn == 0){
                        initial_gyro_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_gyro_measure(&res.accGyroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Only_Gyro) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0],res.accGyroData[1],res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_ONLY_GYRO)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_ONLY_ACC:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_acc_flg){
                    res.rtn = shub_diag_acc_initialize();
                    if(res.rtn == 0){
                        initial_acc_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_acc_measure(&res.accGyroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Only_Acc) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0],res.accGyroData[1],res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_ONLY_ACC)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_CMD_WRITE:
            {
                struct IoctlDiagCmdReq cmdreq;
                
                ret = copy_from_user(&cmdreq, argp, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_from_user) : shub_ioctl(cmd = SHUB_DIAG_CMD_WRITE)\n" );
                    return -EFAULT;
                }
                ret = shub_cmd_wite( &cmdreq );
                cmdreq.rtn = ret;
                DBG_DIAG_IO("ioctl(cmd = Cmd_Write) : ret=%d, cmd=0x%04x, size=%d, data=...\n", 
                             cmdreq.rtn, cmdreq.m_Cmd, cmdreq.m_req_size);
                ret = copy_to_user(argp, &cmdreq, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_to_user) : shub_ioctl(cmd = SHUB_DIAG_CMD_WRITE)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_CMD_READ:
            {
                struct IoctlDiagCmdReq cmdreq;
                
                ret = copy_from_user(&cmdreq, argp, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_from_user) : shub_ioctl(cmd = SHUB_DIAG_CMD_READ)\n" );
                    return -EFAULT;
                }
                ret = shub_cmd_read( &cmdreq );
                cmdreq.rtn = ret;
                DBG_DIAG_IO("ioctl(cmd = Cmd_Read) : ret=%d, cmd=0x%04x, req_size=%d, res_size=%d, data=...\n", 
                             cmdreq.rtn, cmdreq.m_Cmd, cmdreq.m_req_size, cmdreq.m_res_size);
                ret = copy_to_user(argp, &cmdreq, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_to_user) : shub_ioctl(cmd = SHUB_DIAG_CMD_READ)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_ACC_SET_CAL:
            {
                unsigned char flg = 0;
                static sharp_smem_common_type *sh_smem_common;
                struct IoctlDiagAccCalibration res;
                memset(&res, 0, sizeof(struct IoctlDiagAccCalibration));
                
                sh_smem_common = sh_smem_get_common_address();
                if (sh_smem_common != NULL ) {
                    res.AccCal[0] = sh_smem_common->sh_shub_offset_acc_xyz[0];
                    res.AccCal[1] = sh_smem_common->sh_shub_offset_acc_xyz[1];
                    res.AccCal[2] = sh_smem_common->sh_shub_offset_acc_xyz[2];
                    flg = sh_smem_common->sh_shub_offset_acc_flg;
                }
                DBG_DIAG_IO("ioctl(cmd = Acc_set_call) : x=%d, y=%d, z=%d, flg=%d\n", res.AccCal[0], res.AccCal[1], res.AccCal[2], flg);
                if( flg == 1 ) {
                    res.AccCal[0] >>= SHUB_ACC_SHIFT_VAL;
                    res.AccCal[1] >>= SHUB_ACC_SHIFT_VAL;
                    res.AccCal[2] >>= SHUB_ACC_SHIFT_VAL;
                    ret = shub_set_acc_offset(&res.AccCal[0]);
                    if (ret) {
                        printk("error : shub_set_acc_offset(cmd = SHUB_DIAG_ACC_SET_CAL)ret=%d\n", ret);
                        return -EFAULT;
                    }
                }
            }
            break;
        case SHUB_DIAG_MAG_SET_CAL:
            {
                struct IoctlDiagMagCalibration res;
                
                ret = copy_from_user(&res,argp,sizeof(struct IoctlDiagMagCalibration));
                if (ret) {
                    printk("error : copy_from_user(cmd = SHUB_DIAG_MAG_SET_CAL)ret=%d\n", ret);
                    return -EFAULT;
                }
                
                DBG_DIAG_IO("ioctl(cmd = Mag_set_call) : data=...\n");
                ret = shub_cal_mag_axis_interfrence(&res.MagCal[0]);
                if (ret) {
                    printk("error : shub_cal_mag_axis_interfrence(cmd = SHUB_DIAG_MAG_SET_CAL)ret=%d\n", ret);
                    return -EFAULT;
                }
                
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagMagCalibration));
                if (ret) {
                    printk("error : copy_to_user(cmd = SHUB_DIAG_MAG_SET_CAL)ret=%d\n", ret);
                    return -EFAULT;
                }
            }
            break;
		case SHUB_DIAG_MES_ACC_SELF_TEST:
            {
				struct IoctlDiagRes res;
                DBG_DIAG_IO("ioctl(cmd = ACC_SELF_TEST)\n");
                memset(&res, 0, sizeof(struct IoctlDiagRes));
				
                res.rtn = shub_diag_acc_self_test_check(&res.accGyroData[0]);
                
                DBG_DIAG_IO("ioctl(cmd = ACC_SELF_TEST) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0], res.accGyroData[1], res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_ACC_SELF_TEST)\n");
                    return -EFAULT;
                }
            }
            break;
		case SHUB_DIAG_MES_GYRO_SELF_TEST:
            {
				struct IoctlDiagRes res;
                DBG_DIAG_IO("ioctl(cmd = GYRO_SELF_TEST)\n");
                memset(&res, 0, sizeof(struct IoctlDiagRes));
				
                res.rtn = shub_diag_gyro_self_test_check(&res.accGyroData[0]);
                
                DBG_DIAG_IO("ioctl(cmd = GYRO_SELF_TEST) : ret=%d, x=%d, y=%d, z=%d\n", 
                             res.rtn, res.accGyroData[0], res.accGyroData[1], res.accGyroData[2]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));             
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_GYRO_SELF_TEST)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_BARO:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));

                res.rtn = shub_diag_baro_initialize();
                if(res.rtn == 0){
                    res.rtn = shub_diag_baro_data_measure(&res.baroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Baro) : ret=%d, pres=%d, temp=%d\n", 
                             res.rtn, res.baroData[0],res.baroData[1]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_BARO)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_BARO_CONT:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_baro_flg){
                    res.rtn = shub_diag_baro_initialize();
                    if(res.rtn == 0){
                        initial_baro_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_baro_data_measure(&res.baroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Baro_Cont) : ret=%d, pres=%d, temp=%d\n", 
                             res.rtn, res.baroData[0],res.baroData[1]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_BARO_CONT)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_ONLY_BARO:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_baro_flg){
                    res.rtn = shub_diag_baro_initialize();
                    if(res.rtn == 0){
                        initial_baro_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_baro_measure(&res.baroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Only_Baro) : ret=%d, pres=%d, temp=%d\n", 
                             res.rtn, res.baroData[0],res.baroData[1]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_ONLY_BARO)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_MES_BARO_HWTEST:
            {
                struct IoctlDiagRes res;
                memset(&res, 0, sizeof(struct IoctlDiagRes));
                if(!initial_baro_flg){
                    res.rtn = shub_diag_baro_initialize();
                    if(res.rtn == 0){
                        res.rtn = shub_diag_baroSensorWriteSendCmd(LPS22HB_CTRL_REG1, 0x22);
                        initial_baro_flg = 1;
                    }
                }
                if(res.rtn == 0){
                    res.rtn = shub_diag_baro_hwtest_data_measure(&res.baroData[0]);
                }
                DBG_DIAG_IO("ioctl(cmd = Baro_Hwtest) : ret=%d, pres=%d, temp=%d\n", 
                             res.rtn, res.baroData[0],res.baroData[1]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlDiagRes));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUB_DIAG_MES_BARO_HWTEST)\n");
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_SLAVE_WRITE:
            {
                struct IoctlDiagCmdReq cmdreq;
                uint8_t param[12];

                ret = copy_from_user(&cmdreq, argp, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_from_user) : shub_ioctl(cmd = SHUB_DIAG_SLAVE_WRITE)\n" );
                    return -EFAULT;
                }
                memcpy(param, &cmdreq.m_buf[0], sizeof(param));

                cmdreq.m_Cmd = HC_MCU_I2C_IO;
                cmdreq.m_buf[0] = param[0];
                cmdreq.m_buf[1] = 0x01;
                cmdreq.m_buf[2] = param[1];
                cmdreq.m_buf[3] = cmdreq.m_req_size-2;
                memcpy(&cmdreq.m_buf[4], &param[2], sizeof(param)-2);
                cmdreq.m_req_size += 2;
                ret = shub_cmd_wite( &cmdreq );
                cmdreq.rtn = ret;
                DBG_DIAG_IO("ioctl(cmd = SLAVE_WRITE) : ret=%d, cmd=0x%04x, size=%d, data=...\n", 
                             cmdreq.rtn, cmdreq.m_Cmd, cmdreq.m_req_size);
                ret = copy_to_user(argp, &cmdreq, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_to_user) : shub_ioctl(cmd = SHUB_DIAG_CMD_WRITE)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUB_DIAG_SLAVE_READ:
            {
                struct IoctlDiagCmdReq cmdreq;
                uint8_t param[3];

                ret = copy_from_user(&cmdreq, argp, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_from_user) : shub_ioctl(cmd = SHUB_DIAG_SLAVE_READ)\n" );
                    return -EFAULT;
                }
                memcpy(param, &cmdreq.m_buf[0], sizeof(param));
                cmdreq.m_Cmd = HC_MCU_I2C_IO;
                cmdreq.m_buf[0] = param[0];
                cmdreq.m_buf[1] = 0x00;
                cmdreq.m_buf[2] = param[1];
                cmdreq.m_buf[3] = param[2];
                cmdreq.m_req_size += 1;
                ret = shub_cmd_read( &cmdreq );
                cmdreq.rtn = ret;
                DBG_DIAG_IO("ioctl(cmd = SLAVE_READ) : ret=%d, cmd=0x%04x, size=%d, data=...\n", 
                             cmdreq.rtn, cmdreq.m_Cmd, cmdreq.m_req_size);
                ret = copy_to_user(argp, &cmdreq, sizeof(struct IoctlDiagCmdReq));
                if (ret) {
                    printk("error(copy_to_user) : shub_ioctl(cmd = SHUB_DIAG_SLAVE_READ)\n" );
                    return -EFAULT;
                }
            }
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static int32_t shub_io_open( struct inode* inode, struct file* filp )
{
    return 0;
}

static int32_t shub_io_release( struct inode* inode, struct file* filp )
{
    return 0;
}

static unsigned int shub_io_poll(struct file *fp, poll_table *wait)
{
    return 0;
}

static ssize_t shub_io_read(struct file *fp, char *buff, size_t count, loff_t *ops)
{
    return 0;
}

static struct file_operations shub_fops = {
    .owner   = THIS_MODULE,
    .open    = shub_io_open,
    .release = shub_io_release,
    .unlocked_ioctl = shub_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = shub_ioctl,
#endif
    .poll    = shub_io_poll,
    .read    = shub_io_read,
};

static struct miscdevice shub_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "shub_io_diag",
    .fops  = &shub_fops,
};
int32_t shub_probe_diag(struct platform_device *pfdev)
{
    int32_t ret = 0;

    pdev = platform_device_register_simple("shub_io_diag", -1, NULL, 0);
    if (IS_ERR(pdev)) {
        ret = PTR_ERR(pdev);
        goto out_driver;
    }

    ret = misc_register(&shub_device);
    if (ret) {
        printk("shub_io_diag: shub_io_device register failed\n");
        goto exit_misc_device_register_failed;
    }

    return 0;

exit_misc_device_register_failed:
    input_free_device(shub_idev);
    platform_device_unregister(pdev);
out_driver:
    // out_region:
    return ret;
}

int32_t shub_remove_diag(struct platform_device *pfdev)
{
    misc_deregister(&shub_device);
    input_unregister_device(shub_idev);
    input_free_device(shub_idev);
    platform_device_unregister(pdev);
    return 0;
}

static struct platform_driver shub_diag_driver = {
    .probe = shub_probe_diag,
    .remove = shub_remove_diag,
    .shutdown = NULL,
    .driver = {
        .name = "shub_dev_diag",
        .of_match_table = shub_of_match_tb_diag,
    },
};

static int32_t __init shub_init(void)
{
    int ret;
    
    ret = platform_driver_register(&shub_diag_driver);
    
    return ret;
}

static void __exit shub_exit(void)
{
    platform_driver_unregister(&shub_diag_driver);
}

late_initcall(shub_init);
module_exit(shub_exit);

MODULE_DESCRIPTION("SensorHub Input Device (misc)");
MODULE_AUTHOR("LAPIS SEMICOMDUCTOR");
MODULE_LICENSE("GPL v2");
