/*
 * rpmsg_mid_rpmsg.c - Intel RPMSG Driver
 *
 * Copyright (C) 2012 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/rpmsg.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_data/intel_mid_remoteproc.h>

#include <asm/intel_mid_rpmsg.h>

/* Instance for generic kernel IPC calls */
static struct rpmsg_device_data rpmsg_ddata[RPMSG_IPC_COMMAND_TYPE_NUM] = {
	[RPMSG_IPC_COMMAND] = {
		.name = "rpmsg_ipc_command",
		.rpdev = NULL,	/* initialized during driver probe */
		.rpmsg_instance = NULL, /* initialized during driver probe */
	},
	[RPMSG_IPC_SIMPLE_COMMAND] = {
		.name = "rpmsg_ipc_simple_command",
		.rpdev = NULL,
		.rpmsg_instance = NULL,
	},
	[RPMSG_IPC_RAW_COMMAND] = {
		.name = "rpmsg_ipc_raw_command",
		.rpdev = NULL,
		.rpmsg_instance = NULL,
	},
};

/* Providing rpmsg ipc generic interfaces.
 * Modules can call these API directly without registering rpmsg driver.
 *
 * The arg list is the same as intel_scu_ipc_command(),
 * so simply change intel_scu_ipc_command() to rpmsg_send_generic_command()
 */
int rpmsg_send_generic_command(u32 cmd, u32 sub,
				u8 *in, u32 inlen,
				u32 *out, u32 outlen)
{
	struct rpmsg_instance *rpmsg_ipc_instance =
		rpmsg_ddata[RPMSG_IPC_COMMAND].rpmsg_instance;

	return rpmsg_send_command(rpmsg_ipc_instance, cmd, sub,
					in, out, inlen, outlen);
}
EXPORT_SYMBOL(rpmsg_send_generic_command);

int rpmsg_send_generic_simple_command(u32 cmd, u32 sub)
{
	struct rpmsg_instance *rpmsg_ipc_instance =
		rpmsg_ddata[RPMSG_IPC_SIMPLE_COMMAND].rpmsg_instance;

	return rpmsg_send_simple_command(rpmsg_ipc_instance, cmd, sub);
}
EXPORT_SYMBOL(rpmsg_send_generic_simple_command);

int rpmsg_send_generic_raw_command(u32 cmd, u32 sub,
				   u8 *in, u32 inlen,
				   u32 *out, u32 outlen,
				   u32 dptr, u32 sptr)
{
	struct rpmsg_instance *rpmsg_ipc_instance =
		rpmsg_ddata[RPMSG_IPC_RAW_COMMAND].rpmsg_instance;

	return rpmsg_send_raw_command(rpmsg_ipc_instance, cmd, sub,
					in, out, inlen, outlen, sptr, dptr);
}
EXPORT_SYMBOL(rpmsg_send_generic_raw_command);

/* Global lock for rpmsg framework */
static struct rpmsg_lock global_lock = {
	.lock = __MUTEX_INITIALIZER(global_lock.lock),
	.locked_prev = 0,
	.pending = ATOMIC_INIT(0),
};

#define is_global_locked_prev		(global_lock.locked_prev)
#define get_global_locked_prev()	(global_lock.locked_prev++)
#define put_global_locked_prev()	(global_lock.locked_prev--)
#define global_locked_by_current	(global_lock.lock.owner == current)

void rpmsg_global_lock(void)
{
	atomic_inc(&global_lock.pending);
	mutex_lock(&global_lock.lock);
}
EXPORT_SYMBOL(rpmsg_global_lock);

void rpmsg_global_unlock(void)
{
	mutex_unlock(&global_lock.lock);
	if (!atomic_dec_and_test(&global_lock.pending))
		schedule();
}
EXPORT_SYMBOL(rpmsg_global_unlock);

static void rpmsg_lock(void)
{
	if (!mutex_trylock(&global_lock.lock)) {
		if (global_locked_by_current)
			get_global_locked_prev();
		else
			rpmsg_global_lock();
	} else
		atomic_inc(&global_lock.pending);
}

static void rpmsg_unlock(void)
{
	if (!is_global_locked_prev)
		rpmsg_global_unlock();
	else
		put_global_locked_prev();
}

