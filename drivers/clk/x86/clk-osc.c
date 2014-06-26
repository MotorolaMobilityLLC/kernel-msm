/*
 * Intel OSC clocks.
 *
 * Copyright (C) 2014, Intel Corporation
 * Authors: Junxiao Chang <junxiao.chang@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/module.h>

#define CCU_OSC_CTL_OFF		0x404
#define OSC_CLOCK_RATE		19200
#define OSC_CLOCK_COUNT		5
#define to_clk_osc(_hw) container_of(_hw, struct osc_clk_data, hw)

struct osc_clk_data {
	void __iomem *oscaddr;
	int id;
	unsigned int rate;
	unsigned int div;
	struct clk_hw hw;
	struct clk *clk;
};

static unsigned long clk_osc_recalc_rate(struct clk_hw *hw,
				      unsigned long parent_rate)
{
	struct osc_clk_data *pdata = to_clk_osc(hw);
	unsigned int reg_osc_ctrl;
	unsigned int div;
	unsigned long rate;

	reg_osc_ctrl = readl(pdata->oscaddr);
	div = reg_osc_ctrl & (BIT(1) | BIT(0));

	/*
	 * Bit 0:1 is div ctrl:
	 * 00: div 4
	 * 01: div 2
	 * 10: div 1
	 * 11: x2
	 */
	if (div == 0)
		rate = parent_rate / 4;
	else if (div == 1)
		rate = parent_rate / 2;
	else if (div == 2)
		rate = parent_rate;
	else if (div == 3)
		rate = parent_rate * 2;

	return rate;
}

static long clk_osc_round_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long *prate)
{
	struct osc_clk_data *pdata = to_clk_osc(hw);
	unsigned long round;

	/* OSC supports div4, div2, div1, x2. */
	if (rate > *prate * 2)
		pr_warn("%s req rate: %ld. Max osc clk rate: %ld\n",
				__func__, rate, *prate * 2);

	/*
	 * Bit 0:1 is div ctrl:
	 * 00: div 4
	 * 01: div 2
	 * 10: div 1
	 * 11: x2
	 */
	if (rate <= *prate / 4) {
		round = *prate / 4;
		pdata->div = 0;
	} else if (rate <= *prate / 2) {
		round = *prate / 2;
		pdata->div = 1;
	} else if (rate <= *prate) {
		round = *prate;
		pdata->div = 2;
	} else if (rate <= *prate * 2) {
		round = *prate * 2;
		pdata->div = 3;
	} else {
		/* Set to default 2, div 1 */
		round = *prate;
		pdata->div = 2;
	}

	pr_debug("%s: rate:%ld, parent rate:%ld, round rate:%ld.\n",
			__func__, rate, *prate, round);

	return round;
}

static void clk_osc_disable_unused(struct clk_hw *hw)
{
	/*
	 * This API is to prevent clk framework disabling unused clks.
	 * By default, some clks are turned on during BIOS init, if kernel
	 * driver needs these clks and didn't turn on clks in its driver,
	 * new clk framework might impact it.
	 * Linux clk framework will turn off unused clks during bootup.
	 * This hook is added to prevent clk framework turns off unused clks.
	 * If all drivers could handle clks initialization well in future, we
	 * could remove this "disabled_unused" hook.
	 */
	return;
}

static int clk_osc_set_rate(struct clk_hw *hw, unsigned long rate,
			 unsigned long parent_rate)
{
	struct osc_clk_data *pdata = to_clk_osc(hw);
	unsigned int reg_osc_ctrl;

	reg_osc_ctrl = readl(pdata->oscaddr);
	reg_osc_ctrl &= ~(BIT(0) | BIT(1));

	/* div has been saved in round_rate */
	reg_osc_ctrl |= pdata->div;

	writel(reg_osc_ctrl, pdata->oscaddr);
	return 0;
}

