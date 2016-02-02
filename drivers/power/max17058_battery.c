/*
 * Maxim ModelGauge ICs fuel gauge driver
 *
 * Author: Vladimir Barinov <source@cogentembedded.com>
 * Copyright (C) 2016 Cogent Embedded, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#define pr_fmt(fmt)	"MAX17058: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/power/battery-max17058.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#define DRV_NAME "max17058"

/* Register offsets for ModelGauge ICs */
#define MAX17058_VCELL_REG		0x02
#define MAX17058_SOC_REG		0x04
#define MAX17058_MODE_REG		0x06
#define MAX17058_VERSION_REG		0x08
#define MAX17058_HIBRT_REG		0x0A
#define MAX17058_CONFIG_REG		0x0C
#define MAX17058_OCV_REG		0x0E
#define MAX17058_VALRT_REG		0x14
#define MAX17058_CRATE_REG		0x16
#define MAX17058_VRESETID_REG		0x18
#define MAX17058_STATUS_REG		0x1A
#define MAX17058_UNLOCK_REG		0x3E
#define MAX17058_TABLE_REG		0x40
#define MAX17058_RCOMPSEG_REG		0x80
#define MAX17058_CMD_REG		0xFE

/* MODE register bits */
#define MAX17058_MODE_QUICKSTART	(1 << 14)
#define MAX17058_MODE_ENSLEEP		(1 << 13)
#define MAX17058_MODE_HIBSTAT		(1 << 12)

/* CONFIG register bits */
#define MAX17058_CONFIG_SLEEP		(1 << 7)
#define MAX17058_CONFIG_ALSC		(1 << 6)
#define MAX17058_CONFIG_ALRT		(1 << 5)
#define MAX17058_CONFIG_ATHD_MASK	0x1f

/* STATUS register bits */
#define MAX17058_STATUS_ENVR		(1 << 14)
#define MAX17058_STATUS_SC		(1 << 13)
#define MAX17058_STATUS_HD		(1 << 12)
#define MAX17058_STATUS_VR		(1 << 11)
#define MAX17058_STATUS_VL		(1 << 10)
#define MAX17058_STATUS_VH		(1 << 9)
#define MAX17058_STATUS_RI		(1 << 8)

/* VRESETID register bits */
#define MAX17058_VRESETID_DIS		(1 << 8)

#define MAX17058_UNLOCK_VALUE		0x4a57
#define MAX17058_RESET_VALUE		0x5400

#define MAX17058_RCOMP_UPDATE_DELAY	60000
#define MAX17058_LOAD_UPDATE_DELAY	20000

/* Capacity threshold where an interrupt is generated on the ALRT pin */
#define MAX17058_EMPTY_ATHD		15
/* Enable alert for 1% soc change */
#define MAX17058_SOC_CHANGE_ALERT	1
/* Hibernate threshold (crate), where IC enters hibernate mode */
#define MAX17058_HIBRT_THD		20
/* Active threshold (mV), where IC exits hibernate mode */
#define MAX17058_ACTIVE_THD		60
/* Voltage (mV), when IC alerts if battery voltage less then undervoltage */
#define MAX17058_UV			0
/* Voltage (mV), when IC alerts if battery voltage greater then overvoltage */
#define MAX17058_OV			5120
/*
 * Voltage threshold (mV) below which the IC resets itself.
 * Used to detect battery removal and reinsertion
 */
#define MAX17058_RV			2520

enum chip_id {
	ID_MAX17040,
	ID_MAX17041,
	ID_MAX17043,
	ID_MAX17044,
	ID_MAX17048,
	ID_MAX17049,
	ID_MAX17058,
	ID_MAX17059,
};

struct max17058_wakeup_source {
	struct wakeup_source    source;
	unsigned long           disabled;
};

struct max17058_priv {
	struct device			*dev;
	struct regmap			*regmap;
	struct power_supply		battery;
	struct power_supply	*batt_psy;
	struct max17058_platform_data	*pdata;
	enum chip_id			chip;
	struct delayed_work		load_work;
	struct delayed_work		work;
	int				soc_shift;
	struct max17058_wakeup_source max17058_load_wake_source;
	struct dentry			*debug_root;
	bool factory_mode;
};

