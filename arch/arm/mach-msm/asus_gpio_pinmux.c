#include <linux/kernel.h>
#include <mach/gpiomux.h>

/////////////////////////////////////////////////////////////////////
//includ add asus platform gpio table

#ifdef ASUS_WI500Q_PROJECT
#include "wi500q_evb2_gpio_pinmux.h"
#include "wi500q_sr_gpio_pinmux.h"
#include "wi500q_sr2_gpio_pinmux.h"
#endif


/////////////////////////////////////////////////////////////////////
//define asus gpio var.

int __init device_gpio_init(void)
{   

#ifdef ASUS_WI500Q_PROJECT
   	switch (g_ASUS_hwID)
   	{
	case WI500Q_EVB2:
		printk("[KERNEL] wi500q gpio config table = EVB2\n");  
		msm_gpiomux_install(wi500q_evb2_msm8226_gpio_configs,
		ARRAY_SIZE(wi500q_evb2_msm8226_gpio_configs));	 	  
  	break;
	case WI500Q_SR:
		printk("[KERNEL] wi500q gpio config table = SR\n");  
		msm_gpiomux_install(wi500q_sr_msm8226_gpio_configs,
		ARRAY_SIZE(wi500q_sr_msm8226_gpio_configs));	 	  
  	break;
	case WI500Q_SR2:
		printk("[KERNEL] wi500q gpio config table = SR2\n");  
		msm_gpiomux_install(wi500q_sr2_msm8226_gpio_configs,
		ARRAY_SIZE(wi500q_sr2_msm8226_gpio_configs));	 	  
  	break;
	default:
		printk(KERN_ERR "[ERROR] There is NO valid hardware ID, use WI500Q SR GPIO Instead.\n");			   
		msm_gpiomux_install(wi500q_sr_msm8226_gpio_configs,
		ARRAY_SIZE(wi500q_sr_msm8226_gpio_configs));	 
	break;
	}
#endif

 
   return 0;
}
