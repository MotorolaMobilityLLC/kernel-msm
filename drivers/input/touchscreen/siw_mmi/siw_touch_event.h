/*
 * SiW touch event
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_EVENT_H
#define __SIW_TOUCH_EVENT_H

extern void siw_touch_report_event(void *ts_data);
extern void siw_touch_report_all_event(void *ts_data);
extern void siw_touch_send_uevent(void *ts_data, int type);

extern int siw_touch_init_uevent(void *ts_data);
extern void siw_touch_free_uevent(void *ts_data);

extern int siw_touch_init_input(void *ts_data);
extern void siw_touch_free_input(void *ts_data);

#endif	/* __SIW_TOUCH_EVENT_H */

