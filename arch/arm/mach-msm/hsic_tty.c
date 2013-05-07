/* arch/arm/mach-msm/hsic_tty.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/sched.h>

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <mach/usb_bridge.h>

#define MAX_HSIC_TTYS 2
#define MAX_TTY_BUF_SIZE 2048

static DEFINE_MUTEX(hsic_tty_lock);

static uint hsic_tty_modem_wait = 60;
module_param_named(modem_wait, hsic_tty_modem_wait,
		uint, S_IRUGO | S_IWUSR | S_IWGRP);

static uint lge_ds_modem_wait = 20;
module_param_named(ds_modem_wait, lge_ds_modem_wait,
		uint, S_IRUGO | S_IWUSR | S_IWGRP);

#define DATA_BRIDGE_NAME_MAX_LEN		20

#define HSIC_TTY_DATA_RMNET_RX_Q_SIZE		50
#define HSIC_TTY_DATA_RMNET_TX_Q_SIZE		300
#define HSIC_TTY_DATA_SERIAL_RX_Q_SIZE		2
#define HSIC_TTY_DATA_SERIAL_TX_Q_SIZE		2
#define HSIC_TTY_DATA_RX_REQ_SIZE		2048
#define HSIC_TTY_DATA_TX_INTR_THRESHOLD		20

#define CTRL_DTR (1 << 0)
#define CTRL_RTS (1 << 1)

static unsigned int hsic_tty_data_rmnet_tx_q_size =
	HSIC_TTY_DATA_RMNET_TX_Q_SIZE;
module_param(hsic_tty_data_rmnet_tx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_rmnet_rx_q_size =
	HSIC_TTY_DATA_RMNET_RX_Q_SIZE;
module_param(hsic_tty_data_rmnet_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_serial_tx_q_size =
	HSIC_TTY_DATA_SERIAL_TX_Q_SIZE;
module_param(hsic_tty_data_serial_tx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_serial_rx_q_size =
	HSIC_TTY_DATA_SERIAL_RX_Q_SIZE;
module_param(hsic_tty_data_serial_rx_q_size, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_rx_req_size = HSIC_TTY_DATA_RX_REQ_SIZE;
module_param(hsic_tty_data_rx_req_size, uint, S_IRUGO | S_IWUSR);

unsigned int hsic_tty_data_tx_intr_thld = HSIC_TTY_DATA_TX_INTR_THRESHOLD;
module_param(hsic_tty_data_tx_intr_thld, uint, S_IRUGO | S_IWUSR);

/*flow ctrl*/
#define HSIC_TTY_DATA_FLOW_CTRL_EN_THRESHOLD	500
#define HSIC_TTY_DATA_FLOW_CTRL_DISABLE		300
#define HSIC_TTY_DATA_FLOW_CTRL_SUPPORT		1
#define HSIC_TTY_DATA_PENDLIMIT_WITH_BRIDGE	500

static unsigned int hsic_tty_data_fctrl_support =
	HSIC_TTY_DATA_FLOW_CTRL_SUPPORT;
