/*
 * Universal Flash Storage Host Performance Booster
 *
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 * Copyright (C) 2018, Google, Inc.
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

#include <linux/slab.h>
#include <linux/blkdev.h>
#include <scsi/scsi.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <asm/unaligned.h>

#include "../../../block/blk.h"

#include "ufs.h"
#include "ufshcd.h"

#include "ufshpb_toshiba.h"
#include "ufshpb_toshiba_if.h"
#define UFSHPB_PANIC_ON_INVALID_REGION
/*
 * UFSHPB DEBUG
 */
#define hpb_dbg(hba, msg, args...)					\
	do {								\
		if (hba)						\
			dev_dbg(hba->dev, msg, ##args);			\
	} while (0)

#define hpb_trace(hpb, args...)						\
	do {								\
		if (hpb->hba->sdev_ufs_lu[hpb->lun] &&			\
			hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue) \
			blk_add_trace_msg(				\
			hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue,	\
				##args);				\
	} while (0)

/*
 * debug variables
 */
static int alloc_mctx;

/*
 * define global constants
 */
static int sects_per_blk_shift;
static int bits_per_dword_shift;
static int bits_per_dword_mask;
static int bits_per_byte_shift;

static int skip_map_process = 0;
module_param(skip_map_process, int, S_IRUGO);
MODULE_PARM_DESC(skip_map_process, "skip hpb map process");

static int ufshpb_create_sysfs(struct ufs_hba *hba, struct ufshpb_lu *hpb);
static void ufshpb_error_handler(struct work_struct *work);
static void ufshpb_evict_region(struct ufshpb_lu *hpb,
						struct ufshpb_region *cb);
static void ufshpb_purge_active_block(struct ufshpb_lu *hpb);
static void ufshpb_retrieve_rsp_info(struct ufshpb_lu *hpb);
static void ufshpb_tasklet_fn(unsigned long private);

static inline void ufshpb_get_bit_offset(
		struct ufshpb_lu *hpb, int subregion_offset,
		int *dword, int *offset)
{
	*dword = subregion_offset >> bits_per_dword_shift;
	*offset = subregion_offset & bits_per_dword_mask;
}

#define _PRINT_ERR(fmt, ...) printk( KERN_ERR "%s(%d): " fmt, __func__, __LINE__, __VA_ARGS__ )
#define _PRINT_LIST_HEAD(h) _PRINT_ERR( #h "(%#p) next=%#p prev=%#p", (h), (h)->next, (h)->prev)
#define _PRINT_SUBREGION(cp) \
do { \
_PRINT_ERR( #cp "(%#p): mctx=%#p, stat=%d, reg=%#x sreg=%#x", (cp), (cp)->mctx, (cp)->subregion_state, (cp)->region, (cp)->subregion); \
_PRINT_LIST_HEAD(&(cp)->list_subregion);\
} while(0)
#define _PRINT_REGION(cb) \
do { \
_PRINT_ERR( #cb "(%#p) tbl=%#p--%#p st=%d rg=%#x sc=%d ht=%d", \
(cb), (cb)->subregion_tbl, (cb)->subregion_tbl+(cb)->subregion_count, (cb)->region_state, \
(cb)->region, (cb)->subregion_count, (cb)->hit_count); \
_PRINT_LIST_HEAD( &(cb)->list_region ); \
} while(0)
/* called with hpb_lock (irq) */
static bool ufshpb_ppn_dirty_check(struct ufshpb_lu *hpb,
		struct ufshpb_subregion *cp, int subregion_offset)
{
	bool is_dirty;
	unsigned int bit_dword, bit_offset;

	ufshpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);

	if (!cp->mctx->ppn_dirty)
		return false;

	is_dirty = cp->mctx->ppn_dirty[bit_dword] &
		(1 << bit_offset) ? true : false;

	return is_dirty;
}

static void ufshpb_ppn_prep(struct ufshpb_lu *hpb,
		struct ufshcd_lrb *lrbp, unsigned long long ppn)
{
	unsigned char cmd[16] = { 0 };

	cmd[0] = READ_16;
	cmd[2] = lrbp->cmd->cmnd[2];
	cmd[3] = lrbp->cmd->cmnd[3];
	cmd[4] = lrbp->cmd->cmnd[4];
	cmd[5] = lrbp->cmd->cmnd[5];
	memcpy(cmd+6,&ppn,8);
	cmd[14] = 0x01;		// Transfer_len = 0x01
	cmd[15] = 1;
	memcpy(lrbp->cmd->cmnd, cmd, MAX_CDB_SIZE);
	memcpy(lrbp->ucd_req_ptr->sc.cdb, cmd, MAX_CDB_SIZE);
}

/* called with hpb_lock (irq) */
static inline void ufshpb_set_dirty_bits(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb, struct ufshpb_subregion *cp,
		int dword, int offset, unsigned int count)
{
	const unsigned long mask = ((1UL << count) - 1) & 0xffffffff;

	if (cb->region_state == HPBREGION_INACTIVE)
		return;

	cp->mctx->ppn_dirty[dword] |= (mask << offset);
}

static void ufshpb_set_dirty(struct ufshpb_lu *hpb,
			struct ufshcd_lrb *lrbp, int region,
			int subregion, int subregion_offset)
{
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	int count;
	int bit_count, bit_dword, bit_offset;

	count = blk_rq_sectors(lrbp->cmd->request) >> sects_per_blk_shift;
	ufshpb_get_bit_offset(hpb, subregion_offset,
			&bit_dword, &bit_offset);

	do {
		bit_count = min(count, BITS_PER_DWORD - bit_offset);

		cb = hpb->region_tbl + region;
		cp = cb->subregion_tbl + subregion;

		ufshpb_set_dirty_bits(hpb, cb, cp,
				bit_dword, bit_offset, bit_count);

		bit_offset = 0;
		bit_dword++;

		if (bit_dword == hpb->dwords_per_subregion) {
			bit_dword = 0;
			subregion++;

			if (subregion == hpb->subregions_per_region) {
				subregion = 0;
				region++;
			}
		}

		count -= bit_count;
	} while (count);
}

static inline bool ufshpb_is_read_lrbp(struct ufshcd_lrb *lrbp)
{
	if (lrbp->cmd->cmnd[0] == READ_10 || lrbp->cmd->cmnd[0] == READ_16)
		return true;

	return false;
}

static inline bool ufshpb_is_encrypted_lrbp(struct ufshcd_lrb *lrbp)
{
	return (lrbp->utr_descriptor_ptr->header.dword_0 & UTRD_CRYPTO_ENABLE);
}

static inline bool ufshpb_is_write_discard_lrbp(struct ufshcd_lrb *lrbp)
{
	return lrbp->cmd->cmnd[0] == WRITE_10 ||
			lrbp->cmd->cmnd[0] == WRITE_16 ||
			lrbp->cmd->cmnd[0] == UNMAP;
}

static inline void ufshpb_get_pos_from_lpn(struct ufshpb_lu *hpb,
		unsigned int lpn, int *region, int *subregion, int *offset)
{
	int region_offset;

	*region = lpn >> hpb->entries_per_region_shift;
	region_offset = lpn & hpb->entries_per_region_mask;
	*subregion = region_offset >> hpb->entries_per_subregion_shift;
	*offset = region_offset & hpb->entries_per_subregion_mask;
}

static unsigned long long ufshpb_get_ppn(struct ufshpb_map_ctx *mctx, int pos)
{
	unsigned long long *ppn_table;
	int index, offset;

	index = pos / HPB_ENTREIS_PER_OS_PAGE;
	offset = pos % HPB_ENTREIS_PER_OS_PAGE;

	ppn_table = page_address(mctx->m_page[index]);
	return ppn_table[offset];
}

void ufshpb_prep_fn_toshiba(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufshpb_lu *hpb;
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	unsigned int lpn;
	unsigned long long ppn = 0;
	int region, subregion, subregion_offset;
	const struct request *rq = lrbp->cmd->request;
	unsigned long long rq_pos = blk_rq_pos(rq);
	unsigned int rq_sectors = blk_rq_sectors(rq);

	if (hba->ufshpb_state != HPB_PRESENT)
		return;
	/* WKLU could not be HPB-LU */
	if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN)
		return;

	hpb = hba->ufshpb_lup[lrbp->lun];
	if (!hpb || !hpb->lu_hpb_enable) {
		if (ufshpb_is_read_lrbp(lrbp))
			goto read_10;
		return;
	}

	if (hpb->force_disable) {
		if (ufshpb_is_read_lrbp(lrbp))
			goto read_10;
		return;
	}

#if 1
	if (rq->cmd_flags == REQ_OP_SCSI_IN ||
			rq->cmd_flags == REQ_OP_SCSI_OUT)
	{
//		printk("rq->cmd_flags =%x REQ_OP_SCSI_IN=%x REQ_OP_SCSI_OUT=%x\n", rq->cmd_flags ,REQ_OP_SCSI_IN,REQ_OP_SCSI_OUT);
		return;
	}
#endif
	/*
	 * TODO: check if ICE is not supported or not.
	 *
	 * if (ufshpb_is_read_lrbp(lrbp) && ufshpb_is_encrypted_lrbp(lrbp))
	 *	goto read_10;
	 */

	lpn = rq_pos / SECTORS_PER_BLOCK;
	ufshpb_get_pos_from_lpn(hpb, lpn, &region,
			&subregion, &subregion_offset);
	cb = hpb->region_tbl + region;

	if (ufshpb_is_write_discard_lrbp(lrbp)) {
		spin_lock_bh(&hpb->hpb_lock);

		if (cb->region_state == HPBREGION_INACTIVE) {
			spin_unlock_bh(&hpb->hpb_lock);
			return;
		}
		ufshpb_set_dirty(hpb, lrbp, region, subregion,
						subregion_offset);
		spin_unlock_bh(&hpb->hpb_lock);
		return;
	}

	if (!ufshpb_is_read_lrbp(lrbp))
		return;

	if (rq_sectors != SECTORS_PER_BLOCK) {
		hpb_trace(hpb, "%llu + %u READ_10 many_blocks %d - %d",
				rq_pos, rq_sectors, region, subregion);
		return;
	}

	cp = cb->subregion_tbl + subregion;

	spin_lock_bh(&hpb->hpb_lock);
	if (cb->region_state == HPBREGION_INACTIVE ||
			cp->subregion_state != HPBSUBREGION_CLEAN) {
		if (cb->region_state == HPBREGION_INACTIVE) {
			atomic64_inc(&hpb->region_miss);
			hpb_trace(hpb, "%llu + %u READ_10 RG_INACT %d - %d",
					rq_pos, rq_sectors, region, subregion);
		} else if (cp->subregion_state == HPBSUBREGION_DIRTY
				|| cp->subregion_state == HPBSUBREGION_ISSUED) {
			atomic64_inc(&hpb->subregion_miss);
			hpb_trace(hpb, "%llu + %u READ_10 SRG_D %d - %d",
					rq_pos, rq_sectors, region, subregion);
		} else {
			hpb_trace(hpb, "%llu + %u READ_10 ( %d %d ) %d - %d",
					rq_pos, rq_sectors,
				cb->region_state, cp->subregion_state,
				region, subregion);
		}
		spin_unlock_bh(&hpb->hpb_lock);
		return;
	}

	if (ufshpb_ppn_dirty_check(hpb, cp, subregion_offset)) {
		atomic64_inc(&hpb->entry_dirty_miss);
		hpb_trace(hpb, "%llu + %u READ_10 E_D %d - %d",
				rq_pos, rq_sectors, region, subregion);
		spin_unlock_bh(&hpb->hpb_lock);
		return;
	}

	ppn = ufshpb_get_ppn(cp->mctx, subregion_offset);
	spin_unlock_bh(&hpb->hpb_lock);

	ufshpb_ppn_prep(hpb, lrbp, ppn);
	hpb_trace(hpb, "%llu + %u READ_16 %d - %d",
			rq_pos, rq_sectors, region, subregion);
	atomic64_inc(&hpb->hit);
	return;
read_10:
	if (!hpb || !lrbp)
		return;
	hpb_trace(hpb, "%llu + %u READ_10", rq_pos, rq_sectors);
	atomic64_inc(&hpb->miss);
}

static int ufshpb_clean_dirty_bitmap(
		struct ufshpb_lu *hpb, struct ufshpb_subregion *cp)
{
	struct ufshpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == HPBREGION_INACTIVE || !cp->mctx) {
		hpb_dbg(hpb->hba, "%d - %d evicted\n",
				cp->region, cp->subregion);
		return -EINVAL;
	}

	memset(cp->mctx->ppn_dirty, 0x00,
			hpb->entries_per_subregion >> bits_per_byte_shift);
	return 0;
}

