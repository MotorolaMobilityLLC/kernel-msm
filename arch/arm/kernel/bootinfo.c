/*
 * Copyright (C) 2009 Motorola, Inc.
 * Copyright (C) 2012 - 2013 Motorola Mobility. All rights reserved.
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
#include <asm/system.h>
#include <asm/bootinfo.h>
#include <linux/notifier.h>
#include <linux/persistent_ram.h>
#include <linux/apanic_mmc.h>
#include <linux/io.h>
#include <linux/rslib.h>

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

static u32 powerup_reason;
u32 bi_powerup_reason(void)
{
	u32 reason = powerup_reason;

	of_powerup(&reason);
	return reason;
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

/* symbolsize is 8 (bits) */
#define ECC_SYMSIZE	8
/* primitive polynomial is x^8+x^4+x^3+x^2+1 */
#define ECC_POLY	285
/* first consecutive root is 0 */
#define ECC_FCR		0
/* primitive element to generate roots = 1 */
#define ECC_ELEM	1
/* generator polynomial degree (number of roots) = 16 */
#define ECC_SIZE	16
#define ECC_BLOCK_SIZE	128

#define BOOTINFO_BCK_BUF_ALIGN	ECC_BLOCK_SIZE
#define BOOTINFO_BCK_MAGIC	0x626f6f74696e666f	/* bootinfo */
#define BOOTINFO_LKMSG(fmt, args...) do {		\
	persistent_ram_ext_oldbuf_print(fmt, ##args);	\
} while (0)

struct bootinfo_bck {
	u64 magic;
	struct bl_build_sig bl[MAX_BL_BUILD_SIG];
	char linux_banner[512];
} __aligned(BOOTINFO_BCK_BUF_ALIGN);

struct bootinfo_bck_buf {
	struct bootinfo_bck bck;
	u8 ecc[(sizeof(struct bootinfo_bck) / ECC_BLOCK_SIZE) * ECC_SIZE];
} __aligned(BOOTINFO_BCK_BUF_ALIGN);

static int bootinfo_bck_buf_reserved;

int __init bootinfo_bck_size(void)
{
	pr_debug("%s size %d\n", __func__, sizeof(struct bootinfo_bck_buf));
	return sizeof(struct bootinfo_bck_buf);
}

void __init bootinfo_bck_buf_set_reserved(void)
{
	bootinfo_bck_buf_reserved = 1;
}

static void __init bootinfo_lkmsg_bl(struct bl_build_sig *bl)
{
	int i;
	for (i = 0; i < bl_build_sig_count; i++) {
		bl[i].item[MAX_BLD_SIG_ITEM - 1] = 0;
		bl[i].value[MAX_BLD_SIG_VALUE - 1] = 0;
		BOOTINFO_LKMSG("%s = %s\n", bl[i].item, bl[i].value);
	}
}

static void __init bootinfo_apanic_annotate_bl(struct bl_build_sig *bl)
{
	int i;
	for (i = 0; i < bl_build_sig_count; i++) {
		bl[i].item[MAX_BLD_SIG_ITEM - 1] = 0;
		bl[i].value[MAX_BLD_SIG_VALUE - 1] = 0;
		apanic_mmc_annotate(bl[i].item);
		apanic_mmc_annotate("=");
		apanic_mmc_annotate(bl[i].value);
		apanic_mmc_annotate("\n");
	}
}

static void __init bootinfo_bck_update_ecc(struct rs_control *rs,
				struct bootinfo_bck_buf *buf)
{
	u8 *payload, *ecc;
	int i;
	u16 par[ECC_SIZE];

	for (payload = (u8 *)&buf->bck, ecc = (u8 *)&buf->ecc;
			payload < (u8 *)&buf->ecc;
			payload += ECC_BLOCK_SIZE, ecc += ECC_SIZE) {
		memset(par, 0, sizeof(par));
		encode_rs8(rs, payload, ECC_BLOCK_SIZE, par, 0);
		for (i = 0; i < ECC_SIZE; i++)
			ecc[i] = par[i];
	}
}

static int __init bootinfo_bck_ecc(struct rs_control *rs,
				struct bootinfo_bck_buf *buf)
{
	u8 *payload, *ecc;
	int i, bad_blocks = 0, corrected = 0, numerr;
	u16 par[ECC_SIZE];

