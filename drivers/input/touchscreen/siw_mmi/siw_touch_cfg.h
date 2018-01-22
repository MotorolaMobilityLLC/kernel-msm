/*
 * SiW touch core driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/version.h>

#ifndef __SIW_TOUCH_CFG_H
#define __SIW_TOUCH_CFG_H

#if defined(CONFIG_PINCTRL)
#define __SIW_SUPPORT_PINCTRL
#endif

#if defined(CONFIG_REGULATOR)
#define __SIW_SUPPORT_PWRCTRL
#endif

#if defined(CONFIG_NET)
#define __SIW_SUPPORT_ABT
#endif

#define __SIW_SUPPORT_PRD

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_LG4895) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_LG4946) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49406) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49407)
#define __SIW_SUPPORT_WATCH

#if defined(CONFIG_TOUCHSCREEN_SIW_OPT_SINGLE_SCR)
#undef __SIW_SUPPORT_WATCH
#endif
#endif

#define __SIW_SUPPORT_XFER

#if defined(CONFIG_ANDROID)
#define __SIW_SUPPORT_WAKE_LOCK
#endif

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_LG4894) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_LG4895) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_LG4946) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_LG4951) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49105) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49106) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49406) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49407) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49408) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49501)
#define __SIW_SUPPORT_UEVENT
#endif

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49105) ||	\
	defined(CONFIG_TOUCHSCREEN_MMI_SIW_SW49407) ||	\
	defined(CONFIG_TOUCHSCREEN_MMI_SIW_SW49408)
#define __SIW_I2C_TYPE_1
#define __SIW_SPI_TYPE_1
#endif

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49407) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49408) ||	\
	defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49501)
#define __SIW_FW_TYPE_1
#endif

#if defined(CONFIG_OF)
#define __SIW_CONFIG_OF
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
#elif defined(CONFIG_FB)
#define __SIW_CONFIG_FB
#endif

#if defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW1828)
#ifdef __SIW_SUPPORT_ABT
#undef __SIW_SUPPORT_ABT
#endif

#ifdef __SIW_CONFIG_FB
#undef __SIW_CONFIG_FB
#endif

#define __SIW_CONFIG_SYSTEM_PM
#elif defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW42101)
#define __SIW_BUS_ADDR_16BIT

#ifdef __SIW_SUPPORT_ABT
#undef __SIW_SUPPORT_ABT
#endif

#ifdef __SIW_SUPPORT_PRD
#undef __SIW_SUPPORT_PRD
#endif

#define __SIW_CONFIG_SYSTEM_PM

#elif defined(CONFIG_TOUCHSCREEN_SIW_MMI_SW49106)
#ifdef __SIW_SUPPORT_ABT
#undef __SIW_SUPPORT_ABT
#endif
#ifdef __SIW_SUPPORT_PRD
#define __SIW_SUPPORT_PRD_SET_SD_ONLY
#endif
#define __SIW_CONFIG_SYSTEM_PM
#define __SIW_CONFIG_PROX_ON_SUSPEND
#define __SIW_CONFIG_PROX_ON_RESUME

#define __SIW_CONFIG_KNOCK


#else	/* General case for Mobile */
#define __SIW_CONFIG_PROX_ON_SUSPEND
#define __SIW_CONFIG_PROX_ON_RESUME

#define __SIW_CONFIG_KNOCK

#define __SIW_CONFIG_SYSTEM_PM
#endif

#if defined(__SIW_CONFIG_SYSTEM_PM)
#if defined(CONFIG_HIBERNATE_CALLBACKS)
#endif	/* CONFIG_HIBERNATE_CALLBACKS */
#endif

#if defined(__SIW_SUPPORT_WATCH)
#define __SIW_CONFIG_SWIPE
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0))
#define __SIW_ATTR_PERMISSION_ALL
#endif

#define __SIW_ATTR_RST_BY_READ

#define SIW_TOUCH_NAME				"siw_touch"
#define SIW_TOUCH_CORE				"siw_touch_core"
#define SIW_TOUCH_INPUT				"siw_touch_input"
#define SIW_TOUCH_EXT_WATCH			"siw_ext_watch"

#define MAX_FINGER				10
#define MAX_LPWG_CODE				128

enum _SIW_CHIP_TYPE {
	CHIP_NONE	= 0x0000,

	CHIP_LG4894	= 0x0001,
	CHIP_LG4895	= 0x0002,
	CHIP_LG4946	= 0x0003,
	CHIP_LG4951	= 0x0004,

	CHIP_SW1828	= 0x0080,

	CHIP_SW46104	= 0x6104,

	CHIP_SW49105	= 0x0105,
	CHIP_SW49106	= 0x0106,

	CHIP_SW49406	= 0x0406,
	CHIP_SW49407	= 0x0407,
	CHIP_SW49408	= 0x0408,
	CHIP_SW49409	= 0x0409,

	CHIP_SW49501	= 0x0501,

	CHIP_SW42101	= 0x0211,
	CHIP_SW42103	= 0x0213,
};

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 6, 0))
#define mod_delayed_work	queue_delayed_work
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define subsys_system_register(_subsys, _group)	bus_register(_subsys)
#endif

#endif	/* __SIW_TOUCH_CFG_H */

