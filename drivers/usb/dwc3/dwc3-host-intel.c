/*
 * Copyright (C) 2012 Intel Corp.
 * Author: Yu Wang
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/wakelock.h>
#include <linux/usb/otg.h>
#include <linux/platform_device.h>
#include <linux/usb/dwc3-intel-mid.h>
#include "../host/xhci.h"
#include "core.h"
#include "otg.h"

#define WAIT_DISC_EVENT_COMPLETE_TIMEOUT 5 /* 100ms */
#define PORTSC_IO_ADDR 0xf9100430
#define USBCMD_IO_ADDR 0xf9100020

static int dwc3_start_host(struct usb_hcd *hcd);
static int dwc3_stop_host(struct usb_hcd *hcd);
static struct platform_driver dwc3_xhci_driver;
static int __dwc3_stop_host(struct usb_hcd *hcd);
static int __dwc3_start_host(struct usb_hcd *hcd);
static int dwc3_suspend_host(struct usb_hcd *hcd);
static int dwc3_resume_host(struct usb_hcd *hcd);

static struct dwc3_xhci_hcd {
	struct wake_lock wakelock;
	struct xhci_hcd *xhci;
	struct work_struct reset_hcd;
	struct work_struct poll_loopback;
	int is_rx_test;
	int otg_irqnum;
	bool host_started;
	bool comp_test_enable;
	void __iomem *portsc_mmaddr;
} dwc3_xhci;

static void dwc3_host_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;

	/*
	 * Due to some fatal silicon errors, the controller have to do reset
	 * for make driver continue work.
	 */
	xhci->quirks |= XHCI_RESET;
}

static int dwc3_host_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, dwc3_host_quirks);
}

static int xhci_dwc_bus_resume(struct usb_hcd *hcd)
{
	int ret;

	/* before resume bus, delay 1ms to waiting core stable */
	mdelay(1);

	ret = xhci_bus_resume(hcd);
	return ret;
}

static const struct hc_driver xhci_dwc_hc_driver = {
	.description =		"dwc-xhci",
	.product_desc =		"xHCI Host Controller",
	.hcd_priv_size =	sizeof(struct xhci_hcd *),

	/*
	 * generic hardware linkage
	 */
	.irq =			xhci_irq,
	.flags =		HCD_MEMORY | HCD_USB3 | HCD_SHARED,

	/*
	 * basic lifecycle operations
	 */
	.reset =		dwc3_host_setup,
	.start =		xhci_run,
	.stop =			xhci_stop,
	.shutdown =		xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		xhci_urb_enqueue,
	.urb_dequeue =		xhci_urb_dequeue,
	.alloc_dev =		xhci_alloc_dev,
	.free_dev =		xhci_free_dev,
	.alloc_streams =	xhci_alloc_streams,
	.free_streams =		xhci_free_streams,
	.add_endpoint =		xhci_add_endpoint,
	.drop_endpoint =	xhci_drop_endpoint,
	.endpoint_reset =	xhci_endpoint_reset,
	.check_bandwidth =	xhci_check_bandwidth,
	.reset_bandwidth =	xhci_reset_bandwidth,
	.address_device =	xhci_address_device,
	.update_hub_device =	xhci_update_hub_device,
	.reset_device =		xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =	xhci_get_frame,

	/* Root hub support */
	.hub_control =		xhci_hub_control,
	.hub_status_data =	xhci_hub_status_data,
	.bus_suspend =		xhci_bus_suspend,
	.bus_resume =		xhci_dwc_bus_resume,
};

static int if_usb_devices_connected(struct xhci_hcd *xhci)
{
	struct usb_device		*usb_dev;
	int i, connected_devices = 0;

	if (!xhci)
		return -EINVAL;

	usb_dev = xhci->main_hcd->self.root_hub;
	for (i = 1; i <= usb_dev->maxchild; ++i) {
		if (usb_hub_find_child(usb_dev, i))
			connected_devices++;
	}

	usb_dev = xhci->shared_hcd->self.root_hub;
	for (i = 1; i <= usb_dev->maxchild; ++i) {
		if (usb_hub_find_child(usb_dev, i))
			connected_devices++;
	}

	if (connected_devices)
		return 1;

	return 0;
}

