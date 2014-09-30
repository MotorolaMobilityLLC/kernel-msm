/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 **************************************************************************/

#include <linux/compat.h>
#include <linux/ioctl.h>

#include <drm/drmP.h>
#include <drm/drm.h>
#include "psb_drm.h"
#include "drm_shared.h"
#include "psb_drv.h"
#include "psb_fb.h"
#include "psb_reg.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "psb_video_drv.h"

#ifdef SUPPORT_VSP
#include "vsp.h"
#endif

#include "tng_topaz.h"

#include <drm/drm_pciids.h>
#include "pwr_mgmt.h"
#include "psb_intel_display.h"
#include "mdfld_output.h"

#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>

#include <asm/intel_scu_ipc.h>
#include <asm/intel-mid.h>

#ifdef CONFIG_SUPPORT_MIPI
#include "dispmgrnl.h"
#include "mdfld_dsi_dbi.h"
#ifdef CONFIG_MID_DSI_DPU
#include "mdfld_dsi_dbi_dpu.h"
#endif
#include "mdfld_dsi_dbi_dsr.h"
/* SH DPST */
#include "psb_dpst_func.h"
#endif

#ifdef CONFIG_MID_HDMI
#include "mdfld_msic.h"
#endif
#include "psb_intel_hdmi.h"

#include "otm_hdmi.h"
#include "android_hdmi.h"
#include "bufferclass_interface.h"

#include "mdfld_hdcp.h"
#include "mdfld_csc.h"
#include "mrfld_s3d.h"

#include "pvr_bridge.h"

#include "mrfld_clock.h"
#include "mdfld_debugfs.h"

/* MaxFifo/ S0i1-Display */
#include "dc_maxfifo.h"
#define VBLANK_OFF_DELAY_DEFAULT	300

#define KEEP_UNUSED_CODE 0
#define KEEP_UNUSED_CODE_S3D 0

#define KEEP_UNUSED_CODE_DRIVER_DISPATCH 0

#define HDMI_MONITOR_NAME_LENGTH 20

/* Hack to Turn GFX islands up - BEGIN */
static void power_up(int pm_reg, u32 pm_mask);

static void power_up(int pm_reg, u32 pm_mask) {
	u32 pwr_mask = 0;

	pwr_mask = intel_mid_msgbus_read32(0x04, pm_reg);
	// pwr_mask &= ~pm_mask;
	pwr_mask =0;

	intel_mid_msgbus_write32(0x04, pm_reg, pwr_mask);
	udelay(10);
}
/* Hack to Turn GFX islands up - END */

int drm_psb_debug;
int drm_decode_flag = 0x0;
int drm_psb_enable_pr2_cabc = 1;
int drm_psb_enable_gamma;
int drm_psb_enable_color_conversion;
int drm_psb_set_gamma_success = 0;
int drm_psb_set_gamma_pending = 0;
int drm_psb_set_gamma_pipe = MDFLD_PIPE_MAX;
int gamma_setting_save[256] = {0};
int csc_setting_save[6] = {0};
/*EXPORT_SYMBOL(drm_psb_debug);*/
static int drm_psb_trap_pagefaults;

int drm_psb_no_fb;
int drm_psb_force_pipeb;
int drm_idle_check_interval = 5;
int drm_msvdx_pmpolicy = PSB_PMPOLICY_POWERDOWN;
int drm_psb_cpurelax;
int drm_psb_udelaydivider = 1;
int drm_topaz_pmpolicy = PSB_PMPOLICY_POWERDOWN;
int drm_vsp_pmpolicy = PSB_PMPOLICY_SUSPEND_HWIDLE;
int drm_topaz_cgpolicy = PSB_CGPOLICY_ON;
int drm_topaz_cmdpolicy = PSB_CMDPOLICY_PARALLEL;
int drm_topaz_sbuswa;
int drm_psb_ospm = 1;
int drm_psb_dsr;
int drm_psb_gfx_pm;
int drm_psb_vsp_pm;
int drm_psb_ved_pm;
int drm_psb_vec_pm;
int drm_psb_topaz_clockgating;
int gfxrtdelay = 2 * 1000;
int drm_psb_3D_vblank;
int drm_psb_smart_vsync = 1;
int drm_psb_te_timer_delay = (DRM_HZ / 40);
char HDMI_EDID[HDMI_MONITOR_NAME_LENGTH];
int hdmi_state;
u32 DISP_PLANEB_STATUS = ~DISPLAY_PLANE_ENABLE;
int drm_psb_msvdx_tiling = 1;
int drm_msvdx_bottom_half;
int drm_hdmi_hpd_auto;
#ifdef CONFIG_SUPPORT_MIPI
int default_hdmi_scaling_mode = DRM_MODE_SCALE_CENTER;
#else
int default_hdmi_scaling_mode = DRM_MODE_SCALE_ASPECT;
#endif
#ifdef CONFIG_ITE_HDMI_CEC
int hdmi_edid_src_phy_addr = 0;
#endif
int drm_vsp_burst = 1;
int drm_vsp_force_up_freq = 0;
int drm_vsp_force_down_freq = 0;
int drm_vsp_single_int = 0;
int drm_vec_force_up_freq = 0;
int drm_vec_force_down_freq = 0;
int drm_vsp_vpp_batch_cmd = 1;
int drm_video_sepkey = -1;
int gamma_setting[129] = {0};
int csc_setting[6] = {0};
int gamma_number = 129;
int csc_number = 6;

static int psb_probe(struct pci_dev *pdev, const struct pci_device_id *ent);

#ifdef CONFIG_COMPAT
static long psb_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#endif

MODULE_PARM_DESC(debug, "Enable debug output");
MODULE_PARM_DESC(no_fb, "Disable FBdev");
MODULE_PARM_DESC(trap_pagefaults, "Error and reset on MMU pagefaults");
MODULE_PARM_DESC(disable_vsync, "Disable vsync interrupts");
MODULE_PARM_DESC(force_pipeb, "Forces PIPEB to become primary fb");
MODULE_PARM_DESC(ta_mem_size, "TA memory size in kiB");
MODULE_PARM_DESC(ospm, "switch for ospm support");
MODULE_PARM_DESC(rtpm, "Specifies Runtime PM delay for GFX");
MODULE_PARM_DESC(msvdx_pmpolicy, "msvdx power management policy btw frames");
MODULE_PARM_DESC(topaz_pmpolicy, "topaz power managerment policy btw frames");
MODULE_PARM_DESC(vsp_pmpolicy, "vsp power managerment policy btw frames");
MODULE_PARM_DESC(topaz_cgpolicy, "disable VEC and GFX clock gating");
MODULE_PARM_DESC(topaz_cmdpolicy, "execute cmd in parallel mode");
MODULE_PARM_DESC(topaz_sbuswa, "WA for topaz sysbus write");
MODULE_PARM_DESC(PanelID, "Panel info for querying");
MODULE_PARM_DESC(hdmi_edid, "EDID info for HDMI monitor");
MODULE_PARM_DESC(hdmi_state, "Whether HDMI Monitor is connected or not");
MODULE_PARM_DESC(vblank_sync,
		 "whether sync to vblank interrupt when do 3D flip");
MODULE_PARM_DESC(smart_vsync, "Enable Smart Vsync for Display");
MODULE_PARM_DESC(te_delay, "swap delay after TE interrpt");
MODULE_PARM_DESC(cpu_relax, "replace udelay with cpu_relax for video");
MODULE_PARM_DESC(udelay_divider, "divide the usec value of video udelay");
MODULE_PARM_DESC(vsp_pm, "Power on/off the VSP");
MODULE_PARM_DESC(ved_pm, "Power on/off the Msvdx");
MODULE_PARM_DESC(vec_pm, "Power on/off the Topaz");
MODULE_PARM_DESC(hdmi_hpd_auto, "HDMI hot-plug auto test flag");
MODULE_PARM_DESC(default_hdmi_scaling_mode, "Default HDMI scaling mode");
#ifdef CONFIG_ITE_HDMI_CEC
MODULE_PARM_DESC(hdmi_edid_src_phy_addr, "HDMI edid for HDMI CEC HAL");
#endif
MODULE_PARM_DESC(vsp_burst, "VSP burst mode enable");
MODULE_PARM_DESC(vsp_force_up_freq, "force VSP running at certain freq");
MODULE_PARM_DESC(vsp_force_down_freq, "force VSP power down at certain freq");
MODULE_PARM_DESC(vsp_single_int, "force VSP VPP generate one irq per command group");
MODULE_PARM_DESC(vec_force_up_freq, "force VEC running at certain freq");
MODULE_PARM_DESC(vec_force_down_freq, "force VEC power down at certain freq");
MODULE_PARM_DESC(vsp_vpp_batch_cmd, "set vsp vpp for batch cmd submit");
MODULE_PARM_DESC(video_sepkey, "Force sepapp to use specified key index to verify ved/vec/vsp firmware");

module_param_named(enable_color_conversion, drm_psb_enable_color_conversion,
					int, 0600);
module_param_named(enable_gamma, drm_psb_enable_gamma, int, 0600);
module_param_named(debug, drm_psb_debug, int, 0600);
module_param_named(psb_enable_pr2_cabc, drm_psb_enable_pr2_cabc, int, 0600);
module_param_named(no_fb, drm_psb_no_fb, int, 0600);
module_param_named(trap_pagefaults, drm_psb_trap_pagefaults, int, 0600);
module_param_named(force_pipeb, drm_psb_force_pipeb, int, 0600);
module_param_named(msvdx_pmpolicy, drm_msvdx_pmpolicy, int, 0600);
module_param_named(cpu_relax, drm_psb_cpurelax, int, 0600);
module_param_named(udelay_divider, drm_psb_udelaydivider, int, 0600);
module_param_named(topaz_pmpolicy, drm_topaz_pmpolicy, int, 0600);
module_param_named(vsp_pmpolicy, drm_vsp_pmpolicy, int, 0600);
module_param_named(topaz_cgpolicy, drm_topaz_cgpolicy, int, 0600);
module_param_named(topaz_cmdpolicy, drm_topaz_cmdpolicy, int, 0600);
module_param_named(msvdx_bottom_half, drm_msvdx_bottom_half, int, 0600);
module_param_named(topaz_sbuswa, drm_topaz_sbuswa, int, 0600);
module_param_named(ospm, drm_psb_ospm, int, 0600);
module_param_named(dsr, drm_psb_dsr, int, 0600);
module_param_named(gfx_pm, drm_psb_gfx_pm, int, 0600);
module_param_named(vsp_pm, drm_psb_vsp_pm, int, 0600);
module_param_named(ved_pm, drm_psb_ved_pm, int, 0600);
module_param_named(vec_pm, drm_psb_vec_pm, int, 0600);
module_param_named(msvdx_tiling_memory, drm_psb_msvdx_tiling, int, 0600);
module_param_named(rtpm, gfxrtdelay, int, 0600);
module_param_named(topaz_clockgating, drm_psb_topaz_clockgating, int, 0600);
module_param_named(PanelID, PanelID, int, 0600);
module_param_string(hdmi_edid, HDMI_EDID, 20, 0600);
module_param_named(hdmi_state, hdmi_state, int, 0600);
module_param_named(vblank_sync, drm_psb_3D_vblank, int, 0600);
module_param_named(smart_vsync, drm_psb_smart_vsync, int, 0600);
module_param_named(te_delay, drm_psb_te_timer_delay, int, 0600);
#ifdef CONFIG_SLICE_HEADER_PARSING
module_param_named(decode_flag, drm_decode_flag, int, 0600);
#endif
module_param_named(hdmi_hpd_auto, drm_hdmi_hpd_auto, int, 0600);
module_param_named(default_hdmi_scaling_mode, default_hdmi_scaling_mode,
					int, 0600);
#ifdef CONFIG_ITE_HDMI_CEC
module_param_named(hdmi_edid_src_phy_addr, hdmi_edid_src_phy_addr, int, 0600);
#endif
module_param_named(vsp_burst, drm_vsp_burst, int, 0600);
module_param_named(vsp_force_up_freq, drm_vsp_force_up_freq, int, 0600);
module_param_named(vsp_force_down_freq, drm_vsp_force_down_freq, int, 0600);
module_param_named(vsp_single_int, drm_vsp_single_int, int, 0600);
module_param_named(vec_force_up_freq, drm_vec_force_up_freq, int, 0600);
module_param_named(vec_force_down_freq, drm_vec_force_down_freq, int, 0600);
module_param_named(vsp_vpp_batch_cmd, drm_vsp_vpp_batch_cmd, int, 0600);
module_param_named(video_sepkey, drm_video_sepkey, int, 0600);
module_param_array_named(gamma_adjust, gamma_setting, int, &gamma_number, 0600);
module_param_array_named(csc_adjust, csc_setting, int, &csc_number, 0600);

#ifndef MODULE
/* Make ospm configurable via cmdline firstly,
* and others can be enabled if needed.
*/
static int __init config_ospm(char *arg)
{
	/* ospm turn on/off control can be passed in as a cmdline parameter */
	/* to enable this feature add ospm=1 to cmdline */
	/* to disable this feature add ospm=0 to cmdline */
	if (!arg)
		return -EINVAL;

	if (!strcasecmp(arg, "0"))
		drm_psb_ospm = 0;
	else if (!strcasecmp(arg, "1"))
		drm_psb_ospm = 1;

	return 0;
}

static int __init config_dsr(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (!strcasecmp(arg, "0"))
		drm_psb_dsr = 0;
	else if (!strcasecmp(arg, "1"))
		drm_psb_dsr = 1;

	return 0;
}

early_param("ospm", config_ospm);
early_param("dsr", config_dsr);
#endif

static struct pci_device_id pciidlist[] = {
	{0x8086, 0x1180, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1181, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1182, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1183, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1184, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1185, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1186, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1187, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1180},
	{0x8086, 0x1480, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CHIP_MRFLD_1480},
	{0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, pciidlist);
/*
 * Standard IOCTLs.
 */

#define DRM_IOCTL_PSB_KMS_OFF	\
		DRM_IO(DRM_PSB_KMS_OFF + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_KMS_ON	\
		DRM_IO(DRM_PSB_KMS_ON + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_VT_LEAVE	\
		DRM_IO(DRM_PSB_VT_LEAVE + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_VT_ENTER	\
		DRM_IO(DRM_PSB_VT_ENTER + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_EXTENSION	\
		DRM_IOWR(DRM_PSB_EXTENSION + DRM_COMMAND_BASE, \
			 union drm_psb_extension_arg)
#define DRM_IOCTL_PSB_SIZES	\
		DRM_IOR(DRM_PSB_SIZES + DRM_COMMAND_BASE, \
			struct drm_psb_sizes_arg)
#define DRM_IOCTL_PSB_FUSE_REG	\
		DRM_IOWR(DRM_PSB_FUSE_REG + DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_VBT	\
		DRM_IOWR(DRM_PSB_VBT + DRM_COMMAND_BASE, \
			struct gct_ioctl_arg)
#define DRM_IOCTL_PSB_DC_STATE	\
		DRM_IOW(DRM_PSB_DC_STATE + DRM_COMMAND_BASE, \
			struct drm_psb_dc_state_arg)
#define DRM_IOCTL_PSB_ADB	\
		DRM_IOWR(DRM_PSB_ADB + DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_MODE_OPERATION	\
		DRM_IOWR(DRM_PSB_MODE_OPERATION + DRM_COMMAND_BASE, \
			 struct drm_psb_mode_operation_arg)
#define DRM_IOCTL_PSB_STOLEN_MEMORY	\
		DRM_IOWR(DRM_PSB_STOLEN_MEMORY + DRM_COMMAND_BASE, \
			 struct drm_psb_stolen_memory_arg)
#define DRM_IOCTL_PSB_REGISTER_RW	\
		DRM_IOWR(DRM_PSB_REGISTER_RW + DRM_COMMAND_BASE, \
			 struct drm_psb_register_rw_arg)
#define DRM_IOCTL_PSB_GTT_MAP	\
		DRM_IOWR(DRM_PSB_GTT_MAP + DRM_COMMAND_BASE, \
			 struct psb_gtt_mapping_arg)
#define DRM_IOCTL_PSB_GTT_UNMAP	\
		DRM_IOW(DRM_PSB_GTT_UNMAP + DRM_COMMAND_BASE, \
			struct psb_gtt_mapping_arg)
#define DRM_IOCTL_PSB_GETPAGEADDRS	\
		DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_GETPAGEADDRS,\
			 struct drm_psb_getpageaddrs_arg)
#define DRM_IOCTL_PSB_HIST_ENABLE	\
		DRM_IOWR(DRM_PSB_HIST_ENABLE + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_HIST_STATUS	\
		DRM_IOWR(DRM_PSB_HIST_STATUS + DRM_COMMAND_BASE, \
			 struct drm_psb_hist_status_arg)
#define DRM_IOCTL_PSB_UPDATE_GUARD	\
		DRM_IOWR(DRM_PSB_UPDATE_GUARD + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_INIT_COMM	\
		DRM_IOWR(DRM_PSB_INIT_COMM + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_DPST	\
		DRM_IOWR(DRM_PSB_DPST + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_GAMMA	\
		DRM_IOWR(DRM_PSB_GAMMA + DRM_COMMAND_BASE, \
			 struct drm_psb_dpst_lut_arg)
#define DRM_IOCTL_PSB_DPST_BL	\
		DRM_IOWR(DRM_PSB_DPST_BL + DRM_COMMAND_BASE, \
			 uint32_t)
#define DRM_IOCTL_PSB_GET_PIPE_FROM_CRTC_ID	\
		DRM_IOWR(DRM_PSB_GET_PIPE_FROM_CRTC_ID + DRM_COMMAND_BASE, \
			 struct drm_psb_get_pipe_from_crtc_id_arg)

/*DPU/DSR stuff*/
#define DRM_IOCRL_PSB_DPU_QUERY \
	DRM_IOR(DRM_PSB_DPU_QUERY + DRM_COMMAND_BASE, unsigned int)
#define DRM_IOCRL_PSB_DPU_DSR_ON \
	DRM_IOW(DRM_PSB_DPU_DSR_ON + DRM_COMMAND_BASE, unsigned int)
/* #define DRM_IOCRL_PSB_DPU_DSR_OFF \
*	DRM_IOW(DRM_PSB_DPU_DSR_OFF + DRM_COMMAND_BASE, unsigned int)
*/
#define DRM_IOCRL_PSB_DPU_DSR_OFF \
	DRM_IOW(DRM_PSB_DPU_DSR_OFF + DRM_COMMAND_BASE, \
	struct drm_psb_drv_dsr_off_arg)

/*HDMI FB stuff*/
#define DRM_IOCTL_PSB_HDMI_FB_CMD \
	DRM_IOWR(DRM_PSB_HDMI_FB_CMD + DRM_COMMAND_BASE, \
	struct drm_psb_disp_ctrl)

/* HDCP IOCTLs */
#define DRM_IOCTL_PSB_QUERY_HDCP \
		DRM_IOR(DRM_PSB_QUERY_HDCP + DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_VALIDATE_HDCP_KSV \
	DRM_IOWR(DRM_PSB_VALIDATE_HDCP_KSV + DRM_COMMAND_BASE, sqword_tt)
#define DRM_IOCTL_PSB_GET_HDCP_STATUS \
		DRM_IOR(DRM_PSB_GET_HDCP_STATUS + DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_ENABLE_HDCP \
		DRM_IO(DRM_PSB_ENABLE_HDCP + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_DISABLE_HDCP \
		DRM_IO(DRM_PSB_DISABLE_HDCP + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_GET_HDCP_LINK_STATUS \
		DRM_IOR(DRM_PSB_GET_HDCP_LINK_STATUS + \
		DRM_COMMAND_BASE, uint32_t)
#define DRM_IOCTL_PSB_HDCP_DISPLAY_IED_OFF \
		DRM_IO(DRM_PSB_HDCP_DISPLAY_IED_OFF + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_HDCP_DISPLAY_IED_ON \
		DRM_IO(DRM_PSB_HDCP_DISPLAY_IED_ON + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_QUERY_HDCP_DISPLAY_IED_CAPS \
		DRM_IOR(DRM_PSB_QUERY_HDCP_DISPLAY_IED_CAPS \
			+ DRM_COMMAND_BASE, uint32_t)

/* S3D IOCTLs */
#define DRM_IOCTL_PSB_S3D_QUERY \
	DRM_IOWR(DRM_PSB_S3D_QUERY + DRM_COMMAND_BASE, \
		struct drm_psb_s3d_query)
#define DRM_IOCTL_PSB_S3D_PREMODESET \
	DRM_IOW(DRM_PSB_S3D_PREMODESET + DRM_COMMAND_BASE, \
		struct drm_psb_s3d_premodeset)
#define DRM_IOCTL_PSB_S3D_ENABLE \
		DRM_IOW(DRM_PSB_S3D_ENABLE + DRM_COMMAND_BASE, \
		uint32_t)

/* CSC IOCTLS */
#define DRM_IOCTL_PSB_SET_CSC \
	DRM_IOW(DRM_PSB_SET_CSC + DRM_COMMAND_BASE, struct drm_psb_csc_matrix)

/*CSC GAMMA Setting*/
#define DRM_IOCTL_PSB_CSC_GAMMA_SETTING \
                DRM_IOWR(DRM_PSB_CSC_GAMMA_SETTING + DRM_COMMAND_BASE, struct drm_psb_csc_gamma_setting)

#define DRM_IOCTL_PSB_ENABLE_IED_SESSION \
		DRM_IO(DRM_PSB_ENABLE_IED_SESSION + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_DISABLE_IED_SESSION \
		DRM_IO(DRM_PSB_DISABLE_IED_SESSION + DRM_COMMAND_BASE)

/* Panel type query */
#define DRM_IOCTL_PSB_PANEL_QUERY \
		DRM_IOR(DRM_PSB_PANEL_QUERY + DRM_COMMAND_BASE, \
			uint32_t)

/* Idle control */
#define DRM_IOCTL_PSB_IDLE_CTRL \
		DRM_IOW(DRM_PSB_IDLE_CTRL + DRM_COMMAND_BASE, \
			struct drm_psb_idle_ctrl)

/*
 * TTM execbuf extension.
 */

#define DRM_PSB_SCENE_UNREF	  (DRM_PSB_CMDBUF + 1)
#define DRM_IOCTL_PSB_CMDBUF	\
		DRM_IOW(DRM_PSB_CMDBUF + DRM_COMMAND_BASE,	\
			struct drm_psb_cmdbuf_arg)
#define DRM_IOCTL_PSB_SCENE_UNREF	\
		DRM_IOW(DRM_PSB_SCENE_UNREF + DRM_COMMAND_BASE, \
			struct drm_psb_scene)
#define DRM_IOCTL_PSB_KMS_OFF	  DRM_IO(DRM_PSB_KMS_OFF + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_KMS_ON	  DRM_IO(DRM_PSB_KMS_ON + DRM_COMMAND_BASE)
#define DRM_IOCTL_PSB_EXTENSION	\
		DRM_IOWR(DRM_PSB_EXTENSION + DRM_COMMAND_BASE, \
			 union drm_psb_extension_arg)
/*
 * TTM placement user extension.
 */

#define DRM_PSB_PLACEMENT_OFFSET   (DRM_PSB_SCENE_UNREF + 1)

#define DRM_PSB_TTM_PL_CREATE	 (TTM_PL_CREATE + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_REFERENCE (TTM_PL_REFERENCE + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_UNREF	 (TTM_PL_UNREF + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_SYNCCPU	 (TTM_PL_SYNCCPU + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_WAITIDLE  (TTM_PL_WAITIDLE + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_SETSTATUS (TTM_PL_SETSTATUS + DRM_PSB_PLACEMENT_OFFSET)
#define DRM_PSB_TTM_PL_CREATE_UB (TTM_PL_CREATE_UB + DRM_PSB_PLACEMENT_OFFSET)

/*
 * TTM fence extension.
 */

#define DRM_PSB_FENCE_OFFSET	   (DRM_PSB_TTM_PL_CREATE_UB + 1)
#define DRM_PSB_TTM_FENCE_SIGNALED (TTM_FENCE_SIGNALED + DRM_PSB_FENCE_OFFSET)
#define DRM_PSB_TTM_FENCE_FINISH   (TTM_FENCE_FINISH + DRM_PSB_FENCE_OFFSET)
#define DRM_PSB_TTM_FENCE_UNREF    (TTM_FENCE_UNREF + DRM_PSB_FENCE_OFFSET)

#define DRM_PSB_FLIP	   (DRM_PSB_TTM_FENCE_UNREF + 1)
		/*20 */
/* PSB video extension */
#define DRM_PSB_VIDEO_GETPARAM		(DRM_PSB_FLIP + 1)

/*BC_VIDEO ioctl*/
#define DRM_BUFFER_CLASS_VIDEO      (DRM_PSB_VIDEO_GETPARAM + 1)
	/*0x32 */

#define DRM_IOCTL_PSB_TTM_PL_CREATE    \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_CREATE,\
		 union ttm_pl_create_arg)
#define DRM_IOCTL_PSB_TTM_PL_REFERENCE \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_REFERENCE,\
		 union ttm_pl_reference_arg)
#define DRM_IOCTL_PSB_TTM_PL_UNREF    \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_UNREF,\
		struct ttm_pl_reference_req)
#define DRM_IOCTL_PSB_TTM_PL_SYNCCPU	\
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_SYNCCPU,\
		struct ttm_pl_synccpu_arg)
#define DRM_IOCTL_PSB_TTM_PL_WAITIDLE	 \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_WAITIDLE,\
		struct ttm_pl_waitidle_arg)
#define DRM_IOCTL_PSB_TTM_PL_SETSTATUS \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_SETSTATUS,\
		 union ttm_pl_setstatus_arg)
#define DRM_IOCTL_PSB_TTM_PL_CREATE_UB    \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_PL_CREATE_UB,\
		 union ttm_pl_create_ub_arg)
#define DRM_IOCTL_PSB_TTM_FENCE_SIGNALED \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_FENCE_SIGNALED,	\
		  union ttm_fence_signaled_arg)
#define DRM_IOCTL_PSB_TTM_FENCE_FINISH \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_TTM_FENCE_FINISH,	\
		 union ttm_fence_finish_arg)
#define DRM_IOCTL_PSB_TTM_FENCE_UNREF \
	DRM_IOW(DRM_COMMAND_BASE + DRM_PSB_TTM_FENCE_UNREF,	\
		 struct ttm_fence_unref_arg)
#define DRM_IOCTL_PSB_FLIP \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_FLIP, \
		 struct drm_psb_pageflip_arg)
#define DRM_IOCTL_PSB_VIDEO_GETPARAM \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PSB_VIDEO_GETPARAM, \
		 struct drm_lnc_video_getparam_arg)

    /*****************************
     *  HDMI TEST IOCTLs
     */
#define DRM_IOCTL_PSB_HDMITEST    \
	DRM_IOWR(DRM_PSB_HDMITEST + DRM_COMMAND_BASE, drm_psb_hdmireg_t)

/* VSYNC IOCTL */
#define DRM_IOCTL_PSB_VSYNC_SET \
	DRM_IOWR(DRM_PSB_VSYNC_SET + DRM_COMMAND_BASE,		\
			struct drm_psb_vsync_set_arg)

#define DRM_IOCTL_PSB_PANEL_ORIENTATION \
	DRM_IOR(DRM_PSB_PANEL_ORIENTATION + DRM_COMMAND_BASE,          \
			int)

#define DRM_IOCTL_PSB_UPDATE_CURSOR_POS \
	DRM_IOW(DRM_PSB_UPDATE_CURSOR_POS + DRM_COMMAND_BASE,\
			struct intel_dc_cursor_ctx)


struct user_printk_arg {
	char string[512];
};
#define DRM_IOCTL_USER_PRINTK \
	DRM_IOWR(DRM_COMMAND_BASE + DRM_PVR_RESERVED2, \
		 struct user_printk_arg)

static int psb_vt_leave_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_vt_enter_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_sizes_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
static int psb_fuse_reg_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_vbt_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
static int psb_dc_state_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
static int psb_adb_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv);
static int psb_mode_operation_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
static int psb_stolen_memory_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv);
static int psb_vsync_set_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_register_rw_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
#ifdef CONFIG_SUPPORT_MIPI
static int psb_hist_enable_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_hist_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_update_guard_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);
static int psb_init_comm_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
static int psb_dpst_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);
static int psb_gamma_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv);
static int psb_dpst_bl_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
static int psb_dpu_query_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
static int psb_dpu_dsr_on_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
static int psb_dpu_dsr_off_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
#endif
static int psb_get_panel_orientation_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
#ifdef CONFIG_SUPPORT_HDMI
static int psb_disp_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv);