int rpmsg_send_command(struct rpmsg_instance *instance, u32 cmd,
						u32 sub, u8 *in,
						u32 *out, u32 inlen,
						u32 outlen)
{
	int ret = 0;

	if (!instance) {
		pr_err("%s: Instance is NULL\n", __func__);
		return -EFAULT;
	}

	/* Hold global rpmsg lock */
	rpmsg_lock();

	mutex_lock(&instance->instance_lock);

	/* Prepare Tx buffer */
	instance->tx_msg->cmd = cmd;
	instance->tx_msg->sub = sub;
	instance->tx_msg->in = in;
	instance->tx_msg->out = out;
	instance->tx_msg->inlen = inlen;
	instance->tx_msg->outlen = outlen;

	/* Preapre Rx buffer */
	mutex_lock(&instance->rx_lock);
	instance->rx_msg->status = -1;
	mutex_unlock(&instance->rx_lock);
	INIT_COMPLETION(instance->reply_arrived);

	/* Send message to remote processor(SCU) using rpdev channel */
	ret = rpmsg_send_offchannel(
					instance->rpdev,
					instance->endpoint->addr,
					instance->rpdev->dst,
					instance->tx_msg,
					sizeof(*instance->tx_msg)
					);
	if (ret) {
		dev_err(&instance->rpdev->dev, "%s failed: %d\n",
						 __func__, ret);
		goto end;
	}

	if (0 == wait_for_completion_timeout(&instance->reply_arrived,
						RPMSG_TX_TIMEOUT)) {
		dev_err(&instance->rpdev->dev,
				"timeout: %d\n", ret);
		ret = -ETIMEDOUT;
		goto end;
	}

	mutex_lock(&instance->rx_lock);
	ret = instance->rx_msg->status;
	mutex_unlock(&instance->rx_lock);
end:
	mutex_unlock(&instance->instance_lock);
	rpmsg_unlock();

	return ret;
}
EXPORT_SYMBOL(rpmsg_send_command);

int rpmsg_send_raw_command(struct rpmsg_instance *instance, u32 cmd,
						u32 sub, u8 *in,
						u32 *out, u32 inlen,
						u32 outlen, u32 sptr,
						u32 dptr)
{
	int ret = 0;

	if (!instance) {
		pr_err("%s: Instance is NULL\n", __func__);
		return -EFAULT;
	}

	mutex_lock(&instance->instance_lock);
	instance->tx_msg->sptr = sptr;
	instance->tx_msg->dptr = dptr;
	mutex_unlock(&instance->instance_lock);

	ret = rpmsg_send_command(instance, cmd, sub, in, out, inlen, outlen);

	return ret;
}
EXPORT_SYMBOL(rpmsg_send_raw_command);

int rpmsg_send_simple_command(struct rpmsg_instance *instance, u32 cmd,
						u32 sub)
{
	int ret;

	ret = rpmsg_send_command(instance, cmd, sub, NULL, NULL, 0, 0);

	return ret;
}
EXPORT_SYMBOL(rpmsg_send_simple_command);

static void rpmsg_recv_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
#ifdef DEBUG_RPMSG_MSG
	static int rx_count;
#endif
	struct rpmsg_instance *instance = priv;

	if (len != sizeof(struct rx_ipc_msg)) {
		dev_warn(&rpdev->dev, "%s, incorrect msg length\n", __func__);
		return;
	}

#ifdef DEBUG_RPMSG_MSG
	dev_info(&rpdev->dev, "incoming msg %d (src: 0x%x)\n", ++rx_count, src);

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
#endif

	mutex_lock(&instance->rx_lock);

	memcpy(instance->rx_msg, data, len);

	mutex_unlock(&instance->rx_lock);

	complete(&instance->reply_arrived);

}

int alloc_rpmsg_instance(struct rpmsg_channel *rpdev,
				struct rpmsg_instance **pInstance)
{
	int ret = 0;
	struct rpmsg_instance *instance;

	dev_info(&rpdev->dev, "Allocating rpmsg_instance\n");

	instance = kzalloc(sizeof(*instance), GFP_KERNEL);
	if (!instance) {
		ret = -ENOMEM;
		dev_err(&rpdev->dev, "kzalloc rpmsg_instance failed\n");
		goto alloc_out;
	}

	instance->rpdev = rpdev;

	instance->tx_msg = kzalloc(sizeof(struct tx_ipc_msg), GFP_KERNEL);
	if (!instance->tx_msg) {
		ret = -ENOMEM;
		dev_err(&rpdev->dev, "kzalloc instance tx_msg failed\n");
		goto error_tx_msg_create;
	}

	instance->rx_msg = kzalloc(sizeof(struct rx_ipc_msg), GFP_KERNEL);
	if (!instance->rx_msg) {
		ret = -ENOMEM;
		dev_err(&rpdev->dev, "kzalloc instance rx_msg failed\n");
		goto error_rx_msg_create;
	}

