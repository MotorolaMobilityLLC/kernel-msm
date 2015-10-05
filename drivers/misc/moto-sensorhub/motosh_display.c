/*
 * Copyright (C) 2010-2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fb_quickdraw.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/pm_qos.h>
#include <linux/quickwakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

struct qd_trace_event_t {
	u8 op;
	u8 buffer_id;
	u8 response;
	u32 timestamp;
};

#define QD_TRACE_BUF_SIZE 64
struct qd_trace_event_t qd_trace[QD_TRACE_BUF_SIZE];
int qd_trace_index;

#define QD_TRACE_PREPARE 'P'
#define QD_TRACE_DRAW 'D'
#define QD_TRACE_ERASE 'E'
#define QD_TRACE_COMPLETE 'C'

#define QD_TRACE_RXD 'R'
#define QD_TRACE_DONE 'D'
#define QD_TRACE_INVALID 'I'

static char QD_parse_cmd(u8 cmd)
{
	switch (cmd) {
	case AOD_WAKEUP_REASON_QP_PREPARE:
		return QD_TRACE_PREPARE;
	case AOD_WAKEUP_REASON_QP_DRAW:
		return QD_TRACE_DRAW;
	case AOD_WAKEUP_REASON_QP_ERASE:
		return QD_TRACE_ERASE;
	case AOD_WAKEUP_REASON_QP_COMPLETE:
		return QD_TRACE_COMPLETE;
	}
	return (cmd < 10) ? (char)(cmd + '0') : 'U';
}

static char QD_parse_resp(u8 response)
{
	switch (response) {
	case AOD_QP_ACK_RCVD:
		return QD_TRACE_RXD;
	case AOD_QP_ACK_DONE:
		return QD_TRACE_DONE;
	case AOD_QP_ACK_INVALID:
		return QD_TRACE_INVALID;
	}
	return (response < 10) ? (char)(response + '0') : 'U';
}

static void dump_one_event(const struct device *dev,
	struct qd_trace_event_t *event, s64 offset, const char *fmt, ...)
{
	va_list va;
	struct va_format vaf;

	va_start(va, fmt);

	vaf.fmt = fmt;
	vaf.va = &va;

	dev_info(dev, "%lld %c %c%pV\n", event->timestamp + offset,
		QD_parse_cmd(event->op), QD_parse_resp(event->response), &vaf);

	va_end(va);
}

static void motosh_quickpeek_trace_dump(struct motosh_data *ps_motosh,
	u32 timestamp)
{
	struct timespec ts;
	s64 k_timestamp, offset = 0;
	int i;

	if (timestamp) {
		get_monotonic_boottime(&ts);
		k_timestamp = ts.tv_sec*1000LL + ts.tv_nsec/1000000LL;
		offset = (s64)timestamp - k_timestamp;
	}

	dev_info(&ps_motosh->client->dev, "%s: [events: %u]\n", __func__,
		qd_trace_index);
	for (i = 0; i < qd_trace_index; i++) {
		if (qd_trace[i].op == AOD_WAKEUP_REASON_QP_DRAW)
			dump_one_event(&ps_motosh->client->dev, &qd_trace[i],
				offset, " %u", qd_trace[i].buffer_id);
		else
			dump_one_event(&ps_motosh->client->dev, &qd_trace[i],
				offset, "");
	}
	dev_info(&ps_motosh->client->dev, "%s: END\n", __func__);

	qd_trace_index = 0;
}

static void QD_trace_add(struct motosh_data *ps_motosh, u8 command,
	u8 buffer_id, u8 response)
{
	struct timespec ts;
	u32 timestamp;

	if (command == AOD_WAKEUP_REASON_QP_DUMP_TRACE)
		return;

	get_monotonic_boottime(&ts);

	timestamp = ts.tv_sec*1000LL + ts.tv_nsec/1000000LL;

	qd_trace[qd_trace_index].op = command;
	qd_trace[qd_trace_index].buffer_id = buffer_id;
	qd_trace[qd_trace_index].timestamp = timestamp;
	qd_trace[qd_trace_index].response = response;
	qd_trace_index++;

	if (qd_trace_index >= QD_TRACE_BUF_SIZE) {
		dev_err(&ps_motosh->client->dev, "trace buffer overflow!\n");
		motosh_quickpeek_trace_dump(ps_motosh, 0);
	}
}

static int motosh_quickpeek_is_pending_locked(struct motosh_data *ps_motosh)
{
	return !list_empty(&ps_motosh->quickpeek_command_list);
}

int motosh_quickpeek_is_pending(struct motosh_data *ps_motosh)
{
	int ret;

	mutex_lock(&ps_motosh->qp_list_lock);
	ret = motosh_quickpeek_is_pending_locked(ps_motosh);
	mutex_unlock(&ps_motosh->qp_list_lock);

	return ret;
}

/* Should always be called with qp_list_lock locked */
static void motosh_quickpeek_set_in_progress_locked(
	struct motosh_data *ps_motosh, bool in_progress)
{
	ps_motosh->qp_in_progress = in_progress;
}

