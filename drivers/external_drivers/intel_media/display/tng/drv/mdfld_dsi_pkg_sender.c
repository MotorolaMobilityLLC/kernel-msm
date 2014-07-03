/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Jackie Li<yaodong.li@intel.com>
 */

#include <linux/freezer.h>

#include "mdfld_dsi_output.h"
#include "mdfld_dsi_pkg_sender.h"
#include "mdfld_dsi_dbi_dsr.h"
#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_dpi.h"

#define MDFLD_DSI_DBI_FIFO_TIMEOUT		1000
#define MDFLD_DSI_MAX_RETURN_PACKET_SIZE	512
#define MDFLD_DSI_READ_MAX_COUNT		10000

const char *dsi_errors[] = {
	"[ 0:RX SOT Error]",
	"[ 1:RX SOT Sync Error]",
	"[ 2:RX EOT Sync Error]",
	"[ 3:RX Escape Mode Entry Error]",
	"[ 4:RX LP TX Sync Error",
	"[ 5:RX HS Receive Timeout Error]",
	"[ 6:RX False Control Error]",
	"[ 7:RX ECC Single Bit Error]",
	"[ 8:RX ECC Multibit Error]",
	"[ 9:RX Checksum Error]",
	"[10:RX DSI Data Type Not Recognised]",
	"[11:RX DSI VC ID Invalid]",
	"[12:TX False Control Error]",
	"[13:TX ECC Single Bit Error]",
	"[14:TX ECC Multibit Error]",
	"[15:TX Checksum Error]",
	"[16:TX DSI Data Type Not Recognised]",
	"[17:TX DSI VC ID invalid]",
	"[18:High Contention]",
	"[19:Low contention]",
	"[20:DPI FIFO Under run]",
	"[21:HS TX Timeout]",
	"[22:LP RX Timeout]",
	"[23:Turn Around ACK Timeout]",
	"[24:ACK With No Error]",
	"[25:RX Invalid TX Length]",
	"[26:RX Prot Violation]",
	"[27:HS Generic Write FIFO Full]",
	"[28:LP Generic Write FIFO Full]",
	"[29:Generic Read Data Avail]",
	"[30:Special Packet Sent]",
	"[31:Tearing Effect]",
};

static void debug_dbi_hang(struct mdfld_dsi_pkg_sender *sender) {
  struct mdfld_dsi_connector *dsi_connector = sender->dsi_connector;
  struct mdfld_dsi_config *dsi_config = (struct mdfld_dsi_config *)dsi_connector->private;
  struct drm_device *dev = sender->dev;
  struct drm_psb_private *dev_priv = (struct drm_psb_private *)dev->dev_private;
  bool pmon = ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND);

  DRM_ERROR("sender->pipe: 0x%08x\n", sender->pipe);
  DRM_ERROR("dev_priv->um_start: 0x%08x\n", dev_priv->um_start);
  DRM_ERROR("ospm_power_is_hw_on(OSPM_DISPLAY_ISLAND): 0x%08x\n", pmon);
  DRM_ERROR("dsi_config->dsi_hw_context.panel_on: 0x%08x\n", dsi_config->dsi_hw_context.panel_on);
	if (dsi_config->dsr) {
	  DRM_ERROR("dsi_config->dsr->dsr_enabled: 0x%08x\n", ((struct mdfld_dsi_dsr *)dsi_config->dsr)->dsr_enabled);
	  DRM_ERROR("dsi_config->dsr->dsr_state: 0x%08x\n", ((struct mdfld_dsi_dsr *)dsi_config->dsr)->dsr_state);
	}
	if (!pmon) {
	  /* Not safe to dump registers when the power is off */
	  return;
	}
  DRM_ERROR("dsi_config->regs.dspcntr_reg: 0x%08x\n", REG_READ(dsi_config->regs.dspcntr_reg));

  DRM_ERROR("MIPIA_DEVICE_READY_REG: 0x%08x\n", REG_READ(MIPIA_DEVICE_READY_REG));
  DRM_ERROR("MIPIA_DEVICE_READY_REG + MIPIC_REG_OFFSET: 0x%08x\n", REG_READ(MIPIA_DEVICE_READY_REG + MIPIC_REG_OFFSET));
  DRM_ERROR("sender->dpll_reg: 0x%08x\n", REG_READ(sender->dpll_reg));
  DRM_ERROR("sender->dspcntr_reg: 0x%08x\n", REG_READ(sender->dspcntr_reg));
  DRM_ERROR("sender->pipeconf_reg: 0x%08x\n", REG_READ(sender->pipeconf_reg));
  DRM_ERROR("sender->pipestat_reg: 0x%08x\n", REG_READ(sender->pipestat_reg));
  DRM_ERROR("sender->dsplinoff_reg: 0x%08x\n", REG_READ(sender->dsplinoff_reg));
  DRM_ERROR("sender->dspsurf_reg: 0x%08x\n", REG_READ(sender->dspsurf_reg));

  DRM_ERROR("sender->mipi_intr_stat_reg: 0x%08x\n", REG_READ(sender->mipi_intr_stat_reg));
  DRM_ERROR("sender->mipi_lp_gen_data_reg: 0x%08x\n", REG_READ(sender->mipi_lp_gen_data_reg));
  DRM_ERROR("sender->mipi_hs_gen_data_reg: 0x%08x\n", REG_READ(sender->mipi_hs_gen_data_reg));
  DRM_ERROR("sender->mipi_lp_gen_ctrl_reg: 0x%08x\n", REG_READ(sender->mipi_lp_gen_ctrl_reg));
  DRM_ERROR("sender->mipi_hs_gen_ctrl_reg: 0x%08x\n", REG_READ(sender->mipi_hs_gen_ctrl_reg));
  DRM_ERROR("sender->mipi_gen_fifo_stat_reg: 0x%08x\n", REG_READ(sender->mipi_gen_fifo_stat_reg));
  DRM_ERROR("sender->mipi_data_addr_reg: 0x%08x\n", REG_READ(sender->mipi_data_addr_reg));
  DRM_ERROR("sender->mipi_data_len_reg: 0x%08x\n", REG_READ(sender->mipi_data_len_reg));
  DRM_ERROR("sender->mipi_cmd_addr_reg: 0x%08x\n", REG_READ(sender->mipi_cmd_addr_reg));
  DRM_ERROR("sender->mipi_cmd_len_reg: 0x%08x\n", REG_READ(sender->mipi_cmd_len_reg));
  DRM_ERROR("sender->mipi_dpi_control_reg: 0x%08x\n", REG_READ(sender->mipi_dpi_control_reg));
}

static inline int wait_for_gen_fifo_empty(struct mdfld_dsi_pkg_sender *sender,
						u32 mask)
{
	struct drm_device *dev = sender->dev;
	u32 gen_fifo_stat_reg = sender->mipi_gen_fifo_stat_reg;
	int retry = 10000;

	if (sender->work_for_slave_panel)
		gen_fifo_stat_reg += MIPIC_REG_OFFSET;
	while (retry--) {
		if ((mask & REG_READ(gen_fifo_stat_reg)) == mask)
			return 0;
		udelay(3);
	}

	DRM_ERROR("fifo is NOT empty 0x%08x\n", REG_READ(gen_fifo_stat_reg));
	if (!IS_ANN(dev))
		debug_dbi_hang(sender);

	sender->status = MDFLD_DSI_CONTROL_ABNORMAL;
	return -EIO;
}

