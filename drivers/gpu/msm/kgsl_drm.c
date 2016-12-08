/* Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Implements an interface between KGSL and the DRM subsystem.  For now this
 * is pretty simple, but it will take on more of the workload as time goes
 * on
 */
#include "drmP.h"
#include "drm.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm_ion.h>
#ifdef CONFIG_GENLOCK
#include <linux/genlock.h>
#endif
#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_drm.h"
#include "kgsl_mmu.h"
#include "kgsl_sharedmem.h"

#define DRIVER_AUTHOR           "Qualcomm"
#define DRIVER_NAME             "kgsl"
#define DRIVER_DESC             "KGSL DRM"
#define DRIVER_DATE             "20121107"

#define DRIVER_MAJOR            2
#define DRIVER_MINOR            1
#define DRIVER_PATCHLEVEL       1

#define DRM_KGSL_GEM_FLAG_MAPPED (1 << 0)

#define DRM_KGSL_NOT_INITED -1
#define DRM_KGSL_INITED   1

/* Returns true if the memory type is in PMEM */

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
#define TYPE_IS_PMEM(_t) \
  (((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_EBI) || \
   ((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_SMI) || \
   ((_t) & DRM_KGSL_GEM_TYPE_PMEM))
#else
#define TYPE_IS_PMEM(_t) \
  (((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_EBI) || \
   ((_t) & (DRM_KGSL_GEM_TYPE_PMEM | DRM_KGSL_GEM_PMEM_EBI)))
#endif

/* Returns true if the memory type is regular */

#define TYPE_IS_MEM(_t) \
  (((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_KMEM) || \
   ((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_KMEM_NOCACHE) || \
   ((_t) & DRM_KGSL_GEM_TYPE_MEM))

#define TYPE_IS_FD(_t) ((_t) & DRM_KGSL_GEM_TYPE_FD_MASK)

/* Returns true if KMEM region is uncached */

#define IS_MEM_UNCACHED(_t) \
	(((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_KMEM_NOCACHE))

extern u32 mdp_drm_intr_status;
extern u32 mdp_drm_intr_mask;

/* Returns true if memory type is secure */

#define TYPE_IS_SECURE(_t) \
	((_t & DRM_KGSL_GEM_TYPE_MEM_MASK) == DRM_KGSL_GEM_TYPE_MEM_SECURE)

enum MDSS_MDP_REG {
	MDSS_MDP_REG_INTR_EN,
	MDSS_MDP_REG_INTR_STATUS,
	MDSS_MDP_REG_INTR_CLEAR,
};

struct drm_kgsl_private {
	struct drm_device *drm_dev;
	void __iomem *regs;
	size_t reg_size;
	unsigned int irq;
	atomic_t vblank_cnt[DRM_KGSL_CRTC_MAX];
        u32 vsync_irq;
	u32 mdp_reg[3];
	u32 irq_mask[DRM_KGSL_CRTC_MAX];
	bool fake_vbl;
	struct work_struct fake_vbl_work;
	struct mutex fake_vbl_lock;
};

struct drm_kgsl_gem_object {
	struct drm_gem_object *obj;
	uint32_t type;
	struct kgsl_memdesc memdesc;
	struct kgsl_pagetable *pagetable;
	struct ion_handle *ion_handle;
	uint64_t mmap_offset;
	int bufcount;
	int flags;
	struct list_head list;
	int active;

	struct {
		uint32_t offset;
		uint32_t gpuaddr;
	} bufs[DRM_KGSL_GEM_MAX_BUFFERS];

#ifdef CONFIG_GENLOCK
	struct genlock_handle *glock_handle[DRM_KGSL_GEM_MAX_BUFFERS];
#endif
	int bound;
	int lockpid;

	/*
	 * Userdata to indicate
	 * Last operation done READ/WRITE
	 * Last user of this memory CPU/GPU
	 */
	uint32_t user_pdata;

};

struct drm_kgsl_file_private {
	pid_t tgid;
};

static struct ion_client *kgsl_drm_ion_client;

static int kgsl_drm_inited = DRM_KGSL_NOT_INITED;

/* This is a global list of all the memory currently mapped in the MMU */
static struct list_head kgsl_mem_list;

struct kgsl_drm_device_priv {
	struct kgsl_device *device[KGSL_DEVICE_MAX];
	struct kgsl_device_private *devpriv[KGSL_DEVICE_MAX];
};

struct kgsl_drm_gem_info_data {
	struct drm_file *filp;
	struct seq_file *m;
};

static int
kgsl_gem_memory_allocated(struct drm_gem_object *obj)
{
	struct drm_kgsl_gem_object *priv = obj->driver_private;
	return priv->memdesc.size ? 1 : 0;
}

static int
kgsl_gem_alloc_memory(struct drm_gem_object *obj)
{
	struct drm_kgsl_gem_object *priv = obj->driver_private;
	struct kgsl_mmu *mmu;
	struct sg_table *sg_table;
	struct scatterlist *s;
	int index;
	int result = 0;
	int mem_flags = 0;

	/* Return if the memory is already allocated */

	if (kgsl_gem_memory_allocated(obj) || TYPE_IS_FD(priv->type))
		return 0;

	if (priv->pagetable == NULL) {
		mmu = &kgsl_get_device(KGSL_DEVICE_3D0)->mmu;

		priv->pagetable = kgsl_mmu_getpagetable(mmu,
					KGSL_MMU_GLOBAL_PT);

		if (priv->pagetable == NULL) {
			DRM_ERROR("Unable to get the GPU MMU pagetable\n");
			return -EINVAL;
		}
	}

	if (TYPE_IS_PMEM(priv->type)) {
		if (priv->type == DRM_KGSL_GEM_TYPE_EBI ||
		    priv->type & DRM_KGSL_GEM_PMEM_EBI) {
			priv->ion_handle = ion_alloc(kgsl_drm_ion_client,
				obj->size * priv->bufcount, PAGE_SIZE,
				ION_HEAP(ION_SF_HEAP_ID), 0);
			if (IS_ERR_OR_NULL(priv->ion_handle)) {
				DRM_ERROR(
				"Unable to allocate ION Phys memory handle\n");
				return -ENOMEM;
			}

			priv->memdesc.pagetable = priv->pagetable;

			result = ion_phys(kgsl_drm_ion_client,
				priv->ion_handle, (ion_phys_addr_t *)
				&priv->memdesc.physaddr, &priv->memdesc.size);
			if (result) {
				DRM_ERROR(
				"Unable to get ION Physical memory address\n");
				ion_free(kgsl_drm_ion_client,
					priv->ion_handle);
				priv->ion_handle = NULL;
				return result;
			}

			result = memdesc_sg_phys(&priv->memdesc,
				priv->memdesc.physaddr, priv->memdesc.size);
			if (result) {
				DRM_ERROR(
				"Unable to get sg list\n");
				ion_free(kgsl_drm_ion_client,
					priv->ion_handle);
				priv->ion_handle = NULL;
				return result;
			}

			result = kgsl_mmu_get_gpuaddr(priv->pagetable,
							&priv->memdesc);
			if (result) {
				DRM_ERROR(
				"kgsl_mmu_get_gpuaddr failed. result = %d\n",
				result);
				ion_free(kgsl_drm_ion_client,
					priv->ion_handle);
				priv->ion_handle = NULL;
				return result;
			}
			result = kgsl_mmu_map(priv->pagetable, &priv->memdesc);
			if (result) {
				DRM_ERROR(
				"kgsl_mmu_map failed.  result = %d\n", result);
				kgsl_mmu_put_gpuaddr(priv->pagetable,
							&priv->memdesc);
				ion_free(kgsl_drm_ion_client,
					priv->ion_handle);
				priv->ion_handle = NULL;
				return result;
			}
		}
		else
			return -EINVAL;

	} else if (TYPE_IS_MEM(priv->type)) {

		if (priv->type == DRM_KGSL_GEM_TYPE_KMEM ||
			priv->type & DRM_KGSL_GEM_CACHE_MASK)
				list_add(&priv->list, &kgsl_mem_list);

		priv->memdesc.pagetable = priv->pagetable;

		if (!IS_MEM_UNCACHED(priv->type))
			mem_flags = ION_FLAG_CACHED;

		priv->ion_handle = ion_alloc(kgsl_drm_ion_client,
				obj->size * priv->bufcount, PAGE_SIZE,
				ION_HEAP(ION_IOMMU_HEAP_ID), mem_flags);

		if (IS_ERR_OR_NULL(priv->ion_handle)) {
			DRM_ERROR(
				"Unable to allocate ION IOMMU memory handle\n");
				return -ENOMEM;
		}

		sg_table = ion_sg_table(kgsl_drm_ion_client,
				priv->ion_handle);
		if (IS_ERR_OR_NULL(priv->ion_handle)) {
			DRM_ERROR(
			"Unable to get ION sg table\n");
			goto memerr;
		}

		priv->memdesc.sg = sg_table->sgl;

		/* Calculate the size of the memdesc from the sglist */

		priv->memdesc.sglen = 0;

		for (s = priv->memdesc.sg; s != NULL; s = sg_next(s)) {
			priv->memdesc.size += s->length;
			priv->memdesc.sglen++;
		}

		result = kgsl_mmu_get_gpuaddr(priv->pagetable, &priv->memdesc);
		if (result) {
			DRM_ERROR(
			"kgsl_mmu_get_gpuaddr failed.  result = %d\n", result);
			goto memerr;
		}
		result = kgsl_mmu_map(priv->pagetable, &priv->memdesc);
		if (result) {
			DRM_ERROR(
			"kgsl_mmu_map failed.  result = %d\n", result);
			kgsl_mmu_put_gpuaddr(priv->pagetable, &priv->memdesc);
			goto memerr;
		}
	} else if (TYPE_IS_SECURE(priv->type)) {
		priv->memdesc.pagetable = priv->pagetable;
		priv->ion_handle = ion_alloc(kgsl_drm_ion_client,
				obj->size * priv->bufcount, PAGE_SIZE,
				ION_HEAP(ION_CP_MM_HEAP_ID), ION_FLAG_SECURE);
		if (IS_ERR_OR_NULL(priv->ion_handle)) {
			DRM_ERROR("Unable to allocate ION_SECURE memory\n");
			return -ENOMEM;
		}
		sg_table = ion_sg_table(kgsl_drm_ion_client,
				priv->ion_handle);
		if (IS_ERR_OR_NULL(priv->ion_handle)) {
			DRM_ERROR("Unable to get ION sg table\n");
			goto memerr;
		}
		priv->memdesc.sg = sg_table->sgl;
		priv->memdesc.sglen = 0;
		for (s = priv->memdesc.sg; s != NULL; s = sg_next(s)) {
			priv->memdesc.size += s->length;
			priv->memdesc.sglen++;
		}
		/* Skip GPU map for secure buffer */
	} else
		return -EINVAL;

	for (index = 0; index < priv->bufcount; index++) {
		priv->bufs[index].offset = index * obj->size;
		priv->bufs[index].gpuaddr =
			priv->memdesc.gpuaddr +
			priv->bufs[index].offset;
	}
	priv->flags |= DRM_KGSL_GEM_FLAG_MAPPED;


	return 0;

memerr:
	ion_free(kgsl_drm_ion_client,
		priv->ion_handle);
	priv->ion_handle = NULL;
	return -ENOMEM;

}

static void
kgsl_gem_free_memory(struct drm_gem_object *obj)
{
	struct drm_kgsl_gem_object *priv = obj->driver_private;
#ifdef CONFIG_GENLOCK
	int index;
#endif
	if (!kgsl_gem_memory_allocated(obj) || TYPE_IS_FD(priv->type))
		return;

	if (priv->memdesc.gpuaddr) {
		kgsl_mmu_unmap(priv->memdesc.pagetable, &priv->memdesc);
		kgsl_mmu_put_gpuaddr(priv->memdesc.pagetable, &priv->memdesc);
	}

	/* ION will take care of freeing the sg table. */
	priv->memdesc.sg = NULL;
	priv->memdesc.sglen = 0;

	if (priv->ion_handle)
		ion_free(kgsl_drm_ion_client, priv->ion_handle);

	priv->ion_handle = NULL;

	memset(&priv->memdesc, 0, sizeof(priv->memdesc));

#ifdef CONFIG_GENLOCK
	for (index = 0; index < priv->bufcount; index++) {
		if (priv->glock_handle[index])
			genlock_put_handle(priv->glock_handle[index]);
	}
#endif

	kgsl_mmu_putpagetable(priv->pagetable);
	priv->pagetable = NULL;

	if ((priv->type == DRM_KGSL_GEM_TYPE_KMEM) ||
	    (priv->type & DRM_KGSL_GEM_CACHE_MASK))
		list_del(&priv->list);

	priv->flags &= ~DRM_KGSL_GEM_FLAG_MAPPED;

}

int
kgsl_gem_init_object(struct drm_gem_object *obj)
{
	struct drm_kgsl_gem_object *priv;
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		DRM_ERROR("Unable to create GEM object\n");
		return -ENOMEM;
	}

	obj->driver_private = priv;
	priv->obj = obj;

	return 0;
}

void
kgsl_gem_free_object(struct drm_gem_object *obj)
{
	kgsl_gem_free_memory(obj);
	drm_gem_object_release(obj);
	kfree(obj->driver_private);
	kfree(obj);
}

int
kgsl_gem_obj_addr(int drm_fd, int handle, unsigned long *start,
			unsigned long *len)
{
	struct file *filp;
	struct drm_device *dev;
	struct drm_file *file_priv;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = 0;

	filp = fget(drm_fd);
	if (unlikely(filp == NULL)) {
		DRM_ERROR("Unable to get the DRM file descriptor\n");
		return -EINVAL;
	}
	file_priv = filp->private_data;
	if (unlikely(file_priv == NULL)) {
		DRM_ERROR("Unable to get the file private data\n");
		fput(filp);
		return -EINVAL;
	}
	dev = file_priv->minor->dev;
	if (unlikely(dev == NULL)) {
		DRM_ERROR("Unable to get the minor device\n");
		fput(filp);
		return -EINVAL;
	}

	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (unlikely(obj == NULL)) {
		DRM_ERROR("Invalid GEM handle %x\n", handle);
		fput(filp);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	/* We can only use the MDP for PMEM regions */

	if (TYPE_IS_PMEM(priv->type)) {
		*start = priv->memdesc.physaddr +
			priv->bufs[priv->active].offset;

		*len = priv->memdesc.size;
	} else {
		*start = 0;
		*len = 0;
		ret = -EINVAL;
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	fput(filp);
	return ret;
}

static int
kgsl_gem_init_obj(struct drm_device *dev,
		  struct drm_file *file_priv,
		  struct drm_gem_object *obj,
		  int *handle)
{
	struct drm_kgsl_gem_object *priv;
	int ret;

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	memset(&priv->memdesc, 0, sizeof(priv->memdesc));
	priv->bufcount = 1;
	priv->active = 0;
	priv->bound = 0;

	priv->type = DRM_KGSL_GEM_TYPE_KMEM;

	ret = drm_gem_handle_create(file_priv, obj, handle);

	drm_gem_object_unreference(obj);

	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int
kgsl_gem_create_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_kgsl_gem_create *create = data;
	struct drm_gem_object *obj;
	int ret, handle;

	/* Page align the size so we can allocate multiple buffers */
	create->size = ALIGN(create->size, 4096);

	obj = drm_gem_object_alloc(dev, create->size);

	if (obj == NULL) {
		DRM_ERROR("Unable to allocate the GEM object\n");
		return -ENOMEM;
	}

	ret = kgsl_gem_init_obj(dev, file_priv, obj, &handle);
	if (ret) {
		drm_gem_object_release(obj);
		kfree(obj->driver_private);
		kfree(obj);
		DRM_ERROR("Unable to initialize GEM object ret = %d\n", ret);
		return ret;
	}

	create->handle = handle;
	return 0;
}

int
kgsl_gem_create_fd_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_kgsl_gem_create_fd *args = data;
	struct file *file;
	dev_t rdev;
	struct fb_info *info;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret, put_needed, handle;

	file = fget_light(args->fd, &put_needed);

	if (file == NULL) {
		DRM_ERROR("Unable to get the file object\n");
		return -EBADF;
	}

	rdev = file->f_dentry->d_inode->i_rdev;

	/* Only framebuffer objects are supported ATM */

	if (MAJOR(rdev) != FB_MAJOR) {
		DRM_ERROR("File descriptor is not a framebuffer\n");
		ret = -EBADF;
		goto error_fput;
	}

	info = registered_fb[MINOR(rdev)];

	if (info == NULL) {
		DRM_ERROR("Framebuffer minor %d is not registered\n",
			  MINOR(rdev));
		ret = -EBADF;
		goto error_fput;
	}

	obj = drm_gem_object_alloc(dev, info->fix.smem_len);

	if (obj == NULL) {
		DRM_ERROR("Unable to allocate GEM object\n");
		ret = -ENOMEM;
		goto error_fput;
	}

	ret = kgsl_gem_init_obj(dev, file_priv, obj, &handle);

	if (ret) {
		drm_gem_object_release(obj);
		kfree(obj->driver_private);
		kfree(obj);
		goto error_fput;
	}

	mutex_lock(&dev->struct_mutex);

	priv = obj->driver_private;
	priv->memdesc.physaddr = info->fix.smem_start;
	priv->type = DRM_KGSL_GEM_TYPE_FD_FBMEM;

	mutex_unlock(&dev->struct_mutex);
	args->handle = handle;

error_fput:
	fput_light(file, put_needed);

	return ret;
}

int
kgsl_gem_create_from_ion_ioctl(struct drm_device *dev, void *data,
		      struct drm_file *file_priv)
{
	struct drm_kgsl_gem_create_from_ion *args = data;
	struct drm_gem_object *obj;
	struct ion_handle *ion_handle;
	struct drm_kgsl_gem_object *priv;
	struct sg_table *sg_table;
	struct scatterlist *s;
	int ret, handle;
	unsigned long size;
	struct kgsl_mmu *mmu;

	ion_handle = ion_import_dma_buf(kgsl_drm_ion_client, args->ion_fd);
	if (IS_ERR_OR_NULL(ion_handle)) {
		DRM_ERROR("Unable to import dmabuf.  Error number = %d\n",
			(int)PTR_ERR(ion_handle));
		return -EINVAL;
	}

	ion_handle_get_size(kgsl_drm_ion_client, ion_handle, &size);

	if (size == 0) {
		ion_free(kgsl_drm_ion_client, ion_handle);
		DRM_ERROR(
		"cannot create GEM object from zero size ION buffer\n");
		return -EINVAL;
	}

	obj = drm_gem_object_alloc(dev, size);

	if (obj == NULL) {
		ion_free(kgsl_drm_ion_client, ion_handle);
		DRM_ERROR("Unable to allocate the GEM object\n");
		return -ENOMEM;
	}

	ret = kgsl_gem_init_obj(dev, file_priv, obj, &handle);
	if (ret) {
		ion_free(kgsl_drm_ion_client, ion_handle);
		drm_gem_object_release(obj);
		kfree(obj->driver_private);
		kfree(obj);
		DRM_ERROR("Unable to initialize GEM object ret = %d\n", ret);
		return ret;
	}

	priv = obj->driver_private;
	priv->ion_handle = ion_handle;

	priv->type = DRM_KGSL_GEM_TYPE_KMEM;
	list_add(&priv->list, &kgsl_mem_list);

	mmu = &kgsl_get_device(KGSL_DEVICE_3D0)->mmu;

	priv->pagetable = kgsl_mmu_getpagetable(mmu, KGSL_MMU_GLOBAL_PT);

	priv->memdesc.pagetable = priv->pagetable;

	sg_table = ion_sg_table(kgsl_drm_ion_client,
		priv->ion_handle);
	if (IS_ERR_OR_NULL(priv->ion_handle)) {
		DRM_ERROR("Unable to get ION sg table\n");
		ion_free(kgsl_drm_ion_client,
			priv->ion_handle);
		priv->ion_handle = NULL;
		kgsl_mmu_putpagetable(priv->pagetable);
		drm_gem_object_release(obj);
		kfree(priv);
		kfree(obj);
		return -ENOMEM;
	}

	priv->memdesc.sg = sg_table->sgl;

	/* Calculate the size of the memdesc from the sglist */

	priv->memdesc.sglen = 0;

	for (s = priv->memdesc.sg; s != NULL; s = sg_next(s)) {
		priv->memdesc.size += s->length;
		priv->memdesc.sglen++;
	}

	ret = kgsl_mmu_get_gpuaddr(priv->pagetable, &priv->memdesc);
	if (ret) {
		DRM_ERROR("kgsl_mmu_get_gpuaddr failed.  ret = %d\n", ret);
		ion_free(kgsl_drm_ion_client,
			priv->ion_handle);
		priv->ion_handle = NULL;
		kgsl_mmu_putpagetable(priv->pagetable);
		drm_gem_object_release(obj);
		kfree(priv);
		kfree(obj);
		return -ENOMEM;
	}
	ret = kgsl_mmu_map(priv->pagetable, &priv->memdesc);
	if (ret) {
		DRM_ERROR("kgsl_mmu_map failed.  ret = %d\n", ret);
		kgsl_mmu_put_gpuaddr(priv->pagetable, &priv->memdesc);
		ion_free(kgsl_drm_ion_client,
			priv->ion_handle);
		priv->ion_handle = NULL;
		kgsl_mmu_putpagetable(priv->pagetable);
		drm_gem_object_release(obj);
		kfree(priv);
		kfree(obj);
		return -ENOMEM;
	}

	priv->bufs[0].offset = 0;
	priv->bufs[0].gpuaddr = priv->memdesc.gpuaddr;
	priv->flags |= DRM_KGSL_GEM_FLAG_MAPPED;

	args->handle = handle;
	return 0;
}

int
kgsl_gem_get_ion_fd_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_kgsl_gem_get_ion_fd *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	if (TYPE_IS_FD(priv->type))
		ret = -EINVAL;
	else if (TYPE_IS_PMEM(priv->type) || TYPE_IS_MEM(priv->type) ||
			TYPE_IS_SECURE(priv->type)) {
		if (priv->ion_handle) {
			args->ion_fd = ion_share_dma_buf_fd(
				kgsl_drm_ion_client, priv->ion_handle);
			if (args->ion_fd < 0) {
				DRM_ERROR(
				"Could not share ion buffer. Error = %d\n",
					args->ion_fd);
				ret = -EINVAL;
			}
		} else {
			DRM_ERROR("GEM object has no ion memory allocated.\n");
			ret = -EINVAL;
		}
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
kgsl_gem_setmemtype_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_kgsl_gem_memtype *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	if (TYPE_IS_FD(priv->type))
		ret = -EINVAL;
	else {
		if (TYPE_IS_PMEM(args->type) || TYPE_IS_MEM(args->type) ||
			TYPE_IS_SECURE(args->type))
			priv->type = args->type;
		else
			ret = -EINVAL;
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
kgsl_gem_getmemtype_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_memtype *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	args->type = priv->type;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int
kgsl_gem_unbind_gpu_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	return 0;
}

int
kgsl_gem_bind_gpu_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	return 0;
}

/* Allocate the memory and prepare it for CPU mapping */

int
kgsl_gem_alloc_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_kgsl_gem_alloc *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	ret = kgsl_gem_alloc_memory(obj);

	if (ret) {
		DRM_ERROR("Unable to allocate object memory\n");
	}

	args->offset = 0;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
kgsl_gem_mmap_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	/* Ion is used for mmap at this time */
	return 0;
}

/* This function is deprecated */

int
kgsl_gem_prep_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	struct drm_kgsl_gem_prep *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	ret = kgsl_gem_alloc_memory(obj);
	if (ret) {
		DRM_ERROR("Unable to allocate object memory\n");
		drm_gem_object_unreference(obj);
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int
kgsl_gem_get_bufinfo_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_kgsl_gem_bufinfo *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = -EINVAL;
	int index;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	if (!kgsl_gem_memory_allocated(obj)) {
		DRM_ERROR("Memory not allocated for this object\n");
		goto out;
	}

	for (index = 0; index < priv->bufcount; index++) {
		args->offset[index] = priv->bufs[index].offset;
		args->gpuaddr[index] = priv->bufs[index].gpuaddr;
	}

	args->count = priv->bufcount;
	args->active = priv->active;

	ret = 0;

out:
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

#ifdef CONFIG_GENLOCK
/* Get the genlock handles base off the GEM handle
 */

int
kgsl_gem_get_glock_handles_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv)
{
	struct drm_kgsl_gem_glockinfo *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int index;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	for (index = 0; index < priv->bufcount; index++) {
		args->glockhandle[index] = genlock_get_fd_handle(
						priv->glock_handle[index]);
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);
	return 0;
}

int
kgsl_gem_set_glock_handles_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv)
{
	struct drm_kgsl_gem_glockinfo *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int index;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	for (index = 0; index < priv->bufcount; index++) {
		priv->glock_handle[index] = genlock_get_handle_fd(
						args->glockhandle[index]);
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}
#else
int
kgsl_gem_get_glock_handles_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv)
{
	return -EINVAL;
}

int
kgsl_gem_set_glock_handles_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file_priv)
{
	return -EINVAL;
}
#endif

int
kgsl_gem_set_bufcount_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_bufcount *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = -EINVAL;

	if (args->bufcount < 1 || args->bufcount > DRM_KGSL_GEM_MAX_BUFFERS)
		return -EINVAL;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	/* It is too much math to worry about what happens if we are already
	   allocated, so just bail if we are */

	if (kgsl_gem_memory_allocated(obj)) {
		DRM_ERROR("Memory already allocated - cannot change"
			  "number of buffers\n");
		goto out;
	}

	priv->bufcount = args->bufcount;
	ret = 0;

out:
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
kgsl_gem_get_bufcount_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_bufcount *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	args->bufcount =  priv->bufcount;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int kgsl_gem_set_userdata(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_userdata *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	priv->user_pdata =  args->priv_data;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int kgsl_gem_get_userdata(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_userdata *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	args->priv_data =  priv->user_pdata;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int kgsl_gem_cache_ops(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_cache_ops *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	unsigned int cache_op = 0;
	int ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);

	if (!kgsl_gem_memory_allocated(obj)) {
		DRM_ERROR("Object isn't allocated,can't perform Cache ops.\n");
		ret = -EPERM;
		goto done;
	}

	priv = obj->driver_private;

	if (IS_MEM_UNCACHED(priv->type))
		goto done;

	switch (args->flags) {
	case DRM_KGSL_GEM_CLEAN_CACHES:
		cache_op = ION_IOC_CLEAN_CACHES;
		break;
	case DRM_KGSL_GEM_INV_CACHES:
		cache_op = ION_IOC_INV_CACHES;
		break;
	case DRM_KGSL_GEM_CLEAN_INV_CACHES:
		cache_op = ION_IOC_CLEAN_INV_CACHES;
		break;
	default:
		DRM_ERROR("Invalid Cache operation\n");
		ret = -EINVAL;
		goto done;
	}

	if ((obj->size != args->length)) {
		DRM_ERROR("Invalid buffer size ");
		ret = -EINVAL;
		goto done;
	}

	ret = msm_ion_do_cache_op(kgsl_drm_ion_client, priv->ion_handle,
			args->vaddr, args->length, cache_op);

	if (ret)
		DRM_ERROR("Cache operation failed\n");

done:
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
kgsl_gem_set_active_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_kgsl_gem_active *args = data;
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = -EINVAL;

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", args->handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	if (args->active < 0 || args->active >= priv->bufcount) {
		DRM_ERROR("Invalid active buffer %d\n", args->active);
		goto out;
	}

	priv->active = args->active;
	ret = 0;

out:
	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int kgsl_gem_kmem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;
	struct drm_kgsl_gem_object *priv;
	unsigned long offset;
	struct page *page;
	int i;

	mutex_lock(&dev->struct_mutex);

	priv = obj->driver_private;

	offset = (unsigned long) vmf->virtual_address - vma->vm_start;
	i = offset >> PAGE_SHIFT;
	page = sg_page(&(priv->memdesc.sg[i]));

	if (!page) {
		mutex_unlock(&dev->struct_mutex);
		return VM_FAULT_SIGBUS;
	}

	get_page(page);
	vmf->page = page;

	mutex_unlock(&dev->struct_mutex);
	return 0;
}

int kgsl_gem_phys_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;
	struct drm_kgsl_gem_object *priv;
	unsigned long offset, pfn;
	int ret = 0;

	offset = ((unsigned long) vmf->virtual_address - vma->vm_start) >>
		PAGE_SHIFT;

	mutex_lock(&dev->struct_mutex);

	priv = obj->driver_private;

	pfn = (priv->memdesc.physaddr >> PAGE_SHIFT) + offset;
	ret = vm_insert_pfn(vma,
			    (unsigned long) vmf->virtual_address, pfn);
	mutex_unlock(&dev->struct_mutex);

	switch (ret) {
	case -ENOMEM:
	case -EAGAIN:
		return VM_FAULT_OOM;
	case -EFAULT:
		return VM_FAULT_SIGBUS;
	default:
		return VM_FAULT_NOPAGE;
	}
}

static u32
kgsl_drm_get_vblank_counter(struct drm_device *dev, int crtc)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;

	DRM_DEBUG("%s:crtc[%d]\n", __func__, crtc);

	if (crtc >= DRM_KGSL_CRTC_MAX) {
		DRM_ERROR("failed to get vblank counter, \
				CRTC %d not supported\n", crtc);
		return -EINVAL;
	}

	return atomic_read(&dev_priv->vblank_cnt[crtc]);
}

static int
kgsl_drm_enable_vblank(struct drm_device *dev, int crtc)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;

	DRM_DEBUG("%s:crtc[%d]\n", __func__, crtc);

	if (crtc >= DRM_KGSL_CRTC_MAX) {
		DRM_ERROR("failed to disable vblank, \
				CRTC %d not supported\n", crtc);
		return -EINVAL;
	}

	switch (crtc) {
	case DRM_KGSL_CRTC_FAKE:
		mutex_lock(&dev_priv->fake_vbl_lock);
		dev_priv->fake_vbl = true;
		schedule_work(&dev_priv->fake_vbl_work);
		mutex_unlock(&dev_priv->fake_vbl_lock);
		break;
	default:
		break;
	}

	return 0;
}

