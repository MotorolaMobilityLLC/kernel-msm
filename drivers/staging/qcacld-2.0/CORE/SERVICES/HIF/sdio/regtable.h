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
#ifndef _REGTABLE_H_
#define _REGTABLE_H_
//#include "if_usb.h"

#define MISSING  0
extern struct ath_hif_sdio_softc *sc;

typedef struct targetdef_s {
    u_int32_t d_RTC_SOC_BASE_ADDRESS;
    u_int32_t d_RTC_WMAC_BASE_ADDRESS;
    u_int32_t d_SYSTEM_SLEEP_OFFSET;
    u_int32_t d_WLAN_SYSTEM_SLEEP_OFFSET;
    u_int32_t d_WLAN_SYSTEM_SLEEP_DISABLE_LSB;
    u_int32_t d_WLAN_SYSTEM_SLEEP_DISABLE_MASK;
    u_int32_t d_CLOCK_CONTROL_OFFSET;
    u_int32_t d_CLOCK_CONTROL_SI0_CLK_MASK;
    u_int32_t d_RESET_CONTROL_OFFSET;
    u_int32_t d_RESET_CONTROL_MBOX_RST_MASK;
    u_int32_t d_RESET_CONTROL_SI0_RST_MASK;
    u_int32_t d_WLAN_RESET_CONTROL_OFFSET;
    u_int32_t d_WLAN_RESET_CONTROL_COLD_RST_MASK;
    u_int32_t d_WLAN_RESET_CONTROL_WARM_RST_MASK;
    u_int32_t d_GPIO_BASE_ADDRESS;
    u_int32_t d_GPIO_PIN0_OFFSET;
    u_int32_t d_GPIO_PIN1_OFFSET;
    u_int32_t d_GPIO_PIN0_CONFIG_MASK;
    u_int32_t d_GPIO_PIN1_CONFIG_MASK;
    u_int32_t d_SI_CONFIG_BIDIR_OD_DATA_LSB;
    u_int32_t d_SI_CONFIG_BIDIR_OD_DATA_MASK;
    u_int32_t d_SI_CONFIG_I2C_LSB;
    u_int32_t d_SI_CONFIG_I2C_MASK;
    u_int32_t d_SI_CONFIG_POS_SAMPLE_LSB;
    u_int32_t d_SI_CONFIG_POS_SAMPLE_MASK;
    u_int32_t d_SI_CONFIG_INACTIVE_CLK_LSB;
    u_int32_t d_SI_CONFIG_INACTIVE_CLK_MASK;
    u_int32_t d_SI_CONFIG_INACTIVE_DATA_LSB;
    u_int32_t d_SI_CONFIG_INACTIVE_DATA_MASK;
    u_int32_t d_SI_CONFIG_DIVIDER_LSB;
    u_int32_t d_SI_CONFIG_DIVIDER_MASK;
    u_int32_t d_SI_BASE_ADDRESS;
    u_int32_t d_SI_CONFIG_OFFSET;
    u_int32_t d_SI_TX_DATA0_OFFSET;
    u_int32_t d_SI_TX_DATA1_OFFSET;
    u_int32_t d_SI_RX_DATA0_OFFSET;
    u_int32_t d_SI_RX_DATA1_OFFSET;
    u_int32_t d_SI_CS_OFFSET;
    u_int32_t d_SI_CS_DONE_ERR_MASK;
    u_int32_t d_SI_CS_DONE_INT_MASK;
    u_int32_t d_SI_CS_START_LSB;
    u_int32_t d_SI_CS_START_MASK;
    u_int32_t d_SI_CS_RX_CNT_LSB;
    u_int32_t d_SI_CS_RX_CNT_MASK;
    u_int32_t d_SI_CS_TX_CNT_LSB;
    u_int32_t d_SI_CS_TX_CNT_MASK;
    u_int32_t d_BOARD_DATA_SZ;
    u_int32_t d_BOARD_EXT_DATA_SZ;
    u_int32_t d_MBOX_BASE_ADDRESS;
    u_int32_t d_LOCAL_SCRATCH_OFFSET;
    u_int32_t d_CPU_CLOCK_OFFSET;
    u_int32_t d_LPO_CAL_OFFSET;
    u_int32_t d_GPIO_PIN10_OFFSET;
    u_int32_t d_GPIO_PIN11_OFFSET;
    u_int32_t d_GPIO_PIN12_OFFSET;
    u_int32_t d_GPIO_PIN13_OFFSET;
    u_int32_t d_CLOCK_GPIO_OFFSET;
    u_int32_t d_CPU_CLOCK_STANDARD_LSB;
    u_int32_t d_CPU_CLOCK_STANDARD_MASK;
    u_int32_t d_LPO_CAL_ENABLE_LSB;
    u_int32_t d_LPO_CAL_ENABLE_MASK;
    u_int32_t d_CLOCK_GPIO_BT_CLK_OUT_EN_LSB;
    u_int32_t d_CLOCK_GPIO_BT_CLK_OUT_EN_MASK;
    u_int32_t d_ANALOG_INTF_BASE_ADDRESS;
    u_int32_t d_WLAN_MAC_BASE_ADDRESS;
    u_int32_t d_CE0_BASE_ADDRESS;
    u_int32_t d_CE1_BASE_ADDRESS;
    u_int32_t d_FW_INDICATOR_ADDRESS;
    u_int32_t d_DRAM_BASE_ADDRESS;
    u_int32_t d_SOC_CORE_BASE_ADDRESS;
    u_int32_t d_CORE_CTRL_ADDRESS;
    u_int32_t d_CE_COUNT;
    u_int32_t d_MSI_NUM_REQUEST;
    u_int32_t d_MSI_ASSIGN_FW;
    u_int32_t d_MSI_ASSIGN_CE_INITIAL;
    u_int32_t d_PCIE_INTR_ENABLE_ADDRESS;
    u_int32_t d_PCIE_INTR_CLR_ADDRESS;
    u_int32_t d_PCIE_INTR_FIRMWARE_MASK;
    u_int32_t d_PCIE_INTR_CE_MASK_ALL;
    u_int32_t d_CORE_CTRL_CPU_INTR_MASK;
    u_int32_t d_SR_WR_INDEX_ADDRESS;
    u_int32_t d_DST_WATERMARK_ADDRESS;

    /* htt_rx.c */
    u_int32_t d_RX_MSDU_END_4_FIRST_MSDU_MASK;
    u_int32_t d_RX_MSDU_END_4_FIRST_MSDU_LSB;
    u_int32_t d_RX_MPDU_START_0_SEQ_NUM_MASK;
    u_int32_t d_RX_MPDU_START_0_SEQ_NUM_LSB;
    u_int32_t d_RX_MPDU_START_2_PN_47_32_LSB;
    u_int32_t d_RX_MPDU_START_2_PN_47_32_MASK;
    u_int32_t d_RX_MSDU_END_1_EXT_WAPI_PN_63_48_MASK;
    u_int32_t d_RX_MSDU_END_1_EXT_WAPI_PN_63_48_LSB;
    u_int32_t d_RX_MSDU_END_1_KEY_ID_OCT_MASK;
    u_int32_t d_RX_MSDU_END_1_KEY_ID_OCT_LSB;
    u_int32_t d_RX_MSDU_END_4_LAST_MSDU_MASK;
    u_int32_t d_RX_MSDU_END_4_LAST_MSDU_LSB;
    u_int32_t d_RX_ATTENTION_0_MCAST_BCAST_MASK;
    u_int32_t d_RX_ATTENTION_0_MCAST_BCAST_LSB;
    u_int32_t d_RX_ATTENTION_0_FRAGMENT_MASK;
    u_int32_t d_RX_ATTENTION_0_FRAGMENT_LSB;
    u_int32_t d_RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK;
    u_int32_t d_RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK;
    u_int32_t d_RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB;
    u_int32_t d_RX_MSDU_START_0_MSDU_LENGTH_MASK;
    u_int32_t d_RX_MSDU_START_0_MSDU_LENGTH_LSB;
    u_int32_t d_RX_MSDU_START_2_DECAP_FORMAT_OFFSET;
    u_int32_t d_RX_MSDU_START_2_DECAP_FORMAT_MASK;
    u_int32_t d_RX_MSDU_START_2_DECAP_FORMAT_LSB;
    u_int32_t d_RX_MPDU_START_0_ENCRYPTED_MASK;
    u_int32_t d_RX_MPDU_START_0_ENCRYPTED_LSB;
    u_int32_t d_RX_ATTENTION_0_MORE_DATA_MASK;
    u_int32_t d_RX_ATTENTION_0_MSDU_DONE_MASK;
    u_int32_t d_RX_ATTENTION_0_TCP_UDP_CHKSUM_FAIL_MASK;
    /* end */
    /* copy_engine.c */
    u_int32_t d_DST_WR_INDEX_ADDRESS;
    u_int32_t d_SRC_WATERMARK_ADDRESS;
    u_int32_t d_SRC_WATERMARK_LOW_MASK;
    u_int32_t d_SRC_WATERMARK_HIGH_MASK;
    u_int32_t d_DST_WATERMARK_LOW_MASK;
    u_int32_t d_DST_WATERMARK_HIGH_MASK;
    u_int32_t d_CURRENT_SRRI_ADDRESS;
    u_int32_t d_CURRENT_DRRI_ADDRESS;
    u_int32_t d_HOST_IS_SRC_RING_HIGH_WATERMARK_MASK;
    u_int32_t d_HOST_IS_SRC_RING_LOW_WATERMARK_MASK;
    u_int32_t d_HOST_IS_DST_RING_HIGH_WATERMARK_MASK;
    u_int32_t d_HOST_IS_DST_RING_LOW_WATERMARK_MASK;
    u_int32_t d_HOST_IS_ADDRESS;
    u_int32_t d_HOST_IS_COPY_COMPLETE_MASK;
    u_int32_t d_CE_WRAPPER_BASE_ADDRESS;
    u_int32_t d_CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS;
    u_int32_t d_HOST_IE_ADDRESS;
    u_int32_t d_HOST_IE_COPY_COMPLETE_MASK;
    u_int32_t d_SR_BA_ADDRESS;
    u_int32_t d_SR_SIZE_ADDRESS;
    u_int32_t d_CE_CTRL1_ADDRESS;
    u_int32_t d_CE_CTRL1_DMAX_LENGTH_MASK;
    u_int32_t d_DR_BA_ADDRESS;
    u_int32_t d_DR_SIZE_ADDRESS;
    u_int32_t d_MISC_IE_ADDRESS;
    u_int32_t d_MISC_IS_AXI_ERR_MASK;
    u_int32_t d_MISC_IS_DST_ADDR_ERR_MASK;
    u_int32_t d_MISC_IS_SRC_LEN_ERR_MASK;
    u_int32_t d_MISC_IS_DST_MAX_LEN_VIO_MASK;
    u_int32_t d_MISC_IS_DST_RING_OVERFLOW_MASK;
    u_int32_t d_MISC_IS_SRC_RING_OVERFLOW_MASK;
    u_int32_t d_SRC_WATERMARK_LOW_LSB;
    u_int32_t d_SRC_WATERMARK_HIGH_LSB;
    u_int32_t d_DST_WATERMARK_LOW_LSB;
    u_int32_t d_DST_WATERMARK_HIGH_LSB;
    u_int32_t d_CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK;
    u_int32_t d_CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB;
    u_int32_t d_CE_CTRL1_DMAX_LENGTH_LSB;
    u_int32_t d_CE_CTRL1_SRC_RING_BYTE_SWAP_EN_MASK;
    u_int32_t d_CE_CTRL1_DST_RING_BYTE_SWAP_EN_MASK;
    u_int32_t d_CE_CTRL1_SRC_RING_BYTE_SWAP_EN_LSB;
    u_int32_t d_CE_CTRL1_DST_RING_BYTE_SWAP_EN_LSB;
    /* end */
    u_int32_t d_PCIE_INTR_CAUSE_ADDRESS;
    u_int32_t d_SOC_RESET_CONTROL_ADDRESS;
    u_int32_t d_SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_MASK;
    u_int32_t d_SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_LSB;
    u_int32_t d_SOC_RESET_CONTROL_CE_RST_MASK;
    u_int32_t d_SOC_RESET_CONTROL_CPU_WARM_RST_MASK;
    u_int32_t d_CPU_INTR_ADDRESS;
    u_int32_t d_SOC_LF_TIMER_CONTROL0_ADDRESS;
    u_int32_t d_SOC_LF_TIMER_CONTROL0_ENABLE_MASK;
    /* chip id start */
    u_int32_t d_SOC_CHIP_ID_ADDRESS;
    u_int32_t d_SOC_CHIP_ID_VERSION_MASK;
    u_int32_t d_SOC_CHIP_ID_VERSION_LSB;
    u_int32_t d_SOC_CHIP_ID_REVISION_MASK;
    u_int32_t d_SOC_CHIP_ID_REVISION_LSB;
    /* chip id end */
} TARGET_REGISTER_TABLE;

