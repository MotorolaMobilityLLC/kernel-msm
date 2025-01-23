// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Synopsys DesignWare XPCS platform device driver
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pcs-xpcs-qcom.h>
#include <linux/platform_device.h>
#include <linux/mdio.h>
#include <linux/phylink.h>
#include <linux/workqueue.h>
#include "pcs-xpcs-qcom.h"

#define phylink_pcs_to_xpcs(pl_pcs) \
	container_of((pl_pcs), struct dw_xpcs_qcom, pcs)

#define DRV_NAME "qcom-xpcs"

#define XPCSINFO(fmt, args...) \
	pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define XPCSERR(fmt, args...) \
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

phy_interface_t g_interface;
static const int xpcs_usxgmii_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseX_Full_BIT,
	ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
	ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const int xpcs_10gbaser_features[] = {
	ETHTOOL_LINK_MODE_Pause_BIT,
	ETHTOOL_LINK_MODE_Asym_Pause_BIT,
	ETHTOOL_LINK_MODE_10000baseSR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseLR_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseER_Full_BIT,
	__ETHTOOL_LINK_MODE_MASK_NBITS,
};

static const phy_interface_t xpcs_usxgmii_interfaces[] = {
	PHY_INTERFACE_MODE_USXGMII,
};

static const phy_interface_t xpcs_10gbaser_interfaces[] = {
	PHY_INTERFACE_MODE_10GBASER,
};

enum {
	DW_XPCS_USXGMII,
	DW_XPCS_10GBASER,
	DW_XPCS_INTERFACE_MAX,
};

struct xpcs_id {
	u32 id;
	u32 mask;
	const struct xpcs_compat *compat;
};

const struct xpcs_compat *xpcs_find_compat(const struct xpcs_id *id,
					   phy_interface_t interface)
{
	int i, j;

	for (i = 0; i < DW_XPCS_INTERFACE_MAX; i++) {
		const struct xpcs_compat *compat = &id->compat[i];

		for (j = 0; j < compat->num_interfaces; j++)
			if (compat->interface[j] == interface)
				return compat;
	}

	return NULL;
}

int qcom_xpcs_get_an_mode(struct dw_xpcs_qcom *qxpcs, phy_interface_t interface)
{
	const struct xpcs_compat *compat;

	compat = xpcs_find_compat(qxpcs->id, interface);
	if (!compat)
		return -ENODEV;

	return compat->an_mode;
}

static int qcom_xpcs_read(struct dw_xpcs_qcom *qxpcs, u32 reg)
{
	return readl(qxpcs->addr + reg);
}

static void qcom_xpcs_write(struct dw_xpcs_qcom *qxpcs, u32 reg, u16 val)
{
	writel(val, qxpcs->addr + reg);
}

static int qcom_xpcs_poll_reset(struct dw_xpcs_qcom *qxpcs, unsigned int offset,
				unsigned int field)
{
	unsigned int retries = 32;
	int ret;

	do {
		usleep_range(1000, 2000);
		ret = qcom_xpcs_read(qxpcs, offset);
		if (ret < 0)
			return ret;
	} while (ret & field && --retries);

	return (ret & field) ? -ETIMEDOUT : 0;
}

static int xpcs_soft_reset(struct dw_xpcs_qcom *qxpcs,
			   const struct xpcs_compat *compat)
{
	int ret;

	ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_CTRL1);
	if (ret < 0)
		return ret;

	qcom_xpcs_write(qxpcs, DW_SR_MII_PCS_CTRL1, ret | SOFT_RST);

	ret = qcom_xpcs_poll_reset(qxpcs, DW_SR_MII_PCS_CTRL1, SW_RST_BIT_STATUS);

	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_DIG_CTRL1);
	if (ret < 0)
		return ret;

	qcom_xpcs_write(qxpcs, DW_VR_MII_DIG_CTRL1, ret | SOFT_RST);

	return qcom_xpcs_poll_reset(qxpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);
}

