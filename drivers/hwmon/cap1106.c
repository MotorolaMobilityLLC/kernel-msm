/*
 * An I2C driver for SMSC CAP1106.
 *
 * Copyright (c) 2013, ASUSTek Corporation.
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
#include <linux/netdevice.h>

#include <asm/mach-types.h>
#include <linux/i2c/cap1106.h>

#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
extern bool rmnet_netdev_cmp(struct net_device *);
#endif

/*
 * Debug Utility
 */
#define CAP_DEBUG(fmt, arg...)	\
		pr_debug("CAP1106: [%s] " fmt , __func__ , ##arg)
#define CAP_INFO(fmt, arg...)	\
		pr_info("CAP1106: [%s] " fmt , __func__ , ##arg)
#define CAP_ERROR(fmt, arg...)	\
		pr_err("CAP1106: [%s] " fmt , __func__ , ##arg)

#define CAP_SDEV_NAME "ril_proximity"

/*
 * Global Variable
 */
struct cap1106_data {
	struct attribute_group attrs;
	struct i2c_client *client;
	struct workqueue_struct *cap_wq;
	struct delayed_work work;
	struct delayed_work checking_work;
	int enable;
	int obj_detect;
	int overflow_status;
	int app2mdm_enable;
	int sar_gpio;
	char *sar_gpio_name;
	int det_gpio;
	char *det_gpio_name;
	const unsigned char *init_table;
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
	struct notifier_block netdev_obs;
#endif
};

static DEFINE_MUTEX(cap_mtx);
static struct cap1106_data *pivate_data;
static struct switch_dev cap_sdev;
static int is_wood_sensitivity = 0;
static int ac2 = 0; // Accumulated Count Ch2
static int ac6 = 0; // Accumulated Count Ch6
static int ac_limit = 10;
static int force_enable = 0;
static bool bSkip_Checking = false;
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
static int rmnet_ctl = 1;
static int rmnet_if_up = 0;
#endif

/*
 * Function Declaration
 */
static int __devinit cap1106_probe(struct i2c_client *client,
        const struct i2c_device_id *id);
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
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
static void rmnet_state_changed(struct i2c_client *client);
#endif
static ssize_t show_attrs_handler(struct device *dev,
        struct device_attribute *devattr, char *buf);
static ssize_t store_attrs_handler(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count);

/*
 * I2C Driver Structure
 */
static const struct i2c_device_id cap1106_id[] = {
    { CAP1106_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, cap1106_id);

static struct i2c_driver cap1106_driver = {
	.driver = {
		.name = CAP1106_I2C_NAME,
		.owner = THIS_MODULE,
	},
	.probe		= cap1106_probe,
	.remove		= __devexit_p(cap1106_remove),
	.resume		= cap1106_resume,
	.suspend	= cap1106_suspend,
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
DEVICE_ATTR(sensor_app2mdm_sar, 0644, NULL, store_attrs_handler);
DEVICE_ATTR(sensor_main, 0644, show_attrs_handler, NULL);
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
DEVICE_ATTR(sensor_rmnetctl, 0644, show_attrs_handler, store_attrs_handler);
#endif

static struct attribute *cap1106_attr_deb[] = {
	&dev_attr_obj_detect.attr,					// 1
	&dev_attr_sensitivity.attr,					// 2
	&dev_attr_sensor_gain.attr,					// 3
	&dev_attr_sensor_input_2_delta_count.attr,	// 4
	&dev_attr_sensor_input_2_th.attr,			// 5
	&dev_attr_sensor_input_6_delta_count.attr,	// 6
	&dev_attr_sensor_input_6_th.attr,			// 7
	&dev_attr_sensor_input_noise_th.attr,		// 8
	&dev_attr_sensor_input_status.attr,			// 9
	&dev_attr_sensing_cycle.attr,				// 10
	&dev_attr_sensor_onoff.attr,				// 11
	&dev_attr_sensor_recal.attr,				// 12
	&dev_attr_sensor_app2mdm_sar.attr,			// 13
	&dev_attr_sensor_main.attr,			// 14
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
	&dev_attr_sensor_rmnetctl.attr,				// 15
#endif
	NULL
};

static ssize_t show_attrs_handler(struct device *dev,
        struct device_attribute *devattr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cap1106_data *data = i2c_get_clientdata(client);
	const char *attr_name = devattr->attr.name;
	int ret = -1;

	CAP_DEBUG("devattr->attr->name: %s\n", devattr->attr.name);

	mutex_lock(&cap_mtx);
	if (data->enable) {
		if (!strcmp(attr_name, dev_attr_obj_detect.attr.name)) {
			ret = sprintf(buf, "%d\n", data->obj_detect);
		} else if (!strcmp(attr_name, dev_attr_sensitivity.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x1F));
		} else if (!strcmp(attr_name, dev_attr_sensor_gain.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x00) >> 6);
		} else if (!strcmp(attr_name,
		        dev_attr_sensor_input_2_delta_count.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x11));
		} else if (!strcmp(attr_name, dev_attr_sensor_input_2_th.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x31));
		} else if (!strcmp(attr_name,
		        dev_attr_sensor_input_6_delta_count.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x15));
		} else if (!strcmp(attr_name, dev_attr_sensor_input_6_th.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x35));
		} else if (!strcmp(attr_name,
		        dev_attr_sensor_input_noise_th.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x38));
		} else if (!strcmp(attr_name, dev_attr_sensor_input_status.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x03));
		} else if (!strcmp(attr_name, dev_attr_sensing_cycle.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x24));
		} else if (!strcmp(attr_name, dev_attr_sensor_onoff.attr.name)) {
			ret = sprintf(buf, "%d\n", data->enable);
		} else if (!strcmp(attr_name, dev_attr_sensor_recal.attr.name)) {
			ret = sprintf(buf,
			        cap1106_read_reg(client, 0x26) == 0x0 ? "OK\n" : "FAIL\n");
		} else if (!strcmp(attr_name, dev_attr_sensor_main.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x00));
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
		} else if (!strcmp(attr_name, dev_attr_sensor_rmnetctl.attr.name)) {
			ret = sprintf(buf, "%d\n", rmnet_ctl);
#endif
		}
	} else {
		if (!strcmp(attr_name, dev_attr_sensor_main.attr.name)) {
			ret = sprintf(buf, "%02X\n", cap1106_read_reg(client, 0x00));
		} else {
			ret = sprintf(buf, "SENSOR DISABLED\n");
		}
	}



	mutex_unlock(&cap_mtx);

	return ret;
}

