/*
 * Universal Flash Storage Feature Support
 *
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
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

#include "ufsfeature.h"
#include "ufshcd.h"
#include "ufs-qcom.h"

inline int is_vendor_device(struct ufs_hba *hba, int id)
{
	return (hba != NULL && hba->dev_info.wmanufacturerid == id);
}
static int ufsf_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			  u8 *desc_buf, u32 size)
{
	int err = 0;

	pm_runtime_get_sync(hba->dev);

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index, 0,
					    desc_buf, &size);
	if (err)
		ERR_MSG("reading Device Desc failed. err = %d", err);

	pm_runtime_put_sync(hba->dev);

	return err;
}

static int ufsf_read_dev_desc(struct ufsf_feature *ufsf)
{
	u8 desc_buf[UFSF_QUERY_DESC_DEVICE_MAX_SIZE];
	int ret;
	u8 idn = 0;

	if (is_vendor_device(ufsf->hba, UFS_VENDOR_SAMSUNG))
		idn = UFSF_QUERY_DESC_IDN_DEVICE;

	ret = ufsf_read_desc(ufsf->hba, idn, 0,
			     desc_buf, UFSF_QUERY_DESC_DEVICE_MAX_SIZE);
	if (ret)
		return ret;

	ufsf->num_lu = desc_buf[DEVICE_DESC_PARAM_NUM_LU];
	INFO_MSG("device lu count %d", ufsf->num_lu);

	INFO_MSG("length=%u(0x%x) bSupport=0x%.2x, extend=0x%.2x_%.2x",
		  desc_buf[DEVICE_DESC_PARAM_LEN],
		  desc_buf[DEVICE_DESC_PARAM_LEN],
		  desc_buf[DEVICE_DESC_PARAM_UFS_FEAT],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+2],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+3]);

	INFO_MSG("samsung extend=0x%.2x_%.2x",
		  desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP+2],
		  desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP+3]);

	INFO_MSG("Driver Feature Version : (%.6X%s)", UFSFEATURE_DD_VER,
		 UFSFEATURE_DD_VER_POST);

#if defined(CONFIG_UFSHID)
	ufshid_get_dev_info(ufsf, desc_buf);
#endif
#if defined(CONFIG_UFSSID)
	ufssid_get_dev_info(ufsf, desc_buf);
#endif

#if defined(CONFIG_UFSFBO)
	ufsfbo_get_dev_info(ufsf);
#endif
	return 0;
}

static int ufsf_read_geo_desc(struct ufsf_feature *ufsf)
{
	u8 geo_buf[UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE];
	int ret;

	u8 idn = QUERY_DESC_IDN_GEOMETRY;

	if (is_vendor_device(ufsf->hba, UFS_VENDOR_SAMSUNG))
		idn = UFSF_QUERY_DESC_IDN_GEOMETRY;
	ret = ufsf_read_desc(ufsf->hba, idn, 0,
			     geo_buf, UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE);
	if (ret)
		return ret;

#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_NEED_INIT)
		ufshid_get_geo_info(ufsf, geo_buf);
#endif

	return 0;
}

void ufsf_device_check(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	if (ufsf_read_dev_desc(ufsf))
		return;

#if defined(CONFIG_UFSFBO)
	ufsfbo_read_fbo_desc(ufsf);
#endif
	ufsf_read_geo_desc(ufsf);
}

static int ufsf_execute_dev_ctx_req(struct ufsf_feature *ufsf,
				    int lun, unsigned char *cdb,
				    void *buf, int len)
{
	struct scsi_sense_hdr sshdr;
	struct scsi_device *sdev;
	int ret;

	sdev = ufsf->sdev_ufs_lu[lun];
	if (!sdev) {
		WARN_MSG("cannot find scsi_device");
		return -ENODEV;
	}

	ufsf->issue_ioctl = true;
	ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, len, NULL, &sshdr,
			   msecs_to_jiffies(30000), 3, 0, 0, NULL);
	ufsf->issue_ioctl = false;

	return ret;
}

static inline void ufsf_set_read_dev_ctx(unsigned char *cdb, int lba, int len)
{
	cdb[0] = READ_10;
	cdb[1] = 0x02;
	put_unaligned_be32(lba, cdb + 2);
	put_unaligned_be24(len, cdb + 6);
}

static int ufsf_issue_req_dev_ctx(struct ufsf_feature *ufsf, int lun,
				  unsigned char *buf, int buf_len)
{
	unsigned char cdb[10] = { 0 };
	int cmd_len = buf_len >> OS_PAGE_SHIFT;
	int ret;

	ufsf_set_read_dev_ctx(cdb, READ10_DEBUG_LBA, cmd_len);

	ret = ufsf_execute_dev_ctx_req(ufsf, lun, cdb, buf, buf_len);

	if (ret < 0)
		ERR_MSG("failed with err %d", ret);

	return ret;
}

static void ufsf_print_query_buf(unsigned char *field, int size)
{
	unsigned char buf[255];
	int count;
	int i;

	count = snprintf(buf, 8, "(0x00):");

	for (i = 0; i < size; i++) {
		count += snprintf(buf + count, 4, " %.2X", field[i]);

		if ((i + 1) % 16 == 0) {
			buf[count] = '\n';
			buf[count + 1] = '\0';
			printk(buf);
			count = 0;
			count += snprintf(buf, 8, "(0x%.2X):", i + 1);
		} else if ((i + 1) % 4 == 0)
			count += snprintf(buf + count, 3, " :");
	}
	buf[count] = '\n';
	buf[count + 1] = '\0';
	printk(buf);
}

static inline void ufsf_set_read10_debug_cmd(unsigned char *cdb, int lba,
					     int len)
{
	cdb[0] = READ_10;
	cdb[1] = 0x02;
	put_unaligned_be32(lba, cdb + 2);
	put_unaligned_be24(len, cdb + 6);
}

int ufsf_query_ioctl(struct ufsf_feature *ufsf, int lun, void __user *buffer,
		     struct ufs_ioctl_query_data *ioctl_data)
{
	unsigned char *kernel_buf;
	int opcode;
	int err = 0;
	int index = 0;
	int length = 0;
	int buf_len = 0;

	opcode = ioctl_data->opcode & 0xffff;

	INFO_MSG("op %u idn %u size %u(0x%X)", opcode, ioctl_data->idn,
		 ioctl_data->buf_size, ioctl_data->buf_size);

	switch (ioctl_data->idn) {
	case QUERY_DESC_IDN_STRING:
		buf_len = IOCTL_DEV_CTX_MAX_SIZE;
		break;
#if defined(CONFIG_UFSHID)
	case QUERY_ATTR_IDN_HID_OPERATION:
	case QUERY_ATTR_IDN_HID_FRAG_LEVEL:
		buf_len = ioctl_data->buf_size;
		break;
#endif

#if defined(CONFIG_UFSFBO)
	case QUERY_ATTR_IDN_FBO_CONTROL:
		buf_len = ioctl_data->buf_size;
		break;
#endif
	default:
		buf_len = QUERY_DESC_MAX_SIZE;
		break;
	}

	kernel_buf = kzalloc(buf_len, GFP_KERNEL);
	if (!kernel_buf) {
		err = -ENOMEM;
		goto out;
	}

	switch (opcode) {
	case UPIU_QUERY_OPCODE_WRITE_DESC:
		err = copy_from_user(kernel_buf, buffer +
				     sizeof(struct ufs_ioctl_query_data),
				     ioctl_data->buf_size);
		INFO_MSG("buf size %d", ioctl_data->buf_size);
		ufsf_print_query_buf(kernel_buf, ioctl_data->buf_size);
		if (err)
			goto out_release_mem;
		break;

	case UPIU_QUERY_OPCODE_READ_DESC:
		switch (ioctl_data->idn) {
		case QUERY_DESC_IDN_UNIT:
			if (!ufs_is_valid_unit_desc_lun(&ufsf->hba->dev_info, lun, 0)) {
				ERR_MSG("No unit descriptor for lun 0x%x", lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			index = lun;
			INFO_MSG("read lu desc lun: %d", index);
			break;

		case QUERY_DESC_IDN_STRING:
			if (!ufs_is_valid_unit_desc_lun(&ufsf->hba->dev_info, lun, 0)) {
				ERR_MSG("No unit descriptor for lun 0x%x", lun);
				err = -EINVAL;
				goto out_release_mem;
			}
			err = ufsf_issue_req_dev_ctx(ufsf, lun, kernel_buf,
					ioctl_data->buf_size);
			if (err < 0)
				goto out_release_mem;

			goto copy_buffer;
		case QUERY_DESC_IDN_DEVICE:
		case QUERY_DESC_IDN_GEOMETRY:
		case QUERY_DESC_IDN_CONFIGURATION:
		case UFSF_QUERY_DESC_IDN_DEVICE:
		case UFSF_QUERY_DESC_IDN_GEOMETRY:

			break;

		default:
			ERR_MSG("invalid idn %d", ioctl_data->idn);
			err = -EINVAL;
			goto out_release_mem;
		}
		break;

	case UPIU_QUERY_OPCODE_WRITE_ATTR:
		switch (ioctl_data->idn) {
#if defined(CONFIG_UFSHID)
		case QUERY_ATTR_IDN_HID_OPERATION:
			err = copy_from_user(kernel_buf, buffer +
				     sizeof(struct ufs_ioctl_query_data),
				     ioctl_data->buf_size);
			if (err)
				goto out_release_mem;

			err = ufshid_send_file_info(ufsf->hid_dev, lun,
						    kernel_buf, buf_len,
						    ioctl_data->idn);
			if (err)
				ERR_MSG("HID LBA Trigger fail. (%d)", err);

			goto out_release_mem;
#endif

#if defined(CONFIG_UFSFBO)
		case QUERY_ATTR_IDN_FBO_CONTROL:
			err = copy_from_user(kernel_buf, buffer +
					sizeof(struct ufs_ioctl_query_data),
					ioctl_data->buf_size);
			if (err)
				goto out_release_mem;

			err = ufsfbo_send_file_info(ufsf->fbo_dev, lun,
						    kernel_buf, buf_len,
						    ioctl_data->idn, opcode);
			if (err)
				ERR_MSG("FBO LBA Trigger failed. (%d)", err);

			goto out_release_mem;
#endif
		default:
			break;
		}
		break;

	case UPIU_QUERY_OPCODE_READ_ATTR:
		switch (ioctl_data->idn) {
#if defined(CONFIG_UFSHID)
		case QUERY_ATTR_IDN_HID_FRAG_LEVEL:
			err = copy_from_user(kernel_buf, buffer +
				     sizeof(struct ufs_ioctl_query_data),
				     ioctl_data->buf_size);
			if (err)
				goto out_release_mem;

			err = ufshid_send_file_info(ufsf->hid_dev, lun,
						    kernel_buf, buf_len,
						    ioctl_data->idn);
			if (err) {
				ERR_MSG("HID LBA Trigger fail. (%d)", err);
				goto out_release_mem;
			}

			goto copy_buffer;
#endif
#if defined(CONFIG_UFSFBO)
		case QUERY_ATTR_IDN_FBO_CONTROL:
			err = copy_from_user(kernel_buf, buffer +
					sizeof(struct ufs_ioctl_query_data),
					ioctl_data->buf_size);
			if (err)
				goto out_release_mem;

			err = ufsfbo_send_file_info(ufsf->fbo_dev, lun,
						    kernel_buf, buf_len,
						    ioctl_data->idn, opcode);
			if (err)
				ERR_MSG("FBO LBA Trigger failed. (%d)", err);

			goto out_release_mem;
#endif
		default:
			break;
		}
		break;

	default:
		ERR_MSG("invalid opcode %d", opcode);
		err = -EINVAL;
		goto out_release_mem;
	}

	length = ioctl_data->buf_size;

	err = ufshcd_query_descriptor_retry(ufsf->hba, opcode, ioctl_data->idn,
					    index, 0, kernel_buf,
					    &length);
	if (err)
		goto out_release_mem;

copy_buffer:
	if (opcode == UPIU_QUERY_OPCODE_READ_DESC ||
	    opcode == UPIU_QUERY_OPCODE_READ_ATTR) {
		err = copy_to_user(buffer, ioctl_data,
				   sizeof(struct ufs_ioctl_query_data));
		if (err)
			ERR_MSG("Failed copying back to user.");

		err = copy_to_user(buffer + sizeof(struct ufs_ioctl_query_data),
				   kernel_buf, ioctl_data->buf_size);
		if (err)
			ERR_MSG("Fail: copy rsp_buffer to user space.");
	}
out_release_mem:
	kfree(kernel_buf);
out:
	return err;
}

/*
 * Mimic ufshcd_copy_sense_data()
 */