static void ufshpb_clean_active_subregion(
		struct ufshpb_lu *hpb, struct ufshpb_subregion *cp)
{
	struct ufshpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == HPBREGION_INACTIVE || !cp->mctx) {
		hpb_dbg(hpb->hba, "%d - %d evicted\n",
				cp->region, cp->subregion);
		return;
	}
	cp->subregion_state = HPBSUBREGION_CLEAN;
}

static void ufshpb_error_active_subregion(
		struct ufshpb_lu *hpb, struct ufshpb_subregion *cp)
{
	struct ufshpb_region *cb;

	cb = hpb->region_tbl + cp->region;

	/* if mctx is null, active block had been evicted out */
	if (cb->region_state == HPBREGION_INACTIVE || !cp->mctx) {
		dev_err(HPB_DEV(hpb),
			"%d - %d evicted\n", cp->region, cp->subregion);
		return;
	}
	cp->subregion_state = HPBSUBREGION_DIRTY;
}

static void ufshpb_map_compl_process(struct ufshpb_lu *hpb,
		struct ufshpb_map_req *map_req)
{
	unsigned long long debug_ppn_0, debug_ppn_last;

	map_req->RSP_end = ktime_to_ns(ktime_get());

	debug_ppn_0 = ufshpb_get_ppn(map_req->mctx, 0);
	debug_ppn_last = ufshpb_get_ppn(map_req->mctx, hpb->entries_per_subregion-1);

	hpb_trace(hpb, "Noti: C RB %d - %d",
			map_req->region, map_req->subregion);
	hpb_dbg(hpb->hba, "UFSHPB COMPL READ BUFFER %d - %d ( %llx ~ %llx )\n",
			map_req->region, map_req->subregion,
			debug_ppn_0, debug_ppn_last);
	hpb_dbg(hpb->hba, "start=%llu, issue=%llu, endio=%llu, endio=%llu\n",
			map_req->RSP_tasklet_enter1 - map_req->RSP_start,
			map_req->RSP_issue - map_req->RSP_tasklet_enter1,
			map_req->RSP_endio - map_req->RSP_issue,
			map_req->RSP_end - map_req->RSP_endio);

	spin_lock(&hpb->hpb_lock);
	ufshpb_clean_active_subregion(hpb,
			hpb->region_tbl[map_req->region].subregion_tbl +
							map_req->subregion);
	spin_unlock(&hpb->hpb_lock);
}

/*
 * Must held rsp_list_lock before enter this function
 */
static struct ufshpb_rsp_info *ufshpb_get_req_info(struct ufshpb_lu *hpb)
{
	struct ufshpb_rsp_info *rsp_info =
		list_first_entry_or_null(&hpb->lh_rsp_info_free,
				struct ufshpb_rsp_info,
				list_rsp_info);
	if (!rsp_info) {
		hpb_dbg(hpb->hba, "there is no rsp_info");
		return NULL;
	}
	list_del(&rsp_info->list_rsp_info);
	memset(rsp_info, 0x00, sizeof(struct ufshpb_rsp_info));

	INIT_LIST_HEAD(&rsp_info->list_rsp_info);

	return rsp_info;
}

static void ufshpb_map_req_compl_fn(struct request *rq, blk_status_t status)
{
	struct ufshpb_map_req *map_req =
		(struct ufshpb_map_req *)rq->end_io_data;
	struct ufs_hba *hba;
	struct ufshpb_lu *hpb;
	struct scsi_sense_hdr sshdr;
	struct ufshpb_region *cb;
	struct ufshpb_rsp_info *rsp_info;
	unsigned long flags;

	hpb = map_req->hpb;
	hba = hpb->hba;
	cb = hpb->region_tbl + map_req->region;
	map_req->RSP_endio = ktime_to_ns(ktime_get());

	if (hba->ufshpb_state != HPB_PRESENT)
		goto free_map_req;

	if (!status) {
		ufshpb_map_compl_process(hpb, map_req);
		goto free_map_req;
	}

	dev_err(HPB_DEV(hpb),
		"error status %d ( %d - %d )\n",
		status, map_req->region, map_req->subregion);
	scsi_normalize_sense(map_req->sense,
				SCSI_SENSE_BUFFERSIZE, &sshdr);
	dev_err(HPB_DEV(hpb),
		"code %x sense_key %x asc %x ascq %x\n",
				sshdr.response_code,
				sshdr.sense_key, sshdr.asc, sshdr.ascq);
	dev_err(HPB_DEV(hpb),
		"byte4 %x byte5 %x byte6 %x additional_len %x\n",
				sshdr.byte4, sshdr.byte5,
				sshdr.byte6, sshdr.additional_length);
	atomic64_inc(&hpb->rb_fail);

	if (sshdr.sense_key == ILLEGAL_REQUEST) {
		panic("Invalid inactive region %#x", map_req->region);
		spin_lock(&hpb->hpb_lock);
		if (cb->region_state == HPBREGION_PINNED) {
			if (sshdr.asc == 0x06 && sshdr.ascq == 0x01) {
				dev_err(HPB_DEV(hpb), "retry pinned rb %d - %d",
						map_req->region,
						map_req->subregion);
				INIT_LIST_HEAD(&map_req->list_map_req);
				list_add_tail(&map_req->list_map_req,
							&hpb->lh_map_req_retry);
				spin_unlock(&hpb->hpb_lock);

				schedule_delayed_work(&hpb->ufshpb_retry_work,
							msecs_to_jiffies(5000));
				return;
			}

			hpb_dbg(hpb->hba, "pinned rb %d - %d(dirty)",
					map_req->region, map_req->subregion);
			ufshpb_error_active_subregion(hpb,
					cb->subregion_tbl + map_req->subregion);
			spin_unlock(&hpb->hpb_lock);
		} else {
			spin_unlock(&hpb->hpb_lock);

			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			rsp_info = ufshpb_get_req_info(hpb);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			if (!rsp_info) {
				dev_warn(hba->dev,
					"%s:%d No rsp_info\n",
					__func__, __LINE__);
				goto free_map_req;
			}

			rsp_info->type = HPB_RSP_REQ_REGION_UPDATE;
			rsp_info->RSP_start = ktime_to_ns(ktime_get());
			rsp_info->active_cnt = 0;
			rsp_info->inactive_cnt = 1;
			rsp_info->inactive_list[0].region = map_req->region;
			hpb_dbg(hpb->hba,
				"Non-pinned rb %d is added to rsp_info_list",
				map_req->region);

			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
			list_add_tail(&rsp_info->list_rsp_info,
							&hpb->lh_rsp_info);
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

			tasklet_schedule(&hpb->ufshpb_tasklet);
		}
	}
free_map_req:
	spin_lock(&hpb->hpb_lock);
	INIT_LIST_HEAD(&map_req->list_map_req);
	list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	spin_unlock(&hpb->hpb_lock);
}

static inline void ufshpb_set_read_buf_cmd(unsigned char *cmd,
		int region, int subregion, int subregion_mem_size)
{
	cmd[0] = UFSHPB_READ_BUFFER;
	cmd[1] = 0x01;
	cmd[2] = GET_BYTE_1(region);
	cmd[3] = GET_BYTE_0(region);
	cmd[4] = GET_BYTE_1(subregion);
	cmd[5] = GET_BYTE_0(subregion);
	cmd[6] = GET_BYTE_2(subregion_mem_size);
	cmd[7] = GET_BYTE_1(subregion_mem_size);
	cmd[8] = GET_BYTE_0(subregion_mem_size);
	cmd[9] = 0x00;
}

static int ufshpb_add_bio_page(struct ufshpb_lu *hpb,
		struct request_queue *q, struct bio *bio, struct bio_vec *bvec,
		struct ufshpb_map_ctx *mctx)
{
	struct page *page = NULL;
	int i, ret;

	bio_init(bio, bvec, hpb->mpages_per_subregion);

	for (i = 0; i < hpb->mpages_per_subregion; i++) {
		/* virt_to_page(p + (OS_PAGE_SIZE * i)); */
		page = mctx->m_page[i];
		if (!page)
			return -ENOMEM;

		ret = bio_add_pc_page(q, bio, page, hpb->mpage_bytes, 0);
		if (ret != hpb->mpage_bytes) {
			dev_err(HPB_DEV(hpb), "error ret %d\n", ret);
			return -EINVAL;
		}
	}
	return 0;
}

static inline void ufshpb_issue_map_req(struct request_queue *q,
		struct request *req)
{
	unsigned long flags;

	spin_lock_irqsave(q->queue_lock, flags);
	list_add(&req->queuelist, &q->queue_head);
	spin_unlock_irqrestore(q->queue_lock, flags);
}

static int ufshpb_map_req_issue(struct ufshpb_lu *hpb,
		struct request_queue *q, struct ufshpb_map_req *map_req)
{
	struct request *rq;
	struct scsi_request *req;
	struct scsi_cmnd *cmnd;
	struct bio *bio;
	unsigned char cmd[16] = { 0 };
	int ret = 0;

	if (skip_map_process > 2) return 0;
	bio = &map_req->bio;
	ret = ufshpb_add_bio_page(hpb, q, bio, map_req->bvec, map_req->mctx);
	if (ret) {
		hpb_dbg(hpb->hba, "ufshpb_add_bio_page_error %d\n", ret);
		return ret;
	}

	ufshpb_set_read_buf_cmd(cmd, map_req->region, map_req->subregion,
				hpb->subregion_mem_size);

	rq = &map_req->req;
	blk_rq_init(q, rq);
	blk_rq_append_bio(rq, &bio);
	req = scsi_req(rq);
	cmnd = container_of(req, struct scsi_cmnd, req);
	scsi_req_init(req);
	rq->cmd_flags = REQ_OP_SCSI_IN;
	rq->rq_flags = RQF_SOFTBARRIER | RQF_QUIET | RQF_PREEMPT;

	req->cmd_len = COMMAND_SIZE(cmd[0]);
	memcpy(req->cmd, cmd, req->cmd_len);

	rq->timeout = msecs_to_jiffies(30000);
	rq->end_io_data = (void *)map_req;
	rq->end_io = ufshpb_map_req_compl_fn;
	rq->end_io_data = (void *)map_req;
	cmnd->sense_buffer = req->sense = map_req->sense;
	req->sense_len = 0;

	hpb_dbg(hpb->hba, "issue map_request: %d - %d\n",
			map_req->region, map_req->subregion);

	/* this sequence already has spin_lock_irqsave(queue_lock) */
	ufshpb_issue_map_req(q, rq);
	map_req->RSP_issue = ktime_to_ns(ktime_get());

	atomic64_inc(&hpb->map_req_cnt);

	return 0;
}

static int ufshpb_set_map_req(struct ufshpb_lu *hpb,
		int region, int subregion, struct ufshpb_map_ctx *mctx,
		struct ufshpb_rsp_info *rsp_info)
{
	struct ufshpb_map_req *map_req;

	if (skip_map_process > 1) return 0;
	spin_lock(&hpb->hpb_lock);
	map_req = list_first_entry_or_null(&hpb->lh_map_req_free,
					    struct ufshpb_map_req,
					    list_map_req);
	if (!map_req) {
		hpb_dbg(hpb->hba, "There is no map_req\n");
		spin_unlock(&hpb->hpb_lock);
		return -ENOMEM;
	}
	list_del(&map_req->list_map_req);
	memset(map_req, 0x00, sizeof(struct ufshpb_map_req));

	spin_unlock(&hpb->hpb_lock);

	map_req->hpb = hpb;
	map_req->region = region;
	map_req->subregion = subregion;
	map_req->mctx = mctx;
	map_req->lun = hpb->lun;
	map_req->RSP_start = rsp_info->RSP_start;
	map_req->RSP_tasklet_enter1 = rsp_info->RSP_tasklet_enter;

	return ufshpb_map_req_issue(hpb,
		hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue, map_req);
}

static struct ufshpb_map_ctx *ufshpb_get_map_ctx(struct ufshpb_lu *hpb)
{
	struct ufshpb_map_ctx *mctx;

	mctx = list_first_entry_or_null(&hpb->lh_map_ctx,
			struct ufshpb_map_ctx, list_table);
	if (mctx) {
		list_del_init(&mctx->list_table);
		hpb->debug_free_table--;
		return mctx;
	}
	return ERR_PTR(-ENOMEM);
}

static inline void ufshpb_add_lru_info(struct victim_select_info *lru_info,
				       struct ufshpb_region *cb)
{
	cb->region_state = HPBREGION_ACTIVE;
	list_add_tail(&cb->list_region, &lru_info->lru);
	atomic64_inc(&lru_info->active_cnt);
}

static inline void ufshpb_put_map_ctx(
		struct ufshpb_lu *hpb, struct ufshpb_map_ctx *mctx)
{
	list_add(&mctx->list_table, &hpb->lh_map_ctx);
	hpb->debug_free_table++;
}

static inline int ufshpb_add_region(struct ufshpb_lu *hpb,
					struct ufshpb_region *cb)
{
	struct victim_select_info *lru_info;
	struct ufshpb_subregion *cp;
	int subregion;
	int err = 0;

	lru_info = &hpb->lru_info;

	for (subregion = 0; subregion < cb->subregion_count; subregion++) {

		cp = cb->subregion_tbl + subregion;
		WARN_ON(cp->mctx);
		cp->mctx = ufshpb_get_map_ctx(hpb);
		if (IS_ERR(cp->mctx)) {
			err = PTR_ERR(cp->mctx);
			goto out;
		}
		cp->subregion_state = HPBSUBREGION_DIRTY;
	}
	ufshpb_add_lru_info(lru_info, cb);

	atomic64_inc(&hpb->region_add);
out:
	if (err){
		hpb_dbg(hpb->hba,
			"get mctx failed. err %d subregion %d free_table %d\n",
			err, subregion, hpb->debug_free_table);
		while( --subregion >= 0 ) {
			cp = cb->subregion_tbl + subregion;
			ufshpb_put_map_ctx(hpb, cp->mctx);
			cp->subregion_state = HPBSUBREGION_UNUSED;
		}
	}
	return err;
}

static inline void ufshpb_purge_active_page(struct ufshpb_lu *hpb,
		struct ufshpb_subregion *cp, int state)
{
	if (state == HPBSUBREGION_UNUSED) {
		ufshpb_put_map_ctx(hpb, cp->mctx);
		cp->mctx = NULL;
	}
	cp->subregion_state = state;
}

static inline void ufshpb_cleanup_lru_info(struct victim_select_info *lru_info,
					   struct ufshpb_region *cb)
{
	list_del_init(&cb->list_region);
	cb->region_state = HPBREGION_INACTIVE;
	cb->hit_count = 0;
	atomic64_dec(&lru_info->active_cnt);
}

static inline void ufshpb_evict_region(struct ufshpb_lu *hpb,
				     struct ufshpb_region *cb)
{
	struct victim_select_info *lru_info;
	struct ufshpb_subregion *cp;
	int subregion;

	lru_info = &hpb->lru_info;

	hpb_dbg(hpb->hba, "\x1b[41m\x1b[33m C->EVICT region: %d \x1b[0m\n",
		  cb->region);
	hpb_trace(hpb, "Noti: EVIC RG: %d", cb->region);

	ufshpb_cleanup_lru_info(lru_info, cb);
	atomic64_inc(&hpb->region_evict);
	for (subregion = 0; subregion < cb->subregion_count; subregion++) {
		cp = cb->subregion_tbl + subregion;
		ufshpb_purge_active_page(hpb, cp, HPBSUBREGION_UNUSED);
	}
}

static void ufshpb_hit_lru_info(struct victim_select_info *lru_info,
				       struct ufshpb_region *cb)
{
	switch (lru_info->selection_type) {
	case LRU:
		list_move_tail(&cb->list_region, &lru_info->lru);
		break;
	case LFU:
		if (cb->hit_count != 0xffffffff)
			cb->hit_count++;

		list_move_tail(&cb->list_region, &lru_info->lru);
		break;
	default:
		break;
	}
}

static struct ufshpb_region *ufshpb_victim_lru_info(
				struct victim_select_info *lru_info)
{
	struct ufshpb_region *cb;
	struct ufshpb_region *victim_cb = NULL;
	u32 hit_count = 0xffffffff;

	WARN_ON(list_empty(&lru_info->lru));
	switch (lru_info->selection_type) {
	case LRU:
		victim_cb = list_first_entry(&lru_info->lru,
				struct ufshpb_region, list_region);
		break;
	case LFU:
		list_for_each_entry(cb, &lru_info->lru, list_region) {
			if (hit_count > cb->hit_count) {
				hit_count = cb->hit_count;
				victim_cb = cb;
			}
		}
		break;
	default:
		break;
	}
	return victim_cb;
}

static int ufshpb_evict_load_region(struct ufshpb_lu *hpb,
				struct ufshpb_rsp_info *rsp_info)
{
	struct ufshpb_region *cb;
	struct ufshpb_region *victim_cb;
	struct victim_select_info *lru_info;
	int region, ret, iter;

	lru_info = &hpb->lru_info;
	hpb_dbg(hpb->hba, "active_cnt :%lld\n",
			(long long)atomic64_read(&lru_info->active_cnt));

	for (iter = 0; iter < rsp_info->inactive_cnt; iter++) {
		region = rsp_info->inactive_list[iter].region;
		cb = hpb->region_tbl + region;

		if (cb->region_state == HPBREGION_PINNED) {
			/*
			 * Pinned active-block should not drop-out.
			 * But if so, it would treat error as critical,
			 * and it will run ufshpb_eh_work
			 */
			dev_warn(hpb->hba->dev,
				 "UFSHPB pinned active-block drop-out error\n");
		}

		spin_lock(&hpb->hpb_lock);
		if (list_empty(&cb->list_region)) {
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		ufshpb_evict_region(hpb, cb);
		spin_unlock(&hpb->hpb_lock);
	}

	for (iter = 0; iter < rsp_info->active_cnt; iter++) {
		region = rsp_info->active_list[iter].region;
		cb = hpb->region_tbl + region;

		spin_lock(&hpb->hpb_lock);
		/*
		 * if already region is added to lru_list,
		 * just initiate the information of lru.
		 * because the region already has the map ctx.
		 * (!list_empty(&cb->list_region) == region->state=active...)
		 */
		if (!list_empty(&cb->list_region)) {
			ufshpb_hit_lru_info(lru_info, cb);
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		if (cb->region_state != HPBREGION_INACTIVE) {
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		if (atomic64_read(&lru_info->active_cnt) ==
				lru_info->max_lru_active_cnt) {

			victim_cb = ufshpb_victim_lru_info(lru_info);

			if (!victim_cb) {
				dev_warn(hpb->hba->dev,
						"UFSHPB victim_cb is NULL\n");
				goto unlock_error;
			}

			hpb_trace(hpb, "Noti: VT RG %d", victim_cb->region);
			hpb_dbg(hpb->hba, "max lru case. victim : %d\n",
					victim_cb->region);

			ufshpb_evict_region(hpb, victim_cb);
		}

		ret = ufshpb_add_region(hpb, cb);
		if (ret) {
			dev_warn(hpb->hba->dev,
					"UFSHPB memory allocation failed\n");
			goto unlock_error;
		}
		spin_unlock(&hpb->hpb_lock);
	}
	return 0;

unlock_error:
	spin_unlock(&hpb->hpb_lock);
	return -ENOMEM;
}

static inline struct ufshpb_rsp_field *ufshpb_get_hpb_rsp(
		struct ufshcd_lrb *lrbp)
{
	return (struct ufshpb_rsp_field *)&lrbp->ucd_rsp_ptr->sr.sense_data_len;
}

static void ufshpb_rsp_map_cmd_req(struct ufshpb_lu *hpb,
		struct ufshpb_rsp_info *rsp_info)
{
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	int region, subregion;
	int iter;
	int ret;

	if (skip_map_process > 0) return;
	/*
	 *  Before Issue read buffer CMD for active active block,
	 *  prepare the memory from memory pool.
	 */
	ret = ufshpb_evict_load_region(hpb, rsp_info);
	if (ret) {
		hpb_dbg(hpb->hba, "region evict/load failed. ret %d\n", ret);
		goto wakeup_ee_worker;
	}

	for (iter = 0; iter < rsp_info->active_cnt; iter++) {
		region = rsp_info->active_list[iter].region;
		subregion = rsp_info->active_list[iter].subregion;
		cb = hpb->region_tbl + region;

		if (region >= hpb->regions_per_lu ||
					subregion >= cb->subregion_count) {
			hpb_dbg(hpb->hba,
				"ufshpb issue-map %d - %d range error\n",
				region, subregion);
			goto wakeup_ee_worker;
		}

		cp = cb->subregion_tbl + subregion;

		/*
		 * if subregion_state set HPBSUBREGION_ISSUED,
		 * active_page has already been added to list,
		 * so it just ends function.
		 */
		spin_lock(&hpb->hpb_lock);
		if (cp->subregion_state == HPBSUBREGION_ISSUED) {
			spin_unlock(&hpb->hpb_lock);
			continue;
		}

		cp->subregion_state = HPBSUBREGION_ISSUED;

		ret = ufshpb_clean_dirty_bitmap(hpb, cp);

		spin_unlock(&hpb->hpb_lock);

		if (ret)
			continue;

		if (hpb->force_map_req_disable ||
				!hpb->hba->sdev_ufs_lu[hpb->lun] ||
				!hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue)
			return;

		ret = ufshpb_set_map_req(hpb, region, subregion,
						cp->mctx, rsp_info);
		if (ret) {
			hpb_dbg(hpb->hba, "ufshpb_set_map_req error %d\n", ret);
			goto wakeup_ee_worker;
		}
	}
	return;

wakeup_ee_worker:
	hpb->hba->ufshpb_state = HPB_FAILED;
	schedule_work(&hpb->hba->ufshpb_eh_work);
}

/* routine : isr (ufs) */
static int ufshpb_validate_rsp_info(struct ufshpb_lu *hpb, struct ufshpb_rsp_info *rsp_info)
{
	int region, subregion, iter;
	for (iter = 0; iter < rsp_info->inactive_cnt; ) {
		region = rsp_info->inactive_list[iter].region;
		if (region >= hpb->regions_per_lu) {
#ifdef UFSHPB_PANIC_ON_INVALID_REGION
			panic("Invalid inactive region %#x", region);
#else
			dev_dbg(HPB_DEV(hpb), "Invalid inactive region %#x", region);
			if ( --rsp_info->inactive_cnt > 0) {
				memcpy(rsp_info->inactive_list,
				       rsp_info->inactive_list+1,
				       sizeof(rsp_info->inactive_list[0]) * rsp_info->inactive_cnt);
			}
#endif
		} else iter++;
	}
	for (iter = 0; iter < rsp_info->active_cnt; ) {
		region = rsp_info->active_list[iter].region;
		subregion = rsp_info->active_list[iter].subregion;
		if (region >= hpb->regions_per_lu ||
		    subregion >= hpb->region_tbl[region].subregion_count) {
#ifdef UFSHPB_PANIC_ON_INVALID_REGION
			panic( "Invalid active subregion %#x-%#x active_cnt=%d", region, subregion, iter);
#else
			dev_dbg(HPB_DEV(hpb), "Invalid active subregion %#x-%#x", region, subregion);
			if ( --rsp_info->active_cnt > 0) {
				memcpy(rsp_info->active_list,
				       rsp_info->active_list+1,
				       sizeof(rsp_info->active_list[0]) * rsp_info->active_cnt);
			}
#endif
		} else iter++;
	}
	return rsp_info->inactive_cnt + rsp_info->active_cnt;
}

void ufshpb_rsp_upiu_toshiba(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufshpb_lu *hpb;
	struct ufshpb_rsp_field *rsp_field;
	struct ufshpb_rsp_info *rsp_info;
	struct ufshpb_region *region;
	int data_seg_len, num, blk_idx;
	unsigned long flags;

	if (hba->ufshpb_state != HPB_PRESENT)
		return;
	data_seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2)
		& MASK_RSP_UPIU_DATA_SEG_LEN;
	if (!data_seg_len) {
		bool do_tasklet = false;

		if (lrbp->lun >= UFS_UPIU_MAX_GENERAL_LUN)
			return;

		hpb = hba->ufshpb_lup[lrbp->lun];
		if (!hpb)
			return;

		spin_lock(&hpb->rsp_list_lock);
		do_tasklet = !list_empty(&hpb->lh_rsp_info);
		spin_unlock(&hpb->rsp_list_lock);

		if (do_tasklet)
			tasklet_schedule(&hpb->ufshpb_tasklet);
		return;
	}

	if (!(be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2)
		& MASK_RSP_HPB_UPDATE_ALERT))
		return;

	rsp_field = ufshpb_get_hpb_rsp(lrbp);

	if ((BE_BYTE(rsp_field->sense_data_len, 0) != DEV_SENSE_SEG_LEN) ||
			rsp_field->desc_type != DEV_DES_TYPE ||
			rsp_field->additional_len != DEV_ADDITIONAL_LEN ||
			rsp_field->hpb_operation == HPB_RSP_NONE ||
			rsp_field->active_region_cnt > MAX_ACTIVE_NUM ||
			rsp_field->inactive_region_cnt > MAX_INACTIVE_NUM)
		return;

	/*
	 * Fixit: It is not clear whether device's HPB info is reset when HPB Operation=02h.
	 *        Panic when such condition detected so that user can see it easily.
	 */
	if (rsp_field->hpb_operation == HPB_RSP_RESET_REGION_INFO){
		panic("Device informs HPB region information reset: lun=%d",rsp_field->lun);
	}

	if (rsp_field->lun >= UFS_UPIU_MAX_GENERAL_LUN) {
		dev_warn(hba->dev, "lun is not general = %d", rsp_field->lun);
		return;
	}

	hpb = hba->ufshpb_lup[rsp_field->lun];
	if (!hpb) {
		dev_warn(hba->dev,
			"%s:%d UFS-LU%d is not UFSHPB LU\n", __func__,
			__LINE__, lrbp->lun);
		return;
	}

	hpb_dbg(hpb->hba,
		"HPB-Info Noti: %d LUN: %d Seg-Len %d, lun = %d\n",
		rsp_field->hpb_operation, lrbp->lun,
		be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
		MASK_RSP_UPIU_DATA_SEG_LEN, rsp_field->lun);

	atomic64_inc(&hpb->rb_noti_cnt);

	if (!hpb->lu_hpb_enable) {
		dev_warn(hba->dev, "UFSHPB(%s) LU(%d) not HPB-LU\n",
				__func__, lrbp->lun);
		return;
	}

	spin_lock(&hpb->rsp_list_lock);
	rsp_info = ufshpb_get_req_info(hpb);
	spin_unlock(&hpb->rsp_list_lock);
	if (!rsp_info)
		return;

	switch (rsp_field->hpb_operation) {
	case HPB_RSP_REQ_REGION_UPDATE:
		WARN_ON(data_seg_len != DEV_DATA_SEG_LEN);
		rsp_info->type = HPB_RSP_REQ_REGION_UPDATE;

		rsp_info->RSP_start = ktime_to_ns(ktime_get());

		for (num = 0; num < rsp_field->active_region_cnt; num++) {
			blk_idx = num * PER_ACTIVE_INFO_BYTES;
			rsp_info->active_list[num].region =
				BE_BYTE(rsp_field->hpb_active_field, blk_idx);
			rsp_info->active_list[num].subregion =
				BE_BYTE(rsp_field->hpb_active_field,
								blk_idx + 2);
			hpb_dbg(hpb->hba,
				"active num: %d, #block: %d, page#: %d\n",
				num + 1,
				rsp_info->active_list[num].region,
				rsp_info->active_list[num].subregion);
		}
		rsp_info->active_cnt = num;

		for (num = 0; num < rsp_field->inactive_region_cnt; num++) {
			blk_idx = num * PER_INACTIVE_INFO_BYTES;
			rsp_info->inactive_list[num].region =
				BE_BYTE(rsp_field->hpb_inactive_field, blk_idx);
			hpb_dbg(hpb->hba, "inactive num: %d, #block: %d\n",
				  num + 1, rsp_info->inactive_list[num].region);
		}
		rsp_info->inactive_cnt = num;

		hpb_trace(hpb, "Noti: #ACTIVE %d, #INACTIVE %d",
			rsp_info->active_cnt, rsp_info->inactive_cnt);
		hpb_dbg(hpb->hba, "active cnt: %d, inactive cnt: %d\n",
			  rsp_info->active_cnt, rsp_info->inactive_cnt);
		list_for_each_entry(region, &hpb->lru_info.lru, list_region)
			hpb_dbg(hpb->hba, "active list : %d (cnt: %d)\n",
					region->region, region->hit_count);
		hpb_dbg(hpb->hba, "add_list %p -> %p\n",
					rsp_info, &hpb->lh_rsp_info);

//		if (ufshpb_validate_rsp_info(hpb, rsp_info)) {
			spin_lock(&hpb->rsp_list_lock);
			list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info);
			spin_unlock(&hpb->rsp_list_lock);
			tasklet_schedule(&hpb->ufshpb_tasklet);
//		} else {
//			spin_lock_irqsave(&hpb->rsp_list_lock, flags);
//			list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
//			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
//		}

		break;
	case HPB_RSP_RESET_REGION_INFO:
		WARN_ON(data_seg_len != DEV_DATA_SEG_LEN);
		rsp_info->type = HPB_RSP_RESET_REGION_INFO;
		spin_lock(&hpb->rsp_list_lock);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info);
		spin_unlock(&hpb->rsp_list_lock);

		tasklet_schedule(&hpb->ufshpb_tasklet);
		break;
	default:
		hpb_dbg(hpb->hba, "hpb_operation is not available : %d\n",
					rsp_field->hpb_operation);
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		break;
	}
}

static int ufshpb_read_device_desc(struct ufs_hba *hba, u8 *desc_buf, u32 size)
{
	return ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_DEVICE, 0, 0, desc_buf, &size);
}

static int ufshpb_read_geo_desc(struct ufs_hba *hba, u8 *desc_buf, u32 size)
{
	return ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_GEOMETRY, 0, 0, desc_buf, &size);
}

static int ufshpb_read_unit_desc(struct ufs_hba *hba, int lun,
						u8 *desc_buf, u32 size)
{
	return ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
			QUERY_DESC_IDN_UNIT, lun, 0, desc_buf, &size);
}

static inline void ufshpb_add_subregion_to_req_list(struct ufshpb_lu *hpb,
		struct ufshpb_subregion *cp)
{
	list_add_tail(&cp->list_subregion, &hpb->lh_subregion_req);
	cp->subregion_state = HPBSUBREGION_ISSUED;
}

static int ufshpb_execute_req(struct ufshpb_lu *hpb, unsigned char *cmd,
		struct ufshpb_subregion *cp)
{
	unsigned long flags;
	struct request_queue *q;
	char sense[SCSI_SENSE_BUFFERSIZE];
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdp;
	struct ufs_hba *hba = hpb->hba;
	struct request *rq;
	struct scsi_cmnd *cmnd;
	struct scsi_request *req;
	struct bio bio, *biop = &bio;
	int ret = 0;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdp = hba->sdev_ufs_lu[hpb->lun];
	if (sdp) {
		ret = scsi_device_get(sdp);
		if (!ret && !scsi_device_online(sdp)) {
			ret = -ENODEV;
			scsi_device_put(sdp);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	q = sdp->request_queue;
	ret = ufshpb_add_bio_page(hpb, q, &bio, hpb->bvec, cp->mctx);
	if (ret)
		goto put_out;

	rq = blk_get_request(q, REQ_OP_SCSI_IN, GFP_KERNEL);
	if (IS_ERR(rq)) {
		ret =  PTR_ERR(rq);
		goto put_out;
	}
	blk_rq_append_bio(rq, &biop);
	cmnd = blk_mq_rq_to_pdu(rq);
	req = &cmnd->req;

	req->cmd_len = COMMAND_SIZE(cmd[0]);
	memcpy(req->cmd, cmd, req->cmd_len);
	req->sense = sense;
	req->sense_len = 0;
	req->retries = 3;
	rq->timeout = msecs_to_jiffies(10000);

	blk_execute_rq(q, NULL, rq, 1);
	if (cmnd->result) {
		ret = -EIO;
		scsi_normalize_sense(req->sense, SCSI_SENSE_BUFFERSIZE, &sshdr);
		dev_err(HPB_DEV(hpb),
				"code %x sense_key %x asc %x ascq %x",
				sshdr.response_code, sshdr.sense_key, sshdr.asc,
				sshdr.ascq);
		dev_err(HPB_DEV(hpb),
				"byte4 %x byte5 %x byte6 %x additional_len(ufshpb_execute_req) %x",
				sshdr.byte4, sshdr.byte5, sshdr.byte6,
				sshdr.additional_length);
		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_error_active_subregion(hpb, cp);
		spin_unlock_bh(&hpb->hpb_lock);
	} else {
		ret = 0;
		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_clean_dirty_bitmap(hpb, cp);
		spin_unlock_bh(&hpb->hpb_lock);
	}
	blk_put_request(rq);
put_out:
	scsi_device_put(sdp);
	return ret;
}

static int ufshpb_issue_map_req_from_list(struct ufshpb_lu *hpb)
{
	struct ufshpb_subregion *cp, *next_cp;
	int ret;

	LIST_HEAD(req_list);

	spin_lock(&hpb->hpb_lock);
	list_splice_init(&hpb->lh_subregion_req, &req_list);
	spin_unlock(&hpb->hpb_lock);

	list_for_each_entry_safe(cp, next_cp, &req_list, list_subregion) {
		unsigned char cmd[10] = { 0 };

		if ((cp->subregion_state == HPBSUBREGION_ISSUED) ||
		    (cp->subregion_state == HPBSUBREGION_CLEAN))
			/* already activated while in queue */
			continue;

		ufshpb_set_read_buf_cmd(cmd, cp->region, cp->subregion,
					hpb->subregion_mem_size);

		hpb_dbg(hpb->hba, "issue map_request: %d - %d\n",
				cp->region, cp->subregion);

		ret = ufshpb_execute_req(hpb, cmd, cp);
		if (ret < 0) {
			dev_err(HPB_DEV(hpb),
					"region %d sub %d failed with err %d",
					cp->region, cp->subregion, ret);
			continue;
		}

		spin_lock_bh(&hpb->hpb_lock);
		ufshpb_clean_active_subregion(hpb, cp);
		list_del_init(&cp->list_subregion);
		spin_unlock_bh(&hpb->hpb_lock);
	}

	return 0;
}

static void ufshpb_work_handler(struct work_struct *work)
{
	struct ufshpb_lu *hpb;
	int ret;

	hpb = container_of(work, struct ufshpb_lu, ufshpb_work);
	hpb_dbg(hpb->hba, "worker start\n");

	if (!list_empty(&hpb->lh_subregion_req)) {
		pm_runtime_get_sync(HPB_DEV(hpb));
		ret = ufshpb_issue_map_req_from_list(hpb);
		/*
		 * if its function failed at init time,
		 * ufshpb-device will request map-req,
		 * so it is not critical-error, and just finish work-handler
		 */
		if (ret)
			hpb_dbg(hpb->hba, "failed map-issue. ret %d\n", ret);
		pm_runtime_mark_last_busy(HPB_DEV(hpb));
		pm_runtime_put_noidle(HPB_DEV(hpb));
	}

	hpb_dbg(hpb->hba, "worker end\n");
}

static void ufshpb_retry_work_handler(struct work_struct *work)
{
	struct ufshpb_lu *hpb;
	struct delayed_work *dwork = to_delayed_work(work);
	struct ufshpb_map_req *map_req;
	int ret = 0;

	LIST_HEAD(retry_list);

	hpb = container_of(dwork, struct ufshpb_lu, ufshpb_retry_work);
	hpb_dbg(hpb->hba, "retry worker start");

	spin_lock_bh(&hpb->hpb_lock);
	list_splice_init(&hpb->lh_map_req_retry, &retry_list);
	spin_unlock_bh(&hpb->hpb_lock);

	while (1) {
		map_req = list_first_entry_or_null(&retry_list,
				struct ufshpb_map_req, list_map_req);
		if (!map_req) {
			hpb_dbg(hpb->hba, "There is no map_req");
			break;
		}
		list_del(&map_req->list_map_req);

		map_req->retry_cnt++;

		ret = ufshpb_map_req_issue(hpb,
				hpb->hba->sdev_ufs_lu[hpb->lun]->request_queue,
				map_req);
		if (ret) {
			hpb_dbg(hpb->hba, "ufshpb_set_map_req error %d", ret);
			goto wakeup_ee_worker;
		}
	}
	hpb_dbg(hpb->hba, "worker end");
	return;

wakeup_ee_worker:
	hpb->hba->ufshpb_state = HPB_FAILED;
	schedule_work(&hpb->hba->ufshpb_eh_work);
}

static inline void ufshpb_cancel_jobs(struct ufshpb_lu *hpb)
{
	cancel_work_sync(&hpb->ufshpb_work);
	cancel_delayed_work_sync(&hpb->ufshpb_retry_work);
	tasklet_kill(&hpb->ufshpb_tasklet);
}

static void ufshpb_reset_lu(struct ufshpb_lu* hpb) {
	struct ufs_hba *hba = hpb->hba;

	ufshpb_cancel_jobs(hpb);
	ufshpb_retrieve_rsp_info(hpb);
	ufshpb_purge_active_block(hpb);
	dev_info(hba->dev, "UFSHPB lun %d reset\n", hpb->lun);
	tasklet_init(&hpb->ufshpb_tasklet, ufshpb_tasklet_fn, (unsigned long) hpb);
}

static void ufshpb_tasklet_fn(unsigned long private)
{
	struct ufshpb_lu *hpb = (struct ufshpb_lu *)private;
	struct ufshpb_rsp_info *rsp_info;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
				struct ufshpb_rsp_info, list_rsp_info);
		if (!rsp_info) {
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			break;
		}

		rsp_info->RSP_tasklet_enter = ktime_to_ns(ktime_get());

		list_del_init(&rsp_info->list_rsp_info);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);

