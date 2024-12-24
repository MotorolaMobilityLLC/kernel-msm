// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/iio/consumer.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include "ipd.h"

static void ipd_read_voltage(struct ipd_data *ipd, int m)
{
	int ret;

	ret = iio_read_channel_processed(ipd->adc[m], &ipd->adc_data[m]);
	if (!ret)
		dev_dbg(ipd->dev, "IPD Read Voltage data motor:%s %d\n",
					name[m], ipd->adc_data[m]);
	else
		dev_err(ipd->dev, "IPD Failed to Read Voltage data motor:%s\n", name[m]);
}

static void ipd_update_stepcount(struct ipd_data *ipd, int m)
{
	unsigned int stepCounter = ipd->step_num[m];

	if (ipd->dir[m]) {
		if (ipd->counter[m] <= stepCounter) {
			stepCounter = ipd->counter[m];
			ipd->counter[m] = 0;
		} else {
			ipd->counter[m] -= stepCounter;
		}
	} else {
		if ((ipd->counter[m] + stepCounter) > ipd->max_steps[m]) {
			stepCounter = ipd->max_steps[m] - ipd->counter[m];
			ipd->counter[m] = ipd->max_steps[m];
		} else {
			ipd->counter[m] += stepCounter;
		}
	}
	dev_dbg(ipd->dev, "%s motor:%s counter :%d stepcounter :%d\n",
		__func__, name[m], ipd->counter[m], stepCounter);
	ipd->step_num[m] = (stepCounter * ipd->pulse_val[m]);
	dev_dbg(ipd->dev, "%s motor:%s maxsteps :%d dir :%d\n", __func__,
			 name[m], ipd->max_steps[m], !ipd->dir[m]);
}

void ipd_update_step_motor(struct ipd_data *ipd, int m)
{
	int ret = 0;

	if (!ipd)
		return;

	dev_dbg(ipd->dev, "%s motor:%s half period :%d step_num :%d\n",
		__func__, name[m], ipd->half_period[m], ipd->step_num[m]);

	if (!gpio_get_value_cansleep(ipd->nvdd_pin)) {
		ipd->step_num[m] = 0;
		dev_err(ipd->dev, "%s STEP MOTOR IS NOT POWERED ON vdd_pin:%d\n",
			__func__, ipd->nvdd_pin);
		return;
	}

	if (ipd->step_num[m] <= 0) {
		ipd->step_num[m] = 0;
		return;
	}

	mutex_lock(&ipd->mlock[m]);
	/* Enable Hall Sensor Regulator */
	ret = regulator_enable(ipd->vdd);
	if (ret) {
		dev_err(ipd->dev, "%s hall vdd enable error %d\n", __func__, ret);
		goto exit;
	}
	ipd_update_stepcount(ipd, m);
	while (ipd->step_num[m]) {
		ipd->step_num[m]--;
		gpio_set_value_cansleep(ipd->nstep_pin[m], IPD_GPIO_HIGH);
		udelay(ipd->half_period[m]);
		gpio_set_value_cansleep(ipd->nstep_pin[m], IPD_GPIO_LOW);
		if (ipd->nthread_enable[m])
			break;
		udelay(ipd->half_period[m]);
	}
	dev_dbg(ipd->dev, "%s halfPeriod :%d step_num :%d\n",
		__func__, ipd->half_period[m], ipd->step_num[m]);
	msleep(VOLTAGE_SETTLING_TIME);
	ipd_read_voltage(ipd, m);
	/* Disable Hall Sensor Regulator Power optimization */
	ret = regulator_disable(ipd->vdd);
	if (ret)
		dev_err(ipd->dev, "%s hall vdd disable error %d\n", __func__, ret);
exit:
	mutex_unlock(&ipd->mlock[m]);
}

int ipd_thread_function(void *ipd_info, int m)
{
	struct ipd_data *ipd = ipd_info;

	if (!ipd)
		return -EFAULT;

	while (!kthread_should_stop()) {
		ipd_update_step_motor(ipd, m);
		udelay(ipd->sleep_interval[m]);
	}

	return 0;
}

int ipd_thread_function_left(void *ipd_info)
{
	return ipd_thread_function(ipd_info, LEFT);
}

int ipd_thread_function_right(void *ipd_info)
{
	return ipd_thread_function(ipd_info, RIGHT);
}

static void update_step_motor(struct work_struct *w, int m)
{
	struct ipd_data *ipd = container_of(w,
				struct ipd_data, work_queue_step_motor[m]);

	ipd_update_step_motor(ipd, m);
}

static void update_step_motor_left(struct work_struct *w)
{
	update_step_motor(w, LEFT);
}

static void update_step_motor_right(struct work_struct *w)
{
	update_step_motor(w, RIGHT);
}

