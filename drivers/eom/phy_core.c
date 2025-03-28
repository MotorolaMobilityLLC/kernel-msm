// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/phy_core.h>
#include <linux/slab.h>

#if IS_ENABLED(CONFIG_EOM_MSM)

static LIST_HEAD(phy_device_list);
static DEFINE_MUTEX(phy_list_lock);
/* Register a new PHY device */
int register_phy_device(struct eom_phy_ops *ops, void *priv, u16 index,
			u16 vendor_id, u16 device_id, u8 type, u8 lanes)
{
	struct eom_phy_device *entry;

	if (!ops || !ops->phy_read || !ops->phy_write)
		return -EINVAL;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->ops = ops;
	entry->priv = priv;
	entry->index = index;
	entry->vendor_id = vendor_id;
	entry->device_id = device_id;
	entry->type = type;
	entry->lanes = lanes;

	mutex_lock(&phy_list_lock);
	list_add(&entry->list, &phy_device_list);
	mutex_unlock(&phy_list_lock);

	pr_debug("EOM: PHY device registered:vendor=%04x, device=%04x, type=%d, lanes=%d\n",
		vendor_id, device_id, type, lanes);

	return 0;
}
EXPORT_SYMBOL_GPL(register_phy_device);

/* update phy caps */
void update_phy_device_nr_lanes(u16 index, u16 vendor_id, u16 device_id, u8 type, u8 lanes)
{
	struct eom_phy_device *entry = get_eom_phy_device(type, index, vendor_id, device_id);

	if (!entry) {
		pr_err("EOM: Failed to find phy device for vendor=%04x, device=%04x, type=%d, lanes=%d\n",
		       vendor_id, device_id, type, lanes);
		return;
	}

	entry->lanes = lanes;
	pr_debug("EOM: PHY device updated:vendor=%04x, device=%04x, type=%d, lanes=%d\n",
		vendor_id, device_id, type, lanes);
}
EXPORT_SYMBOL_GPL(update_phy_device_nr_lanes);

/* Unregister a PHY device */
void unregister_phy_device(struct eom_phy_ops *ops, u16 index, u8 type)
{
	struct eom_phy_device *entry, *tmp;

	mutex_lock(&phy_list_lock);
	list_for_each_entry_safe(entry, tmp, &phy_device_list, list) {
		if (entry->type == type && entry->ops == ops && entry->index == index) {
			list_del(&entry->list);
			kfree(entry);
			pr_debug("PHY device unregistered\n");
			break;
		}
	}
	mutex_unlock(&phy_list_lock);
}
EXPORT_SYMBOL_GPL(unregister_phy_device);

/* Get PHY device by Vendor ID and Device ID */
struct eom_phy_device *get_eom_phy_device(u8 type, u16 index, u16 vendor_id,
					  u16 device_id)
{
	struct eom_phy_device *entry;

	mutex_lock(&phy_list_lock);
	list_for_each_entry(entry, &phy_device_list, list) {
		if (type == entry->type && entry->index == index &&
		    entry->vendor_id == vendor_id &&
		    entry->device_id == device_id) {
			mutex_unlock(&phy_list_lock);
			return entry;
		}
	}
	mutex_unlock(&phy_list_lock);

	return NULL;
}

/* Read from PHY register */
int phy_read(struct eom_phy_device *phy, u32 reg, u32 *val)
{
	if (!phy || !phy->ops || !phy->ops->phy_read)
		return -EINVAL;

	return phy->ops->phy_read(phy->priv, reg, val);
}
EXPORT_SYMBOL_GPL(phy_read);

/* Write to PHY register */
int phy_write(struct eom_phy_device *phy, u32 reg, u32 val)
{
	if (!phy || !phy->ops || !phy->ops->phy_write)
		return -EINVAL;

	return phy->ops->phy_write(phy->priv, reg, val);
}
EXPORT_SYMBOL_GPL(phy_write);

/* Get number of lanes for a PHY */
int phy_get_num_lanes(struct eom_phy_device *phy)
{
	if (!phy || !phy->ops)
		return -EINVAL;

	/* if get_caps is not implemented return with initial number of lanes */
	if (phy->ops->get_caps)
		phy->ops->get_caps(phy->priv);

	return phy->lanes;
}
EXPORT_SYMBOL_GPL(phy_get_num_lanes);

/* exit the interface */
void phy_core_exit(void)
{
	struct eom_phy_device *entry, *tmp;

	mutex_lock(&phy_list_lock);
	list_for_each_entry_safe(entry, tmp, &phy_device_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}
	mutex_unlock(&phy_list_lock);

	pr_debug("PHY core exited\n");
}
EXPORT_SYMBOL_GPL(phy_core_exit);

#endif /* CONFIG_EOM_MSM */
