/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 */
#include "ssp.h"

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/types.h>

/*************************************************************************/
/* SSP Kernel -> HAL input evnet function                                */
/*************************************************************************/
#define IIO_BUFFER_12_BYTES         20 /* 12 + timestamp 8*/
#define IIO_BUFFER_6_BYTES         14
#define IIO_BUFFER_1_BYTES			9
#define IIO_BUFFER_17_BYTES			25
#define IIO_BUFFER_23_BYTES			31

/* data header defines */
static int ssp_push_23bytes_buffer(struct iio_dev *indio_dev,
	u64 t, int *q)
{
	u8 buf[IIO_BUFFER_23_BYTES];
	int i;

	for (i = 0; i < 4; i++)
		memcpy(buf + 4 * i, &q[i], sizeof(q[i]));
	buf[16] = (u8)q[4];
	for (i = 0; i < 3; i++)
		memcpy(buf + 17 + i * 2, &q[i + 5], sizeof(s16));
	memcpy(buf + 23, &t, sizeof(t));

	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int ssp_push_17bytes_buffer(struct iio_dev *indio_dev,
							u64 t, int *q)
{
	u8 buf[IIO_BUFFER_17_BYTES];
	int i;

	for (i = 0; i < 4; i++)
		memcpy(buf + 4 * i, &q[i], sizeof(q[i]));
	buf[16] = (u8)q[5];
	memcpy(buf + 17, &t, sizeof(t));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int ssp_push_12bytes_buffer(struct iio_dev *indio_dev, u64 t,
									int *q)
{
	u8 buf[IIO_BUFFER_12_BYTES];
	int i;

	for (i = 0; i < 3; i++)
		memcpy(buf + 4 * i, &q[i], sizeof(q[i]));
	memcpy(buf + 12, &t, sizeof(t));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int ssp_push_6bytes_buffer(struct iio_dev *indio_dev,
							u64 t, s16 *d)
{
	u8 buf[IIO_BUFFER_6_BYTES];
	int i;

	for (i = 0; i < 3; i++)
		memcpy(buf + i * 2, &d[i], sizeof(d[i]));

	memcpy(buf + 6, &t, sizeof(t));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

static int ssp_push_1bytes_buffer(struct iio_dev *indio_dev,
							u64 t, u8 *d)
{
	u8 buf[IIO_BUFFER_1_BYTES];

	memcpy(buf, d, sizeof(u8));
	memcpy(buf + 1, &t, sizeof(t));
	mutex_lock(&indio_dev->mlock);
	iio_push_to_buffers(indio_dev, buf);
	mutex_unlock(&indio_dev->mlock);

	return 0;
}

void report_meta_data(struct ssp_data *data, struct sensor_value *s)
{
	input_report_rel(data->meta_input_dev, REL_DIAL, s->meta_data.what);
	input_report_rel(data->meta_input_dev, REL_HWHEEL,
		s->meta_data.sensor + 1);
	input_sync(data->meta_input_dev);
}

void report_acc_data(struct ssp_data *data, struct sensor_value *accdata)
{
	s16 accel_buf[3];

	data->buf[ACCELEROMETER_SENSOR].x = accdata->x;
	data->buf[ACCELEROMETER_SENSOR].y = accdata->y;
	data->buf[ACCELEROMETER_SENSOR].z = accdata->z;

	accel_buf[0] = data->buf[ACCELEROMETER_SENSOR].x;
	accel_buf[1] = data->buf[ACCELEROMETER_SENSOR].y;
	accel_buf[2] = data->buf[ACCELEROMETER_SENSOR].z;

	ssp_push_6bytes_buffer(data->accel_indio_dev, accdata->timestamp,
		accel_buf);
}

void report_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata)
{
	int lTemp[3] = {0,};

	data->buf[GYROSCOPE_SENSOR].x = gyrodata->x;
	data->buf[GYROSCOPE_SENSOR].y = gyrodata->y;
	data->buf[GYROSCOPE_SENSOR].z = gyrodata->z;

	if (data->uGyroDps == GYROSCOPE_DPS500) {
		lTemp[0] = (int)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (int)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (int)data->buf[GYROSCOPE_SENSOR].z;
	} else if (data->uGyroDps == GYROSCOPE_DPS250)	{
		lTemp[0] = (int)data->buf[GYROSCOPE_SENSOR].x >> 1;
		lTemp[1] = (int)data->buf[GYROSCOPE_SENSOR].y >> 1;
		lTemp[2] = (int)data->buf[GYROSCOPE_SENSOR].z >> 1;
	} else if (data->uGyroDps == GYROSCOPE_DPS2000) {
		lTemp[0] = (int)data->buf[GYROSCOPE_SENSOR].x << 2;
		lTemp[1] = (int)data->buf[GYROSCOPE_SENSOR].y << 2;
		lTemp[2] = (int)data->buf[GYROSCOPE_SENSOR].z << 2;
	} else {
		lTemp[0] = (int)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (int)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (int)data->buf[GYROSCOPE_SENSOR].z;
	}

	ssp_push_12bytes_buffer(data->gyro_indio_dev, gyrodata->timestamp,
		lTemp);
}

void report_geomagnetic_raw_data(struct ssp_data *data,
	struct sensor_value *magrawdata)
{
	data->buf[GEOMAGNETIC_RAW].x = magrawdata->x;
	data->buf[GEOMAGNETIC_RAW].y = magrawdata->y;
	data->buf[GEOMAGNETIC_RAW].z = magrawdata->z;
}

void report_mag_data(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_SENSOR].cal_x = magdata->cal_x;
	data->buf[GEOMAGNETIC_SENSOR].cal_y = magdata->cal_y;
	data->buf[GEOMAGNETIC_SENSOR].cal_z = magdata->cal_z;
	data->buf[GEOMAGNETIC_SENSOR].accuracy = magdata->accuracy;

	input_report_rel(data->mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_SENSOR].cal_x);
	input_report_rel(data->mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_SENSOR].cal_y);
	input_report_rel(data->mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_SENSOR].cal_z);
	input_report_rel(data->mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_SENSOR].accuracy + 1);

	input_sync(data->mag_input_dev);
}

