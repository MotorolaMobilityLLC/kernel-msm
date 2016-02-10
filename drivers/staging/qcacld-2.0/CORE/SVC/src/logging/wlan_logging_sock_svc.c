/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/******************************************************************************
 * wlan_logging_sock_svc.c
 *
 ******************************************************************************/

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
#include <vmalloc.h>
#include <wlan_nlink_srv.h>
#include <vos_status.h>
#include <vos_trace.h>
#include <wlan_nlink_common.h>
#include <wlan_logging_sock_svc.h>
#include <vos_types.h>
#include <vos_trace.h>
#include <kthread.h>
#include <adf_os_time.h>
#include "pktlog_ac.h"

#define LOGGING_TRACE(level, args...) \
		VOS_TRACE(VOS_MODULE_ID_HDD, level, ## args)

/* Global variables */

#define ANI_NL_MSG_LOG_TYPE 89
#define ANI_NL_MSG_READY_IND_TYPE 90
#define MAX_LOGMSG_LENGTH 4096

#define HOST_LOG_DRIVER_MSG        0x001
#define HOST_LOG_PER_PKT_STATS     0x002
#define HOST_LOG_FW_FLUSH_COMPLETE 0x003

struct log_msg {
	struct list_head node;
	unsigned int radio;
	unsigned int index;
	/* indicates the current filled log length in logbuf */
	unsigned int filled_length;
	/*
	 * Buf to hold the log msg
	 * tAniHdr + log
	 */
	char logbuf[MAX_LOGMSG_LENGTH];
};

struct wlan_logging {
	/* Log Fatal and ERROR to console */
	bool log_fe_to_console;
	/* Number of buffers to be used for logging */
	int num_buf;
	/* Lock to synchronize access to shared logging resource */
	spinlock_t spin_lock;
	/* Holds the free node which can be used for filling logs */
	struct list_head free_list;
	/* Holds the filled nodes which needs to be indicated to APP */
	struct list_head filled_list;
	/* Wait queue for Logger thread */
	wait_queue_head_t wait_queue;
	/* Logger thread */
	struct task_struct *thread;
	/* Logging thread sets this variable on exit */
	struct completion   shutdown_comp;
	/* Indicates to logger thread to exit */
	bool exit;
	/* Holds number of dropped logs*/
	unsigned int drop_count;
	/* current logbuf to which the log will be filled to */
	struct log_msg *pcur_node;
	/* Event flag used for wakeup and post indication*/
	unsigned long eventFlag;
	/* Indicates logger thread is activated */
	bool is_active;
	/* Flush completion check */
	bool is_flush_complete;
};

static struct wlan_logging gwlan_logging;
static struct log_msg *gplog_msg;

/* PID of the APP to log the message */
static int gapp_pid = INVALID_PID;

/* Utility function to send a netlink message to an application
 * in user space
 */
static int wlan_send_sock_msg_to_app(tAniHdr *wmsg, int radio,
				int src_mod, int pid)
{
	int err = -1;
	int payload_len;
	int tot_msg_len;
	tAniNlHdr *wnl = NULL;
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	int wmsg_length = wmsg->length;
	static int nlmsg_seq;

	if (radio < 0 || radio > ANI_MAX_RADIOS) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
				"%s: invalid radio id [%d]",
				__func__, radio);
		return -EINVAL;
	}

	payload_len = wmsg_length + sizeof(wnl->radio);
	tot_msg_len = NLMSG_SPACE(payload_len);
	skb = dev_alloc_skb(tot_msg_len);
	if (skb == NULL) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
				"%s: dev_alloc_skb() failed for msg size[%d]",
				__func__, tot_msg_len);
		return -ENOMEM;
	}
	nlh = nlmsg_put(skb, pid, nlmsg_seq++, src_mod, payload_len,
		NLM_F_REQUEST);
	if (NULL == nlh) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
				"%s: nlmsg_put() failed for msg size[%d]",
				__func__, tot_msg_len);
		kfree_skb(skb);
		return -ENOMEM;
	}

	wnl = (tAniNlHdr *) nlh;
	wnl->radio = radio;
	memcpy(&wnl->wmsg, wmsg, wmsg_length);
	err = nl_srv_ucast(skb, pid, MSG_DONTWAIT);
	if (err) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_INFO,
				"%s: Failed sending Msg Type [0x%X] to pid[%d]\n",
				__func__, wmsg->type, pid);
	}

	return err;
}

