/*
 * arch/arm/mach-msm/lge/device_lge.c
 *
 * Copyright (C) 2012,2013 LGE Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <asm/system_info.h>
#include <mach/board_lge.h>

/* See include/mach/board_lge.h. CAUTION: These strings come from LK. */
static char *rev_str[] = {"unknown", "evb1", "rev_a", "rev_b"};

static int __init board_revno_setup(char *rev_info)
{
	int i;

	/* it is defined externally in <asm/system_info.h> */
	system_rev = 0;

	for (i = 0; i < ARRAY_SIZE(rev_str); i++) {
		if (!strcmp(rev_info, rev_str[i])) {
			system_rev = i;
			break;
		}
	}

	pr_info("BOARD: LGE %s\n", rev_str[system_rev]);
	return 1;
}
__setup("lge.rev=", board_revno_setup);

int lge_get_board_revno(void)
{
    return system_rev;
}
