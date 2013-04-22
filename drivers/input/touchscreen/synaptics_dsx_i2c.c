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
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#include <linux/input/synaptics_rmi_dsx.h>
#include "synaptics_dsx_i2c.h"
#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#define DRIVER_NAME "synaptics_dsx_i2c"
#define INPUT_PHYS_NAME "synaptics_dsx_i2c/input0"

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
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12
#define F12_STD_QUERY_LEN 10
#define F12_STD_CTRL_LEN 4
#define F12_STD_DATA_LEN 80

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)

#define ONE_TOUCH_RECALIBRATION 49
#define ONE_TOUCH_SUPPRESSION 5
#define MULTI_TOUCH_SUPPRESSION 1

#define X_1T_SUPPRESSION ONE_TOUCH_SUPPRESSION
#define Y_1T_SUPPRESSION ONE_TOUCH_SUPPRESSION
#define X_MT_SUPPRESSION MULTI_TOUCH_SUPPRESSION
#define Y_MT_SUPPRESSION MULTI_TOUCH_SUPPRESSION

#define TYPE_FINGER (1 << 0)
#define TYPE_STYLUS (1 << 1)

#define RESET_GPIO_NAME "touch_reset"

#define SYDBG(fmt, args...)	printk(KERN_ERR "%s: " fmt, __func__, ##args)
#define SYDBG_REG(subpkt, fld) SYDBG(#subpkt "." #fld " = 0x%02X\n", subpkt.fld)

/* F12 packet register description */

struct {
	unsigned char max_x_lsb;
	unsigned char max_x_msb;
	unsigned char max_y_lsb;
	unsigned char max_y_msb;
} f12_c08_0;

struct {
	unsigned char recv_pitch_lsb;
	unsigned char recv_pitch_msb;
	unsigned char trans_pitch_lsb;
	unsigned char trans_pitch_msb;
} f12_c08_1;

struct {
	unsigned char low_recv_clip;
	unsigned char high_recv_clip;
	unsigned char low_trans_clip;
	unsigned char high_trans_clip;
} f12_c08_2;

struct {
	unsigned char num_2d_recv;
	unsigned char num_2d_trans;
} f12_c08_3;

struct {
	unsigned char touch_threshold;
	unsigned char lift_hysteresis;
	unsigned char sm_z_scale_factor_lsb;
	unsigned char sm_z_scale_factor_msb;
	unsigned char lg_z_scale_factor_lsb;
	unsigned char lg_z_scale_factor_msb;
	unsigned char sm_lg_boundary;
} f12_c09_0;

struct {
	unsigned char wx_scale;
	unsigned char wx_offset;
	unsigned char wy_scale;
	unsigned char wy_offset;
} f12_c09_1;

struct {
	unsigned char x_size_lsb;
	unsigned char x_size_msb;
	unsigned char y_size_lsb;
	unsigned char y_size_msb;
} f12_c09_2;

struct {
	unsigned char noise_floor;
	unsigned char min_peak_amplitude;
	unsigned char peak_merge_threshold;
} f12_c10_0;

struct {
	unsigned char drumming_accel_threshold;
	unsigned char drumming_min_distance;
} f12_c10_1;

struct {
	unsigned char sm_corner;
	unsigned char lg_corner;
} f12_c11_0;

struct {
	unsigned char x_min_z;
	unsigned char y_min_z;
	unsigned char x_max_z;
	unsigned char y_max_z;
	unsigned char x_correction_amplitude;
	unsigned char y_correction_amplitude;
} f12_c12_0;

struct {
	unsigned char finger_amplitude_threshold;
	unsigned char sm_finger_amplitude_threshold;
	unsigned char sm_finger_border_size;
	unsigned char negative_finger_amplitude_threshold;
} f12_c15_0;

struct {
	unsigned char palm_amplitude_threshold;
	unsigned char palm_area;
} f12_c15_1;

struct f12_c20_0_type {
	unsigned char x_suppression;
	unsigned char y_suppression;
} f12_c20_0;

struct {
	union {
		struct {
			unsigned char report_always:1;
			unsigned char reserved:7;
		} __packed;
		unsigned char data[1];
	};
} f12_c20_1;

struct {
	union {
		struct {
			unsigned char palm_filter_mode:2;
			unsigned char accept_stylus:1;
			unsigned char reserved:5;
		} __packed;
		unsigned char data[1];
	};
} f12_c22_0;

