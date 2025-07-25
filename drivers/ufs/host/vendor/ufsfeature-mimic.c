// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung UFS Feature Mimic
 *
 * from
 *
 * linux/drivers/ufs/core/ufshcd.c
 */

#include "ufsfeature.h"
#include <ufs/ufshcd.h>
#include "ufshcd-priv.h"
#include "drivers/ufs/core/ufshcd-crypto.h"
#include <trace/hooks/ufshcd.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/sched/clock.h>
#define CREATE_TRACE_POINTS
#include "ufsfeature-trace.h"

/* Max mcq register polling time in microseconds */
#define MCQ_POLL_US 500000

static inline void ufsf_utrl_clear(struct ufs_hba *hba, u32 mask)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		mask = ~mask;
	/*
	 * From the UFSHCI specification: "UTP Transfer Request List CLear
	 * Register (UTRLCLR): This field is bit significant. Each bit
	 * corresponds to a slot in the UTP Transfer Request List, where bit 0
	 * corresponds to request slot 0. A bit in this field is set to ‘0’
	 * by host software to indicate to the host controller that a transfer
	 * request slot is cleared. The host controller
	 * shall free up any resources associated to the request slot
	 * immediately, and shall set the associated bit in UTRLDBR to ‘0’. The
	 * host software indicates no change to request slots by setting the
	 * associated bits in this field to ‘1’. Bits in this field shall only
	 * be set ‘1’ or ‘0’ by host software when UTRLRSR is set to ‘1’."
	 */
	ufshcd_writel(hba, ~mask, REG_UTP_TRANSFER_REQ_LIST_CLEAR);
}

static void __iomem *ufsf_mcq_opr_base(struct ufs_hba *hba,
					 enum ufshcd_mcq_opr n, int i)
{
	struct ufshcd_mcq_opr_info_t *opr = &hba->mcq_opr[n];

	return opr->base + opr->stride * i;
}

static int ufsf_mcq_sq_start(struct ufs_hba *hba, struct ufs_hw_queue *hwq)
{
	void __iomem *reg;
	u32 id = hwq->id, val;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	writel(SQ_START, ufsf_mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTC);
	reg = ufsf_mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTS;
	err = read_poll_timeout(readl, val, !(val & SQ_STS), 20,
				MCQ_POLL_US, false, reg);
	if (err)
		dev_err(hba->dev, "%s: failed. hwq-id=%d, err=%d\n",
			__func__, id, err);
	return err;
}

static int ufsf_mcq_sq_stop(struct ufs_hba *hba, struct ufs_hw_queue *hwq)
{
	void __iomem *reg;
	u32 id = hwq->id, val;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	writel(SQ_STOP, ufsf_mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTC);
	reg = ufsf_mcq_opr_base(hba, OPR_SQD, id) + REG_SQRTS;
	err = read_poll_timeout(readl, val, val & SQ_STS, 20,
				MCQ_POLL_US, false, reg);
	if (err)
		dev_err(hba->dev, "%s: failed. hwq-id=%d, err=%d\n",
			__func__, id, err);
	return err;
}

struct ufs_hw_queue *ufsf_mcq_req_to_hwq(struct ufs_hba *hba,
					 struct request *req)
{
	struct blk_mq_hw_ctx *hctx = READ_ONCE(req->mq_hctx);

	return hctx ? &hba->uhq[hctx->queue_num] : NULL;
}

int ufsf_mcq_sq_cleanup(struct ufs_hba *hba, int task_tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct ufs_hw_queue *hwq;
	void __iomem *reg, *opr_sqd_base;
	u32 nexus, id, val, rtc;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	if (task_tag != hba->nutrs - UFSHCD_NUM_RESERVED) {
		if (!cmd)
			return -EINVAL;
		hwq = ufsf_mcq_req_to_hwq(hba, scsi_cmd_to_rq(cmd));
		if (!hwq)
			return 0;
	} else {
		hwq = hba->dev_cmd_queue;
	}

	id = hwq->id;

	mutex_lock(&hwq->sq_mutex);

	/* stop the SQ fetching before working on it */
	err = ufsf_mcq_sq_stop(hba, hwq);
	if (err)
		goto unlock;

	/* SQCTI = EXT_IID, IID, LUN, Task Tag */
	nexus = lrbp->lun << 8 | task_tag;
	opr_sqd_base = ufsf_mcq_opr_base(hba, OPR_SQD, id);
	writel(nexus, opr_sqd_base + REG_SQCTI);

	/* Initiate Cleanup */
	writel(readl(opr_sqd_base + REG_SQRTC) | SQ_ICU,
		opr_sqd_base + REG_SQRTC);

	/* Poll SQRTSy.CUS = 1. Return result from SQRTSy.RTC */
	reg = opr_sqd_base + REG_SQRTS;
	err = read_poll_timeout(readl, val, val & SQ_CUS, 20,
				MCQ_POLL_US, false, reg);
	rtc = FIELD_GET(SQ_ICU_ERR_CODE_MASK, readl(reg));
	if (err || rtc)
		dev_err(hba->dev, "%s: failed. hwq=%d, tag=%d err=%d RTC=%d\n",
			__func__, id, task_tag, err, rtc);

	if (ufsf_mcq_sq_start(hba, hwq))
		err = -ETIMEDOUT;

unlock:
	mutex_unlock(&hwq->sq_mutex);
	return err;
}

