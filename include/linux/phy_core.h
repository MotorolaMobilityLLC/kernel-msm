/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef PHY_CORE_H
#define PHY_CORE_H

#include <linux/types.h>

/* Phy Type Identifiers */
#define PHY_TYPE_DUMMY 0
#define PHY_TYPE_PCIE 1
#define PHY_TYPE_USB 2

/* PHY Operations */
struct eom_phy_ops {
	int (*phy_read)(void *priv, u32 reg, u32 *val);
	int (*phy_write)(void *priv, u32 reg, u32 val);
	void (*get_caps)(void *priv);
};

/* PHY Device Entry */
struct eom_phy_device {
	struct list_head list;
	struct eom_phy_ops *ops;
	void *priv;
	u16 index;
	u16 vendor_id;
	u16 device_id;
	u8 type;
	u8 lanes;
};

/* Function Prototypes */
#if IS_ENABLED(CONFIG_EOM_MSM)
int register_phy_device(struct eom_phy_ops *ops, void *priv, u16 index, u16 vendor_id,
			u16 device_id, u8 type, u8 lanes);
void unregister_phy_device(struct eom_phy_ops *ops, u16 index, u8 type);
struct eom_phy_device *get_eom_phy_device(u8 type, u16 index, u16 vendor_id, u16 device_id);
int phy_read(struct eom_phy_device *phy, u32 reg, u32 *val);
int phy_write(struct eom_phy_device *phy, u32 reg, u32 val);
int phy_get_num_lanes(struct eom_phy_device *phy);
void update_phy_device_nr_lanes(u16 index, u16 vendor_id, u16 device_id, u8 type, u8 lanes);
void phy_core_exit(void);
#else /* !CONFIG_EOM_MSM */
static inline int register_phy_device(struct eom_phy_ops *ops, void *priv, u16 index, u16 vendor_id,
				      u16 device_id, u8 type, u8 lanes)
{ return 0; }
static inline void unregister_phy_device(struct eom_phy_ops *ops, u16 index, u8 type) {}
static inline struct eom_phy_device *get_eom_phy_device(u8 type, u16 index, u16 vendor_id,
							u16 device_id)
{ return NULL; }
static inline int phy_read(struct eom_phy_device *phy, u32 reg, u32 *val)
{ return 0; }
static inline int phy_write(struct eom_phy_device *phy, u32 reg, u32 val)
{ return 0; }
static inline int phy_get_num_lanes(struct eom_phy_device *phy)
{ return 0; }
static inline void update_phy_device_nr_lanes(u16 index, u16 vendor_id, u16 device_id, u8 type,
					      u8 lanes) {}
static inline void phy_core_exit(void) {}
#endif /* CONFIG_EOM_MSM */
#endif /* PHY_CORE_H */
