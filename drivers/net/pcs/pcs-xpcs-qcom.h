/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Synopsys DesignWare XPCS platform device driver
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/pcs-xpcs-qcom.h>

#define LINK_STS_RETRY_COUNT		50
#define SYNOPSYS_XPCS_ID		0x7996ced0
#define SYNOPSYS_XPCS_MASK		0xffffffff // Reset mask for Dev IDs

/* Vendor regs access */
#define DW_VENDOR			BIT(15)

/* VR_XS_PCS */
#define DW_USXGMII_RST			BIT(10)
#define DW_USXGMII_EN			BIT(9)
#define DW_VR_XS_PCS_DIG_STS		0x2040 /* SWI name: EMAC0_VR_XS_PCS_DIG_STS */
#define DW_RXFIFO_ERR			GENMASK(6, 5)
#define DW_R2TLBE				BIT(14)

/* SR_MII */
#define DW_FULL_DUPLEX			BIT(8)
#define DW_USXGMII_SS_MASK		(BIT(13) | BIT(6) | BIT(5))
#define DW_USXGMII_10000		(BIT(13) | BIT(6))
#define DW_USXGMII_5000			(BIT(13) | BIT(5))
#define DW_GMII_2500			(BIT(5))
#define DW_GMII_1000			(BIT(6))
#define DW_GMII_100				(BIT(13))
#define DW_SGMII_SS_MASK		(BIT(13) | BIT(6))

/* SR_AN */
#define DW_SR_AN_ADV1			0x10
#define DW_SR_AN_ADV2			0x11
#define DW_SR_AN_ADV3			0x12
#define DW_SR_AN_LP_ABL1		0x13
#define DW_SR_AN_LP_ABL2		0x14
#define DW_SR_AN_LP_ABL3		0x15

/* Clause 37 Defines */
/* VR MII MMD registers offsets */
#define DW_SR_MII_MMD_CTRL		0x4000 /* SWI name: EMAC0_SR_MII_CTRL */
#define DW_VR_MII_DIG_CTRL1		0x5000 /* SWI name: EMAC0_VR_MII_DIG_CTRL1 */
#define DW_VR_MII_AN_CTRL		0x5004 /* SWI name: EMAC0_VR_MII_AN_CTRL */
#define DW_VR_MII_AN_INTR_STS		0x5008 /* SWI name: EMAC0_VR_MII_AN_INTR_STS */
/* Enable 2.5G Mode */
#define DW_VR_MII_DIG_CTRL1_2G5_EN	BIT(2)
/* EEE Mode Control Register */
#define DW_VR_XS_PCS_EEE_MCTRL0		0x2018 /* SWI name: EMAC0_VR_XS_PCS_EEE_MCTRL0 */
#define DW_VR_XS_PCS_EEE_MCTRL1		0x202c /* SWI name: EMAC0_VR_XS_PCS_EEE_MCTRL1 */
#define DW_VR_MII_DIG_CTRL2		0x2004 /* SWI name: EMAC0_VR_XS_PCS_DIG_CTRL2 */