/**
 * is_data_path_module() - To check for a Datapath module
 * @mod_id: Module id
 *
 * Checks if the input module id belongs to data path.
 *
 * Return: True if the module belongs to data path, false otherwise
 */
static bool is_data_path_module(VOS_MODULE_ID mod_id)
{
	switch (mod_id) {
	case VOS_MODULE_ID_HDD_DATA:
	case VOS_MODULE_ID_HDD_SAP_DATA:
	case VOS_MODULE_ID_HTC:
	case VOS_MODULE_ID_TXRX:
	case VOS_MODULE_ID_HIF:
	case VOS_MODULE_ID_VOSS:
	case VOS_MODULE_ID_TL:
		return true;
	default:
		return false;
	}
}

static void set_default_logtoapp_log_level(void)
{
	int i;

	/* module id 0 is reserved */
	for (i = 1; i < VOS_MODULE_ID_MAX; i++) {
		if (is_data_path_module(i))
			vos_trace_set_module_trace_level(i,
						VOS_DATA_PATH_TRACE_LEVEL);
		else
			vos_trace_setValue(i, VOS_TRACE_LEVEL_ALL, VOS_TRUE);
	}
}

static void clear_default_logtoapp_log_level(void)
{
	int module;

	for (module = 0; module < VOS_MODULE_ID_MAX; module++) {
		vos_trace_setValue(module, VOS_TRACE_LEVEL_NONE,
				VOS_FALSE);
		vos_trace_setValue(module, VOS_TRACE_LEVEL_FATAL,
				VOS_TRUE);
		vos_trace_setValue(module, VOS_TRACE_LEVEL_ERROR,
				VOS_TRUE);
	}

	vos_trace_setValue(VOS_MODULE_ID_RSV3, VOS_TRACE_LEVEL_NONE,
			VOS_FALSE);
	vos_trace_setValue(VOS_MODULE_ID_RSV4, VOS_TRACE_LEVEL_NONE,
			VOS_FALSE);
}

/* Need to call this with spin_lock acquired */
static int wlan_queue_logmsg_for_app(void)
{
	char *ptr;
	int ret = 0;
	ptr = &gwlan_logging.pcur_node->logbuf[sizeof(tAniHdr)];
	ptr[gwlan_logging.pcur_node->filled_length] = '\0';

	*(unsigned short *)(gwlan_logging.pcur_node->logbuf) =
			ANI_NL_MSG_LOG_TYPE;
	*(unsigned short *)(gwlan_logging.pcur_node->logbuf + 2) =
			gwlan_logging.pcur_node->filled_length;
	list_add_tail(&gwlan_logging.pcur_node->node,
			&gwlan_logging.filled_list);

	if (!list_empty(&gwlan_logging.free_list)) {
		/* Get buffer from free list */
		gwlan_logging.pcur_node =
			(struct log_msg *)(gwlan_logging.free_list.next);
		list_del_init(gwlan_logging.free_list.next);
	} else if (!list_empty(&gwlan_logging.filled_list)) {
		/* Get buffer from filled list */
		/* This condition will drop the packet from being
		 * indicated to app
		 */
		gwlan_logging.pcur_node =
			(struct log_msg *)(gwlan_logging.filled_list.next);
		++gwlan_logging.drop_count;
		/* print every 64th drop count */
		if (vos_is_multicast_logging() &&
				(!(gwlan_logging.drop_count % 0x40))) {
			pr_info("%s: drop_count = %u index = %d filled_length = %d\n",
				__func__, gwlan_logging.drop_count,
				gwlan_logging.pcur_node->index,
				gwlan_logging.pcur_node->filled_length);
		}
		list_del_init(gwlan_logging.filled_list.next);
		ret = 1;
	}

	/* Reset the current node values */
	gwlan_logging.pcur_node->filled_length = 0;
	return ret;
}


