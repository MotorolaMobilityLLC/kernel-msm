/* Copyright (c) 2016 Cirrus Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _UAPI_MSM_CIRRUS_SPK_EN_H
#define _UAPI_MSM_CIRRUS_SPK_EN_H

#include <linux/types.h>
#include <linux/ioctl.h>


#define CIRRUS_SE			0x1000A1AF
#define CIRRUS_SE_ENABLE		0x10002001
#define CIRRUS_SE_PARAM_ID_CTRL		0x10002002

#define CRUS_MODULE_ID_RX		0x00000001

/*
 * CRUS_PARAM_RX_SET_USECASE
 * 0 = Music Playback in firmware
 * 1 = VOICE Playback in firmware
 * 2 = Tuning file loaded using external
 * config load command
 *
 * uses crus_rx_run_case_ctrl for RX apr pckt
 *
 */
#define CRUS_PARAM_RX_SET_USECASE	0x00A1AF02
/*
 * CRUS_PARAM_RX_GET_USECASE
 *
 *
 */
#define CRUS_PARAM_RX_GET_USECASE	0x00A1AF0B
/*
 * CRUS_PARAM_RX_SET_EXT_CONFIG
 * config string loaded from libfirmware
 * max of 7K parameters
 *
 * uses crus_external_config_t apr pckt
 */
#define CRUS_PARAM_RX_SET_EXT_CONFIG	0x00A1AF05
/*
 * CRUS_PARAM_RX_SET_DELTA_CONFIG
 * load seamless transition config string
 *
 * CRUS_PARAM_RX_RUN_DELTA_CONFIG
 * execute the loaded seamless transition
 */
#define CRUS_PARAM_RX_SET_DELTA_CONFIG	0x00A1AF0D
#define CRUS_PARAM_RX_RUN_DELTA_CONFIG	0x00A1AF0E
/*
 * CRUS_PARAM_RX_CHANNEL_SWAP
 * initiate l/r channel swap transition
 */
#define CRUS_PARAM_RX_CHANNEL_SWAP	0x00A1AF12

#define CRUS_AFE_PARAM_ID_ENABLE	0x00010203

#define SPK_ENHA_IOCTL_MAGIC		'a'

#define CRUS_SE_IOCTL_GET	_IOWR(SPK_ENHA_IOCTL_MAGIC, 219, void *)
#define CRUS_SE_IOCTL_SET	_IOWR(SPK_ENHA_IOCTL_MAGIC, 220, void *)

#define CRUS_SE_IOCTL_GET32		_IOWR(SPK_ENHA_IOCTL_MAGIC, 219, \
	compat_uptr_t)
#define CRUS_SE_IOCTL_SET32		_IOWR(SPK_ENHA_IOCTL_MAGIC, 220, \
	compat_uptr_t)

struct crus_se_ioctl_header {
	uint32_t size;
	uint32_t module_id;
	uint32_t param_id;
	uint32_t data_length;
	void *data;
};

#endif /* _UAPI_MSM_CIRRUS_SPK_EN_H */
