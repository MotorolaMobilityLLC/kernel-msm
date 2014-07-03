/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Dale B. Stimson <dale.b.stimson@intel.com>
 *    Austin Hu <austin.hu@intel.com>
 */

#ifndef _PMU_TNG_H_
#define _PMU_TNG_H_

#include <linux/types.h>

/* Per TNG Punit HAS */

#define PUNIT_PORT              0x04

/*
 * Registers on msgbus port 4 (p-unit) for power/freq control.
 * Bits 7:0 of the PM0 (or just PM) registers are power control bits, whereas
 * bits 31:24 are the corresponding status bits.
*/

/*  Subsystem status of all North Cluster IPs (bits NC_PM_SSS_*) */
#define NC_PM_SSS               0x3f

/*
 * Bit masks for power islands, as present in PM0 or PM registers.
 * These reside as control bits in bits 7:0 of each register and
 * as status bits in bits 31:24 of each register.
 * Each power island has a 2-bit field which contains a value of TNG_SSC_*.
 */
#define SSC_TO_SSS_SHIFT        24

/* GFX_SS_PM0 island */
#define GFX_SS_PM0              0x30
#define GFX_SS_PM1              0x31

#define GFX_SLC_SSC             0x03
#define GFX_SDKCK_SSC           0x0c
#define GFX_RSCD_SSC            0x30
#define GFX_SLC_LDO_SSC         0xc0

#define GFX_SLC_SHIFT           0
#define GFX_SDKCK_SHIFT         2
#define GFX_RSCD_SHIFT          4
#define GFX_SLC_LDO_SHIFT       6

/* VED_SS_PMx power island */
#define VED_SS_PM0              0x32
#define VED_SS_PM1              0x33

#define VED_SSC                 0x03

/* VEC_SS_PMx power island */
#define VEC_SS_PM0              0x34
#define VEC_SS_PM1              0x35

#define VEC_SSC                 0x03

/* DSP_SS_PM power islands */
#define DSP_SS_PM               0x36

#define DPA_SSC                 0x03
#define DPB_SSC                 0x0c
#define DPC_SSC                 0x30

#define DPA_SHIFT               0
#define DPB_SHIFT               2
#define DPC_SHIFT               4

/* VSP_SS_PMx power islands */
#define VSP_SS_PM0              0x37
#define VSP_SS_PM1              0x38

#define VSP_SSC                 0x03

/* ISP_SS_PMx power islands */
#define ISP_SS_PM0              0x39
#define ISP_SS_PM1              0x3a

#define ISP_SSC                 0x03

/* MIO_SS_PM power island */
#define MIO_SS_PM               0x3b

#define MIO_SSC                 0x03

/* HDMIO_SS_PM power island */
#define HDMIO_SS_PM             0x3c

#define HDMIO_SSC               0x03

/*
 * Subsystem status bits for NC_PM_SSS.  Status of all North Cluster IPs.
 * These correspond to the above bits.
 */
#define NC_PM_SSS_GFX_SLC       0x00000003
#define NC_PM_SSS_GFX_SDKCK     0x0000000c
#define NC_PM_SSS_GFX_RSCD      0x00000030
#define NC_PM_SSS_VED           0x000000c0
#define NC_PM_SSS_VEC           0x00000300
#define NC_PM_SSS_DPA           0x00000c00
#define NC_PM_SSS_DPB           0x00003000
#define NC_PM_SSS_DPC           0x0000c000
#define NC_PM_SSS_VSP           0x00030000
#define NC_PM_SSS_ISP           0x000c0000
#define NC_PM_SSS_MIO           0x00300000
#define NC_PM_SSS_HDMIO         0x00c00000
#define NC_PM_SSS_GFX_SLC_LDO   0x03000000

/*
 * Frequency bits for *_PM1 registers above.
 */
#define IP_FREQ_VALID     0x80     /* Freq is valid bit */

#define IP_FREQ_SIZE         5     /* number of bits in freq fields */
#define IP_FREQ_MASK      0x1f     /* Bit mask for freq field */

/*  Positions of various frequency fields */
#define IP_FREQ_POS          0     /* Freq control [4:0] */
#define IP_FREQ_GUAR_POS     8     /* Freq guar   [12:8] */
#define IP_FREQ_STAT_POS    24     /* Freq status [28:24] */

#define IP_FREQ_100_00 0x1f        /* 0b11111 100.00 */
#define IP_FREQ_106_67 0x1d        /* 0b11101 106.67 */
#define IP_FREQ_133_30 0x17        /* 0b10111 133.30 */
#define IP_FREQ_160_00 0x13        /* 0b10011 160.00 */
#define IP_FREQ_177_78 0x11        /* 0b10001 177.78 */
#define IP_FREQ_200_00 0x0f        /* 0b01111 200.00 */
#define IP_FREQ_213_33 0x0e        /* 0b01110 213.33 */
#define IP_FREQ_266_67 0x0b        /* 0b01011 266.67 */
#define IP_FREQ_320_00 0x09        /* 0b01001 320.00 */
#define IP_FREQ_355_56 0x08        /* 0b01000 355.56 */
#define IP_FREQ_400_00 0x07        /* 0b00111 400.00 */
#define IP_FREQ_457_14 0x06        /* 0b00110 457.14 */
#define IP_FREQ_533_33 0x05        /* 0b00101 533.33 */
#define IP_FREQ_640_00 0x04        /* 0b00100 640.00 */
#define IP_FREQ_800_00 0x03        /* 0b00011 800.00 */
#define IP_FREQ_RESUME_SET 0x64

/*  Tangier power states for each island */
#define TNG_SSC_I0    (0b00)    /* i0 - power on, no clock or p[ower gating */
#define TNG_SSC_I1    (0b01)    /* i1 - clock gated */
#define TNG_SSC_I2    (0b01)    /* i2 - soft reset */
#define TNG_SSC_D3    (0b11)    /* d3 - power off, hw state not retained */

#define TNG_SSC_MASK  (0b11)    /* bit mask of all involved bits. */

/*  Masks for the completely on and off states for 4 islands */
#define TNG_COMPOSITE_I0    (0b00000000)
#define TNG_COMPOSITE_D3    (0b11111111)

#define DEBUG_PM_CMD 0
#if !defined DEBUG_PM_CMD
#define DEBUG_PM_CMD 1
#endif

int pmu_set_power_state_tng(u32 reg_pm0, u32 si_mask, u32 ns_mask);
#endif /* ifndef _PMU_TNG_H_ */