static ssize_t store_attrs_handler(struct device *dev,
        struct device_attribute *devattr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cap1106_data *data = i2c_get_clientdata(client);
	const char *attr_name = devattr->attr.name;
	unsigned long value;

	if (strict_strtoul(buf, 16, &value)) return -EINVAL;

	CAP_DEBUG("devattr->attr->name: %s, value: 0x%lX\n",
	        devattr->attr.name, value);

	mutex_lock(&cap_mtx);
	if (data->enable) {
		if (!strcmp(attr_name, dev_attr_sensitivity.attr.name)) {
			cap1106_write_reg(client, 0x1F, value & 0x7F);
		} else if (!strcmp(attr_name, dev_attr_sensor_gain.attr.name)) {
			cap1106_write_reg(client, 0x00,
			        (cap1106_read_reg(client, 0x00) & 0x3F) | ((value & 0x03) << 6));
		} else if (!strcmp(attr_name, dev_attr_sensor_input_2_th.attr.name)) {
			cap1106_write_reg(client, 0x31, value & 0x7F);
		} else if (!strcmp(attr_name, dev_attr_sensor_input_6_th.attr.name)) {
			cap1106_write_reg(client, 0x35, value & 0x7F);
		} else if (!strcmp(attr_name,
		        dev_attr_sensor_input_noise_th.attr.name)) {
			cap1106_write_reg(client, 0x38, value & 0x03);
		} else if (!strcmp(attr_name, dev_attr_sensing_cycle.attr.name)) {
			cap1106_write_reg(client, 0x24, value & 0x7F);
		} else if (!strcmp(attr_name, dev_attr_sensor_onoff.attr.name)) {
			if (value == 0) {
				force_enable = 0;
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
				rmnet_ctl = 0;
#endif
				cap1106_enable_sensor(client, 0);
			}
		} else if (!strcmp(attr_name, dev_attr_sensor_recal.attr.name)) {
			cap1106_write_reg(client, 0x26, 0x22);
		} else if (!strcmp(attr_name, dev_attr_sensor_app2mdm_sar.attr.name)) {
			gpio_set_value(data->sar_gpio, value);
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
		} else if (!strcmp(attr_name, dev_attr_sensor_rmnetctl.attr.name)) {
			rmnet_ctl = value;
			rmnet_state_changed(client);
#endif
		}
	} else {
		if (!strcmp(attr_name, dev_attr_sensor_onoff.attr.name)) {
			if (value == 1) {
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
				rmnet_ctl = 0;
#endif
				force_enable = 1;
				cap1106_enable_sensor(client, 1);
			}
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
		} else if (!strcmp(attr_name, dev_attr_sensor_rmnetctl.attr.name)) {
			rmnet_ctl = value;
			rmnet_state_changed(client);
#endif
		}
	}
	mutex_unlock(&cap_mtx);

	return strnlen(buf, count);;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callbacks for switch device
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t print_cap_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", "prox_sar_det");
}

