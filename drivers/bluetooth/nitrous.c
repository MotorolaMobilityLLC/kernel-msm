/*
 * Bluetooth low power control via GPIO
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * The current implementation is very specific to Qualcomm serial driver
 * with BCM chipset.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_data/msm_serial_hs.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/rfkill.h>
#include <linux/wakelock.h>

/* Timeout on UART Tx traffic before releasing wakelock. */
static const int UART_TIMEOUT_SEC = 1;

struct nitrous_bt_lpm {
	int gpio_dev_wake;           /* host -> dev wake gpio */
	int gpio_host_wake;          /* dev -> host wake gpio */
	int gpio_power;              /* power gpio */
	int gpio_reset;              /* optional reset gpio */
	int irq_host_wake;           /* IRQ associated with host wake gpio */
	int dev_wake_pol;            /* 0: active low; 1: active high */
	int host_wake_pol;           /* 0: active low; 1: active high */

	struct wake_lock dev_lock;   /* Wakelock held during Tx */
	struct wake_lock host_lock;  /* Wakelock held during Rx */
	struct hrtimer tx_lpm_timer; /* timer for going into LPM during Tx */
	bool is_suspended;           /* driver is in suspend state */
	bool pending_irq;            /* pending host wake IRQ during suspend */

	struct uart_port *uart_port;
	struct platform_device *pdev;
	struct rfkill *rfkill;
	bool rfkill_blocked;         /* blocked: off; not blocked: on */
};

static struct nitrous_bt_lpm *bt_lpm;  /* Reference for internal data */

/* Helper to fetch gpio information from the device tree and then register it */
static int read_and_request_gpio(struct platform_device *pdev,
				 const char *dt_name, int *gpio,
				 unsigned long gpio_flags, const char *label)
{
	int rc;

	*gpio = of_get_named_gpio(pdev->dev.of_node, dt_name, 0);
	if (unlikely(*gpio < 0)) {
		pr_err("%s: %s not in device tree",  __func__, dt_name);
		return -EINVAL;
	}

	if (!gpio_is_valid(*gpio)) {
		pr_err("%s: %s is invalid", __func__, dt_name);
		return -EINVAL;
	}

	rc = gpio_request_one(*gpio, gpio_flags, label);
	if (unlikely(rc < 0)) {
		pr_err("%s: failed to request gpio: %d (%s), error: %d",
			__func__, *gpio, label, rc);
		return -EINVAL;
	}

	return 0;
}

/*
 * Power up or down UART driver and hold Tx/Rx wakelock.
 *
 * Note that the use of pm_runtime_get here is not ideal as the call is not
 * blocking.  By the time the UART driver is powered up, the serial core might
 * have attempted to do a Tx at the UART driver already.  There is currently a
 * workaround in the MSM serial driver to catch this race.  Over time the clock
 * on and Tx sequence should be made synchronous.
 */
static bool nitrous_uart_power(struct uart_port *uart_port,
			       struct wake_lock *lock, bool on)
{
	if (wake_lock_active(lock) == on)
		return false;

	if (on) {
		wake_lock(lock);
		pm_runtime_get(uart_port->dev);
	} else {
		pm_runtime_put(uart_port->dev);
		wake_unlock(lock);
	}

	return true;
}

/*
 * Wake up or sleep UART and BT device for Tx.
 */
static inline void nitrous_wake_device_locked(struct nitrous_bt_lpm *lpm,
					      bool wake)
{
	if (nitrous_uart_power(lpm->uart_port, &lpm->dev_lock, wake)) {
		/*
		 * If there is a UART power on/off, assert/deassert dev wake
		 * gpio accordingly.
		 */
		int assert_level = (wake == lpm->dev_wake_pol);
		gpio_set_value(lpm->gpio_dev_wake, assert_level);
	}
}

/*
 * Wake up or sleep UART for Rx.
 */
static inline void nitrous_wake_uart(struct nitrous_bt_lpm *lpm, bool wake)
{
	nitrous_uart_power(lpm->uart_port, &lpm->host_lock, wake);
}

/*
 * Called when the tx_lpm_timer expires and the last Tx transaction should have
 * been started about UART_TIMEOUT_SEC second(s) ago.  At this time, the Tx
 * should have been completed.
 */
static enum hrtimer_restart nitrous_tx_lpm_handler(struct hrtimer *timer)
{
	unsigned long flags;

	if (!bt_lpm) {
		pr_err("%s: missing bt_lpm\n", __func__);
		return HRTIMER_NORESTART;
	}

	pr_debug("%s\n", __func__);

