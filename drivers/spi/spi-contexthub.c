/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/cdev.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/cred.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spi/spi-contexthub.h>
#include <linux/spi/spi.h>
#include <linux/wakelock.h>

#include <asm/poll.h>
#include <asm/uaccess.h>

#define DRV_CLASS_NAME "contexthub"
#define DRV_NAME "spich"
#define SPICH_BUFFER_SIZE       4096

enum {
	SPICH_FLAG_TIMESTAMPS_ENABLED = 1,
};

struct spich_data {
	dev_t devno;
	spinlock_t spi_lock;
	struct spi_device *spi;
	struct class *class;
	struct device *device;
	struct cdev cdev;
	unsigned users;
	struct mutex buf_lock;
	u8 *buffer;
	u8 *bufferrx;
	u32 flags;
	int sh2ap_irq;
	struct completion sh2ap_completion;
	struct gpio gpio_array[4];
	struct wake_lock sh2ap_wakelock;
	struct wake_lock sh2ap_data_wakelock;

	enum {
		HUB_ACTIVE,

		/* driver has seen spich_suspend(), acquired the wakelock,
		 * told the contexthub of our intention to suspend
		 * and returned -EBUSY.
		 */
		HUB_SUSPENDING,

		/* contexthub has acknowledged our intention to suspend, will
		 * no longer send traffic other than that for wakeup-sensors.
		 * we're no longer holding the wakelock.
		 */
		HUB_SUSPENDED,

		/* driver has been resumed for a sufficient duration,
		 * acquired the wakelock and told the contexthub to resume again.
		 */
		HUB_WAKING,

	} hub_state;

	struct timer_list resume_timer;
	int force_read;
};

#define GPIO_IDX_AP2SH  0
#define GPIO_IDX_SH2AP  1
#define GPIO_IDX_BOOT0  2
#define GPIO_IDX_NRST   3

static int spich_suspend(struct spi_device *spi, pm_message_t state)
{
	struct spich_data *spich = spi_get_drvdata(spi);

	dev_dbg(&spi->dev, "spich_suspend\n");

	if ((spich->flags & SPICH_FLAG_TIMESTAMPS_ENABLED) == 0) {
		/* We're not in "contexthub" mode. Don't prevent the device
		 * from entering suspend mode. The status of the GPIO pin
		 * (SH2AP) is undefined.
		 */
		return 0;
	}

	del_timer(&spich->resume_timer);

	switch (spich->hub_state) {
		case HUB_ACTIVE:
		{
			wake_lock(&spich->sh2ap_wakelock);
			spich->hub_state = HUB_SUSPENDING;
			spich->force_read = 1;
			complete(&spich->sh2ap_completion);
			return -EBUSY;
		}

		case HUB_SUSPENDING:
		{
			/* SHOULD NEVER BE HERE, we're holding the wakelock */
			break;
		}

		case HUB_SUSPENDED:
		{
			return 0;
		}

		case HUB_WAKING:
		{
			/* SHOULD NEVER BE HERE, we're holding the wakelock */
			break;
		}
	}

	/* SHOULD NEVER BE HERE */
	return 0;
}

static int spich_resume(struct spi_device *spi)
{
	struct spich_data *spich = spi_get_drvdata(spi);

	dev_dbg(&spi->dev, "spich_resume\n");

	if ((spich->flags & SPICH_FLAG_TIMESTAMPS_ENABLED) == 0) {
		/* We're not in "contexthub" mode. Don't prevent the device
		 * from entering suspend mode. The status of the GPIO pin
		 * (SH2AP) is undefined.
		 */
		return 0;
	}

	if (spich->hub_state == HUB_SUSPENDED) {
		mod_timer(&spich->resume_timer, jiffies + msecs_to_jiffies(1000));
	}

	return 0;
}

static void spich_resume_timer_expired(unsigned long me)
{
	struct spich_data *spich = (struct spich_data *)me;

	dev_dbg(&spich->spi->dev, "spich_resume_timer_expired\n");

	switch (spich->hub_state) {
		case HUB_ACTIVE:
		{
			// SHOULD NEVER BE HERE
			break;
		}

		case HUB_SUSPENDING:
		{
			// SHOULD NEVER BE HERE
			break;
		}

		case HUB_SUSPENDED:
		{
			/* We stayed 'resumed' long enough to tell the hub to consider itself
			 * resumed as well.
			 */
			wake_lock(&spich->sh2ap_wakelock);
			spich->hub_state = HUB_WAKING;
			spich->force_read = 1;
			complete(&spich->sh2ap_completion);
			break;
		}

		case HUB_WAKING:
		{
			// SHOULD NEVER BE HERE
			break;
		}
        }
}

