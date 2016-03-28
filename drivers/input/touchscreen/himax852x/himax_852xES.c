/* Himax Android Driver Sample Code Ver 0.3 for HMX852xES chipset
*
* Copyright (C) 2014 Himax Corporation.
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
#define pr_fmt(fmt) "HXTP %s: " fmt, __func__
#include <linux/proc_fs.h>
#include "himax_852xES.h"

#define HIMAX_I2C_RETRY_TIMES 10

#if defined(HX_AUTO_UPDATE_FW)
#include "himax_fw.h"
#endif

#ifndef KEY_APP_SWITCH
#define KEY_APP_SWITCH	580
#endif

#ifdef HX_SMART_WAKEUP
#define HIMAX_GESTURE_PROC_FILE "gesture_open"
static struct proc_dir_entry *gesture_proc = NULL;
extern void qpnp_kernel_vib_enable(int value);
extern int pa12200001_ps_state(void);
#endif

static int HX_TOUCH_INFO_POINT_CNT   = 0;
static int HX_RX_NUM                 = 0;
static int HX_TX_NUM                 = 0;
static int HX_BT_NUM                 = 0;
static int HX_X_RES                  = 0;
static int HX_Y_RES                  = 0;
static int HX_MAX_PT                 = 0;
static bool HX_XY_REVERSE            = false;

/* FW version addr offset */
#define FW_CFG_VER_FLASH_ADDR	132

#ifdef HX_TP_SYS_DIAG
#ifdef HX_TP_SYS_2T2R
static int HX_RX_NUM_2 = 0;
static int HX_TX_NUM_2 = 0;
#endif
static int touch_monitor_stop_flag = 0;
static int touch_monitor_stop_limit = 5;
#endif

static uint8_t vk_press = 0x00;
static uint8_t AA_press = 0x00;
#ifdef HX_ESD_WORKAROUND
static uint8_t IC_STATUS_CHECK	= 0xAA;
#endif
static uint8_t EN_NoiseFilter = 0x00;
static uint8_t Last_EN_NoiseFilter = 0x00;
static int hx_point_num	= 0;
static int p_point_num	= 0xFFFF;
static int tpd_key	   	= 0x00;
static int tpd_key_old	= 0x00;

static bool	config_load	= false;
static struct himax_config *config_selected = NULL;

static int iref_number = 11;
static bool iref_found = false;

static u8 reset_active = false;

#ifdef CONFIG_TOUCHSCREEN_HIMAX_DEBUG
static struct kobject *android_touch_kobj = NULL;
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data);
#endif

#ifdef HX_ESD_WORKAROUND
/* 0:Running, 1:Stop, 2:I2C Fail */
static int himax_hand_shaking(void)
{
	int ret, result;
	uint8_t hw_reset_check[1];
	uint8_t hw_reset_check_2[1];
	uint8_t buf0[2];

	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));
	memset(hw_reset_check_2, 0x00, sizeof(hw_reset_check_2));

	buf0[0] = 0xF2;
	if (IC_STATUS_CHECK == 0xAA) {
		buf0[1] = 0xAA;
		IC_STATUS_CHECK = 0x55;
	} else {
		buf0[1] = 0x55;
		IC_STATUS_CHECK = 0xAA;
	}

	ret = i2c_himax_master_write(private_ts->client, buf0, 2,
		DEFAULT_RETRY_CNT);
	if (ret < 0) {
		pr_err("#1, write reg 0xf2 failed !\n");
		goto work_func_send_i2c_msg_fail;
	}
	msleep(50); 

	buf0[0] = 0xF2;
	buf0[1] = 0x00;
	ret = i2c_himax_master_write(private_ts->client, buf0, 2,
		DEFAULT_RETRY_CNT);
	if (ret < 0) {
		pr_err("#2, write reg 0xf2 failed !\n");
		goto work_func_send_i2c_msg_fail;
	}
	usleep(2000);

	ret = i2c_himax_read(private_ts->client, 0x90,
		hw_reset_check, 1, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		pr_err("#3, read reg 0x90 failed !\n");
		goto work_func_send_i2c_msg_fail;
	}

	if ((IC_STATUS_CHECK != hw_reset_check[0])) {
		usleep(2000);
		ret = i2c_himax_read(private_ts->client, 0x90,
			hw_reset_check_2, 1, DEFAULT_RETRY_CNT);
		if (ret < 0) {
			pr_err("#4, read reg 0x90 failed !\n");
			goto work_func_send_i2c_msg_fail;
		}

		if (hw_reset_check[0] == hw_reset_check_2[0])
			result = 1; 
		else
			result = 0;
	} else {
		result = 0; 
	}

	return result;

work_func_send_i2c_msg_fail:
	return 2;
}
#endif

static int himax_ManualMode(int enter)
{
	uint8_t cmd[2];
	cmd[0] = enter;
	i2c_himax_write(private_ts->client, 0x42 ,&cmd[0], 1, 3);
	return 0;
}

static int himax_FlashMode(int enter)
{
	uint8_t cmd[2];
	cmd[0] = enter;
	i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 1, 3);
	return 0;
}

static int himax_lock_flash(int enable)
{
	uint8_t cmd[5];

	if (i2c_himax_write(private_ts->client, 0xAA ,&cmd[0], 0, 3) < 0)
		return 0;

	/* lock sequence start */
	cmd[0] = 0x01; cmd[1] = 0x00; cmd[2] = 0x06;
	if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 3, 3) < 0)
		return 0;

	cmd[0] = 0x03; cmd[1] = 0x00; cmd[2] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x44 ,&cmd[0], 3, 3) < 0)
		return 0;

	if (enable != 0) {
		cmd[0] = 0x63;
		cmd[1] = 0x02;
		cmd[2] = 0x70;
		cmd[3] = 0x03;
	} else {
		cmd[0] = 0x63;
		cmd[1] = 0x02;
		cmd[2] = 0x30;
		cmd[3] = 0x00;
	}

	if (i2c_himax_write(private_ts->client, 0x45 ,&cmd[0], 4, 3) < 0)
		return 0;

	if (i2c_himax_write_command(private_ts->client, 0x4A, 3) < 0) {
		pr_err("i2c write error !\n");
		return 0;
	}
	msleep(50);

	i2c_himax_write(private_ts->client, 0xA9 ,&cmd[0], 0, 3);

	return 0;
	/* lock sequence stop */
}

static void himax_changeIref(int selected_iref)
{
	unsigned char temp_iref[16][2] = {
		{0x00,0x00},{0x00,0x00},{0x00,0x00},{0x00,0x00},
		{0x00,0x00},{0x00,0x00},{0x00,0x00},{0x00,0x00},
		{0x00,0x00},{0x00,0x00},{0x00,0x00},{0x00,0x00},
		{0x00,0x00},{0x00,0x00},{0x00,0x00},{0x00,0x00}
	}; 												
	uint8_t cmd[10];
	int i = 0;
	int j = 0;

	pr_debug("start to check iref, iref number = %d\n", selected_iref);

	if (i2c_himax_write(private_ts->client, 0xAA ,&cmd[0], 0, 3) < 0)
		return;	

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 2; j++) {
			if (selected_iref == 1)
				temp_iref[i][j] = E_IrefTable_1[i][j];
			else if (selected_iref == 2)
				temp_iref[i][j] = E_IrefTable_2[i][j];
			else if(selected_iref == 3)
				temp_iref[i][j] = E_IrefTable_3[i][j];
			else if(selected_iref == 4)
				temp_iref[i][j] = E_IrefTable_4[i][j];
			else if(selected_iref == 5)
				temp_iref[i][j] = E_IrefTable_5[i][j];
			else if(selected_iref == 6)
				temp_iref[i][j] = E_IrefTable_6[i][j];
			else if(selected_iref == 7)
				temp_iref[i][j] = E_IrefTable_7[i][j];
		}
	}

	if (!iref_found) {
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		cmd[2] = 0x0A;
		if( i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 3, 3) < 0)
			return;

		cmd[0] = 0x00;
		cmd[1] = 0x00;
		cmd[2] = 0x00;
		if (i2c_himax_write(private_ts->client, 0x44 ,&cmd[0], 3, 3) < 0)
			return;														

		if (i2c_himax_write(private_ts->client, 0x46 ,&cmd[0], 0, 3) < 0)
			return;

		if (i2c_himax_read(private_ts->client, 0x59, cmd, 4, 3) < 0)
			return;

		/* find iref group , default is iref 3 */
		for (i = 0; i < 16; i++){
			if ((cmd[0] == temp_iref[i][0]) &&
				(cmd[1] == temp_iref[i][1])) {
				iref_number = i;
				iref_found = true;
				break;
			}
		}

		if (!iref_found) {
			pr_err("Can't find iref number!\n");
			return;
		} else {
			pr_debug("iref_number=%d, cmd[0]=0x%x, cmd[1]=0x%x\n",
				iref_number, cmd[0], cmd[1]);
		}
	}
	usleep(5000);

	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x06;
	if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 3, 3) < 0)
		return;  

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x44 ,&cmd[0], 3, 3) < 0)
		return;

	cmd[0] = temp_iref[iref_number][0];
	cmd[1] = temp_iref[iref_number][1];
	cmd[2] = 0x17;
	cmd[3] = 0x28;

	if (i2c_himax_write(private_ts->client, 0x45 ,&cmd[0], 4, 3) < 0)
		return;

	if (i2c_himax_write(private_ts->client, 0x4A ,&cmd[0], 0, 3) < 0)
		return;

	/* Read SFR to check the result */
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x0A;
	if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 3, 3) < 0)
		return;

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(private_ts->client, 0x44 ,&cmd[0], 3, 3) < 0)
		return;

	if (i2c_himax_write(private_ts->client, 0x46 ,&cmd[0], 0, 3) < 0)
		return;

	if (i2c_himax_read(private_ts->client, 0x59, cmd, 4, 3) < 0)
		return;

	pr_debug("cmd[0]=%d, cmd[1]=%d, temp_iref_1=%d, temp_iref_2=%d\n",
		cmd[0], cmd[1],
		temp_iref[iref_number][0], temp_iref[iref_number][1]);

	if (cmd[0] != temp_iref[iref_number][0] ||
		cmd[1] != temp_iref[iref_number][1]) {
		pr_err("IREF Read Back is not match !\n");
		pr_err("Iref [0]=%d,[1]=%d\n", cmd[0], cmd[1]);
	} else{
		pr_debug("IREF Pass\n");
	}

	i2c_himax_write(private_ts->client, 0xA9 ,&cmd[0], 0, 3);
}