static int wait_for_all_fifos_empty(struct mdfld_dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender,
		(BIT2 | BIT10 | BIT18 | BIT26 | BIT27 | BIT28));
}

static int wait_for_lp_fifos_empty(struct mdfld_dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender, (BIT10 | BIT26));
}

static int wait_for_hs_fifos_empty(struct mdfld_dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender, (BIT2 | BIT18));
}

static int wait_for_dbi_fifo_empty(struct mdfld_dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender, (BIT27));
}

static int wait_for_dpi_fifo_empty(struct mdfld_dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender, (BIT28));
}

static int dsi_error_handler(struct mdfld_dsi_pkg_sender *sender)
{
	struct drm_device *dev = sender->dev;
	u32 intr_stat_reg = sender->mipi_intr_stat_reg;

	int i;
	u32 mask;
	int err = 0;
	int count = 0;
	u32 intr_stat;

	intr_stat = REG_READ(intr_stat_reg);
	if (!intr_stat)
		return 0;

	for (i = 0; i < 32; i++) {
		mask = (0x00000001UL) << i;
		if (!(intr_stat & mask))
			continue;

		switch (mask) {
		case BIT0:
		case BIT1:
		case BIT2:
		case BIT3:
		case BIT4:
		case BIT5:
		case BIT6:
		case BIT7:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT8:
			/*No Action required.*/
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT9:
		case BIT10:
		case BIT11:
		case BIT12:
		case BIT13:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT14:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			break;
		case BIT15:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT16:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT17:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT18:
			REG_WRITE(MIPIA_EOT_DISABLE_REG,
				REG_READ(MIPIA_EOT_DISABLE_REG)|0x30);
			while ((REG_READ(intr_stat_reg) & BIT18)) {
				count++;
				/*
				* Per silicon feedback,
				* if this bit cannot be
				* cleared by 3 times,
				* it should be a real
				* High Contention error.
				*/
				if (count == 4) {
					DRM_INFO("dsi status %s\n",
						dsi_errors[i]);
					break;
				}
				REG_WRITE(intr_stat_reg, mask);
			}
			break;
		case BIT19:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			break;
		case BIT20:
			/*No Action required.*/
			DRM_DEBUG("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT21:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_all_fifos_empty(sender);
			break;
		case BIT22:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_all_fifos_empty(sender);
			break;
		case BIT23:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT24:
			/*No Action required.*/
			DRM_DEBUG("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT25:
		case BIT26:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case BIT27:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_hs_fifos_empty(sender);
			break;
		case BIT28:
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_lp_fifos_empty(sender);
			break;
		case BIT29:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			break;
		case BIT30:
			break;
		case BIT31:
			/*No Action required.*/
			DRM_INFO("dsi status %s\n", dsi_errors[i]);
			break;
		}
	}

	return err;
}

static inline int dbi_cmd_sent(struct mdfld_dsi_pkg_sender *sender)
{
	struct drm_device *dev = sender->dev;
	u32 retry = 0xffff;
	u32 dbi_cmd_addr_reg = sender->mipi_cmd_addr_reg;
	int ret = 0;

	/*query the command execution status*/
	while (retry--) {
		if (!(REG_READ(dbi_cmd_addr_reg) & BIT0))
			break;
	}

	if (!retry) {
		DRM_ERROR("Timeout waiting for DBI Command status\n");
		ret = -EAGAIN;
	}

	return ret;
}

/**
 * NOTE: this interface is abandoned expect for write_mem_start DCS
 * other DCS are sent via generic pkg interfaces
 */
static int send_dcs_pkg(struct mdfld_dsi_pkg_sender *sender,
			struct mdfld_dsi_pkg *pkg)
{
	struct drm_device *dev = sender->dev;
	struct mdfld_dsi_dcs_pkg *dcs_pkg = &pkg->pkg.dcs_pkg;
	u32 dbi_cmd_len_reg = sender->mipi_cmd_len_reg;
	u32 dbi_cmd_addr_reg = sender->mipi_cmd_addr_reg;
	u32 cb_phy = sender->dbi_cb_phy;
	u32 index = 0;
	u8 *cb = (u8 *)sender->dbi_cb_addr;
	int i;
	int ret;

	if (!sender->dbi_pkg_support) {
		DRM_ERROR("Trying to send DCS on a non DBI output, abort!\n");
		return -ENOTSUPP;
	}

	PSB_DEBUG_MIPI("Sending DCS pkg 0x%x...\n", dcs_pkg->cmd);

	/*wait for DBI fifo empty*/
	wait_for_dbi_fifo_empty(sender);

	*(cb + (index++)) = dcs_pkg->cmd;
	if (dcs_pkg->param_num) {
		for (i = 0; i < dcs_pkg->param_num; i++) {
			*(cb + (index++)) = *(dcs_pkg->param + i);
		}
	}

	REG_WRITE(dbi_cmd_len_reg, (1 + dcs_pkg->param_num));
	REG_WRITE(dbi_cmd_addr_reg,
		(cb_phy << CMD_MEM_ADDR_OFFSET)
		| BIT0
		| ((dcs_pkg->data_src == CMD_DATA_SRC_PIPE) ? BIT1 : 0));

	ret = dbi_cmd_sent(sender);
	if (ret) {
		DRM_ERROR("command 0x%x not complete\n", dcs_pkg->cmd);
		return -EAGAIN;
	}

	PSB_DEBUG_MIPI("sent DCS pkg 0x%x...\n", dcs_pkg->cmd);

	return 0;
}

static int __send_short_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	struct drm_device *dev = sender->dev;
	u32 hs_gen_ctrl_reg = sender->mipi_hs_gen_ctrl_reg;
	u32 lp_gen_ctrl_reg = sender->mipi_lp_gen_ctrl_reg;
	u32 gen_ctrl_val = 0;
	struct mdfld_dsi_gen_short_pkg *short_pkg = &pkg->pkg.short_pkg;

	if (sender->work_for_slave_panel) {
		hs_gen_ctrl_reg += MIPIC_REG_OFFSET;
		lp_gen_ctrl_reg += MIPIC_REG_OFFSET;
	}
	gen_ctrl_val |= short_pkg->cmd << MCS_COMMANDS_POS;
	gen_ctrl_val |= 0 << DCS_CHANNEL_NUMBER_POS;
	gen_ctrl_val |= pkg->pkg_type;
	gen_ctrl_val |= short_pkg->param << MCS_PARAMETER_POS;

	if (pkg->transmission_type == MDFLD_DSI_HS_TRANSMISSION) {
		/*wait for hs fifo empty*/
		wait_for_dbi_fifo_empty(sender);
		wait_for_hs_fifos_empty(sender);

		/*send pkg*/
		REG_WRITE(hs_gen_ctrl_reg, gen_ctrl_val);
	} else if (pkg->transmission_type == MDFLD_DSI_LP_TRANSMISSION) {
		wait_for_dbi_fifo_empty(sender);
		wait_for_lp_fifos_empty(sender);

		/*send pkg*/
		REG_WRITE(lp_gen_ctrl_reg, gen_ctrl_val);
	} else {
		DRM_ERROR("Unknown transmission type %d\n",
				pkg->transmission_type);
		return -EINVAL;
	}

	return 0;
}

static int __send_long_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	struct drm_device *dev = sender->dev;
	u32 hs_gen_ctrl_reg = sender->mipi_hs_gen_ctrl_reg;
	u32 hs_gen_data_reg = sender->mipi_hs_gen_data_reg;
	u32 lp_gen_ctrl_reg = sender->mipi_lp_gen_ctrl_reg;
	u32 lp_gen_data_reg = sender->mipi_lp_gen_data_reg;
	u32 gen_ctrl_val = 0;
	u8 *dp = NULL;
	u32 reg_val = 0;
	int i;
	int dword_count = 0, remain_byte_count = 0;
	struct mdfld_dsi_gen_long_pkg *long_pkg = &pkg->pkg.long_pkg;

	dp = long_pkg->data;
	if (sender->work_for_slave_panel) {
		hs_gen_ctrl_reg += MIPIC_REG_OFFSET;
		hs_gen_data_reg += MIPIC_REG_OFFSET;
		lp_gen_ctrl_reg += MIPIC_REG_OFFSET;
		lp_gen_data_reg += MIPIC_REG_OFFSET;
	}

	/**
	 * Set up word count for long pkg
	 * FIXME: double check word count field.
	 * currently, using the byte counts of the payload as the word count.
	 * ------------------------------------------------------------
	 * | DI |   WC   | ECC|         PAYLOAD              |CHECKSUM|
	 * ------------------------------------------------------------
	 */
	gen_ctrl_val |= (long_pkg->len) << WORD_COUNTS_POS;
	gen_ctrl_val |= 0 << DCS_CHANNEL_NUMBER_POS;
	gen_ctrl_val |= pkg->pkg_type;

	if (pkg->transmission_type == MDFLD_DSI_HS_TRANSMISSION) {
		/*wait for hs ctrl and data fifos to be empty*/
		wait_for_dbi_fifo_empty(sender);
		wait_for_hs_fifos_empty(sender);

		dword_count = long_pkg->len / 4;
		remain_byte_count = long_pkg->len % 4;
		for (i = 0; i < dword_count * 4; i = i + 4) {
			reg_val = 0;
			reg_val = *(dp + i);
			reg_val |= *(dp + i + 1) << 8;
			reg_val |= *(dp + i + 2) << 16;
			reg_val |= *(dp + i + 3) << 24;
			PSB_DEBUG_MIPI("HS Sending data 0x%08x\n", reg_val);
			REG_WRITE(hs_gen_data_reg, reg_val);
		}

		if (remain_byte_count) {
			reg_val = 0;
			for (i = 0; i < remain_byte_count; i++)
				reg_val |=
					*(dp + dword_count * 4 + i) << (8 * i);
			PSB_DEBUG_MIPI("HS Sending data 0x%08x\n", reg_val);
			REG_WRITE(hs_gen_data_reg, reg_val);
		}

		REG_WRITE(hs_gen_ctrl_reg, gen_ctrl_val);
	} else if (pkg->transmission_type == MDFLD_DSI_LP_TRANSMISSION) {
		wait_for_dbi_fifo_empty(sender);
		wait_for_lp_fifos_empty(sender);

		dword_count = long_pkg->len / 4;
		remain_byte_count = long_pkg->len % 4;
		for (i = 0; i < dword_count * 4; i = i + 4) {
			reg_val = 0;
			reg_val = *(dp + i);
			reg_val |= *(dp + i + 1) << 8;
			reg_val |= *(dp + i + 2) << 16;
			reg_val |= *(dp + i + 3) << 24;
			PSB_DEBUG_MIPI("LP Sending data 0x%08x\n", reg_val);
			REG_WRITE(lp_gen_data_reg, reg_val);
		}

		if (remain_byte_count) {
			reg_val = 0;
			for (i = 0; i < remain_byte_count; i++) {
				reg_val |=
					*(dp + dword_count * 4 + i) << (8 * i);
			}
			PSB_DEBUG_MIPI("LP Sending data 0x%08x\n", reg_val);
			REG_WRITE(lp_gen_data_reg, reg_val);
		}

		REG_WRITE(lp_gen_ctrl_reg, gen_ctrl_val);
	} else {
		DRM_ERROR("Unknown transmission type %d\n",
				pkg->transmission_type);
		return -EINVAL;
	}

	return 0;

}

