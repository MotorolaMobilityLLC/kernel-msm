/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Javier Torres Castillo <javier.torres.castillo@intel.com>
 */
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/kernel.h>
#include "dev_freq_attrib.h"
#include "df_rgx_defs.h"
#include "dev_freq_debug.h"
#include "df_rgx_burst.h"

extern struct gpu_freq_thresholds a_governor_profile[3];
/**
 * show_dynamic_turbo_state() - Read function for sysfs
 * sys/devices/platform/dfrgx/dynamic_turbo_state.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t show_dynamic_turbo_state(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct busfreq_data *bfdata = dev_get_drvdata(dev);
	int dt_state = dfrgx_burst_is_enabled(&bfdata->g_dfrgx_data);

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: value = %d\n",
				__func__, dt_state);

	return scnprintf(buf, PAGE_SIZE, "%d\n", dt_state);
}

/**
 * store_dynamic_turbo_state() - Write function for sysfs
 * sys/devices/platform/dfrgx/dynamic_turbo_state.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t store_dynamic_turbo_state(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct busfreq_data *bfdata = dev_get_drvdata(dev);
	int dt_state;
	int ret = -EINVAL;

	ret = sscanf(buf, "%d", &dt_state);

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s:  count = %zu, ret = %u , state = %d\n",
				__func__, count, ret, dt_state);
	if (ret != 1)
		goto out;

	dfrgx_burst_set_enable(&bfdata->g_dfrgx_data, dt_state);

	ret = count;
out:
	return ret;
}

/**
 * show_profiling_state() - Read function for sysfs
 * sys/devices/platform/dfrgx/profiling_state.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t show_profiling_state(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct busfreq_data *bfdata = dev_get_drvdata(dev);
	int profiling_state = dfrgx_profiling_is_enabled(&bfdata->g_dfrgx_data);

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: value = %d\n",
				__func__, profiling_state);

	return scnprintf(buf, PAGE_SIZE, "%d\n", profiling_state);
}

/**
 * store_profiling_state() - Write function for sysfs
 * sys/devices/platform/dfrgx/profiling_state.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t store_profiling_state(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct busfreq_data *bfdata = dev_get_drvdata(dev);
	int profiling_state;
	int ret = -EINVAL;

	ret = sscanf(buf, "%d", &profiling_state);

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: count = %zu, ret = %u , state = %d\n",
				__func__, count, ret, profiling_state);
	if (ret != 1)
		goto out;

	dfrgx_profiling_set_enable(&bfdata->g_dfrgx_data, profiling_state);

	ret = count;
out:
	return ret;
}

/**
 * show_profiling_stats() - Read function for sysfs
 * sys/devices/platform/dfrgx/profiling_show_stats.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t show_profiling_stats(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int ret = 0;
	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s\n",
				__func__);

	ret = gpu_profiling_records_show(buf);

	return ret;
}

/**
 * reset_profiling_stats() - Command function to reset stats.
 * Use cat sysfs sys/devices/platform/dfrgx/profiling_reset_stats
 * to perform this action.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to. Ignored.
 */
static ssize_t reset_profiling_stats(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s\n",
				__func__);

	gpu_profiling_records_restart();

	return 0;
}

/**
 * show_turbo_profile() - Read function for sysfs
 * sys/devices/platform/dfrgx/custom_turbo_profile.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t show_turbo_profile(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s\n",
				__func__);

	return scnprintf(buf, PAGE_SIZE, "%d %d\n",
		a_governor_profile[DFRGX_TURBO_PROFILE_CUSTOM].util_th_low,
		a_governor_profile[DFRGX_TURBO_PROFILE_CUSTOM].util_th_high);
}

/**
 * store_turbo_profile() - Write function for sysfs
 * sys/devices/platform/dfrgx/custom_turbo_profile.
 * @dev: platform device.
 * @attr: 1 Attribute associated to this entry.
 * @buf: Buffer to write to.
 */
static ssize_t store_turbo_profile(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct busfreq_data *bfdata = dev_get_drvdata(dev);
	int ret = -EINVAL;
	int low_th = 0, high_th = 0;

	ret = sscanf(buf, "%d %d", &low_th, &high_th);

	DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: count = %zu, ret = %u\n",
				__func__, count, ret);
	if (ret != 2)
		goto out;

	if (low_th > high_th) {
		ret = 0;
		goto out;
	}

	a_governor_profile[DFRGX_TURBO_PROFILE_CUSTOM].util_th_low = low_th;
	a_governor_profile[DFRGX_TURBO_PROFILE_CUSTOM].util_th_high = high_th;

	bfdata->g_dfrgx_data.g_profile_index = DFRGX_TURBO_PROFILE_CUSTOM;
	ret = count;

out:
	return ret;
}

static const struct device_attribute devfreq_attrs[] = {
	__ATTR(dynamic_turbo_state, S_IRUGO | S_IWUSR,
		show_dynamic_turbo_state, store_dynamic_turbo_state),
	__ATTR(profiling_state, S_IRUGO | S_IWUSR,
		show_profiling_state, store_profiling_state),
	__ATTR(profiling_show_stats, S_IRUGO,
		show_profiling_stats, NULL),
	__ATTR(profiling_reset_stats, S_IRUGO,
		reset_profiling_stats, NULL),
	__ATTR(custom_turbo_profile, S_IRUGO | S_IWUSR,
		show_turbo_profile, store_turbo_profile),
	__ATTR(empty, 0, NULL, NULL)
};

/**
 * dev_freq_add_attributes_to_sysfs() - Creates all the sysfs entries
 * for the specified platform device.
 * @dev: platform device.
 */
int dev_freq_add_attributes_to_sysfs(struct device *device)
{
	int error = 0;
	int i = 0;

	if (!device)
		return -1;

	for (i = 0; devfreq_attrs[i].attr.mode != 0; i++) {
		error = device_create_file(device, &devfreq_attrs[i]);
		if (error) {
			DFRGX_DPF(DFRGX_DEBUG_HIGH, "%s: could not"
				"create device file error = %0x\n",
				__func__, error);
			break;
		}
	}

	/*Were all the files created?*/
	if (devfreq_attrs[i].attr.mode != 0) {
		int j = 0;
		for (j = 0; j < i; j++)
			device_remove_file(device, &devfreq_attrs[i]);
	}

	return error;
}

/**
 * dev_freq_remove_attributes_on_sysfs() - Removes all the sysfs entries
 * for the specified
 * platform device.
 * @dev: platform device.
 */
void dev_freq_remove_attributes_on_sysfs(struct device *device)
{
	int i = 0;

	if (device) {
		for (i = 0; devfreq_attrs[i].attr.mode != 0; i++)
			device_remove_file(device, &devfreq_attrs[i]);
	}
}
