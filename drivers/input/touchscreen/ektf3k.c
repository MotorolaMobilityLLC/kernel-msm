/* drivers/input/touchscreen/ektf3k.c - ELAN EKTF3K FIFO verions of driver
 *
 * Copyright (C) 2011 Elan Microelectronics Corporation.
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

#define ELAN_BUFFER_MODE

#include <linux/module.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/debugfs.h>

// for linux 2.6.36.3
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/ioctl.h>
#include <linux/switch.h>
#include <linux/proc_fs.h>
#include <linux/wakelock.h>

#define PACKET_SIZE		40
#define NEW_PACKET_SIZE 55
#define FINGER_NUM		10
		
#define PWR_STATE_DEEP_SLEEP	0
#define PWR_STATE_NORMAL		1
#define PWR_NORMAL_STATE 8
#define PWR_IDLE_STATE 1
#define PWR_STATE_MASK			BIT(3)

#define CMD_S_PKT			0x52
#define CMD_R_PKT			0x53
#define CMD_W_PKT			0x54

#define HELLO_PKT			0x55
#define NORMAL_PKT			0x63
#define NEW_NOMARL_PKT      0x66
#define TEN_FINGERS_PKT			0x62

#define RPT_LOCK_PKT		0x56
#define RPT_UNLOCK_PKT		0xA6

#define RESET_PKT			0x77
#define CALIB_PKT			0xA8

#define IDX_FINGER			3
#define MAX_FINGER_SIZE          31
#define MAX_FINGER_PRESSURE  255

#define ABS_MT_POSITION         0x2a    /* Group a set of X and Y */
#define ABS_MT_AMPLITUDE        0x2b    /* Group a set of Z and W */

#include <linux/i2c/ektf3k.h>

// For Firmware Update 
#define ELAN_IOCTLID	0xD0
#define IOCTL_I2C_SLAVE	_IOW(ELAN_IOCTLID,  1, int)
#define IOCTL_MAJOR_FW_VER  _IOR(ELAN_IOCTLID, 2, int)
#define IOCTL_MINOR_FW_VER  _IOR(ELAN_IOCTLID, 3, int)
#define IOCTL_RESET  _IOR(ELAN_IOCTLID, 4, int)
#define IOCTL_IAP_MODE_LOCK  _IOR(ELAN_IOCTLID, 5, int)
#define IOCTL_CHECK_RECOVERY_MODE  _IOR(ELAN_IOCTLID, 6, int)
#define IOCTL_FW_VER  _IOR(ELAN_IOCTLID, 7, int)
#define IOCTL_X_RESOLUTION  _IOR(ELAN_IOCTLID, 8, int)
#define IOCTL_Y_RESOLUTION  _IOR(ELAN_IOCTLID, 9, int)
#define IOCTL_FW_ID  _IOR(ELAN_IOCTLID, 10, int)
#define IOCTL_ROUGH_CALIBRATE  _IOR(ELAN_IOCTLID, 11, int)
#define IOCTL_IAP_MODE_UNLOCK  _IOR(ELAN_IOCTLID, 12, int)
#define IOCTL_I2C_INT  _IOR(ELAN_IOCTLID, 13, int)
#define IOCTL_RESUME  _IOR(ELAN_IOCTLID, 14, int)
#define IOCTL_FW_UPDATE _IOR(ELAN_IOCTLID, 22, int) 

//don't use firmware update
#define FIRMWARE_UPDATE_WITH_HEADER 1 

uint16_t checksum_err=0;
static uint8_t RECOVERY=0x00;
int FW_VERSION=0x00;
int X_RESOLUTION=0x00;
int Y_RESOLUTION=0x00;
int FW_ID=0x00;
int BOOTCODE_VERSION=0x00;
static int work_lock=0x00;

#define USB_NO_Cable 0
#define USB_DETECT_CABLE 1 
#define USB_SHIFT 0
#define AC_SHIFT 1 
#define USB_Cable ((1 << (USB_SHIFT)) | (USB_DETECT_CABLE))
#define USB_AC_Adapter ((1 << (AC_SHIFT)) | (USB_DETECT_CABLE))
#define USB_CALBE_DETECT_MASK (USB_Cable  | USB_DETECT_CABLE)
/*use for slim port to hdmi*/
#define SLIM_HDMI_MODE 10
#define HDMI_POWER_SOURCE_CMD 3
static unsigned now_usb_cable_status=0;
static unsigned int gPrint_point = 0; 

#define TOUCH_STRESS_TEST 1
#ifdef TOUCH_STRESS_TEST
#define STRESS_IOC_MAGIC 0xF3
#define STRESS_IOC_MAXNR 4
#define STRESS_POLL_DATA _IOR(STRESS_IOC_MAGIC,2,int )
#define STRESS_IOCTL_START_HEAVY 2
#define STRESS_IOCTL_START_NORMAL 1
#define STRESS_IOCTL_END 0
#define START_NORMAL	(HZ/5)
#define START_HEAVY	(HZ/200)
static int poll_mode=0;
static struct delayed_work elan_poll_data_work;
static struct workqueue_struct *stress_work_queue;
static atomic_t touch_char_available = ATOMIC_INIT(1);
#endif

#define _ENABLE_DBG_LEVEL    
#ifdef _ENABLE_DBG_LEVEL
	#define PROC_FS_NAME	"ektf_dbg"
	#define PROC_FS_MAX_LEN	8
	static struct proc_dir_entry *dbgProcFile;
#endif

struct elan_ktf3k_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *elan_wq;
	struct work_struct work;
	int (*power)(int on);
	struct early_suspend early_suspend;
	int intr_gpio;
// Firmware Information
	int fw_ver;
	int fw_id;
	int x_resolution;
	int y_resolution;
// For Firmare Update 
	struct miscdevice firmware;
      struct attribute_group attrs;
	int status;
	struct switch_dev touch_sdev;
	int abs_x_max;
	int abs_y_max;
	int rst_gpio;
	struct wake_lock wakelock;
#ifdef TOUCH_STRESS_TEST
      struct miscdevice  misc_dev;
#endif 
};

static struct elan_ktf3k_ts_data *private_ts = NULL;
static int __fw_packet_handler(struct i2c_client *client, int imediate);
static int elan_ktf3k_ts_rough_calibrate(struct i2c_client *client);
static int elan_ktf3k_ts_hw_reset(struct i2c_client *client, unsigned int time);
static int elan_ktf3k_ts_resume(struct i2c_client *client);

#ifdef FIRMWARE_UPDATE_WITH_HEADER
static int firmware_update_header(struct i2c_client *client, unsigned char *firmware, unsigned int page_number);
#endif

static struct semaphore pSem;
static int mTouchStatus[FINGER_NUM] = {0};

#define FIRMWARE_PAGE_SIZE 132
#define MAX_FIRMWARE_SIZE 52800
#define FIRMWARE_ACK_SIZE 2

/* Debug levels */
#define NO_DEBUG       0
#define DEBUG_ERROR  1
#define DEBUG_INFO     2
#define DEBUG_MESSAGES 5
#define DEBUG_TRACE   10

static int debug = DEBUG_INFO;

#define touch_debug(level, ...) \
	do { \
		if (debug >= (level)) \
			printk("[ektf3k]:" __VA_ARGS__); \
	} while (0)

int elan_iap_open(struct inode *inode, struct file *filp){ 
	touch_debug(DEBUG_INFO, "[ELAN]into elan_iap_open\n");
		if (private_ts == NULL)  touch_debug(DEBUG_ERROR, "private_ts is NULL~~~");
		
	return 0;
}

int elan_iap_release(struct inode *inode, struct file *filp){    
	return 0;
}

