/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.

* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.

* Copyright (C) 2006-2007 - Motorola
* Copyright (c) 2008-2010, The Linux Foundation. All rights reserved.
* Copyright (c) 2013, LGE Inc.
* Date Author Comment

* ----------- -------------- --------------------------------
* 2006-Apr-28 Motorola The kernel module for running the Bluetooth(R)
*                      Sleep-Mode Protocol from the Host side
* 2006-Sep-08 Motorola Added workqueue for handling sleep work.
* 2007-Jan-24 Motorola Added mbm_handle_ioi() call to ISR.
* 2009-Aug-10 Motorola Changed "add_timer" to "mod_timer" to solve
* race when flurry of queued work comes in.
*/

#define pr_fmt(fmt) "Bluetooth: %s: " fmt, __func__

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

#define BT_SLEEP_DBG
#ifndef BT_SLEEP_DBG
#define BT_DBG(fmt, arg...)
#endif

/*
* Defines
*/

#define VERSION "1.1"
#define PROC_DIR "bluetooth/sleep"

#define POLARITY_LOW 0
#define POLARITY_HIGH 1

#define BT_PORT_ID 0

/* To define User/Group of PROC_FS files */
#define AID_BLUETOOTH 1002
#define AID_NET_BT_STACK 3008

enum {
    DEBUG_USER_STATE = 1U << 0,
    DEBUG_SUSPEND = 1U << 1,
    DEBUG_BTWAKE = 1U << 2,
    DEBUG_VERBOSE = 1U << 3,
};

static int debug_mask = DEBUG_SUSPEND | DEBUG_BTWAKE | DEBUG_VERBOSE | DEBUG_USER_STATE; 
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

struct bluesleep_info {
		unsigned host_wake;
		unsigned ext_wake;
		unsigned host_wake_irq;
		struct uart_port *uport;
		struct wake_lock wake_lock;
		int irq_polarity;
		int has_ext_wake;
		int tx_timer_interval;
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle() schedule_delayed_work(&sleep_workqueue, 0)

/* 3 second timeout */
#define TX_TIMER_INTERVAL 3

/* state variable names and bit positions */
#define BT_PROTO 0x01
#define BT_TXDATA 0x02
#define BT_ASLEEP 0x04
#define BT_EXT_WAKE 0x08
#define BT_SUSPEND  0x10

static bool has_lpm_enabled = false;
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

/** Transmission timer */
static void bluesleep_tx_timer_expire(unsigned long data);
static DEFINE_TIMER(tx_timer, bluesleep_tx_timer_expire, 0, 0);

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

	pr_info("HSUART CLOCK:  %s\n", on ? "ON" : "OFF");
	if (on) {
		msm_hs_request_clock_on(bsi->uport);
		msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
		
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
		(test_bit(BT_EXT_WAKE, &flags)) && (bsi->uport != NULL));
}

void bluesleep_sleep_wakeup(void)
{
	if (test_bit(BT_ASLEEP, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("waking up...\n");

		wake_lock(&bsi->wake_lock);
		
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (bsi->tx_timer_interval * HZ));
		
		if (debug_mask & DEBUG_BTWAKE)
			pr_info("BT WAKE: set to wake\n");
		
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);
			
		clear_bit(BT_EXT_WAKE, &flags);
		clear_bit(BT_ASLEEP, &flags);

		/*Activating UART */
		hsuart_power(1);
	} else {
		pr_info("bluesleep_sleep_wakeup : already wake up");
		mod_timer(&tx_timer, jiffies + (bsi->tx_timer_interval * HZ));
	}
}

