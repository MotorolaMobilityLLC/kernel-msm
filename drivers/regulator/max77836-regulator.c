/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/max77836.h>
#include <linux/mfd/max77836-private.h>
#include <linux/regulator/max77836-regulator.h>
#include <linux/module.h>

/* Register */
#define REG_CNFG1_LDO1		0x51
#define REG_CNFG2_LDO1		0x52
#define REG_CNFG1_LDO2		0x53
#define REG_CNFG2_LDO2		0x54
#define REG_CNFG_LDO_BIAS	0x55

/* CNFG1_LDOX */
#define CNFG1_LDO_PWR_MD_L_SHIFT	0x6
#define CNFG1_LDO_TV_L_SHIFT		0x0
#define CNFG1_LDO_PWR_MD_L_MASK		(0x3 << CNFG1_LDO_PWR_MD_L_SHIFT)
#define CNFG1_LDO_TV_L_MASK		(0x3F << CNFG1_LDO_TV_L_SHIFT)

/* CNFG2_LDOX */
#define CNFG2_LDO_OVLMP_EN_L_SHIFT		0x7
#define CNFG2_LDO_ALPM_EN_L_SHIFT		0x6
#define CNFG2_LDO_COMP_L_SHIFT			0x4
#define CNFG2_LDO_POK_L_SHIFT			0x3
#define CNFG2_LDO_ADE_L_SHIFT			0x1
#define CNFG2_LDO_SS_L_SHIFT			0x0

/* CNFG_LDO_BIAS */
#define BIT_L_B_LEN			0x02
#define BIT_L_B_EN			0x01


#define MAX77836_LDO_MINUV	800000		/* 0.8V */
#define MAX77836_LDO_MAXUV	3950000		/* 3.95V */
#define MAX77836_LDO_UVSTEP 50000		/* 0.05V */

/* Undefined register address value */
#define REG_RESERVED 0xFF

struct max77836_data {
	struct device *dev;
	struct max77836_dev *mfd_dev;
	struct i2c_client *i2c;
	struct regulator_dev **rdev;
	int *enable_mode;
	int num_regulators;
};

struct voltage_map_desc {
	int min;
	int max;
	int step;
};

static struct voltage_map_desc reg_voltage_map[] = {
	{
		.min = MAX77836_LDO_MINUV,
		.max = MAX77836_LDO_MAXUV,
		.step = MAX77836_LDO_UVSTEP,
	},
	{
		.min = MAX77836_LDO_MINUV,
		.max = MAX77836_LDO_MAXUV,
		.step = MAX77836_LDO_UVSTEP,
	},
};

static int enable_mode[] = {
	[MAX77836_LDO1] = REG_NORMAL_MODE,
	[MAX77836_LDO2] = REG_NORMAL_MODE,
};

static inline int max77836_get_rid(struct regulator_dev *rdev)
{
	return rdev_get_id(rdev);
}

static int max77836_reg_list_voltage(struct regulator_dev *rdev,
	unsigned int selector)
{
	struct voltage_map_desc *vol_desc = NULL;
	int rid = max77836_get_rid(rdev);
	int val;

	if (rid >= ARRAY_SIZE(reg_voltage_map) || rid < 0)
		return -EINVAL;

	vol_desc = &reg_voltage_map[rid];
	if (!vol_desc)
		return -EINVAL;

	val = vol_desc->min + vol_desc->step * selector;
	if (val > vol_desc->max)
		return -EINVAL;

	return val;
}

static int max77836_reg_is_enabled(struct regulator_dev *rdev)
{
	const struct max77836_data *reg_data = rdev_get_drvdata(rdev);
	u8 val, mask;
	int ret;

	ret = max77836_read_reg(reg_data->i2c, rdev->desc->enable_reg, &val);
	if (ret == -EINVAL)
		return 1; /* "not controllable" */
	else if (ret)
		return ret;

	mask = rdev->desc->enable_mask;
	return ((val & mask) == mask ? 1 : 0);
}

static int max77836_reg_enable(struct regulator_dev *rdev)
{
	const struct max77836_data *reg_data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int rid = max77836_get_rid(rdev);
	u8 val = 0;

	pr_info("%s - %s\n", __func__, desc->name);

	val = reg_data->enable_mode[rid] << CNFG1_LDO_PWR_MD_L_SHIFT;

	return max77836_update_reg(reg_data->i2c,
		desc->enable_reg, val, desc->enable_mask);
}

