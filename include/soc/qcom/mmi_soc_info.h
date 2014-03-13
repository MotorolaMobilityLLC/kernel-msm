/*
 * Copyright (C) 2015 Motorola Mobility, LLC
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#ifndef __MMI_SOC_INFO_H
#define __MMI_SOC_INFO_H

#ifdef CONFIG_MMI_SOC_INFO
void mmi_acpu_bin_set(int *speed, int *pvs, int *ver);
#else
static inline void mmi_acpu_bin_set(int *speed, int *pvs, int *ver) { }
#endif

#endif /* __MMI_SOC_INFO_H */
