/*! @file vfsSpiDrv.c
*******************************************************************************
*  SPI Driver Interface Functions
*
*  This file contains the SPI driver interface functions.
*
*  Copyright (C) 2011-2013 Validity Sensors, Inc.
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor,
*  Boston, MA  02110-1301, USA.
*
*/

#include <linux/vfsSpiDrv.h>

#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/irq.h>

#include <linux/fdtable.h>
#include <linux/eventfd.h>

#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/jiffies.h>


#define VALIDITY_PART_NAME "metallica"
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_mutex);
static struct class *vfsspi_device_class;
static int gpio_irq;

#ifdef CONFIG_OF
static struct of_device_id validity_metallica_table[] = {
	{ .compatible = "validity,metallica",},
	{ },
};
#else
#define validity_metallica_table NULL
#endif


/*
 * vfsspi_devData - The spi driver private structure
 * @devt:Device ID
 * @vfs_spi_lock:The lock for the spi device
 * @spi:The spi device
 * @device_entry:Device entry list
 * @buffer_mutex:The lock for the transfer buffer
 * @is_opened:Indicates that driver is opened
 * @buffer:buffer for transmitting data
 * @null_buffer:buffer for transmitting zeros
 * @stream_buffer:buffer for transmitting data stream
 * @stream_buffer_size:streaming buffer size
 * @drdy_pin:DRDY GPIO pin number
 * @sleep_pin:Sleep GPIO pin number
 * @user_pid:User process ID, associated with the eventfd
 *	indicating DRDY event is to be sent
 * @eventfd: eventfd which kernel uses to notify the user_pid
 *	that DRDY is asserted
 * @current_spi_speed:Current baud rate of SPI master clock
 */
struct vfsspi_device_data {
	dev_t devt;
	struct cdev cdev;
	spinlock_t vfs_spi_lock;
	struct spi_device *spi;
	struct list_head device_entry;
	struct mutex buffer_mutex;
	unsigned int is_opened;
	unsigned char *buffer;
	unsigned char *null_buffer;
	unsigned char *stream_buffer;
	size_t stream_buffer_size;
	unsigned int drdy_pin;
	unsigned int sleep_pin;
#if DO_CHIP_SELECT
	unsigned int cs_pin;
#endif
	int user_pid;
	unsigned int eventfd;
	unsigned int current_spi_speed;
	unsigned int is_drdy_irq_enabled;
	unsigned int drdy_ntf_type;
	struct mutex kernel_lock;
};

static int vfsspi_send_drdy_eventfd(struct vfsspi_device_data *vfsspi_device)
{
	struct task_struct *t;
	struct file *efd_file = NULL;
	struct eventfd_ctx *efd_ctx = NULL;
	int ret = 0;

	pr_debug("vfsspi_send_drdy_eventfd\n");
	if (vfsspi_device->user_pid != 0) {
		rcu_read_lock();
		/* find the task_struct associated with userpid */
		pr_debug("Searching task with PID=%08x\n",
			vfsspi_device->user_pid);
		t = pid_task(find_pid_ns(vfsspi_device->user_pid, &init_pid_ns),
			PIDTYPE_PID);
		if (t == NULL) {
			pr_debug("No such pid\n");
			rcu_read_unlock();
			return -ENODEV;
		}
		efd_file = fcheck_files(t->files, vfsspi_device->eventfd);
		rcu_read_unlock();
		if (efd_file == NULL) {
			pr_err("fcheck_files returned null\n");
			return -ENODEV;
		}
		efd_ctx = eventfd_ctx_fileget(efd_file);
		if (efd_ctx == NULL || efd_ctx == ERR_PTR(-EINVAL)) {
			pr_err("eventfd_ctx_fileget is failed\n");
			return -ENODEV;
		}
		eventfd_signal(efd_ctx, 1);
		eventfd_ctx_put(efd_ctx);
	}

	return ret;
}

