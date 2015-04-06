/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#ifndef __CEPCI_H__
#define __CEPCI_H__

/*
 * Support for Copy Engine over PCI.
 * Structures shared between Host software and Target firmware.
 */


/*
 * Total number of PCIe MSI interrupts requested for all interrupt sources.
 * PCIe standard forces this to be a power of 2.
 * Some Host OS's limit MSI requests that can be granted to 8
 * so for now we abide by this limit and avoid requesting more
 * than that.
 */
#define MSI_NUM_REQUEST_LOG2  3
#define MSI_NUM_REQUEST       (1<<MSI_NUM_REQUEST_LOG2)

/*
 * Granted MSIs are assigned as follows:
 *  Firmware uses the first
 *  Remaining MSIs, if any, are used by Copy Engines
 * This mapping is known to both Target firmware and Host software.
 * It may be changed as long as Host and Target are kept in sync.
 */
#define MSI_ASSIGN_FW         0 /* 1 MSI for firmware (errors, etc.) */
#define MSI_ASSIGN_CE_INITIAL 1 /* 7 MSIs for Copy Engines */
#define MSI_ASSIGN_CE_MAX     7




/*
 * PCI-specific Target state.  Much of this may be of interest
 * to the Host so HOST_INTEREST->hi_interconnect_state points
 * here (and all members are 32-bit quantities in order to
 * facilitate Host access). In particular, Host software is
 * required to initialize pipe_cfg_addr and svc_to_pipe_map.
 */
struct pcie_state_s {
    A_UINT32 pipe_cfg_addr;      /* Pipe configuration Target address */
                                 /* NB: CE_pipe_config[CE_COUNT] */

    A_UINT32 svc_to_pipe_map;    /* Service to pipe map Target address */
                                 /* NB: service_to_pipe[PIPE_TO_CE_MAP_CN] */

    A_UINT32 MSI_requested;      /* number of MSI interrupts requested */
    A_UINT32 MSI_granted;        /* number of MSI interrupts granted */
    A_UINT32 MSI_addr;           /* Message Signalled Interrupt address */
    A_UINT32 MSI_data;           /* Base data */
    A_UINT32 MSI_fw_intr_data;   /* Data for firmware interrupt;
                                    MSI data for other interrupts are
                                    in various SoC registers */

    A_UINT32 power_mgmt_method;  /* PCIE_PWR_METHOD_* */
    A_UINT32 config_flags;       /* PCIE_CONFIG_FLAG_* */
};

/*
 * PCIE_CONFIG_FLAG definitions
 */
#define PCIE_CONFIG_FLAG_ENABLE_L1          0x0000001
#define PCIE_CONFIG_FLAG_CLK_SWITCH_WAIT    0x0000002
#define PCIE_CONFIG_FLAG_AXI_CLK_GATE       0x0000004

#define PIPE_TO_CE_MAP_CNT 32 /* simple implementation constant */

/*
 * Configuration information for a Copy Engine pipe.
 * Passed from Host to Target during startup (one per CE).
 */
struct CE_pipe_config {
    A_UINT32 pipenum;
    A_UINT32 pipedir;
    A_UINT32 nentries;
    A_UINT32 nbytes_max;
    A_UINT32 flags;
    A_UINT32 reserved;
};

#endif /* __CEPCI_H__ */
