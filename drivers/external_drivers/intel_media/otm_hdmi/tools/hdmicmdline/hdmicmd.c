#include "hdmicmd.h"

/*static char *resolution = "1024x600@60"; */
static char *resolution = "resolution";
static int vic = -1;

module_param(resolution, charp, 0000);
MODULE_PARM_DESC(resolution, "resolution");

module_param(vic, int, 0);
MODULE_PARM_DESC(vic, "VIC");

int hdmicmd_init_module(void)
{

	printk(KERN_INFO "hdmicmd_init_module() called\n");

	if (strcmp(resolution, "resolution") == 0 && vic == -1) {
		otm_print_cmdline_option();
	} else if (strcmp(resolution, "resolution") != 0) {
			/* Modify Video Option Based on Passed Resolution */
			otm_cmdline_parse_option(resolution);
	} else if (vic != -1) {
			/* Modify Video Option Based on Passed VIC */
			otm_cmdline_set_vic_option(vic);
	}
	return 0;
}

void hdmicmd_cleanup_module(void)
{
	printk(KERN_INFO "hdmicmd module cleanup\n");
}

module_init(hdmicmd_init_module);
module_exit(hdmicmd_cleanup_module);

MODULE_LICENSE("GPL");
