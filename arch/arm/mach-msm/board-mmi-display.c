/* Copyright (c) 2011-2012, Motorola Mobility Inc. All rights reserved.
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

#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/ion.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-8960.h"
#include "board-mmi.h"

#define HDMI_PANEL_NAME	"hdmi_msm"

static int  mmi_feature_hdmi = 1;
static char panel_name[PANEL_NAME_MAX_LEN + 1];

static int __init mot_parse_atag_display(const struct tag *tag)
{
	const struct tag_display *display_tag = &tag->u.display;
	strlcpy(panel_name, display_tag->display, PANEL_NAME_MAX_LEN);
	panel_name[PANEL_NAME_MAX_LEN] = '\0';
	pr_info("%s: %s\n", __func__, panel_name);
	return 0;
}
__tagtable(ATAG_DISPLAY, mot_parse_atag_display);

#define DISP_MAX_REGS       5
#define DISP_MAX_RESET      2

#define ZERO_IF_NEG(val) ((val) < (0) ? (0) : (val))
#define NEG_1_IF_NEG(val) ((val) < (0) ? (-1) : (val))

enum mmi_disp_gpio_type {
	MMI_DISP_MSM_GPIO,
	MMI_DISP_PM_GPIO,
	MMI_DISP_INVAL_GPIO,
};

/* this must be matched with the device_tree setting for disp_intf */
enum mmi_disp_intf_type {
	MMI_DISP_MIPI_DSI_CM = 4,
	MMI_DISP_MIPI_DSI_VM = 6,
};

struct mmi_disp_gpio_config {
	char gpio_name[PANEL_NAME_MAX_LEN];
	int num;
	enum mmi_disp_gpio_type type;
	int handle;

	u8 init_val;

	u8 en_val;
	int en_pre_delay; /* Delay time for enable gpio */
	int en_post_delay;

	int dis_pre_delay; /* Delay time for disable gpio */
	int dis_post_delay;
};

struct mmi_disp_reg {
	char reg_id[PANEL_NAME_MAX_LEN];
	char reg_name[PANEL_NAME_MAX_LEN];
	struct regulator *handle;
	bool enabled;
	int min_uV;
	int max_uV;
	int reg_delay;

	u8 shared_gpio_en;
	struct mmi_disp_gpio_config en_gpio;
};

struct mmi_disp_reg_lst {
	int num_disp_regs;
	struct mmi_disp_reg disp_reg[DISP_MAX_REGS];
};

struct mmi_disp_data {
	enum mmi_disp_intf_type disp_intf;
	int num_disp_reset;
	struct mmi_disp_gpio_config disp_gpio[DISP_MAX_RESET];

	struct mmi_disp_reg_lst reg_lst;
};

static struct mmi_disp_data mmi_disp_info;
static struct mmi_disp_reg_lst dsi_regs_lst = {
	.num_disp_regs = 2,
	.disp_reg[0] = {
		.reg_id = "dsi_vddio",
		.reg_name = "reg_l23",
		.min_uV = 1800000,
		.max_uV = 1800000,
		.en_gpio = {
			.num = -1,
		},
	},
	.disp_reg[1] = {
		.reg_id = "dsi_vdda",
		.reg_name = "reg_l2",
		.min_uV = 1200000,
		.max_uV = 1200000,
		.en_gpio = {
			.num = -1,
		},
	},
};

static int panel_power_ctrl_en(int on);
static int panel_regs_gpio_init(struct mmi_disp_reg_lst *reg_lst);
static int power_rail_on(struct mmi_disp_reg_lst *reg_lst);
static int power_rail_off(struct mmi_disp_reg_lst *reg_lst);

