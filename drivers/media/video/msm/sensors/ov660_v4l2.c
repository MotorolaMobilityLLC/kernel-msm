/*
 * Copyright (C) 2013, Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include "ov660_v4l2.h"
#include <linux/i2c.h>

#define OV660_CHIP_ID_ADDR 0x6080
#define OV660_CHIP_ID_DATA 0x0660

struct ov660_data_t {
	struct i2c_client *client;
};
static struct ov660_data_t *ov660_data;

static int ov660_write_i2c(uint16_t addr, uint8_t data)
{
	int32_t rc = 0;
	uint8_t buf[3];
	struct i2c_msg msg = {
		.addr = ov660_data->client->addr,
		.flags = 0,
		.buf = buf,
		.len = 3, };

	buf[0] = addr >> 8;
	buf[1] = (addr & 0x00FF);
	buf[2] = data;

	pr_debug("%s: Address: 0x%x%x Data: %x\n", __func__,
			buf[0], buf[1], buf[2]);

	if (ov660_data && ov660_data->client) {
		rc = i2c_transfer(ov660_data->client->adapter, &msg, 1);
		if (rc < 0)
			pr_err("%s: i2c transfer error (%d)\n", __func__, rc);
	}

	return rc;
}

static int ov660_read_i2c(uint16_t addr, uint8_t *data, int data_length)
{
	int32_t rc = -EFAULT;
	uint8_t buf[4];
	uint16_t saddr;
	struct i2c_msg msgs[2];

	if (ov660_data && ov660_data->client) {
		saddr = ov660_data->client->addr;
		msgs[0].addr = saddr;
		msgs[0].flags = 0;
		msgs[0].len = 2;
		msgs[0].buf = buf;

		msgs[1].addr = saddr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = data_length;
		msgs[1].buf = data;

		buf[0] = addr >> 8;
		buf[1] = (addr & 0x00FF);

		rc = i2c_transfer(ov660_data->client->adapter, msgs, 2);
		if (rc < 0)
			pr_err("%s: error writing i2c (%d)\n",
						__func__, rc);
	}
	return rc;
}
static int ov660_write_i2c_tbl(struct ov660_reg_i2c_tbl *in_table,
		uint16_t size)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < size; i++) {
		rc = ov660_write_i2c(in_table->reg_addr, in_table->reg_data);
		in_table++;
	}
	return rc;
}

/* This function replaces the current probe since the power up
 * sequence and power down sequence is tied to the camera driver
 * it self. The camera being used will see if the ov660 is
 * active by calling this probe function */
int32_t ov660_check_probe(void)
{
	int rc = 0;
	uint8_t data[2];
	uint8_t *data_ptr = data;

	/* Need to make sure we are talking to the right device since
	 * there is no other code to check the id of the part */
	ov660_read_i2c(OV660_CHIP_ID_ADDR, data_ptr, 2);

	/* Now checking to see if we match the correct chip id */
	if ((data[0] << 8 | data[1]) == OV660_CHIP_ID_DATA) {
		pr_info("%s: successful\n", __func__);
	} else {
		pr_err("%s: does not exist! with return value %x\n", __func__,
				(data[0] << 8 | data[1]));
		rc = -ENODEV;
	}

	return rc;
}

static int ov660_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENODEV;
		dev_err(&client->dev, "client not i2c capable\n");
		return rc;
	}

	ov660_data = kzalloc(sizeof(struct ov660_data_t), GFP_KERNEL);
	if (ov660_data == NULL) {
		rc = -ENOMEM;
		dev_err(&client->dev, "kzalloc failed\n");
		return rc;
	}

	ov660_data->client = client;
	i2c_set_clientdata(client, ov660_data);

	pr_info("%s: successful!\n", __func__);
	return rc;
}

static int ov660_remove(struct i2c_client *client)
{
	kfree(ov660_data);
	ov660_data = NULL;
	return 0;
}