/* For USB3 host electronic compliance test. Controller have to enter
 * Loopback mode for RX test. But controller easier enter compliance
 * mode by mistake. So driver need to trigger warm reset until enter
 * loopback mode successful.
 **/
static void dwc3_poll_lp(struct work_struct *data)
{
	u32 pls, val;

	if (!dwc3_xhci.comp_test_enable)
		return;

	val = readl(dwc3_xhci.portsc_mmaddr);
	pls = val & PORT_PLS_MASK;

	if (pls == XDEV_COMP && dwc3_xhci.is_rx_test)
		writel(val | PORT_WR, dwc3_xhci.portsc_mmaddr);

	if (!dwc3_xhci.is_rx_test || pls == XDEV_LOOPBACK) {
		iounmap(dwc3_xhci.portsc_mmaddr);
		return;
	}
	else
		schedule_work(&dwc3_xhci.poll_loopback);
}

/* Do xHCI driver reinitialize when met fatal errors */
static void dwc3_host_reset(struct work_struct *data)
{
	struct usb_hcd *hcd;

	if (!dwc3_xhci.host_started || !data)
		return;

	if (!dwc3_xhci.xhci)
		return;
	hcd = dwc3_xhci.xhci->main_hcd;

	/* Need hold wakelock to prevent the S3 interrupt
	 * the reset work.*/
	wake_lock(&dwc3_xhci.wakelock);
	__dwc3_stop_host(hcd);
	__dwc3_start_host(hcd);
	wake_unlock(&dwc3_xhci.wakelock);
}

static void dwc_xhci_enable_phy_auto_resume(struct usb_hcd *hcd, bool enable)
{
	u32 val;

	val = readl(hcd->regs + GUSB2PHYCFG0);
	if (enable)
		val |= GUSB2PHYCFG_ULPI_AUTO_RESUME;
	else
		val &= ~GUSB2PHYCFG_ULPI_AUTO_RESUME;
	writel(val, hcd->regs + GUSB2PHYCFG0);
}

static void dwc_xhci_enable_phy_suspend(struct usb_hcd *hcd, bool enable)
{
	u32 val;

	val = readl(hcd->regs + GUSB3PIPECTL0);
	if (enable)
		val |= GUSB3PIPECTL_SUS_EN;
	else
		val &= ~GUSB3PIPECTL_SUS_EN;
	writel(val, hcd->regs + GUSB3PIPECTL0);

	val = readl(hcd->regs + GUSB2PHYCFG0);
	if (enable)
		val |= GUSB2PHYCFG_SUS_PHY;
	else
		val &= ~GUSB2PHYCFG_SUS_PHY;
	writel(val, hcd->regs + GUSB2PHYCFG0);
}

/* Some SS UMS will be enter polling state after plug in with micro A cable.
 * If trigger warm reset, then link can be rescued to U0.
 *
 * This function copy from hub_port_reset function is USB core.
 */
static int dwc3_link_issue_wa(struct xhci_hcd *xhci)
{
	__le32 __iomem **addr;
	int delay_time, ret;
	u32 pls, val, delay;

	addr = dwc3_xhci.xhci->usb3_ports;
	val = xhci_readl(dwc3_xhci.xhci, addr[0]);

	/* If PORTSC.CCS bit haven't set. We can trigger warm reset
	 * to double confirm if really have no device or link can't trained to
	 * U0.
	 */
	if (!(val & PORT_CONNECT)) {
		val |= PORT_WR;
		xhci_writel(xhci, val, addr[0]);
		xhci_dbg(xhci, "%s: trigger warm reset\n", __func__);
	}

	/* Waiting warm reset complete. */
	for (delay_time = 0; delay_time < 800; delay_time += delay) {
		msleep(delay);
		val = xhci_readl(dwc3_xhci.xhci, addr[0]);
		if (!(val & PORT_RESET))
			break;

		if (delay_time >= 20)
			delay = 200;
	}

	if (val & PORT_RESET)
		xhci_err(xhci, "%s port reset failed!\n", __func__);

	return 0;
}

