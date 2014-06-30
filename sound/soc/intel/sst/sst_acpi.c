/* sst_acpi.c - SST (LPE) driver init file for ACPI enumeration.
 *
 * Copyright (c) 2013, Intel Corporation.
 *
 *  Authors:	Ramesh Babu K V <Ramesh.Babu@intel.com>
 *  Authors:	Omair Mohammed Abdullah <omair.m.abdullah@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <asm/platform_sst.h>
#include <acpi/acpi_bus.h>
#include <sound/intel_sst_ioctl.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

extern struct miscdevice lpe_ctrl;

int sst_workqueue_init(struct intel_sst_drv *ctx)
{
	pr_debug("%s", __func__);

	INIT_LIST_HEAD(&ctx->memcpy_list);
	INIT_LIST_HEAD(&ctx->libmemcpy_list);
	INIT_LIST_HEAD(&sst_drv_ctx->rx_list);
	INIT_LIST_HEAD(&ctx->ipc_dispatch_list);
	INIT_LIST_HEAD(&ctx->block_list);
	INIT_WORK(&ctx->ipc_post_msg.wq, ctx->ops->post_message);
	init_waitqueue_head(&ctx->wait_queue);

	ctx->mad_wq = create_singlethread_workqueue("sst_mad_wq");
	if (!ctx->mad_wq)
		goto err_wq;
	ctx->post_msg_wq =
		create_singlethread_workqueue("sst_post_msg_wq");
	if (!ctx->post_msg_wq)
		goto err_wq;
	return 0;
err_wq:
	return -EBUSY;
}

void sst_init_locks(struct intel_sst_drv *ctx)
{
	mutex_init(&ctx->stream_lock);
	mutex_init(&ctx->sst_lock);
	mutex_init(&ctx->mixer_ctrl_lock);
	mutex_init(&ctx->csr_lock);
	spin_lock_init(&sst_drv_ctx->rx_msg_lock);
	spin_lock_init(&ctx->ipc_spin_lock);
	spin_lock_init(&ctx->block_lock);
	spin_lock_init(&ctx->pvt_id_lock);
}

int sst_destroy_workqueue(struct intel_sst_drv *ctx)
{
	pr_debug("%s", __func__);
	if (ctx->mad_wq)
		destroy_workqueue(ctx->mad_wq);
	if (ctx->post_msg_wq)
		destroy_workqueue(ctx->post_msg_wq);
	return 0;
}

int sst_acpi_probe(struct platform_device *pdev)
{
	return -EINVAL;
}

int sst_acpi_remove(struct platform_device *pdev)
{
	return -EINVAL;
}

MODULE_DESCRIPTION("Intel (R) SST(R) Audio Engine ACPI Driver");
MODULE_AUTHOR("Ramesh Babu K V");
MODULE_AUTHOR("Omair Mohammed Abdullah");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("sst");