static int psb_query_hdcp_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_validate_hdcp_ksv_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_get_hdcp_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_enable_hdcp_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_disable_hdcp_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_get_hdcp_link_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_enable_display_ied_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_disable_display_ied_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_query_display_ied_caps_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);

#if KEEP_UNUSED_CODE_S3D
static int psb_s3d_query_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv);
static int psb_s3d_premodeset_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);
static int psb_s3d_enable_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
#endif /* if KEEP_UNUSED_CODE_S3D */
#endif
static int psb_set_csc_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv);
static int psb_csc_gamma_setting_ioctl(struct drm_device *dev, void *data,
                                 struct drm_file *file_priv);
static int psb_enable_ied_session_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_disable_ied_session_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
static int psb_panel_query_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv);
static int psb_idle_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
static int psb_update_cursor_pos_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);

static int user_printk_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	struct user_printk_arg *user_string = data;
	printk(KERN_ERR "%s", user_string->string);
	return 0;
}

    /****************************
     *  HDMI TEST IOCTLS
     */
static int psb_drm_hdmi_test_ioctl(struct drm_device *,
				   void *, struct drm_file *);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
#define PSB_IOCTL_DEF(ioctl, func, flags) \
	 [DRM_IOCTL_NR(ioctl) - DRM_COMMAND_BASE] = \
	 {ioctl, flags, func}
#else
#define PSB_IOCTL_DEF(ioctl, func, flags) \
	 [DRM_IOCTL_NR(ioctl) - DRM_COMMAND_BASE] = \
	 {ioctl, flags, func, ioctl}
#endif

static struct drm_ioctl_desc psb_ioctls[] = {
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_KMS_OFF, psbfb_kms_off_ioctl,
		      DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_KMS_ON,
		      psbfb_kms_on_ioctl,
		      DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VT_LEAVE, psb_vt_leave_ioctl,
		      DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VT_ENTER,
		      psb_vt_enter_ioctl,
		      DRM_ROOT_ONLY),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_EXTENSION, psb_extension_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_SIZES, psb_sizes_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_FUSE_REG, psb_fuse_reg_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VBT, psb_vbt_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DC_STATE, psb_dc_state_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_ADB, psb_adb_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_MODE_OPERATION, psb_mode_operation_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_STOLEN_MEMORY, psb_stolen_memory_ioctl,
		      DRM_AUTH),
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_REGISTER_RW, psb_register_rw_ioctl,
		      DRM_AUTH),
#else
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_REGISTER_RW, psb_register_rw_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
#endif
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GTT_MAP,
		      psb_gtt_map_meminfo_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GTT_UNMAP,
		      psb_gtt_unmap_meminfo_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GETPAGEADDRS,
		      psb_getpageaddrs_ioctl,
		      DRM_AUTH),
#ifdef CONFIG_SUPPORT_MIPI
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HIST_ENABLE,
		      psb_hist_enable_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HIST_STATUS,
		      psb_hist_status_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_UPDATE_GUARD, psb_update_guard_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_INIT_COMM, psb_init_comm_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DPST, psb_dpst_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GAMMA, psb_gamma_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DPST_BL, psb_dpst_bl_ioctl, DRM_AUTH),
#endif
#if 0
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GET_PIPE_FROM_CRTC_ID,
		      psb_intel_get_pipe_from_crtc_id, 0),
#endif
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_CMDBUF,
		      psb_cmdbuf_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	/*to be removed later */
	/*PSB_IOCTL_DEF(DRM_IOCTL_PSB_SCENE_UNREF, drm_psb_scene_unref_ioctl,
	   DRM_AUTH), */

	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_CREATE, psb_pl_create_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_REFERENCE, psb_pl_reference_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_UNREF, psb_pl_unref_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_SYNCCPU, psb_pl_synccpu_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_WAITIDLE, psb_pl_waitidle_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_SETSTATUS, psb_pl_setstatus_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_PL_CREATE_UB, psb_pl_ub_create_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_FENCE_SIGNALED,
		      psb_fence_signaled_ioctl, DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_FENCE_FINISH, psb_fence_finish_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_TTM_FENCE_UNREF, psb_fence_unref_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	/*to be removed later */
	/*PSB_IOCTL_DEF(DRM_IOCTL_PSB_FLIP, psb_page_flip, DRM_AUTH), */
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VIDEO_GETPARAM,
		psb_video_getparam, DRM_AUTH | DRM_UNLOCKED),
#ifdef CONFIG_SUPPORT_MIPI
	PSB_IOCTL_DEF(DRM_IOCRL_PSB_DPU_QUERY, psb_dpu_query_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCRL_PSB_DPU_DSR_ON, psb_dpu_dsr_on_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCRL_PSB_DPU_DSR_OFF, psb_dpu_dsr_off_ioctl,
		      DRM_AUTH),
#endif
#ifdef CONFIG_SUPPORT_HDMI
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HDMI_FB_CMD, psb_disp_ioctl, 0),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_QUERY_HDCP, psb_query_hdcp_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VALIDATE_HDCP_KSV,
		      psb_validate_hdcp_ksv_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GET_HDCP_STATUS, psb_get_hdcp_status_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_ENABLE_HDCP, psb_enable_hdcp_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DISABLE_HDCP, psb_disable_hdcp_ioctl,
		      DRM_AUTH | DRM_UNLOCKED),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_GET_HDCP_LINK_STATUS,
			psb_get_hdcp_link_status_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HDCP_DISPLAY_IED_OFF,
			psb_disable_display_ied_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HDCP_DISPLAY_IED_ON,
			psb_enable_display_ied_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_QUERY_HDCP_DISPLAY_IED_CAPS,
			psb_query_display_ied_caps_ioctl, DRM_AUTH),
#endif
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_SET_CSC, psb_set_csc_ioctl, DRM_AUTH),
        PSB_IOCTL_DEF(DRM_IOCTL_PSB_CSC_GAMMA_SETTING, psb_csc_gamma_setting_ioctl, DRM_AUTH),
/*
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_S3D_QUERY, psb_s3d_query_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_S3D_PREMODESET, psb_s3d_premodeset_ioctl,
		      DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_S3D_ENABLE, psb_s3d_enable_ioctl, DRM_AUTH),
*/
    /*****************************
     *  HDMI TEST IOCTLs
     */
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_HDMITEST, psb_drm_hdmi_test_ioctl,
		      DRM_AUTH),

	PSB_IOCTL_DEF(DRM_IOCTL_PSB_VSYNC_SET, psb_vsync_set_ioctl,
			DRM_AUTH | DRM_UNLOCKED),

	PSB_IOCTL_DEF(DRM_IOCTL_USER_PRINTK, user_printk_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_ENABLE_IED_SESSION,
		psb_enable_ied_session_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_DISABLE_IED_SESSION,
		psb_disable_ied_session_ioctl, DRM_AUTH),

	PSB_IOCTL_DEF(DRM_IOCTL_PSB_PANEL_QUERY,
		psb_panel_query_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_IDLE_CTRL,
		psb_idle_ioctl, DRM_AUTH),

	PSB_IOCTL_DEF(DRM_IOCTL_PSB_PANEL_ORIENTATION,
		psb_get_panel_orientation_ioctl, DRM_AUTH),
	PSB_IOCTL_DEF(DRM_IOCTL_PSB_UPDATE_CURSOR_POS,
		psb_update_cursor_pos_ioctl, DRM_AUTH),
};

static void psb_set_uopt(struct drm_psb_uopt *uopt)
{
	return;
}

static void psb_lastclose(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct msvdx_private *msvdx_priv;
#ifdef SUPPORT_VSP
	struct vsp_private *vsp_priv;
#endif

	if (!dev_priv)
		return;

	msvdx_priv = dev_priv->msvdx_private;
#ifdef SUPPORT_VSP
	vsp_priv = dev_priv->vsp_private;
#endif
	mutex_lock(&dev_priv->cmdbuf_mutex);
	if (dev_priv->encode_context.buffers) {
		vfree(dev_priv->encode_context.buffers);
		dev_priv->encode_context.buffers = NULL;
	}
	mutex_unlock(&dev_priv->cmdbuf_mutex);

	if (msvdx_priv) {
		mutex_lock(&msvdx_priv->msvdx_mutex);
		if (dev_priv->decode_context.buffers) {
			vfree(dev_priv->decode_context.buffers);
			dev_priv->decode_context.buffers = NULL;
		}
		mutex_unlock(&msvdx_priv->msvdx_mutex);
	}

#ifdef SUPPORT_VSP
	mutex_lock(&vsp_priv->vsp_mutex);
	if (dev_priv->vsp_context.buffers) {
		vfree(dev_priv->vsp_context.buffers);
		dev_priv->vsp_context.buffers = NULL;
	}
	mutex_unlock(&vsp_priv->vsp_mutex);
#endif
}

static void psb_do_takedown(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct ttm_bo_device *bdev = &dev_priv->bdev;

	if (dev_priv->have_mem_mmu) {
		ttm_bo_clean_mm(bdev, DRM_PSB_MEM_MMU);
		dev_priv->have_mem_mmu = 0;
	}

	if (dev_priv->have_tt) {
		ttm_bo_clean_mm(bdev, TTM_PL_TT);
		dev_priv->have_tt = 0;
	}

	psb_msvdx_uninit(dev);

#ifdef SUPPORT_VSP
	vsp_deinit(dev);
#endif
	tng_topaz_uninit(dev);
}

#if KEEP_UNUSED_CODE
static void psb_get_core_freq(struct drm_device *dev)
{
	uint32_t clock;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;

	/*pci_write_config_dword(pci_root, 0xD4, 0x00C32004); */
	/*pci_write_config_dword(pci_root, 0xD0, 0xE0033000); */

	clock = intel_mid_msgbus_read32_raw(0xD00503F0);

	switch (clock & 0x07) {
	case 0:
		dev_priv->core_freq = 100;
		break;
	case 1:
		dev_priv->core_freq = 133;
		break;
	case 2:
		dev_priv->core_freq = 150;
		break;
	case 3:
		dev_priv->core_freq = 178;
		break;
	case 4:
		dev_priv->core_freq = 200;
		break;
	case 5:
	case 6:
	case 7:
		dev_priv->core_freq = 266;
	default:
		dev_priv->core_freq = 0;
	}
}
#endif /* if KEEP_UNUSED_CODE */

#define FB_REG06_MRST 0xD08106F0
#define FB_REG06_MDFLD 0x108106F0
#define FB_TOPAZ_DISABLE BIT0
#define FB_MIPI_DISABLE  BIT11
#define FB_REG09_MRST 0xD08109F0
#define FB_REG09_MDFLD 0x108109F0
#define FB_SKU_MASK  (BIT12|BIT13|BIT14)
#define FB_SKU_SHIFT 12
#define FB_SKU_100 0
#define FB_SKU_100L 1
#define FB_SKU_83 2
#define FB_GFX_CLK_DIVIDE_MASK	(BIT20|BIT21|BIT22)
#define FB_GFX_CLK_DIVIDE_SHIFT 20
#define FB_VED_CLK_DIVIDE_MASK	(BIT23|BIT24)
#define FB_VED_CLK_DIVIDE_SHIFT 23
#define FB_VEC_CLK_DIVIDE_MASK	(BIT25|BIT26)
#define FB_VEC_CLK_DIVIDE_SHIFT 25

void mrst_get_fuse_settings(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	uint32_t fuse_value = 0;
	uint32_t fuse_value_tmp = 0;

	fuse_value = intel_mid_msgbus_read32_raw(IS_MDFLD(dev) ?
			FB_REG06_MDFLD : FB_REG06_MRST);

	dev_priv->iLVDS_enable = fuse_value & FB_MIPI_DISABLE;

	if (IS_FLDS(dev))
		dev_priv->iLVDS_enable = 0;

	PSB_DEBUG_ENTRY("internal display is %s\n",
			dev_priv->iLVDS_enable ? "LVDS display" :
			"MIPI display");

	/*prevent Runtime suspend at start */
	if (dev_priv->iLVDS_enable) {
		dev_priv->is_lvds_on = true;
		dev_priv->is_mipi_on = false;
	} else {
		dev_priv->is_mipi_on = true;
		dev_priv->is_lvds_on = false;
	}

	if (dev_priv->dev->pci_device == PCI_ID_TOPAZ_DISABLED)
		dev_priv->topaz_disabled = 1;
	else
		dev_priv->topaz_disabled = 0;

	dev_priv->video_device_fuse = fuse_value;

	PSB_DEBUG_ENTRY("topaz is %s\n",
			dev_priv->topaz_disabled ? "disabled" : "enabled");

	fuse_value = intel_mid_msgbus_read32_raw(IS_MDFLD(dev) ?
			FB_REG09_MDFLD : FB_REG09_MRST);

	PSB_DEBUG_ENTRY("SKU values is 0x%x.\n", fuse_value);
	fuse_value_tmp = (fuse_value & FB_SKU_MASK) >> FB_SKU_SHIFT;

	dev_priv->fuse_reg_value = fuse_value;

	switch (fuse_value_tmp) {
	case FB_SKU_100:
		dev_priv->core_freq = 200;
		break;
	case FB_SKU_100L:
		dev_priv->core_freq = 100;
		break;
	case FB_SKU_83:
		dev_priv->core_freq = 166;
		break;
	default:
		DRM_ERROR("Invalid SKU values, SKU value = 0x%08x\n",
			  fuse_value_tmp);
		dev_priv->core_freq = 0;
	}
	PSB_DEBUG_ENTRY("LNC core clk is %dMHz.\n", dev_priv->core_freq);

#if 0				/* debug message */
	fuse_value_tmp =
	    (fuse_value & FB_GFX_CLK_DIVIDE_MASK) >> FB_GFX_CLK_DIVIDE_SHIFT;

	switch (fuse_value_tmp) {
	case 0:
		DRM_INFO("Gfx clk : core clk = 1:1.\n");
		break;
	case 1:
		DRM_INFO("Gfx clk : core clk = 4:3.\n");
		break;
	case 2:
		DRM_INFO("Gfx clk : core clk = 8:5.\n");
		break;
	case 3:
		DRM_INFO("Gfx clk : core clk = 2:1.\n");
		break;
	case 4:
		DRM_INFO("Gfx clk : core clk = 16:7.\n");
		break;
	case 5:
		DRM_INFO("Gfx clk : core clk = 8:3.\n");
		break;
	case 6:
		DRM_INFO("Gfx clk : core clk = 16:5.\n");
		break;
	case 7:
		DRM_INFO("Gfx clk : core clk = 4:1.\n");
		break;
	default:
		DRM_ERROR("Invalid GFX CLK DIVIDE values, value = 0x%08x\n",
			  fuse_value_tmp);
	}

	fuse_value_tmp =
	    (fuse_value & FB_VED_CLK_DIVIDE_MASK) >> FB_VED_CLK_DIVIDE_SHIFT;

	switch (fuse_value_tmp) {
	case 0:
		DRM_INFO("Ved clk : core clk = 1:1.\n");
		break;
	case 1:
		DRM_INFO("Ved clk : core clk = 4:3.\n");
		break;
	case 2:
		DRM_INFO("Ved clk : core clk = 8:5.\n");
		break;
	case 3:
		DRM_INFO("Ved clk : core clk = 2:1.\n");
		break;
	default:
		DRM_ERROR("Invalid VED CLK DIVIDE values, value = 0x%08x\n",
			  fuse_value_tmp);
	}

	fuse_value_tmp =
	    (fuse_value & FB_VEC_CLK_DIVIDE_MASK) >> FB_VEC_CLK_DIVIDE_SHIFT;

	switch (fuse_value_tmp) {
	case 0:
		DRM_INFO("Vec clk : core clk = 1:1.\n");
		break;
	case 1:
		DRM_INFO("Vec clk : core clk = 4:3.\n");
		break;
	case 2:
		DRM_INFO("Vec clk : core clk = 8:5.\n");
		break;
	case 3:
		DRM_INFO("Vec clk : core clk = 2:1.\n");
		break;
	default:
		DRM_ERROR("Invalid VEC CLK DIVIDE values, value = 0x%08x\n",
			  fuse_value_tmp);
	}
#endif				/* FIXME remove it after PO */

	if (IS_FLDS(dev)) {
#if KSEL_BYPASS_83_100_ENABLE
		dev_priv->ksel = KSEL_BYPASS_83_100;
#endif				/* KSEL_BYPASS_83_100_ENABLE */

#if  KSEL_CRYSTAL_19_ENABLED
		dev_priv->ksel = KSEL_CRYSTAL_19;
#endif				/*  KSEL_CRYSTAL_19_ENABLED */
	}

	return;
}