static int mipi_dsi_power(int on)
{
	int rc;

	pr_debug("%s (%d) is called\n", __func__, on);

	if (!dsi_regs_lst.disp_reg[0].handle ||
					!dsi_regs_lst.disp_reg[1].handle) {
		/*
		 * This is a HACK for SOL smooth transtion: Because QCOM can
		 * not keep the MIPI lines at LP11 start when the kernel start
		 * so, when the first update, we need to reset the panel to
		 * raise the err detection from panel due to the MIPI lines
		 * drop.
		 * - With Video mode, this will be call mdp4_dsi_video_on()
		 *   before it disable the timing generator. This will prevent
		 *   the image to fade away.
		 * - Command mode, then we have to reset in the first call
		 *   to trun on the power
		 */
		if (mmi_disp_info.disp_intf == MMI_DISP_MIPI_DSI_CM)
			panel_power_ctrl_en(0);

		rc = panel_regs_gpio_init(&dsi_regs_lst);
		if (rc)
			goto end;
	}

	if (on) {
		rc = power_rail_on(&dsi_regs_lst);
		mdelay(10);
	} else
		rc = power_rail_off(&dsi_regs_lst);

	if (rc)
		goto end;
#ifdef MIPI_MOT_PANEL_PLATF_BRINGUP
	panel_power_ctrl(on);
#endif
end:
	return rc;
}


static int __init load_disp_value(struct device_node *node, char *name)
{
	const void *prop;

	prop = of_get_property(node, name, NULL);
	if (!prop)
		return -EINVAL;

	pr_debug("%s: read from devtree %s = %d\n",
						__func__, name, *(u32 *)prop);
	return *(u32 *)prop;
}


static int __init load_disp_string(struct device_node *node, char *name,
								char *disp_str)
{
	const void *prop;
	int len;

	prop = of_get_property(node, name, &len);
	if (!prop)
		return -EINVAL;

	strlcpy(disp_str, (const char *)prop, len);

	pr_debug("%s: read from devtree %s=%s\n", __func__, name, disp_str);
	return 0;
}

static void __init print_mmi_gpio_info(struct mmi_disp_gpio_config *gpio_info)
{
	pr_debug("-- %s ----\n", gpio_info->gpio_name);
	pr_debug("gpio_num        = %d\n", gpio_info->num);
	pr_debug("gpio_num_type   = %d\n", gpio_info->type);
	pr_debug("init_val        = %d\n", gpio_info->init_val);
	pr_debug("en_val          = %d\n", gpio_info->en_val);
	pr_debug("en_pre_delay    = %d\n", gpio_info->en_pre_delay);
	pr_debug("en_post_delay   = %d\n", gpio_info->en_post_delay);
	pr_debug("dis_pre_delay   = %d\n", gpio_info->dis_pre_delay);
	pr_debug("dis_post_delay  = %d\n", gpio_info->dis_post_delay);
}

static void __init print_mmi_reg_info(struct mmi_disp_reg *reg_info)
{
	struct mmi_disp_gpio_config *en_gpio;

	en_gpio = &reg_info->en_gpio;

	pr_debug("-------- %s ----\n", reg_info->reg_name);
	pr_debug("reg_id                = %s\n", reg_info->reg_id);
	pr_debug("min_uV                = %d\n", reg_info->min_uV);
	pr_debug("max_uV                = %d\n", reg_info->max_uV);
	pr_debug("reg_delay             = %d\n", reg_info->reg_delay);
	pr_debug("gpio_num              = %d\n", en_gpio->num);
	pr_debug("shared_gpio_en        = %d\n", reg_info->shared_gpio_en);
	pr_debug("gpio_name             = %s\n", en_gpio->gpio_name);
	pr_debug("gpio_type             = %d\n", en_gpio->type);
	pr_debug("gpio_init_val         = %d\n", en_gpio->init_val);
	pr_debug("gpio_en_val           = %d\n", en_gpio->en_val);
	pr_debug("gpio_en_pre_delay     = %d\n", en_gpio->en_pre_delay);
	pr_debug("gpio_en_en_post_delay = %d\n", en_gpio->en_post_delay);
	pr_debug("gpio_dis_pre_delay    = %d\n", en_gpio->dis_pre_delay);
	pr_debug("gpio_dis_post_delay   = %d\n", en_gpio->dis_post_delay);
}

