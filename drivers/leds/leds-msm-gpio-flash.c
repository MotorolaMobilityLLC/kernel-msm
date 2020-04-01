/* Copyright (c) 2013-2014, 2017, 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/printk.h>
#include <linux/list.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/pinctrl/consumer.h>

/* #define CONFIG_GPIO_FLASH_DEBUG */
#undef CDBG
#ifdef CONFIG_GPIO_FLASH_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define LED_GPIO_FLASH_DRIVER_NAME	"qcom,leds-gpio-flash"
#define LED_TRIGGER_DEFAULT		"none"

#define GPIO_OUT_LOW          (0 << 1)
#define GPIO_OUT_HIGH         (1 << 1)

#define DUTY_CYCLE_BASE       100

enum msm_flash_seq_type_t {
	FLASH_EN,
	FLASH_NOW,
};

enum msm_flash_gpio_type_t {
	NORMAL_GPIO,
	CLK_GPIO,
};

struct msm_flash_ctrl_seq {
	enum msm_flash_seq_type_t seq_type;
	uint8_t flash_on_val;
	uint8_t torch_on_val;
};

struct led_gpio_flash_data {
	int flash_en;
	int flash_now;
	struct clk *flash_en_clk;
	struct clk *flash_now_clk;
	int brightness;
	atomic_t clk_enabled[2];
	uint32_t clk_freq[2];
	uint32_t duty_cycle[2];
	enum msm_flash_gpio_type_t gpio_type[2];
	struct led_classdev cdev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_default;
	struct msm_flash_ctrl_seq ctrl_seq[2];
};

static const struct of_device_id led_gpio_flash_of_match[] = {
	{.compatible = LED_GPIO_FLASH_DRIVER_NAME,},
	{},
};

static void led_gpio_brightness_set(struct led_classdev *led_cdev,
				    enum led_brightness value)
{
	int rc = 0;
	int brightness = value;
	int flash_en = 0;
	int flash_now = 0;
	struct led_gpio_flash_data *flash_led =
	    container_of(led_cdev, struct led_gpio_flash_data, cdev);

	if (brightness > LED_HALF) {
		flash_en =
			flash_led->ctrl_seq[FLASH_EN].flash_on_val;
		flash_now =
			flash_led->ctrl_seq[FLASH_NOW].flash_on_val;
	} else if (brightness > LED_OFF) {
		flash_en =
			flash_led->ctrl_seq[FLASH_EN].torch_on_val;
		flash_now =
			flash_led->ctrl_seq[FLASH_NOW].torch_on_val;
	} else {
		flash_en = 0;
		flash_now = 0;
	}

	CDBG("%s:flash_en=%d, flash_now=%d\n", __func__, flash_en, flash_now);

	if (flash_led->gpio_type[FLASH_EN] == NORMAL_GPIO) {
		rc = gpio_direction_output(flash_led->flash_en, flash_en);
	} else {
		if (flash_en == GPIO_OUT_HIGH &&
			!atomic_read(&flash_led->clk_enabled[FLASH_EN])) {
			rc = clk_prepare_enable(flash_led->flash_en_clk);
			atomic_set(&flash_led->clk_enabled[FLASH_EN], 1);
		} else if (flash_en == GPIO_OUT_LOW &&
			atomic_read(&flash_led->clk_enabled[FLASH_EN])) {
			clk_disable_unprepare(flash_led->flash_en_clk);
			atomic_set(&flash_led->clk_enabled[FLASH_EN], 0);
		}
	}

	if (rc) {
		pr_err("%s: Failed to set flash en.\n", __func__);
		return;
	}

	if (flash_led->gpio_type[FLASH_NOW] == NORMAL_GPIO) {
		rc = gpio_direction_output(flash_led->flash_now, flash_now);
	} else {
		if (flash_now == GPIO_OUT_HIGH &&
			!atomic_read(&flash_led->clk_enabled[FLASH_NOW])) {
			rc = clk_prepare_enable(flash_led->flash_now_clk);
			atomic_set(&flash_led->clk_enabled[FLASH_NOW], 1);
		} else if (flash_now == GPIO_OUT_LOW &&
			atomic_read(&flash_led->clk_enabled[FLASH_NOW]))  {
			clk_disable_unprepare(flash_led->flash_now_clk);
			atomic_set(&flash_led->clk_enabled[FLASH_NOW], 0);
		}
	}

	if (rc) {
		pr_err("%s: Failed to set flash now.\n", __func__);
		return;
	}

	flash_led->brightness = brightness;
}