static int qcom_xpcs_validate(struct phylink_pcs *pcs, unsigned long *supported,
			      const struct phylink_link_state *state)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(xpcs_supported);
	struct dw_xpcs_qcom *qxpcs = phylink_pcs_to_xpcs(pcs);
	const struct xpcs_compat *compat;
	int i;

	/* phylink expects us to report all supported modes with
	 * PHY_INTERFACE_MODE_NA, just don't limit the supported and
	 * advertising masks and exit.
	 */
	if (state->interface == PHY_INTERFACE_MODE_NA)
		return -EINVAL;

	bitmap_zero(xpcs_supported, __ETHTOOL_LINK_MODE_MASK_NBITS);

	compat = xpcs_find_compat(qxpcs->id, state->interface);

	/* Populate the supported link modes for this
	 * PHY interface type
	 */
	if (compat)
		for (i = 0; compat->supported[i] != __ETHTOOL_LINK_MODE_MASK_NBITS; i++)
			set_bit(compat->supported[i], xpcs_supported);

	linkmode_and(supported, supported, xpcs_supported);

	return 0;
}

/* Cannot sleep in interrupt-context, increase retries and remove usleep call. */
static int qcom_xpcs_poll_reset_usxgmii(struct dw_xpcs_qcom *qxpcs, unsigned int offset,
					unsigned int field)
{
	unsigned int retries = 1000;
	int ret;

	do {
		ret = qcom_xpcs_read(qxpcs, offset);
		if (ret < 0)
			return ret;
	} while (ret & field && --retries);

	return (ret & field) ? -ETIMEDOUT : 0;
}

/* Reset PCS and USXGMII Rate Adaptor Logic*/
static int qcom_xpcs_reset_usxgmii(struct dw_xpcs_qcom *qxpcs)
{
	int ret;

	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_DIG_CTRL1);
	if (ret < 0)
		return ret;

	qcom_xpcs_write(qxpcs, DW_VR_MII_DIG_CTRL1, ret | SOFT_RST);

	if (!qxpcs->intr_en)
		ret = qcom_xpcs_poll_reset(qxpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);
	else
		ret = qcom_xpcs_poll_reset_usxgmii(qxpcs, DW_VR_MII_DIG_CTRL1, SW_RST_BIT_STATUS);

	if (ret < 0)
		return ret;

	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_PCS_DIG_CTRL1);
	if (ret < 0)
		return ret;

	qcom_xpcs_write(qxpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_RST);

	if (!qxpcs->intr_en)
		return qcom_xpcs_poll_reset(qxpcs, DW_VR_MII_PCS_DIG_CTRL1, USXG_RST_BIT_STATUS);

	return qcom_xpcs_poll_reset_usxgmii(qxpcs, DW_VR_MII_PCS_DIG_CTRL1, USXG_RST_BIT_STATUS);
}