static void max17058_stay_awake(struct max17058_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(&source->source);
		pr_debug("enabled source %s\n", source->source.name);
	}
}

static void max17058_relax(struct max17058_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(&source->source);
		pr_debug("disabled source %s\n", source->source.name);
	}
}

static void max17058_write_block(struct regmap *regmap, u8 adr, u8 size,
				   u16 *data)
{
	int k;

	/* RAM has different endianness then registers */
	for (k = 0; k < size; k += 2, adr += 2, data++)
		regmap_write(regmap, adr, cpu_to_be16(*data));
}

static int max17058_lsb_to_uvolts(struct max17058_priv *priv, int lsb)
{
	switch (priv->chip) {
	case ID_MAX17040:
	case ID_MAX17043:
		return (lsb >> 4) * 1250; /* 1.25mV per bit */
	case ID_MAX17041:
	case ID_MAX17044:
		return (lsb >> 4) * 2500; /* 2.5mV per bit */
	case ID_MAX17048:
	case ID_MAX17058:
		return lsb * 625 / 8; /* 78.125uV per bit */
	case ID_MAX17049:
	case ID_MAX17059:
		return lsb * 625 / 4; /* 156.25uV per bit */
	default:
		return -EINVAL;
	}
}

static int max17058_get_temp(struct max17058_priv *priv)
{
	/*now return 25, guaranteed to be able to use,
	    after finish read temp fuction*/
	return 250;
}

static int max17058_get_vcell(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int reg;
	int ret;

	ret = regmap_read(regmap, MAX17058_VCELL_REG, &reg);
	if (ret < 0) {
		pr_err(" error!\n");
		return 4000000;
	}

	return max17058_lsb_to_uvolts(priv, reg);
}

static int max17058_get_ocv(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int reg;
	int ret;

	/* Unlock model access */
	regmap_write(regmap, MAX17058_UNLOCK_REG, MAX17058_UNLOCK_VALUE);
	ret = regmap_read(regmap, MAX17058_OCV_REG, &reg);
	/* Lock model access */
	regmap_write(regmap, MAX17058_UNLOCK_REG, 0);
	if (ret < 0) {
		pr_err(" error!\n");
		return 4000000;
	}

	return max17058_lsb_to_uvolts(priv, reg);
}

static int max17058_get_soc(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int reg;
	int ret;
	int soc;

	ret = regmap_read(regmap, MAX17058_SOC_REG, &reg);
	if (ret < 0) {
		pr_err(" error!\n");
		return 50;
	}

	soc = reg / (1 << priv->soc_shift);
	if (soc > 100)
		soc = 100;

	if ((priv->factory_mode) && (soc == 0))
		soc = 50;

	return soc;
}

static int max17058_get_config(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int config;
	int ret;

	ret = regmap_read(regmap, MAX17058_CONFIG_REG, &config);
	if (ret < 0) {
		pr_err(" error!\n");
		return 0;
	}
	return config;
}

static int max17058_get_status(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int status;
	int ret;

	ret = regmap_read(regmap, MAX17058_STATUS_REG, &status);
	if (ret < 0) {
		pr_err(" error!\n");
		return 0;
	}
	return status;
}

