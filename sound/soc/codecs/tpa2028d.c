/* sound/soc/codecs/tpa2028d.c
 *
 * Copyright (C) 2009 LGE, Inc.
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <asm/system.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/ioctls.h>
#include <mach/debug_mm.h>
#include <linux/slab.h>

#include <sound/tpa2028d.h>

#define MODULE_NAME "tpa2028d"

#undef  AMP_DEBUG_PRINT
#define AMP_DEBUG_PRINT

#define AMP_IOCTL_MAGIC 't'
#define AMP_SET_DATA	_IOW(AMP_IOCTL_MAGIC, 0, struct amp_cal *)
#define AMP_GET_DATA	_IOW(AMP_IOCTL_MAGIC, 1, struct amp_cal *)

static uint32_t msm_snd_debug = 1;
module_param_named(debug_mask, msm_snd_debug, uint, 0664);

#if defined (AMP_DEBUG_PRINT)
#define D(fmt, args...) printk(KERN_INFO "[%s:%5d]" fmt, __func__, __LINE__, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

struct amp_config {
	struct i2c_client *client;
	struct audio_amp_platform_data *pdata;
};

static struct amp_config *amp_data = NULL;

int ReadI2C(char reg, char *ret)
{

	unsigned int err;
	unsigned char buf = reg;

	struct i2c_msg msg[2] = {
		{ amp_data->client->addr, 0, 1, &buf },
		{ amp_data->client->addr, I2C_M_RD, 1, ret}
	};
	err = i2c_transfer(amp_data->client->adapter, msg, 2);
	if (err < 0)
		D("i2c read error:%x\n", reg);
	else
		D("i2c read ok:%x\n", reg);
	return 0;

}

int WriteI2C(char reg, char val)
{

	int err;
	unsigned char buf[2];
	struct i2c_msg msg = {amp_data->client->addr, 0, 2, buf };

	buf[0] = reg;
	buf[1] = val;
	err = i2c_transfer(amp_data->client->adapter, &msg, 1);

	if (err < 0) {
		D("i2c write error:%x:%x\n", reg, val);
		return -EIO;
	} else {
		return 0;
	}
}

int tpa2028d_poweron(void)
{
	int fail = 0;
	char agc_compression_rate = amp_data->pdata->agc_compression_rate;
	char agc_output_limiter_disable = amp_data->pdata->agc_output_limiter_disable;
	char agc_fixed_gain = amp_data->pdata->agc_fixed_gain;

	agc_output_limiter_disable = (agc_output_limiter_disable<<7);

	fail |= WriteI2C(IC_CONTROL, 0xE3); /*Tuen On*/
	fail |= WriteI2C(AGC_ATTACK_CONTROL, 0x05); /*Tuen On*/
	fail |= WriteI2C(AGC_RELEASE_CONTROL, 0x0B); /*Tuen On*/
	fail |= WriteI2C(AGC_HOLD_TIME_CONTROL, 0x00); /*Tuen On*/
	fail |= WriteI2C(AGC_FIXED_GAIN_CONTROL, agc_fixed_gain); /*Tuen On*/
	fail |= WriteI2C(AGC1_CONTROL, 0x3A|agc_output_limiter_disable); /*Tuen On*/
	fail |= WriteI2C(AGC2_CONTROL, 0xC0|agc_compression_rate); /*Tuen On*/
	fail |= WriteI2C(IC_CONTROL, 0xC3); /*Tuen On*/

	return fail;
}

int tpa2028d_powerdown(void)
{
	int fail = 0;
	return fail;
}

inline void set_amp_gain(int amp_state)
{
	int fail = 0;

	switch (amp_state) {
	case SPK_ON:
		/* if the delay time is need for chip ready,
		 * should be added to each power or enable function.
		 */
		if (amp_data->pdata->power)
			fail = amp_data->pdata->power(1);
		/*need 5 msec for prevent mute issue*/
		msleep(5);
		if (amp_data->pdata->enable)
			fail = amp_data->pdata->enable(1);
		/*need 10 msec for chip ready*/
		msleep(10);
		fail = tpa2028d_poweron();
		break;
	case SPK_OFF:
		if (amp_data->pdata->enable)
			fail = amp_data->pdata->enable(2);
		fail = tpa2028d_powerdown();
		if (amp_data->pdata->enable)
			fail = amp_data->pdata->enable(0);
		if (amp_data->pdata->power)
			fail = amp_data->pdata->power(0);
		break;
	default:
		D("Amp_state [%d] does not support \n", amp_state);
	}
}
EXPORT_SYMBOL(set_amp_gain);

