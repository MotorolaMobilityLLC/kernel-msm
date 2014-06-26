/* gpio-langwell.c Moorestown platform Langwell chip GPIO driver
 * Copyright (c) 2008 - 2013,  Intel Corporation.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Supports:
 * Moorestown platform Langwell chip.
 * Medfield platform Penwell chip.
 * Whitney point.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/lnw_gpio.h>
#include <linux/pm_runtime.h>
#include <asm/intel-mid.h>
#include <xen/events.h>
#include <linux/irqdomain.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_flis.h>
#include "gpiodebug.h"

#define IRQ_TYPE_EDGE	(1 << 0)
#define IRQ_TYPE_LEVEL	(1 << 1)

/*
 * Langwell chip has 64 pins and thus there are 2 32bit registers to control
 * each feature, while Penwell chip has 96 pins for each block, and need 3 32bit
 * registers to control them, so we only define the order here instead of a
 * structure, to get a bit offset for a pin (use GPDR as an example):
 *
 * nreg = ngpio / 32;
 * reg = offset / 32;
 * bit = offset % 32;
 * reg_addr = reg_base + GPDR * nreg * 4 + reg * 4;
 *
 * so the bit of reg_addr is to control pin offset's GPDR feature
*/

enum GPIO_REG {
	GPLR = 0,	/* pin level read-only */
	GPDR,		/* pin direction */
	GPSR,		/* pin set */
	GPCR,		/* pin clear */
	GRER,		/* rising edge detect */
	GFER,		/* falling edge detect */
	GEDR,		/* edge detect result */
	GAFR,		/* alt function */
	GFBR = 9,	/* glitch filter bypas */
	GPIT,		/* interrupt type */
	GPIP = GFER,	/* level interrupt polarity */
	GPIM = GRER,	/* level interrupt mask */

	/* the following registers only exist on MRFLD */
	GFBR_TNG = 6,
	GIMR,		/* interrupt mask */
	GISR,		/* interrupt source */
	GITR = 32,	/* interrupt type */
	GLPR = 33,	/* level-input polarity */
};

enum GPIO_CONTROLLERS {
	LINCROFT_GPIO,
	PENWELL_GPIO_AON,
	PENWELL_GPIO_CORE,
	CLOVERVIEW_GPIO_AON,
	CLOVERVIEW_GPIO_CORE,
	TANGIER_GPIO,
};

/* langwell gpio driver data */
struct lnw_gpio_ddata_t {
	u16 ngpio;		/* number of gpio pins */
	u32 gplr_offset;	/* offset of first GPLR register from base */
	u32 (*get_flis_offset)(int gpio);
	u32 chip_irq_type;	/* chip interrupt type */
};

struct gpio_flis_pair {
	int gpio;	/* gpio number */
	int offset;	/* register offset from FLIS base */
};

/*
 * The following mapping table lists the pin and flis offset pair
 * of some key gpio pins, the offset of other gpios can be calculated
 * from the table.
 */
static struct gpio_flis_pair gpio_flis_tng_mapping_table[] = {
	{ 0,	0x2900 },
	{ 12,	0x2544 },
	{ 14,	0x0958 },
	{ 16,	0x2D18 },
	{ 17,	0x1D10 },
	{ 19,	0x1D00 },
	{ 23,	0x1D18 },
	{ 31,	-EINVAL }, /* No GPIO 31 in pin list */
	{ 32,	0x1508 },
	{ 44,	0x3500 },
	{ 64,	0x2534 },
	{ 68,	0x2D1C },
	{ 70,	0x1500 },
	{ 72,	0x3D00 },
	{ 77,	0x0D00 },
	{ 97,	0x0954 },
	{ 98,	-EINVAL }, /* No GPIO 98-101 in pin list */
	{ 102,	0x1910 },
	{ 120,	0x1900 },
	{ 124,	0x2100 },
	{ 136,	-EINVAL }, /* No GPIO 136 in pin list */
	{ 137,	0x2D00 },
	{ 143,	-EINVAL }, /* No GPIO 143-153 in pin list */
	{ 154,	0x092C },
	{ 164,	0x3900 },
	{ 177,	0x2500 },
	{ 190,	0x2D50 },
};

static struct gpio_flis_pair gpio_flis_ann_mapping_table[] = {
	{ 0,	0x2900 },
	{ 12,	0x2154 },
	{ 14,	0x2540 },
	{ 16,	0x2930 },
	{ 17,	0x1D18 },
	{ 19,	0x1D08 },
	{ 23,	0x1D20 },
	{ 31,	0x111C },
	{ 32,	0x1508 },
	{ 44,	0x3500 },
	{ 64,	0x312C },
	{ 68,	0x2934 },
	{ 70,	0x1500 },
	{ 72,	0x3D00 },
	{ 77,	0x0D00 },
	{ 87,   0x0D2C },
	{ 88,   0x0D28 },
	{ 89,   0x0D30 },
	{ 97,	0x2130 },
	{ 98,	0x2D18 },
	{ 99,	-EINVAL }, /* No GPIO 99-100 in pin list */
	{ 101,	0x0500 },
	{ 102,	0x1910 },
	{ 120,	0x1900 },
	{ 124,	0x2100 },
	{ 136,	0x0504 },
	{ 137,  0x2D00 },
	{ 143,  0x0508 },
	{ 154,	0x2134 },
	{ 162,	0x2548 },
	{ 164,	0x3D14 },
	{ 176,	0x2500 },

};

/*
 * In new version of FW for Merrifield, I2C FLIS register can not
 * be written directly but go though a IPC way which is sleepable,
 * so we shouldn't use spin_lock_irq to protect the access when
 * is_merr_i2c_flis() return true.
 */
static inline bool is_merr_i2c_flis(u32 offset)
{
	return ((offset >= I2C_FLIS_START)
		&& (offset <= I2C_FLIS_END));
}

