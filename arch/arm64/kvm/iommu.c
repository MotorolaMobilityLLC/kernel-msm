// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Google LLC
 * Author: Mostafa Saleh <smostafa@google.com>
 */

#include <asm/kvm_mmu.h>
#include <linux/cma.h>
#include <linux/of_reserved_mem.h>
#include <linux/kvm_host.h>
#include <kvm/iommu.h>

struct kvm_iommu_driver *iommu_driver;
extern struct kvm_iommu_ops *kvm_nvhe_sym(kvm_iommu_ops);

static struct cma *kvm_iommu_cma;
extern phys_addr_t kvm_nvhe_sym(cma_base);
extern size_t kvm_nvhe_sym(cma_size);

int kvm_iommu_register_driver(struct kvm_iommu_driver *kern_ops)
{
	BUG_ON(!kern_ops);

	/*
	 * Paired with smp_load_acquire(&iommu_driver)
	 * Ensure memory stores happening during a driver
	 * init are observed before executing kvm iommu callbacks.
	 */
	return cmpxchg_release(&iommu_driver, NULL, kern_ops) ? -EBUSY : 0;
}
EXPORT_SYMBOL(kvm_iommu_register_driver);

int kvm_iommu_init_hyp(struct kvm_iommu_ops *hyp_ops,
		       struct kvm_hyp_memcache *atomic_mc,
		       unsigned long init_arg)
{
	BUG_ON(!hyp_ops);

	return kvm_call_hyp_nvhe(__pkvm_iommu_init, hyp_ops,
				 atomic_mc->head, atomic_mc->nr_pages, init_arg);
}
EXPORT_SYMBOL(kvm_iommu_init_hyp);

static int __init pkvm_iommu_cma_setup(struct reserved_mem *rmem)
{
	int err;

	if (!IS_ALIGNED(rmem->base | rmem->size, PMD_SIZE))
		kvm_info("pKVM IOMMU reserved memory not PMD-aligned\n");

	err = cma_init_reserved_mem(rmem->base, rmem->size, 0, rmem->name,
				    &kvm_iommu_cma);
	if (err) {
		kvm_err("Failed to init pKVM IOMMU reserved memory\n");
		kvm_iommu_cma = NULL;
		return err;
	}

	kvm_nvhe_sym(cma_base) = cma_get_base(kvm_iommu_cma);
	kvm_nvhe_sym(cma_size) = cma_get_size(kvm_iommu_cma);

	return 0;
}
RESERVEDMEM_OF_DECLARE(pkvm_cma, "pkvm,cma", pkvm_iommu_cma_setup);

static const u8 pmd_order = PMD_SHIFT - PAGE_SHIFT;

struct page *kvm_iommu_cma_alloc(void)
{
	if (!kvm_iommu_cma)
		return NULL;

	return cma_alloc(kvm_iommu_cma, (1 << pmd_order), pmd_order, true);
}
EXPORT_SYMBOL(kvm_iommu_cma_alloc);

bool kvm_iommu_cma_release(struct page *p)
{
	if (!kvm_iommu_cma || !p)
		return false;

	return cma_release(kvm_iommu_cma, p, 1 << pmd_order);
}
EXPORT_SYMBOL(kvm_iommu_cma_release);

int kvm_iommu_init_driver(void)
{
	if (!smp_load_acquire(&iommu_driver) || !iommu_driver->get_iommu_id) {
		kvm_err("pKVM enabled with no IOMMU driver, do not run confidential" \
			 " workloads in virtual machines\n");
		return -ENODEV;
	}

	/*
	 * init_driver is optional as the driver already registered it self.
	 * This call mainly notify the driver we are about to drop privilege.
	 */
	if (!iommu_driver->init_driver)
		return 0;
	kvm_hyp_iommu_domains = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
				get_order(KVM_IOMMU_DOMAINS_ROOT_SIZE));
	kvm_hyp_iommu_domains = kern_hyp_va(kvm_hyp_iommu_domains);
	if (!kvm_hyp_iommu_domains) {
		kvm_err("No enough mem for IOMMU domains");
		return -ENOMEM;
	}
	return iommu_driver->init_driver();
}

void kvm_iommu_remove_driver(void)
{
	if (smp_load_acquire(&iommu_driver))
		iommu_driver->remove_driver();
}

pkvm_handle_t kvm_get_iommu_id(struct device *dev)
{
	return iommu_driver->get_iommu_id(dev);
}

int pkvm_iommu_suspend(struct device *dev)
{
	int device_id = kvm_get_iommu_id(dev);

	return kvm_call_hyp_nvhe(__pkvm_host_hvc_pd, device_id, 0);
}
EXPORT_SYMBOL(pkvm_iommu_suspend);

int pkvm_iommu_resume(struct device *dev)
{
	int device_id = kvm_get_iommu_id(dev);

	return kvm_call_hyp_nvhe(__pkvm_host_hvc_pd, device_id, 1);
}
EXPORT_SYMBOL(pkvm_iommu_resume);
