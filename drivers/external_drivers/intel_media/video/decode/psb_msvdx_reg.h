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
 *    Fei Jiang <fei.jiang@intel.com>
 *
 **************************************************************************/

#ifndef _PSB_MSVDX_REG_H_
#define _PSB_MSVDX_REG_H_

#ifdef CONFIG_DRM_VXD_BYT
#include "vxd_drv.h"
#else
#include "psb_drv.h"
#include "img_types.h"
#endif

#if (defined MFLD_MSVDX_FABRIC_DEBUG) && MFLD_MSVDX_FABRIC_DEBUG
#define PSB_WMSVDX32(_val, _offs)					\
do {									\
	if (psb_get_power_state(OSPM_VIDEO_DEC_ISLAND) == 0)		\
		panic("msvdx reg 0x%x write failed.\n",			\
				(unsigned int)(_offs));			\
	else								\
		iowrite32(_val, dev_priv->msvdx_reg + (_offs));		\
} while (0)

static inline uint32_t PSB_RMSVDX32(uint32_t _offs)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)gpDrmDevice->dev_private;
	if (psb_get_power_state(OSPM_VIDEO_DEC_ISLAND) == 0) {
		panic("msvdx reg 0x%x read failed.\n", (unsigned int)(_offs));
		return 0;
	} else {
		return ioread32(dev_priv->msvdx_reg + (_offs));
	}
}

#elif (defined MSVDX_REG_DUMP) && MSVDX_REG_DUMP

#define PSB_WMSVDX32(_val, _offs) \
do {                                                \
	printk(KERN_INFO"MSVDX: write %08x to reg 0x%08x\n", \
			(unsigned int)(_val),       \
			(unsigned int)(_offs));     \
	iowrite32(_val, dev_priv->msvdx_reg + (_offs));   \
} while (0)

static inline uint32_t PSB_RMSVDX32(uint32_t _offs)
{
	uint32_t val = ioread32(dev_priv->msvdx_reg + (_offs));
	printk(KERN_INFO"MSVDX: read reg 0x%08x, get %08x\n",
			(unsigned int)(_offs), val);
	return val;
}

#else

#define PSB_WMSVDX32(_val, _offs) \
	iowrite32(_val, dev_priv->msvdx_reg + (_offs))
#define PSB_RMSVDX32(_offs) \
	ioread32(dev_priv->msvdx_reg + (_offs))

#endif

#define REGISTER(__group__, __reg__) (__group__##_##__reg__##_OFFSET)

#define MTX_INTERNAL_REG(R_SPECIFIER , U_SPECIFIER)	\
	(((R_SPECIFIER)<<4) | (U_SPECIFIER))
#define MTX_PC		MTX_INTERNAL_REG(0, 5)

