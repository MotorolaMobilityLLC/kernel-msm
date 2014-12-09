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


#if !defined(__KERNEL__) || defined(CONFIG_BOOTINFO)

#define PU_REASON_INVALID		0xFFFFFFFF

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

#endif


#if defined(__KERNEL__)
#if defined(CONFIG_BOOTINFO)

extern struct proc_dir_entry proc_root;

u32  bi_powerup_reason(void);
u32  bi_mbm_version(void);


#else /* defined(CONFIG_BOOTINFO) */

static inline u32 bi_powerup_reason(void) { return 0xFFFFFFFF; }
static inline u32 bi_mbm_version(void) { return 0xFFFFFFFF; }

#endif /* !defined(CONFIG_BOOTINFO) */
#endif /* defined(__KERNEL__) */

#endif