static int ufsf_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
				u32 val, unsigned long interval_us,
				unsigned long timeout_ms)
{
	int err = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);

	/* ignore bits that we don't intend to wait on */
	val = val & mask;

	while ((ufshcd_readl(hba, reg) & mask) != val) {
		usleep_range(interval_us, interval_us + 50);
		if (time_after(jiffies, timeout)) {
			if ((ufshcd_readl(hba, reg) & mask) != val)
				err = -ETIMEDOUT;
			break;
		}
	}

	return err;
}

static int ufsf_clear_cmd(struct ufs_hba *hba, u32 task_tag)
{
	u32 mask;
	unsigned long flags;
	int err;

	if (hba->mcq_enabled) {
		/*
		 * MCQ mode. Clean up the MCQ resources similar to
		 * what the ufsf_utrl_clear() does for SDB mode.
		 */
		err = ufsf_mcq_sq_cleanup(hba, task_tag);
		if (err) {
			dev_err(hba->dev, "%s: failed tag=%d. err=%d\n",
				__func__, task_tag, err);
			return err;
		}
		return 0;
	}

	mask = 1U << task_tag;

	/* clear outstanding transaction before retry */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufsf_utrl_clear(hba, mask);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/*
	 * wait for h/w to clear corresponding bit in door-bell.
	 * max. wait is 1 sec.
	 */
	return ufsf_wait_for_register(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL,
					mask, ~mask, 1000, 1000);
}

static
int ufsf_copy_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	memcpy(&query_res->upiu_res, &lrbp->ucd_rsp_ptr->qr, QUERY_OSF_SIZE);

	/* Get the descriptor */
	if (hba->dev_cmd.query.descriptor &&
	    lrbp->ucd_rsp_ptr->qr.opcode == UPIU_QUERY_OPCODE_READ_DESC) {
		u8 *descp = (u8 *)lrbp->ucd_rsp_ptr +
				GENERAL_UPIU_REQUEST_SIZE;
		u16 resp_len;
		u16 buf_len;

		/* data segment length */
		resp_len = be16_to_cpu(lrbp->ucd_rsp_ptr->header
				       .data_segment_length);
		buf_len = be16_to_cpu(
				hba->dev_cmd.query.request.upiu_req.length);
		if (likely(buf_len >= resp_len)) {
			memcpy(hba->dev_cmd.query.descriptor, descp, resp_len);
		} else {
			dev_warn(hba->dev,
				 "%s: rsp size %d is bigger than buffer size %d",
				 __func__, resp_len, buf_len);
			return -EINVAL;
		}
	}

	return 0;
}

static inline enum upiu_response_transaction
ufsf_get_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return ucd_rsp_ptr->header.transaction_code;
}

static int
ufsf_dev_cmd_completion(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	enum upiu_response_transaction resp;
	int err = 0;

	hba->ufs_stats.last_hibern8_exit_tstamp = ktime_set(0, 0);
	resp = ufsf_get_req_rsp(lrbp->ucd_rsp_ptr);

	switch (resp) {
	case UPIU_TRANSACTION_NOP_IN:
		if (hba->dev_cmd.type != DEV_CMD_TYPE_NOP) {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response %x\n",
					__func__, resp);
		}
		break;
	case UPIU_TRANSACTION_QUERY_RSP: {
		u8 response = lrbp->ucd_rsp_ptr->header.response;

		if (response == 0)
			err = ufsf_copy_query_response(hba, lrbp);
		break;
	}
	case UPIU_TRANSACTION_REJECT_UPIU:
		/* TODO: handle Reject UPIU Response */
		err = -EPERM;
		dev_err(hba->dev, "%s: Reject UPIU not fully implemented\n",
				__func__);
		break;
	case UPIU_TRANSACTION_RESPONSE:
		if (hba->dev_cmd.type != DEV_CMD_TYPE_RPMB) {
			err = -EINVAL;
			dev_err(hba->dev, "%s: unexpected response %x\n", __func__, resp);
		}
		break;
	default:
		err = -EINVAL;
		dev_err(hba->dev, "%s: Invalid device management cmd response: %x\n",
				__func__, resp);
		break;
	}

	return err;
}