		if (rsp_info->type == HPB_RSP_REQ_REGION_UPDATE) {
			if ( ufshpb_validate_rsp_info(hpb, rsp_info ) > 0 ){
				ufshpb_rsp_map_cmd_req(hpb, rsp_info);
			}
		} else if (rsp_info->type == HPB_RSP_RESET_REGION_INFO) {
			ufshpb_reset_lu(hpb);
			break;
		}

		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
	}
}

static void ufshpb_init_constant(void)
{
	sects_per_blk_shift = ffs(BLOCK) - ffs(SECTOR);
	dev_dbg(NULL, "sects_per_blk_shift: %u %u\n",
		  sects_per_blk_shift, ffs(SECTORS_PER_BLOCK) - 1);

	bits_per_dword_shift = ffs(BITS_PER_DWORD) - 1;
	bits_per_dword_mask = BITS_PER_DWORD - 1;
	dev_dbg(NULL, "bits_per_dword %u shift %u mask 0x%X\n",
		  BITS_PER_DWORD, bits_per_dword_shift, bits_per_dword_mask);

	bits_per_byte_shift = ffs(BITS_PER_BYTE) - 1;
	dev_dbg(NULL, "bits_per_byte %u shift %u\n",
		  BITS_PER_BYTE, bits_per_byte_shift);
}

static inline void ufshpb_req_mempool_remove(struct ufshpb_lu *hpb)
{
	if (!hpb)
		return;
	kfree(hpb->rsp_info);
	vfree(hpb->map_req);
}

static void ufshpb_table_mempool_remove(struct ufshpb_lu *hpb)
{
	struct ufshpb_map_ctx *mctx, *next;
	int i;

	/*
	 * the mctx in the lh_map_ctx has been allocated completely.
	 */
	list_for_each_entry_safe(mctx, next, &hpb->lh_map_ctx, list_table) {
		for (i = 0; i < hpb->mpages_per_subregion; i++)
			__free_page(mctx->m_page[i]);

		vfree(mctx->ppn_dirty);
		kfree(mctx->m_page);
		kfree(mctx);
		alloc_mctx--;
	}
}

static int ufshpb_init_pinned_active_block(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb)
{
	struct ufshpb_subregion *cp;
	int subregion;
	int err = 0;

	err = ufshpb_add_region(hpb, cb);
	if (err)
		return err;

	for (subregion = 0 ; subregion < cb->subregion_count ; subregion++) {
		cp = cb->subregion_tbl + subregion;

		spin_lock(&hpb->hpb_lock);
		ufshpb_add_subregion_to_req_list(hpb, cp);
		spin_unlock(&hpb->hpb_lock);
	}

	cb->region_state = HPBREGION_PINNED;
	return 0;

}

static inline bool ufshpb_is_HPBREGION_PINNED(
		struct ufshpb_lu_desc *lu_desc, int region)
{
	if (lu_desc->lu_hpb_pinned_end_offset != -1 &&
			region >= lu_desc->hpb_pinned_region_startidx &&
				region <= lu_desc->lu_hpb_pinned_end_offset)
		return true;

	return false;
}

static inline void ufshpb_init_jobs(struct ufshpb_lu *hpb)
{
	INIT_WORK(&hpb->ufshpb_work, ufshpb_work_handler);
	INIT_DELAYED_WORK(&hpb->ufshpb_retry_work, ufshpb_retry_work_handler);
	tasklet_init(&hpb->ufshpb_tasklet, ufshpb_tasklet_fn,
							(unsigned long)hpb);
}

static void ufshpb_init_subregion_tbl(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb)
{
	int subregion;

	for (subregion = 0 ; subregion < cb->subregion_count ; subregion++) {
		struct ufshpb_subregion *cp = cb->subregion_tbl + subregion;

		cp->region = cb->region;
		cp->subregion = subregion;
		cp->subregion_state = HPBSUBREGION_UNUSED;
	}
}

static inline int ufshpb_alloc_subregion_tbl(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb, int subregion_count)
{
	cb->subregion_tbl = kvzalloc(
			sizeof(struct ufshpb_subregion) * subregion_count,
			GFP_KERNEL);
	if (!cb->subregion_tbl)
		return -ENOMEM;

	cb->subregion_count = subregion_count;
	hpb_dbg(hpb->hba,
		"region %d subregion_count %d active_page_table bytes %lu\n",
		cb->region, subregion_count,
		sizeof(struct ufshpb_subregion *) * hpb->subregions_per_region);

	return 0;
}

static int ufshpb_table_mempool_init(struct ufshpb_lu *hpb,
		int num_regions, int subregions_per_region,
		int entry_count, int entry_byte)
{
	int i, j;
	struct ufshpb_map_ctx *mctx = NULL;

	INIT_LIST_HEAD(&hpb->lh_map_ctx);

	for (i = 0 ; i < num_regions * subregions_per_region ; i++) {
		mctx = kzalloc(sizeof(struct ufshpb_map_ctx), GFP_KERNEL);
		if (!mctx)
			goto release_mem;

		mctx->m_page = kzalloc(sizeof(struct page *) *
					hpb->mpages_per_subregion, GFP_KERNEL);
		if (!mctx->m_page)
			goto release_mem;

		mctx->ppn_dirty = vzalloc(entry_count >>
						bits_per_byte_shift);
		if (!mctx->ppn_dirty)
			goto release_mem;

		for (j = 0; j < hpb->mpages_per_subregion; j++) {
			mctx->m_page[j] = alloc_page(GFP_KERNEL | __GFP_ZERO);
			if (!mctx->m_page[j])
				goto release_mem;
		}
		hpb_dbg(hpb->hba, "[%d] mctx->m_page %p get_order %d\n", i,
			  mctx->m_page, get_order(hpb->mpages_per_subregion));

		INIT_LIST_HEAD(&mctx->list_table);
		list_add(&mctx->list_table, &hpb->lh_map_ctx);

		hpb->debug_free_table++;
	}

	alloc_mctx = num_regions * subregions_per_region;
	hpb_dbg(hpb->hba, "number of mctx %d %d %d. debug_free_table %d\n",
		  num_regions * subregions_per_region, num_regions,
		  subregions_per_region, hpb->debug_free_table);
	return 0;

release_mem:
	/*
	 * mctxs already added in lh_map_ctx will be removed
	 * in the caller function.
	 */
	if (!mctx)
		goto out;

	if (mctx->m_page) {
		for (j = 0; j < hpb->mpages_per_subregion; j++)
			if (mctx->m_page[i])
				__free_page(mctx->m_page[j]);
		kfree(mctx->m_page);
	}
	if (mctx->ppn_dirty)
		vfree(mctx->ppn_dirty);
	kfree(mctx);
out:
	return -ENOMEM;
}

static int ufshpb_req_mempool_init(struct ufs_hba *hba,
				struct ufshpb_lu *hpb, int queue_depth)
{
	struct ufshpb_rsp_info *rsp_info = NULL;
	struct ufshpb_map_req *map_req = NULL;
	int map_req_cnt = 0;
	int i;

	if (!queue_depth) {
		queue_depth = hba->nutrs;
		hpb_dbg(hba,
			"lu_queue_depth is 0. we use device's queue info.\n");
		hpb_dbg(hba, "hba->nutrs = %d\n", hba->nutrs);
	}

	INIT_LIST_HEAD(&hpb->lh_rsp_info_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_free);
	INIT_LIST_HEAD(&hpb->lh_map_req_retry);

	hpb->rsp_info = kcalloc(queue_depth, sizeof(struct ufshpb_rsp_info),
								GFP_KERNEL);
	if (!hpb->rsp_info)
		goto release_mem;

	map_req_cnt = max(hpb->subregions_per_lu, queue_depth);

	hpb->map_req = vmalloc(map_req_cnt * sizeof(struct ufshpb_map_req));
	if (!hpb->map_req)
		goto release_mem;

	for (i = 0; i < queue_depth; i++) {
		rsp_info = hpb->rsp_info + i;
		INIT_LIST_HEAD(&rsp_info->list_rsp_info);
		list_add_tail(&rsp_info->list_rsp_info, &hpb->lh_rsp_info_free);
	}

	for (i = 0; i < map_req_cnt; i++) {
		map_req = hpb->map_req + i;
		INIT_LIST_HEAD(&map_req->list_map_req);
		list_add_tail(&map_req->list_map_req, &hpb->lh_map_req_free);
	}

	return 0;

release_mem:
	kfree(hpb->rsp_info);
	return -ENOMEM;
}

static void ufshpb_init_lu_constant(struct ufshpb_lu *hpb,
		struct ufshpb_lu_desc *lu_desc,
		struct ufshpb_func_desc *func_desc)
{
	unsigned long long region_unit_size, region_mem_size;
	int entries_per_region;

	/*	From descriptors	*/
	region_unit_size = (unsigned long long)
		SECTOR * (0x01 << func_desc->hpb_region_size);
	region_mem_size = region_unit_size / BLOCK * HPB_ENTRY_SIZE;

	hpb->subregion_unit_size = (unsigned long long)
		SECTOR * (0x01 << func_desc->hpb_subregion_size);
	hpb->subregion_mem_size = hpb->subregion_unit_size /
						BLOCK * HPB_ENTRY_SIZE;