int wlan_log_to_user(VOS_TRACE_LEVEL log_level, char *to_be_sent, int length)
{
	/* Add the current time stamp */
	char *ptr;
	char tbuf[50];
	int tlen;
	int total_log_len;
	unsigned int *pfilled_length;
	bool wake_up_thread = false;
	unsigned long flags;
	uint64_t ts;
	uint32_t rem;

	if (!vos_is_multicast_logging()) {
		/*
		 * This is to make sure that we print the logs to kmsg console
		 * when no logger app is running. This is also needed to
		 * log the initial messages during loading of driver where even
		 * if app is running it will not be able to
		 * register with driver immediately and start logging all the
		 * messages.
		 */
		pr_info("%s\n", to_be_sent);
	} else {

		/* Format the Log time [Seconds.microseconds] */
		ts = adf_get_boottime();
		rem = do_div(ts, VOS_TIMER_TO_SEC_UNIT);
		tlen = snprintf(tbuf, sizeof(tbuf), "[%s][%lu.%06lu] ",
				current->comm,
				(unsigned long) ts, (unsigned long)rem);

		/* 1+1 indicate '\n'+'\0' */
		total_log_len = length + tlen + 1 + 1;

		spin_lock_irqsave(&gwlan_logging.spin_lock, flags);
		// wlan logging svc resources are not yet initialized
		if (!gwlan_logging.pcur_node) {
			spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);
			return -EIO;
		}

		pfilled_length = &gwlan_logging.pcur_node->filled_length;

		/* Check if we can accomodate more log into current
		 * node/buffer
		 */
		if ((MAX_LOGMSG_LENGTH <= (*pfilled_length +
							sizeof(tAniNlHdr))) ||
			((MAX_LOGMSG_LENGTH - (*pfilled_length +
				sizeof(tAniNlHdr))) < total_log_len)) {
			wake_up_thread = true;
			wlan_queue_logmsg_for_app();
			pfilled_length =
				&gwlan_logging.pcur_node->filled_length;
		}

		ptr = &gwlan_logging.pcur_node->logbuf[sizeof(tAniHdr)];

		/* Assumption here is that we receive logs which is always
		 * less than MAX_LOGMSG_LENGTH, where we can accomodate the
		 *   tAniNlHdr + [context][timestamp] + log
		 * VOS_ASSERT if we cannot accomodate the the complete log into
		 * the available buffer.
		 *
		 * Continue and copy logs to the available length and
		 * discard the rest.
		 */
		if (MAX_LOGMSG_LENGTH < (sizeof(tAniNlHdr) + total_log_len)) {
			VOS_ASSERT(0);
			total_log_len = MAX_LOGMSG_LENGTH -
						sizeof(tAniNlHdr) - 2;
		}

		memcpy(&ptr[*pfilled_length], tbuf, tlen);
		memcpy(&ptr[*pfilled_length + tlen], to_be_sent,
				min(length, (total_log_len - tlen)));
		*pfilled_length += tlen + min(length, total_log_len - tlen);
		ptr[*pfilled_length] = '\n';
		*pfilled_length += 1;

		spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);

		/* Wakeup logger thread */
		if ((true == wake_up_thread)) {
			/* If there is logger app registered wakeup the logging
			 * thread (or) if always multicasting of host messages
			 * is enabled, wake up the logging thread
			 */
			set_bit(HOST_LOG_DRIVER_MSG, &gwlan_logging.eventFlag);
			wake_up_interruptible(&gwlan_logging.wait_queue);
		}

		if (gwlan_logging.log_fe_to_console
			&& ((VOS_TRACE_LEVEL_FATAL == log_level)
			|| (VOS_TRACE_LEVEL_ERROR == log_level))) {
			pr_info("%s\n", to_be_sent);
		}
	}
	return 0;
}

