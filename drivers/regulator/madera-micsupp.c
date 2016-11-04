/*
 * madera-micsupp.c  --  Driver for the mic supply regulator on Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/soc.h>

#include <linux/regulator/madera-micsupp.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/pdata.h>
#include <linux/mfd/madera/registers.h>

struct madera_micsupp {
	struct regulator_dev *regulator;
	struct madera *madera;

	struct regulator_consumer_supply supply;
	struct regulator_init_data init_data;

	struct work_struct check_cp_work;
};

static void madera_micsupp_check_cp(struct work_struct *work)
{
	struct madera_micsupp *micsupp =
		container_of(work, struct madera_micsupp, check_cp_work);
	struct snd_soc_dapm_context *dapm = micsupp->madera->dapm;
	struct madera *madera = micsupp->madera;
	struct regmap *regmap = madera->regmap;
	unsigned int reg;
	int ret;

	if (dapm) {
		ret = regmap_read(regmap, MADERA_MIC_CHARGE_PUMP_1, &reg);
		if (ret) {
			dev_err(madera->dev,
				"Failed to read CP state: %d\n", ret);
			return;
		}

		if ((reg & (MADERA_CPMIC_ENA | MADERA_CPMIC_BYPASS)) ==
		    MADERA_CPMIC_ENA) {
			snd_soc_dapm_enable_pin(dapm, "MICSUPP");
			madera->micvdd_regulated = true;
		} else {
			snd_soc_dapm_disable_pin(dapm, "MICSUPP");
			madera->micvdd_regulated = false;
		}

		snd_soc_dapm_sync(dapm);
	}
}

static int madera_micsupp_enable(struct regulator_dev *rdev)
{
	struct madera_micsupp *micsupp = rdev_get_drvdata(rdev);
	int ret;

	ret = regulator_enable_regmap(rdev);

	if (ret == 0)
		schedule_work(&micsupp->check_cp_work);

	return ret;
}

static int madera_micsupp_disable(struct regulator_dev *rdev)
{
	struct madera_micsupp *micsupp = rdev_get_drvdata(rdev);
	int ret;

	ret = regulator_disable_regmap(rdev);
	if (ret == 0)
		schedule_work(&micsupp->check_cp_work);

	return ret;
}

static int madera_micsupp_set_bypass(struct regulator_dev *rdev, bool ena)
{
	struct madera_micsupp *micsupp = rdev_get_drvdata(rdev);
	int ret;

	ret = regulator_set_bypass_regmap(rdev, ena);
	udelay(1000);
	if (ret == 0)
		schedule_work(&micsupp->check_cp_work);

	return ret;
}

static const struct regulator_ops madera_micsupp_ops = {
	.enable = madera_micsupp_enable,
	.disable = madera_micsupp_disable,
	.is_enabled = regulator_is_enabled_regmap,

	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,

	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,

	.get_bypass = regulator_get_bypass_regmap,
	.set_bypass = madera_micsupp_set_bypass,
};

static const struct regulator_linear_range madera_micsupp_ranges[] = {
	REGULATOR_LINEAR_RANGE(900000,  0,    0x14, 25000),
	REGULATOR_LINEAR_RANGE(1500000, 0x15, 0x27, 100000),
};

static const struct regulator_desc madera_micsupp = {
	.name = "MICVDD",
	.supply_name = "CPVDD1",
	.type = REGULATOR_VOLTAGE,
	.n_voltages = 40,
	.ops = &madera_micsupp_ops,

	.vsel_reg = MADERA_LDO2_CONTROL_1,
	.vsel_mask = MADERA_LDO2_VSEL_MASK,
	.enable_reg = MADERA_MIC_CHARGE_PUMP_1,
	.enable_mask = MADERA_CPMIC_ENA,
	.bypass_reg = MADERA_MIC_CHARGE_PUMP_1,
	.bypass_mask = MADERA_CPMIC_BYPASS,

	.linear_ranges = madera_micsupp_ranges,
	.n_linear_ranges = ARRAY_SIZE(madera_micsupp_ranges),

	.enable_time = 3000,

	.owner = THIS_MODULE,
};

static const struct regulator_init_data madera_micsupp_default = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS |
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_BYPASS,
		.min_uV = 900000,
		.max_uV = 3300000,
	},

	.num_consumer_supplies = 1,
};

static int madera_micsupp_of_get_pdata(struct madera *madera,
					struct regulator_config *config,
					const struct regulator_desc *desc)
{
	struct madera_pdata *pdata = &madera->pdata;
	struct madera_micsupp *micsupp = config->driver_data;
	struct device_node *np;
	struct regulator_init_data *init_data;

	np = of_get_child_by_name(madera->dev->of_node, "micvdd");

	if (np) {
		config->of_node = np;

		init_data = of_get_regulator_init_data(madera->dev, np, desc);

		if (init_data) {
			init_data->consumer_supplies = &micsupp->supply;
			init_data->num_consumer_supplies = 1;

			pdata->micsupp.init_data = init_data;
		}
	}

	return 0;
}

static unsigned int madera_get_max_micbias(struct madera *madera)
{
	unsigned int num_micbias, i, micbias_mv;
	unsigned int max_micbias = 0;
	int ret;

	madera_get_num_micbias(madera, &num_micbias, NULL);

	for (i = 0; i < num_micbias; i++) {
		ret = regmap_read(madera->regmap, MADERA_MIC_BIAS_CTRL_1 + i,
				  &micbias_mv);
		if (ret) {
			dev_err(madera->dev,
				"Failed to read micbias level: %d\n", ret);
			return 0;
		}

		micbias_mv = (micbias_mv & MADERA_MICB1_LVL_MASK) >>
			     MADERA_MICB1_LVL_SHIFT;
		micbias_mv = 1500 + (100 * micbias_mv);

		if (micbias_mv > max_micbias)
			max_micbias = micbias_mv;
	}

	return max_micbias * 1000;
}

static int madera_micsupp_probe(struct platform_device *pdev)
{
	struct madera *madera = dev_get_drvdata(pdev->dev.parent);
	const struct regulator_desc *desc;
	struct regulator_config config = { };
	struct madera_micsupp *micsupp;
	int ret;
	unsigned int max_micbias;

	micsupp = devm_kzalloc(&pdev->dev, sizeof(*micsupp), GFP_KERNEL);
	if (!micsupp)
		return -ENOMEM;

	micsupp->madera = madera;
	INIT_WORK(&micsupp->check_cp_work, madera_micsupp_check_cp);

	/*
	 * Since the chip usually supplies itself we provide some
	 * default init_data for it.  This will be overridden with
	 * platform data if provided.
	 */
	desc = &madera_micsupp;
	micsupp->init_data = madera_micsupp_default;

	micsupp->init_data.consumer_supplies = &micsupp->supply;
	micsupp->supply.supply = "MICVDD";
	micsupp->supply.dev_name = dev_name(madera->dev);

	config.dev = madera->dev;
	config.driver_data = micsupp;
	config.regmap = madera->regmap;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!dev_get_platdata(madera->dev)) {
			ret = madera_micsupp_of_get_pdata(madera, &config,
							  desc);
			if (ret < 0)
				return ret;
		}
	}

	max_micbias = madera_get_max_micbias(madera);
	if (max_micbias) {
		/* micvdd must be 200mV more than maximum micbias */
		max_micbias += 200000;
		if (micsupp->init_data.constraints.max_uV >= max_micbias) {
			micsupp->init_data.constraints.max_uV = max_micbias;
			micsupp->init_data.constraints.min_uV = max_micbias;
			micsupp->init_data.constraints.apply_uV = true;
			micsupp->init_data.constraints.valid_ops_mask &=
				~REGULATOR_CHANGE_VOLTAGE;
		}
	}

	if (madera->pdata.micsupp.init_data)
		config.init_data = madera->pdata.micsupp.init_data;
	else
		config.init_data = &micsupp->init_data;

	if (max_micbias) {
		if (config.init_data->constraints.max_uV < max_micbias)
			dev_err(madera->dev,
				"micvdd must be atleast set to %duV\n",
				max_micbias);
	}

	/* Default to bypassed mode */
	regmap_update_bits(madera->regmap, MADERA_MIC_CHARGE_PUMP_1,
			   MADERA_CPMIC_BYPASS, MADERA_CPMIC_BYPASS);

	micsupp->regulator = devm_regulator_register(&pdev->dev, desc,
						     &config);

	of_node_put(config.of_node);

	if (IS_ERR(micsupp->regulator)) {
		ret = PTR_ERR(micsupp->regulator);
		dev_err(madera->dev, "Failed to register mic supply: %d\n",
			ret);
		return ret;
	}

	platform_set_drvdata(pdev, micsupp);

	return 0;
}

static struct platform_driver madera_micsupp_driver = {
	.probe = madera_micsupp_probe,
	.driver		= {
		.name	= "madera-micsupp",
	},
};

module_platform_driver(madera_micsupp_driver);

/* Module information */
MODULE_DESCRIPTION("Madera microphone supply driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.wolfsonmicro.com>");
MODULE_AUTHOR("Richard Fitzgerald <rf@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:madera-micsupp");
