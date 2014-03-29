/*
 * max77836-irq.c - MFD Interrupt controller support for MAX77836
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
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
 * This driver is based on max8997-irq.c
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/max77836.h>
#include <linux/mfd/max77836-private.h>

static const u8 max77836_mask_reg[] = {
	[MAX77836_IRQ_INT1] = MAX77836_REG_INTMASK1,
	[MAX77836_IRQ_INT2] = MAX77836_REG_INTMASK2,
	[MAX77836_IRQ_INT3] = MAX77836_REG_INTMASK3,
};

struct max77836_irq_data {
	int mask;
	enum max77836_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct max77836_irq_data max77836_irqs[] = {
	DECLARE_IRQ(MAX77836_IRQ_INT1_ADC,		MAX77836_IRQ_INT1, 1 << 0),
	DECLARE_IRQ(MAX77836_IRQ_INT1_ADCLOW,		MAX77836_IRQ_INT1, 1 << 1),
	DECLARE_IRQ(MAX77836_IRQ_INT1_ADCERR,		MAX77836_IRQ_INT1, 1 << 2),

	DECLARE_IRQ(MAX77836_IRQ_INT2_CHGTYP,		MAX77836_IRQ_INT2, 1 << 0),
	DECLARE_IRQ(MAX77836_IRQ_INT2_CHGDETREUN,	MAX77836_IRQ_INT2, 1 << 1),
	DECLARE_IRQ(MAX77836_IRQ_INT2_DCDTMR,		MAX77836_IRQ_INT2, 1 << 2),
	DECLARE_IRQ(MAX77836_IRQ_INT2_DBCHG,		MAX77836_IRQ_INT2, 1 << 3),
	DECLARE_IRQ(MAX77836_IRQ_INT2_VBVOLT,		MAX77836_IRQ_INT2, 1 << 4),

	DECLARE_IRQ(MAX77836_IRQ_INT3_EOC,		MAX77836_IRQ_INT3, 1 << 0),
	DECLARE_IRQ(MAX77836_IRQ_INT3_CGMBC,		MAX77836_IRQ_INT3, 1 << 1),
	DECLARE_IRQ(MAX77836_IRQ_INT3_OVP,		MAX77836_IRQ_INT3, 1 << 2),
	DECLARE_IRQ(MAX77836_IRQ_INT3_MBCCHGERR,	MAX77836_IRQ_INT3, 1 << 3),
};

static void max77836_irq_lock(struct irq_data *data)
{
	struct max77836_dev *max77836 = irq_get_chip_data(data->irq);

	mutex_lock(&max77836->irq_lock);
}

static void max77836_irq_sync_unlock(struct irq_data *data)
{
	struct max77836_dev *max77836 = irq_get_chip_data(data->irq);
	struct i2c_client *i2c = max77836->i2c;
	int i;

	for (i = 0; i < MAX77836_IRQ_REGS_NUM; i++) {
		u8 mask_reg = max77836_mask_reg[i];

		if (mask_reg == MAX77836_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;

		max77836->irq_masks_cache[i] = max77836->irq_masks_cur[i];

		max77836_write_reg(i2c, max77836_mask_reg[i],
				max77836->irq_masks_cur[i]);
	}

	mutex_unlock(&max77836->irq_lock);
}

static const inline struct max77836_irq_data *
irq_to_max77836_irq(struct max77836_dev *max77836, int irq)
{
	if (!max77836->pdata)
		return NULL;

	return &max77836_irqs[irq - max77836->pdata->irq_base];
}

static void max77836_irq_mask(struct irq_data *data)
{
	struct max77836_dev *max77836 = irq_get_chip_data(data->irq);
	const struct max77836_irq_data *irq_data =
	    irq_to_max77836_irq(max77836, data->irq);

	if (!irq_data)
		return;

	if (irq_data->group >= MAX77836_IRQ_REGS_NUM)
		return;

	max77836->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static void max77836_irq_unmask(struct irq_data *data)
{
	struct max77836_dev *max77836 = irq_get_chip_data(data->irq);
	const struct max77836_irq_data *irq_data =
	    irq_to_max77836_irq(max77836, data->irq);

	if (!irq_data)
		return;

	if (irq_data->group >= MAX77836_IRQ_REGS_NUM)
		return;

	max77836->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static struct irq_chip max77836_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= max77836_irq_lock,
	.irq_bus_sync_unlock	= max77836_irq_sync_unlock,
	.irq_mask		= max77836_irq_mask,
	.irq_unmask		= max77836_irq_unmask,
};

static irqreturn_t max77836_irq_thread(int irq, void *data)
{
	struct max77836_dev *max77836 = data;
	struct max77836_platform_data *pdata = max77836->pdata;
	u8 irq_reg[MAX77836_IRQ_REGS_NUM] = {0};
	u8 tmp_irq_reg[MAX77836_IRQ_REGS_NUM] = {};
	int gpio_val;
	int i;

	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
		gpio_get_value(pdata->irq_gpio));

	do {
		/* MAX77836 INT1 ~ INT3 */
		max77836_bulk_read(max77836->i2c, MAX77836_REG_INT1,
				MAX77836_IRQ_REGS_NUM,
				&tmp_irq_reg[MAX77836_IRQ_INT1]);

		/* Or temp irq register to irq register for if it retries */
		for (i = MAX77836_IRQ_INT1; i < MAX77836_IRQ_REGS_NUM; i++)
			irq_reg[i] |= tmp_irq_reg[i];

		pr_info("%s: interrupt[1:0x%02x, 2:0x%02x, 3:0x%02x]\n",
			__func__, irq_reg[MAX77836_IRQ_INT1],
			irq_reg[MAX77836_IRQ_INT2], irq_reg[MAX77836_IRQ_INT3]);

		gpio_val = gpio_get_value(pdata->irq_gpio);

		pr_debug("%s: irq gpio post-state(0x%02x)\n", __func__,
				gpio_val);

		if (gpio_get_value(pdata->irq_gpio) == 0)
			pr_warn("%s: irq_gpio is not High, retry read int reg\n",
					__func__);
	} while (gpio_val == 0);

	/* Apply masking */
	for (i = 0; i < MAX77836_IRQ_REGS_NUM; i++)
		irq_reg[i] &= max77836->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < MAX77836_IRQ_NUM; i++) {
		if (irq_reg[max77836_irqs[i].group] & max77836_irqs[i].mask)
			handle_nested_irq(pdata->irq_base + i);
	}

	return IRQ_HANDLED;
}

