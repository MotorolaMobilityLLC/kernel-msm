/*
 * Copyright (C) 2016 Google, Inc.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/platform_data/nanohub.h>

#include "main.h"
#include "comms.h"
#include "bl.h"
#include "spi.h"

#define READ_QUEUE_DEPTH	10
#define OS_LOG_EVENTID		0x3B474F4C
#define FIRST_SENSOR_EVENTID	0x00000200
#define LAST_SENSOR_EVENTID	0x000002FF
#define WAKEUP_INTERRUPT	1

static int nanohub_open(struct inode *, struct file *);
static ssize_t nanohub_read(struct file *, char *, size_t, loff_t *);
static ssize_t nanohub_write(struct file *, const char *, size_t, loff_t *);
static unsigned int nanohub_poll(struct file *, poll_table *);
static int nanohub_release(struct inode *, struct file *);

static const struct iio_info nanohub_iio_info = {
	.driver_module = THIS_MODULE,
};

static const struct file_operations nanohub_fileops = {
	.owner = THIS_MODULE,
	.open = nanohub_open,
	.read = nanohub_read,
	.write = nanohub_write,
	.poll = nanohub_poll,
	.release = nanohub_release,
};

int request_wakeup(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;
	int ret = 0, lock;
	DEFINE_WAIT(wait);

	spin_lock(&data->wakeup_lock);
	if (atomic_inc_return(&data->wakeup_cnt) == 1)
		gpio_direction_output(pdata->wakeup_gpio, 0);
	spin_unlock(&data->wakeup_lock);

	/* if the interrupt line is low, just try and grab the lock */
	if (!gpio_get_value(data->pdata->irq1_gpio))
		lock = atomic_cmpxchg(&data->wakeup_aquired, 0, 1);
	else
		lock = 1;

	if (lock) {
		for (;;) {
			prepare_to_wait_exclusive(&data->wakeup_wait, &wait,
						  TASK_INTERRUPTIBLE);
			if (!gpio_get_value(data->pdata->irq1_gpio)) {
				lock = atomic_cmpxchg(&data->wakeup_aquired, 0, 1);
				if (!lock) {
					finish_wait(&data->wakeup_wait, &wait);
					break;
				}
			}
			if (!signal_pending(current)) {
				schedule();
				continue;
			}
			ret = -ERESTARTSYS;
			abort_exclusive_wait(&data->wakeup_wait, &wait,
					     TASK_INTERRUPTIBLE, NULL);
			break;
		}
	}

	if (ret) {
		spin_lock(&data->wakeup_lock);
		if (atomic_dec_and_test(&data->wakeup_cnt))
			gpio_direction_output(pdata->wakeup_gpio, 1);
		spin_unlock(&data->wakeup_lock);
	}

	return ret;
}

int request_wakeup_timeout(struct nanohub_data *data, long timeout)
{
	const struct nanohub_platform_data *pdata = data->pdata;
	int lock;
	DEFINE_WAIT(wait);

	spin_lock(&data->wakeup_lock);
	if (atomic_inc_return(&data->wakeup_cnt) == 1)
		gpio_direction_output(pdata->wakeup_gpio, 0);
	spin_unlock(&data->wakeup_lock);

	/* if the interrupt line is low, just try and grab the lock */
	if (!gpio_get_value(data->pdata->irq1_gpio))
		lock = atomic_cmpxchg(&data->wakeup_aquired, 0, 1);
	else
		lock = 1;

	if (lock) {
		for (;;) {
			prepare_to_wait_exclusive(&data->wakeup_wait, &wait,
						  TASK_INTERRUPTIBLE);
			if (!gpio_get_value(data->pdata->irq1_gpio)) {
				lock = atomic_cmpxchg(&data->wakeup_aquired, 0, 1);
				if (!lock) {
					finish_wait(&data->wakeup_wait, &wait);
					break;
				}
			}
			if (!signal_pending(current)) {
				timeout = schedule_timeout(timeout);
				if (!timeout) {
					finish_wait(&data->wakeup_wait, &wait);
					break;
				}
				continue;
			}
			timeout = -ERESTARTSYS;
			abort_exclusive_wait(&data->wakeup_wait, &wait,
					     TASK_INTERRUPTIBLE, NULL);
			break;
		}

		if (!timeout
		    && (!lock || !gpio_get_value(data->pdata->irq1_gpio))) {
			if (lock)
				lock = atomic_cmpxchg(&data->wakeup_aquired, 0, 1);
			if (!lock)
				timeout = 1;
		}
	} else {
		timeout = 1;
	}

	if (timeout <= 0) {
		spin_lock(&data->wakeup_lock);
		if (atomic_dec_and_test(&data->wakeup_cnt))
			gpio_direction_output(pdata->wakeup_gpio, 1);
		spin_unlock(&data->wakeup_lock);

		if (timeout == 0)
			timeout = -ETIME;
	} else {
		timeout = 0;
	}

	return timeout;
}

