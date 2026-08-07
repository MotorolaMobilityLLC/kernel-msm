/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef MEM_BUF_GH_H
#define MEM_BUF_GH_H

#include <linux/dma-buf.h>
#include <linux/mem-buf.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <linux/mem-buf-exporter.h>
#include <soc/qcom/secure_buffer.h>
#include <uapi/linux/mem-buf.h>

#include "mem-buf-dev.h"

int mem_buf_acl_to_vmid_perms_list(unsigned int nr_acl_entries, const void __user *acl_entries,
				   int **dst_vmids, int **dst_perms);

#if IS_ENABLED(CONFIG_QCOM_MEM_BUF_GH)
#include <linux/gunyah/gh_rm_drv.h>

int mem_buf_alloc_fd(struct mem_buf_alloc_ioctl_arg *allocation_args);
int mem_buf_retrieve_user(struct mem_buf_retrieve_ioctl_arg *uarg);
int mem_buf_msgq_alloc(struct device *dev);
void mem_buf_msgq_free(struct device *dev);
#ifdef CONFIG_RECLAIM_LENT_MEMORY
void mem_buf_relinquish_all_mem(gh_vmid_t vmid);
u64 mem_buf_account_all_mem(void);
#endif /*CONFIG_RECLAIM_LENT_MEMORY*/
#else
static inline int mem_buf_alloc_fd(struct mem_buf_alloc_ioctl_arg *allocation_args)
{
	return -EOPNOTSUPP;
}

static inline int mem_buf_retrieve_user(struct mem_buf_retrieve_ioctl_arg *uarg)
{
	return -EOPNOTSUPP;
}

static inline int mem_buf_msgq_alloc(struct device *dev)
{
	return 0;
}

static inline void mem_buf_msgq_free(struct device *dev)
{
}

#ifdef CONFIG_RECLAIM_LENT_MEMORY
static inline void mem_buf_relinquish_all_mem(gh_vmid_t vmid)
{
}

static inline u64 mem_buf_account_all_mem(void)
{
	return 0;
}
#endif /*CONFIG_RECLAIM_LENT_MEMORY*/
#endif
#endif
