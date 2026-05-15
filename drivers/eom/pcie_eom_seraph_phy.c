// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/delay.h>
#include <linux/eom_ioctl.h>
#include <linux/module.h>
#include <linux/phy_core.h>
#include <linux/slab.h>

#include "buffer_manager.h"
#include "eom_driver.h"
#include "pcie_eom_seraph_phy_reg.h"

#if IS_ENABLED(CONFIG_PCI_MSM_EOM)

/**
 * seraph_phy_pcie_eom_init - Initialize PCIe EOM sequence for seraph PHY type
 * @phy: EOM PHY device structure
 * @lane: EOM lane structure
 *
 * This function initializes the PCIe PHY registers for EOM testing
 * based on the CMM script sequence. Now supports multiple RC indexes.
 *
 * Return: 0 on success, negative error code on failure
 */
static int seraph_phy_pcie_eom_init(struct eom_phy_device *phy, struct eom_lane *lane)
{
	u32 lanenum = lane->lane_num;
	u32 rc_idx = phy->index;  /* RC index from PHY device */
	u32 reg_addr;
	int ret = 0;

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	pr_info("PHY RC%d EOM Initializing lane %d\n", rc_idx, lanenum);

	/* Validate RC index */
	if (rc_idx >= SERAPH_PHY_MAX_RC_INSTANCES) {
		pr_err("Invalid RC index %d, max supported: %d\n",
		       rc_idx, SERAPH_PHY_MAX_RC_INSTANCES - 1);
		return -EINVAL;
	}

	/* Initialize TX Lane Mode 1 */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_LANE_MODE_1_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000004);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_LANE_MODE_1\n", rc_idx);
		return ret;
	}

	/* Initialize TX Lane Mode 5 */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_LANE_MODE_5_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000008);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_LANE_MODE_5\n", rc_idx);
		return ret;
	}

	/* Initialize RX AUX Control */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000000);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RX_AUX_CONTROL\n", rc_idx);
		return ret;
	}

	/* Initialize RX DFE 4 */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_DFE_4_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000000);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RX_DFE_4\n", rc_idx);
		return ret;
	}

	/* Initialize AUX DATA TCOARSE TFINE */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_DATA_TCOARSE_TFINE_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000000);
	if (ret < 0) {
		pr_err("RC%d: Failed to write AUX_DATA_TCOARSE_TFINE\n", rc_idx);
		return ret;
	}

	/* Initialize VTH CODE */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_VTH_CODE_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000000);
	if (ret < 0) {
		pr_err("RC%d: Failed to write VTH_CODE\n", rc_idx);
		return ret;
	}

	/* Initialize RCLK AUXDATA SEL */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_RCLK_AUXDATA_SEL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x000000FC);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RCLK_AUXDATA_SEL\n", rc_idx);
		return ret;
	}

	ndelay(100);

	/* Reset TX Reset Gen */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_RESET_GEN_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x0);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_RESET_GEN\n", rc_idx);
		return ret;
	}

	ndelay(100);

	/* Initialize CDR Reset Override */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_CDR_RESET_OVERRIDE_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x0000000A);
	if (ret < 0) {
		pr_err("RC%d: Failed to write CDR_RESET_OVERRIDE\n", rc_idx);
		return ret;
	}

	ndelay(100);

	/* Set AUX Control */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000040);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RX_AUX_CONTROL final\n", rc_idx);
		return ret;
	}

	/* TX Lane Mode 5 sequence */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_LANE_MODE_5_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000018);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_LANE_MODE_5 step 1\n", rc_idx);
		return ret;
	}

	ndelay(100);

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_LANE_MODE_5_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000008);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_LANE_MODE_5 step 2\n", rc_idx);
		return ret;
	}

	/* RCLK AUXDATA SEL sequence */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_RCLK_AUXDATA_SEL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x000000FC);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RCLK_AUXDATA_SEL step 1\n", rc_idx);
		return ret;
	}

	ndelay(100);

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_RCLK_AUXDATA_SEL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x000000F4);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RCLK_AUXDATA_SEL step 2\n", rc_idx);
		return ret;
	}

	/* Reset AUX Control */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000000);
	if (ret < 0) {
		pr_err("RC%d: Failed to reset RX_AUX_CONTROL\n", rc_idx);
		return ret;
	}

	/* Wait for stabilization */
	usleep_range(10000, 11000);

	/* Read error counters to clear them */
	u32 temp_err_low, temp_err_high;

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_IA_ERROR_COUNTER_LOW_ADDR, lanenum);
	ret = phy_read(phy, reg_addr, &temp_err_low);
	if (ret < 0) {
		pr_err("RC%d: Failed to read error counter low\n", rc_idx);
		return ret;
	}

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_IA_ERROR_COUNTER_HIGH_ADDR, lanenum);
	ret = phy_read(phy, reg_addr, &temp_err_high);
	if (ret < 0) {
		pr_err("RC%d: Failed to read error counter high\n", rc_idx);
		return ret;
	}

	usleep_range(1000, 1100);

	pr_info("PHY RC%d EOM Initialization completed for lane %d\n", rc_idx, lanenum);
	return 0;
}