#define RTC_SOC_BASE_ADDRESS                     (sc->targetdef->d_RTC_SOC_BASE_ADDRESS)
#define RTC_WMAC_BASE_ADDRESS                    (sc->targetdef->d_RTC_WMAC_BASE_ADDRESS)
#define SYSTEM_SLEEP_OFFSET                      (sc->targetdef->d_SYSTEM_SLEEP_OFFSET)
#define WLAN_SYSTEM_SLEEP_OFFSET                 (sc->targetdef->d_WLAN_SYSTEM_SLEEP_OFFSET)
#define WLAN_SYSTEM_SLEEP_DISABLE_LSB            (sc->targetdef->d_WLAN_SYSTEM_SLEEP_DISABLE_LSB)
#define WLAN_SYSTEM_SLEEP_DISABLE_MASK           (sc->targetdef->d_WLAN_SYSTEM_SLEEP_DISABLE_MASK)
#define CLOCK_CONTROL_OFFSET                     (sc->targetdef->d_CLOCK_CONTROL_OFFSET)
#define CLOCK_CONTROL_SI0_CLK_MASK               (sc->targetdef->d_CLOCK_CONTROL_SI0_CLK_MASK)
#define RESET_CONTROL_OFFSET                     (sc->targetdef->d_RESET_CONTROL_OFFSET)
#define RESET_CONTROL_MBOX_RST_MASK              (sc->targetdef->d_RESET_CONTROL_MBOX_RST_MASK)
#define RESET_CONTROL_SI0_RST_MASK               (sc->targetdef->d_RESET_CONTROL_SI0_RST_MASK)
#define WLAN_RESET_CONTROL_OFFSET                (sc->targetdef->d_WLAN_RESET_CONTROL_OFFSET)
#define WLAN_RESET_CONTROL_COLD_RST_MASK         (sc->targetdef->d_WLAN_RESET_CONTROL_COLD_RST_MASK)
#define WLAN_RESET_CONTROL_WARM_RST_MASK         (sc->targetdef->d_WLAN_RESET_CONTROL_WARM_RST_MASK)
#define GPIO_BASE_ADDRESS                        (sc->targetdef->d_GPIO_BASE_ADDRESS)
#define GPIO_PIN0_OFFSET                         (sc->targetdef->d_GPIO_PIN0_OFFSET)
#define GPIO_PIN1_OFFSET                         (sc->targetdef->d_GPIO_PIN1_OFFSET)
#define GPIO_PIN0_CONFIG_MASK                    (sc->targetdef->d_GPIO_PIN0_CONFIG_MASK)
#define GPIO_PIN1_CONFIG_MASK                    (sc->targetdef->d_GPIO_PIN1_CONFIG_MASK)
#define SI_CONFIG_BIDIR_OD_DATA_LSB              (sc->targetdef->d_SI_CONFIG_BIDIR_OD_DATA_LSB)
#define SI_CONFIG_BIDIR_OD_DATA_MASK             (sc->targetdef->d_SI_CONFIG_BIDIR_OD_DATA_MASK)
#define SI_CONFIG_I2C_LSB                        (sc->targetdef->d_SI_CONFIG_I2C_LSB)
#define SI_CONFIG_I2C_MASK                       (sc->targetdef->d_SI_CONFIG_I2C_MASK)
#define SI_CONFIG_POS_SAMPLE_LSB                 (sc->targetdef->d_SI_CONFIG_POS_SAMPLE_LSB)
#define SI_CONFIG_POS_SAMPLE_MASK                (sc->targetdef->d_SI_CONFIG_POS_SAMPLE_MASK)
#define SI_CONFIG_INACTIVE_CLK_LSB               (sc->targetdef->d_SI_CONFIG_INACTIVE_CLK_LSB)
#define SI_CONFIG_INACTIVE_CLK_MASK              (sc->targetdef->d_SI_CONFIG_INACTIVE_CLK_MASK)
#define SI_CONFIG_INACTIVE_DATA_LSB              (sc->targetdef->d_SI_CONFIG_INACTIVE_DATA_LSB)
#define SI_CONFIG_INACTIVE_DATA_MASK             (sc->targetdef->d_SI_CONFIG_INACTIVE_DATA_MASK)
#define SI_CONFIG_DIVIDER_LSB                    (sc->targetdef->d_SI_CONFIG_DIVIDER_LSB)
#define SI_CONFIG_DIVIDER_MASK                   (sc->targetdef->d_SI_CONFIG_DIVIDER_MASK)
#define SI_BASE_ADDRESS                          (sc->targetdef->d_SI_BASE_ADDRESS)
#define SI_CONFIG_OFFSET                         (sc->targetdef->d_SI_CONFIG_OFFSET)
#define SI_TX_DATA0_OFFSET                       (sc->targetdef->d_SI_TX_DATA0_OFFSET)
#define SI_TX_DATA1_OFFSET                       (sc->targetdef->d_SI_TX_DATA1_OFFSET)
#define SI_RX_DATA0_OFFSET                       (sc->targetdef->d_SI_RX_DATA0_OFFSET)
#define SI_RX_DATA1_OFFSET                       (sc->targetdef->d_SI_RX_DATA1_OFFSET)
#define SI_CS_OFFSET                             (sc->targetdef->d_SI_CS_OFFSET)
#define SI_CS_DONE_ERR_MASK                      (sc->targetdef->d_SI_CS_DONE_ERR_MASK)
#define SI_CS_DONE_INT_MASK                      (sc->targetdef->d_SI_CS_DONE_INT_MASK)
#define SI_CS_START_LSB                          (sc->targetdef->d_SI_CS_START_LSB)
#define SI_CS_START_MASK                         (sc->targetdef->d_SI_CS_START_MASK)
#define SI_CS_RX_CNT_LSB                         (sc->targetdef->d_SI_CS_RX_CNT_LSB)
#define SI_CS_RX_CNT_MASK                        (sc->targetdef->d_SI_CS_RX_CNT_MASK)
#define SI_CS_TX_CNT_LSB                         (sc->targetdef->d_SI_CS_TX_CNT_LSB)
#define SI_CS_TX_CNT_MASK                        (sc->targetdef->d_SI_CS_TX_CNT_MASK)
#define EEPROM_SZ                                (sc->targetdef->d_BOARD_DATA_SZ)
#define EEPROM_EXT_SZ                            (sc->targetdef->d_BOARD_EXT_DATA_SZ)
#define MBOX_BASE_ADDRESS                        (sc->targetdef->d_MBOX_BASE_ADDRESS)
#define LOCAL_SCRATCH_OFFSET                     (sc->targetdef->d_LOCAL_SCRATCH_OFFSET)
#define CPU_CLOCK_OFFSET                         (sc->targetdef->d_CPU_CLOCK_OFFSET)
#define LPO_CAL_OFFSET                           (sc->targetdef->d_LPO_CAL_OFFSET)
#define GPIO_PIN10_OFFSET                        (sc->targetdef->d_GPIO_PIN10_OFFSET)
#define GPIO_PIN11_OFFSET                        (sc->targetdef->d_GPIO_PIN11_OFFSET)
#define GPIO_PIN12_OFFSET                        (sc->targetdef->d_GPIO_PIN12_OFFSET)
#define GPIO_PIN13_OFFSET                        (sc->targetdef->d_GPIO_PIN13_OFFSET)
#define CLOCK_GPIO_OFFSET                        (sc->targetdef->d_CLOCK_GPIO_OFFSET)
#define CPU_CLOCK_STANDARD_LSB                   (sc->targetdef->d_CPU_CLOCK_STANDARD_LSB)
#define CPU_CLOCK_STANDARD_MASK                  (sc->targetdef->d_CPU_CLOCK_STANDARD_MASK)
#define LPO_CAL_ENABLE_LSB                       (sc->targetdef->d_LPO_CAL_ENABLE_LSB)
#define LPO_CAL_ENABLE_MASK                      (sc->targetdef->d_LPO_CAL_ENABLE_MASK)
#define CLOCK_GPIO_BT_CLK_OUT_EN_LSB             (sc->targetdef->d_CLOCK_GPIO_BT_CLK_OUT_EN_LSB)
#define CLOCK_GPIO_BT_CLK_OUT_EN_MASK            (sc->targetdef->d_CLOCK_GPIO_BT_CLK_OUT_EN_MASK)
#define ANALOG_INTF_BASE_ADDRESS                 (sc->targetdef->d_ANALOG_INTF_BASE_ADDRESS)
#define WLAN_MAC_BASE_ADDRESS                    (sc->targetdef->d_WLAN_MAC_BASE_ADDRESS)
#define CE0_BASE_ADDRESS                         (sc->targetdef->d_CE0_BASE_ADDRESS)
#define CE1_BASE_ADDRESS                         (sc->targetdef->d_CE1_BASE_ADDRESS)
#define FW_INDICATOR_ADDRESS                     (sc->targetdef->d_FW_INDICATOR_ADDRESS)
#define DRAM_BASE_ADDRESS                        (sc->targetdef->d_DRAM_BASE_ADDRESS)
#define SOC_CORE_BASE_ADDRESS                    (sc->targetdef->d_SOC_CORE_BASE_ADDRESS)
#define CORE_CTRL_ADDRESS                        (sc->targetdef->d_CORE_CTRL_ADDRESS)
#define CE_COUNT                                 (sc->targetdef->d_CE_COUNT)
#define PCIE_INTR_ENABLE_ADDRESS                 (sc->targetdef->d_PCIE_INTR_ENABLE_ADDRESS)
#define PCIE_INTR_CLR_ADDRESS                    (sc->targetdef->d_PCIE_INTR_CLR_ADDRESS)
#define PCIE_INTR_FIRMWARE_MASK                  (sc->targetdef->d_PCIE_INTR_FIRMWARE_MASK)
#define PCIE_INTR_CE_MASK_ALL                    (sc->targetdef->d_PCIE_INTR_CE_MASK_ALL)
#define CORE_CTRL_CPU_INTR_MASK                  (sc->targetdef->d_CORE_CTRL_CPU_INTR_MASK)
#define PCIE_INTR_CAUSE_ADDRESS                  (sc->targetdef->d_PCIE_INTR_CAUSE_ADDRESS)
#define SOC_RESET_CONTROL_ADDRESS                (sc->targetdef->d_SOC_RESET_CONTROL_ADDRESS)
#define SOC_RESET_CONTROL_CE_RST_MASK            (sc->targetdef->d_SOC_RESET_CONTROL_CE_RST_MASK)
#define SOC_RESET_CONTROL_CPU_WARM_RST_MASK      (sc->targetdef->d_SOC_RESET_CONTROL_CPU_WARM_RST_MASK)
#define CPU_INTR_ADDRESS                         (sc->targetdef->d_CPU_INTR_ADDRESS)
#define SOC_LF_TIMER_CONTROL0_ADDRESS            (sc->targetdef->d_SOC_LF_TIMER_CONTROL0_ADDRESS)
#define SOC_LF_TIMER_CONTROL0_ENABLE_MASK        (sc->targetdef->d_SOC_LF_TIMER_CONTROL0_ENABLE_MASK)
#define SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_LSB  (sc->targetdef->d_SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_LSB)
#define SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_MASK (sc->targetdef->d_SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_MASK)

