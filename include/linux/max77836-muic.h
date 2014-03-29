/*
 * max77836-muic.h - MUIC for the Maxim 14577
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __MAX77836_MUIC_H__
#define __MAX77836_MUIC_H__

#include <linux/muic.h>

#define MUIC_DEV_NAME	"muic-max77836"


/* max77836 muic register read/write related information defines. */

/* MAX77836 STATUS1 register */
#define STATUS1_ADC_SHIFT		0
#define STATUS1_ADCLOW_SHIFT		5
#define STATUS1_ADCERR_SHIFT		6
#define STATUS1_ADC_MASK		(0x1f << STATUS1_ADC_SHIFT)
#define STATUS1_ADCLOW_MASK		(0x1 << STATUS1_ADCLOW_SHIFT)
#define STATUS1_ADCERR_MASK		(0x1 << STATUS1_ADCERR_SHIFT)

/* MAX77836 STATUS2 register */
#define STATUS2_CHGTYP_SHIFT		0
#define STATUS2_CHGDETRUN_SHIFT		3
#define STATUS2_DBCHG_SHIFT		5
#define STATUS2_VBVOLT_SHIFT		6
#define STATUS2_CHGTYP_MASK		(0x7 << STATUS2_CHGTYP_SHIFT)
#define STATUS2_CHGDETRUN_MASK		(0x1 << STATUS2_CHGDETRUN_SHIFT)
#define STATUS2_DBCHG_MASK		(0x1 << STATUS2_DBCHG_SHIFT)
#define STATUS2_VBVOLT_MASK		(0x1 << STATUS2_VBVOLT_SHIFT)

/* MAX77836 CONTROL1 register */
#define COMN1SW_SHIFT			0
#define COMP2SW_SHIFT			3
#define MICEN_SHIFT			6
#define IDBEN_SHIFT			7
#define COMN1SW_MASK			(0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK			(0x7 << COMP2SW_SHIFT)
#define MICEN_MASK			(0x1 << MICEN_SHIFT)
#define IDBEN_MASK			(0x1 << IDBEN_SHIFT)
#define CLEAR_IDBEN_MICEN_MASK		(COMN1SW_MASK | COMP2SW_MASK)

/* MAX77836 CONTROL3 register */
#define CTRL3_JIGSET_SHIFT		0
#define CTRL3_BOOTSET_SHIFT		2
#define CTRL3_ADCDBSET_SHIFT		4
#define CTRL3_JIGSET_MASK		(0x3 << CTRL3_JIGSET_SHIFT)
#define CTRL3_BOOTSET_MASK		(0x3 << CTRL3_BOOTSET_SHIFT)
#define CTRL3_ADCDBSET_MASK		(0x3 << CTRL3_ADCDBSET_SHIFT)

/* MAX77836 MUIC Charger Type Detection Output values */
typedef enum {
	/* No Valid voltage at VB (Vvb < Vvbdet) */
	CHGTYP_NOTHING_ATTACH		= 0x00,
	/* Unknown (D+/D- does not present a valid USB charger signature) */
	CHGTYP_USB			= 0x01,
	/* Charging Downstream Port */
	CHGTYP_CDP			= 0x02,
	/* Dedicated Charger (D+/D- shorted) */
	CHGTYP_DEDICATED_CHARGER	= 0x03,
	/* Special 500mA charger, max current 500mA */
	CHGTYP_500MA_CHARGER		= 0x04,
	/* Special 1A charger, max current 1A */
	CHGTYP_1A_CHARGER		= 0x05,
	/* Reserved for Future Use */
	CHGTYP_RFU			= 0x06,
	/* Dead Battery Charging, max current 100mA */
	CHGTYP_DB_100MA_CHARGER		= 0x07,
} chgtyp_t;

/* muic register value for COMN1, COMN2 in CTRL1 reg  */

/*
 * MAX77836 CONTROL1 register
 * ID Bypass [7] / Mic En [6] / D+ [5:3] / D- [2:0]
 * 0: ID Bypass Open / 1: IDB connect to UID
 * 0: Mic En Open / 1: Mic connect to VB
 * 000: Open / 001: USB / 010: Audio / 011: UART
 */
enum max77836_reg_ctrl1_val {
	MAX77836_MUIC_CTRL1_ID_OPEN	= 0x0,
	MAX77836_MUIC_CTRL1_ID_BYPASS	= 0x1,
	MAX77836_MUIC_CTRL1_MIC_OPEN	= 0x0,
	MAX77836_MUIC_CTRL1_MIC_VB	= 0x1,
	MAX77836_MUIC_CTRL1_COM_OPEN	= 0x00,
	MAX77836_MUIC_CTRL1_COM_USB	= 0x01,
	MAX77836_MUIC_CTRL1_COM_AUDIO	= 0x02,
	MAX77836_MUIC_CTRL1_COM_UART	= 0x03,
};

typedef enum {
	CTRL1_OPEN	= (MAX77836_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_MIC_OPEN << MICEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_OPEN << COMP2SW_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_OPEN << COMN1SW_SHIFT),
	CTRL1_USB	= (MAX77836_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_MIC_OPEN << MICEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_USB << COMP2SW_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_USB << COMN1SW_SHIFT),
	CTRL1_AUDIO	= (MAX77836_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_MIC_OPEN << MICEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_AUDIO << COMP2SW_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_AUDIO << COMN1SW_SHIFT),
	CTRL1_UART	= (MAX77836_MUIC_CTRL1_ID_OPEN << IDBEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_MIC_OPEN << MICEN_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_UART << COMP2SW_SHIFT) | \
			(MAX77836_MUIC_CTRL1_COM_UART << COMN1SW_SHIFT),
} max77836_reg_ctrl1_t;

enum max77836_muic_reg_init_value {
	REG_CONTROL3_VALUE		= (0x20),
};

enum usb_cable_status {
        USB_CABLE_DETACHED = 0,
        USB_CABLE_ATTACHED,
        USB_OTGHOST_DETACHED,
        USB_OTGHOST_ATTACHED,
        USB_POWERED_HOST_DETACHED,
        USB_POWERED_HOST_ATTACHED,
        USB_CABLE_DETACHED_WITHOUT_NOTI,
};

struct max77836_muic_vps_data {
	u8			adcerr;
	u8			adclow;
	enum muic_adc adc;
	u8			vbvolt;
	u8			chgdetrun;
	chgtyp_t		chgtyp;
	max77836_reg_ctrl1_t	control1;
	const char		*vps_name;
	const enum muic_attached_dev attached_dev;
};

/* muic chip specific internal data structure
 * that setted at max77836-muic.c file
 */
struct max77836_muic_data {
	struct device *dev;
	struct i2c_client *i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex muic_mutex;
	struct max77836_platform_data *mfd_pdata;
	struct sec_switch_data *switch_data;

	int	irq_adc;
	int irq_adcerr;
	int	irq_chgtyp;
	int	irq_vbvolt;

	bool is_usb_ready;
	bool is_muic_ready;

	int usb_path;
	int uart_path;

	bool adcen;
	/* muic current attached device */
	enum muic_attached_dev	attached_dev;

	struct delayed_work	init_work;
	struct delayed_work	usb_work;
};

#endif /* __MAX77836_MUIC_H__ */
