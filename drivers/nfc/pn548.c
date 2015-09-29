/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * modifications copyright (C) 2015 NXP B.V.
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
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/spinlock_types.h>
#include <linux/kthread.h>
#include <linux/qpnp/pwm.h>
#include "pn548.h"
#include <linux/wakelock.h>

#define NFC_TRY_NUM    3
#define MAX_BUFFER_SIZE    512

#define MODE_OFF    0
#define MODE_RUN    1
#define MODE_FW     2

#define CHIP "pn54x"
#define DRIVER_DESC "NFC driver for PN54x Family"

struct pn548_dev {
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct mutex		irq_wake_mutex;
	struct device		*dev;
	struct i2c_client	*client;
	struct miscdevice	pn548_device;
	struct clk		*nfc_clk;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	unsigned int		clk_req_gpio;
	bool			irq_enabled;
	bool			irq_wake_enabled;
	spinlock_t		irq_enabled_lock;
	bool			do_reading;
	struct wake_lock	wl;
	bool cancel_read;
};
/*
 *FUNCTION: pn548_disable_irq_wake
 *DESCRIPTION: disable irq wakeup function
 *Parameters
 * struct  pn548_dev *: device structure
 *RETURN VALUE
 * none
 */
static void pn548_disable_irq_wake(struct pn548_dev *pn548_dev)
{
	int ret = 0;

	mutex_lock(&pn548_dev->irq_wake_mutex);
	if (pn548_dev->irq_wake_enabled) {
		pn548_dev->irq_wake_enabled = false;
		ret = irq_set_irq_wake(pn548_dev->client->irq,0);
		if (ret) {
			pr_err("%s failed: ret=%d\n", __func__, ret);
		}
	}
	mutex_unlock(&pn548_dev->irq_wake_mutex);
}
/*
 *FUNCTION: pn548_enable_irq_wake
 *DESCRIPTION: enable irq wakeup function
 *Parameters
 * struct  pn548_dev *: device structure
 *RETURN VALUE
 * none
 */
static void pn548_enable_irq_wake(struct pn548_dev *pn548_dev)
{
	int ret = 0;

	mutex_lock(&pn548_dev->irq_wake_mutex);
	if (!pn548_dev->irq_wake_enabled) {
		pn548_dev->irq_wake_enabled = true;
		ret = irq_set_irq_wake(pn548_dev->client->irq,1);
		if (ret) {
			pr_err("%s failed: ret=%d\n", __func__, ret);
		}
	}
	mutex_unlock(&pn548_dev->irq_wake_mutex);
}

/*
 *FUNCTION: pn548_disable_irq
 *DESCRIPTION: disable irq function
 *Parameters
 * struct  pn548_dev *: device structure
 *RETURN VALUE
 * none
 */
static void pn548_disable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
	if (pn548_dev->irq_enabled) {
		disable_irq_nosync(pn548_dev->client->irq);
		pn548_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}
/*
 *FUNCTION: pn548_enable_irq
 *DESCRIPTION: enable irq function
 *Parameters
 * struct  pn548_dev *: device structure
 *RETURN VALUE
 * none
 */
static void pn548_enable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
	if (!pn548_dev->irq_enabled) {
		pn548_dev->irq_enabled = true;
		enable_irq(pn548_dev->client->irq);
	}
	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}
/*
 *FUNCTION: pn548_dev_irq_handler
 *DESCRIPTION: irq handler, jump here when receive an irq request from NFC chip
 *Parameters
 * int irq: irq number
 * void *dev_id:device structure
 *RETURN VALUE
 * irqreturn_t: irq handle result
 */
static irqreturn_t pn548_dev_irq_handler(int irq, void *dev_id)
{
	struct pn548_dev *pn548_dev = dev_id;

	if (gpio_get_value(pn548_dev->irq_gpio) != 1) {
		return IRQ_HANDLED;
	}

	pn548_disable_irq(pn548_dev);

	wake_lock_timeout(&pn548_dev->wl, 1 * HZ);

	/*set flag for do reading*/
	pn548_dev->do_reading = 1;

	/* Wake up waiting readers */
	wake_up(&pn548_dev->read_wq);

	return IRQ_HANDLED;
}
/*
 *FUNCTION: pn548_enable
 *DESCRIPTION: reset cmd sequence to enable pn548
 *Parameters
 * struct  pn548_dev *pdev: device structure
 * int mode:run mode
 *RETURN VALUE
 * none
 */
