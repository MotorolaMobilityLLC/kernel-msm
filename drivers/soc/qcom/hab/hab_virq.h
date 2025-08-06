/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __HAB_VIRQ_H
#define __HAB_VIRQ_H
#include <linux/eventfd.h>
#include "hab.h"

enum hvirq_id {
	VIRQ_1 = 1,
	VIRQ_2 = 2,
	VIRQ_3 = 3,
	VIRQ_4 = 4,
	VIRQ_5 = 5,
	VIRQ_6 = 6,
};

#define HAB_VIRTIRQ_MAX  (VIRQ_MAX/1000)

#define HAB_VIRQ_NODE  "virt-irq"
typedef int32_t (*virq_rx_cb_t)(int32_t irq, void *priv_data, uint32_t flags);
struct hvirq_dbl {
	struct list_head node;
	void *tx_dbl;
	void *rx_dbl;
	spinlock_t dbl_lock;
	int id;
	struct kref refcount;
	int virq_registered;
	int virtirq_label;
	int virtirq_num;

	/* QVM specific fields*/
	int32_t irq;
	void __iomem *base;

	int dom_id;

	/* Eventfd specific data*/
	struct eventfd_ctx *efd;
	int32_t fd;

	virq_rx_cb_t client_cb;
	void *client_pdata;
	uint32_t virq_send;
	uint32_t virq_recv;
	uint32_t flags;
};

struct virq_handle {
	int virq_num;
	int virq_label;
	uint32_t id;
};

struct local_virq {
	int32_t label[HAB_VIRTIRQ_MAX];
	int32_t cnt_virq;
};

extern struct local_virq virqsettings;

int hab_create_virq_cdev_node(int index);

int hab_virq_alloc(int i, int vmid_remote, int label, int irq, void __iomem *base);
int hab_virq_dealloc(int i, int vmid_remote);
int hab_virq_register(struct virq_uhab_context *ctx, int32_t *virq_handle, unsigned int vmid,
		unsigned int virq_num, virq_rx_cb_t rx_cb, void *priv, unsigned int flags);
int hab_virq_send(struct virq_uhab_context *ctx, int32_t virq_handle, unsigned int flags);
int hab_virq_unregister(struct virq_uhab_context *ctx, int32_t virq_handle, unsigned int flags);
struct hvirq_dbl *hab_virq_get_fromId(struct virq_uhab_context *ctx, int32_t id);
void hab_virq_put(struct hvirq_dbl *dbl);

struct virq_uhab_context *virq_hab_ctx_alloc(int kernel);

void virq_hab_ctx_free(struct kref *ref);

static inline void virq_hab_ctx_get(struct virq_uhab_context *ctx)
{
	if (ctx)
		kref_get(&ctx->refcount);
}

static inline void virq_hab_ctx_put(struct virq_uhab_context *ctx)
{
	if (ctx)
		(void)kref_put(&ctx->refcount, virq_hab_ctx_free);
}

int habhyp_virq_tx_register(struct hvirq_dbl *dbl, int dbl_label);
int habhyp_virq_rx_register(struct hvirq_dbl *dbl, int dbl_label);
int habhyp_virq_send(struct hvirq_dbl *dbl);
int habhyp_virq_tx_unregister(struct hvirq_dbl *dbl);
int habhyp_virq_rx_unregister(struct hvirq_dbl *dbl);
int habhyp_get_virq_num_id(void **virqdev, int label);
#endif