static ssize_t elan_iap_write(struct file *filp, const char *buff, size_t count, loff_t *offp){  
    int ret;
    char *tmp;

    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);
    
    if (tmp == NULL)
        return -ENOMEM;

    if (copy_from_user(tmp, buff, count)) {
        return -EFAULT;
    }
	
    ret = i2c_master_send(private_ts->client, tmp, count);
    if (ret != count) touch_debug(DEBUG_ERROR, "ELAN i2c_master_send fail, ret=%d \n", ret);
    kfree(tmp);
    return ret;

}

ssize_t elan_iap_read(struct file *filp, char *buff, size_t count, loff_t *offp){    
    char *tmp;
    int ret;  
    long rc;
   
    if (count > 8192)
        count = 8192;

    tmp = kmalloc(count, GFP_KERNEL);

    if (tmp == NULL)
        return -ENOMEM;

    ret = i2c_master_recv(private_ts->client, tmp, count);

    if (ret >= 0)
        rc = copy_to_user(buff, tmp, count);
    
    kfree(tmp);

    return ret;
}

static long elan_iap_ioctl(/*struct inode *inode,*/ struct file *filp,    unsigned int cmd, unsigned long arg){

	int __user *ip = (int __user *)arg;
	touch_debug(DEBUG_INFO, "[ELAN]into elan_iap_ioctl cmd=%u\n", cmd);

	switch (cmd) {        
		case IOCTL_I2C_SLAVE: 
			private_ts->client->addr = (int __user)arg;
			break;   
		case IOCTL_MAJOR_FW_VER:            
			break;        
		case IOCTL_MINOR_FW_VER:            
			break;        
		case IOCTL_RESET:
			return elan_ktf3k_ts_hw_reset(private_ts->client, 0);
		case IOCTL_IAP_MODE_LOCK:
			work_lock=1;
			disable_irq(private_ts->client->irq);
			wake_lock(&private_ts->wakelock);
			break;
		case IOCTL_IAP_MODE_UNLOCK:
			work_lock=0;
			enable_irq(private_ts->client->irq);
			wake_unlock(&private_ts->wakelock);
			break;
		case IOCTL_CHECK_RECOVERY_MODE:
			return RECOVERY;
			break;
		case IOCTL_FW_VER:
			__fw_packet_handler(private_ts->client, work_lock);
			msleep(100);
			return FW_VERSION;
			break;
		case IOCTL_X_RESOLUTION:
			__fw_packet_handler(private_ts->client, work_lock);
			msleep(100);
			return X_RESOLUTION;
			break;
		case IOCTL_Y_RESOLUTION:
			__fw_packet_handler(private_ts->client, work_lock);
			msleep(100);
			return Y_RESOLUTION;
			break;
		case IOCTL_FW_ID:
			__fw_packet_handler(private_ts->client, work_lock);
			msleep(100);
			return FW_ID;
			break;
		case IOCTL_ROUGH_CALIBRATE:
			return elan_ktf3k_ts_rough_calibrate(private_ts->client);
		case IOCTL_I2C_INT:
			put_user(gpio_get_value(private_ts->intr_gpio), ip);
			break;
		case IOCTL_RESUME:
			elan_ktf3k_ts_resume(private_ts->client);
			break;
		default:            
			break;   
	}       
	return 0;
}

struct file_operations elan_touch_fops = {    
        .open =         elan_iap_open,    
        .write =        elan_iap_write,    
        .read = 	elan_iap_read,    
        .release =	elan_iap_release,    
	.unlocked_ioctl=elan_iap_ioctl, 
 };

/* Detect old / new FW */
static int isOldFW(struct i2c_client *client)
{
    struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
        
    touch_debug(DEBUG_MESSAGES, "[elan] GPIO_TP_INT_N=%d\n", ts->intr_gpio);
    if (gpio_get_value(ts->intr_gpio) == 0) {
        // Old FW 
	 touch_debug(DEBUG_INFO,  "[elan]detect intr=>Old FW\n");
	 return 1;
    }
	
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void elan_ktf3k_ts_early_suspend(struct early_suspend *h);
static void elan_ktf3k_ts_late_resume(struct early_suspend *h);
#endif

static ssize_t elan_ktf3k_gpio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct elan_ktf3k_ts_data *ts = private_ts;

	ret = gpio_get_value(ts->intr_gpio);
	touch_debug(DEBUG_MESSAGES, "GPIO_TP_INT_N=%d\n", ts->intr_gpio);
	sprintf(buf, "GPIO_TP_INT_N=%d\n", ret);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(gpio, S_IRUGO, elan_ktf3k_gpio_show, NULL);

static ssize_t elan_ktf3k_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct elan_ktf3k_ts_data *ts = private_ts;

	sprintf(buf, "%s_x%4.4x\n", "ELAN_KTF3K", ts->fw_ver);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(vendor, S_IRUGO, elan_ktf3k_vendor_show, NULL);

static ssize_t elan_show_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct elan_ktf3k_ts_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", data->status);
}

DEVICE_ATTR(elan_touchpanel_status, S_IRUGO, elan_show_status, NULL);

static int check_fw_version(const unsigned char*firmware, unsigned int size, int fw_version){
       int id, version;
	   
       if(size < 2*FIRMWARE_PAGE_SIZE)
           return -1;
	   
	 version = firmware[size - 2*FIRMWARE_PAGE_SIZE + 120] | 
	 	      (firmware[size - 2*FIRMWARE_PAGE_SIZE + 121] << 8); 
	 id = firmware[size - 2*FIRMWARE_PAGE_SIZE + 122] | 
	 	      (firmware[size - 2*FIRMWARE_PAGE_SIZE + 123] << 8);
	 
	 touch_debug(DEBUG_INFO, "The firmware was version 0x%X and id:0x%X\n", version, id);

	 if (id == 0x3029 && BOOTCODE_VERSION >= 0x6046) {
		/*if the touch firmware was empty, always update firmware*/
		return fw_version == 0xFFFF ? 1 : version - fw_version;
	 } else {
		/*this buffer doesn't contain the touch firmware*/
		return 0;
	 }
	 
}


/*
static ssize_t update_firmware(struct device *dev, struct device_attribute *devattr,const char *buf, size_t count)
{
	 struct i2c_client *client = to_i2c_client(dev);
	 struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	 struct file *firmware_fp;
	 char file_path[100];
        unsigned int pos = 0;
	 mm_segment_t oldfs;
	 unsigned int page_number;
	 static unsigned char firmware[MAX_FIRMWARE_SIZE];
	 int ret = 0, retry = 0;
	 
	 oldfs=get_fs();
	 set_fs(KERNEL_DS);
	 memset(file_path, 0, 100);
	 sscanf(buf, "%s\n", file_path);
	 touch_debug(DEBUG_INFO, "Update touch firmware with the file path:%s\n", file_path);
	 firmware_fp = filp_open(file_path, O_RDONLY, S_IRUSR |S_IRGRP);
	 if(PTR_ERR(firmware_fp) == -ENOENT){
	     dev_err(&client->dev, "Error to open file %s\n", file_path);
	     return -1;
	 }    
       //start move the firmware file into memory
       firmware_fp->f_pos = 0;
       for(pos = 0, page_number = 0; pos < MAX_FIRMWARE_SIZE; pos += FIRMWARE_PAGE_SIZE, page_number++){
	     if(firmware_fp->f_op->read(firmware_fp, firmware + pos,
		 	FIRMWARE_PAGE_SIZE, &firmware_fp->f_pos) != FIRMWARE_PAGE_SIZE){
	         break;
	     }
	 }
	 filp_close(firmware_fp, NULL);
	 // check the firmware ID and version
	 if(RECOVERY || check_fw_version(firmware, pos, ts->fw_ver) > 0){
	     touch_debug(DEBUG_INFO, "Firmware update start!\n");	
	     do{
//	         ret = firmware_update_header(client, firmware, page_number);//add by mars
	         touch_debug(DEBUG_INFO, "Firmware update finish ret=%d retry=%d !\n", ret, retry++);
	     }while(ret != 0 && retry < 3);
	     if(ret == 0 && RECOVERY) RECOVERY = 0;
	 }else 
	     touch_debug(DEBUG_INFO, "No need to update firmware\n");
	     
	 return count;
}
*/
//DEVICE_ATTR(update_fw,  S_IWUSR, NULL, update_firmware);