/* Should always be called with qp_in_progress TRUE */
static void motosh_quickpeek_set_prepared(struct motosh_data *ps_motosh,
	bool prepared)
{
	BUG_ON(!ps_motosh->qp_in_progress);
	ps_motosh->qp_prepared = prepared;
}

int motosh_quickpeek_disable_when_idle(struct motosh_data *ps_motosh)
{
	int ret = 0;

	mutex_lock(&ps_motosh->lock);
	mutex_lock(&ps_motosh->qp_list_lock);
	if (!ps_motosh->qp_in_progress && !ps_motosh->qp_prepared &&
		!motosh_quickpeek_is_pending_locked(ps_motosh)) {
		ps_motosh->ignore_wakeable_interrupts = true;
		ret = 1;
	}
	mutex_unlock(&ps_motosh->qp_list_lock);
	mutex_unlock(&ps_motosh->lock);

	return ret;
}

static int motosh_quickpeek_status_ack(struct motosh_data *ps_motosh,
	struct motosh_quickpeek_message *qp_message, int ack_return)
{
	int ret = 0;
	unsigned char payload = ack_return & 0x03;
	unsigned int req_bit;
	unsigned char cmdbuff[4];

	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	motosh_wake(ps_motosh);

	if (qp_message && qp_message->message == AOD_WAKEUP_REASON_QP_DRAW)
		payload |= (qp_message->buffer_id &
			AOD_QP_ACK_BUFFER_ID_MASK) << 2;

	req_bit = atomic_read(&ps_motosh->qp_enabled) & 0x01;

	cmdbuff[0] = PD_PEEK_RESPONSE;
	cmdbuff[1] = req_bit;
	cmdbuff[2] = qp_message ? qp_message->message : 0x00;
	cmdbuff[3] = payload;
	if (motosh_i2c_write(ps_motosh, cmdbuff, 4) < 0) {
		dev_err(&ps_motosh->client->dev,
			"Write peek status reg failed\n");
		ret = -EIO;
	}

	dev_dbg(&ps_motosh->client->dev, "%s: message: %d | req_bit: %d | ack_return: %d | buffer_id: %d | ret: %d\n",
		__func__,
		qp_message ? qp_message->message : 0, req_bit, ack_return,
		qp_message ? qp_message->buffer_id : 0, ret);

	QD_trace_add(ps_motosh, qp_message ? qp_message->message : 0,
		qp_message ? qp_message->buffer_id : 0,
		(u8)(ack_return & 0x03));

	motosh_sleep(ps_motosh);

	return ret;
}

int motosh_display_handle_touch_locked(struct motosh_data *ps_motosh)
{
	char *envp[2];
	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	envp[0] = "MOTOSHWAKE=TOUCH";
	envp[1] = NULL;
	dev_info(&ps_motosh->client->dev,
		"Sending uevent, MOTOSH TOUCH wake\n");

	if (kobject_uevent_env(&ps_motosh->client->dev.kobj,
		KOBJ_CHANGE, envp)) {
		dev_err(&ps_motosh->client->dev,
			"Failed to create uevent\n");
		return -EPERM;
	}

	return 0;
}

