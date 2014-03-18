/*
 *
 * Id: stk3x1x_moto.h
 *
 * Copyright (C) 2012~2013 Lex Hsieh     <lex_hsieh@sensortek.com.tw>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 */
#ifndef __STK3X1X_MOTO_H__
#define __STK3X1X_MOTO_H__

/* platform data */
struct stk3x1x_platform_data {
	uint8_t state_reg;
	uint8_t psctrl_reg;
	uint8_t alsctrl_reg;
	uint8_t ledctrl_reg;
	uint8_t	wait_reg;
	int int_pin;
	uint32_t transmittance;
	uint32_t int_flags;
	uint16_t als_thresh_pct;
	uint16_t covered_thresh;
	uint16_t uncovered_thresh;
	uint16_t recal_thresh;
	uint16_t max_noise_fl;
};


#endif /* __STK3X1X_MOTO_H__ */
