/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
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

#ifndef __TAS2552_CORE_H__
#define __TAS2552_CORE_H__

#include <linux/ioctl.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/tlv.h>

/** Register map */

#define TAS2552_DEVICE_STATUS_REG               0x00
#define TAS2552_CFG1_REG                        0x01
#define TAS2552_CFG2_REG                        0x02
#define TAS2552_CFG3_REG                        0x03
#define TAS2552_DOUT_TRIST_MODE_REG             0x04
#define TAS2552_SERIAL_IFACE1_REG               0x05
#define TAS2552_SERIAL_IFACE2_REG               0x06
#define TAS2552_OUT_DATA_REG                    0x07
#define TAS2552_PLL_CTL1_REG                    0x08
#define TAS2552_PLL_CTL2_REG                    0x09
#define TAS2552_PLL_CTL3_REG                    0x0A
#define TAS2552_BATT_GUARD_INFL_REG             0x0B
#define TAS2552_BATT_GUARD_SLOPE_REG            0x0C
#define TAS2552_LIMITER_LVL_CTL_REG             0x0D
#define TAS2552_LIMITER_ATTACK_REG              0x0E
#define TAS2552_LIMITER_REL_REG                 0x0F
#define TAS2552_LIMITER_INTEG_COUNT_REG         0x10
#define TAS2552_PDM_CFG_REG                     0x11
#define TAS2552_PGA_GAIN_REG                    0x12
#define TAS2552_CLASSD_CTL_REG                  0x13
#define TAS2552_BOOST_AUTOPASS_REG              0x14
#define TAS2552_VER_NUMBER_REG                  0x16
#define TAS2552_INT_MASK_REG                    0x17
#define TAS2552_VBOOST_DATA_REG                 0x18
#define TAS2552_VBAT_DAT_REG                    0x19

/* Shutdown Bit Mask */
#define TAS2552_SHUTDOWN				(0x01 << 1)

/* Number of registers */
#define TAS2552_REG_LIST_SIZE  26

#define TAS2552_MAX_REGISTER_VAL 0x19

extern void tas2552_ext_amp_on(int on);

#endif  /* __TAS2552_CORE_H__ */
