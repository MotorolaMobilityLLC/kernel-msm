/*
 * ALSA SoC Texas Instruments TAS2560 High Performance 4W Smart Amplifier
 *
 * Copyright (C) 2016 Texas Instruments, Inc.
 *
 * Author: saiprasad
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#ifdef CONFIG_TAS2560_REGMAP

/* #define DEBUG */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
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
#include <sound/soc.h>

#include "tas2560.h"
#include "tas2560-core.h"
#include "tas2560-misc.h"
#include "tas2560-codec.h"

static int tas2560_change_book_page(struct tas2560_priv *pTAS2560,
	int book, int page)
{
	int nResult = 0;

	if ((pTAS2560->mnCurrentBook == book)
		&& (pTAS2560->mnCurrentPage == page))
		goto end;

	if (pTAS2560->mnCurrentBook != book) {
		nResult = regmap_write(pTAS2560->regmap, TAS2560_BOOKCTL_PAGE, 0);
		if (nResult < 0) {
			dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2560->mnCurrentPage = 0;
		nResult = regmap_write(pTAS2560->regmap, TAS2560_BOOKCTL_REG, book);
		if (nResult < 0) {
			dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2560->mnCurrentBook = book;
	}

	if (pTAS2560->mnCurrentPage != page) {
		nResult = regmap_write(pTAS2560->regmap, TAS2560_BOOKCTL_PAGE, page);
		if (nResult < 0) {
			dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
				__func__, __LINE__, nResult);
			goto end;
		}
		pTAS2560->mnCurrentPage = page;
	}

end:
	return nResult;
}

static int tas2560_dev_read(struct tas2560_priv *pTAS2560,
	unsigned int reg, unsigned int *pValue)
{
	int nResult = 0;

	mutex_lock(&pTAS2560->dev_lock);

	nResult = tas2560_change_book_page(pTAS2560,
		TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_read(pTAS2560->regmap, TAS2560_PAGE_REG(reg), pValue);
	if (nResult < 0)
		dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2560->dev, "%s: BOOK:PAGE:REG %u:%u:%u\n", __func__,
			TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg),
			TAS2560_PAGE_REG(reg));

end:
	mutex_unlock(&pTAS2560->dev_lock);
	return nResult;
}

static int tas2560_dev_write(struct tas2560_priv *pTAS2560,
	unsigned int reg, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2560->dev_lock);

	nResult = tas2560_change_book_page(pTAS2560,
		TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_write(pTAS2560->regmap, TAS2560_PAGE_REG(reg),
			value);
	if (nResult < 0)
		dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2560->dev, "%s: BOOK:PAGE:REG %u:%u:%u, VAL: 0x%02x\n",
			__func__, TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg),
			TAS2560_PAGE_REG(reg), value);

end:
	mutex_unlock(&pTAS2560->dev_lock);
	return nResult;
}

static int tas2560_dev_bulk_write(struct tas2560_priv *pTAS2560,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2560->dev_lock);

	nResult = tas2560_change_book_page(pTAS2560,
		TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_bulk_write(pTAS2560->regmap,
		TAS2560_PAGE_REG(reg), pData, nLength);
	if (nResult < 0)
		dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2560->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg),
			TAS2560_PAGE_REG(reg), nLength);

end:
	mutex_unlock(&pTAS2560->dev_lock);
	return nResult;
}

static int tas2560_dev_bulk_read(struct tas2560_priv *pTAS2560,
	unsigned int reg, unsigned char *pData, unsigned int nLength)
{
	int nResult = 0;

	mutex_lock(&pTAS2560->dev_lock);

	nResult = tas2560_change_book_page(pTAS2560,
		TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_bulk_read(pTAS2560->regmap, TAS2560_PAGE_REG(reg), pData, nLength);
	if (nResult < 0)
		dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2560->dev, "%s: BOOK:PAGE:REG %u:%u:%u, len: 0x%02x\n",
			__func__, TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg),
			TAS2560_PAGE_REG(reg), nLength);
end:
	mutex_unlock(&pTAS2560->dev_lock);
	return nResult;
}

