/*
 * Intel Penwell USB OTG transceiver driver
 * Copyright (C) 2009 - 2010, Intel Corporation.
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

#ifndef __DWC3_INTEL_H
#define __DWC3_INTEL_H

#include "otg.h"

enum intel_mid_pmic_type {
	NO_PMIC,
	SHADY_COVE,
	BASIN_COVE
};

struct intel_dwc_otg_pdata {
	unsigned device_hibernation:1;
	int is_hvp;
	enum intel_mid_pmic_type pmic_type;
	int charger_detect_enable;
	int gpio_cs;
	int gpio_reset;
	int gpio_id;
	int id;
	int charging_compliance;
	struct delayed_work suspend_discon_work;
	u8 ti_phy_vs1;
	int sdp_charging;
	enum usb_phy_intf usb2_phy_type;

	/* USB2 electronic calibration value.
	 *
	 * ULPI(TI1211):
	 * ZHSDRV and IHSTX of VS1 register for TI1211 PHY.
	 * They impact the eye diagram qulity.
	 *
	 * UTMI(Intel):
	 * USB2PERPORT register.
	 * D14:  0=full-bit PE; 1=half-bit PE
	 * D[13:11]:  PE/DE bias (0-to-7)
	 * D[10:08]:  TX bias (0-to-7)
	 */
	int ulpi_eye_calibration;
	int utmi_eye_calibration;

	/* If the VUSBPHY power rail using for providing
	 * power for USB PHY. */
	int using_vusbphy;
};

/* timeout for disconnect from a suspended host */
#define SUSPEND_DISCONNECT_TIMEOUT	(HZ * 300)

#define TUSB1211_VENDOR_ID_LO					0x00
#define TUSB1211_VENDOR_ID_HI					0x01
#define TUSB1211_PRODUCT_ID_LO					0x02
#define TUSB1211_PRODUCT_ID_HI					0x03
#define TUSB1211_FUNC_CTRL						0x04
#define TUSB1211_FUNC_CTRL_SET					0x05
#define TUSB1211_FUNC_CTRL_CLR					0x06
#define TUSB1211_IFC_CTRL						0x07
#define TUSB1211_IFC_CTRL_SET					0x08
#define TUSB1211_IFC_CTRL_CLR					0x09
#define TUSB1211_OTG_CTRL						0x0A
#define TUSB1211_OTG_CTRL_SET					0x0B
#define TUSB1211_OTG_CTRL_CLR					0x0C
#define TUSB1211_USB_INT_EN_RISE				0x0D
#define TUSB1211_USB_INT_EN_RISE_SET			0x0E
#define TUSB1211_USB_INT_EN_RISE_CLR			0x0F
#define TUSB1211_USB_INT_EN_FALL				0x10
#define TUSB1211_USB_INT_EN_FALL_SET			0x11
#define TUSB1211_USB_INT_EN_FALL_CLR			0x12
#define TUSB1211_USB_INT_STS					0x13
#define TUSB1211_USB_INT_LATCH					0x14
#define TUSB1211_DEBUG							0x15
#define TUSB1211_SCRATCH_REG					0x16
#define TUSB1211_SCRATCH_REG_SET				0x17
#define TUSB1211_SCRATCH_REG_CLR				0x18
#define TUSB1211_ACCESS_EXT_REG_SET				0x2F

#define TUSB1211_VENDOR_SPECIFIC1				0x80
#define TUSB1211_VENDOR_SPECIFIC1_SET			0x81
#define TUSB1211_VENDOR_SPECIFIC1_CLR			0x82
#define TUSB1211_POWER_CONTROL					0x3D
#define TUSB1211_POWER_CONTROL_SET				0x3E
#define TUSB1211_POWER_CONTROL_CLR				0x3F

