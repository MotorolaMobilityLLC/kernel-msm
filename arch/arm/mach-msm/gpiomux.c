/* Copyright (c) 2010,2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>

#if DUMP_GPIOMUX
	#define cfg_out		pr_info
#else
	#define cfg_out		pr_debug
#endif

struct msm_gpiomux_rec {
	struct gpiomux_setting *sets[GPIOMUX_NSETTINGS];
	int ref;
#if DUMP_GPIOMUX
	int wrcount;
#endif
};
static DEFINE_SPINLOCK(gpiomux_lock);
static struct msm_gpiomux_rec *msm_gpiomux_recs;
static struct gpiomux_setting *msm_gpiomux_sets;
static unsigned msm_gpiomux_ngpio;

static int msm_gpiomux_store(unsigned gpio, enum msm_gpiomux_setting which,
	struct gpiomux_setting *setting, struct gpiomux_setting *old_setting)
{
	struct msm_gpiomux_rec *rec = msm_gpiomux_recs + gpio;
	unsigned set_slot = gpio * GPIOMUX_NSETTINGS + which;
	unsigned long irq_flags;
	int status = 0;

	if (!msm_gpiomux_recs)
		return -EFAULT;

	if (gpio >= msm_gpiomux_ngpio)
		return -EINVAL;

	spin_lock_irqsave(&gpiomux_lock, irq_flags);

	if (old_setting) {
		if (rec->sets[which] == NULL)
			status = 1;
		else
			*old_setting =  *(rec->sets[which]);
	}

	if (setting) {
		msm_gpiomux_sets[set_slot] = *setting;
		rec->sets[which] = &msm_gpiomux_sets[set_slot];
	} else {
		rec->sets[which] = NULL;
	}

	spin_unlock_irqrestore(&gpiomux_lock, irq_flags);
	return status;
}

int msm_gpiomux_write(unsigned gpio, enum msm_gpiomux_setting which,
	struct gpiomux_setting *setting, struct gpiomux_setting *old_setting)
{
	int ret;
	unsigned long irq_flags;
	struct gpiomux_setting *new_set;
	struct msm_gpiomux_rec *rec = msm_gpiomux_recs + gpio;

	ret = msm_gpiomux_store(gpio, which, setting, old_setting);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&gpiomux_lock, irq_flags);

#if DUMP_GPIOMUX
	rec->wrcount++;
#endif
	new_set = rec->ref ? rec->sets[GPIOMUX_ACTIVE] :
		rec->sets[GPIOMUX_SUSPENDED];
	if (new_set)
		__msm_gpiomux_write(gpio, *new_set);

	spin_unlock_irqrestore(&gpiomux_lock, irq_flags);
	return ret;
}
EXPORT_SYMBOL(msm_gpiomux_write);

int msm_gpiomux_get(unsigned gpio)
{
	struct msm_gpiomux_rec *rec = msm_gpiomux_recs + gpio;
	unsigned long irq_flags;

	if (!msm_gpiomux_recs)
		return -EFAULT;

	if (gpio >= msm_gpiomux_ngpio)
		return -EINVAL;

	spin_lock_irqsave(&gpiomux_lock, irq_flags);
	if (rec->ref++ == 0 && rec->sets[GPIOMUX_ACTIVE])
		__msm_gpiomux_write(gpio, *rec->sets[GPIOMUX_ACTIVE]);
	spin_unlock_irqrestore(&gpiomux_lock, irq_flags);
	return 0;
}
EXPORT_SYMBOL(msm_gpiomux_get);

int msm_gpiomux_put(unsigned gpio)
{
	struct msm_gpiomux_rec *rec = msm_gpiomux_recs + gpio;
	unsigned long irq_flags;

	if (!msm_gpiomux_recs)
		return -EFAULT;

	if (gpio >= msm_gpiomux_ngpio)
		return -EINVAL;

	spin_lock_irqsave(&gpiomux_lock, irq_flags);
	BUG_ON(rec->ref == 0);
	if (--rec->ref == 0 && rec->sets[GPIOMUX_SUSPENDED])
		__msm_gpiomux_write(gpio, *rec->sets[GPIOMUX_SUSPENDED]);
	spin_unlock_irqrestore(&gpiomux_lock, irq_flags);
	return 0;
}
EXPORT_SYMBOL(msm_gpiomux_put);

int msm_tlmm_misc_reg_read(enum msm_tlmm_misc_reg misc_reg)
{
	return readl_relaxed(MSM_TLMM_BASE + misc_reg);
}

void msm_tlmm_misc_reg_write(enum msm_tlmm_misc_reg misc_reg, int val)
{
	writel_relaxed(val, MSM_TLMM_BASE + misc_reg);
	/* ensure the write completes before returning */
	mb();
}

