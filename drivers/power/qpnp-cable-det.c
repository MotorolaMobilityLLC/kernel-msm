/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2014, Motorola Mobility LLC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"CBL: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/bitops.h>

#define CREATE_MASK(NUM_BITS, POS) \
	((unsigned char) (((1 << (NUM_BITS)) - 1) << (POS)))
#define LBC_MASK(MSB_BIT, LSB_BIT) \
	CREATE_MASK(MSB_BIT - LSB_BIT + 1, LSB_BIT)

/* Interrupt offsets */
#define INT_RT_STS_REG				0x10
#define FAST_CHG_ON_IRQ                         BIT(5)
#define OVERTEMP_ON_IRQ				BIT(4)
#define BAT_TEMP_OK_IRQ                         BIT(1)
#define BATT_PRES_IRQ                           BIT(0)

/* USB CHARGER PATH peripheral register offsets */
#define USB_PTH_STS_REG				0x09
#define USB_IN_VALID_MASK			LBC_MASK(7, 6)
#define USB_SUSP_REG				0x47
#define USB_SUSPEND_BIT				BIT(0)

/* CHARGER peripheral register offset */
#define CHG_USB_ENUM_T_STOP_REG			0x4E

/* MISC peripheral register offset */
#define MISC_BOOT_DONE_REG                     0x42
#define MISC_BOOT_DONE				BIT(7)

#define PERP_SUBTYPE_REG			0x05
#define SEC_ACCESS                              0xD0

/* Linear peripheral subtype values */
#define LBC_CHGR_SUBTYPE			0x15
#define LBC_BAT_IF_SUBTYPE			0x16
#define LBC_USB_PTH_SUBTYPE			0x17
#define LBC_MISC_SUBTYPE			0x18

#define QPNP_CBLDET_DEV_NAME	"qcom,qpnp-cable-detection"

/* usb_interrupts */

struct qpnp_cbldet_irq {
	int		irq;
	unsigned long	disabled;
};

enum {
	USBIN_VALID = 0,
	USB_OVER_TEMP,
	USB_CHG_GONE,
	BATT_PRES,
	BATT_TEMPOK,
	CHG_DONE,
	CHG_FAILED,
	CHG_FAST_CHG,
	CHG_VBAT_DET_LO,
	MAX_IRQS,
};

/*
 * struct qpnp_cbldet_chip - device information
 * @dev:			device pointer to access the parent
 * @spmi:			spmi pointer to access spmi information
 * @usb_chgpth_base:		USB charge path peripheral base address
 * @misc_base:			misc peripheral base address
 * @usb_present:		present status of USB
 * @hw_access_lock:		lock to serialize access to charger registers
 * @usb_psy:			power supply to export information to
 *				userspace
 */
struct qpnp_cbldet_chip {
	struct device			*dev;
	struct spmi_device		*spmi;
	u16				usb_chgpth_base;
	u16				misc_base;
	bool				usb_present;
	int				usb_psy_ma;
	struct qpnp_cbldet_irq		irqs[MAX_IRQS];
	spinlock_t			hw_access_lock;
	struct power_supply		*usb_psy;
	struct delayed_work		heartbeat_work;
};

static int __qpnp_cbldet_read(struct spmi_device *spmi, u16 base,
			u8 *val, int count)
{
	int rc = 0;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, base, val, count);
	if (rc)
		pr_err("SPMI read failed base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);

	return rc;
}

static int __qpnp_cbldet_write(struct spmi_device *spmi, u16 base,
			u8 *val, int count)
{
	int rc;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, base, val,
					count);
	if (rc)
		pr_err("SPMI write failed base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);

	return rc;
}

static int qpnp_cbldet_read(struct qpnp_cbldet_chip *chip, u16 base,
			u8 *val, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags;

	if (base == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->hw_access_lock, flags);
	rc = __qpnp_cbldet_read(spmi, base, val, count);
	spin_unlock_irqrestore(&chip->hw_access_lock, flags);

	return rc;
}