static int qcom_xpcs_usxgmii_read_intr_status(struct dw_xpcs_qcom *qxpcs)
{
	int intr_stat;

	intr_stat = qcom_xpcs_read(qxpcs, DW_VR_MII_AN_INTR_STS);
	if (intr_stat < 0)
		return intr_stat;

	if (intr_stat & DW_VR_MII_USXG_ANSGM_SP_LNKSTS) {
		int mmd_ctrl, speed;

		mmd_ctrl = qcom_xpcs_read(qxpcs, DW_SR_MII_MMD_CTRL);
		if (mmd_ctrl < 0)
			return mmd_ctrl;

		mmd_ctrl &= ~DW_USXGMII_SS_MASK;
		speed = (intr_stat & DW_VR_MII_USXG_ANSGM_SP) >> DW_VR_MII_USXG_ANSGM_SP_SHIFT;

		switch (speed) {
		case DW_VR_MII_USXG_ANSGM_SP_10G:
			mmd_ctrl |= DW_USXGMII_10000;
			XPCSINFO("10Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_5G:
			mmd_ctrl |= DW_USXGMII_5000;
			XPCSINFO("5Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_2P5G:
			mmd_ctrl |= DW_GMII_2500;
			XPCSINFO("2.5Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_1000:
			mmd_ctrl |= DW_GMII_1000;
			XPCSINFO("1Gbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_100:
			mmd_ctrl |= DW_GMII_100;
			XPCSINFO("100Mbps-USXGMII enabled\n");
			break;
		case DW_VR_MII_USXG_ANSGM_SP_10:
			XPCSINFO("10Mbps-USXGMII enabled\n");
			break;
		default:
			XPCSERR("Invalid speed mode: %d\n", speed);
			return -EINVAL;
		}

		qcom_xpcs_write(qxpcs, DW_SR_MII_MMD_CTRL, mmd_ctrl);

		mmd_ctrl = qcom_xpcs_reset_usxgmii(qxpcs);
		if (mmd_ctrl < 0)
			XPCSERR("Failed to reset USXGMII\n");

		return mmd_ctrl;
	}
		XPCSERR("Link is down, aborting\n");
		return 0;
}

static void qcom_xpcs_handle_an_intr(struct dw_xpcs_qcom *qxpcs,
				     phy_interface_t interface)
{
	int ret = 0;

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		ret = qcom_xpcs_usxgmii_read_intr_status(qxpcs);
		if (ret < 0)
			goto out;

		XPCSINFO("Finished Autonegotiation for USXGMII\n");
		return;
	default:
		XPCSERR("Invalid MII mode for Autonegotiation\n");
		goto out;
	}

out:
	XPCSERR("Failed to handle Autonegotiation interrupt\n");
}

int qcom_xpcs_check_aneg_ioc(struct dw_xpcs_qcom *qxpcs, phy_interface_t interface)
{
	int ret;

	/* Check if interrupt was set */
	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_AN_INTR_STS);
	if (ret < 0)
		return -EINVAL;

	if (!(ret & DW_VR_MII_ANCMPLT_INTR)) {
		XPCSERR("Autonegotiation is still not finished\n");
		return -ENODEV;
	}

	/* Clear the IOC status */
	ret &= ~DW_VR_MII_ANCMPLT_INTR;
	qcom_xpcs_write(qxpcs, DW_VR_MII_AN_INTR_STS, ret);

	qcom_xpcs_handle_an_intr(qxpcs, interface);
	return 0;
}

/* By default, enable Clause 37 autonegotiation */
static int xpcs_config_aneg_c37(struct dw_xpcs_qcom *qxpcs)
{
	int ret;

	ret = qcom_xpcs_read(qxpcs, DW_SR_MII_MMD_CTRL);
	if (ret < 0)
		return -EINVAL;

	ret |= AN_CL37_EN;
	qcom_xpcs_write(qxpcs, DW_SR_MII_MMD_CTRL, ret);

	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0)
		return -EINVAL;

	ret |= DW_VR_MII_TX_CONFIG_MASK;
	ret |= DW_VR_MII_SGMII_LINK_STS;

	qcom_xpcs_write(qxpcs, DW_VR_MII_AN_CTRL, ret);

	return 0;
}

static int qcom_xpcs_do_config(struct dw_xpcs_qcom *qxpcs, phy_interface_t interface)
{
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(qxpcs->id, interface);
	if (!compat)
		return -ENODEV;

	switch (compat->an_mode) {
	case DW_AN_C37_USXGMII:
		ret = xpcs_config_aneg_c37(qxpcs);
		if (ret < 0)
			return ret;
		break;
	case DW_10GBASER:
		break;
	default:
		XPCSERR("Incompatible Autonegotiation mode\n");
		return -EINVAL;
	}

	return 0;
}

static int qcom_xpcs_config(struct phylink_pcs *pcs, unsigned int mode, phy_interface_t interface,
			    const unsigned long *advertising,
			    bool permit_pause_to_mac)
{
	struct dw_xpcs_qcom *qxpcs = phylink_pcs_to_xpcs(pcs);

	return qcom_xpcs_do_config(qxpcs, interface);
}

static int xpcs_get_state_c37_usxgmii(struct dw_xpcs_qcom *qxpcs,
				      struct phylink_link_state *state)
{
	int ret;

	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_AN_INTR_STS);
	if (ret < 0)
		return -ENODEV;

	if (ret & DW_VR_MII_USXG_ANSGM_SP_LNKSTS)
		state->link = true;
	else
		state->link = false;

	return 0;
}

static void qcom_xpcs_get_state(struct phylink_pcs *pcs,
				struct phylink_link_state *state)
{
	struct dw_xpcs_qcom *qxpcs = phylink_pcs_to_xpcs(pcs);
	const struct xpcs_compat *compat;
	int ret;

	compat = xpcs_find_compat(qxpcs->id, state->interface);
	if (!compat)
		return;