#define SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_GET(x) (((x) & SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_MASK) >> SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_LSB)
#define SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_SET(x) (((x) << SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_LSB) & SOC_RESET_CONTROL_PCIE_RST_SHORT_OVRD_MASK)

/* hif_pci.c */
#define CHIP_ID_ADDRESS                          (sc->targetdef->d_SOC_CHIP_ID_ADDRESS)
#define SOC_CHIP_ID_REVISION_MASK                (sc->targetdef->d_SOC_CHIP_ID_REVISION_MASK)
#define SOC_CHIP_ID_REVISION_LSB                 (sc->targetdef->d_SOC_CHIP_ID_REVISION_LSB)
#define SOC_CHIP_ID_VERSION_MASK                 (sc->targetdef->d_SOC_CHIP_ID_VERSION_MASK)
#define SOC_CHIP_ID_VERSION_LSB                  (sc->targetdef->d_SOC_CHIP_ID_VERSION_LSB)
#define CHIP_ID_REVISION_GET(x) (((x) & SOC_CHIP_ID_REVISION_MASK) >> SOC_CHIP_ID_REVISION_LSB)
#define CHIP_ID_VERSION_GET(x) (((x) & SOC_CHIP_ID_VERSION_MASK) >> SOC_CHIP_ID_VERSION_LSB)
/* hif_pci.c end */