static uint8_t himax_calculateChecksum(bool change_iref)
{
	int iref_flag = 0;
	uint8_t cmd[10];

	memset(cmd, 0x00, sizeof(cmd));

	/* Sleep out */
	if (i2c_himax_write(private_ts->client, 0x81,
		&cmd[0], 0, DEFAULT_RETRY_CNT) < 0)
		return 0;
	msleep(120);

	while (true) {
		if (change_iref) {
			if (iref_flag == 0)
				himax_changeIref(2);		
			else if (iref_flag == 1)
				himax_changeIref(5);
			else if (iref_flag == 2)
				himax_changeIref(1);
			else
				goto CHECK_FAIL;
			iref_flag++;
		}

		cmd[0] = 0x00;
		cmd[1] = 0x04;
		cmd[2] = 0x0A;
		cmd[3] = 0x02;

		if (i2c_himax_write(private_ts->client, 0xED, &cmd[0], 4,
			DEFAULT_RETRY_CNT) < 0)
			return 0;

		/* Enable Flash */
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		cmd[2] = 0x02;
		    	
		if (i2c_himax_write(private_ts->client, 0x43, &cmd[0], 3,
			DEFAULT_RETRY_CNT) < 0)
			return 0;

		cmd[0] = 0x05;
		if (i2c_himax_write(private_ts->client, 0xD2 ,&cmd[0], 1,
			DEFAULT_RETRY_CNT) < 0)
			return 0;

		cmd[0] = 0x01;
		if (i2c_himax_write(private_ts->client, 0x53 ,&cmd[0], 1,
			DEFAULT_RETRY_CNT) < 0)
			return 0;
		msleep(200);

		if (i2c_himax_read(private_ts->client, 0xAD, cmd, 4,
			DEFAULT_RETRY_CNT) < 0)
			return -1;

		pr_debug("0xAD[0,1,2,3] = %u,%u,%u,%u\n",
			cmd[0], cmd[1], cmd[2], cmd[3]);

		if (cmd[0] == 0 && cmd[1] == 0 && cmd[2] == 0 && cmd[3] == 0 ) {
			himax_FlashMode(0);
			goto CHECK_PASS;
		} else {
			himax_FlashMode(0);
			goto CHECK_FAIL;
		}

CHECK_PASS:
		if (change_iref) {
			if (iref_flag < 3)
				continue;
			else
				return 1;
		} else {
			return 1;
		}

CHECK_FAIL:		
		return 0;
	}
	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs(unsigned char *fw, int len,
	bool change_iref)
{
	unsigned char *ImageBuffer = fw;
	int fullFileLength = len;
	int i, j;
	uint8_t cmd[5], last_byte, prePage;
	int FileLength;
	uint8_t checksumResult = 0;

	for (j = 0; j < 3; j++) {
		FileLength = fullFileLength;		

		himax_chip_reset(false, false);

		if (i2c_himax_write(private_ts->client, 0x81 ,&cmd[0], 0,
			DEFAULT_RETRY_CNT) < 0)
			return 0;

		msleep(120);

		himax_lock_flash(0);

		cmd[0] = 0x05; cmd[1] = 0x00; cmd[2] = 0x02;
		if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 3,
			DEFAULT_RETRY_CNT) < 0)
			return 0;

		if (i2c_himax_write(private_ts->client, 0x4F ,&cmd[0], 0,
			DEFAULT_RETRY_CNT) < 0)
			return 0;
		msleep(50);

		himax_ManualMode(1);
		himax_FlashMode(1);

		FileLength = (FileLength + 3) / 4;
		for (i = 0, prePage = 0; i < FileLength; i++) {
			last_byte = 0;
			cmd[0] = i & 0x1F;
			if (cmd[0] == 0x1F || i == FileLength - 1)
				last_byte = 1;

			cmd[1] = (i >> 5) & 0x1F;
			cmd[2] = (i >> 10) & 0x1F;
			if (i2c_himax_write(private_ts->client, 0x44 ,&cmd[0], 3,
				DEFAULT_RETRY_CNT) < 0)
				return 0;

			if (prePage != cmd[1] || i == 0) {
				prePage = cmd[1];
				cmd[0] = 0x01;
				cmd[1] = 0x09;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;

				cmd[0] = 0x01;
				cmd[1] = 0x0D;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;

				cmd[0] = 0x01;
				cmd[1] = 0x09;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;
			}

			memcpy(&cmd[0], &ImageBuffer[4*i], 4);
			if (i2c_himax_write(private_ts->client, 0x45 ,&cmd[0], 4,
				DEFAULT_RETRY_CNT) < 0)
				return 0;

			cmd[0] = 0x01;
			cmd[1] = 0x0D;
			/* cmd[2] = 0x02; */
			if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
				DEFAULT_RETRY_CNT) < 0)
				return 0;

			cmd[0] = 0x01;
			cmd[1] = 0x09;
			/* cmd[2] = 0x02; */
			if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
				DEFAULT_RETRY_CNT) < 0)
				return 0;

			if (last_byte == 1) {
				cmd[0] = 0x01;
				cmd[1] = 0x01;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;

				cmd[0] = 0x01;
				cmd[1] = 0x05;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;

				cmd[0] = 0x01;
				cmd[1] = 0x01;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;

				cmd[0] = 0x01;
				cmd[1] = 0x00;
				/* cmd[2] = 0x02; */
				if (i2c_himax_write(private_ts->client, 0x43 ,&cmd[0], 2,
					DEFAULT_RETRY_CNT) < 0)
					return 0;

				usleep_range(10000, 15000);
				if (i == (FileLength - 1)) {
					himax_FlashMode(0);
					himax_ManualMode(0);
					checksumResult = himax_calculateChecksum(change_iref);
					/* himax_ManualMode(0); */
					himax_lock_flash(1);

					if (checksumResult) {
						return 1;
					} else {
						pr_err("checksumResult failed !\n");
						return 0;
					}
				}
			}
		}
	}
	return 0;
}

static int himax_input_register(struct himax_ts_data *ts)
{
	int ret;
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		pr_err("allocate input device failed !\n");
		return ret;
	}
	ts->input_dev->name = "himax-touchscreen";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);

	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
#if defined(HX_SMART_WAKEUP)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif	
	set_bit(BTN_TOUCH, ts->input_dev->keybit);

	set_bit(KEY_APP_SWITCH, ts->input_dev->keybit);

	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	if (ts->protocol_type == PROTOCOL_TYPE_A) {
		input_set_abs_params(ts->input_dev,
			ABS_MT_TRACKING_ID, 0, 3, 0, 0);
	} else {
		set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
		input_mt_init_slots(ts->input_dev,
			ts->nFinger_support,0);
	}

	pr_debug("x_min=%d, x_max=%d, y_min=%d, y_max=%d\n",
		ts->pdata->abs_x_min, ts->pdata->abs_x_max,
		ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
		ts->pdata->abs_x_min,
		ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
		ts->pdata->abs_y_min,
		ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
		ts->pdata->abs_pressure_min,
		ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
		ts->pdata->abs_pressure_min,
		ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
		ts->pdata->abs_width_min,
		ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);
	return input_register_device(ts->input_dev);
}

