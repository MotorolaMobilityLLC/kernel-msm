/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#define MT9V113_SENSOR_NAME "mt9v113"
#define PLATFORM_DRIVER_NAME "msm_camera_mt9v113"
#define mt9v113_obj mt9v113_##obj
#include "media/msm_cam_sensor.h"
/*#define CONFIG_MSMB_CAMERA_DEBUG */
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#undef pr_info
#define pr_info(fmt, args...) pr_err(fmt, ##args)
#endif
/* Sysctl registers */
#define MT9V113_COMMAND_REGISTER                0x0080
#define MT9V113_COMMAND_REGISTER_APPLY_PATCH    (1 << 0)
#define MT9V113_COMMAND_REGISTER_SET_STATE      (1 << 1)
#define MT9V113_COMMAND_REGISTER_REFRESH        (1 << 2)
#define MT9V113_COMMAND_REGISTER_WAIT_FOR_EVENT (1 << 3)
#define MT9V113_COMMAND_REGISTER_OK             (1 << 15)

DEFINE_MSM_MUTEX(mt9v113_mut);
static struct msm_sensor_ctrl_t MT9V113_s_ctrl;
struct clk *mt9v113_cam_mclk[2];

static struct msm_sensor_power_setting MT9V113_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 2,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 3,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 1,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 0,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 0,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 100,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 2,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 3,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = 3,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	}
};
#define  WORD_LEN  MSM_CAMERA_I2C_WORD_DATA

static struct msm_camera_i2c_reg_array	mt9v113_start_settings[] = {
	{0x3400, 0x7a2c, 0},
};

static int mt9v113_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_i2c_reg_array *table,
		int num)
{
	int i =	0;
	int rc = 0;
	for (i = 0; i < num; ++i) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
			s_ctrl->sensor_i2c_client, table->reg_addr,
			table->reg_data,
			MSM_CAMERA_I2C_WORD_DATA);
		if (table->delay > 0)
			msleep(table->delay);
		if (rc < 0)	{
			msleep(100);
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write(
				s_ctrl->sensor_i2c_client, table->reg_addr,
				table->reg_data,
				MSM_CAMERA_I2C_WORD_DATA);
		}
		table++;
	}
	return 0;
}
static void mt9v113_i2c_read_table(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_camera_i2c_reg_array *table,
		int num)
{
	int i = 0;
	int rc = 0;
	for (i = 0; i < num; ++i) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_read(
			s_ctrl->sensor_i2c_client, table->reg_addr,
			&table->reg_data,
			MSM_CAMERA_I2C_WORD_DATA);
		pr_info("%s addr %x, data %x ,delay %x\n",
			__func__, table->reg_addr,
			table->reg_data, table->delay);
		table++;
	}
}
struct msm_camera_i2c_reg_array mt9v113_status_tbl[] = {
	{0x3400, 0x4028, 0},
	{0x3402, 0x0004, 0},
	{0x0000, 0x2280, 0},
	{0x001A, 0x4028, 0},
	{0x0014, 0x0008, 0},
	{0x0016, 0x4028, 0},
	{0x001C, 0x0008, 0},
	{0x0010, 0x4028, 0},
	{0x301A, 0x0004, 0},
	{0x0018, 0x0008, 0},
};
/******************************************************************************
Initial Setting Table
******************************************************************************/
struct msm_camera_i2c_reg_array mt9v113_reset_tbl[] = {
	/*Reset the register at the beginning */
	{0x0018, 0x4028, 100},/* STANDBY_CONTROL */
	{0x001A, 0x0011, 10}, /* RESET_AND_MISC_CONTROL */
	{0x001A, 0x0010, 10}, /* RESET_AND_MISC_CONTROL */
	{0x0018, 0x4028, 100},/* STANDBY_CONTROL */
};

