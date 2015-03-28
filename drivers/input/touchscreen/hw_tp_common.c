/*Add for huawei TP*/
/*
 * Copyright (c) 2014 Huawei Device Company
 *
 * This file provide common requeirment for different touch IC.
 * 
 * 2014-01-04:Add "tp_get_touch_screen_obj" by sunlibin
 *
 */
#include <linux/hw_tp_common.h>
#include <linux/module.h>
#include <linux/cyttsp4_core.h>

//#include "synaptics_dsx_i2c.h"

static struct kobject *touch_screen_kobject_ts = NULL;
static struct kobject *touch_glove_func_ts = NULL;

struct kobject *tp_get_touch_screen_obj(void)
{
	if (NULL == touch_screen_kobject_ts) {
		touch_screen_kobject_ts =
		    kobject_create_and_add("touch_screen", NULL);
		if (!touch_screen_kobject_ts) {
			tp_log_err("%s: create touch_screen kobjetct error!\n",
				   __func__);
			return NULL;
		} else {
			tp_log_debug
			    ("%s: create sys/touch_screen successful!\n",
			     __func__);
		}
	} else {
		tp_log_debug("%s: sys/touch_screen already exist!\n", __func__);
	}

	return touch_screen_kobject_ts;
}

struct kobject *tp_get_glove_func_obj(void)
{
	struct kobject *properties_kobj;

	properties_kobj = tp_get_touch_screen_obj();
	if (NULL == properties_kobj) {
		tp_log_err("%s: Error, get kobj failed!\n", __func__);
		return NULL;
	}

	if (NULL == touch_glove_func_ts) {
		touch_glove_func_ts =
		    kobject_create_and_add("glove_func", properties_kobj);
		if (!touch_glove_func_ts) {
			tp_log_err("%s: create glove_func kobjetct error!\n",
				   __func__);
			return NULL;
		} else {
			tp_log_debug
			    ("%s: create sys/touch_screen/glove_func successful!\n",
			     __func__);
		}
	} else {
		tp_log_debug("%s: sys/touch_screen/glove_func already exist!\n",
			     __func__);
	}

	return touch_glove_func_ts;
}


