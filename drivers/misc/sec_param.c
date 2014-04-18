/*
 *  drivers/misc/sec_param.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sec_param.h>
#include <linux/file.h>
#include <linux/syscalls.h>

#define PARAM_RD	0
#define PARAM_WR	1

/* parameter block */
#define SEC_PARAM_FILE_NAME	"/dev/block/platform/msm_sdcc.1/by-name/param"
#define SEC_PARAM_FILE_SIZE	0xA00000		/* 10MB */
#define SEC_PARAM_FILE_OFFSET (SEC_PARAM_FILE_SIZE - 0x100000)

/* single global instance */
struct sec_param_data *param_data;

static bool param_sec_operation(void *value, int offset,
		int size, int direction)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	int flag = (direction == PARAM_WR) ? (O_RDWR | O_SYNC) : O_RDONLY;

	pr_debug("%s %p %x %d %d\n", __func__, value, offset, size, direction);
	fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(SEC_PARAM_FILE_NAME, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n",
				__func__, PTR_ERR(filp));
		set_fs(fs);
		return false;
	}

	ret = filp->f_op->llseek(filp, offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK\n", __func__);
		ret = false;
		goto param_sec_debug_out;
	}

	if (direction == PARAM_RD)
		ret = filp->f_op->read(filp, (char __user *)value,
				size, &filp->f_pos);
	else if (direction == PARAM_WR)
		ret = filp->f_op->write(filp, (char __user *)value,
				size, &filp->f_pos);

param_sec_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	return ret;
}

