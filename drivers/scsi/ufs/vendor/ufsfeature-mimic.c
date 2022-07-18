// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung UFS Feature Mimic for android12-5.10
 * commit e41c9937238f52c66c07cbcb3a0ea5ed6de152df
 * Date:   Tue Aug 10 10:18:01 2021 -0700
 *
 * Copyright (C) 2021-2022 Samsung Electronics Co., Ltd.
 *
 * Author:
 *	Keoseong Park <keosung.park@samsung.com>
 *
 * from
 *
 * linux/drivers/scsi/ufs/ufshcd.c
 */

#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufsfeature.h"
#include <trace/events/ufs.h>
#include <trace/hooks/ufshcd.h>

/* Query request retries */
#define QUERY_REQ_RETRIES 3
/* Query request timeout */
#define QUERY_REQ_TIMEOUT 1500 /* 1.5 seconds */

/**
 * ufsf_get_tr_ocs - Get the UTRD Overall Command Status
 * @lrbp: pointer to local command reference block
 *
 * This function is used to get the OCS field from UTRD
 * Returns the OCS field in the UTRD
 */
static inline int ufsf_get_tr_ocs(struct ufshcd_lrb *lrbp)
{
	return le32_to_cpu(lrbp->utr_descriptor_ptr->header.dword_2) & MASK_OCS;
}

/**
 * ufsf_get_req_rsp - returns the TR response transaction type
 * @ucd_rsp_ptr: pointer to response UPIU
 */
static inline int
ufsf_get_req_rsp(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_0) >> 24;
}

static inline int
ufsf_get_rsp_upiu_result(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	return be32_to_cpu(ucd_rsp_ptr->header.dword_1) & MASK_RSP_UPIU_RESULT;
}

/**
 * ufsf_outstanding_req_clear - Clear a bit in outstanding request field
 * @hba: per adapter instance
 * @tag: position of the bit to be cleared
 */
static inline void ufsf_outstanding_req_clear(struct ufs_hba *hba, int tag)
{
	clear_bit(tag, &hba->outstanding_reqs);
}

static int
ufsf_check_query_response(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_query_res *query_res = &hba->dev_cmd.query.response;

	/* Get the UPIU response */
	query_res->response = ufsf_get_rsp_upiu_result(lrbp->ucd_rsp_ptr) >>
				UPIU_RSP_CODE_OFFSET;
	return query_res->response;
}

/**
 * ufsf_copy_query_response() - Copy the Query Response and the data
 * descriptor
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
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
		resp_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
						MASK_QUERY_DATA_SEG_LEN;
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

static inline void ufsf_utrl_clear(struct ufs_hba *hba, u32 pos)
{
	if (hba->quirks & UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR)
		ufshcd_writel(hba, (1 << pos), REG_UTP_TRANSFER_REQ_LIST_CLEAR);
	else
		ufshcd_writel(hba, ~(1 << pos),
				REG_UTP_TRANSFER_REQ_LIST_CLEAR);
}

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

static int
ufsf_clear_cmd(struct ufs_hba *hba, int tag)
{
	int err = 0;
	unsigned long flags;
	u32 mask = 1 << tag;

	/* clear outstanding transaction before retry */
	spin_lock_irqsave(hba->host->host_lock, flags);
	ufsf_utrl_clear(hba, tag);
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	/*
	 * wait for for h/w to clear corresponding bit in door-bell.
	 * max. wait is 1 sec.
	 */
	err = ufsf_wait_for_register(hba,
			REG_UTP_TRANSFER_REQ_DOOR_BELL,
			mask, ~mask, 1000, 1000);

	return err;
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
				    const char *str)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;

	trace_ufshcd_upiu(dev_name(hba->dev), str, &rq->header, &rq->sc.cdb);
}

