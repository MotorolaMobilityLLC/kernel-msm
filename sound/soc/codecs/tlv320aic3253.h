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

#ifndef _TLV320AIC3253_H
#define _TLV320AIC3253_H

/* tlv320AIC3253 register space */

#define AIC3253_PAGE_SIZE		128
#define AIC3253_PAGE_INDX		0

/* This defines the data start byte index in the page memory, while
	doing bulk i2c writes to the mini DSP memory this will be used */

#define AIC3253_PAYLOAD_START	1

#define AIC3253_PSEL		0
#define AIC3253_RESET		1
#define AIC3253_CLKMUX		4
#define AIC3253_PLLPR		5
#define AIC3253_PLLJ		6
#define AIC3253_PLLDMSB		7
#define AIC3253_PLLDLSB		8
#define AIC3253_NDAC		11
#define AIC3253_MDAC		12
#define AIC3253_DOSRMSB		13
#define AIC3253_DOSRLSB		14
#define AIC3253_DSPINT		17
#define AIC3253_NADC		18
#define AIC3253_MADC		19
#define AIC3253_AOSR		20
#define AIC3253_DSPDEC		23
#define AIC3253_CLKMUX2		25
#define AIC3253_CLKOUTM		26
#define AIC3253_IFACE1		27
#define AIC3253_IFACE2		28
#define AIC3253_IFACE3		29
#define AIC3253_BCLKN		30
#define AIC3253_IFACE4		31
#define AIC3253_IFACE5		32
#define AIC3253_IFACE6		33
#define AIC3253_DOUTCTL		53
#define AIC3253_DINCTL		54
#define AIC3253_DACSPB		60
#define AIC3253_ADCSPB		61
#define AIC3253_DACSETUP	63
#define AIC3253_DACMUTE		64
#define AIC3253_LDACVOL		65
#define AIC3253_RDACVOL		66
#define AIC3253_ADCSETUP	81
#define AIC3253_ADCFGA		82
#define AIC3253_LADCVOL		83
#define AIC3253_RADCVOL		84

/* Page 1 registers */
#define AIC3253_PWRCFG		(AIC3253_PAGE_SIZE + 1)
#define AIC3253_LDOCTL		(AIC3253_PAGE_SIZE + 2)
#define AIC3253_LPLYCFG1	(AIC3253_PAGE_SIZE + 3)
#define AIC3253_RPLYCFG2	(AIC3253_PAGE_SIZE + 4)
#define AIC3253_OUTPWRCTL	(AIC3253_PAGE_SIZE + 9)
#define AIC3253_CMMODE		(AIC3253_PAGE_SIZE + 10)
#define AIC3253_HPLROUTE	(AIC3253_PAGE_SIZE + 12)
#define AIC3253_HPRROUTE	(AIC3253_PAGE_SIZE + 13)
#define AIC3253_HPLGAIN		(AIC3253_PAGE_SIZE + 16)
#define AIC3253_HPRGAIN		(AIC3253_PAGE_SIZE + 17)
#define AIC3253_HEADSTART	(AIC3253_PAGE_SIZE + 20)
#define AIC3253_MICBIAS		(AIC3253_PAGE_SIZE + 51)
#define AIC3253_LMICPGAPIN	(AIC3253_PAGE_SIZE + 52)
#define AIC3253_PDMIN		(AIC3253_PAGE_SIZE + 54)
#define AIC3253_PDMCLK		(AIC3253_PAGE_SIZE + 55)

/* Page 44 register */
#define AIC3253_DACFLT	(AIC3253_PAGE_SIZE*44 + 1)

#define AIC3253_SYSCLK_FREQ 12288000

#define AIC3253_WORD_LEN_16BITS		0x00
#define AIC3253_WORD_LEN_20BITS		0x01
#define AIC3253_WORD_LEN_24BITS		0x02
#define AIC3253_WORD_LEN_32BITS		0x03

#define AIC3253_I2S_MODE		0x00
#define AIC3253_DSP_MODE		0x01
#define AIC3253_RIGHT_JUSTIFIED_MODE	0x02
#define AIC3253_LEFT_JUSTIFIED_MODE	0x03

#define AIC3253_PWR_AVDD_DVDD_WEAK_DISABLE	0x00000002
#define AIC3253_PWR_AIC3253_LDO_ENABLE		0x00000004

#define AIC3253_AVDDWEAKDISABLE		0x08
#define AIC3253_LDOCTLEN		0x01

#define AIC3253_LDOIN_18_36		0x01
#define AIC3253_LDOIN2HP		0x02

#define AIC3253_PLLJ_SHIFT		6
#define AIC3253_DOSRMSB_SHIFT		4

#define AIC3253_PLLCLKIN		0x03

#define AIC3253_ADCSETUP_PDM	(0xDC)

#define AIC3253_BCLKMASTER             0x08
#define AIC3253_WCLKMASTER             0x04

#define AIC3253_PLLEN			(0x01 << 7)
#define AIC3253_NDACEN			(0x01 << 7)
#define AIC3253_MDACEN			(0x01 << 7)
#define AIC3253_NADCEN			(0x01 << 7)
#define AIC3253_MADCEN			(0x01 << 7)
#define AIC3253_BCLKEN			(0x01 << 7)
#define AIC3253_DACEN			(0x03 << 6)
#define AIC3253_RDAC2LCHN		(0x02 << 2)
#define AIC3253_LDAC2RCHN		(0x02 << 4)
#define AIC3253_LDAC2LCHN		(0x01 << 4)
#define AIC3253_RDAC2RCHN		(0x01 << 2)
#define AIC3253_DACFLTEN		(0x01 << 2)
#define AIC3253_SSTEP2WCLK		0x01
#define AIC3253_MUTEON			0x0C
#define AIC3253_DACMOD2BCLK		0x01
#define AIC3253_DSPDEN          0x00
#define AIC3253_DACFLTEN        (0x01 << 2)
#define AIC3253_PDMINEN         (0x01 << 2)
#define AIC3253_PDMCLKEN		(0x07 << 1)


extern const char aic3253_dac_coeffs1[];
extern const char aic3253_dac_coeffs2[];
extern const char aic3253_dac_coeffs3[];
extern const char aic3253_dac_coeffs4[];
extern const char aic3253_dac_instr1[];
extern const char aic3253_dac_instr2[];
extern const char aic3253_dac_instr3[];
extern const char aic3253_dac_instr4[];
extern const char aic3253_dac_instr5[];
extern const char aic3253_dac_instr6[];

#endif				/* _TLV320AIC3253_H */
