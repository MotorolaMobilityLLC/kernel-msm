/*
 * SiW touch system interface
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_SYS_H
#define __SIW_TOUCH_SYS_H

extern int siw_touch_sys_bus_use_dma(struct device *dev);

extern int siw_touch_get_boot_mode(void);
extern int siw_touch_boot_mode_check(struct device *dev);

extern int siw_touch_boot_mode_tc_check(struct device *dev);

extern int siw_touch_sys_gpio_set_pull(int pin, int value);

extern int siw_touch_sys_panel_reset(struct device *dev);
extern int siw_touch_sys_get_panel_bl(struct device *dev);
extern int siw_touch_sys_set_panel_bl(struct device *dev, int level);

extern int siw_touch_sys_osc(struct device *dev, int onoff);

extern int siw_touch_init_sysfs(struct siw_ts *ts);
extern void siw_touch_free_sysfs(struct siw_ts *ts);

extern int siw_touch_add_sysfs(struct siw_ts *ts);
extern void siw_touch_del_sysfs(struct siw_ts *ts);

#endif	/* __SIW_TOUCH_SYS_H */

