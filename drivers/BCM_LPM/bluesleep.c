/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2010, The Linux Foundation. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
				 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.

*/

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/serial_core.h>
#include <mach/msm_serial_hs.h>

#include <asm/gpio.h>
#include <asm/mach-types.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"

/*
 * Defines
 */

#define VERSION		"1.1"
#define PROC_DIR	"bluetooth/sleep"
#define DBG             false

#define POLARITY_LOW 0
#define POLARITY_HIGH 1
/* enable/disable wake-on-bluetooth */
#define BT_ENABLE_IRQ_WAKE 1

#define gpio_bt_wake_up_host 48
#define gpio_host_wake_up_bt 61

#define BT_PROC_FILE_PERMISSION 0777

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
#define TX_TIMER_INTERVAL	10

/* state variable names and bit positions */
#define BT_PROTO	0x01
#define BT_TXDATA	0x02
#define BT_ASLEEP	0x04
#define BT_EXT_WAKE     0x08
#define BT_SUSPEND      0x10

static bool has_lpm_enabled = false;

/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;

struct uart_port *bluesleep_uart_port;

static struct bluesleep_info *bsi;

/* module usage */
static atomic_t open_count = ATOMIC_INIT(1);

/*
 * Local function prototypes
 */

static int bluesleep_hci_event(struct notifier_block *this, unsigned long event, void *data);
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

// bool uart_clock_off = false;

/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
	.notifier_call = bluesleep_hci_event,
};

struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */

/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
static inline int bluesleep_can_sleep(void)
{
	/* check if MSM_WAKE_BT_GPIO and BT_WAKE_MSM_GPIO are both deasserted */
	if (DBG) printk("bluesleep_can_sleep\n");

	if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
	if(DBG) printk("host_wake %d\n", gpio_get_value(bsi->host_wake));
	if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
	if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));

	if (!test_bit(BT_EXT_WAKE, &flags))
		if(DBG) printk("!test_bit(BT_EXT_WAKE, &flags) return true , BT_EXT_WAKE != flags\n");
	if (bsi->uport == NULL)
		if(DBG) printk("bsi->uport is null \n");

	if(DBG) printk("sleep result : %d \n",((gpio_get_value(bsi->host_wake) != bsi->irq_polarity) && (!test_bit(BT_EXT_WAKE, &flags)) && (bsi->uport != NULL)));

	return ((gpio_get_value(bsi->host_wake) != bsi->irq_polarity) &&
			(!test_bit(BT_EXT_WAKE, &flags)) &&
			(bsi->uport != NULL));
}

void bluesleep_sleep_wakeup(void)
{
	if(DBG) printk("bluesleep_sleep_wakeup\n");

	if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
	if(DBG) printk("host_wake %d\n", gpio_get_value(bsi->host_wake));
	if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
	if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));

	if (test_bit(BT_ASLEEP, &flags)) {
		if (DBG)  printk("bluetooth waking up...\n");
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);
		set_bit(BT_EXT_WAKE, &flags);
		if(DBG) printk("set_bit : BT_EXT_WAKE\n");
		clear_bit(BT_ASLEEP, &flags);
	}

