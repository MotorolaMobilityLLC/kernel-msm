#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <asm/mach-types.h>

//default value
static int default_als_enable = 1;
static int default_als_disable = 0;
static int default_gain = 2;
static int default_ext_gain = 0;
static int default_mean_time = 4;
static int default_als_wait_time = 0;

//chip control
#define AL3320A_DRIVER_NAME    "al3320a"
#define AL3320A_DRIVER_VERSION "1.0"
#define AL3320A_CMD_SYS_CONFIG 0x00
#define AL3320A_SWRST_SHIT 2
#define AL3320A_MASK_ALS_ENABLE 0x01
#define AL3320A_CMD_ALS_WAIT 0x06
#define AL3320A_CMD_ALS_CONIFG_RANGE 0x07
#define AL3320A_GAIN_SHIT 1
#define AL3320A_CMD_ALS_MEAN_TIME 0x09
#define AL3320A_CMD_ALS_DATA_LOW 0x22
#define AL3320A_CMD_ALS_DATA_HIGH 0x23
#define AL3320A_CMD_ALS_CALIBRATION 0x34

static int al3320a_set_sys_config_als_enable(struct i2c_client *client,int enable);
static int al3320a_get_sys_config_als_enable(struct i2c_client *client);
static int al3320a_set_sys_config_soft_reset(struct i2c_client *client,int enable);
static int al3320a_set_als_waiting(struct i2c_client *client,int wait_time);
static int al3320a_get_als_waiting(struct i2c_client *client);
static int al3320a_set_als_config_range(struct i2c_client *client,int gain , int ext_gain);
static int al3320a_get_als_config_range(struct i2c_client *client);
static int al3320a_set_als_mean_time(struct i2c_client *client, int mean_time);
static int al3320a_get_als_mean_time(struct i2c_client *client);
static int al3320a_get_adc_value(struct i2c_client *client);
static int al3320a_set_als_calibration(struct i2c_client *client,int calibration);
static int al3320a_get_als_calibration(struct i2c_client *client);

//custom functions
static bool flag_probe_init_fail = false;
static int al3320a_get_lux_value(struct i2c_client *client);
static int al3320a_als_on_by_default_config(struct i2c_client *client);
static int al3320a_als_off(struct i2c_client *client);

// als calibration
#define CAL_ALS_PATH "/data/lightsensor/AL3010_Config.ini"
bool flag_al3320_is_load_cal_ini = false;
static int calibration_base_lux = 1000;
static int calibration_regs = 1235; //runtime used
static int default_calibration_regs = 1235; //default K value
static int al3320a_update_calibration(void);

//fine tune
//static bool is_poweron_after_resume = false;
//static struct timeval t_poweron_timestamp;
static int revise_lux_times = 2;


//driver polling event for HAL
static int al3320a_poll_time = 50; // ms , runtime use
static int min_poll_time = 50; // ms
struct delayed_work al3320a_report_poll_event_work;
static struct workqueue_struct *al3320a_poll_work_queue;
static struct miscdevice misc_dev;
static wait_queue_head_t poll_wait_queue_head_t;
static bool flag_pollin = true;

//+++ i2c stress test
#define AL3320A_IOC_MAGIC 0xF3
#define AL3320A_IOC_MAXNR 2
#define AL3320A_POLL_DATA _IOR(AL3320A_IOC_MAGIC,2,int )

#define AL3320A_IOCTL_START_HEAVY 2
#define AL3320A_IOCTL_START_NORMAL 1
#define AL3320A_IOCTL_END 0

#define START_NORMAL    (HZ)
#define START_HEAVY     (HZ)

static int stress_test_poll_mode=0;
struct delayed_work al3320a_stress_test_poll_work;
static struct workqueue_struct *al3320a_stress_test_work_queue;
//---

struct i2c_client *al3320a_client;

struct al3320a_data {
    struct i2c_client *client;
    struct mutex lock;
};

