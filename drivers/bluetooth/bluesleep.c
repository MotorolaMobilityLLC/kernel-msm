/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * Copyright (C) 2006-2007 - Motorola
 * Copyright (c) 2008-2010, The Linux Foundation. All rights reserved.
 *
 *  Date         Author           Comment
 * -----------  --------------   --------------------------------
 * 2006-Apr-28  Motorola         The kernel module for running the Bluetooth(R)
 *                               Sleep-Mode Protocol from the Host side
 * 2006-Sep-08  Motorola         Added workqueue for handling sleep work.
 * 2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.
 * 2009-Aug-10  Motorola         Changed "add_timer" to "mod_timer" to solve
 *                               race when flurry of queued work comes in.
*/

#include <linux/module.h>       /* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
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
#include <linux/serial_core.h>
#include <linux/platform_data/msm_serial_hs.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"


#undef BT_ERR
#define BT_ERR(fmt, arg...) pr_err(fmt " [BT]\n", ##arg)

/*
 * Defines
 */

#define VERSION	 "1.1"
#define PROC_DIR	"bluetooth/sleep"

#define POLARITY_LOW 0
#define POLARITY_HIGH 1

enum msm_hs_clk_states_e {
	MSM_HS_CLK_PORT_OFF,     /* port not in use */
	MSM_HS_CLK_OFF,          /* clock disabled */
	MSM_HS_CLK_REQUEST_OFF,  /* disable after TX and RX flushed */
	MSM_HS_CLK_ON,           /* clock enabled */
};

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
static void bluesleep_uart_awake_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);
DECLARE_DELAYED_WORK(uart_awake_wq, bluesleep_uart_awake_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle() schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_uart_work() schedule_delayed_work(&uart_awake_wq, 0)

/* 0.5 second timeout */
#define TX_TIMER_INTERVAL	1

/* state variable names and bit positions */
#define BT_PROTO	 0x01
#define BT_TXDATA	 0x02
#define BT_ASLEEP	 0x04
#define BT_EXT_WAKE	0x08
#define BT_SUSPEND	0x10

static bool bt_enabled;

static struct platform_device *bluesleep_uart_dev;
static struct bluesleep_info *bsi;

/*
 * Local function prototypes
 */

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
struct mutex bluesleep_mutex;

struct proc_dir_entry *bluetooth_dir, *sleep_dir;

/*
 * Local functions
 */

static int bluesleep_get_uart_state(void)
{
	int state = 0;

	state = msm_hs_get_clock_state(bsi->uport); 
	return state;
}

static void bluesleep_uart_awake_work(struct work_struct *work)
{
	int clk_state;

	if (!bsi->uport) {
		BT_DBG("hsuart_power called. But uport is null");
		return;
	}

	clk_state = bluesleep_get_uart_state();
	if (clk_state == MSM_HS_CLK_OFF) {
		if(bt_enabled) {
			BT_DBG("bluesleep_uart_awake_work : hsuart_power on");
			msm_hs_request_clock_on(bsi->uport);
			msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
		}
	} else if (clk_state == MSM_HS_CLK_REQUEST_OFF) {
		bluesleep_uart_work();
	}
}

static void hsuart_power(int on)
{
	int clk_state;

	if (test_bit(BT_SUSPEND, &flags) && !on) {
		BT_DBG("hsuart_power OFF- it's suspend state. so return.");
		return;
	}

	if (!bsi->uport) {
		BT_DBG("hsuart_power called. But uport is null");
		return;
	}

	if (on) {
		if (test_bit(BT_TXDATA, &flags)) {
			BT_DBG("hsuart_power on");
			msm_hs_request_clock_on(bsi->uport);
			msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
			return;
		}

		clk_state = bluesleep_get_uart_state();
		if (clk_state == MSM_HS_CLK_REQUEST_OFF) {
			BT_DBG("hsuart_power wait");
			bluesleep_uart_work();
		} else {
			BT_DBG("hsuart_power on");
			msm_hs_request_clock_on(bsi->uport);
			msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
		}
	} else {
		BT_DBG("hsuart_power off");
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
	return (!gpio_get_value(bsi->host_wake) && (!gpio_get_value(bsi->ext_wake))&&
				(bsi->uport != NULL));
}

void bluesleep_sleep_wakeup(void)
{
	if (test_bit(BT_ASLEEP, &flags)) {
		BT_DBG("waking up...");
		/*Activating UART */
		hsuart_power(1);
		wake_lock(&bsi->wake_lock);
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);

		set_bit(BT_EXT_WAKE, &flags);
		clear_bit(BT_ASLEEP, &flags);
	} else {
		BT_DBG("bluesleep_sleep_wakeup : already wake up");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	}
}

static void bluesleep_tx_data_wakeup(void)
{
	if (test_bit(BT_ASLEEP, &flags)) {
		BT_DBG("waking up from BT Write...");

		wake_lock(&bsi->wake_lock);
		/* Start the timer */
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);

		set_bit(BT_EXT_WAKE, &flags);
		clear_bit(BT_ASLEEP, &flags);
	} else {
		BT_DBG("bluesleep_tx_data_wakeup : already wake up");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	}
}