static int send_mcs_short_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	PSB_DEBUG_MIPI("Sending MCS short pkg...\n");

	return __send_short_pkg(sender, pkg);
}

static int send_mcs_long_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	PSB_DEBUG_MIPI("Sending MCS long pkg...\n");

	return __send_long_pkg(sender, pkg);
}

static int send_gen_short_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	PSB_DEBUG_MIPI("Sending GEN short pkg...\n");

	return __send_short_pkg(sender, pkg);
}

static int send_gen_long_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	PSB_DEBUG_MIPI("Sending GEN long pkg...\n");

	return __send_long_pkg(sender, pkg);
}

static int send_dpi_spk_pkg(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	struct drm_device *dev = sender->dev;
	u32 dpi_control_reg = sender->mipi_dpi_control_reg;
	u32 intr_stat_reg = sender->mipi_intr_stat_reg;
	u32 dpi_control_val = 0;
	u32 dpi_control_current_setting = 0;
	struct mdfld_dsi_dpi_spk_pkg *spk_pkg = &pkg->pkg.spk_pkg;
	int retry = 10000;

	dpi_control_val = spk_pkg->cmd;

	if (pkg->transmission_type == MDFLD_DSI_LP_TRANSMISSION)
		dpi_control_val |= BIT6;

	/*Wait for DPI fifo empty*/
	wait_for_dpi_fifo_empty(sender);

	/*clean spk packet sent interrupt*/
	REG_WRITE(intr_stat_reg, BIT30);
	dpi_control_current_setting =
		REG_READ(dpi_control_reg);

	/*send out spk packet*/
	if (dpi_control_current_setting != dpi_control_val) {
		REG_WRITE(dpi_control_reg, dpi_control_val);

		/*wait for spk packet sent interrupt*/
		while (--retry && !(REG_READ(intr_stat_reg) & BIT30))
			udelay(3);

		if (!retry) {
			DRM_ERROR("Fail to send SPK Packet 0x%x\n",
				 spk_pkg->cmd);
			return -EINVAL;
		}
	} else
		/*For SHUT_DOWN and TURN_ON, it is better called by
		symmetrical. so skip duplicate called*/
		printk(KERN_WARNING "skip duplicate setting of DPI control\n");
	return 0;
}

static int send_pkg_prepare(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg)
{
	u8 cmd;
	u8 *data;

	PSB_DEBUG_MIPI("Prepare to Send type 0x%x pkg\n", pkg->pkg_type);

	switch (pkg->pkg_type) {
	case MDFLD_DSI_PKG_DCS:
		cmd = pkg->pkg.dcs_pkg.cmd;
		break;
	case MDFLD_DSI_PKG_MCS_SHORT_WRITE_0:
	case MDFLD_DSI_PKG_MCS_SHORT_WRITE_1:
		cmd = pkg->pkg.short_pkg.cmd;
		break;
	case MDFLD_DSI_PKG_MCS_LONG_WRITE:
		data = (u8 *)pkg->pkg.long_pkg.data;
		cmd = *data;
		break;
	default:
		return 0;
	}

	/*this prevents other package sending while doing msleep*/
	sender->status = MDFLD_DSI_PKG_SENDER_BUSY;

	return 0;
}

static int send_pkg_done(struct mdfld_dsi_pkg_sender *sender,
		struct mdfld_dsi_pkg *pkg)
{
	u8 cmd;
	u8 *data = NULL;

	PSB_DEBUG_MIPI("Sent type 0x%x pkg\n", pkg->pkg_type);

