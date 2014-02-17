/*
 * linux/include/asm/bootinfo.h:  Include file for boot information
 *                                provided on Motorola phones
 *
 * Copyright (C) 2009 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * Date         Author          Comment
 * 01/07/2009   Motorola        Initial version
 */

#ifndef __ASMARM_BOOTINFO_H
#define __ASMARM_BOOTINFO_H


/*
 * These #defines are used for the bits in powerup_reason.
 */
#define PU_REASON_USB_CABLE		0x00000010 /* Bit 4  */
#define PU_REASON_FACTORY_CABLE		0x00000020 /* Bit 5  */
#define PU_REASON_PWR_KEY_PRESS		0x00000080 /* Bit 7  */
#define PU_REASON_CHARGER		0x00000100 /* Bit 8  */
#define PU_REASON_POWER_CUT		0x00000200 /* bit 9  */
#define PU_REASON_SW_AP_RESET		0x00004000 /* Bit 14 */
#define PU_REASON_WDOG_AP_RESET		0x00008000 /* Bit 15 */
#define PU_REASON_AP_KERNEL_PANIC	0x00020000 /* Bit 17 */
#define PU_REASON_INVALID		0xFFFFFFFF


/*
 * These #defines are used for the battery status at boot.
 * When no battery is present, the status is BATTERY_LO_VOLTAGE.
 */
#define BATTERY_GOOD_VOLTAGE    1
#define BATTERY_LO_VOLTAGE      2
#define BATTERY_UNKNOWN         (-1)

/*
 * /proc/bootinfo has a strict format.  Each line contains a name/value
 * pair which are separated with a colon and a single space on both
 * sides of the colon.  The following defines help you size the
 * buffers used to read the data from /proc/bootinfo.
 *
 * BOOTINFO_MAX_NAME_LEN:  maximum size in bytes of a name in the
 *                         bootinfo line.  Don't forget to add space
 *                         for the NUL if you need it.
 * BOOTINFO_MAX_VAL_LEN:   maximum size in bytes of a value in the
 *                         bootinfo line.  Don't forget to add space
 *                         for the NUL if you need it.
 * BOOTINFO_BUF_SIZE:      size in bytes of buffer that is large enough
 *                         to read a /proc/bootinfo line.  The extra
 *                         3 is for the " : ".  Don't forget to add
 *                         space for the NUL and newline if you
 *                         need them.
 */
#define BOOTINFO_MAX_NAME_LEN    32
#define BOOTINFO_MAX_VAL_LEN    128
#define BOOTINFO_BUF_SIZE       (BOOTINFO_MAX_NAME_LEN + \
					3 + BOOTINFO_MAX_VAL_LEN)


#if defined(__KERNEL__)
#if defined(CONFIG_BOOTINFO)

extern struct proc_dir_entry proc_root;

u32  bi_powerup_reason(void);
const char *bi_bootreason(void);
u32  bi_mbm_version(void);

void bi_add_bl_build_sig(char *bld_sig);

#else /* defined(CONFIG_BOOTINFO) */

static inline u32 bi_powerup_reason(void) { return 0xFFFFFFFF; }
static inline const char *bi_bootreason(void) { return "NOTSUPPORTED"; }
static inline u32 bi_mbm_version(void) { return 0xFFFFFFFF; }

#endif /* !defined(CONFIG_BOOTINFO) */
#endif /* defined(__KERNEL__) */

#endif