void report_mag_uncaldata(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x = magdata->uncal_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y = magdata->uncal_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z = magdata->uncal_z;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x = magdata->offset_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y = magdata->offset_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z = magdata->offset_z;

	input_report_rel(data->uncal_mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncal_mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncal_mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncal_mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncal_mag_input_dev, REL_DIAL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncal_mag_input_dev, REL_WHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z);
	input_sync(data->uncal_mag_input_dev);
}

void report_uncalib_gyro_data(struct ssp_data *data,
	struct sensor_value *gyrodata)
{
	data->buf[GYRO_UNCALIB_SENSOR].uncal_x = gyrodata->uncal_x;
	data->buf[GYRO_UNCALIB_SENSOR].uncal_y = gyrodata->uncal_y;
	data->buf[GYRO_UNCALIB_SENSOR].uncal_z = gyrodata->uncal_z;
	data->buf[GYRO_UNCALIB_SENSOR].offset_x = gyrodata->offset_x;
	data->buf[GYRO_UNCALIB_SENSOR].offset_y = gyrodata->offset_y;
	data->buf[GYRO_UNCALIB_SENSOR].offset_z = gyrodata->offset_z;

	input_report_rel(data->uncalib_gyro_input_dev, REL_RX,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncalib_gyro_input_dev, REL_RY,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncalib_gyro_input_dev, REL_RZ,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncalib_gyro_input_dev, REL_HWHEEL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncalib_gyro_input_dev, REL_DIAL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_z);
	input_sync(data->uncalib_gyro_input_dev);
}

void report_sig_motion_data(struct ssp_data *data,
	struct sensor_value *sig_motion_data)
{
	data->buf[SIG_MOTION_SENSOR].sig_motion = sig_motion_data->sig_motion;

	input_report_rel(data->sig_motion_input_dev, REL_MISC,
		data->buf[SIG_MOTION_SENSOR].sig_motion);
	input_sync(data->sig_motion_input_dev);
}