static void spich_hub_suspended(struct spich_data *spich, int suspended) {
	/* The hub has now acknowledged being either suspended or resumed,
	 * our driver receives this notification in one of the two transitional
	 * states, HUB_SUSPENDING or HUB_WAKING. In both cases we're currently
	 * holding the wakelock.
	 */
	if (suspended) {
		dev_dbg(&spich->spi->dev, "hub is now suspended\n");
		spich->hub_state = HUB_SUSPENDED;

		mod_timer(&spich->resume_timer, jiffies + msecs_to_jiffies(1000));
	} else {
		dev_dbg(&spich->spi->dev, "hub is now resumed\n");
		spich->hub_state = HUB_ACTIVE;
	}
	wake_unlock(&spich->sh2ap_wakelock);
}

static irqreturn_t sh2ap_isr(int irq, void *data)
{
	struct spich_data *spich = data;

	dev_dbg(&spich->spi->dev, "sh2ap_isr\n");

	/* This ISR triggered on a falling edge, so the sh2ap line is now low.
	 * We'll prevent the AP from going to suspend as long as the line is low
	 * to ensure that the client has read all available data.
	 */

	if (spich->hub_state == HUB_SUSPENDED) {
		/* We only acquire the wakelock if the hub has acknowledged
		 * its suspend state, i.e. all the traffic it sends must be
		 * important.
		 */
		wake_lock(&spich->sh2ap_data_wakelock);
	}
	complete(&spich->sh2ap_completion);

	return IRQ_HANDLED;
}

static int spich_init_instance(struct spich_data *spich)
{
	int status = 0;

	spich->buffer = kmalloc(SPICH_BUFFER_SIZE, GFP_KERNEL);
	if (!spich->buffer) {
		dev_err(&spich->spi->dev, "open/ENOMEM\n");
		status = -ENOMEM;
		goto bail;
	}

	spich->bufferrx = kmalloc(SPICH_BUFFER_SIZE, GFP_KERNEL);
	if (!spich->bufferrx) {
		dev_err(&spich->spi->dev, "open/ENOMEM\n");
		status = -ENOMEM;
		goto bail2;
	}

	status = gpio_request_array(spich->gpio_array,
				    ARRAY_SIZE(spich->gpio_array));

	if (status != 0) {
		dev_err(&spich->spi->dev, "open/gpio_request\n");
		goto bail3;
	}

	spich->sh2ap_irq = gpio_to_irq(spich->gpio_array[GPIO_IDX_SH2AP].gpio);

	init_completion(&spich->sh2ap_completion);

	wake_lock_init(
		&spich->sh2ap_wakelock, WAKE_LOCK_SUSPEND, "sh2ap_wakelock");

	wake_lock_init(
		&spich->sh2ap_data_wakelock, WAKE_LOCK_SUSPEND, "sh2ap_data_wakelock");

	status = devm_request_irq(&spich->spi->dev,
				  spich->sh2ap_irq,
				  sh2ap_isr,
				  IRQF_TRIGGER_FALLING,
				  dev_name(&spich->spi->dev), spich);

	if (status != 0) {
		dev_err(&spich->spi->dev, "open/devm_request_irq\n");
		goto bail4;
	}

	status = enable_irq_wake(spich->sh2ap_irq);

	if (status != 0) {
		dev_err(&spich->spi->dev, "open/enable_irq_wake FAILED\n");
		goto bail5;
	}

	setup_timer(
		&spich->resume_timer,
		spich_resume_timer_expired,
		(unsigned long)spich);

	spich->hub_state = HUB_ACTIVE;
	spich->force_read = 0;

	return 0;

bail5:
	devm_free_irq(&spich->spi->dev, spich->sh2ap_irq, spich);

bail4:
	wake_lock_destroy(&spich->sh2ap_data_wakelock);
	wake_lock_destroy(&spich->sh2ap_wakelock);
	gpio_free_array(spich->gpio_array, ARRAY_SIZE(spich->gpio_array));

bail3:
	kfree(spich->bufferrx);
	spich->bufferrx = NULL;

bail2:
	kfree(spich->buffer);
	spich->buffer = NULL;

bail:
	return status;
}

