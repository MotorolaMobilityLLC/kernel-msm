/*
 * FocalTech ft8607 TouchScreen driver.
 *
 * Copyright (c) 2016  Focal tech Ltd.
 * Copyright (c) 2016, Sharp. All rights reserved.
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
#ifndef __FTS_DEV_H__
#define __FTS_DEV_H__

#define FTS_TOUCH_DEVNAME				"ftsif"
#define FTS_TOUCH_IF_DEVNAME 			"ftsif"
#define FTS_TOUCH_IF_DEVPATH 			"/dev/ftsif"

#define FTS_TM_TXNUM_MAX				27
#define FTS_TM_RXNUM_MAX				16

#define FTS_FINGER_MAX					5

#define FTS_IOC_MAGIC					0xE0

#define FTSDEV_GET_FW_VERSION			_IOR ( FTS_IOC_MAGIC,  1, unsigned short)
#define FTSDEV_GET_FW_VERSION_BUILTIN	_IOR ( FTS_IOC_MAGIC,  2, unsigned short)
#define FTSDEV_FW_UPGRADE				_IOW ( FTS_IOC_MAGIC,  3, struct fts_ioctl_param)


#ifdef CONFIG_COMPAT
#define COMPAT_FTSDEV_FW_UPGRADE		_IOW ( FTS_IOC_MAGIC,  3, struct fts_compat_ioctl_param)

typedef __u32 fts_ioctl_compat_pointer_param;
struct fts_compat_ioctl_param {
	int				size;
	__u32			data;
};
#endif /* CONFIG_COMPAT */


struct fts_ioctl_param {
	int				size;
	unsigned char*	data;
};

/* -----------------------------------------------------------------------------------
 */
#ifdef CONFIG_TOUCHSCREEN_FT8607
extern void fts_setsleep(int on);
#else
static inline void fts_setsleep(int on) { return; }
#endif /* CONFIG_TOUCHSCREEN_FT8607 */

#endif /* __FTS_DEV_H__ */
