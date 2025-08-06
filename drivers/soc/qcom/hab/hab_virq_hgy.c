// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/gunyah/gh_dbl.h>
#include "hab.h"
#include "hab_virq.h"

#define DDUMP_DBL_MASK  0x1

/* These values are configured in device tree of HAB
 * under virt-irq option
 */
#define GH_HYP_IRQ1 0x5
#define GH_HYP_IRQ2 0x6
#define GH_HYP_IRQ3 0x7
#define GH_HYP_IRQ4 0x8
#define GH_HYP_IRQ5 0x9
#define GH_HYP_IRQ6 0xA

static struct virq_handle virqid[] = {
	{ VIRQ_DISP1, GH_HYP_IRQ1, VIRQ_1},
	{ VIRQ_DISP2, GH_HYP_IRQ2, VIRQ_2},
	{ VIRQ_GFX, GH_HYP_IRQ3, VIRQ_3},
	{ VIRQ_MISC, GH_HYP_IRQ4, VIRQ_4},
	{ VIRQ_VID, GH_HYP_IRQ5, VIRQ_5},
	{ VIRQ_AUD, GH_HYP_IRQ6, VIRQ_6},
};

int habhyp_get_virq_num_id(void **virqdev, int label)
{
	int i = 0;
	struct hvirq_dbl *dbl;

	if (*virqdev != NULL) {
		pr_err("invalid input arg for lbl %d\n", label);
		return -EINVAL;
	}

	dbl = kzalloc(sizeof(*dbl), GFP_KERNEL);
	if (dbl == NULL)
		return -ENOMEM;

	spin_lock_init(&dbl->dbl_lock);
	kref_init(&dbl->refcount);
	for (i = 0 ; i < ARRAY_SIZE(virqid); i++) {
		if (label == virqid[i].virq_label) {
			dbl->id =  virqid[i].id;
			dbl->virtirq_num = virqid[i].virq_num;
			*virqdev = dbl;
			return 0;
		}
	}
	hab_virq_put(dbl);
	pr_err("virq lbl %d not supported\n", label);
	return -ENODEV;
}

/* callback function for receiving doorball */
static void gh_dbl_recv_cb(int irq, void *data)
{
	int ret = 0;
	gh_dbl_flags_t dbl_mask = 0x1;
	struct hvirq_dbl *dbl;

	dbl = (struct hvirq_dbl *)data;

	ret = gh_dbl_read_and_clean(dbl->rx_dbl, &dbl_mask, GH_DBL_NONBLOCK);
	if (ret)
		pr_err("dbl read failure id %d lbl %d ret %d\n", dbl->id,
				dbl->virtirq_label, ret);
	else
		pr_debug("read successful for id %d lbl %d\n", dbl->id,
				dbl->virtirq_label);

	spin_lock_irq(&dbl->dbl_lock);
	if (dbl->efd != NULL) {
		pr_info("fd is %d for id %d lbl %d\n", dbl->fd, dbl->id, dbl->virtirq_label);
		eventfd_signal(dbl->efd, 1);
	}
	dbl->virq_recv++;
	spin_unlock_irq(&dbl->dbl_lock);

	/* Eventually call the client cb function */
	if ((dbl->client_cb != NULL) && (dbl->client_pdata != NULL)) {
		ret = dbl->client_cb(irq, dbl->client_pdata, dbl->flags);
		if (ret)
			pr_err("cb fail for id %d lbl %d ret: %d\n", dbl->id,
					dbl->virtirq_label, ret);
	} else
		pr_err("client cb not registered for id %d lbl %d ret: %d\n", dbl->id,
				dbl->virtirq_label, ret);
}

int habhyp_virq_tx_register(struct hvirq_dbl *dbl, int dbl_label)
{
	int ret = 0;

	dbl->tx_dbl = gh_dbl_tx_register(dbl_label);
	if (IS_ERR_OR_NULL(dbl->tx_dbl)) {
		ret = PTR_ERR(dbl->tx_dbl);
		pr_err("tx reg failed for lbl %d\n", dbl_label);
		return ret;
	}
	return 0;
}

int habhyp_virq_rx_register(struct hvirq_dbl *dbl, int dbl_label)
{
	int ret = 0;

	dbl->rx_dbl = gh_dbl_rx_register(dbl_label, gh_dbl_recv_cb, dbl);
	if (IS_ERR_OR_NULL(dbl->rx_dbl)) {
		ret = PTR_ERR(dbl->rx_dbl);
		pr_err("rx reg failed for lbl %d\n", dbl_label);
		return ret;
	}
	return 0;
}

int habhyp_virq_send(struct hvirq_dbl *dbl)
{
	int ret = 0;
	gh_dbl_flags_t dbl_mask = DDUMP_DBL_MASK;

	ret = gh_dbl_send(dbl->tx_dbl, &dbl_mask, 0);
	return ret;
}

int habhyp_virq_tx_unregister(struct hvirq_dbl *dbl)
{
	return gh_dbl_tx_unregister(dbl->tx_dbl);
}

int habhyp_virq_rx_unregister(struct hvirq_dbl *dbl)
{
	return gh_dbl_rx_unregister(dbl->rx_dbl);
}
