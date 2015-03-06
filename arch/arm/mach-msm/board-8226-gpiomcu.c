/* Copyright (c) 2012-2015, mcu gpio init. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <soc/qcom/socinfo.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>


#define MCU_GPIO_BOOT0 111
#define MCU_GPIO_BOOT1 106
#define MCU_GPIO_RESET 87
#define GPIO_LEN	  16

struct gpio_mcu_info
{
	unsigned gpio;
	const char *label;
};

static struct gpio_mcu_info gpio_mcu []=
{
	{MCU_GPIO_BOOT0,"GPIO111"},
	{MCU_GPIO_BOOT1,"GPIO106"},
	{MCU_GPIO_RESET,"GPIO87"}
};

struct proc_dir_entry *mcu_dir;

static ssize_t mcu_write_proc_gpio(unsigned gpio, const char __user *buffer, size_t count)
{
	char b;
	unsigned i;
	char buf[GPIO_LEN] = {0};

	if (copy_from_user(&b, buffer, 1))
	{
		pr_err("Set value failed copy\n");
		return -EFAULT;
	}
	if ( b != '0' && b != '1')
	{
		pr_err("Set value invalid\n");
		return -EINVAL;
	}

	for(i = 0; i < sizeof(gpio_mcu) / sizeof(struct gpio_mcu_info); i ++)
	{
		if(gpio_mcu[i].gpio == gpio)
		{
			strncpy(buf, gpio_mcu[i].label, GPIO_LEN - 1);
			break;
		}
	}
	if(sizeof(gpio_mcu) / sizeof(struct gpio_mcu_info) == i)
	{
		pr_err("gpio %d for invalid.\n", gpio);
		return -EINVAL;
	}
	if (gpio_request(gpio, (const char*)buf))
	{
		pr_err( "Failed to request gpio %d\n", gpio);

		return -EFAULT;
	}

	if (b == '0')
	{
		gpio_direction_output(gpio, 0);
		pr_info("mcu_write_proc_gpio (%u) low.\n", gpio);
	}	
	else if (b == '1')
	{
		gpio_direction_output(gpio, 1);
		pr_info("mcu_write_proc_gpio (%u) high.\n", gpio);
	}
	gpio_free(gpio);
	return count;

}

static ssize_t mcu_read_proc_gpio_boot0
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{
	return 0;
}

static ssize_t mcu_write_proc_gpio_boot0
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	if (count < 1)
	{
		return -EINVAL;
	}

	return mcu_write_proc_gpio(MCU_GPIO_BOOT0, buffer, count);
}

static ssize_t mcu_read_proc_gpio_boot1
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{
	return 0;
}

static ssize_t mcu_write_proc_gpio_boot1
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	if (count < 1)
	{
		return -EINVAL;
	}

	return mcu_write_proc_gpio(MCU_GPIO_BOOT1, buffer, count);

}

static ssize_t mcu_read_proc_gpio_reset
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{
	return 0;
}

static ssize_t mcu_write_proc_gpio_reset
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	if (count < 1)
	{
		return -EINVAL;
	}
	
	return mcu_write_proc_gpio(MCU_GPIO_RESET, buffer, count);
}


static const struct file_operations proc_fops_gpio_boot0 = {
	.owner = THIS_MODULE,
	.read = mcu_read_proc_gpio_boot0,
	.write = mcu_write_proc_gpio_boot0,
};

static const struct file_operations proc_fops_gpio_boot1 = {
	.owner = THIS_MODULE,
	.read = mcu_read_proc_gpio_boot1,
	.write = mcu_write_proc_gpio_boot1,
};

static const struct file_operations proc_fops_gpio_reset = {
	.owner = THIS_MODULE,
	.read = mcu_read_proc_gpio_reset,
	.write = mcu_write_proc_gpio_reset,
};


static int __init board_mcu_gpio_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	mcu_dir = proc_mkdir("mcu", NULL);
	if (mcu_dir == NULL) 
	{
		pr_err("Unable to create /proc/mcu directory.\n");
		return -ENOMEM;
	}

	/* read/write mcu boot0 proc entries */
	ent = proc_create("gpio_mcu_boot0", 0660, mcu_dir, &proc_fops_gpio_boot0);
	if (ent == NULL) 
	{
		pr_err("Unable to create /proc/mcu/gpio_mcu_boot0 entry.\n");
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write mcu boot1 entries */
	ent = proc_create("gpio_mcu_boot1", 0660, mcu_dir, &proc_fops_gpio_boot1);
	if (ent == NULL) 
	{
		pr_err("Unable to create /proc/mcu/gpio_mcu_boot1 entry.\n");
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write mcu reset proc entries */
	ent = proc_create("gpio_mcu_reset", 0660, mcu_dir, &proc_fops_gpio_reset);
	if (ent == NULL) 
	{
		pr_err("Unable to create /proc/mcu/gpio_mcu_reset entry.\n");
		retval = -ENOMEM;
		goto fail;
	}

	return 0;

fail:
	remove_proc_entry("gpio_mcu_boot0", mcu_dir);
	remove_proc_entry("gpio_mcu_boot1", mcu_dir);
	remove_proc_entry("gpio_mcu_reset", mcu_dir);
	remove_proc_entry("mcu", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit board_mcu_gpio_exit(void)
{
	remove_proc_entry("gpio_mcu_boot0", mcu_dir);
	remove_proc_entry("gpio_mcu_boot1", mcu_dir);
	remove_proc_entry("gpio_mcu_reset", mcu_dir);
	remove_proc_entry("mcu", 0);
}

module_init(board_mcu_gpio_init);
module_exit(board_mcu_gpio_exit);