static void spich_destroy_instance(struct spich_data *spich)
{
	del_timer(&spich->resume_timer);

	devm_free_irq(&spich->spi->dev, spich->sh2ap_irq, spich);

	wake_lock_destroy(&spich->sh2ap_data_wakelock);
	wake_lock_destroy(&spich->sh2ap_wakelock);

	gpio_free_array(spich->gpio_array, ARRAY_SIZE(spich->gpio_array));

	kfree(spich->buffer);
	spich->buffer = NULL;

	kfree(spich->bufferrx);
	spich->bufferrx = NULL;
}

static int spich_open(struct inode *inode, struct file *filp)
{
	struct spich_data *spich;
	unsigned users;
	unsigned uid = current_uid();

	spich = container_of(inode->i_cdev, struct spich_data, cdev);

	spin_lock_irq(&spich->spi_lock);
	users = spich->users++;

	/* Processes running as root (uid 0) can always open the device;
	 * otherwise, non-root processes must wait until all other processes
	 * close the device */
	if ((uid != 0) && (users != 0)) {
		--spich->users;
		spin_unlock_irq(&spich->spi_lock);
		return -EBUSY;
	}
	spin_unlock_irq(&spich->spi_lock);

	filp->private_data = spich;
	nonseekable_open(inode, filp);

	return 0;
}

static int spich_release(struct inode *inode, struct file *filp)
{
	struct spich_data *spich;
	unsigned users;

	spich = filp->private_data;
	filp->private_data = NULL;

	spin_lock_irq(&spich->spi_lock);
	users = --spich->users;
	spin_unlock_irq(&spich->spi_lock);

	if (users == 0) {
		if (spich->spi == NULL) {
			spich_destroy_instance(spich);
			mutex_destroy(&spich->buf_lock);
			kfree(spich);
		}
	}

	return 0;
}

static void spich_complete(void *arg)
{
	complete(arg);
}

static ssize_t spich_sync(struct spich_data *spich, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;

	message->complete = spich_complete;
	message->context = &done;

	gpio_set_value(spich->gpio_array[GPIO_IDX_AP2SH].gpio, 0);

	/* Allow the context hub time to wake from stop mode. According to the
	 * spec this can take up to 138us, we choose a slightly more
	 * conservative delay. */
	udelay(250);

	spin_lock_irq(&spich->spi_lock);
	if (spich->spi == NULL) {
		status = -ESHUTDOWN;
	} else {
		status = spi_async(spich->spi, message);
	}
	spin_unlock_irq(&spich->spi_lock);

	if (status == 0) {
		wait_for_completion(&done);
		status = message->status;
		if (status == 0) {
			status = message->actual_length;
		}
	}

	gpio_set_value(spich->gpio_array[GPIO_IDX_AP2SH].gpio, 1);

	if (gpio_get_value(spich->gpio_array[GPIO_IDX_SH2AP].gpio) != 0) {
		wake_unlock(&spich->sh2ap_data_wakelock);
	}

	return status;
}

static void SET_U64(void *_data, uint64_t x)
{
	uint8_t *data = (uint8_t *) _data;
	data[0] = (x >> 56) & 0xff;
	data[1] = (x >> 48) & 0xff;
	data[2] = (x >> 40) & 0xff;
	data[3] = (x >> 32) & 0xff;
	data[4] = (x >> 24) & 0xff;
	data[5] = (x >> 16) & 0xff;
	data[6] = (x >> 8) & 0xff;
	data[7] = x & 0xff;
}