int motosh_display_handle_quickpeek_locked(struct motosh_data *ps_motosh)
{
	int ret = 0;
	u8 aod_qp_reason;
	u8 aod_qp_panel_state;
	struct motosh_quickpeek_message *qp_message = NULL;
	bool ack_only = false;
	unsigned char cmdbuff[1];
	unsigned char readbuff[9];

	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	/* set pm qos restriction to limit cpu idle during quickdraw */
	pm_qos_update_request_timeout(&ps_motosh->pm_qos_req_dma,
		ps_motosh->pdata->qd_pm_qos_latency,
		ps_motosh->pdata->qd_pm_qos_timeout);

	cmdbuff[0] = P_DISPLAY_STATUS;
	if (motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff, 1, 2)
		< 0) {
		dev_err(&ps_motosh->client->dev,
			"Get status reg failed\n");
		ret = -EIO;
		goto error;
	}

	aod_qp_panel_state = readbuff[0] & 0x3;
	aod_qp_reason = (readbuff[1] >> 4) & 0xf;

	qp_message = kzalloc(sizeof(*qp_message), GFP_KERNEL);
	if (!qp_message) {
		dev_err(&ps_motosh->client->dev,
			"%s: kzalloc failed!\n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	qp_message->panel_state = aod_qp_panel_state;
	qp_message->message = aod_qp_reason;

	switch (aod_qp_reason) {
	case AOD_WAKEUP_REASON_QP_PREPARE:
		if (!ps_motosh->qp_prepared)
			qd_trace_index = 0;
		dev_dbg(&ps_motosh->client->dev,
			"Received peek prepare command\n");
		break;
	case AOD_WAKEUP_REASON_QP_COMPLETE:
		cmdbuff[0] = PD_QUICK_PEEK;
		if (motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 1) < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading peek draw data from STM failed\n");
			ret = -EIO;
			goto error;
		}
		qp_message->commit = (readbuff[0] & 0x80) >> 7;
		dev_dbg(&ps_motosh->client->dev,
			"Received peek complete command commit: %d\n",
			qp_message->commit);
		break;
	case AOD_WAKEUP_REASON_QP_DRAW:
		cmdbuff[0] = PD_QUICK_PEEK;
		if (motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 5) < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading peek draw data from STM failed\n");
			ret = -EIO;
			goto error;
		}
		qp_message->buffer_id = readbuff[0] & 0x3f;
		qp_message->commit = (readbuff[0] & 0x80) >> 7;
		qp_message->x1 = readbuff[1] | readbuff[2] << 8;
		qp_message->y1 = readbuff[3] | readbuff[4] << 8;

		dev_dbg(&ps_motosh->client->dev,
			"Received peek draw command for buffer: %d commit: %d (coord: %d, %d)\n",
			qp_message->buffer_id, qp_message->commit,
			qp_message->x1, qp_message->y1);
		break;
	case AOD_WAKEUP_REASON_QP_ERASE:
		cmdbuff[0] = PD_QUICK_PEEK;
		if (motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 9) < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading peek erase data from STM failed\n");
			ret = -EIO;
			goto error;
		}
		qp_message->commit = (readbuff[0] & 0x80) >> 7;
		qp_message->x1 = readbuff[1] | readbuff[2] << 8;
		qp_message->y1 = readbuff[3] | readbuff[4] << 8;
		qp_message->x2 = readbuff[5] | readbuff[6] << 8;
		qp_message->y2 = readbuff[7] | readbuff[8] << 8;

		dev_dbg(&ps_motosh->client->dev,
			"Received peek erase command: commit: %d (%d, %d) -> (%d, %d)\n",
			qp_message->commit, qp_message->x1, qp_message->y1,
			qp_message->x2, qp_message->y2);
		break;
	case AOD_WAKEUP_REASON_QP_DUMP_TRACE:
		{
		u32 timestamp;
		cmdbuff[0] = PD_QUICK_PEEK;
		if (motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff,
			1, 5) < 0) {
			dev_err(&ps_motosh->client->dev,
				"Reading peek draw data from STM failed\n");
			ret = -EIO;
			goto error;
		}
		timestamp = readbuff[0] |
			readbuff[1] << 8 |
			readbuff[2] << 16 |
			readbuff[3] << 24;
		dev_dbg(&ps_motosh->client->dev,
			"Received trace dump request\n");
		motosh_quickpeek_trace_dump(ps_motosh, timestamp);
		}
		ack_only = true;
		break;
	default:
		dev_err(&ps_motosh->client->dev,
			"Unknown quickpeek command [%d]!", aod_qp_reason);
		ret = -EPERM;
		goto error;
	}

	if (!atomic_read(&ps_motosh->qp_enabled)) {
		dev_dbg(&ps_motosh->client->dev,
			"%s: Received quickpeek interrupt with quickpeek disabled!\n",
			__func__);
		ret = -EPERM;
		goto error;
	}

	motosh_quickpeek_status_ack(ps_motosh, qp_message,
		AOD_QP_ACK_RCVD);

	if (ack_only) {
		kfree(qp_message);
		goto exit;
	}

	ps_motosh->quickpeek_occurred = true;

	mutex_lock(&ps_motosh->qp_list_lock);
	list_add_tail(&qp_message->list,
		&ps_motosh->quickpeek_command_list);
	queue_work(ps_motosh->quickpeek_work_queue,
		&ps_motosh->quickpeek_work);
	wake_lock(&ps_motosh->quickpeek_wakelock);
	mutex_unlock(&ps_motosh->qp_list_lock);