static ssize_t
tpa2028d_comp_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	if (val > 3 || val < 0)
		return -EINVAL;

	pdata->agc_compression_rate = val;
	return count;
}

static ssize_t
tpa2028d_comp_rate_show(struct device *dev, struct device_attribute *attr,   char *buf)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	char val = 0;

	ReadI2C(AGC2_CONTROL, &val);

	D("[tpa2028d_comp_rate_show] val : %x \n",val);

	return sprintf(buf, "AGC2_CONTROL : %x, pdata->agc_compression_rate : %d\n", val, pdata->agc_compression_rate);
}

static ssize_t
tpa2028d_out_lim_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;
	pdata->agc_output_limiter_disable = val&0x0001;
	return count;
}

static ssize_t
tpa2028d_out_lim_show(struct device *dev, struct device_attribute *attr,   char *buf)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	char val=0;

	ReadI2C(AGC1_CONTROL, &val);

	D("[tpa2028d_out_lim_show] val : %x \n",val);

	return sprintf(buf, "AGC1_CONTROL : %x, pdata->agc_output_limiter_disable : %d\n", val, pdata->agc_output_limiter_disable);
}

static ssize_t
tpa2028d_fixed_gain_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	pdata->agc_fixed_gain = val;
	return count;
}

static ssize_t
tpa2028d_fixed_gain_show(struct device *dev, struct device_attribute *attr,   char *buf)
{
	struct audio_amp_platform_data *pdata = amp_data->pdata;
	char val=0;

	ReadI2C(AGC_FIXED_GAIN_CONTROL, &val);

	D("[tpa2028d_fixed_gain_show] val : %x \n",val);

	return sprintf(buf, "fixed_gain : %x, pdata->agc_fixed_gain : %d\n", val, pdata->agc_fixed_gain);
}

static struct device_attribute tpa2028d_device_attrs[] = {
	__ATTR(comp_rate,  S_IRUGO | S_IWUSR, tpa2028d_comp_rate_show, tpa2028d_comp_rate_store),
	__ATTR(out_lim, S_IRUGO | S_IWUSR, tpa2028d_out_lim_show, tpa2028d_out_lim_store),
	__ATTR(fixed_gain, S_IRUGO | S_IWUSR, tpa2028d_fixed_gain_show, tpa2028d_fixed_gain_store),
};

static int tpa2028d_amp_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct amp_config *data;
	struct audio_amp_platform_data *pdata;
	struct i2c_adapter *adapter = client->adapter;
	int err, i;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		err = -EOPNOTSUPP;
		return err;
	}

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		D("platform data is null\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct amp_config), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	amp_data = data;
	amp_data->pdata = pdata;
	data->client = client;
	i2c_set_clientdata(client, data);

	set_amp_gain(SPK_OFF);

	for (i = 0; i < ARRAY_SIZE(tpa2028d_device_attrs); i++) {
		err = device_create_file(&client->dev, &tpa2028d_device_attrs[i]);
		if (err)
			return err;
	}
	return 0;
}

static int tpa2028d_amp_remove(struct i2c_client *client)
{
	struct amp_config *data = i2c_get_clientdata(client);
	kfree(data);

	i2c_set_clientdata(client, NULL);
	return 0;
}


static struct i2c_device_id tpa2028d_amp_idtable[] = {
	{ "tpa2028d_amp", 1 },
};

static struct i2c_driver tpa2028d_amp_driver = {
	.probe = tpa2028d_amp_probe,
	.remove = tpa2028d_amp_remove,
	.id_table = tpa2028d_amp_idtable,
	.driver = {
		.name = MODULE_NAME,
	},
};


static int __init tpa2028d_amp_init(void)
{
	return i2c_add_driver(&tpa2028d_amp_driver);
}

static void __exit tpa2028d_amp_exit(void)
{
	return i2c_del_driver(&tpa2028d_amp_driver);
}

module_init(tpa2028d_amp_init);
module_exit(tpa2028d_amp_exit);

MODULE_DESCRIPTION("TPA2028D Amp Control");
MODULE_AUTHOR("Kim EunHye <ehgrace.kim@lge.com>");
MODULE_LICENSE("GPL");
