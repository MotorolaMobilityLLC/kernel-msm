/* Copyright (c) 2016, Motorola Mobility, LLC.
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

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mod_display.h>
#include <linux/mod_display_ops.h>
#include <linux/mod_display_comm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>

static LIST_HEAD(impl_list_head);
static DEFINE_MUTEX(list_lock);

static struct mod_display_impl_data *mod_display_impl;
static struct mod_display_comm_data *mod_display_comm;
static int initialized;

/* External APIs */

int mod_display_get_display_config(
	struct mod_display_panel_config **display_config)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!mod_display_comm) {
		pr_err("%s: No comm interface ready!\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	ret = mod_display_comm->ops->get_display_config(
		mod_display_comm->ops->data, display_config);
	if (ret) {
		pr_err("%s: ret: %d\n", __func__, ret);
		goto exit;
	}

	pr_debug("%s: === EDID BLOCK ===\n", __func__);
	print_hex_dump_debug("HDMI EDID: ", DUMP_PREFIX_NONE, 16, 1,
		(*display_config)->edid_buf, (*display_config)->edid_buf_size,
		false);
	pr_debug("%s: === EDID BLOCK ===\n", __func__);

exit:
	pr_debug("%s-\n", __func__);

	return ret;
}

int mod_display_set_display_config(u8 index)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!mod_display_impl || !mod_display_comm) {
		pr_err("%s: Interfaces are not setup!\n", __func__);
		ret = -ENODEV;
	} else
		ret = mod_display_comm->ops->set_display_config(
			mod_display_comm->ops->data, index);

	pr_debug("%s-\n", __func__);

	return ret;
}

int mod_display_get_display_state(u8 *state)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!mod_display_impl || !mod_display_comm) {
		pr_err("%s: Interfaces are not setup!\n", __func__);
		ret = -ENODEV;
	} else
		ret = mod_display_comm->ops->get_display_state(
			mod_display_comm->ops->data, state);

	pr_debug("%s-\n", __func__);

	return ret;
}

int mod_display_set_display_state(u8 state)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!mod_display_impl || !mod_display_comm) {
		pr_err("%s: Interfaces are not setup!\n", __func__);
		ret = -ENODEV;
	} else
		ret = mod_display_comm->ops->set_display_state(
			mod_display_comm->ops->data, state);


	pr_debug("%s-\n", __func__);

	return ret;
}

/* Internal APIs */