static const struct i2c_device_id ov660_id[] = {
	{OV660_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov660_id);

static struct i2c_driver ov660_i2c_driver = {
	.probe = ov660_probe,
	.remove = ov660_remove,
	.id_table = ov660_id,
	.driver = {
		.name = OV660_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init ov660_init(void)
{
	return i2c_add_driver(&ov660_i2c_driver);
}

static void __init ov660_exit(void)
{
	i2c_del_driver(&ov660_i2c_driver);
}


static struct ov660_reg_i2c_tbl ov660_ov8835_init_settings[] = {
	{0x6b00, 0x10},
	{0x6103, 0x20},
	{0x6010, 0xff},
	{0x6011, 0xff},
	{0x6012, 0xff},
	{0x6013, 0xff},
	{0x6014, 0xff},
	{0x6008, 0x00},
	{0x6009, 0x00},
	{0x600a, 0x00},
	{0x600b, 0x00},
	{0x600c, 0x00},
	{0x600d, 0x00},
	{0x600e, 0x00},
	{0x6021, 0x38},
	{0x6026, 0x56},
	{0x6814, 0x10},
	{0x6813, 0x02},
	{0x6805, 0x08},
	{0x680c, 0x0c},
	{0x6822, 0x4f},
	{0x6204, 0x0c},
	{0x6284, 0x08},
	{0x6410, 0x28},
	{0x6424, 0x01},
	{0x6333, 0x60},
	{0x6310, 0x0c},
	{0x6311, 0xc0},
	{0x6312, 0x09},
	{0x6313, 0x90},
	{0x6314, 0x0c},
	{0x6315, 0xc0},
	{0x6316, 0x09},
	{0x6317, 0x90},
	{0x6318, 0x00},
	{0x6319, 0x00},
	{0x631a, 0x00},
	{0x631b, 0x00},
	{0x6300, 0x0c},
	{0x6301, 0xc0},
	{0x6302, 0x09},
	{0x6303, 0x90},
	{0x6819, 0x0c},
	{0x681a, 0xc0},
	{0x681b, 0x09},
	{0x681c, 0x90},
	{0x6815, 0x80},
	{0x6817, 0x2b},
	{0x620f, 0xab},
	{0x628f, 0xab},
	{0x6204, 0x0c},
	{0x6402, 0x02},
	{0x6400, 0x88},
	{0x6404, 0xa4},
	{0x601b, 0x01},
	{0x6823, 0x0f},
	{0x6702, 0x02},
	{0x6106, 0x40},
	{0x7009, 0x14},
	{0x7011, 0x20},
	{0x7013, 0x10},
	{0x701e, 0x0c},
	{0x701f, 0xbf},
	{0x7020, 0x09},
	{0x7021, 0x8f},
	{0x7002, 0x15},
	{0x7004, 0x00},
	{0x7003, 0x10},
	{0x7041, 0x00},
	{0x7000, 0x40},
	{0x704f, 0x10},
	{0x7082, 0x00},
	{0x7182, 0x07},
	{0x7183, 0x87},
	{0x7184, 0x05},
	{0x7185, 0x05},
	{0x7186, 0x07},
	{0x7187, 0x87},
	{0x7188, 0x05},
	{0x7189, 0x05},
	{0x7110, 0x40},
	{0x7111, 0x20},
	{0x634e, 0x40},
};

static struct ov660_reg_i2c_tbl ov660_ov10820_init_settings[] = {
	{0x6b00, 0x10},
	{0x6103, 0x20},
	{0x6010, 0xff},
	{0x6011, 0xff},
	{0x6012, 0xff},
	{0x6013, 0xff},
	{0x6014, 0xff},
	{0x6008, 0x00},
	{0x6009, 0x00},
	{0x600a, 0x00},
	{0x600b, 0x00},
	{0x600c, 0x00},
	{0x600d, 0x00},
	{0x600e, 0x00},
	{0x6021, 0x7a},
	{0x6026, 0x56},
	{0x60a0, 0x15},
	{0x6814, 0x0f},
	{0x6813, 0x03},
	{0x6805, 0x08},
	{0x680c, 0x0c},
	{0x6822, 0x4f},
	{0x6204, 0x0c},
	{0x6284, 0x08},
	{0x6410, 0x28},
	{0x6424, 0x01},
	{0x6333, 0x60},
	{0x6310, 0x10},
	{0x6311, 0xe0},
	{0x6312, 0x09},
	{0x6313, 0x80},
	{0x6314, 0x10},
	{0x6315, 0xe0},
	{0x6316, 0x09},
	{0x6317, 0x80},
	{0x6318, 0x00},
	{0x6319, 0x00},
	{0x631a, 0x00},
	{0x631b, 0x00},
	{0x6300, 0x10},
	{0x6301, 0xe0},
	{0x6302, 0x09},
	{0x6303, 0x80},
	{0x6819, 0x10},
	{0x681a, 0xe0},
	{0x681b, 0x09},
	{0x681c, 0x80},
	{0x6815, 0x80},
	{0x6817, 0x2b},
	{0x620f, 0xab},
	{0x628f, 0xab},
	{0x6204, 0x0c},
	{0x6402, 0x02},
	{0x6400, 0x88},
	{0x6404, 0xa4},
	{0x601b, 0x01},
	{0x6823, 0x0f},
	{0x6702, 0x02},
	{0x6106, 0x40},
	{0x7009, 0x14},
	{0x7011, 0x40},
	{0x7013, 0x40},
	{0x701e, 0x10},
	{0x701f, 0xdf},
	{0x7020, 0x09},
	{0x7021, 0x7f},
	{0x7001, 0xe0},
	{0x7002, 0x15},
	{0x7003, 0x11},
	{0x7004, 0x01},
	{0x7006, 0x03},
	{0x7041, 0x00},
	{0x7000, 0x40},
	{0x704f, 0x11},
	{0x7082, 0x00},
	{0x7182, 0x07},
	{0x7183, 0x87},
	{0x7184, 0x05},
	{0x7185, 0x05},
	{0x7186, 0x07},
	{0x7187, 0x87},
	{0x7188, 0x05},
	{0x7189, 0x05},
	{0x7110, 0x40},
	{0x7111, 0x20},
	{0x634e, 0x40},
};

int32_t ov660_intialize_10MP(void)
{
	int32_t rc = 0;
	pr_debug("%s: enter\n", __func__);
	rc = ov660_write_i2c_tbl(ov660_ov10820_init_settings,
			ARRAY_SIZE(ov660_ov10820_init_settings));
	pr_debug("%s: exit\n", __func__);
	return rc;
}

int32_t ov660_intialize_8MP(void)
{
	int32_t rc = 0;
	rc = ov660_write_i2c_tbl(ov660_ov8835_init_settings,
			ARRAY_SIZE(ov660_ov8835_init_settings));

	return rc;
}

module_init(ov660_init);
module_exit(ov660_exit);

MODULE_DESCRIPTION("ASIC driver for 8 and 10 MP RGBC Cameras");
MODULE_AUTHOR("MOTOROLA");
MODULE_LICENSE("GPL");


