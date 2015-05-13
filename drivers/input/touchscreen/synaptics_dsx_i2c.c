/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#include <linux/input/synaptics_rmi_dsx.h>
#include "synaptics_dsx_i2c.h"
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
#include "touchx.h"
#endif
#include <linux/pinctrl/consumer.h>
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#define DRIVER_NAME "synaptics_dsx_i2c"
#define INPUT_PHYS_NAME "synaptics_dsx_i2c/input0"
#define TYPE_B_PROTOCOL

#define RMI4_WAIT_READY 0
#define RMI4_HW_RESET 1
#define RMI4_SW_RESET 2

#define NO_0D_WHILE_2D
/*
#define REPORT_2D_Z
*/
#define REPORT_2D_W

#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define EXP_FN_DET_INTERVAL 1000 /* ms */
#define POLLING_PERIOD 1 /* ms */
#define SYN_I2C_RETRY_TIMES 10
#define MAX_ABS_MT_TOUCH_MAJOR 15
#define SYN_MAX_BUTTONS 4

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F01_QUERY1_OFFSET 1
#define F01_QUERY21_OFFSET 21
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12
#define F12_STD_QUERY_LEN 10
#define F12_STD_CTRL_LEN 4
#define F12_STD_DATA_LEN 80

#define RESET_GPIO_NAME "touch_reset"
#define IRQ_GPIO_NAME "touch_irq"

#define SYDBG(fmt, args...)	printk(KERN_ERR "%s: " fmt, __func__, ##args)
#define SYDBG_REG(subpkt, fld) SYDBG(#subpkt "." #fld " = 0x%02X\n", subpkt.fld)

static void synaptics_dsx_resumeinfo_start(
		struct synaptics_rmi4_data *rmi4_data);
static void synaptics_dsx_resumeinfo_finish(
		struct synaptics_rmi4_data *rmi4_data);
static void synaptics_dsx_resumeinfo_isr(
		struct synaptics_rmi4_data *rmi4_data);
static void synaptics_dsx_resumeinfo_purgeoff(
		struct synaptics_rmi4_data *rmi4_data);
static void synaptics_dsx_resumeinfo_ignore(
		struct synaptics_rmi4_data *rmi4_data);
static void synaptics_dsx_resumeinfo_touch(
		struct synaptics_rmi4_data *rmi4_data);

/* F12 packet register description */
static struct {
	unsigned char max_x_lsb;
	unsigned char max_x_msb;
	unsigned char max_y_lsb;
	unsigned char max_y_msb;
} f12_c08_0;

static struct f12_c20_0_type {
	unsigned char x_suppression;
	unsigned char y_suppression;
} f12_c20_0;

static struct f12_c23_0_type {
	union {
		struct {
			unsigned char finger:1;
			unsigned char stylus:1;
			unsigned char palm:1;
			unsigned char unclassified:1;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
} f12_c23_0;

static struct f12_c23_1_type {
	unsigned char max_num_reported_objects;
} f12_c23_1;

static struct {
	unsigned char reported_bytes_per_object;
} f12_c28_0;

static struct synaptics_rmi4_subpkt f12_c08[] = {
	RMI4_SUBPKT(f12_c08_0),
};

static struct synaptics_rmi4_subpkt f12_c20[] = {
	RMI4_SUBPKT(f12_c20_0),
};

static struct synaptics_rmi4_subpkt f12_c23[] = {
	RMI4_SUBPKT(f12_c23_0),
	RMI4_SUBPKT(f12_c23_1),
};

static struct synaptics_rmi4_subpkt f12_c28[] = {
	RMI4_SUBPKT(f12_c28_0),
};

static struct synaptics_rmi4_packet_reg f12_ctrl_reg_array[] = {
	RMI4_REG(8, f12_c08),
	RMI4_REG(20, f12_c20),
	RMI4_REG(23, f12_c23),
	RMI4_REG(28, f12_c28),
};

static struct {
	union {
		struct {
			unsigned char configured:1;
			unsigned char report_rate:1;
			unsigned char charger_connected:1;
			unsigned char reserved:2;
			unsigned char no_sleep:1;
			unsigned char sleep_mode:2;
		} __packed;
		unsigned char data[1];
	};
} f01_c0_0;

static struct {
	unsigned char recalibration_interval;
} f01_c9_0;


static struct synaptics_rmi4_subpkt f01_c0[] = {
	RMI4_SUBPKT_STATIC(0, f01_c0_0),
};

static struct synaptics_rmi4_subpkt f01_c9[] = {
	RMI4_SUBPKT(f01_c9_0),
};

static struct synaptics_rmi4_packet_reg f01_ctrl_reg_array[] = {
	RMI4_REG_STATIC(0, f01_c0, 1),
	RMI4_REG(9, f01_c9),
};

static struct synaptics_rmi4_func_packet_regs synaptics_cfg_regs[2] = {
	{
		.f_number = SYNAPTICS_RMI4_F12,
		.base_addr = 0,
		.nr_regs = ARRAY_SIZE(f12_ctrl_reg_array),
		.regs = f12_ctrl_reg_array,
	},
	{
		.f_number = SYNAPTICS_RMI4_F01,
		.base_addr = 0,
		.nr_regs = ARRAY_SIZE(f01_ctrl_reg_array),
		.regs = f01_ctrl_reg_array,
	},
};

static unsigned char tsb_buff_clean_flag = 1;

static char *synaptics_dsx_find_patch(char *head, char *delimiters, char **next)
{
	char *patch = head;
	for (; patch; patch = *next) {
		*next = strpbrk(patch, delimiters);
		if (*next)
			*(*next)++ = '\0';
		patch = skip_spaces(patch);
		if (!patch || !*patch || *patch == '#')
			continue;
		else
			break;
	}
	return patch;
}

static int synaptics_dsx_parse_patch(int func, char *query,
		struct synaptics_dsx_patch *patch_ptr)
{
	int i, ii, error, num_of_bytes;
	u8 data[64];
	char *next, *bitmask_p, *subpkt_p, *value_p, *pair = query;
	long regstr_v, bitmask_v, subpkt_v, value_v;
	struct synaptics_dsx_func_patch *patch;

	for (i = 0; pair; pair = next, i++) {
		num_of_bytes = 1;
		bitmask_v = subpkt_v = 0L;

		pair = synaptics_dsx_find_patch(pair, ",", &next);

		value_p = strpbrk(pair, "=");
		if (!value_p) {
			pr_err("F%x[%d]: invalid syntax '%s'\n",
				func, i, pair);
			continue;
		}

		/* make sure string is null terminated */
		*value_p = '\0';

		subpkt_p = strpbrk(pair, ":");
		if (subpkt_p) {
			*subpkt_p++ = '\0';
			error = kstrtol(subpkt_p, 10, &subpkt_v);
			if (error) {
				pr_err("F%x[%d]: subpacket conv error\n",
					func, i);
				continue;
			}
		}

		error = kstrtol(pair, 10, &regstr_v);
		if (error) {
			pr_err("F%x[%d]: register conv error\n",
				func, i);
			continue;
		}

		/* advance to data */
		++value_p;

		bitmask_p = strpbrk(value_p, "&");
		if (bitmask_p) {
			*bitmask_p++ = '\0';
			error = kstrtol(bitmask_p, 16, &bitmask_v);
			if (error)
				pr_err("F%x[%d]: bitmask conv error\n",
					func, i);
		}

		/* detect multiple bytes data */
		if ((strlen(value_p)%2) == 0) {
			num_of_bytes = strlen(value_p)/2;
			pr_debug("F%x[%d]: %d byte(s) of data\n",
					func, i, num_of_bytes);
		}

		for (ii = 0; ii < num_of_bytes; ii++) {
			char hex_val[3] = {0};
			hex_val[0] = *value_p;
			hex_val[1] = *(value_p + 1);
			error = kstrtol(hex_val, 16, &value_v);
			if (error)
				pr_err("F%x[%d]: value conv error\n", func, i);
			data[ii] = (u8)value_v;
			value_p += 2;
		}

		/* primitive data validation */
		if (func <= 0 || func > 255 ||
		    regstr_v < 0 || regstr_v > 255) {
			pr_err("F%x[%d]: invalid values\n", func, i);
			continue;
		}

		patch = kzalloc(sizeof(*patch), GFP_KERNEL);
		if (!patch) {
			pr_err("failed to alloc mem\n");
			return -ENOMEM;
		}

		patch->data = kzalloc(num_of_bytes, GFP_KERNEL);
		if (!patch->data) {
			pr_err("failed to alloc mem\n");
			return -ENOMEM;
		}

		patch->func = (u8)func;
		patch->regstr = (u8)regstr_v;
		patch->subpkt = (u8)subpkt_v;
		patch->size = (u8)num_of_bytes;
		patch->bitmask = (u8)bitmask_v;
		memcpy(patch->data, data, num_of_bytes);

		pr_debug("F%x[%d], register %d, subpkt %d, size %d, mask %x\n",
			patch->func, i, patch->regstr, patch->subpkt,
			patch->size, patch->bitmask);
		patch_ptr->cfg_num++;
		list_add_tail(&patch->link, &patch_ptr->cfg_head);
	}

	return 0;
}

/*
 * Special settings passed to the driver via device tree
 * Example: "F12@11:2=1fa0;F1@1=1;"
 *   Where:
 *      F12     - decimal object number
 *      @       - delimits function number and following patch sets
 *      11:2    - decimal register and subpacket numbers (0 offset)
 *      26=a    - register number and hex value to assign
 *      2=1fa0  - 2 bytes of data to write to the register
 */
static void synaptics_dsx_parse_string(struct synaptics_rmi4_data *data,
		const char *patch_ptr, struct synaptics_dsx_patch *patch)
{
	struct device *dev = &data->i2c_client->dev;
	long number_v;
	char *patch_string;
	char *config_p, *next, *patch_set;
	int i, error;

	patch_string = kstrdup(patch_ptr, GFP_KERNEL);
	for (i = 0, patch_set = patch_string; patch_set; patch_set = next) {
		patch_set = synaptics_dsx_find_patch(patch_set, ";\n", &next);
		if (!patch_set)
			break;

		dev_dbg(dev, "patch set %d: \"%s\"\n", i, patch_set);

		config_p = strpbrk(patch_set, "@");
		if ((*patch_set != 'F' && *patch_set != 'f') || !config_p) {
			dev_err(dev, "invalid syntax '%s'\n", patch_set);
			continue;
		}

		/* strip non digits */
		*config_p++ = '\0';

		error = kstrtol(++patch_set, 16, &number_v);
		if (error) {
			dev_err(dev, "kstrtol error %d\n", error);
			continue;
		}

		error = synaptics_dsx_parse_patch(
					(int)number_v, config_p, patch);
		if (error < 0) {
			dev_err(dev, "invalid patch; parse error %d\n", error);
			continue;
		}

		i++;
	}
	kfree(patch_string);
	if (patch->cfg_num)
		dev_info(dev, "processed %d patch sets for %d functions\n",
			patch->cfg_num, i);
	else
		dev_info(dev, "no valid patch sets found\n");
}

#define LAST_SUBPACKET_ROW_IND_MASK 0x80
#define NR_SUBPKT_PRESENCE_BITS 7
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
struct touchxs touchxp;
EXPORT_SYMBOL(touchxp);
#endif

static struct synaptics_rmi4_func_packet_regs *find_function(int f_number)
{
	struct synaptics_rmi4_func_packet_regs *regs = NULL;
	int i;
	for (i = 0; i < ARRAY_SIZE(synaptics_cfg_regs); i++)
		if (synaptics_cfg_regs[i].f_number == f_number) {
			regs = &synaptics_cfg_regs[i];
			break;
		}
	return regs;
}

static struct synaptics_rmi4_packet_reg *find_packet_reg(
	struct synaptics_rmi4_func_packet_regs *regs, int number)
{
	struct synaptics_rmi4_packet_reg *reg = NULL;
	int r;
	for (r = 0; r < regs->nr_regs; r++)
		if (number == regs->regs[r].r_number) {
			reg = &regs->regs[r];
			break;
		}
	return reg;
}

int synaptics_rmi4_scan_f12_reg_info(
	struct synaptics_rmi4_data *rmi4_data,
	unsigned short query_addr,
	unsigned short regs_base_addr,
	struct synaptics_rmi4_func_packet_regs *regs)
{
	unsigned char sz, mask;
	int ii, jj, r, s, retval, max_reg_number;
	unsigned short r_offset;
	unsigned short addr = query_addr;
	unsigned char data[255];
	bool r_exist[128] = {false};

	for (r = 0; r < regs->nr_regs; ++r) {
		regs->regs[r].offset = -1;
		regs->regs[r].size = 0;
		kfree(regs->regs[r].data);
		for (s = 0; s < regs->regs[r].nr_subpkts; ++s) {
			regs->regs[r].subpkt[s].present = 0;
			if (regs->regs[r].subpkt[s].data &&
					regs->regs[r].subpkt[s].size)
				memset(regs->regs[r].subpkt[s].data, 0,
					regs->regs[r].subpkt[s].size);
		}
	}

	regs->base_addr = regs_base_addr;
	retval = rmi4_data->i2c_read(rmi4_data, addr, &sz, 1);
	pr_debug("size of reg presence = %d, addr 0x%x\n", sz, addr);
	if (retval < 0)
		return retval;
	if (!sz)
		return -EIO;
	/* Scan register presence */
	retval = rmi4_data->i2c_read(rmi4_data, ++addr, data, sz);
	if (retval < 0)
		return retval;
	if (!data[0]) {
		pr_err("packet register size greater 255 bytes"
			" not supported\n");
		return -ENOSYS;
	}
	ii = 1;
	for (r = 0, r_offset = 0; ii < sz; ++ii) {
		pr_debug("reg presence [%d] = 0x%02x\n", ii, data[ii]);
		for (jj = 0, mask = 1; jj < 8; ++jj, ++r, mask <<= 1) {
			struct synaptics_rmi4_packet_reg *reg =
						find_packet_reg(regs, r);
			bool present = (data[ii] & mask) != 0;
			bool allocated = reg ? true : false;
			if (present != allocated)
				pr_debug("  reg: r%d is%s present, but was%s"
						" expected\n", r,
						present ? "" : " NOT",
						allocated ? "" : " NOT");
			if (!present) {
				r_exist[r] = false;
				continue;
			}
			r_exist[r] = true;
			if (allocated)
				reg->offset = r_offset;
			pr_debug("  r%d offset = %d\n", r, r_offset);
			r_offset++;
		}
	}