static void dwc_silicon_wa(struct usb_hcd *hcd)
{
	void __iomem *addr;
	u32 val;

	/* Clear GUCTL bit 15 as workaround of DWC controller Bugs
	 * This Bug cause the xHCI driver does not see any
	 * transfer complete events for certain EP after exit
	 * from hibernation mode.*/
	val = readl(hcd->regs + GUCTL);
	val &= ~GUCTL_CMDEVADDR;
	writel(val, hcd->regs + GUCTL);

	/* Disable OTG3-EXI interface by default. It is one
	 * workaround for silicon BUG. It will cause transfer
	 * failed on EP#8 of any USB device.
	 */
	addr = ioremap_nocache(APBFC_EXIOTG3_MISC0_REG, 4);
	val = readl(addr);
	val |= (1 << 3);
	writel(val, addr);
	iounmap(addr);
}

static void dwc_core_reset(struct usb_hcd *hcd)
{
	u32 val;

	val = readl(hcd->regs + GCTL);
	val |= GCTL_CORESOFTRESET;
	writel(val, hcd->regs + GCTL);

	val = readl(hcd->regs + GUSB3PIPECTL0);
	val |= GUSB3PIPECTL_PHYSOFTRST;
	writel(val, hcd->regs + GUSB3PIPECTL0);

	val = readl(hcd->regs + GUSB2PHYCFG0);
	val |= GUSB2PHYCFG_PHYSOFTRST;
	writel(val, hcd->regs + GUSB2PHYCFG0);

	msleep(100);

	val = readl(hcd->regs + GUSB3PIPECTL0);
	val &= ~GUSB3PIPECTL_PHYSOFTRST;
	writel(val, hcd->regs + GUSB3PIPECTL0);

	val = readl(hcd->regs + GUSB2PHYCFG0);
	val &= ~GUSB2PHYCFG_PHYSOFTRST;
	writel(val, hcd->regs + GUSB2PHYCFG0);

	msleep(20);

	val = readl(hcd->regs + GCTL);
	val &= ~GCTL_CORESOFTRESET;
	writel(val, hcd->regs + GCTL);
}

/*
 * On MERR platform, the suspend clock is 19.2MHz.
 * Hence PwrDnScale = 19200 / 16 = 1200 (= 0x4B0).
 * To account for possible jitter of suspend clock and to have margin,
 * So recommend it to be set to 1250 (= 0x4E2).
 * */
static void dwc_set_ssphy_p3_clockrate(struct usb_hcd *hcd)
{
	u32 gctl;

	gctl = readl(hcd->regs + GCTL);
	gctl &= ~GCTL_PWRDNSCALE_MASK;
	gctl |= GCTL_PWRDNSCALE(0x4E2);
	writel(gctl, hcd->regs + GCTL);
}

static ssize_t
show_pm_get(struct device *_dev, struct device_attribute *attr, char *buf)
{
	struct platform_device		*pdev = to_platform_device(_dev);
	struct usb_hcd		*hcd = platform_get_drvdata(pdev);

	pm_runtime_put(hcd->self.controller);
	return 0;

}
static ssize_t store_pm_get(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device		*pdev = to_platform_device(_dev);
	struct usb_hcd		*hcd = platform_get_drvdata(pdev);

	pm_runtime_get(hcd->self.controller);
	return count;

}
static DEVICE_ATTR(pm_get, S_IRUGO|S_IWUSR|S_IWGRP,
			show_pm_get, store_pm_get);
/*
 * This is for host compliance test
 * *
 */
static ssize_t
show_host_comp_test(struct device *_dev, struct device_attribute *attr, char *buf)
{
	char				*next;
	unsigned			size, t;

	next = buf;
	size = PAGE_SIZE;

	t = scnprintf(next, size, "%s\n",
		(dwc3_xhci.comp_test_enable ? "compliance test enabled, echo 0 to disable"
		 : "compliance test disabled, echo 1 to enable")
		);
	size -= t;
	next += t;

	return PAGE_SIZE - size;
}