static enum power_supply_property max17058_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static int max17058_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct max17058_priv *priv = container_of(psy,
						    struct max17058_priv,
						    battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = max17058_get_vcell(priv);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		val->intval = max17058_get_ocv(priv);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = max17058_get_soc(priv);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = max17058_get_temp(priv);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void max17058_update_rcomp(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	struct max17058_platform_data *pdata = priv->pdata;
	u16 rcomp;
	int temp;

	temp = max17058_get_temp(priv)/10;

	if (!pdata->temp_co_up)
		pdata->temp_co_up = -500;
	if (!pdata->temp_co_down)
		pdata->temp_co_down = -5000;

	rcomp = pdata->rcomp0;
	if (temp > 20)
		rcomp += (temp - 20) * pdata->temp_co_up / 1000;
	else
		rcomp += (temp - 20) * pdata->temp_co_down / 1000;

	/* Update RCOMP */
	regmap_update_bits(regmap, MAX17058_CONFIG_REG, 0xff, rcomp << 8);
}

static void max17058_update_work(struct work_struct *work)
{
	struct max17058_priv *priv = container_of(work,
						    struct max17058_priv,
						    work.work);

	max17058_update_rcomp(priv);
	pr_info("%d,    %d,     %d,    %d,    0x%x,    0x%x\n",
		max17058_get_vcell(priv),
		max17058_get_ocv(priv),
		max17058_get_temp(priv),
		max17058_get_soc(priv),
		max17058_get_config(priv),
		max17058_get_status(priv));

	schedule_delayed_work(&priv->work,
			      msecs_to_jiffies(MAX17058_RCOMP_UPDATE_DELAY));
}

static int max17058_load_model(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	struct max17058_platform_data *pdata = priv->pdata;
	int ret = -EINVAL;
	int timeout, k;
	int ocv, config, soc;

	/* Save CONFIG */
	regmap_read(regmap, MAX17058_CONFIG_REG, &config);
	pr_info("confg register = %d\n", config);

	for (timeout = 0; timeout < 100; timeout++) {
		/* Unlock model access */
		regmap_write(regmap, MAX17058_UNLOCK_REG,
			     MAX17058_UNLOCK_VALUE);

		/* Read OCV */
		regmap_read(regmap, MAX17058_OCV_REG, &ocv);
		if (ocv != 0xffff)
			break;
	}

	if (timeout >= 100) {
		dev_err(priv->dev, "timeout to unlock model access\n");
		ret = -EIO;
		goto exit;
	}

	switch (priv->chip) {
	case ID_MAX17058:
	case ID_MAX17059:
		/* Reset chip transaction does not provide ACK */
		regmap_write(regmap, MAX17058_CMD_REG,
			     MAX17058_RESET_VALUE);
		msleep(150);

		for (timeout = 0; timeout < 100; timeout++) {
			int reg;

			/* Unlock Model Access */
			regmap_write(regmap, MAX17058_UNLOCK_REG,
				     MAX17058_UNLOCK_VALUE);

			/* Read OCV */
			regmap_read(regmap, MAX17058_OCV_REG, &reg);
			if (reg != 0xffff)
				break;
		}

		if (timeout >= 100) {
			dev_err(priv->dev, "timeout to unlock model access\n");
			ret = -EIO;
			goto exit;
		}
		break;
	default:
		break;
	}

	switch (priv->chip) {
	case ID_MAX17040:
	case ID_MAX17041:
	case ID_MAX17043:
	case ID_MAX17044:
		/* Write OCV */
		regmap_write(regmap, MAX17058_OCV_REG, pdata->ocvtest);
		/* Write RCOMP to its maximum value */
		regmap_write(regmap, MAX17058_CONFIG_REG, 0xff00);
		break;
	default:
		break;
	}

	/* Write the model */
	max17058_write_block(regmap, MAX17058_TABLE_REG,
			       MAX17058_TABLE_SIZE,
			       (u16 *)pdata->model_data);

	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049: {
			u16 buf[16];

			if (!pdata->rcomp_seg)
				pdata->rcomp_seg = 0x80;

			for (k = 0; k < 16; k++)
				*buf = pdata->rcomp_seg;

			/* Write RCOMPSeg */
			max17058_write_block(regmap, MAX17058_RCOMPSEG_REG,
					       32, buf);
		}
		break;
	default:
		break;
	}

	switch (priv->chip) {
	case ID_MAX17040:
	case ID_MAX17041:
	case ID_MAX17043:
	case ID_MAX17044:
		/* Delay at least 150ms */
		msleep(150);
		break;
	default:
		break;
	}

	/* Write OCV */
	regmap_write(regmap, MAX17058_OCV_REG, pdata->ocvtest);

	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049:
		/* Disable Hibernate */
		regmap_write(regmap, MAX17058_HIBRT_REG, 0);
		/* fall-through */
	case ID_MAX17058:
	case ID_MAX17059:
		/* Lock Model Access */
		regmap_write(regmap, MAX17058_UNLOCK_REG, 0);
		break;
	default:
		break;
	}

	/* Delay between 150ms and 600ms */
	msleep(200);

	/* Read SOC Register and compare to expected result */
	regmap_read(regmap, MAX17058_SOC_REG, &soc);
	dev_info(priv->dev, "check custom mode soc=%d\n", soc);
	soc >>= 8;
	if (soc >= pdata->soc_check_a && soc <= pdata->soc_check_b)
		ret = 0;

	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049:
	case ID_MAX17058:
	case ID_MAX17059:
		/* Unlock model access */
		regmap_write(regmap, MAX17058_UNLOCK_REG,
			     MAX17058_UNLOCK_VALUE);
		break;
	default:
		break;
	}

	/* Restore CONFIG and OCV */
	regmap_write(regmap, MAX17058_CONFIG_REG, config);
	regmap_write(regmap, MAX17058_OCV_REG, ocv);

	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049:
		/* Restore Hibernate */
		regmap_write(regmap, MAX17058_HIBRT_REG,
			     (MAX17058_HIBRT_THD << 8) |
			     MAX17058_ACTIVE_THD);
		break;
	default:
		break;
	}

