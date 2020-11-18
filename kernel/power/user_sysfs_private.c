/*
* Copyright (C) 2012 lenovo, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_wakeup.h>
#ifdef CONFIG_PINCTRL_MSM
#include "../../drivers/pinctrl/qcom/pinctrl-msm.h"
#endif
#include "user_sysfs_private.h"

#define GPIO_CFG(gpio, func, dir, pull, drvstr) \
	((((gpio) & 0x3FF) << 4)        |	  \
	 ((func) & 0xf)                  |	  \
	 (((dir) & 0x1) << 14)           |	  \
	 (((pull) & 0x3) << 15)          |	  \
	 (((drvstr) & 0xF) << 17))

/**
 * extract GPIO pin from bit-field used for gpio_tlmm_config
 */
#define GPIO_PIN(gpio_cfg)    (((gpio_cfg) >>  4) & 0x3ff)
#define GPIO_FUNC(gpio_cfg)   (((gpio_cfg) >>  0) & 0xf)
#define GPIO_DIR(gpio_cfg)    (((gpio_cfg) >> 14) & 0x1)
#define GPIO_PULL(gpio_cfg)   (((gpio_cfg) >> 15) & 0x3)
#define GPIO_DRVSTR(gpio_cfg) (((gpio_cfg) >> 17) & 0xf)

#define TLMM_NUM_GPIO 255

#define HAL_OUTPUT_VAL(config) (((config)&0x40000000)>>30)

static int before_sleep_fetched;
module_param(before_sleep_fetched, int, 0644);
static unsigned before_sleep_configs[TLMM_NUM_GPIO];

static int before_sleep_table_enabled;
module_param(before_sleep_table_enabled, int, 0644);
static unsigned before_sleep_table_configs[TLMM_NUM_GPIO];

#ifdef CONFIG_GPIO_VALID_ARRAY
static unsigned tz_configs[TLMM_NUM_GPIO];
#endif

#ifndef CONFIG_PINCTRL_MSM
int tlmm_get_inout(unsigned gpio)
{
	return 0;
}

int tlmm_set_inout(unsigned gpio, unsigned value)
{
	return 0;
}

int tlmm_get_config(unsigned gpio, unsigned *cfg)
{
	return -EINVAL;
}

int tlmm_set_config(unsigned config)
{
	return -EINVAL;
}
#endif

static int tlmm_dump_cfg(char *buf, int size,
	unsigned gpio, unsigned cfg, int output_val)
{
	/* mA */
	static const char * const drvstr_str[] = {
		"2", "4", "6", "8", "10", "12", "14", "16" };
	/* "NO_PULL", "PULL_DOWN", "KEEPER", "PULL_UP" */
	static const char * const pull_str[] = { "N", "D", "K", "U" };
	/* "Input", "Output" */
	static const char * const dir_str[] = { "I", "O" };
	char func_str[20];

	char *p = buf;

	int drvstr   = GPIO_DRVSTR(cfg);
	int pull     = GPIO_PULL(cfg);
	int dir      = GPIO_DIR(cfg);
	int func     = GPIO_FUNC(cfg);

	/* pr_debug("%s(), drvstr=%d, pull=%d, dir=%d, func=%d\n", */
	/*	__func__, drvstr, pull, dir, func); */
	snprintf(func_str, sizeof(func_str), "%d", func);

	p += snprintf(p, size - (p - buf), "%d: %s%s%s%s", gpio, func_str,
		pull_str[pull], dir_str[dir], drvstr_str[drvstr]);

	p += snprintf(p, size - (p - buf), " = %d", output_val);

	p += snprintf(p, size - (p - buf), "\n");

	return p - buf;
}

static int tlmm_dump_header(char *buf, int size)
{
	char *p = buf;

	p += snprintf(p, size - (p - buf),
		"0:GPIO\n");
	p += snprintf(p, size - (p - buf),
		"N:NO_PULL  D:PULL_DOWN  K:KEEPER  U:PULL_UP\n");
	p += snprintf(p, size - (p - buf),
		"I:Input  O:Output\n");
	p += snprintf(p, size - (p - buf),
		"2:2, 4, 6, 8, 10, 12, 14, 16 mA (driver strength)\n\n");
	return p - buf;
}

