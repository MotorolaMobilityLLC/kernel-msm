// SPDX-License-Identifier: GPL-2.0-only
/*
 * QTI TEE shared memory bridge driver
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "shmbridge: [%d]: " fmt, __LINE__

#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/firmware/qcom/qcom_tzmem.h>
#include <linux/qtee_shmbridge.h>

#include "qtee_shmbridge_internal.h"
#include "qcom_tzmem.h"

struct qcom_tzmem_pool *shmbridge_pool;

bool qtee_shmbridge_is_enabled(void)
{
	return qcom_tzmem_get_status();
}
EXPORT_SYMBOL(qtee_shmbridge_is_enabled);

/* Check whether a bridge starting from paddr exists */
int32_t qtee_shmbridge_query(phys_addr_t paddr)
{
	return qcom_tzmem_query(paddr);
}
EXPORT_SYMBOL(qtee_shmbridge_query);

/* Register paddr & size as a bridge, return bridge handle */
int32_t qtee_shmbridge_register(phys_addr_t paddr, size_t size, uint32_t *ns_vmid_list,
	uint32_t *ns_vm_perm_list, uint32_t ns_vmid_num, uint32_t tz_perm, uint64_t *handle)
{
	if (!handle || !ns_vmid_list || !ns_vm_perm_list) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	return qcom_tzmem_shm_bridge_create_with_vmid(paddr, size, ns_vmid_list[0], handle);

}
EXPORT_SYMBOL(qtee_shmbridge_register);

/* Deregister bridge */
int32_t qtee_shmbridge_deregister(uint64_t handle)
{
	qcom_tzmem_shm_bridge_delete(handle);
	return 0;
}
EXPORT_SYMBOL(qtee_shmbridge_deregister);


/* Sub-allocate from default kernel bridge created by shmb driver */
int32_t qtee_shmbridge_allocate_shm(size_t size, struct qtee_shm *shm)
{
	shm->size = PAGE_ALIGN(size);
	shm->vaddr = qcom_tzmem_alloc(shmbridge_pool, shm->size, GFP_KERNEL);
	if (!shm->vaddr) {
		pr_err("Failed to alloc memory from shmbridge pool, size: %#zx\n", shm->size);
		return -ENOMEM;
	}

	shm->paddr = qcom_tzmem_to_phys(shm->vaddr);

	pr_debug("shm->paddr: 0x%llx, size: %zu\n", (uint64_t)shm->paddr, shm->size);

	return 0;
}
EXPORT_SYMBOL(qtee_shmbridge_allocate_shm);


/* Free buffer that is sub-allocated from default kernel bridge */
void qtee_shmbridge_free_shm(struct qtee_shm *shm)
{
	if (IS_ERR_OR_NULL(shm) || !shm->vaddr)
		return;
	qcom_tzmem_free(shm->vaddr);
}
EXPORT_SYMBOL(qtee_shmbridge_free_shm);

/* cache clean operation for buffer sub-allocated from default bridge */
void qtee_shmbridge_flush_shm_buf(struct qtee_shm *shm)
{
	qcom_tzmem_flush_shm_buf(shm->paddr, shm->size);
}
EXPORT_SYMBOL(qtee_shmbridge_flush_shm_buf);

/* cache invalidation operation for buffer sub-allocated from default bridge */
void qtee_shmbridge_inv_shm_buf(struct qtee_shm *shm)
{
	qcom_tzmem_inv_shm_buf(shm->paddr, shm->size);
}
EXPORT_SYMBOL(qtee_shmbridge_inv_shm_buf);

int qtee_shmbridge_driver_init(void)
{
	struct qcom_tzmem_pool_config pool_config;

	memset(&pool_config, 0, sizeof(pool_config));
	pool_config.initial_size = SZ_1M;
	pool_config.policy = QCOM_TZMEM_POLICY_ON_DEMAND;
	pool_config.max_size = SZ_8M;
	pool_config.is_cached = true;

	shmbridge_pool = qcom_tzmem_pool_new(&pool_config);
	if (IS_ERR(shmbridge_pool)) {
		pr_err("Failed to create qcom_tzmem_pool %ld\n", PTR_ERR(shmbridge_pool));
		return PTR_ERR(shmbridge_pool);
	}

	pr_info("shmbridge pool of size: %#zx is created successfully\n",
		pool_config.initial_size);
	return 0;
}

void qtee_shmbridge_driver_exit(void)
{
	qcom_tzmem_pool_free(shmbridge_pool);
}
