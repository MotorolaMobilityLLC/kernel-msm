/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

/**
 * DOC : wlan_hdd_memdump.c
 *
 * WLAN Host Device Driver file for dumping firmware memory
 *
 */

#include <sme_Api.h>
#include <wlan_hdd_includes.h>
#include "wlan_hdd_memdump.h"
#include "vos_sched.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <linux/uaccess.h> /* for copy_to_user */

/**
 * hdd_fw_dump_context - hdd firmware memory dump context
 *
 * @request_id: userspace assigned firmware memory dump request ID
 * @response_event: firmware memory dump request wait event
 */
struct hdd_fw_dump_context {
	uint32_t request_id;
	struct completion response_event;
};
static struct hdd_fw_dump_context fw_dump_context;

/**
 * memdump_cleanup_timer_cb() - Timer callback function for memory dump cleanup.
 *
 * @data: Callback data (used to stored HDD context)
 *
 * Callback function registered for memory dump cleanup VOS timer.
 *
 * Return: none
 */

static void memdump_cleanup_timer_cb(void *data)
{
	int status;
	hdd_context_t *hdd_ctx = data;
	adf_os_dma_addr_t paddr;
	adf_os_dma_addr_t dma_ctx;
	adf_os_device_t adf_ctx;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return;
	}

	if (!hdd_ctx->fw_dump_loc) {
		hddLog(LOG1, FL("Memory dump already freed"));
		return;
	}

	adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, hdd_ctx->pvosContext);
	if (!adf_ctx) {
		hddLog(LOGE, FL("ADF context is NULL"));
		return;
	}

	paddr = hdd_ctx->dump_loc_paddr;
	mutex_lock(&hdd_ctx->memdump_lock);
	adf_os_mem_free_consistent(adf_ctx,
		FW_MEM_DUMP_SIZE, hdd_ctx->fw_dump_loc, paddr, dma_ctx);
	hdd_ctx->fw_dump_loc = NULL;
	hdd_ctx->memdump_in_progress = false;
	mutex_unlock(&hdd_ctx->memdump_lock);

}

/**
 * wlan_hdd_cfg80211_fw_mem_dump_cb() -  Callback to receive FW memory dump
 * @ctx: pointer to HDD context.
 * @rsp: pointer to fw dump copy complete response
 *
 * This is a callback function used to indicate user space about the
 * availability for firmware memory dump via vendor event.
 *
 * Return: None
 */
static void wlan_hdd_cfg80211_fw_mem_dump_cb(void *ctx,
					     struct fw_dump_rsp *dump_rsp)
{
	hdd_context_t *hdd_ctx = ctx;
	struct hdd_fw_dump_context *context;
	int status;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return;
	}

	spin_lock(&hdd_context_lock);
	context = &fw_dump_context;
	/* validate the response received */
	if (!dump_rsp->dump_complete ||
	    context->request_id != dump_rsp->request_id) {
		spin_unlock(&hdd_context_lock);
		hddLog(LOGE,
		       FL("Error @ request_id: %d response_id: %d status: %d"),
		       context->request_id, dump_rsp->request_id,
		       dump_rsp->dump_complete);
		return;
	} else {
		complete(&context->response_event);
	}
	spin_unlock(&hdd_context_lock);

	return;
}

/**
 * wlan_hdd_send_memdump_rsp - send memory dump response to user space
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: 0 for success; non-zero for failure
 */
static int wlan_hdd_send_memdump_rsp(hdd_context_t *hdd_ctx)
{
	struct sk_buff *skb;
	int status;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return status;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
			NLMSG_HDRLEN + NLA_HDRLEN + sizeof(uint32_t));

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		return -ENOMEM;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_MEMDUMP_SIZE,
			     FW_MEM_DUMP_SIZE)) {
		hddLog(LOGE, FL("nla put fail"));
		goto nla_put_failure;
	}

	cfg80211_vendor_cmd_reply(skb);
	hddLog(LOG1, FL("Memdump event sent successfully to user space"));
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * __wlan_hdd_cfg80211_get_fw_mem_dump() - Get FW memory dump
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the NL data.
 * @data_len:Length of @data
 *
 * This is called when wlan driver needs to get the firmware memory dump
 * via vendor specific command.
 *
 * Return:   0 on success, error number otherwise.
 */