module_param(hsic_tty_data_fctrl_support, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_fctrl_en_thld =
	HSIC_TTY_DATA_FLOW_CTRL_EN_THRESHOLD;
module_param(hsic_tty_data_fctrl_en_thld, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_fctrl_dis_thld =
	HSIC_TTY_DATA_FLOW_CTRL_DISABLE;
module_param(hsic_tty_data_fctrl_dis_thld, uint, S_IRUGO | S_IWUSR);

static unsigned int hsic_tty_data_pend_limit_with_bridge =
	HSIC_TTY_DATA_PENDLIMIT_WITH_BRIDGE;
module_param(hsic_tty_data_pend_limit_with_bridge, uint, S_IRUGO | S_IWUSR);

#define CH_OPENED 0
#define CH_READY 1

static int hsic_tty_enabled;

struct hsic_tty_info {
	struct tty_struct *tty;
	struct wake_lock wake_lock;
	int open_count;
	struct timer_list buf_req_timer;
	struct completion ch_allocated;
	struct platform_driver driver;
	int in_reset;
	int in_reset_updated;
	int is_open;

	wait_queue_head_t ch_opened_wait_queue;
	spinlock_t reset_lock;
	struct hsic_config *hsic;

	/* gadget */
	atomic_t connected;

	/* data transfer queues */
	unsigned int tx_q_size;
	struct list_head tx_idle;
	struct sk_buff_head tx_skb_q;
	spinlock_t tx_lock;

	unsigned int rx_q_size;
	struct list_head rx_idle;
	struct sk_buff_head rx_skb_q;
	spinlock_t rx_lock;

	/* work */
	struct workqueue_struct *wq;
	struct work_struct connect_w;
	struct work_struct disconnect_w;
	struct work_struct write_tomdm_w;
	struct work_struct write_tohost_w;

	struct bridge data_brdg;
	struct bridge ctrl_brdg;

	/*bridge status */
	unsigned long bridge_sts;

	/*counters */
	unsigned long to_modem;
	unsigned long to_host;
	unsigned int rx_throttled_cnt;
	unsigned int rx_unthrottled_cnt;
	unsigned int tx_throttled_cnt;
	unsigned int tx_unthrottled_cnt;
	unsigned int tomodem_drp_cnt;
	unsigned int unthrottled_pnd_skbs;
};

/**
 * HSIC port configuration.
 *
 * @tty_dev_index   Index into hsic_tty[]
 * @port_name       Name of the HSIC port
 * @dev_name        Name of the TTY Device (if NULL, @port_name is used)
 * @edge            HSIC edge
 */
struct hsic_config {
	uint32_t tty_dev_index;
	const char *port_name;
	const char *dev_name;
};

static struct hsic_config hsic_configs[] = {
	{0, "serial_hsic_data", NULL},
	//{1, "rmnet_data_hsic0", NULL},
};

static struct hsic_tty_info hsic_tty[MAX_HSIC_TTYS];

static int is_in_reset(struct hsic_tty_info *info)
{
	return info->in_reset;
}

static void buf_req_retry(unsigned long param)
{
	struct hsic_tty_info *info = (struct hsic_tty_info *)param;
	unsigned long flags;

	spin_lock_irqsave(&info->reset_lock, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock, flags);
		queue_work(info->wq, &info->write_tohost_w);
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);
}

static void hsic_tty_data_write_tohost(struct work_struct *w)
{
	struct hsic_tty_info *info =
		container_of(w, struct hsic_tty_info, write_tohost_w);
	struct tty_struct *tty = info->tty;
	struct sk_buff *skb;
	unsigned char *ptr;
	unsigned long flags;
	int avail;

	pr_debug("%s\n", __func__);

	if (!info)
		return;

	spin_lock_irqsave(&info->tx_lock, flags);
	for (;;) {
		if (is_in_reset(info)) {
			/* signal TTY clients using TTY_BREAK */
			tty_insert_flip_char(tty, 0x00, TTY_BREAK);
			tty_flip_buffer_push(tty);
			break;
		}

		skb = __skb_dequeue(&info->tx_skb_q);
		if (!skb)
			break;

		avail = skb->len;
		if (avail == 0)
			break;

		avail = tty_prepare_flip_string(tty, &ptr, avail);
		if (avail <= 0) {
			if (!timer_pending(&info->buf_req_timer)) {
				init_timer(&info->buf_req_timer);
				info->buf_req_timer.expires = jiffies +
				    ((30 * HZ) / 1000);
				info->buf_req_timer.function = buf_req_retry;
				info->buf_req_timer.data = (unsigned long)info;
				add_timer(&info->buf_req_timer);
			}
			spin_unlock_irqrestore(&info->tx_lock, flags);
			return;
		}

		memcpy(ptr, skb->data, avail);
		dev_kfree_skb_any(skb);

		wake_lock_timeout(&info->wake_lock, HZ / 2);
		tty_flip_buffer_push(tty);

		info->to_host++;
	}

	/* XXX only when writable and necessary */
	tty_wakeup(tty);
	spin_unlock_irqrestore(&info->tx_lock, flags);
}

static int hsic_tty_data_receive(void *p, void *data, size_t len)
{
	struct hsic_tty_info *info = p;
	unsigned long flags;
	struct sk_buff *skb = data;

	if (!info || !atomic_read(&info->connected)) {
		dev_kfree_skb_any(skb);
		return -ENOTCONN;
	}

	pr_debug("%s: p:%p#%d skb_len:%d\n", __func__,
		 info, info->tty->index, skb->len);

	spin_lock_irqsave(&info->tx_lock, flags);
	__skb_queue_tail(&info->tx_skb_q, skb);

	if (hsic_tty_data_fctrl_support &&
	    info->tx_skb_q.qlen >= hsic_tty_data_fctrl_en_thld) {
		set_bit(RX_THROTTLED, &info->data_brdg.flags);
		info->rx_throttled_cnt++;
		pr_debug("%s: flow ctrl enabled: tx skbq len: %u\n",
			 __func__, info->tx_skb_q.qlen);
		spin_unlock_irqrestore(&info->tx_lock, flags);
		queue_work(info->wq, &info->write_tohost_w);
		return -EBUSY;
	}

	spin_unlock_irqrestore(&info->tx_lock, flags);

	queue_work(info->wq, &info->write_tohost_w);

	return 0;
}

static void hsic_tty_data_write_tomdm(struct work_struct *w)
{
	struct hsic_tty_info *info =
		container_of(w, struct hsic_tty_info, write_tomdm_w);
	struct sk_buff *skb;
	unsigned long flags;
	int ret;

	pr_debug("%s\n", __func__);

	if (!info || !atomic_read(&info->connected))
		return;

	spin_lock_irqsave(&info->rx_lock, flags);
	if (test_bit(TX_THROTTLED, &info->data_brdg.flags)) {
		spin_unlock_irqrestore(&info->rx_lock, flags);
		return;
	}

	while ((skb = __skb_dequeue(&info->rx_skb_q))) {
		pr_debug("%s: info:%p tom:%lu pno:%d\n", __func__,
			 info, info->to_modem, info->tty->index);

		spin_unlock_irqrestore(&info->rx_lock, flags);
		ret = data_bridge_write(info->data_brdg.ch_id, skb);
		spin_lock_irqsave(&info->rx_lock, flags);
		if (ret < 0) {
			if (ret == -EBUSY) {
				/*flow control */
				info->tx_throttled_cnt++;
				break;
			}
			pr_err("%s: write error:%d\n", __func__, ret);
			info->tomodem_drp_cnt++;
			dev_kfree_skb_any(skb);
			break;
		}
		info->to_modem++;
	}
	spin_unlock_irqrestore(&info->rx_lock, flags);
}

static void hsic_tty_data_connect_w(struct work_struct *w)
{
	struct hsic_tty_info *info =
		container_of(w, struct hsic_tty_info, connect_w);
	unsigned long flags;
	int ret;

	pr_debug("%s\n", __func__);

	if (!info || !atomic_read(&info->connected) ||
			!test_bit(CH_READY, &info->bridge_sts))
		return;

	pr_debug("%s: info:%p\n", __func__, info);

	ret = ctrl_bridge_open(&info->ctrl_brdg);
	if (ret) {
		pr_err("%s: unable open ctrl bridge ch:%d err:%d\n",
				__func__, info->ctrl_brdg.ch_id, ret);
		return;
	}

	ret = data_bridge_open(&info->data_brdg);
	if (ret) {
		pr_err("%s: unable open data bridge ch:%d err:%d\n",
				__func__, info->data_brdg.ch_id, ret);
		return;
	}

	set_bit(CH_OPENED, &info->bridge_sts);
	ctrl_bridge_set_cbits(info->ctrl_brdg.ch_id, CTRL_RTS | CTRL_DTR);

	spin_lock_irqsave(&info->reset_lock, flags);
	info->in_reset = 0;
	info->in_reset_updated = 1;
	info->is_open = 1;
	wake_up_interruptible(&info->ch_opened_wait_queue);
	spin_unlock_irqrestore(&info->reset_lock, flags);
}

static void hsic_tty_data_disconnect_w(struct work_struct *w)
{
	struct hsic_tty_info *info =
		container_of(w, struct hsic_tty_info, connect_w);
	unsigned long flags;

	pr_debug("%s\n", __func__);

	if (!test_bit(CH_OPENED, &info->bridge_sts))
		return;

	ctrl_bridge_close(info->ctrl_brdg.ch_id);
	data_bridge_close(info->data_brdg.ch_id);
	clear_bit(CH_OPENED, &info->bridge_sts);

	spin_lock_irqsave(&info->reset_lock, flags);
	info->in_reset = 1;
	info->in_reset_updated = 1;
	info->is_open = 0;
	wake_up_interruptible(&info->ch_opened_wait_queue);
	spin_unlock_irqrestore(&info->reset_lock, flags);
	/* schedule task to send TTY_BREAK */
	queue_work(info->wq, &info->write_tohost_w);
}

static int hsic_tty_open(struct tty_struct *tty, struct file *f)
{
	int res = 0;
	unsigned int n = tty->index;
	struct hsic_tty_info *info;
	unsigned long flags;

	pr_debug("%s\n", __func__);

	if (n >= MAX_HSIC_TTYS || !hsic_tty[n].hsic)
		return -ENODEV;

	info = hsic_tty + n;

	mutex_lock(&hsic_tty_lock);
	tty->driver_data = info;

	if (info->open_count++ == 0) {
		/*
		 * Wait for a channel to be allocated so we know
		 * the modem is ready enough.
		 */
		if (hsic_tty_modem_wait) {
			res = try_wait_for_completion(&info->ch_allocated);

			if (res == 0) {
				pr_debug
				    ("%s: Timed out waiting for HSIC channel\n",
				     __func__);
				res = -ETIMEDOUT;
				goto out;
			} else if (res < 0) {
				pr_err
				    ("%s: Error waiting for HSIC channel: %d\n",
				     __func__, res);
				goto out;
			}
			pr_info("%s: opened %s\n", __func__,
				hsic_tty[n].hsic->port_name);

			res = 0;
		}

		info->tty = tty;
		wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND,
			       hsic_tty[n].hsic->port_name);
		if (!atomic_read(&info->connected)) {
			atomic_set(&info->connected, 1);

			spin_lock_irqsave(&info->tx_lock, flags);
			info->to_host = 0;
			info->rx_throttled_cnt = 0;
			info->rx_unthrottled_cnt = 0;
			info->unthrottled_pnd_skbs = 0;
			spin_unlock_irqrestore(&info->tx_lock, flags);

			spin_lock_irqsave(&info->rx_lock, flags);
			info->to_modem = 0;
			info->tomodem_drp_cnt = 0;
			info->tx_throttled_cnt = 0;
			info->tx_unthrottled_cnt = 0;
			spin_unlock_irqrestore(&info->rx_lock, flags);

			set_bit(CH_READY, &info->bridge_sts);

			queue_work(info->wq, &info->connect_w);

			res =
			    wait_event_interruptible_timeout(info->
							     ch_opened_wait_queue,
							     info->is_open,
							     (2 * HZ));
			if (res == 0)
				res = -ETIMEDOUT;
			if (res < 0) {
				pr_err("%s: wait for %s hsic_open failed %d\n",
				       __func__, hsic_tty[n].hsic->port_name,
				       res);
				goto out;
			}
			res = 0;
		}
	}

out:
	mutex_unlock(&hsic_tty_lock);

	return res;
}