int msm_gpiomux_init(size_t ngpio)
{
	if (!ngpio)
		return -EINVAL;

	if (msm_gpiomux_recs)
		return -EPERM;

	msm_gpiomux_recs = kzalloc(sizeof(struct msm_gpiomux_rec) * ngpio,
				   GFP_KERNEL);
	if (!msm_gpiomux_recs)
		return -ENOMEM;

	/* There is no need to zero this memory, as clients will be blindly
	 * installing settings on top of it.
	 */
	msm_gpiomux_sets = kmalloc(sizeof(struct gpiomux_setting) * ngpio *
		GPIOMUX_NSETTINGS, GFP_KERNEL);
	if (!msm_gpiomux_sets) {
		kfree(msm_gpiomux_recs);
		msm_gpiomux_recs = NULL;
		return -ENOMEM;
	}

	msm_gpiomux_ngpio = ngpio;

	return 0;
}
EXPORT_SYMBOL(msm_gpiomux_init);

static int s2int(struct gpiomux_setting *s)
{
	return (s == NULL) ? 0xFFFF
		: ((s->func<<12)|(s->drv<<8)|(s->pull<<4)|(s->dir));
}

static char *s2str(struct gpiomux_setting *s, char *str, int len)
{
	static const char *dir[3] = { "IN", "OUT_HIGH", "OUT_LOW" };
	static const char *pull[4] =
		{ "PULL_NONE", "PULL_DOWN", "PULL_KEEPER", "PULL_UP" };
	if (s) {
		int r;
		if (s->func)
			r = snprintf(str, len, "FUNC_%X", s->func);
		else
			r = snprintf(str, len, "FUNC_GPIO");
		if (r < len) {
			snprintf(str+r, len-r, ", DRV_%dMA, %s, %s",
				 (s->drv+1)<<1, pull[s->pull], dir[s->dir]);
		}
	} else {
		snprintf(str, len, "Not Used");
	}
	return str;
}

static void pr_cfg(struct msm_gpiomux_config *c, bool nowrite)
{
	struct gpiomux_setting *s;
	char str[60];
	cfg_out("###\t\tgpio@%d {\n", c->gpio);
	s = c->settings[GPIOMUX_ACTIVE];
	if (s)
		cfg_out("###\t\t\tactive  = <0x%04X>; /* %s */\n",
			s2int(s), s2str(s, str, sizeof(str)));
	s = c->settings[GPIOMUX_SUSPENDED];
	if (s)
		cfg_out("###\t\t\tsuspend = <0x%04X>; /* %s */\n",
			s2int(s), s2str(s, str, sizeof(str)));
	if (nowrite)
		cfg_out("###\t\t\tinstall-no-write;\n");
	cfg_out("###\t\t};\n");
}

#if DUMP_GPIOMUX
void msm_gpiomux_dump(void)
{
	struct msm_gpiomux_config cfg;
	struct msm_gpiomux_rec *rec = msm_gpiomux_recs;
	cfg_out("--------- %s begin ---------\n", __func__);
	for (cfg.gpio = 0; cfg.gpio < msm_gpiomux_ngpio; cfg.gpio++, rec++) {
		if (!rec->sets[GPIOMUX_ACTIVE] &&
		    !rec->sets[GPIOMUX_SUSPENDED])
			continue;
		cfg.settings[GPIOMUX_ACTIVE] = rec->sets[GPIOMUX_ACTIVE];
		cfg.settings[GPIOMUX_SUSPENDED] = rec->sets[GPIOMUX_SUSPENDED];
		pr_cfg(&cfg, !rec->wrcount);
	}
	cfg_out("--------- %s end ---------\n", __func__);
}
EXPORT_SYMBOL(msm_gpiomux_dump);
#endif

void msm_gpiomux_install_nowrite(struct msm_gpiomux_config *configs,
				unsigned nconfigs)
{
	unsigned c, s;
	int rc;

	for (c = 0; c < nconfigs; ++c) {
		pr_cfg(&configs[c], true);
		for (s = 0; s < GPIOMUX_NSETTINGS; ++s) {
			rc = msm_gpiomux_store(configs[c].gpio, s,
				configs[c].settings[s], NULL);
			if (rc)
				pr_err("%s: write failure: %d\n", __func__, rc);
		}
	}
}
EXPORT_SYMBOL(msm_gpiomux_install_nowrite);