static int al3320a_get_adc_value(struct i2c_client *client)
{
    unsigned int data_low, data_high;

    if(!flag_al3320_is_load_cal_ini){
        if(al3320a_update_calibration()){
            printk("light sensor al3320a : calibration file update fail ! \n");
        }
        flag_al3320_is_load_cal_ini = true;
    }

    //re-init
    if(flag_probe_init_fail){
        if(al3320a_als_on_by_default_config(client)){
            printk("light sensor al3320a : re-init fail in al3320a_get_adc_value()\n");
        }
        flag_probe_init_fail = false;
    }
    data_low = i2c_smbus_read_byte_data(client, AL3320A_CMD_ALS_DATA_LOW);
    if (data_low < 0) {
        return data_low;
    }

    data_high = i2c_smbus_read_byte_data(client, AL3320A_CMD_ALS_DATA_HIGH);
    if (data_high < 0){
        return data_high;
    }

    return (u16)((data_high << 8) | data_low);
}

static int al3320a_set_sys_config_als_enable(struct i2c_client *client,int enable){
    int err = 0;
    if( enable<0 || enable>1){
        printk("light sensor al3320a : set sys config als enable fail , enable = %d\n",enable);
        return -EINVAL;
    }
    err = i2c_smbus_write_byte_data(client,AL3320A_CMD_SYS_CONFIG,enable);
    if(err){
        printk("light sensor al3320a : set sys config als enable fail , err = %d\n",err);
    }
    return err;
}

static int al3320a_get_sys_config_als_enable(struct i2c_client *client){
    int als_enable;
    als_enable = i2c_smbus_read_byte_data(client, AL3320A_CMD_SYS_CONFIG);
    if(als_enable<0){
        printk("light sensor al3320a : get sys config als enable fail , err = %d\n",als_enable);
        return als_enable;
    }
    als_enable &= AL3320A_MASK_ALS_ENABLE;
    return als_enable;
}

static int al3320a_set_sys_config_soft_reset(struct i2c_client *client,int enable){
    int err = 0;
    int als_enable = 0;
    int combo_value = 0;
    if( enable<0 || enable>1 ){
        printk("light sensor al3320a : set sys config soft reset fail , enable = %d\n",enable);
        return -EINVAL;
    }
    als_enable = al3320a_get_sys_config_als_enable(client);
    if( als_enable<0 ){
        //can NOT get als status , stop soft reset
        printk("light sensor al3320a : set sys config soft reset fail , als_enable = %d\n",als_enable);
        return als_enable;
    }
    combo_value = (enable<<AL3320A_SWRST_SHIT) | als_enable;
    err = i2c_smbus_write_byte_data(client,AL3320A_CMD_SYS_CONFIG,combo_value);
    if(err){
        printk("light sensor al3320a : set sys config soft reset fail , err = %d\n",err);
    }
    return err;
}

static int al3320a_set_als_waiting(struct i2c_client *client,int wait_time){
    int err = 0;
    if(wait_time < 0 || wait_time > 255){
        printk("light sensor al3320a : set als waiting fail , wait_time = %d\n",wait_time);
        return -EINVAL;
    }
    err = i2c_smbus_write_byte_data(client,AL3320A_CMD_ALS_WAIT,wait_time);
    if(err){
        printk("light sensor al3320a : set als waiting fail , err = %d\n",err);
    }
    return err;
}

static int al3320a_get_als_waiting(struct i2c_client *client){
    int wait_time;
    wait_time = i2c_smbus_read_byte_data(client,AL3320A_CMD_ALS_WAIT);
    if(wait_time<0){
        printk("light sensor al3320a : get als waiting fail , err = %d\n",wait_time);
    }
    return wait_time;
}