/* calculate the i2c data size */
static void calc_data_size(uint8_t finger_num)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->coord_data_size = 4 * finger_num;
	ts_data->area_data_size = ((finger_num / 4) +
		(finger_num % 4 ? 1 : 0)) * 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size -
		ts_data->area_data_size - 4 - 4 - 1;
	ts_data->raw_data_nframes  =
		((uint32_t)ts_data->x_channel * ts_data->y_channel +
		ts_data->x_channel +
		ts_data->y_channel) / ts_data->raw_data_frame_size +
		(((uint32_t)ts_data->x_channel * ts_data->y_channel +
		ts_data->x_channel +
		ts_data->y_channel) % ts_data->raw_data_frame_size)? 1 : 0;
	pr_debug("coord_data_size=%d, area_data_size=%d\n",
		ts_data->coord_data_size, ts_data->area_data_size);
	pr_debug("raw_data_frame_size=%d, raw_data_nframes=%d\n",
		ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

static void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = HX_MAX_PT * 4 ;

	if ((HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (HX_MAX_PT / 4) * 4 ;
	else
		HX_TOUCH_INFO_POINT_CNT += ((HX_MAX_PT / 4) +1) * 4 ;
}

static void himax_touch_information(void)
{
	char data[12] = {0};

	/* fix IC type to 'HX_85XX_ES_SERIES_PWON' */
	data[0] = 0x8C;
	data[1] = 0x14;
	i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	data[0] = 0x8B;
	data[1] = 0x00;
	data[2] = 0x70;
	i2c_himax_master_write(private_ts->client, &data[0],3,DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	i2c_himax_read(private_ts->client, 0x5A, data, 12, DEFAULT_RETRY_CNT);
	HX_RX_NUM = data[0];
	HX_TX_NUM = data[1];
	HX_MAX_PT = 10;
#ifdef HX_EN_SEL_BUTTON
	HX_BT_NUM = (data[2] & 0x0F);
#endif
	if ((data[4] & 0x04) == 0x04) {
		HX_XY_REVERSE = true;
		HX_Y_RES = data[6] * 256 + data[7];
		HX_X_RES = data[8] * 256 + data[9];
	} else {
		HX_XY_REVERSE = false;
		HX_X_RES = data[6] * 256 + data[7];
		HX_Y_RES = data[8] * 256 + data[9];
	}
	data[0] = 0x8C;
	data[1] = 0x00;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);

#ifdef HX_EN_MUT_BUTTON
	data[0] = 0x8C;
	data[1] = 0x14;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	data[0] = 0x8B;
	data[1] = 0x00;
	data[2] = 0x64;
	i2c_himax_master_write(private_ts->client, &data[0], 3,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	i2c_himax_read(private_ts->client, 0x5A, data, 4,
		DEFAULT_RETRY_CNT);
	HX_BT_NUM = (data[0] & 0x03);
	data[0] = 0x8C;
	data[1] = 0x00;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
#endif

#ifdef HX_TP_SYS_2T2R
	data[0] = 0x8C;
	data[1] = 0x14;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);

	data[0] = 0x8B;
	data[1] = 0x00;
	data[2] = 0x96;
	i2c_himax_master_write(private_ts->client, &data[0], 3,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
			
	i2c_himax_read(private_ts->client, 0x5A, data, 10,
		DEFAULT_RETRY_CNT);

	HX_RX_NUM_2 = data[0];				 
	HX_TX_NUM_2 = data[1];				 
	
	pr_debug("Touch Panel Type = %d\n", data[2]);
	if (data[2] == 0x02)
		Is_2T2R = true;
	else
		Is_2T2R = false;
		
	data[0] = 0x8C;
	data[1] = 0x00;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
#endif
	data[0] = 0x8C;
	data[1] = 0x14;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	data[0] = 0x8B;
	data[1] = 0x00;
	data[2] = 0x02;
	i2c_himax_master_write(private_ts->client, &data[0], 3,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	data[0] = 0x8C;
	data[1] = 0x00;
	i2c_himax_master_write(private_ts->client, &data[0], 2,
		DEFAULT_RETRY_CNT);
	usleep_range(10000, 15000);
	pr_debug("HX_RX_NUM =%d, HX_TX_NUM =%d, HX_MAX_PT=%d\n",
		HX_RX_NUM, HX_TX_NUM, HX_MAX_PT);
}

static int himax_read_sensor_id(struct i2c_client *client)
{	
	uint8_t val_high[1], val_low[1], ID0=0, ID1=0;
	char data[3];
	const int normalRetry = 10;
	int sensor_id;

	/*ID pin PULL High*/
	data[0] = 0x56; data[1] = 0x02; data[2] = 0x02;
	i2c_himax_master_write(client, &data[0],3,normalRetry);
	usleep(1000);

	/* read id pin high */
	i2c_himax_read(client, 0x57, val_high, 1, normalRetry);

	/*ID pin PULL Low*/
	data[0] = 0x56; data[1] = 0x01; data[2] = 0x01;
	i2c_himax_master_write(client, &data[0],3,normalRetry);
	usleep(1000);

	/* read id pin low */
	i2c_himax_read(client, 0x57, val_low, 1, normalRetry);

	if((val_high[0] & 0x01) == 0)
		ID0 = 0x02;/*GND*/
	else if((val_low[0] & 0x01) == 0)
		ID0 = 0x01;/*Floating*/
	else
		ID0 = 0x04;/*VCC*/
	
	if((val_high[0] & 0x02) == 0)
		ID1 = 0x02;/*GND*/
	else if((val_low[0] & 0x02) == 0)
		ID1 = 0x01;/*Floating*/
	else
		ID1 = 0x04;/*VCC*/
	if ((ID0 == 0x04) && (ID1 != 0x04)) {
		/*ID pin PULL High,Low*/
		data[0] = 0x56; data[1] = 0x02; data[2] = 0x01;
		i2c_himax_master_write(client, &data[0],3, normalRetry);
		usleep(1000);
	}
	else if ((ID0 != 0x04) && (ID1 == 0x04)) {
		/*ID pin PULL Low,High*/
		data[0] = 0x56; data[1] = 0x01; data[2] = 0x02;
		i2c_himax_master_write(client, &data[0],3, normalRetry);
		usleep(1000);
	}
	else if ((ID0 == 0x04) && (ID1 == 0x04)) {
		/*ID pin PULL High,High*/
		data[0] = 0x56; data[1] = 0x02; data[2] = 0x02;
		i2c_himax_master_write(client, &data[0],3, normalRetry);
		usleep(1000);
	}
	sensor_id = (ID1<<4) | ID0;

	data[0] = 0xF3; data[1] = sensor_id;
	/*Write to MCU*/
	i2c_himax_master_write(client, &data[0], 2, normalRetry);
	usleep(1000);

	return sensor_id;
}

static void himax_power_on_init_command(struct i2c_client *client)
{
	const int normalRetry = 10;

	i2c_himax_write_command(client, 0x83, normalRetry);
	msleep(30);
	i2c_himax_write_command(client, 0x81, normalRetry);
	msleep(50);
	i2c_himax_write_command(client, 0x82, normalRetry);
	msleep(50);
	i2c_himax_write_command(client, 0x80, normalRetry);
	msleep(50);
	himax_touch_information();
	i2c_himax_write_command(client, 0x83, normalRetry);
	msleep(30);
	i2c_himax_write_command(client, 0x81, normalRetry);
	msleep(50);
}

static void himax_read_tp_info(struct i2c_client *client)
{
	char data[12] = {0};

	if (i2c_himax_read(client, 0x33, data, 1, 3) < 0)
		return;
	private_ts->vendor_fw_ver = data[0];

	if (i2c_himax_read(client, 0x39, data, 1, 3) < 0)
		return;
	private_ts->vendor_config_ver = data[0];

	private_ts->vendor_sensor_id = himax_read_sensor_id(client);

	pr_warn("vendor_id=0x%x, vendor_ver=0x%x, FW=0x%x\n",
		(u32)private_ts->vendor_sensor_id,
		(u32)private_ts->vendor_fw_ver, private_ts->vendor_config_ver);
}

#ifdef HX_AUTO_UPDATE_FW
static void himax_fw_update(void)
{
	unsigned char* ImageBuffer = i_CTPM_FW;
	int fullFileLength = sizeof(i_CTPM_FW);
	uint8_t checksumResult = 0;
	char data[12]= {0};
	int ret;

	if (private_ts->vendor_sensor_id == 0x11) {
		ImageBuffer = i_CTPM_FW;
		fullFileLength = sizeof(i_CTPM_FW);
		pr_info("ctp module_name = truly\n");
	} else {
		pr_warn("No compared id, stop update firmware.\n");
		return;
	}

	checksumResult = himax_calculateChecksum(true);

	pr_warn("image buffer FW: 0x%x, checksum: 0x%x\n",
		*(ImageBuffer + FW_CFG_VER_FLASH_ADDR), (u32)checksumResult);

	if ((private_ts->vendor_config_ver <
		*(ImageBuffer+ FW_CFG_VER_FLASH_ADDR)) ||
		(checksumResult == 0)) {
		ret = fts_ctpm_fw_upgrade_with_sys_fs(ImageBuffer,
			fullFileLength, true);

		himax_chip_reset(false, false);

		/* update fw version */
		if (i2c_himax_read(private_ts->client, 0x33, data, 1, 3) < 0)
			return;
		private_ts->vendor_fw_ver = data[0];

		if (i2c_himax_read(private_ts->client, 0x39, data, 1, 3) < 0)
			return;
		private_ts->vendor_config_ver = data[0];
		pr_warn("update firmware %s ! new FW=0x%x, vendor_ver=0x%x\n",
			ret == 0 ? "failed" : "success",
			(u32)private_ts->vendor_config_ver,
			(u32)private_ts->vendor_fw_ver);
	}
}
#endif

static int himax_load_sensor_config(struct i2c_client *client,
	struct himax_i2c_platform_data *pdata)
{
#ifdef HX_ESD_WORKAROUND
	char data[12] = {0};
#endif

	if (!client) {
		pr_err("client is null !\n");
		return -1;
	}

	if (config_load == false) {
		config_selected = kzalloc(sizeof(*config_selected), GFP_KERNEL);
		if (config_selected == NULL) {
			pr_err("alloc config_selected failed !\n");
			return -1;
		}
	}

#ifndef CONFIG_OF
	pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("pdata is null !\n");
		return -1;
	}
#endif

#ifdef HX_ESD_WORKAROUND
	/* Check R36 to check IC Status */
	i2c_himax_read(client, 0x36, data, 2, 10);
	if ((data[0]!=0x0F) || (data[1]!=0x53)) {
		pr_err("IC is abnormal ! data[0]=%d, data[1]=%d\n", data[0], data[1]);
		return -1;
	}
#endif

	himax_power_on_init_command(client);
	return 1;	
}

static DEFINE_MUTEX(reset_lock);
static void himax_chip_reset(bool load_cfg, bool int_off)
{
	int ret;
	int try = 0;
#ifdef HX_SMART_WAKEUP
	uint8_t buf[2] = {0};
#endif

	if (!mutex_trylock(&reset_lock))
		return;

	reset_active = true;

	if (int_off)
		himax_int_enable(private_ts->client->irq, 0);

	while (try <= 3) {
		if (gpio_is_valid(private_ts->rst_gpio)) {
			himax_rst_gpio_set(private_ts->rst_gpio, 0);
			msleep(20);
			himax_rst_gpio_set(private_ts->rst_gpio, 1);
			msleep(20);
		} else {
			pr_err("no valid reset gpio !\n");
		}

		if (!load_cfg)
			break;

		ret = himax_load_sensor_config(private_ts->client,
			private_ts->pdata);
		if (ret < 0) {
			try++;
			pr_warn("try = %d !\n", try);
		} else {
			break;
		}
	}

#ifdef HX_SMART_WAKEUP
	if (atomic_read(&private_ts->suspend_mode) &&
		private_ts->smwake_enable) {
		buf[0] = 0x8F;
		buf[1] = 0x20;
		ret = i2c_himax_master_write(private_ts->client, buf, 2,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			pr_err("i2c write reg 0x8f failed %d\n", ret);
		pr_info("reset done, back to wakeup mode\n");
	} else {
		pr_debug("reset done\n");
	}
#endif

	if (int_off)
		himax_int_enable(private_ts->client->irq, 1);
	mutex_unlock(&reset_lock);
}

static bool himax_ic_package_check(struct himax_ts_data *ts)
{
	uint8_t cmd[3];
	uint8_t data[3];

	memset(cmd, 0x00, sizeof(cmd));
	memset(data, 0x00, sizeof(data));

	if (i2c_himax_read(ts->client, 0xD1, cmd, 3,
		DEFAULT_RETRY_CNT) < 0)
		return false ;

	if (i2c_himax_read(ts->client, 0x31, data, 3,
		DEFAULT_RETRY_CNT) < 0)
		return false;

	if ((data[0] == 0x85) && (data[1] == 0x31))
		pr_info("Himax IC package '852x ES'\n");
	else
		pr_warn("Unknown IC package !\n");

	return true;
}

#ifdef HX_ESD_MONITOR
static void himax_esd_check_work(struct work_struct *work)
{
	int ret = 0;
	struct delayed_work *dwork = to_delayed_work(work);
	struct himax_ts_data *ts = container_of(dwork,
				struct himax_ts_data, esd_dwork);

	ret = himax_hand_shaking();
	if (ret == 2) {
		pr_err("I2C failed !\n");
		himax_chip_reset(true, true);
	} else if (ret == 1) {
		pr_err("MCU stopped !\n");
		himax_chip_reset(true, true);
	}

	schedule_delayed_work(&ts->esd_dwork,
		msecs_to_jiffies(HX_ESD_MONITOR_INTERVAL));
}
#endif

#ifdef HX_SMART_WAKEUP
static int himax_parse_wake_event(struct himax_ts_data *ts)
{
	uint8_t buf[32];

	if (i2c_himax_read(ts->client, 0x86, buf, 32,
		HIMAX_I2C_RETRY_TIMES))
		pr_err("can't read data from chip !\n");

	if ((buf[0]==0x80) && (buf[1]==0x80) &&
		(buf[2]==0x80) && (buf[3]==0x80) &&
		(buf[4]==0x80) && (buf[5]==0x80) &&
		(buf[6]==0x80)) {
		return 1;
	} else {
		I("%s: not wake packet, buf: %x, %x, %x, %x\n",
			__func__, (u32)buf[0],
			(u32)buf[1],(u32)buf[2], (u32)buf[3]);
		return 0;
	}
}
#endif

static void himax_ts_button_func(int tp_key_index,struct himax_ts_data *ts)
{
	uint16_t x_position = 0, y_position = 0;

	if (tp_key_index != 0x00) {
		I("virtual key index = %d, virtual_key = 0x%p\n",
			tp_key_index, ts->pdata->virtual_key);
		if (tp_key_index == 0x01) {
			vk_press = 1;
			I("back key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[0].index) {
					x_position = (ts->button[0].x_range_min +
						ts->button[0].x_range_max) / 2;
					y_position = (ts->button[0].y_range_min +
						ts->button[0].y_range_max) / 2;
				}

				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						y_position);
					input_mt_sync(ts->input_dev);
				} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, 0);
					input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, 1);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						y_position);
				}
			} else {
				input_report_key(ts->input_dev, KEY_BACK, 1);
			}
		} else if (tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[1].index) {
					x_position = (ts->button[1].x_range_min +
						ts->button[1].x_range_max) / 2;
					y_position = (ts->button[1].y_range_min +
						ts->button[1].y_range_max) / 2;
				}

				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						y_position);
					input_mt_sync(ts->input_dev);
				} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, 0);
					input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, 1);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						y_position);
				}
			} else {
				input_report_key(ts->input_dev, KEY_HOME, 1);
			}
		} else if ( tp_key_index == 0x04) {
			vk_press = 1;
			I("app_switch key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[2].index) {
					x_position = (ts->button[2].x_range_min +
						ts->button[2].x_range_max) / 2;
					y_position = (ts->button[2].y_range_min +
						ts->button[2].y_range_max) / 2;
				}

				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						y_position);
					input_mt_sync(ts->input_dev);
				} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, 0);
					input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, 1);
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
						x_position);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
						y_position);
				}
			} else {
				input_report_key(ts->input_dev, KEY_APP_SWITCH, 1);	
			}
		}
		input_sync(ts->input_dev);
	} else {/*tp_key_index =0x00*/
		I("virtual key released\n");
		vk_press = 0;
		if (ts->protocol_type == PROTOCOL_TYPE_A) {
			input_mt_sync(ts->input_dev);
		} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
			input_mt_slot(ts->input_dev, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		}
		input_report_key(ts->input_dev, KEY_BACK, 0);
		input_report_key(ts->input_dev, KEY_HOME, 0);
		input_report_key(ts->input_dev, KEY_APP_SWITCH, 0);
		input_sync(ts->input_dev);
	}
}