/**
* @brief@ main sleep work handling function which update the flags
* and activate and deactivate UART ,check FIFO.
*/
static void bluesleep_sleep_work(struct work_struct *work)
{
	if(!has_lpm_enabled) {
		BT_ERR("bluesleep_outgoing_data(), bluesleep is not enabled");
		return;
	}

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
			hsuart_power(0);
			
			/* UART clk is not turned off immediately. Release
			* wakelock after 500 ms.
			*/
			wake_lock_timeout(&bsi->wake_lock, HZ / 2);
		} else {
			mod_timer(&tx_timer, jiffies + (bsi->tx_timer_interval * HZ));
			return;
		}
	} else if (test_bit(BT_EXT_WAKE, &flags)
				&& !test_bit(BT_ASLEEP, &flags)) {
				
			mod_timer(&tx_timer, jiffies + (bsi->tx_timer_interval * HZ));

			if (debug_mask & DEBUG_BTWAKE)
				pr_info("BT WAKE: set to wake\n");
				
			if (bsi->has_ext_wake == 1)
				gpio_set_value(bsi->ext_wake, 0);
				
			clear_bit(BT_EXT_WAKE, &flags);
	} else {
		bluesleep_sleep_wakeup();
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
void bluesleep_outgoing_data(void)
{
	unsigned long irq_flags;

	if(!has_lpm_enabled) {
		BT_ERR("bluesleep_outgoing_data(), bluesleep is not enabled");
		return;
	}
	spin_lock_irqsave(&rw_lock, irq_flags);

	/* log data passing by */
	set_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);

	/* if the tx side is sleeping... */
	if (test_bit(BT_EXT_WAKE, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("tx was sleeping\n");
		bluesleep_sleep_wakeup();
	}
}
EXPORT_SYMBOL(bluesleep_outgoing_data);
/**
* Handles transmission timer expiration.
* @param data Not used.
*/
static void bluesleep_tx_timer_expire(unsigned long data)
{
	unsigned long irq_flags;
	
	if (debug_mask & DEBUG_VERBOSE)
		pr_info("Tx timer expired\n");
	
	spin_lock_irqsave(&rw_lock, irq_flags);
	
	/* were we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &flags)) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("Tx has been idle\n");

		if (debug_mask & DEBUG_BTWAKE)
			pr_info("BT WAKE: set to sleep\n");

		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);
		
		set_bit(BT_EXT_WAKE, &flags);
		bluesleep_tx_idle();
	} else {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("Tx data during last period\n");
		mod_timer(&tx_timer, jiffies +
			(bsi->tx_timer_interval * HZ));
	}
	
	/* clear the incoming data flag */
	clear_bit(BT_TXDATA, &flags);

	spin_unlock_irqrestore(&rw_lock, irq_flags);
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
	
	/* start the timer */
	mod_timer(&tx_timer, jiffies + (bsi->tx_timer_interval * HZ));

	/* assert BT_WAKE */
	if (debug_mask & DEBUG_BTWAKE)
		pr_info("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);

	clear_bit(BT_EXT_WAKE, &flags);

	retval = enable_irq_wake(bsi->host_wake_irq);
	if (retval < 0) {
		BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		goto fail;
	}

	set_bit(BT_PROTO, &flags);
	wake_lock(&bsi->wake_lock);
	return 0;

fail:
	del_timer(&tx_timer);
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
		pr_info("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 0);

	clear_bit(BT_EXT_WAKE, &flags);
	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		hsuart_power(1);
	} else {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
	}

	atomic_inc(&open_count);

	if (disable_irq_wake(bsi->host_wake_irq))
		BT_ERR("Couldn't disable hostwake IRQ wakeup mode");

	wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}

static int bluesleep_write_proc_btwrite(struct file *file,
					const char __user * buffer,
					size_t count, loff_t * ppos)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	/* HCI_DEV_WRITE */
	if (b != '0')
		bluesleep_outgoing_data();

	return count;
}

static int bluesleep_write_proc_lpm(struct file *file,
				const char __user * buffer, size_t count,
				loff_t * ppos)
{
	char b;
	
	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b == '0') {
		/* HCI_DEV_UNREG */
		bluesleep_stop();
		has_lpm_enabled = false;
		bsi->uport = NULL;
	} else {
		/* HCI_DEV_REG */
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			bsi->uport = msm_hs_get_uart_port(BT_PORT_ID);
			/* if bluetooth started, start bluesleep */
			bluesleep_start();
		}
	}
	
	return count;
}

static int bluesleep_populate_dt_pinfo(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int tmp;
	u32 val;
	
	if (!of_property_read_u32(np, "tx-timer-interval", &val))
		bsi->tx_timer_interval = val;

	tmp = of_get_named_gpio(np, "host-wake-gpio", 0);
	if (tmp < 0) {
		BT_ERR("couldn't find host_wake gpio");
		return -ENODEV;
	}
	bsi->host_wake = tmp;

	tmp = of_get_named_gpio(np, "ext-wake-gpio", 0);
	if (tmp < 0)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;
		
	if (bsi->has_ext_wake)
		bsi->ext_wake = tmp;

	BT_INFO("bt_host_wake %d, bt_ext_wake %d",
			bsi->host_wake, bsi->ext_wake);
	return 0;
}