/* IPD hall sleep attribute */
static ssize_t ipd_hall_sleep_show(struct device *dev,
	struct device_attribute *attr, char *buf, int m)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int val = 0;

	if (!ipd) {
		dev_err(dev, "%s ipd pointer null\n", __func__);
		return -EFAULT;
	}

	val = ipd->hall_gpio_state[m];
	dev_dbg(dev, "Ipd-StepMotor: %s val %d\n", __func__, val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static ssize_t ipd_hall_sleep_left_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return ipd_hall_sleep_show(dev, attr, buf, LEFT);
}

static ssize_t ipd_hall_sleep_right_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return ipd_hall_sleep_show(dev, attr, buf, RIGHT);
}

static ssize_t ipd_hall_sleep_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count, int m)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret = 0, val = 0;

	if (!ipd) {
		dev_err(dev, "%s ipd pointer null\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write sleep duration error\n");
		return ret;
	}

	if (ipd->step_num[m]) {
		dev_err(dev, "Steps in progress\n");
		return count;
	}

	mutex_lock(&ipd->mlock[m]);
	if (val && (ipd->hall_gpio_state[m] == IPD_GPIO_LOW)) {
		ret = regulator_enable(ipd->vdd);
		if (ret) {
			dev_err(dev, "motor:%s hall vdd enabled error %d\n", name[m], ret);
			count = (size_t) ret;
			goto exit;
		}

		ret = pinctrl_pm_select_default_state(dev);
		if (ret) {
			dev_err(dev, "motor:%s set hall active state error %d\n", name[m], ret);
			count = (size_t) ret;
			goto exit;
		}

		ipd->hall_gpio_state[m] = IPD_GPIO_HIGH;
	} else if (!val && (ipd->hall_gpio_state[m] == IPD_GPIO_HIGH)) {
		ret = pinctrl_pm_select_sleep_state(dev);
		if (ret) {
			dev_err(dev, "motor:%s set hall sleep state error %d\n", name[m], ret);
			count = (size_t) ret;
			goto exit;
		}

		ret = regulator_disable(ipd->vdd);
		if (ret) {
			dev_err(dev, "motor:%s hall vdd disabled error %d\n", name[m], ret);
			count = (size_t) ret;
			goto exit;
		}

		ipd->hall_gpio_state[m] = IPD_GPIO_LOW;
	}
exit:
	mutex_unlock(&ipd->mlock[m]);

	return count;

}

static ssize_t ipd_hall_sleep_left_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return ipd_hall_sleep_store(dev, attr, buf, count, LEFT);
}

static ssize_t ipd_hall_sleep_right_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return ipd_hall_sleep_store(dev, attr, buf, count, RIGHT);
}

/* IPD power attribute */
static ssize_t ipd_active_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int val = 0;

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	val = gpio_get_value_cansleep(ipd->nsleep_pin);
	dev_dbg(dev, "%s val %d\n", __func__, val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t ipd_active_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret = 0, val = 0;

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write sleep duration error\n");
		return ret;
	}

	if (ipd->step_num[LEFT] || ipd->step_num[RIGHT]) {
		dev_err(dev, "Steps in progress\n");
		return count;
	}

	mutex_lock(&ipd->mlock[LEFT]);
	if (val > 0)
		gpio_set_value_cansleep(ipd->nsleep_pin, IPD_GPIO_HIGH);
	else
		gpio_set_value_cansleep(ipd->nsleep_pin, IPD_GPIO_LOW);

	mutex_unlock(&ipd->mlock[LEFT]);

	return count;
}

/* Thread attribute */
static ssize_t enable_thread_show(struct device *dev,
	struct device_attribute *attr, char *buf, int m)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	dev_dbg(dev, "%s motor:%s val %d\n", __func__, name[m], ipd->nthread_enable[m]);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->nthread_enable[m]);
}

static ssize_t enable_thread_left_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return enable_thread_show(dev, attr, buf, LEFT);
}

static ssize_t enable_thread_right_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return enable_thread_show(dev, attr, buf, RIGHT);
}

static ssize_t enable_thread_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count, int m)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret = 0, val = 0;

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write enable thread error\n");
		return ret;
	}

	mutex_lock(&ipd->mlock[m]);
	ipd->nthread_enable[m] = val;
	if (ipd->nthread_enable[m]) {
		if (ipd->distance_thread[m]) {
			dev_dbg(dev, "motor:%s ipd resuming kthread\n", name[m]);
			wake_up_process(ipd->distance_thread[m]);
		} else {
			dev_dbg(dev, "motor:%s ipd creating kthread\n", name[m]);
			if (m)
				ipd->distance_thread[m] = kthread_create(
				ipd_thread_function_right, ipd,
				"IPD Thread Right ");
			else
				ipd->distance_thread[m] = kthread_create(
				ipd_thread_function_left, ipd,
				"IPD Thread left ");
			if (ipd->distance_thread[m])
				wake_up_process(ipd->distance_thread[m]);
			else
				dev_err(dev, "%s motor:%s ERROR creating IPD Thread\n",
								 __func__, name[m]);
		}
	} else {
		if (ipd->distance_thread[m]) {
			dev_dbg(dev, "motor:%s ipd stopping kthread\n", name[m]);
			kthread_stop(ipd->distance_thread[m]);
			ipd->distance_thread[m] = NULL;
		}
	}

	mutex_unlock(&ipd->mlock[m]);

	return count;
}