static void himax_ts_work(struct himax_ts_data *ts)
{
	int ret = 0;
	uint8_t buf[128], finger_num, hw_reset_check[2];
	uint16_t finger_pressed;
	uint8_t finger_on = 0;
	int32_t loop_i;
	uint8_t coordInfoSize = ts->coord_data_size + ts->area_data_size + 4;
	unsigned char check_sum_cal = 0;
	int RawDataLen = 0;
	int raw_cnt_max ;
	int raw_cnt_rmd ;
	int hx_touch_info_size;
	int base,x,y,w;
	int  	i;

#ifdef HX_TP_SYS_DIAG
	uint8_t *mutual_data;
	uint8_t *self_data;
	uint8_t diag_cmd;
	int 	mul_num;
	int 	self_num;
	int 	index = 0;
	int  	temp1, temp2;
	/* coordinate dump start */
	char coordinate_char[15+(HX_MAX_PT+5)*2*5+2];
	struct timeval t;
	struct tm broken;
	/* coordinate dump end */
#endif

	memset(buf, 0x00, sizeof(buf));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	raw_cnt_max = HX_MAX_PT / 4;
	raw_cnt_rmd = HX_MAX_PT % 4;

	if (raw_cnt_rmd != 0x00) {/* more than 4 fingers */
		RawDataLen = 128 - ((HX_MAX_PT+raw_cnt_max+3)*4) - 1;
		hx_touch_info_size = (HX_MAX_PT+raw_cnt_max+2)*4;
	} else {/* less than 4 fingers */
		RawDataLen = 128 - ((HX_MAX_PT+raw_cnt_max+2)*4) - 1;
		hx_touch_info_size = (HX_MAX_PT+raw_cnt_max+1)*4;
	}

#ifdef HX_TP_SYS_DIAG
	diag_cmd = getDiagCommand();
	if (diag_cmd){
		ret = i2c_himax_read(ts->client, 0x86, buf, 128,
			HIMAX_I2C_RETRY_TIMES);
	} else {
		if (touch_monitor_stop_flag != 0) {
			ret = i2c_himax_read(ts->client, 0x86, buf, 128,
				HIMAX_I2C_RETRY_TIMES);
			touch_monitor_stop_flag-- ;
		} else {
			ret = i2c_himax_read(ts->client, 0x86, buf,
				hx_touch_info_size, HIMAX_I2C_RETRY_TIMES);
		}
	}
	if (ret) {
#else
	if (i2c_himax_read(ts->client, 0x86, buf, hx_touch_info_size,
		HIMAX_I2C_RETRY_TIMES)) {
#endif
		pr_err("i2c read reg 0x86 failed !\n");
		goto exit_with_reset;
	} else {
#ifdef HX_ESD_WORKAROUND
		for (i = 0; i < hx_touch_info_size; i++) {
			if (buf[i] == 0x00) /* case 2 ESD recovery flow-Disable */
				check_sum_cal = 1;
			else if(buf[i] == 0xED)/* case 1 ESD recovery flow */
				check_sum_cal = 2;
			else {
				check_sum_cal = 0;
				i = hx_touch_info_size;
			}
		}

		/* IC status is abnormal ,do hand shaking */
#ifdef HX_TP_SYS_DIAG
		diag_cmd = getDiagCommand();
#ifdef HX_ESD_WORKAROUND
		if (check_sum_cal != 0 && !reset_active && diag_cmd == 0)
#else
		if (check_sum_cal != 0 && diag_cmd == 0)
#endif

#else
#ifdef HX_ESD_WORKAROUND
		if (check_sum_cal != 0 && !reset_active)
#else
		if (check_sum_cal != 0)
#endif
#endif
		{
			ret = himax_hand_shaking();
			if (ret == 2) {
				pr_err("I2C failed !\n");
				goto exit_with_reset;
			}

			if ((ret == 1) && (check_sum_cal == 1)) {
				pr_err("ESD event checked - ALL Zero\n");
				himax_chip_reset(true, true);
			} else if (check_sum_cal == 2) {
				pr_err("ESD event checked - ALL 0xED\n");
				himax_chip_reset(true, true);
			}

			return;
		} else if (reset_active) {
			reset_active = false; /*drop 1st interrupts after chip reset*/
			pr_debug("back from reset, ready to serve\n");
			return;
		}
#else
		if (reset_active) {
			reset_active = false; /*drop 1st interrupts after chip reset*/
			pr_debug("back from reset, ready to serve\n");
			return;
		}
#endif

		for (loop_i = 0, check_sum_cal = 0;
			loop_i < hx_touch_info_size; loop_i++)
			check_sum_cal += buf[loop_i];

		if ((check_sum_cal != 0x00)) {
			pr_err("checksum failed: 0x%02x\n", check_sum_cal);
			return;
		}
	}

	/* touch monitor raw data fetch */
#ifdef HX_TP_SYS_DIAG
	diag_cmd = getDiagCommand();
	if (diag_cmd >= 1 && diag_cmd <= 6) {
		/* Check 128th byte CRC */
		for (i = hx_touch_info_size, check_sum_cal = 0; i < 128; i++) {
			check_sum_cal += buf[i];
		}

		if (check_sum_cal % 0x100 != 0)
			goto bypass_checksum_failed_packet;

#ifdef HX_TP_SYS_2T2R
		if(Is_2T2R && diag_cmd == 4) {
			mutual_data = getMutualBuffer_2();
			self_data 	= getSelfBuffer();

			/* initiallize the block number of mutual and self */
			mul_num = getXChannel_2() * getYChannel_2();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel_2() + getYChannel_2() + HX_BT_NUM;
#else
			self_num = getXChannel_2() + getYChannel_2();
#endif
		} else
#endif
		{
			mutual_data = getMutualBuffer();
			self_data 	= getSelfBuffer();

			/* initiallize the block number of mutual and self */
			mul_num = getXChannel() * getYChannel();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel() + getYChannel() + HX_BT_NUM;
#else
			self_num = getXChannel() + getYChannel();
#endif
		}

		/* Himax: Check Raw-Data Header */
		if (buf[hx_touch_info_size] == buf[hx_touch_info_size+1] &&
			buf[hx_touch_info_size+1] == buf[hx_touch_info_size+2] &&
			buf[hx_touch_info_size+2] == buf[hx_touch_info_size+3] &&
			buf[hx_touch_info_size] > 0) {

			index = (buf[hx_touch_info_size] - 1) * RawDataLen;
			for (i = 0; i < RawDataLen; i++) {
				temp1 = index + i;
				if (temp1 < mul_num) {
					mutual_data[index + i] = buf[i + hx_touch_info_size+4];
				} else {
					temp1 = i + index;
					temp2 = self_num + mul_num;
					if (temp1 >= temp2)
						break;
					self_data[i+index-mul_num] = buf[i + hx_touch_info_size+4];				
				}
			}
		} else {
			pr_err("header format is wrong !\n");
		}
	} else if (diag_cmd == 7) {
		memcpy(&(diag_coor[0]), &buf[0], 128);
	}

	/* coordinate dump start */
	if (coordinate_dump_enable == 1) {
		for (i = 0; i < (15 + (HX_MAX_PT + 5) * 2 * 5); i++) {
			coordinate_char[i] = 0x20;
		}
		coordinate_char[15 + (HX_MAX_PT + 5) * 2 * 5] = 0xD;
		coordinate_char[15 + (HX_MAX_PT + 5) * 2 * 5 + 1] = 0xA;
	}
	/* coordinate dump end */

bypass_checksum_failed_packet:
#endif
	EN_NoiseFilter = (buf[HX_TOUCH_INFO_POINT_CNT+2]>>3);
	EN_NoiseFilter = EN_NoiseFilter & 0x01;

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	tpd_key = (buf[HX_TOUCH_INFO_POINT_CNT+2]>>4);
	if (tpd_key == 0x0F)/*All (VK+AA)leave*/
		tpd_key = 0x00;
#else
	tpd_key = 0x00;
#endif

	p_point_num = hx_point_num;
	if (buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
		hx_point_num = 0;
	else
		hx_point_num= buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;

	/* Touch Point information */
	if (hx_point_num != 0 ) {
		if(vk_press == 0x00) {
			finger_num = buf[coordInfoSize - 4] & 0x0F;
			finger_pressed = buf[coordInfoSize - 2] << 8 |
				buf[coordInfoSize - 3];
			finger_on = 1;
			AA_press = 1;
			for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
				if (((finger_pressed >> loop_i) & 1) == 1) {
					base = loop_i * 4;
					x = buf[base] << 8 | buf[base + 1];
					y = (buf[base + 2] << 8 | buf[base + 3]);
					w = buf[(ts->nFinger_support * 4) + loop_i];
					finger_num--;

					if (ts->protocol_type == PROTOCOL_TYPE_B)
						input_mt_slot(ts->input_dev, loop_i);

					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
					input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
	
					if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID,
							loop_i);
						input_mt_sync(ts->input_dev);
					} else {
						ts->last_slot = loop_i;
						input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, 1);
					}

					if (!ts->first_pressed) {
						ts->first_pressed = 1;
						I("S1@%d, %d\n", x, y);
					}
					ts->pre_finger_data[loop_i][0] = x;
					ts->pre_finger_data[loop_i][1] = y;					
				} else {
					if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, loop_i);
						input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, 0);
					}

					if (loop_i == 0 && ts->first_pressed == 1) {
						ts->first_pressed = 2;
						I("E1@%d, %d\n",
						ts->pre_finger_data[0][0],
						ts->pre_finger_data[0][1]);
					}
				}
			}
			ts->pre_finger_mask = finger_pressed;
		} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			himax_ts_button_func(tpd_key,ts);
			finger_on = 0;
		}
		input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
		input_sync(ts->input_dev);
	} else if (hx_point_num == 0){
		if (AA_press) {
			/* leave event */
			finger_on = 0;
			AA_press = 0;
			if (ts->protocol_type == PROTOCOL_TYPE_A)
				input_mt_sync(ts->input_dev);

			for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
				if (((ts->pre_finger_mask >> loop_i) & 1) == 1) {
					if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, loop_i);
						input_mt_report_slot_state(ts->input_dev,
							MT_TOOL_FINGER, 0);
					}
				}
			}

			if (ts->pre_finger_mask > 0)
				ts->pre_finger_mask = 0;

			if (ts->first_pressed == 1) {
				ts->first_pressed = 2;
				I("E1@%d, %d\n",ts->pre_finger_data[0][0],
					ts->pre_finger_data[0][1]);
			}