bool mid_get_pci_revID(struct drm_psb_private *dev_priv)
{
	uint32_t platform_rev_id = 0;
	struct pci_dev *pci_gfx_root = pci_get_bus_and_slot(0, PCI_DEVFN(2, 0));
	if (!pci_gfx_root) {
		DRM_ERROR("pci_gfx_root is NULL\n");
		return false;
	}

	/*get the revison ID, B0:D2:F0;0x08 */
	pci_read_config_dword(pci_gfx_root, 0x08, &platform_rev_id);
	dev_priv->platform_rev_id = (uint8_t) platform_rev_id;
	pci_dev_put(pci_gfx_root);
	PSB_DEBUG_ENTRY("platform_rev_id is %x\n", dev_priv->platform_rev_id);

	return true;
}

#ifdef CONFIG_SUPPORT_MIPI
bool mrst_get_vbt_data(struct drm_psb_private *dev_priv)
{
	struct drm_device *dev = dev_priv->dev;

	dev_priv->panel_id = PanelID;
	dev_priv->mipi_encoder_type = is_panel_vid_or_cmd(dev_priv->dev);

	if (is_dual_dsi(dev) && IS_ANN(dev)) {
		dev_priv->bUseHFPLL = false;
		dev_priv->bRereadZero = false;
	} else if (IS_TNG_B0(dev) || IS_ANN_A0(dev)) {
		if (dev_priv->mipi_encoder_type == MDFLD_DSI_ENCODER_DBI) {
			if (IS_ANN(dev))
				dev_priv->bUseHFPLL = false;
			else {
				dev_priv->bUseHFPLL = true;
				enable_HFPLL(dev_priv->dev);
			}
			dev_priv->bRereadZero = false;
		} else {
			dev_priv->bUseHFPLL = false;
			dev_priv->bRereadZero = true;
		}
	} else {
		dev_priv->bUseHFPLL = false;
		dev_priv->bRereadZero = false;
	}
	return true;
}
#endif

void hdmi_do_hotplug_wq(struct work_struct *work)
{
	struct drm_psb_private *dev_priv = container_of(work,
							struct drm_psb_private,
							hdmi_hotplug_wq);
	atomic_inc(&dev_priv->hotplug_wq_done);
	/* notify user space of hotplug event via a uevent message */

#ifdef CONFIG_X86_MRST
	{
		u8 data = 0;
		intel_scu_ipc_iowrite8(MSIC_VCC330CNT, VCC330_ON);

		intel_scu_ipc_ioread8(MSIC_HDMI_STATUS, &data);

		if (data & HPD_SIGNAL_STATUS) {
			PSB_DEBUG_ENTRY(
				"hdmi_do_hotplug_wq: HDMI plugged in\n");
			hdmi_state = 1;
			if (dev_priv->had_interface)
				dev_priv->had_interface->probe(
					dev_priv->had_pvt_data, 0);

			drm_sysfs_hotplug_event(dev_priv->dev);
		} else {
			PSB_DEBUG_ENTRY("hdmi_do_hotplug_wq: HDMI unplugged\n");
			hdmi_state = 0;
			drm_sysfs_hotplug_event(dev_priv->dev);

			if (dev_priv->had_interface)
				dev_priv->had_interface->
				    disconnect(dev_priv->had_pvt_data);
		}
	}
#endif

	atomic_dec(&dev_priv->hotplug_wq_done);
}

#ifdef CONFIG_SUPPORT_HDMI
void hdmi_do_audio_wq(struct work_struct *work)
{
	struct drm_psb_private *dev_priv = container_of(work,
		struct drm_psb_private,
		hdmi_audio_wq);
	bool hdmi_hpd_connected = false;

	/* As in the hdmi_do_hotplug_wq() function above
	* it seems we should not be running this section of
	* code if we don't also have CONFIG_SUPPORT_HDMI set,
	* some devices might not want/need support for HDMI
	* early in the platform bring up and by having this
	* available to run might produce unexpected results
	* if HDMI connector is plugged in.
	*/

	DRM_INFO("hdmi_do_audio_wq: Checking for HDMI connection at boot\n");
	hdmi_hpd_connected = android_hdmi_is_connected(dev_priv->dev);
	if (hdmi_hpd_connected) {
		DRM_INFO("hdmi_do_audio_wq: HDMI plugged in\n");
		mid_hdmi_audio_signal_event(dev_priv->dev, HAD_EVENT_HOT_PLUG);
	}
}
#endif

#ifdef CONFIG_SUPPORT_HDMI
#define HDMI_HOTPLUG_DELAY (2*HZ)
static void hdmi_hotplug_timer_func(unsigned long data)
{
/* FIXME: TODO: hkpatel - Enable once runtime pm is enabled */
#if 0
	struct drm_device *dev = (struct drm_device *)data;

	PSB_DEBUG_ENTRY("\n");
	ospm_runtime_pm_allow(dev);
#endif
}

static int hdmi_hotplug_timer_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct timer_list *hdmi_timer = &dev_priv->hdmi_timer;

	PSB_DEBUG_ENTRY("\n");

	init_timer(hdmi_timer);

	hdmi_timer->data = (unsigned long)dev;
	hdmi_timer->function = hdmi_hotplug_timer_func;
	hdmi_timer->expires = jiffies + HDMI_HOTPLUG_DELAY;

	PSB_DEBUG_ENTRY("successfully\n");

	return 0;
}

void hdmi_hotplug_timer_start(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct timer_list *hdmi_timer = &dev_priv->hdmi_timer;

	PSB_DEBUG_ENTRY("\n");
	if (!timer_pending(hdmi_timer)) {
		hdmi_timer->expires = jiffies + HDMI_HOTPLUG_DELAY;
		add_timer(hdmi_timer);
	}
}
#endif

static int psb_do_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct ttm_bo_device *bdev = &dev_priv->bdev;
	struct psb_gtt *pg = dev_priv->pg;

	uint32_t tmp;

	int ret = -ENOMEM;

	/*
	 * Initialize sequence numbers for the different command
	 * submission mechanisms.
	 */

	dev_priv->sequence[PSB_ENGINE_2D] = 0;
	dev_priv->sequence[PSB_ENGINE_VIDEO] = 1;
	dev_priv->sequence[LNC_ENGINE_ENCODE] = 0;
	dev_priv->sequence[VSP_ENGINE_VPP] = 1;

	if (pg->mmu_gatt_start & 0x0FFFFFFF) {
		DRM_ERROR("Gatt must be 256M aligned. This is a bug.\n");
		ret = -EINVAL;
		goto out_err;
	}

	dev_priv->sizes.ta_mem_size = 0;

	/* TT region managed by TTM. */
	if (!ttm_bo_init_mm(bdev, TTM_PL_TT,
			    pg->gatt_pages -
			    (pg->gtt_video_start >> PAGE_SHIFT)
			    )) {

		dev_priv->have_tt = 1;
		dev_priv->sizes.tt_size = (pg->gatt_pages -
			(pg->gtt_video_start >> PAGE_SHIFT)) / 256;
		printk("[TTM] TT heap size is %d\n", pg->gtt_video_start);
	}
	if (!ttm_bo_init_mm(bdev,
			    DRM_PSB_MEM_MMU,
			    PSB_MEM_TT_START >> PAGE_SHIFT)) {
		dev_priv->have_mem_mmu = 1;
		dev_priv->sizes.mmu_size =
			PSB_MEM_TT_START / (1024 * 1024);
		printk("[TTM] MMU heap size is %d\n", PSB_MEM_TT_START);
	}

	if (IS_MSVDX_MEM_TILE(dev)) {
		/* Create tiling MMU region managed by TTM */
		tmp = (0x10000000) >> PAGE_SHIFT;
		if (!ttm_bo_init_mm(bdev, DRM_PSB_MEM_MMU_TILING, tmp))
			dev_priv->have_mem_mmu_tiling = 1;
		printk("[TTM] MMU tiling heap size is %d\n", tmp);
	}

	PSB_DEBUG_INIT("Init MSVDX\n");

	/* on TNG B0, VED not needed to be on here since firmware is not loaded in psb_msvdx_init */
	if (IS_TNG_A0(dev))
		power_island_get(OSPM_VIDEO_DEC_ISLAND);

	psb_msvdx_init(dev);

	if (IS_TNG_A0(dev))
		power_island_put(OSPM_VIDEO_DEC_ISLAND);

#ifdef SUPPORT_VSP
	VSP_DEBUG("Init VSP\n");
	vsp_init(dev);
#endif

	PSB_DEBUG_INIT("Init Topaz\n");
	tng_topaz_init(dev);
	return 0;
 out_err:
	psb_do_takedown(dev);
	return ret;
}

static int psb_driver_unload(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;

	/*Fristly, unload pvr driver */
	PVRSRVDrmUnload(dev);

	/*TODO: destroy DSR/DPU infos here */
	psb_backlight_exit();	/*writes minimum value to backlight HW reg */

	if (drm_psb_no_fb == 0)
		psb_modeset_cleanup(dev);

	if (dev_priv) {
		destroy_workqueue(dev_priv->vsync_wq);
		destroy_workqueue(dev_priv->power_wq);

		/* psb_watchdog_takedown(dev_priv); */
		psb_do_takedown(dev);

		if (dev_priv->pf_pd) {
			psb_mmu_free_pagedir(dev_priv->pf_pd);
			dev_priv->pf_pd = NULL;
		}
		if (dev_priv->mmu) {
			struct psb_gtt *pg = dev_priv->pg;

			down_read(&pg->sem);
			psb_mmu_remove_pfn_sequence(psb_mmu_get_default_pd
						    (dev_priv->mmu),
						    pg->mmu_gatt_start,
						    pg->vram_stolen_size >>
						    PAGE_SHIFT);
			up_read(&pg->sem);
			psb_mmu_driver_takedown(dev_priv->mmu);
			dev_priv->mmu = NULL;
		}

#ifdef SUPPORT_VSP
		if (dev_priv->vsp_mmu) {
			struct psb_gtt *pg = dev_priv->pg;

			down_read(&pg->sem);
			psb_mmu_remove_pfn_sequence(
				psb_mmu_get_default_pd
				(dev_priv->vsp_mmu),
				pg->mmu_gatt_start,
				pg->vram_stolen_size >> PAGE_SHIFT);
			up_read(&pg->sem);
			psb_mmu_driver_takedown(dev_priv->vsp_mmu);
			dev_priv->vsp_mmu = NULL;
		}
#endif
		if (IS_MRFLD(dev))
			mrfld_gtt_takedown(dev_priv->pg, 1);
		else
			psb_gtt_takedown(dev_priv->pg, 1);

		if (dev_priv->scratch_page) {
			__free_page(dev_priv->scratch_page);
			dev_priv->scratch_page = NULL;
		}
		if (dev_priv->has_bo_device) {
			ttm_bo_device_release(&dev_priv->bdev);
			dev_priv->has_bo_device = 0;
		}
		if (dev_priv->has_fence_device) {
			ttm_fence_device_release(&dev_priv->fdev);
			dev_priv->has_fence_device = 0;
		}
		if (dev_priv->vdc_reg) {
			iounmap(dev_priv->vdc_reg);
			dev_priv->vdc_reg = NULL;
		}
		if (dev_priv->rgx_reg) {
			iounmap(dev_priv->rgx_reg);
			dev_priv->rgx_reg = NULL;
		}
		if (dev_priv->wrapper_reg) {
			iounmap(dev_priv->wrapper_reg);
			dev_priv->wrapper_reg = NULL;
		}
		if (dev_priv->ved_wrapper_reg) {
			iounmap(dev_priv->ved_wrapper_reg);
			dev_priv->ved_wrapper_reg = NULL;
		}
		if (dev_priv->vec_wrapper_reg) {
			iounmap(dev_priv->vec_wrapper_reg);
			dev_priv->vec_wrapper_reg = NULL;
		}
		if (dev_priv->msvdx_reg) {
			iounmap(dev_priv->msvdx_reg);
			dev_priv->msvdx_reg = NULL;
		}
#ifdef SUPPORT_VSP
		if (dev_priv->vsp_reg) {
			iounmap(dev_priv->vsp_reg);
			dev_priv->vsp_reg = NULL;
		}
#endif

		if (IS_TOPAZ(dev)) {
			if (dev_priv->topaz_reg) {
				iounmap(dev_priv->topaz_reg);
				dev_priv->topaz_reg = NULL;
			}
		}

		if (dev_priv->tdev)
			ttm_object_device_release(&dev_priv->tdev);

		if (dev_priv->has_global)
			psb_ttm_global_release(dev_priv);

		tasklet_kill(&dev_priv->hdmi_audio_bufferdone_tasklet);


		kfree(dev_priv);
		dev->dev_private = NULL;
	}

	ospm_power_uninit();

	return 0;
}