static ssize_t enable_thread_left_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return enable_thread_store(dev, attr, buf, count, LEFT);
}

static ssize_t enable_thread_right_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	return enable_thread_store(dev, attr, buf, count, RIGHT);
}

/* Pulse attribute */
static ssize_t pulse_show(struct device *dev, struct device_attribute *attr,
								 char *buf, int m)
{
	int val;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	val = ipd->pulse_val[m];
	dev_dbg(dev, "%s motor:%s max_steps %d\n", __func__, name[m], val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pulse_left_show(struct device *dev, struct device_attribute *attr,
								 char *buf)
{
	return pulse_show(dev, attr, buf, LEFT);
}

static ssize_t pulse_right_show(struct device *dev, struct device_attribute *attr,
								 char *buf)
{
	return pulse_show(dev, attr, buf, RIGHT);
}

static ssize_t pulse_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count, int m)
{
	int val = 0, ret = 0;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s motor:%s ipd nullptr error\n", __func__, name[m]);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "motor:%s write frequency error\n", name[m]);
		return ret;
	}

	if (ipd->step_num[m]) {
		dev_err(dev, "motor:%s Steps in progress\n", name[m]);
		return count;
	}

	mutex_lock(&ipd->mlock[m]);
	if ((val < MIN_PULSE_CYCLE) && (val > MAX_PULSE_CYCLE))
		ipd->pulse_val[m] = MIN_PULSE_CYCLE;

	ipd->pulse_val[m] = ((val / MIN_PULSE_CYCLE) * MIN_PULSE_CYCLE);

	mutex_unlock(&ipd->mlock[m]);

	return count;
}

static ssize_t pulse_left_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	return pulse_store(dev, attr, buf, count, LEFT);
}

static ssize_t pulse_right_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	return pulse_store(dev, attr, buf, count, RIGHT);
}

/* MaxSteps attribute */
static ssize_t max_steps_show(struct device *dev, struct device_attribute *attr,
						char *buf, int m)
{
	int val;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s motor:%s ipd nullptr error\n", __func__, name[m]);
		return -EFAULT;
	}

	val = ipd->max_steps[m];
	dev_err(dev, "%s motor:%s max_steps %d\n", __func__, name[m], val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t max_steps_left_show(struct device *dev, struct device_attribute *attr,
								 char *buf)
{
	return max_steps_show(dev, attr, buf, LEFT);
}

static ssize_t max_steps_right_show(struct device *dev, struct device_attribute *attr,
								 char *buf)
{
	return max_steps_show(dev, attr, buf, RIGHT);
}

static ssize_t max_steps_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count, int m)
{
	int val = 0, ret = 0;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd null ptr error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write frequency error\n");
		return ret;
	}

	if (ipd->step_num[m]) {
		dev_err(dev, "motor:%s Steps in progress\n", name[m]);
		return count;
	}

	mutex_lock(&ipd->mlock[m]);
	if (val > 0)
		ipd->max_steps[m] = val;

	mutex_unlock(&ipd->mlock[m]);

	return count;
}

static ssize_t max_steps_left_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return max_steps_store(dev, attr, buf, count, LEFT);
}

static ssize_t max_steps_right_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return max_steps_store(dev, attr, buf, count, RIGHT);
}

