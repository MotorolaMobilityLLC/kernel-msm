// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"io-pgtable-msm-secure: " fmt

#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/scatterlist.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/qcom_scm.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#include <linux/io-pgtable.h>

#define IOMMU_SECURE_PTBL_SIZE  3
#define IOMMU_SECURE_PTBL_INIT  4
#define IOMMU_SECURE_MAP2_FLAT 0x12
#define IOMMU_SECURE_UNMAP2_FLAT 0x13
#define IOMMU_TLBINVAL_FLAG 0x00000001

#define io_pgtable_to_data(x)						\
	container_of((x), struct msm_secure_io_pgtable, iop)

#define io_pgtable_ops_to_pgtable(x)					\
	container_of((x), struct io_pgtable, ops)

#define io_pgtable_ops_to_data(x)					\
	io_pgtable_to_data(io_pgtable_ops_to_pgtable(x))

struct msm_secure_io_pgtable {
	struct io_pgtable iop;
};

int msm_iommu_sec_pgtbl_init(void)
{
	size_t psize;
	unsigned int spare = 0;
	int ret;
	struct device dev = {0};
	void *cpu_addr;
	dma_addr_t paddr;
	unsigned long attrs = 0;

	ret = qcom_scm_iommu_secure_ptbl_size(spare, &psize);
	if (ret) {
		pr_err("scm call IOMMU_SECURE_PTBL_SIZE failed\n");
		return ret;
	}

	/* Now allocate memory for the secure page tables */
	attrs = DMA_ATTR_NO_KERNEL_MAPPING;
	dev.coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	arch_setup_dma_ops(&dev, 0, 0, NULL, 1);
	cpu_addr = dma_alloc_attrs(&dev, psize, &paddr, GFP_KERNEL, attrs);
	if (!cpu_addr) {
		pr_err("%s: Failed to allocate %d bytes for PTBL\n",
				__func__, psize);
		return -ENOMEM;
	}

	ret = qcom_scm_iommu_secure_ptbl_init(paddr, psize, spare);
	if (ret) {
		pr_err("scm call IOMMU_SECURE_PTBL_INIT failed\n");
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(msm_iommu_sec_pgtbl_init);

static int msm_secure_map(struct io_pgtable_ops *ops, unsigned long iova,
			phys_addr_t paddr, size_t size, int iommu_prot, gfp_t gfp)
{
	struct msm_secure_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	void *flush_va, *flush_va_end;
	int ret = -EINVAL;

	if (!IS_ALIGNED(iova, SZ_1M) || !IS_ALIGNED(paddr, SZ_1M) ||
			!IS_ALIGNED(size, SZ_1M))
		return -EINVAL;
	flush_va = &paddr;
	flush_va_end = (void *)
		(((unsigned long) flush_va) + sizeof(phys_addr_t));

	/*
	 * Ensure that the buffer is in RAM by the time it gets to TZ
	 */
	/*dmac_clean_range(flush_va, flush_va_end); fix this*/

	ret = qcom_scm_iommu_secure_map(virt_to_phys(&paddr), 1, size,
			cfg->arm_msm_secure_cfg.sec_id,
			cfg->arm_msm_secure_cfg.cbndx, iova, size);
	if (ret)
		return -EINVAL;

	return 0;
}

static dma_addr_t msm_secure_get_phys_addr(struct scatterlist *sg)
{
	/*
	 * Try sg_dma_address first so that we can
	 * map carveout regions that do not have a
	 * struct page associated with them.
	 */
	dma_addr_t pa = sg_dma_address(sg);

	if (pa == 0)
		pa = sg_phys(sg);
	return pa;
}

static int msm_secure_map_sg(struct io_pgtable_ops *ops, unsigned long iova,
			   struct scatterlist *sg, unsigned int nents,
			   int iommu_prot, gfp_t gfp, size_t *size)
{
	struct msm_secure_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	int ret = -EINVAL;
	struct scatterlist *tmp, *sgiter;
	dma_addr_t *pa_list = 0;
	unsigned int cnt, offset = 0, chunk_offset = 0;
	dma_addr_t pa;
	void *flush_va, *flush_va_end;
	unsigned long len = 0;
	u64 args[10] = {0};
	int i;

	for_each_sg(sg, tmp, nents, i)
		len += tmp->length;

	if (!IS_ALIGNED(iova, SZ_1M) || !IS_ALIGNED(len, SZ_1M))
		return -EINVAL;

	if (sg->length == len) {
		cnt = 1;
		pa = msm_secure_get_phys_addr(sg);
		if (!IS_ALIGNED(pa, SZ_1M))
			return -EINVAL;

		args[0] = virt_to_phys(&pa);
		args[1] = cnt;
		args[2] = len;
		flush_va = &pa;
	} else {
		sgiter = sg;
		if (!IS_ALIGNED(sgiter->length, SZ_1M))
			return -EINVAL;
		cnt = sg->length / SZ_1M;
		while ((sgiter = sg_next(sgiter))) {
			if (!IS_ALIGNED(sgiter->length, SZ_1M))
				return -EINVAL;
			cnt += sgiter->length / SZ_1M;
		}

		pa_list = kmalloc_array(cnt, sizeof(*pa_list), GFP_KERNEL);
		if (!pa_list)
			return -ENOMEM;

		sgiter = sg;
		cnt = 0;
		pa = msm_secure_get_phys_addr(sgiter);
		while (offset < len) {

			if (!IS_ALIGNED(pa, SZ_1M)) {
				kfree(pa_list);
				return -EINVAL;
			}

			pa_list[cnt] = pa + chunk_offset;
			chunk_offset += SZ_1M;
			offset += SZ_1M;
			cnt++;

			if (chunk_offset >= sgiter->length && offset < len) {
				chunk_offset = 0;
				sgiter = sg_next(sgiter);
				pa = msm_secure_get_phys_addr(sgiter);
			}
		}

		args[0] = virt_to_phys(pa_list);
		args[1] = cnt;
		args[2] = SZ_1M;
		flush_va = pa_list;
	}

	args[3] = cfg->arm_msm_secure_cfg.sec_id;
	args[4] = cfg->arm_msm_secure_cfg.cbndx;
	args[5] = iova;
	args[6] = len;
	args[7] = 0;

	/*
	 * Ensure that the buffer is in RAM by the time it gets to TZ
	 */

	flush_va_end = (void *) (((unsigned long) flush_va) +
			(cnt * sizeof(*pa_list)));
	/*dmac_clean_range(flush_va, flush_va_end); fix this*/

	ret = qcom_scm_iommu_secure_map(args[0], args[1], args[2], args[3], args[4],
					args[5], args[6]);
	if (ret)
		ret = -EINVAL;
	else {
		ret = len;
		*size = len;
	}
	kfree(pa_list);
	return ret;
}

static size_t msm_secure_unmap(struct io_pgtable_ops *ops, unsigned long iova,
			  size_t len, struct iommu_iotlb_gather *gather)
{
	struct msm_secure_io_pgtable *data = io_pgtable_ops_to_data(ops);
	struct io_pgtable_cfg *cfg = &data->iop.cfg;
	int ret = -EINVAL;
	u64 args[10] = {0};

	if (!IS_ALIGNED(iova, SZ_1M) || !IS_ALIGNED(len, SZ_1M))
		return ret;

	args[0] = cfg->arm_msm_secure_cfg.sec_id;
	args[1] = cfg->arm_msm_secure_cfg.cbndx;
	args[2] = iova;
	args[3] = len;

	ret = qcom_scm_iommu_secure_unmap(args[0], args[1], args[2], args[3]);
	if (!ret)
		ret = len;

	return ret;
}

static phys_addr_t msm_secure_iova_to_phys(struct io_pgtable_ops *ops,
					 unsigned long iova)
{
	return -EINVAL;
}

static struct msm_secure_io_pgtable *
msm_secure_alloc_pgtable_data(struct io_pgtable_cfg *cfg)
{
	struct msm_secure_io_pgtable *data;

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

	data->iop.ops = (struct io_pgtable_ops) {
		.map		= msm_secure_map,
		.map_sg		= msm_secure_map_sg,
		.unmap		= msm_secure_unmap,
		.iova_to_phys	= msm_secure_iova_to_phys,
	};

	return data;
}

static struct io_pgtable *
msm_secure_alloc_pgtable(struct io_pgtable_cfg *cfg, void *cookie)
{
	struct msm_secure_io_pgtable *data =
		msm_secure_alloc_pgtable_data(cfg);

	return &data->iop;
}

static void msm_secure_free_pgtable(struct io_pgtable *iop)
{
	struct msm_secure_io_pgtable *data = io_pgtable_to_data(iop);

	kfree(data);
}

struct io_pgtable_init_fns io_pgtable_arm_msm_secure_init_fns = {
	.alloc	= msm_secure_alloc_pgtable,
	.free	= msm_secure_free_pgtable,
};