struct msm_camera_i2c_reg_array mt9v113_init_tbl[] = {
	{0x098C, 0x02F0, 0}, /* MCU_ADDRESS */
	{0x0990, 0x0000, 0}, /* MCU_DATA_0  */
	{0x098C, 0x02F2, 0}, /* MCU_ADDRESS */
	{0x0990, 0x0210, 0}, /* MCU_DATA_0  */
	{0x098C, 0x02F4, 0}, /* MCU_ADDRESS */
	{0x0990, 0x001A, 0}, /* MCU_DATA_0  */
	{0x098C, 0x2145, 0}, /* MCU_ADDRESS */
	{0x0990, 0x02F4, 0}, /* MCU_DATA_0  */
	{0x098C, 0xA134, 0}, /* MCU_ADDRESS */
	{0x0990, 0x0001, 0}, /* MCU_DATA_0  */
	{0x31E0, 0x0001, 0}, /* PIX_DEF_ID   */
	{0x001A, 0x0010, 0}, /* RESET_AND_MISC_CONTROL */
	{0x3400, 0x7A2C, 0}, /* MIPI_CONTROL */
	{0x321C, 0x8001, 0}, /* OFIFO_CONTROL_STATUS //TBD use IFP */
	{0x001E, 0x0777, 0}, /* PAD_SLEW */
	{0x0016, 0x42DF, 0}, /* CLOCKS_CONTROL */
	/* PLL  24MHz */
	{0x0014, 0xB04B, 0}, /* PLL_CONTROL */
	{0x0014, 0xB049, 0}, /* PLL_CONTROL */
	{0x0010, 0x0112, 0}, /* PLL Dividers=540 //M=18,N=1 */
	{0x0012, 0x0000, 0}, /* PLL P Dividers=0 */
	{0x0014, 0x244B, 10},/* PLL control: TEST_BYPASS on=9291 */
	/* Allow PLL to ck */
	{0x0014, 0x304B, 90},/*PLL control: PLL_ENABLE on=12363 */
	{0x0014, 0xB04A, 1}, /* PLL_CONTROL //TBD */

