/*
 *      Intel_SCU 0.3:  An Intel SCU IOH Based Watchdog Device
 *			for Intel part #(s):
 *				- AF82MP20 PCH
 *
 *      Copyright (C) 2009-2013 Intel Corporation. All rights reserved.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of version 2 of the GNU General
 *      Public License as published by the Free Software Foundation.
 *
 *      This program is distributed in the hope that it will be
 *      useful, but WITHOUT ANY WARRANTY; without even the implied
 *      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *      PURPOSE.  See the GNU General Public License for more details.
 *      You should have received a copy of the GNU General Public
 *      License along with this program; if not, write to the Free
 *      Software Foundation, Inc., 59 Temple Place - Suite 330,
 *      Boston, MA  02111-1307, USA.
 *      The full GNU General Public License is included in this
 *      distribution in the file called COPYING.
 *
 */

#ifndef __INTEL_SCU_WATCHDOG_H
#define __INTEL_SCU_WATCHDOG_H

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#define PFX "intel_scu_watchdog: "
#define WDT_VER "0.3"

#define RESET_ON_PANIC_TIMEOUT 15
#define DEFAULT_PRETIMEOUT 75
#define DEFAULT_TIMEOUT (DEFAULT_PRETIMEOUT + RESET_ON_PANIC_TIMEOUT)

/* Value 0 to reset the reset counter */
#define OSNIB_WRITE_VALUE 0

struct intel_scu_watchdog_dev {
	ulong driver_open;
	ulong driver_closed;
	bool started;
	struct notifier_block reboot_notifier;
	struct miscdevice miscdev;
	bool shutdown_flag;
	bool reboot_flag;
	int reset_type;
	int normal_wd_action;
	int reboot_wd_action;
	int shutdown_wd_action;
#ifdef CONFIG_DEBUG_FS
	bool panic_reboot_notifier;
	struct dentry *dfs_wd;
	struct dentry *dfs_secwd;
	struct dentry *dfs_secwd_trigger;
	struct dentry *dfs_kwd;
	struct dentry *dfs_kwd_trigger;
	struct dentry *dfs_kwd_reset_type;
	struct dentry *dfs_kwd_panic_reboot;
#endif /* CONFIG_DEBUG_FS */
};

#endif /* __INTEL_SCU_WATCHDOG_H */