static void pn548_enable(struct  pn548_dev *pdev, int mode)
{
	/* power on */
	if (MODE_RUN == mode) {
		pr_info("%s power on\n", __func__);
		if (gpio_is_valid(pdev->firm_gpio)) {
			gpio_set_value(pdev->firm_gpio, 0);
		}
		gpio_set_value(pdev->ven_gpio, 1);
		msleep(100);
	}
	else if (MODE_FW == mode) {
		if (!gpio_is_valid(pdev->firm_gpio)) {
			pr_err("%s invalid firm_gpio, mode %d\n", __func__, mode);
			return;
		}
		/* power on with firmware download (requires hw reset)
		 */
		pr_info("%s power on with firmware\n", __func__);
		gpio_set_value(pdev->ven_gpio, 1);
		msleep(20);
		gpio_set_value(pdev->firm_gpio, 1);
		msleep(20);
		gpio_set_value(pdev->ven_gpio, 0);
		msleep(100);
		gpio_set_value(pdev->ven_gpio, 1);
		msleep(20);
	}
	else {
		pr_err("%s bad arg %d\n", __func__, mode);
		return;
	}

	return;
}

/*
 *FUNCTION: pn548_disable
 *DESCRIPTION: power off pn548
 *Parameters
 * struct  pn548_dev *pdev: device structure
 *RETURN VALUE
 * none
 */
static void pn548_disable(struct  pn548_dev *pdev)
{
	/* power off */
	pr_info("%s power off\n", __func__);
	if (gpio_is_valid(pdev->firm_gpio)) {
		gpio_set_value(pdev->firm_gpio, 0);
	}
	gpio_set_value(pdev->ven_gpio, 0);
	msleep(100);

	return;
}

/*
 *FUNCTION: pn548_dev_read
 *DESCRIPTION: read i2c data
 *Parameters
 * struct file *filp:device structure
 * char __user *buf:return to user buffer
 * size_t count:read data count
 * loff_t *offset:offset
 *RETURN VALUE
 * ssize_t: result
 */