static int qpnp_cbldet_write(struct qpnp_cbldet_chip *chip, u16 base,
			u8 *val, int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;
	unsigned long flags;

	if (base == 0) {
		pr_err("base cannot be zero base=0x%02x sid=0x%02x rc=%d\n",
				base, spmi->sid, rc);
		return -EINVAL;
	}

	spin_lock_irqsave(&chip->hw_access_lock, flags);
	rc = __qpnp_cbldet_write(spmi, base, val, count);
	spin_unlock_irqrestore(&chip->hw_access_lock, flags);

	return rc;
}

static int qpnp_cbldet_is_usb_chg_plugged_in(struct qpnp_cbldet_chip *chip)
{
	u8 usbin_valid_rt_sts;
	int rc;

	rc = qpnp_cbldet_read(chip, chip->usb_chgpth_base + USB_PTH_STS_REG,
				&usbin_valid_rt_sts, 1);
	if (rc) {
		pr_err("spmi read failed: addr=%03X, rc=%d\n",
				chip->usb_chgpth_base + USB_PTH_STS_REG, rc);
		return rc;
	}

	pr_debug("usb_path_sts 0x%x\n", usbin_valid_rt_sts);

	return (usbin_valid_rt_sts & USB_IN_VALID_MASK) ? 1 : 0;
}