/* misc */
#define SR_WR_INDEX_ADDRESS                      (sc->targetdef->d_SR_WR_INDEX_ADDRESS)
#define DST_WATERMARK_ADDRESS                    (sc->targetdef->d_DST_WATERMARK_ADDRESS)
/* end */

/* htt_rx.c */
#define RX_MSDU_END_4_FIRST_MSDU_MASK            (pdev->targetdef->d_RX_MSDU_END_4_FIRST_MSDU_MASK)
#define RX_MSDU_END_4_FIRST_MSDU_LSB             (pdev->targetdef->d_RX_MSDU_END_4_FIRST_MSDU_LSB)
#define RX_MPDU_START_0_SEQ_NUM_MASK             (pdev->targetdef->d_RX_MPDU_START_0_SEQ_NUM_MASK)
#define RX_MPDU_START_0_SEQ_NUM_LSB              (pdev->targetdef->d_RX_MPDU_START_0_SEQ_NUM_LSB)
#define RX_MPDU_START_2_PN_47_32_LSB             (pdev->targetdef->d_RX_MPDU_START_2_PN_47_32_LSB)
#define RX_MPDU_START_2_PN_47_32_MASK            (pdev->targetdef->d_RX_MPDU_START_2_PN_47_32_MASK)
#define RX_MSDU_END_1_KEY_ID_OCT_MASK            (pdev->targetdef->d_RX_MSDU_END_1_KEY_ID_OCT_MASK)
#define RX_MSDU_END_1_KEY_ID_OCT_LSB             (pdev->targetdef->d_RX_MSDU_END_1_KEY_ID_OCT_LSB)
#define RX_MSDU_END_1_EXT_WAPI_PN_63_48_MASK     (pdev->targetdef->d_RX_MSDU_END_1_EXT_WAPI_PN_63_48_MASK)
#define RX_MSDU_END_1_EXT_WAPI_PN_63_48_LSB      (pdev->targetdef->d_RX_MSDU_END_1_EXT_WAPI_PN_63_48_LSB)
#define RX_MSDU_END_4_LAST_MSDU_MASK             (pdev->targetdef->d_RX_MSDU_END_4_LAST_MSDU_MASK)
#define RX_MSDU_END_4_LAST_MSDU_LSB              (pdev->targetdef->d_RX_MSDU_END_4_LAST_MSDU_LSB)
#define RX_ATTENTION_0_MCAST_BCAST_MASK          (pdev->targetdef->d_RX_ATTENTION_0_MCAST_BCAST_MASK)
#define RX_ATTENTION_0_MCAST_BCAST_LSB           (pdev->targetdef->d_RX_ATTENTION_0_MCAST_BCAST_LSB)
#define RX_ATTENTION_0_FRAGMENT_MASK             (pdev->targetdef->d_RX_ATTENTION_0_FRAGMENT_MASK)
#define RX_ATTENTION_0_FRAGMENT_LSB              (pdev->targetdef->d_RX_ATTENTION_0_FRAGMENT_LSB)
#define RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK      (pdev->targetdef->d_RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK)
#define RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK     (pdev->targetdef->d_RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK)
#define RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB      (pdev->targetdef->d_RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB)
#define RX_MSDU_START_0_MSDU_LENGTH_MASK         (pdev->targetdef->d_RX_MSDU_START_0_MSDU_LENGTH_MASK)
#define RX_MSDU_START_0_MSDU_LENGTH_LSB          (pdev->targetdef->d_RX_MSDU_START_0_MSDU_LENGTH_LSB)
#define RX_MSDU_START_2_DECAP_FORMAT_OFFSET      (pdev->targetdef->d_RX_MSDU_START_2_DECAP_FORMAT_OFFSET)
#define RX_MSDU_START_2_DECAP_FORMAT_MASK        (pdev->targetdef->d_RX_MSDU_START_2_DECAP_FORMAT_MASK)
#define RX_MSDU_START_2_DECAP_FORMAT_LSB         (pdev->targetdef->d_RX_MSDU_START_2_DECAP_FORMAT_LSB)
#define RX_MPDU_START_0_ENCRYPTED_MASK           (pdev->targetdef->d_RX_MPDU_START_0_ENCRYPTED_MASK)
#define RX_MPDU_START_0_ENCRYPTED_LSB            (pdev->targetdef->d_RX_MPDU_START_0_ENCRYPTED_LSB)
#define RX_ATTENTION_0_MORE_DATA_MASK            (pdev->targetdef->d_RX_ATTENTION_0_MORE_DATA_MASK)
#define RX_ATTENTION_0_MSDU_DONE_MASK            (pdev->targetdef->d_RX_ATTENTION_0_MSDU_DONE_MASK)
#define RX_ATTENTION_0_TCP_UDP_CHKSUM_FAIL_MASK  (pdev->targetdef->d_RX_ATTENTION_0_TCP_UDP_CHKSUM_FAIL_MASK)
/* end */