static void __init print_mmi_disp_data(void)
{
	int i;
	struct mmi_disp_reg_lst *reg_lst = &mmi_disp_info.reg_lst;

	pr_debug("------------------------------------------\n");
	pr_debug("--  panel_name = %s\n", panel_name);
	pr_debug("--  Panel DSI interface = %d\n", mmi_disp_info.disp_intf);
	pr_debug("--  num_disp_reset  = %d\n", mmi_disp_info.num_disp_reset);
	pr_debug("------------------------------------------\n");
	for (i = 0; i < mmi_disp_info.num_disp_reset; i++)
		print_mmi_gpio_info(&mmi_disp_info.disp_gpio[i]);

	pr_debug("-------------------------------------------\n");
	pr_debug("  num_disp_regs = %d\n", reg_lst->num_disp_regs);
	pr_debug("-------------------------------------------\n");
	for (i = 0; i < reg_lst->num_disp_regs; i++)
		print_mmi_reg_info(&reg_lst->disp_reg[i]);
}

static void __init load_disp_reset_info_from_dt(struct device_node *node)
{
	struct mmi_disp_gpio_config *gpio_info;
	int i;
	char prop_name[PANEL_NAME_MAX_LEN];
	int len = PANEL_NAME_MAX_LEN;

	mmi_disp_info.num_disp_reset = load_disp_value(node, "num_disp_rst");
	if (mmi_disp_info.num_disp_reset == -EINVAL)
		mmi_disp_info.num_disp_reset = 1;
	else if (mmi_disp_info.num_disp_reset > DISP_MAX_RESET) {
		pr_err("%s: num_disp_reset=% is more than DISP_MAX_RESET=%d\n",
			__func__, mmi_disp_info.num_disp_reset, DISP_MAX_RESET);

		mmi_disp_info.num_disp_reset = DISP_MAX_RESET;
	}

	for (i = 1; i <= mmi_disp_info.num_disp_reset; i++) {
		gpio_info = &mmi_disp_info.disp_gpio[i-1];

		snprintf(gpio_info->gpio_name, len, "rst_gpio_%d", i);

		snprintf(prop_name, len, "rst_gpio_%d", i);
		gpio_info->num = load_disp_value(node, prop_name);

		snprintf(prop_name, len, "rst_gpio_type_%d", i);
		gpio_info->type = load_disp_value(node, prop_name);
		if (gpio_info->type < 0)
			gpio_info->type = MMI_DISP_INVAL_GPIO;

		snprintf(prop_name, len, "rst_init_state_%d", i);
		gpio_info->init_val = load_disp_value(node, prop_name);

		snprintf(prop_name, len, "rst_en_state_%d", i);
		gpio_info->en_val = load_disp_value(node, prop_name);

		snprintf(prop_name, len, "rst_en_pre_delay_%d", i);
		gpio_info->en_pre_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "rst_en_post_delay_%d", i);
		gpio_info->en_post_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "rst_dis_pre_delay_%d", i);
		gpio_info->dis_pre_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "rst_dis_post_delay_%d", i);
		gpio_info->dis_post_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));
	}
}

