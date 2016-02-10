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

#ifndef __IF_ATH_SDIO_H__
#define __IF_ATH_SDIO_H__

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif
#include <linux/interrupt.h>
#include <osdep.h>
#include <ol_if_athvar.h>
#include <athdefs.h>
#include "a_osapi.h"
#include "hif_internal.h"
#include "hif_trace.h"

#define AR6320_HEADERS_DEF

#define ATH_DBG_DEFAULT   0

#ifdef TARGET_DUMP_FOR_9X15_PLATFORM
#define RAMDUMP_ADDR     0x46E00000
#define RAMDUMP_SIZE     0x100000
#elif defined(CONFIG_ARCH_MDM9607)
#define RAMDUMP_ADDR     0x87A00000
#define RAMDUMP_SIZE     0x200000
#else
#define RAMDUMP_ADDR     0x8F000000
#define RAMDUMP_SIZE     0x700000
#endif

struct ath_hif_sdio_softc {
    struct device *dev;
    struct _NIC_DEV aps_osdev;
    struct ol_softc *ol_sc;
    struct tasklet_struct intr_tq;    /* tasklet */

    int irq;
    /*
    * Guard changes to Target HW state and to software
    * structures that track hardware state.
    */
    spinlock_t target_lock;
    void *hif_handle;
    struct targetdef_s *targetdef;
    struct hostdef_s *hostdef;
};

#if defined(CONFIG_ATH_PROCFS_DIAG_SUPPORT)
int athdiag_procfs_init(void *scn);
void athdiag_procfs_remove(void);
#else
static inline int athdiag_procfs_init(void *scn) { return 0; }
static inline void athdiag_procfs_remove(void) { return; }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
#define DMA_MAPPING_ERROR(dev, addr) dma_mapping_error((addr))
#else
#define DMA_MAPPING_ERROR(dev, addr) dma_mapping_error((dev), (addr))
#endif

int ath_sdio_probe(void *context, void *hif_handle);
void ath_sdio_remove(void *context, void *hif_handle);
int ath_sdio_suspend(void *context);
int ath_sdio_resume(void *context);

/*These functions are exposed to HDD*/
int hif_init_adf_ctx(void *ol_sc);
void hif_deinit_adf_ctx(void *ol_sc);
void hif_disable_isr(void *ol_sc);
void hif_init_pdev_txrx_handle(void *ol_sc, void *txrx_handle);
void hif_reset_soc(void *ol_sc);


void hif_register_tbl_attach(u32 hif_type);
void target_register_tbl_attach(u32 target_type);

void hif_get_hw_info(void *ol_sc, u32 *version, u32 *revision);
void hif_set_fw_info(void *ol_sc, u32 target_fw_version);

#endif /* __IF_ATH_SDIO_H__*/
