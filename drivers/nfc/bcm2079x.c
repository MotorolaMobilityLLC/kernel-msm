/*
 * Copyright (C) 2012 Broadcom Corporation.
 * Copyright (C) 2015 LGE Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/of_gpio.h>

#include <linux/nfc/bcm2079x.h>
#include <linux/wakelock.h>

#include <linux/clk.h>

/* do not change below */
#define MAX_BUFFER_SIZE     288
#define I2C_ID_NAME "bcm2079x"
#define I2C_DEVICE_ADDR 0x76

static struct bcm2079x_platform_data platform_data;

/* Read data */
#define PACKET_HEADER_SIZE_NCI  (4)
#define PACKET_HEADER_SIZE_HCI  (3)
#define PACKET_TYPE_NCI     (16)
#define PACKET_TYPE_HCIEV   (4)
#define MAX_PACKET_SIZE     (PACKET_HEADER_SIZE_NCI + 255)

struct bcm2079x_dev {
	wait_queue_head_t read_wq;
	struct mutex rw_mutex;
	struct i2c_client *client;
	struct miscdevice bcm2079x_device;
	unsigned int wake_gpio;
	unsigned int en_gpio;
	unsigned int irq_gpio;
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	unsigned int count_irq;
	bool irq_wake_enabled;
	struct wake_lock wlock;
	struct clk *s_clk;
};

static void bcm2079x_init_stat(struct bcm2079x_dev *bcm2079x_dev)
{
	bcm2079x_dev->count_irq = 0;
}

static void bcm2079x_disable_irq(struct bcm2079x_dev *bcm2079x_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&bcm2079x_dev->irq_enabled_lock, flags);
	if (bcm2079x_dev->irq_enabled) {
		disable_irq_nosync(bcm2079x_dev->client->irq);
		if (bcm2079x_dev->irq_wake_enabled) {
			disable_irq_wake(bcm2079x_dev->client->irq);
			bcm2079x_dev->irq_wake_enabled = false;
		}
		bcm2079x_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&bcm2079x_dev->irq_enabled_lock, flags);
}

static void bcm2079x_enable_irq(struct bcm2079x_dev *bcm2079x_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&bcm2079x_dev->irq_enabled_lock, flags);
	if (!bcm2079x_dev->irq_enabled) {
		bcm2079x_dev->irq_enabled = true;
		enable_irq(bcm2079x_dev->client->irq);
		if (!bcm2079x_dev->irq_wake_enabled) {
			enable_irq_wake(bcm2079x_dev->client->irq);
			bcm2079x_dev->irq_wake_enabled = true;
		}
	}
	spin_unlock_irqrestore(&bcm2079x_dev->irq_enabled_lock, flags);
}

/*
 * The alias address 0x79, when sent as a 7-bit address from the host processor
 * will match the first byte (highest 2 bits) of the default client address
 * (0x1FA) that is programmed in bcm20791.
 * When used together with the first byte (0xFA) of the byte sequence below,
 * it can be used to address the bcm20791 in a system that does not support
 * 10-bit address and change the default address to 0x38.
 * the new address can be changed by changing the CLIENT_ADDRESS below if 0x38
 * conflicts with other device on the same i2c bus.
 */
#define ALIAS_ADDRESS     0x79

static int change_client_addr(struct bcm2079x_dev *bcm2079x_dev, int addr)
{
	struct i2c_client *client;
	int ret;
	int i;
	char addr_data[] = {
		0xFA, 0xF2, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x2A
	};

	client = bcm2079x_dev->client;
	client->addr = ALIAS_ADDRESS;
	client->flags &= ~I2C_CLIENT_TEN;

	addr_data[5] = addr & 0xFF;
	ret = 0;
	for (i = 1; i < sizeof(addr_data) - 1; ++i)
		ret += addr_data[i];
	addr_data[sizeof(addr_data) - 1] = (ret & 0xFF);
	pr_info("Change client device from (0x%04X) flag ="
		"0x%04x, addr_data[%d] = %02x\n",
		client->addr, client->flags, sizeof(addr_data) - 1,
		addr_data[sizeof(addr_data) - 1]);
	ret = i2c_master_send(client, addr_data, sizeof(addr_data));
	if (ret != sizeof(addr_data)) {
		client->addr = ALIAS_ADDRESS;
		client->flags &= ~I2C_CLIENT_TEN;
		pr_info("Change client device from (0x%04X) flag ="
			"0x%04x, addr_data[%d] = %02x\n",
			client->addr, client->flags, sizeof(addr_data) - 1,
			addr_data[sizeof(addr_data) - 1]);
		ret = i2c_master_send(client, addr_data, sizeof(addr_data));
	}
	client->addr = addr_data[5];

	pr_info
	    ("Change client device changed to (0x%04X) flag = %04x, ret = %d\n",
	     client->addr, client->flags, ret);

	return (ret == sizeof(addr_data) ? 0 : -EIO);
}

static irqreturn_t bcm2079x_dev_irq_handler(int irq, void *dev_id)
{
	struct bcm2079x_dev *bcm2079x_dev = dev_id;
	wake_up(&bcm2079x_dev->read_wq);
	wake_lock(&bcm2079x_dev->wlock);

	return IRQ_HANDLED;
}

static unsigned int bcm2079x_dev_poll(struct file *filp, poll_table * wait)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;
	unsigned int mask = 0;
	unsigned long flags;

	/* patch for GKI exception from Broadcom CSP 796884 */
	poll_wait(filp, &bcm2079x_dev->read_wq, wait);

	spin_lock_irqsave(&bcm2079x_dev->irq_enabled_lock, flags);

	if (gpio_get_value(platform_data.irq_gpio) == 1) /* irq gpio get */
		mask |= POLLIN | POLLRDNORM;

	spin_unlock_irqrestore(&bcm2079x_dev->irq_enabled_lock, flags);

	return mask;
}

static ssize_t bcm2079x_dev_read(struct file *filp, char __user * buf,
				 size_t count, loff_t * offset)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;
	unsigned char tmp[MAX_BUFFER_SIZE];
	int total, len, ret;

	total = 0;
	len = 0;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	mutex_lock(&bcm2079x_dev->rw_mutex);

	/* Read the first 4 bytes to include the length of the NCI or
	 * HCI packet.
	 */
	ret = i2c_master_recv(bcm2079x_dev->client, tmp, 4);
	if (ret == 4) {
		total = ret;
		/* First byte is the packet type */
		switch (tmp[0]) {
		case PACKET_TYPE_NCI:
			len = tmp[PACKET_HEADER_SIZE_NCI - 1];
			break;

		case PACKET_TYPE_HCIEV:
			len = tmp[PACKET_HEADER_SIZE_HCI - 1];
			if (len == 0)
				total--; /*Since payload is 0, decrement total size (from 4 to 3) */
			else
				len--;   /*First byte of payload is in tmp[3] already */
			break;

		default:
			len = 0;         /*Unknown packet byte */
			break;
		}		/* switch */

		/* make sure full packet fits in the buffer */
		if (len > 0 && (len + total) <= count) {
			/* read the remainder of the packet. */
			ret = i2c_master_recv(
					bcm2079x_dev->client, tmp + total, len);
			if (ret == len)
				total += len;
		}
	}

	mutex_unlock(&bcm2079x_dev->rw_mutex);

	if (total > count || copy_to_user(buf, tmp, total)) {
		pr_err("failed to copy to user space, total = %d\n", total);
		total = -EFAULT;
	}
	wake_unlock(&bcm2079x_dev->wlock);
	return total;
}

static ssize_t bcm2079x_dev_write(struct file *filp, const char __user * buf,
				  size_t count, loff_t * offset)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buf, count)) {
		pr_err("failed to copy from user space\n");
		return -EFAULT;
	}

	mutex_lock(&bcm2079x_dev->rw_mutex);
	/* Write data */
	ret = i2c_master_send(bcm2079x_dev->client, tmp, count);
	if (ret != count) {
		pr_err("failed to write %d\n", ret);
		ret = -EIO;
	}
	mutex_unlock(&bcm2079x_dev->rw_mutex);

	return ret;
}

static int bcm2079x_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	struct bcm2079x_dev *bcm2079x_dev = container_of(filp->private_data,
							 struct bcm2079x_dev,
							 bcm2079x_device);
	filp->private_data = bcm2079x_dev;
	bcm2079x_init_stat(bcm2079x_dev);
	bcm2079x_enable_irq(bcm2079x_dev);
	pr_info("%d,%d\n", imajor(inode), iminor(inode));

	return ret;
}

static long bcm2079x_dev_unlocked_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	struct bcm2079x_dev *bcm2079x_dev = filp->private_data;

	pr_info("%s,  cmd (%x) arg %lx \n", __func__, cmd, arg);

	switch (cmd) {
	case BCMNFC_READ_FULL_PACKET:
		break;
	case BCMNFC_READ_MULTI_PACKETS:
		break;
	case BCMNFC_CHANGE_ADDR:
		return change_client_addr(bcm2079x_dev, arg);
	case BCMNFC_POWER_CTL:
		if (arg == 0)
			wake_unlock(&bcm2079x_dev->wlock);
		gpio_set_value(bcm2079x_dev->en_gpio, arg);
		break;
	case BCMNFC_WAKE_CTL:
		gpio_set_value(bcm2079x_dev->wake_gpio, arg);
		break;
	default:
		pr_err("%s, unknown cmd (%x, %lx)\n", __func__, cmd, arg);
		return 0;
	}

	return 0;
}

static const struct file_operations bcm2079x_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.poll = bcm2079x_dev_poll,
	.read = bcm2079x_dev_read,
	.write = bcm2079x_dev_write,
	.open = bcm2079x_dev_open,
	.unlocked_ioctl = bcm2079x_dev_unlocked_ioctl
};

