// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"qptf: %s: " fmt, __func__

#include <linux/completion.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/soc/qcom/qptf.h>
#include "qptf_internal.h"

#define ROOT_NODE "root"
static LIST_HEAD(powerzone_list);
static LIST_HEAD(qptm_list);
static DEFINE_MUTEX(powerzone_list_lock);
static DEFINE_MUTEX(qptm_list_lock);

struct qptm_heirarchy {
	int count;
	int cur_index;
	struct powercap_control_type *pct;
	struct qptm_node *hierarchy;
	struct qptm *root;
};
static struct qptm_heirarchy *qh;
static const char *constraint_name = "Dummy";

static int get_time_window_us(struct powercap_zone *pcz, int cid, u64 *window)
{
	return -EOPNOTSUPP;
}

static int set_time_window_us(struct powercap_zone *pcz, int cid, u64 window)
{
	return -EOPNOTSUPP;
}

static int get_max_power_range_uw(struct powercap_zone *pcz, u64 *max_power_uw)
{
	return -EOPNOTSUPP;
}

static int get_power_limit_uw(struct powercap_zone *pcz,
			      int cid, u64 *power_limit)
{
	return -EOPNOTSUPP;
}

static int set_power_limit_uw(struct powercap_zone *pcz,
			      int cid, u64 power_limit)
{
	return -EOPNOTSUPP;
}

static const char *get_constraint_name(struct powercap_zone *pcz, int cid)
{
	return constraint_name;
}

static int get_max_power_uw(struct powercap_zone *pcz, int id, u64 *max_power)
{

	return -EOPNOTSUPP;
}

static int __get_energy_uj(struct qptm *qptm, u64 *energy_uj)
{
	struct qptm *child;
	u64 energy;
	int ret = 0;

	if (qptm->ops) {
		*energy_uj = qptm->ops->get_qptm_energy_uj(qptm);
		return 0;
	}

	*energy_uj = 0;

	list_for_each_entry(child, &qptm->children, sibling) {
		ret = __get_energy_uj(child, &energy);
		if (ret)
			break;
		*energy_uj += energy;
	}

	return ret;
}

static int get_energy_uj(struct powercap_zone *pcz, u64 *energy_uj)
{
	return __get_energy_uj(to_qptm(pcz), energy_uj);
}

static int __get_power_uw(struct qptm *qptm, u64 *power_uw)
{
	struct qptm *child;
	u64 power;
	int ret = 0;


	if (qptm->ops) {
		*power_uw = qptm->ops->get_qptm_power_uw(qptm);
		return 0;
	}

	*power_uw = 0;

	list_for_each_entry(child, &qptm->children, sibling) {
		ret = __get_power_uw(child, &power);
		if (ret)
			break;
		*power_uw += power;
	}

	return ret;
}

static int get_power_uw(struct powercap_zone *pcz, u64 *power_uw)
{
	return __get_power_uw(to_qptm(pcz), power_uw);
}

static int qptm_release_zone(struct powercap_zone *pcz)
{
	struct qptm *qptm = to_qptm(pcz);
	struct qptm *parent = qptm->parent;

	if (!list_empty(&qptm->children))
		return -EBUSY;

	if (parent)
		list_del(&qptm->sibling);

	if (qptm->ops) {
		qptm->ops->release(qptm);
	} else {
		mutex_lock(&qptm_list_lock);
		list_del(&qptm->node);
		mutex_unlock(&qptm_list_lock);
		kfree(qptm);
	}
	return 0;
}

static struct powercap_zone_constraint_ops constraint_ops = {
	.set_power_limit_uw = set_power_limit_uw,
	.get_power_limit_uw = get_power_limit_uw,
	.set_time_window_us = set_time_window_us,
	.get_time_window_us = get_time_window_us,
	.get_max_power_uw = get_max_power_uw,
	.get_name = get_constraint_name,
};

static struct powercap_zone_ops zone_ops = {
	.get_max_power_range_uw = get_max_power_range_uw,
	.get_power_uw = get_power_uw,
	.get_energy_uj = get_energy_uj,
	.release = qptm_release_zone,
};

