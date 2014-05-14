/*
 * cyttsp5_i2c.c
 * Cypress TrueTouch(TM) Standard Product V5 I2C Module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

/* cyttsp */
#include "cyttsp5_regs.h"


/*******************************************
 *
 * CYTTSP5 DEVTREE -Device Tree Parsing
 *
 *******************************************/

#define MAX_NAME_LENGTH		64

enum cyttsp5_device_type {
	DEVICE_MT,
	DEVICE_TYPE_MAX,
};

struct cyttsp5_device_pdata_func {
	void *(*create_and_get_pdata)(struct device_node *);
	void (*free_pdata)(void *);
};

struct cyttsp5_pdata_ptr {
	void **pdata;
};

struct cyttsp5_extended_mt_platform_data {
	struct cyttsp5_mt_platform_data pdata;
};

static int cyttsp5_hw_power(struct device *dev, int onoff)
{
	int ret;
	static struct regulator *vddo_vreg;
	static struct regulator *avdd_vreg;

	if (!vddo_vreg) {
		vddo_vreg = regulator_get(dev, "vddo");
		if (IS_ERR(vddo_vreg)) {
			vddo_vreg = NULL;
			dev_err(dev, "failed to get vddo regulator\n");
			return -ENODEV;
		}
	}

	if (!avdd_vreg) {
		avdd_vreg = regulator_get(dev, "avdd");
		if (IS_ERR(avdd_vreg)) {
			avdd_vreg = NULL;
			dev_err(dev, "failed to get avdd regulator\n");
			return -ENODEV;
		}
		ret = regulator_set_voltage(avdd_vreg, 2850000, 2850000);
		if (ret) {
			dev_err(dev, "unable to set voltage for avdd_vreg, %d\n",
				ret);
			return ret;
		}
	}

	if (onoff) {
		if (regulator_is_enabled(vddo_vreg)) {
			dev_err(dev, "vddo is already enabled\n");
		} else {
			ret = regulator_enable(vddo_vreg);
			if (ret) {
				dev_err(dev, "unable to enable vddo, %d\n",
					ret);
				return ret;
			}
		}
		if (regulator_is_enabled(avdd_vreg)) {
			dev_err(dev, "avdd is already enabled\n");
		} else {
			ret = regulator_enable(avdd_vreg);
			if (ret) {
				dev_err(dev, "unable to enable avdd, %d\n",
					ret);
				return ret;
			}
		}
	} else {
		if (regulator_is_enabled(vddo_vreg)) {
			ret = regulator_disable(vddo_vreg);
			if (ret) {
				dev_err(dev, "unable to disable vddo, %d\n",
					ret);
				return ret;
			}
		} else {
			dev_err(dev, "vddo is already disabled\n");
		}
		if (regulator_is_enabled(avdd_vreg)) {
			ret = regulator_disable(avdd_vreg);
			if (ret) {
				dev_err(dev, "unable to disable avdd, %d\n",
					ret);
				return ret;
			}
		} else {
			dev_err(dev, "avdd is already disabled\n");
		}
	}
	dev_info(dev, "%s: vddo: %d, avdd: %d\n",
			__func__, regulator_is_enabled(vddo_vreg),
			regulator_is_enabled(avdd_vreg));
	return 0;
}

static int cyttsp5_xres(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	int rc;

	rc = cyttsp5_hw_power(dev, 0);
	if (rc) {
		dev_err(dev, "%s: Fail power down HW\n", __func__);
		goto exit;
	}

	msleep(200);

	rc = cyttsp5_hw_power(dev, 1);
	if (rc) {
		dev_err(dev, "%s: Fail power up HW\n", __func__);
		goto exit;
	}
	msleep(100);

exit:
	return rc;
}