void release_wakeup(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	spin_lock(&data->wakeup_lock);
	if (!atomic_dec_and_test(&data->wakeup_cnt)) {
		gpio_direction_output(pdata->wakeup_gpio, 0);
		spin_unlock(&data->wakeup_lock);
		atomic_dec(&data->wakeup_aquired);
		if (!gpio_get_value(data->pdata->irq1_gpio))
			wake_up_interruptible_sync(&data->wakeup_wait);
	} else {
		gpio_direction_output(pdata->wakeup_gpio, 1);
		spin_unlock(&data->wakeup_lock);
		atomic_dec(&data->wakeup_aquired);
		if (!gpio_get_value(data->pdata->irq1_gpio) ||
		    (gpio_is_valid(data->pdata->irq2_gpio)
		     && !gpio_get_value(data->pdata->irq2_gpio))) {
			atomic_set(&data->kthread_run, 1);
			wake_up_interruptible_sync(&data->kthread_wait);
		}
	}
}

static void nanohub_mask_interrupt(struct nanohub_data *data, uint8_t interrupt)
{
	int ret;
	uint8_t mask_ret;
	int cnt = 10;

	do {
		if (request_wakeup(data))
			return;
		ret =
		    nanohub_comms_tx_rx_retrans(data, CMD_COMMS_MASK_INTR,
						&interrupt, 1,
						(uint8_t *)&mask_ret, 1,
						false, 10, 0);
		release_wakeup(data);
		dev_info(data->sensor_dev,
			 "Masking interrupt %d, ret=%d, mask_ret=%d\n",
			 interrupt, ret, mask_ret);
	} while ((ret != 1 || mask_ret != 1) && --cnt > 0);
}

static void nanohub_unmask_interrupt(struct nanohub_data *data,
				     uint8_t interrupt)
{
	int ret;
	uint8_t unmask_ret;
	int cnt = 10;

	do {
		if (request_wakeup_timeout(data, msecs_to_jiffies(1000)))
			return;
		ret =
		    nanohub_comms_tx_rx_retrans(data, CMD_COMMS_UNMASK_INTR,
						&interrupt, 1,
						(uint8_t *)&unmask_ret, 1,
						false, 10, 0);
		release_wakeup(data);
		dev_info(data->sensor_dev,
			 "Unmasking interrupt %d, ret=%d, mask_ret=%d\n",
			 interrupt, ret, unmask_ret);
	} while ((ret != 1 || unmask_ret != 1) && --cnt > 0);
}

static ssize_t nanohub_wakeup_query(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	const struct nanohub_platform_data *pdata = data->pdata;

	data->err_cnt = 0;
	if (((gpio_is_valid(data->pdata->irq1_gpio)
	      && gpio_get_value(data->pdata->irq1_gpio) == 0)
	     || (gpio_is_valid(data->pdata->irq2_gpio)
		 && gpio_get_value(data->pdata->irq2_gpio) == 0)))
		wake_up_interruptible(&data->wakeup_wait);

	return scnprintf(buf, PAGE_SIZE, "WAKEUP: %d INT1: %d INT2: %d\n",
			 gpio_get_value(pdata->wakeup_gpio),
			 gpio_is_valid(pdata->
				       irq1_gpio) ? gpio_get_value(pdata->
								   irq1_gpio) :
			 -1,
			 gpio_is_valid(pdata->
				       irq2_gpio) ? gpio_get_value(pdata->
								   irq2_gpio) :
			 -1);
}

static ssize_t nanohub_app_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	struct {
		uint64_t appId;
		uint32_t appVer;
		uint32_t appSize;
	} __packed buffer;
	uint32_t i = 0;
	int ret;
	ssize_t len = 0;

	do {
		if (request_wakeup(data))
			return -ERESTARTSYS;

		if (nanohub_comms_tx_rx_retrans
		    (data, CMD_COMMS_QUERY_APP_INFO, (uint8_t *)&i,
		     sizeof(uint32_t), (uint8_t *)&buffer, sizeof(buffer),
		     false, 10, 10) == sizeof(buffer)) {
			ret =
			    scnprintf(buf + len, PAGE_SIZE - len,
				      "app: %d id: %016llx ver: %08x size: %08x\n",
				      i, buffer.appId, buffer.appVer,
				      buffer.appSize);
			if (ret > 0) {
				len += ret;
				i++;
			}
		} else {
			ret = -1;
		}

		release_wakeup(data);
	} while (ret > 0);

	return len;
}

static ssize_t nanohub_firmware_query(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	uint16_t buffer[6];

	if (request_wakeup(data))
		return -ERESTARTSYS;

	if (nanohub_comms_tx_rx_retrans
	    (data, CMD_COMMS_GET_OS_HW_VERSIONS, NULL, 0, (uint8_t *)&buffer,
	     sizeof(buffer), false, 10, 10) == sizeof(buffer)) {
		release_wakeup(data);
		return scnprintf(buf, PAGE_SIZE,
				 "hw type: %04x hw ver: %04x bl ver: %04x os ver: %04x variant ver: %08x\n",
				 buffer[0], buffer[1], buffer[2], buffer[3],
				 buffer[5] << 16 | buffer[4]);
	} else {
		release_wakeup(data);
		return 0;
	}
}