static u32 get_flis_offset_by_gpio(int gpio)
{
	int i;
	int start;
	u32 offset = -EINVAL, size;
	struct gpio_flis_pair *gpio_flis_map;

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) {
		size = ARRAY_SIZE(gpio_flis_tng_mapping_table);
		gpio_flis_map = gpio_flis_tng_mapping_table;
	} else if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE) {
		size = ARRAY_SIZE(gpio_flis_ann_mapping_table);
		gpio_flis_map = gpio_flis_ann_mapping_table;
	} else
		return -EINVAL;

	for (i = 0; i < size - 1; i++) {
		if (gpio >= gpio_flis_map[i].gpio
			&& gpio < gpio_flis_map[i + 1].gpio)
			break;
	}

	start = gpio_flis_map[i].gpio;

	if (gpio_flis_map[i].offset != -EINVAL)
		offset = gpio_flis_map[i].offset + (gpio - start) * 4;

	return offset;
}

static struct lnw_gpio_ddata_t lnw_gpio_ddata[] = {
	[LINCROFT_GPIO] = {
		.ngpio = 64,
	},
	[PENWELL_GPIO_AON] = {
		.ngpio = 96,
		.chip_irq_type = IRQ_TYPE_EDGE,
	},
	[PENWELL_GPIO_CORE] = {
		.ngpio = 96,
		.chip_irq_type = IRQ_TYPE_EDGE,
	},
	[CLOVERVIEW_GPIO_AON] = {
		.ngpio = 96,
		.chip_irq_type = IRQ_TYPE_EDGE | IRQ_TYPE_LEVEL,
	},
	[CLOVERVIEW_GPIO_CORE] = {
		.ngpio = 96,
		.chip_irq_type = IRQ_TYPE_EDGE,
	},
	[TANGIER_GPIO] = {
		.ngpio = 192,
		.gplr_offset = 4,
		.get_flis_offset = get_flis_offset_by_gpio,
		.chip_irq_type = IRQ_TYPE_EDGE | IRQ_TYPE_LEVEL,
	},
};

struct lnw_gpio {
	struct gpio_chip	chip;
	void			*reg_base;
	void			*reg_gplr;
	spinlock_t		lock;
	struct pci_dev		*pdev;
	struct irq_domain	*domain;
	u32			(*get_flis_offset)(int gpio);
	u32			chip_irq_type;
	int			type;
	struct gpio_debug	*debug;
};

#define to_lnw_priv(chip)	container_of(chip, struct lnw_gpio, chip)

static void __iomem *gpio_reg(struct gpio_chip *chip, unsigned offset,
			enum GPIO_REG reg_type)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	unsigned nreg = chip->ngpio / 32;
	u8 reg = offset / 32;
	void __iomem *ptr;
	void *base;

	/**
	 * On TNG B0, GITR[0]'s address is 0xFF008300, while GPLR[0]'s address
	 * is 0xFF008004. To count GITR[0]'s address, it's easier to count
	 * from 0xFF008000. So for GITR,GLPR... we switch the base to reg_base.
	 * This does not affect PNW/CLV, since the reg_gplr is the reg_base,
	 * while on TNG, the reg_gplr has an offset of 0x4.
	 */
	base = reg_type < GITR ? lnw->reg_gplr : lnw->reg_base;
	ptr = (void __iomem *)(base + reg_type * nreg * 4 + reg * 4);
	return ptr;
}

void lnw_gpio_set_alt(int gpio, int alt)
{
	struct lnw_gpio *lnw;
	u32 __iomem *mem;
	int reg;
	int bit;
	u32 offset;
	u32 value;
	unsigned long flags;

	/* use this trick to get memio */
	lnw = irq_get_chip_data(gpio_to_irq(gpio));
	if (!lnw) {
		pr_err("langwell_gpio: can not find pin %d\n", gpio);
		return;
	}
	if (gpio < lnw->chip.base || gpio >= lnw->chip.base + lnw->chip.ngpio) {
		dev_err(lnw->chip.dev, "langwell_gpio: wrong pin %d to config alt\n", gpio);
		return;
	}
#if 0
	if (lnw->irq_base + gpio - lnw->chip.base != gpio_to_irq(gpio)) {
		dev_err(lnw->chip.dev, "langwell_gpio: wrong chip data for pin %d\n", gpio);
		return;
	}
#endif
	gpio -= lnw->chip.base;

	if (lnw->type != TANGIER_GPIO) {
		reg = gpio / 16;
		bit = gpio % 16;

		mem = gpio_reg(&lnw->chip, 0, GAFR);
		spin_lock_irqsave(&lnw->lock, flags);
		value = readl(mem + reg);
		value &= ~(3 << (bit * 2));
		value |= (alt & 3) << (bit * 2);
		writel(value, mem + reg);
		spin_unlock_irqrestore(&lnw->lock, flags);
		dev_dbg(lnw->chip.dev, "ALT: writing 0x%x to %p\n",
			value, mem + reg);
	} else {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return;

		if (!is_merr_i2c_flis(offset))
			spin_lock_irqsave(&lnw->lock, flags);

		value = get_flis_value(offset);
		value &= ~7;
		value |= (alt & 7);
		set_flis_value(value, offset);

		if (!is_merr_i2c_flis(offset))
			spin_unlock_irqrestore(&lnw->lock, flags);
	}
}
EXPORT_SYMBOL_GPL(lnw_gpio_set_alt);

int gpio_get_alt(int gpio)
{
	struct lnw_gpio *lnw;
	u32 __iomem *mem;
	int reg;
	int bit;
	u32 value;
	u32 offset;

	 /* use this trick to get memio */
	lnw = irq_get_chip_data(gpio_to_irq(gpio));
	if (!lnw) {
		pr_err("langwell_gpio: can not find pin %d\n", gpio);
		return -1;
	}
	if (gpio < lnw->chip.base || gpio >= lnw->chip.base + lnw->chip.ngpio) {
		dev_err(lnw->chip.dev,
			"langwell_gpio: wrong pin %d to config alt\n", gpio);
		return -1;
	}
#if 0
	if (lnw->irq_base + gpio - lnw->chip.base != gpio_to_irq(gpio)) {
		dev_err(lnw->chip.dev,
			"langwell_gpio: wrong chip data for pin %d\n", gpio);
		return -1;
	}
#endif
	gpio -= lnw->chip.base;

	if (lnw->type != TANGIER_GPIO) {
		reg = gpio / 16;
		bit = gpio % 16;

		mem = gpio_reg(&lnw->chip, 0, GAFR);
		value = readl(mem + reg);
		value &= (3 << (bit * 2));
		value >>= (bit * 2);
	} else {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return -EINVAL;

		value = get_flis_value(offset) & 7;
	}

	return value;
}
EXPORT_SYMBOL_GPL(gpio_get_alt);

