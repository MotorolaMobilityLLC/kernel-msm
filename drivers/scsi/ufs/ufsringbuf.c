/*
 * Universal Flash Storage Ring Buffer
 *
 * Copyright (C) 2019-2019 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
 *	Jinyoung Choi <j-young.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#include "ufshcd.h"
#include "ufsringbuf.h"

static void ufsringbuf_reset_work_fn(struct work_struct *work);
static int ufsringbuf_issue_vendor_mode_retry(struct ufsf_feature *ufsf,
					      u8 code);
static int ufsringbuf_create_debugfs(struct ufsf_feature *ufsf);

inline int ufsringbuf_get_state(struct ufsf_feature *ufsf)
{
	return atomic_read(&ufsf->ringbuf_state);
}

inline void ufsringbuf_set_state(struct ufsf_feature *ufsf, int state)
{
	atomic_set(&ufsf->ringbuf_state, state);
}

static inline int ufsringbuf_is_not_present(struct ufsringbuf_dev *ringbuf)
{
	enum UFSRINGBUF_STATE cur_state = ufsringbuf_get_state(ringbuf->ufsf);

	if (cur_state != RINGBUF_PRESENT) {
		INFO_MSG("ringbuf_state != RINGBUF_PRESENT (%d)", cur_state);
		return -ENODEV;
	}
	return 0;
}

static int ufsringbuf_set_rtc(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	u32 rtc_a, rtc_b;
	int ret_a, ret_b;
	s64 rtc_ktime = ktime_get();

	rtc_a = (rtc_ktime >> 32) & 0xffffffff;
	rtc_b = rtc_ktime & 0xffffffff;

	INFO_MSG("ktime attr %lld(%.16llX) -> %d(%.8X) %d(%.8X)", rtc_ktime,
		 rtc_ktime, rtc_a, rtc_a, rtc_b, rtc_b);

	ret_a = ufsf_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				      QUERY_ATTR_IDN_RINGBUF_RTCA, 0,
				      &rtc_a);
	if (ret_a)
		ERR_MSG("set RTCA query request fail (%d)", ret_a);
	else
		INFO_MSG("RTCA write query success");

	ret_b = ufsf_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				      QUERY_ATTR_IDN_RINGBUF_RTCB, 0,
				      &rtc_b);
	if (ret_b)
		ERR_MSG("set RTCB query request fail (%d)", ret_b);
	else
		INFO_MSG("RTCB write query success");

	if (ret_a)
		return ret_a;
	return ret_b;
}

struct scsi_device *ufsringbuf_get_sdev(struct ufsf_feature *ufsf,
					int target_lun)
{
	struct scsi_device *sdev = NULL;

	if (target_lun >= 0 && target_lun < UFS_UPIU_MAX_GENERAL_LUN)
		sdev = ufsf->sdev_ufs_lu[target_lun];

	if (!sdev)
		ERR_MSG("get scsi_device fail");
	else
		INFO_MSG("get scsi_device lun (%llu)", sdev->lun);

	return sdev;
}

static inline ktime_t ufsringbuf_get_rtc_ktime(struct history_block_desc *desc)
{
	return ((u64)LI_EN_32(&desc->host_time_a) << 32 |
		LI_EN_32(&desc->host_time_b));
}

static void ufsringbuf_print_scsi_cmd(struct seq_file *file, int line,
				      struct history_block_desc *desc,
				      struct utp_upiu_req *upiu_req)
{
	unsigned char buf[255];
	int count = 0;
	u8 opcode = 0;
	struct timeval tv_rtc =
		ktime_to_timeval(ufsringbuf_get_rtc_ktime(desc));

	count += snprintf(buf + count, 11, "scsi_cmd: ");

	count += snprintf(buf + count, 19, "[%8ld.%.6ld] ", (long)tv_rtc.tv_sec,
			  (long)tv_rtc.tv_usec);

	count += snprintf(buf + count, 19, "dev_time=%-8u ",
			  LI_EN_32(&desc->device_time));

	count += snprintf(buf + count, 9, "lun=0x%1x ",
			  GET_BYTE_2(upiu_req->header.dword_0));

	count += snprintf(buf + count, 8, "tag=%-2d ",
			  GET_BYTE_3(upiu_req->header.dword_0));

	opcode = upiu_req->sc.cdb[0];
	count += snprintf(buf + count, 13, "cmd_id=0x%.2x ", opcode);

	if ((opcode == READ_10) || (opcode == WRITE_10)) {
		count += snprintf(buf + count, 16, "lba=0x%.8x ",
				  LI_EN_32(&upiu_req->sc.cdb[2]));

		count += snprintf(buf + count, 18, "txfer_len=%-6d ",
				  LI_EN_32(&upiu_req->sc.exp_data_transfer_len));
	}
#if defined(CONFIG_UFSHPB)
	else if (opcode == READ_16) {
		count += snprintf(buf + count, 16, "lba=0x%.8x ",
				  LI_EN_32(&upiu_req->sc.cdb[2]));

		count += snprintf(buf + count, 18, "txfer_len=%-6d ",
				  upiu_req->sc.cdb[15]);

	} else if (opcode == UFSHPB_WRITE_BUFFER) {
		count += snprintf(buf + count, 16, "lpn=0x%.8x ",
				  LI_EN_32(&upiu_req->sc.cdb[2]));

		count += snprintf(buf + count, 18, "txfer_len=%-6d ",
				  LI_EN_16(&upiu_req->sc.cdb[7]));

	} else if (opcode == UFSHPB_READ_BUFFER) {
		count += snprintf(buf + count, 11, "rgn=%-5d ",
				  LI_EN_16(&upiu_req->sc.cdb[2]));
		count += snprintf(buf + count, 8, "srgn=%-1d ",
				  LI_EN_16(&upiu_req->sc.cdb[4]));
	}
#endif
	else {
		int size = COMMAND_SIZE(opcode);
		int i;

		count += snprintf(buf + count, 5, "cdb=");
		for (i = 0; i < size; i++)
			count += snprintf(buf + count, 4, "%.2x ",
					  upiu_req->sc.cdb[i]);
	}
	count += snprintf(buf + count, 34,
			  "state=0x%.2x %.2x %.2x sense=0x%.2x %.2x %.2x",
			  desc->command_state, desc->response, desc->status,
			  desc->sense_key, desc->asc, desc->ascq);

	RINGBUF_MSG(file, "%.4d %s", line, buf);
}

static void ufsringbuf_print_task_req(struct seq_file *file, int line,
				      struct history_block_desc *desc,
				      struct utp_upiu_req *upiu_req)
{
	struct utp_upiu_task_req *upiu_task_req =
		(struct utp_upiu_task_req *)upiu_req;
	unsigned char buf[255];
	int count = 0;
	struct timeval tv_rtc =
		ktime_to_timeval(ufsringbuf_get_rtc_ktime(desc));

	count += snprintf(buf + count, 10, "task_rq: ");

	count += snprintf(buf + count, 19, "[%8ld.%.6ld] ", (long)tv_rtc.tv_sec,
			  (long)tv_rtc.tv_usec);

	count += snprintf(buf + count, 19, "dev_time=%-8u ",
			  LI_EN_32(&desc->device_time));

	count += snprintf(buf + count, 9, "lun=0x%1x ",
			  GET_BYTE_2(upiu_task_req->header.dword_0));

	count += snprintf(buf + count, 8, "tag=%-2d ",
			  GET_BYTE_3(upiu_task_req->header.dword_0));

	count += snprintf(buf + count, 13, "cmd_id=0x%.2x ",
			  GET_BYTE_1(upiu_task_req->header.dword_1));

	count += snprintf(buf + count, 34,
			  "state=0x%.2x %.2x %.2x sense=0x%.2x %.2x %.2x",
			  desc->command_state, desc->response, desc->status,
			  desc->sense_key, desc->asc, desc->ascq);

	RINGBUF_MSG(file, "%.4d %s", line, buf);
}


static void ufsringbuf_print_query_req(struct seq_file *file, int line,
				       struct history_block_desc *desc,
				       struct utp_upiu_req *upiu_req)
{
	unsigned char buf[255];
	int count = 0;
	struct timeval tv_rtc =
		ktime_to_timeval(ufsringbuf_get_rtc_ktime(desc));

	count += snprintf(buf + count, 11, "query_rq: ");

	count += snprintf(buf + count, 19, "[%8ld.%.6ld] ", (long)tv_rtc.tv_sec,
			  (long)tv_rtc.tv_usec);

	count += snprintf(buf + count, 19, "dev_time=%-8u ",
			  LI_EN_32(&desc->device_time));

	count += snprintf(buf + count, 9, "lun=0x%1x ",
			  GET_BYTE_2(upiu_req->header.dword_0));

	count += snprintf(buf + count, 8, "tag=%-2d ",
			  GET_BYTE_3(upiu_req->header.dword_0));

	count += snprintf(buf + count, 13, "cmd_id=0x%.2x ",
			  upiu_req->qr.opcode);

	count += snprintf(buf + count, 10, "idn=0x%.2x ",
			  upiu_req->qr.idn);

	count += snprintf(buf + count, 12, "index=0x%.2x ",
			  upiu_req->qr.index);

	count += snprintf(buf + count, 12, "selector=%1d ",
			  upiu_req->qr.selector);

	count += snprintf(buf + count, 34,
			  "state=0x%.2x %.2x %.2x sense=0x%.2x %.2x %.2x",
			  desc->command_state, desc->response, desc->status,
			  desc->sense_key, desc->asc, desc->ascq);

	RINGBUF_MSG(file, "%.4d %s", line, buf);
}

static void ufsringbuf_print_upiu(struct seq_file *file, int line,
				  struct history_block_desc *desc)
{
	struct timeval tv_rtc =
		ktime_to_timeval(ufsringbuf_get_rtc_ktime(desc));

	RINGBUF_MSG(file,
		    "INFO %d [%8ld.%.6ld] dev_time=%u state 0x%.2x 0x%.2x 0x%.2x sense 0x%.2x 0x%.2x 0x%.2x",
		    line, (long)tv_rtc.tv_sec, (long)tv_rtc.tv_usec,
		    LI_EN_32(&desc->device_time),
		    desc->command_state, desc->response,
		    desc->status, desc->sense_key,
		    desc->asc, desc->ascq);

	RINGBUF_MSG(file,
		    "UPIU %d upiu[0..3] %.8X %.8X %.8X %.8X upiu [4..7] %.8X %.8X %.8X %.8X",
		    line, desc->upiu[0], desc->upiu[1],
		    desc->upiu[2], desc->upiu[3],
		    desc->upiu[4], desc->upiu[5],
		    desc->upiu[6], desc->upiu[7]);
}

static void ufsringbuf_print_upiu_parsing(struct seq_file *file, int line,
					  struct history_block_desc *desc)
{
	struct utp_upiu_req *upiu_req = (struct utp_upiu_req *)&desc->upiu;

	if (GET_BYTE_0(upiu_req->header.dword_0) == UPIU_TRANSACTION_COMMAND)
		ufsringbuf_print_scsi_cmd(file, line, desc, upiu_req);
	else if (GET_BYTE_0(upiu_req->header.dword_0) ==
		 UPIU_TRANSACTION_TASK_REQ)
		ufsringbuf_print_task_req(file, line, desc, upiu_req);
	else if (GET_BYTE_0(upiu_req->header.dword_0) ==
		 UPIU_TRANSACTION_QUERY_REQ)
		ufsringbuf_print_query_req(file, line, desc, upiu_req);
	else
		RINGBUF_MSG(file,
			    "UPIU seqno %d upiu[0..3] %.8X %.8X %.8X %.8X upiu [4..7] %.8X %.8X %.8X %.8X",
			    line, desc->upiu[0], desc->upiu[1],
			    desc->upiu[2], desc->upiu[3],
			    desc->upiu[4], desc->upiu[5],
			    desc->upiu[6], desc->upiu[7]);
}

static void ufsringbuf_print_buf(struct seq_file *file, void *buf,
				 int max_lines, bool is_upiu)
{
	struct history_block_desc *desc;
	int line;
	void *p = buf;

	p += HIST_BLK_DESC_START;
	for (line = 0; line < max_lines; line++, p += HIST_BLOCK_DESC_BYTES) {
		desc = (struct history_block_desc *)p;

		if (is_upiu)
			ufsringbuf_print_upiu(file, line, desc);
		else
			ufsringbuf_print_upiu_parsing(file, line, desc);
	}
}

static void ufsringbuf_show_msg(struct ufsf_feature *ufsf,
				struct seq_file *file, void *buf)
{
	struct ufsringbuf_dev *ringbuf;
	int max_lines, data_length;
	unsigned char *p;

	ringbuf = ufsf->ringbuf_dev;

	p = (unsigned char *)buf;
	data_length = ((u32)p[0] << 16 | (u32)p[1] << 8 | (u32)p[2]);
	if (data_length == 0)
		RINGBUF_MSG(file, "Check sense_key in dmesg!");
	INFO_MSG("data_length %d", data_length);

	max_lines = (u32)data_length / HIST_BLOCK_DESC_BYTES;
	INFO_MSG("print lines %d", max_lines);

	RINGBUF_MSG(file, "History block data_length:%d lines:%d",
		    data_length, max_lines);

	if (!ringbuf->parsing) {
		ufsringbuf_print_buf(file, buf, max_lines, true);
		ufsringbuf_print_buf(file, buf, max_lines, false);
	} else {
		ufsringbuf_print_buf(file, buf, max_lines, false);
	}
}

static inline void ufsringbuf_set_vendor_mode_cmd(struct ufsf_feature *ufsf,
						  struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;
	u32 entry_2, entry_6;

	entry_2 = ufsf->ringbuf_dev->input_signature;
	put_unaligned_be32(entry_2, &cdb[2]);

	entry_6 = ufsf->ringbuf_dev->input_parameter;
	put_unaligned_be32(entry_6, &cdb[6]);
}

#if defined(CONFIG_UFSRINGBUF_POC)
static inline void ufsringbuf_set_passwd_cmd(struct ufsf_feature *ufsf,
					     struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;

	put_unaligned_be32(ufsf->ringbuf_dev->input_parameter, &cdb[2]);
}
#endif

/*
 * This function is for vendor cmd.
 * cdb[10] -> cdb[16]
 */