static void
kgsl_drm_disable_vblank(struct drm_device *dev, int crtc)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;

	DRM_DEBUG("%s:crtc[%d]\n", __func__, crtc);

	if (crtc != 0)
		DRM_ERROR("failed to disable vblank.\n");

	switch (crtc) {
	case DRM_KGSL_CRTC_FAKE:
		mutex_lock(&dev_priv->fake_vbl_lock);
		dev_priv->fake_vbl = false;
		mutex_unlock(&dev_priv->fake_vbl_lock);
		break;
	default:
		break;
	}
}

static irqreturn_t
kgsl_drm_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *)arg;
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;
	u32 isr = mdp_drm_intr_status;
	int i=0;

	DRM_DEBUG("%s: isr[0x%x] mdp_drm_intr_status =%u \n",
			__func__, isr, mdp_drm_intr_status);

	if (isr == 0)
		goto irq_done;

	do {
		if (isr & dev_priv->irq_mask[i]) {
			DRM_DEBUG("%s:crtc[%d] \n", __func__,i);
			drm_handle_vblank(dev, i);
			smp_mb__before_atomic();
			atomic_inc(&dev_priv->vblank_cnt[i]);
			smp_mb__after_atomic();
			break;
		}
		i++;
	} while(i < DRM_KGSL_CRTC_MAX);