static ssize_t print_cap_state(struct switch_dev *sdev, char *buf)
{
	if (switch_get_state(sdev))
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
static int netdev_changed_event(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct net_device *dev = (struct net_device *)ptr;
	struct cap1106_data *data = container_of(this, struct cap1106_data, netdev_obs);
	int before_if_up = rmnet_if_up;

	CAP_DEBUG("+\n");
	if (!rmnet_netdev_cmp(dev))
		goto done;

	mutex_lock(&cap_mtx);
	switch (event) {
	case NETDEV_UP:
		++rmnet_if_up;
		CAP_DEBUG("netdev %s is up, rmnet_if_up=%d\n", dev->name, rmnet_if_up);
		break;

	case NETDEV_DOWN:
		--rmnet_if_up;
		CAP_DEBUG("netdev %s is down, rmnet_if_up=%d\n", dev->name, rmnet_if_up);
		break;

	default:
		break;
	}
	if (rmnet_if_up < 0) {
		CAP_ERROR("rmnet_if_up is negative! Unbalanced netdev state change?\n");
		rmnet_if_up = 0;
	}
	if (before_if_up != rmnet_if_up)
		rmnet_state_changed(data->client);
	mutex_unlock(&cap_mtx);

done:
	CAP_DEBUG("-\n");
	return NOTIFY_DONE;
}
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cap1106_enable_sensor(struct i2c_client *client, int enable)
{
	long reg_value;

	struct cap1106_data *data = i2c_get_clientdata(client);

	if (data->enable != enable) {
		reg_value = cap1106_read_reg(client, 0x00);
		if (enable) {
			cap1106_write_reg(client, 0x00, reg_value & 0xCE);
			bSkip_Checking = false;
			// Time to first conversion is 200ms (Max)
			queue_delayed_work(data->cap_wq, &data->work,
			        msecs_to_jiffies(200));
			enable_irq(client->irq);
			queue_delayed_work(data->cap_wq, &data->checking_work,
			        msecs_to_jiffies(1000));
		} else {
			disable_irq(client->irq);
			bSkip_Checking = true;  // if checking_work_function is in progress, checking_work_function must skip.
			//cancel_delayed_work_sync(&data->work);
			//cancel_delayed_work_sync(&data->checking_work);
			//flush_workqueue(data->cap_wq);
			switch_set_state(&cap_sdev, 0);
			cap1106_write_reg(client, 0x00, reg_value | 0x30);
		}
		data->enable = enable;

		CAP_INFO("data->enable: %d\n", data->enable);
	}
}

#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
static void rmnet_state_changed(struct i2c_client *client)
{
	if (force_enable || (rmnet_ctl && rmnet_if_up > 0))
		cap1106_enable_sensor(client, 1);
	else
		cap1106_enable_sensor(client, 0);
}
#endif

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
	int bc2, bc6; // Base Count Ch2, Ch6
	int dc2, dc6; // Delta Count Ch2, Ch6
	struct cap1106_data *data =
	        container_of((struct delayed_work *)work, struct cap1106_data, work);

	disable_irq(data->client->irq);
	// Clear INT, keep GAIN, STBY, DSLEEP
	cap1106_write_reg(data->client, 0x00,
	        cap1106_read_reg(data->client, 0x00) & 0xF0);
	status = cap1106_read_reg(data->client, 0x03);
	dc2 = cap1106_read_reg(data->client, 0x11);
	dc6 = cap1106_read_reg(data->client, 0x15);
	bc2 = cap1106_read_reg(data->client, 0x51);
	bc6 = cap1106_read_reg(data->client, 0x55);
	CAP_DEBUG(
	        "status: 0x%02X, bc2=0x%02X, dc2=0x%02X, bc6=0x%02X, dc6=0x%02X\n",
	        status, bc2, dc2, bc6, dc6);
	if (is_wood_sensitivity == 0) {
		data->obj_detect = (status & 0x22) ? 1 : 0;
		CAP_DEBUG("obj_detect: %d", data->obj_detect);
		switch_set_state(&cap_sdev, data->obj_detect);
		if (data->app2mdm_enable) {
			gpio_set_value(data->sar_gpio, data->obj_detect);
		}
		if ((status == 0x02 && dc2 == 0x7F) || (status == 0x20 && dc6 == 0x7F)
		        || (status == 0x22 && (dc2 == 0x7F || dc6 == 0x7F))) {
			CAP_DEBUG("is_wood_sensitivity = 1\n");
			//set sensitivity and threshold for wood touch
			cap1106_write_reg(data->client, 0x1F, 0x4F);
			cap1106_write_reg(data->client, 0x31, 0x5F);
			cap1106_write_reg(data->client, 0x35, 0x5F);
			is_wood_sensitivity = 1;
			data->overflow_status = status;
			ac2 = 0;
			ac6 = 0;
		} else {
			if (dc2 >= 0x0A && dc2 <= 0x3F) ac2++;
			if (dc6 >= 0x0A && dc6 <= 0x3F) ac6++;

			CAP_DEBUG("ac2=%d, ac6=%d\n", ac2, ac6);
			if (ac2 >= ac_limit || ac6 >= ac_limit) {
				CAP_DEBUG("+++ FORCE RECALIBRATION +++\n");
				cap1106_write_reg(data->client, 0x26, 0x22);
				ac2 = 0;
				ac6 = 0;
			}
		}
	}
	enable_irq(data->client->irq);
}

