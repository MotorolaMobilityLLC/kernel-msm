/*
 * SiW touch debug
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_DBG_H
#define __SIW_TOUCH_DBG_H

#include <linux/errno.h>

#define __SIZE_POW(x)			(1<<(x))

enum _SIW_TOUCH_DBG_FLAG {
	DBG_NONE			= 0,
	DBG_BASE			= (1U << 0),
	DBG_TRACE			= (1U << 1),
	DBG_GET_DATA			= (1U << 2),
	DBG_ABS				= (1U << 3),

	DBG_BUTTON			= (1U << 4),
	DBG_FW_UPGRADE			= (1U << 5),
	DBG_GHOST			= (1U << 6),
	DBG_IRQ				= (1U << 7),

	DBG_PM				= (1U << 8),
	DBG_JITTER			= (1U << 9),
	DBG_ACCURACY			= (1U << 10),
	DBG_BOUNCING			= (1U << 11),

	DBG_GRIP			= (1U << 12),
	DBG_FILTER_RESULT		= (1U << 13),
	DBG_QUICKCOVER			= (1U << 14),
	DBG_LPWG			= (1U << 15),

	DBG_LPWG_COOR			= (1U << 16),
	DBG_NOISE			= (1U << 17),
	DBG_WATCH			= (1U << 18),
	DBG_RSVD18			= (1U << 19),

	DBG_RSVD20			= (1U << 20),
	DBG_RSVD21			= (1U << 21),
	DBG_RSVD22			= (1U << 22),
	DBG_RSVD23			= (1U << 23),

	DBG_GPIO			= (1U << 24),
	DBG_RSVD25			= (1U << 25),
	DBG_NOTI			= (1U << 26),
	DBG_EVENT			= (1U << 27),

	DBG_RSVD28			= (1U << 28),
	DBG_RSVD29			= (1U << 29),
	DBG_OF				= (1U << 30),
	DBG_ETC				= (1U << 31),
};

extern u32 t_pr_dbg_mask;
extern u32 t_dev_dbg_mask;

#define t_pr_info(fmt, args...)		pr_info(fmt, ##args)
#define t_pr_noti(fmt, args...)		pr_notice(fmt, ##args)
#define t_pr_warn(fmt, args...)		pr_warn(fmt, ##args)
#define t_pr_err(fmt, args...)		pr_err(fmt, ##args)

#define t_pr_dbg(condition, fmt, args...)			\
		do {							\
			if (unlikely(t_pr_dbg_mask & (condition)))	\
				pr_info(fmt, ##args);	\
		} while (0)


#define __t_dev_none()		do { } while (0)

#define __t_dev_info(_dev, fmt, args...)	dev_info(_dev, fmt, ##args)
#define __t_dev_warn(_dev, fmt, args...)	dev_warn(_dev, fmt, ##args)
#define __t_dev_err(_dev, fmt, args...)		dev_err(_dev, fmt, ##args)

#define t_dev_info(_dev, fmt, args...)		__t_dev_info(_dev, fmt, ##args)
#define t_dev_warn(_dev, fmt, args...)		__t_dev_warn(_dev, fmt, ##args)

#define t_dev_trace(_dev, fmt, args...)		__t_dev_info(_dev, fmt, ##args)
#define t_dev_err(_dev, fmt, args...)		__t_dev_err(_dev, fmt, ##args)

#define t_dev_info_sel(_dev, _prt, fmt, args...)	\
		do {	\
			if (_prt)	\
				t_dev_info(_dev, fmt, ##args);	\
		} while (0)

#define t_dev_trcf(_dev)		t_dev_trace(_dev, "[%s]\n", __func__)

#define t_dev_dbg(condition, _dev, fmt, args...)			\
		do {							\
			if (unlikely(t_dev_dbg_mask & (condition)))	\
				__t_dev_info(_dev, fmt, ##args);	\
		} while (0)

#define t_dev_dbg_base(_dev, fmt, args...)	\
		t_dev_dbg(DBG_BASE, _dev, fmt, ##args)

#define t_dev_dbg_trace(_dev, fmt, args...)	\
		t_dev_dbg(DBG_TRACE, _dev, fmt, ##args)

#define t_dev_dbg_getd(_dev, fmt, args...)	\
		t_dev_dbg(DBG_GET_DATA, _dev, fmt, ##args)

#define t_dev_dbg_abs(_dev, fmt, args...)	\
		t_dev_dbg(DBG_ABS, _dev, fmt, ##args)

#define t_dev_dbg_button(_dev, fmt, args...)	\
		t_dev_dbg(DBG_BUTTON, _dev, fmt, ##args)

#define t_dev_dbg_fwup(_dev, fmt, args...)	\
		t_dev_dbg(DBG_FW_UPGRADE, _dev, fmt, ##args)

#define t_dev_dbg_ghost(_dev, fmt, args...)	\
		t_dev_dbg(DBG_GHOST, _dev, fmt, ##args)

#define t_dev_dbg_irq(_dev, fmt, args...)	\
		t_dev_dbg(DBG_IRQ, _dev, fmt, ##args)

#define t_dev_dbg_pm(_dev, fmt, args...)	\
		t_dev_dbg(DBG_PM, _dev, fmt, ##args)

#define t_dev_dbg_jitter(_dev, fmt, args...)	\
		t_dev_dbg(DBG_JITTER, _dev, fmt, ##args)

#define t_dev_dbg_acc(_dev, fmt, args...)	\
		t_dev_dbg(DBG_ACCURACY, _dev, fmt, ##args)

#define t_dev_dbg_bounce(_dev, fmt, args...)	\
		t_dev_dbg(DBG_BOUNCING, _dev, fmt, ##args)

#define t_dev_dbg_grip(_dev, fmt, args...)	\
		t_dev_dbg(DBG_GRIP, _dev, fmt, ##args)

#define t_dev_dbg_filter(_dev, fmt, args...)	\
		t_dev_dbg(DBG_FILTER_RESULT, _dev, fmt, ##args)

#define t_dev_dbg_qcover(_dev, fmt, args...)	\
		t_dev_dbg(DBG_QUICKCOVER, _dev, fmt, ##args)

#define t_dev_dbg_lpwg(_dev, fmt, args...)	\
		t_dev_dbg(DBG_LPWG, _dev, fmt, ##args)

#define t_dev_dbg_lpwg_coor(_dev, fmt, args...)	\
		t_dev_dbg(DBG_LPWG_COOR, _dev, fmt, ##args)

#define t_dev_dbg_noise(_dev, fmt, args...)	\
		t_dev_dbg(DBG_NOISE, _dev, fmt, ##args)

#define t_dev_dbg_watch(_dev, fmt, args...)	\
		t_dev_dbg(DBG_WATCH, _dev, fmt, ##args)

#define t_dev_dbg_noti(_dev, fmt, args...)	\
		t_dev_dbg(DBG_NOTI, _dev, fmt, ##args)

#define t_dev_dbg_gpio(_dev, fmt, args...)	\
		t_dev_dbg(DBG_GPIO, _dev, fmt, ##args)

#define t_dev_dbg_event(_dev, fmt, args...)	\
		t_dev_dbg(DBG_EVENT, _dev, fmt, ##args)

#define t_dev_dbg_of(_dev, fmt, args...)	\
		t_dev_dbg(DBG_OF, _dev, fmt, ##args)

#define t_dev_dbg_etc(_dev, fmt, args...)	\
		t_dev_dbg(DBG_ETC, _dev, fmt, ##args)


extern u32 t_dbg_flag;

enum {
	DBG_FLAG_SKIP_IRQ		= (1<<0),
	DBG_FLAG_SKIP_IRQ_RESET	= (1<<1),
	/* */
	DBG_FLAG_SKIP_IEVENT	= (1<<8),
	DBG_FLAG_SKIP_UEVENT	= (1<<9),
	/* */
	DBG_FLAG_SKIP_INIT_LATE	= (1<<16),
	DBG_FLAG_TEST_INIT_LATE	= (1<<17),
	/* */
	DBG_FLAG_SKIP_MON_THREAD = (1<<24),
	DBG_FLAG_TEST_MON_THREAD = (1<<25),
};

#define ETDBOOTFAIL			((ENOMEDIUM<<3) + 0x00)
#define ETDSENTESD			((ENOMEDIUM<<3) + 0x01)
#define ETDSENTESDIRQ		((ENOMEDIUM<<3) + 0x0F)

#endif	/* __SIW_TOUCH_DBG_H */