#if 0
static int get_prop_charge_type(struct qpnp_cbldet_chip *chip)
{
	int rc;
	u8 reg_val;

	if (!get_prop_batt_present(chip))
		return POWER_SUPPLY_CHARGE_TYPE_NONE;

	rc = qpnp_cbldet_read(chip, chip->chgr_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read interrupt sts %d\n", rc);
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	if (reg_val & FAST_CHG_ON_IRQ)
		return POWER_SUPPLY_CHARGE_TYPE_FAST;

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}
#endif

static int qpnp_cbldet_usb_path_init(struct qpnp_cbldet_chip *chip)
{
	int rc = 0;
	u8 reg_val;

	if (qpnp_cbldet_is_usb_chg_plugged_in(chip)) {
		reg_val = 0;
		rc = qpnp_cbldet_write(chip,
			chip->usb_chgpth_base + CHG_USB_ENUM_T_STOP_REG,
			&reg_val, 1);
		if (rc) {
			pr_err("Failed to write enum stop rc=%d\n", rc);
			return -ENXIO;
		}
	}

	return rc;
}

static int qpnp_cbldet_misc_init(struct qpnp_cbldet_chip *chip)
{
	int rc;
	u8 reg_val;

	pr_debug("Setting BOOT_DONE\n");
	reg_val = MISC_BOOT_DONE;
	rc = qpnp_cbldet_write(chip, chip->misc_base + MISC_BOOT_DONE_REG,
				&reg_val, 1);

	return rc;
}

static irqreturn_t qpnp_cbldet_usbin_valid_irq_handler(int irq, void *_chip)
{
	struct qpnp_cbldet_chip *chip = _chip;
	int usb_present;

	usb_present = qpnp_cbldet_is_usb_chg_plugged_in(chip);
	pr_debug("usbin-valid triggered: %d\n", usb_present);

	if (chip->usb_present ^ usb_present) {
		chip->usb_present = usb_present;

		pr_debug("Updating usb_psy PRESENT property\n");
		power_supply_set_present(chip->usb_psy, chip->usb_present);
	}

	cancel_delayed_work(&chip->heartbeat_work);
	if (chip->usb_present)
		schedule_delayed_work(&chip->heartbeat_work,
						msecs_to_jiffies(5000));

	return IRQ_HANDLED;
}

static int qpnp_cbldet_is_overtemp(struct qpnp_cbldet_chip *chip)
{
	u8 reg_val;
	int rc;

	rc = qpnp_cbldet_read(chip, chip->usb_chgpth_base + INT_RT_STS_REG,
				&reg_val, 1);
	if (rc) {
		pr_err("Failed to read interrupt status rc=%d\n", rc);
		return rc;
	}

	pr_debug("OVERTEMP rt status %x\n", reg_val);
	return (reg_val & OVERTEMP_ON_IRQ) ? 1 : 0;
}

static irqreturn_t qpnp_cbldet_usb_overtemp_irq_handler(int irq, void *_chip)
{
	struct qpnp_cbldet_chip *chip = _chip;
	int overtemp = qpnp_cbldet_is_overtemp(chip);

	pr_warn_ratelimited("charger %s temperature limit !!!\n",
					overtemp ? "exceeds" : "within");

	return IRQ_HANDLED;
}

#define SPMI_REQUEST_IRQ(chip, idx, rc, irq_name, threaded, flags, wake)\
do {									\
	if (rc)								\
		break;							\
	if (chip->irqs[idx].irq) {					\
		if (threaded)						\
			rc = devm_request_threaded_irq(chip->dev,	\
				chip->irqs[idx].irq, NULL,		\
				qpnp_cbldet_##irq_name##_irq_handler,	\
				flags, #irq_name, chip);		\
		else							\
			rc = devm_request_irq(chip->dev,		\
				chip->irqs[idx].irq,			\
				qpnp_cbldet_##irq_name##_irq_handler,	\
				flags, #irq_name, chip);		\
		if (rc < 0) {						\
			pr_err("Unable to request " #irq_name " %d\n",	\
								rc);	\
		} else {						\
			rc = 0;						\
			if (wake)					\
				enable_irq_wake(chip->irqs[idx].irq);	\
		}							\
	}								\
} while (0)

#define SPMI_GET_IRQ_RESOURCE(chip, rc, resource, idx, name)		\
do {									\
	if (rc)								\
		break;							\
									\
	rc = spmi_get_irq_byname(chip->spmi, resource, #name);		\
	if (rc < 0) {							\
		pr_err("Unable to get irq resource " #name "%d\n", rc);	\
	} else {							\
		chip->irqs[idx].irq = rc;				\
		rc = 0;							\
	}								\
} while (0)

static int qpnp_cbldet_request_irqs(struct qpnp_cbldet_chip *chip)
{
	int rc = 0;

	SPMI_REQUEST_IRQ(chip, USBIN_VALID, rc, usbin_valid, 0,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 1);

	SPMI_REQUEST_IRQ(chip, USB_OVER_TEMP, rc, usb_overtemp, 0,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, 0);

	return 0;
}

static int qpnp_cbldet_get_irqs(struct qpnp_cbldet_chip *chip, u8 subtype,
					struct spmi_resource *spmi_resource)
{
	int rc = 0;

	switch (subtype) {
	case LBC_USB_PTH_SUBTYPE:
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						USBIN_VALID, usbin-valid);
		SPMI_GET_IRQ_RESOURCE(chip, rc, spmi_resource,
						USB_OVER_TEMP, usb-over-temp);
		break;
	};

	return 0;
}

/* Get/Set initial state of charger */
static void determine_initial_status(struct qpnp_cbldet_chip *chip)
{
	chip->usb_present = qpnp_cbldet_is_usb_chg_plugged_in(chip);
	power_supply_set_present(chip->usb_psy, chip->usb_present);
	/*
	 * Set USB psy online to avoid userspace from shutting down if battery
	 * capacity is at zero and no chargers online.
	 */
	if (chip->usb_present) {
		power_supply_set_online(chip->usb_psy, 1);
		schedule_delayed_work(&chip->heartbeat_work,
						msecs_to_jiffies(5000));
	}

}

static void heartbeat_work(struct work_struct *work)
{
	struct  qpnp_cbldet_chip *chip =
		container_of(work, struct qpnp_cbldet_chip,
					heartbeat_work.work);
	int usb_present = qpnp_cbldet_is_usb_chg_plugged_in(chip);
	pr_debug("CBL Det HB, usb_present: %d\n", usb_present);

	if (!usb_present && chip->usb_present) {
		pr_err("Missed CBL Det irq, notify usb psy\n");
		chip->usb_present = usb_present;
		power_supply_set_present(chip->usb_psy, chip->usb_present);
	} else
		schedule_delayed_work(&chip->heartbeat_work,
						msecs_to_jiffies(5000));
}

static int qpnp_cbldet_probe(struct spmi_device *spmi)
{
	u8 subtype;
	struct qpnp_cbldet_chip *chip;
	struct resource *resource;
	struct spmi_resource *spmi_resource;
	struct power_supply *usb_psy;
	int rc = 0;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("usb supply not found deferring probe\n");
		return -EPROBE_DEFER;
	}

	chip = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_cbldet_chip),
				GFP_KERNEL);
	if (!chip) {
		pr_err("memory allocation failed.\n");
		return -ENOMEM;
	}

	chip->usb_psy = usb_psy;
	chip->dev = &spmi->dev;
	chip->spmi = spmi;
	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);
	spin_lock_init(&chip->hw_access_lock);
	INIT_DELAYED_WORK(&chip->heartbeat_work, heartbeat_work);

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("spmi resource absent\n");
			rc = -ENXIO;
			goto fail_chg_enable;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
							IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
						spmi->dev.of_node->full_name);
			rc = -ENXIO;
			goto fail_chg_enable;
		}

		rc = qpnp_cbldet_read(chip, resource->start + PERP_SUBTYPE_REG,
					&subtype, 1);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			goto fail_chg_enable;
		}

		switch (subtype) {
		case LBC_USB_PTH_SUBTYPE:
			chip->usb_chgpth_base = resource->start;
			rc = qpnp_cbldet_get_irqs(chip, subtype, spmi_resource);
			if (rc) {
				pr_err("Failed to get USB_PTH irqs rc=%d\n",
						rc);
				goto fail_chg_enable;
			}
			break;
		case LBC_MISC_SUBTYPE:
			chip->misc_base = resource->start;
			break;
		default:
			pr_err("Invalid peripheral subtype=0x%x\n", subtype);
			rc = -EINVAL;
		}
	}

	/* Initialize h/w */
	rc = qpnp_cbldet_usb_path_init(chip);
	if (rc) {
		pr_err("unable to initialize LBC USB path rc=%d\n", rc);
		return rc;
	}
	rc = qpnp_cbldet_misc_init(chip);
	if (rc) {
		pr_err("unable to initialize LBC MISC rc=%d\n", rc);
		return rc;
	}

	/* Get/Set charger's initial status */
	determine_initial_status(chip);

	rc = qpnp_cbldet_request_irqs(chip);
	if (rc) {
		pr_err("unable to initialize LBC MISC rc=%d\n", rc);
		goto fail_request_irqs;
	}

	pr_info("Probe usb=%d\n", qpnp_cbldet_is_usb_chg_plugged_in(chip));

	return 0;

fail_chg_enable:
fail_request_irqs:
	dev_set_drvdata(&spmi->dev, NULL);
	return rc;
}

static int qpnp_cbldet_remove(struct spmi_device *spmi)
{
	dev_set_drvdata(&spmi->dev, NULL);
	return 0;
}

static struct of_device_id qpnp_cbldet_match_table[] = {
	{ .compatible = QPNP_CBLDET_DEV_NAME, },
	{}
};

static struct spmi_driver qpnp_cbldet_driver = {
	.probe		= qpnp_cbldet_probe,
	.remove		= qpnp_cbldet_remove,
	.driver		= {
		.name		= QPNP_CBLDET_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_cbldet_match_table,
	},
};

/*
 * qpnp_cbldet_init() - register spmi driver for qpnp-cable-detect
 */
static int __init qpnp_cbldet_init(void)
{
	return spmi_driver_register(&qpnp_cbldet_driver);
}
module_init(qpnp_cbldet_init);

static void __exit qpnp_cbldet_exit(void)
{
	spmi_driver_unregister(&qpnp_cbldet_driver);
}
module_exit(qpnp_cbldet_exit);

MODULE_DESCRIPTION("QPNP Cable Detection driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_CBLDET_DEV_NAME);
