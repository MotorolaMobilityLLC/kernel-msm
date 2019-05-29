/*  Himax Android Driver Sample Code for HX83112 chipset

    Copyright (C) 2018 Himax Corporation.

    This software is licensed under the terms of the GNU General Public
    License version 2, as published by the Free Software Foundation, and
    may be copied, distributed, and modified under those terms.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#ifdef HX_EN_DYNAMIC_NAME
#define hx83112_ic_b9_en   0x300B9000
#define hx83112_ic_eb_en   0x300EB000
#define hx83112_ic_osc_en  0x900880A8
#define hx83112_ic_osc_pw  0x900880E0
#define hx83112_e8_ic_fw   0x300E8006
#define hx83112_cb_ic_fw   0x300CB008
#endif
#endif