static int psb_driver_load(struct drm_device *dev, unsigned long chipset)
{
	struct drm_psb_private *dev_priv;
	struct ttm_bo_device *bdev;
	unsigned long resource_start;
	struct psb_gtt *pg;
	unsigned long irqflags;
	int ret = -ENOMEM;
	uint32_t tt_pages;
	u32 pm_mask = 0x0;
	int pm_reg = 0x0;

	if (IS_MOFD(dev)) {
		pm_reg = 0x3f;
		pm_mask = intel_mid_msgbus_read32(0x04, pm_reg);

		pm_mask = 0x0;
		pm_reg = 0x30; //GFXSS
		power_up(pm_reg,pm_mask);

		pm_mask = 0x0;
		pm_reg = 0x36; //DSPSS
		power_up(pm_reg,pm_mask);

		pm_reg = 0x3c; //HDMISS
		power_up(pm_reg,pm_mask);

		pm_reg = 0x3f;
		pm_mask = intel_mid_msgbus_read32(0x04, pm_reg);
	}

	DRM_INFO("psb - %s\n", PSB_PACKAGE_VERSION);

	DRM_INFO("Run drivers on Merrifield platform!\n");

	dev_priv = kzalloc(sizeof(*dev_priv), GFP_KERNEL);
	if (dev_priv == NULL)
		return -ENOMEM;
	INIT_LIST_HEAD(&dev_priv->video_ctx);
	spin_lock_init(&dev_priv->video_ctx_lock);
	if (IS_FLDS(dev))
		dev_priv->num_pipe = 3;
	else
		dev_priv->num_pipe = 2;
	atomic_set(&dev_priv->hotplug_wq_done, 1);

	/*init DPST umcomm to NULL */
	dev_priv->psb_dpst_state = NULL;
	dev_priv->psb_hotplug_state = NULL;
	dev_priv->hdmi_done_reading_edid = false;
	dev_priv->um_start = false;

	dev_priv->dev = dev;
	bdev = &dev_priv->bdev;

	hdmi_state = 0;
	dev_priv->ied_enabled = false;
	dev_priv->ied_force_clean = false;
	dev_priv->ied_context = NULL;

	drm_hdmi_hpd_auto = 0;

	ret = psb_ttm_global_init(dev_priv);
	if (unlikely(ret != 0))
		goto out_err;
	dev_priv->has_global = 1;

	dev_priv->tdev = ttm_object_device_init
	    (dev_priv->mem_global_ref.object, PSB_OBJECT_HASH_ORDER);
	if (unlikely(dev_priv->tdev == NULL))
		goto out_err;

	mutex_init(&dev_priv->temp_mem);
	mutex_init(&dev_priv->cmdbuf_mutex);
	mutex_init(&dev_priv->reset_mutex);
	INIT_LIST_HEAD(&dev_priv->decode_context.validate_list);
	INIT_LIST_HEAD(&dev_priv->encode_context.validate_list);
#ifdef SUPPORT_VSP
	INIT_LIST_HEAD(&dev_priv->vsp_context.validate_list);
#endif

	mutex_init(&dev_priv->dpms_mutex);
        mutex_init(&dev_priv->gamma_csc_lock);
	mutex_init(&dev_priv->dsr_mutex);
	mutex_init(&dev_priv->vsync_lock);

	spin_lock_init(&dev_priv->reloc_lock);
	spin_lock_init(&dev_priv->irqmask_lock);

	DRM_INIT_WAITQUEUE(&dev_priv->rel_mapped_queue);
	init_waitqueue_head(&dev_priv->eof_wait);

	dev->dev_private = (void *)dev_priv;
	dev_priv->chipset = chipset;
	psb_set_uopt(&dev_priv->uopt);

	PSB_DEBUG_INIT("Mapping MMIO\n");
	resource_start = pci_resource_start(dev->pdev, PSB_MMIO_RESOURCE);

	if (IS_MSVDX(dev))	/* Work around for medfield by Li */
		dev_priv->msvdx_reg =
		    ioremap(resource_start + MRST_MSVDX_OFFSET, PSB_MSVDX_SIZE);
	else
		dev_priv->msvdx_reg =
		    ioremap(resource_start + PSB_MSVDX_OFFSET, PSB_MSVDX_SIZE);

	if (!dev_priv->msvdx_reg)
		goto out_err;

#ifdef SUPPORT_VSP
	dev_priv->vsp_reg =
		ioremap(resource_start + TNG_VSP_OFFSET, TNG_VSP_SIZE);
	if (!dev_priv->vsp_reg)
		goto out_err;
#endif
	if (IS_TOPAZ(dev)) {
		dev_priv->topaz_reg =
		    ioremap(resource_start + TNG_TOPAZ_OFFSET,
			    TNG_TOPAZ_SIZE);

		if (!dev_priv->topaz_reg)
			goto out_err;
	}

	dev_priv->vdc_reg =
	    ioremap(resource_start + PSB_VDC_OFFSET, PSB_VDC_SIZE);
	if (!dev_priv->vdc_reg)
		goto out_err;

	if (IS_MRFLD(dev)) {
		dev_priv->rgx_reg =
			ioremap(resource_start + RGX_OFFSET,
				RGX_SIZE);

		if (!dev_priv->rgx_reg)
			goto out_err;

		dev_priv->wrapper_reg =
			ioremap(resource_start + GFX_WRAPPER_OFFSET,
				GFX_WRAPPER_SIZE);

		if (!dev_priv->wrapper_reg)
			goto out_err;

		dev_priv->ved_wrapper_reg =
			ioremap(resource_start + VED_WRAPPER_OFFSET,
				VED_WRAPPER_SIZE);

		if (!dev_priv->ved_wrapper_reg)
			goto out_err;

		dev_priv->vec_wrapper_reg =
			ioremap(resource_start + VEC_WRAPPER_OFFSET,
				VEC_WRAPPER_SIZE);

		if (!dev_priv->vec_wrapper_reg)
			goto out_err;

	}

	dev_priv->pci_root = pci_get_bus_and_slot(0, 0);
#ifdef CONFIG_SUPPORT_HDMI
	/* setup hdmi driver */
	android_hdmi_driver_setup(dev);
#endif
	if (IS_MID(dev)) {
		mrst_get_fuse_settings(dev);
#ifdef CONFIG_SUPPORT_MIPI
		mrst_get_vbt_data(dev_priv);
#endif
		mid_get_pci_revID(dev_priv);
	}

	PSB_DEBUG_INIT("Init TTM fence and BO driver\n");

	/* Init OSPM support */
	ospm_power_init(dev);

	ret = psb_ttm_fence_device_init(&dev_priv->fdev);
	if (unlikely(ret != 0))
		goto out_err;

	/* For VXD385 DE2.x firmware support 16bit fence value */
	if (IS_FLDS(dev)) {
		dev_priv->fdev.fence_class[PSB_ENGINE_VIDEO].wrap_diff =
		    (1 << 14);
		dev_priv->fdev.fence_class[PSB_ENGINE_VIDEO].flush_diff =
		    (1 << 13);
		dev_priv->fdev.fence_class[PSB_ENGINE_VIDEO].sequence_mask =
		    0x0000ffff;
	}

	dev_priv->has_fence_device = 1;
	ret = ttm_bo_device_init(bdev,
				 dev_priv->bo_global_ref.ref.object,
				 &psb_ttm_bo_driver,
				 DRM_PSB_FILE_PAGE_OFFSET, false);
	if (unlikely(ret != 0))
		goto out_err;
	dev_priv->has_bo_device = 1;
	ttm_lock_init(&dev_priv->ttm_lock);

	ret = -ENOMEM;

	dev_priv->scratch_page = alloc_page(GFP_DMA32 | __GFP_ZERO);
	if (!dev_priv->scratch_page)
		goto out_err;

	set_pages_uc(dev_priv->scratch_page, 1);

	dev_priv->pg = psb_gtt_alloc(dev);
	if (!dev_priv->pg)
		goto out_err;

	if (IS_MRFLD(dev))
		ret = mrfld_gtt_init(dev_priv->pg, 0);
	else
		ret = psb_gtt_init(dev_priv->pg, 0);

	if (ret)
		goto out_err;

	ret = psb_gtt_mm_init(dev_priv->pg);
	if (ret)
		goto out_err;

	dev_priv->mmu = psb_mmu_driver_init((void *)0,
					    drm_psb_trap_pagefaults, 0,
					    dev_priv, IMG_MMU);
	if (!dev_priv->mmu)
		goto out_err;

#ifdef SUPPORT_VSP
	dev_priv->vsp_mmu = psb_mmu_driver_init((void *)0,
					    drm_psb_trap_pagefaults, 0,
					    dev_priv, VSP_MMU);
	if (!dev_priv->vsp_mmu)
		goto out_err;
#endif

	pg = dev_priv->pg;

	tt_pages = (pg->gatt_pages < PSB_TT_PRIV0_PLIMIT) ?
	    (pg->gatt_pages) : PSB_TT_PRIV0_PLIMIT;

	/* CI/RAR use the lower half of TT. */
	pg->gtt_video_start = (tt_pages / 2) << PAGE_SHIFT;

	dev_priv->pf_pd = psb_mmu_alloc_pd(dev_priv->mmu, 1, 0);
	if (!dev_priv->pf_pd)
		goto out_err;

	psb_mmu_set_pd_context(psb_mmu_get_default_pd(dev_priv->mmu), 0);
	psb_mmu_set_pd_context(dev_priv->pf_pd, 1);
#ifdef SUPPORT_VSP
	/* for vsp mmu */
	psb_mmu_set_pd_context(psb_mmu_get_default_pd(dev_priv->vsp_mmu), 0);
#endif
	spin_lock_init(&dev_priv->sequence_lock);

	PSB_DEBUG_INIT("Begin to init MSVDX/Topaz\n");

	/*initialize the MSI for MRST */
	if (IS_MID(dev)) {
		if (pci_enable_msi(dev->pdev)) {
			DRM_ERROR("Enable MSI failed!\n");
		} else {
			PSB_DEBUG_INIT("Enabled MSI IRQ (%d)\n",
				       dev->pdev->irq);
			/* pci_write_config_word(pdev, 0x04, 0x07); */
		}
	}

	ret = drm_vblank_init(dev, dev_priv->num_pipe);
	if (ret)
		goto out_err;

	/*
	 * Install interrupt handlers prior to powering off SGX or else we will
	 * crash.
	 */
	dev_priv->vdc_irq_mask = 0;
	dev_priv->pipestat[0] = 0;
	dev_priv->pipestat[1] = 0;
	dev_priv->pipestat[2] = 0;
	spin_lock_init(&dev_priv->irqmask_lock);
	spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
	PSB_WVDC32(0x00000000, PSB_INT_ENABLE_R);
	PSB_WVDC32(0xFFFFFFFF, PSB_INT_MASK_R);
	spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_irq_install(dev);

	dev->vblank_disable_allowed = 1;
	dev->max_vblank_count = 0xffffff;
	/* only 24 bits of frame count */

	/* For Video mode panels, set the drm_vblank_offdelay so that we turn
	 * off faster than the default of 5 seconds. This is done to have
	 * better S0i1-Display residency for idle use cases
	 */
	drm_vblank_offdelay = VBLANK_OFF_DELAY_DEFAULT;

	dev->driver->get_vblank_counter = psb_get_vblank_counter;

#ifdef CONFIG_SUPPORT_MIPI
	if (IS_FLDS(dev) &&
			(is_panel_vid_or_cmd(dev) == MDFLD_DSI_ENCODER_DBI)) {
#ifdef CONFIG_MID_DSI_DPU
		/*init dpu info */
		mdfld_dbi_dpu_init(dev);
#else
		mdfld_dbi_dsr_init(dev);
#endif				/*CONFIG_MID_DSI_DPU */
		INIT_WORK(&dev_priv->te_work, mdfld_te_handler_work);
		INIT_WORK(&dev_priv->reset_panel_work,
				mdfld_reset_panel_handler_work);
	}
#endif
	INIT_WORK(&dev_priv->vsync_event_work, mdfld_vsync_event_work);

	dev_priv->vsync_wq = alloc_workqueue("vsync_wq", WQ_UNBOUND, 2);
	if (!dev_priv->vsync_wq) {
		DRM_ERROR("failed to create vsync workqueue\n");
		ret = -ENOMEM;
		goto out_err;
	}

	dev_priv->power_wq = alloc_ordered_workqueue("power_wq", WQ_HIGHPRI);
	if (!dev_priv->power_wq) {
		DRM_ERROR("failed to create vsync workqueue\n");
		ret = -ENOMEM;
		goto out_err;
	}

	dev_priv->pipea_dpi_underrun_count = 0;
	dev_priv->pipec_dpi_underrun_count = 0;

	dev_priv->hdmi_first_boot = true;

	if (drm_psb_no_fb == 0) {
		psb_modeset_init(dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
		psb_fbdev_init(dev);

		if (IS_MRFLD(dev))
			intel_drm_kms_helper_poll_init(dev);
		else
			drm_kms_helper_poll_init(dev);
#else
		drm_helper_initial_config(dev);
#endif
	}

	/*must be after mrst_get_fuse_settings() */
	ret = psb_backlight_init(dev);
	if (ret)
		return ret;

	ret = psb_do_init(dev);
	if (ret)
		return ret;

	/* initialize HDMI Hotplug interrupt forwarding
	 * notifications for user mode
	 */
	if (IS_FLDS(dev)) {
		struct pci_dev *pdev = NULL;
		struct device *ddev = NULL;
		struct kobject *kobj = NULL;

		/*find handle to drm kboject */
		pdev = dev->pdev;
		ddev = &pdev->dev;
		kobj = &ddev->kobj;

		dev_priv->psb_hotplug_state = psb_hotplug_init(kobj);
	}

#ifdef CONFIG_SUPPORT_HDMI
	hdmi_hotplug_timer_init(dev);
	atomic_set(&dev_priv->hotplug_wq_done, 0);
	INIT_WORK(&dev_priv->hdmi_hotplug_wq, hdmi_do_hotplug_wq);
	INIT_WORK(&dev_priv->hdmi_audio_wq, hdmi_do_audio_wq);
	INIT_WORK(&dev_priv->hdmi_audio_underrun_wq, hdmi_do_audio_underrun_wq);
	INIT_WORK(&dev_priv->hdmi_audio_bufferdone_wq, hdmi_do_audio_bufferdone_wq);
	tasklet_init(&dev_priv->hdmi_audio_bufferdone_tasklet,
		     hdmi_audio_bufferdone_tasklet_func,
		     (unsigned long)dev_priv);
#endif
	dev_priv->hdmi_first_boot = false;

	/*Intel drm driver load is done, continue doing pvr load */
	DRM_DEBUG("Pvr driver load\n");

#ifdef CONFIG_SUPPORT_MIPI
	/* init display manager */
	dispmgr_start(dev);

	/* SH START DPST
	* SH This hooks dpst with the device.
	*/
	dpst_init(dev, 5, 1);

	mdfld_dsi_dsr_enable(dev_priv->dsi_configs[0]);
#else
	/* power down islands turned on by firmware */
	power_island_put(OSPM_DISPLAY_A |
			OSPM_DISPLAY_C | OSPM_DISPLAY_MIO);
#endif

	return PVRSRVDrmLoad(dev, chipset);
 out_err:
	psb_driver_unload(dev);
	return ret;
}

int psb_driver_device_is_agp(struct drm_device *dev)
{
	return 0;
}

int psb_extension_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	union drm_psb_extension_arg *arg = data;
	struct drm_psb_extension_rep *rep = &arg->rep;

	if (strcmp(arg->extension, "psb_ttm_placement_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_PLACEMENT_OFFSET;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}
	if (strcmp(arg->extension, "psb_ttm_fence_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_FENCE_OFFSET;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}
	if (strcmp(arg->extension, "psb_ttm_execbuf_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_CMDBUF;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	/*return the page flipping ioctl offset */
	if (strcmp(arg->extension, "psb_page_flipping_alphadrop") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_FLIP;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	/* return the video rar offset */
	if (strcmp(arg->extension, "lnc_video_getparam") == 0) {
		rep->exists = 1;
		rep->driver_ioctl_offset = DRM_PSB_VIDEO_GETPARAM;
		rep->sarea_offset = 0;
		rep->major = 1;
		rep->minor = 0;
		rep->pl = 0;
		return 0;
	}

	rep->exists = 0;
	return 0;
}

static int psb_vt_leave_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct ttm_bo_device *bdev = &dev_priv->bdev;
	struct ttm_mem_type_manager *man;
	int clean;
	int ret;

	ret = ttm_vt_lock(&dev_priv->ttm_lock, 1,
			  BCVideoGetPriv(file_priv)->tfile);
	if (unlikely(ret != 0))
		return ret;

	ret = ttm_bo_evict_mm(&dev_priv->bdev, TTM_PL_TT);
	if (unlikely(ret != 0))
		goto out_unlock;

	man = &bdev->man[TTM_PL_TT];
	/*spin_lock(&bdev->lru_lock); */
	clean = drm_mm_clean((struct drm_mm *)man->priv);
	/*spin_unlock(&bdev->lru_lock); */
	if (unlikely(!clean))
		DRM_INFO("Warning: GATT was not clean after VT switch.\n");

	ttm_bo_swapout_all(&dev_priv->bdev);

	return 0;
 out_unlock:
	(void)ttm_vt_unlock(&dev_priv->ttm_lock);
	return ret;
}

static int psb_vt_enter_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	return ttm_vt_unlock(&dev_priv->ttm_lock);
}

static int psb_sizes_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct drm_psb_sizes_arg *arg = (struct drm_psb_sizes_arg *)data;

	*arg = dev_priv->sizes;
	return 0;
}

static int psb_fuse_reg_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;

	*arg = dev_priv->fuse_reg_value;
	return 0;
}

    /*****************************
     *  HDMI TEST IOCTLS
     */
static int psb_drm_hdmi_test_ioctl(struct drm_device *dev,
				   void *data, struct drm_file *file_priv)
{
	drm_psb_hdmireg_p reg = data;

	if (!power_island_get(OSPM_DISPLAY_B | OSPM_DISPLAY_HDMI))
		return -EAGAIN;

	if (reg->mode & HT_WRITE)
		PSB_WVDC32(reg->data, reg->reg);

	if (reg->mode & HT_READ)
		reg->data = PSB_RVDC32(reg->reg);

	power_island_put(OSPM_DISPLAY_B | OSPM_DISPLAY_HDMI);
	return 0;
}				/* psb_drm_hdmi_test_ioctl */

    /*
     *  END HDMI TEST IOCTLS
     ****************************/

static int psb_vbt_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct gct_ioctl_arg *pGCT = data;

	memcpy(pGCT, &dev_priv->gct_data, sizeof(*pGCT));

	return 0;
}

static int psb_csc_gamma_setting_ioctl(struct drm_device *dev, void *data,
                         struct drm_file *file_priv)
{
        struct drm_psb_csc_gamma_setting *csc_gamma_setting = NULL;
        int ret = 0;
        csc_gamma_setting = (struct drm_psb_csc_gamma_setting *)
                                        data;
        printk("seting gamma ioctl!\n");
        if (!csc_gamma_setting)
                return -EINVAL;

        if (csc_gamma_setting->type == GAMMA){
                drm_psb_enable_gamma = 1;
                ret = mdfld_intel_crtc_set_gamma(dev,
                        &csc_gamma_setting->data.gamma_data);
	}
        else if (csc_gamma_setting->type == CSC){
                drm_psb_enable_color_conversion = 1;
                ret = mdfld_intel_crtc_set_color_conversion(dev,
                        &csc_gamma_setting->data.csc_data);
	}
        return ret;
}

static int psb_enable_ied_session_ioctl(struct drm_device *dev, void *data,
						struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
#ifdef CONFIG_SUPPORT_MIPI
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
#endif

	DRM_DEBUG("Enabling IED session...\n");

	if (file_priv == NULL) {
		DRM_ERROR("%s: file_priv is NULL.\n", __func__);
		return -1;
	}

	if (dev_priv->ied_enabled) {
		DRM_ERROR("%s: ied_enabled has been set.\n", __func__);
		return 0;
	}

	dev_priv->ied_enabled = true;
	dev_priv->ied_context = file_priv->filp;

	if (power_island_get(OSPM_DISPLAY_A)) {
#ifdef CONFIG_SUPPORT_MIPI
		mdfld_dsi_dsr_forbid(dsi_config);
#endif

		/* Set bit 31 to enable IED pipeline */
		REG_WRITE(PSB_IED_DRM_CNTL_STATUS, 0x80000000);
		power_island_put(OSPM_DISPLAY_A);
		return 0;
	} else {
		DRM_ERROR("%s: Failed to power on display island.\n", __func__);
		return -1;
	}

	return 0;
}

static int psb_disable_ied_session_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_psb_private *dev_priv = psb_priv(dev);
#ifdef CONFIG_SUPPORT_MIPI
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
#endif

	DRM_DEBUG("Disabling IED session...\n");

	if (file_priv == NULL) {
		DRM_ERROR("%s: file_priv is NULL.\n", __func__);
		return -1;
	}

	if (dev_priv->ied_enabled == false) {
		DRM_ERROR("%s: ied_enabled is not set.\n", __func__);
		return 0;
	}

	if (dev_priv->ied_context != file_priv->filp) {
		DRM_ERROR("%s: Wrong context.\n", __func__);
		return -1;
	}

	if (power_island_get(OSPM_DISPLAY_A)) {
		REG_WRITE(PSB_IED_DRM_CNTL_STATUS, 0);
#ifdef CONFIG_SUPPORT_MIPI
		mdfld_dsi_dsr_allow(dsi_config);
#endif

		power_island_put(OSPM_DISPLAY_A);

		dev_priv->ied_enabled = false;
		dev_priv->ied_context = NULL;
		ret = 0;
	} else {
		DRM_ERROR("%s: Failed to power on display island.\n", __func__);
		ret = -1;
	}

	return 0;
}

static int psb_panel_query_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv) {
#ifdef CONFIG_SUPPORT_MIPI
	uint32_t *arg = data;

	*arg = (is_panel_vid_or_cmd(dev) == MDFLD_DSI_ENCODER_DPI);
#endif
	return 0;
}

static int psb_idle_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv) {

	struct drm_psb_idle_ctrl *ctrl = (struct drm_psb_idle_ctrl*) data;

	switch (ctrl->cmd) {
	case IDLE_CTRL_ENABLE:
		PSB_DEBUG_PM("IDLE_CTRL_ENABLE\n");
		enable_repeat_frame_intr(dev);
		break;
	case IDLE_CTRL_DISABLE:
		PSB_DEBUG_PM("IDLE_CTRL_DISABLE\n");
		exit_maxfifo_mode(dev);
		disable_repeat_frame_intr(dev);
		break;
	case IDLE_CTRL_ENTER:
		PSB_DEBUG_PM("IDLE_CTRL_ENTER\n");
		enter_maxfifo_mode(dev);
		break;
	case IDLE_CTRL_EXIT:
		PSB_DEBUG_PM("IDLE_CTRL_EXIT\n");
		exit_maxfifo_mode(dev);
		enable_repeat_frame_intr(dev);
		break;
	default:
		DRM_ERROR("%s: invalid command.\n", __func__);
		break;
	}

	return 0;
}

#ifdef CONFIG_SUPPORT_HDMI
static int psb_disp_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	static uint32_t hdmi_export_handle;
	struct drm_psb_disp_ctrl *dp_ctrl = data;
	/*DRM_COPY_FROM_USER(&dp_ctrl, data,
		sizeof(struct drm_psb_disp_ctrl)); */
	DRM_INFO("disp cmd:%d\n", dp_ctrl->cmd);
	if (dp_ctrl->cmd == DRM_PSB_DISP_SAVE_HDMI_FB_HANDLE) {
		hdmi_export_handle = dp_ctrl->u.data;
		DRM_INFO("save hdmi export handle:%d\n",
						hdmi_export_handle);
	} else if (dp_ctrl->cmd == DRM_PSB_DISP_GET_HDMI_FB_HANDLE) {
		dp_ctrl->u.data = hdmi_export_handle;
		DRM_INFO("retrive hdmi export handle:%d\n", hdmi_export_handle);
	}
	return 0;
}
#endif

#ifdef CONFIG_SUPPORT_HDMI
static int psb_query_hdcp_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	uint32_t *arg = data;
	uint8_t bksv[5];

	/* Attempting to read the BKSV value from the HDMI device.
	 * A successful read of this data indicates that HDCP is
	 * supported.  A value of zero would indicate that it's
	 * not.
	 */
	*arg = android_query_hdmi_hdcp_sink(dev, bksv);

	return 0;
}
static int psb_validate_hdcp_ksv_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	sqword_tt *arg = data;
	sqword_tt hw_bksv;
	if (android_query_hdmi_hdcp_sink(dev, (uint8_t *)&hw_bksv)) {
		*arg = hw_bksv;
		return 0;
	}

	return -1;
}
static int psb_get_hdcp_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	uint32_t *arg = data;
	*arg = android_check_hdmi_hdcp_enc_status(dev);

	return 0;
}
static int psb_enable_hdcp_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	int ret;
	ret = android_enable_hdmi_hdcp(dev);
	if (ret)
		return 0;
	else
		return -1;
}
static int psb_disable_hdcp_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	int ret;
	ret = android_disable_hdmi_hdcp(dev);
	if (ret)
		return 0;
	else
		return -1;
}
static int psb_enable_display_ied_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	int ret = 0;
	int temp = 0;

	if (power_island_get(OSPM_DISPLAY_ISLAND)) {
		temp = PSB_RVDC32(DSPCHICKENBIT);
		temp &= ~(1 << 31);
		PSB_WVDC32(temp, DSPCHICKENBIT);
		temp = PSB_RVDC32(DSPCHICKENBIT);
		power_island_put(OSPM_DISPLAY_ISLAND);
	} else
		ret = -1;

	return ret;
}
static int psb_disable_display_ied_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	int ret = 0;
	int temp = 0;

	if (power_island_get(OSPM_DISPLAY_ISLAND)) {
		temp = PSB_RVDC32(DSPCHICKENBIT);
		temp |= (1 << 31);
		PSB_WVDC32(temp, DSPCHICKENBIT);
		temp = PSB_RVDC32(DSPCHICKENBIT);
		power_island_put(OSPM_DISPLAY_ISLAND);
	} else
		ret = -1;

	return ret;
}
static int psb_query_display_ied_caps_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	uint32_t *arg = data;

	/* IED control is always enabled on merrifield platform */
	*arg = 1;

	return 0;
}

static int psb_get_hdcp_link_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	uint32_t *arg = data;
	*arg = android_check_hdmi_hdcp_link_status(dev);

	return 0;
}


#endif /* ifdef CONFIG_SUPPORT_HDMI */

static int psb_set_csc_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	struct drm_psb_csc_matrix *csc_matrix = data;
        struct csc_setting csc;

	csc.pipe = csc_matrix->pipe;
	csc.type = CSC_SETTING;
	csc.enable_state = true;
	csc.data_len = CSC_COUNT;
	memcpy(csc.data.csc_data, csc_matrix->matrix, sizeof(csc.data.csc_data));
	drm_psb_enable_color_conversion = 1;
	mdfld_intel_crtc_set_color_conversion(dev, &csc);

	return 0;
}

static int psb_dc_state_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	uint32_t flags;
	uint32_t obj_id;
	struct drm_mode_object *obj;
	struct drm_connector *connector;
	struct drm_crtc *crtc;
	struct drm_psb_dc_state_arg *arg = (struct drm_psb_dc_state_arg *)data;

	if (IS_MID(dev))
		return 0;

	flags = arg->flags;
	obj_id = arg->obj_id;

	if (flags & PSB_DC_CRTC_MASK) {
		obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_CRTC);
		if (!obj) {
			DRM_DEBUG("Invalid CRTC object.\n");
			return -EINVAL;
		}

		crtc = obj_to_crtc(obj);

		mutex_lock(&dev->mode_config.mutex);
		if (drm_helper_crtc_in_use(crtc)) {
			if (flags & PSB_DC_CRTC_SAVE)
				crtc->funcs->save(crtc);
			else
				crtc->funcs->restore(crtc);
		}
		mutex_unlock(&dev->mode_config.mutex);

		return 0;
	} else if (flags & PSB_DC_OUTPUT_MASK) {
		obj = drm_mode_object_find(dev, obj_id,
					   DRM_MODE_OBJECT_CONNECTOR);
		if (!obj) {
			DRM_DEBUG("Invalid connector id.\n");
			return -EINVAL;
		}

		connector = obj_to_connector(obj);
		if (flags & PSB_DC_OUTPUT_SAVE)
			connector->funcs->save(connector);
		else
			connector->funcs->restore(connector);

		return 0;
	}

	DRM_DEBUG("Bad flags 0x%x\n", flags);
	return -EINVAL;
}

