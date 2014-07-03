/**
 * file pnw_topazinit.c
 * TOPAZ initialization and mtx-firmware upload
 *
 */

/**************************************************************************
 *
 * Copyright (c) 2007 Intel Corporation, Hillsboro, OR, USA
 * Copyright (c) Imagination Technologies Limited, UK
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors:
 *      Shengquan(Austin) Yuan <shengquan.yuan@intel.com>
 *      Elaine Wang <elaine.wang@intel.com>
 *      Li Zeng <li.zeng@intel.com>
 **************************************************************************/

/* NOTE: (READ BEFORE REFINE CODE)
 * 1. The FIRMWARE's SIZE is measured by byte, we have to pass the size
 * measured by word to DMAC.
 *
 *
 *
 */

/* include headers */

/* #define DRM_DEBUG_CODE 2 */

#include <linux/firmware.h>

#include <drm/drmP.h>
#include <drm/drm.h>

#include "psb_drv.h"
#include "pnw_topaz.h"
#include "psb_powermgmt.h"
#include "pnw_topaz_hw_reg.h"

#ifdef CONFIG_MDFD_GL3
#include "mdfld_gl3.h"
#endif

/* WARNING: this define is very important */
#define RAM_SIZE (1024 * 24)
#define MAX_TOPAZ_DATA_SIZE (12 * 4096)
#define MAX_TOPAZ_TEXT_SIZE (12 * 4096)
#define MAX_TOPAZ_RAM_SIZE (12 * 4096)

#define	MEMORY_ONLY 0
#define	MEM_AND_CACHE 1
#define CACHE_ONLY 2

#define FIRMWARE_NAME "topazsc_fw.bin"

extern int drm_psb_msvdx_tiling;
/* static function define */
static int topaz_upload_fw(struct drm_device *dev,
			   enum drm_pnw_topaz_codec codec,
			   uint32_t core_id);

#define UPLOAD_FW_BY_DMA 1

#if UPLOAD_FW_BY_DMA
static int topaz_dma_transfer(struct drm_psb_private *dev_priv,
			      uint32_t channel, uint32_t src_phy_addr,
			      uint32_t offset, uint32_t dst_addr,
			      uint32_t byte_num, uint32_t is_increment,
			      uint32_t is_write);
#else
static void topaz_mtx_upload_by_register(struct drm_device *dev,
		uint32_t mtx_mem, uint32_t addr,
		uint32_t size,
		struct ttm_buffer_object *buf,
		uint32_t core);
#endif

static void get_mtx_control_from_dash(struct drm_psb_private *dev_priv,
				      uint32_t core);
static void release_mtx_control_from_dash(struct drm_psb_private *dev_priv,
		uint32_t core);
static void pnw_topaz_mmu_hwsetup(struct drm_psb_private *dev_priv,
				  uint32_t core_id);
static int  mtx_dma_read(struct drm_device *dev, uint32_t core,
			 uint32_t source_addr, uint32_t size);
static int  mtx_dma_write(struct drm_device *dev,
			  uint32_t core);
static void pnw_topaz_restore_bias_table(struct drm_psb_private *dev_priv,
		int core);

/* Reset the encode system buffer registers.*/
static int pnw_topazsc_reset_ESB(struct drm_psb_private *dev_priv, int core_id)
{
	int x_pos, y_pos, i;

	MVEA_WRITE32(MVEA_CR_CMC_ESB_LOGICAL_REGION_SETUP(ESB_HWSYNC),
		     MVEASETUPESBREGION(0, 0,
					TOPAZSC_ESB_REGION_HEIGH,
					TOPAZSC_ESB_REGION_WIDTH,
					TOPAZSC_ESB_REGION_HEIGH,
					REGION_TYPE_LINEAR),
		     core_id);
	MVEA_WRITE32(MVEA_CR_CMC_PROC_ESB_ACCESS, ESB_HWSYNC, core_id);

	i = 0;
	for (y_pos = 0; y_pos < TOPAZSC_ESB_REGION_Y_MAX; y_pos++) {
		for (x_pos = 0; x_pos < TOPAZSC_ESB_REGION_X_MAX;
		     x_pos += 4, i += 4) {
			MVEA_ESB_WRITE32(i, 0, core_id);
		}
	}

	MVEA_WRITE32(MVEA_CR_CMC_ESB_LOGICAL_REGION_SETUP(ESB_HWSYNC),
		     MVEASETUPESBREGION(0, TOPAZSC_ESB_REGION_WIDTH,
					TOPAZSC_ESB_REGION_HEIGH,
					TOPAZSC_ESB_REGION_WIDTH,
					TOPAZSC_ESB_REGION_HEIGH,
					REGION_TYPE_LINEAR),
		     core_id);

	i = 0;
	for (y_pos = 0; y_pos < TOPAZSC_ESB_REGION_Y_MAX; y_pos++) {
		for (x_pos = 0; x_pos < TOPAZSC_ESB_REGION_X_MAX;
		     x_pos += 4, i += 4) {
			MVEA_ESB_WRITE32(i, 0, core_id);
		}
	}

	MVEA_WRITE32(MVEA_CR_CMC_PROC_ESB_ACCESS, 0, core_id);
	return 0;
}

int pnw_error_dump_reg(struct drm_psb_private *dev_priv, int core_id)
{
	uint32_t reg_val;
	int i;
	DRM_ERROR("DMA Register value dump:\n");
	for (i = 0; i < 8; i++) {
		DMAC_READ32(i * 4, &reg_val);
		DRM_ERROR("DMAC REG%d: 0x%08x\n", i, reg_val);
	}
	TOPAZ_MULTICORE_READ32(
		TOPAZSC_CR_MULTICORE_CORE_SEL_0, &reg_val);
	DRM_ERROR("TOPAZSC_CR_MULTICORE_CORE_SEL_0 0x%08x\n", reg_val);
	MTX_READ32(MTX_CR_MTX_SYSC_CDMAA, &reg_val, core_id);
	DRM_ERROR("MTX_CR_MTX_SYSC_CDMAA 0x%08x\n", reg_val);
	MTX_READ32(MTX_CR_MTX_SYSC_CDMAC, &reg_val, core_id);
	DRM_ERROR("MTX_CR_MTX_SYSC_CDMAC 0x%08x\n", reg_val);

	MTX_READ32(MTX_CR_MTX_SYSC_CDMAS0 , &reg_val, core_id);
	DRM_ERROR("MTX_CR_MTX_SYSC_CDMAS0 0x%08x\n", reg_val);
	MTX_READ32(MTX_CR_MTX_SYSC_CDMAS1, &reg_val, core_id);
	DRM_ERROR("MTX_CR_MTX_SYSC_CDMAS1 0x%08x\n", reg_val);
	MTX_READ32(MTX_CR_MTX_SYSC_CDMAT, &reg_val, core_id);
	DRM_ERROR("MTX_CR_MTX_SYSC_CDMAT 0x%08x\n", reg_val);
	for (i = 0; i < 6; i++) {
		TOPAZ_READ32(0x1c + i * 4, &reg_val, core_id);
		DRM_ERROR("MMU REG %d value 0x%08x\n", i, reg_val);
	}

	topaz_read_core_reg(dev_priv, core_id, TOPAZ_MTX_PC, &reg_val);
	DRM_ERROR("PC pointer: 0x%08x\n", reg_val);

	TOPAZ_MULTICORE_READ32(TOPAZSC_CR_MULTICORE_CMD_FIFO_1,
			&reg_val);
	reg_val &= MASK_TOPAZSC_CR_CMD_FIFO_SPACE;
	DRM_ERROR("TOPAZSC: Free words in command FIFO %d\n", reg_val);

	return 0;
}

int pnw_topaz_wait_for_register(struct drm_psb_private *dev_priv,
				uint32_t addr, uint32_t value, uint32_t mask)
{
	uint32_t tmp;
	uint32_t count = 10000;

	/* # poll topaz register for certain times */
	while (count) {
		/* #.# read */
		MM_READ32(addr, 0, &tmp);

		if (value == (tmp & mask))
			return 0;

		/* #.# delay and loop */
		PSB_UDELAY(100);/* derive from reference driver */
		--count;
	}

	/* # now waiting is timeout, return 1 indicat failed */
	/* XXX: testsuit means a timeout 10000 */

	DRM_ERROR("TOPAZ:time out to poll addr(0x%x) expected value(0x%08x), "
		  "actual 0x%08x (0x%08x & 0x%08x)\n",
		  addr, value, tmp & mask, tmp, mask);

	return -EBUSY;

}

static ssize_t psb_topaz_pmstate_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct drm_psb_private *dev_priv;
	struct pnw_topaz_private *topaz_priv;
	unsigned int pmstate;
	unsigned long flags;
	int ret = -EINVAL;

	if (drm_dev == NULL)
		return 0;

	dev_priv = drm_dev->dev_private;
	topaz_priv = dev_priv->topaz_private;
	pmstate = topaz_priv->pmstate;

	pmstate = topaz_priv->pmstate;
	spin_lock_irqsave(&topaz_priv->topaz_lock, flags);
	ret = snprintf(buf, 64, "%s, gating count 0x%08x\n",
		       (pmstate == PSB_PMSTATE_POWERUP) ?
		       "powerup" : "powerdown", topaz_priv->pm_gating_count);
	spin_unlock_irqrestore(&topaz_priv->topaz_lock, flags);

	return ret;
}

static DEVICE_ATTR(topaz_pmstate, 0444, psb_topaz_pmstate_show, NULL);


/* this function finish the first part of initialization, the rest
 * should be done in pnw_topaz_setup_fw
 */
int pnw_topaz_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct ttm_bo_device *bdev = &dev_priv->bdev;
	uint32_t core_id, core_rev;
	int ret = 0, n;
	bool is_iomem;
	struct pnw_topaz_private *topaz_priv;
	void *topaz_bo_virt;

	PSB_DEBUG_GENERAL("TOPAZ: init topazsc data structures\n");
	topaz_priv = kmalloc(sizeof(struct pnw_topaz_private), GFP_KERNEL);
	if (topaz_priv == NULL)
		return -1;

	dev_priv->topaz_private = topaz_priv;
	memset(topaz_priv, 0, sizeof(struct pnw_topaz_private));

	/* get device --> drm_device --> drm_psb_private --> topaz_priv
	 * for psb_topaz_pmstate_show: topaz_pmpolicy
	 * if not pci_set_drvdata, can't get drm_device from device
	 */
	pci_set_drvdata(dev->pdev, dev);
	if (device_create_file(&dev->pdev->dev,
			       &dev_attr_topaz_pmstate))
		DRM_ERROR("TOPAZ: could not create sysfs file\n");
	topaz_priv->sysfs_pmstate = sysfs_get_dirent(
					    dev->pdev->dev.kobj.sd,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 35))
					    NULL,
