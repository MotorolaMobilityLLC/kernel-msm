/* arch/arm/mach-msm/lge/kcal_qlut.c
 *
 * Interface to calibrate lcd color temperature by Qualcomm LUT.
 *
 * Copyright (C) 2012 LGE
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>

extern int lge_set_qlut(void);
extern int g_kcal_r;
extern int g_kcal_g;
extern int g_kcal_b;

static int set_qlut_kcal_values(int kcal_r, int kcal_g, int kcal_b)
{
	g_kcal_r = kcal_r;
	g_kcal_g = kcal_g;
	g_kcal_b = kcal_b;
	return 0;
}

static int refresh_qlut_kcal_display(void)
{
	return lge_set_qlut();
}


int set_kcal_values(int kcal_r, int kcal_g, int kcal_b)
{
	return set_qlut_kcal_values(kcal_r, kcal_g, kcal_b);
}
EXPORT_SYMBOL(set_kcal_values);

int get_kcal_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	*kcal_r = g_kcal_r;
	*kcal_g = g_kcal_g;
	*kcal_b = g_kcal_b;
	return 0;
}
EXPORT_SYMBOL(get_kcal_values);

int refresh_kcal_display(void)
{
	return refresh_qlut_kcal_display();
}
EXPORT_SYMBOL(refresh_kcal_display);