static int al3320a_set_als_config_range(struct i2c_client *client,int gain , int ext_gain){
    int err = 0;
    int combo_value = 0;
    if( gain<0 || gain>3 ){
        printk("light sensor al3320a : set als config range fail , gain = %d\n",gain);
        return -EINVAL;
    }
    if( ext_gain<0 || ext_gain>1 ){
        printk("light sensor al3320a : set als config range fail , ext gain = %d\n",ext_gain);
        return -EINVAL;
    }
    combo_value = (gain<<AL3320A_GAIN_SHIT)|ext_gain;
    err = i2c_smbus_write_byte_data(client,AL3320A_CMD_ALS_CONIFG_RANGE,combo_value);
    if(err){
        printk("light sensor al3320a : set als config range fail , err = %d\n",err);
    }
    return err;
}

static int al3320a_get_als_config_range(struct i2c_client *client){
    int als_config_range = 0;
    als_config_range = i2c_smbus_read_byte_data(client,AL3320A_CMD_ALS_CONIFG_RANGE);
    if(als_config_range<0){
        printk("light sensor al3320a : get als config range fail , err = %d\n",als_config_range);
    }
    return als_config_range;
}

static int al3320a_set_als_mean_time(struct i2c_client *client, int mean_time){
    int err = 0;
    if(mean_time<0 || mean_time>15){
        printk("light sensor al3320a : set als mean time fail , mean_time = %d\n",mean_time);
        return -EINVAL;
    }
    err = i2c_smbus_write_byte_data(client,AL3320A_CMD_ALS_MEAN_TIME,mean_time);
    if(err){
        printk("light sensor al3320a : set als mean time fail , err = %d\n",err);
    }
    return err;
}

static int al3320a_get_als_mean_time(struct i2c_client *client){
    int mean_time = 0;
    mean_time = i2c_smbus_read_byte_data(client,AL3320A_CMD_ALS_MEAN_TIME);
    if( mean_time<0 ){
        printk("light sensor al3320a : get als mean time fail , err = %d\n",mean_time);
    }
    return mean_time;
}

static int al3320a_set_als_calibration(struct i2c_client *client,int calibration){
    int err = 0;
    if( calibration<0 || calibration>255 ){
        printk("light sensor al3320a : set als calibration fail , calibration = %d\n",calibration);
        return -EINVAL;
    }
    err = i2c_smbus_write_byte_data(client,AL3320A_CMD_ALS_CALIBRATION,calibration);
    if(err){
        printk("light sensor al3320a : set als calibration fail , err = %d\n",err);
    }
    return err;
}

static int al3320a_get_als_calibration(struct i2c_client *client){
    int calibration = 0;
    calibration = i2c_smbus_read_byte_data(client,AL3320A_CMD_ALS_CALIBRATION);
    if( calibration<0 ){
        printk("light sensor al3320a : get als calibration fail , err = %d\n",calibration);
    }
    return calibration;
}

static int al3320a_get_lux_value(struct i2c_client *client){
    int adc = al3320a_get_adc_value(client);
    if( adc>=0 ){
        return (u32)( ( adc*calibration_base_lux )/calibration_regs );
    }else{
        return adc;
    }
}

static int al3320a_als_on_by_default_config(struct i2c_client *client){
    int err = 0;
    int als_on_retry_times = 3;
    int i=0;
    // als power on
    for(i=0;i<als_on_retry_times;i++){
        err = al3320a_set_sys_config_als_enable(client,default_als_enable);
        if(err){
            printk("light sensor al3320a :  %d times fail to power on \n",(i+1));
        }else{
            i = als_on_retry_times;//for break loop
        }
    }
    if(err){
        printk("light sensor al3320a : fail to power on , err = %d\n",err);
        return err;
    }
    // als config range
    for(i=0;i<als_on_retry_times;i++){
        err = al3320a_set_als_config_range(client,default_gain,default_ext_gain);
        if(err){
            printk("light sensor al3320a : %d times fail to set range \n",(i+1));
        }else{
            i = als_on_retry_times;//for break loop
        }
    }
    if(err){
        printk("light sensor al3320a : fail to set range , err = %d\n",err);
        return err;
    }
    // als mean time
    for(i=0;i<als_on_retry_times;i++){
        err = al3320a_set_als_mean_time(client,default_mean_time);
        if(err){
            printk("light sensor al3320a : %d times fail to set mean time \n",(i+1));
        }else{
            i = als_on_retry_times;//for break loop
        }
    }
    if(err){
        printk("light sensor al3320a : fail to set mean time , err = %d\n",err);
        return err;
    }
    // als waiting
    for(i=0;i<als_on_retry_times;i++){
        err = al3320a_set_als_waiting(client,default_als_wait_time);
        if(err){
            printk("light sensor al3320a : %d times fail to set waiting time \n",(i+1));
        }else{
            i = als_on_retry_times;//for break loop
        }
    }
    if(err){
        printk("light sensor al3320a : fail to set waiting time , err = %d\n",err);
    }
    return err;
}