static int lnw_gpio_set_debounce(struct gpio_chip *chip, unsigned offset,
				 unsigned debounce)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	void __iomem *gfbr;
	unsigned long flags;
	u32 value;
	enum GPIO_REG reg_type;

	reg_type = (lnw->type == TANGIER_GPIO) ? GFBR_TNG : GFBR;
	gfbr = gpio_reg(chip, offset, reg_type);

	if (lnw->pdev)
		pm_runtime_get(&lnw->pdev->dev);

	spin_lock_irqsave(&lnw->lock, flags);
	value = readl(gfbr);
	if (debounce) {
		/* debounce bypass disable */
		value &= ~BIT(offset % 32);
	} else {
		/* debounce bypass enable */
		value |= BIT(offset % 32);
	}
	writel(value, gfbr);
	spin_unlock_irqrestore(&lnw->lock, flags);

	if (lnw->pdev)
		pm_runtime_put(&lnw->pdev->dev);

	return 0;
}

static void __iomem *gpio_reg_2bit(struct gpio_chip *chip, unsigned offset,
				   enum GPIO_REG reg_type)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	unsigned nreg = chip->ngpio / 32;
	u8 reg = offset / 16;
	void __iomem *ptr;

	ptr = (void __iomem *)(lnw->reg_base + reg_type * nreg * 4 + reg * 4);
	return ptr;
}

static int lnw_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	u32 value;
	void __iomem *gafr;
	int shift, af;

	if (lnw->type > CLOVERVIEW_GPIO_CORE)
		return 0;

	gafr = gpio_reg_2bit(chip, offset, GAFR);
	value = readl(gafr);
	shift = (offset % 16) << 1;
	af = (value >> shift) & 3;

	if (af) {
		value &= ~(3 << shift);
		writel(value, gafr);
	}
	return 0;
}

static int lnw_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	void __iomem *gplr = gpio_reg(chip, offset, GPLR);

	return readl(gplr) & BIT(offset % 32);
}

static void lnw_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	void __iomem *gpsr, *gpcr;

	if (value) {
		gpsr = gpio_reg(chip, offset, GPSR);
		writel(BIT(offset % 32), gpsr);
	} else {
		gpcr = gpio_reg(chip, offset, GPCR);
		writel(BIT(offset % 32), gpcr);
	}
}

static int lnw_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	void __iomem *gpdr = gpio_reg(chip, offset, GPDR);
	u32 value;
	unsigned long flags;

	if (lnw->pdev)
		pm_runtime_get(&lnw->pdev->dev);

	spin_lock_irqsave(&lnw->lock, flags);
	value = readl(gpdr);
	value &= ~BIT(offset % 32);
	writel(value, gpdr);
	spin_unlock_irqrestore(&lnw->lock, flags);

	if (lnw->pdev)
		pm_runtime_put(&lnw->pdev->dev);

	return 0;
}

static int lnw_gpio_direction_output(struct gpio_chip *chip,
			unsigned offset, int value)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	void __iomem *gpdr = gpio_reg(chip, offset, GPDR);
	unsigned long flags;

	lnw_gpio_set(chip, offset, value);

	if (lnw->pdev)
		pm_runtime_get(&lnw->pdev->dev);

	spin_lock_irqsave(&lnw->lock, flags);
	value = readl(gpdr);
	value |= BIT(offset % 32);
	writel(value, gpdr);
	spin_unlock_irqrestore(&lnw->lock, flags);

	if (lnw->pdev)
		pm_runtime_put(&lnw->pdev->dev);

	return 0;
}

static int lnw_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	struct lnw_gpio *lnw = to_lnw_priv(chip);
	return irq_create_mapping(lnw->domain, offset);
}

static int lnw_irq_type(struct irq_data *d, unsigned type)
{
	struct lnw_gpio *lnw = irq_data_get_irq_chip_data(d);
	u32 gpio = irqd_to_hwirq(d);
	unsigned long flags;
	u32 value;
	int ret = 0;
	void __iomem *grer = gpio_reg(&lnw->chip, gpio, GRER);
	void __iomem *gfer = gpio_reg(&lnw->chip, gpio, GFER);
	void __iomem *gpit, *gpip;

	if (gpio >= lnw->chip.ngpio)
		return -EINVAL;

	if (lnw->pdev)
		pm_runtime_get(&lnw->pdev->dev);

	/* Chip that supports level interrupt has extra GPIT registers */
	if (lnw->chip_irq_type & IRQ_TYPE_LEVEL) {
		switch (lnw->type) {
		case CLOVERVIEW_GPIO_AON:
			gpit = gpio_reg(&lnw->chip, gpio, GPIT);
			gpip = gpio_reg(&lnw->chip, gpio, GPIP);
			break;
		case TANGIER_GPIO:
			gpit = gpio_reg(&lnw->chip, gpio, GITR);
			gpip = gpio_reg(&lnw->chip, gpio, GLPR);
			break;
		default:
			ret = -EINVAL;
			goto out;
		}

		spin_lock_irqsave(&lnw->lock, flags);
		if (type & IRQ_TYPE_LEVEL_MASK) {
			/* To prevent glitches from triggering an unintended
			 * level interrupt, configure GLPR register first
			 * and then configure GITR.
			 */
			if (type & IRQ_TYPE_LEVEL_LOW)
				value = readl(gpip) | BIT(gpio % 32);
			else
				value = readl(gpip) & (~BIT(gpio % 32));
			writel(value, gpip);

			value = readl(gpit) | BIT(gpio % 32);
			writel(value, gpit);

			__irq_set_handler_locked(d->irq, handle_level_irq);
		} else if (type & IRQ_TYPE_EDGE_BOTH) {
			value = readl(gpit) & (~BIT(gpio % 32));
			writel(value, gpit);

			if (type & IRQ_TYPE_EDGE_RISING)
				value = readl(grer) | BIT(gpio % 32);
			else
				value = readl(grer) & (~BIT(gpio % 32));
			writel(value, grer);

			if (type & IRQ_TYPE_EDGE_FALLING)
				value = readl(gfer) | BIT(gpio % 32);
			else
				value = readl(gfer) & (~BIT(gpio % 32));
			writel(value, gfer);

			__irq_set_handler_locked(d->irq, handle_edge_irq);
		}
		spin_unlock_irqrestore(&lnw->lock, flags);
	} else {
		if (type & IRQ_TYPE_LEVEL_MASK) {
			ret = -EINVAL;
		} else if (type & IRQ_TYPE_EDGE_BOTH) {
			spin_lock_irqsave(&lnw->lock, flags);

			if (type & IRQ_TYPE_EDGE_RISING)
				value = readl(grer) | BIT(gpio % 32);
			else
				value = readl(grer) & (~BIT(gpio % 32));
			writel(value, grer);

			if (type & IRQ_TYPE_EDGE_FALLING)
				value = readl(gfer) | BIT(gpio % 32);
			else
				value = readl(gfer) & (~BIT(gpio % 32));
			writel(value, gfer);

			spin_unlock_irqrestore(&lnw->lock, flags);
		}
	}

out:
	if (lnw->pdev)
		pm_runtime_put(&lnw->pdev->dev);

	return ret;
}

