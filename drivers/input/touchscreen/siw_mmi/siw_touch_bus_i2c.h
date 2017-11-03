/*
 * SiW touch bus i2c driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_BUS_I2C_H
#define __SIW_TOUCH_BUS_I2C_H

extern int siw_touch_i2c_add_driver(void *data);
extern int siw_touch_i2c_del_driver(void *data);

#endif	/* __SIW_TOUCH_BUS_I2C_H */

