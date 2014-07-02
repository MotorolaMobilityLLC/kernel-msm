/*
 * rawio_msr.c - rawio MSR driver.
 * Read or write x86 MSR registers, based on the rawio framework.
 *
 * Copyright (c) 2013 Bin Gao <bin.gao@intel.com>
 *
 * This file is released under the GPLv2
 *
 *
 * read MSR registers:
 * echo "r[4|8] msr <addr> [<cpu>]" >
 *			/sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 *      cat /sys/kernel/debug/rawio_output
 * By default it's 64bit read(r8) on all cpus.
 * e.g. echo "r msr 0x198" > /sys/kernel/debug/rawio_cmd
 *
 * write a message bus register:
 * echo "w msr <addr> <value> [<cpu>]" >
 *			/sys/kernel/debug/rawio_cmd
 * cat /sys/kernel/debug/rawio_output
 * e.g. echo "w msr 0x198 0xff002038102299a0 2" > /sys/kernel/debug/rawio_cmd
 *      cat /sys/kernel/debug/rawio_output
 * This is a 64bit write toward cpu 2(cpu index starts from 0).
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/seq_file.h>
#include <asm/msr.h>
#include "rawio.h"

static int rawio_msr_read_and_show(struct rawio_driver *driver, int width,
				u64 *input, u8 *postfix, int input_num)
{
	int cpu, ret, i, count;
	u32 msr_addr, data[2];
	char seq_buf[32];

	msr_addr = (u32)input[0];

	if (input_num == 2) {
		cpu = (int)input[1];
		if ((cpu < 0) || (cpu >= nr_cpu_ids)) {
			rawio_err("cpu should be between 0 - %d\n",
						nr_cpu_ids - 1);
			return -EINVAL;
		}
	} else
		cpu = -1;

	if (cpu < 0) {
		/* loop for all cpus */
		i = 0;
		count = nr_cpu_ids;
	} else {
		/* loop for one cpu */
		i = cpu;
		count = cpu + 1;
	}

	for (; i < count; i++) {
		ret = rdmsr_safe_on_cpu(i, msr_addr, &data[0], &data[1]);
		if (ret) {
			rawio_err("msr read error: %d\n", ret);
			return -EIO;
		} else {
			if (width == WIDTH_4) {
				snprintf(seq_buf, 31, "[cpu %2d] %08x\n",
							i, data[0]);
				seq_puts(driver->s, seq_buf);
			} else {
				snprintf(seq_buf, 32, "[cpu %2d] %08x%08x\n",
					 i, data[1], data[0]);
				seq_puts(driver->s, seq_buf);
			}
		}
	}

	return 0;
}
static int rawio_msr_write(struct rawio_driver *driver, int width, u64 *input,
			u8 *postfix, int input_num)
{
	int cpu, ret, i, count;
	u32 msr_addr, data[2];
	u64 value;

	msr_addr = (u32)input[0];
	value = (u64)input[1];

	if (input_num == 3) {
		cpu = (int)input[2];
		if ((cpu < 0) || (cpu >= nr_cpu_ids)) {
			rawio_err("cpu should be between 0 - %d\n",
						nr_cpu_ids - 1);
			return -EINVAL;
		}
	} else
		cpu = -1;

	if (width == WIDTH_4) {
		ret = rdmsr_safe_on_cpu(cpu, msr_addr,
				&data[0], &data[1]);
		if (ret) {
			rawio_err("msr write error: %d\n", ret);
			return -EIO;
		}
		data[0] = (u32)value;
	} else {
		data[0] = (u32)value;
		data[1] = (u32)(value >> 32);
	}

	if (cpu < 0) {
		/* loop for all cpus */
		i = 0;
		count = nr_cpu_ids;
	} else {
		/* loop for one cpu */
		i = cpu;
		count = cpu + 1;
	}

	for (; i < count; i++) {
		ret = wrmsr_safe_on_cpu(i, msr_addr, data[0], data[1]);
		if (ret) {
			rawio_err("msr write error: %d\n", ret);
			return -EIO;
		} else
			seq_puts(driver->s, "write succeeded.\n");
	}

	return 0;
}

static struct rawio_ops rawio_msr_ops = {
	NULL,
	rawio_msr_read_and_show,
	rawio_msr_write,
};

static struct rawio_driver rawio_msr = {
	{NULL, NULL}, /* list node */
	"msr", /* driver name */

	/* read */
	2, /* max args */
	{TYPE_U32, TYPE_U8}, /* type of read args */
	1, /* min args */
	{ 0, }, /* args postfix */

	/* write */
	3, /* max args */
	{TYPE_U32, TYPE_U64, TYPE_U8}, /* type of write args */
	2, /* min args */
	{ 0, }, /* args postfix */

	0, /* index of arg that specifies the register or memory address */

	WIDTH_4 | WIDTH_8, /* supported width */
	WIDTH_8, /* default width */

	"r msr <addr> [<cpu>]\n"
	"w msr <addr> <val> [<cpu>]\n",
	&rawio_msr_ops,
	NULL
};

static int __init rawio_msr_init(void)
{
	if (rawio_register_driver(&rawio_msr))
		return -ENODEV;

	return 0;
}
module_init(rawio_msr_init);

static void __exit rawio_msr_exit(void)
{
	rawio_unregister_driver(&rawio_msr);
}
module_exit(rawio_msr_exit);

MODULE_DESCRIPTION("Rawio MSR driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Bin Gao <bin.gao@intel.com>");
MODULE_LICENSE("GPL v2");