	instance->endpoint = rpmsg_create_ept(rpdev, rpmsg_recv_cb,
							instance,
							RPMSG_ADDR_ANY);
	if (!instance->endpoint) {
		dev_err(&rpdev->dev, "create instance endpoint failed\n");
		ret = -ENOMEM;
		goto error_endpoint_create;
	}

	goto alloc_out;

error_endpoint_create:
	kfree(instance->rx_msg);
	instance->rx_msg = NULL;
error_rx_msg_create:
	kfree(instance->tx_msg);
	instance->tx_msg = NULL;
error_tx_msg_create:
	kfree(instance);
	instance = NULL;
alloc_out:
	*pInstance = instance;
	return ret;

}
EXPORT_SYMBOL(alloc_rpmsg_instance);

void free_rpmsg_instance(struct rpmsg_channel *rpdev,
				struct rpmsg_instance **pInstance)
{
	struct rpmsg_instance *instance = *pInstance;

	mutex_lock(&instance->instance_lock);
	rpmsg_destroy_ept(instance->endpoint);
	kfree(instance->tx_msg);
	instance->tx_msg = NULL;
	kfree(instance->rx_msg);
	instance->rx_msg = NULL;
	mutex_unlock(&instance->instance_lock);
	kfree(instance);
	*pInstance = NULL;
	dev_info(&rpdev->dev, "Freeing rpmsg device\n");
}
EXPORT_SYMBOL(free_rpmsg_instance);

void init_rpmsg_instance(struct rpmsg_instance *instance)
{
	init_completion(&instance->reply_arrived);
	mutex_init(&instance->instance_lock);
	mutex_init(&instance->rx_lock);
}
EXPORT_SYMBOL(init_rpmsg_instance);

static int rpmsg_ipc_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;
	int i;
	struct rpmsg_device_data *ddata = rpmsg_ddata;

	if (rpdev == NULL) {
		pr_err("rpmsg channel %s not created\n", rpdev->id.name);
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed rpmsg_ipc device %s\n", rpdev->id.name);

	for (i = RPMSG_IPC_COMMAND; i < RPMSG_IPC_COMMAND_TYPE_NUM; i++) {
		if (!strncmp(rpdev->id.name, ddata[i].name, RPMSG_NAME_SIZE)) {

			/* Allocate rpmsg instance for kernel IPC calls*/
			ret = alloc_rpmsg_instance(rpdev,
					&ddata[i].rpmsg_instance);
			if (!ddata[i].rpmsg_instance) {
				dev_err(&rpdev->dev,
					"alloc rpmsg instance failed\n");
				goto out;
			}

			/* Initialize rpmsg instance */
			init_rpmsg_instance(ddata[i].rpmsg_instance);

			ddata[i].rpdev = rpdev;
			break;
		}
	}

out:
	return ret;
}

static void rpmsg_ipc_remove(struct rpmsg_channel *rpdev)
{
	int i;
	struct rpmsg_device_data *ddata = rpmsg_ddata;

	for (i = RPMSG_IPC_COMMAND; i < RPMSG_IPC_COMMAND_TYPE_NUM; i++) {
		if (!strncmp(rpdev->id.name, ddata[i].name, RPMSG_NAME_SIZE)) {
			free_rpmsg_instance(rpdev, &ddata[i].rpmsg_instance);
			break;
		}
	}
	dev_info(&rpdev->dev, "Removed rpmsg_ipc device\n");
}

static void rpmsg_ipc_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id rpmsg_ipc_id_table[] = {
	{ .name	= "rpmsg_ipc_command" },
	{ .name	= "rpmsg_ipc_simple_command" },
	{ .name	= "rpmsg_ipc_raw_command" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_ipc_id_table);

static struct rpmsg_driver rpmsg_ipc = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_ipc_id_table,
	.probe		= rpmsg_ipc_probe,
	.callback	= rpmsg_ipc_cb,
	.remove		= rpmsg_ipc_remove,
};

static int __init rpmsg_ipc_init(void)
{
	return register_rpmsg_driver(&rpmsg_ipc);
}
subsys_initcall(rpmsg_ipc_init);

static void __exit rpmsg_ipc_exit(void)
{
	return unregister_rpmsg_driver(&rpmsg_ipc);
}
module_exit(rpmsg_ipc_exit);

MODULE_AUTHOR("Ning Li<ning.li@intel.com>");
MODULE_DESCRIPTION("Intel IPC RPMSG Driver");
MODULE_LICENSE("GPL v2");
