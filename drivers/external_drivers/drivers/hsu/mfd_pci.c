/*
 * mfd_pci.c: driver for High Speed UART device of Intel Medfield platform
 *
 * Refer pxa.c, 8250.c and some other drivers in drivers/serial/
 *
 * (C) Copyright 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

/* Notes:
 * 1. DMA channel allocation: 0/1 channel are assigned to port 0,
 *    2/3 chan to port 1, 4/5 chan to port 3. Even number chans
 *    are used for RX, odd chans for TX
 *
 * 2. The RI/DSR/DCD/DTR are not pinned out, DCD & DSR are always
 *    asserted, only when the HW is reset the DDCD and DDSR will
 *    be triggered
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/intel_mid_pm.h>
#include <linux/pm_qos.h>

#include "mfd.h"

#ifdef CONFIG_PM
static int serial_hsu_pci_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);
	int ret = 0;

	if (up) {
		trace_hsu_func_start(up->index, __func__);
		ret = serial_hsu_do_suspend(up);
		trace_hsu_func_end(up->index, __func__, "");
	}
	return ret;
}

static int serial_hsu_pci_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);
	int ret = 0;

	if (up) {
		trace_hsu_func_start(up->index, __func__);
		ret = serial_hsu_do_resume(up);
		trace_hsu_func_end(up->index, __func__, "");
	}
	return ret;
}
#else
#define serial_hsu_pci_suspend	NULL
#define serial_hsu_pci_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int serial_hsu_pci_runtime_idle(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	return serial_hsu_do_runtime_idle(up);
}

static int serial_hsu_pci_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);
	int ret = 0;

	trace_hsu_func_start(up->index, __func__);
	ret = serial_hsu_do_suspend(up);
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}

static int serial_hsu_pci_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct uart_hsu_port *up = pci_get_drvdata(pdev);
	int ret = 0;

	trace_hsu_func_start(up->index, __func__);
	ret = serial_hsu_do_resume(up);
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}
#else
#define serial_hsu_pci_runtime_idle		NULL
#define serial_hsu_pci_runtime_suspend	NULL
#define serial_hsu_pci_runtime_resume	NULL
#endif

static const struct dev_pm_ops serial_hsu_pci_pm_ops = {

	SET_SYSTEM_SLEEP_PM_OPS(serial_hsu_pci_suspend,
				serial_hsu_pci_resume)
	SET_RUNTIME_PM_OPS(serial_hsu_pci_runtime_suspend,
				serial_hsu_pci_runtime_resume,
				serial_hsu_pci_runtime_idle)
};

DEFINE_PCI_DEVICE_TABLE(hsuart_port_pci_ids) = {
	{ PCI_VDEVICE(INTEL, 0x081B), hsu_port0 },
	{ PCI_VDEVICE(INTEL, 0x081C), hsu_port1 },
	{ PCI_VDEVICE(INTEL, 0x081D), hsu_port2 },
	/* Cloverview support */
	{ PCI_VDEVICE(INTEL, 0x08FC), hsu_port0 },
	{ PCI_VDEVICE(INTEL, 0x08FD), hsu_port1 },
	{ PCI_VDEVICE(INTEL, 0x08FE), hsu_port2 },
	/* Tangier and Anniedale support */
	{ PCI_VDEVICE(INTEL, 0x1191), hsu_port0 },
	/* VLV2 support FDK only */
	{ PCI_VDEVICE(INTEL, 0x0F0A), hsu_port0 },
	{ PCI_VDEVICE(INTEL, 0x0F0C), hsu_port1 },
	/* CHV support, enume by ACPI not PCI now */
	{ PCI_VDEVICE(INTEL, 0x228A), hsu_port0 },
	{ PCI_VDEVICE(INTEL, 0x228C), hsu_port1 },
	{},
};

DEFINE_PCI_DEVICE_TABLE(hsuart_dma_pci_ids) = {
	{ PCI_VDEVICE(INTEL, 0x081E), hsu_dma },
	/* Cloverview support */
	{ PCI_VDEVICE(INTEL, 0x08FF), hsu_dma },
	/* Tangier and Anniedale support */
	{ PCI_VDEVICE(INTEL, 0x1192), hsu_dma },
	{},
};

