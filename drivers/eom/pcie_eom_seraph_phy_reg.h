/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef PCIE_EOM_SERAPH_PHY_REG_H
#define PCIE_EOM_SERAPH_PHY_REG_H

#define SERAPH_PHY_QSERDES_OFFSET_SIZE                    0x800
#define SERAPH_PHY_LANE_SIZE(lanenum)                     (lanenum * SERAPH_PHY_QSERDES_OFFSET_SIZE)

#define QSERDES_TX_PCIE_USB3_UNI_QMP_TX_REG_BASE 0xe00
#define QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE  0x1000
#define HWIO_PCIE_QSERDES_TX_LANE_MODE_1_ADDR \
	(QSERDES_TX_PCIE_USB3_UNI_QMP_TX_REG_BASE + 0x84)
#define HWIO_PCIE_QSERDES_TX_LANE_MODE_5_ADDR \
	(QSERDES_TX_PCIE_USB3_UNI_QMP_TX_REG_BASE + 0x94)
#define HWIO_PCIE_QSERDES_TX_RESET_GEN_ADDR \
	(QSERDES_TX_PCIE_USB3_UNI_QMP_TX_REG_BASE + 0xb8)
#define HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR \
	(QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE + 0x5c)
#define HWIO_PCIE_QSERDES_RX_DFE_4_ADDR \
	(QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE + 0xc0)
#define HWIO_PCIE_QSERDES_RX_AUX_DATA_TCOARSE_TFINE_ADDR \
	(QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE + 0x60)
#define HWIO_PCIE_QSERDES_RX_VTH_CODE_ADDR \
	(QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE + 0x1b0)
#define HWIO_PCIE_QSERDES_RX_RCLK_AUXDATA_SEL_ADDR \
	(QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE + 0x64)
#define HWIO_PCIE_QSERDES_RX_CDR_RESET_OVERRIDE_ADDR \
	(QSERDES_RX_PCIE_USB3_UNI_QMP_RX_REG_BASE + 0x130)
#define HWIO_PCIE_QSERDES_TX_IA_ERROR_COUNTER_LOW_ADDR \
	(QSERDES_TX_PCIE_USB3_UNI_QMP_TX_REG_BASE + 0x11c)
#define HWIO_PCIE_QSERDES_TX_IA_ERROR_COUNTER_HIGH_ADDR \
	(QSERDES_TX_PCIE_USB3_UNI_QMP_TX_REG_BASE + 0x120)

/* Link Control and Status Register */
#define HWIO_PCIE_GEN3X1_LINK_CONTROL_LINK_STATUS_REG_ADDR  (PCIE_RC_BASE_ADDR + 0x80)


/* Helper macros for register access with RC index and lane */
#define SERAPH_PHY_REG_ADDR(base_reg, lanenum)    (base_reg + SERAPH_PHY_LANE_SIZE(lanenum))

#define SERAPH_PHY_MAX_EYE_HEIGHT                         25
#define SERAPH_PHY_MAX_EYE_WIDTH                          64
#define SERAPH_PHY_MAX_TCOARSE                            7
#define SERAPH_PHY_TCOARSE_STEP                           2

/* Link register bit definitions */
#define LINK_WIDTH_MASK                                   0x3f00000
#define LINK_WIDTH_SHIFT                                  0x14
#define LINK_SPEED_MASK                                   0xf0000
#define LINK_SPEED_SHIFT                                  0x10

/* Maximum supported RC instances */
#define SERAPH_PHY_MAX_RC_INSTANCES                       8

#endif /* PCIE_EOM_SERAPH_PHY_REG_H */