irq_done:
	return IRQ_HANDLED;
}

static void
kgsl_drm_fake_vblank_handler(struct work_struct *work)
{
	struct drm_kgsl_private *dev_priv = container_of(work,
			struct drm_kgsl_private, fake_vbl_work);

	/* refresh rate is about 60Hz. */
	usleep_range(15000, 16000);

	DRM_DEBUG("%s\n", __func__);

	mutex_lock(&dev_priv->fake_vbl_lock);
	drm_handle_vblank(dev_priv->drm_dev, DRM_KGSL_CRTC_FAKE);
	mutex_unlock(&dev_priv->fake_vbl_lock);

	if (dev_priv->fake_vbl)
		schedule_work(&dev_priv->fake_vbl_work);
}

static void
kgsl_drm_irq_preinstall(struct drm_device *dev)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;
	int i;
	mdp_drm_intr_mask = 0;

	DRM_DEBUG("%s\n", __func__);

	for (i = 0; i < DRM_KGSL_CRTC_MAX; i++) {
		if ( i != DRM_KGSL_CRTC_FAKE ) {
			atomic_set(&dev_priv->vblank_cnt[i], 0);
			mdp_drm_intr_mask |= dev_priv->irq_mask[i];
		}
	}

	dev->irq_enabled = 0;
}