static void ufsf_add_command_trace(struct ufs_hba *hba,
				   unsigned int tag, const char *str)
{
	sector_t lba = -1;
	u8 opcode = 0, group_id = 0;
	u32 intr, doorbell;
	struct ufshcd_lrb *lrbp = &hba->lrb[tag];
	struct scsi_cmnd *cmd = lrbp->cmd;
	int transfer_len = -1;

	if (!trace_ufshcd_command_enabled()) {
		/* trace UPIU W/O tracing command */
		if (cmd)
			ufsf_add_cmd_upiu_trace(hba, tag, str);
		return;
	}

	if (cmd) { /* data phase exists */
		/* trace UPIU also */
		ufsf_add_cmd_upiu_trace(hba, tag, str);
		opcode = cmd->cmnd[0];
		if ((opcode == READ_10) || (opcode == WRITE_10)) {
			/*
			 * Currently we only fully trace read(10) and write(10)
			 * commands
			 */
			if (cmd->request && cmd->request->bio)
				lba = cmd->request->bio->bi_iter.bi_sector;
			transfer_len = be32_to_cpu(
				lrbp->ucd_req_ptr->sc.exp_data_transfer_len);
			if (opcode == WRITE_10)
				group_id = lrbp->cmd->cmnd[6];
		} else if (opcode == UNMAP) {
			if (cmd->request) {
				lba = scsi_get_lba(cmd);
				transfer_len = blk_rq_bytes(cmd->request);
			}
		}
	}

	intr = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
	doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
	trace_ufshcd_command(dev_name(hba->dev), str, tag,
			doorbell, transfer_len, intr, lba, opcode, group_id);
}

static inline bool ufsf_should_inform_monitor(struct ufs_hba *hba,
					      struct ufshcd_lrb *lrbp)
{
	struct ufs_hba_monitor *m = &hba->monitor;

	return (m->enabled && lrbp && lrbp->cmd &&
		(!m->chunk_size || m->chunk_size == lrbp->cmd->sdb.length) &&
		ktime_before(hba->monitor.enabled_ts, lrbp->issue_time_stamp));
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

static void ufsf_start_monitor(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int dir = ufsf_monitor_opcode2dir(*lrbp->cmd->cmnd);
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	if (dir >= 0 && hba->monitor.nr_queued[dir]++ == 0)
		hba->monitor.busy_start_ts[dir] = ktime_get();
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/**
 * ufsf_send_command - Send SCSI or device management commands
 * @hba: per adapter instance
 * @task_tag: Task tag of the command
 */
static inline
void ufsf_send_command(struct ufs_hba *hba, unsigned int task_tag)
{
	struct ufshcd_lrb *lrbp = &hba->lrb[task_tag];

	lrbp->issue_time_stamp = ktime_get();
	lrbp->compl_time_stamp = ktime_set(0, 0);
	trace_android_vh_ufs_send_command(hba, lrbp);
	ufsf_add_command_trace(hba, task_tag, "send");
	ufsf_clk_scaling_start_busy(hba);
	if (unlikely(ufsf_should_inform_monitor(hba, lrbp)))
		ufsf_start_monitor(hba, lrbp);
	if (ufshcd_has_utrlcnr(hba)) {
		set_bit(task_tag, &hba->outstanding_reqs);
		ufshcd_writel(hba, 1 << task_tag,
			      REG_UTP_TRANSFER_REQ_DOOR_BELL);
	} else {
		unsigned long flags;

		spin_lock_irqsave(hba->host->host_lock, flags);
		set_bit(task_tag, &hba->outstanding_reqs);
		ufshcd_writel(hba, 1 << task_tag,
			      REG_UTP_TRANSFER_REQ_DOOR_BELL);
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}
	/* Make sure that doorbell is committed immediately */
	wmb();
}

/**
 * ufsf_dev_cmd_completion() - handles device management command responses
 * @hba: per adapter instance
 * @lrbp: pointer to local reference block
 */
static int
ufsf_dev_cmd_completion(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	int resp;
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
	case UPIU_TRANSACTION_QUERY_RSP:
		err = ufsf_check_query_response(hba, lrbp);
		if (!err)
			err = ufsf_copy_query_response(hba, lrbp);
		break;
	case UPIU_TRANSACTION_REJECT_UPIU:
		/* TODO: handle Reject UPIU Response */
		err = -EPERM;
		dev_err(hba->dev, "%s: Reject UPIU not fully implemented\n",
				__func__);
		break;
	default:
		err = -EINVAL;
		dev_err(hba->dev, "%s: Invalid device management cmd response: %x\n",
				__func__, resp);
		break;
	}

	return err;
}

static int ufsf_wait_for_dev_cmd(struct ufs_hba *hba,
				 struct ufshcd_lrb *lrbp, int max_timeout)
{
	int err = 0;
	unsigned long time_left;
	unsigned long flags;