void report_rot_data(struct ssp_data *data, struct sensor_value *rotdata)
{
	int rot_buf[8];

	data->buf[ROTATION_VECTOR].quat_a = rotdata->quat_a;
	data->buf[ROTATION_VECTOR].quat_b = rotdata->quat_b;
	data->buf[ROTATION_VECTOR].quat_c = rotdata->quat_c;
	data->buf[ROTATION_VECTOR].quat_d = rotdata->quat_d;

	data->buf[ROTATION_VECTOR].acc_x = rotdata->acc_x;
	data->buf[ROTATION_VECTOR].acc_y = rotdata->acc_y;
	data->buf[ROTATION_VECTOR].acc_z = rotdata->acc_z;

	data->buf[ROTATION_VECTOR].acc_rot = rotdata->acc_rot;
	rot_buf[0] = rotdata->quat_a;
	rot_buf[1] = rotdata->quat_b;
	rot_buf[2] = rotdata->quat_c;
	rot_buf[3] = rotdata->quat_d;
	rot_buf[4] = rotdata->acc_rot;
	rot_buf[5] = rotdata->acc_x;
	rot_buf[6] = rotdata->acc_y;
	rot_buf[7] = rotdata->acc_z;

	ssp_push_23bytes_buffer(data->rot_indio_dev, rotdata->timestamp,
		rot_buf);
}

void report_game_rot_data(struct ssp_data *data,
	struct sensor_value *grvec_data)
{
	int grot_buf[5];

	data->buf[GAME_ROTATION_VECTOR].quat_grv_a = grvec_data->quat_grv_a;
	data->buf[GAME_ROTATION_VECTOR].quat_grv_b = grvec_data->quat_grv_b;
	data->buf[GAME_ROTATION_VECTOR].quat_grv_c = grvec_data->quat_grv_c;
	data->buf[GAME_ROTATION_VECTOR].quat_grv_d = grvec_data->quat_grv_d;
	data->buf[GAME_ROTATION_VECTOR].acc_grv_rot = grvec_data->acc_grv_rot;

	grot_buf[0] = grvec_data->quat_grv_a;
	grot_buf[1] = grvec_data->quat_grv_b;
	grot_buf[2] = grvec_data->quat_grv_c;
	grot_buf[3] = grvec_data->quat_grv_d;
	grot_buf[4] = grvec_data->acc_grv_rot;

	ssp_push_17bytes_buffer(data->game_rot_indio_dev, grvec_data->timestamp,
		grot_buf);
}

void report_step_det_data(struct ssp_data *data,
		struct sensor_value *stepdet_data)
{
	data->buf[STEP_DETECTOR].step_det = stepdet_data->step_det;
	ssp_push_1bytes_buffer(data->step_det_indio_dev,
		stepdet_data->timestamp, &stepdet_data->step_det);
}

void report_step_cnt_data(struct ssp_data *data,
	struct sensor_value *sig_motion_data)
{
	data->buf[STEP_COUNTER].step_diff = sig_motion_data->step_diff;

	data->step_count_total += data->buf[STEP_COUNTER].step_diff;

	input_report_rel(data->step_cnt_input_dev, REL_MISC,
		data->step_count_total + 1);
	input_sync(data->step_cnt_input_dev);
}

#ifdef CONFIG_SENSORS_SSP_ADPD142
void report_hrm_lib_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_LIB].hr = hrmdata->hr;
	data->buf[BIO_HRM_LIB].rri = hrmdata->rri;
	data->buf[BIO_HRM_LIB].snr = hrmdata->snr;

	input_report_rel(data->hrm_lib_input_dev, REL_X,
		data->buf[BIO_HRM_LIB].hr + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Y,
		data->buf[BIO_HRM_LIB].rri + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Z,
		data->buf[BIO_HRM_LIB].snr + 1);
	input_sync(data->hrm_lib_input_dev);
}
#endif