void ufsringbuf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;

	if (!ufsf->ringbuf_dev)
		return;

	if (cdb[0] != VENDOR_CMD_OP)
		return;

	if (cdb[1] == ENTER_VENDOR || cdb[1] == EXIT_VENDOR)
		ufsringbuf_set_vendor_mode_cmd(ufsf, lrbp);
#if defined(CONFIG_UFSRINGBUF_POC)
	else if (cdb[1] == SET_PASSWD)
		ufsringbuf_set_passwd_cmd(ufsf, lrbp);
#endif

	lrbp->cmd->cmd_len = UFS_CDB_SIZE;
}

/*
 * scsi_execute() will copy cdb by 10-byte due to opcode.
 * so it will be changed in ufsf_ringbuf_prep_fn().
 */
static int ufsringbuf_issue_vendor_mode(struct ufsf_feature *ufsf, u8 code)
{
	struct scsi_sense_hdr sshdr;
	unsigned char cdb[10] = { 0 };
	struct scsi_device *sdev = NULL;
	int ret;

	sdev = ufsringbuf_get_sdev(ufsf, GET_DEFAULT_LU);
	if (!sdev)
		return -ENODEV;

	if (code != ENTER_VENDOR && code != EXIT_VENDOR) {
		ERR_MSG("vendor command code (%d)", code);
		return -EINVAL;
	}

	cdb[0] = VENDOR_CMD_OP;
	cdb[1] = code;

	ret = scsi_execute(sdev, cdb, DMA_NONE, NULL, 0, NULL, &sshdr,
			   VENDOR_CMD_TIMEOUT, 0, 0, 0, NULL);
	INFO_MSG("vendor(%s) command %s",
		 code == ENTER_VENDOR ? "ENTER" : "EXIT",
		 ret ? "fail" : "success");
	if (ret) {
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);
	}

	return ret;
}