static void __init load_disp_regs_info_from_dt(struct device_node *node)
{
	struct mmi_disp_reg *reg_info;
	struct mmi_disp_gpio_config *en_gpio;
	int i;
	struct mmi_disp_reg_lst *reg_lst;
	char prop_name[PANEL_NAME_MAX_LEN];
	int len = PANEL_NAME_MAX_LEN;

	reg_lst = &mmi_disp_info.reg_lst;
	reg_lst->num_disp_regs = load_disp_value(node, "num_disp_reg");
	if (reg_lst->num_disp_regs == -EINVAL)
		reg_lst->num_disp_regs = 1;
	else if (reg_lst->num_disp_regs > DISP_MAX_REGS) {
		pr_err("%s: num_disp_reg=% is more than DISP_MAX_REGS=%d\n",
			__func__, reg_lst->num_disp_regs, DISP_MAX_REGS);

		reg_lst->num_disp_regs = DISP_MAX_REGS;
	}

	for (i = 1; i <= reg_lst->num_disp_regs; i++) {
		reg_info = &reg_lst->disp_reg[i-1];

		snprintf(prop_name, len, "reg_id_%d", i);
		load_disp_string(node, prop_name, reg_info->reg_id);

		snprintf(prop_name, len, "reg_name_%d", i);
		load_disp_string(node, prop_name, reg_info->reg_name);

		snprintf(prop_name, len, "reg_min_mv_%d", i);
		reg_info->min_uV =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_max_mv_%d", i);
		reg_info->max_uV =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_delay_%d", i);
		reg_info->reg_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		en_gpio = &reg_info->en_gpio;
		snprintf(prop_name, len, "reg_gpio_en_%d", i);
		en_gpio->num =
				NEG_1_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_shared_gpio_en_%d", i);
		reg_info->shared_gpio_en =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_gpio_name_%d", i);
		load_disp_string(node, prop_name, en_gpio->gpio_name);

		snprintf(prop_name, len, "reg_gpio_en_type_%d", i);
		en_gpio->type = load_disp_value(node, prop_name);
		if (en_gpio->type < 0)
			en_gpio->type = MMI_DISP_INVAL_GPIO;

		snprintf(prop_name, len, "reg_gpio_en_init_state_%d", i);
		en_gpio->init_val =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_gpio_en_state_%d", i);
		en_gpio->en_val =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_gpio_en_delay_%d", i);
		en_gpio->en_pre_delay = 0;
		en_gpio->en_post_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));

		snprintf(prop_name, len, "reg_gpio_dis_delay_%d", i);
		en_gpio->dis_pre_delay = 0;
		en_gpio->dis_post_delay =
				ZERO_IF_NEG(load_disp_value(node, prop_name));
	}
}

static void __init mmi_load_panel_from_dt(void)
{
	struct device_node *node;
	const char *name;
	const void *prop;

	node = of_find_node_by_path("/System@0/Display@0");

	if (!node)
		return;

	if (!of_property_read_string(node, "panel_name", &name)) {
		strlcpy(panel_name, name, PANEL_NAME_MAX_LEN);
		pr_info("%s: panel: %s\n", __func__, panel_name);
	} else
		pr_info("%s: No panel_name in devtree\n", __func__);

	prop = of_get_property(node, "disp_intf", NULL);
	if (!prop) {
		pr_err("%s: no panel interface setting\n", __func__);
		mmi_disp_info.disp_intf = 0;
	} else
		mmi_disp_info.disp_intf =  *(u32 *)prop;

	load_disp_reset_info_from_dt(node);
	load_disp_regs_info_from_dt(node);

	print_mmi_disp_data();

	of_node_put(node);
}

static int panel_gpio_init(struct mmi_disp_gpio_config *en)
{
	int rc;

	if (en->type == MMI_DISP_PM_GPIO) {
		en->handle = PM8921_GPIO_PM_TO_SYS(en->num);
		pr_debug("%s get handle for GPIO_PM(%d)\n", __func__, en->num);
	} else if (en->type == MMI_DISP_MSM_GPIO) {
		pr_debug("%s get handle for GPIO_MSM(%d)\n", __func__, en->num);
		en->handle = en->num;
	} else {
		pr_err("%s Invalid GPIO type=%d for num=%d\n", __func__,
							en->type, en->num);
		en->handle = -1;
		return -EINVAL;
	}

	rc = gpio_request(en->handle, en->gpio_name);
	if (rc) {
		pr_err("%s: failed to req for GPIO=%d\n", __func__, en->num);
		rc = -ENODEV;
		goto end;
	} else
		pr_debug("%s: called gpio_request(%d)\n", __func__, en->num);

	rc = gpio_direction_output(en->handle, en->init_val);
	if (rc) {
		pr_err("%s: failed to set output GPIO(%d)\n",
							__func__, en->num);
		rc = -ENODEV;
		goto end;
	} else
		pr_debug("%s: called set_output GPIO(%d) with init_val=%d\n",
					__func__, en->num, en->init_val);
end:
	return rc;
}

static void panel_delay_time(int delay_msec)
{
	unsigned long delay_usec = (unsigned long) delay_msec * 1000;
	pr_debug("%s: wait for %d msec\n", __func__, delay_msec);
	if (delay_msec > 20)
		usleep_range(delay_usec,
			delay_usec + (2 * USEC_PER_MSEC));
	else
		usleep_range(delay_usec, delay_usec);
}