static int al3320a_als_off(struct i2c_client *client){
    int err = 0;
    // als shutdown
    err = al3320a_set_sys_config_als_enable(client,default_als_disable);
    if(err){
        printk("light sensor al3320a : fail to shutdown , err = %d\n",err);
    }
    return err;
}

/*
 * light sensor calibration
 */
static int al3320a_update_calibration()
{
    char buf[256];
    int calibration_value = 0;
    struct file *fp = NULL;
    mm_segment_t oldfs;
    oldfs=get_fs();
    set_fs(get_ds());
    memset(buf, 0, sizeof(u8)*256);
    fp=filp_open(CAL_ALS_PATH, O_RDONLY, 0);
    if (!IS_ERR(fp)) {
        int ret = 0;
        ret = fp->f_op->read(fp, buf, sizeof(buf), &fp->f_pos);
        sscanf(buf,"%d\n", &calibration_value);
        if(calibration_value > 0){
            calibration_regs = calibration_value;
        }
        filp_close(fp, NULL);
        set_fs(oldfs);
        return 0;
    }else{
        return -1;
    }
}

/* lux */
static ssize_t al3320a_show_lux(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_lux_value(client));
}

/* reg */
static ssize_t al3320a_show_reg(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_adc_value(client));
}

/* power state */
static ssize_t al3320a_show_power_state(struct device *dev,
                     struct device_attribute *attr,
                     char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_sys_config_als_enable(client));
}

/* refresh calibration */
static ssize_t al3320a_refresh_calibration(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    printk("light sensor al3320a : refresh calibration ini \n");
    return sprintf(buf,"%d\n",al3320a_update_calibration());
}

/* revise lux */
static ssize_t al3320a_show_revise_lux(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    int tmp = al3320a_get_lux_value(client)*revise_lux_times ;
    return sprintf(buf, "%d\n", tmp);
}

/* default lux */
static ssize_t al3320a_show_default_lux(struct device *dev,
                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    int show_lux_value = al3320a_get_lux_value(client);
    int show_default_lux_value = (show_lux_value*calibration_regs)/default_calibration_regs;
    return sprintf(buf, "%d\n", show_default_lux_value);
}

/* power onoff*/
static ssize_t al3320a_show_power_onoff(struct device *dev ,
                                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_sys_config_als_enable(client));
}

static ssize_t al3320a_store_power_onoff(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    struct i2c_client *client = to_i2c_client(dev);
    long onoff;
    printk("light sensor al3320a : al3320a_store_power_onoff()\n");
    if (strict_strtol(buf, count, &onoff))
        return -EINVAL;
    if ((onoff != 1) && (onoff != 0))
        return -EINVAL;
    if(onoff == 1){
        printk("light sensor al3320a : al3320a power on\n");
        al3320a_als_on_by_default_config(client);
    }else{
        printk("light sensor al3320a : al3320a power off\n");
        al3320a_als_off(client);
    }
    return strnlen(buf, count);
}

