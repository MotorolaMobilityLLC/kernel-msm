#include <linux/kernel.h>
#include <mach/gpiomux.h>

/////////////////////////////////////////////////////////////////////
//includ add asus platform gpio table

#include "sparrow_evb_gpio_pinmux.h"
#include "sparrow_sr_pni_gpio_pinmux.h"
#include "sparrow_sr_ql_gpio_pinmux.h"
#include "sparrow_sr2_gpio_pinmux.h"
#include "wren_evb_gpio_pinmux.h"

/////////////////////////////////////////////////////////////////////
//define asus gpio var.

int __init device_gpio_init(void)
{   

   	switch (g_ASUS_hwID)
   	{
	case SPARROW_EVB:
		printk("[KERNEL] sparrow gpio config table = EVB\n");
		msm_gpiomux_install(sparrow_evb_msm8226_gpio_configs,
		ARRAY_SIZE(sparrow_evb_msm8226_gpio_configs));
  	break;
	case SPARROW_SR_PNI:
		printk("[KERNEL] sparrow gpio config table = SR_PNI\n");
		msm_gpiomux_install(sparrow_sr_pni_msm8226_gpio_configs,
		ARRAY_SIZE(sparrow_sr_pni_msm8226_gpio_configs));
  	break;
	case SPARROW_SR_QL:
		printk("[KERNEL] sparrow gpio config table = SR_QL\n");
		msm_gpiomux_install(sparrow_sr_ql_msm8226_gpio_configs,
		ARRAY_SIZE(sparrow_sr_ql_msm8226_gpio_configs));
  	break;
	case SPARROW_SR2:
		printk("[KERNEL] sparrow gpio config table = SR_PNI\n");
		msm_gpiomux_install(sparrow_sr2_msm8226_gpio_configs,
		ARRAY_SIZE(sparrow_sr2_msm8226_gpio_configs));
	break;
	case WREN_EVB:
		printk("[KERNEL] wren gpio config table = EVB\n");
		msm_gpiomux_install(wren_evb_msm8226_gpio_configs,
		ARRAY_SIZE(wren_evb_msm8226_gpio_configs));
	break;
	default:
		printk(KERN_ERR "[ERROR] There is NO valid hardware ID, use SPARROW EVB GPIO Instead.\n");
		msm_gpiomux_install(sparrow_evb_msm8226_gpio_configs,
		ARRAY_SIZE(sparrow_evb_msm8226_gpio_configs));
	break;
	}

   return 0;
}
