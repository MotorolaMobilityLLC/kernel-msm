/*
 * Core MFD support for Cirrus Logic Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/registers.h>

#include "madera.h"

#define CS47L35_SILICON_ID	0x6360
#define CS47L85_SILICON_ID	0x6338
#define CS47L90_SILICON_ID	0x6364

struct madera_sysclk_state {
	unsigned int fll;
	unsigned int sysclk;
};

static const char * const madera_core_supplies[] = {
	"AVDD",
	"DBVDD1",
};

static const struct mfd_cell madera_ldo1_devs[] = {
	{ .name = "madera-ldo1" },
};

static const char * const cs47l35_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"CPVDD1",
	"CPVDD2",
	"SPKVDD",
};

static const struct mfd_cell cs47l35_devs[] = {
	{ .name = "madera-irq" },
	{ .name = "madera-micsupp" },
	{ .name = "madera-extcon" },
	{ .name = "madera-gpio" },
	{ .name = "madera-haptics" },
	{ .name = "madera-pwm" },
	{
		.name = "cs47l35-codec",
		.parent_supplies = cs47l35_supplies,
		.num_parent_supplies = ARRAY_SIZE(cs47l35_supplies),
	},
};

static const char * const cs47l85_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"DBVDD3",
	"DBVDD4",
	"CPVDD1",
	"CPVDD2",
	"SPKVDDL",
	"SPKVDDR",
};

static const struct mfd_cell cs47l85_devs[] = {
	{ .name = "madera-irq" },
	{ .name = "madera-micsupp" },
	{ .name = "madera-extcon" },
	{ .name = "madera-gpio" },
	{ .name = "madera-haptics" },
	{ .name = "madera-pwm" },
	{
		.name = "cs47l85-codec",
		.parent_supplies = cs47l85_supplies,
		.num_parent_supplies = ARRAY_SIZE(cs47l85_supplies),
	},
};

static const char * const cs47l90_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"DBVDD3",
	"DBVDD4",
	"CPVDD1",
	"CPVDD2",
};

static const struct mfd_cell cs47l90_devs[] = {
	{ .name = "madera-irq" },
	{ .name = "madera-micsupp" },
	{ .name = "madera-extcon" },
	{ .name = "madera-gpio" },
	{ .name = "madera-haptics" },
	{ .name = "madera-pwm" },
	{
		.name = "cs47l90-codec",
		.parent_supplies = cs47l90_supplies,
		.num_parent_supplies = ARRAY_SIZE(cs47l90_supplies),
	},
};

const char *madera_name_from_type(enum madera_type type)
{
	switch (type) {
	case CS47L35:
		return "CS47L35";
	case CS47L85:
		return "CS47L85";
	case CS47L90:
		return "CS47L90";
	case CS47L91:
		return "CS47L91";
	case WM1840:
		return "WM1840";
	default:
		return "Unknown";
	}
}
EXPORT_SYMBOL_GPL(madera_name_from_type);

#ifdef CONFIG_OF
const struct of_device_id madera_of_match[] = {
	{ .compatible = "cirrus,cs47l35", .data = (void *)CS47L35 },
	{ .compatible = "cirrus,cs47l85", .data = (void *)CS47L85 },
	{ .compatible = "cirrus,cs47l90", .data = (void *)CS47L90 },
	{ .compatible = "cirrus,cs47l91", .data = (void *)CS47L91 },
	{ .compatible = "cirrus,wm1840", .data = (void *)WM1840 },
	{},
};
EXPORT_SYMBOL_GPL(madera_of_match);
#endif

static int madera_poll_reg(struct madera *madera,
			   int timeout, unsigned int reg,
			   unsigned int mask, unsigned int target)
{
	unsigned int val = 0;
	int ret, i;

	for (i = 0; i < timeout; i++) {
		ret = regmap_read(madera->regmap, reg, &val);
		if (ret != 0) {
			dev_err(madera->dev, "Failed to read reg %u: %d\n",
				reg, ret);
			continue;
		}

		if ((val & mask) == target)
			return 0;

		usleep_range(1000, 1010);
	}

	dev_err(madera->dev, "Polling reg %u timed out: %x\n", reg, val);
	return -ETIMEDOUT;
}

static int madera_wait_for_boot(struct madera *madera)
{
	int ret, i;
	unsigned int timeout = 5;
	unsigned int val;

	/* ensure chid can read correctly, then check boot done bit */
	if (madera->type == CS47L35) {
		for (i = 0; i < timeout; i++) {
			ret = regmap_read(madera->regmap, MADERA_SOFTWARE_RESET, &val);
			if (ret != 0) {
				dev_err(madera->dev, "Failed to read chip id, ret %d\n", ret);
				continue;
			}
			if (val == CS47L35_SILICON_ID)
				break;

			usleep_range(1000, 1010);
		}
		if (i >= timeout)
			return -ETIMEDOUT;
	}
	/*
	 * We can't use an interrupt as we need to runtime resume to do so,
	 * we won't race with the interrupt handler as it'll be blocked on
	 * runtime resume.
	 */
	ret = madera_poll_reg(madera, 5, MADERA_IRQ1_RAW_STATUS_1,
			      MADERA_BOOT_DONE_STS1, MADERA_BOOT_DONE_STS1);

	if (!ret)
		regmap_write(madera->regmap, MADERA_IRQ1_STATUS_1,
			     MADERA_BOOT_DONE_EINT1);

	pm_runtime_mark_last_busy(madera->dev);

	return ret;
}