#endif
					    "topaz_pmstate");


	topaz_priv = dev_priv->topaz_private;
	topaz_priv->dev = dev;
	INIT_DELAYED_WORK(&topaz_priv->topaz_suspend_wq,
			  &psb_powerdown_topaz);

	/* # initialize comand topaz queueing [msvdx_queue] */
	INIT_LIST_HEAD(&topaz_priv->topaz_queue);
	/* # spin lock init? CHECK spin lock usage [msvdx_lock] */
	spin_lock_init(&topaz_priv->topaz_lock);

	/* # topaz status init. [msvdx_busy] */
	topaz_priv->topaz_busy = 0;
	/*Initial topaz_cmd_count should be larger than initial
	 *writeback value*/
	topaz_priv->topaz_cmd_count = 1;
	topaz_priv->topaz_fw_loaded = 0;
	/* FIXME: workaround since JPEG firmware is not ready */
	topaz_priv->topaz_cur_codec = 0;
	topaz_priv->topaz_hw_busy = 1;
	/* # gain write back structure,we may only need 32+4=40DW */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_buffer_object_create(bdev, PAGE_SIZE,
				       ttm_bo_type_kernel,
				       DRM_PSB_FLAG_MEM_MMU | TTM_PL_FLAG_NO_EVICT,
				       0, 0, 0, NULL, &(topaz_priv->topaz_bo));
#else
	ret = ttm_buffer_object_create(bdev, PAGE_SIZE,
				       ttm_bo_type_kernel,
				       DRM_PSB_FLAG_MEM_MMU | TTM_PL_FLAG_NO_EVICT,
				       0, 0, NULL, &(topaz_priv->topaz_bo));
#endif
	if (ret != 0) {
		DRM_ERROR("TOPAZ: failed to allocate topaz BO.\n");
		return -1;
	}

	ret = ttm_bo_kmap(topaz_priv->topaz_bo, 0,
			  topaz_priv->topaz_bo->num_pages,
			  &topaz_priv->topaz_bo_kmap);
	if (ret) {
		DRM_ERROR("TOPAZ: map topaz BO bo failed......\n");
		ttm_bo_unref(&topaz_priv->topaz_bo);
		return -1;
	}

	TOPAZ_READ32(TOPAZ_CR_TOPAZ_HW_CFG, &topaz_priv->topaz_num_cores, 0);

	topaz_priv->topaz_num_cores = F_EXTRACT(topaz_priv->topaz_num_cores,
						TOPAZ_CR_NUM_CORES_SUPPORTED);
	PSB_DEBUG_GENERAL("TOPAZ: number of cores: %d\n",
			  topaz_priv->topaz_num_cores);

	if (topaz_priv->topaz_num_cores > TOPAZSC_NUM_CORES) {
		topaz_priv->topaz_num_cores = TOPAZSC_NUM_CORES;
		DRM_ERROR("TOPAZ: number of cores (%d) exceed "
			  "TOPAZSC_NUM_CORES (%d)!\n",
			  topaz_priv->topaz_num_cores, TOPAZSC_NUM_CORES);
	}

	for (n = 0; n <  MAX_TOPAZ_CORES; n++) {
		topaz_priv->topaz_mtx_data_mem[n] = NULL;
		topaz_priv->topaz_mtx_reg_state[n] = NULL;
		topaz_priv->cur_mtx_data_size[n] = 0;
		topaz_priv->topaz_fw[n].text = NULL;
		topaz_priv->topaz_fw[n].data = NULL;
	}

	for (n = 0; n < topaz_priv->topaz_num_cores; n++) {
		TOPAZ_READ32(TOPAZ_CR_IMG_TOPAZ_CORE_ID, &core_id, n);
		TOPAZ_READ32(TOPAZ_CR_IMG_TOPAZ_CORE_REV, &core_rev, n);

		PSB_DEBUG_GENERAL("TOPAZ: core(%d), core_id(%x) core_rev(%x)\n",
				  n, core_id, core_rev);

		topaz_priv->topaz_mtx_reg_state[n] = kmalloc(TOPAZ_MTX_REG_SIZE,
						     GFP_KERNEL);
		if (topaz_priv->topaz_mtx_reg_state[n] == NULL) {
			DRM_ERROR("TOPAZ: failed to allocate space "
				  "for mtx register\n");
			goto out;
		}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		ret = ttm_buffer_object_create(bdev,
					       MAX_TOPAZ_RAM_SIZE,
					       ttm_bo_type_kernel,
					       DRM_PSB_FLAG_MEM_MMU |
					       TTM_PL_FLAG_NO_EVICT,
					       0, 0, 0, NULL,
					       &topaz_priv->topaz_mtx_data_mem[n]);
#else
		ret = ttm_buffer_object_create(bdev,
					       MAX_TOPAZ_RAM_SIZE,
					       ttm_bo_type_kernel,
					       DRM_PSB_FLAG_MEM_MMU |
					       TTM_PL_FLAG_NO_EVICT,
					       0, 0, NULL,
					       &topaz_priv->topaz_mtx_data_mem[n]);
#endif
		if (ret) {
			DRM_ERROR("TOPAZ: failed to allocate ttm buffer for "
				  "mtx data save of core (%d)\n", n);
			goto out;
		}

	}

	topaz_bo_virt = ttm_kmap_obj_virtual(&topaz_priv->topaz_bo_kmap,
					     &is_iomem);
	topaz_priv->topaz_mtx_wb = (uint32_t *) topaz_bo_virt;
	topaz_priv->topaz_wb_offset = topaz_priv->topaz_bo->offset;
	topaz_priv->topaz_sync_addr = (uint32_t *)(topaz_bo_virt
				      + 2048);
	topaz_priv->topaz_sync_offset = topaz_priv->topaz_wb_offset
					+ 2048;

	PSB_DEBUG_GENERAL("TOPAZ: alloc BO for WriteBack\n");
	PSB_DEBUG_GENERAL("TOPAZ: WB offset=0x%08x\n",
			  topaz_priv->topaz_wb_offset);
	PSB_DEBUG_GENERAL("TOPAZ: SYNC offset=0x%08x\n",
			  topaz_priv->topaz_sync_offset);

	/*topaz_cmd_count starts with 1. Reset writback value with 0*/
	memset((void *)(topaz_priv->topaz_mtx_wb), 0,
	       topaz_priv->topaz_num_cores
	       * MTX_WRITEBACK_DATASIZE_ROUND);
	memset((void *)topaz_priv->topaz_sync_addr, 0,
	       MTX_WRITEBACK_DATASIZE_ROUND);

	/*fence sequence number starts with 0. Reset sync seq with ~1*/
	*(topaz_priv->topaz_sync_addr) = ~0;

	pnw_topaz_mmu_flushcache(dev_priv);

	/* # set up MMU */
	for (n = 0; n < topaz_priv->topaz_num_cores; n++)
		pnw_topaz_mmu_hwsetup(dev_priv, n);


	for (n = 0; n < topaz_priv->topaz_num_cores; n++) {
		/* # reset topaz */
		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
			     |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
			     n);

		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
			     |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
			     n);
	}

	PSB_DEBUG_GENERAL("TOPAZ: Reset MVEA successfully.\n");

	/* create firmware storage */
	for (n = 0; n < PNW_TOPAZ_CODEC_NUM_MAX * 2; ++n) {
		/* #.# malloc DRM object for fw storage */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		ret = ttm_buffer_object_create(bdev, MAX_TOPAZ_TEXT_SIZE,
					       ttm_bo_type_kernel,
					       DRM_PSB_FLAG_MEM_MMU |
					       TTM_PL_FLAG_NO_EVICT,
					       0, 0, 0, NULL,
					       &topaz_priv->topaz_fw[n].text);
#else
		ret = ttm_buffer_object_create(bdev, MAX_TOPAZ_TEXT_SIZE,
					       ttm_bo_type_kernel,
					       DRM_PSB_FLAG_MEM_MMU |
					       TTM_PL_FLAG_NO_EVICT,
					       0, 0, NULL,
					       &topaz_priv->topaz_fw[n].text);
#endif
		if (ret) {
			DRM_ERROR("Failed to allocate firmware.\n");
			goto out;
		}

		/* #.# malloc DRM object for fw storage */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		ret = ttm_buffer_object_create(bdev, MAX_TOPAZ_DATA_SIZE,
					       ttm_bo_type_kernel,
					       DRM_PSB_FLAG_MEM_MMU |
					       TTM_PL_FLAG_NO_EVICT,
					       0, 0, 0, NULL,
					       &topaz_priv->topaz_fw[n].data);
#else
		ret = ttm_buffer_object_create(bdev, MAX_TOPAZ_DATA_SIZE,
					       ttm_bo_type_kernel,
					       DRM_PSB_FLAG_MEM_MMU |
					       TTM_PL_FLAG_NO_EVICT,
					       0, 0, NULL,
					       &topaz_priv->topaz_fw[n].data);
#endif
		if (ret) {
			DRM_ERROR("Failed to allocate firmware.\n");
			goto out;
		}
	}

	PSB_DEBUG_GENERAL("TOPAZ:old clock gating reg = 0x%08x\n",
		       PSB_RVDC32(PSB_TOPAZ_CLOCKGATING));
	/* Disable vec auto clockgating due to random encoder HW hang
	   during 1080p video recording */
	PSB_DEBUG_INIT("TOPAZ:reset to disable clock gating\n");

	PSB_WVDC32(VEC_CG_DIS_MASK, PSB_TOPAZ_CLOCKGATING);

	PSB_DEBUG_GENERAL("TOPAZ: Exit initialization\n");
	return 0;

out:
	for (n = 0; n < PNW_TOPAZ_CODEC_NUM_MAX * 2; ++n) {
		if (topaz_priv->topaz_fw[n].text != NULL)
			ttm_bo_unref(&topaz_priv->topaz_fw[n].text);
		if (topaz_priv->topaz_fw[n].data != NULL)
			ttm_bo_unref(&topaz_priv->topaz_fw[n].data);
	}

	for (n = 0; n < MAX_TOPAZ_CORES; n++) {
		if (topaz_priv->topaz_mtx_data_mem[n] != NULL)
			ttm_bo_unref(&topaz_priv->topaz_mtx_data_mem[n]);
		if (topaz_priv->topaz_mtx_reg_state[n] != NULL)
			kfree(topaz_priv->topaz_mtx_reg_state[n]);
	}

	return ret;
}

int pnw_topaz_uninit(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	int n;

	/* flush MMU */
	PSB_DEBUG_GENERAL("XXX: need to flush mmu cache here??\n");
	/* pnw_topaz_mmu_flushcache (dev_priv); */

	if (NULL == topaz_priv) {
		DRM_ERROR("TOPAZ: topaz_priv is NULL!\n");
		return -1;
	}

	/* # reset TOPAZ chip */
	pnw_topaz_reset(dev_priv);

	/* release resources */
	/* # release write back memory */
	topaz_priv->topaz_mtx_wb = NULL;

	for (n = 0; n < topaz_priv->topaz_num_cores; n++) {
		/* release mtx register save space */
		kfree(topaz_priv->topaz_mtx_reg_state[n]);

		/* release mtx data memory save space */
		if (topaz_priv->topaz_mtx_data_mem[n])
			ttm_bo_unref(&topaz_priv->topaz_mtx_data_mem[n]);

		kfree(topaz_priv->topaz_bias_table[n]);
	}
	/* # release firmware storage */
	for (n = 0; n < PNW_TOPAZ_CODEC_NUM_MAX * 2; ++n) {
		if (topaz_priv->topaz_fw[n].text != NULL)
			ttm_bo_unref(&topaz_priv->topaz_fw[n].text);
		if (topaz_priv->topaz_fw[n].data != NULL)
			ttm_bo_unref(&topaz_priv->topaz_fw[n].data);
	}

	ttm_bo_kunmap(&topaz_priv->topaz_bo_kmap);
	ttm_bo_unref(&topaz_priv->topaz_bo);

	if (topaz_priv) {
		pci_set_drvdata(dev->pdev, NULL);
		device_remove_file(&dev->pdev->dev, &dev_attr_topaz_pmstate);
		sysfs_put(topaz_priv->sysfs_pmstate);
		topaz_priv->sysfs_pmstate = NULL;

		kfree(topaz_priv);
		dev_priv->topaz_private = NULL;
	}

	return 0;
}