static void qptm_ops_init(struct qptm *qptm, struct qptm_ops *ops)
{
	if (qptm) {
		INIT_LIST_HEAD(&qptm->children);
		INIT_LIST_HEAD(&qptm->sibling);
		qptm->ops = ops;
	}
}

static void qptm_unregister(struct qptm *qptm)
{
	powercap_unregister_zone(qh->pct, &qptm->zone);

	pr_debug("Unregistered qptm node '%s'\n", qptm->zone.name);
}

static int qptm_register(const char *name, struct qptm *qptm, struct qptm *parent)
{
	struct powercap_zone *pcz;
	struct qptm *prnt = NULL;

	if (!qh->pct)
		return -EAGAIN;

	if (parent && parent->ops)
		return -EINVAL;

	if (!qptm)
		return -EINVAL;

	if (qptm->ops && !(qptm->ops->get_qptm_power_uw &&
			   qptm->ops->get_qptm_energy_uj &&
			   qptm->ops->release))
		return -EINVAL;

	if (!parent || parent == qh->root)
		prnt = NULL;
	else
		prnt = parent;

	pcz = powercap_register_zone(&qptm->zone, qh->pct, name,
				     prnt ? &prnt->zone : NULL,
				     &zone_ops, MAX_QPTM_CONSTRAINTS,
				     &constraint_ops);
	if (IS_ERR(pcz))
		return PTR_ERR(pcz);

	if (parent) {
		list_add_tail(&qptm->sibling, &parent->children);
		qptm->parent = parent;
	} else {
		list_add_tail(&qptm->sibling, &qh->root->children);
		qptm->parent = qh->root;
	}

	pr_debug("Registered qptm node '%s'\n", qptm->zone.name);

	return 0;
}

static struct device_node *of_powerzone_find(struct device_node *pz_np, int id)
{
	struct device_node *qptm_np;
	struct of_phandle_args powerzone_specs;
	int idx;

	for (idx = 0; idx < qh->count; idx++) {
		int count, i;

		if (qh->hierarchy[idx].type != QPTM_NODE_DT)
			continue;

		count = of_count_phandle_with_args(qh->hierarchy[idx].np,
					"power-channels", "#powerzone-cells");
		if (count <= 0) {
			qptm_np = ERR_PTR(-EINVAL);
			goto out;
		}

		for (i = 0; i < count; i++) {
			int ret;

			ret = of_parse_phandle_with_args(qh->hierarchy[idx].np,
					"power-channels", "#powerzone-cells",
					i, &powerzone_specs);
			if (ret < 0) {
				pr_err("%pOFn: Failed to read powerzones cells:%d\n",
					qh->hierarchy[idx].np, ret);
				qptm_np = ERR_PTR(ret);
				goto out;
			}

			if ((pz_np == powerzone_specs.np) &&
					id == powerzone_specs.args[0]) {
				pr_debug("powerzone %pOFn id=0x%x belongs to %pOFn\n",
					pz_np, id, qh->hierarchy[idx].np);
				qptm_np = qh->hierarchy[idx].np;
				goto out;
			}
		}
		of_node_put(qh->hierarchy[idx].np);
	}
	qptm_np = ERR_PTR(-ENODEV);
out:
	of_node_put(qh->hierarchy[idx].np);

	return qptm_np;
}

/**
 * qptm_powerzone_update - Notify qptf about new power update.
 * @pz: a pointer to a powerzone structure corresponding to the channel
 * to be removed
 *
 * Remove qptm node that powerzone channel is belonging to.
 */
void qptm_powerzone_update(struct powerzone *pz)
{
	struct qptm *qptm;
	struct qptm_heirarchy *qh;

	if (!pz || pz->qptm)
		return;

	qptm = pz->qptm;
	qh = qptm->dev;

	sysfs_notify(&qh->pct->dev.kobj, NULL, "enabled");
}
EXPORT_SYMBOL_GPL(qptm_powerzone_update);

static u64 qptf_get_qptm_power_uw(struct qptm *qptm)
{
	struct powerzone *pos;
	u64 tot_power = 0;

	list_for_each_entry(pos, &qptm->pz_list, qnode)
		if (pos->ops && pos->ops->get_power)
			tot_power += pos->ops->get_power(pos);

	return tot_power;
}

