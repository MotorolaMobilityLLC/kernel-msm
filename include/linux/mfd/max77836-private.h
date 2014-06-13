/*
 * max77836-private.h - Common API for the Maxim 14577 internal sub chip
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * SeungJin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MAX77836_PRIVATE_H__
#define __MAX77836_PRIVATE_H__

#include <linux/i2c.h>

#ifdef CONFIG_MFD_MAX77836
#define MAX77836_PMIC_ADDR	(0x46 >> 1)
#define MAX77836_MUIC_ADDR	(0x4a >> 1)


/* Slave addr = 0x47: PMIC */
enum max77836_pmic_reg
{
	MAX77836_PMIC_REG_ID		= 0x20,
	MAX77836_PMIC_REG_REV		= 0x21,
	MAX77836_PMIC_REG_INTSRC	= 0x22,
	MAX77836_PMIC_REG_INTSRC_MASK	= 0x23,
	MAX77836_PMIC_REG_TOPSYS_INT	= 0x24,
	MAX77836_PMIC_REG_TOPSYS_INTM	= 0x25,
	MAX77836_PMIC_REG_TOPSYS_STAT	= 0x26,
	MAX77836_PMIC_REG_COMP = 0x60,
	MAX77836_PMIC_REG_END,
};
#endif

#define MAX77836_I2C_ADDR		(0x4A >> 1)
#define MAX77836_REG_INVALID		(0xff)

/* Slave addr = 0x4A: Interrupt */
enum max77836_reg {
	MAX77836_REG_DEVICEID		= 0x00,
	MAX77836_REG_INT1		= 0x01,
	MAX77836_REG_INT2		= 0x02,
	MAX77836_REG_INT3		= 0x03,
	MAX77836_REG_INTMASK1		= 0x07,
	MAX77836_REG_INTMASK2		= 0x08,
	MAX77836_REG_INTMASK3		= 0x09,
	MAX77836_REG_CDETCTRL1		= 0x0A,
	MAX77836_REG_CONTROL2		= 0x0D,
	MAX77836_REG_CNFG1_LDO1		= 0x51,

	MAX77836_REG_END,
};

/* Slave addr = 0x4A: MUIC */
enum max77836_muic_reg {
	MAX77836_MUIC_REG_STATUS1	= 0x04,
	MAX77836_MUIC_REG_STATUS2	= 0x05,
	MAX77836_MUIC_REG_CONTROL1	= 0x0C,
	MAX77836_MUIC_REG_CONTROL3	= 0x0E,

	MAX77836_MUIC_REG_END,
};

/* Slave addr = 0x4A: Charger */
enum max77836_charger_reg {
	MAX77836_CHG_REG_STATUS3	= 0x06,
	MAX77836_CHG_REG_CHG_CTRL1	= 0x0F,
	MAX77836_CHG_REG_CHG_CTRL2	= 0x10,
	MAX77836_CHG_REG_CHG_CTRL3	= 0x11,
	MAX77836_CHG_REG_CHG_CTRL4	= 0x12,
	MAX77836_CHG_REG_CHG_CTRL5	= 0x13,
	MAX77836_CHG_REG_CHG_CTRL6	= 0x14,
	MAX77836_CHG_REG_CHG_CTRL7	= 0x15,

	MAX77836_CHG_REG_END,
};

/* MAX77836 REGISTER ENABLE or DISABLE bit */
enum max77836_reg_bit_control {
	MAX77836_DISABLE_BIT		= 0,
	MAX77836_ENABLE_BIT,
};

/* MAX77836 COMPOUT REGISTER */
#define BIT_COMPEN			0x10

/* MAX77836 CDETCTRL1 register */
#define CHGDETEN_SHIFT			0
#define CHGTYPMAN_SHIFT			1
#define DCHKTM_SHIFT			4
#define CHGDETEN_MASK			(0x1 << CHGDETEN_SHIFT)
#define CHGTYPMAN_MASK			(0x1 << CHGTYPMAN_SHIFT)
#define DCHKTM_MASK			(0x1 << DCHKTM_SHIFT)

/* MAX77836 CONTROL2 register */
#define CTRL2_LOWPWR_SHIFT		0
#define CTRL2_ADCEN_SHIFT		1
#define CTRL2_CPEN_SHIFT		2
#define CTRL2_ACCDET_SHIFT		5
#define CTRL2_LOWPWR_MASK		(0x1 << CTRL2_LOWPWR_SHIFT)
#define CTRL2_ADCEN_MASK		(0x1 << CTRL2_ADCEN_SHIFT)
#define CTRL2_CPEN_MASK			(0x1 << CTRL2_CPEN_SHIFT)
#define CTRL2_ACCDET_MASK		(0x1 << CTRL2_ACCDET_SHIFT)
#define CTRL2_CPEN1_LOWPWR0 ((MAX77836_ENABLE_BIT << CTRL2_CPEN_SHIFT) | \
				(MAX77836_DISABLE_BIT << CTRL2_LOWPWR_SHIFT))
#define CTRL2_CPEN0_LOWPWR1 ((MAX77836_DISABLE_BIT << CTRL2_CPEN_SHIFT) | \
				(MAX77836_ENABLE_BIT << CTRL2_LOWPWR_SHIFT))

enum max77836_irq_source {
	MAX77836_IRQ_INT1 = 0,
	MAX77836_IRQ_INT2,
	MAX77836_IRQ_INT3,

	MAX77836_IRQ_REGS_NUM,
};

enum max77836_irq {
	/* INT1 */
	MAX77836_IRQ_INT1_ADC,
	MAX77836_IRQ_INT1_ADCLOW,
	MAX77836_IRQ_INT1_ADCERR,

	/* INT2 */
	MAX77836_IRQ_INT2_CHGTYP,
	MAX77836_IRQ_INT2_CHGDETREUN,
	MAX77836_IRQ_INT2_DCDTMR,
	MAX77836_IRQ_INT2_DBCHG,
	MAX77836_IRQ_INT2_VBVOLT,

	/* INT3 */
	MAX77836_IRQ_INT3_EOC,
	MAX77836_IRQ_INT3_CGMBC,
	MAX77836_IRQ_INT3_OVP,
	MAX77836_IRQ_INT3_MBCCHGERR,

	MAX77836_IRQ_NUM,
};

struct max77836_dev {
	struct device *dev;
	struct i2c_client *i2c; /* Slave addr = 0x4A */
#ifdef CONFIG_MFD_MAX77836
	struct i2c_client *i2c_pmic; /* Slave addr = 0x46 */
#endif
	struct mutex i2c_lock;

	int irq;
	struct mutex irq_lock;
	int irq_masks_cur[MAX77836_IRQ_REGS_NUM];
	int irq_masks_cache[MAX77836_IRQ_REGS_NUM];

	/* Device ID */
	u8 vendor_id;	/* Vendor Identification */
	u8 device_id;	/* Chip Version */

	struct max77836_platform_data *pdata;
};

extern int max77836_irq_init(struct max77836_dev *max77836);
extern void max77836_irq_exit(struct max77836_dev *max77836);
extern int max77836_irq_resume(struct max77836_dev *max77836);

/* MAX77836 shared i2c API function */
extern int max77836_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int max77836_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77836_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int max77836_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int max77836_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /* __MAX77836_PRIVATE_H__ */
