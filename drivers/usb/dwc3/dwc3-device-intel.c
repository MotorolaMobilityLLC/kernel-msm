/**
 * Copyright (C) 2012 Intel Corp.
 * Author: Jiebing Li
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/usb/dwc3-intel-mid.h>
#include <linux/usb/phy.h>
#include <linux/wakelock.h>
#include <asm/spid.h>

#include "core.h"
#include "gadget.h"
#include "io.h"
#include "otg.h"

#include "debug.h"

#include "core.c"
#include "ep0.c"
#include "gadget.c"

/* FLIS register */
#define APBFC_EXIOTG3_MISC0_REG		0xF90FF85C

/* Global User Control Register Auto Retry bit*/
#define DWC3_GUCTL_USB_HST_IN_AUTO_RETRY_EN	(1 << 14)

/* Global Configuration Register */
#define DWC3_GRXTHRCFG_USBRXPKTCNTSEL		(1 << 29)
#define DWC3_GRXTHRCFG_USBRXPKTCNT(n)		(n << 24)
#define DWC3_GRXTHRCFG_USBRXPKTCNT_MASK		(0xf << 24)
#define DWC3_GRXTHRCFG_USBMAXRXBURSTSIZE(n)	(n << 19)
#define DWC3_GRXTHRCFG_USBMAXRXBURSTSIZE_MASK	(0x1f << 19)

/**
 * struct dwc3_dev_data - Structure holding platform related
 *			information
 * @flis_reg:		FLIS register
 * @grxthrcfg:		DWC3 GRXTHCFG register
 */
struct dwc3_dev_data {
	struct dwc3		*dwc;
	void __iomem		*flis_reg;
	u32			grxthrcfg;
	struct wake_lock	wake_lock;
	struct mutex		mutex;
};

static struct dwc3_dev_data	*_dev_data;

/*
 * dwc3_set_fils_reg - set FLIS register
 *
 * This is a workaround for OTG3 IP bug of using EP #8 for host mode
 */
static void dwc3_set_flis_reg(void)
{
	u32			reg;
	void __iomem		*flis_reg;

	flis_reg = _dev_data->flis_reg;

	reg = dwc3_readl(flis_reg, DWC3_GLOBALS_REGS_START);
	reg &= ~(1 << 3);
	dwc3_writel(flis_reg, DWC3_GLOBALS_REGS_START, reg);
}

/*
 * dwc3_disable_multi_packet - set GRXTHRCFG register to disable
 * reception multi-packet thresholdingfor DWC2.50a.
 */
static void dwc3_disable_multi_packet(struct dwc3 *dwc)
{
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_GRXTHRCFG);
	_dev_data->grxthrcfg = reg;
	if (reg) {
		reg &= ~DWC3_GRXTHRCFG_USBRXPKTCNTSEL;
		reg &= ~DWC3_GRXTHRCFG_USBRXPKTCNT_MASK;
		reg &= ~DWC3_GRXTHRCFG_USBMAXRXBURSTSIZE_MASK;

		dwc3_writel(dwc->regs, DWC3_GRXTHRCFG, reg);
	}
}

/*
 * dwc3_enable_host_auto_retry - clear Auto Retry Enable bit
 * for device mode
 */
static void dwc3_enable_host_auto_retry(struct dwc3 *dwc, bool enable)
{
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_GUCTL);

	if (enable)
		reg |= DWC3_GUCTL_USB_HST_IN_AUTO_RETRY_EN;
	else
		reg &= ~DWC3_GUCTL_USB_HST_IN_AUTO_RETRY_EN;

	dwc3_writel(dwc->regs, DWC3_GUCTL, reg);
}

static void dwc3_do_extra_change(struct dwc3 *dwc)
{
	u32		reg;

	if (!dwc3_is_cht())
		dwc3_set_flis_reg();

	if (dwc->revision == DWC3_REVISION_250A)
		dwc3_disable_multi_packet(dwc);

	dwc3_enable_host_auto_retry(dwc, false);

	/* the initial/default value of GCTL.PwnDnScale (bit 31:19) is not
	 * properly set. This affects A0 as well as B0 and would affect how
	 * ltssm in u3pmu works during suspend state where periodic rx
	 * termination detect etc need to be performed in U3.
	 *
	 * Our suspend clock is 19.2 MHz.
	 * Hence PwrDnScale = 19200 / 16 = 1200 (= 0x4B0). To account for
	 * possible jitter of suspend clock and to have margin, I recommend
	 * it to be set to 1250 (= 0x4E2). Current default value is wrong and
	 * set to 0x8B0
	 */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_PWRDNSCALE_MASK;
	reg |= DWC3_GCTL_PWRDNSCALE(0x4E2);
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);
}

