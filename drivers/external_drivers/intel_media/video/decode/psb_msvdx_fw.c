/**************************************************************************
 * psb_msvdxinit.c
 * MSVDX initialization and mtx-firmware upload
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
 *    Li Zeng <li.zeng@intel.com>
 *    Fei Jiang <fei.jiang@intel.com>
 *
 **************************************************************************/

#include <drm/drmP.h>
#include <drm/drm.h>
#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"
#endif

#include "psb_msvdx.h"
#include <linux/firmware.h>
#include "psb_msvdx_reg.h"

#ifdef VXD_FW_BUILT_IN_KERNEL
#include <linux/module.h>
#endif

#define UPLOAD_FW_BY_DMA 1
#define STACKGUARDWORD          0x10101010
#define MSVDX_MTX_DATA_LOCATION 0x82880000
#define UNINITILISE_MEM 	0xcdcdcdcd

#ifdef VXD_FW_BUILT_IN_KERNEL
#define FIRMWARE_NAME "msvdx_fw_mfld_DE2.0.bin"
MODULE_FIRMWARE(FIRMWARE_NAME);
#endif

/*MSVDX FW header*/
struct msvdx_fw {
	uint32_t ver;
	uint32_t text_size;
	uint32_t data_size;
	uint32_t data_location;
};

int32_t psb_msvdx_alloc_fw_bo(struct drm_psb_private *dev_priv)
{
	uint32_t core_rev;
	int32_t ret = 0;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	core_rev = PSB_RMSVDX32(MSVDX_CORE_REV_OFFSET);

	if ((core_rev & 0xffffff) < 0x020000)
		msvdx_priv->mtx_mem_size = 16 * 1024;
	else
		msvdx_priv->mtx_mem_size = 56 * 1024;

	PSB_DEBUG_INIT("MSVDX: MTX mem size is 0x%08x bytes, allocate firmware BO size 0x%08x\n", msvdx_priv->mtx_mem_size,
		       msvdx_priv->mtx_mem_size + 4096);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
	ret = ttm_buffer_object_create(&dev_priv->bdev, msvdx_priv->mtx_mem_size + 4096, /* DMA may run over a page */
				       ttm_bo_type_kernel,
				       DRM_PSB_FLAG_MEM_MMU | TTM_PL_FLAG_NO_EVICT,
				       0, 0, 0, NULL, &msvdx_priv->fw);
#else
	ret = ttm_buffer_object_create(&dev_priv->bdev, msvdx_priv->mtx_mem_size + 4096, /* DMA may run over a page */
				       ttm_bo_type_kernel,
				       DRM_PSB_FLAG_MEM_MMU | TTM_PL_FLAG_NO_EVICT,
				       0, 0, NULL, &msvdx_priv->fw);
#endif

	if (ret) {
		DRM_ERROR("MSVDX: allocate firmware BO fail\n");
	}
	return ret;
}

#if UPLOAD_FW_BY_DMA

static void msvdx_get_mtx_control_from_dash(struct drm_psb_private *dev_priv)
{
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int count = 0;
	uint32_t reg_val = 0;

	REGIO_WRITE_FIELD(reg_val, MSVDX_MTX_DEBUG, MTX_DBG_IS_SLAVE, 1);
	REGIO_WRITE_FIELD(reg_val, MSVDX_MTX_DEBUG, MTX_DBG_GPIO_IN, 0x02);
	PSB_WMSVDX32(reg_val, MSVDX_MTX_DEBUG_OFFSET);

	do {
		reg_val = PSB_RMSVDX32(MSVDX_MTX_DEBUG_OFFSET);
		count++;
	} while (((reg_val & 0x18) != 0) && count < 50000);

	if (count >= 50000)
		PSB_DEBUG_GENERAL("MAVDX: timeout in get_mtx_control_from_dash\n");

	/* Save the access control register...*/
	msvdx_priv->psb_dash_access_ctrl = PSB_RMSVDX32(MTX_RAM_ACCESS_CONTROL_OFFSET);
}

static void msvdx_release_mtx_control_from_dash(struct drm_psb_private *dev_priv)
{
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	/* restore access control */
	PSB_WMSVDX32(msvdx_priv->psb_dash_access_ctrl, MTX_RAM_ACCESS_CONTROL_OFFSET);
	/* release bus */
	PSB_WMSVDX32(0x4, MSVDX_MTX_DEBUG_OFFSET);
}

