// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "mem_buf_vmperm: " fmt

#include <linux/highmem.h>
#include <linux/mem-buf-exporter.h>
#include <linux/gunyah/gh_mem_notifier.h>
#include "mem-buf-dev.h"
#include "mem-buf-gh.h"
#include "mem-buf-ids.h"

/*
 * @kref - A refcount for @sgt and @sgt's private data, which
 *	is expected to contain a 'struct mem_buf_vmperm'
 * @rcu - Usage based off of mm/shrinker.c
 */
struct mem_buf_vmperm {
	struct list_head list;
	u32 flags;
	int current_vm_perms;
	u32 mapcount;
	int *vmids;
	int *perms;
	unsigned int nr_acl_entries;
	unsigned int max_acl_entries;
	struct dma_buf *dmabuf;
	struct sg_table *sgt;
	gh_memparcel_handle_t memparcel_hdl;
	struct mutex lock;
	mem_buf_dma_buf_destructor dtor;
	void *dtor_data;
	void (*kref_release)(struct kref *kref);
	struct kref *kref;
	struct rcu_head rcu;
};

/*
 * Xarrays have an internal lock; load/store operations dont't need to
 * hold vmperm_list_lock.
 */
static DEFINE_XARRAY(vmperm_xa);
static LIST_HEAD(vmperm_list);
static DEFINE_MUTEX(vmperm_list_lock);
static void *gh_rm_mem_notifier_cookie;

/*
 * Ensures the vmperm can hold at least nr_acl_entries.
 * Caller must hold vmperm->lock.
 */
static int mem_buf_vmperm_resize(struct mem_buf_vmperm *vmperm,
					u32 new_size)
{
	int *vmids_copy, *perms_copy, *vmids, *perms;
	u32 old_size;

	old_size = vmperm->max_acl_entries;
	if (old_size >= new_size)
		return 0;

	vmids = vmperm->vmids;
	perms = vmperm->perms;
	vmids_copy = kcalloc(new_size, sizeof(*vmids), GFP_KERNEL);
	if (!vmids_copy)
		return -ENOMEM;
	perms_copy = kcalloc(new_size, sizeof(*perms), GFP_KERNEL);
	if (!perms_copy)
		goto out_perms;

	if (vmperm->vmids) {
		memcpy(vmids_copy, vmids, sizeof(*vmids) * old_size);
		kfree(vmids);
	}
	if (vmperm->perms) {
		memcpy(perms_copy, perms, sizeof(*perms) * old_size);
		kfree(perms);
	}
	vmperm->vmids = vmids_copy;
	vmperm->perms = perms_copy;
	vmperm->max_acl_entries = new_size;
	return 0;

out_perms:
	kfree(vmids_copy);
	return -ENOMEM;
}

/*
 * Caller should hold vmperm->lock.
 */
static void mem_buf_vmperm_update_state(struct mem_buf_vmperm *vmperm, int *vmids,
			 int *perms, u32 nr_acl_entries)
{
	int i;
	size_t size = sizeof(*vmids) * nr_acl_entries;

	WARN_ON(vmperm->max_acl_entries < nr_acl_entries);

	memcpy(vmperm->vmids, vmids, size);
	memcpy(vmperm->perms, perms, size);
	vmperm->nr_acl_entries = nr_acl_entries;

	vmperm->current_vm_perms = 0;
	for (i = 0; i < nr_acl_entries; i++) {
		if (vmids[i] == current_vmid)
			vmperm->current_vm_perms = perms[i];
	}
}

/*
 * Some types of errors may leave the memory in an unknown state.
 * Since we cannot guarantee that accessing this memory is safe,
 * acquire an extra reference count to the underlying dmabuf to
 * prevent it from being freed.
 * If this error occurs during dma_buf_release(), the file refcount
 * will already be zero. In this case handling the error is the caller's
 * responsibility.
 */
static void mem_buf_vmperm_set_err(struct mem_buf_vmperm *vmperm)
{
	kref_get(vmperm->kref);
	vmperm->flags |= MEM_BUF_WRAPPER_FLAG_ERR;
}

