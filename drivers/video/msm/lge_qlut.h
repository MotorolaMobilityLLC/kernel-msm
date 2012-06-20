/*
 *  Copyright (C) 2011-2012, LG Eletronics,Inc. All rights reserved.
 *      LGE LCD controller device driver
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

#if defined (CONFIG_LGE_QC_LCDC_LUT)
extern int lge_set_qlut(void);
extern unsigned int p_lg_qc_lcdc_lut[];

#define NUM_QLUT  256
#define R_MASK    0x00ff0000
#define G_MASK    0x000000ff
#define B_MASK    0x0000ff00
#define R_SHIFT   16
#define G_SHIFT   0
#define B_SHIFT   8

#if defined(CONFIG_LGE_KCAL_QLUT)
extern int g_kcal_r;
extern int g_kcal_g;
extern int g_kcal_b;

#define MAX_KCAL_V (NUM_QLUT-1)
#define lut2r(lut) ((lut & R_MASK) >> R_SHIFT)
#define lut2g(lut) ((lut & G_MASK) >> G_SHIFT)
#define lut2b(lut) ((lut & B_MASK) >> B_SHIFT)

#define scaled_by_kcal(rgb, kcal) \
		(((((unsigned int)(rgb) * (unsigned int)(kcal)) << 16) / \
		(unsigned int)MAX_KCAL_V) >> 16)
#endif /* CONFIG_LGE_KCAL_QLUT */
#endif /* CONFIG_LGE_QC_LCDC_LUT */