static struct attribute *elan_attr[] = {
	&dev_attr_elan_touchpanel_status.attr,
	&dev_attr_vendor.attr,
	&dev_attr_gpio.attr,
	//&dev_attr_update_fw.attr,
	NULL
};

static struct kobject *android_touch_kobj;

/*
static int elan_ktf3k_touch_sysfs_init(void)
{
	int ret ;

	android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
	if (android_touch_kobj == NULL) {
		touch_debug(DEBUG_ERROR, "[elan]%s: subsystem_register failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_gpio.attr);
	if (ret) {
		touch_debug(DEBUG_ERROR, "[elan]%s: sysfs_create_file failed\n", __func__);
		return ret;
	}
	ret = sysfs_create_file(android_touch_kobj, &dev_attr_vendor.attr);
	if (ret) {
		touch_debug(DEBUG_ERROR, "[elan]%s: sysfs_create_group failed\n", __func__);
		return ret;
	}
	return 0 ;
}
*/
static void elan_touch_sysfs_deinit(void)
{
	sysfs_remove_file(android_touch_kobj, &dev_attr_vendor.attr);
	sysfs_remove_file(android_touch_kobj, &dev_attr_gpio.attr);
	kobject_del(android_touch_kobj);
}

static int __elan_ktf3k_ts_poll(struct i2c_client *client)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	int status = 0, retry = 150;

	do {
		status = gpio_get_value(ts->intr_gpio);
		dev_dbg(&client->dev, "%s: status = %d\n", __func__, status);
		retry--;
		msleep(20);
	} while (status == 1 && retry > 0);

	dev_dbg(&client->dev, "[elan]%s: poll interrupt status %s\n",
			__func__, status == 1 ? "high" : "low");
	return (status == 0 ? 0 : -ETIMEDOUT);
}

static int elan_ktf3k_ts_poll(struct i2c_client *client)
{
	return __elan_ktf3k_ts_poll(client);
}

static int elan_ktf3k_ts_get_data(struct i2c_client *client, uint8_t *cmd,
			uint8_t *buf, size_t size)
{
	int rc;

	dev_dbg(&client->dev, "[elan]%s: enter\n", __func__);

	if (buf == NULL)
		return -EINVAL;

      down(&pSem);
      rc = i2c_master_send(client, cmd, 4);
      up(&pSem);
	if(rc != 4){
	    dev_err(&client->dev,
			"[elan]%s: i2c_master_send failed\n", __func__);
	    return -EINVAL;
	}
      down(&pSem);
      rc  = i2c_master_recv(client, buf, size);
      up(&pSem);       
	if(rc != size){
	    dev_err(&client->dev,
			"[elan]%s: i2c_master_read failed\n", __func__);
	    return -EINVAL;
	}
	
	return 0;
}

static int elan_ktf3k_ts_read_command(struct i2c_client *client,
			   u8* cmd, u16 cmd_length, u8 *value, u16 value_length){
       struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];
	//__le16 le_addr;
	struct elan_ktf3k_ts_data *ts;
	int length = 0;

	ts = i2c_get_clientdata(client);

	msg[0].addr = client->addr;
	msg[0].flags = 0x00;
	msg[0].len = cmd_length;
	msg[0].buf = cmd;

	down(&pSem);
	length = i2c_transfer(adapter, msg, 1);
	up(&pSem);
	
	if (length == 1) // only send on packet
		return value_length;
	else
		return -EIO;
}

static int elan_ktf3k_i2c_read_packet(struct i2c_client *client, 
	u8 *value, u16 value_length){
       struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[1];
	//__le16 le_addr;
	struct elan_ktf3k_ts_data *ts;
	int length = 0;

	ts = i2c_get_clientdata(client);

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = value_length;
	msg[0].buf = (u8 *) value;
	down(&pSem);
	length = i2c_transfer(adapter, msg, 1);
	up(&pSem);
	
	if (length == 1) // only send on packet
		return value_length;
	else
		return -EIO;
}

static int __hello_packet_handler(struct i2c_client *client)
{
	int rc;
	uint8_t buf_recv[4] = { 0 };
	//uint8_t buf_recv1[4] = { 0 };

	rc = elan_ktf3k_ts_poll(client);
	if (rc < 0) {
		touch_debug(DEBUG_ERROR, "[elan] %s: IRQ is not low!\n", __func__);
		RECOVERY = 1; 
	}

	rc = i2c_master_recv(client, buf_recv, 4);
	if(rc < 0){
		touch_debug(DEBUG_ERROR, "I2C message no Ack!\n");
             return -EINVAL;
	}
	touch_debug(DEBUG_INFO, "[elan] %s: hello packet %2x:%2X:%2x:%2x\n", __func__, buf_recv[0], buf_recv[1], buf_recv[2], buf_recv[3]);
      if(!(buf_recv[0]==0x55 && buf_recv[1]==0x55 && buf_recv[2]==0x55 && buf_recv[3]==0x55)){
		RECOVERY=0x80;
		return RECOVERY;
	}
	return 0;
}

static int wait_for_IRQ_Low(struct i2c_client *client, int utime){
    struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
    int retry_times = 10;
    do{
        usleep_range(utime,utime + 500);
	  if(gpio_get_value(ts->intr_gpio) == 0)
	      return 0; 
    }while(retry_times-- > 0);
	
    touch_debug(DEBUG_INFO,"Wait IRQ time out\n");
    return -1;
}

static int __fw_packet_handler(struct i2c_client *client, int immediate)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	int rc;
	int major, minor;
	uint8_t cmd[] = {CMD_R_PKT, 0x00, 0x00, 0x01};
	uint8_t cmd_x[] = {0x53, 0x60, 0x00, 0x00}; /*Get x resolution*/
	uint8_t cmd_y[] = {0x53, 0x63, 0x00, 0x00}; /*Get y resolution*/
	uint8_t cmd_id[] = {0x53, 0xf0, 0x00, 0x01}; /*Get firmware ID*/
	uint8_t cmd_boot_id[] = {0x53, 0x10, 0x00, 0x01};/*Get boot code version*/
	uint8_t buf_recv[4] = {0};
// Firmware version
	rc = elan_ktf3k_ts_read_command(client, cmd, 4, buf_recv, 4);
	if (rc < 0)
		return rc;
	
	if(immediate){
	    wait_for_IRQ_Low(client, 1000);
	    elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
	    major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	    minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	    ts->fw_ver = major << 8 | minor;
	    FW_VERSION = ts->fw_ver;
	    touch_debug(DEBUG_INFO, "[elan] %s: firmware version: 0x%4.4x\n", __func__, ts->fw_ver);
	}
// X Resolution
	rc = elan_ktf3k_ts_read_command(client, cmd_x, 4, buf_recv, 4);
	if (rc < 0)
		return rc;
	
	if(immediate){
	    wait_for_IRQ_Low(client, 1000);
	    elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
	    minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	    ts->x_resolution =minor;
	    X_RESOLUTION = ts->x_resolution;
	    touch_debug(DEBUG_INFO, "[elan] %s: X resolution: 0x%4.4x\n", __func__, ts->x_resolution);
	}
