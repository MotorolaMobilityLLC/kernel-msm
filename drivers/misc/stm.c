/*
 *  stm.c - MIPI STM Debug Unit driver
 *
 *  Copyright (C) Intel 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The STM (Sytem Trace Macro) Unit driver configure trace output
 * to the Intel Tangier PTI port and DWC3 USB xHCI controller
 * out of the mobile device for analysis with a debugging tool
 * (Lauterbach, Fido). This is part of a solution for the MIPI P1149.7,
 * compact JTAG, standard and USB Debug-Class
 *
 * This header file will allow other parts of the OS to use the
 * interface to write out it's contents for debugging a mobile system.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sdm.h>

#include "stm.h"
#include <asm/intel-mid.h>
#include <asm/intel_soc_debug.h>
#include "../usb/dwc3/core.h"

/* STM Registers */
#define STM_CTRL		0x0000
#define STM_USB3DBGGTHR		0x0008
#define STM_MASMSK0		0x0010
#define STM_MASMSK1		0x0018
#define STM_USBTO		0x0020
#define STM_CHMSK		0x0080
#define STM_AGTBAR0		0x00C0
#define STM_AGTBAR1		0x0140
#define STM_AGTBAR2		0x01C0
#define STM_AGTBAR3		0x0240
#define STM_AGTBAR4		0x02C0
#define STM_AGTBAR5		0x0340
#define STM_AGTBAR6		0x03C0
#define STM_AGTBAR7		0x0440
#define STM_AGTBAR8		0x04C0
#define STM_AGTBAR9		0x0540
#define STM_AGTBAR10		0x05C0
#define STM_AGTBAR11		0x0640

/*
 * STM registers
 */
#define STM_REG_BASE		0x0        /* registers base offset */
#define STM_REG_LEN		0x20       /* address length */
/*
 * TRB buffers
 */
#define STM_TRB_BASE		0x400      /* TRB base offset */
#define STM_TRB_LEN		0x100	   /* address length */
#define STM_TRB_NUM		16         /* number of TRBs */

/*
 * This protects R/W to stm registers
 */
static DEFINE_MUTEX(stmlock);

static struct stm_dev *_dev_stm;

static inline u32 stm_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static inline void stm_writel(void __iomem *base, u32 offset, u32 value)
{
	writel(value, base + offset);
}

/**
 * stm_kernel_set_out()-
 * Kernel API function used to
 * set STM output configuration to PTI or USB.
 *
 * @bus_type:
 *	0 = PTI 4-bits legacy end user
 *	1 = PTI 4-bits NiDnT
 *	2 = PTI 16-bits
 *	3 = PTI 12-bits
 *	4 = PTI 8-bits
 *	15 = USB Debug-Class (DvC.Trace)
 *
 */
int stm_kernel_set_out(int bus_type)
{

	struct stm_dev *drv_stm = _dev_stm;

	/*
	 * since this function is exported, this is treated like an
	 * API function, thus, all parameters should
	 * be checked for validity.
	 */
	if (drv_stm == NULL)
		return 0;

	mutex_lock(&stmlock);

	drv_stm->stm_ctrl_hwreg.reg_word =
		stm_readl(drv_stm->stm_ioaddr, (u32)STM_CTRL);

	switch (bus_type) {
	case STM_PTI_4BIT_LEGACY:
	case STM_PTI_4BIT_NIDNT:
	case STM_PTI_16BIT:
	case STM_PTI_12BIT:
	case STM_PTI_8BIT:
		drv_stm->stm_ctrl_hwreg.pti_out_en = true;
		drv_stm->stm_ctrl_hwreg.usb_debug_en = false;
		drv_stm->stm_ctrl_hwreg.pti_out_mode_sel = bus_type;
		stm_writel(drv_stm->stm_ioaddr, (u32)STM_CTRL,
			   drv_stm->stm_ctrl_hwreg.reg_word);
		break;
	case STM_USB:
		drv_stm->stm_ctrl_hwreg.pti_out_en = false;
		drv_stm->stm_ctrl_hwreg.usb_debug_en = true;
		stm_writel(drv_stm->stm_ioaddr, (u32)STM_CTRL,
			   drv_stm->stm_ctrl_hwreg.reg_word);
		break;
	default:
		/* N/A */
		break;
	}
	mutex_unlock(&stmlock);

	return 1;
}
EXPORT_SYMBOL_GPL(stm_kernel_set_out);