static int bluesleep_populate_pinfo(struct platform_device *pdev)
{
	struct resource *res;
	
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"gpio_host_wake");
	if (!res) {
		BT_ERR("couldn't find host_wake gpio");
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
	struct resource *res;
	int ret;
	
	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		ret = bluesleep_populate_dt_pinfo(pdev);
		if (ret < 0) {
			BT_ERR("couldn't populate info from dt");
			goto free_bsi;
		}
	} else {
		ret = bluesleep_populate_pinfo(pdev);
		if (ret < 0) {
			BT_ERR("couldn't populate info");
			goto free_bsi;
		}
	}
	
	if (!bsi->tx_timer_interval)
		bsi->tx_timer_interval = TX_TIMER_INTERVAL;

	/* configure host_wake as input */
	ret = gpio_request_one(bsi->host_wake, GPIOF_IN, "bt_host_wake");
	if (ret < 0) {
		BT_ERR("failed to configure input"
			" direction for GPIO %d, error %d", bsi->host_wake, ret);
		goto free_bsi;
	}
	
	if (debug_mask & DEBUG_BTWAKE)
		pr_info("BT WAKE: set to wake\n");
	if (bsi->has_ext_wake) {
		/* configure ext_wake as output mode */
		ret = gpio_request_one(bsi->ext_wake, GPIOF_OUT_INIT_LOW,
							"bt_ext_wake");
		if (ret < 0) {
			BT_ERR("failed to configure output"
				" direction for GPIO %d, error %d",
				bsi->ext_wake, ret);
			goto free_bt_host_wake;
		}
	}
	clear_bit(BT_EXT_WAKE, &flags);

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "host_wake");
	if (!res) {
		BT_ERR("couldn't find host_wake irq");
		ret = -ENODEV;
		goto free_bt_host_wake;
	}
	bsi->host_wake_irq = res->start;
	if (bsi->host_wake_irq < 0) {
		BT_ERR("couldn't find host_wake irq");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}
	
	bsi->irq_polarity = POLARITY_LOW;	/*low edge (falling edge) */

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");
	clear_bit(BT_SUSPEND, &flags);

	BT_INFO("host_wake_irq %d, polarity %d",
		bsi->host_wake_irq, bsi->irq_polarity);

	ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
					IRQF_DISABLED | IRQF_TRIGGER_FALLING,
					"bluetooth hostwake", NULL);
	if (ret < 0) {
		BT_ERR("Couldn't acquire BT_HOST_WAKE IRQ");
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
	{.compatible = "qcom,bluesleep"},
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

static const struct file_operations btwrite_fops = {
	.write = bluesleep_write_proc_btwrite
};

static const struct file_operations lpm_fops = {
	.write = bluesleep_write_proc_lpm
};

// For new proc apis
static struct proc_dir_entry *bluesleep_proc_create(const char *name,
						umode_t mode,
						struct proc_dir_entry
						*parent,
						const struct file_operations
						*fops)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	return proc_create(name, mode, parent, fops);
#else
	struct proc_dir_entry *entry;
	entry = create_proc_entry(name, mode, parent);
	if (entry)
		entry->proc_fops = fops;
	return entry;
#endif
}

static void bluesleep_proc_set_uid_gid(struct proc_dir_entry *ent)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	proc_set_user(ent, KUIDT_INIT(AID_BLUETOOTH),
		KGIDT_INIT(AID_NET_BT_STACK));
#else
	ent->uid = AID_BLUETOOTH;
	ent->gid = AID_NET_BT_STACK;
#endif
}

/**
* Initializes the module.
* @return On success, 0. On error, -1, and <code>errno</code> is set
* appropriately.
*/
static int __init bluesleep_init(void)
{
	int retval;
	struct proc_dir_entry *ent;

	BT_INFO("BlueSleep Mode Driver Ver %s", VERSION);

	retval = platform_driver_register(&bluesleep_driver);
	if (retval)
		return retval;

	if (bsi == NULL)
		return 0;

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		BT_ERR("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		BT_ERR("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	/* Creating write only "lpm" entry */
	ent =
		bluesleep_proc_create("btwrite", S_IRUGO | S_IWUSR | S_IWGRP,
							sleep_dir, &btwrite_fops);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	bluesleep_proc_set_uid_gid(ent);

	/* Creating write only "lpm" entry */
	ent =
		bluesleep_proc_create("lpm", S_IRUGO | S_IWUSR | S_IWGRP, sleep_dir,
							&lpm_fops);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/lpm entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}
	bluesleep_proc_set_uid_gid(ent);

	flags = 0;	/* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	/* assert bt wake */
	if (debug_mask & DEBUG_BTWAKE)
		pr_info("BT WAKE: set to wake\n");
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
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&tx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(1);
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
