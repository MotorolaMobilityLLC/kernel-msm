/* include/asm-arm/arch-msm/usbdiag.h
 *
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#ifndef _DRIVERS_USB_DIAG_H_
#define _DRIVERS_USB_DIAG_H_

#include <linux/diagchar.h>

#define DIAG_LEGACY		"diag"
#define DIAG_MDM		"diag_mdm"

struct legacy_diag_ch *usb_diag_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *));
void usb_diag_close(struct legacy_diag_ch *ch);
int usb_diag_alloc_req(struct legacy_diag_ch *ch, int n_write, int n_read);
void usb_diag_free_req(struct legacy_diag_ch *ch);
int usb_diag_read(struct legacy_diag_ch *ch, struct diag_request *d_req);
int usb_diag_write(struct legacy_diag_ch *ch, struct diag_request *d_req);
int usb_diag_init_preference(void);

int diag_read_from_cb(unsigned char * , int);

#endif /* _DRIVERS_USB_DIAG_H_ */