/*	if (uart_clock_off == true) { // uart clock on after host_wake & bt_wake up
		printk("Incoming data, HS uart port not clock on, request clock on \n");
		msm_hs_request_clock_on(bsi->uport);
		wake_lock_timeout(&bsi->wake_lock, HZ);
		uart_clock_off = false;
	}
*/
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	if(DBG) printk("bluesleep_sleep_work\n");

	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			if(DBG) printk("already asleep\n");
			return;
		}
	if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
	if(DBG) printk("host_wake %d\n", gpio_get_value(bsi->host_wake));
	if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
	if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));

		if (msm_hs_tx_empty(bsi->uport) == TIOCSER_TEMT) {
			printk("bluetooth going to sleep...\n");
			set_bit(BT_ASLEEP, &flags);
			if(DBG) printk("set_bit : BT_ASLEEP\n");

			/*Deactivating UART */
			/* UART clk is not turned off immediately. Release
			 * wakelock after 500 ms.
			 */
			wake_lock_timeout(&bsi->wake_lock, HZ / 2);
		} else {
			if(DBG) printk("wait 500ms ...\n");

		  	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
			return;
		}
	} else if (!test_bit(BT_EXT_WAKE, &flags) && !test_bit(BT_ASLEEP, &flags)) {
		if (DBG)  printk("bluesleep can not sleep\n");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));

		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);

		set_bit(BT_EXT_WAKE, &flags);
		if(DBG) printk("set_bit : BT_EXT_WAKE\n");
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
	printk("hostwake line change\n");
	if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
	if(DBG) printk("host_wake %d\n", gpio_get_value(bsi->host_wake));
	if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
	if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));

	spin_lock(&rw_lock);

	if ((gpio_get_value(bsi->host_wake) == bsi->irq_polarity)){ // bt_wake_up_host 
		bluesleep_rx_busy();
	} else
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

	if(DBG) printk("bluesleep_outgoing_data\n");

	spin_lock_irqsave(&rw_lock, irq_flags);

	/* log data passing by */
	set_bit(BT_TXDATA, &flags);
	if(DBG) printk("set_bit : BT_TXDATA\n");

	/* if the tx side is sleeping... */
	if (!test_bit(BT_EXT_WAKE, &flags)) {

	    printk("tx was sleeping\n");
	    bluesleep_sleep_wakeup();
	}

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

static struct uart_port *bluesleep_get_uart_port(void)
{
	if(DBG) printk("bluesleep_get_uart_port\n");
	if (bluesleep_uart_port != NULL)
	    return bluesleep_uart_port;
	else 
	    return NULL;
}

static ssize_t bluesleep_read_proc_lpm(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	return sprintf(buff, "unsupported to read\n");
}

static ssize_t bluesleep_write_proc_lpm(struct file *filp, const char __user *buff, size_t count, loff_t *pos)
{
	char b;
	if(DBG) printk("bluesleep_write_proc_lpm\n");

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buff, 1))
		return -EFAULT;

	if(DBG) printk("write proc lpm value is %c\n", b);

	if (b == '0') {
		/* HCI_DEV_UNREG */
		if(DBG) printk("write proc lpm value to 0 \n");
		bluesleep_stop();
		has_lpm_enabled = false;
		bsi->uport = NULL;
	} else {
		/* HCI_DEV_REG */
		if(DBG) printk("write proc lpm value to 1 \n");
		if (!has_lpm_enabled) {
			has_lpm_enabled = true;
			if(DBG) printk("has_lpm_enable set to true \n");
			bsi->uport = bluesleep_get_uart_port();
			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
	}
	return count;
}

static ssize_t bluesleep_read_proc_btwrite(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	if(DBG) printk("bluesleep_read_proc_btwrite\n");

	return sprintf(buff, "unsupported to read\n");
}

static ssize_t bluesleep_write_proc_btwrite(struct file *filp, const char __user *buff, size_t count, loff_t *pos)
{
	char b;
	if(DBG) printk("bluesleep_write_proc_btwrite\n");

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buff, 1))
		return -EFAULT;

	if(DBG) printk("write proc btwrite value is %c \n", b);

	/* HCI_DEV_WRITE */
	if (b != '0') {
		bluesleep_outgoing_data();
	}
	return count;
}
/**
 * Handles HCI device events.
 * @param this Not used.
 * @param event The event that occurred.
 * @param data The HCI device associated with the event.
 * @return <code>NOTIFY_DONE</code>.
 */