	time_left = wait_for_completion_timeout(hba->dev_cmd.complete,
			msecs_to_jiffies(max_timeout));

	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();
	spin_lock_irqsave(hba->host->host_lock, flags);
	hba->dev_cmd.complete = NULL;
	if (likely(time_left)) {
		err = ufsf_get_tr_ocs(lrbp);
		if (!err)
			err = ufsf_dev_cmd_completion(hba, lrbp);
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (!time_left) {
		err = -ETIMEDOUT;
		dev_dbg(hba->dev, "%s: dev_cmd request timedout, tag %d\n",
			__func__, lrbp->task_tag);
		if (!ufsf_clear_cmd(hba, lrbp->task_tag))
			/* successfully cleared the command, retry if needed */
			err = -EAGAIN;
		/*
		 * in case of an error, after clearing the doorbell,
		 * we also need to clear the outstanding_request
		 * field in hba
		 */
		ufsf_outstanding_req_clear(hba, lrbp->task_tag);
	}

	return err;
}

static void ufsf_prepare_utp_query_req_upiu(struct ufs_hba *hba,
				struct ufshcd_lrb *lrbp, u8 upiu_flags)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;
	struct ufs_query *query = &hba->dev_cmd.query;
	u16 len = be16_to_cpu(query->request.upiu_req.length);

	/* Query request header */
	ucd_req_ptr->header.dword_0 = UPIU_HEADER_DWORD(
			UPIU_TRANSACTION_QUERY_REQ, upiu_flags,
			lrbp->lun, lrbp->task_tag);
	ucd_req_ptr->header.dword_1 = UPIU_HEADER_DWORD(
			0, query->request.query_func, 0, 0);

	/* Data segment length only need for WRITE_DESC */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		ucd_req_ptr->header.dword_2 =
			UPIU_HEADER_DWORD(0, 0, (len >> 8), (u8)len);
	else
		ucd_req_ptr->header.dword_2 = 0;

	/* Copy the Query Request buffer as is */
	memcpy(&ucd_req_ptr->qr, &query->request.upiu_req,
			QUERY_OSF_SIZE);

	/* Copy the Descriptor */
	if (query->request.upiu_req.opcode == UPIU_QUERY_OPCODE_WRITE_DESC)
		memcpy(ucd_req_ptr + 1, query->descriptor, len);

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static void ufsf_prepare_req_desc_hdr(struct ufshcd_lrb *lrbp,
			u8 *upiu_flags, enum dma_data_direction cmd_dir)
{
	struct utp_transfer_req_desc *req_desc = lrbp->utr_descriptor_ptr;
	u32 data_direction;
	u32 dword_0;
	u32 dword_1 = 0;
	u32 dword_3 = 0;

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

	dword_0 = data_direction | (lrbp->command_type
				<< UPIU_COMMAND_TYPE_OFFSET);
	if (lrbp->intr_cmd)
		dword_0 |= UTP_REQ_DESC_INT_CMD;

	/* Prepare crypto related dwords */
	ufshcd_prepare_req_desc_hdr_crypto(lrbp, &dword_0, &dword_1, &dword_3);

	/* Transfer request descriptor header fields */
	req_desc->header.dword_0 = cpu_to_le32(dword_0);
	req_desc->header.dword_1 = cpu_to_le32(dword_1);
	/*
	 * assigning invalid value for command status. Controller
	 * updates OCS on command completion, with the command
	 * status
	 */
	req_desc->header.dword_2 =
		cpu_to_le32(OCS_INVALID_COMMAND_STATUS);
	req_desc->header.dword_3 = cpu_to_le32(dword_3);

	req_desc->prd_table_length = 0;
}

static inline void ufsf_prepare_utp_nop_upiu(struct ufshcd_lrb *lrbp)
{
	struct utp_upiu_req *ucd_req_ptr = lrbp->ucd_req_ptr;

	memset(ucd_req_ptr, 0, sizeof(struct utp_upiu_req));

	/* command descriptor fields */
	ucd_req_ptr->header.dword_0 =
		UPIU_HEADER_DWORD(
			UPIU_TRANSACTION_NOP_OUT, 0, 0, lrbp->task_tag);
	/* clear rest of the fields of basic header */
	ucd_req_ptr->header.dword_1 = 0;
	ucd_req_ptr->header.dword_2 = 0;

	memset(lrbp->ucd_rsp_ptr, 0, sizeof(struct utp_upiu_rsp));
}

static int ufsf_compose_devman_upiu(struct ufs_hba *hba,
				    struct ufshcd_lrb *lrbp)
{
	u8 upiu_flags;
	int ret = 0;