struct f12_c23_0_type {
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

struct f12_c23_1_type {
	unsigned char max_num_reported_objects;
} f12_c23_1;

struct {
	unsigned char reported_bytes_per_object;
} f12_c28_0;

struct synaptics_rmi4_subpkt f12_c08[] = {
	RMI4_SUBPKT(f12_c08_0),
	RMI4_SUBPKT(f12_c08_1),
	RMI4_SUBPKT(f12_c08_2),
	RMI4_SUBPKT(f12_c08_3),
};

struct synaptics_rmi4_subpkt f12_c09[] = {
	RMI4_SUBPKT(f12_c09_0),
	RMI4_SUBPKT(f12_c09_1),
	RMI4_SUBPKT(f12_c09_2),
};

struct synaptics_rmi4_subpkt f12_c10[] = {
	RMI4_SUBPKT(f12_c10_0),
	RMI4_SUBPKT(f12_c10_1),
};

struct synaptics_rmi4_subpkt f12_c11[] = {
	RMI4_SUBPKT(f12_c11_0),
};

struct synaptics_rmi4_subpkt f12_c12[] = {
	RMI4_SUBPKT(f12_c12_0),
};

struct synaptics_rmi4_subpkt f12_c15[] = {
	RMI4_SUBPKT(f12_c15_0),
	RMI4_SUBPKT(f12_c15_1),
};

struct synaptics_rmi4_subpkt f12_c20[] = {
	RMI4_SUBPKT(f12_c20_0),
	RMI4_SUBPKT(f12_c20_1),
};

struct synaptics_rmi4_subpkt f12_c22[] = {
	RMI4_SUBPKT(f12_c22_0),
};

struct synaptics_rmi4_subpkt f12_c23[] = {
	RMI4_SUBPKT(f12_c23_0),
	RMI4_SUBPKT(f12_c23_1),
};

struct synaptics_rmi4_subpkt f12_c28[] = {
	RMI4_SUBPKT(f12_c28_0),
};

struct synaptics_rmi4_packet_reg f12_ctrl_reg_array[] = {
	RMI4_NO_REG(),		/*00*/
	RMI4_NO_REG(),		/*01*/
	RMI4_NO_REG(),		/*02*/
	RMI4_NO_REG(),		/*03*/
	RMI4_NO_REG(),		/*04*/
	RMI4_NO_REG(),		/*05*/
	RMI4_NO_REG(),		/*06*/
	RMI4_NO_REG(),		/*07*/
	RMI4_REG(f12_c08),	/*08*/
	RMI4_NO_REG(),		/*09*/
	RMI4_NO_REG(),		/*10*/
	RMI4_NO_REG(),		/*11*/
	RMI4_NO_REG(),		/*12*/
	RMI4_NO_REG(),		/*13*/
	RMI4_NO_REG(),		/*14*/
	RMI4_NO_REG(),		/*15*/
	RMI4_NO_REG(),		/*16*/
	RMI4_NO_REG(),		/*17*/
	RMI4_NO_REG(),		/*18*/
	RMI4_NO_REG(),		/*19*/
	RMI4_REG(f12_c20),	/*20*/
	RMI4_NO_REG(),		/*21*/
	RMI4_NO_REG(),		/*22*/
	RMI4_REG(f12_c23),	/*23*/
	RMI4_NO_REG(),		/*24*/
	RMI4_NO_REG(),		/*25*/
	RMI4_NO_REG(),		/*26*/
	RMI4_NO_REG(),		/*27*/
	RMI4_REG(f12_c28),	/*28*/
};

static struct synaptics_rmi4_func_packet_regs f12_ctrl_regs = {
	.base_addr = 0,
	.nr_regs = ARRAY_SIZE(f12_ctrl_reg_array),
	.regs = f12_ctrl_reg_array
};

struct synaptics_dsx_hob {
	struct f12_c20_0_type f12_c20_0;
	struct f12_c23_0_type f12_c23_0;
	struct f12_c23_1_type f12_c23_1;
	unsigned char f01_c9;
};

static struct synaptics_dsx_hob hob_data;
static unsigned char tsb_buff_clean_flag = 1;

#define LAST_SUBPACKET_ROW_IND_MASK 0x80
#define NR_SUBPKT_PRESENCE_BITS 7

int synaptics_rmi4_scan_packet_reg_info(
	struct synaptics_rmi4_data *rmi4_data,
	unsigned short query_addr,
	unsigned short regs_base_addr,
	struct synaptics_rmi4_func_packet_regs *regs)
{
	unsigned char sz, mask;
	int ii, jj, r, s, retval;
	unsigned short r_offset;
	unsigned short addr = query_addr;
	unsigned char data[255];

	for (r = 0; r < regs->nr_regs; ++r) {
		regs->regs[r].offset = -1;
		regs->regs[r].size = 0;
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
	pr_debug("size of reg presence = %d\n",	sz);
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
			struct synaptics_rmi4_packet_reg *reg = regs->regs + r;
			int present = (data[ii] & mask) != 0;
			int allocated = r < regs->nr_regs;
			int expected = allocated && reg->expected;
			if (!present || !expected) {
				if (allocated && present)
					reg->offset = r_offset++;
				if (present != expected)
					pr_debug("  reg: r%d"
						" is%s present, but was%s"
						" expected\n", r,
						present ? "" : " NOT",
						expected ? "" : " NOT");
				continue;
			}
			pr_debug("  r%d offset = %d\n", r, r_offset);
			reg->offset = r_offset++;
		}
	}

	/* Scan register size and subpacket presence*/
	sz = data[0];
	pr_debug("subpacket presence sz = %d\n", sz);
	retval = rmi4_data->i2c_read(rmi4_data, ++addr, data, sz);
	if (retval < 0)
		return retval;
	for (r = 0, ii = 0; r < regs->nr_regs && ii < sz; ++r) {
		unsigned int expected_reg_size;
		struct synaptics_rmi4_packet_reg *reg = regs->regs + r;
		if (reg->offset == -1)
			continue;
		reg->size = data[ii++];
		pr_debug("r%d sz = %d\n", r, reg->size);
		if (!reg->size) {
			pr_err("packet register size greater 255 bytes"
				" not supported\n");
			return -ENOSYS;
		}
		expected_reg_size = 0;
		for (s = 0; ii < sz;) {
			pr_debug("  subpkt presence [%d] = 0x%02x\n",
				ii, data[ii]);
			for (jj = 0, mask = 1; jj < NR_SUBPKT_PRESENCE_BITS;
					++jj, ++s, mask <<= 1) {
				struct synaptics_rmi4_subpkt *subpkt =
							reg->subpkt + s;
				int present = (data[ii] & mask) != 0;
				int expected = (s < reg->nr_subpkts) &&
					subpkt->expected;
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
				subpkt->present = 1;
				expected_reg_size += subpkt->size;
			}
			if ((data[ii++] & LAST_SUBPACKET_ROW_IND_MASK) == 0)
				break;
		}
		if (reg->expected && reg->size != expected_reg_size) {
			pr_debug("  r%d size error:"
				" expected %d actual is %d\n",
				r, expected_reg_size, reg->size);
		}
	}
	return 0;
}

