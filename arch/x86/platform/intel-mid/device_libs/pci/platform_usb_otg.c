/*
 * platform_otg_pci.c: USB OTG platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/sfi.h>
#include <linux/pci.h>
#include <asm/intel-mid.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_em_config.h>
#include <asm/spid.h>
#include <asm/intel-mid.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_USB_DWC3_OTG
#include <linux/usb/dwc3-intel-mid.h>
static struct intel_dwc_otg_pdata dwc_otg_pdata;

struct {
	u8 name[16];
	u32 val;
} usb2_el_cal[] = {
	{"ULPICAL_7D", 0x7F},
	{"ULPICAL_7F", 0x7D},
	{"UTMICAL_PEDE3TX0", 0x51801},
	{"UTMICAL_PEDE6TX7", 0x53f01},
};
#define USB_ULPI_SFI_PREFIX "ULPI"
#define USB_UTMI_SFI_PREFIX "UTMI"

void sfi_handle_usb(struct sfi_device_table_entry *pentry, struct devs_id *dev)
{
	int i;

	if (!dev || !dev->name) {
		pr_info("USB SFI entry is NULL!\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(usb2_el_cal); i++) {
		if (!strncmp(dev->name, usb2_el_cal[i].name, strlen(dev->name))) {
			if (!strncmp(dev->name, USB_ULPI_SFI_PREFIX, strlen(USB_ULPI_SFI_PREFIX)))
				dwc_otg_pdata.ulpi_eye_calibration = usb2_el_cal[i].val;
			else if (!strncmp(dev->name, USB_UTMI_SFI_PREFIX, strlen(USB_UTMI_SFI_PREFIX)))
				dwc_otg_pdata.utmi_eye_calibration = usb2_el_cal[i].val;
			else
				pr_info("%s:is Invalid USB SFI Entry Name!\n", dev->name);

			break;
		}
	}
}

static bool dwc_otg_get_usbspecoverride(u32 addr)
{
	void __iomem *usb_comp_iomap;
	bool usb_spec_override;

	/* Read MISCFLAGS byte */
	usb_comp_iomap = ioremap_nocache(addr, 4);
	/* MISCFLAGS.BIT[6] indicates USB spec override */
	usb_spec_override = ioread8(usb_comp_iomap) & SMIP_VIOLATE_BC_MASK;
	iounmap(usb_comp_iomap);

	return usb_spec_override;
}

/*
 * Read USB Complaint charging bit from BIOS.
 *   1 - Not Compliant, default
 *   0 - Compliant
 */
static int dwc_otg_byt_get_usbspecoverride(void)
{
	struct em_config_oem1_data oem1_data;
	int charge_bit = 1, ret = 0;

	ret = em_config_get_oem1_data(&oem1_data);
	if (ret <= 0) {
		pr_err("no OEM1 table, return default value\n");
		return charge_bit;
	}
	charge_bit = oem1_data.fpo_0 & BIT(0);
	pr_info("OEM1 charging bit = %d\n", charge_bit);
	return charge_bit;
}

/* Read SCCB_USB_CFG.bit14 to get the current phy select setting */
static enum usb_phy_intf get_usb2_phy_type(void)
{
	void __iomem *addr;
	u32 val;

	addr = ioremap_nocache(SCCB_USB_CFG, 4);
	if (!addr)
		return USB2_PHY_ULPI;

	val = readl(addr) & SCCB_USB_CFG_SELECT_ULPI;
	iounmap(addr);

	if (val)
		return USB2_PHY_ULPI;
	else
		return USB2_PHY_UTMI;
}