enum utp_ocs ufsf_get_tr_ocs(struct ufshcd_lrb *lrbp,
				      struct cq_entry *cqe)
{
	if (cqe)
		return le32_to_cpu(cqe->status) & MASK_OCS;

	return lrbp->utr_descriptor_ptr->header.ocs & MASK_OCS;
}

static int ufsf_wait_for_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, int max_timeout)
{
	unsigned long time_left = msecs_to_jiffies(max_timeout);
	unsigned long flags;
	bool pending;
	int err;

retry:
	time_left = wait_for_completion_timeout(hba->dev_cmd.complete,
						time_left);

	if (likely(time_left)) {
		/*
		 * The completion handler called complete() and the caller of
		 * this function still owns the @lrbp tag so the code below does
		 * not trigger any race conditions.
		 */
		hba->dev_cmd.complete = NULL;
		err = ufsf_get_tr_ocs(lrbp, NULL);
		if (!err)
			err = ufsf_dev_cmd_completion(hba, lrbp);
	} else {
		err = -ETIMEDOUT;
		dev_dbg(hba->dev, "%s: dev_cmd request timedout, tag %d\n",
			__func__, lrbp->task_tag);

		/* MCQ mode */
		if (hba->mcq_enabled) {
			/* successfully cleared the command, retry if needed */
			if (ufsf_clear_cmd(hba, lrbp->task_tag) == 0)
				err = -EAGAIN;
			hba->dev_cmd.complete = NULL;
			return err;
		}

		/* SDB mode */
		if (ufsf_clear_cmd(hba, lrbp->task_tag) == 0) {
			/* successfully cleared the command, retry if needed */
			err = -EAGAIN;
			/*
			 * Since clearing the command succeeded we also need to
			 * clear the task tag bit from the outstanding_reqs
			 * variable.
			 */
			spin_lock_irqsave(&hba->outstanding_lock, flags);
			pending = test_bit(lrbp->task_tag,
					   &hba->outstanding_reqs);
			if (pending) {
				hba->dev_cmd.complete = NULL;
				__clear_bit(lrbp->task_tag,
					    &hba->outstanding_reqs);
			}
			spin_unlock_irqrestore(&hba->outstanding_lock, flags);

			if (!pending) {
				/*
				 * The completion handler ran while we tried to
				 * clear the command.
				 */
				time_left = 1;
				goto retry;
			}
		} else {
			dev_err(hba->dev, "%s: failed to clear tag %d\n",
				__func__, lrbp->task_tag);

			spin_lock_irqsave(&hba->outstanding_lock, flags);
			pending = test_bit(lrbp->task_tag,
					   &hba->outstanding_reqs);
			if (pending)
				hba->dev_cmd.complete = NULL;
			spin_unlock_irqrestore(&hba->outstanding_lock, flags);

			if (!pending) {
				/*
				 * The completion handler ran while we tried to
				 * clear the command.
				 */
				time_left = 1;
				goto retry;
			}
		}
	}

	return err;
}

static inline int ufsf_monitor_opcode2dir(u8 opcode)
{
	if (opcode == READ_6 || opcode == READ_10 || opcode == READ_16)
		return READ;
	else if (opcode == WRITE_6 || opcode == WRITE_10 || opcode == WRITE_16)
		return WRITE;
	else
		return -EINVAL;
}