static int cyttsp5_init(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev)
{
	int irq_gpio = pdata->irq_gpio;

	if (on) {
		gpio_request(irq_gpio, "TSP_INT");
		gpio_direction_input(irq_gpio);
	} else {
		cyttsp5_hw_power(dev, 0);
		gpio_free(irq_gpio);
	}

	dev_info(dev,
		"%s: irq_gpio=%d onoff=%d\n",
		__func__, irq_gpio, on);
	return 0;
}

static int cyttsp5_power(struct cyttsp5_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	return cyttsp5_hw_power(dev, on);
}

static int cyttsp5_irq_stat(struct cyttsp5_core_platform_data *pdata,
		struct device *dev)
{
	return gpio_get_value(pdata->irq_gpio);
}

static inline int get_inp_dev_name(struct device_node *dev_node,
		const char **inp_dev_name)
{
	return of_property_read_string(dev_node, "cy,inp_dev_name",
			inp_dev_name);
}

static u16 *create_and_get_u16_array(struct device_node *dev_node,
		const char *name, int *size)
{
	const __be32 *values;
	u16 *val_array;
	int len;
	int sz;
	int rc;
	int i;

	values = of_get_property(dev_node, name, &len);
	if (values == NULL)
		return NULL;

	sz = len / sizeof(u32);
	pr_debug("%s: %s size:%d\n", __func__, name, sz);

	val_array = kzalloc(sz * sizeof(u16), GFP_KERNEL);
	if (val_array == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < sz; i++)
		val_array[i] = (u16)be32_to_cpup(values++);

	*size = sz;

	return val_array;

fail:
	return ERR_PTR(rc);
}

static struct touch_framework *create_and_get_touch_framework(
		struct device_node *dev_node)
{
	struct touch_framework *frmwrk;
	u16 *abs;
	int size;
	int rc;

	abs = create_and_get_u16_array(dev_node, "cy,abs", &size);
	if (IS_ERR_OR_NULL(abs))
		return (void *)abs;

	/* Check for valid abs size */
	if (size % CY_NUM_ABS_SET) {
		rc = -EINVAL;
		goto fail_free_abs;
	}

	frmwrk = kzalloc(sizeof(*frmwrk), GFP_KERNEL);
	if (frmwrk == NULL) {
		rc = -ENOMEM;
		goto fail_free_abs;
	}

	frmwrk->abs = abs;
	frmwrk->size = size;

	return frmwrk;

fail_free_abs:
	kfree(abs);

	return ERR_PTR(rc);
}

static void free_touch_framework(struct touch_framework *frmwrk)
{
	kfree(frmwrk->abs);
	kfree(frmwrk);
}