/* for future debug info of msvdx related registers */
static void psb_setup_fw_dump(struct drm_psb_private *dev_priv, uint32_t dma_channel)
{
/* for DMAC REGISTER */
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_CDMAA is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_CDMAA_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_CDMAC value is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_CDMAC_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware DMAC_SETUP value is 0x%x\n", PSB_RMSVDX32(DMAC_DMAC_SETUP_OFFSET + dma_channel));
	DRM_ERROR("MSVDX: Upload firmware DMAC_DMAC_COUNT value is 0x%x\n", PSB_RMSVDX32(DMAC_DMAC_COUNT_OFFSET + dma_channel));
	DRM_ERROR("MSVDX: Upload firmware DMAC_DMAC_PERIPH_OFFSET value is 0x%x\n", PSB_RMSVDX32(DMAC_DMAC_PERIPH_OFFSET + dma_channel));
	DRM_ERROR("MSVDX: Upload firmware DMAC_DMAC_PERIPHERAL_ADDR value is 0x%x\n", PSB_RMSVDX32(DMAC_DMAC_PERIPHERAL_ADDR_OFFSET + dma_channel));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_CONTROL value is 0x%x\n", PSB_RMSVDX32(MSVDX_CONTROL_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware DMAC_DMAC_IRQ_STAT value is 0x%x\n", PSB_RMSVDX32(DMAC_DMAC_IRQ_STAT_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_MMU_CONTROL0 value is 0x%x\n", PSB_RMSVDX32(MSVDX_MMU_CONTROL0_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware DMAC_DMAC_COUNT 2222 value is 0x%x\n", PSB_RMSVDX32(DMAC_DMAC_COUNT_OFFSET + dma_channel));

/* for MTX REGISTER */
	DRM_ERROR("MSVDX: Upload firmware MTX_ENABLE_OFFSET is 0x%x\n", PSB_RMSVDX32(MTX_ENABLE_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_KICK_INPUT_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_KICK_INPUT_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_REGISTER_READ_WRITE_REQUEST_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_REGISTER_READ_WRITE_REQUEST_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_RAM_ACCESS_CONTROL_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_RAM_ACCESS_CONTROL_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_RAM_ACCESS_STATUS_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_RAM_ACCESS_STATUS_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_TIMERDIV_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_TIMERDIV_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_CDMAC_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_CDMAC_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_CDMAA_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_CDMAA_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_CDMAS0_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_CDMAS0_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MTX_SYSC_CDMAT_OFFSET value is 0x%x\n", PSB_RMSVDX32(MTX_SYSC_CDMAT_OFFSET));

/* for MSVDX CORE REGISTER */
	DRM_ERROR("MSVDX: Upload firmware MSVDX_CONTROL_OFFSET is 0x%x\n", PSB_RMSVDX32(MSVDX_CONTROL_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_INTERRUPT_CLEAR_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_INTERRUPT_CLEAR_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_INTERRUPT_STATUS_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_INTERRUPT_STATUS_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_HOST_INTERRUPT_ENABLE_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_HOST_INTERRUPT_ENABLE_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_MAN_CLK_ENABLE_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_MAN_CLK_ENABLE_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_CORE_ID_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_CORE_ID_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_MMU_STATUS_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_MMU_STATUS_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware FE_MSVDX_WDT_CONTROL_OFFSET value is 0x%x\n", PSB_RMSVDX32(FE_MSVDX_WDT_CONTROL_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware FE_MSVDX_WDTIMER_OFFSET value is 0x%x\n", PSB_RMSVDX32(FE_MSVDX_WDTIMER_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware BE_MSVDX_WDT_CONTROL_OFFSET value is 0x%x\n", PSB_RMSVDX32(BE_MSVDX_WDT_CONTROL_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware BE_MSVDX_WDTIMER_OFFSET value is 0x%x\n", PSB_RMSVDX32(BE_MSVDX_WDTIMER_OFFSET));

/* for MSVDX RENDEC REGISTER */
	DRM_ERROR("MSVDX: Upload firmware VEC_SHIFTREG_CONTROL_OFFSET is 0x%x\n", PSB_RMSVDX32(VEC_SHIFTREG_CONTROL_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_CONTROL0_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_CONTROL0_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_BUFFER_SIZE_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_BUFFER_SIZE_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_BASE_ADDR0_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_BASE_ADDR0_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_BASE_ADDR1_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_BASE_ADDR1_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_READ_DATA_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_READ_DATA_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_CONTEXT0_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_CONTEXT0_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_RENDEC_CONTEXT1_OFFSET value is 0x%x\n", PSB_RMSVDX32(MSVDX_RENDEC_CONTEXT1_OFFSET));

	DRM_ERROR("MSVDX: Upload firmware MSVDX_MMU_MEM_REQ value is 0x%x\n", PSB_RMSVDX32(MSVDX_MMU_MEM_REQ_OFFSET));
	DRM_ERROR("MSVDX: Upload firmware MSVDX_SYS_MEMORY_DEBUG2 value is 0x%x\n", PSB_RMSVDX32(0x6fc));
}

static void msvdx_upload_fw(struct drm_psb_private *dev_priv,
			  uint32_t address, const unsigned int words, int fw_sel)
{
	uint32_t reg_val = 0;
	uint32_t cmd;
	uint32_t uCountReg, offset, mmu_ptd;
	uint32_t size = (words * 4); /* byte count */
	uint32_t dma_channel = 0; /* Setup a Simple DMA for Ch0 */
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	PSB_DEBUG_GENERAL("MSVDX: Upload firmware by DMA\n");
	msvdx_get_mtx_control_from_dash(dev_priv);

	/* dma transfers to/from the mtx have to be 32-bit aligned and in multiples of 32 bits */
	PSB_WMSVDX32(address, MTX_SYSC_CDMAA_OFFSET);

	REGIO_WRITE_FIELD_LITE(reg_val, MTX_SYSC_CDMAC, BURSTSIZE,	4); /* burst size in multiples of 64 bits (allowed values are 2 or 4) */
	REGIO_WRITE_FIELD_LITE(reg_val, MTX_SYSC_CDMAC, RNW, 0);	/* false means write to mtx mem, true means read from mtx mem */
	REGIO_WRITE_FIELD_LITE(reg_val, MTX_SYSC_CDMAC, ENABLE,	1);				/* begin transfer */
	REGIO_WRITE_FIELD_LITE(reg_val, MTX_SYSC_CDMAC, LENGTH,	words);		/* This specifies the transfer size of the DMA operation in terms of 32-bit words */
	PSB_WMSVDX32(reg_val, MTX_SYSC_CDMAC_OFFSET);

	/* toggle channel 0 usage between mtx and other msvdx peripherals */
	{
		reg_val = PSB_RMSVDX32(MSVDX_CONTROL_OFFSET);
		REGIO_WRITE_FIELD(reg_val, MSVDX_CONTROL, DMAC_CH0_SELECT,  0);
		PSB_WMSVDX32(reg_val, MSVDX_CONTROL_OFFSET);
	}


	/* Clear the DMAC Stats */
	PSB_WMSVDX32(0 , DMAC_DMAC_IRQ_STAT_OFFSET + dma_channel);

	offset = msvdx_priv->fw->offset;

	if (fw_sel)
		offset += ((msvdx_priv->mtx_mem_size + 8192) & ~0xfff);

	/* use bank 0 */
	cmd = 0;
	PSB_WMSVDX32(cmd, MSVDX_MMU_BANK_INDEX_OFFSET);

	/* Write PTD to mmu base 0*/
	mmu_ptd = psb_get_default_pd_addr(dev_priv->mmu);
	PSB_WMSVDX32(mmu_ptd, MSVDX_MMU_DIR_LIST_BASE_OFFSET + 0);

	/* Invalidate */
	reg_val = PSB_RMSVDX32(MSVDX_MMU_CONTROL0_OFFSET);
	reg_val &= ~0xf;
	REGIO_WRITE_FIELD(reg_val, MSVDX_MMU_CONTROL0, MMU_INVALDC, 1);
	PSB_WMSVDX32(reg_val, MSVDX_MMU_CONTROL0_OFFSET);

	PSB_WMSVDX32(offset, DMAC_DMAC_SETUP_OFFSET + dma_channel);

	/* Only use a single dma - assert that this is valid */
	if ((size / 4) >= (1 << 15)) {
		DRM_ERROR("psb: DMA size beyond limited, aboart firmware uploading\n");
		return;
	}

	uCountReg = PSB_DMAC_VALUE_COUNT(PSB_DMAC_BSWAP_NO_SWAP,
					 0,  /* 32 bits */
					 PSB_DMAC_DIR_MEM_TO_PERIPH,
					 0,
					 (size / 4));
	/* Set the number of bytes to dma*/
	PSB_WMSVDX32(uCountReg, DMAC_DMAC_COUNT_OFFSET + dma_channel);

	cmd = PSB_DMAC_VALUE_PERIPH_PARAM(PSB_DMAC_ACC_DEL_0, PSB_DMAC_INCR_OFF, PSB_DMAC_BURST_2);
	PSB_WMSVDX32(cmd, DMAC_DMAC_PERIPH_OFFSET + dma_channel);

	/* Set destination port for dma */
	cmd = 0;
	REGIO_WRITE_FIELD(cmd, DMAC_DMAC_PERIPHERAL_ADDR, ADDR, MTX_SYSC_CDMAT_OFFSET);
	PSB_WMSVDX32(cmd, DMAC_DMAC_PERIPHERAL_ADDR_OFFSET + dma_channel);


	/* Finally, rewrite the count register with the enable bit set*/
	PSB_WMSVDX32(uCountReg | DMAC_DMAC_COUNT_EN_MASK, DMAC_DMAC_COUNT_OFFSET + dma_channel);

	/* Wait for all to be done */
	if (psb_wait_for_register(dev_priv,
				  DMAC_DMAC_IRQ_STAT_OFFSET + dma_channel,
				  DMAC_DMAC_IRQ_STAT_TRANSFER_FIN_MASK,
				  DMAC_DMAC_IRQ_STAT_TRANSFER_FIN_MASK,
				  2000000, 5)) {
		psb_setup_fw_dump(dev_priv, dma_channel);
		msvdx_release_mtx_control_from_dash(dev_priv);
		return;
	}

	/* Assert that the MTX DMA port is all done aswell */
	if (psb_wait_for_register(dev_priv,
			MTX_SYSC_CDMAS0_OFFSET,
			1, 1, 2000000, 5)) {
		msvdx_release_mtx_control_from_dash(dev_priv);
		return;
	}

	msvdx_release_mtx_control_from_dash(dev_priv);

	PSB_DEBUG_GENERAL("MSVDX: Upload done\n");
}

#else

static void msvdx_upload_fw(struct drm_psb_private *dev_priv,
			  const uint32_t data_mem, uint32_t ram_bank_size,
			  uint32_t address, const unsigned int words,
			  const uint32_t * const data)
{
	uint32_t loop, ctrl, ram_id, addr, cur_bank = (uint32_t) ~0;
	uint32_t access_ctrl;

	PSB_DEBUG_GENERAL("MSVDX: Upload firmware by register interface\n");
	/* Save the access control register... */
	access_ctrl = PSB_RMSVDX32(MTX_RAM_ACCESS_CONTROL_OFFSET);

	/* Wait for MCMSTAT to become be idle 1 */
	psb_wait_for_register(dev_priv, MTX_RAM_ACCESS_STATUS_OFFSET,
			      1,	/* Required Value */
			      0xffffffff, /* Enables */
			      2000000, 5);

	for (loop = 0; loop < words; loop++) {
		ram_id = data_mem + (address / ram_bank_size);
		if (ram_id != cur_bank) {
			addr = address >> 2;
			ctrl = 0;
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCMID, ram_id);
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCM_ADDR, addr);
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCMAI, 1);
			PSB_WMSVDX32(ctrl, MTX_RAM_ACCESS_CONTROL_OFFSET);
			cur_bank = ram_id;
		}
		address += 4;

		PSB_WMSVDX32(data[loop],
			     MTX_RAM_ACCESS_DATA_TRANSFER_OFFSET);

		/* Wait for MCMSTAT to become be idle 1 */
		psb_wait_for_register(dev_priv, MTX_RAM_ACCESS_STATUS_OFFSET,
				      1,	/* Required Value */
				      0xffffffff, /* Enables */
				      2000000, 5);
	}
	PSB_DEBUG_GENERAL("MSVDX: Upload done\n");

	/* Restore the access control register... */
	PSB_WMSVDX32(access_ctrl, MSVDX_MTX_RAM_ACCESS_CONTROL);
}

#endif

#if 0
static int msvdx_verify_fw(struct drm_psb_private *dev_priv,
			 const uint32_t ram_bank_size,
			 const uint32_t data_mem, uint32_t address,
			 const uint32_t words, const uint32_t * const data)
{
	uint32_t loop, ctrl, ram_id, addr, cur_bank = (uint32_t) ~0;
	uint32_t access_ctrl;
	int ret = 0;

	/* Save the access control register... */
	access_ctrl = PSB_RMSVDX32(MTX_RAM_ACCESS_CONTROL_OFFSET);

	/* Wait for MCMSTAT to become be idle 1 */
	psb_wait_for_register(dev_priv, MTX_RAM_ACCESS_STATUS_OFFSET,
			      1,	/* Required Value */
			      0xffffffff, /* Enables */
			      2000000, 5);

	for (loop = 0; loop < words; loop++) {
		uint32_t reg_value;
		ram_id = data_mem + (address / ram_bank_size);

		if (ram_id != cur_bank) {
			addr = address >> 2;
			ctrl = 0;
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCMID, ram_id);
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCM_ADDR, addr);
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCMAI, 1);
			REGIO_WRITE_FIELD_LITE(ctrl,
					       MTX_RAM_ACCESS_CONTROL,
					       MTX_MCMR, 1);

			PSB_WMSVDX32(ctrl, MTX_RAM_ACCESS_CONTROL_OFFSET);

			cur_bank = ram_id;
		}
		address += 4;

		/* Wait for MCMSTAT to become be idle 1 */
		psb_wait_for_register(dev_priv, MTX_RAM_ACCESS_STATUS_OFFSET,
				      1,	/* Required Value */
				      0xffffffff, /* Enables */
				      2000000, 5);

		reg_value = PSB_RMSVDX32(MTX_RAM_ACCESS_DATA_TRANSFER_OFFSET);
		if (data[loop] != reg_value) {
			DRM_ERROR("psb: Firmware validation fails"
				  " at index=%08x\n", loop);
			ret = 1;
			break;
		}
	}

	/* Restore the access control register... */
	PSB_WMSVDX32(access_ctrl, MTX_RAM_ACCESS_CONTROL_OFFSET);

	return ret;
}
#endif