int synaptics_rmi4_read_packet_reg(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs, unsigned char r)
{
	int s, retval, offset;
	static unsigned char data[255];
	struct synaptics_rmi4_packet_reg *reg = regs->regs + r;

	if (r >= regs->nr_regs || !reg->size)
		return -EINVAL;

	if (reg->offset == -1) {
		pr_err("touch register error: can't read r%d - not present\n",
			r);
		return -ENOENT;
	}

	retval = rmi4_data->i2c_read(
			rmi4_data,
			regs->base_addr + reg->offset,
			data,
			reg->size);

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

		memcpy(subpkt->data, data+offset, subpkt->size);
		offset += subpkt->size;
#if defined(CONFIG_DYNAMIC_DEBUG) || defined(DEBUG)
		{
			int kk;
			pr_debug("read r%d.s%d =\n", r, s);
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
		if (regs->regs[r].expected && regs->regs[r].offset >= 0) {
			retval = synaptics_rmi4_read_packet_reg(
				rmi4_data, regs, r);
			if (retval < 0)
				break;
		}
	}
	return retval;
}

static int synaptics_rmi4_write_packet_reg(
	struct synaptics_rmi4_data *rmi4_data,
	struct synaptics_rmi4_func_packet_regs *regs, unsigned char r)
{
	unsigned char data[255];
	struct synaptics_rmi4_packet_reg *reg = regs->regs + r;
	int sz, ii, offset, retval;

	if (r >= regs->nr_regs || !reg->size)
		return -EINVAL;
	if (reg->offset == -1) {
		pr_err("touch register error: writing to absent register r%d\n",
			r);
		return -ENOENT;
	}

	for (ii = 0, sz = 0, offset = 0; ii < reg->nr_subpkts; ++ii) {
		struct synaptics_rmi4_subpkt *subpkt = reg->subpkt + ii;
		if (!subpkt->present)
			continue;

		if (subpkt->data && subpkt->size &&
			(offset + subpkt->size) < sizeof(data)) {
			if ((reg->size - offset) >= subpkt->size) {
				memcpy(data + sz, subpkt->data, subpkt->size);
				sz += subpkt->size;
			} else {
				pr_err("expected %d bytes, only %d present\n",
					offset + subpkt->size, reg->size);
				break;
			}
		} else {
			retval = -EINVAL;
			pr_err("bad subpacket or register greater %d bytes\n",
					sizeof(data));
			goto out;
		}
	}
	retval = rmi4_data->i2c_write(rmi4_data, regs->base_addr + reg->offset,
		data, sz);
out:
	return retval;
}

#ifdef CONFIG_OF
static struct synaptics_dsx_platform_data *
		synaptics_dsx_of_init(struct i2c_client *client,
				struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned key_codes[SYN_MAX_BUTTONS];
	struct synaptics_dsx_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	struct synaptics_dsx_cap_button_map *button_map = NULL;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	pdata->irq_gpio = of_get_gpio(np, 0);
	pdata->reset_gpio = of_get_gpio(np, 1);

	if (gpio_request(pdata->reset_gpio, RESET_GPIO_NAME) < 0)
		pr_err("failed to request reset gpio\n");

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

	pdata->x_flip = pdata->y_flip = false;
	pdata->irq_flags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	pdata->cap_button_map = button_map;

	if (of_property_read_bool(np, "synaptics,purge-enabled")) {
		pr_notice("using purge\n");
		rmi4_data->purge_enabled = true;
	}

	if (of_property_read_bool(np, "synaptics,shared-regulator")) {
		pr_notice("using shared regulator\n");
		rmi4_data->shared_regulator = true;
	}

	if (of_property_read_bool(np, "synaptics,reset-on-resume")) {
		pr_notice("using hard reset on resume\n");
		rmi4_data->reset_on_resume = true;
	}

	if (of_property_read_bool(np, "synaptics,one-touch-enabled")) {
		pr_notice("using one touch while suspended\n");
		rmi4_data->one_touch_enabled = true;
	}

	if (!of_property_read_bool(np, "synaptics,normal-mode")) {
		pr_notice("using normal mode settings from tdat\n");
		rmi4_data->normal_mode = -1;
	} else {
		of_property_read_u32(np, "synaptics,normal-mode",
					&rmi4_data->normal_mode);
		pr_notice("%s mode\n", rmi4_data->normal_mode ?
				"using normal" : "enforce no sleep");
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

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data,
		unsigned short length);

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data);

static void synaptics_rmi4_sensor_sleep(struct synaptics_rmi4_data *rmi4_data);

static void synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data);

static void synaptics_rmi4_sensor_one_touch(
		struct synaptics_rmi4_data *rmi4_data, bool enable);

static void synaptics_rmi4_sensor_multi_touch(
		struct synaptics_rmi4_data *rmi4_data, unsigned char function);

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable);

static void synaptics_dsx_sensor_state(struct synaptics_rmi4_data *rmi4_data,
		int state);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h);

static void synaptics_rmi4_late_resume(struct early_suspend *h);

static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);
#endif

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

static ssize_t synaptics_rmi4_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_hw_irqstat_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_ic_ver_show(struct device *dev,
		struct device_attribute *attr, char *buf);

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
};

static bool exp_fn_inited;
static struct mutex exp_fn_list_mutex;
static struct list_head exp_fn_list;
static struct semaphore reset_semaphore;

static irqreturn_t synaptics_dsx_reset_irq(int irq, void *data)
{
	struct semaphore *sema = data;
	up(sema);
	return IRQ_HANDLED;
}