#define MEMIO_READ_FIELD(vpMem, field)						\
	((uint32_t)(((*((field##_TYPE*)(((uint32_t)vpMem) + field##_OFFSET))) \
			& field##_MASK) >> field##_SHIFT))			\

#define MEMIO_WRITE_FIELD(vpMem, field, value)					\
do { 										\
	((*((field##_TYPE*)(((uint32_t)vpMem) + field##_OFFSET))) =	\
		((*((field##_TYPE*)(((uint32_t)vpMem) + field##_OFFSET)))	\
			& (field##_TYPE)~field##_MASK) |			\
	(field##_TYPE)(((uint32_t)(value) << field##_SHIFT) & field##_MASK)); \
} while (0)

#define MEMIO_WRITE_FIELD_LITE(vpMem, field, value)				\
do {										\
	 (*((field##_TYPE*)(((uint32_t)vpMem) + field##_OFFSET))) =		\
	((*((field##_TYPE*)(((uint32_t)vpMem) + field##_OFFSET))) |		\
		(field##_TYPE)(((uint32_t)(value) << field##_SHIFT)));	\
} while (0)

#define REGIO_READ_FIELD(reg_val, reg, field)					\
	((reg_val & reg##_##field##_MASK) >> reg##_##field##_SHIFT)

#define REGIO_WRITE_FIELD(reg_val, reg, field, value)				\
do {										\
	(reg_val) =								\
	((reg_val) & ~(reg##_##field##_MASK)) |				\
	(((value) << (reg##_##field##_SHIFT)) & (reg##_##field##_MASK));	\
} while (0)


#define REGIO_WRITE_FIELD_LITE(reg_val, reg, field, value)			\
do {										\
	(reg_val) = ((reg_val) | ((value) << (reg##_##field##_SHIFT)));	\
} while (0)

/****** MSVDX.Technical Reference Manual.2.0.2.4.External VXD38x **************
Offset address 			Name 			Identifier
0x0000 - 0x03FF (1024B)		MTX Register		REG_MSVDX_MTX
0x0400 - 0x047F (128B)		VDMC Register		REG_MSVDX _VDMC
0x0480 - 0x04FF (128B)		VDEB Register		REG_MSVDX _VDEB
0x0500 - 0x05FF (256B)		DMAC Register		REG_MSVDX _DMAC
0x0600 - 0x06FF (256B)		MSVDX Core Register	REG_MSVDX _SYS
0x0700 - 0x07FF (256B)		VEC iQ Matrix RAM 	REG_MSVDX_VEC_IQRAM
0x0800 - 0x0FFF (2048B)		VEC Registers		REG_MSVDX _VEC
0x1000 - 0x1FFF (4kB)		Command Register	REG_MSVDX _CMD
0x2000 - 0x2FFF (4kB)		VEC Local RAM		REG_MSVDX _VEC_RAM
0x3000 - 0x4FFF (8kB)		VEC VLC Table		RAM REG_MSVDX _VEC_VLC
0x5000 - 0x5FFF (4kB)		AXI Register		REG_MSVDX _AXI
******************************************************************************/

/*************** MTX registers start: 0x0000 - 0x03FF (1024B) ****************/
#define MTX_ENABLE_OFFSET				(0x0000)
#define MTX_ENABLE_MTX_ENABLE_MASK				(0x00000001)
#define MTX_ENABLE_MTX_ENABLE_SHIFT				(0)

#define MTX_KICK_INPUT_OFFSET				(0x0080)

#define MTX_REGISTER_READ_WRITE_REQUEST_OFFSET		(0x00FC)
#define MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_MASK	(0x80000000)
#define MTX_REGISTER_READ_WRITE_REQUEST_MTX_DREADY_SHIFT	(31)
#define MTX_REGISTER_READ_WRITE_REQUEST_MTX_RNW_MASK		(0x00010000)
#define MTX_REGISTER_READ_WRITE_REQUEST_MTX_RNW_SHIFT		(16)

#define MTX_REGISTER_READ_WRITE_DATA_OFFSET		(0x00F8)

#define MTX_RAM_ACCESS_DATA_TRANSFER_OFFSET		(0x0104)

#define MTX_RAM_ACCESS_CONTROL_OFFSET			(0x0108)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCMID_MASK			(0x0FF00000)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCMID_SHIFT			(20)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCM_ADDR_MASK		(0x000FFFFC)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCM_ADDR_SHIFT		(2)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCMAI_MASK			(0x00000002)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCMAI_SHIFT			(1)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCMR_MASK			(0x00000001)
#define MTX_RAM_ACCESS_CONTROL_MTX_MCMR_SHIFT			(0)

#define MTX_RAM_ACCESS_STATUS_OFFSET			(0x010C)

#define MTX_SOFT_RESET_OFFSET				(0x0200)
#define MTX_SOFT_RESET_MTX_RESET_MASK				(0x00000001)
#define MTX_SOFT_RESET_MTX_RESET_SHIFT				(0)
#define	MTX_SOFT_RESET_MTXRESET				(0x00000001)

#define MTX_SYSC_TIMERDIV_OFFSET			(0x0208)

#define MTX_SYSC_CDMAC_OFFSET				(0x0340)
#define MTX_SYSC_CDMAC_BURSTSIZE_MASK				(0x07000000)
#define MTX_SYSC_CDMAC_BURSTSIZE_SHIFT				(24)
#define MTX_SYSC_CDMAC_RNW_MASK				(0x00020000)
#define MTX_SYSC_CDMAC_RNW_SHIFT				(17)
#define MTX_SYSC_CDMAC_ENABLE_MASK				(0x00010000)
#define MTX_SYSC_CDMAC_ENABLE_SHIFT				(16)
#define MTX_SYSC_CDMAC_LENGTH_MASK				(0x0000FFFF)
#define MTX_SYSC_CDMAC_LENGTH_SHIFT				(0)

#define MTX_SYSC_CDMAA_OFFSET				(0x0344)

#define MTX_SYSC_CDMAS0_OFFSET      			(0x0348)

#define MTX_SYSC_CDMAT_OFFSET				(0x0350)
/************************** MTX registers end **************************/

/**************** DMAC Registers: 0x0500 - 0x05FF (256B) ***************/
#define DMAC_DMAC_COUNT_EN_MASK         		(0x00010000)
#define DMAC_DMAC_IRQ_STAT_TRANSFER_FIN_MASK            (0x00020000)

#define DMAC_DMAC_SETUP_OFFSET				(0x0500)

#define DMAC_DMAC_COUNT_OFFSET				(0x0504)
#define DMAC_DMAC_COUNT_BSWAP_LSBMASK           		(0x00000001)
#define DMAC_DMAC_COUNT_BSWAP_SHIFT            		(30)
#define DMAC_DMAC_COUNT_PW_LSBMASK				(0x00000003)
#define DMAC_DMAC_COUNT_PW_SHIFT                		(27)
#define DMAC_DMAC_COUNT_DIR_LSBMASK				(0x00000001)
#define DMAC_DMAC_COUNT_DIR_SHIFT				(26)
#define DMAC_DMAC_COUNT_PI_LSBMASK				(0x00000003)
#define DMAC_DMAC_COUNT_PI_SHIFT				(24)
#define DMAC_DMAC_COUNT_CNT_LSBMASK				(0x0000FFFF)
#define DMAC_DMAC_COUNT_CNT_SHIFT				(0)
#define DMAC_DMAC_COUNT_EN_MASK				(0x00010000)
#define DMAC_DMAC_COUNT_EN_SHIFT				(16)

#define DMAC_DMAC_PERIPH_OFFSET				(0x0508)
#define DMAC_DMAC_PERIPH_ACC_DEL_LSBMASK			(0x00000007)
#define DMAC_DMAC_PERIPH_ACC_DEL_SHIFT				(29)
#define DMAC_DMAC_PERIPH_INCR_LSBMASK				(0x00000001)
#define DMAC_DMAC_PERIPH_INCR_SHIFT				(27)
#define DMAC_DMAC_PERIPH_BURST_LSBMASK				(0x00000007)
#define DMAC_DMAC_PERIPH_BURST_SHIFT				(24)

#define DMAC_DMAC_IRQ_STAT_OFFSET			(0x050C)
#define DMAC_DMAC_IRQ_STAT_TRANSFER_FIN_MASK			(0x00020000)

#define DMAC_DMAC_PERIPHERAL_ADDR_OFFSET		(0x0514)
#define DMAC_DMAC_PERIPHERAL_ADDR_ADDR_MASK			(0x007FFFFF)
#define DMAC_DMAC_PERIPHERAL_ADDR_ADDR_LSBMASK			(0x007FFFFF)
#define DMAC_DMAC_PERIPHERAL_ADDR_ADDR_SHIFT			(0)

/* DMAC control */
#define PSB_DMAC_VALUE_COUNT(BSWAP, PW, DIR, PERIPH_INCR, COUNT) 	\
		((((BSWAP) & DMAC_DMAC_COUNT_BSWAP_LSBMASK) <<	\
			DMAC_DMAC_COUNT_BSWAP_SHIFT) | 		\
		(((PW) & DMAC_DMAC_COUNT_PW_LSBMASK) <<		\
			DMAC_DMAC_COUNT_PW_SHIFT) | 			\
		(((DIR) & DMAC_DMAC_COUNT_DIR_LSBMASK) <<		\
			DMAC_DMAC_COUNT_DIR_SHIFT) |			\
		(((PERIPH_INCR) & DMAC_DMAC_COUNT_PI_LSBMASK) <<	\
			DMAC_DMAC_COUNT_PI_SHIFT) |			\
		(((COUNT) & DMAC_DMAC_COUNT_CNT_LSBMASK) <<		\
			DMAC_DMAC_COUNT_CNT_SHIFT))

#define PSB_DMAC_VALUE_PERIPH_PARAM(ACC_DEL, INCR, BURST)		\
		((((ACC_DEL) & DMAC_DMAC_PERIPH_ACC_DEL_LSBMASK) <<	\
			DMAC_DMAC_PERIPH_ACC_DEL_SHIFT) | 		\
		(((INCR) & DMAC_DMAC_PERIPH_INCR_LSBMASK) <<		\
			DMAC_DMAC_PERIPH_INCR_SHIFT) | 		\
		(((BURST) & DMAC_DMAC_PERIPH_BURST_LSBMASK) <<		\
			DMAC_DMAC_PERIPH_BURST_SHIFT))

typedef enum {
	/* !< No byte swapping will be performed. */
	PSB_DMAC_BSWAP_NO_SWAP = 0x0,
	/* !< Byte order will be reversed. */
	PSB_DMAC_BSWAP_REVERSE = 0x1,
} DMAC_eBSwap;

typedef enum {
	/* !< Data from memory to peripheral. */
	PSB_DMAC_DIR_MEM_TO_PERIPH = 0x0,
	/* !< Data from peripheral to memory. */
	PSB_DMAC_DIR_PERIPH_TO_MEM = 0x1,
} DMAC_eDir;

typedef enum {
	PSB_DMAC_ACC_DEL_0	= 0x0,	/* !< Access delay zero clock cycles */
	PSB_DMAC_ACC_DEL_256    = 0x1,	/* !< Access delay 256 clock cycles */
	PSB_DMAC_ACC_DEL_512    = 0x2,	/* !< Access delay 512 clock cycles */
	PSB_DMAC_ACC_DEL_768    = 0x3,	/* !< Access delay 768 clock cycles */
	PSB_DMAC_ACC_DEL_1024   = 0x4,	/* !< Access delay 1024 clock cycles */
	PSB_DMAC_ACC_DEL_1280   = 0x5,	/* !< Access delay 1280 clock cycles */
	PSB_DMAC_ACC_DEL_1536   = 0x6,	/* !< Access delay 1536 clock cycles */
	PSB_DMAC_ACC_DEL_1792   = 0x7,	/* !< Access delay 1792 clock cycles */
} DMAC_eAccDel;

typedef enum {
	PSB_DMAC_INCR_OFF	= 0,	/* !< Static peripheral address. */
	PSB_DMAC_INCR_ON	= 1,	/* !< Incrementing peripheral address. */
} DMAC_eIncr;

typedef enum {
	PSB_DMAC_BURST_0	= 0x0,	/* !< burst size of 0 */
	PSB_DMAC_BURST_1        = 0x1,	/* !< burst size of 1 */
	PSB_DMAC_BURST_2        = 0x2,	/* !< burst size of 2 */
	PSB_DMAC_BURST_3        = 0x3,	/* !< burst size of 3 */
	PSB_DMAC_BURST_4        = 0x4,	/* !< burst size of 4 */
	PSB_DMAC_BURST_5        = 0x5,	/* !< burst size of 5 */
	PSB_DMAC_BURST_6        = 0x6,	/* !< burst size of 6 */
	PSB_DMAC_BURST_7        = 0x7,	/* !< burst size of 7 */
} DMAC_eBurst;
/************************** DMAC Registers end **************************/

/**************** MSVDX Core Registers: 0x0600 - 0x06FF (256B) ***************/
#define MSVDX_CONTROL_OFFSET					(0x0600)
#define MSVDX_CONTROL_MSVDX_SOFT_RESET_MASK			(0x00000100)
#define MSVDX_CONTROL_MSVDX_SOFT_RESET_SHIFT			(8)
#define MSVDX_CONTROL_DMAC_CH0_SELECT_MASK			(0x00001000)
#define MSVDX_CONTROL_DMAC_CH0_SELECT_SHIFT			(12)
#define MSVDX_CONTROL_MSVDX_SOFT_RESET_MASK			(0x00000100)
#define MSVDX_CONTROL_MSVDX_SOFT_RESET_SHIFT			(8)
#define MSVDX_CONTROL_MSVDX_FE_SOFT_RESET_MASK			(0x00010000)
#define MSVDX_CONTROL_MSVDX_BE_SOFT_RESET_MASK			(0x00100000)
#define MSVDX_CONTROL_MSVDX_VEC_MEMIF_SOFT_RESET_MASK		(0x01000000)
#define MSVDX_CONTROL_MSVDX_VEC_RENDEC_DEC_SOFT_RESET_MASK 	(0x10000000)
#define msvdx_sw_reset_all \
	(MSVDX_CONTROL_MSVDX_SOFT_RESET_MASK |	  		\
	MSVDX_CONTROL_MSVDX_FE_SOFT_RESET_MASK |		\
	MSVDX_CONTROL_MSVDX_BE_SOFT_RESET_MASK	|		\
	MSVDX_CONTROL_MSVDX_VEC_MEMIF_SOFT_RESET_MASK |	\
	MSVDX_CONTROL_MSVDX_VEC_RENDEC_DEC_SOFT_RESET_MASK)

#define MSVDX_INTERRUPT_CLEAR_OFFSET			(0x060C)

#define MSVDX_INTERRUPT_STATUS_OFFSET			(0x0608)
#define MSVDX_INTERRUPT_STATUS_MMU_FAULT_IRQ_MASK		(0x00000F00)
#define MSVDX_INTERRUPT_STATUS_MMU_FAULT_IRQ_SHIFT		(8)
#define MSVDX_INTERRUPT_STATUS_MTX_IRQ_MASK			(0x00004000)
#define MSVDX_INTERRUPT_STATUS_MTX_IRQ_SHIFT			(14)

#define MSVDX_HOST_INTERRUPT_ENABLE_OFFSET		(0x0610)

#define MSVDX_MAN_CLK_ENABLE_OFFSET			(0x0620)
#define MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK		(0x00000001)
#define MSVDX_MAN_CLK_ENABLE_VDEB_PROCESS_MAN_CLK_ENABLE_MASK 	(0x00000002)
#define MSVDX_MAN_CLK_ENABLE_VDEB_ACCESS_MAN_CLK_ENABLE_MASK 	(0x00000004)
#define MSVDX_MAN_CLK_ENABLE_VDMC_MAN_CLK_ENABLE_MASK 		(0x00000008)
#define MSVDX_MAN_CLK_ENABLE_VEC_ENTDEC_MAN_CLK_ENABLE_MASK 	(0x00000010)
#define MSVDX_MAN_CLK_ENABLE_VEC_ITRANS_MAN_CLK_ENABLE_MASK 	(0x00000020)
#define MSVDX_MAN_CLK_ENABLE_MTX_MAN_CLK_ENABLE_MASK		(0x00000040)
#define MSVDX_MAN_CLK_ENABLE_VDEB_PROCESS_AUTO_CLK_ENABLE_MASK (0x00020000)
#define MSVDX_MAN_CLK_ENABLE_VDEB_ACCESS_AUTO_CLK_ENABLE_MASK	(0x00040000)
#define MSVDX_MAN_CLK_ENABLE_VDMC_AUTO_CLK_ENABLE_MASK 	(0x00080000)
#define MSVDX_MAN_CLK_ENABLE_VEC_ENTDEC_AUTO_CLK_ENABLE_MASK 	(0x00100000)
#define MSVDX_MAN_CLK_ENABLE_VEC_ITRANS_AUTO_CLK_ENABLE_MASK 	(0x00200000)

#define clk_enable_all	\
	(MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK			| \
	MSVDX_MAN_CLK_ENABLE_VDEB_PROCESS_MAN_CLK_ENABLE_MASK 		| \
	MSVDX_MAN_CLK_ENABLE_VDEB_ACCESS_MAN_CLK_ENABLE_MASK 		| \
	MSVDX_MAN_CLK_ENABLE_VDMC_MAN_CLK_ENABLE_MASK	 		| \
	MSVDX_MAN_CLK_ENABLE_VEC_ENTDEC_MAN_CLK_ENABLE_MASK 		| \
	MSVDX_MAN_CLK_ENABLE_VEC_ITRANS_MAN_CLK_ENABLE_MASK 		| \
	MSVDX_MAN_CLK_ENABLE_MTX_MAN_CLK_ENABLE_MASK)

#define clk_enable_minimal \
	(MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK | \
	MSVDX_MAN_CLK_ENABLE_MTX_MAN_CLK_ENABLE_MASK)

#define clk_enable_auto	\
	(MSVDX_MAN_CLK_ENABLE_VDEB_PROCESS_AUTO_CLK_ENABLE_MASK	| \
	MSVDX_MAN_CLK_ENABLE_VDEB_ACCESS_AUTO_CLK_ENABLE_MASK		| \
	MSVDX_MAN_CLK_ENABLE_VDMC_AUTO_CLK_ENABLE_MASK			| \
	MSVDX_MAN_CLK_ENABLE_VEC_ENTDEC_AUTO_CLK_ENABLE_MASK		| \
	MSVDX_MAN_CLK_ENABLE_VEC_ITRANS_AUTO_CLK_ENABLE_MASK		| \
	MSVDX_MAN_CLK_ENABLE_CORE_MAN_CLK_ENABLE_MASK			| \
	MSVDX_MAN_CLK_ENABLE_MTX_MAN_CLK_ENABLE_MASK)

#define MSVDX_CORE_ID_OFFSET				(0x0630)
#define MSVDX_CORE_REV_OFFSET				(0x0640)

#define MSVDX_DMAC_STREAM_STATUS_OFFSET			(0x0648)

#define MSVDX_MMU_CONTROL0_OFFSET			(0x0680)
#define MSVDX_MMU_CONTROL0_MMU_PAUSE_MASK			(0x00000002)
#define MSVDX_MMU_CONTROL0_MMU_PAUSE_SHIFT			(1)
#define MSVDX_MMU_CONTROL0_MMU_INVALDC_MASK          		(0x00000008)
#define MSVDX_MMU_CONTROL0_MMU_INVALDC_SHIFT         		(3)

#define MSVDX_MMU_BANK_INDEX_OFFSET			(0x0688)

#define MSVDX_MMU_STATUS_OFFSET				(0x068C)

#define MSVDX_MMU_CONTROL2_OFFSET			(0x0690)

#define MSVDX_MMU_DIR_LIST_BASE_OFFSET			(0x0694)

#define MSVDX_MMU_MEM_REQ_OFFSET			(0x06D0)

#define MSVDX_MMU_TILE_BASE0_OFFSET			(0x06D4)

#define MSVDX_MMU_TILE_BASE1_OFFSET			(0x06D8)

#define MSVDX_MTX_RAM_BANK_OFFSET			(0x06F0)
#define MSVDX_MTX_RAM_BANK_MTX_RAM_BANK_SIZE_MASK		(0x000F0000)
#define MSVDX_MTX_RAM_BANK_MTX_RAM_BANK_SIZE_SHIFT		(16)

#define MSVDX_MTX_DEBUG_OFFSET				MSVDX_MTX_RAM_BANK_OFFSET
#define MSVDX_MTX_DEBUG_MTX_DBG_IS_SLAVE_MASK			(0x00000004)
#define MSVDX_MTX_DEBUG_MTX_DBG_IS_SLAVE_LSBMASK		(0x00000001)
#define MSVDX_MTX_DEBUG_MTX_DBG_IS_SLAVE_SHIFT			(2)
#define MSVDX_MTX_DEBUG_MTX_DBG_GPIO_IN_MASK			(0x00000003)
#define MSVDX_MTX_DEBUG_MTX_DBG_GPIO_IN_LSBMASK		(0x00000003)
#define MSVDX_MTX_DEBUG_MTX_DBG_GPIO_IN_SHIFT			(0)

/*watch dog for FE and BE*/
#define FE_MSVDX_WDT_CONTROL_OFFSET			(0x0664)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_CONTROL, FE_WDT_CNT_CTRL */
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CNT_CTRL_MASK		(0x00060000)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CNT_CTRL_LSBMASK		(0x00000003)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CNT_CTRL_SHIFT		(17)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_CONTROL, FE_WDT_ENABLE */
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ENABLE_MASK		(0x00010000)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ENABLE_LSBMASK		(0x00000001)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ENABLE_SHIFT		(16)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_CONTROL, FE_WDT_ACTION1 */
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ACTION1_MASK		(0x00003000)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ACTION1_LSBMASK		(0x00000003)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ACTION1_SHIFT		(12)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_CONTROL, FE_WDT_ACTION0 */
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ACTION0_MASK		(0x00000100)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ACTION0_LSBMASK		(0x00000001)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_ACTION0_SHIFT		(8)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_CONTROL, FE_WDT_CLEAR_SELECT */
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CLEAR_SELECT_MASK		(0x00000030)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CLEAR_SELECT_LSBMASK	(0x00000003)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CLEAR_SELECT_SHIFT		(4)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_CONTROL, FE_WDT_CLKDIV_SELECT */
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CLKDIV_SELECT_MASK		(0x00000007)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CLKDIV_SELECT_LSBMASK	(0x00000007)
#define FE_MSVDX_WDT_CONTROL_FE_WDT_CLKDIV_SELECT_SHIFT	(0)

#define FE_MSVDX_WDTIMER_OFFSET				(0x0668)
/* MSVDX_CORE, CR_FE_MSVDX_WDTIMER, FE_WDT_COUNTER */
#define FE_MSVDX_WDTIMER_FE_WDT_COUNTER_MASK			(0x0000FFFF)
#define FE_MSVDX_WDTIMER_FE_WDT_COUNTER_LSBMASK		(0x0000FFFF)
#define FE_MSVDX_WDTIMER_FE_WDT_COUNTER_SHIFT			(0)

#define FE_MSVDX_WDT_COMPAREMATCH_OFFSET		(0x066c)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_COMPAREMATCH, FE_WDT_CM1 */
#define FE_MSVDX_WDT_COMPAREMATCH_FE_WDT_CM1_MASK		(0xFFFF0000)
#define FE_MSVDX_WDT_COMPAREMATCH_FE_WDT_CM1_LSBMASK		(0x0000FFFF)
#define FE_MSVDX_WDT_COMPAREMATCH_FE_WDT_CM1_SHIFT		(16)
/* MSVDX_CORE, CR_FE_MSVDX_WDT_COMPAREMATCH, FE_WDT_CM0 */
#define FE_MSVDX_WDT_COMPAREMATCH_FE_WDT_CM0_MASK		(0x0000FFFF)
#define FE_MSVDX_WDT_COMPAREMATCH_FE_WDT_CM0_LSBMASK		(0x0000FFFF)
#define FE_MSVDX_WDT_COMPAREMATCH_FE_WDT_CM0_SHIFT		(0)

#define BE_MSVDX_WDT_CONTROL_OFFSET			(0x0670)
/* MSVDX_CORE, CR_BE_MSVDX_WDT_CONTROL, BE_WDT_CNT_CTRL */
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CNT_CTRL_MASK		(0x001E0000)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CNT_CTRL_LSBMASK		(0x0000000F)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CNT_CTRL_SHIFT		(17)
/* MSVDX_CORE, CR_BE_MSVDX_WDT_CONTROL, BE_WDT_ENABLE */
#define BE_MSVDX_WDT_CONTROL_BE_WDT_ENABLE_MASK		(0x00010000)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_ENABLE_LSBMASK		(0x00000001)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_ENABLE_SHIFT		(16)
/* MSVDX_CORE, CR_BE_MSVDX_WDT_CONTROL, BE_WDT_ACTION0 */
#define BE_MSVDX_WDT_CONTROL_BE_WDT_ACTION0_MASK		(0x00000100)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_ACTION0_LSBMASK		(0x00000001)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_ACTION0_SHIFT		(8)
/* MSVDX_CORE, CR_BE_MSVDX_WDT_CONTROL, BE_WDT_CLEAR_SELECT */
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CLEAR_SELECT_MASK		(0x000000F0)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CLEAR_SELECT_LSBMASK	(0x0000000F)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CLEAR_SELECT_SHIFT		(4)
/* MSVDX_CORE, CR_BE_MSVDX_WDT_CONTROL, BE_WDT_CLKDIV_SELECT */
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CLKDIV_SELECT_MASK		(0x00000007)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CLKDIV_SELECT_LSBMASK	(0x00000007)
#define BE_MSVDX_WDT_CONTROL_BE_WDT_CLKDIV_SELECT_SHIFT	(0)

#define BE_MSVDX_WDTIMER_OFFSET				(0x0674)
/* MSVDX_CORE, CR_BE_MSVDX_WDTIMER, BE_WDT_COUNTER */
#define BE_MSVDX_WDTIMER_BE_WDT_COUNTER_MASK			(0x0000FFFF)
#define BE_MSVDX_WDTIMER_BE_WDT_COUNTER_LSBMASK		(0x0000FFFF)
#define BE_MSVDX_WDTIMER_BE_WDT_COUNTER_SHIFT			(0)

#define BE_MSVDX_WDT_COMPAREMATCH_OFFSET		(0x678)
/* MSVDX_CORE, CR_BE_MSVDX_WDT_COMPAREMATCH, BE_WDT_CM0 */
#define BE_MSVDX_WDT_COMPAREMATCH_BE_WDT_CM0_MASK		(0x0000FFFF)
#define BE_MSVDX_WDT_COMPAREMATCH_BE_WDT_CM0_LSBMASK		(0x0000FFFF)
#define BE_MSVDX_WDT_COMPAREMATCH_BE_WDT_CM0_SHIFT		(0)

/*watch dog end*/
/************************** MSVDX Core Registers end *************************/

/******************* VEC Registers: 0x0800 - 0x0FFF (2048B) ******************/
#define VEC_SHIFTREG_CONTROL_OFFSET			(0x0818)
#define VEC_SHIFTREG_CONTROL_SR_MASTER_SELECT_MASK		(0x00000300)
#define VEC_SHIFTREG_CONTROL_SR_MASTER_SELECT_SHIFT		(8)
/************************** VEC Registers end **************************/

/************************** RENDEC Registers **************************/
#define MSVDX_RENDEC_CONTROL0_OFFSET			(0x0868)
#define MSVDX_RENDEC_CONTROL0_RENDEC_INITIALISE_MASK		(0x00000001)
#define MSVDX_RENDEC_CONTROL0_RENDEC_INITIALISE_SHIFT		(0)

#define MSVDX_RENDEC_CONTROL1_OFFSET			(0x086C)
#define MSVDX_RENDEC_CONTROL1_RENDEC_DECODE_START_SIZE_MASK	(0x000000FF)
#define MSVDX_RENDEC_CONTROL1_RENDEC_DECODE_START_SIZE_SHIFT	(0)
#define MSVDX_RENDEC_CONTROL1_RENDEC_BURST_SIZE_W_MASK		(0x000C0000)
#define MSVDX_RENDEC_CONTROL1_RENDEC_BURST_SIZE_W_SHIFT		(18)
#define MSVDX_RENDEC_CONTROL1_RENDEC_BURST_SIZE_R_MASK		(0x00030000)
#define MSVDX_RENDEC_CONTROL1_RENDEC_BURST_SIZE_R_SHIFT		(16)
#define MSVDX_RENDEC_CONTROL1_RENDEC_EXTERNAL_MEMORY_MASK	(0x01000000)
#define MSVDX_RENDEC_CONTROL1_RENDEC_EXTERNAL_MEMORY_SHIFT	(24)
#define MSVDX_RENDEC_CONTROL1_RENDEC_DEC_DISABLE_MASK		(0x08000000)
#define MSVDX_RENDEC_CONTROL1_RENDEC_DEC_DISABLE_SHIFT		(27)

#define MSVDX_RENDEC_BUFFER_SIZE_OFFSET			(0x0870)
#define MSVDX_RENDEC_BUFFER_SIZE_RENDEC_BUFFER_SIZE0_MASK	(0x0000FFFF)
#define MSVDX_RENDEC_BUFFER_SIZE_RENDEC_BUFFER_SIZE0_SHIFT	(0)
#define MSVDX_RENDEC_BUFFER_SIZE_RENDEC_BUFFER_SIZE1_MASK	(0xFFFF0000)
#define MSVDX_RENDEC_BUFFER_SIZE_RENDEC_BUFFER_SIZE1_SHIFT	(16)

#define MSVDX_RENDEC_BASE_ADDR0_OFFSET			(0x0874)

#define MSVDX_RENDEC_BASE_ADDR1_OFFSET			(0x0878)

#define MSVDX_RENDEC_READ_DATA_OFFSET			(0x0898)

#define MSVDX_RENDEC_CONTEXT0_OFFSET			(0x0950)

#define MSVDX_RENDEC_CONTEXT1_OFFSET			(0x0954)

#define MSVDX_RENDEC_CONTEXT2_OFFSET			(0x0958)

#define MSVDX_RENDEC_CONTEXT3_OFFSET			(0x095C)

#define MSVDX_RENDEC_CONTEXT4_OFFSET			(0x0960)

#define MSVDX_RENDEC_CONTEXT5_OFFSET			(0x0964)
/*************************** RENDEC registers end ****************************/

/******************** VEC Local RAM: 0x2000 - 0x2FFF (4kB) *******************/
/* vec local MEM save/restore */
#define VEC_LOCAL_MEM_BYTE_SIZE (4 * 1024)
#define VEC_LOCAL_MEM_OFFSET 0x2000

#define MSVDX_EXT_FW_ERROR_STATE 		(0x2CC4)
/* Decode operations in progress or not complete */
#define MSVDX_FW_STATUS_IN_PROGRESS			0x00000000
/* there's no work underway on the hardware, idle, can be powered down */
#define MSVDX_FW_STATUS_HW_IDLE				0x00000001
/* Panic, waiting to be reloaded */
#define MSVDX_FW_STATUS_HW_PANIC			0x00000003

/*
 * This defines the MSVDX communication buffer
 */
#define MSVDX_COMMS_SIGNATURE_VALUE	(0xA5A5A5A5)	/*!< Signature value */
/*!< Host buffer size (in 32-bit words) */
#define NUM_WORDS_HOST_BUF		(100)
/*!< MTX buffer size (in 32-bit words) */
#define NUM_WORDS_MTX_BUF		(100)

#define MSVDX_COMMS_AREA_ADDR			(0x02fe0)
#define MSVDX_COMMS_CORE_WTD			(MSVDX_COMMS_AREA_ADDR - 0x08)
#define MSVDX_COMMS_ERROR_TRIG			(MSVDX_COMMS_AREA_ADDR - 0x08)
#define MSVDX_COMMS_FIRMWARE_ID			(MSVDX_COMMS_AREA_ADDR - 0x0C)
#define MSVDX_COMMS_OFFSET_FLAGS		(MSVDX_COMMS_AREA_ADDR + 0x18)
#define	MSVDX_COMMS_MSG_COUNTER			(MSVDX_COMMS_AREA_ADDR - 0x04)
#define MSVDX_COMMS_FW_STATUS			(MSVDX_COMMS_AREA_ADDR - 0x10)
#define	MSVDX_COMMS_SIGNATURE			(MSVDX_COMMS_AREA_ADDR + 0x00)
#define	MSVDX_COMMS_TO_HOST_BUF_SIZE		(MSVDX_COMMS_AREA_ADDR + 0x04)
#define MSVDX_COMMS_TO_HOST_RD_INDEX		(MSVDX_COMMS_AREA_ADDR + 0x08)
#define MSVDX_COMMS_TO_HOST_WRT_INDEX		(MSVDX_COMMS_AREA_ADDR + 0x0C)
#define MSVDX_COMMS_TO_MTX_BUF_SIZE		(MSVDX_COMMS_AREA_ADDR + 0x10)
#define MSVDX_COMMS_TO_MTX_RD_INDEX		(MSVDX_COMMS_AREA_ADDR + 0x14)
#define MSVDX_COMMS_TO_MTX_CB_RD_INDEX		(MSVDX_COMMS_AREA_ADDR + 0x18)
#define MSVDX_COMMS_TO_MTX_WRT_INDEX		(MSVDX_COMMS_AREA_ADDR + 0x1C)
#define MSVDX_COMMS_TO_HOST_BUF			(MSVDX_COMMS_AREA_ADDR + 0x20)
#define MSVDX_COMMS_TO_MTX_BUF	\
			(MSVDX_COMMS_TO_HOST_BUF + (NUM_WORDS_HOST_BUF << 2))

/*
 * FW FLAGs: it shall be written by the host prior to starting the Firmware.
 */
/* Disable Firmware based Watch dog timers. */
#define DSIABLE_FW_WDT				0x0008
	/* Abort Immediately on errors */
#define ABORT_ON_ERRORS_IMMEDIATE		0x0010
	/* Aborts faulted slices as soon as possible. Allows non faulted slices to
	 * reach backend but the faulted slice will not be allowed to start. */
#define ABORT_FAULTED_SLICE_IMMEDIATE		0x0020
	/* Flush faulted slices - Debug option */
#define FLUSH_FAULTED_SLICES			0x0080
	/* Don't interrupt host when to host buffer becomes full.
	 * Stall until space is freed up by host on it's own. */
#define NOT_INTERRUPT_WHEN_HOST_IS_FULL		0x0200
	/* Contiguity warning msg will be send to host for stream with
         * FW_ERROR_DETECTION_AND_RECOVERY flag set if non-contiguous
	 * macroblocks are detected. */
#define NOT_ENABLE_ON_HOST_CONCEALMENT		0x0400
	/* Return VDEB Signature Value in Completion message.
	 * This requires a VDEB data flush every slice for constant results.*/
#define RETURN_VDEB_DATA_IN_COMPLETION		0x0800
	/* Disable Auto Clock Gating. */
#define DSIABLE_Auto_CLOCK_GATING		0x1000
	/* Disable Idle GPIO signal. */
#define DSIABLE_IDLE_GPIO_SIG			0x2000
	/* Enable Setup, FE and BE Time stamps in completion message.
	 * Used by IMG only for firmware profiling. */
#define ENABLE_TIMESTAMPS_IN_COMPLETE_MSG	0x4000
	/* Disable off-host 2nd pass Deblocking in Firmware.  */
#define DSIABLE_OFFHOST_SECOND_DEBLOCK		0x20000
	/* Sum address signature to data signature
	 * when returning VDEB signature values. */
#define SUM_ADD_SIG_TO_DATA_SIGNATURE		0x80000

/*
#define MSVDX_COMMS_AREA_END	\
  (MSVDX_COMMS_TO_MTX_BUF + (NUM_WORDS_HOST_BUF << 2))
*/
#define MSVDX_COMMS_AREA_END 0x03000

#if (MSVDX_COMMS_AREA_END != 0x03000)
#error
#endif
/***************************** VEC Local RAM end *****************************/

#endif