static int
kgsl_drm_irq_postinstall(struct drm_device *dev)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;
	u32 mask;

	mdss_mdp_clk_ctrl(1, false);
	mask = readl_relaxed(dev_priv->regs +
			dev_priv->mdp_reg[MDSS_MDP_REG_INTR_EN]);

	DRM_DEBUG("%s:regs[0x%lx]\n", __func__, (uintptr_t)dev_priv->regs);

	mask |= dev_priv->vsync_irq;
	writel_relaxed(dev_priv->vsync_irq,
		dev_priv->regs + dev_priv->mdp_reg[MDSS_MDP_REG_INTR_CLEAR]);
	writel_relaxed(mask,
		dev_priv->regs + dev_priv->mdp_reg[MDSS_MDP_REG_INTR_EN]);

	dev->irq_enabled = 1;

	mdss_mdp_clk_ctrl(0, false);

	return 0;
}

static void
kgsl_drm_irq_uninstall(struct drm_device *dev)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;
	u32 mask;

	mdss_mdp_clk_ctrl(1, false);
	mask = readl_relaxed(dev_priv->regs + dev_priv->mdp_reg[MDSS_MDP_REG_INTR_EN]);

	DRM_DEBUG("%s:regs[0x%lx]\n", __func__, (uintptr_t)dev_priv->regs);

	mask &= ~dev_priv->vsync_irq;
	writel_relaxed(mask,
			dev_priv->regs + dev_priv->mdp_reg[MDSS_MDP_REG_INTR_EN]);

	mdp_drm_intr_mask = 0;
	dev->irq_enabled = 0;

	mdss_mdp_clk_ctrl(0, false);
}