#define UFS_SENSE_SIZE	18
static void ufsf_copy_sense_data(struct ufshcd_lrb *lrbp)
{
	unsigned int seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
		MASK_RSP_UPIU_DATA_SEG_LEN;
	int len;

	if (lrbp->sense_buffer && seg_len) {
		int len_to_copy;

		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);
		len_to_copy = min_t(int, UFS_SENSE_SIZE, len);

		memcpy(lrbp->sense_buffer, lrbp->ucd_rsp_ptr->sr.sense_data,
		       len_to_copy);
	}
}

void ufsf_upiu_check_for_ccd(struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;
	int data_seg_len, sense_data_len;
	struct utp_cmd_rsp *sr = &lrbp->ucd_rsp_ptr->sr;

	if (cdb[0] != VENDOR_OP || cdb[1] != VENDOR_CCD)
		return;

	data_seg_len = be32_to_cpu(lrbp->ucd_rsp_ptr->header.dword_2) &
				       MASK_RSP_UPIU_DATA_SEG_LEN;
	sense_data_len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);

	if (data_seg_len != CCD_DATA_SEG_LEN ||
	    sense_data_len != CCD_SENSE_DATA_LEN) {
		WARN_MSG("CCD info is wrong. so check it.");
		WARN_MSG("CCD data_seg_len = %d, sense_data_len %d",
			 data_seg_len, sense_data_len);
	} else {
		INFO_MSG("CCD info is correct!!");
	}

	INFO_MSG("sense : %02X %02X %02X %02X %02X %02X\n",
		 sr->sense_data[0], sr->sense_data[1], sr->sense_data[2],
		 sr->sense_data[3], sr->sense_data[4], sr->sense_data[5]);

	/*
	 * sense_len will be not set as Descriptor Type isn't 0x70
	 * if not set sense_len, sense will not be able to copy
	 * in sg_scsi_ioctl()
	 */
	lrbp->cmd->req.sense_len = CCD_SENSE_DATA_LEN;

	ufsf_copy_sense_data(lrbp);
}

