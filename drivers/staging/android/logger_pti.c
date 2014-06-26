/*
 * logger_pti.c - logger messages redirection to PTI
 *
 *  Copyright (C) Intel 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * To active logger to PTI messages, configure 'out' parameter
 * of 'logger_pti' module in sysfs with one or more values
 *  # echo "main,system" > /sys/module/logger_pti/parameters/out
 *
 * To active logger to PTI messages from boot, add this
 * commandline parameter to the boot commandline
 *  logger_pti.out=main,system
 *
 * Possible log buffers are : main, system, radio, events, kernel
 * See logger.h if others.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pti.h>
#include "logger.h"
#include "logger_kernel.h"

struct pti_plugin {
	char *log_name;
	bool enabled;
	struct logger_plugin *plugin;
	struct pti_masterchannel *mc;
	struct list_head list;
};

static LIST_HEAD(plugin_list);

/**
 * @logger_pti_init() - this callback function is called by logger.c
 * when a plug-in is added (via a call to logger_add_plugin)
 *
 * @cb_data: callback data for the plug-in (in our case it is a pointer
 *           to the pti_plugin structure
 */
static void logger_pti_init(void *cb_data)
{
	struct pti_plugin *pti_plugin;

	if (unlikely(cb_data == NULL))
		return;

	/* Channel-ID is reserved at plug-in initialization.
	 * Each plug-in (associated to a given logger) is associated
	 * with one channel-ID.
	 */
	pti_plugin = (struct pti_plugin *)cb_data;
	pti_plugin->mc = pti_request_masterchannel(1, pti_plugin->log_name);
}

/**
 * @logger_pti_exit() - this callback function is called by logger.c
 * when a plug-in is removed (via a call to logger_remove_plugin)
 *
 * @cb_data: callback data for the plug-in (in our case it is a pointer
 *           to the pti_plugin structure
 */
static void logger_pti_exit(void *cb_data)
{
	struct pti_plugin *pti_plugin;

	if (unlikely(cb_data == NULL))
		return;

	/* Release channel-ID when removing the plug-in */
	pti_plugin = (struct pti_plugin *)cb_data;
	pti_release_masterchannel(pti_plugin->mc);
}

/**
 * @logger_pti_write_seg() - this callback function is called by logger.c
 * when writing a segment of message (logger_aio_write)
 *
 * @buf:      data to be written (message segment)
 * @len:      length of the data to be written
 * @from_usr: true if data is from user-space
 * @som:      Start Of Message indication
 * @eom:      End Of Message indication
 * @cb_data:  callback data for the plug-in (in our case it is a pointer
 *            to the pti_plugin structure
 */
static void logger_pti_write_seg(void *buf, unsigned int len,
				 bool from_usr, bool som, bool eom,
				 void *cb_data)
{
	struct pti_plugin *pti_plugin = (struct pti_plugin *)cb_data;

	if (unlikely(pti_plugin == NULL))
		return;

	if (from_usr) {
		char *tmp_buf = kmalloc(len, GFP_KERNEL);
		if ((!tmp_buf) || copy_from_user(tmp_buf, buf, len)) {
			kfree(tmp_buf);
			return;
		}
		pti_writedata(pti_plugin->mc, (u8 *)tmp_buf, len, eom);
		kfree(tmp_buf);
	} else
		pti_writedata(pti_plugin->mc, (u8 *)buf, len, eom);
}

/**
 * @logger_pti_write_seg_recover() - this callback function is called
 * by logger.c when an issue is encountered while writing a segmented
 * message (logger_aio_write)
 *
 * @cb_data: callback data for the plug-in (in our case it is a pointer
 *           to the pti_plugin structure
 */
static void logger_pti_write_seg_recover(void *cb_data)
{
	/* An issue has occured in logger_aio_write function.
	 * To avoid messing up the STP flow, force the End Of Message
	 * indication by writing a zero byte.
	 */
	__u8 data = 0x00;
	struct pti_plugin *pti_plugin;

	if (unlikely(cb_data == NULL))
		return;

	pti_plugin = (struct pti_plugin *)cb_data;
	pti_writedata(pti_plugin->mc, &data, 1, true);
}

/**
 * @create_pti_plugin() - creates a @pti_plugin for a given logger
 *
 * @name: logger's name
 */
