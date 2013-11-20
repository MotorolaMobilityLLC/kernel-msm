/*
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/nmi326.h>

#define ISDBT_DEV_NAME		"isdbt"
#define ISDBT_DEV_MAJOR		225
#define SPI_RW_BUF		(188*50*2)

struct nmi326_data {
	struct spi_device	*spi;
	int			gpio_reset;
	int			gpio_irq;
	int			gpio_ce;
	int			gpio_enable;
	bool			enable_active_high;
	int			irq;
	bool			irq_enabled;
	spinlock_t		irq_lock;
	bool			poll_triggered;
	dev_t			isdbt_dev_no;
	struct class		*isdbt_dev_class;
	struct device		*isdbt_dev;
	struct cdev		isdbt_cdev;
	wait_queue_head_t	isdbt_waitqueue;
	struct mutex		access_mutex;
	struct regulator	*regulator;
	char			*buffer;
};

static struct nmi326_data *s_nmi326;

static int nmi326_spi_read(struct spi_device *spi, u8 *buf, size_t len)
{
	struct spi_message msg;
	struct spi_transfer transfer;
	int rc;

	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);

	transfer.tx_buf = (unsigned char *)NULL;
	transfer.rx_buf = (unsigned char *)buf;
	transfer.len = len;
	transfer.bits_per_word = 8;
	transfer.delay_usecs = 0;

	spi_message_add_tail(&transfer, &msg);
	rc = spi_sync(spi, &msg);

	if (rc < 0)
		return rc;

	return len;
}

static int nmi326_spi_write(struct spi_device *spi, u8 *buf, size_t len)
{
	struct spi_message msg;
	struct spi_transfer transfer;
	int rc;

	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);

	transfer.tx_buf = (unsigned char *)buf;
	transfer.rx_buf = (unsigned char *)NULL;
	transfer.len = len;
	transfer.bits_per_word = 8;
	transfer.delay_usecs = 0;

	spi_message_add_tail(&transfer, &msg);
	rc = spi_sync(spi, &msg);

	if (rc < 0)
		return rc;

	return len;
}

#define ID_ADDRESS	0x6400
#define WORD_ACCESS	0x70
#define READ_LEN	4

static unsigned long nmi326_spi_read_chip_id(struct spi_device *spi)
{
	unsigned char b[6];
	unsigned long adr = ID_ADDRESS;
	int retry;
	size_t len;
	unsigned char sta = 0;
	unsigned long val = 0;
	int ret_size;

	b[0] = WORD_ACCESS;
	b[1] = 0x00;
	b[2] = READ_LEN;
	b[3] = (uint8_t)(adr >> 16);
	b[4] = (uint8_t)(adr >> 8);
	b[5] = (uint8_t)(adr);
	len = 6;

	ret_size = nmi326_spi_write(spi, b, len);
	/* wait for complete */
	retry = 10;
	do {
		ret_size = nmi326_spi_read(spi, &sta, 1);

		if (sta == 0xff)
			break;
	} while (retry--);

	if (sta == 0xff) {
		/* read count */
		nmi326_spi_read(spi, b, 3);
		len = b[2] | (b[1] << 8) | (b[0] << 16);
		if (len == READ_LEN) {
			nmi326_spi_read(spi, (unsigned char *)&val, READ_LEN);
			b[0] = 0xff;
			nmi326_spi_write(spi, b, 1);

			dev_notice(&spi->dev, "NMI326 Chip ID = 0x%lx\n", val);
		} else
			dev_err(&spi->dev, "NMI326 Error bad count %d\n", len);
	} else
		dev_err(&spi->dev, "NMI326 Error, SPI bus, not complete\n");

	return val;
}

static irqreturn_t isdbt_irq_handler(int irq, void *pd)
{
	struct nmi326_data *pdata = (struct nmi326_data *)(pd);

	pdata->poll_triggered = true;
	disable_irq_nosync(pdata->irq);
	pdata->irq_enabled = false;
	wake_up(&(pdata->isdbt_waitqueue));

	return IRQ_HANDLED;
}

static int isdbt_open(struct inode *inode, struct file *filp)
{
	/* Note, this assumes only one handle will be opened at a time.
	 * If multiple simultaneous instances are needed, this can be
	 * improved so each opened instanced has its own structure, buffer,
	 * and flags.
	 */
	filp->private_data = s_nmi326;

	return 0;
}