static int send_filled_buffers_to_user(void)
{
	int ret = -1;
	struct log_msg *plog_msg;
	int payload_len;
	int tot_msg_len;
	tAniNlHdr *wnl;
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;
	static int nlmsg_seq;
	unsigned long flags;
	static int rate_limit;

	while (!list_empty(&gwlan_logging.filled_list)
		&& !gwlan_logging.exit) {

		skb = dev_alloc_skb(MAX_LOGMSG_LENGTH);
		if (skb == NULL) {
			if (!rate_limit) {
				pr_err("%s: dev_alloc_skb() failed for msg size[%d] drop count = %u\n",
					__func__, MAX_LOGMSG_LENGTH,
					gwlan_logging.drop_count);
			}
			rate_limit = 1;
			ret = -ENOMEM;
			break;
		}
		rate_limit = 0;

		spin_lock_irqsave(&gwlan_logging.spin_lock, flags);

		plog_msg = (struct log_msg *)
			(gwlan_logging.filled_list.next);
		list_del_init(gwlan_logging.filled_list.next);
		spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);
		/* 4 extra bytes for the radio idx */
		payload_len = plog_msg->filled_length +
			sizeof(wnl->radio) + sizeof(tAniHdr);

		tot_msg_len = NLMSG_SPACE(payload_len);
		nlh = nlmsg_put(skb, 0, nlmsg_seq++,
				ANI_NL_MSG_LOG, payload_len,
				NLM_F_REQUEST);
		if (NULL == nlh) {
			spin_lock_irqsave(&gwlan_logging.spin_lock, flags);
			list_add_tail(&plog_msg->node,
				&gwlan_logging.free_list);
			spin_unlock_irqrestore(&gwlan_logging.spin_lock,
							flags);
			pr_err("%s: drop_count = %u\n", __func__,
				++gwlan_logging.drop_count);
			pr_err("%s: nlmsg_put() failed for msg size[%d]\n",
				__func__, tot_msg_len);
			dev_kfree_skb(skb);
			skb = NULL;
			ret = -EINVAL;
			continue;
		}

		wnl = (tAniNlHdr *) nlh;
		wnl->radio = plog_msg->radio;
		memcpy(&wnl->wmsg, plog_msg->logbuf,
				plog_msg->filled_length +
				sizeof(tAniHdr));

		spin_lock_irqsave(&gwlan_logging.spin_lock, flags);
		list_add_tail(&plog_msg->node,
				&gwlan_logging.free_list);
		spin_unlock_irqrestore(&gwlan_logging.spin_lock, flags);

		ret = nl_srv_bcast(skb);
		/* print every 64th drop count */
		if (ret < 0 && (!(gwlan_logging.drop_count % 0x40))) {
			pr_err("%s: Send Failed %d drop_count = %u\n",
				__func__, ret, ++gwlan_logging.drop_count);
			skb = NULL;
		} else {
			skb = NULL;
			ret = 0;
		}
	}

	return ret;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * wlan_report_log_completion() - Report bug report completion to userspace
 * @is_fatal: Type of event, fatal or not
 * @indicator: Source of bug report, framework/host/firmware
 * @reason_code: Reason for triggering bug report
 *
 * This function is used to report the bug report completion to userspace
 *
 * Return: None
 */
void wlan_report_log_completion(uint32_t is_fatal,
				uint32_t indicator,
				uint32_t reason_code)
{
	WLAN_VOS_DIAG_EVENT_DEF(wlan_diag_event,
				struct vos_event_wlan_log_complete);

	wlan_diag_event.is_fatal = is_fatal;
	wlan_diag_event.indicator = indicator;
	wlan_diag_event.reason_code = reason_code;
	wlan_diag_event.reserved = 0;

	WLAN_VOS_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_LOG_COMPLETE);
}
#endif

/**
 * send_flush_completion_to_user() - Indicate flush completion to the user
 *
 * This function is used to send the flush completion message to user space
 *
 * Return: None
 */
void send_flush_completion_to_user(void)
{
	uint32_t is_fatal, indicator, reason_code;

	vos_get_log_completion(&is_fatal, &indicator, &reason_code);

	/* Error on purpose, so that it will get logged in the kmsg */
	LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
			"%s: Sending flush done to userspace", __func__);

	wlan_report_log_completion(is_fatal, indicator, reason_code);
}

/**
 * wlan_logging_thread() - The WLAN Logger thread
 * @Arg - pointer to the HDD context
 *
 * This thread logs log message to App registered for the logs.
 */
static int wlan_logging_thread(void *Arg)
{
	int ret_wait_status = 0;
	int ret = 0;

	set_user_nice(current, -2);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	daemonize("wlan_logging_thread");
#endif

	while (!gwlan_logging.exit) {
		ret_wait_status = wait_event_interruptible(
		    gwlan_logging.wait_queue,
		    (!list_empty(&gwlan_logging.filled_list)
		  || test_bit(HOST_LOG_DRIVER_MSG, &gwlan_logging.eventFlag)
		  || test_bit(HOST_LOG_PER_PKT_STATS,
		     &gwlan_logging.eventFlag)
		  || test_bit(HOST_LOG_FW_FLUSH_COMPLETE,
		     &gwlan_logging.eventFlag)
		  || gwlan_logging.exit));

		if (ret_wait_status == -ERESTARTSYS) {
			pr_err("%s: wait_event_interruptible returned -ERESTARTSYS",
				__func__);
			break;
		}

		if (gwlan_logging.exit) {
			pr_err("%s: Exiting the thread\n", __func__);
			break;
		}

		if (test_and_clear_bit(HOST_LOG_DRIVER_MSG,
				       &gwlan_logging.eventFlag)) {
			ret = send_filled_buffers_to_user();
			if (-ENOMEM == ret) {
				msleep(200);
			}
		}

		if (test_and_clear_bit(HOST_LOG_PER_PKT_STATS,
				       &gwlan_logging.eventFlag)) {
			ret = pktlog_send_per_pkt_stats_to_user();
			if (-ENOMEM == ret) {
				msleep(200);
			}
		}

		if (test_and_clear_bit(HOST_LOG_FW_FLUSH_COMPLETE,
					&gwlan_logging.eventFlag)) {
			/* Flush bit could have been set while we were mid
			 * way in the logging thread. So, need to check other
			 * buffers like log messages, per packet stats again
			 * to flush any residual data in them
			 */
			if (gwlan_logging.is_flush_complete == true) {
				gwlan_logging.is_flush_complete = false;
				send_flush_completion_to_user();
			} else {
				gwlan_logging.is_flush_complete = true;
				set_bit(HOST_LOG_DRIVER_MSG,
						&gwlan_logging.eventFlag);
				set_bit(HOST_LOG_PER_PKT_STATS,
						&gwlan_logging.eventFlag);
				set_bit(HOST_LOG_FW_FLUSH_COMPLETE,
						&gwlan_logging.eventFlag);
				wake_up_interruptible(
					&gwlan_logging.wait_queue);
			}
		}
	}

	pr_info("%s: Terminating\n", __func__);

	complete_and_exit(&gwlan_logging.shutdown_comp, 0);

	return 0;
}

