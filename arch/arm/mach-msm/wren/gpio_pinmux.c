#include <linux/kernel.h>
#include <mach/gpiomux.h>

/////////////////////////////////////////////////////////////////////
//includ add asus platform gpio table

#include "evb_gpio_pinmux.h"

/////////////////////////////////////////////////////////////////////
//define asus gpio var.

int __init device_gpio_init(void)
{
	switch (g_ASUS_hwID)
	{
		case WREN_EVB:
			printk("[KERNEL] wren gpio config table = EVB\n");
			msm_gpiomux_install(evb_msm8226_gpio_configs,
					ARRAY_SIZE(evb_msm8226_gpio_configs));
			break;
		default:
			printk(KERN_ERR "[ERROR] There is NO valid hardware ID, use WREN EVB GPIO Instead.\n");
			msm_gpiomux_install(evb_msm8226_gpio_configs,
					ARRAY_SIZE(evb_msm8226_gpio_configs));
			break;
	}
	return 0;
}