int tlmm_dump_info(char *buf, int size, int tlmm_num, int tlmm_count)
{
	unsigned i;
	char *p = buf;
	unsigned cfg;
	int output_val = 0;
	int ret;

	if (tlmm_num < 0 || tlmm_num >= TLMM_NUM_GPIO)
		return p - buf;

	if (tlmm_count == 1) {
#ifdef CONFIG_GPIO_VALID_ARRAY
		if (tz_configs[tlmm_num])
			return p - buf;
#endif
		ret = tlmm_get_config(tlmm_num, &cfg);
		if (ret == 0) {
			output_val = tlmm_get_inout(tlmm_num);

			p += tlmm_dump_cfg(p, size - (p - buf),
				tlmm_num, cfg, output_val);
		}
	} else {
		p += tlmm_dump_header(p, size - (p - buf));
		p += snprintf(p, size - (p - buf),
			"Format: gpio func pull dir strength [output]\n");
		p += snprintf(p, size - (p - buf),
			"Shortcut: gpio output\n");
		p += snprintf(p, size - (p - buf),
			"echo  20 0 D O 2 1, Config tlmm 20 and output 1\n");
		p += snprintf(p, size - (p - buf),
			"echo  20 1, Set tlmm 20 to output 1\n");

		for (i = tlmm_num; i < tlmm_num + tlmm_count; ++i) {
			if (i >= TLMM_NUM_GPIO)
				break;

#ifdef CONFIG_GPIO_VALID_ARRAY
			if (tz_configs[i])
				continue;
#endif
			ret = tlmm_get_config(i, &cfg);
			if (ret == 0) {
				output_val = tlmm_get_inout(i);

				p += tlmm_dump_cfg(p, size - (p - buf),
					i, cfg, output_val);
			}
		}
		pr_debug("%s(), %d\n", __func__, __LINE__);
		p += snprintf(p, size - (p - buf), "(%d)\n", (int)(p - buf));
	}
	return p - buf;
}

/* save tlmm config before sleep */
void tlmm_before_sleep_save_configs(void)
{
	unsigned i;
	int ret;

	/* only save tlmm configs when it has been fetched */
	if (!before_sleep_fetched)
		return;

	pr_debug("%s(), Save tlmms before suspend!\n", __func__);
	before_sleep_fetched = false;
	for (i = 0; i < TLMM_NUM_GPIO; ++i) {
		unsigned cfg;
		int output_val = 0;

#ifdef CONFIG_GPIO_VALID_ARRAY
		if (tz_configs[i])
			continue;
#endif
		ret = tlmm_get_config(i, &cfg);
		if (ret < 0)
			break;
		output_val = tlmm_get_inout(i);

		before_sleep_configs[i] = cfg | (output_val << 30);
	}
}

int tlmm_before_sleep_dump_info(char *buf, int size)
{
	unsigned i;
	char *p = buf;

	p += snprintf(p, size - (p - buf), "tlmm_before_sleep:\n");
	if (!before_sleep_fetched) {
		before_sleep_fetched = true;

		p += tlmm_dump_header(p, size - (p - buf));

		for (i = 0; i < TLMM_NUM_GPIO; ++i) {
			unsigned cfg;
			int output_val = 0;

#ifdef CONFIG_GPIO_VALID_ARRAY
			if (tz_configs[i])
				continue;
#endif
			cfg = before_sleep_configs[i];
			output_val = HAL_OUTPUT_VAL(cfg);
			/* cfg &= ~0x40000000; */
			p += tlmm_dump_cfg(p, size - (p - buf),
				i, cfg, output_val);
		}
		p += snprintf(p, size - (p - buf), "(%d)\n", (int)(p - buf));
	}
	return p - buf;
}

/* set tlmms config before sleep */
void tlmm_before_sleep_set_configs(void)
{
	int res;
	unsigned i;

	/* only set tlmms before sleep when it's enabled */
	if (!before_sleep_table_enabled)
		return;

	pr_debug("%s(), Set tlmms before suspend!\n", __func__);
	for (i = 0; i < TLMM_NUM_GPIO; ++i) {
		unsigned cfg;
		int gpio;
		int dir;
		int func;
		int output_val = 0;

#ifdef CONFIG_GPIO_VALID_ARRAY
		if (tz_configs[i])
			continue;
#endif
		cfg = before_sleep_table_configs[i];

		gpio = GPIO_PIN(cfg);
		if (gpio != i)
			continue;

		output_val = HAL_OUTPUT_VAL(cfg);
		dir = GPIO_DIR(cfg);
		func = GPIO_FUNC(cfg);

		pr_debug("%s(), [%d]: 0x%x\n", __func__, i, cfg);
		if (gpio < TLMM_NUM_GPIO)
			res = tlmm_set_config(cfg & ~0x40000000);
		if (res < 0) {
			pr_debug("Error: Config failed.\n");
			break;
		}

		if ((func == 0) && (dir == 1))
			tlmm_set_inout(i, output_val);
	}
}

