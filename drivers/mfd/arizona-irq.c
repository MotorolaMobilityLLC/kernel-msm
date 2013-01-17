/*
 * Arizona interrupt support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"

static int arizona_map_irq(struct arizona *arizona, int irq)
{
	switch (irq) {
	case ARIZONA_IRQ_GP5_FALL:
	case ARIZONA_IRQ_GP5_RISE:
	case ARIZONA_IRQ_JD_FALL:
	case ARIZONA_IRQ_JD_RISE:
	case ARIZONA_IRQ_MICD_CLAMP_FALL:
	case ARIZONA_IRQ_MICD_CLAMP_RISE:
		return arizona->pdata.irq_base + 2 + irq;
	default:
		return arizona->pdata.irq_base + 2 + ARIZONA_NUM_IRQ + irq;
	}
}

int arizona_request_irq(struct arizona *arizona, int irq, char *name,
			   irq_handler_t handler, void *data)
{
	irq = arizona_map_irq(arizona, irq);
	if (irq < 0)
		return irq;

	return request_threaded_irq(irq, NULL, handler, IRQF_ONESHOT,
				    name, data);
}
EXPORT_SYMBOL_GPL(arizona_request_irq);

void arizona_free_irq(struct arizona *arizona, int irq, void *data)
{
	irq = arizona_map_irq(arizona, irq);
	if (irq < 0)
		return;

	free_irq(irq, data);
}
EXPORT_SYMBOL_GPL(arizona_free_irq);

int arizona_set_irq_wake(struct arizona *arizona, int irq, int on)
{
	int val = 0;

	if (on) {
		val = 0xffff;
		irq_set_irq_wake(arizona->irq, 1);
	} else {
		irq_set_irq_wake(arizona->irq, 0);
	}

	switch (irq) {
	case ARIZONA_IRQ_MICD_CLAMP_RISE:
		return regmap_update_bits(arizona->regmap,
					  ARIZONA_WAKE_CONTROL,
					  val & ARIZONA_WKUP_MICD_CLAMP_RISE,
					  val & ARIZONA_WKUP_MICD_CLAMP_RISE);
	case ARIZONA_IRQ_MICD_CLAMP_FALL:
		return regmap_update_bits(arizona->regmap,
					  ARIZONA_WAKE_CONTROL,
					  val & ARIZONA_WKUP_MICD_CLAMP_FALL,
					  val & ARIZONA_WKUP_MICD_CLAMP_FALL);
	case ARIZONA_IRQ_GP5_FALL:
		return regmap_update_bits(arizona->regmap,
					  ARIZONA_WAKE_CONTROL,
					  val & ARIZONA_WKUP_GP5_RISE,
					  val & ARIZONA_WKUP_GP5_RISE);
	case ARIZONA_IRQ_GP5_RISE:
		return regmap_update_bits(arizona->regmap,
					  ARIZONA_WAKE_CONTROL,
					  val & ARIZONA_WKUP_GP5_FALL,
					  val & ARIZONA_WKUP_GP5_FALL);
	case ARIZONA_IRQ_JD_RISE:
		return regmap_update_bits(arizona->regmap,
					  ARIZONA_WAKE_CONTROL,
					  val & ARIZONA_WKUP_JD1_RISE,
					  val & ARIZONA_WKUP_JD1_RISE);
	case ARIZONA_IRQ_JD_FALL:
		return regmap_update_bits(arizona->regmap,
					  ARIZONA_WAKE_CONTROL,
					  val & ARIZONA_WKUP_JD1_FALL,
					  val & ARIZONA_WKUP_JD1_FALL);
	default:
		return -ENXIO;
	}
}
EXPORT_SYMBOL_GPL(arizona_set_irq_wake);

static irqreturn_t arizona_boot_done(int irq, void *data)
{
	struct arizona *arizona = data;

	dev_dbg(arizona->dev, "Boot done\n");

	return IRQ_HANDLED;
}

static irqreturn_t arizona_ctrlif_err(int irq, void *data)
{
	struct arizona *arizona = data;

	/*
	 * For pretty much all potential sources a register cache sync
	 * won't help, we've just got a software bug somewhere.
	 */
	dev_err(arizona->dev, "Control interface error\n");

	return IRQ_HANDLED;
}

static irqreturn_t arizona_irq_thread(int irq, void *data)
{
	struct arizona *arizona = data;
	unsigned int val;
	int ret;

	ret = pm_runtime_get_sync(arizona->dev);
	if (ret < 0) {
		dev_err(arizona->dev, "Failed to resume device: %d\n", ret);
		return IRQ_NONE;
	}

	/* Always handle the AoD domain */
	handle_nested_irq(arizona->virq[0]);

	/*
	 * Check if one of the main interrupts is asserted and only
	 * check that domain if it is.
	 */
	ret = regmap_read(arizona->regmap, ARIZONA_IRQ_PIN_STATUS, &val);
	if (ret == 0 && val & ARIZONA_IRQ1_STS) {
		handle_nested_irq(arizona->virq[1]);
	} else if (ret != 0) {
		dev_err(arizona->dev, "Failed to read main IRQ status: %d\n",
			ret);
	}

	pm_runtime_put(arizona->dev);

	return IRQ_HANDLED;
}