void report_tilt_wake_data(struct ssp_data *data, struct sensor_value *tiltdata)
{
	data->buf[TILT_TO_WAKE].x = tiltdata->x;
	data->buf[TILT_TO_WAKE].y = tiltdata->y;
	data->buf[TILT_TO_WAKE].z = tiltdata->z;

	input_report_rel(data->tilt_wake_input_dev, REL_X,
		data->buf[TILT_TO_WAKE].x >= 0 ?
		(data->buf[TILT_TO_WAKE].x + 1) :
		data->buf[TILT_TO_WAKE].x);
	input_report_rel(data->tilt_wake_input_dev, REL_Y,
		data->buf[TILT_TO_WAKE].y >= 0 ?
		(data->buf[TILT_TO_WAKE].y + 1) :
		data->buf[TILT_TO_WAKE].y);
	input_report_rel(data->tilt_wake_input_dev, REL_Z,
		data->buf[TILT_TO_WAKE].z >= 0 ?
		(data->buf[TILT_TO_WAKE].z + 1) :
		data->buf[TILT_TO_WAKE].z);
	input_sync(data->tilt_wake_input_dev);
	wake_lock_timeout(&data->ssp_wake_lock, 3 * HZ);
}

int initialize_event_symlink(struct ssp_data *data)
{
	int iRet = 0;

	iRet = sensors_create_symlink(data->mag_input_dev);
	if (iRet < 0)
		goto iRet_mag_sysfs_create_link;

	iRet = sensors_create_symlink(data->uncal_mag_input_dev);
	if (iRet < 0)
		goto iRet_uncal_mag_sysfs_create_link;

	iRet = sensors_create_symlink(data->sig_motion_input_dev);
	if (iRet < 0)
		goto iRet_sig_motion_sysfs_create_link;

	iRet = sensors_create_symlink(data->uncalib_gyro_input_dev);
	if (iRet < 0)
		goto iRet_uncal_gyro_sysfs_create_link;

	iRet = sensors_create_symlink(data->step_cnt_input_dev);
	if (iRet < 0)
		goto iRet_step_cnt_sysfs_create_link;

	iRet = sensors_create_symlink(data->meta_input_dev);
	if (iRet < 0)
		goto iRet_meta_sysfs_create_link;

#ifdef CONFIG_SENSORS_SSP_ADPD142
	iRet = sensors_create_symlink(data->hrm_lib_input_dev);
	if (iRet < 0)
		goto iRet_hrm_lib_sysfs_create_link;
#endif
	iRet = sensors_create_symlink(data->tilt_wake_input_dev);
	if (iRet < 0)
		goto iRet_tilt_wake_sysfs_create_link;
	return SUCCESS;

iRet_tilt_wake_sysfs_create_link:
#ifdef CONFIG_SENSORS_SSP_ADPD142
	sensors_remove_symlink(data->hrm_lib_input_dev);
iRet_hrm_lib_sysfs_create_link:
#endif
	sensors_remove_symlink(data->meta_input_dev);
iRet_meta_sysfs_create_link:
	sensors_remove_symlink(data->step_cnt_input_dev);
iRet_step_cnt_sysfs_create_link:
	sensors_remove_symlink(data->uncalib_gyro_input_dev);
iRet_uncal_gyro_sysfs_create_link:
	sensors_remove_symlink(data->sig_motion_input_dev);
iRet_sig_motion_sysfs_create_link:
	sensors_remove_symlink(data->uncal_mag_input_dev);
iRet_uncal_mag_sysfs_create_link:
	sensors_remove_symlink(data->mag_input_dev);
iRet_mag_sysfs_create_link:
	pr_err("[SSP]: %s - could not create event symlink\n", __func__);

	return FAIL;
}

void remove_event_symlink(struct ssp_data *data)
{
	sensors_remove_symlink(data->mag_input_dev);
	sensors_remove_symlink(data->uncal_mag_input_dev);
	sensors_remove_symlink(data->sig_motion_input_dev);
	sensors_remove_symlink(data->uncalib_gyro_input_dev);
	sensors_remove_symlink(data->step_cnt_input_dev);
	sensors_remove_symlink(data->meta_input_dev);
#ifdef CONFIG_SENSORS_SSP_ADPD142
	sensors_remove_symlink(data->hrm_lib_input_dev);
#endif
	sensors_remove_symlink(data->tilt_wake_input_dev);
}

static const struct iio_info accel_info = {
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec accel_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = -1,
		.scan_index = 3,
		.scan_type = IIO_ST('s', IIO_BUFFER_6_BYTES*8,
				IIO_BUFFER_6_BYTES*8, 0)
	}
};