int kgsl_gem_prime_handle_to_fd(struct drm_device *dev,
		struct drm_file *file_priv, uint32_t handle, uint32_t flags,
		int *prime_fd)
{
	struct drm_gem_object *obj;
	struct drm_kgsl_gem_object *priv;
	int ret = 0;

	obj = drm_gem_object_lookup(dev, file_priv, handle);

	if (obj == NULL) {
		DRM_ERROR("Invalid GEM handle %x\n", handle);
		return -EBADF;
	}

	mutex_lock(&dev->struct_mutex);
	priv = obj->driver_private;

	if (TYPE_IS_FD(priv->type))
		ret = -EINVAL;
	else if (TYPE_IS_PMEM(priv->type) || TYPE_IS_MEM(priv->type)) {
		if (priv->ion_handle) {
			*prime_fd = (int)ion_share_dma_buf_fd(
				kgsl_drm_ion_client, priv->ion_handle);
		} else {
			DRM_ERROR("GEM object has no ion memory allocated.\n");
			ret = -EINVAL;
		}
	} else {
		DRM_ERROR("GEM object has unknown memory type.\n");
		ret = -EINVAL;
	}

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int kgsl_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle)
{
	struct drm_gem_object *obj;
	struct ion_handle *ion_handle;
	struct drm_kgsl_gem_object *priv;
	struct sg_table *sg_table;
	struct scatterlist *s;
	int ret, gem_handle;
	unsigned long size;
	struct kgsl_mmu *mmu;

	ion_handle = ion_import_dma_buf(kgsl_drm_ion_client, prime_fd);
	if (IS_ERR_OR_NULL(ion_handle)) {
		DRM_ERROR("Unable to import dmabuf\n");
		return -EINVAL;
	}

	ret = ion_handle_get_size(kgsl_drm_ion_client, ion_handle, &size);
	if (ret < 0) {
		ion_free(kgsl_drm_ion_client, ion_handle);
		DRM_ERROR("Failed to get Ion buffer size\n");
		return -EINVAL;
	}

	if (size == 0) {
		ion_free(kgsl_drm_ion_client, ion_handle);
		DRM_ERROR("Tried to create GEM object from"	\
			"zero size ION buffer\n");
		return -EINVAL;
	}

	obj = drm_gem_object_alloc(dev, size);

	if (obj == NULL) {
		DRM_ERROR("Unable to allocate the GEM object\n");
		return -ENOMEM;
	}

	ret = kgsl_gem_init_obj(dev, file_priv, obj, &gem_handle);
	if (ret) {
		drm_gem_object_release(obj);
		kfree(obj->driver_private);
		kfree(obj);
		return ret;
	}

	priv = obj->driver_private;
	priv->ion_handle = ion_handle;

	priv->type = DRM_KGSL_GEM_TYPE_KMEM;
	list_add(&priv->list, &kgsl_mem_list);

#if defined(CONFIG_ARCH_MSM7X27) || defined(CONFIG_ARCH_MSM8625)
	mmu = &kgsl_get_device(KGSL_DEVICE_2D0)->mmu;
#else
	mmu = &kgsl_get_device(KGSL_DEVICE_3D0)->mmu;
#endif

	priv->pagetable = kgsl_mmu_getpagetable(mmu, KGSL_MMU_GLOBAL_PT);

	priv->memdesc.pagetable = priv->pagetable;

	sg_table = ion_sg_table(kgsl_drm_ion_client,
		priv->ion_handle);
	if (IS_ERR_OR_NULL(priv->ion_handle)) {
		DRM_ERROR("Unable to get ION sg table\n");
		ion_free(kgsl_drm_ion_client,
			priv->ion_handle);
		priv->ion_handle = NULL;
		kgsl_mmu_putpagetable(priv->pagetable);
		drm_gem_object_release(obj);
		kfree(priv);
		kfree(obj);
		return -ENOMEM;
	}

	priv->memdesc.sg = sg_table->sgl;

	/* Calculate the size of the memdesc from the sglist */

	priv->memdesc.sglen = 0;

	for (s = priv->memdesc.sg; s != NULL; s = sg_next(s)) {
		priv->memdesc.size += s->length;
		priv->memdesc.sglen++;
	}

	ret = kgsl_mmu_get_gpuaddr(priv->pagetable, &priv->memdesc);
	if (ret) {
		DRM_ERROR("kgsl_mmu_get_gpuaddr failed.  ret = %d\n", ret);
		ion_free(kgsl_drm_ion_client,
			priv->ion_handle);
		priv->ion_handle = NULL;
		kgsl_mmu_putpagetable(priv->pagetable);
		drm_gem_object_release(obj);
		kfree(priv);
		kfree(obj);
		return -ENOMEM;
	}

	ret = kgsl_mmu_map(priv->pagetable, &priv->memdesc);
	if (ret) {
		DRM_ERROR("kgsl_mmu_map failed.  ret = %d\n", ret);
		kgsl_mmu_put_gpuaddr(priv->pagetable, &priv->memdesc);
		ion_free(kgsl_drm_ion_client,
			priv->ion_handle);
		priv->ion_handle = NULL;
		kgsl_mmu_putpagetable(priv->pagetable);
		drm_gem_object_release(obj);
		kfree(priv);
		kfree(obj);
		return -ENOMEM;
	}

	priv->bufs[0].offset = 0;
	priv->bufs[0].gpuaddr = priv->memdesc.gpuaddr;
	priv->flags |= DRM_KGSL_GEM_FLAG_MAPPED;

	*handle = gem_handle;
	return 0;
}

