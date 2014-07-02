/*
 * rawio_iomem.c - a driver to read or write a device's I/O memory, based on
 *                 the rawio debugfs framework.
 *
 * Copyright (c) 2013 Bin Gao <bin.gao@intel.com>
 *
 * This file is released under the GPLv2
 *
 *
 * 1: byte, 2: word, 4: dword
 *
 * I/O mem read:
 * echo "r[1|2|4] iomem <physical_addr> [<len>]" > /sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "r iomem 0xff003040 20" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 *
 * I/O mem write:
 * echo "w[1|2|4] iomem <physical_addr> <val>" > /sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "w2 iomem 0xff003042 0xb03f" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/pci.h>
#include <linux/pnp.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include "rawio.h"

/*
 * On some platforms, a read or write to a device which is in a low power
 * state will cause a system error, or even cause a system reboot.
 * To address this, we use runtime PM APIs to bring up the device to
 * runnint state before use and put back to original state after use.
 *
 * We could use lookup_resource() to map an physical address to a device,
 * but there are some problems:
 * 1) lookup_resource() is not exported, so a kernel module can't use it;
 * 2) To use the 'name' field of 'struct resource' to match a device is
 *    not reliable;
 * So we rather walk known device types than look up the resrouce list
 * to map a physical address(I/O memory address) to a device.
 */

/* return true if range(start: m2, size: n2) is in range(start: m1, size: n1) */
#define IN_RANGE(m1, n1, m2, n2) \
	(((m2 >= m1) && (m2 < (m1 + n1))) && \
	(((m2 + n2) >= m1) && ((m2 + n2) < (m1 + n1))))

struct dev_walker {
	resource_size_t addr;
	resource_size_t size;
	struct device *dev;
	int error;
};

#ifdef CONFIG_PCI
int walk_pci_devices(struct device *dev, void *data)
{
	int i;
	resource_size_t start, len;
	struct pci_dev *pdev = (struct pci_dev *) to_pci_dev(dev);
	struct dev_walker *walker = (struct dev_walker *) data;

	if (!pdev)
		return -ENODEV;

	walker->dev = NULL;
	for (i = 0; i < 6; i++) {
		start = pci_resource_start(pdev, i);
		len = pci_resource_len(pdev, i);
		if (IN_RANGE(start, len, walker->addr, walker->size)) {
			walker->dev = dev;
			return 1;
		}
	}

	return 0;
}
#endif

int walk_platform_devices(struct device *dev, void *data)
{
	int i;
	struct resource *r;
	resource_size_t start, len;
	struct platform_device *plat_dev = to_platform_device(dev);
	struct dev_walker *walker = (struct dev_walker *) data;

	walker->dev = NULL;
	for (i = 0; i < plat_dev->num_resources; i++) {
		r = platform_get_resource(plat_dev, IORESOURCE_MEM, i);
		if (!r)
			continue;
		start = r->start;
		len = r->end - r->start + 1;
		if (IN_RANGE(start, len, walker->addr, walker->size)) {
			walker->dev = dev;
			return 1;
		}
	}

	return 0;
}

