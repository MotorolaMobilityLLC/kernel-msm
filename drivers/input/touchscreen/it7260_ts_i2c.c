/* drivers/input/touchscreen/IT7260_ts_i2c.c
 *
 * Copyright (C) 2007 Google, Inc.
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

#include <linux/module.h>
#include <linux/delay.h>
//#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#define MAX_BUFFER_SIZE		144
#define DEVICE_NAME			"IT7260"
#define IT7260_I2C_NAME		"IT7260"
#define SCREEN_X_RESOLUTION	320
#define SCREEN_Y_RESOLUTION	320
//If I2C interface on host platform can only transfer 8 bytes at a time,
//uncomment the following line.
//#define HAS_8_BYTES_LIMIT

#define IOC_MAGIC				'd'
#define IOCTL_SET 				_IOW(IOC_MAGIC, 1, struct ioctl_cmd168)
#define IOCTL_GET 				_IOR(IOC_MAGIC, 2, struct ioctl_cmd168)

struct ioctl_cmd168 {
	unsigned short bufferIndex;
	unsigned short length;
	unsigned short buffer[MAX_BUFFER_SIZE];
};

struct IT7260_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	struct work_struct work;
	#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
	#endif
	uint8_t debug_log_level;
};


// ASUS_BSP +++ Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"
char magic_key[16];
// ASUS_BSP --- Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"

static struct timer_list tp_timer;
static void tp_irq_handler_reg(unsigned long arg);

static int ite7260_major = 0; // dynamic major by default
static int ite7260_minor = 0;
static struct cdev ite7260_cdev;
static struct class *ite7260_class = NULL;
static dev_t ite7260_dev;
static struct input_dev *input_dev;
struct IT7260_ts_data *gl_ts;
static int Calibration__success_flag = 0;
static int Upgrade__success_flag = 0; 
struct device *class_dev = NULL;
static int it7260_status = 0; //ASUS_BSP Cliff +++ add for ATD check


#ifdef DEBUG
#define TS_DEBUG(fmt,args...)  printk( KERN_DEBUG "[it7260_i2c]: " fmt, ## args)
#define DBG() printk("[%s]:%d => \n",__FUNCTION__,__LINE__)
#else
#define TS_DEBUG(fmt,args...)
#define DBG()
#endif

static struct workqueue_struct *IT7260_wq;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void IT7260_ts_early_suspend(struct early_suspend *h);
static void IT7260_ts_late_resume(struct early_suspend *h);
#endif

//static int IdentifyCapSensor(struct IT7260_ts_data *ts);

int i2cReadFromIt7260(struct i2c_client *client, unsigned char bufferIndex,
		unsigned char dataBuffer[], unsigned short dataLength);

int i2cWriteToIt7260(struct i2c_client *client, unsigned char bufferIndex,
		unsigned char const dataBuffer[], unsigned short dataLength);

bool waitCommandDone(void)
{
	unsigned char ucQuery = 0xFF;
	unsigned int count = 0;
	
	do{
		if(!i2cReadFromIt7260(gl_ts->client, 0x80, &ucQuery, 1)){
			ucQuery = 0x01;
		}
		count++;
	}while(ucQuery & 0x01 && count < 500);
	
	if( ucQuery == 0){
        return  true;
    }else{
        return  false;
    }
}

bool fnFirmwareReinitialize(void)
{
	u8 pucBuffer[2];
//	u16 wCommandResponse = 0xFFFF;
	unsigned char wCommandResponse[2] = {0xFF, 0xFF};
	waitCommandDone();

	pucBuffer[0] = 0x6F;
	if(!i2cWriteToIt7260(gl_ts->client, 0x20, pucBuffer, 1))	{
		return false;
	}
	
	waitCommandDone();

	if(!i2cReadFromIt7260(gl_ts->client, 0xA0, wCommandResponse, 2 ))	{
		return false;
	}
//	if(wCommandResponse != 0x0000)
	if(wCommandResponse[0] | wCommandResponse[1])
	{
		return false;
	}
	return true;
}

static int CalibrationCapSensor(void)
{
//	unsigned char ucQuery;
	unsigned char pucCmd[80];
	int ret = 0;

	waitCommandDone();

	memset(pucCmd, 0x00, 20);
	pucCmd[0] = 0x13;
	////0x13 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00

	ret = i2cWriteToIt7260(gl_ts->client, 0x20, pucCmd, 5);

	if (ret < 0) {
		printk(KERN_ERR "[%s:%d] 0x13_write_failed\n", __FUNCTION__, __LINE__);
		return ret;
	}

	waitCommandDone();

	memset(pucCmd, 0x00, 20);
	ret = i2cReadFromIt7260(gl_ts->client, 0xA0, pucCmd, 1);

	if( pucCmd == 0x00 ){
		if (fnFirmwareReinitialize()){
			printk("!! fnFirmwareReinitialize success \r\n");
		}else{
			printk("!! fnFirmwareReinitialize fail \r\n"); 
  		} 
	}

	if (ret == 1)
		return 0;
	else
		return 1;
}

bool fnEnterFirmwareUpgradeMode(void)
{
	char pucBuffer[MAX_BUFFER_SIZE];
//	short wCommandResponse = 0xFFFF;
	unsigned char wCommandResponse[2] = {0xFF, 0xFF};
	waitCommandDone();
	
	pucBuffer[0] = 0x60;
	pucBuffer[1] = 0x00;
	pucBuffer[2] = 'I';
	pucBuffer[3] = 'T';
	pucBuffer[4] = '7';
	pucBuffer[5] = '2';
	pucBuffer[6] = '6';
	pucBuffer[7] = '0';
	pucBuffer[8] = 0x55;
	pucBuffer[9] = 0xAA;

#ifdef HAS_8_BYTES_LIMIT
	if( !i2cWriteToIt7260(gl_ts->client, 0x20, pucBuffer, 6) ){
		return false;
	}
#else
	if( !i2cWriteToIt7260(gl_ts->client, 0x20, pucBuffer, 10) ){
		return false;
	}
#endif
	
	waitCommandDone();

	if(!i2cReadFromIt7260(gl_ts->client, 0xA0, wCommandResponse, 2) ){
			return false;
	}

//	if(wCommandResponse != 0x0000)
	if(wCommandResponse[0] | wCommandResponse[1])
       {
		return false;
	}
	return true;
}

bool fnExitFirmwareUpgradeMode(void)
{
	char pucBuffer[MAX_BUFFER_SIZE];
//	short wCommandResponse = 0xFFFF;
	unsigned char wCommandResponse[2] = {0xFF, 0xFF};
	waitCommandDone();

	pucBuffer[0] = 0x60;
	pucBuffer[1] = 0x80;
	pucBuffer[2] = 'I';
	pucBuffer[3] = 'T';
	pucBuffer[4] = '7';
	pucBuffer[5] = '2';
	pucBuffer[6] = '6';
	pucBuffer[7] = '0';
	pucBuffer[8] = 0xAA;
	pucBuffer[9] = 0x55;

#ifdef HAS_8_BYTES_LIMIT
	if( !i2cWriteToIt7260(gl_ts->client, 0x20, pucBuffer, 6) ){
		return false;
	}
#else
	if( !i2cWriteToIt7260(gl_ts->client, 0x20, pucBuffer, 10) ){
		return false;
	}
#endif

	waitCommandDone();

	if(!i2cReadFromIt7260(gl_ts->client, 0xA0, wCommandResponse, 2 )){
		return false;
	}

//	if(wCommandResponse != 0x0000)
	if(wCommandResponse[0] | wCommandResponse[1])     
       {
		return false;
	}

	return true;
}

bool fnSetStartOffset(unsigned short wOffset)
{
	u8 pucBuffer[MAX_BUFFER_SIZE];
//	short wCommandResponse = 0xFFFF;
	unsigned char wCommandResponse[2] = {0xFF, 0xFF};
	waitCommandDone();

	pucBuffer[0] = 0x61;
	pucBuffer[1] = 0;
	pucBuffer[2] = ( wOffset & 0x00FF );
	pucBuffer[3] = (( wOffset & 0xFF00 ) >> 8 );

	if( !i2cWriteToIt7260(gl_ts->client, 0x20, pucBuffer, 4) ){
		return false;
	}

	waitCommandDone();

	if( !i2cReadFromIt7260(gl_ts->client, 0xA0, wCommandResponse, 2)  ){
		return false;
	}
		
//	if(wCommandResponse != 0x0000)
	if(wCommandResponse[0] | wCommandResponse[1])        
       {
		return false;
	}
		
	return true;
}

static bool fnWriteAndCompareFlash(unsigned int nLength, char pnBuffer[], unsigned short nStartOffset)
{
	unsigned int nIndex = 0;
	unsigned char buffer[130] = {0};
	unsigned char bufWrite[130] = {0};
	unsigned char bufRead[130] = {0};
#ifdef HAS_8_BYTES_LIMIT    
	unsigned char bufTemp[4] = {0};
#endif
	unsigned char nReadLength;
	int retryCount;
	int i;

#ifdef HAS_8_BYTES_LIMIT
	nReadLength = 4;
#else
	nReadLength = 128;
#endif

	while ( nIndex < nLength ) {
		retryCount = 0;
		do {
			fnSetStartOffset(nStartOffset + nIndex);
#ifdef HAS_8_BYTES_LIMIT
			// Write to Flash
			for(u8 nOffset = 0; nOffset < 128; nOffset += 4) {
				buffer[0] = 0xF0;
				buffer[1] = nOffset;
				for (i = 0; i < 4; i++) {
					bufWrite[nOffset + i] = buffer[2 + i] = pnBuffer[nIndex + i + nOffset];
				}
				i2cWriteToIt7260(gl_ts->client, 0x20, buffer, 6);
			}

			buffer[0] = 0xF1;
			fnWriteCommand(1, buffer);
#else
			buffer[0] = 0x62;
			buffer[1] = 128;
			for (i = 0; i < 128; i++) {
				bufWrite[i] = buffer[2 + i] = pnBuffer[nIndex + i];
			}
			i2cWriteToIt7260(gl_ts->client, 0x20, buffer, 130);
#endif
			// Read from Flash
			buffer[0] = 0x63;
	  		buffer[1] = nReadLength;
	  		
#ifdef HAS_8_BYTES_LIMIT
			for (u8 nOffset = 0; nOffset < 128; nOffset += 4) {
				fnSetStartOffset(nStartOffset + nIndex + nOffset);
				i2cWriteToIt7260(gl_ts->client, 0x20, buffer, 2);
				waitCommandDone();
				i2cReadFromIt7260(gl_ts->client, 0xA0, bufTemp, nReadLength);
				for( i = 0; i < 4; i++ ){
						bufRead[nOffset] = bufTemp[i];
				}
			}
#else
	 		fnSetStartOffset(nStartOffset + nIndex);
			i2cWriteToIt7260(gl_ts->client, 0x20, buffer, 2);
			waitCommandDone();
			i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead, nReadLength);
#endif
			// Compare
			/*
			if (nStartOffset == 0){
				printk("Cliff:fw_ver : %x,%x,%x,%x\n",bufRead[8], bufRead[9], bufRead[10], bufRead[11]);
			}
			else{
				printk("Cliff:cfg_ver : %x,%x,%x,%x\n",bufRead[nReadLength-8], bufRead[nReadLength-7], bufRead[nReadLength-6], bufRead[nReadLength-5]);
			}
			* */
			for (i = 0; i < 128; i++) {
				if (bufRead[i] != bufWrite[i]) {
					break;
				}
			}
			if (i == 128) break;
		}while ( retryCount++ < 4 );
		
		if ( retryCount == 4 && i != 128 )
			return false;
		nIndex += 128;
	}
	return true;
}

