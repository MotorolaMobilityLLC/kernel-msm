#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/usb/otg.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/gpio.h>

#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/gadget.h>
#include <linux/usb/dwc3-intel-mid.h>
#include <linux/usb/xhci-ush-hsic-pci.h>
#include "otg.h"

#define VERSION "2.50a"

static int otg_id = -1;
static int dwc3_intel_cht_notify_charger_type(struct dwc_otg2 *otg,
		enum power_supply_charger_event event);
static int dwc3_set_id_mux(void __iomem *reg_base, int is_device_on);

static int charger_detect_enable(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;

	if (!otg || !otg->otg_data)
		return 0;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	return data->charger_detect_enable;
}

static ssize_t store_vbus_evt(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long flags;
	struct dwc_otg2	*otg = dwc3_get_otg();

	if (count != 2) {
		otg_err(otg, "return EINVAL\n");
		return -EINVAL;
	}

	if (count > 0 && buf[count-1] == '\n')
		((char *) buf)[count-1] = 0;

	switch (buf[0]) {
	case '1':
		otg_dbg(otg, "Change the VBUS to High\n");
		otg->otg_events |= OEVT_B_DEV_SES_VLD_DET_EVNT;
		spin_lock_irqsave(&otg->lock, flags);
		dwc3_wakeup_otg_thread(otg);
		spin_unlock_irqrestore(&otg->lock, flags);
		return count;
	case '0':
		otg_dbg(otg, "Change the VBUS to Low\n");
		otg->otg_events |= OEVT_A_DEV_SESS_END_DET_EVNT;
		spin_lock_irqsave(&otg->lock, flags);
		dwc3_wakeup_otg_thread(otg);
		spin_unlock_irqrestore(&otg->lock, flags);
		return count;
	default:
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR(vbus_evt, S_IWUSR|S_IWGRP,
			NULL, store_vbus_evt);


static ssize_t store_otg_id(struct device *_dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long flags;
	struct dwc_otg2 *otg = dwc3_get_otg();

	if (!otg)
		return 0;
	if (count != 2) {
		otg_err(otg, "return EINVAL\n");
		return -EINVAL;
	}

	if (count > 0 && buf[count-1] == '\n')
		((char *) buf)[count-1] = 0;

	switch (buf[0]) {
	case 'a':
	case 'A':
		otg_dbg(otg, "Change ID to A\n");
		otg->user_events |= USER_ID_A_CHANGE_EVENT;
		spin_lock_irqsave(&otg->lock, flags);
		dwc3_wakeup_otg_thread(otg);
		otg_id = 0;
		spin_unlock_irqrestore(&otg->lock, flags);
		return count;
	case 'b':
	case 'B':
		otg_dbg(otg, "Change ID to B\n");
		otg->user_events |= USER_ID_B_CHANGE_EVENT;
		spin_lock_irqsave(&otg->lock, flags);
		dwc3_wakeup_otg_thread(otg);
		otg_id = 1;
		spin_unlock_irqrestore(&otg->lock, flags);
		return count;
	default:
		otg_err(otg, "Just support change ID to A!\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t
show_otg_id(struct device *_dev, struct device_attribute *attr, char *buf)
{
	char				*next;
	unsigned			size, t;

	next = buf;
	size = PAGE_SIZE;

	t = scnprintf(next, size,
		"USB OTG ID: %s\n",
		(otg_id ? "B" : "A")
		);
	size -= t;
	next += t;

	return PAGE_SIZE - size;
}

static DEVICE_ATTR(otg_id, S_IRUGO|S_IWUSR|S_IWGRP,
			show_otg_id, store_otg_id);

static void set_sus_phy(struct dwc_otg2 *otg, int bit)
{
	u32 data = 0;

	data = otg_read(otg, GUSB2PHYCFG0);
	if (bit)
		data |= GUSB2PHYCFG_SUS_PHY;
	else
		data &= ~GUSB2PHYCFG_SUS_PHY;

	otg_write(otg, GUSB2PHYCFG0, data);

	data = otg_read(otg, GUSB3PIPECTL0);
	if (bit)
		data |= GUSB3PIPECTL_SUS_EN;
	else
		data &= ~GUSB3PIPECTL_SUS_EN;
	otg_write(otg, GUSB3PIPECTL0, data);
}

int dwc3_intel_cht_platform_init(struct dwc_otg2 *otg)
{
	u32 gctl;
	int retval;

	/* Don't let phy go to suspend mode, which
	 * will cause FS/LS devices enum failed in host mode.
	 */
#if 0
	set_sus_phy(otg, 0);
#endif

	retval = device_create_file(otg->dev, &dev_attr_otg_id);
	if (retval < 0) {
		otg_dbg(otg,
			"Can't register sysfs attribute: %d\n", retval);
		return -ENOMEM;
	}

	retval = device_create_file(otg->dev, &dev_attr_vbus_evt);
	if (retval < 0) {
		otg_dbg(otg,
			"Can't register sysfs attribute: %d\n", retval);
		return -ENOMEM;
	}

	otg_dbg(otg, "\n");
	otg_write(otg, OEVTEN, 0);
	otg_write(otg, OCTL, 0);
	gctl = otg_read(otg, GCTL);
	gctl |= GCTL_PRT_CAP_DIR_OTG << GCTL_PRT_CAP_DIR_SHIFT;
	otg_write(otg, GCTL, gctl);

	return 0;
}

int dwc3_intel_cht_get_id(struct dwc_otg2 *otg)
{
	/* For BYT ID is not connected to USB, always FLOAT */
	return RID_FLOAT;
}

int dwc3_intel_cht_b_idle(struct dwc_otg2 *otg)
{
	struct usb_hcd *hcd = NULL;
	u32 ret, val;
	u32 gctl, tmp;

	/* set mux to host mode in b_idle state */
	hcd = container_of(otg->otg.host, struct usb_hcd, self);
	if (!hcd) {
		mdelay(100);
		return -ENODEV;
	}

	ret = dwc3_set_id_mux(hcd->regs, 0);
	if (ret)
		return ret;

	/* Disable hibernation mode by default */
	gctl = otg_read(otg, GCTL);
	gctl &= ~GCTL_GBL_HIBERNATION_EN;
	otg_write(otg, GCTL, gctl);

	/* Reset ADP related registers */
	otg_write(otg, ADPCFG, 0);
	otg_write(otg, ADPCTL, 0);
	otg_write(otg, ADPEVTEN, 0);
	tmp = otg_read(otg, ADPEVT);
	otg_write(otg, ADPEVT, tmp);

	otg_write(otg, OCFG, 0);
	otg_write(otg, OEVTEN, 0);
	tmp = otg_read(otg, OEVT);
	otg_write(otg, OEVT, tmp);
	otg_write(otg, OCTL, OCTL_PERI_MODE);

	/* Force config to device mode as default */
	gctl = otg_read(otg, GCTL);
	gctl &= ~GCTL_PRT_CAP_DIR;
	gctl |= GCTL_PRT_CAP_DIR_DEV << GCTL_PRT_CAP_DIR_SHIFT;
	otg_write(otg, GCTL, gctl);

	mdelay(100);

	return 0;
}

static int dwc3_intel_cht_set_power(struct usb_phy *_otg,
		unsigned ma)
{
	unsigned long flags;
	struct dwc_otg2 *otg = dwc3_get_otg();
	struct power_supply_cable_props cap;
	struct intel_dwc_otg_pdata *data;

	/* Just return if charger detection is not enabled */
	if (!charger_detect_enable(otg))
		return 0;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;
	if (!data)
		return -EINVAL;

	if (otg->charging_cap.chrg_type ==
			POWER_SUPPLY_CHARGER_TYPE_USB_CDP)
		return 0;
	else if (otg->charging_cap.chrg_type !=
			POWER_SUPPLY_CHARGER_TYPE_USB_SDP) {
		otg_err(otg, "%s: currently, chrg type is not SDP!\n",
				__func__);
		return -EINVAL;
	}

	if (ma == OTG_DEVICE_SUSPEND) {
		spin_lock_irqsave(&otg->lock, flags);
		cap.chrg_type = otg->charging_cap.chrg_type;
		cap.ma = otg->charging_cap.ma;
		cap.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_SUSPEND;
		spin_unlock_irqrestore(&otg->lock, flags);

		/* mA is zero mean D+/D- opened cable.
		 * If SMIP set, then notify 500mA.
		 * Otherwise, notify 0mA.
		*/
		if (!cap.ma) {
			if (data->charging_compliance) {
				cap.ma = 500;
				cap.chrg_evt =
					POWER_SUPPLY_CHARGER_EVENT_CONNECT;
			}
		/* For standard SDP, if SMIP set, then ignore suspend */
		} else if (data->charging_compliance)
			return 0;
		/* Stander SDP(cap.mA != 0) and SMIP not set.
		 * Should send 0mA with SUSPEND event
		 */
		else
			cap.ma = 0;

		atomic_notifier_call_chain(&otg->usb2_phy.notifier,
				USB_EVENT_CHARGER, &cap);
		otg_dbg(otg, "Notify EM	CHARGER_EVENT_SUSPEND\n");

		return 0;
	} else if (ma == OTG_DEVICE_RESUME) {
		otg_dbg(otg, "Notify EM CHARGER_EVENT_CONNECT\n");
		dwc3_intel_cht_notify_charger_type(otg,
				POWER_SUPPLY_CHARGER_EVENT_CONNECT);

		return 0;
	}

	/* For SMIP set case, only need to report 500/900mA */
	if (data->charging_compliance) {
		if ((ma != OTG_USB2_500MA) &&
				(ma != OTG_USB3_900MA))
			return 0;
	}

	/* Covert macro to integer number*/
	switch (ma) {
	case OTG_USB2_100MA:
		ma = 100;
		break;
	case OTG_USB3_150MA:
		ma = 150;
		break;
	case OTG_USB2_500MA:
		ma = 500;
		break;
	case OTG_USB3_900MA:
		ma = 900;
		break;
	default:
		otg_err(otg, "Device driver set invalid SDP current value!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&otg->lock, flags);
	otg->charging_cap.ma = ma;
	spin_unlock_irqrestore(&otg->lock, flags);

	dwc3_intel_cht_notify_charger_type(otg,
			POWER_SUPPLY_CHARGER_EVENT_CONNECT);

	return 0;
}

int dwc3_intel_cht_enable_vbus(struct dwc_otg2 *otg, int enable)
{
	/* Return 0, as VBUS is controlled by FSA in BYT */
	return 0;
}

static int dwc3_intel_cht_notify_charger_type(struct dwc_otg2 *otg,
		enum power_supply_charger_event event)
{
	struct power_supply_cable_props cap;
	unsigned long flags;

	/* Just return if charger detection is not enabled */
	if (!charger_detect_enable(otg))
		return 0;

	if (event > POWER_SUPPLY_CHARGER_EVENT_DISCONNECT) {
		otg_err(otg,
		"%s: Invalid power_supply_charger_event!\n", __func__);
		return -EINVAL;
	}

	if ((otg->charging_cap.chrg_type ==
			POWER_SUPPLY_CHARGER_TYPE_USB_SDP) &&
			((otg->charging_cap.ma != 100) &&
			 (otg->charging_cap.ma != 150) &&
			 (otg->charging_cap.ma != 500) &&
			 (otg->charging_cap.ma != 900))) {
		otg_err(otg, "%s: invalid SDP current!\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&otg->lock, flags);
	cap.chrg_type = otg->charging_cap.chrg_type;
	cap.ma = otg->charging_cap.ma;
	cap.chrg_evt = event;
	spin_unlock_irqrestore(&otg->lock, flags);

	atomic_notifier_call_chain(&otg->usb2_phy.notifier, USB_EVENT_CHARGER,
			&cap);

	return 0;
}

static enum power_supply_charger_cable_type
			dwc3_intel_cht_get_charger_type(struct dwc_otg2 *otg)
{
	/* No need to do charger detection if not enabled */
	return POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
}

static int dwc3_intel_cht_handle_notification(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct dwc_otg2 *otg = dwc3_get_otg();
	int state, val;
	unsigned long flags;

	if (!otg)
		return NOTIFY_BAD;

	val = *(int *)data;

	spin_lock_irqsave(&otg->lock, flags);
	switch (event) {
	case USB_EVENT_VBUS:
		if (val) {
			otg->otg_events |= OEVT_B_DEV_SES_VLD_DET_EVNT;
			otg->otg_events &= ~OEVT_A_DEV_SESS_END_DET_EVNT;
		} else {
			otg->otg_events |= OEVT_A_DEV_SESS_END_DET_EVNT;
			otg->otg_events &= ~OEVT_B_DEV_SES_VLD_DET_EVNT;
		}
		state = NOTIFY_OK;
		break;
	default:
		otg_dbg(otg, "DWC OTG Notify unknow notify message\n");
		state = NOTIFY_DONE;
	}
	dwc3_wakeup_otg_thread(otg);
	spin_unlock_irqrestore(&otg->lock, flags);

	return state;

}

static int dwc3_set_id_mux(void __iomem *reg_base, int is_device_on)
{
	u32		reg;
	u32		timeout = 500;

	/* Set ID Mux to host. The power of the registers are always on. */
	reg = readl(reg_base + DUAL_ROLE_CFG0);
	if (!(reg & SW_IDPIN_EN)) {
		reg |= SW_IDPIN_EN;
		writel(reg, reg_base + DUAL_ROLE_CFG0);
	}

	reg = readl(reg_base + DUAL_ROLE_CFG0);
	if (is_device_on)
		reg |= SW_IDPIN;
	else
		reg &= ~SW_IDPIN;
	writel(reg, reg_base + DUAL_ROLE_CFG0);

	do {
		reg = readl(reg_base + DUAL_ROLE_CFG1);
		if (is_device_on) {
			if (!(reg & SUS))
				break;
		} else {
			if (reg & SUS)
				break;
		}
		timeout--;
		if (!timeout)
			return -ETIMEDOUT;

		mdelay(1);
	} while (1);

	return 0;
}

int dwc3_intel_cht_after_stop_peripheral(struct dwc_otg2 *otg)
{
	return 0;
}

int dwc3_intel_cht_prepare_start_peripheral(struct dwc_otg2 *otg)
{
	struct usb_hcd *hcd = NULL;
	u32 ret, val;

	hcd = container_of(otg->otg.host, struct usb_hcd, self);
	if (!hcd) {
		mdelay(100);
		return -ENODEV;
	}

	ret = dwc3_set_id_mux(hcd->regs, 1);
	if (ret)
		return ret;

	return 0;
}

int dwc3_intel_cht_suspend(struct dwc_otg2 *otg)
{
	struct pci_dev *pci_dev;
	pci_power_t state = PCI_D3cold;

	if (!otg)
		return 0;

	pci_dev = to_pci_dev(otg->dev);

	if (otg->state == DWC_STATE_B_PERIPHERAL ||
			otg->state == DWC_STATE_A_HOST)
		state = PCI_D3hot;

	set_sus_phy(otg, 1);

	if (pci_save_state(pci_dev)) {
		otg_err(otg, "pci_save_state failed!\n");
		return -EIO;
	}

	pci_disable_device(pci_dev);
	pci_set_power_state(pci_dev, state);

	return 0;
}

int dwc3_intel_cht_resume(struct dwc_otg2 *otg)
{
	struct pci_dev *pci_dev;

	if (!otg)
		return 0;

	pci_dev = to_pci_dev(otg->dev);

	/* From synopsys spec 12.2.11.
	 * Software cannot access memory-mapped I/O space
	 * for 10ms.
	 */
	mdelay(10);

	pci_restore_state(pci_dev);
	if (pci_enable_device(pci_dev) < 0) {
		otg_err(otg, "pci_enable_device failed.\n");
		return -EIO;
	}

#if 0
	set_sus_phy(otg, 0);
#endif
	return 0;
}

struct dwc3_otg_hw_ops dwc3_intel_cht_otg_pdata = {
	.mode = DWC3_DEVICE_ONLY,
	.bus = DWC3_PCI,
	.get_id = dwc3_intel_cht_get_id,
	.b_idle = dwc3_intel_cht_b_idle,
	.set_power = dwc3_intel_cht_set_power,
	.enable_vbus = dwc3_intel_cht_enable_vbus,
	.platform_init = dwc3_intel_cht_platform_init,
	.get_charger_type = dwc3_intel_cht_get_charger_type,
	.otg_notifier_handler = dwc3_intel_cht_handle_notification,
	.prepare_start_peripheral = dwc3_intel_cht_prepare_start_peripheral,
	.after_stop_peripheral = dwc3_intel_cht_after_stop_peripheral,
	.notify_charger_type = dwc3_intel_cht_notify_charger_type,

	.suspend = dwc3_intel_cht_suspend,
	.resume = dwc3_intel_cht_resume,
};

static int __init dwc3_intel_cht_init(void)
{
	return dwc3_otg_register(&dwc3_intel_cht_otg_pdata);
}
module_init(dwc3_intel_cht_init);

static void __exit dwc3_intel_cht_exit(void)
{
	dwc3_otg_unregister(&dwc3_intel_cht_otg_pdata);
}
module_exit(dwc3_intel_cht_exit);

MODULE_AUTHOR("Wang Yu <yu.y.wang@intel.com>");
MODULE_AUTHOR("Wu, Hao <hao.wu@intel.com>");
MODULE_AUTHOR("Yuan, Hang <hang.yuan@intel.com>");
MODULE_DESCRIPTION("DWC3 Intel CHT OTG Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(VERSION);
