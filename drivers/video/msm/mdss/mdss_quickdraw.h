/* Copyright (c) 2014, Motorola Mobility, LLC.
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

#ifndef MDSS_QUICKDRAW_H
#define MDSS_QUICKDRAW_H

#include "mdss.h"

#ifdef CONFIG_MDSS_QUICKDRAW

void mdss_quickdraw_register(struct msm_fb_data_type *mfd);

#else

static inline void mdss_quickdraw_register(struct msm_fb_data_type *mfd) {};

#endif

#endif /* MDSS_QUICKDRAW_H */
