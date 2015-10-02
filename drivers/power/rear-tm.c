/*
 * Copyright(c) 2015, LGE Inc. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/spmi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/switch.h>

struct rear_tm_data {
	struct device *dev;
	struct spmi_device *spmi;
	struct qpnp_adc_tm_chip *adc_tm_dev;
	struct qpnp_vadc_chip *vadc_dev;
	struct qpnp_adc_tm_btm_param adc_param;
	struct tm_ctrl_data *warm_cfg;
	unsigned int warm_cfg_size;
	struct wakeup_source ws;
	int thermalstate;
	struct switch_dev sdev;
	int enabled;
};

struct tm_ctrl_data {
	int thresh;
	int thresh_clr;
	unsigned int action;
};

enum {
	REAR_NORMAL = 0,
	REAR_CALL_ALERT,
	REAR_CALL_DROPPED,
	REAR_POWEROFF,
	REAR_ACTION_MAX
};

static char *rear_action_str[REAR_ACTION_MAX] = {
	"normal",
	"call_alert",
	"call_dropped",
	"poweroff",
};

static int rear_tm_notification_init(struct rear_tm_data *rear_tm);
static void rear_tm_notification(enum qpnp_tm_state state, void *ctx);

static struct rear_tm_data *get_rear_tm(struct device *dev)
{
	struct switch_dev *sdev = dev_get_drvdata(dev);
	struct rear_tm_data *rear_tm = container_of(
			sdev, struct rear_tm_data, sdev);

	return rear_tm;
}

static ssize_t rear_sysfs_thermalstate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rear_tm_data *rear_tm = get_rear_tm(dev);

	return snprintf(buf, PAGE_SIZE, "%s(%d)\n",
			rear_action_str[rear_tm->thermalstate],
			rear_tm->thermalstate);
}

static ssize_t rear_sysfs_thermalstate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	struct rear_tm_data *rear_tm = get_rear_tm(dev);

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	switch (val) {
	case REAR_NORMAL:
	case REAR_CALL_ALERT:
	case REAR_CALL_DROPPED:
		rear_tm->thermalstate = val;
		switch_set_state(&rear_tm->sdev, val);
		break;
	default:
		pr_err("val(%u) does not allowed.\n", val);
		return -EINVAL;
	}

	return count;
}

static ssize_t rear_sysfs_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int cur_temp = 0;
	struct qpnp_vadc_result result;
	struct rear_tm_data *rear_tm = get_rear_tm(dev);

	ret = qpnp_vadc_read(rear_tm->vadc_dev, P_MUX8_1_1, &result);
	if (!ret) {
		cur_temp = (int)result.physical;
	} else {
		pr_warn("Unable to read vbat ret=%d\n", ret);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", cur_temp);
}

static ssize_t rear_sysfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	struct rear_tm_data *rear_tm = get_rear_tm(dev);

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	rear_tm->enabled = val;

	if (rear_tm->enabled) {
		pr_info("enable rear temp monitoring.\n");
		qpnp_adc_tm_channel_measure(rear_tm->adc_tm_dev,
					&rear_tm->adc_param);
	} else {
		pr_info("disable rear temp monitoring.\n");
		qpnp_adc_tm_disable_chan_meas(rear_tm->adc_tm_dev,
					&rear_tm->adc_param);
	}

	return count;
}

static ssize_t rear_sysfs_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rear_tm_data *rear_tm = get_rear_tm(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			rear_tm->enabled);
}

static DEVICE_ATTR(thermalstate, S_IRUGO | S_IWUSR,
		rear_sysfs_thermalstate_show, rear_sysfs_thermalstate_store);
static DEVICE_ATTR(temp, S_IRUGO | S_IWUSR, rear_sysfs_temp_show, NULL);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,
		rear_sysfs_enable_show, rear_sysfs_enable_store);

static struct attribute *rear_tm_attributes[] = {
	&dev_attr_thermalstate.attr,
	&dev_attr_temp.attr,
	&dev_attr_enable.attr,
	NULL
};

static const struct attribute_group rear_tm_attr_group = {
	.attrs = rear_tm_attributes,
};

static int rear_param_get_index(struct rear_tm_data *rear_tm, int action)
{
	int i;

	for (i = 0; i < rear_tm->warm_cfg_size; i++) {
		if (rear_tm->warm_cfg[i].action == action)
			return i;
	}

	return -EINVAL;
}

static ssize_t rear_param_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, ret;
	struct rear_tm_data *rear_tm = get_rear_tm(dev);


	for (i = 0; i < REAR_POWEROFF; i++) {
		if (!strcmp(attr->attr.name, rear_action_str[i]))
			break;
	}

	if (i == REAR_POWEROFF) {
		pr_err("fail to find thermal action.\n");
		return -EINVAL;
	}

	ret = rear_param_get_index(rear_tm, i);
	if (ret < 0) {
		pr_err("no parameter for action %d\n", i);
		return -EINVAL;
	}
	i = ret;

	return snprintf(buf, PAGE_SIZE, "%d,%d\n",
			rear_tm->warm_cfg[i].thresh,
			rear_tm->warm_cfg[i].thresh_clr);
}

static ssize_t rear_param_attr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int i, ret;
	int thresh, thresh_clr;
	struct rear_tm_data *rear_tm = get_rear_tm(dev);

	if (sscanf(buf, "%d,%d", &thresh, &thresh_clr) != 2)
		return -EINVAL;

	for (i = 0; i < REAR_POWEROFF; i++) {
		if (!strcmp(attr->attr.name, rear_action_str[i]))
			break;
	}

	if (i == REAR_POWEROFF) {
		pr_err("fail to find thermal action.\n");
		return -EINVAL;
	}

	if (thresh_clr >= thresh) {
		pr_err("thresh_clr should be less than thresh\n");
		return -EINVAL;
	}

	ret = rear_param_get_index(rear_tm, i);
	if (ret < 0) {
		pr_err("no parameter for action %d\n", i);
		return -EINVAL;
	}
	i = ret;

	rear_tm->warm_cfg[i].thresh = thresh;
	rear_tm->warm_cfg[i].thresh_clr = thresh_clr;
	pr_info("rear_tm change idx(%d) thresh(%d) thresh_clr(%d)\n", i,
		rear_tm->warm_cfg[i].thresh,
		rear_tm->warm_cfg[i].thresh_clr);

	rear_tm_notification_init(rear_tm);
	return count;
}

static DEVICE_ATTR(call_alert, S_IRUGO | S_IWUSR,
		rear_param_attr_show, rear_param_attr_store);
static DEVICE_ATTR(call_dropped, S_IRUGO | S_IWUSR,
		rear_param_attr_show, rear_param_attr_store);
static DEVICE_ATTR(poweroff, S_IRUGO | S_IWUSR,
		rear_param_attr_show, rear_param_attr_store);

static struct attribute *rear_tm_param_attributes[] = {
	&dev_attr_call_alert.attr,
	&dev_attr_call_dropped.attr,
	&dev_attr_poweroff.attr,
	NULL,
};

static struct attribute_group rear_tm_param_group = {
	.name = "parameters",
	.attrs = rear_tm_param_attributes,
};

static void rear_tm_action(struct rear_tm_data *rear_tm, int action)
{
	if (action < 0 || action >= REAR_ACTION_MAX) {
		pr_warn("unknown action(%d)\n", action);
		return;
	}

	pr_debug("action: %s(%d)\n", rear_action_str[action], action);

	rear_tm->thermalstate = action;

	if (action < REAR_POWEROFF)
		switch_set_state(&rear_tm->sdev, action);
}

static int rear_tm_get_level(struct rear_tm_data *rear_tm,
		enum qpnp_tm_state state)
{
	int max_size = rear_tm->warm_cfg_size - 1;
	int i = max_size;
	if (state == ADC_TM_WARM_STATE) {
		while((rear_tm->adc_param.high_temp !=
				rear_tm->warm_cfg[i].thresh) && (i > 0))
			i--;
	} else {
		while((rear_tm->adc_param.low_temp !=
				rear_tm->warm_cfg[i].thresh_clr) && (i > 0))
			i--;
	}
	return i;
}

static void rear_tm_notification(enum qpnp_tm_state state, void *ctx)
{
	struct rear_tm_data *rear_tm = ctx;
	int i, next;
	int ret;
	int cur_temp;
	struct qpnp_vadc_result results;
	int max_size = rear_tm->warm_cfg_size - 1;

	if (state >= ADC_TM_STATE_NUM) {
		pr_err("invalid notification %d\n", state);
		return;
	}

	__pm_stay_awake(&rear_tm->ws);

	if (state == ADC_TM_WARM_STATE)
		cur_temp = rear_tm->adc_param.high_temp;
	else
		cur_temp = rear_tm->adc_param.low_temp;

	ret = qpnp_vadc_read(rear_tm->vadc_dev, P_MUX8_1_1, &results);
	if (!ret)
		cur_temp = (int)results.physical;
	else
		pr_warn("Unable to read vbat ret=%d\n", ret);

	pr_info("state: %s, temp: %d\n",
			state == ADC_TM_WARM_STATE ? "warm" : "cool",
			cur_temp);

	i = rear_tm_get_level(rear_tm, state);
	if (state == ADC_TM_WARM_STATE) {
		next = min(i+1, max_size);
		rear_tm_action(rear_tm, rear_tm->warm_cfg[i].action);
	} else {
		next = max(i-1, 0);
		rear_tm_action(rear_tm, REAR_NORMAL); // clear state
	}
	rear_tm->adc_param.high_temp = rear_tm->warm_cfg[next].thresh;
	rear_tm->adc_param.low_temp = rear_tm->warm_cfg[next].thresh_clr;

	if (state == ADC_TM_WARM_STATE) {
		rear_tm->adc_param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
		if (i == max_size)
			rear_tm->adc_param.timer_interval = ADC_MEAS1_INTERVAL_16S;
	} else {
		rear_tm->adc_param.timer_interval = ADC_MEAS1_INTERVAL_1S;
		if (i == 0)
			rear_tm->adc_param.state_request = ADC_TM_LOW_THR_ENABLE;
	}

	pr_info("next low temp = %d next high temp = %d\n",
					rear_tm->adc_param.low_temp,
					rear_tm->adc_param.high_temp);

	ret = qpnp_adc_tm_channel_measure(rear_tm->adc_tm_dev,
						&rear_tm->adc_param);
	if (ret)
		pr_warn("request adc_tm error\n");

	__pm_relax(&rear_tm->ws);
}

static int rear_tm_notification_init(struct rear_tm_data *rear_tm)
{
	int ret;

	rear_tm->adc_param.low_temp = rear_tm->warm_cfg[0].thresh_clr;
	rear_tm->adc_param.high_temp = rear_tm->warm_cfg[0].thresh;
	rear_tm->adc_param.state_request = ADC_TM_LOW_THR_ENABLE;
	rear_tm->adc_param.timer_interval = ADC_MEAS1_INTERVAL_1S;
	rear_tm->adc_param.btm_ctx = rear_tm;
	rear_tm->adc_param.threshold_notification = rear_tm_notification;
	rear_tm->adc_param.channel = P_MUX8_1_1;

	ret = qpnp_adc_tm_channel_measure(rear_tm->adc_tm_dev,
						&rear_tm->adc_param);
	if (ret)
		pr_err("request adc_tm error %d\n", ret);

	return ret;
}

static int rear_tm_parse_dt(struct device_node *np,
				struct rear_tm_data *rear_tm)
{
	int ret;

	rear_tm->adc_tm_dev = qpnp_get_adc_tm(rear_tm->dev, "rear-tm");
	if (IS_ERR(rear_tm->adc_tm_dev)) {
		ret = PTR_ERR(rear_tm->adc_tm_dev);
		pr_err("adc-tm not ready\n");
		goto out;
	}

	rear_tm->vadc_dev = qpnp_get_vadc(rear_tm->dev, "rear");
	if (IS_ERR(rear_tm->vadc_dev)) {
		ret = PTR_ERR(rear_tm->vadc_dev);
		if (ret != -EPROBE_DEFER)
			pr_err("vadc property missing\n");
		goto out;
	}

	if (!of_get_property(np, "tm,warm-cfg",
				&rear_tm->warm_cfg_size)) {
		pr_err("failed to get warm_cfg\n");
		ret = -EINVAL;
		goto out;
	}

	rear_tm->warm_cfg = devm_kzalloc(rear_tm->dev,
				rear_tm->warm_cfg_size, GFP_KERNEL);
	if (!rear_tm->warm_cfg) {
		pr_err("Unable to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, "tm,warm-cfg",
					(u32 *)rear_tm->warm_cfg,
					rear_tm->warm_cfg_size / sizeof(u32));
	if (ret) {
		pr_err("failed to get tm,warm-cfg\n");
		goto out;
	}

	rear_tm->warm_cfg_size /= sizeof(struct tm_ctrl_data);

out:
	return ret;
}

static int rear_tm_probe(struct spmi_device *spmi)
{
	struct rear_tm_data *rear_tm;
	struct device_node *dev_node = spmi->dev.of_node;
	struct qpnp_vadc_result results;
	int ret = 0;

	rear_tm = devm_kzalloc(&spmi->dev,
			sizeof(struct rear_tm_data), GFP_KERNEL);
	if (!rear_tm) {
		pr_err("failed to alloc memory\n");
		return -ENOMEM;
	}

	rear_tm->dev = &spmi->dev;
	rear_tm->spmi = spmi;
	rear_tm->enabled = 1;
	rear_tm->thermalstate = REAR_NORMAL;
	dev_set_drvdata(&spmi->dev, rear_tm);

	if (dev_node) {
		ret = rear_tm_parse_dt(dev_node, rear_tm);
		if (ret) {
			pr_err("failed to parse dt\n");
			return ret;
		}
	} else {
		pr_err("not supported for non OF\n");
		return -ENODEV;
	}

	ret = qpnp_vadc_read(rear_tm->vadc_dev, P_MUX8_1_1, &results);
	if (ret) {
		pr_err("Unable to read vbat ret=%d\n", ret);
		return ret;
	}
	pr_info("rear temp physical: %lld\n", results.physical);

	wakeup_source_init(&rear_tm->ws, "rear_tm_ws");

	rear_tm->sdev.name = "thermalstate";
	ret = switch_dev_register(&rear_tm->sdev);
	if (ret < 0) {
		pr_err("%s: failed to register switch device\n", __func__);
		goto err_switch_dev_register;
	}

	/* We want the initial state to be normal */
	switch_set_state(&rear_tm->sdev, REAR_NORMAL);

	ret = rear_tm_notification_init(rear_tm);
	if (ret) {
		pr_err("failed to init adc tm\n");
		goto err_rear_tm_notification_init;
	}

	ret = sysfs_create_group(&rear_tm->sdev.dev->kobj, &rear_tm_attr_group);
	if (ret < 0) {
		pr_err("failed to create sysfs thermalstate attribute\n");
		goto err_sysfs_create_group_1;
	}

	ret = sysfs_create_group(&rear_tm->sdev.dev->kobj, &rear_tm_param_group);
	if (ret < 0) {
		pr_err("failed to create param group attributes\n");
		goto err_sysfs_create_group_2;
	}

	return 0;