	if (hba->ufs_version <= ufshci_version(1, 1))
		lrbp->command_type = UTP_CMD_TYPE_DEV_MANAGE;
	else
		lrbp->command_type = UTP_CMD_TYPE_UFS_STORAGE;

	ufsf_prepare_req_desc_hdr(lrbp, &upiu_flags, DMA_NONE);
	if (hba->dev_cmd.type == DEV_CMD_TYPE_QUERY)
		ufsf_prepare_utp_query_req_upiu(hba, lrbp, upiu_flags);
	else if (hba->dev_cmd.type == DEV_CMD_TYPE_NOP)
		ufsf_prepare_utp_nop_upiu(lrbp);
	else
		ret = -EINVAL;

	return ret;
}

static int ufsf_compose_dev_cmd(struct ufs_hba *hba,
		struct ufshcd_lrb *lrbp, enum dev_cmd_type cmd_type, int tag)
{
	lrbp->cmd = NULL;
	lrbp->sense_bufflen = 0;
	lrbp->sense_buffer = NULL;
	lrbp->task_tag = tag;
	lrbp->lun = 0; /* device management cmd is not specific to any LUN */
	lrbp->intr_cmd = true; /* No interrupt aggregation */
	ufshcd_prepare_lrbp_crypto(NULL, lrbp);
	hba->dev_cmd.type = cmd_type;

	return ufsf_compose_devman_upiu(hba, lrbp);
}

static inline bool ufsf_valid_tag(struct ufs_hba *hba, int tag)
{
	return tag >= 0 && tag < hba->nutrs;
}

static void ufsf_add_query_upiu_trace(struct ufs_hba *hba, unsigned int tag,
				      const char *str)
{
	struct utp_upiu_req *rq = hba->lrb[tag].ucd_req_ptr;

	trace_ufshcd_upiu(dev_name(hba->dev), str, &rq->header, &rq->qr);
}

/**
 * ufsf_exec_dev_cmd - API for sending device management requests
 * @hba: UFS hba
 * @cmd_type: specifies the type (NOP, Query...)
 * @timeout: timeout in milliseconds
 *
 * NOTE: Since there is only one available tag for device management commands,
 * it is expected you hold the hba->dev_cmd.lock mutex.
 */
static int ufsf_exec_dev_cmd(struct ufs_hba *hba,
			     enum dev_cmd_type cmd_type, int timeout)
{
	struct request_queue *q = hba->cmd_queue;
	struct request *req;
	struct ufshcd_lrb *lrbp;
	int err;
	int tag;
	struct completion wait;

	down_read(&hba->clk_scaling_lock);

