/*
 * Copyright (C) 2009 Motorola, Inc.
 * Copyright (C) 2012 Motorola Mobility. All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <asm/setup.h>
#include <soc/qcom/bootinfo.h>
#include <linux/seq_file.h>
#include <linux/pstore.h>
#include <linux/pstore_ram.h>

#ifdef CONFIG_ARM64
/* these are defined in kernel/setup.c for "arm" targets */
unsigned int system_rev;
EXPORT_SYMBOL(system_rev);

unsigned int system_serial_low;
EXPORT_SYMBOL(system_serial_low);

unsigned int system_serial_high;
EXPORT_SYMBOL(system_serial_high);
#endif

/*
 * EMIT_BOOTINFO and EMIT_BOOTINFO_STR are used to emit the bootinfo
 * information for data provided via DEVICE TREE.
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
			seq_printf(m, strname ": " fmt "\n", \
					bi_##name()); \
		} while (0)

#define EMIT_BOOTINFO_STR(strname, strval) \
		do { \
			if (strlen(strval) == 0) { \
				seq_printf(m, "%s: UNKNOWN\n", \
						strname); \
			} else { \
				seq_printf(m, "%s: %s\n", \
						strname, strval); \
			} \
		} while (0)

#define EMIT_BOOTINFO_LASTKMSG(buf, strname, fmt, name) \
		do { \
			snprintf(buf, sizeof(buf), strname ": " fmt "\n", \
					bi_##name()); \
			pstore_annotate(buf); \
		} while (0)

/*-------------------------------------------------------------------------*/

/*
 * powerup_reason contains the powerup reason provided by the ATAGs when
 * the machine boots.
 *
 * Exported symbols:
 * bi_powerup_reason()             -- returns the powerup reason
 */

#ifdef CONFIG_OF
static void of_powerup(u32 *pwr)
{
	struct device_node *n = of_find_node_by_path("/chosen");

	of_property_read_u32(n, "mmi,powerup_reason", pwr);
	of_node_put(n);
}
#else
static inline void of_powerup(u32 *pwr) { }
#endif

u32 bi_powerup_reason(void)
{
	u32 reason = PU_REASON_INVALID;

	of_powerup(&reason);
	return reason;
}
EXPORT_SYMBOL(bi_powerup_reason);

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
 */
#ifdef CONFIG_OF
static void of_mbmver(u32 *ver)
{
	struct device_node *n = of_find_node_by_path("/chosen");

	of_property_read_u32(n, "mmi,mbmversion", ver);
	of_node_put(n);
}
#else
static inline void of_mbmver(u32 *ver) { }
#endif

u32 bi_mbm_version(void)
{
	u32 version = 0xFFFFFFFF;

	of_mbmver(&version);
	return version;
}
EXPORT_SYMBOL(bi_mbm_version);

#define EMIT_MBM_VERSION() \
	    EMIT_BOOTINFO("MBM_VERSION", "0x%08x", mbm_version)

/*
 * boot_seq contains the boot sequence number.
 * boot_seq default to 0 if it is not set.
 *
 * Exported symbols:
 * bi_boot_seq()                -- returns the boot seq
 */
#ifdef CONFIG_OF
static void of_bootseq(u32 *seq)
{
	struct device_node *n = of_find_node_by_path("/chosen");

	of_property_read_u32(n, "mmi,boot_seq", seq);
	of_node_put(n);
}
#else
static inline void of_boot_seq(u32 *seq) { }
#endif

u32 bi_boot_seq(void)
{
	u32 seq = 0x0;

	of_bootseq(&seq);
	return seq;
}
EXPORT_SYMBOL(bi_boot_seq);

