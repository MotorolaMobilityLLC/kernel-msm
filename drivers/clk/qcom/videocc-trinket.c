// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,videocc-trinket.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_cx, VDD_NUM, 1, vdd_corner);

static struct clk_vdd_class *video_cc_trinket_regulators[] = {
	&vdd_cx,
};

enum {
	P_BI_TCXO,
	P_SLEEP_CLK,
	P_VIDEO_PLL0_OUT_AUX,
	P_VIDEO_PLL0_OUT_AUX2,
	P_VIDEO_PLL0_OUT_MAIN,
};

static const struct pll_vco spark_vco[] = {
	{ 1000000000, 2000000000, 0 },
	{ 750000000, 1500000000, 1 },
	{ 500000000, 1000000000, 2 },
	{ 300000000, 500000000, 3 },
	{ 550000000, 1100000000, 4 },
};

/* 600MHz configuration */
static const struct alpha_pll_config video_pll0_config = {
	.l = 0x1F,
	.alpha_hi = 0x40,
	.alpha = 0x00,
	.alpha_en_mask = BIT(24),
	.vco_val = 0x2 << 20,
	.vco_mask = 0x3 << 20,
	.main_output_mask = BIT(0),
	.config_ctl_val = 0x4001055b,
};

static const u8 clk_alpha_pll_regs_offset[][PLL_OFF_MAX_REGS] = {
	[CLK_ALPHA_PLL_TYPE_DEFAULT] =  {
		[PLL_OFF_L_VAL] = 0x04,
		[PLL_OFF_ALPHA_VAL] = 0x08,
		[PLL_OFF_ALPHA_VAL_U] = 0x0c,
		[PLL_OFF_USER_CTL] = 0x10,
		[PLL_OFF_USER_CTL_U] = 0x14,
		[PLL_OFF_CONFIG_CTL] = 0x18,
		[PLL_OFF_TEST_CTL] = 0x1C,
		[PLL_OFF_TEST_CTL_U] = 0x20,
	},
};

static struct clk_alpha_pll video_pll0 = {
	.offset = 0x42c,
	.vco_table = spark_vco,
	.num_vco = ARRAY_SIZE(spark_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_DEFAULT],
	.flags = SUPPORTS_DYNAMIC_UPDATE,
	.clkr = {
		.hw.init = &(const struct clk_init_data){
			.name = "video_pll0",
			.parent_data = &(const struct clk_parent_data){
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_cx,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_MIN] = 1100000000,
				[VDD_NOMINAL] = 2000000000},
		},
	},
};

static const struct parent_map video_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_VIDEO_PLL0_OUT_MAIN, 1 },
	{ P_VIDEO_PLL0_OUT_AUX, 2 },
	{ P_VIDEO_PLL0_OUT_AUX2, 3 },
};

static const struct clk_parent_data video_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &video_pll0.clkr.hw },
	{ .hw = &video_pll0.clkr.hw },
	{ .hw = &video_pll0.clkr.hw },
};