static void hsic_tty_close(struct tty_struct *tty, struct file *f)
{
	struct hsic_tty_info *info = tty->driver_data;
	unsigned long flags;
	int res = 0;
	int n = tty->index;
	struct sk_buff *skb;

	pr_debug("%s\n", __func__);

	if (info == 0)
		return;

	mutex_lock(&hsic_tty_lock);
	if (--info->open_count == 0) {
		spin_lock_irqsave(&info->reset_lock, flags);
		info->is_open = 0;
		spin_unlock_irqrestore(&info->reset_lock, flags);
		if (info->tty) {
			wake_lock_destroy(&info->wake_lock);
			info->tty = 0;
		}
		tty->driver_data = 0;
		del_timer(&info->buf_req_timer);
		if (atomic_read(&info->connected)) {
			atomic_set(&info->connected, 0);

			spin_lock_irqsave(&info->tx_lock, flags);
			clear_bit(RX_THROTTLED, &info->data_brdg.flags);
			spin_unlock_irqrestore(&info->tx_lock, flags);

			spin_lock_irqsave(&info->rx_lock, flags);
			clear_bit(TX_THROTTLED, &info->data_brdg.flags);
			spin_unlock_irqrestore(&info->rx_lock, flags);

			queue_work(info->wq, &info->disconnect_w);

			pr_info("%s: waiting to close hsic %s completely\n",
				__func__, hsic_tty[n].hsic->port_name);
			/* wait for reopen ready status in seconds */
			res =
			    wait_event_interruptible_timeout(info->
							     ch_opened_wait_queue,
							     !info->is_open,
							     (lge_ds_modem_wait
							      * HZ));
			if (res == 0) {
				/* just in case, remain result value */
				res = -ETIMEDOUT;
				pr_err("%s: timeout to wait for %s hsic_close.\
                        next hsic_open may fail....%d\n", __func__, hsic_tty[n].hsic->port_name, res);
			}
			if (res < 0) {
				pr_err("%s: wait for %s hsic_close failed.\
                        next hsic_open may fail....%d\n", __func__, hsic_tty[n].hsic->port_name, res);
			}

			data_bridge_close(info->data_brdg.ch_id);

			clear_bit(CH_READY, &info->bridge_sts);
			clear_bit(CH_OPENED, &info->bridge_sts);

			spin_lock_irqsave(&info->tx_lock, flags);
			while ((skb = __skb_dequeue(&info->tx_skb_q)))
				dev_kfree_skb_any(skb);
			spin_unlock_irqrestore(&info->tx_lock, flags);

			spin_lock_irqsave(&info->rx_lock, flags);
			while ((skb = __skb_dequeue(&info->rx_skb_q)))
				dev_kfree_skb_any(skb);
			spin_unlock_irqrestore(&info->rx_lock, flags);
		}
	}
	mutex_unlock(&hsic_tty_lock);
}