static void *create_and_get_mt_pdata(struct device_node *dev_node)
{
	struct cyttsp5_extended_mt_platform_data *ext_pdata;
	struct cyttsp5_mt_platform_data *pdata;
	u32 value;
	int rc;

	ext_pdata = kzalloc(sizeof(*ext_pdata), GFP_KERNEL);
	if (ext_pdata == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	pdata = &ext_pdata->pdata;

	rc = get_inp_dev_name(dev_node, &pdata->inp_dev_name);
	if (rc)
		goto fail_free_pdata;

	/* Optional fields */
	rc = of_property_read_u32(dev_node, "cy,flags", &value);
	if (!rc)
		pdata->flags = value;

	rc = of_property_read_u32(dev_node, "cy,vkeys_x", &value);
	if (!rc)
		pdata->vkeys_x = value;

	rc = of_property_read_u32(dev_node, "cy,vkeys_y", &value);
	if (!rc)
		pdata->vkeys_y = value;

	/* Required fields */
	pdata->frmwrk = create_and_get_touch_framework(dev_node);
	if (pdata->frmwrk == NULL) {
		rc = -EINVAL;
		goto fail_free_pdata;
	} else if (IS_ERR(pdata->frmwrk)) {
		rc = PTR_ERR(pdata->frmwrk);
		goto fail_free_pdata;
	}

	return pdata;

fail_free_pdata:
	kfree(ext_pdata);
fail:
	return ERR_PTR(rc);
}

static void free_mt_pdata(void *pdata)
{
	struct cyttsp5_mt_platform_data *mt_pdata =
		(struct cyttsp5_mt_platform_data *)pdata;
	struct cyttsp5_extended_mt_platform_data *ext_mt_pdata =
		container_of(mt_pdata,
			struct cyttsp5_extended_mt_platform_data, pdata);

	free_touch_framework(mt_pdata->frmwrk);
	kfree(ext_mt_pdata);
}

static struct cyttsp5_device_pdata_func device_pdata_funcs[DEVICE_TYPE_MAX] = {
	[DEVICE_MT] = {
		.create_and_get_pdata = create_and_get_mt_pdata,
		.free_pdata = free_mt_pdata,
	},
};

static struct cyttsp5_pdata_ptr pdata_ptr[DEVICE_TYPE_MAX];

static const char *device_names[DEVICE_TYPE_MAX] = {
	[DEVICE_MT] = "cy,mt",
};

static void set_pdata_ptr(struct cyttsp5_platform_data *pdata)
{
	pdata_ptr[DEVICE_MT].pdata = (void **)&pdata->mt_pdata;
}

static int get_device_type(struct device_node *dev_node,
		enum cyttsp5_device_type *type)
{
	const char *name;
	enum cyttsp5_device_type t;
	int rc;

	rc = of_property_read_string(dev_node, "name", &name);
	if (rc)
		return rc;

	for (t = 0; t < DEVICE_TYPE_MAX; t++)
		if (!strncmp(name, device_names[t], MAX_NAME_LENGTH)) {
			*type = t;
			return 0;
		}

	return -EINVAL;
}

static void *create_and_get_device_pdata(struct device_node *dev_node,
		enum cyttsp5_device_type type)
{
	return device_pdata_funcs[type].create_and_get_pdata(dev_node);
}

static void free_device_pdata(enum cyttsp5_device_type type)
{
	device_pdata_funcs[type].free_pdata(*pdata_ptr[type].pdata);
}

static struct touch_settings *create_and_get_touch_setting(
		struct device_node *core_node, const char *name)
{
	struct touch_settings *setting;
	char *tag_name;
	u32 tag_value;
	u16 *data;
	int size;
	int rc;

	data = create_and_get_u16_array(core_node, name, &size);
	if (IS_ERR_OR_NULL(data))
		return (void *)data;

	pr_debug("%s: Touch setting:'%s' size:%d\n", __func__, name, size);

	setting = kzalloc(sizeof(*setting), GFP_KERNEL);
	if (setting == NULL) {
		rc = -ENOMEM;
		goto fail_free_data;
	}

	setting->data = (u8 *)data;
	setting->size = size;

	tag_name = kzalloc(MAX_NAME_LENGTH, GFP_KERNEL);
	if (tag_name == NULL) {
		rc = -ENOMEM;
		goto fail_free_setting;
	}

	snprintf(tag_name, MAX_NAME_LENGTH, "%s-tag", name);

	rc = of_property_read_u32(core_node, tag_name, &tag_value);
	if (!rc)
		setting->tag = tag_value;

	kfree(tag_name);

	return setting;

fail_free_setting:
	kfree(setting);
fail_free_data:
	kfree(data);

	return ERR_PTR(rc);
}

static void free_touch_setting(struct touch_settings *setting)
{
	if (setting) {
		kfree(setting->data);
		kfree(setting);
	}
}

static char *touch_setting_names[CY_IC_GRPNUM_NUM] = {
	NULL,			/* CY_IC_GRPNUM_RESERVED */
	"cy,cmd_regs",		/* CY_IC_GRPNUM_CMD_REGS */
	"cy,tch_rep",		/* CY_IC_GRPNUM_TCH_REP */
	"cy,data_rec",		/* CY_IC_GRPNUM_DATA_REC */
	"cy,test_rec",		/* CY_IC_GRPNUM_TEST_REC */
	"cy,pcfg_rec",		/* CY_IC_GRPNUM_PCFG_REC */
	"cy,tch_parm_val",	/* CY_IC_GRPNUM_TCH_PARM_VAL */
	"cy,tch_parm_size",	/* CY_IC_GRPNUM_TCH_PARM_SIZE */
	NULL,			/* CY_IC_GRPNUM_RESERVED1 */
	NULL,			/* CY_IC_GRPNUM_RESERVED2 */
	"cy,opcfg_rec",		/* CY_IC_GRPNUM_OPCFG_REC */
	"cy,ddata_rec",		/* CY_IC_GRPNUM_DDATA_REC */
	"cy,mdata_rec",		/* CY_IC_GRPNUM_MDATA_REC */
	"cy,test_regs",		/* CY_IC_GRPNUM_TEST_REGS */
	"cy,btn_keys",		/* CY_IC_GRPNUM_BTN_KEYS */
	NULL,			/* CY_IC_GRPNUM_TTHE_REGS */
};

static struct cyttsp5_core_platform_data *create_and_get_core_pdata(
		struct device_node *core_node)
{
	struct cyttsp5_core_platform_data *pdata;
	u32 value;
	int rc;
	int i;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) {
		rc = -ENOMEM;
		goto fail;
	}

	/* Required fields */
	rc = of_property_read_u32(core_node, "cy,irq_gpio", &value);
	if (rc)
		goto fail_free;
	pdata->irq_gpio = value;

	rc = of_property_read_u32(core_node, "cy,hid_desc_register", &value);
	if (rc)
		goto fail_free;
	pdata->hid_desc_register = value;

	/* Optional fields */
	rc = of_property_read_u32(core_node, "cy,rst_gpio", &value);
	if (!rc)
		pdata->rst_gpio = value;

	rc = of_property_read_u32(core_node, "cy,pwr_1p8", &value);
	if (!rc)
		pdata->pwr_1p8 = value;

	rc = of_property_read_u32(core_node, "cy,pwr_3p3", &value);
	if (!rc)
		pdata->pwr_3p3 = value;

	rc = of_property_read_u32(core_node, "cy,level_irq_udelay", &value);
	if (!rc)
		pdata->level_irq_udelay = value;

	rc = of_property_read_u32(core_node, "cy,vendor_id", &value);
	if (!rc)
		pdata->vendor_id = value;

	rc = of_property_read_u32(core_node, "cy,product_id", &value);
	if (!rc)
		pdata->product_id = value;

	rc = of_property_read_u32(core_node, "cy,flags", &value);
	if (!rc)
		pdata->flags = value;

	for (i = 0; i < ARRAY_SIZE(touch_setting_names); i++) {
		if (touch_setting_names[i] == NULL)
			continue;

		pdata->sett[i] = create_and_get_touch_setting(core_node,
				touch_setting_names[i]);
		if (IS_ERR(pdata->sett[i])) {
			rc = PTR_ERR(pdata->sett[i]);
			goto fail_free_sett;
		} else if (pdata->sett[i] == NULL)
			pr_debug("%s: No data for setting '%s'\n", __func__,
				touch_setting_names[i]);
	}

	pr_info("%s: irq_gpio:%d rst_gpio:%d hid_desc_register:%d\n"
		"level_irq_udelay:%d vendor_id:%d product_id:%d\n"
		"flags:%d pwr_1p8:%d pwr_3p3:%d\n",
		__func__,
		pdata->irq_gpio, pdata->rst_gpio, pdata->hid_desc_register,
		pdata->level_irq_udelay, pdata->vendor_id, pdata->product_id,
		pdata->flags, pdata->pwr_1p8, pdata->pwr_3p3);

	pdata->xres = cyttsp5_xres;
	pdata->init = cyttsp5_init;
	pdata->power = cyttsp5_power;
	pdata->irq_stat = cyttsp5_irq_stat;

	return pdata;

fail_free_sett:
	for (i--; i >= 0; i--)
		free_touch_setting(pdata->sett[i]);
fail_free:
	kfree(pdata);
fail:
	return ERR_PTR(rc);
}