static int tas2560_dev_update_bits(struct tas2560_priv *pTAS2560, unsigned int reg,
			 unsigned int mask, unsigned int value)
{
	int nResult = 0;

	mutex_lock(&pTAS2560->dev_lock);
	nResult = tas2560_change_book_page(pTAS2560,
		TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg));
	if (nResult < 0)
		goto end;

	nResult = regmap_update_bits(pTAS2560->regmap, TAS2560_PAGE_REG(reg), mask, value);
	if (nResult < 0)
		dev_err(pTAS2560->dev, "%s, ERROR, L=%d, E=%d\n",
			__func__, __LINE__, nResult);
	else
		dev_dbg(pTAS2560->dev, "%s: BOOK:PAGE:REG %u:%u:%u, mask: 0x%x, val=0x%x\n",
			__func__, TAS2560_BOOK_ID(reg), TAS2560_PAGE_ID(reg),
			TAS2560_PAGE_REG(reg), mask, value);
end:
	mutex_unlock(&pTAS2560->dev_lock);
	return nResult;
}

static bool tas2560_volatile(struct device *dev, unsigned int reg)
{
	return false;
}

static bool tas2560_writeable(struct device *dev, unsigned int reg)
{
	return true;
}

static const struct regmap_config tas2560_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.writeable_reg = tas2560_writeable,
	.volatile_reg = tas2560_volatile,
	.cache_type = REGCACHE_NONE,
	.max_register = 128,
};

static void tas2560_hw_reset(struct tas2560_priv *pTAS2560)
{
	if (gpio_is_valid(pTAS2560->mnResetGPIO)) {
		gpio_direction_output(pTAS2560->mnResetGPIO, 0);
		msleep(5);
		gpio_direction_output(pTAS2560->mnResetGPIO, 1);
		msleep(2);
	}

	pTAS2560->mnCurrentBook = -1;
	pTAS2560->mnCurrentPage = -1;
}

void tas2560_clearIRQ(struct tas2560_priv *pTAS2560)
{
	unsigned int nValue;
	int nResult = 0;

	nResult = pTAS2560->read(pTAS2560, TAS2560_FLAGS_1, &nValue);
	if (nResult >= 0)
		pTAS2560->read(pTAS2560, TAS2560_FLAGS_2, &nValue);
}

void tas2560_enableIRQ(struct tas2560_priv *pTAS2560, bool enable)
{
	if (enable) {
		if (pTAS2560->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2560->mnIRQGPIO))
			enable_irq(pTAS2560->mnIRQ);

		schedule_delayed_work(&pTAS2560->irq_work, msecs_to_jiffies(10));
		pTAS2560->mbIRQEnable = true;
	} else {
		if (!pTAS2560->mbIRQEnable)
			return;

		if (gpio_is_valid(pTAS2560->mnIRQGPIO))
			disable_irq_nosync(pTAS2560->mnIRQ);
		pTAS2560->mbIRQEnable = false;
	}
}

