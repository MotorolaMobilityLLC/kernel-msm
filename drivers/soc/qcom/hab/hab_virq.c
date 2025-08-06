// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "hab_virq.h"

struct hab_virq_dev {
	struct hvirq_dbl *dbl;
};

static struct hab_virq_dev g_virtirq_dev[HAB_VIRTIRQ_MAX][HABCFG_VMID_MAX] = { 0 };

#ifndef CONFIG_MSM_VIRQ_HAB
int habhyp_virq_tx_register(struct hvirq_dbl *dbl, int dbl_label)
{
	return -EOPNOTSUPP;
}

int habhyp_virq_rx_register(struct hvirq_dbl *dbl, int dbl_label)
{
	return -EOPNOTSUPP;
}

int habhyp_virq_send(struct hvirq_dbl *dbl)
{
	return -EOPNOTSUPP;
}

int habhyp_virq_tx_unregister(struct hvirq_dbl *dbl)
{
	return -EOPNOTSUPP;
}

int habhyp_virq_rx_unregister(struct hvirq_dbl *dbl)
{
	return -EOPNOTSUPP;
}

int habhyp_get_virq_num_id(void **virqdev, int label)
{
	return -EOPNOTSUPP;
}
#endif
int hab_virq_alloc(int i, int vmid_remote, int label, int irq, void __iomem *base)
{
	struct hvirq_dbl *dbl = NULL;
	int ret = 0;

	ret = habhyp_get_virq_num_id((void **)&dbl, label);
	if (ret != 0) {
		if (ret == -EOPNOTSUPP) {
			pr_err("resume with hab insmod\n");
			return 0;
		}
		pr_err("failed to create virq for label 0x%x\n",
				label);
	} else {
		dbl->dom_id = vmid_remote;
		dbl->virq_registered = 0;
		dbl->virtirq_label = label;
		dbl->irq = irq;
		dbl->base = base;
		dbl->client_cb = NULL;
		dbl->efd = NULL;
		g_virtirq_dev[i][vmid_remote].dbl = dbl;
		pr_debug("alloc dbl id %d dom_id %d lbl %d\n",
				dbl->id, dbl->dom_id, dbl->virtirq_label);
	}
	return ret;
}

int hab_virq_dealloc(int i, int vmid_remote)
{
	struct hvirq_dbl *dbl;

	dbl = g_virtirq_dev[i][vmid_remote].dbl;
	if (dbl != NULL) {
		pr_debug("dealloc dbl id %d lbl %d dom_id %d\n", dbl->id,
				dbl->virtirq_label, dbl->dom_id);
		hab_virq_put(dbl);
		g_virtirq_dev[i][vmid_remote].dbl = NULL;
	}

	return 0;
}

static void hab_virq_free(struct kref *ref)
{
	struct hvirq_dbl *dbl = container_of(ref, struct hvirq_dbl, refcount);

	kfree(dbl);
}

struct hvirq_dbl *hab_virq_get_fromid(struct virq_uhab_context *ctx, int32_t id)
{
	struct hvirq_dbl *dbl;

	read_lock(&ctx->ctx_lock);
	list_for_each_entry(dbl, &ctx->virq, node) {
		if ((dbl->id == id) && (kref_get_unless_zero(&dbl->refcount) != 0)) {
			read_unlock(&ctx->ctx_lock);
			return dbl;
		}
	}
	read_unlock(&ctx->ctx_lock);

	pr_err("virq id %d not found\n", id);
	return NULL;
}

void hab_virq_get(struct hvirq_dbl *dbl)
{
	if (dbl != NULL)
		kref_get(&dbl->refcount);
}

void hab_virq_put(struct hvirq_dbl *dbl)
{
	if (dbl != NULL)
		kref_put(&dbl->refcount, hab_virq_free);
}

struct hvirq_dbl *hab_virtirq_freelabel_find(int vmid_remote, int virq_num)
{
	int i;
	struct hvirq_dbl *dbl;