static int bluesleep_hci_event(struct notifier_block *this,
				unsigned long event, void *data)
{
	struct hci_dev *hdev = (struct hci_dev *) data;
	struct hci_uart *hu;
	struct uart_state *state;

	if (!hdev)
		return NOTIFY_DONE;

	if(DBG) printk("bluesleep_hci_event\n");

	switch (event) {
	case HCI_DEV_REG:
		if(DBG) printk("bluesleep_hci_event : HCI_DEV_REG \n");

		if (!bluesleep_hdev) {
			bluesleep_hdev = hdev;
			hu  = (struct hci_uart *) hdev->driver_data;
			state = (struct uart_state *) hu->tty->driver_data;
			bsi->uport = state->uart_port;
			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
		break;
	case HCI_DEV_UNREG:
		/* if bluetooth stopped, stop bluesleep also */
		if(DBG) printk("bluesleep_hci_event : HCI_DEV_UNREG \n");

		bluesleep_stop();
		bluesleep_hdev = NULL;
		bsi->uport = NULL;
		break;
	case HCI_DEV_WRITE:
		if(DBG) printk("bluesleep_hci_event : HCI_DEV_WRITE \n");

		bluesleep_outgoing_data();
		break;
	}

	return NOTIFY_DONE;
}

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if(DBG) printk("Tx timer expired\n");

	/* were we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &flags)) {
		if(DBG) printk("Tx has been idle\n");
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);

		if(DBG) printk("gpio_get_value(bsi->ext_wake)\n");
		clear_bit(BT_EXT_WAKE, &flags);
		bluesleep_tx_idle();
	} else {
		if(DBG) printk("Tx data during last period\n");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag */
	if(DBG) printk("clear_bit : BT_TXDATA\n");
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

	if (DBG) printk("bluesleep_start \n");

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

	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));

	/* assert BT_WAKE */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 1);

	set_bit(BT_EXT_WAKE, &flags);
	if(DBG) printk("set_bit : BT_EXT_WAKE\n");

#if BT_ENABLE_IRQ_WAKE
	retval = enable_irq_wake(bsi->host_wake_irq);
	if (retval < 0) {
		if(DBG) printk("Couldn't enable BT_HOST_WAKE as wakeup interrupt\n");
		goto fail;
	}
#endif

	set_bit(BT_PROTO, &flags);
	if(DBG) printk("set_bit : BT_PROTO\n");

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
	
	if(DBG) printk("blesleep_stop\n");

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (!test_bit(BT_PROTO, &flags)) {
		spin_unlock_irqrestore(&rw_lock, irq_flags);
		return;
	}

	/* assert BT_WAKE */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 1);

	set_bit(BT_EXT_WAKE, &flags);
	if(DBG) printk("set_bit : BT_EXT_WAKE\n");

	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
	if(DBG) printk("host_wake %d\n", gpio_get_value(bsi->host_wake));
	if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
	if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));
/*
	if (test_bit(BT_SUSPEND, &flags)) {
		clear_bit(BT_SUSPEND, &flags);
		msm_hs_request_clock_on(bsi->uport);
		wake_lock_timeout(&bsi->wake_lock, HZ);
	}
*/
	atomic_inc(&open_count);
	spin_unlock_irqrestore(&rw_lock, irq_flags);

#if BT_ENABLE_IRQ_WAKE
	if (disable_irq_wake(bsi->host_wake_irq))
	if(DBG) printk("Couldn't disable hostwake IRQ wakeup mode\n");
#endif
	if(DBG) printk("bluesleep_stop , wake_lock_timeout");
	wake_lock_timeout(&bsi->wake_lock, HZ / 2);
}