static int synaptics_dsx_ic_reset(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned long start = jiffies;
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	sema_init(&reset_semaphore, 0);
	retval = request_irq(rmi4_data->irq, synaptics_dsx_reset_irq,
			IRQF_TRIGGER_RISING, "synaptics_reset",
			&reset_semaphore);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to request irq: %d\n",
				__func__, retval);
		return retval;
	}

	gpio_set_value(platform_data->reset_gpio, 0);
	udelay(1500);
	gpio_set_value(platform_data->reset_gpio, 1);

	retval = down_timeout(&reset_semaphore, msecs_to_jiffies(50));
	free_irq(rmi4_data->irq, &reset_semaphore);

	if (retval) {
		dev_err(&rmi4_data->i2c_client->dev,
				"timed out waiting for reset to complete\n");
		return -ETIMEDOUT;
	}

	/* delay extra 0.5 ms to ensure 1st i2c bus accessing works correctly */
	udelay(500);
	pr_debug("successful reset took %ums\n",
				jiffies_to_msecs(jiffies-start));
	return 0;
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
	input_set_drvdata(rmi4_data->input_dev, rmi4_data);

	pr_debug("allocated input device\n");

	return 0;
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

	if (ui_mode)
		state = standby ? STATE_STANDBY : STATE_ACTIVE;
	else
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
		synaptics_rmi4_irq_enable(rmi4_data, false);

		if (rmi4_data->one_touch_enabled)
			synaptics_rmi4_sensor_one_touch(rmi4_data, true);

		if (!rmi4_data->sensor_sleep) {
			synaptics_rmi4_sensor_sleep(rmi4_data);
			rmi4_data->touch_stopped = true;
		}
			break;

	case STATE_ACTIVE:
		if (rmi4_data->sensor_sleep) {
			synaptics_rmi4_sensor_wake(rmi4_data);
			rmi4_data->touch_stopped = false;
		}

		if (rmi4_data->one_touch_enabled)
			synaptics_rmi4_sensor_one_touch(rmi4_data, false);

		synaptics_rmi4_irq_enable(rmi4_data, true);
			break;

	case STATE_STANDBY:
		synaptics_rmi4_irq_enable(rmi4_data, false);
		if (!rmi4_data->sensor_sleep) {
			synaptics_rmi4_sensor_sleep(rmi4_data);
			rmi4_data->touch_stopped = true;
		}
			break;

	case STATE_BL:
	case STATE_INIT:
		synaptics_rmi4_irq_enable(rmi4_data, false);
		if (rmi4_data->sensor_sleep) {
			synaptics_rmi4_sensor_wake(rmi4_data);
			rmi4_data->touch_stopped = false;
		}
			break;
	}

	pr_info("state change %s -> %s\n",
			synaptics_dsx_state_name(rmi4_data->state),
			synaptics_dsx_state_name(state));

	synaptics_dsx_set_state_safe(rmi4_data, state);
}

static const char * const panel_event_names[] = {
	"PANEL PWR-OFF",
	"PANEL PWR-ON",
	"DISPLAY ON"
};

static const char *synaptics_dsx_get_panel_event(int event)
{
	return panel_event_names[event];
}

static int synaptics_dsx_panel_cb(struct notifier_block *nb,
		unsigned long event, void *ignore)
{
	int value = -1;
	struct synaptics_rmi4_data *rmi4_data =
		container_of(nb, struct synaptics_rmi4_data, panel_nb);

	switch (event) {
	case MMI_PANEL_EVENT_PWR_OFF:
		if (rmi4_data->shared_regulator)
			synaptics_rmi4_early_suspend(&rmi4_data->early_suspend);
		value = 1; /* set flag */
			break;
	case MMI_PANEL_EVENT_POST_INIT:
		value = 0; /* clear flag */
		synaptics_dsx_sensor_ready_state(rmi4_data, false);
			break;
	case MMI_PANEL_EVENT_PWR_ON:
		if (rmi4_data->shared_regulator)
			synaptics_rmi4_late_resume(&rmi4_data->early_suspend);
			break;
	default:
			return -EINVAL;
	}

	pr_info("%s\n", synaptics_dsx_get_panel_event(event));

	if (rmi4_data->purge_enabled && value != -1) {
		atomic_set(&rmi4_data->panel_off_flag, value);
		pr_debug("touches purge is %s\n", value ? "ON" : "OFF");
	}

	return 0;
}

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data);
	if (retval < 0) {
		dev_err(dev,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			rmi4_data->rmi4_mod_info.product_id_string);
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int firmware_id;
	unsigned int config_id;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	batohui(&firmware_id, rmi->build_id, sizeof(rmi->build_id));
	batohui(&config_id, rmi->config_id, sizeof(rmi->config_id));

	return scnprintf(buf, PAGE_SIZE, "%x-%08x\n", firmware_id, config_id);
}

static ssize_t synaptics_rmi4_resume_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	int i;
	int c_res;
	int offset = 0;

	c_res = rmi4_data->last_resume;
	/* Resume buffer not allocated or there were no resumes yet */
	if (rmi4_data->number_resumes <= 0 || c_res < 0)
		return scnprintf(buf, PAGE_SIZE,
				"No resume information found.\n");

	offset += scnprintf(buf + offset, PAGE_SIZE - offset,
		"Count\tStart\t\tFinish\t# no-events\t"
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

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

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
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

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

static ssize_t synaptics_rmi4_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			rmi4_data->irq_enabled ? "ENABLED" : "DISABLED");
}

