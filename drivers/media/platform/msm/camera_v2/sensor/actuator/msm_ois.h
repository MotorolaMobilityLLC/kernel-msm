/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef MSM_OIS_H
#define MSM_OIS_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define	MSM_OIS_MAX_VREGS (10)

struct msm_ois_ctrl_t;

enum msm_ois_state_t {
	OIS_POWER_UP,
	OIS_POWER_DOWN,
};

struct msm_ois_func_tbl {
	int32_t (*ini_set_ois)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*enable_ois)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
	int32_t (*disable_ois)(struct msm_ois_ctrl_t *,
		struct msm_ois_set_info_t *);
};

struct msm_ois {
	struct msm_ois_func_tbl func_tbl;
};

struct msm_ois_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_OIS_MAX_VREGS];
	int num_vreg;
};

struct msm_ois_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t ois_device_type;
	struct msm_sd_subdev msm_sd;
	struct mutex *ois_mutex;
	struct msm_ois_func_tbl *func_tbl;
	enum msm_camera_i2c_data_type i2c_data_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *ois_v4l2_subdev_ops;
	void *user_data;
	uint32_t ois_pwd;
	uint32_t ois_enable;
	struct msm_camera_i2c_reg_array *i2c_reg_tbl;
	uint16_t i2c_tbl_index;
	enum cci_i2c_master_t cci_master;
	uint32_t subdev_id;
	enum msm_ois_state_t ois_state;
	struct msm_ois_vreg vreg_cfg;
};

/* OnSemi Gyro/Hall Acceptance Test defines */
/* Command Status */
#define EXE_END   0x02  /* Execute End (Adjust OK) */
#define EXE_HXADJ 0x06  /* Adjust NG : X Hall NG (Gain or Offset) */
#define EXE_HYADJ 0x0A  /* Adjust NG : Y Hall NG (Gain or Offset) */
#define EXE_LXADJ 0x12  /* Adjust NG : X Loop NG (Gain) */
#define EXE_LYADJ 0x22  /* Adjust NG : Y Loop NG (Gain) */
#define EXE_GXADJ 0x42  /* Adjust NG : X Gyro NG (offset) */
#define EXE_GYADJ 0x82  /* Adjust NG : Y Gyro NG (offset) */
#define EXE_OCADJ 0x402 /* Adjust NG : OSC Clock NG */
#define EXE_ERR   0x99  /* Execute Error End */

/* Hall Examination of Acceptance */
#define EXE_HXMVER 0x06 /* X Error */
#define EXE_HYMVER 0x0A /* Y Error */

/* Gyro Examination of Acceptance */
#define EXE_GXABOVE 0x06 /* X Above */
#define EXE_GXBELOW 0x0A /* X Below */
#define EXE_GYABOVE 0x12 /* Y Above */
#define EXE_GYBELOW 0x22 /* Y Below */

#define ACT_CHECK_LEVEL 0x3E4CCCCD /* 0.2 */
#define ACT_THRESHOLD   0x200 /* 28dB 20log(2/(0.2*256)) */
#define GEA_DIF_HIG  0x0010
#define GEA_DIF_LOW  0x0001
#define XACTTEST        10
#define YACTTEST        11

#define WC_DPON 0x0105

#define WC_DPI1ADD0 0x01B0
#define WC_DPI1ADD1 0x01B1
#define WC_DPI2ADD0 0x01B2
#define WC_DPI2ADD1 0x01B3

#define WC_DPO1ADD0 0x01B8
#define WC_DPO1ADD1 0x01B9
#define WC_DPO2ADD0 0x01BA
#define WC_DPO2ADD1 0x01BB

#define WC_SINON   0x0180
#define WC_SINFRQ0 0x0181
#define WC_SINFRQ1 0x0182
#define WC_SINPHSX 0x0183
#define WC_SINPHSY 0x0184

#define WC_RAMDLYMOD0 0x018E
#define WC_RAMDLYMOD1 0x018F
#define WC_RAMINITON  0x0102

#define WC_MESMODE     0x0190
#define WC_MESSINMODE  0x0191
#define WC_MESLOOP0    0x0192
#define WC_MESLOOP1    0x0193
#define WC_MES1ADD0    0x0194
#define WC_MES1ADD1    0x0195
#define WC_MES2ADD0    0x0196
#define WC_MES2ADD1    0x0197
#define WC_MESABS      0x0198
#define WC_MESWAIT     0x0199

#define WC_RAMACCMOD   0x018C

#define WH_EQSWX 0x0170
#define WH_EQSWY 0x0171
#define EQSINSW 0x3C

#define SXINZ1 0x1433
#define SYINZ1 0x14B3
#define SXSIN  0x10D5
#define SYSIN  0x11D5
#define SXOFFZ1 0x1461
#define SYOFFZ1 0x14E1
#define MSMEAN 0x1230

#define ON  0x01
#define OFF 0x00

#define TESTRD 0x027F

#define MES1AA 0x10F0
#define MES1AB 0x10F1
#define MES1AC 0x10F2
#define MES1AD 0x10F3
#define MES1AE 0x10F4
#define MES1BA 0x10F5
#define MES1BB 0x10F6
#define MES1BC 0x10F7
#define MES1BD 0x10F8
#define MES1BE 0x10F9

#define MES2AA 0x11F0
#define MES2AB 0x11F1
#define MES2AC 0x11F2
#define MES2AD 0x11F3
#define MES2AE 0x11F4
#define MES2BA 0x11F5
#define MES2BB 0x11F6
#define MES2BC 0x11F7
#define MES2BD 0x11F8
#define MES2BE 0x11F9

#define MSABS1AV 0x1041

#define SINXZ 0x1400
#define SINYZ 0x1480

#define MES1BZ2 0x144C
#define MES2BZ2 0x14CC

#define MSPP1AV 0x1042

#define READ_COUNT_NUM 3

#define SINEWAVE 0
#define XHALWAVE 1
#define CIRCWAVE 255

#define X_DIR 0x00 /* X Direction */
#define Y_DIR 0x01 /* Y Direction */

#define HALL_ADJ 0
#define LOOPGAIN 1
#define THROUGH  2
#define NOISE    3

#define CLR_FRAM1 0x02
#define AD2Z 0x144A
#define AD3Z 0x14CA

#endif
