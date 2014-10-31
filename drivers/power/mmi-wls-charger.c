/*
 * Copyright (C) 2012-2014 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 */
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/param.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/printk.h>

#define MMI_WLS_CHRG_DRV_NAME "mmi-wls-charger"
#define MMI_WLS_CHRG_PSY_NAME "wireless"

#define MMI_WLS_CHRG_TEMP_HYS_COLD 2
#define MMI_WLS_CHRG_TEMP_HYS_HOT  5
#define MMI_WLS_CHRG_CHRG_CMPLT_SOC 100
#define MMI_WLS_NUM_GPIOS 3

struct mmi_wls_chrg_chip {
	struct i2c_client *client;
	struct device *dev;

	int num_gpios;
	struct gpio *list;
	unsigned int state;
	unsigned int irq_num;
	struct delayed_work mmi_wls_chrg_work;
	struct power_supply wl_psy;
	struct power_supply *batt_psy;
	struct power_supply *dc_psy;
	struct power_supply *usb_psy;
	int pad_det_n_gpio;
	int charge_cmplt_n_gpio;
	int charge_term_gpio;
	int priority;
	int resume_soc;
	int resume_vbatt;
	int hot_temp;
	int cold_temp;
	struct dentry *debug_root;
	u32 peek_poke_address;
	bool force_shutdown;
};

enum mmi_wls_chrg_state {
	MMI_WLS_CHRG_WAIT = 0,
	MMI_WLS_CHRG_RUNNING,
	MMI_WLS_CHRG_OUT_OF_TEMP_HOT,
	MMI_WLS_CHRG_OUT_OF_TEMP_COLD,
	MMI_WLS_CHRG_CHRG_CMPLT,
	MMI_WLS_CHRG_WIRED_CONN,
};

enum mmi_wls_charger_priority {
	MMI_WLS_CHRG_WIRELESS = 0,
	MMI_WLS_CHRG_WIRED,
};

#define DEFAULT_PRIORITY MMI_WLS_CHRG_WIRED
#define DEFAULT_RESUME_SOC 99
#define DEFAULT_RESUME_VBATT 0
#define DEFAULT_HOT_TEMP 60
#define DEFAULT_COLD_TEMP -20

static int mmi_wls_chrg_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = i2c_smbus_write_word_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int mmi_wls_chrg_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	struct mmi_wls_chrg_chip *chip = data;
	int temp;

	temp = mmi_wls_chrg_read_reg(chip->client, chip->peek_poke_address);

	*val = temp;
	return 0;
}