#ifdef CONFIG_GPIO_VALID_ARRAY
int tlmm_tz_dump_info(char *buf, int size)
{
	unsigned i;
	char *p = buf;

	p += snprintf(p, size - (p - buf),
		"Format: gpio skip\n");
	p += snprintf(p, size - (p - buf),
		"Skip tlmms that locked by TZ.\n");

	for (i = 0; i < TLMM_NUM_GPIO; ++i) {
		p += snprintf(p, size - (p - buf), "%d: ", i);
		p += snprintf(p, size - (p - buf), "%d\n", tz_configs[i]);
	}
	p += snprintf(p, size - (p - buf), "(%d)\n", (int)(p - buf));
	return p - buf;
}
#endif

int tlmm_before_sleep_table_set_cfg(unsigned gpio, unsigned cfg)
{
	if (gpio >= TLMM_NUM_GPIO && gpio != 255 && gpio != 256) {
		pr_debug("gpio >= TLMM_NUM_GPIO && gpio != 255 && gpio != 256!\n");
		return -EINVAL;
	}

	if (gpio < TLMM_NUM_GPIO) {
		before_sleep_table_configs[gpio] = cfg;
		before_sleep_table_enabled = true;
	} else if (gpio == 255)
		before_sleep_table_enabled = true;
	else if (gpio == 256)
		before_sleep_table_enabled = false;

	return 0;
}

int tlmm_before_sleep_table_dump_info(char *buf, int size)
{
	unsigned i;
	char *p = buf;

	p += tlmm_dump_header(p, size - (p - buf));
	p += snprintf(p, size - (p - buf),
		"Format: gpio function pull direction strength [output]\n");
	p += snprintf(p, size - (p - buf),
		"Set tlmm before suspend.\n");
	p += snprintf(p, size - (p - buf),
		"echo 20 0 D O 2 1, tlmm 20 output 1\n");
	p += snprintf(p, size - (p - buf),
		"echo 20, Disable tlmm 20's setting\n");
	p += snprintf(p, size - (p - buf),
		"echo 255, Enable the setting\n");
	p += snprintf(p, size - (p - buf),
		"echo 256, Disable the setting\n");

	for (i = 0; i < TLMM_NUM_GPIO; ++i) {
		unsigned cfg;
		int output_val = 0;

#ifdef CONFIG_GPIO_VALID_ARRAY
		if (tz_configs[i])
			continue;
#endif
		cfg = before_sleep_table_configs[i];
		output_val = HAL_OUTPUT_VAL(cfg);
		/* cfg &= ~0x40000000; */
		p += tlmm_dump_cfg(p, size - (p - buf), i, cfg, output_val);
	}
	p += snprintf(p, size - (p - buf), "(%d)\n", (int)(p - buf));
	return p - buf;
}

#define private_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

static int tlmm_num;
static ssize_t tlmm_num_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += snprintf(p, PAGE_SIZE, "Start gpio to check by cat tlmm\n");
	p += snprintf(p, PAGE_SIZE - (p - buf), "0~%d\n", TLMM_NUM_GPIO - 1);
	p += snprintf(p, PAGE_SIZE - (p - buf), "%d\n", tlmm_num);
	return p - buf;
}

static ssize_t tlmm_num_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	int gpio;
	int res;

	res = kstrtoint(buf, 10, &gpio);
	pr_debug("res=%d. %d\n", res, gpio);

	if (res != 0)
		goto tlmm_num_store_wrong_para;

	if (gpio >= TLMM_NUM_GPIO)
		goto tlmm_num_store_wrong_para;

	tlmm_num = gpio;
	pr_debug("tlmm_num: %d\n", tlmm_num);

	goto tlmm_num_store_ok;