static const struct iio_info gyro_info = {
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec gyro_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = -1,
		.scan_index = 3,
		.scan_type = IIO_ST('s', IIO_BUFFER_12_BYTES*8,
				IIO_BUFFER_12_BYTES*8, 0)
	}
};

static const struct iio_info game_rot_info = {
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec game_rot_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = -1,
		.scan_index = 3,
		.scan_type = IIO_ST('s', IIO_BUFFER_17_BYTES*8,
				IIO_BUFFER_17_BYTES*8, 0)
	}
};

static const struct iio_info rot_info = {
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec rot_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = -1,
		.scan_index = 3,
		.scan_type = IIO_ST('s', IIO_BUFFER_23_BYTES*8,
				IIO_BUFFER_23_BYTES*8, 0)
	}
};

static const struct iio_info step_det_info = {
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec step_det_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = -1,
		.scan_index = 3,
		.scan_type = IIO_ST('s', IIO_BUFFER_1_BYTES*8,
				IIO_BUFFER_1_BYTES*8, 0)
	}
};

static const struct iio_info pressure_info = {
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec pressure_channels[] = {
	{
		.type = IIO_TIMESTAMP,
		.channel = -1,
		.scan_index = 3,
		.scan_type = IIO_ST('s', IIO_BUFFER_12_BYTES * 8,
			IIO_BUFFER_12_BYTES * 8, 0)
	}
};

int initialize_input_dev(struct ssp_data *data)
{
	int iRet = 0;

	/* Registering iio device - start */
	data->accel_indio_dev = iio_device_alloc(0);
	if (!data->accel_indio_dev)
		goto err_alloc_accel;

	data->accel_indio_dev->name = "accelerometer_sensor";
	data->accel_indio_dev->dev.parent = &data->spi->dev;
	data->accel_indio_dev->info = &accel_info;
	data->accel_indio_dev->channels = accel_channels;
	data->accel_indio_dev->num_channels = ARRAY_SIZE(accel_channels);
	data->accel_indio_dev->modes = INDIO_DIRECT_MODE;
	data->accel_indio_dev->currentmode = INDIO_DIRECT_MODE;

	iRet = ssp_iio_configure_ring(data->accel_indio_dev);
	if (iRet)
		goto err_config_ring_accel;

	iRet = iio_buffer_register(data->accel_indio_dev,
		data->accel_indio_dev->channels,
		data->accel_indio_dev->num_channels);
	if (iRet)
		goto err_register_buffer_accel;

	iRet = iio_device_register(data->accel_indio_dev);
	if (iRet)
		goto err_register_device_accel;

	data->gyro_indio_dev = iio_device_alloc(0);
	if (!data->gyro_indio_dev)
		goto err_alloc_gyro;

	data->gyro_indio_dev->name = "gyro_sensor";
	data->gyro_indio_dev->dev.parent = &data->spi->dev;
	data->gyro_indio_dev->info = &gyro_info;
	data->gyro_indio_dev->channels = gyro_channels;
	data->gyro_indio_dev->num_channels = ARRAY_SIZE(gyro_channels);
	data->gyro_indio_dev->modes = INDIO_DIRECT_MODE;
	data->gyro_indio_dev->currentmode = INDIO_DIRECT_MODE;

	iRet = ssp_iio_configure_ring(data->gyro_indio_dev);
	if (iRet)
		goto err_config_ring_gyro;

	iRet = iio_buffer_register(data->gyro_indio_dev,
		data->gyro_indio_dev->channels,
		data->gyro_indio_dev->num_channels);
	if (iRet)
		goto err_register_buffer_gyro;

	iRet = iio_device_register(data->gyro_indio_dev);
	if (iRet)
		goto err_register_device_gyro;

	data->game_rot_indio_dev = iio_device_alloc(0);
	if (!data->game_rot_indio_dev)
		goto err_alloc_game_rot;

	data->game_rot_indio_dev->name = "game_rotation_vector";
	data->game_rot_indio_dev->dev.parent = &data->spi->dev;
	data->game_rot_indio_dev->info = &game_rot_info;
	data->game_rot_indio_dev->channels = game_rot_channels;
	data->game_rot_indio_dev->num_channels = ARRAY_SIZE(game_rot_channels);
	data->game_rot_indio_dev->modes = INDIO_DIRECT_MODE;
	data->game_rot_indio_dev->currentmode = INDIO_DIRECT_MODE;

	iRet = ssp_iio_configure_ring(data->game_rot_indio_dev);
	if (iRet)
		goto err_config_ring_game_rot;

	iRet = iio_buffer_register(data->game_rot_indio_dev,
		data->game_rot_indio_dev->channels,
		data->game_rot_indio_dev->num_channels);
	if (iRet)
		goto err_register_buffer_game_rot;

	iRet = iio_device_register(data->game_rot_indio_dev);
	if (iRet)
		goto err_register_device_game_rot;

	data->rot_indio_dev = iio_device_alloc(0);
	if (!data->rot_indio_dev)
		goto err_alloc_rot;

	data->rot_indio_dev->name = "rotation_vector_sensor";
	data->rot_indio_dev->dev.parent = &data->spi->dev;
	data->rot_indio_dev->info = &rot_info;
	data->rot_indio_dev->channels = rot_channels;
	data->rot_indio_dev->num_channels = ARRAY_SIZE(rot_channels);
	data->rot_indio_dev->modes = INDIO_DIRECT_MODE;
	data->rot_indio_dev->currentmode = INDIO_DIRECT_MODE;

	iRet = ssp_iio_configure_ring(data->rot_indio_dev);
	if (iRet)
		goto err_config_ring_rot;

	iRet = iio_buffer_register(data->rot_indio_dev,
		data->rot_indio_dev->channels,
		data->rot_indio_dev->num_channels);
	if (iRet)
		goto err_register_buffer_rot;

	iRet = iio_device_register(data->rot_indio_dev);
	if (iRet)
		goto err_register_device_rot;

	data->step_det_indio_dev = iio_device_alloc(0);
	if (!data->step_det_indio_dev)
		goto err_alloc_step_det;

	data->step_det_indio_dev->name = "step_det_sensor";
	data->step_det_indio_dev->dev.parent = &data->spi->dev;
	data->step_det_indio_dev->info = &step_det_info;
	data->step_det_indio_dev->channels = step_det_channels;
	data->step_det_indio_dev->num_channels = ARRAY_SIZE(step_det_channels);
	data->step_det_indio_dev->modes = INDIO_DIRECT_MODE;
	data->step_det_indio_dev->currentmode = INDIO_DIRECT_MODE;

	iRet = ssp_iio_configure_ring(data->step_det_indio_dev);
	if (iRet)
		goto err_config_ring_step_det;

	iRet = iio_buffer_register(data->step_det_indio_dev,
		data->step_det_indio_dev->channels,
		data->step_det_indio_dev->num_channels);
	if (iRet)
		goto err_register_buffer_step_det;

	iRet = iio_device_register(data->step_det_indio_dev);
	if (iRet)
		goto err_register_device_step_det;

	data->mag_input_dev = input_allocate_device();
	if (data->mag_input_dev == NULL)
		goto err_initialize_mag_input_dev;

	data->mag_input_dev->name = "geomagnetic_sensor";
	input_set_capability(data->mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->mag_input_dev, EV_REL, REL_HWHEEL);

	iRet = input_register_device(data->mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->mag_input_dev);
		goto err_initialize_mag_input_dev;
	}
	input_set_drvdata(data->mag_input_dev, data);

	data->uncal_mag_input_dev = input_allocate_device();
	if (data->uncal_mag_input_dev == NULL)
		goto err_initialize_uncal_mag_input_dev;

	data->uncal_mag_input_dev->name = "uncal_geomagnetic_sensor";
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->uncal_mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncal_mag_input_dev);
		goto err_initialize_uncal_mag_input_dev;
	}
	input_set_drvdata(data->uncal_mag_input_dev, data);

	data->sig_motion_input_dev = input_allocate_device();
	if (data->sig_motion_input_dev == NULL)
		goto err_initialize_sig_motion_input_dev;

	data->sig_motion_input_dev->name = "sig_motion_sensor";
	input_set_capability(data->sig_motion_input_dev, EV_REL, REL_MISC);
	iRet = input_register_device(data->sig_motion_input_dev);
	if (iRet < 0) {
		input_free_device(data->sig_motion_input_dev);
		goto err_initialize_sig_motion_input_dev;
	}
	input_set_drvdata(data->sig_motion_input_dev, data);

	data->uncalib_gyro_input_dev = input_allocate_device();
	if (data->uncalib_gyro_input_dev == NULL)
		goto err_initialize_uncal_gyro_input_dev;

	data->uncalib_gyro_input_dev->name = "uncal_gyro_sensor";
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->uncalib_gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncalib_gyro_input_dev);
		goto err_initialize_uncal_gyro_input_dev;
	}
	input_set_drvdata(data->uncalib_gyro_input_dev, data);

	data->step_cnt_input_dev = input_allocate_device();
	if (data->step_cnt_input_dev == NULL)
		goto err_initialize_step_cnt_input_dev;

	data->step_cnt_input_dev->name = "step_cnt_sensor";
	input_set_capability(data->step_cnt_input_dev, EV_REL, REL_MISC);
	iRet = input_register_device(data->step_cnt_input_dev);
	if (iRet < 0) {
		input_free_device(data->step_cnt_input_dev);
		goto err_initialize_step_cnt_input_dev;
	}
	input_set_drvdata(data->step_cnt_input_dev, data);

	data->meta_input_dev = input_allocate_device();
	if (data->meta_input_dev == NULL)
		goto err_initialize_meta_input_dev;

	data->meta_input_dev->name = "meta_event";
	input_set_capability(data->meta_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->meta_input_dev, EV_REL, REL_DIAL);
	iRet = input_register_device(data->meta_input_dev);
	if (iRet < 0) {
		input_free_device(data->meta_input_dev);
		goto err_initialize_meta_input_dev;
	}
	input_set_drvdata(data->meta_input_dev, data);