static int
__wlan_hdd_cfg80211_get_fw_mem_dump(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	int status;
	VOS_STATUS sme_status;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct fw_dump_req fw_mem_dump_req;
	struct fw_dump_seg_req* seg_req;
	uint8_t loop;
	adf_os_dma_addr_t paddr;
	adf_os_dma_addr_t dma_ctx;
	adf_os_device_t adf_ctx;
	unsigned long rc;
	struct hdd_fw_dump_context *context;

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Command not allowed in FTM mode"));
		return -EINVAL;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is invalid"));
		return status;
	}

	adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, hdd_ctx->pvosContext);
	if (!adf_ctx) {
		hddLog(LOGE, FL("ADF context is NULL"));
		return -EINVAL;
	}

	if (hdd_ctx->memdump_in_progress) {
		hddLog(LOGE, FL("Already a memdump req in progress."));
		return -EBUSY;
	}

	/*
	 * Allocate memory for fw memory dump. Memory allocated should be
	 * contiguous. Physical address of the allocated memory is passed
	 * to the FW for copy
	 *
	 * Reuse the memory if available.
	 */
	mutex_lock(&hdd_ctx->memdump_lock);
	if (!hdd_ctx->fw_dump_loc) {
		hdd_ctx->fw_dump_loc = adf_os_mem_alloc_consistent(
			adf_ctx, FW_MEM_DUMP_SIZE, &paddr, dma_ctx);
		if (!hdd_ctx->fw_dump_loc) {
			mutex_unlock(&hdd_ctx->memdump_lock);
			hddLog(LOGE, FL("adf_os_mem_alloc_consistent failed"));
			return -ENOMEM;
		}
		hdd_ctx->dump_loc_paddr = paddr;
	}
	mutex_unlock(&hdd_ctx->memdump_lock);

	/*
	 * Currently request_id and num_seg is assumed to be default(1)
	 * It is assumed that firmware dump requested is for DRAM section
	 * only
	 */

	fw_mem_dump_req.request_id = FW_MEM_DUMP_REQ_ID;
	fw_mem_dump_req.num_seg = FW_MEM_DUMP_NUM_SEG;

	hddLog(LOG1, FL("request_id:%d num_seg:%d"),
		fw_mem_dump_req.request_id, fw_mem_dump_req.num_seg);
	seg_req = (struct fw_dump_seg_req *) fw_mem_dump_req.segment;
	for (loop = 0; loop < fw_mem_dump_req.num_seg; loop++) {
		seg_req->seg_id = 1;
		seg_req->seg_start_addr_lo = FW_DRAM_LOCATION;
		seg_req->seg_start_addr_hi = 0;
		seg_req->seg_length = FW_MEM_DUMP_SIZE;
		seg_req->dst_addr_lo = hdd_ctx->dump_loc_paddr;
		seg_req->dst_addr_hi = 0;
		hddLog(LOG1, FL("seg_number:%d"), loop);
		hddLog(LOG1,
		    FL("seg_id:%d start_addr_lo:0x%x start_addr_hi:0x%x"),
		    seg_req->seg_id, seg_req->seg_start_addr_lo,
		    seg_req->seg_start_addr_hi);
		hddLog(LOG1,
		    FL("seg_length:%d dst_addr_lo:0x%x dst_addr_hi:0x%x"),
		    seg_req->seg_length, seg_req->dst_addr_lo,
		    seg_req->dst_addr_hi);
		seg_req++;
	}

	/**
	 * Start the cleanup timer.
	 * Memory allocated for this request will be freed up
	 * once the timer expires. Memory dump request is expected to be
	 * completed by this time.
	 *
	 * User space will not be able to access the dump after this time.
	 * New request should be issued to get the dump again.
	 */
	vos_timer_start(&hdd_ctx->memdump_cleanup_timer,
			MEMDUMP_COMPLETION_TIME_MS);
	hdd_ctx->memdump_in_progress = true;

	spin_lock(&hdd_context_lock);
	context = &fw_dump_context;
	context->request_id = fw_mem_dump_req.request_id;
	INIT_COMPLETION(context->response_event);
	spin_unlock(&hdd_context_lock);

	sme_status = sme_fw_mem_dump(hdd_ctx->hHal, &fw_mem_dump_req);
	if (VOS_STATUS_SUCCESS != sme_status) {
		hddLog(LOGE, FL("sme_fw_mem_dump Failed"));
		mutex_lock(&hdd_ctx->memdump_lock);
		adf_os_mem_free_consistent(adf_ctx,
			FW_MEM_DUMP_SIZE, hdd_ctx->fw_dump_loc, paddr, dma_ctx);
		hdd_ctx->fw_dump_loc = NULL;
		mutex_unlock(&hdd_ctx->memdump_lock);
		hdd_ctx->memdump_in_progress = false;
		if (VOS_TIMER_STATE_RUNNING ==
		  vos_timer_getCurrentState(&hdd_ctx->memdump_cleanup_timer)) {
			vos_timer_stop(&hdd_ctx->memdump_cleanup_timer);
		}
		return -EINVAL;
	}

	rc = wait_for_completion_timeout(&context->response_event,
		msecs_to_jiffies(MEMDUMP_COMPLETION_TIME_MS));
	if (!rc) {
		hddLog(LOGE, FL("Target response timed out for request_id: %d"),
		       context->request_id);
		return -ETIMEDOUT;
	}

	status = wlan_hdd_send_memdump_rsp(hdd_ctx);
	if (status)
		hddLog(LOGE,
			FL("Failed to send FW memory dump rsp to user space"));

	return status;
}

