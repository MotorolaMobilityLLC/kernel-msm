/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful but, except 
 * as otherwise stated in writing, without any warranty; without even the 
 * implied warranty of merchantability or fitness for a particular purpose. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK 
 *
 ******************************************************************************/

#include <drm/drmP.h>
#include <drm/drm.h>

#include "img_defs.h"
#include "mutex.h"
#include "lock.h"
#include "pvr_drm.h"
#include "pvrsrv_interface.h"
#include "pvr_bridge.h"
#include "srvkm.h"
#include "mmap.h"
#include "dc_mrfld.h"
#include "drm_shared.h"
#include "linkage.h"

#if defined(PDUMP)
#include "linuxsrv.h"
#endif

#define PVR_DRM_SRVKM_CMD       DRM_PVR_RESERVED1
#define PVR_DRM_IS_MASTER_CMD   DRM_PVR_RESERVED4
#define PVR_DRM_DBGDRV_CMD      DRM_PVR_RESERVED6

#define PVR_DRM_SRVKM_IOCTL \
	DRM_IOW(DRM_COMMAND_BASE + PVR_DRM_SRVKM_CMD, PVRSRV_BRIDGE_PACKAGE)

#define PVR_DRM_IS_MASTER_IOCTL \
	DRM_IO(DRM_COMMAND_BASE + PVR_DRM_IS_MASTER_CMD)

#if defined(PDUMP)
#define	PVR_DRM_DBGDRV_IOCTL \
	DRM_IOW(DRM_COMMAND_BASE + PVR_DRM_DBGDRV_CMD, IOCTL_PACKAGE)
#endif

static int
PVRDRMIsMaster(struct drm_device *dev, void *arg, struct drm_file *pFile)
{
	return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
static struct drm_ioctl_desc pvr_ioctls[] = {
	{PVR_DRM_SRVKM_IOCTL, DRM_UNLOCKED, PVRSRV_BridgeDispatchKM},
	{PVR_DRM_IS_MASTER_IOCTL, DRM_MASTER, PVRDRMIsMaster},
#if defined(PDUMP)
	{PVR_DRM_DBGDRV_IOCTL, 0, dbgdrv_ioctl}
#endif
};
#else
static struct drm_ioctl_desc pvr_ioctls[] = {
	{PVR_DRM_SRVKM_IOCTL, DRM_UNLOCKED, PVRSRV_BridgeDispatchKM, PVR_DRM_SRVKM_IOCTL},
	{PVR_DRM_IS_MASTER_IOCTL, DRM_MASTER, PVRDRMIsMaster, PVR_DRM_IS_MASTER_IOCTL},
#if defined(PDUMP)
	{PVR_DRM_DBGDRV_IOCTL, 0, dbgdrv_ioctl. PVR_DRM_DBGDRV_IOCTL}
#endif
};
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)) */

DECLARE_WAIT_QUEUE_HEAD(sWaitForInit);

static bool bInitComplete;
static bool bInitFailed;

#if !defined(PVR_DRM_NOT_PCI)
struct pci_dev *gpsPVRLDMDev;
#endif

struct drm_device *gpsPVRDRMDev;

#define PVR_DRM_FILE struct drm_file *

int PVRSRVDrmLoad(struct drm_device *dev, unsigned long flags)
{
	int iRes = 0;

	/* Init the mutex lock gsDebugMutexNonIRQ */
	PVRDPFInit();

	DRM_DEBUG("PVRSRVDrmLoad");

	gpsPVRDRMDev = dev;
#if !defined(PVR_DRM_NOT_PCI)
	gpsPVRLDMDev = dev->pdev;
#endif

#if defined(PDUMP)
	iRes = dbgdrv_init();
	if (iRes != 0)
	{
		goto exit;
	}
#endif
	
	iRes = PVRCore_Init();
	if (iRes != 0)
	{
		goto exit_dbgdrv_cleanup;
	}

	if (MerrifieldDCInit(dev) != PVRSRV_OK)
	{
		DRM_ERROR("%s: display class init failed\n", __FUNCTION__);
		goto exit_pvrcore_cleanup;
	}

	goto exit;

exit_pvrcore_cleanup:
	PVRCore_Cleanup();

exit_dbgdrv_cleanup:
#if defined(PDUMP)
	dbgdrv_cleanup();
#endif
exit:
	if (iRes != 0)
	{
		bInitFailed = true;
	}
	bInitComplete = true;

	wake_up_interruptible(&sWaitForInit);

	return iRes;
}

int PVRSRVDrmUnload(struct drm_device *dev)
{
	DRM_DEBUG("PVRSRVDrmUnload");

	if (MerrifieldDCDeinit() != PVRSRV_OK)
	{
		DRM_ERROR("%s: can't deinit display class\n", __FUNCTION__);
	}

	PVRCore_Cleanup();

#if defined(PDUMP)
	dbgdrv_cleanup();
#endif

	return 0;
}

int PVRSRVDrmOpen(struct drm_device *dev, struct drm_file *file)
{
	while (!bInitComplete)
	{
		DEFINE_WAIT(sWait);

		prepare_to_wait(&sWaitForInit, &sWait, TASK_INTERRUPTIBLE);

		if (!bInitComplete)
		{
			DRM_DEBUG("%s: Waiting for module initialisation to complete", __FUNCTION__);

			schedule();
		}

		finish_wait(&sWaitForInit, &sWait);

		if (signal_pending(current))
		{
			return -ERESTARTSYS;
		}
	}

	if (bInitFailed)
	{
		DRM_DEBUG("%s: Module initialisation failed", __FUNCTION__);
		return -EINVAL;
	}

	return PVRSRVOpen(dev, file);
}

