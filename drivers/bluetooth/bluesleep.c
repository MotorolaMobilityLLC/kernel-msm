/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.


 * Copyright (C) 2006-2007 - Motorola
 * Copyright (c) 2008-2010, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013, LGE Inc.

 * Date         Author           Comment
 * -----------  --------------   --------------------------------
 * 2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
 *                               Sleep-Mode Protocol from the Host side
 *  2006-Sep-08  Motorola        Added workqueue for handling sleep work.
 *  2007-Jan-24  Motorola        Added mbm_handle_ioi() call to ISR.
 *  2009-Aug-10  Motorola        Changed "add_timer" to "mod_timer" to solve
 *                               race when flurry of queued work comes in.
 */

#define pr_fmt(fmt)	"Bluetooth: %s: " fmt, __func__

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/serial_core.h>
#include <linux/platform_data/msm_serial_hs.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"

#define BT_SLEEP_DBG
#ifndef BT_SLEEP_DBG
#define BT_DBG(fmt, arg...)
#endif
/*
 * Defines
 */

#define VERSION		"1.2"
#define PROC_DIR	"bluetooth/sleep"

#define POLARITY_LOW 0
#define POLARITY_HIGH 1
#define HS_UART_ON 1
#define HS_UART_OFF 0
#define BT_PORT_ID	0

/* enable/disable wake-on-bluetooth */
#define BT_ENABLE_IRQ_WAKE 1

#define BT_BLUEDROID_SUPPORT 1
enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 1,
	DEBUG_BTWAKE = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
	DEBUG_BTWRITE = 1U << 4,

};