/* Must be freed via mem_buf_vmperm_free. */
static struct mem_buf_vmperm *mem_buf_vmperm_alloc_flags(
	struct sg_table *sgt, u32 flags,
	int *vmids, int *perms, u32 nr_acl_entries,
	void (*release)(struct kref *), struct kref *kref,
	gh_memparcel_handle_t hdl)
{
	struct mem_buf_vmperm *vmperm;
	int ret;

	vmperm = kzalloc(sizeof(*vmperm), GFP_KERNEL);
	if (!vmperm)
		return ERR_PTR(-ENOMEM);

	mutex_init(&vmperm->lock);
	mutex_lock(&vmperm->lock);
	ret = mem_buf_vmperm_resize(vmperm, nr_acl_entries);
	if (ret) {
		mutex_unlock(&vmperm->lock);
		goto err_resize_state;
	}
	mem_buf_vmperm_update_state(vmperm, vmids, perms,
					nr_acl_entries);
	mutex_unlock(&vmperm->lock);
	vmperm->sgt = sgt;
	vmperm->flags = flags;
	vmperm->memparcel_hdl = hdl;
	vmperm->kref_release = release;
	vmperm->kref = kref;

	/*
	 * Hold an additional refcount, which represents the need to call
	 * mem_buf_vmperm_try_reclaim() on this memparcel_hdl.
	 * If it succeeds, the refcount is dropped.
	 */
	if (hdl != MEM_BUF_MEMPARCEL_INVALID) {
		if (xa_insert(&vmperm_xa, hdl, vmperm, GFP_KERNEL))
			goto err_xa;

		kref_get(vmperm->kref);
	}

	mutex_lock(&vmperm_list_lock);
	list_add_rcu(&vmperm->list, &vmperm_list);
	mutex_unlock(&vmperm_list_lock);

	return vmperm;

err_xa:
	kfree(vmperm->perms);
	kfree(vmperm->vmids);
err_resize_state:
	kfree(vmperm);
	return ERR_PTR(-ENOMEM);
}

struct mem_buf_vmperm *mem_buf_vmperm_alloc_accept(struct sg_table *sgt,
	gh_memparcel_handle_t memparcel_hdl, int *vmids, int *perms,
	unsigned int nr_acl_entries, void (*release)(struct kref *),
	struct kref *kref)
{
	return  mem_buf_vmperm_alloc_flags(sgt, MEM_BUF_WRAPPER_FLAG_ACCEPT,
		vmids, perms, nr_acl_entries, release, kref, memparcel_hdl);
}
EXPORT_SYMBOL(mem_buf_vmperm_alloc_accept);

struct mem_buf_vmperm *mem_buf_vmperm_alloc_staticvm(struct sg_table *sgt,
	int *vmids, int *perms, u32 nr_acl_entries,
	void (*release)(struct kref *), struct kref *kref)
{
	return mem_buf_vmperm_alloc_flags(sgt,
		MEM_BUF_WRAPPER_FLAG_STATIC_VM,
		vmids, perms, nr_acl_entries, release, kref,
		MEM_BUF_MEMPARCEL_INVALID);
}
EXPORT_SYMBOL(mem_buf_vmperm_alloc_staticvm);

struct mem_buf_vmperm *mem_buf_vmperm_alloc(struct sg_table *sgt,
	void (*release)(struct kref *), struct kref *kref)
{
	int vmids[1];
	int perms[1];

	vmids[0] = current_vmid;
	perms[0] = PERM_READ | PERM_WRITE | PERM_EXEC;
	return mem_buf_vmperm_alloc_flags(sgt, 0,
		vmids, perms, 1, release, kref,
		MEM_BUF_MEMPARCEL_INVALID);
}
EXPORT_SYMBOL(mem_buf_vmperm_alloc);

static int __mem_buf_vmperm_reclaim(struct mem_buf_vmperm *vmperm)
{
	int ret;
	int new_vmids[] = {current_vmid};
	int new_perms[] = {PERM_READ | PERM_WRITE | PERM_EXEC};

	ret = mem_buf_unassign_mem(vmperm->sgt, vmperm->vmids,
				   vmperm->nr_acl_entries,
				   vmperm->memparcel_hdl);
	if (ret)
		return ret;

	mem_buf_vmperm_update_state(vmperm, new_vmids, new_perms, 1);
	vmperm->flags &= ~MEM_BUF_WRAPPER_FLAG_LENDSHARE;
	xa_erase(&vmperm_xa, vmperm->memparcel_hdl);
	vmperm->memparcel_hdl = MEM_BUF_MEMPARCEL_INVALID;
	kref_put(vmperm->kref, vmperm->kref_release);
	return 0;
}