/* sensor enable */
static ssize_t al3320a_enable_sensor_polling(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    long enable;
    printk("light sensor al3320a : al3320a_enable_sensor_polling()\n");
    if (strict_strtol(buf, 10, &enable))
        return -EINVAL;
    if ((enable != 1) && (enable != 0))
        return -EINVAL;

    if(enable == 1){
        printk("light sensor al3320a : al3320a poll enable\n");
        flag_pollin = true;
        queue_delayed_work(al3320a_poll_work_queue, &al3320a_report_poll_event_work, al3320a_poll_time);
    }else{
        printk("light sensor al3320a : al3320a poll disable\n");
        cancel_delayed_work_sync(&al3320a_report_poll_event_work);
    }
    return strnlen(buf, count);
}

/* set delay time */
static ssize_t al3320a_set_delay(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    long delay_time;
    if (strict_strtol(buf, count, &delay_time))
        return -EINVAL;

    if(delay_time<min_poll_time){
        al3320a_poll_time = min_poll_time;
    }else{
        al3320a_poll_time = delay_time;
    }
    printk("light sensor al3320a : al3320a_poll_time = %d\n",al3320a_poll_time);
    return strnlen(buf, count);
}

/* get delay time */
static ssize_t al3320a_get_delay(struct device *dev ,
                                 struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", al3320a_poll_time);
}

/* set soft reset */
static ssize_t al3320a_store_sys_config_soft_reset(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    struct i2c_client *client = to_i2c_client(dev);
    long do_soft_reset;
    printk("light sensor al3320a : al3320a_store_sys_config_soft_reset()\n");
    if(strict_strtol(buf,count,&do_soft_reset)){
        return -EINVAL;
    }
    if(do_soft_reset==1){
        printk("light sensor al3320a : set sys config soft reset \n");
        al3320a_set_sys_config_soft_reset(client,1);
        return strnlen(buf,count);
    }else{
        return -EINVAL;
    }
}

/* store als waiting */
static ssize_t al3320a_store_als_waiting(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    struct i2c_client *client = to_i2c_client(dev);
    long waiting;
    int err = 0;
    printk("light sensor al3320a : al3320a_store_als_waiting()\n");
    if (strict_strtol(buf, count, &waiting)){
        return -EINVAL;
    }
    printk("light sensor al3320a : set als waiting = %ld \n",waiting);
    err = al3320a_set_als_waiting(client,waiting);
    if(err){
        return err;
    }
    return strnlen(buf, count);
}

/* show als waiting */
static ssize_t al3320a_show_als_waiting(struct device *dev ,
                                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_als_waiting(client));
}

/* store als range */
static ssize_t al3320a_store_als_config_range(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    struct i2c_client *client = to_i2c_client(dev);
    long config_range;
    int err = 0;
    int gain = 0;
    int ext_gain = 0;
    printk("light sensor al3320a : al3320a_store_als_config_range()\n");
    if (strict_strtol(buf, count, &config_range)){
        return -EINVAL;
    }
    gain = config_range>>1;
    ext_gain = config_range & 1;
    printk("light sensor al3320a : set als config range = %ld , gain =%d , ext_gain = %d \n",config_range,gain,ext_gain);
    err = al3320a_set_als_config_range(client,gain,ext_gain);
    if(err){
        return err;
    }
    return strnlen(buf, count);
}

/* show als range */
static ssize_t al3320a_show_als_config_range(struct device *dev ,
                                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_als_config_range(client));
}

/* store als mean time */
static ssize_t al3320a_store_als_mean_time(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    struct i2c_client *client = to_i2c_client(dev);
    long mean_time;
    int err = 0;
    printk("light sensor al3320a : al3320a_store_als_mean_time()\n");
    if (strict_strtol(buf, count, &mean_time)){
        return -EINVAL;
    }
    printk("light sensor al3320a : set als mean time = %ld \n",mean_time);
    err = al3320a_set_als_mean_time(client,mean_time);
    if(err){
        return err;
    }
    return strnlen(buf, count);
}