static int max77836_reg_disable(struct regulator_dev *rdev)
{
	const struct max77836_data *reg_data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;


	pr_info("%s - %s\n", __func__, desc->name);

	return max77836_update_reg(reg_data->i2c,
		desc->enable_reg, REG_OUTPUT_DISABLE, desc->enable_mask);
}

static int max77836_reg_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct max77836_data *reg_data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	u8 val = 0;
	int ret;

	ret = max77836_read_reg(reg_data->i2c, desc->enable_reg, &val);
	if (ret)
		return ret;

	val = val & desc->vsel_mask;
	return val;
}

static int max77836_reg_set_voltage_sel(struct regulator_dev *rdev,
	unsigned int selector)
{
	struct max77836_data *reg_data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;

	pr_info("%s - %s - 0x%x\n", __func__, desc->name, selector);

	return max77836_update_reg(reg_data->i2c,
		desc->vsel_reg, selector, desc->vsel_mask);
}

static struct regulator_ops max77836_ldo_ops = {
	.list_voltage		= max77836_reg_list_voltage,
	.is_enabled			= max77836_reg_is_enabled,
	.enable				= max77836_reg_enable,
	.disable			= max77836_reg_disable,
	.get_voltage_sel	= max77836_reg_get_voltage_sel,
	.set_voltage_sel	= max77836_reg_set_voltage_sel,
};

#define regulator_desc_ldo(num) {				\
	.name			= "max77836_l"#num,		\
	.id			= MAX77836_LDO##num,		\
	.ops			= &max77836_ldo_ops,		\
	.type			= REGULATOR_VOLTAGE,		\
	.owner			= THIS_MODULE,			\
	.min_uV			= MAX77836_LDO_MINUV,		\
	.uV_step		= MAX77836_LDO_UVSTEP,		\
	.n_voltages		= CNFG1_LDO_TV_L_MASK + 1,	\
	.vsel_reg		= REG_CNFG1_LDO##num,		\
	.vsel_mask		= CNFG1_LDO_TV_L_MASK,		\
	.enable_reg		= REG_CNFG1_LDO##num,		\
	.enable_mask		= CNFG1_LDO_PWR_MD_L_MASK,	\
}

static struct regulator_desc regulators[] = {
	regulator_desc_ldo(1),
	regulator_desc_ldo(2),
};

/*
static ssize_t max77836_reg_store_ldo_control(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct max77836_data *reg = dev_get_drvdata(dev);
	int ldo_num = 0, val = 0;
	int ret = 0;

	sscanf(buf, "%d %d", &ldo_num, &val);
	if (ldo_num < 0 || ldo_num >= MAX77836_LDO_MAX)
		return -EINVAL;

	val = !!val;

	if (val) {
		ret = max77836_reg_enable(reg->rdev[ldo_num]);
		if (ret)
			pr_err("%s: Failed to enable regulator, %d\n",
					__func__, ret);
	} else {
		ret = max77836_reg_disable(reg->rdev[ldo_num]);
		if (ret)
			pr_err("%s: Failed to disable regulator, %d\n",
					__func__, ret);
	}

	return count;
}


static DEVICE_ATTR(ldo_control, S_IWUGO, NULL, max77836_reg_store_ldo_control);

static struct attribute *max77836_reg_attributes[] = {
	&dev_attr_ldo_control.attr,
	NULL
};


static const struct attribute_group max77836_reg_group = {
	.attrs = max77836_reg_attributes,
};
*/


static int max77836_reg_hw_init(struct max77836_data *max77836_reg,
		struct max77836_reg_platform_data *pdata)
{
	int reg_cnfg2_data, reg_cnfg2_addr;

	reg_cnfg2_data =
		pdata->overvoltage_clamp_enable << CNFG2_LDO_OVLMP_EN_L_SHIFT |
		pdata->auto_low_power_mode_enable << CNFG2_LDO_ALPM_EN_L_SHIFT |
		pdata->compensation_ldo << CNFG2_LDO_COMP_L_SHIFT |
		pdata->active_discharge_enable << CNFG2_LDO_ADE_L_SHIFT |
		pdata->soft_start_slew_rate << CNFG2_LDO_SS_L_SHIFT;