static irqreturn_t cap1106_interrupt_handler(int irq, void *dev)
{
	struct cap1106_data *data = i2c_get_clientdata(dev);
	queue_delayed_work(data->cap_wq, &data->work, 0);
	return IRQ_HANDLED;
}

static int cap1106_config_irq(struct i2c_client *client)
{
	int rc = 0;

	struct cap1106_data *data = i2c_get_clientdata(client);

	if (data->app2mdm_enable && gpio_is_valid(data->sar_gpio)) {
		rc = gpio_request(data->sar_gpio, data->sar_gpio_name);
		if (rc) {
			CAP_ERROR("APP2MDM_SAR_gpio_request_failed, rc=%d\n", rc);
			goto err_APP2MDM_SAR_gpio_request_failed;
		}

		rc = gpio_direction_output(data->sar_gpio, 0);
		if (rc) {
			CAP_ERROR("APP2MDM_SAR_gpio_direction_output_failed, rc=%d\n", rc);
			goto err_APP2MDM_SAR_gpio_direction_output_failed;
		}
		CAP_INFO("GPO=%02d OK", data->sar_gpio);
	}

	if (gpio_is_valid(data->det_gpio)) {
		rc = gpio_request(data->det_gpio, data->det_gpio_name);
		if (rc) {
			CAP_ERROR("SAR_DET_3G_gpio_request_failed, rc=%d\n", rc);
			goto err_SAR_DET_3G_gpio_request_failed;
		}

		rc = gpio_direction_input(data->det_gpio);
		if (rc) {
			CAP_ERROR("SAR_DET_3G_gpio_direction_input_failed, rc=%d\n", rc);
			goto err_SAR_DET_3G_gpio_direction_input_failed;
		}

		rc = request_irq(client->irq, cap1106_interrupt_handler,
		        IRQF_TRIGGER_FALLING, data->det_gpio_name, client);

		if (rc) {
			CAP_ERROR("SAR_DET_3G_request_irq_failed, rc=%d\n", rc);
			goto err_SAR_DET_3G_request_irq_failed;
		}

		CAP_INFO("GPI=%02d, VALUE=%d OK\n",
		        data->det_gpio, gpio_get_value(data->det_gpio));
	}

	return 0;

err_SAR_DET_3G_request_irq_failed:
err_SAR_DET_3G_gpio_direction_input_failed:
	gpio_free(data->det_gpio);
err_SAR_DET_3G_gpio_request_failed:
err_APP2MDM_SAR_gpio_direction_output_failed:
	gpio_free(data->sar_gpio);
err_APP2MDM_SAR_gpio_request_failed:
	return rc;
}

