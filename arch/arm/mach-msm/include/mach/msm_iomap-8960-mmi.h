/* Copyright (c) 2012, Motorola Mobility, Inc
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

#ifndef __ASM_ARCH_MSM_IOMAP_8960_MMI_H
#define __ASM_ARCH_MSM_IOMAP_8960_MMI_H

#include <mach/msm_iomap-8960.h>

/* Address of GSBI blocks */
#define MSM_GSBI1_PHYS  0x16000000
#define MSM_GSBI2_PHYS  0x16100000
#define MSM_GSBI3_PHYS  0x16200000
#define MSM_GSBI4_PHYS  0x16300000
#define MSM_GSBI5_PHYS  0x16400000
#define MSM_GSBI6_PHYS  0x16500000
#define MSM_GSBI7_PHYS  0x16600000
#define MSM_GSBI8_PHYS  0x1A000000
#define MSM_GSBI9_PHYS  0x1A100000
#define MSM_GSBI10_PHYS 0x1A200000
#define MSM_GSBI11_PHYS 0x12440000
#define MSM_GSBI12_PHYS 0x12480000

#define MSM_UART1DM_PHYS        (MSM_GSBI1_PHYS + 0x40000)
#define MSM_UART2DM_PHYS        (MSM_GSBI2_PHYS + 0x40000)
#define MSM_UART3DM_PHYS        (MSM_GSBI3_PHYS + 0x40000)
#define MSM_UART4DM_PHYS        (MSM_GSBI4_PHYS + 0x40000)
#define MSM_UART5DM_PHYS        (MSM_GSBI5_PHYS + 0x40000)
#define MSM_UART6DM_PHYS        (MSM_GSBI6_PHYS + 0x40000)
#define MSM_UART7DM_PHYS        (MSM_GSBI7_PHYS + 0x40000)
#define MSM_UART8DM_PHYS        (MSM_GSBI8_PHYS + 0x40000)
#define MSM_UART9DM_PHYS        (MSM_GSBI9_PHYS + 0x40000)
#define MSM_UART10DM_PHYS       (MSM_GSBI10_PHYS + 0x40000)
#define MSM_UART11DM_PHYS       (MSM_GSBI11_PHYS + 0x40000)
#define MSM_UART12DM_PHYS       (MSM_GSBI12_PHYS + 0x40000)

/* GSBI QUP devices */
#define MSM_GSBI1_QUP_PHYS      (MSM_GSBI1_PHYS + 0x80000)
#define MSM_GSBI2_QUP_PHYS      (MSM_GSBI2_PHYS + 0x80000)
#define MSM_GSBI3_QUP_PHYS      (MSM_GSBI3_PHYS + 0x80000)
#define MSM_GSBI4_QUP_PHYS      (MSM_GSBI4_PHYS + 0x80000)
#define MSM_GSBI5_QUP_PHYS      (MSM_GSBI5_PHYS + 0x80000)
#define MSM_GSBI6_QUP_PHYS      (MSM_GSBI6_PHYS + 0x80000)
#define MSM_GSBI7_QUP_PHYS      (MSM_GSBI7_PHYS + 0x80000)
#define MSM_GSBI8_QUP_PHYS      (MSM_GSBI8_PHYS + 0x80000)
#define MSM_GSBI9_QUP_PHYS      (MSM_GSBI9_PHYS + 0x80000)
#define MSM_GSBI10_QUP_PHYS     (MSM_GSBI10_PHYS + 0x80000)
#define MSM_GSBI11_QUP_PHYS     (MSM_GSBI11_PHYS + 0x20000)
#define MSM_GSBI12_QUP_PHYS     (MSM_GSBI12_PHYS + 0x20000)
#define MSM_QUP_SIZE            SZ_4K

#define MSM_GSBI4_I2C_SDA	20
#define MSM_GSBI4_I2C_CLK	21

#endif
