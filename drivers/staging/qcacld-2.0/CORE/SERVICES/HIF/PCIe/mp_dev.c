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

#include "if_pci.h"

/*chaninfo*/
#define  CHANINFOMEM_S2_READ_MASK               0x00000008
#define  CHANINFO_CTRL_CAPTURE_CHAN_INFO_MASK   0x00000001
#define  CHANINFO_CTRL_CHANINFOMEM_BW_MASK      0x00000030
#define  MULTICHAIN_ENABLE_RX_CHAIN_MASK_MASK   0x00000007

/*agc*/
#define GAINS_MIN_OFFSETS_CF_AGC_HIST_ENABLE_MASK              0x00040000
#define GAINS_MIN_OFFSETS_CF_AGC_HIST_GC_MASK                  0x00080000
#define GAINS_MIN_OFFSETS_CF_AGC_HIST_VOTING_MASK              0x00100000
#define GAINS_MIN_OFFSETS_CF_AGC_HIST_PHY_ERR_MASK             0x00200000
#define  AGC_HISTORY_DUMP_MASK    GAINS_MIN_OFFSETS_CF_AGC_HIST_ENABLE_MASK| \
                                  GAINS_MIN_OFFSETS_CF_AGC_HIST_GC_MASK| \
                                  GAINS_MIN_OFFSETS_CF_AGC_HIST_VOTING_MASK| \
                                  GAINS_MIN_OFFSETS_CF_AGC_HIST_PHY_ERR_MASK

#define BB_chaninfo_ctrl         0x1a370
#define BB_multichain_enable     0x1a2a0
#define BB_chn_tables_intf_addr  0x19894
#define BB_chn1_tables_intf_addr 0x1a894
#define BB_chn_tables_intf_data  0x19898
#define BB_chn1_tables_intf_data 0x1a898
#define BB_gains_min_offsets     0x19e08
#define BB_chaninfo_tab_b0       0x03200
#define BB_chaninfo_tab_b1       0x03300
#define BB_watchdog_status       0x1a7c0
#define BB_watchdog_ctrl_1       0x1a7c4
#define BB_watchdog_ctrl_2       0x1a7c8
#define BB_watchdog_status_B     0x1a7e0

struct priv_ctrl_ctx {
    u_int32_t chaninfo_ctrl_orig;
    u_int32_t gain_min_offsets_orig;
    u_int32_t anyreg_start;
    u_int32_t anyreg_len;
};

static struct priv_ctrl_ctx g_Priv_Dump_Ctx;

static INLINE void set_target_reg_bits(struct hif_pci_softc* hif, u_int32_t reg, u_int32_t bitmask, u_int32_t val)
{
    u_int32_t value = A_PCI_READ32(hif->mem + (reg));
    u_int32_t shift = 0;
    value &= ~(bitmask);
    while (!((bitmask >> shift) & 0x01)) {
        shift++;
    }
    value |= (((val)<<shift)&(bitmask));
    A_PCI_WRITE32(hif->mem + (reg), value);
}

static INLINE u_int32_t get_target_reg_bits(struct hif_pci_softc* hif, u_int32_t reg, u_int32_t bitmask)
{
    u_int32_t value = A_PCI_READ32(hif->mem + (reg));
    u_int32_t shift = 0;
    while (!((bitmask >> shift) & 0x01)) {
        shift++;
    }
    return (value>>shift)&bitmask;
}

void priv_start_cap_chaninfo(struct hif_pci_softc* hif_sc)
{
    set_target_reg_bits(hif_sc, BB_chaninfo_ctrl, CHANINFO_CTRL_CAPTURE_CHAN_INFO_MASK, 1);
}

void priv_start_agc(struct hif_pci_softc* hif_sc)
{
    g_Priv_Dump_Ctx.gain_min_offsets_orig = A_PCI_READ32(hif_sc->mem + BB_gains_min_offsets);
    set_target_reg_bits(hif_sc, BB_gains_min_offsets, AGC_HISTORY_DUMP_MASK, 0x0f);
}

void priv_stop_agc(struct hif_pci_softc* hif_sc)
{
    set_target_reg_bits(hif_sc, BB_gains_min_offsets, AGC_HISTORY_DUMP_MASK, 0);
}