static ssize_t nanohub_hw_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	const struct nanohub_platform_data *pdata = data->pdata;

	atomic_inc(&data->wakeup_aquired);

	if (gpio_is_valid(pdata->irq1_gpio))
		disable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		disable_irq(data->irq2);
	if (atomic_read(&data->wakeup_cnt) > 0
	    && gpio_is_valid(pdata->wakeup_gpio))
		gpio_direction_output(pdata->wakeup_gpio, 1);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 0);
	data->err_cnt = 0;
	udelay(30);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 1);
	usleep_range(750000, 800000);
	if (gpio_is_valid(pdata->irq1_gpio))
		enable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		enable_irq(data->irq2);

	atomic_dec(&data->wakeup_aquired);

	if (atomic_read(&data->wakeup_cnt) > 0
	    && gpio_is_valid(pdata->wakeup_gpio))
		gpio_direction_output(pdata->wakeup_gpio, 0);
	if (!gpio_is_valid(pdata->irq2_gpio))
		nanohub_unmask_interrupt(data, 2);

	return count;
}

static ssize_t nanohub_erase_shared(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	const struct nanohub_platform_data *pdata = data->pdata;
	uint8_t status = CMD_ACK;

	atomic_inc(&data->wakeup_aquired);

	nanohub_bl_open(data);
	if (gpio_is_valid(pdata->irq1_gpio))
		disable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		disable_irq(data->irq2);
	if (atomic_read(&data->wakeup_cnt) > 0
	    && gpio_is_valid(pdata->wakeup_gpio))
		gpio_direction_output(pdata->wakeup_gpio, 1);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 0);
	if (gpio_is_valid(pdata->boot0_gpio))
		gpio_direction_output(pdata->boot0_gpio, 1);
	data->err_cnt = 0;
	udelay(30);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 1);

	usleep_range(70000, 75000);
	status = nanohub_bl_erase_shared(data);
	dev_info(dev, "nanohub_bl_erase_shared: status=%02x\n",
		 status);

	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 0);
	if (gpio_is_valid(pdata->boot0_gpio))
		gpio_direction_output(pdata->boot0_gpio, 0);
	nanohub_bl_close(data);
	udelay(30);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 1);
	usleep_range(750000, 800000);
	if (gpio_is_valid(pdata->irq1_gpio))
		enable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		enable_irq(data->irq2);

	atomic_dec(&data->wakeup_aquired);

	if (atomic_read(&data->wakeup_cnt) > 0
	    && gpio_is_valid(pdata->wakeup_gpio))
		gpio_direction_output(pdata->wakeup_gpio, 0);
	if (!gpio_is_valid(pdata->irq2_gpio))
		nanohub_unmask_interrupt(data, 2);

	return count;
}

static ssize_t nanohub_download_bl(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	const struct nanohub_platform_data *pdata = data->pdata;
	const struct firmware *fw_entry;
	int ret;
	uint8_t status = CMD_ACK;

	atomic_inc(&data->wakeup_aquired);

	nanohub_bl_open(data);
	if (gpio_is_valid(pdata->irq1_gpio))
		disable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		disable_irq(data->irq2);
	if (atomic_read(&data->wakeup_cnt) > 0
	    && gpio_is_valid(pdata->wakeup_gpio))
		gpio_direction_output(pdata->wakeup_gpio, 1);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 0);
	if (gpio_is_valid(pdata->boot0_gpio))
		gpio_direction_output(pdata->boot0_gpio, 1);
	data->err_cnt = 0;
	udelay(30);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 1);

	ret = request_firmware(&fw_entry, "nanohub.full.bin", dev);
	if (ret) {
		dev_err(dev, "nanohub_download_bl: err=%d\n", ret);
	} else {
		usleep_range(70000, 75000);
		status =
		    nanohub_bl_download(data, pdata->bl_addr, fw_entry->data,
					fw_entry->size);
		dev_info(dev, "nanohub_download_bl: status=%02x\n",
			 status);
		release_firmware(fw_entry);
	}

	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 0);
	if (gpio_is_valid(pdata->boot0_gpio))
		gpio_direction_output(pdata->boot0_gpio, 0);
	nanohub_bl_close(data);
	udelay(30);
	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 1);
	usleep_range(750000, 800000);
	if (gpio_is_valid(pdata->irq1_gpio))
		enable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		enable_irq(data->irq2);

	atomic_dec(&data->wakeup_aquired);

	if (atomic_read(&data->wakeup_cnt) > 0
	    && gpio_is_valid(pdata->wakeup_gpio))
		gpio_direction_output(pdata->wakeup_gpio, 0);
	if (!gpio_is_valid(pdata->irq2_gpio))
		nanohub_unmask_interrupt(data, 2);

	return count;
}

static ssize_t nanohub_download_kernel(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	const struct firmware *fw_entry;
	int ret;

	ret = request_firmware(&fw_entry, "nanohub.update.bin", dev);
	if (ret) {
		dev_err(dev, "nanohub_download_kernel: err=%d\n", ret);
		return -EIO;
	} else {
		ret =
		    nanohub_comms_kernel_download(data, fw_entry->data,
						  fw_entry->size);

		release_firmware(fw_entry);

		return count;
	}

}