exit:
	return ret;

error:
	motosh_quickpeek_status_ack(ps_motosh, qp_message, AOD_QP_ACK_INVALID);
	kfree(qp_message);
	goto exit;
}

void motosh_quickpeek_reset_locked(struct motosh_data *ps_motosh)
{
	struct motosh_quickpeek_message *entry, *entry_tmp;
	struct list_head temp_list;
	int ret = 0;

	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	/* Move the current list to a temp list so we can flush the queue */
	mutex_lock(&ps_motosh->qp_list_lock);
	list_replace_init(&ps_motosh->quickpeek_command_list, &temp_list);
	mutex_unlock(&ps_motosh->qp_list_lock);

	flush_work(&ps_motosh->quickpeek_work);

	/* From here, no quickpeek interrupts will be handled, because we have
	   the main lock. Also, the quickpeek workqueue is idle, so basically
	   nothing quickpeek is happening. Safe to do lots of things now. */

	if (ps_motosh->qp_prepared) {
		/* Cleanup fb driver state */
		ret = fb_quickdraw_cleanup(0);

		if (ret)
			dev_err(&ps_motosh->client->dev,
				"%s: Failed to cleanup (ret: %d)\n",
				__func__, ret);
	}

	list_for_each_entry_safe(entry, entry_tmp, &temp_list, list) {
		list_del(&entry->list);
		motosh_quickpeek_status_ack(ps_motosh, entry, AOD_QP_ACK_DONE);
		kfree(entry);
	}

	/* remove pm qos restriction */
	pm_qos_update_request(&ps_motosh->pm_qos_req_dma, PM_QOS_DEFAULT_VALUE);

	/* This is the ONLY place we should set this explicitly instead of using
	   the helper */
	ps_motosh->qp_prepared = false;
	ps_motosh->ignored_interrupts = 0;
	ps_motosh->ignore_wakeable_interrupts = false;

	wake_unlock(&ps_motosh->quickpeek_wakelock);
	wake_up_all(&ps_motosh->quickpeek_wait_queue);
}

/* Should always be called with qp_in_progress TRUE */
static int motosh_quickpeek_check_state(struct motosh_data *ps_motosh,
	struct motosh_quickpeek_message *qp_message)
{
	int ret = 0;

	BUG_ON(!ps_motosh->qp_in_progress);

	switch (qp_message->message) {
	case AOD_WAKEUP_REASON_QP_PREPARE:
		if (ps_motosh->qp_prepared)
			ret = -EINVAL;
		break;
	case AOD_WAKEUP_REASON_QP_DRAW:
		if (!ps_motosh->qp_prepared)
			ret = -EINVAL;
		break;
	case AOD_WAKEUP_REASON_QP_ERASE:
		if (!ps_motosh->qp_prepared)
			ret = -EINVAL;
		break;
	case AOD_WAKEUP_REASON_QP_COMPLETE:
		if (!ps_motosh->qp_prepared)
			ret = -EINVAL;
		break;
	default:
		dev_err(&ps_motosh->client->dev,
			"%s: Unknown quickpeek message: %d\n",
			__func__,
			qp_message->message);
		ret = -EINVAL;
		goto exit;
	}

	if (ret)
		dev_err(&ps_motosh->client->dev,
			"%s: ILLEGAL MESSAGE message:%d qp_prepared:%d\n",
			__func__, qp_message->message, ps_motosh->qp_prepared);

exit:
	return ret;
}