static int msvdx_get_fw_bo(struct drm_device *dev,
			   const struct firmware **raw, uint8_t *name)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	int rc, fw_size;
	void *ptr = NULL;
	struct ttm_bo_kmap_obj tmp_kmap;
	bool is_iomem;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	void *gpu_addr;

	rc = request_firmware(raw, name, &dev->pdev->dev);
	if (*raw == NULL || rc < 0) {
		DRM_ERROR("MSVDX: %s request_firmware failed: Reason %d\n",
			  name, rc);
		return 1;
	}

	if ((*raw)->size < sizeof(struct msvdx_fw)) {
		DRM_ERROR("MSVDX: %s is is not correct size(%zd)\n",
			  name, (*raw)->size);
		return 1;
	}

	ptr = (void *)((*raw))->data;

	if (!ptr) {
		DRM_ERROR("MSVDX: Failed to load %s\n", name);
		return 1;
	}

	/* another sanity check... */
	fw_size = sizeof(struct msvdx_fw) +
		  sizeof(uint32_t) * ((struct msvdx_fw *) ptr)->text_size +
		  sizeof(uint32_t) * ((struct msvdx_fw *) ptr)->data_size;
	if ((*raw)->size < fw_size) {
		DRM_ERROR("MSVDX: %s is is not correct size(%zd)\n",
			  name, (*raw)->size);
		return 1;
	}

	/* there is 4 byte split between text and data,
	 * also there is 4 byte guard after data */
	if (((struct msvdx_fw *)ptr)->text_size + 8 +
		((struct msvdx_fw *)ptr)->data_size >
		msvdx_priv->mtx_mem_size) {
		DRM_ERROR("MSVDX: fw size is bigger than mtx_mem_size.\n");
		return 1;
	}

	rc = ttm_bo_kmap(msvdx_priv->fw, 0, (msvdx_priv->fw)->num_pages, &tmp_kmap);
	if (rc) {
		PSB_DEBUG_GENERAL("drm_bo_kmap failed: %d\n", rc);
		ttm_bo_unref(&msvdx_priv->fw);
		ttm_bo_kunmap(&tmp_kmap);
		return 1;
	} else {
		uint32_t *last_word;
		gpu_addr = ttm_kmap_obj_virtual(&tmp_kmap, &is_iomem);

		memset(gpu_addr, UNINITILISE_MEM, msvdx_priv->mtx_mem_size);

		memcpy(gpu_addr, ptr + sizeof(struct msvdx_fw),
			sizeof(uint32_t) * ((struct msvdx_fw *)ptr)->text_size);

		memcpy(gpu_addr + (((struct msvdx_fw *) ptr)->data_location - MSVDX_MTX_DATA_LOCATION),
			(void *)ptr + sizeof(struct msvdx_fw) + sizeof(uint32_t) * ((struct msvdx_fw *)ptr)->text_size,
			sizeof(uint32_t) * ((struct msvdx_fw *)ptr)->data_size);

		last_word = (uint32_t *)(gpu_addr + msvdx_priv->mtx_mem_size - 4);
		/* Write a know value to last word in mtx memory*/
		/* Usefull for detection of stack overrun */
		*last_word = STACKGUARDWORD;
	}

	ttm_bo_kunmap(&tmp_kmap);
	PSB_DEBUG_GENERAL("MSVDX: releasing firmware resouces\n");
	PSB_DEBUG_GENERAL("MSVDX: Load firmware into BO successfully\n");
	release_firmware(*raw);
	return rc;
}