	max_reg_number = r;
	pr_debug("max register number = %d\n", max_reg_number);

	/* Scan register size and subpacket presence*/
	sz = data[0];
	pr_debug("subpacket presence sz = %d, addr 0x%x\n", sz, addr+1);
	retval = rmi4_data->i2c_read(rmi4_data, ++addr, data, sz);
	if (retval < 0)
		return retval;
	for (r = 0, ii = 0; r < max_reg_number && ii < sz; ++r) {
		unsigned int reg_size, expected_reg_size;
		struct synaptics_rmi4_packet_reg *reg =
					find_packet_reg(regs, r);
		if (!r_exist[r]) {
			pr_debug("r%d does not exist; go to the next...\n", r);
			continue;
		}
		reg_size = data[ii++];
		if (!reg_size) {
			pr_err("packet register size greater 255 bytes"
				" not supported\n");
			return -ENOSYS;
		}
		if (reg && reg->offset != -1) {
			reg->data = kzalloc(reg_size, GFP_KERNEL);
			if (reg->data) {
				reg->size = reg_size;
				pr_debug("r%d allocated %d bytes buffer @%p\n",
					reg->r_number, reg->size, reg->data);
			} else
				pr_err("cannot alloc register data buffer\n");
		}
		pr_debug("r%d sz = %d\n", r, reg_size);
		expected_reg_size = 0;
		for (s = 0; ii < sz;) {
			pr_debug("  subpkt presence [%d] = 0x%02x\n",
				ii, data[ii]);
			for (jj = 0, mask = 1; jj < NR_SUBPKT_PRESENCE_BITS;
					++jj, ++s, mask <<= 1) {
				struct synaptics_rmi4_subpkt *subpkt = NULL;
				bool present = (data[ii] & mask) != 0;
				bool expected = false;
				if (reg && s < reg->nr_subpkts) {
					subpkt = reg->subpkt + s;
					expected = subpkt->expected;
				}
				if (!present || !expected) {
					if (present != expected)
						pr_debug("    subpacket:"
							" r%d s%d is%s present,"
							" but was%s expected\n",
							r, s,
							present ? "" : " NOT",
							expected ? "" : " NOT");
					continue;
				}
				pr_debug("    r%d.s%d is present\n", r, s);
				if (subpkt) {
					subpkt->present = true;
					expected_reg_size += subpkt->size;
				}
			}
			if ((data[ii++] & LAST_SUBPACKET_ROW_IND_MASK) == 0) {
				pr_debug("no more subpackets\n");
				break;
			}
		}
		if (reg && reg->expected && reg->size != expected_reg_size) {
			pr_debug("  r%d size error:"
				" expected %d actual is %d\n",
				r, expected_reg_size, reg->size);
		}
	}
	return 0;
}

int synaptics_rmi4_read_packet_reg(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs, int r_number)
{
	int s, retval, offset;
	struct synaptics_rmi4_packet_reg *reg = find_packet_reg(regs, r_number);

	if (!reg || !reg->size) {
		pr_err("register %d not found or zero size\n", r_number);
		return -EINVAL;
	}

	if (reg->offset == -1) {
		pr_err("touch register error: can't read r%d - not present\n",
			r_number);
		return -ENOENT;
	}

	pr_debug("F%02x reading r%d@%x, size %d to @%p\n", regs->f_number,
			reg->r_number, regs->base_addr + reg->offset,
			reg->size, reg->data);

	retval = rmi4_data->i2c_read(rmi4_data,
			regs->base_addr + reg->offset,
			reg->data, reg->size);
	if (retval < 0)
		return retval;

	for (s = 0, offset = 0; s < reg->nr_subpkts; ++s) {
		struct synaptics_rmi4_subpkt *subpkt = reg->subpkt + s;
		if (!subpkt->present)
			continue;

		if ((reg->size - offset) < subpkt->size) {
			pr_err("subpkt size error: expected %d bytes,"
				" only %d present\n", subpkt->size,
				(reg->size - offset));
			break;
		}

		memcpy(subpkt->data, reg->data+offset, subpkt->size);
		offset += subpkt->size;
#if defined(CONFIG_DYNAMIC_DEBUG) || defined(DEBUG)
		{
			int kk;
			pr_debug("read r%d.s%d =\n", reg->r_number, s);
			for (kk = 0; kk < subpkt->size; ++kk)
				pr_debug("%02x\n",
					((unsigned char *)subpkt->data)[kk]);
		}
#endif
	}
	return retval;
}

int synaptics_rmi4_read_packet_regs(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs)
{
	int r;
	int retval = 0;

	for (r = 0; r < regs->nr_regs; ++r) {
		pr_debug("about to read F%02x register %d [%d/%d]\n",
				regs->f_number,
				regs->regs[r].r_number,
				regs->regs[r].expected,
				regs->regs[r].offset >= 0);
		if (regs->regs[r].expected && regs->regs[r].offset >= 0) {
			retval = synaptics_rmi4_read_packet_reg(
				rmi4_data, regs, regs->regs[r].r_number);
			if (retval < 0)
				break;
		}
	}
	return retval;
}

static int synaptics_rmi4_write_packet_reg(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs, int r_number)
{
	struct synaptics_rmi4_packet_reg *reg = find_packet_reg(regs, r_number);
	int sz, ii, offset, retval;

	if (!reg || !reg->size) {
		pr_err("r%d does not exist or zero size\n", r_number);
		return -EINVAL;
	}
	if (reg->offset == -1) {
		pr_err("touch register error: writing to absent register r%d\n",
			r_number);
		return -ENOENT;
	}

	for (ii = 0, sz = 0, offset = 0; ii < reg->nr_subpkts; ++ii) {
		struct synaptics_rmi4_subpkt *subpkt = reg->subpkt + ii;
		if (!subpkt->present)
			continue;

		if (subpkt->data && subpkt->size &&
			(offset + subpkt->size) <= reg->size) {
			if ((reg->size - offset) >= subpkt->size) {
				memcpy(reg->data + sz, subpkt->data,
							subpkt->size);
				sz += subpkt->size;
			} else {
				pr_err("expected %d bytes, only %d present\n",
					offset + subpkt->size, reg->size);
				break;
			}
		} else {
			retval = -EINVAL;
			pr_err("cannot update subpacket %d\n", ii);
			goto out;
		}
	}

	pr_debug("writing r%d@%x, size %d to @%p\n", reg->r_number,
			regs->base_addr + reg->offset, reg->size, reg->data);

	retval = rmi4_data->i2c_write(rmi4_data,
			regs->base_addr + reg->offset,
			reg->data, sz);
out:
	return retval;
}

#ifdef CONFIG_OF
static int synaptics_dsx_gpio_config(
		struct synaptics_dsx_platform_data *pdata, bool enable)
{
	int retval = 0;

	if (enable) {
		if (!gpio_is_valid(pdata->irq_gpio)) {
			pr_err("invalid %s\n", IRQ_GPIO_NAME);
			retval = -EINVAL;
		}
		retval = gpio_request(pdata->irq_gpio, IRQ_GPIO_NAME);
		if (retval) {
			pr_err("unable to request %s [%d]: rc=%d\n",
				IRQ_GPIO_NAME, pdata->irq_gpio, retval);
			goto err_gpio;
		}
		retval = gpio_direction_input(pdata->irq_gpio);
		if (retval) {
			pr_err("unable to set %s [%d] dir: rc=%d\n",
				IRQ_GPIO_NAME, pdata->irq_gpio, retval);
			goto err_gpio;
		}

		if (!gpio_is_valid(pdata->reset_gpio)) {
			pr_err("invalid %s\n", RESET_GPIO_NAME);
			retval = -EINVAL;
		}
		retval = gpio_request(pdata->reset_gpio, RESET_GPIO_NAME);
		if (retval) {
			pr_err("unable to request %s [%d]: rc=%d\n",
				RESET_GPIO_NAME, pdata->reset_gpio, retval);
			goto err_gpio;
		}
		retval = gpio_direction_output(pdata->reset_gpio, 1);
		if (retval) {
			pr_err("unable to set %s [%d] dir: rc=%d\n",
				RESET_GPIO_NAME, pdata->reset_gpio, retval);
			goto err_gpio;
		}
	} else {
		gpio_free(pdata->irq_gpio);
		gpio_free(pdata->reset_gpio);
	}

err_gpio:
	return retval;
}

int synaptics_dsx_dt_parse_state(struct synaptics_rmi4_data *data,
		struct device_node *np_config,
		struct synaptics_dsx_patch *state)
{
	const char *patch_data;
	struct device_node *np_state;
	int err;

	np_state = of_node_get(np_config);
	err = of_property_read_string(np_config, "patch-data",
				(const char **)&patch_data);
	if (err < 0) {
		pr_err("unable to read patch-data\n");
		return err;
	}

	synaptics_dsx_parse_string(data, patch_data, state);