static ssize_t
store_host_comp_test(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device		*pdev = to_platform_device(_dev);
	struct usb_hcd		*hcd = platform_get_drvdata(pdev);
	struct xhci_hcd		*xhci = hcd_to_xhci(hcd);

	void __iomem *addr;
	u32 val;

	if (count != 2) {
		dev_err(hcd->self.controller, "return EINVAL\n");
		return -EINVAL;
	}

	if (count > 0 && buf[count-1] == '\n')
		((char *) buf)[count-1] = 0;

	switch (buf[0]) {
	case 'R':
			if (!dwc3_xhci.comp_test_enable)
				break;
			dwc3_xhci.is_rx_test = 1;
			dwc3_xhci.portsc_mmaddr = ioremap_nocache(PORTSC_IO_ADDR, 4);
			if (!dwc3_xhci.portsc_mmaddr) {
				dev_err(hcd->self.controller,
						"ioremap failed!\n");
				return -ENOMEM;
			}
			schedule_work(&dwc3_xhci.poll_loopback);
			break;
	case 'T':
			dwc3_xhci.is_rx_test = 0;
			if (!dwc3_xhci.comp_test_enable)
				break;
			break;
	case '0':
		if (dwc3_xhci.comp_test_enable) {
			dev_dbg(hcd->self.controller, "run xHC\n");
			addr = ioremap_nocache(USBCMD_IO_ADDR, 4);
			if (!addr) {
				dev_err(hcd->self.controller,
						"ioremap failed!\n");
				return -ENOMEM;
			}
			val = readl(addr);
			val |= CMD_RUN;
			writel(val, addr);
			iounmap(addr);
			pm_runtime_put(hcd->self.controller);
			wake_unlock(&hcd->wake_lock);
			dwc3_xhci.comp_test_enable = false;
		}
		break;
	case '1':
		if (!dwc3_xhci.comp_test_enable) {
			dev_dbg(hcd->self.controller, "halt xHC\n");
			wake_lock(&hcd->wake_lock);
			pm_runtime_get_sync(hcd->self.controller);
			addr = ioremap_nocache(USBCMD_IO_ADDR, 4);
			if (!addr) {
				dev_err(hcd->self.controller,
						"ioremap failed!\n");
				return -ENOMEM;
			}
			val = readl(addr);
			val &= ~CMD_RUN;
			writel(val, addr);
			iounmap(addr);
			dwc3_xhci.comp_test_enable = true;
		}
		break;
	default:
		dev_dbg(hcd->self.controller,
				"Just support 0(halt)/1(run)\n");
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR(host_comp_test, S_IRUGO|S_IWUSR|S_IWGRP,
			show_host_comp_test, store_host_comp_test);

static void dwc_set_host_mode(struct usb_hcd *hcd)
{
	writel(0x45801000, hcd->regs + GCTL);

	msleep(20);
}

static int dwc3_start_host(struct usb_hcd *hcd)
{
	dwc3_xhci.host_started = true;
	__dwc3_start_host(hcd);

	return 0;

}

static int __dwc3_start_host(struct usb_hcd *hcd)
{
	int ret = -EINVAL;
	struct xhci_hcd *xhci;
	struct usb_hcd *xhci_shared_hcd;

	if (!hcd)
		return ret;

	if (hcd->rh_registered) {
		dev_dbg(hcd->self.controller,
				"%s() - Already registered", __func__);
		return 0;
	}

	if (dwc3_xhci.comp_test_enable) {
		dev_dbg(hcd->self.controller,
				"%s() - Now is in comp test mode", __func__);
		return 0;
	}

	pm_runtime_get_sync(hcd->self.controller);

	dwc_core_reset(hcd);
	dwc_silicon_wa(hcd);
	dwc_set_host_mode(hcd);
	dwc_set_ssphy_p3_clockrate(hcd);

	/* Clear the hcd->flags.
	 * To prevent incorrect flags set during last time. */
	hcd->flags = 0;

	ret = usb_add_hcd(hcd, dwc3_xhci.otg_irqnum, IRQF_SHARED);
	if (ret)
		return -EINVAL;

	xhci = hcd_to_xhci(hcd);
	dwc3_xhci.xhci = xhci;
	xhci->reset_hcd_work = &dwc3_xhci.reset_hcd;
	xhci->shared_hcd = usb_create_shared_hcd(&xhci_dwc_hc_driver,
		   hcd->self.controller, dev_name(hcd->self.controller), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto dealloc_usb2_hcd;
	}

	/* Set the xHCI pointer before xhci_pci_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	xhci->shared_hcd->regs = hcd->regs;

	xhci->shared_hcd->rsrc_start = hcd->rsrc_start;
	xhci->shared_hcd->rsrc_len = hcd->rsrc_len;

	ret = usb_add_hcd(xhci->shared_hcd, dwc3_xhci.otg_irqnum, IRQF_SHARED);
	if (ret)
		goto put_usb3_hcd;

	dwc3_link_issue_wa(xhci);
	pm_runtime_put(hcd->self.controller);

	ret = device_create_file(hcd->self.controller, &dev_attr_pm_get);
	if (ret < 0)
		dev_err(hcd->self.controller,
			"Can't register sysfs attribute: %d\n", ret);

	dwc3_xhci_driver.shutdown = usb_hcd_platform_shutdown;

	return ret;

put_usb3_hcd:
	if (xhci->shared_hcd) {
		xhci_shared_hcd = xhci->shared_hcd;
		usb_remove_hcd(xhci_shared_hcd);
		usb_put_hcd(xhci_shared_hcd);
	}

dealloc_usb2_hcd:
	local_irq_disable();
	usb_hcd_irq(0, hcd);
	local_irq_enable();
	usb_remove_hcd(hcd);

	kfree(xhci);
	*((struct xhci_hcd **) hcd->hcd_priv) = NULL;

	pm_runtime_put(hcd->self.controller);
	return ret;
}

static int __dwc3_stop_host(struct usb_hcd *hcd)
{
	int count = 0;
	u32 data;
	struct xhci_hcd *xhci;
	struct usb_hcd *xhci_shared_hcd;

	if (!hcd)
		return -EINVAL;

	if (dwc3_xhci.comp_test_enable) {
		dev_dbg(hcd->self.controller,
				"%s() - Now is in comp test mode", __func__);
		return 0;
	}

	xhci = hcd_to_xhci(hcd);

	pm_runtime_get_sync(hcd->self.controller);

	/* Disable hibernation mode for D0i3cold. */
	data = readl(hcd->regs + GCTL);
	data &= ~GCTL_GBL_HIBERNATION_EN;
	writel(data, hcd->regs + GCTL);

	/* When plug out micro A cable, there will be two flows be executed.
	 * The first one is xHCI controller get disconnect event. The
	 * second one is PMIC get ID change event. During these events
	 * handling, they both try to call usb_disconnect. Then met some
	 * conflicts and cause kernel panic.
	 * So treat disconnect event as first priority, handle the ID change
	 * event until disconnect event handled done.*/
	while (if_usb_devices_connected(xhci)) {
		msleep(20);
		if (count++ > WAIT_DISC_EVENT_COMPLETE_TIMEOUT)
			break;
	};
	dwc3_xhci_driver.shutdown = NULL;

	if (xhci->shared_hcd) {
		xhci_shared_hcd = xhci->shared_hcd;
		usb_remove_hcd(xhci_shared_hcd);
		usb_put_hcd(xhci_shared_hcd);
	}

	usb_remove_hcd(hcd);

	dwc3_xhci.xhci = NULL;
	kfree(xhci);
	*((struct xhci_hcd **) hcd->hcd_priv) = NULL;

	dwc_xhci_enable_phy_suspend(hcd, false);

	pm_runtime_put(hcd->self.controller);
	device_remove_file(hcd->self.controller, &dev_attr_pm_get);
	return 0;
}

static int dwc3_stop_host(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci;

	xhci = hcd_to_xhci(hcd);
	if (!xhci)
		return -ENODEV;

	dwc3_xhci.host_started = false;

	cancel_work_sync(xhci->reset_hcd_work);
	__dwc3_stop_host(hcd);

	return 0;
}

static int xhci_dwc_drv_probe(struct platform_device *pdev)
{
	struct dwc_otg2 *otg;
	struct usb_phy *usb_phy;
	struct dwc_device_par *pdata;
	struct usb_hcd *hcd;
	struct resource *res;
	int retval = 0;
	int ret;

	if (usb_disabled())
		return -ENODEV;

	pr_debug("initializing FSL-SOC USB Controller\n");

	/* Need platform data for setup */
	pdata = (struct dwc_device_par *)pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev,
			"No platform data for %s.\n", dev_name(&pdev->dev));
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}
	dwc3_xhci.otg_irqnum = res->start;

	hcd = usb_create_hcd(&xhci_dwc_hc_driver,
			&pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		retval = -ENOMEM;
		return retval;
	}

	hcd->regs = pdata->io_addr;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Found HC with no IRQ. Check %s setup!\n",
			dev_name(&pdev->dev));
		return -ENODEV;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = res->end - res->start;

	usb_phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (usb_phy)
		otg_set_host(usb_phy->otg, &hcd->self);

	otg = container_of(usb_phy->otg, struct dwc_otg2, otg);
	if (otg) {
		otg->start_host = dwc3_start_host;
		otg->stop_host = dwc3_stop_host;
		otg->suspend_host = dwc3_suspend_host;
		otg->resume_host = dwc3_resume_host;
	}

	usb_put_phy(usb_phy);

	/* Enable wakeup irq */
	hcd->has_wakeup_irq = 1;
	INIT_WORK(&dwc3_xhci.reset_hcd, dwc3_host_reset);
	INIT_WORK(&dwc3_xhci.poll_loopback, dwc3_poll_lp);
	wake_lock_init(&dwc3_xhci.wakelock, WAKE_LOCK_SUSPEND,
			"dwc3_host_wakelock");

	platform_set_drvdata(pdev, hcd);
	pm_runtime_no_callbacks(hcd->self.controller);
	pm_runtime_enable(hcd->self.controller);
	ret = device_create_file(hcd->self.controller, &dev_attr_host_comp_test);
	if (ret < 0)
		dev_err(hcd->self.controller,
			"Can't register sysfs attribute: %d\n", ret);

	return retval;
}