static ssize_t nanohub_download_app(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_drvdata(dev);
	const struct firmware *fw_entry;
	char buffer[70];
	int i, ret, ret1, ret2, file_len = 0, appid_len = 0, ver_len = 0;
	const char *appid = NULL, *ver = NULL;
	unsigned long version;
	uint64_t id;
	uint32_t cur_version;
	bool update = true;

	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			if (i + 1 == count) {
				break;
			} else {
				if (appid == NULL)
					appid = buf + i + 1;
				else if (ver == NULL)
					ver = buf + i + 1;
				else
					break;
			}
		} else if (buf[i] == '\n' || buf[i] == '\r') {
			break;
		} else {
			if (ver)
				ver_len++;
			else if (appid)
				appid_len++;
			else
				file_len++;
		}
	}

	if (file_len > 64 || appid_len > 16 || ver_len > 8 || file_len < 1)
		return -EIO;

	memcpy(buffer, buf, file_len);
	memcpy(buffer + file_len, ".napp", 5);
	buffer[file_len + 5] = '\0';

	ret = request_firmware(&fw_entry, buffer, dev);
	if (ret) {
		dev_err(dev, "nanohub_download_app(%s): err=%d\n",
			buffer, ret);
		return -EIO;
	} else {
		if (appid_len > 0 && ver_len > 0) {
			memcpy(buffer, appid, appid_len);
			buffer[appid_len] = '\0';

			ret1 = kstrtoull(buffer, 16, &id);

			memcpy(buffer, ver, ver_len);
			buffer[ver_len] = '\0';

			ret2 = kstrtoul(buffer, 16, &version);

			if (ret1 == 0 && ret2 == 0) {
				if (request_wakeup(data))
					return -ERESTARTSYS;
				if (nanohub_comms_tx_rx_retrans
				    (data, CMD_COMMS_GET_APP_VERSIONS,
				     (uint8_t *)&id, sizeof(id),
				     (uint8_t *)&cur_version,
				     sizeof(cur_version), false, 10,
				     10) == sizeof(cur_version)) {
					if (cur_version == version)
						update = false;
				}
				release_wakeup(data);
			}
		}

		if (update)
			ret =
			    nanohub_comms_app_download(data, fw_entry->data,
						       fw_entry->size);

		release_firmware(fw_entry);

		return count;
	}
}

static struct device_attribute attributes[] = {
	__ATTR(wakeup, 0440, nanohub_wakeup_query, NULL),
	__ATTR(app_info, 0440, nanohub_app_info, NULL),
	__ATTR(firmware_version, 0440, nanohub_firmware_query, NULL),
	__ATTR(download_bl, 0220, NULL, nanohub_download_bl),
	__ATTR(download_kernel, 0220, NULL, nanohub_download_kernel),
	__ATTR(download_app, 0220, NULL, nanohub_download_app),
	__ATTR(erase_shared, 0220, NULL, nanohub_erase_shared),
	__ATTR(reset, 0220, NULL, nanohub_hw_reset),
};

static int create_sysfs(struct nanohub_data *data)
{
	int i, ret;
	dev_t dev;

	data->sensor_class = class_create(THIS_MODULE, "nanohub");
	if (IS_ERR(data->sensor_class)) {
		pr_err("nanohub: class_create failed\n");
		return PTR_ERR(data->sensor_class);
	}

	alloc_chrdev_region(&dev, 0, 1, "nanohub");
	cdev_init(&data->cdev, &nanohub_fileops);
	data->cdev.owner = THIS_MODULE;
	cdev_add(&data->cdev, dev, 1);

	data->sensor_dev = device_create(data->sensor_class, NULL, dev, data,
					 "%s", "nanohub");

	if (IS_ERR(data->sensor_dev)) {
		pr_err("nanohub: device_create failed\n");
		class_destroy(data->sensor_class);
		return PTR_ERR(data->sensor_dev);
	}

	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(data->sensor_dev, &attributes[i]);
		if (ret) {
			dev_err(data->sensor_dev,
				"device_create_file failed\n");
			device_unregister(data->sensor_dev);
			class_destroy(data->sensor_class);
			return ret;
		}
	}

	ret =
	    sysfs_create_link(&data->sensor_dev->kobj, &data->iio_dev->dev.kobj,
			      "iio");
	if (ret) {
		dev_err(data->sensor_dev,
			"sysfs_create_link failed\n");
		device_unregister(data->sensor_dev);
		class_destroy(data->sensor_class);
		return ret;
	}

	return 0;
}

static int nanohub_open(struct inode *inode, struct file *file)
{
	struct nanohub_data *data;

	nonseekable_open(inode, file);

	data = container_of(inode->i_cdev, struct nanohub_data, cdev);

	file->private_data = data;
	return 0;
}

static ssize_t nanohub_read(struct file *file, char *buffer, size_t length,
			    loff_t *offset)
{
	struct nanohub_data *data = file->private_data;
	struct nanohub_buf *buf;
	int ret;

	if (atomic_read(&data->read_cnt) == 0) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible
		    (data->read_wait, atomic_read(&data->read_cnt)))
			return -ERESTARTSYS;
	}

	atomic_dec(&data->read_cnt);

	spin_lock(&data->read_lock);
	buf = list_entry(data->read_data.next, struct nanohub_buf, list);
	list_del(&buf->list);
	spin_unlock(&data->read_lock);

	ret = copy_to_user(buffer, buf->buffer, buf->length);
	if (ret != 0)
		ret = -EFAULT;
	else
		ret = buf->length;

	spin_lock(&data->read_lock);
	list_add_tail(&buf->list, &data->read_free);
	spin_unlock(&data->read_lock);

	atomic_inc(&data->read_free_cnt);
	atomic_set(&data->kthread_run, 1);
	wake_up_interruptible_sync(&data->kthread_wait);

	return ret;
}