static int panel_gpio_enable(struct mmi_disp_gpio_config *en, int enable)
{
	int rc = 0, val;
	int pre_delay, post_delay;

	if (enable) {
		val = en->en_val;
		pre_delay = en->en_pre_delay;
		post_delay = en->en_post_delay;
	} else {
		val = (en->en_val == 0) ? 1 : 0;
		pre_delay = en->dis_pre_delay;
		post_delay = en->dis_post_delay;
	}

	if (pre_delay > 0)
		panel_delay_time(pre_delay);

	gpio_set_value_cansleep(en->handle, val);

	pr_debug("%s: set GPIO(%d) = %d.\n", __func__, en->num, val);

	if (post_delay > 0)
		panel_delay_time(post_delay);

	return rc;
}

static int panel_regs_gpio_init(struct mmi_disp_reg_lst *reg_lst)
{
	int i, rc = 0;
	struct mmi_disp_reg *reg;
	struct mmi_disp_gpio_config *en;

	for (i = 0; i < reg_lst->num_disp_regs; i++) {
		reg = &reg_lst->disp_reg[i];
		en = &reg->en_gpio;

		if (reg->reg_id[0] != '\0') {
			pr_debug("%s: Init .. reg[%d]\n", __func__, i+1);
			reg->handle = regulator_get(&msm_mipi_dsi1_device.dev,
								reg->reg_id);
			if (IS_ERR(reg->handle)) {
				pr_err("%s: could not get 8921_%s, rc = %ld\n",
						__func__, reg->reg_id,
						PTR_ERR(reg->handle));
				rc = -ENODEV;
				goto end;
			} else
				pr_debug("%s: called regulator_get(%s)\n",
							__func__, reg->reg_id);

			if (reg->min_uV && reg->max_uV) {
				rc = regulator_set_voltage(reg->handle,
						reg->min_uV, reg->max_uV);
				if (rc) {
					pr_err("%s: failed to set volt for " \
						"%s(%d,%d)\n",
						__func__, reg->reg_id,
						reg->min_uV, reg->max_uV);
					rc = -EINVAL;
					goto end;
				} else
					pr_debug("%s: set volt for %s(%d,%d)\n",
						__func__, reg->reg_id,
						reg->min_uV, reg->max_uV);
			}
		} else
			pr_debug("%s: doesn't need to Init .. reg[%d]\n",
								__func__, i+1);

		if (en->num >= 0) {
			rc = panel_gpio_init(en);
			if (rc)
				goto end;
		}
	}
end:
	return rc;
}


static int panel_reset_enable(int enable)
{
	int i, rc = 0;
	struct mmi_disp_gpio_config *en;
	static bool reset_init;

	pr_debug("%s(%d) is called\n", __func__, enable);
	if (!reset_init) {
		for (i = 0; i < mmi_disp_info.num_disp_reset; i++) {
			en = &mmi_disp_info.disp_gpio[i];
			if (en->num >= 0) {
				rc = panel_gpio_init(en);
				if (rc)
					goto end;
			}
		}

		reset_init = true;
	}

	for (i = 0; i < mmi_disp_info.num_disp_reset; i++) {
		en = &mmi_disp_info.disp_gpio[i];
		if (en->num >= 0) {
			rc = panel_gpio_enable(en, enable);
			if (rc)
				goto end;
		}
	}
end:
	return rc;
}

static int panel_reg_enable(struct mmi_disp_reg *reg, int enable)
{
	int rc = 0;

	pr_debug("%s(%d) is called for %s\n", __func__, enable, reg->reg_name);
	if (!IS_ERR_OR_NULL(reg->handle)) {
		if (enable && reg->enabled == false) {
			rc = regulator_enable(reg->handle);
			if (rc) {
				pr_err("%s: failed to reg enable for %s\n",
						__func__, reg->reg_id);
				rc = -ENODEV;
			} else {
				reg->enabled = true;
				pr_debug("%s: called reg enable for %s\n",
						__func__, reg->reg_id);
			}
		} else if (!enable && reg->enabled == true) {
			rc = regulator_disable(reg->handle);
			if (rc) {
				pr_err("%s: failed to reg disable for %s\n",
						__func__, reg->reg_id);
				rc = -ENODEV;
			} else {
				reg->enabled = false;
				pr_debug("%s: called reg disable for %s\n",
						__func__, reg->reg_id);
			}
		}
	} else {
		pr_err("%s: invalid handle for reg_name=%s\n", __func__,
							reg->reg_id);
		rc = -EINVAL;
	}

	return rc;
}