/*
 * Process all the Netlink messages from Logger Socket app in user space
 */
static int wlan_logging_proc_sock_rx_msg(struct sk_buff *skb)
{
	tAniNlHdr *wnl;
	int radio;
	int type;
	int ret;

	wnl = (tAniNlHdr *) skb->data;
	radio = wnl->radio;
	type = wnl->nlh.nlmsg_type;

	if (radio < 0 || radio > ANI_MAX_RADIOS) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
				"%s: invalid radio id [%d]\n",
				__func__, radio);
		return -EINVAL;
	}

	if (gapp_pid != INVALID_PID) {
		if (wnl->nlh.nlmsg_pid > gapp_pid) {
			gapp_pid = wnl->nlh.nlmsg_pid;
		}

		spin_lock_bh(&gwlan_logging.spin_lock);
		if (gwlan_logging.pcur_node->filled_length) {
			wlan_queue_logmsg_for_app();
		}
		spin_unlock_bh(&gwlan_logging.spin_lock);
		set_bit(HOST_LOG_DRIVER_MSG, &gwlan_logging.eventFlag);
		wake_up_interruptible(&gwlan_logging.wait_queue);
	} else {
		/* This is to set the default levels (WLAN logging
		 * default values not the VOS trace default) when
		 * logger app is registered for the first time.
		 */
		gapp_pid = wnl->nlh.nlmsg_pid;
	}

	ret = wlan_send_sock_msg_to_app(&wnl->wmsg, 0,
			ANI_NL_MSG_LOG, wnl->nlh.nlmsg_pid);
	if (ret < 0) {
		LOGGING_TRACE(VOS_TRACE_LEVEL_ERROR,
				"wlan_send_sock_msg_to_app: failed");
	}

	return ret;
}