/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	if (mutex_is_locked(&bluesleep_mutex))
		BT_DBG("Wait for mutex unlock in bluesleep_sleep_work");

	mutex_lock(&bluesleep_mutex);

	if (bluesleep_can_sleep()) {
		/* already asleep, this is an error case */
		if (test_bit(BT_ASLEEP, &flags)) {
			BT_DBG("already asleep");
			mutex_unlock(&bluesleep_mutex);
			return;
		}

		if (msm_hs_tx_empty(bsi->uport)) {
			if (test_bit(BT_TXDATA, &flags)) {
				BT_DBG("Wait until timer expires.");

				mod_timer(&tx_timer,
					jiffies + TX_TIMER_INTERVAL * HZ);
				mutex_unlock(&bluesleep_mutex);
				return;
			}

			BT_DBG("going to sleep...");

			set_bit(BT_ASLEEP, &flags);
			/*Deactivating UART */
			hsuart_power(0);

			/* Moved from Timer expired */
			if (bsi->has_ext_wake == 1)
				gpio_set_value(bsi->ext_wake, 0);
			clear_bit(BT_EXT_WAKE, &flags);

			/*Deactivating UART */
			/* UART clk is not turned off immediately. Release
			* wakelock after 500 ms.
			*/
			wake_lock_timeout(&bsi->wake_lock, HZ / 2);
			} else {
			BT_DBG("host can enter sleep but some tx remained.");

			mod_timer(&tx_timer, jiffies + TX_TIMER_INTERVAL * HZ);
			mutex_unlock(&bluesleep_mutex);
			return;
		}
	} else if (!test_bit(BT_EXT_WAKE, &flags)
			&& !test_bit(BT_ASLEEP, &flags)) {
		BT_DBG("host_wake high");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);

		set_bit(BT_EXT_WAKE, &flags);
	} else {
		bluesleep_sleep_wakeup();
	}
	mutex_unlock(&bluesleep_mutex);
}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
	BT_DBG("hostwake line change");

	if ((gpio_get_value(bsi->host_wake) == bsi->irq_polarity))
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();
}

/**
 * Handles proper timer action when outgoing data is delivered to the
 * HCI line discipline. Sets BT_TXDATA.
 */
static void bluesleep_outgoing_data(void)
{
	if (!bt_enabled) {
		BT_ERR("bluesleep_outgoing_data(), BT is not enabled");
		return;
	}

	if (mutex_is_locked(&bluesleep_mutex))
		BT_DBG("Wait for mutex unlock in bluesleep_outgoing_data");

	mutex_lock(&bluesleep_mutex);
	/* log data passing by */
	set_bit(BT_TXDATA, &flags);

	BT_DBG("bluesleep_outgoing_data.");

	if (!test_bit(BT_EXT_WAKE, &flags))
		BT_DBG("BT_EXT_WAKE freed");

	if (!test_bit(BT_ASLEEP, &flags))
		BT_DBG("BT_ASLEEP freed");

	/*
	** Uart Clk should be enabled promptly
	** before bluedroid write TX data.
	*/
	hsuart_power(1);

	bluesleep_tx_data_wakeup();

	mutex_unlock(&bluesleep_mutex);
}