bool fnFirmwareDownload(unsigned int unFirmwareLength, u8* pFirmware, unsigned int unConfigLength, u8* pConfig)
{
	if((unFirmwareLength == 0 || pFirmware == NULL) && (unConfigLength == 0 || pConfig == NULL)){
		return false;
	}

	if(!fnEnterFirmwareUpgradeMode())	{
		return false;
	}
  
	if(unFirmwareLength != 0 && pFirmware != NULL){
		// Download firmware
		if(!fnWriteAndCompareFlash(unFirmwareLength, pFirmware, 0)){
			return false;
		}
	}

	if(unConfigLength != 0 && pConfig != NULL){
		// Download configuration
		unsigned short wFlashSize = 0x8000;
		if(!fnWriteAndCompareFlash(unConfigLength, pConfig, wFlashSize - (unsigned short)unConfigLength)){
			return false;
		}
	}

	if(!fnExitFirmwareUpgradeMode()){
		return false;
	}

	if(!fnFirmwareReinitialize()){
		return false;
	}
	
	return true;
}

static int Upgrade_FW_CFG(void)
{
	unsigned int fw_size = 0;
	unsigned int config_size = 0;
	struct file *fw_fd = NULL;
	struct file *config_fd = NULL;
	
	unsigned char bufRead[10] = {0};
	unsigned char bufRead2[10] = {0};
	
	u8 cmdbuf[] = { 0x01, 0x00 }; //Read FW
	u8 cmdbuf2[] = { 0x01, 0x06 }; //Read CFG
	
	mm_segment_t fs;
	u8 *fw_buf = kzalloc(0x8000, GFP_KERNEL);
	u8 *config_buf = kzalloc(0x500, GFP_KERNEL);
	if ( fw_buf  == NULL || config_buf == NULL  ){
		printk("kzalloc failed\n");
	}

	i2cWriteToIt7260(gl_ts->client, 0x20, cmdbuf, 2);
	waitCommandDone();
	i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead, 10);
	waitCommandDone();
	i2cWriteToIt7260(gl_ts->client, 0x20, cmdbuf2, 2);
	waitCommandDone();
	i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead2, 10);
	
	printk("IT7260_NOW:fw_ver : %x,%x,%x,%x\n\n",bufRead[5], bufRead[6], bufRead[7], bufRead[8]);
	printk("IT7260_NOW:cfg_ver : %x,%x,%x,%x\n\n",bufRead2[1], bufRead2[2], bufRead2[3], bufRead2[4]);

	fs = get_fs();
	set_fs(get_ds());
	
	fw_fd = filp_open("/system/etc/firmware/it7260.fw", O_RDONLY, 0);
	if (fw_fd < 0)
		printk("open /system/etc/firmware/it7260.fw failed\n");
	else
		fw_size = fw_fd->f_op->read(fw_fd, fw_buf, 0x8000, &fw_fd->f_pos);
	printk("IT7260_UPDATE:fw_ver : %x,%x,%x,%x\n",fw_buf[8], fw_buf[9], fw_buf[10], fw_buf[11]);
	printk("--------------------- fw_size = %x\n", fw_size);

	config_fd = filp_open("/system/etc/firmware/it7260.cfg", O_RDONLY, 0);
	if (config_fd < 0)
		printk("open /system/etc/firmware/it7260.cfg failed\n");
	else
		config_size = config_fd->f_op->read(config_fd, config_buf, 0x500, &config_fd->f_pos);
	printk("IT7260_UPDATE:cfg_ver : %x,%x,%x,%x\n",config_buf[config_size-8], config_buf[config_size-7], config_buf[config_size-6], config_buf[config_size-5]);
	printk("--------------------- config_size = %x\n", config_size);

	set_fs(fs);
	filp_close(fw_fd,NULL);
	filp_close(config_fd,NULL);
	
	if ((bufRead[5] == fw_buf[8] && bufRead[6] == fw_buf[9] && bufRead[7] == fw_buf[10] && bufRead[8] < fw_buf[11]) || 
	(bufRead2[1] == config_buf[config_size-8] && bufRead2[2] == config_buf[config_size-7] && bufRead2[3] == config_buf[config_size-6] && bufRead2[4] < config_buf[config_size-5])){

	//START UPDATE
	disable_irq(gl_ts->client->irq);
	if (fnFirmwareDownload(fw_size, fw_buf, config_size, config_buf) == false){
		//fail
		enable_irq(gl_ts->client->irq);
		return 1; 
	}else{
		//success
		enable_irq(gl_ts->client->irq);
		return 0;
	}
	}
	else
	{
		//stop
		printk("You Don't Need Update FW/CFG!! \n\n");
		return 2;
	}
}

