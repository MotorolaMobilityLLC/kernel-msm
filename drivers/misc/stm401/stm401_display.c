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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
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
#include <linux/quickwakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/stm401.h>

static int stm401_quickpeek_is_pending_locked(struct stm401_data *ps_stm401)
{
	return !list_empty(&ps_stm401->quickpeek_command_list);
}

int stm401_quickpeek_is_pending(struct stm401_data *ps_stm401)
{
	int ret;

	mutex_lock(&ps_stm401->qp_list_lock);
	ret = stm401_quickpeek_is_pending_locked(ps_stm401);
	mutex_unlock(&ps_stm401->qp_list_lock);

	return ret;
}

/* Should always be called with qp_list_lock locked */
static void stm401_quickpeek_set_in_progress_locked(
	struct stm401_data *ps_stm401, bool in_progress)
{
	ps_stm401->qp_in_progress = in_progress;
}

/* Should always be called with qp_in_progress TRUE */
static void stm401_quickpeek_set_prepared(struct stm401_data *ps_stm401,
	bool prepared)
{
	BUG_ON(!ps_stm401->qp_in_progress);
	ps_stm401->qp_prepared = prepared;
}

int stm401_quickpeek_disable_when_idle(struct stm401_data *ps_stm401)
{
	int ret = 0;

	mutex_lock(&ps_stm401->lock);
	mutex_lock(&ps_stm401->qp_list_lock);
	if (!ps_stm401->qp_in_progress && !ps_stm401->qp_prepared &&
		!stm401_quickpeek_is_pending_locked(ps_stm401)) {
		ps_stm401->ignore_wakeable_interrupts = true;
		ret = 1;
	}
	mutex_unlock(&ps_stm401->qp_list_lock);
	mutex_unlock(&ps_stm401->lock);

	return ret;
}

static int stm401_quickpeek_status_ack(struct stm401_data *ps_stm401,
	struct stm401_quickpeek_message *qp_message, int ack_return)
{
	int ret = 0;
	unsigned char payload = ack_return & 0x03;
	unsigned int req_bit = atomic_read(&ps_stm401->qp_enabled) & 0x01;
	unsigned char cmdbuff[4];

	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	stm401_wake(ps_stm401);

	if (qp_message && qp_message->message == AOD_WAKEUP_REASON_QP_DRAW)
		payload |= (qp_message->buffer_id &
			AOD_QP_ACK_BUFFER_ID_MASK) << 2;

	cmdbuff[0] = STM401_PEEKSTATUS_REG;
	cmdbuff[1] = req_bit;
	cmdbuff[2] = qp_message ? qp_message->message : 0x00;
	cmdbuff[3] = payload;
	if (stm401_i2c_write(ps_stm401, cmdbuff, 4) < 0) {
		dev_err(&ps_stm401->client->dev,
			"Write peek status reg failed\n");
		ret = -EIO;
	}

	dev_dbg(&ps_stm401->client->dev, "%s: message: %d | req_bit: %d | ack_return: %d | buffer_id: %d | ret: %d\n",
		__func__,
		qp_message ? qp_message->message : 0, req_bit, ack_return,
		qp_message ? qp_message->buffer_id : 0, ret);

	stm401_sleep(ps_stm401);

	return ret;
}

int stm401_display_handle_touch_locked(struct stm401_data *ps_stm401)
{
	char *envp[2];
	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	envp[0] = "STM401WAKE=TOUCH";
	envp[1] = NULL;
	dev_info(&ps_stm401->client->dev,
		"Sending uevent, STM401 TOUCH wake\n");

	if (kobject_uevent_env(&ps_stm401->client->dev.kobj,
		KOBJ_CHANGE, envp)) {
		dev_err(&ps_stm401->client->dev,
			"Failed to create uevent\n");
		return -EIO;
	}

	return 0;
}

