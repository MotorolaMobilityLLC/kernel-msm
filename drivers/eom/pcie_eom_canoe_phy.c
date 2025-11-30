// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/delay.h>
#include <linux/eom_ioctl.h>
#include <linux/module.h>
#include <linux/phy_core.h>
#include <linux/slab.h>

#include "buffer_manager.h"
#include "eom_driver.h"
#include "pcie_eom_canoe_phy_reg.h"

#if IS_ENABLED(CONFIG_PCI_MSM_EOM)
static int msm_pcie_eom_init(struct eom_phy_device *phy, struct eom_lane *lane,
			      u32 is_positive_seq)
{
	u32 lanenum = lane->lane_num;
	int ret = -EINVAL;

	if (atomic_read(&g_eom_seq_stop) || atomic_read(&lane->eom_seq_stop))
		return 0;

	pr_info("RC%d EOM Initializing lanes\n", phy->index);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_TX0_RESET_GEN_MUXES + LANE_SIZE(lanenum), 0x3);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_CDR_RESET_OVERRIDE + LANE_SIZE(lanenum),
			    0xa);
	if (ret < 0)
		return ret;

	if (is_positive_seq)
		ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_EOM_CTRL1 + LANE_SIZE(lanenum),
				    0x28);
	else
		ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_EOM_CTRL1 + LANE_SIZE(lanenum),
				    0x38);

	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_EOM_CTRL2 + LANE_SIZE(lanenum), 0x08);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_AUX_CONTROL + LANE_SIZE(lanenum), 0x40);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL + LANE_SIZE(lanenum), 0xfc);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL2 + LANE_SIZE(lanenum), 0x80);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xea);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_AUXDATA_TB + LANE_SIZE(lanenum), 0x08);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_VERTICAL_CODE + LANE_SIZE(lanenum),
			    0x00);
	if (ret < 0)
		return ret;

	/* Delay to allow register value to take effect */
	ndelay(100);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xeb);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xef);
	if (ret < 0)
		return ret;

	/* Delay to allow register value to take effect */
	ndelay(100);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xeb);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL + LANE_SIZE(lanenum), 0xfc);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL + LANE_SIZE(lanenum), 0xf4);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xea);

	return ret;
}

static int msm_pcie_eom_process_eye_sample(struct eom_lane *lane, struct eom_phy_device *phy,
					u32 lanenum, u32 xcoord, u32 vthreshold, u32 ycoord,
					u32 dtime, u32 is_positive_seq)
{
	u32 temp_err_low, temp_err_high;
	u32 absolute_ycoord;
	u32 errorcntr;
	u32 xtmp;
	int ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_VERTICAL_CODE + LANE_SIZE(lanenum),
			    ycoord);
	if (ret < 0)
		return ret;

	xtmp = (xcoord | 0x40);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_AUX_CONTROL + LANE_SIZE(lanenum), xtmp);
	if (ret < 0)
		return ret;

	/* Delay to allow register value to take effect */
	ndelay(100);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xEB);
	if (ret < 0)
		return ret;

	/* Delay to allow register value to take effect */
	ndelay(100);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xEF);
	if (ret < 0)
		return ret;

	/* Delay to allow register value to take effect */
	ndelay(100);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xEB);
	if (ret < 0)
		return ret;

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL + LANE_SIZE(lanenum), 0xfc);
	if (ret < 0)
		return ret;

	/* Delay to allow register value to take effect */
	ndelay(100);

	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL + LANE_SIZE(lanenum), 0xf4);
	if (ret < 0)
		return ret;

	/* dwell for sufficient transactions to occur */
	msleep(dtime);

	ret = read_phy_reg(phy, PCIE_PHY_QSERDES_RX0_IA_ERROR_COUNTER_LOW + LANE_SIZE(lanenum),
			   &temp_err_low);
	if (ret < 0)
		return ret;

	ret = read_phy_reg(phy, PCIE_PHY_QSERDES_RX0_IA_ERROR_COUNTER_HIGH + LANE_SIZE(lanenum),
			   &temp_err_high);
	if (ret < 0)
		return ret;

	errorcntr = (temp_err_low & 0xff);
	errorcntr = errorcntr | ((temp_err_high & 0xff) << 8);
	absolute_ycoord = ycoord + (vthreshold * MAX_VERTICAL_THRESHOLD);
	struct eom_entry entry = { (xcoord > 31) ? xcoord - 64 : xcoord,
				   (is_positive_seq ? 1 : -1) * absolute_ycoord,
				   errorcntr };
	eom_buffer_write(lane->buffer, (char *)&entry, sizeof(entry));
	ret = write_phy_reg(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3 + LANE_SIZE(lanenum), 0xEA);
	if (ret < 0)
		return ret;

	return 0;
}

