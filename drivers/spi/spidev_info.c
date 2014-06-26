/*
 * SPI debugfs interface for spidev register
 *
 * Copyright (C) 2014, Intel Corporation
 * Authors: Huiquan Zhong <huiquan.zhong@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

static struct spi_board_info spidev_info = {
	.modalias = "spidev",
	.max_speed_hz = 1000000,
	.bus_num = 1,
	.chip_select = 0,
	.mode = SPI_MODE_0,
};

static int spidev_debug_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t spidev_debug_write(struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	char buf[32];
	ssize_t buf_size;
	char *start = buf;
	unsigned int bus_num, cs_num;

	if (*ppos < 0 || !cnt)
		return -EINVAL;

	buf_size = min(cnt, (sizeof(buf)-1));

	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	while (*start == ' ')
		start++;
	bus_num = simple_strtoul(start, &start, 10);

	while (*start == ' ')
		start++;
	if (kstrtouint(start, 10, &cs_num))
		return -EINVAL;

	spidev_info.bus_num = bus_num;
	spidev_info.chip_select = cs_num;

	spi_register_board_info(&spidev_info, 1);

	return buf_size;
}

static const struct file_operations spidev_debug_fops = {
	.open		= spidev_debug_open,
	.write		= spidev_debug_write,
	.llseek		= generic_file_llseek,
};

struct dentry *spidev_node;
static __init int spidev_debug_init(void)
{

	spidev_node = debugfs_create_file("spidev_node", S_IFREG | S_IWUSR,
				NULL, NULL, &spidev_debug_fops);
	if (!spidev_node) {
		pr_err("Failed to create spidev_node debug file\n");
		return -ENOMEM;
	}

	return 0;
}

static __exit void spidev_debug_exit(void)
{
	debugfs_remove(spidev_node);
}
module_init(spidev_debug_init);
module_exit(spidev_debug_exit);