	switch (pkg->pkg_type) {
	case MDFLD_DSI_PKG_DCS:
		cmd = pkg->pkg.dcs_pkg.cmd;
		break;
	case MDFLD_DSI_PKG_MCS_SHORT_WRITE_0:
	case MDFLD_DSI_PKG_MCS_SHORT_WRITE_1:
		cmd = pkg->pkg.short_pkg.cmd;
		break;
	case MDFLD_DSI_PKG_MCS_LONG_WRITE:
	case MDFLD_DSI_PKG_GEN_LONG_WRITE:
		data = (u8 *)pkg->pkg.long_pkg.data;
		cmd = *data;
		break;
	default:
		return 0;
	}

	/*update panel status*/
	if (unlikely(cmd == enter_sleep_mode))
		sender->panel_mode |= MDFLD_DSI_PANEL_MODE_SLEEP;
	else if (unlikely(cmd == exit_sleep_mode))
		sender->panel_mode &= ~MDFLD_DSI_PANEL_MODE_SLEEP;

	if (sender->status != MDFLD_DSI_CONTROL_ABNORMAL)
		sender->status = MDFLD_DSI_PKG_SENDER_FREE;

	/*after sending pkg done, free the data buffer for mcs long pkg*/
	if (pkg->pkg_type == MDFLD_DSI_PKG_MCS_LONG_WRITE ||
		pkg->pkg_type == MDFLD_DSI_PKG_GEN_LONG_WRITE) {
		if (data != NULL)
			kfree(data);
	}

	return 0;
}

static int do_send_pkg(struct mdfld_dsi_pkg_sender *sender,
			struct mdfld_dsi_pkg *pkg)
{
	int ret = 0;

	PSB_DEBUG_MIPI("Sending type 0x%x pkg\n", pkg->pkg_type);

	if (sender->status == MDFLD_DSI_PKG_SENDER_BUSY) {
		DRM_ERROR("sender is busy\n");
		return -EAGAIN;
	}

	ret = send_pkg_prepare(sender, pkg);
	if (ret) {
		DRM_ERROR("send_pkg_prepare error\n");
		return ret;
	}

	switch (pkg->pkg_type) {
	case MDFLD_DSI_PKG_DCS:
		ret = send_dcs_pkg(sender, pkg);
		break;
	case MDFLD_DSI_PKG_GEN_SHORT_WRITE_0:
	case MDFLD_DSI_PKG_GEN_SHORT_WRITE_1:
	case MDFLD_DSI_PKG_GEN_SHORT_WRITE_2:
	case MDFLD_DSI_PKG_GEN_READ_0:
	case MDFLD_DSI_PKG_GEN_READ_1:
	case MDFLD_DSI_PKG_GEN_READ_2:
		ret = send_gen_short_pkg(sender, pkg);
		break;
	case MDFLD_DSI_PKG_GEN_LONG_WRITE:
		ret = send_gen_long_pkg(sender, pkg);
		break;
	case MDFLD_DSI_PKG_MCS_SHORT_WRITE_0:
	case MDFLD_DSI_PKG_MCS_SHORT_WRITE_1:
	case MDFLD_DSI_PKG_MCS_READ:
		ret = send_mcs_short_pkg(sender, pkg);
		break;
	case MDFLD_DSI_PKG_MCS_LONG_WRITE:
		ret = send_mcs_long_pkg(sender, pkg);
		break;
	case MDFLD_DSI_DPI_SPK:
		ret = send_dpi_spk_pkg(sender, pkg);
		break;
	default:
		DRM_ERROR("Invalid pkg type 0x%x\n", pkg->pkg_type);
		ret = -EINVAL;
	}

	send_pkg_done(sender, pkg);

	return ret;
}

static int send_pkg(struct mdfld_dsi_pkg_sender *sender,
			struct mdfld_dsi_pkg *pkg)
{
	int err = 0;

	/*handle DSI error*/
	err = dsi_error_handler(sender);
	if (err) {
		DRM_ERROR("Error handling failed\n");
		err = -EAGAIN;
		goto send_pkg_err;
	}

	/*send pkg*/
	err = do_send_pkg(sender, pkg);
	if (err) {
		DRM_ERROR("sent pkg failed\n");
		dsi_error_handler(sender);
		err = -EAGAIN;
		goto send_pkg_err;
	}

	/*FIXME: should I query complete and fifo empty here?*/
send_pkg_err:
	return err;
}

static struct mdfld_dsi_pkg *
pkg_sender_get_pkg_locked(struct mdfld_dsi_pkg_sender *sender)
{
	struct mdfld_dsi_pkg *pkg;

	if (list_empty(&sender->free_list)) {
		DRM_ERROR("No free pkg left\n");
		return NULL;
	}

	pkg = list_first_entry(&sender->free_list, struct mdfld_dsi_pkg, entry);

	/*detach from free list*/
	list_del_init(&pkg->entry);

	return pkg;
}

static void pkg_sender_put_pkg_locked(struct mdfld_dsi_pkg_sender *sender,
		struct mdfld_dsi_pkg *pkg)
{
	memset(pkg, 0, sizeof(struct mdfld_dsi_pkg));

	INIT_LIST_HEAD(&pkg->entry);

	list_add_tail(&pkg->entry, &sender->free_list);
}

static int mdfld_dbi_cb_init(struct mdfld_dsi_pkg_sender *sender,
		struct psb_gtt *pg, int pipe)
{
	uint32_t phy;
	void *virt_addr = NULL;

	switch (pipe) {
	case 0:
		phy = pg->gtt_phys_start - 0x1000;
		break;
	case 2:
		phy = pg->gtt_phys_start - 0x800;
		break;
	default:
		DRM_ERROR("Unsupported channel\n");
		return -EINVAL;
	}

	/*mapping*/
	virt_addr = ioremap_nocache(phy, 0x800);
	if (!virt_addr) {
		DRM_ERROR("Map DBI command buffer error\n");
		return -ENOMEM;
	}

	if (IS_ANN(dev))
		memset(virt_addr, 0x0, 0x800);

	sender->dbi_cb_phy = phy;
	sender->dbi_cb_addr = virt_addr;

	PSB_DEBUG_ENTRY("DBI command buffer initailized. phy %x, addr %p\n",
			phy, virt_addr);

	return 0;
}

static void mdfld_dbi_cb_destroy(struct mdfld_dsi_pkg_sender *sender)
{
	PSB_DEBUG_ENTRY("\n");

	if (sender && sender->dbi_cb_addr)
		iounmap(sender->dbi_cb_addr);
}

static inline void pkg_sender_queue_pkg(struct mdfld_dsi_pkg_sender *sender,
					struct mdfld_dsi_pkg *pkg,
					int delay)
{
	mutex_lock(&sender->lock);

	if (!delay) {
		send_pkg(sender, pkg);

		pkg_sender_put_pkg_locked(sender, pkg);
	} else {
		/*queue it*/
		list_add_tail(&pkg->entry, &sender->pkg_list);
	}

	mutex_unlock(&sender->lock);
}

