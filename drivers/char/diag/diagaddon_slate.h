/*
 * Copyright (C) 2010-2012 Motorola Mobility LLC
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
 */

#ifndef __DIAGADDON_SLATE_H__
#define __DIAGADDON_SLATE_H__
#ifdef CONFIG_DIAG_OVER_USB
#include <linux/usb/usbdiag.h>
#endif

#define DIAG_IOCTL_GET_STATE_AND_CONN_INFO  15
#define DIAG_IOCTL_GET_KEY_EVENT_MASK       16
#define DIAG_IOCTL_GET_PEN_EVENT_MASK       17

#define DIAG_MASK_CMD_SAVE                           0
#define DIAG_MASK_CMD_RESTORE                        1
#define DIAG_MASK_CMD_ADD_GET_RSSI                   2
#define DIAG_MASK_CMD_ADD_GET_STATE_AND_CONN_ATT     3
#define DIAG_MASK_CMD_ADD_GET_SEARCHER_DUMP          4

#define DIAG_LOG_CMD_TYPE_NONE                    0
#define DIAG_LOG_CMD_TYPE_GET_RSSI                1
#define DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT  2
#define DIAG_LOG_CMD_TYPE_RESTORE_LOG_MASKS       3
#define DIAG_LOG_CMD_TYPE_GET_1x_SEARCHER_DUMP    4

#define DIAG_LOG_TYPE_RSSI                        0
#define DIAG_LOG_TYPE_STATE                       1
#define DIAG_LOG_TYPE_CONN_ATT                    2

#ifdef CONFIG_DIAG_EXTENSION
/* This structure is for addon. It is used by slate feature */
struct diag_addon {
	struct list_head list;

	/* function list of addon
	return-value of the functions decide
	whether the callback-function of next-addon is called or not.
	refer to DIAGADDON_BASE below.
	*/
	int (*ioctl)(struct file *filp, unsigned int iocmd,
					unsigned long ioarg, int *retval);
	int (*force_returntype)(int pkt_type, int *retval);
	int (*addon_channel_diag_write)(struct diag_request *write_ptr,
							int *retval);
	int (*channel_diag_write)(int id, unsigned char *buf,
						 int len, int ctxt);
	void *private;

	/* function list of diag-driver to use addon */
	int (*diag_process_apps_pkt)(unsigned char *buf, int len);
};

#define DIAGADDON_BASE(func, retval, ...)		\
	do {						\
		struct diag_addon *addon;		 \
		int next_addon_call;			\
		list_for_each_entry(addon, &driver->addon_list, list) {	\
			if (addon->func) {		\
				next_addon_call =	\
					addon->func(__VA_ARGS__, retval);\
				if (next_addon_call == false)	\
					break;			\
			}				\
		}					\
	} while (0)

#define DIAGADDON_EXIST() (!list_empty(&driver->addon_list))
#define DIAGADDON_ioctl(retval, ...)\
		DIAGADDON_BASE(ioctl, retval, ##__VA_ARGS__)
#define DIAGADDON_force_returntype(retval, ...)\
		DIAGADDON_BASE(force_returntype, retval, ##__VA_ARGS__)
#define DIAGADDON_channel_diag_write(retval, ...)\
		DIAGADDON_BASE(addon_channel_diag_write, retval, ##__VA_ARGS__)

int diag_addon_register(struct diag_addon *addon);
int diag_addon_unregister(struct diag_addon *addon);
#else
#define DIAGADDON_EXIST() 0
#define DIAGADDON_ioctl(retval, ...) do {} while (0)
#define DIAGADDON_force_returntype(retval, ...) do {} while (0)
#define DIAGADDON_channel_diag_write(retval, i...) do {} while (0)
#endif /* endif of '#ifdef CONFIG_DIAG_EXTENSION' */

#endif