#ifdef HX_TP_SYS_DIAG
			/* coordinate dump start */
			if (coordinate_dump_enable == 1) {
				do_gettimeofday(&t);
				time_to_tm(t.tv_sec, 0, &broken);

				sprintf(&coordinate_char[0], "%2d:%2d:%2d:%lu,",
					broken.tm_hour, broken.tm_min,
					broken.tm_sec, t.tv_usec/1000);
				sprintf(&coordinate_char[15], "Touch up!");
				coordinate_fn->f_op->write(coordinate_fn,
					&coordinate_char[0],
					15 + (HX_MAX_PT + 5) * 2 * sizeof(char) * 5 + 2,
					&coordinate_fn->f_pos);
			}
			/* coordinate dump end */
#endif
		} else if (tpd_key != 0x00) {
			/* report key */
			himax_ts_button_func(tpd_key,ts);
			finger_on = 1;
		} else if ((tpd_key_old != 0x00)&&(tpd_key == 0x00)) {
			himax_ts_button_func(tpd_key,ts);
			finger_on = 0;
		}
		input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
		input_sync(ts->input_dev);
	}
	tpd_key_old = tpd_key;
	Last_EN_NoiseFilter = EN_NoiseFilter;

	return;

exit_with_reset:
	pr_warn("reset CHIP !\n");
	himax_chip_reset(true, true);
}

static irqreturn_t himax_ts_thread(int irq, void *ptr)
{
#ifdef HX_SMART_WAKEUP
	struct himax_ts_data *ts = ptr;
	int ret;
#endif

#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode) && !FAKE_POWER_KEY_SEND &&
		ts->smwake_enable) {
		wake_lock_timeout(&ts->smwake_wake_lock, 2*HZ);
		if (himax_parse_wake_event((struct himax_ts_data *)ptr)) {
			ret = pa12200001_ps_state();
			if (ret == 0) {
				pr_info("wakeup system\n");
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				msleep(20);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);
				FAKE_POWER_KEY_SEND=true;
				qpnp_kernel_vib_enable(50);
			}
		}
		return IRQ_HANDLED;
	}
#endif

	himax_ts_work((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static int himax_ts_register_interrupt(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;

	ts->irq_enabled = 0;
	ts->use_irq = 1;

	ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
	if (ret == 0) {
		ts->irq_enabled = 1;
		irq_enable_count = 1;
		pr_info("request irq %d success\n", client->irq);
#ifdef HX_SMART_WAKEUP
		irq_set_irq_wake(client->irq, 1);
#endif
	} else {
		ts->use_irq = 0;
		pr_err("request irq %d failed !\n", client->irq);
	}
	return ret;
}

#ifdef CONFIG_TOUCHSCREEN_HIMAX_DEBUG
static ssize_t touch_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	ts_data = private_ts;

	ret += sprintf(buf,
		"%s_FW:%#x_CFG:%#x_SensorId:%#x\n", HIMAX852XES_NAME,
		ts_data->vendor_fw_ver,
		ts_data->vendor_config_ver, ts_data->vendor_sensor_id);

	return ret;
}

static DEVICE_ATTR(vendor, (S_IRUGO), touch_vendor_show, NULL);

#ifdef HX_TP_SYS_DIAG
static uint8_t *getMutualBuffer(void)
{
	return diag_mutual;
}
static uint8_t *getSelfBuffer(void)
{
	return &diag_self[0];
}
static uint8_t getXChannel(void)
{
	return x_channel;
}
static uint8_t getYChannel(void)
{
	return y_channel;
}
static uint8_t getDiagCommand(void)
{
	return diag_command;
}
static void setXChannel(uint8_t x)
{
	x_channel = x;
}
static void setYChannel(uint8_t y)
{
	y_channel = y;
}
static void setMutualBuffer(void)
{
	diag_mutual = kzalloc(x_channel * y_channel * sizeof(uint8_t),
		GFP_KERNEL);
}

#ifdef HX_TP_SYS_2T2R
static uint8_t *getMutualBuffer_2(void)
{
	return diag_mutual_2;
}
static uint8_t getXChannel_2(void)
{
	return x_channel_2;
}
static uint8_t getYChannel_2(void)
{
	return y_channel_2;
}
static void setXChannel_2(uint8_t x)
{
	x_channel_2 = x;
}
static void setYChannel_2(uint8_t y)
{
	y_channel_2 = y;
}
static void setMutualBuffer_2(void)
{
	diag_mutual_2 = kzalloc(x_channel_2 * y_channel_2 * sizeof(uint8_t),
		GFP_KERNEL);
}
#endif

static ssize_t himax_diag_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	uint32_t loop_i;
	uint16_t mutual_num, self_num, width;
#ifdef HX_TP_SYS_2T2R
	if(Is_2T2R && diag_command == 4)
	{
		mutual_num 	= x_channel_2 * y_channel_2;
		self_num 	= x_channel_2 + y_channel_2; /* don't add KEY_COUNT */
		width 		= x_channel_2;
		count += sprintf(buf + count,
			"ChannelStart: %4d, %4d\n\n", x_channel_2, y_channel_2);
	} else
#endif		
	{
		mutual_num 	= x_channel * y_channel;
		self_num 	= x_channel + y_channel; /* don't add KEY_COUNT */
		width 		= x_channel;
		count += sprintf(buf + count,
			"ChannelStart: %4d, %4d\n\n", x_channel, y_channel);
	}

	/*  start to show out the raw data in adb shell */
	if (diag_command >= 1 && diag_command <= 6) {
		if (diag_command <= 3) {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				count += sprintf(buf + count, "%4d", diag_mutual[loop_i]);
				if ((loop_i % width) == (width - 1))
					count += sprintf(buf + count, " %3d\n",
						diag_self[width + loop_i/width]);
			}
			count += sprintf(buf + count, "\n");
			for (loop_i = 0; loop_i < width; loop_i++) {
				count += sprintf(buf + count, "%4d", diag_self[loop_i]);
				if (((loop_i) % width) == (width - 1))
					count += sprintf(buf + count, "\n");
			}
#ifdef HX_EN_SEL_BUTTON
			count += sprintf(buf + count, "\n");
			for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
					count += sprintf(buf + count, "%4d",
						diag_self[HX_RX_NUM + HX_TX_NUM + loop_i]);
#endif
#ifdef HX_TP_SYS_2T2R
		} else if (Is_2T2R && diag_command == 4 ) {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				count += sprintf(buf + count, "%4d", diag_mutual_2[loop_i]);
				if ((loop_i % width) == (width - 1))
					count += sprintf(buf + count, " %3d\n",
						diag_self[width + loop_i/width]);
			}
			count += sprintf(buf + count, "\n");
			for (loop_i = 0; loop_i < width; loop_i++) {
				count += sprintf(buf + count, "%4d", diag_self[loop_i]);
				if (((loop_i) % width) == (width - 1))
					count += sprintf(buf + count, "\n");
			}

#ifdef HX_EN_SEL_BUTTON
			count += sprintf(buf + count, "\n");
			for (loop_i = 0; loop_i < HX_BT_NUM; loop_i++)
				count += sprintf(buf + count, "%4d",
					diag_self[HX_RX_NUM_2 + HX_TX_NUM_2 + loop_i]);
#endif
#endif
		} else if (diag_command > 4) {
			for (loop_i = 0; loop_i < self_num; loop_i++) {
				count += sprintf(buf + count, "%4d", diag_self[loop_i]);
				if (((loop_i - mutual_num) % width) == (width - 1))
					count += sprintf(buf + count, "\n");
			}
		} else {
			for (loop_i = 0; loop_i < mutual_num; loop_i++) {
				count += sprintf(buf + count, "%4d", diag_mutual[loop_i]);
				if ((loop_i % width) == (width - 1))
					count += sprintf(buf + count, "\n");
			}
		}
		count += sprintf(buf + count, "ChannelEnd");
		count += sprintf(buf + count, "\n");
	} else if (diag_command == 7) {
		for (loop_i = 0; loop_i < 128 ;loop_i++) {
			if ((loop_i % 16) == 0)
				count += sprintf(buf + count, "LineStart:");
				count += sprintf(buf + count, "%4d", diag_coor[loop_i]);
			if ((loop_i % 16) == 15)
				count += sprintf(buf + count, "\n");
		}
	}
	return count;
}