int pnw_topaz_reset(struct drm_psb_private *dev_priv)
{
	struct pnw_topaz_private *topaz_priv;
	uint32_t i;

	topaz_priv = dev_priv->topaz_private;
	topaz_priv->topaz_busy = 0;
	topaz_priv->topaz_cmd_count = 0;
	for (i = 0; i < MAX_TOPAZ_CORES; i++)
		topaz_priv->cur_mtx_data_size[i] = 0;
	topaz_priv->topaz_needs_reset = 0;

	memset((void *)(topaz_priv->topaz_mtx_wb), 0,
	       MAX_TOPAZ_CORES * MTX_WRITEBACK_DATASIZE_ROUND);

	for (i = 0; i < topaz_priv->topaz_num_cores; i++) {
		/* # reset topaz */
		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
			     |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
			     i);

		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
			     |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
			     i);
	}

	/* # set up MMU */
	for (i = 0; i < topaz_priv->topaz_num_cores; i++)
		pnw_topaz_mmu_hwsetup(dev_priv, i);

	return 0;
}


/* read firmware bin file and load all data into driver */
int pnw_topaz_init_fw(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	const struct firmware *raw = NULL;
	unsigned char *ptr;
	int ret = 0;
	int n;
	struct topazsc_fwinfo *cur_fw;
	size_t cur_size, total_size;
	struct pnw_topaz_codec_fw *cur_codec;
	struct ttm_buffer_object **cur_drm_obj;
	struct ttm_bo_kmap_obj tmp_kmap;
	bool is_iomem;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	topaz_priv->stored_initial_qp = 0;

	/* # get firmware */
	ret = request_firmware(&raw, FIRMWARE_NAME, &dev->pdev->dev);
	if (ret != 0) {
		DRM_ERROR("TOPAZ: request_firmware failed: %d\n", ret);
		ret = -EINVAL;
		return ret;
	}

	if ((NULL == raw) || (raw->size < sizeof(struct topazsc_fwinfo))) {
		DRM_ERROR("TOPAZ: firmware file is not correct size.\n");
		ret = -EINVAL;
		goto out;
	}

	total_size = raw->size;
	PSB_DEBUG_GENERAL("TOPAZ: opened firmware, size %zu\n", raw->size);

	if (total_size > (PNW_TOPAZ_CODEC_NUM_MAX * 2 *
			(MAX_TOPAZ_DATA_SIZE + MAX_TOPAZ_TEXT_SIZE))) {
		DRM_ERROR("%s firmwae size(%zu) isn't correct\n",
				__func__, total_size);
	}
	ptr = (unsigned char *) raw->data;

	if (!ptr) {
		DRM_ERROR("TOPAZ: failed to load firmware.\n");
		goto out;
	}

	/* # load fw from file */
	PSB_DEBUG_GENERAL("TOPAZ: load firmware.....\n");
	cur_fw = NULL;
	for (n = 0; n < PNW_TOPAZ_CODEC_NUM_MAX * 2; ++n) {
		if (total_size < sizeof(struct topazsc_fwinfo)) {
			PSB_DEBUG_GENERAL("TOPAZ: WARNING: Rearch end of "
					  "firmware. Have loaded %d firmwares.",
					  n);
			break;
		}

		total_size -=  sizeof(struct topazsc_fwinfo);
		cur_fw = (struct topazsc_fwinfo *) ptr;
		if (cur_fw->codec > PNW_TOPAZ_CODEC_NUM_MAX * 2) {
			DRM_ERROR("%s L%d unknown video codec(%d)",
					__func__, __LINE__,
					cur_fw->codec);
			ret = -EINVAL;
			goto out;
		}

		cur_codec = &topaz_priv->topaz_fw[cur_fw->codec];
		cur_codec->ver = cur_fw->ver;
		cur_codec->codec = cur_fw->codec;
		cur_codec->text_size = cur_fw->text_size;
		cur_codec->data_size = cur_fw->data_size;
		cur_codec->data_location = cur_fw->data_location;

		if (total_size < (cur_fw->text_size + cur_fw->data_size) ||
			       cur_fw->text_size > MAX_TOPAZ_TEXT_SIZE	||
			       cur_fw->data_size > MAX_TOPAZ_DATA_SIZE) {
			PSB_DEBUG_GENERAL("TOPAZ: WARNING: wrong size number" \
					"of data(%d) or text(%d). Have loaded" \
					" %d firmwares.", n,
					  cur_fw->data_size,
					  cur_fw->text_size);
			ret = -EINVAL;
			goto out;
		}

		total_size -= cur_fw->text_size;
		total_size -= cur_fw->data_size;

		PSB_DEBUG_GENERAL("TOPAZ: load firemware %s.\n",
				  codec_to_string(cur_fw->codec / 2));

		/* #.# handle text section */
		ptr += sizeof(struct topazsc_fwinfo);
		cur_drm_obj = &cur_codec->text;
		cur_size = cur_fw->text_size;

		/* #.# fill DRM object with firmware data */
		ret = ttm_bo_kmap(*cur_drm_obj, 0, (*cur_drm_obj)->num_pages,
				  &tmp_kmap);
		if (ret) {
			PSB_DEBUG_GENERAL("drm_bo_kmap failed: %d\n", ret);
			ttm_bo_unref(cur_drm_obj);
			*cur_drm_obj = NULL;
			ret = -EINVAL;
			goto out;
		}

		if (cur_size > ((*cur_drm_obj)->num_pages << PAGE_SHIFT)) {
			DRM_ERROR("%s L%d data size(%zu) is bigger than" \
					" BO size(%lu pages)\n",
					__func__, __LINE__,
					cur_size,
					(*cur_drm_obj)->num_pages);
			ret = -EINVAL;
			goto out;
		}

		PSB_DEBUG_GENERAL("\ttext_size: %d, "
				  "data_size %d, data_location 08%x\n",
				  cur_codec->text_size,
				  cur_codec->data_size,
				  cur_codec->data_location);
		memcpy(ttm_kmap_obj_virtual(&tmp_kmap, &is_iomem), ptr,
		       cur_size);

		ttm_bo_kunmap(&tmp_kmap);

		/* #.# handle data section */
		ptr += cur_fw->text_size;
		cur_drm_obj = &cur_codec->data;
		cur_size = cur_fw->data_size;

		if (cur_size > ((*cur_drm_obj)->num_pages << PAGE_SHIFT)) {
			DRM_ERROR("%s L%d data size(%zu) is bigger than" \
					" BO size(%lu pages)\n",
					__func__, __LINE__,
					cur_size,
					(*cur_drm_obj)->num_pages);
			ret = -EINVAL;
			goto out;
		}

		/* #.# fill DRM object with firmware data */
		ret = ttm_bo_kmap(*cur_drm_obj, 0, (*cur_drm_obj)->num_pages,
				  &tmp_kmap);
		if (ret) {
			PSB_DEBUG_GENERAL("drm_bo_kmap failed: %d\n", ret);
			ttm_bo_unref(cur_drm_obj);
			*cur_drm_obj = NULL;
			ret = -EINVAL;
			goto out;
		}

		memcpy(ttm_kmap_obj_virtual(&tmp_kmap, &is_iomem), ptr,
		       cur_size);

		ttm_bo_kunmap(&tmp_kmap);

		/* #.# update ptr */
		ptr += cur_fw->data_size;
	}

	release_firmware(raw);
	PSB_DEBUG_GENERAL("TOPAZ: return from firmware init\n");

	return 0;

out:
	if (raw) {
		PSB_DEBUG_GENERAL("release firmware....\n");
		release_firmware(raw);
	}

	return ret;
}