#ifdef CONFIG_SUPPORT_MIPI
static int psb_dpst_bl_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;
	dev_priv->blc_adj2 = *arg;

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	{
		struct backlight_device bd;
		bd.props.brightness = psb_get_brightness(&bd);
		psb_set_brightness(&bd);
	}
#endif
	return 0;
}
#endif

static int psb_adb_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t *arg = data;
	dev_priv->blc_adj1 = *arg;

#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
	{
		struct backlight_device bd;
		bd.props.brightness = psb_get_brightness(&bd);
		psb_set_brightness(&bd);
	}
#endif
	return 0;
}

#ifdef CONFIG_SUPPORT_MIPI
static int psb_hist_enable_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	u32 irqCtrl = 0;
	struct dpst_guardband guardband_reg;
	struct dpst_ie_histogram_control ie_hist_cont_reg;
	uint32_t *enable = data;

	/* FIXME: revisit the power island when touching the DPST feature. */
	if (!power_island_get(OSPM_DISPLAY_A))
		return 0;

	if (*enable == 1) {
		ie_hist_cont_reg.data = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
		ie_hist_cont_reg.ie_pipe_assignment = 0;
		ie_hist_cont_reg.histogram_mode_select = DPST_YUV_LUMA_MODE;
		ie_hist_cont_reg.ie_histogram_enable = 1;
		PSB_WVDC32(ie_hist_cont_reg.data, HISTOGRAM_LOGIC_CONTROL);

		guardband_reg.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		guardband_reg.interrupt_enable = 1;
		guardband_reg.interrupt_status = 1;
		PSB_WVDC32(guardband_reg.data, HISTOGRAM_INT_CONTROL);

		irqCtrl = PSB_RVDC32(PIPEASTAT);
		PSB_WVDC32(irqCtrl | PIPE_DPST_EVENT_ENABLE, PIPEASTAT);
		/* Wait for two vblanks */
	} else {
		guardband_reg.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
		guardband_reg.interrupt_enable = 0;
		guardband_reg.interrupt_status = 1;
		PSB_WVDC32(guardband_reg.data, HISTOGRAM_INT_CONTROL);

		ie_hist_cont_reg.data = PSB_RVDC32(HISTOGRAM_LOGIC_CONTROL);
		ie_hist_cont_reg.ie_histogram_enable = 0;
		PSB_WVDC32(ie_hist_cont_reg.data, HISTOGRAM_LOGIC_CONTROL);

		irqCtrl = PSB_RVDC32(PIPEASTAT);
		irqCtrl &= ~PIPE_DPST_EVENT_ENABLE;
		PSB_WVDC32(irqCtrl, PIPEASTAT);
	}

	power_island_put(OSPM_DISPLAY_A);

	return 0;
}

static int psb_hist_status_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_psb_hist_status_arg *hist_status = data;
	uint32_t *arg = hist_status->buf;
	u32 iedbr_reg_data = 0;
	struct dpst_ie_histogram_control ie_hist_cont_reg;
	u32 i;
	int dpst3_bin_threshold_count = 0;
	uint32_t blm_hist_ctl = HISTOGRAM_LOGIC_CONTROL;
	uint32_t iebdr_reg = HISTOGRAM_BIN_DATA;
	uint32_t segvalue_max_22_bit = 0x3fffff;
	uint32_t iedbr_busy_bit = 0x80000000;
	int dpst3_bin_count = 32;

	/* FIXME: revisit the power island when touching the DPST feature. */
	if (!power_island_get(OSPM_DISPLAY_A))
		return 0;

	ie_hist_cont_reg.data = PSB_RVDC32(blm_hist_ctl);
	ie_hist_cont_reg.bin_reg_func_select = dpst3_bin_threshold_count;
	ie_hist_cont_reg.bin_reg_index = 0;

	PSB_WVDC32(ie_hist_cont_reg.data, blm_hist_ctl);

	for (i = 0; i < dpst3_bin_count; i++) {
		iedbr_reg_data = PSB_RVDC32(iebdr_reg);

		if (!(iedbr_reg_data & iedbr_busy_bit)) {
			arg[i] = iedbr_reg_data & segvalue_max_22_bit;
		} else {
			i = 0;
			ie_hist_cont_reg.data = PSB_RVDC32(blm_hist_ctl);
			ie_hist_cont_reg.bin_reg_index = 0;
			PSB_WVDC32(ie_hist_cont_reg.data, blm_hist_ctl);
		}
	}

	power_island_put(OSPM_DISPLAY_A);

	return 0;
}

static int psb_init_comm_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct pci_dev *pdev = NULL;
	struct device *ddev = NULL;
	struct kobject *kobj = NULL;
	uint32_t *arg = data;

	if (*arg == 1) {
		/*find handle to drm kboject */
		pdev = dev->pdev;
		ddev = &pdev->dev;
		kobj = &ddev->kobj;

		if (dev_priv->psb_dpst_state == NULL) {
			/*init dpst kmum comms */
			dev_priv->psb_dpst_state = psb_dpst_init(kobj);
		} else {
			PSB_DEBUG_ENTRY("DPST already initialized\n");
		}

		psb_irq_enable_dpst(dev);
		psb_dpst_notify_change_um(DPST_EVENT_INIT_COMPLETE,
					  dev_priv->psb_dpst_state);
	} else {
		/*hotplug and dpst destroy examples */
		psb_irq_disable_dpst(dev);
		psb_dpst_notify_change_um(DPST_EVENT_TERMINATE,
					  dev_priv->psb_dpst_state);
		psb_dpst_device_pool_destroy(dev_priv->psb_dpst_state);
		dev_priv->psb_dpst_state = NULL;
	}
	return 0;
}

/* return the current mode to the dpst module */
static int psb_dpst_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	uint32_t *arg = data;
	uint32_t x;
	uint32_t y;
	uint32_t reg;

	if (!power_island_get(OSPM_DISPLAY_A))
		return 0;

	reg = PSB_RVDC32(PIPEASRC);

	power_island_put(OSPM_DISPLAY_A);

	/* horizontal is the left 16 bits */
	x = reg >> 16;
	/* vertical is the right 16 bits */
	y = reg & 0x0000ffff;

	/* the values are the image size minus one */
	x += 1;
	y += 1;

	*arg = (x << 16) | y;

	return 0;
}

static int psb_gamma_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_psb_dpst_lut_arg *lut_arg = data;
	struct drm_mode_object *obj;
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct psb_intel_crtc *psb_intel_crtc;
	int i = 0;
	int32_t obj_id;

	obj_id = lut_arg->output_id;
	obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_CONNECTOR);
	if (!obj) {
		DRM_DEBUG("Invalid Connector object.\n");
		return -EINVAL;
	}

	connector = obj_to_connector(obj);
	crtc = connector->encoder->crtc;
	psb_intel_crtc = to_psb_intel_crtc(crtc);

	for (i = 0; i < 256; i++)
		psb_intel_crtc->lut_adj[i] = lut_arg->lut[i];

	psb_intel_crtc_load_lut(crtc);

	return 0;
}

static int psb_update_guard_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv)
{
	struct dpst_guardband *input = (struct dpst_guardband *)data;
	struct dpst_guardband reg_data;

	/* FIXME: revisit the power island when touching the DPST feature. */
	if (!power_island_get(OSPM_DISPLAY_A))
		return 0;

	reg_data.data = PSB_RVDC32(HISTOGRAM_INT_CONTROL);
	reg_data.guardband = input->guardband;
	reg_data.guardband_interrupt_delay = input->guardband_interrupt_delay;
	/* PSB_DEBUG_ENTRY( "guardband = %u\ninterrupt delay = %u\n",
	   reg_data.guardband, reg_data.guardband_interrupt_delay); */
	PSB_WVDC32(reg_data.data, HISTOGRAM_INT_CONTROL);

	power_island_put(OSPM_DISPLAY_A);

	return 0;
}
#endif

static int psb_mode_operation_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv)
{
	uint32_t obj_id;
	uint16_t op;
	struct drm_mode_modeinfo *umode;
	struct drm_display_mode *mode = NULL;
	struct drm_psb_mode_operation_arg *arg;
	struct drm_mode_object *obj;
	struct drm_connector *connector;
	struct drm_framebuffer *drm_fb;
	struct psb_framebuffer *psb_fb;
	struct drm_connector_helper_funcs *connector_funcs;
	int ret = 0;
	struct drm_psb_private *dev_priv = psb_priv(dev);

	arg = (struct drm_psb_mode_operation_arg *)data;
	obj_id = arg->obj_id;
	op = arg->operation;

	switch (op) {
	case PSB_MODE_OPERATION_SET_DC_BASE:
		obj = drm_mode_object_find(dev, obj_id, DRM_MODE_OBJECT_FB);
		if (!obj) {
			DRM_ERROR("Invalid FB id %d\n", obj_id);
			return -EINVAL;
		}

		drm_fb = obj_to_fb(obj);
		psb_fb = to_psb_fb(drm_fb);

		if (power_island_get(OSPM_DISPLAY_A)) {
			REG_WRITE(DSPASURF, psb_fb->offset);
			REG_READ(DSPASURF);
			power_island_put(OSPM_DISPLAY_A);
		} else {
			dev_priv->saveDSPASURF = psb_fb->offset;
		}

		return 0;
	case PSB_MODE_OPERATION_MODE_VALID:
		umode = &arg->mode;

		mutex_lock(&dev->mode_config.mutex);

		obj =
		    drm_mode_object_find(dev, obj_id,
					 DRM_MODE_OBJECT_CONNECTOR);
		if (!obj) {
			ret = -EINVAL;
			goto mode_op_out;
		}

		connector = obj_to_connector(obj);

		mode = drm_mode_create(dev);
		if (!mode) {
			ret = -ENOMEM;
			goto mode_op_out;
		}

		/* drm_crtc_convert_umode(mode, umode); */
		{
			mode->clock = umode->clock;
			mode->hdisplay = umode->hdisplay;
			mode->hsync_start = umode->hsync_start;
			mode->hsync_end = umode->hsync_end;
			mode->htotal = umode->htotal;
			mode->hskew = umode->hskew;
			mode->vdisplay = umode->vdisplay;
			mode->vsync_start = umode->vsync_start;
			mode->vsync_end = umode->vsync_end;
			mode->vtotal = umode->vtotal;
			mode->vscan = umode->vscan;
			mode->vrefresh = umode->vrefresh;
			mode->flags = umode->flags;
			mode->type = umode->type;
			strncpy(mode->name, umode->name, DRM_DISPLAY_MODE_LEN);
			mode->name[DRM_DISPLAY_MODE_LEN - 1] = 0;
		}

		connector_funcs = (struct drm_connector_helper_funcs *)
			connector->helper_private;

		if (connector_funcs->mode_valid) {
			arg->data = (void *)
				(uintptr_t) connector_funcs->
				mode_valid(connector, mode);
		}

		/*do some clean up work */
		if (mode)
			drm_mode_destroy(dev, mode);
 mode_op_out:
		mutex_unlock(&dev->mode_config.mutex);
		return ret;

	default:
		DRM_DEBUG("Unsupported psb mode operation");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int psb_stolen_memory_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct drm_psb_stolen_memory_arg *arg = data;

	arg->base = dev_priv->pg->stolen_base;
	arg->size = dev_priv->pg->vram_stolen_size;

	return 0;
}

#ifdef CONFIG_SUPPORT_MIPI
static int psb_dpu_query_ioctl(struct drm_device *dev, void *arg,
			       struct drm_file *file_priv)
{
	int *data = (int *)arg;
	DRM_DRIVER_PRIVATE_T *dev_priv = dev->dev_private;
	mdfld_dsi_encoder_t encoder_type;

	/*reject requests from non-flds platforms */
	if (!IS_FLDS(dev)) {
		DRM_INFO("Not Medfield or Merrifield platform! return.");
		return -EOPNOTSUPP;
	}
	DRM_INFO("dsr query.\n");

	dev_priv->um_start = true;
	encoder_type = is_panel_vid_or_cmd(dev);

	if (encoder_type == MDFLD_DSI_ENCODER_DPI) {
		DRM_INFO("DSI panel is working in video mode\n");
		dev_priv->b_dsr_enable = false;
		*data = 0;
		return 0;
	}
#if defined(CONFIG_MID_DSI_DSR)
	dev_priv->b_dsr_enable = true;
	*data = MDFLD_DSR_RR | MDFLD_DSR_FULLSCREEN;
#elif defined(CONFIG_MID_DSI_DPU)
	dev_priv->b_dsr_enable = true;
	*data = MDFLD_DSR_RR | MDFLD_DPU_ENABLE;
#else				/*DBI panel but DSR was not defined */
	DRM_INFO("DSR is disabled by kernel configuration.\n");

	dev_priv->b_dsr_enable = false;
	*data = 0;
#endif				/*CONFIG_MID_DSI_DSR */
	return 0;
}

static int psb_dpu_dsr_on_ioctl(struct drm_device *dev, void *arg,
				struct drm_file *file_priv)
{
	u32 *param = (u32 *) arg;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	mdfld_dsi_encoder_t encoder_type;

	/*reject requests from non-fld platforms */
	if (!IS_FLDS(dev)) {
		DRM_INFO("Not Medfield or Merrifield platform! return.");
		return -EOPNOTSUPP;
	}

	encoder_type = is_panel_vid_or_cmd(dev);

	if (encoder_type == MDFLD_DSI_ENCODER_DPI) {
		DRM_INFO("DSI panel is working in video mode\n");
		dev_priv->b_dsr_enable = false;
		return 0;
	}

	if (!param) {
		DRM_ERROR("Invalid parameter\n");
		return -EINVAL;
	}

	PSB_DEBUG_ENTRY("dsr kick in. param 0x%08x\n", *param);

	if (*param == DRM_PSB_DSR_DISABLE) {
		PSB_DEBUG_ENTRY("DSR is turned off\n");
		dev_priv->b_dsr_enable = false;
#if defined(CONFIG_MID_DSI_DPU)
		mdfld_dbi_dpu_report_fullscreen_damage(dev);
#elif defined(CONFIG_MID_DSI_DSR)
		mdfld_dsi_dbi_exit_dsr(dev, MDFLD_DSR_2D_3D, 0, 0);
#endif
		return 0;
	} else if (*param == DRM_PSB_DSR_ENABLE) {
		PSB_DEBUG_ENTRY("DSR is turned on\n");
#if defined(CONFIG_MID_DSI_DPU) || defined(CONFIG_MID_DSI_DSR)
		dev_priv->b_dsr_enable = true;
#endif
		return 0;
	}

	return -EINVAL;
}

static int psb_dpu_dsr_off_ioctl(struct drm_device *dev, void *arg,
				 struct drm_file *file_priv)
{
#if defined(CONFIG_MID_DSI_DPU)
	struct drm_psb_drv_dsr_off_arg *dsr_off_arg =
	    (struct drm_psb_drv_dsr_off_arg *)arg;
	struct psb_drm_dpu_rect rect = dsr_off_arg->damage_rect;

	return mdfld_dsi_dbi_dsr_off(dev, &rect);
#elif defined(CONFIG_MID_DSI_DSR)
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	if ((dev_priv->dsr_fb_update & MDFLD_DSR_2D_3D) !=
		MDFLD_DSR_2D_3D)
			mdfld_dsi_dbi_exit_dsr(dev, MDFLD_DSR_2D_3D, 0, 0);
#if 0
	{
		static int pipe;

		if (pipe > 0) {
			pipe = 0;
			if (gdbi_output && gbdispstatus == false) {
				dev_priv->b_dsr_enable = true;
				mdfld_dsi_dbi_enter_dsr(gdbi_output, 1);
				mdfld_dsi_dbi_enter_dsr(gdbi_output, 2);
			}
		}
	}
#endif

#endif
	return 0;
}
#endif

#if KEEP_UNUSED_CODE
/*wait for vblank*/
static void overlay_wait_vblank(struct drm_device *dev, uint32_t ovadd)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t ovadd_pipe;
	uint32_t pipestat_reg;
	int retry;

	ovadd_pipe = ((ovadd >> 6) & 0x3);
	switch (ovadd_pipe) {
	case 0:
		pipestat_reg = PIPEASTAT;
		break;
	case 1:
		pipestat_reg = PIPECSTAT;
		break;
	case 2:
		pipestat_reg = PIPEBSTAT;
		break;
	default:
		DRM_ERROR("wrong OVADD pipe seletion\n");
		return;
	}

	/**
	 * wait for vblank upto 30ms,the period of vblank is 22ms.
	 */
	retry = 3000;
	while (--retry) {
		if ((PSB_RVDC32(pipestat_reg) & PIPE_VBLANK_STATUS))
			break;
		udelay(10);
	}

	if (!retry)
		DRM_ERROR("wait vblank timeout!\n");
}
#endif /* if KEEP_UNUSED_CODE */

static void vsync_state_dump(struct drm_device *dev, int pipe)
{
	if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND, OSPM_UHB_FORCE_POWER_ON))
		return;

	DRM_INFO("vblank_refcount = %u\n", atomic_read(&dev->vblank_refcount[pipe]));
	DRM_INFO("vblank_enabled = %d\n", dev->vblank_enabled[pipe]);
	DRM_INFO("vblank_count = %u\n", drm_vblank_count(dev, pipe));
	DRM_INFO("PIPECONF = 0x%08x\n", pipe ? REG_READ(PIPEBCONF) : REG_READ(PIPEACONF));
	DRM_INFO("PIPESTAT = 0x%08x\n\n", pipe ? REG_READ(PIPEBSTAT) : REG_READ(PIPEASTAT));
	ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
}

static int psb_vsync_set_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct drm_psb_vsync_set_arg *arg = data;
#ifdef CONFIG_SUPPORT_MIPI
	struct mdfld_dsi_config *dsi_config = NULL;
#endif
	struct timespec now;
	uint32_t pipe;
	union drm_wait_vblank vblwait;
	u32 vbl_count = 0;
	s64 nsecs = 0;
	int ret = -EINVAL;

	if (arg->vsync_operation_mask) {
		pipe = arg->vsync.pipe;
		if (pipe < 0 || pipe > 2) {
			DRM_ERROR("%s: invalid pipe %d", __func__, pipe);
			return -EINVAL;
		}

		if (arg->vsync_operation_mask & GET_VSYNC_COUNT) {
			vbl_count = drm_vblank_count(dev, pipe);

			getrawmonotonic(&now);
			nsecs = timespec_to_ns(&now);

			arg->vsync.timestamp = (uint64_t)nsecs;
			arg->vsync.vsync_count = (uint64_t)vbl_count;
		}

#ifdef CONFIG_SUPPORT_MIPI
		if (!pipe)
			dsi_config = dev_priv->dsi_configs[0];
		else if (pipe == 2)
			dsi_config = dev_priv->dsi_configs[1];
#endif

		if (arg->vsync_operation_mask & VSYNC_WAIT) {
			if (IS_MOFD(dev))
				mutex_lock(&dev->mode_config.mutex);

#ifdef CONFIG_SUPPORT_MIPI
			if (dev_priv->vsync_enabled[pipe] && ((pipe == 1) ||
						(dsi_config &&
						 dsi_config->dsi_hw_context.panel_on))) {
#else
			if (dev_priv->vsync_enabled[pipe] && (pipe == 1)) {
#endif
				vblwait.request.type =
					(_DRM_VBLANK_RELATIVE |
					 _DRM_VBLANK_NEXTONMISS);
				vblwait.request.sequence = 1;

				if (pipe == 1)
					vblwait.request.type |=
						_DRM_VBLANK_SECONDARY;

				ret = drm_wait_vblank(dev, (void *)&vblwait,
						file_priv);
				if (ret && (ret != -EINTR)) {
					DRM_ERROR("fail to get vsync on pipe %d, ret %d\n", pipe, ret);
					vsync_state_dump(dev, pipe);

#ifdef CONFIG_SUPPORT_MIPI
					if (!IS_ANN(dev)) {
						if (pipe != 1 &&
							is_panel_vid_or_cmd(dev) == MDFLD_DSI_ENCODER_DBI &&
							dsi_config &&
							dsi_config->dsi_hw_context.panel_on) {
							schedule_work(&dev_priv->reset_panel_work);
						}
					}
#endif
				} else if (ret == -EINTR)
					ret = 0;
			}else{
				DRM_INFO("request VSYNC on pipe(%d) when vsync_enabled=%d.\n",
						 pipe, dev_priv->vsync_enabled[pipe]);
			}

			if (IS_MOFD(dev))
				mutex_unlock(&dev->mode_config.mutex);

			getrawmonotonic(&now);
			nsecs = timespec_to_ns(&now);
			arg->vsync.timestamp = (uint64_t)nsecs;
			return ret;
		}

		if (arg->vsync_operation_mask & VSYNC_ENABLE) {
			if (dev_priv->vsync_enabled[pipe]) {
				DRM_ERROR("%s: vsync has been enabled on pipe %d",
						__func__, pipe);
				return 0;
			}
#ifdef CONFIG_SUPPORT_MIPI
			mdfld_dsi_dsr_forbid(dsi_config);
#if 0
			ret = drm_vblank_get(dev, pipe);
			if (ret != 0) {
				DRM_ERROR("%s: fail to enable vsync on pipe %d\n",
						__func__, pipe);
				mdfld_dsi_dsr_allow(dsi_config);
			} else
#endif
#endif
			dev_priv->vsync_enabled[pipe] = true;
			ret = 0;
		}

		if (arg->vsync_operation_mask & VSYNC_DISABLE) {
			if (!dev_priv->vsync_enabled[pipe]) {
				DRM_ERROR("%s: vsync has been disabled on pipe %d",
						__func__, pipe);
				return 0;
			}
			dev_priv->vsync_enabled[pipe] = false;
#ifdef CONFIG_SUPPORT_MIPI
#if 0
			drm_vblank_put(dev, pipe);
#endif
			mdfld_dsi_dsr_allow(dsi_config);
#endif
			ret = 0;
		}
	}

	return ret;
}

