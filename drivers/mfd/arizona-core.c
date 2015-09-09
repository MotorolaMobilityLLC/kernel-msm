/*
 * Arizona core driver
 *
 * Copyright 2014 CirrusLogic, Inc.
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/asound.h>
#include <sound/soc.h>

#include <linux/mfd/arizona/core.h>
#include <linux/mfd/arizona/registers.h>

#include "arizona.h"

static const char *wm5102_core_supplies[] = {
	"AVDD",
	"DBVDD1",
};

int arizona_clk32k_enable(struct arizona *arizona)
{
	int ret = 0;

	mutex_lock(&arizona->clk_lock);

	arizona->clk32k_ref++;

	if (arizona->clk32k_ref == 1) {
		switch (arizona->pdata.clk32k_src) {
		case ARIZONA_32KZ_MCLK1:
			ret = pm_runtime_get_sync(arizona->dev);
			if (ret != 0)
				goto out;
			break;
		}

		ret = regmap_update_bits(arizona->regmap, ARIZONA_CLOCK_32K_1,
					 ARIZONA_CLK_32K_ENA,
					 ARIZONA_CLK_32K_ENA);
	}

out:
	if (ret != 0)
		arizona->clk32k_ref--;

	mutex_unlock(&arizona->clk_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_clk32k_enable);

int arizona_clk32k_disable(struct arizona *arizona)
{
	int ret = 0;

	mutex_lock(&arizona->clk_lock);

	BUG_ON(arizona->clk32k_ref <= 0);

	arizona->clk32k_ref--;

	if (arizona->clk32k_ref == 0) {
		regmap_update_bits(arizona->regmap, ARIZONA_CLOCK_32K_1,
				   ARIZONA_CLK_32K_ENA, 0);

		switch (arizona->pdata.clk32k_src) {
		case ARIZONA_32KZ_MCLK1:
			pm_runtime_put_sync(arizona->dev);
			break;
		}
	}

	mutex_unlock(&arizona->clk_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_clk32k_disable);

static int arizona_dvfs_apply_boost(struct arizona *arizona)
{
	int ret;

	ret = regulator_set_voltage(arizona->dcvdd, 1800000, 1800000);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to boost DCVDD: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(arizona->regmap,
				ARIZONA_DYNAMIC_FREQUENCY_SCALING_1,
				ARIZONA_SUBSYS_MAX_FREQ, 1);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to enable subsys max: %d\n", ret);

		regulator_set_voltage(arizona->dcvdd, 1200000, 1800000);
		return ret;
	}

	return 0;
}

static int arizona_dvfs_remove_boost(struct arizona *arizona)
{
	int ret;

	ret = regmap_update_bits(arizona->regmap,
				ARIZONA_DYNAMIC_FREQUENCY_SCALING_1,
				ARIZONA_SUBSYS_MAX_FREQ, 0);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to disable subsys max: %d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage(arizona->dcvdd, 1200000, 1800000);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to unboost DCVDD : %d\n", ret);
		return ret;
	}

	return 0;
}

int arizona_dvfs_up(struct arizona *arizona, unsigned int flags)
{
	unsigned int old_flags;
	int ret = 0;

	mutex_lock(&arizona->subsys_max_lock);

	old_flags = arizona->subsys_max_rq;
	arizona->subsys_max_rq |= flags;

	/* If currently caching the change will be applied in runtime resume */
	if (arizona->subsys_max_cached) {
		dev_dbg(arizona->dev, "subsys_max_cached (dvfs up)\n");
		goto out;
	}

	if (arizona->subsys_max_rq != old_flags) {
		switch (arizona->type) {
		case WM5102:
		case WM8997:
		case WM8998:
		case WM1814:
			ret = arizona_dvfs_apply_boost(arizona);
			break;

		default:
			break;
		}

	}
out:
	mutex_unlock(&arizona->subsys_max_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_dvfs_up);

int arizona_dvfs_down(struct arizona *arizona, unsigned int flags)
{
	unsigned int old_flags;
	int ret = 0;

	mutex_lock(&arizona->subsys_max_lock);

	old_flags = arizona->subsys_max_rq;
	arizona->subsys_max_rq &= ~flags;

	/* If currently caching the change will be applied in runtime resume */
	if (arizona->subsys_max_cached) {
		dev_dbg(arizona->dev, "subsys_max_cached (dvfs down)\n");
		goto out;
	}

	if ((old_flags != 0) && (arizona->subsys_max_rq == 0)) {
		switch (arizona->type) {
		case WM5102:
		case WM8997:
		case WM8998:
		case WM1814:
			ret = arizona_dvfs_remove_boost(arizona);
			break;

		default:
			break;
		}
	}
out:
	mutex_unlock(&arizona->subsys_max_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_dvfs_down);

static irqreturn_t arizona_clkgen_err(int irq, void *data)
{
	struct arizona *arizona = data;

	dev_err(arizona->dev, "CLKGEN error\n");

	return IRQ_HANDLED;
}

static irqreturn_t arizona_underclocked(int irq, void *data)
{
	struct arizona *arizona = data;
	unsigned int val;
	int ret;

	ret = regmap_read(arizona->regmap, ARIZONA_INTERRUPT_RAW_STATUS_8,
			  &val);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to read underclock status: %d\n",
			ret);
		return IRQ_NONE;
	}

	if (val & ARIZONA_AIF3_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "AIF3 underclocked\n");
	if (val & ARIZONA_AIF2_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "AIF2 underclocked\n");
	if (val & ARIZONA_AIF1_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "AIF1 underclocked\n");
	if (val & ARIZONA_ISRC3_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "ISRC3 underclocked\n");
	if (val & ARIZONA_ISRC2_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "ISRC2 underclocked\n");
	if (val & ARIZONA_ISRC1_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "ISRC1 underclocked\n");
	if (val & ARIZONA_FX_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "FX underclocked\n");
	if (val & ARIZONA_ASRC_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "ASRC underclocked\n");
	if (val & ARIZONA_DAC_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "DAC underclocked\n");
	if (val & ARIZONA_ADC_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "ADC underclocked\n");
	if (val & ARIZONA_MIXER_UNDERCLOCKED_STS)
		dev_err(arizona->dev, "Mixer dropped sample\n");

	return IRQ_HANDLED;
}