void msm_gpiomux_install(struct msm_gpiomux_config *configs, unsigned nconfigs)
{
	unsigned c, s;
	int rc;

	for (c = 0; c < nconfigs; ++c) {
		pr_cfg(&configs[c], false);
		for (s = 0; s < GPIOMUX_NSETTINGS; ++s) {
			rc = msm_gpiomux_write(configs[c].gpio, s,
				configs[c].settings[s], NULL);
			if (rc)
				pr_err("%s: write failure: %d\n", __func__, rc);
		}
	}
}
EXPORT_SYMBOL(msm_gpiomux_install);

#ifdef CONFIG_MSM_OF_GPIOMUX
static int msm_gpiomux_dt_get_settings(struct device_node *of_node,
	int id, struct msm_gpiomux_config *c, struct gpiomux_setting *s)
{
	const char *prop[] = {
		[GPIOMUX_ACTIVE] = "active",
		[GPIOMUX_SUSPENDED] = "suspend",
	};
	u32 data, err = 0;
	if (of_property_read_u32(of_node, prop[id], &data)) {
		c->settings[id] = NULL;
		return 0;
	}
	s += id;
	s->func = (data>>12) & 0xF;
	s->drv = (data>>8) & 0xF;
	if (s->drv > GPIOMUX_DRV_16MA)
		err |= 0x4;
	s->pull = (data>>4) & 0xF;
	if (s->pull > GPIOMUX_PULL_UP)
		err |= 0x2;
	s->dir = data & 0xF;
	if (s->dir > GPIOMUX_OUT_LOW)
		err |= 0x1;
	if (err) {
		pr_err("Invalid gpio@%d settings: %s = <0x%04X> err(0x%X)\n",
			c->gpio, prop[id], data, err);
		return -EINVAL;
	}
	c->settings[id] = s;
	return 0;
}

static int msm_gpiomux_dt_install(struct device_node *of_dev)
{
	const char *prop = "qcom,msm-gpiomux-install";
	struct gpiomux_setting settings[GPIOMUX_NSETTINGS];
	struct msm_gpiomux_config config;
	struct device_node *of_node;
	of_dev = of_get_child_by_name(of_dev, prop);
	if (!of_dev) {
		pr_err("%s: not entry %s\n", __func__, prop);
		return -EINVAL;
	}
	cfg_out("%s: processing %s\n", __func__, prop);
	for_each_child_of_node(of_dev, of_node) {
		unsigned long gpio;
		const char *name = strrchr(of_node_full_name(of_node), '/');
		if (name)
			name++;
		if (!name || strnicmp(name, "gpio@", 5)) {
err_node_name:
			pr_err("Invalid gpiomux node %s\n", name);
			continue;
		}
		if (kstrtoul(name+5, 10, &gpio))
			goto err_node_name;
		if (gpio >= msm_gpiomux_ngpio) {
			pr_err("%s is out of gpio range %d\n",
				name, msm_gpiomux_ngpio);
			continue;
		}
		config.gpio = gpio;
		if (msm_gpiomux_dt_get_settings(of_node, GPIOMUX_ACTIVE,
						&config, settings))
			continue;
		if (msm_gpiomux_dt_get_settings(of_node, GPIOMUX_SUSPENDED,
						&config, settings))
			continue;
		if (of_property_read_bool(of_node, "install-no-write"))
			msm_gpiomux_install_nowrite(&config, 1);
		else
			msm_gpiomux_install(&config, 1);
	}
	return 0;
}
#endif /* CONFIG_MSM_OF_GPIOMUX */

int msm_gpiomux_init_dt(void)
{
	int rc;
	unsigned int ngpio;
	struct device_node *of_gpio_node;

	of_gpio_node = of_find_compatible_node(NULL, NULL, "qcom,msm-gpio");
	if (!of_gpio_node) {
		pr_err("%s: Failed to find qcom,msm-gpio node\n", __func__);
		return -ENODEV;
	}

	rc = of_property_read_u32(of_gpio_node, "ngpio", &ngpio);
	if (rc) {
		pr_err("%s: Failed to find ngpio property in msm-gpio device node %d\n"
				, __func__, rc);
		return rc;
	}

	rc = msm_gpiomux_init(ngpio);
#ifdef CONFIG_MSM_OF_GPIOMUX
	if (!rc)
		rc = msm_gpiomux_dt_install(of_gpio_node);
#endif
	return rc;
}
EXPORT_SYMBOL(msm_gpiomux_init_dt);