static u64 qptf_get_qptm_energy_uj(struct qptm *qptm)
{
	struct powerzone *pos;
	u64 tot_energy = 0;

	list_for_each_entry(pos, &qptm->pz_list, qnode)
		if (pos->ops && pos->ops->get_energy)
			tot_energy += pos->ops->get_energy(pos);

	return tot_energy;
}

static int qptm_set_enable(struct qptm *qptm, bool mode)
{
	int ret = 0;
	struct powerzone *pos;

	list_for_each_entry(pos, &qptm->pz_list, qnode) {
		if (pos->ops->get_enable && pos->ops->set_enable) {
			if (mode != pos->ops->get_enable(pos)) {
				/* one power zone disable is sufficient now */
				ret = pos->ops->set_enable(pos, mode);
				break;
			}
		}
	}
	if (ret)
		pr_err("Failed to %s power zones\n", mode ? "disable" : "enable");

	return ret;
}

static int qptm_get_enable(struct qptm *qptm, bool *mode)
{
	int ret = 0;
	struct powerzone *pos;

	list_for_each_entry(pos, &qptm->pz_list, qnode) {
		if (pos->ops->get_enable) {
			if (!pos->ops->get_enable(pos)) {
				*mode = false;
				return 0;
			}
		}
	}
	*mode = true;

	return ret;
}

static void destroy_qptm(struct qptm *qptm)
{
	struct powerzone *pos, *aux;

	qptm_unregister(qptm);
	mutex_lock(&qptm_list_lock);
	list_del(&qptm->node);
	mutex_unlock(&qptm_list_lock);
	mutex_lock(&qptm->lock);
	list_for_each_entry_safe(pos, aux, &qptm->pz_list, qnode) {
		list_del(&pos->qnode);
		pos->attached = false;
	}
	mutex_unlock(&qptm->lock);
}

static void qptm_release(struct qptm *qptm)
{
	destroy_qptm(qptm);
}

static struct qptm_ops qptm_ops = {
	.get_qptm_power_uw = qptf_get_qptm_power_uw,
	.get_qptm_energy_uj = qptf_get_qptm_energy_uj,
	.set_enable = qptm_set_enable,
	.get_enable = qptm_get_enable,
	.release = qptm_release,
};

static struct qptm *find_qptm_from_list(struct device_node *np)
{
	struct qptm *pos;

	list_for_each_entry(pos, &qptm_list, node)
		if (pos->np == np)
			return pos;

	return NULL;
}

static struct qptm *create_qptm(struct device *dev, struct device_node *np)
{
	struct qptm *qptm;
	struct of_phandle_args powerzone_spec;
	int idx = 0, count, ret;
	struct powerzone *pos;
	bool found = false;

	count = of_count_phandle_with_args(np, "power-channels", "#powerzone-cells");
	if (count <= 0) {
		pr_err("Add a powerzones property with at least one powerzone\n");
		return ERR_PTR(-EINVAL);
	}

	qptm = find_qptm_from_list(np);
	if (!qptm) {
		qptm = devm_kzalloc(dev, sizeof(*qptm), GFP_KERNEL);
		if (!qptm)
			return ERR_PTR(-ENOMEM);
		strscpy(qptm->name, np->name, QPTM_NAME_MAX);
		qptm->count = count;
		qptm->np = np;
		mutex_init(&qptm->lock);
		INIT_LIST_HEAD(&qptm->pz_list);
		mutex_lock(&qptm_list_lock);
		list_add(&qptm->node, &qptm_list);
		mutex_unlock(&qptm_list_lock);
	}