/* show als mean time */
static ssize_t al3320a_show_als_mean_time(struct device *dev ,
                                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_als_mean_time(client));
}

/* store als calibration */
static ssize_t al3320a_store_als_calibration(struct device *dev, struct device_attribute *attr,
        const char *buf , size_t count){
    struct i2c_client *client = to_i2c_client(dev);
    long calibration;
    int err = 0;
    printk("light sensor al3320a : al3320a_store_als_calibration()\n");
    if (strict_strtol(buf, count, &calibration)){
        return -EINVAL;
    }
    printk("light sensor al3320a : set als calibration = %ld \n",calibration);
    err = al3320a_set_als_calibration(client,calibration);
    if(err){
        return err;
    }
    return strnlen(buf, count);
}

/* show als calibration */
static ssize_t al3320a_show_als_calibration(struct device *dev ,
                                 struct device_attribute *attr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    return sprintf(buf, "%d\n", al3320a_get_als_calibration(client));
}


static SENSOR_DEVICE_ATTR(show_reg, 0644, al3320a_show_reg, NULL, 1);
static SENSOR_DEVICE_ATTR(show_lux, 0644, al3320a_show_lux, NULL, 2);
static SENSOR_DEVICE_ATTR(lightsensor_status, 0666, al3320a_show_power_state, al3320a_enable_sensor_polling, 3);
static SENSOR_DEVICE_ATTR(refresh_cal, 0644, al3320a_refresh_calibration, NULL, 4);
static SENSOR_DEVICE_ATTR(show_revise_lux, 0644, al3320a_show_revise_lux, NULL, 5);
static SENSOR_DEVICE_ATTR(show_default_lux, 0644, al3320a_show_default_lux, NULL, 6);
static SENSOR_DEVICE_ATTR(power_onoff,0644,al3320a_show_power_onoff,al3320a_store_power_onoff,7);
static SENSOR_DEVICE_ATTR(poll_time,0666,al3320a_get_delay,al3320a_set_delay,8);
static SENSOR_DEVICE_ATTR(store_soft_reset,0644,NULL,al3320a_store_sys_config_soft_reset,9);
static SENSOR_DEVICE_ATTR(als_waiting,0644,al3320a_show_als_waiting,al3320a_store_als_waiting,10);
static SENSOR_DEVICE_ATTR(als_config_range,0644,al3320a_show_als_config_range,al3320a_store_als_config_range,11);
static SENSOR_DEVICE_ATTR(als_mean_time,0644,al3320a_show_als_mean_time,al3320a_store_als_mean_time,12);
static SENSOR_DEVICE_ATTR(als_calibration,0644,al3320a_show_als_calibration,al3320a_store_als_calibration,13);

static struct attribute *al3320a_attributes[] = {
    &sensor_dev_attr_show_reg.dev_attr.attr,
    &sensor_dev_attr_show_lux.dev_attr.attr,
    &sensor_dev_attr_lightsensor_status.dev_attr.attr,
    &sensor_dev_attr_refresh_cal.dev_attr.attr,
    &sensor_dev_attr_show_revise_lux.dev_attr.attr,
    &sensor_dev_attr_show_default_lux.dev_attr.attr,
    &sensor_dev_attr_power_onoff.dev_attr.attr,
    &sensor_dev_attr_poll_time.dev_attr.attr,
    &sensor_dev_attr_store_soft_reset.dev_attr.attr,
    &sensor_dev_attr_als_waiting.dev_attr.attr,
    &sensor_dev_attr_als_config_range.dev_attr.attr,
    &sensor_dev_attr_als_mean_time.dev_attr.attr,
    &sensor_dev_attr_als_calibration.dev_attr.attr,
    NULL
};

static const struct attribute_group al3320a_attr_group = {
    .attrs = al3320a_attributes,
};