static irqreturn_t arizona_overclocked(int irq, void *data)
{
	struct arizona *arizona = data;
	unsigned int val[3];
	int ret;

	ret = regmap_bulk_read(arizona->regmap, ARIZONA_INTERRUPT_RAW_STATUS_6,
			       &val[0], 3);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to read overclock status: %d\n",
			ret);
		return IRQ_NONE;
	}

	switch (arizona->type) {
	case WM8998:
	case WM1814:
		val[0] = ((val[0] & 0x60e0) >> 1) |
			 ((val[0] & 0x1e00) >> 2) |
			 (val[0] & 0x000f);
		break;
	default:
		break;
	}

	if (val[0] & ARIZONA_PWM_OVERCLOCKED_STS)
		dev_err(arizona->dev, "PWM overclocked\n");
	if (val[0] & ARIZONA_FX_CORE_OVERCLOCKED_STS)
		dev_err(arizona->dev, "FX core overclocked\n");
	if (val[0] & ARIZONA_DAC_SYS_OVERCLOCKED_STS)
		dev_err(arizona->dev, "DAC SYS overclocked\n");
	if (val[0] & ARIZONA_DAC_WARP_OVERCLOCKED_STS)
		dev_err(arizona->dev, "DAC WARP overclocked\n");
	if (val[0] & ARIZONA_ADC_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ADC overclocked\n");
	if (val[0] & ARIZONA_MIXER_OVERCLOCKED_STS)
		dev_err(arizona->dev, "Mixer overclocked\n");
	if (val[0] & ARIZONA_AIF3_SYNC_OVERCLOCKED_STS)
		dev_err(arizona->dev, "AIF3 overclocked\n");
	if (val[0] & ARIZONA_AIF2_SYNC_OVERCLOCKED_STS)
		dev_err(arizona->dev, "AIF2 overclocked\n");
	if (val[0] & ARIZONA_AIF1_SYNC_OVERCLOCKED_STS)
		dev_err(arizona->dev, "AIF1 overclocked\n");
	if (val[0] & ARIZONA_PAD_CTRL_OVERCLOCKED_STS)
		dev_err(arizona->dev, "Pad control overclocked\n");

	if (val[1] & ARIZONA_SLIMBUS_SUBSYS_OVERCLOCKED_STS)
		dev_err(arizona->dev, "Slimbus subsystem overclocked\n");
	if (val[1] & ARIZONA_SLIMBUS_ASYNC_OVERCLOCKED_STS)
		dev_err(arizona->dev, "Slimbus async overclocked\n");
	if (val[1] & ARIZONA_SLIMBUS_SYNC_OVERCLOCKED_STS)
		dev_err(arizona->dev, "Slimbus sync overclocked\n");
	if (val[1] & ARIZONA_ASRC_ASYNC_SYS_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ASRC async system overclocked\n");
	if (val[1] & ARIZONA_ASRC_ASYNC_WARP_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ASRC async WARP overclocked\n");
	if (val[1] & ARIZONA_ASRC_SYNC_SYS_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ASRC sync system overclocked\n");
	if (val[1] & ARIZONA_ASRC_SYNC_WARP_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ASRC sync WARP overclocked\n");
	if (val[1] & ARIZONA_ADSP2_1_OVERCLOCKED_STS)
		dev_err(arizona->dev, "DSP1 overclocked\n");
	if (val[1] & ARIZONA_ISRC3_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ISRC3 overclocked\n");
	if (val[1] & ARIZONA_ISRC2_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ISRC2 overclocked\n");
	if (val[1] & ARIZONA_ISRC1_OVERCLOCKED_STS)
		dev_err(arizona->dev, "ISRC1 overclocked\n");

	if (val[2] & ARIZONA_SPDIF_OVERCLOCKED_STS)
		dev_err(arizona->dev, "SPDIF overclocked\n");

	return IRQ_HANDLED;
}

static int arizona_poll_reg(struct arizona *arizona,
			    int timeout, unsigned int reg,
			    unsigned int mask, unsigned int target)
{
	unsigned int val = 0;
	int ret, i;

	for (i = 0; i < timeout; i++) {
		ret = regmap_read(arizona->regmap, reg, &val);
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to read reg %u: %d\n",
				reg, ret);
			continue;
		}

		if ((val & mask) == target)
			return 0;

		msleep(1);
	}

	dev_err(arizona->dev, "Polling reg %u timed out: %x\n", reg, val);
	return -ETIMEDOUT;
}

static int arizona_wait_for_boot(struct arizona *arizona)
{
	int ret;

	/*
	 * We can't use an interrupt as we need to runtime resume to do so,
	 * we won't race with the interrupt handler as it'll be blocked on
	 * runtime resume.
	 */
	switch (arizona->type) {
	case WM5102:
	case WM8997:
	case WM1814:
	case WM8998:
	case WM5110:
	case WM8280:
	case WM1831:
	case CS47L24:
		ret = arizona_poll_reg(arizona, 5,
				       ARIZONA_INTERRUPT_RAW_STATUS_5,
				       ARIZONA_BOOT_DONE_STS,
				       ARIZONA_BOOT_DONE_STS);
		if (!ret)
			regmap_write(arizona->regmap,
				     ARIZONA_INTERRUPT_STATUS_5,
				     ARIZONA_BOOT_DONE_STS);
		break;
	default:
		ret = arizona_poll_reg(arizona, 5, CLEARWATER_IRQ1_RAW_STATUS_1,
				       CLEARWATER_BOOT_DONE_STS1,
				       CLEARWATER_BOOT_DONE_STS1);

		if (!ret)
			regmap_write(arizona->regmap, CLEARWATER_IRQ1_STATUS_1,
				     CLEARWATER_BOOT_DONE_EINT1);
		break;
	}

	pm_runtime_mark_last_busy(arizona->dev);

	return ret;
}

static inline void arizona_enable_reset(struct arizona *arizona)
{
	if (arizona->pdata.reset)
		gpio_set_value_cansleep(arizona->pdata.reset, 0);
}

static void arizona_disable_reset(struct arizona *arizona)
{
	if (arizona->pdata.reset) {
		switch (arizona->type) {
		case WM5110:
		case WM8280:
			msleep(5);
			break;
		default:
			break;
		}

		gpio_set_value_cansleep(arizona->pdata.reset, 1);
		msleep(1);
	}
}

struct arizona_sysclk_state {
	unsigned int fll;
	unsigned int sysclk;
};

static int arizona_enable_freerun_sysclk(struct arizona *arizona,
					 struct arizona_sysclk_state *state)
{
	int ret, err;

	/* Cache existing FLL and SYSCLK settings */
	ret = regmap_read(arizona->regmap, ARIZONA_FLL1_CONTROL_1, &state->fll);
	if (ret) {
		dev_err(arizona->dev, "Failed to cache FLL settings: %d\n",
			ret);
		return ret;
	}
	ret = regmap_read(arizona->regmap, ARIZONA_SYSTEM_CLOCK_1,
			  &state->sysclk);
	if (ret) {
		dev_err(arizona->dev, "Failed to cache SYSCLK settings: %d\n",
			ret);
		return ret;
	}

	/* Start up SYSCLK using the FLL in free running mode */
	ret = regmap_write(arizona->regmap, ARIZONA_FLL1_CONTROL_1,
			ARIZONA_FLL1_ENA | ARIZONA_FLL1_FREERUN);
	if (ret) {
		dev_err(arizona->dev,
			"Failed to start FLL in freerunning mode: %d\n",
			ret);
		return ret;
	}
	ret = arizona_poll_reg(arizona, 25, ARIZONA_INTERRUPT_RAW_STATUS_5,
			       ARIZONA_FLL1_CLOCK_OK_STS,
			       ARIZONA_FLL1_CLOCK_OK_STS);
	if (ret) {
		ret = -ETIMEDOUT;
		goto err_fll;
	}

	ret = regmap_write(arizona->regmap, ARIZONA_SYSTEM_CLOCK_1, 0x0144);
	if (ret) {
		dev_err(arizona->dev, "Failed to start SYSCLK: %d\n", ret);
		goto err_fll;
	}

	return 0;

err_fll:
	err = regmap_write(arizona->regmap, ARIZONA_FLL1_CONTROL_1, state->fll);
	if (err)
		dev_err(arizona->dev,
			"Failed to re-apply old FLL settings: %d\n", err);

	return ret;
}

static int arizona_disable_freerun_sysclk(struct arizona *arizona,
					  struct arizona_sysclk_state *state)
{
	int ret;

	ret = regmap_write(arizona->regmap, ARIZONA_SYSTEM_CLOCK_1,
			   state->sysclk);
	if (ret) {
		dev_err(arizona->dev,
			"Failed to re-apply old SYSCLK settings: %d\n", ret);
		return ret;
	}

	ret = regmap_write(arizona->regmap, ARIZONA_FLL1_CONTROL_1, state->fll);
	if (ret) {
		dev_err(arizona->dev,
			"Failed to re-apply old FLL settings: %d\n", ret);
		return ret;
	}

	return 0;
}

static int wm5102_apply_hardware_patch(struct arizona *arizona)
{
	struct arizona_sysclk_state state;
	int err, ret;

	ret = arizona_enable_freerun_sysclk(arizona, &state);
	if (ret)
		return ret;

	/* Start the write sequencer and wait for it to finish */
	ret = regmap_write(arizona->regmap, ARIZONA_WRITE_SEQUENCER_CTRL_0,
			ARIZONA_WSEQ_ENA | ARIZONA_WSEQ_START | 160);
	if (ret) {
		dev_err(arizona->dev, "Failed to start write sequencer: %d\n",
			ret);
		goto err;
	}

	ret = arizona_poll_reg(arizona, 5, ARIZONA_WRITE_SEQUENCER_CTRL_1,
			       ARIZONA_WSEQ_BUSY, 0);
	if (ret) {
		regmap_write(arizona->regmap, ARIZONA_WRITE_SEQUENCER_CTRL_0,
			     ARIZONA_WSEQ_ABORT);
		ret = -ETIMEDOUT;
	}

err:
	err = arizona_disable_freerun_sysclk(arizona, &state);

	return ret ?: err;
}

/*
 * Register patch to some of the CODECs internal write sequences
 * to ensure a clean exit from the low power sleep state.
 */
static const struct reg_default wm5110_sleep_patch[] = {
	{ 0x337A, 0xC100 },
	{ 0x337B, 0x0041 },
	{ 0x3300, 0xA210 },
	{ 0x3301, 0x050C },
};

static int wm5110_apply_sleep_patch(struct arizona *arizona)
{
	struct arizona_sysclk_state state;
	int err, ret;

	ret = arizona_enable_freerun_sysclk(arizona, &state);
	if (ret)
		return ret;

	ret = regmap_multi_reg_write_bypassed(arizona->regmap,
					      wm5110_sleep_patch,
					      ARRAY_SIZE(wm5110_sleep_patch));

	err = arizona_disable_freerun_sysclk(arizona, &state);

	if (ret)
		return ret;
	else
		return err;
}

static int wm5102_clear_write_sequencer(struct arizona *arizona)
{
	int ret;

	ret = regmap_write(arizona->regmap, ARIZONA_WRITE_SEQUENCER_CTRL_3,
			   0x0);
	if (ret) {
		dev_err(arizona->dev,
			"Failed to clear write sequencer state: %d\n", ret);
		return ret;
	}

	arizona_enable_reset(arizona);
	regulator_disable(arizona->dcvdd);

	msleep(20);

	ret = regulator_enable(arizona->dcvdd);
	if (ret) {
		dev_err(arizona->dev, "Failed to re-enable DCVDD: %d\n", ret);
		return ret;
	}
	arizona_disable_reset(arizona);

	return 0;
}

static int arizona_soft_reset(struct arizona *arizona)
{
	int ret;

	ret = regmap_write(arizona->regmap, ARIZONA_SOFTWARE_RESET, 0);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to reset device: %d\n", ret);
		goto err;
	}
	msleep(1);

err:
	return ret;
}

#ifdef CONFIG_PM_RUNTIME
static int arizona_restore_dvfs(struct arizona *arizona)
{
	int ret;

	switch (arizona->type) {
	default:
		return 0;	/* no DVFS */

	case WM5102:
	case WM8997:
	case WM8998:
	case WM1814:
		break;
	}

	ret = 0;
	mutex_lock(&arizona->subsys_max_lock);
	if (arizona->subsys_max_rq != 0) {
		dev_dbg(arizona->dev, "Restore subsys_max boost\n");
		ret = arizona_dvfs_apply_boost(arizona);
	}

	arizona->subsys_max_cached = false;
	mutex_unlock(&arizona->subsys_max_lock);
	return ret;
}

static int arizona_dcvdd_notify(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct arizona *arizona = container_of(nb, struct arizona,
					       dcvdd_notifier);

	dev_dbg(arizona->dev, "DCVDD notify %lx\n", action);

	if (action & REGULATOR_EVENT_DISABLE)
		msleep(20);

	return NOTIFY_DONE;
}

static int arizona_runtime_resume(struct device *dev)
{
	struct arizona *arizona = dev_get_drvdata(dev);
	int ret;

	dev_dbg(arizona->dev, "Leaving AoD mode\n");

	switch (arizona->type) {
	case WM5110:
	case WM8280:
		if (arizona->rev == 3)
			arizona_enable_reset(arizona);
		break;
	case WM5102:
	case WM8997:
	case WM8998:
	case WM1814:
	case WM1831:
	case CS47L24:
		break;
	default:
		if (arizona->external_dcvdd)
			arizona_enable_reset(arizona);
		break;
	};

	ret = regulator_enable(arizona->dcvdd);
	if (ret != 0) {
		dev_err(arizona->dev, "Failed to enable DCVDD: %d\n", ret);
		return ret;
	}

	regcache_cache_only(arizona->regmap, false);

	switch (arizona->type) {
	case WM5102:
		if (arizona->external_dcvdd) {
			ret = regmap_update_bits(arizona->regmap,
						 ARIZONA_ISOLATION_CONTROL,
						 ARIZONA_ISOLATE_DCVDD1, 0);
			if (ret != 0) {
				dev_err(arizona->dev,
					"Failed to connect DCVDD: %d\n", ret);
				goto err;
			}
		}

		ret = wm5102_patch(arizona);
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to apply patch: %d\n",
				ret);
			goto err;
		}

		ret = wm5102_apply_hardware_patch(arizona);
		if (ret) {
			dev_err(arizona->dev,
				"Failed to apply hardware patch: %d\n",
				ret);
			goto err;
		}
		break;
	case WM5110:
	case WM8280:
		if (arizona->rev == 3) {
			if (!arizona->pdata.reset) {
				ret = arizona_soft_reset(arizona);
				if (ret != 0)
					goto err;
			} else {
				arizona_disable_reset(arizona);
			}
		}

		ret = arizona_wait_for_boot(arizona);
		if (ret)
			goto err;

		if (arizona->external_dcvdd) {
			ret = regmap_update_bits(arizona->regmap,
						 ARIZONA_ISOLATION_CONTROL,
						 ARIZONA_ISOLATE_DCVDD1, 0);
			if (ret) {
				dev_err(arizona->dev,
					"Failed to connect DCVDD: %d\n", ret);
				goto err;
			}
		} else {
			/*
			 * As this is only called for the internal regulator
			 * (where we know voltage ranges available) it is ok
			 * to request an exact range.
			 */
			ret = regulator_set_voltage(arizona->dcvdd,
						    1200000, 1200000);
			if (ret < 0) {
				dev_err(arizona->dev,
					"Failed to set resume voltage: %d\n",
					ret);
				goto err;
			}
		}
		break;
	case WM8997:
	case WM8998:
	case WM1814:
	case WM1831:
	case CS47L24:
		ret = arizona_wait_for_boot(arizona);
		if (ret != 0) {
			goto err;
		}

		if (arizona->external_dcvdd) {
			ret = regmap_update_bits(arizona->regmap,
						 ARIZONA_ISOLATION_CONTROL,
						 ARIZONA_ISOLATE_DCVDD1, 0);
			if (ret != 0) {
				dev_err(arizona->dev,
					"Failed to connect DCVDD: %d\n", ret);
				goto err;
			}
		}
		break;
	default:
		if (arizona->external_dcvdd)
			arizona_disable_reset(arizona);

		ret = arizona_wait_for_boot(arizona);
		if (ret != 0) {
			goto err;
		}

		mutex_lock(&arizona->reg_setting_lock);
		regmap_write(arizona->regmap, 0x80, 0x3);
		ret = regcache_sync_region(arizona->regmap, CLEARWATER_CP_MODE,
					   CLEARWATER_CP_MODE);
		regmap_write(arizona->regmap, 0x80, 0x0);
		mutex_unlock(&arizona->reg_setting_lock);

		if (ret != 0) {
			dev_err(arizona->dev, "Failed to restore keyed cache\n");
			goto err;
		}
		break;
	}

	ret = regcache_sync(arizona->regmap);
	if (ret != 0) {
		dev_err(arizona->dev,
			"Failed to restore 16-bit register cache\n");
		goto err;
	}

	if (arizona->regmap_32bit) {
		ret = regcache_sync(arizona->regmap_32bit);
		if (ret != 0) {
			dev_err(arizona->dev,
				"Failed to restore 32-bit register cache\n");
			goto err;
		}
	}

	ret = arizona_restore_dvfs(arizona);
	if (ret < 0)
		goto err;

	return 0;

err:
	regcache_cache_only(arizona->regmap, true);
	regulator_disable(arizona->dcvdd);
	return ret;
}

static int arizona_runtime_suspend(struct device *dev)
{
	struct arizona *arizona = dev_get_drvdata(dev);
	int ret;

	dev_dbg(arizona->dev, "Entering AoD mode\n");

	switch(arizona->type) {
	case WM5102:
	case WM8997:
	case WM8998:
	case WM1814:
		/* Must disable DVFS boost before powering down DCVDD */
		mutex_lock(&arizona->subsys_max_lock);
		arizona->subsys_max_cached = true;
		arizona_dvfs_remove_boost(arizona);
		mutex_unlock(&arizona->subsys_max_lock);
		break;
	default:
		break;
	}

	if (arizona->external_dcvdd) {
		switch (arizona->type) {
		case WM5102:
		case WM5110:
		case WM8997:
		case WM8280:
		case WM8998:
		case WM1814:
		case WM1831:
		case CS47L24:
			ret = regmap_update_bits(arizona->regmap,
						 ARIZONA_ISOLATION_CONTROL,
						 ARIZONA_ISOLATE_DCVDD1,
						 ARIZONA_ISOLATE_DCVDD1);
			if (ret != 0) {
				dev_err(arizona->dev, "Failed to isolate DCVDD: %d\n",
					ret);
				goto err;
			}
			break;
		default:
			break;
		}
	} else {
		switch (arizona->type) {
		case WM5110:
		case WM8280:
			/*
			 * As this is only called for the internal regulator
			 * (where we know voltage ranges available) it is ok
			 * to request an exact range.
			 */
			ret = regulator_set_voltage(arizona->dcvdd,
						    1175000, 1175000);
			if (ret < 0) {
				dev_err(arizona->dev,
					"Failed to set suspend voltage: %d\n",
					ret);
				goto err;
			}
			break;
		default:
			break;
		}
	}

	regcache_cache_only(arizona->regmap, true);
	regcache_mark_dirty(arizona->regmap);
	if (arizona->regmap_32bit)
		regcache_mark_dirty(arizona->regmap_32bit);
	regulator_disable(arizona->dcvdd);

	return 0;
err:
	arizona_restore_dvfs(arizona);
	return ret;
}
#else
static inline int arizona_dcvdd_notify(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int arizona_suspend_noirq(struct device *dev)
{
	struct arizona *arizona = dev_get_drvdata(dev);

	dev_dbg(arizona->dev, "Late suspend, reenabling IRQ\n");

	if (arizona->irq_sem) {
		enable_irq(arizona->irq);
		arizona->irq_sem = 0;
	}

	return 0;
}

static int arizona_suspend(struct device *dev)
{
	struct arizona *arizona = dev_get_drvdata(dev);

	dev_dbg(arizona->dev, "Early suspend, disabling IRQ\n");

	disable_irq(arizona->irq);
	arizona->irq_sem = 1;

	return 0;
}

static int arizona_resume_noirq(struct device *dev)
{
	struct arizona *arizona = dev_get_drvdata(dev);

	dev_dbg(arizona->dev, "Early resume, disabling IRQ\n");
	disable_irq(arizona->irq);

	arizona->irq_sem = 1;

	return 0;
}

#ifdef CONFIG_MFD_ARIZONA_DEFERRED_RESUME
static void arizona_resume_deferred(struct work_struct *work)
{
	struct arizona *arizona =
			container_of(work, struct arizona, deferred_resume_work);
	int level = -1;

	if (arizona->dapm)
		level = snd_power_get_state(arizona->dapm->card->snd_card);

	if ((arizona->dapm) && (level != SNDRV_CTL_POWER_D0)) {
		if (!schedule_work(&arizona->deferred_resume_work))
			dev_err(arizona->dev, "Resume work item may be lost\n");
	} else {
		dev_dbg(arizona->dev, "Deferred resume, reenabling IRQ\n");
		if (arizona->irq_sem) {
			enable_irq(arizona->irq);
			arizona->irq_sem = 0;
		}
	}
}
#endif

static int arizona_resume(struct device *dev)
{
	struct arizona *arizona = dev_get_drvdata(dev);
#ifdef CONFIG_MFD_ARIZONA_DEFERRED_RESUME
	int level = -1;

	if (arizona->dapm)
		level = snd_power_get_state(arizona->dapm->card->snd_card);

	if ((arizona->dapm) && (level != SNDRV_CTL_POWER_D0)) {
		if (!schedule_work(&arizona->deferred_resume_work))
			dev_err(dev, "Resume work item may be lost\n");
	} else {
#endif
		dev_dbg(arizona->dev, "Late resume, reenabling IRQ\n");
		if (arizona->irq_sem) {
			enable_irq(arizona->irq);
			arizona->irq_sem = 0;
		}
#ifdef CONFIG_MFD_ARIZONA_DEFERRED_RESUME
	}
#endif

	return 0;
}
#endif

const struct dev_pm_ops arizona_pm_ops = {
	SET_RUNTIME_PM_OPS(arizona_runtime_suspend,
			   arizona_runtime_resume,
			   NULL)
	SET_SYSTEM_SLEEP_PM_OPS(arizona_suspend, arizona_resume)
#ifdef CONFIG_PM_SLEEP
	.suspend_noirq = arizona_suspend_noirq,
	.resume_noirq = arizona_resume_noirq,
#endif
};
EXPORT_SYMBOL_GPL(arizona_pm_ops);

#ifdef CONFIG_OF
unsigned long arizona_of_get_type(struct device *dev)
{
	const struct of_device_id *id = of_match_device(arizona_of_match, dev);

	if (id)
		return (unsigned long)id->data;
	else
		return 0;
}
EXPORT_SYMBOL_GPL(arizona_of_get_type);

int arizona_of_get_named_gpio(struct arizona *arizona, const char *prop,
			      bool mandatory)
{
	int gpio;

	gpio = of_get_named_gpio(arizona->dev->of_node, prop, 0);
	if (gpio < 0) {
		if (mandatory)
			dev_err(arizona->dev,
				"Mandatory DT gpio %s missing/malformed: %d\n",
				prop, gpio);

		gpio = 0;
	}

	return gpio;
}
EXPORT_SYMBOL_GPL(arizona_of_get_named_gpio);

int arizona_of_read_u32_array(struct arizona *arizona,
			      const char *prop, bool mandatory,
			      u32 *data, size_t num)
{
	int ret;

	ret = of_property_read_u32_array(arizona->dev->of_node, prop,
					 data, num);

	if (ret >= 0)
		return 0;

	switch (ret) {
	case -EINVAL:
		if (mandatory)
			dev_err(arizona->dev,
				"Mandatory DT property %s is missing\n",
				prop);
		break;
	default:
		dev_err(arizona->dev,
			"DT property %s is malformed: %d\n",
			prop, ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(arizona_of_read_u32_array);

int arizona_of_read_u32(struct arizona *arizona,
			       const char* prop, bool mandatory,
			       u32 *data)
{
	return arizona_of_read_u32_array(arizona, prop, mandatory, data, 1);
}
EXPORT_SYMBOL_GPL(arizona_of_read_u32);

static int arizona_of_get_max_channels(struct arizona *arizona,
				       const char *prop)
{
	struct arizona_pdata *pdata = &arizona->pdata;
	struct device_node *np = arizona->dev->of_node;
	struct property *tempprop;
	const __be32 *cur;
	u32 val;
	int i;

	i = 0;
	of_property_for_each_u32(np, prop, tempprop, cur, val) {
		if (i == ARRAY_SIZE(pdata->max_channels_clocked))
			break;

		pdata->max_channels_clocked[i++] = val;
	}

	return 0;
}

static int arizona_of_get_inmode(struct arizona *arizona,
					const char *prop)
{
	struct arizona_pdata *pdata = &arizona->pdata;
	struct device_node *np = arizona->dev->of_node;
	struct property *tempprop;
	const __be32 *cur;
	u32 val;
	int i;

	i = 0;
	of_property_for_each_u32(np, prop, tempprop, cur, val) {
		if (i == ARRAY_SIZE(pdata->inmode))
			break;

		pdata->inmode[i++] = val;
	}

	return 0;
}

static int arizona_of_get_dmicref(struct arizona *arizona,
					const char *prop)
{
	struct arizona_pdata *pdata = &arizona->pdata;
	struct device_node *np = arizona->dev->of_node;
	struct property *tempprop;
	const __be32 *cur;
	u32 val;
	int i;

	i = 0;
	of_property_for_each_u32(np, prop, tempprop, cur, val) {
		if (i == ARRAY_SIZE(pdata->dmic_ref))
			break;

		pdata->dmic_ref[i++] = val;
	}

	return 0;
}

static int arizona_of_get_dmic_clksrc(struct arizona *arizona,
					const char *prop)
{
	struct arizona_pdata *pdata = &arizona->pdata;
	struct device_node *np = arizona->dev->of_node;
	struct property *tempprop;
	const __be32 *cur;
	u32 val;
	int i;

	i = 0;
	of_property_for_each_u32(np, prop, tempprop, cur, val) {
		if (i == ARRAY_SIZE(pdata->dmic_clksrc))
			break;

		pdata->dmic_clksrc[i++] = val;
	}

	return 0;
}

static int arizona_of_get_gpio_defaults(struct arizona *arizona,
					const char *prop)
{
	struct arizona_pdata *pdata = &arizona->pdata;
	struct device_node *np = arizona->dev->of_node;
	struct property *tempprop;
	const __be32 *cur;
	u32 val;
	int i;

	i = 0;
	of_property_for_each_u32(np, prop, tempprop, cur, val) {
		if (i == ARRAY_SIZE(pdata->gpio_defaults))
			break;

		pdata->gpio_defaults[i++] = val;
	}
	if (!i)
		return 0;

	/*
	 * All values are literal except out of range values
	 * which are chip default, translate into platform
	 * data which uses 0 as chip default and out of range
	 * as zero.
	 */
	for (i = 0; i < ARRAY_SIZE(pdata->gpio_defaults); i++) {
		if (pdata->gpio_defaults[i] > 0xffff)
			pdata->gpio_defaults[i] = 0;
		else if (pdata->gpio_defaults[i] == 0)
			pdata->gpio_defaults[i] = 0x10000;
	}

	return 0;
}

static int arizona_of_get_u32_num_groups(struct arizona *arizona,
					const char *prop,
					int group_size)
{
	int len_prop;
	int num_groups;

	if (!of_get_property(arizona->dev->of_node, prop, &len_prop))
		return -EINVAL;

	num_groups =  len_prop / (group_size * sizeof(u32));

	if (num_groups * group_size * sizeof(u32) != len_prop) {
		dev_err(arizona->dev,
			"DT property %s is malformed: %d\n",
			prop, -EOVERFLOW);
		return -EOVERFLOW;
	}

	return num_groups;
}

static int arizona_of_get_micd_ranges(struct arizona *arizona,
				      const char *prop)
{
	int nranges;
	int i, j;
	int ret = 0;
	u32 value;
	struct arizona_micd_range *micd_ranges;

	nranges = arizona_of_get_u32_num_groups(arizona, prop, 2);
	if (nranges < 0)
		return nranges;

	micd_ranges = devm_kzalloc(arizona->dev,
				   nranges * sizeof(struct arizona_micd_range),
				   GFP_KERNEL);

	for (i = 0, j = 0; i < nranges; ++i) {
		ret = of_property_read_u32_index(arizona->dev->of_node,
						 prop, j++, &value);
		if (ret < 0)
			goto error;
		micd_ranges[i].max = value;

		ret = of_property_read_u32_index(arizona->dev->of_node,
						 prop, j++, &value);
		if (ret < 0)
			goto error;
		micd_ranges[i].key = value;
	}

	arizona->pdata.micd_ranges = micd_ranges;
	arizona->pdata.num_micd_ranges = nranges;

	return ret;

error:
	devm_kfree(arizona->dev, micd_ranges);
	dev_err(arizona->dev, "DT property %s is malformed: %d\n", prop, ret);
	return ret;
}

static int arizona_of_get_micd_configs(struct arizona *arizona,
				       const char *prop)
{
	int nconfigs;
	int i, j, group_size;
	int ret = 0;
	u32 value;
	struct arizona_micd_config *micd_configs;

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
	case CS47L35:
		group_size = 3;
		break;
	default:
		group_size = 4;
		break;
	}

	nconfigs = arizona_of_get_u32_num_groups(arizona, prop, group_size);
	if (nconfigs < 0)
		return nconfigs;

	micd_configs = devm_kzalloc(arizona->dev,
				    nconfigs *
				    sizeof(struct arizona_micd_config),
				    GFP_KERNEL);

	for (i = 0, j = 0; i < nconfigs; ++i) {
		ret = of_property_read_u32_index(arizona->dev->of_node,
						 prop, j++, &value);
		if (ret < 0)
			goto error;
		micd_configs[i].src = value;

		if (group_size == 4) {
			ret = of_property_read_u32_index(arizona->dev->of_node,
							 prop, j++, &value);
			if (ret < 0)
				goto error;
			micd_configs[i].gnd = value;
		}

		ret = of_property_read_u32_index(arizona->dev->of_node,
						 prop, j++, &value);
		if (ret < 0)
			goto error;
		micd_configs[i].bias = value;

		ret = of_property_read_u32_index(arizona->dev->of_node,
						 prop, j++, &value);
		if (ret < 0)
			goto error;
		micd_configs[i].gpio = value;
	}

	arizona->pdata.micd_configs = micd_configs;
	arizona->pdata.num_micd_configs = nconfigs;

	return ret;

error:
	devm_kfree(arizona->dev, micd_configs);
	dev_err(arizona->dev, "DT property %s is malformed: %d\n", prop, ret);

	return ret;
}

static int arizona_of_get_micbias(struct arizona *arizona,
				  const char *prop, int index,
				  int num_micbias_outputs)
{
	int ret, i;
	int j = 0;
	u32 micbias_config[4 + ARIZONA_MAX_CHILD_MICBIAS] = {0};

	ret = arizona_of_read_u32_array(arizona, prop, false,
					micbias_config,
					4 + num_micbias_outputs);

	if (ret >= 0) {
		arizona->pdata.micbias[index].mV = micbias_config[j++];
		arizona->pdata.micbias[index].ext_cap = micbias_config[j++];
		for (i = 0; i < num_micbias_outputs; i++)
			arizona->pdata.micbias[index].discharge[i] =
				micbias_config[j++];
		arizona->pdata.micbias[index].soft_start = micbias_config[j++];
		arizona->pdata.micbias[index].bypass = micbias_config[j++];
	}

	return ret;
}

static int arizona_of_get_core_pdata(struct arizona *arizona)
{
	struct arizona_pdata *pdata = &arizona->pdata;
	u32 out_mono[ARIZONA_MAX_OUTPUT];
	int i, num_micbias_outputs;

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
		num_micbias_outputs = 1;
		break;
	case CS47L35:
		num_micbias_outputs = MARLEY_NUM_CHILD_MICBIAS;
		break;
	default:
		num_micbias_outputs = MOON_NUM_CHILD_MICBIAS;
		break;
	}

	memset(&out_mono, 0, sizeof(out_mono));

	pdata->reset = arizona_of_get_named_gpio(arizona, "wlf,reset", true);
	pdata->ldoena = arizona_of_get_named_gpio(arizona, "wlf,ldoena", true);

	arizona_of_read_s32(arizona, "wlf,clk32k-src", false,
				&pdata->clk32k_src);

	arizona_of_get_micd_ranges(arizona, "wlf,micd-ranges");
	arizona_of_get_micd_configs(arizona, "wlf,micd-configs");

	arizona_of_get_micbias(arizona, "wlf,micbias1", 0, num_micbias_outputs);
	arizona_of_get_micbias(arizona, "wlf,micbias2", 1, num_micbias_outputs);
	arizona_of_get_micbias(arizona, "wlf,micbias3", 2, num_micbias_outputs);
	arizona_of_get_micbias(arizona, "wlf,micbias4", 3, num_micbias_outputs);

	arizona_of_get_gpio_defaults(arizona, "wlf,gpio-defaults");

	arizona_of_get_max_channels(arizona, "wlf,max-channels-clocked");

	arizona_of_get_dmicref(arizona, "wlf,dmic-ref");

	arizona_of_get_inmode(arizona, "wlf,inmode");

	arizona_of_get_dmic_clksrc(arizona, "wlf,dmic-clksrc");

	arizona_of_read_u32_array(arizona, "wlf,out-mono", false,
				  out_mono, ARRAY_SIZE(out_mono));
	for (i = 0; i < ARRAY_SIZE(out_mono); ++i)
		pdata->out_mono[i] = !!out_mono[i];

	arizona_of_read_u32(arizona, "wlf,wm5102t-output-pwr", false,
				&pdata->wm5102t_output_pwr);

	arizona_of_read_s32(arizona, "wlf,hpdet-ext-res", false,
				&pdata->hpdet_ext_res);

	pdata->rev_specific_fw = of_property_read_bool(arizona->dev->of_node,
						       "wlf,rev-specific-fw");
	return 0;
}

const struct of_device_id arizona_of_match[] = {
	{ .compatible = "wlf,wm5102", .data = (void *)WM5102 },
	{ .compatible = "wlf,wm8280", .data = (void *)WM8280 },
	{ .compatible = "wlf,wm5110", .data = (void *)WM5110 },
	{ .compatible = "wlf,wm8997", .data = (void *)WM8997 },
	{ .compatible = "wlf,wm8998", .data = (void *)WM8998 },
	{ .compatible = "wlf,wm1814", .data = (void *)WM1814 },
	{ .compatible = "wlf,wm8285", .data = (void *)WM8285 },
	{ .compatible = "wlf,wm1840", .data = (void *)WM1840 },
	{ .compatible = "wlf,wm1831", .data = (void *)WM1831 },
	{ .compatible = "cirrus,cs47l24", .data = (void *)CS47L24 },
	{ .compatible = "cirrus,cs47l35", .data = (void *)CS47L35 },
	{ .compatible = "cirrus,cs47l85", .data = (void *)WM8285 },
	{ .compatible = "cirrus,cs47l90", .data = (void *)CS47L90 },
	{ .compatible = "cirrus,cs47l91", .data = (void *)CS47L91 },
	{},
};
EXPORT_SYMBOL_GPL(arizona_of_match);
#else
static inline int arizona_of_get_core_pdata(struct arizona *arizona)
{
	return 0;
}
#endif

static const struct mfd_cell early_devs[] = {
	{ .name = "arizona-ldo1" },
};

static const char *wm5102_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"DBVDD3",
	"CPVDD",
	"SPKVDDL",
	"SPKVDDR",
};

static const struct mfd_cell wm5102_devs[] = {
	{ .name = "arizona-micsupp" },
	{
		.name = "arizona-extcon",
		.parent_supplies = wm5102_supplies,
		.num_parent_supplies = 1, /* We only need MICVDD */
	},
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{
		.name = "wm5102-codec",
		.parent_supplies = wm5102_supplies,
		.num_parent_supplies = ARRAY_SIZE(wm5102_supplies),
	},
};

static const struct mfd_cell florida_devs[] = {
	{ .name = "arizona-micsupp" },
	{
		.name = "arizona-extcon",
		.parent_supplies = wm5102_supplies,
		.num_parent_supplies = 1, /* We only need MICVDD */
	},
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{
		.name = "florida-codec",
		.parent_supplies = wm5102_supplies,
		.num_parent_supplies = ARRAY_SIZE(wm5102_supplies),
	},
};

static const char *cs47l24_supplies[] = {
	"MICVDD",
	"CPVDD",
	"SPKVDD",
};

static const struct mfd_cell largo_devs[] = {
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{
		.name = "largo-codec",
		.parent_supplies = cs47l24_supplies,
		.num_parent_supplies = ARRAY_SIZE(cs47l24_supplies),
	},
};

static const char *wm8997_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"CPVDD",
	"SPKVDD",
};

static const struct mfd_cell wm8997_devs[] = {
	{ .name = "arizona-micsupp" },
	{
		.name = "arizona-extcon",
		.parent_supplies = wm8997_supplies,
		.num_parent_supplies = 1, /* We only need MICVDD */
	},
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{
		.name = "wm8997-codec",
		.parent_supplies = wm8997_supplies,
		.num_parent_supplies = ARRAY_SIZE(wm8997_supplies),
	},
};

static const struct mfd_cell vegas_devs[] = {
	{ .name = "arizona-micsupp" },
	{ .name = "arizona-extcon" },
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{
		.name = "vegas-codec",
		.parent_supplies = wm5102_supplies,
		.num_parent_supplies = ARRAY_SIZE(wm5102_supplies),
	},
};

static const char *clearwater_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"DBVDD3",
	"DBVDD4",
	"CPVDD",
	"SPKVDDL",
	"SPKVDDR",
};

static const struct mfd_cell clearwater_devs[] = {
	{ .name = "arizona-micsupp" },
	{ .name = "arizona-extcon" },
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{
		.name = "clearwater-codec",
		.parent_supplies = clearwater_supplies,
		.num_parent_supplies = ARRAY_SIZE(clearwater_supplies),
	},
};

static const char *marley_supplies[] = {
	"MICVDD",
	"DBVDD2",
	"CPVDD",
	"SPKVDD",
};

static const struct mfd_cell marley_devs[] = {
	{ .name = "arizona-micsupp" },
	{ .name = "arizona-extcon" },
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{ .name = "marley-codec" },
	{
		.name = "marley-codec",
		.parent_supplies = marley_supplies,
		.num_parent_supplies = ARRAY_SIZE(marley_supplies),
	},
};

static struct mfd_cell moon_devs[] = {
	{ .name = "arizona-micsupp" },
	{ .name = "arizona-extcon" },
	{ .name = "arizona-gpio" },
	{ .name = "arizona-haptics" },
	{ .name = "arizona-pwm" },
	{ .name = "moon-codec" },
};

static const struct {
	unsigned int enable;
	unsigned int conf_reg;
	unsigned int vol_reg;
	unsigned int adc_reg;
} arizona_florida_channel_defs[] = {
	{
		ARIZONA_IN1R_ENA, ARIZONA_IN1L_CONTROL,
		ARIZONA_ADC_DIGITAL_VOLUME_1R, ARIZONA_ADC_VCO_CAL_5
	},
	{
		ARIZONA_IN1L_ENA, ARIZONA_IN1L_CONTROL,
		ARIZONA_ADC_DIGITAL_VOLUME_1L, ARIZONA_ADC_VCO_CAL_4
	},
	{
		ARIZONA_IN2R_ENA, ARIZONA_IN2L_CONTROL,
		ARIZONA_ADC_DIGITAL_VOLUME_2R, ARIZONA_ADC_VCO_CAL_7
	},
	{
		ARIZONA_IN2L_ENA, ARIZONA_IN2L_CONTROL,
		ARIZONA_ADC_DIGITAL_VOLUME_2L, ARIZONA_ADC_VCO_CAL_6
	},
	{
		ARIZONA_IN3R_ENA, ARIZONA_IN3L_CONTROL,
		ARIZONA_ADC_DIGITAL_VOLUME_3R, ARIZONA_ADC_VCO_CAL_9
	},
	{
		ARIZONA_IN3L_ENA, ARIZONA_IN3L_CONTROL,
		ARIZONA_ADC_DIGITAL_VOLUME_3L, ARIZONA_ADC_VCO_CAL_8
	},
};

void arizona_florida_mute_analog(struct arizona* arizona,
				 unsigned int mute)
{
	unsigned int val, chans;
	int i;

	regmap_read(arizona->regmap, ARIZONA_INPUT_ENABLES_STATUS, &chans);

	for (i = 0; i < ARRAY_SIZE(arizona_florida_channel_defs); ++i) {
		if (!(chans & arizona_florida_channel_defs[i].enable))
			continue;

		/* Check for analogue input */
		regmap_read(arizona->regmap,
			    arizona_florida_channel_defs[i].conf_reg,
			    &val);
		if (val & 0x0400)
			continue;

		regmap_update_bits(arizona->regmap,
				   arizona_florida_channel_defs[i].vol_reg,
				   ARIZONA_IN1L_MUTE,
				   mute);
	}
}
EXPORT_SYMBOL_GPL(arizona_florida_mute_analog);

static bool arizona_florida_get_input_state(struct arizona* arizona)
{
	unsigned int val, chans;
	int count, i, j;

	regmap_read(arizona->regmap, ARIZONA_INPUT_ENABLES_STATUS, &chans);

	for (i = 0; i < ARRAY_SIZE(arizona_florida_channel_defs); ++i) {
		if (!(chans & arizona_florida_channel_defs[i].enable))
			continue;

		/* Check for analogue input */
		regmap_read(arizona->regmap,
			    arizona_florida_channel_defs[i].conf_reg,
			    &val);
		if (val & 0x0400)
			continue;

		count = 0;

		for (j = 0; j < 4; ++j) {
			regmap_read(arizona->regmap,
				    arizona_florida_channel_defs[i].adc_reg,
				    &val);
			val &= ARIZONA_ADC1L_COUNT_RD_MASK;
			val >>= ARIZONA_ADC1L_COUNT_RD_SHIFT;

			dev_dbg(arizona->dev, "ADC Count: %d\n", val);

			if (val > 78 || val < 54)
				count++;
		}

		if (count == j)
			return true;
	}

	return false;
}