static void free_core_pdata(void *pdata)
{
	struct cyttsp5_core_platform_data *core_pdata = pdata;
	int i;

	for (i = 0; i < ARRAY_SIZE(touch_setting_names); i++)
		free_touch_setting(core_pdata->sett[i]);
	kfree(core_pdata);
}

static int cyttsp5_devtree_create_and_get_pdata(struct device *adap_dev)
{
	struct cyttsp5_platform_data *pdata;
	struct device_node *core_node, *dev_node, *dev_node_fail;
	enum cyttsp5_device_type type;
	int count = 0;
	int rc = 0;

	if (!adap_dev->of_node)
		return 0;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	adap_dev->platform_data = pdata;
	set_pdata_ptr(pdata);

	/* There should be only one core node */
	for_each_child_of_node(adap_dev->of_node, core_node) {
		const char *name;

		rc = of_property_read_string(core_node, "name", &name);
		if (!rc)
			pr_debug("%s: name:%s\n", __func__, name);

		pdata->core_pdata = create_and_get_core_pdata(core_node);
		if (IS_ERR(pdata->core_pdata)) {
			rc = PTR_ERR(pdata->core_pdata);
			break;
		}

		/* Increment reference count */
		of_node_get(core_node);

		for_each_child_of_node(core_node, dev_node) {
			count++;
			rc = get_device_type(dev_node, &type);
			if (rc)
				break;

			*pdata_ptr[type].pdata
				= create_and_get_device_pdata(dev_node, type);
			if (IS_ERR(*pdata_ptr[type].pdata))
				rc = PTR_ERR(*pdata_ptr[type].pdata);
			if (rc)
				break;

			/* Increment reference count */
			of_node_get(dev_node);
		}

		if (rc) {
			free_core_pdata(pdata->core_pdata);
			of_node_put(core_node);
			for_each_child_of_node(core_node, dev_node_fail) {
				if (dev_node == dev_node_fail)
					break;
				rc = get_device_type(dev_node, &type);
				if (rc)
					break;
				free_device_pdata(type);
				of_node_put(dev_node);
			}
			break;
		}

		pdata->loader_pdata = &_cyttsp5_loader_platform_data;
	}

	pr_debug("%s: %d child node(s) found\n", __func__, count);

	return rc;
}

