#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>


#include <linux/kernel.h>
#include "edid_print.h"

static int log_level = -1;

module_param(log_level, int, 0);
MODULE_PARM_DESC(log_level, "OTM_HDMI_LOG_LEVEL");

int edidprint_init_module(void)
{

	printk("edidprint_init_module() called\n");
	set_log_level(log_level);
	return 0;
}

void edidprint_cleanup_module(void)
{
	printk("edidprint module cleanup\n");
}

module_init(edidprint_init_module);
module_exit(edidprint_cleanup_module);

MODULE_LICENSE("GPL");
