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

#ifndef __TPA6165_CORE_H__
#define __TPA6165_CORE_H__

#include <linux/ioctl.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/tlv.h>

/** Register map */

#define TPA6165_DEVICE_STATUS_REG1		0x00
#define TPA6165_DEVICE_STATUS_REG2		0x01
#define TPA6165_DEVICE_STATUS_REG3		0x02
#define TPA6165_INT_MASK_REG1			0x04
#define TPA6165_INT_MASK_REG2			0x05
#define TPA6165_HPH_VOL_CTL_REG1		0x07
#define TPA6165_HPH_VOL_CTL_REG2		0x08
#define TPA6165_MICB_CTL_REG			0x09
#define TPA6165_DEV_REV_ID_REG			0x0B
#define TPA6165_RSVD_DEF_REG1			0x12
#define TPA6165_RSVD_DEF_REG2			0x13
#define TPA6165_KEYSCAN_DEBOUNCE_REG		0x15
#define TPA6165_KEYSCAN_DELAY_REG		0x16
#define TPA6165_MB_KEYSCAN_REG			0x17
#define TPA6165_JACK_DETECT_TEST_HW1		0x18
#define TPA6165_ACC_STATE_REG			0x19
#define TPA6165_JACK_DETECT_TEST_HW2		0x1A
#define TPA6165_CLK_CTL			0x1C
#define TPA6165_ENABLE_REG1			0x1D
#define TPA6165_ENABLE_REG2			0x1E

/* Number of registers */

#define TPA6165_MAX_REGISTER_VAL 0x1F

/* jack Detect Mask */
#define TPA6165_JACK_DETECT  0x80

/* jack sense mask */
#define TPA6165_JACK_SENSE  0x04

/* device reset */
#define TPA6165_DEVICE_RESET 0x08

/* jack detect config */
#define TPA6165_JACK_SHORT_Z  0x80
#define TPA6165_JACK_HP_LO_TH  0x40

/* single button press mask */
#define TPA6165_JACK_BUTTON  0x02

/* multi button press mask */
#define TPA6165_MULTI_BUTTON  0x04

/* mic in mask */
#define TPA6165_MIC_IN_MASK  0x08

/* config det done mask */
#define TPA6165_DET_DONE  0x40

/* wake up the device from lowest power state, shutdown mask */
#define TPA6165_SHUTDOWN_DISABLE  0x0

/* sleep state mask */
#define TPA6165_SLEEP_ENABLE  (1<<6)

/* mute/unmute mask */
#define TPA6165_MUTE (1<<6)

/* vol slew interrupt mask */
#define TPA6165_VOL_SLEW_INT (1<<5)

/* key interrupt mask */
#define TPA6165_KEY_INT (1<<2)

/* mic switch interrupt mask */
#define TPA6165_MICSW_INT (1<<1)

/* vol slew done */
#define TPA6165_VOL_SLEW_DONE (1<<5)

/* left/right channels mask */
#define TPA6165_LEFT_RIGHT_CH (0x3<<6)

/* mic en mask */
#define TPA6165_MIC_AMP_EN (0x1 << 3)

/* mic bias en mask */
#define TPA6165_MIC_BIAS_EN (0x1 << 4)

/* auto mode mask */
#define TPA6165_AUTO_MODE (0x3)

/* button press release mask */
#define TPA6165_PRESS (0x1 << 1)

/* Accesory Types Masks */

/* Nothing Inserted */
#define TPA6165_NO_ACC_DET1  0x00
#define TPA6165_NO_ACC_DET2  0x1D
#define TPA6165_NO_ACC_DET3  0x21

/* Stereo Headset with Mic on sleeve */
#define TPA6165_STEREO_MIC_0N_SLEEVE  0x01

/* Stereo Headset with Mic on Ring two */
#define TPA6165_STEREO_MIC_0N_RING  0x02

/* Mono Headset with Mic on Sleeve */
#define TPA6165_MONO_MIC_0N_SLEEVE1  0x0A
#define TPA6165_MONO_MIC_0N_SLEEVE2  0x0C
#define TPA6165_MONO_MIC_0N_SLEEVE3  0x0E

/* Mono Headset with Mic on Ring two */
#define TPA6165_MONO_MIC_0N_RING1  0x0B
#define TPA6165_MONO_MIC_0N_RING2  0x0D
#define TPA6165_MONO_MIC_0N_RING3  0x0F

/* Stereo Headphone */
#define TPA6165_STEREO_HEADPHONE1  0x10
#define TPA6165_STEREO_HEADPHONE2  0x11
#define TPA6165_STEREO_HEADPHONE3  0x12

/* Stereo Lineout */
#define TPA6165_STEREO_LINEOUT1  0x1A
#define TPA6165_STEREO_LINEOUT2  0x1B
#define TPA6165_STEREO_LINEOUT3  0x1C
#define TPA6165_STEREO_LINEOUT4  0x1F

/*Force acc type */
#define TPA6165_FORCE_TYPE  (1<<7)

/* amp states */
#define TPA6165_AMP_DISABLED	0
#define TPA6165_AMP_ENABLED		1

/* mic states */
#define TPA6165_MIC_DISABLED	0
#define TPA6165_MIC_ENABLED		1

/* power states */
#define TPA6165_SPECIAL_SLEEP	2
#define TPA6165_SLEEP			1
#define TPA6165_WAKEUP			0

struct tpa6165_regs {
	u8 reg;
	u8 value;
};

extern int tpa6165_hs_detect(struct snd_soc_codec *codec);

extern void tpa6165_hp_event(int event);
extern void tpa6165_mic_event(int event);
#endif  /* __TPA6165_CORE_H__ */
