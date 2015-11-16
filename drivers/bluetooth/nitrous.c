/*
 * Bluetooth low power control via GPIO
 *
 *	Copyright (C) 2015 Google, Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/rfkill.h>
#include <linux/platform_data/msm_serial_hs.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>

static const char MODULE_NAME[] = "nitrous_bluetooth";

static void set_host_wake(bool value);
static void set_dev_wake_locked(bool value);

/* rfkill interface to turn on or off BT portion of the chip */
static struct rfkill *bt_rfkill;
static bool bt_rfkill_blocked;

static const char *WAKE_LOCK_NAME_DEV_WAKE = "bt_dev_wake";
static const char *WAKE_LOCK_NAME_HOST_WAKE = "bt_host_wake";

/* Time after idle uart before releasing wakelock */
static const int UART_TIMEOUT_SEC = 1;

/* Wakelock release timeout after deassertion HOST_WAKE, in jiffies */
static const long HOST_WAKE_TIMEOUT = HZ/2;

struct uart_lock {
	struct wake_lock wake_lock;
	int value;
	bool uart_enabled;
	long timeout_ms;
};

struct bcm_bt_lpm {
	/* gpio_dev_wake is used by the device to wake the host. */
	unsigned int gpio_dev_wake;  /* BT_DEV_WAKE (host -> dev) */
	/* gpio_host_wake is used by the host to wake up the device. */
	unsigned int gpio_host_wake; /* BT_HOST_WAKE (dev -> host) */
	unsigned int irq_host_wake;
	/* gpio_power is used to turn the BT portion of the chip on or off */
	unsigned int gpio_power;     /* BT_REG_ON */

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uart_port;

	/* Lock controlled by host */
	struct uart_lock dev_lock;

	/* Lock controlled by device */
	struct uart_lock host_lock;

	struct platform_device *pdev;
} bt_lpm;

/* Section containing potentially-highly-platform-dependent functions */

static struct uart_port *get_uart_port_by_number(int port_number)
{
	return msm_hs_get_uart_port(port_number);
}

static void set_uart_wake_peer(wake_peer_fn wake_peer)
{
	msm_hs_set_wake_peer(bt_lpm.uart_port, wake_peer);
}

/* Helper to fetch gpio information from the device tree and then register it */
static int read_and_request_gpio(
		struct platform_device *pdev,
		const char *dt_name,
		int *gpio,
		unsigned long gpio_flags,
		const char *label) {
	int ret;

	*gpio = of_get_named_gpio(pdev->dev.of_node, dt_name, 0);
	if (unlikely(*gpio < 0)) {
		pr_err("%s: %s: %s not in device tree", MODULE_NAME, __func__, dt_name);
		return -EINVAL;
	}

	if (!gpio_is_valid(*gpio)) {
		pr_err("%s: %s: %s is invalid", MODULE_NAME, __func__, dt_name);
		return -EINVAL;
	}

	ret = gpio_request_one(*gpio, gpio_flags, label);
	if (unlikely(ret < 0)) {
		pr_err("%s: %s: failed to request gpio: %d (%s), error: %d",
		       MODULE_NAME, __func__, *gpio, label, ret);
		return -EINVAL;
	}

	return 0;
}

/* Section dealing with rfkill */

/* Turn chip off when blocked is true */
static int bt_rfkill_set_power(void *data, bool blocked)
{
	pr_info("%s: %s(blocked=%d)\n", MODULE_NAME, __func__, blocked);

	if (blocked == bt_rfkill_blocked) {
		pr_info("%s: %s already in requsted state. Ignoring.\n", MODULE_NAME, __func__);
		return 0;
	}
	bt_rfkill_blocked = blocked;

	if (!blocked) {
		/* Bring up the host_wake irq */
		enable_irq(bt_lpm.irq_host_wake);
		if (enable_irq_wake(bt_lpm.irq_host_wake) < 0) {
			pr_err("%s: %s: unable to enable IRQ wake support\n",
			       MODULE_NAME, __func__);
		}

		/* No need to lock, as the blocked case tears down the timer
		   and no data should go over the UART when we've turned off
		   the BT chip */
		set_dev_wake_locked(true);
		gpio_set_value(bt_lpm.gpio_power, 0);

		/* Wake the device and bring up the regulators */
		msleep(30);
		gpio_set_value(bt_lpm.gpio_power, 1);
	} else {
		/* Bringing down the host_wake irq and cancelling the timer
		   ensures the isr and callback are not running */
		if (disable_irq_wake(bt_lpm.irq_host_wake) < 0) {
			pr_err("%s: %s: unable to disable IRQ wake support\n",
			       MODULE_NAME, __func__);
		}
		disable_irq(bt_lpm.irq_host_wake);
		hrtimer_cancel(&bt_lpm.enter_lpm_timer);

		/* Power down the BT chip */
		gpio_set_value(bt_lpm.gpio_power, 0);

		/* Chip won't necessarily toggle host_wake during power down.
		   Ensure all wakelocks are released */
		set_host_wake(false);
		set_dev_wake_locked(false);
	}

	return 0;
}