	return 0;
}

int synaptics_dsx_dt_parse_mode(struct synaptics_rmi4_data *data,
		const char *mode_name, struct synaptics_dsx_patchset *mode)
{
	struct device *dev = &data->i2c_client->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_modes;
	int ret = 0;
	char *propname;
	struct property *prop;
	const __be32 *list;
	int size, config;
	phandle phandle;
	struct device_node *np_config;

	np_modes = of_find_node_by_name(np, "touchstate_modes");
	if (!np_modes) {
		pr_warn("can't find touchstate modes node\n");
		ret = -EINVAL;
		goto err;
	}

	pr_debug("processing mode %s\n", mode_name);
	propname = kasprintf(GFP_KERNEL, "touchmode-%s", mode_name);
	prop = of_find_property(np_modes, propname, &size);
	kfree(propname);
	of_node_put(np_modes);
	if (!prop) {
		pr_err("can't find mode %s\n", mode_name);
		ret = -EINVAL;
		goto err;
	}
	list = prop->value;
	size /= sizeof(*list);

	if (size > MAX_NUM_STATES) {
		pr_err("unexpected number of states %d\n", size);
		ret = -EINVAL;
		goto err;
	}

	for (config = 0; config < size; config++) {
		phandle = be32_to_cpup(list++);

		/* Look up the touchstate configuration node */
		np_config = of_find_node_by_phandle(phandle);
		if (!np_config) {
			dev_err(dev,
				"prop %s index %i invalid phandle\n",
				prop->name, config);
			ret = -EINVAL;
			goto err;
		}

		/* Parse the node */
		ret = synaptics_dsx_dt_parse_state(data, np_config,
				 &mode->patch_data[config]);
		of_node_put(np_config);
		if (ret < 0)
			goto err;
	}
err:
	return ret;
}

static struct synaptics_dsx_platform_data *
		synaptics_dsx_of_init(struct i2c_client *client,
				struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned key_codes[SYN_MAX_BUTTONS];
	struct synaptics_dsx_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	struct synaptics_dsx_cap_button_map *button_map = NULL;

	pr_err("entered of_init function\n");

	rmi4_data->patching_enabled = 1;
	retval = synaptics_dsx_dt_parse_mode(rmi4_data, "default",
			rmi4_data->default_mode);
	if (retval) {
		pr_warn("failed to load default mode\n");
		rmi4_data->patching_enabled = 0;
	}

	if (rmi4_data->patching_enabled) {
		retval = synaptics_dsx_dt_parse_mode(rmi4_data, "alternate",
				rmi4_data->alternate_mode);
		if (retval) {
			pr_warn("alternate mode not found;"
				" using default instead\n");
			rmi4_data->alternate_mode = rmi4_data->default_mode;
		}
	}

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	pdata->irq_gpio = of_get_gpio(np, 0);
	pdata->reset_gpio = of_get_gpio(np, 1);

	memset(key_codes, 0, sizeof(key_codes));
	retval = of_property_read_u32_array(np, "synaptics,key-buttons",
			 key_codes, SYN_MAX_BUTTONS);
	if (!retval) {
		int ii;
		unsigned char *button_codes;

		button_map = kzalloc(sizeof(*button_map), GFP_KERNEL);
		if (IS_ERR_OR_NULL(button_map)) {
			dev_err(&client->dev, "button allocation failure\n");
			return NULL;
		}

		for (ii = 0; ii < SYN_MAX_BUTTONS; ii++)
			if (key_codes[ii])
				button_map->nbuttons++;

		button_codes = kzalloc(button_map->nbuttons, GFP_KERNEL);
		if (IS_ERR_OR_NULL(button_codes)) {
			dev_err(&client->dev, "button allocation failure\n");
			kfree(button_map);
			return NULL;
		}

		for (ii = 0; ii < button_map->nbuttons; ii++)
			*(button_codes + ii) = (unsigned char)key_codes[ii];

		button_map->map = button_codes;
	}

	pdata->irq_flags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	pdata->cap_button_map = button_map;

	if (of_property_read_bool(np, "synaptics,gpio-config")) {
		pr_notice("using gpio config\n");
		pdata->gpio_config = synaptics_dsx_gpio_config;
	}

	if (of_property_read_bool(np, "synaptics,x-flip")) {
		pr_notice("using flipped X axis\n");
		pdata->x_flip = true;
	}

	if (of_property_read_bool(np, "synaptics,y-flip")) {
		pr_notice("using flipped Y axis\n");
		pdata->y_flip = true;
	}

	if (of_property_read_bool(np, "synaptics,purge-enabled")) {
		pr_notice("using purge\n");
		rmi4_data->purge_enabled = true;
	}

	return pdata;
}
#else
static inline struct synaptics_dsx_platform_data *
		synaptics_dsx_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static void synaptics_dsx_validate_product_string(unsigned char *id)
{
	unsigned char *s;
	for (s = id; *s; s++) {
		if (*s == '-') {
			*s = 0;
			break;
		}
		if (!isdigit(*s) && isupper(*s))
			*s = tolower(*s);
	}
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data,
		unsigned char *f01_cmd_base_addr);

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable);

static void synaptics_dsx_sensor_state(struct synaptics_rmi4_data *rmi4_data,
		int state);

#if defined(CONFIG_FB) && !defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
static int synaptics_dsx_panel_cb(struct notifier_block *nb,
		unsigned long event, void *data);
#endif

static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_resume_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_irqtimes_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_poweron_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_reporting_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_reporting_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f01_query1 {
	union {
		struct {
			unsigned char has_query42:1;
			unsigned char has_doze_holdoff:1;
			unsigned char has_adjustable_doze:1;
			unsigned char has_charger_input:1;
			unsigned char has_sensor_id:1;
			unsigned char reserved:1;
			unsigned char non_compliant:1;
			unsigned char custom_map:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f01_query42 {
	union {
		struct {
			unsigned char reserved:2;
			unsigned char has_recalibration:1;
			unsigned char has_report_rate:1;
			unsigned char has_swr:1;
			unsigned char has_guest:1;
			unsigned char has_multi_physical:1;
			unsigned char has_ds4_queries:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char reserved:5;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control_3_4 {
	unsigned char transmitterbutton;
	unsigned char receiverbutton;
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	unsigned char *button_int_enable;
	unsigned char *multi_button;
	struct synaptics_rmi4_f1a_control_3_4 *electrode_map;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char button_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

struct synaptics_rmi4_exp_fn {
	enum exp_fn fn_type;
	enum ic_modes mode;
	bool inserted;
	int (*func_init)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_remove)(struct synaptics_rmi4_data *rmi4_data);
	void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask);
	struct list_head link;
};

static struct device_attribute attrs[] = {
	__ATTR(reset, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			synaptics_rmi4_f01_reset_store),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUSR | S_IRGRP,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(0dbutton, (S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP),
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
	__ATTR(resumeinfo, S_IRUSR | S_IRGRP,
			synaptics_rmi4_resume_show,
			synaptics_rmi4_store_error),
	__ATTR(drv_irq, (S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP),
			synaptics_rmi4_drv_irq_show,
			synaptics_rmi4_drv_irq_store),
	__ATTR(hw_irqstat, S_IRUSR | S_IRGRP,
			synaptics_rmi4_hw_irqstat_show,
			synaptics_rmi4_store_error),
	__ATTR(ic_ver, S_IRUGO,
			synaptics_rmi4_ic_ver_show,
			synaptics_rmi4_store_error),
	__ATTR(irqinfo, S_IRUSR | S_IRGRP,
			synaptics_rmi4_irqtimes_show,
			synaptics_rmi4_store_error),
	__ATTR(poweron, S_IRUSR | S_IRGRP,
			synaptics_rmi4_poweron_show,
			synaptics_rmi4_store_error),
	__ATTR(reporting, (S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP),
			synaptics_rmi4_reporting_show,
			synaptics_rmi4_reporting_store),
};

struct synaptics_exp_fn_ctrl {
	bool inited;
	struct mutex list_mutex;
	struct list_head fn_list;
	struct delayed_work det_work;
	struct workqueue_struct *det_workqueue;
	struct synaptics_rmi4_data *rmi4_data_ptr;
};

DEFINE_MUTEX(exp_fn_ctrl_mutex);
static struct synaptics_exp_fn_ctrl exp_fn_ctrl;
static struct semaphore reset_semaphore;

static irqreturn_t synaptics_dsx_reset_irq(int irq, void *data)
{
	struct semaphore *sema = data;
	up(sema);
	return IRQ_HANDLED;
}

int synaptics_rmi4_sw_reset(struct synaptics_rmi4_data *rmi4_data,
				bool requery) {
	int retval;
	unsigned char page_number;
	unsigned short pdt_entry_addr;
	struct synaptics_rmi4_fn_desc rmi_fd;
	unsigned short cmd_reg_addr;
	bool cmd_reg_found = false;

	if (!requery) {
		cmd_reg_addr = rmi4_data->f01_cmd_base_addr;
		cmd_reg_found = true;
		goto sw_reset;
	}

	for (page_number = 0; !cmd_reg_found && page_number < PAGES_TO_SERVICE;
			page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(&rmi4_data->i2c_client->dev,
						"%s: Reached end of PDT\n",
						__func__);
				break;
			}

			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: F%02x found (page %d)\n",
					__func__, rmi_fd.fn_number,
					page_number);

			if (rmi_fd.fn_number == SYNAPTICS_RMI4_F01) {
				cmd_reg_addr = rmi_fd.cmd_base_addr;
				cmd_reg_found = true;
				break;
			}
		}
	}

sw_reset:
	if (cmd_reg_found) {
		uint8_t command = 1;
		retval = synaptics_rmi4_i2c_write(rmi4_data,
			cmd_reg_addr,
			&command, sizeof(command));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to issue SW reset command, error = %d\n",
				__func__, retval);
			return retval;
		}
		return 0;
	} else
		return -ENOENT;

}


static int synaptics_dsx_ic_reset(
		struct synaptics_rmi4_data *rmi4_data, int reset)
{
	int retval;
	unsigned long start = jiffies;
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	sema_init(&reset_semaphore, 0);

	if (reset == RMI4_HW_RESET) {
		gpio_set_value(platform_data->reset_gpio, 0);
		udelay(1500);
	} else if (reset == RMI4_SW_RESET) {
		retval = synaptics_rmi4_sw_reset(rmi4_data, true);
		if (retval < 0)
			return retval;
	}

	retval = request_irq(rmi4_data->irq, synaptics_dsx_reset_irq,
			IRQF_TRIGGER_RISING, "synaptics_reset",
			&reset_semaphore);
	if (retval < 0)
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to request irq: %d\n",
				__func__, retval);

	if (reset == RMI4_HW_RESET)
		gpio_set_value(platform_data->reset_gpio, 1);

	retval = down_timeout(&reset_semaphore, msecs_to_jiffies(100));
	if (retval) {
		dev_err(&rmi4_data->i2c_client->dev,
				"timed out waiting for reset to complete\n");
		retval = -ETIMEDOUT;
	} else {
		retval = (int)jiffies_to_msecs(jiffies-start);
		/* insert delay to ensure 1st i2c bus access succeeds */
		udelay(1000);
	}

	free_irq(rmi4_data->irq, &reset_semaphore);

	/* perform SW reset if HW reset failed */
	if (reset != RMI4_SW_RESET && retval == -ETIMEDOUT)
		retval = synaptics_dsx_ic_reset(rmi4_data, RMI4_SW_RESET);

	return retval;
}

static int synaptics_dsx_alloc_input(struct synaptics_rmi4_data *rmi4_data)
{
	rmi4_data->input_dev = input_allocate_device();
	if (IS_ERR_OR_NULL(rmi4_data->input_dev))
		return PTR_ERR(rmi4_data->input_dev);

	rmi4_data->input_dev->name = DRIVER_NAME;
	rmi4_data->input_dev->phys = INPUT_PHYS_NAME;
	rmi4_data->input_dev->id.bustype = BUS_I2C;
	rmi4_data->input_dev->dev.parent = &rmi4_data->i2c_client->dev;

	set_bit(EV_SYN, rmi4_data->input_dev->evbit);

	pr_debug("allocated input device\n");

	return 0;
}

static void synaptics_dsx_patch_func(
		struct synaptics_rmi4_data *rmi4_data,
		int f_number,
		struct synaptics_dsx_patch *patch)
{
	int error;
	unsigned char *value;
	struct device *dev = &rmi4_data->i2c_client->dev;
	struct synaptics_rmi4_subpkt *subpkt;
	struct synaptics_dsx_func_patch *fp;
	struct synaptics_rmi4_packet_reg *reg;
	struct synaptics_rmi4_func_packet_regs *regs = find_function(f_number);

	pr_debug("patching F%x\n", regs->f_number);
	list_for_each_entry(fp, &patch->cfg_head, link) {
		if (fp->func != f_number)
			continue;
		reg = find_packet_reg(regs, fp->regstr);
		if (!reg || reg->offset < 0) {
			pr_err("F%x@%d not present\n", f_number, fp->regstr);
			continue;
		}
		if (fp->subpkt >= reg->nr_subpkts) {
			pr_err("F%x@%d:%d not present\n",
				f_number, fp->regstr, fp->subpkt);
			continue;
		}

		error =	synaptics_rmi4_read_packet_reg(rmi4_data, regs,
			reg->r_number);
		if (error < 0)
			return;
#if defined(CONFIG_DYNAMIC_DEBUG) || defined(DEBUG)
		{
			int ss, kk;
			pr_debug("F%x@%d before patching:\n",
					regs->f_number, reg->r_number);
			for (ss = 0; ss < reg->nr_subpkts; ss++) {
				subpkt = reg->subpkt + ss;
				for (kk = 0; kk < subpkt->size; ++kk)
					pr_debug("%02x\n",
					((unsigned char *)subpkt->data)[kk]);
			}
		}
#endif
		subpkt = reg->subpkt + fp->subpkt;
		if (!subpkt->present || !subpkt->data ||
			subpkt->size < fp->size) {
			pr_debug("F%x@%d:%d improperly allocated\n",
				f_number, fp->regstr, fp->subpkt);
			continue;
		}
		if (fp->bitmask && fp->size == 1) {
			value = (unsigned char *)subpkt->data;
			pr_debug("patching by mask: source 0x%x\n", *value);
			*value &= ~(fp->bitmask);
			pr_debug("patching by mask: clear-ed 0x%x\n", *value);
			*value |= *fp->data;
			pr_debug("patching by mask: result 0x%x\n", *value);
		} else
			memcpy(subpkt->data, fp->data, fp->size);

		dev_dbg(dev, "patched F%x@%d:%d, sz=%d\n",
			f_number, fp->regstr, fp->subpkt, fp->size);
#if defined(CONFIG_DYNAMIC_DEBUG) || defined(DEBUG)
		{
			int ss, kk;
			pr_debug("F%x@%d after patching:\n",
					regs->f_number, reg->r_number);
			for (ss = 0; ss < reg->nr_subpkts; ss++) {
				subpkt = reg->subpkt + ss;
				for (kk = 0; kk < subpkt->size; ++kk)
					pr_debug("%02x\n",
					((unsigned char *)subpkt->data)[kk]);
			}
		}
#endif
		error = synaptics_rmi4_write_packet_reg(
					rmi4_data, regs, reg->r_number);
		if (error < 0)
			dev_warn(dev, "F%x@%d patch write failed\n",
					f_number, reg->r_number);
	}
}

static void synaptics_dsx_enable_wakeup_source(
		struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	int error;
	error = irq_set_irq_wake(rmi4_data->irq, (int)enable);
	pr_debug("%s wakeup; rc=%d\n", enable ? "enabled" : "disabled", error);
}

static inline void synaptics_dsx_set_alternate_mode(
		struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_dsx_patchset *mode,
		bool wakeable, bool persistent)
{
	rmi4_data->mode_is_wakeable = wakeable;
	rmi4_data->mode_is_persistent = persistent;
	rmi4_data->current_mode = mode;
	if (wakeable)
		synaptics_dsx_enable_wakeup_source(rmi4_data, true);
}

static inline void synaptics_dsx_restore_default_mode(
		struct synaptics_rmi4_data *rmi4_data)
{
	if (rmi4_data->mode_is_wakeable)
		synaptics_dsx_enable_wakeup_source(rmi4_data, false);
	rmi4_data->mode_is_wakeable = false;
	rmi4_data->mode_is_persistent = true;
	rmi4_data->current_mode = rmi4_data->default_mode;
}

static void synaptics_dsx_state_config(
		struct synaptics_rmi4_data *rmi4_data, int state)
{
	int i;
	struct synaptics_dsx_patch *patch =
			&rmi4_data->current_mode->patch_data[state];

	if (!patch || !patch->cfg_num) {
		pr_debug("patchset is empty!\n");
		return;
	}

	if (rmi4_data->mode_is_wakeable)
		synaptics_dsx_enable_wakeup_source(rmi4_data, true);

	if (rmi4_data->patching_enabled) {
		for (i = 0; i < ARRAY_SIZE(synaptics_cfg_regs); i++)
			synaptics_dsx_patch_func(rmi4_data,
				synaptics_cfg_regs[i].f_number, patch);

		pr_debug("applied %s in mode %s\n",
			state == ACTIVE_IDX ? "ACTIVE" : "SUSPEND",
			rmi4_data->current_mode == rmi4_data->default_mode ?
			"DEFAULT" : "OTHER");
	}
}

#define DSX(a)	(#a)
static const char * const synaptics_state_names[] = SYNAPTICS_DSX_STATES;
#undef DSX

static const char *synaptics_dsx_state_name(int state)
{
	int index = state < 0 || state > STATE_INVALID ? STATE_INVALID : state;
	return synaptics_state_names[index];
}

static int synaptics_dsx_get_state_safe(struct synaptics_rmi4_data *rmi4_data)
{
	int state;
	mutex_lock(&(rmi4_data->state_mutex));
	state = rmi4_data->state;
	mutex_unlock(&(rmi4_data->state_mutex));
	return state;
}

static void synaptics_dsx_set_state_safe(struct synaptics_rmi4_data *rmi4_data,
		int state)
{
	mutex_lock(&(rmi4_data->state_mutex));
	rmi4_data->state = state;
	mutex_unlock(&(rmi4_data->state_mutex));
}

static int synaptics_dsx_wait_for_idle(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned long start_wait_jiffies = jiffies;

	do {
		int current_state;

		current_state = synaptics_dsx_get_state_safe(rmi4_data);
		if (!(current_state == STATE_INIT ||
			current_state == STATE_FLASH ||
			current_state == STATE_UNKNOWN))
			break;

		usleep_range(1000, 1000);

	} while (1);

	if ((jiffies - start_wait_jiffies))
		pr_info("entering suspend delayed for %ums\n",
			jiffies_to_msecs(jiffies - start_wait_jiffies));
	return 0;
}

static int synaptics_dsx_sensor_ready_state(
		struct synaptics_rmi4_data *rmi4_data, bool standby)
{
	bool ui_mode;
	int retval, state;
	struct synaptics_rmi4_f01_device_status status;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
	if (retval < 0) {
		pr_err("failed to query touch ic status\n");
		return retval;
	}

	state = synaptics_dsx_get_state_safe(rmi4_data);

	ui_mode = status.flash_prog == 0;
	pr_debug("UI mode: %s\n", ui_mode ? "true" : "false");

	if (ui_mode) {
		state = standby ? STATE_STANDBY : STATE_ACTIVE;
	} else
		if (!(state == STATE_INIT || state == STATE_FLASH))
			state = STATE_BL;

	synaptics_dsx_sensor_state(rmi4_data, state);