static int hsic_tty_write(struct tty_struct *tty, const unsigned char *buf,
			  int len)
{
	struct hsic_tty_info *info = tty->driver_data;
	int avail;
	struct sk_buff *skb;

	pr_debug("%s\n", __func__);

	/* if we're writing to a packet channel we will
	 ** never be able to write more data than there
	 ** is currently space for
	 */
	if (is_in_reset(info))
		return -ENETRESET;

	avail = test_bit(CH_OPENED, &info->bridge_sts);
	/* if no space, we'll have to setup a notification later to wake up the
	 * tty framework when space becomes avaliable
	 */
	if (!avail)
		return 0;

	skb = alloc_skb(len, GFP_ATOMIC);
	memcpy(skb_put(skb, len), buf, len);

	spin_lock(&info->rx_lock);
	__skb_queue_tail(&info->rx_skb_q, skb);
	queue_work(info->wq, &info->write_tomdm_w);
	spin_unlock(&info->rx_lock);

	return len;
}

static int hsic_tty_write_room(struct tty_struct *tty)
{
	struct hsic_tty_info *info = tty->driver_data;
	return test_bit(CH_OPENED, &info->bridge_sts);
}

static int hsic_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct hsic_tty_info *info = tty->driver_data;
	return test_bit(CH_OPENED, &info->bridge_sts);
}