static int lnw_set_maskunmask(struct irq_data *d, enum GPIO_REG reg_type,
				unsigned unmask)
{
	struct lnw_gpio *lnw = irq_data_get_irq_chip_data(d);
	u32 gpio = irqd_to_hwirq(d);
	unsigned long flags;
	u32 value;
	void __iomem *gp_reg;

	gp_reg = gpio_reg(&lnw->chip, gpio, reg_type);

	spin_lock_irqsave(&lnw->lock, flags);

	if (unmask) {
		/* enable interrupt from GPIO input pin */
		value = readl(gp_reg) | BIT(gpio % 32);
	} else {
		/* disable interrupt from GPIO input pin */
		value = readl(gp_reg) & (~BIT(gpio % 32));
	}

	writel(value, gp_reg);

	spin_unlock_irqrestore(&lnw->lock, flags);

	return 0;
}

static void lnw_irq_unmask(struct irq_data *d)
{
	struct lnw_gpio *lnw = irq_data_get_irq_chip_data(d);
	u32 gpio = irqd_to_hwirq(d);
	void __iomem *gpit;
	void __iomem *gisr;

	if (gpio >= lnw->chip.ngpio)
		return;

	switch (lnw->type) {
	case CLOVERVIEW_GPIO_AON:
		gpit = gpio_reg(&lnw->chip, gpio, GPIT);

		/* if it's level trigger, unmask GPIM */
		if (readl(gpit) & BIT(gpio % 32))
			lnw_set_maskunmask(d, GPIM, 1);

		break;
	case TANGIER_GPIO:
		gpit = gpio_reg(&lnw->chip, gpio, GITR);
		gisr = gpio_reg(&lnw->chip, gpio, GISR);

		if (readl(gpit) & BIT(gpio % 32))
			writel(BIT(gpio % 32), gisr);

		lnw_set_maskunmask(d, GIMR, 1);
		break;
	default:
		break;
	}
}

static void lnw_irq_mask(struct irq_data *d)
{
	struct lnw_gpio *lnw = irq_data_get_irq_chip_data(d);
	u32 gpio = irqd_to_hwirq(d);
	void __iomem *gpit;

	if (gpio >= lnw->chip.ngpio)
		return;

	switch (lnw->type) {
	case CLOVERVIEW_GPIO_AON:
		gpit = gpio_reg(&lnw->chip, gpio, GPIT);

		/* if it's level trigger, mask GPIM */
		if (readl(gpit) & BIT(gpio % 32))
			lnw_set_maskunmask(d, GPIM, 0);

		break;
	case TANGIER_GPIO:
		lnw_set_maskunmask(d, GIMR, 0);
		break;
	default:
		break;
	}
}

static int lwn_irq_set_wake(struct irq_data *d, unsigned on)
{
	return 0;
}

static void lnw_irq_ack(struct irq_data *d)
{
}

static void lnw_irq_shutdown(struct irq_data *d)
{
	struct lnw_gpio *lnw = irq_data_get_irq_chip_data(d);
	u32 gpio = irqd_to_hwirq(d);
	unsigned long flags;
	u32 value;
	void __iomem *grer = gpio_reg(&lnw->chip, gpio, GRER);
	void __iomem *gfer = gpio_reg(&lnw->chip, gpio, GFER);

	spin_lock_irqsave(&lnw->lock, flags);
	value = readl(grer) & (~BIT(gpio % 32));
	writel(value, grer);
	value = readl(gfer) & (~BIT(gpio % 32));
	writel(value, gfer);
	spin_unlock_irqrestore(&lnw->lock, flags);
};


static struct irq_chip lnw_irqchip = {
	.name		= "LNW-GPIO",
	.flags		= IRQCHIP_SET_TYPE_MASKED,
	.irq_mask	= lnw_irq_mask,
	.irq_unmask	= lnw_irq_unmask,
	.irq_set_type	= lnw_irq_type,
	.irq_set_wake	= lwn_irq_set_wake,
	.irq_ack	= lnw_irq_ack,
	.irq_shutdown	= lnw_irq_shutdown,
};

static DEFINE_PCI_DEVICE_TABLE(lnw_gpio_ids) = {   /* pin number */
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x080f),
		.driver_data = LINCROFT_GPIO },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x081f),
		.driver_data = PENWELL_GPIO_AON },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x081a),
		.driver_data = PENWELL_GPIO_CORE },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x08eb),
		.driver_data = CLOVERVIEW_GPIO_AON },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x08f7),
		.driver_data = CLOVERVIEW_GPIO_CORE },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x1199),
		.driver_data = TANGIER_GPIO },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, lnw_gpio_ids);

