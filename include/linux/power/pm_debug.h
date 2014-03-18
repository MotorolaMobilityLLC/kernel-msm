/* Copyright (c) 2012-2014, Motorola Mobility Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_POWER_PM_DEBUG_H__
#define __LINUX_POWER_PM_DEBUG_H__

#ifdef CONFIG_PM_DEBUG_WAKEUP

/* cleanup gpio wakeup source buffer */
void wakeup_source_gpio_cleanup(void);

/* add a gpio wakeup irq into buffer */
int wakeup_source_gpio_add_irq(unsigned int irq);

/* cleanup gic wakeup source buffer */
void wakeup_source_gic_cleanup(void);

/* add a gic wakeup irq into buffer */
int wakeup_source_gic_add_irq(unsigned int irq);

/* cleanup PMIC wakeup source buffer */
void wakeup_source_pmic_cleanup(void);

/* add a PMIC wakeup irq into buffer */
int wakeup_source_pmic_add_irq(unsigned int irq);

#else /* !CONFIG_PM_DEBUG_WAKEUP */

/* cleanup gpio wakeup source buffer */
static inline void wakeup_source_gpio_cleanup(void) { }

/* add a gpio wakeup irq into buffer */
static inline int wakeup_source_gpio_add_irq(unsigned int irq)
{
	return 0;
}

/* cleanup gic wakeup source buffer */
static inline void wakeup_source_gic_cleanup(void) { }

/* add a gic wakeup irq into buffer */
static inline int wakeup_source_gic_add_irq(unsigned int irq)
{
	return 0;
}

/* cleanup pm8xxx wakeup source buffer */
static inline void wakeup_source_pmic_cleanup(void) { }

/* add a pm8xxx wakeup irq into buffer */
static inline int wakeup_source_pmic_add_irq(unsigned int irq)
{
	return 0;
}

#endif /* CONFIG_PM_DEBUG_WAKEUP */
#endif /* __LINUX_POWER_PM_DEBUG_H__ */