int wlan_logging_sock_activate_svc(int log_fe_to_console, int num_buf)
{
	int i = 0;
	unsigned long irq_flag;

	pr_info("%s: Initalizing FEConsoleLog = %d NumBuff = %d\n",
			__func__, log_fe_to_console, num_buf);

	gapp_pid = INVALID_PID;

	gplog_msg = (struct log_msg *) vmalloc(
			num_buf * sizeof(struct log_msg));
	if (!gplog_msg) {
		pr_err("%s: Could not allocate memory\n", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(gplog_msg, (num_buf * sizeof(struct log_msg)));

	gwlan_logging.log_fe_to_console = !!log_fe_to_console;
	gwlan_logging.num_buf = num_buf;

	spin_lock_irqsave(&gwlan_logging.spin_lock, irq_flag);
	INIT_LIST_HEAD(&gwlan_logging.free_list);
	INIT_LIST_HEAD(&gwlan_logging.filled_list);

	for (i = 0; i < num_buf; i++) {
		list_add(&gplog_msg[i].node, &gwlan_logging.free_list);
		gplog_msg[i].index = i;
	}
	gwlan_logging.pcur_node = (struct log_msg *)
		(gwlan_logging.free_list.next);
	list_del_init(gwlan_logging.free_list.next);
	spin_unlock_irqrestore(&gwlan_logging.spin_lock, irq_flag);

	init_waitqueue_head(&gwlan_logging.wait_queue);
	gwlan_logging.exit = false;
	clear_bit(HOST_LOG_DRIVER_MSG, &gwlan_logging.eventFlag);
	clear_bit(HOST_LOG_PER_PKT_STATS, &gwlan_logging.eventFlag);
	clear_bit(HOST_LOG_FW_FLUSH_COMPLETE, &gwlan_logging.eventFlag);
	init_completion(&gwlan_logging.shutdown_comp);
	gwlan_logging.thread = kthread_create(wlan_logging_thread, NULL,
					"wlan_logging_thread");
	if (IS_ERR(gwlan_logging.thread)) {
		pr_err("%s: Could not Create LogMsg Thread Controller",
		       __func__);
		spin_lock_irqsave(&gwlan_logging.spin_lock, irq_flag);
		gwlan_logging.pcur_node = NULL;
		spin_unlock_irqrestore(&gwlan_logging.spin_lock, irq_flag);
		vfree(gplog_msg);
		gplog_msg = NULL;
		return -ENOMEM;
	}
	wake_up_process(gwlan_logging.thread);
	gwlan_logging.is_active = true;
	gwlan_logging.is_flush_complete = false;

	nl_srv_register(ANI_NL_MSG_LOG, wlan_logging_proc_sock_rx_msg);

	pr_info("%s: Activated wlan_logging svc\n", __func__);
	return 0;
}

int wlan_logging_sock_deactivate_svc(void)
{
	unsigned long irq_flag;

	if (!gplog_msg)
		return 0;

	nl_srv_unregister(ANI_NL_MSG_LOG, wlan_logging_proc_sock_rx_msg);
	clear_default_logtoapp_log_level();
	gapp_pid = INVALID_PID;

	INIT_COMPLETION(gwlan_logging.shutdown_comp);
	gwlan_logging.exit = true;
	gwlan_logging.is_active = false;
	vos_set_multicast_logging(0);
	gwlan_logging.is_flush_complete = false;
	clear_bit(HOST_LOG_DRIVER_MSG, &gwlan_logging.eventFlag);
	clear_bit(HOST_LOG_PER_PKT_STATS, &gwlan_logging.eventFlag);
	clear_bit(HOST_LOG_FW_FLUSH_COMPLETE, &gwlan_logging.eventFlag);
	wake_up_interruptible(&gwlan_logging.wait_queue);
	wait_for_completion(&gwlan_logging.shutdown_comp);

	spin_lock_irqsave(&gwlan_logging.spin_lock, irq_flag);
	gwlan_logging.pcur_node = NULL;
	spin_unlock_irqrestore(&gwlan_logging.spin_lock, irq_flag);
	vfree(gplog_msg);
	gplog_msg = NULL;

	pr_info("%s: Deactivate wlan_logging svc\n", __func__);

	return 0;
}

int wlan_logging_sock_init_svc(void)
{
	spin_lock_init(&gwlan_logging.spin_lock);
	gapp_pid = INVALID_PID;
	gwlan_logging.pcur_node = NULL;

	return 0;
}

int wlan_logging_sock_deinit_svc(void)
{
	gwlan_logging.pcur_node = NULL;
	gapp_pid = INVALID_PID;

       return 0;
}

/**
 * wlan_logging_set_per_pkt_stats() - This function triggers per packet logging
 *
 * This function is used to send signal to the logger thread for logging per
 * packet stats
 *
 * Return: None
 *
 */
void wlan_logging_set_per_pkt_stats(void)
{
	if (gwlan_logging.is_active == false)
		return;

	set_bit(HOST_LOG_PER_PKT_STATS, &gwlan_logging.eventFlag);
	wake_up_interruptible(&gwlan_logging.wait_queue);
}

/**
 * wlan_logging_set_log_level() - Set the logging level
 *
 * This function is used to set the logging level of host debug messages
 *
 * Return: None
 */
void wlan_logging_set_log_level(void)
{
	set_default_logtoapp_log_level();
}

/*
 * wlan_logging_set_fw_flush_complete() - FW log flush completion
 *
 * This function is used to send signal to the logger thread to indicate
 * that the flushing of FW logs is complete by the FW
 *
 * Return: None
 *
 */
void wlan_logging_set_fw_flush_complete(void)
{
	if (gwlan_logging.is_active == false)
		return;

	set_bit(HOST_LOG_FW_FLUSH_COMPLETE, &gwlan_logging.eventFlag);
	wake_up_interruptible(&gwlan_logging.wait_queue);
}
#endif /* WLAN_LOGGING_SOCK_SVC_ENABLE */
