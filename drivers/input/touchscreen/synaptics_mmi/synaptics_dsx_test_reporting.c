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
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#include "synaptics_dsx_i2c.h"

#define WATCHDOG_HRTIMER
#define WATCHDOG_TIMEOUT_S 1
#define STATUS_WORK_INTERVAL 20 /* ms */

/* enable for report status polling */
#undef REPORT_STATUS_POLLING

/*
#define RAW_HEX
#define HUMAN_READABLE
*/

#define STATUS_IDLE 0
#define STATUS_BUSY 1
#define STATUS_ERROR 2

#define DATA_REPORT_INDEX_OFFSET 1
#define DATA_REPORT_DATA_OFFSET 3

#define COMMAND_GET_REPORT 1
#define COMMAND_FORCE_CAL 2
#define COMMAND_FORCE_UPDATE 4
#define COMMAND_ENTER_IN_CELL_TESTMODE 16

#define HIGH_RESISTANCE_DATA_SIZE 6
#define FULL_RAW_CAP_MIN_MAX_DATA_SIZE 4
#define TREX_DATA_SIZE 7

#define NO_AUTO_CAL_MASK 0x01

static inline ssize_t synaptics_dsx_test_reporting_show_error(struct device *dev,
		struct device_attribute *attr, char *buf);
static inline ssize_t synaptics_dsx_test_reporting_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define concat(a, b) a##b

#define GROUP(_attrs) {\
	.attrs = _attrs,\
}

#define attrify(propname) (&dev_attr_##propname.attr)

