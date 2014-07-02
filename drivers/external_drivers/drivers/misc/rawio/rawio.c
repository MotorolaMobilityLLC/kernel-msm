/*
 * rawio.c - a debugfs based framework for reading/writing registers
 * from a I/O device.
 * With pluggable rawio drivers, it can support PCI devices, I2C devices,
 * memory mapped I/O devices, etc.
 * It's designed for helping debug Linux device drivers on embedded system or
 * SoC platforms.
 *
 * Copyright (c) 2013 Bin Gao <bin.gao@intel.com>
 *
 * This file is released under the GPLv2
 *
 *
 * Two files are created in debugfs root folder: rawio_cmd and rawio_output.
 * To read or write via the rawio debugfs interface, first echo a rawio
 * command to the file rawio_cmd, then cat the file rawio_output:
 * $ echo "<rawio command>" > /sys/kernel/debug/rawio_cmd
 * $ cat /sys/kernel/debug/rawio_output
 * The cat command is required for both read and write operations.
 * For details of rawio command format, see specific rawio drivers.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "rawio.h"

#define SHOW_NUM_PER_LINE	(32 / active_width)
#define LINE_WIDTH		32
#define IS_WHITESPACE(c)	((c) == ' ' || (c) == '\t' || (c) == '\n')

static struct dentry *rawio_cmd_dentry, *rawio_output_dentry;
static char rawio_cmd_buf[RAWIO_CMD_LEN], rawio_err_buf[RAWIO_ERR_LEN + 1];
static DEFINE_MUTEX(rawio_lock);
static LIST_HEAD(rawio_driver_head);
static struct rawio_driver *active_driver;
static enum width active_width;
static enum ops active_ops;
static u64 args_val[RAWIO_ARGS_MAX];
static u8 args_postfix[RAWIO_ARGS_MAX];
static int num_args_val;

static void store_value(u64 *where, void *value, enum type type)
{
	switch (type) {
	case TYPE_U8:
		*(u8 *)where = *(u8 *)value;
		break;
	case TYPE_U16:
		*(u16 *)where = *(u16 *)value;
		break;
	case TYPE_U32:
		*(u32 *)where = *(u32 *)value;
		break;
	case TYPE_U64:
		*where = *(u64 *)value;
		break;
	case TYPE_S8:
		*(s8 *)where = *(s8 *)value;
		break;
	case TYPE_S16:
		*(s16 *)where = *(s16 *)value;
		break;
	case TYPE_S32:
		*(s32 *)where = *(s32 *)value;
		break;
	case TYPE_S64:
		*(s64 *)where = *(s64 *)value;
		break;
	default:
		break;
	}
}

int rawio_register_driver(struct rawio_driver *driver)
{
	mutex_lock(&rawio_lock);
	list_add_tail(&driver->list, &rawio_driver_head);
	mutex_unlock(&rawio_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(rawio_register_driver);

int rawio_unregister_driver(struct rawio_driver *driver)
{
	mutex_lock(&rawio_lock);
	list_del(&driver->list);
	mutex_unlock(&rawio_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(rawio_unregister_driver);

void rawio_err(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(rawio_err_buf, RAWIO_ERR_LEN, fmt, args);
	va_end(args);
}
EXPORT_SYMBOL_GPL(rawio_err);

static int parse_arguments(char *input, char **args)
{
	int count, located;
	char *p = input;
	int input_len = strlen(input);

	count = 0;
	located = 0;
	while (*p != 0) {
		if (p - input >= input_len)
			break;

		/* Locate the first character of a argument */
		if (!IS_WHITESPACE(*p)) {
			if (!located) {
				located = 1;
				args[count++] = p;
				if (count > RAWIO_ARGS_MAX)
					break;
			}
		} else {
			if (located) {
				*p = 0;
				located = 0;
			}
		}
		p++;
	}

	return count;
}

static int parse_driver_args(struct rawio_driver *driver, char **arg_list,
		int num_args, enum ops ops, u64 *arg_val, u8 *postfix)
{
	int i;
	size_t str_len;
	enum type type;
	u64 value;
	char *str;
	char *args_postfix;

	for (i = 0; i < num_args; i++) {
		switch (ops) {
		case OPS_RD:
			type = driver->args_rd_types[i];
			args_postfix = driver->args_rd_postfix;
			break;
		case OPS_WR:
			type = driver->args_wr_types[i];
			args_postfix = driver->args_wr_postfix;
			break;
		default:
			return -EINVAL;
		}

		if (args_postfix[i]) {
			str = (char *) arg_list[i];
			str_len = strlen(str);
			if (str[str_len - 1] == args_postfix[i]) {
				postfix[i] = 1;
				str[str_len - 1] = 0;
			} else {
				postfix[i] = 0;
			}
		}

		if (kstrtou64(arg_list[i], 0, &value))
				goto failed;
		store_value(arg_val + i, &value, type);
	}

	return 0;

failed:
	snprintf(rawio_err_buf, RAWIO_ERR_LEN,
		"invalid argument %s, usage:\n", arg_list[i]);
	strncat(rawio_err_buf, driver->help, RAWIO_ERR_LEN -
					strlen(rawio_err_buf));
	return -EINVAL;
}