	{0x098C, 0x2703, 0}, /* Output Width (A) */
	{0x0990, 0x0280, 0}, /* =640             */
	{0x098C, 0x2705, 0}, /* Output Height (A)*/
	{0x0990, 0x01E0, 0}, /* =480             */
	{0x098C, 0x2707, 0}, /* Output Width (B) */
	{0x0990, 0x0280, 0}, /* =640             */
	{0x098C, 0x2709, 0}, /* Output Height (B)*/
	{0x0990, 0x01E0, 0}, /* =480             */
	{0x098C, 0x270D, 0}, /* Row Start (A)    */
	{0x0990, 0x0000, 0}, /* =0               */
	{0x098C, 0x270F, 0}, /* Column Start (A) */
	{0x0990, 0x0000, 0}, /* =0               */
	{0x098C, 0x2711, 0}, /* Row End (A)      */
	{0x0990, 0x01E7, 0}, /* =487             */
	{0x098C, 0x2713, 0}, /* Column End (A)   */
	{0x0990, 0x0287, 0}, /* =647             */
	{0x098C, 0x2715, 0}, /* Row Speed (A)    */
	{0x0990, 0x0001, 0}, /* =1               */
	{0x098C, 0x2717, 0}, /* Read Mode (A)    */
	{0x0990, 0x0026, 0}, /* =38              */
	{0x098C, 0x2719, 0}, /* sensor_fine_correction (A) */
	{0x0990, 0x001A, 0}, /* =26                        */
	{0x098C, 0x271B, 0}, /* sensor_fine_IT_min (A)     */
	{0x0990, 0x006B, 0}, /* =107                       */
	{0x098C, 0x271D, 0}, /* sensor_fine_IT_max_margin (A)*/
	{0x0990, 0x006B, 0}, /* =107                         */
	{0x098C, 0x271F, 0}, /* Frame Lines (A)              */
	{0x0990, 0x0216, 0}, /* =554                         */
	{0x098C, 0x2721, 0}, /* Line Length (A)              */
	{0x0990, 0x034A, 0}, /* =842                         */
	{0x098C, 0x2723, 0}, /* Row Start (B)                */
	{0x0990, 0x0000, 0}, /* =0                           */
	{0x098C, 0x2725, 0}, /* Column Start (B)             */
	{0x0990, 0x0000, 0}, /* =0                           */
	{0x098C, 0x2727, 0}, /* Row End (B)                  */
	{0x0990, 0x01E7, 0}, /* =487                         */
	{0x098C, 0x2729, 0}, /* Column End (B)               */
	{0x0990, 0x287 , 0}, /* =647                         */
	{0x098C, 0x272B, 0}, /* Row Speed (B)                */
	{0x0990, 0x0001, 0}, /* =1                           */
	{0x098C, 0x272D, 0}, /* Read Mode (B)                */
	{0x0990, 0x0026, 0}, /* =38                          */
	{0x098C, 0x272F, 0}, /* sensor_fine_correction (B)   */
	{0x0990, 0x001A, 0}, /* =26                          */
	{0x098C, 0x2731, 0}, /* sensor_fine_IT_min (B)       */
	{0x0990, 0x006B, 0}, /* =107                         */
	{0x098C, 0x2733, 0}, /* sensor_fine_IT_max_margin (B)*/
	{0x0990, 0x006B, 0}, /* =107                         */
	{0x098C, 0x2735, 0}, /* Frame Lines (B)              */
	{0x0990, 0x0216, 0}, /* =1135                        */
	{0x098C, 0x2737, 0}, /* Line Length (B)              */
	{0x0990, 0x034A, 0}, /* =842                         */
	{0x098C, 0x2739, 0}, /* Crop_X0 (A) */
	{0x0990, 0x0000, 0}, /* =0          */
	{0x098C, 0x273B, 0}, /* Crop_X1 (A) */
	{0x0990, 0x027F, 0}, /* =639        */
	{0x098C, 0x273D, 0}, /* Crop_Y0 (A) */
	{0x0990, 0x0000, 0}, /* =0          */
	{0x098C, 0x273F, 0}, /* Crop_Y1 (A) */
	{0x0990, 0x01DF, 0}, /* =479        */
	{0x098C, 0x2747, 0}, /* Crop_X0 (B) */
	{0x0990, 0x0000, 0}, /* =0          */
	{0x098C, 0x2749, 0}, /* Crop_X1 (B) */
	{0x0990, 0x027F, 0}, /* =639        */
	{0x098C, 0x274B, 0}, /* Crop_Y0 (B) */
	{0x0990, 0x0000, 0}, /* =0          */
	{0x098C, 0x274D, 0}, /* Crop_Y1 (B) */
	{0x0990, 0x01DF, 0}, /* =479        */
	{0x098C, 0x222D, 0}, /* R9 Step        */
	{0x0990, 0x0086, 0}, /* =139           */
	{0x098C, 0xA408, 0}, /* search_f1_50   */
	{0x0990, 0x0020, 0}, /* =33            */
	{0x098C, 0xA409, 0}, /* search_f2_50   */
	{0x0990, 0x0022, 0}, /* =35            */
	{0x098C, 0xA40A, 0}, /* search_f1_60   */
	{0x0990, 0x0027, 0}, /* =40            */
	{0x098C, 0xA40B, 0}, /* search_f2_60   */
	{0x0990, 0x0029, 0}, /* =42            */
	{0x098C, 0x2411, 0}, /* R9_Step_60 (A) */
	{0x0990, 0x0086, 0}, /* =139           */
	{0x098C, 0x2413, 0}, /* R9_Step_50 (A) */
	{0x0990, 0x00A0, 0}, /* =166           */
	{0x098C, 0x2415, 0}, /* R9_Step_60 (B) */
	{0x0990, 0x0086, 0}, /* =139           */
	{0x098C, 0x2417, 0}, /* R9_Step_50 (B) */
	{0x0990, 0x00A0, 0}, /* =166           */
	{0x098C, 0xA404, 0}, /* FD Mode        */
	{0x0990, 0x0010, 0}, /* =16            */
	{0x098C, 0xA40D, 0}, /* Stat_min       */
	{0x0990, 0x0002, 0}, /* =2             */
	{0x098C, 0xA40E, 0}, /* Stat_max       */
	{0x0990, 0x0003, 0}, /* =3             */
	{0x098C, 0xA410, 0}, /* Min_amplitude  */
	{0x0990, 0x000A, 0}, /* =10            */
	/* Lens Shading */
	/* 85% LSC */
	{0x3210, 0x09B0, 0}, /* COLOR_PIPELINE_CONTROL */
	{0x364E, 0x0210, 0}, /* P_GR_P0Q0 */
	{0x3650, 0x0CCA, 0}, /* P_GR_P0Q1 */
	{0x3652, 0x2D12, 0}, /* P_GR_P0Q2 */
	{0x3654, 0xCD0C, 0}, /* P_GR_P0Q3 */
	{0x3656, 0xC632, 0}, /* P_GR_P0Q4 */
	{0x3658, 0x00D0, 0}, /* P_RD_P0Q0 */
	{0x365A, 0x60AA, 0}, /* P_RD_P0Q1 */
	{0x365C, 0x7272, 0}, /* P_RD_P0Q2 */
	{0x365E, 0xFE09, 0}, /* P_RD_P0Q3 */
	{0x3660, 0xAD72, 0}, /* P_RD_P0Q4 */
	{0x3662, 0x0170, 0}, /* P_BL_P0Q0 */
	{0x3664, 0x5DCB, 0}, /* P_BL_P0Q1 */
	{0x3666, 0x27D2, 0}, /* P_BL_P0Q2 */
	{0x3668, 0xCE4D, 0}, /* P_BL_P0Q3 */
	{0x366A, 0x9DB3, 0}, /* P_BL_P0Q4 */
	{0x366C, 0x0150, 0}, /* P_GB_P0Q0 */
	{0x366E, 0xB809, 0}, /* P_GB_P0Q1 */
	{0x3670, 0x30B2, 0}, /* P_GB_P0Q2 */
	{0x3672, 0x82AD, 0}, /* P_GB_P0Q3 */
	{0x3674, 0xC1D2, 0}, /* P_GB_P0Q4 */
	{0x3676, 0xC4CD, 0}, /* P_GR_P1Q0 */
	{0x3678, 0xBE47, 0}, /* P_GR_P1Q1 */
	{0x367A, 0x5E4F, 0}, /* P_GR_P1Q2 */
	{0x367C, 0x9F10, 0}, /* P_GR_P1Q3 */
	{0x367E, 0xEC30, 0}, /* P_GR_P1Q4 */
	{0x3680, 0x914D, 0}, /* P_RD_P1Q0 */
	{0x3682, 0x846A, 0}, /* P_RD_P1Q1 */
	{0x3684, 0x66AB, 0}, /* P_RD_P1Q2 */
	{0x3686, 0x20D0, 0}, /* P_RD_P1Q3 */
	{0x3688, 0x1714, 0}, /* P_RD_P1Q4 */
	{0x368A, 0x99AC, 0}, /* P_BL_P1Q0 */
	{0x368C, 0x5CED, 0}, /* P_BL_P1Q1 */
	{0x368E, 0x00B1, 0}, /* P_BL_P1Q2 */
	{0x3690, 0x716C, 0}, /* P_BL_P1Q3 */
	{0x3692, 0x9594, 0}, /* P_BL_P1Q4 */
	{0x3694, 0xA22D, 0}, /* P_GB_P1Q0 */
	{0x3696, 0xB88A, 0}, /* P_GB_P1Q1 */
	{0x3698, 0x02B0, 0}, /* P_GB_P1Q2 */
	{0x369A, 0xC38F, 0}, /* P_GB_P1Q3 */
	{0x369C, 0x2B30, 0}, /* P_GB_P1Q4 */
	{0x369E, 0x6B32, 0}, /* P_GR_P2Q0 */
	{0x36A0, 0x128C, 0}, /* P_GR_P2Q1 */
	{0x36A2, 0x2574, 0}, /* P_GR_P2Q2 */
	{0x36A4, 0xD1B3, 0}, /* P_GR_P2Q3 */
	{0x36A6, 0xC2B8, 0}, /* P_GR_P2Q4 */
	{0x36A8, 0x1893, 0}, /* P_RD_P2Q0 */
	{0x36AA, 0x8DB0, 0}, /* P_RD_P2Q1 */
	{0x36AC, 0x2134, 0}, /* P_RD_P2Q2 */
	{0x36AE, 0x9014, 0}, /* P_RD_P2Q3 */
	{0x36B0, 0xFC57, 0}, /* P_RD_P2Q4 */
	{0x36B2, 0x2DB2, 0}, /* P_BL_P2Q0 */
	{0x36B4, 0x8FB1, 0}, /* P_BL_P2Q1 */
	{0x36B6, 0x9832, 0}, /* P_BL_P2Q2 */
	{0x36B8, 0x1CD4, 0}, /* P_BL_P2Q3 */
	{0x36BA, 0xE437, 0}, /* P_BL_P2Q4 */
	{0x36BC, 0x5992, 0}, /* P_GB_P2Q0 */
	{0x36BE, 0x99AF, 0}, /* P_GB_P2Q1 */
	{0x36C0, 0x0F54, 0}, /* P_GB_P2Q2 */
	{0x36C2, 0x9A52, 0}, /* P_GB_P2Q3 */
	{0x36C4, 0xB358, 0}, /* P_GB_P2Q4 */
	{0x36C6, 0xC3EE, 0}, /* P_GR_P3Q0 */
	{0x36C8, 0xC3F1, 0}, /* P_GR_P3Q1 */
	{0x36CA, 0x94D2, 0}, /* P_GR_P3Q2 */
	{0x36CC, 0x4175, 0}, /* P_GR_P3Q3 */
	{0x36CE, 0x4EB7, 0}, /* P_GR_P3Q4 */
	{0x36D0, 0xF310, 0}, /* P_RD_P3Q0 */
	{0x36D2, 0x0C51, 0}, /* P_RD_P3Q1 */
	{0x36D4, 0x6C75, 0}, /* P_RD_P3Q2 */
	{0x36D6, 0xDA96, 0}, /* P_RD_P3Q3 */
	{0x36D8, 0x8FF9, 0}, /* P_RD_P3Q4 */
	{0x36DA, 0x9C10, 0}, /* P_BL_P3Q0 */
	{0x36DC, 0x99B2, 0}, /* P_BL_P3Q1 */
	{0x36DE, 0xFCD4, 0}, /* P_BL_P3Q2 */
	{0x36E0, 0x6CD5, 0}, /* P_BL_P3Q3 */
	{0x36E2, 0x7E98, 0}, /* P_BL_P3Q4 */
	{0x36E4, 0xE64E, 0}, /* P_GB_P3Q0 */
	{0x36E6, 0x8E72, 0}, /* P_GB_P3Q1 */
	{0x36E8, 0x38D2, 0}, /* P_GB_P3Q2 */
	{0x36EA, 0x4935, 0}, /* P_GB_P3Q3 */
	{0x36EC, 0xBCB6, 0}, /* P_GB_P3Q4 */
	{0x36EE, 0xB5F3, 0}, /* P_GR_P4Q0 */
	{0x36F0, 0xBAD4, 0}, /* P_GR_P4Q1 */
	{0x36F2, 0x8E39, 0}, /* P_GR_P4Q2 */
	{0x36F4, 0x1FF8, 0}, /* P_GR_P4Q3 */
	{0x36F6, 0x1D3C, 0}, /* P_GR_P4Q4 */
	{0x36F8, 0x8CB3, 0}, /* P_RD_P4Q0 */
	{0x36FA, 0x8834, 0}, /* P_RD_P4Q1 */
	{0x36FC, 0xBF58, 0}, /* P_RD_P4Q2 */
	{0x36FE, 0x4239, 0}, /* P_RD_P4Q3 */
	{0x3700, 0x19F9, 0}, /* P_RD_P4Q4 */
	{0x3702, 0x770D, 0}, /* P_BL_P4Q0 */
	{0x3704, 0x7234, 0}, /* P_BL_P4Q1 */
	{0x3706, 0xCB98, 0}, /* P_BL_P4Q2 */
	{0x3708, 0x84B9, 0}, /* P_BL_P4Q3 */
	{0x370A, 0x33FC, 0}, /* P_BL_P4Q4 */
	{0x370C, 0xA1D2, 0}, /* P_GB_P4Q0 */
	{0x370E, 0xAE33, 0}, /* P_GB_P4Q1 */
	{0x3710, 0x8E79, 0}, /* P_GB_P4Q2 */
	{0x3712, 0x4AB8, 0}, /* P_GB_P4Q3 */
	{0x3714, 0x2D1C, 0}, /* P_GB_P4Q4 */
	{0x3644, 0x013C, 0}, /* POLY_ORIGIN_C */
	{0x3642, 0x00E8, 0}, /* POLY_ORIGIN_R */
	{0x3210, 0x09B8, 0}, /* COLOR_PIPELINE_CONTROL */
	/* Char Setting and fine-tuning */
	{0x098C, 0xAB1F, 0}, /* MCU_ADDRESS [HG_LLMODE]               */
	{0x0990, 0x00C7, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xAB31, 0}, /* MCU_ADDRESS [HG_NR_STOP_G]            */
	{0x0990, 0x001E, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x274F, 0}, /* MCU_ADDRESS [RESERVED_MODE_4F]        */
	{0x0990, 0x0004, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2741, 0}, /* MCU_ADDRESS [RESERVED_MODE_41]        */
	{0x0990, 0x0004, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xAB20, 0}, /* MCU_ADDRESS [HG_LL_SAT1]              */
	{0x0990, 0x0054, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xAB21, 0}, /* MCU_ADDRESS [HG_LL_INTERPTHRESH1]     */
	{0x0990, 0x0046, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xAB22, 0}, /* MCU_ADDRESS [HG_LL_APCORR1]           */
	{0x0990, 0x0002, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xAB24, 0}, /* MCU_ADDRESS [HG_LL_SAT2]              */
	{0x0990, 0x0005, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2B28, 0}, /* MCU_ADDRESS [HG_LL_BRIGHTNESSSTART]   */
	{0x0990, 0x170C, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2B2A, 0}, /* MCU_ADDRESS [HG_LL_BRIGHTNESSSTOP]    */
	{0x0990, 0x3E80, 0}, /* MCU_DATA_0                            */
	{0x3210, 0x09B8, 0}, /* COLOR_PIPELINE_CONTROL                */
	{0x098C, 0x2306, 0}, /* MCU_ADDRESS [AWB_CCM_L_0]             */
	{0x0990, 0x0315, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2308, 0}, /* MCU_ADDRESS [AWB_CCM_L_1]             */
	{0x0990, 0xFDDC, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x230A, 0}, /* MCU_ADDRESS [AWB_CCM_L_2]             */
	{0x0990, 0x003A, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x230C, 0}, /* MCU_ADDRESS [AWB_CCM_L_3]             */
	{0x0990, 0xFF58, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x230E, 0}, /* MCU_ADDRESS [AWB_CCM_L_4]             */
	{0x0990, 0x02B7, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2310, 0}, /* MCU_ADDRESS [AWB_CCM_L_5]             */
	{0x0990, 0xFF31, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2312, 0}, /* MCU_ADDRESS [AWB_CCM_L_6]             */
	{0x0990, 0xFF4C, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2314, 0}, /* MCU_ADDRESS [AWB_CCM_L_7]             */
	{0x0990, 0xFE4C, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2316, 0}, /* MCU_ADDRESS [AWB_CCM_L_8]             */
	{0x0990, 0x039E, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2318, 0}, /* MCU_ADDRESS [AWB_CCM_L_9]             */
	{0x0990, 0x001C, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x231A, 0}, /* MCU_ADDRESS [AWB_CCM_L_10]            */
	{0x0990, 0x0039, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x231C, 0}, /* MCU_ADDRESS [AWB_CCM_RL_0]            */
	{0x0990, 0x007F, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x231E, 0}, /* MCU_ADDRESS [AWB_CCM_RL_1]            */
	{0x0990, 0xFF77, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2320, 0}, /* MCU_ADDRESS [AWB_CCM_RL_2]            */
	{0x0990, 0x000A, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2322, 0}, /* MCU_ADDRESS [AWB_CCM_RL_3]            */
	{0x0990, 0x0020, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2324, 0}, /* MCU_ADDRESS [AWB_CCM_RL_4]            */
	{0x0990, 0x001B, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2326, 0}, /* MCU_ADDRESS [AWB_CCM_RL_5]            */
	{0x0990, 0xFFC6, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2328, 0}, /* MCU_ADDRESS [AWB_CCM_RL_6]            */
	{0x0990, 0x0086, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x232A, 0}, /* MCU_ADDRESS [AWB_CCM_RL_7]            */
	{0x0990, 0x00B5, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x232C, 0}, /* MCU_ADDRESS [AWB_CCM_RL_8]            */
	{0x0990, 0xFEC3, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x232E, 0}, /* MCU_ADDRESS [AWB_CCM_RL_9]            */
	{0x0990, 0x0001, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2330, 0}, /* MCU_ADDRESS [AWB_CCM_RL_10]           */
	{0x0990, 0xFFEF, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA348, 0}, /* MCU_ADDRESS [AWB_GAIN_BUFFER_SPEED]   */
	{0x0990, 0x0008, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA349, 0}, /* MCU_ADDRESS [AWB_JUMP_DIVISOR]        */
	{0x0990, 0x0002, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA34A, 0}, /* MCU_ADDRESS [AWB_GAIN_MIN]            */
	{0x0990, 0x0090, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA34B, 0}, /* MCU_ADDRESS [AWB_GAIN_MAX]            */
	{0x0990, 0x00FF, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA34C, 0}, /* MCU_ADDRESS [AWB_GAINMIN_B]           */
	{0x0990, 0x0075, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA34D, 0}, /* MCU_ADDRESS [AWB_GAINMAX_B]           */
	{0x0990, 0x00EF, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA351, 0}, /* MCU_ADDRESS [AWB_CCM_POSITION_MIN]    */
	{0x0990, 0x0000, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA352, 0}, /* MCU_ADDRESS [AWB_CCM_POSITION_MAX]    */
	{0x0990, 0x007F, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA354, 0}, /* MCU_ADDRESS [AWB_SATURATION]          */
	{0x0990, 0x0043, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA355, 0}, /* MCU_ADDRESS [AWB_MODE]                */
	{0x0990, 0x0001, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA35D, 0}, /* MCU_ADDRESS [AWB_STEADY_BGAIN_OUT_MIN]*/
	{0x0990, 0x0078, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA35E, 0}, /* MCU_ADDRESS [AWB_STEADY_BGAIN_OUT_MAX]*/
	{0x0990, 0x0086, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA35F, 0}, /* MCU_ADDRESS [AWB_STEADY_BGAIN_IN_MIN] */
	{0x0990, 0x007E, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA360, 0}, /* MCU_ADDRESS [AWB_STEADY_BGAIN_IN_MAX] */
	{0x0990, 0x0082, 0}, /* MCU_DATA_0                            */
	{0x098C, 0x2361, 0}, /* MCU_ADDRESS [AWB_CNT_PXL_TH]          */
	{0x0990, 0x0040, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA363, 0}, /* MCU_ADDRESS [AWB_TG_MIN0]             */
	{0x0990, 0x00D2, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA364, 0}, /* MCU_ADDRESS [AWB_TG_MAX0]             */
	{0x0990, 0x00F6, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA302, 0}, /* MCU_ADDRESS [AWB_WINDOW_POS]          */
	{0x0990, 0x0000, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA303, 0}, /* MCU_ADDRESS [AWB_WINDOW_SIZE]         */
	{0x0990, 0x00EF, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xAB20, 0}, /* MCU_ADDRESS [HG_LL_SAT1]              */
	{0x0990, 0x0024, 0}, /* MCU_DATA_0                            */
	/* AE Setting Fr ame rate 7.5fps ~ 30fps*/
	{0x098C, 0xA20C, 0}, /* MCU_ADDRESS                           */
	{0x0990, 0x0004, 0}, /* AE_MAX_INDEX                          */
	{0x098C, 0xA215, 0}, /* MCU_ADDRESS                           */
	{0x0990, 0x0004, 0}, /* AE_INDEX_TH23                         */
	{0x098C, 0xA20D, 0}, /* MCU_ADDRESS [AE_MIN_VIRTGAIN]         */
	{0x0990, 0x0030, 0}, /* MCU_DATA_0                            */
	{0x098C, 0xA20E, 0}, /* MCU_ADDRESS [AE_MAX_VIRTGAIN]         */
	{0x0990, 0x0080, 0}, /* MCU_DATA_0                            */
	/* Sharpness */
	{0x098C, 0xAB22, 0}, /* MCU_ADDRESS [HG_LL_APCORR1]           */
	{0x0990, 0x0001, 0}, /* MCU_DATA_0                            */
	/* AE Gate */
	{0x098C, 0xA207, 0}, /* MCU_ADDRESS [AE_GATE] */
	{0x0990, 0x0006, 0}, /* MCU_DATA_0            */
	/* AE Target */
	{0x098C, 0xA24F, 0}, /* MCU_ADDRESS [AE_BASETARGET]      */
	{0x0990, 0x0044, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB3C, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_0] */
	{0x0990, 0x0000, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB3D, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_1] */
	{0x0990, 0x0007, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB3E, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_2] */
	{0x0990, 0x0016, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB3F, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_3] */
	{0x0990, 0x0039, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB40, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_4] */
	{0x0990, 0x005F, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB41, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_5] */
	{0x0990, 0x007A, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB42, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_6] */
	{0x0990, 0x008F, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB43, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_7] */
	{0x0990, 0x00A1, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB44, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_8] */
	{0x0990, 0x00AF, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB45, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_9] */
	{0x0990, 0x00BB, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB46, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_10]*/
	{0x0990, 0x00C6, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB47, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_11]*/
	{0x0990, 0x00CF, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB48, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_12]*/
	{0x0990, 0x00D8, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB49, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_13]*/
	{0x0990, 0x00E0, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB4A, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_14]*/
	{0x0990, 0x00E7, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB4B, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_15]*/
	{0x0990, 0x00EE, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB4C, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_16]*/
	{0x0990, 0x00F4, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB4D, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_17]*/
	{0x0990, 0x00FA, 0}, /* MCU_DATA_0                       */
	{0x098C, 0xAB4E, 0}, /* MCU_ADDRESS [HG_GAMMA_TABLE_A_18]*/
	{0x0990, 0x00FF, 0}, /* MCU_DATA_0                       */
	/* saturation */
	{0x098C, 0xAB20, 0}, /* MCU_ADDRESS [HG_LL_SAT1]*/
	{0x0990, 0x0045, 0}, /* MCU_DATA_0              */
	{0x098C, 0xAB24, 0}, /* MCU_ADDRESS [HG_LL_SAT2]*/
	{0x0990, 0x0034, 0}, /* MCU_DATA_0              */
	{0x098C, 0xA20C, 0}, /* MCU_ADDRESS             */
	{0x0990, 0x0004, 0}, /* AE_MAX_INDEX            */
	{0x098C, 0xA215, 0}, /* MCU_ADDRESS             */
	{0x0990, 0x0004, 0}, /* AE_INDEX_TH23           */
	{0x098C, 0xA34A, 0}, /* MCU_ADDRESS             */
	{0x0990, 0x007A, 0}, /* MCU_DATA_0              */
	{0x098C, 0xA103, 0}, /* MCU_ADDRESS             */
	{0x0990, 0x0006, 200}, /* MCU_DATA_0 */
	{0x098C, 0xA103, 0}, /* MCU_ADDRESS  */
	{0x0990, 0x0005, 100}, /* MCU_DATA_0 */
	{0x098C, 0xA102, 0},
	{0x0990, 0x000F, 0},