static int psb_register_rw_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_psb_register_rw_arg *arg = data;
#ifdef CONFIG_SUPPORT_MIPI
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct mdfld_dsi_config *dsi_config = NULL;
#endif
	u32 power_island = 0;

	if (arg->overlay_write_mask != 0) {
		if (arg->overlay_write_mask & OV_REGRWBITS_OGAM_ALL)
			power_island |= OSPM_DISPLAY_A;

		if (arg->overlay_write_mask & OVC_REGRWBITS_OGAM_ALL)
			power_island |= OSPM_DISPLAY_C;

		if (power_island_get(power_island)) {
			u32 index = arg->overlay.index;
			u32 ov_ogamc5_reg;
			u32 ov_ogamc4_reg;
			u32 ov_ogamc3_reg;
			u32 ov_ogamc2_reg;
			u32 ov_ogamc1_reg;
			u32 ov_ogamc0_reg;

			switch (index) {
			case 0:  /* overlay A */
				ov_ogamc5_reg = OV_OGAMC5;
				ov_ogamc4_reg = OV_OGAMC4;
				ov_ogamc3_reg = OV_OGAMC3;
				ov_ogamc2_reg = OV_OGAMC2;
				ov_ogamc1_reg = OV_OGAMC1;
				ov_ogamc0_reg = OV_OGAMC0;
				break;
			case 1:  /* overlay C */
				ov_ogamc5_reg = OVC_OGAMC5;
				ov_ogamc4_reg = OVC_OGAMC4;
				ov_ogamc3_reg = OVC_OGAMC3;
				ov_ogamc2_reg = OVC_OGAMC2;
				ov_ogamc1_reg = OVC_OGAMC1;
				ov_ogamc0_reg = OVC_OGAMC0;
				break;
			default:
				DRM_ERROR("Invalid overlay index %d\n", index);
				return -EINVAL;
			}

#ifdef CONFIG_SUPPORT_MIPI
			/*forbid dsr which will restore regs*/
			dsi_config = dev_priv->dsi_configs[0];
			mdfld_dsi_dsr_forbid(dsi_config);
#endif

			if (arg->overlay_write_mask & OV_REGRWBITS_OGAM_ALL) {
				PSB_WVDC32(arg->overlay.OGAMC5, ov_ogamc5_reg);
				PSB_WVDC32(arg->overlay.OGAMC4, ov_ogamc4_reg);
				PSB_WVDC32(arg->overlay.OGAMC3, ov_ogamc3_reg);
				PSB_WVDC32(arg->overlay.OGAMC2, ov_ogamc2_reg);
				PSB_WVDC32(arg->overlay.OGAMC1, ov_ogamc1_reg);
				PSB_WVDC32(arg->overlay.OGAMC0, ov_ogamc0_reg);
			}

#ifdef CONFIG_SUPPORT_MIPI
			if (arg->overlay_write_mask & OV_REGRWBITS_OVADD) {
				PSB_WVDC32(arg->overlay.OVADD, OV_OVADD);
				if (arg->overlay.b_wms){
					mdfld_dsi_dsr_update_panel_fb(dsi_config);
				}
			}
			/*allow entering dsr*/
			mdfld_dsi_dsr_allow(dsi_config);
#endif

			power_island_put(power_island);
		}
	}
#if 0
	if (arg->plane_enable_mask != 0)
		DC_MRFLD_Enable_Plane(arg->plane.type,
				arg->plane.index, arg->plane.ctx);

	if (arg->plane_disable_mask != 0)
		DC_MRFLD_Disable_Plane(arg->plane.type,
				arg->plane.index, arg->plane.ctx);
#endif
	if (arg->get_plane_state_mask != 0) {
		u32 pipe = arg->plane.ctx;
		bool bDisabled = DC_MRFLD_Is_Plane_Disabled(arg->plane.type,
						arg->plane.index, pipe);
		if (bDisabled)
			arg->plane.ctx = PSB_DC_PLANE_DISABLED;
		else
			arg->plane.ctx = PSB_DC_PLANE_ENABLED;
	}

	if (arg->overlay_read_mask & OVSTATUS_REGRBIT_OVR_UPDT) {
		u32 ovstat_reg = OV_DOVASTA;
		u32 pipe = arg->plane.ctx;
		u32 pipeconf_reg;
		power_island |= OSPM_DISPLAY_A;
		if (arg->plane.index) {
			power_island |= OSPM_DISPLAY_C;
			ovstat_reg = OVC_DOVCSTA;
		}
		/* By default overlay is not updated since last vblank event*/
		arg->plane.ctx = 1;
		if (pipe == PIPEA)
			pipeconf_reg = PIPEACONF;
		else if (pipe == PIPEB)
			pipeconf_reg = PIPEBCONF;
		else {
			DRM_ERROR("Invalid pipe:%d!\n", pipe);
			return -EINVAL;
		}

		if (REG_READ(pipeconf_reg) & BIT31) {
			if (power_island_get(power_island)) {
				arg->plane.ctx =
					(PSB_RVDC32(ovstat_reg) & BIT31) == 0 ? 0 : 1;
				power_island_put(power_island);
			}
		} else
			DRM_INFO("%s: pipe %d is disabled!\n", __func__, pipe);
	}
	return 0;
}

static int psb_get_panel_orientation_ioctl(struct drm_device *dev, void *data,
						 struct drm_file *file_priv)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	int *arg = data;

	if (dev_priv->panel_180_rotation)
            *arg = 1; // panel orientation 180
        else
            *arg = 0;
	return 0;
}

static int psb_update_cursor_pos_ioctl(struct drm_device *dev, void *data,
						 struct drm_file *file_priv)
{
	struct intel_dc_cursor_ctx *ctx = (struct intel_dc_cursor_ctx*) data;

	if (ctx == NULL) {
		DRM_ERROR("%s: invalid cursor context\n", __func__);
		return -1;
	}

	return DCUpdateCursorPos(ctx->pipe, ctx->pos);
}


/* always available as we are SIGIO'd */
static unsigned int psb_poll(struct file *filp, struct poll_table_struct *wait)
{
	return POLLIN | POLLRDNORM;
}

static int psb_driver_open(struct drm_device *dev, struct drm_file *priv)
{
	DRM_DEBUG("\n");
	return PVRSRVOpen(dev, priv);
}

static long psb_unlocked_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	unsigned int nr = DRM_IOCTL_NR(cmd);
	long ret;

	DRM_DEBUG("cmd = %x, nr = %x\n", cmd, nr);
	/*
	 * The driver private ioctls and TTM ioctls should be
	 * thread-safe.
	 */

	if ((nr >= DRM_COMMAND_BASE) && (nr < DRM_COMMAND_END)
	    && (nr < DRM_COMMAND_BASE + dev->driver->num_ioctls)) {
		struct drm_ioctl_desc *ioctl =
		    &psb_ioctls[nr - DRM_COMMAND_BASE];

		if (unlikely(ioctl->cmd != cmd)) {
			DRM_ERROR("Invalid drm cmnd %d ioctl->cmd %x, cmd %x\n",
				  nr - DRM_COMMAND_BASE, ioctl->cmd, cmd);
			return -EINVAL;
		}
	}
	/*
	 * Not all old drm ioctls are thread-safe.
	 */

	ret = drm_ioctl(filp, cmd, arg);

	return ret;
}

static int psb_blc_proc_show(struct seq_file *seq, void *v)
{
	struct drm_minor *minor = (struct drm_minor *)seq->private;
	struct drm_device *dev = minor->dev;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	int user_brightness = 0;
	int final_brightness = 0;

	user_brightness = psb_get_brightness(NULL);
	final_brightness = (user_brightness * dev_priv->blc_adj1) / 255;
	final_brightness = (final_brightness * dev_priv->blc_adj2) / 255;

	DRM_INFO("%i\n", final_brightness);
	seq_printf(seq, "%i\n", final_brightness);

	return 0;
}

static int psb_blc_proc_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
	return single_open(file, psb_blc_proc_show, PDE(inode)->data);
#else
	return single_open(file, psb_blc_proc_show, PDE_DATA(inode));
#endif
}

static const struct file_operations psb_blc_proc_fops = {
	.owner = THIS_MODULE,
	.open = psb_blc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_rtpm_read(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	PSB_DEBUG_ENTRY("Current Runtime PM delay for GFX: %d (ms)\n",
			gfxrtdelay);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_rtpm_write(struct file *file, const char *buffer,
			  unsigned long count, void *data)
{
	char buf[2];
	int temp = 0;
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count - 1] != '\n')
			return -EINVAL;
		temp = buf[0] - '0';
		switch (temp) {
		case 1:
			gfxrtdelay = 10 * 1000;
			break;

		case 2:
			gfxrtdelay = 20 * 1000;
			break;
		default:
			gfxrtdelay = 30 * 1000;
			break;
		}
		PSB_DEBUG_ENTRY("Runtime PM delay set for GFX: %d (ms)\n",
				gfxrtdelay);
	}
	return count;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_gfx_pm_read(char *buf, char **start, off_t offset, int request,
			   int *eof, void *data)
{
	printk(KERN_ALERT "drm_psb_gfx_pm: %d\n", drm_psb_gfx_pm);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_gfx_pm_write(struct file *file, const char *buffer,
			    unsigned long count, void *data)
{
	char buf[2];
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count - 1] != '\n')
			return -EINVAL;
		drm_psb_gfx_pm = buf[0] - '0';
	}

	printk(KERN_ALERT "\n drm_psb_gfx_pm: %x\n", drm_psb_gfx_pm);

	if (drm_psb_gfx_pm) {
		printk(KERN_ALERT "\n Starting Test Sequence: %x\n",
		       drm_psb_gfx_pm);
#if 0	/* IF MRFLD */
		{
			int i = 0;
			/*  Invalid - MRFLD_GFX_ALL_ISLANDS is a bit mask.*/
			for (i = 0; i < MRFLD_GFX_ALL_ISLANDS; i++) {
				mrfld_set_power_state(OSPM_DISPLAY_ISLAND,
							i,
							POWER_ISLAND_DOWN);
				mdelay(1);
				mrfld_set_power_state(OSPM_DISPLAY_ISLAND,
							i,
							POWER_ISLAND_UP);
			}
		}
#else
		mrfld_set_power_state(OSPM_DISPLAY_ISLAND,
					MRFLD_GFX_ALL_ISLANDS,
					POWER_ISLAND_DOWN);
		mdelay(1);
		mrfld_set_power_state(OSPM_DISPLAY_ISLAND,
					MRFLD_GFX_ALL_ISLANDS,
					POWER_ISLAND_UP);
#endif
	}
	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_vsp_pm_read(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	printk(KERN_ALERT "psb_vsp_pm_read: %d\n", drm_psb_vsp_pm);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_vsp_pm_write(struct file *file, const char *buffer,
			  unsigned long count, void *data)
{
	char buf[2];
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count-1] != '\n')
			return -EINVAL;
		drm_psb_vsp_pm = buf[0] - '0';
	}

	printk(KERN_ALERT  "\n drm_psb_vsp_pm=%x\n", drm_psb_vsp_pm);

	if (drm_psb_gfx_pm == 0) {
		printk(KERN_ALERT "\n Starting power off the VSP...\n");

		mrfld_set_power_state(OSPM_VIDEO_VPP_ISLAND,
					0,
					POWER_ISLAND_DOWN);

	} else if (drm_psb_gfx_pm == 1) {
		printk(KERN_ALERT "\n Starting power on the VSP...\n");
		mrfld_set_power_state(OSPM_VIDEO_VPP_ISLAND,
					0,
					POWER_ISLAND_UP);
	} else {
		printk(KERN_ALERT "\n Don't support this operation! %x\n",
			drm_psb_vsp_pm);
	}

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_ved_pm_read(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	printk(KERN_ALERT "drm_psb_ved_pm: %d\n", drm_psb_ved_pm);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_ved_pm_write(struct file *file, const char *buffer,
			  unsigned long count, void *data)
{
	char buf[2];
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count-1] != '\n')
			return -EINVAL;
		drm_psb_ved_pm = buf[0] - '0';
	}

	printk(KERN_ALERT  "\n drm_psb_ved_pm=%x\n", drm_psb_ved_pm);

	if (drm_psb_ved_pm == 0) {
		printk(KERN_ALERT "\n Starting power off the VED...\n");

		mrfld_set_power_state(OSPM_VIDEO_DEC_ISLAND,
					0,
					POWER_ISLAND_DOWN);

	} else if (drm_psb_ved_pm == 1) {
		printk(KERN_ALERT "\n Starting power on the VED...\n");
		mrfld_set_power_state(OSPM_VIDEO_DEC_ISLAND,
					0,
					POWER_ISLAND_UP);
	} else {
		printk(KERN_ALERT "\n Don't support this operation! %x\n",
			drm_psb_ved_pm);
	}

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_vec_pm_read(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	printk(KERN_ALERT "drm_psb_vec_pm: %d\n", drm_psb_vec_pm);

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_vec_pm_write(struct file *file, const char *buffer,
			  unsigned long count, void *data)
{
	char buf[2];
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count-1] != '\n')
			return -EINVAL;
		drm_psb_vec_pm = buf[0] - '0';
	}

	printk(KERN_ALERT  "\n drm_psb_vec_pm=%x\n", drm_psb_vec_pm);

	if (drm_psb_vec_pm == 0) {
		printk(KERN_ALERT "\n Starting power off the VEC...\n");

		mrfld_set_power_state(OSPM_VIDEO_ENC_ISLAND,
					0,
					POWER_ISLAND_DOWN);

	} else if (drm_psb_vec_pm == 1) {
		printk(KERN_ALERT "\n Starting power on the VEC...\n");
		mrfld_set_power_state(OSPM_VIDEO_ENC_ISLAND,
					0,
					POWER_ISLAND_UP);
	} else {
		printk(KERN_ALERT "\n Don't support this operation! %x\n",
			drm_psb_vec_pm);
	}

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_dsr_read(char *buf, char **start, off_t offset, int request,
			int *eof, void *data)
{
	if (drm_psb_dsr)
		DRM_INFO("GFX DSR: enabled	      ");
	else
		DRM_INFO("GFX DSR: disabled	      ");

	return 0;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_dsr_write(struct file *file, const char *buffer,
			 unsigned long count, void *data)
{
	char buf[2];
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count - 1] != '\n')
			return -EINVAL;
		drm_psb_dsr = buf[0] - '0';
	}
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_ospm_read(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	struct drm_minor *minor = (struct drm_minor *)data;
	struct drm_device *dev = minor->dev;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	int len = 0;
#ifdef OSPM_STAT
	unsigned long on_time = 0;
	unsigned long off_time = 0;
#endif

	*start = &buf[offset];
	*eof = 0;

	/*#ifdef SUPPORT_ACTIVE_POWER_MANAGEMENT
	   DRM_INFO("GFX D0i3: enabled        ");
	   #else
	   DRM_INFO("GFX D0i3: disabled       ");
	   #endif */
	if (drm_psb_ospm)
		DRM_INFO("GFX D0i3: enabled	      ");
	else
		DRM_INFO("GFX D0i3: disabled	      ");

#ifdef OSPM_STAT
	switch (dev_priv->graphics_state) {
	case PSB_PWR_STATE_ON:
		DRM_INFO("GFX state:%s\n", "on");
		break;
	case PSB_PWR_STATE_OFF:
		DRM_INFO("GFX state:%s\n", "off");
		break;
	default:
		DRM_INFO("GFX state:%s\n", "unknown");
	}

	on_time = dev_priv->gfx_on_time * 1000 / HZ;
	off_time = dev_priv->gfx_off_time * 1000 / HZ;
	switch (dev_priv->graphics_state) {
	case PSB_PWR_STATE_ON:
		on_time += (jiffies - dev_priv->gfx_last_mode_change) *
		    1000 / HZ;
		break;
	case PSB_PWR_STATE_OFF:
		off_time += (jiffies - dev_priv->gfx_last_mode_change) *
		    1000 / HZ;
		break;
	}
	DRM_INFO("GFX(count/ms):\n");
	DRM_INFO("on:%lu/%lu, off:%lu/%lu\n",
		 dev_priv->gfx_on_cnt, on_time, dev_priv->gfx_off_cnt,
		 off_time);
#endif
	if (len > request + offset)
		return request;
	*eof = 1;
	return len - offset;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_ospm_write(struct file *file, const char *buffer,
			  unsigned long count, void *data)
{
	char buf[2];
	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count - 1] != '\n')
			return -EINVAL;
		drm_psb_ospm = buf[0] - '0';
		PSB_DEBUG_ENTRY(" SGX (D0i3) drm_psb_ospm: %d\n",
				drm_psb_ospm);
		/*Work around for video encode, it needs sgx always on */
		if (!drm_psb_ospm) {
			/* So weird */
			power_island_get(OSPM_GRAPHICS_ISLAND);
			power_island_put(OSPM_GRAPHICS_ISLAND);
		}
	}
	return count;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
static int psb_display_register_read(char *buf, char **start, off_t offset,
				     int request, int *eof, void *data)
{
	struct drm_minor *minor = (struct drm_minor *)data;
	struct drm_device *dev = minor->dev;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	/*do nothing */
	int len = dev_priv->count;
	*eof = 1;
	memcpy(buf, dev_priv->buf, dev_priv->count);
	return len - offset;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
/*
* use to read and write display register. and print to standard output.
*/
static int psb_display_register_write(struct file *file, const char *buffer,
				      unsigned long count, void *data)
{
	struct drm_minor *minor = (struct drm_minor *)data;
	struct drm_device *dev = minor->dev;
	struct drm_psb_private *dev_priv =
	    (struct drm_psb_private *)dev->dev_private;
	struct mdfld_dsi_config *dsi_config = dev_priv->dsi_configs[0];
	int reg_val = 0;
	char buf[256];
	char op = '0';
	int reg = 0, start = 0, end = 0;
	unsigned int val = 0;
	int len = 0;
	int Offset = 0;
	int ret = 0;

	dev_priv->count = 0;
	memset(buf, '\0', sizeof(buf));

	if (count > sizeof(buf)) {
		PSB_DEBUG_ENTRY
		    ("The input is too bigger, kernel can not handle.\n");
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count - 1] != '\n')
			return -EINVAL;
		PSB_DEBUG_ENTRY("input = %s", buf);
	}

	sscanf(buf, "%c%x%x", &op, &reg, &val);

	if (op != 'r' && op != 'w' && op != 'a') {
		PSB_DEBUG_ENTRY("The input format is not right!\n");
		PSB_DEBUG_ENTRY("for exampe: r 70184(read register 70184.)\n");
		PSB_DEBUG_ENTRY("for exampe: w 70184 123(write register 70184 with value 123.)\n");
		PSB_DEBUG_ENTRY("for exmape: a 60000 60010(read all registers start at 60000 and end at 60010.\n)");
		return -EINVAL;
	}
	if ((reg < 0xa000 || reg > 0x720ff) &&
				(reg < 0x40 || reg > 0x64)) {
		PSB_DEBUG_ENTRY("the register is out of display controller registers rang.\n");
		return -EINVAL;
	}

	if (val < 0) {
		PSB_DEBUG_ENTRY("the register value is should be greater than zero.\n");
		return -EINVAL;
	}

	if ((reg % 0x4) != 0) {
		PSB_DEBUG_ENTRY("the register address should aligned to 4 byte. please refrence display controller specification.\n");
		return -EINVAL;
	}

	if (!power_island_get(OSPM_DISPLAY_ISLAND)) {
		PSB_DEBUG_ENTRY("Display controller can not power on.!\n");
		return -EPERM;
	}
	if (IS_FLDS(dev)) {
#ifndef CONFIG_MID_DSI_DPU
		if (!(dev_priv->dsr_fb_update & MDFLD_DSR_MIPI_CONTROL) &&
		    (dev_priv->dbi_panel_on || dev_priv->dbi_panel_on2))
			mdfld_dsi_dbi_exit_dsr(dev,
			MDFLD_DSR_MIPI_CONTROL, 0, 0);
#endif
	}

#ifdef CONFIG_SUPPORT_MIPI
	/*forbid dsr which will restore regs*/
	mdfld_dsi_dsr_forbid(dsi_config);
#endif

	if (op == 'r') {
		if (reg >= 0xa000) {
			reg_val = REG_READ(reg);
			PSB_DEBUG_ENTRY("Read :reg=0x%08x , val=0x%08x.\n", reg,
					reg_val);
		}
		dev_priv->count = sprintf(dev_priv->buf, "%08x %08x\n", reg,
					  reg_val);
	}
	if (op == 'w') {
		if (reg >= 0xa000) {
			reg_val = REG_READ(reg);
			PSB_DEBUG_ENTRY
			    ("Before change:reg=0x%08x , val=0x%08x.\n", reg,
			     reg_val);
			REG_WRITE(reg, val);
			reg_val = REG_READ(reg);
			PSB_DEBUG_ENTRY
			    ("After change:reg=0x%08x , val=0x%08x.\n", reg,
			     reg_val);
		}
	}

	if (op == 'a') {
		start = reg;
		end = val;
		PSB_DEBUG_ENTRY("start:0x%08x\n", start);
		PSB_DEBUG_ENTRY("end:  0x%08x\n", end);
		if ((start % 0x4) != 0) {
			PSB_DEBUG_ENTRY
			("The start address should be 4 byte aligned. Please reference the display controller specification.\n");
			ret = -EINVAL;
			goto fun_exit;
		}

		if ((end % 0x4) != 0) {
			PSB_DEBUG_ENTRY
			("The end address should be 4 byte aligned. Please reference the display controller specification.\n");
			ret = -EINVAL;
			goto fun_exit;
		}

		len = end - start + 1;
		if (len <= 0)
			PSB_DEBUG_ENTRY("The end address should be greater than the start address.\n");

		if (end < 0xa000 || end > 0x720ff) {
			PSB_DEBUG_ENTRY
			("The end address is out of the display controller register range.\n");
			ret = -EINVAL;
			goto fun_exit;
		}

		if (start < 0xa000 || start > 0x720ff) {
			PSB_DEBUG_ENTRY
			    ("The start address is out of the display controller register range.\n");
			ret = -EINVAL;
			goto fun_exit;
		}
		for (Offset = start; Offset < end;
					Offset = Offset + 0x10) {
			if (reg >= 0xa000) {
				PSB_DEBUG_ENTRY
				    ("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				     Offset, REG_READ(Offset + 0x0),
				     REG_READ(Offset + 0x4),
				     REG_READ(Offset + 0x8),
				     REG_READ(Offset + 0xc));

				dev_priv->count +=
				    sprintf(dev_priv->buf + dev_priv->count,
						"%08x %08x %08x %08x %08x\n",
					    Offset, REG_READ(Offset + 0x0),
					    REG_READ(Offset + 0x4),
					    REG_READ(Offset + 0x8),
					    REG_READ(Offset + 0xc));
			}
		}
	}