/* setup fw when start a new context */
int pnw_topaz_setup_fw(struct drm_device *dev, enum drm_pnw_topaz_codec codec)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	uint32_t verify_pc;
	int core_id;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	int i, ret = 0;

	/*Reset TopazSC*/
	PSB_DEBUG_GENERAL("TOPAZ: should reset topaz when context change\n");

	if (topaz_priv->topaz_num_cores > MAX_TOPAZ_CORES) {
		DRM_ERROR("TOPAZ: Invalid core nubmer %d\n",
			  topaz_priv->topaz_num_cores);
		return -EINVAL;
	}

	PSB_DEBUG_GENERAL("TOPAZ: Set up mmu for all %d cores\n",
			  topaz_priv->topaz_num_cores);

	for (core_id = 0; core_id < topaz_priv->topaz_num_cores; core_id++)
		pnw_topaz_mmu_hwsetup(dev_priv, core_id);

	/* # reset MVEA */
	for (core_id = 0; core_id < topaz_priv->topaz_num_cores; core_id++) {
		pnw_topazsc_reset_ESB(dev_priv, core_id);

		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
			     |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
			     F_ENCODE(1, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
			     core_id);

		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
			     |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
			     F_ENCODE(0, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
			     core_id);
	}

	psb_irq_uninstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);

	PSB_DEBUG_GENERAL("TOPAZ: will setup firmware ....\n");

	/*start each MTX in turn MUST start with master to
	 * enable comms to other cores*/
	for (core_id = topaz_priv->topaz_num_cores - 1;
	     core_id >= 0; core_id--) {
		topaz_set_mtx_target(dev_priv, core_id, 0);
		/* # reset mtx */
		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_SRST,
			      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_MVEA_SOFT_RESET)
			      |
			      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_MTX_SOFT_RESET) |
			      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_VLC_SOFT_RESET),
			      core_id);

		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_SRST, 0x0, core_id);

		/* # upload fw by drm */
		PSB_DEBUG_GENERAL("TOPAZ: will upload firmware to %d cores\n",
				  topaz_priv->topaz_num_cores);

		topaz_upload_fw(dev, codec, core_id);

		PSB_DEBUG_GENERAL("TOPAZ: after upload fw ....\n");

		/* D0.5, D0.6 and D0.7 */
		for (i = 5; i < 8; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x1 | (i << 4),
					     0);
		}
		/* Saves 8 Registers of D1 Bank  */
		/* D1.5, D1.6 and D1.7 */
		for (i = 5; i < 8; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x2 | (i << 4),
					     0);
		}

		PSB_DEBUG_GENERAL("TOPAZ: setting up pc address: 0x%08x"
				  "for core (%d)\n",
				  PC_START_ADDRESS, core_id);
		topaz_write_core_reg(dev_priv, core_id,
				     TOPAZ_MTX_PC, PC_START_ADDRESS);

		topaz_read_core_reg(dev_priv, core_id,
				    TOPAZ_MTX_PC, &verify_pc);

		PSB_DEBUG_GENERAL("TOPAZ: verify pc address for core"
				  " (%d):0x%08x\n",
				  core_id, verify_pc);

		/* enable auto clock is essential for this driver */
		TOPAZ_WRITE32(TOPAZ_CR_TOPAZ_AUTO_CLK_GATE,
			      F_ENCODE(1, TOPAZ_CR_TOPAZ_VLC_AUTO_CLK_GATE) |
			      F_ENCODE(1, TOPAZ_CR_TOPAZ_DB_AUTO_CLK_GATE),
			      core_id);
		MVEA_WRITE32(MVEA_CR_MVEA_AUTO_CLOCK_GATING,
			     F_ENCODE(1, MVEA_CR_MVEA_IPE_AUTO_CLK_GATE) |
			     F_ENCODE(1, MVEA_CR_MVEA_SPE_AUTO_CLK_GATE) |
			     F_ENCODE(1, MVEA_CR_MVEA_CMPRS_AUTO_CLK_GATE) |
			     F_ENCODE(1, MVEA_CR_MVEA_JMCOMP_AUTO_CLK_GATE),
			     core_id);

		/* flush the command FIFO - only has effect on master MTX */
		if (core_id == 0)
			TOPAZ_WRITE32(TOPAZ_CR_TOPAZ_CMD_FIFO_2,
				      F_ENCODE(1, TOPAZ_CR_CMD_FIFO_FLUSH),
				      0);

		/* clear MTX interrupt */
		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_INTCLEAR,
			      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTCLR_MTX),
			      core_id);

		/* put the number of cores in use in the scratch register
		 * so is is ready when the firmware wakes up. */
		TOPAZ_WRITE32(TOPAZ_CR_FIRMWARE_REG_1 +
				(MTX_SCRATCHREG_TOMTX << 2),
				topaz_priv->topaz_num_cores, core_id);

		/* # turn on MTX */
		topaz_set_mtx_target(dev_priv, core_id, 0);
		MTX_WRITE32(MTX_CORE_CR_MTX_ENABLE_OFFSET,
			    MTX_CORE_CR_MTX_ENABLE_MTX_ENABLE_MASK,
			    core_id);

		topaz_set_mtx_target(dev_priv, core_id, 0);
		MTX_WRITE32(MTX_CR_MTX_KICK, 1, core_id);
	}

	topaz_priv->topaz_cmd_count = 1;
	/* # poll on the interrupt which the firmware will generate */
	/*With DDKv186, interrupt would't be generated automatically after
	 * firmware set up*/
	PSB_DEBUG_GENERAL("TOPAZ: send NULL command to test firmware\n");
	for (core_id = 0; core_id < topaz_priv->topaz_num_cores; core_id++) {
		pnw_topaz_kick_null_cmd(dev_priv, core_id,
					topaz_priv->topaz_sync_offset,
					topaz_priv->topaz_cmd_count++, 0);

		ret = pnw_wait_on_sync(dev_priv,
				       topaz_priv->topaz_cmd_count - 1,
				       topaz_priv->topaz_sync_addr
				       + MTX_WRITEBACK_VALUE);
		if (0 != ret) {
			DRM_ERROR("TOPAZ: Failed to upload firmware for codec"
				  " %d!\n",
				  codec);
			pnw_error_dump_reg(dev_priv, core_id);
			return -EBUSY;
		}

		*(topaz_priv->topaz_sync_addr + MTX_WRITEBACK_VALUE)
		= TOPAZ_FIRMWARE_MAGIC;
		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_INTCLEAR,
			      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTCLR_MTX),
			      core_id);
	}


	PSB_DEBUG_GENERAL("TOPAZ: after topaz mtx setup ....\n");

	memset((void *)(topaz_priv->topaz_mtx_wb),
	       0,
	       topaz_priv->topaz_num_cores
	       * MTX_WRITEBACK_DATASIZE_ROUND);

	PSB_DEBUG_GENERAL("TOPAZ: firmware uploaded.\n");

	topaz_priv->topaz_busy = 0;

	for (core_id = 0; core_id < topaz_priv->topaz_num_cores; core_id++) {
		MVEA_WRITE32(MVEA_CR_BUFFER_SIDEBAND,
		     F_ENCODE(CACHE_ONLY, MVEA_CR_ABOVE_PARAM_OUT_SBAND) |
		     F_ENCODE(CACHE_ONLY, MVEA_CR_BELOW_PARAM_OUT_SBAND) |
		     F_ENCODE(MEM_AND_CACHE, MVEA_CR_ABOVE_PIX_OUT_SBAND) |
		     F_ENCODE(MEM_AND_CACHE, MVEA_CR_RECON_SBAND) |
		     F_ENCODE(CACHE_ONLY, MVEA_CR_REF_SBAND) |
		     F_ENCODE(CACHE_ONLY, MVEA_CR_ABOVE_PARAM_IN_SBAND) |
		     F_ENCODE(CACHE_ONLY, MVEA_CR_BELOW_PARAM_IN_SBAND) |
		     F_ENCODE(MEMORY_ONLY, MVEA_CR_CURR_PARAM_SBAND) |
		     F_ENCODE(CACHE_ONLY, MVEA_CR_ABOVE_PIX_IN_SBAND) |
		     F_ENCODE(MEMORY_ONLY, MVEA_CR_CURR_MB_SBAND),
		     core_id);
		MVEA_WRITE32(MVEA_CR_IPE_JITTER_FACTOR, 3 - 1, core_id);

		/*setup the jitter, base it on image size (using the height)*/
		if (topaz_priv->frame_h >= 720)
			MVEA_WRITE32(MVEA_CR_IPE_JITTER_FACTOR, 3 - 1,
					core_id);
		else if (topaz_priv->frame_w >= 480)
			MVEA_WRITE32(MVEA_CR_IPE_JITTER_FACTOR, 2 - 1,
					core_id);
		else
			MVEA_WRITE32(MVEA_CR_IPE_JITTER_FACTOR, 3 - 1,
					core_id);
	}

	psb_irq_preinstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
	psb_irq_postinstall_islands(dev, OSPM_VIDEO_ENC_ISLAND);
	pnw_topaz_enableirq(dev);

	return 0;
}

#if UPLOAD_FW_BY_DMA
int topaz_upload_fw(struct drm_device *dev, enum drm_pnw_topaz_codec codec, uint32_t core_id)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	const struct pnw_topaz_codec_fw *cur_codec_fw;
	uint32_t text_size, data_size;
	uint32_t data_location;
	uint32_t cur_mtx_data_size;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	int ret = 0;

	if (codec >= PNW_TOPAZ_CODEC_NUM_MAX) {
		DRM_ERROR("TOPAZ: Invalid codec %d\n", codec);
		return -EINVAL;
	}

	/* # MTX reset */
	PSB_DEBUG_GENERAL("TOPAZ: mtx reset.\n");
	MTX_WRITE32(MTX_CORE_CR_MTX_SOFT_RESET_OFFSET,
		    MTX_CORE_CR_MTX_SOFT_RESET_MTX_RESET_MASK,
		    core_id);

	/* # upload the master and slave firmware by DMA */
	if (core_id == 0)
		cur_codec_fw = &topaz_priv->topaz_fw[codec * 2];
	else
		cur_codec_fw = &topaz_priv->topaz_fw[codec*2 + 1];

	PSB_DEBUG_GENERAL("Topaz:upload codec %s(%d) text sz=%d data sz=%d\n"
			  "data location(0x%08x) to core(%d).\n",
			  codec_to_string(codec), codec,
			  cur_codec_fw->text_size, cur_codec_fw->data_size,
			  cur_codec_fw->data_location, core_id);

	/* # upload text. text_size is byte size*/
	text_size = cur_codec_fw->text_size / 4;
	/* adjust transfer sizes of text and data sections to match burst size*/
	text_size = ((text_size * 4 + (MTX_DMA_BURSTSIZE_BYTES - 1))
		     & ~(MTX_DMA_BURSTSIZE_BYTES - 1)) / 4;

	PSB_DEBUG_GENERAL("TOPAZ: text_size round up to %d\n", text_size);
	/* setup the MTX to start recieving data:
	   use a register for the transfer which will point to the source
	   (MTX_CR_MTX_SYSC_CDMAT) */
	/*MTX burst size (4 * 2 * 32bits = 32bytes) should match DMA burst
	  size (2 * 128bits = 32bytes) */
	/* #.# fill the dst addr */

	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAA, MTX_DMA_MEMORY_BASE, core_id);
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAC,
		    F_ENCODE(4, MTX_BURSTSIZE) |
		    F_ENCODE(0, MTX_RNW) |
		    F_ENCODE(1, MTX_ENABLE) |
		    F_ENCODE(text_size, MTX_LENGTH), core_id);

	/* #.# set DMAC access to host memory via BIF (deserted)*/
	/* TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 1, core_id);*/

	/* #.# transfer the codec */
	if (0 != topaz_dma_transfer(dev_priv, 0, cur_codec_fw->text->offset, 0,
				    MTX_CR_MTX_SYSC_CDMAT,
				    text_size, core_id, 0)) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}

	/* #.# wait dma finish */
	ret = pnw_topaz_wait_for_register(dev_priv,
					  DMAC_START + IMG_SOC_DMAC_IRQ_STAT(0),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN));
	if (ret != 0) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}

	/* #.# clear interrupt */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(0), 0);

	PSB_DEBUG_GENERAL("TOPAZ: firmware text upload complete.\n");

	/* # return access to topaz core (deserted)*/
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 0, core_id);*/

	/* # upload data */
	data_size = cur_codec_fw->data_size / 4;
	data_size = ((data_size * 4 + (MTX_DMA_BURSTSIZE_BYTES - 1))
		     & ~(MTX_DMA_BURSTSIZE_BYTES - 1)) / 4;

	data_location = cur_codec_fw->data_location;
	PSB_DEBUG_GENERAL("TOPAZ: data_size round up to %d\n"
			  "data_location round up to 0x%08x\n",
			  data_size, data_location);
	/* #.# fill the dst addr */
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAA,
		    data_location, core_id);
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAC,
		    F_ENCODE(4, MTX_BURSTSIZE) |
		    F_ENCODE(0, MTX_RNW) |
		    F_ENCODE(1, MTX_ENABLE) |
		    F_ENCODE(data_size, MTX_LENGTH), core_id);
	/* #.# set DMAC access to host memory via BIF(deserted) */
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 1, core_id);*/

	/* #.# transfer the codec */
	if (0 != topaz_dma_transfer(dev_priv, 0, cur_codec_fw->data->offset, 0,
				    MTX_CR_MTX_SYSC_CDMAT, data_size, core_id, 0)) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}

	/* #.# wait dma finish */
	ret = pnw_topaz_wait_for_register(dev_priv,
					  DMAC_START + IMG_SOC_DMAC_IRQ_STAT(0),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN));
	if (ret != 0) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}
	/* #.# clear interrupt */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(0), 0);

	pnw_topaz_mmu_flushcache(dev_priv);
	PSB_DEBUG_GENERAL("TOPAZ: firmware data upload complete.\n");
	/* # return access to topaz core(deserted) */
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 0, core_id);*/

	/* record this codec's mtx data size(+stack) for
	 * context save & restore */
	/* FIXME: since non-root sighting fixed by pre allocated,
	 * only need to correct the buffer size
	 */
	if (core_id == 0)
		cur_mtx_data_size =
			TOPAZ_MASTER_FW_MAX - (cur_codec_fw->data_location - \
					MTX_DMA_MEMORY_BASE);
	else
		cur_mtx_data_size =
			TOPAZ_SLAVE_FW_MAX - (cur_codec_fw->data_location - \
					MTX_DMA_MEMORY_BASE);
	topaz_priv->cur_mtx_data_size[core_id] = cur_mtx_data_size / 4;
	PSB_DEBUG_GENERAL("TOPAZ: Need to save %d words data for core %d\n",
			cur_mtx_data_size / 4, core_id);

	return 0;
}