/**
 * Starts the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_start(void)
{
	int retval;

	if (test_bit(BT_PROTO, &flags))
		return;

	/* start the timer */
	mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));

	/* assert BT_WAKE */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 1);

	set_bit(BT_EXT_WAKE, &flags);
	retval = enable_irq_wake(bsi->host_wake_irq);
	if (retval < 0) {
		BT_ERR("Couldn't enable BT_HOST_WAKE as wakeup interrupt");
		goto fail;
	}
	set_bit(BT_PROTO, &flags);
	wake_lock(&bsi->wake_lock);
	return;
fail:
	del_timer(&tx_timer);

	return;
}

static void bluesleep_abnormal_stop(void)
{
	BT_ERR("bluesleep_abnormal_stop");

	if (!test_bit(BT_PROTO, &flags)) {
		BT_ERR("(bluesleep_abnormal_stop) proto is not set.");
		bsi->uport = NULL;
		return;
	}

	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if (disable_irq_wake(bsi->host_wake_irq))
		BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");

	wake_lock_timeout(&bsi->wake_lock, HZ / 2);

	clear_bit(BT_TXDATA, &flags);
	bsi->uport = NULL;
}

/**
 * Stops the Sleep-Mode Protocol on the Host.
 */
static void bluesleep_stop(void)
{
	if (!test_bit(BT_PROTO, &flags)) {
		BT_ERR("(bluesleep_stop_wq) proto is not set.");
		bsi->uport = NULL;
		return;
	}
	/* assert BT_WAKE */
	if (bsi->has_ext_wake == 1)
		gpio_set_value(bsi->ext_wake, 1);

	set_bit(BT_EXT_WAKE, &flags);
	del_timer(&tx_timer);
	clear_bit(BT_PROTO, &flags);

	if (test_bit(BT_ASLEEP, &flags)) {
		clear_bit(BT_ASLEEP, &flags);
		hsuart_power(1);
	}

	if (disable_irq_wake(bsi->host_wake_irq))
		BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");

	wake_lock_timeout(&bsi->wake_lock, HZ / 2);

	bsi->uport = NULL;
}

struct uart_port *bluesleep_get_uart_port(void)
{
	struct uart_port *uport = NULL;

	uport = msm_hs_get_uart_port(0);

	return uport;
}

static ssize_t bluesleep_read_proc_lpm
				(struct file *file,
				char __user *userbuf,
				size_t bytes,
				loff_t *off)
{
	return 0;
}

static ssize_t bluesleep_write_proc_lpm
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	char b;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&b, buffer, 1))
		return -EFAULT;

	if (b == '0') {
		BT_ERR("(bluesleep_write_proc_lpm) Unreg HCI notifier.");
		/* HCI_DEV_UNREG */
		bt_enabled = false;
		bluesleep_stop();
	} else if (b == '1') {
		BT_ERR("(bluesleep_write_proc_lpm) Reg HCI notifier.");
		/* HCI_DEV_REG */
		if (!bt_enabled) {
			bt_enabled = true;
			bsi->uport = bluesleep_get_uart_port();
			/* if bluetooth started, start bluesleep*/
			bluesleep_start();
		}
	} else if (b == '2') {
		BT_ERR("don`t control ext_wake & uart clk");
		if (bt_enabled) {
			bt_enabled = false;
			bluesleep_abnormal_stop();
		}
	}

	return count;
}

static ssize_t bluesleep_read_proc_btwrite
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{
	return 0;
}