#if defined(SUPPORT_DRM_EXT)
void PVRSRVDrmPostClose(struct drm_device *dev, struct drm_file *file)
{
	PVRSRVRelease(file->driver_priv);

	file->driver_priv = NULL;
}
#else
int PVRSRVDrmRelease(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	void *psDriverPriv = file_priv->driver_priv;
	int ret;

	ret = drm_release(inode, filp);

	if (ret != 0)
	{
		
		DRM_DEBUG("%s : drm_release failed: %d", __FUNCTION__, ret);
	}

	PVRSRVRelease(filp);

	return 0;
}
#endif

void PVRSRVQueryIoctls(struct drm_ioctl_desc *ioctls)
{
	int i;

	for (i = 0; i < DRM_ARRAY_SIZE(pvr_ioctls); i++)
	{
		unsigned int slot = DRM_IOCTL_NR(pvr_ioctls[i].cmd) - DRM_COMMAND_BASE;
		ioctls[slot] = pvr_ioctls[i];
	}
}

/* FIXME: ALEX. This func might need rework. */
/*********************************
 *  Implemented to solve this FIXME
 *
 *  williamx.f.schmidt@intel.com
 */
unsigned int PVRSRVGetMeminfoSize(void* hMemHandle)
{
    PVRSRV_MEMINFO  minfo;

    if (copy_from_user(&minfo,hMemHandle,sizeof minfo))
    {
        return 0;
    }

    return minfo.uiAllocationSize;
}

/* FIXME: ALEX. This func might need rework. */
/*********************************
 *  Implemented to solve this FIXME
 *
 *  williamx.f.schmidt@intel.com
 */
void * PVRSRVGetMeminfoCPUAddr(void* hMemHandle)
{
    PVRSRV_MEMINFO  minfo;

    if (copy_from_user(&minfo,hMemHandle,sizeof minfo))
    {
        return 0;
    }

    return minfo.pvCpuVirtAddr;
}

/* FIXME: ALEX. To be implemented. */
/*********************************
 *  Implemented to solve this FIXME
 *
 *  williamx.f.schmidt@intel.com
 */
int PVRSRVGetMeminfoPages(void* hMemHandle, int npages, struct page ***pages)
{
    PVRSRV_MEMINFO  minfo;
    struct page   **pglist;
    uint32_t        kaddr;
    int             res;

    if (copy_from_user(&minfo,hMemHandle,sizeof minfo))
    {
        return -EFAULT;
    }

    kaddr = (uint32_t)minfo.pvCpuVirtAddr;

    if ((pglist = kzalloc(npages * sizeof(struct page*),GFP_KERNEL)) == NULL)
    {
        return -ENOMEM;
    }

    down_read(&current->mm->mmap_sem);
    res = get_user_pages(current,current->mm,kaddr,npages,0,0,pglist,NULL);
    up_read(&current->mm->mmap_sem);

    if (res <= 0)
    {
        kfree(pglist);
        return res;
    }

    *pages = pglist;
	return 0;
}

/*********************************
 *  Compliment to the above 'PVRSRVGetMeminfoPages to account for the
 *      situation where one cannot  obtain the page list  for the cpu
 *      virt addresses from user space. This will be called when that
 *      function fails and attempt to obtain an equivalent pfn list.
 *
 *  williamx.f.schmidt@intel.com
 */
int PVRSRVGetMeminfoPfn(void           *hMemHandle,
                        int             npages,
                        unsigned long **pfns)
{
    PVRSRV_MEMINFO          minfo;
    struct vm_area_struct  *vma;
    unsigned long          *pfnlist;
    uint32_t                kaddr;
    int                     res, pg = 0;

    /*
     *  This 'handle' is a pointer in user space to a meminfo struct.
     *  We need to copy it here and get the user's view of memory.
     */
    if (copy_from_user(&minfo,hMemHandle,sizeof minfo))
    {
        return 0;
    }

    kaddr = (uint32_t)minfo.pvCpuVirtAddr;

    if ((pfnlist = kzalloc(npages * sizeof(unsigned long),
                           GFP_KERNEL)) == NULL)
    {
        return -ENOMEM;
    }

    while (pg < npages)
    {
        if ((vma = find_vma(current->mm,
                            kaddr + (pg * PAGE_SIZE))) == NULL)
        {
            kfree(pfnlist);
            return -EFAULT;
        }

        if ((res = follow_pfn(
                        vma,
                        (unsigned long)(kaddr + (pg * PAGE_SIZE)),
                        &pfnlist[pg])) < 0)
        {
            kfree(pfnlist);
            return res;
        }

        ++pg;
    }

    *pfns = pfnlist;
    return 0;
} /* PVRSRVGetMeminfoPfn */

/* FIXME: ALEX. To be implemented. */
int PVRSRVInterrupt(struct drm_device* dev)
{
	return 1;
}

int PVRSRVMMap(struct file *pFile, struct vm_area_struct *ps_vma)
{
	return MMapPMR(pFile, ps_vma);
}