static void dwc3_enable_hibernation(struct dwc3 *dwc, bool on)
{
	u32 num, reg;

	if (DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1)
		!= DWC3_GHWPARAMS1_EN_PWROPT_HIB) {
		dev_err(dwc->dev, "Device Mode Hibernation is not supported\n");
		return;
	}

	num = DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(
		 dwc->hwparams.hwparams4);
	if (num != 1)
		dev_err(dwc->dev, "number of scratchpad buffer: %d\n", num);

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);

	if (on) {
		dwc3_writel(dwc->regs, DWC3_GCTL,
				reg | DWC3_GCTL_GBLHIBERNATIONEN);

		dwc3_send_gadget_generic_command(dwc, DWC3_DGCMD_SET_SCRATCH_ADDR_LO,
				dwc->scratch_array_dma & 0xffffffffU);
	} else
		dwc3_writel(dwc->regs, DWC3_GCTL,
				reg & ~DWC3_GCTL_GBLHIBERNATIONEN);
}

/*
 * Re-write irq functions. Not use irq thread. Because irqthread has negative
 * impact on usb performance, especially for usb network performance, USB3 UDP
 * download performance will drop from 80MB/s to 40MB/s if irqthread is enabled.
 */
static irqreturn_t dwc3_quirks_process_event_buf(struct dwc3 *dwc, u32 buf)
{
	struct dwc3_event_buffer *evt;
	u32 count;
	int left;

	evt = dwc->ev_buffs[buf];

	count = dwc3_readl(dwc->regs, DWC3_GEVNTCOUNT(buf));
	count &= DWC3_GEVNTCOUNT_MASK;
	if (!count)
		return IRQ_NONE;

	evt->count = count;

	/* WORKAROUND: Add 4 us delay workaround to A-unit issue in A0 stepping.
	* Can be removed after B0.
	*/
	if (dwc->is_otg && dwc->revision == DWC3_REVISION_210A)
		udelay(4);

	/* WORKAROUND: Add 4 us delay as moorfield seems to have memory
	 * inconsistent issue
	 */
	if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
		INTEL_MID_BOARD(1, TABLET, MOFD))
		udelay(4);

	left = evt->count;

	while (left > 0) {
		union dwc3_event event;

		event.raw = *(u32 *) (evt->buf + evt->lpos);

		dwc3_process_event_entry(dwc, &event);

		/*
		* FIXME we wrap around correctly to the next entry as
		* almost all entries are 4 bytes in size. There is one
		* entry which has 12 bytes which is a regular entry
		* followed by 8 bytes data. ATM I don't know how
		* things are organized if we get next to the a
		* boundary so I worry about that once we try to handle
		* that.
		*/
		evt->lpos = (evt->lpos + 4) % DWC3_EVENT_BUFFERS_SIZE;
		left -= 4;

		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(buf), 4);
	}

	evt->count = 0;

	return IRQ_HANDLED;
}

static irqreturn_t dwc3_quirks_interrupt(int irq, void *_dwc)
{
	struct dwc3	*dwc = _dwc;
	int		i;
	irqreturn_t	ret = IRQ_NONE;

	spin_lock(&dwc->lock);
	if (dwc->pm_state != PM_ACTIVE) {
		if (dwc->pm_state == PM_SUSPENDED) {
			dev_info(dwc->dev, "u2/u3 pmu is received\n");
			pm_runtime_get(dwc->dev);
			dwc->pm_state = PM_RESUMING;
			ret = IRQ_HANDLED;
		}
		goto out;
	}

	for (i = 0; i < dwc->num_event_buffers; i++) {
		irqreturn_t status;

		status = dwc3_quirks_process_event_buf(dwc, i);
		if (status == IRQ_HANDLED)
			ret = status;
	}

out:
	spin_unlock(&dwc->lock);

	return ret;
}