/* Take care to NEVER lock the ps_motosh->lock in this function */
void motosh_quickpeek_work_func(struct work_struct *work)
{
	struct motosh_data *ps_motosh = container_of(work,
			struct motosh_data, quickpeek_work);
	int ret;

	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	motosh_wake(ps_motosh);

	mutex_lock(&ps_motosh->qp_list_lock);

	motosh_quickpeek_set_in_progress_locked(ps_motosh, true);

	while (atomic_read(&ps_motosh->qp_enabled) &&
	       !list_empty(&ps_motosh->quickpeek_command_list) &&
	       !motosh_misc_data->in_reset_and_init) {
		struct motosh_quickpeek_message *qp_message;
		int ack_return = AOD_QP_ACK_DONE;
		int x = -1;
		int y = -1;

		qp_message = list_first_entry(
			&ps_motosh->quickpeek_command_list,
			struct motosh_quickpeek_message, list);
		list_del(&qp_message->list);
		mutex_unlock(&ps_motosh->qp_list_lock);

		dev_dbg(&ps_motosh->client->dev, "%s: Handling message: %d\n",
			__func__, qp_message->message);

		if (motosh_quickpeek_check_state(ps_motosh, qp_message)) {
			ack_return = AOD_QP_ACK_INVALID;
			goto loop;
		}

		switch (qp_message->message) {
		case AOD_WAKEUP_REASON_QP_PREPARE:
			ret = fb_quickdraw_prepare(qp_message->panel_state);
			if (ret == QUICKDRAW_ESD_RECOVERED) {
				dev_err(&ps_motosh->client->dev,
					"%s: ESD Recovered: %d\n", __func__,
					ret);
				ack_return = AOD_QP_ACK_ESD_RECOVERED;
				motosh_quickpeek_set_prepared(ps_motosh, true);
			} else if (ret) {
				dev_err(&ps_motosh->client->dev,
					"%s: Prepare Error: %d\n", __func__,
					ret);
				ack_return = AOD_QP_ACK_INVALID;
			} else
				motosh_quickpeek_set_prepared(ps_motosh, true);
			break;
		case AOD_WAKEUP_REASON_QP_DRAW:
			if (qp_message->buffer_id >
				AOD_QP_DRAW_MAX_BUFFER_ID) {
				dev_err(&ps_motosh->client->dev,
					"%s: ILLEGAL buffer_id: %d\n",
					__func__,
					qp_message->buffer_id);
				ack_return = AOD_QP_ACK_INVALID;
				break;
			}
			if (qp_message->x1 != AOD_QP_DRAW_NO_OVERRIDE)
				x = qp_message->x1;
			if (qp_message->y1 != AOD_QP_DRAW_NO_OVERRIDE)
				y = qp_message->y1;
			ret = fb_quickdraw_execute(qp_message->buffer_id,
				qp_message->commit, x, y);
			if (ret) {
				dev_err(&ps_motosh->client->dev,
					"%s: Failed to execute (ret: %d)\n",
					__func__, ret);
				ack_return = AOD_QP_ACK_INVALID;
			}
			break;
		case AOD_WAKEUP_REASON_QP_ERASE:
			if (qp_message->x2 <= qp_message->x1 ||
			    qp_message->y2 <= qp_message->y1) {
				dev_err(&ps_motosh->client->dev,
					"%s: ILLEGAL coordinates\n", __func__);
				ack_return = AOD_QP_ACK_INVALID;
				break;
			}
			ret = fb_quickdraw_erase(qp_message->commit,
				qp_message->x1, qp_message->y1,
				qp_message->x2, qp_message->y2);
			if (ret) {
				dev_err(&ps_motosh->client->dev,
					"%s: Failed to erase (ret: %d)\n",
					__func__, ret);
				ack_return = AOD_QP_ACK_INVALID;
			}
			break;
		case AOD_WAKEUP_REASON_QP_COMPLETE:
			ret = fb_quickdraw_cleanup(qp_message->commit);
			if (ret) {
				dev_err(&ps_motosh->client->dev,
					"%s: Failed to cleanup (ret: %d)\n",
					__func__, ret);
				ack_return = AOD_QP_ACK_INVALID;
			}
			motosh_quickpeek_set_prepared(ps_motosh, false);
			break;
		default:
			dev_err(&ps_motosh->client->dev,
				"%s: Unknown quickpeek message: %d\n",
				__func__,
				qp_message->message);
			break;
		}

loop:
		motosh_quickpeek_status_ack(ps_motosh, qp_message, ack_return);

		mutex_lock(&ps_motosh->qp_list_lock);
		kfree(qp_message);
	}

	if (!motosh_quickpeek_is_pending_locked(ps_motosh) &&
		!ps_motosh->qp_prepared) {
		wake_unlock(&ps_motosh->quickpeek_wakelock);
		wake_up_all(&ps_motosh->quickpeek_wait_queue);

		/* remove pm qos restriction */
		pm_qos_update_request(&ps_motosh->pm_qos_req_dma,
			PM_QOS_DEFAULT_VALUE);
	}

	motosh_quickpeek_set_in_progress_locked(ps_motosh, false);

	mutex_unlock(&ps_motosh->qp_list_lock);

	motosh_sleep(ps_motosh);
}