static ssize_t nanohub_write(struct file *file, const char *buffer,
			     size_t length, loff_t *offset)
{
	struct nanohub_data *data = file->private_data;
	int ret;

	if (request_wakeup(data))
		return -ERESTARTSYS;

	ret = nanohub_comms_write(data, buffer, length);

	release_wakeup(data);

	return ret;
}

static unsigned int nanohub_poll(struct file *file, poll_table *wait)
{
	struct nanohub_data *data = file->private_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(file, &data->read_wait, wait);

	if (atomic_read(&data->read_cnt))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int nanohub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static void destroy_sysfs(struct nanohub_data *data)
{
	int i;
	dev_t dev;

	sysfs_remove_link(&data->sensor_dev->kobj, "iio");
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(data->sensor_dev, &attributes[i]);
	device_unregister(data->sensor_dev);
	dev = data->cdev.dev;
	cdev_del(&data->cdev);
	unregister_chrdev_region(dev, 1);
	class_destroy(data->sensor_class);
}

static irqreturn_t nanohub_irq1(int irq, void *dev_id)
{
	struct nanohub_data *data = (struct nanohub_data *)dev_id;

	if (atomic_read(&data->wakeup_cnt) == 0) {
		atomic_set(&data->kthread_run, 1);
		wake_up_interruptible_sync(&data->kthread_wait);
	} else {
		wake_up_interruptible_sync(&data->wakeup_wait);
	}

	return IRQ_HANDLED;
}

static irqreturn_t nanohub_irq2(int irq, void *dev_id)
{
	struct nanohub_data *data = (struct nanohub_data *)dev_id;

	atomic_set(&data->kthread_run, 1);
	wake_up_interruptible_sync(&data->kthread_wait);

	return IRQ_HANDLED;
}

static bool nanohub_os_log(char *buffer, int len)
{
	if (le32_to_cpu((((uint32_t *)buffer)[0]) & 0x7FFFFFFF) ==
	    OS_LOG_EVENTID) {
		char *mtype, *mdata = &buffer[5];

		buffer[len] = 0x00;

		switch (buffer[4]) {
		case 'E':
			mtype = KERN_ERR;
			break;
		case 'W':
			mtype = KERN_WARNING;
			break;
		case 'I':
			mtype = KERN_INFO;
			break;
		case 'D':
			mtype = KERN_DEBUG;
			break;
		default:
			mtype = KERN_DEFAULT;
			mdata--;
			break;
		}
		printk("%snanohub: %s", mtype, mdata);
		return true;
	} else {
		return false;
	}
}

static void nanohub_process_buffer(struct nanohub_data *data,
				   struct nanohub_buf **buf,
				   int ret)
{
	uint32_t event_id;
	uint8_t interrupt;
	bool wakeup = false;

	data->err_cnt = 0;
	if (ret < 4 || nanohub_os_log((*buf)->buffer, ret)) {
		release_wakeup(data);
		return;
	}

	(*buf)->length = ret;

	spin_lock(&data->read_lock);
	list_add_tail(&(*buf)->list, &data->read_data);
	spin_unlock(&data->read_lock);

	event_id = le32_to_cpu((((uint32_t *)(*buf)->buffer)[0]) & 0x7FFFFFFF);
	if (ret >= sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) &&
	    event_id > FIRST_SENSOR_EVENTID &&
	    event_id <= LAST_SENSOR_EVENTID) {
		interrupt = (*buf)->buffer[sizeof(uint32_t) +
					   sizeof(uint64_t) + 3];
		if (interrupt == WAKEUP_INTERRUPT)
			wakeup = true;
	}
	*buf = NULL;
	atomic_inc(&data->read_cnt);
	wake_up_interruptible_sync(&data->read_wait);
	/* (for wakeup interrupts): hold a wake lock for 10ms so the sensor hal
	 * has time to grab its own wake lock */
	if (wakeup)
		wake_lock_timeout(&data->wakelock_read, msecs_to_jiffies(10));
	release_wakeup(data);
}