static struct rawio_driver *find_driver(const char *name)
{
	struct rawio_driver *driver;

	mutex_lock(&rawio_lock);
	list_for_each_entry(driver, &rawio_driver_head, list) {
		if (!strncmp(driver->name, name, strlen(name))) {
			mutex_unlock(&rawio_lock);
			return driver;
		}
	}
	mutex_unlock(&rawio_lock);

	return NULL;
}

static ssize_t rawio_cmd_write(struct file *file, const char __user *buf,
				size_t len, loff_t *offset)
{
	char cmd[RAWIO_CMD_LEN];
	char *arg_list[RAWIO_ARGS_MAX];
	int num_args;
	enum ops ops;
	enum width width;
	struct rawio_driver *driver;

	rawio_err_buf[0] = 0;

	if (len >= RAWIO_CMD_LEN) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN, "command is too long.\n"
					"max allowed command length is %d\n",
							RAWIO_CMD_LEN);
		goto done;
	}

	if (copy_from_user(cmd, buf, len)) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"copy_from_user() failed.\n");
		goto done;
	}
	cmd[len] = 0;

	rawio_cmd_buf[0] = 0;
	strncpy(rawio_cmd_buf, cmd, len);
	rawio_cmd_buf[len] = 0;

	num_args = parse_arguments(cmd, arg_list);
	if (num_args < RAWIO_ARGS_MIN) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"invalid command(too few arguments)\n");
		goto done;
	}
	if (num_args > RAWIO_ARGS_MAX) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"invalid command(too many arguments)\n");
		goto done;
	}

	/* arg 0: ops(read/write) and width (8/16/32/64 bit) */
	if (arg_list[0][0] == 'r')
		ops = OPS_RD;
	else if (arg_list[0][0] == 'w')
		ops = OPS_WR;
	else {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"invalid operation: %c, only r and w are supported\n",
							 arg_list[0][0]);
		goto done;
	}

	if (strlen(arg_list[0]) >= 3) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"invalid bus width: %s, only 1 2 4 8 are supported\n",
							 arg_list[0] + 1);
		goto done;
	}

	if (strlen(arg_list[0]) == 1)
		width = WIDTH_DEFAULT;
	else {
		switch (arg_list[0][1]) {
		case '1':
			width = WIDTH_1;
			break;
		case '2':
			width = WIDTH_2;
			break;
		case '4':
			width = WIDTH_4;
			break;
		case '8':
			width = WIDTH_8;
			break;
		default:
			snprintf(rawio_err_buf, RAWIO_ERR_LEN,
				"invalid bus width: %c, only 1 2 4 8 are supported\n",
								arg_list[0][1]);
			goto done;
		}
	}

	/* arg1: driver name */
	driver = find_driver(arg_list[1]);
	if (!driver) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"unsupported driver type: %s\n", arg_list[1]);
		goto done;
	}

	if (width == WIDTH_DEFAULT)
		width = driver->default_width;

	if (!(width & driver->supported_width)) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"unsupported driver width: %s\n", arg_list[0]);
		goto done;
	}

	/* arg2, ..., argn: driver specific arguments */
	num_args = num_args - 2;
	if (((ops == OPS_RD) && (num_args > driver->args_rd_max_num)) ||
		((ops == OPS_WR) && (num_args > driver->args_wr_max_num))) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"too many arguments, usage:\n");
		strncat(rawio_err_buf, driver->help, RAWIO_ERR_LEN -
						strlen(rawio_err_buf));
		goto done;
	}
	if (((ops == OPS_RD) && (num_args < driver->args_rd_min_num)) ||
		((ops == OPS_WR) && (num_args < driver->args_wr_min_num))) {
		snprintf(rawio_err_buf, RAWIO_ERR_LEN,
			"too few arguments, usage:\n");
		strncat(rawio_err_buf, driver->help, RAWIO_ERR_LEN -
						strlen(rawio_err_buf));
		goto done;
	}

	if (parse_driver_args(driver, arg_list + 2, num_args, ops,
				args_val, args_postfix))
		goto done;

	active_driver = driver;
	active_width = width;
	active_ops = ops;
	num_args_val = num_args;
done:
	return len;
}