static void ufsf_start_monitor(struct ufs_hba *hba,
			       const struct ufshcd_lrb *lrbp)
{
	int dir = ufsf_monitor_opcode2dir(*lrbp->cmd->cmnd);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (dir >= 0 && hba->monitor.nr_queued[dir]++ == 0)
		hba->monitor.busy_start_ts[dir] = ktime_get();
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static inline bool ufsf_should_inform_monitor(struct ufs_hba *hba,
					      struct ufshcd_lrb *lrbp)
{
	const struct ufs_hba_monitor *m = &hba->monitor;

	return (m->enabled && lrbp && lrbp->cmd &&
		(!m->chunk_size || m->chunk_size == lrbp->cmd->sdb.length) &&
		ktime_before(hba->monitor.enabled_ts, lrbp->issue_time_stamp));
}

static void ufsf_clk_scaling_start_busy(struct ufs_hba *hba)
{
	bool queue_resume_work = false;
	ktime_t curr_t = ktime_get();
	unsigned long flags;

	if (!ufshcd_is_clkscaling_supported(hba))
		return;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (!hba->clk_scaling.active_reqs++)
		queue_resume_work = true;

	if (!hba->clk_scaling.is_enabled || hba->pm_op_in_progress) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		return;
	}

	if (queue_resume_work)
		queue_work(hba->clk_scaling.workq,
			   &hba->clk_scaling.resume_work);

	if (!hba->clk_scaling.window_start_t) {
		hba->clk_scaling.window_start_t = curr_t;
		hba->clk_scaling.tot_busy_t = 0;
		hba->clk_scaling.is_busy_started = false;
	}

	if (!hba->clk_scaling.is_busy_started) {
		hba->clk_scaling.busy_start_t = curr_t;
		hba->clk_scaling.is_busy_started = true;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static void ufsf_add_cmd_upiu_trace(struct ufs_hba *hba, unsigned int tag,
				    enum ufs_trace_str_t str_t)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;
	struct utp_upiu_header *header;

	if (!trace_ufsfeature_upiu_enabled())
		return;

	if (str_t == UFS_CMD_SEND)
		header = &rq->header;
	else
		header = &hba->lrb[tag].ucd_rsp_ptr->header;

	trace_ufsfeature_upiu(hba, str_t, header, &rq->sc.cdb,
			  UFS_TSF_CDB);
}

static void ufsf_add_command_trace(struct ufs_hba *hba, unsigned int tag,
				   enum ufs_trace_str_t str_t)
{
	u64 lba = 0;
	u8 opcode = 0, group_id = 0;
	u32 doorbell = 0;
	u32 intr;
	int hwq_id = -1;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);
	int transfer_len = -1;

	if (!cmd)
		return;

	/* trace UPIU also */
	ufsf_add_cmd_upiu_trace(hba, tag, str_t);
	if (!trace_ufsfeature_command_enabled())
		return;

	opcode = cmd->cmnd[0];

	if (opcode == READ_10 || opcode == WRITE_10) {
		/*
		 * Currently we only fully trace read(10) and write(10) commands
		 */
		transfer_len =
		       be32_to_cpu(lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
		lba = scsi_get_lba(cmd);
		if (opcode == WRITE_10)
			group_id = lrbp->cmd->cmnd[6];
	} else if (opcode == UNMAP) {
		/*
		 * The number of Bytes to be unmapped beginning with the lba.
		 */
		transfer_len = blk_rq_bytes(rq);
		lba = scsi_get_lba(cmd);
	}

	intr = ufshcd_readl(hba, REG_INTERRUPT_STATUS);

	if (hba->mcq_enabled) {
		struct ufs_hw_queue *hwq = ufsf_mcq_req_to_hwq(hba, rq);

		hwq_id = hwq->id;
	} else {
		doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	}

	trace_ufsfeature_command(cmd->device, hba, str_t, tag, doorbell, hwq_id,
			     transfer_len, intr, lba, opcode, group_id);
}

static inline
void ufsf_send_command(struct ufs_hba *hba, unsigned int task_tag,
		       struct ufs_hw_queue *hwq)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	unsigned long flags;

	lrbp->issue_time_stamp = ktime_get();
	lrbp->issue_time_stamp_local_clock = local_clock();
	lrbp->compl_time_stamp = ktime_set(0, 0);
	lrbp->compl_time_stamp_local_clock = 0;
	trace_android_vh_ufs_send_command(hba, lrbp);
	ufsf_add_command_trace(hba, task_tag, UFS_CMD_SEND);
	if (lrbp->cmd)
		ufsf_clk_scaling_start_busy(hba);
	if (unlikely(ufsf_should_inform_monitor(hba, lrbp)))
		ufsf_start_monitor(hba, lrbp);

	if (hba->mcq_enabled) {
		int utrd_size = sizeof(struct utp_transfer_req_desc);
		struct utp_transfer_req_desc *src = lrbp->utr_descriptor_ptr;
		struct utp_transfer_req_desc *dest;

		spin_lock(&hwq->sq_lock);
		dest = hwq->sqe_base_addr + hwq->sq_tail_slot;
		memcpy(dest, src, utrd_size);
		ufshcd_inc_sq_tail(hwq);
		spin_unlock(&hwq->sq_lock);
	} else {
		spin_lock_irqsave(&hba->outstanding_lock, flags);
		if (hba->vops && hba->vops->setup_xfer_req)
			hba->vops->setup_xfer_req(hba, lrbp->task_tag,
						  !!lrbp->cmd);
		__set_bit(lrbp->task_tag, &hba->outstanding_reqs);
		ufshcd_writel(hba, 1 << lrbp->task_tag,
			      REG_UTP_TRANSFER_REQ_DOOR_BELL);
		spin_unlock_irqrestore(&hba->outstanding_lock, flags);
	}
}

