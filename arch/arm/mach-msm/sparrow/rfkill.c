/* Control bluetooth power for bcm board */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>

#define GPIO_BT_RESET 36
#define GPIO_BT_REGEN 34

static struct rfkill *bt_rfk;
static const char bt_name_20715[] = "bcm20715";
static const char bt_name_4343w[] = "bcm4343w";
static int bluetooth_set_power(void *data, bool blocked)
{
	int rc_reset = 0;
	int rc_reg = 0;
	int request_fail = 0;

	// only check bt_reset when HWID is Sparrow EVB  +++
	if (g_ASUS_hwID == SPARROW_EVB) {
	    rc_reset = gpio_request(GPIO_BT_RESET, "bt_reset");
	    if (rc_reset) {
		request_fail = rc_reset;
		printk ("rfkill: gpio GPIO_BT_RESET request fail , free gpio \n");
		gpio_free(GPIO_BT_RESET);
	    }
        }
	// only check bt_reset when HWID is Sparrow EVB  ---

	rc_reg = gpio_request(GPIO_BT_REGEN, "bt_reg");
	if (rc_reg) {
		request_fail = rc_reg;
		printk ("rfkill: gpio GPIO_BT_REGEN request fail , free gpio \n");
		gpio_free(GPIO_BT_REGEN);
	}

	if (request_fail != 0) {
		printk ("rfkill: gpio request fail , free gpio \n");
	} else if (!blocked) {
		printk("rfkilL: BT power on\n");
		// BT reg_eng high
		gpio_direction_output(GPIO_BT_REGEN, 0);
		usleep(300);
		gpio_direction_output(GPIO_BT_REGEN, 1);
		gpio_free(GPIO_BT_REGEN);

		// only pull high BT_RESET when HWID is Sparrow EVB +++
		if (g_ASUS_hwID == SPARROW_EVB) {
		    gpio_direction_output(GPIO_BT_RESET, 0);
		    usleep(300);
		    gpio_direction_output(GPIO_BT_RESET, 1);
		    gpio_free(GPIO_BT_RESET);
		}
		// only pull high BT_RESET when HWID is Sparrow EVB ---
	} else {
		printk("rfkill: BT shutdown \n");
		gpio_direction_output(GPIO_BT_REGEN, 0);
		gpio_free(GPIO_BT_REGEN);

		//only pull low BT_RESET when HWID is Sparrow EVB +++
		if (g_ASUS_hwID == SPARROW_EVB) {
		    gpio_direction_output(GPIO_BT_RESET, 0);
		    gpio_free(GPIO_BT_RESET);
		}
		//only pull low BT_RESET when HWID is Sparrow EVB ---
	}
	return 0;
}

static struct rfkill_ops bcm_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int bcm_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* blocked = off */

	printk("rfkill: bcm_rfkill_probe\n");


	bluetooth_set_power(NULL, default_state);

	// distingush EVB and non-EVB
	if (g_ASUS_hwID == SPARROW_EVB) {
	    bt_rfk = rfkill_alloc(bt_name_20715, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bcm_rfkill_ops , NULL);
	} else
	    bt_rfk = rfkill_alloc(bt_name_4343w, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bcm_rfkill_ops , NULL);


	if (!bt_rfk) {
		rc = -ENOMEM;
		printk("rfkill: rfkill alloc fail\n");
	}

	//rfkill_set_states(bt_rfk, default_state, false);
	rfkill_init_sw_state(bt_rfk, default_state);

	/* userspace cannot take exclusive control */

	rc = rfkill_register(bt_rfk);
	if (rc) {
		printk("rfkill: rfkill register fail \n");
		rfkill_destroy(bt_rfk);
	}

	return 0;
}

static int bcm_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);

	return 0;
}
static struct of_device_id bcm_20715_match_table[] = {
	{ .compatible = "bcm,bcm-rfkill", },
	{}
};

static struct platform_driver bcm_rfkill_driver = {
	.probe = bcm_rfkill_probe,
	.remove = bcm_rfkill_remove,
	.driver = {
		.name = "bcm_rfkill",
		.owner = THIS_MODULE,
		.of_match_table = bcm_20715_match_table,
	},
};

static int __init bcm_rfkill_init(void)
{
	printk("rfkill: bcm rfkill init\n");
	return platform_driver_register(&bcm_rfkill_driver);
}

static void __exit bcm_rfkill_exit(void)
{
	platform_driver_unregister(&bcm_rfkill_driver);
}

module_init(bcm_rfkill_init);
module_exit(bcm_rfkill_exit);
MODULE_DESCRIPTION("bcm rfkill");
MODULE_AUTHOR("Beryl Hou <beryl_hou@asus.com>");
MODULE_LICENSE("GPL");
