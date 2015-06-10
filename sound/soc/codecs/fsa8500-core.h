/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __FSA8500_CORE_H__
#define __FSA8500_CORE_H__

#include <linux/ioctl.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/tlv.h>

/** Register map */

#define FSA8500_DEVICE_ID		0x01
#define FSA8500_INT_REG1		0x02
#define FSA8500_INT_REG2		0x03
#define FSA8500_INT_REG3		0x04
#define FSA8500_INT_REG4		0x05
#define FSA8500_INT_REG5		0x06
#define FSA8500_INT_MASK_REG1		0x07
#define FSA8500_INT_MASK_REG2		0x08
#define FSA8500_INT_MASK_REG3		0x09
#define FSA8500_INT_MASK_REG4		0x0A
#define FSA8500_INT_MASK_REG5		0x0B

#define FSA8500_J_DET_T			0x0C
#define FSA8500_KEY_T_AMP_GAIN		0x0D
#define FSA8500_MP3_MODE_T		0x0E
#define FSA8500_DETECTION_T		0x0F
#define FSA8500_DEBOUNCE_T		0x10
#define FSA8500_CONTROL1		0x11
#define FSA8500_CONTROL2		0x12
#define FSA8500_DETECTION_TH1		0x13
#define FSA8500_DETECTION_TH2		0x14
#define FSA8500_RESET_CONTROL		0x15
#define FSA8500_RESERVED_REG		0x16

#define FSA8500_ACCESSORY_1		0x01
#define FSA8500_ACCESSORY_2		0x02
#define FSA8500_ACCESSORY_3		0x04

/* Number of registers */

#define FSA8500_MAX_REGISTER_VAL	0x17

/* Reset bits */
#define FSA8500_RESET_GLOBAL		0x01
#define FSA8500_RESET_DETECT		0x02
#define FSA8500_RESET			(FSA8500_RESET_GLOBAL| \
					FSA8500_RESET_DETECT)

/* Disable UART detection bit */
#define FSA8500_UART_OFF		0x08

/* Disable UART detection bit */
#define FSA8500_LINT_OFF		0x02

/* amp states */
#define FSA8500_AMP_DISABLED		0
#define FSA8500_AMP_ENABLED		1

/* mic states */
#define FSA8500_MIC_DISABLED		0
#define FSA8500_MIC_ENABLED		1

/* number supported keys */
#define FSA8500_NUM_KEYS		7

extern int fsa8500_hs_detect(struct snd_soc_codec *codec);

extern void fsa8500_hp_event(int event);
extern void fsa8500_mic_event(int event);
#endif  /* __FSA8500_CORE_H__ */