inline int ufsf_get_scsi_device(struct ufs_hba *hba, struct scsi_device *sdev)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(hba->host->host_lock, flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		spin_unlock_irqrestore(hba->host->host_lock, flags);
		scsi_device_put(sdev);
		ERR_MSG("scsi_device_get failed.(%d)", ret);
		return -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);

	return ret;
}

inline bool ufsf_is_valid_lun(int lun)
{
	return lun < UFS_UPIU_MAX_GENERAL_LUN;
}

inline void ufsf_slave_configure(struct ufsf_feature *ufsf,
				 struct scsi_device *sdev)
{
	if (ufsf_is_valid_lun(sdev->lun)) {
		ufsf->sdev_ufs_lu[sdev->lun] = sdev;
		ufsf->slave_conf_cnt++;
		INFO_MSG("lun[%d] sdev(%p) q(%p) slave_conf_cnt(%d)",
			 (int)sdev->lun, sdev, sdev->request_queue,
			 ufsf->slave_conf_cnt);

	}
	schedule_work(&ufsf->device_check_work);
}

inline int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	int ret = 0;

#if defined(CONFIG_UFSSID)
	if (ufsf->sid_dev)
		ufssid_prep_fn(ufsf, lrbp);
#endif
	return ret;
}

/*
 * called by ufshcd_vops_device_reset()
 */