/* copy_engine.c */
#define DST_WR_INDEX_ADDRESS                     (sc->targetdef->d_DST_WR_INDEX_ADDRESS)
#define SRC_WATERMARK_ADDRESS                    (sc->targetdef->d_SRC_WATERMARK_ADDRESS)
#define SRC_WATERMARK_LOW_MASK                   (sc->targetdef->d_SRC_WATERMARK_LOW_MASK)
#define SRC_WATERMARK_HIGH_MASK                  (sc->targetdef->d_SRC_WATERMARK_HIGH_MASK)
#define DST_WATERMARK_LOW_MASK                   (sc->targetdef->d_DST_WATERMARK_LOW_MASK)
#define DST_WATERMARK_HIGH_MASK                  (sc->targetdef->d_DST_WATERMARK_HIGH_MASK)
#define CURRENT_SRRI_ADDRESS                     (sc->targetdef->d_CURRENT_SRRI_ADDRESS)
#define CURRENT_DRRI_ADDRESS                     (sc->targetdef->d_CURRENT_DRRI_ADDRESS)
#define HOST_IS_SRC_RING_HIGH_WATERMARK_MASK     (sc->targetdef->d_HOST_IS_SRC_RING_HIGH_WATERMARK_MASK)
#define HOST_IS_SRC_RING_LOW_WATERMARK_MASK      (sc->targetdef->d_HOST_IS_SRC_RING_LOW_WATERMARK_MASK)
#define HOST_IS_DST_RING_HIGH_WATERMARK_MASK     (sc->targetdef->d_HOST_IS_DST_RING_HIGH_WATERMARK_MASK)
#define HOST_IS_DST_RING_LOW_WATERMARK_MASK      (sc->targetdef->d_HOST_IS_DST_RING_LOW_WATERMARK_MASK)
#define HOST_IS_ADDRESS                          (sc->targetdef->d_HOST_IS_ADDRESS)
#define HOST_IS_COPY_COMPLETE_MASK               (sc->targetdef->d_HOST_IS_COPY_COMPLETE_MASK)
#define CE_WRAPPER_BASE_ADDRESS                  (sc->targetdef->d_CE_WRAPPER_BASE_ADDRESS)
#define CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS     (sc->targetdef->d_CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS)
#define HOST_IE_ADDRESS                          (sc->targetdef->d_HOST_IE_ADDRESS)
#define HOST_IE_COPY_COMPLETE_MASK               (sc->targetdef->d_HOST_IE_COPY_COMPLETE_MASK)
#define SR_BA_ADDRESS                            (sc->targetdef->d_SR_BA_ADDRESS)
#define SR_SIZE_ADDRESS                          (sc->targetdef->d_SR_SIZE_ADDRESS)
#define CE_CTRL1_ADDRESS                         (sc->targetdef->d_CE_CTRL1_ADDRESS)
#define CE_CTRL1_DMAX_LENGTH_MASK                (sc->targetdef->d_CE_CTRL1_DMAX_LENGTH_MASK)
#define DR_BA_ADDRESS                            (sc->targetdef->d_DR_BA_ADDRESS)
#define DR_SIZE_ADDRESS                          (sc->targetdef->d_DR_SIZE_ADDRESS)
#define MISC_IE_ADDRESS                          (sc->targetdef->d_MISC_IE_ADDRESS)
#define MISC_IS_AXI_ERR_MASK                     (sc->targetdef->d_MISC_IS_AXI_ERR_MASK)
#define MISC_IS_DST_ADDR_ERR_MASK                (sc->targetdef->d_MISC_IS_DST_ADDR_ERR_MASK)
#define MISC_IS_SRC_LEN_ERR_MASK                 (sc->targetdef->d_MISC_IS_SRC_LEN_ERR_MASK)
#define MISC_IS_DST_MAX_LEN_VIO_MASK             (sc->targetdef->d_MISC_IS_DST_MAX_LEN_VIO_MASK)
#define MISC_IS_DST_RING_OVERFLOW_MASK           (sc->targetdef->d_MISC_IS_DST_RING_OVERFLOW_MASK)
#define MISC_IS_SRC_RING_OVERFLOW_MASK           (sc->targetdef->d_MISC_IS_SRC_RING_OVERFLOW_MASK)
#define SRC_WATERMARK_LOW_LSB                    (sc->targetdef->d_SRC_WATERMARK_LOW_LSB)
#define SRC_WATERMARK_HIGH_LSB                   (sc->targetdef->d_SRC_WATERMARK_HIGH_LSB)
#define DST_WATERMARK_LOW_LSB                    (sc->targetdef->d_DST_WATERMARK_LOW_LSB)
#define DST_WATERMARK_HIGH_LSB                   (sc->targetdef->d_DST_WATERMARK_HIGH_LSB)
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK (sc->targetdef->d_CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK)
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB  (sc->targetdef->d_CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB)
#define CE_CTRL1_DMAX_LENGTH_LSB                 (sc->targetdef->d_CE_CTRL1_DMAX_LENGTH_LSB)
#define CE_CTRL1_SRC_RING_BYTE_SWAP_EN_MASK      (sc->targetdef->d_CE_CTRL1_SRC_RING_BYTE_SWAP_EN_MASK)
#define CE_CTRL1_DST_RING_BYTE_SWAP_EN_MASK      (sc->targetdef->d_CE_CTRL1_DST_RING_BYTE_SWAP_EN_MASK)
#define CE_CTRL1_SRC_RING_BYTE_SWAP_EN_LSB       (sc->targetdef->d_CE_CTRL1_SRC_RING_BYTE_SWAP_EN_LSB)
#define CE_CTRL1_DST_RING_BYTE_SWAP_EN_LSB       (sc->targetdef->d_CE_CTRL1_DST_RING_BYTE_SWAP_EN_LSB)
/* end */

