
/*
 * Copyright (C) 2012 Motorola Mobility, LLC.
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

#ifndef __TFA9890_CORE_H__
#define __TFA9890_CORE_H__

#include <linux/ioctl.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/tlv.h>

/* Register Map */

#define TFA9890_SYS_STATUS_REG		0x00
#define TFA9890_BATT_STATUS_REG		0x01
#define TFA9890_TEMP_STATUS_REG		0x02
#define TFA9890_REV_ID				0x03
#define TFA9890_I2S_CTL_REG			0x04
#define TFA9890_BATT_CTL_REG		0x05
#define TFA9890_VOL_CTL_REG			0x06
#define TFA9890_DC2DC_CTL_REG		0x07
#define TFA9890_SPK_CTL_REG			0x08
#define TFA9890_SYS_CTL1_REG		0x09
#define TFA9890_SYS_CTL2_REG		0x0A
#define TFA9890_MTP_KEY_REG			0x0b
#define TFA9890_LDO_REG				0x0e
#define TFA9890_PWM_CTL_REG			0x41
#define TFA9890_CURRT_CTL_REG		0x46
#define TFA9890_CURRT_SNS1_REG		0x47
#define TFA9890_CURRT_SNS2_REG		0x48
#define TFA9890_CURRC_SNS_REG		0x49
#define TFA9890_DEM_CTL_REG			0x4E
#define TFA9890_MTP_COPY_REG		0x62
#define TFA9890_CF_CONTROLS			0x70
#define TFA9890_CF_MAD				0x71
#define TFA9890_CF_MEM				0x72
#define TFA9890_CF_STATUS			0x73
#define TFA9890_MTP_REG				0x80

#define TFA9890_REG_CACHE_SIZE	129

/* Device Revision ID */
#define TFA9890_DEV_ID			0x0014

/* DSP Module IDs */
#define TFA9890_DSP_MOD_FRAMEWORK		0
#define TFA9890_DSP_MOD_SPEAKERBOOST		1
#define TFA9890_DSP_MOD_BIQUADFILTERBANK	2

/* RPC command ID's*/
#define TFA9890_PARAM_SET_LSMODEL	0x06
#define TFA9890_PARAM_SET_EQ	0x0A
#define TFA9890_PARAM_SET_PRESET	0x0D
#define TFA9890_PARAM_SET_CONFIG	0x0E
#define TFA9890_PARAM_SET_DBDRC		0x0F
#define TFA9890_PARAM_SET_AGCINS	0x10

/* Gets the speaker calibration impedance (@25 degrees celsius) */
#define TFA9890_PARAM_GET_RE0          0x85
/* Gets current LoudSpeaker Model. */
#define TFA9890_PARAM_GET_LSMODEL      0x86
#define TFA9890_PARAM_GET_STATE        0xC0
#define TFA9890_PARAM_GET_LSMODELW		0xC1
#define TFA9890_FW_PARAM_GET_STATE		0x84
#define TFA9890_PARAM_GET_FEATURE_BITS 0x85
#define TFA9890_PARAM_GET_CFGPST		0x80


/* DSP Firmware Size in bytes*/
#define TFA9890_SPK_FW_SIZE			424
#define TFA9890_CFG_FW_SIZE			166
#define TFA9890_PST_FW_SIZE			88
#define TFA9890_COEFF_FW_SIZE		181
#define TFA9890_DEV_STATE_SIZE		24
#define TFA9890_SPK_EX_FW_SIZE		424

/* DSP Write/Read */
#define TFA9890_DSP_WRITE			0
#define TFA9890_DSP_READ			1

/* RPC Status results */
#define TFA9890_STATUS_OK			0
#define TFA9890_INVALID_MODULE_ID	2
#define TFA9890_INVALID_PARAM_ID	3
#define TFA9890_INVALID_INFO_ID		4

/* memory address to be accessed on dsp
 * (0 : Status, 1 : ID, 2 : parameters) */
#define TFA9890_CF_MAD_STATUS	0x0
#define TFA9890_CF_MAD_ID	0x1
#define TFA9890_CF_MAD_PARAM	0x2

/* DSP control Register */
#define TFA9890_CF_CTL_MEM_REQ	0x2
#define TFA9890_CF_INT_REQ	(1 << 4)
#define TFA9890_CF_ACK_REQ	(1 << 8)