static int ufsringbuf_issue_vendor_mode_retry(struct ufsf_feature *ufsf,
					      u8 code)
{
	int ret, retries;

	for (retries = 0; retries < 3; retries++) {
		ret = ufsringbuf_issue_vendor_mode(ufsf, code);
		if (ret)
			ERR_MSG("vendor_mode[%s] issue fail. retries %d",
				code == ENTER_VENDOR ? "ENTER" : "EXIT",
				retries + 1);
		else
			break;
	}
	return ret;
}

static int ufsringbuf_issue_read_buffer(struct ufsf_feature *ufsf,
					void *buf, int transfer_bytes)
{
	struct scsi_device *sdev;
	struct scsi_sense_hdr sshdr;
	unsigned char cdb[10] = { 0 };
	int ret = 0, ret_issue = 0, retries;

	sdev = ufsringbuf_get_sdev(ufsf, GET_DEFAULT_LU);
	if (!sdev)
		return -ENODEV;

	ret = ufsringbuf_issue_vendor_mode_retry(ufsf, ENTER_VENDOR);
	if (ret) {
		ERR_MSG("enter vendor_mode fail. (%d)", ret);
		ret_issue = -EACCES;
		goto out;
	}

	cdb[0] = READ_BUFFER;
	cdb[1] = RING_BUFFER_MODE;
	cdb[2] = (ufsf->ringbuf_dev->volatile_hist) ?
		GET_VOLATILE_BUF : GET_NON_VOLATILE_BUF;
	cdb[6] = GET_BYTE_2(transfer_bytes);
	cdb[7] = GET_BYTE_1(transfer_bytes);
	cdb[8] = GET_BYTE_0(transfer_bytes);

	for (retries = 0; retries < 3; retries++) {
		ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, transfer_bytes,
				   NULL, &sshdr, msecs_to_jiffies(30000), 3, 0, 0,
				   NULL);
		if (ret)
			INFO_MSG("RB for Ringbuffer fail ret %d retries %d",
				 ret, retries);
		else
			break;
	}

	INFO_MSG("RB for Ringbuffer %s", ret ? "fail" : "success");
	if (ret) {
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);

		ret_issue = ret;
	}