static int spich_message(struct spich_data *spich,
			 struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
	struct spi_message msg;
	struct spi_transfer *k_xfers;
	struct spi_transfer *k_tmp;
	struct spi_ioc_transfer *u_tmp;
	u8 *buf;
	u8 *bufrx;
	int status = -EFAULT;
	unsigned n;
	unsigned total;
	struct timespec t;
	uint64_t now_us;

	spi_message_init(&msg);

	k_xfers = kcalloc(n_xfers, sizeof(struct spi_transfer), GFP_KERNEL);
	if (k_xfers == NULL) {
		return -ENOMEM;
	}

	if (spich->flags & SPICH_FLAG_TIMESTAMPS_ENABLED) {
		get_monotonic_boottime(&t);
		now_us = t.tv_sec * 1000000ull + (t.tv_nsec + 500ull) / 1000ull;
	}

	buf = spich->buffer;
	bufrx = spich->bufferrx;
	total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers; n;
	     --n, ++k_tmp, ++u_tmp) {
		k_tmp->len = u_tmp->len;

		/* to prevent malicious attacks with extremely large lengths,
		 * check to make sure each transfer is less than the buffer
		 * size. This prevents overflow of 'total'.
		 */
		if (k_tmp->len > SPICH_BUFFER_SIZE) {
			status = -EMSGSIZE;
			goto done;
		}

		total += k_tmp->len;
		if (total > SPICH_BUFFER_SIZE) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			k_tmp->rx_buf = bufrx;
			if (!access_ok(VERIFY_WRITE,
				       (u8 __user *) (uintptr_t) u_tmp->rx_buf,
				       u_tmp->len)) {
				goto done;
			}
		}

		if (u_tmp->tx_buf) {
			k_tmp->tx_buf = buf;
			if (copy_from_user(buf, (const u8 __user *)(uintptr_t)
					   u_tmp->tx_buf, u_tmp->len)) {
				goto done;
			}

			if (u_tmp->len >= 10
			    && (spich->flags & SPICH_FLAG_TIMESTAMPS_ENABLED)) {
				/* Bit 7 of the cmd byte is used as an indication
				 * of whether or not the hub should considers itself
				 * suspended at the time of the transaction.
				 */
				uint8_t modified_cmd = buf[9] & 0x7f;
				if (spich->hub_state != HUB_ACTIVE
					&& spich->hub_state != HUB_WAKING) {
					modified_cmd |= 0x80;
				}

				buf[9] = modified_cmd;

				SET_U64(&buf[1], now_us);
			}
		}

		buf += k_tmp->len;
		bufrx += k_tmp->len;

		k_tmp->cs_change = ! !u_tmp->cs_change;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;

		spi_message_add_tail(k_tmp, &msg);
	}

	status = spich_sync(spich, &msg);

	if (status < 0) {
		goto done;
	}

	buf = spich->bufferrx;
	for (n = n_xfers, u_tmp = u_xfers; n; --n, ++u_tmp) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user
			    ((u8 __user *) (uintptr_t) u_tmp->rx_buf, buf,
			     u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
		}

		buf += u_tmp->len;
	}

	status = total;

done:
	kfree(k_xfers);
	return status;
}

#define SPI_MODE_MASK                           \
    (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH          \
    | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP      \
    | SPI_NO_CS | SPI_READY)