ssize_t IT7260_calibration_show_temp(char *buf)
{
	return sprintf(buf, "%d", Calibration__success_flag);
}

ssize_t IT7260_calibration_store_temp(const char *buf)
{
	if(!CalibrationCapSensor()) {
		printk("IT7260_calibration_OK\n\n");
		Calibration__success_flag = 1;
		return 0;
	} else {
		printk("IT7260_calibration_failed\n");
		Calibration__success_flag = -1;
		return -1;
	}
}

//ASUS_BSP Cliff +++ add for ATD check
static ssize_t IT7260_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	if (it7260_status) {
		return sprintf(buf, "1\n");
	} else {
		return sprintf(buf, "0\n");
	}
}

static ssize_t IT7260_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 cmdbuf[] = { 0x01, 0x00 };
	u8 cmdbuf2[] = { 0x01, 0x06 };
	unsigned char bufRead[10] = {0};
	unsigned char bufRead2[10] = {0};
	
	i2cWriteToIt7260(gl_ts->client, 0x20, cmdbuf, 2);
	waitCommandDone();
	i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead, 10);
	waitCommandDone();
	i2cWriteToIt7260(gl_ts->client, 0x20, cmdbuf2, 2);
	waitCommandDone();
	i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead2, 10);
	
	return sprintf(buf, "%x,%x,%x,%x # %x,%x,%x,%x\n",bufRead[5], bufRead[6], bufRead[7], bufRead[8],bufRead2[1], bufRead2[2], bufRead2[3], bufRead2[4]);
}

ssize_t IT7260_status_store_temp(int ret)
{
	u8 cmdbuf[] = { 0x01, 0x00 };
	u8 cmdbuf2[] = { 0x01, 0x06 };
	unsigned char bufRead[10] = {0};
	unsigned char bufRead2[10] = {0};

	i2cWriteToIt7260(gl_ts->client, 0x20, cmdbuf, 2);
	waitCommandDone();
	i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead, 10);
	waitCommandDone();
	i2cWriteToIt7260(gl_ts->client, 0x20, cmdbuf2, 2);
	waitCommandDone();
	i2cReadFromIt7260(gl_ts->client, 0xA0, bufRead2, 10);
	
	printk("IT7260_NOW:fw_ver : %x,%x,%x,%x\n\n",bufRead[5], bufRead[6], bufRead[7], bufRead[8]);
	printk("IT7260_NOW:cfg_ver : %x,%x,%x,%x\n\n",bufRead2[1], bufRead2[2], bufRead2[3], bufRead2[4]);

	return 1;
}