	return 0;
}

static void synaptics_dsx_sensor_state(struct synaptics_rmi4_data *rmi4_data,
		int state)
{
	if (synaptics_dsx_get_state_safe(rmi4_data) == state)
		return;

	switch (state) {
	case STATE_UNKNOWN:
	case STATE_FLASH:
		/* no special handling for these states */
			break;

	case STATE_SUSPEND:
		synaptics_dsx_wait_for_idle(rmi4_data);
		if (!rmi4_data->mode_is_wakeable)
			synaptics_rmi4_irq_enable(rmi4_data, false);
		if (!rmi4_data->in_bootloader)
			synaptics_dsx_state_config(rmi4_data, SUSPEND_IDX);
			break;

	case STATE_ACTIVE:
		if (!rmi4_data->in_bootloader)
			synaptics_dsx_state_config(rmi4_data, ACTIVE_IDX);

		synaptics_rmi4_irq_enable(rmi4_data, true);

		if (!rmi4_data->mode_is_persistent) {
			synaptics_dsx_restore_default_mode(rmi4_data);
			pr_debug("Non-persistent mode; restoring default\n");
		}
			break;

	case STATE_STANDBY:
		synaptics_rmi4_irq_enable(rmi4_data, false);
			break;

	case STATE_BL:
		if (!rmi4_data->in_bootloader)
			rmi4_data->in_bootloader = true;
	case STATE_INIT:
		/* de-allocate input device earlier to allow */
		/* EventHub become notified of input removal */
		if (rmi4_data->input_registered) {
			input_unregister_device(rmi4_data->input_dev);
			rmi4_data->input_dev = NULL;
			rmi4_data->input_registered = false;

			pr_debug("de-allocated input device\n");
		}

		synaptics_rmi4_irq_enable(rmi4_data, false);
			break;
	}

	pr_info("state change %s -> %s\n",
			synaptics_dsx_state_name(rmi4_data->state),
			synaptics_dsx_state_name(state));

	synaptics_dsx_set_state_safe(rmi4_data, state);
}

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_dsx_ic_reset(rmi4_data, RMI4_HW_RESET);
	if (retval > 0)
		pr_debug("successful reset took %dms\n", retval);
	else
		dev_warn(&rmi4_data->i2c_client->dev, "%s: timed out waiting for idle\n",
			__func__);

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			rmi4_data->rmi4_mod_info.product_id_string);
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int firmware_id;
	unsigned int config_id;
	struct synaptics_rmi4_device_info *rmi;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	rmi = &(rmi4_data->rmi4_mod_info);

	batohui(&firmware_id, rmi->build_id, sizeof(rmi->build_id));
	batohui(&config_id, rmi->config_id, sizeof(rmi->config_id));

	return scnprintf(buf, PAGE_SIZE, "%x-%08x\n", firmware_id, config_id);
}

static ssize_t synaptics_rmi4_resume_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	int c_res;
	int offset = 0;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	c_res = rmi4_data->last_resume;
	/* Resume buffer not allocated or there were no resumes yet */
	if (rmi4_data->number_resumes <= 0 || c_res < 0)
		return scnprintf(buf, PAGE_SIZE,
				"No resume information found.\n");

	offset += scnprintf(buf + offset, PAGE_SIZE - offset,
		"Count\tStart\t\tFinish\t# ignored\t"
		"ISR\t\tpurge off\tsendevent\n");

	for (i = 0; i < rmi4_data->number_resumes; i++) {
		offset += scnprintf(buf + offset, PAGE_SIZE - offset,
			"%d\t%4ld.%03ld\t%4ld.%03ld\t%d\t%4ld.%03ld\t"
			"%4ld.%03ld\t%4ld.%03ld\n",
		i+1,
		rmi4_data->resume_info[c_res].start.tv_sec%1000,
		rmi4_data->resume_info[c_res].start.tv_nsec/1000000,
		rmi4_data->resume_info[c_res].finish.tv_sec%1000,
		rmi4_data->resume_info[c_res].finish.tv_nsec/1000000,
		rmi4_data->resume_info[c_res].ignored_events,
		rmi4_data->resume_info[c_res].isr.tv_sec%1000,
		rmi4_data->resume_info[c_res].isr.tv_nsec/1000000,
		rmi4_data->resume_info[c_res].purge_off.tv_sec%1000,
		rmi4_data->resume_info[c_res].purge_off.tv_nsec/1000000,
		rmi4_data->resume_info[c_res].send_touch.tv_sec%1000,
		rmi4_data->resume_info[c_res].send_touch.tv_nsec/1000000);

		if (c_res <= 0)
			c_res = rmi4_data->number_resumes-1;
		else
			c_res--;
	}
	return offset;
}

static ssize_t synaptics_rmi4_irqtimes_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	int c_res;
	int offset = 0;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	c_res = rmi4_data->last_irq;
	/* Resume buffer not allocated or there were no irq data collected yet*/
	if (rmi4_data->number_irq <= 0 || c_res < 0)
		return scnprintf(buf, PAGE_SIZE,
				"No resume information found.\n");

	offset += scnprintf(buf + offset, PAGE_SIZE - offset,
		"Count\tIRQ Start\n");

	for (i = 0; i < rmi4_data->number_irq; i++) {
		offset += scnprintf(buf + offset, PAGE_SIZE - offset,
			"%d\t%4ld.%03ld\n",
		i+1,
		rmi4_data->irq_info[i].irq_time.tv_sec%1000,
		rmi4_data->irq_info[i].irq_time.tv_nsec/1000000);

		if (c_res <= 0)
			c_res = rmi4_data->number_irq-1;
		else
			c_res--;
	}
	return offset;
}

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			device_status.flash_prog);
}

static ssize_t synaptics_rmi4_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	switch (gpio_get_value(rmi4_data->board->irq_gpio)) {
	case 0:
		return scnprintf(buf, PAGE_SIZE, "Low\n");
	case 1:
		return scnprintf(buf, PAGE_SIZE, "High\n");
	default:
		printk(KERN_ERR "%s: Failed to get GPIO for irq %d.\n",
				__func__,
				rmi4_data->irq);
		return scnprintf(buf, PAGE_SIZE, "Unknown\n");
	}
}

static bool reporting_stopped;

static ssize_t synaptics_rmi4_reporting_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			reporting_stopped ? "STOPPED" : "RUNNING");
}

static ssize_t synaptics_rmi4_reporting_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value = 0;
	int err = 0;

	if (*buf == 's' || *buf == 'S') {
		reporting_stopped = true;
	} else if (*buf == 'r' || *buf == 'R') {
		reporting_stopped = false;
	} else {
		err = kstrtoul(buf, 10, &value);
		if (err < 0) {
			pr_err("Failed to convert value\n");
			return -EINVAL;
		}
		reporting_stopped = value == 0;
	}
	return count;
}

static ssize_t synaptics_rmi4_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			rmi4_data->irq_enabled ? "ENABLED" : "DISABLED");
}

static ssize_t synaptics_rmi4_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long value = 0;
	int err = 0;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		return -EINVAL;
	}

	switch (value) {
	case 0:
		/* Disable irq */
		synaptics_rmi4_irq_enable(rmi4_data, false);
		break;
	case 1:
		/* Enable irq */
		synaptics_rmi4_irq_enable(rmi4_data, true);
		break;
	default:
		printk(KERN_ERR "%s: Invalid value\n", __func__);
		return -EINVAL;
	}
	return count;
}

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	return scnprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->button_0d_enabled);
}

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	unsigned char ii;
	unsigned char intr_enable;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	rmi = &(rmi4_data->rmi4_mod_info);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	if (rmi4_data->button_0d_enabled == input)
		return count;

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
			ii = fhandler->intr_reg_num;

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;

			if (input == 1)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_i2c_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;
		}
	}

	rmi4_data->button_0d_enabled = input;

	return count;
}

static ssize_t synaptics_rmi4_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int build_id, config_id;
	struct synaptics_rmi4_device_info *rmi;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	rmi = &(rmi4_data->rmi4_mod_info);
	batohui(&build_id, rmi->build_id, sizeof(rmi->build_id));
	batohui(&config_id, rmi->config_id, sizeof(rmi->config_id));
	return scnprintf(buf, PAGE_SIZE,
			"%s%s\n%s%x\n%s%x\n",
			"Product ID: ", rmi->product_id_string,
			"Build ID: ", build_id,
			"Config ID: ", config_id);
}

static ssize_t synaptics_rmi4_poweron_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	return scnprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&rmi4_data->touch_stopped) == 0 &&
		rmi4_data->flash_enabled);
}

 /**
 * synaptics_rmi4_set_page()
 *
 * Called by synaptics_rmi4_i2c_read() and synaptics_rmi4_i2c_write().
 *
 * This function writes to the page select register to switch to the
 * assigned page.
 */
static int synaptics_rmi4_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned int address)
{
	int retval = 0;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = rmi4_data->i2c_client;

	page = ((address >> 8) & MASK_8BIT);
	if (page != rmi4_data->current_page) {
		buf[0] = MASK_8BIT;
		buf[1] = page;
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			retval = i2c_master_send(i2c, buf, PAGE_SELECT_LEN);
			if (retval != PAGE_SELECT_LEN) {
				dev_err(&i2c->dev,
						"%s: I2C retry %d\n",
						__func__, retry + 1);
				msleep(20);
			} else {
				rmi4_data->current_page = page;
				break;
			}
		}
	} else
		return PAGE_SELECT_LEN;
	return (retval == PAGE_SELECT_LEN) ? retval : -EIO;
}

 /**
 * synaptics_rmi4_i2c_read()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function reads data of an arbitrary length from the sensor,
 * starting from an assigned register address of the sensor, via I2C
 * with a retry mechanism.
 */
static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf;
	struct i2c_msg msg[] = {
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = 0,
			.len = 1,
			.buf = &buf,
		},
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		},
	};

	buf = addr & MASK_8BIT;

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	retval = synaptics_rmi4_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 2) == 2) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C read %db@x%x over retry limit\n",
				__func__, length, addr);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}

 /**
 * synaptics_rmi4_i2c_write()
 *
 * Called by various functions in this driver, and also exported to
 * other expansion Function modules such as rmi_dev.
 *
 * This function writes data of an arbitrary length to the sensor,
 * starting from an assigned register address of the sensor, via I2C with
 * a retry mechanism.
 */
static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf[length + 1];
	struct i2c_msg msg[] = {
		{
			.addr = rmi4_data->i2c_client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	mutex_lock(&(rmi4_data->rmi4_io_ctrl_mutex));

	retval = synaptics_rmi4_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN)
		goto exit;

	buf[0] = addr & MASK_8BIT;
	memcpy(&buf[1], &data[0], length);

	for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(rmi4_data->i2c_client->adapter, msg, 1) == 1) {
			retval = length;
			break;
		}
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C retry %d\n",
				__func__, retry + 1);
		msleep(20);
	}

	if (retry == SYN_I2C_RETRY_TIMES) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: I2C write %db@x%x over retry limit\n",
				__func__, length, addr);
		retval = -EIO;
	}

exit:
	mutex_unlock(&(rmi4_data->rmi4_io_ctrl_mutex));

	return retval;
}

 /**
 * synaptics_rmi4_f12_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $12
 * finger data has been detected.
 *
 * This function reads the Function $12 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char index = 0;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char finger_status;
	unsigned char finger_data[F12_STD_DATA_LEN];
	unsigned short data_addr;
	unsigned short data_size;
	int x;
	int y;
	int p;
	int w;
	int id;
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	unsigned char number_of_fingers_actually_touching = 0;
#endif

	fingers_supported = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	data_size = fingers_supported * fhandler->size_of_data_register_block;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			finger_data,
			data_size);
	if (retval < 0 || reporting_stopped)
		return 0;

	if (atomic_read(&rmi4_data->panel_off_flag)) {
		synaptics_dsx_resumeinfo_ignore(rmi4_data);
		return 0;
	} else
		synaptics_dsx_resumeinfo_purgeoff(rmi4_data);

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (touchxp.touchx)
		mutex_lock(&touchxp.virtual_touch_mutex);

	for (finger = 0; finger < fingers_supported; finger++,
			 index += fhandler->size_of_data_register_block) {
		if (finger_data[index] == 0)
			continue;
		number_of_fingers_actually_touching++;
	}
	index = 0;
#endif

	for (finger = 0; finger < fingers_supported; finger++,
			 index += fhandler->size_of_data_register_block) {
		finger_status = finger_data[index];
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, finger_status);
#endif
		if (finger_status) {
			x = finger_data[index+1] | (finger_data[index+2] << 8);
			y = finger_data[index+3] | (finger_data[index+4] << 8);
			p = finger_data[index+5];
			w = finger_data[index+5];
			id = finger;

			if (rmi4_data->board->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->board->y_flip)
				y = rmi4_data->sensor_max_y - y;

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
			touchxp.touch_magic_dev = rmi4_data->input_dev;
			if (touchxp.touchx)
				touchxp.touchx(&x, &y, finger,
					       number_of_fingers_actually_touching);
#endif

			dev_dbg(&rmi4_data->i2c_client->dev,
						"%s: Finger %d:\n"
						"x = %d\n"
						"y = %d\n"
						"p = %d\n"
						"w = %d\n",
						__func__, finger,
						x, y, p, w);

			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_PRESSURE, p);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, w);
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif
			touch_count++;
#ifdef TYPE_B_PROTOCOL
		} else {
			/* Touch no longer active, close out slot */
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
#endif
		}

		synaptics_dsx_resumeinfo_touch(rmi4_data);
	}


#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (!touch_count)
		touchxp.finger_down = 0;
#endif

#ifndef TYPE_B_PROTOCOL
	if (!touch_count)
		input_mt_sync(rmi4_data->input_dev);
#endif

	input_mt_report_pointer_emulation(rmi4_data->input_dev, false);
	input_sync(rmi4_data->input_dev);

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (touchxp.touchx)
		mutex_unlock(&touchxp.virtual_touch_mutex);
#endif

	return touch_count;
}

 /**
 * synaptics_rmi4_f11_abs_report()
 *
 * Called by synaptics_rmi4_report_touch() when valid Function $11
 * finger data has been detected.
 *
 * This function reads the Function $11 data registers, determines the
 * status of each finger supported by the Function, processes any
 * necessary coordinate manipulation, reports the finger data to
 * the input subsystem, and returns the number of fingers detected.
 */
static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char data_reg_blk_size;
	unsigned char finger_status_reg[3];
	unsigned char data[F11_STD_DATA_LEN];
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
	int wx;
	int wy;
	int z;
#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	unsigned char number_of_fingers_actually_touching = 0;
#endif

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;
	data_reg_blk_size = fhandler->size_of_data_register_block;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0 || reporting_stopped)
		return 0;

	if (atomic_read(&rmi4_data->panel_off_flag)) {
		synaptics_dsx_resumeinfo_ignore(rmi4_data);
		return 0;
	} else
		synaptics_dsx_resumeinfo_purgeoff(rmi4_data);

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (touchxp.touchx)
		mutex_lock(&touchxp.virtual_touch_mutex);

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
				& MASK_2BIT;
		if (finger_status)
			number_of_fingers_actually_touching++;
	}
#endif

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
				& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, finger_status);