/**
 * Read the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the
 * pin is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_btwake(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	if(DBG) printk("bluepower_read_proc_btwake\n");
	return sprintf(buff, "btwake:%u\n", gpio_get_value(bsi->ext_wake));
}

/**
 * Write the <code>BT_WAKE</code> GPIO pin value via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluepower_write_proc_btwake(struct file *filp, const char __user *buff, size_t count, loff_t *pos)
{
	char *buf;

	if(DBG) printk("bluepower_write_proc_btwake\n");

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buff, count)) {
		kfree(buf);
		return -EFAULT;
	}

	if (buf[0] == '0') {
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);
		clear_bit(BT_EXT_WAKE, &flags);
		if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
		if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
		if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));

	} else if (buf[0] == '1') {
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);
			set_bit(BT_EXT_WAKE, &flags);
			if(DBG) printk("set_bit : BT_EXT_WAKE\n");
		if(DBG) printk("ext_wake %d\n", gpio_get_value(bsi->ext_wake));
		if(DBG) printk("gpio_host_wake_up_bt %d\n", gpio_get_value(gpio_host_wake_up_bt));
		if(DBG) printk("gpio_bt_wake_up_host %d\n", gpio_get_value(gpio_bt_wake_up_host));
	} else {
		kfree(buf);
		return -EINVAL;
	}

	kfree(buf);
	return count;
}

/**
 * Read the <code>BT_HOST_WAKE</code> GPIO pin value via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the pin
 * is high, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluepower_read_proc_hostwake(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	if(DBG) printk("bluepower_read_proc_hostwake\n");
	return sprintf(buff, "hostwake: %u \n", gpio_get_value(bsi->host_wake));
}


/**
 * Read the low-power status of the Host via the proc interface.
 * When this function returns, <code>page</code> contains a 1 if the Host
 * is asleep, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_asleep(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	unsigned int asleep;
	if(DBG) printk("bluesleep_read_proc_asleep\n");
	asleep = test_bit(BT_ASLEEP, &flags) ? 1 : 0;
	return sprintf(buff, "asleep: %u\n", asleep);
}

/**
 * Read the low-power protocol being used by the Host via the proc interface.
 * When this function returns, <code>page</code> will contain a 1 if the Host
 * is using the Sleep Mode Protocol, 0 otherwise.
 * @param page Buffer for writing data.
 * @param start Not used.
 * @param offset Not used.
 * @param count Not used.
 * @param eof Whether or not there is more data to be read.
 * @param data Not used.
 * @return The number of bytes written.
 */
static int bluesleep_read_proc_proto(struct file *filp, char __user *buff, size_t count, loff_t *pos)
{
	unsigned int proto;
	if(DBG) printk("bluesleep_read_proc_proto");
	proto = test_bit(BT_PROTO, &flags) ? 1 : 0;
	return sprintf(buff, "proto: %u\n", proto);
}

/**
 * Modify the low-power protocol used by the Host via the proc interface.
 * @param file Not used.
 * @param buffer The buffer to read from.
 * @param count The number of bytes to be written.
 * @param data Not used.
 * @return On success, the number of bytes written. On error, -1, and
 * <code>errno</code> is set appropriately.
 */
static int bluesleep_write_proc_proto(struct file *filp, const char __user *buff, size_t count, loff_t *pos)
{
	char proto;

	if(DBG) printk("bluesleep_write_proc_proto\n");

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&proto, buff, 1))
		return -EFAULT;

	if (proto == '0')
		bluesleep_stop();
	else
		bluesleep_start();

	/* claim that we wrote everything */
	return count;
}

void bluesleep_setup_uart_port(struct uart_port *uport)
{
	if(DBG) printk("bluesleep_setup_uart_port\n");
	if (uport != NULL);
		if(DBG) printk("uport != null \n");

	bluesleep_uart_port = uport;
}