static int kgsl_drm_gem_one_info(int id, void *ptr, void *data)
{
	struct drm_gem_object *obj = ptr;
	struct drm_kgsl_gem_object *priv = obj->driver_private;
	struct kgsl_drm_gem_info_data *gem_info_data = data;
	struct drm_kgsl_file_private *file_priv =
				gem_info_data->filp->driver_priv;

	seq_printf(gem_info_data->m, "%3ld \t%3d \t%3d \t%2d \t\t%2d \t0x%08lx"\
				" \t0x%x \t%2d \t\t%2d \t\t%2d\n",
				(uintptr_t)gem_info_data->filp->pid,
				file_priv->tgid,
				id,
				atomic_read(&obj->refcount.refcount),
				atomic_read(&obj->handle_count),
				(unsigned long)priv->memdesc.size,
				priv->flags,
				priv->bufcount,
				obj->export_dma_buf ? 1 : 0,
				obj->import_attach ? 1 : 0);

	return 0;
}

static int kgsl_drm_gem_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *drm_dev = node->minor->dev;
	struct kgsl_drm_gem_info_data gem_info_data;

	gem_info_data.m = m;

	seq_printf(gem_info_data.m, "pid \ttgid \thandle \trefcount \thcount "\
				"\tsize \t\tflags \tbufcount \texyport_to_fd "\
				"\timport_from_fd\n");

	mutex_lock(&drm_dev->struct_mutex);
	list_for_each_entry(gem_info_data.filp, &drm_dev->filelist, lhead) {
		spin_lock(&gem_info_data.filp->table_lock);
		idr_for_each(&gem_info_data.filp->object_idr,
				kgsl_drm_gem_one_info, &gem_info_data);
		spin_unlock(&gem_info_data.filp->table_lock);
	}
	mutex_unlock(&drm_dev->struct_mutex);

	return 0;
}

