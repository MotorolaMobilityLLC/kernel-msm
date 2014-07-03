/* MSIC GPIO (access through IPC) driver for Cloverview
 * (C) Copyright 2011 Intel Corporation
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/rpmsg.h>
#include <linux/mfd/intel_msic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_scu_pmic.h>

#define DRIVER_NAME "msic_gpio"

#define CTLO_DOUT_MASK		(1 << 0)
#define CTLO_DOUT_H		(1 << 0)
#define CTLO_DOUT_L		(0 << 0)
#define CTLO_DIR_MASK		(1 << 5)
#define CTLO_DIR_O		(1 << 5)
#define CTLO_DIR_I		(0 << 5)
#define CTLO_OUT_DEF		(0x38)
#define CTLO_IN_DEF		(0x18)

#define CTL_VALUE_MASK		(1 << 0)

struct msic_gpio {
	struct gpio_chip chip;
	int ngpio_lv; /* number of low voltage gpio */
	u16 gpio0_lv_ctlo;
	u16 gpio0_lv_ctli;
	u16 gpio0_hv_ctlo;
	u16 gpio0_hv_ctli;
};

static struct msic_gpio msic_gpio;

static int msic_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct msic_gpio *mg = &msic_gpio;

	u16 ctlo = offset < mg->ngpio_lv ? mg->gpio0_lv_ctlo + offset
			: mg->gpio0_hv_ctlo + (offset - mg->ngpio_lv);

	return intel_scu_ipc_iowrite8(ctlo, CTLO_IN_DEF);
}

static int msic_gpio_direction_output(struct gpio_chip *chip,
			unsigned offset, int value)
{
	struct msic_gpio *mg = &msic_gpio;

	u16 ctlo = offset < mg->ngpio_lv ? mg->gpio0_lv_ctlo + offset
			: mg->gpio0_hv_ctlo + (offset - mg->ngpio_lv);

	return intel_scu_ipc_iowrite8(ctlo,
			CTLO_OUT_DEF | (value ? CTLO_DOUT_H : CTLO_DOUT_L));
}

static int msic_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct msic_gpio *mg = &msic_gpio;
	u8 value;
	int ret;
	u16 ctlo, ctli, reg;

	ctlo = offset < mg->ngpio_lv ? mg->gpio0_lv_ctlo + offset
			: mg->gpio0_hv_ctlo + (offset - mg->ngpio_lv);
	ctli = offset < mg->ngpio_lv ? mg->gpio0_lv_ctli + offset
			: mg->gpio0_hv_ctli + (offset - mg->ngpio_lv);

	/* First get pin direction */
	ret = intel_scu_ipc_ioread8(ctlo, &value);
	if (ret)
		return -EIO;

	/* The pin values for output and input direction
	 * are stored in different registers.
	 */
	reg = (value & CTLO_DIR_O) ? ctlo : ctli;

	ret = intel_scu_ipc_ioread8(reg, &value);
	if (ret)
		return -EIO;

	return value & CTL_VALUE_MASK;
}

static void msic_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct msic_gpio *mg = &msic_gpio;

	u16 ctlo = offset < mg->ngpio_lv ? mg->gpio0_lv_ctlo + offset
			: mg->gpio0_hv_ctlo + (offset - mg->ngpio_lv);

	intel_scu_ipc_update_register(ctlo,
			value ? CTLO_DOUT_H : CTLO_DOUT_L, CTLO_DOUT_MASK);
}

static int msic_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct intel_msic_gpio_pdata *pdata = dev->platform_data;
	struct msic_gpio *mg = &msic_gpio;
	int retval;

	dev_dbg(dev, "base %d\n", pdata->gpio_base);

	if (!pdata || !pdata->gpio_base) {
		dev_err(dev, "incorrect or missing platform data\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, mg);

	mg->ngpio_lv = pdata->ngpio_lv;
	mg->gpio0_lv_ctlo = pdata->gpio0_lv_ctlo;
	mg->gpio0_lv_ctli = pdata->gpio0_lv_ctli;
	mg->gpio0_hv_ctlo = pdata->gpio0_hv_ctlo;
	mg->gpio0_hv_ctli = pdata->gpio0_hv_ctli;
	mg->chip.label = dev_name(&pdev->dev);
	mg->chip.direction_input = msic_gpio_direction_input;
	mg->chip.direction_output = msic_gpio_direction_output;
	mg->chip.get = msic_gpio_get;
	mg->chip.set = msic_gpio_set;
	mg->chip.base = pdata->gpio_base;
	mg->chip.ngpio = pdata->ngpio_lv + pdata->ngpio_hv;
	mg->chip.can_sleep = pdata->can_sleep;
	mg->chip.dev = dev;

	retval = gpiochip_add(&mg->chip);
	if (retval)
		dev_err(dev, "%s: Can not add msic gpio chip.\n", __func__);

	return retval;
}

static int msic_gpio_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_set_drvdata(dev, NULL);
	return 0;
}

static struct platform_driver msic_gpio_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
	},
	.probe		= msic_gpio_probe,
	.remove		= msic_gpio_remove,
};

static int msic_gpio_init(void)
{
	return platform_driver_register(&msic_gpio_driver);
}

static void msic_gpio_exit(void)
{
	return platform_driver_unregister(&msic_gpio_driver);
}

static int msic_gpio_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("msic_gpio rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed msic_gpio rpmsg device\n");

	ret = msic_gpio_init();

out:
	return ret;
}

static void msic_gpio_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	msic_gpio_exit();
	dev_info(&rpdev->dev, "Removed msic_gpio rpmsg device\n");
}

static void msic_gpio_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id msic_gpio_rpmsg_id_table[] = {
	{ .name	= "rpmsg_msic_gpio" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, msic_gpio_rpmsg_id_table);

static struct rpmsg_driver msic_gpio_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= msic_gpio_rpmsg_id_table,
	.probe		= msic_gpio_rpmsg_probe,
	.callback	= msic_gpio_rpmsg_cb,
	.remove		= msic_gpio_rpmsg_remove,
};

static int __init msic_gpio_rpmsg_init(void)
{
	return register_rpmsg_driver(&msic_gpio_rpmsg);
}
fs_initcall(msic_gpio_rpmsg_init);

static void __exit msic_gpio_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&msic_gpio_rpmsg);
}
module_exit(msic_gpio_rpmsg_exit);

MODULE_AUTHOR("Bin Yang <bin.yang@intel.com>");
MODULE_DESCRIPTION("Intel MSIC GPIO driver");
MODULE_LICENSE("GPL v2");