static int mem_buf_vmperm_relinquish(struct mem_buf_vmperm *vmperm)
{
	int ret;
	struct gh_sgl_desc *sgl_desc;

	sgl_desc = mem_buf_sgt_to_gh_sgl_desc(vmperm->sgt);
	if (IS_ERR(sgl_desc))
		return PTR_ERR(sgl_desc);

	ret = mem_buf_unmap_mem_s1(sgl_desc);
	kvfree(sgl_desc);
	if (ret)
		return ret;
	/*
	 * mem_buf_retrieve_release() uses memunmap_pages() to remove
	 * this from the linux page tables. This occurs after the
	 * stage 2 pagetable mapping is removed below.
	 */
	ret = mem_buf_unmap_mem_s2(vmperm->memparcel_hdl);
	if (ret)
		return ret;

	ret = gh_rm_mem_notify(vmperm->memparcel_hdl, GH_RM_MEM_NOTIFY_OWNER_RELEASED,
			GH_MEM_NOTIFIER_TAG_MEM_BUF, NULL);
	if (ret)
		pr_err("%s: gh_rm_mem_notify failed\n", __func__);

	xa_erase(&vmperm_xa, vmperm->memparcel_hdl);
	vmperm->memparcel_hdl = MEM_BUF_MEMPARCEL_INVALID;
	vmperm->flags &= ~MEM_BUF_WRAPPER_FLAG_ACCEPT;
	kref_put(vmperm->kref, vmperm->kref_release);
	return 0;
}

static void vmperm_rcu_cb(struct rcu_head *head)
{
	struct mem_buf_vmperm *vmperm = container_of(head, struct mem_buf_vmperm, rcu);

	kfree(vmperm->perms);
	kfree(vmperm->vmids);
	mutex_destroy(&vmperm->lock);
	kfree(vmperm);
}

void mem_buf_vmperm_free(struct mem_buf_vmperm *vmperm)
{
	WARN_ON(vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE);
	WARN_ON(vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT);

	mutex_lock(&vmperm_list_lock);
	list_del_rcu(&vmperm->list);
	mutex_unlock(&vmperm_list_lock);

	call_rcu(&vmperm->rcu, vmperm_rcu_cb);
}
EXPORT_SYMBOL_GPL(mem_buf_vmperm_free);

/*
 * Attempt to return to the default security state. For memory in the
 * LENDSHARE state, this is full access by the current VM. For memory
 * in the ACCEPT state, this is no access by the current VM.
 *
 * This function can fail; the hypervisor or other system entities
 * may hold references to memory in a secure state.
 */