static int xhci_dwc_drv_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct usb_phy *usb_phy;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	usb_phy = usb_get_phy(USB_PHY_TYPE_USB2);
	otg_set_host(usb_phy->otg, NULL);
	usb_put_phy(usb_phy);

	if (xhci)
		dwc3_stop_host(hcd);
	usb_put_hcd(hcd);

	pm_runtime_disable(hcd->self.controller);
	pm_runtime_set_suspended(hcd->self.controller);
	wake_lock_destroy(&dwc3_xhci.wakelock);
	return 0;
}


#ifdef CONFIG_PM
/* dwc_hcd_suspend_common and dwc_hcd_resume_common are refer to
 * suspend_common and resume_common in usb core.
 * Because the usb core function just support PCI device.
 * So re-write them in here to support platform devices.
 */
static int dwc_hcd_suspend_common(struct device *dev)
{
	struct platform_device		*pdev = to_platform_device(dev);
	struct usb_hcd		*hcd = platform_get_drvdata(pdev);
	struct xhci_hcd		*xhci = hcd_to_xhci(hcd);
	int			retval = 0;
	u32 data = 0;

	if (!xhci) {
		dev_dbg(dev, "%s: host already stop!\n", __func__);
		return 0;
	}

	/* Root hub suspend should have stopped all downstream traffic,
	 * and all bus master traffic.  And done so for both the interface
	 * and the stub usb_device (which we check here).  But maybe it
	 * didn't; writing sysfs power/state files ignores such rules...
	 */
	if (HCD_RH_RUNNING(hcd)) {
		dev_warn(dev, "Root hub is not suspended\n");
		return -EBUSY;
	}
	if (hcd->shared_hcd) {
		hcd = hcd->shared_hcd;
		if (HCD_RH_RUNNING(hcd)) {
			dev_warn(dev, "Secondary root hub is not suspended\n");
			return -EBUSY;
		}
	}

	if (!HCD_DEAD(hcd)) {
		/* Optimization: Don't suspend if a root-hub wakeup is
		 * pending and it would cause the HCD to wake up anyway.
		 */
		if (HCD_WAKEUP_PENDING(hcd))
			return -EBUSY;
		if (hcd->shared_hcd &&
				HCD_WAKEUP_PENDING(hcd->shared_hcd))
			return -EBUSY;
		if (hcd->state != HC_STATE_SUSPENDED ||
				xhci->shared_hcd->state != HC_STATE_SUSPENDED)
			retval = -EINVAL;

		if (!retval) {
			/* The auto-resume is diabled by default. Need enable it
			 * if there have valid connection. To ensure that when
			 * device resumes, host does resume reflect within
			 * 900 usec as in USB spec.
			 */
			if (if_usb_devices_connected(xhci) == 1)
				dwc_xhci_enable_phy_auto_resume(
						xhci->main_hcd, true);

			/* Ensure that suspend enable are set for
			 * USB2 and USB3 PHY
			 */
			dwc_xhci_enable_phy_suspend(hcd, true);

			data = readl(hcd->regs + GCTL);
			data |= GCTL_GBL_HIBERNATION_EN;
			writel(data, hcd->regs + GCTL);
			dev_dbg(hcd->self.controller, "set xhci hibernation enable!\n");
			retval = xhci_suspend(xhci);
		}

		/* Check again in case wakeup raced with pci_suspend */
		if ((retval == 0 && HCD_WAKEUP_PENDING(hcd)) ||
				(retval == 0 && hcd->shared_hcd &&
				 HCD_WAKEUP_PENDING(hcd->shared_hcd))) {
			xhci_resume(xhci, false);
			retval = -EBUSY;
		}
		if (retval)
			return retval;
	}

	synchronize_irq(dwc3_xhci.otg_irqnum);

	return retval;

}

