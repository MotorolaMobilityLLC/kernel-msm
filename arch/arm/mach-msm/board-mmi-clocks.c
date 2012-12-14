/* Copyright (c) 2012, Motorola Mobility, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <linux/clk.h>
#include <linux/of.h>
#include "clock-8960.h"
#include "clock-gsbi-8960.h"

/* Value of QUALCOMM_CLOCKTREE_CLOCK */
#define DT_QC_CLOCKTREE_CLOCK	0x001E0015

static struct clk *clk_dt_lookup_table[] __initdata = {
	[0x0000] = &(gsbi1_uart_clk.c),
	[0x0001] = &(gsbi1_qup_clk.c),
	[0x0002] = &(gsbi1_p_clk.c),
	[0x0003] = &(gsbi2_uart_clk.c),
	[0x0004] = &(gsbi2_qup_clk.c),
	[0x0005] = &(gsbi2_p_clk.c),
	[0x0006] = &(gsbi3_uart_clk.c),
	[0x0007] = &(gsbi3_qup_clk.c),
	[0x0008] = &(gsbi3_p_clk.c),
	[0x0009] = &(gsbi4_uart_clk.c),
	[0x000A] = &(gsbi4_qup_clk.c),
	[0x000B] = &(gsbi4_p_clk.c),
	[0x000C] = &(gsbi5_uart_clk.c),
	[0x000D] = &(gsbi5_qup_clk.c),
	[0x000E] = &(gsbi5_p_clk.c),
	[0x000F] = &(gsbi6_uart_clk.c),
	[0x0010] = &(gsbi6_qup_clk.c),
	[0x0011] = &(gsbi6_p_clk.c),
	[0x0012] = &(gsbi7_uart_clk.c),
	[0x0013] = &(gsbi7_qup_clk.c),
	[0x0014] = &(gsbi7_p_clk.c),
	[0x0015] = &(gsbi8_uart_clk.c),
	[0x0016] = &(gsbi8_qup_clk.c),
	[0x0017] = &(gsbi8_p_clk.c),
	[0x0018] = &(gsbi9_uart_clk.c),
	[0x0019] = &(gsbi9_qup_clk.c),
	[0x001A] = &(gsbi9_p_clk.c),
	[0x001B] = &(gsbi10_uart_clk.c),
	[0x001C] = &(gsbi10_qup_clk.c),
	[0x001D] = &(gsbi10_p_clk.c),
	[0x001E] = &(gsbi11_uart_clk.c),
	[0x001F] = &(gsbi11_qup_clk.c),
	[0x0020] = &(gsbi11_p_clk.c),
	[0x0021] = &(gsbi12_uart_clk.c),
	[0x0022] = &(gsbi12_qup_clk.c),
	[0x0023] = &(gsbi12_p_clk.c),
	[0x0024] = &(cam1_clk.c),
	[0x0025] = &(cam0_clk.c),
	[0x0026] = &(cam2_clk.c),
};

#define CLK_DT_VALID_RANGE(c)	(c < ARRAY_SIZE(clk_dt_lookup_table))

__init struct clk_lookup *mmi_init_clocks_from_dt(int *size)
{
	struct clk_lookup *msm_clocks_mmi = NULL;
	struct device_node *parent;
	struct device_node *child;
	u32 clk_type;
	int count = 0;
	int index = 0;

	if (!size)
		return NULL;

	parent = of_find_node_by_path("/System@0/ClockTree@0");
	if (!parent)
		goto out;

	/* count number of clock nodes */
	for_each_child_of_node(parent, child) {
		clk_type = 0;
		of_property_read_u32(child, "type", &clk_type);
		if (clk_type == DT_QC_CLOCKTREE_CLOCK)
			count++;
	}

	if (!count) {
		pr_warn("%s: found no clocks in device tree\n", __func__);
		goto out;
	}

	msm_clocks_mmi = kmalloc(sizeof(struct clk_lookup)*count,
				GFP_KERNEL);
	if (!msm_clocks_mmi) {
		pr_warn("%s: clk_lookup allocation failed\n", __func__);
		count = 0;
		goto out;
	}

	for_each_child_of_node(parent, child) {
		clk_type = 0;
		if (of_property_read_u32(child, "type", &clk_type)) {
			pr_warn("%s: reading clock type failed!\n",
				__func__);
			continue;
		} else {
			const char *nm;
			const char *dev;
			u32 id;
			uint16_t clkid;
			struct clk *clk;
			struct clk_lookup *p;

			if (clk_type != DT_QC_CLOCKTREE_CLOCK)
				continue;

			if (of_property_read_string(child, "clockname", &nm)) {
				pr_warn("%s: clockname property not found!\n",
					__func__);
				continue;
			}

			if (of_property_read_string(child, "deviceid", &dev)) {
				pr_warn("%s: deviceid property not found!\n",
					__func__);
				continue;
			}

			if (of_property_read_u32(child, "clock", &id)) {
				pr_warn("%s: clock property not found!\n",
					__func__);
				continue;
			}

			clkid = id;
			if (!CLK_DT_VALID_RANGE(clkid)) {
				pr_warn("%s: clock index (%d) out of range\n",
					__func__, clkid);
				continue;
			}
			clk = clk_dt_lookup_table[clkid];

			/* basic sanity passed, load to table */
			p = &msm_clocks_mmi[index];
			p->clk = clk;
			p->con_id = kstrndup(nm, strlen(nm), GFP_KERNEL);
			if (!p->con_id) {
				pr_warn("%s: clockname string dup failed\n",
					__func__);
				continue;
			}

			p->dev_id = kstrndup(dev, strlen(dev), GFP_KERNEL);
			if (!p->dev_id) {
				pr_warn("%s: deviceid string dup failed\n",
					__func__);
				kfree(p->con_id);
				continue;
			}
			index++;
		}
	}

	if (count != index) {
		pr_warn("%s: some clocks failed to install\n", __func__);
		count = index;
	}
out:
	of_node_put(parent);
	*size = count;
	return msm_clocks_mmi;
}