/* Voltage attribute */
static ssize_t voltage_show(struct device *dev, struct device_attribute *attr,
					char *buf, int m)
{
	int val;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	ipd_read_voltage(ipd, m);
	val = ipd->adc_data[m];
	dev_dbg(dev, "%s motor:%s val %d\n", __func__, name[m], val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t voltage_left_show(struct device *dev, struct device_attribute *attr,
								 char *buf)
{
	return voltage_show(dev, attr, buf, LEFT);
}

static ssize_t voltage_right_show(struct device *dev, struct device_attribute *attr,
								 char *buf)
{
	return voltage_show(dev, attr, buf, RIGHT);
}

static ssize_t voltage_left_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

static ssize_t voltage_right_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	return count;
}

/* Step attribute */
static ssize_t step_show(struct device *dev, struct device_attribute *attr,
	char *buf, int m)
{
	int val = 0;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	val = ipd->step_num[m];
	dev_dbg(dev, "%s motor:%s val %d\n", __func__, name[m], val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t step_left_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return step_show(dev, attr, buf, LEFT);
}

static ssize_t step_right_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return step_show(dev, attr, buf, RIGHT);
}

static ssize_t step_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count, int m)
{
	unsigned int val = 0;
	int ret = 0;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write step error\n");
		return ret;
	}

	mutex_lock(&ipd->mlock[m]);
	if (ipd->step_num[m]) {
		dev_err(dev, "motor:%s Steps in progress\n", name[m]);
		mutex_unlock(&ipd->mlock[m]);
		return count;
	}

	if (val > 0) {
		gpio_set_value_cansleep(ipd->nen_pin[m], IPD_GPIO_HIGH);
		ipd->step_num[m]  = val;
		ipd->half_period[m] =  ((MAX_FREQUENCY_HZ * 2) / ipd->step_freq[m]);
		ipd->sleep_interval[m] = (ipd->half_period[m] * 1000);

		dev_dbg(dev, "motor:%s counter %d dir:%d step %d val %d\n",
			name[m], ipd->counter[m], !ipd->dir[m], ipd->step_num[m], val);
		gpio_set_value_cansleep(ipd->ndir_pin[m], ipd->dir[m]);
		gpio_set_value_cansleep(ipd->nen_pin[m], IPD_GPIO_LOW);
		dev_dbg(dev, "motor:%s Upadate the step value %u\n", name[m], ipd->step_num[m]);
	} else {
		gpio_set_value_cansleep(ipd->nen_pin[m], IPD_GPIO_HIGH);
		ipd->step_num[m] = 0;
		dev_dbg(dev, "motor:%s failed to upadate step value %u\n", name[m], val);
	}

	mutex_unlock(&ipd->mlock[m]);

	if (!ipd->nthread_enable[m])
		queue_work(ipd->ipd_workqueue[m], &ipd->work_queue_step_motor[m]);

	return count;
}

static ssize_t step_left_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	return step_store(dev, attr, buf, count, LEFT);
}

static ssize_t step_right_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	return step_store(dev, attr, buf, count, RIGHT);
}

/* Frequency attribute */
static ssize_t frequency_show(struct device *dev,
	struct device_attribute *attr, char *buf, int m)
{
	int val;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	val = ipd->step_freq[m];
	dev_dbg(dev, "%s motor:%s val %d\n", __func__, name[m], val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t frequency_left_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return frequency_show(dev, attr, buf, LEFT);
}

static ssize_t frequency_right_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return frequency_show(dev, attr, buf, RIGHT);
}

static ssize_t frequency_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count, int m)
{
	int val = 0, ret = 0;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write frequency error\n");
		return ret;
	}

	mutex_lock(&ipd->mlock[m]);
	if (ipd->step_num[m]) {
		dev_err(dev, "motor:%s Steps in progress\n", name[m]);
		mutex_unlock(&ipd->mlock[m]);
		return count;
	}

	if (val >= DEFAULT_IPD_FREQUENCY && val <= MAX_FREQUENCY_HZ) {
		ipd->step_freq[m] = val;
		dev_dbg(dev, "motor:%s Upadate the frequency %u\n", name[m], val);
	} else {
		ipd->step_freq[m] = DEFAULT_IPD_FREQUENCY;
		dev_dbg(dev, "motor:%s Failed to update restored to default\n", name[m]);
	}

	mutex_unlock(&ipd->mlock[m]);

	return count;
}

static ssize_t frequency_left_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return frequency_store(dev, attr, buf, count, LEFT);
}

static ssize_t frequency_right_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return frequency_store(dev, attr, buf, count, RIGHT);
}

/* Direction attribute */
static ssize_t direction_show(struct device *dev,
	struct device_attribute *attr, char *buf, int m)
{
	int val;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd null pointer error\n", __func__);
		return -EFAULT;
	}

	val = !(ipd->dir[m]);
	dev_dbg(dev, "%s motor:%s val %d\n", __func__, name[m], val);

	return scnprintf(buf, PAGE_SIZE, "%s\n", val == 0 ? "INCREASE_IPD"
								: "DECREASE_IPD");
}

static ssize_t direction_left_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return direction_show(dev, attr, buf, LEFT);
}

static ssize_t direction_right_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return direction_show(dev, attr, buf, RIGHT);
}