// Y Resolution	
	rc = elan_ktf3k_ts_read_command(client, cmd_y, 4, buf_recv, 4);
	if (rc < 0)
		return rc;
	
	if(immediate){
	    wait_for_IRQ_Low(client, 1000);
	    elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
	    minor = ((buf_recv[2])) | ((buf_recv[3] & 0xf0) << 4);
	    ts->y_resolution =minor;
	    Y_RESOLUTION = ts->y_resolution;
	    touch_debug(DEBUG_INFO, "[elan] %s: Y resolution: 0x%4.4x\n", __func__, ts->y_resolution);
	}
// Firmware ID
	rc = elan_ktf3k_ts_read_command(client, cmd_id, 4, buf_recv, 4);
	if (rc < 0)
		return rc;
	
	if(immediate){
	    wait_for_IRQ_Low(client, 1000);
	    elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
	    major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	    minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);
	    ts->fw_id = major << 8 | minor;
	    FW_ID = ts->fw_id;
	    touch_debug(DEBUG_INFO, "[elan] %s: firmware id: 0x%4.4x\n", __func__, ts->fw_id);
	}
/*boot code version*/
	rc = elan_ktf3k_ts_read_command(client, cmd_boot_id, 4, buf_recv, 4);
	if (rc < 0)
		return rc;

	if (immediate) {
	    wait_for_IRQ_Low(client, 1000);
	    elan_ktf3k_i2c_read_packet(client, buf_recv, 4);
	    major = ((buf_recv[1] & 0x0f) << 4) | ((buf_recv[2] & 0xf0) >> 4);
	    minor = ((buf_recv[2] & 0x0f) << 4) | ((buf_recv[3] & 0xf0) >> 4);

	    BOOTCODE_VERSION = major << 8 | minor;
	    touch_debug(DEBUG_INFO, "[elan] %s: boot code id: 0x%4.4x\n", __func__, BOOTCODE_VERSION);
	}
	return 0;
}

static inline int elan_ktf3k_ts_parse_xy(uint8_t *data,
			uint16_t *x, uint16_t *y)
{
	*x = *y = 0;

	*x = (data[0] & 0xf0);
	*x <<= 4;
	*x |= data[1];

	*y = (data[0] & 0x0f);
	*y <<= 8;
	*y |= data[2];

	return 0;
}

static int elan_ktf3k_ts_setup(struct i2c_client *client)
{
	int rc, count = 10;
retry:	
        // Reset
        elan_ktf3k_ts_hw_reset(client, 250);
	// Check if old firmware. If not, send the notmal_command to enter normal mode
       if( isOldFW(client) == 0 ){ //if check is new bootcode
           touch_debug(DEBUG_INFO, "The boot code is new!\n");
	}else
	    touch_debug(DEBUG_INFO, "The boot code is old!\n");
	
	rc = __hello_packet_handler(client);
	touch_debug(DEBUG_INFO, "[elan] hello packet's rc = %d\n",rc);
	if (rc < 0){ 
               if (rc == -ETIME && count > 0) {
			count--;
			dev_err(&client->dev, "wait main hello timeout, reset\n");
			goto retry;
		}
		else
			goto hand_shake_failed;

	}

	dev_dbg(&client->dev, "[elan] %s: hello packet got.\n", __func__);
        msleep(200);
	rc = __fw_packet_handler(client, 1);
	if (rc < 0)
		goto hand_shake_failed;
	dev_dbg(&client->dev, "[elan] %s: firmware checking done.\n", __func__);
hand_shake_failed:
	return rc;
}