#ifdef CONFIG_SENSORS_SSP_ADPD142
	data->hrm_lib_input_dev = input_allocate_device();
	if (data->hrm_lib_input_dev == NULL)
		goto err_initialize_hrm_lib_input_dev;

	data->hrm_lib_input_dev->name = "hrm_lib_sensor";
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->hrm_lib_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_lib_input_dev);
		goto err_initialize_hrm_lib_input_dev;
	}
	input_set_drvdata(data->hrm_lib_input_dev, data);
#endif

	data->tilt_wake_input_dev = input_allocate_device();
	if (data->tilt_wake_input_dev == NULL)
		goto err_initialize_tilt_wake_input_dev;

	data->tilt_wake_input_dev->name = "tilt_wake_sensor";
	input_set_capability(data->tilt_wake_input_dev, EV_REL, REL_X);
	input_set_capability(data->tilt_wake_input_dev, EV_REL, REL_Y);
	input_set_capability(data->tilt_wake_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->tilt_wake_input_dev);
	if (iRet < 0) {
		input_free_device(data->tilt_wake_input_dev);
		goto err_initialize_tilt_wake_input_dev;
	}
	input_set_drvdata(data->tilt_wake_input_dev, data);

	return SUCCESS;

err_initialize_tilt_wake_input_dev:
	pr_err("[SSP]: %s - could not allocate tilt wake input device\n",
		__func__);
