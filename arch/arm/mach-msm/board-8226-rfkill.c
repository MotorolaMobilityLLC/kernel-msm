/*
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 HTC Corporation.
 * Copyright (C) 2011 Broadcom Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <asm/gpio.h>
#include <asm/mach-types.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

#ifdef CONFIG_BCM4335BT
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#endif /* CONFIG_BCM4335BT */

#define GPIO_BT_RESET_N	  75
#define GPIO_BT_REG_ON_N	76

#ifdef CONFIG_BCM4335BT
#define BTRFKILL_DBG    1
#if (BTRFKILL_DBG)
#define BTRFKILLDBG(fmt, arg...) printk(KERN_ERR " ** [ BTRFKILL :  %s ( %d ) ] " fmt "\n" , __FUNCTION__, __LINE__, ## arg)
#else
#define BTRFKILLDBG(fmt, arg...)
#endif
#endif /* CONFIG_BCM4335BT */

static struct rfkill *bt_rfk;
static const char bt_name[] = "brcm_Bluetooth_rfkill";

#ifdef CONFIG_BCM4335BT
#define BTLOCK_NAME     "btlock"
#define BTLOCK_MINOR    224
/* BT lock waiting timeout, in second */
#define BTLOCK_TIMEOUT	2

#define PR(msg, ...) printk("####"msg, ##__VA_ARGS__)

struct btlock {
	int lock;
	int cookie;
};
static struct semaphore btlock;
static int count = 1;
static int owner_cookie = -1;

int bcm_bt_lock(int cookie)
{
	int ret;
	char cookie_msg[5] = {0};

	ret = down_timeout(&btlock, msecs_to_jiffies(BTLOCK_TIMEOUT*1000));
	if (ret == 0) {
		memcpy(cookie_msg, &cookie, sizeof(cookie));
		owner_cookie = cookie;
		count--;
		PR("btlock acquired cookie: %s\n", cookie_msg);
	}

	return ret;
}
EXPORT_SYMBOL(bcm_bt_lock);

void bcm_bt_unlock(int cookie)
{
	char owner_msg[5] = {0};
	char cookie_msg[5] = {0};

	memcpy(cookie_msg, &cookie, sizeof(cookie));
	if (owner_cookie == cookie) {
		owner_cookie = -1;
		if (count++ > 1)
			PR("error, release a lock that was not acquired**\n");
		up(&btlock);
		PR("btlock released, cookie: %s\n", cookie_msg);
	} else {
		memcpy(owner_msg, &owner_cookie, sizeof(owner_cookie));
		PR("ignore lock release,  cookie mismatch: %s owner %s \n", cookie_msg,
				owner_cookie == 0 ? "NULL" : owner_msg);
	}
}
EXPORT_SYMBOL(bcm_bt_unlock);

static int btlock_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int btlock_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t btlock_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct btlock lock_para;
	ssize_t ret = 0;

	if (count < sizeof(struct btlock))
		return -EINVAL;

	if (copy_from_user(&lock_para, buffer, sizeof(struct btlock))) {
		return -EFAULT;
	}

	if (lock_para.lock == 0) {
		bcm_bt_unlock(lock_para.cookie);
	} else if (lock_para.lock == 1) {
		ret = bcm_bt_lock(lock_para.cookie);
	} else if (lock_para.lock == 2) {
		ret = bcm_bt_lock(lock_para.cookie);
	}

	return ret;
}

static long btlock_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations btlock_fops = {
	.owner   = THIS_MODULE,
	.open    = btlock_open,
	.release = btlock_release,
	.write   = btlock_write,
	.unlocked_ioctl = btlock_unlocked_ioctl,
};

static struct miscdevice btlock_misc = {
	.name  = BTLOCK_NAME,
	.minor = BTLOCK_MINOR,
	.fops  = &btlock_fops,
};

static int bcm_btlock_init(void)
{
	int ret;

	PR("init\n");

	ret = misc_register(&btlock_misc);
	if (ret != 0) {
		PR("Error: failed to register Misc driver,  ret = %d\n", ret);
		return ret;
	}
	sema_init(&btlock, 1);

	return ret;
}

static void bcm_btlock_exit(void)
{
	PR("btlock_exit:\n");

	misc_deregister(&btlock_misc);
}
#endif /* CONFIG_BCM4335BT */

