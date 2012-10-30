/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
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
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <sound/soc.h>

#include "tas2552-core.h"

#define NAME "tas2552"

#define I2C_RETRY_DELAY		5 /* ms */
#define I2C_RETRIES		5

static struct i2c_client *tas2552_client;

struct tas2552_data {
	struct regulator *supply;
	uint8_t power_state;
};

static const u8 tas2552_reg_defaults[TAS2552_REG_LIST_SIZE] = {
0x0,
0x32, /* PLL CLK set to to use internal oscillator, and all blocks shutdown */
0xca, /* Disable Boost Auto-Pass Through, enable Boost */
0x80, /* select analog audio input */
0x0,
0x0,
0x0,
0x81,
0x10, /* pll set up register */
0x0,
0x0,
/* limiter and battery guard slope control registers */
0x8f,
0x80,
0xbe,
0x8,
0x4,
0x0,
0x1, /* PDM data config register*/
0x1c, /* gain Register */
0x40,
0x0,
0x0,
0x4,
0x0, /* Int mask register */
0x0,
0x0
};

/* I2C Read/Write Functions */
static int tas2552_i2c_read(struct tas2552_data *tas2552,
				u8 reg, u8 *value)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = tas2552_client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		 },
		{
		 .addr = tas2552_client->addr,
		 .flags = I2C_M_RD,
		 .len = 1,
		 .buf = value,
		 },
	};

	do {
		err = i2c_transfer(tas2552_client->adapter, msgs,
					ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tas2552_client->dev, "read transfer error %d\n", err);
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tas2552_i2c_write(struct tas2552_data *tas2552,
				u8 reg, u8 value)
{
	int err;
	int tries = 0;
	u8 buf[2] = {0, 0};

	struct i2c_msg msgs[] = {
		{
		 .addr = tas2552_client->addr,
		 .flags = 0,
		 .len = 2,
		 .buf = buf,
		 },
	};

	buf[0] = reg;
	buf[1] = value;
	do {
		err = i2c_transfer(tas2552_client->adapter, msgs,
				ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&tas2552_client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}


static int tas2552_reg_read(struct tas2552_data *tas2552,
			u8 reg,
			u8 *value)
{
	int ret = -EINVAL;
	if (tas2552->power_state)
		ret = tas2552_i2c_read(tas2552, reg, value);

	return ret;
}

static int tas2552_reg_write(struct tas2552_data *tas2552,
			u8 reg,
			u8 value,
			u8 mask)
{
	int retval;
	u8 old_value;

	value &= mask;

	retval = tas2552_reg_read(tas2552, reg, &old_value);

	pr_debug("Old value = 0x%08X reg no. = 0x%02X\n", old_value, reg);

	if (retval != 0)
		goto error;

	old_value &= ~mask;
	value |= old_value;

	pr_debug("New value = 0x%08X  reg no. = 0x%02X\n", value, reg);
	if (tas2552->power_state)
		retval = tas2552_i2c_write(tas2552, reg, value);

error:
	return retval;
}

static int tas2552_initialize(void)
{
	int i = 0, retval = 0;
	struct tas2552_data *tas2552 = i2c_get_clientdata(tas2552_client);

	/* Initialize device registers */
	for (i = 0; i < ARRAY_SIZE(tas2552_reg_defaults); i++) {
		retval = tas2552_reg_write(tas2552, i,
					tas2552_reg_defaults[i], 0xff);
		if (retval != 0)
			goto error;
	}

error:
	return retval;

}

void tas2552_ext_amp_on(int on)
{
	struct tas2552_data *tas2552 = i2c_get_clientdata(tas2552_client);
	pr_info("tas2552 amp event %d", on);
	if (on)
		tas2552_reg_write(tas2552, TAS2552_CFG1_REG, 0,
						TAS2552_SHUTDOWN);
	else
		tas2552_reg_write(tas2552, TAS2552_CFG1_REG, TAS2552_SHUTDOWN,
						TAS2552_SHUTDOWN);
}
EXPORT_SYMBOL_GPL(tas2552_ext_amp_on);

#ifdef CONFIG_DEBUG_FS

static struct dentry *tas2552_dir;
static struct dentry *tas2552_reg_file;
static struct dentry *tas2552_set_reg_file;

static int tas2552_registers_print(struct seq_file *s, void *p)
{
	struct tas2552_data *tas2552 = s->private;
	u8 reg;
	u8 value;
	pr_info("%s: print registers", __func__);
	seq_printf(s, "tas2552 registers:\n");
	if (tas2552->power_state) {
		for (reg = 0; reg < TAS2552_REG_LIST_SIZE; reg++) {
			tas2552_reg_read(tas2552, reg, &value);
			seq_printf(s, "[0x%x]:  0x%x\n", reg, value);
		}
	}

	return 0;
}

static int tas2552_registers_open(struct inode *inode, struct file *file)
{
	return single_open(file, tas2552_registers_print, inode->i_private);
}

static int tas2552_set_reg_open_file(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t tas2552_set_reg(struct file *file,
				  const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct tas2552_data *tas2552 = file->private_data;
	char buf[32];
	ssize_t buf_size;
	unsigned int user_reg;
	unsigned int user_value;
	u8 reg_read_back;
	/* Get userspace string and assure termination */
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	if (sscanf(buf, "0x%02x  0x%02x", &user_reg, &user_value) != 2)
		return -EFAULT;

	if (user_reg > TAS2552_MAX_REGISTER_VAL) {
		pr_info("%s: val check failed", __func__);
		return -EINVAL;
	}

	pr_info("%s: set register 0x%02x value 0x%02x", __func__,
		user_reg, user_value);

	tas2552_reg_write(tas2552, user_reg & 0xFF, user_value & 0XFF, 0xFF);
	tas2552_reg_read(tas2552, user_reg, &reg_read_back);

	pr_info("%s: debug write reg[0x%02x] with 0x%02x after readback: 0x%02x\n",
				__func__, user_reg, user_value, reg_read_back);

	return buf_size;
}


static const struct file_operations tas2552_registers_fops = {
	.open = tas2552_registers_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations tas2552_get_set_reg_fops = {
	.open = tas2552_set_reg_open_file,
	.write = tas2552_set_reg,
	.owner = THIS_MODULE,
};

static void tas2552_setup_debugfs(struct tas2552_data *tas2552)
{
	if (!tas2552)
		goto exit_no_debugfs;

	tas2552_dir = debugfs_create_dir("tas2552", NULL);
	if (!tas2552_dir)
		goto exit_no_debugfs;

	tas2552_reg_file = debugfs_create_file("registers",
				S_IRUGO, tas2552_dir, tas2552,
				&tas2552_registers_fops);
	if (!tas2552_reg_file)
		goto exit_destroy_dir;

	tas2552_set_reg_file = debugfs_create_file("set_reg",
				S_IWUSR, tas2552_dir, tas2552,
				&tas2552_get_set_reg_fops);
	if (!tas2552_set_reg_file)
		goto exit_destroy_set_reg;

	return;

exit_destroy_set_reg:
	debugfs_remove(tas2552_reg_file);
exit_destroy_dir:
	debugfs_remove(tas2552_dir);
exit_no_debugfs:

	return;
}
static inline void tas2552_remove_debugfs(void)
{
	debugfs_remove(tas2552_set_reg_file);
	debugfs_remove(tas2552_reg_file);
	debugfs_remove(tas2552_dir);
}
#else
static inline void tas2552_setup_debugfs(struct tas2552_data *tas2552)
{
}
static inline void tas2552_remove_debugfs(void)
{
}
#endif

static int tas2552_power(int on)
{
	int ret;
	struct tas2552_data *tas2552 = i2c_get_clientdata(tas2552_client);
	if (on) {
		ret = regulator_enable(tas2552->supply);
		if (ret < 0)
			pr_err("%s: Error enabling s4 regulator.\n", __func__);
		else
			tas2552->power_state = on;
	} else
		ret = regulator_disable(tas2552->supply);

	return ret;
}


static int __devinit tas2552_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct tas2552_data *tas2552;
	int err = 0;
	u8 dev_ver = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed\n");
		return -EIO;
	}

	tas2552 = kzalloc(sizeof(*tas2552), GFP_KERNEL);
	if (tas2552 == NULL) {
		dev_err(&client->dev,
			"Failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	/* set global variables */
	tas2552_client = client;
	i2c_set_clientdata(client, tas2552);

	/* enable regulator*/
	tas2552->supply = regulator_get(&client->dev, "tas_avdd");
	if (IS_ERR(tas2552->supply)) {
		pr_err("%s: Error getting S4 regulator.\n", __func__);
		err = PTR_ERR(tas2552->supply);
		goto reg_get;
	}

	regulator_set_voltage(tas2552->supply, 1800000, 1800000);

	tas2552_power(1);
	/* read version no */
	err = tas2552_reg_read(tas2552, TAS2552_VER_NUMBER_REG, &dev_ver);
	if (err != 0 || (dev_ver & 0xf) != 0x4) {
		dev_err(&client->dev, "Failed to verify tas2552 ver id!\n");
		goto tas2552_dev_id_fail;
	}
	err = tas2552_initialize();
	if (err != 0) {
		dev_err(&client->dev, "Failed to initialize tas2552!\n");
		goto init_fail;
	}
	/* setup debug fs interfaces */
	tas2552_setup_debugfs(tas2552);

	pr_info("tas2552 probed successfully\n");

	return 0;

init_fail:
tas2552_dev_id_fail:
	tas2552_power(0);
	regulator_put(tas2552->supply);
reg_get:
	kfree(tas2552);
	return err;
}

static int __devexit tas2552_remove(struct i2c_client *client)
{
	struct tas2552_data *tas2552 = i2c_get_clientdata(client);

	tas2552_power(0);
	regulator_put(tas2552->supply);
	tas2552_remove_debugfs();
	kfree(tas2552);
	return 0;
}

static const struct i2c_device_id tas2552_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tas2552_id);

static struct i2c_driver tas2552_driver = {
	.driver = {
			.name = NAME,
			.owner = THIS_MODULE,
	},
	.probe = tas2552_probe,
	.remove = __devexit_p(tas2552_remove),
	.id_table = tas2552_id,
};

static int __init tas2552_init(void)
{
	return i2c_add_driver(&tas2552_driver);
}

static void __exit tas2552_exit(void)
{
	i2c_del_driver(&tas2552_driver);
	return;
}

module_init(tas2552_init);
module_exit(tas2552_exit);

MODULE_DESCRIPTION("tas2552 driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility");