/**
 * seraph_phy_pcie_eom_process_eye_sample - Process a single eye sample point
 * @lane: EOM lane structure
 * @phy: EOM PHY device structure
 * @lanenum: Lane number
 * @xcoord: X coordinate (horizontal offset)
 * @ycoord: Y coordinate (vertical threshold)
 * @tcoarse: Coarse timing adjustment
 * @dtime: Dwell time in milliseconds
 *
 * This function processes a single point in the eye diagram by setting
 * the appropriate registers and collecting error count data.
 * Now supports multiple RC indexes.
 *
 * Return: 0 on success, negative error code on failure
 */
static int seraph_phy_pcie_eom_process_eye_sample(struct eom_lane *lane,
						  struct eom_phy_device *phy,
						  u32 lanenum, u32 xcoord, u32 ycoord,
						  u32 tcoarse, u32 dtime)
{
	u32 temp_err_low, temp_err_high;
	u32 errorcntr;
	u32 horizontal_offset;
	u32 reg_addr;
	u32 rc_idx = phy->index;  /* RC index from PHY device */
	int ret;

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	/* Set vertical threshold (VTH_CODE) */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_VTH_CODE_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, ycoord & 0x1F);
	if (ret < 0) {
		pr_err("RC%d: Failed to write VTH_CODE\n", rc_idx);
		return ret;
	}

	ndelay(100);

	/* Set horizontal offset in AUX_CONTROL */
	horizontal_offset = xcoord;
	reg_addr =  SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, horizontal_offset ^ 0x20);
	if (ret < 0) {
		pr_err("RC%d: Failed to write AUX_CONTROL step 1\n", rc_idx);
		return ret;
	}

	ndelay(100);

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, (horizontal_offset ^ 0x20) | 0x40);
	if (ret < 0) {
		pr_err("RC%d: Failed to write AUX_CONTROL step 2\n", rc_idx);
		return ret;
	}

	ndelay(100);

	/* TX Lane Mode 5 sequence for error counter reset */
	reg_addr =  SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_LANE_MODE_5_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000018);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_LANE_MODE_5 reset step 1\n", rc_idx);
		return ret;
	}

	ndelay(100);

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_LANE_MODE_5_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x00000008);
	if (ret < 0) {
		pr_err("RC%d: Failed to write TX_LANE_MODE_5 reset step 2\n", rc_idx);
		return ret;
	}

	/* RCLK AUXDATA SEL sequence */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_RCLK_AUXDATA_SEL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x000000FC);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RCLK_AUXDATA_SEL measurement step 1\n", rc_idx);
		return ret;
	}

	ndelay(100);

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_RCLK_AUXDATA_SEL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, 0x000000F4);
	if (ret < 0) {
		pr_err("RC%d: Failed to write RCLK_AUXDATA_SEL measurement step 2\n", rc_idx);
		return ret;
	}

	/* Clear AUX_CONTROL bit 6 */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_RX_AUX_CONTROL_ADDR, lanenum);
	ret = phy_write(phy, reg_addr, (horizontal_offset ^ 0x20) & (~0x40));
	if (ret < 0) {
		pr_err("RC%d: Failed to clear AUX_CONTROL bit 6\n", rc_idx);
		return ret;
	}

	/* Dwell time for error accumulation */
	msleep(dtime);

	/* Read error counters */
	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_IA_ERROR_COUNTER_LOW_ADDR, lanenum);
	ret = phy_read(phy, reg_addr, &temp_err_low);
	if (ret < 0) {
		pr_err("RC%d: Failed to read error counter low\n", rc_idx);
		return ret;
	}

	reg_addr = SERAPH_PHY_REG_ADDR(HWIO_PCIE_QSERDES_TX_IA_ERROR_COUNTER_HIGH_ADDR, lanenum);
	ret = phy_read(phy, reg_addr, &temp_err_high);
	if (ret < 0) {
		pr_err("RC%d: Failed to read error counter high\n", rc_idx);
		return ret;
	}

	/* Combine error counters */
	errorcntr = (temp_err_low & 0xff);
	errorcntr = errorcntr | ((temp_err_high & 0xff) << 8);

	/* Store the sample in buffer */
	struct eom_entry entry = {
		.x = xcoord,
		.y = ycoord + (tcoarse * SERAPH_PHY_MAX_EYE_HEIGHT),
		.error_count = errorcntr
	};

	eom_buffer_write(lane->buffer, (char *)&entry, sizeof(entry));

	pr_debug("RC%d Lane:%d, xcoord=%d, ycoord=%d, tcoarse=%d, errors=%d\n",
		 rc_idx, lanenum, xcoord, ycoord, tcoarse, errorcntr);

	return 0;
}