static ssize_t synaptics_rmi4_drv_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	unsigned long value = 0;
	int err = 0;

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
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

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
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

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
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;
	rmi = &(rmi4_data->rmi4_mod_info);
	batohui(&build_id, rmi->build_id, sizeof(rmi->build_id));
	batohui(&config_id, rmi->config_id, sizeof(rmi->config_id));
	return scnprintf(buf, PAGE_SIZE,
			"%s%s\n%s%x\n%s%x\n",
			"Product ID: ", rmi->product_id_string,
			"Build ID: ", build_id,
			"Config ID: ", config_id);
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
	struct synaptics_rmi4_resume_info *tmp_resume_i = NULL;
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char index = 0;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char finger_data[F12_STD_DATA_LEN];
	unsigned short data_addr;
	unsigned short data_size;
	int x;
	int y;
	int p;
	int w;
	int id;

	fingers_supported = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	data_size = fingers_supported * fhandler->size_of_data_register_block;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			data_addr,
			finger_data,
			data_size);
	if (retval < 0)
		return 0;

	if (rmi4_data->number_resumes > 0) {
		tmp_resume_i =
			&(rmi4_data->resume_info[rmi4_data->last_resume]);

		if (atomic_read(&rmi4_data->panel_off_flag)) {
			tmp_resume_i->ignored_events++;
			return 0;
		}
		if (tmp_resume_i->purge_off.tv_sec == 0)
			getnstimeofday(&(tmp_resume_i->purge_off));
	} else {
		if (atomic_read(&rmi4_data->panel_off_flag)) {
			return 0;
		}
	}

	for (finger = 0; finger < fingers_supported; finger++,
			 index += fhandler->size_of_data_register_block) {
		if (finger_data[index] == 0)
			continue;

		x = finger_data[index+1] | (finger_data[index+2] << 8);
		y = finger_data[index+3] | (finger_data[index+4] << 8);
		p = finger_data[index+5];
		w = finger_data[index+5];
		id = finger;

		if (rmi4_data->board->x_flip)
			x = rmi4_data->sensor_max_x - x;
		if (rmi4_data->board->y_flip)
			y = rmi4_data->sensor_max_y - y;

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
		input_report_abs(rmi4_data->input_dev,
					ABS_MT_TRACKING_ID, id);
		input_mt_sync(rmi4_data->input_dev);
		touch_count++;

		if (rmi4_data->number_resumes > 0 &&
			tmp_resume_i->send_touch.tv_sec == 0)
			getnstimeofday(&(tmp_resume_i->send_touch));
	}

	if (!touch_count)
		input_mt_sync(rmi4_data->input_dev);

	input_sync(rmi4_data->input_dev);

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
	if (retval < 0)
		return 0;

	if (atomic_read(&rmi4_data->panel_off_flag))
		return 0;

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

			if (rmi4_data->board->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->board->y_flip)
				y = rmi4_data->sensor_max_y - y;

			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Finger %d:\n"
					"status = 0x%02x\n"
					"x = %d\n"
					"y = %d\n"
					"wx = %d\n"
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
			input_mt_sync(rmi4_data->input_dev);
			touch_count++;
		}
	}

	if (!touch_count)
		input_mt_sync(rmi4_data->input_dev);

	input_sync(rmi4_data->input_dev);

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

	if (atomic_read(&rmi4_data->panel_off_flag))
		return;

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
		touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
				fhandler);

		*touch_count += touch_count_2d;

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;

	case SYNAPTICS_RMI4_F12:
		touch_count_2d = synaptics_rmi4_f12_abs_report(rmi4_data,
				fhandler);

		*touch_count += touch_count_2d;

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;

	case SYNAPTICS_RMI4_F1A:
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

	mutex_lock(&exp_fn_list_mutex);
	if (!list_empty(&exp_fn_list)) {
		list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
			if (exp_fhandler->inserted &&
					(exp_fhandler->func_attn != NULL))
				exp_fhandler->func_attn(rmi4_data, intr[0]);
		}
	}
	mutex_unlock(&exp_fn_list_mutex);

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
	struct synaptics_rmi4_resume_info *tmp_resume_i;

	if (rmi4_data->number_resumes > 0) {
		tmp_resume_i =
			&(rmi4_data->resume_info[rmi4_data->last_resume]);
		if (tmp_resume_i->isr.tv_sec == 0)
			getnstimeofday(&(tmp_resume_i->isr));
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
#ifdef REPORT_2D_W
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			MAX_ABS_MT_TOUCH_MAJOR, 0, 0);
#endif

	input_set_events_per_packet(rmi4_data->input_dev, 64);

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

	fhandler->fn_number = fd->fn_number;
	fhandler->num_of_data_sources = (fd->intr_src_count  & MASK_3BIT);

	pr_debug("scan F12 ctrl registers\n");
	retval = synaptics_rmi4_scan_packet_reg_info(
			rmi4_data,
			fhandler->full_addr.query_base + 4,
			fhandler->full_addr.ctrl_base,
			&f12_ctrl_regs);
	if (retval < 0)
		return retval;

	retval = synaptics_rmi4_read_packet_regs(rmi4_data, &f12_ctrl_regs);
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
	/* FIXME replace hard coded value with querying */
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_PRESSURE, 0,
			255, 0, 0);
#ifdef REPORT_2D_W
	/* FIXME replace hard coded value with querying */
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TOUCH_MAJOR, 0,
			255, 0, 0);