static int motosh_takeback_locked(struct motosh_data *ps_motosh)
{
	int count = 0;
	unsigned char cmdbuff[2];
	unsigned char readbuff[1];

	dev_dbg(&motosh_misc_data->client->dev, "%s\n", __func__);
	motosh_wake(ps_motosh);

	if (ps_motosh->mode == NORMALMODE) {
		motosh_quickpeek_reset_locked(ps_motosh);

		cmdbuff[0] = PD_PEEK_RESPONSE;
		cmdbuff[1] = 0x00;
		if (motosh_i2c_write(ps_motosh, cmdbuff, 2) < 0) {
			dev_err(&ps_motosh->client->dev,
				"Write peek status reg failed\n");
			goto EXIT;
		}

		do {
			/* issue command again if half way through timeout */
			if (count == MOTOSH_BUSY_RESUME_COUNT >> 1) {
				dev_err(&ps_motosh->client->dev,
					"Write peek status reg retry\n");

				cmdbuff[0] = PD_PEEK_RESPONSE;
				cmdbuff[1] = 0x00;
				if (motosh_i2c_write(ps_motosh,
						     cmdbuff, 2) < 0) {
					dev_err(&ps_motosh->client->dev,
						"Write peek status reg failed\n");
					goto EXIT;
				}
			}
			cmdbuff[0] = P_DISPLAY_STATUS;
			if (motosh_i2c_write_read(ps_motosh,
					cmdbuff, readbuff, 1, 1) < 0) {
				dev_err(&ps_motosh->client->dev,
					"Get status reg failed\n");
				goto EXIT;
			}

			if (!(readbuff[0] & MOTOSH_BUSY_STATUS_MASK))
				break;

			usleep_range(MOTOSH_BUSY_SLEEP_USEC,
						MOTOSH_BUSY_SLEEP_USEC + 1);
			count++;
		} while (count < MOTOSH_BUSY_RESUME_COUNT);

		if (count == MOTOSH_BUSY_RESUME_COUNT) {
			/* re-use ioexpander reset reason, its not applicable
			   on this platform and hence unique until resolved */
			u8 pending_reset_reason = 0x06;

			dev_err(&ps_motosh->client->dev,
				"timedout while waiting for STMBUSY LOW\n");

			/* notify HAL so we can trigger a bug2go */
			motosh_as_data_buffer_write(ps_motosh, DT_RESET,
						&pending_reset_reason,
						1, 0, false);
			/* re-gain control the hard way */
			motosh_reset_and_init(START_RESET);
		}
	}

EXIT:
	motosh_sleep(ps_motosh);
	return 0;
}

static int motosh_handover_locked(struct motosh_data *ps_motosh)
{
	int ret = 0;
	unsigned char cmdbuff[2];

	dev_dbg(&motosh_misc_data->client->dev, "%s\n", __func__);
	motosh_wake(ps_motosh);

	if (ps_motosh->mode == NORMALMODE) {
		cmdbuff[0] = PD_PEEK_RESPONSE;
		cmdbuff[1] = 0x01;
		if (motosh_i2c_write(ps_motosh, cmdbuff, 2) < 0) {
			dev_err(&ps_motosh->client->dev,
				"Write peek status reg failed\n");
			ret = -EIO;
		}
	}

	motosh_sleep(ps_motosh);
	return ret;
}

