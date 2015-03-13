/* Himax Android Driver Sample Code Ver 0.3 for HMX852xES chipset
*
* Copyright (C) 2014-2015 Himax Corporation.
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

#include <linux/himax_852xES.h>
#include <linux/himax_platform.h>

#define HIMAX_I2C_RETRY_TIMES		10
#define SUPPORT_FINGER_DATA_CHECKSUM	0x0F
#define TS_WAKE_LOCK_TIMEOUT		(2 * HZ)

static int		HX_TOUCH_INFO_POINT_CNT;
static int		HX_RX_NUM;
static int		HX_TX_NUM;
static int		HX_BT_NUM;
static int		HX_X_RES;
static int		HX_Y_RES;
static int		HX_MAX_PT;
static unsigned int	FW_VER_MAJ_FLASH_ADDR;
static unsigned int 	FW_VER_MAJ_FLASH_LENG;
static unsigned int 	FW_VER_MIN_FLASH_ADDR;
static unsigned int 	FW_VER_MIN_FLASH_LENG;
static unsigned int 	CFG_VER_MAJ_FLASH_ADDR;
static unsigned int 	CFG_VER_MAJ_FLASH_LENG;
static unsigned int 	CFG_VER_MIN_FLASH_ADDR;
static unsigned int 	CFG_VER_MIN_FLASH_LENG;
static unsigned char	IC_CHECKSUM;
static unsigned char	IC_TYPE;
static bool HX_XY_REVERSE = false;
static bool HX_INT_IS_EDGE = false;

#ifdef HX_TP_SYS_DIAG
static int  touch_monitor_stop_flag 		= 0;
#endif

static uint8_t 	vk_press = 0x00;
static uint8_t 	AA_press = 0x00;
#ifdef HX_ESD_WORKAROUND
static uint8_t	IC_STATUS_CHECK	= 0xAA;
#endif
static uint8_t 	EN_NoiseFilter = 0x00;
static uint8_t	Last_EN_NoiseFilter = 0x00;
static int	hx_point_num	= 0;
static int	p_point_num	= 0xFFFF;
static int	tpd_key	   	= 0x00;
static int	tpd_key_old	= 0x00;

static bool	config_load		= false;
static struct himax_config *config_selected = NULL;

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
static struct kobject *android_touch_kobj = NULL;// Sys kobject variable
#endif

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h);
static void himax_ts_late_resume(struct early_suspend *h);
#endif

#ifdef HX_ESD_WORKAROUND
static int himax_hand_shaking(void)
{
	int ret;
	uint8_t hw_reset_check[2];
	uint8_t buf0[2];

	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	buf0[0] = 0xF2;
	if (IC_STATUS_CHECK == 0xAA) {
		buf0[1] = 0xAA;
		IC_STATUS_CHECK = 0x55;
	} else {
		buf0[1] = 0x55;
		IC_STATUS_CHECK = 0xAA;
	}

	ret = i2c_himax_master_write(private_ts->client, buf0, 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:write 0xF2 failed line: %d \n",__LINE__);
		return I2C_FAIL;
	}
	msleep(50);

	buf0[0] = 0xF2;
	buf0[1] = 0x00;
	ret = i2c_himax_master_write(private_ts->client, buf0, 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:write 0x92 failed line: %d \n",__LINE__);
		return I2C_FAIL;
	}
	msleep(2);

	ret = i2c_himax_read(private_ts->client, 0x90, &hw_reset_check[0], 1, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:i2c_himax_read 0x90 failed line: %d \n",__LINE__);
		return I2C_FAIL;
	}

	if ((IC_STATUS_CHECK != hw_reset_check[0])) {
		msleep(2);
		ret = i2c_himax_read(private_ts->client, 0x90, &hw_reset_check[1], 1, DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("[Himax]:i2c_himax_read 0x90 failed line: %d \n",__LINE__);
			return I2C_FAIL;
		}

		if (hw_reset_check[0] == hw_reset_check[1])
			return RESULT_STOP;
		else
			return RESULT_RUN;
	} else {
		return RESULT_RUN;
	}
}
#endif

static int himax_input_register(struct himax_ts_data *ts)
{
	int ret;
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
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
#if defined(HX_SMART_WAKEUP) || defined(HX_PALM_REPORT)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	//set_bit(KEY_APP_SWITCH, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	if (ts->protocol_type == PROTOCOL_TYPE_A) {
		//ts->input_dev->mtsize = ts->nFinger_support;
		input_set_abs_params(ts->input_dev,
					ABS_MT_TRACKING_ID,
					0, 3, 0, 0);
	} else {/* PROTOCOL_TYPE_B */
		set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
		input_mt_init_slots(ts->input_dev, ts->nFinger_support,0);
	}

	I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
		ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,ts->pdata->abs_y_min, ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,ts->pdata->abs_width_min, ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_AMPLITUDE, 0, ((ts->pdata->abs_pressure_max << 16) | ts->pdata->abs_width_max), 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_POSITION, 0, (BIT(31) | (ts->pdata->abs_x_max << 16) | ts->pdata->abs_y_max), 0, 0);

	return input_register_device(ts->input_dev);
}

