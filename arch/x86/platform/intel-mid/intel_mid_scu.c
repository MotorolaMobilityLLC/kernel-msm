/*
 * intel_mid_scu.c: Intel MID SCU platform initialization code
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/sfi.h>
#include <linux/intel_pmic_gpio.h>
#include <linux/irq.h>
#include <linux/rpmsg.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <linux/platform_data/intel_mid_remoteproc.h>

struct rpmsg_ns_list nslist = {
	.list = LIST_HEAD_INIT(nslist.list),
	.lock = __MUTEX_INITIALIZER(nslist.lock),
};

static struct intel_mid_rproc_pdata intel_scu_pdata = {
	.name		= "intel_rproc_scu",
	.firmware	= "intel_mid/intel_mid_remoteproc.fw",
	.nslist		= &nslist,
};

static u64 intel_scu_dmamask = DMA_BIT_MASK(32);

static struct platform_device intel_scu_device = {
	.name		= "intel_rproc_scu",
	.id		= -1,
	.dev		= {
		.platform_data = &intel_scu_pdata,
		.dma_mask = &intel_scu_dmamask,
	},
};

void register_rpmsg_service(char *name, int id, u32 addr)
{
	struct rpmsg_ns_info *info;
	info = rpmsg_ns_alloc(name, id, addr);
	rpmsg_ns_add_to_list(info, &nslist);
}

int intel_mid_rproc_init(void)
{
	int err;

	/* generic rpmsg channels */
	register_rpmsg_service("rpmsg_ipc_command", RPROC_SCU, RP_IPC_COMMAND);
	register_rpmsg_service("rpmsg_ipc_simple_command",
				RPROC_SCU, RP_IPC_SIMPLE_COMMAND);
	register_rpmsg_service("rpmsg_ipc_raw_command",
				RPROC_SCU, RP_IPC_RAW_COMMAND);

	register_rpmsg_service("rpmsg_pmic", RPROC_SCU, RP_PMIC_ACCESS);
	register_rpmsg_service("rpmsg_mip", RPROC_SCU, RP_MIP_ACCESS);
	register_rpmsg_service("rpmsg_fw_update",
					RPROC_SCU, RP_FW_ACCESS);
	register_rpmsg_service("rpmsg_ipc_util",
					RPROC_SCU, RP_IPC_UTIL);
	register_rpmsg_service("rpmsg_flis", RPROC_SCU, RP_FLIS_ACCESS);
	register_rpmsg_service("rpmsg_watchdog", RPROC_SCU, RP_SET_WATCHDOG);
	register_rpmsg_service("rpmsg_umip", RPROC_SCU, RP_UMIP_ACCESS);
	register_rpmsg_service("rpmsg_osip", RPROC_SCU, RP_OSIP_ACCESS);
	register_rpmsg_service("rpmsg_vrtc", RPROC_SCU, RP_VRTC);
	register_rpmsg_service("rpmsg_fw_logging", RPROC_SCU, RP_FW_LOGGING);
	register_rpmsg_service("rpmsg_kpd_led", RPROC_SCU,
				RP_MSIC_KPD_LED);
	register_rpmsg_service("rpmsg_modem_nvram", RPROC_SCU,
					RP_IPC_RAW_COMMAND);
	register_rpmsg_service("rpmsg_mid_pwm", RPROC_SCU,
				RP_MSIC_PWM);

	err = platform_device_register(&intel_scu_device);
	if (err < 0)
		pr_err("Fail to register intel-mid-rproc platform device.\n");

	return 0;
}
arch_initcall_sync(intel_mid_rproc_init);