static int nanohub_kthread(void *arg)
{
	struct nanohub_data *data = (struct nanohub_data *)arg;
	struct nanohub_buf *buf = NULL;
	DEFINE_WAIT(wait);
	int ret;
	uint32_t clear_interrupts[8] = { 0x00000006 };

	data->err_cnt = 0;

	prepare_to_wait(&data->kthread_wait, &wait, TASK_INTERRUPTIBLE);
	schedule();
	finish_wait(&data->kthread_wait, &wait);

	for (;;) {
		init_wait(&wait);
		atomic_set(&data->kthread_run, 0);
		if (buf || atomic_read(&data->read_free_cnt) > 0) {
			if (request_wakeup(data))
				continue;

			if (buf == NULL
			    && atomic_add_unless(&data->read_free_cnt, -1,
						 0) > 0) {
				spin_lock(&data->read_lock);
				buf =
				    list_entry(data->read_free.next,
					       struct nanohub_buf, list);
				list_del(&buf->list);
				spin_unlock(&data->read_lock);
			}

			if (buf != NULL) {
				ret = nanohub_comms_rx_retrans_boottime(
				    data, CMD_COMMS_READ, buf->buffer,
				    255, 10, 0);

				if (ret > 0) {
					nanohub_process_buffer(data, &buf, ret);
				} else if (ret == 0) {
					/* queue empty, go to sleep */
					data->err_cnt = 0;
					data->interrupts[0] &= ~0x00000006;
					release_wakeup(data);
					prepare_to_wait(&data->kthread_wait,
							&wait,
							TASK_INTERRUPTIBLE);
					if (atomic_read(&data->kthread_run) ==
					    0)
						schedule();
					finish_wait(&data->kthread_wait, &wait);
				} else {
					release_wakeup(data);
					if (++data->err_cnt >= 10) {
						dev_err(data->sensor_dev,
							"nanohub_kthread: err_cnt=%d\n",
							data->err_cnt);
						schedule_timeout
						    (msecs_to_jiffies(1000));
					}
				}
			} else {
				release_wakeup(data);
			}
		} else if ((gpio_get_value(data->pdata->irq1_gpio) == 0) ||
			   (gpio_is_valid(data->pdata->irq2_gpio)
			    && gpio_get_value(data->pdata->irq2_gpio) == 0)) {
			/* pending interrupt, but no room to read data -
			 * clear interrupts */
			if (request_wakeup(data))
				continue;
			nanohub_comms_tx_rx_retrans(data,
						    CMD_COMMS_CLR_GET_INTR,
						    (uint8_t *)
						    clear_interrupts,
						    sizeof(clear_interrupts),
						    (uint8_t *) data->
						    interrupts,
						    sizeof(data->interrupts),
						    false, 10, 0);
			release_wakeup(data);
			prepare_to_wait(&data->kthread_wait, &wait,
					TASK_INTERRUPTIBLE);
			if (atomic_read(&data->kthread_run) == 0)
				schedule();
			finish_wait(&data->kthread_wait, &wait);
		} else {
			prepare_to_wait(&data->kthread_wait, &wait,
					TASK_INTERRUPTIBLE);
			if (atomic_read(&data->kthread_run) == 0)
				schedule();
			finish_wait(&data->kthread_wait, &wait);
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static struct nanohub_platform_data *nanohub_parse_dt(struct device *dev)
{
	struct nanohub_platform_data *pdata;
	struct device_node *dt = dev->of_node;
	const uint32_t *tmp;
	struct property *prop;
	uint32_t u, i;
	int ret;

	if (!dt)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ret = pdata->irq1_gpio =
	    of_get_named_gpio(dt, "sensorhub,irq1-gpio", 0);
	if (ret < 0) {
		pr_err("nanohub: missing sensorhub,irq1-gpio in device tree\n");
		goto free_pdata;
	}

	/* optional (strongly recommended) */
	pdata->irq2_gpio = of_get_named_gpio(dt, "sensorhub,irq2-gpio", 0);

	ret = pdata->wakeup_gpio =
	    of_get_named_gpio(dt, "sensorhub,wakeup-gpio", 0);
	if (ret < 0) {
		pr_err
		    ("nanohub: missing sensorhub,wakeup-gpio in device tree\n");
		goto free_pdata;
	}

	ret = pdata->nreset_gpio =
	    of_get_named_gpio(dt, "sensorhub,nreset-gpio", 0);
	if (ret < 0) {
		pr_err
		    ("nanohub: missing sensorhub,nreset-gpio in device tree\n");
		goto free_pdata;
	}

	/* optional (stm32f bootloader) */
	pdata->boot0_gpio = of_get_named_gpio(dt, "sensorhub,boot0-gpio", 0);

	/* optional (spi) */
	pdata->spi_cs_gpio = of_get_named_gpio(dt, "sensorhub,spi-cs-gpio", 0);

	/* optional (stm32f bootloader) */
	of_property_read_u32(dt, "sensorhub,bl-addr", &pdata->bl_addr);

	/* optional (stm32f bootloader) */
	tmp = of_get_property(dt, "sensorhub,num-flash-banks", NULL);
	if (tmp) {
		pdata->num_flash_banks = be32_to_cpup(tmp);
		pdata->flash_banks =
		    devm_kzalloc(dev,
				 sizeof(struct nanohub_flash_bank) *
				 pdata->num_flash_banks, GFP_KERNEL);
		if (!pdata->flash_banks)
			goto no_mem;

		/* TODO: investigate replacing with of_property_read_u32_array
		 */
		i = 0;
		of_property_for_each_u32(dt, "sensorhub,flash-banks", prop, tmp,
					 u) {
			if (i / 3 >= pdata->num_flash_banks)
				break;
			switch (i % 3) {
			case 0:
				pdata->flash_banks[i / 3].bank = u;
				break;
			case 1:
				pdata->flash_banks[i / 3].address = u;
				break;
			case 2:
				pdata->flash_banks[i / 3].length = u;
				break;
			}
			i++;
		}
	}

	/* optional (stm32f bootloader) */
	tmp = of_get_property(dt, "sensorhub,num-shared-flash-banks", NULL);
	if (tmp) {
		pdata->num_shared_flash_banks = be32_to_cpup(tmp);
		pdata->shared_flash_banks =
		    devm_kzalloc(dev,
				 sizeof(struct nanohub_flash_bank) *
				 pdata->num_shared_flash_banks, GFP_KERNEL);
		if (!pdata->shared_flash_banks)
			goto no_mem_shared;

		/* TODO: investigate replacing with of_property_read_u32_array
		 */
		i = 0;
		of_property_for_each_u32(dt, "sensorhub,shared-flash-banks",
					 prop, tmp, u) {
			if (i / 3 >= pdata->num_shared_flash_banks)
				break;
			switch (i % 3) {
			case 0:
				pdata->shared_flash_banks[i / 3].bank = u;
				break;
			case 1:
				pdata->shared_flash_banks[i / 3].address = u;
				break;
			case 2:
				pdata->shared_flash_banks[i / 3].length = u;
				break;
			}
			i++;
		}
	}

	return pdata;

no_mem_shared:
	devm_kfree(dev, pdata->flash_banks);
no_mem:
	ret = -ENOMEM;
free_pdata:
	devm_kfree(dev, pdata);
	return ERR_PTR(ret);
}
#else
static struct nanohub_platform_data *nanohub_parse_dt(struct device *dev)
{
	struct nanohub_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	return pdata;
}
#endif

struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev)
{
	int ret, error, i;
	const struct nanohub_platform_data *pdata;
	struct nanohub_data *data;
	struct nanohub_buf *buf;

	if (!iio_dev) {
		iio_dev = iio_device_alloc(sizeof(struct nanohub_data));
		if (!iio_dev)
			return ERR_PTR(-ENOMEM);
	}

	iio_dev->name = "nanohub";
	iio_dev->dev.parent = dev;
	iio_dev->info = &nanohub_iio_info;
	iio_dev->channels = NULL;
	iio_dev->num_channels = 0;

	data = iio_priv(iio_dev);
	data->iio_dev = iio_dev;

	buf = vmalloc(sizeof(struct nanohub_buf) * READ_QUEUE_DEPTH);

	init_waitqueue_head(&data->kthread_wait);
	atomic_set(&data->kthread_run, 0);

	spin_lock_init(&data->read_lock);
	spin_lock_init(&data->wakeup_lock);
	init_waitqueue_head(&data->read_wait);
	INIT_LIST_HEAD(&data->read_data);
	INIT_LIST_HEAD(&data->read_free);
	atomic_set(&data->read_cnt, 0);
	atomic_set(&data->read_free_cnt, READ_QUEUE_DEPTH);
	for (i = 0; i < READ_QUEUE_DEPTH; i++)
		list_add_tail(&buf[i].list, &data->read_free);
	wake_lock_init(&data->wakelock_read, WAKE_LOCK_SUSPEND,
		       "nanohub_wakelock_read");

	atomic_set(&data->wakeup_cnt, 0);
	atomic_set(&data->wakeup_aquired, 0);
	init_waitqueue_head(&data->wakeup_wait);

	pdata = dev_get_platdata(dev);
	if (!pdata) {
		pdata = nanohub_parse_dt(dev);
		if (IS_ERR(pdata))
			return ERR_PTR(PTR_ERR(pdata));
	}
	data->pdata = pdata;

	if (gpio_is_valid(pdata->irq1_gpio)) {
		error = gpio_request(pdata->irq1_gpio, "nanohub_irq1");
		if (error) {
			pr_err("nanohub: irq1_gpio request failed\n");
		} else {
			gpio_direction_input(pdata->irq1_gpio);
			data->irq1 = gpio_to_irq(pdata->irq1_gpio);

			if (data->irq1 >= 0) {
				ret =
				    request_threaded_irq(data->irq1, NULL,
							 nanohub_irq1,
							 IRQF_TRIGGER_FALLING |
							 IRQF_ONESHOT,
							 "nanohub-irq1", data);
				if (ret < 0)
					data->irq1 = -1;
			}
		}
	} else {
		pr_err("nanohub: irq1_gpio is not valid\n");
		data->irq1 = -1;
	}

	if (gpio_is_valid(pdata->irq2_gpio)) {
		error = gpio_request(pdata->irq2_gpio, "nanohub_irq2");
		if (error) {
			pr_info("nanohub: irq2_gpio request failed\n");
		} else {
			gpio_direction_input(pdata->irq2_gpio);
			data->irq2 = gpio_to_irq(pdata->irq2_gpio);

			if (data->irq2 >= 0) {
				ret =
				    request_threaded_irq(data->irq2, NULL,
							 nanohub_irq2,
							 IRQF_TRIGGER_FALLING |
							 IRQF_ONESHOT,
							 "nanohub-irq2", data);
				if (ret < 0)
					data->irq2 = -1;
			}
		}
	} else {
		pr_info("nanohub: irq2_gpio is not valid\n");
		data->irq2 = -1;
	}

	if (gpio_is_valid(pdata->nreset_gpio)) {
		error = gpio_request(pdata->nreset_gpio, "nanohub_nreset");
		if (error)
			pr_err("nanohub: nreset_gpio request failed\n");
		else
			gpio_direction_output(pdata->nreset_gpio, 1);
	} else {
		pr_err("nanohub: nreset_gpio is not valid\n");
	}

	if (gpio_is_valid(pdata->wakeup_gpio)) {
		error = gpio_request(pdata->wakeup_gpio, "nanohub_wakeup");
		if (error)
			pr_err("nanohub: wakeup_gpio request failed\n");
		else
			gpio_direction_output(pdata->wakeup_gpio, 1);
	} else {
		pr_err("nanohub: wakeup_gpio is not valid\n");
	}

	if (gpio_is_valid(pdata->boot0_gpio)) {
		error = gpio_request(pdata->boot0_gpio, "nanohub_boot0");
		if (error)
			pr_info("nanohub: boot0_gpio request failed\n");
		else
			gpio_direction_output(pdata->boot0_gpio, 0);
	} else {
		pr_info("nanohub: boot0_gpio is not valid\n");
	}

	kthread_run(nanohub_kthread, data, "nanohub");

	ret = iio_device_register(iio_dev);
	if (ret) {
		pr_err("nanohub: iio_device_register failed\n");
		return ERR_PTR(ret);
	}

	ret = create_sysfs(data);
	if (ret) {
		pr_err("nanohub: create_sysfs failed\n");
		return ERR_PTR(ret);
	}

	if (gpio_is_valid(pdata->irq1_gpio))
		disable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		disable_irq(data->irq2);
	udelay(30);

	return iio_dev;
}

int nanohub_reset(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	if (gpio_is_valid(pdata->nreset_gpio))
		gpio_direction_output(pdata->nreset_gpio, 1);
	usleep_range(650000, 700000);
	if (gpio_is_valid(pdata->irq1_gpio))
		enable_irq(data->irq1);
	if (gpio_is_valid(pdata->irq2_gpio))
		enable_irq(data->irq2);
	if (!gpio_is_valid(pdata->irq2_gpio))
		nanohub_unmask_interrupt(data, 2);

	return 0;
}

int nanohub_remove(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);
	const struct nanohub_platform_data *pdata = data->pdata;

	if (gpio_is_valid(pdata->irq1_gpio)) {
		if (data->irq1 >= 0)
			free_irq(data->irq1, data);
		gpio_free(pdata->irq1_gpio);
	}
	if (gpio_is_valid(pdata->irq2_gpio)) {
		if (data->irq2 >= 0)
			free_irq(data->irq2, data);
		gpio_free(pdata->irq2_gpio);
	}
	if (gpio_is_valid(pdata->nreset_gpio)) {
		gpio_direction_output(pdata->nreset_gpio, 0);
		gpio_free(pdata->nreset_gpio);
	}
	if (gpio_is_valid(pdata->wakeup_gpio)) {
		gpio_direction_output(pdata->wakeup_gpio, 1);
		gpio_free(pdata->wakeup_gpio);
	}
	if (gpio_is_valid(pdata->boot0_gpio)) {
		gpio_direction_output(pdata->boot0_gpio, 0);
		gpio_free(pdata->boot0_gpio);
	}

	destroy_sysfs(data);
	iio_device_unregister(iio_dev);
	iio_device_free(iio_dev);

	return 0;
}

int nanohub_suspend(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);
	const struct nanohub_platform_data *pdata = data->pdata;

	if (gpio_is_valid(pdata->irq2_gpio))
		disable_irq(data->irq2);
	else
		nanohub_mask_interrupt(data, 2);

	if (!gpio_get_value(data->pdata->irq1_gpio)
	    || atomic_read(&data->wakeup_cnt) > 0
	    || atomic_read(&data->kthread_run) > 0) {
		if (gpio_is_valid(pdata->irq2_gpio))
			enable_irq(data->irq2);
		else
			nanohub_unmask_interrupt(data, 2);

		if (gpio_is_valid(data->pdata->irq2_gpio)
		    && !gpio_get_value(data->pdata->irq2_gpio)) {
			atomic_set(&data->kthread_run, 1);
			wake_up_interruptible(&data->kthread_wait);
		}

		return -EBUSY;
	}

	enable_irq_wake(data->irq1);

	return 0;
}

