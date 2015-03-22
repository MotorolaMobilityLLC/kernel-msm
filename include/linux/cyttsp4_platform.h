/* < DTS2013062605264 sunlibin 20130702 begin */
/* add cypress new driver ttda-02.03.01.476713 */
/*
 * cyttsp4_platform.h
 * Cypress TrueTouch(TM) Standard Product V4 Platform Module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_PLATFORM_H
#define _LINUX_CYTTSP4_PLATFORM_H

#include <linux/cyttsp4_core.h>

#if IS_ENABLED(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4)
extern struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data;

extern int cyttsp4_xres(struct cyttsp4_core_platform_data *pdata,
		struct device *dev);
extern int cyttsp4_init(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev);
extern int cyttsp4_power(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
extern int cyttsp4_irq_stat(struct cyttsp4_core_platform_data *pdata,
		struct device *dev);
#ifdef CYTTSP4_DETECT_HW
extern int cyttsp4_detect(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, cyttsp4_platform_read read);
#else
#define cyttsp4_detect		NULL
#endif

#else /* !CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4 */
static struct cyttsp4_loader_platform_data _cyttsp4_loader_platform_data;
#define cyttsp4_xres		NULL
#define cyttsp4_init		NULL
#define cyttsp4_power		NULL
#define cyttsp4_irq_stat	NULL
#define cyttsp4_detect		NULL
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4 */

#endif /* _LINUX_CYTTSP4_PLATFORM_H */

/* DTS2013062605264 sunlibin 20130702 end > */