static int clk_osc_is_enabled(struct clk_hw *hw)
{
	struct osc_clk_data *pdata = to_clk_osc(hw);
	unsigned int reg_osc_ctrl;

	reg_osc_ctrl = readl(pdata->oscaddr);
	if (reg_osc_ctrl & BIT(31))
		return 1;

	return 0;
}

static int clk_osc_enable(struct clk_hw *hw)
{
	struct osc_clk_data *pdata = to_clk_osc(hw);
	unsigned int reg_osc_ctrl;

	reg_osc_ctrl = readl(pdata->oscaddr);
	reg_osc_ctrl |= BIT(31);

	writel(reg_osc_ctrl, pdata->oscaddr);
	return 0;
}

static void clk_osc_disable(struct clk_hw *hw)
{
	struct osc_clk_data *pdata = to_clk_osc(hw);
	unsigned int reg_osc_ctrl;

	reg_osc_ctrl = readl(pdata->oscaddr);
	reg_osc_ctrl &= ~(BIT(31));

	writel(reg_osc_ctrl, pdata->oscaddr);
	return;
}

const struct clk_ops clk_osc_ops = {
	.recalc_rate = clk_osc_recalc_rate,
	.round_rate = clk_osc_round_rate,
	.set_rate = clk_osc_set_rate,
	.is_enabled = clk_osc_is_enabled,
	.disable_unused = clk_osc_disable_unused,
	.enable = clk_osc_enable,
	.disable = clk_osc_disable,
};

struct clk *ccu_osc_clk_register(const char *name, const char *parent, void __iomem *oscaddr, int id)
{
	struct osc_clk_data *pdata;
	struct clk *clk;
	struct clk_init_data init;

	pdata = kzalloc(sizeof(struct osc_clk_data), GFP_KERNEL);
	if (pdata == NULL)
		return NULL;

	init.name = name;
	init.ops = &clk_osc_ops;
	init.flags = CLK_IS_BASIC;
	init.parent_names = (parent ? &parent : NULL);
	init.num_parents = (parent ? 1 : 0);

	pdata->oscaddr = oscaddr;
	pdata->id = id;
	pdata->hw.init = &init;

	clk = clk_register(NULL, &pdata->hw);
	if (IS_ERR(clk)) {
		kfree(pdata);
		pr_err("%s:clk register error:%p\n", __func__, clk);
	}

	return clk;
}

/*
 * ccu_osc_clk_init
 * ccu is in same PCI device with PMU. CCU address is at offset 0x800.
 * ccu's inialization is called from PMU init.
 */
int ccu_osc_clk_init(void __iomem *ccubase)
{
	struct clk *clk;
	int i, ret;
	char name[12];

	pr_debug("%s entry\n", __func__);

	clk = clk_register_fixed_rate(NULL, "clk-osc", NULL,
			CLK_IS_ROOT, OSC_CLOCK_RATE);
	if (IS_ERR(clk)) {
		pr_err("%s:clk register fail.\n", __func__);
		return -1;
	}
	clk_register_clkdev(clk, "clk-osc", NULL);

	for (i = 0; i < OSC_CLOCK_COUNT; i++) {
		memset(name, 0, sizeof(name));
		sprintf(name, "osc.%d", i);
		clk = ccu_osc_clk_register(name, "clk-osc", ccubase
				+ CCU_OSC_CTL_OFF + i * 4, i);
		if (!IS_ERR(clk))
			ret = clk_register_clkdev(clk, name, NULL);
	}

	return 0;
}

/*
 * ccu_osc_clk_uninit
 * When kernel is trying to remove PMU driver, return -1 to prevent it.
 * clk framework doesn't have a de-register function, so clk driver couldn't
 * be stopped. Currently PMU driver will not be removed, this function is to
 * double confirm CCU/OSC resource will not be free.
 */
int ccu_osc_clk_uninit(void)
{
	pr_warn("OSC clk resources couldn't be released.\n");
	return -1;
}
