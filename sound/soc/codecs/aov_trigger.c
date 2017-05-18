/*
 * Copyright (c) 2014, Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <sound/soc.h>
#include <linux/mfd/madera/core.h>
#include "madera.h"

#define MAX_DSP_TO_CHECK 4
#define MAX_NUM_PANIC_CODE 4

struct dsp_event_info {
	int panic;
	int event;
	u16 panic_code[MAX_NUM_PANIC_CODE];
};
static bool aov_trigger_active;
static struct kobject aov_trigger_kobj;
static struct snd_soc_codec *aov_codec;
static struct notifier_block aov_trigger_nb;

static struct dsp_event_info dsp_info[MAX_DSP_TO_CHECK];
static DEFINE_MUTEX(dsp_info_mutex);

static struct attribute aov_sysfs_attr_trigger = {
	.name = "trigger",
	.mode = S_IRUSR | S_IRGRP
};

static struct attribute aov_sysfs_attr_register = {
	.name = "register",
	.mode = S_IWUSR | S_IWGRP
};

static struct attribute aov_sysfs_attr_event = {
	.name = "event",
	.mode = S_IRUSR | S_IRGRP
};

static const char *reg_cmd = "register";
static const char *unreg_cmd = "unregister";

static int aov_trigger_notify(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct madera_voice_trigger_info *trig_info = data;
	int dsp;

	if (aov_codec == NULL)
		return -EPERM;
	/*
	 * Once a trigger is received, unregister from
	 * trigger notification. Client needs to re-arm
	 * the rigger.
	 */

	dev_dbg(aov_codec->dev, "received madera notity event: 0x%lx", event);
	if ((event == MADERA_NOTIFY_VOICE_TRIGGER) &&
	    (trig_info->core_num >= 0) &&
	    (trig_info->core_num < MAX_DSP_TO_CHECK)) {

		dsp = trig_info->core_num;

		switch (trig_info->code) {
		case MADERA_TRIGGER_VOICE:
			if (!aov_trigger_active)
				return 0;
			aov_trigger_active = false;
			dev_info(aov_codec->dev,
				"DSP%d: notify aov trigger.", dsp);
			sysfs_notify(&aov_trigger_kobj, NULL,
				     aov_sysfs_attr_trigger.name);
			break;
		case MADERA_TRIGGER_TEXT:
			mutex_lock(&dsp_info_mutex);
			dsp_info[dsp].event = 1;
			mutex_unlock(&dsp_info_mutex);
			dev_info(aov_codec->dev,
				"DSP%d: aov notify text event.", dsp);
			sysfs_notify(&aov_trigger_kobj, NULL,
				     aov_sysfs_attr_event.name);
			break;
		case MADERA_TRIGGER_PANIC:
			mutex_lock(&dsp_info_mutex);
			dsp_info[dsp].panic = 1;
			memcpy(dsp_info[dsp].panic_code, trig_info->err_msg,
			       sizeof(u16) * MAX_NUM_PANIC_CODE);
			mutex_unlock(&dsp_info_mutex);
			dev_info(aov_codec->dev,
				"DSP%d: aov notify panic.", dsp);
			sysfs_notify(&aov_trigger_kobj, NULL,
				     aov_sysfs_attr_event.name);
			break;
		}
	}

	return 0;
}

static void aov_register_trigger(void)
{
	if (aov_codec == NULL)
		return;
	aov_trigger_active = true;
	dev_dbg(aov_codec->dev, "registered aov trigger.");
}

static void aov_unregister_trigger(void)
{
	if (aov_codec == NULL)
		return;
	aov_trigger_active = false;
	dev_dbg(aov_codec->dev, "unregistered aov trigger.");
}

static void aov_sysfs_release(struct kobject *kobj)
{
	aov_codec = NULL;
}

static ssize_t aov_sysfs_show(struct kobject *kobj,
				  struct attribute *attr, char *buf)
{
	int i, j;
	int count;
	char *ptr = buf;

	if (attr != &aov_sysfs_attr_event)
		return 0;

