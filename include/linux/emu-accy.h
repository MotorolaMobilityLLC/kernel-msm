/*
 * Copyright (C) 2010-2012 Motorola, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __EMU_ACCY_H__
#define __EMU_ACCY_H__

#include <linux/ioctl.h>

enum emu_accy {
	EMU_ACCY_USB,		/* USB Host connected */
	EMU_ACCY_FACTORY,
	EMU_ACCY_CHARGER,
	EMU_ACCY_USB_DEVICE,  /* USB Device connected */
	EMU_ACCY_WHISPER_PPD,
	EMU_ACCY_WHISPER_SPD,
	EMU_ACCY_WHISPER_SMART_DOCK,
	EMU_ACCY_NONE,

	/* Used while debouncing the accessory. */
	EMU_ACCY_UNKNOWN,
};

enum {
	NO_DOCK,
	DESK_DOCK,
	CAR_DOCK,
	LE_DOCK,
	HE_DOCK,
	MOBILE_DOCK,
	CHARGER_DOCK,
};

enum {
	NO_DEVICE,
	EMU_OUT,
};

enum {
	AUTH_NOT_STARTED,
	AUTH_IN_PROGRESS,
	AUTH_FAILED,
	AUTH_PASSED,
};

enum {
	UNDOCKED,
	DOCKED,
};

enum {
	AUTH_REQUIRED,
	AUTH_NOT_REQUIRED,
};

enum {
	MUXMODE_USB = 1,
	MUXMODE_UART,
	MUXMODE_AUDIO,
	MUXMODE_UNDEFINED,
};

enum {
	CHARGE_NONE,
	CHARGE_VLCL_REQ = 3,
};

enum {
	CMD_CHARGER_CAPABILITY = 1,
	CMD_CHARGER_VLCL_RESP = 3,
};

#ifdef __KERNEL__
struct emu_det_vlcl_request {
	unsigned int mV;
	unsigned int mA;
	int status;
	void (*callback)(void *);
	void *callback_param;
};

void set_mux_ctrl_mode_for_audio(int mode);
void semu_audio_register_notify(struct notifier_block *nb);
void emu_det_vlcl_register_notify(struct notifier_block *nb);
int  emu_det_vlcl_request(struct emu_det_vlcl_request *req);
#endif

/*
 *  CPCAP compatibility section
 */

#undef CPCAP_DEV_NAME
#define CPCAP_DEV_NAME	"emu_det"

#define CPCAP_WHISPER_MODE_PU		0x00000001
#define CPCAP_WHISPER_ENABLE_UART	0x00000002
#define CPCAP_WHISPER_ACCY_MASK		0xF8000000
#define CPCAP_WHISPER_ACCY_SHFT		27
#define CPCAP_WHISPER_ID_SIZE		16
#define CPCAP_WHISPER_PROP_SIZE		7
#define CHARGER_CAPABILITY_SIZE         163
#define CHARGER_MSG_SIZE                9

struct cpcap_whisper_request {
	unsigned int cmd;
	char dock_id[CPCAP_WHISPER_ID_SIZE];
	char dock_prop[CPCAP_WHISPER_PROP_SIZE];
};

struct cpcap_charger_request {
	int cmd;
	char charger_capability[CHARGER_CAPABILITY_SIZE];
	char charger_msg[CHARGER_MSG_SIZE];
};

#define CPCAP_IOCTL_NUM_ACCY_WHISPER	23
#define CPCAP_IOCTL_NUM_ACCY_CHARGER	24

#define CPCAP_IOCTL_ACCY_WHISPER \
	_IOW(0, CPCAP_IOCTL_NUM_ACCY_WHISPER, struct cpcap_whisper_request*)

#define CPCAP_IOCTL_ACCY_CHARGER \
	_IOW(0, CPCAP_IOCTL_NUM_ACCY_CHARGER, struct cpcap_charger_request*)

#endif  /* __EMU_ACCY_H__ */