tlmm_num_store_wrong_para:
	pr_debug("Wrong Input.\n");
	pr_debug("Format: tlmm_num\n");
	pr_debug("      tlmm_num: 0~%d\n", TLMM_NUM_GPIO - 1);

tlmm_num_store_ok:
	return n;
}
static int tlmm_count = TLMM_NUM_GPIO;
static ssize_t tlmm_count_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += snprintf(p, PAGE_SIZE, "Number of gpios to check by cat tlmm\n");
	p += snprintf(p, PAGE_SIZE - (p - buf), "0~%d\n", TLMM_NUM_GPIO);
	p += snprintf(p, PAGE_SIZE - (p - buf), "%d\n", tlmm_count);
	return p - buf;
}

static ssize_t tlmm_count_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	int gpio;
	int res;

	res = kstrtoint(buf, 10, &gpio);
	pr_debug("res=%d. %d\n", res, gpio);

	if (res != 0)
		goto tlmm_count_store_wrong_para;

	if (gpio >= TLMM_NUM_GPIO)
		goto tlmm_count_store_wrong_para;

	tlmm_count = gpio;
	pr_debug("tlmm_count: %d\n", tlmm_count);

	goto tlmm_count_store_ok;

tlmm_count_store_wrong_para:
	pr_debug("Wrong Input.\n");
	pr_debug("Format: tlmm_count\n");
	pr_debug("      tlmm_count: 0~%d\n", TLMM_NUM_GPIO);

tlmm_count_store_ok:
	return n;
}

static ssize_t tlmm_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *p = buf;

	p += tlmm_dump_info(buf, PAGE_SIZE, tlmm_num, tlmm_count);
	return p - buf;
}

static ssize_t tlmm_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	char pull_c, dir_c;
	int gpio, func, pull, dir, drvstr, output_val;
	unsigned cfg;
	int res;

	res = sscanf(buf, "%d %d %c %c %d %d",
		&gpio, &func, &pull_c, &dir_c, &drvstr, &output_val);
	pr_debug("res=%d.  %d %d %c %c %d %d\n",
		res, gpio, func, pull_c, dir_c, drvstr, output_val);

	/* Add a shortcut wrting format to change an output gpio's value */
	if (res == 2 && gpio < TLMM_NUM_GPIO && (func == 0 || func == 1)) {
		output_val = func;
		goto tlmm_store_only_output_val;
	}
	if ((res != 5) && (res != 6))
		goto tlmm_store_wrong_para;

	if (gpio >= TLMM_NUM_GPIO)
		goto tlmm_store_wrong_para;

	if ('N' == pull_c)
		pull = 0;
	else if ('D' == pull_c)
		pull = 1;
	else if ('K' == pull_c)
		pull = 2;
	else if ('U' == pull_c)
		pull = 3;
	else
		goto tlmm_store_wrong_para;

	if ('I' == dir_c)
		dir = 0;
	else if ('O' == dir_c)
		dir = 1;
	else
		goto tlmm_store_wrong_para;

	drvstr = drvstr/2 - 1; /* 2mA -> 0, 4mA -> 1, 6mA -> 2, ... */
	if (drvstr > 7)
		goto tlmm_store_wrong_para;

	if (output_val > 1)
		goto tlmm_store_wrong_para;

	pr_debug("final set: %d %d %d %d %d %d\n",
		gpio, func, pull, dir, drvstr, output_val);

	cfg = GPIO_CFG(gpio, func, dir, pull, drvstr);
	if (gpio < TLMM_NUM_GPIO) {
#ifdef CONFIG_GPIO_VALID_ARRAY
		if (tz_configs[gpio])
			res = -1;
		else
#endif
			res = tlmm_set_config(cfg);
		if (res < 0) {
			pr_debug("Error: Config failed.\n");
			goto tlmm_store_wrong_para;
		}
	}
	pr_debug("final set: %d %d %d %d %d %d\n",
		gpio, func, pull, dir, drvstr, output_val);
	if ((func == 0)  && (dir == 1)) /* gpio output */
tlmm_store_only_output_val:
		tlmm_set_inout(gpio, output_val);

	goto tlmm_store_ok;

tlmm_store_wrong_para:
	pr_debug("Wrong Input or gpio is locked by TZ.\n");
	pr_debug("Standard Format: gpio_num  function  pull  direction  strength [output_value]\n");
	pr_debug("Shortcut Format: gpio_num  output_value\n");
	pr_debug("      gpio_num: 0 ~ 145\n");
	pr_debug("      function: number, where 0 is GPIO\n");
	pr_debug("      pull: 'N': NO_PULL, 'D':PULL_DOWN, 'K':KEEPER, 'U': PULL_UP\n");
	pr_debug("      direction: 'I': Input, 'O': Output\n");
	pr_debug("      strength:  2, 4, 6, 8, 10, 12, 14, 16\n");
	pr_debug("      output_value:  Optional. 0 or 1. valid if GPIO output\n");
	pr_debug(" e.g.  'echo  20 0 D I 2'  ==> set pin 20 as GPIO input\n");
	pr_debug(" e.g.  'echo  20 0 D O 2 1'  ==> set pin 20 as GPIO output and the output = 1\n");
	pr_debug(" e.g.  'echo  20 1'  ==> set output gpio pin 20 output = 1\n");

tlmm_store_ok:
	return n;
}

#ifdef CONFIG_GPIO_VALID_ARRAY
static ssize_t tlmm_tz_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += tlmm_tz_dump_info(buf, PAGE_SIZE);
	return p - buf;
}

static ssize_t tlmm_tz_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	int gpio, func = 0;
	int res;

	res = sscanf(buf, "%d %d", &gpio, &func);
	pr_debug("res=%d.  %d %d\n", res, gpio, func);

	if (res == 1)
		func = 1;

	pr_debug("final set: %d %d\n", gpio, func);

	if (gpio >= TLMM_NUM_GPIO)
		goto tlmm_tz_store_wrong_para;

	tz_configs[gpio] = func;

	goto tlmm_tz_store_ok;

tlmm_tz_store_wrong_para:
	pr_debug("Wrong Input.\n");
	pr_debug("Format: gpio [0|1]\n");

tlmm_tz_store_ok:
	return n;
}
#endif

/* Set GPIO's sleep config from sysfs */
static ssize_t tlmm_before_sleep_table_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += tlmm_before_sleep_table_dump_info(buf, PAGE_SIZE);
	return p - buf;
}

static ssize_t tlmm_before_sleep_table_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	char pull_c, dir_c;
	int gpio, func = 0, pull = 0, dir = 0, drvstr = 0, output_val = 0;
	int ignore;
	unsigned cfg;
	int res;

	res = sscanf(buf, "%d %d %c %c %d %d",
		&gpio, &func, &pull_c, &dir_c, &drvstr, &output_val);
	pr_debug("res=%d.  %d %d %c %c %d %d\n",
		res, gpio, func, pull_c, dir_c, drvstr, output_val);

	/* if only gpio, means ignore(disable) the gpio's sleep config */
	if (res == 1) {
		ignore = 1;
		pr_debug("final set: to disable gpio %d sleep config\n", gpio);
	} else {
		ignore = 0;

		if ((res != 5) && (res != 6))
			goto tlmm_before_sleep_table_store_wrong_para;

		if (gpio >= TLMM_NUM_GPIO)
			goto tlmm_before_sleep_table_store_wrong_para;

		if ('N' == pull_c)
			pull = 0;
		else if ('D' == pull_c)
			pull = 1;
		else if ('K' == pull_c)
			pull = 2;
		else if ('U' == pull_c)
			pull = 3;
		else
			goto tlmm_before_sleep_table_store_wrong_para;

		if ('I' == dir_c)
			dir = 0;
		else if ('O' == dir_c)
			dir = 1;
		else
			goto tlmm_before_sleep_table_store_wrong_para;

		drvstr = drvstr/2 - 1; /* 2mA -> 0, 4mA -> 1, 6mA -> 2, ... */
		if (drvstr > 7)
			goto tlmm_before_sleep_table_store_wrong_para;

		pr_debug("final set: %d %d %d %d %d\n",
			gpio, func, pull, dir, drvstr);
	}

	cfg = GPIO_CFG(ignore ? 0xff : gpio, func, dir, pull, drvstr);
	res = tlmm_before_sleep_table_set_cfg(gpio, cfg | (output_val << 30));
	if (res < 0) {
		pr_debug("Error: Config failed.\n");
		goto tlmm_before_sleep_table_store_wrong_para;
	}

	goto tlmm_before_sleep_table_store_ok;

tlmm_before_sleep_table_store_wrong_para:
	pr_debug("Wrong Input.\n");
	pr_debug("Format: refer to tlmm's format except  'echo gpio_num > xxx' to disable the gpio's setting\n");

