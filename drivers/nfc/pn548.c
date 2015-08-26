/*
 * Copyright (C) 2010 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/nfc/pn548.h>
#include <linux/nfc/pn548_hwadapter.h>
#include <linux/of_gpio.h>

#include <linux/wakelock.h>
#include <linux/async.h>

#define NFC_POWER_OFF       false
#define NFC_POWER_ON        true

#define MAX_BUFFER_SIZE     512
#define NFC_TIMEOUT_MS      2000

static bool sIsWakeLocked = false;

static bool sPowerState = NFC_POWER_OFF;

static struct i2c_client *pn548_client;

struct wake_lock nfc_wake_lock;

static void pn548_disable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
	if (pn548_dev->irq_enabled) {
		disable_irq_nosync(pn548_get_irq_pin(pn548_dev));
		disable_irq_wake(pn548_get_irq_pin(pn548_dev));
		pn548_dev->irq_enabled = false;
		pr_debug(PN548_DRV_NAME ":%s disable IRQ\n", __func__);
	} else {
		pr_debug("%s IRQ is already disabled!\n", __func__);
	}

	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}

static void pn548_enable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);

	if (!pn548_dev->irq_enabled) {
		pn548_dev->irq_enabled = true;
		enable_irq(pn548_dev->client->irq);
		enable_irq_wake(pn548_dev->client->irq);
		pr_debug(PN548_DRV_NAME ":%s enable IRQ\n", __func__);
	} else {
		pr_debug("%s IRQ is already enabled!\n", __func__);
	}

	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn548_dev_irq_handler(int irq, void *dev_id)
{
	struct pn548_dev *pn548_dev = dev_id;
	unsigned long flags;
	unsigned int irq_gpio_val;

	irq_gpio_val = gpio_get_value(pn548_dev->irq_gpio);

	if (irq_gpio_val == 0) {
		pr_err("%s: False Interrupt!\n", __func__);
		return IRQ_HANDLED;
	}

	if (sPowerState == NFC_POWER_ON) {
		spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
		/* Wake up waiting readers */
		wake_up(&pn548_dev->read_wq);
		if (sIsWakeLocked == false) {
			wake_lock(&nfc_wake_lock);
			sIsWakeLocked = true;
		} else {
			pr_debug("%s already wake locked!\n", __func__);
		}
		spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
	} else {
		pr_err("%s, NFC IRQ Triggered during NFC OFF\n", __func__);
	}

	return IRQ_HANDLED;
}

static ssize_t pn548_dev_read(struct file *filp, char __user *buf,
					size_t count, loff_t *offset)
{
	struct pn548_dev *pn548_dev = filp->private_data;
	static char tmp[MAX_BUFFER_SIZE];
	int ret;
	static bool isFirstPacket = true;
	unsigned long flags;

	if (count > MAX_BUFFER_SIZE)
	count = MAX_BUFFER_SIZE;

	if (isFirstPacket == false) {
		ret = wait_event_interruptible_timeout(pn548_dev->read_wq,
				gpio_get_value(pn548_dev->irq_gpio),
				msecs_to_jiffies(NFC_TIMEOUT_MS));
		if (ret == 0) {
			pr_err("%s: no more interrupt after %dms (%d)!\n",
			       __func__, NFC_TIMEOUT_MS,
			       gpio_get_value(pn548_dev->irq_gpio));
			spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
			if (sIsWakeLocked == true) {
				wake_unlock(&nfc_wake_lock);
				sIsWakeLocked = false;
			}
			spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock,
					       flags);
			isFirstPacket = true;
		}
	}

	if (isFirstPacket == true)
	{
		ret = wait_event_interruptible(pn548_dev->read_wq,
					gpio_get_value(pn548_dev->irq_gpio));
		if (ret == 0)
			isFirstPacket = false;
	}

	if (ret == -ERESTARTSYS)
		return ret;

	/* Read data */
	mutex_lock(&pn548_dev->read_mutex);
	memset(tmp, 0x00, MAX_BUFFER_SIZE);
	ret = i2c_master_recv(pn548_dev->client, tmp, count);
	mutex_unlock(&pn548_dev->read_mutex);
	if (count == 0) {
		pr_err("%s: reading 0 bytes! skip! (%d)\n", __func__, ret);
		return ret;
	}
	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}

	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n",
							__func__, ret);
		return -EIO;
	}

	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static ssize_t pn548_dev_write(struct file *filp, const char __user *buf,
						size_t count, loff_t *offset)
{
	struct pn548_dev  *pn548_dev;
	static char tmp[MAX_BUFFER_SIZE];
	int ret;

	pn548_dev = filp->private_data;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(tmp, 0x00, MAX_BUFFER_SIZE);
	if (copy_from_user(tmp, buf, count)) {
		pr_err(PN548_DRV_NAME ":%s : failed to copy from user space\n",
								__func__);
		return -EFAULT;
	}

	mutex_lock(&pn548_dev->read_mutex);
	ret = i2c_master_send(pn548_dev->client, tmp, count);
	mutex_unlock(&pn548_dev->read_mutex);
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	return ret;
}

