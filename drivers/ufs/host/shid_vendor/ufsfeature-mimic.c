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
#include "../core/ufshcd-priv.h"
#include "../core/ufshcd-crypto.h"
#include <trace/hooks/ufshcd.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/sched/clock.h>
#define CREATE_TRACE_POINTS
#include "ufsfeature-trace.h"

/* Max mcq register polling time in microseconds */
#define MCQ_POLL_US 500000

/* Task management command timeout */
#define TM_CMD_TIMEOUT	100 /* msecs */

/**
 * ufsf_wait_for_register - wait for register value to change
 * @hba: per-adapter interface
 * @reg: mmio register offset
 * @mask: mask to apply to the read register value
 * @val: value to wait for
 * @interval_us: polling interval in microseconds
 * @timeout_ms: timeout in milliseconds
 *
 * Return:
 * -ETIMEDOUT on error, zero on success.
 */
int ufsf_wait_for_register(struct ufs_hba *hba, u32 reg, u32 mask,
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

/**
 * ufsf_utmrl_clear - Clear a bit in UTRMLCLR register
 * @hba: per adapter instance
 * @pos: position of the bit to be cleared
 */
static inline void ufsf_utmrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos), REG_UTP_TASK_REQ_LIST_CLEAR);
}

static int ufsf_clear_tm_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	u32 mask = 1 << tag;
	unsigned long flags;

	if (!test_bit(tag, &hba->outstanding_tasks))
		goto out;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ufsf_utmrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/* poll for max. 1 sec to clear door bell register by h/w */
	err = ufsf_wait_for_register(hba,
			REG_UTP_TASK_REQ_DOOR_BELL,
			mask, 0, 1000, 1000);

	dev_err(hba->dev, "Clearing task management function with tag %d %s\n",
		tag, err < 0 ? "failed" : "succeeded");

out:
	return err;
}

static void ufsf_add_tm_upiu_trace(struct ufs_hba *hba, unsigned int tag,
				   enum ufs_trace_str_t str_t)
{
	trace_android_vh_ufs_send_tm_command(hba, tag, (int)str_t);

	if (str_t == UFS_TM_SEND)
		dev_info(hba->dev, "%s: UFS_TSF_TM_INPUT\n", __func__);
	else
		dev_info(hba->dev, "%s: UFS_TSF_TM_OUTPUT\n", __func__);
}

static int __ufsf_issue_tm_cmd(struct ufs_hba *hba,
			       struct utp_task_req_desc *treq, u8 tm_function)
{
	struct request_queue *q = hba->tmf_queue;
	struct Scsi_Host *host = hba->host;
	DECLARE_COMPLETION_ONSTACK(wait);
	struct request *req;
	unsigned long flags;
	int task_tag, err;

	/*
	 * blk_mq_alloc_request() is used here only to get a free tag.
	 */
	req = blk_mq_alloc_request(q, REQ_OP_DRV_OUT, 0);
	if (IS_ERR(req))
		return PTR_ERR(req);

	req->end_io_data = &wait;
	ufshcd_hold(hba);

	spin_lock_irqsave(host->host_lock, flags);

	task_tag = req->tag;
	WARN_ONCE(task_tag < 0 || task_tag >= hba->nutmrs, "Invalid tag %d\n",
		  task_tag);
	hba->tmf_rqs[req->tag] = req;
	treq->upiu_req.req_header.dword_0 |= cpu_to_be32(task_tag);

	memcpy(hba->utmrdl_base_addr + task_tag, treq, sizeof(*treq));
	ufshcd_vops_setup_task_mgmt(hba, task_tag, tm_function);

	/* send command to the controller */
	__set_bit(task_tag, &hba->outstanding_tasks);

	ufshcd_writel(hba, 1 << task_tag, REG_UTP_TASK_REQ_DOOR_BELL);
	/* Make sure that doorbell is committed immediately */
	wmb();

	spin_unlock_irqrestore(host->host_lock, flags);

	ufsf_add_tm_upiu_trace(hba, task_tag, UFS_TM_SEND);

	/* wait until the task management command is completed */
	err = wait_for_completion_io_timeout(&wait,
			msecs_to_jiffies(TM_CMD_TIMEOUT));
	if (!err) {
		ufsf_add_tm_upiu_trace(hba, task_tag, UFS_TM_ERR);
		dev_err(hba->dev, "%s: task management cmd 0x%.2x timed-out\n",
				__func__, tm_function);
		if (ufsf_clear_tm_cmd(hba, task_tag))
			dev_WARN(hba->dev, "%s: unable to clear tm cmd (slot %d) after timeout\n",
					__func__, task_tag);
		err = -ETIMEDOUT;
	} else {
		err = 0;
		memcpy(treq, hba->utmrdl_base_addr + task_tag, sizeof(*treq));

		ufsf_add_tm_upiu_trace(hba, task_tag, UFS_TM_COMP);
	}

	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->tmf_rqs[req->tag] = NULL;
	__clear_bit(task_tag, &hba->outstanding_tasks);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	ufshcd_release(hba);
	blk_mq_free_request(req);

