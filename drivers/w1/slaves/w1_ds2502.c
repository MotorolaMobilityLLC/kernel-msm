/*
 * Copyright (C) 2006-2009, 2011 Motorola Mobility, Inc.
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
 *
 * Based on w1_ds2780.c which is:
 * Copyright (C) 2010 Indesign, LLC
 */

#include <linux/device.h>
#include <linux/idr.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "../w1.h"
#include "../w1_family.h"
#include "w1_ds2502.h"

enum w1_ds2502_state {
	STATE_DETECT = 0,
	STATE_IDENTIFY,
	STATE_SEND_COMMAND,
	STATE_RX_DATA,
	STATE_READ_DONE,
	STATE_RETRY_DETECT
};

const unsigned char battery_command[NUM_BATTERY_COMMAND] = {
	READ_DATA_COMMAND,
	DATA_ADDRESS_LSB,
	DATA_ADDRESS_MSB
};

struct w1_ds2502_data {
	unsigned char serial_numbers[NUM_SERIAL_NUMBER][NUM_SECURITY_EPROM];
	unsigned char unique_id[UID_SIZE];
	int status;
	int conflict_bit_index;
};

/* This macro performs 1-bit right rotation on byte x. */
#define ror1(x, y) ((x >> 1) | ((y & 1) << 7))

static void w1_ds2502_shift_left64(struct w1_slave *sl,
				   unsigned char serial_num_index,
				   unsigned char bit)
{
	int index;
	struct w1_ds2502_data *data = sl->family_data;

	for (index = 0; index < (NUM_SECURITY_EPROM - 1); index++)
		data->serial_numbers[serial_num_index][index] =
			ror1(data->serial_numbers[serial_num_index][index],
			     data->serial_numbers[serial_num_index][index+1]);

	data->serial_numbers[serial_num_index][NUM_SECURITY_EPROM - 1] =
		ror1(data->serial_numbers[serial_num_index]
		     [NUM_SECURITY_EPROM - 1], bit);
}

static enum w1_ds2502_state w1_ds2502_detect(struct w1_slave *sl)
{
	enum w1_ds2502_state next_state = STATE_RETRY_DETECT;

	if (w1_reset_bus(sl->master) == 0)
		next_state = STATE_IDENTIFY;

	return next_state;
}

static enum w1_ds2502_state w1_ds2502_identify(struct w1_slave *sl,
					       unsigned char *serial_num_index,
					       bool *multiple_devices_found_ptr)
{
	enum w1_ds2502_state next_state = STATE_SEND_COMMAND;
	unsigned char bit_index = 0;
	unsigned char read_bit;
	unsigned char read_complement_bit;
	unsigned char bit;
	unsigned char temp_data;
	int error_found = 0;
	struct w1_ds2502_data *data = sl->family_data;

	w1_write_8(sl->master, SEARCH_ROM_COMMAND);

	for (bit_index = 0; bit_index < MEM_BIT_COUNT; bit_index++) {
		/* Determine if this is the first serial number found or if
		 * this would be a new conflict */
		bit = 1;
		if ((*serial_num_index == 0) ||
		    ((data->conflict_bit_index < bit_index) &&
		((data->serial_numbers[(*serial_num_index)-1][bit_index >> 3] &
		       (1 << (bit_index & 0x07))) != 0))) {
			bit = 0;
		}

		temp_data = w1_triplet(sl->master, bit);
		read_bit = temp_data & 0x01;
		read_complement_bit = (temp_data >> 1) & 0x01;

		/* Determine if all devices have fallen off the bus. */
		if ((read_bit == 1) && (read_complement_bit == 1)) {
			pr_err("%s: STATE_RETRY_DETECT\n", __func__);
			next_state = STATE_RETRY_DETECT;
			error_found = -EFAULT;
			break;
		}
		/* Determine if there is a bit conflict. */
		else if ((read_bit == 0) && (read_complement_bit == 0)) {
			*multiple_devices_found_ptr = 1;

			if (bit == 0) {
				/* New conflict found so move conflict_bit_index
				 * to the current bit position. */
				data->conflict_bit_index = bit_index;
			}
		}

		/* Or in the new bit value. */
		w1_ds2502_shift_left64(sl, *serial_num_index,
				       (temp_data & 0x04) >> 2);
	}

	data->status = error_found;
	return next_state;
}