static const struct rfkill_ops bt_rfkill_ops = {
	.set_block = bt_rfkill_set_power,
};

static int bt_rfkill_init(struct platform_device *pdev)
{
	int rc;

	rc = read_and_request_gpio(
		pdev,
		"power-gpio",
		&bt_lpm.gpio_power,
		GPIOF_OUT_INIT_LOW,
		"power_gpio"
	);
	if (unlikely(rc < 0)) {
		goto err_gpio_power_reg;
	}

	bt_rfkill = rfkill_alloc(
		"nitrous_bluetooth",
		&pdev->dev,
		RFKILL_TYPE_BLUETOOTH,
		&bt_rfkill_ops,
		NULL
	);
	if (unlikely(!bt_rfkill)) {
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rc = rfkill_register(bt_rfkill);
	if (unlikely(rc)) {
		goto err_rfkill_register;
	}

	rfkill_set_states(bt_rfkill, true, false);
	/* Set blocked state to false, so the call to
	   bt_rfkill_set_power can run and set blocked to true */
	bt_rfkill_blocked = false;
	bt_rfkill_set_power(NULL, true);
	return 0;

err_rfkill_register:
	rfkill_destroy(bt_rfkill);
	bt_rfkill = NULL;
err_rfkill_alloc:
	gpio_free(bt_lpm.gpio_power);
err_gpio_power_reg:
	bt_lpm.gpio_power = 0;
	return rc;
}

static void bt_rfkill_cleanup(void)
{
	bt_rfkill_set_power(NULL, true);
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);
	bt_rfkill = NULL;
	gpio_free(bt_lpm.gpio_power);
	bt_lpm.gpio_power = 0;
}

/* Section dealing with low power management */

static void uart_lock_init(struct uart_lock *lock, const char *name, long timeout_ms)
{
	/* Set to -1 to force initial evaluation */
	lock->value = -1;
	lock->timeout_ms = timeout_ms;
	lock->uart_enabled = false;
	wake_lock_init(&lock->wake_lock, WAKE_LOCK_SUSPEND, name);
}

static void uart_lock_cleanup(struct uart_lock *lock) {
	wake_lock_destroy(&lock->wake_lock);
}

static bool update_uart_lock(struct uart_lock *lock, bool value)
{
	if (lock->value == value)
		return false;

	lock->value = value;
	if (value) {
		wake_lock(&lock->wake_lock);

		/* Grab a power manager reference if we don't have it */
		if (!lock->uart_enabled) {
			pr_debug("%s: %s: runtime get\n", MODULE_NAME, __func__);
			pm_runtime_get(bt_lpm.uart_port->dev);
			lock->uart_enabled = true;
		}
	} else {
		if (lock->timeout_ms > 0)
			wake_lock_timeout(&lock->wake_lock, lock->timeout_ms);
		else
			wake_unlock(&lock->wake_lock);

		/* Release a power manager reference if we have it */
		if (lock->uart_enabled) {
			pr_debug("%s: %s: runtime put\n", MODULE_NAME, __func__);
			pm_runtime_put(bt_lpm.uart_port->dev);
			lock->uart_enabled = false;
		}
	}

	return true;
}

/*
 * Drives the BT_DEV_WAKE GPIO from the host to the chip to request that the BT
 * chip remain active or not.
 */
static void set_dev_wake_locked(bool value)
{
	if (update_uart_lock(&bt_lpm.dev_lock, value)) {
		pr_debug("%s:-- %s (now: %d)\n", MODULE_NAME, __func__, value);
		gpio_set_value(bt_lpm.gpio_dev_wake, value);
	}
}

/*
 * Called after the timer expires indicating there has
 * been no traffic on the UART for the specified period of time.
 * De-assert the wake line to the BT chip so it can go to sleep.
 */
static enum hrtimer_restart enter_lpm(struct hrtimer *timer) {
	unsigned long flags;

	pr_info("%s:-- %s\n", MODULE_NAME, __func__);

	spin_lock_irqsave(&bt_lpm.uart_port->lock, flags);
	set_dev_wake_locked(false);
	spin_unlock_irqrestore(&bt_lpm.uart_port->lock, flags);
	return HRTIMER_NORESTART;
}

/*
 * Called every time the uart driver sends a buffer. Cancel the existing
 * timer, if any, and start a new timer. Make sure we are asserting
 * the wake line to the BT chip.
 *
 * The uart driver calls this from a context locked by uart_port->lock.
 */
void nitrous_uart_sending_buffer_locked(struct uart_port *port) {
	if (hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer) == -1) {
		dev_warn(&bt_lpm.pdev->dev, "%s timer executing unable to cancel",
		         __func__);
	}

	set_dev_wake_locked(true);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
				  HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(nitrous_uart_sending_buffer_locked);

/*
 * Enable UART and acquire wakelock to ensure communication
 * with BT chip.
 *
 * Locking is not needed, as it is only called from the ISR
 * or from rfkill when the ISR is disabled.
 */
static void set_host_wake(bool value)
{
	if (update_uart_lock(&bt_lpm.host_lock, value)) {
		pr_debug("%s:-- %s (now: %d)\n", MODULE_NAME, __func__, value);
	}
}