static int __init bluesleep_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	if(DBG) printk("bluesleep_probe\n");

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		if(DBG) printk("couldn't find host_wake gpio\n");
		ret = -ENODEV;
		goto free_bsi;
	}
	bsi->host_wake = res->start;

	ret = gpio_request(bsi->host_wake, "bt_host_wake");

	if (ret)
		goto free_bsi;
	
	/* configure host_wake as input */
	ret = gpio_direction_input(bsi->host_wake);
	printk("request bsi->host_wake gpio # %d \n", bsi->host_wake);

	if (ret < 0) {
		pr_err("gpio-keys: failed to configure input"
			" direction for GPIO %d, error %d\n",
			bsi->host_wake, ret);
		gpio_free(bsi->host_wake);
		goto free_bsi;
	}


	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res)
		bsi->has_ext_wake = 0;
	else
		bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake) {
		bsi->ext_wake = res->start;
		ret = gpio_request(bsi->ext_wake, "bt_ext_wake");
		if (DBG)  printk("request bsi->ext_wake gpio # %d \n", bsi->ext_wake);

		if (ret)
			goto free_bt_host_wake;
		/* configure ext_wake as output mode*/
		ret = gpio_direction_output(bsi->ext_wake, 1);
		if (ret < 0) {
				pr_err("gpio-keys: failed to configure output"
					" direction for GPIO %d, error %d\n",
						bsi->ext_wake, ret);
				gpio_free(bsi->ext_wake);
				goto free_bt_host_wake;
		}
	} else {
		set_bit(BT_EXT_WAKE, &flags);
		if(DBG) printk("set_bit : BT_EXT_WAKE\n");
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				"host_wake");
	if (!res) {
		if(DBG) printk("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_host_wake;
	}

	bsi->host_wake_irq = res->start;

	if (bsi->host_wake_irq < 0) {
		if(DBG) printk("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	if (res->flags & IORESOURCE_IRQ_LOWEDGE)
		bsi->irq_polarity = POLARITY_LOW;/*low edge (falling edge)*/
	else
		bsi->irq_polarity = POLARITY_HIGH;/*anything else*/

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");

	if(DBG) printk("bluesleep_probe , wake_lock_init\n");

	clear_bit(BT_SUSPEND, &flags);

	if (bsi->irq_polarity == POLARITY_LOW) {
		ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				"bluetooth hostwake", NULL);
	} else {
		ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_RISING,
				"bluetooth hostwake", NULL);
	}
	if (ret < 0) {
		if(DBG) printk("Couldn't acquire BT_HOST_WAKE IRQ");
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
	if(DBG) printk("bluesleep_remove , wake_lock_destroy");

	kfree(bsi);
	return 0;
}

static int bluesleep_resume(struct platform_device *pdev)
{
	if (test_bit(BT_SUSPEND, &flags)) {
		printk("bluesleep resuming...\n");

/*		if (bsi->uport != NULL && 
			(gpio_get_value(bsi->host_wake) == bsi->irq_polarity)) { // incoming data request , request uart clock on
			msm_hs_request_clock_on(bsi->uport);
			printk("Incoming data, request HS uart port clock on \n");
			wake_lock_timeout(&bsi->wake_lock, HZ);
			uart_clock_off = false;
		}
*/
		clear_bit(BT_SUSPEND, &flags); 
	}
	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("bluesleep suspending...\n");
	if (test_bit(BT_ASLEEP, &flags)) { //allow going to suspend only when BT chip is in asleep state
		set_bit(BT_SUSPEND, &flags);
		if(DBG) printk("bt chip is asleep , set_bit : BT_SUSPEND\n");
/*		if(DBG) printk("Request HS uart port clock off \n");
		msm_hs_request_clock_off(bsi->uport);
		uart_clock_off = true;
*/
	} 

	return 0;
}


static struct platform_driver bluesleep_driver = {
	.remove = bluesleep_remove,
	.suspend = bluesleep_suspend,
	.resume = bluesleep_resume,
	.driver = {
		.name = "bluesleep",
		.owner = THIS_MODULE,
	},
};
/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */

static const struct file_operations btwake_ops = {
	.read		= bluepower_read_proc_btwake,
	.write		= bluepower_write_proc_btwake,
};

static const struct file_operations proto_ops = {
	.read		= bluesleep_read_proc_proto,
	.write		= bluesleep_write_proc_proto,
};

static const struct file_operations hostwake_ops = {
	.read		= bluepower_read_proc_hostwake,
};

static const struct file_operations asleep_ops = {
	.read		= bluesleep_read_proc_asleep,
};

static const struct file_operations lpm_ops = {
	.read		= bluesleep_read_proc_lpm,
	.write		= bluesleep_write_proc_lpm,
};

static const struct file_operations btwrite_ops = {
	.read		= bluesleep_read_proc_btwrite,
	.write		= bluesleep_write_proc_btwrite,
};

static int bluesleep_create_proc(void)
{
	struct proc_dir_entry *ent;
	int retval;

	bluetooth_dir = proc_mkdir("bluetooth", NULL);
	if (bluetooth_dir == NULL) {
		if(DBG) printk("Unable to create /proc/bluetooth directory");
		return -ENOMEM;
	}

	sleep_dir = proc_mkdir("sleep", bluetooth_dir);
	if (sleep_dir == NULL) {
		if(DBG) printk("Unable to create /proc/%s directory", PROC_DIR);
		return -ENOMEM;
	}

	/* Creating read/write "btwake" entry */
	ent = proc_create("btwake", BT_PROC_FILE_PERMISSION, sleep_dir,&btwake_ops);
	if (ent == NULL) {
		if(DBG) printk("Unable to create /proc/%s/btwake entry", PROC_DIR);
		retval = ENOMEM;
	}

	/* read only proc entries */
	if (proc_create("hostwake", BT_PROC_FILE_PERMISSION, sleep_dir,&hostwake_ops) == NULL) {
		if(DBG) printk("Unable to create /proc/%s/hostwake entry", PROC_DIR);
		retval = ENOMEM;
	}

	/* read/write proc entries */
	ent = proc_create("proto", BT_PROC_FILE_PERMISSION, sleep_dir,&proto_ops);
	if (ent == NULL) {
		if(DBG) printk("Unable to create /proc/%s/proto entry", PROC_DIR);
		retval = ENOMEM;
	}

	/* read only proc entries */
	if (proc_create("asleep", BT_PROC_FILE_PERMISSION,sleep_dir, &asleep_ops) == NULL) {
		if(DBG) printk("Unable to create /proc/%s/asleep entry", PROC_DIR);
		retval = ENOMEM;
	}

	/* read/write proc entries */
	ent = proc_create("lpm", BT_PROC_FILE_PERMISSION, sleep_dir,&lpm_ops);
	if (ent == NULL) {
		if(DBG) printk("Unable to create /proc/%s/lpm entry", PROC_DIR);
		retval = -ENOMEM;
	}

	/* read/write proc entries */
	ent = proc_create("btwrite", BT_PROC_FILE_PERMISSION, sleep_dir, &btwrite_ops);
	if (ent == NULL) {
		if(DBG) printk("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		retval = -ENOMEM;
	}


	if (retval < 0)
		return retval;
	else
		return 0; 
}

static int __init bluesleep_init(void)
{
	int retval;

	if(DBG) printk("MSM Sleep Mode Driver Ver %s\n", VERSION);

	retval = platform_driver_probe(&bluesleep_driver, bluesleep_probe);
	if (retval)
		return retval;
	if (bsi == NULL)
		return 0;

	bluesleep_hdev = NULL;
	
	if( bluesleep_create_proc() < 0)
		goto fail;

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	/* assert bt wake */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 1);
	set_bit(BT_EXT_WAKE, &flags);
	if(DBG) printk("set_bit: BT_EXT_WAKE\n");

	hci_register_notifier(&hci_event_nblock);

	return 0;

fail:
	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
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
		gpio_set_value(bsi->ext_wake, 1);
	set_bit(BT_EXT_WAKE, &flags);
	if(DBG) printk("set_bit: BT_EXT_WAKE\n");

	if (test_bit(BT_PROTO, &flags)) {
		if (disable_irq_wake(bsi->host_wake_irq))
			if(DBG) printk("Couldn't disable hostwake IRQ wakeup mode\n");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&tx_timer);
/*
		if (test_bit(BT_ASLEEP, &flags)) {
			msm_hs_request_clock_on(bsi->uport);
			wake_lock_timeout(&bsi->wake_lock, HZ);
		}
*/
	}

	hci_unregister_notifier(&hci_event_nblock);
	platform_driver_unregister(&bluesleep_driver);

	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