static ssize_t direction_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count, int m)
{
	int val = 0, ret = 0;
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd null pointer error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "write direction error\n");
		return ret;
	}
	if (ipd->step_num[m]) {
		dev_err(dev, "motor:%s Steps in progress\n", name[m]);
		return count;
	}
	mutex_lock(&ipd->mlock[m]);
	if (val > 0) {
		ipd->dir[m] = DECREASE_IPD_VAL;
		gpio_set_value_cansleep(ipd->ndir_pin[m], IPD_GPIO_LOW);
		dev_dbg(dev, "motor:%s Decrement Ipd Value\n", name[m]);
	} else {
		ipd->dir[m] = INCREASE_IPD_VAL;
		gpio_set_value_cansleep(ipd->ndir_pin[m], IPD_GPIO_HIGH);
		dev_dbg(dev, "motor:%s Increment Ipd Value\n", name[m]);
	}
	mutex_unlock(&ipd->mlock[m]);

	return count;
}

static ssize_t direction_left_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return direction_store(dev, attr, buf, count, LEFT);
}

static ssize_t direction_right_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return direction_store(dev, attr, buf, count, RIGHT);
}

static DEVICE_ATTR_RW(ipd_active);
static DEVICE_ATTR_RW(pulse_left);
static DEVICE_ATTR_RW(pulse_right);
static DEVICE_ATTR_RW(max_steps_left);
static DEVICE_ATTR_RW(max_steps_right);
static DEVICE_ATTR_RW(voltage_left);
static DEVICE_ATTR_RW(voltage_right);
static DEVICE_ATTR_RW(step_left);
static DEVICE_ATTR_RW(step_right);
static DEVICE_ATTR_RW(frequency_left);
static DEVICE_ATTR_RW(frequency_right);
static DEVICE_ATTR_RW(direction_left);
static DEVICE_ATTR_RW(direction_right);
static DEVICE_ATTR_RW(enable_thread_left);
static DEVICE_ATTR_RW(enable_thread_right);
static DEVICE_ATTR_RW(ipd_hall_sleep_left);
static DEVICE_ATTR_RW(ipd_hall_sleep_right);


static struct attribute *ipd_attrs[] = {
	&dev_attr_direction_left.attr,
	&dev_attr_direction_right.attr,
	&dev_attr_frequency_left.attr,
	&dev_attr_frequency_right.attr,
	&dev_attr_step_left.attr,
	&dev_attr_step_right.attr,
	&dev_attr_voltage_left.attr,
	&dev_attr_voltage_right.attr,
	&dev_attr_enable_thread_left.attr,
	&dev_attr_enable_thread_right.attr,
	&dev_attr_ipd_active.attr,
	&dev_attr_max_steps_left.attr,
	&dev_attr_max_steps_right.attr,
	&dev_attr_pulse_left.attr,
	&dev_attr_pulse_right.attr,
	&dev_attr_ipd_hall_sleep_left.attr,
	&dev_attr_ipd_hall_sleep_right.attr,
	NULL,
};

static struct attribute_group ipd_attribute_group = {
	.name = "ipd",
	.attrs = ipd_attrs,
};

/* Configure initial values to ipd step motor */
static void step_motor_setup(struct ipd_data *ipd, int m)
{
	ipd->step_freq[m]      = DEFAULT_IPD_FREQUENCY;
	ipd->step_num[m]       = 0;
	ipd->max_steps[m]      = MAX_STEP_COUNT;
	ipd->pulse_val[m]      = (MIN_PULSE_CYCLE * 2);
	ipd->counter[m]        = (MAX_STEP_COUNT / 2);
	ipd->dir[m]            = DECREASE_IPD_VAL;
	ipd->half_period[m]    = ((MAX_FREQUENCY_HZ * 2) / ipd->step_freq[m]);
	ipd->sleep_interval[m] = (ipd->half_period[m]);
	ipd->hall_gpio_state[m] = IPD_GPIO_LOW;
	ipd->pre_gpio_state[m] = IPD_GPIO_LOW;
	/* Operating: nEN -> LOW, nsleep -> HIGH */
	/* Disabled:  nEN -> HIGH, nsleep -> HIGH */
	/* Sleep:                  nsleep -> LOW */

	if (m == LEFT) {
		gpio_set_value_cansleep(ipd->nvdd_pin, IPD_GPIO_HIGH);

		/* sleep state */
		gpio_set_value_cansleep(ipd->nsleep_pin, IPD_GPIO_LOW);
	}

	/* Setting enable pin to HIGH -- disabled state */
	gpio_set_value_cansleep(ipd->nen_pin[m], IPD_GPIO_HIGH);

	dev_dbg(ipd->dev,  "motor:%s pwrStatus : %d nen %d sleep %d\n",
						name[m], gpio_get_value(ipd->nvdd_pin),
		gpio_get_value(ipd->nen_pin[m]), gpio_get_value(ipd->nsleep_pin));
	dev_dbg(ipd->dev, "motor:%s Initial Setup  Done !!!\n", name[m]);
}