static ssize_t himax_diag_dump(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	const uint8_t command_ec_128_raw_flag = 0x02;
	const uint8_t command_ec_24_normal_flag = 0x00;
	uint8_t command_ec_128_raw_baseline_flag = 0x01;
	uint8_t command_ec_128_raw_bank_flag = 0x03;
	
	uint8_t command_F1h[2] = {0xF1, 0x00};				
	uint8_t receive[1];

	memset(receive, 0x00, sizeof(receive));

	diag_command = buf[0] - '0';
	
	I("[Himax]diag_command=0x%x\n", diag_command);
	if (diag_command == 0x01)	{/* DC */
		command_F1h[1] = command_ec_128_raw_baseline_flag;
		i2c_himax_write(private_ts->client, command_F1h[0],
			&command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x02) {/* IIR */
		command_F1h[1] = command_ec_128_raw_flag;
		i2c_himax_write(private_ts->client, command_F1h[0],
			&command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x03) {/* BANK */
		command_F1h[1] = command_ec_128_raw_bank_flag;/* 0x03 */
		i2c_himax_write(private_ts->client, command_F1h[0],
			&command_F1h[1], 1, DEFAULT_RETRY_CNT);		
	} else if (diag_command == 0x04 ) { /* 2T3R IIR */
		command_F1h[1] = 0x04; /* 2T3R IIR */
		i2c_himax_write(private_ts->client, command_F1h[0],
			&command_F1h[1], 1, DEFAULT_RETRY_CNT);
	} else if (diag_command == 0x00) {/* Disable */
		command_F1h[1] = command_ec_24_normal_flag;
		i2c_himax_write(private_ts->client, command_F1h[0],
			&command_F1h[1], 1, DEFAULT_RETRY_CNT);	
		touch_monitor_stop_flag = touch_monitor_stop_limit;		
	} else if (diag_command == 0x08){/* coordinate dump start */
		coordinate_fn = filp_open(DIAG_COORDINATE_FILE,
			O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, 0666);
		if (IS_ERR(coordinate_fn)) {
			E("%s: coordinate_dump_file_create error\n", __func__);
			coordinate_dump_enable = 0;
			filp_close(coordinate_fn,NULL);
		}
		coordinate_dump_enable = 1;
	} else if (diag_command == 0x09){
		coordinate_dump_enable = 0;
		if (!IS_ERR(coordinate_fn))
			filp_close(coordinate_fn,NULL);
	} else {/* coordinate dump end */
		E("[Himax]Diag command error!diag_command=0x%x\n",
			diag_command);
	}
	return count;
}
static DEVICE_ATTR(diag, (S_IWUSR|S_IRUGO|S_IWUGO),
	himax_diag_show, himax_diag_dump);
#endif

#ifdef HX_TP_SYS_DEBUG
static ssize_t himax_debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	if (debug_level_cmd == 't') {
		if (fw_update_complete)
			count += sprintf(buf, "FW Update Complete ");
		else
			count += sprintf(buf, "FW Update Fail ");
	} else if (debug_level_cmd == 'h') {
		if (handshaking_result == 0)
			count += sprintf(buf,
			"Handshaking Result = %d (MCU Running)\n",
			handshaking_result);
		else if (handshaking_result == 1)
			count += sprintf(buf,
			"Handshaking Result = %d (MCU Stop)\n",
			handshaking_result);
		else if (handshaking_result == 2)
			count += sprintf(buf,
			"Handshaking Result = %d (I2C Error)\n",
			handshaking_result);
		else
			count += sprintf(buf, "Handshaking Result = error \n");
	} else if (debug_level_cmd == 'v') {
		count += sprintf(buf + count, "FW_VER = ");
		count += sprintf(buf + count, "0x%2.2X \n",
			private_ts->vendor_fw_ver);
		count += sprintf(buf + count, "CONFIG_VER = ");
		count += sprintf(buf + count, "0x%2.2X \n",
			private_ts->vendor_config_ver);
		count += sprintf(buf + count, "\n");
	} else if (debug_level_cmd == 'd') {
		count += sprintf(buf + count, "RX Num : %d\n",
			HX_RX_NUM);
		count += sprintf(buf + count, "TX Num : %d\n",
			HX_TX_NUM);
		count += sprintf(buf + count, "BT Num : %d\n",
			HX_BT_NUM);
		count += sprintf(buf + count, "X Resolution : %d\n",
			HX_X_RES);
		count += sprintf(buf + count, "Y Resolution : %d\n",
			HX_Y_RES);
		count += sprintf(buf + count, "Max Point : %d\n",
			HX_MAX_PT);

		#ifdef HX_TP_SYS_2T2R
		if (Is_2T2R) {
			count += sprintf(buf + count, "2T2R panel\n");
			count += sprintf(buf + count, "RX Num_2 : %d\n",
				HX_RX_NUM_2);
			count += sprintf(buf + count, "TX Num_2 : %d\n",
				HX_TX_NUM_2);
		}
		#endif
	}
	return count;
}

static ssize_t himax_debug_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct file* filp = NULL;
	mm_segment_t oldfs;
	int result = 0;
	char fileName[128];
	char data[12] = {0};

	if (buf[0] == 'h') {
		debug_level_cmd = buf[0];
		himax_int_enable(private_ts->client->irq,0);
		handshaking_result = himax_hand_shaking();
		himax_int_enable(private_ts->client->irq,1);
		return count;
	} else if (buf[0] == 'v') {
		debug_level_cmd = buf[0];
		return count;
	} else if (buf[0] == 'd') {
		debug_level_cmd = buf[0];
		return count;
	} else if (buf[0] == 't') {
#ifdef HX_ESD_MONITOR
		cancel_delayed_work_sync(&private_ts->esd_dwork);
#endif
		himax_int_enable(private_ts->client->irq,0);

		debug_level_cmd 		= buf[0];
		fw_update_complete		= false;

		memset(fileName, 0, sizeof(fileName));
		snprintf(fileName, count-2, "%s", &buf[2]);
		pr_notice("update fw from file(%s) start...\n", fileName);
		filp = filp_open(fileName, O_RDONLY, 0);
		if (IS_ERR(filp)) {
			pr_err("open firmware file failed !\n");
			goto firmware_upgrade_err;
		}

		oldfs = get_fs();
		set_fs(get_ds());
		/* read the latest firmware binary file */
		result = filp->f_op->read(filp, upgrade_fw, sizeof(upgrade_fw),
			&filp->f_pos);
		if (result < 0) {
			pr_err("read firmware file failed !\n");
			set_fs(oldfs);
			filp_close(filp, NULL);
			goto firmware_upgrade_err;
		}
		set_fs(oldfs);
		filp_close(filp, NULL);

		pr_notice("len=%d, New FW: %02X, %02X, %02X, %02X ...\n",
			result,
			upgrade_fw[0], upgrade_fw[1], upgrade_fw[2], upgrade_fw[3]);

		if (result > 0) {
			if (fts_ctpm_fw_upgrade_with_sys_fs(upgrade_fw,
				result, true) == 0) {
				pr_err("fw update failed !\n");
				fw_update_complete = false;
			} else {
				pr_warn("fw update success !\n");
				fw_update_complete = true;
			}

			himax_chip_reset(true, true);

			if (i2c_himax_read(private_ts->client, 0x33, data, 1, 3) < 0)
				goto firmware_upgrade_err;
			private_ts->vendor_fw_ver = data[0];

			if (i2c_himax_read(private_ts->client, 0x39, data, 1, 3) < 0)
				goto firmware_upgrade_err;
			private_ts->vendor_config_ver = data[0];
			pr_warn("vendor_config_ver = 0x%x, vendor_fw_ver = 0x%x\n",
				(u32)private_ts->vendor_config_ver,
				(u32)private_ts->vendor_fw_ver);
		}
	} else {
		return count;
	}

firmware_upgrade_err:
	himax_int_enable(private_ts->client->irq, 1);

#ifdef HX_ESD_MONITOR
	schedule_delayed_work(&private_ts->esd_dwork,
		msecs_to_jiffies(HX_ESD_MONITOR_INTERVAL));
#endif
	return count;
}
static DEVICE_ATTR(debug, (S_IWUSR|S_IRUGO|S_IWUGO), himax_debug_show, himax_debug_store);
#endif //HX_TP_SYS_DEBUG