static struct drm_info_list kgsl_drm_debugfs_list[] = {
	{"gem_info", kgsl_drm_gem_info, DRIVER_GEM},
};

#define KGSL_DRM_DEBUGFS_ENTRIES ARRAY_SIZE(kgsl_drm_debugfs_list)

int kgsl_drm_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(kgsl_drm_debugfs_list,
					KGSL_DRM_DEBUGFS_ENTRIES,
					minor->debugfs_root, minor);
}

void kgsl_drm_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(kgsl_drm_debugfs_list,
				 KGSL_DRM_DEBUGFS_ENTRIES, minor);
}


static void kgsl_drm_parse_dt(struct drm_device *dev)
{
	struct drm_kgsl_private *dev_priv;
	struct platform_device *pdev;
	int ret;

	pdev = dev->driver->kdriver.platform_device;
	if (!pdev) {
		DRM_ERROR("failed to get platform device.\n");
		return;
	}

	dev_priv = dev->dev_private;
	if (!dev_priv) {
		DRM_ERROR("failed to get drm device private.\n");
		return;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,mdss-vsync-irq",
			&dev_priv->vsync_irq);
	if (ret)
		DRM_ERROR("prop qcom,mdss-vsync-irq : u32 read\n");

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,mdss-mdp-reg-intr-en",
			&dev_priv->mdp_reg[MDSS_MDP_REG_INTR_EN]);
	if (ret)
		DRM_ERROR("prop qcom,intr-en: u32 read\n");

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,mdss-mdp-reg-intr-status",
			&dev_priv->mdp_reg[MDSS_MDP_REG_INTR_STATUS]);
	if (ret)
		DRM_ERROR("prop qcom,intr-status : u32 read\n");

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,mdss-mdp-reg-intr-clear",
			&dev_priv->mdp_reg[MDSS_MDP_REG_INTR_CLEAR]);
	if (ret)
		DRM_ERROR("prop qcom,intr-clear : u32 read\n");

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,mdss-vsync-primary-mask",
			&dev_priv->irq_mask[DRM_KGSL_CRTC_PRIMARY]);
	if (ret)
		DRM_ERROR("prop qcom,primary-mask: u32 read\n");

	DRM_DEBUG("mdss: vsync_irq=%x intr_en=%x intr_status=%x intr_clr=%x\n",
			dev_priv->vsync_irq,
			dev_priv->mdp_reg[MDSS_MDP_REG_INTR_EN],
			dev_priv->mdp_reg[MDSS_MDP_REG_INTR_STATUS],
			dev_priv->mdp_reg[MDSS_MDP_REG_INTR_CLEAR]);

	DRM_DEBUG("irq_mask: primary=%x\n",
			dev_priv->irq_mask[DRM_KGSL_CRTC_PRIMARY]);

	return;
}