#define TUSB1211_VENDOR_SPECIFIC2				0x80
#define TUSB1211_VENDOR_SPECIFIC2_SET			0x81
#define TUSB1211_VENDOR_SPECIFIC2_CLR			0x82
#define TUSB1211_VENDOR_SPECIFIC2_STS			0x83
#define TUSB1211_VENDOR_SPECIFIC2_LATCH			0x84
#define TUSB1211_VENDOR_SPECIFIC3				0x85
#define TUSB1211_VENDOR_SPECIFIC3_SET			0x86
#define TUSB1211_VENDOR_SPECIFIC3_CLR			0x87
#define TUSB1211_VENDOR_SPECIFIC4				0x88
#define TUSB1211_VENDOR_SPECIFIC4_SET			0x89
#define TUSB1211_VENDOR_SPECIFIC4_CLR			0x8A
#define TUSB1211_VENDOR_SPECIFIC5				0x8B
#define TUSB1211_VENDOR_SPECIFIC5_SET			0x8C
#define TUSB1211_VENDOR_SPECIFIC5_CLR			0x8D
#define TUSB1211_VENDOR_SPECIFIC6				0x8E
#define TUSB1211_VENDOR_SPECIFIC6_SET			0x8F
#define TUSB1211_VENDOR_SPECIFIC6_CLR			0x90

#define VS1_DATAPOLARITY						(1 << 6)
#define VS1_ZHSDRV(v)					((v & 0x3) << 5)
#define VS1_IHSTX(v)						 ((v & 0x7))

#define VS2STS_VBUS_MNTR_STS					(1 << 7)
#define VS2STS_REG3V3IN_MNTR_STS				(1 << 6)
#define VS2STS_SVLDCONWKB_WDOG_STS				(1 << 5)
#define VS2STS_ID_FLOAT_STS						(1 << 4)
#define VS2STS_ID_RARBRC_STS(v)					((v & 0x3) << 2)
#define VS2STS_BVALID_STS						(1 << 0)

#define VS3_CHGD_IDP_SRC_EN						(1 << 6)
#define VS3_IDPULLUP_WK_EN						(1 << 5)
#define VS3_SW_USB_DET							(1 << 4)
#define VS3_DATA_CONTACT_DET_EN					(1 << 3)
#define VS3_REG3V3_VSEL(v)					   (v & 0x7)

#define VS4_ACA_DET_EN							(1 << 6)
#define VS4_RABUSIN_EN							(1 << 5)
#define VS4_R1KSERIES							(1 << 4)
#define VS4_PSW_OSOD							(1 << 3)
#define VS4_PSW_CMOS							(1 << 2)
#define VS4_CHGD_SERX_DP						(1 << 1)
#define VS4_CHGD_SERX_DM						(1 << 0)

#define VS5_AUTORESUME_WDOG_EN					(1 << 6)
#define VS5_ID_FLOAT_EN							(1 << 5)
#define VS5_ID_RES_EN							(1 << 4)
#define VS5_SVLDCONWKB_WDOG_EN					(1 << 3)
#define VS5_VBUS_MNTR_RISE_EN					(1 << 2)
#define VS5_VBUS_MNTR_FALL_EN					(1 << 1)
#define VS5_REG3V3IN_MNTR_EN					(1 << 0)

#define DEBUG_LINESTATE                       (0x3 << 0)

#define OTGCTRL_USEEXTVBUS_INDICATOR			(1 << 7)
#define OTGCTRL_DRVVBUSEXTERNAL					(1 << 6)
#define OTGCTRL_DRVVBUS							(1 << 5)
#define OTGCTRL_CHRGVBUS						(1 << 4)
#define OTGCTRL_DISCHRGVBUS						(1 << 3)
#define OTGCTRL_DMPULLDOWN						(1 << 2)
#define OTGCTRL_DPPULLDOWN						(1 << 1)
#define OTGCTRL_IDPULLUP						(1 << 0)

