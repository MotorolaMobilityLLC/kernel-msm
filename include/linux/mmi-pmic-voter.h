// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019-2020, The Linux Foundation. All rights reserved.
 */

 #ifndef __MMI_PMIC_VOTER_H
#define __MMI_PMIC_VOTER_H

#include <linux/pmic-voter.h>

int mmi_get_effective_result(struct votable *votable);
int mmi_vote(struct votable *votable, const char *client_str, bool enabled, int val);
int mmi_rerun_election(struct votable *votable);
struct votable *mmi_find_votable(const char *name);
int mmi_pmic_vote_force_val_set(struct votable *votable, u32 val);
int mmi_pmic_vote_force_active_get(struct votable *votable, u64 *val);
int mmi_pmic_vote_force_active_set(struct votable *votable, u64 val);

#endif /* __MMI_PMIC_VOTER_H */