/**
 * wlan_hdd_cfg80211_get_fw_mem_dump() - Get FW memory dump
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the NL data.
 * @data_len:Length of @data
 *
 * This is called when wlan driver needs to get the firmware memory dump
 * via vendor specific command.
 *
 * Return:   0 on success, error number otherwise.
 */
int wlan_hdd_cfg80211_get_fw_mem_dump(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_fw_mem_dump(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

#define PROCFS_MEMDUMP_DIR "debug"
#define PROCFS_MEMDUMP_NAME "fwdump"
#define PROCFS_MEMDUMP_PERM 0444

static struct proc_dir_entry *proc_file, *proc_dir;

/** memdump_get_file_data() - get data available in proc file
 *
 * @file - handle for the proc file.
 *
 * This function is used to retrieve the data passed while
 * creating proc file entry.
 *
 * Return: void pointer to hdd_context
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)) || defined(WITH_BACKPORTS)
static void *memdump_get_file_data(struct file *file)
{
	void *hdd_ctx;

	hdd_ctx = PDE_DATA(file_inode(file));
	return hdd_ctx;
}
#else
static void *memdump_get_file_data(struct file *file)
{
	void *hdd_ctx;

	hdd_ctx = PDE(file->f_path.dentry->d_inode)->data;
	return hdd_ctx;
}
#endif

/**
 * memdump_read() - perform read operation in memory dump proc file
 *
 * @file  - handle for the proc file.
 * @buf   - pointer to user space buffer.
 * @count - number of bytes to be read.
 * @pos   - offset in the from buffer.
 *
 * This function performs read operation for the memory dump proc file.
 *
 * Return: number of bytes read on success, error code otherwise.
 */
static ssize_t memdump_read(struct file *file, char __user *buf,
					size_t count, loff_t *pos)
{
	int status;
	hdd_context_t *hdd_ctx;
	adf_os_dma_addr_t paddr;
	adf_os_dma_addr_t dma_ctx;
	adf_os_device_t adf_ctx;

	hdd_ctx = memdump_get_file_data(file);

	hddLog(LOG1, FL("Read req for size:%zu pos:%llu"), count, *pos);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}
	adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, hdd_ctx->pvosContext);
	if (!adf_ctx) {
		hddLog(LOGE, FL("ADF context is NULL"));
		return -EINVAL;
	}

	if (!hdd_ctx->memdump_in_progress) {
		hddLog(LOGE, FL("Current mem dump request timed out/failed"));
		return -EINVAL;
	}

	if (*pos < 0) {
		hddLog(LOGE, FL("Invalid start offset for memdump read"));
		return -EINVAL;
	} else if (*pos >= FW_MEM_DUMP_SIZE || !count) {
		hddLog(LOGE, FL("No more data to copy"));
		return 0;
	} else if (count > FW_MEM_DUMP_SIZE - *pos) {
		count = FW_MEM_DUMP_SIZE - *pos;
	}

	if (!hdd_ctx->fw_dump_loc) {
		hddLog(LOGE, FL("Invalid fw mem dump location"));
		return -EINVAL;
	}

	if (copy_to_user(buf, hdd_ctx->fw_dump_loc + *pos, count)) {
		hddLog(LOGE, FL("copy to user space failed"));
		return -EFAULT;
	}

	/* offset(pos) should be updated here based on the copy done*/
	*pos += count;

	/* Entire FW memory dump copy completed */
	if (*pos >= FW_MEM_DUMP_SIZE) {
		paddr = hdd_ctx->dump_loc_paddr;
		mutex_lock(&hdd_ctx->memdump_lock);
		adf_os_mem_free_consistent(adf_ctx,
			FW_MEM_DUMP_SIZE, hdd_ctx->fw_dump_loc, paddr, dma_ctx);
		hdd_ctx->fw_dump_loc = NULL;
		hdd_ctx->memdump_in_progress = false;
		if (VOS_TIMER_STATE_RUNNING ==
		  vos_timer_getCurrentState(&hdd_ctx->memdump_cleanup_timer)) {
			vos_timer_stop(&hdd_ctx->memdump_cleanup_timer);
		}
		mutex_unlock(&hdd_ctx->memdump_lock);
	}

	return count;
}