out:
	ret = ufsringbuf_issue_vendor_mode_retry(ufsf, EXIT_VENDOR);
	if (ret)
		ERR_MSG("exit vendor_mode fail. (%d)", ret);

	/* clear header location */
	if (ret_issue || ret)
		memset(buf, 0x00, HIST_BUFFER_HEADER_BYTES);

	return ret_issue ? ret_issue : ret;
}

static inline int ufsringbuf_version_check(int spec_version)
{
	INFO_MSG("Support RingBuffer Spec : Driver = (%.4x), Device = (%.4x)",
		 UFSRINGBUF_VER, spec_version);

	INFO_MSG("Ringbuf D/D version (%.6X%s)", UFSRINGBUF_DD_VER,
		 UFSRINGBUF_DD_VER_POST);

	if (spec_version != UFSRINGBUF_VER) {
		ERR_MSG("UFS RingBuffer version mismatched");
		return -ENODEV;
	}
	return 0;
}

void ufsringbuf_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	int ret = 0, spec_version;

	ufsf->ringbuf_dev = NULL;


	if (!(LI_EN_32(&desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP]) &
	    UFS_FEATURE_SUPPORT_RINGBUF_BIT)) {
		INFO_MSG("bUFSExFeaturesSupport: RingBuffer not support");
		goto err_out;
	}

	INFO_MSG("bUFSExFeaturesSupport: RingBuffer support");
	spec_version =
		LI_EN_16(&desc_buf[DEVICE_DESC_PARAM_RING_BUF_VER]);
	ret = ufsringbuf_version_check(spec_version);
	if (ret)
		goto err_out;

	ufsf->ringbuf_dev = kzalloc(sizeof(struct ufsringbuf_dev),
				    GFP_KERNEL);
	if (!ufsf->ringbuf_dev) {
		ERR_MSG("ringbuf_dev memalloc fail");
		goto err_out;
	}

	ufsf->ringbuf_dev->ufsf = ufsf;
	return;
