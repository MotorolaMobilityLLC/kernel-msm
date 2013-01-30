/*
 * An I2C driver for SMSC Proximity Sensor CAP1106.
 *
 * Copyright (c) 2012, ASUSTek Corporation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/earlysuspend.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/gpio.h>
#include <linux/input.h>

#include <asm/mach-types.h>

MODULE_DESCRIPTION("SMSC Proximity Sensor CAP1106 Driver");
MODULE_LICENSE("GPL");

/*----------------------------------------------------------------------------
 ** Debug Utility
 **----------------------------------------------------------------------------*/
#define PROX_SENSOR_DEBUG 0
#define PROX_SENSOR_VERBOSE_DEBUG 0

#if PROX_SENSOR_DEBUG
#define PROX_DEBUG(format, arg...)	\
    printk(KERN_INFO "CAP1106: [%s] " format , __FUNCTION__ , ## arg)
#else
#define PROX_DEBUG(format, arg...)
#endif

#define PROX_INFO(format, arg...)	\
    printk(KERN_INFO "CAP1106: [%s] " format , __FUNCTION__ , ## arg)
#define PROX_ERROR(format, arg...)	\
    printk(KERN_ERR "CAP1106: [%s] " format , __FUNCTION__ , ## arg)

#define SAR_DET_3G 52
#define NAME_RIL_PROX "ril_proximity"

/*----------------------------------------------------------------------------
 ** Global Variable
 **----------------------------------------------------------------------------*/
struct cap1106_data {
    struct attribute_group attrs;
    struct i2c_client *client;
    struct delayed_work work;
    struct delayed_work checking_work;
    int enable;
    int obj_detect;
    int overflow_status;
};

static DEFINE_MUTEX(prox_mtx);
static struct cap1106_data *prox_data;
static struct workqueue_struct *prox_wq;
static struct switch_dev prox_sdev;
static long checking_work_period = 100; //default (ms)
static int is_wood_sensitivity = 0;
static int ac2 = 0; // Accumulated Count Ch2
static int ac3 = 0; // Accumulated Count Ch3
static int ac6 = 0; // Accumulated Count Ch6
static int ac_limit = 10;
static int force_enable = 1;

/*----------------------------------------------------------------------------
 ** FUNCTION DECLARATION
 **----------------------------------------------------------------------------*/
static int __devinit cap1106_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devexit cap1106_remove(struct i2c_client *client);
static int cap1106_suspend(struct i2c_client *client, pm_message_t mesg);
static int cap1106_resume(struct i2c_client *client);
static s32 cap1106_read_reg(struct i2c_client *client, u8 command);
static s32 cap1106_write_reg(struct i2c_client *client, u8 command, u8 value);
static int __init cap1106_init(void);
static void __exit cap1106_exit(void);
static irqreturn_t cap1106_interrupt_handler(int irq, void *dev);
static void cap1106_work_function(struct work_struct *work);
static int cap1106_init_sensor(struct i2c_client *client);
static int cap1106_config_irq(struct i2c_client *client);
static void cap1106_enable_sensor(struct i2c_client *client, int enable);
static ssize_t show_attrs_handler(struct device *dev, struct device_attribute *devattr, char *buf);
static ssize_t store_attrs_handler(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

/*----------------------------------------------------------------------------
 ** I2C Driver Structure
 **----------------------------------------------------------------------------*/
static const struct i2c_device_id cap1106_id[] = {
    {"cap1106", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, cap1106_id);

static struct i2c_driver cap1106_driver = {
    .driver = {
        .name	= "cap1106",
        .owner	= THIS_MODULE,
    },
    .probe		= cap1106_probe,
    .remove		= __devexit_p(cap1106_remove),
    .resume     = cap1106_resume,
    .suspend    = cap1106_suspend,
    .id_table	= cap1106_id,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Device Attributes Sysfs Show/Store
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DEVICE_ATTR(obj_detect, 0644, show_attrs_handler, NULL);
DEVICE_ATTR(sensitivity, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_gain, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_input_2_delta_count, 0644, show_attrs_handler, NULL);
DEVICE_ATTR(sensor_input_2_th, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_input_6_delta_count, 0644, show_attrs_handler, NULL);
DEVICE_ATTR(sensor_input_6_th, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_input_noise_th, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_input_status, 0644, show_attrs_handler, NULL);
DEVICE_ATTR(sensing_cycle, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_onoff, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_recal, 0644, show_attrs_handler, store_attrs_handler);
DEVICE_ATTR(sensor_input_3_delta_count, 0644, show_attrs_handler, NULL);
DEVICE_ATTR(sensor_input_3_th, 0644, show_attrs_handler, store_attrs_handler);

static struct attribute *cap1106_attr_deb[] = {
    &dev_attr_obj_detect.attr,                  // 1
    &dev_attr_sensitivity.attr,                 // 2
    &dev_attr_sensor_gain.attr,                 // 3
    &dev_attr_sensor_input_2_delta_count.attr,  // 4
    &dev_attr_sensor_input_2_th.attr,           // 5
    &dev_attr_sensor_input_6_delta_count.attr,  // 6
    &dev_attr_sensor_input_6_th.attr,           // 7
    &dev_attr_sensor_input_noise_th.attr,       // 8
    &dev_attr_sensor_input_status.attr,         // 9
    &dev_attr_sensing_cycle.attr,               // 10
    &dev_attr_sensor_onoff.attr,                // 11
    &dev_attr_sensor_recal.attr,                // 12
    &dev_attr_sensor_input_3_delta_count.attr,  // 13
    &dev_attr_sensor_input_3_th.attr,           // 14
    NULL
};

static ssize_t show_attrs_handler(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct cap1106_data *data = i2c_get_clientdata(client);
    const char *attr_name = devattr->attr.name;
    int ret = -1;

    PROX_DEBUG("=== devattr->attr->name: %s\n", devattr->attr.name);

    mutex_lock(&prox_mtx);
    if (data->enable) {
        if (!strcmp(attr_name, dev_attr_obj_detect.attr.name)) {                        // 1
            ret = sprintf(buf, "%d\n", data->obj_detect);
        } else if (!strcmp(attr_name, dev_attr_sensitivity.attr.name)) {                // 2
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x1F));
        } else if (!strcmp(attr_name, dev_attr_sensor_gain.attr.name)) {                // 3
            ret = sprintf(buf, "%02X\n", (cap1106_read_reg(client, 0x00) & 0xC0) >> 6);
        } else if (!strcmp(attr_name, dev_attr_sensor_input_2_delta_count.attr.name)) { // 4
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x11));
        } else if (!strcmp(attr_name, dev_attr_sensor_input_2_th.attr.name)) {          // 5
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x31));
        } else if (!strcmp(attr_name, dev_attr_sensor_input_6_delta_count.attr.name)) { // 6
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x15));
        } else if (!strcmp(attr_name, dev_attr_sensor_input_6_th.attr.name)) {          // 7
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x35));
        } else if (!strcmp(attr_name, dev_attr_sensor_input_noise_th.attr.name)) {      // 8
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x38) & 0x3);
        } else if (!strcmp(attr_name, dev_attr_sensor_input_status.attr.name)) {        // 9
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x03));
        } else if (!strcmp(attr_name, dev_attr_sensing_cycle.attr.name)) {              // 10
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x24));
        } else if (!strcmp(attr_name, dev_attr_sensor_onoff.attr.name)) {               // 11
            ret = sprintf(buf, "%d\n", data->enable);
        } else if (!strcmp(attr_name, dev_attr_sensor_recal.attr.name)) {               // 12
            ret = sprintf(buf, cap1106_read_reg(client, 0x26) == 0x0 ? "OK\n" : "FAIL\n");
        } else if (!strcmp(attr_name, dev_attr_sensor_input_3_delta_count.attr.name)) { // 13
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x12));
        } else if (!strcmp(attr_name, dev_attr_sensor_input_3_th.attr.name)) {          // 14
            ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x32));
        }
    } else {
        ret = sprintf(buf, "SENSOR DISABLED\n");
    }
    mutex_unlock(&prox_mtx);

    return ret;
}