#else
/* This function is only for debug */
void topaz_mtx_upload_by_register(struct drm_device *dev, uint32_t mtx_mem,
				  uint32_t addr, uint32_t size,
				  struct ttm_buffer_object *buf,
				  uint32_t core)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	uint32_t *buf_p;
	uint32_t debug_reg, bank_size, bank_ram_size, bank_count;
	uint32_t cur_ram_id, ram_addr , ram_id;
	int map_ret, lp;
	struct ttm_bo_kmap_obj bo_kmap;
	bool is_iomem;
	uint32_t cur_addr, ui32Size;

	PSB_DEBUG_GENERAL("TOPAZ: mtx upload: mtx_mem(0x%08x) addr(0x%08x)"
			  "size(%d)\n", mtx_mem, addr, size);

	get_mtx_control_from_dash(dev_priv, core);

	map_ret = ttm_bo_kmap(buf, 0, buf->num_pages, &bo_kmap);
	if (map_ret) {
		DRM_ERROR("TOPAZ: drm_bo_kmap failed: %d\n", map_ret);
		return;
	}
	buf_p = (uint32_t *) ttm_kmap_obj_virtual(&bo_kmap, &is_iomem);


	TOPAZ_READ32(TOPAZ_CORE_CR_MTX_DEBUG_OFFSET, &debug_reg, core);
	debug_reg = 0x0a0a0600;
	/*bank_size = (debug_reg & 0xf0000) >> 16;
	  bank_ram_size = 1 << (bank_size + 2);*/

	/*Bank size 4096, BanK number 6, Totally ram size:24k*/
	ui32Size = 0x1 << (F_EXTRACT(debug_reg,
				     TOPAZ_CR_MTX_LAST_RAM_BANK_SIZE) + 2);
	/* all other banks */
	bank_size = 0x1 << (F_EXTRACT(debug_reg,
				      TOPAZ_CR_MTX_RAM_BANK_SIZE) + 2);
	/* total RAM size */
	bank_ram_size = ui32Size + (bank_size *
				    (F_EXTRACT(debug_reg, TOPAZ_CR_MTX_RAM_BANKS) - 1));

	bank_count = (debug_reg & 0xf00) >> 8;

	PSB_DEBUG_GENERAL("TOPAZ: bank size %d, bank count %d, ram size %d\n",
			  bank_size, bank_count, bank_ram_size);

	pnw_topaz_wait_for_register(dev_priv,
				    REG_START_TOPAZ_MTX_HOST(core)
				    + MTX_CR_MTX_RAM_ACCESS_STATUS,
				    MASK_MTX_MTX_MTX_MCM_STAT,
				    MASK_MTX_MTX_MTX_MCM_STAT);

	cur_ram_id = -1;
	cur_addr = addr;
	for (lp = 0; lp < size / 4; ++lp) {
		ram_id = mtx_mem + (cur_addr / bank_size);

		if (cur_ram_id != ram_id) {
			ram_addr = cur_addr >> 2;

			MTX_WRITE32(MTX_CR_MTX_RAM_ACCESS_CONTROL,
				    F_ENCODE(ram_id, MTX_MTX_MCMID) |
				    F_ENCODE(ram_addr, MTX_MTX_MCM_ADDR) |
				    F_ENCODE(1, MTX_MTX_MCMAI),
				    core);

			cur_ram_id = ram_id;
		}
		cur_addr += 4;

		MTX_WRITE32(MTX_CR_MTX_RAM_ACCESS_DATA_TRANSFER,
			    *(buf_p + lp), core);

		pnw_topaz_wait_for_register(dev_priv,
					    MTX_CR_MTX_RAM_ACCESS_STATUS
					    + REG_START_TOPAZ_MTX_HOST(core),
					    MASK_MTX_MTX_MTX_MCM_STAT,
					    MASK_MTX_MTX_MTX_MCM_STAT);
	}

	release_mtx_control_from_dash(dev_priv, core);
	ttm_bo_kunmap(&bo_kmap);

	PSB_DEBUG_GENERAL("TOPAZ: register data upload done\n");
	return;
}

/* This function is only for debug when DMA isn't working */
int topaz_upload_fw(struct drm_device *dev, enum drm_pnw_topaz_codec codec,
		    uint32_t core_id)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	const struct pnw_topaz_codec_fw *cur_codec_fw;
	uint32_t text_size, data_size;
	uint32_t data_location;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	if (codec >= PNW_TOPAZ_CODEC_NUM_MAX) {
		DRM_ERROR("TOPAZ: Invalid codec %d\n", codec);
		return -EINVAL;
	}

	/* # refer HLD document */
	/* # MTX reset */
	PSB_DEBUG_GENERAL("TOPAZ: mtx reset.\n");
	MTX_WRITE32(MTX_CORE_CR_MTX_SOFT_RESET_OFFSET,
		    MTX_CORE_CR_MTX_SOFT_RESET_MTX_RESET_MASK,
		    core_id);

	/* # upload the master and slave firmware by DMA */
	if (core_id == 0)
		cur_codec_fw = &topaz_priv->topaz_fw[codec * 2];
	else
		cur_codec_fw = &topaz_priv->topaz_fw[codec * 2 + 1];

	PSB_DEBUG_GENERAL("Topaz:upload codec by MTX reg %s(%d)"
			  " text sz=%d data sz=%d"
			  " data location(%d) to core(%d).\n",
			  codec_to_string(codec), codec,
			  cur_codec_fw->text_size, cur_codec_fw->data_size,
			  cur_codec_fw->data_location, core_id);

	/* # upload text */
	text_size = cur_codec_fw->text_size;

	topaz_mtx_upload_by_register(dev, MTX_CORE_CODE_MEM,
				     0,
				     /*PC_START_ADDRESS - MTX_MEMORY_BASE,*/
				     text_size, cur_codec_fw->text, core_id);

	/* # upload data */
	data_size = cur_codec_fw->data_size;
	data_location = cur_codec_fw->data_location;

	topaz_mtx_upload_by_register(dev, MTX_CORE_DATA_MEM,
				     data_location - MTX_DMA_MEMORY_BASE, data_size,
				     cur_codec_fw->data, core_id);

	return 0;
}

#endif /* UPLOAD_FW_BY_DMA */

/* is_increment is always 0, so use it as core_id for workaround*/
int topaz_dma_transfer(struct drm_psb_private *dev_priv, uint32_t channel,
		       uint32_t src_phy_addr, uint32_t offset,
		       uint32_t soc_addr, uint32_t byte_num,
		       uint32_t is_increment, uint32_t is_write)
{
	uint32_t dmac_count;
	uint32_t irq_stat;
	uint32_t count;

	PSB_DEBUG_GENERAL("TOPAZ: using dma to transfer firmware\n");
	/* # check that no transfer is currently in progress and no
	   interrupts are outstanding ?? (why care interrupt) */
	DMAC_READ32(IMG_SOC_DMAC_COUNT(channel), &dmac_count);
	if (0 != (dmac_count & (MASK_IMG_SOC_EN | MASK_IMG_SOC_LIST_EN))) {
		DRM_ERROR("TOPAZ: there is tranfer in progress\n");
		return -1;
	}

	/* assert(0==(dmac_count & (MASK_IMG_SOC_EN | MASK_IMG_SOC_LIST_EN)));*/

	/* clear status of any previous interrupts */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(channel), 0);

	/* check irq status */
	DMAC_READ32(IMG_SOC_DMAC_IRQ_STAT(channel), &irq_stat);
	/* assert(0 == irq_stat); */
	if (0 != irq_stat)
		DRM_ERROR("TOPAZ: there is hold up\n");

	/*MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAA, MTX_DMA_MEMORY_BASE, is_increment);
	  MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAC,
	  F_ENCODE(4, MTX_BURSTSIZE) |
	  F_ENCODE(0, MTX_RNW) |
	  F_ENCODE(1, MTX_ENABLE) |
	  F_ENCODE(byte_num, MTX_LENGTH), is_increment);*/

	/* per hold - allow HW to sort itself out */
	DMAC_WRITE32(IMG_SOC_DMAC_PER_HOLD(channel), 16);
	/* clear previous interrupts */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(channel), 0);

	DMAC_WRITE32(IMG_SOC_DMAC_SETUP(channel),
		     (src_phy_addr + offset));
	count = DMAC_VALUE_COUNT(DMAC_BSWAP_NO_SWAP, DMAC_PWIDTH_32_BIT,
				 is_write, DMAC_PWIDTH_32_BIT, byte_num);
	/* generate an interrupt at the end of transfer */
	/* count |= MASK_IMG_SOC_TRANSFER_IEN; */
	/*count |= F_ENCODE(is_write, IMG_SOC_DIR);*/
	DMAC_WRITE32(IMG_SOC_DMAC_COUNT(channel), count);

	/* Burst : 2 * 128 bits = 32 bytes*/
	DMAC_WRITE32(IMG_SOC_DMAC_PERIPH(channel),
		     DMAC_VALUE_PERIPH_PARAM(DMAC_ACC_DEL_0,
					     0, DMAC_BURST_2));

	/* is_increment here is actually core_id*/
	DMAC_WRITE32(IMG_SOC_DMAC_PERIPHERAL_ADDR(channel),
		     MTX_CR_MTX_SYSC_CDMAT
		     + REG_START_TOPAZ_MTX_HOST(is_increment));

	/* Finally, rewrite the count register with
	 * the enable bit set to kick off the transfer
	 */
	DMAC_WRITE32(IMG_SOC_DMAC_COUNT(channel), count | MASK_IMG_SOC_EN);

	PSB_DEBUG_GENERAL("TOPAZ: dma transfer started.\n");

	return 0;
}

void topaz_write_core_reg(struct drm_psb_private *dev_priv,
			  uint32_t core,
			  uint32_t reg,
			  const uint32_t val)
{
	uint32_t tmp;
	get_mtx_control_from_dash(dev_priv, core);

	/* put data into MTX_RW_DATA */
	MTX_WRITE32(MTX_CORE_CR_MTX_REGISTER_READ_WRITE_DATA_OFFSET, val, core);

	/* request a write */
	tmp = reg &
	      ~MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK;

	MTX_WRITE32(MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_OFFSET,
		    tmp, core);

	/* wait for operation finished */
	pnw_topaz_wait_for_register(dev_priv,
				    REG_START_TOPAZ_MTX_HOST(core) +
				    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_OFFSET,
				    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK,
				    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK);

	release_mtx_control_from_dash(dev_priv, core);
}

