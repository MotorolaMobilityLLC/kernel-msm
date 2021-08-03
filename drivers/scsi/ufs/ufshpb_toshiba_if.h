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
#if defined(CONFIG_UFSHPB_TOSHIBA)
static inline bool ufshcd_is_hpb_supported(unsigned int mfrid){
        return (IS_TOSHIBA_DEVICE(mfrid)) ;
}
#else
static inline bool ufshcd_is_hpb_supported(unsigned int mfrid){return false;}
#endif
struct ufshcd_lrb;
void ufshcd_init_hpb_toshiba(struct ufs_hba *hba);
void ufshpb_prep_fn_toshiba(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb_rsp_upiu_toshiba(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufshpb_release_toshiba(struct ufs_hba *hba, int state);
void ufshpb_suspend_toshiba(struct ufs_hba *hba);
void ufshpb_resume_toshiba(struct ufs_hba *hba);
#endif /* End of Header */