void arizona_florida_clear_input(struct arizona *arizona)
{
	mutex_lock(&arizona->reg_setting_lock);
	regmap_write(arizona->regmap, 0x80, 0x3);

	if (arizona_florida_get_input_state(arizona)) {
		arizona_florida_mute_analog(arizona, ARIZONA_IN1L_MUTE);

		regmap_write(arizona->regmap, 0x3A6, 0x5555);
		regmap_write(arizona->regmap, 0x3A5, 0x3);
		msleep(10);
		regmap_write(arizona->regmap, 0x3A5, 0x0);

		if (arizona_florida_get_input_state(arizona)) {
			regmap_write(arizona->regmap, 0x3A6, 0xAAAA);
			regmap_write(arizona->regmap, 0x3A5, 0x5);
			msleep(10);
			regmap_write(arizona->regmap, 0x3A5, 0x0);
		}

		regmap_write(arizona->regmap, 0x3A6, 0x0);

		msleep(5);

		arizona_florida_mute_analog(arizona, 0);
	}

	regmap_write(arizona->regmap, 0x80, 0x0);
	mutex_unlock(&arizona->reg_setting_lock);
}
EXPORT_SYMBOL_GPL(arizona_florida_clear_input);

int arizona_get_num_micbias(struct arizona *arizona,
	unsigned int *micbiases, unsigned int *child_micbiases)
{
	unsigned int num_micbiases, num_child_micbiases;

	if (!arizona)
		return -EINVAL;

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
		num_micbiases = ARIZONA_MAX_MICBIAS;
		num_child_micbiases = 0;
		break;
	case CS47L35:
		num_micbiases = MARLEY_NUM_MICBIAS;
		num_child_micbiases = MARLEY_NUM_CHILD_MICBIAS;
		break;
	default:
		num_micbiases = MOON_NUM_MICBIAS;
		num_child_micbiases = MOON_NUM_CHILD_MICBIAS;
		break;
	}

	if (micbiases)
		*micbiases = num_micbiases;
	if (child_micbiases)
		*child_micbiases = num_child_micbiases;

	return 0;
}
EXPORT_SYMBOL_GPL(arizona_get_num_micbias);