void topaz_read_core_reg(struct drm_psb_private *dev_priv,
			 uint32_t core,
			 uint32_t reg,
			 uint32_t *ret_val)
{
	uint32_t tmp;

	get_mtx_control_from_dash(dev_priv, core);

	/* request a write */
	tmp = (reg &
	       ~MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK);

	MTX_WRITE32(MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_OFFSET,
		    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_RNW_MASK
		    | tmp, core);

	/* wait for operation finished */
	pnw_topaz_wait_for_register(dev_priv,
				    REG_START_TOPAZ_MTX_HOST(core) +
				    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_OFFSET,
				    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK,
				    MTX_CORE_CR_MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK);

	/* read  */
	MTX_READ32(MTX_CORE_CR_MTX_REGISTER_READ_WRITE_DATA_OFFSET,
		   ret_val, core);

	release_mtx_control_from_dash(dev_priv, core);
}

void get_mtx_control_from_dash(struct drm_psb_private *dev_priv, uint32_t core)
{
	int debug_reg_slave_val;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	int count = 0;

	/* GetMTXControlFromDash */
	TOPAZ_WRITE32(TOPAZ_CORE_CR_MTX_DEBUG_OFFSET,
		      F_ENCODE(1, TOPAZ_CR_MTX_DBG_IS_SLAVE) |
		      F_ENCODE(2, TOPAZ_CR_MTX_DBG_GPIO_OUT), core);
	do {
		TOPAZ_READ32(TOPAZ_CORE_CR_MTX_DEBUG_OFFSET,
			     &debug_reg_slave_val, core);
		count++;
	} while (((debug_reg_slave_val & 0x18) != 0) && count < 50000);

	if (count >= 50000)
		PSB_DEBUG_GENERAL("TOPAZ: timeout in "
				  "get_mtx_control_from_dash\n");

	/* save access control */
	TOPAZ_READ32(MTX_CORE_CR_MTX_RAM_ACCESS_CONTROL_OFFSET,
		     &topaz_priv->topaz_dash_access_ctrl, core);
}

void release_mtx_control_from_dash(struct drm_psb_private *dev_priv,
				   uint32_t core)
{
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	/* restore access control */
	TOPAZ_WRITE32(MTX_CORE_CR_MTX_RAM_ACCESS_CONTROL_OFFSET,
		      topaz_priv->topaz_dash_access_ctrl, core);

	/* release bus */
	TOPAZ_WRITE32(TOPAZ_CORE_CR_MTX_DEBUG_OFFSET,
		      F_ENCODE(1, TOPAZ_CR_MTX_DBG_IS_SLAVE), core);
}

/* When width or height is bigger than 1280. Encode will
   treat TTM_PL_TT buffers as tilied memory */
#define PSB_TOPAZ_TILING_THRESHOLD (1280)
void pnw_topaz_mmu_hwsetup(struct drm_psb_private *dev_priv, uint32_t core_id)
{
	uint32_t pd_addr = psb_get_default_pd_addr(dev_priv->mmu);
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	u32 val;
	bool is_src_tiled = false;

	PSB_DEBUG_GENERAL("TOPAZ: core (%d) MMU set up.\n", core_id);

	/* bypass all request while MMU is being configured */
	TOPAZ_WRITE32(TOPAZ_CR_MMU_CONTROL0,
		      F_ENCODE(1, TOPAZ_CR_MMU_BYPASS)
		      | F_ENCODE(1, TOPAZ_CR_MMU_BYPASS_DMAC), core_id);

	/* set MMU hardware at the page table directory */
	PSB_DEBUG_GENERAL("TOPAZ: write PD phyaddr=0x%08x "
			  "into MMU_DIR_LIST0/1\n", pd_addr);
	/*There's two of these (0) and (1).. only 0 is currently used*/
	TOPAZ_WRITE32(TOPAZ_CR_MMU_DIR_LIST_BASE(0), pd_addr, core_id);
	/*TOPAZ_WRITE32(TOPAZ_CR_MMU_DIR_LIST_BASE(1), 0, core_id);*/

	/* setup index register, all pointing to directory bank 0 */
	TOPAZ_WRITE32(TOPAZ_CR_MMU_BANK_INDEX, 0, core_id);

	if ((topaz_priv->frame_w > PSB_TOPAZ_TILING_THRESHOLD) ||
			(topaz_priv->frame_h > PSB_TOPAZ_TILING_THRESHOLD))
		is_src_tiled = true;

	if (drm_psb_msvdx_tiling && dev_priv->have_mem_mmu_tiling &&
		is_src_tiled) {
		uint32_t tile_start =
			dev_priv->bdev.man[TTM_PL_TT].gpu_offset;
		uint32_t tile_end =
			dev_priv->bdev.man[DRM_PSB_MEM_MMU_TILING].gpu_offset +
			(dev_priv->bdev.man[DRM_PSB_MEM_MMU_TILING].size
			<< PAGE_SHIFT);

		/* Enable memory tiling */
		val = ((tile_start >> 20) + (((tile_end >> 20) - 1) << 12) +
		((0x8 | 2) << 24)); /* 2k stride */

		PSB_DEBUG_GENERAL("Topax: Set up MMU Tiling register %08x\n",
					val);
		TOPAZ_WRITE32(TOPAZ_CR_MMU_TILE_BASE0, val, core_id);
	}

	/* now enable MMU access for all requestors */
	TOPAZ_WRITE32(TOPAZ_CR_MMU_CONTROL0,
		      F_ENCODE(0, TOPAZ_CR_MMU_BYPASS)
		      | F_ENCODE(0, TOPAZ_CR_MMU_BYPASS_DMAC), core_id);
}

void pnw_topaz_mmu_flushcache(struct drm_psb_private *dev_priv)
{
	uint32_t mmu_control;

	if (dev_priv->topaz_disabled)
		return;

	PSB_DEBUG_GENERAL("TOPAZ: pnw_topaz_mmu_flushcache\n");
#if 0
	PSB_DEBUG_GENERAL("XXX: Only one PTD/PTE cache"
			  " so flush using the master core\n");
#endif
	/* XXX: disable interrupt */
	TOPAZ_READ32(TOPAZ_CR_MMU_CONTROL0, &mmu_control, 0);
	mmu_control |= F_ENCODE(1, TOPAZ_CR_MMU_INVALDC);
	/*mmu_control |= F_ENCODE(1, TOPAZ_CR_MMU_FLUSH);*/

#if 0
	PSB_DEBUG_GENERAL("Set Invalid flag (this causes a flush with MMU\n"
			  "still operating afterwards even if not cleared,\n"
			  "but may want to replace with MMU_FLUSH?\n");
#endif
	TOPAZ_WRITE32(TOPAZ_CR_MMU_CONTROL0, mmu_control, 0);

	/* clear it */
	mmu_control &= (~F_ENCODE(1, TOPAZ_CR_MMU_INVALDC));
	/* mmu_control &= (~F_ENCODE(1, TOPAZ_CR_MMU_FLUSH)); */
	TOPAZ_WRITE32(TOPAZ_CR_MMU_CONTROL0, mmu_control, 0);
#ifdef CONFIG_MDFD_GL3
	gl3_invalidate();
#endif
}


static void pnw_topaz_restore_bias_table(struct drm_psb_private *dev_priv,
		int core)
{
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	u32 *p_command;
	unsigned int reg_cnt, reg_off, reg_val;
	u32 cmd_cnt;
	u32 max_cmd_cnt;

	if (core >= MAX_TOPAZ_CORES ||
			topaz_priv->topaz_bias_table[core] == NULL) {
		/*
		 * If VEC D0i3 isn't enabled, the bias table won't be saved
		 * in initialization. No need to restore.
		 */
		return;
	}

	/* First word is command header. Ignore */
	p_command = (u32 *)(topaz_priv->topaz_bias_table[core]
				+ sizeof(struct topaz_cmd_header));
	/* Second word indicates how many register write command sets */
	cmd_cnt = *p_command;
	max_cmd_cnt = PNW_TOPAZ_BIAS_TABLE_MAX_SIZE -
		sizeof(struct topaz_cmd_header);
	max_cmd_cnt /= TOPAZ_WRITEREG_BYTES_PER_SET;
	if (cmd_cnt > max_cmd_cnt) {
		DRM_ERROR("%s the number of command sets(%d) is wrong\n",
				__func__,
				cmd_cnt);
		return;
	}
	p_command++;

	PSB_DEBUG_GENERAL("TOPAZ: Restore BIAS table(size %d) for core %d\n",
			cmd_cnt,
			core);

	for (reg_cnt = 0; reg_cnt < cmd_cnt; reg_cnt++) {
		reg_off = *p_command;
		p_command++;
		reg_val = *p_command;
		p_command++;

		if (reg_off > TOPAZ_BIASREG_MAX(core) ||
				reg_off < TOPAZ_BIASREG_MIN(core))
			DRM_ERROR("TOPAZ: Ignore write (0x%08x)"
					" to register 0x%08x\n",
					reg_val, reg_off);
		else
			MM_WRITE32(0, reg_off, reg_val);
	}

	return;
}