	/* Release UART and BT resources */
	spin_lock_irqsave(&bt_lpm->uart_port->lock, flags);
	nitrous_wake_device_locked(bt_lpm, false);
	spin_unlock_irqrestore(&bt_lpm->uart_port->lock, flags);

	return HRTIMER_NORESTART;
}

/*
 * Called before UART driver starts transmitting data out. UART and BT resources
 * are requested to allow a transmission.
 *
 * Note that the calling context from the serial core should have the
 * uart_port locked.
 */
void nitrous_prepare_uart_tx_locked(struct uart_port *port)
{
	if (!bt_lpm) {
		pr_err("%s: missing bt_lpm\n", __func__);
		return;
	}

	if (bt_lpm->rfkill_blocked) {
		pr_err("%s: unexpected Tx when rfkill is blocked\n", __func__);
		return;
	}

	hrtimer_cancel(&bt_lpm->tx_lpm_timer);
	nitrous_wake_device_locked(bt_lpm, true);
	hrtimer_start(&bt_lpm->tx_lpm_timer, ktime_set(UART_TIMEOUT_SEC, 0),
		HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(nitrous_prepare_uart_tx_locked);

/*
 * ISR to handle host wake line from the BT chip.
 *
 * If an interrupt is received during system suspend, the handling of the
 * interrupt will be delayed until the driver is resumed.  This allows the use
 * of pm runtime framework to wake the serial driver.
 */
static irqreturn_t nitrous_host_wake_isr(int irq, void *dev)
{
	int host_wake, rc;
	struct platform_device *pdev = container_of(dev,
			struct platform_device, dev);
	struct nitrous_bt_lpm *lpm = platform_get_drvdata(pdev);

	if (!lpm) {
		pr_err("%s: missing lpm\n", __func__);
		return IRQ_HANDLED;
	}

	if (lpm->rfkill_blocked) {
		pr_err("%s: unexpected host wake IRQ\n", __func__);
		return IRQ_HANDLED;
	}

	host_wake = gpio_get_value(lpm->gpio_host_wake);
	pr_debug("%s: host wake gpio: %d\n", __func__, host_wake);

	/* Invert the interrupt type to catch the next edge */
	rc = irq_set_irq_type(irq,
		host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
	if (unlikely(rc))
		pr_err("%s: error setting irq type %d\n", __func__, rc);

	if (lpm->is_suspended) {
		/* Mark pending irq flag to delay processing. */
		lpm->pending_irq = true;
	} else {
		/* Wake up UART right the way if not suspended. */
		bool uart_enable = (host_wake == lpm->host_wake_pol);
		nitrous_wake_uart(lpm, uart_enable);
	}

	return IRQ_HANDLED;
}

static int nitrous_lpm_init(struct nitrous_bt_lpm *lpm)
{
	int rc;

	hrtimer_init(&lpm->tx_lpm_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	lpm->tx_lpm_timer.function = nitrous_tx_lpm_handler;

	lpm->irq_host_wake = gpio_to_irq(lpm->gpio_host_wake);

	rc = request_irq(lpm->irq_host_wake, nitrous_host_wake_isr,
		lpm->host_wake_pol ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH,
		"bt_host_wake", &lpm->pdev->dev);
	if (rc < 0) {
		pr_err("%s: unable to request IRQ for bt_host_wake GPIO\n",
			__func__);
		goto err_request_irq;
	}

	wake_lock_init(&lpm->dev_lock, WAKE_LOCK_SUSPEND, "bt_dev_tx_wake");
	wake_lock_init(&lpm->host_lock, WAKE_LOCK_SUSPEND, "bt_host_rx_wake");

	/* Configure wake peer callback to be called at the onset of Tx. */
	msm_hs_set_wake_peer(lpm->uart_port, nitrous_prepare_uart_tx_locked);

	return 0;

err_request_irq:
	lpm->irq_host_wake = 0;
	return rc;
}

static void nitrous_lpm_cleanup(struct nitrous_bt_lpm *lpm)
{
	free_irq(lpm->irq_host_wake, NULL);
	lpm->irq_host_wake = 0;
	msm_hs_set_wake_peer(lpm->uart_port, NULL);
	wake_lock_destroy(&lpm->dev_lock);
	wake_lock_destroy(&lpm->host_lock);
}

/*
 * Set BT power on/off (blocked is true: off; blocked is false: on)
 */
static int nitrous_rfkill_set_power(void *data, bool blocked)
{
	struct nitrous_bt_lpm *lpm = data;

	if (!lpm) {
		pr_err("%s: missing lpm\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: %s (blocked=%d)\n", __func__, blocked ? "off" : "on",
		blocked);

	if (blocked == lpm->rfkill_blocked) {
		pr_info("%s already in requsted state. Ignoring.\n", __func__);
		return 0;
	}

	if (!blocked) {
		int rc;

		/*
		 * Power up the BT chip. Datasheet of BCM4343W suggests at
		 * least a 10ms time delay between consecutive toggles.
		 */
		if (gpio_is_valid(lpm->gpio_reset))
			gpio_set_value(lpm->gpio_reset, 0);

		gpio_set_value(lpm->gpio_power, 0);
		msleep(30);
		gpio_set_value(lpm->gpio_power, 1);


		if (gpio_is_valid(lpm->gpio_reset)) {
			usleep(10 * 1000);
			gpio_set_value(lpm->gpio_reset, 1);
			usleep(10 * 1000);
		}

		/* Enable host_wake irq to get ready */
		rc = irq_set_irq_type(lpm->irq_host_wake,
			lpm->host_wake_pol ?
				IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);
		if (unlikely(rc))
			pr_err("%s: error setting irq type %d\n", __func__, rc);
		enable_irq(lpm->irq_host_wake);
	} else {
		/* Disable host wake IRQ and release Rx wakelock*/
		disable_irq(lpm->irq_host_wake);
		nitrous_wake_device_locked(lpm, false);

		/* Cancel pending LPM timer and release Tx wakelock*/
		hrtimer_cancel(&lpm->tx_lpm_timer);
		nitrous_wake_uart(lpm, false);

		/* Power down the BT chip */
		if (gpio_is_valid(lpm->gpio_reset))
			gpio_set_value(lpm->gpio_reset, 0);
		gpio_set_value(lpm->gpio_power, 0);
	}

	lpm->rfkill_blocked = blocked;

	return 0;
}

static const struct rfkill_ops nitrous_rfkill_ops = {
	.set_block = nitrous_rfkill_set_power,
};

static int nitrous_rfkill_init(struct platform_device *pdev,
	struct nitrous_bt_lpm *lpm)
{
	int rc;

	/* This GPIO only exists on some boards.  Make it optional */
	rc = read_and_request_gpio(
		pdev,
		"reset-gpio",
		&lpm->gpio_reset,
		GPIOF_OUT_INIT_LOW,
		"reset_gpio"
	);

	rc = read_and_request_gpio(
		pdev,
		"power-gpio",
		&lpm->gpio_power,
		GPIOF_OUT_INIT_LOW,
		"power_gpio"
	);
	if (unlikely(rc < 0))
		goto err_gpio_power_reg;

	lpm->rfkill = rfkill_alloc(
		"nitrous_bluetooth",
		&pdev->dev,
		RFKILL_TYPE_BLUETOOTH,
		&nitrous_rfkill_ops,
		lpm
	);
	if (unlikely(!lpm->rfkill)) {
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	/* Make sure rfkill core is initialized to be blocked initially. */
	rfkill_init_sw_state(lpm->rfkill, true);
	rc = rfkill_register(lpm->rfkill);
	if (unlikely(rc))
		goto err_rfkill_register;

	/* Power off chip at startup. */
	nitrous_rfkill_set_power(lpm, true);
	return 0;

err_rfkill_register:
	rfkill_destroy(lpm->rfkill);
	lpm->rfkill = NULL;
err_rfkill_alloc:
	gpio_free(lpm->gpio_power);
err_gpio_power_reg:
	lpm->gpio_power = 0;
	if (gpio_is_valid(lpm->gpio_reset)) {
		gpio_free(lpm->gpio_reset);
		lpm->gpio_reset = -1;  /* invalid gpio */
	}
	return rc;
}

static void nitrous_rfkill_cleanup(struct nitrous_bt_lpm *lpm)
{
	nitrous_rfkill_set_power(lpm, true);
	rfkill_unregister(lpm->rfkill);
	rfkill_destroy(lpm->rfkill);
	lpm->rfkill = NULL;
	gpio_free(lpm->gpio_power);
	lpm->gpio_power = 0;
	if (gpio_is_valid(lpm->gpio_reset)) {
		gpio_free(lpm->gpio_reset);
		lpm->gpio_reset = -1;  /* invalid gpio */
	}
}

static int nitrous_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nitrous_bt_lpm *lpm;
	struct device_node *np = dev->of_node;
	u32 port_number;
	int rc = 0;

	lpm = devm_kzalloc(dev, sizeof(struct nitrous_bt_lpm), GFP_KERNEL);
	if (!lpm)
		return -ENOMEM;

	lpm->pdev = pdev;

	if (of_property_read_u32(np, "uart-port", &port_number)) {
		pr_err("%s: UART port not in dev tree\n",  __func__);
		return -EINVAL;
	}
	lpm->uart_port = msm_hs_get_uart_port(port_number);

	if (of_property_read_u32(np, "host-wake-polarity",
		&lpm->host_wake_pol)) {
		pr_err("%s: host wake polarity not in dev tree.\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_u32(np, "dev-wake-polarity",
		&lpm->dev_wake_pol)) {
		pr_err("%s: dev wake polarity not in dev tree\n", __func__);
		return -EINVAL;
	}

	rc = read_and_request_gpio(
		pdev,
		"dev-wake-gpio",
		&lpm->gpio_dev_wake,
		GPIOF_OUT_INIT_LOW,
		"dev_wake_gpio"
	);
	if (unlikely(rc < 0))
		goto err_gpio_dev_req;

	rc = read_and_request_gpio(
		pdev,
		"host-wake-gpio",
		&lpm->gpio_host_wake,
		GPIOF_IN,
		"host_wake_gpio"
	);
	if (unlikely(rc < 0))
		goto err_gpio_host_req;

	device_init_wakeup(dev, true);

	rc = nitrous_lpm_init(lpm);
	if (unlikely(rc))
		goto err_lpm_init;

	rc = nitrous_rfkill_init(pdev, lpm);
	if (unlikely(rc))
		goto err_rfkill_init;

	platform_set_drvdata(pdev, lpm);
	bt_lpm = lpm;

	return rc;

err_rfkill_init:
	nitrous_rfkill_cleanup(lpm);
err_lpm_init:
	nitrous_lpm_cleanup(lpm);
	device_init_wakeup(dev, false);
	gpio_free(lpm->gpio_host_wake);
err_gpio_host_req:
	lpm->gpio_host_wake = 0;
	gpio_free(lpm->gpio_dev_wake);
err_gpio_dev_req:
	lpm->gpio_dev_wake = 0;
	devm_kfree(dev, lpm);
	return rc;
}

static int nitrous_remove(struct platform_device *pdev)
{
	struct nitrous_bt_lpm *lpm = platform_get_drvdata(pdev);

	if (!lpm) {
		pr_err("%s: missing lpm\n", __func__);
		return -EINVAL;
	}

	nitrous_rfkill_cleanup(lpm);
	nitrous_lpm_cleanup(lpm);

	gpio_free(lpm->gpio_dev_wake);
	gpio_free(lpm->gpio_host_wake);
	lpm->gpio_dev_wake = 0;
	lpm->gpio_host_wake = 0;
	devm_kfree(&pdev->dev, lpm);

	return 0;
}

static int nitrous_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nitrous_bt_lpm *lpm = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev) && !lpm->rfkill_blocked) {
		enable_irq_wake(lpm->irq_host_wake);

		/* Reset flag to capture pending irq before resume */
		lpm->pending_irq = false;
	}

	lpm->is_suspended = true;

	return 0;
}

static int nitrous_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct nitrous_bt_lpm *lpm = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev) && !lpm->rfkill_blocked) {
		disable_irq_wake(lpm->irq_host_wake);

		/* Handle pending host wake irq. */
		if (lpm->pending_irq) {
			pr_info("%s: pending host_wake irq\n", __func__);
			nitrous_wake_uart(lpm, true);
			lpm->pending_irq = false;
		}
	}

	lpm->is_suspended = false;

	return 0;
}

static struct of_device_id nitrous_match_table[] = {
	{.compatible = "goog,nitrous"},
	{}
};

static const struct dev_pm_ops nitrous_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nitrous_suspend, nitrous_resume)
};

static struct platform_driver nitrous_platform_driver = {
	.probe = nitrous_probe,
	.remove =  nitrous_remove,
	.driver = {
		.name = "nitrous_bluetooth",
		.owner = THIS_MODULE,
		.of_match_table = nitrous_match_table,
		.pm = &nitrous_pm_ops,
	},
};

static int __init nitrous_init(void)
{
	return platform_driver_register(&nitrous_platform_driver);
}

static void __exit nitrous_exit(void)
{
	platform_driver_unregister(&nitrous_platform_driver);
}

module_init(nitrous_init);
module_exit(nitrous_exit);
MODULE_DESCRIPTION("Nitrous Oxide Driver for Bluetooth");
MODULE_AUTHOR("Google");
MODULE_LICENSE("GPL");