/**
 * struct memdump_fops - file operations for memory dump feature
 * @read - read function for memory dump operation.
 *
 * This structure initialize the file operation handle for memory
 * dump feature
 */
static const struct file_operations memdump_fops = {
	read: memdump_read
};

/**
 * memdump_procfs_init() - Initialize procfs for memory dump
 *
 * @vos_ctx - Global vos context.
 *
 * This function create file under proc file system to be used later for
 * processing firmware memory dump
 *
 * Return:   0 on success, error code otherwise.
 */
static int memdump_procfs_init(void *vos_ctx)
{
	hdd_context_t *hdd_ctx;

	hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx);
	if (!hdd_ctx) {
		hddLog(LOGE , FL("Invalid HDD context"));
		return -EINVAL;
	}

	proc_dir = proc_mkdir(PROCFS_MEMDUMP_DIR, NULL);
	if (proc_dir == NULL) {
		remove_proc_entry(PROCFS_MEMDUMP_DIR, NULL);
		pr_debug("Error: Could not initialize /proc/%s\n",
			 PROCFS_MEMDUMP_DIR);
		return -ENOMEM;
	}

	proc_file = proc_create_data(PROCFS_MEMDUMP_NAME,
				     PROCFS_MEMDUMP_PERM, proc_dir,
				     &memdump_fops, hdd_ctx);
	if (proc_file == NULL) {
		remove_proc_entry(PROCFS_MEMDUMP_NAME, proc_dir);
		pr_debug("Error: Could not initialize /proc/%s\n",
			  PROCFS_MEMDUMP_NAME);
		return -ENOMEM;
	}

	pr_debug("/proc/%s/%s created\n", PROCFS_MEMDUMP_DIR,
		 PROCFS_MEMDUMP_NAME);
	return 0;
}

/**
 * memdump_procfs_remove() - Remove file/dir under procfs for memory dump
 *
 * This function removes file/dir under proc file system that was
 * processing firmware memory dump
 *
 * Return:  None
 */
