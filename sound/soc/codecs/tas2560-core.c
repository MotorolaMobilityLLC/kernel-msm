/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2560-core.c
**
** Description:
**     TAS2560 common functions for Android Linux
**
** =============================================================================
*/
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas2560.h"

#define TAS2560_MDELAY 0xFFFFFFFE
#define TAS2560_MSLEEP 0xFFFFFFFD

#define MAX_CLIENTS 8

static unsigned int p_tas2560_default_data[] = {
	/* reg address			size	values	*/
	TAS2560_DR_BOOST_REG_1,	0x01,	0x0c,
	TAS2560_DR_BOOST_REG_2,	0x01,	0x33,
	/*rampDown 15dB/us, clock1 hysteresis, 0.34ms; clock2 hysteresis, 10.6us */
	TAS2560_CLK_ERR_CTRL2,	0x01,	0x21,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_irq_config[] = {
	TAS2560_IRQ_PIN_REG,	0x01,	0x41,
	TAS2560_INT_MODE_REG,	0x01,	0x80,/* active high until INT_STICKY_1 and INT_STICKY_2 are read to be cleared. */
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_48kSR_BCLK_1P536_data[] = {
	TAS2560_CLK_SEL,		0x01,	0x01,	/* BCLK in, P = 1 */
	TAS2560_SET_FREQ,		0x01,	0x20,	/* J = 32 */
	TAS2560_PLL_D_MSB,		0x01,	0x00,	/* D = 0 */
	TAS2560_PLL_D_LSB,		0x01,	0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_48kSR_BCLK_3P072_data[] = {
	TAS2560_CLK_SEL,		0x01,	0x01,	/* BCLK in, P = 1 */
	TAS2560_SET_FREQ,		0x01,	0x10,	/* J = 16 */
	TAS2560_PLL_D_MSB,		0x01,	0x00,	/* D = 0 */
	TAS2560_PLL_D_LSB,		0x01,	0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_startup_data[] = {
	/* reg address			size	values	*/
	TAS2560_DEV_MODE_REG,	0x01,	0x02,
	TAS2560_MUTE_REG,		0x01,	0x41,
	TAS2560_MDELAY,			0x01,	0x02,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_unmute_data[] = {
	/* reg address			size	values	*/
	TAS2560_CLK_ERR_CTRL,	0x01,	0x0b,
	TAS2560_INT_GEN_REG,	0x01,	0xff,
	TAS2560_MUTE_REG,		0x01,	0x40,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_shutdown_data[] = {
	/* reg address			size	values	*/
	TAS2560_INT_GEN_REG,	0x01,	0x00,
	TAS2560_CLK_ERR_CTRL,	0x01,	0x00,
	TAS2560_MUTE_REG,		0x01,	0x41,
	TAS2560_MSLEEP,			0x01,	10,	/* delay 10ms */
	TAS2560_MUTE_REG,		0x01,	0x01,
	TAS2560_MSLEEP,			0x01,	20,	/* delay 20ms */
	TAS2560_DEV_MODE_REG,	0x01,	0x01,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_boost_headroom_data[] = {
	TAS2560_BOOST_HEAD,		0x04,	0x06, 0x66, 0x66, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_thermal_foldback[] = {
	TAS2560_THERMAL_FOLDBACK,		0x04,	0x39, 0x80, 0x00, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

/*
* PPG = dec2hex(10^(<required dB>/20)*2^31))
*/
static unsigned int p_tas2560_PPG_data[] = {
	TAS2560_PPG,	0x04,	0x65, 0xac, 0x8c, 0x2e,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_idle_chnl_detect[] = {
	TAS2560_IDLE_CHNL_DETECT,	0x04,	0, 0, 0, 0,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_HPF_data[] = {
	/* reg address			size	values */
	/*Isense path HPF cut off -> 2Hz*/
	TAS2560_ISENSE_PATH_CTL1,	0x04,	0x7F, 0xFB, 0xB5, 0x00,
	TAS2560_ISENSE_PATH_CTL2,	0x04,	0x80, 0x04, 0x4C, 0x00,
	TAS2560_ISENSE_PATH_CTL3,	0x04,	0x7F, 0xF7, 0x6A, 0x00,
	/*all pass*/
	TAS2560_HPF_CUTOFF_CTL1,	0x04,	0x7F, 0xFF, 0xFF, 0xFF,
	TAS2560_HPF_CUTOFF_CTL2,	0x04,	0x00, 0x00, 0x00, 0x00,
	TAS2560_HPF_CUTOFF_CTL3,	0x04,	0x00, 0x00, 0x00, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_Vsense_biquad_data[] = {
	/* vsense delay in biquad = 3/8 sample @48KHz */
	TAS2560_VSENSE_DEL_CTL1,	0x04,	0x3a, 0x46, 0x74, 0x00,
	TAS2560_VSENSE_DEL_CTL2,	0x04,	0x22, 0xf3, 0x07, 0x00,
	TAS2560_VSENSE_DEL_CTL3,	0x04,	0x80, 0x77, 0x61, 0x00,
	TAS2560_VSENSE_DEL_CTL4,	0x04,	0x22, 0xa7, 0xcc, 0x00,
	TAS2560_VSENSE_DEL_CTL5,	0x04,	0x3a, 0x0c, 0x93, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_48khz_data[] = {
	/* reg address			size	values */
	TAS2560_SR_CTRL1,		0x01,	0x01,
	TAS2560_SR_CTRL2,		0x01,	0x08,
	TAS2560_SR_CTRL3,		0x01,	0x10,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_16khz_data[] = {
	/* reg address			size	values */
	TAS2560_SR_CTRL1,		0x01,	0x01,
	TAS2560_SR_CTRL2,		0x01,	0x18,
	TAS2560_SR_CTRL3,		0x01,	0x20,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_8khz_data[] = {
	/* reg address			size	values */
	TAS2560_SR_CTRL1,		0x01,	0x01,
	TAS2560_SR_CTRL2,		0x01,	0x30,
	TAS2560_SR_CTRL3,		0x01,	0x20,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_4Ohm_data[] = {
	/* reg address			size	values */
	TAS2560_BOOST_ON,		0x04,	0x6f, 0x5c, 0x28, 0xf5,
	TAS2560_BOOST_OFF,		0x04,	0x67, 0xae, 0x14, 0x7a,
	TAS2560_BOOST_TABLE_CTRL1,	0x04,	0x1c, 0x00, 0x00, 0x00,
	TAS2560_BOOST_TABLE_CTRL2,	0x04,	0x1f, 0x0a, 0x3d, 0x70,
	TAS2560_BOOST_TABLE_CTRL3,	0x04,	0x22, 0x14, 0x7a, 0xe1,
	TAS2560_BOOST_TABLE_CTRL4,	0x04,	0x25, 0x1e, 0xb8, 0x51,
	TAS2560_BOOST_TABLE_CTRL5,	0x04,	0x28, 0x28, 0xf5, 0xc2,
	TAS2560_BOOST_TABLE_CTRL6,	0x04,	0x2b, 0x33, 0x33, 0x33,
	TAS2560_BOOST_TABLE_CTRL7,	0x04,	0x2e, 0x3d, 0x70, 0xa3,
	TAS2560_BOOST_TABLE_CTRL8,	0x04,	0x31, 0x47, 0xae, 0x14,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_6Ohm_data[] = {
	/* reg address			size	values */
	TAS2560_BOOST_ON,		0x04,	0x73, 0x33, 0x33, 0x33,
	TAS2560_BOOST_OFF,		0x04,	0x6b, 0x85, 0x1e, 0xb8,
	TAS2560_BOOST_TABLE_CTRL1,	0x04,	0x1d, 0x99, 0x99, 0x99,
	TAS2560_BOOST_TABLE_CTRL2,	0x04,	0x20, 0xcc, 0xcc, 0xcc,
	TAS2560_BOOST_TABLE_CTRL3,	0x04,	0x24, 0x00, 0x00, 0x00,
	TAS2560_BOOST_TABLE_CTRL4,	0x04,	0x27, 0x33, 0x33, 0x33,
	TAS2560_BOOST_TABLE_CTRL5,	0x04,	0x2a, 0x66, 0x66, 0x66,
	TAS2560_BOOST_TABLE_CTRL6,	0x04,	0x2d, 0x99, 0x99, 0x99,
	TAS2560_BOOST_TABLE_CTRL7,	0x04,	0x30, 0xcc, 0xcc, 0xcc,
	TAS2560_BOOST_TABLE_CTRL8,	0x04,	0x34, 0x00, 0x00, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static unsigned int p_tas2560_8Ohm_data[] = {
	/* reg address			size	values */
	TAS2560_BOOST_ON,		0x04,	0x75, 0xc2, 0x8e, 0x00,
	TAS2560_BOOST_OFF,		0x04,	0x6e, 0x14, 0x79, 0x00,
	TAS2560_BOOST_TABLE_CTRL1,	0x04,	0x1e, 0x00, 0x00, 0x00,
	TAS2560_BOOST_TABLE_CTRL2,	0x04,	0x21, 0x3d, 0x71, 0x00,
	TAS2560_BOOST_TABLE_CTRL3,	0x04,	0x24, 0x7a, 0xe1, 0x00,
	TAS2560_BOOST_TABLE_CTRL4,	0x04,	0x27, 0xb8, 0x52, 0x00,
	TAS2560_BOOST_TABLE_CTRL5,	0x04,	0x2a, 0xf5, 0xc3, 0x00,
	TAS2560_BOOST_TABLE_CTRL6,	0x04,	0x2e, 0x33, 0x33, 0x00,
	TAS2560_BOOST_TABLE_CTRL7,	0x04,	0x31, 0x70, 0xa4, 0x00,
	TAS2560_BOOST_TABLE_CTRL8,	0x04,	0x34, 0xae, 0x14, 0x00,
	0xFFFFFFFF, 0xFFFFFFFF
};

static int tas2560_i2c_load_data(struct tas2560_priv *pTAS2560, unsigned int *pData)
{
	unsigned int nRegister;
	unsigned int *nData;
	unsigned char Buf[128];
	unsigned int nLength = 0;
	unsigned int i = 0;
	unsigned int nSize = 0;
	int nResult = 0;

	do {
		nRegister = pData[nLength];
		nSize = pData[nLength + 1];
		nData = &pData[nLength + 2];
		if (nRegister == TAS2560_MSLEEP) {
			msleep(nData[0]);
			dev_dbg(pTAS2560->dev, "%s, msleep = %d\n",
				__func__, nData[0]);
		} else if (nRegister == TAS2560_MDELAY) {
			mdelay(nData[0]);
			dev_dbg(pTAS2560->dev, "%s, mdelay = %d\n",
				__func__, nData[0]);
		} else {
			if (nRegister != 0xFFFFFFFF) {
				if (nSize > 128) {
					dev_err(pTAS2560->dev,
						"%s, Line=%d, invalid size, maximum is 128 bytes!\n",
						__func__, __LINE__);
					break;
				}

				if (nSize > 1) {
					for (i = 0; i < nSize; i++)
						Buf[i] = (unsigned char)nData[i];
					nResult = pTAS2560->bulk_write(pTAS2560, nRegister, Buf, nSize);
					if (nResult < 0)
						break;
				} else if (nSize == 1) {
					nResult = pTAS2560->write(pTAS2560, nRegister, nData[0]);
					if (nResult < 0)
						break;
				} else {
					dev_err(pTAS2560->dev,
						"%s, Line=%d,invalid size, minimum is 1 bytes!\n",
						__func__, __LINE__);
				}
			}
		}
		nLength = nLength + 2 + pData[nLength + 1];
	} while (nRegister != 0xFFFFFFFF);

	return nResult;
}

void tas2560_sw_shutdown(struct tas2560_priv *pTAS2560, int sw_shutdown)
{
	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, sw_shutdown);

	if (sw_shutdown)
		pTAS2560->update_bits(pTAS2560, TAS2560_PWR_REG,
			TAS2560_PWR_BIT_MASK, 0);
	else
		pTAS2560->update_bits(pTAS2560, TAS2560_PWR_REG,
			TAS2560_PWR_BIT_MASK, TAS2560_PWR_BIT_MASK);
}

int tas2560_set_SampleRate(struct tas2560_priv *pTAS2560, unsigned int nSamplingRate)
{
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, nSamplingRate);

	switch (nSamplingRate) {
	case 48000:
		tas2560_i2c_load_data(pTAS2560, p_tas2560_48khz_data);
		break;
	case 44100:
		pTAS2560->write(pTAS2560, TAS2560_SR_CTRL1, 0x11);
		break;
	case 16000:
		tas2560_i2c_load_data(pTAS2560, p_tas2560_16khz_data);
		break;
	case 8000:
		tas2560_i2c_load_data(pTAS2560, p_tas2560_8khz_data);
		break;
	default:
		dev_err(pTAS2560->dev, "Invalid Sampling rate, %d\n", nSamplingRate);
		ret = -1;
		break;
	}

	if (ret >= 0)
		pTAS2560->mnSamplingRate = nSamplingRate;

	return ret;
}

int tas2560_set_bit_rate(struct tas2560_priv *pTAS2560, unsigned int nBitRate)
{
	int ret = 0, n = -1;

	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, nBitRate);
	switch (nBitRate) {
	case 16:
		n = 0;
	break;
	case 20:
		n = 1;
	break;
	case 24:
		n = 2;
	break;
	case 32:
		n = 3;
	break;
	}

	if (n >= 0)
		ret = pTAS2560->update_bits(pTAS2560,
			TAS2560_DAI_FMT, 0x03, n);

	return ret;
}

int tas2560_get_bit_rate(struct tas2560_priv *pTAS2560)
{
	int nBitRate = -1, value = -1, ret = 0;

	ret = pTAS2560->read(pTAS2560, TAS2560_DAI_FMT, &value);
	value &= 0x03;

	switch (value) {
	case 0:
		nBitRate = 16;
	break;
	case 1:
		nBitRate = 20;
	break;
	case 2:
		nBitRate = 24;
	break;
	case 3:
		nBitRate = 32;
	break;
	default:
	break;
	}

	return nBitRate;
}

int tas2560_set_ASI_fmt(struct tas2560_priv *pTAS2560, unsigned int fmt)
{
	u8 serial_format = 0, asi_cfg_1 = 0;
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		asi_cfg_1 = 0x00;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		asi_cfg_1 = TAS2560_WCLKDIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		asi_cfg_1 = TAS2560_BCLKDIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		asi_cfg_1 = (TAS2560_BCLKDIR | TAS2560_WCLKDIR);
		break;
	default:
		dev_err(pTAS2560->dev, "ASI format master is not found\n");
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		asi_cfg_1 |= 0x00;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		asi_cfg_1 |= TAS2560_WCLKINV;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		asi_cfg_1 |= TAS2560_BCLKINV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		asi_cfg_1 = (TAS2560_WCLKINV | TAS2560_BCLKINV);
		break;
	default:
		dev_err(pTAS2560->dev, "ASI format Inverse is not found\n");
		ret = -EINVAL;
	}

	pTAS2560->update_bits(pTAS2560, TAS2560_ASI_CFG_1, TAS2560_DIRINV_MASK | 0x02,
			asi_cfg_1 | 0x02);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case (SND_SOC_DAIFMT_I2S):
		serial_format |= (TAS2560_DATAFORMAT_I2S << TAS2560_DATAFORMAT_SHIFT);
		break;
	case (SND_SOC_DAIFMT_DSP_A):
	case (SND_SOC_DAIFMT_DSP_B):
		serial_format |= (TAS2560_DATAFORMAT_DSP << TAS2560_DATAFORMAT_SHIFT);
		break;
	case (SND_SOC_DAIFMT_RIGHT_J):
		serial_format |= (TAS2560_DATAFORMAT_RIGHT_J << TAS2560_DATAFORMAT_SHIFT);
		break;
	case (SND_SOC_DAIFMT_LEFT_J):
		serial_format |= (TAS2560_DATAFORMAT_LEFT_J << TAS2560_DATAFORMAT_SHIFT);
		break;
	default:
		dev_err(pTAS2560->dev, "DAI Format is not found, fmt=0x%x\n", fmt);
		ret = -EINVAL;
		break;
	}

	pTAS2560->update_bits(pTAS2560, TAS2560_DAI_FMT, TAS2560_DAI_FMT_MASK,
			serial_format);

	return ret;
}

int tas2560_set_pll_clkin(struct tas2560_priv *pTAS2560, int clk_id,
		unsigned int freq)
{
	int ret = 0;
	unsigned char pll_in = 0;

	dev_dbg(pTAS2560->dev, "%s, clkid=%d, freq=%d\n", __func__, clk_id, freq);

	switch (clk_id) {
	case TAS2560_PLL_CLKIN_BCLK:
		pll_in = 0;
		break;
	case TAS2560_PLL_CLKIN_MCLK:
		pll_in = 1;
		break;
	case TAS2560_PLL_CLKIN_PDMCLK:
		pll_in = 2;
		break;
	default:
		dev_err(pTAS2560->dev, "Invalid clk id: %d\n", clk_id);
		ret = -EINVAL;
		break;
	}

	if (ret >= 0) {
		pTAS2560->update_bits(pTAS2560, TAS2560_CLK_SEL,
			TAS2560_PLL_SRC_MASK, pll_in << 6);
		pTAS2560->mnClkid = clk_id;
		pTAS2560->mnClkin = freq;
	}

	return ret;
}

int tas2560_setupPLL(struct tas2560_priv *pTAS2560, int pll_clkin)
{
	unsigned int pll_clk = pTAS2560->mnSamplingRate * 1024;
	unsigned int power = 0, temp;
	unsigned int d, pll_clkin_divide;
	u8 j, p;
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, pll_clkin);

	if (!pll_clkin || (pll_clkin < 0)) {
		if (pTAS2560->mnClkid != TAS2560_PLL_CLKIN_BCLK) {
			dev_err(pTAS2560->dev,
				"pll_in %d, pll_clkin frequency err:%d\n",
				pTAS2560->mnClkid, pll_clkin);
			return -EINVAL;
		}

		pll_clkin = pTAS2560->mnSamplingRate * pTAS2560->mnFrameSize;
	}

	pTAS2560->read(pTAS2560, TAS2560_PWR_REG, &power);
	if (power & TAS2560_PWR_BIT_MASK) {
		dev_dbg(pTAS2560->dev, "power down to update PLL\n");
		pTAS2560->write(pTAS2560, TAS2560_PWR_REG, TAS2560_PWR_BIT_MASK|TAS2560_MUTE_MASK);
		msleep(1);
	}

	/* Fill in the PLL control registers for J & D
	 * pll_clk = (pll_clkin * J.D) / P
	 * Need to fill in J and D here based on incoming freq
	 */
	if (pll_clkin <= 40000000)
		p = 1;
	else if (pll_clkin <= 80000000)
		p = 2;
	else if (pll_clkin <= 160000000)
		p = 3;
	else {
		dev_err(pTAS2560->dev, "PLL Clk In %d not covered here\n", pll_clkin);
		ret = -EINVAL;
	}

	if (ret >= 0) {
		j = (pll_clk * p) / pll_clkin;
		d = (pll_clk * p) % pll_clkin;
		d /= (pll_clkin / 10000);

		pll_clkin_divide = pll_clkin / (1 << p);

		if ((d == 0)
			&& ((pll_clkin_divide < 512000) || (pll_clkin_divide > 20000000))) {
			dev_err(pTAS2560->dev, "PLL cal ERROR!!!, pll_in=%d\n", pll_clkin);
			ret = -EINVAL;
		}

		if ((d != 0)
			&& ((pll_clkin_divide < 10000000) || (pll_clkin_divide > 20000000))) {
			dev_err(pTAS2560->dev, "PLL cal ERROR!!!, pll_in=%d\n", pll_clkin);
			ret = -EINVAL;
		}

		if (j == 0) {
			dev_err(pTAS2560->dev, "PLL cal ERROR!!!, j ZERO\n");
			ret = -EINVAL;
		}
	}

	if (ret >= 0) {
		dev_info(pTAS2560->dev,
			"PLL clk_in = %d, P=%d, J.D=%d.%d\n", pll_clkin, p, j, d);
		/* update P */
		if (p == 64)
			temp = 0;
		else
			temp = p;
		pTAS2560->update_bits(pTAS2560, TAS2560_CLK_SEL, TAS2560_PLL_P_MASK, temp);

		/* Update J */
		temp = j;
		if (pll_clkin < 1000000)
			temp |= 0x80;
		pTAS2560->write(pTAS2560, TAS2560_SET_FREQ, temp);

		/* Update D */
		temp = (d & 0x00ff);
		pTAS2560->write(pTAS2560, TAS2560_PLL_D_LSB, temp);
		temp = ((d & 0x3f00) >> 8);
		pTAS2560->write(pTAS2560, TAS2560_PLL_D_MSB, temp);
	}

	/* Restore PLL status */
	if (power & TAS2560_PWR_BIT_MASK) {
		pTAS2560->write(pTAS2560, TAS2560_PWR_REG, power);
		msleep(1);
	}

	return ret;
}

/* 0, 8Ohm; 1, 6Ohm; 2, 4Ohm */
int tas2560_setLoad(struct tas2560_priv *pTAS2560, int load)
{
	int ret = 0;
	int value = -1;

	dev_dbg(pTAS2560->dev, "%s: %d\n", __func__, load);

	switch (load) {
	case LOAD_8OHM:
		value = 0;
		tas2560_i2c_load_data(pTAS2560, p_tas2560_8Ohm_data);
		break;
	case LOAD_6OHM:
		value = 1;
		tas2560_i2c_load_data(pTAS2560, p_tas2560_6Ohm_data);
		break;
	case LOAD_4OHM:
		value = 2;
		tas2560_i2c_load_data(pTAS2560, p_tas2560_4Ohm_data);
		break;
	default:
		break;
	}

	if (value >= 0) {
		pTAS2560->update_bits(pTAS2560, TAS2560_LOAD, LOAD_MASK, value << 3);
	}
	return ret;
}

int tas2560_getLoad(struct tas2560_priv *pTAS2560)
{
	int ret = -1;
	int value = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->read(pTAS2560, TAS2560_LOAD, &value);

	value = (value&0x18) >> 3;

	switch (value) {
	case 0:
		ret = LOAD_8OHM;
		break;
	case 1:
		ret = LOAD_6OHM;
		break;
	case 2:
		ret = LOAD_4OHM;
		break;
	default:
		break;
	}

	return ret;
}

int tas2560_get_volume(struct tas2560_priv *pTAS2560)
{
	int ret = -1;
	int value = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	ret = pTAS2560->read(pTAS2560, TAS2560_SPK_CTRL_REG, &value);
	if (ret >= 0)
		return (value & 0x0f);

	return ret;
}

int tas2560_set_volume(struct tas2560_priv *pTAS2560, int volume)
{
	int ret = -1;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	ret = pTAS2560->update_bits(pTAS2560, TAS2560_SPK_CTRL_REG, 0x0f, volume&0x0f);

	return ret;
}

int tas2560_parse_dt(struct device *dev, struct tas2560_priv *pTAS2560)
{
	struct device_node *np = dev->of_node;
	int rc = 0, ret = 0;

	rc = of_property_read_u32(np, "ti,load", &pTAS2560->mnLoad);
	if (rc) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,load", np->full_name, rc);
	} else {
		dev_dbg(pTAS2560->dev, "ti,load=%d", pTAS2560->mnLoad);
	}

	rc = of_property_read_u32(np, "ti,asi-format", &pTAS2560->mnASIFormat);
	if (rc) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,asi-format", np->full_name, rc);
	} else {
		dev_dbg(pTAS2560->dev, "ti,asi-format=%d", pTAS2560->mnASIFormat);
	}

	pTAS2560->mnResetGPIO = of_get_named_gpio(np, "ti,reset-gpio", 0);
	if (!gpio_is_valid(pTAS2560->mnResetGPIO)) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,reset-gpio", np->full_name, pTAS2560->mnResetGPIO);
	} else {
		dev_dbg(pTAS2560->dev, "ti,reset-gpio=%d", pTAS2560->mnResetGPIO);
	}

	pTAS2560->mnIRQGPIO = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(pTAS2560->mnIRQGPIO)) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,irq-gpio", np->full_name, pTAS2560->mnIRQGPIO);
	} else {
		dev_dbg(pTAS2560->dev, "ti,irq-gpio=%d", pTAS2560->mnIRQGPIO);
	}

	rc = of_property_read_u32(np, "ti,ppg", &pTAS2560->mnPPG);
	if (rc) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,ppg", np->full_name, rc);
	} else {
		dev_dbg(pTAS2560->dev, "ti,ppg=%d", pTAS2560->mnPPG);
	}

	rc = of_property_read_u32(np, "ti,pll", &pTAS2560->mnPLL);
	if (rc) {
		dev_err(pTAS2560->dev, "Looking up %s property in node %s failed %d\n",
			"ti,pll", np->full_name, rc);
	} else {
		dev_dbg(pTAS2560->dev, "ti,pll=%d", pTAS2560->mnPLL);
	}

	return ret;
}

int tas2560_load_default(struct tas2560_priv *pTAS2560)
{
	return tas2560_i2c_load_data(pTAS2560, p_tas2560_default_data);
}

int tas2560_load_platdata(struct tas2560_priv *pTAS2560)
{
	int nResult = 0;

	if (gpio_is_valid(pTAS2560->mnIRQGPIO)) {
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_irq_config);
		if (nResult < 0)
			goto end;
	}

	if (pTAS2560->mnPLL == PLL_BCLK_1P536_48KSR)
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_48kSR_BCLK_1P536_data);
	else if (pTAS2560->mnPLL == PLL_BCLK_3P072_48KSR)
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_48kSR_BCLK_3P072_data);
	if (nResult < 0)
		goto end;

	if ((pTAS2560->mnPLL == PLL_BCLK_1P536_48KSR)
		|| (pTAS2560->mnPLL == PLL_BCLK_3P072_48KSR))
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_48khz_data);
	if (nResult < 0)
		goto end;

	if (pTAS2560->mnLoad == LOAD_8OHM) {
		nResult = pTAS2560->write(pTAS2560, TAS2560_LOAD, 0x83);
	} else if (pTAS2560->mnLoad == LOAD_6OHM) {
		nResult = pTAS2560->write(pTAS2560, TAS2560_LOAD, 0x8b);
	} else {
		/* 4 Ohm speaker */
		nResult = pTAS2560->write(pTAS2560, TAS2560_LOAD, 0x9b);
	}
	if (nResult < 0)
		goto end;

	if (pTAS2560->mnASIFormat == TAS2560_DATAFORMAT_I2S) {
		nResult = tas2560_set_ASI_fmt(pTAS2560,
			SND_SOC_DAIFMT_CBS_CFS|SND_SOC_DAIFMT_NB_NF|SND_SOC_DAIFMT_I2S);
		if (nResult < 0)
			goto end;
		nResult = pTAS2560->write(pTAS2560, TAS2560_ASI_OFFSET_1, 0x00);
	} else if (pTAS2560->mnASIFormat == TAS2560_DATAFORMAT_DSP) {
		nResult = tas2560_set_ASI_fmt(pTAS2560,
			SND_SOC_DAIFMT_CBS_CFS|SND_SOC_DAIFMT_IB_IF|SND_SOC_DAIFMT_DSP_A);
		if (nResult < 0)
			goto end;
		nResult = pTAS2560->write(pTAS2560, TAS2560_ASI_OFFSET_1, 0x01);
	} else {
		dev_err(pTAS2560->dev, "need to implement!!!\n");
	}

	if (nResult < 0)
		goto end;

	nResult = tas2560_set_bit_rate(pTAS2560, 16);

end:

	return nResult;
}

static int tas2560_load_postpwrup(struct tas2560_priv *pTAS2560)
{
	int nResult = 0;

	nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_idle_chnl_detect);
	if (nResult < 0)
		goto end;

	if (pTAS2560->mnPPG) {
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_PPG_data);
		if (nResult < 0)
			goto end;
	}

	nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_HPF_data);
	if (nResult < 0)
		goto end;

	if (pTAS2560->mnLoad == LOAD_8OHM) {
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_8Ohm_data);
	} else if (pTAS2560->mnLoad == LOAD_6OHM) {
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_6Ohm_data);
	}
	if (nResult < 0)
		goto end;

	nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_boost_headroom_data);
	if (nResult < 0)
		goto end;

	nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_thermal_foldback);
	if (nResult < 0)
		goto end;

	nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_Vsense_biquad_data);