static int bcm2079x_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret;
	struct bcm2079x_dev *bcm2079x_dev = NULL;

	bcm2079x_dev = kzalloc(sizeof(*bcm2079x_dev), GFP_KERNEL);
	if (bcm2079x_dev == NULL) {
		pr_err("failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		platform_data.irq_gpio =
		    of_get_named_gpio_flags(client->dev.of_node, "bcm,gpio_irq",
					    0, NULL);
		platform_data.en_gpio =
		    of_get_named_gpio_flags(client->dev.of_node, "bcm,gpio_ven",
					    0, NULL);
		platform_data.wake_gpio =
		    of_get_named_gpio_flags(client->dev.of_node,
					    "bcm,gpio_mode", 0, NULL);

		pr_info("%d, platform_data.irq_gpio\n", platform_data.irq_gpio);
		pr_info("%d, platform_data.en_gpio\n", platform_data.en_gpio);
		pr_info("%d, platform_data.wake_gpio\n",
			platform_data.wake_gpio);

	} else {
		pr_err("nfc probe of_node fail\n");
		ret = -ENODEV;
		goto err_mem;
	}

	bcm2079x_dev->s_clk = clk_get(&client->dev, "ref_clk");
	clk_prepare_enable(bcm2079x_dev->s_clk);

	ret = gpio_request_one(platform_data.irq_gpio, GPIOF_IN, "nfc_int");
	if (ret)
		goto err_irq;

	ret = gpio_request_one(platform_data.en_gpio, GPIOF_OUT_INIT_LOW,
			     "nfc_ven");
	if (ret)
		goto err_en;

	ret = gpio_request_one(platform_data.wake_gpio, GPIOF_OUT_INIT_LOW,
			     "nfc_firm");
	if (ret)
		goto err_wake;

	bcm2079x_dev->wake_gpio = platform_data.wake_gpio;
	bcm2079x_dev->irq_gpio = platform_data.irq_gpio;
	bcm2079x_dev->en_gpio = platform_data.en_gpio;
	bcm2079x_dev->client = client;

	/* init mutex and queues */
	init_waitqueue_head(&bcm2079x_dev->read_wq);
	mutex_init(&bcm2079x_dev->rw_mutex);
	spin_lock_init(&bcm2079x_dev->irq_enabled_lock);

	bcm2079x_dev->bcm2079x_device.minor = MISC_DYNAMIC_MINOR;
	bcm2079x_dev->bcm2079x_device.name = I2C_ID_NAME;
	bcm2079x_dev->bcm2079x_device.fops = &bcm2079x_dev_fops;

	ret = misc_register(&bcm2079x_dev->bcm2079x_device);
	if (ret) {
		pr_err("misc_register failed\n");
		goto err_misc_register;
	}

	wake_lock_init(&bcm2079x_dev->wlock, WAKE_LOCK_SUSPEND, "NFCWAKE");

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("requesting IRQ %d\n", client->irq);
	bcm2079x_dev->irq_enabled = true;

	ret = request_irq(client->irq, bcm2079x_dev_irq_handler,
			  IRQF_TRIGGER_RISING, client->name, bcm2079x_dev);
	if (ret) {
		pr_err("request_irq failed\n");
		goto err_request_irq_failed;
	}
	bcm2079x_dev->irq_wake_enabled = false;
	bcm2079x_disable_irq(bcm2079x_dev);
	i2c_set_clientdata(client, bcm2079x_dev);

	pr_info("%s, probing bcm2079x driver exited successfully\n", __func__);
	return 0;

err_request_irq_failed:
	wake_lock_destroy(&bcm2079x_dev->wlock);
	misc_deregister(&bcm2079x_dev->bcm2079x_device);
err_misc_register:
	mutex_destroy(&bcm2079x_dev->rw_mutex);
	gpio_free(platform_data.wake_gpio);
err_wake:
	gpio_free(platform_data.en_gpio);
err_en:
	gpio_free(platform_data.irq_gpio);
err_irq:
	clk_disable_unprepare(bcm2079x_dev->s_clk);
	clk_put(bcm2079x_dev->s_clk);
err_mem:
	kfree(bcm2079x_dev);
	return ret;
}

static int bcm2079x_remove(struct i2c_client *client)
{
	struct bcm2079x_dev *bcm2079x_dev = i2c_get_clientdata(client);

	free_irq(client->irq, bcm2079x_dev);
	wake_lock_destroy(&bcm2079x_dev->wlock);
	misc_deregister(&bcm2079x_dev->bcm2079x_device);
	mutex_destroy(&bcm2079x_dev->rw_mutex);
	gpio_free(bcm2079x_dev->irq_gpio);
	gpio_free(bcm2079x_dev->en_gpio);
	gpio_free(bcm2079x_dev->wake_gpio);
	clk_disable_unprepare(bcm2079x_dev->s_clk);
	clk_put(bcm2079x_dev->s_clk);
	kfree(bcm2079x_dev);

	return 0;
}

static const struct i2c_device_id bcm2079x_id[] = {
	{I2C_ID_NAME, I2C_DEVICE_ADDR},
	{}
};

static struct i2c_driver bcm2079x_driver = {
	.id_table = bcm2079x_id,
	.probe = bcm2079x_probe,
	.remove = bcm2079x_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = I2C_ID_NAME,
	},
};

/*
 * module load/unload record keeping
 */

static int __init bcm2079x_dev_init(void)
{
	return i2c_add_driver(&bcm2079x_driver);
}

module_init(bcm2079x_dev_init);

static void __exit bcm2079x_dev_exit(void)
{
	i2c_del_driver(&bcm2079x_driver);
}

module_exit(bcm2079x_dev_exit);

MODULE_DEVICE_TABLE(i2c, bcm2079x_id);
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("NFC bcm2079x driver");
MODULE_LICENSE("GPL");
