/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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



#ifndef __ATH_PCI_H__
#define __ATH_PCI_H__

#include <linux/version.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>

#define CONFIG_COPY_ENGINE_SUPPORT /* TBDXXX: here for now */
#define ATH_DBG_DEFAULT   0
#include <osdep.h>
#include <ol_if_athvar.h>
#include <athdefs.h>
#include "osapi_linux.h"
#include "hif.h"
#include "cepci.h"

/* Maximum number of Copy Engine's supported */
#define CE_COUNT_MAX 8
#define CE_HTT_H2T_MSG_SRC_NENTRIES 2048

struct CE_state;
struct ol_softc;

/* An address (e.g. of a buffer) in Copy Engine space. */
typedef ath_dma_addr_t CE_addr_t;

#ifdef FEATURE_RUNTIME_PM
/* Driver States for Runtime Power Management */
enum hif_pm_runtime_state {
	HIF_PM_RUNTIME_STATE_NONE,
	HIF_PM_RUNTIME_STATE_ON,
	HIF_PM_RUNTIME_STATE_INPROGRESS,
	HIF_PM_RUNTIME_STATE_SUSPENDED,
};

/* Debugging stats for Runtime PM */
struct hif_pci_pm_stats {
	u32 suspended;
	u32 suspend_err;
	u32 resumed;
	u32 runtime_get;
	u32 runtime_put;
	u32 request_resume;
	u32 allow_suspend;
	u32 prevent_suspend;
	u32 prevent_suspend_timeout;
	u32 allow_suspend_timeout;
	u32 runtime_get_err;
	void *last_resume_caller;
	unsigned long suspend_jiffies;
};
#endif
struct hif_pci_softc {
    void __iomem *mem; /* PCI address. */
                       /* For efficiency, should be first in struct */

    struct device *dev;
    struct pci_dev *pdev;
    struct _NIC_DEV aps_osdev;
    struct ol_softc *ol_sc;
    int num_msi_intrs; /* number of MSI interrupts granted */
			/* 0 --> using legacy PCI line interrupts */
    struct tasklet_struct intr_tq;    /* tasklet */

    int irq;
    int irq_event;
    int cacheline_sz;
    /*
    * Guard changes to Target HW state and to software
    * structures that track hardware state.
    */
    adf_os_spinlock_t target_lock;

    unsigned int ce_count; /* Number of Copy Engines supported */
    struct CE_state *CE_id_to_state[CE_COUNT_MAX]; /* Map CE id to CE_state */
    HIF_DEVICE *hif_device;

    bool force_break;  /* Flag to indicate whether to break out the DPC context */
    unsigned int receive_count; /* count Num Of Receive Buffers handled for one interrupt DPC routine */
    u16 devid;
    struct targetdef_s *targetdef;
    struct hostdef_s *hostdef;
    atomic_t tasklet_from_intr;
    atomic_t wow_done;
#ifdef FEATURE_WLAN_D0WOW
    atomic_t in_d0wow;
#endif
    atomic_t ce_suspend;
    atomic_t pci_link_suspended;
    bool hif_init_done;
    bool recovery;
    bool hdd_startup_reinit_flag;
    int htc_endpoint;
#ifdef FEATURE_RUNTIME_PM
    atomic_t pm_state;
    uint32_t prevent_suspend_cnt;
    struct hif_pci_pm_stats pm_stats;
    struct work_struct pm_work;
    struct spinlock runtime_lock;
    struct timer_list runtime_timer;
    struct list_head prevent_suspend_list;
    unsigned long runtime_timer_expires;
#ifdef WLAN_OPEN_SOURCE
    struct dentry *pm_dentry;
#endif
#endif
};
#define TARGID(sc) ((A_target_id_t)(&(sc)->mem))
#define TARGID_TO_HIF(targid) (((struct hif_pci_softc *)((char *)(targid) - (char *)&(((struct hif_pci_softc *)0)->mem)))->hif_device)

int athdiag_procfs_init(void *scn);
void athdiag_procfs_remove(void);

bool hif_pci_targ_is_awake(struct hif_pci_softc *sc, void *__iomem *mem);

bool hif_pci_targ_is_present(A_target_id_t targetid, void *__iomem *mem);

bool hif_max_num_receives_reached(unsigned int count);

int HIF_PCIDeviceProbed(hif_handle_t hif_hdl);
irqreturn_t HIF_fw_interrupt_handler(int irq, void *arg);

/* routine to modify the initial buffer count to be allocated on an os
 * platform basis. Platform owner will need to modify this as needed
 */
adf_os_size_t initBufferCount(adf_os_size_t maxSize);

/* Function to disable ASPM */
void hif_disable_aspm(void);

void hif_pci_save_htc_htt_config_endpoint(int htc_endpoint);

int hif_pci_set_ram_config_reg(struct hif_pci_softc *sc, uint32_t config);
int hif_pci_check_fw_reg(struct hif_pci_softc *sc);
int hif_pci_check_soc_status(struct hif_pci_softc *sc);
void dump_CE_debug_register(struct hif_pci_softc *sc);

/*These functions are exposed to HDD*/
int hif_init_adf_ctx(void *ol_sc);
void hif_init_pdev_txrx_handle(void *ol_sc, void *txrx_handle);
void hif_disable_isr(void *ol_sc);
void hif_reset_soc(void *ol_sc);
void hif_deinit_adf_ctx(void *ol_sc);
void hif_get_hw_info(void *ol_sc, u32 *version, u32 *revision);
void hif_set_fw_info(void *ol_sc, u32 target_fw_version);

#ifdef IPA_UC_OFFLOAD
/*
 * Micro controller needs PCI BAR address to access CE register
 * If Micro controller data path enabled, control path will
 * try to get PCI BAR address and will send to IPA driver
 */
void hif_read_bar(struct hif_pci_softc *sc, u32 *bar_value);
#endif /* IPA_UC_OFFLOAD */

/*
 * A firmware interrupt to the Host is indicated by the
 * low bit of SCRATCH_3_ADDRESS being set.
 */
#define FW_EVENT_PENDING_REG_ADDRESS SCRATCH_3_ADDRESS

/*
 * Typically, MSI Interrupts are used with PCIe. To force use of legacy
 * "ABCD" PCI line interrupts rather than MSI, define FORCE_LEGACY_PCI_INTERRUPTS.
 * Even when NOT forced, the driver may attempt to use legacy PCI interrupts
 * MSI allocation fails
 */
#define LEGACY_INTERRUPTS(sc) ((sc)->num_msi_intrs == 0)

/*
 * There may be some pending tx frames during platform suspend.
 * Suspend operation should be delayed until those tx frames are
 * transfered from the host to target. This macro specifies how
 * long suspend thread has to sleep before checking pending tx
 * frame count.
 */
#define OL_ATH_TX_DRAIN_WAIT_DELAY     50 /* ms */

#define HIF_CE_DRAIN_WAIT_DELAY        10 /* ms */
/*
 * Wait time (in unit of OL_ATH_TX_DRAIN_WAIT_DELAY) for pending
 * tx frame completion before suspend. Refer: hif_pci_suspend()
 */
#define OL_ATH_TX_DRAIN_WAIT_CNT       10

#define HIF_CE_DRAIN_WAIT_CNT          20
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
void wlan_hif_pci_suspend(void);
#endif
#endif /* __ATH_PCI_H__ */
