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
#include "clock-gsbi-8960.h"

/* Value of QUALCOMM_CLOCKTREE_CLOCK */
#define DT_QC_CLOCKTREE_CLOCK	0x001E0015

static struct clk *clk_dt_lookup_table[] __initdata = {
	&(gsbi1_uart_clk.c),
	&(gsbi1_qup_clk.c),
	&(gsbi1_p_clk.c),
	&(gsbi2_uart_clk.c),
	&(gsbi2_qup_clk.c),
	&(gsbi2_p_clk.c),
	&(gsbi3_uart_clk.c),
	&(gsbi3_qup_clk.c),
	&(gsbi3_p_clk.c),
	&(gsbi4_uart_clk.c),
	&(gsbi4_qup_clk.c),
	&(gsbi4_p_clk.c),
	&(gsbi5_uart_clk.c),
	&(gsbi5_qup_clk.c),
	&(gsbi5_p_clk.c),
	&(gsbi6_uart_clk.c),
	&(gsbi6_qup_clk.c),
	&(gsbi6_p_clk.c),
	&(gsbi7_uart_clk.c),
	&(gsbi7_qup_clk.c),
	&(gsbi7_p_clk.c),
	&(gsbi8_uart_clk.c),
	&(gsbi8_qup_clk.c),
	&(gsbi8_p_clk.c),
	&(gsbi9_uart_clk.c),
	&(gsbi9_qup_clk.c),
	&(gsbi9_p_clk.c),
	&(gsbi10_uart_clk.c),
	&(gsbi10_qup_clk.c),
	&(gsbi10_p_clk.c),
	&(gsbi11_uart_clk.c),
	&(gsbi11_qup_clk.c),
	&(gsbi11_p_clk.c),
	&(gsbi12_uart_clk.c),
	&(gsbi12_qup_clk.c),
	&(gsbi12_p_clk.c),
};

#define CLK_DT_VALID_RANGE(c)	(c < ARRAY_SIZE(clk_dt_lookup_table))

__init struct clk_lookup *mmi_init_clocks_from_dt(int *size)
{
	struct clk_lookup *msm_clocks_mmi = NULL;
	struct device_node *parent;
	struct device_node *child;
	int count = 0;
	int index = 0;

	if (!size)
		return NULL;

	parent = of_find_node_by_path("/System@0/ClockTree@0");
	if (!parent)
		goto out;

	/* count number of clock nodes */
	for_each_child_of_node(parent, child) {
		int len;
		const void *prop;

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32)))
			if (*(u32 *)prop == DT_QC_CLOCKTREE_CLOCK)
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
		int len;
		const void *prop_type;

		prop_type = of_get_property(child, "type", &len);
		if (prop_type && (len == sizeof(u32))) {
			const void *name;
			const void *dev;
			const void *id;
			int clk_len;
			int dev_len;
			uint16_t clkid;
			struct clk *clk;
			struct clk_lookup *p;

			if (*(u32 *)prop_type != DT_QC_CLOCKTREE_CLOCK)
				continue;

			name = of_get_property(child, "clockname", &clk_len);
			if (!name || !clk_len) {
				pr_warn("%s: clockname property not found!\n",
					__func__);
				continue;
			}

			dev = of_get_property(child, "deviceid", &dev_len);
			if (!dev || !dev_len) {
				pr_warn("%s: deviceid property not found!\n",
					__func__);
				continue;
			}

			id = of_get_property(child, "clock", &len);
			if (!id || len != sizeof(uint16_t)) {
				pr_warn("%s: clock property not found!\n",
					__func__);
				continue;
			}

			clkid = *(uint16_t *)id;
			if (!CLK_DT_VALID_RANGE(clkid)) {
				pr_warn("%s: clock index (%d) out of range\n",
					__func__, clkid);
				continue;
			}
			clk = clk_dt_lookup_table[clkid];

			/* basic sanity passed, load to table */
			p = &msm_clocks_mmi[index];
			p->clk = clk;
			p->con_id = kstrndup((const char *)name, clk_len,
					GFP_KERNEL);
			if (!p->con_id) {
				pr_warn("%s: clockname string dup failed\n",
					__func__);
				continue;
			}

			p->dev_id = kstrndup((const char *)dev, dev_len,
					GFP_KERNEL);
			if (!p->dev_id) {
				pr_warn("%s: deviceid string dup failed\n",
					__func__);
				kfree(p->con_id);
				continue;
			}
			index++;
		}
	}
	WARN_ON(count != index);
	if (count != index) {
		int i;

		for (i = 0; i < index; i++) {
			kfree(msm_clocks_mmi[i].con_id);
			kfree(msm_clocks_mmi[i].dev_id);
		}
		kfree(msm_clocks_mmi);
		count = 0;
		msm_clocks_mmi = NULL;
	}

out:
	of_node_put(parent);
	*size = count;
	return msm_clocks_mmi;
}