	switch (compat->an_mode) {
	case DW_AN_C37_USXGMII:
		ret = xpcs_get_state_c37_usxgmii(qxpcs, state);
		if (ret < 0)
			XPCSERR("Failed to get USXGMII state\n");
		break;
	default:
		return;
	}
}

void qcom_xpcs_link_up_usxgmii(struct dw_xpcs_qcom *qxpcs, int speed)
{
	int mmd_ctrl;

	mmd_ctrl = qcom_xpcs_read(qxpcs, DW_SR_MII_MMD_CTRL);
	if (mmd_ctrl < 0)
		goto out;

	mmd_ctrl &= ~DW_USXGMII_SS_MASK;

	switch (speed) {
	case SPEED_10000:
		mmd_ctrl |= DW_USXGMII_10000;
		XPCSINFO("10Gbps-USXGMII enabled\n");
		break;
	case SPEED_5000:
		mmd_ctrl |= DW_USXGMII_5000;
		XPCSINFO("5Gbps-USXGMII enabled\n");
		break;
	case SPEED_2500:
		mmd_ctrl |= DW_GMII_2500;
		XPCSINFO("2.5Gbps-USXGMII enabled\n");
		break;
	case SPEED_1000:
		mmd_ctrl |= DW_GMII_1000;
		XPCSINFO("1Gbps-USXGMII enabled\n");
		break;
	case SPEED_100:
		mmd_ctrl |= DW_GMII_100;
		XPCSINFO("100Mbps-USXGMII enabled\n");
		break;
	case SPEED_10:
		XPCSINFO("10Mbps-USXGMII enabled\n");
		break;
	default:
		XPCSERR("Invalid speed mode selected\n");
		return;
	}

	qcom_xpcs_write(qxpcs, DW_SR_MII_MMD_CTRL, mmd_ctrl);

	mmd_ctrl = qcom_xpcs_reset_usxgmii(qxpcs);
	if (mmd_ctrl < 0)
		goto out;

	XPCSINFO("USXGMII link is up\n");
	return;
out:
	XPCSERR("Failed to bring up USXGMII link\n");
}

static int qcom_xpcs_select_mode(struct dw_xpcs_qcom *qxpcs, phy_interface_t interface)
{
	int ret;

	g_interface = interface;

	if (interface == PHY_INTERFACE_MODE_USXGMII || interface == PHY_INTERFACE_MODE_10GBASER) {
		ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_CTRL2);
		if (ret < 0)
			goto out;

		ret &= ~PCS_TYPE_SEL_10GBR;
		qcom_xpcs_write(qxpcs, DW_SR_MII_PCS_CTRL2, ret);

		ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_CTRL1);
		if (ret < 0)
			goto out;

		qcom_xpcs_write(qxpcs, DW_SR_MII_PCS_CTRL1, ret | LPM_EN);

		usleep_range(1, 20);

		ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_CTRL1);
		if (ret < 0)
			goto out;

		ret &= ~LPM_EN;
		qcom_xpcs_write(qxpcs, DW_SR_MII_PCS_CTRL1, ret);

		ret = qcom_xpcs_read(qxpcs, DW_VR_MII_PCS_DIG_CTRL1);
		if (ret < 0)
			goto out;

		if (interface == PHY_INTERFACE_MODE_USXGMII)
			qcom_xpcs_write(qxpcs, DW_VR_MII_PCS_DIG_CTRL1, ret | DW_USXGMII_EN);

		return 0;
	}

	XPCSERR("Incompatible MII interface: %d\n", interface);
	return -EINVAL;

out:
	XPCSERR("Register read failed\n");
	return -EINVAL;
}

/* USXGMII: Return early if interrupt was enabled.
 * Autonegotiation ISR will set speed and duplex instead.
 * SGMII: For 2.5Gbps, let ISR do NOP since SGMII+ not supported in
 * registers for CL37. qcom_xpcs_link_up_sgmii will take care of
 * setting 2.5Gbps.
 */
