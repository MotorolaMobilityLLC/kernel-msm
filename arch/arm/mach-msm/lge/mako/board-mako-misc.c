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
#include <mach/msm_iomap.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>

#include <mach/board_lge.h>

#include "devices.h"
#include <linux/android_vibrator.h>

#include <linux/i2c.h>

#ifdef CONFIG_BU52031NVX
#include <linux/mfd/pm8xxx/cradle.h>
#endif

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

#if !defined(CONFIG_ANDROID_VIBRATOR)
/* temporal code due to build error..  */
#define GPIO_LIN_MOTOR_EN    0
#define GPIO_LIN_MOTOR_PWM   0
#endif

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
#endif

#if defined(CONFIG_ANDROID_VIBRATOR)
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
#endif

#ifdef CONFIG_ANDROID_VIBRATOR
static struct regulator *vreg_l16 = NULL;
static bool snddev_reg_8921_l16_status = false;

static int vibrator_power_set(int enable)
{
	int rc = -EINVAL;
	if (NULL == vreg_l16) {
		vreg_l16 = regulator_get(NULL, "8921_l16");   //2.6 ~ 3V
		INFO_MSG("enable=%d\n", enable);

		if (IS_ERR(vreg_l16)) {
			pr_err("%s: regulator get of 8921_lvs6 failed (%ld)\n"
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

	INFO_MSG("amp=%d, n_value=%d\n", amp, n_value);

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
		INFO_MSG("GPIO_LIN_MOTOR_PWM is enable with M=%d N=%d D=%d\n",
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
		INFO_MSG("GPIO_LIN_MOTOR_PWM is disalbe \n");
	}

	return 0;
}

static int vibrator_ic_enable_set(int enable)
{
	int gpio_lin_motor_en = 0;
	gpio_lin_motor_en = PM8921_GPIO_PM_TO_SYS(GPIO_LIN_MOTOR_EN);

	INFO_MSG("enable=%d\n", enable);

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
	if (rc) {
		ERR_MSG("GPIO_LIN_MOTOR_EN %d request failed\n", gpio_motor_en);
		return 0;
	}

	/* gpio init */
	rc = gpio_request(gpio_motor_pwm, "lin_motor_pwm");
	if (unlikely(rc < 0))
		ERR_MSG("not able to get gpio\n");

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

#ifdef CONFIG_BU52031NVX
#define GPIO_POUCH_INT     22
#define GPIO_CARKIT_INT    23

static unsigned hall_ic_int_gpio[] = {GPIO_POUCH_INT, GPIO_CARKIT_INT};

static unsigned hall_ic_gpio[] = {	
	GPIO_CFG(22, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	
	GPIO_CFG(23, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static void __init hall_ic_init(void)
{
	int rc = 0;
	int i = 0;

	printk(KERN_INFO "%s, line: %d\n", __func__, __LINE__);

	rc = gpio_request(GPIO_POUCH_INT, "cradle_detect_s");	
	if (rc) {		
		printk(KERN_ERR	"%s: pouch_int  %d request failed\n",
			__func__, GPIO_POUCH_INT);
		return;
	}

	rc = gpio_request(GPIO_CARKIT_INT, "cradle_detect_n");	
	if (rc) {		
		printk(KERN_ERR	"%s: pouch_int  %d request failed\n",
			__func__, GPIO_CARKIT_INT);
		return;
	}

	for (i=0; i<ARRAY_SIZE(hall_ic_gpio); i++) {
		rc = gpio_tlmm_config(hall_ic_gpio[i], GPIO_CFG_ENABLE);
		gpio_direction_input(hall_ic_int_gpio[i]);

		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=fg%d\n",
				__func__, hall_ic_gpio[i], rc);			
			return;			
		}
	}

	printk(KERN_INFO "yoogyeong.lee@%s_END\n", __func__);
}

static struct pm8xxx_cradle_platform_data cradle_data = {
#if defined(CONFIG_BU52031NVX_POUCHDETECT)
	.pouch_detect_pin = GPIO_POUCH_INT,
	.pouch_irq = MSM_GPIO_TO_INT(GPIO_POUCH_INT),
#endif
	.carkit_detect_pin = GPIO_CARKIT_INT,
	.carkit_irq = MSM_GPIO_TO_INT(GPIO_CARKIT_INT),
	.irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
};

static struct platform_device cradle_device = {
	.name = PM8XXX_CRADLE_DEV_NAME,
	.id = -1,
	.dev = {
		.platform_data = &cradle_data,
	},
};

#endif

void __init apq8064_init_misc(void)
{
	INFO_MSG("%s\n", __func__);

#if defined(CONFIG_ANDROID_VIBRATOR)
	vibrator_gpio_init();
#endif

	platform_add_devices(misc_devices, ARRAY_SIZE(misc_devices));

#ifdef CONFIG_BU52031NVX
	hall_ic_init();
	platform_device_register(&cradle_device);
#endif
}