end:
	return nResult;
}

int tas2560_LoadConfig(struct tas2560_priv *pTAS2560, bool bPowerOn)
{
	int nResult = 0;

	if (bPowerOn) {
		dev_dbg(pTAS2560->dev, "%s power down to load config\n", __func__);
		if (hrtimer_active(&pTAS2560->mtimer))
			hrtimer_cancel(&pTAS2560->mtimer);
		pTAS2560->enableIRQ(pTAS2560, false);
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_shutdown_data);
		if (nResult < 0)
			goto end;
	}

	pTAS2560->hw_reset(pTAS2560);

	nResult = pTAS2560->write(pTAS2560, TAS2560_SW_RESET_REG, 0x01);
	if (nResult < 0)
		goto end;
	msleep(1);

	nResult = tas2560_load_default(pTAS2560);
	if (nResult < 0) {
		goto end;
	}

	nResult = tas2560_load_platdata(pTAS2560);
	if (nResult < 0) {
		goto end;
	}

	if (bPowerOn) {
		dev_dbg(pTAS2560->dev, "%s power up\n", __func__);
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_startup_data);
		if (nResult < 0)
			goto end;
		nResult = tas2560_load_postpwrup(pTAS2560);
		if (nResult < 0)
			goto end;
		nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_unmute_data);
		if (nResult < 0)
			goto end;
		pTAS2560->enableIRQ(pTAS2560, true);
	}