void qcom_xpcs_link_up(struct phylink_pcs *pcs, phy_interface_t interface,
		       int speed, int duplex)
{
	struct dw_xpcs_qcom *qxpcs = phylink_pcs_to_xpcs(pcs);

	if (qxpcs->intr_en)
		return;

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		qcom_xpcs_link_up_usxgmii(qxpcs, speed);
		break;
	default:
		XPCSERR("Invalid MII mode: %s\n", phy_modes(interface));
		return;
	}
}
EXPORT_SYMBOL_GPL(qcom_xpcs_link_up);

static u32 xpcs_get_id(struct dw_xpcs_qcom *qxpcs)
{
	int ret;
	u32 id;

	/* Next, search C37 PCS using Vendor-Specific MII MMD */
	ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_DEV_ID1);
	if (ret < 0)
		return 0xffffffff;

	id = ret << 16;

	ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_DEV_ID2);
	if (ret < 0)
		return 0xffffffff;

	/* If Device IDs are not all zeros, we found C37 AN-type device */
	if (id | ret)
		return id | ret;

	return 0xffffffff;
}

static const struct xpcs_compat synopsys_xpcs_compat[DW_XPCS_INTERFACE_MAX] = {
	[DW_XPCS_USXGMII] = {
		.supported = xpcs_usxgmii_features,
		.interface = xpcs_usxgmii_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_usxgmii_interfaces),
		.an_mode = DW_AN_C37_USXGMII,
	},
	[DW_XPCS_10GBASER] = {
		.supported = xpcs_10gbaser_features,
		.interface = xpcs_10gbaser_interfaces,
		.num_interfaces = ARRAY_SIZE(xpcs_10gbaser_interfaces),
		.an_mode = DW_10GBASER,
	},
};

static const struct xpcs_id xpcs_id_list[] = {
	{
		.id = SYNOPSYS_XPCS_ID,
		.mask = SYNOPSYS_XPCS_MASK,
		.compat = synopsys_xpcs_compat,
	},
};

static const struct phylink_pcs_ops qcom_xpcs_phylink_ops = {
	.pcs_validate = qcom_xpcs_validate,
	.pcs_config = qcom_xpcs_config,
	.pcs_get_state = qcom_xpcs_get_state,
};

struct phylink_pcs *qcom_xpcs_create(struct device_node *np, phy_interface_t interface)
{
	const struct xpcs_compat *compat;
	struct platform_device *pdev;
	struct dw_xpcs_qcom *qxpcs;
	u32 xpcs_id;
	int i, ret = 0;

	pdev = of_find_device_by_node(np);
	of_node_put(np);

	if (!pdev || !platform_get_drvdata(pdev)) {
		if (pdev)
			put_device(&pdev->dev);

		return ERR_PTR(-EPROBE_DEFER);
	}

	qxpcs = platform_get_drvdata(pdev);

	qxpcs->phy_interface = interface;

	xpcs_id = xpcs_get_id(qxpcs);
	if (xpcs_id == 0xffffffff) {
		XPCSERR("Invalid qxpcs Device ID\n");
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(xpcs_id_list); i++) {
		const struct xpcs_id *entry = &xpcs_id_list[i];

		if ((xpcs_id & entry->mask) != entry->id)
			continue;

		qxpcs->id = entry;
		compat = xpcs_find_compat(entry, interface);
		if (!compat) {
			XPCSERR("Incompatible MII interface: %d\n", interface);
			ret = -ENODEV;
			goto out;
		}

		qxpcs->pcs.ops = &qcom_xpcs_phylink_ops;
		qxpcs->pcs.poll = true;

		ret = xpcs_soft_reset(qxpcs, compat);
		if (ret) {
			XPCSERR("Soft reset of XPCS block failed\n");
			goto out;
		}

		/* Select PCS mode before further configuration */
		ret = qcom_xpcs_select_mode(qxpcs, interface);
		if (ret < 0)
			goto out;

		return &qxpcs->pcs;
	}

out:
	XPCSERR("qxpcs creation failed\n");
	kfree(qxpcs);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_create);