/**
 * stm_kernel_get_out()-
 * Kernel API function used to get
 * STM output cofiguration PTI or USB.
 *
 */
int stm_kernel_get_out(void)
{
	struct stm_dev *drv_stm = _dev_stm;
	int ret = -EOPNOTSUPP;

	if (drv_stm == NULL)
		return -EOPNOTSUPP;

	mutex_lock(&stmlock);

	drv_stm->stm_ctrl_hwreg.reg_word =
		stm_readl(drv_stm->stm_ioaddr, (u32)STM_CTRL);

	if (!drv_stm->stm_ctrl_hwreg.usb_debug_en) {
		if (drv_stm->stm_ctrl_hwreg.pti_out_en)
			ret = (int)drv_stm->stm_ctrl_hwreg.pti_out_mode_sel;
	} else {
		ret = (int)STM_USB;
	}
	mutex_unlock(&stmlock);

	return ret;
}
EXPORT_SYMBOL_GPL(stm_kernel_get_out);

/**
 * stm_set_out() - 'out' parameter set function from 'STM' module
 *
 * called when writing to 'out' parameter from 'STM' module in sysfs
 */
static int stm_set_out(const char *val, struct kernel_param *kp)
{
	int bus_type_value;
	int ret = -EINVAL;

	if (sscanf(val, "%2d", &bus_type_value) != 1)
		return ret;

	return stm_kernel_set_out(bus_type_value);
}

/**
 * stm_get_out() - 'out' parameter get function from 'STM' module
 *
 * called when reading 'out' parameter from 'STM' module in sysfs
 */
static int stm_get_out(char *buffer, struct kernel_param *kp)
{
	int i;

	i = stm_kernel_get_out();
	if (i == -EOPNOTSUPP) {
		buffer[0] = '\0';
		return 0;
	}

	return sprintf(buffer, "%2d", i);
}

/**
 * stm_init() - initialize stmsub3dbgthr register
 *
 * @return - 0 on Success
 */
static int stm_init(void)
{
	struct stm_dev *stm = _dev_stm;
	struct stm_usb3_ctrl *usb3dbg;

	if (!stm)
		return -ENODEV;

	usb3dbg = &stm->stm_usb3_hwreg;
	usb3dbg->reg_word = stm_readl(stm->stm_ioaddr, (u32)STM_USB3DBGGTHR);

	usb3dbg->reg_word = 0xFF;

	stm_writel(stm->stm_ioaddr, (u32)STM_USB3DBGGTHR, usb3dbg->reg_word);

	return 0;
}

/**
 * stm_alloc_static_trb_pool() - set stm trb pool dma_addr and return
 * trb_pool
 *
 * @dma_addr - trb pool dma physical address to set
 * @return - trb pool address ioremaped pointer
 */
static void *stm_alloc_static_trb_pool(dma_addr_t *dma_addr)
{
	struct stm_dev *stm = _dev_stm;
	if (!stm)
		return NULL;

	*dma_addr = stm->stm_trb_base;
	return stm->trb_ioaddr;
}

static void ebc_io_free_static_trb_pool(void)
{
	/* Nothing to do, HW TRB */
}