#define EMIT_BOOT_SEQ() \
	    EMIT_BOOTINFO("BOOT_SEQ", "0x%08x", boot_seq)

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
	int pos;
	char *value;
	char *ptr;

	if (!bld_sig || (bl_build_sig_count >= MAX_BL_BUILD_SIG)) {
		return;
	}

	value = (char *)memchr((void *)bld_sig, '=', MAX_BLD_SIG_ITEM);
	if (!value) {
		return;
	}
	pos = value - bld_sig;

	ptr = bl_build_sigs[bl_build_sig_count].item;
	strlcpy(ptr, bld_sig, pos+1);
	convert_to_upper(ptr);

	ptr = bl_build_sigs[bl_build_sig_count].value;
	strlcpy(ptr, value+1, MAX_BLD_SIG_VALUE);

	bl_build_sig_count++;

	return;
}
EXPORT_SYMBOL(bi_add_bl_build_sig);

#ifdef CONFIG_OF
static void __init of_blsig(void)
{
	struct property *p;
	struct device_node *n;

	/* Only do this one time once we find the sigs */
	if (bl_build_sig_count)
		return;

	n = of_find_node_by_path("/chosen/mmi,bl_sigs");
	if (n == NULL)
		return;

	for_each_property_of_node(n, p)
		if (strcmp(p->name, "name"))
			bi_add_bl_build_sig(p->value);

	of_node_put(n);
}
#else
static inline void of_blsig(void) { }
#endif

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

#define BOOTREASON_MAX_LEN 64
static char bootreason[BOOTREASON_MAX_LEN];
int __init bootinfo_bootreason_init(char *s)
{
	strlcpy(bootreason, s, BOOTREASON_MAX_LEN);
	return 1;
}
__setup("androidboot.bootreason=", bootinfo_bootreason_init);

const char *bi_bootreason(void)
{
	return bootreason;
}
EXPORT_SYMBOL(bi_bootreason);

static void bootinfo_lastkmsg_annotate_bl(struct bl_build_sig *bl)
{
	int i;
	char buf[BOOTREASON_MAX_LEN];
	for (i = 0; i < bl_build_sig_count; i++) {
		bl[i].item[MAX_BLD_SIG_ITEM - 1] = 0;
		bl[i].value[MAX_BLD_SIG_VALUE - 1] = 0;
		pstore_annotate(bl[i].item);
		pstore_annotate("=");
		pstore_annotate(bl[i].value);
		pstore_annotate("\n");
	}

	EMIT_BOOTINFO_LASTKMSG(buf, "MBM_VERSION", "0x%08x", mbm_version);
	pstore_annotate(linux_banner);
	EMIT_BOOTINFO_LASTKMSG(buf, "SERIAL", "0x%llx", serial);
	EMIT_BOOTINFO_LASTKMSG(buf, "HW_REV", "0x%04x", hwrev);
	EMIT_BOOTINFO_LASTKMSG(buf, "BOOT_SEQ", "0x%08x", boot_seq);
	persistent_ram_annotation_append("POWERUPREASON: 0x%08x\n",
						bi_powerup_reason());
	persistent_ram_annotation_append("\nBoot info:\n");
	persistent_ram_annotation_append("Last boot reason: %s\n", bootreason);
}

/* get_bootinfo fills in the /proc/bootinfo information.
 * We currently only have the powerup reason, mbm_version, serial
 * and hwrevision.
 */

static int get_bootinfo(struct seq_file *m, void *v)
{
	int len = 0;

	EMIT_SERIAL();
	EMIT_HWREV();
	EMIT_POWERUPREASON();
	EMIT_MBM_VERSION();
	EMIT_BL_BUILD_SIG();
	EMIT_BOOT_SEQ();
	EMIT_BOOTINFO("Last boot reason", "%s", bootreason);

	return len;
}

static struct proc_dir_entry *proc_bootinfo;

static int bootinfo_proc_open(struct inode *inode, struct file *file) {
	return single_open(file, get_bootinfo, PDE_DATA(inode));
}

static const struct file_operations bootinfo_proc_fops = {
	.open           = bootinfo_proc_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};

int __init bootinfo_init_module(void)
{
	of_blsig();
	proc_bootinfo = &proc_root;
	if (!proc_create_data("bootinfo", 0, NULL, &bootinfo_proc_fops, NULL))
		printk(KERN_ERR "Failed to create bootinfo entry");
	bootinfo_lastkmsg_annotate_bl(bl_build_sigs);
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