static int panel_power_output(int on, struct mmi_disp_reg_lst *reg_lst)
{
	int rc = 0, start, end;
	struct mmi_disp_reg *reg;
	struct mmi_disp_gpio_config *en;

	pr_debug("%s(%d) is called\n", __func__, on);

	/*
	 * If request to disable the power then we need to put reset lines
	 * into the disable state first
	 */
	if (!on) {
		/* If this is disable then it disables in reversed order */
		start = reg_lst->num_disp_regs - 1;
		end = 0;
	} else {
		/* If this is enable then it able in normal  order */
		start = 0;
		end = reg_lst->num_disp_regs;
	}

	while (1) {
		reg = &reg_lst->disp_reg[start];
		en = &reg->en_gpio;

		/*
		* If reg has NO enable gpio line, then it can be enable or
		* disable at this moment
		*/
		if (en->num != -1) {
			/*
			 * If reg has enable gpio line, then the gpio_en can be
			 * enable or disable here
			 */
			rc = panel_gpio_enable(en, on);
			if (rc)
				goto end;

		} else if (!IS_ERR_OR_NULL(reg->handle) && (en->num == -1) &&
						(reg->shared_gpio_en == 0)) {
			/*
			 * If there is a reg that doesn't have gpio
			 * enable then we need to enable/disable at
			 * this point
			 */

			rc = panel_reg_enable(reg, on);
			if (rc)
				goto end;

			/* If the reg has the gpio_en line then the time delay
			 * en_pre_delay, dis_pre_delay, etc, will be used.
			 * if the reg doesn't have the gpio_en line then this
			 * time delay, reg_delay will be used instead
			 */
			if (reg->reg_delay)
				panel_delay_time(reg->reg_delay);

		}

		if (on) {
			start++;
			if (start >= end)
				break;
		} else {
			if (start <= end)
				break;
			start--;
		}

	}

end:
	return rc;
}

static int panel_power_ctrl_en(int on)
{
	int rc;

	if (!on) {
		rc = panel_reset_enable(on);
		if (rc)
			goto end;
	}

	rc = panel_power_output(on, &mmi_disp_info.reg_lst);
	if (rc)
		goto end;

	if (on) {
		rc = panel_reset_enable(on);
		if (rc)
			goto end;
	}
end:
	return rc;
}

static int power_rail_on_stage(struct mmi_disp_reg_lst *reg_lst)
{
	int i, rc = 0;
	struct mmi_disp_reg *reg;
	struct mmi_disp_gpio_config *en;

	pr_debug("%s is called\n", __func__);

	for (i = 0; i < reg_lst->num_disp_regs; i++) {
		reg = &reg_lst->disp_reg[i];
		en = &reg->en_gpio;

		if (reg->reg_id[0] != '\0' && !IS_ERR_OR_NULL(reg->handle)) {
			rc = regulator_set_optimum_mode(reg->handle, 100000);
			if (rc < 0) {
				pr_err("%s: failed to set_optimum for %s." \
					"rc=%d\n", __func__, reg->reg_id, rc);
				rc = -EINVAL;
				goto end;
			} else {
				pr_debug("%s: set set_optimum for %s(100000)\n",
							__func__, reg->reg_id);
				rc = 0;
			}
		}

		/*
		 * If reg has enable gpio line, or it doesn't gpio en because
		 * it is shared with other reg then it can be enable now
		 */
		if (!IS_ERR_OR_NULL(reg->handle)) {
			if ((en->num >= 0) || (reg->shared_gpio_en == 1)) {
				rc = panel_reg_enable(reg, 1);
				if (rc)
					goto end;
			}
		}
	}
end:
	return rc;
}


