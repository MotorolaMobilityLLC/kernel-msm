/*
 * bootinfo.c: This file contains the bootinfo code.  This code
 *	  provides boot information via /proc/bootinfo.  The
 *	  information currently includes:
 *            the powerup reason
 *        This file also provides EZX compatible interfaces for
 *	  retrieving the powerup reason.  All new user-space consumers
 *	  of the powerup reason should use the /proc/bootinfo
 *	  interface and all kernel-space consumers of the powerup
 *	  reason should use the bi_powerup_reason interface.  The EZX
 *	  compatibility code is deprecated.
 *
 *
 * Copyright (C) 2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Revision History:
 *
 * Date         Author    Comment
 * ----------   --------  -----------
 * 30/06/2009   Motorola  Initialize version
 */


#ifdef CONFIG_BOOTINFO

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <asm/setup.h>
#include <asm/system.h>
#include <asm/bootinfo.h>
#include <linux/notifier.h>

/*
 * EMIT_BOOTINFO and EMIT_BOOTINFO_STR are used to emit the bootinfo
 * information for data provided via ATAGs.
 *
 * The format for all bootinfo lines is "name : val" and these macros
 * enforce that format.
 *
 * strname is the name printed in the name/val pair.
 * name is the name of the function to call
 *
 * EMIT_BOOTINFO and EMIT_BOOTINFO_STR depend on buf and len to already
 * be defined.
 */