#endif
	input_set_abs_params(rmi4_data->input_dev,
			ABS_MT_TRACKING_ID, 0,
			rmi4_data->num_of_fingers - 1, 0, 0);

	/* FIXME replace hard coded value with querying */
	input_set_events_per_packet(rmi4_data->input_dev, 64);

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
	unsigned char ii;
	unsigned char page_number;
	unsigned char intr_count = 0;
	unsigned char data_sources = 0;
	unsigned char f01_query[F01_STD_QUERY_LEN];
	unsigned short pdt_entry_addr;
	unsigned short intr_addr;
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

				synaptics_rmi4_sensor_multi_touch(
						rmi4_data, SYNAPTICS_RMI4_F01);

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
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
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
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
					return retval;
				}

				retval = synaptics_rmi4_f12_init(rmi4_data,
						fhandler, &rmi_fd, intr_count);
				if (retval < 0)
					return retval;

				synaptics_rmi4_sensor_multi_touch(
						rmi4_data, SYNAPTICS_RMI4_F12);
				break;

			case SYNAPTICS_RMI4_F1A:
				if (rmi_fd.intr_src_count == 0)
					break;

				retval = synaptics_rmi4_alloc_fh(&fhandler,
						&rmi_fd, page_number);
				if (retval < 0) {
					dev_err(&rmi4_data->i2c_client->dev,
							"%s: Failed to alloc for F%d\n",
							__func__,
							rmi_fd.fn_number);
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
				"%s: Non-Synaptics device found, manufacturer ID = %d\n",
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

	/* Enable the interrupt sources */
	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (rmi4_data->intr_mask[ii] != 0x00) {
			dev_dbg(&rmi4_data->i2c_client->dev,
					"%s: Interrupt enable mask %d = 0x%02x\n",
					__func__, ii, rmi4_data->intr_mask[ii]);
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			retval = synaptics_rmi4_i2c_write(rmi4_data,
					intr_addr,
					&(rmi4_data->intr_mask[ii]),
					sizeof(rmi4_data->intr_mask[ii]));
			if (retval < 0)
				return retval;
		}
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

static void synaptics_dsx_on_resume(struct synaptics_rmi4_data *rmi4_data)
{
	/*
	 * Enforce touch release event report to work-around such event
	 * missing while touch IC is off.
	 */
	input_mt_sync(rmi4_data->input_dev);
	input_sync(rmi4_data->input_dev);

	/* reset some TSB global vars like fingers_on_2d after resume
	 * of reset touch IC
	 */
	if (rmi4_data->button_0d_enabled) {
		tsb_buff_clean_flag = 1;
		rmi4_data->fingers_on_2d = false;
	}
}

static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data)
{
	int current_state, retval;
	bool need_to_query = false;
	unsigned char command = 0x01;

	synaptics_dsx_on_resume(rmi4_data);

	current_state = synaptics_dsx_get_state_safe(rmi4_data);
	if (current_state == STATE_UNKNOWN) {
		synaptics_rmi4_cleanup(rmi4_data);

		if (rmi4_data->input_registered) {
			input_unregister_device(rmi4_data->input_dev);
			rmi4_data->input_dev = NULL;
			rmi4_data->input_registered = false;

			pr_debug("de-allocated input device\n");
		}
		need_to_query = true;
	}

	if (rmi4_data->reset_on_resume)
		/* do hard reset instead of soft */
		synaptics_dsx_ic_reset(rmi4_data);
	else {
		retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_cmd_base_addr,
			&command,
			sizeof(command));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
			return retval;
		}
		msleep(100);
	}

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
	}

	return 0;
}

/**
* synaptics_rmi4_detection_work()
*
* Called by the kernel at the scheduled time.
*
* This function is a self-rearming work thread that checks for the
* insertion and removal of other expansion Function modules such as
* rmi_dev and calls their initialization and removal callback functions
* accordingly.
*/
static void synaptics_rmi4_detection_work(struct work_struct *work)
{
	struct synaptics_rmi4_exp_fn *exp_fhandler, *next_list_entry;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(work, struct synaptics_rmi4_data,
			det_work.work);

	queue_delayed_work(rmi4_data->det_workqueue,
			&rmi4_data->det_work,
			msecs_to_jiffies(EXP_FN_DET_INTERVAL));

	mutex_lock(&exp_fn_list_mutex);
	if (!list_empty(&exp_fn_list)) {
		list_for_each_entry_safe(exp_fhandler,
				next_list_entry,
				&exp_fn_list,
				link) {
			if ((exp_fhandler->func_init != NULL) &&
					(exp_fhandler->inserted == false)) {
				synaptics_dsx_sensor_ready_state(
							rmi4_data, false);
				exp_fhandler->func_init(rmi4_data);
				synaptics_dsx_sensor_ready_state(
							rmi4_data, true);
				exp_fhandler->inserted = true;
			} else if ((exp_fhandler->func_init == NULL) &&
					(exp_fhandler->inserted == true)) {
				exp_fhandler->func_remove(rmi4_data);
				list_del(&exp_fhandler->link);
				kfree(exp_fhandler);
			}
		}
	}
	mutex_unlock(&exp_fn_list_mutex);

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
		unsigned char intr_mask))
{
	struct synaptics_rmi4_exp_fn *exp_fhandler;

	if (!exp_fn_inited) {
		mutex_init(&exp_fn_list_mutex);
		INIT_LIST_HEAD(&exp_fn_list);
		exp_fn_inited = 1;
	}

	mutex_lock(&exp_fn_list_mutex);
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
		list_add_tail(&exp_fhandler->link, &exp_fn_list);
	} else {
		list_for_each_entry(exp_fhandler, &exp_fn_list, link) {
			if (exp_fhandler->func_init == func_init) {
				exp_fhandler->inserted = false;
				exp_fhandler->func_init = NULL;
				exp_fhandler->func_attn = NULL;
				goto exit;
			}
		}
	}

exit:
	mutex_unlock(&exp_fn_list_mutex);

	return;
}
EXPORT_SYMBOL(synaptics_rmi4_new_function);

 /**
 * synaptics_rmi4_probe()
 *
 * Called by the kernel when an association with an I2C device of the
 * same name is made (after doing i2c_add_driver).
 *
 * This funtion allocates and initializes the resources for the driver
 * as an input driver, turns on the power to the sensor, queries the
 * sensor for its supported Functions and characteristics, registers
 * the driver to the input subsystem, sets up the interrupt, handles
 * the registration of the early_suspend and late_resume functions,
 * and creates a work queue for detection of other expansion Function
 * modules.
 */