exit:
	/* Lock model access */
	regmap_write(regmap, MAX17058_UNLOCK_REG, 0);

	/* Wait 150ms minimum */
	msleep(150);

	return ret;
}

static void max17058_load_mode_handler(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int ret;
	int timeout;
	int reg;

	/* Skip load model if reset indicator cleared */
	regmap_read(regmap, MAX17058_STATUS_REG, &reg);
	if (!(reg & MAX17058_STATUS_RI))
		return;

	for (timeout = 0; timeout < 10; timeout++) {
		/* Load custom model data */
		ret = max17058_load_model(priv);
		if (!ret)
			break;
	}

	if (timeout >= 10) {
		dev_info(priv->dev, "failed to load custom model\n");
		return;
	}

	pr_info("ok\n");

	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049:
	case ID_MAX17058:
	case ID_MAX17059:
		/* Clear reset indicator bit */
		regmap_update_bits(regmap, MAX17058_STATUS_REG,
				   MAX17058_STATUS_RI, 0);
		break;
	default:
		break;
	}
}

static void max17058_load_model_work(struct work_struct *work)
{
	struct max17058_priv *priv = container_of(work,
						    struct max17058_priv,
						    load_work.work);

	max17058_stay_awake(&priv->max17058_load_wake_source);
	max17058_load_mode_handler(priv);
	max17058_relax(&priv->max17058_load_wake_source);

	schedule_delayed_work(&priv->load_work,
			      msecs_to_jiffies(MAX17058_LOAD_UPDATE_DELAY));
}

static irqreturn_t max17058_irq_handler(int id, void *dev)
{
	struct max17058_priv *priv = dev;

	/* clear alert status bit */
	regmap_update_bits(priv->regmap, MAX17058_CONFIG_REG,
			   MAX17058_CONFIG_ALRT, 0);

	power_supply_changed(&priv->battery);
	return IRQ_HANDLED;
}

static int max17058_init(struct max17058_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	struct max17058_platform_data *pdata = priv->pdata;
	int ret;
	int reg;

	ret = regmap_read(regmap, MAX17058_VERSION_REG, &reg);
	if (ret < 0)
		return -ENODEV;

	pr_info("IC production version 0x%04x\n", reg);

	/* Default model uses 8 bits per percent */
	priv->soc_shift = 8;

	if (!priv->pdata) {
		dev_info(priv->dev, "no platform data provided\n");
		return 0;
	}

	switch (pdata->bits) {
	case 19:
		priv->soc_shift = 9;
		break;
	case 18:
	default:
		priv->soc_shift = 8;
		break;
	}

	/* Set RCOMP */
	max17058_update_rcomp(priv);

	/* Clear alert status bit, wake-up, set alert threshold */
	reg = 0;
	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049:
		reg |= MAX17058_SOC_CHANGE_ALERT ? MAX17058_CONFIG_ALSC : 0;
		/* fall-through */
	case ID_MAX17043:
	case ID_MAX17044:
	case ID_MAX17058:
	case ID_MAX17059:
		reg |= 32 - (MAX17058_EMPTY_ATHD << (priv->soc_shift - 8));
		break;
	default:
		break;
	}
	regmap_update_bits(regmap, MAX17058_CONFIG_REG,
			   MAX17058_CONFIG_ALRT | MAX17058_CONFIG_SLEEP |
			   MAX17058_CONFIG_ALSC | MAX17058_CONFIG_ATHD_MASK,
			   reg);

	switch (priv->chip) {
	case ID_MAX17048:
	case ID_MAX17049:
		/* Set Hibernate thresholds */
		reg = (MAX17058_HIBRT_THD * 125 / 26) & 0xff;
		reg <<= 8;
		reg |= (MAX17058_ACTIVE_THD * 4 / 5) & 0xff;
		regmap_write(regmap, MAX17058_HIBRT_REG, reg);

		/* Set undervoltage/overvoltage alerts */
		reg = (MAX17058_UV / 20) & 0xff;
		reg <<= 8;
		reg |= (MAX17058_OV / 20) & 0xff;
		regmap_write(regmap, MAX17058_VALRT_REG, reg);
		/* fall-through */
	case ID_MAX17058:
	case ID_MAX17059:
		/* Disable sleep mode and quick start */
		regmap_write(regmap, MAX17058_MODE_REG, 0);

		/* Setup reset voltage threshold */
		if (MAX17058_RV)
			reg = ((MAX17058_RV / 40) & 0x7f) << 9;
		else
			reg = MAX17058_VRESETID_DIS;
		regmap_write(regmap, MAX17058_VRESETID_REG, reg);
		break;
	default:
		break;
	}

	/* Schedule load custom model work */
	if (pdata->model_data)
		schedule_delayed_work(&priv->load_work, msecs_to_jiffies(0));

	return 0;
}