static void lnw_irq_handler(unsigned irq, struct irq_desc *desc)
{
	struct irq_data *data = irq_desc_get_irq_data(desc);
	struct irq_chip *chip = irq_data_get_irq_chip(data);
	struct lnw_gpio *lnw;
	struct gpio_debug *debug;
	u32 base, gpio, mask;
	unsigned long pending;
	void __iomem *gp_reg;
	enum GPIO_REG reg_type;
	struct irq_desc *lnw_irq_desc;
	unsigned int lnw_irq;
#ifdef CONFIG_XEN
	lnw = xen_irq_get_handler_data(irq);
#else
	lnw = irq_data_get_irq_handler_data(data);
#endif /* CONFIG_XEN */

	debug = lnw->debug;

	reg_type = (lnw->type == TANGIER_GPIO) ? GISR : GEDR;

	/* check GPIO controller to check which pin triggered the interrupt */
	for (base = 0; base < lnw->chip.ngpio; base += 32) {
		gp_reg = gpio_reg(&lnw->chip, base, reg_type);
		while ((pending = (lnw->type != TANGIER_GPIO) ?
			readl(gp_reg) :
			(readl(gp_reg) &
			readl(gpio_reg(&lnw->chip, base, GIMR))))) {
			gpio = __ffs(pending);
			DEFINE_DEBUG_IRQ_CONUNT_INCREASE(lnw->chip.base +
				base + gpio);
			/* Mask irq if not requested in kernel */
			lnw_irq = irq_find_mapping(lnw->domain, base + gpio);
			lnw_irq_desc = irq_to_desc(lnw_irq);
			if (lnw_irq_desc && unlikely(!lnw_irq_desc->action)) {
				lnw_irq_mask(&lnw_irq_desc->irq_data);
				continue;
			}

			mask = BIT(gpio);
			/* Clear before handling so we can't lose an edge */
			writel(mask, gp_reg);
			generic_handle_irq(lnw_irq);
		}
	}

	chip->irq_eoi(data);
}

static char conf_reg_msg[] =
	"\nGPIO configuration register:\n"
	"\t[ 2: 0]\tpinmux\n"
	"\t[ 6: 4]\tpull strength\n"
	"\t[ 8: 8]\tpullup enable\n"
	"\t[ 9: 9]\tpulldown enable\n"
	"\t[10:10]\tslew A, B setting\n"
	"\t[12:12]\toverride input enable\n"
	"\t[13:13]\toverride input enable enable\n"
	"\t[14:14]\toverride output enable\n"
	"\t[15:15]\toverride output enable enable\n"
	"\t[16:16]\toverride input value\n"
	"\t[17:17]\tenable input data override\n"
	"\t[18:18]\toverride output value\n"
	"\t[19:19]\tenable output data override\n"
	"\t[21:21]\topen drain enable\n"
	"\t[22:22]\tenable OVR_IOSTBY_VAL\n"
	"\t[23:23]\tOVR_IOSTBY_VAL\n"
	"\t[24:24]\tSBY_OUTDATAOV_EN\n"
	"\t[25:25]\tSBY_INDATAOV_EN\n"
	"\t[26:26]\tSBY_OVOUTEN_EN\n"
	"\t[27:27]\tSBY_OVINEN_EN\n"
	"\t[29:28]\tstandby pullmode\n"
	"\t[30:30]\tstandby open drain mode\n";

static char *pinvalue[] = {"low", "high"};
static char *pindirection[] = {"in", "out"};
static char *irqtype[] = {"irq_none", "edge_rising", "edge_falling",
			"edge_both"};
static char *pinmux[] = {"mode0", "mode1", "mode2", "mode3", "mode4", "mode5",
			"mode6", "mode7"};
static char *pullmode[] = {"nopull", "pullup", "pulldown"};
static char *pullstrength[] = {"2k", "20k", "50k", "910ohms"};
static char *enable[] = {"disable", "enable"};
static char *override_direction[] = {"no-override", "override-enable",
			"override-disable"};
static char *override_value[] = {"no-override", "override-high",
			"override-low"};
static char *standby_trigger[] = {"no-override", "override-trigger",
			"override-notrigger"};
static char *standby_pupd_state[] = {"keep", "pulldown", "pullup", "nopull"};

static int gpio_get_pinvalue(struct gpio_control *control, void *private_data,
		unsigned gpio)
{
	struct lnw_gpio *lnw = private_data;
	u32 value;

	value = lnw_gpio_get(&lnw->chip, gpio);

	return value ? 1 : 0;
}

static int gpio_set_pinvalue(struct gpio_control *control, void *private_data,
		unsigned gpio, unsigned int num)
{
	struct lnw_gpio *lnw = private_data;

	lnw_gpio_set(&lnw->chip, gpio, num);
	return 0;
}

static int gpio_get_normal(struct gpio_control *control, void *private_data,
		unsigned gpio)
{
	struct lnw_gpio *lnw = private_data;
	u32 __iomem *mem;
	u32 value;

	mem = gpio_reg(&lnw->chip, gpio, control->reg);

	value = readl(mem);
	value &= BIT(gpio % 32);

	if (control->invert)
		return value ? 0 : 1;
	else
		return value ? 1 : 0;
}

static int gpio_set_normal(struct gpio_control *control, void *private_data,
		unsigned gpio, unsigned int num)
{
	struct lnw_gpio *lnw = private_data;
	u32 __iomem *mem;
	u32 value;
	unsigned long flags;

	mem = gpio_reg(&lnw->chip, gpio, control->reg);

	spin_lock_irqsave(&lnw->lock, flags);
	value = readl(mem);
	value &= ~BIT(gpio % 32);
	if (control->invert) {
		if (num)
			value &= ~BIT(gpio % 32);
		else
			value |= BIT(gpio % 32);
	} else {
		if (num)
			value |= BIT(gpio % 32);
		else
			value &= ~BIT(gpio % 32);
	}
	writel(value, mem);
	spin_unlock_irqrestore(&lnw->lock, flags);

	return 0;
}

static int gpio_get_irqtype(struct gpio_control *control, void *private_data,
		unsigned gpio)
{
	struct lnw_gpio *lnw = private_data;
	void __iomem *grer = gpio_reg(&lnw->chip, gpio, GRER);
	void __iomem *gfer = gpio_reg(&lnw->chip, gpio, GFER);
	u32 value;
	int num;

	value = readl(grer) & BIT(gpio % 32);
	num = value ? 1 : 0;
	value = readl(gfer) & BIT(gpio % 32);
	if (num)
		num = value ? 3 : 1;
	else
		num = value ? 2 : 0;

	return num;
}

static int flis_get_normal(struct gpio_control *control, void *private_data,
		unsigned gpio)
{
	struct lnw_gpio *lnw = private_data;
	u32 offset, value;
	int num;

	if (lnw->type == TANGIER_GPIO) {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return -1;

		value = get_flis_value(offset);
		num = (value >> control->shift) & control->mask;
		if (num < control->num)
			return num;
	}

	return -1;
}