/* Power off the DWC_xpcs controller */
void qcom_xpcs_destroy(struct dw_xpcs_qcom *qxpcs)
{
	int i, ret;
	u32 xpcs_id;
	const struct xpcs_compat *compat;

	xpcs_id = xpcs_get_id(qxpcs);
	if (xpcs_id == 0xffffffff) {
		XPCSERR("Invalid XPCS Device ID\n");
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(xpcs_id_list); i++) {
		const struct xpcs_id *entry = &xpcs_id_list[i];

		if ((xpcs_id & entry->mask) != entry->id)
			continue;

		qxpcs->id = entry;
		compat = xpcs_find_compat(entry, g_interface);
		if (!compat) {
			XPCSERR("Incompatible MII interface: %d\n", g_interface);
			ret = -ENODEV;
			goto out;
		}

		ret = xpcs_soft_reset(qxpcs, compat);
		if (ret) {
			XPCSERR("Soft reset of XPCS block failed\n");
			ret = -ENODEV;
		}
		goto done;
	}

	/* Enable xpc_spdown_o signal assertion on xpcs power down, then
	 * intiate the power down sequence
	 */
	ret = qcom_xpcs_read(qxpcs, DW_SR_MII_VSMMD_CTRL);
	if (ret < 0)
		goto out;

	qcom_xpcs_write(qxpcs, DW_SR_MII_VSMMD_CTRL, ret & ~PD_CTRL);

	ret = qcom_xpcs_read(qxpcs, DW_SR_MII_PCS_CTRL1);
	if (ret < 0)
		goto out;

	qcom_xpcs_write(qxpcs, DW_SR_MII_PCS_CTRL1, ret | LPM_EN);
	goto done;

out:
	XPCSERR("Could not power down the XPCS\n");
done:
	kfree(qxpcs);
}
EXPORT_SYMBOL_GPL(qcom_xpcs_destroy);

irqreturn_t qcom_xpcs_isr(int irq, void *dev_data)
{
	int ret;
	struct dw_xpcs_qcom *qxpcs = (struct dw_xpcs_qcom *)dev_data;

	ret = qcom_xpcs_check_aneg_ioc(qxpcs, qxpcs->phy_interface);
	if (ret < 0)
		return IRQ_HANDLED;

	return IRQ_HANDLED;
}

int qcom_xpcs_intr_enable(struct dw_xpcs_qcom *qxpcs)
{
	int ret = 0;

	ret = qcom_xpcs_read(qxpcs, DW_VR_MII_AN_CTRL);
	if (ret < 0) {
		XPCSERR("Failed to enable CL37 interrupts\n");
		return ret;
	}

	ret |= DW_VR_MII_AN_INTR_EN;

	qcom_xpcs_write(qxpcs, DW_VR_MII_AN_CTRL, ret);

	ret = request_irq(qxpcs->pcs_intr, qcom_xpcs_isr,
			  IRQF_SHARED, "qcom-xpcs", qxpcs);
	if (ret)
		pr_err("Unable to register XPCS IRQ %d\n",
		       qxpcs->pcs_intr);

	return ret;
}

static int qcom_xpcs_probe(struct platform_device *pdev)
{
	int ret;
	struct dw_xpcs_qcom *qxpcs;

	qxpcs = kzalloc(sizeof(*qxpcs), GFP_KERNEL);
	if (!qxpcs)
		return -ENOMEM;

	qxpcs->addr = devm_platform_ioremap_resource_byname(pdev, "qxpcs");
	if (IS_ERR_OR_NULL(qxpcs->addr)) {
		pr_err("XPCS not supported from device tree!\n");
		kfree(qxpcs);
		return -ENODEV;
	}

	qxpcs->pcs_intr = platform_get_irq_byname_optional(pdev, "pcs_intr");
	if (qxpcs->pcs_intr < 0) {
		pr_info("XPCS IRQ is not enabled\n");
	} else {
		ret = qcom_xpcs_intr_enable(qxpcs);
		if (ret)
			qxpcs->intr_en = false;
		else
			qxpcs->intr_en = true;
	}

	platform_set_drvdata(pdev, qxpcs);

	return 0;
}

static const struct of_device_id qcom_xpcs_match[] = {
	{ .compatible = "qcom,xpcs"},
	{}
};

static struct platform_driver pcs_xpcs_qcom_driver = {
	.probe = qcom_xpcs_probe,
	.driver = {
		.name = "dwxpcs",
		.of_match_table = of_match_ptr(qcom_xpcs_match),
	},
};
module_platform_driver(pcs_xpcs_qcom_driver);

MODULE_LICENSE("GPL");