static void calcDataSize(uint8_t finger_num)
{
	struct himax_ts_data *ts_data = private_ts;
	ts_data->coord_data_size = 4 * finger_num;
	ts_data->area_data_size = ((finger_num / 4) + (finger_num % 4 ? 1 : 0)) * 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size - ts_data->area_data_size - 4 - 4 - 1;
	ts_data->raw_data_nframes  = ((uint32_t)ts_data->x_channel * ts_data->y_channel +
	ts_data->x_channel + ts_data->y_channel) / ts_data->raw_data_frame_size +
		(((uint32_t)ts_data->x_channel * ts_data->y_channel +
			ts_data->x_channel + ts_data->y_channel) % ts_data->raw_data_frame_size)? 1 : 0;
	I("%s: coord_data_size: %d, area_data_size:%d, raw_data_frame_size:%d, raw_data_nframes:%d", __func__, ts_data->coord_data_size, ts_data->area_data_size, ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

static void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = HX_MAX_PT * 4 ;

	if ( (HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (HX_MAX_PT / 4) * 4 ;
	else
		HX_TOUCH_INFO_POINT_CNT += ((HX_MAX_PT / 4) +1) * 4 ;
}

void himax_touch_information(void)
{
	char data[12] = {0};
	int ret = 0;

	I("%s:IC_TYPE =%d\n", __func__,IC_TYPE);

	if (IC_TYPE == HX_85XX_ES_SERIES_PWON) {
		data[0] = 0x8C;
		data[1] = 0x14;
		ret = i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x70;
		ret = i2c_himax_master_write(private_ts->client, &data[0],3,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		ret = i2c_himax_read(private_ts->client, 0x5A, data, 12, DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		HX_RX_NUM = data[0];				 // FE(70)
		HX_TX_NUM = data[1];				 // FE(71)
		HX_MAX_PT = (data[2] & 0xF0) >> 4; // FE(72)
#ifdef HX_EN_SEL_BUTTON
		HX_BT_NUM = (data[2] & 0x0F); //FE(72)
#endif
		if((data[4] & 0x04) == 0x04) {//FE(74)
			HX_XY_REVERSE = true;
			HX_Y_RES = data[6]*256 + data[7]; //FE(76),FE(77)
			HX_X_RES = data[8]*256 + data[9]; //FE(78),FE(79)
		} else {
			HX_XY_REVERSE = false;
			HX_X_RES = data[6]*256 + data[7]; //FE(76),FE(77)
			HX_Y_RES = data[8]*256 + data[9]; //FE(78),FE(79)
		}
		data[0] = 0x8C;
		data[1] = 0x00;
		ret = i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
#ifdef HX_EN_MUT_BUTTON
		data[0] = 0x8C;
		data[1] = 0x14;
		ret = i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x64;
		ret = i2c_himax_master_write(private_ts->client, &data[0],3,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		ret = i2c_himax_read(private_ts->client, 0x5A, data, 4, DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		HX_BT_NUM = (data[0] & 0x03);
		data[0] = 0x8C;
		data[1] = 0x00;
		ret = i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
#endif
		data[0] = 0x8C;
		data[1] = 0x14;
		ret = i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x02;
		ret = i2c_himax_master_write(private_ts->client, &data[0],3,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		ret = i2c_himax_read(private_ts->client, 0x5A, data, 10, DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		if((data[1] & 0x01) == 1)//FE(02)
			HX_INT_IS_EDGE = true;
		else
			HX_INT_IS_EDGE = false;
		
		data[0] = 0x8C;
		data[1] = 0x00;
		ret = i2c_himax_master_write(private_ts->client, &data[0],2,DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}
		msleep(10);
		I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d \n", __func__,HX_RX_NUM,HX_TX_NUM,HX_MAX_PT);
	} else {
		HX_RX_NUM		= 0;
		HX_TX_NUM		= 0;
		HX_BT_NUM		= 0;
		HX_X_RES		= 0;
		HX_Y_RES		= 0;
		HX_MAX_PT		= 0;
		HX_XY_REVERSE		= false;
		HX_INT_IS_EDGE		= false;
	}
}
static int himax_read_Sensor_ID(struct i2c_client *client)
{
	uint8_t val_high[1], val_low[1], ID0=0, ID1=0;
	char data[3];
	const int normalRetry = 10;
	int sensor_id;
	int ret = 0;

	data[0] = 0x56; data[1] = 0x02; data[2] = 0x02;/*ID pin PULL High*/
	ret = i2c_himax_master_write(client, &data[0],3,normalRetry);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ret;
	}
	msleep(1);

	//read id pin high
	ret = i2c_himax_read(client, 0x57, val_high, 1, normalRetry);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ret;
	}
	data[0] = 0x56; data[1] = 0x01; data[2] = 0x01;/*ID pin PULL Low*/
	ret = i2c_himax_master_write(client, &data[0],3,normalRetry);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ret;
	}
	msleep(1);

	//read id pin low
	ret = i2c_himax_read(client, 0x57, val_low, 1, normalRetry);
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ret;
	}

	if((val_high[0] & 0x01) ==0)
		ID0=0x02;/*GND*/
	else if((val_low[0] & 0x01) ==0)
		ID0=0x01;/*Floating*/
	else
		ID0=0x04;/*VCC*/

	if((val_high[0] & 0x02) ==0)
		ID1=0x02;/*GND*/
	else if((val_low[0] & 0x02) ==0)
		ID1=0x01;/*Floating*/
	else
		ID1=0x04;/*VCC*/
	if((ID0==0x04)&&(ID1!=0x04))
		{
			data[0] = 0x56; data[1] = 0x02; data[2] = 0x01;/*ID pin PULL High,Low*/
			ret = i2c_himax_master_write(client, &data[0],3,normalRetry);
			if (ret < 0) {
				E("%s: i2c access fail!\n", __func__);
				return ret;
			}
			msleep(1);

		}
	else if((ID0!=0x04)&&(ID1==0x04))
		{
			data[0] = 0x56; data[1] = 0x01; data[2] = 0x02;/*ID pin PULL Low,High*/
			ret = i2c_himax_master_write(client, &data[0],3,normalRetry);
			if (ret < 0) {
				E("%s: i2c access fail!\n", __func__);
				return ret;
			}
			msleep(1);

		}
	else if((ID0==0x04)&&(ID1==0x04))
		{
			data[0] = 0x56; data[1] = 0x02; data[2] = 0x02;/*ID pin PULL High,High*/
			ret = i2c_himax_master_write(client, &data[0],3,normalRetry);
			if (ret < 0) {
				E("%s: i2c access fail!\n", __func__);
				return ret;
			}
			msleep(1);

		}
	sensor_id=(ID1<<4)|ID0;

	data[0] = 0xF3; data[1] = sensor_id;
	ret = i2c_himax_master_write(client, &data[0],2,normalRetry);/*Write to MCU*/
	if (ret < 0) {
		E("%s: i2c access fail!\n", __func__);
		return ret;
	}
	msleep(1);

	return sensor_id;

}
static void himax_power_on_initCMD(struct i2c_client *client)
{
	const int normalRetry = 10;

	I("%s:\n", __func__);
	//Sense on to update the information
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

static int himax_loadSensorConfig(struct i2c_client *client, struct himax_i2c_platform_data *pdata)
{
#ifdef HX_ESD_WORKAROUND
	char data[12] = {0};
#endif

	if (!client) {
		E("%s: Necessary parameters client are null!\n", __func__);
		return -1;
	}

	if(config_load == false)
		{
			config_selected = kzalloc(sizeof(*config_selected), GFP_KERNEL);
			if (config_selected == NULL) {
				E("%s: alloc config_selected fail!\n", __func__);
				return -1;
			}
		}
#ifndef CONFIG_OF
	pdata = client->dev.platform_data;
		if (!pdata) {
			E("%s: Necessary parameters pdata are null!\n", __func__);
			return -1;
		}
#endif
#ifdef HX_ESD_WORKAROUND
	//Check R36 to check IC Status
	i2c_himax_read(client, 0x36, data, 2, 10);
	if(data[0] != 0x0F || data[1] != 0x53)
	{
		//IC is abnormal
		E("[HimaxError] %s R36 Fail : R36[0]=%d,R36[1]=%d,R36 Counter=%d \n",__func__,data[0],data[1],ESD_R36_FAIL);
		return -EPERM;
	}
#endif

	himax_power_on_initCMD(client);

	I("%s: initialization complete\n", __func__);

	return 1;
}

#ifdef HX_RST_PIN_FUNC
void himax_HW_reset(uint8_t loadconfig,uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;
	if (ts->rst_gpio) {
		if(int_off)
			{
				if (ts->use_irq)
					himax_int_enable(private_ts->client->irq,0);
				else {
					hrtimer_cancel(&ts->timer);
					ret = cancel_work_sync(&ts->work);
				}
			}

		I("%s: Now reset the Touch chip.\n", __func__);

		himax_rst_gpio_set(ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(ts->rst_gpio, 1);
		msleep(20);

		if(loadconfig)
			himax_loadSensorConfig(private_ts->client,private_ts->pdata);

		if(int_off)
			{
				if (ts->use_irq)
					himax_int_enable(private_ts->client->irq,1);
				else
					hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
			}
	}
}
#endif

#ifdef HX_TP_SYS_DEBUG
static u8 himax_read_FW_ver(bool hw_reset)
{
	uint8_t cmd[3];

	himax_int_enable(private_ts->client->irq,0);

	#ifdef HX_RST_PIN_FUNC
	if (hw_reset) {
		himax_HW_reset(false,false);
	}
	#endif

	msleep(120);
	if (i2c_himax_read(private_ts->client, 0x33, cmd, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	private_ts->vendor_fw_ver = cmd[0];
	I("FW_VER : %d \n",cmd[0]);

	if (i2c_himax_read(private_ts->client, 0x39, cmd, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	private_ts->vendor_config_ver = cmd[0];
	I("CFG_VER : %d \n",cmd[0]);

	#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true,false);
	#endif

	himax_int_enable(private_ts->client->irq,1);

	return 0;
}
#endif

static bool himax_ic_package_check(struct himax_ts_data *ts)
{
	uint8_t cmd[3];
	uint8_t data[3];

	memset(cmd, 0x00, sizeof(cmd));
	memset(data, 0x00, sizeof(data));

	if (i2c_himax_read(ts->client, 0xD1, cmd, 3, DEFAULT_RETRY_CNT) < 0)
		return false ;

	if (i2c_himax_read(ts->client, 0x31, data, 3, DEFAULT_RETRY_CNT) < 0)
		return false;

	if((data[0] == 0x85 && data[1] == 0x30) || (cmd[0] == 0x05 && cmd[1] == 0x85 && cmd[2] == 0x29))
		{
			IC_TYPE 		= HX_85XX_E_SERIES_PWON;
			IC_CHECKSUM = HX_TP_BIN_CHECKSUM_CRC;
			//Himax: Set FW and CFG Flash Address
			FW_VER_MAJ_FLASH_ADDR	= 133;	//0x0085
			FW_VER_MAJ_FLASH_LENG	= 1;;
			FW_VER_MIN_FLASH_ADDR	= 134;  //0x0086
			FW_VER_MIN_FLASH_LENG	= 1;
			CFG_VER_MAJ_FLASH_ADDR = 160;	//0x00A0
			CFG_VER_MAJ_FLASH_LENG = 12;
			CFG_VER_MIN_FLASH_ADDR = 172;	//0x00AC
			CFG_VER_MIN_FLASH_LENG = 12;
			I("Himax IC package 852x E\n");
		}
	else if((data[0] == 0x85 && data[1] == 0x31))
		{
			IC_TYPE         = HX_85XX_ES_SERIES_PWON;
			IC_CHECKSUM 		= HX_TP_BIN_CHECKSUM_CRC;
			//Himax: Set FW and CFG Flash Address
			FW_VER_MAJ_FLASH_ADDR   = 133;  //0x0085
			FW_VER_MAJ_FLASH_LENG   = 1;;
			FW_VER_MIN_FLASH_ADDR   = 134;  //0x0086
			FW_VER_MIN_FLASH_LENG   = 1;
			CFG_VER_MAJ_FLASH_ADDR 	= 160;   //0x00A0
			CFG_VER_MAJ_FLASH_LENG 	= 12;
			CFG_VER_MIN_FLASH_ADDR 	= 172;   //0x00AC
			CFG_VER_MIN_FLASH_LENG 	= 12;
			I("Himax IC package 852x ES\n");
			}
	else if ((data[0] == 0x85 && data[1] == 0x28) || (cmd[0] == 0x04 && cmd[1] == 0x85 &&
		(cmd[2] == 0x26 || cmd[2] == 0x27 || cmd[2] == 0x28))) {
		IC_TYPE                = HX_85XX_D_SERIES_PWON;
		IC_CHECKSUM            = HX_TP_BIN_CHECKSUM_CRC;
		//Himax: Set FW and CFG Flash Address
		FW_VER_MAJ_FLASH_ADDR  = 133;                    // 0x0085
		FW_VER_MAJ_FLASH_LENG  = 1;;
		FW_VER_MIN_FLASH_ADDR  = 134;                    // 0x0086
		FW_VER_MIN_FLASH_LENG  = 1;
		CFG_VER_MAJ_FLASH_ADDR = 160;                    // 0x00A0
		CFG_VER_MAJ_FLASH_LENG = 12;
		CFG_VER_MIN_FLASH_ADDR = 172;                    // 0x00AC
		CFG_VER_MIN_FLASH_LENG = 12;
		I("Himax IC package 852x D\n");
	} else if ((data[0] == 0x85 && data[1] == 0x23) || (cmd[0] == 0x03 && cmd[1] == 0x85 &&
			(cmd[2] == 0x26 || cmd[2] == 0x27 || cmd[2] == 0x28 || cmd[2] == 0x29))) {
		IC_TYPE                = HX_85XX_C_SERIES_PWON;
		IC_CHECKSUM            = HX_TP_BIN_CHECKSUM_SW;
		//Himax: Set FW and CFG Flash Address
		FW_VER_MAJ_FLASH_ADDR  = 133;                   // 0x0085
		FW_VER_MAJ_FLASH_LENG  = 1;
		FW_VER_MIN_FLASH_ADDR  = 134;                   // 0x0086
		FW_VER_MIN_FLASH_LENG  = 1;
		CFG_VER_MAJ_FLASH_ADDR = 135;                   // 0x0087
		CFG_VER_MAJ_FLASH_LENG = 12;
		CFG_VER_MIN_FLASH_ADDR = 147;                   // 0x0093
		CFG_VER_MIN_FLASH_LENG = 12;
		I("Himax IC package 852x C\n");
	} else if ((data[0] == 0x85 && data[1] == 0x26) ||
		   (cmd[0] == 0x02 && cmd[1] == 0x85 &&
		   (cmd[2] == 0x19 || cmd[2] == 0x25 || cmd[2] == 0x26))) {
		IC_TYPE                = HX_85XX_B_SERIES_PWON;
		IC_CHECKSUM            = HX_TP_BIN_CHECKSUM_SW;
		//Himax: Set FW and CFG Flash Address
		FW_VER_MAJ_FLASH_ADDR  = 133;                   // 0x0085
		FW_VER_MAJ_FLASH_LENG  = 1;
		FW_VER_MIN_FLASH_ADDR  = 728;                   // 0x02D8
		FW_VER_MIN_FLASH_LENG  = 1;
		CFG_VER_MAJ_FLASH_ADDR = 692;                   // 0x02B4
		CFG_VER_MAJ_FLASH_LENG = 3;
		CFG_VER_MIN_FLASH_ADDR = 704;                   // 0x02C0
		CFG_VER_MIN_FLASH_LENG = 3;
		I("Himax IC package 852x B\n");
	} else if ((data[0] == 0x85 && data[1] == 0x20) || (cmd[0] == 0x01 &&
			cmd[1] == 0x85 && cmd[2] == 0x19)) {
		IC_TYPE     = HX_85XX_A_SERIES_PWON;
		IC_CHECKSUM = HX_TP_BIN_CHECKSUM_SW;
		I("Himax IC package 852x A\n");
	} else {
		E("Himax IC package incorrect!!\n");
	}
	return true;
}

static void himax_read_TP_info(struct i2c_client *client)
{
	char data[12] = {0};

	//read fw version
	if (i2c_himax_read(client, 0x33, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	private_ts->vendor_fw_ver = data[0];

	//read config version
	if (i2c_himax_read(client, 0x39, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	private_ts->vendor_config_ver = data[0];

	//read sensor ID
	private_ts->vendor_sensor_id = himax_read_Sensor_ID(client);

	I("sensor_id=%x.\n",private_ts->vendor_sensor_id);
	I("fw_ver=%x.\n",private_ts->vendor_fw_ver);
}

#ifdef HX_ESD_WORKAROUND
	void ESD_HW_REST(void)
	{
		ESD_RESET_ACTIVATE = 1;
		ESD_COUNTER = 0;
		ESD_R36_FAIL = 0;
#ifdef HX_CHIP_STATUS_MONITOR
		HX_CHIP_POLLING_COUNT=0;
#endif
		I("START_Himax TP: ESD - Reset\n");

		while(ESD_R36_FAIL <=3 )
		{

		himax_rst_gpio_set(private_ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(private_ts->rst_gpio, 1);
		msleep(20);

		if(himax_loadSensorConfig(private_ts->client, private_ts->pdata)<0)
			ESD_R36_FAIL++;
		else
			break;
		}
		I("END_Himax TP: ESD - Reset\n");
	}
#endif

#ifdef HX_CHIP_STATUS_MONITOR
static void himax_chip_monitor_function(struct work_struct *work) //for ESD solution
{
	int ret=0;

	I(" %s: POLLING_COUNT=%x, STATUS=%x \n", __func__,HX_CHIP_POLLING_COUNT,ret);
	if(HX_CHIP_POLLING_COUNT >= (HX_POLLING_TIMES-1))
	{
		HX_ON_HAND_SHAKING=1;
		ret = himax_hand_shaking();
		HX_ON_HAND_SHAKING=0;
		if(ret == 2)
		{
			I(" %s: I2C Fail \n", __func__);
			ESD_HW_REST();
		}
		else if(ret == 1)
		{
			I(" %s: MCU Stop \n", __func__);
			ESD_HW_REST();
		}
		HX_CHIP_POLLING_COUNT=0;
	}
	else
		HX_CHIP_POLLING_COUNT++;

	queue_delayed_work(private_ts->himax_chip_monitor_wq, &private_ts->himax_chip_monitor, HX_POLLING_TIMER*HZ);

	return;
}
#endif

#ifdef HX_DOT_VIEW
static void himax_set_cover_func(unsigned int enable)
{
	uint8_t cmd[4];

	if (enable)
		cmd[0] = 0x40;
	else
		cmd[0] = 0x00;

	return;
}

static int hallsensor_hover_status_handler_func(struct notifier_block *this,
	unsigned long status, void *unused)
{
	int pole = 0, pole_value = 0;
	struct himax_ts_data *ts = private_ts;

	pole_value = 0x1 & status;
	pole = (0x2 & status) >> 1;
	I("[HL] %s[%s]\n", pole? "att_s" : "att_n", pole_value ? "Near" : "Far");

	if (pole == 1) {
		if (pole_value == 0)
			ts->cover_enable = 0;
		else{
				ts->cover_enable = 1;
			}

		himax_set_cover_func(ts->cover_enable);
		I("[HL] %s: cover_enable = %d.\n", __func__, ts->cover_enable);
	}

	return NOTIFY_OK;
}

static struct notifier_block hallsensor_status_handler = {
	.notifier_call = hallsensor_hover_status_handler_func,
};
#endif

#ifdef HX_SMART_WAKEUP
static int himax_parse_wake_event(struct himax_ts_data *ts)
{
	uint8_t buf[5];
	if (i2c_himax_read(ts->client, 0x86, buf, 4,HIMAX_I2C_RETRY_TIMES))
		E("%s: can't read data from chip!\n", __func__);

	if((buf[0]==0x57)&&(buf[1]==0x61)&&(buf[2]==0x6B)&&(buf[3]==0x65))
		{
			I("%s: WAKE UP system!\n", __func__);
			return 1;//Yes, wake up system
		}
	else
		{
			I("%s: NOT WKAE packet, SKIP!\n", __func__);
			I("buf[0]=%x, buf[1]=%x, buf[2]=%x, buf[3]=%x\n",buf[0],buf[1],buf[2],buf[3]);
			return 0;
		}
}
#endif

inline static void himax_ts_report_abs(struct input_dev *input,
			uint16_t x, uint16_t y) {
	input_report_abs(input, ABS_MT_TOUCH_MAJOR, 100);
	input_report_abs(input, ABS_MT_WIDTH_MAJOR, 100);
	input_report_abs(input, ABS_MT_PRESSURE, 100);
	input_report_abs(input, ABS_MT_POSITION_X, x);
	input_report_abs(input, ABS_MT_POSITION_Y, y);
}

static void himax_ts_button_func(int tp_key_index,struct himax_ts_data *ts)
{
	uint16_t x_position = 0, y_position = 0;
if ( tp_key_index != 0x00)
	{
		I("virtual key index =%x\n",tp_key_index);
		if ( tp_key_index == 0x01) {
			vk_press = 1;
			I("back key pressed\n");
				if (ts->pdata->virtual_key)
				{
					if (ts->button[0].index) {
						x_position = (ts->button[0].x_range_min + ts->button[0].x_range_max) / 2;
						y_position = (ts->button[0].y_range_min + ts->button[0].y_range_max) / 2;
					}
					if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
						himax_ts_report_abs(ts->input_dev, x_position, y_position);
						input_mt_sync(ts->input_dev);
					} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
						1);
						himax_ts_report_abs(ts->input_dev, x_position, y_position);
					}
				}
				else
					input_report_key(ts->input_dev, KEY_BACK, 1);
		}
		else if ( tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");
				if (ts->pdata->virtual_key)
				{
					if (ts->button[1].index) {
						x_position = (ts->button[1].x_range_min + ts->button[1].x_range_max) / 2;
						y_position = (ts->button[1].y_range_min + ts->button[1].y_range_max) / 2;
					}
						if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
						himax_ts_report_abs(ts->input_dev, x_position, y_position);
						input_mt_sync(ts->input_dev);
					} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
						1);
						himax_ts_report_abs(ts->input_dev, x_position, y_position);
					}
				}
				else
					input_report_key(ts->input_dev, KEY_HOME, 1);
		}
		else if ( tp_key_index == 0x04) {
			vk_press = 1;
			I("APP_switch key pressed\n");
				if (ts->pdata->virtual_key)
				{
					if (ts->button[2].index) {
						x_position = (ts->button[2].x_range_min + ts->button[2].x_range_max) / 2;
						y_position = (ts->button[2].y_range_min + ts->button[2].y_range_max) / 2;
					}
						if (ts->protocol_type == PROTOCOL_TYPE_A) {
						input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, 0);
						himax_ts_report_abs(ts->input_dev, x_position, y_position);
						input_mt_sync(ts->input_dev);
					} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
						1);
						himax_ts_report_abs(ts->input_dev, x_position, y_position);
					}
				}
		}
		input_sync(ts->input_dev);
	}
else/*tp_key_index =0x00*/
	{
		I("virtual key released\n");
		vk_press = 0;
		if (ts->protocol_type == PROTOCOL_TYPE_A) {
			input_mt_sync(ts->input_dev);
		}
		else if (ts->protocol_type == PROTOCOL_TYPE_B) {
			input_mt_slot(ts->input_dev, 0);
			input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
		}
		input_report_key(ts->input_dev, KEY_BACK, 0);
		input_report_key(ts->input_dev, KEY_HOME, 0);
	input_sync(ts->input_dev);
	}
}

inline static void himax_ts_work(struct himax_ts_data *ts)
{
#ifdef HX_TP_SYS_DIAG
	int ret = 0;
#endif
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

#ifdef HX_TP_SYS_DIAG
	uint8_t *mutual_data;
	uint8_t *self_data;
	uint8_t diag_cmd;
	int  	i;
	int 	mul_num;
	int 	self_num;
	int 	index = 0;
	int  	temp1, temp2;
	//coordinate dump start
	char coordinate_char[15+(HX_MAX_PT+5)*2*5+2];
	struct timeval t;
	struct tm broken;
	//coordinate dump end
#endif
#ifdef HX_CHIP_STATUS_MONITOR
		int j=0;
#endif

	memset(buf, 0x00, sizeof(buf));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

#ifdef HX_CHIP_STATUS_MONITOR
		HX_CHIP_POLLING_COUNT=0;
		if(HX_ON_HAND_SHAKING)//chip on hand shaking,wait hand shaking
		{
			for(j=0; j<100; j++)
				{
					if(HX_ON_HAND_SHAKING==0)//chip on hand shaking end
						{
							I("%s:HX_ON_HAND_SHAKING OK check %d times\n",__func__,j);
							break;
						}
					else
						msleep(1);
				}
			if(j==100)
				{
					E("%s:HX_ON_HAND_SHAKING timeout reject interrupt\n",__func__);
					return;
				}
		}
#endif
	raw_cnt_max = HX_MAX_PT/4;
	raw_cnt_rmd = HX_MAX_PT%4;

	if (raw_cnt_rmd != 0x00) //more than 4 fingers
	{
		RawDataLen = 128 - ((HX_MAX_PT+raw_cnt_max+3)*4) - 1;
		hx_touch_info_size = (HX_MAX_PT+raw_cnt_max+2)*4;
	}
	else //less than 4 fingers
	{
		RawDataLen = 128 - ((HX_MAX_PT+raw_cnt_max+2)*4) - 1;
		hx_touch_info_size = (HX_MAX_PT+raw_cnt_max+1)*4;
	}

#ifdef HX_TP_SYS_DIAG
	diag_cmd = getDiagCommand();
	if( diag_cmd ){
		ret = i2c_himax_read(ts->client, 0x86, buf, 128,HIMAX_I2C_RETRY_TIMES);
	}
	else{
		if(touch_monitor_stop_flag != 0){
			ret = i2c_himax_read(ts->client, 0x86, buf, 128,HIMAX_I2C_RETRY_TIMES);
			touch_monitor_stop_flag-- ;
		}
		else{
			ret = i2c_himax_read(ts->client, 0x86, buf, hx_touch_info_size,HIMAX_I2C_RETRY_TIMES);
		}
	}
	if (ret)
#else
	if (i2c_himax_read(ts->client, 0x86, buf, hx_touch_info_size,HIMAX_I2C_RETRY_TIMES))
#endif
	{
		E("%s: can't read data from chip!\n", __func__);
		goto err_workqueue_out;
	}
	else
	{
#ifdef HX_ESD_WORKAROUND
			for(i = 0; i < hx_touch_info_size; i++)
			{
				if (buf[i] == 0x00) //case 2 ESD recovery flow-Disable
				{
					check_sum_cal = 1;
				}
				else if(buf[i] == 0xED)/*case 1 ESD recovery flow*/
				{
					check_sum_cal = 2;
				}
				else
				{
					check_sum_cal = 0;
					i = hx_touch_info_size;
				}
			}

			//IC status is abnormal ,do hand shaking
#ifdef HX_TP_SYS_DIAG
			diag_cmd = getDiagCommand();
#ifdef HX_ESD_WORKAROUND
			if (check_sum_cal != 0 && ESD_RESET_ACTIVATE == 0 && diag_cmd == 0)  //ESD Check
#else
			if (check_sum_cal != 0 && diag_cmd == 0)
#endif
#else
#ifdef HX_ESD_WORKAROUND
			if (check_sum_cal != 0 && ESD_RESET_ACTIVATE == 0 )  //ESD Check
#else
			if (check_sum_cal !=0)
#endif
#endif
			{
				ret = himax_hand_shaking();
				if (ret == I2C_FAIL)
					goto err_workqueue_out;

				else if ((ret == RESULT_STOP) && (check_sum_cal == 1)) {
					I("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
					ESD_HW_REST();
				} else if (check_sum_cal == 2) {
					I("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");
					ESD_HW_REST();
				}

				//himax_int_enable(ts->client->irq,1);
				return;
			}
			else if (ESD_RESET_ACTIVATE)
			{
				ESD_RESET_ACTIVATE = 0;/*drop 1st interrupts after chip reset*/
				I("[HIMAX TP MSG]:%s: Back from reset, ready to serve.\n", __func__);
				return;
			}
#endif

		for (loop_i = 0, check_sum_cal = 0; loop_i < hx_touch_info_size; loop_i++)
			check_sum_cal += buf[loop_i];

		if ((check_sum_cal != 0x00) )
		{
			I("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n", check_sum_cal);
			return;
		}
	}

	if (ts->debug_log_level & BIT(0)) {
		I("%s: raw data:\n", __func__);
		for (loop_i = 0; loop_i < hx_touch_info_size; loop_i++) {
			I("0x%2.2X ", buf[loop_i]);
			if (loop_i % 8 == 7)
				I("\n");
		}
	}

	//touch monitor raw data fetch
#ifdef HX_TP_SYS_DIAG
	diag_cmd = getDiagCommand();
	if (diag_cmd >= 1 && diag_cmd <= 6)
	{
		//Check 128th byte CRC
		for (i = hx_touch_info_size, check_sum_cal = 0; i < 128; i++)
		{
			check_sum_cal += buf[i];
		}
		if (check_sum_cal % 0x100 != 0)
		{
			goto bypass_checksum_failed_packet;
		}
		{
			mutual_data = getMutualBuffer();
			self_data 	= getSelfBuffer();

			// initiallize the block number of mutual and self
			mul_num = getXChannel() * getYChannel();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel() + getYChannel() + HX_BT_NUM;
#else
			self_num = getXChannel() + getYChannel();
#endif
		}

		//Himax: Check Raw-Data Header
		if (buf[hx_touch_info_size] == buf[hx_touch_info_size+1] && buf[hx_touch_info_size+1] == buf[hx_touch_info_size+2]
		&& buf[hx_touch_info_size+2] == buf[hx_touch_info_size+3] && buf[hx_touch_info_size] > 0)
		{
			index = (buf[hx_touch_info_size] - 1) * RawDataLen;
			//I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
			for (i = 0; i < RawDataLen; i++)
			{
				temp1 = index + i;

				if (temp1 < mul_num)
				{ //mutual
					mutual_data[index + i] = buf[i + hx_touch_info_size+4];	//4: RawData Header
				}
				else
				{//self
					temp1 = i + index;
					temp2 = self_num + mul_num;

					if (temp1 >= temp2)
					{
						break;
					}

					self_data[i+index-mul_num] = buf[i + hx_touch_info_size+4];	//4: RawData Header					
				}
			}
		}
		else
		{
			I("[HIMAX TP MSG]%s: header format is wrong!\n", __func__);
		}
	}
	else if (diag_cmd == 7)
	{
		memcpy(&(diag_coor[0]), &buf[0], 128);
	}
	//coordinate dump start
	if (coordinate_dump_enable == 1)
	{
		for(i=0; i<(15 + (HX_MAX_PT+5)*2*5); i++)
		{
			coordinate_char[i] = 0x20;
		}
		coordinate_char[15 + (HX_MAX_PT+5)*2*5] = 0xD;
		coordinate_char[15 + (HX_MAX_PT+5)*2*5 + 1] = 0xA;
	}
	//coordinate dump end
bypass_checksum_failed_packet:
#endif
		EN_NoiseFilter = (buf[HX_TOUCH_INFO_POINT_CNT+2]>>3);
		EN_NoiseFilter = EN_NoiseFilter & 0x01;

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
		tpd_key = (buf[HX_TOUCH_INFO_POINT_CNT+2]>>4);
		if (tpd_key == 0x0F)/*All (VK+AA)leave*/
		{
			tpd_key = 0x00;
		}
#else
		tpd_key = 0x00;
#endif

		p_point_num = hx_point_num;

		if (buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
			hx_point_num = 0;
		else
			hx_point_num= buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;

		// Touch Point information
		if (hx_point_num != 0 ) {
			if(vk_press == 0x00)
				{
					uint16_t old_finger = ts->pre_finger_mask;
					finger_num = buf[coordInfoSize - 4] & 0x0F;
					finger_pressed = buf[coordInfoSize - 2] << 8 | buf[coordInfoSize - 3];
					finger_on = 1;
					AA_press = 1;
					for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
						if (((finger_pressed >> loop_i) & 1) == 1) {
							base = loop_i * 4;
							x = buf[base] << 8 | buf[base + 1];
							y = (buf[base + 2] << 8 | buf[base + 3]);
							w = buf[(ts->nFinger_support * 4) + loop_i];
							finger_num--;

							if ((ts->debug_log_level & BIT(3)) > 0)
							{
								if ((((old_finger >> loop_i) ^ (finger_pressed >> loop_i)) & 1) == 1)
								{
									if (ts->useScreenRes)
									{
										I("status:%X, Screen:F:%02d Down, X:%d, Y:%d, W:%d, N:%d\n",
										finger_pressed, loop_i+1, x * ts->widthFactor >> SHIFTBITS,
										y * ts->heightFactor >> SHIFTBITS, w, EN_NoiseFilter);
									}
									else
									{
										I("status:%X, Raw:F:%02d Down, X:%d, Y:%d, W:%d, N:%d\n",
										finger_pressed, loop_i+1, x, y, w, EN_NoiseFilter);
									}
								}
							}

							if (ts->protocol_type == PROTOCOL_TYPE_B)
							{
								input_mt_slot(ts->input_dev, loop_i);
							}

							input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
							input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
							input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
							input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
							input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);

							if (ts->protocol_type == PROTOCOL_TYPE_A)
							{
								input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, loop_i);
								input_mt_sync(ts->input_dev);
							}
							else
							{
								ts->last_slot = loop_i;
								input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
							}

							if (!ts->first_pressed)
							{
								ts->first_pressed = 1;
								I("S1@%d, %d\n", x, y);
							}

							ts->pre_finger_data[loop_i][0] = x;
							ts->pre_finger_data[loop_i][1] = y;


							if (ts->debug_log_level & BIT(1))
								I("Finger %d=> X:%d, Y:%d W:%d, Z:%d, F:%d, N:%d\n",
									loop_i + 1, x, y, w, w, loop_i + 1, EN_NoiseFilter);						
						} else {
							if (ts->protocol_type == PROTOCOL_TYPE_B)
							{
								input_mt_slot(ts->input_dev, loop_i);
								input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
							}

							if (loop_i == 0 && ts->first_pressed == 1)
							{
								ts->first_pressed = 2;
								I("E1@%d, %d\n",
								ts->pre_finger_data[0][0] , ts->pre_finger_data[0][1]);
							}
							if ((ts->debug_log_level & BIT(3)) > 0)
							{
								if ((((old_finger >> loop_i) ^ (finger_pressed >> loop_i)) & 1) == 1)
								{
									if (ts->useScreenRes)
									{
										I("status:%X, Screen:F:%02d Up, X:%d, Y:%d, N:%d\n",
										finger_pressed, loop_i+1, ts->pre_finger_data[loop_i][0] * ts->widthFactor >> SHIFTBITS,
										ts->pre_finger_data[loop_i][1] * ts->heightFactor >> SHIFTBITS, Last_EN_NoiseFilter);
									}
									else
									{
										I("status:%X, Raw:F:%02d Up, X:%d, Y:%d, N:%d\n",
										finger_pressed, loop_i+1, ts->pre_finger_data[loop_i][0],
										ts->pre_finger_data[loop_i][1], Last_EN_NoiseFilter);
									}
								}
							}
						}
					}
					ts->pre_finger_mask = finger_pressed;
				}else if ((tpd_key_old != 0x00)&&(tpd_key == 0x00)) {
					//temp_x[0] = 0xFFFF;
					//temp_y[0] = 0xFFFF;
					//temp_x[1] = 0xFFFF;
					//temp_y[1] = 0xFFFF;
					himax_ts_button_func(tpd_key,ts);
					finger_on = 0;
				}
#ifdef HX_ESD_WORKAROUND
				ESD_COUNTER = 0;
#endif
			input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
			input_sync(ts->input_dev);
		} else if (hx_point_num == 0){
#if defined(HX_PALM_REPORT)
			loop_i = 0;
			base = loop_i * 4;
			x = buf[base] << 8 | buf[base + 1];
			y = (buf[base + 2] << 8 | buf[base + 3]);
			w = buf[(ts->nFinger_support * 4) + loop_i];
			I(" %s HX_PALM_REPORT_loopi=%d,base=%x,X=%x,Y=%x,W=%x \n",__func__,loop_i,base,x,y,w);

			if((!atomic_read(&ts->suspend_mode))&&(x==0xFA5A)&&(y==0xFA5A)&&(w==0x00))
				{
					I(" %s HX_PALM_REPORT KEY power event press\n",__func__);
					input_report_key(ts->input_dev, KEY_POWER, 1);
					input_sync(ts->input_dev);
					msleep(100);
					I(" %s HX_PALM_REPORT KEY power event release\n",__func__);
					input_report_key(ts->input_dev, KEY_POWER, 0);
					input_sync(ts->input_dev);
					return;
				}
#endif
			if(AA_press)
				{
				// leave event
				finger_on = 0;
				AA_press = 0;
				if (ts->protocol_type == PROTOCOL_TYPE_A)
					input_mt_sync(ts->input_dev);

				for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
						if (((ts->pre_finger_mask >> loop_i) & 1) == 1) {
							if (ts->protocol_type == PROTOCOL_TYPE_B) {
								input_mt_slot(ts->input_dev, loop_i);
								input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
							}
						}
					}
				if (ts->pre_finger_mask > 0) {
					for (loop_i = 0; loop_i < ts->nFinger_support && (ts->debug_log_level & BIT(3)) > 0; loop_i++) {
						if (((ts->pre_finger_mask >> loop_i) & 1) == 1) {
							if (ts->useScreenRes) {
								I("status:%X, Screen:F:%02d Up, X:%d, Y:%d, N:%d\n", 0, loop_i+1, ts->pre_finger_data[loop_i][0] * ts->widthFactor >> SHIFTBITS,
									ts->pre_finger_data[loop_i][1] * ts->heightFactor >> SHIFTBITS, Last_EN_NoiseFilter);
							} else {
								I("status:%X, Raw:F:%02d Up, X:%d, Y:%d, N:%d\n",0, loop_i+1, ts->pre_finger_data[loop_i][0],ts->pre_finger_data[loop_i][1], Last_EN_NoiseFilter);
							}
						}
					}
					ts->pre_finger_mask = 0;
				}

				if (ts->first_pressed == 1) {
					ts->first_pressed = 2;
					I("E1@%d, %d\n",ts->pre_finger_data[0][0] , ts->pre_finger_data[0][1]);
				}

				if (ts->debug_log_level & BIT(1))
					I("All Finger leave\n");

#ifdef HX_TP_SYS_DIAG
					//coordinate dump start
					if (coordinate_dump_enable == 1)
					{
						do_gettimeofday(&t);
						time_to_tm(t.tv_sec, 0, &broken);

						snprintf(&coordinate_char[0], ARRAY_SIZE(coordinate_char), "%2d:%2d:%2d:%lu,", broken.tm_hour, broken.tm_min, broken.tm_sec, t.tv_usec/1000);
						snprintf(&coordinate_char[15], (ARRAY_SIZE(coordinate_char) - 15), "Touch up!");
						coordinate_fn->f_op->write(coordinate_fn,&coordinate_char[0],15 + (HX_MAX_PT+5)*2*sizeof(char)*5 + 2,&coordinate_fn->f_pos);
					}
					//coordinate dump end
#endif
			}
			else if (tpd_key != 0x00) {
				//report key
				//temp_x[0] = 0xFFFF;
				//temp_y[0] = 0xFFFF;
				//temp_x[1] = 0xFFFF;
				//temp_y[1] = 0xFFFF;
				himax_ts_button_func(tpd_key,ts);
				finger_on = 1;
			}
			else if ((tpd_key_old != 0x00)&&(tpd_key == 0x00)) {
				//temp_x[0] = 0xFFFF;
				//temp_y[0] = 0xFFFF;
				//temp_x[1] = 0xFFFF;
				//temp_y[1] = 0xFFFF;
				himax_ts_button_func(tpd_key,ts);
				finger_on = 0;
			}
#ifdef HX_ESD_WORKAROUND
				ESD_COUNTER = 0;
#endif
			input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
			input_sync(ts->input_dev);
		}
		tpd_key_old = tpd_key;
		Last_EN_NoiseFilter = EN_NoiseFilter;

workqueue_out:
	return;

err_workqueue_out:
	I("%s: Now reset the Touch chip.\n", __func__);

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true,false);
#endif

	goto workqueue_out;
}

#ifdef QCT
static irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	struct himax_ts_data *ts = ptr;
	struct timespec timeStart, timeEnd, timeDelta;

	if (ts->debug_log_level & BIT(2)) {
		getnstimeofday(&timeStart);
		/*I(" Irq start time = %ld.%06ld s\n",
			timeStart.tv_sec, timeStart.tv_nsec/1000);*/
	}
#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode)&&(!FAKE_POWER_KEY_SEND)&&(ts->SMWP_enable)) {
		wake_lock_timeout(&ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		if(himax_parse_wake_event((struct himax_ts_data *)ptr)) {
			I(" %s SMART WAKEUP KEY power event press\n",__func__);
			input_report_key(ts->input_dev, KEY_POWER, 1);
			input_sync(ts->input_dev);
			msleep(100);
			I(" %s SMART WAKEUP KEY power event release\n",__func__);
			input_report_key(ts->input_dev, KEY_POWER, 0);
			input_sync(ts->input_dev);
			FAKE_POWER_KEY_SEND=true;
			return IRQ_HANDLED;
		}
	}
#endif
	himax_ts_work((struct himax_ts_data *)ptr);
	if(ts->debug_log_level & BIT(2)) {
		getnstimeofday(&timeEnd);
		timeDelta.tv_nsec = (timeEnd.tv_sec*1000000000+timeEnd.tv_nsec)
				-(timeStart.tv_sec*1000000000+timeStart.tv_nsec);
		/*I("Irq finish time = %ld.%06ld s\n",
			timeEnd.tv_sec, timeEnd.tv_nsec/1000);*/
		I("Touch latency = %ld us\n", timeDelta.tv_nsec/1000);
	}
	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

static enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;

	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

int himax_ts_register_interrupt(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;

	ts->irq_enabled = 0;
	//Work functon
	if (client->irq) {/*INT mode*/
		ts->use_irq = 1;
		if(HX_INT_IS_EDGE) {
			I("%s edge triiger falling\n ",__func__);
			ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, ts);
		} else {
			I("%s level trigger low\n ",__func__);
			ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
		}

		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			I("%s: irq enabled at qpio: %d\n", __func__, client->irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(client->irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else
		I("%s: client->irq is empty, use polling mode.\n", __func__);

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_WORKAROUND function*/
		ts->himax_wq = create_singlethread_workqueue("himax_touch");

		INIT_WORK(&ts->work, himax_ts_work_func);

		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}
	return ret;
}
#endif

#if defined(HX_USB_DETECT)
static void himax_cable_tp_status_handler_func(int connect_status)
{
	struct himax_ts_data *ts;
	int ret = 0;
	I("Touch: cable change to %d\n", connect_status);
	ts = private_ts;
	if (ts->cable_config) {
		if (!atomic_read(&ts->suspend_mode)) {
			if ((!!connect_status) != ts->usb_connected) {
				if (!!connect_status) {
					ts->cable_config[1] = 0x01;
					ts->usb_connected = 0x01;
				} else {
					ts->cable_config[1] = 0x00;
					ts->usb_connected = 0x00;
				}

				ret = i2c_himax_master_write(ts->client, ts->cable_config,
					sizeof(ts->cable_config), HIMAX_I2C_RETRY_TIMES);
				if (ret < 0) {
					E("%s: i2c access fail!\n", __func__);
					return ret;
				}

				I("%s: Cable status change: 0x%2.2X\n", __func__, ts->cable_config[1]);
			} else
				I("%s: Cable status is the same as previous one, ignore.\n", __func__);
		} else {
			if (connect_status)
				ts->usb_connected = 0x01;
			else
				ts->usb_connected = 0x00;
			I("%s: Cable status remembered: 0x%2.2X\n", __func__, ts->usb_connected);
		}
	}
}

static struct t_cable_status_notifier himax_cable_status_handler = {
	.name = "usb_tp_connected",
	.func = himax_cable_tp_status_handler_func,
};

#endif

#ifdef CONFIG_FB
static void himax_fb_register(struct work_struct *work)
{
	int ret = 0;
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
							work_att.work);
	I(" %s in", __func__);

	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if (ret)
		E(" Unable to register fb_notifier: %d\n", ret);
}
#endif

//=============================================================================================================
//
//	Segment : Himax SYS Debug Function
//
//=============================================================================================================
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)

static ssize_t touch_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	ts_data = private_ts;

	ret += snprintf(buf,  PAGE_SIZE, "%s_FW:%#x_CFG:%#x_SensorId:%#x\n",
		HIMAX852xes_NAME, ts_data->vendor_fw_ver,
		ts_data->vendor_config_ver, ts_data->vendor_sensor_id);

	return ret;
}

static DEVICE_ATTR(vendor, S_IRUSR|S_IRGRP, touch_vendor_show, NULL);

static ssize_t touch_attn_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct himax_ts_data *ts_data;
	ts_data = private_ts;

	snprintf(buf, PAGE_SIZE, "attn = %x\n",
			himax_int_gpio_read(ts_data->pdata->gpio_irq));
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(attn, S_IRUSR|S_IRGRP, touch_attn_show, NULL);

static ssize_t himax_int_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;

	count = snprintf(buf, PAGE_SIZE, "%d\n", ts->irq_enabled);

	return count;
}

static ssize_t himax_int_status_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = private_ts;
	int value, ret=0;

	if (sysfs_streq(buf, "0"))
		value = false;
	else if (sysfs_streq(buf, "1"))
		value = true;
	else
		return -EINVAL;

	if (value) {
		if(HX_INT_IS_EDGE) {
#ifdef QCT
			ret = request_threaded_irq(ts->client->irq, NULL, himax_ts_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ts->client->name, ts);
#endif
		} else {
#ifdef QCT
			ret = request_threaded_irq(ts->client->irq, NULL, himax_ts_thread,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT, ts->client->name, ts);
#endif
		}
		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
		}
	} else {
		himax_int_enable(ts->client->irq,0);
		free_irq(ts->client->irq, ts);
		ts->irq_enabled = 0;
	}

	return count;
}

