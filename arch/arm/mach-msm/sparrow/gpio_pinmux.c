#include <linux/kernel.h>
#include <mach/gpiomux.h>

/////////////////////////////////////////////////////////////////////
//includ add asus platform gpio table

#include "evb_gpio_pinmux.h"
#include "sr_pni_gpio_pinmux.h"
#include "sr_ql_gpio_pinmux.h"
#include "sr2_gpio_pinmux.h"
#include "er_gpio_pinmux.h"
#include "pr_gpio_pinmux.h"

/////////////////////////////////////////////////////////////////////
//define asus gpio var.

int __init device_gpio_init(void)
{
	switch (g_ASUS_hwID)
	{
		case SPARROW_EVB:
			printk("[KERNEL] sparrow gpio config table = EVB\n");
			msm_gpiomux_install(evb_msm8226_gpio_configs,
					ARRAY_SIZE(evb_msm8226_gpio_configs));
			break;
		case SPARROW_SR_PNI:
			printk("[KERNEL] sparrow gpio config table = SR_PNI\n");
			msm_gpiomux_install(sr_pni_msm8226_gpio_configs,
					ARRAY_SIZE(sr_pni_msm8226_gpio_configs));
			break;
		case SPARROW_SR_QL:
			printk("[KERNEL] sparrow gpio config table = SR_QL\n");
			msm_gpiomux_install(sr_ql_msm8226_gpio_configs,
					ARRAY_SIZE(sr_ql_msm8226_gpio_configs));
			break;
		case SPARROW_SR2:
			printk("[KERNEL] sparrow gpio config table = SR2\n");
			msm_gpiomux_install(sr2_msm8226_gpio_configs,
					ARRAY_SIZE(sr2_msm8226_gpio_configs));
			break;
		case SPARROW_ER:
			printk("[KERNEL] sparrow gpio config table = ER\n");
			msm_gpiomux_install(er_msm8226_gpio_configs,
					ARRAY_SIZE(er_msm8226_gpio_configs));
			break;
		case SPARROW_PR:
			printk("[KERNEL] sparrow gpio config table = PR\n");
			msm_gpiomux_install(pr_msm8226_gpio_configs,
					ARRAY_SIZE(pr_msm8226_gpio_configs));
			break;
		default:
			printk(KERN_ERR "[ERROR] There is NO valid hardware ID, use SPARROW EVB GPIO Instead.\n");
			msm_gpiomux_install(evb_msm8226_gpio_configs,
					ARRAY_SIZE(evb_msm8226_gpio_configs));
			break;
	}

   return 0;
}