static void irq_work_routine(struct work_struct *work)
{
	struct tas2560_priv *pTAS2560 =
		container_of(work, struct tas2560_priv, irq_work.work);
	unsigned int nDevInt1Status = 0, nDevInt2Status = 0;
	int nCounter = 2;
	int nResult = 0;

#ifdef CONFIG_TAS2560_CODEC
	mutex_lock(&pTAS2560->codec_lock);
#endif

#ifdef CONFIG_TAS2560_MISC
	mutex_lock(&pTAS2560->file_lock);
#endif
	if (pTAS2560->mbRuntimeSuspend) {
		dev_info(pTAS2560->dev, "%s, Runtime Suspended\n", __func__);
		goto end;
	}

	if (!pTAS2560->mbPowerUp) {
		dev_info(pTAS2560->dev, "%s, device not powered\n", __func__);
		goto end;
	}

	nResult = tas2560_dev_write(pTAS2560, TAS2560_INT_GEN_REG, 0x00);
	if (nResult < 0)
		goto reload;

	nResult = tas2560_dev_read(pTAS2560, TAS2560_FLAGS_1, &nDevInt1Status);
	if (nResult >= 0)
		nResult = tas2560_dev_read(pTAS2560, TAS2560_FLAGS_2, &nDevInt2Status);
	else
		goto reload;

	if (((nDevInt1Status & 0xfc) != 0) || ((nDevInt2Status & 0xc0) != 0)) {
		/* in case of INT_OC, INT_UV, INT_OT, INT_BO, INT_CL, INT_CLK1, INT_CLK2 */
		dev_dbg(pTAS2560->dev, "IRQ critical Error : 0x%x, 0x%x\n",
			nDevInt1Status, nDevInt2Status);

		if (nDevInt1Status & 0x80) {
			pTAS2560->mnErrCode |= ERROR_OVER_CURRENT;
			dev_err(pTAS2560->dev, "SPK over current!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_OVER_CURRENT;

		if (nDevInt1Status & 0x40) {
			pTAS2560->mnErrCode |= ERROR_UNDER_VOLTAGE;
			dev_err(pTAS2560->dev, "SPK under voltage!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_UNDER_VOLTAGE;

		if (nDevInt1Status & 0x20) {
			pTAS2560->mnErrCode |= ERROR_CLK_HALT;
			dev_err(pTAS2560->dev, "clk halted!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_CLK_HALT;

		if (nDevInt1Status & 0x10) {
			pTAS2560->mnErrCode |= ERROR_DIE_OVERTEMP;
			dev_err(pTAS2560->dev, "die over temperature!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_DIE_OVERTEMP;

		if (nDevInt1Status & 0x08) {
			pTAS2560->mnErrCode |= ERROR_BROWNOUT;
			dev_err(pTAS2560->dev, "brownout!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_BROWNOUT;

		if (nDevInt1Status & 0x04) {
			pTAS2560->mnErrCode |= ERROR_CLK_LOST;
		} else
			pTAS2560->mnErrCode &= ~ERROR_CLK_LOST;

		if (nDevInt2Status & 0x80) {
			pTAS2560->mnErrCode |= ERROR_CLK_DET1;
			dev_err(pTAS2560->dev, "clk detection 1!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_CLK_DET1;

		if (nDevInt2Status & 0x40) {
			pTAS2560->mnErrCode |= ERROR_CLK_DET2;
			dev_err(pTAS2560->dev, "clk detection 2!\n");
		} else
			pTAS2560->mnErrCode &= ~ERROR_CLK_DET2;

		goto reload;
	} else {
		dev_dbg(pTAS2560->dev, "IRQ status : 0x%x, 0x%x\n",
				nDevInt1Status, nDevInt2Status);
		nCounter = 2;

		while (nCounter > 0) {
			nResult = tas2560_dev_read(pTAS2560, TAS2560_POWER_UP_FLAG_REG, &nDevInt1Status);
			if (nResult < 0)
				goto reload;

			if ((nDevInt1Status & 0xc0) == 0xc0)
				break;

			nCounter--;
			if (nCounter > 0) {
				/* in case check pow status just after power on TAS2560 */
				dev_dbg(pTAS2560->dev, "PowSts B: 0x%x, check again after 10ms\n",
					nDevInt1Status);
				msleep(10);
			}
		}

		if ((nDevInt1Status & 0xc0) != 0xc0) {
			dev_err(pTAS2560->dev, "%s, Critical ERROR B[%d]_P[%d]_R[%d]= 0x%x\n",
				__func__,
				TAS2560_BOOK_ID(TAS2560_POWER_UP_FLAG_REG),
				TAS2560_PAGE_ID(TAS2560_POWER_UP_FLAG_REG),
				TAS2560_PAGE_REG(TAS2560_POWER_UP_FLAG_REG),
				nDevInt1Status);
			pTAS2560->mnErrCode |= ERROR_CLASSD_PWR;
			goto reload;
		}
		pTAS2560->mnErrCode &= ~ERROR_CLASSD_PWR;
	}

	nResult = tas2560_dev_write(pTAS2560, TAS2560_INT_GEN_REG, 0xff);
	if (nResult < 0)
		goto reload;

	goto end;

reload:
	/* hardware reset and reload */
	tas2560_LoadConfig(pTAS2560, true);

end:
	if (!hrtimer_active(&pTAS2560->mtimer)) {
		dev_dbg(pTAS2560->dev, "%s, start timer\n", __func__);
		hrtimer_start(&pTAS2560->mtimer,
			ns_to_ktime((u64)CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
#ifdef CONFIG_TAS2560_MISC
	mutex_unlock(&pTAS2560->file_lock);
#endif

#ifdef CONFIG_TAS2560_CODEC
	mutex_unlock(&pTAS2560->codec_lock);
#endif
}

static enum hrtimer_restart timer_func(struct hrtimer *timer)
{
	struct tas2560_priv *pTAS2560 = container_of(timer, struct tas2560_priv, mtimer);

	if (pTAS2560->mbPowerUp) {
		if (!delayed_work_pending(&pTAS2560->irq_work))
			schedule_delayed_work(&pTAS2560->irq_work, msecs_to_jiffies(20));
	}

	return HRTIMER_NORESTART;
}

static irqreturn_t tas2560_irq_handler(int irq, void *dev_id)
{
	struct tas2560_priv *pTAS2560 = (struct tas2560_priv *)dev_id;

	tas2560_enableIRQ(pTAS2560, false);

	/* get IRQ status after 100 ms */
	if (!delayed_work_pending(&pTAS2560->irq_work))
		schedule_delayed_work(&pTAS2560->irq_work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static int tas2560_runtime_suspend(struct tas2560_priv *pTAS2560)
{
	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	pTAS2560->mbRuntimeSuspend = true;

	if (hrtimer_active(&pTAS2560->mtimer)) {
		dev_dbg(pTAS2560->dev, "cancel die temp timer\n");
		hrtimer_cancel(&pTAS2560->mtimer);
	}

	if (delayed_work_pending(&pTAS2560->irq_work)) {
		dev_dbg(pTAS2560->dev, "cancel IRQ work\n");
		cancel_delayed_work_sync(&pTAS2560->irq_work);
	}

	return 0;
}

static int tas2560_runtime_resume(struct tas2560_priv *pTAS2560)
{
	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	if (pTAS2560->mbPowerUp) {
		if (!hrtimer_active(&pTAS2560->mtimer)) {
			dev_dbg(pTAS2560->dev, "%s, start check timer\n", __func__);
			hrtimer_start(&pTAS2560->mtimer,
				ns_to_ktime((u64)CHECK_PERIOD * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	}

	pTAS2560->mbRuntimeSuspend = false;

	return 0;
}

static int tas2560_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct tas2560_priv *pTAS2560;
	int nResult;

	dev_info(&client->dev, "%s enter\n", __func__);

	pTAS2560 = devm_kzalloc(&client->dev, sizeof(struct tas2560_priv), GFP_KERNEL);
	if (pTAS2560 == NULL) {
		dev_err(&client->dev, "%s, -ENOMEM \n", __func__);
		nResult = -ENOMEM;
		goto end;
	}

	pTAS2560->dev = &client->dev;
	i2c_set_clientdata(client, pTAS2560);
	dev_set_drvdata(&client->dev, pTAS2560);

	pTAS2560->regmap = devm_regmap_init_i2c(client, &tas2560_i2c_regmap);
	if (IS_ERR(pTAS2560->regmap)) {
		nResult = PTR_ERR(pTAS2560->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
					nResult);
		goto end;
	}

	if (client->dev.of_node)
		tas2560_parse_dt(&client->dev, pTAS2560);

	if (gpio_is_valid(pTAS2560->mnResetGPIO)) {
		nResult = gpio_request(pTAS2560->mnResetGPIO, "TAS2560_RESET");
		if (nResult) {
			dev_err(pTAS2560->dev, "%s: Failed to request gpio %d\n", __func__,
				pTAS2560->mnResetGPIO);
			nResult = -EINVAL;
			goto free_gpio;
		}
	}

	if (gpio_is_valid(pTAS2560->mnSwitchGPIO)) {
		nResult = gpio_request(pTAS2560->mnSwitchGPIO, "TAS2560_EAR_SWITH");
		if (nResult) {
			dev_err(pTAS2560->dev, "%s: Failed to request gpio %d\n", __func__,
				pTAS2560->mnSwitchGPIO);
			nResult = -EINVAL;
			goto free_gpio;
		}
		gpio_direction_output(pTAS2560->mnSwitchGPIO, 0);
	}

	if (gpio_is_valid(pTAS2560->mnIRQGPIO)) {
		nResult = gpio_request(pTAS2560->mnIRQGPIO, "TAS2560-IRQ");
		if (nResult < 0) {
			dev_err(pTAS2560->dev, "%s: GPIO %d request error\n",
				__func__, pTAS2560->mnIRQGPIO);
			goto free_gpio;
		}
		gpio_direction_input(pTAS2560->mnIRQGPIO);
		pTAS2560->mnIRQ = gpio_to_irq(pTAS2560->mnIRQGPIO);
		dev_dbg(pTAS2560->dev, "irq = %d\n", pTAS2560->mnIRQ);
		nResult = request_threaded_irq(pTAS2560->mnIRQ, tas2560_irq_handler,
					NULL, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					client->name, pTAS2560);
		if (nResult < 0) {
			dev_err(pTAS2560->dev,
				"request_irq failed, %d\n", nResult);
			goto free_gpio;
		}
		disable_irq_nosync(pTAS2560->mnIRQ);
		INIT_DELAYED_WORK(&pTAS2560->irq_work, irq_work_routine);
	}

	pTAS2560->read = tas2560_dev_read;
	pTAS2560->write = tas2560_dev_write;
	pTAS2560->bulk_read = tas2560_dev_bulk_read;
	pTAS2560->bulk_write = tas2560_dev_bulk_write;
	pTAS2560->update_bits = tas2560_dev_update_bits;
	pTAS2560->hw_reset = tas2560_hw_reset;
	pTAS2560->enableIRQ = tas2560_enableIRQ;
	pTAS2560->clearIRQ = tas2560_clearIRQ;
	pTAS2560->runtime_suspend = tas2560_runtime_suspend;
	pTAS2560->runtime_resume = tas2560_runtime_resume;
	mutex_init(&pTAS2560->dev_lock);

	nResult = tas2560_LoadConfig(pTAS2560, false);
	if (nResult < 0)
		goto destroy_mutex;

#ifdef CONFIG_TAS2560_CODEC
	mutex_init(&pTAS2560->codec_lock);
	tas2560_register_codec(pTAS2560);
#endif

#ifdef CONFIG_TAS2560_MISC
	mutex_init(&pTAS2560->file_lock);
	tas2560_register_misc(pTAS2560);
#endif

	hrtimer_init(&pTAS2560->mtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pTAS2560->mtimer.function = timer_func;

destroy_mutex:
	if (nResult < 0)
		mutex_destroy(&pTAS2560->dev_lock);

free_gpio:
	if (nResult < 0) {
		if (gpio_is_valid(pTAS2560->mnResetGPIO))
			gpio_free(pTAS2560->mnResetGPIO);
		if (gpio_is_valid(pTAS2560->mnIRQGPIO))
			gpio_free(pTAS2560->mnIRQGPIO);
		if (gpio_is_valid(pTAS2560->mnSwitchGPIO))
			gpio_free(pTAS2560->mnSwitchGPIO);
	}

end:
	return nResult;
}

static int tas2560_i2c_remove(struct i2c_client *client)
{
	struct tas2560_priv *pTAS2560 = i2c_get_clientdata(client);

	dev_info(pTAS2560->dev, "%s\n", __func__);

#ifdef CONFIG_TAS2560_CODEC
	tas2560_deregister_codec(pTAS2560);
	mutex_destroy(&pTAS2560->codec_lock);
#endif

#ifdef CONFIG_TAS2560_MISC
	tas2560_deregister_misc(pTAS2560);
	mutex_destroy(&pTAS2560->file_lock);
#endif

	if (gpio_is_valid(pTAS2560->mnResetGPIO))
		gpio_free(pTAS2560->mnResetGPIO);
	if (gpio_is_valid(pTAS2560->mnIRQGPIO))
		gpio_free(pTAS2560->mnIRQGPIO);
	if (gpio_is_valid(pTAS2560->mnSwitchGPIO))
		gpio_free(pTAS2560->mnSwitchGPIO);

	return 0;
}


static const struct i2c_device_id tas2560_i2c_id[] = {
	{ "tas2560", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas2560_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id tas2560_of_match[] = {
	{ .compatible = "ti,tas2560" },
	{},
};
MODULE_DEVICE_TABLE(of, tas2560_of_match);
#endif


static struct i2c_driver tas2560_i2c_driver = {
	.driver = {
		.name   = "tas2560",
		.owner  = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(tas2560_of_match),
#endif
	},
	.probe      = tas2560_i2c_probe,
	.remove     = tas2560_i2c_remove,
	.id_table   = tas2560_i2c_id,
};

module_i2c_driver(tas2560_i2c_driver);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2560 I2C Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif
