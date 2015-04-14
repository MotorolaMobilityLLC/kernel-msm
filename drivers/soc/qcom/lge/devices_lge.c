/* Copyright (c) 2015, LGE Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/string.h>
#include <soc/qcom/lge/board_lge.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/platform_device.h>
#include <linux/mdss_io_util.h>

int __init lge_charger_mode_init(char *s)
{
	if (!strcmp(s, "charger"))
		mdss_set_bl_ctrl_by_panel(true);

	return 1;
}
__setup("androidboot.mode=", lge_charger_mode_init);

static enum hw_rev_type lge_bd_rev = HW_REV_MAX;

/* CAUTION: These strings are come from LK. */
char *rev_str[] = {"evb1", "evb2", "evb3", "rev_0", "rev_a", "rev_b", "rev_c",
	"rev_d", "rev_e", "rev_f", "rev_g", "rev_10", "rev_11", "rev_12",
	"reserved"};

static int __init board_revno_setup(char *rev_info)
{
	int i;

	for (i = 0; i < HW_REV_MAX; i++) {
		if (!strncmp(rev_info, rev_str[i], 6)) {
			lge_bd_rev = i;
			break;
		}
	}

	pr_info("BOARD : LGE %s\n", rev_str[lge_bd_rev]);
	return 1;
}
__setup("lge.rev=", board_revno_setup);

enum hw_rev_type lge_get_board_revno(void)
{
	return lge_bd_rev;
}

/*
   for download complete using LAF image
   return value : 1 --> right after laf complete & reset
 */

int android_dlcomplete = 0;

int __init lge_android_dlcomplete(char *s)
{
	if (strncmp(s, "1", 1) == 0)
		android_dlcomplete = 1;
	else
		android_dlcomplete = 0;
	pr_info("androidboot.dlcomplete = %d\n", android_dlcomplete);

	return 1;
}
__setup("androidboot.dlcomplete=", lge_android_dlcomplete);

int lge_get_android_dlcomplete(void)
{
	return android_dlcomplete;
}

static enum lge_laf_mode_type lge_laf_mode = LGE_LAF_MODE_NORMAL;

int __init lge_laf_mode_init(char *s)
{
	if (strcmp(s, ""))
		lge_laf_mode = LGE_LAF_MODE_LAF;

	return 1;
}
__setup("androidboot.laf=", lge_laf_mode_init);

enum lge_laf_mode_type lge_get_laf_mode(void)
{
	return lge_laf_mode;
}

static int lge_boot_reason = -1; /* undefined for error checking */
static int __init lge_check_bootreason(char *reason)
{
	int ret = 0;

	/* handle corner case of kstrtoint */
	if (!strcmp(reason, "0xffffffff")) {
		lge_boot_reason = 0xffffffff;
		return 1;
	}

	ret = kstrtoint(reason, 16, &lge_boot_reason);
	if (!ret)
		printk(KERN_INFO "LGE REBOOT REASON: %x\n", lge_boot_reason);
	else
		printk(KERN_INFO "LGE REBOOT REASON: Couldn't get bootreason - %d\n",
				ret);

	return 1;
}
__setup("lge.bootreason=", lge_check_bootreason);

int lge_get_bootreason(void)
{
	return lge_boot_reason;
}
