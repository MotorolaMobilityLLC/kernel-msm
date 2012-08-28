/*
  * Copyright (C) 2011,2012 LGE, Inc.
  *
  * Author: Sungwoo Cho <sungwoo.cho@lge.com>
  *
  * This software is licensed under the terms of the GNU General
  * License version 2, as published by the Free Software Foundation,
  * may be copied, distributed, and modified under those terms.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
  * GNU General Public License for more details.
  */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/android_vibrator.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#ifdef CONFIG_SLIMPORT_ANX7808
#include <linux/platform_data/slimport_device.h>
#endif
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/board_lge.h>

#include "devices.h"
#include "board-mako.h"
/* gpio and clock control for vibrator */

#define REG_WRITEL(value, reg)	writel(value, (MSM_CLK_CTL_BASE+reg))
#define REG_READL(reg)          readl((MSM_CLK_CTL_BASE+reg))

#define GPn_MD_REG(n)                           (0x2D00+32*(n))
#define GPn_NS_REG(n)                           (0x2D24+32*(n))

/* When use SM100 with GP_CLK
  170Hz motor : 22.4KHz - M=1, N=214 ,
  230Hz motor : 29.4KHZ - M=1, N=163 ,
  */

/* Vibrator GPIOs */
#ifdef CONFIG_ANDROID_VIBRATOR
#define GPIO_LIN_MOTOR_EN                33
#define GPIO_LIN_MOTOR_PWR               47
#define GPIO_LIN_MOTOR_PWM               3

#define GP_CLK_ID                        0 /* gp clk 0 */
#define GP_CLK_M_DEFAULT                 1
#define GP_CLK_N_DEFAULT                 166
#define GP_CLK_D_MAX                     GP_CLK_N_DEFAULT
#define GP_CLK_D_HALF                    (GP_CLK_N_DEFAULT >> 1)

#define MOTOR_AMP                        120

static struct gpiomux_setting vibrator_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

/* gpio 3 */
static struct gpiomux_setting vibrator_active_cfg_gpio3 = {
	.func = GPIOMUX_FUNC_2, /*gp_mn:2 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config gpio2_vibrator_configs[] = {
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE]    = &vibrator_active_cfg_gpio3,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};

static int gpio_vibrator_en = 33;
static int gpio_vibrator_pwm = 3;
static int gp_clk_id = 0;

static int vibrator_gpio_init(void)
{
	gpio_vibrator_en = GPIO_LIN_MOTOR_EN;
	gpio_vibrator_pwm = GPIO_LIN_MOTOR_PWM;
	return 0;
}

static struct regulator *vreg_l16 = NULL;
static bool snddev_reg_8921_l16_status = false;

static int vibrator_power_set(int enable)
{
	int rc = -EINVAL;
	if (NULL == vreg_l16) {
		vreg_l16 = regulator_get(NULL, "vibrator");   //2.6 ~ 3V
		pr_debug("%s: enable=%d\n", __func__, enable);

		if (IS_ERR(vreg_l16)) {
			pr_err("%s: regulator get of vibrator failed (%ld)\n"
					, __func__, PTR_ERR(vreg_l16));
			printk("woosock ERROR\n");
			rc = PTR_ERR(vreg_l16);
			return rc;
		}
	}
	rc = regulator_set_voltage(vreg_l16, 2800000, 2800000);
	
	if (enable == snddev_reg_8921_l16_status)
		return 0;

	if (enable) {
		rc = regulator_set_voltage(vreg_l16, 2800000, 2800000);
		if (rc < 0)
			pr_err("LGE:  VIB %s: regulator_set_voltage(l1) failed (%d)\n",
			__func__, rc);
			
		rc = regulator_enable(vreg_l16);

		if (rc < 0)
			pr_err("LGE: VIB %s: regulator_enable(l1) failed (%d)\n", __func__, rc);
		snddev_reg_8921_l16_status = true;

	} else {
		rc = regulator_disable(vreg_l16);
		if (rc < 0)
			pr_err("%s: regulator_disable(l1) failed (%d)\n", __func__, rc);
		snddev_reg_8921_l16_status = false;
	}	

	return 0;
}

static int vibrator_pwm_set(int enable, int amp, int n_value)
{
	/* TODO: set clk for amp */
	uint M_VAL = GP_CLK_M_DEFAULT;
	uint D_VAL = GP_CLK_D_MAX;
	uint D_INV = 0;                 /* QCT support invert bit for msm8960 */
	uint clk_id = gp_clk_id;

	pr_debug("%s: amp=%d, n_value=%d\n", __func__, amp, n_value);