err_sysfs_create_group_2:
	sysfs_remove_group(&rear_tm->sdev.dev->kobj, &rear_tm_attr_group);

err_sysfs_create_group_1:
	qpnp_adc_tm_disable_chan_meas(rear_tm->adc_tm_dev,
			&rear_tm->adc_param);

err_rear_tm_notification_init:
	switch_dev_unregister(&rear_tm->sdev);

err_switch_dev_register:
	wakeup_source_trash(&rear_tm->ws);
	dev_set_drvdata(&spmi->dev, NULL);
	return ret;
}

static int rear_tm_remove(struct spmi_device *spmi)
{
	struct rear_tm_data *rear_tm = dev_get_drvdata(&spmi->dev);

	sysfs_remove_group(&rear_tm->sdev.dev->kobj, &rear_tm_param_group);
	sysfs_remove_group(&rear_tm->sdev.dev->kobj, &rear_tm_attr_group);
	qpnp_adc_tm_disable_chan_meas(rear_tm->adc_tm_dev,
			&rear_tm->adc_param);
	switch_dev_unregister(&rear_tm->sdev);
	wakeup_source_trash(&rear_tm->ws);
	dev_set_drvdata(&spmi->dev, NULL);

	return 0;
}

static struct of_device_id rear_tm_match[] = {
	{.compatible = "rear_tm", },
	{}
};

static struct spmi_driver rear_tm_driver = {
	.probe = rear_tm_probe,
	.remove = rear_tm_remove,
	.driver = {
		.name = "rear_tm_drv",
		.owner = THIS_MODULE,
		.of_match_table = rear_tm_match,
	},
};

static int __init rear_tm_init(void)
{
	return spmi_driver_register(&rear_tm_driver);
}
late_initcall(rear_tm_init);

static void __exit rear_tm_exit(void)
{
	spmi_driver_unregister(&rear_tm_driver);
}
module_exit(rear_tm_exit);

MODULE_DESCRIPTION("REAR Temp Monitor Driver");
MODULE_AUTHOR("kinam kim <kinam119.kim@lge.com>");
MODULE_LICENSE("GPL");