#define show_prototype_ext(propname, perm)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf);\
\
struct device_attribute dev_attr_##propname =\
		__ATTR(propname, (perm),\
		concat(synaptics_rmi4_f54, _##propname##_show),\
		synaptics_dsx_test_reporting_store_error);

#define show_prototype(propname)\
	show_prototype_ext(propname, S_IRUSR | S_IRGRP | S_IROTH)

#define store_prototype(propname)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count);\
\
struct device_attribute dev_attr_##propname =\
		__ATTR(propname, S_IWUSR | S_IWGRP,\
		synaptics_dsx_test_reporting_show_error,\
		concat(synaptics_rmi4_f54, _##propname##_store));

#define show_store_prototype_ext(propname, perm)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf);\
\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count);\
\
struct device_attribute dev_attr_##propname =\
		__ATTR(propname, (perm),\
		concat(synaptics_rmi4_f54, _##propname##_show),\
		concat(synaptics_rmi4_f54, _##propname##_store));

#define show_store_prototype(propname)\
	show_store_prototype_ext(propname, \
		S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP)

#define simple_show_func(rtype, propname, fmt)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf)\
{\
	return snprintf(buf, PAGE_SIZE, fmt, f54->rtype.propname);\
} \

#define simple_show_func_unsigned(rtype, propname)\
simple_show_func(rtype, propname, "%u\n")

#define show_func(rtype, rgrp, propname, fmt)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf)\
{\
	int retval;\
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;\
\
	mutex_lock(&f54->rtype##_mutex);\
\
	retval = f54->fn_ptr->read(rmi4_data,\
			f54->rtype.rgrp->address,\
			f54->rtype.rgrp->data,\
			sizeof(f54->rtype.rgrp->data));\
	mutex_unlock(&f54->rtype##_mutex);\
	if (retval < 0) {\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read " #rtype\
				" " #rgrp "\n",\
				__func__);\
		return retval;\
	} \
\
	return snprintf(buf, PAGE_SIZE, fmt,\
			f54->rtype.rgrp->propname);\
} \

#define show_func_unsigned(rtype, rgrp, propname)\
show_func(rtype, rgrp, propname, "%u\n")

#define show_store_func(rtype, rgrp, propname, fmt)\
show_func(rtype, rgrp, propname, fmt)\
\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count)\
{\
	int retval;\
	unsigned long setting;\
	unsigned long o_setting;\
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;\
\
	retval = sstrtoul(buf, 10, &setting);\
	if (retval)\
		return retval;\
\
	mutex_lock(&f54->rtype##_mutex);\
	retval = f54->fn_ptr->read(rmi4_data,\
			f54->rtype.rgrp->address,\
			f54->rtype.rgrp->data,\
			sizeof(f54->rtype.rgrp->data));\
	if (retval < 0) {\
		mutex_unlock(&f54->rtype##_mutex);\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read " #rtype\
				" " #rgrp "\n",\
				__func__);\
		return retval;\
	} \
\
	if (f54->rtype.rgrp->propname == setting) {\
		mutex_unlock(&f54->rtype##_mutex);\
		return count;\
	} \
\
	o_setting = f54->rtype.rgrp->propname;\
	f54->rtype.rgrp->propname = setting;\
\
	retval = f54->fn_ptr->write(rmi4_data,\
			f54->rtype.rgrp->address,\
			f54->rtype.rgrp->data,\
			sizeof(f54->rtype.rgrp->data));\
	if (retval < 0) {\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to write " #rtype\
				" " #rgrp "\n",\
				__func__);\
		f54->rtype.rgrp->propname = o_setting;\
		mutex_unlock(&f54->rtype##_mutex);\
		return retval;\
	} \
\
	mutex_unlock(&f54->rtype##_mutex);\
	return count;\
} \

#define show_store_func_unsigned(rtype, rgrp, propname)\
show_store_func(rtype, rgrp, propname, "%u\n")

#define show_subpkt_func(rtype, rgrp, subpkt, propname, fmt)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf)\
{\
	int retval;\
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;\
\
	mutex_lock(&f54->rtype##_mutex);\
\
	retval = f54->fn_ptr->read(rmi4_data,\
			f54->rtype.rgrp->address,\
			f54->rtype.rgrp->data,\
			f54->rtype.rgrp->size);\
	mutex_unlock(&f54->rtype##_mutex);\
	if (retval < 0) {\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read " #rtype\
				" " #rgrp #subpkt"\n",\
				__func__);\
		return retval;\
	} \
\
	return snprintf(buf, PAGE_SIZE, fmt,\
			f54->rtype.rgrp->sp##subpkt->propname);\
} \

#define show_subpkt_func_unsigned(rtype, rgrp, subpkt, propname)\
show_subpkt_func(rtype, rgrp, subpkt, propname, "%u\n")

#define show_store_subpkt_func(rtype, rgrp, subpkt, propname, fmt)\
show_subpkt_func(rtype, rgrp, subpkt, propname, fmt)\
\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count)\
{\
	int retval;\
	unsigned long setting;\
	unsigned long o_setting;\
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;\
\
	retval = sstrtoul(buf, 10, &setting);\
	if (retval)\
		return retval;\
\
	mutex_lock(&f54->rtype##_mutex);\
	retval = f54->fn_ptr->read(rmi4_data,\
			f54->rtype.rgrp->address,\
			f54->rtype.rgrp->data,\
			f54->rtype.rgrp->size);\
	if (retval < 0) {\
		mutex_unlock(&f54->rtype##_mutex);\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read " #rtype\
				" " #rgrp #subpkt"\n",\
				__func__);\
		return retval;\
	} \
\
	if (f54->rtype.rgrp->sp##subpkt->propname == setting) {\
		mutex_unlock(&f54->rtype##_mutex);\
		return count;\
	} \
\
	o_setting = f54->rtype.rgrp->sp##subpkt->propname;\
	f54->rtype.rgrp->sp##subpkt->propname = setting;\
\
	retval = f54->fn_ptr->write(rmi4_data,\
			f54->rtype.rgrp->address,\
			f54->rtype.rgrp->data,\
			f54->rtype.rgrp->size);\
	if (retval < 0) {\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to write " #rtype\
				" " #rgrp #subpkt"\n",\
				__func__);\
		f54->rtype.rgrp->sp##subpkt->propname = o_setting;\
		mutex_unlock(&f54->rtype##_mutex);\
		return retval;\
	} \
\
	mutex_unlock(&f54->rtype##_mutex);\
	return count;\
} \

#define show_store_subpkt_func_unsigned(rtype, rgrp, subpkt, propname)\
show_store_subpkt_func(rtype, rgrp, subpkt, propname, "%u\n")

#define show_replicated_func(rtype, rgrp, propname, fmt)\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf)\
{\
	int retval;\
	int size = 0;\
	unsigned char ii;\
	unsigned char length;\
	unsigned char element_count;\
	unsigned char *temp;\
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;\
\
	mutex_lock(&f54->rtype##_mutex);\
\
	length = f54->rtype.rgrp->length;\
	element_count = length / sizeof(*f54->rtype.rgrp->data);\
\
	retval = f54->fn_ptr->read(rmi4_data,\
			f54->rtype.rgrp->address,\
			(unsigned char *)f54->rtype.rgrp->data,\
			length);\
	mutex_unlock(&f54->rtype##_mutex);\
	if (retval < 0) {\
		dev_dbg(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read " #rtype\
				" " #rgrp "\n",\
				__func__);\
	} \
\
	temp = buf;\
\
	for (ii = 0; ii < element_count; ii++) {\
		retval = snprintf(temp, PAGE_SIZE - size, fmt " ",\
				f54->rtype.rgrp->data[ii].propname);\
		if (retval < 0) {\
			dev_err(&rmi4_data->i2c_client->dev,\
					"%s: Failed to write output\n",\
					__func__);\
			return retval;\
		} \
		size += retval;\
		temp += retval;\
	} \
\
	retval = snprintf(temp, PAGE_SIZE - size, "\n");\
	if (retval < 0) {\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Faild to write null terminator\n",\
				__func__);\
		return retval;\
	} \
\
	return size + retval;\
} \

#define show_replicated_func_unsigned(rtype, rgrp, propname)\
show_replicated_func(rtype, rgrp, propname, "%u")

#define show_store_replicated_func(rtype, rgrp, propname, fmt)\
show_replicated_func(rtype, rgrp, propname, fmt)\
\
static ssize_t concat(synaptics_rmi4_f54, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count)\
{\
	int retval;\
	unsigned int setting;\
	unsigned char ii;\
	unsigned char length;\
	unsigned char element_count;\
	const unsigned char *temp;\
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;\
\
	mutex_lock(&f54->rtype##_mutex);\
\
	length = f54->rtype.rgrp->length;\
	element_count = length / sizeof(*f54->rtype.rgrp->data);\
\
	retval = f54->fn_ptr->read(rmi4_data,\
			f54->rtype.rgrp->address,\
			(unsigned char *)f54->rtype.rgrp->data,\
			length);\
	if (retval < 0) {\
		dev_dbg(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read " #rtype\
				" " #rgrp "\n",\
				__func__);\
	} \
\
	temp = buf;\
\
	for (ii = 0; ii < element_count; ii++) {\
		if (sscanf(temp, fmt, &setting) == 1) {\
			f54->rtype.rgrp->data[ii].propname = setting;\
		} else {\
			mutex_unlock(&f54->rtype##_mutex);\
			return -EINVAL;\
		} \
\
		while (*temp != 0) {\
			temp++;\
			if (isspace(*(temp - 1)) && !isspace(*temp))\
				break;\
		} \
	} \
\
	retval = f54->fn_ptr->write(rmi4_data,\
			f54->rtype.rgrp->address,\
			(unsigned char *)f54->rtype.rgrp->data,\
			length);\
	mutex_unlock(&f54->rtype##_mutex);\
	if (retval < 0) {\
		dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to write " #rtype\
				" " #rgrp "\n",\
				__func__);\
		return retval;\
	} \
\
	return count;\
} \

#define show_store_replicated_func_unsigned(rtype, rgrp, propname)\
show_store_replicated_func(rtype, rgrp, propname, "%u")

#define DATA_REG_ADD(reg, skip, cond) \
	do { if (cond) { \
		attrs_data_regs_exist[reg_num] = true;\
		data->reg_##reg = kzalloc(sizeof(*data->reg_##reg),\
			GFP_KERNEL);\
		if (!data->reg_##reg)\
			goto exit_no_mem;\
		pr_debug("d%s addr = 0x%02x added (%d)\n",\
			 #reg, reg_addr, reg_num);\
		data->reg_##reg->address = reg_addr;\
		reg_addr += skip;\
	} \
	reg_num++;\
	} while (0)

#define DATA_REG_PRESENCE(reg, skip, cond) \
	do { if (cond) { \
		pr_debug("d%s addr = 0x%02x\n", #reg, reg_addr);\
		reg_addr += skip;\
	} \
	} while (0)

#define CTRL_REG_ADD(reg, skip, cond) \
	do { if (cond) { \
		attrs_ctrl_regs_exist[reg_num] = true;\
		control->reg_##reg = kzalloc(sizeof(*control->reg_##reg),\
			GFP_KERNEL);\
		if (!control->reg_##reg)\
			goto exit_no_mem;\
		pr_debug("c%s addr = 0x%02x added (%d)\n",\
			 #reg, reg_addr, reg_num);\
		control->reg_##reg->address = reg_addr;\
		reg_addr += skip;\
	} \
	reg_num++;\
	} while (0)

#define CTRL_REG_ADD_EXT(reg, skip, cond, size) \
	do { if (cond) { \
		attrs_ctrl_regs_exist[reg_num] = true;\
		control->reg_##reg = kzalloc(sizeof(*control->reg_##reg),\
			GFP_KERNEL);\
		if (!control->reg_##reg)\
			goto exit_no_mem;\
		control->reg_##reg->data = kzalloc(size, GFP_KERNEL);\
		if (!control->reg_##reg->data)\
			goto exit_no_mem;\
		pr_debug("c%s addr = 0x%02x size = %zu added (%d)\n",\
			 #reg, reg_addr, size, reg_num);\
		control->reg_##reg->length = size;\
		control->reg_##reg->address = reg_addr;\
		reg_addr += skip;\
	} \
	reg_num++;\
	} while (0)

#define CTRL_SUBPKT_SZ(reg, subpkt) \
	sizeof(*((struct f54_control_##reg *)0)->sp##subpkt)

#define CTRL_PKT_REG_START(reg, max_size) \
	do {\
		control->reg_##reg = kzalloc(sizeof(*control->reg_##reg),\
			GFP_KERNEL);\
		if (!control->reg_##reg)\
			goto exit_no_mem;\
		control->reg_##reg->data = kzalloc(max_size,\
			GFP_KERNEL);\
		if (!control->reg_##reg->data)\
			goto exit_no_mem;\
	} while (0)

#define CTRL_SUBPKT_ADD(reg, subpkt, cond) \
	do { if (cond) { \
		attrs_ctrl_regs_exist[reg_num] = true; \
		pr_debug("subpkt c%s_%d added (%d)\n", \
			 #reg, subpkt, reg_num); \
		control->reg_##reg->sp##subpkt = \
			(void *)control->reg_##reg->data + \
				control->reg_##reg->size; \
		control->reg_##reg->size += \
			sizeof(*control->reg_##reg->sp##subpkt); \
	} \
	reg_num++;\
	} while (0)

#define CTRL_PKT_REG_END(reg, skip) \
	do { if (control->reg_##reg->size) { \
		control->reg_##reg->address = reg_addr; \
		pr_debug("c%s addr = 0x%02x\n", \
			 #reg, reg_addr); \
		reg_addr += skip; \
	} \
	else { \
		kfree(control->reg_##reg->data); \
		kfree(control->reg_##reg); \
	} } while (0)

#define CTRL_REG_PRESENCE(reg, skip, cond) \
	do { if ((cond)) {\
		pr_debug("c%s addr = 0x%02x\n",\
			 #reg, reg_addr);\
		reg_addr += (skip);\
	} } while (0)

#define CTRL_REG_RESERVED_PRESENCE(reg, skip, cond) \
	do { if ((cond)) { \
		pr_debug("c%s addr = 0x%02x (reserved)\n",\
			 #reg, reg_addr);\
		reg_addr += skip;\
	} } while (0)

#define QUERY_REG_READ(reg, cond)\
	do { if (cond) { \
		retval = f54->fn_ptr->read(rmi4_data,\
				reg_addr,\
				f54->query##reg.data,\
				sizeof(f54->query##reg.data));\
		if (retval < 0) { \
			dev_err(&rmi4_data->i2c_client->dev,\
				"%s: Failed to read query %s register\n",\
					#reg, __func__);\
			goto err;\
		} \
		pr_debug("q%s addr = 0x%02x, val = 0x%02x\n",\
			#reg, reg_addr, f54->query##reg.data[0]);\
		reg_addr += 1;\
	} \
	else { \
		memset(&f54->query##reg.data, 0, sizeof(f54->query##reg.data));\
	} } while (0)

enum f54_report_types {
	F54_8BIT_IMAGE = 1,
	F54_16BIT_IMAGE = 2,
	F54_RAW_16BIT_IMAGE = 3,
	F54_HIGH_RESISTANCE = 4,
	F54_TX_TO_TX_SHORT = 5,
	F54_RX_TO_RX1 = 7,
	F54_TRUE_BASELINE = 9,
	F54_FULL_RAW_CAP_MIN_MAX = 13,
	F54_RX_OPENS1 = 14,
	F54_TX_OPEN = 15,
	F54_TX_TO_GROUND = 16,
	F54_RX_TO_RX2 = 17,
	F54_RX_OPENS2 = 18,
	F54_FULL_RAW_CAP = 19,
	F54_FULL_RAW_CAP_RX_COUPLING_COMP = 20,
	F54_SENSOR_SPEED = 22,
	F54_ADC_RANGE = 23,
	F54_TREX_OPENS = 24,
	F54_TREX_TO_GND = 25,
	F54_TREX_SHORTS = 26,
	F54_HYB_ABS_RX_TX_DELTA = 59,
	F54_HYB_ABS_RX_TX_RAW = 63,
	INVALID_REPORT_TYPE = -1,
};

struct f54_query {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;

			/* query 1 */
			unsigned char num_of_tx_electrodes;

			/* query 2 */
			unsigned char f54_query2_b0__1:2;
			unsigned char has_baseline:1;
			unsigned char has_image8:1;
			unsigned char f54_query2_b4__5:2;
			unsigned char has_image16:1;
			unsigned char f54_query2_b7:1;

			/* queries 3.0 and 3.1 */
			unsigned short clock_rate;

			/* query 4 */
			unsigned char touch_controller_family;

			/* query 5 */
			unsigned char has_pixel_touch_threshold_adjustment:1;
			unsigned char f54_query5_b1__7:7;

			/* query 6 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_interference_metric:1;
			unsigned char has_sense_frequency_control:1;
			unsigned char has_firmware_noise_mitigation:1;
			unsigned char has_ctrl11:1;
			unsigned char has_two_byte_report_rate:1;
			unsigned char has_one_byte_report_rate:1;
			unsigned char has_relaxation_control:1;

			/* query 7 */
			unsigned char curve_compensation_mode:2;
			unsigned char f54_query7_b2__7:6;

			/* query 8 */
			unsigned char f54_query8_b0:1;
			unsigned char has_iir_filter:1;
			unsigned char has_cmn_removal:1;
			unsigned char has_cmn_maximum:1;
			unsigned char has_touch_hysteresis:1;
			unsigned char has_edge_compensation:1;
			unsigned char has_per_frequency_noise_control:1;
			unsigned char has_enhanced_stretch:1;

			/* query 9 */
			unsigned char has_force_fast_relaxation:1;
			unsigned char has_multi_metric_state_machine:1;
			unsigned char has_signal_clarity:1;
			unsigned char has_variance_metric:1;
			unsigned char has_0d_relaxation_control:1;
			unsigned char has_0d_acquisition_control:1;
			unsigned char has_status:1;
			unsigned char has_slew_metric:1;

			/* query 10 */
			unsigned char has_h_blank:1;
			unsigned char has_v_blank:1;
			unsigned char has_long_h_blank:1;
			unsigned char has_startup_fast_relaxation:1;
			unsigned char has_esd_control:1;
			unsigned char has_noise_mitigation2:1;
			unsigned char has_noise_state:1;
			unsigned char has_energy_ratio_relaxation:1;

			/* query 11 */
			unsigned char has_excessive_noise_reporting:1;
			unsigned char has_slew_option:1;
			unsigned char has_two_overhead_bursts:1;
			unsigned char has_query13:1;
			unsigned char has_one_overhead_burst:1;
			unsigned char f54_query11_b5:1;
			unsigned char has_ctrl88:1;
			unsigned char has_query15:1;
		} __packed;
		unsigned char data[13];
	};
};

struct f54_query12 {
	union {
		struct {
			unsigned char number_of_sensing_frequencies:4;
			unsigned char f54_query12_b4__7:4;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query13 {
	union {
		struct {
			unsigned char has_ctrl86:1;
			unsigned char has_ctrl87:1;
			unsigned char has_ctrl87_sub0:1;
			unsigned char has_ctrl87_sub1:1;
			unsigned char has_ctrl87_sub2:1;
			unsigned char has_cid_im:1;
			unsigned char has_noise_mitigation_enh:1;
			unsigned char has_rail_im:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query14 {
	union {
		struct {
			unsigned char size_of_ctr87;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query15 {
	union {
		struct {
			unsigned char has_ctrl90:1;
			unsigned char has_traismit_strength:1;
			unsigned char has_ctrl87_sub3:1;
			unsigned char has_query16:1;
			unsigned char has_query20:1;
			unsigned char has_query21:1;
			unsigned char has_query22:1;
			unsigned char has_query25:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query16 {
	union {
		struct {
			unsigned char has_query17:1;
			unsigned char has_data17:1;
			unsigned char has_ctrl92:1;
			unsigned char has_ctrl93:1;
			unsigned char has_ctrl94_query18:1;
			unsigned char has_ctrl95_query19:1;
			unsigned char has_ctrl99:1;
			unsigned char has_ctrl100:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query17 {
	union {
		struct {
			unsigned char q17_num_of_sense_freqs;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query18 {
	union {
		struct {
			unsigned char has_ctrl94_sub0:1;
			unsigned char has_ctrl94_sub1:1;
			unsigned char has_ctrl94_sub2:1;
			unsigned char has_ctrl94_sub3:1;
			unsigned char has_ctrl94_sub4:1;
			unsigned char has_ctrl94_sub5:1;
			unsigned char has_ctrl94_sub6:1;
			unsigned char has_ctrl94_sub7:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query19 {
	union {
		struct {
			unsigned char size_of_ctrl95;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query20 {
	union {
		struct {
			unsigned char adc_clock_divisor;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query21 {
	union {
		struct {
			unsigned char has_abs_rx:1;
			unsigned char has_abs_tx:1;
			unsigned char has_ctrl91:1;
			unsigned char has_ctrl96:1;
			unsigned char has_ctrl97:1;
			unsigned char has_ctrl98:1;
			unsigned char has_data19:1;
			unsigned char has_query24_data18:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query22 {
	union {
		struct {
			unsigned char has_packed_image:1;
			unsigned char has_ctrl101:1;
			unsigned char has_dynamic_sense_display_ratio:1;
			unsigned char has_query23:1;
			unsigned char has_ctrl103_query26:1;
			unsigned char has_ctrl104:1;
			unsigned char has_ctrl105:1;
			unsigned char has_query28:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query23 {
	union {
		struct {
			unsigned char has_ctrl102:1;
			unsigned char has_ctrl102_sub1:1;
			unsigned char has_ctrl102_sub2:1;
			unsigned char has_ctrl102_sub4:1;
			unsigned char has_ctrl102_sub5:1;
			unsigned char has_ctrl102_sub9:1;
			unsigned char has_ctrl102_sub10:1;
			unsigned char has_ctrl102_sub11:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query24 {
	union {
		struct {
			unsigned char size_of_data18;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query25 {
	union {
		struct {
			unsigned char has_ctrl106:1;
			unsigned char has_ctrl102_sub12:1;
			unsigned char has_ctrl107:1;
			unsigned char has_ctrl108:1;
			unsigned char has_ctrl109:1;
			unsigned char has_data20:1;
			unsigned char has_tags_for_moisture:1;
			unsigned char has_query27:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query26 {
	union {
		struct {
			unsigned char has_ctrl103_sub0:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query27 {
	union {
		struct {
			unsigned char has_ctrl110:1;
			unsigned char has_data21:1;
			unsigned char has_ctrl111:1;
			unsigned char has_ctrl112:1;
			unsigned char has_ctrl113:1;
			unsigned char has_data22:1;
			unsigned char has_ctrl114:1;
			unsigned char has_query29:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query28 {
	union {
		struct {
			unsigned char has_capacitance_correction;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query29 {
	union {
		struct {
			unsigned char has_ctrl115:1;
			unsigned char has_ground_ring_options:1;
			unsigned char has_lost_burst_tuning:1;
			unsigned char has_aux_exvcom2_select:1;
			unsigned char has_ctrl116:1;
			unsigned char has_data23:1;
			unsigned char has_ctrl117:1;
			unsigned char has_query30:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query30 {
	union {
		struct {
			unsigned char has_ctrl118:1;
			unsigned char has_ctrl119:1;
			unsigned char has_ctrl120:1;
			unsigned char has_ctrl121:1;
			unsigned char has_ctrl122_query31:1;
			unsigned char has_ctrl123:1;
			unsigned char has_ctrl124:1;
			unsigned char has_query32:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query31 {
	union {
		struct {
			unsigned char num_of_active_stylus_sensing_freqs;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query32 {
	union {
		struct {
			unsigned char has_ctrl125:1;
			unsigned char has_ctrl126:1;
			unsigned char has_ctrl127:1;
			unsigned char has_abs_charge_pump_disable:1;
			unsigned char has_query33:1;
			unsigned char has_data24:1;
			unsigned char has_query34:1;
			unsigned char has_query35:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query33 {
	union {
		struct {
			unsigned char has_ctrl128:1;
			unsigned char has_ctrl129:1;
			unsigned char has_ctrl130:1;
			unsigned char has_ctrl131:1;
			unsigned char has_ctrl132:1;
			unsigned char has_ctrl133:1;
			unsigned char has_ctrl134:1;
			unsigned char has_query36:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query34 {
	union {
		struct {
			unsigned char max_fnm_sliding_window_width;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query35 {
	union {
		struct {
			unsigned char has_data25:1;
			unsigned char has_ctrl135:1;
			unsigned char has_ctrl136:1;
			unsigned char has_ctrl137:1;
			unsigned char has_ctrl138:1;
			unsigned char has_ctrl139:1;
			unsigned char has_data26:1;
			unsigned char has_ctrl140:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query36 {
	union {
		struct {
			unsigned char has_ctrl141:1;
			unsigned char has_ctrl142:1;
			unsigned char has_query37:1;
			unsigned char has_ctrl143:1;
			unsigned char has_ctrl144:1;
			unsigned char has_ctrl145:1;
			unsigned char has_ctrl146:1;
			unsigned char has_query38:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query38 {
	union {
		struct {
			unsigned char has_ctrl147:1;
			unsigned char has_ctrl148:1;
			unsigned char has_ctrl149:1;
			unsigned char f54_q38_b3:1;
			unsigned char has_ctrl151:1;
			unsigned char f54_q38_b5b6:2;
			unsigned char has_query39:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query39 {
	union {
		struct {
			unsigned char f54_b0_to_b6:7;
			unsigned char has_query40:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query40 {
	union {
		struct {
			unsigned char f54_q40_b0:1;
			unsigned char has_ctrl163_query41:1;
			unsigned char f54_q40_b2:1;
			unsigned char has_ctrl165_query42:1;
			unsigned char f54_q40_b4:1;
			unsigned char has_ctrl167:1;
			unsigned char f54_q40_b6:1;
			unsigned char has_query43:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query41 {
	union {
		struct {
			unsigned char size_of_ctrl163;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query42 {
	union {
		struct {
			unsigned char size_of_ctrl165;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query43 {
	union {
		struct {
			unsigned char f54_q43_b0_to_b6:7;
			unsigned char has_query46:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query46 {
	union {
		struct {
			unsigned char f54_q46_b0b1:2;
			unsigned char has_ctrl179:1;
			unsigned char f54_q46_b3:1;
			unsigned char has_data27:1;
			unsigned char has_data28:1;
			unsigned char f54_q46_b6b7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_control_0 {
	union {
		struct {
			unsigned char no_relax:1;
			unsigned char no_scan:1;
			unsigned char force_fast_relaxation:1;
			unsigned char startup_fast_relaxation:1;
			unsigned char gesture_cancels_sfr:1;
			unsigned char enable_energy_ratio_relaxation:1;
			unsigned char excessive_noise_attn_enable:1;
			unsigned char f54_control0_b7:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_1 {
	union {
		struct {
			unsigned char bursts_per_cluster:4;
			unsigned char f54_ctrl1_b4__7:4;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_2 {
	union {
		struct {
			unsigned short saturation_cap;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_3 {
	union {
		struct {
			unsigned char pixel_touch_threshold;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_4__6 {
	union {
		struct {
			/* control 4 */
			unsigned char rx_feedback_cap:2;
			unsigned char bias_current:2;
			unsigned char f54_ctrl4_b4__7:4;

			/* control 5 */
			unsigned char low_ref_cap:2;
			unsigned char low_ref_feedback_cap:2;
			unsigned char low_ref_polarity:1;
			unsigned char f54_ctrl5_b5__7:3;

			/* control 6 */
			unsigned char high_ref_cap:2;
			unsigned char high_ref_feedback_cap:2;
			unsigned char high_ref_polarity:1;
			unsigned char f54_ctrl6_b5__7:3;
		} __packed;
		struct {
			unsigned char data[3];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_7 {
	union {
		struct {
			unsigned char cbc_cap:2;
			unsigned char cbc_polarity:2;
			unsigned char cbc_tx_carrier_selection:1;
			unsigned char f54_ctrl7_b5__7:3;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_8__9 {
	union {
		struct {
			/* control 8 */
			unsigned short integration_duration:10;
			unsigned short f54_ctrl8_b10__15:6;

			/* control 9 */
			unsigned char reset_duration;
		} __packed;
		struct {
			unsigned char data[3];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_10 {
	union {
		struct {
			unsigned char noise_sensing_bursts_per_image:4;
			unsigned char f54_ctrl10_b4__7:4;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_11 {
	union {
		struct {
			unsigned short f54_ctrl11;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_12__13 {
	union {
		struct {
			/* control 12 */
			unsigned char slow_relaxation_rate;

			/* control 13 */
			unsigned char fast_relaxation_rate;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_14 {
	union {
		struct {
				unsigned char rxs_on_xaxis:1;
				unsigned char curve_comp_on_txs:1;
				unsigned char f54_ctrl14_b2__7:6;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_15n {
		unsigned char sensor_rx_assignment;
};

struct f54_control_15 {
	struct f54_control_15n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_16n {
	unsigned char sensor_tx_assignment;
};

struct f54_control_16 {
	struct f54_control_16n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_17n {
	unsigned char burst_count_b8__10:3;
	unsigned char disable:1;
	unsigned char f54_ctrl17_b4:1;
	unsigned char filter_bandwidth:3;
};

struct f54_control_17 {
	struct f54_control_17n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_18n {
	unsigned char burst_count_b0__7;
};

struct f54_control_18 {
	struct f54_control_18n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_19n {
	unsigned char stretch_duration;
};

struct f54_control_19 {
	struct f54_control_19n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_20 {
	union {
		struct {
			unsigned char disable_noise_mitigation:1;
			unsigned char f54_ctrl20_b2__7:7;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_21 {
	union {
		struct {
			unsigned short freq_shift_noise_threshold;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_22__26 {
	union {
		struct {
			/* control 22 */
			unsigned char f54_ctrl22;

			/* control 23 */
			unsigned short medium_noise_threshold;

			/* control 24 */
			unsigned short high_noise_threshold;

			/* control 25 */
			unsigned char noise_density;

			/* control 26 */
			unsigned char frame_count;
		} __packed;
		struct {
			unsigned char data[7];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_27 {
	union {
		struct {
			unsigned char iir_filter_coef;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_28 {
	union {
		struct {
			unsigned short quiet_threshold;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_29 {
	union {
		struct {
			/* control 29 */
			unsigned char f54_ctrl29_b0__6:7;
			unsigned char cmn_filter_disable:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_30 {
	union {
		struct {
			unsigned char cmn_filter_max;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_31 {
	union {
		struct {
			unsigned char touch_hysteresis;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_32__35 {
	union {
		struct {
			/* control 32 */
			unsigned short rx_low_edge_comp;

			/* control 33 */
			unsigned short rx_high_edge_comp;

			/* control 34 */
			unsigned short tx_low_edge_comp;

			/* control 35 */
			unsigned short tx_high_edge_comp;
		} __packed;
		struct {
			unsigned char data[8];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_36n {
	unsigned char axis1_comp;
};

struct f54_control_36 {
	struct f54_control_36n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_37n {
	unsigned char axis2_comp;
};

struct f54_control_37 {
	struct f54_control_37n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_38n {
	unsigned char noise_control_1;
};

struct f54_control_38 {
	struct f54_control_38n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_39n {
	unsigned char noise_control_2;
};

struct f54_control_39 {
	struct f54_control_39n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_40n {
	unsigned char noise_control_3;
};

struct f54_control_40 {
	struct f54_control_40n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_89_subpkt0 {
	unsigned char c89_cid_sel_opt:2;
	unsigned char c89_cid_voltage_sel:3;
	unsigned char c89_byte0_b5_b7:3;
	unsigned char c89_cid_im_noise_threshold_lsb;
	unsigned char c89_cid_im_noise_threshold_msb;
} __packed;

struct f54_control_89_subpkt1 {
	unsigned char c89_fnm_pixel_touch_mult;
	unsigned char c89_freq_scan_threshold_lsb;
	unsigned char c89_freq_scan_threshold_msb;
	unsigned char c89_quiet_im_threshold_lsb;
	unsigned char c89_quiet_im_threshold_msb;
} __packed;

struct f54_control_89_subpkt2 {
	unsigned char c89_rail_sel_opt:2;
	unsigned char c89_byte0_b3_b7:6;
	unsigned char c89_rail_im_noise_threshold_lsb;
	unsigned char c89_rail_im_noise_threshold_msb;
	unsigned char c89_rail_im_quiet_threshold_lsb;
	unsigned char c89_rail_im_quiet_threshold_msb;
} __packed;

struct f54_control_89 {
	unsigned char *data;
	int size;
	unsigned short address;
	struct f54_control_89_subpkt0 *sp0;
	struct f54_control_89_subpkt1 *sp1;
	struct f54_control_89_subpkt2 *sp2;
};

struct f54_control_93 {
	union {
		struct {
			unsigned char c93_freq_shift_noise_threshold_lsb;
			unsigned char c93_freq_shift_noise_threshold_msb;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_95 {
	struct f54_control_95n *data;
	unsigned short address;
	unsigned char length;
};

struct f54_control_99 {
	union {
		struct {
			unsigned char c99_int_dur_lsb;
			unsigned char c99_int_dur_msb;
			unsigned char c99_reset_dur;
		} __packed;
		struct {
			unsigned char data[3];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_107 {
	union {
		struct {
			unsigned char c107_abs_int_dur;
			unsigned char c107_abs_reset_dur;
			unsigned char c107_abs_filter_bw;
			unsigned char c107_abs_rstretch;
			unsigned char c107_abs_burst_count_1;
			unsigned char c107_abs_burst_count_2;
			unsigned char c107_abs_stretch_dur;
			unsigned char c107_abs_adc_clock_div;
			unsigned char c107_abs_sub_burtst_size;
			unsigned char c107_abs_trigger_delay;
		} __packed;
		struct {
			unsigned char data[10];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_113 {
	union {
		struct {
			unsigned char c113_en_hybrid_on_tx:1;
			unsigned char c113_en_hybrid_on_rx:1;
			unsigned char c113_tx_consistency_chk:1;
			unsigned char c113_rx_consistency_chk:1;
			unsigned char c113_en_gradient_baseline:1;
			unsigned char c113_byte0_b5b7:3;
			unsigned char c113_consistency_thresh;
			unsigned char c113_tx_obj_thresh;
			unsigned char c113_rx_obj_thresh;
			unsigned char c113_tx_thermal_update_int_lsb;
			unsigned char c113_tx_thermal_update_int_msb;
			unsigned char c113_rx_thermal_update_int_lsb;
			unsigned char c113_rx_thermal_update_int_msb;
			unsigned char c113_tx_negative_thresh;
			unsigned char c113_rx_negative_thresh;
		} __packed;
		struct {
			unsigned char data[10];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_116 {
	union {
		struct {
			unsigned char c116_im_thresh;
			unsigned char c116_debounce;
			unsigned char c116_quiet_im;
			unsigned char c116_filter;
		} __packed;
		struct {
			unsigned char data[4];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_137 {
	union {
		struct {
			unsigned char c137_cmnr_adjust;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_145 {
	union {
		struct {
			unsigned char c145_bursts_per_cluster_rx;
			unsigned char c145_bursts_per_cluster_tx;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_146 {
	union {
		struct {
			unsigned char c146_int_dur_lsb;
			unsigned char c146_int_dur_msb;
			unsigned char c146_reset_dur;
			unsigned char c146_filter_bandwidth;
			unsigned char c146_stretch_dur;
			unsigned char c146_burst_count1;
			unsigned char c146_burst_count2;
			unsigned char c146_stretch_dur2;
			unsigned char c146_adc_clock_div;
		} __packed;
		struct {
			unsigned char data[9];
			unsigned short address;
		} __packed;
	};
};

struct f54_control {
	struct f54_control_0 *reg_0;
	struct f54_control_1 *reg_1;
	struct f54_control_2 *reg_2;
	struct f54_control_3 *reg_3;
	struct f54_control_4__6 *reg_4__6;
	struct f54_control_7 *reg_7;
	struct f54_control_8__9 *reg_8__9;
	struct f54_control_10 *reg_10;
	struct f54_control_11 *reg_11;
	struct f54_control_12__13 *reg_12__13;
	struct f54_control_14 *reg_14;
	struct f54_control_15 *reg_15;
	struct f54_control_16 *reg_16;
	struct f54_control_17 *reg_17;
	struct f54_control_18 *reg_18;
	struct f54_control_19 *reg_19;
	struct f54_control_20 *reg_20;
	struct f54_control_21 *reg_21;
	struct f54_control_22__26 *reg_22__26;
	struct f54_control_27 *reg_27;
	struct f54_control_28 *reg_28;
	struct f54_control_29 *reg_29;
	struct f54_control_30 *reg_30;
	struct f54_control_31 *reg_31;
	struct f54_control_32__35 *reg_32__35;
	struct f54_control_36 *reg_36;
	struct f54_control_37 *reg_37;
	struct f54_control_38 *reg_38;
	struct f54_control_39 *reg_39;
	struct f54_control_40 *reg_40;
	struct f54_control_89 *reg_89;
	struct f54_control_93 *reg_93;
	struct f54_control_95 *reg_95;
	struct f54_control_99 *reg_99;
	struct f54_control_107 *reg_107;
	struct f54_control_113 *reg_113;
	struct f54_control_116 *reg_116;
	struct f54_control_137 *reg_137;
	struct f54_control_145 *reg_145;
	struct f54_control_146 *reg_146;
	struct f55_control_0 *reg_0_f55;
	struct f55_control_8 *reg_8_f55;
};

struct f54_data_4 {
	union {
		struct {
			unsigned char d4_sense_freq_sel:4;
			unsigned char d4_baseline_sel:2;
			unsigned char d4_b6:1;
			unsigned char d4_inhibit_freq_shift:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};
struct f54_data_6 {
	union {
		struct {
			unsigned char d6_interference_metric_lsb;
			unsigned char d6_interference_metric_msb;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;

	};
};

struct f54_data_7_0 {
	union {
		struct {
			unsigned char d7_current_report_rate_lsb;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_7_1 {
	union {
		struct {
			unsigned char d7_current_report_rate_msb;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_8 {
	union {
		struct {
			unsigned char d8_variance_metric_lsb;
			unsigned char d8_variance_metric_msb;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_9 {
	union {
		struct {
			unsigned char d9_averaged_im_lsb;
			unsigned char d9_averaged_im_msb;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_10 {
	union {
		struct {
			unsigned char d10_noise_state;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_14 {
	union {
		struct {
			unsigned char d14_cid_im_lsb;
			unsigned char d14_cid_im_msb;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_16 {
	union {
		struct {
			unsigned char d16_freq_scan_im_lsb;
			unsigned char d16_freq_scan_im_msb;
		} __packed;
		struct {
			unsigned char data[2];
			unsigned short address;
		} __packed;
	};
};

struct f54_data_17 {
	union {
		struct {
			unsigned char d17_freq:7;
			unsigned char d17_inhibit_freq_shift:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_data {
	struct f54_data_4 *reg_4;
	struct f54_data_6 *reg_6;
	struct f54_data_7_0 *reg_7_0;
	struct f54_data_7_1 *reg_7_1;
	struct f54_data_8 *reg_8;
	struct f54_data_9 *reg_9;
	struct f54_data_10 *reg_10;
	struct f54_data_14 *reg_14;
	struct f54_data_16 *reg_16;
	struct f54_data_17 *reg_17;
};

struct f55_query_0_2 {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;

			/* query 1 */
			unsigned char num_of_tx_electrodes;

			/* query 2 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_edge_compensation:1;
			unsigned char curve_compensation_mode:2;
			unsigned char has_ctrl6:1;
			unsigned char has_alternate_tx_assignment:1;
			unsigned char f55_q2_has_single_layer_multitouch:1;
			unsigned char has_query5:1;
		} __packed;
		unsigned char data[3];
	};
};

struct f55_query_3 {
	union {
		struct {
			unsigned char f55_q3_has_ctrl8:1;
			unsigned char has_ctrl9:1;
			unsigned char has_on_cell_pattern:1;
			unsigned char has_data0:1;
			unsigned char has_single_wide_pattern:1;
			unsigned char has_mirrored_tx_pattern:1;
			unsigned char has_discrete_pattern:1;
			unsigned char has_query9:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_control_0 {
	union {
		struct {
			unsigned char f55_c0_receivers_on_x:1;
			unsigned char f55_c0_curve_compensation_on_tx:1;
			unsigned char f55_c0_trx_sense:1;
			unsigned char f55_c0_trx_config:1;
			unsigned char f55_c0_guard_disable:1;
			unsigned char f55_c0_b5_b7:3;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f55_control_8 {
	union {
		struct {
			unsigned char f55_c8_pattern_type;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct synaptics_rmi4_fn55_desc {
	unsigned short query_base_addr;
	unsigned short control_base_addr;
};

struct synaptics_rmi4_f54_handle {
	bool no_auto_cal;
	int status;
	unsigned char intr_mask;
	unsigned char intr_reg_num;
	unsigned char *report_data;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short fifoindex;
	unsigned int report_size;
	unsigned int data_buffer_size;
	enum f54_report_types report_type;
	enum f54_report_types user_report_type1;
	enum f54_report_types user_report_type2;
	struct mutex status_mutex;
	struct mutex data_mutex;
	struct mutex control_mutex;
	struct f54_query query;
	struct f54_query12 query12;
	struct f54_query13 query13;
	struct f54_query14 query14;
	struct f54_query15 query15;
	struct f54_query16 query16;
	struct f54_query17 query17;
	struct f54_query18 query18;
	struct f54_query19 query19;
	struct f54_query20 query20;
	struct f54_query21 query21;
	struct f54_query22 query22;
	struct f54_query23 query23;
	struct f54_query24 query24;
	struct f54_query25 query25;
	struct f54_query26 query26;
	struct f54_query27 query27;
	struct f54_query28 query28;
	struct f54_query29 query29;
	struct f54_query30 query30;
	struct f54_query31 query31;
	struct f54_query32 query32;
	struct f54_query33 query33;
	struct f54_query34 query34;
	struct f54_query35 query35;
	struct f54_query36 query36;
	struct f54_query38 query38;
	struct f54_query39 query39;
	struct f54_query40 query40;
	struct f54_query41 query41;
	struct f54_query42 query42;
	struct f54_query43 query43;
	struct f54_query46 query46;
	struct f54_control control;
	struct f54_data data;
	struct kobject *attr_dir;
	struct hrtimer watchdog;
	struct work_struct timeout_work;
	struct delayed_work status_work;
	struct workqueue_struct *status_workqueue;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_fn55_desc *fn55;
	struct f55_query_0_2 query_f55_0_2;
	struct f55_query_3 query_f55_3;
	struct wake_lock test_wake_lock;
};

store_prototype(reset)
show_prototype_ext(status, S_IRUGO)
show_prototype_ext(report_size, S_IRUGO)
show_prototype_ext(num_of_mapped_rx, S_IRUGO)
show_prototype_ext(num_of_mapped_tx, S_IRUGO)
show_store_prototype(no_auto_cal)
show_store_prototype_ext(report_type, S_IRUGO | S_IWUSR | S_IWGRP)
show_store_prototype_ext(user_report_type1, S_IRUGO | S_IWUSR | S_IWGRP)
show_store_prototype_ext(user_report_type2, S_IRUGO | S_IWUSR | S_IWGRP)
show_prototype_ext(user_get_report1, S_IRUGO)
show_prototype_ext(user_get_report2, S_IRUGO)
show_store_prototype(fifoindex)
store_prototype(get_report)
show_store_prototype(force_update)
store_prototype(force_cal)
show_store_prototype(enter_in_cell_test_mode)
show_prototype_ext(num_of_rx_electrodes, S_IRUGO)
show_prototype_ext(num_of_tx_electrodes, S_IRUGO)
show_prototype(has_image16)
show_prototype(has_image8)
show_prototype(has_baseline)
show_prototype(clock_rate)
show_prototype(touch_controller_family)
show_prototype(has_pixel_touch_threshold_adjustment)
show_prototype(has_sensor_assignment)
show_prototype(has_interference_metric)
show_prototype(has_sense_frequency_control)
show_prototype(has_firmware_noise_mitigation)
show_prototype(has_two_byte_report_rate)
show_prototype(has_one_byte_report_rate)
show_prototype(has_relaxation_control)
show_prototype(curve_compensation_mode)
show_prototype(has_iir_filter)
show_prototype(has_cmn_removal)
show_prototype(has_cmn_maximum)
show_prototype(has_touch_hysteresis)
show_prototype(has_edge_compensation)
show_prototype(has_per_frequency_noise_control)
show_prototype(has_enhanced_stretch)
show_prototype(has_force_fast_relaxation)
show_prototype(has_multi_metric_state_machine)
show_prototype(has_signal_clarity)
show_prototype(has_variance_metric)
show_prototype(has_0d_relaxation_control)
show_prototype(has_0d_acquisition_control)
show_prototype(has_status)
show_prototype(has_slew_metric)
show_prototype(has_h_blank)
show_prototype(has_v_blank)
show_prototype(has_long_h_blank)
show_prototype(has_startup_fast_relaxation)
show_prototype(has_esd_control)
show_prototype(has_noise_mitigation2)
show_prototype(has_noise_state)
show_prototype(has_energy_ratio_relaxation)
show_prototype(number_of_sensing_frequencies)
show_prototype(has_data17)
show_prototype(has_ctrl113)
show_prototype(has_ctrl145)
show_prototype(has_ctrl146)
show_prototype(q17_num_of_sense_freqs)
show_store_prototype(no_relax)
show_store_prototype(no_scan)
show_store_prototype(bursts_per_cluster)
show_store_prototype(saturation_cap)
show_store_prototype(pixel_touch_threshold)
show_store_prototype(rx_feedback_cap)
show_store_prototype(low_ref_cap)
show_store_prototype(low_ref_feedback_cap)
show_store_prototype(low_ref_polarity)
show_store_prototype(high_ref_cap)
show_store_prototype(high_ref_feedback_cap)
show_store_prototype(high_ref_polarity)
show_store_prototype(cbc_cap)
show_store_prototype(cbc_polarity)
show_store_prototype(cbc_tx_carrier_selection)
show_store_prototype(integration_duration)
show_store_prototype(reset_duration)
show_store_prototype(noise_sensing_bursts_per_image)
show_store_prototype(slow_relaxation_rate)
show_store_prototype(fast_relaxation_rate)
show_store_prototype(rxs_on_xaxis)
show_store_prototype(curve_comp_on_txs)
show_prototype(sensor_rx_assignment)
show_prototype(sensor_tx_assignment)
show_prototype(burst_count)
show_prototype(disable)
show_prototype(filter_bandwidth)
show_prototype(stretch_duration)
show_store_prototype(disable_noise_mitigation)
show_store_prototype(freq_shift_noise_threshold)
show_store_prototype(medium_noise_threshold)
show_store_prototype(high_noise_threshold)
show_store_prototype(noise_density)
show_store_prototype(frame_count)
show_store_prototype(iir_filter_coef)
show_store_prototype(quiet_threshold)
show_store_prototype(cmn_filter_disable)
show_store_prototype(cmn_filter_max)
show_store_prototype(touch_hysteresis)
show_store_prototype(rx_low_edge_comp)
show_store_prototype(rx_high_edge_comp)
show_store_prototype(tx_low_edge_comp)
show_store_prototype(tx_high_edge_comp)
show_store_prototype(axis1_comp)
show_store_prototype(axis2_comp)
show_prototype(noise_control_1)
show_prototype(noise_control_2)
show_prototype(noise_control_3)
show_store_prototype(d4_sense_freq_sel)
show_store_prototype(d4_baseline_sel)
show_store_prototype(d4_inhibit_freq_shift)
show_prototype(d6_interference_metric_lsb)
show_prototype(d6_interference_metric_msb)
show_prototype(d7_current_report_rate_lsb)
show_prototype(d7_current_report_rate_msb)
show_prototype(d8_variance_metric_lsb)
show_prototype(d8_variance_metric_msb)
show_prototype(d9_averaged_im_lsb)
show_prototype(d9_averaged_im_msb)
show_prototype(d10_noise_state)
show_store_prototype(d16_freq_scan_im_lsb)
show_store_prototype(d16_freq_scan_im_msb)
show_prototype(d14_cid_im_lsb)
show_prototype(d14_cid_im_msb)
show_store_prototype(d17_inhibit_freq_shift)
show_store_prototype(d17_freq)
show_store_prototype(c89_cid_sel_opt)
show_store_prototype(c89_cid_voltage_sel)
show_store_prototype(c89_cid_im_noise_threshold_lsb)
show_store_prototype(c89_cid_im_noise_threshold_msb)
show_store_prototype(c89_fnm_pixel_touch_mult)
show_store_prototype(c89_freq_scan_threshold_lsb)
show_store_prototype(c89_freq_scan_threshold_msb)
show_store_prototype(c89_quiet_im_threshold_lsb)
show_store_prototype(c89_quiet_im_threshold_msb)
show_store_prototype(c89_rail_sel_opt)
show_store_prototype(c89_rail_im_noise_threshold_lsb)
show_store_prototype(c89_rail_im_noise_threshold_msb)
show_store_prototype(c89_rail_im_quiet_threshold_lsb)
show_store_prototype(c89_rail_im_quiet_threshold_msb)
show_store_prototype(c93_freq_shift_noise_threshold_lsb)
show_store_prototype(c93_freq_shift_noise_threshold_msb)
show_store_prototype(c95_disable)
show_store_prototype(c95_filter_bw)
show_store_prototype(c95_first_burst_length_lsb)
show_store_prototype(c95_first_burst_length_msb)
show_store_prototype(c95_addl_burst_length_lsb)
show_store_prototype(c95_addl_burst_length_msb)
show_store_prototype(c95_i_stretch)
show_store_prototype(c95_r_stretch)
show_store_prototype(c95_noise_control1)
show_store_prototype(c95_noise_control2)
show_store_prototype(c95_noise_control3)
show_store_prototype(c95_noise_control4)
show_store_prototype(c99_int_dur_lsb)
show_store_prototype(c99_int_dur_msb)
show_store_prototype(c99_reset_dur)
show_store_prototype(c107_abs_int_dur)
show_store_prototype(c107_abs_reset_dur)
show_store_prototype(c107_abs_filter_bw)
show_store_prototype(c107_abs_rstretch)
show_store_prototype(c107_abs_burst_count_1)
show_store_prototype(c107_abs_burst_count_2)
show_store_prototype(c107_abs_stretch_dur)
show_store_prototype(c107_abs_adc_clock_div)
show_store_prototype(c107_abs_sub_burtst_size)
show_store_prototype(c107_abs_trigger_delay)
show_store_prototype(c113_en_hybrid_on_tx)
show_store_prototype(c113_en_hybrid_on_rx)
show_store_prototype(c113_tx_consistency_chk)
show_store_prototype(c113_rx_consistency_chk)
show_store_prototype(c113_en_gradient_baseline)
show_store_prototype(c113_consistency_thresh)
show_store_prototype(c113_tx_obj_thresh)
show_store_prototype(c113_rx_obj_thresh)
show_store_prototype(c113_tx_thermal_update_int_lsb)
show_store_prototype(c113_tx_thermal_update_int_msb)
show_store_prototype(c113_rx_thermal_update_int_lsb)
show_store_prototype(c113_rx_thermal_update_int_msb)
show_store_prototype(c113_tx_negative_thresh)
show_store_prototype(c113_rx_negative_thresh)
show_store_prototype(c116_im_thresh)
show_store_prototype(c116_debounce)
show_store_prototype(c116_quiet_im)
show_store_prototype(c116_filter)
show_store_prototype(c137_cmnr_adjust)
show_store_prototype(c145_bursts_per_cluster_rx)
show_store_prototype(c145_bursts_per_cluster_tx)
show_store_prototype(c146_int_dur_lsb)
show_store_prototype(c146_int_dur_msb)
show_store_prototype(c146_reset_dur)
show_store_prototype(c146_filter_bandwidth)
show_store_prototype(c146_stretch_dur)
show_store_prototype(c146_burst_count1)
show_store_prototype(c146_burst_count2)
show_store_prototype(c146_stretch_dur2)
show_store_prototype(c146_adc_clock_div)
show_prototype(f55_q2_has_single_layer_multitouch)
show_prototype(f55_c0_receivers_on_x)
show_prototype(f55_c8_pattern_type)

static ssize_t synaptics_rmi4_f54_data_read(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct attribute *attrs[] = {
	attrify(reset),
	attrify(status),
	attrify(report_size),
	attrify(num_of_mapped_rx),
	attrify(num_of_mapped_tx),
	attrify(no_auto_cal),
	attrify(report_type),
	attrify(user_report_type1),
	attrify(user_report_type2),
	attrify(user_get_report1),
	attrify(user_get_report2),
	attrify(fifoindex),
	attrify(get_report),
	attrify(force_update),
	attrify(force_cal),
	attrify(enter_in_cell_test_mode),
	attrify(num_of_rx_electrodes),
	attrify(num_of_tx_electrodes),
	attrify(has_image16),
	attrify(has_image8),
	attrify(has_baseline),
	attrify(clock_rate),
	attrify(touch_controller_family),
	attrify(has_pixel_touch_threshold_adjustment),
	attrify(has_sensor_assignment),
	attrify(has_interference_metric),
	attrify(has_sense_frequency_control),
	attrify(has_firmware_noise_mitigation),
	attrify(has_two_byte_report_rate),
	attrify(has_one_byte_report_rate),
	attrify(has_relaxation_control),
	attrify(curve_compensation_mode),
	attrify(has_iir_filter),
	attrify(has_cmn_removal),
	attrify(has_cmn_maximum),
	attrify(has_touch_hysteresis),
	attrify(has_edge_compensation),
	attrify(has_per_frequency_noise_control),
	attrify(has_enhanced_stretch),
	attrify(has_force_fast_relaxation),
	attrify(has_multi_metric_state_machine),
	attrify(has_signal_clarity),
	attrify(has_variance_metric),
	attrify(has_0d_relaxation_control),
	attrify(has_0d_acquisition_control),
	attrify(has_status),
	attrify(has_slew_metric),
	attrify(has_h_blank),
	attrify(has_v_blank),
	attrify(has_long_h_blank),
	attrify(has_startup_fast_relaxation),
	attrify(has_esd_control),
	attrify(has_noise_mitigation2),
	attrify(has_noise_state),
	attrify(has_energy_ratio_relaxation),
	attrify(number_of_sensing_frequencies),
	attrify(has_data17),
	attrify(has_ctrl113),
	attrify(has_ctrl145),
	attrify(has_ctrl146),
	attrify(q17_num_of_sense_freqs),
	attrify(f55_q2_has_single_layer_multitouch),
	NULL,
};

static struct attribute_group attr_group = GROUP(attrs);

static struct attribute *attrs_reg_0[] = {
	attrify(no_relax),
	attrify(no_scan),
	NULL,
};

static struct attribute *attrs_reg_1[] = {
	attrify(bursts_per_cluster),
	NULL,
};

static struct attribute *attrs_reg_2[] = {
	attrify(saturation_cap),
	NULL,
};

static struct attribute *attrs_reg_3[] = {
	attrify(pixel_touch_threshold),
	NULL,
};

static struct attribute *attrs_reg_4__6[] = {
	attrify(rx_feedback_cap),
	attrify(low_ref_cap),
	attrify(low_ref_feedback_cap),
	attrify(low_ref_polarity),
	attrify(high_ref_cap),
	attrify(high_ref_feedback_cap),
	attrify(high_ref_polarity),
	NULL,
};

static struct attribute *attrs_reg_7[] = {
	attrify(cbc_cap),
	attrify(cbc_polarity),
	attrify(cbc_tx_carrier_selection),
	NULL,
};

static struct attribute *attrs_reg_8__9[] = {
	attrify(integration_duration),
	attrify(reset_duration),
	NULL,
};

static struct attribute *attrs_reg_10[] = {
	attrify(noise_sensing_bursts_per_image),
	NULL,
};

static struct attribute *attrs_reg_11[] = {
	NULL,
};

static struct attribute *attrs_reg_12__13[] = {
	attrify(slow_relaxation_rate),
	attrify(fast_relaxation_rate),
	NULL,
};

static struct attribute *attrs_reg_14__16[] = {
	attrify(rxs_on_xaxis),
	attrify(curve_comp_on_txs),
	attrify(sensor_rx_assignment),
	attrify(sensor_tx_assignment),
	NULL,
};

static struct attribute *attrs_reg_17__19[] = {
	attrify(burst_count),
	attrify(disable),
	attrify(filter_bandwidth),
	attrify(stretch_duration),
	NULL,
};

static struct attribute *attrs_reg_20[] = {
	attrify(disable_noise_mitigation),
	NULL,
};

static struct attribute *attrs_reg_21[] = {
	attrify(freq_shift_noise_threshold),
	NULL,
};

static struct attribute *attrs_reg_22__26[] = {
	attrify(medium_noise_threshold),
	attrify(high_noise_threshold),
	attrify(noise_density),
	attrify(frame_count),
	NULL,
};

static struct attribute *attrs_reg_27[] = {
	attrify(iir_filter_coef),
	NULL,
};

static struct attribute *attrs_reg_28[] = {
	attrify(quiet_threshold),
	NULL,
};

static struct attribute *attrs_reg_29[] = {
	attrify(cmn_filter_disable),
	NULL,
};

static struct attribute *attrs_reg_30[] = {
	attrify(cmn_filter_max),
	NULL,
};

static struct attribute *attrs_reg_31[] = {
	attrify(touch_hysteresis),
	NULL,
};

static struct attribute *attrs_reg_32__35[] = {
	attrify(rx_low_edge_comp),
	attrify(rx_high_edge_comp),
	attrify(tx_low_edge_comp),
	attrify(tx_high_edge_comp),
	NULL,
};

static struct attribute *attrs_reg_36[] = {
	attrify(axis1_comp),
	NULL,
};

static struct attribute *attrs_reg_37[] = {
	attrify(axis2_comp),
	NULL,
};

static struct attribute *attrs_reg_38__40[] = {
	attrify(noise_control_1),
	attrify(noise_control_2),
	attrify(noise_control_3),
	NULL,
};

static struct attribute *attrs_reg_89_subpkt0[] = {
	attrify(c89_cid_sel_opt),
	attrify(c89_cid_voltage_sel),
	attrify(c89_cid_im_noise_threshold_lsb),
	attrify(c89_cid_im_noise_threshold_msb),
	NULL,
};

static struct attribute *attrs_reg_89_subpkt1[] = {
	attrify(c89_fnm_pixel_touch_mult),
	attrify(c89_freq_scan_threshold_lsb),
	attrify(c89_freq_scan_threshold_msb),
	attrify(c89_quiet_im_threshold_lsb),
	attrify(c89_quiet_im_threshold_msb),
	NULL,
};

static struct attribute *attrs_reg_89_subpkt2[] = {
	attrify(c89_rail_sel_opt),
	attrify(c89_rail_im_noise_threshold_lsb),
	attrify(c89_rail_im_noise_threshold_msb),
	attrify(c89_rail_im_quiet_threshold_lsb),
	attrify(c89_rail_im_quiet_threshold_msb),
	NULL,
};

static struct attribute *attrs_reg_93[] = {
	attrify(c93_freq_shift_noise_threshold_lsb),
	attrify(c93_freq_shift_noise_threshold_msb),
	NULL,
};

static struct attribute *attrs_reg_95[] = {
	attrify(c95_disable),
	attrify(c95_filter_bw),
	attrify(c95_first_burst_length_lsb),
	attrify(c95_first_burst_length_msb),
	attrify(c95_addl_burst_length_lsb),
	attrify(c95_addl_burst_length_msb),
	attrify(c95_i_stretch),
	attrify(c95_r_stretch),
	attrify(c95_noise_control1),
	attrify(c95_noise_control2),
	attrify(c95_noise_control3),
	attrify(c95_noise_control4),
	NULL,
};

static struct attribute *attrs_reg_99[] = {
	attrify(c99_int_dur_lsb),
	attrify(c99_int_dur_msb),
	attrify(c99_reset_dur),
	NULL,
};

static struct attribute *attrs_reg_107[] = {
	attrify(c107_abs_int_dur),
	attrify(c107_abs_reset_dur),
	attrify(c107_abs_filter_bw),
	attrify(c107_abs_rstretch),
	attrify(c107_abs_burst_count_1),
	attrify(c107_abs_burst_count_2),
	attrify(c107_abs_stretch_dur),
	attrify(c107_abs_adc_clock_div),
	attrify(c107_abs_sub_burtst_size),
	attrify(c107_abs_trigger_delay),
	NULL,
};

static struct attribute *attrs_reg_113[] = {
	attrify(c113_en_hybrid_on_tx),
	attrify(c113_en_hybrid_on_rx),
	attrify(c113_tx_consistency_chk),
	attrify(c113_rx_consistency_chk),
	attrify(c113_en_gradient_baseline),
	attrify(c113_consistency_thresh),
	attrify(c113_tx_obj_thresh),
	attrify(c113_rx_obj_thresh),
	attrify(c113_tx_thermal_update_int_lsb),
	attrify(c113_tx_thermal_update_int_msb),
	attrify(c113_rx_thermal_update_int_lsb),
	attrify(c113_rx_thermal_update_int_msb),
	attrify(c113_tx_negative_thresh),
	attrify(c113_rx_negative_thresh),
	NULL,
};

static struct attribute *attrs_reg_116[] = {
	attrify(c116_im_thresh),
	attrify(c116_debounce),
	attrify(c116_quiet_im),
	attrify(c116_filter),
	NULL,
};

static struct attribute *attrs_reg_137[] = {
	attrify(c137_cmnr_adjust),
	NULL,
};

static struct attribute *attrs_reg_145[] = {
	attrify(c145_bursts_per_cluster_rx),
	attrify(c145_bursts_per_cluster_tx),
	NULL,
};

static struct attribute *attrs_reg_146[] = {
	attrify(c146_int_dur_lsb),
	attrify(c146_int_dur_msb),
	attrify(c146_reset_dur),
	attrify(c146_filter_bandwidth),
	attrify(c146_stretch_dur),
	attrify(c146_burst_count1),
	attrify(c146_burst_count2),
	attrify(c146_stretch_dur2),
	attrify(c146_adc_clock_div),
	NULL,
};

static struct attribute *attrs_f55_c0[] = {
	attrify(f55_c0_receivers_on_x),
	NULL,
};

static struct attribute *attrs_f55_c8[] = {
	attrify(f55_c8_pattern_type),
	NULL,
};

static struct attribute_group attrs_ctrl_regs[] = {
	GROUP(attrs_reg_0),
	GROUP(attrs_reg_1),
	GROUP(attrs_reg_2),
	GROUP(attrs_reg_3),
	GROUP(attrs_reg_4__6),
	GROUP(attrs_reg_7),
	GROUP(attrs_reg_8__9),
	GROUP(attrs_reg_10),
	GROUP(attrs_reg_11),
	GROUP(attrs_reg_12__13),
	GROUP(attrs_reg_14__16),
	GROUP(attrs_reg_17__19),
	GROUP(attrs_reg_20),
	GROUP(attrs_reg_21),
	GROUP(attrs_reg_22__26),
	GROUP(attrs_reg_27),
	GROUP(attrs_reg_28),
	GROUP(attrs_reg_29),
	GROUP(attrs_reg_30),
	GROUP(attrs_reg_31),
	GROUP(attrs_reg_32__35),
	GROUP(attrs_reg_36),
	GROUP(attrs_reg_37),
	GROUP(attrs_reg_38__40),
	GROUP(attrs_reg_89_subpkt0),
	GROUP(attrs_reg_89_subpkt1),
	GROUP(attrs_reg_89_subpkt2),
	GROUP(attrs_reg_93),
	GROUP(attrs_reg_95),
	GROUP(attrs_reg_99),
	GROUP(attrs_reg_107),
	GROUP(attrs_reg_113),
	GROUP(attrs_reg_116),
	GROUP(attrs_reg_137),
	GROUP(attrs_reg_145),
	GROUP(attrs_reg_146),
	GROUP(attrs_f55_c0),
	GROUP(attrs_f55_c8),
};

static bool attrs_ctrl_regs_exist[ARRAY_SIZE(attrs_ctrl_regs)];

static struct attribute *data_attrs_reg_4[] = {
	attrify(d4_sense_freq_sel),
	attrify(d4_baseline_sel),
	attrify(d4_inhibit_freq_shift),
	NULL,
};

static struct attribute *data_attrs_reg_6[] = {
	attrify(d6_interference_metric_lsb),
	attrify(d6_interference_metric_msb),
	NULL,
};

static struct attribute *data_attrs_reg_7_0[] = {
	attrify(d7_current_report_rate_lsb),
	NULL,
};

static struct attribute *data_attrs_reg_7_1[] = {
	attrify(d7_current_report_rate_msb),
	NULL,
};

static struct attribute *data_attrs_reg_8[] = {
	attrify(d8_variance_metric_lsb),
	attrify(d8_variance_metric_msb),
	NULL,
};

static struct attribute *data_attrs_reg_9[] = {
	attrify(d9_averaged_im_lsb),
	attrify(d9_averaged_im_msb),
	NULL,
};

static struct attribute *data_attrs_reg_10[] = {
	attrify(d10_noise_state),
	NULL,
};

static struct attribute *data_attrs_reg_14[] = {
	attrify(d14_cid_im_lsb),
	attrify(d14_cid_im_msb),
	NULL,
};

static struct attribute *data_attrs_reg_16[] = {
	attrify(d16_freq_scan_im_lsb),
	attrify(d16_freq_scan_im_msb),
	NULL,
};

static struct attribute *data_attrs_reg_17[] = {
	attrify(d17_freq),
	attrify(d17_inhibit_freq_shift),
	NULL,
};

static struct attribute_group attrs_data_regs[] = {
	GROUP(data_attrs_reg_4),
	GROUP(data_attrs_reg_6),
	GROUP(data_attrs_reg_7_0),
	GROUP(data_attrs_reg_7_1),
	GROUP(data_attrs_reg_8),
	GROUP(data_attrs_reg_9),
	GROUP(data_attrs_reg_10),
	GROUP(data_attrs_reg_14),
	GROUP(data_attrs_reg_16),
	GROUP(data_attrs_reg_17),
};

static bool attrs_data_regs_exist[ARRAY_SIZE(attrs_data_regs)];

static struct bin_attribute dev_report_data = {
	.attr = {
		.name = "report_data",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = synaptics_rmi4_f54_data_read,
};

static struct synaptics_rmi4_f54_handle *f54;

static struct completion remove_complete;

static inline ssize_t synaptics_dsx_test_reporting_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_warn(&f54->rmi4_data->i2c_client->dev,
			"%s: Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_dsx_test_reporting_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	dev_warn(&f54->rmi4_data->i2c_client->dev,
			"%s: Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static bool is_report_type_valid(enum f54_report_types report_type)
{
	switch (report_type) {
	case F54_8BIT_IMAGE:
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_HIGH_RESISTANCE:
	case F54_TX_TO_TX_SHORT:
	case F54_RX_TO_RX1:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP_MIN_MAX:
	case F54_RX_OPENS1:
	case F54_TX_OPEN:
	case F54_TX_TO_GROUND:
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
	case F54_SENSOR_SPEED:
	case F54_ADC_RANGE:
	case F54_TREX_OPENS:
	case F54_TREX_TO_GND:
	case F54_TREX_SHORTS:
	case F54_HYB_ABS_RX_TX_DELTA:
	case F54_HYB_ABS_RX_TX_RAW:
		return true;
		break;
	default:
		return false;
	}
}

static int set_user_report_type(enum f54_report_types *user_report_type,
	const char *buf, size_t count)
{
	unsigned int report_type;
	int retval = 0;
	retval = kstrtouint(buf, 10, &report_type);
	if (retval)
		goto out;

	if (!is_report_type_valid((enum f54_report_types)report_type)) {
		dev_err(&f54->rmi4_data->i2c_client->dev,
				"%s: Report type not supported by driver\n",
				__func__);
		retval = -EINVAL;
	}

out:
	if (retval == 0) {
		*user_report_type = report_type;
		retval = count;
	} else
		*user_report_type = INVALID_REPORT_TYPE;

	return retval;
}

static ssize_t synaptics_rmi4_f54_user_report_type1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return set_user_report_type(&f54->user_report_type1, buf, count);
}

static ssize_t synaptics_rmi4_f54_user_report_type2_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return set_user_report_type(&f54->user_report_type2, buf, count);
}

static void set_report_size(void)
{
	unsigned char rx = f54->rmi4_data->num_of_rx;
	unsigned char tx = f54->rmi4_data->num_of_tx;

	switch (f54->report_type) {
	case F54_8BIT_IMAGE:
		f54->report_size = rx * tx;
		break;
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
	case F54_SENSOR_SPEED:
	case F54_ADC_RANGE:
		f54->report_size = 2 * rx * tx;
		break;
	case F54_HIGH_RESISTANCE:
		f54->report_size = HIGH_RESISTANCE_DATA_SIZE;
		break;
	case F54_TX_TO_TX_SHORT:
	case F54_TX_OPEN:
	case F54_TX_TO_GROUND:
		f54->report_size = (tx + 7) / 8;
		break;
	case F54_RX_TO_RX1:
	case F54_RX_OPENS1:
		if (rx < tx)
			f54->report_size = 2 * rx * rx;
		else
			f54->report_size = 2 * rx * tx;
		break;
	case F54_FULL_RAW_CAP_MIN_MAX:
		f54->report_size = FULL_RAW_CAP_MIN_MAX_DATA_SIZE;
		break;
	case F54_RX_TO_RX2:
	case F54_RX_OPENS2:
		if (rx <= tx)
			f54->report_size = 0;
		else
			f54->report_size = 2 * rx * (rx - tx);
		break;
	case F54_TREX_OPENS:
	case F54_TREX_TO_GND:
	case F54_TREX_SHORTS:
		f54->report_size = TREX_DATA_SIZE;
		break;
	case F54_HYB_ABS_RX_TX_DELTA:
	case F54_HYB_ABS_RX_TX_RAW:
		f54->report_size = 4 * (rx + tx);
		break;
	default:
		f54->report_size = 0;
	}

	return;
}

static int set_interrupt(bool set)
{
	int retval;
	unsigned char ii;
	unsigned char zero = 0x00;
	unsigned char *intr_mask;
	unsigned short f01_ctrl_reg;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	intr_mask = rmi4_data->intr_mask;
	f01_ctrl_reg = rmi4_data->f01_ctrl_base_addr + 1 + f54->intr_reg_num;

	if (!set) {
		retval = f54->fn_ptr->write(rmi4_data,
				f01_ctrl_reg,
				&zero,
				sizeof(zero));
		if (retval < 0)
			return retval;
		pr_debug("cleared intr_mask\n");
	}

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (intr_mask[ii] != 0x00) {
			f01_ctrl_reg = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			if (set) {
				retval = f54->fn_ptr->write(rmi4_data,
						f01_ctrl_reg,
						&zero,
						sizeof(zero));
				if (retval < 0)
					return retval;
				pr_debug("cleared intr_mask\n");
			} else {
				retval = f54->fn_ptr->write(rmi4_data,
						f01_ctrl_reg,
						&(intr_mask[ii]),
						sizeof(intr_mask[ii]));
				if (retval < 0)
					return retval;
			}
		}
	}

	f01_ctrl_reg = rmi4_data->f01_ctrl_base_addr + 1 + f54->intr_reg_num;

	if (set) {
		retval = f54->fn_ptr->write(rmi4_data,
				f01_ctrl_reg,
				&f54->intr_mask,
				1);
		if (retval < 0)
			return retval;
		pr_debug("set intr_mask = %d\n", f54->intr_mask);
	}

	return 0;
}

#ifdef WATCHDOG_HRTIMER
static void timeout_set_status(struct work_struct *work)
{
	int retval;
	unsigned char command;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	mutex_lock(&f54->status_mutex);
	if (f54->status == STATUS_BUSY) {
		retval = f54->fn_ptr->read(rmi4_data,
				f54->command_base_addr,
				&command,
				sizeof(command));
		pr_debug("read F54_Cmd0=%d, rc = %d\n", command, retval);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read command register\n",
					__func__);
		} else if (command & COMMAND_GET_REPORT) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Report type not supported by FW\n",
					__func__);
		} else {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Get report not detected\n",
					__func__);
		}
		f54->status = -ETIMEDOUT;
		f54->report_type = INVALID_REPORT_TYPE;
		f54->report_size = 0;
		set_interrupt(false);
		wake_unlock(&f54->test_wake_lock);
	}
	mutex_unlock(&f54->status_mutex);

	return;
}

static enum hrtimer_restart get_report_timeout(struct hrtimer *timer)
{
	schedule_work(&(f54->timeout_work));

	return HRTIMER_NORESTART;
}
#endif

#ifdef RAW_HEX
static void print_raw_hex_report(void)
{
	unsigned int ii;

	pr_info("%s: Report data (raw hex)\n", __func__);

	switch (f54->report_type) {
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_HIGH_RESISTANCE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP_MIN_MAX:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
	case F54_SENSOR_SPEED:
	case F54_ADC_RANGE:
	for (ii = 0; ii < f54->report_size; ii += 2) {
		pr_info("%03d: 0x%02x%02x\n",
				ii / 2,
				f54->report_data[ii + 1],
				f54->report_data[ii]);
	}
		break;
	default:
		for (ii = 0; ii < f54->report_size; ii++)
			pr_info("%03d: 0x%02x\n", ii, f54->report_data[ii]);
		break;
	}

	return;
}
#endif

#ifdef HUMAN_READABLE
static void print_image_report(void)
{
	unsigned int ii;
	unsigned int jj;
	short *report_data;

	switch (f54->report_type) {
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_RX_COUPLING_COMP:
		pr_info("%s: Report data (image)\n", __func__);

		report_data = (short *)f54->report_data;

		for (ii = 0; ii < f54->rmi4_data->num_of_tx; ii++) {
			for (jj = 0; jj < f54->rmi4_data->num_of_rx; jj++) {
				if (*report_data < -64)
					pr_cont(".");
				else if (*report_data < 0)
					pr_cont("-");
				else if (*report_data > 64)
					pr_cont("*");
				else if (*report_data > 0)
					pr_cont("+");
				else
					pr_cont("0");

				report_data++;
			}
			pr_info("");
		}
		pr_info("%s: End of report\n", __func__);
		break;
	default:
		pr_info("%s: Image not supported for report type %d\n",
				__func__, f54->report_type);
	}

	return;
}
#endif

static void free_control_mem(void)
{
	struct f54_control control = f54->control;

	kfree(control.reg_0);
	kfree(control.reg_1);
	kfree(control.reg_2);
	kfree(control.reg_3);
	kfree(control.reg_4__6);
	kfree(control.reg_7);
	kfree(control.reg_8__9);
	kfree(control.reg_10);
	kfree(control.reg_11);
	kfree(control.reg_12__13);
	kfree(control.reg_14);
	kfree(control.reg_15->data);
	kfree(control.reg_15);
	kfree(control.reg_16->data);
	kfree(control.reg_16);
	kfree(control.reg_17->data);
	kfree(control.reg_17);
	kfree(control.reg_18->data);
	kfree(control.reg_18);
	kfree(control.reg_19->data);
	kfree(control.reg_19);
	kfree(control.reg_20);
	kfree(control.reg_21);
	kfree(control.reg_22__26);
	kfree(control.reg_27);
	kfree(control.reg_28);
	kfree(control.reg_29);
	kfree(control.reg_30);
	kfree(control.reg_31);
	kfree(control.reg_32__35);
	kfree(control.reg_36->data);
	kfree(control.reg_36);
	kfree(control.reg_37->data);
	kfree(control.reg_37);
	kfree(control.reg_38->data);
	kfree(control.reg_38);
	kfree(control.reg_39->data);
	kfree(control.reg_39);
	kfree(control.reg_40->data);
	kfree(control.reg_40);
	kfree(control.reg_89);
	kfree(control.reg_95->data);
	kfree(control.reg_95);
	kfree(control.reg_99->data);
	kfree(control.reg_99);

	return;
}

static void free_data_mem(void)
{
	struct f54_data data = f54->data;

	kfree(data.reg_4);
	kfree(data.reg_6);
	kfree(data.reg_7_0);
	kfree(data.reg_7_1);
	kfree(data.reg_8);
	kfree(data.reg_9);
	kfree(data.reg_10);
	kfree(data.reg_14);
	kfree(data.reg_16);
	kfree(data.reg_17);
}

static void remove_sysfs(void)
{
	int reg_num;

	sysfs_remove_bin_file(f54->attr_dir, &dev_report_data);

	sysfs_remove_group(f54->attr_dir, &attr_group);

	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_ctrl_regs); reg_num++)
		sysfs_remove_group(f54->attr_dir, &attrs_ctrl_regs[reg_num]);

	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_data_regs); reg_num++)
		sysfs_remove_group(f54->attr_dir, &attrs_data_regs[reg_num]);

	kobject_put(f54->attr_dir);

	return;
}

static int synaptics_rmi4_f54_reset(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	mutex_lock(&f54->status_mutex);

	rmi4_data->irq_enable(rmi4_data, false);

	retval = rmi4_data->reset_device(rmi4_data);

	rmi4_data->irq_enable(rmi4_data, true);

	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	f54->status = STATUS_IDLE;

	mutex_unlock(&f54->status_mutex);

	return 0;
}

static ssize_t synaptics_rmi4_f54_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_f54_reset();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t synaptics_rmi4_f54_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", f54->status);
}

static ssize_t synaptics_rmi4_f54_report_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->report_size);
}

static ssize_t synaptics_rmi4_f54_num_of_mapped_rx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->rmi4_data->num_of_rx);
}

static ssize_t synaptics_rmi4_f54_num_of_mapped_tx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->rmi4_data->num_of_tx);
}

static ssize_t synaptics_rmi4_f54_no_auto_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->no_auto_cal);
}

static ssize_t synaptics_rmi4_f54_no_auto_cal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char data;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting > 1)
		return -EINVAL;

	retval = f54->fn_ptr->read(rmi4_data,
			f54->control_base_addr,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read control register\n",
				__func__);
		return retval;
	}

	if ((data & NO_AUTO_CAL_MASK) == setting)
		return count;

	data = (data & ~NO_AUTO_CAL_MASK) | (data & NO_AUTO_CAL_MASK);

	retval = f54->fn_ptr->write(rmi4_data,
			f54->control_base_addr,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write control register\n",
				__func__);
		return retval;
	}

	f54->no_auto_cal = (setting == 1);

	return count;
}

static ssize_t synaptics_rmi4_f54_report_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", f54->report_type);
}

static int synaptics_rmi4_f54_report_type_set(enum f54_report_types report_type)
{
	unsigned char data;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	int retval, report_type_valid = is_report_type_valid(report_type);

	if (!report_type_valid) {
		f54->report_type = INVALID_REPORT_TYPE;
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Requested report type unsupported\n", __func__);
		return -EINVAL;
	}

	f54->report_type = report_type;
	pr_debug("requested report type = %d\n", report_type);

	mutex_lock(&f54->status_mutex);
	f54->report_type = report_type;
	data = (unsigned char)report_type;
	retval = f54->fn_ptr->write(rmi4_data,
			f54->data_base_addr,
			&data,
			sizeof(data));
	mutex_unlock(&f54->status_mutex);

	if (retval < 0)
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to write report type\n", __func__);
	return 0;
}

static ssize_t synaptics_rmi4_f54_report_type_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (f54->status != STATUS_BUSY) {
		retval = synaptics_rmi4_f54_report_type_set(
			(enum f54_report_types)setting);

		if (retval < 0)
			return retval;

		return count;
	} else {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Previous get report still ongoing\n",
				__func__);
		return -EINVAL;
	}
}

static ssize_t synaptics_rmi4_f54_user_report_type1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->user_report_type1);
}

static ssize_t synaptics_rmi4_f54_user_report_type2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->user_report_type2);
}

static ssize_t synaptics_rmi4_f54_fifoindex_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char data[2];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = f54->fn_ptr->read(rmi4_data,
			f54->data_base_addr + DATA_REPORT_INDEX_OFFSET,
			data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read data registers\n",
				__func__);
		return retval;
	}

	batohs(&f54->fifoindex, data);

	return snprintf(buf, PAGE_SIZE, "%u\n", f54->fifoindex);
}
static ssize_t synaptics_rmi4_f54_fifoindex_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char data[2];
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	f54->fifoindex = setting;

	hstoba(data, (unsigned short)setting);

	retval = f54->fn_ptr->write(rmi4_data,
			f54->data_base_addr + DATA_REPORT_INDEX_OFFSET,
			data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write data registers\n",
				__func__);
		return retval;
	}

	return count;
}

#if defined(REPORT_STATUS_POLLING)
static int test_wait_for_command_completion(void)
{
	int retval;
	unsigned char value;
	unsigned char timeout_count;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	timeout_count = 0;
	do {
		retval = f54->fn_ptr->read(rmi4_data,
				f54->command_base_addr,
				&value,
				sizeof(value));
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to read command register\n",
					__func__);
			return retval;
		}

		if (value == 0x00)
			break;

		msleep(100);
		timeout_count++;
	} while (timeout_count < STATUS_WORK_INTERVAL);

	if (timeout_count == STATUS_WORK_INTERVAL) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Timed out waiting for command completion\n",
				__func__);
		return -ETIMEDOUT;
	}

	return 0;
}
#endif

ssize_t send_get_report_command(void)
{
	int retval;
	unsigned char command = (unsigned char)COMMAND_GET_REPORT;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	mutex_lock(&f54->status_mutex);

	if (f54->status != STATUS_IDLE) {
		if (f54->status != STATUS_BUSY) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Invalid status (%d)\n",
					__func__, f54->status);
		} else {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: Previous get report still ongoing\n",
					__func__);
		}
		mutex_unlock(&f54->status_mutex);
		return -EBUSY;
	}

	wake_lock(&f54->test_wake_lock);
#if !defined(REPORT_STATUS_POLLING)
	set_interrupt(true);
#endif
	f54->status = STATUS_BUSY;

	retval = f54->fn_ptr->write(rmi4_data,
			f54->command_base_addr,
			&command,
			sizeof(command));

	/* unlock status mutex to allow jumping to error_exit label */
	mutex_unlock(&f54->status_mutex);

	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write get report command\n",
				__func__);
		goto error_exit;
	}

#if defined(REPORT_STATUS_POLLING)
	/* if report status control method is polling */
	pr_debug("will be polling test report status\n");
	retval = test_wait_for_command_completion();
	if (retval < 0)
		goto error_exit;

	/* invoke status work immediately */
	queue_delayed_work(f54->status_workqueue, &f54->status_work, 0);
#else
#ifdef WATCHDOG_HRTIMER
	hrtimer_start(&f54->watchdog,
			ktime_set(WATCHDOG_TIMEOUT_S, 0),
			HRTIMER_MODE_REL);
#endif
#endif
out:
	return retval;

error_exit:
	mutex_lock(&f54->status_mutex);
#if !defined(REPORT_STATUS_POLLING)
	set_interrupt(false);
#endif
	wake_unlock(&f54->test_wake_lock);
	f54->status = retval;
	mutex_unlock(&f54->status_mutex);

	goto out;
}

static ssize_t synaptics_rmi4_f54_get_report_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	if (!is_report_type_valid(f54->report_type)) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Invalid report type\n",
				__func__);
		return -EINVAL;
	}

	retval = send_get_report_command();
	if (retval < 0)
		return retval;

	return count;
}

static ssize_t synaptics_rmi4_f54_force_update_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char data[1];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = f54->fn_ptr->read(rmi4_data,
			f54->command_base_addr,
			data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read command 0 register\n",
				__func__);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
		(data[0] & COMMAND_FORCE_UPDATE) == COMMAND_FORCE_UPDATE);
}

static ssize_t synaptics_rmi4_f54_force_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char command;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return count;

	command = (unsigned char)COMMAND_FORCE_UPDATE;

	if (f54->status == STATUS_BUSY)
		return -EBUSY;

	retval = f54->fn_ptr->write(rmi4_data,
			f54->command_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write force update command\n",
				__func__);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f54_enter_in_cell_test_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char data[1];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = f54->fn_ptr->read(rmi4_data,
			f54->command_base_addr,
			data,
			sizeof(data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read command 0 register\n",
				__func__);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
		(data[0] & COMMAND_ENTER_IN_CELL_TESTMODE) ==
			COMMAND_ENTER_IN_CELL_TESTMODE);
}


static ssize_t synaptics_rmi4_f54_enter_in_cell_test_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char command;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return count;

	command = (unsigned char)COMMAND_ENTER_IN_CELL_TESTMODE;

	if (f54->status == STATUS_BUSY)
		return -EBUSY;

	retval = f54->fn_ptr->write(rmi4_data,
			f54->command_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
		"%s: Failed to write Enter In-Cell Test Mode command\n",
			__func__);
		return retval;
	}

	return count;
}


static ssize_t synaptics_rmi4_f54_force_cal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char command;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return count;

	command = (unsigned char)COMMAND_FORCE_CAL;

	if (f54->status == STATUS_BUSY)
		return -EBUSY;

	retval = f54->fn_ptr->write(rmi4_data,
			f54->command_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write force cal command\n",
				__func__);
		return retval;
	}

	return count;
}

simple_show_func_unsigned(query, num_of_rx_electrodes)
simple_show_func_unsigned(query, num_of_tx_electrodes)
simple_show_func_unsigned(query, has_image16)
simple_show_func_unsigned(query, has_image8)
simple_show_func_unsigned(query, has_baseline)
simple_show_func_unsigned(query, clock_rate)
simple_show_func_unsigned(query, touch_controller_family)
simple_show_func_unsigned(query, has_pixel_touch_threshold_adjustment)
simple_show_func_unsigned(query, has_sensor_assignment)
simple_show_func_unsigned(query, has_interference_metric)
simple_show_func_unsigned(query, has_sense_frequency_control)
simple_show_func_unsigned(query, has_firmware_noise_mitigation)
simple_show_func_unsigned(query, has_two_byte_report_rate)
simple_show_func_unsigned(query, has_one_byte_report_rate)
simple_show_func_unsigned(query, has_relaxation_control)
simple_show_func_unsigned(query, curve_compensation_mode)
simple_show_func_unsigned(query, has_iir_filter)
simple_show_func_unsigned(query, has_cmn_removal)
simple_show_func_unsigned(query, has_cmn_maximum)
simple_show_func_unsigned(query, has_touch_hysteresis)
simple_show_func_unsigned(query, has_edge_compensation)
simple_show_func_unsigned(query, has_per_frequency_noise_control)
simple_show_func_unsigned(query, has_enhanced_stretch)
simple_show_func_unsigned(query, has_force_fast_relaxation)
simple_show_func_unsigned(query, has_multi_metric_state_machine)
simple_show_func_unsigned(query, has_signal_clarity)
simple_show_func_unsigned(query, has_variance_metric)
simple_show_func_unsigned(query, has_0d_relaxation_control)
simple_show_func_unsigned(query, has_0d_acquisition_control)
simple_show_func_unsigned(query, has_status)
simple_show_func_unsigned(query, has_slew_metric)
simple_show_func_unsigned(query, has_h_blank)
simple_show_func_unsigned(query, has_v_blank)
simple_show_func_unsigned(query, has_long_h_blank)
simple_show_func_unsigned(query, has_startup_fast_relaxation)
simple_show_func_unsigned(query, has_esd_control)
simple_show_func_unsigned(query, has_noise_mitigation2)
simple_show_func_unsigned(query, has_noise_state)
simple_show_func_unsigned(query, has_energy_ratio_relaxation)
simple_show_func_unsigned(query12, number_of_sensing_frequencies)
simple_show_func_unsigned(query16, has_data17)
simple_show_func_unsigned(query27, has_ctrl113)
simple_show_func_unsigned(query36, has_ctrl145)
simple_show_func_unsigned(query36, has_ctrl146)
simple_show_func_unsigned(query17, q17_num_of_sense_freqs)
show_store_func_unsigned(data, reg_4, d4_inhibit_freq_shift)
show_store_func_unsigned(data, reg_4, d4_baseline_sel)
show_store_func_unsigned(data, reg_4, d4_sense_freq_sel)
show_store_func_unsigned(data, reg_16, d16_freq_scan_im_lsb)
show_store_func_unsigned(data, reg_16, d16_freq_scan_im_msb)
show_store_func_unsigned(data, reg_17, d17_inhibit_freq_shift)
show_store_func_unsigned(data, reg_17, d17_freq)
show_func_unsigned(data, reg_6, d6_interference_metric_lsb)
show_func_unsigned(data, reg_6, d6_interference_metric_msb)
show_func_unsigned(data, reg_7_0, d7_current_report_rate_lsb)
show_func_unsigned(data, reg_7_1, d7_current_report_rate_msb)
show_func_unsigned(data, reg_8, d8_variance_metric_lsb)
show_func_unsigned(data, reg_8, d8_variance_metric_msb)
show_func_unsigned(data, reg_9, d9_averaged_im_lsb)
show_func_unsigned(data, reg_9, d9_averaged_im_msb)
show_func_unsigned(data, reg_10, d10_noise_state)
show_func_unsigned(data, reg_14, d14_cid_im_lsb)
show_func_unsigned(data, reg_14, d14_cid_im_msb)

show_store_func_unsigned(control, reg_0, no_relax)
show_store_func_unsigned(control, reg_0, no_scan)
show_store_func_unsigned(control, reg_1, bursts_per_cluster)
show_store_func_unsigned(control, reg_2, saturation_cap)
show_store_func_unsigned(control, reg_3, pixel_touch_threshold)
show_store_func_unsigned(control, reg_4__6, rx_feedback_cap)
show_store_func_unsigned(control, reg_4__6, low_ref_cap)
show_store_func_unsigned(control, reg_4__6, low_ref_feedback_cap)
show_store_func_unsigned(control, reg_4__6, low_ref_polarity)
show_store_func_unsigned(control, reg_4__6, high_ref_cap)
show_store_func_unsigned(control, reg_4__6, high_ref_feedback_cap)
show_store_func_unsigned(control, reg_4__6, high_ref_polarity)
show_store_func_unsigned(control, reg_7, cbc_cap)
show_store_func_unsigned(control, reg_7, cbc_polarity)
show_store_func_unsigned(control, reg_7, cbc_tx_carrier_selection)
show_store_func_unsigned(control, reg_8__9, integration_duration)
show_store_func_unsigned(control, reg_8__9, reset_duration)
show_store_func_unsigned(control, reg_10, noise_sensing_bursts_per_image)
show_store_func_unsigned(control, reg_12__13, slow_relaxation_rate)
show_store_func_unsigned(control, reg_12__13, fast_relaxation_rate)
show_store_func_unsigned(control, reg_14, rxs_on_xaxis)
show_store_func_unsigned(control, reg_14, curve_comp_on_txs)
show_store_func_unsigned(control, reg_20, disable_noise_mitigation)
show_store_func_unsigned(control, reg_21, freq_shift_noise_threshold)
show_store_func_unsigned(control, reg_22__26, medium_noise_threshold)
show_store_func_unsigned(control, reg_22__26, high_noise_threshold)
show_store_func_unsigned(control, reg_22__26, noise_density)
show_store_func_unsigned(control, reg_22__26, frame_count)
show_store_func_unsigned(control, reg_27, iir_filter_coef)
show_store_func_unsigned(control, reg_28, quiet_threshold)
show_store_func_unsigned(control, reg_29, cmn_filter_disable)
show_store_func_unsigned(control, reg_30, cmn_filter_max)
show_store_func_unsigned(control, reg_31, touch_hysteresis)
show_store_func_unsigned(control, reg_32__35, rx_low_edge_comp)
show_store_func_unsigned(control, reg_32__35, rx_high_edge_comp)
show_store_func_unsigned(control, reg_32__35, tx_low_edge_comp)
show_store_func_unsigned(control, reg_32__35, tx_high_edge_comp)

show_replicated_func_unsigned(control, reg_15, sensor_rx_assignment)
show_replicated_func_unsigned(control, reg_16, sensor_tx_assignment)
show_replicated_func_unsigned(control, reg_17, disable)
show_replicated_func_unsigned(control, reg_17, filter_bandwidth)
show_replicated_func_unsigned(control, reg_19, stretch_duration)
show_replicated_func_unsigned(control, reg_38, noise_control_1)
show_replicated_func_unsigned(control, reg_39, noise_control_2)
show_replicated_func_unsigned(control, reg_40, noise_control_3)

show_store_replicated_func_unsigned(control, reg_36, axis1_comp)
show_store_replicated_func_unsigned(control, reg_37, axis2_comp)

show_store_subpkt_func_unsigned(control, reg_89, 0, c89_cid_sel_opt)
show_store_subpkt_func_unsigned(control, reg_89, 0, c89_cid_voltage_sel)
show_store_subpkt_func_unsigned(control, reg_89, 0,
					c89_cid_im_noise_threshold_lsb)
show_store_subpkt_func_unsigned(control, reg_89, 0,
					c89_cid_im_noise_threshold_msb)

show_store_subpkt_func_unsigned(control, reg_89, 1, c89_fnm_pixel_touch_mult)
show_store_subpkt_func_unsigned(control, reg_89, 1, c89_freq_scan_threshold_lsb)
show_store_subpkt_func_unsigned(control, reg_89, 1, c89_freq_scan_threshold_msb)
show_store_subpkt_func_unsigned(control, reg_89, 1, c89_quiet_im_threshold_lsb)
show_store_subpkt_func_unsigned(control, reg_89, 1, c89_quiet_im_threshold_msb)

show_store_subpkt_func_unsigned(control, reg_89, 2, c89_rail_sel_opt);
show_store_subpkt_func_unsigned(control, reg_89, 2,
						c89_rail_im_noise_threshold_lsb);
show_store_subpkt_func_unsigned(control, reg_89, 2,
						c89_rail_im_noise_threshold_msb);
show_store_subpkt_func_unsigned(control, reg_89, 2,
						c89_rail_im_quiet_threshold_lsb);
show_store_subpkt_func_unsigned(control, reg_89, 2,
						c89_rail_im_quiet_threshold_msb);

show_store_func_unsigned(control, reg_93, c93_freq_shift_noise_threshold_lsb)
show_store_func_unsigned(control, reg_93, c93_freq_shift_noise_threshold_msb)

show_store_replicated_func_unsigned(control, reg_95, c95_disable)
show_store_replicated_func_unsigned(control, reg_95, c95_filter_bw)
show_store_replicated_func_unsigned(control, reg_95, c95_first_burst_length_lsb)
show_store_replicated_func_unsigned(control, reg_95, c95_first_burst_length_msb)
show_store_replicated_func_unsigned(control, reg_95, c95_addl_burst_length_lsb)
show_store_replicated_func_unsigned(control, reg_95, c95_addl_burst_length_msb)
show_store_replicated_func_unsigned(control, reg_95, c95_i_stretch)
show_store_replicated_func_unsigned(control, reg_95, c95_r_stretch)
show_store_replicated_func_unsigned(control, reg_95, c95_noise_control1)
show_store_replicated_func_unsigned(control, reg_95, c95_noise_control2)
show_store_replicated_func_unsigned(control, reg_95, c95_noise_control3)
show_store_replicated_func_unsigned(control, reg_95, c95_noise_control4)

show_store_func_unsigned(control, reg_99, c99_int_dur_lsb)
show_store_func_unsigned(control, reg_99, c99_int_dur_msb)
show_store_func_unsigned(control, reg_99, c99_reset_dur)

show_store_func_unsigned(control, reg_107, c107_abs_int_dur)
show_store_func_unsigned(control, reg_107, c107_abs_reset_dur)
show_store_func_unsigned(control, reg_107, c107_abs_filter_bw)
show_store_func_unsigned(control, reg_107, c107_abs_rstretch)
show_store_func_unsigned(control, reg_107, c107_abs_burst_count_1)
show_store_func_unsigned(control, reg_107, c107_abs_burst_count_2)
show_store_func_unsigned(control, reg_107, c107_abs_stretch_dur)
show_store_func_unsigned(control, reg_107, c107_abs_adc_clock_div)
show_store_func_unsigned(control, reg_107, c107_abs_sub_burtst_size)
show_store_func_unsigned(control, reg_107, c107_abs_trigger_delay)

show_store_func_unsigned(control, reg_113, c113_en_hybrid_on_tx)
show_store_func_unsigned(control, reg_113, c113_en_hybrid_on_rx)
show_store_func_unsigned(control, reg_113, c113_tx_consistency_chk)
show_store_func_unsigned(control, reg_113, c113_rx_consistency_chk)
show_store_func_unsigned(control, reg_113, c113_en_gradient_baseline)
show_store_func_unsigned(control, reg_113, c113_consistency_thresh)
show_store_func_unsigned(control, reg_113, c113_tx_obj_thresh)
show_store_func_unsigned(control, reg_113, c113_rx_obj_thresh)
show_store_func_unsigned(control, reg_113, c113_tx_thermal_update_int_lsb)
show_store_func_unsigned(control, reg_113, c113_tx_thermal_update_int_msb)
show_store_func_unsigned(control, reg_113, c113_rx_thermal_update_int_lsb)
show_store_func_unsigned(control, reg_113, c113_rx_thermal_update_int_msb)
show_store_func_unsigned(control, reg_113, c113_tx_negative_thresh)
show_store_func_unsigned(control, reg_113, c113_rx_negative_thresh)

show_store_func_unsigned(control, reg_116, c116_im_thresh)
show_store_func_unsigned(control, reg_116, c116_debounce)
show_store_func_unsigned(control, reg_116, c116_quiet_im)
show_store_func_unsigned(control, reg_116, c116_filter)

show_store_func_unsigned(control, reg_137, c137_cmnr_adjust)

show_store_func_unsigned(control, reg_145, c145_bursts_per_cluster_rx)
show_store_func_unsigned(control, reg_145, c145_bursts_per_cluster_tx)

show_store_func_unsigned(control, reg_146, c146_int_dur_lsb)
show_store_func_unsigned(control, reg_146, c146_int_dur_msb)
show_store_func_unsigned(control, reg_146, c146_reset_dur)
show_store_func_unsigned(control, reg_146, c146_filter_bandwidth)
show_store_func_unsigned(control, reg_146, c146_stretch_dur)
show_store_func_unsigned(control, reg_146, c146_burst_count1)
show_store_func_unsigned(control, reg_146, c146_burst_count2)
show_store_func_unsigned(control, reg_146, c146_stretch_dur2)
show_store_func_unsigned(control, reg_146, c146_adc_clock_div)

simple_show_func_unsigned(query_f55_0_2, f55_q2_has_single_layer_multitouch)
show_func_unsigned(control, reg_0_f55, f55_c0_receivers_on_x)
show_func_unsigned(control, reg_8_f55, f55_c8_pattern_type)

static ssize_t synaptics_rmi4_f54_burst_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	int size = 0;
	unsigned char ii;
	unsigned char *temp;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	mutex_lock(&f54->control_mutex);

	retval = f54->fn_ptr->read(rmi4_data,
			f54->control.reg_17->address,
			(unsigned char *)f54->control.reg_17->data,
			f54->control.reg_17->length);
	if (retval < 0) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Failed to read control reg_17\n",
				__func__);
	}

	retval = f54->fn_ptr->read(rmi4_data,
			f54->control.reg_18->address,
			(unsigned char *)f54->control.reg_18->data,
			f54->control.reg_18->length);
	if (retval < 0) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Failed to read control reg_18\n",
				__func__);
	}

	mutex_unlock(&f54->control_mutex);

	temp = buf;

	for (ii = 0; ii < f54->control.reg_17->length; ii++) {
		retval = snprintf(temp, PAGE_SIZE - size, "%u ", (1 << 8) *
			f54->control.reg_17->data[ii].burst_count_b8__10 +
			f54->control.reg_18->data[ii].burst_count_b0__7);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Faild to write output\n",
					__func__);
			return retval;
		}
		size += retval;
		temp += retval;
	}

	retval = snprintf(temp, PAGE_SIZE - size, "\n");
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Faild to write null terminator\n",
				__func__);
		return retval;
	}

	return size + retval;
}

static ssize_t synaptics_rmi4_f54_user_get_report_show(
	char *buf, enum f54_report_types report_type)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	if (f54->status != STATUS_IDLE) {
		if (f54->status == STATUS_BUSY)
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: WARNING: Resetting in busy state\n",
				__func__);

		retval = synaptics_rmi4_f54_reset();
		if (retval < 0)
			goto out;
	}

	retval = synaptics_rmi4_f54_report_type_set(report_type);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to set report type\n", __func__);
		goto out;
	}

	retval = send_get_report_command();

out:
	retval = scnprintf(buf, PAGE_SIZE, "%d\n", retval);
	return retval;
}

static ssize_t synaptics_rmi4_f54_user_get_report1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return synaptics_rmi4_f54_user_get_report_show(
		buf, f54->user_report_type1);
}

static ssize_t synaptics_rmi4_f54_user_get_report2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return synaptics_rmi4_f54_user_get_report_show(
		buf, f54->user_report_type2);
}

static ssize_t synaptics_rmi4_f54_data_read(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	mutex_lock(&f54->data_mutex);

	if (!f54->report_data || !f54->report_size) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Report type %d data not available\n",
				__func__, f54->report_type);
		mutex_unlock(&f54->data_mutex);
		return -EINVAL;
	}
	if (pos > f54->report_size || pos < 0) {
		mutex_unlock(&f54->data_mutex);
		return 0;
	}

	count = min_t(loff_t, count, f54->report_size - pos);
	memcpy(buf, f54->report_data + pos, count);
	mutex_unlock(&f54->data_mutex);
	pr_debug("copied %zu bytes to the caller's buffer\n", count);
	return count;
}

static int synaptics_rmi4_f54_set_sysfs(void)
{
	int retval;
	int reg_num;
	int dreg_num;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	f54->attr_dir = kobject_create_and_add("f54",
			&rmi4_data->i2c_client->dev.kobj);
	if (!f54->attr_dir) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to create sysfs directory\n",
			__func__);
		goto exit_1;
	}

	retval = sysfs_create_bin_file(f54->attr_dir, &dev_report_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto exit_2;
	}

	retval = sysfs_create_group(f54->attr_dir, &attr_group);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs attributes\n",
				__func__);
		goto exit_3;
	}

	for (reg_num = 0; reg_num < ARRAY_SIZE(attrs_ctrl_regs); reg_num++) {
		if (attrs_ctrl_regs_exist[reg_num]) {
			retval = sysfs_create_group(f54->attr_dir,
					&attrs_ctrl_regs[reg_num]);
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to create sysfs attributes\n",
						__func__);
				goto exit_4;
			}
		}
	}

	for (dreg_num = 0; dreg_num < ARRAY_SIZE(attrs_data_regs); dreg_num++) {
		if (attrs_data_regs_exist[dreg_num]) {
			retval = sysfs_create_group(f54->attr_dir,
					&attrs_data_regs[dreg_num]);
			if (retval < 0) {
				dev_err(&rmi4_data->i2c_client->dev,
						"%s: Failed to create sysfs attributes for data register %d\n",
						__func__, dreg_num);
				goto exit_5;
			}
		}
	}

	return 0;

exit_5:
	for (dreg_num--; dreg_num >= 0; dreg_num--)
		sysfs_remove_group(f54->attr_dir, &attrs_data_regs[dreg_num]);

exit_4:
	for (reg_num--; reg_num >= 0; reg_num--)
		sysfs_remove_group(f54->attr_dir, &attrs_ctrl_regs[reg_num]);

	sysfs_remove_group(f54->attr_dir, &attr_group);

exit_3:
	sysfs_remove_bin_file(f54->attr_dir, &dev_report_data);

exit_2:
	kobject_put(f54->attr_dir);

exit_1:
	return -ENODEV;
}

/*
 * Fill in base register address and offset of F54
 * control registers to allow run time patching
 */
int synaptics_rmi4_scan_f54_ctrl_reg_info(
	struct synaptics_rmi4_func_packet_regs *f54_ctrl_regs)
{
	int ii, error = f54_ctrl_regs->nr_regs;
	unsigned char *data;
	struct synaptics_rmi4_packet_reg *reg;
	struct synaptics_rmi4_subpkt *subpkt;

	f54_ctrl_regs->base_addr = f54->control_base_addr;
	for (ii = 0; ii < f54_ctrl_regs->nr_regs; ii++) {
		reg = &f54_ctrl_regs->regs[ii];
		subpkt = &reg->subpkt[0];
		if (reg->r_number == 2 && f54->control.reg_2) {
			data = kzalloc(sizeof(f54->control.reg_2->data),
					GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->control.reg_2->address -
					f54->control_base_addr;
			reg->size = 2;
			reg->data = data;
			subpkt->present = true;
			subpkt->offset = 0;
			error--;
			continue;
		}

		if (reg->r_number == 95 && f54->control.reg_95) {
			int jj, num_of_subpkts;
			data = kzalloc(f54->control.reg_95->length, GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->control.reg_95->address -
					f54->control_base_addr;
			reg->size = f54->control.reg_95->length;
			reg->data = data;
			/* not going over the number of predefined subpackets */
			num_of_subpkts = min((int)reg->nr_subpkts,
				(int)(f54->control.reg_95->length /
				subpkt->size));
			for (jj = 0; jj < num_of_subpkts; jj++) {
				subpkt = &reg->subpkt[jj];
				subpkt->present = true;
				subpkt->offset = jj * subpkt->size;
			}
			error--;
		}
	}
	return error;
}

/*
 * Fill in base register address and offset of F54
 * command register to allow run time patching
 */
int synaptics_rmi4_scan_f54_cmd_reg_info(
	struct synaptics_rmi4_func_packet_regs *f54_cmd_regs)
{
	unsigned char *data;
	struct synaptics_rmi4_packet_reg *reg = f54_cmd_regs->regs;
	struct synaptics_rmi4_subpkt *subpkt = &reg->subpkt[0];

	f54_cmd_regs->base_addr = f54->command_base_addr;
	data = kzalloc(1, GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	reg->offset = 0;
	reg->size = 1;
	reg->data = data;
	subpkt->present = true;
	subpkt->offset = 0;
	return 0;
}

/*
  * Fill in base register address and offset of F54 query
  * register 12 to allow run time access
  */
int synaptics_rmi4_scan_f54_query_reg_info(
	struct synaptics_rmi4_func_packet_regs *f54_query_regs)
{
	struct synaptics_rmi4_packet_reg *reg = f54_query_regs->regs;
	struct synaptics_rmi4_subpkt *subpkt = &reg->subpkt[0];
	unsigned char *data = kzalloc(sizeof(f54->query12.data),
					GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	f54_query_regs->base_addr = f54->query_base_addr;
	/* need an offset off of base address here */
	reg->offset = 12;
	reg->size = sizeof(f54->query12.data);
	reg->data = data;
	subpkt->present = true;
	subpkt->offset = 0;
	return 0;
}

/*
 * Fill in base register address and offset of F54 data
 * registers 10 & 17 to allow run time access
 */
int synaptics_rmi4_scan_f54_data_reg_info(
	struct synaptics_rmi4_func_packet_regs *f54_data_regs)
{
	int ii, error = f54_data_regs->nr_regs;
	unsigned char *data;
	struct synaptics_rmi4_subpkt *subpkt;
	struct synaptics_rmi4_packet_reg *reg;

	f54_data_regs->base_addr = f54->data_base_addr;
	for (ii = 0; ii < f54_data_regs->nr_regs; ii++) {
		reg = &f54_data_regs->regs[ii];
		if (reg->r_number == 6 && f54->data.reg_6) {
			subpkt = &reg->subpkt[0];
			data = kzalloc(sizeof(f54->data.reg_6->data),
					GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->data.reg_6->address -
					f54->data_base_addr;
			reg->size = sizeof(f54->data.reg_6->data);
			reg->data = data;
			subpkt->present = true;
			subpkt->offset = 0;
			error--;
			continue;
		}

		if (reg->r_number == 10 && f54->data.reg_10) {
			subpkt = &reg->subpkt[0];
			data = kzalloc(sizeof(f54->data.reg_10->data),
					GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->data.reg_10->address -
					f54->data_base_addr;
			reg->size = sizeof(f54->data.reg_10->data);
			reg->data = data;
			subpkt->present = true;
			subpkt->offset = 0;
			error--;
			continue;
		}

		if (reg->r_number == 14 && f54->data.reg_14) {
			subpkt = &reg->subpkt[0];
			data = kzalloc(sizeof(f54->data.reg_14->data),
					GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->data.reg_14->address -
					f54->data_base_addr;
			reg->size = sizeof(f54->data.reg_14->data);
			reg->data = data;
			subpkt->present = true;
			subpkt->offset = 0;
			error--;
			continue;
		}

		if (reg->r_number == 16 && f54->data.reg_16) {
			subpkt = &reg->subpkt[0];
			data = kzalloc(sizeof(f54->data.reg_16->data),
					GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->data.reg_16->address -
					f54->data_base_addr;
			reg->size = sizeof(f54->data.reg_16->data);
			reg->data = data;
			subpkt->present = true;
			subpkt->offset = 0;
			error--;
			continue;
		}

		if (reg->r_number == 17 && f54->data.reg_17) {
			subpkt = &reg->subpkt[0];
			data = kzalloc(sizeof(f54->data.reg_17->data),
					GFP_KERNEL);
			if (!data)
				return -ENOMEM;
			/* need an offset off of base address here */
			reg->offset = f54->data.reg_17->address -
					f54->data_base_addr;
			reg->size = sizeof(f54->data.reg_17->data);
			reg->data = data;
			subpkt->present = true;
			subpkt->offset = 0;
			error--;
		}
	}
	return error ? -ENOSYS : error;
}

static int synaptics_rmi4_f54_set_ctrl(void)
{
	unsigned char length;
	unsigned char reg_num = 0;
	unsigned short reg_addr = f54->control_base_addr;
	struct f54_control *control = &f54->control;
	struct f54_query *query = &f54->query;
	struct f54_query12 *query12 = &f54->query12;
	struct f54_query13 *query13 = &f54->query13;
	struct f54_query15 *query15 = &f54->query15;
	struct f54_query16 *query16 = &f54->query16;
	struct f54_query17 *query17 = &f54->query17;
	struct f54_query21 *query21 = &f54->query21;
	struct f54_query22 *query22 = &f54->query22;
	struct f54_query23 *query23 = &f54->query23;
	struct f54_query25 *query25 = &f54->query25;
	struct f54_query27 *query27 = &f54->query27;
	struct f54_query29 *query29 = &f54->query29;
	struct f54_query30 *query30 = &f54->query30;
	struct f54_query32 *query32 = &f54->query32;
	struct f54_query33 *query33 = &f54->query33;
	struct f54_query35 *query35 = &f54->query35;
	struct f54_query36 *query36 = &f54->query36;
	struct f54_query38 *query38 = &f54->query38;
	struct f54_query40 *query40 = &f54->query40;
	struct f54_query46 *query46 = &f54->query46;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	unsigned char num_of_sensing_freqs;

	num_of_sensing_freqs = f54->query12.number_of_sensing_frequencies;

	CTRL_REG_ADD(0, 1, true);
	CTRL_REG_ADD(1, 1, ((f54->query.touch_controller_family == 0) ||
			(f54->query.touch_controller_family == 1)));
	CTRL_REG_ADD(2, 2, true);
	CTRL_REG_ADD(3, 1, (f54->query.has_pixel_touch_threshold_adjustment == 1));
	CTRL_REG_ADD(4__6, 3, ((f54->query.touch_controller_family == 0) ||
			(f54->query.touch_controller_family == 1)));
	CTRL_REG_ADD(7, 1, (f54->query.touch_controller_family == 1));
	CTRL_REG_ADD(8__9, 3, ((f54->query.touch_controller_family == 0) ||
			(f54->query.touch_controller_family == 1)));
	CTRL_REG_ADD(10, 1, f54->query.has_interference_metric);
	CTRL_REG_ADD(11, 2, f54->query.has_ctrl11);
	CTRL_REG_ADD(12__13, 2, f54->query.has_relaxation_control);

	/* controls 14 15 16 */
	if (f54->query.has_sensor_assignment == 1) {
		attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_14 = kzalloc(sizeof(*(control->reg_14)),
				GFP_KERNEL);
		if (!control->reg_14)
			goto exit_no_mem;
		control->reg_14->address = reg_addr;
		reg_addr += sizeof(control->reg_14->data);

		control->reg_15 = kzalloc(sizeof(*(control->reg_15)),
				GFP_KERNEL);
		if (!control->reg_15)
			goto exit_no_mem;
		control->reg_15->length = f54->query.num_of_rx_electrodes;
		control->reg_15->data = kzalloc(control->reg_15->length *
				sizeof(*(control->reg_15->data)), GFP_KERNEL);
		if (!control->reg_15->data)
			goto exit_no_mem;
		control->reg_15->address = reg_addr;
		reg_addr += control->reg_15->length;

		control->reg_16 = kzalloc(sizeof(*(control->reg_16)),
				GFP_KERNEL);
		if (!control->reg_16)
			goto exit_no_mem;
		control->reg_16->length = f54->query.num_of_tx_electrodes;
		control->reg_16->data = kzalloc(control->reg_16->length *
				sizeof(*(control->reg_16->data)), GFP_KERNEL);
		if (!control->reg_16->data)
			goto exit_no_mem;
		control->reg_16->address = reg_addr;
		reg_addr += control->reg_16->length;
	}
	reg_num++;

	/* controls 17 18 19 */
	if (f54->query.has_sense_frequency_control == 1) {
		attrs_ctrl_regs_exist[reg_num] = true;
		control->reg_17 = kzalloc(sizeof(*(control->reg_17)),
				GFP_KERNEL);
		if (!control->reg_17)
			goto exit_no_mem;
		control->reg_17->length = num_of_sensing_freqs;
		control->reg_17->data = kzalloc(num_of_sensing_freqs *
				sizeof(*(control->reg_17->data)), GFP_KERNEL);
		if (!control->reg_17->data)
			goto exit_no_mem;
		control->reg_17->address = reg_addr;
		reg_addr += num_of_sensing_freqs;

		control->reg_18 = kzalloc(sizeof(*(control->reg_18)),
				GFP_KERNEL);
		if (!control->reg_18)
			goto exit_no_mem;
		control->reg_18->length = num_of_sensing_freqs;
		control->reg_18->data = kzalloc(num_of_sensing_freqs *
				sizeof(*(control->reg_18->data)), GFP_KERNEL);
		if (!control->reg_18->data)
			goto exit_no_mem;
		control->reg_18->address = reg_addr;
		reg_addr += num_of_sensing_freqs;

		control->reg_19 = kzalloc(sizeof(*(control->reg_19)),
				GFP_KERNEL);
		if (!control->reg_19)
			goto exit_no_mem;
		control->reg_19->length = num_of_sensing_freqs;
		control->reg_19->data = kzalloc(num_of_sensing_freqs *
				sizeof(*(control->reg_19->data)), GFP_KERNEL);
		if (!control->reg_19->data)
			goto exit_no_mem;
		control->reg_19->address = reg_addr;
		reg_addr += num_of_sensing_freqs;
	}
	reg_num++;

	CTRL_REG_ADD(20, 1, true);
	CTRL_REG_ADD(21, 2, f54->query.has_sense_frequency_control);
	CTRL_REG_ADD(22__26, 7, f54->query.has_firmware_noise_mitigation);
	CTRL_REG_ADD(27, 1, f54->query.has_iir_filter);
	CTRL_REG_ADD(28, 2, f54->query.has_firmware_noise_mitigation);
	CTRL_REG_ADD(29, 1, f54->query.has_cmn_removal);
	CTRL_REG_ADD(30, 1, f54->query.has_cmn_maximum);
	CTRL_REG_ADD(31, 1, f54->query.has_touch_hysteresis);
	CTRL_REG_ADD(32__35, 8, f54->query.has_edge_compensation);

	/* control 36 */
	if ((f54->query.curve_compensation_mode == 1) ||
			(f54->query.curve_compensation_mode == 2)) {
		attrs_ctrl_regs_exist[reg_num] = true;

		if (f54->query.curve_compensation_mode == 1) {
			length = max(f54->query.num_of_rx_electrodes,
					f54->query.num_of_tx_electrodes);
		} else if (f54->query.curve_compensation_mode == 2) {
			length = f54->query.num_of_rx_electrodes;
		}

		control->reg_36 = kzalloc(sizeof(*(control->reg_36)),
				GFP_KERNEL);
		if (!control->reg_36)
			goto exit_no_mem;
		control->reg_36->length = length;
		control->reg_36->data = kzalloc(length *
				sizeof(*(control->reg_36->data)), GFP_KERNEL);
		if (!control->reg_36->data)
			goto exit_no_mem;
		control->reg_36->address = reg_addr;
		reg_addr += length;
	}
	reg_num++;

	/* control 37 */
	if (f54->query.curve_compensation_mode == 2) {
		attrs_ctrl_regs_exist[reg_num] = true;

		control->reg_37 = kzalloc(sizeof(*(control->reg_37)),
				GFP_KERNEL);
		if (!control->reg_37)
			goto exit_no_mem;
		control->reg_37->length = f54->query.num_of_tx_electrodes;
		control->reg_37->data = kzalloc(control->reg_37->length *
				sizeof(*(control->reg_37->data)), GFP_KERNEL);
		if (!control->reg_37->data)
			goto exit_no_mem;

		control->reg_37->address = reg_addr;
		reg_addr += control->reg_37->length;
	}
	reg_num++;

	/* controls 38 39 40 */
	if (f54->query.has_per_frequency_noise_control == 1) {
		attrs_ctrl_regs_exist[reg_num] = true;

		control->reg_38 = kzalloc(sizeof(*(control->reg_38)),
				GFP_KERNEL);
		if (!control->reg_38)
			goto exit_no_mem;
		control->reg_38->length = num_of_sensing_freqs;

		control->reg_38->data = kzalloc(num_of_sensing_freqs *
				sizeof(*(control->reg_38->data)), GFP_KERNEL);
		if (!control->reg_38->data)
			goto exit_no_mem;
		control->reg_38->address = reg_addr;
		reg_addr += num_of_sensing_freqs;

		control->reg_39 = kzalloc(sizeof(*(control->reg_39)),
				GFP_KERNEL);
		if (!control->reg_39)
			goto exit_no_mem;
		control->reg_39->length = num_of_sensing_freqs;

		control->reg_39->data = kzalloc(num_of_sensing_freqs *
				sizeof(*(control->reg_39->data)), GFP_KERNEL);
		if (!control->reg_39->data)
			goto exit_no_mem;
		control->reg_39->address = reg_addr;
		reg_addr += num_of_sensing_freqs;

		control->reg_40 = kzalloc(sizeof(*(control->reg_40)),
				GFP_KERNEL);
		if (!control->reg_40)
			goto exit_no_mem;
		control->reg_40->length = num_of_sensing_freqs;

		control->reg_40->data = kzalloc(num_of_sensing_freqs *
				sizeof(*(control->reg_40->data)), GFP_KERNEL);
		if (!control->reg_40->data)
			goto exit_no_mem;
		control->reg_40->address = reg_addr;
		reg_addr += num_of_sensing_freqs;
	}
	reg_num++;

	CTRL_REG_PRESENCE(41, 1, query->has_signal_clarity);
	CTRL_REG_PRESENCE(42, 2, query->has_variance_metric);
	CTRL_REG_PRESENCE(43, 2, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(44, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(45, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(46, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(47, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(48, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(49, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(50, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(51, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(52, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(53, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(53, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(54, 1, query->has_multi_metric_state_machine);
	CTRL_REG_PRESENCE(55, 1, query->has_0d_relaxation_control);
	CTRL_REG_PRESENCE(56, 1, query->has_0d_relaxation_control);
	CTRL_REG_PRESENCE(57, 1, query->has_0d_acquisition_control);
	CTRL_REG_PRESENCE(58, 1, query->has_0d_acquisition_control);
	CTRL_REG_PRESENCE(59, 2, query->has_h_blank);
	CTRL_REG_PRESENCE(60, 1, query->has_h_blank || query->has_v_blank ||
				query->has_long_h_blank);
	CTRL_REG_PRESENCE(61, 1, query->has_h_blank || query->has_v_blank ||
				query->has_long_h_blank);
	CTRL_REG_PRESENCE(62, 1, query->has_h_blank || query->has_v_blank ||
				query->has_long_h_blank);
	CTRL_REG_PRESENCE(63, 1, query->has_h_blank || query->has_v_blank ||
				query->has_long_h_blank ||
				query->has_slew_metric ||
				query->has_slew_option ||
				query->has_noise_mitigation2);

	if (query->has_h_blank) {
		CTRL_REG_PRESENCE(64, 7, 1);
		CTRL_REG_PRESENCE(65, 7, 1);
	} else if (query->has_v_blank || query->has_long_h_blank) {
		CTRL_REG_PRESENCE(64, 1, 1);
		CTRL_REG_PRESENCE(65, 1, 1);
	}

	if (query->has_h_blank || query->has_v_blank ||
		query->has_long_h_blank) {
		CTRL_REG_PRESENCE(66, 1, 1);
		CTRL_REG_PRESENCE(67, 1, 1);
		CTRL_REG_PRESENCE(68, 1, 1);
		CTRL_REG_PRESENCE(69, 1, 1);
		CTRL_REG_PRESENCE(70, 1, 1);
		CTRL_REG_PRESENCE(71, 1, 1);
		CTRL_REG_PRESENCE(72, 2, 1);
		CTRL_REG_PRESENCE(73, 2, 1);
	}
	CTRL_REG_PRESENCE(74, 2, query->has_slew_metric);
	CTRL_REG_PRESENCE(75, query12->number_of_sensing_frequencies,
				query->has_enhanced_stretch &&
				query->has_sense_frequency_control);
	CTRL_REG_PRESENCE(76, 1, query->has_startup_fast_relaxation);
	CTRL_REG_PRESENCE(77, 1, query->has_esd_control);
	CTRL_REG_PRESENCE(78, 1, query->has_esd_control);
	CTRL_REG_PRESENCE(79, 1, query->has_noise_mitigation2);
	CTRL_REG_PRESENCE(80, 1, query->has_noise_mitigation2);
	CTRL_REG_PRESENCE(81, 1, query->has_noise_mitigation2);
	CTRL_REG_PRESENCE(82, 1, query->has_noise_mitigation2);
	CTRL_REG_PRESENCE(83, 1, query->has_noise_mitigation2);
	CTRL_REG_PRESENCE(84, 1, query->has_energy_ratio_relaxation);
	CTRL_REG_PRESENCE(85, 1, query->has_energy_ratio_relaxation);
	CTRL_REG_PRESENCE(86, 1, query13->has_ctrl86);
	CTRL_REG_PRESENCE(87, 1, query13->has_ctrl87);
	CTRL_REG_PRESENCE(88, 1, query->has_ctrl88);

/*
	CTRL_REG_ADD(89, 1, query13->has_cid_im ||
				query13->has_noise_mitigation_enh ||
				query13->has_rail_im);
*/
	CTRL_PKT_REG_START(89,
		CTRL_SUBPKT_SZ(89, 0) +
		CTRL_SUBPKT_SZ(89, 1) +
		CTRL_SUBPKT_SZ(89, 2));

	CTRL_SUBPKT_ADD(89, 0, query13->has_cid_im);
	CTRL_SUBPKT_ADD(89, 1, query13->has_noise_mitigation_enh);
	CTRL_SUBPKT_ADD(89, 2, query13->has_rail_im);
	CTRL_PKT_REG_END(89, 1);

	CTRL_REG_PRESENCE(90, 1, query15->has_ctrl90);
	CTRL_REG_PRESENCE(91, 1, query21->has_ctrl91);
	CTRL_REG_PRESENCE(92, 1, query16->has_ctrl92);

	CTRL_REG_ADD(93, 1, query16->has_ctrl93);

	CTRL_REG_PRESENCE(94, 1, query16->has_ctrl94_query18);

	CTRL_REG_ADD_EXT(95, 1, query16->has_ctrl95_query19,
		sizeof(struct f54_control_95n) *
		query17->q17_num_of_sense_freqs);

	CTRL_REG_PRESENCE(96, 1, query21->has_ctrl96);
	CTRL_REG_PRESENCE(97, 1, query21->has_ctrl97);
	CTRL_REG_PRESENCE(98, 1, query21->has_ctrl98);

	CTRL_REG_ADD(99, 1, (query16->has_ctrl99 ||
		((f54->query.touch_controller_family == 2) ||
		(f54->query.touch_controller_family == 4))));

	CTRL_REG_PRESENCE(100, 1, query16->has_ctrl100);
	CTRL_REG_PRESENCE(101, 1, query22->has_ctrl101);
	CTRL_REG_PRESENCE(102, 1, query23->has_ctrl102);
	CTRL_REG_PRESENCE(103, 1, query22->has_ctrl103_query26);
	CTRL_REG_PRESENCE(104, 1, query22->has_ctrl104);
	CTRL_REG_PRESENCE(105, 1, query22->has_ctrl105);
	CTRL_REG_PRESENCE(106, 1, query25->has_ctrl106);

	CTRL_REG_ADD(107, 1, query25->has_ctrl107);

	CTRL_REG_PRESENCE(108, 1, query25->has_ctrl108);
	CTRL_REG_PRESENCE(109, 1, query25->has_ctrl109);
	CTRL_REG_PRESENCE(110, 1, query27->has_ctrl110);
	CTRL_REG_PRESENCE(111, 1, query27->has_ctrl111);
	CTRL_REG_PRESENCE(112, 1, query27->has_ctrl112);

	CTRL_REG_ADD(113, 1, query27->has_ctrl113);

	CTRL_REG_PRESENCE(114, 1, query27->has_ctrl114);
	CTRL_REG_PRESENCE(115, 1, query29->has_ctrl115);

	CTRL_REG_ADD(116, 1, query29->has_ctrl116);

	CTRL_REG_PRESENCE(117, 1, query29->has_ctrl117);
	CTRL_REG_PRESENCE(118, 1, query30->has_ctrl118);
	CTRL_REG_PRESENCE(119, 1, query30->has_ctrl119);
	CTRL_REG_PRESENCE(120, 1, query30->has_ctrl120);
	CTRL_REG_PRESENCE(121, 1, query30->has_ctrl121);
	CTRL_REG_PRESENCE(122, 1, query30->has_ctrl122_query31);
	CTRL_REG_PRESENCE(123, 1, query30->has_ctrl123);
	CTRL_REG_PRESENCE(124, 1, query30->has_ctrl124);
	CTRL_REG_PRESENCE(125, 1, query32->has_ctrl125);
	CTRL_REG_PRESENCE(126, 1, query32->has_ctrl126);
	CTRL_REG_PRESENCE(127, 1, query32->has_ctrl127);
	CTRL_REG_PRESENCE(128, 1, query33->has_ctrl128);
	CTRL_REG_PRESENCE(129, 1, query33->has_ctrl129);
	CTRL_REG_PRESENCE(130, 1, query33->has_ctrl130);
	CTRL_REG_PRESENCE(131, 1, query33->has_ctrl131);
	CTRL_REG_PRESENCE(132, 1, query33->has_ctrl132);
	CTRL_REG_PRESENCE(133, 1, query33->has_ctrl133);
	CTRL_REG_PRESENCE(134, 1, query33->has_ctrl134);
	CTRL_REG_PRESENCE(135, 1, query35->has_ctrl135);
	CTRL_REG_PRESENCE(136, 1, query35->has_ctrl136);

	CTRL_REG_ADD(137, 1, query35->has_ctrl137);

	CTRL_REG_PRESENCE(138, 1, query35->has_ctrl138);
	CTRL_REG_PRESENCE(139, 1, query35->has_ctrl139);
	CTRL_REG_PRESENCE(140, 1, query35->has_ctrl140);
	CTRL_REG_PRESENCE(141, 1, query36->has_ctrl141);
	CTRL_REG_PRESENCE(142, 1, query36->has_ctrl142);
	CTRL_REG_PRESENCE(143, 1, query36->has_ctrl143);
	CTRL_REG_PRESENCE(144, 1, query36->has_ctrl144);

	CTRL_REG_ADD(145, 1, query36->has_ctrl145);
	CTRL_REG_ADD(146, 1, query36->has_ctrl146);

	CTRL_REG_PRESENCE(147, 1, query38->has_ctrl147);
	CTRL_REG_PRESENCE(148, 1, query38->has_ctrl148);
	CTRL_REG_PRESENCE(149, 1, query38->has_ctrl149);
	CTRL_REG_RESERVED_PRESENCE(150, 1, 0);
	CTRL_REG_PRESENCE(151, 1, query38->has_ctrl151);
	CTRL_REG_RESERVED_PRESENCE(152, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(153, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(154, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(155, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(156, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(157, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(158, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(159, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(160, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(161, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(162, 1, 0);
	CTRL_REG_PRESENCE(163, 1, query40->has_ctrl163_query41);
	CTRL_REG_RESERVED_PRESENCE(164, 1, 0);
	CTRL_REG_PRESENCE(165, 1, query40->has_ctrl165_query42);
	CTRL_REG_RESERVED_PRESENCE(166, 1, 0);
	CTRL_REG_PRESENCE(167, 1, query40->has_ctrl167);
	CTRL_REG_RESERVED_PRESENCE(168, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(169, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(170, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(171, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(172, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(173, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(174, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(175, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(176, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(177, 1, 0);
	CTRL_REG_RESERVED_PRESENCE(178, 1, 0);
	CTRL_REG_PRESENCE(179, 1, query46->has_ctrl179);

	/* add F55 control registers */
	reg_addr = f54->fn55->control_base_addr;

	CTRL_REG_ADD(0_f55, 1, f54->query_f55_0_2.has_sensor_assignment);

	CTRL_REG_PRESENCE(1_f55, 1, f54->query_f55_0_2.has_sensor_assignment);
	CTRL_REG_PRESENCE(2_f55, 1, f54->query_f55_0_2.has_sensor_assignment);
	CTRL_REG_PRESENCE(3_f55, 1, f54->query_f55_0_2.has_edge_compensation);
	CTRL_REG_PRESENCE(4_f55, 1, f54->query_f55_0_2.curve_compensation_mode);
	CTRL_REG_PRESENCE(5_f55, 1, f54->query_f55_0_2.curve_compensation_mode);
	CTRL_REG_PRESENCE(6_f55, 1, f54->query_f55_0_2.has_ctrl6);
	CTRL_REG_PRESENCE(7_f55, 1,
		f54->query_f55_0_2.has_alternate_tx_assignment);

	CTRL_REG_ADD(8_f55, 1,
		(f54->query_f55_0_2.f55_q2_has_single_layer_multitouch &&
		f54->query_f55_3.f55_q3_has_ctrl8));
	return 0;

exit_no_mem:
	dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to alloc mem for control registers\n",
			__func__);
	return -ENOMEM;
}

static int synaptics_rmi4_f54_set_data(void)
{
	unsigned char reg_num = 0;
	unsigned short reg_addr = f54->data_base_addr;
	struct f54_data *data = &f54->data;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	/* data 0 - data 3: skip mandatory registers */
	reg_addr += 4;

	/* data 4 */
	DATA_REG_ADD(4, 1, f54->query.has_sense_frequency_control);

	/* F54_ANALOG_Data5 (reserved) is not present */
	/* - do not increment */

	/* data 6 */
	DATA_REG_ADD(6, 2, f54->query.has_interference_metric);
	/* data 7.0 */
	DATA_REG_ADD(7_0, 1, (f54->query.has_one_byte_report_rate ||
		f54->query.has_two_byte_report_rate));
	/* data 7.1 */
	DATA_REG_ADD(7_1, 1, f54->query.has_two_byte_report_rate);
	/* data 8 */
	DATA_REG_ADD(8, 2, f54->query.has_variance_metric);
	/* data 9 */
	DATA_REG_ADD(9, 2, f54->query.has_multi_metric_state_machine);
	/* data 10 */
	DATA_REG_ADD(10, 1, (f54->query.has_multi_metric_state_machine ||
		f54->query.has_noise_state));

	/* data 11 */
	DATA_REG_PRESENCE(11, 1, f54->query.has_status);
	/* data 12 */
	DATA_REG_PRESENCE(12, 2, f54->query.has_slew_metric);
	/* data 13 */
	DATA_REG_PRESENCE(13, 2, f54->query.has_multi_metric_state_machine);

	/* data 14 */
	DATA_REG_ADD(14, 1, f54->query13.has_cid_im);

	/* data 15 */
	DATA_REG_PRESENCE(15, 1, f54->query13.has_rail_im);

	/* data 16 */
	DATA_REG_ADD(16, 1, f54->query13.has_noise_mitigation_enh);
	/* data 17 */
	DATA_REG_ADD(17, 1, f54->query16.has_data17);

	return 0;

exit_no_mem:
	dev_err(&rmi4_data->i2c_client->dev,
			"%s: Failed to alloc mem for data registers\n",
			__func__);
	return -ENOMEM;
}

static int synaptics_rmi4_f55_read_query(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	uint16_t reg_addr;

	retval = f54->fn_ptr->read(rmi4_data,
			f54->fn55->query_base_addr,
			f54->query_f55_0_2.data,
			sizeof(f54->query_f55_0_2.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read F55 query registers 0-2\n",
				__func__);
		goto err;
	}

	reg_addr  = f54->fn55->query_base_addr;

	QUERY_REG_READ(_f55_0_2, 1);
	QUERY_REG_READ(_f55_3,
		f54->query_f55_0_2.f55_q2_has_single_layer_multitouch);
return 0;

err:
	return retval;
}

static void synaptics_rmi4_f54_sensor_mapping(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct f54_control *control = &f54->control;
	unsigned int rx_len;
	unsigned int tx_len;
	unsigned char *buffer;
	unsigned char *rx_buffer;
	unsigned char *tx_buffer;
	unsigned short offset;
	int i;

	rmi4_data->num_of_rx = 0;
	rmi4_data->num_of_tx = 0;

	if (f54->query.has_sensor_assignment == 1) {
		rx_len = control->reg_15->length;
		tx_len = control->reg_16->length;
		offset = control->reg_15->address;

	} else if (f54->fn55) {
		if (f54->query_f55_0_2.has_sensor_assignment) {
			rx_len = f54->query_f55_0_2.num_of_rx_electrodes;
			tx_len = f54->query_f55_0_2.num_of_tx_electrodes;

			offset = f54->fn55->control_base_addr + 1;
		} else {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Has sensor assignment is not set in F55\n",
					__func__);
			return;
		}
	} else {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Cannot find sensor mapping\n",
				__func__);
		return;
	}

	buffer = kzalloc((rx_len +  tx_len) * sizeof(unsigned char),
				GFP_KERNEL);

	retval = f54->fn_ptr->read(rmi4_data, offset, buffer, rx_len + tx_len);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read control registers\n",
				__func__);
		goto exit;
	}
	rx_buffer = buffer;
	tx_buffer = buffer + rx_len;

	for (i = 0; i < rx_len; i++) {
		if (rx_buffer[i] != 0xFF)
			rmi4_data->num_of_rx++;
	}
	for (i = 0; i < tx_len; i++) {
		if (tx_buffer[i] != 0xFF)
			rmi4_data->num_of_tx++;
	}

	dev_info(&rmi4_data->i2c_client->dev,
				"RxTx mapped from F$%s\n" \
				"\n\tRx mapped count %d(%d)\n\tTx \
				mapped count %d(%d)\n\n",
				f54->fn55 ? "55" : "54",
				rmi4_data->num_of_rx,
				rx_len,
				rmi4_data->num_of_tx,
				tx_len);
exit:
	kfree(buffer);
}

static void synaptics_rmi4_f54_status_work(struct work_struct *work)
{
	int retval;
	unsigned char report_index[2];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	pr_debug("enter; status = %d\n", f54->status);
	if (f54->status != STATUS_BUSY) {
		retval = STATUS_ERROR;
		goto error_exit;
	}

	set_report_size();
	pr_debug("report_size = %d\n", f54->report_size);
	if (f54->report_size == 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Report data size = 0\n",
				__func__);
		retval = -EINVAL;
		goto error_exit;
	}

	if (f54->data_buffer_size < f54->report_size) {
		mutex_lock(&f54->data_mutex);
		if (f54->data_buffer_size)
			kfree(f54->report_data);
		f54->report_data = kzalloc(f54->report_size, GFP_KERNEL);
		if (!f54->report_data) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to alloc mem for data buffer\n",
					__func__);
			f54->data_buffer_size = 0;
			mutex_unlock(&f54->data_mutex);
			retval = -ENOMEM;
			goto error_exit;
		}
		f54->data_buffer_size = f54->report_size;
		mutex_unlock(&f54->data_mutex);
	}

	report_index[0] = 0;
	report_index[1] = 0;

	retval = f54->fn_ptr->write(rmi4_data,
			f54->data_base_addr + DATA_REPORT_INDEX_OFFSET,
			report_index,
			sizeof(report_index));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write report data index\n",
				__func__);
		retval = -EINVAL;
		goto error_exit;
	}

	retval = f54->fn_ptr->read(rmi4_data,
			f54->data_base_addr + DATA_REPORT_DATA_OFFSET,
			f54->report_data,
			f54->report_size);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read report data\n",
				__func__);
		retval = -EINVAL;
		goto error_exit;
	}

	pr_debug("retrieved %d bytes of test report data\n", retval);
	retval = STATUS_IDLE;

#ifdef RAW_HEX
	print_raw_hex_report();
#endif

#ifdef HUMAN_READABLE
	print_image_report();
#endif

error_exit:
	mutex_lock(&f54->status_mutex);
#if !defined(REPORT_STATUS_POLLING)
	set_interrupt(false);
#endif
	wake_unlock(&f54->test_wake_lock);
	f54->status = retval;
	mutex_unlock(&f54->status_mutex);
	pr_debug("status set to %d\n", f54->status);

	return;
}

static void synaptics_rmi4_f54_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (f54->intr_mask & intr_mask) {
		queue_delayed_work(f54->status_workqueue,
				&f54->status_work,
				msecs_to_jiffies(STATUS_WORK_INTERVAL));
	}

	return;
}

int synaptics_rmi4_f54_read_query(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	uint16_t reg_addr;

	retval = f54->fn_ptr->read(rmi4_data,
			f54->query_base_addr,
			f54->query.data,
			sizeof(f54->query.data));
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read query registers\n",
				__func__);
		goto err;
	}

	reg_addr  = f54->query_base_addr + sizeof(f54->query.data);

	QUERY_REG_READ(12, f54->query.has_sense_frequency_control);
	QUERY_REG_READ(13, f54->query.has_query13);
	QUERY_REG_READ(14, f54->query13.has_ctrl87);
	QUERY_REG_READ(15, f54->query.has_query15);
	QUERY_REG_READ(16, f54->query15.has_query16);
	QUERY_REG_READ(17, f54->query16.has_query17);
	QUERY_REG_READ(18, f54->query16.has_ctrl94_query18);
	QUERY_REG_READ(19, f54->query16.has_ctrl95_query19);
	QUERY_REG_READ(20, f54->query15.has_query20);
	QUERY_REG_READ(21, f54->query15.has_query21);
	QUERY_REG_READ(22, f54->query15.has_query22);
	QUERY_REG_READ(23, f54->query22.has_query23);
	QUERY_REG_READ(24, f54->query21.has_query24_data18);
	QUERY_REG_READ(25, f54->query15.has_query25);
	QUERY_REG_READ(26, f54->query22.has_ctrl103_query26);
	QUERY_REG_READ(27, f54->query25.has_query27);
	QUERY_REG_READ(28, f54->query22.has_query28);
	QUERY_REG_READ(29, f54->query27.has_query29);
	QUERY_REG_READ(30, f54->query29.has_query30);
	QUERY_REG_READ(31, f54->query30.has_ctrl122_query31);
	QUERY_REG_READ(32, f54->query30.has_query32);
	QUERY_REG_READ(33, f54->query32.has_query33);
	QUERY_REG_READ(34, f54->query32.has_query34);
	QUERY_REG_READ(35, f54->query32.has_query35);
	QUERY_REG_READ(36, f54->query33.has_query36);
	if (f54->query36.has_query37)
		reg_addr += 1;
	QUERY_REG_READ(38, f54->query36.has_query38);
	QUERY_REG_READ(39, f54->query38.has_query39);
	QUERY_REG_READ(40, f54->query39.has_query40);
	QUERY_REG_READ(41, f54->query40.has_ctrl163_query41);
	QUERY_REG_READ(42, f54->query40.has_ctrl165_query42);
	QUERY_REG_READ(43, f54->query40.has_query43);
	reg_addr += 1; /* query44 is reserved - always present */
	reg_addr += 1; /* query45 is reserved - always present */
	QUERY_REG_READ(46, f54->query43.has_query46);

return 0;

err:
	return retval;
}

static int synaptics_rmi4_f54_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	bool hasF54 = false;
	bool hasF55 = false;
	unsigned short ii;
	unsigned char page;
	unsigned char intr_count = 0;
	unsigned char intr_offset;

	struct synaptics_rmi4_fn_desc rmi_fd;

	f54 = kzalloc(sizeof(*f54), GFP_KERNEL);
	if (!f54) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for f54\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	f54->fn_ptr = kzalloc(sizeof(*(f54->fn_ptr)), GFP_KERNEL);
	if (!f54->fn_ptr) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fn_ptr\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_f54;
	}

	f54->rmi4_data = rmi4_data;
	f54->fn_ptr->read = rmi4_data->i2c_read;
	f54->fn_ptr->write = rmi4_data->i2c_write;
	f54->fn_ptr->enable = rmi4_data->irq_enable;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (ii = PDT_START; ii > PDT_END; ii -= PDT_ENTRY_SIZE) {
			ii |= (page << 8);

			retval = f54->fn_ptr->read(rmi4_data,
					ii,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				goto exit_free_mem;

			if (!rmi_fd.fn_number)
				break;

			if (rmi_fd.fn_number == SYNAPTICS_RMI4_F54) {
				hasF54 = true;
				f54->query_base_addr =
					rmi_fd.query_base_addr | (page << 8);
				pr_debug("query_base_addr 0x%04x\n",
					f54->query_base_addr);
				f54->control_base_addr =
					rmi_fd.ctrl_base_addr | (page << 8);
				pr_debug("ctrl_base_addr 0x%04x\n",
					f54->control_base_addr);
				f54->data_base_addr =
					rmi_fd.data_base_addr | (page << 8);
				pr_debug("data_base_addr 0x%04x\n",
					f54->data_base_addr);
				f54->command_base_addr =
					rmi_fd.cmd_base_addr | (page << 8);
				pr_debug("command_base_addr 0x%04x\n",
					f54->command_base_addr);
			} else if (rmi_fd.fn_number == SYNAPTICS_RMI4_F55) {
				hasF55 = true;
				f54->fn55 = kmalloc(sizeof(*f54->fn55),
								GFP_KERNEL);
				f54->fn55->query_base_addr =
					rmi_fd.query_base_addr | (page << 8);
				f54->fn55->control_base_addr =
					rmi_fd.ctrl_base_addr | (page << 8);
			}

			if (hasF54 && hasF55)
				goto found;

			if (!hasF54)
				intr_count +=
					(rmi_fd.intr_src_count & MASK_3BIT);
		}
	}

	if (!hasF54) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: F$54 is not available\n",
				__func__);
		goto exit;
	}
found:
	f54->intr_reg_num = (intr_count + 7) / 8;
	if (f54->intr_reg_num != 0)
		f54->intr_reg_num -= 1;

	f54->intr_mask = 0;
	intr_offset = intr_count % 8;
	for (ii = intr_offset;
			ii < ((rmi_fd.intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++) {
		f54->intr_mask |= 1 << ii;
	}

	retval = synaptics_rmi4_f54_read_query();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read query registers\n",
				__func__);
		goto exit_free_mem;
	}

	retval = synaptics_rmi4_f55_read_query();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to set up F55 query registers\n",
				__func__);
		goto exit_free_control;
	}

	retval = synaptics_rmi4_f54_set_ctrl();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to set up control registers\n",
				__func__);
		goto exit_free_control;
	}

	synaptics_rmi4_f54_sensor_mapping();

	retval = synaptics_rmi4_f54_set_data();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to set up data registers\n",
				__func__);
		goto exit_free_data;
	}

	mutex_init(&f54->status_mutex);
	mutex_init(&f54->data_mutex);
	mutex_init(&f54->control_mutex);

	retval = synaptics_rmi4_f54_set_sysfs();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs entries\n",
				__func__);
		goto exit_sysfs;
	}

	f54->status_workqueue =
			create_singlethread_workqueue("f54_status_workqueue");
	INIT_DELAYED_WORK(&f54->status_work,
			synaptics_rmi4_f54_status_work);

	f54->user_report_type1 = F54_16BIT_IMAGE;
	f54->user_report_type2 = F54_RAW_16BIT_IMAGE;

	wake_lock_init(&f54->test_wake_lock, WAKE_LOCK_SUSPEND,
		"synaptics_test_report");

#ifdef WATCHDOG_HRTIMER
	/* Watchdog timer to catch unanswered get report commands */
	hrtimer_init(&f54->watchdog, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	f54->watchdog.function = get_report_timeout;

	/* Work function to do actual cleaning up */
	INIT_WORK(&f54->timeout_work, timeout_set_status);
#endif

	return 0;

exit_sysfs:
exit_free_data:
	free_data_mem();

exit_free_control:
	free_control_mem();

exit_free_mem:
	kfree(f54->fn_ptr);

exit_free_f54:
	kfree(f54);

exit:
	return retval;
}

static void synaptics_rmi4_f54_remove(struct synaptics_rmi4_data *rmi4_data)
{
#ifdef WATCHDOG_HRTIMER
	hrtimer_cancel(&f54->watchdog);
#endif

	cancel_delayed_work_sync(&f54->status_work);
	flush_workqueue(f54->status_workqueue);
	destroy_workqueue(f54->status_workqueue);

	remove_sysfs();

	free_data_mem();
	free_control_mem();

	kfree(f54->report_data);
	kfree(f54->fn55);
	kfree(f54->fn_ptr);
	kfree(f54);

	complete(&remove_complete);

	return;
}

static int __init rmi4_f54_module_init(void)
{
	synaptics_rmi4_new_function(RMI_F54, true,
			synaptics_rmi4_f54_init,
			synaptics_rmi4_f54_remove,
			synaptics_rmi4_f54_attn,
			IC_MODE_UI);

	return 0;
}

static void __exit rmi4_f54_module_exit(void)
{
	init_completion(&remove_complete);
	synaptics_rmi4_new_function(RMI_F54, false,
			synaptics_rmi4_f54_init,
			synaptics_rmi4_f54_remove,
			synaptics_rmi4_f54_attn,
			IC_MODE_UI);

	wait_for_completion(&remove_complete);
	return;
}

module_init(rmi4_f54_module_init);
module_exit(rmi4_f54_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Test Reporting Module");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SYNAPTICS_DSX_DRIVER_VERSION);