int stm401_display_handle_quickpeek_locked(struct stm401_data *ps_stm401,
	bool releaseWakelock)
{
	int ret = 0;
	u8 aod_qp_reason;
	u8 aod_qp_panel_state;
	struct stm401_quickpeek_message *qp_message;

	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	stm401_cmdbuff[0] = STM401_STATUS_REG;
	if (stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2)
		< 0) {
		dev_err(&ps_stm401->client->dev,
			"Get status reg failed\n");
		goto error;
	}

	aod_qp_panel_state = stm401_readbuff[0] & 0x3;
	aod_qp_reason = (stm401_readbuff[1] >> 4) & 0xf;

	qp_message = kzalloc(sizeof(*qp_message), GFP_KERNEL);
	if (!qp_message) {
		dev_err(&ps_stm401->client->dev,
			"%s: kzalloc failed!\n", __func__);
		goto error;
	}

	qp_message->panel_state = aod_qp_panel_state;
	qp_message->message = aod_qp_reason;

	switch (aod_qp_reason) {
	case AOD_WAKEUP_REASON_QP_PREPARE:
		dev_dbg(&ps_stm401->client->dev,
			"Received peek prepare command\n");
		break;
	case AOD_WAKEUP_REASON_QP_COMPLETE:
		dev_dbg(&ps_stm401->client->dev,
			"Received peek complete command\n");
		break;
	case AOD_WAKEUP_REASON_QP_DRAW:
		stm401_cmdbuff[0] = STM401_PEEKDATA_REG;
		if (stm401_i2c_write_read(ps_stm401, stm401_cmdbuff,
			1, 5) < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading peek draw data from STM failed\n");
			goto error;
		}
		qp_message->buffer_id = stm401_readbuff[0] & 0x3f;
		qp_message->x1 = stm401_readbuff[1] |
			stm401_readbuff[2] << 8;
		qp_message->y1 = stm401_readbuff[3] |
			stm401_readbuff[4] << 8;

		dev_dbg(&ps_stm401->client->dev,
			"Received peek draw command for buffer: %d (coord: %d, %d)\n",
			qp_message->buffer_id,
			qp_message->x1, qp_message->y1);
		break;
	case AOD_WAKEUP_REASON_QP_ERASE:
		stm401_cmdbuff[0] = STM401_PEEKDATA_REG;
		if (stm401_i2c_write_read(ps_stm401, stm401_cmdbuff,
			1, 9) < 0) {
			dev_err(&ps_stm401->client->dev,
				"Reading peek erase data from STM failed\n");
			goto error;
		}
		qp_message->x1 = stm401_readbuff[1] |
			stm401_readbuff[2] << 8;
		qp_message->y1 = stm401_readbuff[3] |
			stm401_readbuff[4] << 8;
		qp_message->x2 = stm401_readbuff[5] |
			stm401_readbuff[6] << 8;
		qp_message->y2 = stm401_readbuff[7] |
			stm401_readbuff[8] << 8;

		dev_dbg(&ps_stm401->client->dev,
			"Received peek erase command: (%d, %d) -> (%d, %d)\n",
			qp_message->x1, qp_message->y1,
			qp_message->x2, qp_message->y2);
		break;
	default:
		dev_err(&ps_stm401->client->dev,
			"Unknown quickpeek command [%d]!", aod_qp_reason);
		goto error;
	}

	if (!atomic_read(&ps_stm401->qp_enabled)) {
		dev_info(&ps_stm401->client->dev,
			"%s: Received quickpeek interrupt with quickpeek disabled!\n",
			__func__);
		goto error;
	}

	ps_stm401->quickpeek_occurred = true;

	mutex_lock(&ps_stm401->qp_list_lock);
	list_add_tail(&qp_message->list,
		&ps_stm401->quickpeek_command_list);
	queue_work(ps_stm401->quickpeek_work_queue,
		&ps_stm401->quickpeek_work);
	wake_lock(&ps_stm401->quickpeek_wakelock);
	mutex_unlock(&ps_stm401->qp_list_lock);

	stm401_quickpeek_status_ack(ps_stm401, qp_message,
		AOD_QP_ACK_RCVD);

exit:
	/* If this is only us, we dont need a full 1 sec */
	if (releaseWakelock)
		wake_unlock(&ps_stm401->wakelock);

	return ret;

error:
	stm401_quickpeek_status_ack(ps_stm401, qp_message, AOD_QP_ACK_INVALID);
	kfree(qp_message);
	ret = -EIO;
	goto exit;
}

void stm401_quickpeek_reset_locked(struct stm401_data *ps_stm401)
{
	struct stm401_quickpeek_message *entry, *entry_tmp;
	struct list_head temp_list;
	int ret = 0;

	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	/* Move the current list to a temp list so we can flush the queue */
	mutex_lock(&ps_stm401->qp_list_lock);
	list_replace_init(&ps_stm401->quickpeek_command_list, &temp_list);
	mutex_unlock(&ps_stm401->qp_list_lock);

	flush_work(&ps_stm401->quickpeek_work);

	/* From here, no quickpeek interrupts will be handled, because we have
	   the main lock. Also, the quickpeek workqueue is idle, so basically
	   nothing quickpeek is happening. Safe to do lots of things now. */

	if (ps_stm401->qp_prepared) {
		/* Cleanup fb driver state */
		ret = fb_quickdraw_cleanup();

		if (ret)
			dev_err(&ps_stm401->client->dev,
				"%s: Failed to cleanup (ret: %d)\n",
				__func__, ret);
	}

	list_for_each_entry_safe(entry, entry_tmp, &temp_list, list) {
		list_del(&entry->list);
		stm401_quickpeek_status_ack(ps_stm401, entry, AOD_QP_ACK_DONE);
		kfree(entry);
	}

	/* This is the ONLY place we should set this explicitly instead of using
	   the helper */
	ps_stm401->qp_prepared = false;
	ps_stm401->ignored_interrupts = 0;
	ps_stm401->ignore_wakeable_interrupts = false;

	wake_unlock(&ps_stm401->quickpeek_wakelock);
	wake_up_all(&ps_stm401->quickpeek_wait_queue);
}