	switch (pdata->reg_id) {
	case MAX77836_LDO1:
		reg_cnfg2_addr = REG_CNFG2_LDO1;
		break;
	case MAX77836_LDO2:
		reg_cnfg2_addr = REG_CNFG2_LDO2;
		break;
	default:
		return -ENODEV;
	}
	return max77836_write_reg(
		max77836_reg->i2c, reg_cnfg2_addr, reg_cnfg2_data);
}

static int max77836_regulator_probe(struct platform_device *pdev)
{
	struct max77836_dev *max77836 = dev_get_drvdata(pdev->dev.parent);
	struct max77836_platform_data *pdata = dev_get_platdata(max77836->dev);
	struct max77836_data *max77836_reg = NULL;
	struct regulator_dev **rdev;
	int rc, size;
	int i;
	struct regulator_config config = { };

	max77836_reg = devm_kzalloc(&pdev->dev,	sizeof(*max77836_reg),
					GFP_KERNEL);
	if (unlikely(max77836_reg == NULL))
		return -ENOMEM;

	dev_info(&pdev->dev, "func: %s\n", __func__);

	max77836_reg->dev = &pdev->dev;
	max77836_reg->mfd_dev = max77836;
	max77836_reg->i2c = max77836->i2c_pmic;
	max77836_reg->enable_mode = enable_mode;
	max77836_reg->num_regulators = pdata->num_regulators;
	platform_set_drvdata(pdev, max77836_reg);

	size = sizeof(struct regulator_dev *) * pdata->num_regulators;
	max77836_reg->rdev = kzalloc(size, GFP_KERNEL);
	if (!max77836_reg->rdev) {
		dev_err(&pdev->dev, "%s: Failed to allocate rdev memory\n",
				__func__);
		kfree(max77836_reg);
		return -ENOMEM;
	}

	rdev = max77836_reg->rdev;

	config.dev = &pdev->dev;
/*	config.regmap = iodev->regmap; */
	config.driver_data = max77836_reg;
/*	platform_set_drvdata(pdev, max77686); */
	for (i = 0; i < max77836_reg->num_regulators; i++) {
		rc = max77836_reg_hw_init(max77836_reg, &pdata->regulators[i]);
		if (IS_ERR_VALUE(rc))
			goto err;

		config.init_data = pdata->regulators[i].init_data;

		rdev[i] = regulator_register(&regulators[i], &config);

		if (IS_ERR_OR_NULL(rdev)) {
			dev_err(&pdev->dev,	"%s: regulator-%d register failed\n",
				__func__, i);
			goto err;
		}
	}

/*
	rc = sysfs_create_group(&max77836_reg->dev->kobj, &max77836_reg_group);
	if (rc)
		dev_err(&pdev->dev,
			"%s: failed to create sysfs attribute group\n",
			__func__);
*/

	return 0;
err:
	dev_err(&pdev->dev, "%s: returns %d\n", __func__, rc);
	while (--i >= 0)
		if (rdev[i])
			regulator_unregister(rdev[i]);

	kfree(rdev);
	kfree(max77836_reg);
	return rc;
}

static int max77836_regulator_remove(struct platform_device *pdev)
{
	struct max77836_data *max77836_reg = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < max77836_reg->num_regulators; i++)
		if (max77836_reg->rdev[i])
			regulator_unregister(max77836_reg->rdev[i]);

	kfree(max77836_reg->rdev);
	kfree(max77836_reg);
	return 0;
}

static const struct platform_device_id max77836_regulator_id[] = {
	{ "max77836-regulator", 0},
	{ },
};

MODULE_DEVICE_TABLE(platform, max77836_regulator_id);

static struct platform_driver max77836_regulator_driver = {
	.driver = {
		.name = "max77836-regulator",
		.owner = THIS_MODULE,
	},
	.probe = max77836_regulator_probe,
	.remove = max77836_regulator_remove,
	.id_table = max77836_regulator_id,
};

static int __init max77836_regulator_init(void)
{
	return platform_driver_register(&max77836_regulator_driver);
}

module_init(max77836_regulator_init);

static void __exit max77836_regulator_exit(void)
{
	platform_driver_unregister(&max77836_regulator_driver);
}

module_exit(max77836_regulator_exit);

MODULE_DESCRIPTION("MAXIM 77836 Regulator Driver");
MODULE_AUTHOR("Clark Kim <clark.kim@maximintegrated.com>");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max77836-regulator");