int arizona_dev_init(struct arizona *arizona)
{
	struct device *dev = arizona->dev;
	const char *type_name = "Unknown";
	unsigned int reg, val, mask;
	int (*apply_patch)(struct arizona *) = NULL;
	int ret, i, max_inputs, j;
	unsigned int max_micbias = 0, num_child_micbias = 0;
	unsigned int num_dmic_clksrc = 0;

	dev_set_drvdata(arizona->dev, arizona);
	mutex_init(&arizona->clk_lock);
	mutex_init(&arizona->subsys_max_lock);
	mutex_init(&arizona->reg_setting_lock);
	mutex_init(&arizona->rate_lock);

	if (dev_get_platdata(arizona->dev))
		memcpy(&arizona->pdata, dev_get_platdata(arizona->dev),
		       sizeof(arizona->pdata));
	else
		arizona_of_get_core_pdata(arizona);

	regcache_cache_only(arizona->regmap, true);

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8997:
	case WM8280:
	case WM8998:
	case WM1814:
	case WM8285:
	case WM1840:
	case WM1831:
	case CS47L24:
	case CS47L35:
	case CS47L90:
	case CS47L91:
		for (i = 0; i < ARRAY_SIZE(wm5102_core_supplies); i++)
			arizona->core_supplies[i].supply
				= wm5102_core_supplies[i];
		arizona->num_core_supplies = ARRAY_SIZE(wm5102_core_supplies);
		break;
	default:
		dev_err(arizona->dev, "Unknown device type %d\n",
			arizona->type);
		return -EINVAL;
	}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_MFD_ARIZONA_DEFERRED_RESUME)
		/* deferred resume work */
		INIT_WORK(&arizona->deferred_resume_work, arizona_resume_deferred);