static int flis_set_normal(struct gpio_control *control, void *private_data,
		unsigned gpio, unsigned int num)
{
	struct lnw_gpio *lnw = private_data;
	u32 shift = control->shift;
	u32 mask = control->mask;
	u32 offset, value;
	unsigned long flags;

	if (lnw->type == TANGIER_GPIO) {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return -1;

		if (!is_merr_i2c_flis(offset))
			spin_lock_irqsave(&lnw->lock, flags);
		value = get_flis_value(offset);
		value &= ~(mask << shift);
		value |= ((num & mask) << shift);
		set_flis_value(value, offset);
		if (!is_merr_i2c_flis(offset))
			spin_unlock_irqrestore(&lnw->lock, flags);
		return 0;
	}

	return -1;
}

static int flis_get_override(struct gpio_control *control, void *private_data,
		unsigned gpio)
{
	struct lnw_gpio *lnw = private_data;
	u32 offset, value;
	u32 val_bit, en_bit;
	int num;

	if (lnw->type == TANGIER_GPIO) {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return -1;

		val_bit = 1 << control->shift;
		en_bit = 1 << control->rshift;

		value = get_flis_value(offset);

		if (value & en_bit)
			if (value & val_bit)
				num = 1;
			else
				num = 2;
		else
			num = 0;

		return num;
	}

	return -1;
}

static int flis_set_override(struct gpio_control *control, void *private_data,
		unsigned gpio, unsigned int num)
{
	struct lnw_gpio *lnw = private_data;
	u32 offset, value;
	u32 val_bit, en_bit;
	unsigned long flags;

	if (lnw->type == TANGIER_GPIO) {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return -1;

		val_bit = 1 << control->shift;
		en_bit = 1 << control->rshift;

		if (!is_merr_i2c_flis(offset))
			spin_lock_irqsave(&lnw->lock, flags);
		value = get_flis_value(offset);
		switch (num) {
		case 0:
			value &= ~(en_bit | val_bit);
			break;
		case 1:
			value |= (en_bit | val_bit);
			break;
		case 2:
			value |= en_bit;
			value &= ~val_bit;
			break;
		default:
			break;
		}
		set_flis_value(value, offset);
		if (!is_merr_i2c_flis(offset))
			spin_unlock_irqrestore(&lnw->lock, flags);

		return 0;
	}

	return -1;
}

#define GPIO_VALUE_CONTROL(xtype, xinfo, xnum) \
{	.type = xtype, .pininfo = xinfo, .num = xnum, \
	.get = gpio_get_pinvalue, .set = gpio_set_pinvalue}
#define GPIO_NORMAL_CONTROL(xtype, xinfo, xnum, xreg, xinvert) \
{	.type = xtype, .pininfo = xinfo, .num = xnum, .reg = xreg, \
	.invert = xinvert, .get = gpio_get_normal, .set = gpio_set_normal}
#define GPIO_IRQTYPE_CONTROL(xtype, xinfo, xnum) \
{	.type = xtype, .pininfo = xinfo, .num = xnum, \
	.get = gpio_get_irqtype, .set = NULL}
#define FLIS_NORMAL_CONTROL(xtype, xinfo, xnum, xshift, xmask) \
{	.type = xtype, .pininfo = xinfo, .num = xnum, .shift = xshift, \
	.mask = xmask, .get = flis_get_normal, .set = flis_set_normal}
#define FLIS_OVERRIDE_CONTROL(xtype, xinfo, xnum, xshift, xrshift) \
{	.type = xtype, .pininfo = xinfo, .num = xnum, .shift = xshift, \
	.rshift = xrshift, .get = flis_get_override, .set = flis_set_override}

static struct gpio_control lnw_gpio_controls[] = {
GPIO_VALUE_CONTROL(TYPE_PIN_VALUE, pinvalue, 2),
GPIO_NORMAL_CONTROL(TYPE_DIRECTION, pindirection, 2, GPDR, 0),
GPIO_IRQTYPE_CONTROL(TYPE_IRQ_TYPE, irqtype, 4),
GPIO_NORMAL_CONTROL(TYPE_DEBOUNCE, enable, 2, GFBR_TNG, 1),
FLIS_NORMAL_CONTROL(TYPE_PINMUX, pinmux, 8, 0, 0x7),
FLIS_NORMAL_CONTROL(TYPE_PULLSTRENGTH, pullstrength, 4, 4, 0x7),
FLIS_NORMAL_CONTROL(TYPE_PULLMODE, pullmode, 3, 8, 0x3),
FLIS_NORMAL_CONTROL(TYPE_OPEN_DRAIN, enable, 2, 21, 0x1),
FLIS_OVERRIDE_CONTROL(TYPE_OVERRIDE_INDIR, override_direction, 3, 12, 13),
FLIS_OVERRIDE_CONTROL(TYPE_OVERRIDE_OUTDIR, override_direction, 3, 14, 15),
FLIS_OVERRIDE_CONTROL(TYPE_OVERRIDE_INVAL, override_value, 3, 16, 17),
FLIS_OVERRIDE_CONTROL(TYPE_OVERRIDE_OUTVAL, override_value, 3, 18, 19),
FLIS_OVERRIDE_CONTROL(TYPE_SBY_OVR_IO, standby_trigger, 3, 23, 22),
FLIS_OVERRIDE_CONTROL(TYPE_SBY_OVR_OUTVAL, override_value, 3, 18, 24),
FLIS_OVERRIDE_CONTROL(TYPE_SBY_OVR_INVAL, override_value, 3, 16, 25),
FLIS_OVERRIDE_CONTROL(TYPE_SBY_OVR_OUTDIR, override_direction, 3, 14, 26),
FLIS_OVERRIDE_CONTROL(TYPE_SBY_OVR_INDIR, override_direction, 3, 12, 27),
FLIS_NORMAL_CONTROL(TYPE_SBY_PUPD_STATE, standby_pupd_state, 4, 28, 0x3),
FLIS_NORMAL_CONTROL(TYPE_SBY_OD_DIS, enable, 2, 30, 0x1),
};