/* SET macros */
#define WLAN_SYSTEM_SLEEP_DISABLE_SET(x)         (((x) << WLAN_SYSTEM_SLEEP_DISABLE_LSB) & WLAN_SYSTEM_SLEEP_DISABLE_MASK)
#define SI_CONFIG_BIDIR_OD_DATA_SET(x)           (((x) << SI_CONFIG_BIDIR_OD_DATA_LSB) & SI_CONFIG_BIDIR_OD_DATA_MASK)
#define SI_CONFIG_I2C_SET(x)                     (((x) << SI_CONFIG_I2C_LSB) & SI_CONFIG_I2C_MASK)
#define SI_CONFIG_POS_SAMPLE_SET(x)              (((x) << SI_CONFIG_POS_SAMPLE_LSB) & SI_CONFIG_POS_SAMPLE_MASK)
#define SI_CONFIG_INACTIVE_CLK_SET(x)            (((x) << SI_CONFIG_INACTIVE_CLK_LSB) & SI_CONFIG_INACTIVE_CLK_MASK)
#define SI_CONFIG_INACTIVE_DATA_SET(x)           (((x) << SI_CONFIG_INACTIVE_DATA_LSB) & SI_CONFIG_INACTIVE_DATA_MASK)
#define SI_CONFIG_DIVIDER_SET(x)                 (((x) << SI_CONFIG_DIVIDER_LSB) & SI_CONFIG_DIVIDER_MASK)
#define SI_CS_START_SET(x)                       (((x) << SI_CS_START_LSB) & SI_CS_START_MASK)
#define SI_CS_RX_CNT_SET(x)                      (((x) << SI_CS_RX_CNT_LSB) & SI_CS_RX_CNT_MASK)
#define SI_CS_TX_CNT_SET(x)                      (((x) << SI_CS_TX_CNT_LSB) & SI_CS_TX_CNT_MASK)
#define LPO_CAL_ENABLE_SET(x)                    (((x) << LPO_CAL_ENABLE_LSB) & LPO_CAL_ENABLE_MASK)
#define CPU_CLOCK_STANDARD_SET(x)                (((x) << CPU_CLOCK_STANDARD_LSB) & CPU_CLOCK_STANDARD_MASK)
#define CLOCK_GPIO_BT_CLK_OUT_EN_SET(x)          (((x) << CLOCK_GPIO_BT_CLK_OUT_EN_LSB) & CLOCK_GPIO_BT_CLK_OUT_EN_MASK)
/* copy_engine.c */
#define SRC_WATERMARK_LOW_SET(x)                 (((x) << SRC_WATERMARK_LOW_LSB) & SRC_WATERMARK_LOW_MASK)
#define SRC_WATERMARK_HIGH_SET(x)                (((x) << SRC_WATERMARK_HIGH_LSB) & SRC_WATERMARK_HIGH_MASK)
#define DST_WATERMARK_LOW_SET(x)                 (((x) << DST_WATERMARK_LOW_LSB) & DST_WATERMARK_LOW_MASK)
#define DST_WATERMARK_HIGH_SET(x)                (((x) << DST_WATERMARK_HIGH_LSB) & DST_WATERMARK_HIGH_MASK)
#define CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_GET(x) (((x) & CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_MASK) >> CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_LSB)
#define CE_CTRL1_DMAX_LENGTH_SET(x)              (((x) << CE_CTRL1_DMAX_LENGTH_LSB) & CE_CTRL1_DMAX_LENGTH_MASK)
#define CE_CTRL1_SRC_RING_BYTE_SWAP_EN_SET(x)    (((x) << CE_CTRL1_SRC_RING_BYTE_SWAP_EN_LSB) & CE_CTRL1_SRC_RING_BYTE_SWAP_EN_MASK)
#define CE_CTRL1_DST_RING_BYTE_SWAP_EN_SET(x)    (((x) << CE_CTRL1_DST_RING_BYTE_SWAP_EN_LSB) & CE_CTRL1_DST_RING_BYTE_SWAP_EN_MASK)
/* end */

