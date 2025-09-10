// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "sc_tcm: " fmt

#include <linux/memory.h>
#include <linux/module.h>
#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/memremap.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#include "sc_tcm_internal.h"

struct sc_tcm_device {
	unsigned long base;
	unsigned long size;
	struct gen_pool *pool;
	struct mutex lock;
};

static struct sc_tcm_device *sc_tcm;

static int sc_tcm_add_mem(struct sc_tcm_device *sc_tcm)
{
	struct page **pages;
	u64 nr_pages, phys, base;
	int i, ret;

	base = sc_tcm->base;
	nr_pages = sc_tcm->size >> PAGE_SHIFT;
	pages = kvmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	for (i = 0, phys = base; i < nr_pages; ++i, phys += PAGE_SIZE)
		pages[i] = phys_to_page(phys);

	ret = __vmap_pages_range_noflush((unsigned long)phys_to_virt(base),
			((unsigned long)phys_to_virt(base)) + sc_tcm->size,
			PAGE_KERNEL, pages, PAGE_SHIFT);
	if (ret) {
		pr_err("Failed to linear map the sc tcm memory(%lx, %lx), ret: %d\n",
				base, sc_tcm->size, ret);
		kvfree(pages);
		return ret;
	}
	/**
	 * The pages array is only needed temporarily for the vmap operation.
	 * Once the mapping is established, the array is no longer needed as
	 * the kernel maintains the mapping internally.
	 */
	kvfree(pages);

	return 0;
}

void *sc_tcm_mem_alloc(u64 len)
{
	u64 paddr;

	/*
	 * Note: these checks are race free as we are registering
	 * a single SC TCM memory region.
	 */
	if (!sc_tcm)
		return ERR_PTR(-EINVAL);

	mutex_lock(&sc_tcm->lock);
	paddr = gen_pool_alloc(sc_tcm->pool, len);
	mutex_unlock(&sc_tcm->lock);

	if (!paddr)
		return ERR_PTR(-ENOMEM);

	return phys_to_virt(paddr);
}
EXPORT_SYMBOL_GPL(sc_tcm_mem_alloc);

void sc_tcm_mem_free(void *ptr, u64 len)
{
	phys_addr_t paddr;

	if (!ptr)
		return;
	if (!sc_tcm) {
		pr_err("sc-tcm memory is not registered\n");
		return;
	}

	paddr = virt_to_phys(ptr);

	mutex_lock(&sc_tcm->lock);
	gen_pool_free(sc_tcm->pool, paddr, len);
	mutex_unlock(&sc_tcm->lock);
}
EXPORT_SYMBOL_GPL(sc_tcm_mem_free);

static int register_sc_tcm_mem(u64 base, u64 size)
{
	int ret;

	if (sc_tcm) {
		pr_err("SC TCM  memory is already registered @base: %lx, size: %lx\n",
				sc_tcm->base, sc_tcm->size);
		return -EEXIST;
	}

	sc_tcm = kzalloc(sizeof(*sc_tcm), GFP_KERNEL);
	if (!sc_tcm)
		return -ENOMEM;

	sc_tcm->base = base;
	sc_tcm->size = size;
	ret = sc_tcm_add_mem(sc_tcm);
	if (ret)
		goto err_sc_tcm_add;

	sc_tcm->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!sc_tcm->pool) {
		ret = -ENOMEM;
		goto err_sc_tcm_add;
	}

	ret = gen_pool_add(sc_tcm->pool, base, size, -1);
	if (ret)
		goto err_gen_pool_add;

	mutex_init(&sc_tcm->lock);

	return 0;
err_gen_pool_add:
	gen_pool_destroy(sc_tcm->pool);
err_sc_tcm_add:
	kfree(sc_tcm);
	sc_tcm = NULL;
	return ret;
}

static int sc_tcm_region(phys_addr_t *base, size_t *size)
{
	struct device_node *node;
	struct device_node *mem_node;
	struct reserved_mem *rmem;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL, "qcom,seraph-llcc");
	if (!node)
		return -EINVAL;

	mem_node = of_parse_phandle(node, "memory-region", 0);
	if (!mem_node) {
		ret = -EINVAL;
		goto of_node_put;
	}

	rmem = of_reserved_mem_lookup(mem_node);
	if (!rmem) {
		ret = -EINVAL;
		goto mem_node_put;
	}

	*base = rmem->base;
	*size = rmem->size;
mem_node_put:
	of_node_put(mem_node);
of_node_put:
	of_node_put(node);
	return ret;

}

static int __init sc_tcm_module_init(void)
{
	int ret;
	phys_addr_t base;
	size_t size;

	ret = sc_tcm_region(&base, &size);
	if (ret) {
		pr_err("SC TCM region is not defined\n");
		return ret;
	}

	ret = register_sc_tcm_mem(base, size);
	if (ret) {
		pr_err("Failed to register the sc tcm memory\n");
		return ret;
	}

	pr_info("Registration of sc tcm memory(%lx, %lx) is successful\n", base, size);

	return 0;
}
subsys_initcall(sc_tcm_module_init);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. SC TCM Driver");
MODULE_LICENSE("GPL");