static ssize_t pn548_dev_read(struct file *filp, char __user *buf,
			size_t count, loff_t *offset)
{
	struct pn548_dev *pn548_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;
	int retry = 0;

	/*max size is 512*/
	if (count > MAX_BUFFER_SIZE) {
		count = MAX_BUFFER_SIZE;
	}

	mutex_lock(&pn548_dev->read_mutex);

	/*read data when interrupt occur*/
	if (!gpio_get_value(pn548_dev->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto fail;
		}

		pn548_dev->do_reading = 0;
		pn548_enable_irq(pn548_dev);

		ret = wait_event_interruptible(pn548_dev->read_wq,
					pn548_dev->do_reading);

		pn548_disable_irq(pn548_dev);

		/*user cancel data read op*/
		if (pn548_dev->cancel_read) {
			pn548_dev->cancel_read = false;
			ret = -1;
			goto fail;
		}

		if (ret) {
			goto fail;
		}
	}

	/* Read data, at most tree times */
	for (retry = 0; retry < NFC_TRY_NUM; retry++) {
		ret = i2c_master_recv(pn548_dev->client, tmp, count);
		if (ret == (int)count) {
			break;
		} else {
			if (retry > 0) {
				pr_info("%s : read retry times =%d returned %d\n", __func__,retry,ret);
			}
			msleep(10);
			continue;
		}
	}

	mutex_unlock(&pn548_dev->read_mutex);

	/* pn548 seems to be slow in handling I2C read requests
	 * so add 1ms delay after recv operation */
	udelay(1000);

	if (ret < 0) {
		pr_err("%s: PN548 i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}

	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n", __func__, ret);
		return -EIO;
	}

	/*copy data to user*/
	if (copy_to_user(buf, tmp, ret)) {
		pr_warning("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}
	return ret;

fail:
	mutex_unlock(&pn548_dev->read_mutex);
	if (ret != -ERESTARTSYS)
		pr_err("%s : goto fail, and ret : %d \n", __func__, ret);

	return ret;
}
/*
 *FUNCTION: pn548_dev_write
 *DESCRIPTION: write i2c data
 *Parameters
 * struct file *filp:device structure
 * char __user *buf:user buffer to write
 * size_t count:write data count
 * loff_t *offset:offset
 *RETURN VALUE
 * ssize_t: result
 */
static ssize_t pn548_dev_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *offset)
{
	struct pn548_dev  *pn548_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;
	int retry = 0;

	/*max size is 512*/
	if (count > MAX_BUFFER_SIZE) {
		count = MAX_BUFFER_SIZE;
	}

	/*copy data from user*/
	if (copy_from_user(tmp, buf, count)) {
		pr_err("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	/* Write data */
	/* Write data, at most tree times */
	for (retry = 0; retry < NFC_TRY_NUM; retry++) {
		ret = i2c_master_send(pn548_dev->client, tmp, count);
		if (ret == (int)count) {
			break;
		} else {
			if (retry > 0) {
				pr_info("%s : send retry times =%d returned %d\n", __func__,retry,ret);
			}
			msleep(50);
			continue;
		}
	}

	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}

	/* pn548 seems to be slow in handling I2C write requests
	 * so add 1ms delay after I2C send oparation */
	udelay(1000);

	return ret;
}
/*
 *FUNCTION: pn548_reset
 *DESCRIPTION: reset pn548
 *Parameters
 * struct  pn548_dev *pdev: device structure
 *RETURN VALUE
 * none
 */
static void pn548_reset(struct  pn548_dev *pdev)
{
	/*hardware reset*/
	/* power on */
	gpio_set_value(pdev->ven_gpio, 1);
	msleep(20);

	/* power off */
	gpio_set_value(pdev->ven_gpio, 0);
	msleep(60);

	/* power on */
	gpio_set_value(pdev->ven_gpio, 1);
	msleep(20);

	return;
}
/*
 *FUNCTION: check_pn548
 *DESCRIPTION: To test if nfc chip is ok
 *Parameters
 * struct i2c_client *client:i2c device structure
 * struct  pn548_dev *pdev:device structure
 *RETURN VALUE
 * int: check result
 */
static int check_pn548(struct i2c_client *client, struct  pn548_dev *pdev)
{
	int ret = -1;
	int count = 0;
	const char host_to_pn548[1] = {0x20};
	const char firm_dload_cmd[8]={0x00, 0x04, 0xD0, 0x09, 0x00, 0x00, 0xB1, 0x84};

	/* reset chip */
	gpio_set_value(pdev->firm_gpio, 0);
	pn548_reset(pdev);

	do {
		ret = i2c_master_send(client,  host_to_pn548, sizeof(host_to_pn548));
		if (ret < 0) {
			pr_err("%s:pn548_i2c_write failed and ret = %d,at %d times\n", __func__,ret,count);
		} else {
			pr_info("%s:pn548_i2c_write success and ret = %d,at %d times\n",__func__,ret,count);
			msleep(10);
			pn548_reset(pdev);
			break;
		}
		count++;
		msleep(10);
	} while (count <NFC_TRY_NUM);

	/*In case firmware dload failed, will cause host_to_pn548 cmd send failed*/
	if (count == NFC_TRY_NUM) {
		for (count = 0; count < NFC_TRY_NUM;count++) {
			gpio_set_value(pdev->firm_gpio, 1);
			pn548_reset(pdev);

			ret = i2c_master_send(client,  firm_dload_cmd, sizeof(firm_dload_cmd));
			if (ret < 0) {
				pr_err("%s:pn548_i2c_write download cmd failed:%d, ret = %d\n", __func__, count, ret);
				continue;
			}
			gpio_set_value(pdev->firm_gpio, 0);
			pn548_reset(pdev);
			break;
		}
	}

	gpio_set_value(pdev->firm_gpio, 0);
	gpio_set_value(pdev->ven_gpio, 0);

	return ret;
}
/*
 *FUNCTION: pn548_dev_open
 *DESCRIPTION: pn548_dev_open, used by user space to enable pn548
 *Parameters
 * struct inode *inode:device inode
 * struct file *filp:device file
 *RETURN VALUE
 * int: result
 */
static int pn548_dev_open(struct inode *inode, struct file *filp)
{
	struct pn548_dev *pn548_dev = container_of(filp->private_data,
						struct pn548_dev,
						pn548_device);

	filp->private_data = pn548_dev;
	pn548_enable_irq(pn548_dev);
	pr_info("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}
/*
 *FUNCTION: pn548_dev_release
 *DESCRIPTION: pn548_dev_release, used by user space to release pn548
 *Parameters
 * struct inode *inode:device inode
 * struct file *filp:device file
 *RETURN VALUE
 * int: result
 */
static int pn548_dev_release(struct inode *inode, struct file *filp)
{
	pr_info("%s : closing %d,%d\n", __func__, imajor(inode), iminor(inode));

	return 0;
}
/*
 *FUNCTION: pn548_dev_ioctl
 *DESCRIPTION: pn548_dev_ioctl, used by user space
 *Parameters
 * struct file *filp:device file
 * unsigned int cmd:command
 * unsigned long arg:parameters
 *RETURN VALUE
 * long: result
 */
static long pn548_dev_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	struct pn548_dev *pn548_dev = filp->private_data;

	pr_info("%s, cmd=%d, arg=%lu\n", __func__, cmd, arg);

	switch (cmd) {
	case PN548_SET_PWR:
		if (2 == arg) {
			/* power on with firmware download (requires hw reset)
			 */
			pr_err("%s power on with firmware\n", __func__);
			pn548_enable(pn548_dev, MODE_FW);
		} else if (1 == arg) {
			/* power on */
			pr_err("%s power on\n", __func__);
			pn548_enable(pn548_dev, MODE_RUN);
			pn548_enable_irq_wake(pn548_dev);
			msleep(20);
		} else  if (0 == arg) {
			/* power off */
			pr_err("%s power off\n", __func__);
			pn548_disable(pn548_dev);
			pn548_disable_irq_wake(pn548_dev);
			msleep(60);
		} else if (3 == arg) {
			pr_info("%s Read Cancel\n", __func__);
			pn548_dev->cancel_read = true;
			pn548_dev->do_reading = 1;
			wake_up(&pn548_dev->read_wq);
		} else {
			pr_err("%s bad SET_PWR arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	case PN548_CLK_REQ:
		if(1 == arg){
			if(gpio_is_valid(pn548_dev->clk_req_gpio)){
				gpio_set_value(pn548_dev->clk_req_gpio, 1);
			}
		}
		else if(0 == arg) {
			if(gpio_is_valid(pn548_dev->clk_req_gpio)){
				gpio_set_value(pn548_dev->clk_req_gpio, 0);
			}
		} else {
			pr_err("%s bad CLK_REQ arg %lu\n", __func__, arg);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s bad ioctl 0x%x\n", __func__, cmd);
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations pn548_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn548_dev_read,
	.write	= pn548_dev_write,
	.open	= pn548_dev_open,
	.release   = pn548_dev_release,
	.unlocked_ioctl	= pn548_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = pn548_dev_ioctl,
#endif
};
/*
 *FUNCTION: pn548_parse_dt
 *DESCRIPTION: pn548_parse_dt, get gpio configuration from device tree system
 *Parameters
 * struct device *dev:device data
 * struct pn548_i2c_platform_data *pdata:i2c data
 *RETURN VALUE
 * int: result
 */
static int pn548_parse_dt(struct device *dev,
			struct pn548_i2c_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
 	int ret = 0;

	/*int gpio*/
	pdata->irq_gpio =  of_get_named_gpio_flags(np, "nxp,nfc_int", 0,NULL);
	if (pdata->irq_gpio < 0) {
		pr_err( "failed to get \"huawei,nfc_int\"\n");
		goto err;
	}

	/*nfc_fm_dload gpio*/
	pdata->fwdl_en_gpio = of_get_named_gpio_flags(np, "nxp,nfc_firm", 0,NULL);
	if (pdata->fwdl_en_gpio< 0) {
		pr_err( "failed to get \"huawei,nfc_firm\"\n");
		goto err;
	}

	/*nfc_ven gpio*/
	pdata->ven_gpio = of_get_named_gpio_flags(np, "nxp,nfc_ven", 0,NULL);
	if (pdata->ven_gpio < 0) {
		pr_err( "failed to get \"huawei,nfc_ven\"\n");
		goto err;
	}

	/*nfc_clk_req gpio*/
	pdata->clk_req_gpio = of_get_named_gpio(np, "nxp,nfc_clk", 0);
	if (pdata->clk_req_gpio < 0) {
		pr_err( "failed to get \"huawei,nfc_clk\"\n");
		goto err;
	}

	pr_info("%s : huawei,clk-req-gpio=%d\n",__func__,pdata->clk_req_gpio);

err:
	return ret;
}
/*
 *FUNCTION: pn548_gpio_request
 *DESCRIPTION: pn548_gpio_request, nfc gpio configuration
 *Parameters
 * struct device *dev:device data
 * struct pn548_i2c_platform_data *pdata:i2c data
 *RETURN VALUE
 * int: result
 */
static int pn548_gpio_request(struct device *dev,
			struct pn548_i2c_platform_data *pdata)
{
	int ret;

	pr_info("%s : pn548_gpio_request enter\n", __func__);

	/*NFC_INT*/
	ret = gpio_request(pdata->irq_gpio, "nfc_int");
	if (ret) {
		goto err_irq;
	}

	ret = gpio_direction_input(pdata->irq_gpio);
	if (ret) {
		goto err_fwdl_en;
	}

	/*NFC_FWDL*/
	ret = gpio_request(pdata->fwdl_en_gpio, "nfc_wake");
	if (ret) {
		goto err_fwdl_en;
	}

	ret = gpio_direction_output(pdata->fwdl_en_gpio,0);
	if (ret) {
		goto err_ven;
	}

	/*NFC_VEN*/
	ret = gpio_request(pdata->ven_gpio,"nfc_ven");
	if (ret) {
		goto err_ven;
	}
	ret = gpio_direction_output(pdata->ven_gpio, 0);
	if (ret) {
		goto err_clk_req;
	}

	/*NFC_CLKReq*/
	ret = gpio_request(pdata->clk_req_gpio,"nfc_clk_req");
	if (ret) {
		goto err_clk_req;
	}

	ret = gpio_direction_input(pdata->clk_req_gpio);
	if (ret) {
		goto err_clk_input;
	}

	return 0;

err_clk_input:
	gpio_free(pdata->clk_req_gpio);
err_clk_req:
	gpio_free(pdata->ven_gpio);
err_ven:
	gpio_free(pdata->fwdl_en_gpio);
err_fwdl_en:
	gpio_free(pdata->irq_gpio);
err_irq:

	pr_err( "%s: gpio request err %d\n", __func__, ret);
	return ret;
}
/*
 *FUNCTION: pn548_gpio_release
 *DESCRIPTION: pn548_gpio_release, release nfc gpio
 *Parameters
 * struct pn548_i2c_platform_data *pdata:i2c data
 *RETURN VALUE
 * none
 */
static void pn548_gpio_release(struct pn548_i2c_platform_data *pdata)
{
	gpio_free(pdata->ven_gpio);
	gpio_free(pdata->irq_gpio);
	gpio_free(pdata->fwdl_en_gpio);
	gpio_free(pdata->clk_req_gpio);
}
/*
 *FUNCTION: pn548_probe
 *DESCRIPTION: pn548_probe
 *Parameters
 * struct i2c_client *client:i2c client data
 * const struct i2c_device_id *id:i2c device id
 *RETURN VALUE
 * int: result
 */
static int pn548_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct clk *nfc_clk = NULL;
	struct pn548_i2c_platform_data *platform_data;
	struct pn548_dev *pn548_dev;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	platform_data = kzalloc(sizeof(struct pn548_i2c_platform_data),
				GFP_KERNEL);
	if (platform_data == NULL) {
		dev_err(&client->dev, "failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_platform_data;
	}

	/*get gpio config*/
	ret = pn548_parse_dt(&client->dev, platform_data);
	if (ret < 0) {
		dev_err(&client->dev, "failed to parse device tree: %d\n", ret);
		goto err_parse_dt;
	}

	/*config nfc clock*/
	nfc_clk  = clk_get(&client->dev, "pn548_clk");
	if (nfc_clk == NULL) {
		dev_err(&client->dev, "failed to get clk: %d\n", ret);
		goto err_parse_dt;
	}
	clk_set_rate(nfc_clk,19200000);

	ret = clk_prepare_enable(nfc_clk);
	if (ret) {
		dev_err(&client->dev, "failed to enable clk: %d\n", ret);
		goto err_gpio_request;
	}

	/*config nfc gpio*/
	ret = pn548_gpio_request(&client->dev, platform_data);
	if (ret) {
		dev_err(&client->dev, "failed to request gpio\n");
		goto err_gpio_request;
	}

	pn548_dev = kzalloc(sizeof(*pn548_dev), GFP_KERNEL);
	if (pn548_dev == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}
	client->irq = gpio_to_irq(platform_data->irq_gpio);

	pn548_dev->irq_gpio = platform_data->irq_gpio;
	pn548_dev->ven_gpio  = platform_data->ven_gpio;
	pn548_dev->firm_gpio  = platform_data->fwdl_en_gpio;
	pn548_dev->clk_req_gpio = platform_data->clk_req_gpio;
	pn548_dev->client   = client;
	pn548_dev->dev = &client->dev;
	pn548_dev->do_reading = 0;
	pn548_dev->nfc_clk = nfc_clk;
	pn548_dev->irq_wake_enabled = false;

	/*check if nfc chip is ok*/
	ret = check_pn548(client, pn548_dev);
	if (ret < 0) {
		pr_err("%s: pn548 check failed \n",__func__);
		goto err_i2c;
	}

	gpio_set_value(pn548_dev->firm_gpio, 0);
	gpio_set_value(pn548_dev->ven_gpio, 0); //0

	/* Initialise mutex and work queue */
	init_waitqueue_head(&pn548_dev->read_wq);
	mutex_init(&pn548_dev->read_mutex);
	mutex_init(&pn548_dev->irq_wake_mutex);
	spin_lock_init(&pn548_dev->irq_enabled_lock);
	wake_lock_init(&pn548_dev->wl,WAKE_LOCK_SUSPEND,"nfc_locker");

	/* register as a misc device - character based with one entry point */
	pn548_dev->pn548_device.minor = MISC_DYNAMIC_MINOR;
	pn548_dev->pn548_device.name = CHIP;
	pn548_dev->pn548_device.fops = &pn548_dev_fops;

	ret = misc_register(&pn548_dev->pn548_device);
	if (ret) {
		dev_err(&client->dev, "%s: misc_register err %d\n",
			__func__, ret);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	pr_info("%s : requesting IRQ %d\n", __func__, client->irq);
	pn548_dev->irq_enabled = true;
	ret = request_irq(client->irq, pn548_dev_irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND | IRQF_ONESHOT,
			client->name, pn548_dev);
	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}
	pn548_disable_irq(pn548_dev);
	i2c_set_clientdata(client, pn548_dev);

	pr_info("%s success.\n", __func__);
	return 0;

err_request_irq_failed:
	misc_deregister(&pn548_dev->pn548_device);
err_misc_register:
	mutex_destroy(&pn548_dev->read_mutex);
	mutex_destroy(&pn548_dev->irq_wake_mutex);
	wake_lock_destroy(&pn548_dev->wl);
	kfree(pn548_dev);
err_exit:
err_i2c:
	pn548_gpio_release(platform_data);
err_gpio_request:
	if (nfc_clk) {
		clk_put(nfc_clk);
		nfc_clk = NULL;
	}
err_parse_dt:
	kfree(platform_data);
err_platform_data:
	dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}
/*
 *FUNCTION: pn548_remove
 *DESCRIPTION: pn548_remove
 *Parameters
 * struct i2c_client *client:i2c client data
 *RETURN VALUE
 * int: result
 */
static int pn548_remove(struct i2c_client *client)
{
	struct pn548_dev *pn548_dev;

	pr_info("%s\n", __func__);

	pn548_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn548_dev);
	misc_deregister(&pn548_dev->pn548_device);
	mutex_destroy(&pn548_dev->read_mutex);
	mutex_destroy(&pn548_dev->irq_wake_mutex);
	wake_lock_destroy(&pn548_dev->wl);
	if (pn548_dev->nfc_clk) {
		clk_put(pn548_dev->nfc_clk);
		pn548_dev->nfc_clk = NULL;
	}
	gpio_free(pn548_dev->irq_gpio);
	gpio_free(pn548_dev->ven_gpio);
	gpio_free(pn548_dev->firm_gpio);
	gpio_free(pn548_dev->clk_req_gpio);
	kfree(pn548_dev);

	return 0;
}

static struct of_device_id pn548_match_table[] = {
	{ .compatible = "nxp,pn548", },
	{ },
};

MODULE_DEVICE_TABLE(of, pn548_match_table);

static const struct i2c_device_id pn548_id[] = {
	{ "pn548", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, pn548_id);

static struct i2c_driver pn548_driver = {
	.id_table	= pn548_id,
	.probe		= pn548_probe,
	.remove		= pn548_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn548",
		.of_match_table	= pn548_match_table,
	},
};
/*
 * module load/unload record keeping
 */
static int __init pn548_dev_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&pn548_driver);
}

static void __exit pn548_dev_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&pn548_driver);
}

module_init(pn548_dev_init);
module_exit(pn548_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