#endif

	/* Mark DCVDD as external, LDO1 driver will clear if internal */
	arizona->external_dcvdd = true;

	switch (arizona->type) {
	case WM1831:
	case CS47L24:
	case CS47L35:
	case CS47L90:
	case CS47L91:
		break;
	default:
		ret = mfd_add_devices(arizona->dev, -1, early_devs,
				      ARRAY_SIZE(early_devs), NULL, 0, NULL);
		if (ret != 0) {
			dev_err(dev, "Failed to add early children: %d\n", ret);
			return ret;
		}
		break;
	}

	ret = devm_regulator_bulk_get(dev, arizona->num_core_supplies,
				      arizona->core_supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to request core supplies: %d\n",
			ret);
		goto err_early;
	}

	/**
	 * Don't use devres here because the only device we have to get
	 * against is the MFD device and DCVDD will likely be supplied by
	 * one of its children. Meaning that the regulator will be
	 * destroyed by the time devres calls regulator put.
	 */
	arizona->dcvdd = regulator_get(arizona->dev, "DCVDD");
	if (IS_ERR(arizona->dcvdd)) {
		ret = PTR_ERR(arizona->dcvdd);
		dev_err(dev, "Failed to request DCVDD: %d\n", ret);
		goto err_early;
	}

	arizona->dcvdd_notifier.notifier_call = arizona_dcvdd_notify;
	ret = regulator_register_notifier(arizona->dcvdd,
					  &arizona->dcvdd_notifier);
	if (ret < 0) {
		dev_err(dev, "Failed to register DCVDD notifier %d\n", ret);
		goto err_dcvdd;
	}

	if (arizona->pdata.reset) {
		/* Start out with /RESET low to put the chip into reset */
		ret = devm_gpio_request_one(arizona->dev, arizona->pdata.reset,
					    GPIOF_DIR_OUT | GPIOF_INIT_LOW,
					    "arizona /RESET");
		if (ret != 0) {
			dev_err(dev, "Failed to request /RESET: %d\n", ret);
			goto err_notifier;
		}
	}

	/* Ensure period of reset asserted before we apply the supplies */
	msleep(20);

	ret = regulator_bulk_enable(arizona->num_core_supplies,
				    arizona->core_supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable core supplies: %d\n",
			ret);
		goto err_notifier;
	}

	ret = regulator_enable(arizona->dcvdd);
	if (ret != 0) {
		dev_err(dev, "Failed to enable DCVDD: %d\n", ret);
		goto err_enable;
	}

	arizona_disable_reset(arizona);

	regcache_cache_only(arizona->regmap, false);

	/* Verify that this is a chip we know about */
	ret = regmap_read(arizona->regmap, ARIZONA_SOFTWARE_RESET, &reg);
	if (ret != 0) {
		dev_err(dev, "Failed to read ID register: %d\n", ret);
		goto err_reset;
	}

	switch (reg) {
	case 0x5102:
	case 0x5110:
	case 0x6349:
	case 0x6363:
	case 0x8997:
	case 0x6338:
	case 0x6360:
	case 0x6364:
		break;
	default:
		dev_err(arizona->dev, "Unknown device ID: %x\n", reg);
		goto err_reset;
	}

	/* If we have a /RESET GPIO we'll already be reset */
	if (!arizona->pdata.reset) {
		ret = arizona_soft_reset(arizona);
		if (ret != 0)
			goto err_reset;
	}

	/* Ensure device startup is complete */
	switch (arizona->type) {
	case WM5102:
		ret = regmap_read(arizona->regmap,
				  ARIZONA_WRITE_SEQUENCER_CTRL_3, &val);
		if (ret) {
			dev_err(dev,
				"Failed to check write sequencer state: %d\n",
				ret);
		} else if (val & 0x01) {
			ret = wm5102_clear_write_sequencer(arizona);
			if (ret)
				return ret;
		}
		break;
	default:
		break;
	}

	ret = arizona_wait_for_boot(arizona);
	if (ret) {
		dev_err(arizona->dev, "Device failed initial boot: %d\n", ret);
		goto err_reset;
	}

	/* Read the device ID information & do device specific stuff */
	ret = regmap_read(arizona->regmap, ARIZONA_SOFTWARE_RESET, &reg);
	if (ret != 0) {
		dev_err(dev, "Failed to read ID register: %d\n", ret);
		goto err_reset;
	}

	ret = regmap_read(arizona->regmap, ARIZONA_DEVICE_REVISION,
			  &arizona->rev);
	if (ret != 0) {
		dev_err(dev, "Failed to read revision register: %d\n", ret);
		goto err_reset;
	}
	arizona->rev &= ARIZONA_DEVICE_REVISION_MASK;

	switch (reg) {
#ifdef CONFIG_MFD_WM5102
	case 0x5102:
		type_name = "WM5102";
		if (arizona->type != WM5102) {
			dev_err(arizona->dev, "WM5102 registered as %d\n",
				arizona->type);
			arizona->type = WM5102;
		}
		apply_patch = wm5102_patch;
		arizona->rev &= 0x7;
		break;
#endif
#ifdef CONFIG_MFD_FLORIDA
	case 0x5110:
		switch (arizona->type) {
		case WM8280:
			if (arizona->rev >= 0x5)
				type_name = "WM8281";
			else
				type_name = "WM8280";
			break;

		case WM5110:
			type_name = "WM5110";
			break;

		default:
			dev_err(arizona->dev, "Florida codec registered as %d\n",
				arizona->type);
			arizona->type = WM8280;
			type_name = "Florida";
			break;
		}
		apply_patch = florida_patch;
		break;
#endif
#ifdef CONFIG_MFD_LARGO
	case 0x6363:
		switch (arizona->type) {
		case CS47L24:
			type_name = "CS47L24";
			break;

		case WM1831:
			type_name = "WM1831";
			break;

		default:
			dev_err(arizona->dev, "Largo codec registered as %d\n",
				arizona->type);
			arizona->type = CS47L24;
			type_name = "Largo";
			break;
		}
		apply_patch = largo_patch;
		break;
#endif
#ifdef CONFIG_MFD_WM8997
	case 0x8997:
		type_name = "WM8997";
		if (arizona->type != WM8997) {
			dev_err(arizona->dev, "WM8997 registered as %d\n",
				arizona->type);
			arizona->type = WM8997;
		}
		apply_patch = wm8997_patch;
		break;
#endif
#ifdef CONFIG_MFD_VEGAS
	case 0x6349:
		switch (arizona->type) {
		case WM8998:
			type_name = "WM8998";
			break;

		case WM1814:
			type_name = "WM1814";
			break;

		default:
			dev_err(arizona->dev,
				"Unknown Vegas codec registered as WM8998\n");
			arizona->type = WM8998;
		}

		apply_patch = vegas_patch;
		break;
#endif
#ifdef CONFIG_MFD_CLEARWATER
	case 0x6338:
		switch (arizona->type) {
		case WM8285:
			type_name = "CS47L85";
			break;

		case WM1840:
			type_name = "WM1840";
			break;

		default:
			dev_err(arizona->dev,
				"Unknown Clearwater codec registered as CS47L85\n");
			arizona->type = WM8285;
		}

		apply_patch = clearwater_patch;
		break;
#endif
#ifdef CONFIG_MFD_MARLEY
	case 0x6360:
		switch (arizona->type) {
		case CS47L35:
			type_name = "CS47L35";
			break;

		default:
			dev_err(arizona->dev,
			   "Unknown Marley codec registered as CS47L35\n");
			arizona->type = CS47L35;
		}

		apply_patch = marley_patch;
		break;
#endif
#ifdef CONFIG_MFD_MOON
	case 0x6364:
		switch (arizona->type) {
		case CS47L90:
			type_name = "CS47L90";
			break;
		case CS47L91:
			type_name = "CS47L91";
			break;
		default:
			dev_err(arizona->dev,
				"Unknown Moon codec registered as CS47L90\n");
			arizona->type = CS47L90;
		}

		apply_patch = moon_patch;
		break;
#endif
	default:
		dev_err(arizona->dev, "Unknown device ID %x\n", reg);
		goto err_reset;
	}

	dev_info(dev, "%s revision %c\n", type_name, arizona->rev + 'A');

	if (apply_patch) {
		ret = apply_patch(arizona);
		if (ret != 0) {
			dev_err(arizona->dev, "Failed to apply patch: %d\n",
				ret);
			goto err_reset;
		}

		switch (arizona->type) {
		case WM5102:
			ret = wm5102_apply_hardware_patch(arizona);
			if (ret) {
				dev_err(arizona->dev,
					"Failed to apply hardware patch: %d\n",
					ret);
				goto err_reset;
			}
			break;
		case WM5110:
		case WM8280:
			ret = wm5110_apply_sleep_patch(arizona);
			if (ret) {
				dev_err(arizona->dev,
					"Failed to apply sleep patch: %d\n",
					ret);
				goto err_reset;
			}
			break;
		default:
			break;
		}
	}

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8280:
	case WM8997:
	case WM8998:
	case WM1814:
	case WM1831:
	case CS47L24:
		for (i = 0; i < ARIZONA_MAX_GPIO_REGS; i++) {
			if (!arizona->pdata.gpio_defaults[i])
				continue;

			regmap_write(arizona->regmap, ARIZONA_GPIO1_CTRL + i,
				     arizona->pdata.gpio_defaults[i]);
		}
		break;
	default:
		for (i = 0; i < ARRAY_SIZE(arizona->pdata.gpio_defaults); i++) {
			if (!arizona->pdata.gpio_defaults[i])
				continue;

			regmap_write(arizona->regmap, CLEARWATER_GPIO1_CTRL_1 + i,
				     arizona->pdata.gpio_defaults[i]);
		}
		break;
	}

	/* Chip default */
	if (!arizona->pdata.clk32k_src)
		arizona->pdata.clk32k_src = ARIZONA_32KZ_MCLK2;

	switch (arizona->pdata.clk32k_src) {
	case ARIZONA_32KZ_MCLK1:
	case ARIZONA_32KZ_MCLK2:
		regmap_update_bits(arizona->regmap, ARIZONA_CLOCK_32K_1,
				   ARIZONA_CLK_32K_SRC_MASK,
				   arizona->pdata.clk32k_src - 1);
		arizona_clk32k_enable(arizona);
		break;
	case ARIZONA_32KZ_NONE:
		regmap_update_bits(arizona->regmap, ARIZONA_CLOCK_32K_1,
				   ARIZONA_CLK_32K_SRC_MASK, 2);
		break;
	default:
		dev_err(arizona->dev, "Invalid 32kHz clock source: %d\n",
			arizona->pdata.clk32k_src);
		ret = -EINVAL;
		goto err_reset;
	}

	pm_runtime_set_autosuspend_delay(arizona->dev, 100);
	pm_runtime_use_autosuspend(arizona->dev);
	pm_runtime_enable(arizona->dev);

	pm_runtime_get(dev);
	arizona_get_num_micbias(arizona, &max_micbias, &num_child_micbias);

	for (i = 0; i < max_micbias; i++) {
		if (!arizona->pdata.micbias[i].mV &&
		    !arizona->pdata.micbias[i].bypass)
			continue;

		/* Apply default for bypass mode */
		if (!arizona->pdata.micbias[i].mV)
			arizona->pdata.micbias[i].mV = 2800;

		val = (arizona->pdata.micbias[i].mV - 1500) / 100;

		mask = ARIZONA_MICB1_LVL_MASK | ARIZONA_MICB1_EXT_CAP |
			ARIZONA_MICB1_BYPASS | ARIZONA_MICB1_RATE;

		val <<= ARIZONA_MICB1_LVL_SHIFT;

		if (arizona->pdata.micbias[i].ext_cap)
			val |= ARIZONA_MICB1_EXT_CAP;

		if (num_child_micbias == 0) {
			mask |= ARIZONA_MICB1_DISCH;
			if (arizona->pdata.micbias[i].discharge[0])
				val |= ARIZONA_MICB1_DISCH;
		}

		if (arizona->pdata.micbias[i].soft_start)
			val |= ARIZONA_MICB1_RATE;

		if (arizona->pdata.micbias[i].bypass)
			val |= ARIZONA_MICB1_BYPASS;

		regmap_update_bits(arizona->regmap,
			ARIZONA_MIC_BIAS_CTRL_1 + i,
			mask, val);

		if (num_child_micbias) {
			val = 0;
			mask = 0;
			for (j = 0; j < num_child_micbias; j++) {
				mask |= (ARIZONA_MICB1A_DISCH << j*4);
				if (arizona->pdata.micbias[i].discharge[j])
					val |= (ARIZONA_MICB1A_DISCH << j*4);
			}
			regmap_update_bits(arizona->regmap,
				ARIZONA_MIC_BIAS_CTRL_5 + i*2,
				mask, val);
		}
	}

	switch (arizona->type) {
	case WM5102:
	case WM5110:
	case WM8280:
		/* These arizona chips have 4 inputs and
		settings for INxL and INxR are same*/
		max_inputs = 4;
		break;
	case WM8997:
	case WM1831:
	case CS47L24:
	case WM8998:
	case WM1814:
	case CS47L35:
		max_inputs = 2;
		break;
	case WM8285:
	case WM1840:
		/* DMIC Ref for IN4-6 is fixed for WM8285/1840 and
		settings for INxL and INxR are different*/
		max_inputs = 3;
		break;
	default:
		/*DMIC Ref for IN3-5 is fixed for CS47L90 and
		settings for INxL and INxR are different*/
		max_inputs = 2;
		/* For CS47L90/91 dmic clk src can be set same as
		pdm speaker clock this is used when pdm speaker
		feedsback IV data via pdm input */
		num_dmic_clksrc = 5;
		break;
	}

	for (i = 0; i < num_dmic_clksrc; i++) {
		regmap_update_bits(arizona->regmap,
			   ARIZONA_IN1R_CONTROL + (i * 8),
			   MOON_IN1_DMICCLK_SRC_MASK,
			   (arizona->pdata.dmic_clksrc[i])
				<< MOON_IN1_DMICCLK_SRC_SHIFT);
	}

	for (i = 0; i < max_inputs; i++) {
		switch (arizona->type) {
		case WM5102:
		case WM5110:
		case WM8997:
		case WM8280:
		case WM1831:
		case CS47L24:
			val = arizona->pdata.dmic_ref[i]
				<< ARIZONA_IN1_DMIC_SUP_SHIFT;
			val |= (arizona->pdata.inmode[i] & 2)
				<< (ARIZONA_IN1_MODE_SHIFT - 1);
			val |= (arizona->pdata.inmode[i] & 1)
				<< ARIZONA_IN1_SINGLE_ENDED_SHIFT;
			mask = ARIZONA_IN1_DMIC_SUP_MASK |
					ARIZONA_IN1_MODE_MASK |
					ARIZONA_IN1_SINGLE_ENDED_MASK;
			break;
		case WM8998:
		case WM1814:
			val = arizona->pdata.dmic_ref[i]
				<< ARIZONA_IN1_DMIC_SUP_SHIFT;
			val |= (arizona->pdata.inmode[i] & 2)
				<< (ARIZONA_IN1_MODE_SHIFT - 1);
			mask = ARIZONA_IN1_DMIC_SUP_MASK |
					ARIZONA_IN1_MODE_MASK;
			regmap_update_bits(arizona->regmap,
				   ARIZONA_ADC_DIGITAL_VOLUME_1L + (i * 8),
				   ARIZONA_IN1L_SRC_SE_MASK,
				   (arizona->pdata.inmode[i] & 1)
					<< ARIZONA_IN1L_SRC_SE_SHIFT);
			regmap_update_bits(arizona->regmap,
				   ARIZONA_ADC_DIGITAL_VOLUME_1R + (i * 8),
				   ARIZONA_IN1R_SRC_SE_MASK,
				   (arizona->pdata.inmode[i] & 1)
					<< ARIZONA_IN1R_SRC_SE_SHIFT);
			break;
		default:
			val = arizona->pdata.dmic_ref[i]
				<< ARIZONA_IN1_DMIC_SUP_SHIFT;
			val |= (arizona->pdata.inmode[2*i] & 2)
				<< (ARIZONA_IN1_MODE_SHIFT - 1);
			mask = ARIZONA_IN1_DMIC_SUP_MASK |
					ARIZONA_IN1_MODE_MASK;
			regmap_update_bits(arizona->regmap,
				   ARIZONA_ADC_DIGITAL_VOLUME_1L + (i * 8),
				   ARIZONA_IN1L_SRC_SE_MASK,
				   (arizona->pdata.inmode[2*i] & 1)
					<< ARIZONA_IN1L_SRC_SE_SHIFT);
			regmap_update_bits(arizona->regmap,
				   ARIZONA_ADC_DIGITAL_VOLUME_1R + (i * 8),
				   ARIZONA_IN1R_SRC_SE_MASK,
				   (arizona->pdata.inmode[(2*i) + 1] & 1)
					<< ARIZONA_IN1R_SRC_SE_SHIFT);
			break;
		}

		regmap_update_bits(arizona->regmap,
				   ARIZONA_IN1L_CONTROL + (i * 8), mask, val);
	}

	for (i = 0; i < ARIZONA_MAX_OUTPUT; i++) {
		/* Default is 0 so noop with defaults */
		if (arizona->pdata.out_mono[i])
			val = ARIZONA_OUT1_MONO;
		else
			val = 0;

		regmap_update_bits(arizona->regmap,
				   ARIZONA_OUTPUT_PATH_CONFIG_1L + (i * 8),
				   ARIZONA_OUT1_MONO, val);
	}

	for (i = 0; i < ARIZONA_MAX_PDM_SPK; i++) {
		if (arizona->pdata.spk_mute[i])
			regmap_update_bits(arizona->regmap,
					   ARIZONA_PDM_SPK1_CTRL_1 + (i * 2),
					   ARIZONA_SPK1_MUTE_ENDIAN_MASK |
					   ARIZONA_SPK1_MUTE_SEQ1_MASK,
					   arizona->pdata.spk_mute[i]);

		if (arizona->pdata.spk_fmt[i])
			regmap_update_bits(arizona->regmap,
					   ARIZONA_PDM_SPK1_CTRL_2 + (i * 2),
					   ARIZONA_SPK1_FMT_MASK,
					   arizona->pdata.spk_fmt[i]);
	}

	pm_runtime_set_active(arizona->dev);
	pm_runtime_enable(arizona->dev);

	/* Set up for interrupts */
	ret = arizona_irq_init(arizona);
	if (ret != 0)
		goto err_reset;

	pm_runtime_set_autosuspend_delay(arizona->dev, 100);
	pm_runtime_use_autosuspend(arizona->dev);

	arizona_request_irq(arizona, ARIZONA_IRQ_CLKGEN_ERR, "CLKGEN error",
			    arizona_clkgen_err, arizona);
	arizona_request_irq(arizona, ARIZONA_IRQ_OVERCLOCKED, "Overclocked",
			    arizona_overclocked, arizona);
	arizona_request_irq(arizona, ARIZONA_IRQ_UNDERCLOCKED, "Underclocked",
			    arizona_underclocked, arizona);

	/**
	 * Give us a sane default for the headphone impedance in case the
	 * extcon driver is not used
	 */
	arizona->hp_impedance = 32;

	switch (arizona->type) {
	case WM5102:
		ret = mfd_add_devices(arizona->dev, -1, wm5102_devs,
				      ARRAY_SIZE(wm5102_devs), NULL, 0, NULL);
		break;
	case WM8280:
	case WM5110:
		ret = mfd_add_devices(arizona->dev, -1, florida_devs,
				      ARRAY_SIZE(florida_devs), NULL, 0, NULL);
		break;
	case WM1831:
	case CS47L24:
		ret = mfd_add_devices(arizona->dev, -1, largo_devs,
				      ARRAY_SIZE(largo_devs), NULL, 0, NULL);
		break;
	case WM8997:
		ret = mfd_add_devices(arizona->dev, -1, wm8997_devs,
				      ARRAY_SIZE(wm8997_devs), NULL, 0, NULL);
		break;
	case WM8998:
	case WM1814:
		ret = mfd_add_devices(arizona->dev, -1, vegas_devs,
				      ARRAY_SIZE(vegas_devs), NULL, 0, NULL);
		break;
	case WM8285:
	case WM1840:
		ret = mfd_add_devices(arizona->dev, -1, clearwater_devs,
				      ARRAY_SIZE(clearwater_devs), NULL, 0, NULL);
		break;
	case CS47L35:
		ret = mfd_add_devices(arizona->dev, -1, marley_devs,
				      ARRAY_SIZE(marley_devs), NULL, 0, NULL);
	case CS47L90:
	case CS47L91:
		ret = mfd_add_devices(arizona->dev, -1, moon_devs,
				      ARRAY_SIZE(moon_devs), NULL, 0, NULL);
		break;
	}

	if (ret != 0) {
		dev_err(arizona->dev, "Failed to add subdevices: %d\n", ret);
		goto err_irq;
	}

	return 0;