void priv_dump_chaninfo(struct hif_pci_softc* hif_sc)
{
    u_int32_t bw, val;
    u_int32_t len, i, tmp;
    u_int32_t chain_mask;
    u_int32_t chain0, chain1;

    chain_mask = get_target_reg_bits(hif_sc, BB_multichain_enable, MULTICHAIN_ENABLE_RX_CHAIN_MASK_MASK);
    chain0 = chain_mask & 1;
    chain1 = chain_mask & 2;

    printk("\nChannel info dump:\n");
    bw = get_target_reg_bits(hif_sc, BB_chaninfo_ctrl, CHANINFO_CTRL_CHANINFOMEM_BW_MASK);

    if (bw == 0) {
        len = 53;
    } else if (bw == 1) {
        len = 57;
    } else if (bw == 2) {
        len = 59*2-1;
    } else {
        len = 60*2+61*2;
    }

    /*
     * each tone is 16 bit valid, write to 32bit buffer each.
     * bw==0(legacy20): 53 tones.
     * bw==1(ht/vht20): 57 tones.
     * bw==2(ht/vht40): 59+58 tones.
     * bw==3(vht80): 60*2+61*2 tones.
     */

    if (chain0) {
        A_PCI_WRITE32(hif_sc->mem+BB_chn_tables_intf_addr, 0x80003200);
    }
    if (chain1) {
        A_PCI_WRITE32(hif_sc->mem+BB_chn1_tables_intf_addr, 0x80003200);
    }

    set_target_reg_bits(hif_sc, BB_chaninfo_ctrl, CHANINFOMEM_S2_READ_MASK, 0);

    if (chain0) {
        if (bw < 2) {
            len = (bw == 0) ? 53 : 57;
            for (i=0; i<len; i++) {
                val = A_PCI_READ32(hif_sc->mem+BB_chn_tables_intf_data) & 0x0000ffff;
                printk("0x%x\t",val);
                if (i%4 == 0)
                    printk("\n");
            }
        } else {
            len = (bw == 2) ? 59 : 60;
            for (i=0; i<len; i++) {
                tmp = A_PCI_READ32(hif_sc->mem+BB_chn_tables_intf_data);
                printk("0x%x\t", ((tmp>>16) & 0x0000ffff));
                printk("0x%x\t", (tmp & 0x0000ffff));
                if (i%2 == 0)
                    printk("\n");
            }
            if (bw > 2) {
                //bw == 3 for vht80
                A_PCI_WRITE32(hif_sc->mem+BB_chn_tables_intf_addr, 0x80003300);
                len = 61;
                for (i=0; i<len; i++) {
                    tmp = A_PCI_READ32(hif_sc->mem+BB_chn_tables_intf_data);
                    printk("0x%x\t", ((tmp>>16) & 0x0000ffff));
                    printk("0x%x\t", (tmp & 0x0000ffff));
                    if (i%2 == 0)
                        printk("\n");
                }
            }
        }
    }
    if (chain1) {
        if (bw < 2) {
            len = (bw == 0) ? 53 : 57;
            for (i=0; i<len; i++) {
                val = A_PCI_READ32(hif_sc->mem+BB_chn1_tables_intf_data) & 0x0000ffff;
                printk("0x%x\t",val);
                if (i%4 == 0)
                    printk("\n");
            }
        } else {
            len = (bw == 2) ? 59 : 60;
            for (i=0; i<len; i++) {
                tmp = A_PCI_READ32(hif_sc->mem+BB_chn1_tables_intf_data);
                printk("0x%x\n", (tmp>>16) & 0x0000ffff);
                printk("0x%x\n", tmp & 0x0000ffff);
                if (i%2 == 0)
                    printk("\n");
            }
            if (bw > 2) {
                //bw == 3 for vht80
                A_PCI_WRITE32(hif_sc->mem+BB_chn1_tables_intf_addr, 0x80003300);
                len = 61;
                for (i=0; i<len; i++) {
                    tmp = A_PCI_READ32(hif_sc->mem+BB_chn1_tables_intf_data);
                    printk("0x%x\t", ((tmp>>16) & 0x0000ffff));
                    printk("0x%x\t", (tmp & 0x0000ffff));
                    if (i%2 == 0)
                        printk("\n");
                }
            }
        }
    }
    printk("\nChannel info dump complete\n");
}

void priv_dump_agc(struct hif_pci_softc* hif_sc)
{
    int i, len = 30;  //check this value for Rome and Peregrine
    u_int32_t chain0, chain1, chain_mask, val;

    chain_mask = get_target_reg_bits(hif_sc, BB_multichain_enable, MULTICHAIN_ENABLE_RX_CHAIN_MASK_MASK);
    chain0 = chain_mask & 1;
    chain1 = chain_mask & 2;

    len = len<<1;  //each agc item is 64bit, total*2
    priv_stop_agc(hif_sc);

    set_target_reg_bits(hif_sc, BB_chaninfo_ctrl, CHANINFOMEM_S2_READ_MASK, 0);

    printk("AGC history buffer dump:\n");
    if (chain0) {
        for (i = 0; i < len; i++) {
            val = A_PCI_READ32(hif_sc->mem + BB_chaninfo_tab_b0 + i*4);
            printk("0x%x\t", val);
            if (i%4 == 0)
                printk("\n");
        }
    }
    if (chain1) {
        for (i = 0; i < len; i++) {
            val = A_PCI_READ32(hif_sc->mem + BB_chaninfo_tab_b1 + i*4);
            printk("0x%x\t", val);
            if (i%4 == 0)
                printk("\n");
        }
    }
    printk("\nAGC history buffer dump complete\n");
    //restore original value
    A_PCI_WRITE32(hif_sc->mem + BB_gains_min_offsets, g_Priv_Dump_Ctx.gain_min_offsets_orig);
    return;
}

void priv_dump_bbwatchdog(struct hif_pci_softc* hif_sc)
{
    u_int32_t val;

    printk("BB watchdog dump\n");
    val = A_PCI_READ32(hif_sc->mem + BB_watchdog_status);
    printk("0x%x\t",val);
    val = A_PCI_READ32(hif_sc->mem + BB_watchdog_ctrl_1);
    printk("0x%x\t",val);
    val = A_PCI_READ32(hif_sc->mem + BB_watchdog_ctrl_2);
    printk("0x%x\t",val);
    val = A_PCI_READ32(hif_sc->mem + BB_watchdog_status_B);
    printk("0x%x\n",val);
}