int max77836_irq_init(struct max77836_dev *max77836)
{
	struct max77836_platform_data *pdata = max77836->pdata;
	struct i2c_client *i2c = max77836->i2c;
	int i;
	int cur_irq;
	int ret;
	u8 val;

	if (!pdata->irq_gpio) {
		pr_warn("%s:%s No interrupt specified.\n", MFD_DEV_NAME,
				__func__);
		pdata->irq_base = 0;
		return 0;
	}

	if (!pdata->irq_base) {
		pr_err("%s:%s No interrupt base specified.\n", MFD_DEV_NAME,
				__func__);
		return 0;
	}

	mutex_init(&max77836->irq_lock);

	max77836->irq = gpio_to_irq(pdata->irq_gpio);
	ret = gpio_request(pdata->irq_gpio, "max77836_irq");
	if (ret) {
		pr_err("%s:%s failed requesting gpio(%d)\n", MFD_DEV_NAME,
			__func__, pdata->irq_gpio);
		return ret;
	}
	gpio_direction_input(pdata->irq_gpio);
	gpio_free(pdata->irq_gpio);

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	max77836_write_reg(max77836->i2c_pmic, MAX77836_PMIC_REG_INTSRC_MASK, 0xF7);
	max77836_read_reg(max77836->i2c_pmic, MAX77836_PMIC_REG_INTSRC_MASK, &val);
	pr_info("%s: INTSRC:0x%x\n", __func__, val);

	/* Mask individual interrupt sources */
	for (i = 0; i < MAX77836_IRQ_REGS_NUM; i++) {
		/* IRQ  0:MASK 1:NOT MASK */
		max77836->irq_masks_cur[i] = 0x00;
		max77836->irq_masks_cache[i] = 0x00;

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (max77836_mask_reg[i] == MAX77836_REG_INVALID)
			continue;

		max77836_write_reg(i2c, max77836_mask_reg[i],
				max77836->irq_masks_cur[i]);
	}

	/* Register with genirq */
	for (i = 0; i < MAX77836_IRQ_NUM; i++) {
		cur_irq = i + pdata->irq_base;
		ret = irq_set_chip_data(cur_irq, max77836);
		if (ret) {
			pr_err("%s: Failed to set_chip_data (irq:%d, ret:%d)\n",
				__func__, cur_irq, ret);
			continue;
		}

		irq_set_chip_and_handler(cur_irq, &max77836_irq_chip, handle_edge_irq);
		irq_set_nested_thread(cur_irq, true);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = request_threaded_irq(max77836->irq, NULL, max77836_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "max77836-irq", max77836);
	if (ret) {
		pr_err("%s:%s Failed to request IRQ(%d) ret(%d)\n",
				MFD_DEV_NAME, __func__, max77836->irq, ret);
		return ret;
	}

	return 0;
}

int max77836_irq_resume(struct max77836_dev *max77836)
{
	int ret = 0;

	if (max77836->irq && max77836->pdata && max77836->pdata->irq_base)
		ret = max77836_irq_thread(max77836->pdata->irq_base, max77836);

	pr_info("%s:%s irq_resume ret=%d", MFD_DEV_NAME, __func__, ret);

	return ret >= 0 ? 0 : ret;
}

void max77836_irq_exit(struct max77836_dev *max77836)
{
	if (max77836->irq)
		free_irq(max77836->irq, max77836);
}