/* Should always be called with qp_in_progress TRUE */
static int stm401_quickpeek_check_state(struct stm401_data *ps_stm401,
	struct stm401_quickpeek_message *qp_message)
{
	int ret = 0;

	BUG_ON(!ps_stm401->qp_in_progress);

	switch (qp_message->message) {
	case AOD_WAKEUP_REASON_QP_PREPARE:
		if (ps_stm401->qp_prepared)
			ret = -EINVAL;
		break;
	case AOD_WAKEUP_REASON_QP_DRAW:
		if (!ps_stm401->qp_prepared)
			ret = -EINVAL;
		break;
	case AOD_WAKEUP_REASON_QP_ERASE:
		if (!ps_stm401->qp_prepared)
			ret = -EINVAL;
		break;
	case AOD_WAKEUP_REASON_QP_COMPLETE:
		if (!ps_stm401->qp_prepared)
			ret = -EINVAL;
		break;
	default:
		dev_err(&ps_stm401->client->dev,
			"%s: Unknown quickpeek message: %d\n",
			__func__,
			qp_message->message);
		ret = -EINVAL;
		goto exit;
	}

	if (ret)
		dev_err(&ps_stm401->client->dev,
			"%s: ILLEGAL MESSAGE message:%d qp_prepared:%d\n",
			__func__, qp_message->message, ps_stm401->qp_prepared);

exit:
	return ret;
}

