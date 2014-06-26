/*
 * include/media/lm3559.h
 *
 * Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
#ifndef _LM3559_H_
#define _LM3559_H_

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>

#define LM3559_NAME    "lm3559"
#define LM3560_NAME    "lm3560"

#define	v4l2_queryctrl_entry_integer(_id, _name,\
		_minimum, _maximum, _step, \
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.name = _name, \
		.minimum = (_minimum), \
		.maximum = (_maximum), \
		.step = (_step), \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}
#define	v4l2_queryctrl_entry_boolean(_id, _name,\
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_BOOLEAN, \
		.name = _name, \
		.minimum = 0, \
		.maximum = 1, \
		.step = 1, \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}

#define	s_ctrl_id_entry_integer(_id, _name, \
		_minimum, _maximum, _step, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_integer(_id, _name,\
				_minimum, _maximum, _step,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

#define	s_ctrl_id_entry_boolean(_id, _name, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_boolean(_id, _name,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

/* Value settings for Flash Time-out Duration*/
#define LM3559_DEFAULT_TIMEOUT          480U
#define LM3559_DEFAULT_TIMEOUT_SETTING 0x07  /* 480ms */
#define LM3559_MIN_TIMEOUT              32U
#define LM3559_MAX_TIMEOUT              1024U
#define LM3559_TIMEOUT_STEPSIZE         32U

/* Flash modes */
#define LM3559_MODE_SHUTDOWN            0
#define LM3559_MODE_INDICATOR           1
#define LM3559_MODE_TORCH               2
#define LM3559_MODE_FLASH               3

/* timer delay time */
#define LM3559_TIMER_DELAY		5

/* Percentage <-> value macros */
#define LM3559_MIN_PERCENT                   0U
#define LM3559_MAX_PERCENT                   100U
#define LM3559_CLAMP_PERCENTAGE(val) \
	clamp(val, LM3559_MIN_PERCENT, LM3559_MAX_PERCENT)
/* we add 1 to the value to end up in the range [min..100%]
 * rather than in the range [0..max] where max < 100 */
#define LM3559_VALUE_TO_PERCENT(v, step)     ((((v)+1) * (step)) / 100)
/* we subtract 1 from the percentage to make sure we round down into
 * the valid range of [0..max] and not [1..max+1] */
#define LM3559_PERCENT_TO_VALUE(p, step)     ((((p)-1) * 100) / (step))

/* Flash brightness in percentage */
#define LM3559_FLASH_DEFAULT_BRIGHTNESS		100

/* Torch brightness, input is percentage, output is [0..7] */
#define LM3559_TORCH_STEP                    1250
#define LM3559_TORCH_DEFAULT              2
#define LM3559_TORCH_DEFAULT_BRIGHTNESS \
	LM3559_VALUE_TO_PERCENT(LM3559_TORCH_DEFAULT, LM3559_TORCH_STEP)

/* Indicator brightness, input is percentage, output is [0..7] */
#define LM3559_INDICATOR_STEP                1250
#define LM3559_INDICATOR_DEFAULT          1
#define LM3559_INDICATOR_DEFAULT_BRIGHTNESS \
	LM3559_VALUE_TO_PERCENT(LM3559_INDICATOR_DEFAULT, LM3559_INDICATOR_STEP)

/*
 * lm3559_platform_data - Flash controller platform data
 */
struct lm3559_platform_data {
	int gpio_torch;
	int gpio_strobe;
	int gpio_reset;

	unsigned int current_limit;
	unsigned int envm_tx2;
	unsigned int tx2_polarity;
	unsigned int flash_current_limit;
	unsigned int disable_tx2;
};

#endif /* _LM3559_H_ */