	for (payload = (u8 *)&buf->bck, ecc = (u8 *)&buf->ecc;
			payload < (u8 *)&buf->ecc;
			payload += ECC_BLOCK_SIZE, ecc += ECC_SIZE) {
		for (i = 0; i < ECC_SIZE; i++)
			par[i] = ecc[i];
		numerr = decode_rs8(rs, payload, par, ECC_BLOCK_SIZE,
				NULL, 0, NULL, 0, NULL);
		if (numerr > 0)
			corrected += numerr;
		else if (numerr < 0)
			bad_blocks++;
	}
	if (corrected)
		BOOTINFO_LKMSG("\n%s: corrected %d bytes\n", __func__,
					corrected);
	if (bad_blocks)
		BOOTINFO_LKMSG("\n%s: %d bad blocks\n", __func__, bad_blocks);

	return bad_blocks;
}

static int __init bootinfo_bck_buf_check(struct rs_control *rs,
				struct bootinfo_bck_buf *buf)
{
	struct bootinfo_bck *bck = &buf->bck;

	if (bootinfo_bck_ecc(rs, buf))
		return -EINVAL;
	if (bck->magic != BOOTINFO_BCK_MAGIC)
		return -EINVAL;
	return 0;
}

static int __init bootinfo_blsig_comp(struct bl_build_sig *prev,
					struct bl_build_sig *this)
{
	int i, j;
	if (!prev || !this || (bl_build_sig_count > MAX_BL_BUILD_SIG))
		return -EINVAL;
	for (i = 0; i < bl_build_sig_count; i++) {
		for (j = 0; j < bl_build_sig_count; j++) {
			if (memcmp(prev[i].item, this[j].item,
					MAX_BLD_SIG_ITEM))
				continue;
			if (memcmp(prev[i].value, this[j].value,
					MAX_BLD_SIG_VALUE))
				return -EINVAL;
			else
				break;
		}
		if (j >= bl_build_sig_count)
			return -EINVAL;
	}
	return 0;
}

static void __init bootinfo_emit_to_last_kmsg(void)
{
	struct bootinfo_bck_buf *buf;
	struct rs_control *rs;

	if (!bootinfo_bck_buf_reserved) {
		pr_err("%s bck buf is not reserved\n", __func__);
		BOOTINFO_LKMSG("\nTHIS\n");
		goto print_this;
	}

	rs = init_rs(ECC_SYMSIZE, ECC_POLY, ECC_FCR, ECC_ELEM, ECC_SIZE);
	if (!rs) {
		pr_err("%s init_rs failed\n", __func__);
		BOOTINFO_LKMSG("\nTHIS\n");
		goto print_this;
	}

	buf = (struct bootinfo_bck_buf *)allocate_contiguous_ebi(
			bootinfo_bck_size(), BOOTINFO_BCK_BUF_ALIGN, 1);
	if (!buf) {
		free_rs(rs);
		pr_err("%s can't get bck buf (size %d)\n", __func__,
				bootinfo_bck_size());
		BOOTINFO_LKMSG("\nTHIS\n");
		goto print_this;
	}

	if (!bootinfo_bck_buf_check(rs, buf)) {
		int changed = 0;
		if (bootinfo_blsig_comp(buf->bck.bl, bl_build_sigs))
			changed |= 1;
		if (strncmp(linux_banner, buf->bck.linux_banner,
			sizeof(buf->bck.linux_banner)))
			changed |= 2;
		if (changed) {
			BOOTINFO_LKMSG("\nPREV\n");
			if (changed & 1)
				bootinfo_lkmsg_bl(buf->bck.bl);
			if (changed & 2) {
				buf->bck.linux_banner[
					sizeof(buf->bck.linux_banner) - 1] = 0;
				BOOTINFO_LKMSG(buf->bck.linux_banner);
			}
			BOOTINFO_LKMSG("\nTHIS\n");
		}
	} else {
		BOOTINFO_LKMSG("\nTHIS\n");
	}

	memset(buf, 0, sizeof(struct bootinfo_bck_buf));
	buf->bck.magic = BOOTINFO_BCK_MAGIC;
	memcpy(buf->bck.bl, bl_build_sigs, sizeof(bl_build_sigs));
	strlcpy(buf->bck.linux_banner, linux_banner,
			sizeof(buf->bck.linux_banner));
	bootinfo_bck_update_ecc(rs, buf);
	iounmap(buf);
	free_rs(rs);
print_this:
	bootinfo_lkmsg_bl(bl_build_sigs);
	BOOTINFO_LKMSG(linux_banner);
}

void __init bi_add_bl_build_sig(char *bld_sig)
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

static struct proc_dir_entry *proc_bootinfo;

int __init bootinfo_init_module(void)
{
	of_blsig();
	if (!mbm_version)
		of_mbmver(&mbm_version);
	proc_bootinfo = &proc_root;
	create_proc_read_entry("bootinfo", 0, NULL, get_bootinfo, NULL);
	bootinfo_emit_to_last_kmsg();
	bootinfo_apanic_annotate_bl(bl_build_sigs);
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