/* Take care to NEVER lock the ps_stm401->lock in this function */
void stm401_quickpeek_work_func(struct work_struct *work)
{
	struct stm401_data *ps_stm401 = container_of(work,
			struct stm401_data, quickpeek_work);
	int ret;

	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	stm401_wake(ps_stm401);

	mutex_lock(&ps_stm401->qp_list_lock);

	stm401_quickpeek_set_in_progress_locked(ps_stm401, true);

	while (atomic_read(&ps_stm401->qp_enabled) &&
	       !list_empty(&ps_stm401->quickpeek_command_list) &&
	       !stm401_misc_data->in_reset_and_init) {
		struct stm401_quickpeek_message *qp_message;
		int ack_return = AOD_QP_ACK_DONE;
		int x = -1;
		int y = -1;

		qp_message = list_first_entry(
			&ps_stm401->quickpeek_command_list,
			struct stm401_quickpeek_message, list);
		list_del(&qp_message->list);
		mutex_unlock(&ps_stm401->qp_list_lock);

		dev_dbg(&ps_stm401->client->dev, "%s: Handling message: %d\n",
			__func__, qp_message->message);

		if (stm401_quickpeek_check_state(ps_stm401, qp_message)) {
			ack_return = AOD_QP_ACK_INVALID;
			goto loop;
		}

		switch (qp_message->message) {
		case AOD_WAKEUP_REASON_QP_PREPARE:
			ret = fb_quickdraw_prepare(qp_message->panel_state);
			if (ret == QUICKDRAW_ESD_RECOVERED) {
				dev_err(&ps_stm401->client->dev,
					"%s: ESD Recovered: %d\n", __func__,
					ret);
				ack_return = AOD_QP_ACK_ESD_RECOVERED;
				stm401_quickpeek_set_prepared(ps_stm401, true);
			} else if (ret) {
				dev_err(&ps_stm401->client->dev,
					"%s: Prepare Error: %d\n", __func__,
					ret);
				ack_return = AOD_QP_ACK_INVALID;
			} else
				stm401_quickpeek_set_prepared(ps_stm401, true);
			break;
		case AOD_WAKEUP_REASON_QP_DRAW:
			if (qp_message->buffer_id >
				AOD_QP_DRAW_MAX_BUFFER_ID) {
				dev_err(&ps_stm401->client->dev,
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
			ret = fb_quickdraw_execute(
				qp_message->buffer_id, x, y);
			if (ret) {
				dev_err(&ps_stm401->client->dev,
					"%s: Failed to execute (ret: %d)\n",
					__func__, ret);
				ack_return = AOD_QP_ACK_INVALID;
			}
			break;
		case AOD_WAKEUP_REASON_QP_ERASE:
			if (qp_message->x2 <= qp_message->x1 ||
			    qp_message->y2 <= qp_message->y1) {
				dev_err(&ps_stm401->client->dev,
					"%s: ILLEGAL coordinates\n", __func__);
				ack_return = AOD_QP_ACK_INVALID;
				break;
			}
			ret = fb_quickdraw_erase(
				qp_message->x1, qp_message->y1,
				qp_message->x2, qp_message->y2);
			if (ret) {
				dev_err(&ps_stm401->client->dev,
					"%s: Failed to erase (ret: %d)\n",
					__func__, ret);
				ack_return = AOD_QP_ACK_INVALID;
			}
			break;
		case AOD_WAKEUP_REASON_QP_COMPLETE:
			ret = fb_quickdraw_cleanup();
			if (ret) {
				dev_err(&ps_stm401->client->dev,
					"%s: Failed to cleanup (ret: %d)\n",
					__func__, ret);
				ack_return = AOD_QP_ACK_INVALID;
			}
			stm401_quickpeek_set_prepared(ps_stm401, false);
			break;
		default:
			dev_err(&ps_stm401->client->dev,
				"%s: Unknown quickpeek message: %d\n",
				__func__,
				qp_message->message);
			break;
		}

loop:
		stm401_quickpeek_status_ack(ps_stm401, qp_message, ack_return);

		mutex_lock(&ps_stm401->qp_list_lock);
		kfree(qp_message);
	}

	if (!stm401_quickpeek_is_pending_locked(ps_stm401) &&
		!ps_stm401->qp_prepared) {
		wake_unlock(&ps_stm401->quickpeek_wakelock);
		wake_up_all(&ps_stm401->quickpeek_wait_queue);
	}

	stm401_quickpeek_set_in_progress_locked(ps_stm401, false);

	mutex_unlock(&ps_stm401->qp_list_lock);

	stm401_sleep(ps_stm401);
}

static int stm401_takeback_locked(struct stm401_data *ps_stm401)
{
	int count = 0;

	dev_dbg(&stm401_misc_data->client->dev, "%s\n", __func__);
	stm401_wake(ps_stm401);

	if (ps_stm401->mode == NORMALMODE) {
		stm401_quickpeek_reset_locked(ps_stm401);

		/* New I2C Implementation */
		stm401_cmdbuff[0] = STM401_PEEKSTATUS_REG;
		stm401_cmdbuff[1] = 0x00;
		if (stm401_i2c_write(ps_stm401, stm401_cmdbuff, 2) < 0) {
			dev_err(&ps_stm401->client->dev,
				"Write peek status reg failed\n");
			goto EXIT;
		}

		do {
			stm401_cmdbuff[0] = STM401_STATUS_REG;
			if (stm401_i2c_write_read(ps_stm401,
					stm401_cmdbuff, 1, 1) < 0) {
				dev_err(&ps_stm401->client->dev,
					"Get status reg failed\n");
				goto EXIT;
			}

			if (!(stm401_readbuff[0] & STM401_BUSY_STATUS_MASK))
				break;

			usleep_range(STM401_BUSY_SLEEP_USEC,
						STM401_BUSY_SLEEP_USEC);
			count++;
		} while (count < STM401_BUSY_RESUME_COUNT);

		if (count == STM401_BUSY_RESUME_COUNT)
			dev_err(&ps_stm401->client->dev,
				"timedout while waiting for STMBUSY LOW\n");
	} else
		dev_warn(&ps_stm401->client->dev,
			"takeback in invalid mode[%d]\n", ps_stm401->mode);

EXIT:
	stm401_sleep(ps_stm401);
	return 0;
}

static int stm401_handover_locked(struct stm401_data *ps_stm401)
{
	int ret = 0;

	dev_dbg(&stm401_misc_data->client->dev, "%s\n", __func__);
	stm401_wake(ps_stm401);

	if (ps_stm401->mode == NORMALMODE) {
		/* New I2C Implementation */
		stm401_cmdbuff[0] = STM401_PEEKSTATUS_REG;
		stm401_cmdbuff[1] = 0x01;
		if (stm401_i2c_write(ps_stm401, stm401_cmdbuff, 2) < 0) {
			dev_err(&ps_stm401->client->dev,
				"Write peek status reg failed\n");
			ret = -EIO;
		}
	} else
		dev_warn(&ps_stm401->client->dev,
			"handover in invalid mode[%d]\n", ps_stm401->mode);

	stm401_sleep(ps_stm401);
	return ret;
}

int stm401_resolve_aod_enabled_locked(struct stm401_data *ps_stm401)
{
	int ret = 0;

	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	mutex_lock(&ps_stm401->aod_enabled.vote_lock);

	if ((ps_stm401->aod_enabled.resolved_vote == AOD_QP_ENABLED_VOTE_MASK)
		!= (ps_stm401->aod_enabled.vote ==
			AOD_QP_ENABLED_VOTE_MASK)) {

		ps_stm401->aod_enabled.resolved_vote =
			ps_stm401->aod_enabled.vote;

		if (atomic_read(&ps_stm401->qp_enabled))
			ret = stm401_handover_locked(ps_stm401);
		else
			ret = stm401_takeback_locked(ps_stm401);
	}

	mutex_unlock(&ps_stm401->aod_enabled.vote_lock);

	return ret;
}

void stm401_vote_aod_enabled_locked(struct stm401_data *ps_stm401, int voter,
				    bool enable)
{
	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	mutex_lock(&ps_stm401->aod_enabled.vote_lock);

	stm401_store_vote_aod_enabled_locked(ps_stm401, voter, enable);

	if (ps_stm401->aod_enabled.vote == AOD_QP_ENABLED_VOTE_MASK)
		atomic_set(&ps_stm401->qp_enabled, 1);
	else
		atomic_set(&ps_stm401->qp_enabled, 0);

	mutex_unlock(&ps_stm401->aod_enabled.vote_lock);
}

void stm401_store_vote_aod_enabled(struct stm401_data *ps_stm401, int voter,
				    bool enable)
{
	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	mutex_lock(&ps_stm401->aod_enabled.vote_lock);
	stm401_store_vote_aod_enabled_locked(ps_stm401, voter, enable);
	mutex_unlock(&ps_stm401->aod_enabled.vote_lock);
}

void stm401_store_vote_aod_enabled_locked(struct stm401_data *ps_stm401,
	int voter, bool enable)
{
	voter &= AOD_QP_ENABLED_VOTE_MASK;
	if (enable)
		ps_stm401->aod_enabled.vote |= voter;
	else
		ps_stm401->aod_enabled.vote &= ~voter;
}

unsigned short stm401_get_interrupt_status(struct stm401_data *ps_stm401,
	unsigned char reg, int *err)
{
	stm401_wake(ps_stm401);

	stm401_cmdbuff[0] = reg;
	*err = stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);
	if (*err < 0) {
		dev_err(&ps_stm401->client->dev, "Reading from STM failed\n");
		stm401_sleep(ps_stm401);
		return 0;
	}

	stm401_sleep(ps_stm401);
	return (stm401_readbuff[1] << 8) | stm401_readbuff[0];
}

/* WARNING: This code is extrememly prone to race conditions. Be very careful
   modifying this code or the variables it uses to control flow. */

static int stm401_qw_check(void *data)
{
	struct stm401_data *ps_stm401 = (struct stm401_data *)data;
	unsigned short irq_status;
	int err, ret = 0;

	dev_dbg(&ps_stm401->client->dev, "%s\n", __func__);

	mutex_lock(&ps_stm401->lock);

	if (ps_stm401->quickpeek_occurred) {
		ret = 1;
		goto EXIT;
	}

	irq_status = stm401_get_interrupt_status(ps_stm401, WAKESENSOR_STATUS,
		&err);
	if (err < 0)
		goto EXIT;

	ps_stm401->qw_irq_status = irq_status;

	if (irq_status & M_QUICKPEEK) {
		queue_work(ps_stm401->irq_work_queue,
			&ps_stm401->irq_wake_work);
		ret = 1;
	}

EXIT:
	mutex_unlock(&ps_stm401->lock);

	return ret;
}

static int stm401_qw_execute(void *data)
{
	struct stm401_data *ps_stm401 = (struct stm401_data *)data;
	int ret = 1;

	if (!wait_event_timeout(ps_stm401->quickpeek_wait_queue,
		stm401_quickpeek_disable_when_idle(ps_stm401),
		msecs_to_jiffies(STM401_LATE_SUSPEND_TIMEOUT)))
		ret = 0;

	return ret;
}

static struct quickwakeup_ops stm401_quickwakeup_ops = {
	.name = NAME,
	.qw_execute = stm401_qw_execute,
	.qw_check   = stm401_qw_check,
};

void stm401_quickwakeup_init(struct stm401_data *ps_stm401)
{
	stm401_quickwakeup_ops.data = ps_stm401;
	quickwakeup_register(&stm401_quickwakeup_ops);
}