#define EMIT_BOOTINFO(strname, fmt, name) \
		do { \
			len += sprintf(buf+len, strname " : " fmt "\n", \
					bi_##name()); \
		} while (0)

#define EMIT_BOOTINFO_STR(strname, strval) \
		do { \
			if (strlen(strval) == 0) { \
				len += sprintf(buf+len, "%s : UNKNOWN\n", \
						strname); \
			} else { \
				len += sprintf(buf+len, "%s : %s\n", \
						strname, strval); \
			} \
		} while (0)

/*-------------------------------------------------------------------------*/

/*
 * powerup_reason contains the powerup reason provided by the ATAGs when
 * the machine boots.
 *
 * Exported symbols:
 * bi_powerup_reason()             -- returns the powerup reason
 * bi_set_powerup_reason()         -- sets the powerup reason
 */
static u32 powerup_reason;
u32 bi_powerup_reason(void)
{
	return powerup_reason;
}
EXPORT_SYMBOL(bi_powerup_reason);

void bi_set_powerup_reason(u32 __powerup_reason)
{
	powerup_reason = __powerup_reason;
}
EXPORT_SYMBOL(bi_set_powerup_reason);

#define EMIT_POWERUPREASON() \
	    EMIT_BOOTINFO("POWERUPREASON", "0x%08x", powerup_reason)


/*
 * mbm_version contains the MBM version.
 * mbm_loader_version contains the MBM loader version.
 * mbm_version and mbm_loader_version default to 0 if they are
 * not set.
 *
 * Exported symbols:
 * bi_mbm_version()                -- returns the MBM version
 * bi_set_mbm_version()            -- sets the MBM version
 * bi_mbm_loader_version()         -- returns the MBM loader version
 * bi_set_mbm_loader_version()     -- sets the MBM loader version
 */
static u32 mbm_version;
u32 bi_mbm_version(void)
{
	return mbm_version;
}
EXPORT_SYMBOL(bi_mbm_version);

void bi_set_mbm_version(u32 __mbm_version)
{
	mbm_version = __mbm_version;
}
EXPORT_SYMBOL(bi_set_mbm_version);

static u32 mbm_loader_version;
u32 bi_mbm_loader_version(void)
{
	return mbm_loader_version;
}
EXPORT_SYMBOL(bi_mbm_loader_version);

void bi_set_mbm_loader_version(u32 __mbm_loader_version)
{
	mbm_loader_version = __mbm_loader_version;
}
EXPORT_SYMBOL(bi_set_mbm_loader_version);

#define EMIT_MBM_VERSION() \
	    EMIT_BOOTINFO("MBM_VERSION", "0x%08x", mbm_version)
#define EMIT_MBM_LOADER_VERSION() \
	    EMIT_BOOTINFO("MBM_LOADER_VERSION", "0x%08x", mbm_loader_version)


/*
 * flat_dev_tree_address contains the Motorola flat dev tree address.
 * flat_dev_tree_address defaults to -1 (0xffffffff) if it is not set.
 *
 * Exported symbols:
 * bi_flat_dev_tree_address()      -- returns the flat dev tree address
 * bi_set_flat_dev_tree_address()  -- sets the flat dev tree address
 */
static u32 flat_dev_tree_address = -1;
u32 bi_flat_dev_tree_address(void)
{
	return flat_dev_tree_address;
}
EXPORT_SYMBOL(bi_flat_dev_tree_address);

void bi_set_flat_dev_tree_address(u32 __flat_dev_tree_address)
{
	flat_dev_tree_address = __flat_dev_tree_address;
}
EXPORT_SYMBOL(bi_set_flat_dev_tree_address);

#define EMIT_FLAT_DEV_TREE_ADDRESS() \
		EMIT_BOOTINFO("FLAT_DEV_TREE_ADDRESS", "0x%08x", \
					flat_dev_tree_address)


/*
 * battery_status_at_boot indicates the battery status
 * when the machine started to boot.
 * battery_status_at_boot defaults to -1 (0xffff) if the battery
 * status can't be determined.
 *
 * Exported symbols:
 * bi_battery_status_at_boot()         -- returns the battery boot status
 * bi_set_battery_status_at_boot()     -- sets the battery boot status
 */
static u16 battery_status_at_boot = -1;
u16 bi_battery_status_at_boot(void)
{
	return battery_status_at_boot;
}
EXPORT_SYMBOL(bi_battery_status_at_boot);

void bi_set_battery_status_at_boot(u16 __battery_status_at_boot)
{
	battery_status_at_boot = __battery_status_at_boot;
}
EXPORT_SYMBOL(bi_set_battery_status_at_boot);

#define EMIT_BATTERY_STATUS_AT_BOOT() \
		EMIT_BOOTINFO("BATTERY_STATUS_AT_BOOT", "0x%04x", \
					battery_status_at_boot)

/*
 * cid_recover_boot contains the flag to indicate whether phone should
 * boot into recover mode or not.
 * cid_recover_boot defaults to 0 if it is not set.
 *
 * Exported symbols:
 * bi_cid_recover_boot()        -- returns the value of recover boot
 * bi_set_cid_recover_boot()    -- sets the value of recover boot
 */
static u32 cid_recover_boot;
u32 bi_cid_recover_boot(void)
{
	return cid_recover_boot;
}
EXPORT_SYMBOL(bi_cid_recover_boot);

void bi_set_cid_recover_boot(u32 __cid_recover_boot)
{
	cid_recover_boot = __cid_recover_boot;
}
EXPORT_SYMBOL(bi_set_cid_recover_boot);


#define EMIT_CID_RECOVER_BOOT() \
		EMIT_BOOTINFO("CID_RECOVER_BOOT", "0x%04x", cid_recover_boot)

/*
 * BL build signature a succession of lines of text each denoting
 * build/versioning information for each bootloader component,
 * as passed along from bootloader via ATAG_BL_BUILD_SIG(s)
 */

#define MAX_BL_BUILD_SIG  10
#define MAX_BLD_SIG_ITEM  20
#define MAX_BLD_SIG_VALUE 80

struct bl_build_sig {
	char item[MAX_BLD_SIG_ITEM];
	char value[MAX_BLD_SIG_VALUE];
};

static unsigned bl_build_sig_count;
static struct bl_build_sig bl_build_sigs[MAX_BL_BUILD_SIG];

static void convert_to_upper(char *str)
{
	while (*str) {
		*str = toupper(*str);
		str++;
	}
}

void bi_add_bl_build_sig(char *bld_sig)
{
	char *item;
	char *value = NULL;

	if (!bld_sig || (bl_build_sig_count >= MAX_BL_BUILD_SIG)) {
		return;
	}

	item = strsep(&bld_sig, "=");
	if (!item) {
		return;
	}

	value = strsep((char **)&bld_sig, "=");
	if (!value) {
		return;
	}

	convert_to_upper(item);
	strncpy((char *)bl_build_sigs[bl_build_sig_count].item,
		item, MAX_BLD_SIG_ITEM);
	bl_build_sigs[bl_build_sig_count].item[MAX_BLD_SIG_ITEM - 1] = '\0';

	strncpy((char *)bl_build_sigs[bl_build_sig_count].value,
		value, MAX_BLD_SIG_VALUE);
	bl_build_sigs[bl_build_sig_count].value[MAX_BLD_SIG_VALUE - 1] = '\0';

	bl_build_sig_count++;

	return;
}
EXPORT_SYMBOL(bi_add_bl_build_sig);

#define EMIT_BL_BUILD_SIG() \
		do { \
			int i; \
			for (i = 0; i < bl_build_sig_count; i++) { \
				EMIT_BOOTINFO_STR(bl_build_sigs[i].item, \
						bl_build_sigs[i].value); \
			} \
		} while (0)

/* System revision s global symbol exported by setup.c
 * use wrapper to maintain coherent format with the other
 * boot info elements
 */

static u32 bi_hwrev(void)
{
	return system_rev;
}

#define EMIT_HWREV() \
		EMIT_BOOTINFO("HW_REV", "0x%04x", hwrev)

/* Serial high and low are symbols exported by setup.c
 * use wrapper to maintain coherent format with the other
 * boot info elements
 */

static u64 bi_serial(void)
{
	return ((u64)system_serial_high << 32)
			| (u64)system_serial_low;
}

#define EMIT_SERIAL() \
		EMIT_BOOTINFO("SERIAL", "0x%llx", serial)

/* get_bootinfo fills in the /proc/bootinfo information.
 * We currently only have the powerup reason, mbm_version, serial
 * and hwrevision.
 */

static int get_bootinfo(char *buf, char **start,
						off_t offset, int count,
						int *eof, void *data)
{
	int len = 0;

	EMIT_SERIAL();
	EMIT_HWREV();
	EMIT_POWERUPREASON();
	EMIT_MBM_VERSION();
	EMIT_BATTERY_STATUS_AT_BOOT();
	EMIT_CID_RECOVER_BOOT();
	EMIT_BL_BUILD_SIG();

	return len;
}

static int bootinfo_panic(struct notifier_block *this,
						unsigned long event, void *ptr)
{
	printk(KERN_ERR "mbm_version=0x%08x", mbm_version);
	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = bootinfo_panic,
	.priority = 1,
};

static struct proc_dir_entry *proc_bootinfo;

int __init bootinfo_init_module(void)
{
	proc_bootinfo = &proc_root;
	create_proc_read_entry("bootinfo", 0, NULL, get_bootinfo, NULL);
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
	return 0;
}

void __exit bootinfo_cleanup_module(void)
{
	if (proc_bootinfo) {
		remove_proc_entry("bootinfo", proc_bootinfo);
		proc_bootinfo = NULL;
	}
}

module_init(bootinfo_init_module);
module_exit(bootinfo_cleanup_module);

MODULE_AUTHOR("MOTOROLA");
#endif /* CONFIG_BOOTINFO */