static const struct freq_tbl ftbl_video_cc_venus_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(133333333, P_VIDEO_PLL0_OUT_MAIN, 4.5, 0, 0),
	F(240000000, P_VIDEO_PLL0_OUT_MAIN, 2.5, 0, 0),
	F(300000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(380000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(410000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	F(460000000, P_VIDEO_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 video_cc_venus_clk_src = {
	.cmd_rcgr = 0x7f0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = video_cc_parent_map_0,
	.freq_tbl = ftbl_video_cc_venus_clk_src,
	.enable_safe_config = true,
	.clkr.hw.init = &(const struct clk_init_data){
		.name = "video_cc_venus_clk_src",
		.parent_data = video_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(video_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_cx,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 133333333,
			[VDD_LOW] = 240000000,
			[VDD_LOW_L1] = 300000000,
			[VDD_NOMINAL] = 380000000,
			[VDD_NOMINAL_L1] = 410000000,
			[VDD_HIGH] = 460000000},
	},
};

static struct clk_branch video_cc_apb_clk = {
	.halt_reg = 0x990,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x990,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_apb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_vcodec0_axi_clk = {
	.halt_reg = 0x8f0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8f0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_vcodec0_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_vcodec0_core_clk = {
	.halt_reg = 0x890,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x890,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_vcodec0_core_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_venus_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ahb_clk = {
	.halt_reg = 0x9b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x9b0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_venus_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ctl_axi_clk = {
	.halt_reg = 0x8d0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8d0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_venus_ctl_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch video_cc_venus_ctl_core_clk = {
	.halt_reg = 0x850,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x850,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data){
			.name = "video_cc_venus_ctl_core_clk",
			.parent_hws = (const struct clk_hw*[]){
				&video_cc_venus_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *video_cc_trinket_clocks[] = {
	[VIDEO_CC_APB_CLK] = &video_cc_apb_clk.clkr,
	[VIDEO_CC_VCODEC0_AXI_CLK] = &video_cc_vcodec0_axi_clk.clkr,
	[VIDEO_CC_VCODEC0_CORE_CLK] = &video_cc_vcodec0_core_clk.clkr,
	[VIDEO_CC_VENUS_AHB_CLK] = &video_cc_venus_ahb_clk.clkr,
	[VIDEO_CC_VENUS_CLK_SRC] = &video_cc_venus_clk_src.clkr,
	[VIDEO_CC_VENUS_CTL_AXI_CLK] = &video_cc_venus_ctl_axi_clk.clkr,
	[VIDEO_CC_VENUS_CTL_CORE_CLK] = &video_cc_venus_ctl_core_clk.clkr,
	[VIDEO_CC_PLL0] = &video_pll0.clkr,
};

static const struct qcom_reset_map video_cc_trinket_resets[] = {
	[VIDEO_CC_INTERFACE_BCR] = { 0x8b0 },
	[VIDEO_CC_VCODEC0_BCR] = { 0x870 },
	[VIDEO_CC_VENUS_BCR] = { 0x810 },
};

static const struct regmap_config video_cc_trinket_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xb94,
	.fast_io = true,
};

static const struct qcom_cc_desc video_cc_trinket_desc = {
	.config = &video_cc_trinket_regmap_config,
	.clks = video_cc_trinket_clocks,
	.num_clks = ARRAY_SIZE(video_cc_trinket_clocks),
	.resets = video_cc_trinket_resets,
	.num_resets = ARRAY_SIZE(video_cc_trinket_resets),
	.clk_regulators = video_cc_trinket_regulators,
	.num_clk_regulators = ARRAY_SIZE(video_cc_trinket_regulators),
};

static const struct of_device_id video_cc_trinket_match_table[] = {
	{ .compatible = "qcom,trinket-videocc" },
	{ }
};
MODULE_DEVICE_TABLE(of, video_cc_trinket_match_table);

static int video_cc_trinket_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &video_cc_trinket_desc);
	if (IS_ERR(regmap)) {
		pr_err("Failed to map the video_cc registers\n");
		return PTR_ERR(regmap);
	}

	clk_alpha_pll_configure(&video_pll0, regmap, &video_pll0_config);

	/*
	 * Keep clocks always enabled:
	 *	video_cc_sleep_clk
	 *	video_cc_xo_clk
	 */
	regmap_update_bits(regmap, 0xb18, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0xab8, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &video_cc_trinket_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register VIDEO CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered VIDEO CC clocks\n");

	return ret;
}

static void video_cc_trinket_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &video_cc_trinket_desc);
}

static struct platform_driver video_cc_trinket_driver = {
	.probe = video_cc_trinket_probe,
	.driver = {
		.name = "video_cc-trinket",
		.of_match_table = video_cc_trinket_match_table,
		.sync_state = video_cc_trinket_sync_state,
	},
};

static int __init video_cc_trinket_init(void)
{
	return platform_driver_register(&video_cc_trinket_driver);
}
subsys_initcall(video_cc_trinket_init);

static void __exit video_cc_trinket_exit(void)
{
	platform_driver_unregister(&video_cc_trinket_driver);
}
module_exit(video_cc_trinket_exit);

MODULE_DESCRIPTION("QTI VIDEO_CC TRINKET Driver");
MODULE_LICENSE("GPL v2");