static int bluetooth_set_power(void *data, bool blocked)
{
#ifdef CONFIG_BCM4335BT
	int lock_cookie_bt = 'B' | 'T'<<8 | '3'<<16 | '5'<<24;	/* cookie is "BT35" */
#endif /* CONFIG_BCM4335BT */

	printk(KERN_ERR "bluetooth_set_power set blocked=%d", blocked);
	if (!blocked) { 
#ifdef CONFIG_BCM4335BT
		if (bcm_bt_lock(lock_cookie_bt) != 0)
			printk("** BT rfkill: timeout in acquiring bt lock**\n");
#endif /* CONFIG_BCM4335BT */
		gpio_direction_output(GPIO_BT_RESET_N, 0);
		gpio_direction_output(GPIO_BT_REG_ON_N, 0);
		msleep(30);
		gpio_direction_output(GPIO_BT_RESET_N, 1);
		gpio_direction_output(GPIO_BT_REG_ON_N, 1);
		printk(KERN_ERR "Bluetooth RESET HIGH!!");
	} else {
		gpio_direction_output(GPIO_BT_RESET_N, 0);
		gpio_direction_output(GPIO_BT_REG_ON_N, 0);
		printk(KERN_ERR "Bluetooth RESET LOW!!");
	}
	return 0;
}

static struct rfkill_ops bluetooth_rfkill_ops = {
	.set_block = bluetooth_set_power,
};

static int bluetooth_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;  /* off */

#ifdef CONFIG_BCM4335BT
	bcm_btlock_init();
#endif /* CONFIG_BCM4335BT */
	printk(KERN_INFO "bluetooth_rfkill_probe\n");

	rc = gpio_request(GPIO_BT_RESET_N, "bt_reset");
	if (rc) {
		printk(KERN_ERR "GPIO req error no=%d", rc);
		gpio_free(GPIO_BT_RESET_N);
		gpio_free(GPIO_BT_REG_ON_N);
		rc = gpio_request(GPIO_BT_RESET_N, "bt_reset");
		if(rc) {
			printk(KERN_ERR "GPIO req error no=%d", rc);
			goto err_gpio_reset;
		}
	}
	gpio_direction_output(GPIO_BT_RESET_N, 0);
	gpio_direction_output(GPIO_BT_REG_ON_N, 0);

	bluetooth_set_power(NULL, default_state);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			&bluetooth_rfkill_ops, NULL);
	if (!bt_rfk) {
		printk(KERN_ERR "rfkill alloc failed.\n");
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_set_states(bt_rfk, default_state, false);

	/* userspace cannot take exclusive control */

	rc = rfkill_register(bt_rfk);
	if (rc)
		goto err_rfkill_reg;

	return 0;


err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:
err_gpio_reset:
	gpio_free(GPIO_BT_RESET_N);
	gpio_free(GPIO_BT_REG_ON_N);
	printk(KERN_ERR "bluetooth_rfkill_probe error!\n");
	return rc;
}

static int bluetooth_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);
	gpio_free(GPIO_BT_RESET_N);
	gpio_free(GPIO_BT_REG_ON_N);

#ifdef CONFIG_BCM4335BT
	bcm_btlock_exit();
#endif /* CONFIG_BCM4335BT */

	return 0;
}
struct bluetooth_rfkill_platform_data {
	int gpio_reset;
};

static struct bluetooth_rfkill_platform_data bluetooth_rfkill_platform;

static struct platform_device bluetooth_rfkill_device = {
	.name = "bluetooth_rfkill",
	.id   = -1,
	.dev = {
		.platform_data = &bluetooth_rfkill_platform,
	},
};

static struct platform_driver bluetooth_rfkill_driver = {
	.probe = bluetooth_rfkill_probe,
	.remove = bluetooth_rfkill_remove,
	.driver = {
		.name = "bluetooth_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init bluetooth_rfkill_init(void)
{
	int ret;

	printk(KERN_INFO "bluetooth_rfkill_init\n");
	ret = platform_driver_register(&bluetooth_rfkill_driver);
	if (ret)
		printk(KERN_ERR "Fail to register rfkill platform driver\n");
	printk(KERN_INFO "bluetooth_rfkill_init done\n");

	return platform_device_register(&bluetooth_rfkill_device);;
}

static void __exit bluetooth_rfkill_exit(void)
{
	platform_device_unregister(&bluetooth_rfkill_device);
	platform_driver_unregister(&bluetooth_rfkill_driver);
}

module_init(bluetooth_rfkill_init);
module_exit(bluetooth_rfkill_exit);

MODULE_DESCRIPTION("bluetooth rfkill");
MODULE_AUTHOR("Nick Pelly <npelly@google.com>");
MODULE_LICENSE("GPL");