err_out:
	ufsringbuf_set_state(ufsf, RINGBUF_FAILED);
}

void ufsringbuf_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf)
{
	struct ufsringbuf_dev *ringbuf = ufsf->ringbuf_dev;
	int hist_buf_size;

	hist_buf_size =
		LI_EN_16(&geo_buf[GEOMETRY_DESC_RINGBUF_MAX_HIST_BUFSIZE]);
	if (!hist_buf_size) {
		ERR_MSG("history buffer size is 0. so ringbuf disabled");
		kfree(ringbuf);
		ufsringbuf_set_state(ufsf, RINGBUF_FAILED);
		return;
	}

	ringbuf->transfer_bytes = hist_buf_size * HIST_BUFFER_UNIT;

	INFO_MSG("[57:58] wMaxCommandHistoryBufferSize %u", hist_buf_size);
	INFO_MSG("RingBuffer Size %u (Bytes)", ringbuf->transfer_bytes);
	INFO_MSG("max_lines_on_print %d",
		 (ringbuf->transfer_bytes / HIST_BLOCK_DESC_BYTES) - 1);
}

void ufsringbuf_init(struct ufsf_feature *ufsf)
{
	struct ufsringbuf_dev *ringbuf;
	int ret;

	INFO_MSG("RINGBUF_INIT_START");

	ringbuf = ufsf->ringbuf_dev;
	if (!ringbuf) {
		ERR_MSG("ringbuf_dev was not allocated. so disable ringbuf.");
		ufsringbuf_set_state(ufsf, RINGBUF_FAILED);
		return;
	}

	ringbuf->msg_buffer = kzalloc(ringbuf->transfer_bytes, GFP_KERNEL);
	if (!ringbuf->msg_buffer) {
		ERR_MSG("ringbuf msg_buffer allocation fail");
		goto memalloc_fail;
	}

	ringbuf->volatile_hist = false;
	ringbuf->parsing = false;
	ringbuf->record_en_drv = false;

	ringbuf->input_signature = 0;
	ringbuf->input_parameter = 0;
#if defined(CONFIG_UFSRINGBUF_POC)
	ringbuf->input_signature = INPUT_SIG;
	ringbuf->input_parameter = INPUT_PARAM;
#endif

	INIT_DELAYED_WORK(&ringbuf->ringbuf_reset_work,
			  ufsringbuf_reset_work_fn);

	ret = ufsringbuf_create_debugfs(ufsf);
	if (ret) {
		ERR_MSG("Creating UFS Ringbuffer debugfs files. (%d)", ret);
		goto err_out;
	}

	INFO_MSG("UFS RingBuffer create debugfs finished");

	ufsringbuf_set_state(ufsf, RINGBUF_PRESENT);
	return;
err_out:
	kfree(ringbuf->msg_buffer);
memalloc_fail:
	kfree(ufsf->ringbuf_dev);
	ufsringbuf_set_state(ufsf, RINGBUF_FAILED);
}

static void ufsringbuf_print_to_kern(struct ufsf_feature *ufsf)
{
	struct ufsringbuf_dev *ringbuf;
	int ret;
	void *buf;

	ringbuf = ufsf->ringbuf_dev;

	buf = ringbuf->msg_buffer;

	ret = ufsringbuf_issue_read_buffer(ufsf, buf, ringbuf->transfer_bytes);
	if (ret) {
		ERR_MSG("issue READ_BUFFER for ringbuffer fail. (%d)", ret);
		return;
	}

	ufsringbuf_show_msg(ufsf, NULL, buf);
}

static void ufsringbuf_reset_work_fn(struct work_struct *dwork)
{
	struct ufsringbuf_dev *ringbuf;
	struct ufsf_feature *ufsf;

	ringbuf = container_of(dwork, struct ufsringbuf_dev,
			       ringbuf_reset_work.work);
	ufsf = ringbuf->ufsf;

	if (ufsringbuf_get_state(ufsf) != RINGBUF_RESET)
		return;

	INFO_MSG("reset work.");

	pm_runtime_get_sync(ufsf->hba->dev);
	ufsringbuf_print_to_kern(ufsf);
	pm_runtime_put_sync(ufsf->hba->dev);

	/*
	 * The record_en flag was set by user
	 * must be re-enabled by the user when reset occurs.
	 */
	ringbuf->record_en_drv = false;

	/*
	 * The ringbuffer in dev will be init when user issues record_en by sysfs.
	 * So state changes to PRESENT after we get the ringbuffer data from dev.
	 * Otherwise, we couldn't get the ringbuffer data before reset.
	 */
	ufsringbuf_set_state(ufsf, RINGBUF_PRESENT);
}

void ufsringbuf_reset_host(struct ufsf_feature *ufsf)
{
	ufsringbuf_set_state(ufsf, RINGBUF_RESET);
}