static enum w1_ds2502_state w1_ds2502_send_command(struct w1_slave *sl)
{
	enum w1_ds2502_state next_state = STATE_RETRY_DETECT;
	unsigned char current_cmd;
	unsigned char bus_crc;
	unsigned char cmd_count;
	unsigned char computed_crc;

	for (cmd_count = 0; cmd_count < NUM_BATTERY_COMMAND; cmd_count++) {
		current_cmd = battery_command[cmd_count];
		w1_write_8(sl->master, current_cmd);
	}

	/* Time to read CRC (1byte) for MEMORY FUNCTION COMMAND. This will be
	 * compared with the computed CRC by the master. */
	bus_crc = w1_read_8(sl->master);
	computed_crc = w1_calc_crc8((unsigned char *)battery_command,
				    NUM_BATTERY_COMMAND);

	if (bus_crc == computed_crc)
		next_state = STATE_RX_DATA;
	else
		pr_err("%s: crc err\n", __func__);

	return next_state;
}

static enum w1_ds2502_state w1_ds2502_rx_data(struct w1_slave *sl,
					      unsigned char *dest_addr,
					      unsigned char *serial_num_index,
					      bool multiple_devices_found)
{
	enum w1_ds2502_state next_state = STATE_RETRY_DETECT;
	unsigned char computed_crc;
	unsigned char rx_data[EEPROM_SIZE+1];
	unsigned char rx_data_index = 0;
	bool crc_success = 0;
	int count;
	struct w1_ds2502_data *data = sl->family_data;

	while (rx_data_index < EEPROM_SIZE) {
		/* Read data from the bus, including CRC. */
		for (count = 0; count < (EEPROM_PAGE_SIZE+1); count++)
			rx_data[rx_data_index + count] = w1_read_8(sl->master);

		if (rx_data_index < (EEPROM_PAGE_SIZE * 4)) {
			computed_crc = w1_calc_crc8(&rx_data[rx_data_index],
						    EEPROM_PAGE_SIZE);
			if (computed_crc ==
			    rx_data[rx_data_index + EEPROM_PAGE_SIZE])
				crc_success = 1;
			else {
				pr_err("%s: RX Data crc error\n", __func__);
				crc_success = 0;
				break;
			}
		}
		rx_data_index += EEPROM_PAGE_SIZE;
	}

	if (crc_success == 1) {
		if (rx_data[BATT_AUX_SEL] == MAIN_BATT_PRESENT) {
			memcpy(dest_addr, &rx_data[0], EEPROM_SIZE);

			memcpy(data->unique_id,
			       &data->serial_numbers[*serial_num_index][0],
			       UID_SIZE);

			next_state = STATE_READ_DONE;
		} else {
			*serial_num_index = *serial_num_index + 1;

			/* If multiple devices were detected on the bus, then
			 * check the next device for the data. */
			if ((multiple_devices_found) &&
			    (*serial_num_index < NUM_SERIAL_NUMBER)) {
				next_state = STATE_IDENTIFY;
				w1_reset_bus(sl->master);
			} else {
				data->status = -EINVAL;
				next_state = STATE_READ_DONE;
			}
		}
	}

	return next_state;
}

static enum w1_ds2502_state w1_ds2502_retry(struct w1_slave *sl,
					    unsigned char *retry_count_ptr,
					    unsigned char *serial_num_index,
					    bool multiple_devices_found)
{
	enum w1_ds2502_state next_state = STATE_DETECT;

	(*retry_count_ptr)++;

	if (*retry_count_ptr >= MAX_RETRIES) {
		(*serial_num_index)++;

		/* If more than 1 device was found on the bus and if all
		 * devices have not been checked then read the next device. */
		if ((multiple_devices_found) &&
		    (*serial_num_index < NUM_SERIAL_NUMBER)) {
			*retry_count_ptr = 0;

			w1_reset_bus(sl->master);
			next_state = STATE_IDENTIFY;
		}
	} else {
		/* If more than one device was found on the bus, first do
		 * retries on the current device. */
		if (multiple_devices_found) {
			w1_reset_bus(sl->master);
			next_state = STATE_IDENTIFY;
		}
	}

	return next_state;
}

