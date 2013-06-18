/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MSM_OIS_H
#define __MSM_OIS_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <mach/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_sd.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

struct msm_ois_fn_t {
	void (*ois_on) (void);
	void (*ois_off) (void);
	int (*ois_mode) (enum ois_mode_t);
	int (*ois_stat) (struct msm_sensor_ois_info_t *);
};

struct msm_ois_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t act_device_type;
	struct msm_sd_subdev msm_sd;
	struct mutex *ois_mutex;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *act_v4l2_subdev_ops;

	enum cci_i2c_master_t cci_master;

	uint16_t sid_ois;
	struct msm_ois_fn_t *ois_func_tbl;
};

#ifdef CONFIG_OIS_ROHM_BU24205GWL
int msm_init_ois(void);
int msm_ois_off(void);
int msm_ois_mode(enum ois_mode_t);
int msm_ois_info(struct msm_sensor_ois_info_t *);
#endif

/*#define MSM_OIS_DEBUG*/
#undef CDBG
#ifdef MSM_OIS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#endif
