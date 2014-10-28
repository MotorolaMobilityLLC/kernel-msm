/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DEVFREQ_DEVBW_H
#define _DEVFREQ_DEVBW_H

#include <linux/devfreq.h>

<<<<<<< HEAD:arch/arm/boot/dts/qcom/apqsamarium.dtsi
/include/ "msmsamarium.dtsi"
||||||| merged common ancestors
#include "msmsamarium.dtsi"
=======
#ifdef CONFIG_MSM_DEVFREQ_DEVBW
int devfreq_add_devbw(struct device *dev);
int devfreq_remove_devbw(struct device *dev);
#else
static inline int devfreq_add_devbw(struct device *dev)
{
	return 0;
}
static inline int devfreq_remove_devbw(struct device *dev)
{
	return 0;
}
#endif
>>>>>>> 07723b4952fbbd1b6f76c1219699ba0b30b189e1:include/soc/qcom/devfreq_devbw.h

#endif /* _DEVFREQ_DEVBW_H */
