/*
 * rawio_pci.c - a driver to read/write pci configuration space registers based
 *               on the rawio framework.
 *
 * 1: byte, 2: word, 4: dword
 *
 * read pci config space registers
 * echo "r[1|2|4] pci <domain> <bus> <dev> <func> <reg> [<len>]" >
 *				/sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "r1 pci 0 0 3 0 8 12" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 *
 * write a pci config space register:
 * echo "w[1|2|4] pci <domain> <bus> <dev> <func> <reg> <val>" >
 *				/sys/kernel/debug/rawio_output
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "w pci 0 0 0x11 2 0x10 0xffffffff" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 *
 *
 * Copyright (c) 2013 Bin Gao <bin.gao@intel.com>
 *
 * This file is released under the GPLv2
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pm_runtime.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include "rawio.h"

static int pci_prepare(int pci_domain, unsigned int pci_bus,
		u8 pci_dev, u8 pci_func, enum width width,
		u16 pci_reg, u16 len, struct pci_dev **ppdev)
{
	struct pci_dev *pdev;
	int ret;

	if (((width == WIDTH_2) && (pci_reg & 0x1)) ||
		((width == WIDTH_4) && (pci_reg & 0x3))) {
		rawio_err("register address requires 2 bytes aligned for 16 bit access, and 4 bytes aligned for 32 bit access\n");
		return -EINVAL;
	}

	pdev = pci_get_domain_bus_and_slot(pci_domain, pci_bus,
				PCI_DEVFN(pci_dev, pci_func));
	if (!pdev) {
		rawio_err("pci device %04x:%02x:%02x.%01x doesn't exist\n",
				pci_domain, pci_bus, pci_dev, pci_func);
		return -ENODEV;
	}

	if (((pci_reg >= 0x100) && !pci_is_pcie(pdev)) ||
				(pci_reg >= 0x1000)) {
		rawio_err("register address is out of range\n");
		return -EINVAL;
	}

	if ((((pci_reg + len * width) >= 0x100) && !pci_is_pcie(pdev)) ||
				((pci_reg + len * width) >= 0x1000)) {
		rawio_err("register address is out of range\n");
		return -EINVAL;
	}

	ret = pm_runtime_get_sync(&pdev->dev);
	if ((ret >= 0) || (ret == -EACCES))
		goto out;

	rawio_err("can't put pci device %04x:%02x:%02x.%01xinto running state, pm_runtime_get_sync() returned %d\n",
			pci_domain, pci_bus, pci_dev, pci_func, ret);
	return -EBUSY;

out:
	*ppdev = pdev;
	return 0;
}

static void pci_finish(struct pci_dev *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
}

static int rawio_pci_read(struct rawio_driver *driver, int width,
			u64 *input, u8 *postfix, int input_num,
			void **output, int *output_num)
{
	int i, ret, pci_domain;
	struct pci_dev *pdev;
	unsigned int pci_bus;
	u8 pci_dev, pci_func;
	u16 pci_reg, len;
	void *buf;

	pci_domain = (int)input[0];
	pci_bus = (unsigned int)input[1];
	pci_dev = (u8)input[2];
	pci_func = (u8)input[3];
	pci_reg = (u16)input[4];
	len = 1;
	if (input_num == 6)
		len = (u16)input[5];

	ret = pci_prepare(pci_domain, pci_bus, pci_dev, pci_func,
				width, pci_reg, len, &pdev);
	if (ret)
		return ret;

	buf = kzalloc(width * len, GFP_KERNEL);
	if (buf == NULL) {
		rawio_err("can't alloc memory\n");
		pci_finish(pdev);
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		switch (width) {
		case WIDTH_1:
			pci_read_config_byte(pdev, pci_reg + i, (u8 *)buf + i);
			break;
		case WIDTH_2:
			pci_read_config_word(pdev, pci_reg + i * 2,
						(u16 *)buf + i);
			break;
		case WIDTH_4:
			pci_read_config_dword(pdev, pci_reg + i * 4,
						(u32 *)buf + i);
			break;
		default:
			break;
		}
	}

	pci_finish(pdev);
	*output = buf;
	*output_num = len;
	return 0;
}

static int rawio_pci_write(struct rawio_driver *driver, int width,
			u64 *input, u8 *postfix, int input_num)
{
	int ret, pci_domain;
	struct pci_dev *pdev;
	unsigned int pci_bus;
	u8 pci_dev, pci_func;
	u16 pci_reg;
	u32 value;

	pci_domain = (int)input[0];
	pci_bus = (unsigned int)input[1];
	pci_dev = (u8)input[2];
	pci_func = (u8)input[3];
	pci_reg = (u16)input[4];
	value = (u32) input[5];

	ret = pci_prepare(pci_domain, pci_bus, pci_dev, pci_func,
					width, pci_reg, 1, &pdev);
	if (ret)
		return ret;

	switch (width) {
	case WIDTH_1:
		pci_write_config_byte(pdev, pci_reg, (u8) value);
		break;
	case WIDTH_2:
		pci_write_config_word(pdev, pci_reg, (u16) value);
		break;
	case WIDTH_4:
		pci_write_config_dword(pdev, pci_reg, value);
		break;
	default:
		break;
	}

	pci_finish(pdev);
	return 0;
}

static struct rawio_ops rawio_pci_ops = {
	rawio_pci_read,
	NULL,
	rawio_pci_write,
};

static struct rawio_driver rawio_pci = {
	{NULL, NULL},
	"pci",

	/* read */
	6, /* max args */
	{TYPE_S32, TYPE_U32, TYPE_U8, TYPE_U8, TYPE_U16, TYPE_S16}, /* types */
	5, /* min args */

	{ 0, }, /* postfix */

	/* write */
	6, /* max args */
	{TYPE_S32, TYPE_U32, TYPE_U8, TYPE_U8, TYPE_U16, TYPE_U32}, /* types */
	6, /* min args */
	{ 0, },

	4, /* index of address arg */

	WIDTH_1 | WIDTH_2 | WIDTH_4, /* supported width */
	WIDTH_4, /* default width */
	"r[1|2|4] pci <domain> <bus> <dev> <func> <reg> [<len>]\n"
	"w[1|2|4] pci <domain> <bus> <dev> <func> <reg> <val>\n",
	&rawio_pci_ops,
	NULL
};

static int __init rawio_pci_init(void)
{
	if (rawio_register_driver(&rawio_pci))
		return -ENODEV;

	return 0;
}
module_init(rawio_pci_init);

static void __exit rawio_pci_exit(void)
{
	rawio_unregister_driver(&rawio_pci);
}
module_exit(rawio_pci_exit);

MODULE_DESCRIPTION("Rawio PCI driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Bin Gao <bin.gao@intel.com>");
MODULE_LICENSE("GPL v2");