/* Return no. of bytes written to device. Negative number for errors */
static inline ssize_t vfsspi_writeSync(struct vfsspi_device_data *vfsspi_device,
					size_t len)
{
	int    status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("vfsspi_writeSync\n");

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.rx_buf = vfsspi_device->null_buffer;
	t.tx_buf = vfsspi_device->buffer;
	t.len = len;
	t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
	status = spi_sync(vfsspi_device->spi, &m);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
	if (status == 0)
		status = m.actual_length;
	pr_debug("vfsspi_writeSync,length=%d\n", m.actual_length);
	return status;
}

/* Return no. of bytes read > 0. negative integer incase of error. */
static inline ssize_t vfsspi_readSync(struct vfsspi_device_data *vfsspi_device,
					size_t len)
{
	int    status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("vfsspi_readSync\n");

	spi_message_init(&m);
	memset(&t, 0x0, sizeof(t));

	memset(vfsspi_device->null_buffer, 0x0, len);
	t.tx_buf = vfsspi_device->null_buffer;
	t.rx_buf = vfsspi_device->buffer;
	t.len = len;
	t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
	status = spi_sync(vfsspi_device->spi, &m);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
	if (status == 0)
		status = len;

	pr_debug("vfsspi_readSync,length=%d\n", len);

	return status;
}

static ssize_t vfsspi_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *fPos)
{
	struct vfsspi_device_data *vfsspi_device = NULL;
	ssize_t               status = 0;

	pr_debug("vfsspi_write\n");

	if (count > DEFAULT_BUFFER_SIZE || count <= 0)
		return -EMSGSIZE;

	vfsspi_device = filp->private_data;

	mutex_lock(&vfsspi_device->buffer_mutex);

	if (vfsspi_device->buffer) {
		unsigned long missing = 0;

		missing = copy_from_user(vfsspi_device->buffer, buf, count);

		if (missing == 0)
			status = vfsspi_writeSync(vfsspi_device, count);
		else
			status = -EFAULT;
	}

	mutex_unlock(&vfsspi_device->buffer_mutex);

	return status;
}

static ssize_t vfsspi_read(struct file *filp, char __user *buf,
			size_t count, loff_t *fPos)
{
	struct vfsspi_device_data *vfsspi_device = NULL;
	ssize_t                status    = 0;

	pr_debug("vfsspi_read\n");

	if (count > DEFAULT_BUFFER_SIZE || count <= 0)
		return -EMSGSIZE;
	if (buf == NULL)
		return -EFAULT;


	vfsspi_device = filp->private_data;

	mutex_lock(&vfsspi_device->buffer_mutex);

	status  = vfsspi_readSync(vfsspi_device, count);


	if (status > 0) {
		unsigned long missing = 0;
		/* data read. Copy to user buffer.*/
		missing = copy_to_user(buf, vfsspi_device->buffer, status);

		if (missing == status) {
			pr_err("copy_to_user failed\n");
			/* Nothing was copied to user space buffer. */
			status = -EFAULT;
		} else {
			status = status - missing;
		}
	}

	mutex_unlock(&vfsspi_device->buffer_mutex);

	return status;
}

