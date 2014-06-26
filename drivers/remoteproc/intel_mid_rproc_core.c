/*
 * INTEL MID Remote Processor Core driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_ids.h>
#include <linux/platform_data/intel_mid_remoteproc.h>

#include "intel_mid_rproc_core.h"
#include "remoteproc_internal.h"

#define RPMSG_NS_ADDR	53

/**
 * rpmsg_ns_alloc() - allocate a name service annoucement structure
 * @name: name of remote service
 * @id: rproc type
 * @addr: address of remote service
 */
struct rpmsg_ns_info *rpmsg_ns_alloc(const char *name, int id, u32 addr)
{
	struct rpmsg_ns_info *ns_info;

	ns_info = kzalloc(sizeof(struct rpmsg_ns_info), GFP_KERNEL);
	if (ns_info) {
		strcpy(ns_info->name, name);
		ns_info->type = id;
		ns_info->addr = addr;
		ns_info->flags = RPMSG_NS_CREATE;
	}

	return ns_info;
};
EXPORT_SYMBOL_GPL(rpmsg_ns_alloc);

/**
 * rpmsg_ns_add_to_list() -- add a name service node to the global list
 * @info: name service node
 */
void rpmsg_ns_add_to_list(struct rpmsg_ns_info *info,
				struct rpmsg_ns_list *nslist)
{
	mutex_lock(&nslist->lock);
	list_add_tail(&info->node, &nslist->list);
	mutex_unlock(&nslist->lock);
}
EXPORT_SYMBOL_GPL(rpmsg_ns_add_to_list);

/**
 * free_rpmsg_ns() -- free rpmsg name service node
 * @info: name service node
 */
void free_rpmsg_ns(struct rpmsg_ns_info *info)
{
	kfree(info);
}

/**
 * rpmsg_ns_del_list() -- free rpmsg name service list
 */
void rpmsg_ns_del_list(struct rpmsg_ns_list *nslist)
{
	struct rpmsg_ns_info *info, *next;
	mutex_lock(&nslist->lock);
	list_for_each_entry_safe(info, next, &nslist->list, node) {
		list_del(&info->node);
		free_rpmsg_ns(info);
	}
	mutex_unlock(&nslist->lock);
}
EXPORT_SYMBOL_GPL(rpmsg_ns_del_list);

/**
 * find_rvdev() - find the rproc state of a supported virtio device
 * @rproc: rproc handle
 * @id: virtio device id
 */
struct rproc_vdev *find_rvdev(struct rproc *rproc, int id)
{
	struct rproc_vdev *rvdev;

	list_for_each_entry(rvdev, &rproc->rvdevs, node)
		if (rvdev->vdev.id.device == id)
			return rvdev;

	return NULL;
}

/*
 * Since we could not get vring structure directly from rproc_vring
 * structure, we have to create two local vrings and identify them
 * by matching with rproc_vrings.
 * @id: virtio device id.
 * Currently one rproc_vdev is supported by firmware, and the id is
 * VIRTIO_ID_RPMSG (declared in linux/virtio_ids.h).
 */
int find_vring_index(struct rproc *rproc, int vqid, int id)
{
	struct rproc_vdev *rvdev;
	struct device *dev = rproc->dev.parent;
	int vring_idx = 0;

	rvdev = find_rvdev(rproc, id);
	if (rvdev == NULL) {
		dev_err(dev, "virtio device not found\n");
		return -EINVAL;
	}

	while (vring_idx < RVDEV_NUM_VRINGS) {
		if (rvdev->vring[vring_idx].notifyid == vqid)
			break;
		vring_idx++;
	}

	/* no match found? there's a problem */
	if (vring_idx == RVDEV_NUM_VRINGS) {
		dev_err(dev, "Can not find vring\n");
		return -EINVAL;
	}

	return vring_idx;
}

void intel_mid_rproc_vring_init(struct rproc *rproc,
			struct vring *vring, enum local_vring_idx id)
{
	int align, len;
	void *addr;
	struct rproc_vdev *rvdev;
	struct device *dev = rproc->dev.parent;

