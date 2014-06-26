/*
 * INTEL MID Remote Processor Head File
 *
 * Copyright (C) 2012 Intel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _ASM_INTEL_MID_REMOTEPROC_H
#define _ASM_INTEL_MID_REMOTEPROC_H

#define RP_IPC_COMMAND		0xA0
#define RP_IPC_SIMPLE_COMMAND	0xA1
#define RP_IPC_RAW_COMMAND	0xA2

#define	RP_PMIC_ACCESS		0xFF
#define	RP_DFU_REQUEST		0xFE
#define	RP_SET_WATCHDOG		0xF8
#define	RP_FLIS_ACCESS		0xF5
#define	RP_GET_FW_REVISION	0xF4
#define	RP_COLD_BOOT		0xF3
#define	RP_COLD_RESET		0xF1
#define	RP_COLD_OFF		0x80
#define	RP_MIP_ACCESS		0xEC
#define RP_GET_HOBADDR		0xE5
#define RP_OSC_CLK_CTRL		0xE6
#define RP_S0IX_COUNTER		0xE8
#define RP_WRITE_OSNIB		0xE4
#define RP_CLEAR_FABERROR	0xE3
#define RP_SCULOG_CTRL		0xE1
#define RP_FW_UPDATE		0xFE
#define RP_VRTC			0xFA
#define RP_PMDB			0xE0
#define RP_SCULOG_TRACE		0x90
#define RP_WRITE_OEMNIB		0xDF	/* Command is used to write OEMNIB */
					/* data. Used with extended OSHOB  */
					/* OSNIB only.                     */
/*
 * Assigning some temp ids for following devices
 * TODO: Need to change it to some meaningful
 *       values.
 */
#define RP_PMIC_GPIO		0X02
#define RP_PMIC_AUDIO		0x03
#define RP_MSIC_GPIO		0x05
#define RP_MSIC_AUDIO		0x06
#define RP_MSIC_OCD		0x07
#define RP_MSIC_BATTERY		0XEF
#define RP_MSIC_THERMAL		0x09
#define RP_MSIC_POWER_BTN	0x10
#define RP_IPC			0X11
#define RP_IPC_UTIL		0X12
#define RP_FW_ACCESS		0X13
#define RP_UMIP_ACCESS		0x14
#define RP_OSIP_ACCESS		0x15
#define RP_MSIC_ADC		0x16
#define RP_BQ24192		0x17
#define RP_MSIC_CLV_AUDIO	0x18
#define RP_PMIC_CCSM		0x19
#define RP_PMIC_I2C		0x20
#define RP_MSIC_MRFLD_AUDIO	0x21
#define RP_MSIC_PWM		0x22
#define RP_MSIC_KPD_LED		0x23
#define RP_BCOVE_ADC		0x24
#define RP_BCOVE_THERMAL	0x25
#define RP_MRFL_OCD		0x26
#define RP_FW_LOGGING		0x27
#define RP_PMIC_CHARGER		0x28

enum rproc_type {
	RPROC_SCU = 0,
	RPROC_PSH,
	RPROC_NUM,
};

struct rproc_ops;
struct platform_device;
struct rpmsg_ns_msg;

struct rpmsg_ns_info {
	enum rproc_type type;
	char name[RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
	struct list_head node;
};

struct rpmsg_ns_list {
	struct list_head list;
	struct mutex lock;
};

extern struct rpmsg_ns_info *rpmsg_ns_alloc(const char *name,
						int id, u32 addr);
extern void rpmsg_ns_add_to_list(struct rpmsg_ns_info *info,
					struct rpmsg_ns_list *nslist);

/*
 * struct intel_mid_rproc_pdata - intel mid remoteproc's platform data
 * @name: the remoteproc's name
 * @firmware: name of firmware file to load
 * @ops: start/stop rproc handlers
 * @device_enable: handler for enabling a device
 * @device_shutdown: handler for shutting down a device
 */
struct intel_mid_rproc_pdata {
	const char *name;
	const char *firmware;
	const struct rproc_ops *ops;
	int (*device_enable) (struct platform_device *pdev);
	int (*device_shutdown) (struct platform_device *pdev);
	struct rpmsg_ns_list *nslist;
};

#endif /* _ASM_INTEL_MID_REMOTEPROC_H */