#endif
		if (finger_status) {
			data_offset = data_addr +
					num_of_finger_status_regs +
					(finger * data_reg_blk_size);
			retval = synaptics_rmi4_i2c_read(rmi4_data,
					data_offset,
					data,
					data_reg_blk_size);
			if (retval < 0)
				return 0;

			x = (data[0] << 4) | (data[2] & MASK_4BIT);
			y = (data[1] << 4) | ((data[2] >> 4) & MASK_4BIT);
			wx = (data[3] & MASK_4BIT);
			wy = (data[3] >> 4) & MASK_4BIT;
			z = data[4];

			if (rmi4_data->board->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->board->y_flip)
				y = rmi4_data->sensor_max_y - y;

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
			touchxp.touch_magic_dev = rmi4_data->input_dev;
			if (touchxp.touchx)
				touchxp.touchx(&x, &y, finger,
					number_of_fingers_actually_touching);
#endif

			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Finger %d:\n"
					"x = %d\n"
					"y = %d\n"
					"wx = %d\n"
					"wy = %d\n"
					"z = %d\n",
					__func__, finger,
					x, y, wx, wy, z);

			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_PRESSURE, z);

#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif
			touch_count++;
#ifdef TYPE_B_PROTOCOL
		} else {
			/* Touch no longer active, close out slot */
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
#endif
		}

		synaptics_dsx_resumeinfo_touch(rmi4_data);
	}

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (!touch_count)
		touchxp.finger_down = 0;
#endif

#ifndef TYPE_B_PROTOCOL
	if (!touch_count)
		input_mt_sync(rmi4_data->input_dev);
#endif

	input_mt_report_pointer_emulation(rmi4_data->input_dev, false);
	input_sync(rmi4_data->input_dev);

#ifdef CONFIG_TOUCHSCREEN_TOUCHX_BASE
	if (touchxp.touchx)
		mutex_unlock(&touchxp.virtual_touch_mutex);
#endif

	return touch_count;
}

static void synaptics_rmi4_f1a_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char button;
	unsigned char index;
	unsigned char shift;
	unsigned char status;
	unsigned char *data;
	unsigned short data_addr = fhandler->full_addr.data_base;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif

	if (tsb_buff_clean_flag) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		tsb_buff_clean_flag = 0;
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read button data registers\n",
				__func__);
		return;
	}

	if (atomic_read(&rmi4_data->panel_off_flag)) {
		synaptics_dsx_resumeinfo_ignore(rmi4_data);
		return;
	} else
		synaptics_dsx_resumeinfo_purgeoff(rmi4_data);

	data = f1a->button_data_buffer;

	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status)
			continue;
		else
			current_status[button] = status;

		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Button %d (code %d) ->%d\n",
				__func__, button,
				f1a->button_map[button],
				status);
#ifdef NO_0D_WHILE_2D
		if (rmi4_data->fingers_on_2d == false) {
			if (status == 1) {
				before_2d_status[button] = 1;
			} else {
				if (while_2d_status[button] == 1) {
					while_2d_status[button] = 0;
					continue;
				} else {
					before_2d_status[button] = 0;
				}
			}
			input_report_key(rmi4_data->input_dev,
					f1a->button_map[button],
					status);
		} else {
			if (before_2d_status[button] == 1) {
				before_2d_status[button] = 0;
				input_report_key(rmi4_data->input_dev,
						f1a->button_map[button],
						status);
			} else {
				if (status == 1)
					while_2d_status[button] = 1;
				else
					while_2d_status[button] = 0;
			}
		}
#else
		input_report_key(rmi4_data->input_dev,
				f1a->button_map[button],
				status);
#endif
		synaptics_dsx_resumeinfo_touch(rmi4_data);
	}

	input_sync(rmi4_data->input_dev);

	return;
}

static void synaptics_rmi4_f01_handler(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned short data_addr = fhandler->full_addr.data_base;
	struct synaptics_rmi4_f01_device_status status;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			status.data,
			sizeof(status.data));
	if (retval < 0)
		return;

	switch (status.status_code) {
	case 0x00:
		printk(KERN_INFO "%s: No error.\n", __func__);
		break;

	case 0x01:
		printk(KERN_INFO "%s: Touch IC reset complete.\n", __func__);
		break;

	case 0x02:
		printk(KERN_ERR "%s: Touch IC configuration error--%s.\n",
			__func__, "check platform settings");
		break;

	case 0x03:
		printk(KERN_ERR "%s: Touch IC device failure.\n", __func__);
		break;

	case 0x04:
		printk(KERN_ERR "%s: Configuration CRC failure.\n", __func__);
		break;

	case 0x05:
		printk(KERN_ERR "%s: Firmware CRC failure.\n", __func__);
		break;

	case 0x06:
		printk(KERN_ERR "%s: CRC in progress.\n", __func__);
		break;

	default:
		printk(KERN_ERR "%s: Unknown error 0x%02X received.\n",
			__func__, status.status_code);
		break;
	}
}

 /**
 * synaptics_rmi4_report_touch()
 *
 * Called by synaptics_rmi4_sensor_report().
 *
 * This function calls the appropriate finger data reporting function
 * based on the function handler it receives and returns the number of
 * fingers detected.
 */
static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		unsigned char *touch_count)
{
	unsigned char touch_count_2d;

	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Function %02x reporting\n",
			__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
	case SYNAPTICS_RMI4_F01:
		synaptics_rmi4_f01_handler(rmi4_data, fhandler);
		break;

	case SYNAPTICS_RMI4_F11:
		synaptics_dsx_resumeinfo_isr(rmi4_data);

		touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
				fhandler);

		*touch_count += touch_count_2d;

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;

	case SYNAPTICS_RMI4_F12:
		synaptics_dsx_resumeinfo_isr(rmi4_data);

		touch_count_2d = synaptics_rmi4_f12_abs_report(rmi4_data,
				fhandler);

		*touch_count += touch_count_2d;

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;

	case SYNAPTICS_RMI4_F1A:
		synaptics_dsx_resumeinfo_isr(rmi4_data);

		synaptics_rmi4_f1a_report(rmi4_data, fhandler);
		break;

	default:
		break;
	}

	return;
}

 /**
 * synaptics_rmi4_sensor_report()
 *
 * Called by synaptics_rmi4_irq().
 *
 * This function determines the interrupt source(s) from the sensor
 * and calls synaptics_rmi4_report_touch() with the appropriate
 * function handler for each function with valid data inputs.
 */
static int synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char touch_count = 0;
	unsigned char intr[MAX_INTR_REGISTERS];
	unsigned int intr_status_mask;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fn *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_data_base_addr + 1,
			intr,
			rmi4_data->num_of_intr_regs);
	if (retval < 0)
		return retval;

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->num_of_data_sources) {
			if (fhandler->intr_mask &
					intr[fhandler->intr_reg_num]) {
				synaptics_rmi4_report_touch(rmi4_data,
						fhandler, &touch_count);
			}
		}
	}

	batohui(&intr_status_mask, intr, rmi4_data->num_of_intr_regs);
	/*
	 * Go through external handlers list only when interrupt
	 * is not handled by currently active internal functions.
	 */
	if (intr_status_mask & (~rmi4_data->active_fn_intr_mask)) {
		mutex_lock(&exp_fn_ctrl.list_mutex);
		if (!list_empty(&exp_fn_ctrl.fn_list)) {
			list_for_each_entry(exp_fhandler,
					&exp_fn_ctrl.fn_list, link) {
				if (exp_fhandler->inserted &&
					(exp_fhandler->func_attn != NULL))
					exp_fhandler->func_attn(
							rmi4_data, intr[0]);
			}
		}
		mutex_unlock(&exp_fn_ctrl.list_mutex);
	}

	return touch_count;
}

 /**
 * synaptics_rmi4_irq()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;
	struct synaptics_rmi4_irq_info *tmp_q;

	if (rmi4_data->number_irq > 0) {
		rmi4_data->last_irq++;
		if (rmi4_data->last_irq >= rmi4_data->number_irq)
			rmi4_data->last_irq = 0;
		tmp_q =
			&(rmi4_data->irq_info[rmi4_data->last_irq]);
		getnstimeofday(&(tmp_q->irq_time));
	}

	synaptics_rmi4_sensor_report(rmi4_data);

	return IRQ_HANDLED;
}

 /**
 * synaptics_rmi4_irq_enable()
 *
 * Called by synaptics_rmi4_probe() and the power management functions
 * in this driver and also exported to other expansion Function modules
 * such as rmi_dev.
 *
 * This function handles the enabling and disabling of the attention
 * irq including the setting up of the ISR thread.
 */
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char intr_status;
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	if (enable) {
		if (rmi4_data->irq_enabled)
			return retval;

		/* Clear interrupts first */
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr + 1,
				&intr_status,
				rmi4_data->num_of_intr_regs);
		if (retval < 0)
			return retval;

		retval = request_threaded_irq(rmi4_data->irq, NULL,
				synaptics_rmi4_irq, platform_data->irq_flags,
				DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Started irq thread\n", __func__);

		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;

		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Stopped irq thread\n", __func__);
		}
	}

	return retval;
}

 /**
 * synaptics_rmi4_f11_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This funtion parses information from the Function 11 registers
 * and determines the number of fingers supported, x and y data ranges,
 * offset to the associated interrupt status register, interrupt bit
 * mask, and gathers finger data acquisition capabilities from the query
 * registers.
 */
static int synaptics_rmi4_f11_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned char intr_offset;
	unsigned char abs_data_size;
	unsigned char abs_data_blk_size;
	unsigned char query[F11_STD_QUERY_LEN];
	unsigned char control[F11_STD_CTRL_LEN];

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = (fd->intr_src_count  & MASK_3BIT);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			query,
			sizeof(query));
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	if ((query[1] & MASK_3BIT) <= 4)
		fhandler->num_of_data_points = (query[1] & MASK_3BIT) + 1;
	else if ((query[1] & MASK_3BIT) == 5)
		fhandler->num_of_data_points = 10;

	rmi4_data->num_of_fingers = fhandler->num_of_data_points;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			control,
			sizeof(control));
	if (retval < 0)
		return retval;

	/* Maximum x and y */
	rmi4_data->sensor_max_x = ((control[6] & MASK_8BIT) << 0) |
			((control[7] & MASK_4BIT) << 8);
	rmi4_data->sensor_max_y = ((control[8] & MASK_8BIT) << 0) |
			((control[9] & MASK_4BIT) << 8);
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Function %02x max x = %d max y = %d\n",
			__func__, fhandler->fn_number,
			rmi4_data->sensor_max_x,
			rmi4_data->sensor_max_y);

	/* Reporting mode */
	if (!(control[0] & MASK_3BIT)) {
		pr_warn("Reporting mode: continuous\n");
	} else {
		dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: thresholds: x=0x%x, y=0x%x\n",
			__func__, control[2], control[3]);
	}

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	abs_data_size = query[5] & MASK_2BIT;
	abs_data_blk_size = 3 + (2 * (abs_data_size == 0 ? 1 : 0));
	fhandler->size_of_data_register_block = abs_data_blk_size;

#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);

	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_X, 0,
			rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_Y, 0,
			rmi4_data->sensor_max_y, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_PRESSURE, 0,
			255, 0, 0);
#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			15, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MINOR, 0,
			15, 0, 0);
#endif
#ifdef TYPE_B_PROTOCOL
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers, 0);
#else
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TRACKING_ID, 0,
			rmi4_data->num_of_fingers - 1, 0, 0);
	input_set_events_per_packet(rmi4_data->input_dev, 64);
#endif
	return retval;
}

static int synaptics_rmi4_f1a_alloc_mem(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	struct synaptics_rmi4_f1a_handle *f1a;

	f1a = kzalloc(sizeof(*f1a), GFP_KERNEL);
	if (!f1a) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for function handle\n",
				__func__);
		return -ENOMEM;
	}

	fhandler->data = (void *)f1a;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			fhandler->full_addr.query_base,
			f1a->button_query.data,
			sizeof(f1a->button_query.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read query registers\n",
				__func__);
		return retval;
	}

	f1a->button_count = f1a->button_query.max_button_count + 1;
	f1a->button_bitmask_size = (f1a->button_count + 7) / 8;

	f1a->button_data_buffer = kcalloc(f1a->button_bitmask_size,
			sizeof(*(f1a->button_data_buffer)), GFP_KERNEL);
	if (!f1a->button_data_buffer) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for data buffer\n",
				__func__);
		return -ENOMEM;
	}

	f1a->button_map = kcalloc(f1a->button_count,
			sizeof(*(f1a->button_map)), GFP_KERNEL);
	if (!f1a->button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for button map\n",
				__func__);
		return -ENOMEM;
	}

	return 0;
}

 /**
 * synaptics_rmi4_f12_init()
 *
 * Called by synaptics_rmi4_query_device().
 *
 * This funtion parses information from the Function 12 registers
 * and determines the number of fingers supported, x and y data ranges,
 * offset to the associated interrupt status register, interrupt bit
 * mask, and gathers finger data acquisition capabilities from the query
 * registers.
 */
static int synaptics_rmi4_f12_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned char intr_offset;
	struct synaptics_rmi4_func_packet_regs *regs =
					find_function(SYNAPTICS_RMI4_F12);
	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = (fd->intr_src_count  & MASK_3BIT);

	retval = synaptics_rmi4_scan_f12_reg_info(rmi4_data,
			fhandler->full_addr.query_base + 4,
			fhandler->full_addr.ctrl_base, regs);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_read_packet_regs(rmi4_data, regs);
	if (retval < 0)
		return retval;

	/* Maximum number of fingers supported */
	if (f12_c23[1].present) {
		fhandler->num_of_data_points =
			f12_c23_1.max_num_reported_objects;
		rmi4_data->num_of_fingers = f12_c23_1.max_num_reported_objects;
	} else
		return -ENOENT;

	if (f12_c08[0].present) {
		rmi4_data->sensor_max_x =
			(f12_c08_0.max_x_msb << 8) | f12_c08_0.max_x_lsb;
		rmi4_data->sensor_max_y =
			(f12_c08_0.max_y_msb << 8) | f12_c08_0.max_y_lsb;
	} else
		return -ENOENT;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	/* Data size per touch */
	if (f12_c28[0].present) {
		for (ii = 0; ii < 8; ii++)
			if (f12_c28_0.reported_bytes_per_object & (1 << ii))
				fhandler->size_of_data_register_block++;
	} else
		return -ENOENT;