int mod_display_notification(enum mod_display_notification event)
{
	int ret = 0;
	struct mod_display_impl_data *cur;
	struct mod_display_panel_config *display_config;

	pr_debug("%s+\n", __func__);

	if (!mod_display_comm) {
		pr_err("%s: No comm interface ready!\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	switch (event) {
	case MOD_NOTIFY_AVAILABLE:
		if (mod_display_impl) {
			pr_err("%s: Implementation already setup!\n", __func__);
			ret = -EBUSY;
			break;
		}

		ret = mod_display_get_display_config(&display_config);
		if (ret) {
			pr_err("%s: Unable to get display config (ret: %d)",
				__func__, ret);
			break;
		}

		mutex_lock(&list_lock);
		list_for_each_entry(cur, &impl_list_head, list) {
			if (cur->mod_display_type ==
				display_config->display_type) {
				mod_display_impl = cur;
				pr_debug("%s: Using impl for type: %d\n",
					__func__, display_config->display_type);
				break;
			}
		}
		mutex_unlock(&list_lock);

		if (mod_display_impl) {
			if (mod_display_impl->ops->handle_available)
				mod_display_impl->ops->handle_available(
					mod_display_impl->ops->data);
		} else {
			pr_err("%s: Unable to find impl for type: %d\n",
				__func__, display_config->display_type);
			ret = -EINVAL;
		}

		kfree(display_config);
		break;
	case MOD_NOTIFY_UNAVAILABLE:
		if (!mod_display_impl) {
			pr_err("%s: No implementation ready!\n", __func__);
			ret = -ENODEV;
			break;
		}

		if (mod_display_impl->ops->handle_unavailable)
			mod_display_impl->ops->handle_unavailable(
				mod_display_impl->ops->data);

		mod_display_impl = NULL;

		break;
	case MOD_NOTIFY_CONNECT:
		if (!mod_display_impl) {
			pr_err("%s: No implementation ready!\n", __func__);
			ret = -ENODEV;
			break;
		}

		if (mod_display_impl->ops->handle_connect)
			mod_display_impl->ops->handle_connect(
				mod_display_impl->ops->data);

		break;
	case MOD_NOTIFY_DISCONNECT:
		if (!mod_display_impl) {
			pr_err("%s: No implementation ready!\n", __func__);
			ret = -ENODEV;
			break;
		}

		if (mod_display_impl->ops->handle_disconnect)
			mod_display_impl->ops->handle_disconnect(
				mod_display_impl->ops->data);

		break;
	case MOD_NOTIFY_FAILURE:
		pr_err("%s: Failure event received!\n", __func__);
		break;
	default:
		pr_err("%s: Invalid event: %d\n", __func__, event);
	}

exit:
	pr_debug("%s-\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mod_display_notification);

static struct dentry *debugfs_root;

int mod_display_debugfs_setup(struct miscdevice *device)
{
	BUG_ON(debugfs_root);

	debugfs_root = debugfs_create_dir(MOD_DISPLAY_NAME, NULL);
	if (IS_ERR_OR_NULL(debugfs_root)) {
		pr_err("Debugfs create dir failed with error: %ld\n",
					PTR_ERR(debugfs_root));
		return -ENODEV;
	}

	return 0;
}

int mod_display_debugfs_cleanup(struct miscdevice *device)
{
	BUG_ON(!debugfs_root);

	debugfs_remove_recursive(debugfs_root);

	debugfs_root = NULL;

	return 0;
}

static const struct file_operations mod_display_fops = {
	.owner = THIS_MODULE,
};

static struct miscdevice mod_display_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MOD_DISPLAY_NAME,
	.fops = &mod_display_fops,
};

int mod_display_init(void)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	BUG_ON(initialized);

	if (!list_empty(&impl_list_head) && mod_display_comm) {
		ret = misc_register(&mod_display_misc_device);

		mod_display_debugfs_setup(&mod_display_misc_device);

		ret = mod_display_comm->ops->host_ready(
			mod_display_comm->ops->data);

		initialized = 1;
	}

	pr_debug("%s-\n", __func__);

	return ret;
}

int mod_display_deinit(void)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (!initialized) {
		pr_warn("%s: NOT INITIALIZED!\n", __func__);
		goto exit;
	}

	mod_display_debugfs_cleanup(&mod_display_misc_device);

	ret = misc_deregister(&mod_display_misc_device);

	initialized = 0;

exit:
	pr_debug("%s-\n", __func__);

	return ret;
}

int mod_display_register_impl(struct mod_display_impl_data *impl)
{
	int ret = 0;
	struct mod_display_impl_data *cur;

	pr_debug("%s+\n", __func__);

	mutex_lock(&list_lock);

	list_for_each_entry(cur, &impl_list_head, list) {
		if (cur->mod_display_type == impl->mod_display_type) {
			pr_err("%s: Duplicate impl for type: %d\n", __func__,
				impl->mod_display_type);
			ret = -EEXIST;
			goto exit_mutex;
		}
	}

	list_add_tail(&impl->list, &impl_list_head);
	pr_err("%s: Registered impl for type: %d\n", __func__,
		impl->mod_display_type);

	ret = mod_display_init();

exit_mutex:
	mutex_unlock(&list_lock);

	pr_debug("%s-\n", __func__);

	return ret;
}

int mod_display_register_comm(struct mod_display_comm_data *comm)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	BUG_ON(mod_display_comm);

	mod_display_comm = comm;

	ret = mod_display_init();

	pr_debug("%s-\n", __func__);

	return ret;
}
EXPORT_SYMBOL(mod_display_register_comm);

int mod_display_unregister_comm(struct mod_display_comm_data *comm)
{
	int ret = 0;

	pr_debug("%s+\n", __func__);

	if (mod_display_impl) {
		pr_info("%s: Cleaning up a lost connection\n", __func__);
		mod_display_notification(MOD_NOTIFY_UNAVAILABLE);
	}

	BUG_ON(!mod_display_comm);
	BUG_ON(mod_display_comm != comm);

	ret = mod_display_deinit();

	mod_display_comm = NULL;

	pr_debug("%s-\n", __func__);

	return ret;

}
EXPORT_SYMBOL(mod_display_unregister_comm);

static void __exit mod_display_exit(void)
{
	if (initialized)
		misc_deregister(&mod_display_misc_device);
}

MODULE_AUTHOR("Jared Suttles <jared.suttles@motorola.com>");
MODULE_DESCRIPTION("Mod display driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

module_exit(mod_display_exit);