	{0x098C, 0xA244, 0},
	{0x0990, 0x00BB, 0},
	{0x3400, 0x7a26, 0},
};

static struct v4l2_subdev_info MT9V113_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id MT9V113_i2c_id[] = {
	{MT9V113_SENSOR_NAME, (kernel_ulong_t)&MT9V113_s_ctrl},
	{ }
};

static int32_t msm_MT9V113_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &MT9V113_s_ctrl);
}

static struct i2c_driver MT9V113_i2c_driver = {
	.id_table = MT9V113_i2c_id,
	.probe  = msm_MT9V113_i2c_probe,
	.driver = {
		.name = MT9V113_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client MT9V113_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id MT9V113_dt_match[] = {
	{.compatible = "qcom,mt9v113", .data = &MT9V113_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, MT9V113_dt_match);

static struct platform_driver MT9V113_platform_driver = {
	.driver = {
		.name = "qcom,mt9v113",
		.owner = THIS_MODULE,
		.of_match_table = MT9V113_dt_match,
	},
};

static int32_t MT9V113_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(MT9V113_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init MT9V113_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&MT9V113_platform_driver,
		MT9V113_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&MT9V113_i2c_driver);
}

static void __exit MT9V113_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (MT9V113_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&MT9V113_s_ctrl);
		platform_driver_unregister(&MT9V113_platform_driver);
	} else
		i2c_del_driver(&MT9V113_i2c_driver);
	return;
}