#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, rmi4_data->input_dev->propbit);
#endif
	set_bit(EV_ABS, rmi4_data->input_dev->evbit);

	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_X, 0,
			rmi4_data->sensor_max_x, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_POSITION_Y, 0,
			rmi4_data->sensor_max_y, 0, 0);
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_PRESSURE, 0,
			255, 0, 0);
#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			255, 0, 0);
#endif
#ifdef TYPE_B_PROTOCOL
	input_mt_init_slots(rmi4_data->input_dev,
			rmi4_data->num_of_fingers, 0);
#else
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TRACKING_ID, 0,
			rmi4_data->num_of_fingers - 1, 0, 0);
	input_set_events_per_packet(rmi4_data->input_dev, 64);
#endif
	return retval;
}

static int synaptics_rmi4_cap_button_map(
				struct synaptics_rmi4_data *rmi4_data,
				struct synaptics_rmi4_fn *fhandler)
{
	unsigned char ii;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	const struct synaptics_dsx_platform_data *pdata = rmi4_data->board;

	if (!pdata->cap_button_map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: cap_button_map is \
				NULL in board file\n",
				__func__);
		return -ENODEV;
	} else if (!pdata->cap_button_map->map) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Button map is missing in board file\n",
				__func__);
		return -ENODEV;
	} else {
		if (pdata->cap_button_map->nbuttons !=
			f1a->button_count) {
			f1a->valid_button_count = min(f1a->button_count,
				pdata->cap_button_map->nbuttons);
		} else {
			f1a->valid_button_count = f1a->button_count;
		}

		set_bit(EV_KEY, rmi4_data->input_dev->evbit);

		for (ii = 0; ii < f1a->valid_button_count; ii++) {
			f1a->button_map[ii] =
					pdata->cap_button_map->map[ii];
			set_bit(f1a->button_map[ii],
					rmi4_data->input_dev->keybit);
			input_set_capability(rmi4_data->input_dev,
					EV_KEY, f1a->button_map[ii]);
		}
	}

	return 0;
}

static void synaptics_rmi4_f1a_kfree(struct synaptics_rmi4_fn *fhandler)
{
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;

	if (f1a) {
		kfree(f1a->button_data_buffer);
		kfree(f1a->button_map);
		kfree(f1a);
		fhandler->data = NULL;
	}

	return;
}

static int synaptics_rmi4_f1a_init(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	int retval;
	unsigned char ii;
	unsigned short intr_offset;

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = (fd->intr_src_count  & MASK_3BIT);

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < ((fd->intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	retval = synaptics_rmi4_f1a_alloc_mem(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	retval = synaptics_rmi4_cap_button_map(rmi4_data, fhandler);
	if (retval < 0)
		goto error_exit;

	rmi4_data->button_0d_enabled = 1;

	return 0;

error_exit:
	synaptics_rmi4_f1a_kfree(fhandler);

	return retval;
}

static int synaptics_rmi4_alloc_fh(struct synaptics_rmi4_fn **fhandler,
		struct synaptics_rmi4_fn_desc *rmi_fd, int page_number)
{
	*fhandler = kzalloc(sizeof(**fhandler), GFP_KERNEL);
	if (!(*fhandler))
		return -ENOMEM;

	(*fhandler)->full_addr.data_base =
			(rmi_fd->data_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.ctrl_base =
			(rmi_fd->ctrl_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.cmd_base =
			(rmi_fd->cmd_base_addr |
			(page_number << 8));
	(*fhandler)->full_addr.query_base =
			(rmi_fd->query_base_addr |
			(page_number << 8));

	return 0;
}

static unsigned char *simple_init_packet_reg(
		struct synaptics_rmi4_packet_reg *reg, short r_offset)
{
	unsigned char *data_ptr = NULL;
	if (reg->subpkt[0].data && reg->subpkt[0].size) {
		reg->subpkt[0].present = true;
		reg->subpkt[0].offset = 0;
		memset(reg->subpkt[0].data, 0, reg->subpkt[0].size);

		reg->offset = r_offset;
		kfree(reg->data);
		data_ptr = kzalloc(reg->size, GFP_KERNEL);
	}
	return data_ptr;
}

static void simple_deinit_packet_reg(struct synaptics_rmi4_packet_reg *reg)
{
	reg->expected = false;
	reg->offset = -1;
	reg->size = 0;
	reg->subpkt[0].expected = false;
	reg->subpkt[0].offset = -1;
}

static void synaptics_rmi4_scan_f01_reg_info(
		struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short query42_offset = F01_QUERY21_OFFSET;
	unsigned short query43_size = 0;
	unsigned short ctrl9_offset = 0;
	bool has_recalibration = false;
	bool can_access_ctrl9 = false;
	u8 query43[4];
	struct synaptics_rmi4_f01_query42 query42;
	struct synaptics_rmi4_f01_query1 query1;
	struct synaptics_rmi4_func_packet_regs *regs;
	struct synaptics_rmi4_packet_reg *reg;

	regs = find_function(SYNAPTICS_RMI4_F01);
	if (unlikely(regs == NULL)) {
		dev_info(&rmi4_data->i2c_client->dev,
			"%s: F01 registers not defined\n", __func__);
		return;
	}

	regs->base_addr = rmi4_data->f01_ctrl_base_addr;
	pr_debug("F01_RMI_Ctrl base addr 0x%x\n", regs->base_addr);

	reg = find_packet_reg(regs, 0);
	if (unlikely(reg == NULL)) {
		dev_warn(&rmi4_data->i2c_client->dev,
			"%s: F01_RMI_Ctrl0 not defined\n",
			__func__);
		return;
	}

	if (regs && reg && reg->nr_subpkts) {
		reg->data = simple_init_packet_reg(reg, 0);
		if (!reg->data) {
			simple_deinit_packet_reg(reg);
			dev_warn(&rmi4_data->i2c_client->dev,
				"%s: remove r%d.s0 from the list\n",
				__func__, reg->r_number);
		}
	}

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr + F01_QUERY1_OFFSET,
			query1.data,
			sizeof(query1.data));
	if (retval < 0) {
		dev_warn(&rmi4_data->i2c_client->dev,
			"%s: error reading F01_RMI_Query1\n", __func__);
		return;
	}

	if (query1.reserved) {
		ctrl9_offset++;		/* has F01_RMI_Ctrl4 */
		query42_offset++;	/* has F01_RMI_Query21 */
		query42_offset += 18;	/* has F01_RMI_Query23-41 */
		pr_debug("has F01_RMI_Query21,23-41 and F01_RMI_Ctrl4\n");
	}

	if (query1.has_sensor_id) {
		query42_offset++;	/* has F01_RMI_Query22 */
		pr_debug("has F01_RMI_Query22\n");
	}

	if (query1.has_adjustable_doze) {
		ctrl9_offset += 2;	/* has F01_RMI_Ctrl2 & Ctrl3 */
		pr_debug("has F01_RMI_Ctrl2 and F01_RMI_Ctrl3\n");
	}

	if (query1.has_doze_holdoff) {
		ctrl9_offset++;		/* has F01_RMI_Ctrl5 */
		pr_debug("has F01_RMI_Ctrl5\n");
	}

	query42.data[0] = 0;
	if (query1.has_query42) {
		pr_debug("F01_RMI_Query42 offset %d\n", query42_offset);
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_query_base_addr +
				query42_offset,
				query42.data,
				sizeof(query42.data));
		if (retval < 0) {
			dev_warn(&rmi4_data->i2c_client->dev,
				"%s: error reading F01_RMI_Query42\n",
				__func__);
		} else
			has_recalibration = query42.has_recalibration;
	}

	if (query42.has_ds4_queries) {
		retval = synaptics_rmi4_i2c_read(rmi4_data,
				rmi4_data->f01_query_base_addr +
				query42_offset + 1,
				query43,
				sizeof(query43));
		if (retval < 0) {
			dev_warn(&rmi4_data->i2c_client->dev,
				"%s: error reading F01_RMI_Query43\n",
				__func__);
		} else {
			can_access_ctrl9 = true;
			query43_size = query43[0] & 0xf;
			pr_debug("F01_RMI_Query43 size: %d\n", query43_size);
		}
	}

	if (query43_size) {
		if (query43[2] & 0x1)
			ctrl9_offset++;	/* has F01_RMI_Ctrl6 */
		if (query43[2] & 0x2)
			ctrl9_offset++;	/* has F01_RMI_Ctrl7 */
		if (query43[2] & 0x4)
			ctrl9_offset++;	/* has F01_RMI_Ctrl8 */
	}

	reg = find_packet_reg(regs, 9);
	if (unlikely(reg == NULL)) {
		dev_warn(&rmi4_data->i2c_client->dev,
			"%s: F01_RMI_Ctrl9 not present\n", __func__);
		return;
	}

	if (regs && reg) {
		if (has_recalibration == false || can_access_ctrl9 == false) {
			pr_debug("remove F01_RMI_Ctrl9\n");
			simple_deinit_packet_reg(reg);
			return;
		}

		ctrl9_offset += rmi4_data->num_of_intr_regs;
		pr_debug("F01_RMI_Ctrl9 offset %d\n", ctrl9_offset);
		reg->data = simple_init_packet_reg(reg, ctrl9_offset);
		if (!reg->data) {
			simple_deinit_packet_reg(reg);
			dev_warn(&rmi4_data->i2c_client->dev,
				"%s: remove r%d.s0 from the list\n",
				__func__, reg->r_number);
		}
	}
}

 /**
 * synaptics_rmi4_query_device()
 *
 * Called by synaptics_rmi4_probe().
 *
 * This funtion scans the page description table, records the offsets
 * to the register types of Function $01, sets up the function handlers
 * for Function $11 and Function $12, determines the number of interrupt
 * sources from the sensor, adds valid Functions with data inputs to the
 * Function linked list, parses information from the query registers of
 * Function $01, and enables the interrupt sources from the valid Functions
 * with data inputs.
 */
static int synaptics_rmi4_query_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char page_number;
	unsigned char intr_count = 0;
	unsigned char data_sources = 0;
	unsigned char f01_query[F01_STD_QUERY_LEN] = {0};
	unsigned short pdt_entry_addr;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;
	struct f34_properties f34_query;
	struct synaptics_rmi4_f01_device_status status;

	rmi = &(rmi4_data->rmi4_mod_info);

	INIT_LIST_HEAD(&rmi->support_fn_list);

	/* Scan the page description tables of the pages to service */
	for (page_number = 0; page_number < PAGES_TO_SERVICE; page_number++) {
		for (pdt_entry_addr = PDT_START; pdt_entry_addr > PDT_END;
				pdt_entry_addr -= PDT_ENTRY_SIZE) {
			pdt_entry_addr |= (page_number << 8);

			retval = synaptics_rmi4_i2c_read(rmi4_data,
					pdt_entry_addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			fhandler = NULL;

			if (rmi_fd.fn_number == 0) {
				dev_dbg(&rmi4_data->i2c_client->dev,
						"%s: Reached end of PDT\n",
						__func__);
				break;
			}

			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: F%02x found (page %d)\n",
					__func__, rmi_fd.fn_number,
					page_number);

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F34:
				retval = synaptics_rmi4_i2c_read(rmi4_data,
						rmi_fd.query_base_addr +
							F34_PROPERTIES_OFFSET,
						&f34_query.data[0],
						sizeof(f34_query));
				if (retval < 0)
					return retval;

				if (f34_query.has_config_id) {
					retval = synaptics_rmi4_i2c_read(
							rmi4_data,
							rmi_fd.ctrl_base_addr,
							rmi->config_id,
							sizeof(rmi->config_id));
					if (retval < 0)
						return retval;
				}
				break;

			case SYNAPTICS_RMI4_F01:
				rmi4_data->f01_query_base_addr =
						rmi_fd.query_base_addr;
				rmi4_data->f01_ctrl_base_addr =
						rmi_fd.ctrl_base_addr;
				rmi4_data->f01_data_base_addr =
						rmi_fd.data_base_addr;
				rmi4_data->f01_cmd_base_addr =
						rmi_fd.cmd_base_addr;

				retval = synaptics_rmi4_i2c_read(rmi4_data,
						rmi4_data->f01_data_base_addr,
						status.data,
						sizeof(status.data));
				if (retval < 0)
					return retval;

				rmi4_data->in_bootloader =
						status.flash_prog == 1;
				break;

			case SYNAPTICS_RMI4_F11:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%x\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f11_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;
				break;

			case SYNAPTICS_RMI4_F12:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%x\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f12_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;

				break;

			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to alloc for F%x\n",
						__func__, rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f1a_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				switch (retval) {
				case -ENOMEM:
					return retval;
				case -ENODEV:
					/* removing buttons handler if there */
					/* were no key codes in platform data */
					kfree(fhandler);
					fhandler = NULL;
					rmi_fd.intr_src_count = 0;
					break;
				}
				break;
			}

			/* Accumulate the interrupt count */
			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);

			if (fhandler && rmi_fd.intr_src_count) {
				list_add_tail(&fhandler->link,
						&rmi->support_fn_list);
			}
		}
	}

	rmi4_data->num_of_intr_regs = (intr_count + 7) / 8;
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Number of interrupt registers = %d\n",
			__func__, rmi4_data->num_of_intr_regs);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr,
			f01_query,
			sizeof(f01_query));
	if (retval < 0)
		return retval;

	/* RMI Version 4.0 currently supported */
	rmi->version_major = 4;
	rmi->version_minor = 0;

	rmi->manufacturer_id = f01_query[0];
	rmi->product_props = f01_query[1];
	memcpy(&rmi->product_info[0], &f01_query[2],
					SYNAPTICS_RMI4_PRODUCT_INFO_SIZE);
	memcpy(rmi->serial, &f01_query[4], SYNAPTICS_RMI4_SERIAL_SIZE);
	memcpy(rmi->product_id_string, &f01_query[11],
					SYNAPTICS_RMI4_PRODUCT_ID_SIZE);
	rmi->product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE] = 0;

	synaptics_dsx_validate_product_string(rmi->product_id_string);

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr+PACKAGE_ID_OFFSET,
			rmi->package_id,
			sizeof(rmi->package_id));
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_query_base_addr+FW_VERSION_OFFSET,
			rmi->build_id,
			sizeof(rmi->build_id));
	if (retval < 0)
		return retval;

	if (rmi->manufacturer_id != 1) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Non-Synaptics device found,"
			"manufacturer ID = %d\n",
			__func__, rmi->manufacturer_id);
	}

	if (!rmi4_data->in_bootloader && !rmi4_data->input_registered) {
		retval = input_register_device(rmi4_data->input_dev);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to register input device\n",
				__func__);
			return retval;
		}
		rmi4_data->input_registered = true;
	}

	memset(rmi4_data->intr_mask, 0x00, sizeof(rmi4_data->intr_mask));

	/*
	 * Map out the interrupt bit masks for the interrupt sources
	 * from the registered function handlers.
	 */
	list_for_each_entry(fhandler, &rmi->support_fn_list, link)
		data_sources += fhandler->num_of_data_sources;
	if (data_sources) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				rmi4_data->intr_mask[fhandler->intr_reg_num] |=
						fhandler->intr_mask;
			}
		}
	}

	/* interrupt mask for currently active functions */
	batohui(&rmi4_data->active_fn_intr_mask,
			rmi4_data->intr_mask, rmi4_data->num_of_intr_regs);
	pr_debug("active func intr_mask 0x%x\n",
			rmi4_data->active_fn_intr_mask);

	if (rmi4_data->in_bootloader)
		pr_info("Product: %s is in bootloader mode\n",
			rmi->product_id_string);
	else {
		unsigned int config_id, firmware_id;

		batohui(&firmware_id, rmi->build_id, sizeof(rmi->build_id));
		batohui(&config_id, rmi->config_id, sizeof(config_id));
		pr_info("Product: %s, firmware id: %x, config id: %08x\n",
			rmi->product_id_string, firmware_id, config_id);

		synaptics_rmi4_scan_f01_reg_info(rmi4_data);
	}

	return 0;
}