/* Assign the initial value of the gpio configuration */
static int step_motor_gpio_config(struct ipd_data *ipd, int m)
{
	int status, ret;

	if (!m) {
		status = devm_gpio_request_one(ipd->dev, ipd->nvdd_pin,
				GPIOF_OUT_INIT_LOW, "ipd_vddpwr");
		if (status) {
			dev_err(ipd->dev, "failed to acquire vdd gpio:%d\n", ipd->nvdd_pin);
			return -EINVAL;
		}

		ret = gpio_get_value(ipd->nvdd_pin);
		dev_dbg(ipd->dev, "ipd->nvdd_pin %d gpio value : %d\n", ipd->nvdd_pin,
							 ret);

		status = devm_gpio_request_one(ipd->dev, ipd->nsleep_pin,
					 GPIOF_OUT_INIT_LOW, "ipd_sleep");
		if (status) {
			dev_err(ipd->dev, "failed to acquire sleep gpio:%d\n", ipd->nsleep_pin);
			return -EINVAL;
		}

		ret = gpio_get_value(ipd->nsleep_pin);
		dev_dbg(ipd->dev, "ipd->nsleep_pin %d gpio value : %d\n",
						 ipd->nsleep_pin, ret);
	}

	if (m)
		status = devm_gpio_request_one(ipd->dev, ipd->nstep_pin[m],
					 GPIOF_OUT_INIT_LOW, "ipd_step_right");
	else
		status = devm_gpio_request_one(ipd->dev, ipd->nstep_pin[m],
					 GPIOF_OUT_INIT_LOW, "ipd_step_left");
	if (status) {
		dev_err(ipd->dev, "motor:%s failed to acquire step gpio:%d\n", name[m],
									ipd->nstep_pin[m]);
		return -EINVAL;
	}

	ret = gpio_get_value(ipd->nstep_pin[m]);
	dev_dbg(ipd->dev, "motor:%s ipd->nstep_pin %d gpio value : %d\n", name[m],
					ipd->nstep_pin[m], ret);

	if (m)
		status = devm_gpio_request_one(ipd->dev, ipd->nen_pin[m],
					 GPIOF_OUT_INIT_HIGH, "ipd_nEn_right");
	else
		status = devm_gpio_request_one(ipd->dev, ipd->nen_pin[m],
					 GPIOF_OUT_INIT_HIGH, "ipd_nEn_left");
	if (status) {
		dev_err(ipd->dev, "motor: %s failed to acquire nEn gpio:%d\n", name[m],
									ipd->nen_pin[m]);
		return -EINVAL;
	}

	ret = gpio_get_value(ipd->nen_pin[m]);
	dev_dbg(ipd->dev, "motor:%s ipd->nen_pin %d gpio value : %d\n",
			name[m], ipd->nen_pin[m], ret);

	if (m)
		status = devm_gpio_request_one(ipd->dev, ipd->ndir_pin[m],
					 GPIOF_OUT_INIT_LOW, "ipd_dir_right");
	else
		status = devm_gpio_request_one(ipd->dev, ipd->ndir_pin[m],
					 GPIOF_OUT_INIT_LOW, "ipd_dir_left");
	if (status) {
		dev_err(ipd->dev, "motor:%s failed to acquire direction gpio:%d\n", name[m],
										ipd->ndir_pin[m]);
		return -EINVAL;
	}

	ret = gpio_get_value(ipd->ndir_pin[m]);
	dev_dbg(ipd->dev, "motor:%s ipd->ndir_pin %d gpio value : %d\n", name[m],
						ipd->ndir_pin[m], ret);

	return 0;
}