static int dwc_hcd_resume_common(struct device *dev)
{
	struct platform_device		*pdev = to_platform_device(dev);
	struct usb_hcd		*hcd = platform_get_drvdata(pdev);
	struct xhci_hcd		*xhci = hcd_to_xhci(hcd);
	int			retval = 0;

	if (!xhci)
		return 0;

	if (HCD_RH_RUNNING(hcd) ||
			(hcd->shared_hcd &&
			 HCD_RH_RUNNING(hcd->shared_hcd))) {
		dev_dbg(dev, "can't resume, not suspended!\n");
		return 0;
	}

	if (!HCD_DEAD(hcd)) {
		retval = xhci_resume(xhci, false);
		if (retval) {
			dev_err(dev, "PCI post-resume error %d!\n", retval);
			if (hcd->shared_hcd)
				usb_hc_died(hcd->shared_hcd);
			usb_hc_died(hcd);
		}
	}

	dev_dbg(dev, "hcd_pci_runtime_resume: %d\n", retval);

	return retval;
}

static int dwc3_suspend_host(struct usb_hcd *hcd)
{
	int retval;

	if (!hcd)
		return -EINVAL;

	retval = dwc_hcd_suspend_common(hcd->self.controller);

	if (retval)
		dwc_xhci_enable_phy_auto_resume(
			hcd, false);

	dev_dbg(hcd->self.controller, "%s: %d\n", __func__, retval);
	return retval;
}

static int dwc3_resume_host(struct usb_hcd *hcd)
{
	int retval;

	if (!hcd)
		return -EINVAL;

	dwc_xhci_enable_phy_auto_resume(
			hcd, false);

	retval = dwc_hcd_resume_common(hcd->self.controller);
	dev_dbg(hcd->self.controller, "%s: %d\n", __func__, retval);

	return retval;
}
#endif

static struct platform_driver dwc3_xhci_driver = {
	.probe = xhci_dwc_drv_probe,
	.remove = xhci_dwc_drv_remove,
	.driver = {
		.name = "dwc3-host",
	},
};