static inline void madera_enable_reset(struct madera *madera)
{
	if (gpio_is_valid(madera->pdata.reset))
		gpio_set_value_cansleep(madera->pdata.reset, 0);
}

static void madera_disable_reset(struct madera *madera)
{
	if (gpio_is_valid(madera->pdata.reset)) {
		gpio_set_value_cansleep(madera->pdata.reset, 1);
		usleep_range(1000, 1010);
	}
}

static int madera_soft_reset(struct madera *madera)
{
	int ret;

	ret = regmap_write(madera->regmap, MADERA_SOFTWARE_RESET, 0);
	if (ret != 0) {
		dev_err(madera->dev, "Failed to soft reset device: %d\n", ret);
		return ret;
	}
	usleep_range(1000, 1010);
	return 0;
}

static int madera_dcvdd_notify(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	struct madera *madera = container_of(nb, struct madera,
					     dcvdd_notifier);

	dev_dbg(madera->dev, "DCVDD notify %lx\n", action);

	if (action & REGULATOR_EVENT_DISABLE)
		madera->dcvdd_powered_off = true;

	return NOTIFY_DONE;
}

#ifdef CONFIG_PM
static int madera_runtime_resume(struct device *dev)
{
	struct madera *madera = dev_get_drvdata(dev);
	bool force_reset = false;
	int ret;

	dev_dbg(madera->dev, "Leaving sleep mode\n");

	/* If DCVDD didn't power off we must force a reset so that the
	 * cache syncs correctly. If we have a hardware reset this must
	 * be done before powering up DCVDD. If not, we'll use a software
	 * reset after powering-up DCVDD
	 */
	if (!madera->dcvdd_powered_off) {
		dev_dbg(madera->dev, "DCVDD did not power off, forcing reset\n");
		force_reset = true;
		madera_enable_reset(madera);
	}

	ret = regulator_enable(madera->dcvdd);
	if (ret) {
		dev_err(madera->dev, "Failed to enable DCVDD: %d\n", ret);
		return ret;
	}

	regcache_cache_only(madera->regmap, false);
	regcache_cache_only(madera->regmap_32bit, false);

	if (force_reset) {
		if (gpio_is_valid(madera->pdata.reset)) {
			madera_disable_reset(madera);
		} else {
			ret = madera_soft_reset(madera);
			if (ret)
				goto err;
		}
	}

	ret = madera_wait_for_boot(madera);
	if (ret)
		goto err;

	mutex_lock(&madera->reg_setting_lock);
	regmap_write(madera->regmap, 0x80, 0x3);
	ret = regcache_sync_region(madera->regmap, MADERA_HP_CHARGE_PUMP_8,
				   MADERA_HP_CHARGE_PUMP_8);
	regmap_write(madera->regmap, 0x80, 0x0);
	mutex_unlock(&madera->reg_setting_lock);

	if (ret) {
		dev_err(madera->dev, "Failed to restore keyed cache\n");
		goto err;
	}

	ret = regcache_sync(madera->regmap);
	if (ret) {
		dev_err(madera->dev,
			"Failed to restore 16-bit register cache\n");
		goto err;
	}

	ret = regcache_sync(madera->regmap_32bit);
	if (ret) {
		dev_err(madera->dev,
			"Failed to restore 32-bit register cache\n");
		goto err;
	}

	return 0;

err:
	regcache_cache_only(madera->regmap_32bit, true);
	regcache_cache_only(madera->regmap, true);
	madera->dcvdd_powered_off = false;
	regulator_disable(madera->dcvdd);
	return ret;
}