int dwc3_start_peripheral(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			irq;
	int			ret = 0;

	wake_lock(&_dev_data->wake_lock);
	pm_runtime_get_sync(dwc->dev);

	mutex_lock(&_dev_data->mutex);
	spin_lock_irqsave(&dwc->lock, flags);

	if (dwc->gadget_driver && dwc->soft_connected) {
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);

		spin_unlock_irqrestore(&dwc->lock, flags);
		dwc3_core_init(dwc);
		spin_lock_irqsave(&dwc->lock, flags);

		if (dwc->hiber_enabled)
			dwc3_enable_hibernation(dwc, true);
		dwc3_do_extra_change(dwc);
		dwc3_event_buffers_setup(dwc);
		ret = dwc3_init_for_enumeration(dwc);
		if (ret)
			goto err1;

		dwc3_gadget_run_stop(dwc, 1);
		if (dwc->hiber_enabled)
			dwc3_gadget_keep_conn(dwc, 1);
	}

	dwc->pm_state = PM_ACTIVE;

	spin_unlock_irqrestore(&dwc->lock, flags);

	irq = platform_get_irq(to_platform_device(dwc->dev), 0);
	if (dwc->quirks_disable_irqthread)
		ret = request_irq(irq, dwc3_quirks_interrupt,
				IRQF_SHARED, "dwc3", dwc);
	else
		ret = request_threaded_irq(irq, dwc3_interrupt, dwc3_thread_interrupt,
				IRQF_SHARED, "dwc3", dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to request irq #%d --> %d\n",
				irq, ret);
		goto err0;
	}
	mutex_unlock(&_dev_data->mutex);

	return 0;

err1:
	spin_unlock_irqrestore(&dwc->lock, flags);
err0:
	mutex_unlock(&_dev_data->mutex);

	return ret;
}

int dwc3_stop_peripheral(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	u8			epnum;
	int			irq;

	mutex_lock(&_dev_data->mutex);
	spin_lock_irqsave(&dwc->lock, flags);

	/* Disable hibernation for D0i3cold */
	dwc3_enable_hibernation(dwc, false);

	dwc3_stop_active_transfers(dwc);

	if (dwc->gadget.speed != USB_SPEED_UNKNOWN) {
		dwc3_disconnect_gadget(dwc);

		dwc->gadget.speed = USB_SPEED_UNKNOWN;
	}

	dwc->start_config_issued = false;

	/* Clear Run/Stop bit */
	dwc3_gadget_run_stop(dwc, 0);
	dwc3_gadget_keep_conn(dwc, 0);

	for (epnum = 0; epnum < 2; epnum++) {
		struct dwc3_ep  *dep;

		dep = dwc->eps[epnum];

		if (dep->flags & DWC3_EP_ENABLED)
			__dwc3_gadget_ep_disable(dep);
	}

	dwc3_gadget_disable_irq(dwc);

	dwc3_event_buffers_cleanup(dwc);

	if (_dev_data->grxthrcfg && dwc->revision == DWC3_REVISION_250A) {
		dwc3_writel(dwc->regs, DWC3_GRXTHRCFG, _dev_data->grxthrcfg);
		_dev_data->grxthrcfg = 0;
	}

	dwc3_enable_host_auto_retry(dwc, true);

	if (dwc->pm_state != PM_SUSPENDED)
		pm_runtime_put(dwc->dev);

	dwc->pm_state = PM_DISCONNECTED;
	spin_unlock_irqrestore(&dwc->lock, flags);

	irq = platform_get_irq(to_platform_device(dwc->dev), 0);
	free_irq(irq, dwc);

	mutex_unlock(&_dev_data->mutex);

	cancel_delayed_work_sync(&dwc->link_work);

	wake_unlock(&_dev_data->wake_lock);

	return 0;
}

static int dwc3_device_gadget_pullup(struct usb_gadget *g, int is_on)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			ret;

	/*
	 * FIXME If pm_state is PM_RESUMING, we should wait for it to
	 * become PM_ACTIVE before continue. The chance of hitting
	 * PM_RESUMING is rare, but if so, we'll return directly.
	 *
	 * If some gadget reaches here in atomic context,
	 * pm_runtime_get_sync will cause a sleep problem.
	 */
	if (dwc->pm_state == PM_RESUMING) {
		dev_err(dwc->dev, "%s: PM_RESUMING, return -EIO\n", __func__);
		return -EIO;
	}

	if (dwc->pm_state == PM_SUSPENDED) {

		/* WORKAROUND Wait 300 ms and check if the state is still PM_SUSPENDED
		 * before resuming the controller. This avoids resuming the controller
		 * during enumeration and causing PHY hangs.
		 */
		msleep(300);
		if (dwc->pm_state == PM_SUSPENDED)
			pm_runtime_get_sync(dwc->dev);
	}

	is_on = !!is_on;

	mutex_lock(&_dev_data->mutex);

	spin_lock_irqsave(&dwc->lock, flags);
	if (dwc->soft_connected == is_on) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		goto done;
	}

	dwc->soft_connected = is_on;

	if (dwc->pm_state == PM_DISCONNECTED) {
		spin_unlock_irqrestore(&dwc->lock, flags);
		goto done;
	}

	if (is_on) {
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);

		/* Per dwc3 databook 2.40a section 8.1.9, re-connection
		 * should follow steps described section 8.1.1 power on
		 * or soft reset.
		 */
		spin_unlock_irqrestore(&dwc->lock, flags);
		dwc3_core_init(dwc);
		spin_lock_irqsave(&dwc->lock, flags);

		if (dwc->hiber_enabled)
			dwc3_enable_hibernation(dwc, true);
		dwc3_do_extra_change(dwc);
		dwc3_event_buffers_setup(dwc);
		dwc3_init_for_enumeration(dwc);
		ret = dwc3_gadget_run_stop(dwc, 1);
		if (dwc->hiber_enabled)
			dwc3_gadget_keep_conn(dwc, 1);
	} else {
		u8 epnum;

		for (epnum = 0; epnum < 2; epnum++) {
			struct dwc3_ep  *dep;

			dep = dwc->eps[epnum];

			if (dep->flags & DWC3_EP_ENABLED)
				__dwc3_gadget_ep_disable(dep);
		}

		dwc3_stop_active_transfers(dwc);
		dwc3_gadget_keep_conn(dwc, 0);
		ret = dwc3_gadget_run_stop(dwc, 0);
		dwc3_gadget_disable_irq(dwc);

		/* Clear all OTG events which will confuse host and
		 * make enumeration failed after pullup with on.
		 */
		dwc3_writel(dwc->regs, OCTL, 0);
		dwc3_writel(dwc->regs, OEVTEN, 0);
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_OTG);
	}

	spin_unlock_irqrestore(&dwc->lock, flags);
	mutex_unlock(&_dev_data->mutex);

	return ret;

done:
	mutex_unlock(&_dev_data->mutex);

	return 0;
}

static const struct usb_gadget_ops dwc3_device_gadget_ops = {
	.get_frame		= dwc3_gadget_get_frame,
	.wakeup			= dwc3_gadget_wakeup,
	.set_selfpowered	= dwc3_gadget_set_selfpowered,
	.pullup			= dwc3_device_gadget_pullup,
	.udc_start		= dwc3_gadget_start,
	.udc_stop		= dwc3_gadget_stop,
	.vbus_draw		= dwc3_vbus_draw,
};