int nanohub_resume(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);
	const struct nanohub_platform_data *pdata = data->pdata;

	disable_irq_wake(data->irq1);

	if (gpio_is_valid(pdata->irq2_gpio))
		enable_irq(data->irq2);
	else
		nanohub_unmask_interrupt(data, 2);

	if (!gpio_get_value(data->pdata->irq1_gpio)) {
		if (atomic_read(&data->wakeup_cnt) == 0) {
			atomic_set(&data->kthread_run, 1);
			wake_up_interruptible(&data->kthread_wait);
		} else {
			wake_up_interruptible(&data->wakeup_wait);
		}
	} else if (gpio_is_valid(data->pdata->irq2_gpio)
		   && !gpio_get_value(data->pdata->irq2_gpio)) {
		atomic_set(&data->kthread_run, 1);
		wake_up_interruptible(&data->kthread_wait);
	}

	return 0;
}

static int __init nanohub_init(void)
{
	int ret = 0;

#ifdef CONFIG_NANOHUB_I2C
	if (ret == 0)
		ret = nanohub_i2c_init();
#endif
#ifdef CONFIG_NANOHUB_SPI
	if (ret == 0)
		ret = nanohub_spi_init();
#endif
	return ret;
}

static void __exit nanohub_cleanup(void)
{
#ifdef CONFIG_NANOHUB_I2C
	nanohub_i2c_cleanup();
#endif
#ifdef CONFIG_NANOHUB_SPI
	nanohub_spi_cleanup();
#endif
}

module_init(nanohub_init);
module_exit(nanohub_cleanup);

MODULE_AUTHOR("Ben Fennema");
MODULE_LICENSE("GPL");
