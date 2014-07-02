/*
 * rawio_msgbus.c - rawio Message Bus driver.
 * Read or write message bus registers, based on the rawio framework.
 *
 * Copyright (c) 2013 Bin Gao <bin.gao@intel.com>
 *
 * This file is released under the GPLv2
 *
 *
 * read message bus registers:
 * echo "r msgbus <port> <addr> [<len>]" >
 *			/sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "r msgbus 2 0x30 4" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 *
 * write a message bus register:
 * echo "w msgbus <port> <addr> <val>" >
 *			/sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "w msgbus 2 0x30 0x20f8" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <asm/intel_mid_pcihelpers.h>
#include "rawio.h"

static int rawio_msgbus_read(struct rawio_driver *driver, int width, u64 *input,
	u8 *postfix, int input_num, void **output, int *output_num)
{
	int i, len;
	u8 msgbus_port;
	u16 msgbus_addr;
	u32 *buf;

	msgbus_port = (u8)input[0];
	msgbus_addr = (u16)input[1];

	len = 1;
	if (input_num == 3)
		len = (u16)input[2];

	buf = kzalloc(width * len, GFP_KERNEL);
	if (buf == NULL) {
		rawio_err("can't alloc memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++)
		*(buf + i) = intel_mid_msgbus_read32(msgbus_port,
						msgbus_addr + i);

	*output = buf;
	*output_num = len;
	return 0;
}

static int rawio_msgbus_write(struct rawio_driver *driver, int width, u64 *input,
			u8 *postfix, int input_num)
{
	u8 msgbus_port;
	u16 msgbus_addr;
	u32 value;

	msgbus_port = (u8)input[0];
	msgbus_addr = (u16)input[1];
	value = (u32)input[2];

	intel_mid_msgbus_write32(msgbus_port, msgbus_addr, value);
	return 0;
}

static struct rawio_ops rawio_msgbus_ops = {
	rawio_msgbus_read,
	NULL,
	rawio_msgbus_write,
};

static struct rawio_driver rawio_msgbus = {
	{NULL, NULL}, /* list node */
	"msgbus", /* driver name */

	/* read */
	3, /* max args */
	{TYPE_U8, TYPE_U16, TYPE_U16, TYPE_U8}, /* type of read args */
	2, /* min args */
	{ 0, }, /* args postfix */

	/* write */
	3, /* max args */
	{TYPE_U8, TYPE_U16, TYPE_U16, TYPE_U8}, /* type of write args */
	3, /* min args */
	{ 0, }, /* args postfix */

	1, /* index of arg that specifies the register or memory address */

	WIDTH_4, /* supported width */
	WIDTH_4, /* default width */

	"r msgbus <port> <addr> [<len>]\n"
	"w msgbus <port> <addr> <val>\n",
	&rawio_msgbus_ops,
	NULL
};

static int __init rawio_msgbus_init(void)
{
	if (rawio_register_driver(&rawio_msgbus))
		return -ENODEV;

	return 0;
}
module_init(rawio_msgbus_init);

static void __exit rawio_msgbus_exit(void)
{
	rawio_unregister_driver(&rawio_msgbus);
}
module_exit(rawio_msgbus_exit);

MODULE_DESCRIPTION("Rawio Message Bus driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Bin Gao <bin.gao@intel.com>");
MODULE_LICENSE("GPL v2");