static DEVICE_ATTR(enabled, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP,
			himax_int_status_show, himax_int_status_store);

static ssize_t himax_layout_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE,
			"%d %d %d %d \n", ts->pdata->abs_x_min,
			ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	return count;
}

static ssize_t himax_layout_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts = private_ts;
	char buf_tmp[5];
	int i = 0, j = 0, k = 0, ret;
	unsigned long value;
	int layout[4] = {0};

	for (i = 0; i < 20; i++) {
		if (buf[i] == ',' || buf[i] == '\n') {
			memset(buf_tmp, 0x0, sizeof(buf_tmp));
			if (i - j <= 5)
				memcpy(buf_tmp, buf + j, i - j);
			else {
				I("buffer size is over 5 char\n");
				return count;
			}
			j = i + 1;
			if (k < 4) {
				ret = strict_strtol(buf_tmp, 10, &value);
				layout[k++] = value;
			}
		}
	}
	if (k == 4) {
		ts->pdata->abs_x_min=layout[0];
		ts->pdata->abs_x_max=layout[1];
		ts->pdata->abs_y_min=layout[2];
		ts->pdata->abs_y_max=layout[3];
		I("%d, %d, %d, %d\n",
			ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
		input_unregister_device(ts->input_dev);
		himax_input_register(ts);
	} else
		I("ERR@%d, %d, %d, %d\n",
			ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
	return count;
}

static DEVICE_ATTR(layout, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP,
		himax_layout_show, himax_layout_store);

static ssize_t himax_debug_level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts_data;
	size_t count = 0;
	ts_data = private_ts;

	count += snprintf(buf, PAGE_SIZE, "%d\n", ts_data->debug_log_level);

	return count;
}