static int kgsl_drm_load(struct drm_device *dev, unsigned long flags)
{
	struct drm_kgsl_private *dev_priv;
	struct platform_device *pdev;
	struct resource *res;
	struct drm_minor *minor;
	int ret;

	minor = dev->primary;

	pdev = dev->driver->kdriver.platform_device;
	if (!pdev) {
		DRM_ERROR("failed to get platform device.\n");
		return -EFAULT;
	}

	dev_priv = devm_kzalloc(&pdev->dev, sizeof(*dev_priv), GFP_KERNEL);
	if (!dev_priv) {
		DRM_ERROR("failed to alloc dev private data.\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mdp_phys");
	if (!res) {
		DRM_ERROR("failed to get MDP base address\n");
		return -ENOMEM;
	}

	dev_priv->reg_size = resource_size(res);
	dev_priv->regs = devm_ioremap(&pdev->dev, res->start,
					dev_priv->reg_size);
	if (unlikely(!dev_priv->regs)) {
		DRM_ERROR("failed to map MDP base\n");
		return -ENOMEM;
	}

	/* acquire interrupt */
	dev_priv->irq = platform_get_irq_byname(pdev, KGSL_DRM_IRQ);

	/* init workqueue for fake vsync */
	INIT_WORK(&dev_priv->fake_vbl_work, kgsl_drm_fake_vblank_handler);

	/* mutext init */
	mutex_init(&dev_priv->fake_vbl_lock);

	/* store dev structure */
	dev_priv->drm_dev = dev;

	dev->dev_private = (void *)dev_priv;

	kgsl_drm_parse_dt(dev);

	DRM_DEBUG("%s:irq[%d]start[0x%x]regs[0x%lx]reg_size[0x%x]\n", __func__,
		dev_priv->irq, (int)res->start, (uintptr_t)dev_priv->regs,
		(int)dev_priv->reg_size);


	/*
	 * initialize variables related to vblank and waitqueue.
	 * 1 : primary device(LCD)
	 * 2 : secondary device(HDMI)
	 * 3 : writeback 0 device(Rotator)
	 * 4 : writeback 2 device(WFD)
	 * 5 : fake vblank device(Standby mode)
	 */
	ret = drm_vblank_init(dev, DRM_KGSL_CRTC_MAX);
	if (ret) {
		DRM_ERROR("failed to init vblank.\n");
		return ret;
	}

	ret = drm_debugfs_create_files(kgsl_drm_debugfs_list,
						KGSL_DRM_DEBUGFS_ENTRIES,
						minor->debugfs_root, minor);
	if (ret)
		DRM_DEBUG_DRIVER("failed to create kgsl-drm debugfs.\n");

	return 0;
}

static int kgsl_drm_unload(struct drm_device *dev)
{
	struct drm_kgsl_private *dev_priv =
		(struct drm_kgsl_private *)dev->dev_private;

	kfree(dev_priv);

	drm_debugfs_remove_files(kgsl_drm_debugfs_list,
				KGSL_DRM_DEBUGFS_ENTRIES, dev->primary);

	return 0;
}

static int kgsl_drm_open(struct drm_device *dev, struct drm_file *file)
{
	struct drm_kgsl_file_private *file_priv;

	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;

	file_priv->tgid = task_tgid_nr(current);
	file->driver_priv = file_priv;

	return 0;
}

static void kgsl_drm_postclose(struct drm_device *dev, struct drm_file *file)
{
	BUG_ON(!file->driver_priv);

	kfree(file->driver_priv);
	file->driver_priv = NULL;
}

struct drm_ioctl_desc kgsl_drm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(KGSL_GEM_CREATE, kgsl_gem_create_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_PREP, kgsl_gem_prep_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_SETMEMTYPE, kgsl_gem_setmemtype_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_GETMEMTYPE, kgsl_gem_getmemtype_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_BIND_GPU, kgsl_gem_bind_gpu_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_UNBIND_GPU, kgsl_gem_unbind_gpu_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_ALLOC, kgsl_gem_alloc_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_MMAP, kgsl_gem_mmap_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_GET_BUFINFO, kgsl_gem_get_bufinfo_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_GET_ION_FD, kgsl_gem_get_ion_fd_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_CREATE_FROM_ION,
				kgsl_gem_create_from_ion_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_SET_BUFCOUNT,
				kgsl_gem_set_bufcount_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_GET_BUFCOUNT,
				kgsl_gem_get_bufcount_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_SET_GLOCK_HANDLES_INFO,
				kgsl_gem_set_glock_handles_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_GET_GLOCK_HANDLES_INFO,
				kgsl_gem_get_glock_handles_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_SET_ACTIVE, kgsl_gem_set_active_ioctl, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_SET_USERDATA, kgsl_gem_set_userdata, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_GET_USERDATA, kgsl_gem_get_userdata, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_CACHE_OPS, kgsl_gem_cache_ops, 0),
	DRM_IOCTL_DEF_DRV(KGSL_GEM_CREATE_FD, kgsl_gem_create_fd_ioctl,
		      DRM_MASTER),
};

static const struct file_operations kgsl_drm_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.fasync = drm_fasync,
};

static struct drm_driver driver = {
	.driver_features = DRIVER_GEM | DRIVER_PRIME |
		DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED,
	.load = kgsl_drm_load,
	.unload = kgsl_drm_unload,
	.open = kgsl_drm_open,
	.postclose = kgsl_drm_postclose,
	.gem_init_object = kgsl_gem_init_object,
	.gem_free_object = kgsl_gem_free_object,
	.get_vblank_counter = kgsl_drm_get_vblank_counter,
	.enable_vblank = kgsl_drm_enable_vblank,
	.disable_vblank = kgsl_drm_disable_vblank,
	.irq_handler = kgsl_drm_irq_handler,
	.irq_preinstall = kgsl_drm_irq_preinstall,
	.irq_postinstall = kgsl_drm_irq_postinstall,
	.irq_uninstall = kgsl_drm_irq_uninstall,
	.prime_handle_to_fd = kgsl_gem_prime_handle_to_fd,
	.prime_fd_to_handle = kgsl_gem_prime_fd_to_handle,
	.ioctls = kgsl_drm_ioctls,
	.fops = &kgsl_drm_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

static int kgsl_drm_probe(struct platform_device *pdev)
{
	/* Only initialize once */
	if (kgsl_drm_inited == DRM_KGSL_INITED)
		return 0;

	kgsl_drm_inited = DRM_KGSL_INITED;

	driver.num_ioctls = DRM_ARRAY_SIZE(kgsl_drm_ioctls);
	driver.kdriver.platform_device = pdev;

	INIT_LIST_HEAD(&kgsl_mem_list);

	/* Create ION Client */
	kgsl_drm_ion_client = msm_ion_client_create("kgsl_drm");
	if (!kgsl_drm_ion_client) {
		DRM_ERROR("Unable to create ION client\n");
		return -ENOMEM;
	}

	return drm_platform_init(&driver, pdev);
}

static int kgsl_drm_remove(struct platform_device *pdev)
{
	kgsl_drm_inited = DRM_KGSL_NOT_INITED;

	if (kgsl_drm_ion_client)
		ion_client_destroy(kgsl_drm_ion_client);
	kgsl_drm_ion_client = NULL;

	drm_platform_exit(&driver, driver.kdriver.platform_device);

	return 0;
}

static int kgsl_drm_suspend(struct platform_device *dev, pm_message_t state)
{
	/* TODO */
	return 0;
}

static int kgsl_drm_resume(struct platform_device *dev)
{
	/* TODO */
	return 0;
}

static const struct of_device_id kgsl_drm_dt_match[] = {
	{ .compatible = "qcom,kgsl_drm",},
	{}
};
MODULE_DEVICE_TABLE(of, kgsl_drm_dt_match);

static struct platform_driver kgsl_drm_driver = {
	.probe = kgsl_drm_probe,
	.remove = kgsl_drm_remove,
	.suspend = kgsl_drm_suspend,
	.resume = kgsl_drm_resume,
	.shutdown = NULL,
	.driver = {
		/*
		 * Driver name must match the device name added in
		 * platform.c.
		 */
		.name = "kgsl_drm",
		.of_match_table = kgsl_drm_dt_match,
	},
};

static int __init kgsl_drm_init(void)
{
	return platform_driver_register(&kgsl_drm_driver);
}

static void __exit kgsl_drm_exit(void)
{
	platform_driver_unregister(&kgsl_drm_driver);
}

module_init(kgsl_drm_init);
module_exit(kgsl_drm_exit);