static int show_max17058_config(struct seq_file *m, void *data)
{
	struct max17058_priv *priv = m->private;
	int i;

	if (!priv || !priv->pdata) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	seq_printf(m, "rcomp0\t=\t%u\n"
			"temp_co_up\t=\t%d\n"
			"temp_co_down\t=\t%d\n"
			"ocvtest\t=\t%u\n"
			"soc_check_a\t=\t%u\n"
			"soc_check_b\t=\t%u\n"
			"bits\t=\t%u\n"
			"batt_psy_name\t=\t%s\n"
			"rcomp_seg\t=\t%u\n",
			priv->pdata->rcomp0,
			priv->pdata->temp_co_up,
			priv->pdata->temp_co_down,
			priv->pdata->ocvtest,
			priv->pdata->soc_check_a,
			priv->pdata->soc_check_b,
			priv->pdata->bits,
			priv->pdata->batt_psy_name,
			priv->pdata->rcomp_seg);

	for (i = 0; i < MAX17058_TABLE_SIZE;  i++)
		seq_printf(m, "model_data\t=\t%x\n",
			*(priv->pdata->model_data + i));

	return 0;
}

static int max17058_config_open(struct inode *inode, struct file *file)
{
	struct max17058_priv *priv = inode->i_private;

	return single_open(file, show_max17058_config, priv);
}

