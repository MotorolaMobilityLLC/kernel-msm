// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/eom_ioctl.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/phy_core.h>
#include <linux/slab.h>

extern const unsigned char pcie_link_speed[];

struct pcie_phy_dev {
	struct pci_dev *pdev;
	/* Generic PHY interface */
	struct eom_phy_ops phy_ops;
	/* Mapped PHY register base */
	void __iomem *base;
	/* Root Complex index */
	int rc_index;
	int link_speed;
	u32 nr_lanes;
	struct list_head list;
};

static LIST_HEAD(pcie_phy_list);

static DEFINE_MUTEX(pcie_phy_lock);

static int pcie_phy_check_state(struct pcie_phy_dev *pcie_phy)
{
	enum pcie_link_width width;
	enum pci_bus_speed speed;
	u16 lnksta16;

	pcie_capability_read_word(pcie_phy->pdev, PCI_EXP_LNKSTA, &lnksta16);
	speed = FIELD_GET(PCI_EXP_LNKSTA_CLS, lnksta16);
	width = FIELD_GET(PCI_EXP_LNKSTA_NLW, lnksta16);
	if (!(speed > 0 && width > 0)) {
		pr_err("EOM:PCIe (RC%d) Link not up can't read register\n", pcie_phy->rc_index);
		return -ENODEV;
	}

	return 0;
}

/* Read from PCIe PHY register */
static int pcie_phy_read(void *priv, u32 offset, u32 *value)
{
	struct pcie_phy_dev *pcie_phy = (struct pcie_phy_dev *)priv;

	if (pcie_phy_check_state(pcie_phy) < 0)
		return -ENODEV;

	*value = readl(pcie_phy->base + offset);

	return 0;
}

/* Write to PCIe PHY register */
static int pcie_phy_write(void *priv, u32 offset, u32 value)
{
	struct pcie_phy_dev *pcie_phy = (struct pcie_phy_dev *)priv;
	u32 temp;

	if (pcie_phy_check_state(pcie_phy) < 0)
		return -ENODEV;

	writel(value, pcie_phy->base + offset);
	temp = readl(pcie_phy->base + offset);

	return 0;
}

/* Get current PCie link speed and width, then update the current number of lanes */
static void pcie_phy_get_caps(void *priv)
{
	struct pcie_phy_dev *pcie_phy = (struct pcie_phy_dev *)priv;
	int rc_index = pcie_phy->rc_index;
	enum pcie_link_width width;
	enum pci_bus_speed speed;
	u16 lnksta16;

	pcie_phy->pdev = pci_get_domain_bus_and_slot(rc_index, 0, 0);
	pr_debug("PCIe dev vendor %x, device %x subsystem vendor %x class %x\n",
		pcie_phy->pdev->vendor, pcie_phy->pdev->device,
		pcie_phy->pdev->subsystem_vendor, pcie_phy->pdev->class);

	pcie_capability_read_word(pcie_phy->pdev, PCI_EXP_LNKSTA, &lnksta16);
	speed = FIELD_GET(PCI_EXP_LNKSTA_CLS, lnksta16);
	width = FIELD_GET(PCI_EXP_LNKSTA_NLW, lnksta16);
	pcie_phy->nr_lanes = width;
	pcie_phy->link_speed = pcie_link_speed[speed];
	update_phy_device_nr_lanes(rc_index, 0, 0, TYPE_PCIE, pcie_phy->nr_lanes);
}

/* Initialize all PCIe PHY instances */
static int __init pcie_phy_init(void)
{
	struct pcie_phy_dev *pcie_phy;
	int index, rc_index, ret;
	struct device_node *np;
	struct resource res;

	/* Iterate through all PCIe RC nodes */
	for_each_compatible_node(np, NULL, "qcom,pci-msm") {

		/* Get the Root Complex (RC) index */
		if (of_property_read_u32(np, "linux,pci-domain", &rc_index)) {
			pr_err("PCIe node %pOF missing cell-index\n", np);
			continue;
		}

		/* Find the index of "phy" in reg-names */
		index = of_property_match_string(np, "reg-names", "phy");
		if (index < 0) {
			pr_err("PCIe RC %d: No 'phy' entry in reg-names\n",
			       rc_index);
			continue;
		}

		/* Get the PHY register base address */
		if (of_address_to_resource(np, index, &res)) {
			pr_err("PCIe RC %d: Failed to get PHY register address\n",
			       rc_index);
			continue;
		}

		/* Allocate and initialize the PCIe PHY structure */
		pcie_phy = kzalloc(sizeof(*pcie_phy), GFP_KERNEL);
		if (!pcie_phy)
			return -ENOMEM;

		/* Map the register base */
		pcie_phy->base = ioremap(res.start, resource_size(&res));
		if (!pcie_phy->base) {
			pr_err("PCIe PHY registration: ioremap for RC %d Failed\n", rc_index);
			kfree(pcie_phy);
			continue;
		}

		/* Set RC index and register PHY with the generic PHY interface */
		pcie_phy->rc_index = rc_index;
		/* set default nr lanes to '1' as we will have at least one lane */
		pcie_phy->nr_lanes = 1;

		pr_debug("PCIe PHY registration: for RC %d\n", rc_index);

		pcie_phy->phy_ops.phy_read = pcie_phy_read;
		pcie_phy->phy_ops.phy_write = pcie_phy_write;
		pcie_phy->phy_ops.get_caps = pcie_phy_get_caps;
		ret = register_phy_device(&pcie_phy->phy_ops, pcie_phy,
					  rc_index, 0, 0, TYPE_PCIE,
					  pcie_phy->nr_lanes);
		if (ret) {
			pr_err("PCIe PHY registration: for RC %d Failed\n", rc_index);
			iounmap(pcie_phy->base);
			kfree(pcie_phy);
			continue;
		}

		/* Add to the global list of PCIe PHYs */
		mutex_lock(&pcie_phy_lock);
		list_add_tail(&pcie_phy->list, &pcie_phy_list);
		mutex_unlock(&pcie_phy_lock);
		pr_debug("PCIe PHY registered (RC %d, base: 0x%llx, size: 0x%llx)\n",
			rc_index, (unsigned long long)res.start,
			(unsigned long long)resource_size(&res));
	}

	return 0;
}

/* Cleanup function */
static void __exit pcie_phy_exit(void)
{
	struct pcie_phy_dev *pcie_phy, *tmp;

	mutex_lock(&pcie_phy_lock);
	list_for_each_entry_safe(pcie_phy, tmp, &pcie_phy_list, list) {
		unregister_phy_device(&pcie_phy->phy_ops, pcie_phy->rc_index, TYPE_PCIE);
		iounmap(pcie_phy->base);
		list_del(&pcie_phy->list);
		kfree(pcie_phy);
	}
	mutex_unlock(&pcie_phy_lock);
}
module_init(pcie_phy_init);
module_exit(pcie_phy_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multi-RC PCIe PHY Access Driver for EOM");