static int msm_pcie_eom_eye_seq(struct eom_lane *lane, u32 is_positive_seq, u32 lanenum)
{
	struct eom_phy_device *phy = lane->phy_dev;
	u32 vthreshold, ycoord;
	u32 dtime = lane->dwell_time_us / 1000;
	int ret = -EINVAL;
	u32 xcoord = 0;
	u32 ytemp;

	vthreshold = 0;

	if (atomic_read(&g_eom_seq_stop) || atomic_read(&lane->eom_seq_stop))
		return 0;

	while (xcoord < MAX_EYE_WIDTH) {
		vthreshold = 0;
		while (vthreshold < MAX_VERTICAL_THRESHOLD) {
			ycoord = 0;
			ytemp = vthreshold | 0x08;
			ret = write_phy_reg(phy,
					    PCIE_PHY_QSERDES_RX0_AUXDATA_TB +
						    LANE_SIZE(lanenum),
					    ytemp);
			if (ret < 0)
				return ret;

			while (ycoord < MAX_EYE_HEIGHT) {
				ret = msm_pcie_eom_process_eye_sample(lane, phy, lanenum, xcoord,
								   vthreshold, ycoord, dtime,
								   is_positive_seq);
				if (ret < 0)
					return ret;

				ycoord = ycoord + 1;
				if (atomic_read(&g_eom_seq_stop) ||
				    atomic_read(&lane->eom_seq_stop)) {
					break;
				}
			}
			vthreshold++;
			if (atomic_read(&g_eom_seq_stop) || atomic_read(&lane->eom_seq_stop))
				break;
		}
		xcoord = xcoord + 1;
		if (atomic_read(&g_eom_seq_stop) || atomic_read(&lane->eom_seq_stop))
			break;
	}

	return 0;
}

/**
 * phy_pcie_eom_sequence - Canoe PCIe PHY EOM sequence implementation
 * @lane: EOM lane structure
 *
 * This strong symbol overrides the weak fallback in eom_driver.c when
 * CONFIG_ARCH_CANOE is enabled, providing the Canoe-specific implementation.
 *
 * Return: 0 on success, negative error code on failure
 */
int phy_pcie_eom_sequence(struct eom_lane *lane)
{
	struct eom_phy_device *phy = lane->phy_dev;
	int ret = 0;

	pr_info("Running Canoe PCIe EOM for %s instance %u lane %d\n",
		lane->seq->name, lane->phy_dev->index, lane->lane_num);

	pr_info("Initializing Phy and running EOM for positive sequence\n");
	ret = msm_pcie_eom_init(phy, lane, true);
	if (ret < 0)
		return ret;

	ret = msm_pcie_eom_eye_seq(lane, 1, lane->lane_num);
	if (ret < 0)
		return ret;

	if (atomic_read(&g_eom_seq_stop) || atomic_read(&lane->eom_seq_stop))
		return 0;

	/* Wait for some time rerun EOM for negative sequence */
	ndelay(1000);

	pr_info("Initializing Phy and running EOM for negative sequence\n");
	ret = msm_pcie_eom_init(phy, lane, false);
	if (ret < 0)
		return ret;

	ret = msm_pcie_eom_eye_seq(lane, 0, lane->lane_num);
	if (ret < 0)
		return ret;

	return 0;
}
#endif /* CONFIG_PCI_MSM_EOM */