static int set_reg(void *data, u64 val)
{
	struct mmi_wls_chrg_chip *chip = data;
	int rc;
	u16 temp;

	temp = (u8) val;
	rc = mmi_wls_chrg_write_reg(chip->client,
				    chip->peek_poke_address, temp);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(poke_poke_debug_ops, get_reg, set_reg, "0x%02llx\n");

static int mmi_wls_chrg_get_psy_info(struct power_supply *psy,
				     enum power_supply_property psp, int *data)
{
	union power_supply_propval ret = {0,};

	if (psy) {
		if (psy->get_property) {
			if (!psy->get_property(psy, psp, &ret)) {
				*data = ret.intval;
				return 0;
			}
		}
	}
	pr_err("mmi_wls_chrg_get_psy_info: get_prop Fail!\n");
	return 1;
}

static void mmi_wls_chrg_get_psys(struct mmi_wls_chrg_chip *chip)
{
	int i = 0;
	struct power_supply *psy;

	if (!chip) {
		pr_err_once("Chip not ready\n");
		return;
	}

	for (i = 0; i < chip->wl_psy.num_supplies; i++) {
		psy = power_supply_get_by_name(chip->wl_psy.supplied_from[i]);

		if (psy)
			switch (psy->type) {
			case POWER_SUPPLY_TYPE_BATTERY:
			case POWER_SUPPLY_TYPE_BMS:
				chip->batt_psy = psy;
				break;
			case POWER_SUPPLY_TYPE_MAINS:
			case POWER_SUPPLY_TYPE_WIRELESS:
				chip->dc_psy = psy;
				break;
			case POWER_SUPPLY_TYPE_USB:
			case POWER_SUPPLY_TYPE_USB_DCP:
			case POWER_SUPPLY_TYPE_USB_CDP:
			case POWER_SUPPLY_TYPE_USB_ACA:
			default:
				/* USB type dynamically changes so default */
				chip->usb_psy = psy;
			}
	}

	if (!chip->batt_psy)
		pr_err("BATT PSY Not Found\n");
	if (!chip->dc_psy)
		pr_err("DC PSY Not Found\n");
	if (!chip->usb_psy)
		pr_err("USB PSY Not Found\n");
}
static void mmi_wls_chrg_worker(struct work_struct *work)
{
	int batt_temp;
	int batt_volt;
	int batt_soc;
	int powered = 0;
	int wired = 0;
	int taper_reached;
	struct delayed_work *dwork;
	struct mmi_wls_chrg_chip *chip;

	dwork = to_delayed_work(work);
	chip = container_of(dwork,
			    struct mmi_wls_chrg_chip,
			    mmi_wls_chrg_work);

	if (!chip->dc_psy || !chip->usb_psy || !chip->batt_psy) {
		mmi_wls_chrg_get_psys(chip);
		if (!chip->dc_psy || !chip->usb_psy || !chip->batt_psy)
			return;
	}


	if (mmi_wls_chrg_get_psy_info(chip->dc_psy,
				      POWER_SUPPLY_PROP_PRESENT,
				      &powered)) {
		dev_err(chip->dev, "Error Reading DC Present\n");
		return;
	}


	if (mmi_wls_chrg_get_psy_info(chip->usb_psy,
				      POWER_SUPPLY_PROP_PRESENT,
				      &wired)) {
		dev_err(chip->dev, "Error Reading USB Present\n");
		return;
	}


	if (mmi_wls_chrg_get_psy_info(chip->batt_psy,
				      POWER_SUPPLY_PROP_TEMP,
				      &batt_temp)) {
		dev_err(chip->dev, "Error Reading Temperature\n");
		return;
	}
	/* Convert Units to Celsius */
	batt_temp /= 10;

	if (mmi_wls_chrg_get_psy_info(chip->batt_psy,
				      POWER_SUPPLY_PROP_VOLTAGE_NOW,
				      &batt_volt)) {
		dev_err(chip->dev, "Error Reading Voltage\n");
		return;
	}

	if (mmi_wls_chrg_get_psy_info(chip->batt_psy,
				      POWER_SUPPLY_PROP_CAPACITY,
				      &batt_soc)) {
		dev_err(chip->dev, "Error Reading Capacity\n");
		return;
	}

	if (mmi_wls_chrg_get_psy_info(chip->batt_psy,
				      POWER_SUPPLY_PROP_TAPER_REACHED,
				      &taper_reached)) {
		dev_err(chip->dev, "Error Reading Taper Reached status\n");
		return;
	}

	if (batt_soc == 0)
		chip->force_shutdown = true;

	dev_dbg(chip->dev, "State Before = %d\n", chip->state);

	switch (chip->state) {
	case MMI_WLS_CHRG_WAIT:
		if (wired && (chip->priority == MMI_WLS_CHRG_WIRED)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 0);
			chip->state = MMI_WLS_CHRG_WIRED_CONN;
		} else if (powered) {
			chip->state = MMI_WLS_CHRG_RUNNING;
		}
		break;
	case MMI_WLS_CHRG_WIRED_CONN:
		if (!wired) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 1);
			chip->state = MMI_WLS_CHRG_WAIT;
		}
		break;
	case MMI_WLS_CHRG_RUNNING:
		if (wired && (chip->priority == MMI_WLS_CHRG_WIRED)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 0);
			chip->state = MMI_WLS_CHRG_WIRED_CONN;
		} else if (!powered) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 1);
			gpio_set_value(chip->charge_term_gpio, 0);
			chip->state = MMI_WLS_CHRG_WAIT;
		} else if (batt_temp >= chip->hot_temp) {
			gpio_set_value(chip->charge_term_gpio, 1);
			chip->state = MMI_WLS_CHRG_OUT_OF_TEMP_HOT;
		} else if (batt_temp <= chip->cold_temp) {
			gpio_set_value(chip->charge_term_gpio, 1);
			chip->state = MMI_WLS_CHRG_OUT_OF_TEMP_COLD;
		} else if (taper_reached) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 0);
			chip->state = MMI_WLS_CHRG_CHRG_CMPLT;
		}
		break;
	case MMI_WLS_CHRG_OUT_OF_TEMP_HOT:
		if (wired && (chip->priority == MMI_WLS_CHRG_WIRED)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 0);
			chip->state = MMI_WLS_CHRG_WIRED_CONN;
		} else if (batt_temp < (chip->hot_temp -
					MMI_WLS_CHRG_TEMP_HYS_HOT)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 1);
			gpio_set_value(chip->charge_term_gpio, 0);
			chip->state = MMI_WLS_CHRG_WAIT;
		}
		break;
	case MMI_WLS_CHRG_OUT_OF_TEMP_COLD:
		if (wired && (chip->priority == MMI_WLS_CHRG_WIRED)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 0);
			chip->state = MMI_WLS_CHRG_WIRED_CONN;
		} else if (batt_temp > (chip->cold_temp +
					MMI_WLS_CHRG_TEMP_HYS_COLD)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 1);
			gpio_set_value(chip->charge_term_gpio, 0);
			chip->state = MMI_WLS_CHRG_WAIT;
		}
		break;
	case MMI_WLS_CHRG_CHRG_CMPLT:
		if (wired && (chip->priority == MMI_WLS_CHRG_WIRED)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 0);
			chip->state = MMI_WLS_CHRG_WIRED_CONN;
		} else if ((batt_soc <= chip->resume_soc) ||
			   (batt_volt <= chip->resume_vbatt)) {
			gpio_set_value(chip->charge_cmplt_n_gpio, 1);
			gpio_set_value(chip->charge_term_gpio, 0);
			chip->state = MMI_WLS_CHRG_WAIT;
		}
		break;
	}

	dev_dbg(chip->dev, "State After = %d\n", chip->state);

	return;
}