static int stm_xfer_start(void)
{
	struct stm_dev *stm = _dev_stm;
	struct stm_ctrl *stm_ctrl;
	u32 reg_word;

	if (!stm)
		return -ENODEV;

	/* REVERTME : filter PUNIT and SCU MasterID when switching to USB */
	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_ANNIEDALE) {
		pr_info("%s\n REVERTME : filter PUNIT and SCU MasterID\n", __func__);
		reg_word = stm_readl(stm->stm_ioaddr, (u32)STM_MASMSK1);
		reg_word |= 0x28;
		stm_writel(stm->stm_ioaddr, (u32)STM_MASMSK1, reg_word);

		pr_info("%s\n REVERTME : USBTO\n", __func__);
		reg_word = stm_readl(stm->stm_ioaddr, (u32)STM_USBTO);
		reg_word |= 0x01;
		stm_writel(stm->stm_ioaddr, (u32)STM_USBTO, reg_word);
	}

	stm_ctrl = &stm->stm_ctrl_hwreg;
	stm_ctrl->reg_word = stm_readl(stm->stm_ioaddr, (u32)STM_CTRL);

	stm_ctrl->usb_debug_en = true;
	stm_ctrl->pti_out_en = false;

	stm_writel(stm->stm_ioaddr, (u32)STM_CTRL, stm_ctrl->reg_word);
	pr_info("%s\n switch STM output to DvC.Trace ", __func__);

	return 0;
}

static int stm_xfer_stop(void)
{
	struct stm_dev *stm = _dev_stm;
	struct stm_ctrl *stm_ctrl;

	if (!stm)
		return -ENODEV;

	stm_ctrl = &stm->stm_ctrl_hwreg;
	stm_ctrl->reg_word = stm_readl(stm->stm_ioaddr, (u32)STM_CTRL);

	stm_ctrl->usb_debug_en = false;
	stm_ctrl->pti_out_en = true;

	stm_writel(stm->stm_ioaddr, (u32)STM_CTRL, stm_ctrl->reg_word);
	pr_info("%s\n switch STM to 4bits MIPI PTI (default)", __func__);

	return 0;
}

static struct ebc_io stm_ebc_io_ops = {
	.name = "stmbuf4kB",
	.epname = "ep1in",
	.epnum = 3,
	.is_ondemand = 1,
	.static_trb_pool_size = 4,
	.init = stm_init,
	.alloc_static_trb_pool = stm_alloc_static_trb_pool,
	.free_static_trb_pool = ebc_io_free_static_trb_pool,
	.xfer_start = stm_xfer_start,
	.xfer_stop = stm_xfer_stop,
};

#define EXI_IN_TRB_POOL_OFFSET (4*16)
static void *exi_inbound_alloc_static_trb_pool(dma_addr_t *dma_addr)
{
	struct stm_dev *stm = _dev_stm;
	if (!stm)
		return NULL;

	*dma_addr = stm->stm_trb_base + EXI_IN_TRB_POOL_OFFSET;
	return stm->trb_ioaddr + EXI_IN_TRB_POOL_OFFSET;
}

static struct ebc_io exi_in_ebc_io_ops = {
	.name = "exi-inbound",
	.epname = "ep8in",
	.epnum = 17,
	.is_ondemand = 0,
	.static_trb_pool_size = 4,
	.alloc_static_trb_pool = exi_inbound_alloc_static_trb_pool,
	.free_static_trb_pool = ebc_io_free_static_trb_pool,
};

#define EXI_OUT_TRB_POOL_OFFSET (8*16)
static void *exi_outbound_alloc_static_trb_pool(dma_addr_t *dma_addr)
{
	struct stm_dev *stm = _dev_stm;
	if (!stm)
		return NULL;

	*dma_addr = stm->stm_trb_base + EXI_OUT_TRB_POOL_OFFSET;
	return stm->trb_ioaddr + EXI_OUT_TRB_POOL_OFFSET;
}

static struct ebc_io exi_out_ebc_io_ops = {
	.name = "exi-outbound",
	.epname = "ep8out",
	.epnum = 16,
	.is_ondemand = 0,
	.static_trb_pool_size = 2,
	.alloc_static_trb_pool = exi_outbound_alloc_static_trb_pool,
	.free_static_trb_pool = ebc_io_free_static_trb_pool,
};

int stm_is_enabled()
{
	return (_dev_stm != NULL);
}
EXPORT_SYMBOL_GPL(stm_is_enabled);

