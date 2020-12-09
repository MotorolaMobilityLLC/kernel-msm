// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/string.h>
#include <linux/pmic-voter.h>
#include <linux/mmi-pmic-voter.h>

int mmi_get_effective_result(struct votable *votable)
{
	if(votable)
		return get_effective_result(votable);

	return -EINVAL;
}
EXPORT_SYMBOL(mmi_get_effective_result);

int mmi_vote(struct votable *votable, const char *client_str, bool enabled, int val)
{
	if(votable)
		return vote(votable, client_str, enabled, val);

	return -EINVAL;
}
EXPORT_SYMBOL(mmi_vote);

int mmi_rerun_election(struct votable *votable)
{
	if(votable)
		return rerun_election(votable);

	return -EINVAL;
}
EXPORT_SYMBOL(mmi_rerun_election);

struct votable *mmi_find_votable(const char *name)
{
	return find_votable(name);
}
EXPORT_SYMBOL(mmi_find_votable);

int mmi_pmic_vote_force_val_set(struct votable *votable, u32 val)
{
	return pmic_vote_force_val_set(votable, val);
}
EXPORT_SYMBOL(mmi_pmic_vote_force_val_set);

int mmi_pmic_vote_force_active_get(struct votable *votable, u64 *val)
{
	return pmic_vote_force_active_get(votable, val);
}
EXPORT_SYMBOL(mmi_pmic_vote_force_active_get);

int mmi_pmic_vote_force_active_set(struct votable *votable, u64 val)
{
	return pmic_vote_force_active_set(votable, val);
}
EXPORT_SYMBOL(mmi_pmic_vote_force_active_set);

