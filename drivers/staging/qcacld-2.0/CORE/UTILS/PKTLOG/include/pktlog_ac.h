/*
 * Copyright (c) 2012-2013, 2016 The Linux Foundation. All rights reserved.
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

#ifndef _PKTLOG_AC_H_
#define _PKTLOG_AC_H_
#ifndef REMOVE_PKT_LOG

#include "ol_if_athvar.h"
#include <pktlog_ac_api.h>
#include <pktlog_ac_fmt.h>
#include "osdep.h"
#include <wmi_unified.h>
#include <wmi_unified_api.h>
#include <wdi_event_api.h>

#define NO_REG_FUNCS	4

/* Locking interface for pktlog */
#define PKTLOG_LOCK_INIT(_pl_info)	spin_lock_init(&(_pl_info)->log_lock)
#define	PKTLOG_LOCK_DESTROY(_pl_info)
#define PKTLOG_LOCK(_pl_info)		spin_lock(&(_pl_info)->log_lock)
#define PKTLOG_UNLOCK(_pl_info)		spin_unlock(&(_pl_info)->log_lock)

#define PKTLOG_MODE_SYSTEM	1
#define PKTLOG_MODE_ADAPTER	2

/*
 * The proc entry starts with magic number and version field which will be
 * used by post processing scripts. These fields are not needed by applications
 * that do not use these scripts. This is skipped using the offset value.
 */
#define PKTLOG_READ_OFFSET    8

/* Opaque softc */
struct ol_ath_generic_softc_t;
typedef struct ol_ath_generic_softc_t* ol_ath_generic_softc_handle;
extern void pktlog_disable_adapter_logging(struct ol_softc *scn);
extern int pktlog_alloc_buf(struct ol_softc *scn);
extern void pktlog_release_buf(struct ol_softc *scn);

ssize_t pktlog_read_proc_entry(char *buf, size_t nbytes, loff_t *ppos,
			       struct ath_pktlog_info *pl_info,
			       bool *read_complete);

struct ol_pl_arch_dep_funcs {
	void (*pktlog_init) (struct ol_softc *scn);
	int (*pktlog_enable) (struct ol_softc *scn,
			      int32_t log_state);
	int (*pktlog_setsize) (struct ol_softc *scn,
			       int32_t log_state);
	int (*pktlog_disable) (struct ol_softc *scn);
};

struct ol_pl_os_dep_funcs {
	int (*pktlog_attach) (struct ol_softc *scn);
	void (*pktlog_detach) (struct ol_softc *scn);
};

struct ath_pktlog_wmi_params {
	WMI_PKTLOG_EVENT pktlog_event;
	WMI_CMD_ID cmd_id;
};

extern struct ol_pl_arch_dep_funcs ol_pl_funcs;
extern struct ol_pl_os_dep_funcs *g_ol_pl_os_dep_funcs;

/* Pktlog handler to save the state of the pktlogs */
struct ol_pktlog_dev_t {
	struct ol_pl_arch_dep_funcs *pl_funcs;
	struct ath_pktlog_info *pl_info;
	ol_ath_generic_softc_handle scn;
	char *name;
	bool tgt_pktlog_enabled;
	osdev_t sc_osdev;
};

#define PKTLOG_SYSCTL_SIZE	14

/*
 * Linux specific pktlog state information
 */
struct ath_pktlog_info_lnx {
	struct ath_pktlog_info info;
	struct ctl_table sysctls[PKTLOG_SYSCTL_SIZE];
	struct proc_dir_entry *proc_entry;
	struct ctl_table_header *sysctl_header;
};

#define PL_INFO_LNX(_pl_info)   ((struct ath_pktlog_info_lnx *)(_pl_info))

extern struct ol_pktlog_dev_t ol_pl_dev;

/*
 * WDI related data and functions
 * Callback function to the WDI events
 */
void pktlog_callback(void *pdev, enum WDI_EVENT event, void *log_data);

#define ol_pktlog_attach(_scn)			\
	do {					\
		if (g_ol_pl_os_dep_funcs) {	\
			g_ol_pl_os_dep_funcs->pktlog_attach(_scn);	\
		}				\
	} while (0)

#define ol_pktlog_detach(_scn)			\
	do {					\
		if (g_ol_pl_os_dep_funcs) {	\
			g_ol_pl_os_dep_funcs->pktlog_detach(_scn);	\
		}				\
	} while (0)

void pktlog_init(struct ol_softc *scn);
int pktlog_enable(struct ol_softc *scn, int32_t log_state);
int pktlog_setsize(struct ol_softc *scn, int32_t log_state);
int pktlog_disable(struct ol_softc *scn);
int pktlogmod_init(void *context);
void pktlogmod_exit(void *context);
#else /* REMOVE_PKT_LOG */
#define ol_pktlog_attach(_scn)	({ (void)_scn; })
#define ol_pktlog_detach(_scn)	({ (void)_scn; })
static inline void pktlog_init(struct ol_softc *scn)
{
	return;
}
static int pktlog_enable(struct ol_softc *scn, int32_t log_state)
{
	return 0;
}
static int pktlog_setsize(struct ol_softc *scn, int32_t log_state)
{
	return 0;
}
static int pktlog_disable(struct ol_softc *scn)
{
	return 0;
}
#endif /* REMOVE_PKT_LOG */
#endif /* _PKTLOG_AC_H_ */