static void  al3320a_report_poll_event(struct work_struct * work)
{
    //wake up device poll wait queue
    flag_pollin = true;
    wake_up_interruptible(&poll_wait_queue_head_t);
    //printk("light sensor al3320a : %s\n", __func__);
    //add next work for polling
    queue_delayed_work(al3320a_poll_work_queue, &al3320a_report_poll_event_work, al3320a_poll_time);
}

static void al3320a_stress_test_poll(struct work_struct * work)
{
    al3320a_get_adc_value(al3320a_client);
    if(stress_test_poll_mode ==0)
        msleep(5);
    queue_delayed_work(al3320a_stress_test_work_queue, &al3320a_stress_test_poll_work, stress_test_poll_mode);
}

int al3320a_open(struct inode *inode, struct file *filp)
{
    printk("light sensor al3320a : %s\n", __func__);
    return 0;
}

int al3320a_release(struct inode *inode, struct file *filp)
{
    printk("light sensor al3320a : %s\n", __func__);
    return 0;
}

static unsigned int al3320a_poll(struct file *filp, poll_table *wait){
    unsigned int mask = 0;
    poll_wait(filp, &poll_wait_queue_head_t, wait);
    if(flag_pollin==true){
        mask |= POLLIN;
        flag_pollin=false;
    }
    //printk("light sensor al3320a : %s\n", __func__);
    return mask;
}

long al3320a_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int err = 1;
    if (_IOC_TYPE(cmd) != AL3320A_IOC_MAGIC)
    return -ENOTTY;
    if (_IOC_NR(cmd) > AL3320A_IOC_MAXNR)
    return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (err)
        return -EFAULT;

    switch (cmd) {
        case AL3320A_POLL_DATA:
            if (arg == AL3320A_IOCTL_START_HEAVY){
                printk("light sensor al3320a : ioctl heavy\n");
                stress_test_poll_mode = START_HEAVY;
                queue_delayed_work(al3320a_stress_test_work_queue, &al3320a_stress_test_poll_work,
                        stress_test_poll_mode);
            }
            else if (arg == AL3320A_IOCTL_START_NORMAL){
                printk("light sensor al3320a : ioctl normal\n");
                stress_test_poll_mode = START_NORMAL;
                queue_delayed_work(al3320a_stress_test_work_queue, &al3320a_stress_test_poll_work,
                        stress_test_poll_mode);
            }
            else if  (arg == AL3320A_IOCTL_END){
                printk("light sensor al3320a : ioctl end\n");
                cancel_delayed_work_sync(&al3320a_stress_test_poll_work);
            }
            else
                return -ENOTTY;
            break;
        default: /* redundant, as cmd was checked against MAXNR */
            return -ENOTTY;
    }

    return 0;
}

struct file_operations al3320a_fops = {
    .owner = THIS_MODULE,
    .open = al3320a_open,
    .release = al3320a_release,
    .poll = al3320a_poll,
    .unlocked_ioctl = al3320a_ioctl,
};

