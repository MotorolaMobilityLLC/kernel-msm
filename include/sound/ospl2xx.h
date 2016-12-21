/* Copyright (c) 2015-2016, Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __OSPL2XX_H__
#define __OSPL2XX_H__

#include <sound/apr_audio-v2.h>
#include <sound/soc.h>

/* module IDs */
#define AFE_CUSTOM_OPALUM_RX_MODULE                    0x00A1AF00
#define AFE_CUSTOM_OPALUM_TX_MODULE                    0x00A1BF00

/* RX module parameter IDs */
/* Get Module status */
#define PARAM_ID_OPALUM_RX_ENABLE                      0x00A1AF01
/* Switch between internal config strings */
#define PARAM_ID_OPALUM_RX_SET_USE_CASE                0x00A1AF02
/* Run production line test */
#define PARAM_ID_OPALUM_RX_RUN_CALIBRATION             0x00A1AF03
/* Initialize parameters from external config string */
#define PARAM_ID_OPALUM_RX_SET_EXTERNAL_CONFIG         0x00A1AF05
/* Get the currently set excursion model */
#define PARAM_ID_OPALUM_RX_EXC_MODEL                   0x00A1AF06
/* Get current temperature */
#define PARAM_ID_OPLAUM_RX_TEMPERATURE                 0x00A1AF07
/* Set temperature calibration values */
#define PARAM_ID_OPALUM_RX_TEMP_CAL_DATA               0x00A1AF08
/* Get the array index of the parameter set currently in use.
   Will return -1 when an external string is used */
#define PARAM_ID_OPALUM_RX_CURRENT_PARAM_SET           0x00A1AF11
/* Set volume control attenuation */
#define PARAM_ID_OPALUM_RX_VOLUME_CONTROL              0x00A1AF12

/* TX module parameter IDs */
/* Get Module status */
#define PARAM_ID_OPALUM_TX_ENABLE                      0x00A1BF01
/* Switch between internal config strings */
#define PARAM_ID_OPALUM_TX_SET_USE_CASE                0x00A1BF02
/* Run production line test */
#define PARAM_ID_OPALUM_TX_RUN_CALIBRATION             0x00A1BF03
/* Get f0 calibration values */
#define PARAM_ID_OPALUM_TX_F0_CALIBRATION_VALUE        0x00A1BF05
/* Get impedance calibration values */
#define PARAM_ID_OPALUM_TX_TEMP_MEASUREMENT_VALUE      0x00A1BF06
/* Get f0 curve */
#define PARAM_ID_OPALUM_TX_F0_CURVE                    0x00A1BF07
/* Initialize parameters from external config string */
#define PARAM_ID_OPALUM_TX_SET_EXTERNAL_CONFIG         0x00A1BF08
/* Get the array index of the parameter set currently in use.
   Will return -1 when an external string is used */
#define PARAM_ID_OPALUM_TX_CURRENT_PARAM_SET           0x00A1BF11


#define F0_CURVE_POINTS 90

struct afe_custom_opalum_set_config_t {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
	struct afe_port_param_data_v2 data;
} __packed;

struct afe_custom_opalum_get_config_t {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2 param;
	struct afe_port_param_data_v2 data;
} __packed;

struct opalum_single_data_ctrl_t {
	int32_t value;
};

struct opalum_dual_data_ctrl_t {
	int32_t data1;
	int32_t data2;
};

struct opalum_f0_calib_data_t {
	int f0;
	int ref_diff;
};

struct opalum_f0_curve_t {
	int32_t curve[F0_CURVE_POINTS];
};

struct opalum_temp_calib_data_t {
	int acc;
	int count;
};

struct opalum_tri_data_ctrl_t {
	int32_t data1;
	int32_t data2;
	int32_t data3;
};

struct opalum_external_config_t {
	uint32_t total_size;
	uint32_t chunk_size;
	uint32_t done;
	const char *config;
};

int ospl2xx_init(struct snd_soc_pcm_runtime *rtd);

#endif /* __OSPL2XX_H__ */