static void hsic_tty_unthrottle(struct tty_struct *tty)
{
	struct hsic_tty_info *info = tty->driver_data;
	unsigned long flags;

	pr_debug("%s\n", __func__);

	spin_lock_irqsave(&info->reset_lock, flags);
	if (info->is_open) {
		spin_unlock_irqrestore(&info->reset_lock, flags);
		if (hsic_tty_data_fctrl_support &&
		    info->tx_skb_q.qlen <= hsic_tty_data_fctrl_dis_thld &&
		    test_and_clear_bit(RX_THROTTLED, &info->data_brdg.flags)) {
			info->rx_unthrottled_cnt++;
			info->unthrottled_pnd_skbs = info->tx_skb_q.qlen;
			pr_debug("%s: disable flow ctrl:"
				 " tx skbq len: %u\n",
				 __func__, info->tx_skb_q.qlen);
			data_bridge_unthrottle_rx(info->data_brdg.ch_id);
			queue_work(info->wq, &info->write_tohost_w);
		}
		return;
	}
	spin_unlock_irqrestore(&info->reset_lock, flags);
}

static struct tty_operations hsic_tty_ops = {
	.open = hsic_tty_open,
	.close = hsic_tty_close,
	.write = hsic_tty_write,
	.write_room = hsic_tty_write_room,
	.chars_in_buffer = hsic_tty_chars_in_buffer,
	.unthrottle = hsic_tty_unthrottle,
};

