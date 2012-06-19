/* include/sound/tpa2028d.h
 *
 * Copyright (C) 2009 LGE, Inc.
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

/*
 * Revision 0.1 -- BJB -- 3/18/10 -- Original Version
 * Revision 0.2 -- BJB -- 3/19/10 -- Corrected dB to DB in Register 0x01, Added function prototypes.
 * Revision 0.3 -- AME -- 5/17/10 -- No changes to TPA2055D3_FDK.h file.
 *
 */

/* ---- I2C ADDR -----*/
#define IC_CONTROL		(char) 0x01
#define AGC_ATTACK_CONTROL	(char) 0x02
#define AGC_RELEASE_CONTROL	(char) 0x03
#define AGC_HOLD_TIME_CONTROL	(char) 0x04
#define AGC_FIXED_GAIN_CONTROL	(char) 0x05
#define AGC1_CONTROL		(char) 0x06
#define AGC2_CONTROL		(char) 0x07

struct amp_cal {
	u8 dev_type;
	u8 gain_type;
	u8 data;
};

void set_amp_gain(int num);

struct audio_amp_platform_data {
	int (*enable)(int);
	int (*power)(bool);
	char agc_compression_rate;
	char agc_output_limiter_disable;
	char agc_fixed_gain;
};

/* SPK FUNCTION */
#define SPK_ON 1
#define SPK_OFF 0
