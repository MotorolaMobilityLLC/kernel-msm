/*
 * max77836.c - mfd core driver for the Maxim 14577
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * SeungJin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max8997.c
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77836.h>
#include <linux/mfd/max77836-private.h>
#include <linux/regulator/machine.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

int max77836_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77836->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77836->i2c_lock);
	if (ret < 0)
		return ret;

	ret &= 0xff;
	*dest = ret;
	return 0;
}

int max77836_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77836->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77836->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}

int max77836_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77836->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77836->i2c_lock);
	return ret;
}

int max77836_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77836->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77836->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}

int max77836_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77836->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&max77836->i2c_lock);
	return ret;
}

int max77836_set_cdetctrl1_reg(struct i2c_client *i2c, u8 val)
{
	pr_info("%s:%s(0x%02x)\n", MFD_DEV_NAME, __func__, val);

	return max77836_write_reg(i2c, MAX77836_REG_CDETCTRL1, val);
}

int max77836_get_cdetctrl1_reg(struct i2c_client *i2c, u8 *val)
{
	u8 value = 0;
	int ret = 0;

	ret = max77836_read_reg(i2c, MAX77836_REG_CDETCTRL1, &value);
	if (ret)
		*val = 0xff;
	else
		*val = value;

	pr_info("%s:%s(0x%02x), ret(%d)\n", MFD_DEV_NAME, __func__, *val, ret);

	return ret;
}

int max77836_set_control2_reg(struct i2c_client *i2c, u8 val)
{
	pr_info("%s:%s(0x%02x)\n", MFD_DEV_NAME, __func__, val);

	return max77836_write_reg(i2c, MAX77836_REG_CONTROL2, val);
}

int max77836_get_control2_reg(struct i2c_client *i2c, u8 *val)
{
	u8 value = 0;
	int ret = 0;

	ret = max77836_read_reg(i2c, MAX77836_REG_CONTROL2, &value);
	if (ret)
		*val = 0xff;
	else
		*val = value;

	pr_info("%s:%s(0x%02x), ret(%d)\n", MFD_DEV_NAME, __func__, *val, ret);

	return ret;
}

static int of_max77836_dt(struct device *dev, struct max77836_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if(!np)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "max77836,irq-gpio",
				0, &pdata->irq_gpio_flags);
	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

#ifdef CONFIG_SPARSE_IRQ
	pdata->irq_base = irq_alloc_descs(-1, 0, MAX77836_IRQ_NUM, -1);
	if (pdata->irq_base < 0) {
		pr_err("%s: irq_alloc_descs Fail ret(%d)\n", __func__, pdata->irq_base);
		return -EFAULT;
	}
#else
	ret = of_property_read_u32(np, "max77836,irq-base", &pdata->irq_base);
	if (ret) {
		pr_err("%s: Failed to read irq-base\n", __func__);
		return -EFAULT;
	}
#endif
	pr_info("%s: irq_base:%d\n", __func__, pdata->irq_base);

	pdata->wakeup = of_property_read_bool(np, "max77836,wakeup");
	return 0;
}

static struct mfd_cell max77836_devs[] = {
	{ .name = "muic-max77836", },
	{ .name = "max77836-safeout", },
#if defined(CONFIG_CHARGER_MAX77836)
	{ .name = "max77836-charger", },
#endif
#if defined(CONFIG_REGULATOR_MAX77836)
	{ .name = "max77836-regulator", },
#endif

};

static int max77836_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct max77836_dev *max77836;
	struct max77836_platform_data *pdata = NULL;
	u8 reg_data;
	int ret = 0;

	printk(KERN_CRIT "%s called\n", __func__);
	max77836 = kzalloc(sizeof(struct max77836_dev), GFP_KERNEL);
	if (max77836 == NULL)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct max77836_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "%s: Failed to allocate memory\n", __func__);
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77836_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "%s: Failed to get device of_node\n", __func__);
			kfree(pdata);
			goto err;
		}

		/*pdata update to other modules*/
		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;
	i2c_set_clientdata(i2c, max77836);
	max77836->dev = &i2c->dev;
	max77836->i2c = i2c;
	max77836->irq = i2c->irq;
	if (pdata) {
		max77836->pdata = pdata;
	} else {
		ret = -EIO;
		goto err;
	}
	pdata->set_cdetctrl1_reg = max77836_set_cdetctrl1_reg;
	pdata->get_cdetctrl1_reg = max77836_get_cdetctrl1_reg;
	pdata->set_control2_reg = max77836_set_control2_reg;
	pdata->get_control2_reg = max77836_get_control2_reg;
#ifdef CONFIG_REGULATOR_MAX77836
	pdata->regulators = max77836_reglator_pdata;
	pdata->num_regulators = MAX77836_LDO_MAX;