static inline void ufsf_prepare_utp_nop_upiu(struct ufshcd_lrb *lrbp)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;

	memset(ucd_req_ptr, 0, sizeof(struct utp_upiu_req));

	ucd_req_ptr->header = (struct utp_upiu_header){
		.transaction_code = UPIU_TRANSACTION_NOP_OUT,
		.task_tag = lrbp->task_tag,
	};

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static void ufsf_prepare_utp_query_req_upiu(struct ufs_hba *hba,
					    struct ufshcd_lrb *lrbp,
					    u8 upiu_flags)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	struct ufs_query *query = &hba->dev_cmd.query;
	u16 len = be16_to_cpu(query->request.upiu_req.length);

	/* Query request header */
	ucd_req_ptr->header = (struct utp_upiu_header){
		.transaction_code = UPIU_TRANSACTION_QUERY_REQ,
		.flags = upiu_flags,
		.lun = lrbp->lun,
		.task_tag = lrbp->task_tag,
		.query_function = query->request.query_func,
		/* Data segment length only need for WRITE_DESC */
		.data_segment_length =
			query->request.upiu_req.opcode ==
					UPIU_QUERY_OPCODE_WRITE_DESC ?
				cpu_to_be16(len) :
				0,
	};

	/* Copy the Query Request buffer as is */
	memcpy(&ucd_req_ptr->qr, &query->request.upiu_req,
			QUERY_OSF_SIZE);

	/* Copy the Descriptor */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		memcpy(ucd_req_ptr + 1, query->descriptor, len);

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static void
ufsf_prepare_req_desc_hdr(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			    u8 *upiu_flags, enum dma_data_direction cmd_dir,
			    int ehs_length)
{
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	struct request_desc_header *h = &req_desc->header;
	enum utp_data_direction data_direction;

	lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	*h = (typeof(*h)){ };

	if (cmd_dir == DMA_FROM_DEVICE) {
		data_direction = UTP_DEVICE_TO_HOST;
		*upiu_flags = UPIU_CMD_FLAGS_READ;
	} else if (cmd_dir == DMA_TO_DEVICE) {
		data_direction = UTP_HOST_TO_DEVICE;
		*upiu_flags = UPIU_CMD_FLAGS_WRITE;
	} else {
		data_direction = UTP_NO_DATA_TRANSFER;
		*upiu_flags = UPIU_CMD_FLAGS_NONE;
	}

	h->command_type = lrbp->command_type;
	h->data_direction = data_direction;
	h->ehs_length = ehs_length;

	if (lrbp->intr_cmd)
		h->interrupt = 1;

	/* Prepare crypto related dwords */
	ufshcd_prepare_req_desc_hdr_crypto(lrbp, h);

	/*
	 * assigning invalid value for command status. Controller
	 * updates OCS on command completion, with the command
	 * status
	 */
	h->ocs = OCS_INVALID_COMMAND_STATUS;

	req_desc->prd_table_length = 0;
}

static int ufsf_compose_devman_upiu(struct ufs_hba *hba,
				    struct ufshcd_lrb *lrbp)
{
	u8 upiu_flags;
	int ret = 0;

	ufsf_prepare_req_desc_hdr(hba, lrbp, &upiu_flags, DMA_NONE, 0);

	if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
		ufsf_prepare_utp_query_req_upiu(hba, lrbp, upiu_flags);
	else if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
		ufsf_prepare_utp_nop_upiu(lrbp);
	else
		ret = -EINVAL;

	return ret;
}