	mutex_lock(&dsp_info_mutex);
	for (i = 0; i < MAX_DSP_TO_CHECK; i++) {
		count = scnprintf(ptr, PAGE_SIZE - (ptr - buf),
				  "dsp %d: panic:%d event:%d code:",
				  i, dsp_info[i].panic, dsp_info[i].event);
		ptr += count;
		for (j = 0; j < MAX_NUM_PANIC_CODE; j++) {
			count = scnprintf(ptr, PAGE_SIZE - (ptr - buf),
					  "%04x", dsp_info[i].panic_code[j]);
			ptr += count;
		}
		count = scnprintf(ptr, PAGE_SIZE - (ptr - buf), "\n");
		ptr += count;
	}
	memset(dsp_info, 0, sizeof(struct dsp_event_info) * MAX_DSP_TO_CHECK);
	mutex_unlock(&dsp_info_mutex);

	return ptr - buf;
}

static ssize_t aov_sysfs_store(struct kobject *kobj,
				   struct attribute *attr,
				   const char *buf, size_t count)
{
	ssize_t ret = -EIO;

	if (attr != &aov_sysfs_attr_register)
		return ret;

	if (!strncmp(reg_cmd, buf, strlen(reg_cmd))) {
		aov_register_trigger();
		ret = count;
	} else if (!strncmp(unreg_cmd, buf, strlen(unreg_cmd))) {
		aov_unregister_trigger();
		ret = count;
	}
	return ret;
}

static const struct sysfs_ops aov_sysfs_ops = {
	.show = aov_sysfs_show,
	.store = aov_sysfs_store,
};

static struct kobj_type ktype_aov_trigger = {
	.sysfs_ops = &aov_sysfs_ops,
	.default_attrs = NULL,
	.release = aov_sysfs_release,
};

int aov_trigger_init(void)
{
	int ret = 0;
	static int aov_trigger_init_called;

	if (aov_trigger_init_called)
		return ret;
	aov_trigger_init_called = 1;

	ret = kobject_init_and_add(&aov_trigger_kobj,
				   &ktype_aov_trigger,
				   kernel_kobj, "aov");
	if (ret)
		goto exit;

	sysfs_attr_init(&aov_sysfs_attr_trigger);
	ret = sysfs_create_file(&aov_trigger_kobj,
				&aov_sysfs_attr_trigger);
	if (ret) {
		pr_err("%s: trigger node creation failed, ret=%d\n",
			   __func__, ret);
		goto exit_remove_kobj;
	}

	sysfs_attr_init(&aov_sysfs_attr_register);
	ret = sysfs_create_file(&aov_trigger_kobj,
				&aov_sysfs_attr_register);
	if (ret) {
		pr_err("%s: register node creation failed, ret=%d\n",
			   __func__, ret);
		goto exit_remove_trigger;
	}

	sysfs_attr_init(&aov_sysfs_attr_event);
	ret = sysfs_create_file(&aov_trigger_kobj,
				&aov_sysfs_attr_event);
	if (ret) {
		pr_err("%s: event node creation failed, ret=%d\n",
			   __func__, ret);
		goto exit_remove_register;
	}
	goto exit;

exit_remove_register:
	sysfs_remove_file(&aov_trigger_kobj,
			&aov_sysfs_attr_register);
exit_remove_trigger:
	sysfs_remove_file(&aov_trigger_kobj,
			&aov_sysfs_attr_trigger);
exit_remove_kobj:
	kobject_put(&aov_trigger_kobj);
exit:
	return ret;
}
EXPORT_SYMBOL_GPL(aov_trigger_init);

void aov_trigger_register_notifier(struct snd_soc_codec *codec)
{
	if (!aov_codec) {
		aov_trigger_nb.notifier_call = aov_trigger_notify;
		aov_codec = codec;
		madera_register_notifier(aov_codec, &aov_trigger_nb);
	}
}
EXPORT_SYMBOL_GPL(aov_trigger_register_notifier);
