#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include "axc_bq51220Charger.h"
#include <linux/asus_bat.h>
#include <linux/asus_chg.h>
extern void asus_chg_set_chg_mode(enum asus_chg_src chg_src);

#define GPIO_WC_POK 85
struct workqueue_struct *wc_intr_wq = NULL;
struct delayed_work wc_worker;

static irqreturn_t wc_power_ok_handler(int irq, void *dev_id)
{
        queue_delayed_work(wc_intr_wq, &wc_worker, 0);       
	  printk("[BAT][WC]%s\n",__FUNCTION__);
        return IRQ_HANDLED;
}

static irqreturn_t wc_pd_det_handler(int irq, void *dev_id)
{      
	  printk("[BAT][WC]%s\n",__FUNCTION__);
        return IRQ_HANDLED;
}

static Bq51220_PIN wc_gGpio_pin[]=
{
    {//WC_POK
        .gpio = 85,
        .name = "WC_POK",
        .in_out_flag = 0,
        .handler = wc_power_ok_handler,
        .trigger_flag= IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,      
    },

    {//WC_PD_DET
        .gpio = 20,
        .name = "WC_PD_DET",
        .in_out_flag = 0,
        .handler = wc_pd_det_handler,
        .trigger_flag= IRQF_TRIGGER_FALLING|IRQF_TRIGGER_RISING,      
    },

    {//WC_EN1
        .gpio = 3,
        .name = "WC_EN1",
        .in_out_flag = 1,
        .init_value = 0,
    },

    {//WC_EN2
        .gpio = 2,
        .name = "WC_EN2",
        .in_out_flag = 1,
        .init_value = 0,
    },
};

static void wc_work(struct work_struct *dat)
{
	if(1 == gpio_get_value(GPIO_WC_POK))
		asus_chg_set_chg_mode(ASUS_CHG_SRC_WC);
	else
		asus_chg_set_chg_mode(ASUS_CHG_SRC_NONE);
}

static int AXC_bq51220_GPIO_setting(void)
{
	Bq51220_PIN_DEF i;
	int err = 0;
	printk("%s+++\n",__FUNCTION__);

	for(i = 0; i< Bq51220_PIN_COUNT;i++){

		//rquest
		err = gpio_request(wc_gGpio_pin[i].gpio, wc_gGpio_pin[i].name);
		if (err < 0) {
			printk("[BAT][WC]gpio_request %s failed,err = %d\n", wc_gGpio_pin[i].name, err);
			goto err_exit;
		}
		
		//input
       		if(wc_gGpio_pin[i].in_out_flag == 0){
	            	err = gpio_direction_input(wc_gGpio_pin[i].gpio);
   
        	    	if (err  < 0) {
        	        	printk( "[BAT][WC]gpio_direction_input %s failed, err = %d\n", wc_gGpio_pin[i].name, err);
        	        	goto err_exit;
        	    	}

			if(wc_gGpio_pin[i].handler != NULL){

                		wc_gGpio_pin[i].irq = gpio_to_irq(wc_gGpio_pin[i].gpio);

				printk("[BAT][WC]:GPIO:%d,IRQ:%d\n",wc_gGpio_pin[i].gpio,  wc_gGpio_pin[i].irq);
                
				if(true == wc_gGpio_pin[i].irq_enabled){
                    
                    			enable_irq_wake(wc_gGpio_pin[i].irq);

              			}

                		err = request_irq(wc_gGpio_pin[i].irq , 
                    				  wc_gGpio_pin[i].handler, 
                    				  wc_gGpio_pin[i].trigger_flag,
					          wc_gGpio_pin[i].name, 
                    				  NULL);

                		if (err  < 0) {
                    			printk( "[BAT][CHG][SMB]request_irq %s failed, err = %d\n",wc_gGpio_pin[i].name,err);
                    		goto err_exit;
                		}

            		}
		//output
		}else{
			 gpio_direction_output(wc_gGpio_pin[i].gpio, wc_gGpio_pin[i].init_value);            
        	}
	}
	printk("%s---\n",__FUNCTION__);
	return 0;

err_exit:
	    for(i = 0; i<Bq51220_PIN_COUNT;i++){        
        		gpio_free(wc_gGpio_pin[i].gpio);
    	    }
	printk("[BAT][WC]AXC_Smb346_Charger_InitGPIO_err_exit\n");  
	return err;  
}

void AXC_bq51220_Charger_Init(void)
{
	wc_intr_wq = create_singlethread_workqueue("wirelessChg_intr_wq");
        INIT_DELAYED_WORK(&wc_worker,wc_work);
	
	if (0 != AXC_bq51220_GPIO_setting()) 
	{
		printk( "[BAT][WC]Charger gpio can't init\n");
       }
}