	for (idx = 0; idx < count; idx++) {
		u32 ch_id;

		ret = of_parse_phandle_with_args(np, "power-channels",
			"#powerzone-cells", idx, &powerzone_spec);

		if (ret < 0) {
			pr_err("Invalid cooling-device entry\n");
			ret = -EINVAL;
			goto error_exit;
		}

		of_node_put(powerzone_spec.np);

		if (powerzone_spec.args_count < 1) {
			pr_err("wrong reference to powerzones, missing channel ids\n");
			ret = -EINVAL;
			goto error_exit;
		}

		ch_id = powerzone_spec.args[0];
		mutex_lock(&powerzone_list_lock);
		list_for_each_entry(pos, &powerzone_list, node) {
			if (pos->ch_id != ch_id)
				continue;
			found = true;
			if (!pos->attached) {
				mutex_lock(&qptm->lock);
				list_add(&pos->qnode, &qptm->pz_list);
				mutex_unlock(&qptm->lock);
				pos->attached = true;
			}
			break;
		}
		mutex_unlock(&powerzone_list_lock);
		if (!found) {
			ret = -EINVAL;
			goto error_exit;
		}
	}

	if (count != list_count_nodes(&qptm->pz_list))
		return qptm;

	for (idx = 0; idx < qh->count; idx++) {
		if (qh->hierarchy[idx].np != np)
			continue;
		qh->hierarchy[idx].qptm = qptm;
		qptm_ops_init(qptm, &qptm_ops);
		ret = qptm_register(np->name, qptm,
			qh->hierarchy[idx].parent ?
			qh->hierarchy[idx].parent->qptm :
			qh->root);
		if (ret) {
			pr_err("Failed to register '%s': %d\n", qptm->name, ret);
			goto error_exit;
		}
		break;
	}
	return qptm;

error_exit:
	return ret ? ERR_PTR(ret) : NULL;
}

/**
 * qptm_channel_register - register a qptm power capable channel with qptf
 * @dev: a device structure pointer
 * @channel_id: a channel identifier for power capable device
 * @ops: supported powerzone ops for that channel
 * @data: private data pointer from power capable device
 *
 * It registers a power capble device with qptf. The qptf framework checks
 * whether the requested channel is part of power-zone devicetree binding
 * for any qptm node or not. If it is not part of DT node, it exits with
 * error pointer. Otherwise, it will add powerzone channels to qptm node.
 *
 * The powerzone ops  must be initialized with the power numbers
 * before calling this function.
 *
 * Return: zero on success, a negative value in case of error:
 */
struct powerzone *qptm_channel_register(struct device *dev, int channel_id,
			struct powerzone_ops *ops, void *data)
{
	struct powerzone *pz;
	struct device_node *qptm_np;

	if (!ops || channel_id > 0xffff)
		return ERR_PTR(-EINVAL);

	qptm_np = of_powerzone_find(dev->of_node, channel_id);
	if (IS_ERR(qptm_np)) {
		if (PTR_ERR(qptm_np) != -ENODEV)
			pr_err("Failed to find powerzone for %pOFn id=0x%x\n",
			dev->of_node, channel_id);
		return ERR_CAST(qptm_np);
	}

	pz = devm_kzalloc(dev, sizeof(*pz), GFP_KERNEL);
	if (!pz)
		return ERR_PTR(-ENOMEM);

	pz->qptm_np = qptm_np;
	pz->ch_id = channel_id;
	pz->ops = ops;
	pz->devdata = data;
	mutex_init(&pz->lock);

	mutex_lock(&powerzone_list_lock);
	list_add(&pz->node, &powerzone_list);
	mutex_unlock(&powerzone_list_lock);
	pz->qptm = create_qptm(dev, qptm_np);

	return pz;
}
EXPORT_SYMBOL_GPL(qptm_channel_register);

/**
 * qptm_channel_unregister - Unregister a powerzone channel from qptm
 * @pz: a pointer to a powerzone structure corresponding to the channel
 * to be removed
 *
 * Remove qptm node that powerzone channel is belonging to.
 */
void qptm_channel_unregister(struct powerzone *pz)
{
	if (pz->qptm)
		destroy_qptm(pz->qptm);

	mutex_lock(&powerzone_list_lock);
	list_del(&pz->node);
	mutex_unlock(&powerzone_list_lock);
}
EXPORT_SYMBOL_GPL(qptm_channel_unregister);

static struct qptm *qptm_setup_virtual(struct qptm_node *hierarchy,
				       struct qptm *parent)
{
	struct qptm *qptm;
	int ret;

	qptm = kzalloc(sizeof(*qptm), GFP_KERNEL);
	if (!qptm)
		return ERR_PTR(-ENOMEM);