#ifdef HX_TP_SYS_RESET
static ssize_t himax_reset_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
#ifdef HX_ESD_MONITOR
	cancel_delayed_work_sync(&private_ts->esd_dwork);
#endif
	if ((buf[0]=='0') && (buf[1]=='0'))
		himax_chip_reset(false, false);
	else if ((buf[0]=='0') && (buf[1]=='1'))
		himax_chip_reset(false, true);
	else if ((buf[0]=='1') && (buf[1]=='0'))
		himax_chip_reset(true, false);
	else if ((buf[0]=='1') && (buf[1]=='1'))
		himax_chip_reset(true, true);
#ifdef HX_ESD_MONITOR
	schedule_delayed_work(&private_ts->esd_dwork,
		msecs_to_jiffies(HX_ESD_MONITOR_INTERVAL));
#endif
	return count;
}
static DEVICE_ATTR(reset, (S_IWUSR|S_IRUGO), NULL, himax_reset_store);
#endif

#ifdef HX_TP_SYS_SELF_TEST
static ssize_t himax_chip_self_test_function(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int val=0x00;
	val = himax_chip_self_test();

	if (val == 0x00)
		return sprintf(buf, "Self_Test Pass\n");
	else
		return sprintf(buf, "Self_Test Fail\n");
}

static int himax_chip_self_test(void)
{
	uint8_t cmdbuf[11];
	uint8_t valuebuf[16];
	int i=0, pf_value=0x00;

	memset(cmdbuf, 0x00, sizeof(cmdbuf));
	memset(valuebuf, 0x00, sizeof(valuebuf));

	cmdbuf[0] = 0x06;
	i2c_himax_write(private_ts->client, 0xF1,&cmdbuf[0], 1,
		DEFAULT_RETRY_CNT);
	msleep(120);
	i2c_himax_write(private_ts->client, 0x83,&cmdbuf[0], 0,
		DEFAULT_RETRY_CNT);
	msleep(120);
	i2c_himax_write(private_ts->client, 0x81,&cmdbuf[0], 0,
		DEFAULT_RETRY_CNT);
	msleep(2000);
	i2c_himax_write(private_ts->client, 0x82,&cmdbuf[0], 0,
		DEFAULT_RETRY_CNT);
	msleep(120);
	i2c_himax_write(private_ts->client, 0x80,&cmdbuf[0], 0,
		DEFAULT_RETRY_CNT);
	msleep(120);
	cmdbuf[0] = 0x00;
	i2c_himax_write(private_ts->client, 0xF1,&cmdbuf[0], 1,
		DEFAULT_RETRY_CNT);
	msleep(120);
	i2c_himax_read(private_ts->client, 0xB1, valuebuf, 8,
		DEFAULT_RETRY_CNT);
	usleep(10000);

	for (i = 0; i < 8; i++) {
		I("[Himax]: After slf test 0xB1 buff_back[%d] = 0x%x\n",
			i, valuebuf[i]);
	}
	msleep(30);

	if (valuebuf[0] == 0xAA) {
		I("[Himax]: self-test pass\n");
		pf_value = 0x0;
	} else {
		E("[Himax]: self-test fail\n");
		pf_value = 0x1;
	}

	return pf_value;
}
static DEVICE_ATTR(tp_self_test, (S_IWUSR|S_IRUGO|S_IWUGO),
	himax_chip_self_test_function, NULL);
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_smart_wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	count = snprintf(buf, PAGE_SIZE, "%d\n", ts->smwake_enable);
	return count;
}

static ssize_t himax_smart_wake_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = private_ts;

	if (sysfs_streq(buf, "0"))
		ts->smwake_enable = false;
	else if (sysfs_streq(buf, "1"))
		ts->smwake_enable = true;
	else
		return -EINVAL;

	pr_debug("enable = %d\n", ts->smwake_enable);

	return count;
}
static DEVICE_ATTR(SMWP, (S_IWUSR|S_IRUGO|S_IWUGO),
	himax_smart_wake_show, himax_smart_wake_store);
#endif

static int himax_touch_sysfs_init(void)
{
	int ret;
	android_touch_kobj = kobject_create_and_add("android_touch", NULL);
	if (android_touch_kobj == NULL) {
		E("%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_vendor failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_reset.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_reset failed\n", __func__);
		return ret;
	}

	#ifdef HX_TP_SYS_DIAG
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_diag.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_diag failed\n");
		return ret;
	}
	#endif

	#ifdef HX_TP_SYS_DEBUG
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug.attr);
	if (ret) {
		E("create_file dev_attr_debug failed\n");
		return ret;
	}
	#endif

	#ifdef HX_TP_SYS_SELF_TEST
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_tp_self_test.attr);
	if (ret) {
		E("sysfs_create_file dev_attr_tp_self_test failed\n");
		return ret;
	}
	#endif

	#ifdef HX_SMART_WAKEUP
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_SMWP.attr);
	if (ret)  {
		E("sysfs_create_file dev_attr_cover failed\n");
		return ret;
	}
	#endif

	return 0 ;
}

static void himax_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_reset.attr);

	#ifdef HX_TP_SYS_DEBUG
	sysfs_remove_file(android_touch_kobj, &dev_attr_debug.attr);
	#endif

	#ifdef HX_TP_SYS_DIAG
	sysfs_remove_file(android_touch_kobj, &dev_attr_diag.attr);
	#endif

	#ifdef HX_TP_SYS_SELF_TEST
	sysfs_remove_file(android_touch_kobj, &dev_attr_tp_self_test.attr);
	#endif

	#ifdef HX_SMART_WAKEUP
	sysfs_remove_file(android_touch_kobj, &dev_attr_SMWP.attr);
	#endif

	kobject_del(android_touch_kobj);
}
#endif

#ifdef CONFIG_OF
static void himax_vk_parser(struct device_node *dt,
	struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);
	if (node == NULL) {
		pr_debug("virtualkey configuration is not found in dt\n");
		return;
	} else {
		while ((pp = of_get_next_child(node, pp)))
			cnt++;
		if (!cnt)
			return;

		vk = kzalloc(cnt * (sizeof *vk), GFP_KERNEL);
		pp = NULL;
		while ((pp = of_get_next_child(node, pp))) {
			if (of_property_read_u32(pp, "idx", &data) == 0)
				vk[i].index = data;
			if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
				vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
				vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
			} else
				I(" range faile");
			i++;
		}
		pdata->virtual_key = vk;
		for (i = 0; i < cnt; i++) {
			pr_info("vk[%d]: idx=%d, x_min=%d, y_max=%d\n",
				i, pdata->virtual_key[i].index,
				pdata->virtual_key[i].x_range_min,
				pdata->virtual_key[i].y_range_max);
		}
	}
}

static int himax_parse_dt(struct himax_ts_data *ts,
	struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->client->dev.of_node;
	u32 data = 0;

	prop = of_find_property(dt, "himax,panel-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			pr_warn("Invalid panel coords size %d !\n", coords_size);
	}

	if (of_property_read_u32_array(dt, "himax,panel-coords",
		coords, coords_size) == 0) {
		pdata->abs_x_min = coords[0], pdata->abs_x_max = coords[1];
		pdata->abs_y_min = coords[2], pdata->abs_y_max = coords[3];
		pr_debug("panel-coords = (%d, %d), (%d, %d)\n",
			pdata->abs_x_min, pdata->abs_x_max,
			pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			pr_warn("Invalid panel coords size %d !\n", coords_size);
	}

	rc = of_property_read_u32_array(dt, "himax,display-coords",
		coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		pr_err("read prop 'himax,display-coords' failed !\n");
		return rc;
	}
	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	pr_debug("display-coords = (%d, %d)",
		pdata->screenWidth, pdata->screenHeight);

	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_irq))
		pr_warn("gpio_irq value is not valid\n");

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,reset-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_reset))
		pr_warn("gpio_rst value is not valid\n");

	pdata->gpio_pwr_en = of_get_named_gpio(dt, "himax,power_ldo-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_pwr_en))
		pr_warn("gpio_pwr_en value is not valid\n");

	pr_debug("gpio_irq=%d, gpio_rst=%d, gpio_pwr_en=%d\n",
		pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_pwr_en);

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		pr_info("use protocol type %c\n",
			data == PROTOCOL_TYPE_A ? 'A' : 'B');
	}

	himax_vk_parser(dt, pdata);
	return 0;
}
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_gesture_proc_read(struct file *file,
	char __user *buf, size_t size, loff_t *ppos)
{
	int cnt= 0;
	char *page = NULL;
	page = kzalloc(50, GFP_KERNEL);
	cnt = sprintf(page, "himax gesture open\n");
	cnt = simple_read_from_buffer(buf, size, ppos, page, cnt);
	kfree(page);
	return cnt;
}

static ssize_t himax_gesture_proc_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	char i;
	struct himax_ts_data *ts = private_ts;

	if (copy_from_user(&i, buf, 1)) {
		return -EFAULT;
	}

	if (atomic_read(&ts->suspend_mode) == 0) {
		if (i == '1')
			ts->smwake_enable = true;
		else if(i == '0')
			ts->smwake_enable = false;
		else
			return -EINVAL;
	}
	return size;
}
static const struct file_operations himax_gesture_proc_fops= {
	.read	= himax_gesture_proc_read,
	.write	= himax_gesture_proc_write,
};
#endif