static int cyttsp5_devtree_clean_pdata(struct device *adap_dev)
{
	struct cyttsp5_platform_data *pdata;
	struct device_node *core_node, *dev_node;
	enum cyttsp5_device_type type;
	int rc = 0;

	if (!adap_dev->of_node)
		return 0;

	pdata = dev_get_platdata(adap_dev);
	set_pdata_ptr(pdata);
	for_each_child_of_node(adap_dev->of_node, core_node) {
		free_core_pdata(pdata->core_pdata);
		of_node_put(core_node);
		for_each_child_of_node(core_node, dev_node) {
			rc = get_device_type(dev_node, &type);
			if (rc)
				break;
			free_device_pdata(type);
			of_node_put(dev_node);
		}
	}

	return rc;
}


/*******************************************
 *
 * CYTTSP5 I2C -Driver Registeer
 *
 *******************************************/

#define CY_I2C_DATA_SIZE  (2 * 256)

static int cyttsp5_i2c_read_default(struct device *dev, void *buf, int size)
{
	struct i2c_client *client = to_i2c_client(dev);
	int rc;

	if (!buf || !size || size > CY_I2C_DATA_SIZE)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, size);

	return (rc < 0) ? rc : rc != size ? -EIO : 0;
}

static int cyttsp5_i2c_read_default_nosize(struct device *dev, u8 *buf, u32 max)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;
	u32 size;

	if (!buf)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = (client->flags & I2C_M_TEN) | I2C_M_RD;
	msgs[0].len = 2;
	msgs[0].buf = buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);
	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;

	size = get_unaligned_le16(&buf[0]);
	if (!size || size == 2)
		return 0;

	if (size > max)
		return -EINVAL;

	rc = i2c_master_recv(client, buf, size);

	return (rc < 0) ? rc : rc != (int)size ? -EIO : 0;
}