int motosh_resolve_aod_enabled_locked(struct motosh_data *ps_motosh)
{
	int ret = 0;

	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	mutex_lock(&ps_motosh->aod_enabled.vote_lock);

	if ((ps_motosh->aod_enabled.resolved_vote == AOD_QP_ENABLED_VOTE_MASK)
		!= (ps_motosh->aod_enabled.vote ==
			AOD_QP_ENABLED_VOTE_MASK)) {

		ps_motosh->aod_enabled.resolved_vote =
			ps_motosh->aod_enabled.vote;

		if (atomic_read(&ps_motosh->qp_enabled))
			ret = motosh_handover_locked(ps_motosh);
		else
			ret = motosh_takeback_locked(ps_motosh);
	}

	mutex_unlock(&ps_motosh->aod_enabled.vote_lock);

	return ret;
}

void motosh_vote_aod_enabled_locked(struct motosh_data *ps_motosh, int voter,
				    bool enable)
{
	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	mutex_lock(&ps_motosh->aod_enabled.vote_lock);

	motosh_store_vote_aod_enabled_locked(ps_motosh, voter, enable);

	if (ps_motosh->aod_enabled.vote == AOD_QP_ENABLED_VOTE_MASK)
		atomic_set(&ps_motosh->qp_enabled, 1);
	else
		atomic_set(&ps_motosh->qp_enabled, 0);

	mutex_unlock(&ps_motosh->aod_enabled.vote_lock);
}

void motosh_store_vote_aod_enabled(struct motosh_data *ps_motosh, int voter,
				    bool enable)
{
	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	mutex_lock(&ps_motosh->aod_enabled.vote_lock);
	motosh_store_vote_aod_enabled_locked(ps_motosh, voter, enable);
	mutex_unlock(&ps_motosh->aod_enabled.vote_lock);
}

void motosh_store_vote_aod_enabled_locked(struct motosh_data *ps_motosh,
	int voter, bool enable)
{
	voter &= AOD_QP_ENABLED_VOTE_MASK;
	if (enable)
		ps_motosh->aod_enabled.vote |= voter;
	else
		ps_motosh->aod_enabled.vote &= ~voter;
}

static unsigned long motosh_get_wake_interrupt_status(
		struct motosh_data *ps_motosh, int *err)
{
	unsigned char cmdbuff[1];
	unsigned char readbuff[3];

	motosh_wake(ps_motosh);

	cmdbuff[0] = WAKESENSOR_STATUS;
	*err = motosh_i2c_write_read(ps_motosh, cmdbuff, readbuff, 1, 3);
	if (*err < 0) {
		dev_err(&ps_motosh->client->dev, "Reading from STM failed\n");
		motosh_sleep(ps_motosh);
		return 0;
	}

	motosh_sleep(ps_motosh);
	return (readbuff[2] << 16) | (readbuff[1] << 8) | readbuff[0];
}

/* WARNING: This code is extrememly prone to race conditions. Be very careful
   modifying this code or the variables it uses to control flow. */

static int motosh_qw_check(void *data)
{
	struct motosh_data *ps_motosh = (struct motosh_data *)data;
	unsigned long irq_status;
	int err, ret = 0;

	dev_dbg(&ps_motosh->client->dev, "%s\n", __func__);

	mutex_lock(&ps_motosh->lock);

	if (ps_motosh->quickpeek_occurred) {
		ret = 1;
		goto EXIT;
	}

	irq_status = motosh_get_wake_interrupt_status(ps_motosh, &err);
	if (err < 0)
		goto EXIT;

	ps_motosh->qw_irq_status = irq_status;

	if (irq_status & M_QUICKPEEK) {
		queue_delayed_work(ps_motosh->irq_work_queue,
			&ps_motosh->irq_wake_work, 0);
		ret = 1;
	}

EXIT:
	mutex_unlock(&ps_motosh->lock);

	return ret;
}

static int motosh_qw_execute(void *data)
{
	struct motosh_data *ps_motosh = (struct motosh_data *)data;
	int ret = 1;

	if (!wait_event_timeout(ps_motosh->quickpeek_wait_queue,
		motosh_quickpeek_disable_when_idle(ps_motosh),
		msecs_to_jiffies(MOTOSH_LATE_SUSPEND_TIMEOUT)))
		ret = 0;

	return ret;
}

static struct quickwakeup_ops motosh_quickwakeup_ops = {
	.name = NAME,
	.qw_execute = motosh_qw_execute,
	.qw_check   = motosh_qw_check,
};

void motosh_quickwakeup_init(struct motosh_data *ps_motosh)
{
	motosh_quickwakeup_ops.data = ps_motosh;
	quickwakeup_register(&motosh_quickwakeup_ops);
}