/**
 * seraph_phy_pcie_eom_eye_seq - Execute the eye diagram sequence
 * @lane: EOM lane structure
 * @lanenum: Lane number
 *
 * This function executes the complete eye diagram measurement sequence
 * based on the CMM script logic with tcoarse, ycoord, and xcoord loops.
 * Now supports multiple RC indexes.
 *
 * Return: 0 on success, negative error code on failure
 */
static int seraph_phy_pcie_eom_eye_seq(struct eom_lane *lane, u32 lanenum)
{
	struct eom_phy_device *phy = lane->phy_dev;
	u32 tcoarse, ycoord, xcoord;
	u32 dtime = lane->dwell_time_us / 1000; /* Convert to milliseconds */
	u32 rc_idx = phy->index;  /* RC index from PHY device */
	u32 reg_addr;
	int ret = 0;

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	pr_info("Starting PHY RC%d EOM eye sequence for lane %d\n", rc_idx, lanenum);

	/* Main measurement loops - following CMM script structure */
	for (tcoarse = 0; tcoarse < SERAPH_PHY_MAX_TCOARSE; tcoarse += SERAPH_PHY_TCOARSE_STEP) {

		if (atomic_read(&lane->eom_seq_stop))
			break;

		/* Set tcoarse value in AUX_DATA_TCOARSE_TFINE register */
		reg_addr = SERAPH_PHY_REG_ADDR(
				HWIO_PCIE_QSERDES_RX_AUX_DATA_TCOARSE_TFINE_ADDR, lanenum);
		ret = phy_write(phy, reg_addr, ((tcoarse & 0x7) << 0x4));
		if (ret < 0) {
			pr_err("RC%d: Failed to write tcoarse value %d\n", rc_idx, tcoarse);
			return ret;
		}

		for (ycoord = 0; ycoord < SERAPH_PHY_MAX_EYE_HEIGHT; ycoord++) {

			if (atomic_read(&lane->eom_seq_stop))
				break;

			for (xcoord = 0; xcoord < SERAPH_PHY_MAX_EYE_WIDTH; xcoord++) {

				if (atomic_read(&lane->eom_seq_stop))
					break;

				ret = seraph_phy_pcie_eom_process_eye_sample(
					lane, phy, lanenum, xcoord, ycoord, tcoarse, dtime);

				if (ret < 0) {
					pr_err("RC%d: Failed for eye sample at x=%d, y=%d, tcoarse=%d\n",
					       rc_idx, xcoord, ycoord, tcoarse);
					return ret;
				}
			}
		}
	}

	pr_info("Completed PHY RC%d EOM eye sequence for lane %d\n", rc_idx, lanenum);
	return 0;
}

/**
 * phy_pcie_eom_sequence - Seraph PCIe PHY EOM sequence implementation
 * @lane: EOM lane structure
 *
 * This strong symbol overrides the weak fallback in eom_driver.c when
 * CONFIG_ARCH_SERAPH is enabled, providing the Seraph-specific implementation.
 * Supports multiple RC indexes.
 *
 * Return: 0 on success, negative error code on failure
 */
int phy_pcie_eom_sequence(struct eom_lane *lane)
{
	struct eom_phy_device *phy = lane->phy_dev;
	u32 rc_idx = phy->index;  /* RC index from PHY device */
	int ret = 0;

	pr_info("Running Seraph PHY PCIe EOM for RC%u lane %d\n", rc_idx, lane->lane_num);

	/* Validate RC index */
	if (rc_idx >= SERAPH_PHY_MAX_RC_INSTANCES) {
		pr_err("Invalid RC index %d, max supported: %d\n",
		       rc_idx, SERAPH_PHY_MAX_RC_INSTANCES - 1);
		return -EINVAL;
	}

	/* Initialize the PHY for EOM testing */
	ret = seraph_phy_pcie_eom_init(phy, lane);
	if (ret < 0) {
		pr_err("PHY RC%d EOM initialization failed: %d\n", rc_idx, ret);
		return ret;
	}

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	/* Execute the eye diagram sequence */
	ret = seraph_phy_pcie_eom_eye_seq(lane, lane->lane_num);
	if (ret < 0) {
		pr_err("PHY RC%d EOM eye sequence failed: %d\n", rc_idx, ret);
		return ret;
	}

	pr_info("PHY RC%d PCIe EOM sequence completed successfully\n", rc_idx);
	return 0;
}
#endif /* CONFIG_PCI_MSM_EOM */
