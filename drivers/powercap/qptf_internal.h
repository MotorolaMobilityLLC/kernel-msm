/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QPTF_INTERNAL_H__
#define __QPTF_INTERNAL_H__

#include <linux/powercap.h>

#define MAX_QPTM_CONSTRAINTS 1
#define QPTM_NAME_MAX	32

enum QPTM_NODE_TYPE {
	QPTM_NODE_VIRTUAL = 0,
	QPTM_NODE_DT,
};

struct qptm_ops;

struct qptm {
	struct powercap_zone zone;
	struct list_head sibling;
	struct list_head children;
	struct qptm_ops *ops;
	char name[QPTM_NAME_MAX];
	struct device_node *np;
	int count;
	struct list_head pz_list;
	struct list_head node;
	struct mutex lock;
	struct qptm *parent;
	void *dev;
};

struct qptm_ops {
	int (*set_enable)(struct qptm *qptm, bool enable);
	int (*get_enable)(struct qptm *qptm, bool *enable);
	u64 (*get_qptm_power_uw)(struct qptm *qptm);
	u64 (*get_qptm_energy_uj)(struct qptm *qptm);
	void (*release)(struct qptm *qptm);
};

struct qptm_node {
	enum QPTM_NODE_TYPE type;
	char name[QPTM_NAME_MAX];
	struct qptm_node *parent;
	struct qptm *qptm;
	struct device_node *np;
};

static inline struct qptm *to_qptm(struct powercap_zone *zone)
{
	return container_of(zone, struct qptm, zone);
}
#endif /* __QPTF_INTERNAL_H__ */