static int dwc3_device_intel_probe(struct platform_device *pdev)
{
	struct device_node	*node = pdev->dev.of_node;
	struct dwc3		*dwc;
	struct device		*dev = &pdev->dev;
	int			ret = -ENOMEM;
	void			*mem;
	struct intel_dwc_otg_pdata	*otg_data = dev->parent->platform_data;

	struct dwc_device_par	*pdata;
	struct usb_phy		*usb_phy;
	struct dwc_otg2		*otg;

	mem = devm_kzalloc(dev, sizeof(*dwc) + DWC3_ALIGN_MASK, GFP_KERNEL);
	if (!mem) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}
	dwc = PTR_ALIGN(mem, DWC3_ALIGN_MASK + 1);
	dwc->mem = mem;

	_dev_data = kzalloc(sizeof(*_dev_data), GFP_KERNEL);
	if (!_dev_data) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	_dev_data->dwc = dwc;

	pdata = (struct dwc_device_par *)pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "No platform data for %s.\n",
				dev_name(&pdev->dev));
		return -ENODEV;
	}

	if (node) {
		dwc->usb2_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 0);
		dwc->usb3_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 1);
	} else {
		dwc->usb2_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
		dwc->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
	}

	if (IS_ERR(dwc->usb2_phy)) {
		ret = PTR_ERR(dwc->usb2_phy);

		/*
		 * if -ENXIO is returned, it means PHY layer wasn't
		 * enabled, so it makes no sense to return -EPROBE_DEFER
		 * in that case, since no PHY driver will ever probe.
		 */
		if (ret == -ENXIO)
			return ret;

		dev_err(dev, "no usb2 phy configured\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(dwc->usb3_phy)) {
		ret = PTR_ERR(dwc->usb2_phy);

		/*
		 * if -ENXIO is returned, it means PHY layer wasn't
		 * enabled, so it makes no sense to return -EPROBE_DEFER
		 * in that case, since no PHY driver will ever probe.
		 */
		if (ret == -ENXIO)
			return ret;

		dev_err(dev, "no usb3 phy configured\n");
		return -EPROBE_DEFER;
	}

	mutex_init(&_dev_data->mutex);
	spin_lock_init(&dwc->lock);
	platform_set_drvdata(pdev, dwc);

	dwc->regs   = pdata->io_addr + DWC3_GLOBALS_REGS_START;
	dwc->regs_size  = pdata->len - DWC3_GLOBALS_REGS_START;
	dwc->dev	= dev;
	if (otg_data && otg_data->usb2_phy_type == USB2_PHY_UTMI)
		dwc->utmi_phy = 1;
	if (otg_data)
		dwc->hiber_enabled = !!otg_data->device_hibernation;

	dev->dma_mask	= dev->parent->dma_mask;
	dev->dma_parms	= dev->parent->dma_parms;
	dma_set_coherent_mask(dev, dev->parent->coherent_dma_mask);

	if (!strncmp("super", maximum_speed, 5))
		dwc->maximum_speed = DWC3_DCFG_SUPERSPEED;
	else if (!strncmp("high", maximum_speed, 4))
		dwc->maximum_speed = DWC3_DCFG_HIGHSPEED;
	else if (!strncmp("full", maximum_speed, 4))
		dwc->maximum_speed = DWC3_DCFG_FULLSPEED1;
	else if (!strncmp("low", maximum_speed, 3))
		dwc->maximum_speed = DWC3_DCFG_LOWSPEED;
	else
		dwc->maximum_speed = DWC3_DCFG_SUPERSPEED;

	dwc->needs_fifo_resize = of_property_read_bool(node, "tx-fifo-resize");

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_get_sync(dev);
	pm_runtime_forbid(dev);

	dwc3_cache_hwparams(dwc);
	dwc3_core_num_eps(dwc);

	if (!dwc3_is_cht())
		_dev_data->flis_reg =
			ioremap_nocache(APBFC_EXIOTG3_MISC0_REG, 4);

	ret = dwc3_alloc_event_buffers(dwc, DWC3_EVENT_BUFFERS_SIZE);
	if (ret) {
		dev_err(dwc->dev, "failed to allocate event buffers\n");
		ret = -ENOMEM;
		goto err0;
	}

	/*
	* Not use irq thread, because irqthread has negative impact
	* on usb performance, especially for usb network performance.
	*/
	dwc->quirks_disable_irqthread = 1;

	usb_phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!usb_phy) {
		dev_err(dev, "failed to get usb2 phy\n");
		return -ENODEV;
	}
	otg = container_of(usb_phy, struct dwc_otg2, usb2_phy);
	otg->start_device = dwc3_start_peripheral;
	otg->stop_device = dwc3_stop_peripheral;
	otg->vbus_draw = dwc3_vbus_draw;
	usb_put_phy(usb_phy);
	dwc->is_otg = 1;

	wake_lock_init(&_dev_data->wake_lock,
			WAKE_LOCK_SUSPEND, "dwc_wake_lock");

	ret = dwc3_gadget_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize gadget\n");
		goto err0;
	}
	dwc->gadget.ops = &dwc3_device_gadget_ops;
	dwc->gadget.is_otg = 1;

	dwc->mode = DWC3_MODE_DEVICE;

	ret = dwc3_debugfs_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize debugfs\n");
		goto err1;
	}

	pm_runtime_allow(dev);
	pm_runtime_put(dev);

	return 0;

err1:
	dwc3_gadget_exit(dwc);

err0:
	dwc3_free_event_buffers(dwc);

	return ret;
}

static int dwc3_device_intel_remove(struct platform_device *pdev)
{
	iounmap(_dev_data->flis_reg);

	wake_lock_destroy(&_dev_data->wake_lock);

	dwc3_remove(pdev);

	kfree(_dev_data);
	_dev_data = NULL;

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static const struct dev_pm_ops dwc3_device_pm_ops = {
	.runtime_suspend	= dwc3_runtime_suspend,
	.runtime_resume		= dwc3_runtime_resume,
};
#define DWC3_DEVICE_PM_OPS	(&dwc3_device_pm_ops)
#else
#define DWC3_DEVICE_PM_OPS	NULL
#endif

static struct platform_driver dwc3_device_intel_driver = {
	.probe		= dwc3_device_intel_probe,
	.remove		= dwc3_device_intel_remove,
	.driver		= {
		.name	= "dwc3-device",
		.of_match_table	= of_match_ptr(of_dwc3_match),
		.pm	= DWC3_DEVICE_PM_OPS,
	},
};

module_platform_driver(dwc3_device_intel_driver);

MODULE_ALIAS("platform:dwc3");
MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("DesignWare USB3 DRD Controller Driver");