void ufsringbuf_reset(struct ufsf_feature *ufsf)
{
	struct ufsringbuf_dev *ringbuf = ufsf->ringbuf_dev;

	if (!ringbuf)
		return;

	INFO_MSG("record_en(drv) %d", ringbuf->record_en_drv);

	if (!ringbuf->record_en_drv) {
		ufsringbuf_set_state(ufsf, RINGBUF_PRESENT);
	} else {
		if (delayed_work_pending(&ringbuf->ringbuf_reset_work))
			cancel_delayed_work(&ringbuf->ringbuf_reset_work);

		schedule_delayed_work(&ringbuf->ringbuf_reset_work,
				      RESET_DELAY);
	}
}

void ufsringbuf_remove(struct ufsf_feature *ufsf)
{
	struct ufsringbuf_dev *ringbuf = ufsf->ringbuf_dev;

	if (!ringbuf)
		return;

	INFO_MSG("start RingBuffer release");

	ufsringbuf_set_state(ufsf, RINGBUF_FAILED);

	debugfs_remove_recursive(ringbuf->ringbuf_root);

	kfree(ringbuf->msg_buffer);

	kfree(ringbuf);

	INFO_MSG("end RingBuffer release");
}

void ufsringbuf_resume(struct ufsf_feature *ufsf)
{
	struct ufsringbuf_dev *ringbuf = ufsf->ringbuf_dev;
	int ret;

	if (!ringbuf || !ringbuf->record_en_drv)
		return;

	INFO_MSG("ringbuf resume.. record_en_drv(%s)",
		 ufsf->ringbuf_dev->record_en_drv ? "SET_RTC" : "NOT_YET");
	ret = ufsringbuf_set_rtc(ufsf);
	if (ret)
		ERR_MSG("RTC SET fail. (%d)", ret);
	else
		INFO_MSG("RTC SET query success");
}

/***********************************************************************
 * There are functions for DEBUGFS in below.
 **********************************************************************/
static int ufsringbuf_print_show(struct seq_file *file, void *data)
{
	struct ufsf_feature *ufsf = (struct ufsf_feature *)file->private;

	INFO_MSG("print..");
	ufsringbuf_show_msg(ufsf, file, ufsf->ringbuf_dev->msg_buffer);

	return 0;
}

static int ufsringbuf_print_open(struct inode *inode, struct file *file)
{
	struct ufsf_feature *ufsf = (struct ufsf_feature *)inode->i_private;
	struct ufs_hba *hba = ufsf->hba;
	int transfer_bytes;
	void *buf;
	int ret;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	transfer_bytes = ufsf->ringbuf_dev->transfer_bytes;
	buf = ufsf->ringbuf_dev->msg_buffer;
	BUG_ON(!buf);

	pm_runtime_get_sync(hba->dev);
	ret = ufsringbuf_issue_read_buffer(ufsf, buf, transfer_bytes);
	if (ret)
		ERR_MSG("read buffer fail. check response code. (%d)", ret);
	pm_runtime_put_sync(hba->dev);

	return single_open(file, ufsringbuf_print_show, inode->i_private);
}

static const struct file_operations fops_print = {
	.open = ufsringbuf_print_open,
	.read = seq_read,
	.release = single_release,
};

static int ufsringbuf_get_record_en(void *data, u64 *attr_val)
{
	struct ufsf_feature *ufsf = data;
	struct ufs_hba *hba = ufsf->hba;
	int ret = 0, ret_issue = 0;
	bool flag_res = false;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	pm_runtime_get_sync(hba->dev);

	ret = ufsringbuf_issue_vendor_mode_retry(ufsf, ENTER_VENDOR);
	if (ret) {
		ERR_MSG("enter vendor_mode fail. (%d)", ret);
		return -EACCES;
	}

	ret = ufsf_query_flag_retry(ufsf->hba, UPIU_QUERY_OPCODE_READ_FLAG,
				    QUERY_FLAG_IDN_CMD_HISTORY_RECORD_EN, 0,
				    &flag_res);
	if (ret) {
		ERR_MSG("query fail. (%d)", ret);
		ret_issue = ret;
	}

	ret = ufsringbuf_issue_vendor_mode_retry(ufsf, EXIT_VENDOR);
	if (ret)
		ERR_MSG("exit vendor_mode fail. (%d)", ret);

	if (!ret_issue) {
		INFO_MSG("record_en read query success! flag_res %d", flag_res);
		*attr_val = (flag_res == true) ? 1 : 0;
	} else {
		INFO_MSG("record_en read query fail! flag_res %d", flag_res);
		*attr_val = (u64)-ENODEV;
	}
	pm_runtime_put_sync(hba->dev);
	return ret;
}