static struct intel_dwc_otg_pdata *get_otg_platform_data(struct pci_dev *pdev)
{
	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MRFL_DWC3_OTG:
		if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
			INTEL_MID_BOARD(1, TABLET, MOFD)) {
			dwc_otg_pdata.pmic_type = SHADY_COVE;
			dwc_otg_pdata.charger_detect_enable = 0;
			dwc_otg_pdata.usb2_phy_type = get_usb2_phy_type();
			 dwc_otg_pdata.charging_compliance =
				dwc_otg_get_usbspecoverride(MOFD_SMIP_VIOLATE_BC_ADDR);

			if (dwc_otg_pdata.usb2_phy_type == USB2_PHY_ULPI) {
				dwc_otg_pdata.charger_detect_enable = 1;
				dwc_otg_pdata.using_vusbphy = 0;
			} else
				dwc_otg_pdata.using_vusbphy = 1;
		} else if (INTEL_MID_BOARD(1, PHONE, MRFL)) {
			dwc_otg_pdata.pmic_type = BASIN_COVE;
			dwc_otg_pdata.using_vusbphy = 1;
			dwc_otg_pdata.charger_detect_enable = 1;
			dwc_otg_pdata.device_hibernation = 1;

			dwc_otg_pdata.charging_compliance =
				dwc_otg_get_usbspecoverride(MERR_SMIP_VIOLATE_BC_ADDR);
			dwc_otg_pdata.usb2_phy_type = USB2_PHY_ULPI;
			dwc_otg_pdata.ulpi_eye_calibration = 0x7D;

		} else if (intel_mid_identify_sim() ==
				INTEL_MID_CPU_SIMULATION_HVP) {
			dwc_otg_pdata.pmic_type = NO_PMIC;
			dwc_otg_pdata.using_vusbphy = 0;
			dwc_otg_pdata.is_hvp = 1;
			dwc_otg_pdata.charger_detect_enable = 0;
			dwc_otg_pdata.usb2_phy_type = USB2_PHY_ULPI;
		}
		return &dwc_otg_pdata;
	case PCI_DEVICE_ID_INTEL_BYT_OTG:
		/* FIXME: Hardcode now, but need to use ACPI table for GPIO */
		if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, RVP3) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, RVP3)) {
			dwc_otg_pdata.gpio_cs = 156;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x4f;
		} else if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, 8PR0) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, 8PR0)) {
			dwc_otg_pdata.gpio_cs = 54;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x4f;
		} else if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, 8PR1) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, 8PR1)) {
			dwc_otg_pdata.gpio_cs = 54;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x7f;
			dwc_otg_pdata.sdp_charging = 1;
			dwc_otg_pdata.charging_compliance =
				!dwc_otg_byt_get_usbspecoverride();
		} else if (INTEL_MID_BOARD(3, TABLET, BYT, BLK, PRO, CRV2) ||
			INTEL_MID_BOARD(3, TABLET, BYT, BLK, ENG, CRV2)) {
			dwc_otg_pdata.gpio_cs = 54;
			dwc_otg_pdata.gpio_reset = 144;
			dwc_otg_pdata.ti_phy_vs1 = 0x7f;
			dwc_otg_pdata.gpio_id = 156;
			dwc_otg_pdata.sdp_charging = 1;
			dwc_otg_pdata.charging_compliance =
				!dwc_otg_byt_get_usbspecoverride();
		}
		return &dwc_otg_pdata;
	default:
		break;
	}

	return NULL;
}
#endif

#ifdef CONFIG_USB_PENWELL_OTG
#include <linux/usb/penwell_otg.h>
static struct intel_mid_otg_pdata otg_pdata = {
	.gpio_vbus = 0,
	.gpio_cs = 0,
	.gpio_reset = 0,
	.charging_compliance = 0,
	.hnp_poll_support = 0,
	.power_budget = 500
};

static struct intel_mid_otg_pdata *get_otg_platform_data(struct pci_dev *pdev)
{
	struct intel_mid_otg_pdata *pdata = &otg_pdata;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MFD_OTG:
		if (INTEL_MID_BOARD(2, TABLET, MFLD, SLP, PRO) ||
			INTEL_MID_BOARD(2, TABLET, MFLD, SLP, ENG))
			pdata->gpio_vbus = 54;

		if (!INTEL_MID_BOARD(2, TABLET, MFLD, RR, PRO) &&
			!INTEL_MID_BOARD(2, TABLET, MFLD, RR, ENG))
			pdata->power_budget = 200;
		break;

	case PCI_DEVICE_ID_INTEL_CLV_OTG:
		pdata->gpio_cs = get_gpio_by_name("usb_otg_phy_cs");
		if (pdata->gpio_cs == -1) {
			pr_err("%s: No gpio pin usb_otg_phy_cs\n", __func__);
			return NULL;
		}
		pdata->gpio_reset = get_gpio_by_name("usb_otg_phy_rst");
		if (pdata->gpio_reset == -1) {
			pr_err("%s: No gpio pin usb_otg_phy_rst\n", __func__);
			return NULL;
		}
		break;

	default:
		break;
	}

	return pdata;
}
#endif

static void otg_pci_early_quirks(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = get_otg_platform_data(pci_dev);
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_MFD_OTG,
			otg_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_CLV_OTG,
			otg_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_MRFL_DWC3_OTG,
			otg_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_OTG,
			otg_pci_early_quirks);

static void quirk_byt_otg_d3_delay(struct pci_dev *dev)
{
	dev->d3_delay = 10;
}
DECLARE_PCI_FIXUP_ENABLE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_BYT_OTG,
			quirk_byt_otg_d3_delay);