inline void ufsf_reset_host(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	struct Scsi_Host *host = hba->host;
	unsigned long flags;
	u32 eh_flags;

	if (!ufsf->check_init)
		return;

	/*
	 * Check if it is error handling(eh) context.
	 *
	 * In the following cases, we can enter here even though it is not in eh
	 * context.
	 *  - when ufshcd_is_link_off() is true in ufshcd_resume()
	 *  - when ufshcd_vops_suspend() fails in ufshcd_suspend()
	 */
	spin_lock_irqsave(host->host_lock, flags);
	eh_flags = ufshcd_eh_in_progress(hba);
	spin_unlock_irqrestore(host->host_lock, flags);
	if (!eh_flags)
		return;

#if defined(CONFIG_UFSHID)
	INFO_MSG("run reset_host.. hid_state(%d) -> HID_RESET",
		 ufshid_get_state(ufsf));
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_reset_host(ufsf);
#endif

#if defined(CONFIG_UFSFBO)
	INFO_MSG("run reset_host.. fbo_state(%d) -> FBO_RESET",
		 ufsfbo_get_state(ufsf));
	if (ufsfbo_get_state(ufsf) == FBO_PRESENT)
		ufsfbo_reset_host(ufsf);
#endif
	schedule_work(&ufsf->reset_wait_work);
}

