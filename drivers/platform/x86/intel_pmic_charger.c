/* Cloverview Plus PMIC Charger (USBDET interrupt) driver
 * Copyright (C) 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/rpmsg.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <linux/platform_data/intel_mid_remoteproc.h>
#include <linux/reboot.h>

/* Wake lock to prevent platform from going to
	S3 when USBDET interrupt Trigger */
static struct wake_lock wakelock;

#define DRIVER_NAME "pmic_charger"
#define MSIC_SPWRSRCINT 0x192
#define MSIC_SUSBDET_MASK_BIT	0x2
#define MSIC_SBATTDET_MASK_BIT  0x1

static irqreturn_t pmic_charger_thread_handler(int irq, void *devid)
{
	int retval = 0;
	uint8_t spwrsrcint;
	struct device *dev = (struct device *)devid;

	retval = intel_scu_ipc_ioread8(MSIC_SPWRSRCINT, &spwrsrcint);
	if (retval) {
		dev_err(dev, "IPC Failed to read %d\n", retval);
		return retval;
	}

	if ((spwrsrcint & MSIC_SUSBDET_MASK_BIT) == 0) {
		if (wake_lock_active(&wakelock))
			wake_unlock(&wakelock);
	}

	/* Shutdown upon battery removal */
	if ((spwrsrcint & MSIC_SBATTDET_MASK_BIT) == 0) {
		dev_info(dev, "battery removal shutdown\n");
		kernel_power_off();
	}

	dev_info(dev, "pmic charger interrupt: %d\n", irq);

	return IRQ_HANDLED;
}

static irqreturn_t pmic_charger_irq_handler(int irq, void *devid)
{
	if (!wake_lock_active(&wakelock))
		wake_lock(&wakelock);

	return IRQ_WAKE_THREAD;
}

static int pmic_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int irq = platform_get_irq(pdev, 0);
	int ret;

	/* Initialize the wakelock */
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "pmic_charger_wakelock");

	/* Register a handler for USBDET interrupt */
	ret = request_threaded_irq(irq, pmic_charger_irq_handler,
			pmic_charger_thread_handler,
			IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
			"pmic_usbdet_interrupt", dev);
	if (ret) {
		dev_info(dev, "register USBDET IRQ with error %d\n", ret);
		return ret;
	}

	dev_info(dev, "registered USBDET IRQ %d\n", irq);

	return 0;
}

static int pmic_charger_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int irq = platform_get_irq(pdev, 0);
	wake_lock_destroy(&wakelock);
	free_irq(irq, dev);

	return 0;
}

#ifdef CONFIG_PM
static int pmic_charger_suspend(struct device *dev)
{
	dev_dbg(dev, "pmic charger suspend\n");
	return 0;
}

static int pmic_charger_resume(struct device *dev)
{
	dev_dbg(dev, "pmic charger resume\n");
	return 0;
}
#endif

static const struct dev_pm_ops pmic_charger_pm_ops = {
	.suspend                = pmic_charger_suspend,
	.resume                 = pmic_charger_resume,
};

static struct platform_driver pmic_charger_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.pm		= &pmic_charger_pm_ops,
	},
	.probe		= pmic_charger_probe,
	.remove = pmic_charger_remove,
};

static int __init pmic_charger_module_init(void)
{
	return platform_driver_register(&pmic_charger_driver);
}

static void pmic_charger_module_exit(void)
{
	platform_driver_unregister(&pmic_charger_driver);
}

/* RPMSG related functionality */
static int pmic_charger_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed pmic_charger rpmsg device\n");

	ret = pmic_charger_module_init();

out:
	return ret;
}

static void pmic_charger_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	pmic_charger_module_exit();
	dev_info(&rpdev->dev, "Removed pmic_charger rpmsg device\n");
}

static void pmic_charger_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id pmic_charger_rpmsg_id_table[] = {
	{ .name	= "rpmsg_pmic_charger" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, pmic_charger_rpmsg_id_table);

static struct rpmsg_driver pmic_charger_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= pmic_charger_rpmsg_id_table,
	.probe		= pmic_charger_rpmsg_probe,
	.callback	= pmic_charger_rpmsg_cb,
	.remove		= pmic_charger_rpmsg_remove,
};

static int __init pmic_charger_rpmsg_init(void)
{
	return register_rpmsg_driver(&pmic_charger_rpmsg);
}

static void __exit pmic_charger_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&pmic_charger_rpmsg);
}

module_init(pmic_charger_rpmsg_init);
module_exit(pmic_charger_rpmsg_exit);

MODULE_AUTHOR("Dongsheng Zhang <dongsheng.zhang@intel.com>");
MODULE_AUTHOR("Rapaka, Naveen <naveen.rapaka@intel.com>");
MODULE_DESCRIPTION("Intel Pmic charger Driver");
MODULE_LICENSE("GPL v2");