static int hsic_tty_dummy_probe(struct platform_device *pdev)
{
	int n;
	int idx;

	for (n = 0; n < ARRAY_SIZE(hsic_configs); ++n) {
		idx = hsic_configs[n].tty_dev_index;

		if (!hsic_configs[n].dev_name)
			continue;

		if (/* pdev->id == hsic_configs[n].edge && */
		    !strncmp(pdev->name, hsic_configs[n].dev_name,
			    DATA_BRIDGE_NAME_MAX_LEN)) {
			complete_all(&hsic_tty[idx].ch_allocated);
			pr_info("%s: %s ch_allocated\n", __func__,
				hsic_configs[n].dev_name);
			return 0;
		}
	}
	pr_err("%s: unknown device '%s'\n", __func__, pdev->name);

	return -ENODEV;
}

static struct tty_driver *hsic_tty_driver;

static int hsic_tty_init(void)
{
	int ret;
	int n;
	int idx;

	pr_info("%s\n", __func__);

	hsic_tty_driver = alloc_tty_driver(MAX_HSIC_TTYS);
	if (hsic_tty_driver == 0)
		return -ENOMEM;

	hsic_tty_driver->owner = THIS_MODULE;
	hsic_tty_driver->driver_name = "hsic_tty_driver";
	hsic_tty_driver->name = "hsic";
	hsic_tty_driver->major = 0;
	hsic_tty_driver->minor_start = 0;
	hsic_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	hsic_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	hsic_tty_driver->init_termios = tty_std_termios;
	hsic_tty_driver->init_termios.c_iflag = 0;
	hsic_tty_driver->init_termios.c_oflag = 0;
	hsic_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD;
	hsic_tty_driver->init_termios.c_lflag = 0;
	hsic_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS |
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(hsic_tty_driver, &hsic_tty_ops);

	ret = tty_register_driver(hsic_tty_driver);
	if (ret) {
		put_tty_driver(hsic_tty_driver);
		pr_err("%s: driver registration failed %d\n", __func__, ret);
		return ret;
	}

	for (n = 0; n < ARRAY_SIZE(hsic_configs); ++n) {
		idx = hsic_configs[n].tty_dev_index;

		if (hsic_configs[n].dev_name == NULL)
			hsic_configs[n].dev_name = hsic_configs[n].port_name;

		tty_register_device(hsic_tty_driver, idx, 0);
		init_completion(&hsic_tty[idx].ch_allocated);

		hsic_tty[idx].wq =
		    create_singlethread_workqueue(hsic_configs[n].port_name);
		if (!hsic_tty[idx].wq) {
			pr_err("%s: Unable to create workqueue:%s\n",
			       __func__, hsic_configs[n].port_name);
			return -ENOMEM;
		}

		/* port initialization */
		spin_lock_init(&hsic_tty[idx].rx_lock);
		spin_lock_init(&hsic_tty[idx].tx_lock);

		INIT_WORK(&hsic_tty[idx].connect_w, hsic_tty_data_connect_w);
		INIT_WORK(&hsic_tty[idx].disconnect_w,
			  hsic_tty_data_disconnect_w);
		INIT_WORK(&hsic_tty[idx].write_tohost_w,
			  hsic_tty_data_write_tohost);
		INIT_WORK(&hsic_tty[idx].write_tomdm_w,
			  hsic_tty_data_write_tomdm);

		INIT_LIST_HEAD(&hsic_tty[idx].tx_idle);
		INIT_LIST_HEAD(&hsic_tty[idx].rx_idle);

		skb_queue_head_init(&hsic_tty[idx].tx_skb_q);
		skb_queue_head_init(&hsic_tty[idx].rx_skb_q);

		hsic_tty[idx].data_brdg.ch_id = idx;
		hsic_tty[idx].data_brdg.ctx = &hsic_tty[idx];
		hsic_tty[idx].data_brdg.name = "serial_hsic_data";
		hsic_tty[idx].data_brdg.ops.send_pkt = hsic_tty_data_receive;

		hsic_tty[idx].ctrl_brdg.ch_id = idx;
		hsic_tty[idx].ctrl_brdg.ctx = &hsic_tty[idx];
		hsic_tty[idx].ctrl_brdg.name = "serial_hsic_ctrl";

		hsic_tty[idx].driver.probe = hsic_tty_dummy_probe;
		hsic_tty[idx].driver.driver.name = hsic_configs[n].dev_name;
		hsic_tty[idx].driver.driver.owner = THIS_MODULE;
		spin_lock_init(&hsic_tty[idx].reset_lock);
		hsic_tty[idx].is_open = 0;
		init_waitqueue_head(&hsic_tty[idx].ch_opened_wait_queue);
		ret = platform_driver_register(&hsic_tty[idx].driver);

		if (ret) {
			pr_err("%s: init failed %d (%d)\n", __func__, idx, ret);
			hsic_tty[idx].driver.probe = NULL;
			goto out;
		}
		hsic_tty[idx].hsic = &hsic_configs[n];
	}
	return 0;

out:
	/* unregister platform devices */
	for (n = 0; n < ARRAY_SIZE(hsic_configs); ++n) {
		idx = hsic_configs[n].tty_dev_index;

		if (hsic_tty[idx].driver.probe) {
			pr_info("unregister hsic_tty driver %d\n", idx);
			platform_driver_unregister(&hsic_tty[idx].driver);
		}
		tty_unregister_device(hsic_tty_driver, idx);
	}

	tty_unregister_driver(hsic_tty_driver);
	put_tty_driver(hsic_tty_driver);
	hsic_tty_driver = NULL;
	return ret;
}

