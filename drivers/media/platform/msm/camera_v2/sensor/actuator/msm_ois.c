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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_ois.h"
#include "msm_cci.h"

DEFINE_MSM_MUTEX(msm_ois_mutex);
/*#define MSM_OIS_DEBUG*/
#undef CDBG
#ifdef MSM_OIS_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define MAX_POLL_COUNT 100

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *o_ctrl);
static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *o_ctrl);

static int32_t msm_ois_gea(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t *output);

static int32_t msm_ois_hea(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_hea_t *output);

static uint16_t RunHallAcceptance(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t direction);
static uint8_t RunGyroAcceptance(struct msm_ois_ctrl_t *o_ctrl);

static void RamWrite32A(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint32_t data);
static void RegWriteA(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint32_t data);
static void RegReadA(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint8_t *reg_data);
static void RamReadA(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint16_t *reg_data);
static void RamAccessFixedMode(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t UcAccessMode);
static void BusyWait(struct msm_ois_ctrl_t *o_ctrl, uint16_t TargetAddr,
	uint8_t TargetData);
static void SetSineWaveParams(struct msm_ois_ctrl_t *o_ctrl, uint8_t UcTableVal,
	uint8_t UcMethodVal);
static uint8_t CommandReadCheck(struct msm_ois_ctrl_t *o_ctrl);
static void MeasureFilter(struct msm_ois_ctrl_t *o_ctrl, uint8_t mode);
static int16_t GenMeas(struct msm_ois_ctrl_t *o_ctrl, uint16_t UsRamAddr,
	uint8_t UcMeasMode);

static struct msm_ois msm_ois_table;

static struct i2c_driver msm_ois_i2c_driver;
static struct msm_ois *oiss[] = {
	&msm_ois_table,
};