static int serial_hsu_pci_port_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct uart_hsu_port *up;
	int ret, port;
	resource_size_t start, len;

	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

	dev_info(&pdev->dev,
		"FUNC: %d driver: %ld addr:%lx len:%lx\n",
		PCI_FUNC(pdev->devfn), ent->driver_data,
		(ulong) start, (ulong) len);

	port = intel_mid_hsu_func_to_port(PCI_FUNC(pdev->devfn));
	if (port == -1)
		return 0;

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	ret = pci_request_region(pdev, 0, "hsu");
	if (ret)
		goto err;

	up = serial_hsu_port_setup(&pdev->dev, port, start, len,
			pdev->irq);
	if (IS_ERR(up))
		goto err;

	pci_set_drvdata(pdev, up);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_allow(&pdev->dev);
	return 0;
err:
	pci_disable_device(pdev);
	return ret;
}

static void serial_hsu_pci_port_remove(struct pci_dev *pdev)
{
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	pm_runtime_forbid(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	serial_hsu_port_free(up);
	pci_set_drvdata(pdev, NULL);
	pci_disable_device(pdev);
}

static void serial_hsu_pci_port_shutdown(struct pci_dev *pdev)
{
	struct uart_hsu_port *up = pci_get_drvdata(pdev);

	if (!up)
		return;

	serial_hsu_port_shutdown(up);
}

static struct pci_driver hsu_port_pci_driver = {
	.name =		"HSU serial",
	.id_table =	hsuart_port_pci_ids,
	.probe =	serial_hsu_pci_port_probe,
	.remove =	serial_hsu_pci_port_remove,
	.shutdown =	serial_hsu_pci_port_shutdown,
/* Disable PM only when kgdb(poll mode uart) is enabled */
#if defined(CONFIG_PM) && !defined(CONFIG_CONSOLE_POLL)
	.driver = {
		.pm = &serial_hsu_pci_pm_ops,
	},
#endif
};

static int serial_hsu_pci_dma_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	int ret, share_irq = 0;
	resource_size_t start, len;

	start = pci_resource_start(pdev, 0);
	len = pci_resource_len(pdev, 0);

	dev_info(&pdev->dev,
		"FUNC: %d driver: %ld addr:%lx len:%lx\n",
		PCI_FUNC(pdev->devfn), ent->driver_data,
		(ulong) pci_resource_start(pdev, 0),
		(ulong) pci_resource_len(pdev, 0));

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	ret = pci_request_region(pdev, 0, "hsu dma");
	if (ret)
		goto err;

	/* share irq with port? ANN all and TNG chip from B0 stepping */
	if ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER &&
		pdev->revision >= 0x1) ||
		intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE)
		share_irq = 1;

	ret = serial_hsu_dma_setup(&pdev->dev, start, len, pdev->irq, share_irq);
	if (ret)
		goto err;

	return 0;
err:
	pci_disable_device(pdev);
	return ret;
}

static void serial_hsu_pci_dma_remove(struct pci_dev *pdev)
{
	serial_hsu_dma_free();
	pci_disable_device(pdev);
	pci_unregister_driver(&hsu_port_pci_driver);
}

static struct pci_driver hsu_dma_pci_driver = {
	.name =		"HSU DMA",
	.id_table =	hsuart_dma_pci_ids,
	.probe =	serial_hsu_pci_dma_probe,
	.remove =	serial_hsu_pci_dma_remove,
};

static int __init hsu_pci_init(void)
{
	int ret;

	ret = pci_register_driver(&hsu_dma_pci_driver);
	if (!ret) {
		ret = pci_register_driver(&hsu_port_pci_driver);
		if (ret)
			pci_unregister_driver(&hsu_dma_pci_driver);
	}

	return ret;
}

static void __exit hsu_pci_exit(void)
{
	pci_unregister_driver(&hsu_port_pci_driver);
	pci_unregister_driver(&hsu_dma_pci_driver);
}

module_init(hsu_pci_init);
module_exit(hsu_pci_exit);

MODULE_AUTHOR("Yang Bin <bin.yang@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:medfield-hsu");
