/* include/uapi/linux/ir2e71y.h  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
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

#ifndef UAPI_IR2E71Y_H
#define UAPI_IR2E71Y_H
/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_OPT_CHANGE_INT_1             (0x01)
#define IR2E71Y_OPT_CHANGE_INT_2             (0x02)

#define IR2E71Y_LUT_MBR_NUM                  (3)
#define IR2E71Y_LUT_STAGE_NUM                (16)

enum {
    IR2E71Y_PHOTO_SENSOR_TYPE_APP,
    IR2E71Y_PHOTO_SENSOR_TYPE_LUX,
    IR2E71Y_PHOTO_SENSOR_TYPE_CAMERA,
    IR2E71Y_PHOTO_SENSOR_TYPE_KEYLED,
    IR2E71Y_PHOTO_SENSOR_TYPE_DIAG,
    IR2E71Y_PHOTO_SENSOR_TYPE_SENSORHUB,
    NUM_IR2E71Y_PHOTO_SENSOR_TYPE
};

enum {
    IR2E71Y_PHOTO_SENSOR_DISABLE,
    IR2E71Y_PHOTO_SENSOR_ENABLE,
    NUM_IR2E71Y_PHOTO_SENSOR
};

enum {
    IR2E71Y_RESULT_SUCCESS,
    IR2E71Y_RESULT_FAILURE,
    IR2E71Y_RESULT_FAILURE_I2C_TMO,
    IR2E71Y_RESULT_ALS_INT_OFF,
    NUM_IR2E71Y_RESULT
};

/* ------------------------------------------------------------------------- */
/* STRUCT                                                                    */
/* ------------------------------------------------------------------------- */
struct ir2e71y_photo_sensor_val {
    unsigned short value;
    unsigned int   lux;
    int mode;
    int result;
};

struct ir2e71y_photo_sensor_power_ctl {
    int type;
    int power;
};

struct ir2e71y_photo_sensor_raw_val {
    unsigned short clear;
    unsigned short ir;
    int result;
};

struct ir2e71y_photo_sensor_trigger {
    unsigned short level;
    unsigned short side;
    unsigned short en_hi_edge;
    unsigned short en_lo_edge;
    unsigned short enable;
};

struct ir2e71y_photo_sensor_int_trigger {
    struct ir2e71y_photo_sensor_trigger trigger1;
    struct ir2e71y_photo_sensor_trigger trigger2;
    int type;
    int result;
};

struct ir2e71y_light_info {
    unsigned int lux;
    unsigned short level;
    unsigned int clear_ir_rate;
    int result;
};

struct ir2e71y_lut_data {
    unsigned short red[IR2E71Y_LUT_STAGE_NUM][IR2E71Y_LUT_MBR_NUM];
    unsigned short green[IR2E71Y_LUT_STAGE_NUM][IR2E71Y_LUT_MBR_NUM];
    unsigned short blue[IR2E71Y_LUT_STAGE_NUM][IR2E71Y_LUT_MBR_NUM];
};

struct ir2e71y_lut_info {
    unsigned short lut_status;
    struct ir2e71y_lut_data lut;
};

/* ------------------------------------------------------------------------- */
/* IOCTL                                                                     */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_IOC_MAGIC 's'
#define IR2E71Y_IOCTL_GET_LUX                            _IOWR (IR2E71Y_IOC_MAGIC, 1, struct ir2e71y_photo_sensor_val)
#define IR2E71Y_IOCTL_PHOTO_SENSOR_POW_CTL               _IOW  (IR2E71Y_IOC_MAGIC, 2, struct ir2e71y_photo_sensor_power_ctl)
#define IR2E71Y_IOCTL_GET_ALS                            _IOWR (IR2E71Y_IOC_MAGIC, 3, struct ir2e71y_photo_sensor_raw_val)
#define IR2E71Y_IOCTL_SET_ALSINT                         _IOWR (IR2E71Y_IOC_MAGIC, 4, struct ir2e71y_photo_sensor_int_trigger)
#define IR2E71Y_IOCTL_GET_ALSINT                         _IOWR (IR2E71Y_IOC_MAGIC, 5, struct ir2e71y_photo_sensor_int_trigger)
#define IR2E71Y_IOCTL_GET_LIGHT_INFO                     _IOWR (IR2E71Y_IOC_MAGIC, 6, struct ir2e71y_light_info)
#define IR2E71Y_IOCTL_GET_LUT_INFO                       _IOR  (IR2E71Y_IOC_MAGIC, 7, struct ir2e71y_lut_info)

#endif /* UAPI_IR2E71Y_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