static enum power_supply_property mmi_wls_chrg_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int mmi_wls_chrg_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct mmi_wls_chrg_chip *chip;
	int powered = 0;

	if (!psy || !psy->dev || !val) {
		pr_err("mmi_wls_chrg_get_property NO dev!\n");
		return -ENODEV;
	}

	chip = dev_get_drvdata(psy->dev->parent);
	if (!chip) {
		pr_err("mmi_wls_chrg_get_property NO dev chip!\n");
		return -ENODEV;
	}

	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = !gpio_get_value(chip->pad_det_n_gpio);
		if (chip->force_shutdown)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (chip->dc_psy) {
			mmi_wls_chrg_get_psy_info(chip->dc_psy,
				      POWER_SUPPLY_PROP_PRESENT, &powered);
			val->intval = powered;
		} else {
			val->intval = (!gpio_get_value(chip->pad_det_n_gpio) &&
			       gpio_get_value(chip->charge_cmplt_n_gpio) &&
			       !gpio_get_value(chip->charge_term_gpio));
		}
		if (chip->force_shutdown)
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void mmi_wls_charger_external_power_changed(struct power_supply *psy)
{
	struct mmi_wls_chrg_chip *chip = container_of(psy,
						      struct mmi_wls_chrg_chip,
						      wl_psy);

	dev_dbg(chip->dev, "mmi_wls_charger External Change Reported!\n");

	schedule_delayed_work(&chip->mmi_wls_chrg_work,
			      msecs_to_jiffies(100));
	return;
}

static irqreturn_t pad_det_handler(int irq, void *dev_id)
{

	struct mmi_wls_chrg_chip *chip = dev_id;

	if (gpio_get_value(chip->pad_det_n_gpio))
		power_supply_changed(&chip->wl_psy);

	dev_dbg(chip->dev, "pad_det_handler pad_det_n =%x\n",
			gpio_get_value(chip->pad_det_n_gpio));

	return IRQ_HANDLED;
}

static const struct of_device_id mmi_wls_chrg_of_tbl[] = {
	{ .compatible = "mmi,wls-charger-bq51021", .data = NULL},
	{},
};