typedef struct hostdef_s {
    A_UINT32 d_INT_STATUS_ENABLE_ERROR_LSB;
    A_UINT32 d_INT_STATUS_ENABLE_ERROR_MASK;
    A_UINT32 d_INT_STATUS_ENABLE_CPU_LSB;
    A_UINT32 d_INT_STATUS_ENABLE_CPU_MASK;
    A_UINT32 d_INT_STATUS_ENABLE_COUNTER_LSB;
    A_UINT32 d_INT_STATUS_ENABLE_COUNTER_MASK;
    A_UINT32 d_INT_STATUS_ENABLE_MBOX_DATA_LSB;
    A_UINT32 d_INT_STATUS_ENABLE_MBOX_DATA_MASK;
    A_UINT32 d_ERROR_STATUS_ENABLE_RX_UNDERFLOW_LSB;
    A_UINT32 d_ERROR_STATUS_ENABLE_RX_UNDERFLOW_MASK;
    A_UINT32 d_ERROR_STATUS_ENABLE_TX_OVERFLOW_LSB;
    A_UINT32 d_ERROR_STATUS_ENABLE_TX_OVERFLOW_MASK;
    A_UINT32 d_COUNTER_INT_STATUS_ENABLE_BIT_LSB;
    A_UINT32 d_COUNTER_INT_STATUS_ENABLE_BIT_MASK;
    A_UINT32 d_INT_STATUS_ENABLE_ADDRESS;
    A_UINT32 d_CPU_INT_STATUS_ENABLE_BIT_LSB;
    A_UINT32 d_CPU_INT_STATUS_ENABLE_BIT_MASK;
    A_UINT32 d_HOST_INT_STATUS_ADDRESS;
    A_UINT32 d_CPU_INT_STATUS_ADDRESS;
    A_UINT32 d_ERROR_INT_STATUS_ADDRESS;
    A_UINT32 d_ERROR_INT_STATUS_WAKEUP_MASK;
    A_UINT32 d_ERROR_INT_STATUS_WAKEUP_LSB;
    A_UINT32 d_ERROR_INT_STATUS_RX_UNDERFLOW_MASK;
    A_UINT32 d_ERROR_INT_STATUS_RX_UNDERFLOW_LSB;
    A_UINT32 d_ERROR_INT_STATUS_TX_OVERFLOW_MASK;
    A_UINT32 d_ERROR_INT_STATUS_TX_OVERFLOW_LSB;
    A_UINT32 d_COUNT_DEC_ADDRESS;
    A_UINT32 d_HOST_INT_STATUS_CPU_MASK;
    A_UINT32 d_HOST_INT_STATUS_CPU_LSB;
    A_UINT32 d_HOST_INT_STATUS_ERROR_MASK;
    A_UINT32 d_HOST_INT_STATUS_ERROR_LSB;
    A_UINT32 d_HOST_INT_STATUS_COUNTER_MASK;
    A_UINT32 d_HOST_INT_STATUS_COUNTER_LSB;
    A_UINT32 d_RX_LOOKAHEAD_VALID_ADDRESS;
    A_UINT32 d_WINDOW_DATA_ADDRESS;
    A_UINT32 d_WINDOW_READ_ADDR_ADDRESS;
    A_UINT32 d_WINDOW_WRITE_ADDR_ADDRESS;
    A_UINT32 d_SOC_GLOBAL_RESET_ADDRESS;
    A_UINT32 d_RTC_STATE_ADDRESS;
    A_UINT32 d_RTC_STATE_COLD_RESET_MASK;
    A_UINT32 d_PCIE_LOCAL_BASE_ADDRESS;
    A_UINT32 d_PCIE_SOC_WAKE_RESET;
    A_UINT32 d_PCIE_SOC_WAKE_ADDRESS;
    A_UINT32 d_PCIE_SOC_WAKE_V_MASK;
    A_UINT32 d_RTC_STATE_V_MASK;
    A_UINT32 d_RTC_STATE_V_LSB;
    A_UINT32 d_FW_IND_EVENT_PENDING;
    A_UINT32 d_FW_IND_INITIALIZED;
    A_UINT32 d_RTC_STATE_V_ON;
#if defined(SDIO_3_0)
    A_UINT32 d_HOST_INT_STATUS_MBOX_DATA_MASK;
    A_UINT32 d_HOST_INT_STATUS_MBOX_DATA_LSB;
#endif
    A_UINT32 d_PCIE_SOC_RDY_STATUS_ADDRESS;
    A_UINT32 d_PCIE_SOC_RDY_STATUS_BAR_MASK;
    A_UINT32 d_SOC_PCIE_BASE_ADDRESS;
    A_UINT32 d_MSI_MAGIC_ADR_ADDRESS;
    A_UINT32 d_MSI_MAGIC_ADDRESS;
} HOST_REGISTER_TABLE;

#define INT_STATUS_ENABLE_ERROR_LSB              (sc->hostdef->d_INT_STATUS_ENABLE_ERROR_LSB)
#define INT_STATUS_ENABLE_ERROR_MASK             (sc->hostdef->d_INT_STATUS_ENABLE_ERROR_MASK)
#define INT_STATUS_ENABLE_CPU_LSB                (sc->hostdef->d_INT_STATUS_ENABLE_CPU_LSB)
#define INT_STATUS_ENABLE_CPU_MASK               (sc->hostdef->d_INT_STATUS_ENABLE_CPU_MASK)
#define INT_STATUS_ENABLE_COUNTER_LSB            (sc->hostdef->d_INT_STATUS_ENABLE_COUNTER_LSB)
#define INT_STATUS_ENABLE_COUNTER_MASK           (sc->hostdef->d_INT_STATUS_ENABLE_COUNTER_MASK)
#define INT_STATUS_ENABLE_MBOX_DATA_LSB          (sc->hostdef->d_INT_STATUS_ENABLE_MBOX_DATA_LSB)
#define INT_STATUS_ENABLE_MBOX_DATA_MASK         (sc->hostdef->d_INT_STATUS_ENABLE_MBOX_DATA_MASK)
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW_LSB     (sc->hostdef->d_ERROR_STATUS_ENABLE_RX_UNDERFLOW_LSB)
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW_MASK    (sc->hostdef->d_ERROR_STATUS_ENABLE_RX_UNDERFLOW_MASK)
#define ERROR_STATUS_ENABLE_TX_OVERFLOW_LSB      (sc->hostdef->d_ERROR_STATUS_ENABLE_TX_OVERFLOW_LSB)
#define ERROR_STATUS_ENABLE_TX_OVERFLOW_MASK     (sc->hostdef->d_ERROR_STATUS_ENABLE_TX_OVERFLOW_MASK)
#define COUNTER_INT_STATUS_ENABLE_BIT_LSB        (sc->hostdef->d_COUNTER_INT_STATUS_ENABLE_BIT_LSB)
#define COUNTER_INT_STATUS_ENABLE_BIT_MASK       (sc->hostdef->d_COUNTER_INT_STATUS_ENABLE_BIT_MASK)
#define INT_STATUS_ENABLE_ADDRESS                (sc->hostdef->d_INT_STATUS_ENABLE_ADDRESS)
#define CPU_INT_STATUS_ENABLE_BIT_LSB            (sc->hostdef->d_CPU_INT_STATUS_ENABLE_BIT_LSB)
#define CPU_INT_STATUS_ENABLE_BIT_MASK           (sc->hostdef->d_CPU_INT_STATUS_ENABLE_BIT_MASK)
#define HOST_INT_STATUS_ADDRESS                  (sc->hostdef->d_HOST_INT_STATUS_ADDRESS)
#define CPU_INT_STATUS_ADDRESS                   (sc->hostdef->d_CPU_INT_STATUS_ADDRESS)
#define ERROR_INT_STATUS_ADDRESS                 (sc->hostdef->d_ERROR_INT_STATUS_ADDRESS)
#define ERROR_INT_STATUS_WAKEUP_MASK             (sc->hostdef->d_ERROR_INT_STATUS_WAKEUP_MASK)
#define ERROR_INT_STATUS_WAKEUP_LSB              (sc->hostdef->d_ERROR_INT_STATUS_WAKEUP_LSB)
#define ERROR_INT_STATUS_RX_UNDERFLOW_MASK       (sc->hostdef->d_ERROR_INT_STATUS_RX_UNDERFLOW_MASK)
#define ERROR_INT_STATUS_RX_UNDERFLOW_LSB        (sc->hostdef->d_ERROR_INT_STATUS_RX_UNDERFLOW_LSB)
#define ERROR_INT_STATUS_TX_OVERFLOW_MASK        (sc->hostdef->d_ERROR_INT_STATUS_TX_OVERFLOW_MASK)
#define ERROR_INT_STATUS_TX_OVERFLOW_LSB         (sc->hostdef->d_ERROR_INT_STATUS_TX_OVERFLOW_LSB)
#define COUNT_DEC_ADDRESS                        (sc->hostdef->d_COUNT_DEC_ADDRESS)
#define HOST_INT_STATUS_CPU_MASK                 (sc->hostdef->d_HOST_INT_STATUS_CPU_MASK)
#define HOST_INT_STATUS_CPU_LSB                  (sc->hostdef->d_HOST_INT_STATUS_CPU_LSB)
#define HOST_INT_STATUS_ERROR_MASK               (sc->hostdef->d_HOST_INT_STATUS_ERROR_MASK)
#define HOST_INT_STATUS_ERROR_LSB                (sc->hostdef->d_HOST_INT_STATUS_ERROR_LSB)
#define HOST_INT_STATUS_COUNTER_MASK             (sc->hostdef->d_HOST_INT_STATUS_COUNTER_MASK)
#define HOST_INT_STATUS_COUNTER_LSB              (sc->hostdef->d_HOST_INT_STATUS_COUNTER_LSB)
#define RX_LOOKAHEAD_VALID_ADDRESS               (sc->hostdef->d_RX_LOOKAHEAD_VALID_ADDRESS)
#define WINDOW_DATA_ADDRESS                      (sc->hostdef->d_WINDOW_DATA_ADDRESS)
#define WINDOW_READ_ADDR_ADDRESS                 (sc->hostdef->d_WINDOW_READ_ADDR_ADDRESS)
#define WINDOW_WRITE_ADDR_ADDRESS                (sc->hostdef->d_WINDOW_WRITE_ADDR_ADDRESS)
#define SOC_GLOBAL_RESET_ADDRESS                 (sc->hostdef->d_SOC_GLOBAL_RESET_ADDRESS)
#define RTC_STATE_ADDRESS                        (sc->hostdef->d_RTC_STATE_ADDRESS)
#define RTC_STATE_COLD_RESET_MASK                (sc->hostdef->d_RTC_STATE_COLD_RESET_MASK)
#define PCIE_LOCAL_BASE_ADDRESS                  (sc->hostdef->d_PCIE_LOCAL_BASE_ADDRESS)
#define PCIE_SOC_WAKE_RESET                      (sc->hostdef->d_PCIE_SOC_WAKE_RESET)
#define PCIE_SOC_WAKE_ADDRESS                    (sc->hostdef->d_PCIE_SOC_WAKE_ADDRESS)
#define PCIE_SOC_WAKE_V_MASK                     (sc->hostdef->d_PCIE_SOC_WAKE_V_MASK)
#define RTC_STATE_V_MASK                         (sc->hostdef->d_RTC_STATE_V_MASK)
#define RTC_STATE_V_LSB                          (sc->hostdef->d_RTC_STATE_V_LSB)
#define FW_IND_EVENT_PENDING                     (sc->hostdef->d_FW_IND_EVENT_PENDING)
#define FW_IND_INITIALIZED                       (sc->hostdef->d_FW_IND_INITIALIZED)
#define RTC_STATE_V_ON                           (sc->hostdef->d_RTC_STATE_V_ON)
#if defined(SDIO_3_0)
#define HOST_INT_STATUS_MBOX_DATA_MASK           (sc->hostdef->d_HOST_INT_STATUS_MBOX_DATA_MASK)
#define HOST_INT_STATUS_MBOX_DATA_LSB            (sc->hostdef->d_HOST_INT_STATUS_MBOX_DATA_LSB)
#endif