static int himax852xes_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;
	struct himax_ts_data *ts;
	struct himax_i2c_platform_data *pdata;

	pr_debug("enter, addr=0x%x\n", (u32)client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c check functionality failed !\n");
		return -ENODEV;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		pr_err("allocate himax_ts_data failed !\n");
		err = -ENOMEM;
		goto err_check_functionality_failed;
	}

	i2c_set_clientdata(client, ts);
	ts->client = client;
	ts->dev = &client->dev;
	private_ts = ts;

	if (client->dev.of_node) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			err = -ENOMEM;
			goto err_alloc_ts_data_failed;
		}

		err = himax_parse_dt(ts, pdata);
		if (err < 0) {
			pr_err("parse dt failed !\n");
			goto err_dt_platform_data_fail;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("platform_data is null !\n");
			err = -ENOMEM;
			goto err_alloc_ts_data_failed;
		}
	}

	ts->rst_gpio = pdata->gpio_reset;

	err = himax_gpio_power_config(ts->client, pdata);
	if (err)
		goto err_dt_platform_data_fail;

	if (!himax_ic_package_check(ts)) {
		pr_err("Himax chip is not found !\n");
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

	himax_read_tp_info(client);

#ifdef HX_AUTO_UPDATE_FW
	himax_fw_update();
#endif

	if (himax_load_sensor_config(client, pdata) < 0) {
		pr_err("load sensor configuration failed !\n");
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	} else {
		pr_info("sensor configuration loaded\n");
	}

	calculate_point_number();

#ifdef HX_TP_SYS_DIAG
	setXChannel(HX_RX_NUM);
	setYChannel(HX_TX_NUM);

	setMutualBuffer();
	if (getMutualBuffer() == NULL) {
		pr_err("mutual buffer allocate failed !\n");
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	}
#ifdef HX_TP_SYS_2T2R
	if (Is_2T2R) {
		setXChannel_2(HX_RX_NUM_2);
		setYChannel_2(HX_TX_NUM_2);

		setMutualBuffer_2();

		if (getMutualBuffer_2() == NULL) {
			pr_err("mutual buffer 2 allocate failed !\n");
			return -1;
		}
	}
#endif
#endif

#ifdef CONFIG_OF
	ts->power = pdata->power;
#endif
	ts->pdata = pdata;

	ts->x_channel = HX_RX_NUM;
	ts->y_channel = HX_TX_NUM;
	ts->nFinger_support = HX_MAX_PT;
	calc_data_size(ts->nFinger_support);

#ifdef CONFIG_OF
	ts->pdata->abs_pressure_min        = 0;
	ts->pdata->abs_pressure_max        = 200;
	ts->pdata->abs_width_min           = 0;
	ts->pdata->abs_width_max           = 200;
	pdata->cable_config[0]             = 0x90;
	pdata->cable_config[1]             = 0x00;
#endif

	atomic_set(&ts->suspend_mode, 0);

	ts->protocol_type = pdata->protocol_type;

	err = himax_input_register(ts);
	if (err) {
		pr_err("register input device '%s' failed !\n",
			ts->input_dev->name);
		err = -EFAULT;
		goto err_ic_gpio_power_failed;
	}

#ifdef CONFIG_FB	
	ts->fb_notif.notifier_call = fb_notifier_callback;
	err = fb_register_client(&ts->fb_notif);
	if (err) {
		pr_err("register fb notifier failed %d !\n", err);
		goto err_input_register_device_failed;
	}
#endif

#ifdef HX_SMART_WAKEUP
	ts->smwake_enable = false;
	wake_lock_init(&ts->smwake_wake_lock, WAKE_LOCK_SUSPEND,
		HIMAX852XES_NAME);
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	err = himax_touch_sysfs_init();
	if (err)
		goto err_wakelock_init_failed;
#endif

#ifdef HX_SMART_WAKEUP
	gesture_proc = proc_create_data(HIMAX_GESTURE_PROC_FILE, 0666,
		NULL, &himax_gesture_proc_fops, NULL);
	if (IS_ERR_OR_NULL(gesture_proc))
		pr_err("create proc file %s failed !\n",
		HIMAX_GESTURE_PROC_FILE);
#endif

	if (client->irq) {
		err = himax_ts_register_interrupt(ts->client);
		if (err)
			goto err_sysfs_init_failed;
	} else {
		pr_err("client->irq not found, return !!\n");
		goto err_sysfs_init_failed;
	}

#ifdef HX_ESD_MONITOR
	INIT_DELAYED_WORK(&ts->esd_dwork, himax_esd_check_work);
	schedule_delayed_work(&ts->esd_dwork,
			msecs_to_jiffies(MSEC_PER_SEC*10));
#endif

	pr_notice("successfully !\n");
	return 0;

err_sysfs_init_failed:
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_sysfs_deinit();
err_wakelock_init_failed:
#endif

#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->smwake_wake_lock);
#endif

err_input_register_device_failed:
	input_unregister_device(ts->input_dev);
err_ic_gpio_power_failed:
	gpio_free(pdata->gpio_reset);
	gpio_free(pdata->gpio_pwr_en);
err_dt_platform_data_fail:
	kfree(pdata);
err_alloc_ts_data_failed:
	kfree(ts);
err_check_functionality_failed:
	return err;
}

static int himax852xes_remove(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_sysfs_deinit();
#endif

#ifdef CONFIG_FB
	if (fb_unregister_client(&ts->fb_notif))
		dev_err(&client->dev,
			"Error occurred while unregistering fb_notifier.\n");
#endif

#ifdef HX_ESD_MONITOR
	cancel_delayed_work_sync(&ts->esd_dwork);
#endif

	if (ts->protocol_type == PROTOCOL_TYPE_B)
		input_mt_destroy_slots(ts->input_dev);

	input_unregister_device(ts->input_dev);
#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->smwake_wake_lock);
#endif
	kfree(ts);

	return 0;
}

static int himax852xes_suspend(struct device *dev)
{
	int ret;
	uint8_t buf[2] = {0};

	struct himax_ts_data *ts = dev_get_drvdata(dev);

	if (atomic_read(&ts->suspend_mode)) {
		pr_info("already suspended, skip\n");
		return 0;
	}

#ifdef HX_ESD_MONITOR
	cancel_delayed_work_sync(&ts->esd_dwork);
#endif

	atomic_set(&ts->suspend_mode, 1);
#ifdef HX_SMART_WAKEUP
	if (ts->smwake_enable) {
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND = false;
		buf[0] = 0x8F;
		buf[1] = 0x20;
		himax_int_enable(ts->client->irq, 0);
		ret = i2c_himax_master_write(ts->client, buf, 2,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			pr_err("i2c write failed, addr=0x%x, reg=0x%x\n",
				ts->client->addr, (uint32_t)buf[0]);

		pr_warn("smart wakeup enabled, reject suspend\n");
		himax_int_enable(ts->client->irq, 1);
		return 0;
	}
#endif

	himax_int_enable(ts->client->irq, 0);

	/* Himax 852xes IC enter sleep mode */
	buf[0] = HX_CMD_TSSOFF;
	ret = i2c_himax_master_write(ts->client, buf, 1,
		HIMAX_I2C_RETRY_TIMES);
	if (ret < 0)
		pr_err("i2c write failed, reg=0x%x\n", HX_CMD_TSSOFF);
	msleep(40);

	buf[0] = HX_CMD_TSSLPIN;
	ret = i2c_himax_master_write(ts->client, buf, 1,
		HIMAX_I2C_RETRY_TIMES);
	if (ret < 0)
		pr_err("i2c write failed, reg=0x%x\n", HX_CMD_TSSLPIN);

	ts->pre_finger_mask = 0;
	pr_warn("suspend done\n");

	return 0;
}

static int himax852xes_resume(struct device *dev)
{
#ifdef HX_SMART_WAKEUP
	int ret;
	uint8_t buf[2] = {0};
#endif

	struct himax_ts_data *ts = dev_get_drvdata(dev);

	if (!atomic_read(&ts->suspend_mode)) {
		pr_info("already resumed, skip\n");
		return 0;
	}

	atomic_set(&ts->suspend_mode, 0);
#ifdef HX_SMART_WAKEUP
	if (ts->smwake_enable) {
		/* Sense Off */
		i2c_himax_write_command(ts->client, 0x82,
			HIMAX_I2C_RETRY_TIMES);
		msleep(40);
		/* Sleep in */
		i2c_himax_write_command(ts->client, 0x80,
			HIMAX_I2C_RETRY_TIMES);
		buf[0] = 0x8F;
		buf[1] = 0x00;
		ret = i2c_himax_master_write(ts->client, buf, 2,
			HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
			pr_err("i2c write failed, addr=0x%x, reg=0x%x\n",
				ts->client->addr, (uint32_t)buf[0]);

		msleep(50);
	}
#endif

	himax_chip_reset(false, false);

	/* Sense On */
	i2c_himax_write_command(ts->client, 0x83, HIMAX_I2C_RETRY_TIMES);
	msleep(30);
	i2c_himax_write_command(ts->client, 0x81, HIMAX_I2C_RETRY_TIMES);

	himax_int_enable(ts->client->irq,1);
	pr_warn("resume ok\n");

#ifdef HX_ESD_MONITOR
	schedule_delayed_work(&ts->esd_dwork,
		msecs_to_jiffies(HX_ESD_MONITOR_INTERVAL));
#endif

	return 0;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts=
		container_of(self, struct himax_ts_data, fb_notif);

	if ((event!=FB_EVENT_BLANK) && (event!=FB_EARLY_EVENT_BLANK) &&
		(event!=FB_R_EARLY_EVENT_BLANK))
		return 0;

	if (evdata && evdata->data && ts && ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case FB_BLANK_UNBLANK:
			if (event == FB_EVENT_BLANK)
				himax852xes_resume(&ts->client->dev);
			break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			if (event == FB_EARLY_EVENT_BLANK)
				himax852xes_suspend(&ts->client->dev);

			if (event == FB_R_EARLY_EVENT_BLANK)
				himax852xes_resume(&ts->client->dev);
			break;
		}
	}
	return 0;
}
#endif

static const struct i2c_device_id himax852xes_ts_id[] = {
	{HIMAX852XES_NAME, 0 },
	{}
};

#ifdef CONFIG_OF
static struct of_device_id himax_match_table[] = {
	{.compatible = "himax,852x" },
	{},
};
#else
#define himax_match_table NULL
#endif

static struct i2c_driver himax852xes_driver = {
	.id_table	= himax852xes_ts_id,
	.probe		= himax852xes_probe,
	.remove		= himax852xes_remove,
	.driver		= {
		.name = HIMAX852XES_NAME,
		.owner = THIS_MODULE,
		.of_match_table = himax_match_table,
	},
};

static void __init himax852xes_init_async(void *unused, async_cookie_t cookie)
{
	i2c_add_driver(&himax852xes_driver);
}

static int __init himax852xes_init(void)
{
	async_schedule(himax852xes_init_async, NULL);
	return 0;
}

static void __exit himax852xes_exit(void)
{
	i2c_del_driver(&himax852xes_driver);
}

module_init(himax852xes_init);
module_exit(himax852xes_exit);

MODULE_DESCRIPTION("Himax852xes driver");
MODULE_LICENSE("GPL");
