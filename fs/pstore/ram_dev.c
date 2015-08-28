#include <linux/pstore_ram.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

static struct ramoops_platform_data ramoops_data = {
	.mem_size			= 0x00200000,
	.mem_address		= 0x11F00000,
	.record_size		= 0x00010000,
	.console_size		= 0x00010000,
	.ftrace_size		= 0x00001000,
	.dump_oops			= 1,
	//.ecc				= 1,
};

static struct platform_device ramoops_dev = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ramoops_data,
	},
};


static int __init ramoops_dev_init(void)
{
	int ret;

	ret = platform_device_register(&ramoops_dev);

	if (ret)
		printk("JASON:unable to register platform device ramoops_dev\n");
	else
		printk("JASON:ramoops_dev_init success\n");

	return ret;
}

module_init(ramoops_dev_init);