	if (enable) {
		D_VAL = ((GP_CLK_D_MAX * amp) >> 7);
		if (D_VAL > GP_CLK_D_HALF) {
			if (D_VAL == GP_CLK_D_MAX) {      /* Max duty is 99% */
				D_VAL = 2;
			} else {
				D_VAL = GP_CLK_D_MAX - D_VAL;
			}
			D_INV = 1;
		}

		REG_WRITEL(
			(((M_VAL & 0xffU) << 16U) + /* M_VAL[23:16] */
			((~(D_VAL << 1)) & 0xffU)),  /* D_VAL[7:0] */
			GPn_MD_REG(clk_id));

		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(1U << 11U) +  /* CLK_ROOT_ENA[11]  : Enable(1) */
			((D_INV & 0x01U) << 10U) +  /* CLK_INV[10]       : Disable(0) */
			(1U << 9U) +   /* CLK_BRANCH_ENA[9] : Enable(1) */
			(1U << 8U) +   /* NMCNTR_EN[8]      : Enable(1) */
			(0U << 7U) +   /* MNCNTR_RST[7]     : Not Active(0) */
			(2U << 5U) +   /* MNCNTR_MODE[6:5]  : Dual-edge mode(2) */
			(3U << 3U) +   /* PRE_DIV_SEL[4:3]  : Div-4 (3) */
			(5U << 0U)),   /* SRC_SEL[2:0]      : CXO (5)  */
			GPn_NS_REG(clk_id));
		pr_debug("%s: PWM is enable with M=%d N=%d D=%d\n",
				__func__,
				M_VAL, n_value, D_VAL);
	} else {
		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(0U << 11U) +  /* CLK_ROOT_ENA[11]  : Disable(0) */
			(0U << 10U) +  /* CLK_INV[10]	    : Disable(0) */
			(0U << 9U) +	 /* CLK_BRANCH_ENA[9] : Disable(0) */
			(0U << 8U) +   /* NMCNTR_EN[8]      : Disable(0) */
			(0U << 7U) +   /* MNCNTR_RST[7]     : Not Active(0) */
			(2U << 5U) +   /* MNCNTR_MODE[6:5]  : Dual-edge mode(2) */
			(3U << 3U) +   /* PRE_DIV_SEL[4:3]  : Div-4 (3) */
			(5U << 0U)),   /* SRC_SEL[2:0]      : CXO (5)  */
			GPn_NS_REG(clk_id));
		pr_debug("%s: PWM is disable\n", __func__);
	}

	return 0;
}

static int vibrator_ic_enable_set(int enable)
{
	int gpio_lin_motor_en = 0;
	gpio_lin_motor_en = PM8921_GPIO_PM_TO_SYS(GPIO_LIN_MOTOR_EN);

	pr_debug("%s: enable=%d\n", __func__, enable);

	if (enable)
		gpio_direction_output(gpio_lin_motor_en, 1);
	else
		gpio_direction_output(gpio_lin_motor_en, 0);

	return 0;
}

static int vibrator_init(void)
{
	int rc;
	int gpio_motor_en = 0;
	int gpio_motor_pwm = 0;

	gpio_motor_en = gpio_vibrator_en;
	gpio_motor_pwm = gpio_vibrator_pwm;

	/* GPIO function setting */
	msm_gpiomux_install(gpio2_vibrator_configs,
			ARRAY_SIZE(gpio2_vibrator_configs));

	/* GPIO setting for Motor EN in pmic8921 */
	gpio_motor_en = PM8921_GPIO_PM_TO_SYS(GPIO_LIN_MOTOR_EN);
	rc = gpio_request(gpio_motor_en, "lin_motor_en");
	if (rc < 0) {
		pr_err("%s: MOTOR_EN %d request failed\n",
				__func__, gpio_motor_en);
		return rc;
	}

	/* gpio init */
	rc = gpio_request(gpio_motor_pwm, "lin_motor_pwm");
	if (rc < 0) {
		gpio_free(gpio_motor_en);
		pr_err("%s: MOTOR_PWM %d request failed\n",
				__func__, gpio_motor_pwm);
		return rc;
	}

	vibrator_ic_enable_set(0);
	vibrator_pwm_set(0, 0, GP_CLK_N_DEFAULT);
	vibrator_power_set(0);

	return 0;
}

static struct android_vibrator_platform_data vibrator_data = {
	.enable_status = 0,
	.amp = MOTOR_AMP,
	.vibe_n_value = GP_CLK_N_DEFAULT,
	.power_set = vibrator_power_set,
	.pwm_set = vibrator_pwm_set,
	.ic_enable_set = vibrator_ic_enable_set,
	.vibrator_init = vibrator_init,
};

static struct platform_device android_vibrator_device = {
	.name   = "android-vibrator",
	.id = -1,
	.dev = {
		.platform_data = &vibrator_data,
	},
};
#endif /* CONFIG_ANDROID_VIBRATOR */