int pnw_topaz_restore_mtx_state(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	int32_t core_id;
	uint32_t *mtx_reg_state;
	int i, need_restore = 0;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	unsigned long irq_flags;

	if (!topaz_priv->topaz_mtx_saved)
		return -1;

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if ((pos->ctx_type & 0xff) == VAEntrypointEncSlice ||
			(pos->ctx_type & 0xff) == VAEntrypointEncPicture)
			need_restore = 1;
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	if (0 == need_restore) {
		topaz_priv->topaz_mtx_saved = 0;
		PSB_DEBUG_GENERAL("topaz: no vec context found. needn't"
				  " to restore mtx registers.\n");
		return 0;
	}

	if (topaz_priv->topaz_fw_loaded == 0) {
		PSB_DEBUG_GENERAL("TOPAZ: needn't to restore context"
				  " without firmware uploaded\n");
		return 0;
	}

	if (topaz_priv->topaz_mtx_data_mem[0] == NULL) {
		PSB_DEBUG_GENERAL("TOPAZ: try to restore context"
				  " without space allocated, return"
				  " directly without restore\n");
		return -1;
	}

	/*TopazSC will be reset, no need to restore context.*/
	if (topaz_priv->topaz_needs_reset) {
		PSB_DEBUG_TOPAZ("TOPAZ: Reset. No need to restore context\n");
		topaz_priv->topaz_mtx_saved = 0;
		return 0;
	}

	/*There is no need to restore context for JPEG encoding*/
	if (PNW_IS_JPEG_ENC(topaz_priv->topaz_cur_codec)) {
		if (pnw_topaz_setup_fw(dev, topaz_priv->topaz_cur_codec))
			DRM_ERROR("TOPAZ: Setup JPEG firmware fails!\n");
		topaz_priv->topaz_mtx_saved = 0;
		return 0;
	}

	pnw_topaz_mmu_flushcache(dev_priv);

	for (core_id = 0; core_id < topaz_priv->topaz_num_cores; core_id++)
		pnw_topaz_mmu_hwsetup(dev_priv, core_id);
	for (core_id = 0; core_id < topaz_priv->topaz_num_cores; core_id++) {
		pnw_topazsc_reset_ESB(dev_priv, core_id);
		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
				F_ENCODE(1, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
				F_ENCODE(1, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
				F_ENCODE(1, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
				F_ENCODE(1, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
				|
				F_ENCODE(1, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
				F_ENCODE(1, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
				core_id);
		MVEA_WRITE32(MVEA_CR_IMG_MVEA_SRST,
				F_ENCODE(0, MVEA_CR_IMG_MVEA_SPE_SOFT_RESET) |
				F_ENCODE(0, MVEA_CR_IMG_MVEA_IPE_SOFT_RESET) |
				F_ENCODE(0, MVEA_CR_IMG_MVEA_CMPRS_SOFT_RESET) |
				F_ENCODE(0, MVEA_CR_IMG_MVEA_JMCOMP_SOFT_RESET)
				|
				F_ENCODE(0, MVEA_CR_IMG_MVEA_CMC_SOFT_RESET) |
				F_ENCODE(0, MVEA_CR_IMG_MVEA_DCF_SOFT_RESET),
				core_id);
	}

	for (core_id = topaz_priv->topaz_num_cores - 1;
	     core_id >= 0; core_id--) {
		topaz_set_mtx_target(dev_priv, core_id, 0);

		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_SRST,
				F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_MVEA_SOFT_RESET)
				|
				F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_IO_SOFT_RESET) |
				F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_DB_SOFT_RESET) |
				F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_MTX_SOFT_RESET) |
				F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_VLC_SOFT_RESET),
				core_id);
		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_SRST,
			0,
			core_id);

		topaz_set_mtx_target(dev_priv, core_id, 0);
		MTX_WRITE32(MTX_CORE_CR_MTX_SOFT_RESET_OFFSET,
			MTX_CORE_CR_MTX_SOFT_RESET_MTX_RESET_MASK,
			core_id);
		MTX_WRITE32(MTX_CORE_CR_MTX_SOFT_RESET_OFFSET,
			0, core_id);

		/* # upload fw by drm */
		PSB_DEBUG_GENERAL("TOPAZ: Restore text and data for core %d\n",
				  core_id);
		if (0 != mtx_dma_write(dev, core_id))
			return -1;

		pnw_topaz_mmu_flushcache(dev_priv);
	}

	for (core_id = topaz_priv->topaz_num_cores - 1;
			core_id >= 0; core_id--) {
		topaz_set_mtx_target(dev_priv, core_id, 0);

		mtx_reg_state = topaz_priv->topaz_mtx_reg_state[core_id];
		/* restore register */
		/* Saves 8 Registers of D0 Bank  */
		/* DoRe0, D0Ar6, D0Ar4, D0Ar2, D0FrT, D0.5, D0.6 and D0.7 */
		for (i = 0; i < 8; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x1 | (i << 4),
					     *mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 8 Registers of D1 Bank  */
		/* D1Re0, D1Ar5, D1Ar3, D1Ar1, D1RtP, D1.5, D1.6 and D1.7 */
		for (i = 0; i < 8; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x2 | (i << 4),
					     *mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 4 Registers of A0 Bank  */
		/* A0StP, A0FrP, A0.2 and A0.3 */
		for (i = 0; i < 4; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x3 | (i << 4),
					     *mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 4 Registers of A1 Bank  */
		/* A1GbP, A1LbP, A1.2 and A1.3 */
		for (i = 0; i < 4; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x4 | (i << 4),
					     *mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves PC and PCX  */
		for (i = 0; i < 2; i++) {
			topaz_write_core_reg(dev_priv, core_id, 0x5 | (i << 4),
					     *mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 8 Control Registers */
		/* TXSTAT, TXMASK, TXSTATI, TXMASKI, TXPOLL, TXGPIOI, TXPOLLI,
		 * TXGPIOO */
		for (i = 0; i < 8; i++) {
			topaz_write_core_reg(dev_priv, core_id,
					0x7 | (i << 4),
					*mtx_reg_state);
			mtx_reg_state++;
		}
		COMMS_WRITE32(TOPAZ_COMMS_CR_STAT_1(0), \
				*mtx_reg_state++, core_id);
		COMMS_WRITE32(TOPAZ_COMMS_CR_STAT_0(0), \
				*mtx_reg_state++, core_id);
		COMMS_WRITE32(TOPAZ_COMMS_CR_MTX_STATUS(0), \
				*mtx_reg_state++, core_id);
		COMMS_WRITE32(TOPAZ_COMMS_CR_CMD_WB_VAL(0), \
				*mtx_reg_state++, core_id);
		COMMS_WRITE32(TOPAZ_COMMS_CR_CMD_WB_ADDR(0), \
				*mtx_reg_state++, core_id);
		COMMS_WRITE32(TOPAZ_COMMS_CR_CMD_DATA_ADDR(0), \
				*mtx_reg_state++, core_id);
		COMMS_WRITE32(TOPAZ_COMMS_CR_CMD_WORD(0), \
				*mtx_reg_state++, core_id);

		/* enable auto clock is essential for this driver */
		TOPAZ_WRITE32(TOPAZ_CR_TOPAZ_AUTO_CLK_GATE,
			      F_ENCODE(1, TOPAZ_CR_TOPAZ_VLC_AUTO_CLK_GATE) |
			      F_ENCODE(1, TOPAZ_CR_TOPAZ_DB_AUTO_CLK_GATE),
			      core_id);
		MVEA_WRITE32(MVEA_CR_MVEA_AUTO_CLOCK_GATING,
			     F_ENCODE(1, MVEA_CR_MVEA_IPE_AUTO_CLK_GATE) |
			     F_ENCODE(1, MVEA_CR_MVEA_SPE_AUTO_CLK_GATE) |
			     F_ENCODE(1, MVEA_CR_MVEA_CMPRS_AUTO_CLK_GATE) |
			     F_ENCODE(1, MVEA_CR_MVEA_JMCOMP_AUTO_CLK_GATE),
			     core_id);

		topaz_read_core_reg(dev_priv, core_id,
			TOPAZ_MTX_PC, &i);

		PSB_DEBUG_GENERAL("TOPAZ: verify pc address for core"
			" (%d):0x%08x\n",
			core_id, i);

		/* flush the command FIFO - only has effect on master MTX */
		if (core_id == 0)
			TOPAZ_WRITE32(TOPAZ_CR_TOPAZ_CMD_FIFO_2,
				      F_ENCODE(1, TOPAZ_CR_CMD_FIFO_FLUSH),
				      0);

		/* clear MTX interrupt */
		TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_INTCLEAR,
			      F_ENCODE(1, TOPAZ_CR_IMG_TOPAZ_INTCLR_MTX),
			      core_id);

		/* put the number of cores in use in the scratch register
		 * so is is ready when the firmware wakes up. */
		TOPAZ_WRITE32(TOPAZ_CORE_NUMBER_SET_OFFSET, 2, core_id);

		/* # turn on MTX */
		topaz_set_mtx_target(dev_priv, core_id, 0);
		MTX_WRITE32(MTX_CORE_CR_MTX_ENABLE_OFFSET,
			    MTX_CORE_CR_MTX_ENABLE_MTX_ENABLE_MASK,
			    core_id);

	}

	if (!PNW_IS_JPEG_ENC(topaz_priv->topaz_cur_codec)) {
		for (core_id = topaz_priv->topaz_num_cores - 1;
				core_id >= 0; core_id--) {
		    /*MPEG4/H263 only use core 0*/
		    if (!PNW_IS_H264_ENC(topaz_priv->topaz_cur_codec)
			    && core_id > 0)
			continue;
		    pnw_topaz_restore_bias_table(dev_priv, core_id);
		}
	}

	PSB_DEBUG_GENERAL("TOPAZ: send NULL command to test firmware\n");
	topaz_priv->topaz_mtx_saved = 0;
	return 0;
}

int pnw_topaz_save_mtx_state(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	uint32_t *mtx_reg_state;
	int i, need_save = 0;
	uint32_t data_location, data_size;
	int core;
	struct pnw_topaz_codec_fw *cur_codec_fw;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct psb_video_ctx *pos, *n;
	unsigned long irq_flags;

	topaz_priv->topaz_mtx_saved = 0;

	/*TopazSC will be reset, no need to save context.*/
	if (topaz_priv->topaz_needs_reset) {
		PSB_DEBUG_TOPAZ("TOPAZ: Will be reset\n");
		return 0;
	}

	spin_lock_irqsave(&dev_priv->video_ctx_lock, irq_flags);
	list_for_each_entry_safe(pos, n, &dev_priv->video_ctx, head) {
		if ((pos->ctx_type & 0xff) == VAEntrypointEncSlice ||
			(pos->ctx_type & 0xff) == VAEntrypointEncPicture)
			need_save = 1;
	}
	spin_unlock_irqrestore(&dev_priv->video_ctx_lock, irq_flags);

	if (0 == need_save) {
		PSB_DEBUG_TOPAZ("TOPAZ: vec context not found. No need"
				  " to save mtx registers.\n");
		return 0;
	}

	PSB_DEBUG_INIT("TOPAZ: Found one vec codec(%d)." \
			  "Need to save mtx registers.\n",
			topaz_priv->topaz_cur_codec);

	if (topaz_priv->topaz_num_cores > MAX_TOPAZ_CORES) {
		DRM_ERROR("TOPAZ: Invalid core numbers: %d\n",
			  topaz_priv->topaz_num_cores);
		return -1;
	}

	if (topaz_priv->topaz_fw_loaded == 0) {
		PSB_DEBUG_GENERAL("TOPAZ: No need to restore context since"
				  " firmware has not been uploaded.\n");
		return -1;
	}

	/*JPEG encoding needn't to save context*/
	if (PNW_IS_JPEG_ENC(topaz_priv->topaz_cur_codec)) {
		topaz_priv->topaz_mtx_saved = 1;
		return 0;
	}

	for (core = topaz_priv->topaz_num_cores - 1; core >= 0; core--) {
		if (topaz_priv->topaz_mtx_data_mem[core] == NULL) {
			PSB_DEBUG_GENERAL("TOPAZ: try to save context "
					  "without space allocated, return"
					  " directly without save\n");
			return -1;
		}

		topaz_set_mtx_target(dev_priv, core, 0);
		/* stop mtx */
		MTX_WRITE32(MTX_CORE_CR_MTX_ENABLE_OFFSET,
				MTX_CORE_CR_MTX_ENABLE_MTX_TOFF_MASK, core);

		if (core == 0) {
			if (0 != pnw_topaz_wait_for_register(dev_priv,
							     REG_START_TOPAZ_MTX_HOST(core)
							     + MTX_CORE_CR_MTX_TXRPT_OFFSET,
							     TXRPT_WAITONKICK_VALUE,
							     0xffffffff)) {
				DRM_ERROR("TOPAZ: Stop MTX failed!\n");
				topaz_priv->topaz_needs_reset = 1;
				return -1;
			}
		}

		mtx_reg_state = topaz_priv->topaz_mtx_reg_state[core];

		/* Saves 8 Registers of D0 Bank  */
		/* DoRe0, D0Ar6, D0Ar4, D0Ar2, D0FrT, D0.5, D0.6 and D0.7 */
		for (i = 0; i < 8; i++) {
			topaz_read_core_reg(dev_priv, core, 0x1 | (i << 4),
					    mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 8 Registers of D1 Bank  */
		/* D1Re0, D1Ar5, D1Ar3, D1Ar1, D1RtP, D1.5, D1.6 and D1.7 */
		for (i = 0; i < 8; i++) {
			topaz_read_core_reg(dev_priv, core, 0x2 | (i << 4),
					    mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 4 Registers of A0 Bank  */
		/* A0StP, A0FrP, A0.2 and A0.3 */
		for (i = 0; i < 4; i++) {
			topaz_read_core_reg(dev_priv, core, 0x3 | (i << 4),
					    mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 4 Registers of A1 Bank  */
		/* A1GbP, A1LbP, A1.2 and A1.3 */
		for (i = 0; i < 4; i++) {
			topaz_read_core_reg(dev_priv, core, 0x4 | (i << 4),
					    mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves PC and PCX  */
		for (i = 0; i < 2; i++) {
			topaz_read_core_reg(dev_priv, core, 0x5 | (i << 4),
					    mtx_reg_state);
			mtx_reg_state++;
		}
		/* Saves 8 Control Registers */
		/* TXSTAT, TXMASK, TXSTATI, TXMASKI, TXPOLL, TXGPIOI, TXPOLLI,
		 * TXGPIOO */
		for (i = 0; i < 8; i++) {
			topaz_read_core_reg(dev_priv, core, 0x7 | (i << 4),
					    mtx_reg_state);
			mtx_reg_state++;
		}

		COMMS_READ32(TOPAZ_COMMS_CR_STAT_1(0), \
				mtx_reg_state++, core);
		COMMS_READ32(TOPAZ_COMMS_CR_STAT_0(0), \
				mtx_reg_state++, core);
		COMMS_READ32(TOPAZ_COMMS_CR_MTX_STATUS(0), \
				mtx_reg_state++, core);
		COMMS_READ32(TOPAZ_COMMS_CR_CMD_WB_VAL(0), \
				mtx_reg_state++, core);
		COMMS_READ32(TOPAZ_COMMS_CR_CMD_WB_ADDR(0), \
				mtx_reg_state++, core);
		COMMS_READ32(TOPAZ_COMMS_CR_CMD_DATA_ADDR(0), \
				mtx_reg_state++, core);
		COMMS_READ32(TOPAZ_COMMS_CR_CMD_WORD(0), \
				mtx_reg_state++, core);

		/* save mtx data memory */
		if (0 == core) {
			/*master core*/
			cur_codec_fw = &topaz_priv->topaz_fw[
					       topaz_priv->topaz_cur_codec * 2];
		} else {
			cur_codec_fw = &topaz_priv->topaz_fw[
					       topaz_priv->topaz_cur_codec * 2 \
					       + 1];
		}

		data_size = topaz_priv->cur_mtx_data_size[core];
		if (data_size > (MAX_TOPAZ_RAM_SIZE / 4)) {
			DRM_ERROR("TOPAZ: %s wrong data size %d!\n",
					__func__, data_size);
			data_size = MAX_TOPAZ_RAM_SIZE;
		}

		data_location = cur_codec_fw->data_location
				& ~(MTX_DMA_BURSTSIZE_BYTES - 1);
		if (0 != mtx_dma_read(dev, core,
				      data_location,
				      data_size)) {
			DRM_ERROR("TOPAZ: mtx_dma_read failed!\n");
			return -1;
		}
		pnw_topaz_mmu_flushcache(dev_priv);
	}

	topaz_priv->topaz_mtx_saved = 1;
	return 0;
}

int mtx_dma_read(struct drm_device *dev, uint32_t core,
		 uint32_t source_addr, uint32_t size)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct ttm_buffer_object *target;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;

	/* setup mtx DMAC registers to do transfer */
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAA, source_addr, core);
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAC,
		    F_ENCODE(4, MTX_BURSTSIZE) |
		    F_ENCODE(1, MTX_RNW) |
		    F_ENCODE(1, MTX_ENABLE) |
		    F_ENCODE(size, MTX_LENGTH), core);

	/* give the DMAC access to the host memory via BIF */
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 1, 0);*/

	target = topaz_priv->topaz_mtx_data_mem[core];
	/* transfert the data */
	if (0 != topaz_dma_transfer(dev_priv, 0, target->offset, 0,
				    MTX_CR_MTX_SYSC_CDMAT,
				    size, core, 1)) {
		pnw_error_dump_reg(dev_priv, core);
		return -1;
	}

	/* wait for it transfer */
	if (0 != pnw_topaz_wait_for_register(dev_priv,
				IMG_SOC_DMAC_IRQ_STAT(0) + DMAC_START,
				F_ENCODE(1, IMG_SOC_TRANSFER_FIN),
				F_ENCODE(1, IMG_SOC_TRANSFER_FIN))) {
		pnw_error_dump_reg(dev_priv, core);
		return -1;
	}

	/* clear interrupt */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(0), 0);
	/* give access back to topaz core */
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 0, 0);*/
	return 0;
}

int mtx_dma_write(struct drm_device *dev, uint32_t core_id)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	struct pnw_topaz_codec_fw *cur_codec_fw;
	uint32_t text_size, data_size;
	uint32_t data_location;
	int ret = 0;

	if (core_id == 0)/*for master core*/
		cur_codec_fw = &topaz_priv->topaz_fw[
				       topaz_priv->topaz_cur_codec * 2];
	else
		cur_codec_fw = &topaz_priv->topaz_fw[
				       topaz_priv->topaz_cur_codec * 2 + 1];

	PSB_DEBUG_GENERAL("Topaz: Restore codec %s(%d) text sz=%d data sz=%d\n"
			  "data location(0x%x) to core(%d).\n",
			  codec_to_string(topaz_priv->topaz_cur_codec),
			  topaz_priv->topaz_cur_codec,
			  cur_codec_fw->text_size, cur_codec_fw->data_size,
			  cur_codec_fw->data_location, core_id);

	/* # upload text. text_size is byte size*/
	text_size = cur_codec_fw->text_size / 4;
	/* adjust transfer sizes of text and data sections to match burst size*/
	text_size = ((text_size * 4 + (MTX_DMA_BURSTSIZE_BYTES - 1))
		     & ~(MTX_DMA_BURSTSIZE_BYTES - 1)) / 4;

	PSB_DEBUG_GENERAL("TOPAZ: text_size round up to %d\n", text_size);
	/* setup the MTX to start recieving data:
	   use a register for the transfer which will point to the source
	   (MTX_CR_MTX_SYSC_CDMAT) */
	/*MTX burst size (4 * 2 * 32bits = 32bytes) should match DMA burst
	  size (2 * 128bits = 32bytes) */
	/* #.# fill the dst addr */

	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAA, MTX_DMA_MEMORY_BASE, core_id);
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAC,
		    F_ENCODE(4, MTX_BURSTSIZE) |
		    F_ENCODE(0, MTX_RNW) |
		    F_ENCODE(1, MTX_ENABLE) |
		    F_ENCODE(text_size, MTX_LENGTH), core_id);

	/* #.# set DMAC access to host memory via BIF (deserted)*/
	/* TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 1, core_id);*/

	/* #.# transfer the codec */
	if (0 != topaz_dma_transfer(dev_priv, 0, cur_codec_fw->text->offset, 0,
				    MTX_CR_MTX_SYSC_CDMAT,
				    text_size, core_id, 0)) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}

	/* #.# wait dma finish */
	ret = pnw_topaz_wait_for_register(dev_priv,
					  DMAC_START + IMG_SOC_DMAC_IRQ_STAT(0),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN));
	if (ret != 0) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}

	/* #.# clear interrupt */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(0), 0);

	PSB_DEBUG_GENERAL("TOPAZ: firmware text upload complete.\n");

	/* # return access to topaz core (deserted)*/
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 0, core_id);*/

	/* # upload data */
	data_size = topaz_priv->cur_mtx_data_size[core_id];
	data_location = cur_codec_fw->data_location;
	data_location = data_location & (~(MTX_DMA_BURSTSIZE_BYTES - 1));

	if (data_size > (MAX_TOPAZ_RAM_SIZE / 4)) {
		DRM_ERROR("TOPAZ: %s wrong data size %d!\n",
				__func__, data_size);
		data_size = MAX_TOPAZ_RAM_SIZE;
	}

	PSB_DEBUG_GENERAL("TOPAZ: data_size round up to %d\n"
			"data_location round up to 0x%08x\n",
			data_size, data_location);
	/* #.# fill the dst addr */
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAA,
		    data_location, core_id);
	MTX_WRITE32(MTX_CR_MTX_SYSC_CDMAC,
		    F_ENCODE(4, MTX_BURSTSIZE) |
		    F_ENCODE(0, MTX_RNW) |
		    F_ENCODE(1, MTX_ENABLE) |
		    F_ENCODE(data_size, MTX_LENGTH), core_id);
	/* #.# set DMAC access to host memory via BIF(deserted) */
	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 1, core_id);*/

	/* #.# transfer the codec */
	if (0 != topaz_dma_transfer(dev_priv, 0,
				topaz_priv->topaz_mtx_data_mem[core_id]->offset,
				0,
				MTX_CR_MTX_SYSC_CDMAT, data_size, core_id, 0)) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}

	/* #.# wait dma finish */
	ret = pnw_topaz_wait_for_register(dev_priv,
					  DMAC_START + IMG_SOC_DMAC_IRQ_STAT(0),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN),
					  F_ENCODE(1, IMG_SOC_TRANSFER_FIN));
	if (ret != 0) {
		pnw_error_dump_reg(dev_priv, core_id);
		return -1;
	}
	/* #.# clear interrupt */
	DMAC_WRITE32(IMG_SOC_DMAC_IRQ_STAT(0), 0);

	PSB_DEBUG_GENERAL("TOPAZ: firmware text upload complete.\n");

	/*TOPAZ_WRITE32(TOPAZ_CR_IMG_TOPAZ_DMAC_MODE, 0, 0);*/
	return 0;
}


void pnw_reset_fw_status(struct drm_device *dev, u32 flag)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;
	struct pnw_topaz_private *topaz_priv = dev_priv->topaz_private;
	u32 reg;

	/*Before end the session, mark firmware MTX data as invalid.*/
	if (topaz_priv) {
		topaz_priv->topaz_mtx_saved = 0;
		if (flag & PNW_TOPAZ_START_CTX)
			topaz_priv->topaz_needs_reset = 0;
		else if (flag & PNW_TOPAZ_END_CTX) {
			TOPAZ_MTX_WB_READ32(topaz_priv->topaz_sync_addr,
				0, MTX_WRITEBACK_VALUE, &reg);
			PSB_DEBUG_TOPAZ("TOPAZ: current fence 0x%08x " \
				"last writeback 0x%08x\n",
				dev_priv->sequence[LNC_ENGINE_ENCODE],
				reg);
			if (topaz_priv->topaz_needs_reset) {
				DRM_ERROR("TOPAZ: reset Topaz\n");
				ospm_apm_power_down_topaz(topaz_priv->dev);
			}
		}
	}
}