static ssize_t bluesleep_write_proc_btwrite
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
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

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
	/* were we silent during the last timeout? */
	if (!test_bit(BT_TXDATA, &flags)) {
		BT_DBG("Tx has been idle");

		bluesleep_tx_idle();
	} else {
		BT_DBG("Tx data during last period");
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL*HZ));
	}

	/* clear the incoming data flag */
	clear_bit(BT_TXDATA, &flags);
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
static ssize_t bluepower_read_proc_btwake
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{

	return sprintf(userbuf, "ext_wake: %u \n", gpio_get_value(bsi->ext_wake));
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
static ssize_t bluepower_write_proc_btwake
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	char *buf;

	if (count < 1)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}
	if (buf[0] == '0') {
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 0);

		clear_bit(BT_EXT_WAKE, &flags);
		BT_DBG("bluepower_write_proc_btwake 0...");
	} else if (buf[0] == '1') {
		if (bsi->has_ext_wake == 1)
			gpio_set_value(bsi->ext_wake, 1);

		set_bit(BT_EXT_WAKE, &flags);
		BT_DBG("bluepower_write_proc_btwake 1...");
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
static ssize_t bluepower_read_proc_hostwake
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{

	return sprintf(userbuf, "hostwake: %u \n", gpio_get_value(bsi->host_wake));
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
static ssize_t bluesleep_read_proc_asleep
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{
	unsigned int asleep;	
	asleep = test_bit(BT_ASLEEP, &flags) ? 1 : 0;	
	return sprintf(userbuf, "asleep: %u\n", asleep);
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
static ssize_t bluesleep_read_proc_proto
				(struct file *file, char __user *userbuf,
				size_t bytes, loff_t *off)
{
	return 0;
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
static ssize_t bluesleep_write_proc_proto
				(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos)
{
	char proto;

	if (count < 1)
		return -EINVAL;

	if (copy_from_user(&proto, buffer, 1))
		return -EFAULT;

	if (proto == '0')
		bluesleep_stop();
	else
		bluesleep_start();

	/* claim that we wrote everything */
	return count;
}

void bluesleep_setup_uart_port(struct platform_device *uart_dev)
{
	bluesleep_uart_dev = uart_dev;
}

static int bluesleep_probe(struct platform_device *pdev)
{
	int ret;

	BT_ERR("bluesleep probe\n");

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi) {
		BT_ERR("failed to allocate memory to bsi\n");
		return -ENOMEM;
	}

	bsi->host_wake = 64;

	ret = gpio_request(bsi->host_wake, "bt_host_wake");
	if (ret) {
		BT_ERR("bluesleep_probe gpio_request for host_wake is failed");
		goto free_bsi;
	}

	/* configure host_wake as input */
	BT_ERR("configure input direction\n");
	ret = gpio_direction_input(bsi->host_wake);
	if (ret < 0) {
		BT_ERR("failed to configure ID for GPIO %d, error %d",
				bsi->host_wake, ret);
		gpio_free(bsi->host_wake);
		goto free_bsi;
	}
	bsi->has_ext_wake = 1;

	if (bsi->has_ext_wake) {
		bsi->ext_wake = 63;

		/* configure ext_wake as output mode*/
		ret = gpio_direction_output(bsi->ext_wake, 0);
		if (ret < 0) {
			BT_ERR("failed to configure OD for GPIO %d, error %d",
				  bsi->ext_wake, ret);
			gpio_free(bsi->ext_wake);
			goto free_bt_host_wake;
		}
	} else
		set_bit(BT_EXT_WAKE, &flags);

	bsi->host_wake_irq = gpio_to_irq(64);
	if (bsi->host_wake_irq < 0) {
		BT_ERR("couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}
	bsi->irq_polarity = POLARITY_HIGH;

	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND, "bluesleep");
	clear_bit(BT_SUSPEND, &flags);

	if (bsi->irq_polarity == POLARITY_LOW) {
		ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				"bluetooth_hostwake", NULL);
	} else {
		ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_RISING,
				"bluetooth_hostwake", NULL);
	}
	if (ret  < 0) {
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
	bsi = NULL;
	return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	free_irq(bsi->host_wake_irq, NULL);
	gpio_free(bsi->host_wake);
	gpio_free(bsi->ext_wake);
	wake_lock_destroy(&bsi->wake_lock);
	kfree(bsi);
	bsi = NULL;
	return 0;
}


static int bluesleep_resume(struct platform_device *pdev)
{
	if (test_bit(BT_SUSPEND, &flags)) {
		if (!bt_enabled) {
			gpio_tlmm_config(GPIO_CFG(bsi->host_wake, 0,
				GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		}
		if ((bsi->uport != NULL) &&
			(gpio_get_value(bsi->host_wake) == bsi->irq_polarity)) {
				BT_DBG("bluesleep resume form BT event...");
				hsuart_power(1);
		}
		clear_bit(BT_SUSPEND, &flags);
	}
	return 0;
}

static int bluesleep_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (!bt_enabled) {
		gpio_tlmm_config(GPIO_CFG(bsi->host_wake, 0, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA),
					GPIO_CFG_ENABLE);
	}
	set_bit(BT_SUSPEND, &flags);
	return 0;
}

static struct of_device_id bluesleep_match_table[] = {
	{ .compatible = "qcom,bluesleep" },	
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

static const struct file_operations proc_fops_btwake = {
	.owner = THIS_MODULE,
	.read = bluepower_read_proc_btwake,
	.write = bluepower_write_proc_btwake,
};
static const struct file_operations proc_fops_hostwake = {
	.owner = THIS_MODULE,
	.read = bluepower_read_proc_hostwake,
};
static const struct file_operations proc_fops_proto = {
	.owner = THIS_MODULE,
	.read = bluesleep_read_proc_proto,
	.write = bluesleep_write_proc_proto,
};
static const struct file_operations proc_fops_asleep = {
	.owner = THIS_MODULE,
	.read = bluesleep_read_proc_asleep,
};
static const struct file_operations proc_fops_lpm = {
	.owner = THIS_MODULE,
	.read = bluesleep_read_proc_lpm,
	.write = bluesleep_write_proc_lpm,
};
static const struct file_operations proc_fops_btwrite = {
	.owner = THIS_MODULE,
	.read = bluesleep_read_proc_btwrite,
	.write = bluesleep_write_proc_btwrite,
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

	bt_enabled = false;

	retval = platform_driver_register(&bluesleep_driver);

	if (retval)
		return retval;

	if (bsi == NULL) {
		BT_ERR("bluesleep_init failed. bsi is NULL!!");
		return -ENOMEM;
	}

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

	/* Creating read/write "btwake" entry */
	ent = proc_create("btwake", 0660, sleep_dir, &proc_fops_btwake);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read only proc entries */
	if (proc_create("hostwake", 0660, sleep_dir,
			&proc_fops_hostwake) == NULL) {
		BT_ERR("Unable to create /proc/%s/hostwake entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create("proto", 0660, sleep_dir, &proc_fops_proto);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/proto entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read only proc entries */
	if (proc_create("asleep", 0660, sleep_dir, &proc_fops_asleep) == NULL) {
		BT_ERR("Unable to create /proc/%s/asleep entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create("lpm", 0660, sleep_dir, &proc_fops_lpm);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/lpm entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	/* read/write proc entries */
	ent = proc_create("btwrite", 0660, sleep_dir, &proc_fops_btwrite);
	if (ent == NULL) {
		BT_ERR("Unable to create /proc/%s/btwrite entry", PROC_DIR);
		retval = -ENOMEM;
		goto fail;
	}

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	mutex_init(&bluesleep_mutex);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	set_bit(BT_EXT_WAKE, &flags);

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
	if (test_bit(BT_PROTO, &flags)) {
		if (disable_irq_wake(bsi->host_wake_irq))
			BT_ERR("Couldn't disable hostwake IRQ wakeup mode\n");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&tx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(1);
	}

	platform_driver_unregister(&bluesleep_driver);

	remove_proc_entry("btwrite", sleep_dir);
	remove_proc_entry("lpm", sleep_dir);
	remove_proc_entry("asleep", sleep_dir);
	remove_proc_entry("proto", sleep_dir);
	remove_proc_entry("hostwake", sleep_dir);
	remove_proc_entry("btwake", sleep_dir);
	remove_proc_entry("sleep", bluetooth_dir);
	remove_proc_entry("bluetooth", 0);

	mutex_destroy(&bluesleep_mutex);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