static long spich_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct spich_data *spich;
	struct spi_device *spi;

	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC) {
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		err =
		    !access_ok(VERIFY_WRITE, (void __user *)arg,
			       _IOC_SIZE(cmd));
	}

	if (err == 0 && (_IOC_DIR(cmd) & _IOC_WRITE)) {
		err =
		    !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if (err) {
		return -EFAULT;
	}

	spich = filp->private_data;
	spin_lock_irq(&spich->spi_lock);
	spi = spi_dev_get(spich->spi);
	spin_unlock_irq(&spich->spi_lock);

	if (spi == NULL) {
		return -ESHUTDOWN;
	}

	mutex_lock(&spich->buf_lock);

	switch (cmd) {
	case SPI_IOC_RESET_HUB:
		{
			u32 tmp;
			err = __get_user(tmp, (u8 __user *) arg);

			if (err != 0) {
				break;
			}

			gpio_set_value(spich->gpio_array[GPIO_IDX_BOOT0].gpio,
				       tmp ? 1 : 0);

			gpio_set_value(spich->gpio_array[GPIO_IDX_NRST].gpio,
				       0);
			mdelay(50);
			gpio_set_value(spich->gpio_array[GPIO_IDX_NRST].gpio,
				       1);

			spich->hub_state = HUB_ACTIVE;
			wake_unlock(&spich->sh2ap_wakelock);
			break;
		}

	case SPI_IOC_NOTIFY_HUB_SUSPENDED:
	{
		u32 tmp;
		err = __get_user(tmp, (u8 __user *)arg);

		if (err != 0) {
			break;
		}

		if (spich->hub_state == HUB_SUSPENDING && tmp) {
			spich_hub_suspended(spich, 1);
		} else if (spich->hub_state == HUB_WAKING && !tmp) {
			spich_hub_suspended(spich, 0);
		}
		break;
	}

	case SPI_IOC_ENABLE_TIMESTAMPS:
		{
			u32 tmp;
			err = __get_user(tmp, (u8 __user *) arg);
			if (err != 0) {
				break;
			}

			if (tmp) {
				spich->flags |= SPICH_FLAG_TIMESTAMPS_ENABLED;
			} else {
				spich->flags &= ~SPICH_FLAG_TIMESTAMPS_ENABLED;
			}
			break;
		}

	case SPI_IOC_WR_MODE:
		{
			u32 tmp;
			u8 save;

			err = __get_user(tmp, (u8 __user *) arg);
			if (err != 0) {
				break;
			}

			save = spi->mode;
			if (tmp & ~SPI_MODE_MASK) {
				err = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u8) tmp;

			err = spi_setup(spi);
			if (err < 0) {
				spi->mode = save;
			} else {
				dev_dbg(&spi->dev, "spi mode %02x\n", tmp);
			}
			break;
		}

	case SPI_IOC_WR_LSB_FIRST:
		{
			u32 tmp;
			u8 save;

			err = __get_user(tmp, (u8 __user *) arg);
			if (err != 0) {
				break;
			}

			save = spi->mode;

			if (tmp) {
				spi->mode |= SPI_LSB_FIRST;
			} else {
				spi->mode &= ~SPI_LSB_FIRST;
			}

			err = spi_setup(spi);
			if (err < 0) {
				spi->mode = save;
			} else {
				dev_dbg(&spi->dev, "%csb first\n",
					tmp ? 'l' : 'm');
			}
			break;
		}

	case SPI_IOC_WR_BITS_PER_WORD:
		{
			u32 tmp;
			u8 save;

			err = __get_user(tmp, (u8 __user *) arg);
			if (err != 0) {
				break;
			}

			save = spi->bits_per_word;

			spi->bits_per_word = tmp;

			err = spi_setup(spi);

			if (err < 0) {
				spi->bits_per_word = save;
			} else {
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
			}
			break;
		}

	case SPI_IOC_WR_MAX_SPEED_HZ:
		{
			u32 tmp;
			u32 save;

			err = __get_user(tmp, (__u32 __user *) arg);
			if (err != 0) {
				break;
			}

			save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;

			err = spi_setup(spi);

			if (err < 0) {
				spi->max_speed_hz = save;
			} else {
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
			}
			break;
		}

	case SPI_IOC_TXRX:
		{
			struct spi_ioc_transfer t;

			if (__copy_from_user(&t, (void __user *)arg, sizeof(t))) {
				err = -EFAULT;
			}

			if (gpio_get_value
			    (spich->gpio_array[GPIO_IDX_SH2AP].gpio) == 1) {
				wait_for_completion_interruptible
				    (&spich->sh2ap_completion);
			}

			if (spich->hub_state == HUB_SUSPENDED) {
			    /* If the userspace is sending some transaction,
			     * then we can tell the hub that we are awake
			     */
			    wake_lock(&spich->sh2ap_wakelock);
			    del_timer(&spich->resume_timer);
			    spich->hub_state = HUB_WAKING;
			    spich->force_read = 1;
			}

			err = spich_message(spich, &t, 1);
			break;
		}

	default:
		{
			u32 tmp;
			unsigned n_ioc;
			struct spi_ioc_transfer *ioc;

			if (_IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
			    || _IOC_DIR(cmd) != _IOC_WRITE) {
				err = -ENOTTY;
				break;
			}

			tmp = _IOC_SIZE(cmd);
			if ((tmp % sizeof(struct spi_ioc_transfer)) != 0) {
				err = -EINVAL;
				break;
			}

			n_ioc = tmp / sizeof(struct spi_ioc_transfer);
			if (n_ioc == 0) {
				break;
			}

			ioc = kmalloc(tmp, GFP_KERNEL);
			if (!ioc) {
				err = -ENOMEM;
				break;
			}

			if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
				kfree(ioc);
				err = -EFAULT;
			} else {
				err = spich_message(spich, ioc, n_ioc);
			}

			kfree(ioc);
			break;
		}
	}

	mutex_unlock(&spich->buf_lock);
	spi_dev_put(spi);

	return err;
}