#define DW_SR_MII_PCS_CTRL1		0x0000 /* SWI name: EMAC0_SR_XS_PCS_CTRL1 */
#define DW_SR_MII_PCS_STS1		0x0004 /* SWI name: EMAC0_SR_XS_PCS_STS1 */
#define DW_SR_MII_PCS_DEV_ID1		0x0008
#define DW_SR_MII_PCS_DEV_ID2		0x000c
#define DW_SR_MII_PCS_SPD_ABL		0x0010
#define DW_SR_MII_PCS_DEV_PKG1		0x0014
#define DW_SR_MII_PCS_DEV_PKG2		0x0018
#define DW_SR_MII_PCS_CTRL2		0x001c
#define DW_SR_MII_PCS_STS2		0x0020
#define DW_SR_MII_PCS_STS3		0x0024
#define DW_SR_MII_PCS_PKG1		0x0038
#define DW_SR_MII_PCS_PKG2		0x003c
#define DW_SR_MII_PCS_EEE_ABL		0x0050
#define DW_SR_MII_PCS_EEE_ABL2		0x0054
#define DW_SR_MII_PCS_EEE_WKERR		0x0058
#define DW_SR_MII_PCS_LSTS		0x0060
#define DW_SR_MII_PCS_TCTRL		0x0064
#define DW_SR_MII_PCS_KR_STS1		0x0080
#define DW_SR_MII_PCS_KR_STS2		0x0084
#define DW_SR_MII_PCS_TP_A0		0x0088
#define DW_SR_MII_PCS_TP_A1		0x008c
#define DW_SR_MII_PCS_TP_A2		0x0090
#define DW_SR_MII_PCS_TP_A3		0x0094
#define DW_SR_MII_PCS_TP_B0		0x0098
#define DW_SR_MII_PCS_TP_B1		0x009c
#define DW_SR_MII_PCS_TP_B2		0x00a0
#define DW_SR_MII_PCS_TP_B3		0x00a4
#define DW_SR_MII_PCS_TP_CTRL		0x00a8
#define DW_SR_MII_PCS_TP_ERRCTR		0x00ac
#define DW_SR_MII_PCS_ABL		0x1c20
#define DW_SR_MII_PCS_TIME_SYNC_TX_MAX_DLY_LWR		0x1c24
#define DW_SR_MII_PCS_TIME_SYNC_TX_MAX_DLY_UPR		0x1c28
#define DW_SR_MII_PCS_TIME_SYNC_TX_MIN_DLY_LWR		0x1c2c
#define DW_SR_MII_PCS_TIME_SYNC_TX_MIN_DLY_UPR		0x1c30
#define DW_SR_MII_PCS_TIME_SYNC_RX_MAX_DLY_LWR		0x1c34
#define DW_SR_MII_PCS_TIME_SYNC_RX_MAX_DLY_UPR		0x1c38
#define DW_SR_MII_PCS_TIME_SYNC_RX_MIN_DLY_LWR		0x1c3c
#define DW_SR_MII_PCS_TIME_SYNC_RX_MIN_DLY_UPR		0x1c40
#define DW_VR_MII_PCS_DIG_CTRL1		0x2000
#define DW_VR_MII_PCS_DIG_ERRCNT_SEL	0x2008
#define DW_VR_MII_PCS_XAUI_CTRL		0x2010
#define DW_VR_MII_PCS_DEBUG_CTRL	0x2014
#define DW_VR_MII_PCS_KR_CTRL		0x201c
#define DW_VR_MII_PCS_EEE_TXTIMER	0x2020
#define DW_VR_MII_PCS_EEE_RXTIMER	0x2024
#define DW_VR_MII_PCS_ICG_ERRCNT1	0x2044
#define DW_VR_MII_PCS_GPIO		0x2054
#define DW_SR_MII_VSMMD_PMA_ID1		0x3000
#define DW_SR_MII_VSMMD_PMA_ID2		0x3004
#define DW_SR_MII_VSMMD_DEV_ID1		0x3008
#define DW_SR_MII_VSMMD_DEV_ID2		0x300c
#define DW_SR_MII_VSMMD_PCS_ID1		0x3010
#define DW_SR_MII_VSMMD_PCS_ID2		0x3014
#define DW_SR_MII_VSMMD_AN_ID1		0x3018
#define DW_SR_MII_VSMMD_AN_ID2		0x301c
#define DW_SR_MII_VSMMD_MMD_STS		0x3020
#define DW_SR_MII_VSMMD_CTRL		0x3024
#define DW_SR_MII_VSMMD_PKGID1		0x3038
#define DW_SR_MII_VSMMD_PKGID2		0x303c
#define DW_SR_MII_MMD_STS		0x4004
#define DW_SR_MII_MMD_DEV_ID1		0x4008
#define DW_SR_MII_MMD_DEV_ID2		0x400c
#define DW_SR_MII_AN_ADV		0x4010
#define DW_SR_MII_LP_BABL		0x4014
#define DW_SR_MII_AN_EXPN		0x4018
#define DW_SR_MII_EXT_STS		0x403c
#define DW_VR_MII_LINK_TIMER_CTRL	0x5028

/* XPCS power-down configuration registers */
#define PD_CTRL					BIT(5)
#define LPM_EN					BIT(11)

/* VR_MII_DIG_CTRL1 */
#define DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW		BIT(9)
#define DW_VR_MII_DIG_CTRL1_PHY_MODE_CTRL	BIT(0)

/* VR_MII_DIG_CTRL2 */
#define DW_VR_MII_DIG_CTRL2_TX_POL_INV		BIT(4)
#define DW_VR_MII_DIG_CTRL2_RX_POL_INV		BIT(0)

/* VR_MII_AN_CTRL */
#define DW_VR_MII_AN_CTRL_TX_CONFIG_SHIFT	3
#define DW_VR_MII_TX_CONFIG_MASK		BIT(3)
#define DW_VR_MII_TX_CONFIG_PHY_SIDE		0x1
#define DW_VR_MII_TX_CONFIG_MAC_SIDE		0x0
#define DW_VR_MII_AN_CTRL_PCS_MODE_SHIFT	1
#define DW_VR_MII_PCS_MODE_MASK			GENMASK(2, 1)
#define DW_VR_MII_PCS_MODE_C37_1000BASEX	0x0
#define DW_VR_MII_PCS_MODE_C37_SGMII		BIT(2)
#define DW_VR_MII_PCS_MODE_C37_QSGMII		0x3
#define DW_VR_MII_AN_INTR_EN			BIT(0)
#define DW_VR_MII_SGMII_LINK_STS		BIT(4)
#define DW_VR_MII_CTRL					BIT(8)