static inline int process_pkg_list(struct mdfld_dsi_pkg_sender *sender)
{
	struct mdfld_dsi_pkg *pkg;
	int ret = 0;

	mutex_lock(&sender->lock);

	while (!list_empty(&sender->pkg_list)) {
		pkg = list_first_entry(&sender->pkg_list,
				struct mdfld_dsi_pkg, entry);
		ret = send_pkg(sender, pkg);

		if (ret) {
			DRM_INFO("Returning eror from process_pkg_lisgt");
			goto errorunlock;
		}

		list_del_init(&pkg->entry);

		pkg_sender_put_pkg_locked(sender, pkg);
	}

	mutex_unlock(&sender->lock);
	return 0;

errorunlock:
	mutex_unlock(&sender->lock);
	return ret;
}

static int mdfld_dsi_send_mcs_long(struct mdfld_dsi_pkg_sender *sender,
				   u8 *data,
				   u32 len,
				   u8 transmission,
				   int delay)
{
	struct mdfld_dsi_pkg *pkg;
	u8 *pdata = NULL;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	/* alloc a data buffer to save the long pkg data,
	 * free the buffer when send_pkg_done.
	 * */
	pdata = kmalloc(sizeof(u8) * len, GFP_KERNEL);
	if (!pdata) {
		DRM_ERROR("No memory for long_pkg data\n");
		return -ENOMEM;
	}

	memcpy(pdata, data, len * sizeof(u8));

	pkg->pkg_type = MDFLD_DSI_PKG_MCS_LONG_WRITE;
	pkg->transmission_type = transmission;
	pkg->pkg.long_pkg.data = pdata;
	pkg->pkg.long_pkg.len = len;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int mdfld_dsi_send_mcs_short(struct mdfld_dsi_pkg_sender *sender,
					u8 cmd, u8 param, u8 param_num,
					u8 transmission,
					int delay)
{
	struct mdfld_dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	if (param_num) {
		pkg->pkg_type = MDFLD_DSI_PKG_MCS_SHORT_WRITE_1;
		pkg->pkg.short_pkg.param = param;
	} else {
		pkg->pkg_type = MDFLD_DSI_PKG_MCS_SHORT_WRITE_0;
		pkg->pkg.short_pkg.param = 0;
	}
	pkg->transmission_type = transmission;
	pkg->pkg.short_pkg.cmd = cmd;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int mdfld_dsi_send_gen_short(struct mdfld_dsi_pkg_sender *sender,
					u8 param0, u8 param1, u8 param_num,
					u8 transmission,
					int delay)
{
	struct mdfld_dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	switch (param_num) {
	case 0:
		pkg->pkg_type = MDFLD_DSI_PKG_GEN_SHORT_WRITE_0;
		pkg->pkg.short_pkg.cmd = 0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 1:
		pkg->pkg_type = MDFLD_DSI_PKG_GEN_SHORT_WRITE_1;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 2:
		pkg->pkg_type = MDFLD_DSI_PKG_GEN_SHORT_WRITE_2;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = param1;
		break;
	}

	pkg->transmission_type = transmission;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int mdfld_dsi_send_gen_long(struct mdfld_dsi_pkg_sender *sender,
				   u8 *data,
				   u32 len,
				   u8 transmission,
				   int delay)
{
	struct mdfld_dsi_pkg *pkg;
	u8 *pdata = NULL;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	/* alloc a data buffer to save the long pkg data,
	 * free the buffer when send_pkg_done.
	 * */
	pdata = kmalloc(sizeof(u8)*len, GFP_KERNEL);
	if (!pdata) {
		DRM_ERROR("No memory for long_pkg data\n");
		return -ENOMEM;
	}

	memcpy(pdata, data, len*sizeof(u8));

	pkg->pkg_type = MDFLD_DSI_PKG_GEN_LONG_WRITE;
	pkg->transmission_type = transmission;
	pkg->pkg.long_pkg.data = pdata;
	pkg->pkg.long_pkg.len = len;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int __read_panel_data(struct mdfld_dsi_pkg_sender *sender,
				struct mdfld_dsi_pkg *pkg,
				u8 *data,
				u32 len)
{
	struct drm_device *dev = sender->dev;
	int i;
	u32 gen_data_reg;
	u32 gen_data_value;
	int retry = MDFLD_DSI_READ_MAX_COUNT;
	u8 transmission = pkg->transmission_type;
	int dword_count = 0, remain_byte_count = 0;

	/*Check the len. Max value is 0x40
	based on the generic read FIFO size*/
	if (len * sizeof(*data) > 0x40) {
		len = 0x40 / sizeof(*data);
		DRM_ERROR("Bigger than Max.Set the len to Max 0x40 bytes\n");
	}

	/**
	 * do reading.
	 * 0) set the max return pack size
	 * 1) send out generic read request
	 * 2) polling read data avail interrupt
	 * 3) read data
	 */
	mutex_lock(&sender->lock);

	/*Set the Max return pack size*/
	wait_for_all_fifos_empty(sender);
	REG_WRITE(MIPIA_MAX_RETURN_PACK_SIZE_REG, (len*sizeof(*data)) & 0x3FF);
	wait_for_all_fifos_empty(sender);

	REG_WRITE(sender->mipi_intr_stat_reg, BIT29);

	if ((REG_READ(sender->mipi_intr_stat_reg) & BIT29))
		DRM_ERROR("Can NOT clean read data valid interrupt\n");

	/*send out read request*/
	send_pkg(sender, pkg);

	pkg_sender_put_pkg_locked(sender, pkg);

	/*polling read data avail interrupt*/
	while (--retry && !(REG_READ(sender->mipi_intr_stat_reg) & BIT29))
		udelay(3);

	if (!retry) {
		mutex_unlock(&sender->lock);
		return -ETIMEDOUT;
	}

	REG_WRITE(sender->mipi_intr_stat_reg, BIT29);

	/*read data*/
	if (transmission == MDFLD_DSI_HS_TRANSMISSION)
		gen_data_reg = sender->mipi_hs_gen_data_reg;
	else if (transmission == MDFLD_DSI_LP_TRANSMISSION)
		gen_data_reg = sender->mipi_lp_gen_data_reg;
	else {
		DRM_ERROR("Unknown transmission");
		mutex_unlock(&sender->lock);
		return -EINVAL;
	}

	dword_count = len / 4;
	remain_byte_count = len % 4;
	for (i = 0; i < dword_count * 4; i = i + 4) {
		gen_data_value = REG_READ(gen_data_reg);
		*(data + i)     = gen_data_value & 0x000000FF;
		*(data + i + 1) = (gen_data_value >> 8)  & 0x000000FF;
		*(data + i + 2) = (gen_data_value >> 16) & 0x000000FF;
		*(data + i + 3) = (gen_data_value >> 24) & 0x000000FF;
	}
	if (remain_byte_count) {
		gen_data_value = REG_READ(gen_data_reg);
		for (i = 0; i < remain_byte_count; i++) {
			*(data + dword_count * 4 + i)  =
				(gen_data_value >> (8 * i)) & 0x000000FF;
		}
	}

	mutex_unlock(&sender->lock);

	return len;
}

static int mdfld_dsi_read_gen(struct mdfld_dsi_pkg_sender *sender,
				u8 param0,
				u8 param1,
				u8 param_num,
				u8 *data,
				u32 len,
				u8 transmission)
{
	struct mdfld_dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	switch (param_num) {
	case 0:
		pkg->pkg_type = MDFLD_DSI_PKG_GEN_READ_0;
		pkg->pkg.short_pkg.cmd = 0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 1:
		pkg->pkg_type = MDFLD_DSI_PKG_GEN_READ_1;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 2:
		pkg->pkg_type = MDFLD_DSI_PKG_GEN_READ_2;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = param1;
		break;
	}

	pkg->transmission_type = transmission;

	INIT_LIST_HEAD(&pkg->entry);

	return __read_panel_data(sender, pkg, data, len);
}

static int mdfld_dsi_read_mcs(struct mdfld_dsi_pkg_sender *sender,
				u8 cmd,
				u8 *data,
				u32 len,
				u8 transmission)
{
	struct mdfld_dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	pkg->pkg_type = MDFLD_DSI_PKG_MCS_READ;
	pkg->pkg.short_pkg.cmd = cmd;
	pkg->pkg.short_pkg.param = 0;

	pkg->transmission_type = transmission;

	INIT_LIST_HEAD(&pkg->entry);

	return __read_panel_data(sender, pkg, data, len);
}

static int mdfld_dsi_send_dpi_spk_pkg(struct mdfld_dsi_pkg_sender *sender,
				u32 spk_pkg,
				u8 transmission)
{
	struct mdfld_dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}

	pkg->pkg_type = MDFLD_DSI_DPI_SPK;
	pkg->transmission_type = transmission;
	pkg->pkg.spk_pkg.cmd = spk_pkg;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, 0);

	return 0;
}

void dsi_controller_dbi_init(struct mdfld_dsi_config *dsi_config, int pipe)
{
	struct drm_device *dev = dsi_config->dev;
	u32 reg_offset = pipe ? MIPIC_REG_OFFSET : 0;
	int lane_count = dsi_config->lane_count;
	u32 val = 0;

	PSB_DEBUG_ENTRY("Init DBI interface on pipe %d...\n", pipe);

	/*un-ready device*/
	REG_WRITE((MIPIA_DEVICE_READY_REG + reg_offset), 0x00000000);

	/*init dsi adapter before kicking off*/
	REG_WRITE((MIPIA_CONTROL_REG + reg_offset), 0x00000018);

	/*TODO: figure out how to setup these registers*/
	REG_WRITE((MIPIA_DPHY_PARAM_REG + reg_offset), 0x150c3408);
	REG_WRITE((MIPIA_CLK_LANE_SWITCH_TIME_CNT_REG + reg_offset),
			0x000a0014);
	REG_WRITE((MIPIA_DBI_BW_CTRL_REG + reg_offset), 0x00000400);
	REG_WRITE((MIPIA_DBI_FIFO_THROTTLE_REG + reg_offset), 0x00000001);
	REG_WRITE((MIPIA_HS_LS_DBI_ENABLE_REG + reg_offset), 0x00000000);

	/*enable all interrupts*/
	REG_WRITE((MIPIA_INTR_EN_REG + reg_offset), 0xffffffff);
	/*max value: 20 clock cycles of txclkesc*/
	REG_WRITE((MIPIA_TURN_AROUND_TIMEOUT_REG + reg_offset), 0x0000001f);
	/*min 21 txclkesc, max: ffffh*/
	REG_WRITE((MIPIA_DEVICE_RESET_TIMER_REG + reg_offset), 0x0000ffff);
	/*min: 7d0 max: 4e20*/
	REG_WRITE((MIPIA_INIT_COUNT_REG + reg_offset), 0x00000fa0);

	/*set up max return packet size*/
	REG_WRITE((MIPIA_MAX_RETURN_PACK_SIZE_REG + reg_offset),
			MDFLD_DSI_MAX_RETURN_PACKET_SIZE);

	/*set up func_prg*/
	val |= lane_count;
	val |= (dsi_config->channel_num << DSI_DBI_VIRT_CHANNEL_OFFSET);
	val |= DSI_DBI_COLOR_FORMAT_OPTION2;
	REG_WRITE((MIPIA_DSI_FUNC_PRG_REG + reg_offset), val);

	REG_WRITE((MIPIA_HS_TX_TIMEOUT_REG + reg_offset), 0x3fffff);
	REG_WRITE((MIPIA_LP_RX_TIMEOUT_REG + reg_offset), 0xffff);

	REG_WRITE((MIPIA_HIGH_LOW_SWITCH_COUNT_REG + reg_offset), 0x46);
	REG_WRITE((MIPIA_EOT_DISABLE_REG + reg_offset), 0x00000000);
	REG_WRITE((MIPIA_LP_BYTECLK_REG + reg_offset), 0x00000004);
	REG_WRITE((MIPIA_DEVICE_READY_REG + reg_offset), 0x00000001);
}

int mdfld_dsi_cmds_kick_out(struct mdfld_dsi_pkg_sender *sender)
{
	return process_pkg_list(sender);
}

int mdfld_dsi_status_check(struct mdfld_dsi_pkg_sender *sender)
{
	return dsi_error_handler(sender);
}

int mdfld_dsi_check_fifo_empty(struct mdfld_dsi_pkg_sender *sender)
{
	struct drm_device *dev;

	if (!sender) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	dev = sender->dev;

	if (!sender->dbi_pkg_support) {
		DRM_ERROR("No DBI pkg sending on this sender\n");
		return -ENOTSUPP;
	}

	return REG_READ(sender->mipi_gen_fifo_stat_reg) & BIT27;
}

int mdfld_dsi_send_dcs(struct mdfld_dsi_pkg_sender *sender,
			u8 dcs, u8 *param, u32 param_num, u8 data_src,
			int delay)
{
	u32 cb_phy;
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;
	u32 index = 0;
	u8 *cb;
	int retry = 1;
	u8 *dst = NULL;
	u8 *pSendparam = NULL;
	int err = 0;
	u32 fifo_sr;
	int i;
	int loop_num = 1;
	int offset = 0;

	if (!sender) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	cb_phy = sender->dbi_cb_phy;
	dev = sender->dev;
	cb = (u8 *)sender->dbi_cb_addr;
	dev_priv = dev->dev_private;

	if (!sender->dbi_pkg_support) {
		DRM_ERROR("No DBI pkg sending on this sender\n");
		return -ENOTSUPP;
	}

	/*
	 * If dcs is write_mem_start, send it directly using
	 * DSI adapter interface
	 */
	if (dcs == write_mem_start) {

		/**
		 * query whether DBI FIFO is empty,
		 * if not sleep the drv and wait for it to become empty.
		 * The MIPI frame done interrupt will wake up the drv.
		 */
		if (is_dual_dsi(dev))
			loop_num = 2;
		mutex_lock(&sender->lock);

		/*handle DSI error*/
		if (dsi_error_handler(sender)) {
			mutex_unlock(&sender->lock);
			DRM_ERROR("Error handling failed\n");
			return  -EAGAIN;
		}
		/**
		 * check the whether is there any write_mem_start already being
		 * sent in this between current te_seq and next te.
		 * if yes, simply reject the rest of write_mem_start because it
		 * is unnecessary. otherwise, go ahead and kick of a
		 * write_mem_start.
		 */
		if (atomic64_read(&sender->last_screen_update) ==
			atomic64_read(&sender->te_seq)) {
			mutex_unlock(&sender->lock);
			if (dev_priv->b_async_flip_enable)
				DRM_INFO("reject write_mem_start last_screen_update[%lld], te_seq[%lld]\n",
						atomic64_read(&sender->last_screen_update), atomic64_read(&sender->te_seq));
			return -EAGAIN;
		}

		for(i = 0; i < loop_num; i++) {
			if (i != 0)
				offset = MIPIC_REG_OFFSET;

			if (!IS_TNG_A0(dev)) {
				retry = wait_event_interruptible_timeout(dev_priv->eof_wait,
				  (REG_READ(sender->mipi_gen_fifo_stat_reg) & BIT27),
				    msecs_to_jiffies(MDFLD_DSI_DBI_FIFO_TIMEOUT));
			} else {
				retry = MDFLD_DSI_DBI_FIFO_TIMEOUT;
				while (retry && !(REG_READ(sender->mipi_gen_fifo_stat_reg + offset) & BIT27)) {
					udelay(500);
					retry--;
				}
			}

			/*if DBI FIFO timeout, drop this frame*/
			if (!retry) {
				DRM_ERROR("DBI FIFO timeout, drop frame\n");
				mutex_unlock(&sender->lock);
				if (!IS_ANN(dev)) {
					debug_dbi_hang(sender);
					panic("DBI FIFO timeout, drop frame\n");
				}
				return 0;
			}

			if (i != 0)
				sender->work_for_slave_panel = true;

			/*wait for generic fifo*/
			if (REG_READ(HS_LS_DBI_ENABLE_REG + offset) & BIT0)
				wait_for_lp_fifos_empty(sender);
			else
				wait_for_hs_fifos_empty(sender);
			sender->work_for_slave_panel = false;
		}

		/*record the last screen update timestamp*/
		atomic64_set(&sender->last_screen_update,
			atomic64_read(&sender->te_seq));
		*(cb + (index++)) = write_mem_start;

		/* Set write_mem_start to mipi C first */
		if (is_dual_dsi(dev))
			REG_WRITE(sender->mipi_cmd_len_reg + MIPIC_REG_OFFSET, 1);
		REG_WRITE(sender->mipi_cmd_len_reg, 1);
		if (is_dual_dsi(dev))
			REG_WRITE(sender->mipi_cmd_addr_reg + MIPIC_REG_OFFSET, cb_phy | BIT0 | BIT1);
		REG_WRITE(sender->mipi_cmd_addr_reg, cb_phy | BIT0 | BIT1);

		if (is_dual_dsi(dev)) {
			retry = MDFLD_DSI_DBI_FIFO_TIMEOUT;
			while (retry && (REG_READ(sender->mipi_cmd_addr_reg + MIPIC_REG_OFFSET) & BIT0)) {
				udelay(1);
				retry--;
			}
		}

		retry = MDFLD_DSI_DBI_FIFO_TIMEOUT;
		while (retry && (REG_READ(sender->mipi_cmd_addr_reg) & BIT0)) {
			usleep_range(990, 1010);
			retry--;
		}
		mutex_unlock(&sender->lock);
		return 0;
	}

	if (param_num == 0)
		err =  mdfld_dsi_send_mcs_short_hs(sender, dcs, 0, 0, delay);
	else if (param_num == 1)
		err =  mdfld_dsi_send_mcs_short_hs(sender, dcs, param[0], 1,
				delay);
	else if (param_num > 1) {
		/*transfer to dcs package*/
		pSendparam = kmalloc(sizeof(u8) * (param_num + 1), GFP_KERNEL);
		if (!pSendparam) {
			DRM_ERROR("No memory\n");
			return -ENOMEM;
		}

		(*pSendparam) = dcs;

		dst = pSendparam + 1;
		memcpy(dst, param, param_num);

		err = mdfld_dsi_send_mcs_long_hs(sender, pSendparam,
				param_num + 1, delay);

		/*free pkg*/
		kfree(pSendparam);
	}

	return err;
}

int mdfld_dsi_send_mcs_short_hs(struct mdfld_dsi_pkg_sender *sender,
				u8 cmd, u8 param, u8 param_num, int delay)
{
	if (!sender) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_mcs_short(sender, cmd, param, param_num,
			MDFLD_DSI_HS_TRANSMISSION, delay);
}

int mdfld_dsi_send_mcs_short_lp(struct mdfld_dsi_pkg_sender *sender,
				u8 cmd, u8 param, u8 param_num, int delay)
{
	if (!sender) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_mcs_short(sender, cmd, param, param_num,
			MDFLD_DSI_LP_TRANSMISSION, delay);
}

int mdfld_dsi_send_mcs_long_hs(struct mdfld_dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_mcs_long(sender, data, len,
			MDFLD_DSI_HS_TRANSMISSION, delay);
}

int mdfld_dsi_send_mcs_long_lp(struct mdfld_dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_mcs_long(sender, data, len,
			MDFLD_DSI_LP_TRANSMISSION, delay);
}

int mdfld_dsi_send_gen_short_hs(struct mdfld_dsi_pkg_sender *sender,
				u8 param0, u8 param1, u8 param_num, int delay)
{
	if (!sender) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_gen_short(sender, param0, param1, param_num,
			MDFLD_DSI_HS_TRANSMISSION, delay);
}

int mdfld_dsi_send_gen_short_lp(struct mdfld_dsi_pkg_sender *sender,
				u8 param0, u8 param1, u8 param_num, int delay)
{
	if (!sender || param_num < 0 || param_num > 2) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_gen_short(sender, param0, param1, param_num,
			MDFLD_DSI_LP_TRANSMISSION, delay);
}

int mdfld_dsi_send_gen_long_hs(struct mdfld_dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_gen_long(sender, data, len,
			MDFLD_DSI_HS_TRANSMISSION, delay);
}

int mdfld_dsi_send_gen_long_lp(struct mdfld_dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_gen_long(sender, data, len,
			MDFLD_DSI_LP_TRANSMISSION, delay);
}

int mdfld_dsi_read_gen_hs(struct mdfld_dsi_pkg_sender *sender,
			u8 param0,
			u8 param1,
			u8 param_num,
			u8 *data,
			u32 len)
{
	if (!sender || !data || param_num < 0 || param_num > 2
		|| !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_read_gen(sender, param0, param1, param_num,
				data, len, MDFLD_DSI_HS_TRANSMISSION);

}

int mdfld_dsi_read_gen_lp(struct mdfld_dsi_pkg_sender *sender,
			u8 param0,
			u8 param1,
			u8 param_num,
			u8 *data,
			u32 len)
{
	if (!sender || !data || param_num < 0 || param_num > 2
		|| !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_read_gen(sender, param0, param1, param_num,
				data, len, MDFLD_DSI_LP_TRANSMISSION);
}

int mdfld_dsi_read_mcs_hs(struct mdfld_dsi_pkg_sender *sender,
			u8 cmd,
			u8 *data,
			u32 len)
{
	if (!sender || !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_read_mcs(sender, cmd, data, len,
				MDFLD_DSI_HS_TRANSMISSION);
}
EXPORT_SYMBOL(mdfld_dsi_read_mcs_hs);

int mdfld_dsi_read_mcs_lp(struct mdfld_dsi_pkg_sender *sender,
			u8 cmd,
			u8 *data,
			u32 len)
{
	if (!sender || !data || !len) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_read_mcs(sender, cmd, data, len,
				MDFLD_DSI_LP_TRANSMISSION);
}

int mdfld_dsi_send_dpi_spk_pkg_hs(struct mdfld_dsi_pkg_sender *sender,
				u32 spk_pkg)
{
	if (!sender) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_dpi_spk_pkg(sender, spk_pkg,
				MDFLD_DSI_HS_TRANSMISSION);
}

int mdfld_dsi_send_dpi_spk_pkg_lp(struct mdfld_dsi_pkg_sender *sender,
				u32 spk_pkg)
{
	if (!sender) {
		DRM_ERROR("Invalid parameters\n");
		return -EINVAL;
	}

	return mdfld_dsi_send_dpi_spk_pkg(sender, spk_pkg,
				MDFLD_DSI_LP_TRANSMISSION);
}

int mdfld_dsi_wait_for_fifos_empty(struct mdfld_dsi_pkg_sender *sender)
{
	return wait_for_all_fifos_empty(sender);
}

void mdfld_dsi_report_te(struct mdfld_dsi_pkg_sender *sender)
{
	if (sender)
		atomic64_inc(&sender->te_seq);
}

int mdfld_dsi_pkg_sender_init(struct mdfld_dsi_connector *dsi_connector,
		int pipe)
{
	int ret;
	struct mdfld_dsi_pkg_sender *pkg_sender;
	struct mdfld_dsi_config *dsi_config =
		mdfld_dsi_get_config(dsi_connector);
	if (!dsi_config) {
		DRM_ERROR("dsi_config is NULL\n");
		return -EINVAL;
	}

	struct drm_device *dev = dsi_config->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_gtt *pg = dev_priv->pg;
	int i;
	struct mdfld_dsi_pkg *pkg, *tmp;

	PSB_DEBUG_ENTRY("\n");

	if (!dsi_connector) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	pkg_sender = dsi_connector->pkg_sender;

	if (!pkg_sender || IS_ERR(pkg_sender)) {
		pkg_sender = kzalloc(sizeof(struct mdfld_dsi_pkg_sender),
				GFP_KERNEL);
		if (!pkg_sender) {
			DRM_ERROR("Create DSI pkg sender failed\n");
			return -ENOMEM;
		}

		dsi_connector->pkg_sender = (void *)pkg_sender;
	}

	pkg_sender->dev = dev;
	pkg_sender->dsi_connector = dsi_connector;
	pkg_sender->pipe = pipe;
	pkg_sender->pkg_num = 0;
	pkg_sender->panel_mode = 0;
	pkg_sender->status = MDFLD_DSI_PKG_SENDER_FREE;

	/*int dbi command buffer*/
	if (dsi_config->type == MDFLD_DSI_ENCODER_DBI) {
		pkg_sender->dbi_pkg_support = 1;
		ret = mdfld_dbi_cb_init(pkg_sender, pg, pipe);
		if (ret) {
			DRM_ERROR("DBI command buffer map failed\n");
			goto mapping_err;
		}
	}

	/*init regs*/
	if (pipe == 0) {
		pkg_sender->dpll_reg = MRST_DPLL_A;
		pkg_sender->dspcntr_reg = DSPACNTR;
		pkg_sender->pipeconf_reg = PIPEACONF;
		pkg_sender->dsplinoff_reg = DSPALINOFF;
		pkg_sender->dspsurf_reg = DSPASURF;
		pkg_sender->pipestat_reg = PIPEASTAT;

		pkg_sender->mipi_intr_stat_reg = MIPIA_INTR_STAT_REG;
		pkg_sender->mipi_lp_gen_data_reg = MIPIA_LP_GEN_DATA_REG;
		pkg_sender->mipi_hs_gen_data_reg = MIPIA_HS_GEN_DATA_REG;
		pkg_sender->mipi_lp_gen_ctrl_reg = MIPIA_LP_GEN_CTRL_REG;
		pkg_sender->mipi_hs_gen_ctrl_reg = MIPIA_HS_GEN_CTRL_REG;
		pkg_sender->mipi_gen_fifo_stat_reg = MIPIA_GEN_FIFO_STAT_REG;
		pkg_sender->mipi_data_addr_reg = MIPIA_DATA_ADD_REG;
		pkg_sender->mipi_data_len_reg = MIPIA_DATA_LEN_REG;
		pkg_sender->mipi_cmd_addr_reg = MIPIA_CMD_ADD_REG;
		pkg_sender->mipi_cmd_len_reg = MIPIA_CMD_LEN_REG;
		pkg_sender->mipi_dpi_control_reg = MIPIA_DPI_CONTROL_REG;
	} else if (pipe == 2) {
		pkg_sender->dpll_reg = MRST_DPLL_A;
		pkg_sender->dspcntr_reg = DSPCCNTR;
		pkg_sender->pipeconf_reg = PIPECCONF;
		pkg_sender->dsplinoff_reg = DSPCLINOFF;
		pkg_sender->dspsurf_reg = DSPCSURF;
		pkg_sender->pipestat_reg = 72024;

		pkg_sender->mipi_intr_stat_reg =
			MIPIA_INTR_STAT_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_lp_gen_data_reg =
			MIPIA_LP_GEN_DATA_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_hs_gen_data_reg =
			MIPIA_HS_GEN_DATA_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_lp_gen_ctrl_reg =
			MIPIA_LP_GEN_CTRL_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_hs_gen_ctrl_reg =
			MIPIA_HS_GEN_CTRL_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_gen_fifo_stat_reg =
			MIPIA_GEN_FIFO_STAT_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_data_addr_reg =
			MIPIA_DATA_ADD_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_data_len_reg =
			MIPIA_DATA_LEN_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_cmd_addr_reg =
			MIPIA_CMD_ADD_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_cmd_len_reg =
			MIPIA_CMD_LEN_REG + MIPIC_REG_OFFSET;
		pkg_sender->mipi_dpi_control_reg =
			MIPIA_DPI_CONTROL_REG + MIPIC_REG_OFFSET;
	}

	/*init pkg list*/
	INIT_LIST_HEAD(&pkg_sender->pkg_list);
	INIT_LIST_HEAD(&pkg_sender->free_list);

	/*init lock*/
	mutex_init(&pkg_sender->lock);

	/*allocate free pkg pool*/
	for (i = 0; i < MDFLD_MAX_PKG_NUM; i++) {
		pkg = kzalloc(sizeof(struct mdfld_dsi_pkg), GFP_KERNEL);
		if (!pkg) {
			ret = -ENOMEM;
			goto pkg_alloc_err;
		}

		INIT_LIST_HEAD(&pkg->entry);

		/*append to free list*/
		list_add_tail(&pkg->entry, &pkg_sender->free_list);
	}

	/*init te & screen update seqs*/
	atomic64_set(&pkg_sender->te_seq, 0);
	atomic64_set(&pkg_sender->last_screen_update, 0);

	PSB_DEBUG_ENTRY("initialized\n");

	return 0;

pkg_alloc_err:
	list_for_each_entry_safe(pkg, tmp, &pkg_sender->free_list, entry) {
		list_del(&pkg->entry);
		kfree(pkg);
	}

	/*free mapped command buffer*/
	mdfld_dbi_cb_destroy(pkg_sender);
mapping_err:
	kfree(pkg_sender);
	dsi_connector->pkg_sender = NULL;

	return ret;
}

void mdfld_dsi_pkg_sender_destroy(struct mdfld_dsi_pkg_sender *sender)
{
	struct mdfld_dsi_pkg *pkg, *tmp;

	if (!sender || IS_ERR(sender))
		return;

	/*free pkg pool*/
	list_for_each_entry_safe(pkg, tmp, &sender->free_list, entry) {
		list_del(&pkg->entry);
		kfree(pkg);
	}

	/*free pkg list*/
	list_for_each_entry_safe(pkg, tmp, &sender->pkg_list, entry) {
		list_del(&pkg->entry);
		kfree(pkg);
	}

	/*free mapped command buffer*/
	mdfld_dbi_cb_destroy(sender);

	/*free*/
	kfree(sender);

	PSB_DEBUG_ENTRY("destroyed\n");
}