#endif

	mutex_init(&max77836->i2c_lock);

	ret = max77836_read_reg(i2c, MAX77836_REG_DEVICEID, &reg_data);
	if (ret < 0) {
		pr_err("%s:%s device not found on this channel(%d)\n",
				MFD_DEV_NAME, __func__, ret);
		goto err;
	} else {
		/* print Device Id */
		max77836->vendor_id = (reg_data & 0x7);
		max77836->device_id = ((reg_data & 0xF8) >> 0x3);
		pr_info("%s:%s device found: vendor=0x%x, device_id=0x%x\n",
				MFD_DEV_NAME, __func__, max77836->vendor_id,
				max77836->device_id);
	}

	max77836->i2c_pmic = i2c_new_dummy(i2c->adapter, MAX77836_PMIC_ADDR);
		i2c_set_clientdata(max77836->i2c_pmic, max77836);

	ret = max77836_irq_init(max77836);
	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(max77836->dev, -1, max77836_devs,
			ARRAY_SIZE(max77836_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	ret = max77836_read_reg(max77836->i2c_pmic,
			MAX77836_PMIC_REG_COMP, &reg_data);
	if (ret < 0)
		pr_err("%s:%s REG_COMP read failed. err:%d\n",
				MFD_DEV_NAME, __func__, ret);
	else {
		pr_info("Prev COMP REG=%02x\n", reg_data);
		reg_data |= BIT_COMPEN;
		ret = max77836_write_reg(max77836->i2c_pmic,
				MAX77836_PMIC_REG_COMP, reg_data);
		if (ret < 0)
			pr_err("%s:%s REG_COMP, write failed. err:%d\n",
					MFD_DEV_NAME, __func__, ret);
		ret = max77836_read_reg(max77836->i2c_pmic,
				MAX77836_PMIC_REG_COMP, &reg_data);
		pr_info("Read COMP REG=%02x\n", reg_data);
	}

	device_init_wakeup(max77836->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(max77836->dev);
err_irq_init:
	if (max77836->i2c_pmic)
		i2c_unregister_device(max77836->i2c_pmic);
err:
	kfree(max77836);
	return ret;
}

static int max77836_i2c_remove(struct i2c_client *i2c)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);

	mfd_remove_devices(max77836->dev);
#ifdef CONFIG_REGULATOR_MAX77836
	i2c_unregister_device(max77836->i2c_pmic);
#endif
	kfree(max77836);

	return 0;
}

static void max77836_i2c_shutdown(struct i2c_client *i2c)
{
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);
	u8 reg_data;
	int ret = 0;

	ret = max77836_read_reg(max77836->i2c_pmic,
			MAX77836_PMIC_REG_COMP, &reg_data);
	if (ret < 0)
		pr_err("%s:%s REG_COMP read failed. err:%d\n",
				MFD_DEV_NAME, __func__, ret);
	else {
		pr_info("Prev COMP REG=%02x\n", reg_data);
		reg_data &= ~BIT_COMPEN;
		ret = max77836_write_reg(max77836->i2c_pmic,
				MAX77836_PMIC_REG_COMP, reg_data);
		if (ret < 0)
			pr_err("%s:%s REG_COMP, write failed. err:%d\n",
					MFD_DEV_NAME, __func__, ret);
		ret = max77836_read_reg(max77836->i2c_pmic,
				MAX77836_PMIC_REG_COMP, &reg_data);
		pr_info("Read COMP REG=%02x\n", reg_data);
	}
}

static const struct i2c_device_id max77836_i2c_id[] = {
	{ MFD_DEV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77836_i2c_id);

static struct of_device_id max77836_i2c_match_table[] = {
	{ .compatible = "max77836,i2c", },
	{ },
};
MODULE_DEVICE_TABLE(of, max77836_i2c_match_table);

#ifdef CONFIG_PM
static int max77836_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(max77836->irq);

	return 0;
}

static int max77836_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77836_dev *max77836 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		disable_irq_wake(max77836->irq);

	return max77836_irq_resume(max77836);
}
#else
#define max77836_suspend	NULL
#define max77836_resume		NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops max77836_pm = {
	.suspend = max77836_suspend,
	.resume = max77836_resume,
};

static struct i2c_driver max77836_i2c_driver = {
	.driver = {
		.name = MFD_DEV_NAME,
		.owner = THIS_MODULE,
		.pm = &max77836_pm,
		.of_match_table = max77836_i2c_match_table,
	},
	.probe = max77836_i2c_probe,
	.remove = max77836_i2c_remove,
	.shutdown = max77836_i2c_shutdown,
	.id_table = max77836_i2c_id,
};

static int __init max77836_i2c_init(void)
{
	int ret;

	ret = i2c_add_driver(&max77836_i2c_driver);
	printk("%s, %d\n",__func__, ret);
	return ret;
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77836_i2c_init);

static void __exit max77836_i2c_exit(void)
{
	i2c_del_driver(&max77836_i2c_driver);
}
module_exit(max77836_i2c_exit);

MODULE_DESCRIPTION("MAXIM 14577 multi-function core driver");
MODULE_AUTHOR("SeungJin Hahn <sjin.hahn@samsung.com>");
MODULE_LICENSE("GPL");