static int create_pti_plugin(const char *name)
{
	int ret = 0;
	struct logger_plugin *log_plugin;
	struct pti_plugin *pti_plugin;

	log_plugin = kzalloc(sizeof(struct logger_plugin), GFP_KERNEL);
	if (log_plugin == NULL)
		return -ENOMEM;

	pti_plugin = kzalloc(sizeof(struct pti_plugin), GFP_KERNEL);
	if (pti_plugin == NULL) {
		ret = -ENOMEM;
		goto out_free_log_plugin;
	}

	pti_plugin->log_name = kstrdup(name, GFP_KERNEL);
	pti_plugin->enabled = false;
	pti_plugin->plugin = log_plugin;

	log_plugin->init = logger_pti_init;
	log_plugin->exit = logger_pti_exit;
	log_plugin->write_seg = logger_pti_write_seg;
	log_plugin->write_seg_recover = logger_pti_write_seg_recover;
	log_plugin->data = (void *)pti_plugin;

	list_add_tail(&pti_plugin->list, &plugin_list);

	return 0;

out_free_log_plugin:
	kfree(log_plugin);
	return ret;
}

static int __init init_logger_pti(void)
{
	int ret;

	ret = create_pti_plugin(LOGGER_LOG_RADIO);
	if (unlikely(ret))
		goto out;

	ret = create_pti_plugin(LOGGER_LOG_EVENTS);
	if (unlikely(ret))
		goto out;

	ret = create_pti_plugin(LOGGER_LOG_SYSTEM);
	if (unlikely(ret))
		goto out;

	ret = create_pti_plugin(LOGGER_LOG_MAIN);
	if (unlikely(ret))
		goto out;

	ret = create_pti_plugin(LOGGER_LOG_KERNEL_BOT);
	if (unlikely(ret))
		goto out;

	return 0;

out:
	return ret;
}

static void __exit exit_logger_pti(void)
{
	struct pti_plugin *current_plugin, *next_plugin;

	list_for_each_entry_safe(current_plugin, next_plugin,
				 &plugin_list, list) {
		kfree(current_plugin->log_name);
		kfree(current_plugin->plugin);
		list_del(&current_plugin->list);
		kfree(current_plugin);
	}
}

module_init(init_logger_pti)
module_exit(exit_logger_pti)

/*
 * set_out - 'out' parameter set function from 'logger_pti' module
 *
 * called when writing to 'out' parameter from 'logger_pti' module in sysfs
 */
static int set_out(const char *val, struct kernel_param *kp)
{
	const char *name;
	struct pti_plugin *plugin;

	list_for_each_entry(plugin, &plugin_list, list) {
		name = plugin->log_name;

		/* remove "log_" in the log_name string */
		name += 4;

		/* hack: user asks for "kernel", but the
		 * plugin is actually associated to "kern_bot" logger
		 */
		if (!strcmp(name, "kern_bot"))
		    name = "kernel";

		if (strstr(val, name)) {
			if (plugin->enabled == false) {
				logger_add_plugin(plugin->plugin,
						  plugin->log_name);
				plugin->enabled = true;
			}
		} else if (plugin->enabled == true) {
			logger_remove_plugin(plugin->plugin, plugin->log_name);
			plugin->enabled = false;
		}
	}

	return 0;
}

/*
 * get_out - 'out' parameter get function from 'logger_pti' module
 *
 * called when reading 'out' parameter from 'logger_pti' module in sysfs
 */
static int get_out(char *buffer, struct kernel_param *kp)
{
	const char *name;
	const char *k = ",";
	struct pti_plugin *plugin;

	list_for_each_entry(plugin, &plugin_list, list) {
		if (plugin->enabled == true) {
			name = plugin->log_name;

			/* remove "log_" in the log_name string */
			name += 4;

			/* hack: if plugin is associated to "kern_bot" logger,
			 * user actually wants to see "kernel"
			 */
			if (!strcmp(name, "kern_bot"))
			    name = "kernel";

			strcat(buffer, name);
			strcat(buffer, k);
		}
	}
	buffer[strlen(buffer)-1] = '\0';

	return strlen(buffer);
}

module_param_call(out, set_out, get_out, NULL, 0644);
MODULE_PARM_DESC(out,
		 "configure logger to pti [main|events|radio|system|kernel]");