	for (i = 0 ; i < HAB_VIRTIRQ_MAX; i++) {
		dbl = g_virtirq_dev[i][vmid_remote].dbl;
		if (dbl == NULL) {
			pr_err("dbl is NULL for virq_num %d\n", virq_num);
			return NULL;
		} else if (!dbl->virq_registered && (dbl->dom_id == vmid_remote)
				&& (dbl->virtirq_num == virq_num)) {
			return dbl;
		}
	}

	pr_err("Invalid virq_num %d or not found in devicetree\n", virq_num);
	return NULL;
}

int hab_virq_register(struct virq_uhab_context *ctx, int32_t *virq_handle, unsigned int vmid,
		unsigned int virq_num, virq_rx_cb_t rx_cb, void *priv, unsigned int flags)
{
	int ret = 0;
	struct hvirq_dbl *dbl = NULL;
	int *fd = NULL;

	dbl = hab_virtirq_freelabel_find(vmid, virq_num);
	if (dbl == NULL) {
		pr_err("find virtirq dbl failed vmid %d virq_num %d\n", vmid, virq_num);
		return -ENODEV;
	}

	hab_virq_get(dbl);

	if (HABMM_VIRQ_FLAGS_TX & flags) {
		ret = habhyp_virq_tx_register(dbl, dbl->virtirq_label);
		if (ret != 0) {
			pr_err("Failed to register tx virq_num %d ret %d\n", virq_num, ret);
			hab_virq_put(dbl);
			return ret;
		}

	} else if (HABMM_VIRQ_FLAGS_RX & flags) {
		if (rx_cb == NULL && (priv != NULL))
			fd = (int *)priv;

		/*
		 * fd will only be passed from userspace,
		 * so deal with it in case of non kernel context
		 */
		if (!ctx->kernel && (fd != NULL)) {
			if (*fd != -1) {
				dbl->efd = eventfd_ctx_fdget(*fd);

				if (IS_ERR(dbl->efd)) {
					ret = PTR_ERR(dbl->efd);
					pr_err("eventfd_ctx_fdget failed virq_num %d ret %d\n",
							virq_num, ret);
					hab_virq_put(dbl);
					return ret;
				}
				dbl->fd = *fd;
			}
		}

		ret = habhyp_virq_rx_register(dbl, dbl->virtirq_label);
		if (ret != 0) {
			pr_err("Failed to register rx virq_num %d ret %d\n", virq_num, ret);
			hab_virq_put(dbl);
			return ret;
		}

		/* Setup client callback & data */
		if ((rx_cb != NULL) && (priv != NULL)) {
			dbl->client_cb = rx_cb;
			dbl->client_pdata = priv;
		}

	} else {
		pr_err("Invalid flag passed for id %d lbl %d\n", dbl->id, dbl->virtirq_label);
		hab_virq_put(dbl);
		return -EINVAL;
	}

	dbl->virq_registered = 1;
	dbl->flags = flags;
	virq_hab_ctx_get(ctx);
	write_lock(&ctx->ctx_lock);
	list_add_tail(&dbl->node, &ctx->virq);
	ctx->virq_total++;
	write_unlock(&ctx->ctx_lock);

	*virq_handle = dbl->id;
	hab_virq_put(dbl);
	return ret;
}

int hab_virq_send(struct virq_uhab_context *ctx,
		int32_t virq_handle, unsigned int flags)
{
	struct hvirq_dbl *dbl;
	int ret;

	dbl = hab_virq_get_fromid(ctx, virq_handle);
	if (dbl == NULL) {
		pr_err("dbl device not found for id %d\n", virq_handle);
		return -ENODEV;
	}

	ret = habhyp_virq_send(dbl);
	if (ret) {
		pr_err("failed to raise virq to the sender dbl id %d ret %d\n", virq_handle, ret);
		hab_virq_put(dbl);
		return ret;
	}

	spin_lock(&dbl->dbl_lock);
	dbl->virq_send++;
	spin_unlock(&dbl->dbl_lock);

	hab_virq_put(dbl);

	return ret;
}