static int __devinit synaptics_rmi4_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;
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
	if (!rmi4_data) {
		dev_err(&client->dev,
				"%s: Failed to alloc mem for rmi4_data\n",
				__func__);
		return -ENOMEM;
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

	rmi = &(rmi4_data->rmi4_mod_info);

	rmi4_data->i2c_client = client;
	rmi4_data->current_page = MASK_8BIT;
	rmi4_data->board = platform_data;
	rmi4_data->touch_stopped = false;
	rmi4_data->sensor_sleep = false;
	rmi4_data->irq_enabled = false;

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
	mutex_init(&(rmi4_data->rmi4_io_ctrl_mutex));
	mutex_init(&(rmi4_data->state_mutex));

	if (platform_data->gpio_config) {
		retval = platform_data->gpio_config(
				platform_data->irq_gpio, true);
		if (retval < 0) {
			dev_err(&client->dev,
					"%s: Failed to configure GPIO\n",
					__func__);
			goto err_input_device;
		}
	}

	/* get irq number initialized before calling reset */
	rmi4_data->irq = gpio_to_irq(platform_data->irq_gpio);

	i2c_set_clientdata(client, rmi4_data);

	synaptics_dsx_ic_reset(rmi4_data);

	retval = synaptics_dsx_alloc_input(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to allocate input device\n",
				__func__);
		goto err_input_device;
	}

	retval = synaptics_rmi4_query_device(rmi4_data);
	if (retval < 0) {
		dev_err(&client->dev,
				"%s: Failed to query device\n",
				__func__);
		goto err_query_device;
	}

	rmi4_data->regulator = regulator_get(&client->dev, "touch_vdd");
	if (IS_ERR(rmi4_data->regulator)) {
		dev_err(&client->dev,
				"%s: Failed to get regulator\n",
				__func__);
	} else {
		platform_data->regulator_en = true;
		regulator_enable(rmi4_data->regulator);
	}

	init_waitqueue_head(&rmi4_data->wait);

#ifdef CONFIG_HAS_EARLYSUSPEND
	rmi4_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	rmi4_data->early_suspend.suspend = synaptics_rmi4_early_suspend;
	rmi4_data->early_suspend.resume = synaptics_rmi4_late_resume;
	if (!rmi4_data->shared_regulator)
		register_early_suspend(&rmi4_data->early_suspend);
#endif

	if (!exp_fn_inited) {
		mutex_init(&exp_fn_list_mutex);
		INIT_LIST_HEAD(&exp_fn_list);
		exp_fn_inited = 1;
	}

	rmi4_data->det_workqueue =
			create_singlethread_workqueue("rmi_det_workqueue");
	INIT_DELAYED_WORK(&rmi4_data->det_work,
			synaptics_rmi4_detection_work);
	queue_delayed_work(rmi4_data->det_workqueue,
			&rmi4_data->det_work,
			msecs_to_jiffies(EXP_FN_DET_INTERVAL));

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

	rmi4_data->panel_nb.notifier_call = synaptics_dsx_panel_cb;
	mmi_panel_register_notifier(&rmi4_data->panel_nb);
	pr_debug("register panel notifier\n");

	if (rmi4_data->in_bootloader)
		pr_info("Product: %s is in bootloader mode\n",
			rmi->product_id_string);
	else {
		unsigned int config_id, firmware_id;

		batohui(&firmware_id, rmi->build_id, sizeof(rmi->build_id));
		batohui(&config_id, rmi->config_id, sizeof(config_id));
		pr_info("Product: %s, firmware id: %x, config id: %08x\n",
			rmi->product_id_string, firmware_id, config_id);
	}

	synaptics_dsx_sensor_ready_state(rmi4_data, true);

	return retval;

err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (!rmi4_data->shared_regulator)
		unregister_early_suspend(&rmi4_data->early_suspend);
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
static int __devexit synaptics_rmi4_remove(struct i2c_client *client)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = i2c_get_clientdata(client);
	struct synaptics_rmi4_device_info *rmi;
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	rmi = &(rmi4_data->rmi4_mod_info);

	cancel_delayed_work_sync(&rmi4_data->det_work);
	flush_workqueue(rmi4_data->det_workqueue);
	destroy_workqueue(rmi4_data->det_workqueue);

	rmi4_data->touch_stopped = true;
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

	synaptics_rmi4_cleanup(rmi4_data);
	kfree(rmi4_data);

	return 0;
}

static void synaptics_rmi4_sensor_multi_touch(
		struct synaptics_rmi4_data *rmi4_data, unsigned char function)
{
	int retval;

	switch (function) {
	case SYNAPTICS_RMI4_F12:
		/* Store multitouch config registers */
		hob_data.f12_c20_0 = f12_c20_0;
		hob_data.f12_c23_0 = f12_c23_0;
		hob_data.f12_c23_1 = f12_c23_1;

		if (!hob_data.f12_c20_0.x_suppression)
			hob_data.f12_c20_0.x_suppression = X_MT_SUPPRESSION;

		if (!hob_data.f12_c20_0.y_suppression)
			hob_data.f12_c20_0.y_suppression = Y_MT_SUPPRESSION;
		break;
	case SYNAPTICS_RMI4_F01:
		retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr + 5,
			&hob_data.f01_c9,
			sizeof(hob_data.f01_c9));
		if (retval < 0)
			pr_err("error storing recalibration interval\n");
		break;
	}
}

 /**
 * synaptics_rmi4_sensor_one_touch()
 *
 * Called on suspend and resume.
 *
 * This function reconfigures finger data acquisition.
 */