static void synaptics_rmi4_cleanup(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fn *fhandler, *next_list_entry;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	list_for_each_entry_safe(fhandler, next_list_entry,
					&rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A)
			synaptics_rmi4_f1a_kfree(fhandler);
		else
			kfree(fhandler->data);
		list_del(&fhandler->link);
		kfree(fhandler);
	}
}

static void synaptics_dsx_release_all(struct synaptics_rmi4_data *rmi4_data)
{
	/* reset some TSB global vars like fingers_on_2d after resume
	 * of reset touch IC
	 */
	if (rmi4_data->button_0d_enabled) {
		tsb_buff_clean_flag = 1;
		rmi4_data->fingers_on_2d = false;
	}

	if (rmi4_data->input_dev) {
		/*
		 * Enforce touch release event report to work-around
		 * such event missing while touch IC is off.
		*/
#ifdef TYPE_B_PROTOCOL
		int i;
		for (i = 0; i < rmi4_data->num_of_fingers; i++) {
			input_mt_slot(rmi4_data->input_dev, i);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, false);
		}
#else
		input_mt_sync(rmi4_data->input_dev);
#endif
		input_sync(rmi4_data->input_dev);
	}
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data,
		unsigned char *f01_cmd_base_addr)
{
	int current_state, retval;
	bool need_to_query = false;

	current_state = synaptics_dsx_get_state_safe(rmi4_data);
	if (current_state == STATE_UNKNOWN) {
		synaptics_rmi4_cleanup(rmi4_data);
		need_to_query = true;
	}

	retval = synaptics_dsx_ic_reset(rmi4_data, RMI4_HW_RESET);
	if (retval > 0)
		pr_debug("successful reset took %dms\n", retval);
	else
		dev_warn(&rmi4_data->i2c_client->dev, "%s: timed out waiting for idle\n",
			__func__);

	if (need_to_query) {
		if (!rmi4_data->input_dev) {
			retval = synaptics_dsx_alloc_input(rmi4_data);
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to allocate input device\n",
					__func__);
				return retval;
			}
		}

		retval = synaptics_rmi4_query_device(rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to query device\n",
				__func__);
			return retval;
		}
		/* kick off detection work after touch ic changes its mode */
		if (exp_fn_ctrl.det_workqueue)
			queue_delayed_work(exp_fn_ctrl.det_workqueue,
					&exp_fn_ctrl.det_work, 0);
	}

	return 0;
}

/**
* synaptics_rmi4_detection_work()
*
* Called by the kernel at the scheduled time.
*
* This function is armed by synaptics_new_function call. It checks for
* the insertion and removal of other expansion Function modules such as
* rmi_dev and calls their initialization and removal callback functions
* accordingly.
*/
static void synaptics_rmi4_detection_work(struct work_struct *work)
{
	struct synaptics_rmi4_exp_fn *exp_fhandler, *next_list_entry;
	struct synaptics_rmi4_data *rmi4_data;
	int state;

	mutex_lock(&exp_fn_ctrl_mutex);
	rmi4_data = exp_fn_ctrl.rmi4_data_ptr;
	mutex_unlock(&exp_fn_ctrl_mutex);

	if (rmi4_data == NULL) {
		if (exp_fn_ctrl.det_workqueue)
			queue_delayed_work(exp_fn_ctrl.det_workqueue,
				&exp_fn_ctrl.det_work,
				msecs_to_jiffies(EXP_FN_DET_INTERVAL));
		return;
	}

	mutex_lock(&exp_fn_ctrl.list_mutex);
	if (list_empty(&exp_fn_ctrl.fn_list))
		goto release_mutex;

	list_for_each_entry_safe(exp_fhandler,
				next_list_entry,
				&exp_fn_ctrl.fn_list,
				link) {
		if ((exp_fhandler->func_init != NULL) &&
				(exp_fhandler->inserted == false)) {
			if (rmi4_data->in_bootloader &&
			   (exp_fhandler->mode == IC_MODE_UI))
				continue;
			exp_fhandler->func_init(rmi4_data);
			state = synaptics_dsx_get_state_safe(rmi4_data);
			exp_fhandler->inserted = true;
		} else if ((exp_fhandler->func_init == NULL) &&
			   (exp_fhandler->inserted == true)) {
			exp_fhandler->func_remove(rmi4_data);
			list_del(&exp_fhandler->link);
			kfree(exp_fhandler);
		}
	}

release_mutex:
	mutex_unlock(&exp_fn_ctrl.list_mutex);

	return;
}

/**
* synaptics_rmi4_new_function()
*
* Called by other expansion Function modules in their module init and
* module exit functions.
*
* This function is used by other expansion Function modules such as
* rmi_dev to register themselves with the driver by providing their
* initialization and removal callback function pointers so that they
* can be inserted or removed dynamically at module init and exit times,
* respectively.
*/
void synaptics_rmi4_new_function(enum exp_fn fn_type, bool insert,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask), enum ic_modes mode)
{
	struct synaptics_rmi4_exp_fn *exp_fhandler;

	mutex_lock(&exp_fn_ctrl_mutex);
	if (!exp_fn_ctrl.inited) {
		mutex_init(&exp_fn_ctrl.list_mutex);
		INIT_LIST_HEAD(&exp_fn_ctrl.fn_list);
		exp_fn_ctrl.det_workqueue =
			create_singlethread_workqueue("rmi_det_workqueue");
		if (IS_ERR_OR_NULL(exp_fn_ctrl.det_workqueue))
			pr_err("unable to create a workqueue\n");
		INIT_DELAYED_WORK(&exp_fn_ctrl.det_work,
			synaptics_rmi4_detection_work);
		exp_fn_ctrl.inited = true;
	}
	mutex_unlock(&exp_fn_ctrl_mutex);

	mutex_lock(&exp_fn_ctrl.list_mutex);
	if (insert) {
		exp_fhandler = kzalloc(sizeof(*exp_fhandler), GFP_KERNEL);
		if (!exp_fhandler) {
			pr_err("failed to alloc mem for expansion function\n");
			goto exit;
		}
		exp_fhandler->fn_type = fn_type;
		exp_fhandler->func_init = func_init;
		exp_fhandler->func_attn = func_attn;
		exp_fhandler->func_remove = func_remove;
		exp_fhandler->inserted = false;
		exp_fhandler->mode = mode;
		list_add_tail(&exp_fhandler->link, &exp_fn_ctrl.fn_list);
	} else {
		list_for_each_entry(exp_fhandler, &exp_fn_ctrl.fn_list, link) {
			if (exp_fhandler->func_init == func_init) {
				exp_fhandler->inserted = false;
				exp_fhandler->func_init = NULL;
				exp_fhandler->func_attn = NULL;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&exp_fn_ctrl.list_mutex);
	if (exp_fn_ctrl.det_workqueue)
		queue_delayed_work(exp_fn_ctrl.det_workqueue,
					&exp_fn_ctrl.det_work, 0);
	return;
}
EXPORT_SYMBOL(synaptics_rmi4_new_function);

static int synaptics_dsx_init_mode(struct synaptics_rmi4_data *data,
		struct synaptics_dsx_patchset **pmode)
{
	int i;
	struct synaptics_dsx_patchset *mode = *pmode =
		kzalloc(sizeof(struct synaptics_dsx_patchset), GFP_KERNEL);
	if (!mode)
		return -ENOMEM;

	mode->patch_num = MAX_NUM_STATES;
	mode->patch_data = kzalloc(sizeof(struct synaptics_dsx_patch) *
		mode->patch_num, GFP_KERNEL);
	if (!mode->patch_data) {
		kfree(mode);
		return -ENOMEM;
	}

	for (i = 0; i < mode->patch_num; i++)
		INIT_LIST_HEAD(&mode->patch_data[i].cfg_head);
	return 0;
}

static void synaptics_dsx_queued_resume(struct work_struct *w)
{
	struct synaptics_rmi4_data *rmi4_data =
		container_of(w, struct synaptics_rmi4_data, resume_work);
	synaptics_rmi4_resume(&(rmi4_data->i2c_client->dev));
}

static inline int synaptics_dsx_display_off(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	cancel_work_sync(&rmi4_data->resume_work);
	return synaptics_rmi4_suspend(dev);
}

static inline int synaptics_dsx_display_on(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	pr_debug("queue resume\n");
	queue_work(system_wq, &rmi4_data->resume_work);
	return 0;
}

 /**
 * synaptics_rmi4_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 * This funtion allocates and initializes the resources for the driver
 * as an input driver, turns on the power to the sensor, queries the
 * sensor for its supported Functions and characteristics, registers
 * the driver to the input subsystem, sets up the interrupt, and creates
 * a work queue for detection of other expansion Function modules.
 */
static int synaptics_rmi4_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval = -EINVAL;
	struct pinctrl *pinctrl;
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_device_info *rmi;
	struct synaptics_dsx_platform_data *platform_data;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data not supported\n",
				__func__);
		return -EIO;
	}

	rmi4_data = kzalloc(sizeof(*rmi4_data), GFP_KERNEL);
	pr_debug("rmi4_data allocated = %p\n", rmi4_data);
	if (!rmi4_data) {
		dev_err(&client->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
	}

	/* assign pointer to client structure right away for further use */
	rmi4_data->i2c_client = client;

	retval = synaptics_dsx_init_mode(rmi4_data, &rmi4_data->default_mode);
	if (retval) {
		dev_err(&client->dev, "%s: Failed to alloc dflt mode mem\n",
				__func__);
		return retval;
	}

	retval = synaptics_dsx_init_mode(rmi4_data, &rmi4_data->alternate_mode);
	if (retval) {
		dev_err(&client->dev, "%s: Failed to alloc alt mode mem\n",
				__func__);
		return retval;
	}

	if (client->dev.of_node)
		platform_data = synaptics_dsx_of_init(client, rmi4_data);
	else
		platform_data = client->dev.platform_data;

	if (!platform_data) {
		dev_err(&client->dev,
				"%s: No platform data found\n",
				__func__);
		kfree(rmi4_data);
		return -EINVAL;
	}

	rmi4_data->current_mode = rmi4_data->default_mode;

	rmi = &(rmi4_data->rmi4_mod_info);

	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->board = platform_data;
	rmi4_data->irq_enabled = false;
	atomic_set(&rmi4_data->touch_stopped, 1);
	rmi4_data->ic_on = true;
	rmi4_data->flash_enabled = false;

	rmi4_data->i2c_read = synaptics_rmi4_i2c_read;
	rmi4_data->i2c_write = synaptics_rmi4_i2c_write;
	rmi4_data->set_state = synaptics_dsx_sensor_state;
	rmi4_data->ready_state = synaptics_dsx_sensor_ready_state;
	rmi4_data->irq_enable = synaptics_rmi4_irq_enable;
	rmi4_data->reset_device = synaptics_rmi4_reset_device;

	/* Initialize some resume debug information */
	rmi4_data->resume_info = kzalloc(
		sizeof(struct synaptics_rmi4_resume_info) *
			 MAX_NUMBER_TRACKED_RESUMES,
		GFP_KERNEL);
	if (!rmi4_data->resume_info) {
		dev_err(&client->dev,
		"%s: Failed to allocate memory for resume information\n",
				__func__);
		rmi4_data->number_resumes = 0;
	} else
		rmi4_data->number_resumes = MAX_NUMBER_TRACKED_RESUMES;
	rmi4_data->last_resume = -1;

	/* Initialize some interrupit timing debug information */
	rmi4_data->irq_info = kzalloc(
		sizeof(struct synaptics_rmi4_irq_info) *
			 MAX_NUMBER_TRACKED_IRQS,
		GFP_KERNEL);
	if (!rmi4_data->irq_info) {
		dev_err(&client->dev,
		"%s: Failed to allocate memory for IRQ debug information\n",
				__func__);
		rmi4_data->number_irq = 0;
	} else
		rmi4_data->number_irq = MAX_NUMBER_TRACKED_IRQS;
	rmi4_data->last_irq = -1;

	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->state_mutex));

	pinctrl = devm_pinctrl_get_select(&rmi4_data->i2c_client->dev,
		"active");
	if (IS_ERR(pinctrl)) {
		long int error = PTR_ERR(pinctrl);
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: pinctrl failed err %ld\n", __func__, error);
		goto err_input_device;
	}

	if (platform_data->gpio_config)
		retval = platform_data->gpio_config(platform_data, true);
	else {
		retval = gpio_request(platform_data->reset_gpio,
						RESET_GPIO_NAME);
		if (!retval)
			retval = gpio_direction_output(
						platform_data->reset_gpio, 1);
	}
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to configure GPIO\n",
				__func__);
		goto err_input_device;
	}

	/* get irq number initialized before calling reset */
	rmi4_data->irq = gpio_to_irq(platform_data->irq_gpio);

	i2c_set_clientdata(client, rmi4_data);

	retval = synaptics_dsx_alloc_input(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to allocate input device\n",
				__func__);
		goto err_input_device;
	}

	rmi4_data->regulator = regulator_get(&client->dev, "touch_vdd");
	if (IS_ERR(rmi4_data->regulator)) {
		dev_warn(&client->dev,
				"%s: Failed to get regulator\n",
				__func__);
	} else {
		int error = regulator_enable(rmi4_data->regulator);
		if (error) {
			regulator_put(rmi4_data->regulator);
			dev_err(&client->dev,
				"%s: Error %d enabling touch-vdd regulator\n",
				__func__, error);
			goto err_regulator_enable;
		}
		platform_data->regulator_en = true;
		pr_debug("touch-vdd regulator is %s\n",
			regulator_is_enabled(rmi4_data->regulator) ?
			"on" : "off");
	}

	retval = synaptics_dsx_ic_reset(rmi4_data, RMI4_HW_RESET);
	if (retval > 0)
		pr_debug("successful reset took %dms\n", retval);
	else
		dev_warn(&rmi4_data->i2c_client->dev, "%s: timed out waiting for idle\n",
			__func__);

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	init_waitqueue_head(&rmi4_data->wait);
	INIT_WORK(&rmi4_data->resume_work, synaptics_dsx_queued_resume);