	hpb->hpb_ver = func_desc->hpb_ver;
	hpb->lu_max_active_regions = lu_desc->lu_max_active_hpb_regions;
	hpb->lru_info.max_lru_active_cnt =
					lu_desc->lu_max_active_hpb_regions;

	/*	relation : lu <-> region <-> sub region <-> entry	 */
	hpb->lu_num_blocks = lu_desc->lu_logblk_cnt;
	entries_per_region = region_mem_size / HPB_ENTRY_SIZE;
	hpb->entries_per_subregion = hpb->subregion_mem_size / HPB_ENTRY_SIZE;
	hpb->subregions_per_region = region_mem_size / hpb->subregion_mem_size;

	/*
	 * 1. regions_per_lu
	 *		= (lu_num_blocks * 4096) / region_unit_size
	 *		= (lu_num_blocks * HPB_ENTRY_SIZE) / region_mem_size
	 *		= lu_num_blocks / (region_mem_size / HPB_ENTRY_SIZE)
	 *
	 * 2. regions_per_lu = lu_num_blocks / subregion_mem_size (is trik...)
	 *    if HPB_ENTRY_SIZE != subregions_per_region, it is error.
	 */
	hpb->regions_per_lu = ((unsigned long long)hpb->lu_num_blocks
			+ (region_mem_size / HPB_ENTRY_SIZE) - 1)
			/ (region_mem_size / HPB_ENTRY_SIZE);
	hpb->subregions_per_lu = ((unsigned long long)hpb->lu_num_blocks
			+ (hpb->subregion_mem_size / HPB_ENTRY_SIZE) - 1)
			/ (hpb->subregion_mem_size / HPB_ENTRY_SIZE);

	/*	mempool info	*/
	hpb->mpage_bytes = OS_PAGE_SIZE;
	hpb->mpages_per_subregion = hpb->subregion_mem_size / hpb->mpage_bytes;

	/*	Bitmask Info.	 */
	hpb->dwords_per_subregion = hpb->entries_per_subregion / BITS_PER_DWORD;
	hpb->entries_per_region_shift = ffs(entries_per_region) - 1;
	hpb->entries_per_region_mask = entries_per_region - 1;
	hpb->entries_per_subregion_shift = ffs(hpb->entries_per_subregion) - 1;
	hpb->entries_per_subregion_mask = hpb->entries_per_subregion - 1;

	pr_info("===== From Device Descriptor! =====\n");
	pr_info("hpb_region_size = %d, hpb_subregion_size = %d\n",
			func_desc->hpb_region_size,
			func_desc->hpb_subregion_size);
	pr_info("=====   Constant Values(LU)   =====\n");
	pr_info("region_unit_size = %llu, region_mem_size %llu\n",
			region_unit_size, region_mem_size);
	pr_info("subregion_unit_size = %llu, subregion_mem_size %d\n",
			hpb->subregion_unit_size, hpb->subregion_mem_size);
	pr_info("lu_num_blks = %d, reg_per_lu = %d, subreg_per_lu = %d\n",
			hpb->lu_num_blocks, hpb->regions_per_lu,
			hpb->subregions_per_lu);
	pr_info("subregions_per_region = %d\n",
			hpb->subregions_per_region);
	pr_info("entries_per_region %u shift %u mask 0x%X\n",
			entries_per_region, hpb->entries_per_region_shift,
			hpb->entries_per_region_mask);
	pr_info("entries_per_subregion %u shift %u mask 0x%X\n",
			hpb->entries_per_subregion,
			hpb->entries_per_subregion_shift,
			hpb->entries_per_subregion_mask);
	pr_info("mpages_per_subregion : %d\n",
			hpb->mpages_per_subregion);
	pr_info("===================================\n");
}

static int ufshpb_lu_hpb_init(struct ufs_hba *hba, struct ufshpb_lu *hpb,
		struct ufshpb_func_desc *func_desc,
		struct ufshpb_lu_desc *lu_desc, int lun)
{
	struct ufshpb_region *region_table, *cb;
	struct ufshpb_subregion *cp;
	int region, subregion;
	int total_subregion_count, subregion_count;
	bool do_work_handler;
	int ret, j;

	hpb->lu_hpb_enable = true;

	ufshpb_init_lu_constant(hpb, lu_desc, func_desc);

	region_table = kzalloc(sizeof(struct ufshpb_region) *
				hpb->regions_per_lu, GFP_KERNEL);
	if (!region_table)
		return -ENOMEM;

	hpb_dbg(hba, "active_block_table bytes: %lu\n",
		(sizeof(struct ufshpb_region) * hpb->regions_per_lu));

	hpb->region_tbl = region_table;

	spin_lock_init(&hpb->hpb_lock);
	spin_lock_init(&hpb->rsp_list_lock);

	/* init lru information */
	INIT_LIST_HEAD(&hpb->lru_info.lru);
	hpb->lru_info.selection_type = LRU;

	INIT_LIST_HEAD(&hpb->lh_subregion_req);
	INIT_LIST_HEAD(&hpb->lh_rsp_info);
	INIT_LIST_HEAD(&hpb->lh_map_ctx);

	ret = ufshpb_table_mempool_init(hpb,
			lu_desc->lu_max_active_hpb_regions,
			hpb->subregions_per_region,
			hpb->entries_per_subregion, HPB_ENTRY_SIZE);
	if (ret) {
		dev_err(HPB_DEV(hpb), "ppn table mempool init fail!\n");
		goto release_mempool;
	}

	ret = ufshpb_req_mempool_init(hba, hpb, lu_desc->lu_queue_depth);
	if (ret) {
		dev_err(HPB_DEV(hpb), "rsp_info_mempool init fail!\n");
		goto release_mempool;
	}

	total_subregion_count = hpb->subregions_per_lu;

	ufshpb_init_jobs(hpb);

	hpb_dbg(hba, "total_subregion_count: %d\n", total_subregion_count);
	for (region = 0, subregion_count = 0,
			total_subregion_count = hpb->subregions_per_lu;
			region < hpb->regions_per_lu;
			region++, total_subregion_count -= subregion_count) {
		cb = region_table + region;
		cb->region = region;

		/* init lru region information*/
		INIT_LIST_HEAD(&cb->list_region);
		cb->hit_count = 0;

		subregion_count = min(total_subregion_count,
				hpb->subregions_per_region);
		hpb_dbg(hba, "total: %d subregion_count: %d\n",
				total_subregion_count, subregion_count);

		ret = ufshpb_alloc_subregion_tbl(hpb, cb, subregion_count);
		if (ret)
			goto release_region_cp;
		ufshpb_init_subregion_tbl(hpb, cb);

		if (ufshpb_is_HPBREGION_PINNED(lu_desc, region)) {
			hpb_dbg(hba, "region: %d PINNED %d ~ %d\n",
				region, lu_desc->hpb_pinned_region_startidx,
				lu_desc->lu_hpb_pinned_end_offset);
			ret = ufshpb_init_pinned_active_block(hpb, cb);
			if (ret)
				goto release_region_cp;
			do_work_handler = true;
		} else {
			hpb_dbg(hba, "region: %d inactive\n", cb->region);
			cb->region_state = HPBREGION_INACTIVE;
		}
	}

	if (total_subregion_count != 0) {
		dev_err(HPB_DEV(hpb),
			"error total_subregion_count: %d\n",
			total_subregion_count);
		goto release_region_cp;
	}

	hpb->hba = hba;
	hpb->lun = lun;

	if (do_work_handler)
		schedule_work(&hpb->ufshpb_work);

	/*
	 * even if creating sysfs failed, ufshpb could run normally.
	 * so we don't deal with error handling
	 */
	ufshpb_create_sysfs(hba, hpb);
	return 0;

release_region_cp:
	for (j = 0 ; j < region ; j++) {
		cb = region_table + j;
		if (cb->subregion_tbl) {
			for (subregion = 0; subregion < cb->subregion_count;
								subregion++) {
				cp = cb->subregion_tbl + subregion;

				if (cp->mctx)
					ufshpb_put_map_ctx(hpb, cp->mctx);
			}
			kfree(cb->subregion_tbl);
		}
	}

release_mempool:
	ufshpb_table_mempool_remove(hpb);
	kfree(region_table);
	return ret;
}

static int ufshpb_get_hpb_lu_desc(struct ufs_hba *hba,
		struct ufshpb_lu_desc *lu_desc, int lun)
{
	int ret;
	u8 logical_buf[UFSHPB_QUERY_DESC_UNIT_MAX_SIZE] = { 0 };

	ret = ufshpb_read_unit_desc(hba, lun, logical_buf,
				UFSHPB_QUERY_DESC_UNIT_MAX_SIZE);
	if (ret) {
		dev_err(hba->dev, "read unit desc failed. ret %d\n", ret);
		return ret;
	}

	lu_desc->lu_queue_depth = logical_buf[UNIT_DESC_PARAM_LU_Q_DEPTH];

	// 2^log, ex) 0x0C = 4KB
	lu_desc->lu_logblk_size = logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_SIZE];
	lu_desc->lu_logblk_cnt =
		SHIFT_BYTE_7((u64)
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT]) |
		SHIFT_BYTE_6((u64)
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 1]) |
		SHIFT_BYTE_5((u64)
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 2]) |
		SHIFT_BYTE_4((u64)
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 3]) |
		SHIFT_BYTE_3(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 4]) |
		SHIFT_BYTE_2(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 5]) |
		SHIFT_BYTE_1(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 6]) |
		SHIFT_BYTE_0(
			logical_buf[UNIT_DESC_PARAM_LOGICAL_BLK_COUNT + 7]);

	if (logical_buf[UNIT_DESC_PARAM_LU_ENABLE] == LU_HPB_ENABLE)
		lu_desc->lu_hpb_enable = true;
	else
		lu_desc->lu_hpb_enable = false;

	lu_desc->lu_max_active_hpb_regions =
		BE_BYTE(logical_buf, UNIT_DESC_HPB_LU_MAX_ACTIVE_REGIONS);
	lu_desc->hpb_pinned_region_startidx =
		BE_BYTE(logical_buf, UNIT_DESC_HPB_LU_PIN_REGION_START_OFFSET);
	lu_desc->lu_num_hpb_pinned_regions =
		BE_BYTE(logical_buf, UNIT_DESC_HPB_LU_NUM_PIN_REGIONS);

	if (lu_desc->lu_hpb_enable) {
		hpb_dbg(hba, "LUN(%d) [0A] bLogicalBlockSize %d\n",
				lun, lu_desc->lu_logblk_size);
		hpb_dbg(hba, "LUN(%d) [0B] qLogicalBlockCount %llu\n",
				lun, lu_desc->lu_logblk_cnt);
		hpb_dbg(hba, "LUN(%d) [03] bLuEnable %d\n",
				lun, logical_buf[UNIT_DESC_PARAM_LU_ENABLE]);
		hpb_dbg(hba, "LUN(%d) [06] bLuQueueDepth %d\n",
				lun, lu_desc->lu_queue_depth);
		hpb_dbg(hba, "LUN(%d) [23:24] wLUMaxActiveHPBRegions %d\n",
				lun, lu_desc->lu_max_active_hpb_regions);
		hpb_dbg(hba, "LUN(%d) [25:26] wHPBPinnedRegionStartIdx %d\n",
				lun, lu_desc->hpb_pinned_region_startidx);
		hpb_dbg(hba, "LUN(%d) [27:28] wNumHPBPinnedRegions %d\n",
				lun, lu_desc->lu_num_hpb_pinned_regions);
	}

	if (lu_desc->lu_num_hpb_pinned_regions > 0) {
		lu_desc->lu_hpb_pinned_end_offset =
			lu_desc->hpb_pinned_region_startidx +
			lu_desc->lu_num_hpb_pinned_regions - 1;
	} else
		lu_desc->lu_hpb_pinned_end_offset = -1;

	if (lu_desc->lu_hpb_enable)
		pr_info("UFSHPB: Enable, LU: %d, REG: %d, PIN: %d - %d\n",
			lun,
			lu_desc->lu_max_active_hpb_regions,
			lu_desc->hpb_pinned_region_startidx,
			lu_desc->lu_num_hpb_pinned_regions);
	return 0;
}