static void arizona_irq_dummy(struct irq_data *data)
{
}

static struct irq_chip arizona_irq_chip = {
	.name			= "arizona",
	.irq_disable		= arizona_irq_dummy,
	.irq_enable		= arizona_irq_dummy,
};

int arizona_irq_init(struct arizona *arizona)
{
	int flags = IRQF_ONESHOT;
	int ret, i;
	struct regmap_irq_chip *aod, *irq;
	bool ctrlif_error = true;
	int irq_base;

	switch (arizona->type) {
#ifdef CONFIG_MFD_WM5102
	case WM5102:
		aod = &wm5102_aod;
		irq = &wm5102_irq;

		ctrlif_error = false;
		break;
#endif
#ifdef CONFIG_MFD_WM5110
	case WM5110:
		aod = &wm5110_aod;
		irq = &wm5110_irq;

		ctrlif_error = false;
		break;
#endif
	default:
		BUG_ON("Unknown Arizona class device" == NULL);
		return -EINVAL;
	}

	/* Disable all wake sources by default */
	regmap_write(arizona->regmap, ARIZONA_WAKE_CONTROL, 0);

	if (arizona->pdata.irq_active_high) {
		ret = regmap_update_bits(arizona->regmap, ARIZONA_IRQ_CTRL_1,
					 ARIZONA_IRQ_POL, 0);
		if (ret != 0) {
			dev_err(arizona->dev, "Couldn't set IRQ polarity: %d\n",
				ret);
			goto err;
		}

		flags |= IRQF_TRIGGER_HIGH;
	} else {
		flags |= IRQF_TRIGGER_LOW;
	}

        /* set virtual IRQs */
	if (arizona->pdata.irq_base > 0) {
	        arizona->virq[0] = arizona->pdata.irq_base;
	        arizona->virq[1] = arizona->pdata.irq_base + 1;
		irq_base = arizona->pdata.irq_base + 2;
	} else {
		dev_err(arizona->dev, "No irq_base specified\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(arizona->virq); i++) {
		irq_set_chip_and_handler(arizona->virq[i], &arizona_irq_chip,
					 handle_edge_irq);
		irq_set_nested_thread(arizona->virq[i], 1);

                /* ARM needs us to explicitly flag the IRQ as valid
                 * and will set them noprobe when we do so. */
#ifdef CONFIG_ARM
                set_irq_flags(arizona->virq[i], IRQF_VALID);
#else
                irq_set_noprobe(arizona->virq[i]);
#endif

	}

	ret = regmap_add_irq_chip(arizona->regmap,
				  arizona->virq[0],
				  IRQF_ONESHOT, irq_base, aod,
				  &arizona->aod_irq_chip);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to add AOD IRQs: %d\n", ret);
		goto err_domain;
	}

	ret = regmap_add_irq_chip(arizona->regmap,
				  arizona->virq[1],
				  IRQF_ONESHOT, irq_base + ARIZONA_NUM_IRQ, irq,
				  &arizona->irq_chip);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to add IRQs: %d\n", ret);
		goto err_aod;
	}

	/* Make sure the boot done IRQ is unmasked for resumes */
	i = arizona_map_irq(arizona, ARIZONA_IRQ_BOOT_DONE);
	ret = request_threaded_irq(i, NULL, arizona_boot_done, IRQF_ONESHOT,
				   "Boot done", arizona);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request boot done %d: %d\n",
			arizona->irq, ret);
		goto err_boot_done;
	}

	/* Handle control interface errors in the core */
	if (ctrlif_error) {
		i = arizona_map_irq(arizona, ARIZONA_IRQ_CTRLIF_ERR);
		ret = request_threaded_irq(i, NULL, arizona_ctrlif_err,
					   IRQF_ONESHOT,
					   "Control interface error", arizona);
		if (ret != 0) {
			dev_err(arizona->dev,
				"Failed to request CTRLIF_ERR %d: %d\n",
				arizona->irq, ret);
			goto err_ctrlif;
		}
	}

	ret = request_threaded_irq(arizona->irq, NULL, arizona_irq_thread,
				   flags, "arizona", arizona);

	if (ret != 0) {
		dev_err(arizona->dev, "Failed to request IRQ %d: %d\n",
			arizona->irq, ret);
		goto err_main_irq;
	}

	return 0;

err_main_irq:
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_CTRLIF_ERR), arizona);
err_ctrlif:
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_BOOT_DONE), arizona);
err_boot_done:
	regmap_del_irq_chip(arizona->virq[1],
			    arizona->irq_chip);
err_aod:
	regmap_del_irq_chip(arizona->virq[0],
			    arizona->aod_irq_chip);
err_domain:
err:
	return ret;
}

int arizona_irq_exit(struct arizona *arizona)
{
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_CTRLIF_ERR), arizona);
	free_irq(arizona_map_irq(arizona, ARIZONA_IRQ_BOOT_DONE), arizona);
	regmap_del_irq_chip(arizona->irq,
			    arizona->irq_chip);
	regmap_del_irq_chip(arizona->irq,
			    arizona->aod_irq_chip);
	free_irq(arizona->irq, arizona);

	return 0;
}