/* Parse the device tree configuration and assign gpio pins */
static int step_motor_parse_dts_dual(struct ipd_data *ipd, int m)
{
	int ret;

	if (m == RIGHT)
		ipd->nstep_pin[m] = of_get_named_gpio(ipd->dev->of_node, "step-gpio-right", 0);
	else
		ipd->nstep_pin[m] = of_get_named_gpio(ipd->dev->of_node, "step-gpio-left", 0);
	if (!gpio_is_valid(ipd->nstep_pin[m])) {
		dev_err(ipd->dev, "motor:%s failed to parse step gpio\n", name[m]);
		return -EINVAL;
	}

	ret = gpio_get_value(ipd->nstep_pin[m]);
	dev_dbg(ipd->dev, "motor:%s Step gpio:%d gpio value : %d\n",
						name[m],  ipd->nstep_pin[m], ret);

	if (m == LEFT) {
		ipd->nvdd_pin = of_get_named_gpio(ipd->dev->of_node, "pwr-gpio", 0);
		if (!gpio_is_valid(ipd->nvdd_pin)) {
			dev_err(ipd->dev, "failed to parse vdd gpio\n");
			return -EINVAL;
		}

		ret = gpio_get_value(ipd->nvdd_pin);
		dev_dbg(ipd->dev, "PWR gpio:%d gpio value : %d\n", ipd->nvdd_pin, ret);

		ipd->nsleep_pin = of_get_named_gpio(ipd->dev->of_node, "nsleep-gpio", 0);
		if (!gpio_is_valid(ipd->nsleep_pin)) {
			dev_err(ipd->dev, "failed to parse sleep gpio\n");
			return -EINVAL;
		}

		ret = gpio_get_value(ipd->nsleep_pin);
		dev_dbg(ipd->dev, "Sleep gpio:%d gpio value : %d\n",
							ipd->nsleep_pin, ret);
	}

	if (m == RIGHT)
		ipd->nen_pin[m] = of_get_named_gpio(ipd->dev->of_node, "nen-gpio-right", 0);
	else
		ipd->nen_pin[m] = of_get_named_gpio(ipd->dev->of_node, "nen-gpio-left", 0);
	if (!gpio_is_valid(ipd->nen_pin[m])) {
		dev_err(ipd->dev, "motor:%s failed to parse en gpio\n", name[m]);
		return -EINVAL;
	}

	ret = gpio_get_value(ipd->nen_pin[m]);
	dev_dbg(ipd->dev, "motor:%s nEn gpio:%d gpio value : %d\n",
							name[m], ipd->nen_pin[m], ret);

	if (m == RIGHT)
		ipd->ndir_pin[m] = of_get_named_gpio(ipd->dev->of_node, "dir-gpio-right", 0);
	else
		ipd->ndir_pin[m] = of_get_named_gpio(ipd->dev->of_node, "dir-gpio-left", 0);
	if (!gpio_is_valid(ipd->ndir_pin[m])) {
		dev_err(ipd->dev, "motor:%s failed to parse direction gpio\n", name[m]);
		return -EINVAL;
	}

	ret = gpio_get_value(ipd->ndir_pin[m]);
	dev_dbg(ipd->dev, "motor:%s Dir gpio:%d gpio value : %d\n",
							name[m], ipd->ndir_pin[m], ret);

	return 0;
}


static int step_motor_probe(struct platform_device *pdev)
{
	struct ipd_data *ipd = NULL;
	int ret;

	ipd = devm_kzalloc(&pdev->dev, sizeof(*ipd), GFP_KERNEL);
	if (!ipd)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, ipd);
	ipd->dev = &pdev->dev;

	/* Get IIO Channel data*/
	ipd->adc[LEFT] =  devm_iio_channel_get(&pdev->dev, "ipd_step_motor_left");
	if (IS_ERR(ipd->adc[LEFT])) {
		ret = PTR_ERR(ipd->adc[LEFT]);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "ipd failed to get Left IIO channel data\n");
		return ret;
	}

	ipd->adc[RIGHT] =  devm_iio_channel_get(&pdev->dev, "ipd_step_motor_right");
	if (IS_ERR(ipd->adc[RIGHT])) {
		ret = PTR_ERR(ipd->adc[RIGHT]);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "ipd failed to get Right IIO channel data\n");
		return ret;
	}

	ipd->vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(ipd->vdd)) {
		ret = PTR_ERR(ipd->vdd);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "ipd-StepMotor: Failed to get hall vdd regulator: %ld\n",
								PTR_ERR(ipd->vdd));
		return ret;
	}

	ret = pinctrl_pm_select_sleep_state(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "ipd-StepMotor: Failed to select sleep state\n");
		return -ENODEV;
	}

	if (step_motor_parse_dts_dual(ipd, LEFT)) {
		dev_err(&pdev->dev, "ipd motor LEFT failed to parse DTS\n");
		return -ENODEV;
	}

	if (step_motor_parse_dts_dual(ipd, RIGHT)) {
		dev_err(&pdev->dev, "ipd motor RIGHT failed to parse DTS\n");
		return -ENODEV;
	}

	if (step_motor_gpio_config(ipd, LEFT)) {
		dev_err(&pdev->dev, "ipd motor LEFT failed to gpio configures\n");
		return -ENODEV;
	}

	if (step_motor_gpio_config(ipd, RIGHT)) {
		dev_err(&pdev->dev, "ipd motor RIGHT failed to gpio configures\n");
		return -ENODEV;
	}

	step_motor_setup(ipd, LEFT);
	step_motor_setup(ipd, RIGHT);

	if (devm_device_add_group(&pdev->dev, &ipd_attribute_group)) {
		dev_err(&pdev->dev, "ipd failed to create sysfs group\n");
		return -ENODEV;
	}

	INIT_WORK(&ipd->work_queue_step_motor[LEFT], update_step_motor_left);
	ipd->ipd_workqueue[LEFT] = alloc_ordered_workqueue("ipd_workqueue_left", 0);
	INIT_WORK(&ipd->work_queue_step_motor[RIGHT], update_step_motor_right);
	ipd->ipd_workqueue[RIGHT] = alloc_ordered_workqueue("ipd_workqueue_right", 0);
	mutex_init(&ipd->mlock[LEFT]);
	mutex_init(&ipd->mlock[RIGHT]);

	ipd_read_voltage(ipd, LEFT);
	ipd_read_voltage(ipd, RIGHT);

	return 0;
}