static int __set_record_en(struct ufsf_feature *ufsf, bool set)
{
	int ret = 0, ret_issue = 0;
	enum query_opcode op = 0;

	op = set ? (UPIU_QUERY_OPCODE_SET_FLAG) :
		(UPIU_QUERY_OPCODE_CLEAR_FLAG);

	INFO_MSG("record_en %s flag query request", set ? "SET" : "CLEAR");

	ret = ufsringbuf_issue_vendor_mode_retry(ufsf, ENTER_VENDOR);
	if (ret) {
		ERR_MSG("enter vendor_mode fail. (%d)", ret);
		return -EACCES;
	}

	ret = ufsf_query_flag_retry(ufsf->hba, op,
				    QUERY_FLAG_IDN_CMD_HISTORY_RECORD_EN, 0,
				    NULL);
	if (ret) {
		ERR_MSG("query fail. (%d)", ret);
		ret_issue = ret;
	} else {
		INFO_MSG("query success");
		ufsf->ringbuf_dev->record_en_drv = set;
	}
	ret = ufsringbuf_issue_vendor_mode_retry(ufsf, EXIT_VENDOR);
	if (ret)
		ERR_MSG("exit vendor_mode fail. (%d)", ret);

	return ret_issue ? ret_issue : ret;
}

static int ufsringbuf_set_record_en(void *data, u64 attr_val)
{
	struct ufsf_feature *ufsf = data;
	struct ufs_hba *hba = ufsf->hba;
	int ret = 0;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	if (attr_val != 0 && attr_val != 1)
		return -EINVAL;

	INFO_MSG("attr_val %llu", attr_val);

	pm_runtime_get_sync(hba->dev);

	/*
	 * Don't need to set the rtc for unset record_en
	 */
	if (attr_val) {
		ret = ufsringbuf_set_rtc(ufsf);
		if (ret) {
			ERR_MSG("RTC SET fail. Stop record_en issue (%d)", ret);
			return ret;
		}
		INFO_MSG("RTC SET query success");
	}

	ret = __set_record_en(ufsf, attr_val ? true : false);

	pm_runtime_put_sync(hba->dev);

	return ret;
}

static int ufsringbuf_check_hex_input(char *kbuf, int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		if (!((kbuf[i] >= '0' && kbuf[i] <= '9') ||
		    (kbuf[i] >= 'A' && kbuf[i] <= 'F') ||
		    (kbuf[i] >= 'a' && kbuf[i] <= 'f')))
			return -EINVAL;
	}

	return 0;
}

#define CREATE_DEFAULT_OPS(name)					\
static int ufsringbuf_get_##name(void *data, u64 *attr_val)		\
{									\
	struct ufsf_feature *ufsf = data;				\
									\
	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))		\
		return -ENODEV;						\
	*attr_val = ufsf->ringbuf_dev->name ? 1 : 0;			\
	return 0;							\
}									\
static int ufsringbuf_set_##name(void *data, u64 attr_val)		\
{									\
	struct ufsf_feature *ufsf = data;				\
									\
	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))		\
		return -ENODEV;						\
	if (attr_val != 0 && attr_val != 1)				\
		return -EINVAL;						\
	ufsf->ringbuf_dev->name = attr_val ? true : false;		\
	return 0;							\
}

#define CREATE_DEFAULT_FOPS(name)					\
static int ufsringbuf_open_##name(struct inode *inode, struct file *filp)\
{									\
	return simple_attr_open(inode, filp,				\
				ufsringbuf_get_##name,			\
				ufsringbuf_set_##name, "%llu\n");	\
}									\
static const struct file_operations fops_##name = {			\
	.owner	 = THIS_MODULE,						\
	.open	 = ufsringbuf_open_##name,				\
	.release = simple_attr_release,					\
	.read	 = simple_attr_read,					\
	.write	 = simple_attr_write,					\
	.llseek	 = generic_file_llseek,					\
}

#define CREATE_DEFAULT_OPS_VENDOR(name)					\
static ssize_t ufsringbuf_set_##name(struct file *filp,			\
					 const char __user *ubuf,	\
					 size_t cnt,			\
					 loff_t *ppos)			\
{									\
	struct ufsf_feature *ufsf = filp->f_mapping->host->i_private;	\
	char kbuf[BUFF_LINE_SIZE] = { 0 };				\
	loff_t buff_pos = 0;						\
	int ret;							\
									\
	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))		\
		return -ENODEV;						\
	ret = simple_write_to_buffer(kbuf, BUFF_LINE_SIZE - 1,		\
				     &buff_pos, ubuf, cnt);		\
	if (ret < 0)							\
		return ret;						\
	kbuf[ret] = '\0';						\
	if ((ret - 1) != VENDOR_INPUT_LEN) {				\
		ERR_MSG("input length must be (%d). input (%d)",	\
			VENDOR_INPUT_LEN, ret - 1);			\
		return -EINVAL;						\
	}								\
	ret = ufsringbuf_check_hex_input(kbuf, ret - 1);		\
	if (ret) {							\
		ERR_MSG("input is not hex value. input (%s)", kbuf);	\
		return ret;						\
	}								\
	ret = kstrtouint(kbuf, 16, &ufsf->ringbuf_dev->name);		\
	if (!ret)							\
		INFO_MSG(#name" (0x%08X)", ufsf->ringbuf_dev->name);	\
	else								\
		ERR_MSG("input is not valid. (%d)", ret);		\
	return cnt;							\
}