#define FUNCCTRL_SUSPENDM						(1 << 6)
#define FUNCCTRL_RESET							(1 << 5)
#define FUNCCTRL_OPMODE(v)				((v & 0x3) << 3)
#define FUNCCTRL_TERMSELECT						(1 << 2)
#define FUNCCTRL_XCVRSELECT(v)					(v & 0x3)

#define PWCTRL_HWDETECT							(1 << 7)
#define PWCTRL_DP_VSRC_EN						(1 << 6)
#define PWCTRL_VDAT_DET							(1 << 5)
#define PWCTRL_DP_WKPU_EN						(1 << 4)
#define PWCTRL_BVALID_FALL						(1 << 3)
#define PWCTRL_BVALID_RISE						(1 << 2)
#define PWCTRL_DET_COMP							(1 << 1)
#define PWCTRL_SW_CONTROL						(1 << 0)


#define PMIC_VLDOCNT                0xAF
#define PMIC_VLDOCNT_VUSBPHYEN      (1 << 2)

#define PMIC_TLP1ESBS0I1VNNBASE		0X6B
#define PMIC_I2COVRDADDR			0x59
#define PMIC_I2COVROFFSET			0x5A
#define PMIC_USBPHYCTRL				0x30
#define PMIC_I2COVRWRDATA			0x5B
#define PMIC_I2COVRCTRL				0x58
#define PMIC_I2COVRCTL_I2CWR		0x01

#define USBPHYRSTB				(1 << 0)
#define USBPHYCTRL_D0			(1 << 0)
#define PMIC_USBIDCTRL				0x19
#define USBIDCTRL_ACA_DETEN_D1	(1 << 1)
#define USBIDCTRL_USB_IDEN_D0	(1 << 0)
#define PMIC_USBIDSTS				0x1A
#define USBIDSTS_ID_GND			(1 << 0)
#define USBIDSTS_ID_RARBRC_STS(v)	((v & 0x3)  << 1)
#define USBIDSTS_ID_FLOAT_STS	(1 << 3)
#define PMIC_USBPHYCTRL_D0		(1 << 0)
#define APBFC_EXIOTG3_MISC0_REG			0xF90FF85C

#define DATACON_TIMEOUT		750
#define DATACON_INTERVAL	10
#define PCI_DEVICE_ID_DWC 0x119E

#define VENDOR_ID_MASK (0x03 << 6)
#define BASIN_COVE_PMIC_ID (0x03 << 6)

#define PMIC_MAJOR_REV (0x07 << 3)
#define PMIC_A0_MAJOR_REV 0x00

/* ShardyCove register */
#define PMIC_SCHGRIRQ1		0X4F
#define PMIC_GPADCREQ_REG	0xDC
#define PMIC_ADCIRQ_REG		0x06
#define PMIC_USBIDRSLTL		0xEF
#define PMIC_USBIDRSLTH		0xEE
#define PMIC_GPADCREQ_ADC_USBID	(1 << 1)
#define PMIC_ADCIRQ_USBID	(1 << 0)
#define PMIC_USBIDRSLTL_USBID_L_MASK	(0xFF)
#define PMIC_USBIDRSLTH_USBID_H_MASK	(0x0F)
#define PMIC_USBIDRSLTH_USBID_CURSRC_MASK	(0xF0)
#define PMIC_SCHGRIRQ1_SUSBIDDET(v)	((v & 0x3) << 3)

/* SCCB registers */
#define SCCB_USB_CFG	0xff03a018
#define SCCB_USB_CFG_SELECT_ULPI	(1 << 14)

/* SMIP address which check if violate BC */
#define MOFD_SMIP_VIOLATE_BC_ADDR	0xFFFC631B
#define MERR_SMIP_VIOLATE_BC_ADDR	0xFFFCE717
#define SMIP_VIOLATE_BC_MASK	0x40

/* UTMI(Intel) PHY USB2PERPORT register */
#define UTMI_PHY_USB2PERPORT	0xf90B1200
#endif /* __DWC3_INTEL_H */
