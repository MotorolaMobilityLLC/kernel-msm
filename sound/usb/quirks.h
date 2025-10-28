/* SPDX-License-Identifier: GPL-2.0 */
/*
 * xhci quirk driver
 *
 * Copyright (c) 2025 Motorola Inc.
 */

#ifndef __XHCI_QUIRKS_H
#define __XHCI_QUIRKS_H

#include <linux/usb.h>
#include "usbaudio.h"
#include <linux/hashtable.h>
#include <linux/jiffies.h>

void xhci_init_snd_quirk(struct snd_usb_audio *chip);
void xhci_deinit_snd_quirk(struct snd_usb_audio *chip);
void xhci_apply_quirk(struct usb_device *udev);

#endif