#ifdef CONFIG_COMPAT
static long spich_compat_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	return spich_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spich_compat_ioctl      NULL
#endif

static ssize_t spich_sync_read(struct spich_data *spich, size_t len)
{
	struct spi_transfer t = {
		.tx_buf = spich->buffer,
		.rx_buf = spich->bufferrx,
		.len = len,
	};
	struct spi_message m;
	struct timespec ts;
	uint64_t now_us;
	ssize_t status;

	memset(spich->buffer, 0, len);
	spich->buffer[0] = 0x55;

	get_monotonic_boottime(&ts);
	now_us = ts.tv_sec * 1000000ull + (ts.tv_nsec + 500ull) / 1000ull;
	SET_U64(&spich->buffer[1], now_us);

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	status = spich_sync(spich, &m);

	if (status != 0) {
		return status;
	}

	status = len;
	return status;
}

static ssize_t spich_read(struct file *filp, char __user * buf, size_t count,
			  loff_t * f_pos)
{
	struct spich_data *spich;
	ssize_t status;

	if (count > SPICH_BUFFER_SIZE) {
		return -EMSGSIZE;
	}

	spich = filp->private_data;
	if (gpio_get_value(spich->gpio_array[GPIO_IDX_SH2AP].gpio) == 1) {
		wait_for_completion_interruptible(&spich->sh2ap_completion);
	}

	mutex_lock(&spich->buf_lock);

	status = spich_sync_read(spich, count);

	if (status > 0) {
		unsigned long missing =
		    copy_to_user(buf, spich->bufferrx, status);

		if (missing == status) {
			status = -EFAULT;
		} else {
			status -= missing;
		}
	}

	mutex_unlock(&spich->buf_lock);

	return status;
}

static unsigned int spich_poll(struct file *filp,
			       struct poll_table_struct *wait)
{
	struct spich_data *spich;
	unsigned int mask = 0;
	unsigned users;
	unsigned uid = current_uid();

	spich = filp->private_data;

	if (gpio_get_value(spich->gpio_array[GPIO_IDX_SH2AP].gpio) == 1) {
		poll_wait(filp, &spich->sh2ap_completion.wait, wait);
	}

	/* If this a non-root process and there is another (root) user,
	 * then only the root user gets access to SPI traffic */
	spin_lock_irq(&spich->spi_lock);
	users = spich->users;
	if ((uid != 0) && (users > 1)) {
		spin_unlock_irq(&spich->spi_lock);
		return 0;
	}
	spin_unlock_irq(&spich->spi_lock);

	if (gpio_get_value(spich->gpio_array[GPIO_IDX_SH2AP].gpio) == 0
		|| spich->force_read) {
		mask |= POLLIN | POLLRDNORM;

		spich->force_read = 0;
	}

	return mask;
}

static const struct file_operations spich_fops = {
	.owner = THIS_MODULE,

	.unlocked_ioctl = spich_ioctl,
	.compat_ioctl = spich_compat_ioctl,
	.open = spich_open,
	.release = spich_release,
	.llseek = no_llseek,
	.read = spich_read,
	.poll = spich_poll,
};