static int cyttsp5_i2c_write_read_specific(struct device *dev, u8 write_len,
		u8 *write_buf, u8 *read_buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg msgs[2];
	u8 msg_count = 1;
	int rc;

	if (!write_buf || !write_len)
		return -EINVAL;

	msgs[0].addr = client->addr;
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].len = write_len;
	msgs[0].buf = write_buf;
	rc = i2c_transfer(client->adapter, msgs, msg_count);

	if (rc < 0 || rc != msg_count)
		return (rc < 0) ? rc : -EIO;
	else
		rc = 0;

	if (read_buf)
		rc = cyttsp5_i2c_read_default_nosize(dev, read_buf,
				CY_I2C_DATA_SIZE);

	return rc;
}

static struct cyttsp5_bus_ops cyttsp5_i2c_bus_ops = {
	.bustype = BUS_I2C,
	.read_default = cyttsp5_i2c_read_default,
	.read_default_nosize = cyttsp5_i2c_read_default_nosize,
	.write_read_specific = cyttsp5_i2c_write_read_specific,
};

static struct of_device_id cyttsp5_i2c_of_match[] = {
	{ .compatible = "cy,cyttsp5_i2c_adapter", },
	{ }
};
MODULE_DEVICE_TABLE(of, cyttsp5_i2c_of_match);

static int cyttsp5_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id)
{
	struct device *dev = &client->dev;
	const struct of_device_id *match;

	dev_info(&client->dev, "%s\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(dev, "I2C functionality not Supported\n");
		return -EIO;
	}

	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match)
		cyttsp5_devtree_create_and_get_pdata(dev);

	return cyttsp5_probe(&cyttsp5_i2c_bus_ops, &client->dev, client->irq,
			  CY_I2C_DATA_SIZE);
}

static int cyttsp5_i2c_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	const struct of_device_id *match;
	struct cyttsp5_core_data *cd = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s\n", __func__);

	cyttsp5_release(cd);

	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match)
		cyttsp5_devtree_clean_pdata(dev);

	return 0;
}

static void cyttsp5_i2c_shutdown(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	const struct of_device_id *match;
	struct cyttsp5_core_data *cd = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s\n", __func__);

	cyttsp5_release(cd);

	match = of_match_device(of_match_ptr(cyttsp5_i2c_of_match), dev);
	if (match)
		cyttsp5_devtree_clean_pdata(dev);
}

static const struct i2c_device_id cyttsp5_i2c_id[] = {
	{ CYTTSP5_I2C_NAME, 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cyttsp5_i2c_id);

static struct i2c_driver cyttsp5_i2c_driver = {
	.driver = {
		.name = CYTTSP5_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cyttsp5_pm_ops,
		.of_match_table = cyttsp5_i2c_of_match,
	},
	.probe = cyttsp5_i2c_probe,
	.remove = cyttsp5_i2c_remove,
	.shutdown = cyttsp5_i2c_shutdown,
	.id_table = cyttsp5_i2c_id,
};

static int __init cyttsp5_i2c_init(void)
{
	int rc = 0;
#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_notice("%s : LPM Charging Mode!!\n", __func__);
		return rc;
	}
#endif
	rc = i2c_add_driver(&cyttsp5_i2c_driver);

	pr_info("%s: rc=%d\n", __func__, rc);
	return rc;
}
module_init(cyttsp5_i2c_init);

static void __exit cyttsp5_i2c_exit(void)
{
	i2c_del_driver(&cyttsp5_i2c_driver);
}
module_exit(cyttsp5_i2c_exit);

MODULE_ALIAS("i2c:cyttsp5");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard Product I2C driver");
MODULE_AUTHOR("Cypress Semiconductor <ttdrivers@cypress.com>");