static ssize_t IT7260_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	printk(KERN_DEBUG "%s():\n", __func__);
	sscanf(buf, "%d", &ret);
	IT7260_status_store_temp(ret);
	return count;
}

static ssize_t IT7260_version_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	printk(KERN_DEBUG "%s():\n", __func__);
	return count;
}

static DEVICE_ATTR(status, 0666, IT7260_status_show, IT7260_status_store);
static DEVICE_ATTR(version, 0666, IT7260_version_show, IT7260_version_store);

static struct attribute *it7260_attrstatus[] = {
	&dev_attr_status.attr,
	&dev_attr_version.attr,
	NULL
};

static const struct attribute_group it7260_attrstatus_group = {
	.attrs = it7260_attrstatus,
};
//ASUS_BSP Cliff --- add for ATD check

static ssize_t IT7260_calibration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_DEBUG "%s():\n", __func__);
	

	
	
	return IT7260_calibration_show_temp(buf);
}


static ssize_t IT7260_calibration_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	printk(KERN_DEBUG "%s():\n", __func__);
	IT7260_calibration_store_temp(buf);
	return count;
}

ssize_t IT7260_upgrade_show_temp(char *buf)
{
	return sprintf(buf, "%d", Upgrade__success_flag);
}

ssize_t IT7260_upgrade_store_temp(void)
{
	int ret = Upgrade_FW_CFG();
	if(ret == 0) {
		printk("IT7260_upgrade_OK\n\n");
		Upgrade__success_flag = 1;
		return 0;
	} 
	else if (ret == 2)
	{
		printk("IT7260_upgrade_STOP\n\n");
		return 0;
	}
	else {
		printk("IT7260_upgrade_failed\n");
		Upgrade__success_flag = -1;
		return -1;
	}
}

static ssize_t IT7260_upgrade_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_DEBUG "%s():\n", __func__);
	return IT7260_upgrade_show_temp(buf);
}

static ssize_t IT7260_upgrade_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	printk(KERN_DEBUG "%s():\n", __func__);
	IT7260_upgrade_store_temp();
	return count;
}

static DEVICE_ATTR(calibration, 0666, IT7260_calibration_show, IT7260_calibration_store);
static DEVICE_ATTR(upgrade, 0666, IT7260_upgrade_show, IT7260_upgrade_store);

static struct attribute *it7260_attributes[] = {
	&dev_attr_calibration.attr,
	&dev_attr_upgrade.attr,
	NULL
};

static const struct attribute_group it7260_attr_group = {
	.attrs = it7260_attributes,
};

int i2cReadFromIt7260(struct i2c_client *client, unsigned char bufferIndex,
		unsigned char dataBuffer[], unsigned short dataLength) {
	int ret;
	struct i2c_msg msgs[2] = { { .addr = client->addr,
			//Platform dependent
			.flags = I2C_M_NOSTART,//For NVidia Tegra2/Qualcomm/Marvell/Freescale/TI/...
			//.flags = 0,//For AMLogic/Broadcomm/...
			.len = 1, .buf = &bufferIndex }, { .addr = client->addr, .flags =
			I2C_M_RD, .len = dataLength, .buf = dataBuffer } };

	memset(dataBuffer, 0xFF, dataLength);
	ret = i2c_transfer(client->adapter, msgs, 2);
	return ret;
}

int i2cWriteToIt7260(struct i2c_client *client, unsigned char bufferIndex,
		unsigned char const dataBuffer[], unsigned short dataLength) {
	unsigned char buffer4Write[256];
	struct i2c_msg msgs[1] = { { .addr = client->addr, .flags = 0, .len =
			dataLength + 1, .buf = buffer4Write } };

	buffer4Write[0] = bufferIndex;
	memcpy(&(buffer4Write[1]), dataBuffer, dataLength);
	return i2c_transfer(client->adapter, msgs, 1);
}