/* VR_MII_AN_INTR_STS */
#define DW_VR_MII_AN_STS_C37_ANSGM_FD		BIT(1)
#define DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT	2
#define DW_VR_MII_AN_STS_C37_ANSGM_SP		GENMASK(3, 2)
#define DW_VR_MII_C37_ANSGM_SP_10		0x0
#define DW_VR_MII_C37_ANSGM_SP_100		0x1
#define DW_VR_MII_C37_ANSGM_SP_1000		0x2
#define DW_VR_MII_C37_ANSGM_SP_LNKSTS		BIT(4)
#define DW_VR_MII_USXG_ANSGM_FD			BIT(13)
#define DW_VR_MII_USXG_ANSGM_SP_SHIFT		10
#define DW_VR_MII_USXG_ANSGM_SP			GENMASK(12, 10)
#define DW_VR_MII_USXG_ANSGM_SP_10		0x0
#define DW_VR_MII_USXG_ANSGM_SP_100		0x1
#define DW_VR_MII_USXG_ANSGM_SP_1000		0x2
#define DW_VR_MII_USXG_ANSGM_SP_10G		0x3
#define DW_VR_MII_USXG_ANSGM_SP_2P5G		0x4
#define DW_VR_MII_USXG_ANSGM_SP_5G		0x5
#define DW_VR_MII_USXG_ANSGM_SP_LNKSTS		BIT(14)
#define DW_VR_MII_ANCMPLT_INTR			BIT(0)

/* Bit 15 is the Soft Reset field for various registers */
#define SOFT_RST				BIT(15)
#define SW_RST_BIT_STATUS			0x8000
#define USXG_RST_BIT_STATUS			0x0400

/* SR MII MMD Control defines */
#define AN_CL37_EN			BIT(12)	/* Enable Clause 37 auto-nego */

/* VR MII EEE Control 0 defines */
#define DW_VR_MII_EEE_LTX_EN			BIT(0)  /* LPI Tx Enable */
#define DW_VR_MII_EEE_LRX_EN			BIT(1)  /* LPI Rx Enable */
#define DW_VR_MII_EEE_TX_QUIET_EN		BIT(2)  /* Tx Quiet Enable */
#define DW_VR_MII_EEE_RX_QUIET_EN		BIT(3)  /* Rx Quiet Enable */
#define DW_VR_MII_EEE_TX_EN_CTRL		BIT(4)  /* Tx Control Enable */
#define DW_VR_MII_EEE_RX_EN_CTRL		BIT(7)  /* Rx Control Enable */

#define DW_VR_MII_EEE_MULT_FACT_100NS_SHIFT	8
#define DW_VR_MII_EEE_MULT_FACT_100NS		GENMASK(11, 8)

/* VR MII EEE Control 1 defines */
#define DW_VR_MII_EEE_TRN_LPI		BIT(0)	/* Transparent Mode Enable */

/* SR MII PCS Control 2 defines */
#define PCS_TYPE_SEL_10GBR			GENMASK(3, 0)
#define PCS_TYPE_SEL_10GBX			BIT(0)

/* SR MII PCS Control 2 defines */
#define PCS_TYPE_SEL_2500BASEX			GENMASK(3, 1)

/* SR MII PCS KR Control defines */
#define USXG_MODE_SEL				GENMASK(12, 10)
#define USXGMII_5G				BIT(10)
#define USXGMII_2P5G				BIT(11)

/* SR_MII_AN_ADV */
#define DW_SR_MII_AN_ADV_FD			BIT(5)
#define DW_SR_MII_AN_ADV_HD			BIT(6)
#define DW_SR_MII_STS_LINK_STS			BIT(2)

/* Required settings to enable 2.5G SGMII mode */
#define LINK_TIMER_1P6MS			0x07a1
#define CL37_TMR_OVR_RIDE			BIT(3)

/* EEE Ability register defines */
#define KREEE_SUPPORT				BIT(6)
#define KX4EEE_SUPPORT				BIT(5)
#define KXEEE_SUPPORT				BIT(4)

/* Tx Preemable Control for Debug Register */
#define TX_PMBL_CTRL				BIT(8)

struct xpcs_compat {
	const int *supported;
	const phy_interface_t *interface;
	int num_interfaces;
	int an_mode;
};
