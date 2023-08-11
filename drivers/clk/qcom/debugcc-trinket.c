// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "clk-debug.h"
#include "common.h"

static struct measure_clk_data debug_mux_priv = {
	.ctl_reg = 0x62038,
	.status_reg = 0x6203C,
	.xo_div4_cbcr = 0x28008,
};

static const char *const disp_cc_debug_mux_parent_names[] = {
	"disp_cc_mdss_ahb_clk",
	"disp_cc_mdss_byte0_clk",
	"disp_cc_mdss_byte0_intf_clk",
	"disp_cc_mdss_dp_aux_clk",
	"disp_cc_mdss_dp_crypto_clk",
	"disp_cc_mdss_dp_link_clk",
	"disp_cc_mdss_dp_link_intf_clk",
	"disp_cc_mdss_dp_pixel_clk",
	"disp_cc_mdss_esc0_clk",
	"disp_cc_mdss_mdp_clk",
	"disp_cc_mdss_mdp_lut_clk",
	"disp_cc_mdss_non_gdsc_ahb_clk",
	"disp_cc_mdss_pclk0_clk",
	"disp_cc_mdss_rot_clk",
	"disp_cc_mdss_vsync_clk",
	"disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x1A,		/* disp_cc_mdss_ahb_clk */
	0x12,		/* disp_cc_mdss_byte0_clk */
	0x13,		/* disp_cc_mdss_byte0_intf_clk */
	0x19,		/* disp_cc_mdss_dp_aux_clk */
	0x17,		/* disp_cc_mdss_dp_crypto_clk */
	0x15,		/* disp_cc_mdss_dp_link_clk */
	0x16,		/* disp_cc_mdss_dp_link_intf_clk */
	0x18,		/* disp_cc_mdss_dp_pixel_clk */
	0x14,		/* disp_cc_mdss_esc0_clk */
	0xE,		/* disp_cc_mdss_mdp_clk */
	0x10,		/* disp_cc_mdss_mdp_lut_clk */
	0x1B,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0xD,		/* disp_cc_mdss_pclk0_clk */
	0xF,		/* disp_cc_mdss_rot_clk */
	0x11,		/* disp_cc_mdss_vsync_clk */
	0x23,		/* disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x7000,
	.post_div_offset = 0x5008,
	.cbcr_offset = 0x500C,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = disp_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(disp_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "disp_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = disp_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(disp_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"disp_cc_debug_mux",
	"gcc_ahb2phy_csi_clk",
	"gcc_ahb2phy_usb_clk",
	"gcc_apc_vs_clk",
	"gcc_bimc_gpu_axi_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_camera_ahb_clk",
	"gcc_camera_xo_clk",
	"gcc_camss_cci_ahb_clk",
	"gcc_camss_cci_clk",
	"gcc_camss_cphy_csid0_clk",
	"gcc_camss_cphy_csid1_clk",
	"gcc_camss_cphy_csid2_clk",
	"gcc_camss_cphy_csid3_clk",
	"gcc_camss_cpp_ahb_clk",
	"gcc_camss_cpp_axi_clk",
	"gcc_camss_cpp_clk",
	"gcc_camss_cpp_vbif_ahb_clk",
	"gcc_camss_csi0_ahb_clk",
	"gcc_camss_csi0_clk",
	"gcc_camss_csi0phytimer_clk",
	"gcc_camss_csi0pix_clk",
	"gcc_camss_csi0rdi_clk",
	"gcc_camss_csi1_ahb_clk",
	"gcc_camss_csi1_clk",
	"gcc_camss_csi1phytimer_clk",
	"gcc_camss_csi1pix_clk",
	"gcc_camss_csi1rdi_clk",
	"gcc_camss_csi2_ahb_clk",
	"gcc_camss_csi2_clk",
	"gcc_camss_csi2phytimer_clk",
	"gcc_camss_csi2pix_clk",
	"gcc_camss_csi2rdi_clk",
	"gcc_camss_csi3_ahb_clk",
	"gcc_camss_csi3_clk",
	"gcc_camss_csi3pix_clk",
	"gcc_camss_csi3rdi_clk",
	"gcc_camss_csi_vfe0_clk",
	"gcc_camss_csi_vfe1_clk",
	"gcc_camss_csiphy0_clk",
	"gcc_camss_csiphy1_clk",
	"gcc_camss_csiphy2_clk",
	"gcc_camss_csiphy3_clk",
	"gcc_camss_gp0_clk",
	"gcc_camss_gp1_clk",
	"gcc_camss_ispif_ahb_clk",
	"gcc_camss_jpeg_ahb_clk",
	"gcc_camss_jpeg_axi_clk",
	"gcc_camss_jpeg_clk",
	"gcc_camss_mclk0_clk",
	"gcc_camss_mclk1_clk",
	"gcc_camss_mclk2_clk",
	"gcc_camss_mclk3_clk",
	"gcc_camss_micro_ahb_clk",
	"gcc_camss_throttle_nrt_axi_clk",
	"gcc_camss_throttle_rt_axi_clk",
	"gcc_camss_top_ahb_clk",
	"gcc_camss_vfe0_ahb_clk",
	"gcc_camss_vfe0_clk",
	"gcc_camss_vfe0_stream_clk",
	"gcc_camss_vfe1_ahb_clk",
	"gcc_camss_vfe1_clk",
	"gcc_camss_vfe1_stream_clk",
	"gcc_camss_vfe_vbif_ahb_clk",
	"gcc_camss_vfe_vbif_axi_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_cpuss_gnoc_clk",
	"gcc_cpuss_throttle_core_clk",
	"gcc_cpuss_throttle_xo_clk",
	"gcc_disp_ahb_clk",
	"gcc_disp_gpll0_div_clk_src",
	"gcc_disp_hf_axi_clk",
	"gcc_disp_throttle_core_clk",
	"gcc_disp_xo_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gpu_cfg_ahb_clk",
	"gcc_gpu_gpll0_clk_src",
	"gcc_gpu_gpll0_div_clk_src",
	"gcc_gpu_memnoc_gfx_clk",
	"gcc_gpu_snoc_dvm_gfx_clk",
	"gcc_gpu_throttle_core_clk",
	"gcc_gpu_throttle_xo_clk",
	"gcc_mss_vs_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_prng_ahb_clk",
	"gcc_qmip_camera_nrt_ahb_clk",
	"gcc_qmip_camera_rt_ahb_clk",
	"gcc_qmip_cpuss_cfg_ahb_clk",
	"gcc_qmip_disp_ahb_clk",
	"gcc_qmip_gpu_cfg_ahb_clk",
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap1_core_2x_clk",
	"gcc_qupv3_wrap1_core_clk",
	"gcc_qupv3_wrap1_s0_clk",
	"gcc_qupv3_wrap1_s1_clk",
	"gcc_qupv3_wrap1_s2_clk",
	"gcc_qupv3_wrap1_s3_clk",
	"gcc_qupv3_wrap1_s4_clk",
	"gcc_qupv3_wrap1_s5_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_qupv3_wrap_1_m_ahb_clk",
	"gcc_qupv3_wrap_1_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_cpuss_ahb_clk",
	"gcc_sys_noc_ufs_phy_axi_clk",
	"gcc_sys_noc_usb3_prim_axi_clk",
	"gcc_ufs_phy_ahb_clk",
	"gcc_ufs_phy_axi_clk",
	"gcc_ufs_phy_ice_core_clk",
	"gcc_ufs_phy_phy_aux_clk",
	"gcc_ufs_phy_rx_symbol_0_clk",
	"gcc_ufs_phy_tx_symbol_0_clk",
	"gcc_ufs_phy_unipro_core_clk",
	"gcc_usb30_prim_master_clk",
	"gcc_usb30_prim_mock_utmi_clk",
	"gcc_usb30_prim_sleep_clk",
	"gcc_usb3_prim_phy_com_aux_clk",
	"gcc_usb3_prim_phy_pipe_clk",
	"gcc_vdda_vs_clk",
	"gcc_vddcx_vs_clk",
	"gcc_vddmx_vs_clk",
	"gcc_video_ahb_clk",
	"gcc_video_axi0_clk",
	"gcc_video_throttle_core_clk",
	"gcc_video_xo_clk",
	"gcc_vs_ctrl_ahb_clk",
	"gcc_vs_ctrl_clk",
	"gcc_wcss_vs_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_snoc_clk",
	"video_cc_debug_mux",
};

static int gcc_debug_mux_sels[] = {
	0x41,		/* disp_cc_debug_mux */
	0x5F,		/* gcc_ahb2phy_csi_clk */
	0x60,		/* gcc_ahb2phy_usb_clk */
	0xBF,		/* gcc_apc_vs_clk */
	0x8C,		/* gcc_bimc_gpu_axi_clk */
	0x72,		/* gcc_boot_rom_ahb_clk */
	0x36,		/* gcc_camera_ahb_clk */
	0x3E,		/* gcc_camera_xo_clk */
	0x11F,		/* gcc_camss_cci_ahb_clk */
	0x11E,		/* gcc_camss_cci_clk */
	0x13A,		/* gcc_camss_cphy_csid0_clk */
	0x140,		/* gcc_camss_cphy_csid1_clk */
	0x145,		/* gcc_camss_cphy_csid2_clk */
	0x14B,		/* gcc_camss_cphy_csid3_clk */
	0x154,		/* gcc_camss_cpp_ahb_clk */
	0x156,		/* gcc_camss_cpp_axi_clk */
	0x153,		/* gcc_camss_cpp_clk */
	0x155,		/* gcc_camss_cpp_vbif_ahb_clk */
	0x13B,		/* gcc_camss_csi0_ahb_clk */
	0x139,		/* gcc_camss_csi0_clk */
	0x120,		/* gcc_camss_csi0phytimer_clk */
	0x13D,		/* gcc_camss_csi0pix_clk */
	0x13C,		/* gcc_camss_csi0rdi_clk */
	0x141,		/* gcc_camss_csi1_ahb_clk */
	0x13F,		/* gcc_camss_csi1_clk */
	0x121,		/* gcc_camss_csi1phytimer_clk */
	0x143,		/* gcc_camss_csi1pix_clk */
	0x142,		/* gcc_camss_csi1rdi_clk */
	0x146,		/* gcc_camss_csi2_ahb_clk */
	0x144,		/* gcc_camss_csi2_clk */
	0x122,		/* gcc_camss_csi2phytimer_clk */
	0x148,		/* gcc_camss_csi2pix_clk */
	0x147,		/* gcc_camss_csi2rdi_clk */
	0x14C,		/* gcc_camss_csi3_ahb_clk */
	0x14A,		/* gcc_camss_csi3_clk */
	0x14E,		/* gcc_camss_csi3pix_clk */
	0x14D,		/* gcc_camss_csi3rdi_clk */
	0x12E,		/* gcc_camss_csi_vfe0_clk */
	0x12F,		/* gcc_camss_csi_vfe1_clk */
	0x135,		/* gcc_camss_csiphy0_clk */
	0x136,		/* gcc_camss_csiphy1_clk */
	0x137,		/* gcc_camss_csiphy2_clk */
	0x138,		/* gcc_camss_csiphy3_clk */
	0x118,		/* gcc_camss_gp0_clk */
	0x119,		/* gcc_camss_gp1_clk */
	0x134,		/* gcc_camss_ispif_ahb_clk */
	0x124,		/* gcc_camss_jpeg_ahb_clk */
	0x125,		/* gcc_camss_jpeg_axi_clk */
	0x123,		/* gcc_camss_jpeg_clk */
	0x11A,		/* gcc_camss_mclk0_clk */
	0x11B,		/* gcc_camss_mclk1_clk */
	0x11C,		/* gcc_camss_mclk2_clk */
	0x11D,		/* gcc_camss_mclk3_clk */
	0x152,		/* gcc_camss_micro_ahb_clk */
	0x150,		/* gcc_camss_throttle_nrt_axi_clk */
	0x151,		/* gcc_camss_throttle_rt_axi_clk */
	0x14F,		/* gcc_camss_top_ahb_clk */
	0x12A,		/* gcc_camss_vfe0_ahb_clk */
	0x128,		/* gcc_camss_vfe0_clk */
	0x129,		/* gcc_camss_vfe0_stream_clk */
	0x12D,		/* gcc_camss_vfe1_ahb_clk */
	0x12B,		/* gcc_camss_vfe1_clk */
	0x12C,		/* gcc_camss_vfe1_stream_clk */
	0x130,		/* gcc_camss_vfe_vbif_ahb_clk */
	0x131,		/* gcc_camss_vfe_vbif_axi_clk */
	0x1D,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0xA6,		/* gcc_cpuss_gnoc_clk */
	0xAE,		/* gcc_cpuss_throttle_core_clk */
	0xAD,		/* gcc_cpuss_throttle_xo_clk */
	0x37,		/* gcc_disp_ahb_clk */
	0x46,		/* gcc_disp_gpll0_div_clk_src */
	0x3C,		/* gcc_disp_hf_axi_clk */
	0x48,		/* gcc_disp_throttle_core_clk */
	0x3F,		/* gcc_disp_xo_clk */
	0xB6,		/* gcc_gp1_clk */
	0xB7,		/* gcc_gp2_clk */
	0xB8,		/* gcc_gp3_clk */
	0xDB,		/* gcc_gpu_cfg_ahb_clk */
	0xE1,		/* gcc_gpu_gpll0_clk_src */
	0xE2,		/* gcc_gpu_gpll0_div_clk_src */
	0xDE,		/* gcc_gpu_memnoc_gfx_clk */
	0xE0,		/* gcc_gpu_snoc_dvm_gfx_clk */
	0xE5,		/* gcc_gpu_throttle_core_clk */
	0xE4,		/* gcc_gpu_throttle_xo_clk */
	0xBE,		/* gcc_mss_vs_clk */
	0x6F,		/* gcc_pdm2_clk */
	0x6D,		/* gcc_pdm_ahb_clk */
	0x6E,		/* gcc_pdm_xo4_clk */
	0x70,		/* gcc_prng_ahb_clk */
	0x39,		/* gcc_qmip_camera_nrt_ahb_clk */
	0x47,		/* gcc_qmip_camera_rt_ahb_clk */
	0xAC,		/* gcc_qmip_cpuss_cfg_ahb_clk */
	0x3A,		/* gcc_qmip_disp_ahb_clk */
	0xE3,		/* gcc_qmip_gpu_cfg_ahb_clk */
	0x38,		/* gcc_qmip_video_vcodec_ahb_clk */
	0x66,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x65,		/* gcc_qupv3_wrap0_core_clk */
	0x67,		/* gcc_qupv3_wrap0_s0_clk */
	0x68,		/* gcc_qupv3_wrap0_s1_clk */
	0x69,		/* gcc_qupv3_wrap0_s2_clk */
	0x6A,		/* gcc_qupv3_wrap0_s3_clk */
	0x6B,		/* gcc_qupv3_wrap0_s4_clk */
	0x6C,		/* gcc_qupv3_wrap0_s5_clk */
	0xED,		/* gcc_qupv3_wrap1_core_2x_clk */
	0xEC,		/* gcc_qupv3_wrap1_core_clk */
	0xEE,		/* gcc_qupv3_wrap1_s0_clk */
	0xEF,		/* gcc_qupv3_wrap1_s1_clk */
	0xF0,		/* gcc_qupv3_wrap1_s2_clk */
	0xF1,		/* gcc_qupv3_wrap1_s3_clk */
	0xF2,		/* gcc_qupv3_wrap1_s4_clk */
	0xF3,		/* gcc_qupv3_wrap1_s5_clk */
	0x63,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x64,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0xEA,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0xEB,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0xE8,		/* gcc_sdcc1_ahb_clk */
	0xE7,		/* gcc_sdcc1_apps_clk */
	0xE9,		/* gcc_sdcc1_ice_core_clk */
	0x62,		/* gcc_sdcc2_ahb_clk */
	0x61,		/* gcc_sdcc2_apps_clk */
	0x9,		/* gcc_sys_noc_cpuss_ahb_clk */
	0x19,		/* gcc_sys_noc_ufs_phy_axi_clk */
	0x18,		/* gcc_sys_noc_usb3_prim_axi_clk */
	0x10F,		/* gcc_ufs_phy_ahb_clk */
	0x10E,		/* gcc_ufs_phy_axi_clk */
	0x115,		/* gcc_ufs_phy_ice_core_clk */
	0x116,		/* gcc_ufs_phy_phy_aux_clk */
	0x111,		/* gcc_ufs_phy_rx_symbol_0_clk */
	0x110,		/* gcc_ufs_phy_tx_symbol_0_clk */
	0x114,		/* gcc_ufs_phy_unipro_core_clk */
	0x58,		/* gcc_usb30_prim_master_clk */
	0x5A,		/* gcc_usb30_prim_mock_utmi_clk */
	0x59,		/* gcc_usb30_prim_sleep_clk */
	0x5B,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x5C,		/* gcc_usb3_prim_phy_pipe_clk */
	0xBB,		/* gcc_vdda_vs_clk */
	0xB9,		/* gcc_vddcx_vs_clk */
	0xBA,		/* gcc_vddmx_vs_clk */
	0x35,		/* gcc_video_ahb_clk */
	0x3B,		/* gcc_video_axi0_clk */
	0x49,		/* gcc_video_throttle_core_clk */
	0x3D,		/* gcc_video_xo_clk */
	0xBD,		/* gcc_vs_ctrl_ahb_clk */
	0xBC,		/* gcc_vs_ctrl_clk */
	0xC0,		/* gcc_wcss_vs_clk */
	0xDD,		/* gpu_cc_debug_mux */
	0x97,		/* mc_cc_debug_mux */
	0x1A,		/* measure_only_cnoc_clk */
	0xC2,		/* measure_only_ipa_2x_clk */
	0x7,		/* measure_only_snoc_clk */
	0x42,		/* video_cc_debug_mux */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62000,
	.post_div_offset = 0x30000,
	.cbcr_offset = 0x30004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 1,
	.mux_sels = gcc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gcc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
	},
};