static void Read_Point(struct IT7260_ts_data *ts) {
	unsigned char ucQuery = 0;
	unsigned char pucPoint[14];
	int ret = 0;
	int xraw, yraw;
	static int x[2] = { (int) -1, (int) -1 };
	static int y[2] = { (int) -1, (int) -1 };
	static bool finger[2] = { 0, 0 };
	static int home_match_flag = 0;

	i2cReadFromIt7260(ts->client, 0x80, &ucQuery, 1);
	if (ucQuery < 0) {
		printk("=error Read_Point=\n");
		if (ts->use_irq)
			enable_irq(ts->client->irq);
		return;
	} else {
		if (ucQuery & 0x80) {
#ifdef HAS_8_BYTES_LIMIT
			i2cReadFromIt7260(ts->client, 0xC0, &(pucPoint[6]), 8);
			ret = i2cReadFromIt7260(ts->client, 0xE0, pucPoint, 6);
#else //HAS_8_BYTES_LIMIT
			ret = i2cReadFromIt7260(ts->client, 0xE0, pucPoint, 14);
#endif //HAS_8_BYTES_LIMIT
			//printk("=Read_Point read ret[%d]--point[%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
				//ret,pucPoint[0],pucPoint[1],pucPoint[2],
				//pucPoint[3],pucPoint[4],pucPoint[5],pucPoint[6],pucPoint[7],pucPoint[8],
				//pucPoint[9],pucPoint[10],pucPoint[11],pucPoint[12],pucPoint[13]);
// ASUS_BSP +++ Tingyi "[PDK][DEBUG] Framework for asusdebugtool launch by touch"
// Sample code for magic key detection
#if 1
{	
	static unsigned long time_start_check = 0;
	static unsigned int match_offset = 0;
	static unsigned int match_step = 0;
	//static unsigned char hack_code[] = {0x9,0x0,0x9,0x0,0x9,0x0,0x9,0x0,0xFF};


	if (time_start_check && jiffies > time_start_check + 5 * HZ){
		//printk("Magic:TIMEOUT!!!!\n");
		time_start_check = 0;
		match_offset = 0;
		match_step = 0;
	}

	xraw = ((pucPoint[3] & 0x0F) << 8) + pucPoint[2];
	yraw = ((pucPoint[3] & 0xF0) << 4) + pucPoint[4];
	
	//printk("Magic:x=%d, y=%d, %x, %x, %x, %x, %x, %x\n\n\n\n\n\n\n", xraw, yraw,
	//pucPoint[0], pucPoint[1], pucPoint[2], pucPoint[3], pucPoint[4], pucPoint[5]);

	if (pucPoint[0])
	{
		if (xraw < 80 && match_step == 0)
		{
			if ((yraw < (80 + match_offset*50)) && (yraw > (match_offset*50)))
			{
				match_offset++;
				if (match_offset == 1){
					time_start_check = jiffies;
				}
				if (match_offset > 5){
					//step1
					match_step++;
				}
			}
		}
		if (match_step == 2)
		{
			if (xraw < 120 && yraw < 80)
			{
				//printk("Magic:match_offset = 1\n");
				match_offset = 1;
			}
			else if (match_offset == 1)
			{
				if (xraw > 190){
					//printk("Magic:match_offset = 2\n");
					match_offset = 2;
				}
			}
			else if (match_offset == 2)
			{
				if (xraw < 120 && yraw > 260){
// ASUS_BSP +++ Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"
					printk("MagicTouch:DEBUG KEY DETECTED !!!!\n");
					strcpy(magic_key,"DEBUG");
					kobject_uevent(&class_dev->kobj, KOBJ_CHANGE);
// ASUS_BSP --- Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"

					time_start_check = 0;
					match_offset = 0;
					match_step = 0;
				}
			}
		}
	}
	else
	{
		if (match_step == 1){
			//step2
			match_offset = 0;
			match_step++;
		}
		else{
			//reset
			time_start_check = 0;
			match_offset = 0;
			match_step = 0;
		}
	}
}
#endif
// ASUS_BSP +++ Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"
#if 0
{
	static unsigned long last_time_detect_home = 0;
	static int home_match_flag = 0;
	if (!home_match_flag && pucPoint[0] == 0xB && (jiffies > last_time_detect_home + 3 * HZ)){
		home_match_flag = 1;
		printk("MagicTouch:HOME KEY DETECTED !!!!\n");
		strcpy(magic_key,"HOME");
		kobject_uevent(&class_dev->kobj, KOBJ_CHANGE);
		last_time_detect_home = jiffies;
	}else{
		home_match_flag = 0;
	}
}
#endif
// ASUS_BSP --- Tingyi "[PDK][DEBUG] Framework for asusdebugtool launch by touch"
			if (ret) {

				// palm
				if (pucPoint[1] & 0x01) {
					if (!home_match_flag){
						home_match_flag = 1;
						printk("MagicTouch:HOME KEY DETECTED !!!!\n");
						strcpy(magic_key,"HOME");
						kobject_uevent(&class_dev->kobj, KOBJ_CHANGE);
					}
					if (ts->use_irq)
						enable_irq(ts->client->irq);
					//pr_info("pucPoint 1 is 0x01, it's a palm\n") ;
					return;
				}
				else {
					home_match_flag = 0;
				}
				
				// no more data
				if (!(pucPoint[0] & 0x08)) {
					if (finger[0]) {
						input_report_key(ts->input_dev, BTN_TOUCH,0);
						input_sync(ts->input_dev);
						finger[0] = 0;
					}
		
					if (ts->use_irq)
						enable_irq(ts->client->irq);
					//pr_info("(pucPoint[0] & 0x08) is false, means no more data\n") ;
					return;
				}

				if (pucPoint[0] & 0x01) {
					char pressure_point;

					xraw = ((pucPoint[3] & 0x0F) << 8) + pucPoint[2];
					yraw = ((pucPoint[3] & 0xF0) << 4) + pucPoint[4];

					pressure_point = pucPoint[5] & 0x0f;
					//printk("=Read_Point1 x=%d y=%d p=%d=\n",xraw,yraw,pressure_point);

					input_report_abs(ts->input_dev, ABS_X, xraw);
					input_report_abs(ts->input_dev, ABS_Y, yraw);
					input_report_key(ts->input_dev, BTN_TOUCH,1);
					input_sync(ts->input_dev);
					x[0] = xraw;
					y[0] = yraw;
					finger[0] = 1;
					//printk("=input Read_Point1 x=%d y=%d p=%d=\n",xraw,yraw,pressure_point);
				} 
			}
		}
	}
	if (ts->use_irq)
		enable_irq(ts->client->irq);

	//IdentifyCapSensor(gl_ts);
}

///////////////////////////////////////////////////////////////////////////////////////

static void IT7260_ts_work_func(struct work_struct *work) {

	struct IT7260_ts_data *ts = container_of(work, struct IT7260_ts_data, work);
	//printk(KERN_INFO "=IT7260_ts_work_func=\n"); 
	gl_ts = ts;
	Read_Point(ts);
}

int delayCount = 1;
static irqreturn_t IT7260_ts_irq_handler(int irq, void *dev_id) {
	struct IT7260_ts_data *ts = dev_id;

	if (delayCount == 1)
	{
		pr_info("=IT7260_ts_irq_handler=\n");
		printk ("=IT7260_ts_irq_handler=\n");
	}
	disable_irq_nosync(ts->client->irq);
	queue_work(IT7260_wq, &ts->work);
	
	return IRQ_HANDLED;
}

/////////////////////////////////////////////////////////
void sendCalibrationCmd(void) {
	int ret = 0;
	struct IT7260_ts_data *ts = gl_ts;
	unsigned char data[] = { 0x13, 0x00, 0x00, 0x00, 0x00 };
	unsigned char resp[2];
    unsigned char ucQuery;

	ret = i2cWriteToIt7260(ts->client, 0x20, data, 5);
	printk(KERN_INFO "IT7260 sent calibration command [%d]!!!\n", ret);

    do {
        ucQuery = 0x00;
        mdelay(1000);
        i2cReadFromIt7260(ts->client, 0x80, &ucQuery, 1);
    } while ((ucQuery & 0x01) != 0);

	//Read out response to clear interrupt.
	i2cReadFromIt7260(ts->client, 0xA0, resp, 2);

    //Send SW-Reset command
    data[0] = 0x0C;
    i2cWriteToIt7260(ts->client, 0x20, data, 1);
}

EXPORT_SYMBOL( sendCalibrationCmd);


