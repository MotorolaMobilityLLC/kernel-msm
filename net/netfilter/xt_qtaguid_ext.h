/*
 * Copyright (C) [year] Motorola Mobility LLC.
 * All Rights Reserved.
 * Motorola Mobility Confidential Restricted.
 */

#ifndef __XT_QTAGUID_EXT_H__
#define __XT_QTAGUID_EXT_H__

#include <linux/netfilter/x_tables.h>

#define DEFAULT_MASK	0
#define DEBUG_OS_MASK	BIT(0)
#define DEBUG_ALL_MASK	BIT(1)

int recent_owner_init(void);

uid_t recent_owner_lookup(const struct sk_buff *skb,
			  struct xt_action_param *par);

void recent_owner_update(const struct sk_buff *skb,
			 struct xt_action_param *par,
			 uid_t uid,
			 int debug_os_data);

#endif  /* ifndef __XT_QTAGUID_EXT_H__ */