static int elan_ktf3k_ts_set_power_state(struct i2c_client *client, int state)
{
	uint8_t cmd[] = {CMD_W_PKT, 0x50, 0x00, 0x01};
	int length;

	dev_dbg(&client->dev, "[elan] %s: enter\n", __func__);

	cmd[1] |= (state << 3);

	dev_dbg(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);

      down(&pSem);
      length = i2c_master_send(client, cmd, sizeof(cmd));
      up(&pSem);
	if (length != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf3k_ts_rough_calibrate(struct i2c_client *client){
      uint8_t cmd[] = {CMD_W_PKT, 0x29, 0x00, 0x01};
      int length;

	touch_debug(DEBUG_INFO, "[elan] %s: enter\n", __func__);
	touch_debug(DEBUG_INFO,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);
	
	down(&pSem);
	length = i2c_master_send(client, cmd, sizeof(cmd));
	up(&pSem);
	if (length != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int elan_ktf3k_ts_get_power_state(struct i2c_client *client)
{
	int rc = 0;
	uint8_t cmd[] = {CMD_R_PKT, 0x50, 0x00, 0x01};
	uint8_t buf[4], power_state;

	rc = elan_ktf3k_ts_get_data(client, cmd, buf, 4);
	if (rc)
		return rc;

	power_state = buf[1];
	touch_debug(DEBUG_INFO, "[elan] dump repsponse: %0x\n", power_state);
	power_state = power_state & 0x0F;
	dev_dbg(&client->dev, "[elan] power state = %s\n",
		power_state == PWR_STATE_DEEP_SLEEP ?
		"Deep Sleep" : "Normal/Idle");

	return power_state;
}

static int elan_ktf3k_ts_hw_reset(struct i2c_client *client, unsigned int time)
{
      struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
      touch_debug(DEBUG_INFO, "[ELAN] Start HW reset!\n");
      gpio_direction_output(ts->rst_gpio, 0);
	usleep_range(1000,1500);
	gpio_direction_output(ts->rst_gpio, 1);
	if(time) msleep(time);
	return 0;
}


static int elan_ktf3k_ts_set_power_source(struct i2c_client *client, u8 state)
{
	uint8_t cmd[] = {CMD_W_PKT, 0x40, 0x00, 0x01};
	int length = 0;

	dev_dbg(&client->dev, "[elan] %s: enter\n", __func__);
	/*
	0x52 0x40 0x00 0x01  =>    Battery Mode
	0x52 0x41 0x00 0x01  =>    USB and AC Adapter Mode
	0x52 0x43 0x00 0x01  =>    SLIM Port to HDMI
	*/
	cmd[1] |= state & 0x0F;

	dev_dbg(&client->dev,
		"[elan] dump cmd: %02x, %02x, %02x, %02x\n",
		cmd[0], cmd[1], cmd[2], cmd[3]);
      dev_info(&private_ts->client->dev, "Update power source to %d\n", state);	
      down(&pSem);
      length = i2c_master_send(client, cmd, sizeof(cmd));
      up(&pSem);
	if (length != sizeof(cmd)) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_send failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/*
static int elan_ktf3k_ts_get_power_source(struct i2c_client *client)
{
	int rc = 0;
	uint8_t cmd[] = {CMD_R_PKT, 0x40, 0x00, 0x01};
	uint8_t buf[4] = {0};

	//rc = elan_ktf2k_ts_get_data(client, cmd, buf, 4);
	rc = elan_ktf3k_ts_read_command(client, cmd, 4, buf, 4);
	if (rc < 0)
		return rc;

	return 0;
}
*/

static void update_power_source(void){
      unsigned power_source = now_usb_cable_status;
      if(private_ts == NULL || work_lock) return;
	// Send power state 1 if USB cable and AC charger was plugged on. 
	if (power_source == SLIM_HDMI_MODE) {
		elan_ktf3k_ts_set_power_source(private_ts->client, HDMI_POWER_SOURCE_CMD);
	} else {
		elan_ktf3k_ts_set_power_source(private_ts->client, power_source != USB_NO_Cable);
	}
}

void touch_callback(unsigned cable_status){ 
      now_usb_cable_status = cable_status;
      update_power_source();
}

static int elan_ktf3k_ts_recv_data(struct i2c_client *client, uint8_t *buf, int size)
{

	int rc, bytes_to_recv = size;

	if (buf == NULL)
		return -EINVAL;

	memset(buf, 0, bytes_to_recv);
	rc = i2c_master_recv(client, buf, bytes_to_recv);

	if (rc != bytes_to_recv) {
		dev_err(&client->dev,
			"[elan] %s: i2c_master_recv error?! \n", __func__);
		rc = i2c_master_recv(client, buf, bytes_to_recv);
		return -EINVAL;
	}

	return rc;
}

static void elan_ktf3k_ts_report_data(struct i2c_client *client, uint8_t *buf)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	struct input_dev *idev = ts->input_dev;
	uint16_t x, y, touch_size, pressure_size;
	uint16_t fbits=0, checksum=0;
	uint8_t i, num;
	static uint8_t size_index[10] = {35, 35, 36, 36, 37, 37, 38, 38, 39, 39};
	uint16_t active = 0;
	uint8_t idx=IDX_FINGER;

      num = buf[2] & 0xf; 
	for (i=0; i<34;i++)
		checksum +=buf[i];
	
       if ((num < 3) || ((checksum & 0x00ff) == buf[34])) { 
	    fbits = buf[2] & 0x30;	
	    fbits = (fbits << 4) | buf[1]; 
          for(i = 0; i < FINGER_NUM; i++){
              active = fbits & 0x1;
              if(active || mTouchStatus[i]){
		     input_mt_slot(ts->input_dev, i);
                  input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, active);
                  if(active){
                      elan_ktf3k_ts_parse_xy(&buf[idx], &x, &y);
			   x = x > ts->abs_x_max ? 0 : ts->abs_x_max - x;
			   y = y > ts->abs_y_max ? ts->abs_y_max : y; 
		         touch_size = ((i & 0x01) ? buf[size_index[i]] : (buf[size_index[i]] >> 4)) & 0x0F;
			   pressure_size = touch_size << 4; // shift left touch size value to 4 bits for max pressure value 255   
                      input_report_abs(idev, ABS_MT_TOUCH_MAJOR, touch_size);
                      input_report_abs(idev, ABS_MT_PRESSURE, pressure_size);
                      input_report_abs(idev, ABS_MT_POSITION_X, y);
                      input_report_abs(idev, ABS_MT_POSITION_Y, x);
                      if(unlikely(gPrint_point)) touch_debug(DEBUG_INFO, "[elan] finger id=%d X=%d y=%d size=%d pressure=%d\n", i, x, y, touch_size, pressure_size);
		     }
		 }
		 mTouchStatus[i] = active;
              fbits = fbits >> 1;
              idx += 3;
	    }
          input_sync(idev);
	} // checksum
	else {
		checksum_err +=1;
		touch_debug(DEBUG_ERROR, "[elan] Checksum Error %d byte[2]=%X\n", checksum_err, buf[2]);
	}   
     	
	return;
}

static void elan_ktf3k_ts_report_data2(struct i2c_client *client, uint8_t *buf)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	struct input_dev *idev = ts->input_dev;
	uint16_t x, y, touch_size, pressure_size;
	uint16_t fbits=0, checksum=0;
	uint8_t i, num;
	uint16_t active = 0; 
	uint8_t idx=IDX_FINGER;

      num = buf[2] & 0xf;
	for (i=0; i<34;i++)
		checksum +=buf[i];
	
	if ( (num < 3) || ((checksum & 0x00ff) == buf[34])) {   
          fbits = buf[2] & 0x30;	
	    fbits = (fbits << 4) | buf[1]; 
	    //input_report_key(idev, BTN_TOUCH, 1);
          for(i = 0; i < FINGER_NUM; i++){
              active = fbits & 0x1;
              if(active || mTouchStatus[i]){
		     input_mt_slot(ts->input_dev, i);
                  input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, active);
                  if(active){
		         elan_ktf3k_ts_parse_xy(&buf[idx], &x, &y);
                      x = x > ts->abs_x_max ? 0 : ts->abs_x_max - x;
			   y = y > ts->abs_y_max ? ts->abs_y_max : y;
			   touch_size = buf[35 + i];
			   pressure_size = buf[45 + i];	 
			   input_report_abs(idev, ABS_MT_TOUCH_MAJOR, touch_size);
			   input_report_abs(idev, ABS_MT_PRESSURE, pressure_size);
			   input_report_abs(idev, ABS_MT_POSITION_X, y);
			   input_report_abs(idev, ABS_MT_POSITION_Y, x);
			   if(unlikely(gPrint_point)) touch_debug(DEBUG_INFO, "[elan] finger id=%d X=%d y=%d size=%d pressure=%d\n", i, x, y, touch_size, pressure_size);
		     }
		 }
		 mTouchStatus[i] = active;
              fbits = fbits >> 1;
              idx += 3;
	    }
          input_sync(idev);
	} // checksum
	else {
		checksum_err +=1;
		touch_debug(DEBUG_ERROR, "[elan] Checksum Error %d byte[2]=%X\n", checksum_err, buf[2]);
	} 

	return;
}

static void process_resp_message(struct elan_ktf3k_ts_data *ts, const unsigned char *buf,
	                                                      unsigned int size){
      int major, minor;

      if(buf == NULL) return;
      switch(buf[1] & 0xF0){
      case 0x00:// firmware version
          major = ((buf[1] & 0x0f) << 4) | ((buf[2] & 0xf0) >> 4);
          minor = ((buf[2] & 0x0f) << 4) | ((buf[3] & 0xf0) >> 4);
          ts->fw_ver = major << 8 | minor;
          FW_VERSION = ts->fw_ver;
          break;
      case 0x20: // Rough calibrate response
          major = buf[2] & 0x0F;	  	
          touch_debug(DEBUG_INFO, "Get the Rough Calibrate result:%d\n", major);
          break;
      case 0x40: // the power source
          major = buf[1] & 0x0f;
          touch_debug(DEBUG_INFO, "Get the power source:%d\n", major);
      case 0x50: // the power state
          major = buf[1] & 0x0f;
          touch_debug(DEBUG_INFO, "Get the power mode:%d\n", major);
          break;
      case 0xF0: // firmware id
          major = ((buf[1] & 0x0f) << 4) | ((buf[2] & 0xf0) >> 4);
          minor = ((buf[2] & 0x0f) << 4) | ((buf[3] & 0xf0) >> 4);
          ts->fw_id = major << 8 | minor;
          FW_ID = ts->fw_id;
          break;	   
      default: 
          touch_debug(DEBUG_INFO, "[elan] Get unknow packet {0x%02X, 0x%02X, 0x%02X, 0x%02X}\n", buf[0], buf[1], buf[2], buf[3]);	
      }
}

static void elan_ktf3k_ts_work_func(struct work_struct *work)
{
	int rc;
	struct elan_ktf3k_ts_data *ts =
		container_of(work, struct elan_ktf3k_ts_data, work);
	uint8_t buf[NEW_PACKET_SIZE + 4] = { 0 };
	uint8_t buf1[NEW_PACKET_SIZE] = { 0 };
	uint8_t buf2[NEW_PACKET_SIZE] = { 0 };

	if(work_lock!=0) {
		touch_debug(DEBUG_INFO, "Firmware update during touch event handling");
		enable_irq(ts->client->irq);
		return;
	}
	      
#ifndef ELAN_BUFFER_MODE
		rc = elan_ktf3k_ts_recv_data(ts->client, buf, 40);
#else
             down(&pSem);
		rc = elan_ktf3k_ts_recv_data(ts->client, buf, 4); // read the first four bytes
#endif 
		if (rc < 0)
		{
                   up(&pSem);
			enable_irq(ts->client->irq);
			return;
		}
#ifndef ELAN_BUFFER_MODE
		elan_ktf3k_ts_report_data(ts->client, buf);
#else
             switch(buf[0]){
             case NORMAL_PKT:
		    rc = elan_ktf3k_ts_recv_data(ts->client, buf+4, 40); // read the finger report packet.
		    up(&pSem);
		    elan_ktf3k_ts_report_data(ts->client, buf+4);
	           // Second package
	 	    if ((buf[1] == 2) || (buf[1] == 3)) {
		        rc = elan_ktf3k_ts_recv_data(ts->client, buf1,PACKET_SIZE);
                     if (rc < 0){
			      enable_irq(ts->client->irq);
				return;
			  }
			  elan_ktf3k_ts_report_data(ts->client, buf1);
		    }
		
	           // Final package
		    if (buf[1] == 3) {
		        rc = elan_ktf3k_ts_recv_data(ts->client, buf2, PACKET_SIZE);
		        if (rc < 0){
		            enable_irq(ts->client->irq);
                         return;
                     }
                     elan_ktf3k_ts_report_data(ts->client, buf2);
                 }
		    break;
		case NEW_NOMARL_PKT:
		    rc = elan_ktf3k_ts_recv_data(ts->client, buf+4, NEW_PACKET_SIZE); // read the finger report packet.
		    up(&pSem);
		    elan_ktf3k_ts_report_data2(ts->client, buf+4);
	           // Second package
	 	    if ((buf[1] == 2) || (buf[1] == 3)) {
		        rc = elan_ktf3k_ts_recv_data(ts->client, buf1,NEW_PACKET_SIZE);
                     if (rc < 0){
			      enable_irq(ts->client->irq);
				return;
			  }
			  elan_ktf3k_ts_report_data2(ts->client, buf1);
		    }
		
	           // Final package
		    if (buf[1] == 3) {
		        rc = elan_ktf3k_ts_recv_data(ts->client, buf2, NEW_PACKET_SIZE);
		        if (rc < 0){
		            enable_irq(ts->client->irq);
                         return;
                     }
                     elan_ktf3k_ts_report_data2(ts->client, buf2);
                 }
		    break;
             case CMD_S_PKT:
		    up(&pSem);
                 process_resp_message(ts, buf, 4);
		    break;
		default:
		    up(&pSem);	
		    touch_debug(DEBUG_INFO, "[elan] Get unknow packet {0x%02X, 0x%02X, 0x%02X, 0x%02X}\n", buf[0], buf[1], buf[2], buf[3]);
	       }		 
#endif
		enable_irq(ts->client->irq);
}

static irqreturn_t elan_ktf3k_ts_irq_handler(int irq, void *dev_id)
{
	struct elan_ktf3k_ts_data *ts = dev_id;
	struct i2c_client *client = ts->client;
	
	dev_dbg(&client->dev, "[elan] %s\n", __func__);
	disable_irq_nosync(ts->client->irq);
	queue_work(ts->elan_wq, &ts->work);

	return IRQ_HANDLED;
}

static int elan_ktf3k_ts_register_interrupt(struct i2c_client *client)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	int err = 0;

	err = request_irq(client->irq, elan_ktf3k_ts_irq_handler,
			IRQF_TRIGGER_LOW, client->name, ts);
	if (err)
		dev_err(&client->dev, "[elan] %s: request_irq %d failed\n",
				__func__, client->irq);

	return err;
}