static int cap1106_init_sensor(struct i2c_client *client)
{
	unsigned char bIdx;
	int rc = 0;
	struct cap1106_data *data = i2c_get_clientdata(client);

	for (bIdx = 0; bIdx < CAP1106_INIT_TABLE_SIZE; bIdx += 2) {
		if ((rc = cap1106_write_reg(client, *(data->init_table + bIdx),
		        *(data->init_table + bIdx + 1)))) {
			CAP_ERROR("I2C write error, rc=0x%X\n", rc);
			break;
		}
	}

	CAP_INFO("I2C_NAME: %s, I2C_ADDR: 0x%X %s\n", client->name, client->addr, rc ? "FAIL" : "OK");

	return rc;
}

static void cap1106_checking_work_function(struct work_struct *work)
{
	int status;
	int bc2, bc6;
	int dc2, dc6;

	struct cap1106_data *data =
	        container_of((struct delayed_work *)work, struct cap1106_data, checking_work);

	mutex_lock(&cap_mtx);
	CAP_DEBUG("+\n");
	if( bSkip_Checking )
	{
		// skip this round
		mutex_unlock(&cap_mtx);
		return;
	}

	if (is_wood_sensitivity == 1) {
		if (data->enable) {
			status = cap1106_read_reg(data->client, 0x03);
			dc2 = cap1106_read_reg(data->client, 0x11);
			dc6 = cap1106_read_reg(data->client, 0x15);
			bc2 = cap1106_read_reg(data->client, 0x51);
			bc6 = cap1106_read_reg(data->client, 0x55);
			CAP_DEBUG(
			        "status: 0x%02X, bc2=0x%02X, dc2=0x%02X, bc6=0x%02X, dc6=0x%02X\n",
			        status, bc2, dc2, bc6, dc6);
			if ((dc2 == 0x00 && dc6 == 0x00)
					|| (dc2 == 0xFF && dc6 == 0xFF)
			        || (dc2 == 0x00 && dc6 == 0xFF)
			        || (dc2 == 0xFF && dc6 == 0x00)
			        || (data->overflow_status == 0x02 && (dc2 > 0x5F) && (dc2 <= 0x7F))
			        || (data->overflow_status == 0x20 && (dc6 > 0x5F) && (dc6 <= 0x7F))
			        || (data->overflow_status == 0x22 && (((dc2 > 0x5F) && (dc2 <= 0x7F)) || ((dc6 > 0x5F) && (dc6 <= 0x7F))))) {
				CAP_DEBUG("is_wood_sensitivity = 0\n");
				//set sensitivity and threshold for 2cm body distance
				cap1106_write_reg(data->client, 0x1F, 0x2F);
				cap1106_write_reg(data->client, 0x31, 0x0A);
				cap1106_write_reg(data->client, 0x35, 0x0A);
				is_wood_sensitivity = 0;
				queue_delayed_work(data->cap_wq, &data->work, 0);
			}
		}
	}
	queue_delayed_work(data->cap_wq, &data->checking_work,
	        msecs_to_jiffies(1000));
	mutex_unlock(&cap_mtx);
}