static ssize_t himax_debug_level_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct himax_ts_data *ts;
	char buf_tmp[11];
	int i;
	ts = private_ts;
	memset(buf_tmp, 0x0, sizeof(buf_tmp));
	memcpy(buf_tmp, buf, count);

	ts->debug_log_level = 0;
	for(i=0; i<count-1; i++) {
		if( buf_tmp[i]>='0' && buf_tmp[i]<='9' )
			ts->debug_log_level |= (buf_tmp[i]-'0');
		else if( buf_tmp[i]>='A' && buf_tmp[i]<='F' )
			ts->debug_log_level |= (buf_tmp[i]-'A'+10);
		else if( buf_tmp[i]>='a' && buf_tmp[i]<='f' )
			ts->debug_log_level |= (buf_tmp[i]-'a'+10);

		if(i!=count-2)
			ts->debug_log_level <<= 4;
	}

	if (ts->debug_log_level & BIT(3)) {
		if (ts->pdata->screenWidth > 0 && ts->pdata->screenHeight > 0 &&
		 (ts->pdata->abs_x_max - ts->pdata->abs_x_min) > 0 &&
		 (ts->pdata->abs_y_max - ts->pdata->abs_y_min) > 0) {
			ts->widthFactor = (ts->pdata->screenWidth << SHIFTBITS)/(ts->pdata->abs_x_max - ts->pdata->abs_x_min);
			ts->heightFactor = (ts->pdata->screenHeight << SHIFTBITS)/(ts->pdata->abs_y_max - ts->pdata->abs_y_min);
			if (ts->widthFactor > 0 && ts->heightFactor > 0)
				ts->useScreenRes = 1;
			else {
				ts->heightFactor = 0;
				ts->widthFactor = 0;
				ts->useScreenRes = 0;
			}
		} else
			I("Enable finger debug with raw position mode!\n");
	} else {
		ts->useScreenRes = 0;
		ts->widthFactor = 0;
		ts->heightFactor = 0;
	}

	return count;
}