static unsigned int lnw_get_conf_reg(struct gpio_debug *debug, unsigned gpio)
{
	struct lnw_gpio *lnw = debug->private_data;
	u32 offset, value = 0;

	if (lnw->type == TANGIER_GPIO) {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return -EINVAL;

		value = get_flis_value(offset);
	}

	return value;
}

static void lnw_set_conf_reg(struct gpio_debug *debug, unsigned gpio,
		unsigned int value)
{
	struct lnw_gpio *lnw = debug->private_data;
	u32 offset;

	if (lnw->type == TANGIER_GPIO) {
		offset = lnw->get_flis_offset(gpio);
		if (WARN(offset == -EINVAL, "invalid pin %d\n", gpio))
			return;

		set_flis_value(value, offset);
	}

	return;
}

static char **lnw_get_avl_pininfo(struct gpio_debug *debug, unsigned gpio,
		unsigned int type, unsigned *num)
{
	struct gpio_control *control;

	control = find_gpio_control(lnw_gpio_controls,
			ARRAY_SIZE(lnw_gpio_controls), type);
	if (control == NULL)
		return NULL;

	*num = control->num;

	return control->pininfo;
}

static char *lnw_get_cul_pininfo(struct gpio_debug *debug, unsigned gpio,
		unsigned int type)
{
	struct lnw_gpio *lnw = debug->private_data;
	struct gpio_control *control;
	int num;

	control = find_gpio_control(lnw_gpio_controls,
			ARRAY_SIZE(lnw_gpio_controls), type);
	if (control == NULL)
		return NULL;

	num = control->get(control, lnw, gpio);
	if (num == -1)
		return NULL;

	return *(control->pininfo + num);
}

static void lnw_set_pininfo(struct gpio_debug *debug, unsigned gpio,
		unsigned int type, const char *info)
{
	struct lnw_gpio *lnw = debug->private_data;
	struct gpio_control *control;
	int num;

	control = find_gpio_control(lnw_gpio_controls,
			ARRAY_SIZE(lnw_gpio_controls), type);
	if (control == NULL)
		return;

	num = find_pininfo_num(control, info);
	if (num == -1)
		return;

	if (control->set)
		control->set(control, lnw, gpio, num);
}

static int lnw_get_register_msg(char **buf, unsigned long *size)
{
	*buf = conf_reg_msg;
	*size = strlen(conf_reg_msg);

	return 0;
}

static struct gpio_debug_ops lnw_gpio_debug_ops = {
	.get_conf_reg = lnw_get_conf_reg,
	.set_conf_reg = lnw_set_conf_reg,
	.get_avl_pininfo = lnw_get_avl_pininfo,
	.get_cul_pininfo = lnw_get_cul_pininfo,
	.set_pininfo = lnw_set_pininfo,
	.get_register_msg = lnw_get_register_msg,
};

static void lnw_irq_init_hw(struct lnw_gpio *lnw)
{
	void __iomem *reg;
	unsigned base;

	for (base = 0; base < lnw->chip.ngpio; base += 32) {
		/* Clear the rising-edge detect register */
		reg = gpio_reg(&lnw->chip, base, GRER);
		writel(0, reg);
		/* Clear the falling-edge detect register */
		reg = gpio_reg(&lnw->chip, base, GFER);
		writel(0, reg);
		/* Clear the edge detect status register */
		reg = gpio_reg(&lnw->chip, base, GEDR);
		writel(~0, reg);
	}
}

static int lnw_gpio_irq_map(struct irq_domain *d, unsigned int virq,
			    irq_hw_number_t hw)
{
	struct lnw_gpio *lnw = d->host_data;

	irq_set_chip_and_handler_name(virq, &lnw_irqchip, handle_simple_irq,
				      "demux");
	irq_set_chip_data(virq, lnw);
	irq_set_irq_type(virq, IRQ_TYPE_NONE);

	return 0;
}

static const struct irq_domain_ops lnw_gpio_irq_ops = {
	.map = lnw_gpio_irq_map,
	.xlate = irq_domain_xlate_twocell,
};

static int lnw_gpio_runtime_resume(struct device *dev)
{
	return 0;
}

static int lnw_gpio_runtime_suspend(struct device *dev)
{
	return 0;
}

static int lnw_gpio_runtime_idle(struct device *dev)
{
	int err = pm_schedule_suspend(dev, 500);

	if (!err)
		return 0;

	return -EBUSY;
}

static const struct dev_pm_ops lnw_gpio_pm_ops = {
	SET_RUNTIME_PM_OPS(lnw_gpio_runtime_suspend,
			   lnw_gpio_runtime_resume,
			   lnw_gpio_runtime_idle)
};