/* Status Masks */
#define TFA9890_SYSCTRL_CONFIGURED	(1<<5)
#define TFA9890_STATUS_PLLS			(1<<1)
#define TFA9890_STATUS_CLKS			(1<<6)
#define TFA9890_STATUS_MTPB			(1<<8)
#define TFA9890_STATUS_CIMTP		(1<<11)
#define TFA9890_STATUS_MTPEX		(1<<1)
#define TFA9890_STATUS_ACS			(1<<11)
#define TFA9890_STATUS_DC2DC		(1<<4)
#define TFA9890_STATUS_AMPE			(1<<3)
#define TFA9890_STATUS_MUTE			(1<<5)
#define TFA9890_STATUS_CF			(1<<8)
#define TFA9890_STATUS_TEMP			(0x1ff)
#define TFA9890_STATUS_VDDS			(0x1)
#define TFA9890_STATUS_AMP_SWS			(1<<12)
#define TFA9890_STATUS_SPKS			(1<<10)
#define TFA9890_STATUS_WDS			(1<<13)
#define TFA9890_STATUS_ARFS			(1<<15)

/* Params masks */
#define TFA9890_I2S_FORMAT		(0x8)
#define TFA887_SAMPLE_RATE		(0xf << 12)
#define TFA9890_SAMPLE_RATE_11k	(0x1 << 12)
#define TFA9890_SAMPLE_RATE_12k (0x2 << 12)
#define TFA9890_SAMPLE_RATE_16k (0x3 << 12)
#define TFA9890_SAMPLE_RATE_22k (0x4 << 12)
#define TFA9890_SAMPLE_RATE_24k (0x5 << 12)
#define TFA9890_SAMPLE_RATE_32k (0x6 << 12)
#define TFA9890_SAMPLE_RATE_44k (0x7 << 12)
#define TFA9890_SAMPLE_RATE_48k (0x8 << 12)
#define TFA9890_FORMAT_MASK		(0x8)
#define TFA9890_FORMAT_I2S		(0x6)
#define TFA9890_FORMAT_LSB		(0x4)
#define TFA9890_FORMAT_MSB		(0x2)
#define TFA9890_PWM_MASK		(0xf << 1)
#define TFA9890_PWM_VAL			(0x7 << 1)
#define TFA9890_POWER_DOWN		(0x1)
#define TFA9890_MTPOTC			(0x1)
#define TFA9890_I2S_CHS12		(0x3 << 3)
#define TFA9890_DOLS_DATAO		(0x7)
#define TFA9890_DORS_DATAO		(0x7 << 3)

/* enable I2S left channel input */
#define TFA9890_I2S_LEFT_IN		(0x1 << 3)
/* enable I2S right channel input */
#define TFA9890_I2S_RIGHT_IN	(0x2 << 3)
/* route gain info on datao pin on left channel */
#define TFA9890_DOLS_GAIN		(0x1)
/* route gain info on datao pin on right channel */
#define TFA9890_DORS_GAIN		(0x1 << 3)



/* Mute States */
#define TFA9890_MUTE_OFF	0
#define TFA9890_DIGITAL_MUTE	1
#define TFA9890_AMP_MUTE	2

/* MTP memory unlock key */
#define TFA9890_MTK_KEY		0x005A

/* DSP init status */
#define TFA9890_DSP_INIT_PENDING	0
#define TFA9890_DSP_INIT_DONE	1
#define TFA9890_DSP_INIT_FAIL	-1

/* Spkr impedence exponent, used for converting to Fixed pt
 *value */
#define TFA9890_SPKR_IMP_EXP	9
#define TFA9890_MAX_I2C_SIZE	252

/* IC vesion definitions */
#define TFA9890_N1B4		0x0
#define TFA9890_N1B12		0x1
#define TFA9890_N1C2		0x2

/* DSP patch header lenght */
#define TFA9890_PATCH_HEADER		0x6

/* Version type n1b2 masks and values */
#define TFA9890_N1B12_VER1_MASK		0x0003
#define TFA9890_N1B12_VER3_MASK		0xfe00

#define TFA9890_N1B12_VER1_VAL		0x0002
#define TFA9890_N1B12_VER2_VAL		0x4c64
#define TFA9890_N1B12_VER3_VAL		0xa000

struct tfa9890_regs {
	int reg;
	int value;
};

#endif