tlmm_before_sleep_table_store_ok:
	return n;
}

static ssize_t tlmm_before_sleep_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += tlmm_before_sleep_dump_info(buf, PAGE_SIZE);
	return p - buf;
}

static ssize_t tlmm_before_sleep_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	pr_err("%s: no support.\n", __func__);
	return n;
}

static ssize_t vreg_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	char *p = buf;

	p += vreg_dump_info(buf);
	return p - buf;
}

static ssize_t vreg_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	pr_err("%s: no support.\n", __func__);
	return n;
}

static ssize_t vreg_before_sleep_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	p += vreg_before_sleep_dump_info(buf);
	return p - buf;
}

static ssize_t vreg_before_sleep_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	pr_err("%s: no support.\n", __func__);
	return n;
}

static ssize_t wake_lock_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	char *s = buf;

	s += wakelock_dump_active_info(s, PAGE_SIZE - (s - buf));

	return (s - buf);
}

static ssize_t wake_lock_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t n)
{
	pr_err("%s: no support yet.\n", __func__);

	return -EPERM;
}

private_attr(tlmm_num);
private_attr(tlmm_count);
private_attr(tlmm);
#ifdef CONFIG_GPIO_VALID_ARRAY
private_attr(tlmm_tz);
#endif
private_attr(tlmm_before_sleep_table);
private_attr(tlmm_before_sleep);
private_attr(vreg_before_sleep);
private_attr(vreg);
private_attr(wake_lock);

static struct attribute *g_private_attr[] = {
	&tlmm_num_attr.attr,
	&tlmm_count_attr.attr,
	&tlmm_attr.attr,
#ifdef CONFIG_GPIO_VALID_ARRAY
	&tlmm_tz_attr.attr,
#endif
	&tlmm_before_sleep_table_attr.attr,
	&tlmm_before_sleep_attr.attr,
	&vreg_attr.attr,
	&vreg_before_sleep_attr.attr,
	&wake_lock_attr.attr,
	NULL,
};

static struct attribute_group private_attr_group = {
	.attrs = g_private_attr,
};

static struct kobject *sysfs_private_kobj;

#ifdef CONFIG_GPIO_VALID_ARRAY
static const struct of_device_id msm_tz_gpio_match_table[] = {
	{.compatible = "qcom,tz_gpio"},
	{},
};

static int tlmm_tz_init(void)
{
	struct device_node *node;
	const __be32 *list;
	int size;
	int result = 0;

	node = of_find_matching_node(NULL, msm_tz_gpio_match_table);
	pr_debug("%s(), %d, node=%p\n", __func__, __LINE__, node);

	if (!node)
		return -ENODEV;

	list = of_get_property(node, "tlmm-tz", &size);
	pr_debug("%s(), %d, size=%d\n", __func__, __LINE__, size);

	if (!list || !size)
		return -ENODEV;

	/*
	 * Size is in bytes. Convert to size of uint32_t
	 */
	size /= sizeof(*list);

	while (size) {
		unsigned long gpio = be32_to_cpup(list++);

		pr_debug("%s(), %d, gpio=%ld\n", __func__, __LINE__, gpio);
		if (gpio < TLMM_NUM_GPIO)
			tz_configs[gpio] = 1;
		size--;
	}

	return result;
}
#endif

static int __init sysfs_private_init(void)
{
	int result;

	pr_debug("%s(), %d\n", __func__, __LINE__);

	sysfs_private_kobj = kobject_create_and_add("private", NULL);
	if (!sysfs_private_kobj)
		return -ENOMEM;

	result = sysfs_create_group(sysfs_private_kobj, &private_attr_group);
	pr_debug("%s(), %d, result=%d\n", __func__, __LINE__, result);

#ifdef CONFIG_GPIO_VALID_ARRAY
	result = tlmm_tz_init();
	pr_debug("%s(), %d, result=%d\n", __func__, __LINE__, result);
#endif

	return result;
}

static void __exit sysfs_private_exit(void)
{
	pr_debug("%s(), %d\n", __func__, __LINE__);
	sysfs_remove_group(sysfs_private_kobj, &private_attr_group);

	kobject_put(sysfs_private_kobj);
}

module_init(sysfs_private_init);
module_exit(sysfs_private_exit);