static int32_t msm_ois_write_settings(struct msm_ois_ctrl_t *o_ctrl,
	uint16_t size, struct reg_settings_ois_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	struct msm_camera_i2c_seq_reg_array reg_setting;
	CDBG("Enter\n");

	for (i = 0; i < size; i++) {
		switch (settings[i].i2c_operation) {
		case MSM_OIS_WRITE: {
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
				rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_write(
					&o_ctrl->i2c_client,
					settings[i].reg_addr,
					settings[i].reg_data,
					MSM_CAMERA_I2C_BYTE_DATA);
				break;
			case MSM_CAMERA_I2C_WORD_DATA:
				rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_write(
					&o_ctrl->i2c_client,
					settings[i].reg_addr,
					settings[i].reg_data,
					MSM_CAMERA_I2C_WORD_DATA);
				break;
			case MSM_CAMERA_I2C_DWORD_DATA:
				reg_setting.reg_addr = settings[i].reg_addr;
				reg_setting.reg_data[0] = (uint8_t)
					((settings[i].reg_data &
					0xFF000000) >> 24);
				reg_setting.reg_data[1] = (uint8_t)
					((settings[i].reg_data &
					0x00FF0000) >> 16);
				reg_setting.reg_data[2] = (uint8_t)
					((settings[i].reg_data &
					0x0000FF00) >> 8);
				reg_setting.reg_data[3] = (uint8_t)
					(settings[i].reg_data & 0x000000FF);
				reg_setting.reg_data_size = 4;
				rc = o_ctrl->i2c_client.i2c_func_tbl->
					i2c_write_seq(&o_ctrl->i2c_client,
					reg_setting.reg_addr,
					reg_setting.reg_data,
					reg_setting.reg_data_size);
				if (rc < 0)
					return rc;
				break;

			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
		}
			break;

		case MSM_OIS_POLL: {
			int32_t poll_count = 0;
			switch (settings[i].data_type) {
			case MSM_CAMERA_I2C_BYTE_DATA:
				do {
					rc = o_ctrl->i2c_client.i2c_func_tbl
						->i2c_poll(&o_ctrl->i2c_client,
						settings[i].reg_addr,
						settings[i].reg_data,
						MSM_CAMERA_I2C_BYTE_DATA);

					if (poll_count++ > MAX_POLL_COUNT) {
						pr_err("MSM_OIS_POLL failed");
						break;
					}
				} while (rc != 0);
				break;
			case MSM_CAMERA_I2C_WORD_DATA:
				do {
					rc = o_ctrl->i2c_client.i2c_func_tbl
						->i2c_poll(&o_ctrl->i2c_client,
						settings[i].reg_addr,
						settings[i].reg_data,
						MSM_CAMERA_I2C_WORD_DATA);

					if (poll_count++ > MAX_POLL_COUNT) {
						pr_err("MSM_OIS_POLL failed");
						break;
					}
				} while (rc != 0);
				break;
			default:
				pr_err("Unsupport data type: %d\n",
					settings[i].data_type);
				break;
			}
		}
		}

		if (0 != settings[i].delay)
			msleep(settings[i].delay);

		if (rc < 0)
			break;
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ini_set_ois(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *init_settings = NULL;
	int32_t rc = -EFAULT;
	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = o_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->ois_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = o_ctrl->cci_master;
	} else {
		o_ctrl->i2c_client.client->addr =
			set_info->ois_params.i2c_addr;
	}
	o_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;

	if (set_info->ois_params.init_setting_size) {
		init_settings = kmalloc(sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.init_setting_size),
			GFP_KERNEL);
		if (init_settings == NULL) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error allocating memory for init_settings\n");
			return -EFAULT;
		}
		if (copy_from_user(init_settings,
			(void *)set_info->ois_params.init_settings,
			set_info->ois_params.init_setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(init_settings);
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error copying init_settings\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.init_setting_size, init_settings);
		kfree(init_settings);
		if (rc < 0) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_gea(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t *output)
{
	*output = RunGyroAcceptance(o_ctrl);

	return 0;
}

static int32_t msm_ois_hea(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_hea_t *output)
{
	output->x = RunHallAcceptance(o_ctrl, X_DIR);
	output->y = RunHallAcceptance(o_ctrl, Y_DIR);

	return 0;
}

static void MeasureFilter(struct msm_ois_ctrl_t *o_ctrl, uint8_t mode)
{
	if (mode == NOISE) {
		/* Measure Filter1 Setting */
		RamWrite32A(o_ctrl, MES1AA, 0x3CA175C0); /* 0x10F0 LPF150Hz */
		RamWrite32A(o_ctrl, MES1AB, 0x3CA175C0); /* 0x10F1 */
		RamWrite32A(o_ctrl, MES1AC, 0x3F75E8C0); /* 0x10F2 */
		RamWrite32A(o_ctrl, MES1AD, 0x00000000); /* 0x10F3 */
		RamWrite32A(o_ctrl, MES1AE, 0x00000000); /* 0x10F4 */
		RamWrite32A(o_ctrl, MES1BA, 0x3CA175C0); /* 0x10F5 LPF150Hz */
		RamWrite32A(o_ctrl, MES1BB, 0x3CA175C0); /* 0x10F6 */
		RamWrite32A(o_ctrl, MES1BC, 0x3F75E8C0); /* 0x10F7 */
		RamWrite32A(o_ctrl, MES1BD, 0x00000000); /* 0x10F8 */
		RamWrite32A(o_ctrl, MES1BE, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A(o_ctrl, MES2AA, 0x3CA175C0); /* 0x11F0 LPF150Hz */
		RamWrite32A(o_ctrl, MES2AB, 0x3CA175C0); /* 0x11F1 */
		RamWrite32A(o_ctrl, MES2AC, 0x3F75E8C0); /* 0x11F2 */
		RamWrite32A(o_ctrl, MES2AD, 0x00000000); /* 0x11F3 */
		RamWrite32A(o_ctrl, MES2AE, 0x00000000); /* 0x11F4 */
		RamWrite32A(o_ctrl, MES2BA, 0x3CA175C0); /* 0x11F5 LPF150Hz */
		RamWrite32A(o_ctrl, MES2BB, 0x3CA175C0); /* 0x11F6 */
		RamWrite32A(o_ctrl, MES2BC, 0x3F75E8C0); /* 0x11F7 */
		RamWrite32A(o_ctrl, MES2BD, 0x00000000); /* 0x11F8 */
		RamWrite32A(o_ctrl, MES2BE, 0x00000000); /* 0x11F9 */
	} else if (mode == THROUGH) { /* for Through */
		/* Measure Filter1 Setting */
		RamWrite32A(o_ctrl, MES1AA, 0x3F800000); /* 0x10F0 Through */
		RamWrite32A(o_ctrl, MES1AB, 0x00000000); /* 0x10F1 */
		RamWrite32A(o_ctrl, MES1AC, 0x00000000); /* 0x10F2 */
		RamWrite32A(o_ctrl, MES1AD, 0x00000000); /* 0x10F3 */
		RamWrite32A(o_ctrl, MES1AE, 0x00000000); /* 0x10F4 */
		RamWrite32A(o_ctrl, MES1BA, 0x3F800000); /* 0x10F5  Through */
		RamWrite32A(o_ctrl, MES1BB, 0x00000000); /* 0x10F6 */
		RamWrite32A(o_ctrl, MES1BC, 0x00000000); /* 0x10F7 */
		RamWrite32A(o_ctrl, MES1BD, 0x00000000); /* 0x10F8 */
		RamWrite32A(o_ctrl, MES1BE, 0x00000000); /* 0x10F9 */

		/* Measure Filter2 Setting */
		RamWrite32A(o_ctrl, MES2AA, 0x3F800000); /* 0x11F0 Through */
		RamWrite32A(o_ctrl, MES2AB, 0x00000000); /* 0x11F1 */
		RamWrite32A(o_ctrl, MES2AC, 0x00000000); /* 0x11F2 */
		RamWrite32A(o_ctrl, MES2AD, 0x00000000); /* 0x11F3 */
		RamWrite32A(o_ctrl, MES2AE, 0x00000000); /* 0x11F4 */
		RamWrite32A(o_ctrl, MES2BA, 0x3F800000); /* 0x11F5 Through */
		RamWrite32A(o_ctrl, MES2BB, 0x00000000); /* 0x11F6 */
		RamWrite32A(o_ctrl, MES2BC, 0x00000000); /* 0x11F7 */
		RamWrite32A(o_ctrl, MES2BD, 0x00000000); /* 0x11F8 */
		RamWrite32A(o_ctrl, MES2BE, 0x00000000); /* 0x11F9 */
	}

	return;
}

static uint16_t RunHallAcceptance(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t direction)
{
	uint16_t MsppVal;

	MeasureFilter(o_ctrl, NOISE);

	if (!direction) {
		RamWrite32A(o_ctrl, SXSIN, ACT_CHECK_LEVEL); /* 0x10D5 */
		RamWrite32A(o_ctrl, SYSIN, 0x000000); /* 0x11D5 */
		SetSineWaveParams(o_ctrl, 0x05, XACTTEST);
	} else {
		RamWrite32A(o_ctrl, SXSIN, 0x000000); /* 0x10D5 */
		RamWrite32A(o_ctrl, SYSIN, ACT_CHECK_LEVEL); /* 0x11D5 */
		SetSineWaveParams(o_ctrl, 0x05, YACTTEST);
	}

	if (!direction) { /* AXIS X */
		RegWriteA(o_ctrl, WC_MES1ADD0,
			(uint8_t)SXINZ1); /* 0x0194 */
		/* 0x0195 */
		RegWriteA(o_ctrl, WC_MES1ADD1,
			(uint8_t)((SXINZ1 >> 8) & 0x0001));
	} else { /* AXIS Y */
		RegWriteA(o_ctrl, WC_MES1ADD0, (uint8_t)SYINZ1); /* 0x0194 */
		/* 0x0195 */
		RegWriteA(o_ctrl, WC_MES1ADD1,
			(uint8_t)((SYINZ1 >> 8) & 0x0001));
	}

	RegWriteA(o_ctrl, WC_MESSINMODE, 0x00); /* 0x0191 0 cross */
	RegWriteA(o_ctrl, WC_MESLOOP1, 0x00); /* 0x0193 */
	RegWriteA(o_ctrl, WC_MESLOOP0, 0x02); /* 0x0192 2 Times Measure */
	/* 0x10AE 1/CmMesLoop[15:0]/2 */
	RamWrite32A(o_ctrl, MSMEAN, 0x3F000000);
	RegWriteA(o_ctrl, WC_MESABS, 0x00); /* 0x0198 none ABS */

	BusyWait(o_ctrl, WC_MESMODE, 0x02); /* 0x0190 Sine wave Measure */

	RamAccessFixedMode(o_ctrl, ON); /* Fixed mode */
	RamReadA(o_ctrl, MSPP1AV, &MsppVal);
	RamAccessFixedMode(o_ctrl, OFF); /* Float mode */

	if (!direction) { /* AXIS X */
		SetSineWaveParams(o_ctrl, 0x00, XACTTEST); /* STOP */
	} else {
		SetSineWaveParams(o_ctrl, 0x00, YACTTEST); /* STOP */
	}

	return MsppVal;
}

/******************************************************************************
Function Name       : ClearGyro
Return Value        : NONE
Argument Value      : UsClrFil - Select filter to clear.
						If 0x0000, clears entire filter.
						UcClrMod -
						0x01: FRAM0 Clear,
						0x02: FRAM1,
						0x03: All RAM Clear
Explanation         : Gyro RAM clear function
History             : First edition 2013.01.09 Y.Shigeoka
******************************************************************************/
static void ClearGyro(struct msm_ois_ctrl_t *o_ctrl, uint16_t UsClearFilter,
	uint8_t UcClearMode)
{
	uint8_t UcRamClear;
	uint8_t count = 0;

	/*Select Filter to clear*/
	/* 0x018F FRAM Initialize Hbyte */
	RegWriteA(o_ctrl, WC_RAMDLYMOD1, (uint8_t) (UsClearFilter >> 8));
	/* 0x018E FRAM Initialize Lbyte */
	RegWriteA(o_ctrl, WC_RAMDLYMOD0, (uint8_t) UsClearFilter);

	/*Enable Clear*/
	RegWriteA(o_ctrl, WC_RAMINITON, UcClearMode); /* 0x0102 */

	/*Check RAM Clear complete*/
	do {
		RegReadA(o_ctrl, WC_RAMINITON, &UcRamClear);
		UcRamClear &= UcClearMode;
		if (count++ >= 100)
			break;
	} while (UcRamClear != 0x00);
}

/******************************************************************************
Function Name : RunGyroAcceptance
Return Value  : Result
Argument Value  : NONE
Explanation     : Gyro Examination of Acceptance
History : First edition  2014.02.13 T.Tokoro
******************************************************************************/
static uint8_t RunGyroAcceptance(struct msm_ois_ctrl_t *o_ctrl)
{
	uint8_t   UcResult, UcCount, UcXLowCount;
	uint8_t   UcYLowCount, UcXHighCount, UcYHighCount;
	uint16_t  UsGxoVal[10], UsGyoVal[10], UsDiff;

	UcResult = EXE_END;
	UcXLowCount = UcYLowCount = UcXHighCount = UcYHighCount = 0;

	MeasureFilter(o_ctrl, THROUGH);

	for (UcCount = 0; UcCount < 10; UcCount++) {
		/* X */
		RegWriteA(o_ctrl, WC_MES1ADD0, 0x00); /* 0x0194 */
		RegWriteA(o_ctrl, WC_MES1ADD1, 0x00); /* 0x0195 */
		/* Measure Filter RAM Clear */
		ClearGyro(o_ctrl, 0x1000, CLR_FRAM1);
		/* GYRMON1(0x1110) <- GXADZ(0x144A) */
		UsGxoVal[UcCount] = (uint16_t) GenMeas(o_ctrl, AD2Z, 0);

		/* Y */
		RegWriteA(o_ctrl, WC_MES1ADD0, 0x00); /* 0x0194 */
		RegWriteA(o_ctrl, WC_MES1ADD1, 0x00); /* 0x0195 */
		ClearGyro(o_ctrl, 0x1000, CLR_FRAM1);

		/* Measure Filter RAM Clear */
		/* GYRMON2(0x1111) <- GYADZ(0x14CA) */
		UsGyoVal[UcCount] = (uint16_t) GenMeas(o_ctrl, AD3Z, 0);

		if (UcCount > 0) {
			if ((int16_t)UsGxoVal[0] > (int16_t)UsGxoVal[UcCount]) {
				UsDiff = (uint16_t)((int16_t)UsGxoVal[0] -
					(int16_t)UsGxoVal[UcCount]);
			} else {
				UsDiff = (uint16_t)((int16_t)UsGxoVal[UcCount] -
					(int16_t)UsGxoVal[0]);
			}

			if (UsDiff > GEA_DIF_HIG) {
				/* UcResult = UcRst | EXE_GXABOVE; */
				UcXHighCount++;
			}
			if (UsDiff < GEA_DIF_LOW) {
				/* UcResult = UcRst | EXE_GXBELOW; */
				UcXLowCount++;
			}

			if ((int16_t)UsGyoVal[0] > (int16_t)UsGyoVal[UcCount]) {
				UsDiff = (uint16_t)((int16_t)UsGyoVal[0] -
					(int16_t)UsGyoVal[UcCount]);
			} else {
				UsDiff = (uint16_t)((int16_t)UsGyoVal[UcCount] -
					(int16_t)UsGyoVal[0]);
			}

			if (UsDiff > GEA_DIF_HIG) {
				/* UcResult = UcRst | EXE_GYABOVE; */
				UcYHighCount++;
			}
			if (UsDiff < GEA_DIF_LOW) {
				/* UcResult = UcRst | EXE_GYBELOW; */
				UcYLowCount++;
			}
		}
	}

	if (UcXHighCount >= 1)
		UcResult = UcResult | EXE_GXABOVE;

	if (UcXLowCount > 8)
		UcResult = UcResult | EXE_GXBELOW;

	if (UcYHighCount >= 1)
		UcResult = UcResult | EXE_GYABOVE;

	if (UcYLowCount > 8)
		UcResult = UcResult | EXE_GYBELOW;

	return UcResult;
}

/******************************************************************************
Function Name  : GenMeas
Return Value   : A/D Convert Result
Argument Value : Measure Filter Input Signal Ram Address
Explanation    : General Measure Function
History        : First edition  2013.01.10 Y.Shigeoka
******************************************************************************/
static int16_t GenMeas(struct msm_ois_ctrl_t *o_ctrl, uint16_t UsRamAddr,
	uint8_t UcMeasMode)
{
	int16_t SsMesRlt;

	RegWriteA(o_ctrl, WC_MES1ADD0, (uint8_t)UsRamAddr); /* 0x0194 */
	RegWriteA(o_ctrl, WC_MES1ADD1,
		(uint8_t)((UsRamAddr >> 8) & 0x0001)); /* 0x0195 */
	RamWrite32A(o_ctrl, MSABS1AV, 0x00000000);  /* 0x1041 Clear */

	if (!UcMeasMode) {
		RegWriteA(o_ctrl, WC_MESLOOP1, 0x04); /* 0x0193 */
		RegWriteA(o_ctrl, WC_MESLOOP0, 0x00); /* 0x0192 1024x Measure */
		/* 0x1230 1/CmMesLoop[15:0] */
		RamWrite32A(o_ctrl, MSMEAN, 0x3A7FFFF7);
	} else {
		RegWriteA(o_ctrl, WC_MESLOOP1, 0x00); /* 0x0193 */
		/* 0x0192 1 Times Measure */
		RegWriteA(o_ctrl, WC_MESLOOP0, 0x01);
		/* 0x1230 1/CmMesLoop[15:0] */
		RamWrite32A(o_ctrl, MSMEAN, 0x3F800000);
	}

	RegWriteA(o_ctrl, WC_MESABS, 0x00); /* 0x0198 none ABS */
	BusyWait(o_ctrl, WC_MESMODE, 0x01); /* 0x0190 normal Measure */

	RamAccessFixedMode(o_ctrl, ON); /* Fix mode */

	RamReadA(o_ctrl, MSABS1AV, (uint16_t *)&SsMesRlt); /* 0x1041 */

	RamAccessFixedMode(o_ctrl, OFF); /* Float mode */
	return SsMesRlt;
}

static void RamWrite32A(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint32_t data) {

	uint8_t ram_data[4];

	ram_data[0] = (data >> 24) & 0xff;
	ram_data[1] = (data >> 16) & 0xff;
	ram_data[2] = (data >> 8) & 0xff;
	ram_data[3] = (data) & 0xff;

	o_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
		&o_ctrl->i2c_client, addr, &(ram_data[0]), sizeof(ram_data));
}

static void RegReadA(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint8_t *reg_data) {
	o_ctrl->i2c_client.i2c_func_tbl->i2c_read_seq(
		&o_ctrl->i2c_client,
		addr,
		reg_data,
		MSM_CAMERA_I2C_BYTE_DATA);
}

static void RegWriteA(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint32_t data) {

	uint8_t reg_data;

	reg_data = (data) & 0xff;

	o_ctrl->i2c_client.i2c_func_tbl->i2c_write_seq(
		&o_ctrl->i2c_client, addr, &reg_data, sizeof(reg_data));
}

static void RamReadA(struct msm_ois_ctrl_t *o_ctrl, uint32_t addr,
	uint16_t *reg_data) {
	o_ctrl->i2c_client.i2c_func_tbl->i2c_read(
		&o_ctrl->i2c_client,
		addr,
		reg_data,
		MSM_CAMERA_I2C_WORD_DATA);
}
/******************************************************************************
Function Name  : RamAccessFixedMode
Return Value   : NONE
Argument Value : 0:OFF  1:ON
Explanation    : Ram Access Fix Mode setting function
History        : First edition  2013.05.21 Y.Shigeoka
******************************************************************************/
static void RamAccessFixedMode(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t UcAccessMode)
{
	switch (UcAccessMode) {
	case OFF:
		/* 0x018C GRAM Access Float32bit */
		RegWriteA(o_ctrl, WC_RAMACCMOD, 0x00);
		break;
	case ON:
		/* 0x018C GRAM Access Fixed32bit */
		RegWriteA(o_ctrl, WC_RAMACCMOD, 0x31);
		break;
	}
}

/******************************************************************************
Function Name : BusyWait
Return Value   : NONE
Argument Value : Trigger Register Address, Trigger Register Data
Explanation   : Busy Wait Function
History       : First edition  2013.01.09 Y.Shigeoka
******************************************************************************/
static void BusyWait(struct msm_ois_ctrl_t *o_ctrl, uint16_t TargetAddr,
	uint8_t TargetData)
{
	uint8_t FlagVal;

	/* Trigger Register Setting */
	RegWriteA(o_ctrl, TargetAddr, TargetData);

	FlagVal = 1;

	while (FlagVal) {
		RegReadA(o_ctrl, TargetAddr, &FlagVal);
		FlagVal &= (TargetData & 0x0F);
		/* Dead Lock check (response check) */
		if (CommandReadCheck(o_ctrl) != 0)
			break;
	};
}

/******************************************************************************
Function Name  : CommandReadCheck
Return Value   : 1 : ERROR
Argument Value : NONE
Explanation    : Check Cver function
******************************************************************************/
static uint8_t CommandReadCheck(struct msm_ois_ctrl_t *o_ctrl)
{
	uint8_t UcTestRD;
	uint8_t UcCount;

	for (UcCount = 0; UcCount < READ_COUNT_NUM; UcCount++) {
		RegReadA(o_ctrl, TESTRD, &UcTestRD); /* 0x027F */
		if (UcTestRD == 0xAC)
			return 0;
	}

	return 1;
}

/******************************************************************************
Function Name       : SetSinWavePara
Return Value        : NONE
Argument Value      : NONE
Explanation         : Sine wave Test Function
History             : First edition 2013.01.15 Y.Shigeoka
******************************************************************************/

/********* Parameter Setting *********/
/* Servo Sampling Clock =       23.4375kHz */
/* Freq = CmSinFreq*Fs/65536/16 */
/* 05 00 XX MM XX:Freq MM:Sin or Circle */

const unsigned short CucFreqVal[17] = {
	0xFFFF, /*  0:  Stop */
	0x002C, /*  1: 0.983477Hz */
	0x0059, /*  2: 1.989305Hz */
	0x0086, /*  3: 2.995133Hz */
	0x00B2, /*  4: 3.97861Hz */
	0x00DF, /*  5: 4.984438Hz */
	0x010C, /*  6: 5.990267Hz */
	0x0139, /*  7: 6.996095Hz */
	0x0165, /*  8: 7.979572Hz */
	0x0192, /*  9: 8.9854Hz */
	0x01BF, /*  A: 9.991229Hz */
	0x01EC, /*  B: 10.99706Hz */
	0x0218, /*  C: 11.98053Hz */
	0x0245, /*  D: 12.98636Hz */
	0x0272, /*  E: 13.99219Hz */
	0x029F, /*  F: 14.99802Hz */
	0x02CB  /* 10: 15.9815Hz */
};
/* if sine or circle movement uses a LPF, this define has to be enabled */
#define USE_SINLPF

/* sxsin(0x10D5), sysin(0x11D5) */

static void SetSineWaveParams(struct msm_ois_ctrl_t *o_ctrl,
	uint8_t UcTableVal, uint8_t UcMethodVal)
{
	uint16_t UsFreqDat;
	uint8_t UcEqSwX, UcEqSwY;

	if (UcTableVal > 0x10)
		UcTableVal = 0x10; /* Limit */

	UsFreqDat = CucFreqVal[UcTableVal];

	if (UcMethodVal == SINEWAVE) {
		RegWriteA(o_ctrl, WC_SINPHSX, 0x00); /* 0x0183 */
		RegWriteA(o_ctrl, WC_SINPHSY, 0x00); /* 0x0184 */
	} else if (UcMethodVal == CIRCWAVE) {
		RegWriteA(o_ctrl, WC_SINPHSX, 0x00); /* 0x0183 */
		RegWriteA(o_ctrl, WC_SINPHSY, 0x20); /* 0x0184 */
	} else {
		RegWriteA(o_ctrl, WC_SINPHSX, 0x00); /* 0x0183 */
		RegWriteA(o_ctrl, WC_SINPHSY, 0x00); /* 0x0184 */
	}

#ifdef USE_SINLPF
	if ((UcMethodVal == CIRCWAVE) || (UcMethodVal == SINEWAVE))
		MeasureFilter(o_ctrl, NOISE); /* LPF */

#endif

	if (UsFreqDat == 0xFFFF) { /* Sine */
		RegReadA(o_ctrl, WH_EQSWX, &UcEqSwX); /* 0x0170 */
		RegReadA(o_ctrl, WH_EQSWY, &UcEqSwY); /* 0x0171 */
		UcEqSwX &= ~EQSINSW;
		UcEqSwY &= ~EQSINSW;
		RegWriteA(o_ctrl, WH_EQSWX, UcEqSwX); /* 0x0170 */
		RegWriteA(o_ctrl, WH_EQSWY, UcEqSwY); /* 0x0171 */

#ifdef USE_SINLPF
		if ((UcMethodVal == CIRCWAVE)
			|| (UcMethodVal == SINEWAVE)
			|| (UcMethodVal == XACTTEST)
			|| (UcMethodVal == YACTTEST)) {
			/* 0x0105 Data pass off */
			RegWriteA(o_ctrl, WC_DPON, 0x00);

			/* 0x01B8 output initial */
			RegWriteA(o_ctrl, WC_DPO1ADD0, 0x00);

			/* 0x01B9 output initial */
			RegWriteA(o_ctrl, WC_DPO1ADD1, 0x00);

			/* 0x01BA output initial */
			RegWriteA(o_ctrl, WC_DPO2ADD0, 0x00);

			/* 0x01BB output initial */
			RegWriteA(o_ctrl, WC_DPO2ADD1, 0x00);

			/* 0x01B0 input initial */
			RegWriteA(o_ctrl, WC_DPI1ADD0, 0x00);

			/* 0x01B1 input initial */
			RegWriteA(o_ctrl, WC_DPI1ADD1, 0x00);

			/* 0x01B2 input initial */
			RegWriteA(o_ctrl, WC_DPI2ADD0, 0x00);

			/* 0x01B3 input initial */
			RegWriteA(o_ctrl, WC_DPI2ADD1, 0x00);

			/* TODO: Is this needed?? */
			/* Ram Access */
			/* RamAccessFixedMode(o_ctrl, ON ); */

			/* Fix mode */
			/* 0x1461 set optical value */
			/* RamWriteA(o_ctrl, SXOFFZ1, OPTCEN_X ); */
			/* 0x14E1 set optical value */
			/* RamWriteA(o_ctrl, SYOFFZ1, OPTCEN_Y ); */

			/* Ram Access */
			/* RamAccessFixedMode(OFF); */

			/* Float mode */
			RegWriteA(o_ctrl, WC_MES1ADD0,  0x00); /* 0x0194 */
			RegWriteA(o_ctrl, WC_MES1ADD1,  0x00); /* 0x0195 */
			RegWriteA(o_ctrl, WC_MES2ADD0,  0x00); /* 0x0196 */
			RegWriteA(o_ctrl, WC_MES2ADD1,  0x00); /* 0x0197 */
		}
#endif /* USE_SINLPF */

	RegWriteA(o_ctrl, WC_SINON, 0x00); /* 0x0180 Sine wave  */

	} else {
		RegReadA(o_ctrl, WH_EQSWX, &UcEqSwX); /* 0x0170 */
		RegReadA(o_ctrl, WH_EQSWY, &UcEqSwY); /* 0x0171 */
		if ((UcMethodVal == CIRCWAVE) || (UcMethodVal == SINEWAVE)) {
#ifdef USE_SINLPF
			/* 0x01B0 input Meas-Fil */
			RegWriteA(o_ctrl, WC_DPI1ADD0, (uint8_t) MES1BZ2);
			/* 0x01B1 input Meas-Fil */
			RegWriteA(o_ctrl, WC_DPI1ADD1,
				(uint8_t)((MES1BZ2 >> 8) & 0x0001));
			RegWriteA(o_ctrl, WC_DPI2ADD0,
				(uint8_t)MES2BZ2); /* 0x01B2 input Meas-Fil */
			/* 0x01B3 input Meas-Fil*/
			RegWriteA(o_ctrl, WC_DPI2ADD1,
				(uint8_t)((MES2BZ2 >> 8) & 0x0001));
			RegWriteA(o_ctrl, WC_DPO1ADD0,
				(uint8_t)SXOFFZ1); /* 0x01B8 output SXOFFZ1 */
			/* 0x01B9 output SXOFFZ1 */
			RegWriteA(o_ctrl, WC_DPO1ADD1,
				(uint8_t)((SXOFFZ1 >> 8) & 0x0001));
			RegWriteA(o_ctrl, WC_DPO2ADD0,
				(uint8_t)SYOFFZ1); /* 0x01BA output SYOFFZ1 */
			/* 0x01BA output SYOFFZ1 */
			RegWriteA(o_ctrl, WC_DPO2ADD1,
				(uint8_t)((SYOFFZ1 >> 8) & 0x0001));
			/* 0x0194 */
			RegWriteA(o_ctrl, WC_MES1ADD0, (uint8_t)SINXZ);
			/* 0x0195 */
			RegWriteA(o_ctrl, WC_MES1ADD1,
				(uint8_t)((SINXZ >> 8) & 0x0001));
			RegWriteA(o_ctrl, WC_MES2ADD0,
				(uint8_t)SINYZ); /* 0x0196 */
			/* 0x0197 */
			RegWriteA(o_ctrl, WC_MES2ADD1,
				(uint8_t)((SINYZ >> 8) & 0x0001));
			/* 0x0105 Data pass[1:0] on */
			RegWriteA(o_ctrl, WC_DPON, 0x03);

			UcEqSwX &= ~EQSINSW;
			UcEqSwY &= ~EQSINSW;
#else
			UcEqSwX |= 0x08;
			UcEqSwY |= 0x08;
#endif /* USE_SINLPF */
		} else if ((UcMethodVal == XACTTEST) ||
			(UcMethodVal == YACTTEST)) {
			RegWriteA(o_ctrl, WC_DPI2ADD0,
				(uint8_t)MES2BZ2); /* 0x01B2 input Meas-Fil */
			/*0x01B3 input Meas-Fil*/
			RegWriteA(o_ctrl, WC_DPI2ADD1,
				(uint8_t)((MES2BZ2 >> 8) & 0x0001));
			if (UcMethodVal == XACTTEST) {
				/* 0x01BA output SXOFFZ1 */
				RegWriteA(o_ctrl, WC_DPO2ADD0,
					(uint8_t)SXOFFZ1);
				/* 0x01BB output SXOFFZ1 */
				RegWriteA(o_ctrl, WC_DPO2ADD1,
					(uint8_t)((SXOFFZ1 >> 8) & 0x0001));
				/* 0x0196 */
				RegWriteA(o_ctrl, WC_MES2ADD0, (uint8_t)SINXZ);
				/* 0x0197 */
				RegWriteA(o_ctrl, WC_MES2ADD1,
					(uint8_t)((SINXZ >> 8) & 0x0001));
			} else {
				/* 0x01BA output SYOFFZ1 */
				RegWriteA(o_ctrl, WC_DPO2ADD0,
					(uint8_t)SYOFFZ1);
				/* 0x01BB output SYOFFZ1 */
				RegWriteA(o_ctrl, WC_DPO2ADD1,
					(uint8_t)((SYOFFZ1 >> 8) & 0x0001));
				/* 0x0196 */
				RegWriteA(o_ctrl, WC_MES2ADD0, (uint8_t)SINYZ);
				/* 0x0197 */
				RegWriteA(o_ctrl, WC_MES2ADD1,
					(uint8_t)((SINYZ >> 8) & 0x0001));
			}

		RegWriteA(o_ctrl, WC_DPON, 0x02); /* 0x0105  Data pass[1] on */
		UcEqSwX &= ~EQSINSW;
		UcEqSwY &= ~EQSINSW;
		} else {
			if (UcMethodVal == XHALWAVE)
				UcEqSwX = 0x22; /* SW[5] */
			else
				UcEqSwY = 0x22; /* SW[5] */
		}
		/* 0x0181 Freq L */
		RegWriteA(o_ctrl, WC_SINFRQ0, (uint8_t)UsFreqDat);
		/* 0x0182 Freq H */
		RegWriteA(o_ctrl, WC_SINFRQ1, (uint8_t)(UsFreqDat >> 8));
		/*0x0191 Sine 0 cross*/
		RegWriteA(o_ctrl, WC_MESSINMODE, 0x00);
		RegWriteA(o_ctrl, WH_EQSWX, UcEqSwX); /* 0x0170 */
		RegWriteA(o_ctrl, WH_EQSWY, UcEqSwY); /* 0x0171 */
		RegWriteA(o_ctrl, WC_SINON, 0x01); /* 0x0180 Sine wave */
	}
}

static int32_t msm_ois_enable(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *enable_ois_settings = NULL;
	int32_t rc = -EFAULT;
	CDBG("Enter\n");

	if (set_info->ois_params.enable_ois_setting_size) {
		enable_ois_settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.enable_ois_setting_size),
			GFP_KERNEL);
		if (enable_ois_settings == NULL) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(enable_ois_settings,
			(void *)set_info->ois_params.enable_ois_settings,
			set_info->ois_params.enable_ois_setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(enable_ois_settings);
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.enable_ois_setting_size,
			enable_ois_settings);
		kfree(enable_ois_settings);
		if (rc < 0) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}

static int32_t msm_ois_disable(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *disable_ois_settings = NULL;
	int32_t rc = -EFAULT;
	CDBG("Enter\n");

	if (set_info->ois_params.disable_ois_setting_size) {
		disable_ois_settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.disable_ois_setting_size),
			GFP_KERNEL);
		if (disable_ois_settings == NULL) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(disable_ois_settings,
			(void *)set_info->ois_params.disable_ois_settings,
			set_info->ois_params.disable_ois_setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(disable_ois_settings);
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.disable_ois_setting_size,
			disable_ois_settings);
		kfree(disable_ois_settings);
		if (rc < 0) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}

