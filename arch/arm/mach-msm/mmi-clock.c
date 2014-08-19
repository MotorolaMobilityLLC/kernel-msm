#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <mach/clk-provider.h>
#include "clock.h"
#include "clock-local2.h"
#include "mmi-clock.h"

static struct clk_lookup mmi_msm_clocks_8226[] = {
	/* Camera Clocks */
	CLK_LOOKUP("cam_src_clk", mclk1_clk_src.c, "32.qcom,camera"),
	CLK_LOOKUP("cam_clk", camss_mclk1_clk.c, "32.qcom,camera"),
};

#ifdef CONFIG_OF
static void of_override_clk(u32 *enable)
{
	struct device_node *chosen_node = NULL;

	chosen_node = of_find_node_by_name(NULL, "chosen");
	if (!chosen_node) {
		pr_err("%s: get chosen node failed\n", __func__);
		return;
	}

	of_property_read_u32(chosen_node, "mmi,clock_override", enable);
	of_node_put(chosen_node);
}
#else
static inline void of_override_clk(u32 *enable) { }
#endif

static u32 override_clk(void)
{
	u32 enable = 0;

	of_override_clk(&enable);
	return enable;
}

int mmi_msm_clock_init(struct clock_init_data *data)
{
	struct clk_lookup *clock_tbl;
	struct clk_lookup *ptr;
	size_t num_clocks;

	if (override_clk()) {
		num_clocks = data->size + ARRAY_SIZE(mmi_msm_clocks_8226);
		clock_tbl = kmalloc(sizeof(struct clk_lookup)*num_clocks,
				GFP_KERNEL);
		if (!clock_tbl) {
			pr_err("%s: allocate clk_lookup failed\n", __func__);
			pr_err("%s: All Peripheral may not work\n", __func__);
			return -ENOMEM;
		}
		ptr = clock_tbl;

		/* Prepend MMI Clock entries to override msm clock setting */
		memcpy(ptr, mmi_msm_clocks_8226,
			sizeof(struct clk_lookup) *
				ARRAY_SIZE(mmi_msm_clocks_8226));
		ptr += ARRAY_SIZE(mmi_msm_clocks_8226);

		memcpy(ptr, data->table, sizeof(struct clk_lookup)*data->size);
		data->table = clock_tbl;
		data->size = num_clocks;
	}

	return 0;
}