static void ufsf_setup_dev_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			     enum dev_cmd_type cmd_type, u8 lun, int tag)
{
	lrbp->cmd = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = lun;
	lrbp->intr_cmd = true; /* No interrupt aggregation */
	ufshcd_prepare_lrbp_crypto(NULL, lrbp);
	hba->dev_cmd.type = cmd_type;
}

static int ufsf_compose_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, enum dev_cmd_type cmd_type, int tag)
{
	ufsf_setup_dev_cmd(hba, lrbp, cmd_type, 0, tag);

	return ufsf_compose_devman_upiu(hba, lrbp);
}

static void ufsf_add_query_upiu_trace(struct ufs_hba *hba,
				      enum ufs_trace_str_t str_t,
				      struct utp_upiu_req *rq_rsp)
{
	if (!trace_ufsfeature_upiu_enabled())
		return;

	trace_ufsfeature_upiu(hba, str_t, &rq_rsp->header,
			  &rq_rsp->qr, UFS_TSF_OSF);
}

static int ufsf_issue_dev_cmd(struct ufs_hba *hba, struct ufshcd_lrb *lrbp,
			  const u32 tag, int timeout)
{
	DECLARE_COMPLETION_ONSTACK(wait);
	int err;

	hba->dev_cmd.complete = &wait;

	ufsf_add_query_upiu_trace(hba, UFS_QUERY_SEND, lrbp->ucd_req_ptr);

	ufsf_send_command(hba, tag, hba->dev_cmd_queue);
	err = ufsf_wait_for_dev_cmd(hba, lrbp, timeout);

	ufsf_add_query_upiu_trace(hba, err ? UFS_QUERY_ERR : UFS_QUERY_COMP,
				    (struct utp_upiu_req *)lrbp->ucd_rsp_ptr);

	return err;
}

/**
 * ufsf_exec_dev_cmd - API for sending device management requests
 * @hba: UFS hba
 * @cmd_type: specifies the type (NOP, Query...)
 * @timeout: timeout in milliseconds
 *
 * Return: 0 upon success; < 0 upon failure.
 *
 * NOTE: Since there is only one available tag for device management commands,
 * it is expected you hold the hba->dev_cmd.lock mutex.
 */
static int ufsf_exec_dev_cmd(struct ufs_hba *hba,
		enum dev_cmd_type cmd_type, int timeout)
{
	const u32 tag = hba->reserved_slot;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	int err;

	/* Protects use of hba->reserved_slot. */
	lockdep_assert_held(&hba->dev_cmd.lock);

	err = ufsf_compose_dev_cmd(hba, lrbp, cmd_type, tag);
	if (unlikely(err))
		return err;

	return ufsf_issue_dev_cmd(hba, lrbp, tag, timeout);
}

static inline void ufsf_init_query(struct ufs_hba *hba,
		struct ufs_query_req **request, struct ufs_query_res **response,
		enum query_opcode opcode, u8 idn, u8 index, u8 selector)
{
	*request = &hba->dev_cmd.query.request;
	*response = &hba->dev_cmd.query.response;
	memset(*request, 0, sizeof(struct ufs_query_req));
	memset(*response, 0, sizeof(struct ufs_query_res));
	(*request)->upiu_req.opcode = opcode;
	(*request)->upiu_req.idn = idn;
	(*request)->upiu_req.index = index;
	(*request)->upiu_req.selector = selector;
}

static void ufsf_dev_man_lock(struct ufs_hba *hba)
{
	ufshcd_hold(hba);
	mutex_lock(&hba->dev_cmd.lock);
	down_read(&hba->clk_scaling_lock);
}

static void ufsf_dev_man_unlock(struct ufs_hba *hba)
{
	up_read(&hba->clk_scaling_lock);
	mutex_unlock(&hba->dev_cmd.lock);
	ufshcd_release(hba);
}