void enableAutoTune(void) {
	int ret = 0;
	struct IT7260_ts_data *ts = gl_ts;
	unsigned char data[] = { 0x13, 0x00, 0x01, 0x00, 0x00 };
	unsigned char resp[2];
    unsigned char ucQuery;

	ret = i2cWriteToIt7260(ts->client, 0x20, data, 5);
	printk(KERN_INFO "IT7260 sent calibration command [%d]!!!\n", ret);

    do {
        ucQuery = 0x00;
        mdelay(1000);
        i2cReadFromIt7260(ts->client, 0x80, &ucQuery, 1);
    } while ((ucQuery & 0x01) != 0);

	//Read out response to clear interrupt.
	i2cReadFromIt7260(ts->client, 0xA0, resp, 2);

    //Send SW-Reset command
    data[0] = 0x0C;
    i2cWriteToIt7260(ts->client, 0x20, data, 1);
}

EXPORT_SYMBOL(enableAutoTune);

static void tp_irq_handler_reg(unsigned long arg)
{
    delayCount = 0;
    queue_work(IT7260_wq, &(gl_ts->work));
}
#if 1
static int IdentifyCapSensor(struct IT7260_ts_data *ts) {
	unsigned char ucQuery;
	unsigned char pucCmd[80];
	int ret = 0;
	//pr_info("=entry IdentifyCapSensor=\n");
	//pr_info("=entry IdentifyCapSensor---name[%s]---addr[%x]-flags[%d]=\n",ts->client->name,ts->client->addr,ts->client->flags);
	do {
		ret = i2cReadFromIt7260(ts->client, 0x80, &ucQuery, 1);
		if (ret < 0) return ret;
	} while (ucQuery & 0x01);

	//pr_info("=IdentifyCapSensor write cmd=\n");
	pucCmd[0] = 0x00;
	ret = i2cWriteToIt7260(ts->client, 0x20, pucCmd, 1);
	if (ret < 0) {
		printk(KERN_ERR "i2cWriteToIt7260() failed\n");
		/* fail? */
		return ret;
	}

	do {
		ret = i2cReadFromIt7260(ts->client, 0x80, &ucQuery, 1);
		if (ret < 0) return ret;
	} while (ucQuery & 0x01);
	//pr_info("=IdentifyCapSensor write read id=\n");
	ret = i2cReadFromIt7260(ts->client, 0xA0, pucCmd, 8);
	printk(
			"CLIFF=IdentifyCapSensor read id--[%d][%x][%x][%x][%x][%x][%x][%x][%x][%x][%x]=\n",
			ret, pucCmd[0], pucCmd[1], pucCmd[2], pucCmd[3], pucCmd[4],
			pucCmd[5], pucCmd[6], pucCmd[7], pucCmd[8], pucCmd[9]);
			
	return ret;
}
#endif

//ASUS_BSP +++ Maggie_Lee "Implement I2C stress test for I2C devices"
#ifdef CONFIG_I2C_STRESS_TEST
#include <linux/i2c_testcase.h>
#define I2C_TEST_TOUCH_SENSOR_FAIL (-1)

static int TestTouchSensorI2C (struct i2c_client *apClient)
{
	int err = 0;

	i2c_log_in_test_case("%s ++\n", __func__);
	
	err=IdentifyCapSensor(gl_ts);
	if(err<0){
		i2c_log_in_test_case("Fail to read sensor ID\n");
		goto error;
	}

	msleep(5);
	
	i2c_log_in_test_case("%s --\n", __func__);

	return I2C_TEST_PASS;

error:
	return I2C_TEST_TOUCH_SENSOR_FAIL;
}

static struct i2c_test_case_info gTouchTestCaseInfo[] =
{
     __I2C_STRESS_TEST_CASE_ATTR(TestTouchSensorI2C),
};
#endif
//ASUS_BSP --- Maggie_Lee "Implement I2C stress test for I2C devices"

static int IT7260_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id) {
	struct IT7260_ts_data *ts;
	int ret = 0;
	struct IT7260_i2c_platform_data *pdata;
	//unsigned long irqflags;
	int input_err = 0;
	

	u8 cmdbuf[2] = { 0x07, 0 };
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "IT7260_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	//irqflags = IRQF_TRIGGER_HIGH;
	pr_info("=entry IT7260_ts_probe=\n");

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_check_functionality_failed;
	}
	ts->client = client;

	ts->debug_log_level = 0x3;
	//ts->input_dev = input_dev;

	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;
//ASUS_BSP Cliff +++ add for ATD check
	ret = sysfs_create_group(&(client->dev.kobj), &it7260_attrstatus_group); 
	if(ret){
		dev_err(&client->dev, "failed to register sysfs\n");
	}