int32_t MT9V113_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	int32_t rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	pr_info("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		pr_info("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		pr_info("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			pr_info("%s:%d subdev_id[%d] %d\n", __func__,
				__LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		pr_info("%s:%d mount angle valid %d value %d\n",
			__func__, __LINE__,
			cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		mt9v113_i2c_write_table(s_ctrl, mt9v113_reset_tbl,
			ARRAY_SIZE(mt9v113_reset_tbl));
		mt9v113_i2c_write_table(s_ctrl, mt9v113_init_tbl,
			ARRAY_SIZE(mt9v113_init_tbl));
		mt9v113_i2c_read_table(s_ctrl, mt9v113_status_tbl,
			ARRAY_SIZE(mt9v113_status_tbl));
		break;
	case CFG_SET_RESOLUTION:
		break;
	case CFG_SET_STOP_STREAM:
		break;
	case CFG_SET_START_STREAM:
		pr_info("CFG_SET_START_STREAM\n");
		mt9v113_i2c_write_table(s_ctrl, mt9v113_start_settings,
			ARRAY_SIZE(mt9v113_start_settings));
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		pr_info("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr)
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info.power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(struct msm_sensor_power_setting)
				      * size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
				rc = -ENOMEM;
				break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;

		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info.power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_info("%s sensor id 0x%x\n", __func__,
			sensor_slave_info.slave_addr);
		pr_info("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		pr_info("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		pr_info("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			pr_info("%s i %d power setting %d %d %ld %d\n",
				__func__, slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
		    sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
		}
	case CFG_SET_SATURATION: {
		int32_t sat_lev;
		if (copy_from_user(&sat_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Saturation Value is %d", __func__, sat_lev);
		break;
		}
	case CFG_SET_CONTRAST: {
		int32_t con_lev;
		if (copy_from_user(&con_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Contrast Value is %d", __func__, con_lev);
		break;
		}
	case CFG_SET_SHARPNESS: {
		int32_t shp_lev;
		if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
			sizeof(int32_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		pr_debug("%s: Sharpness Value is %d", __func__, shp_lev);
		break;
		}
	case CFG_SET_AUTOFOCUS: {
		/* TO-DO: set the Auto Focus */
		pr_debug("%s: Setting Auto Focus", __func__);
		break;
		}
	case CFG_CANCEL_AUTOFOCUS: {
		/* TO-DO: Cancel the Auto Focus */
		pr_debug("%s: Cancelling Auto Focus", __func__);
		break;
		}
	default:
		rc = -EFAULT;
		break;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

static int32_t mt9v113_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	pr_info("%s called\n", __func__);
	msm_sensor_power_up(s_ctrl);
	rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);

	return rc;
}
static struct msm_sensor_fn_t MT9V113_sensor_func_tbl = {
	.sensor_config = MT9V113_sensor_config,
	.sensor_power_up = mt9v113_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t MT9V113_s_ctrl = {
	.sensor_i2c_client = &MT9V113_sensor_i2c_client,
	.power_setting_array.power_setting = MT9V113_power_setting,
	.power_setting_array.size = ARRAY_SIZE(MT9V113_power_setting),
	.msm_sensor_mutex = &mt9v113_mut,
	.sensor_v4l2_subdev_info = MT9V113_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(MT9V113_subdev_info),
	.func_tbl = &MT9V113_sensor_func_tbl,
};

module_init(MT9V113_init_module);
module_exit(MT9V113_exit_module);
MODULE_DESCRIPTION("Aptina VGA sensor driver");
MODULE_LICENSE("GPL v2");