int hab_virq_unregister(struct virq_uhab_context *ctx,
		int32_t virq_handle, unsigned int flags)
{
	int ret = 0;
	struct hvirq_dbl *dbl = NULL;

	dbl = hab_virq_get_fromid(ctx, virq_handle);
	if (dbl == NULL) {
		pr_err("unregister fail, cannot find dbl id %d\n", virq_handle);
		return -EINVAL;
	}

	if (HABMM_VIRQ_FLAGS_TX & flags) {
		ret = habhyp_virq_tx_unregister(dbl);
		if (ret) {
			pr_err("failed to unregister dbl id %d ret is %d\n", dbl->id, ret);
			hab_virq_put(dbl);
			return ret;
		}
	} else if (HABMM_VIRQ_FLAGS_RX & flags) {
		ret = habhyp_virq_rx_unregister(dbl);
		if (ret) {
			pr_err("failed to unregister dbl id %d ret is %d\n", dbl->id, ret);
			hab_virq_put(dbl);
			return ret;
		}

		if (dbl->client_cb != NULL)
			dbl->client_cb = NULL;

		if (dbl->efd != NULL) {
			eventfd_ctx_put(dbl->efd);
			dbl->efd = NULL;
		}
	} else {
		pr_err("Invalid flag passed for id %d lbl %d\n", dbl->id, dbl->virtirq_label);
		hab_virq_put(dbl);
		return -EINVAL;
	}

	spin_lock(&dbl->dbl_lock);
	dbl->virq_registered = 0;
	spin_unlock(&dbl->dbl_lock);

	write_lock(&ctx->ctx_lock);
	list_del(&dbl->node);
	ctx->virq_total--;
	write_unlock(&ctx->ctx_lock);
	hab_virq_put(dbl);
	virq_hab_ctx_put(ctx);
	return ret;
}

struct virq_uhab_context *virq_hab_ctx_alloc(int kernel)
{
	struct virq_uhab_context *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL)
		return NULL;

	INIT_LIST_HEAD(&ctx->virq);
	kref_init(&ctx->refcount);

	ctx->kernel = kernel;
	rwlock_init(&ctx->ctx_lock);

	spin_lock(&hab_driver.drvlock);
	list_add_tail(&ctx->node, &hab_driver.virq_uctx_list);
	hab_driver.virq_ctx_cnt++;
	spin_unlock(&hab_driver.drvlock);

	pr_debug("virq ctx %pK live %d\n",
			ctx, hab_driver.virq_ctx_cnt);

	return ctx;
}

void virq_hab_ctx_free(struct kref *ref)
{
	struct virq_uhab_context *ctx =
		container_of(ref, struct virq_uhab_context, refcount);
	struct virq_uhab_context *ctxdel, *ctxtmp;

	/* walk virq list to find the leakage */
	spin_lock(&hab_driver.drvlock);
	hab_driver.virq_ctx_cnt--;
	list_for_each_entry_safe(ctxdel, ctxtmp, &hab_driver.virq_uctx_list, node) {
		if (ctxdel == ctx)
			list_del(&ctxdel->node);
	}
	spin_unlock(&hab_driver.drvlock);
	pr_debug("live ctx %d refcnt %d kernel %d owner %d\n",
			hab_driver.virq_ctx_cnt, get_refcnt(ctx->refcount),
			ctx->kernel, ctx->owner);
	kfree(ctx);
}

static int hab_virq_open(struct inode *inodep, struct file *filep)
{
	int result = 0;
	struct virq_uhab_context *ctx;

	ctx = virq_hab_ctx_alloc(0);
	if (ctx == NULL) {
		filep->private_data = NULL;
		return -ENOMEM;
	}

	ctx->owner = task_tgid_nr(current);
	filep->private_data = ctx;

	pr_debug("ctx owner %d refcnt %d\n", ctx->owner,
			get_refcnt(ctx->refcount));

	return result;
}

