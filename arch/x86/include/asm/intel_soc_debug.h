/*
 * intel_soc_debug.h
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef INTEL_SOC_DEBUG_H
#define INTEL_SOC_DEBUG_H

#define DEBUG_FEATURE_PTI      0x00000001
#define DEBUG_FEATURE_RTIT     0x00000002
#define DEBUG_FEATURE_LAKEMORE 0x00000004
#define DEBUG_FEATURE_SOCHAPS  0x00000008
#define DEBUG_FEATURE_USB3DFX  0x00000010

/* cpu_has_debug_feature checks whether the debug
 * feature passed as parameter is enabled.
 * The passed parameter shall be one (and only one)
 * of the above values (DEBUG_FEATURE_XXX).
 * The function returns 1 if the debug feature is
 * enabled and 0 otherwise.
 */

#ifdef CONFIG_INTEL_DEBUG_FEATURE
extern int cpu_has_debug_feature(u32 bit);
#else
static inline int cpu_has_debug_feature(u32 bit) { return 0; };
#endif

#endif