int mem_buf_vmperm_try_reclaim(struct mem_buf_vmperm *vmperm)
{
	int ret = 0;

	if (vmperm->dtor) {
		ret = vmperm->dtor(vmperm->dtor_data);
		if (ret)
			return ret;
	}

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE)
		ret = __mem_buf_vmperm_reclaim(vmperm);
	else if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT)
		ret = mem_buf_vmperm_relinquish(vmperm);

	mutex_unlock(&vmperm->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(mem_buf_vmperm_try_reclaim);

int mem_buf_dma_buf_attach(struct dma_buf *dmabuf, struct dma_buf_attachment *attachment)
{
	struct mem_buf_dma_buf_ops *ops;

	ops  = container_of(dmabuf->ops, struct mem_buf_dma_buf_ops, dma_ops);
	return ops->attach(dmabuf, attachment);
}
EXPORT_SYMBOL(mem_buf_dma_buf_attach);

struct mem_buf_vmperm *to_mem_buf_vmperm(struct dma_buf *dmabuf)
{
	struct mem_buf_dma_buf_ops *ops;

	if (dmabuf->ops->attach != mem_buf_dma_buf_attach)
		return ERR_PTR(-EINVAL);

	ops = container_of(dmabuf->ops, struct mem_buf_dma_buf_ops, dma_ops);
	return ops->lookup(dmabuf);
}
EXPORT_SYMBOL(to_mem_buf_vmperm);

int mem_buf_dma_buf_set_destructor(struct dma_buf *buf,
				   mem_buf_dma_buf_destructor dtor,
				   void *dtor_data)
{
	struct mem_buf_vmperm *vmperm = to_mem_buf_vmperm(buf);

	if (IS_ERR(vmperm))
		return PTR_ERR(vmperm);

	vmperm->dtor = dtor;
	vmperm->dtor_data = dtor_data;

	return 0;
}
EXPORT_SYMBOL(mem_buf_dma_buf_set_destructor);

/*
 * With CFI enabled, ops->attach must be set from *this* modules in order
 * for the comparison test in to_mem_buf_vmperm() to work.
 */
struct dma_buf *
mem_buf_dma_buf_export(struct dma_buf_export_info *exp_info,
		       struct mem_buf_dma_buf_ops *ops)
{
	struct mem_buf_vmperm *vmperm;
	struct dma_buf *dmabuf;
	struct dma_buf_ops *dma_ops = &ops->dma_ops;

	if (dma_ops->attach != mem_buf_dma_buf_attach) {
		if (!dma_ops->attach) {
			dma_ops->attach = mem_buf_dma_buf_attach;
		} else {
			pr_err("Attach callback must be null! %ps\n", exp_info->ops);
			return ERR_PTR(-EINVAL);
		}
	}
	exp_info->ops = dma_ops;

	dmabuf = dma_buf_export(exp_info);
	if (IS_ERR(dmabuf))
		return dmabuf;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (WARN_ON(IS_ERR(vmperm))) {
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}

	vmperm->dmabuf = dmabuf;
	return dmabuf;
}
EXPORT_SYMBOL(mem_buf_dma_buf_export);

void mem_buf_vmperm_pin(struct mem_buf_vmperm *vmperm)
{
	mutex_lock(&vmperm->lock);
	vmperm->mapcount++;
	mutex_unlock(&vmperm->lock);
}
EXPORT_SYMBOL(mem_buf_vmperm_pin);

void mem_buf_vmperm_unpin(struct mem_buf_vmperm *vmperm)
{
	mutex_lock(&vmperm->lock);
	if (!WARN_ON(vmperm->mapcount == 0))
		vmperm->mapcount--;
	mutex_unlock(&vmperm->lock);
}
EXPORT_SYMBOL(mem_buf_vmperm_unpin);

static bool mem_buf_check_rw_perm(struct mem_buf_vmperm *vmperm)
{
	u32 perms = PERM_READ | PERM_WRITE;
	bool ret = false;

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ERR)
		goto unlock;
	if (!(((vmperm->current_vm_perms & perms) == perms) && vmperm->mapcount))
		goto unlock;
	ret = true;
unlock:
	mutex_unlock(&vmperm->lock);
	return ret;
}

/*
 * DC IVAC requires write permission, so no CMO on read-only buffers.
 * We allow mapping to iommu regardless of permissions.
 * Caller must have previously called mem_buf_vmperm_pin
 */
bool mem_buf_vmperm_can_cmo(struct mem_buf_vmperm *vmperm)
{
	return mem_buf_check_rw_perm(vmperm);
}
EXPORT_SYMBOL(mem_buf_vmperm_can_cmo);

bool mem_buf_vmperm_can_vmap(struct mem_buf_vmperm *vmperm)
{
	return mem_buf_check_rw_perm(vmperm);
}
EXPORT_SYMBOL(mem_buf_vmperm_can_vmap);

bool mem_buf_vmperm_can_mmap(struct mem_buf_vmperm *vmperm, struct vm_area_struct *vma)
{
	bool ret = false;

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ERR)
		goto unlock;
	if (!vmperm->mapcount)
		goto unlock;
	if (!(vmperm->current_vm_perms & PERM_READ))
		goto unlock;
	if (!(vmperm->current_vm_perms & PERM_WRITE) &&
		vma->vm_flags & VM_WRITE)
		goto unlock;
	if (!(vmperm->current_vm_perms & PERM_EXEC) &&
		vma->vm_flags & VM_EXEC)
		goto unlock;
	ret = true;
unlock:
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_vmperm_can_mmap);