static uint32_t *msvdx_get_fw(struct drm_device *dev,
			      const struct firmware **raw, uint8_t *name)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	int rc, fw_size;
	void *ptr = NULL;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;

	rc = request_firmware(raw, name, &dev->pdev->dev);
	if (*raw == NULL || rc < 0) {
		DRM_ERROR("MSVDX: %s request_firmware failed: Reason %d\n",
			  name, rc);
		return NULL;
	}

	if ((*raw)->size < sizeof(struct msvdx_fw)) {
		DRM_ERROR("MSVDX: %s is is not correct size(%zd)\n",
			  name, (*raw)->size);
		release_firmware(*raw);
		return NULL;
	}

	ptr = (int *)((*raw))->data;

	if (!ptr) {
		DRM_ERROR("MSVDX: Failed to load %s\n", name);
		release_firmware(*raw);
		return NULL;
	}

	/* another sanity check... */
	fw_size = sizeof(struct msvdx_fw) +
		  sizeof(uint32_t) * ((struct msvdx_fw *) ptr)->text_size +
		  sizeof(uint32_t) * ((struct msvdx_fw *) ptr)->data_size;
	if ((*raw)->size < fw_size) {
		DRM_ERROR("MSVDX: %s is is not correct size(%zd)\n",
			  name, (*raw)->size);
		release_firmware(*raw);
		return NULL;
	} else if ((*raw)->size > fw_size) { /* there is ec firmware */
		ptr += ((fw_size + 0xfff) & ~0xfff);
		fw_size += (sizeof(struct msvdx_fw) +
			    sizeof(uint32_t) * ((struct msvdx_fw *) ptr)->text_size +
			    sizeof(uint32_t) * ((struct msvdx_fw *) ptr)->data_size);

		ptr = (int *)((*raw))->data;  /* Resotre ptr to start of the firmware file */
	}

	msvdx_priv->msvdx_fw = kzalloc(fw_size, GFP_KERNEL);
	if (msvdx_priv->msvdx_fw == NULL)
		DRM_ERROR("MSVDX: allocate FW buffer failed\n");
	else {
		memcpy(msvdx_priv->msvdx_fw, ptr, fw_size);
		msvdx_priv->msvdx_fw_size = fw_size;
	}

	PSB_DEBUG_GENERAL("MSVDX: releasing firmware resouces\n");
	release_firmware(*raw);

	return msvdx_priv->msvdx_fw;
}

