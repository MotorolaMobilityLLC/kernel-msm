/*
 * PCI interface driver for DW SPI Core
 *
 * Copyright (c) 2009, Intel Corporation.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/module.h>

#include "spi-dw.h"

#define DRIVER_NAME "dw_spi_pci"

struct dw_spi_pci {
	struct pci_dev	*pdev;
	struct dw_spi	dws;
};

static int spi_pci_probe(struct pci_dev *pdev,
	const struct pci_device_id *ent)
{
	struct dw_spi_pci *dwpci;
	struct dw_spi *dws;
	int pci_bar = 0;
	int ret;

	printk(KERN_INFO "DW: found PCI SPI controller(ID: %04x:%04x)\n",
		pdev->vendor, pdev->device);

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	dwpci = kzalloc(sizeof(struct dw_spi_pci), GFP_KERNEL);
	if (!dwpci) {
		ret = -ENOMEM;
		goto err_disable;
	}

	dwpci->pdev = pdev;
	dws = &dwpci->dws;

	/* Get basic io resource and map it */
	dws->paddr = pci_resource_start(pdev, pci_bar);
	dws->iolen = pci_resource_len(pdev, pci_bar);

	ret = pci_request_region(pdev, pci_bar, dev_name(&pdev->dev));
	if (ret)
		goto err_kfree;

	dws->regs = ioremap_nocache((unsigned long)dws->paddr,
				pci_resource_len(pdev, pci_bar));
	if (!dws->regs) {
		ret = -ENOMEM;
		goto err_release_reg;
	}

	dws->parent_dev = &pdev->dev;
	dws->bus_num = ent->driver_data;
	dws->num_cs = 4;
	dws->irq = pdev->irq;

	/*
	 * Specific handling for Intel MID paltforms, like dma setup,
	 * clock rate, FIFO depth.
	 */
	ret = dw_spi_mid_init(dws, ent->driver_data);
	if (ret)
		goto err_unmap;

	ret = dw_spi_add_host(dws);
	if (ret)
		goto err_unmap;

	/* PCI hook and SPI hook use the same drv data */
	pci_set_drvdata(pdev, dwpci);

	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	return 0;

err_unmap:
	iounmap(dws->regs);
err_release_reg:
	pci_release_region(pdev, pci_bar);
err_kfree:
	kfree(dwpci);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void spi_pci_remove(struct pci_dev *pdev)
{
	struct dw_spi_pci *dwpci = pci_get_drvdata(pdev);

	pci_set_drvdata(pdev, NULL);
	dw_spi_remove_host(&dwpci->dws);
	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	iounmap(dwpci->dws.regs);
	pci_release_region(pdev, 0);
	kfree(dwpci);
	pci_disable_device(pdev);
}

#ifdef CONFIG_PM
static int spi_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct dw_spi_pci *dwpci = pci_get_drvdata(pdev);
	int ret;

	ret = dw_spi_suspend_host(&dwpci->dws);
	if (ret)
		return ret;
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);
	return ret;
}

static int spi_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct dw_spi_pci *dwpci = pci_get_drvdata(pdev);
	int ret;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	ret = pci_enable_device(pdev);
	if (ret)
		return ret;
	return dw_spi_resume_host(&dwpci->dws);
}

static int spi_dw_pci_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct dw_spi_pci *dwpci = pci_get_drvdata(pdev);

	dev_dbg(dev, "PCI runtime suspend called\n");
	return dw_spi_suspend_host(&dwpci->dws);
}

static int spi_dw_pci_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct dw_spi_pci *dwpci = pci_get_drvdata(pdev);

	dev_dbg(dev, "pci_runtime_resume called\n");
	return dw_spi_resume_host(&dwpci->dws);
}

static int spi_dw_pci_runtime_idle(struct device *dev)
{
	int err;

	dev_dbg(dev, "pci_runtime_idle called\n");
	if (system_state == SYSTEM_BOOTING)
		/* if SPI UART is set as default console and earlyprintk
		 * is enabled, it cannot shutdown SPI controller during booting.
		 */
		err = pm_schedule_suspend(dev, 30000);
	else
		err = pm_schedule_suspend(dev, 500);

	if (err != 0)
		return 0;

	return -EBUSY;
}

#else
#define spi_suspend	NULL
#define spi_resume	NULL
#define spi_dw_pci_runtime_suspend NULL
#define spi_dw_pci_runtime_resume NULL
#define spi_dw_pci_runtime_idle NULL
#endif

static DEFINE_PCI_DEVICE_TABLE(pci_ids) = {
	/* Intel Medfield platform SPI controller 1 */
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x0800), .driver_data = 0 },
	/* Intel Cloverview platform SPI controller 1 */
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x08E1), .driver_data = 0 },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x08EE), .driver_data = 1 },
	/* Intel EVx platform SPI controller 1 */
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x0812), .driver_data = 2 },
	{},
};

static const struct dev_pm_ops dw_spi_pm_ops = {
	.suspend = spi_suspend,
	.resume = spi_resume,
	.runtime_suspend = spi_dw_pci_runtime_suspend,
	.runtime_resume = spi_dw_pci_runtime_resume,
	.runtime_idle = spi_dw_pci_runtime_idle,
};

static struct pci_driver dw_spi_driver = {
	.name =		DRIVER_NAME,
	.id_table =	pci_ids,
	.probe =	spi_pci_probe,
	.remove =	spi_pci_remove,
	.driver =	{
		.pm	= &dw_spi_pm_ops,
	},
};

static int __init mrst_spi_init(void)
{
	return pci_register_driver(&dw_spi_driver);
}

static void __exit mrst_spi_exit(void)
{
	pci_unregister_driver(&dw_spi_driver);
}

module_init(mrst_spi_init);
module_exit(mrst_spi_exit);

MODULE_AUTHOR("Feng Tang <feng.tang@intel.com>");
MODULE_DESCRIPTION("PCI interface driver for DW SPI Core");
MODULE_LICENSE("GPL v2");
