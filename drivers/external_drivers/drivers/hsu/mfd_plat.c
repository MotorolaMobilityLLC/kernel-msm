/*
 * mfd_plat.c: driver for High Speed UART device of Intel Medfield platform
 *
 * (C) Copyright 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/intel_mid_pm.h>
#include <linux/pm_qos.h>
#include <linux/pci.h>

#include "mfd.h"

#ifdef CONFIG_ACPI
static const struct acpi_device_id hsu_acpi_ids[] = {
	{ "80860F0A", hsu_vlv2 },
	{ "8086228A", hsu_chv },
	{ }
};
MODULE_DEVICE_TABLE(acpi, hsu_acpi_ids);
#endif

#ifdef CONFIG_PM
static int serial_hsu_plat_suspend(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	int ret = 0;

	if (up) {
		trace_hsu_func_start(up->index, __func__);
		ret = serial_hsu_do_suspend(up);
		trace_hsu_func_end(up->index, __func__, "");
	}
	return ret;
}

static int serial_hsu_plat_resume(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	int ret = 0;

	if (up) {
		trace_hsu_func_start(up->index, __func__);
		ret = serial_hsu_do_resume(up);
		trace_hsu_func_end(up->index, __func__, "");
	}
	return ret;
}
#else
#define serial_hsu_plat_suspend	NULL
#define serial_hsu_plat_resume	NULL
#endif

#ifdef CONFIG_PM_RUNTIME
static int serial_hsu_plat_runtime_idle(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);

	return serial_hsu_do_runtime_idle(up);
}

static int serial_hsu_plat_runtime_suspend(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	int ret = 0;

	trace_hsu_func_start(up->index, __func__);
	ret = serial_hsu_do_suspend(up);
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}

static int serial_hsu_plat_runtime_resume(struct device *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	int ret = 0;

	trace_hsu_func_start(up->index, __func__);
	ret = serial_hsu_do_resume(up);
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}
#else
#define serial_hsu_plat_runtime_idle		NULL
#define serial_hsu_plat_runtime_suspend	NULL
#define serial_hsu_plat_runtime_resume	NULL
#endif

static const struct dev_pm_ops serial_hsu_plat_pm_ops = {

	SET_SYSTEM_SLEEP_PM_OPS(serial_hsu_plat_suspend,
				serial_hsu_plat_resume)
	SET_RUNTIME_PM_OPS(serial_hsu_plat_runtime_suspend,
				serial_hsu_plat_runtime_resume,
				serial_hsu_plat_runtime_idle)
};

static int serial_hsu_plat_port_probe(struct platform_device *pdev)
{
	struct uart_hsu_port *up;
	int port = pdev->id, irq;
	struct resource *mem, *ioarea;
	resource_size_t start, len;

#ifdef CONFIG_ACPI
	const struct acpi_device_id *id;
	for (id = hsu_acpi_ids; id->id[0]; id++)
		if (!strncmp(id->id, dev_name(&pdev->dev), strlen(id->id))) {
			acpi_status status;
			unsigned long long tmp;

			status = acpi_evaluate_integer(ACPI_HANDLE(&pdev->dev),
					"_UID", NULL, &tmp);
			if (ACPI_FAILURE(status))
				return -ENODEV;
			port = tmp - 1;
			if (intel_mid_hsu_plat_init(port,
				id->driver_data, &pdev->dev))
				return -ENODEV;
		}
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -EINVAL;
	}
	start = mem->start;
	len = resource_size(mem);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq; /* -ENXIO */
	}

	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "HSU region already claimed\n");
		return -EBUSY;
	}

	up = serial_hsu_port_setup(&pdev->dev, port, start, len,
			irq);
	if (IS_ERR(up)) {
		release_mem_region(mem->start, resource_size(mem));
		dev_err(&pdev->dev, "failed to setup HSU\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, up);

	if (!pdev->dev.dma_mask) {
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	}
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	return 0;
}

static int serial_hsu_plat_port_remove(struct platform_device *pdev)
{
	struct uart_hsu_port *up = platform_get_drvdata(pdev);
	struct resource *mem;

	pm_runtime_forbid(&pdev->dev);
	serial_hsu_port_free(up);
	platform_set_drvdata(pdev, NULL);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem)
		release_mem_region(mem->start, resource_size(mem));

	return 0;
}

static void serial_hsu_plat_port_shutdown(struct platform_device *pdev)
{
	struct uart_hsu_port *up = platform_get_drvdata(pdev);

	if (!up)
		return;

	serial_hsu_port_shutdown(up);
}

static struct platform_driver hsu_plat_driver = {
	.remove		= serial_hsu_plat_port_remove,
	.shutdown 	= serial_hsu_plat_port_shutdown,
	.driver		= {
		.name	= "HSU serial",
		.owner	= THIS_MODULE,
/* Disable PM only when kgdb(poll mode uart) is enabled */
#if defined(CONFIG_PM) && !defined(CONFIG_CONSOLE_POLL)
		.pm     = &serial_hsu_plat_pm_ops,
#endif
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(hsu_acpi_ids),
#endif
	},
};

static int __init hsu_plat_init(void)
{
	struct pci_dev *hsu_pci;

	/*
	 * Try to get pci device, if exist, then exit ACPI platform
	 * register, On BYT FDK, include two enum mode: PCI, ACPI,
	 * ignore ACPI enum mode.
	 */
	hsu_pci = pci_get_device(PCI_VENDOR_ID_INTEL, 0x0F0A, NULL);
	if (hsu_pci) {
		pr_info("HSU serial: Find HSU controller in PCI device, "
			"exit ACPI platform register!\n");
		return 0;
	}

	return platform_driver_probe(&hsu_plat_driver, serial_hsu_plat_port_probe);
}

static void __exit hsu_plat_exit(void)
{
	platform_driver_unregister(&hsu_plat_driver);
}

module_init(hsu_plat_init);
module_exit(hsu_plat_exit);

MODULE_AUTHOR("Jason Chen <jason.cj.chen@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:medfield-hsu-plat");
