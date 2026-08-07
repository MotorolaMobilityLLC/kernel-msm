/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QPTF_H__
#define __QPTF_H__

struct qptm;
struct powerzone_ops;

struct powerzone {
	struct qptm *qptm;
	struct device_node *qptm_np;
	struct powerzone_ops *ops;
	uint16_t ch_id;
	struct list_head node;
	bool attached;
	struct list_head qnode;
	struct mutex lock;
	void *devdata;
};

struct powerzone_ops {
	int (*set_enable)(struct powerzone *pz, bool enable);
	bool (*get_enable)(struct powerzone *pz);
	u64 (*get_energy)(struct powerzone *pz);
	u64 (*get_max_energy)(struct powerzone *pz);
	u64 (*get_power)(struct powerzone *pz);
	u64 (*get_max_power)(struct powerzone *pz);
};

#if IS_ENABLED(CONFIG_QCOM_POWER_TELEMETRY_FRAMEWORK)
struct powerzone *qptm_channel_register(struct device *dev, int channel_id,
			struct powerzone_ops *ops, void *data);
void qptm_channel_unregister(struct powerzone *pz);
void qptm_powerzone_update(struct powerzone *pz);
#else
static inline struct powerzone *qptm_channel_register(struct device *dev, int channel_id,
			struct powerzone_ops *ops, void *data)
{
	return NULL;
}
static inline void qptm_channel_unregister(struct powerzone *pz) { };
static inline void qptm_powerzone_update(struct powerzone *pz) { };
#endif

#endif /* __QPTF_H__ */