void msvdx_write_mtx_core_reg(struct drm_psb_private *dev_priv,
			    const uint32_t core_reg, const uint32_t val)
{
	uint32_t reg = 0;

	/* Put data in MTX_RW_DATA */
	PSB_WMSVDX32(val, MTX_REGISTER_READ_WRITE_DATA_OFFSET);

	/* DREADY is set to 0 and request a write */
	reg = core_reg;
	REGIO_WRITE_FIELD_LITE(reg, MTX_REGISTER_READ_WRITE_REQUEST,
			       MTX_RNW, 0);
	REGIO_WRITE_FIELD_LITE(reg, MTX_REGISTER_READ_WRITE_REQUEST,
			       MTX_DREADY, 0);
	PSB_WMSVDX32(reg, MTX_REGISTER_READ_WRITE_REQUEST_OFFSET);

	psb_wait_for_register(dev_priv,
			      MTX_REGISTER_READ_WRITE_REQUEST_OFFSET,
			      MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK,
			      MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK,
			      2000000, 5);
}

int psb_setup_fw(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	uint32_t ram_bank_size;
	struct msvdx_private *msvdx_priv = dev_priv->msvdx_private;
	int ret = 0;
	struct msvdx_fw *fw;
	uint32_t *fw_ptr = NULL;
	uint32_t *text_ptr = NULL;
	uint32_t *data_ptr = NULL;
	const struct firmware *raw = NULL;
	int ec_firmware = 0;

	/* todo : Assert the clock is on - if not turn it on to upload code */
	PSB_DEBUG_GENERAL("MSVDX: psb_setup_fw\n");

	psb_msvdx_mtx_set_clocks(dev_priv->dev, clk_enable_all);

	/* Reset MTX */
	PSB_WMSVDX32(MTX_SOFT_RESET_MTX_RESET_MASK,
			MTX_SOFT_RESET_OFFSET);

	PSB_WMSVDX32(FIRMWAREID, MSVDX_COMMS_FIRMWARE_ID);

	PSB_WMSVDX32(0, MSVDX_COMMS_ERROR_TRIG);
	PSB_WMSVDX32(199, MTX_SYSC_TIMERDIV_OFFSET); /* MTX_SYSC_TIMERDIV */
	PSB_WMSVDX32(0, MSVDX_EXT_FW_ERROR_STATE); /* EXT_FW_ERROR_STATE */
	PSB_WMSVDX32(0, MSVDX_COMMS_MSG_COUNTER);
	PSB_WMSVDX32(0, MSVDX_COMMS_SIGNATURE);
	PSB_WMSVDX32(0, MSVDX_COMMS_TO_HOST_RD_INDEX);
	PSB_WMSVDX32(0, MSVDX_COMMS_TO_HOST_WRT_INDEX);
	PSB_WMSVDX32(0, MSVDX_COMMS_TO_MTX_RD_INDEX);
	PSB_WMSVDX32(0, MSVDX_COMMS_TO_MTX_WRT_INDEX);
	PSB_WMSVDX32(0, MSVDX_COMMS_FW_STATUS);
#ifndef CONFIG_SLICE_HEADER_PARSING
	PSB_WMSVDX32(RETURN_VDEB_DATA_IN_COMPLETION | NOT_ENABLE_ON_HOST_CONCEALMENT,
			MSVDX_COMMS_OFFSET_FLAGS);
#else
	/* decode flag should be set as 0 according to IMG's said */
	PSB_WMSVDX32(drm_decode_flag, MSVDX_COMMS_OFFSET_FLAGS);
#endif
	PSB_WMSVDX32(0, MSVDX_COMMS_SIGNATURE);

	/* read register bank size */
	{
		uint32_t bank_size, reg;
		reg = PSB_RMSVDX32(MSVDX_MTX_RAM_BANK_OFFSET);
		bank_size =
			REGIO_READ_FIELD(reg, MSVDX_MTX_RAM_BANK,
					 MTX_RAM_BANK_SIZE);
		ram_bank_size = (uint32_t)(1 << (bank_size + 2));
	}

	PSB_DEBUG_GENERAL("MSVDX: RAM bank size = %d bytes\n",
			  ram_bank_size);

	/* if FW already loaded from storage */
	if (msvdx_priv->msvdx_fw) {
		fw_ptr = msvdx_priv->msvdx_fw;
	} else {
#ifdef VXD_FW_BUILT_IN_KERNEL
		fw_ptr = msvdx_get_fw(dev, &raw, FIRMWARE_NAME);
#else
		fw_ptr = msvdx_get_fw(dev, &raw, "msvdx_fw_mfld_DE2.0.bin");
#endif
		PSB_DEBUG_GENERAL("MSVDX:load msvdx_fw_mfld_DE2.0.bin by udevd\n");
	}
	if (!fw_ptr) {
		DRM_ERROR("MSVDX:load msvdx_fw.bin failed,is udevd running?\n");
		ret = 1;
		goto out;
	}

	if (!msvdx_priv->is_load) { /* Load firmware into BO */
		PSB_DEBUG_GENERAL("MSVDX:load msvdx_fw.bin by udevd into BO\n");
#ifdef VXD_FW_BUILT_IN_KERNEL
		ret = msvdx_get_fw_bo(dev, &raw, FIRMWARE_NAME);
#else
		ret = msvdx_get_fw_bo(dev, &raw, "msvdx_fw_mfld_DE2.0.bin");
#endif
		if (ret) {
			DRM_ERROR("MSVDX: failed to call msvdx_get_fw_bo.\n");
			ret = 1;
			goto out;
		}
		msvdx_priv->is_load = 1;
	}

	fw = (struct msvdx_fw *) fw_ptr;

	if (ec_firmware) {
		fw_ptr += (((sizeof(struct msvdx_fw) + (fw->text_size + fw->data_size) * 4 + 0xfff) & ~0xfff) / sizeof(uint32_t));
		fw = (struct msvdx_fw *) fw_ptr;
	}

	/*
	if (fw->ver != 0x02) {
		DRM_ERROR("psb: msvdx_fw.bin firmware version mismatch,"
			"got version=%02x expected version=%02x\n",
			fw->ver, 0x02);
		ret = 1;
		goto out;
	}
	*/
	text_ptr =
		(uint32_t *)((uint8_t *) fw_ptr + sizeof(struct msvdx_fw));
	data_ptr = text_ptr + fw->text_size;

	if (fw->text_size == 2858)
		PSB_DEBUG_GENERAL(
		"MSVDX: FW ver 1.00.10.0187 of SliceSwitch variant\n");
	else if (fw->text_size == 3021)
		PSB_DEBUG_GENERAL(
		"MSVDX: FW ver 1.00.10.0187 of FrameSwitch variant\n");
	else if (fw->text_size == 2841)
		PSB_DEBUG_GENERAL("MSVDX: FW ver 1.00.10.0788\n");
	else if (fw->text_size == 3147)
		PSB_DEBUG_GENERAL("MSVDX: FW ver BUILD_DXVA_FW1.00.10.1042 of SliceSwitch variant\n");
	else if (fw->text_size == 3097)
		PSB_DEBUG_GENERAL("MSVDX: FW ver BUILD_DXVA_FW1.00.10.0963.02.0011 of FrameSwitch variant\n");
	else
		PSB_DEBUG_GENERAL("MSVDX: FW ver unknown\n");

	PSB_DEBUG_GENERAL("MSVDX: Retrieved pointers for firmware\n");
	PSB_DEBUG_GENERAL("MSVDX: text_size: %d\n", fw->text_size);
	PSB_DEBUG_GENERAL("MSVDX: data_size: %d\n", fw->data_size);
	PSB_DEBUG_GENERAL("MSVDX: data_location: 0x%x\n",
		fw->data_location);
	PSB_DEBUG_GENERAL("MSVDX: First 4 bytes of text: 0x%x\n",
		*text_ptr);
	PSB_DEBUG_GENERAL("MSVDX: First 4 bytes of data: 0x%x\n",
		*data_ptr);

	PSB_DEBUG_GENERAL("MSVDX: Uploading firmware\n");

#if UPLOAD_FW_BY_DMA
	msvdx_upload_fw(dev_priv, 0, msvdx_priv->mtx_mem_size / 4, ec_firmware);
#else
	msvdx_upload_fw(dev_priv, MTX_CORE_CODE_MEM, ram_bank_size,
			PC_START_ADDRESS - MTX_CODE_BASE, fw->text_size,
			text_ptr);
	msvdx_upload_fw(dev_priv, MTX_CORE_DATA_MEM, ram_bank_size,
			fw->data_location - MTX_DATA_BASE, fw->data_size,
			data_ptr);
#endif

#if 0
	/* todo :  Verify code upload possibly only in debug */
	ret = psb_verify_fw(dev_priv, ram_bank_size,
			MTX_CORE_CODE_MEM,
			PC_START_ADDRESS - MTX_CODE_BASE,
			fw->text_size, text_ptr);
	if (ret) {
		/* Firmware code upload failed */
		ret = 1;
		goto out;
	}

	ret = psb_verify_fw(dev_priv, ram_bank_size, MTX_CORE_DATA_MEM,
	fw->data_location - MTX_DATA_BASE,
	fw->data_size, data_ptr);
	if (ret) {
		/* Firmware data upload failed */
		ret = 1;
		goto out;
	}
#endif

	/*	-- Set starting PC address	*/
	msvdx_write_mtx_core_reg(dev_priv, MTX_PC, PC_START_ADDRESS);

	/*	-- Turn on the thread	*/
	PSB_WMSVDX32(MTX_ENABLE_MTX_ENABLE_MASK, MTX_ENABLE_OFFSET);

	/* Wait for the signature value to be written back */
	ret = psb_wait_for_register(dev_priv, MSVDX_COMMS_SIGNATURE,
				    MSVDX_COMMS_SIGNATURE_VALUE, /*Required value*/
				    0xffffffff, /* Enabled bits */
				    2000000, 5);
	if (ret) {
		DRM_ERROR("MSVDX: firmware fails to initialize.\n");
		goto out;
	}

	PSB_DEBUG_GENERAL("MSVDX: MTX Initial indications OK\n");
	PSB_DEBUG_GENERAL("MSVDX: MSVDX_COMMS_AREA_ADDR = %08x\n",
			  MSVDX_COMMS_AREA_ADDR);
#ifdef CONFIG_SLICE_HEADER_PARSING
	msvdx_rendec_init_by_msg(dev);
#endif
out:
	return ret;
}