static DEVICE_ATTR(debug_level, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP,
		himax_debug_level_show, himax_debug_level_dump);

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
	diag_mutual = kzalloc(x_channel * y_channel * sizeof(uint8_t), GFP_KERNEL);
}

#endif

#ifdef HX_TP_SYS_RESET
static ssize_t himax_reset_set(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] == '1')
		himax_HW_reset(true,false);
	return count;
}

static DEVICE_ATTR(reset, S_IWUSR|S_IWGRP, NULL, himax_reset_set);
#endif

#ifdef HX_TP_SYS_HITOUCH
static ssize_t himax_hitouch_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if(hitouch_command == 0)
		ret += snprintf(buf + ret, PAGE_SIZE, "Driver Version:2.0 \n");

	return ret;
}

//-------------------------------------------------------
//himax_hitouch_store
//command 0 : Get Driver Version
//command 1 : Hitouch Connect
//command 2 : Hitouch Disconnect
//--------------------------------------------------------
static ssize_t himax_hitouch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if(buf[0] == '0')
	{
		hitouch_command = 0;
	}
	else if(buf[0] == '1')
	{
		hitouch_is_connect = true;
		I("hitouch_is_connect = true\n");
	}
	else if(buf[0] == '2')
	{
		hitouch_is_connect = false;
		I("hitouch_is_connect = false\n");
	}
	return count;
}