static int power_rail_on(struct mmi_disp_reg_lst *reg_lst)
{
	int rc;

	pr_debug("%s is called\n", __func__);

	rc = power_rail_on_stage(reg_lst);
	if (rc)
		goto end;

	rc = panel_power_output(1, reg_lst);
end:
	return rc;
}

static int power_rail_off_unstage(struct mmi_disp_reg_lst *reg_lst)
{
	int i, rc = 0;
	struct mmi_disp_reg *reg;
	struct mmi_disp_gpio_config *en;

	pr_debug("%s is called\n", __func__);

	/* this is disable then it disables in reversed order */
	for (i = reg_lst->num_disp_regs - 1; i >= 0; i--) {
		reg = &reg_lst->disp_reg[i];
		en = &reg->en_gpio;

		/* If reg has en gpio line, then it can be disable now */
		if (!IS_ERR_OR_NULL(reg->handle)) {
			if ((en->num >= 0) || (reg->shared_gpio_en == 1)) {
				rc = panel_reg_enable(reg, 0);
				if (rc)
					goto end;
			}
		}

		if (reg->reg_id[0] != '\0' && !IS_ERR_OR_NULL(reg->handle)) {
			rc = regulator_set_optimum_mode(reg->handle, 100);
			if (rc < 0) {
				pr_err("%s: failed to set_optimum for %s." \
					"rc=%d\n", __func__, reg->reg_id, rc);
				rc = -EINVAL;
				goto end;
			} else {
				pr_debug("%s: set set_optimum for %s(100)\n",
							__func__, reg->reg_id);
				rc = 0;
			}
		}
	}
end:
	return rc;
}


static int power_rail_off(struct mmi_disp_reg_lst *reg_lst)
{
	int rc;

	pr_debug("%s is called\n", __func__);

	rc = panel_power_output(0, reg_lst);
	if (rc)
		goto end;

	rc = power_rail_off_unstage(reg_lst);
	if (rc)
		goto end;
end:
	return rc;
}

static int panel_power_ctrl(int on)
{
	static bool panel_gpio_init;
	int rc;

	pr_debug("%s (%d) is called\n", __func__, on);

	if (!panel_gpio_init) {
		rc = panel_regs_gpio_init(&mmi_disp_info.reg_lst);
		if (rc)
			goto end;

		panel_gpio_init = true;
	}

	if (on) {
		rc = power_rail_on(&mmi_disp_info.reg_lst);
		if (rc)
			goto end;

		rc = panel_reset_enable(on);
		if (rc)
			goto end;
	} else {
		rc = panel_reset_enable(on);
		if (rc)
			goto end;

		rc = power_rail_off(&mmi_disp_info.reg_lst);
		if (rc)
			goto end;
	}
end:
	return rc;
}

static int msm_fb_detect_panel(const char *name)
{
	if (!strncmp(name, panel_name, PANEL_NAME_MAX_LEN)) {
		pr_info("%s: detected %s\n", __func__, name);
		return 0;
	}

	if (mmi_feature_hdmi && !strncmp(name, HDMI_PANEL_NAME,
				strnlen(HDMI_PANEL_NAME, PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
}


static struct mipi_dsi_panel_platform_data mipi_dsi_mot_pdata = {
};

static struct platform_device mipi_dsi_mot_panel_device = {
	.name = "mipi_dsi_mot_panel",
	.id = 0,
	.dev = {
		.platform_data = &mipi_dsi_mot_pdata,
	}
};

void __init mmi_display_init(struct msm_fb_platform_data *msm_fb_pdata,
				struct mipi_dsi_platform_data *mipi_dsi_pdata)
{
	mmi_load_panel_from_dt();
	msm_fb_pdata->detect_client = msm_fb_detect_panel;
	mipi_dsi_pdata->vsync_gpio = 0;
	mipi_dsi_pdata->dsi_power_save = mipi_dsi_power;
	mipi_dsi_pdata->panel_power_save = panel_power_ctrl;
	mipi_dsi_pdata->panel_power_en = panel_power_ctrl_en;
	platform_device_register(&mipi_dsi_mot_panel_device);
}
