/*
 * Copyright (C) 2013 Intel Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/module.h>

#include "reboot_target.h"

/* Currently registered Reboot Target setter  */
static struct reboot_target *var;

struct name2id {
	const char *name;
	int id;
};

static const unsigned int DEFAULT_TARGET_INDEX = 0;

static const struct name2id NAME2ID[] = {
	{ "main",       0x00 },
	{ "android",    0x00 },
	{ "charging",   0x0A },
	{ "recovery",   0x0C },
	{ "fastboot",   0x0E },
	{ "bootloader", 0x0E },
	{ "factory",    0x12 },
	{ "dnx",        0x14 },
	{ "ramconsole", 0x16 },
	{ "factory2",   0x18 },
};

#define ALLOW_FACTORY_PARAM_NAME "allow_factory="

static int reboot_target_name2id(const char *name)
{
	int i;
	char *allow_factory;

	allow_factory = strstr(saved_command_line, ALLOW_FACTORY_PARAM_NAME);
	if (!allow_factory && strstr(name, "factory"))
		return NAME2ID[DEFAULT_TARGET_INDEX].id;

	for (i = 0; i < ARRAY_SIZE(NAME2ID); i++)
		if (!strcmp(NAME2ID[i].name, name))
			return NAME2ID[i].id;

	return -EINVAL;
}

const char *reboot_target_id2name(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(NAME2ID); i++)
		if (NAME2ID[i].id == id)
			return NAME2ID[i].name;

	return "";
}

static int set_reboot_target(const char *name)
{
	int id;

	if (name == NULL) {
		pr_err("Error in %s: NULL target\n", __func__);
		return -EINVAL;
	}

	id  = reboot_target_name2id(name);
	if (id < 0) {
		pr_err("Error in %s: '%s' is not a valid target\n",
		       __func__, name );
		return -EINVAL;
	}

	return var ? var->set_reboot_target(name, id) : -ENODEV;
}

static int reboot_target_notify(struct notifier_block *notifier,
				unsigned long what, void *data)
{
	const char *target = (const char *)data;
	int ret;

	if (what != SYS_RESTART)
		goto out;

	if (!target || target[0] == '\0')
		target = NAME2ID[DEFAULT_TARGET_INDEX].name;

	ret = set_reboot_target(target);
	if (ret)
		pr_err("%s: Failed to set the reboot target, return=%d\n",
		       __func__, ret);

out:
	return NOTIFY_DONE;
}

static struct notifier_block reboot_target_notifier = {
	.notifier_call = reboot_target_notify,
};

int reboot_target_register(struct reboot_target *new)
{
	if (var)
		return -EBUSY;

	var = new;
	return 0;
}
EXPORT_SYMBOL(reboot_target_register);

int reboot_target_unregister(struct reboot_target *old)
{
	if (old && old == var) {
		var = NULL;
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(reboot_target_unregister);

int __init reboot_target_init(void)
{
	int ret = 0;

	ret = register_reboot_notifier(&reboot_target_notifier);
	if (ret)
		pr_err("%s: failed to register reboot_notifier\n", __func__);

	return ret;
}

void __exit reboot_target_exit(void)
{
	unregister_reboot_notifier(&reboot_target_notifier);
}

module_init(reboot_target_init);
module_exit(reboot_target_exit);

MODULE_AUTHOR("Jeremy Compostella <jeremy.compostella@intel.com>");
MODULE_DESCRIPTION("Intel Reboot Target");
MODULE_LICENSE("GPL v2");