static int validate_lend_vmids(struct mem_buf_lend_kernel_arg *arg,
				u32 op)
{
	int i;
	bool found = false;

	for (i = 0; i < arg->nr_acl_entries; i++) {
		if (arg->vmids[i] == current_vmid) {
			found = true;
			break;
		}
	}

	if (found && op == GH_RM_TRANS_TYPE_LEND) {
		pr_err_ratelimited("Lend cannot target the current VM\n");
		return -EINVAL;
	} else if (!found && op == GH_RM_TRANS_TYPE_SHARE) {
		pr_err_ratelimited("Share must target the current VM\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * Allow sharing buffers which are not mapped by either mmap, vmap, or dma.
 * Also allow sharing mapped buffers if the new S2 permissions are at least
 * as permissive as the old S2 permissions. Currently differences in
 * executable permission are ignored, under the assumption the memory will
 * not be used for this purpose.
 */
static bool validate_lend_mapcount(struct mem_buf_vmperm *vmperm,
				   struct mem_buf_lend_kernel_arg *arg)
{
	int i;
	int perms = PERM_READ | PERM_WRITE;

	if (!vmperm->mapcount)
		return true;

	for (i = 0; i < arg->nr_acl_entries; i++) {
		if (arg->vmids[i] == current_vmid &&
		    (arg->perms[i] & perms) == perms)
			return true;
	}

	pr_err_ratelimited("%s: dma-buf is pinned, dumping permissions!\n", __func__);
	for (i = 0; i < arg->nr_acl_entries; i++)
		pr_err_ratelimited("%s: VMID=%d PERM=%d\n", __func__,
				    arg->vmids[i], arg->perms[i]);
	return false;
}

/*
 * Notice - lending to VMs which don't exist yet is allowed, but notifying
 * vmids that don't exist is not. Since we don't have a good way to
 * check if a vmid exists (yet), send one notification at a time.
 */
static void mem_buf_lend_notify(struct mem_buf_vmperm *vmperm)
{
	int ret, i;
	struct gh_notify_vmid_desc *vmid_desc;

	vmid_desc = kzalloc(struct_size(vmid_desc, vmid_entries, 1),
				GFP_KERNEL);
	if (!vmid_desc)
		return;

	vmid_desc->n_vmid_entries = 1;
	for (i = 0; i < vmperm->nr_acl_entries; i++) {
		vmid_desc->vmid_entries[0].vmid = vmperm->vmids[i];

		ret = gh_rm_mem_notify(vmperm->memparcel_hdl, GH_RM_MEM_NOTIFY_RECIPIENT_SHARED,
				GH_MEM_NOTIFIER_TAG_MEM_BUF, vmid_desc);
		if (ret)
			pr_err("%s: gh_rm_mem_notify failed for vmid %d\n",
				__func__, vmperm->vmids[i]);
	}
	kfree(vmid_desc);
}

static int mem_buf_lend_internal(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg,
			u32 op)
{
	struct mem_buf_vmperm *vmperm;
	struct sg_table *sgt;
	int ret;

	if (!arg->nr_acl_entries || !arg->vmids || !arg->perms)
		return -EINVAL;

	if (!mem_buf_dev) {
		pr_err("%s: mem-buf driver not probed!\n", __func__);
		return -ENODEV;
	}

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm)) {
		pr_err_ratelimited("dmabuf ops %ps are not a mem_buf_dma_buf_ops\n",
				dmabuf->ops);
		return -EINVAL;
	}
	sgt = vmperm->sgt;

	ret = validate_lend_vmids(arg, op);
	if (ret)
		return ret;

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ERR) {
		pr_err_ratelimited("dma-buf is not in a usable state!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_STATIC_VM) {
		pr_err_ratelimited("dma-buf is staticvm type!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE) {
		pr_err_ratelimited("dma-buf already lent or shared!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT) {
		pr_err_ratelimited("dma-buf not owned by current vm!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (!validate_lend_mapcount(vmperm, arg)) {
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	/*
	 * Although it would be preferrable to require clients to decide
	 * whether they require cache maintenance prior to caling this function
	 * for backwards compatibility with ion we will always do CMO.
	 */
	dma_map_sgtable(mem_buf_dev, vmperm->sgt, DMA_TO_DEVICE, 0);
	dma_unmap_sgtable(mem_buf_dev, vmperm->sgt, DMA_TO_DEVICE, 0);

	ret = mem_buf_vmperm_resize(vmperm, arg->nr_acl_entries);
	if (ret)
		goto err_resize;

	ret = mem_buf_assign_mem(op, vmperm->sgt, arg);
	if (ret) {
		if (ret == -EADDRNOTAVAIL)
			mem_buf_vmperm_set_err(vmperm);
		goto err_assign;
	}

	mem_buf_vmperm_update_state(vmperm, arg->vmids, arg->perms,
			arg->nr_acl_entries);
	vmperm->flags |= MEM_BUF_WRAPPER_FLAG_LENDSHARE;
	vmperm->memparcel_hdl = arg->memparcel_hdl;
	kref_get(vmperm->kref);
	mutex_unlock(&vmperm->lock);

	ret = xa_insert(&vmperm_xa, vmperm->memparcel_hdl, vmperm,
				GFP_KERNEL);
	if (ret)
		goto err_xa;
	mem_buf_lend_notify(vmperm);

	return 0;
err_xa:
	mem_buf_vmperm_try_reclaim(vmperm);
	return ret;

err_assign:
err_resize:
	mutex_unlock(&vmperm->lock);
	return ret;
}

/*
 * Kernel API for Sharing, Lending, Receiving or Reclaiming
 * a dma-buf from a remote Virtual Machine.
 */
int mem_buf_lend(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg)
{
	return mem_buf_lend_internal(dmabuf, arg, GH_RM_TRANS_TYPE_LEND);
}
EXPORT_SYMBOL(mem_buf_lend);

int mem_buf_share(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg)
{
	int i, ret, len, found = false;
	int *orig_vmids, *vmids = NULL;
	int *orig_perms, *perms = NULL;

	len = arg->nr_acl_entries;
	for (i = 0; i < len; i++) {
		if (arg->vmids[i] == current_vmid) {
			found = true;
			break;
		}
	}

	if (found)
		return mem_buf_lend_internal(dmabuf, arg, GH_RM_TRANS_TYPE_SHARE);

	vmids = kmalloc_array(len + 1, sizeof(*vmids), GFP_KERNEL);
	if (!vmids)
		return -ENOMEM;

	perms = kmalloc_array(len + 1, sizeof(*perms), GFP_KERNEL);
	if (!perms) {
		kfree(vmids);
		return -ENOMEM;
	}

	/* Add current vmid with RWX permissions to the list */
	memcpy(vmids, arg->vmids, sizeof(*vmids) * len);
	memcpy(perms, arg->perms, sizeof(*perms) * len);
	vmids[len] = current_vmid;
	perms[len] = PERM_READ | PERM_WRITE | PERM_EXEC;

	/* Temporarily switch out the old arrays */
	orig_vmids = arg->vmids;
	orig_perms = arg->perms;
	arg->vmids = vmids;
	arg->perms = perms;
	arg->nr_acl_entries += 1;

	ret = mem_buf_lend_internal(dmabuf, arg, GH_RM_TRANS_TYPE_SHARE);
	/* Swap back */
	arg->vmids = orig_vmids;
	arg->perms = orig_perms;
	arg->nr_acl_entries -= 1;

	kfree(vmids);
	kfree(perms);
	return ret;
}
EXPORT_SYMBOL(mem_buf_share);

int mem_buf_reclaim(struct dma_buf *dmabuf)
{
	struct mem_buf_vmperm *vmperm;
	int ret;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm)) {
		pr_err_ratelimited("dmabuf ops %ps are not a mem_buf_dma_buf_ops\n",
				dmabuf->ops);
		return -EINVAL;
	}

	mutex_lock(&vmperm->lock);
	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_STATIC_VM) {
		pr_err_ratelimited("dma-buf is staticvm type!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (!(vmperm->flags & MEM_BUF_WRAPPER_FLAG_LENDSHARE)) {
		pr_err_ratelimited("dma-buf isn't lent or shared!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	if (vmperm->flags & MEM_BUF_WRAPPER_FLAG_ACCEPT) {
		pr_err_ratelimited("dma-buf not owned by current vm!\n");
		mutex_unlock(&vmperm->lock);
		return -EINVAL;
	}

	ret = __mem_buf_vmperm_reclaim(vmperm);
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_reclaim);

bool mem_buf_dma_buf_exclusive_owner(struct dma_buf *dmabuf)
{
	struct mem_buf_vmperm *vmperm;
	bool ret = false;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (WARN_ON(IS_ERR(vmperm)))
		return false;

	mutex_lock(&vmperm->lock);
	ret = !vmperm->flags;
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_dma_buf_exclusive_owner);

int mem_buf_dma_buf_get_vmperm(struct dma_buf *dmabuf, const int **vmids,
		const int **perms, int *nr_acl_entries)
{
	struct mem_buf_vmperm *vmperm;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm))
		return PTR_ERR(vmperm);

	mutex_lock(&vmperm->lock);

	*vmids = vmperm->vmids;
	*perms = vmperm->perms;
	*nr_acl_entries = vmperm->nr_acl_entries;

	mutex_unlock(&vmperm->lock);
	return 0;
}
EXPORT_SYMBOL(mem_buf_dma_buf_get_vmperm);

int mem_buf_dma_buf_copy_vmperm(struct dma_buf *dmabuf, int **vmids,
		int **perms, int *nr_acl_entries)
{
	struct mem_buf_vmperm *vmperm;
	size_t size;
	int *vmids_copy, *perms_copy;
	int ret;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm))
		return PTR_ERR(vmperm);

	mutex_lock(&vmperm->lock);
	size = sizeof(*vmids_copy) * vmperm->nr_acl_entries;
	vmids_copy = kmemdup(vmperm->vmids, size, GFP_KERNEL);
	if (!vmids_copy) {
		ret = -ENOMEM;
		goto err_vmids;
	}

	perms_copy = kmemdup(vmperm->perms, size, GFP_KERNEL);
	if (!perms_copy) {
		ret = -ENOMEM;
		goto err_perms;
	}

	*vmids = vmids_copy;
	*perms = perms_copy;
	*nr_acl_entries = vmperm->nr_acl_entries;

	mutex_unlock(&vmperm->lock);
	return 0;

err_perms:
	kfree(vmids_copy);
err_vmids:
	mutex_unlock(&vmperm->lock);
	return ret;
}
EXPORT_SYMBOL(mem_buf_dma_buf_copy_vmperm);

