/*
 * Copyright (C) 2016 SHARP CORPORATION
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
#ifndef __SHTPS_DEV_H__
#define __SHTPS_DEV_H__

#define SH_TOUCH_DEVNAME				"shtps_fts"
#define SH_TOUCH_IF_DEVNAME 			"shtpsif"
#define SH_TOUCH_IF_DEVPATH 			"/dev/shtpsif"

#define SHTPS_TM_TXNUM_MAX				27
#define SHTPS_TM_RXNUM_MAX				16

#define SHTPS_FINGER_MAX				5

#define TPS_IOC_MAGIC					0xE0

#define TPSDEV_GET_FW_VERSION			_IOR ( TPS_IOC_MAGIC,  1, unsigned short)
#define TPSDEV_GET_FW_VERSION_BUILTIN	_IOR ( TPS_IOC_MAGIC,  2, unsigned short)
#define TPSDEV_FW_UPGRADE				_IOW ( TPS_IOC_MAGIC,  3, struct shtps_ioctl_param)


#ifdef CONFIG_COMPAT
#define COMPAT_TPSDEV_FW_UPGRADE		_IOW ( TPS_IOC_MAGIC,  3, struct shtps_compat_ioctl_param)

typedef __u32 shtps_ioctl_compat_pointer_param;
struct shtps_compat_ioctl_param {
	int				size;
	__u32			data;
};
#endif /* CONFIG_COMPAT */


struct shtps_ioctl_param {
	int				size;
	unsigned char*	data;
};

/* -----------------------------------------------------------------------------------
 */
#ifdef CONFIG_TOUCHSCREEN_FT8607
extern void msm_tps_setsleep(int on);
#else
static inline void msm_tps_setsleep(int on) { return; }
#endif /* CONFIG_TOUCHSCREEN_FT8607 */

#endif /* __SHTPS_DEV_H__ */