	strscpy(qptm->name, hierarchy->name, QPTM_NAME_MAX);
	qptm->np = hierarchy->np;
	mutex_init(&qptm->lock);
	INIT_LIST_HEAD(&qptm->pz_list);
	mutex_lock(&qptm_list_lock);
	list_add(&qptm->node, &qptm_list);
	mutex_unlock(&qptm_list_lock);
	qptm_ops_init(qptm, NULL);
	hierarchy->qptm = qptm;

	ret = qptm_register(hierarchy->name, qptm, parent);
	if (ret) {
		pr_err("Failed to register qptm node '%s': %d\n",
		       hierarchy->name, ret);
		kfree(qptm);
		return ERR_PTR(ret);
	}

	return qptm;
}

static int for_qptm_each_child(struct qptm_node *hierarchy,
		const struct qptm_node *it, struct qptm *parent)
{
	struct qptm *qptm = NULL;
	int i, ret;

	for (i = 0; i < qh->count; i++) {
		if (hierarchy[i].parent != it)
			continue;

		if (hierarchy[i].type == QPTM_NODE_VIRTUAL) {
			qptm = qptm_setup_virtual(&hierarchy[i], parent);
			/*
			 * A NULL pointer means there is no children, hence we
			 * continue without going deeper in the recursivity.
			 */
			if (!qptm)
				continue;
		}

		if (IS_ERR(qptm)) {
			pr_warn("Failed to create '%s' in the hierarchy\n",
				hierarchy[i].name);
			continue;
		}

		ret = for_qptm_each_child(hierarchy, &hierarchy[i], qptm);
		if (ret)
			return ret;
	}

	return 0;
}

static int qptm_pct_set_enable(struct powercap_control_type *pct, bool mode)
{
	int ret = 0;
	struct qptm *pos;

	if (qh->pct != pct)
		return -EINVAL;

	list_for_each_entry(pos, &qptm_list, node) {
		if (pos->ops && pos->ops->set_enable) {
			ret = pos->ops->set_enable(pos, mode);
			break;
		}
	}

	if (ret)
		pr_err("Failed to %s power zones\n", mode ? "disable" : "enable");

	return ret;
}

static int qptm_pct_get_enable(struct powercap_control_type *pct, bool *mode)
{
	int ret = 0;
	struct qptm *pos;

	if (qh->pct != pct)
		return -EINVAL;

	list_for_each_entry(pos, &qptm_list, node) {
		if (pos->ops && pos->ops->get_enable) {
			ret = pos->ops->get_enable(pos, mode);
			break;
		}
	}

	if (ret)
		pr_err("Failed to get power zones modes\n");

	return ret;
}

static struct powercap_control_type_ops pc_ops = {
	.set_enable = qptm_pct_set_enable,
	.get_enable = qptm_pct_get_enable,
};

static int qptm_create_root_node(void)
{
	 /*
	  * Create a root qptm node and this node won't be adding
	  * into powercap sysfs. It helps to add different dptm
	  * nodes as independent node under root node.
	  **/
	qh->root = kzalloc(sizeof(*qh->root), GFP_KERNEL);
	if (!qh->root)
		return -ENOMEM;

	strscpy(qh->root->name, ROOT_NODE, QPTM_NAME_MAX);
	qh->root->np = NULL;
	mutex_init(&qh->root->lock);
	INIT_LIST_HEAD(&qh->root->pz_list);
	mutex_lock(&qptm_list_lock);
	list_add(&qh->root->node, &qptm_list);
	mutex_unlock(&qptm_list_lock);
	qptm_ops_init(qh->root, NULL);

	return 0;
}

static int qptm_create_hierarchy(struct qptm_node *hierarchy)
{
	int ret;

	if (qh->pct)
		return -EBUSY;

	qh->pct = powercap_register_control_type(NULL, "qpt", &pc_ops);
	if (IS_ERR(qh->pct)) {
		pr_err("Failed to register control type\n");
		ret = PTR_ERR(qh->pct);
		goto out_pct;
	}

	if (!hierarchy) {
		ret = -EFAULT;
		goto out_err;
	}

	ret = for_qptm_each_child(hierarchy, NULL, qh->root);
	if (ret)
		goto out_err;

	return 0;

out_err:
	powercap_unregister_control_type(qh->pct);
out_pct:
	qh->pct = NULL;

	return ret;
}