#ifdef CONFIG_SENSORS_SSP_ADPD142
	input_unregister_device(data->hrm_lib_input_dev);
err_initialize_hrm_lib_input_dev:
	pr_err("[SSP]: %s - could not allocate hrm lib input device\n",
		__func__);
#endif
	input_unregister_device(data->meta_input_dev);
err_initialize_meta_input_dev:
	pr_err("[SSP]: %s - could not allocate meta event input device\n",
		__func__);
	input_unregister_device(data->step_cnt_input_dev);
err_initialize_step_cnt_input_dev:
	pr_err("[SSP]: %s - could not allocate step cnt input device\n",
		__func__);
	input_unregister_device(data->uncalib_gyro_input_dev);
err_initialize_uncal_gyro_input_dev:
	pr_err("[SSP]: %s - could not allocate uncal gyro input device\n",
		__func__);
	input_unregister_device(data->sig_motion_input_dev);
err_initialize_sig_motion_input_dev:
	pr_err("[SSP]: %s - could not allocate sig motion input device\n",
		__func__);
	input_unregister_device(data->uncal_mag_input_dev);
err_initialize_uncal_mag_input_dev:
	pr_err("[SSP]: %s - could not allocate uncal mag input device\n",
		__func__);
	input_unregister_device(data->mag_input_dev);