static int32_t msm_ois_set_still_mode(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *still_mode_ois_settings = NULL;
	int32_t rc = -EFAULT;
	CDBG("Enter\n");

	if (set_info->ois_params.still_mode_ois_setting_size) {
		still_mode_ois_settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.still_mode_ois_setting_size),
			GFP_KERNEL);
		if (still_mode_ois_settings == NULL) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(still_mode_ois_settings,
			(void *)set_info->ois_params.still_mode_ois_settings,
			set_info->ois_params.still_mode_ois_setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(still_mode_ois_settings);
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.still_mode_ois_setting_size,
			still_mode_ois_settings);
		kfree(still_mode_ois_settings);
		if (rc < 0) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}

static int32_t msm_ois_set_movie_mode(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *movie_mode_ois_settings = NULL;
	int32_t rc = -EFAULT;
	CDBG("Enter\n");

	if (set_info->ois_params.movie_mode_ois_setting_size) {
		movie_mode_ois_settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.movie_mode_ois_setting_size),
			GFP_KERNEL);
		if (movie_mode_ois_settings == NULL) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(movie_mode_ois_settings,
			(void *)set_info->ois_params.movie_mode_ois_settings,
			set_info->ois_params.movie_mode_ois_setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(movie_mode_ois_settings);
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.movie_mode_ois_setting_size,
			movie_mode_ois_settings);
		kfree(movie_mode_ois_settings);
		if (rc < 0) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}