/*
 * Interrupt service routine watching the host wake line from the
 * BT chip to ensure we are awake when the chip needs us to be.
 */
static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int ret;
	int host_wake;

	host_wake = gpio_get_value(bt_lpm.gpio_host_wake);

	/* Invert the interrupt type to catch the next edge */
	ret = irq_set_irq_type(
		irq,
		(host_wake > 0) ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH
	);

	/* Host wake is active low, so invert when passing to set_host_wake */
	set_host_wake(!host_wake);
	return IRQ_HANDLED;
}

static int lpm_init(struct platform_device *pdev)
{
	int rc;

	pr_debug("%s: %s\n", MODULE_NAME, __func__);

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(UART_TIMEOUT_SEC, 0);
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.irq_host_wake = gpio_to_irq(bt_lpm.gpio_host_wake);
	rc = request_irq(
		bt_lpm.irq_host_wake,
		host_wake_isr,
		IRQF_TRIGGER_HIGH,
		"bt_host_wake",
		NULL
	);
	if (rc < 0) {
		pr_err("%s: %s: unable to request IRQ for bt_host_wake GPIO\n",
		       MODULE_NAME, __func__);
		goto err_request_irq;
	}

	uart_lock_init(&bt_lpm.dev_lock, WAKE_LOCK_NAME_DEV_WAKE, 0);
	uart_lock_init(&bt_lpm.host_lock, WAKE_LOCK_NAME_HOST_WAKE, HOST_WAKE_TIMEOUT);
	set_uart_wake_peer(nitrous_uart_sending_buffer_locked);

	return 0;

err_request_irq:
	bt_lpm.irq_host_wake = 0;
	return rc;
}

static void lpm_cleanup(void)
{
	free_irq(bt_lpm.irq_host_wake, NULL);
	bt_lpm.irq_host_wake = 0;

	set_uart_wake_peer(NULL);
	uart_lock_cleanup(&bt_lpm.dev_lock);
	uart_lock_cleanup(&bt_lpm.host_lock);
}

static int nitrous_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	u32 port_number;
	int rc = 0;

	pr_debug("%s: %s\n", MODULE_NAME, __func__);

	bt_lpm.pdev = pdev;

	if (of_property_read_u32(np, "uart-port", &port_number)) {
		pr_err("%s: %s: uart port not in device tree", MODULE_NAME, __func__);
		return -EINVAL;
	}
	bt_lpm.uart_port = get_uart_port_by_number(port_number);

	rc = read_and_request_gpio(
		pdev,
		"dev-wake-gpio",
		&bt_lpm.gpio_dev_wake,
		GPIOF_OUT_INIT_LOW,
		"dev_wake_gpio"
	);
	if (unlikely(rc < 0)) {
		goto err_gpio_dev_req;
	}

	rc = read_and_request_gpio(
		pdev,
		"host-wake-gpio",
		&bt_lpm.gpio_host_wake,
		GPIOF_IN,
		"host_wake_gpio"
	);
	if (unlikely(rc < 0)) {
		goto err_gpio_host_req;
	}

	rc = lpm_init(pdev);
	if (unlikely(rc)) {
		goto err_lpm_init;
	}

	rc = bt_rfkill_init(pdev);
	if (unlikely(rc)) {
		goto err_rfkill_init;
	}

	return rc;

err_rfkill_init:
	lpm_cleanup();
err_lpm_init:
	gpio_free(bt_lpm.gpio_host_wake);
err_gpio_host_req:
	bt_lpm.gpio_host_wake = 0;
	gpio_free(bt_lpm.gpio_dev_wake);
err_gpio_dev_req:
	bt_lpm.gpio_dev_wake = 0;
	return rc;
}

static int nitrous_remove(struct platform_device *pdev)
{
	pr_debug("%s: %s\n", MODULE_NAME, __func__);

	bt_rfkill_cleanup();
	lpm_cleanup();

	gpio_free(bt_lpm.gpio_dev_wake);
	gpio_free(bt_lpm.gpio_host_wake);
	bt_lpm.gpio_dev_wake = 0;
	bt_lpm.gpio_host_wake = 0;

	return 0;
}

static struct of_device_id nitrous_match_table[] = {
	{.compatible = "goog,nitrous"},
	{}
};

static struct platform_driver nitrous_platform_driver = {
	.probe = nitrous_probe,
	.remove =  nitrous_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = nitrous_match_table,
	},
};

static int __init nitrous_init(void)
{
	pr_debug("%s: %s\n", MODULE_NAME, __func__);
	return platform_driver_register(&nitrous_platform_driver);
}

static void __exit nitrous_exit(void)
{
	pr_debug("%s: %s\n", MODULE_NAME, __func__);
	platform_driver_unregister(&nitrous_platform_driver);
}

module_init(nitrous_init);
module_exit(nitrous_exit);

MODULE_DESCRIPTION("Nitrous Oxide Driver for Bluetooth");
MODULE_AUTHOR("Google");
MODULE_LICENSE("GPL");