static void hsic_tty_release(void)
{
	int n, idx;

	pr_info("%s\n", __func__);

	if (hsic_tty_driver == NULL)
		return;

	for (n = 0; n < ARRAY_SIZE(hsic_configs); ++n) {
		idx = hsic_configs[n].tty_dev_index;
		if (hsic_tty[idx].driver.probe) {
			pr_info("unregister hsic_tty driver %d\n", idx);
			platform_driver_unregister(&hsic_tty[idx].driver);
		}
		tty_unregister_device(hsic_tty_driver, idx);
	}
	tty_unregister_driver(hsic_tty_driver);
	put_tty_driver(hsic_tty_driver);
	hsic_tty_driver = NULL;
}

static int hsic_tty_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = hsic_tty_enabled;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	if (hsic_tty_enabled >> 1) {
		pr_err("value is not zero or one\n");
		hsic_tty_enabled = old_val;
		return -EINVAL;
	}

	if (old_val == hsic_tty_enabled) {
		pr_err("value is the same as old one\n");
		return -EINVAL;
	}

	if (hsic_tty_enabled) {
		ret = hsic_tty_init();
		if (ret)
			hsic_tty_enabled = 0;
	} else {
		hsic_tty_release();
	}

	return ret;
}
module_param_call(enable, hsic_tty_set, param_get_int, &hsic_tty_enabled, 0644);

static int __init hsic_tty_module_init(void)
{
	hsic_tty_enabled = 0;
	return 0;
}

module_init(hsic_tty_module_init);