static const char *const gpu_cc_debug_mux_parent_names[] = {
	"gpu_cc_ahb_clk",
	"gpu_cc_crc_ahb_clk",
	"gpu_cc_cx_apb_clk",
	"gpu_cc_cx_gfx3d_clk",
	"gpu_cc_cx_gfx3d_slv_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cx_snoc_dvm_clk",
	"gpu_cc_cxo_aon_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_gx_gfx3d_clk",
	"gpu_cc_sleep_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x10,		/* gpu_cc_ahb_clk */
	0x11,		/* gpu_cc_crc_ahb_clk */
	0x14,		/* gpu_cc_cx_apb_clk */
	0x1A,		/* gpu_cc_cx_gfx3d_clk */
	0x1B,		/* gpu_cc_cx_gfx3d_slv_clk */
	0x18,		/* gpu_cc_cx_gmu_clk */
	0x15,		/* gpu_cc_cx_snoc_dvm_clk */
	0xA,		/* gpu_cc_cxo_aon_clk */
	0x19,		/* gpu_cc_cxo_clk */
	0xB,		/* gpu_cc_gx_gfx3d_clk */
	0x16,		/* gpu_cc_sleep_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x1568,
	.post_div_offset = 0x10FC,
	.cbcr_offset = 0x1100,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = gpu_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gpu_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "gpu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gpu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gpu_cc_debug_mux_parent_names),
	},
};

static const char *const video_cc_debug_mux_parent_names[] = {
	"video_cc_apb_clk",
	"video_cc_at_clk",
	"video_cc_sleep_clk",
	"video_cc_vcodec0_axi_clk",
	"video_cc_vcodec0_core_clk",
	"video_cc_venus_ahb_clk",
	"video_cc_venus_ctl_axi_clk",
	"video_cc_venus_ctl_core_clk",
	"video_cc_xo_clk",
};

static int video_cc_debug_mux_sels[] = {
	0x8,		/* video_cc_apb_clk */
	0xB,		/* video_cc_at_clk */
	0xD,		/* video_cc_sleep_clk */
	0x6,		/* video_cc_vcodec0_axi_clk */
	0x3,		/* video_cc_vcodec0_core_clk */
	0x9,		/* video_cc_venus_ahb_clk */
	0x5,		/* video_cc_venus_ctl_axi_clk */
	0x1,		/* video_cc_venus_ctl_core_clk */
	0xC,		/* video_cc_xo_clk */
};