	rvdev = find_rvdev(rproc, VIRTIO_ID_RPMSG);
	if (rvdev == NULL) {
		dev_err(dev, "virtio device not found\n");
		return;
	}

	addr = rvdev->vring[id].va;
	align = rvdev->vring[id].align;
	len = rvdev->vring[id].len;
	vring_init(vring, len, addr, align);
}

/**
 * intel_mid_rproc_vq_interrupt() - inform a vq interrupt to rproc
 *				    after vq buffers are handled
 * @rproc: rproc handle
 * @msg: vq notify id
 */
void intel_mid_rproc_vq_interrupt(struct rproc *rproc, int msg)
{
	struct device *dev = rproc->dev.parent;

	if (rproc_vq_interrupt(rproc, msg) == IRQ_NONE)
		dev_err(dev, "no message was found in vqid %d\n", msg);
}

/**
 * intel_mid_rproc_msg_handle() - generic interface as a vq buffer handle
 *				  during rpmsg transaction
 * @iproc: intel mid rproc data
 */
int intel_mid_rproc_msg_handle(struct intel_mid_rproc *iproc)
{
	int ret;
	struct vring *r_vring, *s_vring;
	void *r_virt_addr, *s_virt_addr;
	u16 r_idx, s_idx;
	u64 r_dma_addr, s_dma_addr;
	u32 r_len, s_len;

	r_vring = &iproc->rx_vring;
	s_vring = &iproc->tx_vring;

	r_idx = iproc->r_vring_last_used & (r_vring->num - 1);
	s_idx = iproc->s_vring_last_used & (s_vring->num - 1);

	r_dma_addr = r_vring->desc[r_idx].addr;
	s_dma_addr = s_vring->desc[s_idx].addr;

	r_virt_addr = phys_to_virt(r_dma_addr);
	s_virt_addr = phys_to_virt(s_dma_addr);

	ret = iproc->rproc_rpmsg_handle(r_virt_addr, s_virt_addr,
						&r_len, &s_len);

	r_vring->used->ring[r_idx].id = r_idx;
	r_vring->used->ring[r_idx].len = r_len;
	r_vring->used->idx++;

	s_vring->used->ring[s_idx].id = s_idx;
	s_vring->used->ring[s_idx].len = s_len;
	s_vring->used->idx++;

	iproc->r_vring_last_used++;
	iproc->s_vring_last_used++;

	return ret;
}

/**
 * Remoteproc side rx buffer handler during name service creation.
 * @iproc: intel mid rproc data
 * @ns_info: name service info
 *
 * After remote processor receives name service messages, it needs to
 * update the elements of its virtio device's rx virtqueue buffer
 * before next rpmsg transaction.
 * Here we have this function simulating the above effect.
 */
int intel_mid_rproc_ns_handle(struct intel_mid_rproc *iproc,
				struct rpmsg_ns_info *ns_info)
{
	u16 index;
	u32 len;
	u64 dma_addr;
	void *virt_addr;

	struct vring *r_vring;
	struct rpmsg_hdr *msg;
	struct rpmsg_ns_msg *nsm;

	if (ns_info == NULL) {
		pr_err("ns_info = NULL\n");
		return -ENODEV;
	}

	r_vring = &iproc->rx_vring;

	index = iproc->r_vring_last_used & (r_vring->num - 1);

	len = sizeof(*msg) + sizeof(*nsm);

	dma_addr = r_vring->desc[index].addr;
	virt_addr = phys_to_virt(dma_addr);

	msg = (struct rpmsg_hdr *)virt_addr;
	nsm = (struct rpmsg_ns_msg *)(virt_addr + sizeof(*msg));

	nsm->addr = ns_info->addr;
	nsm->flags = ns_info->flags;
	strncpy(nsm->name, ns_info->name, RPMSG_NAME_SIZE);

	msg->len = sizeof(*nsm);
	msg->src = nsm->addr;
	msg->dst = RPMSG_NS_ADDR;

	r_vring->used->ring[index].id = index;
	r_vring->used->ring[index].len = len;
	r_vring->used->idx++;

	iproc->r_vring_last_used++;

	return 0;
}
