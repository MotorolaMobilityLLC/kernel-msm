#include "hdcp.h"

static char *option = "option";

module_param(option, charp, 0000);
MODULE_PARM_DESC(option, "option");

int my_init_module(void)
{
	if (strcmp(option, "option") == 0) {
		printk(KERN_INFO "hdcp:m:: Err! usage\n");
		printk(KERN_INFO "hdcp:m:: insmod ./hdcp.ko option=enable\n");
		printk(KERN_INFO "hdcp:m:: insmod ./hdcp.ko option=disable\n");
		printk(KERN_INFO "hdcp:m:: insmod ./hdcp.ko option=mismatch\n");
	} else {
		if (strcmp(option, "mismatch") == 0) {
			printk(KERN_INFO "[hdcp:m]: force Ri mismatch\n");
			module_force_ri_mismatch = true;
		} else if (strcmp(option, "enable") == 0) {
			printk(KERN_INFO "[hdcp:m]: hdcp enable\n");
			module_disable_hdcp = false;
		} else if (strcmp(option, "disable") == 0) {
			printk(KERN_INFO "[hdcp:m]: hdcp disable\n");
			module_disable_hdcp = true;
		} else {
			printk(KERN_INFO "[hdcp:m]: unsupported option %s\n",
				option);
		}
	}
	return 0;
}

void my_cleanup_module(void)
{
	/* Nothing to Do */
}

module_init(my_init_module);
module_exit(my_cleanup_module);

MODULE_LICENSE("GPL");