static int ufshpb_read_dev_desc_support(struct ufs_hba *hba,
		struct ufshpb_func_desc *desc)
{
	u8 desc_buf[UFSHPB_QUERY_DESC_DEVICE_MAX_SIZE];
	int err;

	if (!ufshcd_is_hpb_supported(hba))
		return -ENODEV;

	err = ufshpb_read_device_desc(hba, desc_buf,
			UFSHPB_QUERY_DESC_DEVICE_MAX_SIZE);
	if (err)
		return err;


	desc->lu_cnt = desc_buf[DEVICE_DESC_PARAM_NUM_LU];
	hpb_dbg(hba, "device lu count %d\n", desc->lu_cnt);

	desc->hpb_ver = get_unaligned_be16(desc_buf + DEVICE_DESC_PARAM_HPB_VER );

	pr_info("UFSHPB: Version = %.2x.%.2x, Driver Version = %.2x.%.2x\n",
			GET_BYTE_1(desc->hpb_ver),
			GET_BYTE_0(desc->hpb_ver),
			GET_BYTE_1(UFSHPB_VER),
			GET_BYTE_0(UFSHPB_VER));

	if (desc->hpb_ver != UFSHPB_VER)
		return -ENODEV;
	return 0;
}

static int ufshpb_read_geo_desc_support(struct ufs_hba *hba,
		struct ufshpb_func_desc *desc)
{
	int err;
	u8 geometry_buf[UFSHPB_QUERY_DESC_GEOMETRY_MAX_SIZE];

	err = ufshpb_read_geo_desc(hba, geometry_buf,
				UFSHPB_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (err)
		return err;

	desc->hpb_region_size = geometry_buf[GEOMETRY_DESC_HPB_REGION_SIZE];
	desc->hpb_number_lu = geometry_buf[GEOMETRY_DESC_HPB_NUMBER_LU];
	desc->hpb_subregion_size =
			geometry_buf[GEOMETRY_DESC_HPB_SUBREGION_SIZE];
	desc->hpb_device_max_active_regions =
			get_unaligned_be16(geometry_buf
					+ GEOMETRY_DESC_HPB_DEVICE_MAX_ACTIVE_REGIONS);

	hpb_dbg(hba, "[48] bHPBRegionSiz %u\n", desc->hpb_region_size);
	hpb_dbg(hba, "[49] bHPBNumberLU %u\n", desc->hpb_number_lu);
	hpb_dbg(hba, "[4A] bHPBSubRegionSize %u\n", desc->hpb_subregion_size);
	hpb_dbg(hba, "[4B:4C] wDeviceMaxActiveHPBRegions %u\n",
			desc->hpb_device_max_active_regions);

	if (desc->hpb_number_lu == 0) {
		dev_warn(hba->dev, "UFSHPB: HPB is not supported\n");
		return -ENODEV;
	}
	/* for activation */
	hba->ufshpb_max_regions = desc->hpb_device_max_active_regions;
	return 0;
}

static int ufshpb_inform_host_reset(struct ufs_hba *hba)
{
	int i;
	int err;
	bool flag_res = 1;

	err = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_SET_FLAG,
		QUERY_FLAG_IDN_HPB_RESET, NULL);
	if (err) {
		dev_err(hba->dev,
			"%s setting fHPBReset flag failed with error %d\n",
			__func__, err);
		goto out;
	}

	/* poll for max. 1000 iterations for fDeviceInit flag to clear */
	for (i = 0; i < 1000 && !err && flag_res; i++)
		err = ufshcd_query_flag(hba, UPIU_QUERY_OPCODE_READ_FLAG,
			QUERY_FLAG_IDN_HPB_RESET, &flag_res);

	if (err)
		dev_err(hba->dev,
			"%s reading fHPBReset flag failed with error %d\n",
			__func__, err);
	else if (flag_res)
		dev_err(hba->dev,
			"%s fHPBReset was not cleared by the device\n",
			__func__);

out:
	return err;
}

static int ufshpb_init(struct ufs_hba *hba)
{
	struct ufshpb_func_desc func_desc;
	int lun, ret;
	int hpb_dev = 0;

	pm_runtime_get_sync(hba->dev);

	ret = ufshpb_read_dev_desc_support(hba, &func_desc);
	if (ret)
		goto out_state;

	ret = ufshpb_read_geo_desc_support(hba, &func_desc);
	if (ret)
		goto out_state;

	ufshpb_init_constant();
	ufshpb_inform_host_reset(hba);
	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		struct ufshpb_lu_desc lu_desc;

		ret = ufshpb_get_hpb_lu_desc(hba, &lu_desc, lun);
		if (ret)
			goto out_state;

		if (lu_desc.lu_hpb_enable == false)
			continue;

		hba->ufshpb_lup[lun] = kzalloc(sizeof(struct ufshpb_lu),
								GFP_KERNEL);
		if (!hba->ufshpb_lup[lun])
			goto out_free_mem;

		ret = ufshpb_lu_hpb_init(hba, hba->ufshpb_lup[lun],
				&func_desc, &lu_desc, lun);
		if (ret) {
			if (ret == -ENODEV)
				continue;
			else
				goto out_free_mem;
		}
		hpb_dev++;
	}

	if (hpb_dev) {
		INIT_WORK(&hba->ufshpb_eh_work, ufshpb_error_handler);
		hba->ufshpb_state = HPB_PRESENT;
		pm_runtime_mark_last_busy(hba->dev);
		pm_runtime_put_noidle(hba->dev);
		return 0;
	}

out_free_mem:
	ufshpb_release_toshiba(hba, HPB_NOT_SUPPORTED);
out_state:
	hba->ufshpb_state = HPB_NOT_SUPPORTED;
	pm_runtime_mark_last_busy(hba->dev);
	pm_runtime_put_noidle(hba->dev);
	return ret;
}

static void ufshpb_map_loading_trigger(struct ufshpb_lu *hpb,
		bool dirty, bool only_pinned, bool do_work_handler)
{
	int region, subregion;

	if (do_work_handler)
		goto work_out;

	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		struct ufshpb_region *cb;

		cb = hpb->region_tbl + region;

		if (cb->region_state != HPBREGION_ACTIVE &&
				cb->region_state != HPBREGION_PINNED)
			continue;

		if ((only_pinned && cb->region_state == HPBREGION_PINNED) ||
				!only_pinned) {
			spin_lock(&hpb->hpb_lock);
			for (subregion = 0; subregion < cb->subregion_count;
								subregion++)
				ufshpb_add_subregion_to_req_list(hpb,
						cb->subregion_tbl + subregion);
			spin_unlock(&hpb->hpb_lock);
			do_work_handler = true;
		}
		if (dirty) {
			for (subregion = 0; subregion < cb->subregion_count;
								subregion++)
				cb->subregion_tbl[subregion].subregion_state =
							HPBSUBREGION_DIRTY;
		}
	}
work_out:
	if (do_work_handler)
		schedule_work(&hpb->ufshpb_work);
}

static void ufshpb_purge_active_block(struct ufshpb_lu *hpb)
{
	int region, subregion;
	int state;
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;

	spin_lock(&hpb->hpb_lock);
	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		cb = hpb->region_tbl + region;

		if (cb->region_state == HPBREGION_INACTIVE) {
			hpb_dbg(hpb->hba, "region %d inactive\n", region);
			continue;
		}

		if (cb->region_state == HPBREGION_PINNED) {
			state = HPBSUBREGION_DIRTY;
		} else if (cb->region_state == HPBREGION_ACTIVE) {
			state = HPBSUBREGION_UNUSED;
			ufshpb_cleanup_lru_info(&hpb->lru_info, cb);
		} else {
			hpb_dbg(hpb->hba, "Unsupported state of region\n");
			continue;
		}

		hpb_dbg(hpb->hba, "region %d state %d dft %d\n",
				region, state,
				hpb->debug_free_table);
		for (subregion = 0 ; subregion < cb->subregion_count;
							subregion++) {
			cp = cb->subregion_tbl + subregion;

			ufshpb_purge_active_page(hpb, cp, state);
		}
		hpb_dbg(hpb->hba, "region %d state %d dft %d\n",
				region, state, hpb->debug_free_table);
	}
	spin_unlock(&hpb->hpb_lock);
}

static void ufshpb_retrieve_rsp_info(struct ufshpb_lu *hpb)
{
	struct ufshpb_rsp_info *rsp_info;
	unsigned long flags;

	while (1) {
		spin_lock_irqsave(&hpb->rsp_list_lock, flags);
		rsp_info = list_first_entry_or_null(&hpb->lh_rsp_info,
				struct ufshpb_rsp_info, list_rsp_info);
		if (!rsp_info) {
			spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
			break;
		}
		list_move_tail(&rsp_info->list_rsp_info,
				&hpb->lh_rsp_info_free);
		spin_unlock_irqrestore(&hpb->rsp_list_lock, flags);
		hpb_dbg(hpb->hba, "add_list %p -> %p",
				&hpb->lh_rsp_info_free, rsp_info);
	}
}

static void ufshpb_probe(struct ufs_hba *hba)
{
	struct ufshpb_lu *hpb;
	int lu;

	for (lu = 0 ; lu < UFS_UPIU_MAX_GENERAL_LUN ; lu++) {
		hpb = hba->ufshpb_lup[lu];

		if (hpb && hpb->lu_hpb_enable) {
			ufshpb_reset_lu(hpb);
		}
	}
	ufshpb_inform_host_reset(hba);

	hba->ufshpb_state = HPB_PRESENT;
}

static void ufshpb_destroy_subregion_tbl(struct ufshpb_lu *hpb,
		struct ufshpb_region *cb)
{
	int subregion;

	for (subregion = 0 ; subregion < cb->subregion_count ; subregion++) {
		struct ufshpb_subregion *cp;

		cp = cb->subregion_tbl + subregion;
		cp->subregion_state = HPBSUBREGION_UNUSED;
		ufshpb_put_map_ctx(hpb, cp->mctx);
	}

	kfree(cb->subregion_tbl);
}

static void ufshpb_destroy_region_tbl(struct ufshpb_lu *hpb)
{
	int region;

	if (!hpb)
		return;

	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		struct ufshpb_region *cb;

		cb = hpb->region_tbl + region;
		if (cb->region_state == HPBREGION_PINNED ||
				cb->region_state == HPBREGION_ACTIVE) {
			cb->region_state = HPBREGION_INACTIVE;

			ufshpb_destroy_subregion_tbl(hpb, cb);
		}
	}

	ufshpb_table_mempool_remove(hpb);
	kfree(hpb->region_tbl);
}

void ufshpb_release_toshiba(struct ufs_hba *hba, int state)
{
	int lun;

	hba->ufshpb_state = HPB_FAILED;

	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		struct ufshpb_lu *hpb = hba->ufshpb_lup[lun];

		if (!hpb)
			continue;

		hba->ufshpb_lup[lun] = NULL;

		if (!hpb->lu_hpb_enable)
			continue;

		hpb->lu_hpb_enable = false;

		ufshpb_cancel_jobs(hpb);

		ufshpb_destroy_region_tbl(hpb);

		ufshpb_req_mempool_remove(hpb);

		kobject_uevent(&hpb->kobj, KOBJ_REMOVE);
		kobject_del(&hpb->kobj); // TODO --count & del?

		kfree(hpb);
	}

	if (alloc_mctx != 0)
		dev_warn(hba->dev, "warning: alloc_mctx %d", alloc_mctx);

	hba->ufshpb_state = state;
}
static void ufshpb_init_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	struct delayed_work *dwork = to_delayed_work(work);
#if defined(CONFIG_SCSI_SCAN_ASYNC)
	unsigned long flags;
#endif

	hba = container_of(dwork, struct ufs_hba, ufshpb_init_work);

	if (hba->ufshpb_state == HPB_NOT_SUPPORTED)
		return;

