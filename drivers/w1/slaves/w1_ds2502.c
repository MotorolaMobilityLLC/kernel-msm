/*
 * Copyright (C) 2014-2015 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>

#include "../w1.h"
#include "../w1_family.h"
#include <linux/crc16.h>
#include <linux/w1-gpio.h>

/* Logging macro */
/*#define W1_DS2502_DRIVER_DEBUG*/

#undef CDBG
#ifdef W1_DS2502_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif


#define DS2503_NUM_READ_MEM_CMD       3

#define DS2502_ROM_SERIAL_NUM_SIZE    48

#define MAX_RETRIES                   3

#define DS2502_READ_ROM_CMD           0x33
#define DS2502_SKIP_ROM_CMD           0xcc

#define DS2502_READ_MEMORY_CMD        0xf0
#define DS2502_READ_ADDRESS_MSB       0x00
#define DS2502_READ_ADDRESS_LSB       0x00

#define DS2502_MAX_RETRIES            3

#define DS2502_EEPROM_SIZE            128
#define DS2502_FILE_OUTPUT_SIZE       (DS2502_EEPROM_SIZE+1)

#define CRC16_INIT                    0


enum w1_ds2502_state {
	STATE_DETECT = 0,
	STATE_IDENTIFY,
	STATE_SEND_COMMAND,
	STATE_RX_DATA,
	STATE_READ_DONE,
};

enum w1_ds2502_status {
	STATUS_DATA_NOT_READ = 0,
	STATUS_DATA_POPULATED,
};

const unsigned char read_memory_cmd_addr[DS2503_NUM_READ_MEM_CMD] = {
	DS2502_READ_MEMORY_CMD,
	DS2502_READ_ADDRESS_LSB,
	DS2502_READ_ADDRESS_MSB
};

struct w1_ds2502_data {
	unsigned char rawdata[DS2502_EEPROM_SIZE];
	unsigned char crcvalid;
	int status;
};

#ifdef W1_DS2502_DRIVER_DEBUG
static void w1_ds2502_dump_data(struct w1_ds2502_data *data)
{
	int x;

	for (x = 0; x < DS2502_EEPROM_SIZE; x++)
		CDBG("%s: 0x%04x: 0x%02x\n", __func__, x, data->rawdata[x]);

	CDBG("%s: CRC Valid: %d", __func__, data->crcvalid);
}
#endif


static enum w1_ds2502_state w1_ds2502_detect(struct w1_slave *sl)
{
	enum w1_ds2502_state next_state = STATE_DETECT;

	CDBG("%s: Enter\n", __func__);

	if (w1_reset_bus(sl->master) == 0)
		next_state = STATE_IDENTIFY;
	else
		pr_err("%s: w1_reset_bus failed", __func__);

	return next_state;
}

static enum w1_ds2502_state w1_ds2502_identify(struct w1_slave *sl)
{
	enum w1_ds2502_state next_state = STATE_SEND_COMMAND;

	CDBG("%s: Enter\n", __func__);

	w1_write_8(sl->master, DS2502_SKIP_ROM_CMD);

	return next_state;
}

static enum w1_ds2502_state w1_ds2502_send_command(struct w1_slave *sl)
{
	enum w1_ds2502_state next_state = STATE_DETECT;
	unsigned char current_cmd;
	unsigned char bus_crc;
	unsigned char cmd_count;
	unsigned char computed_crc;

	CDBG("%s: Enter\n", __func__);

	for (cmd_count = 0; cmd_count < DS2503_NUM_READ_MEM_CMD; cmd_count++) {
		current_cmd = read_memory_cmd_addr[cmd_count];
		w1_write_8(sl->master, current_cmd);
	}

	/* Time to read CRC (1byte) for MEMORY FUNCTION COMMAND. This will be
	 * compared with the computed CRC by the master. */
	bus_crc = w1_read_8(sl->master);
	computed_crc = w1_calc_crc8((unsigned char *)read_memory_cmd_addr,
				    DS2503_NUM_READ_MEM_CMD);

	CDBG("%s: bus_crc:0x%x computed_crc:0x%x\n", __func__,
	     bus_crc, computed_crc);
	if (bus_crc == computed_crc)
		next_state = STATE_RX_DATA;

	return next_state;
}

static enum w1_ds2502_state w1_ds2502_rx_data(struct w1_slave *sl,
		unsigned char retry_cnt)
{
	enum w1_ds2502_state next_state = STATE_DETECT;
	unsigned int data_crc;
	unsigned int computed_crc;
	struct w1_ds2502_data *data = sl->family_data;

	CDBG("%s: Enter\n", __func__);

	/* start address was sent in STATE_SEND_COMMAND state */
	w1_read_block(sl->master, &data->rawdata[0], DS2502_EEPROM_SIZE);

	/* check CRC16 in data */
	data_crc = (data->rawdata[DS2502_EEPROM_SIZE-2] << 8) |
		    (data->rawdata[DS2502_EEPROM_SIZE-1]);
	computed_crc = crc16(CRC16_INIT, &data->rawdata[0],
			DS2502_EEPROM_SIZE-2);

	CDBG("%s: data_crc:0x%x computed_crc:0x%x\n", __func__,
	     data_crc, computed_crc);
	if (computed_crc == data_crc) {
		data->crcvalid = 1;
		next_state = STATE_READ_DONE;
	}

	if (data->crcvalid == 0 && retry_cnt == MAX_RETRIES-1) {
		/* tried a few times, but CRC is invalid */
		pr_err("%s: CRC invalid, report data read (%d)",
				__func__, retry_cnt);
		next_state = STATE_READ_DONE;
	}

	return next_state;
}