static int hab_virq_release(struct inode *inodep, struct file *filep)
{
	struct virq_uhab_context *ctx = filep->private_data;

	if (ctx == NULL)
		return 0;

	pr_debug("inode %pK filep %pK ctx %pK\n", inodep, filep, ctx);

	virq_hab_ctx_put(ctx);
	filep->private_data = NULL;

	return 0;
}

static long hab_virq_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct virq_uhab_context *ctx = (struct virq_uhab_context *)filep->private_data;
	struct hab_virq *send_virq_param = NULL;
	struct hab_virq_register *virq_register_param = NULL;
	struct hab_virq_unregister *virq_unregister_param = NULL;
	unsigned char data[256] = { 0 };
	long ret = 0;

	if (_IOC_SIZE(cmd) && (cmd & IOC_INOUT)) {
		if (_IOC_SIZE(cmd) > sizeof(data))
			return -EINVAL;

		if ((cmd & IOC_IN) != 0U)
			if (copy_from_user(data, (void __user *)arg, _IOC_SIZE(cmd))) {
				pr_err("copy_from_user failed cmd=%x size=%d\n",
						cmd, _IOC_SIZE(cmd));
				return -EFAULT;
			}
	}

	switch (cmd) {
	case IOCTL_HAB_VIRQ_REGISTER:
		virq_register_param = (struct hab_virq_register *)data;
		ret = hab_virq_register(ctx, &virq_register_param->virq_handle,
				virq_register_param->vmid,
				virq_register_param->virq_num, NULL,
				&virq_register_param->efd, virq_register_param->flags);
		break;
	case IOCTL_HAB_SEND_VIRQ:
		send_virq_param = (struct hab_virq *)data;
		ret = hab_virq_send(ctx, send_virq_param->virq_handle,
				send_virq_param->flags);
		break;
	case IOCTL_HAB_VIRQ_UNREGISTER:
		virq_unregister_param = (struct hab_virq_unregister *)data;
		ret = hab_virq_unregister(ctx, virq_unregister_param->virq_handle,
				virq_unregister_param->flags);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	if ((ret != -ENOIOCTLCMD) && _IOC_SIZE(cmd) && (cmd & IOC_OUT))
		if (copy_to_user((void __user *) arg, data, _IOC_SIZE(cmd))) {
			pr_err("copy_to_user failed cmd=%x\n", cmd);
			ret = -EFAULT;
		}

	return ret;
}

static long hab_virq_compat_ioctl(struct file *filep, unsigned int cmd,
		unsigned long arg)
{
	return hab_virq_ioctl(filep, cmd, arg);
}


static const struct file_operations hab_virq_fops = {
	.owner = THIS_MODULE,
	.open = hab_virq_open,
	.release = hab_virq_release,
	.unlocked_ioctl = hab_virq_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hab_virq_compat_ioctl,
#endif
};

int hab_create_virq_cdev_node(int index)
{
	int result;
	dev_t dev_no;

	cdev_init(&(hab_driver.cdev[index]), &hab_virq_fops);

	hab_driver.cdev[index].owner = THIS_MODULE;

	dev_no = MKDEV(hab_driver.major, index);

	result = cdev_add(&(hab_driver.cdev[index]), dev_no, 1);
	if (result) {
		pr_err("cdev_add failed for index %d %d\n", index, result);
		return result;
	}

	hab_driver.dev[index] = device_create(hab_driver.class, NULL,
			dev_no, &hab_driver, "hab-virq");

	if (IS_ERR_OR_NULL(hab_driver.dev[index])) {
		result = PTR_ERR(hab_driver.dev[index]);
		pr_err("index %d device_create /dev/hab-virq failed %d\n",
				index, result);
		cdev_del(&hab_driver.cdev[index]);
		hab_driver.dev[index] = NULL;
		return result;
	}

	pr_debug("create char device for /dev/hab-virq successful\n");
	return 0;
}