static int madera_runtime_suspend(struct device *dev)
{
	struct madera *madera = dev_get_drvdata(dev);

	dev_dbg(madera->dev, "Entering sleep mode\n");

	regcache_cache_only(madera->regmap, true);
	regcache_mark_dirty(madera->regmap);
	regcache_cache_only(madera->regmap_32bit, true);
	regcache_mark_dirty(madera->regmap_32bit);

	madera->dcvdd_powered_off = false;
	regulator_disable(madera->dcvdd);

	return 0;
}
#endif

const struct dev_pm_ops madera_pm_ops = {
	SET_RUNTIME_PM_OPS(madera_runtime_suspend,
			   madera_runtime_resume,
			   NULL)
};
EXPORT_SYMBOL_GPL(madera_pm_ops);

int madera_get_num_micbias(struct madera *madera, unsigned int *n_micbiases,
			   unsigned int *n_child_micbiases)
{
	unsigned int biases, children;

	switch (madera->type) {
	case CS47L35:
		biases = 2;
		children = 2;
		break;
	case CS47L85:
	case WM1840:
		biases = 4;
		children = 0;
		break;
	default:
		biases = 2;
		children = 4;
		break;
	}

	if (n_micbiases)
		*n_micbiases = biases;

	if (n_child_micbiases)
		*n_child_micbiases = children;

	return 0;
}
EXPORT_SYMBOL_GPL(madera_get_num_micbias);