static DEVICE_ATTR(hitouch, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP,
					himax_hitouch_show, himax_hitouch_store);
#endif

#ifdef HX_DOT_VIEW
static ssize_t himax_cover_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	count = snprintf(buf, PAGE_SIZE, "%d\n", ts->cover_enable);

	return count;
}

static ssize_t himax_cover_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct himax_ts_data *ts = private_ts;

	if (sysfs_streq(buf, "0"))
		ts->cover_enable = 0;
	else if (sysfs_streq(buf, "1"))
		ts->cover_enable = 1;
	else
		return -EINVAL;
	himax_set_cover_func(ts->cover_enable);

	I("%s: cover_enable = %d.\n", __func__, ts->cover_enable);

	return count;
}

static DEVICE_ATTR(cover, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP,
	himax_cover_show, himax_cover_store);
#endif

#ifdef HX_SMART_WAKEUP
static ssize_t himax_SMWP_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct himax_ts_data *ts = private_ts;
	size_t count = 0;
	count = snprintf(buf, PAGE_SIZE, "%d\n", ts->SMWP_enable);

	return count;
}

static ssize_t himax_SMWP_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct himax_ts_data *ts = private_ts;

	if (sysfs_streq(buf, "0"))
		ts->SMWP_enable = 0;
	else if (sysfs_streq(buf, "1"))
		ts->SMWP_enable = 1;
	else
		return -EINVAL;

	I("%s: SMART_WAKEUP_enable = %d.\n", __func__, ts->SMWP_enable);

	return count;
}