//ASUS_BSP Cliff --- add for ATD check

	ret=IdentifyCapSensor(ts);
	if(ret<0){
		printk ("CLIFF:IdentifyCapSensor FAIL");
		goto err_power_failed;
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	ts->early_suspend.suspend = IT7260_ts_early_suspend;
	ts->early_suspend.resume = IT7260_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	input_dev = input_allocate_device();
	if (input_dev == NULL) {
		input_err = -ENOMEM;
		printk(KERN_ERR "IT7260_ts_probe: Failed to allocate input device\n");
		goto input_error;
	}else ts->input_dev = input_dev;
	input_dev->name = "IT7260";
	input_dev->phys = "I2C";
	input_dev->id.bustype = BUS_I2C;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x7260;
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
    set_bit(KEY_BACK,input_dev->keybit);
    set_bit(KEY_HOME,input_dev->keybit);
    set_bit(KEY_SEARCH,input_dev->keybit);
    set_bit(KEY_MENU,input_dev->keybit);
    set_bit(INPUT_PROP_DIRECT,input_dev->propbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	//set_bit(BTN_2, input_dev->keybit);

	input_set_abs_params(input_dev, ABS_X, 0, SCREEN_X_RESOLUTION, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, SCREEN_Y_RESOLUTION, 0, 0);
	input_err = input_register_device(input_dev);
	if (input_err) goto input_error;

	//IT7260_wq = create_singlethread_workqueue("IT7260_wq");
    IT7260_wq = create_workqueue("IT7260_wq");
	if (!IT7260_wq)
		goto err_check_functionality_failed;
	INIT_WORK(&ts->work, IT7260_ts_work_func);

    // >>> 
    init_timer(&tp_timer) ;

    tp_timer.expires = jiffies + 10 * HZ;
    tp_timer.function = &tp_irq_handler_reg;

    add_timer(&tp_timer);
    // >>> 

	pr_info("IT7260_ts_probe-client->irq[%d]=\n", client->irq);
	if (client->irq) {
		ret = request_irq(client->irq, IT7260_ts_irq_handler, IRQF_TRIGGER_LOW,
//		ret = request_irq(client->irq, IT7260_ts_irq_handler, IRQF_DISABLED | IRQF_TRIGGER_LOW,
//		ret = request_irq(client->irq, IT7260_ts_irq_handler, IRQF_SHARED | IRQF_TRIGGER_LOW,
				client->name, ts);
		pr_info("IT7260_ts_probe-request_irq[%d]=%d\n", client->irq,ret);
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "request_irq failed\n");
	}
	
	ret = sysfs_create_group(&(client->dev.kobj), &it7260_attr_group); 
	if(ret){
		dev_err(&client->dev, "failed to register sysfs\n");
	}

	//ASUS_BSP +++ Maggie_Lee "Implement I2C stress test for I2C devices"
	#ifdef CONFIG_I2C_STRESS_TEST
	i2c_add_test_case(client, "TouchSensorTest", ARRAY_AND_SIZE(gTouchTestCaseInfo));
	#endif
	//ASUS_BSP --- Maggie_Lee "Implement I2C stress test for I2C devices"

	gl_ts = ts;
	it7260_status = 1;
	
	pr_info("=end IT7260_ts_probe=\n");

	//To reset point queue.
	i2cWriteToIt7260(ts->client, 0x20, cmdbuf, 1);
	mdelay(10);
	i2cReadFromIt7260(ts->client, 0xA0, cmdbuf, 2);
	mdelay(10);
	
	return 0;
	err_power_failed: kfree(ts);
	err_check_functionality_failed:
	if (IT7260_wq)
		destroy_workqueue(IT7260_wq);
	input_error:
	if(input_dev) {
		input_free_device(input_dev);
	}
	return ret;
}

static int IT7260_ts_remove(struct i2c_client *client) {
	   destroy_workqueue(IT7260_wq);
		input_free_device(input_dev);
		it7260_status = 0;
	return 0;
}


static int IT7260_ts_suspend(struct i2c_client *client, pm_message_t mesg) {
	char ret;
	u8 cmdbuf[] = { 0x04, 0x00, 0x02 };

    printk(KERN_DEBUG "IT7260_ts_i2c call suspend\n");
	if (i2cWriteToIt7260(client, 0x20, cmdbuf, 3) >= 0)
		ret = 0;
	else
		ret = -1;

	return ret;
}