fun_exit:
#ifdef CONFIG_SUPPORT_MIPI
	/*allow entering dsr*/
	mdfld_dsi_dsr_allow(dsi_config);
#endif

	power_island_put(OSPM_DISPLAY_ISLAND);
	return count;
}
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */

#ifdef CONFIG_SUPPORT_HDMI
ssize_t gpio_control_read(struct file *file, char *buffer,
				      size_t count, loff_t *offset)
{
	unsigned int value = 0;
	unsigned int pin_num = otm_hdmi_get_hpd_pin();
	if (pin_num)
		value = gpio_get_value(pin_num);

	printk(KERN_ALERT "read pin_num: %8d value:%8d\n", pin_num, value);
	return 0;
}

ssize_t gpio_control_write(struct file *file, const char *buffer,
				      size_t count, loff_t *offset)
{
	char buf[2];
	int  gpio_control;
	bool auto_state = drm_hdmi_hpd_auto;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	struct drm_minor *minor =
		(struct drm_minor *) PDE_DATA(file_inode(file));
#else
	struct drm_minor *minor =
		(struct drm_minor *) PDE(file->f_path.dentry->d_inode)->data;
#endif
	struct drm_device *dev = minor->dev;

	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count-1] != '\n')
			return -EINVAL;
		gpio_control = buf[0] - '0';

		switch (gpio_control) {
		case 0x0:
			otm_hdmi_override_cable_status(false, auto_state);
			android_hdmi_irq_test(dev);
			break;
		case 0x1:
			otm_hdmi_override_cable_status(true, auto_state);
			android_hdmi_irq_test(dev);
			break;
		default:
			printk(KERN_ALERT "invalid parameters\n");
		}
	}
	return count;
}

static ssize_t csc_control_read(struct file *file, char __user *buf,
				    size_t nbytes,loff_t *ppos)
{
	return 0;
}

static ssize_t csc_control_write(struct file *file, const char *buffer,
				      size_t count, loff_t *ppos)
{
	char buf[2];
	int  csc_control;
	struct drm_minor *minor = (struct drm_minor *)PDE_DATA(file_inode(file));
	struct drm_device *dev = minor->dev;
	struct csc_setting csc;
	struct gamma_setting gamma;

	if (count != sizeof(buf)) {
		return -EINVAL;
	} else {
		if (copy_from_user(buf, buffer, count))
			return -EINVAL;
		if (buf[count-1] != '\n')
			return -EINVAL;
		csc_control = buf[0] - '0';
		PSB_DEBUG_ENTRY(" csc control: %d\n", csc_control);

		switch (csc_control) {
		case 0x0:
			csc.pipe = 0;
			csc.type = CSC_REG_SETTING;
			csc.enable_state = true;
			csc.data_len = CSC_REG_COUNT;
			memcpy(csc.data.csc_reg_data, csc_setting, sizeof(csc.data.csc_reg_data));
                        drm_psb_enable_color_conversion = 1;
			mdfld_intel_crtc_set_color_conversion(dev, &csc);
			break;
		case 0x1:
			gamma.pipe = 0;
			gamma.type = GAMMA_REG_SETTING;
			gamma.enable_state = true;
			gamma.data_len = GAMMA_10_BIT_TABLE_COUNT;
			memcpy(gamma.gamma_tableX100, gamma_setting, sizeof(gamma.gamma_tableX100));
                        drm_psb_enable_gamma = 1;
			mdfld_intel_crtc_set_gamma(dev, &gamma);
			break;
		default:
			printk("invalied parameters\n");
		}
	}
	return count;
}

static const struct file_operations psb_csc_proc_fops = {
       .owner = THIS_MODULE,
       .read = csc_control_read,
       .write = csc_control_write,
};

static int psb_hdmi_proc_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int psb_hdmi_proc_close(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations psb_hdmi_proc_fops = {
	.owner	= THIS_MODULE,
	.open	= psb_hdmi_proc_open,
	.read	= gpio_control_read,
	.write	= gpio_control_write,
	.release= psb_hdmi_proc_close,
};

static int psb_hdmi_proc_init(struct drm_minor *minor)
{
	struct proc_dir_entry *gpio_setting;

	gpio_setting = proc_create_data(GPIO_PROC_ENTRY,
				0644, minor->proc_root,
				&psb_hdmi_proc_fops, minor);

	if (!gpio_setting)
		return -1;

	return 0;
}

#endif /* CONFIG_SUPPORT_HDMI */

/* When a client dies:
 *    - Check for and clean up flipped page state
 */
void psb_driver_preclose(struct drm_device *dev, struct drm_file *priv)
{
}

static void psb_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	drm_put_dev(dev);
}

static void psb_shutdown(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *enc_funcs;

	if (dev_priv->early_suspended)
		return;

	mutex_lock(&dev->mode_config.mutex);

	/*
	 * We borrow the early_suspended to avoid entering flip path after
	 * shutdown is called
	 */
	dev_priv->early_suspended = true;

	/* wait for the previous flip to be finished */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		enc_funcs = encoder->helper_private;
		if (!drm_helper_encoder_in_use(encoder))
			continue;
		if (enc_funcs && enc_funcs->save)
			enc_funcs->save(encoder);
	}
	mutex_unlock(&dev->mode_config.mutex);
}

static int psb_proc_init(struct drm_minor *minor)
{
        struct proc_dir_entry *csc_setting;
#ifdef CONFIG_SUPPORT_HDMI
	psb_hdmi_proc_init(minor);
#endif
        csc_setting = proc_create_data(CSC_PROC_ENTRY, 0644, minor->proc_root, &psb_csc_proc_fops, minor);

	return 0;
}

static void psb_proc_cleanup(struct drm_minor *minor)
{
#ifdef CONFIG_SUPPORT_HDMI
	remove_proc_entry(GPIO_PROC_ENTRY, minor->proc_root);
#endif
	return;
}

#if defined(CONFIG_DEBUG_FS)
/*debugfs init entry*/
static int psb_debugfs_init(struct drm_minor *minor)
{
	/*call original psb_proc_init*/
	/*NOTE: why we are doing proc init here????*/
	if (psb_proc_init(minor))
		DRM_ERROR("psb_proc_init failed\n");

	/*call debugfs init*/
	return mdfld_debugfs_init(minor);
}

/*debugfs cleanup entry*/
static void psb_debugfs_cleanup(struct drm_minor *minor)
{
	/*call original psb_proc_cleanup*/
	/*NOTE: why we are doing proc cleanup here????*/
	psb_proc_cleanup(minor);

	/*call debugfs cleanup*/
	mdfld_debugfs_cleanup(minor);
}
#endif
static const struct dev_pm_ops psb_pm_ops = {
	.runtime_suspend = rtpm_suspend,
	.runtime_resume = rtpm_resume,
	.runtime_idle = rtpm_idle,
	.suspend_noirq = rtpm_suspend,
	.resume_noirq = rtpm_resume,
};

static struct vm_operations_struct psb_ttm_vm_ops;

/**
 * NOTE: driver_private of drm_file is now a PVRSRV_FILE_PRIVATE_DATA struct
 * pPriv in PVRSRV_FILE_PRIVATE_DATA contains the original psb_fpriv;
 */
int psb_open(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv;
	struct drm_psb_private *dev_priv;
	struct psb_fpriv *psb_fp;

	int ret;

	DRM_DEBUG("\n");

	ret = drm_open(inode, filp);
	if (unlikely(ret))
		return ret;

	psb_fp = kzalloc(sizeof(*psb_fp), GFP_KERNEL);

	if (unlikely(psb_fp == NULL))
		goto out_err0;

	file_priv = (struct drm_file *) filp->private_data;

	/* In case that the local file priv has created a master,
	 * which has been referenced, even if it's not authenticated
	 * (non-root user). */
	if ((file_priv->minor->master)
		&& (file_priv->master == file_priv->minor->master)
		&& (!file_priv->is_master))
		file_priv->is_master = 1;

	dev_priv = psb_priv(file_priv->minor->dev);

	DRM_DEBUG("is_master %d\n", file_priv->is_master ? 1 : 0);

	psb_fp->tfile = ttm_object_file_init(dev_priv->tdev,
					     PSB_FILE_OBJECT_HASH_ORDER);
	psb_fp->bcd_index = -1;
	if (unlikely(psb_fp->tfile == NULL))
		goto out_err1;

	if (!file_priv->driver_priv) {
		DRM_ERROR("drm file private is NULL\n");
		goto out_err1;
	}

	BCVideoSetPriv(file_priv, psb_fp);

	if (unlikely(dev_priv->bdev.dev_mapping == NULL))
		dev_priv->bdev.dev_mapping = dev_priv->dev->dev_mapping;

	return 0;

out_err1:
	kfree(psb_fp);
out_err0:
	(void) drm_release(inode, filp);
	return ret;
}

int psb_release(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv;
	struct psb_fpriv *psb_fp;
	struct drm_psb_private *dev_priv;
	struct msvdx_private *msvdx_priv;
	struct ttm_object_file *tfile;
	struct psb_msvdx_ec_ctx *ec_ctx;
	int ret, i;

	file_priv = (struct drm_file *)filp->private_data;
	psb_fp = BCVideoGetPriv(file_priv);
	dev_priv = psb_priv(file_priv->minor->dev);
	tfile = psb_fpriv(file_priv)->tfile;
	msvdx_priv = (struct msvdx_private *)dev_priv->msvdx_private;

	/* Set flag to clean-up platform IED state as
		user space component might have died*/
	if ((dev_priv->ied_context == file_priv->filp) &&
					dev_priv->ied_enabled) {
		dev_priv->ied_force_clean = true;
		dev_priv->ied_context = NULL;
	}
#if 0
	/*cleanup for msvdx */
	if (msvdx_priv->tfile == BCVideoGetPriv(file_priv)->tfile) {
		msvdx_priv->fw_status = 0;
		msvdx_priv->host_be_opp_enabled = 0;
		memset(&msvdx_priv->frame_info, 0,
		       sizeof(struct drm_psb_msvdx_frame_info) *
		       MAX_DECODE_BUFFERS);
	}
#endif

	tng_topaz_handle_sigint(file_priv->minor->dev, filp);

	BCVideoDestroyBuffers(psb_fp->bcd_index);

	if (msvdx_priv->msvdx_ec_ctx[0] != NULL) {
		for (i = 0; i < PSB_MAX_EC_INSTANCE; i++) {
			if (msvdx_priv->msvdx_ec_ctx[i]->tfile == tfile)
				break;
		}

		if (i < PSB_MAX_EC_INSTANCE) {
			ec_ctx = msvdx_priv->msvdx_ec_ctx[i];
			PSB_DEBUG_ENTRY("remove ec ctx with tfile %p\n",
					ec_ctx->tfile);
			ec_ctx->tfile = NULL;
			ec_ctx->fence = PSB_MSVDX_INVALID_FENCE;
		}
	}

	ttm_object_file_release(&psb_fp->tfile);
	kfree(psb_fp);

	/* remove video context */
	/* psb_remove_videoctx(dev_priv, filp); */

	ret = drm_release(inode, filp);

	return ret;
}

/**
 * if vm_pgoff < DRM_PSB_FILE_PAGE_OFFSET call directly to PVRMMap
 */
int psb_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_file *file_priv;
	struct drm_psb_private *dev_priv;
	int ret;

	if (vma->vm_pgoff < DRM_PSB_FILE_PAGE_OFFSET ||
	    vma->vm_pgoff > 2 * DRM_PSB_FILE_PAGE_OFFSET)
		return PVRSRVMMap(filp, vma);

	file_priv = (struct drm_file *) filp->private_data;
	dev_priv = psb_priv(file_priv->minor->dev);

	ret = ttm_bo_mmap(filp, vma, &dev_priv->bdev);
	if (unlikely(ret != 0))
		return ret;

	if (unlikely(dev_priv->ttm_vm_ops == NULL)) {
		dev_priv->ttm_vm_ops = (struct vm_operations_struct *)vma->vm_ops;
		psb_ttm_vm_ops = *vma->vm_ops;
		psb_ttm_vm_ops.fault = &psb_ttm_fault;
	}

	vma->vm_ops = &psb_ttm_vm_ops;

	return 0;
}

static const struct file_operations driver_psb_fops = {
	.owner = THIS_MODULE,
	.open = psb_open,
	.release = psb_release,
	.unlocked_ioctl = psb_unlocked_ioctl,
	.mmap = psb_mmap,
	.poll = psb_poll,
	.fasync = drm_fasync,
	.read = drm_read,
#ifdef CONFIG_COMPAT
        .compat_ioctl = psb_compat_ioctl,
#endif
};

static struct drm_driver driver = {
	.driver_features = DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED |
	    DRIVER_IRQ_VBL | DRIVER_MODESET,
	.load = psb_driver_load,
	.unload = psb_driver_unload,

	.ioctls = psb_ioctls,
	.num_ioctls = DRM_ARRAY_SIZE(psb_ioctls),
	.device_is_agp = psb_driver_device_is_agp,
	.irq_preinstall = psb_irq_preinstall,
	.irq_postinstall = psb_irq_postinstall,
	.irq_uninstall = psb_irq_uninstall,
	.irq_handler = psb_irq_handler,
	.enable_vblank = psb_enable_vblank,
	.disable_vblank = psb_disable_vblank,
	.get_vblank_counter = psb_get_vblank_counter,
	.get_vblank_timestamp = intel_get_vblank_timestamp,
	.get_scanout_position = intel_get_crtc_scanoutpos,
	.firstopen = NULL,
	.lastclose = psb_lastclose,
	.open = psb_driver_open,
	.postclose = PVRSRVDrmPostClose,
#if defined(CONFIG_DEBUG_FS)
	.debugfs_init = psb_debugfs_init,
	.debugfs_cleanup = psb_debugfs_cleanup,
#endif

#if KEEP_UNUSED_CODE_DRIVER_DISPATCH
/*
*	.get_map_ofs = drm_core_get_map_ofs,
*	.get_reg_ofs = drm_core_get_reg_ofs,
*/
#endif /* if KEEP_UNUSED_CODE_DRIVER_DISPATCH */
	.preclose = psb_driver_preclose,
	.fops = &driver_psb_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = PSB_DRM_DRIVER_DATE,
	.major = PSB_DRM_DRIVER_MAJOR,
	.minor = PSB_DRM_DRIVER_MINOR,
	.patchlevel = PSB_DRM_DRIVER_PATCHLEVEL
};

static struct pci_driver psb_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = psb_probe,
	.remove = psb_remove,
#ifdef CONFIG_PM
	.driver.pm = &psb_pm_ops,
#endif
	.shutdown = psb_shutdown
};

static int psb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	pdev->d3_delay = 0;
	return drm_get_pci_dev(pdev, ent, &driver);
}

#ifndef MODULE
static __init int parse_panelid(char *arg)
{
	/* panel ID can be passed in as a cmdline parameter */
	/* to enable this feature add panelid=TMD to cmdline for TMD panel */
	if (!arg)
		return -EINVAL;

	if (!strcasecmp(arg, "TMD_CMD"))
		PanelID = TMD_CMD;
	else if (!strcasecmp(arg, "TPO_CMD"))
		PanelID = TPO_CMD;
	else if (!strcasecmp(arg, "PYR_CMD"))
		PanelID = PYR_CMD;
	else if (!strcasecmp(arg, "TMD_VID"))
		PanelID = TMD_VID;
	else if (!strcasecmp(arg, "TPO_VID"))
		PanelID = TPO_VID;
	else if (!strcasecmp(arg, "PYR_VID"))
		PanelID = PYR_VID;
	else
		PanelID = GCT_DETECT;

	return 0;
}

early_param("panelid", parse_panelid);
#endif

#ifndef MODULE
static __init int parse_hdmi_edid(char *arg)
{
	/* HDMI EDID info can be passed in as a cmdline parameter,
	 * and need to remove it after we can get EDID info via MSIC.*/
	if ((!arg) || (strlen(arg) >= 20))
		return -EINVAL;

	strncpy(HDMI_EDID, arg, sizeof(HDMI_EDID));

	return 0;
}

early_param("hdmi_edid", parse_hdmi_edid);
#endif

static int __init psb_init(void)
{
	int ret;

#if defined(MODULE) && defined(CONFIG_NET)
	psb_kobject_uevent_init();
#endif

	PVRSRVQueryIoctls(psb_ioctls);

	BCVideoQueryIoctls(psb_ioctls);

	ret = drm_pci_init(&driver, &psb_pci_driver);
	if (ret != 0) {
		DRM_ERROR("drm_init fail!\n");
		return ret;
	}
#ifdef WANT_GFX
	/*init for bc_video */
	ret = BCVideoModInit();
	if (ret != 0)
		return ret;
#endif

#ifdef CONFIG_SUPPORT_HDMI
	if (otm_hdmi_hpd_init() == OTM_HDMI_SUCCESS)
		DRM_INFO("OTM_HDMI_INIT_SUCCESS\n");
	else
		DRM_INFO("OTM_HDMI_INIT_FAILE\n");
#endif

	return ret;
}

static void __exit psb_exit(void)
{
	int ret;
	/*cleanup for bc_video */
	ret = BCVideoModCleanup();
	if (ret != 0)
		return;
	drm_pci_exit(&driver, &psb_pci_driver);
}

#ifdef CONFIG_COMPAT

#define PVR_DRM_SRVKM_CMD       DRM_PVR_RESERVED1
#define PVR_DRM_IS_MASTER_CMD   DRM_PVR_RESERVED4
#define PVR_DRM_DBGDRV_CMD      DRM_PVR_RESERVED6

#define PVR_DRM_SRVKM_IOCTL \
	DRM_IOW(DRM_COMMAND_BASE + PVR_DRM_SRVKM_CMD, PVRSRV_BRIDGE_PACKAGE)

#define PVR_DRM_IS_MASTER_IOCTL \
	DRM_IO(DRM_COMMAND_BASE + PVR_DRM_IS_MASTER_CMD)

typedef struct drm_psb_mem_alloc32 {
	int region;
	int alignment;
	int size;
	u32 region_offset;	/* offset from start of fb or agp */
} drm_psb_mem_alloc32_t;

typedef struct drm_psb_mem_alloc {
	int region;
	int alignment;
	int size;
	int __user *region_offset;	/* offset from start of fb or agp */
} drm_psb_mem_alloc_t;

typedef struct pvrsrv_bridge_package_32
{
	u32	ui32BridgeID;		/*!< ioctl/drvesc index */
	u32	ui32Size;			/*!< size of structure */
	u32	pvParamIn;			/*!< input data buffer */
	u32	ui32InBufferSize;		/*!< size of input data buf */
	u32	pvParamOut;			/*!< output data buffer */
	u32	ui32OutBufferSize;		/*!< size of output data buf */
} pvrsrv_bridge_package_32_t;

int compat_PVRSRV_BridgeDispatchKM2(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	int retval;
	pvrsrv_bridge_package_32_t req32;
	PVRSRV_BRIDGE_PACKAGE __user *request;

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	request = compat_alloc_user_space(sizeof(*request));
	if (!access_ok(VERIFY_WRITE, request, sizeof(*request))
		|| __put_user(req32.ui32BridgeID, &request->ui32BridgeID)
		|| __put_user(req32.ui32Size, &request->ui32Size)
		|| __put_user((void __user *)(unsigned long)req32.pvParamIn, &request->pvParamIn)
		|| __put_user(req32.ui32InBufferSize, &request->ui32InBufferSize)
		|| __put_user((void __user *)(unsigned long)req32.pvParamOut, &request->pvParamOut)
		|| __put_user(req32.ui32OutBufferSize, &request->ui32OutBufferSize)) {
		printk(KERN_ERR "%s: __put_user failed\n", __func__);
		return -EFAULT;
	}

	/* Correct cmd with the proper size */
	cmd &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
	cmd |= (sizeof(*request) << _IOC_SIZESHIFT);

	retval = drm_ioctl(filp, cmd, (unsigned long)request);
	return retval;
}