static int pn548_dev_open(struct inode *inode, struct file *filp)
{
	struct pn548_dev *pn548_dev = i2c_get_clientdata(pn548_client);

	filp->private_data = pn548_dev;
	pr_debug("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}

static long pn548_dev_unlocked_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct pn548_dev *pn548_dev = filp->private_data;
	unsigned long flags;

	switch (cmd) {
	case pn548_SET_PWR:
		if (arg == 2) {
			/*  power on with firmware download */
			dprintk(PN548_DRV_NAME
				":%s power on with firmware\n",
				__func__);
			gpio_set_value(pn548_dev->ven_gpio, 1);
			gpio_set_value(pn548_dev->firm_gpio, 1);
			msleep(10);
			gpio_set_value(pn548_dev->ven_gpio, 0);
			msleep(10);
			gpio_set_value(pn548_dev->ven_gpio, 1);
			msleep(10);
		} else if (arg == 1) {
			/* power on */
			pr_info(PN548_DRV_NAME ":%s power on\n", __func__);
			if (sPowerState == NFC_POWER_OFF) {
				gpio_set_value(pn548_dev->firm_gpio, 0);
				gpio_set_value(pn548_dev->ven_gpio, 1);
				msleep(10);

				pn548_enable_irq(pn548_dev);
				spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
				sPowerState = NFC_POWER_ON;
				spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
			} else {
				pr_err("%s NFC is alread On!\n", __func__);
			}
		} else  if (arg == 0) {
			/* power off */
			pr_info(PN548_DRV_NAME ":%s power off\n", __func__);
			if (sPowerState == NFC_POWER_ON) {
				gpio_set_value(pn548_dev->firm_gpio, 0);
				gpio_set_value(pn548_dev->ven_gpio, 0);
				msleep(10);

				pn548_disable_irq(pn548_dev);
				spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);

				if (sIsWakeLocked == true) {
					pr_err("%s: Release Wake_Lock\n", __func__);
					wake_unlock(&nfc_wake_lock);
					sIsWakeLocked = false;
				}
				sPowerState = NFC_POWER_OFF;
				spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
			} else {
				pr_err("%s NFC is alread Off!\n", __func__);
			}
		} else {
			pr_err("%s bad arg %ld\n", __func__, arg);
			return -EINVAL;
		}
		break;

	case pn548_HW_REVISION:
		return pn548_get_hw_revision();

	default:
		pr_err("%s bad ioctl %d\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn548_dev_fops = {
	.owner  = THIS_MODULE,
	.llseek = no_llseek,
	.read   = pn548_dev_read,
	.write  = pn548_dev_write,
	.open   = pn548_dev_open,
	.unlocked_ioctl = pn548_dev_unlocked_ioctl,
	.compat_ioctl   = pn548_dev_unlocked_ioctl,
};

static int pn548_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;
	struct pn548_dev *pn548_dev = NULL;

	pn548_client = client;
	pn548_dev = kzalloc(sizeof(*pn548_dev), GFP_KERNEL);
	if (pn548_dev == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	pn548_parse_dt(&client->dev, pn548_dev);

	pn548_dev->client   = client;

	ret = gpio_request(pn548_dev->irq_gpio, "nfc_int");
	if (ret) {
		pr_err(PN548_DRV_NAME ": nfc_int request failed!\n");
		goto err_int;
	}

	ret = gpio_request(pn548_dev->ven_gpio, "nfc_ven");
	if (ret) {
		pr_err(PN548_DRV_NAME ": nfc_ven request failed!\n");
		goto err_ven;
	}

	ret = gpio_request(pn548_dev->firm_gpio, "nfc_firm");
	if (ret) {
		pr_err(PN548_DRV_NAME ": nfc_firm request failed!\n");
		goto err_firm;
	}

	pn548_gpio_enable(pn548_dev);

	ret = gpio_direction_output(pn548_dev->ven_gpio,0);
	ret = gpio_direction_output(pn548_dev->firm_gpio,0);
	ret = gpio_direction_input(pn548_dev->irq_gpio);

	/* init mutex and queues */
	init_waitqueue_head(&pn548_dev->read_wq);
	mutex_init(&pn548_dev->read_mutex);
	spin_lock_init(&pn548_dev->irq_enabled_lock);

	pn548_dev->pn548_device.minor = MISC_DYNAMIC_MINOR;
	pn548_dev->pn548_device.name = PN548_DRV_NAME;
	pn548_dev->pn548_device.fops = &pn548_dev_fops;

	ret = misc_register(&pn548_dev->pn548_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __func__);
		goto err_misc_register;
	}

	wake_lock_init(&nfc_wake_lock, WAKE_LOCK_SUSPEND, "NFCWAKE");

	/* request irq.  the irq is set whenever the chip has data available
	* for reading.  it is cleared when all data has been read.
	*/
	ret = request_irq(pn548_gpio_to_irq(pn548_dev), pn548_dev_irq_handler,
				IRQF_TRIGGER_RISING|IRQF_NO_SUSPEND,
				client->name, pn548_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}

	/*
	* Below code does not pack with pn548_enable_irq
	* request irq make irq enable.
	* so if irq enable in pn548_enable_irq called,
	* unbalanced enable log will be appeared.
	*/
	pn548_dev->irq_enabled = true;
	enable_irq_wake(pn548_get_irq_pin(pn548_dev));
	pn548_disable_irq(pn548_dev);
	i2c_set_clientdata(client, pn548_dev);

	pr_info("%s done\n", __func__);

	return 0;

err_request_irq_failed:
	misc_deregister(&pn548_dev->pn548_device);

err_misc_register:
	mutex_destroy(&pn548_dev->read_mutex);
	gpio_free(pn548_dev->firm_gpio);

err_firm:
	gpio_free(pn548_dev->ven_gpio);

err_ven:
	gpio_free(pn548_dev->irq_gpio);

err_int:
	kfree(pn548_dev);

err_exit:

	return ret;
}

static int pn548_remove(struct i2c_client *client)
{
	struct pn548_dev *pn548_dev;

	pn548_dev = i2c_get_clientdata(client);
	free_irq(pn548_gpio_to_irq(pn548_dev), pn548_dev);
	misc_deregister(&pn548_dev->pn548_device);
	mutex_destroy(&pn548_dev->read_mutex);
	gpio_free(pn548_dev->firm_gpio);
	gpio_free(pn548_dev->ven_gpio);
	gpio_free(pn548_dev->irq_gpio);
	kfree(pn548_dev);

	return 0;
}

static void pn548_shutdown(struct i2c_client *client)
{
	struct pn548_dev *pn548_dev;

	pn548_dev = i2c_get_clientdata(client);
	pn548_shutdown_cb(pn548_dev);

	return;
}

static const struct i2c_device_id pn548_id[] = {
	{ PN548_DRV_NAME, 0 },
	{ }
};

static struct of_device_id pn548_match_table[] = {
	{ .compatible = "nxp,pn548",},
	{ },
};

static struct i2c_driver pn548_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = PN548_DRV_NAME,
		.of_match_table = pn548_match_table,
	},
	.probe      = pn548_probe,
	.remove     = pn548_remove,
	.shutdown   = pn548_shutdown,
	.id_table   = pn548_id,
};

static void async_dev_init(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&pn548_driver);
	if (ret < 0)
		pr_err("[NFC]failed to i2c_add_driver\n");
	return;
}

static int __init pn548_dev_init(void)
{
	async_schedule(async_dev_init, NULL);

	return 0;
}

module_init(pn548_dev_init);

static void __exit pn548_dev_exit(void)
{
	i2c_del_driver(&pn548_driver);
}
module_exit(pn548_dev_exit);

MODULE_DEVICE_TABLE(i2c, pn548_id);
MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN548 driver");
MODULE_LICENSE("GPL");