static struct clk_debug_mux video_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0xA4C,
	.post_div_offset = 0xA30,
	.cbcr_offset = 0xA38,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0x7,
	.post_div_shift = 0,
	.post_div_val = 5,
	.mux_sels = video_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(video_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "video_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = video_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(video_cc_debug_mux_parent_names),
	},
};

static const char *const mc_cc_debug_mux_parent_names[] = {
	"measure_only_mccc_clk",
};

static struct clk_debug_mux mc_cc_debug_mux = {
	.period_offset = 0x50,
	.hw.init = &(struct clk_init_data){
		.name = "mc_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = mc_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(mc_cc_debug_mux_parent_names),
	},
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &video_cc_debug_mux, .regmap_name = "qcom,videocc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cnoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ipa_2x_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ipa_2x_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_snoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_snoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_trinket_hws[] = {
	&measure_only_cnoc_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_snoc_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,trinket-debugcc" },
	{ }
};

static int clk_debug_trinket_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(video_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(video_cc_debug_mux_sels));

	clk = devm_clk_get(&pdev->dev, "xo_clk_src");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}

	debug_mux_priv.cxo = clk;

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		if (IS_ERR_OR_NULL(mux_list[i].mux->regmap)) {
			ret = map_debug_bases(pdev, mux_list[i].regmap_name,
					      mux_list[i].mux);
			if (ret == -EBADR)
				continue;
			else if (ret)
				return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_trinket_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_trinket_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%d)\n",
				clk_hw_get_name(debugcc_trinket_hws[i]),
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		ret = devm_clk_register_debug_mux(&pdev->dev, mux_list[i].mux);
		if (ret) {
			dev_err(&pdev->dev, "Unable to register mux clk %s, err:(%d)\n",
				qcom_clk_hw_get_name(&mux_list[i].mux->hw),
				ret);
			return ret;
		}
	}

	ret = clk_debug_measure_register(&gcc_debug_mux.hw);
	if (ret) {
		dev_err(&pdev->dev, "Could not register Measure clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered debug measure clocks\n");

	return ret;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_trinket_probe,
	.driver = {
		.name = "trinket-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_trinket_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_trinket_init);

MODULE_DESCRIPTION("QTI DEBUG CC TRINKET Driver");
MODULE_LICENSE("GPL v2");
