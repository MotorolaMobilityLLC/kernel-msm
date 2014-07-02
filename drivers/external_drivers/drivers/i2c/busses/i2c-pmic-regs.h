/*
 * i2c-pmic-regs.h - PMIC I2C registers
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Yegnesh Iyer <yegnesh.s.iyer@intel.com>
 */

#ifndef __I2C_PMIC_REGS_H__
#define __I2C_PMIC_REGS_H__

#include <linux/mutex.h>
#include <linux/wakelock.h>

/*********************************************************************
 *		Generic defines
 *********************************************************************/

#define D7 (1 << 7)
#define D6 (1 << 6)
#define D5 (1 << 5)
#define D4 (1 << 4)
#define D3 (1 << 3)
#define D2 (1 << 2)
#define D1 (1 << 1)
#define D0 (1 << 0)

#define PMIC_SRAM_INTR_ADDR 0xFFFFF616

#define I2C_MSG_LEN		4

#define I2COVRCTRL_ADDR		0x58
#define I2COVRDADDR_ADDR	0x59
#define I2COVROFFSET_ADDR	0x5A
#define I2COVRWRDATA_ADDR	0x5B
#define I2COVRRDDATA_ADDR	0x5C

#define IRQLVL1_ADDR			0x01
#define IRQLVL1_MASK_ADDR		0x0c
#define IRQLVL1_CHRGR_MASK		D5

#define MCHGRIRQ1_ADDR			0x13
#define MCHGRIRQ0_ADDR			0x12

#define PMIC_I2C_INTR_MASK ((u8)(D3|D2|D1))
#define I2COVRCTRL_I2C_RD D1
#define I2COVRCTRL_I2C_WR D0
#define CHGRIRQ0_ADDR			0x07

#define IRQ0_I2C_BIT_POS 1

struct pmic_i2c_dev {
	int irq;
	u32 pmic_intr_sram_addr;
	struct i2c_adapter adapter;
	int i2c_rw;
	wait_queue_head_t i2c_wait;
	struct mutex i2c_pmic_rw_lock;
	void __iomem *pmic_intr_map;
	struct wake_lock i2c_wake_lock;
	struct device *dev;
};

#endif