static void __qptm_destroy_hierarchy(struct qptm *qptm)
{
	struct qptm *child, *aux;

	list_for_each_entry_safe(child, aux, &qptm->children, sibling)
		__qptm_destroy_hierarchy(child);

	/*
	 * At this point, we know all children were removed from the
	 * recursive call before
	 */
	if (qptm != qh->root) {
		qptm_unregister(qptm);
	} else {
		mutex_lock(&qptm_list_lock);
		list_del(&qptm->node);
		mutex_unlock(&qptm_list_lock);
		kfree(qptm);
	}
}

static void qptm_destroy_hierarchy(void)
{
	if (!qh->pct)
		return;

	__qptm_destroy_hierarchy(qh->root);

	powercap_unregister_control_type(qh->pct);

	qh->pct = NULL;

	qh->root = NULL;
}

static int of_each_qptm_child(struct device_node *root,
		struct device_node *np, struct qptm_node *parent)
{
	struct device_node *child;
	int ret;

	for_each_child_of_node(root, child) {
		if (qh->cur_index >= qh->count) {
			pr_err("mismatch in qptm count\n");
			return -EINVAL;
		}
		qh->hierarchy[qh->cur_index].np = child;
		strscpy(qh->hierarchy[qh->cur_index].name, child->name, QPTM_NAME_MAX);
		if (of_property_read_bool(child, "power-channels"))
			qh->hierarchy[qh->cur_index].type = QPTM_NODE_DT;
		else
			qh->hierarchy[qh->cur_index].type = QPTM_NODE_VIRTUAL;
		qh->hierarchy[qh->cur_index].parent = parent;
		qh->cur_index++;

		ret = of_each_qptm_child(child, root,
				&qh->hierarchy[qh->cur_index - 1]);
		if (ret)
			return ret;
	}

	return 0;
}

static int get_qptm_count(struct device_node *root)
{
	struct device_node *child;
	int count = 0;

	for_each_child_of_node(root, child) {
		count++;
		count += get_qptm_count(child);
	}

	return count;
}

static int qptf_init(void)
{
	struct device_node *np;
	int ret = 0;

	np = of_find_node_by_name(NULL, "power-zones");
	if (!np) {
		pr_debug("No power-zone channels are configured, exiting\n");
		return 0;
	}

	qh = kzalloc(sizeof(*qh), GFP_KERNEL);
	if (!qh)
		return -ENOMEM;

	ret = get_qptm_count(np);
	if (ret <= 0) {
		ret = -ENODEV;
		pr_err("Invalid count: %d\n", ret);
		goto release_qh;
	}

	qh->count = ret;
	qh->hierarchy = kcalloc(qh->count, sizeof(*qh->hierarchy), GFP_KERNEL);
	if (!qh->hierarchy) {
		ret = -ENOMEM;
		goto release_qh;
	}

	ret = qptm_create_root_node();
	if (ret < 0)
		goto release_hierarchy;

	ret = of_each_qptm_child(np, NULL, NULL);
	if (ret) {
		pr_err("Failed to read powerzones hierarchy: %d\n", ret);
		goto release_root;
	}

	ret = qptm_create_hierarchy(qh->hierarchy);
	if (ret < 0)
		goto release_root;

	return 0;

release_root:
	mutex_lock(&qptm_list_lock);
	list_del(&qh->root->node);
	mutex_unlock(&qptm_list_lock);
	kfree(qh->root);
release_hierarchy:
	kfree(qh->hierarchy);
release_qh:
	kfree(qh);

	return ret;
}

static void qptf_exit(void)
{
	if (!qh)
		return;

	qptm_destroy_hierarchy();
	if (qh && qh->hierarchy)
		kfree(qh->hierarchy);
	kfree(qh);
	qh = NULL;
}

module_init(qptf_init);
module_exit(qptf_exit);
MODULE_DESCRIPTION("Qcom Power Telemetry framework driver");
MODULE_LICENSE("GPL");