static int __devinit cap1106_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
	int rc = 0;

	struct cap1106_data *data;
	struct cap1106_i2c_platform_data *pdata;

	data = kzalloc(sizeof(struct cap1106_data), GFP_KERNEL);
	if (!data) {
		CAP_ERROR("kzalloc failed!\n");
		rc = -ENOMEM;
		goto err_kzalloc_failed;
	}

	data->cap_wq = create_singlethread_workqueue("cap_wq");
	if (!data->cap_wq) {
		CAP_ERROR("create_singlethread_workqueue failed!\n");
		rc = -ENOMEM;
		goto err_create_singlethread_workqueue_failed;
	}

	INIT_DELAYED_WORK(&data->work, cap1106_work_function);
	INIT_DELAYED_WORK(&data->checking_work, cap1106_checking_work_function);

	data->client = client;
	i2c_set_clientdata(client, data);

	pdata = client->dev.platform_data;

	if (likely(pdata != NULL)) {
		data->app2mdm_enable = pdata->app2mdm_enable;
		data->sar_gpio = pdata->sar_gpio;
		data->sar_gpio_name = pdata->sar_gpio_name;
		data->det_gpio = pdata->det_gpio;
		data->det_gpio_name = pdata->det_gpio_name;
		data->init_table = pdata->init_table;

		CAP_INFO("data->app2mdm_enable: %d\n", data->app2mdm_enable);
		CAP_INFO("data->sar_gpio: %d\n", data->sar_gpio);
		CAP_INFO("data->sar_gpio_name: %s\n", data->sar_gpio_name);
		CAP_INFO("data->det_gpio: %d\n", data->det_gpio);
		CAP_INFO("data->det_gpio_name: %s\n", data->det_gpio_name);
	}

	data->client->flags = 0;
	strlcpy(data->client->name, CAP1106_I2C_NAME, I2C_NAME_SIZE);
	data->enable = 0;

	rc = cap1106_init_sensor(data->client);
	if (rc) {
		CAP_ERROR("Sensor initialization failed!\n");
		goto err_init_sensor_failed;
	}

	data->attrs.attrs = cap1106_attr_deb;

	rc = sysfs_create_group(&data->client->dev.kobj, &data->attrs);
	if (rc) {
		CAP_ERROR("Create the sysfs group failed!\n");
		goto err_create_sysfs_group_failed;
	}

	/* register switch class */
	cap_sdev.name = CAP_SDEV_NAME;
	cap_sdev.print_name = print_cap_name;
	cap_sdev.print_state = print_cap_state;

	rc = switch_dev_register(&cap_sdev);

	if (rc) {
		CAP_ERROR("Switch device registration failed!\n");
		goto err_register_switch_class_failed;
	}

	rc = cap1106_config_irq(data->client);
	if (rc) {
		CAP_ERROR("Sensor INT configuration failed!\n");
		goto err_config_irq_failed;
	}

	data->enable = force_enable;
	data->overflow_status = 0x0;
	if (data->enable) {
		queue_delayed_work(data->cap_wq, &data->work, msecs_to_jiffies(200));
		queue_delayed_work(data->cap_wq, &data->checking_work,
				msecs_to_jiffies(1000));
	} else
		disable_irq(data->client->irq);

#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
	data->netdev_obs.notifier_call = netdev_changed_event;
	register_netdevice_notifier(&data->netdev_obs);
#endif

	pivate_data = data;

	CAP_INFO("OK\n");

	return 0;

err_config_irq_failed:
err_register_switch_class_failed:
	sysfs_remove_group(&data->client->dev.kobj, &data->attrs);
err_create_sysfs_group_failed:
err_init_sensor_failed:
	destroy_workqueue(data->cap_wq);
err_create_singlethread_workqueue_failed:
	kfree(data);
err_kzalloc_failed:
	return rc;
}

static int __devexit cap1106_remove(struct i2c_client *client)
{
	struct cap1106_data *data = i2c_get_clientdata(client);
	CAP_DEBUG("+\n");
#if defined(CONFIG_CAP_SENSOR_RMNET_CTL)
	unregister_netdevice_notifier(&data->netdev_obs);
#endif
	switch_dev_unregister(&cap_sdev);
	sysfs_remove_group(&client->dev.kobj, &data->attrs);
	free_irq(client->irq, client);
	if (data->cap_wq) destroy_workqueue(data->cap_wq);
	kfree(data);
	CAP_DEBUG("-\n");
	return 0;
}

static int cap1106_suspend(struct i2c_client *client, pm_message_t mesg)
{
	CAP_DEBUG("+\n");
	mutex_lock(&cap_mtx);
	cap1106_enable_sensor(client, 0);
	mutex_unlock(&cap_mtx);
	CAP_DEBUG("-\n");
	return 0;
}

static int cap1106_resume(struct i2c_client *client)
{
	CAP_DEBUG("+\n");
	mutex_lock(&cap_mtx);
	if (force_enable) cap1106_enable_sensor(client, 1);
	mutex_unlock(&cap_mtx);
	CAP_DEBUG("-\n");
	return 0;
}

static int __init cap1106_init(void)
{
	CAP_INFO("\n");
	return i2c_add_driver(&cap1106_driver);
}

static void __exit cap1106_exit(void)
{
	CAP_INFO("\n");
	i2c_del_driver(&cap1106_driver);
}

module_init(cap1106_init);
module_exit(cap1106_exit);
MODULE_DESCRIPTION("SMSC CAP1106 Driver");
MODULE_LICENSE("GPL");