static DEVICE_ATTR(SMWP, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP,
	himax_SMWP_show, himax_SMWP_store);

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

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_debug_level.attr);
	if (ret) {
		E("%s: create_file dev_attr_debug_level failed\n", __func__);
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

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_attn.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_attn failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_enabled.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_enabled failed\n", __func__);
		return ret;
	}

	ret = sysfs_create_file(android_touch_kobj, &dev_attr_layout.attr);
	if (ret) {
		E("%s: sysfs_create_file dev_attr_layout failed\n", __func__);
		return ret;
	}

	#ifdef HX_TP_SYS_HITOUCH
		ret = sysfs_create_file(android_touch_kobj, &dev_attr_hitouch.attr);
		if (ret)
		{
			E("sysfs_create_file dev_attr_hitouch failed\n");
			return ret;
		}
	#endif

	#ifdef HX_DOT_VIEW
		ret = sysfs_create_file(android_touch_kobj, &dev_attr_cover.attr);
		if (ret)
		{
			E("sysfs_create_file dev_attr_cover failed\n");
			return ret;
		}
	#endif

	#ifdef HX_SMART_WAKEUP
		ret = sysfs_create_file(android_touch_kobj, &dev_attr_SMWP.attr);
		if (ret)
		{
			E("sysfs_create_file dev_attr_cover failed\n");
			return ret;
		}
	#endif

	return 0 ;
}

static void himax_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_debug_level.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_reset.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_attn.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_enabled.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_layout.attr);

	#ifdef HX_TP_SYS_HITOUCH
	sysfs_remove_file(android_touch_kobj, &dev_attr_hitouch.attr);
	#endif

	#ifdef HX_DOT_VIEW
	sysfs_remove_file(android_touch_kobj, &dev_attr_cover.attr);
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
		I(" DT-No vk info in DT");
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
		for (i = 0; i < cnt; i++)
			I(" vk[%d] idx:%d x_min:%d, y_max:%d", i,pdata->virtual_key[i].index,
				pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
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
			D(" %s:Invalid panel coords size %d", __func__, coords_size);
	}

	if (of_property_read_u32_array(dt,
			"himax,panel-coords", coords, coords_size) == 0) {
		pdata->abs_x_min = coords[0], pdata->abs_x_max = coords[1];
		pdata->abs_y_min = coords[2], pdata->abs_y_max = coords[3];
		I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
				pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			D(" %s:Invalid display coords size %d", __func__, coords_size);
	}
	rc = of_property_read_u32_array(dt,
			"himax,display-coords", coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		D(" %s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}
	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	I(" DT-%s:display-coords = (%d, %d)", __func__, pdata->screenWidth,
		pdata->screenHeight);

	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_irq)) {
		I(" DT:gpio_irq value is not valid\n");
	}

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,reset-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		I(" DT:gpio_rst value is not valid\n");
	}
	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,power_ldo-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_3v3_en)) {
		I(" DT:gpio_3v3_en value is not valid\n");
	}
	I(" DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d",
			pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_3v3_en);

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d", pdata->protocol_type);
	}
	himax_vk_parser(dt, pdata);

	return 0;
}
#endif

static int himax852xes_probe(struct i2c_client *client,
							const struct i2c_device_id *id)
{
	int err = 0;
	struct himax_ts_data *ts;
	struct himax_i2c_platform_data *pdata;

	//Check I2C functionality
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		return -ENODEV;
	}
	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_check_functionality_failed;
	}

	i2c_set_clientdata(client, ts);
	ts->client = client;
	ts->dev = &client->dev;

#ifdef CONFIG_OF
	if (client->dev.of_node) { /*DeviceTree Init Platform_data*/
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			err = -ENOMEM;
			goto err_alloc_ts_data_failed;
		}
		err = himax_parse_dt(ts, pdata);
		if (err < 0) {
			I(" pdata is NULL for DT\n");
			goto err_dt_platform_data_fail;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			I(" pdata is NULL(dev.platform_data)\n");
			err = -ENOMEM;
			goto err_alloc_ts_data_failed;
		}
	}
#else
	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		I(" pdata is NULL(dev.platform_data)\n");
		err = -ENOMEM;
		goto err_alloc_ts_data_failed;
	}

#endif

#ifdef HX_RST_PIN_FUNC
	ts->rst_gpio = pdata->gpio_reset;
#endif

	err = himax_gpio_power_config(ts->client, pdata);
	if (err)
		goto err_dt_platform_data_fail;

#ifndef CONFIG_OF
	if (pdata->power) {
		ret = pdata->power(1);
		if (ret < 0) {
			E("%s: power on failed\n", __func__);
			goto err_power_failed;
		}
	}