static int isdbt_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static int isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int rc;
	struct nmi326_data *pdata = (struct nmi326_data *)(filp->private_data);

	mutex_lock(&pdata->access_mutex);

	rc = nmi326_spi_read(pdata->spi, pdata->buffer, count);
	if ((rc > 0) && copy_to_user(buf, pdata->buffer, rc))
		rc = 0;

	mutex_unlock(&pdata->access_mutex);

	return rc;
}

static int isdbt_write(struct file *filp, const char *buf, size_t count,
		loff_t *f_pos)
{
	struct nmi326_data *pdata = (struct nmi326_data *)(filp->private_data);
	int rc;

	mutex_lock(&pdata->access_mutex);

	if (copy_from_user(pdata->buffer, buf, count))
		rc = 0;
	else
		rc = nmi326_spi_write(pdata->spi, pdata->buffer, count);

	mutex_unlock(&pdata->access_mutex);

	return rc;
}

static long isdbt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long rc = 0;
	struct nmi326_data *pdata = (struct nmi326_data *)(filp->private_data);

	switch (cmd) {
	case IOCTL_ISDBT_POWER_ON:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_POWER_ON\n");
		if (pdata->regulator)
			regulator_enable(pdata->regulator);
		if (gpio_is_valid(pdata->gpio_enable))
			gpio_set_value(pdata->gpio_enable,
				pdata->enable_active_high);
		/* ideally we'd like to perform any necessary delay here,
		   but userspace appears to be already doing it */
		break;

	case IOCTL_ISDBT_POWER_OFF:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_POWER_OFF\n");
		if (gpio_is_valid(pdata->gpio_enable))
			gpio_set_value(pdata->gpio_enable,
				!pdata->enable_active_high);
		if (pdata->regulator)
			regulator_disable(pdata->regulator);
		break;

	case IOCTL_ISDBT_RST_DN:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_RST_DN\n");
		if (gpio_is_valid(pdata->gpio_reset))
			gpio_set_value(pdata->gpio_reset, 0);
		/* ideally we'd like to perform any necessary delay here,
		   but userspace appears to be already doing it */
		break;

	case IOCTL_ISDBT_RST_UP:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_RST_UP\n");
		if (gpio_is_valid(pdata->gpio_reset))
			gpio_set_value(pdata->gpio_reset, 1);
		break;

	case IOCTL_ISDBT_INTERRUPT_REGISTER:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_INTERRUPT_REGISTER\n");
		break;

	case IOCTL_ISDBT_INTERRUPT_UNREGISTER:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_INTERRUPT_UNREGISTER\n");
		break;

	case IOCTL_ISDBT_INTERRUPT_ENABLE:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_INTERRUPT_ENABLE\n");
		spin_lock_irq(&pdata->irq_lock);
		if (!pdata->irq_enabled) {
			enable_irq(pdata->irq);
			pdata->irq_enabled = true;
		}
		spin_unlock_irq(&pdata->irq_lock);
		break;

	case IOCTL_ISDBT_INTERRUPT_DISABLE:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_INTERRUPT_DISABLE\n");
		spin_lock_irq(&pdata->irq_lock);
		if (pdata->irq_enabled) {
			disable_irq_nosync(pdata->irq);
			pdata->irq_enabled = false;
		}
		spin_unlock_irq(&pdata->irq_lock);
		break;

	case IOCTL_ISDBT_INTERRUPT_DONE:
		dev_dbg(&pdata->spi->dev, "IOCTL_ISDBT_INTERRUPT_DONE\n");
		spin_lock_irq(&pdata->irq_lock);
		if (!pdata->irq_enabled) {
			enable_irq(pdata->irq);
			pdata->irq_enabled = true;
		}
		spin_unlock_irq(&pdata->irq_lock);
		break;

	default:
		dev_err(&pdata->spi->dev, "IOCTL_ISDBT INVALID!\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static unsigned int isdbt_poll(struct file *filp,
			struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct nmi326_data *pdata = (struct nmi326_data *)(filp->private_data);

	poll_wait(filp, &pdata->isdbt_waitqueue, wait);

	if (pdata->poll_triggered) {
		mask = POLLIN | POLLRDNORM;
		pdata->poll_triggered = false;
	} else {
		dev_dbg(&pdata->spi->dev, "%s timed out\n", __func__);
	}

	return mask;
}

static const struct file_operations isdbt_fops = {
	.owner		= THIS_MODULE,
	.open		= isdbt_open,
	.release	= isdbt_release,
	.read		= isdbt_read,
	.write		= isdbt_write,
	.unlocked_ioctl	= isdbt_ioctl,
	.poll		= isdbt_poll,
};

static int isdbt_init_device(struct nmi326_data *pdata)
{
	int rc;

	rc = alloc_chrdev_region(&pdata->isdbt_dev_no, 0, 1, ISDBT_DEV_NAME);

	if (rc < 0) {
		dev_err(&pdata->spi->dev, "alloc_chrdev_region failed %d\n",
			rc);
		return rc;
	}

	pdata->isdbt_dev_class = class_create(THIS_MODULE, ISDBT_DEV_NAME);
	if (IS_ERR(pdata->isdbt_dev_class)) {
		rc = PTR_ERR(pdata->isdbt_dev_class);
		dev_err(&pdata->spi->dev, "class_create failed %d\n", rc);
		goto unregister_chrdev_region;
	}

	pdata->isdbt_dev = device_create(pdata->isdbt_dev_class, NULL,
				pdata->isdbt_dev_no, pdata, ISDBT_DEV_NAME);
	if (IS_ERR(pdata->isdbt_dev)) {
		rc = PTR_ERR(pdata->isdbt_dev);
		dev_err(&pdata->spi->dev, "device_create failed %d\n", rc);
		goto class_destroy;
	}

	cdev_init(&pdata->isdbt_cdev, &isdbt_fops);
	pdata->isdbt_cdev.owner = THIS_MODULE;

	rc = cdev_add(&pdata->isdbt_cdev, MKDEV(MAJOR(pdata->isdbt_dev_no), 0),
		1);
	if (rc < 0) {
		dev_err(pdata->isdbt_dev, "cdev_add failed %d\n", rc);
		goto device_destroy;
	}

	spin_lock_init(&pdata->irq_lock);
	init_waitqueue_head(&(pdata->isdbt_waitqueue));
	pdata->irq = gpio_to_irq(pdata->gpio_irq);
	pdata->poll_triggered = false;
	set_irq_flags(pdata->irq, IRQF_VALID | IRQF_NOAUTOEN);
	pdata->irq_enabled = false;
	rc = request_irq(pdata->irq, isdbt_irq_handler, IRQF_TRIGGER_LOW,
		ISDBT_DEV_NAME, (void *)pdata);

	if (rc) {
		dev_err(pdata->isdbt_dev, "request_threaded_irq failed %d\n",
			rc);
		goto device_destroy;
	}

	return rc;

device_destroy:
	device_destroy(pdata->isdbt_dev_class, pdata->isdbt_dev_no);
class_destroy:
	class_destroy(pdata->isdbt_dev_class);
unregister_chrdev_region:
	unregister_chrdev_region(pdata->isdbt_dev_no, 1);

	return rc;
}

static int isdbt_remove_device(struct nmi326_data *pdata)
{
	free_irq(pdata->irq, pdata);
	device_destroy(pdata->isdbt_dev_class, pdata->isdbt_dev_no);
	class_destroy(pdata->isdbt_dev_class);
	unregister_chrdev_region(pdata->isdbt_dev_no, 1);

	return 0;
}

static int nmi326_probe(struct spi_device *spi)
{
	int rc;
	struct nmi326_data *pdata;

	spi->mode = (SPI_MODE_0);
	spi->bits_per_word = 8;

	rc = spi_setup(spi);
	if (rc < 0) {
		dev_err(&spi->dev, "spi_setup failed %d\n", rc);
		return rc;
	}

	pdata = devm_kzalloc(&spi->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&spi->dev, "devm_kzalloc failed\n");
		return -ENOMEM;
	}

	pdata->spi = spi;

	pdata->buffer = devm_kzalloc(&spi->dev, SPI_RW_BUF, GFP_KERNEL);
	if (!pdata->buffer) {
		dev_err(&spi->dev, "devm_kzalloc failed\n");
		return -ENOMEM;
	}

	mutex_init(&pdata->access_mutex);

	pdata->gpio_irq = of_get_named_gpio(spi->dev.of_node, "irq-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_irq)) {
		dev_err(&spi->dev, "%s: of_get_gpio failed for gpio_irq: %d\n",
			__func__, pdata->gpio_irq);
		return pdata->gpio_irq;
	}

	gpio_request(pdata->gpio_irq, "isdbt_irq");
	gpio_direction_input(pdata->gpio_irq);

	pdata->gpio_reset = of_get_named_gpio(spi->dev.of_node, "reset-gpio",
		0);
	if (gpio_is_valid(pdata->gpio_reset)) {
		gpio_request(pdata->gpio_reset, "isdbt_reset");
		gpio_direction_output(pdata->gpio_reset, 0);
		udelay(10);
	} else {
		dev_warn(&spi->dev, "%s: gpio_reset not being used\n",
			__func__);
	}

	pdata->regulator = regulator_get(&spi->dev, "isdbt_vdd");
	if (IS_ERR(pdata->regulator)) {
		pdata->regulator = NULL;
		dev_warn(&spi->dev, "%s: isdbt_vdd not specified\n", __func__);
	} else {
		regulator_enable(pdata->regulator);
	}

	pdata->gpio_ce = of_get_named_gpio(spi->dev.of_node, "ce-gpio", 0);
	if (gpio_is_valid(pdata->gpio_ce)) {
		gpio_request(pdata->gpio_ce, "isdbt_ce");
		gpio_direction_output(pdata->gpio_ce, 1);
	}

	if (gpio_is_valid(pdata->gpio_ce) || pdata->regulator)
		msleep(40);

	pdata->gpio_enable = of_get_named_gpio(spi->dev.of_node,
		"enable-gpio", 0);
	if (gpio_is_valid(pdata->gpio_enable)) {
		gpio_request(pdata->gpio_enable, "isdbt_enable");
		pdata->enable_active_high = of_property_read_bool(
			spi->dev.of_node, "enable-active-high");
		gpio_direction_output(pdata->gpio_enable,
			pdata->enable_active_high);
	}

	if (gpio_is_valid(pdata->gpio_reset)) {
		gpio_set_value(pdata->gpio_reset, 1);
		/* minimum usleep based on xo freq of 26MHz or higher */
		usleep_range(2900, 10000);
	}

	rc = nmi326_spi_read_chip_id(spi);

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 0);

	if (gpio_is_valid(pdata->gpio_enable))
		gpio_set_value(pdata->gpio_enable, !pdata->enable_active_high);

	if (pdata->regulator)
		regulator_disable(pdata->regulator);

	if (!rc) {
		rc = -ENODEV;
		goto free_gpios;
	}

	s_nmi326 = pdata;
	spi_set_drvdata(spi, pdata);
	rc = isdbt_init_device(pdata);

	if (rc < 0) {
		dev_err(&spi->dev, "isdbt_init_device failed\n");
		goto free_gpios;
	}

	return rc;