static ssize_t w1_ds2502_read_eeprom_nolock(struct w1_slave *sl,
						char *buf,
						size_t count)
{
	unsigned char retry_count = 0;
	enum w1_ds2502_state state = STATE_DETECT;
	unsigned char serial_number_index = 0;
	static bool multiple_devices_found;
	struct w1_ds2502_data *data = sl->family_data;

	data->status = -EINVAL;
	memset(data->unique_id, 0xFF, UID_SIZE);

	while ((retry_count < MAX_RETRIES) && (state != STATE_READ_DONE)) {
		switch (state) {
		case STATE_DETECT:
			serial_number_index = 0;
			data->conflict_bit_index = 0;
			state = w1_ds2502_detect(sl);
			break;

		case STATE_IDENTIFY:
			state = w1_ds2502_identify(sl, &serial_number_index,
						   &multiple_devices_found);
			break;

		case STATE_SEND_COMMAND:
			state = w1_ds2502_send_command(sl);
			break;

		case STATE_RX_DATA:
			state = w1_ds2502_rx_data(sl, buf, &serial_number_index,
						  multiple_devices_found);
			break;

		case STATE_RETRY_DETECT:
		default:
			state = w1_ds2502_retry(sl, &retry_count,
						&serial_number_index,
						multiple_devices_found);
			break;
		}
	}

	return (data->status == 0) ? count : 0;
}

ssize_t w1_ds2502_read_eeprom(struct w1_slave *sl,
				char *buf,
				size_t count)
{
	ssize_t rc;

	mutex_lock(&sl->master->mutex);
	rc = w1_ds2502_read_eeprom_nolock(sl, buf, count);
	if (!rc) {
		unsigned long flags;
		pr_err("%s: Retry with IRQ disable\n", __func__);
		local_irq_save(flags);
		rc = w1_ds2502_read_eeprom_nolock(sl, buf, count);
		local_irq_restore(flags);
	}
	mutex_unlock(&sl->master->mutex);
	return rc;
}
EXPORT_SYMBOL(w1_ds2502_read_eeprom);

static ssize_t w1_ds2502_read_eeprom_file(struct file *filp,
					  struct kobject *kobj,
					  struct bin_attribute *bin_attr,
					  char *buf, loff_t off, size_t count)
{
	unsigned char rx_data[EEPROM_SIZE];
	struct w1_slave *sl = kobj_to_w1_slave(kobj);
	size_t rc;

	if (off >= EEPROM_SIZE)
		return 0;
	rc = w1_ds2502_read_eeprom(sl, rx_data, EEPROM_SIZE);

	if (rc < 0)
		return rc;

	if (rc != EEPROM_SIZE)
		return -EIO;

	if (count + off > EEPROM_SIZE)
		count = EEPROM_SIZE - off;
	memcpy(buf, rx_data + off, count);
	return count;
}

ssize_t w1_ds2502_read_uid(struct w1_slave *sl, char *buf, size_t count)
{
	int size = (UID_SIZE < count) ? UID_SIZE : count;
	int retval = 0;
	struct w1_ds2502_data *data = sl->family_data;

	mutex_lock(&sl->master->mutex);

	if (data->status == 0) {
		memcpy(buf, data->unique_id, size);
		retval = size;
	}

	mutex_unlock(&sl->master->mutex);

	return retval;
}
EXPORT_SYMBOL(w1_ds2502_read_uid);

static ssize_t w1_ds2502_read_uid_file(struct file *filp, struct kobject *kobj,
				  struct bin_attribute *bin_attr,
				  char *buf, loff_t off, size_t count)
{
	struct w1_slave *sl = kobj_to_w1_slave(kobj);

	return w1_ds2502_read_uid(sl, buf, count);
}

static struct bin_attribute w1_ds2502_eeprom_attr = {
	.attr = {
		.name = "eeprom",
		.mode = S_IRUGO | S_IWUSR,
	},
	.size = EEPROM_SIZE,
	.read = w1_ds2502_read_eeprom_file,
};