end:
	return nResult;
}

int tas2560_enable(struct tas2560_priv *pTAS2560, bool bEnable)
{
	int nResult = 0;

	if (bEnable) {
		if (!pTAS2560->mbPowerUp) {
			dev_dbg(pTAS2560->dev, "%s power up\n", __func__);
			pTAS2560->clearIRQ(pTAS2560);
			nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_startup_data);
			if (nResult < 0)
				goto end;
			nResult = tas2560_load_postpwrup(pTAS2560);
			if (nResult < 0)
				goto end;
			nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_unmute_data);
			pTAS2560->mbPowerUp = true;
			if (nResult < 0)
				goto end;
			pTAS2560->enableIRQ(pTAS2560, true);
		}
	} else {
		if (pTAS2560->mbPowerUp) {
			dev_dbg(pTAS2560->dev, "%s power down\n", __func__);
			if (hrtimer_active(&pTAS2560->mtimer))
				hrtimer_cancel(&pTAS2560->mtimer);
			pTAS2560->enableIRQ(pTAS2560, false);
			nResult = tas2560_i2c_load_data(pTAS2560, p_tas2560_shutdown_data);
			pTAS2560->mbPowerUp = false;
		}
	}

end:
	return nResult;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2560 common functions for Android Linux");
MODULE_LICENSE("GPL v2");