static void w1_ds2502_first_eeprom_read(struct w1_slave *sl)
{
	unsigned char retry_count = 0;
	enum w1_ds2502_state state = STATE_DETECT;
	struct w1_ds2502_data *data = sl->family_data;

	while ((retry_count < MAX_RETRIES) && (state != STATE_READ_DONE)) {
		switch (state) {
		case STATE_DETECT:
			state = w1_ds2502_detect(sl);
			break;

		case STATE_IDENTIFY:
			state = w1_ds2502_identify(sl);
			break;

		case STATE_SEND_COMMAND:
			state = w1_ds2502_send_command(sl);
			break;

		case STATE_RX_DATA:
			state = w1_ds2502_rx_data(sl, retry_count);
			break;

		default:
			break;
		}
		if (state == STATE_DETECT)
			retry_count++;
	}

	if (state == STATE_READ_DONE) {
#ifdef W1_DS2502_DRIVER_DEBUG
		w1_ds2502_dump_data(data);
#endif
		data->status = STATUS_DATA_POPULATED;
	} else if (retry_count == MAX_RETRIES) {
		pr_err("%s: max retry reached (%d)", __func__, retry_count);
	}
}

static ssize_t w1_ds2502_read_eeprom(struct file *filp, struct kobject *kobj,
				     struct bin_attribute *bin_attr,
				     char *buf, loff_t off, size_t count)
{
	struct w1_slave *sl = kobj_to_w1_slave(kobj);
	struct w1_gpio_platform_data *w1_gpio_pdata =
			sl->master->bus_master->data;
	struct w1_ds2502_data *data = sl->family_data;
	unsigned int num_bytes_copied = 0;
	int error;

	CDBG("%s: Enter count=%zu\n", __func__, count);

	if (count < DS2502_FILE_OUTPUT_SIZE) {
		pr_err("%s: input buffer too small %zu", __func__, count);
		return num_bytes_copied;
	}

	if (data->status == STATUS_DATA_NOT_READ) {

		/* turn on regulator */
		if (w1_gpio_pdata->w1_gpio_vdd != NULL) {
			error = regulator_enable(w1_gpio_pdata->w1_gpio_vdd);
			if (error) {
				pr_err("%s: Error %d enabling w1-vdd regulator\n",
					__func__, error);
			} else {
				w1_gpio_pdata->regulator_en = 1;
				CDBG("%s: en: w1-vdd regulator is %s\n", __func__,
					regulator_is_enabled(w1_gpio_pdata->w1_gpio_vdd) ?
					"on" : "off");
			}
		}

		/* read data for first time */
		mutex_lock(&sl->master->mutex);

		w1_ds2502_first_eeprom_read(sl);

		mutex_unlock(&sl->master->mutex);

		/* turn off regulator */
		if (w1_gpio_pdata->regulator_en) {
			error = regulator_disable(w1_gpio_pdata->w1_gpio_vdd);
			if (error) {
				pr_err("%s: Error %d disabling w1-vdd regulator\n",
					__func__, error);
			} else {
				w1_gpio_pdata->regulator_en = 0;
				CDBG("%s: dis: w1-vdd regulator is %s\n", __func__,
					regulator_is_enabled(w1_gpio_pdata->w1_gpio_vdd) ?
					"on" : "off");
			}
		}
	} else {
		/* already read, so skip reading */
		CDBG("%s: already read data once\n", __func__);
	}

	if (data->status == STATUS_DATA_POPULATED) {
#ifdef W1_DS2502_DRIVER_DEBUG
		w1_ds2502_dump_data(data);
#endif
		memcpy(buf, &data->rawdata[0], DS2502_EEPROM_SIZE);
		memcpy(buf+DS2502_EEPROM_SIZE, &data->crcvalid, 1);
		num_bytes_copied = DS2502_FILE_OUTPUT_SIZE;
	}

	return (num_bytes_copied == 0) ? 0 : count;
}

static struct bin_attribute w1_ds2502_eeprom_attr = {
	.attr = {
		.name = "eeprom",
		.mode = S_IRUGO | S_IWUSR,
	},
	.size = DS2502_FILE_OUTPUT_SIZE,
	.read = w1_ds2502_read_eeprom,
};

static int w1_ds2502_add_slave(struct w1_slave *sl)
{
	int retval;
	struct w1_ds2502_data *data;

	CDBG("%s: Enter\n", __func__);

	data = kzalloc(sizeof(struct w1_ds2502_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->crcvalid = 0;
	data->status = STATUS_DATA_NOT_READ;
	sl->family_data = data;

	retval = sysfs_create_bin_file(&sl->dev.kobj, &w1_ds2502_eeprom_attr);

	w1_ds2502_first_eeprom_read(sl);

	CDBG("%s: retval:%d\n", __func__, retval);
	if (retval)
		kfree(data);

	return retval;
}

static void w1_ds2502_remove_slave(struct w1_slave *sl)
{
	CDBG("%s: Enter\n", __func__);

	kfree(sl->family_data);
	sl->family_data = NULL;
	sysfs_remove_bin_file(&sl->dev.kobj, &w1_ds2502_eeprom_attr);
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
MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("w1 DS2502 driver");