	/*
	 * Get free slot, sleep if slots are unavailable.
	 * Even though we use wait_event() which sleeps indefinitely,
	 * the maximum wait time is bounded by SCSI request timeout.
	 */
	req = blk_get_request(q, REQ_OP_DRV_OUT, 0);
	if (IS_ERR(req)) {
		err = PTR_ERR(req);
		goto out_unlock;
	}
	tag = req->tag;
	WARN_ON_ONCE(!ufsf_valid_tag(hba, tag));
	/* Set the timeout such that the SCSI error handler is not activated. */
	req->timeout = msecs_to_jiffies(2 * timeout);
	blk_mq_start_request(req);

	init_completion(&wait);
	lrbp = &hba->lrb[tag];
	WARN_ON(lrbp->cmd);
	err = ufsf_compose_dev_cmd(hba, lrbp, cmd_type, tag);
	if (unlikely(err))
		goto out;

	hba->dev_cmd.complete = &wait;

	ufsf_add_query_upiu_trace(hba, tag, "query_send");
	/* Make sure descriptors are ready before ringing the doorbell */
	wmb();

	ufsf_send_command(hba, tag);
	err = ufsf_wait_for_dev_cmd(hba, lrbp, timeout);
	ufsf_add_query_upiu_trace(hba, tag,
			err ? "query_complete_err" : "query_complete");

out:
	blk_put_request(req);
out_unlock:
	up_read(&hba->clk_scaling_lock);
	return err;
}

/**
 * ufsf_init_query() - init the query response and request parameters
 * @hba: per-adapter instance
 * @request: address of the request pointer to be initialized
 * @response: address of the response pointer to be initialized
 * @opcode: operation to perform
 * @idn: flag idn to access
 * @index: LU number to access
 * @selector: query/flag/descriptor further identification
 */
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

/**
 * ufsf_query_flag() - API function for sending flag query requests
 * @hba: per-adapter instance
 * @opcode: flag query to perform
 * @idn: flag idn to access
 * @index: flag index to access
 * @flag_res: the flag value after the query request completes
 *
 * Returns 0 for success, non-zero in case of failure
 */
int ufsf_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 index, u8 selector,  bool *flag_res)
{
	struct ufs_query_req *request = NULL;
	struct ufs_query_res *response = NULL;
	int err;
	int timeout = QUERY_REQ_TIMEOUT;

	BUG_ON(!hba);

	ufshcd_hold(hba, false);
	mutex_lock(&hba->dev_cmd.lock);
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
	mutex_unlock(&hba->dev_cmd.lock);
	ufshcd_release(hba);
	return err;
}

int ufsf_query_flag_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum flag_idn idn, u8 index, u8 selector,
			  bool *flag_res)
{
	int ret;
	int retries;

	for (retries = 0; retries < QUERY_REQ_RETRIES; retries++) {
		ret = ufsf_query_flag(hba, opcode, idn, index, selector,
				      flag_res);
		if (ret)
			dev_dbg(hba->dev,
				"%s: failed with error %d, retries %d\n",
				__func__, ret, retries);
		else
			break;
	}

	if (ret)
		dev_err(hba->dev,
			"%s: query attribute, opcode %d, idn %d, failed with error %d after %d retires\n",
			__func__, opcode, idn, ret, retries);
	return ret;
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

int ufsf_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us)
{
	unsigned long flags;
	int ret = 0;
	u32 tm_doorbell;
	u32 tr_doorbell;
	bool timeout = false, do_last_check = false;
	ktime_t start;

	ufshcd_hold(hba, false);
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
		tr_doorbell = ufshcd_readl(hba, REG_UTP_TRANSFER_REQ_DOOR_BELL);
		if (!tm_doorbell && !tr_doorbell) {
			timeout = false;
			break;
		} else if (do_last_check) {
			break;
		}

		spin_unlock_irqrestore(hba->host->host_lock, flags);
		schedule();
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
	} while (tm_doorbell || tr_doorbell);

	if (timeout) {
		dev_err(hba->dev,
			"%s: timedout waiting for doorbell to clear (tm=0x%x, tr=0x%x)\n",
			__func__, tm_doorbell, tr_doorbell);
		ret = -EBUSY;
	}
out:
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufshcd_release(hba);
	return ret;
}

MODULE_LICENSE("GPL v2");
