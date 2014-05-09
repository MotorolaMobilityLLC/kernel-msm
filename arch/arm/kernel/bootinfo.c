/*
 * Copyright (C) 2009 Motorola, Inc.
 * Copyright (C) 2012 - 2014 Motorola Mobility. All rights reserved.
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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <asm/bootinfo.h>
#include <linux/pstore.h>

#define EMIT_BOOTINFO_LASTKMSG(buf, strname, fmt, name) \
		do { \
			snprintf(buf, sizeof(buf), strname ": " fmt "\n", \
					bi_##name()); \
			pstore_annotate(buf); \
		} while (0)

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
static void __init of_mbmver(u32 *ver)
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

#ifdef CONFIG_OF
static void __init of_hwrev(u32 *revision)
{
	struct device_node *n = of_find_node_by_path("/chosen");

	of_property_read_u32(n, "linux,hwrev", revision);
	of_node_put(n);
}
#else
static inline void of_hwrev(u32 *ver) { }
#endif

static u32 bi_hwrev(void)
{
	u32 rev = 0xFFFFFFFF;
	of_hwrev(&rev);
	return rev;
}

#ifdef CONFIG_OF
static void __init of_serial(u32 *serial_h, u32 *serial_l)
{
	struct device_node *n = of_find_node_by_path("/chosen");

	of_property_read_u32(n, "linux,seriallow", serial_l);
	of_property_read_u32(n, "linux,serialhigh", serial_h);
	of_node_put(n);
}
#else
static inline void of_serial(u32 *serial_h, u32 *serial_l) { }
#endif

static u64 bi_serial(void)
{
	u32 serial_high = 0xFFFFFFFF;
	u32 serial_low = 0xFFFFFFFF;

	of_serial(&serial_high, &serial_low);
	return ((u64)serial_high << 32) | (u64)serial_low;
}

#define BOOTREASON_MAX_LEN 64
static char bootreason[BOOTREASON_MAX_LEN + 1];
int __init board_bootreason_init(char *s)
{
	strncpy(bootreason, s, BOOTREASON_MAX_LEN);
	bootreason[BOOTREASON_MAX_LEN] = '\0';
	return 1;
}
__setup("bootreason=", board_bootreason_init);

const char *bi_bootreason(void)
{
	return bootreason;
}
EXPORT_SYMBOL(bi_bootreason);

static void bootinfo_annotate_lastkmsg(void)
{
	char buf[BOOTREASON_MAX_LEN];
	pstore_annotate("Boot info:\n");
	EMIT_BOOTINFO_LASTKMSG(buf, "Last boot reason", "%s", bootreason);
}

/* get_bootinfo fills in the /proc/bootinfo information.
 * We currently only have the powerup reason, mbm_version, serial
 * and hwrevision.
 */
static int get_bootinfo(struct seq_file *m, void *v)
{
	seq_printf(m, "SERIAL : 0x%llx\n", bi_serial());
	seq_printf(m, "HW_REV : 0x%04x\n", bi_hwrev());
	seq_printf(m, "POWERUPREASON : 0x%08x\n", bi_powerup_reason());
	seq_printf(m, "MBM_VERSION : 0x%08x\n", bi_mbm_version());
	seq_printf(m, "Last boot reason : %s \n", bootreason);
	return 0;
}
static int  bootinfo_open(struct inode *inode, struct  file *file)
{
	return single_open(file, get_bootinfo, NULL);
}

static const struct file_operations bootinfo_fops = {
	.owner = THIS_MODULE,
	.open = bootinfo_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct proc_dir_entry *proc_bootinfo;

static int __init bootinfo_init_module(void)
{
	proc_bootinfo = &proc_root;
	proc_create("bootinfo", 0, NULL, &bootinfo_fops);
	bootinfo_annotate_lastkmsg();
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