static int step_motor_remove_dual(struct ipd_data *ipd, int m)
{
	if (ipd->nthread_enable[m]) {
		kthread_stop(ipd->distance_thread[m]);
		ipd->distance_thread[m] = NULL;
	}
	/* TODO: Need to check if this is per thread */
	flush_scheduled_work();
	mutex_destroy(&ipd->mlock[m]);

	return 0;
}

static int step_motor_remove(struct platform_device *pdev)
{
	struct ipd_data *ipd = dev_get_drvdata(&pdev->dev);

	if (!ipd) {
		dev_err(&pdev->dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	step_motor_remove_dual(ipd, LEFT);
	step_motor_remove_dual(ipd, RIGHT);
	devm_device_remove_group(ipd->dev, &ipd_attribute_group);
	gpio_set_value_cansleep(ipd->nsleep_pin, IPD_GPIO_LOW);
	gpio_set_value_cansleep(ipd->nvdd_pin, IPD_GPIO_LOW);

	return 0;
}

static int step_motor_suspend_dual(struct ipd_data *ipd, int m)
{
	int ret = 0;

	ipd->pre_gpio_state[m] = ipd->hall_gpio_state[m];
	if (ipd->hall_gpio_state[m] == IPD_GPIO_HIGH) {
		ret = pinctrl_pm_select_sleep_state(ipd->dev);
		if (ret) {
			dev_err(ipd->dev, "motor:%s set hall sleep state error %d\n", name[m], ret);
			return ret;
		}

		if (!m) {
			ret = regulator_disable(ipd->vdd);
			if (ret) {
				dev_err(ipd->dev, "motor:%s hall vdd enabled error %d\n",
								name[m], ret);
				return ret;
			}
		}

		ipd->hall_gpio_state[m] = IPD_GPIO_LOW;
	}

	return 0;
}

static int step_motor_suspend(struct device *dev)
{
	/* Add lock if needed*/
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	dev_dbg(dev, "Enter Suspend Mode\n");
	gpio_set_value_cansleep(ipd->nsleep_pin, IPD_GPIO_LOW);
	step_motor_suspend_dual(ipd, LEFT);
	step_motor_suspend_dual(ipd, RIGHT);
	dev_dbg(dev, "Exit Suspend Mode\n");

	return 0;
}

static int step_motor_resume_dual(struct ipd_data *ipd, int m)
{
	int ret = 0;

	if (ipd->hall_gpio_state[m] == IPD_GPIO_LOW &&
			 ipd->pre_gpio_state[m] == IPD_GPIO_HIGH) {
		if (!m) {
			ret = regulator_enable(ipd->vdd);
			if (ret) {
				dev_err(ipd->dev, "hall vdd enabled error %d\n", ret);
				return ret;
			}
		}

		ret = pinctrl_pm_select_default_state(ipd->dev);
		if (ret) {
			dev_err(ipd->dev, "set hall active state error %d\n", ret);
			return ret;
		}

		ipd->hall_gpio_state[m] = IPD_GPIO_HIGH;
		ipd->pre_gpio_state[m] = IPD_GPIO_LOW;
	}

	return ret;
}

static int step_motor_resume(struct device *dev)
{
	/* Add lock if needed*/
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (!ipd) {
		dev_err(dev, "%s ipd nullptr error\n", __func__);
		return -EFAULT;
	}

	step_motor_resume_dual(ipd, RIGHT);
	dev_dbg(dev, "Exit Resume Mode\n");

	return 0;
}

static const struct dev_pm_ops step_motor_pm_ops = {
	.suspend = step_motor_suspend,
	.resume = step_motor_resume,
};

static const struct of_device_id step_motor_table[] = {
	{ .compatible = "qcom,ipd_step_motor" },
	{ }
};
MODULE_DEVICE_TABLE(of, step_motor_table);

static struct platform_driver ipd_motor_driver = {
	.driver = {
		.name   = "ipd_step_motor",
		.of_match_table = of_match_ptr(step_motor_table),
		.pm = &step_motor_pm_ops,
	},
	.probe  = step_motor_probe,
	.remove = step_motor_remove,
};
module_platform_driver(ipd_motor_driver);

MODULE_DESCRIPTION("QTI Dual IPD Step Motor driver");
MODULE_ALIAS("platform:ipd_step_motor");
MODULE_LICENSE("GPL");