inline void ufsf_init(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_NEED_INIT)
		ufshid_init(ufsf);
#endif
#if defined(CONFIG_UFSSID)
	if (ufsf->sid_dev)
		ufssid_init(ufsf);
#endif
#if defined(CONFIG_UFSFBO)
	if (ufsfbo_get_state(ufsf) == FBO_NEED_INIT)
		ufsfbo_init(ufsf);
#endif
	ufsf->check_init = true;
}

inline void ufsf_reset(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_RESET)
		ufshid_reset(ufsf);
#endif
#if defined(CONFIG_UFSFBO)
	if (ufsfbo_get_state(ufsf) == FBO_RESET)
		ufsfbo_reset(ufsf);
#endif
}

inline void ufsf_remove(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_remove(ufsf);
#endif
#if defined(CONFIG_UFSSID)
	if (ufsf->sid_dev)
		ufssid_remove(ufsf);
#endif
#if defined(CONFIG_UFSFBO)
	if (ufsfbo_get_state(ufsf) == FBO_PRESENT)
		ufsfbo_remove(ufsf);
#endif
}

static void ufsf_device_check_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, device_check_work);

	mutex_lock(&ufsf->device_check_lock);
	if (!ufsf->check_init) {
		ufsf_device_check(ufsf->hba);
		ufsf_init(ufsf);
	}
	mutex_unlock(&ufsf->device_check_lock);
}
/*
 * worker to change the feature state to present after processing the error handler.
 */
static void ufsf_reset_wait_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;
	struct ufs_hba *hba;
	struct Scsi_Host *host;
	u32 ufshcd_state;
	unsigned long flags;

	ufsf = container_of(work, struct ufsf_feature, reset_wait_work);
	hba = ufsf->hba;
	host = hba->host;

	/*
	 * Wait completion of hba->eh_work.
	 *
	 * reset_wait_work is scheduled at ufsf_reset_host(),
	 * so it can be waken up before eh_work is completed.
	 *
	 * ufsf_reset must be called after eh_work has completed.
	 */
	flush_work(&hba->eh_work);

	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_state = hba->ufshcd_state;
	spin_unlock_irqrestore(host->host_lock, flags);

	if (ufshcd_state == UFSHCD_STATE_OPERATIONAL)
		ufsf_reset(ufsf);
}