static ssize_t store_attrs_handler(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct cap1106_data *data = i2c_get_clientdata(client);
    const char *attr_name = devattr->attr.name;
    unsigned long value;

    if (strict_strtoul(buf, 16, &value))
        return -EINVAL;

    PROX_DEBUG("=== devattr->attr->name: %s, value: %lu\n", devattr->attr.name, value);

    mutex_lock(&prox_mtx);
    if (data->enable) {
        if (!strcmp(attr_name, dev_attr_sensitivity.attr.name)) { // 2
            cap1106_write_reg(client, 0x1F, value & 0x7F);
        } else if (!strcmp(attr_name, dev_attr_sensor_gain.attr.name)) { // 3
            cap1106_write_reg(client, 0x00, (cap1106_read_reg(client, 0x00) & 0x3F) | ((cap1106_read_reg(client, 0x00) & 0x3) << 6));
        } else if (!strcmp(attr_name, dev_attr_sensor_input_2_th.attr.name)) { // 5
            cap1106_write_reg(client, 0x31, value & 0x7F);
        } else if (!strcmp(attr_name, dev_attr_sensor_input_6_th.attr.name)) { // 7
            cap1106_write_reg(client, 0x35, value & 0x7F);
        } else if (!strcmp(attr_name, dev_attr_sensor_input_noise_th.attr.name)) { // 8
            cap1106_write_reg(client, 0x38, value & 0x3);
        } else if (!strcmp(attr_name, dev_attr_sensing_cycle.attr.name)) { // 10
            cap1106_write_reg(client, 0x24, value & 0x7F);
        } else if (!strcmp(attr_name, dev_attr_sensor_onoff.attr.name)) { // 11
            if ((value == 0) || (value == 1)) {
                force_enable = value;
                cap1106_enable_sensor(client, value);
            }
        } else if (!strcmp(attr_name, dev_attr_sensor_recal.attr.name)) { // 12
            cap1106_write_reg(client, 0x26, 0x22);
        } else if (!strcmp(attr_name, dev_attr_sensor_input_3_th.attr.name)) { // 14
            cap1106_write_reg(client, 0x32, value & 0x7F);
        }
    } else {
        if (!strcmp(attr_name, dev_attr_sensor_onoff.attr.name)) { // 11
            if ((value == 0) || (value == 1)) {
                force_enable = value;
                cap1106_enable_sensor(client, value);
            }
        }
    }
    mutex_unlock(&prox_mtx);

    return strnlen(buf, count);;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks for switch device
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t print_prox_name(struct switch_dev *sdev, char *buf)
{
    return sprintf(buf, "%s\n", "prox_sar_det");
}

static ssize_t print_prox_state(struct switch_dev *sdev, char *buf)
{
    int state = -1;
    if (switch_get_state(sdev))
        state = 1;
    else
        state = 0;

    return sprintf(buf, "%d\n", state);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if PROX_SENSOR_VERBOSE_DEBUG
static void dump_registers(struct i2c_client *client)
{
    int value;
    value = cap1106_read_reg(client, 0x00);
    PROX_ERROR("=== Main Control(0x00) is %x\n", value);
    value = cap1106_read_reg(client, 0x02);
    PROX_ERROR("=== Genaral Status(0x02) is %x\n", value);
    value = cap1106_read_reg(client, 0x03);
    PROX_ERROR("=== Sensor Input Status(0x03) is %x\n", value);
    value = cap1106_read_reg(client, 0x0A);
    PROX_ERROR("=== Noise Flag Status(0x0A) is %x\n", value);
    value = cap1106_read_reg(client, 0x21);
    PROX_ERROR("=== Sensor Input Enable Register(0x21) is %x\n", value);
    value = cap1106_read_reg(client, 0x44);
    PROX_ERROR("=== configuration 2(0x44) is %x\n", value);
    value = cap1106_read_reg(client, 0xFD);
    PROX_ERROR("=== Product ID(0xFD) is %x\n", value);
    value = cap1106_read_reg(client, 0xFE);
    PROX_ERROR("=== Manufacturer ID(0xFE) is %x\n", value);
    value = cap1106_read_reg(client, 0xFF);
    PROX_ERROR("=== Revision (0xFF) is %x\n", value);
}
#endif

static void cap1106_enable_sensor(struct i2c_client *client, int enable)
{
    long reg_value;

    struct cap1106_data *data = i2c_get_clientdata(client);

    if (data->enable != enable) {
        reg_value = cap1106_read_reg(client, 0x00);
        if (enable) {
            cap1106_write_reg(client, 0x00, (reg_value & 0xEF) | (!enable << 4));
            // Time to first conversion is 200ms (Max)
            queue_delayed_work(prox_wq, &data->work, msecs_to_jiffies(200));
            enable_irq(client->irq);
            queue_delayed_work(prox_wq, &prox_data->checking_work, checking_work_period);
        } else {
            disable_irq(client->irq);
            cancel_delayed_work_sync(&data->work);
            cancel_delayed_work_sync(&data->checking_work);
            flush_workqueue(prox_wq);
            switch_set_state(&prox_sdev, 0);
            cap1106_write_reg(client, 0x00, (reg_value & 0xEF) | (!enable << 4));
        }
        data->enable = enable;
    }
}

static s32 cap1106_read_reg(struct i2c_client *client, u8 command)
{
    return i2c_smbus_read_byte_data(client, command);
}

static s32 cap1106_write_reg(struct i2c_client *client, u8 command, u8 value)
{
    return i2c_smbus_write_byte_data(client, command, value);
}

static void cap1106_work_function(struct work_struct *work)
{
    int status;
    int dc2, dc3, dc6; // Delta Count Ch2, Ch3, Ch6
    int bc2, bc3, bc6; // Base Count Ch2, Ch3, Ch6
    struct cap1106_data *data = container_of((struct delayed_work *)work, struct cap1106_data, work);

    disable_irq(data->client->irq);
    cap1106_write_reg(data->client, 0x00, 0x80); // Clear INT and Set Gain to MAX
    status = cap1106_read_reg(data->client, 0x03);
    dc2 = cap1106_read_reg(prox_data->client, 0x11);
    dc3 = cap1106_read_reg(prox_data->client, 0x12);
    dc6 = cap1106_read_reg(prox_data->client, 0x15);
    bc2 = cap1106_read_reg(prox_data->client, 0x51);
    bc3 = cap1106_read_reg(prox_data->client, 0x52);
    bc6 = cap1106_read_reg(prox_data->client, 0x55);
    PROX_DEBUG("Status: 0x%02X, BC2=0x%02X, DC2=0x%02X, BC3=0x%02X, DC3=0x%02X, BC6=0x%02X, DC6=0x%02X\n", status, bc2, dc2, bc3, dc3, bc6, dc6);
    if (is_wood_sensitivity == 0) {
        if (machine_is_apq8064_deb()) {
            data->obj_detect = status & 0x26 ? 1 : 0;
            switch_set_state(&prox_sdev, data->obj_detect);
            if ((status == 0x02 && dc2 == 0x7F)
                    || (status == 0x04 && dc3 == 0x7F)
                    || (status == 0x20 && dc6 == 0x7F)
                    || (status == 0x06 && (dc2 == 0x7F || dc3 == 0x7F))
                    || (status == 0x22 && (dc2 == 0x7F || dc6 == 0x7F))
                    || (status == 0x24 && (dc3 == 0x7F || dc6 == 0x7F))
                    || (status == 0x26 && (dc2 == 0x7F || dc3 == 0x7F || dc6 == 0x7F))) {
                PROX_DEBUG("Deb, set to wood sensitivity------>\n");
                //set sensitivity and threshold for wood touch
                cap1106_write_reg(prox_data->client, 0x1f, 0x4f);
                cap1106_write_reg(prox_data->client, 0x31, 0x50);
                cap1106_write_reg(prox_data->client, 0x32, 0x50);
                cap1106_write_reg(prox_data->client, 0x35, 0x50);
                is_wood_sensitivity = 1;
                data->overflow_status = status;
                ac2 = 0;
                ac3 = 0;
                ac6 = 0;
            } else {
                if (dc2 >= 0x08 && dc2 <= 0x3F)
                    ac2++;
                if (dc3 >= 0x08 && dc3 <= 0x3F)
                    ac3++;
                if (dc6 >= 0x0a && dc6 <= 0x3F)
                    ac6++;

                PROX_DEBUG("Deb, ac2=%d, ac3=%d, ac6=%d\n", ac2, ac3, ac6);
                if (ac2 >= ac_limit || ac3 >= ac_limit || ac6 >= ac_limit) {
                    PROX_DEBUG("Deb, +++ FORCE RECALIBRATION +++\n");
                    cap1106_write_reg(data->client, 0x26, 0x26);
                    ac2 = 0;
                    ac3 = 0;
                    ac6 = 0;
                }
            }
        }
    }
    enable_irq(data->client->irq);
}

static irqreturn_t cap1106_interrupt_handler(int irq, void *dev)
{
    struct cap1106_data *data = i2c_get_clientdata(dev);
    queue_delayed_work(prox_wq, &data->work, 0);
    return IRQ_HANDLED;
}

static int cap1106_config_irq(struct i2c_client *client)
{
    int rc = 0 ;
    unsigned gpio = SAR_DET_3G;
    const char *label = "cap1106_alert";

    rc = gpio_request(gpio, label);
    if (rc) {
        PROX_ERROR("%s: gpio_request failed for gpio %s\n", __func__, "SAR_DET_3G");
        goto err_gpio_request_failed;
    }

    rc = gpio_direction_input(gpio) ;
    if (rc) {
        PROX_ERROR("%s: gpio_direction_input failed for gpio %s\n", __func__, "SAR_DET_3G");
        goto err_gpio_direction_input_failed;
    }

    rc = request_irq(client->irq, cap1106_interrupt_handler, IRQF_TRIGGER_FALLING, label, client);
    if(rc){
        PROX_ERROR("Could not register for %s interrupt, irq = %d, rc = %d\n", label, client->irq, rc);
        goto err_gpio_request_irq_failed;
    }

    PROX_INFO("GPIO=0x%02X, IRQ=0x%02X, SAR_DET_3G=0x%02X\n", gpio, client->irq, gpio_get_value(SAR_DET_3G));

#if PROX_SENSOR_VERBOSE_DEBUG
    dump_registers(client);
#endif

    return 0 ;
err_gpio_request_irq_failed:
err_gpio_direction_input_failed:
    gpio_free(gpio);
err_gpio_request_failed:
    return rc;
}

static int cap1106_init_sensor(struct i2c_client *client)
{
    u8 bIdx;
    int rc = 0;
    const u8 init_table_deb[] = {
        0x1f, 0x1f, // Data sensitivity (need to be fine tune for real system).
        0x20, 0x20, // MAX duration disable
        0x21, 0x26, // Enable CS2+CS3+CS6.
        0x22, 0xff, // MAX duration time to max , repeat period time to max
        0x24, 0x39, // digital count update time to 140*64ms
        0x27, 0x26, // Enable INT. for CS2+CS3+CS6.
        0x28, 0x22, // disable repeat irq
        0x2a, 0x00, // all channel run in the same time
        0x31, 0x08, // Threshold of CS 2 (need to be fine tune for real system).
        0x32, 0x0a, // Threshold of CS 3 (need to be fine tune for real system).
        0x35, 0x0a, // Threshold of CS 6 (need to be fine tune for real system).
        0x26, 0x22, // force re-cal
        0x00, 0x00, // Reset INT. bit.
    };

    PROX_DEBUG("NAME: %s, ADDR: 0x%X\n", client->name, client->addr);

    if (machine_is_apq8064_deb()) {
        for (bIdx = 0; bIdx < sizeof(init_table_deb) / sizeof(init_table_deb[0]); bIdx += 2) {
            if ((rc = cap1106_write_reg(client, init_table_deb[bIdx],
                    init_table_deb[bIdx + 1]))) {
                PROX_ERROR("=== Deb, Write Error, rc=0x%X\n", rc);
                break;
            }
        }
    }

#if PROX_SENSOR_VERBOSE_DEBUG
    dump_registers(client);
#endif

    return rc;
}

static void cap1106_checking_work_function(struct work_struct *work) {
    int status;
    int dc2, dc3, dc6;
    int bc2, bc3, bc6;

    if (is_wood_sensitivity == 1){
        mutex_lock(&prox_mtx);
        if (prox_data->enable) {
            status = cap1106_read_reg(prox_data->client, 0x03);
            dc2 = cap1106_read_reg(prox_data->client, 0x11);
            dc3 = cap1106_read_reg(prox_data->client, 0x12);
            dc6 = cap1106_read_reg(prox_data->client, 0x15);
            bc2 = cap1106_read_reg(prox_data->client, 0x51);
            bc3 = cap1106_read_reg(prox_data->client, 0x52);
            bc6 = cap1106_read_reg(prox_data->client, 0x55);
            PROX_DEBUG("Status: 0x%02X, BC2=0x%02X, DC2=0x%02X, BC3=0x%02X, DC3=0x%02X, BC6=0x%02X, DC6=0x%02X\n", status, bc2, dc2, bc3, dc3, bc6, dc6);
            if (machine_is_apq8064_deb()) {
                if ((dc2 == 0x00 && dc3 == 0x00)
                    || (dc2 == 0xFF && dc3 == 0xFF)
                    || (dc2 == 0x00 && dc3 == 0xFF)
                    || (dc2 == 0xFF && dc3 == 0x00)
                    || (dc2 == 0x00 && dc6 == 0x00)
                    || (dc2 == 0xFF && dc6 == 0xFF)
                    || (dc2 == 0x00 && dc6 == 0xFF)
                    || (dc2 == 0xFF && dc6 == 0x00)
                    || (dc3 == 0x00 && dc6 == 0x00)
                    || (dc3 == 0xFF && dc6 == 0xFF)
                    || (dc3 == 0x00 && dc6 == 0xFF)
                    || (dc3 == 0xFF && dc6 == 0x00)
                    || (prox_data->overflow_status == 0x02 && (dc2 > 0x50) && (dc2 <= 0x7F))
                    || (prox_data->overflow_status == 0x04 && (dc3 > 0x50) && (dc3 <= 0x7F))
                    || (prox_data->overflow_status == 0x20 && (dc6 > 0x50) && (dc6 <= 0x7F))
                    || (prox_data->overflow_status == 0x06 && (((dc2 > 0x50) && (dc2 <= 0x7F)) || ((dc3 > 0x50) && (dc3 <= 0x7F))))
                    || (prox_data->overflow_status == 0x22 && (((dc2 > 0x50) && (dc2 <= 0x7F)) || ((dc6 > 0x50) && (dc6 <= 0x7F))))
                    || (prox_data->overflow_status == 0x24 && (((dc3 > 0x50) && (dc3 <= 0x7F)) || ((dc6 > 0x50) && (dc6 <= 0x7F))))
                    || (prox_data->overflow_status == 0x26 && (((dc2 > 0x50) && (dc2 <= 0x7F)) || ((dc3 > 0x50) && (dc3 <= 0x7F)) || ((dc6 > 0x50) && (dc6 <= 0x7F))))) {
                    PROX_DEBUG("Deb, unset is_wood_sensitivity to 0\n");
                    //set sensitivity and threshold for 2cm body distance
                    cap1106_write_reg(prox_data->client, 0x1f, 0x1f);
                    cap1106_write_reg(prox_data->client, 0x31, 0x08);
                    cap1106_write_reg(prox_data->client, 0x32, 0x0a);
                    cap1106_write_reg(prox_data->client, 0x35, 0x0a);
                    is_wood_sensitivity = 0;
                    queue_delayed_work(prox_wq, &prox_data->work, 0);
                }
            }
        }
        mutex_unlock(&prox_mtx);
    }
    queue_delayed_work(prox_wq, &prox_data->checking_work, checking_work_period);
}

static int __devinit cap1106_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int rc = 0;

    prox_data->client = client;

    /* Touch data processing workqueue initialization */
    INIT_DELAYED_WORK(&prox_data->work, cap1106_work_function);
    INIT_DELAYED_WORK(&prox_data->checking_work,cap1106_checking_work_function);

    i2c_set_clientdata(client, prox_data);
    prox_data->client->flags = 0;
    strlcpy(prox_data->client->name, "cap1106", I2C_NAME_SIZE);
    prox_data->enable = 0;

    rc = cap1106_init_sensor(prox_data->client);
    if (rc) {
        PROX_ERROR("Sensor initialization failed!\n");
        goto err_init_sensor_failed;
    }

    if (machine_is_apq8064_deb())
        prox_data->attrs.attrs = cap1106_attr_deb;

    rc = sysfs_create_group(&prox_data->client->dev.kobj, &prox_data->attrs);
    if (rc) {
        PROX_ERROR("Create the sysfs group failed!\n");
        goto err_create_sysfs_group_failed;
    }

    /* register switch class */
    prox_sdev.name = NAME_RIL_PROX;
    prox_sdev.print_name = print_prox_name;
    prox_sdev.print_state = print_prox_state;

    rc = switch_dev_register(&prox_sdev);

    if (rc) {
        PROX_ERROR("Switch device registration failed!\n");
        goto err_register_switch_class_failed;
    }

    rc = cap1106_config_irq(prox_data->client);
    if (rc) {
        PROX_ERROR("Sensor INT configuration failed!\n");
        goto err_config_irq_failed;
    }

    prox_data->enable = 1;
    prox_data->overflow_status = 0x0;
    queue_delayed_work(prox_wq, &prox_data->work, msecs_to_jiffies(200));
    queue_delayed_work(prox_wq, &prox_data->checking_work, checking_work_period);

    PROX_INFO("OK\n");

    return 0;

err_config_irq_failed:
err_register_switch_class_failed:
    sysfs_remove_group(&prox_data->client->dev.kobj, &prox_data->attrs);
err_create_sysfs_group_failed:
err_init_sensor_failed:
    return rc;
}

static int __devexit cap1106_remove(struct i2c_client *client)
{
    PROX_DEBUG("\n");
    switch_dev_unregister(&prox_sdev);
    sysfs_remove_group(&client->dev.kobj, &prox_data->attrs);
    free_irq(client->irq, client);
    kfree(prox_data);
    return 0;
}

static int cap1106_suspend(struct i2c_client *client, pm_message_t mesg)
{
    PROX_DEBUG("+\n");
    mutex_lock(&prox_mtx);
    cap1106_enable_sensor(client, 0);
    mutex_unlock(&prox_mtx);
    PROX_DEBUG("-\n");
    return 0;
}

static int cap1106_resume(struct i2c_client *client)
{
    PROX_DEBUG("+\n");
    mutex_lock(&prox_mtx);
    if (force_enable)
        cap1106_enable_sensor(client, 1);
    mutex_unlock(&prox_mtx);
    PROX_DEBUG("-\n");
    return 0;
}

static int __init cap1106_init(void)
{
    int rc;

    PROX_INFO("+++\n");

    if (!machine_is_apq8064_deb()) {
        PROX_ERROR("Cap1106 driver doesn't support this project\n");
        return -EINVAL;
    }

    prox_wq = create_singlethread_workqueue("prox_wq");
    if(!prox_wq) {
        PROX_ERROR("create_singlethread_workqueue failed!\n");
        rc = -ENOMEM;
        goto err_create_singlethread_workqueue_failed;
    }

    prox_data = kzalloc(sizeof(struct cap1106_data), GFP_KERNEL);
    if (!prox_data) {
        PROX_ERROR("kzalloc failed!\n");
        rc = -ENOMEM;
        goto err_kzalloc_failed;
    }

    rc = i2c_add_driver(&cap1106_driver);
    if (rc) {
        PROX_ERROR("i2c_add_driver failed!\n");
        goto err_i2c_add_driver_failed;
    }

    PROX_INFO("---\n");

    return 0;

err_i2c_add_driver_failed:
    kfree(prox_data);
err_kzalloc_failed:
    destroy_workqueue(prox_wq);
err_create_singlethread_workqueue_failed:
    return rc;
}

static void __exit cap1106_exit(void)
{
    PROX_DEBUG("\n");

    if (!machine_is_apq8064_deb()) {
        PROX_ERROR("Cap1106 driver doesn't support this project\n");
        return;
    }

    i2c_del_driver(&cap1106_driver);

    if (prox_wq)
        destroy_workqueue(prox_wq);
}

module_init(cap1106_init);
module_exit(cap1106_exit);