static void memdump_procfs_remove(void)
{
	remove_proc_entry(PROCFS_MEMDUMP_NAME, proc_dir);
	pr_debug("/proc/%s/%s removed\n", PROCFS_MEMDUMP_DIR,
					  PROCFS_MEMDUMP_NAME);
	remove_proc_entry(PROCFS_MEMDUMP_DIR, NULL);
	pr_debug("/proc/%s removed\n", PROCFS_MEMDUMP_DIR);
}

/**
 * memdump_init() - Intialization function for memory dump feature
 *
 * This function creates proc file for memdump feature and registers
 * HDD callback function with SME.
 *
 * Return - 0 on success, error otherwise
 */
int memdump_init(void)
{
	hdd_context_t *hdd_ctx;
	void *vos_ctx;
	int status = 0;
	eHalStatus cb_status;
	VOS_STATUS vos_status;

	vos_ctx = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_ctx) {
		hddLog(LOGE, FL("Invalid VOS context"));
		return -EINVAL;
	}

	hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx);
	if (!hdd_ctx) {
		hddLog(LOGE , FL("Invalid HDD context"));
		return -EINVAL;
	}

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Not initializing memdump in FTM mode"));
		return -EINVAL;
	}

	cb_status = sme_fw_mem_dump_register_cb(hdd_ctx->hHal,
				wlan_hdd_cfg80211_fw_mem_dump_cb);
	if (eHAL_STATUS_SUCCESS != cb_status) {
		hddLog(LOGE , FL("Failed to register the callback"));
		return -EINVAL;
	}

	status = memdump_procfs_init(vos_ctx);
	if (status) {
		hddLog(LOGE , FL("Failed to create proc file"));
		return status;
	}

	init_completion(&fw_dump_context.response_event);

	vos_status = vos_timer_init(&hdd_ctx->memdump_cleanup_timer,
				    VOS_TIMER_TYPE_SW, memdump_cleanup_timer_cb,
				    (void *)hdd_ctx);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		hddLog(LOGE, FL("Failed to init memdump cleanup timer"));
		return -EINVAL;
	}

	mutex_init(&hdd_ctx->memdump_lock);

	return 0;
}

/**
 * memdump_deinit() - De initialize memdump feature
 *
 * This function removes proc file created for memdump feature.
 *
 * Return: None
 */
void memdump_deinit(void) {
	hdd_context_t *hdd_ctx;
	void *vos_ctx;
	adf_os_dma_addr_t paddr;
	adf_os_dma_addr_t dma_ctx;
	adf_os_device_t adf_ctx;
	VOS_STATUS vos_status;

	vos_ctx = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if (!vos_ctx) {
		hddLog(LOGE, FL("Invalid VOS context"));
		return;
	}

	hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx);
	if(!hdd_ctx) {
		hddLog(LOGE , FL("Invalid HDD context"));
		return;
	}

	if (VOS_FTM_MODE == hdd_get_conparam()) {
		hddLog(LOGE, FL("Not deinitializing memdump in FTM mode"));
		return;
	}

	adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, hdd_ctx->pvosContext);
	if (!adf_ctx) {
		hddLog(LOGE, FL("ADF context is NULL"));
		return;
	}

	memdump_procfs_remove();
	sme_fw_mem_dump_unregister_cb(hdd_ctx->hHal);

	mutex_lock(&hdd_ctx->memdump_lock);
	if (hdd_ctx->fw_dump_loc) {
		paddr = hdd_ctx->dump_loc_paddr;
		adf_os_mem_free_consistent(adf_ctx,
			FW_MEM_DUMP_SIZE, hdd_ctx->fw_dump_loc, paddr, dma_ctx);
		hdd_ctx->fw_dump_loc = NULL;
		hdd_ctx->memdump_in_progress = false;
	}
	mutex_unlock(&hdd_ctx->memdump_lock);

	if (VOS_TIMER_STATE_RUNNING ==
	  vos_timer_getCurrentState(&hdd_ctx->memdump_cleanup_timer)) {
		vos_timer_stop(&hdd_ctx->memdump_cleanup_timer);
	}

	vos_status = vos_timer_destroy(&hdd_ctx->memdump_cleanup_timer);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		hddLog(LOGE, FL("Failed to deallocate timer"));
	}
}