static ssize_t elan_touch_switch_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "ELAN-%4.4x-%4.4x\n", 
		   private_ts->fw_id, private_ts->fw_ver);
}

static ssize_t elan_touch_switch_state(struct switch_dev *sdev, char *buf)
{ 
      	return sprintf(buf, "%s\n", "0");
}

#ifdef _ENABLE_DBG_LEVEL
static int ektf_proc_read(char *buffer, char **buffer_location, off_t offset, int buffer_length, int *eof, void *data )
{
	int ret;
	
	touch_debug(DEBUG_MESSAGES, "call proc_read\n");
	
	if(offset > 0)  /* we have finished to read, return 0 */
		ret  = 0;
	else 
		ret = sprintf(buffer, "Debug Level: Release Date: %s\n","2011/10/05");

	return ret;
}

static int ektf_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char procfs_buffer_size = 0; 
	int i, ret = 0;
	unsigned char procfs_buf[PROC_FS_MAX_LEN+1] = {0};
	unsigned int command;

	procfs_buffer_size = count;
	if(procfs_buffer_size > PROC_FS_MAX_LEN ) 
		procfs_buffer_size = PROC_FS_MAX_LEN+1;
	
	if( copy_from_user(procfs_buf, buffer, procfs_buffer_size) ) 
	{
		touch_debug(DEBUG_ERROR, " proc_write faied at copy_from_user\n");
		return -EFAULT;
	}

	command = 0;
	for(i=0; i<procfs_buffer_size-1; i++)
	{
		if( procfs_buf[i]>='0' && procfs_buf[i]<='9' )
			command |= (procfs_buf[i]-'0');
		else if( procfs_buf[i]>='A' && procfs_buf[i]<='F' )
			command |= (procfs_buf[i]-'A'+10);
		else if( procfs_buf[i]>='a' && procfs_buf[i]<='f' )
			command |= (procfs_buf[i]-'a'+10);
		
		if(i!=procfs_buffer_size-2)
			command <<= 4;
	}

	command = command&0xFFFFFFFF;
      switch(command){
      case 0xF1: 
	  	 gPrint_point = 1; 
	  	 break;
      case 0xF2: 
	  	 gPrint_point = 0; 
	  	 break;
      case 0xFF:
	      	ret = elan_ktf3k_ts_rough_calibrate(private_ts->client);
		break;
	}
	touch_debug(DEBUG_INFO, "Run command: 0x%08X  result:%d\n", command, ret);

	return count; // procfs_buffer_size;
}
#endif // #ifdef _ENABLE_DBG_LEV


#ifdef FIRMWARE_UPDATE_WITH_HEADER
#define FIRMWARE_PAGE_SIZE 132
static unsigned char touch_firmware[] = {
 #include "fw_data.b"
 }; 

#define SIZE_PER_PACKET 4

static int sendI2CPacket(struct i2c_client *client, const unsigned char *buf, unsigned int length){
     int ret, i;
     int retry_times = 10;
     for(i = 0; i < length; i += ret){
            ret  = i2c_master_send(client, buf + i,  length < SIZE_PER_PACKET ? length : SIZE_PER_PACKET);
	    
	    if(ret <= 0){
	          retry_times--;
		    ret = 0;
	      }
	     if(ret < (length < SIZE_PER_PACKET ? length : SIZE_PER_PACKET)){
	          touch_debug(DEBUG_INFO,"Sending packet broken\n");
		    //printk("[ektf3k]:Sending packet broken\n");	  
	     } 
	     if(retry_times < 0){
	          touch_debug(DEBUG_INFO,"Failed sending I2C touch firmware packet.\n");
		   //printk("[ektf3k]:Failed sending I2C touch firmware packet.\n");	  
	          break;
	     }
	   
     }

     return i;
}

static int recvI2CPacket(struct i2c_client *client, unsigned char *buf, unsigned int length){
     int ret, i, retry_times = 10;
     for(i = 0; i < length; i += ret){
            ret  = i2c_master_recv(client, buf + i,  length - i);
            if(ret <= 0){
	          retry_times--;
		    ret = 0;
	      }  
				
	     if(retry_times < 0){
	          touch_debug(DEBUG_INFO,"Failed sending I2C touch firmware packet.\n");
	          //printk("[ektf3k]:Failed sending I2C touch firmware packet.\n");
	          break;
	     }
     }

     return i;
}