static const struct file_operations max17058_config_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= max17058_config_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_max17058_register(struct seq_file *m, void *data)
{
	struct max17058_priv *priv = m->private;
	struct regmap *regmap = priv->regmap;
	int reg;
	int ret;

	if (!regmap) {
		pr_err("chip not valid\n");
		return -ENODEV;
	}

	ret = regmap_read(regmap, MAX17058_VCELL_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_VCELL_REG\t=\t0x%x\n", reg);
	ret = regmap_read(regmap, MAX17058_SOC_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_SOC_REG\t=\t0x%x\n", reg);
	ret = regmap_read(regmap, MAX17058_VERSION_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_VERSION_REG\t=\t0x%x\n", reg);
	ret = regmap_read(regmap, MAX17058_CONFIG_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_CONFIG_REG\t=\t0x%x\n", reg);
	ret = regmap_read(regmap, MAX17058_HIBRT_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_HIBRT_REG\t=\t0x%x\n", reg);
	ret = regmap_read(regmap, MAX17058_STATUS_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_STATUS_REG\t=\t0x%x\n", reg);
	ret = regmap_read(regmap, MAX17058_VRESETID_REG, &reg);
	if (!ret)
		seq_printf(m, "MAX17058_VRESETID_REG\t=\t0x%x\n", reg);

	return 0;
}

static int max17058_register_open(struct inode *inode, struct file *file)
{
	struct max17058_priv *priv = inode->i_private;

	return single_open(file, show_max17058_register, priv);
}

static const struct file_operations max17058_register_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= max17058_register_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct max17058_platform_data *max17058_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct max17058_platform_data *pdata;
	struct property *prop;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	ret = of_property_read_u8(np, "maxim,rcomp0", &pdata->rcomp0);
	if (ret)
		pdata->rcomp0 = 25;

	ret = of_property_read_u32(np, "maxim,temp-co-up", &pdata->temp_co_up);
	if (ret)
		pdata->temp_co_up = -500;

	ret = of_property_read_u32(np, "maxim,temp-co-down",
				   &pdata->temp_co_down);
	if (ret)
		pdata->temp_co_down = -5000;

	ret = of_property_read_u16(np, "maxim,ocvtest", &pdata->ocvtest);
	if (ret)
		pdata->ocvtest = 0;

	ret = of_property_read_u8(np, "maxim,soc-check-a", &pdata->soc_check_a);
	if (ret)
		pdata->soc_check_a = 0;

	ret = of_property_read_u8(np, "maxim,soc-check-b", &pdata->soc_check_b);
	if (ret)
		pdata->soc_check_b = 0;

	ret = of_property_read_u8(np, "maxim,bits", &pdata->bits);
	if (ret)
		pdata->bits = 18;

	ret = of_property_read_string(np, "maxim,batt-psy-name",
				     &pdata->batt_psy_name);

	ret = of_property_read_u16(np, "maxim,rcomp-seg", &pdata->rcomp_seg);
	if (ret)
		pdata->rcomp_seg = 0;

	prop = of_find_property(np, "maxim,model-data", NULL);
	if (prop && prop->length == MAX17058_TABLE_SIZE) {
		pdata->model_data = devm_kzalloc(dev, MAX17058_TABLE_SIZE,
						 GFP_KERNEL);
		if (!pdata->model_data)
			goto out;

		ret = of_property_read_u8_array(np, "maxim,model-data",
						pdata->model_data,
						MAX17058_TABLE_SIZE);
		if (ret) {
			dev_warn(dev, "failed to get model_data %d\n", ret);
			devm_kfree(dev, pdata->model_data);
			pdata->model_data = NULL;
		}
	}

	pr_debug("rcomp0=%u,temp_co_up=%d, temp_co_down =%d,ovtest=%u,soc_check_a=%u,soc_check_b=%u,bits=%u,batt_psy_name=%s,rcom_seg=%u\n",
			pdata->rcomp0,
			pdata->temp_co_up,
			pdata->temp_co_down,
			pdata->ocvtest,
			pdata->soc_check_a,
			pdata->soc_check_b,
			pdata->bits,
			pdata->batt_psy_name,
			pdata->rcomp_seg);

out:
	return pdata;
}

static const struct regmap_config max17058_regmap = {
	.reg_bits	= 8,
	.val_bits	= 16,
};

static bool max17058_mmi_factory(void)
{
	struct device_node *np = of_find_node_by_path("/chosen");
	u32 fact_cable = 0;

	if (np)
		of_property_read_u32(np, "mmi,factory-cable", &fact_cable);

	of_node_put(np);
	return !!fact_cable ? true : false;
}

static int max17058_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter =  to_i2c_adapter(client->dev.parent);
	struct max17058_priv *priv;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EIO;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (client->dev.of_node)
		priv->pdata = max17058_parse_dt(&client->dev);
	else
		priv->pdata = client->dev.platform_data;

	priv->dev	= &client->dev;
	priv->chip	= id->driver_data;
	priv->factory_mode = false;

	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &max17058_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->battery.name		= "bms";
	priv->battery.type		= POWER_SUPPLY_TYPE_BMS;
	priv->battery.get_property	= max17058_get_property;
	priv->battery.properties	= max17058_battery_props;
	priv->battery.num_properties	= ARRAY_SIZE(max17058_battery_props);

	wakeup_source_init(&priv->max17058_load_wake_source.source,
		"max17058_load_wake");
	INIT_DELAYED_WORK(&priv->load_work, max17058_load_model_work);
	INIT_DELAYED_WORK(&priv->work, max17058_update_work);

	ret = max17058_init(priv);
	if (ret)
		return ret;

	ret = power_supply_register(&client->dev, &priv->battery);
	if (ret) {
		dev_err(priv->dev, "failed: power supply register\n");
		goto err_supply;
	}

	priv->factory_mode = max17058_mmi_factory();
	if (priv->factory_mode)
		dev_info(&client->dev, "Factory Mode\n");

	if (client->irq) {
		switch (priv->chip) {
		case ID_MAX17040:
		case ID_MAX17041:
			dev_err(priv->dev, "alert line is not supported\n");
			ret = -EINVAL;
			goto err_irq;
		default:
			ret = devm_request_threaded_irq(priv->dev, client->irq,
							NULL,
							max17058_irq_handler,
							IRQF_TRIGGER_FALLING,
							priv->battery.name,
							priv);
			if (ret) {
				dev_err(priv->dev, "failed to request irq %d\n",
					client->irq);
				goto err_irq;
			}
		}
	}

	priv->debug_root = debugfs_create_dir("max17058_battery", NULL);
	if (!priv->debug_root)
		pr_err("Couldn't create debug dir\n");

	if (priv->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_file("max17058_config", S_IFREG | S_IRUGO,
					  priv->debug_root, priv,
					  &max17058_config_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create max17058_config debug file\n");

		ent = debugfs_create_file("max17058_register",
				S_IFREG | S_IRUGO, priv->debug_root, priv,
					  &max17058_register_debugfs_ops);
		if (!ent)
			pr_err("Couldn't create  max17058_register debug file\n");

	}

	schedule_delayed_work(&priv->work,
			      msecs_to_jiffies(0));
	return 0;

err_irq:
	power_supply_unregister(&priv->battery);
err_supply:
	wakeup_source_trash(&priv->max17058_load_wake_source.source);
	cancel_delayed_work_sync(&priv->load_work);
	cancel_delayed_work_sync(&priv->work);
	return ret;
}

static int max17058_remove(struct i2c_client *client)
{
	struct max17058_priv *priv = i2c_get_clientdata(client);

	wakeup_source_trash(&priv->max17058_load_wake_source.source);

	cancel_delayed_work_sync(&priv->load_work);
	cancel_delayed_work_sync(&priv->work);

	power_supply_unregister(&priv->battery);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max17058_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17058_priv *priv = i2c_get_clientdata(client);

	switch (priv->chip) {
	case ID_MAX17040:
	case ID_MAX17041:
		return 0;
	default:
		if (client->irq) {
			disable_irq(client->irq);
			enable_irq_wake(client->irq);
		}
	}

	return 0;
}

static int max17058_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct max17058_priv *priv = i2c_get_clientdata(client);

	switch (priv->chip) {
	case ID_MAX17040:
	case ID_MAX17041:
		return 0;
	default:
		if (client->irq) {
			disable_irq_wake(client->irq);
			enable_irq(client->irq);
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(max17058_pm_ops,
			 max17058_suspend, max17058_resume);
#define MAX17058_PM_OPS (&max17058_pm_ops)
#else
#define MAX17058_PM_OPS NULL
#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id max17058_match[] = {
	{ .compatible = "maxim,max17040" },
	{ .compatible = "maxim,max17041" },
	{ .compatible = "maxim,max17043" },
	{ .compatible = "maxim,max17044" },
	{ .compatible = "maxim,max17048" },
	{ .compatible = "maxim,max17049" },
	{ .compatible = "maxim,max17058" },
	{ .compatible = "maxim,max17059" },
	{ },
};
MODULE_DEVICE_TABLE(of, max17058_match);

static const struct i2c_device_id max17058_id[] = {
	{ "max17040", ID_MAX17040 },
	{ "max17041", ID_MAX17041 },
	{ "max17043", ID_MAX17043 },
	{ "max17044", ID_MAX17044 },
	{ "max17048", ID_MAX17048 },
	{ "max17049", ID_MAX17049 },
	{ "max17058", ID_MAX17058 },
	{ "max17059", ID_MAX17059 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17058_id);

static struct i2c_driver max17058_i2c_driver = {
	.driver	= {
		.name		= DRV_NAME,
		.of_match_table	= of_match_ptr(max17058_match),
		.pm		= MAX17058_PM_OPS,
	},
	.probe		= max17058_probe,
	.remove		= max17058_remove,
	.id_table	= max17058_id,
};
module_i2c_driver(max17058_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Barinov");
MODULE_DESCRIPTION("Maxim ModelGauge fuel gauge");