static int lnw_gpio_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	void *base;
	resource_size_t start, len;
	struct lnw_gpio *lnw;
	struct gpio_debug *debug;
	u32 gpio_base;
	u32 irq_base;
	int retval;
	struct lnw_gpio_ddata_t *ddata;
	int pid;

	pid = id->driver_data;
	ddata = &lnw_gpio_ddata[pid];

	retval = pci_enable_device(pdev);
	if (retval)
		return retval;

	retval = pci_request_regions(pdev, "langwell_gpio");
	if (retval) {
		dev_err(&pdev->dev, "error requesting resources\n");
		goto err_pci_req_region;
	}
	/* get the gpio_base from bar1 */
	start = pci_resource_start(pdev, 1);
	len = pci_resource_len(pdev, 1);
	base = ioremap_nocache(start, len);
	if (!base) {
		dev_err(&pdev->dev, "error mapping bar1\n");
		retval = -EFAULT;
		goto err_ioremap;
	}
	irq_base = *(u32 *)base;
	gpio_base = *((u32 *)base + 1);
	/* release the IO mapping, since we already get the info from bar1 */
	iounmap(base);
	/* get the register base from bar0 */
	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);
	base = devm_ioremap_nocache(&pdev->dev, start, len);
	if (!base) {
		dev_err(&pdev->dev, "error mapping bar0\n");
		retval = -EFAULT;
		goto err_ioremap;
	}

	lnw = devm_kzalloc(&pdev->dev, sizeof(*lnw), GFP_KERNEL);
	if (!lnw) {
		dev_err(&pdev->dev, "can't allocate langwell_gpio chip data\n");
		retval = -ENOMEM;
		goto err_ioremap;
	}

	lnw->type = pid;
	lnw->reg_base = base;
	lnw->reg_gplr = lnw->reg_base + ddata->gplr_offset;
	lnw->get_flis_offset = ddata->get_flis_offset;
	lnw->chip_irq_type = ddata->chip_irq_type;
	lnw->chip.label = dev_name(&pdev->dev);
	lnw->chip.request = lnw_gpio_request;
	lnw->chip.direction_input = lnw_gpio_direction_input;
	lnw->chip.direction_output = lnw_gpio_direction_output;
	lnw->chip.set_pinmux = lnw_gpio_set_alt;
	lnw->chip.get_pinmux = gpio_get_alt;
	lnw->chip.get = lnw_gpio_get;
	lnw->chip.set = lnw_gpio_set;
	lnw->chip.to_irq = lnw_gpio_to_irq;
	lnw->chip.base = gpio_base;
	lnw->chip.ngpio = ddata->ngpio;
	lnw->chip.can_sleep = 0;
	lnw->chip.set_debounce = lnw_gpio_set_debounce;
	lnw->chip.dev = &pdev->dev;
	lnw->pdev = pdev;
	spin_lock_init(&lnw->lock);
	lnw->domain = irq_domain_add_simple(pdev->dev.of_node,
					    lnw->chip.ngpio, irq_base,
					    &lnw_gpio_irq_ops, lnw);
	if (!lnw->domain) {
		retval = -ENOMEM;
		goto err_ioremap;
	}

	pci_set_drvdata(pdev, lnw);
	retval = gpiochip_add(&lnw->chip);
	if (retval) {
		dev_err(&pdev->dev, "langwell gpiochip_add error %d\n", retval);
		goto err_ioremap;
	}

	lnw_irq_init_hw(lnw);
#ifdef CONFIG_XEN
	xen_irq_set_handler_data(pdev->irq, lnw);
#else
	irq_set_handler_data(pdev->irq, lnw);
#endif /* CONFIG_XEN */
	irq_set_chained_handler(pdev->irq, lnw_irq_handler);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	/* add for gpiodebug */
	debug = gpio_debug_alloc();
	if (debug) {
		__set_bit(TYPE_OVERRIDE_OUTDIR, debug->typebit);
		__set_bit(TYPE_OVERRIDE_OUTVAL, debug->typebit);
		__set_bit(TYPE_OVERRIDE_INDIR, debug->typebit);
		__set_bit(TYPE_OVERRIDE_INVAL, debug->typebit);
		__set_bit(TYPE_SBY_OVR_IO, debug->typebit);
		__set_bit(TYPE_SBY_OVR_OUTVAL, debug->typebit);
		__set_bit(TYPE_SBY_OVR_INVAL, debug->typebit);
		__set_bit(TYPE_SBY_OVR_OUTDIR, debug->typebit);
		__set_bit(TYPE_SBY_OVR_INDIR, debug->typebit);
		__set_bit(TYPE_SBY_PUPD_STATE, debug->typebit);
		__set_bit(TYPE_SBY_OD_DIS, debug->typebit);

		debug->chip = &lnw->chip;
		debug->ops = &lnw_gpio_debug_ops;
		debug->private_data = lnw;
		lnw->debug = debug;

		retval = gpio_debug_register(debug);
		if (retval) {
			dev_err(&pdev->dev, "langwell gpio_debug_register failed %d\n",
				retval);
			gpio_debug_remove(debug);
		}
	}

	return 0;

err_ioremap:
	pci_release_regions(pdev);
err_pci_req_region:
	pci_disable_device(pdev);
	return retval;
}

static struct pci_driver lnw_gpio_driver = {
	.name		= "langwell_gpio",
	.id_table	= lnw_gpio_ids,
	.probe		= lnw_gpio_probe,
	.driver		= {
		.pm	= &lnw_gpio_pm_ops,
	},
};


static int wp_gpio_probe(struct platform_device *pdev)
{
	struct lnw_gpio *lnw;
	struct gpio_chip *gc;
	struct resource *rc;
	int retval = 0;

	rc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rc)
		return -EINVAL;

	lnw = kzalloc(sizeof(struct lnw_gpio), GFP_KERNEL);
	if (!lnw) {
		dev_err(&pdev->dev,
			"can't allocate whitneypoint_gpio chip data\n");
		return -ENOMEM;
	}
	lnw->reg_base = ioremap_nocache(rc->start, resource_size(rc));
	if (lnw->reg_base == NULL) {
		retval = -EINVAL;
		goto err_kmalloc;
	}
	spin_lock_init(&lnw->lock);
	gc = &lnw->chip;
	gc->label = dev_name(&pdev->dev);
	gc->owner = THIS_MODULE;
	gc->direction_input = lnw_gpio_direction_input;
	gc->direction_output = lnw_gpio_direction_output;
	gc->get = lnw_gpio_get;
	gc->set = lnw_gpio_set;
	gc->to_irq = NULL;
	gc->base = 0;
	gc->ngpio = 64;
	gc->can_sleep = 0;
	retval = gpiochip_add(gc);
	if (retval) {
		dev_err(&pdev->dev, "whitneypoint gpiochip_add error %d\n",
								retval);
		goto err_ioremap;
	}
	platform_set_drvdata(pdev, lnw);
	return 0;
err_ioremap:
	iounmap(lnw->reg_base);
err_kmalloc:
	kfree(lnw);
	return retval;
}

static int wp_gpio_remove(struct platform_device *pdev)
{
	struct lnw_gpio *lnw = platform_get_drvdata(pdev);
	int err;
	err = gpiochip_remove(&lnw->chip);
	if (err)
		dev_err(&pdev->dev, "failed to remove gpio_chip.\n");
	iounmap(lnw->reg_base);
	kfree(lnw);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver wp_gpio_driver = {
	.probe		= wp_gpio_probe,
	.remove		= wp_gpio_remove,
	.driver		= {
		.name	= "wp_gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init lnw_gpio_init(void)
{
	int ret;
	ret =  pci_register_driver(&lnw_gpio_driver);
	if (ret < 0)
		return ret;
	ret = platform_driver_register(&wp_gpio_driver);
	if (ret < 0)
		pci_unregister_driver(&lnw_gpio_driver);
	return ret;
}

fs_initcall(lnw_gpio_init);