static int32_t msm_ois_set_pantilt(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info)
{
	struct reg_settings_ois_t *pantilt_on_ois_settings = NULL;
	int32_t rc = -EFAULT;
	CDBG("Enter\n");

	if (set_info->ois_params.pantilt_on_ois_setting_size) {
		pantilt_on_ois_settings = kmalloc(
			sizeof(struct reg_settings_ois_t) *
			(set_info->ois_params.pantilt_on_ois_setting_size),
			GFP_KERNEL);
		if (pantilt_on_ois_settings == NULL) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error allocating memory\n");
			return -EFAULT;
		}
		if (copy_from_user(pantilt_on_ois_settings,
			(void *)set_info->ois_params.pantilt_on_ois_settings,
			set_info->ois_params.pantilt_on_ois_setting_size *
			sizeof(struct reg_settings_ois_t))) {
			kfree(pantilt_on_ois_settings);
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error copying\n");
			return -EFAULT;
		}

		rc = msm_ois_write_settings(o_ctrl,
			set_info->ois_params.pantilt_on_ois_setting_size,
			pantilt_on_ois_settings);
		kfree(pantilt_on_ois_settings);
		if (rc < 0) {
			kfree(o_ctrl->i2c_reg_tbl);
			pr_err("Error\n");
			return -EFAULT;
		}
	}

	CDBG("Exit\n");

	return rc;
}