#if !defined(SOC_PCIE_BASE_ADDRESS)
#define SOC_PCIE_BASE_ADDRESS 0
#endif

#if !defined(PCIE_SOC_RDY_STATUS_ADDRESS)
#define PCIE_SOC_RDY_STATUS_ADDRESS 0
#define PCIE_SOC_RDY_STATUS_BAR_MASK 0
#endif

#if !defined(MSI_MAGIC_ADR_ADDRESS)
#define MSI_MAGIC_ADR_ADDRESS 0
#define MSI_MAGIC_ADDRESS 0
#endif

/* SET/GET macros */
#define INT_STATUS_ENABLE_ERROR_SET(x)          (((x) << INT_STATUS_ENABLE_ERROR_LSB) & INT_STATUS_ENABLE_ERROR_MASK)
#define INT_STATUS_ENABLE_CPU_SET(x)            (((x) << INT_STATUS_ENABLE_CPU_LSB) & INT_STATUS_ENABLE_CPU_MASK)
#define INT_STATUS_ENABLE_COUNTER_SET(x)        (((x) << INT_STATUS_ENABLE_COUNTER_LSB) & INT_STATUS_ENABLE_COUNTER_MASK)
#define INT_STATUS_ENABLE_MBOX_DATA_SET(x)      (((x) << INT_STATUS_ENABLE_MBOX_DATA_LSB) & INT_STATUS_ENABLE_MBOX_DATA_MASK)
#define CPU_INT_STATUS_ENABLE_BIT_SET(x)        (((x) << CPU_INT_STATUS_ENABLE_BIT_LSB) & CPU_INT_STATUS_ENABLE_BIT_MASK)
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW_SET(x) (((x) << ERROR_STATUS_ENABLE_RX_UNDERFLOW_LSB) & ERROR_STATUS_ENABLE_RX_UNDERFLOW_MASK)
#define ERROR_STATUS_ENABLE_TX_OVERFLOW_SET(x)  (((x) << ERROR_STATUS_ENABLE_TX_OVERFLOW_LSB) & ERROR_STATUS_ENABLE_TX_OVERFLOW_MASK)
#define COUNTER_INT_STATUS_ENABLE_BIT_SET(x)    (((x) << COUNTER_INT_STATUS_ENABLE_BIT_LSB) & COUNTER_INT_STATUS_ENABLE_BIT_MASK)
#define ERROR_INT_STATUS_WAKEUP_GET(x)          (((x) & ERROR_INT_STATUS_WAKEUP_MASK) >> ERROR_INT_STATUS_WAKEUP_LSB)
#define ERROR_INT_STATUS_RX_UNDERFLOW_GET(x)    (((x) & ERROR_INT_STATUS_RX_UNDERFLOW_MASK) >> ERROR_INT_STATUS_RX_UNDERFLOW_LSB)
#define ERROR_INT_STATUS_TX_OVERFLOW_GET(x)     (((x) & ERROR_INT_STATUS_TX_OVERFLOW_MASK) >> ERROR_INT_STATUS_TX_OVERFLOW_LSB)
#define HOST_INT_STATUS_CPU_GET(x)              (((x) & HOST_INT_STATUS_CPU_MASK) >> HOST_INT_STATUS_CPU_LSB)
#define HOST_INT_STATUS_ERROR_GET(x)            (((x) & HOST_INT_STATUS_ERROR_MASK) >> HOST_INT_STATUS_ERROR_LSB)
#define HOST_INT_STATUS_COUNTER_GET(x)          (((x) & HOST_INT_STATUS_COUNTER_MASK) >> HOST_INT_STATUS_COUNTER_LSB)
#define RTC_STATE_V_GET(x)                      (((x) & RTC_STATE_V_MASK) >> RTC_STATE_V_LSB)
#if defined(SDIO_3_0)
#define HOST_INT_STATUS_MBOX_DATA_GET(x)        (((x) & HOST_INT_STATUS_MBOX_DATA_MASK) >> HOST_INT_STATUS_MBOX_DATA_LSB)
#endif
#endif
