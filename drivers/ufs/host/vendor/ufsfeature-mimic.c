// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung UFS Feature Mimic for android15-6.6
 * commit 3009510505edcc8a0622b6c99925fb841b5e132e
 * Date:   Fri Aug 16 14:33:14 2024 +0000
 *
 * from
 *
 * linux/drivers/ufs/core/ufshcd.c
 */

#include <ufs/ufshcd.h>

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

MODULE_LICENSE("GPL v2");