static int32_t msm_ois_set_centering_on_off(struct msm_ois_ctrl_t *o_ctrl,
	struct msm_ois_set_info_t *set_info, uint8_t enable_centering_ois)
{
	struct reg_settings_ois_t *centering_on_ois_settings = NULL;
	struct reg_settings_ois_t *centering_off_ois_settings = NULL;
	int32_t rc = -EFAULT;
	CDBG("Enter\n");

	if (enable_centering_ois) {
		if (set_info->ois_params.centering_on_ois_setting_size) {
			centering_on_ois_settings =
				kmalloc(sizeof(struct reg_settings_ois_t) *
				(set_info->ois_params.
				centering_on_ois_setting_size),
				GFP_KERNEL);
			if (centering_on_ois_settings == NULL) {
				kfree(o_ctrl->i2c_reg_tbl);
				pr_err("Error allocating memory\n");
				return -EFAULT;
			}
			if (copy_from_user(centering_on_ois_settings,
				(void *)set_info->ois_params.
				centering_on_ois_settings,
				set_info->ois_params.
				centering_on_ois_setting_size *
				sizeof(struct reg_settings_ois_t))) {
				kfree(centering_on_ois_settings);
				kfree(o_ctrl->i2c_reg_tbl);
				pr_err("Error copying\n");
				return -EFAULT;
			}

			rc = msm_ois_write_settings(o_ctrl,
				set_info->ois_params.
				centering_on_ois_setting_size,
				centering_on_ois_settings);
			kfree(centering_on_ois_settings);
			if (rc < 0) {
				kfree(o_ctrl->i2c_reg_tbl);
				pr_err("Error\n");
				return -EFAULT;
			}
		}
	} else {
		if (set_info->ois_params.centering_off_ois_setting_size) {
			centering_off_ois_settings =
				kmalloc(sizeof(struct reg_settings_ois_t) *
				(set_info->ois_params.
				centering_off_ois_setting_size),
				GFP_KERNEL);
			if (centering_off_ois_settings == NULL) {
				kfree(o_ctrl->i2c_reg_tbl);
				pr_err("Error allocating memory\n");
				return -EFAULT;
			}
			if (copy_from_user(centering_off_ois_settings,
				(void *)set_info->ois_params.
				centering_off_ois_settings,
				set_info->ois_params.
				centering_off_ois_setting_size *
				sizeof(struct reg_settings_ois_t))) {
				kfree(centering_off_ois_settings);
				kfree(o_ctrl->i2c_reg_tbl);
				pr_err("Error copying\n");
				return -EFAULT;
			}

			rc = msm_ois_write_settings(o_ctrl,
				set_info->ois_params.
				centering_off_ois_setting_size,
				centering_off_ois_settings);
			kfree(centering_off_ois_settings);
			if (rc < 0) {
				kfree(o_ctrl->i2c_reg_tbl);
				pr_err("Error\n");
				return -EFAULT;
			}
		}

	}

