/*
 * Copyright (C) 2020 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _UFSHPB_TOSHIBA_IF_H_
#define _UFSHPB_TOSHIBA_IF_H_
#include "ufs_quirks.h"

#define UFSHCD_DEVICE_HPB_SUPPORT_BIT_POSITION 7
#define UFSHCD_DEVICE_HPB_SUPPORT_BIT UFS_BIT(UFSHCD_DEVICE_HPB_SUPPORT_BIT_POSITION)
#if defined(CONFIG_UFSHPB_TOSHIBA)
static inline bool ufshcd_is_hpb_supported(struct ufs_hba *hba)
{
	return ((!!(hba->dev_info.d_ext_ufs_feature_sup & UFSHCD_DEVICE_HPB_SUPPORT_BIT)) &&
			(!!(hba->dev_info.quirks &UFS_DEVICE_QUIRK_HPB_DEVICECONTROL))) ;
}
#else
static inline bool ufshcd_is_hpb_supported(struct ufs_hba *hba) {}
#endif
struct ufshcd_lrb;
void ufshcd_init_hpb(struct ufs_hba *hba);
void ufshpb_prep_fn_toshiba(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb_rsp_upiu_toshiba(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb_release_toshiba(struct ufs_hba *hba, int state);
void ufshpb_suspend_toshiba(struct ufs_hba *hba);
void ufshpb_resume_toshiba(struct ufs_hba *hba);
#endif /* End of Header */