static int vfsspi_xfer(struct vfsspi_device_data *vfsspi_device,
			struct vfsspi_ioctl_transfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;

	pr_debug("vfsspi_xfer\n");

	if (vfsspi_device == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || tr->len <= 0)
		return -EMSGSIZE;

	if (tr->tx_buffer != NULL) {

		if (copy_from_user(vfsspi_device->null_buffer,
				tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	t.tx_buf = vfsspi_device->null_buffer;
	t.rx_buf = vfsspi_device->buffer;
	t.len = tr->len;
	t.speed_hz = vfsspi_device->current_spi_speed;

	spi_message_add_tail(&t, &m);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
	status = spi_sync(vfsspi_device->spi, &m);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
	if (status == 0) {
		if (tr->rx_buffer != NULL) {
			unsigned missing = 0;

			missing = copy_to_user(tr->rx_buffer,
					       vfsspi_device->buffer, tr->len);

			if (missing != 0)
				tr->len = tr->len - missing;
		}
	}
	pr_debug("vfsspi_xfer,length=%d\n", tr->len);
	return status;

} /* vfsspi_xfer */

static int vfsspi_rw_spi_message(struct vfsspi_device_data *vfsspi_device,
				 unsigned long arg)
{
	int err;
	struct vfsspi_ioctl_transfer transfer_request;
	if (copy_from_user(&transfer_request, (void *)arg,
			   sizeof(struct vfsspi_ioctl_transfer)) != 0) {
		pr_err("%s: Failed to copy from user\n", __func__);
		return -EFAULT;
	}
	err = vfsspi_xfer(vfsspi_device, &transfer_request);
	if (err != 0) {
		pr_err("%s: Failed xfer %d\n", __func__, err);
		return err;
	}
	if (copy_to_user((void *)arg, &transfer_request,
			 sizeof(struct vfsspi_ioctl_transfer)) != 0) {
		pr_err("%s: Failed to copy to user\n", __func__);
		return -EFAULT;
	}
	return 0;
}

static int vfsspi_set_clk(struct vfsspi_device_data *vfsspi_device,
			  unsigned long arg)
{
	unsigned short clock = 0;
	struct spi_device *spidev = NULL;

	if (copy_from_user(&clock, (void *)arg,
			   sizeof(unsigned short)) != 0)
		return -EFAULT;

	spin_lock_irq(&vfsspi_device->vfs_spi_lock);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 0);
#endif
	spidev = spi_dev_get(vfsspi_device->spi);
#if DO_CHIP_SELECT
	gpio_set_value(vfsspi_device->cs_pin, 1);
#endif
	spin_unlock_irq(&vfsspi_device->vfs_spi_lock);
	if (spidev != NULL) {
		switch (clock) {
		case 0:	/* Running baud rate. */
			pr_debug("Running baud rate.\n");
			spidev->max_speed_hz = MAX_BAUD_RATE;
			vfsspi_device->current_spi_speed = MAX_BAUD_RATE;
			break;
		case 0xFFFF: /* Slow baud rate */
			pr_debug("slow baud rate.\n");
			spidev->max_speed_hz = SLOW_BAUD_RATE;
			vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
			break;
		default:
			pr_debug("baud rate is %d.\n", clock);
			vfsspi_device->current_spi_speed =
				clock * BAUD_RATE_COEF;
			if (vfsspi_device->current_spi_speed > MAX_BAUD_RATE)
				vfsspi_device->current_spi_speed =
					MAX_BAUD_RATE;
			spidev->max_speed_hz = vfsspi_device->current_spi_speed;
			break;
		}
		spi_dev_put(spidev);
	}
	return 0;
}

static int vfsspi_register_drdy_eventfd(
				struct vfsspi_device_data *vfsspi_device,
				unsigned long arg)
{
	struct vfsspi_ioctl_register_eventfd usr_eventfd;
	if (copy_from_user(&usr_eventfd,
			(void *)arg, sizeof(usr_eventfd)) != 0) {
		pr_err("Failed copy from user.\n");
		return -EFAULT;
	} else {
		vfsspi_device->user_pid = usr_eventfd.user_pid;
		vfsspi_device->eventfd = usr_eventfd.eventfd;
	}
	return 0;
}

static irqreturn_t vfsspi_irq(int irq, void *context)
{
	struct vfsspi_device_data *vfsspi_device = context;

	if (gpio_get_value(vfsspi_device->drdy_pin) == DRDY_ACTIVE_STATUS)
		vfsspi_send_drdy_eventfd(vfsspi_device);

	return IRQ_HANDLED;
}

static int vfsspi_enableIrq(struct vfsspi_device_data *vfsspi_device)
{
	pr_debug("vfsspi_enableIrq\n");

	if (vfsspi_device->is_drdy_irq_enabled == DRDY_IRQ_ENABLE) {
		pr_debug("DRDY irq already enabled\n");
		return -EINVAL;
	}

	enable_irq(gpio_irq);
	vfsspi_device->is_drdy_irq_enabled = DRDY_IRQ_ENABLE;

	return 0;
}

static int vfsspi_disableIrq(struct vfsspi_device_data *vfsspi_device)
{
	pr_debug("vfsspi_disableIrq\n");

	if (vfsspi_device->is_drdy_irq_enabled == DRDY_IRQ_DISABLE) {
		pr_debug("DRDY irq already disabled\n");
		return -EINVAL;
	}

	disable_irq_nosync(gpio_irq);
	vfsspi_device->is_drdy_irq_enabled = DRDY_IRQ_DISABLE;

	return 0;
}

static int vfsspi_set_drdy_int(struct vfsspi_device_data *vfsspi_device,
			       unsigned long arg)
{
	unsigned short drdy_enable_flag;
	if (copy_from_user(&drdy_enable_flag, (void *)arg,
			   sizeof(drdy_enable_flag)) != 0) {
		pr_err("Failed copy from user.\n");
		return -EFAULT;
	}
	if (drdy_enable_flag == 0)
		vfsspi_disableIrq(vfsspi_device);
	else {
		vfsspi_enableIrq(vfsspi_device);
		if (gpio_get_value(vfsspi_device->drdy_pin) ==
		    DRDY_ACTIVE_STATUS) {
			vfsspi_send_drdy_eventfd(vfsspi_device);
		}
	}
	return 0;
}

static void vfsspi_hard_reset(struct vfsspi_device_data *vfsspi_device)
{
	pr_debug("vfsspi_hard_reset\n");

	if (vfsspi_device != NULL) {
		gpio_set_value(vfsspi_device->sleep_pin, 0);
		usleep(1000);
		gpio_set_value(vfsspi_device->sleep_pin, 1);
		usleep(5000);
	}
}

static void vfsspi_suspend(struct vfsspi_device_data *vfsspi_device)
{
	pr_debug("vfsspi_suspend\n");

	if (vfsspi_device != NULL) {
		spin_lock(&vfsspi_device->vfs_spi_lock);
		gpio_set_value(vfsspi_device->sleep_pin, 0);
		spin_unlock(&vfsspi_device->vfs_spi_lock);
	}
}

static long vfsspi_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	int ret_val = 0;
	struct vfsspi_device_data *vfsspi_device = NULL;

	pr_debug("vfsspi_ioctl\n");

	if (_IOC_TYPE(cmd) != VFSSPI_IOCTL_MAGIC) {
		pr_err("invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			cmd, _IOC_TYPE(cmd), VFSSPI_IOCTL_MAGIC);
		return -ENOTTY;
	}

	vfsspi_device = filp->private_data;
	mutex_lock(&vfsspi_device->buffer_mutex);
	switch (cmd) {
	case VFSSPI_IOCTL_DEVICE_RESET:
		pr_debug("VFSSPI_IOCTL_DEVICE_RESET:\n");
		vfsspi_hard_reset(vfsspi_device);
		break;
	case VFSSPI_IOCTL_DEVICE_SUSPEND:
		pr_debug("VFSSPI_IOCTL_DEVICE_SUSPEND:\n");
		vfsspi_suspend(vfsspi_device);
		break;
	case VFSSPI_IOCTL_RW_SPI_MESSAGE:
		pr_debug("VFSSPI_IOCTL_RW_SPI_MESSAGE");
		ret_val = vfsspi_rw_spi_message(vfsspi_device, arg);
		break;
	case VFSSPI_IOCTL_SET_CLK:
		pr_debug("VFSSPI_IOCTL_SET_CLK\n");
		ret_val = vfsspi_set_clk(vfsspi_device, arg);
		break;
	case VFSSPI_IOCTL_REGISTER_DRDY_EVENTFD:
		pr_debug("VFSSPI_IOCTL_REGISTER_DRDY_EVENTFD\n");
		ret_val = vfsspi_register_drdy_eventfd(vfsspi_device, arg);
		break;
	case VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE:
	{
		struct vfsspi_ioc_select_drdy_ntf_type drdy_types;
		pr_debug("VFSSPI_IOCTL_SELECT_DRDY_NTF_TYPE\n");
		if (copy_from_user(&drdy_types, (void *)arg,
			sizeof(struct vfsspi_ioc_select_drdy_ntf_type)) != 0) {
			pr_err("copy from user failed.\n");
			ret_val = -EFAULT;
		} else {
			vfsspi_device->drdy_ntf_type =
						VFSSPI_DRDY_NOTIFY_TYPE_EVENTFD;
			drdy_types.selected_type = vfsspi_device->drdy_ntf_type;
			if (copy_to_user((void *)arg, &(drdy_types),
			sizeof(struct vfsspi_ioc_select_drdy_ntf_type)) == 0) {
				ret_val = 0;
			} else {
				pr_err("copy to user failed\n");
			}
		}
	}
	break;
	case VFSSPI_IOCTL_SET_DRDY_INT:
		pr_debug("VFSSPI_IOCTL_SET_DRDY_INT\n");
		ret_val = vfsspi_set_drdy_int(vfsspi_device, arg);
		break;
	default:
		ret_val = -EFAULT;
		break;
	}
	mutex_unlock(&vfsspi_device->buffer_mutex);
	return ret_val;
}

static int vfsspi_open(struct inode *inode, struct file *filp)
{
	struct vfsspi_device_data *vfsspi_device = NULL;
	int status = -ENXIO;

	pr_debug("vfsspi_open\n");

	mutex_lock(&device_list_mutex);

	list_for_each_entry(vfsspi_device, &device_list, device_entry) {
		if (vfsspi_device->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status == 0) {
		mutex_lock(&vfsspi_device->kernel_lock);
		if (vfsspi_device->is_opened != 0) {
			status = -EBUSY;
			pr_err("vfsspi_open: is_opened != 0, -EBUSY");
			goto vfsspi_open_out;
		}
		vfsspi_device->user_pid = 0;
		if (vfsspi_device->buffer != NULL) {
			pr_err("vfsspi_open: buffer != NULL");
			goto vfsspi_open_out;
		}
		vfsspi_device->null_buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (vfsspi_device->null_buffer == NULL) {
			status = -ENOMEM;
			pr_err("vfsspi_open: null_buffer == NULL, -ENOMEM");
			goto vfsspi_open_out;
		}
		vfsspi_device->buffer =
			kmalloc(DEFAULT_BUFFER_SIZE, GFP_KERNEL);
		if (vfsspi_device->buffer == NULL) {
			status = -ENOMEM;
			kfree(vfsspi_device->null_buffer);
			pr_err("vfsspi_open: buffer == NULL, -ENOMEM");
			goto vfsspi_open_out;
		}
		vfsspi_device->is_opened = 1;
		filp->private_data = vfsspi_device;
		nonseekable_open(inode, filp);

vfsspi_open_out:
		mutex_unlock(&vfsspi_device->kernel_lock);
	}
	mutex_unlock(&device_list_mutex);
	return status;
}


static int vfsspi_release(struct inode *inode, struct file *filp)
{
	struct vfsspi_device_data *vfsspi_device = NULL;
	int                   status     = 0;

	pr_debug("vfsspi_release\n");

	mutex_lock(&device_list_mutex);
	vfsspi_device = filp->private_data;
	filp->private_data = NULL;
	vfsspi_device->is_opened = 0;
	if (vfsspi_device->buffer != NULL) {
		kfree(vfsspi_device->buffer);
		vfsspi_device->buffer = NULL;
	}

	if (vfsspi_device->null_buffer != NULL) {
		kfree(vfsspi_device->null_buffer);
		vfsspi_device->null_buffer = NULL;
	}

	mutex_unlock(&device_list_mutex);
	return status;
}

/* file operations associated with device */
static const struct file_operations vfsspi_fops = {
	.owner   = THIS_MODULE,
	.write   = vfsspi_write,
	.read    = vfsspi_read,
	.unlocked_ioctl   = vfsspi_ioctl,
	.open    = vfsspi_open,
	.release = vfsspi_release,
};

static int vfsspi_probe(struct spi_device *spi)
{
	int status = 0;
	struct vfsspi_device_data *vfsspi_device;
	struct device *dev;

	pr_info("vfsspi_probe\n");

	vfsspi_device = kzalloc(sizeof(*vfsspi_device), GFP_KERNEL);

	if (vfsspi_device == NULL)
		return -ENOMEM;

	/* Initialize driver data. */
	vfsspi_device->current_spi_speed = SLOW_BAUD_RATE;
	vfsspi_device->spi = spi;

	spin_lock_init(&vfsspi_device->vfs_spi_lock);
	mutex_init(&vfsspi_device->buffer_mutex);
	mutex_init(&vfsspi_device->kernel_lock);

	INIT_LIST_HEAD(&vfsspi_device->device_entry);

	if (vfsspi_device == NULL) {
		status = -EFAULT;
		goto vfsspi_probe_drdy_failed;
	}

	vfsspi_device->drdy_pin  = VFSSPI_DRDY_PIN;
	vfsspi_device->sleep_pin = VFSSPI_SLEEP_PIN;

	if (gpio_request(vfsspi_device->drdy_pin, "vfsspi_drdy") < 0) {
		status = -EBUSY;
		goto vfsspi_probe_drdy_failed;
	}

	if (gpio_request(vfsspi_device->sleep_pin, "vfsspi_sleep")) {
		status = -EBUSY;
		goto vfsspi_probe_sleep_failed;
	}

#if DO_CHIP_SELECT
	pr_debug("HANDLING CHIP SELECT");
	vfsspi_device->cs_pin  = VFSSPI_CS_PIN;
	if (gpio_request(vfsspi_device->cs_pin, "vfsspi_cs") < 0) {
		status = -EBUSY;
		goto vfsspi_probe_cs_failed;
	}
	status = gpio_direction_output(vfsspi_device->cs_pin, 1);
	if (status < 0) {
		pr_err("gpio_direction_input CS failed\n");
		status = -EBUSY;
		goto vfsspi_probe_gpio_init_failed;
	}
	gpio_set_value(vfsspi_device->cs_pin, 1);
#endif

	status = gpio_direction_output(vfsspi_device->sleep_pin, 1);
	if (status < 0) {
		pr_err("gpio_direction_output SLEEP failed\n");
		status = -EBUSY;
		goto vfsspi_probe_gpio_init_failed;
	}

	status = gpio_direction_input(vfsspi_device->drdy_pin);
	if (status < 0) {
		pr_err("gpio_direction_input DRDY failed\n");
		status = -EBUSY;
		goto vfsspi_probe_gpio_init_failed;
	}

	/* Check for sensor. */
	vfsspi_hard_reset(vfsspi_device);
	usleep(50000);
	if (gpio_get_value(vfsspi_device->drdy_pin) != DRDY_ACTIVE_STATUS) {
		pr_err("FPS not found\n");
		status = -EBUSY;
		goto vfsspi_probe_gpio_init_failed;
	} else {
		pr_err("FPS found\n");
	}

	gpio_irq = gpio_to_irq(vfsspi_device->drdy_pin);

	if (gpio_irq < 0) {
		pr_err("gpio_to_irq failed\n");
		status = -EBUSY;
		goto vfsspi_probe_gpio_init_failed;
	}

	if (request_irq(gpio_irq, vfsspi_irq, IRQF_TRIGGER_RISING,
			"vfsspi_irq", vfsspi_device) < 0) {
		pr_err("request_irq failed\n");
		status = -EBUSY;
		goto vfsspi_probe_irq_failed;
	}

	vfsspi_disableIrq(vfsspi_device);

	spi->bits_per_word = BITS_PER_WORD;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;

	status = spi_setup(spi);

	if (status != 0)
		goto vfsspi_probe_failed;

	mutex_lock(&device_list_mutex);
	/* Create device node */
	/* register major number for character device */
	status = alloc_chrdev_region(&(vfsspi_device->devt),
				     0, 1, VALIDITY_PART_NAME);
	if (status < 0) {
		pr_err("alloc_chrdev_region failed\n");
		goto vfsspi_probe_alloc_chardev_failed;
	}

	cdev_init(&(vfsspi_device->cdev), &vfsspi_fops);
	vfsspi_device->cdev.owner = THIS_MODULE;
	status = cdev_add(&(vfsspi_device->cdev), vfsspi_device->devt, 1);
	if (status < 0) {
		pr_err("cdev_add failed\n");
		unregister_chrdev_region(vfsspi_device->devt, 1);
		goto vfsspi_probe_cdev_add_failed;
	}

	vfsspi_device_class = class_create(THIS_MODULE, "validity_fingerprint");

	if (IS_ERR(vfsspi_device_class)) {
		pr_err("vfsspi_init: class_create() is failed - unregister chrdev.\n");
		cdev_del(&(vfsspi_device->cdev));
		unregister_chrdev_region(vfsspi_device->devt, 1);
		status = PTR_ERR(vfsspi_device_class);
		goto vfsspi_probe_class_create_failed;
	}

	dev = device_create(vfsspi_device_class, &spi->dev,
			    vfsspi_device->devt, vfsspi_device, "vfsspi");
	status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	if (status == 0)
		list_add(&vfsspi_device->device_entry, &device_list);
	mutex_unlock(&device_list_mutex);

	if (status != 0)
		goto vfsspi_probe_failed;

	spi_set_drvdata(spi, vfsspi_device);

	pr_info("vfsspi_probe successful");

	return 0;

vfsspi_probe_failed:
vfsspi_probe_class_create_failed:
	cdev_del(&(vfsspi_device->cdev));
vfsspi_probe_cdev_add_failed:
	unregister_chrdev_region(vfsspi_device->devt, 1);
vfsspi_probe_alloc_chardev_failed:
vfsspi_probe_irq_failed:
	free_irq(gpio_irq, vfsspi_device);
vfsspi_probe_gpio_init_failed:
#if DO_CHIP_SELECT
		gpio_free(vfsspi_device->cs_pin);
vfsspi_probe_cs_failed:
#endif
	gpio_free(vfsspi_device->sleep_pin);
vfsspi_probe_sleep_failed:
	gpio_free(vfsspi_device->drdy_pin);
vfsspi_probe_drdy_failed:
	mutex_destroy(&vfsspi_device->buffer_mutex);
	mutex_destroy(&vfsspi_device->kernel_lock);
	kfree(vfsspi_device);
	pr_err("vfsspi_probe failed!!\n");
	return status;
}

static int vfsspi_remove(struct spi_device *spi)
{
	int status = 0;

	struct vfsspi_device_data *vfsspi_device = NULL;

	pr_debug("vfsspi_remove\n");

	vfsspi_device = spi_get_drvdata(spi);

	if (vfsspi_device != NULL) {
		spin_lock_irq(&vfsspi_device->vfs_spi_lock);
		vfsspi_device->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&vfsspi_device->vfs_spi_lock);

		mutex_lock(&device_list_mutex);

		free_irq(gpio_irq, vfsspi_device);
#if DO_CHIP_SELECT
		gpio_free(vfsspi_device->cs_pin);
#endif
		gpio_free(vfsspi_device->sleep_pin);
		gpio_free(vfsspi_device->drdy_pin);

		/* Remove device entry. */
		list_del(&vfsspi_device->device_entry);
		device_destroy(vfsspi_device_class, vfsspi_device->devt);
		class_destroy(vfsspi_device_class);
		cdev_del(&(vfsspi_device->cdev));
		unregister_chrdev_region(vfsspi_device->devt, 1);

		mutex_destroy(&vfsspi_device->buffer_mutex);
		mutex_destroy(&vfsspi_device->kernel_lock);

		kfree(vfsspi_device);
		mutex_unlock(&device_list_mutex);
	}

	return status;
}

struct spi_driver vfsspi_spi = {
	.driver = {
		.name  = VALIDITY_PART_NAME,
		.owner = THIS_MODULE,
		.of_match_table = validity_metallica_table,
	},
		.probe  = vfsspi_probe,
		.remove = vfsspi_remove,
};

static int __init vfsspi_init(void)
{
	int status = 0;

	pr_debug("vfsspi_init\n");

	status = spi_register_driver(&vfsspi_spi);
	if (status < 0) {
		pr_err("vfsspi_init: spi_register_driver() is failed - unregister chrdev.\n");
		return status;
	}
	pr_debug("init is successful\n");

	return status;
}

static void __exit vfsspi_exit(void)
{
	pr_debug("vfsspi_exit\n");
	spi_unregister_driver(&vfsspi_spi);
}

module_init(vfsspi_init);
module_exit(vfsspi_exit);

MODULE_DESCRIPTION("Validity FPS sensor");
MODULE_LICENSE("GPL");