	CDBG("Exit\n");

	return rc;
}

static int32_t msm_ois_vreg_control(struct msm_ois_ctrl_t *o_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_ois_vreg *vreg_cfg;

	vreg_cfg = &o_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_OIS_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		rc = msm_camera_config_single_vreg(&(o_ctrl->pdev->dev),
			&vreg_cfg->cam_vreg[i],
			(struct regulator **)&vreg_cfg->data[i],
			config);
	}
	return rc;
}

static int32_t msm_ois_power_down(struct msm_ois_ctrl_t *o_ctrl)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	if (o_ctrl->ois_state != OIS_POWER_DOWN) {
		if (o_ctrl->ois_enable) {
			rc = gpio_direction_output(o_ctrl->ois_pwd, 0);
			if (!rc)
				gpio_free(o_ctrl->ois_pwd);
		}

		rc = msm_ois_vreg_control(o_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}



		kfree(o_ctrl->i2c_reg_tbl);
		o_ctrl->i2c_reg_tbl = NULL;
		o_ctrl->i2c_tbl_index = 0;
		o_ctrl->ois_state = OIS_POWER_DOWN;
	}
	CDBG("Exit\n");
	return rc;
}


static int msm_ois_init(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0, i = 0;
	CDBG("Enter\n");

	for (i = 0; i < ARRAY_SIZE(oiss); i++)
		o_ctrl->func_tbl = &oiss[i]->func_tbl;

	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_config(struct msm_ois_ctrl_t *o_ctrl,
	void __user *argp)
{
	struct msm_ois_cfg_data *cdata =
		(struct msm_ois_cfg_data *)argp;
	int32_t rc = 0;
	mutex_lock(o_ctrl->ois_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_OIS_INIT:
		rc = msm_ois_init(o_ctrl);
		if (rc < 0)
			pr_err("msm_ois_init failed %d\n", rc);
		break;
	case CFG_OIS_GEA:
		rc = msm_ois_gea(o_ctrl, &cdata->gea);
		if (rc < 0)
			pr_err("msm_ois_gea failed %d\n", rc);
		break;
	case CFG_OIS_HEA:
		rc = msm_ois_hea(o_ctrl, &cdata->hea);
		if (rc < 0)
			pr_err("msm_ois_hea failed %d\n", rc);
		break;
	case CFG_GET_OIS_INFO:
		cdata->is_ois_supported = 1;
		break;
	case CFG_OIS_POWERDOWN:
		rc = msm_ois_power_down(o_ctrl);
		if (rc < 0)
			pr_err("msm_ois_power_down failed %d\n", rc);
		break;
	case CFG_OIS_INI_SET:
		rc = o_ctrl->func_tbl->ini_set_ois(o_ctrl,
			&cdata->cfg.set_info);
		if (rc < 0)
			pr_err("init setup ois failed %d\n", rc);
		break;
	case CFG_OIS_ENABLE:
		rc = o_ctrl->func_tbl->enable_ois(o_ctrl,
			&cdata->cfg.set_info);
		if (rc < 0)
			pr_err("ois enable failed %d\n", rc);
		break;
	case CFG_OIS_DISABLE:
		rc = o_ctrl->func_tbl->disable_ois(o_ctrl,
			&cdata->cfg.set_info);
		if (rc < 0)
			pr_err("ois disable failed %d\n", rc);
		break;
	case CFG_OIS_SET_MOVIE_MODE:
		rc = msm_ois_set_movie_mode(o_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("ois set movie mode failed %d\n", rc);
		break;
	case CFG_OIS_SET_STILL_MODE:
		rc = msm_ois_set_still_mode(o_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("ois set still mode failed %d\n", rc);
		break;
	case CFG_OIS_SET_CENTERING_ON:
		rc = msm_ois_set_centering_on_off(o_ctrl, &cdata->cfg.set_info,
			cdata->cfg.enable_centering_ois);
		if (rc < 0)
			pr_err("ois set centering failed %d\n", rc);
		break;
	case CFG_OIS_SET_PANTILT_ON:
		rc = msm_ois_set_pantilt(o_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("ois set pantilt on failed %d\n", rc);
		break;
	case CFG_OIS_POWERUP:
		rc = msm_ois_power_up(o_ctrl);
		if (rc < 0)
			pr_err("Failed ois power up%d\n", rc);
		break;
	case CFG_OIS_I2C_WRITE_SEQ_TABLE: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.settings,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
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
		rc = o_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_seq_table(&o_ctrl->i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}
	default:
		break;
	}
	mutex_unlock(o_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_get_subdev_id(struct msm_ois_ctrl_t *o_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = o_ctrl->pdev->id;
	else
		*subdev_id = o_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq = msm_camera_cci_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq = msm_camera_qup_i2c_write_seq,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_ois_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_ois_ctrl_t *o_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!o_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = o_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&o_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	kfree(o_ctrl->i2c_reg_tbl);
	o_ctrl->i2c_reg_tbl = NULL;

	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_ois_internal_ops = {
	.close = msm_ois_close,
};

static long msm_ois_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_ois_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d o_ctrl %p argp %p\n", __func__, __LINE__, o_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_ois_get_subdev_id(o_ctrl, argp);
	case VIDIOC_MSM_OIS_CFG:
		return msm_ois_config(o_ctrl, argp);
	case MSM_SD_SHUTDOWN:
		msm_ois_close(sd, NULL);
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

static int32_t msm_ois_power_up(struct msm_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	CDBG("%s called\n", __func__);

	CDBG("vcm info: %d %d\n", o_ctrl->ois_pwd,
		o_ctrl->ois_enable);

	rc = msm_ois_vreg_control(o_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	if (o_ctrl->ois_enable) {
		rc = gpio_request(o_ctrl->ois_pwd, "msm_ois");
		if (!rc) {
			CDBG("Enable VCM PWD\n");
			gpio_direction_output(o_ctrl->ois_pwd, 1);
		}
	}
	o_ctrl->ois_state = OIS_POWER_UP;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_ois_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_ois_ctrl_t *o_ctrl = v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	mutex_lock(o_ctrl->ois_mutex);
	if (on)
		rc = msm_ois_power_up(o_ctrl);
	else
		rc = msm_ois_power_down(o_ctrl);
	mutex_unlock(o_ctrl->ois_mutex);
	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_ois_subdev_core_ops = {
	.ioctl = msm_ois_subdev_ioctl,
	.s_power = msm_ois_power,
};

static struct v4l2_subdev_ops msm_ois_subdev_ops = {
	.core = &msm_ois_subdev_core_ops,
};

static const struct i2c_device_id msm_ois_i2c_id[] = {
	{"qcom,ois", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_ois_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_ois_ctrl_t *ois_ctrl_t = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_ois_i2c_probe: client is null\n");
		rc = -EINVAL;
		goto probe_failure;
	}

	ois_ctrl_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!ois_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	CDBG("client = 0x%p\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&ois_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", ois_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	ois_ctrl_t->i2c_driver = &msm_ois_i2c_driver;
	ois_ctrl_t->i2c_client.client = client;
	/* Set device type as I2C */
	ois_ctrl_t->ois_device_type = MSM_CAMERA_I2C_DEVICE;
	ois_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	ois_ctrl_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	ois_ctrl_t->ois_mutex = &msm_ois_mutex;

	/* Assign name for sub device */
	snprintf(ois_ctrl_t->msm_sd.sd.name, sizeof(ois_ctrl_t->msm_sd.sd.name),
		"%s", ois_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&ois_ctrl_t->msm_sd.sd,
		ois_ctrl_t->i2c_client.client,
		ois_ctrl_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&ois_ctrl_t->msm_sd.sd, ois_ctrl_t);
	ois_ctrl_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	ois_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&ois_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	ois_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	ois_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	ois_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&ois_ctrl_t->msm_sd);
	ois_ctrl_t->ois_state = OIS_POWER_DOWN;
	pr_info("msm_ois_i2c_probe: succeeded\n");
	CDBG("Exit\n");

probe_failure:
	return rc;
}

static int32_t msm_ois_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_ois_ctrl_t *msm_ois_t = NULL;
	struct msm_ois_vreg *vreg_cfg;
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_ois_t = kzalloc(sizeof(struct msm_ois_ctrl_t),
		GFP_KERNEL);
	if (!msm_ois_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_ois_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_ois_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_ois_t->cci_master, rc);
	if (rc < 0) {
		kfree(msm_ois_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_ois_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_ois_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}

	msm_ois_t->ois_v4l2_subdev_ops = &msm_ois_subdev_ops;
	msm_ois_t->ois_mutex = &msm_ois_mutex;

	/* Set platform device handle */
	msm_ois_t->pdev = pdev;
	/* Set device type as platform device */
	msm_ois_t->ois_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_ois_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_ois_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_ois_t->i2c_client.cci_client) {
		kfree(msm_ois_t->vreg_cfg.cam_vreg);
		kfree(msm_ois_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_ois_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = MASTER_MAX;
	v4l2_subdev_init(&msm_ois_t->msm_sd.sd,
		msm_ois_t->ois_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_ois_t->msm_sd.sd, msm_ois_t);
	msm_ois_t->msm_sd.sd.internal_ops = &msm_ois_internal_ops;
	msm_ois_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_ois_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_ois_t->msm_sd.sd.name), "msm_ois");
	media_entity_init(&msm_ois_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_ois_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_ois_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_OIS;
	msm_ois_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_ois_t->msm_sd);
	msm_ois_t->ois_state = OIS_POWER_DOWN;
	CDBG("Exit\n");
	return rc;
}

static const struct of_device_id msm_ois_i2c_dt_match[] = {
	{.compatible = "qcom,ois"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ois_i2c_dt_match);

static struct i2c_driver msm_ois_i2c_driver = {
	.id_table = msm_ois_i2c_id,
	.probe  = msm_ois_i2c_probe,
	.remove = __exit_p(msm_ois_i2c_remove),
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_i2c_dt_match,
	},
};

static const struct of_device_id msm_ois_dt_match[] = {
	{.compatible = "qcom,ois", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ois_dt_match);

static struct platform_driver msm_ois_platform_driver = {
	.driver = {
		.name = "qcom,ois",
		.owner = THIS_MODULE,
		.of_match_table = msm_ois_dt_match,
	},
};

static int __init msm_ois_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_probe(&msm_ois_platform_driver,
		msm_ois_platform_probe);
	if (!rc)
		return rc;
	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_ois_i2c_driver);
}

static struct msm_ois msm_ois_table = {
	.func_tbl = {
		.ini_set_ois = msm_ini_set_ois,
		.enable_ois = msm_ois_enable,
		.disable_ois = msm_ois_disable,
	},
};

module_init(msm_ois_init_module);
MODULE_DESCRIPTION("MSM OIS");
MODULE_LICENSE("GPL v2");