#if defined(CONFIG_PNP) && !defined(MODULE)
int walk_pnp_devices(struct device *dev, void *data)
{
	int i;
	struct resource *r;
	resource_size_t start, len;
	struct pnp_dev *pnp_dev = (struct pnp_dev *) to_pnp_dev(dev);
	struct dev_walker *walker = (struct dev_walker *) data;

	walker->dev = NULL;
	for (i = 0; (r = pnp_get_resource(pnp_dev, IORESOURCE_MEM, i)); i++) {
		if (!pnp_resource_valid(r))
			continue;

		start = r->start;
		len = r->end - r->start + 1;
		if (IN_RANGE(start, len, walker->addr, walker->size)) {
			walker->dev = dev;
			return 1;
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_ACPI
static acpi_status do_walk_acpi_device(acpi_handle handle, u32 nesting_level,
				void *context, void **ret)
{
	struct dev_walker *walker = (struct dev_walker *) context;
	struct acpi_device *adev = NULL;
	resource_size_t start, len;
	struct resource_list_entry *rentry;
	struct list_head resource_list;
	struct resource *resources;
	int i, count;

	acpi_bus_get_device(handle, &adev);
	if (!adev)
		return AE_CTRL_DEPTH;

	/*
	 * Simply skip this acpi device if it's already attached to
	 * other bus types(e.g. platform dev or a pnp dev).
	 */
	if (adev->physical_node_count)
		return 0;

	INIT_LIST_HEAD(&resource_list);
	count = acpi_dev_get_resources(adev, &resource_list, NULL, NULL);
	if (count <= 0)
		return AE_CTRL_DEPTH;

	resources = kmalloc(count * sizeof(struct resource), GFP_KERNEL);
	if (!resources) {
		pr_err("rawio: No memory for resources\n");
		acpi_dev_free_resource_list(&resource_list);
		walker->error = -ENOMEM;
		return AE_CTRL_TERMINATE;
	}

	count = 0;
	list_for_each_entry(rentry, &resource_list, node)
		resources[count++] = rentry->res;

	acpi_dev_free_resource_list(&resource_list);

	for (i = 0; i < count; i++) {
		start = resources[i].start;
		len = resources[i].end - resources[i].start + 1;
		if (IN_RANGE(start, len, walker->addr, walker->size)) {
			walker->dev = &adev->dev;
			kfree(resources);
			return AE_CTRL_TERMINATE;
		}
	}

	kfree(resources);
	return AE_CTRL_DEPTH;
}
#endif

static struct device *walk_devices(resource_size_t addr, resource_size_t size)
{
	int ret;
	struct dev_walker walker;

	walker.addr = addr;
	walker.size = size;
	walker.dev = NULL;
	walker.error = 0;

#ifdef CONFIG_PCI
	ret = bus_for_each_dev(&pci_bus_type, NULL, (void *)&walker,
						walk_pci_devices);
	if (ret == 1)
		return  walker.dev;
#endif

	ret = bus_for_each_dev(&platform_bus_type, NULL, (void *)&walker,
						walk_platform_devices);
	if (ret == 1)
		return walker.dev;

#if defined(CONFIG_PNP) && !defined(MODULE)
	ret = bus_for_each_dev(&pnp_bus_type, NULL, (void *)&walker,
						walk_pnp_devices);
	if (ret == 1)
		return walker.dev;
#endif

#ifdef CONFIG_ACPI
	acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
			NULL, do_walk_acpi_device, &walker, NULL);

	if (walker.error)
		return NULL;

	if (walker.dev)
		return walker.dev;
#endif

	return NULL;
}


static int rawio_iomem_read(struct rawio_driver *driver, int width,
			u64 *input, u8 *postfix, int input_num,
			void **output, int *output_num)
{
	int i, size, count;
	phys_addr_t addr;
	void *buf;
	void __iomem *va;
	struct device *dev = NULL;

	addr = (phys_addr_t)input[0];
	count = 1;
	if (input_num == 2)
		count = (int)input[1];
	size = width * count;

	if (((width == WIDTH_2) && (addr & 0x1)) ||
		((width == WIDTH_4) && (addr & 0x3)) ||
		((width == WIDTH_8) && (addr & 0x7))) {
		rawio_err("address requires 2 bytes aligned for 16 bit access, 4 bytes aligned for 32 bit access and 8 bytes aligned for 64 bit access\n");
		return -EINVAL;
	}

	va = ioremap_nocache(addr, size);
	if (!va) {
		rawio_err("can't map physical address %llx\n", addr);
		return -EIO;
	}

	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL) {
		rawio_err("can't alloc memory\n");
		iounmap(va);
		return -ENOMEM;
	}

	dev = walk_devices(addr, size);
	if (dev)
		pm_runtime_get_sync(dev);

	for (i = 0; i < count; i++) {
		switch (width) {
		case WIDTH_1:
			*((u8 *)buf + i) = ioread8(va + i);
			break;
		case WIDTH_2:
			*((u16 *)buf + i) = ioread16(va + i * 2);
			break;
		case WIDTH_4:
			*((u32 *)buf + i) = ioread32(va + i * 4);
			break;
		default:
			break;
		}
	}

	if (dev)
		pm_runtime_put_sync(dev);
	iounmap(va);
	*output = buf;
	*output_num = count;
	return 0;
}

static int rawio_iomem_write(struct rawio_driver *driver, int width,
			u64 *input, u8 *postfix, int input_num)
{
	unsigned long val;
	phys_addr_t addr;
	void __iomem *va;
	struct device *dev;

	addr = (phys_addr_t)input[0];
	val = (u64)input[1];

	if (((width == WIDTH_2) && (addr & 0x1)) ||
		((width == WIDTH_4) && (addr & 0x3)) ||
		((width == WIDTH_8) && (addr & 0x7))) {
		rawio_err("address requires 2 bytes aligned for 16 bit access, 4 bytes aligned for 32 bit access and 8 bytes aligned for 64 bit access\n");
		return -EINVAL;
	}

	va = ioremap_nocache(addr, width);
	if (!va) {
		rawio_err("can't map physical address %llx\n", addr);
		return -EIO;
	}

	dev = walk_devices(addr, 0);
	if (dev)
		pm_runtime_get_sync(dev);

	switch (width) {
	case WIDTH_1:
		 *(u8 *)va = (u8)val;
		break;
	case WIDTH_2:
		 *(u16 *)va = (u16)val;
		break;
	case WIDTH_4:
		 *(u32 *)va = (u32)val;
		break;
	default:
		break;
	}

	if (dev)
		pm_runtime_put_sync(dev);
	iounmap(va);
	return 0;
}

static struct rawio_ops rawio_iomem_ops = {
	rawio_iomem_read,
	NULL,
	rawio_iomem_write,
};

static struct rawio_driver rawio_iomem = {
	{NULL, NULL},
	"iomem",

	/* read */
	2, /* max args */
	{TYPE_U64, TYPE_S16}, /* args type */
	1, /* min args */
	{ 0, 0, }, /* args postfix */

	/* write */
	2, /* max args */
	{TYPE_U64, TYPE_U64},
	2, /* min args */
	{ 0, 0, },

	0, /* index to address arg */

	WIDTH_1 | WIDTH_2 | WIDTH_4, /* supported access width*/
	WIDTH_4, /* default access width */
	"r[1|2|4] iomem <addr> [<len>]\nw[1|2|4] iomem <addr> <val>\n",
	&rawio_iomem_ops,
	NULL
};

static int __init rawio_iomem_init(void)
{
	return rawio_register_driver(&rawio_iomem);
}
module_init(rawio_iomem_init);

static void __exit rawio_iomem_exit(void)
{
	rawio_unregister_driver(&rawio_iomem);
}
module_exit(rawio_iomem_exit);

MODULE_DESCRIPTION("Rawio I/O memory driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Bin Gao <bin.gao@intel.com>");
MODULE_LICENSE("GPL v2");