static int spich_probe(struct spi_device *spi)
{
	struct spich_data *spich;
	int error = 0;
	int i;

	spich = kzalloc(sizeof(struct spich_data), GFP_KERNEL);

	if (!spich) {
		dev_err(&spi->dev, "memory allocation error!.\n");
		return -ENOMEM;
	}

	spich->spi = spi;
	spin_lock_init(&spich->spi_lock);
	mutex_init(&spich->buf_lock);

	spich->gpio_array[GPIO_IDX_AP2SH].flags = GPIOF_OUT_INIT_HIGH;
	spich->gpio_array[GPIO_IDX_AP2SH].label = "contexthub,ap2sh";
	spich->gpio_array[GPIO_IDX_SH2AP].flags = GPIOF_DIR_IN;
	spich->gpio_array[GPIO_IDX_SH2AP].label = "contexthub,sh2ap";
	spich->gpio_array[GPIO_IDX_BOOT0].flags = GPIOF_OUT_INIT_LOW;
	spich->gpio_array[GPIO_IDX_BOOT0].label = "contexthub,boot0";
	spich->gpio_array[GPIO_IDX_NRST].flags = GPIOF_OUT_INIT_HIGH;
	spich->gpio_array[GPIO_IDX_NRST].label = "contexthub,nrst";

	spich->class = class_create(THIS_MODULE, DRV_CLASS_NAME);
	if (IS_ERR(spich->class)) {
		dev_err(&spich->spi->dev, "failed to create class.\n");
		error = PTR_ERR(spich->class);
		goto err_class_create;
	}

	error = alloc_chrdev_region(&spich->devno, 0, 1, DRV_NAME);
	if (error < 0) {
		dev_err(&spich->spi->dev, "alloc_chrdev_region failed.\n");
		goto err_alloc_chrdev;
	}

	spich->device = device_create(spich->class, NULL, spich->devno,
				      NULL, "%s", DRV_NAME);

	if (IS_ERR(spich->device)) {
		dev_err(&spich->spi->dev, "device_create failed.\n");
		error = PTR_ERR(spich->device);
		goto err_device_create;
	}

	cdev_init(&spich->cdev, &spich_fops);
	spich->cdev.owner = THIS_MODULE;

	error = cdev_add(&spich->cdev, spich->devno, 1);
	if (error) {
		dev_err(&spich->spi->dev, "cdev_add failed.\n");
		goto err_device_create;
	}

	for (i = 0; i < ARRAY_SIZE(spich->gpio_array); i++) {
		int gpio;

		gpio = of_get_named_gpio(spich->spi->dev.of_node,
					 spich->gpio_array[i].label, 0);
		if (gpio < 0) {
			dev_err(&spich->spi->dev,
				"failed to find %s property in DT\n",
				spich->gpio_array[i].label);
			goto err_device_create;
		}
		spich->gpio_array[i].gpio = gpio;
		dev_info(&spich->spi->dev, "%s: %s=%u\n",
			 __func__, spich->gpio_array[i].label,
			 spich->gpio_array[i].gpio);
	}

	device_init_wakeup(&spi->dev, 1 /* wakeup */);

	error = spich_init_instance(spich);
	if (error) {
		dev_err(&spich->spi->dev, "spich_init_instance failed.\n");
		goto err_spich_init_instance;
	}

	spi_set_drvdata(spi, spich);

	return 0;

err_spich_init_instance:
	cdev_del(&spich->cdev);

err_device_create:
	device_destroy(spich->class, spich->devno);

err_alloc_chrdev:
	class_destroy(spich->class);

err_class_create:
	kfree(spich);

	return error;
}

static int spich_remove(struct spi_device *spi)
{
	struct spich_data *spich = spi_get_drvdata(spi);
	unsigned users;

	device_init_wakeup(&spi->dev, 0 /* wakeup */);

	cdev_del(&spich->cdev);
	device_destroy(spich->class, spich->devno);
	class_destroy(spich->class);

	spin_lock_irq(&spich->spi_lock);
	spich->spi = NULL;
	users = spich->users;
	spi_set_drvdata(spi, NULL);
	spin_unlock_irq(&spich->spi_lock);

	if (users == 0) {
		spich_destroy_instance(spich);
		mutex_destroy(&spich->buf_lock);
		kfree(spich);
	}

	return 0;
}

static const struct of_device_id spich_dt_ids[] = {
	{.compatible = "contexthub," DRV_NAME},
	{},
};

MODULE_DEVICE_TABLE(of, spich_dt_ids);

static struct spi_driver spich_spi_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(spich_dt_ids),
		   },
	.probe = spich_probe,
	.remove = spich_remove,
	.suspend = spich_suspend,
	.resume = spich_resume,
};

static int __init spich_init(void)
{
	pr_info(DRV_NAME ": %s\n", __func__);

	if (spi_register_driver(&spich_spi_driver)) {
		pr_err(DRV_NAME ": failed to spi_register_driver\n");
		return -EINVAL;
	}
	return 0;
}

static void __exit spich_exit(void)
{
	pr_info(DRV_NAME ": %s\n", __func__);

	spi_unregister_driver(&spich_spi_driver);
}

module_init(spich_init);
module_exit(spich_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Google");
MODULE_DESCRIPTION("User mode SPI context hub interface");
