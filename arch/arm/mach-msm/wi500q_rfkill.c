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
static const char bt_name[] = "bcm20715";

static int bluetooth_set_power(void *data, bool blocked)
{
	int rc_reset = 0 , rc_reg = 0;
	rc_reset = gpio_request(GPIO_BT_RESET, "bt_reset");
	if (rc_reset)
		gpio_free(GPIO_BT_RESET);

	rc_reg = gpio_request(GPIO_BT_REGEN, "bt_reg");
	if (rc_reg)
		gpio_free(GPIO_BT_REGEN);

	if (rc_reset || rc_reg) {
		printk ("gpio request fail , free gpio \n");
	} else if (!blocked) {
		printk("BT power on\n");
		// BT reg_eng high
 		gpio_direction_output(GPIO_BT_REGEN, 0);
		usleep(300);
 		gpio_direction_output(GPIO_BT_REGEN, 1);

		// BT reset high
 		gpio_direction_output(GPIO_BT_RESET, 0);
		usleep(300);	
		gpio_direction_output(GPIO_BT_RESET, 1);
		gpio_free(GPIO_BT_REGEN);
		gpio_free(GPIO_BT_RESET);

	} else {
		printk("BT shutdown \n");
 		gpio_direction_output(GPIO_BT_REGEN, 0);
		gpio_direction_output(GPIO_BT_RESET, 0);
		gpio_free(GPIO_BT_REGEN);
		gpio_free(GPIO_BT_RESET);
	}
	return 0;
}

static struct rfkill_ops bcm_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int bcm_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = false;  /* off */

	printk("bcm_rfkill_probe\n");


	bluetooth_set_power(NULL, default_state);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
				&bcm_rfkill_ops , NULL);
	if (!bt_rfk) {
		rc = -ENOMEM;
		printk("rfkill alloc fail\n");
	}

	rfkill_set_states(bt_rfk, default_state, false);

	/* userspace cannot take exclusive control */

	rc = rfkill_register(bt_rfk);
	if (rc) {
		printk("rfkill register fail \n");
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
	printk("bcm rfkill init");
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