static int firmware_update_header(struct i2c_client *client, unsigned char *firmware, unsigned int pages_number){

    int ret, i;
    int sendCount;
    int recvCount;
    int write_times; 
    unsigned char packet_data[8] = {0};
    unsigned char nb_isp_cmd[4] = {0x45, 0x49, 0x41, 0x50};
    unsigned char *cursor; 
    struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	
    if(ts == NULL) 
        return -1;

    touch_debug(DEBUG_INFO, "Start firmware update!\n");
    disable_irq(client->irq);  // Blocking call no need to do extra wait
    wake_lock(&ts->wakelock);
    work_lock = 1;
	/*add delay for waiting bootcode initial*/
	elan_ktf3k_ts_hw_reset(client, 20);
	touch_debug(DEBUG_INFO, "Send command into IAP mode\n");
	/*get into IAP mode*/
	if (sendI2CPacket(client, nb_isp_cmd, sizeof(nb_isp_cmd)) < 0)
		goto fw_update_failed;

    msleep(100);
    packet_data[0] = 0x10; 
    if(sendI2CPacket(client, packet_data, 1) < 0) // send dummy byte
	      goto fw_update_failed;
	  
    cursor = firmware;
    touch_debug(DEBUG_INFO, "pages_number=%d\n", pages_number);
    for(i = 0; i < pages_number; i++){
        write_times = 0; 
page_write_retry:
	  touch_debug(DEBUG_MESSAGES, "Update page number %d\n", i);

          
          if((sendCount = sendI2CPacket(client, cursor, FIRMWARE_PAGE_SIZE)) != FIRMWARE_PAGE_SIZE){
	      dev_err(&client->dev, "Fail to Update page number %d\n", i);
		goto fw_update_failed;
	  }
          touch_debug(DEBUG_INFO, "sendI2CPacket send %d bytes\n", sendCount);

         msleep(25);
          if((recvCount = recvI2CPacket(client, packet_data, FIRMWARE_ACK_SIZE)) != FIRMWARE_ACK_SIZE){
	      dev_err(&client->dev, "Fail to Update page number %d\n", i);
	      goto fw_update_failed;
	  }

          touch_debug(DEBUG_INFO, "recvI2CPacket recv %d bytes: %x %x\n", recvCount, packet_data[0], packet_data[1]);

	
	  if(packet_data[0] != 0xaa || packet_data[1] != 0xaa){
	      touch_debug(DEBUG_INFO, "message received: %02X %02X Page %d rewrite\n", packet_data[0], packet_data[1], i);
		if(write_times++ > 3)
		    goto fw_update_failed;
			
		goto page_write_retry;
	  }
	  
		  
	  cursor += FIRMWARE_PAGE_SIZE;
    }
	
    elan_ktf3k_ts_hw_reset(client, 0);

    /*check irq*/
    wait_for_IRQ_Low(client, 500000);/*500ms * 10*/

    if (recvI2CPacket(client, packet_data, 4) < 0)
		goto fw_update_failed;
    /*add debug message for hello packet*/
   touch_debug(DEBUG_INFO, "[elan] %s: hello packet %2x:%2X:%2x:%2x\n", __func__, packet_data[0], packet_data[1], packet_data[2], packet_data[3]);

   __fw_packet_handler(ts->client, 1);
    ret = 0;
    goto fw_update_finish;
fw_update_failed:
    ret = -1;
    touch_debug(DEBUG_INFO, "Failed the touch firmware update!\n");	
fw_update_finish:  
    work_lock = 0;
    wake_unlock(&ts->wakelock);
    enable_irq(client->irq);	
    touch_debug(DEBUG_INFO, "Finish the touch firmware update!\n");
    return ret; 
}



#endif

#ifdef TOUCH_STRESS_TEST
static void  stress_poll_data(struct work_struct * work)
{
	bool status;
	u8 count[7];
	status = elan_ktf3k_ts_recv_data(private_ts->client, count, 7);
	if(status < 0)
		printk("Read touch sensor data fail\n");

	if(poll_mode ==0)
		msleep(5);

	queue_delayed_work(stress_work_queue, &elan_poll_data_work, poll_mode);
}

int elan_stress_open(struct inode *inode, struct file *filp)
{
	printk("%s\n", __func__);
	if( !atomic_dec_and_test(&touch_char_available)){
		atomic_inc(&touch_char_available);
		return -EBUSY; /* already open */
	}
	return 0;          /* success */
}


int elan_stress_release(struct inode *inode, struct file *filp)
{
	printk("%s\n", __func__);
	atomic_inc(&touch_char_available); /* release the device */
	return 0;          /* success */
}

long elan_stress_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 1;

	printk("[elan_stress_ioctl]%d\n", cmd);
	if (_IOC_TYPE(cmd) != STRESS_IOC_MAGIC)
	return -ENOTTY;
	if (_IOC_NR(cmd) > STRESS_IOC_MAXNR)
	return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
	err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
	err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;
	switch (cmd) {
		case STRESS_POLL_DATA:
			if (arg == STRESS_IOCTL_START_HEAVY){
				printk("touch sensor heavey\n");
			poll_mode = START_HEAVY;
			queue_delayed_work(stress_work_queue, &elan_poll_data_work, poll_mode);
			}
			else if (arg == STRESS_IOCTL_START_NORMAL){
				printk("touch sensor normal\n");
				poll_mode = START_NORMAL;
				queue_delayed_work(stress_work_queue, &elan_poll_data_work, poll_mode);
			}
			else if  (arg == STRESS_IOCTL_END){
			printk("touch sensor end\n");
			cancel_delayed_work_sync(&elan_poll_data_work);
			}
			else
				return -ENOTTY;
			break;
		break;
	default: /* redundant, as cmd was checked against MAXNR */
	    return -ENOTTY;
	}
	return 0;
}

static struct file_operations stress_fops = {
		.owner =    THIS_MODULE,
		.unlocked_ioctl =	elan_stress_ioctl,
		.open =		elan_stress_open,
		.release =	elan_stress_release,
		};
#endif