static int mmi_wls_chrg_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret = 0;
	int i;
	int gpio_count;
	char **sf;
	enum of_gpio_flags flags;
	const struct of_device_id *match;
	struct mmi_wls_chrg_chip *chip;
	struct device_node *np = client->dev.of_node;
	dev_info(&client->dev, "Probe of mmi_wls_charger\n");
	if (!np) {
		dev_err(&client->dev, "No OF DT node found.\n");
		return -ENODEV;
	}

	match = of_match_device(mmi_wls_chrg_of_tbl, &client->dev);
	if (!match) {
		dev_err(&client->dev, "No Match found\n");
		return -ENODEV;
	}

	if (match && match->compatible)
		dev_info(&client->dev, "Using %s\n", match->compatible);

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;

	gpio_count = of_gpio_count(np);

	if (!gpio_count) {
		dev_err(&client->dev, "No GPIOS defined.\n");
		return -ENODEV;
	}

	/* Make sure number of GPIOs defined matches the supplied number of
	 * GPIO name strings.
	 */
	if (gpio_count != of_property_count_strings(np, "gpio-names")) {
		dev_err(&client->dev, "GPIO info and name mismatch\n");
		return -ENODEV;
	}

	chip->list = devm_kzalloc(&client->dev,
				sizeof(struct gpio) * gpio_count,
				GFP_KERNEL);
	if (!chip->list) {
		dev_err(&client->dev, "GPIO List allocate failure\n");
		return -ENOMEM;
	}

	chip->num_gpios = gpio_count;
	for (i = 0; i < gpio_count; i++) {
		chip->list[i].gpio = of_get_gpio_flags(np, i, &flags);
		chip->list[i].flags = flags;
		of_property_read_string_index(np, "gpio-names", i,
						&chip->list[i].label);
		dev_dbg(&client->dev, "GPIO: %d  FLAGS: %ld  LABEL: %s\n",
			chip->list[i].gpio, chip->list[i].flags,
			chip->list[i].label);
	}

	ret = gpio_request_array(chip->list, chip->num_gpios);
	if (ret) {
		dev_err(&client->dev, "failed to request GPIOs\n");
		return -ENODEV;
	}

	for (i = 0; i < chip->num_gpios; i++) {
		ret = gpio_export(chip->list[i].gpio, 1);
		if (ret) {
			dev_err(&client->dev, "Failed to export GPIO %s: %d\n",
				chip->list[i].label, chip->list[i].gpio);
			goto fail_gpios;
		}

		ret = gpio_export_link(&client->dev, chip->list[i].label,
					chip->list[i].gpio);
		if (ret) {
			dev_err(&client->dev, "Failed to link GPIO %s: %d\n",
				chip->list[i].label, chip->list[i].gpio);
			goto fail_gpios;
		}
	}

	if (chip->num_gpios == MMI_WLS_NUM_GPIOS) {
		chip->pad_det_n_gpio = chip->list[0].gpio;
		chip->charge_cmplt_n_gpio = chip->list[1].gpio;
		chip->charge_term_gpio = chip->list[2].gpio;
	}

	if (chip->pad_det_n_gpio) {
		int irq;
		irq = gpio_to_irq(chip->pad_det_n_gpio);
		if (irq < 0) {
			dev_err(&client->dev, " gpio_to_irq failed\n");
			ret = -ENODEV;
			goto fail_gpios;
		}
		ret = devm_request_irq(&client->dev, irq, pad_det_handler,
					IRQF_TRIGGER_RISING, "pad_det_n", chip);
		if (ret < 0) {
			dev_err(&client->dev, "request pad_det irq failed\n");
			goto fail_gpios;
		}
	}

	ret = of_property_read_u32(np, "mmi,priority",
						&chip->priority);
	if ((ret < 0) || (chip->priority == 0))
		chip->priority  = DEFAULT_PRIORITY;

	ret = of_property_read_u32(np, "mmi,resume-soc",
						&chip->resume_soc);
	if ((ret < 0) || (chip->resume_soc == 0))
		chip->resume_soc = DEFAULT_RESUME_SOC;

	ret = of_property_read_u32(np, "mmi,resume-vbatt-mv",
						&chip->resume_vbatt);
	if ((ret < 0) || (chip->resume_vbatt == 0))
		chip->resume_vbatt = DEFAULT_RESUME_VBATT;

	ret = of_property_read_u32(np, "mmi,hot-temp-thresh",
						&chip->hot_temp);
	if ((ret < 0) || (chip->hot_temp == 0))
		chip->hot_temp = DEFAULT_HOT_TEMP;

	ret = of_property_read_u32(np, "mmi,cold-temp-thresh",
				   (u32 *)&chip->cold_temp);
	if ((ret < 0) || (chip->cold_temp == 0))
		chip->cold_temp = DEFAULT_COLD_TEMP;

	chip->state = MMI_WLS_CHRG_WAIT;
	INIT_DELAYED_WORK(&chip->mmi_wls_chrg_work, mmi_wls_chrg_worker);

	chip->wl_psy.num_supplies =
		of_property_count_strings(np, "supply-names");

	chip->wl_psy.supplied_from =
		devm_kzalloc(&client->dev,
			     sizeof(char *) *
			     chip->wl_psy.num_supplies,
			     GFP_KERNEL);

	if (!chip->wl_psy.supplied_from) {
		dev_err(&client->dev,
			"mmi_wls fail create supplied_from\n");
		goto fail_gpios;
	}


	for (i = 0; i < chip->wl_psy.num_supplies; i++) {
		sf = &chip->wl_psy.supplied_from[i];
		of_property_read_string_index(np, "supply-names", i,
					      (const char **)sf);
		dev_info(&client->dev, "supply name: %s\n",
			chip->wl_psy.supplied_from[i]);
	}

	chip->wl_psy.name = MMI_WLS_CHRG_PSY_NAME;
	chip->wl_psy.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wl_psy.properties = mmi_wls_chrg_props;
	chip->wl_psy.num_properties = ARRAY_SIZE(mmi_wls_chrg_props);
	chip->wl_psy.get_property = mmi_wls_chrg_get_property;
	chip->wl_psy.external_power_changed =
		mmi_wls_charger_external_power_changed;
	ret = power_supply_register(&client->dev, &chip->wl_psy);
	if (ret < 0) {
		dev_err(&client->dev,
			"power_supply_register wireless failed ret = %d\n",
			ret);
		goto fail_gpios;
	}

	mmi_wls_chrg_get_psys(chip);

	chip->debug_root = debugfs_create_dir("mmi_wls_chrg", NULL);
	if (!chip->debug_root)
		dev_err(&client->dev, "Couldn't create debug dir\n");

	if (chip->debug_root) {
		struct dentry *ent;

		ent = debugfs_create_x32("address",
					 S_IFREG | S_IWUSR | S_IRUGO,
					 chip->debug_root,
					 &(chip->peek_poke_address));
		if (!ent)
			dev_err(&client->dev,
				"Couldn't create address debug file\n");

		ent = debugfs_create_file("data",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  chip->debug_root, chip,
					  &poke_poke_debug_ops);
		if (!ent)
			dev_err(&client->dev,
				"Couldn't create data debug file\n");
	}

	i2c_set_clientdata(client, chip);

	chip->force_shutdown = false;

	schedule_delayed_work(&chip->mmi_wls_chrg_work,
			      msecs_to_jiffies(100));

	return 0;

fail_gpios:
	gpio_free_array(chip->list, chip->num_gpios);
	return ret;
}

static int mmi_wls_chrg_remove(struct i2c_client *client)
{
	struct mmi_wls_chrg_chip *chip = i2c_get_clientdata(client);

	if (chip)
		gpio_free_array(chip->list, chip->num_gpios);

	return 0;
}

static const struct i2c_device_id mmi_wls_charger_id[] = {
	{"mmi-wls-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mmi_wls_charger_charger_id);

static struct i2c_driver mmi_wls_charger_driver = {
	.driver		= {
		.name		= MMI_WLS_CHRG_DRV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= mmi_wls_chrg_of_tbl,
	},
	.probe		= mmi_wls_chrg_probe,
	.remove		= mmi_wls_chrg_remove,
	.id_table	= mmi_wls_charger_id,
};
module_i2c_driver(mmi_wls_charger_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("mmi-wls-charger driver");
MODULE_ALIAS("i2c:mmi-wls-charger");