#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
	rmi4_data->panel_nb.pre_display_off = synaptics_rmi4_suspend;
	rmi4_data->panel_nb.display_on = synaptics_rmi4_resume;
	rmi4_data->panel_nb.dev = &client->dev;
	if (!mmi_panel_register_notifier(&rmi4_data->panel_nb))
		pr_info("registered MMI panel notifier\n");
	else
		dev_err(&client->dev,
				"%s: Unable to register MMI notifier\n",
				__func__);
#elif defined(CONFIG_FB)
	rmi4_data->panel_nb.notifier_call = synaptics_dsx_panel_cb;
	if (!fb_register_client(&rmi4_data->panel_nb))
		pr_debug("registered FB notifier\n");
	else
		dev_err(&client->dev,
				"%s: Unable to register FB notifier\n",
				__func__);
#endif
	mutex_lock(&exp_fn_ctrl_mutex);
	if (!exp_fn_ctrl.inited) {
		mutex_init(&exp_fn_ctrl.list_mutex);
		INIT_LIST_HEAD(&exp_fn_ctrl.fn_list);
		exp_fn_ctrl.det_workqueue =
			create_singlethread_workqueue("rmi_det_workqueue");
		if (IS_ERR_OR_NULL(exp_fn_ctrl.det_workqueue))
			pr_err("unable to create a workqueue\n");
		INIT_DELAYED_WORK(&exp_fn_ctrl.det_work,
			synaptics_rmi4_detection_work);
		exp_fn_ctrl.inited = true;
	}
	mutex_unlock(&exp_fn_ctrl_mutex);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			goto err_sysfs;
		}
	}

	synaptics_dsx_sensor_ready_state(rmi4_data, true);

	mutex_lock(&exp_fn_ctrl_mutex);
	exp_fn_ctrl.rmi4_data_ptr = rmi4_data;
	mutex_unlock(&exp_fn_ctrl_mutex);

	return retval;

err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
	}

#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
	mmi_panel_unregister_notifier(&rmi4_data->panel_nb);
#elif defined(CONFIG_FB)
	fb_unregister_client(&rmi4_data->panel_nb);
#endif

err_query_device:
	if (rmi4_data->input_registered) {
		input_unregister_device(rmi4_data->input_dev);
		rmi4_data->input_dev = NULL;
	}

	if (platform_data->regulator_en) {
		regulator_disable(rmi4_data->regulator);
		regulator_put(rmi4_data->regulator);
	}

err_regulator_enable:
	synaptics_rmi4_cleanup(rmi4_data);
	input_free_device(rmi4_data->input_dev);

err_input_device:
	kfree(rmi4_data);

	return retval;
}

 /**
 * synaptics_rmi4_remove()
 *
 * Called by the kernel when the association with an I2C device of the
 * same name is broken (when the driver is unloaded).
 *
 * This funtion terminates the work queue, stops sensor data acquisition,
 * frees the interrupt, unregisters the driver from the input subsystem,
 * turns off the power to the sensor, and frees other allocated resources.
 */
static int synaptics_rmi4_remove(struct i2c_client *client)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = i2c_get_clientdata(client);
	struct synaptics_rmi4_device_info *rmi;
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (exp_fn_ctrl.inited) {
		cancel_delayed_work_sync(&exp_fn_ctrl.det_work);
		flush_workqueue(exp_fn_ctrl.det_workqueue);
		destroy_workqueue(exp_fn_ctrl.det_workqueue);
	}

	atomic_set(&rmi4_data->touch_stopped, 1);
	wake_up(&rmi4_data->wait);

	synaptics_rmi4_irq_enable(rmi4_data, false);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
	}

	if (rmi4_data->input_registered) {
		input_unregister_device(rmi4_data->input_dev);
		rmi4_data->input_dev = NULL;
	}

	if (platform_data->regulator_en) {
		regulator_disable(rmi4_data->regulator);
		regulator_put(rmi4_data->regulator);
	}

#if defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
	mmi_panel_unregister_notifier(&rmi4_data->panel_nb);
#elif defined(CONFIG_FB)
	fb_unregister_client(&rmi4_data->panel_nb);
#endif
	synaptics_rmi4_cleanup(rmi4_data);
	kfree(rmi4_data);

	return 0;
}

#if defined(CONFIG_FB) && !defined(CONFIG_MMI_PANEL_NOTIFICATIONS)
static int synaptics_dsx_panel_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct synaptics_rmi4_data *rmi4_data =
		container_of(nb, struct synaptics_rmi4_data, panel_nb);

	if ((event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK) &&
			evdata && evdata->data && rmi4_data) {
		int *blank = evdata->data;
		pr_debug("fb notification: event = %lu blank = %d\n", event, *blank);
		/* entering suspend upon early blank event */
		/* to ensure shared power supply is still on */
		/* for in-cell design touch solutions */
		if (event == FB_EARLY_EVENT_BLANK) {
			if (*blank != FB_BLANK_POWERDOWN)
				return 0;
			synaptics_dsx_display_off(&rmi4_data->i2c_client->dev);
		} else if (*blank == FB_BLANK_UNBLANK ||
			(*blank == FB_BLANK_VSYNC_SUSPEND &&
			atomic_read(&rmi4_data->touch_stopped))) {
			synaptics_dsx_display_on(&rmi4_data->i2c_client->dev);
		}
	}

	return 0;
}
#endif

static void synaptics_dsx_resumeinfo_start(
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_resume_info *tmp;

	if (rmi4_data->number_resumes > 0) {
		rmi4_data->last_resume++;
		if (rmi4_data->last_resume >= rmi4_data->number_resumes)
			rmi4_data->last_resume = 0;

		tmp = &(rmi4_data->resume_info[rmi4_data->last_resume]);
		getnstimeofday(&(tmp->start));
		tmp->ignored_events = 0;
		tmp->isr.tv_sec = 0;
		tmp->isr.tv_nsec = 0;
		tmp->send_touch.tv_sec = 0;
		tmp->send_touch.tv_nsec = 0;
		tmp->purge_off.tv_sec = 0;
		tmp->purge_off.tv_nsec = 0;
	}
}

static void synaptics_dsx_resumeinfo_finish(
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_resume_info *tmp;

	if (rmi4_data->number_resumes > 0 && rmi4_data->last_resume >= 0) {
		tmp = &(rmi4_data->resume_info[rmi4_data->last_resume]);
		getnstimeofday(&(tmp->finish));
	}
}

static void synaptics_dsx_resumeinfo_isr(
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_resume_info *tmp;

	if (rmi4_data->number_resumes > 0 && rmi4_data->last_resume >= 0) {
		tmp = &(rmi4_data->resume_info[rmi4_data->last_resume]);
		if (tmp->isr.tv_sec == 0)
			getnstimeofday(&(tmp->isr));
	}
}

static void synaptics_dsx_resumeinfo_purgeoff(
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_resume_info *tmp;

	if (rmi4_data->number_resumes > 0 && rmi4_data->last_resume >= 0) {
		tmp = &(rmi4_data->resume_info[rmi4_data->last_resume]);

		if (tmp->purge_off.tv_sec == 0)
			getnstimeofday(&(tmp->purge_off));
	}
}

static void synaptics_dsx_resumeinfo_ignore(
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_resume_info *tmp;

	if (rmi4_data->number_resumes > 0 && rmi4_data->last_resume >= 0) {
		tmp = &(rmi4_data->resume_info[rmi4_data->last_resume]);

		tmp->ignored_events++;
	}
}

static void synaptics_dsx_resumeinfo_touch(
		struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_resume_info *tmp;

	if (rmi4_data->number_resumes > 0 && rmi4_data->last_resume >= 0) {
		tmp = &(rmi4_data->resume_info[rmi4_data->last_resume]);

		if (tmp->send_touch.tv_sec == 0)
			getnstimeofday(&(tmp->send_touch));
	}
}
 /**
 * synaptics_rmi4_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 * This function stops finger data acquisition and puts the sensor to
 * sleep, disables the interrupt, and turns off the power to the sensor.
 */
static int synaptics_rmi4_suspend(struct device *dev)
{
	struct pinctrl *pinctrl;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	if (atomic_cmpxchg(&rmi4_data->touch_stopped, 0, 1) == 1)
		return 0;

	rmi4_data->flash_enabled = false;
	synaptics_dsx_sensor_state(rmi4_data, STATE_SUSPEND);
	synaptics_dsx_release_all(rmi4_data);

	if (rmi4_data->purge_enabled) {
		int value = 1; /* set flag */
		atomic_set(&rmi4_data->panel_off_flag, value);
		pr_debug("touches purge is %s\n", value ? "ON" : "OFF");
	}

	if (rmi4_data->ic_on) {
		/* use pinctrl to put touch RESET GPIO into SUSPEND state */
		gpio_free(platform_data->reset_gpio);
		pinctrl = devm_pinctrl_get_select_default(
			&rmi4_data->i2c_client->dev);
		if (IS_ERR(pinctrl))
			dev_err(&rmi4_data->i2c_client->dev,
				"pinctrl failed err %ld\n", PTR_ERR(pinctrl));

		/* if touch REGULATOR is available - turn it OFF */
		if (platform_data->regulator_en) {
			regulator_disable(rmi4_data->regulator);
			pr_debug("touch-vdd regulator is %s\n",
				regulator_is_enabled(rmi4_data->regulator) ?
				"on" : "off");
		}

		rmi4_data->ic_on = false;
	}

	return 0;
}

 /**
 * synaptics_rmi4_resume()
 *
 * Called by the kernel during the resume phase when the system
 * wakes up from suspend.
 *
 * This function turns on the power to the sensor, wakes the sensor
 * from sleep, enables the interrupt, and starts finger data
 * acquisition.
 */
static int synaptics_rmi4_resume(struct device *dev)
{
	int retval;
	int reset = RMI4_HW_RESET;
	struct pinctrl *pinctrl;
	struct synaptics_rmi4_data *rmi4_data =
					i2c_get_clientdata(to_i2c_client(dev));
	const struct synaptics_dsx_platform_data *platform_data =
					rmi4_data->board;

	if (atomic_cmpxchg(&rmi4_data->touch_stopped, 1, 0) == 0)
		return 0;

	synaptics_dsx_resumeinfo_start(rmi4_data);

	if (!rmi4_data->ic_on) {
		/* if touch REGULATOR is avaialble - turn it ON */
		if (platform_data->regulator_en) {
			int error = regulator_enable(rmi4_data->regulator);
			if (error) {
				pr_err("Error %d enabling touch-vdd regulator\n",
					error);
				return error;
			}
			pr_debug("touch-vdd regulator is %s\n",
				regulator_is_enabled(rmi4_data->regulator) ?
				"on" : "off");
		}

		/* if RESET GPIO is in SUSPEND state - no HW reset */
		retval = gpio_get_value(platform_data->reset_gpio);
		pr_debug("reset gpio state: %d\n", retval);
		if (retval == 0)
			reset = RMI4_WAIT_READY;

		pinctrl = devm_pinctrl_get_select(&rmi4_data->i2c_client->dev,
			"active");
		if (IS_ERR(pinctrl)) {
			long int error = PTR_ERR(pinctrl);
			dev_err(&rmi4_data->i2c_client->dev,
				"pinctrl failed err %ld\n", error);
		}

		if (gpio_request(platform_data->reset_gpio,
						RESET_GPIO_NAME) < 0)
			pr_err("failed to request reset gpio\n");

		gpio_direction_output(platform_data->reset_gpio, 1);
		rmi4_data->ic_on = true;
	}

	/* perform HW reset if needed and wait for touch IC boot completion */
	retval = synaptics_dsx_ic_reset(rmi4_data, reset);
	if (retval > 0)
		pr_debug("waited for idle %dms\n", retval);
	else
		dev_warn(&rmi4_data->i2c_client->dev, "%s: timed out waiting for idle\n",
			__func__);

	/* read touch IC state */
	synaptics_dsx_sensor_ready_state(rmi4_data, false);
	/* transition to active state is completed - allow flashing */
	rmi4_data->flash_enabled = true;

	if (rmi4_data->purge_enabled) {
		int value = 0; /* clear flag */
		atomic_set(&rmi4_data->panel_off_flag, value);
		pr_debug("touches purge is %s\n", value ? "ON" : "OFF");
	}

	synaptics_dsx_resumeinfo_finish(rmi4_data);
	return 0;
}

static const struct dev_pm_ops synaptics_rmi4_dev_pm_ops = {
	.suspend = synaptics_rmi4_suspend,
	.resume  = synaptics_rmi4_resume,
};

#ifdef CONFIG_OF
static const struct of_device_id synaptics_rmi4_match_tbl[] = {
	{ .compatible = "synaptics," DRIVER_NAME },
	{ },
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_match_tbl);
#endif

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, synaptics_rmi4_id_table);

static struct i2c_driver synaptics_rmi4_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = synaptics_rmi4_remove,
	.id_table = synaptics_rmi4_id_table,
};

 /**
 * synaptics_rmi4_init()
 *
 * Called by the kernel during do_initcalls (if built-in)
 * or when the driver is loaded (if a module).
 *
 * This function registers the driver to the I2C subsystem.
 *
 */
static int __init synaptics_rmi4_init(void)
{
	return i2c_add_driver(&synaptics_rmi4_driver);
}

 /**
 * synaptics_rmi4_exit()
 *
 * Called by the kernel when the driver is unloaded.
 *
 * This funtion unregisters the driver from the I2C subsystem.
 *
 */
static void __exit synaptics_rmi4_exit(void)
{
	i2c_del_driver(&synaptics_rmi4_driver);
}

module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Touch Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SYNAPTICS_DSX_DRIVER_VERSION);