static void synaptics_rmi4_sensor_one_touch(
		struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	int retval;
	unsigned char recalibration;
	if (enable) {
		f12_c20_0.x_suppression = X_1T_SUPPRESSION;
		f12_c20_0.y_suppression = Y_1T_SUPPRESSION;
		f12_c23_0.data[0] = TYPE_FINGER | TYPE_STYLUS;
		f12_c23_1.max_num_reported_objects = 1;
		recalibration = ONE_TOUCH_RECALIBRATION;
	} else {
		f12_c20_0 = hob_data.f12_c20_0;
		f12_c23_0 = hob_data.f12_c23_0;
		f12_c23_1 = hob_data.f12_c23_1;
		recalibration = hob_data.f01_c9;
	}

	synaptics_rmi4_write_packet_reg(rmi4_data, &f12_ctrl_regs, 20);
	synaptics_rmi4_write_packet_reg(rmi4_data, &f12_ctrl_regs, 23);
	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr + 5,
			&recalibration,
			sizeof(recalibration));
	if (retval < 0)
		pr_err("error setting recalibration interval\n");
}

 /**
 * synaptics_rmi4_sensor_sleep()
 *
 * Called by synaptics_rmi4_early_suspend() and synaptics_rmi4_suspend().
 *
 * This function stops finger data acquisition and puts the sensor to sleep.
 */
static void synaptics_rmi4_sensor_sleep(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;
	unsigned char clear_mask = MASK_2BIT;
	unsigned char set_mask = SENSOR_SLEEP;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return;
	}

	if (rmi4_data->normal_mode >= 0) {
		clear_mask = MASK_3BIT;
		set_mask |= NO_SLEEP_OFF;
	}

	device_ctrl = (device_ctrl & ~clear_mask);
	device_ctrl = (device_ctrl | set_mask);

	dev_dbg(&(rmi4_data->input_dev->dev),
			"%s: Sleep mode 0x%x\n", __func__, device_ctrl);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to enter sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = false;
		return;
	} else {
		rmi4_data->sensor_sleep = true;
	}

	return;
}

 /**
 * synaptics_rmi4_sensor_wake()
 *
 * Called by synaptics_rmi4_resume() and synaptics_rmi4_late_resume().
 *
 * This function wakes the sensor from sleep.
 */
static void synaptics_rmi4_sensor_wake(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char device_ctrl;
	unsigned char clear_mask = MASK_3BIT;
	unsigned char set_mask = NORMAL_OPERATION;

	retval = synaptics_rmi4_i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return;
	}

	if (rmi4_data->normal_mode < 0)
		clear_mask = MASK_2BIT;
	else if (rmi4_data->normal_mode == 0)
		set_mask |= NO_SLEEP_ON;

	device_ctrl = (device_ctrl & ~clear_mask);
	device_ctrl = (device_ctrl | set_mask);

	dev_dbg(&(rmi4_data->input_dev->dev),
			"%s: Wake up to mode 0x%x\n", __func__, device_ctrl);

	retval = synaptics_rmi4_i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(&(rmi4_data->input_dev->dev),
				"%s: Failed to wake from sleep mode\n",
				__func__);
		rmi4_data->sensor_sleep = true;
		return;
	} else {
		rmi4_data->sensor_sleep = false;
	}

	return;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
 /**
 * synaptics_rmi4_early_suspend()
 *
 * Called by the kernel during the early suspend phase when the system
 * enters suspend.
 *
 * This function calls synaptics_rmi4_sensor_sleep() to stop finger
 * data acquisition and put the sensor to sleep.
 */
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);
	synaptics_rmi4_suspend(&(rmi4_data->input_dev->dev));
	return;
}

 /**
 * synaptics_rmi4_late_resume()
 *
 * Called by the kernel during the late resume phase when the system
 * wakes up from suspend.
 *
 * This function goes through the sensor wake process if the system wakes
 * up from early suspend (without going into suspend).
 */
static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	struct synaptics_rmi4_resume_info *tmp_resume_i;

	if (rmi4_data->number_resumes > 0) {
		rmi4_data->last_resume++;
		if (rmi4_data->last_resume >= rmi4_data->number_resumes)
			rmi4_data->last_resume = 0;
		tmp_resume_i =
			&(rmi4_data->resume_info[rmi4_data->last_resume]);
		getnstimeofday(&(tmp_resume_i->start));
		tmp_resume_i->ignored_events = 0;
		tmp_resume_i->isr.tv_sec = 0;
		tmp_resume_i->send_touch.tv_sec = 0;
		tmp_resume_i->purge_off.tv_sec = 0;

		synaptics_rmi4_resume(&(rmi4_data->input_dev->dev));

		getnstimeofday(&(tmp_resume_i->finish));
	} else
		synaptics_rmi4_resume(&(rmi4_data->input_dev->dev));
	return;
}
#endif

 /**
 * synaptics_rmi4_suspend()
 *
 * Called by the kernel during the suspend phase when the system
 * enters suspend.
 *
 * This function stops finger data acquisition and puts the sensor to
 * sleep (if not already done so during the early suspend phase),
 * disables the interrupt, and turns off the power to the sensor.
 */
static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	synaptics_dsx_sensor_state(rmi4_data, STATE_SUSPEND);

	if (platform_data->regulator_en)
		regulator_disable(rmi4_data->regulator);

	gpio_free(platform_data->reset_gpio);

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
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_platform_data *platform_data =
			rmi4_data->board;

	if (platform_data->regulator_en)
		regulator_enable(rmi4_data->regulator);

	if (gpio_request(platform_data->reset_gpio, RESET_GPIO_NAME) < 0)
		pr_err("failed to request reset gpio\n");

	synaptics_rmi4_reset_device(rmi4_data);

	synaptics_dsx_sensor_ready_state(rmi4_data, false);

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
#if !defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_PM)
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = __devexit_p(synaptics_rmi4_remove),
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