static struct bin_attribute w1_ds2502_uid_attr = {
	.attr = {
		.name = "uid",
		.mode = S_IRUGO | S_IWUSR,
	},
	.size = UID_SIZE,
	.read = w1_ds2502_read_uid_file,
};

static DEFINE_IDR(bat_idr);
static DEFINE_MUTEX(bat_idr_lock);

static int new_bat_id(void)
{
	int ret;

	while (1) {
		int id;

		ret = idr_pre_get(&bat_idr, GFP_KERNEL);
		if (ret == 0)
			return -ENOMEM;

		mutex_lock(&bat_idr_lock);
		ret = idr_get_new(&bat_idr, NULL, &id);
		mutex_unlock(&bat_idr_lock);

		if (ret == 0) {
			ret = id & MAX_ID_MASK;
			break;
		} else if (ret == -EAGAIN) {
			continue;
		} else {
			break;
		}
	}

	return ret;
}

static void release_bat_id(int id)
{
	mutex_lock(&bat_idr_lock);
	idr_remove(&bat_idr, id);
	mutex_unlock(&bat_idr_lock);
}

static int w1_ds2502_add_slave(struct w1_slave *sl)
{
	int retval;
	int id;
	char *devname;
	struct platform_device *pdev;
	struct w1_ds2502_data *data;

	data = kzalloc(sizeof(struct w1_ds2502_data), GFP_KERNEL);
	if (!data) {
		retval = -ENOMEM;
		goto data_alloc_failed;
	}
	sl->family_data = data;

	if ((((sl->reg_num.id) >> 32) & 0xFFFF) == MMI_MANUFACTURE_CODE) {
		devname = "mmi-battery";
		sl->master->search_count = 0;
	} else
		devname = "ds2502-device";

	id = new_bat_id();
	if (id < 0) {
		retval = id;
		goto noid;
	}

	pdev = platform_device_alloc(devname, id);
	if (!pdev) {
		retval = -ENOMEM;
		goto pdev_alloc_failed;
	}
	pdev->dev.parent = &sl->dev;

	retval = platform_device_add(pdev);
	if (retval)
		goto pdev_add_failed;

	retval = sysfs_create_bin_file(&sl->dev.kobj, &w1_ds2502_eeprom_attr);
	if (retval)
		goto error_create_eeprom_file;

	retval = sysfs_create_bin_file(&sl->dev.kobj, &w1_ds2502_uid_attr);
	if (retval)
		goto error_create_uid_file;

	return 0;

error_create_uid_file:
	sysfs_remove_bin_file(&sl->dev.kobj, &w1_ds2502_eeprom_attr);
error_create_eeprom_file:
	platform_device_del(pdev);
pdev_add_failed:
	platform_device_put(pdev);
pdev_alloc_failed:
	release_bat_id(id);
noid:
	kfree(data);
data_alloc_failed:
	return retval;
}

static void w1_ds2502_remove_slave(struct w1_slave *sl)
{
	struct platform_device *pdev = dev_get_drvdata(&sl->dev);
	int id = pdev->id;

	platform_device_unregister(pdev);
	release_bat_id(id);
	kfree(sl->family_data);
	sl->family_data = NULL;
	sysfs_remove_bin_file(&sl->dev.kobj, &w1_ds2502_eeprom_attr);
	sysfs_remove_bin_file(&sl->dev.kobj, &w1_ds2502_uid_attr);
	sl->master->search_count = -1;
}

static struct w1_family_ops w1_ds2502_fops = {
	.add_slave    = w1_ds2502_add_slave,
	.remove_slave = w1_ds2502_remove_slave,
};

static struct w1_family w1_ds2502_family = {
	.fid  = W1_EEPROM_DS2502,
	.fops = &w1_ds2502_fops,
};

static int __init w1_ds2502_init(void)
{
	return w1_register_family(&w1_ds2502_family);
}

static void __exit w1_ds2502_exit(void)
{
	w1_unregister_family(&w1_ds2502_family);
}

module_init(w1_ds2502_init);
module_exit(w1_ds2502_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility Inc.");
MODULE_DESCRIPTION("w1 DS2502 driver");
