/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/

/*!
 * \file dx_cc_regs.h
 * \brief Macro definitions for accessing Dx CryptoCell register space
 *
 * For SeP code define DX_CC_SEP
 * For Host physical/direct registers access define DX_CC_HOST
 * For Host virtual mapping of registers define DX_CC_HOST_VIRT
 */

#ifndef _DX_CC_REGS_H_
#define _DX_CC_REGS_H_

#include "dx_bitops.h"

/* Include register base addresses data */
#if defined(DX_CC_SEP)
#include "dx_reg_base_sep.h"

#elif defined(DX_CC_HOST) || defined(DX_CC_HOST_VIRT) || defined(DX_CC_TEE)
#include "dx_reg_base_host.h"

#else
#error Define either DX_CC_SEP or DX_CC_HOST or DX_CC_HOST_VIRT
#endif

/* CC registers address calculation */
#if defined(DX_CC_SEP)
#define DX_CC_REG_ADDR(unit_name, reg_name)            \
	 (DX_BASE_CC_PERIF + DX_BASE_ ## unit_name + \
	  DX_ ## reg_name ## _REG_OFFSET)

/* In host macros we ignore the unit_name because all offsets are from base */
#elif defined(DX_CC_HOST)
#define DX_CC_REG_ADDR(unit_name, reg_name)            \
	(DX_BASE_CC + DX_ ## reg_name ## _REG_OFFSET)

#elif defined(DX_CC_TEE)
#define DX_CC_REG_ADDR(unit_name, reg_name)            \
	(DX_BASE_CC + DX_ ## reg_name ## _REG_OFFSET)

#elif defined(DX_CC_HOST_VIRT)
#define DX_CC_REG_ADDR(cc_base_virt, unit_name, reg_name) \
	(((unsigned long)(cc_base_virt)) + DX_ ## reg_name ## _REG_OFFSET)

#endif

/* Register Offset macros (from registers base address in host) */
#if defined(DX_CC_HOST) || defined(DX_CC_HOST_VIRT)

#define DX_CC_REG_OFFSET(reg_domain, reg_name)               \
	(DX_ ## reg_domain ## _ ## reg_name ## _REG_OFFSET)

/* Indexed GPR offset macros - note the (not original) preprocessor tricks...*/
/* (Using the macro without the "_" prefix is allowed with another macro      *
 *  as the gpr_idx) */
#define _SEP_HOST_GPR_REG_OFFSET(gpr_idx) \
	DX_CC_REG_OFFSET(HOST, SEP_HOST_GPR ## gpr_idx)
#define SEP_HOST_GPR_REG_OFFSET(gpr_idx) _SEP_HOST_GPR_REG_OFFSET(gpr_idx)
#define _HOST_SEP_GPR_REG_OFFSET(gpr_idx) \
	DX_CC_REG_OFFSET(HOST, HOST_SEP_GPR ## gpr_idx)
#define HOST_SEP_GPR_REG_OFFSET(gpr_idx) _HOST_SEP_GPR_REG_OFFSET(gpr_idx)

/* GPR IRQ bit mask by GPR index */
#define _SEP_HOST_GPR_IRQ_MASK(gpr_idx) \
	(1 << DX_HOST_IRR_SEP_HOST_GPR ## gpr_idx ## _INT_BIT_SHIFT)
#define SEP_HOST_GPR_IRQ_MASK(gpr_idx) _SEP_HOST_GPR_IRQ_MASK(gpr_idx)

#elif defined(DX_CC_SEP)

#define DX_CC_REG_OFFSET(unit_name, reg_name)               \
	(DX_BASE_ ## unit_name + DX_ ## reg_name ## _REG_OFFSET)

/* Indexed GPR address macros - note the (not original) preprocessor tricks...*/
/* (Using the macro without the "_" prefix is allowed with another macro      *
 *  as the gpr_idx) */
#define _SEP_HOST_GPR_REG_ADDR(gpr_idx) \
	DX_CC_REG_ADDR(SEP_RGF, SEP_SEP_HOST_GPR ## gpr_idx)
#define SEP_HOST_GPR_REG_ADDR(gpr_idx) _SEP_HOST_GPR_REG_ADDR(gpr_idx)
#define _HOST_SEP_GPR_REG_ADDR(gpr_idx) \
	DX_CC_REG_ADDR(SEP_RGF, SEP_HOST_SEP_GPR ## gpr_idx)
#define HOST_SEP_GPR_REG_ADDR(gpr_idx) _HOST_SEP_GPR_REG_ADDR(gpr_idx)

#elif defined(DX_CC_TEE)

#define DX_CC_REG_OFFSET(unit_name, reg_name) \
	(DX_BASE_ ## unit_name + DX_ ## reg_name ## _REG_OFFSET)

#else
#error "Undef exec domain,not DX_CC_SEP, DX_CC_HOST, DX_CC_HOST_VIRT, DX_CC_TEE"
#endif

/* Registers address macros for ENV registers (development FPGA only) */
#ifdef DX_BASE_ENV_REGS

#if defined(DX_CC_HOST)
#define DX_ENV_REG_ADDR(reg_name) \
	(DX_BASE_ENV_REGS + DX_ENV_ ## reg_name ## _REG_OFFSET)

#elif defined(DX_CC_HOST_VIRT)
/* The OS driver resource address space covers the ENV registers, too */
/* Since DX_BASE_ENV_REGS is given in absolute address, we calc. the distance */
#define DX_ENV_REG_ADDR(cc_base_virt, reg_name) \
	(((cc_base_virt) + (DX_BASE_ENV_REGS - DX_BASE_CC)) + \
	 DX_ENV_ ## reg_name ## _REG_OFFSET)

#endif

#endif				/*DX_BASE_ENV_REGS */

/* Bit fields access */
#define DX_CC_REG_FLD_GET(unit_name, reg_name, fld_name, reg_val)	      \
	(DX_ ## reg_name ## _ ## fld_name ## _BIT_SIZE == 0x20 ?	      \
	reg_val /* Optimization for 32b fields */ :			      \
	BITFIELD_GET(reg_val, DX_ ## reg_name ## _ ## fld_name ## _BIT_SHIFT, \
		     DX_ ## reg_name ## _ ## fld_name ## _BIT_SIZE))

#define DX_CC_REG_FLD_SET(                                               \
	unit_name, reg_name, fld_name, reg_shadow_var, new_fld_val)      \
do {                                                                     \
	if (DX_ ## reg_name ## _ ## fld_name ## _BIT_SIZE == 0x20)       \
		reg_shadow_var = new_fld_val; /* Optimization for 32b fields */\
	else                                                             \
		BITFIELD_SET(reg_shadow_var,                             \
			DX_ ## reg_name ## _ ## fld_name ## _BIT_SHIFT,  \
			DX_ ## reg_name ## _ ## fld_name ## _BIT_SIZE,   \
			new_fld_val);                                    \
} while (0)

/* Usage example:
   u32 reg_shadow = READ_REGISTER(DX_CC_REG_ADDR(CRY_KERNEL,AES_CONTROL));
   DX_CC_REG_FLD_SET(CRY_KERNEL,AES_CONTROL,NK_KEY0,reg_shadow, 3);
   DX_CC_REG_FLD_SET(CRY_KERNEL,AES_CONTROL,NK_KEY1,reg_shadow, 1);
   WRITE_REGISTER(DX_CC_REG_ADDR(CRY_KERNEL,AES_CONTROL), reg_shadow);
 */

#endif /*_DX_CC_REGS_H_*/
