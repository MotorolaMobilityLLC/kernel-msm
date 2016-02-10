/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
#ifndef __ATH_USB_H__
#define __ATH_USB_H__

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif
#include <linux/interrupt.h>
#include <linux/reboot.h>

/*
 * There may be some pending tx frames during platform suspend.
 * Suspend operation should be delayed until those tx frames are
 * transfered from the host to target. This macro specifies how
 * long suspend thread has to sleep before checking pending tx
 * frame count.
 */
#define OL_ATH_TX_DRAIN_WAIT_DELAY     50	/* ms */
/*
 * Wait time (in unit of OL_ATH_TX_DRAIN_WAIT_DELAY) for pending
 * tx frame completion before suspend. Refer: hif_pci_suspend()
 */
#define OL_ATH_TX_DRAIN_WAIT_CNT       10

#define CONFIG_COPY_ENGINE_SUPPORT	/* TBDXXX: here for now */
#define ATH_DBG_DEFAULT   0
#include <osdep.h>
#include <ol_if_athvar.h>
#include <athdefs.h>
#include "osapi_linux.h"
#include "hif.h"

struct hif_usb_softc {
	/* For efficiency, should be first in struct */
	struct device *dev;
	struct usb_dev *pdev;
	struct _NIC_DEV aps_osdev;
	struct ol_softc *ol_sc;
	/*
	 * Guard changes to Target HW state and to software
	 * structures that track hardware state.
	 */
	adf_os_spinlock_t target_lock;

	HIF_DEVICE *hif_device;

	u16 devid;
	struct targetdef_s *targetdef;
	struct hostdef_s *hostdef;
	struct usb_interface *interface;
	struct notifier_block reboot_notifier;  /* default mode before reboot */
	u8 suspend_state;
	atomic_t hdd_removed;
	atomic_t hdd_removed_processing;
	int hdd_removed_wait_cnt;
	u8 *fw_data;
	u32 fw_data_len;
};
#if defined(CONFIG_ATH_PROCFS_DIAG_SUPPORT)
int athdiag_procfs_init(void *scn);
void athdiag_procfs_remove(void);
#else
static inline int athdiag_procfs_init(void *scn) { return 0; }
static inline void athdiag_procfs_remove(void) { return; }
#endif

/*These functions are exposed to HDD*/
int hif_init_adf_ctx(void *ol_sc);
void hif_init_pdev_txrx_handle(void *ol_sc, void *txrx_handle);
void hif_disable_isr(void *ol_sc);
void hif_reset_soc(void *ol_sc);
void hif_deinit_adf_ctx(void *ol_sc);

void hif_get_hw_info(void *ol_sc, u32 *version, u32 *revision);
void hif_set_fw_info(void *ol_sc, u32 target_fw_version);
#endif /* __ATH_USB_H__ */