static int debug_mask = DEBUG_USER_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct bluesleep_info {
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	struct uart_port *uport;
	struct wake_lock wake_lock;
	int irq_polarity;
	int has_ext_wake;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* 1 second timeout */
#define RX_TIMER_INTERVAL  1

/* state variable names and bit positions */
#define BT_PROTO	0x01
#define BT_TXDATA	0x02
#define BT_ASLEEP	0x04
#define BT_EXT_WAKE	0x08
#define BT_SUSPEND	0x10
#define BT_RXTIMER	0x20

#define PROC_BTWAKE	0
#define PROC_HOSTWAKE	1
#define PROC_PROTO	2
#define PROC_ASLEEP	3
#define PROC_LPM	4
#define PROC_BTWRITE	5

static bool has_lpm_enabled;

static struct platform_device *bluesleep_uart_dev;
static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */
static int bluesleep_start(void);
static void bluesleep_stop(void);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Reception timer */
static void bluesleep_rx_timer_expire(unsigned long data);
static DEFINE_TIMER(rx_timer, bluesleep_rx_timer_expire, 0, 0);

/** Lock for state transitions */
static spinlock_t rw_lock;

struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */

static void hsuart_power(int on)
{
	if (test_bit(BT_SUSPEND, &flags))
		return;
	if (on) {
		// make sure port is active before enable it.
		if(bsi->uport->state->port.count)
		{
			msm_hs_request_clock_on(bsi->uport);
			msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
		}
	} else {
		msm_hs_set_mctrl(bsi->uport, 0);
		msm_hs_request_clock_off(bsi->uport);
	}
}

/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
int bluesleep_can_sleep(void)
{
	/* check if WAKE_BT_GPIO and BT_WAKE_GPIO are both deasserted */
	return ((gpio_get_value(bsi->host_wake) != bsi->irq_polarity) &&
		(test_bit(BT_EXT_WAKE, &flags)) && (!test_bit(BT_RXTIMER, &flags)) &&
		(bsi->uport != NULL));
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("already asleep\n");
			return;
		}

		if (msm_hs_tx_empty(bsi->uport)) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("going to sleep...\n");
			set_bit(BT_ASLEEP, &flags);
			/*Deactivating UART */
			hsuart_power(HS_UART_OFF);
			/* UART clk is not turned off immediately. Release
			 * wakelock after 500 ms.
			 */
			wake_lock_timeout(&bsi->wake_lock, HZ / 2);
		} else {
			pr_err("This should never happen.\n");
			return;
		}
	} else if (test_bit(BT_ASLEEP, &flags)) {
		/* Can not sleep but UART has already sleep */
		if (debug_mask & DEBUG_SUSPEND)
			pr_err("waking up...\n");
		wake_lock(&bsi->wake_lock);
		clear_bit(BT_ASLEEP, &flags);

		/* Add a timer to make sure that UART
		 * would not be turned on&off very frequentently
		 */
		mod_timer(&rx_timer, jiffies + (RX_TIMER_INTERVAL * HZ));

		set_bit(BT_RXTIMER, &flags);
		hsuart_power(HS_UART_ON);
	} else {
		if (debug_mask & DEBUG_SUSPEND)
			pr_err("default branch, do nothing.\n");
	}
}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("hostwake line change\n");

	spin_lock(&rw_lock);
	if ((gpio_get_value(bsi->host_wake) == bsi->irq_polarity))
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
static void bluesleep_outgoing_data(void)
{
	unsigned long irq_flags;
	int power_on_uart = 0;

	spin_lock_irqsave(&rw_lock, irq_flags);

	/* if the tx side is sleeping... */
	if (test_bit(BT_EXT_WAKE, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("tx was sleeping\n");
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
	}

	if (test_bit(BT_ASLEEP, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_err("waking up...\n");
		wake_lock(&bsi->wake_lock);
		clear_bit(BT_ASLEEP, &flags);
		power_on_uart = 1;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (power_on_uart == 1)
		hsuart_power(HS_UART_ON);
}

/**
 * Function to check wheather bluetooth can sleep when btwrite was deasserted
 * by bluedroid.
 */
static void bluesleep_tx_allow_sleep(void)
{
	unsigned long irq_flags;

	if (debug_mask & DEBUG_VERBOSE)
		pr_err("%s\n", __FUNCTION__);
	if (debug_mask & DEBUG_SUSPEND)
		pr_err("Tx has been idle\n");

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 1);
	set_bit(BT_EXT_WAKE, &flags);
	bluesleep_tx_idle();

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Handles reception timer expiration.
 * Clear BT_RXTIMER.
 * @param data Not used.
 */
static void bluesleep_rx_timer_expire(unsigned long data)
{
	clear_bit(BT_RXTIMER, &flags);
	bluesleep_rx_idle();
}

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	/* schedule a tasklet to handle the change in the host wake line */
	tasklet_schedule(&hostwake_task);
	return IRQ_HANDLED;
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int bluesleep_start(void)
{
	int retval;
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return 0;
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	if (!atomic_dec_and_test(&open_count)) {
		atomic_inc(&open_count);
		return -EBUSY;
	}

	spin_lock_irqsave(&rw_lock, irq_flags);

	/* assert BT_WAKE */
	if (debug_mask & DEBUG_BTWAKE)
		pr_err("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
	clear_bit(BT_ASLEEP, &flags);
	spin_unlock_irqrestore(&rw_lock, irq_flags);

#if BT_ENABLE_IRQ_WAKE
	retval = enable_irq_wake(bsi->host_wake_irq);
	if (retval < 0) {
		pr_err("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		goto fail;
	}
#endif
	set_bit(BT_PROTO, &flags);
	wake_lock(&bsi->wake_lock);
	return 0;
fail:
	del_timer(&rx_timer);
	atomic_inc(&open_count);

	return retval;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	/* assert BT_WAKE */
	if (debug_mask & DEBUG_BTWAKE)
		pr_err("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
	del_timer(&rx_timer);
	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		hsuart_power(HS_UART_ON);
	} else {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
	}

	atomic_inc(&open_count);

#if BT_ENABLE_IRQ_WAKE
	if (disable_irq_wake(bsi->host_wake_irq))
		pr_err("Couldn't disable hostwake IRQ wakeup mode");
#endif
	wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}

void bluesleep_setup_uart_port(struct platform_device *uart_dev)
{
	bluesleep_uart_dev = uart_dev;
}

static int bluesleep_populate_dt_pinfo(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int tmp;

	tmp = of_get_named_gpio(np, "bt_host_wake", 0);
	if (tmp < 0) {
		pr_err("couldn't find host_wake gpio");
		return -ENODEV;
	}
	bsi->host_wake = tmp;

	tmp = of_get_named_gpio(np, "bt_ext_wake", 0);
	if (tmp < 0)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake)
		bsi->ext_wake = tmp;

	BT_INFO("bt_host_wake %d, bt_ext_wake %d",
			bsi->host_wake,
			bsi->ext_wake);
	return 0;
}

static int bluesleep_populate_pinfo(struct platform_device *pdev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		pr_err("couldn't find host_wake gpio");
		return -ENODEV;
	}
	bsi->host_wake = res->start;
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake)
		bsi->ext_wake = res->start;
	return 0;
}

static int bluesleep_probe(struct platform_device *pdev)
{
	int ret;

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		ret = bluesleep_populate_dt_pinfo(pdev);
		if (ret < 0) {
			pr_err("couldn't populate info from dt");
			return ret;
		}
	} else {
		ret = bluesleep_populate_pinfo(pdev);
		if (ret < 0) {
			pr_err("couldn't populate info");
			return ret;
		}
	}

	/* configure host_wake as input */
	ret = gpio_request_one(bsi->host_wake, GPIOF_IN, "bt_host_wake");
	if (ret < 0) {
		pr_err("failed to configure input direction for GPIO %d err %d",
				bsi->host_wake, ret);
		goto free_bsi;
	}

	if (debug_mask & DEBUG_BTWAKE)
		pr_err("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake) {
		/* configure ext_wake as output mode*/
		ret = gpio_request_one(bsi->ext_wake,
				GPIOF_OUT_INIT_HIGH, "bt_ext_wake");
		if (ret < 0) {
			pr_err("failed to configure output direction for GPIO %d err %d",
					bsi->ext_wake, ret);
			goto free_bt_host_wake;
		}
	}
	clear_bit(BT_EXT_WAKE, &flags);

	bsi->host_wake_irq = gpio_to_irq(bsi->host_wake);
	if (bsi->host_wake_irq < 0) {
		pr_err("couldn't find host_wake irq");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	bsi->irq_polarity = POLARITY_LOW;/*low edge (falling edge)*/

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");
	clear_bit(BT_SUSPEND, &flags);

	BT_INFO("host_wake_irq %d, polarity %d",
			bsi->host_wake_irq,
			bsi->irq_polarity);

	ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
			IRQF_DISABLED | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"bluetooth hostwake", NULL);
	if (ret  < 0) {
		pr_err("Couldn't acquire BT_HOST_WAKE IRQ");
		goto free_bt_ext_wake;
	}

	return 0;

free_bt_ext_wake:
	gpio_free(bsi->ext_wake);
free_bt_host_wake:
	gpio_free(bsi->host_wake);
free_bsi:
	kfree(bsi);
	return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	free_irq(bsi->host_wake_irq, NULL);
	gpio_free(bsi->host_wake);
	gpio_free(bsi->ext_wake);
	wake_lock_destroy(&bsi->wake_lock);
	kfree(bsi);
	return 0;
}


static int bluesleep_resume(struct platform_device *pdev)
{
	if (test_bit(BT_SUSPEND, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("bluesleep resuming...\n");
		if ((bsi->uport != NULL) &&
			(gpio_get_value(bsi->host_wake) == bsi->irq_polarity)) {
			if (debug_mask & DEBUG_SUSPEND)
				pr_info("bluesleep resume from BT event...\n");
			msm_hs_request_clock_on(bsi->uport);
			msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
		}
		clear_bit(BT_SUSPEND, &flags);
	}
	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (debug_mask & DEBUG_SUSPEND)
		pr_info("bluesleep suspending...\n");
	set_bit(BT_SUSPEND, &flags);
	return 0;
}

static struct of_device_id bluesleep_match_table[] = {
	{ .compatible = "bcm,bluesleep" },
	{}
};

static struct platform_driver bluesleep_driver = {
	.probe = bluesleep_probe,
	.remove = bluesleep_remove,
	.suspend = bluesleep_suspend,
	.resume = bluesleep_resume,
	.driver = {
		.name = "bluesleep",
		.owner = THIS_MODULE,
		.of_match_table = bluesleep_match_table,
	},
};

static int bluesleep_proc_show(struct seq_file *m, void *v)
{
	switch ((long)m->private) {
	case PROC_LPM:
		seq_printf(m, "lpm: %u\n",
				test_bit(has_lpm_enabled, &flags) ? 1 : 0);
		break;
	case PROC_BTWRITE:
		seq_printf(m, "btwrite: %u\n",
				test_bit(BT_TXDATA, &flags) ? 1 : 0);
		break;

	default:
		return 0;
	}
	return 0;
}

static ssize_t bluesleep_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *pos)
{
	void *data = PDE_DATA(file_inode(file));
	char lbuf[32];

	if (count >= sizeof(lbuf))
		count = sizeof(lbuf)-1;

	if (copy_from_user(lbuf, buf, count))
		return -EFAULT;
	lbuf[count] = 0;

	switch ((long)data) {
	case PROC_LPM:
		if (lbuf[0] == '0') {
			/* HCI_DEV_UNREG */
			bluesleep_stop();
			has_lpm_enabled = false;
			bsi->uport = NULL;
		} else {
			/* HCI_DEV_REG */
			if (!has_lpm_enabled) {
				has_lpm_enabled = true;
				bsi->uport = msm_hs_get_uart_port(BT_PORT_ID);
				/* if bluetooth started, start bluesleep*/
				bluesleep_start();
			}
		}
		break;
	case PROC_BTWRITE:
		if (lbuf[0] != '0')
			bluesleep_outgoing_data();
		else
			bluesleep_tx_allow_sleep();
		break;
	default:
		return 0;
	}

	return count;
}

static int bluesleep_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bluesleep_proc_show, PDE_DATA(inode));
}