#ifdef CONFIG_OF
unsigned long madera_of_get_type(struct device *dev)
{
	const struct of_device_id *id = of_match_device(madera_of_match, dev);

	if (id)
		return (unsigned long)id->data;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(madera_of_get_type);

static void madera_of_report_error(struct madera *madera, const char *prop,
				   bool mandatory, int err)
{
	switch (err) {
	case -ENOENT:
		if (mandatory)
			dev_err(madera->dev,
				"Mandatory DT property %s is missing\n", prop);
		break;
	default:
		dev_err(madera->dev,
			"DT property %s is malformed: %d\n", prop, err);
		break;
	}
}

int madera_of_read_uint_array(struct madera *madera, const char *prop,
			     bool mandatory,
			     unsigned int *dest, int minlen, int maxlen)
{
	struct device_node *np = madera->dev->of_node;
	struct property *tempprop;
	const __be32 *cur;
	u32 val;
	int n_elems, i, ret;

	n_elems = of_property_count_u32_elems(np, prop);
	if (n_elems < 0) {
		/* of_property_count_u32_elems uses -EINVAL to mean missing */
		if (n_elems == -EINVAL)
			ret = -ENOENT;
		else
			ret = n_elems;
		goto err;
	}
	if (n_elems < minlen) {
		ret = -EOVERFLOW;
		goto err;
	}
	if (n_elems == 0)
		return 0;

	i = 0;
	of_property_for_each_u32(np, prop, tempprop, cur, val) {
		if (i == maxlen)
			break;

		dest[i++] = val;
	}

	return i;

err:
	madera_of_report_error(madera, prop, mandatory, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(madera_of_read_uint_array);

int madera_of_read_uint(struct madera *madera, const char *prop,
		       bool mandatory, unsigned int *data)
{
	u32 value;
	int ret;

	ret = of_property_read_u32(madera->dev->of_node, prop, &value);
	if (ret < 0) {
		/* of_property_read_u32 uses EINVAL to mean missing */
		if (ret == -EINVAL)
			ret = -ENOENT;
		madera_of_report_error(madera, prop, mandatory, ret);
		return ret;
	}

	*data = value;

	return 0;
}
EXPORT_SYMBOL_GPL(madera_of_read_uint);

static int madera_of_get_gpio_defaults(struct madera *madera, const char *prop)
{
	struct madera_pdata *pdata = &madera->pdata;
	int n;


	n = madera_of_read_uint_array(madera, prop, false,
					pdata->gpio_defaults,
					0, ARRAY_SIZE(pdata->gpio_defaults));
	if (n < 0)
		return n;

	/*
	 * All values are literal except out of range values
	 * which are chip default, translate into platform
	 * data which uses 0 as chip default and out of range
	 * as zero.
	 */
	while (n > 0) {
		--n;
		if (pdata->gpio_defaults[n] > 0xffff)
			pdata->gpio_defaults[n] = 0; /* use chip default */
		else if (pdata->gpio_defaults[n] == 0)
			pdata->gpio_defaults[n] = 0xffffffff; /* set to zero */
	}

	return 0;
}

static int madera_of_get_micbias(struct madera *madera,
				 const char *prop, int index)
{
	int ret, i;
	int j = 0;
	u32 micbias_config[4 + MADERA_MAX_CHILD_MICBIAS] = {0};

	BUILD_BUG_ON(ARRAY_SIZE(madera->pdata.micbias[0].discharge) !=
		     MADERA_MAX_CHILD_MICBIAS);

	ret = madera_of_read_uint_array(madera, prop, false, micbias_config,
					ARRAY_SIZE(micbias_config),
					ARRAY_SIZE(micbias_config));

	if (ret > 0) {
		madera->pdata.micbias[index].mV = micbias_config[j++];
		madera->pdata.micbias[index].ext_cap = micbias_config[j++];
		for (i = 0; i < MADERA_MAX_CHILD_MICBIAS; i++)
			madera->pdata.micbias[index].discharge[i] =
				micbias_config[j++];
		madera->pdata.micbias[index].soft_start = micbias_config[j++];
		madera->pdata.micbias[index].bypass = micbias_config[j];
	}

	return ret;
}

static int madera_of_get_core_pdata(struct madera *madera)
{
	struct madera_pdata *pdata = &madera->pdata;

	pdata->reset = of_get_named_gpio(madera->dev->of_node,
					 "reset-gpios", 0);
	if (pdata->reset < 0)
		madera_of_report_error(madera, "reset-gpios", false,
					pdata->reset);

	madera_of_read_uint(madera, "cirrus,clk32k-src", false,
			    &pdata->clk32k_src);

	madera_of_get_micbias(madera, "cirrus,micbias1", 0);
	madera_of_get_micbias(madera, "cirrus,micbias2", 1);
	madera_of_get_micbias(madera, "cirrus,micbias3", 2);
	madera_of_get_micbias(madera, "cirrus,micbias4", 3);

	/* We have to deal with the gpio defaults here. If none of the pins
	 * will be used as a GPIO it's not mandatory to include the GPIO
	 * child driver but the hardware block must still be setup correctly
	 */
	madera_of_get_gpio_defaults(madera, "cirrus,gpio-defaults");

	return 0;
}
#endif

static void madera_configure_gpio(struct madera *madera)
{
	unsigned int val;
	int i;

	/* We can't leave this to the GPIO driver because most of the
	 * pins are used as digital audio interfaces, not GPIOs,
	 * so must be configured correctly on boot#
	 */
	for (i = 0; i < ARRAY_SIZE(madera->pdata.gpio_defaults); i++) {
		val = madera->pdata.gpio_defaults[i];

		if (val == 0)
			continue;	/* leave at chip default */

		if (val > 0xffff)
			val = 0;	/* write as zero */

		regmap_write(madera->regmap, MADERA_GPIO1_CTRL_1 + i, val);
	}
}

static int madera_configure_clk32k(struct madera *madera)
{
	unsigned int src_val;
	int ret = 0;

	switch (madera->pdata.clk32k_src) {
	case 0:
		/* Default to MCLK2 */
		src_val = MADERA_32KZ_MCLK2 - 1;
		break;
	case MADERA_32KZ_MCLK1:
	case MADERA_32KZ_MCLK2:
	case MADERA_32KZ_SYSCLK:
		src_val = (unsigned int)madera->pdata.clk32k_src - 1;
		break;
	default:
		dev_err(madera->dev, "Invalid 32kHz clock source: %d\n",
			madera->pdata.clk32k_src);
		return -EINVAL;
	}

	ret = regmap_update_bits(madera->regmap, MADERA_CLOCK_32K_1,
			MADERA_CLK_32K_ENA_MASK | MADERA_CLK_32K_SRC_MASK,
			MADERA_CLK_32K_ENA | src_val);
	if (ret)
		dev_err(madera->dev, "Failed to init 32kHz clock: %d\n", ret);

	return ret;
}

static void madera_configure_micbias(struct madera *madera)
{
	unsigned int max_micbias = 0, num_child_micbias = 0;
	unsigned int val, mask;
	int i, j;

	madera_get_num_micbias(madera, &max_micbias, &num_child_micbias);

	for (i = 0; i < max_micbias; i++) {
		if (!madera->pdata.micbias[i].mV &&
		    !madera->pdata.micbias[i].bypass)
			continue;

		/* Apply default for bypass mode */
		if (!madera->pdata.micbias[i].mV)
			madera->pdata.micbias[i].mV = 2800;

		val = (madera->pdata.micbias[i].mV - 1500) / 100;

		mask = MADERA_MICB1_LVL_MASK | MADERA_MICB1_EXT_CAP |
			MADERA_MICB1_BYPASS | MADERA_MICB1_RATE;

		val <<= MADERA_MICB1_LVL_SHIFT;

		if (madera->pdata.micbias[i].ext_cap)
			val |= MADERA_MICB1_EXT_CAP;

		if (num_child_micbias == 0) {
			mask |= MADERA_MICB1_DISCH;
			if (madera->pdata.micbias[i].discharge[0])
				val |= MADERA_MICB1_DISCH;
		}

		if (madera->pdata.micbias[i].soft_start)
			val |= MADERA_MICB1_RATE;

		if (madera->pdata.micbias[i].bypass)
			val |= MADERA_MICB1_BYPASS;

		regmap_update_bits(madera->regmap, MADERA_MIC_BIAS_CTRL_1 + i,
				   mask, val);

		if (num_child_micbias) {
			val = 0;
			mask = 0;
			for (j = 0; j < num_child_micbias; j++) {
				mask |= (MADERA_MICB1A_DISCH << (j * 4));
				if (madera->pdata.micbias[i].discharge[j])
					val |= (MADERA_MICB1A_DISCH << (j * 4));
			}
			regmap_update_bits(madera->regmap,
					   MADERA_MIC_BIAS_CTRL_5 + (i * 2),
					   mask, val);
		}
	}
}

int madera_dev_init(struct madera *madera)
{
	struct device *dev = madera->dev;
	const char *name;
	unsigned int hwid, reg;
	int (*patch_fn)(struct madera *) = NULL;
	const struct mfd_cell *mfd_devs;
	int n_devs, i;
	int ret;

	dev_set_drvdata(madera->dev, madera);
	mutex_init(&madera->reg_setting_lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&madera->notifier);

	/* default headphone impedance in case the extcon driver is not used */
	for (i = 0; i < ARRAY_SIZE(madera->hp_impedance_x100); ++i)
		madera->hp_impedance_x100[i] = 3200;

	if (dev_get_platdata(madera->dev)) {
		memcpy(&madera->pdata, dev_get_platdata(madera->dev),
		       sizeof(madera->pdata));

		/* We use 0 in pdata to indicate a GPIO has not been set,
		 * translate to -1 so that gpio_is_valid() will work
		 */
		if (!madera->pdata.reset)
			madera->pdata.reset = -1;
	} else if (IS_ENABLED(CONFIG_OF)) {
		madera_of_get_core_pdata(madera);
	}

	regcache_cache_only(madera->regmap, true);
	regcache_cache_only(madera->regmap_32bit, true);

	for (i = 0; i < ARRAY_SIZE(madera_core_supplies); i++)
		madera->core_supplies[i].supply = madera_core_supplies[i];
	madera->num_core_supplies = ARRAY_SIZE(madera_core_supplies);

	switch (madera->type) {
	case CS47L35:
	case CS47L90:
	case CS47L91:
		break;
	case CS47L85:
	case WM1840:
		ret = mfd_add_devices(madera->dev, -1, madera_ldo1_devs,
			      ARRAY_SIZE(madera_ldo1_devs), NULL, 0, NULL);
		if (ret != 0) {
			dev_err(dev, "Failed to add LDO1 child: %d\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(madera->dev, "Unknown device type %d\n", madera->type);
		return -EINVAL;
	}

	ret = devm_regulator_bulk_get(dev, madera->num_core_supplies,
				      madera->core_supplies);
	if (ret) {
		dev_err(dev, "Failed to request core supplies: %d\n", ret);
		goto err_devs;
	}

	/**
	 * Don't use devres here because the only device we have to get
	 * against is the MFD device and DCVDD will likely be supplied by
	 * one of its children. Meaning that the regulator will be
	 * destroyed by the time devres calls regulator put.
	 */
	madera->dcvdd = regulator_get(madera->dev, "DCVDD");
	if (IS_ERR(madera->dcvdd)) {
		ret = PTR_ERR(madera->dcvdd);
		dev_err(dev, "Failed to request DCVDD: %d\n", ret);
		goto err_devs;
	}

	madera->dcvdd_notifier.notifier_call = madera_dcvdd_notify;
	ret = regulator_register_notifier(madera->dcvdd,
					  &madera->dcvdd_notifier);
	if (ret) {
		dev_err(dev, "Failed to register DCVDD notifier %d\n", ret);
		goto err_dcvdd;
	}

	if (gpio_is_valid(madera->pdata.reset)) {
		/* Start out with /RESET low to put the chip into reset */
		ret = devm_gpio_request_one(madera->dev, madera->pdata.reset,
					    GPIOF_DIR_OUT | GPIOF_INIT_LOW,
					    "madera /RESET");
		if (ret) {
			dev_err(dev, "Failed to request /RESET: %d\n", ret);
			goto err_notifier;
		}
	}

	/* Ensure period of reset asserted before we apply the supplies */
	msleep(20);

	ret = regulator_bulk_enable(madera->num_core_supplies,
				    madera->core_supplies);
	if (ret) {
		dev_err(dev, "Failed to enable core supplies: %d\n", ret);
		goto err_notifier;
	}

	ret = regulator_enable(madera->dcvdd);
	if (ret) {
		dev_err(dev, "Failed to enable DCVDD: %d\n", ret);
		goto err_enable;
	}

	madera_disable_reset(madera);

	regcache_cache_only(madera->regmap, false);
	regcache_cache_only(madera->regmap_32bit, false);

	/* If we don't have a /RESET GPIO use a soft reset */
	if (!madera->pdata.reset) {
		ret = madera_soft_reset(madera);
		if (ret)
			goto err_reset;
	}

	ret = madera_wait_for_boot(madera);
	if (ret) {
		dev_err(madera->dev, "Device failed initial boot: %d\n", ret);
		goto err_reset;
	}
	/* Verify that this is a chip we know about before we
	 * starting doing any writes to its registers
	 */
	ret = regmap_read(madera->regmap, MADERA_SOFTWARE_RESET, &reg);
	if (ret) {
		dev_err(dev, "Failed to read ID register: %d\n", ret);
		goto err_reset;
	}

	switch (reg) {
	case CS47L35_SILICON_ID:
	case CS47L85_SILICON_ID:
	case CS47L90_SILICON_ID:
		break;
	default:
		dev_err(madera->dev, "Unknown device ID: %x\n", reg);
		ret = -EINVAL;
		goto err_reset;
	}

	/* Read the device ID information & do device specific stuff */
	ret = regmap_read(madera->regmap, MADERA_SOFTWARE_RESET, &hwid);
	if (ret) {
		dev_err(dev, "Failed to read ID register: %d\n", ret);
		goto err_reset;
	}

	ret = regmap_read(madera->regmap, MADERA_HARDWARE_REVISION,
			  &madera->rev);
	if (ret) {
		dev_err(dev, "Failed to read revision register: %d\n", ret);
		goto err_reset;
	}
	madera->rev &= MADERA_HW_REVISION_MASK;

	name = madera_name_from_type(madera->type);

	switch (hwid) {
	case CS47L35_SILICON_ID:
		if (IS_ENABLED(CONFIG_MFD_CS47L35)) {
			switch (madera->type) {
			case CS47L35:
				patch_fn = cs47l35_patch;
				mfd_devs = cs47l35_devs;
				n_devs = ARRAY_SIZE(cs47l35_devs);
				break;
			default:
				ret = -EINVAL;
				break;
			}
		} else {
			ret = -EINVAL;
		}
		break;
	case CS47L85_SILICON_ID:
		if (IS_ENABLED(CONFIG_MFD_CS47L85)) {
			switch (madera->type) {
			case CS47L85:
			case WM1840:
				patch_fn = cs47l85_patch;
				mfd_devs = cs47l85_devs;
				n_devs = ARRAY_SIZE(cs47l85_devs);
				break;
			default:
				ret = -EINVAL;
				break;
			}
		} else {
			ret = -EINVAL;
		}
		break;
	case CS47L90_SILICON_ID:
		if (IS_ENABLED(CONFIG_MFD_CS47L90)) {
			switch (madera->type) {
			case CS47L90:
			case CS47L91:
				patch_fn = cs47l90_patch;
				mfd_devs = cs47l90_devs;
				n_devs = ARRAY_SIZE(cs47l90_devs);
				break;
			default:
				ret = -EINVAL;
				break;
			}
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret == -EINVAL) {
		dev_err(madera->dev, "Device ID 0x%x is not a %s\n",
			hwid, name);
		goto err_reset;
	}

	dev_info(dev, "%s silicon revision %d\n", name, madera->rev);

	/* Apply hardware patch */
	if (patch_fn) {
		ret = patch_fn(madera);
		if (ret) {
			dev_err(madera->dev, "Failed to apply patch %d\n", ret);
			goto err_reset;
		}
	}

	madera_configure_gpio(madera);

	ret = madera_configure_clk32k(madera);
	if (ret)
		goto err_reset;

	madera_configure_micbias(madera);

	pm_runtime_set_active(madera->dev);
	pm_runtime_enable(madera->dev);
	pm_runtime_set_autosuspend_delay(madera->dev, 100);
	pm_runtime_use_autosuspend(madera->dev);
	pm_runtime_get(madera->dev);

	ret = mfd_add_devices(madera->dev, -1, mfd_devs, n_devs, NULL, 0, NULL);
	if (ret) {
		dev_err(madera->dev, "Failed to add subdevices: %d\n", ret);
		goto err_reset;
	}

	return 0;

err_reset:
	madera_enable_reset(madera);
	regulator_disable(madera->dcvdd);
err_enable:
	regulator_bulk_disable(madera->num_core_supplies,
			       madera->core_supplies);
err_notifier:
	regulator_unregister_notifier(madera->dcvdd, &madera->dcvdd_notifier);
err_dcvdd:
	regulator_put(madera->dcvdd);
err_devs:
	mfd_remove_devices(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(madera_dev_init);

int madera_dev_exit(struct madera *madera)
{
	pm_runtime_disable(madera->dev);

	regulator_disable(madera->dcvdd);
	regulator_unregister_notifier(madera->dcvdd, &madera->dcvdd_notifier);
	regulator_put(madera->dcvdd);

	mfd_remove_devices(madera->dev);
	madera_enable_reset(madera);

	regulator_bulk_disable(madera->num_core_supplies,
			       madera->core_supplies);
	return 0;
}
EXPORT_SYMBOL_GPL(madera_dev_exit);