#define CREATE_DEFAULT_FOPS_VENDOR(name)				\
static int ufsringbuf_open_##name(struct inode *inode, struct file *filp)\
{									\
	return single_open(filp, NULL, inode->i_private);		\
}									\
static const struct file_operations fops_##name = {			\
	.owner	 = THIS_MODULE,						\
	.open	 = ufsringbuf_open_##name,				\
	.release = single_release,					\
	.write	 = ufsringbuf_set_##name,				\
}

CREATE_DEFAULT_OPS(volatile_hist);
CREATE_DEFAULT_OPS(parsing);
CREATE_DEFAULT_FOPS(volatile_hist);
CREATE_DEFAULT_FOPS(parsing);
CREATE_DEFAULT_FOPS(record_en);

CREATE_DEFAULT_OPS_VENDOR(input_parameter);
CREATE_DEFAULT_OPS_VENDOR(input_signature);
CREATE_DEFAULT_FOPS_VENDOR(input_parameter);
CREATE_DEFAULT_FOPS_VENDOR(input_signature);

#if defined(CONFIG_UFSRINGBUF_POC)
/*
 * For POC.
 */
static int ufsringbuf_get_debug_set_pwd(void *data, u64 *attr_val)
{
	struct ufsf_feature *ufsf = data;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	INFO_MSG("It is impossible to get passwd from device");

	return 0;
}

static int ufsringbuf_set_debug_set_pwd(void *data, u64 attr_val)
{
	struct ufsf_feature *ufsf = data;
	struct scsi_sense_hdr sshdr;
	unsigned char cmd[10] = { 0 };
	struct scsi_device *sdev = NULL;
	int ret = 0;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	pm_runtime_get_sync(ufsf->hba->dev);

	sdev = ufsringbuf_get_sdev(ufsf, GET_DEFAULT_LU);
	if (!sdev) {
		ret = -ENODEV;
		goto out;
	}

	cmd[0] = VENDOR_CMD_OP;
	cmd[1] = SET_PASSWD;

	ret = scsi_execute(sdev, cmd, DMA_NONE, NULL, 0, NULL, &sshdr,
			   VENDOR_CMD_TIMEOUT, 0, 0, 0, NULL);

	INFO_MSG("vendor(SET_PWD) command %s", ret ? "fail" : "successful");
	if (ret) {
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);
	}
out:
	pm_runtime_put_sync(ufsf->hba->dev);
	return ret;
}

static int ufsringbuf_get_issue_abort_cmd(void *data, u64 *attr_val)
{
	struct ufsf_feature *ufsf = data;
	u8 resp = 0xF;
	int err;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	pm_runtime_get_sync(ufsf->hba->dev);
	err = ufshcd_issue_tm_cmd(ufsf->hba, 0, 0,
				  UFS_ABORT_TASK, &resp);
	if (err || resp != UPIU_TASK_MANAGEMENT_FUNC_COMPL) {
		if (!err) {
			err = resp; /* service response error */
			pr_err("%s: issued. tag = 0, err %d\n",
			       __func__, err);
		} else {
			pr_err("%s: issued. tag = 0, success\n",
			       __func__);
		}
	}

	pm_runtime_put_sync(ufsf->hba->dev);
	return 0;
}

static int ufsringbuf_set_issue_abort_cmd(void *data, u64 attr_val)
{
	struct ufsf_feature *ufsf = data;

	if (ufsringbuf_is_not_present(ufsf->ringbuf_dev))
		return -ENODEV;

	INFO_MSG("do anything");

	return 0;
}

CREATE_DEFAULT_FOPS(debug_set_pwd);
CREATE_DEFAULT_FOPS(issue_abort_cmd);
#endif

#define CREATE_DEBUGFS_FILE(ufsf, root, file, name, rw)	do {		\
	file = debugfs_create_file(#name, rw, root, ufsf,		\
				   &fops_##name);			\
	if (!file) {							\
		ERR_MSG("Create ringbuf file(%s) fail", #name);	\
		return -ENODEV;						\
	} } while (0)

static int ufsringbuf_create_debugfs(struct ufsf_feature *ufsf)
{
	struct dentry *file, *ringbuf_root = ufsf->ringbuf_dev->ringbuf_root;

	file = debugfs_lookup("ufsringbuf_dev", ringbuf_root);
	if (file) {
		debugfs_remove_recursive(file);
		INFO_MSG("ufsringbuf_dev exist!! so remove first.");
	}

	ringbuf_root = debugfs_create_dir("ufsringbuf_dev", NULL);
	if (!ringbuf_root) {
		ERR_MSG("Create ringbuf directory fail");
		return -ENODEV;
	}

	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, record_en, RW);
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, volatile_hist, RW);
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, parsing, RW);
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, input_parameter, W);
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, input_signature, W);
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, print, R);

#if defined(CONFIG_UFSRINGBUF_POC)
	/*
	 * This function will be removed in released version.
	 * It exists only for POC purpose.
	 */
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, debug_set_pwd, W);
	CREATE_DEBUGFS_FILE(ufsf, ringbuf_root, file, issue_abort_cmd, R);
#endif
	return 0;
}
