/*
 * Copyright (C) 2010 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _PN548_HWADAPTER_H_
#define _PN548_HWADAPTER_H_

#include <linux/nfc/pn548.h>
#include <linux/of_gpio.h>

int pn548_get_hw_revision(void);
unsigned int pn548_get_irq_pin(struct pn548_dev *dev);
int pn548_gpio_to_irq(struct pn548_dev *dev);
void pn548_gpio_enable(struct pn548_dev *pn548_dev);
void pn548_shutdown_cb(struct pn548_dev *pn548_dev);

void pn548_parse_dt(struct device *dev, struct pn548_dev *pn548_dev);

#endif /* _PN548_HWADAPTER_H_ */