static int IT7260_ts_resume(struct i2c_client *client) {
	unsigned char ucQuery;

	printk(KERN_DEBUG "IT7260_ts_i2c call resume\n");
	i2cReadFromIt7260(client, 0x80, &ucQuery, 1);
	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void IT7260_ts_early_suspend(struct early_suspend *h)
{
	struct IT7260_ts_data *ts;
	ts = container_of(h, struct IT7260_ts_data, early_suspend);
	IT7260_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void IT7260_ts_late_resume(struct early_suspend *h)
{
	struct IT7260_ts_data *ts;
	ts = container_of(h, struct IT7260_ts_data, early_suspend);
	IT7260_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id IT7260_ts_id[] = { { IT7260_I2C_NAME, 0 },
		{ } };

MODULE_DEVICE_TABLE(i2c, IT7260_ts_id);

static struct of_device_id IT7260_match_table[] = {
	{ .compatible = "ITE,IT7260_ts",},
	{},
};

static struct i2c_driver IT7260_ts_driver = { 
		.driver = {
			.owner = THIS_MODULE,
			.name = IT7260_I2C_NAME,
			.of_match_table = IT7260_match_table,
		  },
		.probe = IT7260_ts_probe,
		.remove = IT7260_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
		.suspend = IT7260_ts_suspend, 
		.resume = IT7260_ts_resume,
#endif
		.id_table = IT7260_ts_id, };

struct ite7260_data {
	rwlock_t lock;
	unsigned short bufferIndex;
	unsigned short length;
	unsigned short buffer[MAX_BUFFER_SIZE];
};


long ite7260_ioctl(struct file *filp, unsigned int cmd,unsigned long arg) {
//int ite7260_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
//		unsigned long arg) {
//	struct ite7260_data *dev = filp->private_data;
	int retval = 0;
	int i;
//	unsigned char ucQuery;
	unsigned char buffer[MAX_BUFFER_SIZE];
	struct ioctl_cmd168 data;
	unsigned char datalen;
	unsigned char ent[] = {0x60, 0x00, 0x49, 0x54, 0x37, 0x32};
	unsigned char ext[] = {0x60, 0x80, 0x49, 0x54, 0x37, 0x32};

	//pr_info("=ite7260_ioctl=\n");
	memset(&data, 0, sizeof(struct ioctl_cmd168));

	switch (cmd) {
	case IOCTL_SET:
		//pr_info("=IOCTL_SET=\n");
		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if ( copy_from_user(&data, (int __user *)arg, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		buffer[0] = (unsigned char) data.bufferIndex;
		//pr_info("%.2X ", buffer[0]);
		for (i = 1; i < data.length + 1; i++) {
			buffer[i] = (unsigned char) data.buffer[i - 1];
			//pr_info("%.2X ", buffer[i]);
		}
        if (!memcmp(&(buffer[1]), ent, sizeof(ent))) {

	        pr_info("Disabling IRQ.\n");

	        disable_irq(gl_ts->client->irq);

        }

        if (!memcmp(&(buffer[1]), ext, sizeof(ext))) {

	        pr_info("Enabling IRQ.\n");

	        enable_irq(gl_ts->client->irq);

        }

		//pr_info("=================================================\n");
		//pr_info("name[%s]---addr[%x]-flags[%d]=\n",gl_ts->client->name,gl_ts->client->addr,gl_ts->client->flags);
		datalen = (unsigned char) (data.length + 1);
		//pr_info("datalen=%d\n", datalen);
		//write_lock(&dev->lock);
		retval = i2cWriteToIt7260(gl_ts->client,
				(unsigned char) data.bufferIndex, &(buffer[1]), datalen - 1);
		//write_unlock(&dev->lock);
		//pr_info("SET:retval=%x\n", retval);
		retval = 0;
		break;

	case IOCTL_GET:		 
		//pr_info("=IOCTL_GET=\n");
		if (!access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto done;
		}

		//pr_info("sizeof(struct ioctl_cmd168)=%d\n", sizeof(struct ioctl_cmd168));
		if ( copy_from_user(&data, (int __user *)arg, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		
		if( data.length == 0 ){
#ifdef HAS_8_BYTES_LIMIT
			data.buffer[0] = 8;
#else
			data.buffer[0] = 128;
#endif
			if ( copy_to_user((int __user *)arg, &data, sizeof(struct ioctl_cmd168)) ) {
				retval = -EFAULT;
				goto done;
			}
			break;
		}

		//pr_info("=================================================\n");
		//pr_info("name[%s]---addr[%x]-flags[%d]=\n",gl_ts->client->name,gl_ts->client->addr,gl_ts->client->flags);
		//read_lock(&dev->lock);
		retval = i2cReadFromIt7260(gl_ts->client,
				(unsigned char) data.bufferIndex, (unsigned char*) buffer,
				(unsigned char) data.length);
		//read_unlock(&dev->lock);
		//pr_info("GET:retval=%x\n", retval);
		retval = 0;
		for (i = 0; i < data.length; i++) {
			data.buffer[i] = (unsigned short) buffer[i];
		}
		//pr_info("GET:bufferIndex=%x, dataLength=%d, buffer[0]=%x, buffer[1]=%x, buffer[2]=%x, buffer[3]=%x\n", data.bufferIndex, data.length, buffer[0], buffer[1], buffer[2], buffer[3]);
		//pr_info("GET:bufferIndex=%x, dataLength=%d, buffer[0]=%x, buffer[1]=%x, buffer[2]=%x, buffer[3]=%x\n", data.bufferIndex, data.length, data.buffer[0], data.buffer[1], data.buffer[2], data.buffer[3]);
		//if (data.bufferIndex == 0x80)
		//	data.buffer[0] = 0x00;
		if ( copy_to_user((int __user *)arg, &data, sizeof(struct ioctl_cmd168)) ) {
			retval = -EFAULT;
			goto done;
		}
		break;
		
	default:
		retval = -ENOTTY;
		break;
	}

	done:
	//pr_info("DONE! retval=%d\n", retval);
	return (retval);
}

int ite7260_open(struct inode *inode, struct file *filp) {
	int i;
	struct ite7260_data *dev;

	pr_info("=ite7260_open=\n");
	dev = kmalloc(sizeof(struct ite7260_data), GFP_KERNEL);
	if (dev == NULL) {
		return -ENOMEM;
	}

	/* initialize members */
	rwlock_init(&dev->lock);
	for (i = 0; i < MAX_BUFFER_SIZE; i++) {
		dev->buffer[i] = 0xFF;
	}

	filp->private_data = dev;

	return 0; /* success */
}

int ite7260_close(struct inode *inode, struct file *filp) {
	struct ite7260_data *dev = filp->private_data;

	if (dev) {
		kfree(dev);
	}

	return 0; /* success */
}

struct file_operations ite7260_fops = {
	.owner = THIS_MODULE,
	.open = ite7260_open,
	.release = ite7260_close,
	//.ioctl = ite7260_ioctl,
	.unlocked_ioctl = ite7260_ioctl, // Qualcomm
};


// ASUS_BSP +++ Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"
static ssize_t show_magic_key(struct device *device,
			 struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%s\n", magic_key);
	magic_key[0] = 0;
	return ret;
}
static struct device_attribute device_attrs[] = {
	__ATTR(magic_key, S_IRUGO, show_magic_key, NULL),
};
// ASUS_BSP --- Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"


static int __init IT7260_ts_init(void) {
	dev_t dev = MKDEV(ite7260_major, 0);
	int alloc_ret = 0;
	int cdev_err = 0;

	DBG();

	alloc_ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (alloc_ret) {
		TS_DEBUG("IT7260 cdev can't get major number\n");
		goto error;
	}
	ite7260_major = MAJOR(dev);

	// allocate the character device
	cdev_init(&ite7260_cdev, &ite7260_fops);
	ite7260_cdev.owner = THIS_MODULE;
	ite7260_cdev.ops = &ite7260_fops;
	cdev_err = cdev_add(&ite7260_cdev, MKDEV(ite7260_major, ite7260_minor), 1);
	if(cdev_err) {
		goto error;
	}

	// register class
	ite7260_class = class_create(THIS_MODULE, DEVICE_NAME);
	if(IS_ERR(ite7260_class)) {
		TS_DEBUG("Err: failed in creating class.\n");
		goto error;
	}

	ite7260_dev = MKDEV(ite7260_major, ite7260_minor);
	class_dev = device_create(ite7260_class, NULL, ite7260_dev, NULL, DEVICE_NAME);
	if(class_dev == NULL)
	{
		TS_DEBUG("Err: failed in creating device.\n");
		goto error;
	}

// ASUS_BSP +++ Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"
	magic_key[0] = 0;
	device_create_file(class_dev, &device_attrs[0]);
// ASUS_BSP --- Tingyi "[ROBIN][TOUCH] Report mgaci key for HOME and DEBUG"


	TS_DEBUG("=========================================\n");
	TS_DEBUG("register IT7260 cdev, major: %d, minor: %d \n", ite7260_major, ite7260_minor);
	TS_DEBUG("=========================================\n");

	return i2c_add_driver(&IT7260_ts_driver);

	error:
	if(cdev_err == 0) {
		cdev_del(&ite7260_cdev);
	}
	if(alloc_ret == 0) {
		unregister_chrdev_region(dev, 1);
	}
	return -1;
}

static void __exit IT7260_ts_exit(void) {
	dev_t dev = MKDEV(ite7260_major, ite7260_minor);

	// unregister class
	device_destroy(ite7260_class, ite7260_dev);
	class_destroy(ite7260_class);

	// unregister driver handle
	cdev_del(&ite7260_cdev);
	unregister_chrdev_region(dev, 1);

	i2c_del_driver(&IT7260_ts_driver);
	if (IT7260_wq)
	destroy_workqueue(IT7260_wq);
}

module_init( IT7260_ts_init);
module_exit( IT7260_ts_exit);

MODULE_DESCRIPTION("IT7260 Touchscreen Driver");
MODULE_LICENSE("GPL");