	return err;
}

/**
 * ufsf_issue_tm_cmd - issues task management commands to controller
 * @hba: per adapter instance
 * @lun_id: LUN ID to which TM command is sent
 * @task_id: task ID to which the TM command is applicable
 * @tm_function: task management function opcode
 * @tm_response: task management service response return value
 *
 * Returns non-zero value on error, zero on success.
 */
int ufsf_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		      u8 tm_function, u8 *tm_response)
{
	struct utp_task_req_desc treq = { };
	enum utp_ocs ocs_value;
	int err;

	/* Configure task request descriptor */
	treq.header.interrupt = 1;
	treq.header.ocs = OCS_INVALID_COMMAND_STATUS;

	/* Configure task request UPIU */
	treq.upiu_req.req_header.transaction_code = UPIU_TRANSACTION_TASK_REQ;
	treq.upiu_req.req_header.lun = lun_id;
	treq.upiu_req.req_header.tm_function = tm_function;

	/*
	 * The host shall provide the same value for LUN field in the basic
	 * header and for Input Parameter.
	 */
	treq.upiu_req.input_param1 = cpu_to_be32(lun_id);
	treq.upiu_req.input_param2 = cpu_to_be32(task_id);

	err = __ufsf_issue_tm_cmd(hba, &treq, tm_function);
	if (err == -ETIMEDOUT)
		return err;

	ocs_value = treq.header.ocs & MASK_OCS;
	if (ocs_value != OCS_SUCCESS)
		dev_err(hba->dev, "%s: failed, ocs = 0x%x\n",
				__func__, ocs_value);
	else if (tm_response)
		*tm_response = be32_to_cpu(treq.upiu_rsp.output_param1) &
				MASK_TM_SERVICE_RESP;
	return err;
}

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
	u32 utag = blk_mq_unique_tag(req);
	u32 hwq = blk_mq_unique_tag_to_hwq(utag);

	return &hba->uhq[hwq];
}

int ufsf_mcq_sq_cleanup(struct ufs_hba *hba, int task_tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct ufs_hw_queue *hwq;
	void __iomem *reg, *opr_sqd_base;
	u32 nexus, id, val;
	int err;

	if (hba->quirks & UFSHCD_QUIRK_MCQ_BROKEN_RTC)
		return -ETIMEDOUT;

	if (task_tag != hba->nutrs - UFSHCD_NUM_RESERVED) {
		if (!cmd)
			return -EINVAL;
		hwq = ufsf_mcq_req_to_hwq(hba, scsi_cmd_to_rq(cmd));
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

	/* SQRTCy.ICU = 1 */
	writel(SQ_ICU, opr_sqd_base + REG_SQRTC);

	/* Poll SQRTSy.CUS = 1. Return result from SQRTSy.RTC */
	reg = opr_sqd_base + REG_SQRTS;
	err = read_poll_timeout(readl, val, val & SQ_CUS, 20,
				MCQ_POLL_US, false, reg);
	if (err)
		dev_err(hba->dev, "%s: failed. hwq=%d, tag=%d err=%ld\n",
			__func__, id, task_tag,
			FIELD_GET(SQ_ICU_ERR_CODE_MASK, readl(reg)));

	if (ufsf_mcq_sq_start(hba, hwq))
		err = -ETIMEDOUT;

unlock:
	mutex_unlock(&hwq->sq_mutex);
	return err;
}

/**
 * ufsf_get_req_rsp - returns the TR response transaction type
 * @ucd_rsp_ptr: pointer to response UPIU
 *
 * Return: UPIU type.
 */
enum upiu_response_transaction ufsf_get_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return ucd_rsp_ptr->header.transaction_code;
}

enum utp_ocs ufsf_get_tr_ocs(struct ufshcd_lrb *lrbp,
				    struct cq_entry *cqe)
{
	if (cqe)
		return le32_to_cpu(cqe->status) & MASK_OCS;

	return lrbp->utr_descriptor_ptr->header.ocs & MASK_OCS;
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

	trace_ufsfeature_upiu(dev_name(hba->dev), str_t, header, &rq->sc.cdb,
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

	if (is_mcq_enabled(hba)) {
		struct ufs_hw_queue *hwq = ufsf_mcq_req_to_hwq(hba, rq);

		hwq_id = hwq->id;
	} else {
		doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	}
	trace_ufsfeature_command(dev_name(hba->dev), str_t, tag,
			doorbell, hwq_id, transfer_len, intr, lba, opcode, group_id);
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

	if (is_mcq_enabled(hba)) {
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
	trace_android_vh_ufs_send_command_post_change(hba, lrbp);
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
