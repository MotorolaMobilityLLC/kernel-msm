/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Jim Bish <jim.bish@intel.com>
 */

#include <drm/drmP.h>
#include <drm/drm.h>
#include "psb_drm.h"
#include "psb_drv.h"
#include "psb_fb.h"
#include "psb_reg.h"
#include "psb_intel_reg.h"
#include "psb_msvdx.h"
#include "pnw_topaz.h"
#include <drm/drm_pciids.h>
#include "pvr_drm_shared.h"
#include "psb_powermgmt.h"
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_MDFD_GL3

/*
 * GL3 Control
 */

#define MDFLD_GCL_CR_CTL2		0xB0000
/* address at location is the address that had the error */
#define MDFLD_GCL_ERR_ADDR		0xB0004
/*GL3 ECO Register*/
#define MDFLD_GCL_CR_ECO		0xB0030
#define MDFLD_GCL_ERR_STATUS		0xB0008

/* unmask bit 21 to get GL3 interrupts */
#define MDFLD_IMG_MASK			0x20A8
/*
 * GL3 registers and bits
 */

#define MDFLD_GL3_CONTROL				0x2100
#define MDFLD_GL3_USE_WRT_INVAL			0x2104
#define MDFLD_GL3_STATUS				0x2108
#define MDFLD_GL3_G_CONTROL				0x20FC
#define MDFLD_GL3_SOFT_RESET_ENABLE		(1<<4)

#define MDFLD_GL3_DISABLE				(1<<31)
#define MDFLD_GL3_BYP_PREQ2_USSE3 		(1<<29)
#define MDLFD_GL3_BYP_PREQ2_USSE2 		(1<<28)
#define MDFLD_GL3_BYP_PREQ2_PDS			(1<<27)
#define MDFLD_GL3_BYP_PREQ2_USEC		(1<<26)
#define MDFLD_GL3_FLUSH					(1<<25)
#define MDFLD_GL3_FLUSH_CTL				(1<<24)
#define MDFLD_GL3_BYP_CPU_COH			(1<<23)
#define MDFLD_GL3_BYP_VED				(1<<21)
#define MDFLD_GL3_BYP_VEC				(1<<20)
#define MDFLD_GL3_BYP_GFX				(1<<19)
#define MDFLD_GL3_BYP_PREQ1_USE1		(1<<18)
#define MDFLD_GL3_BYP_PREQ1_USE0		(1<<17)
#define MDFLD_GL3_BYP_PREQ1_ISPZ		(1<<16)
#define MDFLD_GL3_BYP_PREQ1_ISPP 		(1<<15)
#define MDFLD_GL3_BYP_PREQ1_TSPP		(1<<14)
#define MDFLD_GL3_BYP_PREQ1_PBE			(1<<13)
#define MDFLD_GL3_BYP_PREQ1_VDM			(1<<12)
#define MDFLD_GL3_BYP_PREQ1_TA			(1<<11)
#define MDFLD_GL3_BYP_PREQ1_MADD		(1<<10)
#define MDFLD_GL3_BYP_PREQ1_MMU			(1<<9)
#define MDFLD_GL3_USE_INVAL_REQ_USSE3	(1<<8)
#define MDFLD_GL3_USE_INVAL_REQ_USSE2	(1<<7)
#define MDFLD_GL3_USE_INVAL_REQ_USSE1	(1<<6)
#define MDFLD_GL3_USE_INVAL_REQ_USSE0	(1<<5)
#define MDFLD_GL3_BYP_WR				(1<<4)
#define MDFLD_GL3_IGN_VED_HINT			(1<<3)
#define MDFLD_GL3_IGN_VEC_HINT			(1<<2)
#define MDFLD_GL3_INVALIDATE			(1<<1)
#define MDFLD_GL3_PAUSE					(1)

/*
 * GL3 Masks
 */

#define MDFLD_GL3_ENABLE_CACHE (MDFLD_GL3_BYP_PREQ2_USSE3 | MDLFD_GL3_BYP_PREQ2_USSE2 | \
	MDFLD_GL3_BYP_PREQ1_USE1 | MDFLD_GL3_BYP_PREQ1_USE0 | \
	MDFLD_GL3_BYP_PREQ1_ISPZ | MDFLD_GL3_BYP_PREQ1_PBE | MDFLD_GL3_BYP_PREQ1_VDM | \
	MDFLD_GL3_BYP_PREQ1_TA | MDFLD_GL3_BYP_PREQ1_MMU | MDFLD_GL3_USE_INVAL_REQ_USSE3 | \
	MDFLD_GL3_USE_INVAL_REQ_USSE2 | MDFLD_GL3_USE_INVAL_REQ_USSE1 | \
	MDFLD_GL3_USE_INVAL_REQ_USSE0)

#define MDFLD_GL3_INVALIDATE_CACHE (MDFLD_GL3_ENABLE_CACHE | MDFLD_GL3_INVALIDATE)

#define MDFLD_GL3_FLUSH_CACHE (MDFLD_GL3_ENABLE_CACHE | MDFLD_GL3_FLUSH)

#define MDFLD_GL3_DISABLE_CACHE (MDFLD_GL3_ENABLE_CACHE | MDFLD_GL3_DISABLE)

/*
	GL3 attributes controlled via GCL_CR_CTL2
*/
#define GCL_CR_CTL2_WRTHRU_ENA				(1)
#define GCL_CR_CTL2_TAG_ECC_CHECK_ENABLE		(1<<2)
#define GCL_CR_CTL2_GL3_GL3Q_GATING_DISABLE		(1<<8)
#define GCL_CR_CTL2_GL3_GCL_GATING_DISABLE		(1<<9)
#define GCL_CR_CTL2_GL3_L3_GATING_DISABLE		(1<<10)
#define GCL_CR_CTL2_DATA_ECC_CHECK_ENABLE		(1<<19)
#define GCL_CR_CTL2_L2_SLEEP_TRANSISTOR_STRENGTH_ENABLE	(1<<20)

#define GCL_CR_CTL2_ATTRIBUTES (GCL_CR_CTL2_WRTHRU_ENA | \
				GCL_CR_CTL2_TAG_ECC_CHECK_ENABLE | \
				GCL_CR_CTL2_GL3_GL3Q_GATING_DISABLE | \
				GCL_CR_CTL2_GL3_GCL_GATING_DISABLE | \
				GCL_CR_CTL2_GL3_L3_GATING_DISABLE | \
				GCL_CR_CTL2_DATA_ECC_CHECK_ENABLE | \
		GCL_CR_CTL2_L2_SLEEP_TRANSISTOR_STRENGTH_ENABLE)

/*
	GL3 attributes controlled via GCL_CR_ECO
*/
#define MDFLD_GCL_CR_ECO_EVICT_INVAL			(1)

/* GL3 */
#define MDFLD_GL3_WRITE(_val, _offs) \
	iowrite32(_val, dev_priv->gl3_reg + (_offs))
#define MDFLD_GL3_READ(_offs) \
	ioread32(dev_priv->gl3_reg + (_offs))

void gl3_enable(void);
void gl3_invalidate(void);
void gl3_flush(void);
void gl3_reset(void);
bool gl3_exist(void);
void gl3_disable(void);

#endif