#endif

	private_ts = ts;

	if (himax_ic_package_check(ts) == false) {
		E("Himax chip doesn NOT EXIST");
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

	himax_read_TP_info(client);

	//Himax Power On and Load Config
	if (himax_loadSensorConfig(client, pdata) < 0) {
		E("%s: Load Sesnsor configuration failed, unload driver.\n", __func__);
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	}

	calculate_point_number();

#ifdef HX_TP_SYS_DIAG
	setXChannel(HX_RX_NUM); // X channel
	setYChannel(HX_TX_NUM); // Y channel
	setMutualBuffer();

	if (getMutualBuffer() == NULL) {
		E("%s: mutual buffer allocate fail failed\n", __func__);
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	}
#endif

#ifdef CONFIG_OF
	ts->power = pdata->power;
#endif
	ts->pdata = pdata;
	ts->x_channel = HX_RX_NUM;
	ts->y_channel = HX_TX_NUM;
	ts->nFinger_support = HX_MAX_PT;
	//calculate the i2c data size
	calcDataSize(ts->nFinger_support);
	I("%s: calcDataSize complete\n", __func__);
#ifdef CONFIG_OF
	ts->pdata->abs_pressure_min        = 0;
	ts->pdata->abs_pressure_max        = 200;
	ts->pdata->abs_width_min           = 0;
	ts->pdata->abs_width_max           = 200;
	pdata->cable_config[0]             = 0x90;
	pdata->cable_config[1]             = 0x00;
#endif
	ts->suspended                      = false;
#if defined(HX_USB_DETECT)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
	ts->protocol_type = pdata->protocol_type;
	I("%s: Use Protocol Type %c\n", __func__,
	ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	err = himax_input_register(ts);
	if (err) {
		E("%s: Unable to register %s input device\n",
			__func__, ts->input_dev->name);
		err = -ENOMEM;
		goto err_ic_gpio_power_failed;
	}

#ifdef CONFIG_FB
	ts->himax_att_wq = create_singlethread_workqueue("HMX_ATT_reuqest");
	if (!ts->himax_att_wq) {
		E(" allocate syn_att_wq failed\n");
		err = -ENOMEM;
		goto err_input_register_device_failed;
	}
	INIT_DELAYED_WORK(&ts->work_att, himax_fb_register);
	queue_delayed_work(ts->himax_att_wq, &ts->work_att, msecs_to_jiffies(15000));

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	ts->early_suspend.suspend = himax_ts_early_suspend;
	ts->early_suspend.resume = himax_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef HX_CHIP_STATUS_MONITOR//for ESD solution
	ts->himax_chip_monitor_wq =
			create_singlethread_workqueue("himax_chip_monitor_wq");
	if (!ts->himax_chip_monitor_wq)
	{
		E(" %s: create workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_work_failed;
	}
	INIT_DELAYED_WORK(&ts->himax_chip_monitor, himax_chip_monitor_function);
	queue_delayed_work(ts->himax_chip_monitor_wq,
		&ts->himax_chip_monitor, HX_POLLING_TIMER*HZ);
#endif

#ifdef HX_SMART_WAKEUP
	ts->SMWP_enable=0;
	wake_lock_init(&ts->ts_SMWP_wake_lock, WAKE_LOCK_SUSPEND, HIMAX852xes_NAME);
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	err = himax_touch_sysfs_init();
	if (err)
		goto err_wakelock_init_failed;
#endif

#ifdef HX_ESD_WORKAROUND
	ESD_RESET_ACTIVATE = 0;
#endif
#if defined(HX_USB_DETECT)
	if (ts->cable_config)
		cable_detect_register_notifier(&himax_cable_status_handler);
#endif
#ifdef HX_DOT_VIEW
	register_notifier_by_hallsensor(&hallsensor_status_handler);
#endif

	err = himax_ts_register_interrupt(ts->client);
	if (err)
		goto err_sysfs_init_failed;

	return 0;

err_sysfs_init_failed:
	himax_touch_sysfs_deinit();
err_wakelock_init_failed:
#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->ts_SMWP_wake_lock);
#endif
#ifdef HX_CHIP_STATUS_MONITOR
err_work_failed:
	cancel_delayed_work(&ts->himax_chip_monitor);
#endif
	cancel_delayed_work(&ts->work_att);
err_input_register_device_failed:
	input_unregister_device(ts->input_dev);
err_ic_gpio_power_failed:
	gpio_free(pdata->gpio_reset);
	gpio_free(pdata->gpio_3v3_en);
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
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts->early_suspend);
#endif

	if (!ts->use_irq)
		hrtimer_cancel(&ts->timer);

	destroy_workqueue(ts->himax_wq);
#ifdef HX_CHIP_STATUS_MONITOR
	destroy_workqueue(ts->himax_chip_monitor_wq);
#endif
	if (ts->protocol_type == PROTOCOL_TYPE_B)
		input_mt_destroy_slots(ts->input_dev);

	input_unregister_device(ts->input_dev);
#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->ts_SMWP_wake_lock);
#endif
	kfree(ts);

	return 0;

}

static int himax852xes_suspend(struct device *dev)
{
	int ret;
	uint8_t buf[2] = {0};
#ifdef HX_CHIP_STATUS_MONITOR
	int t=0;
#endif
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	if(ts->suspended)
	{
		I("%s: Already suspended. Skipped. \n", __func__);
		return 0;
	}
	else
	{
		ts->suspended = true;
		I("%s: enter \n", __func__);
	}

#ifdef HX_TP_SYS_HITOUCH
	if(hitouch_is_connect)
	{
		I("[himax] %s: Hitouch connect, reject suspend\n",__func__);
		return 0;
	}
#endif

#ifdef HX_CHIP_STATUS_MONITOR
	if(HX_ON_HAND_SHAKING)
	{
		for(t=0; t<100; t++)
		{
			if(HX_ON_HAND_SHAKING==0)
			{
				I("%s:HX_ON_HAND_SHAKING OK check %d times\n",__func__,t);
				break;
			}
			else
				msleep(1);
		}
		if(t==100)
		{
			E("%s:HX_ON_HAND_SHAKING timeout reject suspend\n",__func__);
			return 0;
		}
	}
#endif

#ifdef HX_SMART_WAKEUP
	if(ts->SMWP_enable)
	{
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND=false;
		buf[0] = 0x8F;
		buf[1] = 0x20;
		ret = i2c_himax_master_write(ts->client, buf, 2, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
		{
			E("[himax] %s: I2C access failed addr = 0x%x\n",
						__func__, ts->client->addr);
		}
		I("[himax] %s: SMART_WAKEUP enable, reject suspend\n",__func__);
		return 0;
	}
#endif

	himax_int_enable(ts->client->irq,0);

#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
#endif

	buf[0] = HX_CMD_TSSOFF;
	ret = i2c_himax_master_write(ts->client, buf, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0)
	{
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}
	msleep(40);

	buf[0] = HX_CMD_TSSLPIN;
	ret = i2c_himax_master_write(ts->client, buf, 1, HIMAX_I2C_RETRY_TIMES);
	if (ret < 0)
	{
		E("[himax] %s: I2C access failed addr = 0x%x\n", __func__, ts->client->addr);
	}

	if (!ts->use_irq) {
		ret = cancel_work_sync(&ts->work);
		if (ret)
			himax_int_enable(ts->client->irq,1);
	}

	//ts->first_pressed = 0;
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;
	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(0);

	return 0;
}

static int himax852xes_resume(struct device *dev)
{
#ifdef HX_SMART_WAKEUP
	int ret;
	uint8_t buf[2] = {0};
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	int t=0;
#endif
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter \n", __func__);

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(1);
#ifdef HX_CHIP_STATUS_MONITOR
	if(HX_ON_HAND_SHAKING)//chip on hand shaking,wait hand shaking
	{
		for(t=0; t<100; t++)
			{
				if(HX_ON_HAND_SHAKING==0)//chip on hand shaking end
					{
						I("%s:HX_ON_HAND_SHAKING OK check %d times\n",__func__,t);
						break;
					}
				else
					msleep(1);
			}
		if(t==100)
			{
				E("%s:HX_ON_HAND_SHAKING timeout reject resume\n",__func__);
				return 0;
			}
	}
#endif
#ifdef HX_SMART_WAKEUP
	if(ts->SMWP_enable)
	{
		//Sense Off
		i2c_himax_write_command(ts->client, 0x82, HIMAX_I2C_RETRY_TIMES);
		msleep(40);
		//Sleep in
		i2c_himax_write_command(ts->client, 0x80, HIMAX_I2C_RETRY_TIMES);
		buf[0] = 0x8F;
		buf[1] = 0x00;
		ret = i2c_himax_master_write(ts->client, buf, 2, HIMAX_I2C_RETRY_TIMES);
		if (ret < 0)
		{
			E("[himax] %s: I2C access failed addr = 0x%x\n",
						__func__, ts->client->addr);
		}
		msleep(50);
	}
#endif
	//Sense On
	i2c_himax_write_command(ts->client, 0x83, HIMAX_I2C_RETRY_TIMES);
	msleep(30);
	i2c_himax_write_command(ts->client, 0x81, HIMAX_I2C_RETRY_TIMES);
	atomic_set(&ts->suspend_mode, 0);

	himax_int_enable(ts->client->irq,1);

#ifdef HX_CHIP_STATUS_MONITOR
	HX_CHIP_POLLING_COUNT = 0;
	queue_delayed_work(ts->himax_chip_monitor_wq,
		&ts->himax_chip_monitor, HX_POLLING_TIMER*HZ); //for ESD solution
#endif

	ts->suspended = false;
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

	I(" %s\n", __func__);
	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts &&
			ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case FB_BLANK_UNBLANK:
			himax852xes_resume(&ts->client->dev);
		break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax852xes_suspend(&ts->client->dev);
		break;
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h)
{
	struct himax_ts_data *ts;
	ts = container_of(h, struct himax_ts_data, early_suspend);
	himax852xes_suspend(ts->client, PMSG_SUSPEND);
}

static void himax_ts_late_resume(struct early_suspend *h)
{
	struct himax_ts_data *ts;
	ts = container_of(h, struct himax_ts_data, early_suspend);
	himax852xes_resume(ts->client);
}
#endif

static const struct dev_pm_ops himax852xes_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = himax852xes_suspend,
	.resume  = himax852xes_resume,
#else
	.suspend = himax852xes_suspend,
#endif
};


static const struct i2c_device_id himax852xes_ts_id[] = {
	{HIMAX852xes_NAME, 0 },
	{}
};

#ifdef CONFIG_OF
static struct of_device_id himax_match_table[] = {
	{.compatible = "himax,8528d" },
	{},
};
#else
#define himax_match_table NULL
#endif

#ifdef QCT
static struct i2c_driver himax852xes_driver = {
	.id_table	= himax852xes_ts_id,
	.probe		= himax852xes_probe,
	.remove		= himax852xes_remove,
	.driver		= {
		.name = HIMAX852xes_NAME,
		.owner = THIS_MODULE,
		.of_match_table = himax_match_table,
#ifdef CONFIG_PM
		.pm	= &himax852xes_pm_ops,
#endif
	},
};
#endif

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