/**
 * stm_dev_init()- Used to setup STM resources on the pci bus.
 *
 * @pdev- pci_dev struct values for pti device.
 * @stm- stm_dev struct managing stm resources
 *
 * Returns:
 *	0 for success
 *	otherwise, error
 */
int stm_dev_init(struct pci_dev *pdev,
		 struct stm_dev *stm)
{
	int retval = 0;
	int pci_bar = 0;

	if (!cpu_has_debug_feature(DEBUG_FEATURE_PTI))
		return -ENODEV;

	dev_dbg(&pdev->dev, "%s %s(%d): STM PCI ID %04x:%04x\n", __FILE__,
		__func__, __LINE__, pdev->vendor, pdev->device);

	stm->stm_addr = pci_resource_start(pdev, pci_bar);

	retval = pci_request_region(pdev, pci_bar, dev_name(&pdev->dev));
	if (retval != 0) {
		dev_err(&pdev->dev,
			"%s(%d): pci_request_region() returned error %d\n",
			__func__, __LINE__, retval);
		return retval;
	}
	pr_info("stm add %x\n", stm->stm_addr);

	stm->stm_reg_base = stm->stm_addr+STM_REG_BASE;
	stm->stm_ioaddr = ioremap_nocache((u32)stm->stm_reg_base,
					  STM_REG_LEN);
	if (!stm->stm_ioaddr) {
		retval = -ENOMEM;
		goto out_release_region;
	}

	stm->stm_trb_base = stm->stm_addr+STM_TRB_BASE;
	stm->trb_ioaddr = ioremap_nocache((u32)stm->stm_trb_base,
					  STM_TRB_LEN);
	if (!stm->trb_ioaddr) {
		retval = -ENOMEM;
		goto out_iounmap_stm_ioaddr;
	}

	stm->stm_ctrl_hwreg.reg_word = stm_readl(stm->stm_ioaddr,
						 (u32)STM_CTRL);
	stm->stm_usb3_hwreg.reg_word = stm_readl(stm->stm_ioaddr,
						 (u32)STM_USB3DBGGTHR);

	_dev_stm = stm;

	dwc3_register_io_ebc(&stm_ebc_io_ops);
	dwc3_register_io_ebc(&exi_in_ebc_io_ops);
	dwc3_register_io_ebc(&exi_out_ebc_io_ops);

	pr_info("successfully registered ebc io ops\n");

	return retval;

out_iounmap_stm_ioaddr:
	pci_iounmap(pdev, stm->stm_ioaddr);

out_release_region:
	pci_release_region(pdev, pci_bar);

	_dev_stm = NULL;
	return retval;

}
EXPORT_SYMBOL_GPL(stm_dev_init);

/**
 * stm_dev_clean()- Driver exit method to free STM resources from
 *		   PCI bus.
 * @pdev: variable containing pci info of STM.
 * @dev_stm: stm_dev resources to clean.
 */
void stm_dev_clean(struct pci_dev *pdev,
		   struct stm_dev *dev_stm)
{
	int pci_bar = 0;

	/* If STM driver was not initialized properly,
	 * there is nothing to do.
	 */
	if (_dev_stm == NULL)
		return;

	dwc3_unregister_io_ebc(&stm_ebc_io_ops);
	dwc3_unregister_io_ebc(&exi_in_ebc_io_ops);
	dwc3_unregister_io_ebc(&exi_out_ebc_io_ops);

	if (dev_stm != NULL) {
		pci_iounmap(pdev, dev_stm->stm_ioaddr);
		pci_iounmap(pdev, dev_stm->trb_ioaddr);
	}

	pci_release_region(pdev, pci_bar);

	_dev_stm = NULL;
}
EXPORT_SYMBOL_GPL(stm_dev_clean);

module_param_call(stm_out, stm_set_out, stm_get_out, NULL, 0644);
MODULE_PARM_DESC(stm_out, "configure System Trace Macro output");

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florent Pirou");
MODULE_DESCRIPTION("STM Driver");