int ufsf_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 index, u8 selector, bool *flag_res)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;
	int timeout = QUERY_REQ_TIMEOUT;

	BUG_ON(!hba);

	ufsf_dev_man_lock(hba);

	ufsf_init_query(hba, &request, &response, opcode, idn, index,
			selector);

	switch (opcode) {
	case UPIU_QUERY_OPCODE_SET_FLAG:
	case UPIU_QUERY_OPCODE_CLEAR_FLAG:
	case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_WRITE_REQUEST;
		break;
	case UPIU_QUERY_OPCODE_READ_FLAG:
		request->query_func = UPIU_QUERY_FUNC_STANDARD_READ_REQUEST;
		if (!flag_res) {
			/* No dummy reads */
			dev_err(hba->dev, "%s: Invalid argument for read request\n",
					__func__);
			err = -EINVAL;
			goto out_unlock;
		}
		break;
	default:
		dev_err(hba->dev,
			"%s: Expected query flag opcode but got = %d\n",
			__func__, opcode);
		err = -EINVAL;
		goto out_unlock;
	}

	err = ufsf_exec_dev_cmd(hba, DEV_CMD_TYPE_QUERY, timeout);

	if (err) {
		dev_err(hba->dev,
			"%s: Sending flag query for idn %d failed, err = %d\n",
			__func__, idn, err);
		goto out_unlock;
	}

	if (flag_res)
		*flag_res = (be32_to_cpu(response->upiu_res.value) &
				MASK_QUERY_UPIU_FLAG_LOC) & 0x1;

out_unlock:
	ufsf_dev_man_unlock(hba);
	return err;
}

void ufsf_scsi_unblock_requests(struct ufs_hba *hba)
{
	if (atomic_dec_and_test(&hba->scsi_block_reqs_cnt))
		scsi_unblock_requests(hba->host);
}

void ufsf_scsi_block_requests(struct ufs_hba *hba)
{
	if (atomic_inc_return(&hba->scsi_block_reqs_cnt) == 1)
		scsi_block_requests(hba->host);
}

static u32 ufsf_pending_cmds(struct ufs_hba *hba)
{
	const struct scsi_device *sdev;
	u32 pending = 0;

	lockdep_assert_held(hba->host->host_lock);
	__shost_for_each_device(sdev, hba->host)
		pending += sbitmap_weight(&sdev->budget_map);

	return pending;
}

int ufsf_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us)
{
	unsigned long flags;
	int ret = 0;
	u32 tm_doorbell;
	u32 tr_pending;
	bool timeout = false, do_last_check = false;
	ktime_t start;

	ufshcd_hold(hba);
	spin_lock_irqsave(hba->host->host_lock, flags);
	/*
	 * Wait for all the outstanding tasks/transfer requests.
	 * Verify by checking the doorbell registers are clear.
	 */
	start = ktime_get();
	do {
		if (hba->ufshcd_state != UFSHCD_STATE_OPERATIONAL) {
			ret = -EBUSY;
			goto out;
		}

		tm_doorbell = ufshcd_readl(hba, REG_UTP_TASK_REQ_DOOR_BELL);
		tr_pending = ufsf_pending_cmds(hba);
		if (!tm_doorbell && !tr_pending) {
			timeout = false;
			break;
		} else if (do_last_check) {
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		io_schedule_timeout(msecs_to_jiffies(20));
		if (ktime_to_us(ktime_sub(ktime_get(), start)) >
		    wait_timeout_us) {
			timeout = true;
			/*
			 * We might have scheduled out for long time so make
			 * sure to check if doorbells are cleared by this time
			 * or not.
			 */
			do_last_check = true;
		}
		spin_lock_irqsave(hba->host->host_lock, flags);
	} while (tm_doorbell || tr_pending);

	if (timeout) {
		dev_err(hba->dev,
			"%s: timedout waiting for doorbell to clear (tm=0x%x, tr=0x%x)\n",
			__func__, tm_doorbell, tr_pending);
		ret = -EBUSY;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_release(hba);
	return ret;
}

int ufsf_get_bkops_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_BKOPS_STATUS, 0, 0, status);
}

static inline int ufsf_get_ee_status(struct ufs_hba *hba, u32 *status)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			QUERY_ATTR_IDN_EE_STATUS, 0, 0, status);
}

int __ufsf_write_ee_control(struct ufs_hba *hba, u32 ee_ctrl_mask)
{
	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				       QUERY_ATTR_IDN_EE_CONTROL, 0, 0,
				       &ee_ctrl_mask);
}

static int ufsf_update_ee_control(struct ufs_hba *hba, u16 *mask,
				  const u16 *other_mask, u16 set, u16 clr)
{
	u16 new_mask, ee_ctrl_mask;
	int err = 0;

	mutex_lock(&hba->ee_ctrl_mutex);
	new_mask = (*mask & ~clr) | set;
	ee_ctrl_mask = new_mask | *other_mask;
	if (ee_ctrl_mask != hba->ee_ctrl_mask)
		err = __ufsf_write_ee_control(hba, ee_ctrl_mask);
	/* Still need to update 'mask' even if 'ee_ctrl_mask' was unchanged */
	if (!err) {
		hba->ee_ctrl_mask = ee_ctrl_mask;
		*mask = new_mask;
	}
	mutex_unlock(&hba->ee_ctrl_mutex);
	return err;
}