struct drm_psb_register_rw_arg_32 {
	uint32_t b_force_hw_on;
	uint32_t display_read_mask;
	uint32_t display_write_mask;
	struct {
		uint32_t pfit_controls;
		uint32_t pfit_autoscale_ratios;
		uint32_t pfit_programmed_scale_ratios;
		uint32_t pipeasrc;
		uint32_t pipebsrc;
		uint32_t vtotal_a;
		uint32_t vtotal_b;
		uint32_t dspcntr_a;
		uint32_t dspcntr_b;
		uint32_t pipestat_a;
		uint32_t int_mask;
		uint32_t int_enable;
	} display;
	uint32_t overlay_read_mask;
	uint32_t overlay_write_mask;
	struct {
		uint32_t OVADD;
		uint32_t OGAMC0;
		uint32_t OGAMC1;
		uint32_t OGAMC2;
		uint32_t OGAMC3;
		uint32_t OGAMC4;
		uint32_t OGAMC5;
		uint32_t IEP_ENABLED;
		uint32_t IEP_BLE_MINMAX;
		uint32_t IEP_BSSCC_CONTROL;
		uint32_t index;
		uint32_t b_wait_vblank;
		uint32_t b_wms;
		uint32_t buffer_handle;
	} overlay;
	uint32_t vsync_operation_mask;
	struct {
		uint32_t pipe;
		int vsync_pipe;
		int vsync_count;
		uint64_t timestamp  __attribute__ ((__packed__));
	} vsync;
	uint32_t sprite_enable_mask;
	uint32_t sprite_disable_mask;
	struct {
		uint32_t dspa_control;
		uint32_t dspa_key_value;
		uint32_t dspa_key_mask;
		uint32_t dspc_control;
		uint32_t dspc_stride;
		uint32_t dspc_position;
		uint32_t dspc_linear_offset;
		uint32_t dspc_size;
		uint32_t dspc_surface;
	} sprite;
	uint32_t subpicture_enable_mask;
	uint32_t subpicture_disable_mask;
	struct {
		uint32_t CursorADDR;
		uint32_t xPos;
		uint32_t yPos;
		uint32_t CursorSize;
	} cursor;
	uint32_t cursor_enable_mask;
	uint32_t cursor_disable_mask;
	uint32_t plane_enable_mask;
	uint32_t plane_disable_mask;
	uint32_t get_plane_state_mask;
	struct {
		uint32_t type;
		uint32_t index;
		uint32_t ctx;
	} plane;
}__attribute__ ((__packed__));

int compat_PVRSRV_BridgeDispatchKM3(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	int retval;
	struct drm_psb_register_rw_arg_32 req32;
	struct drm_psb_register_rw_arg __user *request;
	struct drm_psb_register_rw_arg returnBuffer;
	struct drm_psb_register_rw_arg_32 __user *p_buf =
		(struct drm_psb_register_rw_arg_32 __user *)((void __user *)arg);

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	request = compat_alloc_user_space(sizeof(*request));
	if (!access_ok(VERIFY_WRITE, request, sizeof(*request))
		|| __put_user(req32.b_force_hw_on, &request->b_force_hw_on)
		|| __put_user(req32.display_read_mask, &request->display_read_mask)
		|| __put_user(req32.display_write_mask, &request->display_write_mask)
		|| __put_user(req32.display.pfit_controls, &request->display.pfit_controls)
		|| __put_user(req32.display.pfit_autoscale_ratios, &request->display.pfit_autoscale_ratios)
		|| __put_user(req32.display.pfit_programmed_scale_ratios, &request->display.pfit_programmed_scale_ratios)
		|| __put_user(req32.display.pipeasrc, &request->display.pipeasrc)
		|| __put_user(req32.display.pipebsrc, &request->display.pipebsrc)
		|| __put_user(req32.display.vtotal_a, &request->display.vtotal_a)
		|| __put_user(req32.display.vtotal_b, &request->display.vtotal_b)
		|| __put_user(req32.display.dspcntr_a, &request->display.dspcntr_a)
		|| __put_user(req32.display.dspcntr_b, &request->display.dspcntr_b)
		|| __put_user(req32.display.pipestat_a, &request->display.pipestat_a)
		|| __put_user(req32.display.int_mask, &request->display.int_mask)
		|| __put_user(req32.display.int_enable, &request->display.int_enable)
		|| __put_user(req32.overlay_read_mask, &request->overlay_read_mask)
		|| __put_user(req32.overlay_write_mask, &request->overlay_write_mask)
		|| __put_user(req32.overlay.OVADD, &request->overlay.OVADD)
		|| __put_user(req32.overlay.OGAMC0, &request->overlay.OGAMC0)
		|| __put_user(req32.overlay.OGAMC1, &request->overlay.OGAMC1)
		|| __put_user(req32.overlay.OGAMC2, &request->overlay.OGAMC2)
		|| __put_user(req32.overlay.OGAMC3, &request->overlay.OGAMC3)
		|| __put_user(req32.overlay.OGAMC4, &request->overlay.OGAMC4)
		|| __put_user(req32.overlay.OGAMC5, &request->overlay.OGAMC5)
		|| __put_user(req32.overlay.IEP_ENABLED, &request->overlay.IEP_ENABLED)
		|| __put_user(req32.overlay.IEP_BLE_MINMAX, &request->overlay.IEP_BLE_MINMAX)
		|| __put_user(req32.overlay.IEP_BSSCC_CONTROL, &request->overlay.IEP_BSSCC_CONTROL)
		|| __put_user(req32.overlay.index, &request->overlay.index)
		|| __put_user(req32.overlay.b_wait_vblank, &request->overlay.b_wait_vblank)
		|| __put_user(req32.overlay.b_wms, &request->overlay.b_wms)
		|| __put_user(req32.overlay.buffer_handle, &request->overlay.buffer_handle)
		|| __put_user(req32.vsync_operation_mask, &request->vsync_operation_mask)
		|| __put_user(req32.vsync.pipe, &request->vsync.pipe)
		|| __put_user(req32.vsync.vsync_pipe, &request->vsync.vsync_pipe)
		|| __put_user(req32.vsync.vsync_count, &request->vsync.vsync_count)
		|| __put_user(req32.vsync.timestamp, &request->vsync.timestamp)
		|| __put_user(req32.sprite_enable_mask, &request->sprite_enable_mask)
		|| __put_user(req32.sprite_disable_mask, &request->sprite_disable_mask)
		|| __put_user(req32.sprite.dspa_control, &request->sprite.dspa_control)
		|| __put_user(req32.sprite.dspa_key_value, &request->sprite.dspa_key_value)
		|| __put_user(req32.sprite.dspa_key_mask, &request->sprite.dspa_key_mask)
		|| __put_user(req32.sprite.dspc_control, &request->sprite.dspc_control)
		|| __put_user(req32.sprite.dspc_stride, &request->sprite.dspc_stride)
		|| __put_user(req32.sprite.dspc_position, &request->sprite.dspc_position)
		|| __put_user(req32.sprite.dspc_linear_offset, &request->sprite.dspc_linear_offset)
		|| __put_user(req32.sprite.dspc_size, &request->sprite.dspc_size)
		|| __put_user(req32.sprite.dspc_surface, &request->sprite.dspc_surface)
		|| __put_user(req32.subpicture_enable_mask, &request->subpicture_enable_mask)
		|| __put_user(req32.subpicture_disable_mask, &request->subpicture_disable_mask)
		|| __put_user(req32.cursor.CursorADDR, &request->cursor.CursorADDR)
		|| __put_user(req32.cursor.xPos, &request->cursor.xPos)
		|| __put_user(req32.cursor.yPos, &request->cursor.yPos)
		|| __put_user(req32.cursor.CursorSize, &request->cursor.CursorSize)
		|| __put_user(req32.cursor_enable_mask, &request->cursor_enable_mask)
		|| __put_user(req32.cursor_disable_mask, &request->cursor_disable_mask)
		|| __put_user(req32.plane_enable_mask, &request->plane_enable_mask)
		|| __put_user(req32.plane_disable_mask, &request->plane_disable_mask)
		|| __put_user(req32.get_plane_state_mask, &request->get_plane_state_mask)
		|| __put_user(req32.plane.type, &request->plane.type)
		|| __put_user(req32.plane.index, &request->plane.index)
		|| __put_user(req32.plane.ctx, &request->plane.ctx)) {
		printk(KERN_ERR "%s: __put_user failed\n", __func__);
		return -EFAULT;
	}

	/* Correct cmd with the proper size */
	cmd &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
	cmd |= (sizeof(*request) << _IOC_SIZESHIFT);

	retval = drm_ioctl(filp, cmd, (unsigned long)request);
	if (copy_from_user(&returnBuffer, (void __user *)request, sizeof(returnBuffer))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (!access_ok(VERIFY_WRITE, request, sizeof(*p_buf))
		|| __put_user(returnBuffer.b_force_hw_on, &p_buf->b_force_hw_on)
		|| __put_user(returnBuffer.display_read_mask, &p_buf->display_read_mask)
		|| __put_user(returnBuffer.display_write_mask, &p_buf->display_write_mask)
		|| __put_user(returnBuffer.display.pfit_controls, &p_buf->display.pfit_controls)
		|| __put_user(returnBuffer.display.pfit_autoscale_ratios, &p_buf->display.pfit_autoscale_ratios)
		|| __put_user(returnBuffer.display.pfit_programmed_scale_ratios, &p_buf->display.pfit_programmed_scale_ratios)
		|| __put_user(returnBuffer.display.pipeasrc, &p_buf->display.pipeasrc)
		|| __put_user(returnBuffer.display.pipebsrc, &p_buf->display.pipebsrc)
		|| __put_user(returnBuffer.display.vtotal_a, &p_buf->display.vtotal_a)
		|| __put_user(returnBuffer.display.vtotal_b, &p_buf->display.vtotal_b)
		|| __put_user(returnBuffer.display.dspcntr_a, &p_buf->display.dspcntr_a)
		|| __put_user(returnBuffer.display.dspcntr_b, &p_buf->display.dspcntr_b)
		|| __put_user(returnBuffer.display.pipestat_a, &p_buf->display.pipestat_a)
		|| __put_user(returnBuffer.display.int_mask, &p_buf->display.int_mask)
		|| __put_user(returnBuffer.display.int_enable, &p_buf->display.int_enable)
		|| __put_user(returnBuffer.overlay_read_mask, &p_buf-> overlay_read_mask)
		|| __put_user(returnBuffer.overlay_write_mask, &p_buf->overlay_write_mask)
		|| __put_user(returnBuffer.overlay.OVADD, &p_buf->overlay.OVADD)
		|| __put_user(returnBuffer.overlay.OGAMC0, &p_buf->overlay.OGAMC0)
		|| __put_user(returnBuffer.overlay.OGAMC1, &p_buf->overlay.OGAMC1)
		|| __put_user(returnBuffer.overlay.OGAMC2, &p_buf->overlay.OGAMC2)
		|| __put_user(returnBuffer.overlay.OGAMC3, &p_buf->overlay.OGAMC3)
		|| __put_user(returnBuffer.overlay.OGAMC4, &p_buf->overlay.OGAMC4)
		|| __put_user(returnBuffer.overlay.OGAMC5, &p_buf->overlay.OGAMC5)
		|| __put_user(returnBuffer.overlay.IEP_ENABLED, &p_buf->overlay.IEP_ENABLED)
		|| __put_user(returnBuffer.overlay.IEP_BLE_MINMAX, &p_buf->overlay.IEP_BLE_MINMAX)
		|| __put_user(returnBuffer.overlay.IEP_BSSCC_CONTROL, &p_buf->overlay.IEP_BSSCC_CONTROL)
		|| __put_user(returnBuffer.overlay.index, &p_buf->overlay.index)
		|| __put_user(returnBuffer.overlay.b_wait_vblank, &p_buf->overlay.b_wait_vblank)
		|| __put_user(returnBuffer.overlay.b_wms, &p_buf-> overlay.b_wms)
		|| __put_user(returnBuffer.overlay.buffer_handle, &p_buf->overlay.buffer_handle)
		|| __put_user(returnBuffer.vsync_operation_mask, &p_buf->vsync_operation_mask)
		|| __put_user(returnBuffer.vsync.pipe, &p_buf->vsync.pipe)
		|| __put_user(returnBuffer.vsync.vsync_pipe, &p_buf->vsync.vsync_pipe)
		|| __put_user(returnBuffer.vsync.vsync_count, &p_buf->vsync.vsync_count)
		|| __put_user(returnBuffer.vsync.timestamp, &p_buf->vsync.timestamp)
		|| __put_user(returnBuffer.sprite_enable_mask, &p_buf->sprite_enable_mask)
		|| __put_user(returnBuffer.sprite_disable_mask, &p_buf->sprite_disable_mask)
		|| __put_user(returnBuffer.sprite.dspa_control, &p_buf->sprite.dspa_control)
		|| __put_user(returnBuffer.sprite.dspa_key_value, &p_buf->sprite.dspa_key_value)
		|| __put_user(returnBuffer.sprite.dspa_key_mask, &p_buf->sprite.dspa_key_mask)
		|| __put_user(returnBuffer.sprite.dspc_control, &p_buf->sprite.dspc_control)
		|| __put_user(returnBuffer.sprite.dspc_stride, &p_buf->sprite.dspc_stride)
		|| __put_user(returnBuffer.sprite.dspc_position, &p_buf->sprite.dspc_position)
		|| __put_user(returnBuffer.sprite.dspc_linear_offset, &p_buf->sprite.dspc_linear_offset)
		|| __put_user(returnBuffer.sprite.dspc_size, &p_buf->sprite.dspc_size)
		|| __put_user(returnBuffer.sprite.dspc_surface, &p_buf->sprite.dspc_surface)
		|| __put_user(returnBuffer.subpicture_enable_mask, &p_buf->subpicture_enable_mask)
		|| __put_user(returnBuffer.subpicture_disable_mask, &p_buf->subpicture_disable_mask)
		|| __put_user(returnBuffer.cursor.CursorADDR, &p_buf->cursor.CursorADDR)
		|| __put_user(returnBuffer.cursor.xPos, &p_buf->cursor.xPos)
		|| __put_user(returnBuffer.cursor.yPos, &p_buf->cursor.yPos)
		|| __put_user(returnBuffer.cursor.CursorSize, &p_buf->cursor.CursorSize)
		|| __put_user(returnBuffer.cursor_enable_mask, &p_buf->cursor_enable_mask)
		|| __put_user(returnBuffer.cursor_disable_mask, &p_buf->cursor_disable_mask)
		|| __put_user(returnBuffer.plane_enable_mask, &p_buf->plane_enable_mask)
		|| __put_user(returnBuffer.plane_disable_mask, &p_buf->plane_disable_mask)
		|| __put_user(returnBuffer.plane.type, &p_buf->plane.type)
		|| __put_user(returnBuffer.plane.index, &p_buf->plane.index)
		|| __put_user(returnBuffer.plane.ctx, &p_buf->plane.ctx)) {
		printk(KERN_ERR "%s: __put_user failed\n", __func__);
		return -EFAULT;
	}

	return retval;
}

struct drm_psb_vsync_set_arg_32 {
	uint32_t vsync_operation_mask;
	struct {
		uint32_t pipe;
		int vsync_pipe;
		int vsync_count;
		uint64_t timestamp  __attribute__ ((__packed__));
	} vsync;
} __attribute__ ((__packed__));

int compat_PVRSRV_BridgeDispatchKM4(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval;
	struct drm_psb_vsync_set_arg_32 req32;
	struct drm_psb_vsync_set_arg __user *request;
	struct drm_psb_vsync_set_arg returnBuffer;
	struct drm_psb_vsync_set_arg_32 __user *p_buf =
		(struct drm_psb_vsync_set_arg_32 __user *)((void __user *)arg);

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	request = compat_alloc_user_space(sizeof(*request));
	if (!access_ok(VERIFY_WRITE, request, sizeof(*request))
		|| __put_user(req32.vsync_operation_mask, &request->vsync_operation_mask)
		|| __put_user(req32.vsync.pipe, &request->vsync.pipe)
		|| __put_user(req32.vsync.vsync_pipe, &request->vsync.vsync_pipe)
		|| __put_user(req32.vsync.vsync_count, &request->vsync.vsync_count)
		|| __put_user(req32.vsync.timestamp, &request->vsync.timestamp)) {
		printk(KERN_ERR "%s __put_user failed\n", __func__);
		return -EFAULT;
	}

	/* Correct cmd with the proper size */
	cmd &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
	cmd |= (sizeof(*request) << _IOC_SIZESHIFT);

	retval = drm_ioctl(filp, cmd, (unsigned long)request);
	if (copy_from_user(&returnBuffer, (void __user *)request, sizeof(returnBuffer))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (!access_ok(VERIFY_WRITE, request, sizeof(*p_buf))
		|| __put_user(returnBuffer.vsync_operation_mask, &p_buf->vsync_operation_mask)
		|| __put_user(returnBuffer.vsync.pipe, &p_buf->vsync.pipe)
		|| __put_user(returnBuffer.vsync.vsync_pipe, &p_buf->vsync.vsync_pipe)
		|| __put_user(returnBuffer.vsync.vsync_count, &p_buf->vsync.vsync_count)
		|| __put_user(returnBuffer.vsync.timestamp, &p_buf->vsync.timestamp)){
		printk(KERN_ERR "%s: __put_user failed\n", __func__);
		return -EFAULT;
	}

	return retval;
}

struct psb_gtt_mapping_arg_32 {
	uint32_t type;
	uint32_t hKernelMemInfo; /* void *hKernelMemInfo; */
	uint32_t offset_pages;
	uint32_t page_align;
	uint32_t bcd_device_id;
	uint32_t bcd_buffer_id;
	uint32_t bcd_buffer_count;
	uint32_t bcd_buffer_stride;
	uint32_t vaddr;
	uint32_t size;
};

int compat_PVRSRV_BridgeDispatchKM5(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval;
	struct psb_gtt_mapping_arg_32 req32;
	struct psb_gtt_mapping_arg __user *request;
	struct psb_gtt_mapping_arg returnBuffer;
	struct psb_gtt_mapping_arg_32 __user *p_buf = (struct psb_gtt_mapping_arg_32 __user *)((void __user *)arg);

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	request = compat_alloc_user_space(sizeof(*request));
	if (!access_ok(VERIFY_WRITE, request, sizeof(*request))
		|| __put_user(req32.type, &request->type)
		|| __put_user((void *)(unsigned long)req32.hKernelMemInfo, &request->hKernelMemInfo)
		|| __put_user(req32.offset_pages, &request->offset_pages)
		|| __put_user(req32.page_align, &request->page_align)
		|| __put_user(req32.bcd_device_id, &request->bcd_device_id)
		|| __put_user(req32.bcd_buffer_id, &request->bcd_buffer_id)
		|| __put_user(req32.bcd_buffer_count, &request->bcd_buffer_count)
		|| __put_user(req32.bcd_buffer_stride, &request->bcd_buffer_stride)
		|| __put_user(req32.vaddr, &request->vaddr)
		|| __put_user(req32.size, &request->size)) {
		printk(KERN_ERR "%s: __put_user failed\n", __func__);
		return -EFAULT;
	}

	/* Correct cmd with the proper size */
	cmd &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
	cmd |= (sizeof(*request) << _IOC_SIZESHIFT);

	retval = drm_ioctl(filp, cmd, (unsigned long)request);
	if (copy_from_user(&returnBuffer, (void __user *)request, sizeof(returnBuffer))) {
		printk(KERN_ERR "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
        }

	if (!access_ok(VERIFY_WRITE, request, sizeof(*p_buf))
		|| __put_user(returnBuffer.type, &p_buf->type)
		|| __put_user((uint32_t)(unsigned long)returnBuffer.hKernelMemInfo, &p_buf->hKernelMemInfo)
		|| __put_user(returnBuffer.offset_pages, &p_buf->offset_pages)
		|| __put_user(returnBuffer.page_align, &p_buf->page_align)
		|| __put_user(returnBuffer.bcd_device_id, &p_buf->bcd_device_id)
		|| __put_user(returnBuffer.bcd_buffer_id, &p_buf->bcd_buffer_id)
		|| __put_user(returnBuffer.bcd_buffer_count, &p_buf->bcd_buffer_count)
		|| __put_user(returnBuffer.bcd_buffer_stride, &p_buf->bcd_buffer_stride)
		|| __put_user(returnBuffer.vaddr, &p_buf->vaddr)
		|| __put_user(returnBuffer.size, &p_buf->size)) {
		printk(KERN_ERR "%s: __put_user failed\n", __func__);
		return -EFAULT;
	}

	return retval;
}

static drm_ioctl_compat_t *psb_compat_ioctls[] = {
	[PVR_DRM_SRVKM_CMD] = compat_PVRSRV_BridgeDispatchKM2,
	[DRM_PSB_REGISTER_RW] = compat_PVRSRV_BridgeDispatchKM3,
	[DRM_PSB_VSYNC_SET] = compat_PVRSRV_BridgeDispatchKM4,
	[DRM_PSB_GTT_MAP] = compat_PVRSRV_BridgeDispatchKM5,
	[DRM_PSB_GTT_UNMAP] = compat_PVRSRV_BridgeDispatchKM5,
};

/**
 * Called whenever a 32-bit process running under a 64-bit kernel
 * performs an ioctl on /dev/dri/card<n>.
 *
 * \param filp file pointer.
 * \param cmd command.
 * \param arg user argument.
 * \return zero on success or negative number on failure.
 */
static long psb_compat_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	drm_ioctl_compat_t *fn = NULL;
	long ret;

	/*
	 * The driver private ioctls and TTM ioctls should be
	 * thread-safe.
	 */

	if (nr < DRM_COMMAND_BASE) {
		ret = drm_compat_ioctl(filp, cmd, arg);
		goto out;
	}

	if (nr < DRM_COMMAND_BASE + DRM_ARRAY_SIZE(psb_compat_ioctls))
		fn = psb_compat_ioctls[nr - DRM_COMMAND_BASE];

	if (fn != NULL) {
		ret = (*fn) (filp, cmd, arg);
	} else {
		ret = drm_ioctl(filp, cmd, arg);
	}
out:
	return ret;
}
#endif /* CONFIG_COMPAT */

#ifdef CONFIG_SUPPORT_TMD_MIPI_600X1024_DISPLAY
module_init(psb_init);
#else
late_initcall(psb_init);
#endif
module_exit(psb_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