static int rawio_output_show(struct seq_file *s, void *unused)
{
	u32 start, end, start_nature, end_nature;
	int ret, i, comp1, comp2, output_len;
	void *output;
	char seq_buf[16];

	mutex_lock(&rawio_lock);

	if (strlen(rawio_err_buf) > 0) {
		seq_puts(s, rawio_err_buf);
		mutex_unlock(&rawio_lock);
		return 0;
	}

	active_driver->s = s;

	if (active_ops == OPS_WR) {
		ret = active_driver->ops->write(active_driver, active_width,
				args_val, args_postfix, num_args_val);
		if (ret)
			seq_puts(s, rawio_err_buf);
		else
			seq_puts(s, "write succeeded.\n");

		mutex_unlock(&rawio_lock);
		return 0;
	}

	if (active_driver->ops->read_and_show) {
		ret = active_driver->ops->read_and_show(active_driver,
			active_width, args_val, args_postfix, num_args_val);
		if (ret)
			seq_puts(s, rawio_err_buf);
		mutex_unlock(&rawio_lock);
		return 0;
	}

	ret = active_driver->ops->read(active_driver, active_width, args_val,
			args_postfix, num_args_val, &output, &output_len);
	if (ret) {
		seq_puts(s, rawio_err_buf);
		mutex_unlock(&rawio_lock);
		return 0;
	}

	start_nature = (u32)args_val[active_driver->addr_pos];
	start = (start_nature / LINE_WIDTH) * LINE_WIDTH;
	end_nature = start_nature + (output_len - 1) * active_width;
	end = (end_nature / LINE_WIDTH + 1) * LINE_WIDTH - active_width;
	comp1 = (start_nature - start) / active_width;
	comp2 = (end - end_nature) / active_width;

	mutex_unlock(&rawio_lock);

	for (i = 0; i < comp1 + comp2 + output_len; i++) {
		if ((i % SHOW_NUM_PER_LINE) == 0) {
			snprintf(seq_buf, sizeof(seq_buf), "[%08x]",
						(u32)start + i * 4);
			seq_puts(s, seq_buf);
		}
		if (i < comp1 || i >= output_len + comp1) {
			switch (active_width) {
			case WIDTH_8:
				seq_puts(s, " ****************");
				break;
			case WIDTH_4:
				seq_puts(s, " ********");
				break;
			case WIDTH_2:
				seq_puts(s, " ****");
				break;
			case WIDTH_1:
				seq_puts(s, " **");
				break;
			default:
				break;
			}
		} else {
			switch (active_width) {
			case WIDTH_8:
				snprintf(seq_buf, sizeof(seq_buf), "[%016llx]",
					*((u64 *)output + i - comp1));
				seq_puts(s, seq_buf);
				break;
			case WIDTH_4:
				snprintf(seq_buf, sizeof(seq_buf), " %08x",
					*((u32 *)output + i - comp1));
				seq_puts(s, seq_buf);
				break;
			case WIDTH_2:
				snprintf(seq_buf, sizeof(seq_buf), " %04x",
					*((u16 *)output + i - comp1));
				seq_puts(s, seq_buf);
				break;
			case WIDTH_1:
				snprintf(seq_buf, sizeof(seq_buf), " %02x",
					*((u8 *)output + i - comp1));
				seq_puts(s, seq_buf);
				break;
			default:
				break;
			}
		}

		if ((i + 1) % SHOW_NUM_PER_LINE == 0)
			seq_puts(s, "\n");
	}

	kfree(output);
	return 0;
}

static int rawio_cmd_show(struct seq_file *s, void *unused)
{
	seq_puts(s, rawio_cmd_buf);
	return 0;
}

static int rawio_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, rawio_cmd_show, NULL);
}

static const struct file_operations rawio_cmd_fops = {
	.owner		= THIS_MODULE,
	.open		= rawio_cmd_open,
	.read		= seq_read,
	.write		= rawio_cmd_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int rawio_output_open(struct inode *inode, struct file *file)
{
	return single_open(file, rawio_output_show, NULL);
}

static const struct file_operations rawio_output_fops = {
	.owner		= THIS_MODULE,
	.open		= rawio_output_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init rawio_init(void)
{
	rawio_cmd_dentry = debugfs_create_file("rawio_cmd",
		S_IFREG | S_IRUGO | S_IWUSR, NULL, NULL, &rawio_cmd_fops);
	rawio_output_dentry = debugfs_create_file("rawio_output",
		S_IFREG | S_IRUGO, NULL, NULL, &rawio_output_fops);
	if (!rawio_cmd_dentry || !rawio_output_dentry) {
		pr_err("rawio: can't create debugfs node\n");
		return -EFAULT;
	}

	return 0;
}
module_init(rawio_init);

static void __exit rawio_exit(void)
{
	debugfs_remove(rawio_cmd_dentry);
	debugfs_remove(rawio_output_dentry);
}
module_exit(rawio_exit);

MODULE_DESCRIPTION("Raw IO read/write utility framework driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Bin Gao <bin.gao@intel.com>");
MODULE_LICENSE("GPL v2");