static enum led_brightness led_gpio_brightness_get(struct led_classdev
						   *led_cdev)
{
	struct led_gpio_flash_data *flash_led =
	    container_of(led_cdev, struct led_gpio_flash_data, cdev);
	return flash_led->brightness;
}

static int led_gpio_get_dt_data(struct device *dev,
			struct led_gpio_flash_data *flash_led)
{
	int rc = 0;
	int i = 0;
	const char *temp_str = NULL;
	const char *seq_name = NULL;
	uint32_t array_flash_seq[2];
	uint32_t array_torch_seq[2];
	struct device_node *node = dev->of_node;

	flash_led->cdev.default_trigger = LED_TRIGGER_DEFAULT;
	rc = of_property_read_string(node, "linux,default-trigger",
			&temp_str);
	if (!rc)
		flash_led->cdev.default_trigger = temp_str;

	rc = of_property_read_string(node, "linux,name", &flash_led->cdev.name);
	if (rc) {
		pr_err("Failed to read linux name. rc = %d\n", rc);
		return rc;
	}

	/* Configure the gpio type as NORMAL_GPIO by default,
	 * the gpio type should be CLK_GPIO if the frequency
	 * is not 0.
	 */
	flash_led->gpio_type[FLASH_EN] = NORMAL_GPIO;
	flash_led->gpio_type[FLASH_NOW] = NORMAL_GPIO;
	rc = of_property_read_u32_array(node, "qcom,clk-freq",
			flash_led->clk_freq, 2);
	if (!rc) {
		if (flash_led->clk_freq[FLASH_EN])
			flash_led->gpio_type[FLASH_EN] = CLK_GPIO;

		if (flash_led->clk_freq[FLASH_NOW])
			flash_led->gpio_type[FLASH_NOW] = CLK_GPIO;
	}

	if (flash_led->gpio_type[FLASH_EN] == NORMAL_GPIO) {
		flash_led->flash_en =
			of_get_named_gpio(node, "qcom,flash-en", 0);
		if (flash_led->flash_en < 0) {
			pr_err("Read flash-en property failed. rc = %d\n",
				flash_led->flash_en);
			return -EINVAL;
		}
		rc = gpio_request(flash_led->flash_en, "FLASH_EN");
		if (rc) {
			pr_err("%s: Failed to request gpio %d,rc = %d\n",
				__func__, flash_led->flash_en, rc);
			return rc;
		}
	} else {
		flash_led->flash_en_clk =
			devm_clk_get(dev, "flash_en_clk");
		if (IS_ERR(flash_led->flash_en_clk)) {
			pr_err("Failed to get flash-en clk.\n");
			return -EINVAL;
		}

		flash_led->clk_freq[FLASH_EN] =
			clk_round_rate(flash_led->flash_en_clk,
			flash_led->clk_freq[FLASH_EN]);
		rc = clk_set_rate(flash_led->flash_en_clk,
				flash_led->clk_freq[FLASH_EN]);
		if (rc) {
			pr_err("%s: Failed to set rate for flash en.\n",
				__func__);
			return rc;
		}
	}

	if (flash_led->gpio_type[FLASH_NOW] == NORMAL_GPIO) {
		flash_led->flash_now =
			of_get_named_gpio(node, "qcom,flash-now", 0);
		if (flash_led->flash_now < 0) {
			pr_err("Read flash-now property failed. rc = %d\n",
				flash_led->flash_now);
			return -EINVAL;
		}
		rc = gpio_request(flash_led->flash_now, "FLASH_NOW");
		if (rc) {
			pr_err("%s: Failed to request gpio %d,rc = %d\n",
				__func__, flash_led->flash_now, rc);
			return rc;
		}
	} else {
		flash_led->flash_now_clk =
			devm_clk_get(dev, "flash_now_clk");
		if (IS_ERR(flash_led->flash_now_clk)) {
			pr_err("Failed to get flash-now clk.\n");
			return -EINVAL;
		}

		flash_led->clk_freq[FLASH_NOW] =
			clk_round_rate(flash_led->flash_now_clk,
			flash_led->clk_freq[FLASH_NOW]);
		rc = clk_set_rate(flash_led->flash_now_clk,
				flash_led->clk_freq[FLASH_NOW]);
		if (rc) {
			pr_err("%s: Failed to set rate for flash now.\n",
				__func__);
			return rc;
		}
	}

