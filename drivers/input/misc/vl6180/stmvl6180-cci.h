/*
 *  stmvl6180-cci.h - Linux kernel modules for STM VL6180 FlightSense TOF sensor
 *
 *  Copyright (C) 2015 STMicroelectronics Imaging Division
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
/*
 * Defines
 */
#ifndef STMVL6180_CCI_H
#define STMVL6180_CCI_H

#ifdef CAMERA_CCI
#include <soc/qcom/camera2.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_cci.h"

#define	MSM_TOF_MAX_VREGS (10)
#define STMVL6180_GPIO_ENABLE              1
#define STMVL6180_GPIO_DISABLE             0

struct msm_tof_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_TOF_MAX_VREGS];
	int num_vreg;
};

struct cci_data {
	struct msm_camera_i2c_client g_client;
	struct msm_camera_i2c_client *client;
	struct platform_device *pdev;
	enum msm_camera_device_type_t device_type;
	enum cci_i2c_master_t cci_master;
	struct msm_tof_vreg vreg_cfg;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *subdev_ops;
	uint32_t subdev_id;
	int int_gpio;
	int en_gpio;
	uint32_t slave_addr;
	uint32_t i2c_freq_mode;
	uint8_t power_up;
	char subdev_initialized;
};
int stmvl6180_init_cci(void);
void stmvl6180_exit_cci(void *);
int stmvl6180_power_down_cci(void *);
int stmvl6180_power_up_cci(void *, unsigned int *);
#endif /* CAMERA_CCI */
#endif /* STMVL6180_CCI_H */