static int al3320a_probe(struct i2c_client *client,
                    const struct i2c_device_id *id)
{
    struct al3320a_data *data;
    int err = 0;

    data = kzalloc(sizeof(struct al3320a_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    i2c_set_clientdata(client, data);
    mutex_init(&data->lock);

    /* al3320a init */
    err = al3320a_als_on_by_default_config(client);
    if(err){
        printk("light sensor al3320a : als init fail in probe! \n");
        flag_probe_init_fail = true;
    }

    /* register sysfs hooks */
    err = sysfs_create_group(&client->dev.kobj, &al3320a_attr_group);
    if (err){
        printk("light sensor al3320a : init sysfs fail\n");
        goto exit_kfree;
    }
    al3320a_client = client;

    //poll event
    al3320a_poll_work_queue = create_singlethread_workqueue("lightsensor_poll_wq");
    if(!al3320a_poll_work_queue){
        printk("light sensor al3320a : unable to create poll workqueue\n");
        goto remove_sysfs_group;
    }
    INIT_DELAYED_WORK(&al3320a_report_poll_event_work, al3320a_report_poll_event);

    //stress test
    al3320a_stress_test_work_queue = create_singlethread_workqueue("i2c_lightsensor_wq");
    if(!al3320a_stress_test_work_queue){
        printk("light sensor al3320a : unable to create i2c stress test workqueue\n");
        goto destroy_wq;
    }
    INIT_DELAYED_WORK(&al3320a_stress_test_poll_work, al3320a_stress_test_poll);
    //misc device for HAL poll
    misc_dev.minor = MISC_DYNAMIC_MINOR;
    misc_dev.name = "lightsensor";
    misc_dev.fops  = &al3320a_fops;
    init_waitqueue_head(&poll_wait_queue_head_t);
    err = misc_register(&misc_dev);
    if(err){
        printk("light sensor al3320a : register misc dev fail\n");
        goto destroy_stress_test_wq;
    }
    printk("light sensor al3320a : probe success\n");
    return 0;

destroy_stress_test_wq:
    destroy_workqueue(al3320a_stress_test_work_queue);
destroy_wq:
    destroy_workqueue(al3320a_poll_work_queue);
remove_sysfs_group:
    sysfs_remove_group(&client->dev.kobj, &al3320a_attr_group);
exit_kfree:
    kfree(data);
    return err;
}



#ifdef CONFIG_PM
static int al3320a_suspend(struct device *dev)
{
    int ret = 0;
    struct i2c_client *client = to_i2c_client(dev);
    printk("al3320a_suspend+\n");
    ret = al3320a_als_off(client);
    if(ret){
        printk("light sensor al3320a : power off fail \n");
    }
    printk("al3320a_suspend-\n");
    return 0;// DO NOT return err , cause system fail
}

static int al3320a_resume(struct device *dev)
{
    int ret=0;
    struct i2c_client *client = to_i2c_client(dev);
    printk("al3320a_resume+\n");
    ret = al3320a_als_on_by_default_config(client);
    if(ret){
        printk("light sensor al3320a : power on fail \n");
    }
    printk("al3320a_resume-\n");
    return 0;// DO NOT return err , cause system fail
}

static const struct dev_pm_ops al3320a_pm_ops = {
    .suspend = al3320a_suspend,
    .resume  = al3320a_resume,
};
#define AL3320A_PM_OPS (&al3320a_pm_ops)

#else
#define AL3320A_PM_OPS NULL
#endif /* CONFIG_PM */


static int al3320a_remove(struct i2c_client *client)
{
    misc_deregister(&misc_dev);
    sysfs_remove_group(&client->dev.kobj, &al3320a_attr_group);
    al3320a_als_on_by_default_config(client);
    kfree(i2c_get_clientdata(client));
    printk("light sensor al3320a : al3320a remove successed\n");
    return 0;
}

static const struct i2c_device_id al3320a_id[] = {
    { "al3320a", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, al3320a_id);

static struct i2c_driver al3320a_driver = {
    .driver = {
        .name    = AL3320A_DRIVER_NAME,
        .owner    = THIS_MODULE,
        .pm = AL3320A_PM_OPS,
    },
    .probe    = al3320a_probe,
    .remove    = al3320a_remove,
    .id_table = al3320a_id,
};

static int __init al3320a_init(void)
{
    int ret=0;
    printk(KERN_INFO "%s+ #####\n", __func__);
    printk("light sensor al3320a : module init \n");
    ret = i2c_add_driver(&al3320a_driver);
    printk(KERN_INFO "%s- #####\n", __func__);
    return ret;
}

static void __exit al3320a_exit(void)
{
    printk("light sensor al3320a : module exit \n");
    i2c_del_driver(&al3320a_driver);
}

MODULE_AUTHOR("asus");
MODULE_DESCRIPTION("al3320a driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(AL3320A_DRIVER_VERSION);

module_init(al3320a_init);
module_exit(al3320a_exit);