	/* Configure the duty cycle if need. */
	if (flash_led->gpio_type[FLASH_EN] == CLK_GPIO ||
		flash_led->gpio_type[FLASH_NOW] == CLK_GPIO) {
		rc = of_property_read_u32_array(node, "qcom,duty-cycle",
				flash_led->duty_cycle, 2);
		if (!rc &&
			flash_led->duty_cycle[FLASH_EN] >= DUTY_CYCLE_BASE &&
			flash_led->duty_cycle[FLASH_NOW] >= DUTY_CYCLE_BASE) {
			pr_err("%s: the duty cycle value is invalid.\n",
				__func__);
			return -EINVAL;
		}
	}

	/* Based on clk protocol, only RCG clks support duty cycle
	 * configuration, so if the used clk doesn't support set duty
	 * cycle, we use the clk's parent rcg clk to configure the
	 * duty cycle.
	 */
	if (flash_led->duty_cycle[FLASH_EN]) {
		struct clk *flash_en_duty_cycle_clk = NULL;

		flash_en_duty_cycle_clk = devm_clk_get(dev,
			"flash_en_duty_cycle_clk");
		if (!IS_ERR(flash_en_duty_cycle_clk)) {
			rc = clk_set_duty_cycle(flash_en_duty_cycle_clk,
					flash_led->duty_cycle[FLASH_EN],
					DUTY_CYCLE_BASE);
			clk_put(flash_en_duty_cycle_clk);
		} else {
			rc = clk_set_duty_cycle(flash_led->flash_en_clk,
					flash_led->duty_cycle[FLASH_EN],
					DUTY_CYCLE_BASE);
		}

		if (rc) {
			pr_err("Failed to set duty cycle for flash en.\n");
			return rc;
		}
	}

	if (flash_led->duty_cycle[FLASH_NOW]) {
		struct clk *flash_now_duty_cycle_clk = NULL;

		flash_now_duty_cycle_clk = devm_clk_get(dev,
			"flash_now_duty_cycle_clk");
		if (!IS_ERR(flash_now_duty_cycle_clk)) {
			rc = clk_set_duty_cycle(flash_now_duty_cycle_clk,
					flash_led->duty_cycle[FLASH_NOW],
					DUTY_CYCLE_BASE);
			clk_put(flash_now_duty_cycle_clk);
		} else {
			rc = clk_set_duty_cycle(flash_led->flash_now_clk,
					flash_led->duty_cycle[FLASH_NOW],
					DUTY_CYCLE_BASE);
		}

		if (rc) {
			pr_err("Failed to set duty cycle for flash now.\n");
			return rc;
		}
	}

	rc = of_property_read_u32_array(node, "qcom,flash-seq-val",
			array_flash_seq, 2);
	if (rc < 0) {
		pr_err("Failed to get flash op seq, rc = %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(node, "qcom,torch-seq-val",
			array_torch_seq, 2);
	if (rc < 0) {
		pr_err("Failed to get torch op seq, rc = %d\n", rc);
		return rc;
	}

	pr_debug("%s: seq: flash: %d, %d torch:%d, %d\n", __func__,
		array_flash_seq[0], array_flash_seq[1],
		array_torch_seq[0], array_torch_seq[1]);

	for (i = 0; i < 2; i++) {
		rc = of_property_read_string_index(node,
				"qcom,op-seq", i,
				&seq_name);
		CDBG("%s seq_name[%d] = %s\n", __func__, i,
				seq_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		if (!strcmp(seq_name, "flash_en")) {
			flash_led->ctrl_seq[FLASH_EN].seq_type =
				FLASH_EN;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, flash_led->ctrl_seq[FLASH_EN].seq_type);
			if (array_flash_seq[i] == 0)
				flash_led->ctrl_seq[FLASH_EN].flash_on_val =
					GPIO_OUT_LOW;
			else
				flash_led->ctrl_seq[FLASH_EN].flash_on_val =
					GPIO_OUT_HIGH;

			if (array_torch_seq[i] == 0)
				flash_led->ctrl_seq[FLASH_EN].torch_on_val =
					GPIO_OUT_LOW;
			else
				flash_led->ctrl_seq[FLASH_EN].torch_on_val =
					GPIO_OUT_HIGH;
		} else if (!strcmp(seq_name, "flash_now")) {
			flash_led->ctrl_seq[FLASH_NOW].seq_type =
				FLASH_NOW;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, flash_led->ctrl_seq[FLASH_NOW].seq_type);
			if (array_flash_seq[i] == 0)
				flash_led->ctrl_seq[FLASH_NOW].flash_on_val =
					GPIO_OUT_LOW;
			else
				flash_led->ctrl_seq[FLASH_NOW].flash_on_val =
					GPIO_OUT_HIGH;

			if (array_torch_seq[i] == 0)
				flash_led->ctrl_seq[FLASH_NOW].torch_on_val =
					GPIO_OUT_LOW;
			else
				flash_led->ctrl_seq[FLASH_NOW].torch_on_val =
					GPIO_OUT_HIGH;
		}
	}

