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
		dwc_otg_pdata.pmic_type = SHADY_COVE;
		dwc_otg_pdata.charger_detect_enable = 0;
		dwc_otg_pdata.usb2_phy_type = get_usb2_phy_type();
		 dwc_otg_pdata.charging_compliance =
			dwc_otg_get_usbspecoverride(MOFD_SMIP_VIOLATE_BC_ADDR);

		if (dwc_otg_pdata.usb2_phy_type == USB2_PHY_ULPI) {
			dwc_otg_pdata.charger_detect_enable = 1;
			dwc_otg_pdata.using_vusbphy = 0;
		} else {
			dwc_otg_pdata.using_vusbphy = 1;
			dwc_otg_pdata.utmi_fs_det_wa = 1;
			dwc_otg_pdata.utmi_eye_calibration = 0x51801;
		}
		return &dwc_otg_pdata;
	default:
		break;
	}

	return NULL;
}
#endif

static void otg_pci_early_quirks(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = get_otg_platform_data(pci_dev);
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_MRFL_DWC3_OTG,
			otg_pci_early_quirks);

