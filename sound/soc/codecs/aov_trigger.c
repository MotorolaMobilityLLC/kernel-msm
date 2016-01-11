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

#include <linux/export.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <sound/soc.h>
#include "arizona.h"

#define MAX_DSP_TO_CHECK 4
#define MAX_NUM_PANIC_CODE 4

struct dsp_event_info {
	int panic;
	int event;
	u16 panic_code[MAX_NUM_PANIC_CODE];
};

static struct kobject aov_trigger_kobj;
static struct snd_soc_codec *aov_codec;

static struct dsp_event_info dsp_info[MAX_DSP_TO_CHECK];
static DEFINE_MUTEX(dsp_info_mutex);

static struct attribute aov_sysfs_attr_trigger = {
	.name = "trigger",
	.mode = S_IRUSR | S_IRGRP | S_IROTH
};

static struct attribute aov_sysfs_attr_register = {
	.name = "register",
	.mode = S_IWUSR | S_IWGRP | S_IWOTH
};

static struct attribute aov_sysfs_attr_event = {
	.name = "event",
	.mode = S_IRUSR | S_IRGRP | S_IROTH
};

static const char *reg_cmd = "register";
static const char *unreg_cmd = "unregister";

static void aov_trigger_notify(void)
{
	if (aov_codec != NULL) {
		/*
		  Once a trigger is received, unregister from
		  trigger notification. Client needs to re-arm
		  the rigger.
		*/
		arizona_set_ez2ctrl_cb(aov_codec, NULL);
		dev_info(aov_codec->dev, "notify aov trigger.");
		sysfs_notify(&aov_trigger_kobj, NULL,
			     aov_sysfs_attr_trigger.name);
	}
}

static void aov_panic_notify(int dsp, u16 *msg)
{
	if (dsp < 0 || dsp >= MAX_DSP_TO_CHECK)
		return;

	if (aov_codec == NULL)
		return;

	mutex_lock(&dsp_info_mutex);
	dsp_info[dsp].panic = 1;
	if (msg != NULL) {
		memcpy(dsp_info[dsp].panic_code, msg,
		       sizeof(u16) * MAX_NUM_PANIC_CODE);
	}
	mutex_unlock(&dsp_info_mutex);
	dev_info(aov_codec->dev, "aov notify panic.");
	sysfs_notify(&aov_trigger_kobj, NULL,
		     aov_sysfs_attr_event.name);
}

static void aov_event_notify(int dsp)
{
	if (dsp < 0 || dsp >= MAX_DSP_TO_CHECK)
		return;

	if (aov_codec == NULL)
		return;

	mutex_lock(&dsp_info_mutex);
	dsp_info[dsp].event = 1;
	mutex_unlock(&dsp_info_mutex);
	dev_info(aov_codec->dev, "aov notify event.");
	sysfs_notify(&aov_trigger_kobj, NULL,
		     aov_sysfs_attr_event.name);
}

static void aov_register_trigger(void)
{
	if (aov_codec != NULL) {
		arizona_set_ez2ctrl_cb(aov_codec, aov_trigger_notify);
		dev_info(aov_codec->dev, "registered aov trigger.");
	}
}

static void aov_unregister_trigger(void)
{
	if (aov_codec != NULL) {
		arizona_set_ez2ctrl_cb(aov_codec, NULL);
		dev_info(aov_codec->dev, "unregistered aov trigger.");
	}
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

int aov_trigger_init(struct snd_soc_codec *codec)
{
	int ret = 0;

	if (!aov_codec) {
		ret = kobject_init_and_add(&aov_trigger_kobj,
					   &ktype_aov_trigger,
					   kernel_kobj, "aov");
		if (ret)
			goto done;

		sysfs_attr_init(&aov_sysfs_attr_trigger);
		ret = sysfs_create_file(&aov_trigger_kobj,
					&aov_sysfs_attr_trigger);
		if (ret) {
			pr_err("%s: trigger node creation failed, ret=%d\n",
			       __func__, ret);
			goto done;
		}

		sysfs_attr_init(&aov_sysfs_attr_register);
		ret = sysfs_create_file(&aov_trigger_kobj,
					&aov_sysfs_attr_register);
		if (ret) {
			pr_err("%s: register node creation failed, ret=%d\n",
			       __func__, ret);
			goto done;
		}

		sysfs_attr_init(&aov_sysfs_attr_event);
		ret = sysfs_create_file(&aov_trigger_kobj,
					&aov_sysfs_attr_event);
		if (ret) {
			pr_err("%s: event node creation failed, ret=%d\n",
			       __func__, ret);
			goto done;
		}
		arizona_set_ez2panic_cb(codec, &aov_panic_notify);
		arizona_set_ez2text_cb(codec, &aov_event_notify);
		aov_codec = codec;
	}
done:
	return ret;
}
EXPORT_SYMBOL_GPL(aov_trigger_init);