static int elan_ktf3k_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err = 0;
	struct elan_ktf3k_i2c_platform_data *pdata;
	struct elan_ktf3k_ts_data *ts;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		touch_debug(DEBUG_ERROR, "[elan] %s: i2c check functionality error\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct elan_ktf3k_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		touch_debug(DEBUG_INFO, "[elan] %s: allocate elan_ktf3k_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->elan_wq = create_singlethread_workqueue("elan_wq");
	if (!ts->elan_wq) {
		touch_debug(DEBUG_ERROR, "[elan] %s: create workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}

	INIT_WORK(&ts->work, elan_ktf3k_ts_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;

	if (likely(pdata != NULL)) {
		ts->intr_gpio = pdata->intr_gpio;
		ts->rst_gpio = pdata->rst_gpio;
	}

       sema_init(&pSem, 1);
	err = elan_ktf3k_ts_setup(client);
	if (err < 0) {
		touch_debug(DEBUG_ERROR, "Main code fail\n");
		ts->status = 0;
		RECOVERY = 1;
		err = 0;
		//goto err_detect_failed;
	}
	
	ts->status = 1; // set I2C status is OK;
	wake_lock_init(&ts->wakelock, WAKE_LOCK_SUSPEND, "elan_touch");
	if(err==0x80)
	    touch_debug(DEBUG_INFO, "[ELAN] Touch is in boot mode!\n");

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev, "[elan] Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "elan-touchscreen";  

	//set_bit(BTN_TOUCH, ts->input_dev->keybit);
	ts->abs_x_max =  pdata->abs_x_max;
	ts->abs_y_max = pdata->abs_y_max;
	touch_debug(DEBUG_INFO, "[Elan] Max X=%d, Max Y=%d\n", ts->abs_x_max, ts->abs_y_max);

	input_mt_init_slots(ts->input_dev, FINGER_NUM);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, pdata->abs_y_min,  pdata->abs_y_max, 0, 0); // for 800 * 1280 
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, pdata->abs_x_min,  pdata->abs_x_max, 0, 0);// for 800 * 1280 
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, MAX_FINGER_SIZE, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, MAX_FINGER_PRESSURE, 0, 0);

	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	err = input_register_device(ts->input_dev);
	if (err) {
		dev_err(&client->dev,
			"[elan]%s: unable to register %s input device\n",
			__func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	elan_ktf3k_ts_register_interrupt(ts->client);

	if (gpio_get_value(ts->intr_gpio) == 0) {
		touch_debug(DEBUG_INFO, "[elan]%s: handle missed interrupt\n", __func__);
		elan_ktf3k_ts_irq_handler(client->irq, ts);
	}

	
#ifdef FIRMWARE_UPDATE_WITH_HEADER	
      if (RECOVERY || check_fw_version(touch_firmware, sizeof(touch_firmware), ts->fw_ver) > 0)
          firmware_update_header(client, touch_firmware, sizeof(touch_firmware)/FIRMWARE_PAGE_SIZE);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 21;
	ts->early_suspend.suspend = elan_ktf3k_ts_early_suspend;
	ts->early_suspend.resume = elan_ktf3k_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	private_ts = ts;

	//elan_ktf2k_touch_sysfs_init();
      ts->attrs.attrs = elan_attr;
	err = sysfs_create_group(&client->dev.kobj, &ts->attrs);
	if (err) {
		dev_err(&client->dev, "Not able to create the sysfs\n");
	}
	
#ifdef _ENABLE_DBG_LEVEL
	dbgProcFile = create_proc_entry(PROC_FS_NAME, 0600, NULL);
	if (dbgProcFile == NULL) 
	{
		remove_proc_entry(PROC_FS_NAME, NULL);
		touch_debug(DEBUG_INFO, " Could not initialize /proc/%s\n", PROC_FS_NAME);
	}
	else
	{
		dbgProcFile->read_proc = ektf_proc_read;
		dbgProcFile->write_proc = ektf_proc_write;
		touch_debug(DEBUG_INFO, " /proc/%s created\n", PROC_FS_NAME);
	}
#endif // #ifdef _ENABLE_DBG_LEVEL

#ifdef TOUCH_STRESS_TEST
      stress_work_queue = create_singlethread_workqueue("i2c_touchsensor_wq");
	if(!stress_work_queue){
		dev_err(&client->dev, "Unable to create stress_work_queue workqueue\n");
	}
      INIT_DELAYED_WORK(&elan_poll_data_work, stress_poll_data);
	ts->misc_dev.minor  = MISC_DYNAMIC_MINOR;
	ts->misc_dev.name = "touchpanel";
	ts->misc_dev.fops = &stress_fops;
	err = misc_register(&ts->misc_dev);
	if (err) {
	    dev_err(&client->dev, "elan stress test: Unable to register %s \\misc device\n", ts->misc_dev.name);
	}
#endif
      /* Register Switch file */
      ts->touch_sdev.name = "touch";
      ts->touch_sdev.print_name = elan_touch_switch_name;
	ts->touch_sdev.print_state = elan_touch_switch_state;
	if(switch_dev_register(&ts->touch_sdev) < 0){
		touch_debug(DEBUG_ERROR, "switch_dev_register for dock failed!\n");
		//goto exit;
	}
	switch_set_state(&ts->touch_sdev, 0);
	   
	touch_debug(DEBUG_INFO, "[elan] Start touchscreen %s in interrupt mode\n",
		ts->input_dev->name);

  ts->firmware.minor = MISC_DYNAMIC_MINOR;
  ts->firmware.name = "elan-iap";
  ts->firmware.fops = &elan_touch_fops;
  ts->firmware.mode = S_IFREG|S_IRWXUGO; 

  if (misc_register(&ts->firmware) < 0)
  	touch_debug(DEBUG_ERROR, "[ELAN]misc_register failed!!");
  else
    touch_debug(DEBUG_INFO, "[ELAN]misc_register finished!!");	

  update_power_source();
  return 0;

err_input_register_device_failed:
	if (ts->input_dev)
		input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
//err_detect_failed:
//	if (ts->elan_wq)
//		destroy_workqueue(ts->elan_wq);

err_create_wq_failed:
	kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:

	return err;
}

static int elan_ktf3k_ts_remove(struct i2c_client *client)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);

	elan_touch_sysfs_deinit();

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);

	if (ts->elan_wq)
		destroy_workqueue(ts->elan_wq);
	input_unregister_device(ts->input_dev);
	wake_lock_destroy(&ts->wakelock);
#ifdef TOUCH_STRESS_TEST
	misc_deregister(&ts->misc_dev);
#endif
	kfree(ts);
#ifdef _ENABLE_DBG_LEVEL
	remove_proc_entry(PROC_FS_NAME, NULL);
#endif
	return 0;
}

void force_release_pos(struct i2c_client *client)
{
        struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
        int i;
        for (i=0; i < FINGER_NUM; i++) {
                if (mTouchStatus[i] == 0) continue;
                input_mt_slot(ts->input_dev, i);
                input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
                mTouchStatus[i] = 0;
        }

        input_sync(ts->input_dev);
}

static int elan_ktf3k_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
	int rc = 0;

	touch_debug(DEBUG_INFO, "[elan] %s: enter\n", __func__);
	disable_irq(client->irq);
	force_release_pos(client);
	rc = cancel_work_sync(&ts->work);
	if (rc)
		enable_irq(client->irq);

	if(work_lock == 0)
	    rc = elan_ktf3k_ts_set_power_state(client, PWR_STATE_DEEP_SLEEP);

	return 0;
}

static int elan_ktf3k_ts_resume(struct i2c_client *client)
{

	int rc = 0, retry = 5;
      //struct elan_ktf3k_ts_data *ts = i2c_get_clientdata(client);
      //int delay_time;

	//gpio_direction_output(31, 0);
	  
	touch_debug(DEBUG_INFO, "[elan] %s: enter\n", __func__);
	if(work_lock == 0){
	    do {
		rc = elan_ktf3k_ts_set_power_state(client, PWR_STATE_NORMAL);
		rc = elan_ktf3k_ts_get_power_state(client);
		if (rc != PWR_NORMAL_STATE && rc != PWR_IDLE_STATE)
			touch_debug(DEBUG_ERROR,  "[elan] %s: wake up tp failed! err = %d\n",
				__func__, rc);
		else
			break;
	    } while (--retry);
	}
	//force_release_pos(client);
      enable_irq(client->irq);	
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void elan_ktf3k_ts_early_suspend(struct early_suspend *h)
{
	struct elan_ktf3k_ts_data *ts;
	ts = container_of(h, struct elan_ktf3k_ts_data, early_suspend);
	elan_ktf3k_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void elan_ktf3k_ts_late_resume(struct early_suspend *h)
{
	struct elan_ktf3k_ts_data *ts;
	ts = container_of(h, struct elan_ktf3k_ts_data, early_suspend);
	elan_ktf3k_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id elan_ktf3k_ts_id[] = {
	{ ELAN_KTF3K_NAME, 0 },
	{ }
};

static struct i2c_driver ektf3k_ts_driver = {
	.probe		= elan_ktf3k_ts_probe,
	.remove		= elan_ktf3k_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= elan_ktf3k_ts_suspend,
	.resume		= elan_ktf3k_ts_resume,
#endif
	.id_table	= elan_ktf3k_ts_id,
	.driver		= {
		.name = ELAN_KTF3K_NAME,
	},
};

static int __devinit elan_ktf3k_ts_init(void)
{
	touch_debug(DEBUG_INFO, "[elan] %s\n", __func__);
	return i2c_add_driver(&ektf3k_ts_driver);
}

static void __exit elan_ktf3k_ts_exit(void)
{
	i2c_del_driver(&ektf3k_ts_driver);
	return;
}

module_init(elan_ktf3k_ts_init);
module_exit(elan_ktf3k_ts_exit);

MODULE_DESCRIPTION("ELAN KTF3K Touchscreen Driver");
MODULE_LICENSE("GPL");
