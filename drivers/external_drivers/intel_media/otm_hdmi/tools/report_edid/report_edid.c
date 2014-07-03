#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

/* 
 * TODO: Revert back afer EDID Parse
 *	 for established modes is implemented 
 */

extern void test_otm_hdmi_report_edid_full();
/* extern test_otm_hdmi_report_edid(); */

int report_edid_init_module(void)
{
	printk(KERN_INFO "report_edid module() called\n");
	test_otm_hdmi_report_edid_full();
	/* test_otm_hdmi_report_edid(); */
	return 0;
}

void report_edid_cleanup_module(void)
{
	printk(KERN_INFO "report_edid module cleanup");
}

module_init(report_edid_init_module);
module_exit(report_edid_cleanup_module);

MODULE_LICENSE("GPL");