err_irq:
	arizona_irq_exit(arizona);
err_reset:
	arizona_enable_reset(arizona);
	regulator_disable(arizona->dcvdd);
err_enable:
	regulator_bulk_disable(arizona->num_core_supplies,
			       arizona->core_supplies);
err_notifier:
	regulator_unregister_notifier(arizona->dcvdd, &arizona->dcvdd_notifier);
err_dcvdd:
	regulator_put(arizona->dcvdd);
err_early:
	mfd_remove_devices(dev);
	return ret;
}
EXPORT_SYMBOL_GPL(arizona_dev_init);

int arizona_dev_exit(struct arizona *arizona)
{
	pm_runtime_disable(arizona->dev);

	regulator_disable(arizona->dcvdd);
	regulator_unregister_notifier(arizona->dcvdd, &arizona->dcvdd_notifier);
	regulator_put(arizona->dcvdd);

	mfd_remove_devices(arizona->dev);
	arizona_free_irq(arizona, ARIZONA_IRQ_UNDERCLOCKED, arizona);
	arizona_free_irq(arizona, ARIZONA_IRQ_OVERCLOCKED, arizona);
	arizona_free_irq(arizona, ARIZONA_IRQ_CLKGEN_ERR, arizona);
	arizona_irq_exit(arizona);
	arizona_enable_reset(arizona);

	regulator_bulk_disable(arizona->num_core_supplies,
			       arizona->core_supplies);
	return 0;
}
EXPORT_SYMBOL_GPL(arizona_dev_exit);