	return rc;
}

static int led_gpio_flash_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct led_gpio_flash_data *flash_led = NULL;

	flash_led = devm_kzalloc(&pdev->dev, sizeof(struct led_gpio_flash_data),
				 GFP_KERNEL);
	if (flash_led == NULL)
		return -ENOMEM;

	flash_led->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(flash_led->pinctrl)) {
		pr_err("%s:failed to get pinctrl\n", __func__);
		rc = PTR_ERR(flash_led->pinctrl);
		goto error;
	}

	flash_led->gpio_state_default = pinctrl_lookup_state(flash_led->pinctrl,
		"flash_default");
	if (IS_ERR(flash_led->gpio_state_default)) {
		pr_err("%s:can not get active pinstate\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	rc = pinctrl_select_state(flash_led->pinctrl,
		flash_led->gpio_state_default);
	if (rc) {
		pr_err("%s:set state failed!\n", __func__);
		goto error;
	}

	rc = led_gpio_get_dt_data(&pdev->dev, flash_led);
	if (rc) {
		pr_err("%s: get device tree data failed.\n",
				__func__);
		goto error;
	}

	/* Add these atomic variables to make sure clk is disabled
	 * just after the clk has been enabled.
	 */
	atomic_set(&flash_led->clk_enabled[FLASH_EN], 0);
	atomic_set(&flash_led->clk_enabled[FLASH_NOW], 0);

	platform_set_drvdata(pdev, flash_led);
	flash_led->cdev.max_brightness = LED_FULL;
	flash_led->cdev.brightness_set = led_gpio_brightness_set;
	flash_led->cdev.brightness_get = led_gpio_brightness_get;

	rc = led_classdev_register(&pdev->dev, &flash_led->cdev);
	if (rc) {
		pr_err("%s: Failed to register led dev. rc = %d\n",
			__func__, rc);
		goto error;
	}
	pr_err("%s:probe successfully!\n", __func__);
	return 0;

error:
	if (flash_led->gpio_type[FLASH_EN] == CLK_GPIO &&
		IS_ERR(flash_led->flash_en_clk))
		devm_clk_put(&pdev->dev, flash_led->flash_en_clk);
	else if (flash_led->gpio_type[FLASH_EN] == NORMAL_GPIO &&
		flash_led->flash_en)
		gpio_free(flash_led->flash_en);

	if (flash_led->gpio_type[FLASH_NOW] == CLK_GPIO &&
		IS_ERR(flash_led->flash_now_clk))
		devm_clk_put(&pdev->dev, flash_led->flash_now_clk);
	else if (flash_led->gpio_type[FLASH_NOW] == NORMAL_GPIO &&
		flash_led->flash_now)
		gpio_free(flash_led->flash_now);

	if (IS_ERR(flash_led->pinctrl))
		devm_pinctrl_put(flash_led->pinctrl);

	devm_kfree(&pdev->dev, flash_led);
	return rc;
}

static int led_gpio_flash_remove(struct platform_device *pdev)
{
	struct led_gpio_flash_data *flash_led =
	    (struct led_gpio_flash_data *)platform_get_drvdata(pdev);
	if (IS_ERR(flash_led->pinctrl))
		devm_pinctrl_put(flash_led->pinctrl);
	led_classdev_unregister(&flash_led->cdev);
	devm_kfree(&pdev->dev, flash_led);
	return 0;
}

static struct platform_driver led_gpio_flash_driver = {
	.probe = led_gpio_flash_probe,
	.remove = led_gpio_flash_remove,
	.driver = {
		   .name = LED_GPIO_FLASH_DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = led_gpio_flash_of_match,
	}
};

static int __init led_gpio_flash_init(void)
{
	return platform_driver_register(&led_gpio_flash_driver);
}

static void __exit led_gpio_flash_exit(void)
{
	return platform_driver_unregister(&led_gpio_flash_driver);
}

late_initcall(led_gpio_flash_init);
module_exit(led_gpio_flash_exit);

MODULE_DESCRIPTION("QTI GPIO LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-msm-gpio-flash");