static void ufsf_resume_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf = container_of(work, struct ufsf_feature, resume_work);
	struct ufs_hba *hba = ufsf->hba;
	bool is_link_off = ufshcd_is_link_off(hba);

	/*
	 * Resume of UFS feature should be called after power & link state
	 * are changed to active. Therefore, it is synchronized as follows.
	 *
	 * System PM: waits to acquire the semaphore used by ufshcd_wl_resume()
	 * Runtime PM: resume using ufshcd_rpm_get_sync()
	 */
	down(&hba->host_sem);
	ufshcd_rpm_get_sync(hba);

	if (ufshcd_is_ufs_dev_active(hba) && ufshcd_is_link_active(hba))
		ufsf_resume(ufsf, is_link_off);

	ufshcd_rpm_put_sync(hba);
	up(&hba->host_sem);
}

#if defined(CONFIG_UFSHID)
static void ufsf_on_idle(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, on_idle_work);
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_on_idle(ufsf);
}
#endif

inline void ufsf_set_init_state(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	ufsf->hba = hba;
	ufsf->slave_conf_cnt = 0;
	ufsf->issue_ioctl = false;

	mutex_init(&ufsf->device_check_lock);
	INIT_WORK(&ufsf->device_check_work, ufsf_device_check_work_handler);
	INIT_WORK(&ufsf->reset_wait_work, ufsf_reset_wait_work_handler);
	INIT_WORK(&ufsf->resume_work, ufsf_resume_work_handler);

#if defined(CONFIG_UFSHID)
	INIT_WORK(&ufsf->on_idle_work, ufsf_on_idle);
	ufshid_set_state(ufsf, HID_NEED_INIT);
#endif
#if defined(CONFIG_UFSFBO)
	ufsfbo_set_state(ufsf, FBO_NEED_INIT);
#endif
}

inline void ufsf_suspend(struct ufsf_feature *ufsf)
{
	/*
	 * Wait completion of reset_wait_work.
	 *
	 * When suspend occurrs immediately after reset
	 * and reset_wait_work is executed late,
	 * we can enter here before ufsf_reset() cleans up the feature's reset sequence.
	 */
	flush_work(&ufsf->reset_wait_work);

#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_suspend(ufsf);
#endif

#if defined(CONFIG_UFSFBO)
	if (ufsfbo_get_state(ufsf) == FBO_PRESENT)
		ufsfbo_suspend(ufsf);
#endif
}

inline void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_SUSPEND)
		ufshid_resume(ufsf);
#endif
#if defined(CONFIG_UFSFBO)
	if (ufsfbo_get_state(ufsf) == FBO_SUSPEND) {
		ufsfbo_resume(ufsf, is_link_off);
	}
#endif
}

inline void ufsf_change_lun(struct ufsf_feature *ufsf,
			    struct ufshcd_lrb *lrbp)
{
	int ctx_lba = get_unaligned_be32(lrbp->cmd->cmnd + 2);

	if (unlikely(ufsf->issue_ioctl == true &&
	    ctx_lba == READ10_DEBUG_LBA)) {
		lrbp->lun = READ10_DEBUG_LUN;
		INFO_MSG("lun 0x%X lba 0x%X", lrbp->lun, ctx_lba);
	}
}

/*
 * Wrapper functions for ufshid.
 */
#if defined(CONFIG_UFSHID) && defined(CONFIG_UFSHID_DEBUG)
inline void ufsf_hid_acc_io_stat(struct ufsf_feature *ufsf,
				 struct ufshcd_lrb *lrbp)
{
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_acc_io_stat_during_trigger(ufsf, lrbp);
}
#else
inline void ufsf_hid_acc_io_stat(struct ufsf_feature *ufsf,
				 struct ufshcd_lrb *lrbp) {}
#endif

MODULE_LICENSE("GPL v2");