free_gpios:
	gpio_free(pdata->gpio_irq);

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

	if (gpio_is_valid(pdata->gpio_ce))
		gpio_free(pdata->gpio_ce);

	if (gpio_is_valid(pdata->gpio_enable))
		gpio_free(pdata->gpio_enable);

	return rc;
}

static int nmi326_remove(struct spi_device *spi)
{
	struct nmi326_data *pdata;

	pdata = spi_get_drvdata(spi);
	if (pdata) {
		isdbt_remove_device(pdata);
		gpio_free(pdata->gpio_irq);

		if (gpio_is_valid(pdata->gpio_reset))
			gpio_free(pdata->gpio_reset);

		if (gpio_is_valid(pdata->gpio_ce))
			gpio_free(pdata->gpio_ce);

		if (gpio_is_valid(pdata->gpio_enable))
			gpio_free(pdata->gpio_enable);
	}

	return 0;
}

static struct of_device_id nmi326_match_table[] = {
	{
		.compatible = "nmi,nmi326",
	},
	{}
};

static struct spi_driver nmi326_spi_driver = {
	.driver = {
		.name	= "isdbtspi",
		.of_match_table = nmi326_match_table,
		.owner	= THIS_MODULE,
	},
	.probe = nmi326_probe,
	.remove = __devexit_p(nmi326_remove),
};

static int __init nmi326_init(void)
{
	return spi_register_driver(&nmi326_spi_driver);
}

static void __exit nmi326_exit(void)
{
	spi_unregister_driver(&nmi326_spi_driver);
}

module_init(nmi326_init);
module_exit(nmi326_exit);

MODULE_DESCRIPTION("Driver for NMI326 ISDB-T DTV");
MODULE_AUTHOR("Motorola Mobility LLC");