bool sec_open_param(void)
{
	int ret = true;
	int offset = SEC_PARAM_FILE_OFFSET;

	if (param_data != NULL)
		return true;

	param_data = kmalloc(sizeof(struct sec_param_data), GFP_KERNEL);

	ret = param_sec_operation(param_data, offset,
			sizeof(struct sec_param_data), PARAM_RD);

	if (!ret) {
		kfree(param_data);
		param_data = NULL;
		pr_err("%s PARAM OPEN FAIL\n", __func__);
		return false;
	}
	return ret;

}
EXPORT_SYMBOL(sec_open_param);
bool sec_get_param(enum sec_param_index index, void *value)
{
	int ret = true;
	ret = sec_open_param();
	if (!ret)
		return ret;

	switch (index) {
	case param_index_debuglevel:
		memcpy(value, &(param_data->debuglevel), sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(value, &(param_data->uartsel), sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(value, &(param_data->rory_control),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_done:
		memcpy(value, &(param_data->movinand_checksum_done),
				sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(value, &(param_data->movinand_checksum_pass),
				sizeof(unsigned int));
		break;
#if defined(CONFIG_MACH_MSM8226_SPRAT)
	case param_index_dumpmode:
		memcpy(value, &(param_data->dumpmode),
			sizeof(unsigned int));
		break;
#endif
#if defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_AEGIS2)
	case param_slideCount:
		memcpy(value, &(param_data->slideCount),
				sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(value, &(param_data->update_cp_bin),
				sizeof(unsigned int));
		pr_info("param_data.update_cp_bin :[%d]!!",
			param_data->update_cp_bin);
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_boot_alarm_set:
		memcpy(value, &(param_data->boot_alarm_set),
			sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_l:
		memcpy(value, &(param_data->boot_alarm_value_l),
			sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_h:
		memcpy(value, &(param_data->boot_alarm_value_h),
			sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	case param_index_normal_poweroff:
		memcpy(&(param_data->normal_poweroff), value,
			sizeof(unsigned int));
		break;
#endif
	default:
		return false;
	}
	return true;
}
EXPORT_SYMBOL(sec_get_param);

bool sec_set_param(enum sec_param_index index, void *value)
{
	int ret = true;
	int offset = SEC_PARAM_FILE_OFFSET;

	ret = sec_open_param();
	if (!ret)
		return ret;

	switch (index) {
	case param_index_debuglevel:
		memcpy(&(param_data->debuglevel),
				value, sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(&(param_data->uartsel),
				value, sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(&(param_data->rory_control),
				value, sizeof(unsigned int));
		break;
#if defined(CONFIG_MACH_MSM8226_SPRAT)
	case param_index_dumpmode:
		memcpy(&(param_data->dumpmode), value,
			sizeof(unsigned int));
		break;
#endif
	case param_index_movinand_checksum_done:
		memcpy(&(param_data->movinand_checksum_done),
				value, sizeof(unsigned int));
		break;
	case param_index_movinand_checksum_pass:
		memcpy(&(param_data->movinand_checksum_pass),
				value, sizeof(unsigned int));
		break;
#if defined(CONFIG_MACH_APEXQ) || defined(CONFIG_MACH_AEGIS2)
	case param_slideCount:
		memcpy(&(param_data->slideCount),
				value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_GSM_MODEM_SPRD6500
	case param_update_cp_bin:
		memcpy(&(param_data->update_cp_bin),
				value, sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_RTC_AUTO_PWRON_PARAM
	case param_index_boot_alarm_set:
		memcpy(&(param_data->boot_alarm_set), value,
			sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_l:
		memcpy(&(param_data->boot_alarm_value_l), value,
			sizeof(unsigned int));
		break;
	case param_index_boot_alarm_value_h:
		memcpy(&(param_data->boot_alarm_value_h), value,
			sizeof(unsigned int));
		break;
#endif
#ifdef CONFIG_SEC_MONITOR_BATTERY_REMOVAL
	case param_index_normal_poweroff:
		memcpy(&(param_data->normal_poweroff), value,
			sizeof(unsigned int));
		break;
#endif
	default:
		return false;
	}
	return param_sec_operation(param_data, offset,
			sizeof(struct sec_param_data), PARAM_WR);
}
EXPORT_SYMBOL(sec_set_param);
/* ##########################################################
 * #
 * # SEC PARAM Driver sysfs file
 * # 1. movinand_checksum_done_show
 * # 2. movinand_checksum_pass_show
 * ##########################################################
 * */
static struct class *sec_param_class;
struct device *sec_param_dev;

static ssize_t movinand_checksum_done_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	sec_get_param(param_index_movinand_checksum_done, &ret);
	if (ret == 1 || ret == 0) {
		pr_err("checksum is not in valuable range.\n");
		ret = 1;
	}
	return snprintf(buf, sizeof(buf), "%u\n", ret);
}
static DEVICE_ATTR(movinand_checksum_done,
				0664, movinand_checksum_done_show, NULL);

static ssize_t movinand_checksum_pass_show
(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	sec_get_param(param_index_movinand_checksum_pass, &ret);
	if (ret == 1 || ret == 0) {
		pr_err("checksum is not in valuable range.\n");
		ret = 1;
	}
	return snprintf(buf, sizeof(buf), "%u\n", ret);
}
static DEVICE_ATTR(movinand_checksum_pass,
				0664, movinand_checksum_pass_show, NULL);

int sec_param_sysfs_init(void)
{

	int ret = 0;

	sec_param_class = class_create(THIS_MODULE, "sec_param");

	if (IS_ERR(sec_param_class)) {
		pr_err("Failed to create class(mdnie_class)!!\n");
		ret = -1;
		goto failed_create_class;
	}

	sec_param_dev = device_create(sec_param_class,
					NULL, 0, NULL, "sec_param");

	if (IS_ERR(sec_param_dev)) {
		pr_err("Failed to create device(sec_param_dev)!!");
		ret = -1;
		goto failed_create_dev;
	}

	if (device_create_file(sec_param_dev,
				&dev_attr_movinand_checksum_done) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
				dev_attr_movinand_checksum_done.attr.name);
		ret = -1;
	}

	if (device_create_file(sec_param_dev,
				&dev_attr_movinand_checksum_pass) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
				dev_attr_movinand_checksum_pass.attr.name);
		ret = -1;
	}
failed_create_class:
	return ret;

failed_create_dev:
	class_destroy(sec_param_class);
	return ret;
}
module_init(sec_param_sysfs_init);