static struct platform_device *misc_devices[] __initdata = {
#ifdef CONFIG_ANDROID_VIBRATOR
	&android_vibrator_device,
#endif
};

#ifdef CONFIG_SLIMPORT_ANX7808
#define GPIO_SLIMPORT_CBL_DET    PM8921_GPIO_PM_TO_SYS(14)
#define GPIO_SLIMPORT_PWR_DWN    PM8921_GPIO_PM_TO_SYS(15)
#define ANX_AVDD33_EN            PM8921_GPIO_PM_TO_SYS(16)
#define GPIO_SLIMPORT_RESET_N    31
#define GPIO_SLIMPORT_INT_N      43

static int anx7808_dvdd_onoff(bool on)
{
	static bool power_state = 0;
	static struct regulator *anx7808_dvdd_reg = NULL;
	int rc = 0;

	if (power_state == on) {
		pr_info("anx7808 dvdd is already %s \n", power_state ? "on" : "off");
		goto out;
	}

	if (!anx7808_dvdd_reg) {
		anx7808_dvdd_reg= regulator_get(NULL, "slimport_dvdd");
		if (IS_ERR(anx7808_dvdd_reg)) {
			rc = PTR_ERR(anx7808_dvdd_reg);
			pr_err("%s: regulator_get anx7808_dvdd_reg failed. rc=%d\n",
					__func__, rc);
			anx7808_dvdd_reg = NULL;
			goto out;
		}
		rc = regulator_set_voltage(anx7808_dvdd_reg, 1100000, 1100000);
		if (rc ) {
			pr_err("%s: regulator_set_voltage anx7808_dvdd_reg failed\
				rc=%d\n", __func__, rc);
			goto out;
		}
	}

	if (on) {
		rc = regulator_set_optimum_mode(anx7808_dvdd_reg, 100000);
		if (rc < 0) {
			pr_err("%s : set optimum mode 100000, anx7808_dvdd_reg failed \
					(%d)\n", __func__, rc);
			goto out;
		}
		rc = regulator_enable(anx7808_dvdd_reg);
		if (rc) {
			pr_err("%s : anx7808_dvdd_reg enable failed (%d)\n",
					__func__, rc);
			goto out;
		}
	}
	else {
		rc = regulator_disable(anx7808_dvdd_reg);
		if (rc) {
			pr_err("%s : anx7808_dvdd_reg disable failed (%d)\n",
				__func__, rc);
			goto out;
		}
		rc = regulator_set_optimum_mode(anx7808_dvdd_reg, 100);
		if (rc < 0) {
			pr_err("%s : set optimum mode 100, anx7808_dvdd_reg failed \
				(%d)\n", __func__, rc);
			goto out;
		}
	}
	power_state = on;

out:
	return rc;

}

static int anx7808_avdd_onoff(bool on)
{
	static bool init_done = 0;
	int rc = 0;

	if (!init_done) {
		rc = gpio_request_one(ANX_AVDD33_EN,
					GPIOF_OUT_INIT_HIGH, "anx_avdd33_en");
		if (rc) {
			pr_err("request anx_avdd33_en failed, rc=%d\n", rc);
			return rc;
		}
		init_done = 1;
	}

	gpio_set_value(ANX_AVDD33_EN, on);
	return 0;
}

static struct anx7808_platform_data anx7808_pdata = {
	.gpio_p_dwn = GPIO_SLIMPORT_PWR_DWN,
	.gpio_reset = GPIO_SLIMPORT_RESET_N,
	.gpio_int = GPIO_SLIMPORT_INT_N,
	.gpio_cbl_det = GPIO_SLIMPORT_CBL_DET,
	.dvdd_power = anx7808_dvdd_onoff,
	.avdd_power = anx7808_avdd_onoff,
};

struct i2c_board_info i2c_anx7808_info[] = {
	{
		I2C_BOARD_INFO("anx7808", 0x72 >> 1),
		.platform_data = &anx7808_pdata,
	},
};

static struct i2c_registry i2c_anx7808_devices __initdata = {
	I2C_FFA,
	APQ_8064_GSBI1_QUP_I2C_BUS_ID,
	i2c_anx7808_info,
	ARRAY_SIZE(i2c_anx7808_info),
};

static void __init lge_add_i2c_anx7808_device(void)
{
	i2c_register_board_info(i2c_anx7808_devices.bus,
		i2c_anx7808_devices.info,
		i2c_anx7808_devices.len);
}
#endif

void __init apq8064_init_misc(void)
{
#if defined(CONFIG_ANDROID_VIBRATOR)
	vibrator_gpio_init();
#endif

	platform_add_devices(misc_devices, ARRAY_SIZE(misc_devices));

#ifdef CONFIG_SLIMPORT_ANX7808
	lge_add_i2c_anx7808_device();
#endif
}