int mem_buf_dma_buf_get_memparcel_hdl(struct dma_buf *dmabuf,
				      gh_memparcel_handle_t *memparcel_hdl)
{
	struct mem_buf_vmperm *vmperm;

	vmperm = to_mem_buf_vmperm(dmabuf);
	if (IS_ERR(vmperm))
		return PTR_ERR(vmperm);

	if (vmperm->memparcel_hdl == MEM_BUF_MEMPARCEL_INVALID)
		return -EINVAL;

	*memparcel_hdl = vmperm->memparcel_hdl;

	return 0;
}
EXPORT_SYMBOL(mem_buf_dma_buf_get_memparcel_hdl);

/* Find the object with matching memparcel_hdl, and grab a reference */
static struct mem_buf_vmperm *vmperm_lookup(u32 hdl)
{
	struct mem_buf_vmperm *vmperm;

	rcu_read_lock();
	vmperm = xa_load(&vmperm_xa, hdl);
	if (vmperm && !kref_get_unless_zero(vmperm->kref))
		vmperm = NULL;
	rcu_read_unlock();

	return vmperm;
}

/*
 * Treat NOTIF_MEM_RELEASED as a hint. For example, if a buffer is shared
 * with both TUIVM and OEMVM, then we would only receive 2 MEM_RELEASED
 * calls if TUIVM and OEMVM both support sending notifications.
 */
static void mem_buf_vmperm_gh_notifier(enum gh_mem_notifier_tag tag, unsigned long action,
					void *data, void *_msg)
{
	struct mem_buf_vmperm *vmperm;
	struct gh_rm_notif_mem_released_payload *msg = _msg;

	if (action != GH_RM_NOTIF_MEM_RELEASED)
		return;

	/* Acquire refcount */
	vmperm = vmperm_lookup(msg->mem_handle);
	if (!vmperm) {
		pr_err("%s: No vmperm for handle %d\n", __func__, msg->mem_handle);
		return;
	}

	mem_buf_vmperm_try_reclaim(vmperm);
	/* Drop refcount from vmperm_lookup */
	kref_put(vmperm->kref, vmperm->kref_release);
}

int mem_buf_dma_buf_init(void)
{
	gh_rm_mem_notifier_cookie = gh_mem_notifier_register(GH_MEM_NOTIFIER_TAG_MEM_BUF,
						mem_buf_vmperm_gh_notifier, NULL);
	if (IS_ERR(gh_rm_mem_notifier_cookie)) {
		pr_err("Failed: gh_mem_notifier_register\n");
		return PTR_ERR(gh_rm_mem_notifier_cookie);
	}

	return 0;
}