static const struct file_operations bluesleep_proc_readwrite_fops = {
	.owner	= THIS_MODULE,
	.open	= bluesleep_proc_open,
	.read   = seq_read,
	.write  = bluesleep_proc_write,
	.release = single_release,

};
static const struct file_operations bluesleep_proc_read_fops = {
	.owner	= THIS_MODULE,
	.open	= bluesleep_proc_open,
	.read   = seq_read,
	.release = single_release,

};

/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	pr_err("BlueSleep Mode Driver Ver %s", VERSION);

	retval = platform_driver_register(&bluesleep_driver);
	if (retval) {
		pr_err("\n:retval = %d",retval);
		return retval;
	}

	if (bsi == NULL) {
		pr_err("bsi is NULL.");
		return 0;
	}

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		pr_err("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		pr_err("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	/* read/write proc entries */
	ent = proc_create_data("lpm", S_IRUGO | S_IWUSR | S_IWGRP,
			sleep_dir, &bluesleep_proc_readwrite_fops,
			(void *)PROC_LPM);
	if (ent == NULL) {
		pr_err("Unable to create /proc/%s/lpm entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create_data("btwrite", S_IRUGO | S_IWUSR | S_IWGRP,
			sleep_dir, &bluesleep_proc_readwrite_fops,
			(void *)PROC_BTWRITE);
	if (ent == NULL) {
		pr_err("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&rx_timer);
	rx_timer.function = bluesleep_rx_timer_expire;
	rx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	/* assert bt wake */
	if (debug_mask & DEBUG_BTWAKE)
		pr_err("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);

	return 0;

fail:
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
	return retval;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	if (bsi == NULL)
		return;

	/* assert bt wake */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);
	clear_bit(BT_EXT_WAKE, &flags);
	if (test_bit(BT_PROTO, &flags)) {
		if (disable_irq_wake(bsi->host_wake_irq))
			pr_err("Couldn't disable hostwake IRQ wakeup mode");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&rx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(HS_UART_ON);
	}

	platform_driver_unregister(&bluesleep_driver);

	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