int ufsf_update_ee_drv_mask(struct ufs_hba *hba,
					  u16 set, u16 clr)
{
	return ufsf_update_ee_control(hba, &hba->ee_drv_mask,
				      &hba->ee_usr_mask, set, clr);
}

int ufsf_enable_ee(struct ufs_hba *hba, u16 mask)
{
	return ufsf_update_ee_drv_mask(hba, mask, 0);
}

int ufsf_disable_ee(struct ufs_hba *hba, u16 mask)
{
	return ufsf_update_ee_drv_mask(hba, 0, mask);
}

static int ufsf_enable_auto_bkops(struct ufs_hba *hba)
{
	int err = 0;

	if (hba->auto_bkops_enabled)
		goto out;

	err = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG,
			QUERY_FLAG_IDN_BKOPS_EN, 0, NULL);
	if (err) {
		dev_err(hba->dev, "%s: failed to enable bkops %d\n",
				__func__, err);
		goto out;
	}

	hba->auto_bkops_enabled = true;
	dev_info(hba->dev, "%s: auto bkops - Enabled\n", __func__);

	/* No need of URGENT_BKOPS exception from the device */
	err = ufsf_disable_ee(hba, MASK_EE_URGENT_BKOPS);
	if (err)
		dev_err(hba->dev, "%s: failed to disable exception event %d\n",
				__func__, err);
out:
	return err;
}

static void ufsf_bkops_exception_event_handler(struct ufs_hba *hba)
{
	int err;
	u32 curr_status = 0;

	if (hba->is_urgent_bkops_lvl_checked)
		goto enable_auto_bkops;

	err = ufsf_get_bkops_status(hba, &curr_status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get BKOPS status %d\n",
				__func__, err);
		goto out;
	}

	/*
	 * We are seeing that some devices are raising the urgent bkops
	 * exception events even when BKOPS status doesn't indicate performace
	 * impacted or critical. Handle these device by determining their urgent
	 * bkops status at runtime.
	 */
	if (curr_status < BKOPS_STATUS_PERF_IMPACT) {
		dev_err(hba->dev, "%s: device raised urgent BKOPS exception for bkops status %d\n",
				__func__, curr_status);
		/* update the current status as the urgent bkops level */
		hba->urgent_bkops_lvl = curr_status;
		hba->is_urgent_bkops_lvl_checked = true;
	}

enable_auto_bkops:
	err = ufsf_enable_auto_bkops(hba);
out:
	if (err < 0)
		dev_err(hba->dev, "%s: failed to handle urgent bkops %d\n",
				__func__, err);
}

static void ufsf_temp_exception_event_handler(struct ufs_hba *hba, u16 status)
{
	u32 value;

	if (ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				QUERY_ATTR_IDN_CASE_ROUGH_TEMP, 0, 0, &value))
		return;

	dev_info(hba->dev, "exception Tcase %d\n", value - 80);

	ufs_hwmon_notify_event(hba, status & MASK_EE_URGENT_TEMP);

	/*
	 * A placeholder for the platform vendors to add whatever additional
	 * steps required
	 */
}

/**
 * ufsf_exception_event_handler - handle exceptions raised by device
 * @work: pointer to work data
 *
 * Read bExceptionEventStatus attribute from the device and handle the
 * exception event accordingly.
 */
void ufsf_exception_event_handler(struct ufs_hba *hba)
{
	int err;
	u32 status = 0;

	ufsf_scsi_block_requests(hba);
	err = ufsf_get_ee_status(hba, &status);
	if (err) {
		dev_err(hba->dev, "%s: failed to get exception status %d\n",
				__func__, err);
		goto out;
	}

	dev_info(hba->dev, "%s: status 0x%x\n", __func__, status);

	if (status & hba->ee_drv_mask & MASK_EE_URGENT_BKOPS)
		ufsf_bkops_exception_event_handler(hba);

	if (status & hba->ee_drv_mask & MASK_EE_URGENT_TEMP)
		ufsf_temp_exception_event_handler(hba, status);
out:
	ufsf_scsi_unblock_requests(hba);
}

MODULE_LICENSE("GPL v2");