#if defined(CONFIG_SCSI_SCAN_ASYNC)
	mutex_lock(&hba->host->scan_mutex);
	spin_lock_irqsave(hba->host->host_lock, flags);
	if (hba->host->async_scan == 1) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		mutex_unlock(&hba->host->scan_mutex);
		schedule_delayed_work(&hba->ufshpb_init_work,
						msecs_to_jiffies(100));
		return;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	mutex_unlock(&hba->host->scan_mutex);
#endif
	if (hba->ufshpb_state == HPB_NEED_INIT) {
		int err = ufshpb_init(hba);

		if (hba->ufshpb_state == HPB_NOT_SUPPORTED)
			pr_info("UFSHPB: run without HPB - err=%d\n", err);
	} else if (hba->ufshpb_state == HPB_RESET) {
		ufshpb_probe(hba);
	}
}

#if HPB_ENABLE
void ufshcd_init_hpb(struct ufs_hba *hba)
{
	int lun;

	hba->ufshpb_state = HPB_NEED_INIT;
	for (lun = 0 ; lun < UFS_UPIU_MAX_GENERAL_LUN ; lun++) {
		hba->ufshpb_lup[lun] = NULL;
		hba->sdev_ufs_lu[lun] = NULL;
	}

	INIT_DELAYED_WORK(&hba->ufshpb_init_work, ufshpb_init_handler);
}
#else
void ufshcd_init_hpb()
{
}
#endif

static void ufshpb_error_handler(struct work_struct *work)
{
	struct ufs_hba *hba;

	hba = container_of(work, struct ufs_hba, ufshpb_eh_work);

	dev_warn(hba->dev, "UFSHPB driver runs without UFSHPB\n");
	dev_warn(hba->dev, "UFSHPB will be removed from the kernel\n");

	ufshpb_release_toshiba(hba, HPB_FAILED);
}

static void ufshpb_stat_init(struct ufshpb_lu *hpb)
{
	atomic64_set(&hpb->hit, 0);
	atomic64_set(&hpb->miss, 0);
	atomic64_set(&hpb->region_miss, 0);
	atomic64_set(&hpb->subregion_miss, 0);
	atomic64_set(&hpb->entry_dirty_miss, 0);
	atomic64_set(&hpb->map_req_cnt, 0);
	atomic64_set(&hpb->region_evict, 0);
	atomic64_set(&hpb->region_add, 0);
	atomic64_set(&hpb->rb_fail, 0);
}

static ssize_t ufshpb_sysfs_get_ppn_lba_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value)) {
		dev_err(HPB_DEV(hpb), "kstrtoul error\n");
		return -EINVAL;
	}

	if (value > hpb->lu_num_blocks) {
		dev_err(HPB_DEV(hpb),
			"value %lu > lu_num_blocks %d error\n",
			value, hpb->lu_num_blocks);
		return -EINVAL;
	}
	hpb->lba_to_show = value;
	return count;
}
static ssize_t ufshpb_sysfs_get_ppn_lba_show(struct ufshpb_lu *hpb,
		char *buf)
{
	unsigned long long ppn;
	int region, subregion, subregion_offset;
	struct ufshpb_region *cb;
	struct ufshpb_subregion *cp;
	int dirty;

	ufshpb_get_pos_from_lpn(hpb, hpb->lba_to_show, &region, &subregion,
						&subregion_offset);

	cb = hpb->region_tbl + region;
	cp = cb->subregion_tbl + subregion;

	if (cb->region_state != HPBREGION_INACTIVE) {
		ppn = ufshpb_get_ppn(cp->mctx, subregion_offset);
		spin_lock_bh(&hpb->hpb_lock);
		dirty = ufshpb_ppn_dirty_check(hpb, cp, subregion_offset);
		spin_unlock_bh(&hpb->hpb_lock);
	} else {
		ppn = 0;
		dirty = -1;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%016llx\n",dirty,ppn);
}

static ssize_t ufshpb_sysfs_map_req_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld %lld\n",
			(long long)atomic64_read(&hpb->rb_noti_cnt),
			(long long)atomic64_read(&hpb->map_req_cnt));
}

static ssize_t ufshpb_sysfs_count_reset_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long debug;

	if (kstrtoul(buf, 0, &debug))
		return -EINVAL;

	ufshpb_stat_init(hpb);

	return count;
}

static ssize_t ufshpb_sysfs_add_evict_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld %lld\n",
			(long long)atomic64_read(&hpb->region_add),
			(long long)atomic64_read(&hpb->region_evict));
}

static ssize_t ufshpb_sysfs_hit_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n",
				(long long)atomic64_read(&hpb->hit));
}

static ssize_t ufshpb_sysfs_miss_show(struct ufshpb_lu *hpb, char *buf)
{
	long long region_miss, subregion_miss, entry_dirty_miss, rb_fail;

	region_miss = atomic64_read(&hpb->region_miss);
	subregion_miss = atomic64_read(&hpb->subregion_miss);
	entry_dirty_miss = atomic64_read(&hpb->entry_dirty_miss);
	rb_fail = atomic64_read(&hpb->rb_fail);

	return snprintf(buf, PAGE_SIZE,
		"%lld %lld %lld %lld %lld\n",
			region_miss + subregion_miss + entry_dirty_miss,
			region_miss, subregion_miss, entry_dirty_miss, rb_fail);
}

static ssize_t ufshpb_sysfs_version_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"%.2x.%.2x %.2x.%.2x\n",
			GET_BYTE_1(hpb->hpb_ver), GET_BYTE_0(hpb->hpb_ver),
			GET_BYTE_1(UFSHPB_DD_VER), GET_BYTE_0(UFSHPB_DD_VER));
}

static ssize_t ufshpb_sysfs_active_list_show(struct ufshpb_lu *hpb, char *buf)
{
	int ret = 0, count = 0;
	struct ufshpb_region *region;

	list_for_each_entry(region, &hpb->lru_info.lru, list_region) {
		ret = snprintf(buf + count, PAGE_SIZE - count,
				"%d %d ",
				region->region, region->hit_count);
		count += ret;
	}
	ret = snprintf(buf + count, PAGE_SIZE - count, "\n");
	count += ret;

	return count;
}

static ssize_t ufshpb_sysfs_active_block_status_show(struct ufshpb_lu *hpb,
							char *buf)
{
	int ret = 0, count = 0, region;

	for (region = 0 ; region < hpb->regions_per_lu ; region++) {
		ret = snprintf(buf + count, PAGE_SIZE - count,
				"%d:%d ",
				region, hpb->region_tbl[region].region_state);
		count += ret;
	}

	ret = snprintf(buf + count, PAGE_SIZE - count, "\n");
	count += ret;

	return count;
}

static ssize_t ufshpb_sysfs_map_loading_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value > 2)
		return -EINVAL;

	if (value == 1)
		ufshpb_map_loading_trigger(hpb, false, false, false);
	else if (value == 2)
		ufshpb_map_loading_trigger(hpb, false, false, true);

	return count;
}

static ssize_t ufshpb_sysfs_map_disable_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hpb->force_map_req_disable);
}

static ssize_t ufshpb_sysfs_map_disable_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value)
		value = 1;

	hpb->force_map_req_disable = value;
	return count;
}

static ssize_t ufshpb_sysfs_disable_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hpb->force_disable);
}

static ssize_t ufshpb_sysfs_disable_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value)
		value = 1;

	hpb->force_disable = value;
	return count;
}

static int global_region;

static inline bool is_region_active(struct ufshpb_lu *hpb, int region)
{
	if (hpb->region_tbl[region].region_state == HPBREGION_ACTIVE ||
			hpb->region_tbl[region].region_state ==
						HPBREGION_PINNED)
		return true;

	return false;
}

static ssize_t ufshpb_sysfs_active_group_store(struct ufshpb_lu *hpb,
		const char *buf, size_t count)
{
	unsigned long block;
	int region;

	if (kstrtoul(buf, 0, &block))
		return -EINVAL;

	region = block >> hpb->entries_per_region_shift;
	if (region >= hpb->regions_per_lu) {
		dev_err(HPB_DEV(hpb),
			"error region %d max %d\n",
			region, hpb->regions_per_lu);
		region = hpb->regions_per_lu - 1;
	}

	global_region = region;

	dev_info(HPB_DEV(hpb),
		"block %lu region %d active %d",
			block, region, is_region_active(hpb, region));

	return count;
}

static ssize_t ufshpb_sysfs_active_group_show(struct ufshpb_lu *hpb, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"%d\n",	is_region_active(hpb, global_region));
}

static struct ufshpb_sysfs_entry ufshpb_sysfs_entries[] = {
	__ATTR(is_active_group, 0644,
			ufshpb_sysfs_active_group_show,
			ufshpb_sysfs_active_group_store),
	__ATTR(read_16_disable, 0644,
			ufshpb_sysfs_disable_show, ufshpb_sysfs_disable_store),
	__ATTR(map_cmd_disable, 0644,
			ufshpb_sysfs_map_disable_show,
			ufshpb_sysfs_map_disable_store),
	__ATTR(map_loading, 0200, NULL, ufshpb_sysfs_map_loading_store),
	__ATTR(active_block_status, 0444,
			ufshpb_sysfs_active_block_status_show, NULL),
	__ATTR(HPBVersion, 0444, ufshpb_sysfs_version_show, NULL),
	__ATTR(hit_count, 0444, ufshpb_sysfs_hit_show, NULL),
	__ATTR(miss_count, 0444, ufshpb_sysfs_miss_show, NULL),
	__ATTR(active_list, 0444, ufshpb_sysfs_active_list_show, NULL),
	__ATTR(add_evict_count, 0444, ufshpb_sysfs_add_evict_show, NULL),
	__ATTR(count_reset, 0200, NULL, ufshpb_sysfs_count_reset_store),
	__ATTR(map_req_count, 0444, ufshpb_sysfs_map_req_show, NULL),
	__ATTR(get_ppn_from_lba, 0644,
			ufshpb_sysfs_get_ppn_lba_show,
			ufshpb_sysfs_get_ppn_lba_store),
	__ATTR_NULL
};

static ssize_t ufshpb_attr_show(struct kobject *kobj,
		struct attribute *attr, char *page)
{
	struct ufshpb_sysfs_entry *entry;
	struct ufshpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr,
			struct ufshpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct ufshpb_lu, kobj);

	if (!entry->show)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->show(hpb, page);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static ssize_t ufshpb_attr_store(struct kobject *kobj,
		struct attribute *attr,
		const char *page, size_t length)
{
	struct ufshpb_sysfs_entry *entry;
	struct ufshpb_lu *hpb;
	ssize_t error;

	entry = container_of(attr, struct ufshpb_sysfs_entry, attr);
	hpb = container_of(kobj, struct ufshpb_lu, kobj);

	if (!entry->store)
		return -EIO;

	mutex_lock(&hpb->sysfs_lock);
	error = entry->store(hpb, page, length);
	mutex_unlock(&hpb->sysfs_lock);
	return error;
}

static const struct sysfs_ops ufshpb_sysfs_ops = {
	.show = ufshpb_attr_show,
	.store = ufshpb_attr_store,
};

static struct kobj_type ufshpb_ktype = {
	.sysfs_ops = &ufshpb_sysfs_ops,
	.release = NULL,
};

static int ufshpb_create_sysfs(struct ufs_hba *hba,
		struct ufshpb_lu *hpb)
{
	struct device *dev = hba->dev;
	struct ufshpb_sysfs_entry *entry;
	int err;

	hpb->sysfs_entries = ufshpb_sysfs_entries;

	ufshpb_stat_init(hpb);

	kobject_init(&hpb->kobj, &ufshpb_ktype);
	mutex_init(&hpb->sysfs_lock);

	err = kobject_add(&hpb->kobj, kobject_get(&dev->kobj),
			"ufshpb_lu%d", hpb->lun);
	if (!err) {
		for (entry = hpb->sysfs_entries;
				entry->attr.name != NULL ; entry++) {
			if (sysfs_create_file(&hpb->kobj, &entry->attr))
				break;
		}
		kobject_uevent(&hpb->kobj, KOBJ_ADD);
	}

	return err;
}