err_initialize_mag_input_dev:
	pr_err("[SSP]: failed to allocate memory for iio pressure_sensor device\n");
	iio_device_unregister(data->step_det_indio_dev);
err_register_device_step_det:
	pr_err("[SSP]: failed to register step_det device\n");
	iio_buffer_unregister(data->step_det_indio_dev);
err_register_buffer_step_det:
	pr_err("[SSP]: failed to register step_det buffer\n");
	ssp_iio_unconfigure_ring(data->step_det_indio_dev);
err_config_ring_step_det:
	pr_err("[SSP]: failed to configure step_det ring buffer\n");
	iio_device_free(data->step_det_indio_dev);
err_alloc_step_det:
	pr_err("[SSP]: failed to allocate memory for iio step_det device\n");
	iio_device_unregister(data->rot_indio_dev);
err_register_device_rot:
	pr_err("[SSP]: failed to register rot device\n");
	iio_buffer_unregister(data->rot_indio_dev);
err_register_buffer_rot:
	pr_err("[SSP]: failed to register rot buffer\n");
	ssp_iio_unconfigure_ring(data->rot_indio_dev);
err_config_ring_rot:
	pr_err("[SSP]: failed to configure rot ring buffer\n");
	iio_device_free(data->rot_indio_dev);
err_alloc_rot:
	pr_err("[SSP]: failed to allocate memory for iio rot device\n");
	iio_device_unregister(data->game_rot_indio_dev);
err_register_device_game_rot:
	pr_err("[SSP]: failed to register game_rot device\n");
	iio_buffer_unregister(data->game_rot_indio_dev);
err_register_buffer_game_rot:
	pr_err("[SSP]: failed to register game_rot buffer\n");
	ssp_iio_unconfigure_ring(data->game_rot_indio_dev);
err_config_ring_game_rot:
	pr_err("[SSP]: failed to configure game_rot ring buffer\n");
	iio_device_free(data->game_rot_indio_dev);
err_alloc_game_rot:
	pr_err("[SSP]: failed to allocate memory for iio game_rot device\n");
	iio_device_unregister(data->gyro_indio_dev);
err_register_device_gyro:
	pr_err("[SSP]: failed to register gyro device\n");
	iio_buffer_unregister(data->gyro_indio_dev);
err_register_buffer_gyro:
	pr_err("[SSP]: failed to register gyro buffer\n");
	ssp_iio_unconfigure_ring(data->gyro_indio_dev);
err_config_ring_gyro:
	pr_err("[SSP]: failed to configure gyro ring buffer\n");
	iio_device_free(data->gyro_indio_dev);
err_alloc_gyro:
	pr_err("[SSP]: failed to allocate memory for iio gyro device\n");
	iio_device_unregister(data->accel_indio_dev);
err_register_device_accel:
	pr_err("[SSP]: failed to register accel device\n");
	iio_buffer_unregister(data->accel_indio_dev);
err_register_buffer_accel:
	pr_err("[SSP]: failed to register accel buffer\n");
	ssp_iio_unconfigure_ring(data->accel_indio_dev);
err_config_ring_accel:
	pr_err("[SSP]: failed to configure accel ring buffer\n");
	iio_device_free(data->accel_indio_dev);
err_alloc_accel:
	pr_err("[SSP]: failed to allocate memory for iio accel device\n");
	return ERROR;
}

void remove_input_dev(struct ssp_data *data)
{
	input_unregister_device(data->mag_input_dev);
	input_unregister_device(data->uncal_mag_input_dev);
	input_unregister_device(data->sig_motion_input_dev);
	input_unregister_device(data->uncalib_gyro_input_dev);
	input_unregister_device(data->step_cnt_input_dev);
	input_unregister_device(data->meta_input_dev);
#ifdef CONFIG_SENSORS_SSP_ADPD142
	input_unregister_device(data->hrm_lib_input_dev);
#endif
	input_unregister_device(data->tilt_wake_input_dev);
}
