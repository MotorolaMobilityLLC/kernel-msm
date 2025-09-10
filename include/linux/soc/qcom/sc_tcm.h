/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _SOC_QCOM_SC_TCM_H_
#define _SOC_QCOM_SC_TCM_H_

#ifdef CONFIG_QCOM_SC_TCM_MEM_MODULE
/**
 * sc_tcm_mem_alloc - Allocate memory from SC TCM region
 * @len: Length of memory to allocate
 *
 * Returns pointer to allocated memory or ERR_PTR on failure.
 * Caller must check the return value with IS_ERR() before using.
 */
extern void *sc_tcm_mem_alloc(u64 len);

/**
 * sc_tcm_mem_free - Free previously allocated SC TCM memory
 * @ptr: Pointer returned by sc_tcm_mem_alloc
 * @len: Length of the allocation
 */
extern void sc_tcm_mem_free(void *ptr, u64 len);
#else /* !CONFIG_QCOM_SC_TCM_MEM */
static inline void *sc_tcm_mem_alloc(u64 len) { return NULL; }
static inline void sc_tcm_mem_free(void *ptr, u64 len) {}
#endif

#endif
