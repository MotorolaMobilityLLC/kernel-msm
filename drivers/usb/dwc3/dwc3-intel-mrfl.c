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

#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/gadget.h>
#include <linux/usb/dwc3-intel-mid.h>
#include <linux/iio/consumer.h>
#include <asm/intel_scu_pmic.h>
#include "otg.h"

#define VERSION "2.10a"

static int otg_id = -1;
static int enable_usb_phy(struct dwc_otg2 *otg, bool on_off);
static int dwc3_intel_notify_charger_type(struct dwc_otg2 *otg,
		enum power_supply_charger_event event);
static struct power_supply_cable_props cap_record;
static int shady_cove_get_id(struct dwc_otg2 *otg);

static int charger_detect_enable(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;

	if (!otg || !otg->otg_data)
		return 0;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	return data->charger_detect_enable;
}

static int is_basin_cove(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;
	if (!otg || !otg->otg_data)
		return -EINVAL;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	return data->pmic_type == BASIN_COVE;
}

static int is_utmi_phy(struct dwc_otg2 *otg)
{
	if (!otg || !otg->otg_data)
		return -EINVAL;

	return otg->usb2_phy.intf == USB2_PHY_UTMI;
}

void dwc3_switch_mode(struct dwc_otg2 *otg, u32 mode)
{
	u32 reg;

	reg = otg_read(otg, GCTL);
	reg &= ~(GCTL_PRT_CAP_DIR_OTG << GCTL_PRT_CAP_DIR_SHIFT);
	reg |= mode << GCTL_PRT_CAP_DIR_SHIFT;
	otg_write(otg, GCTL, reg);
}


static int is_hybridvp(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;
	if (!otg || !otg->otg_data)
		return -EINVAL;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	return data->is_hvp;
}

static void usb2phy_eye_optimization(struct dwc_otg2 *otg)
{
	void __iomem *addr;
	struct usb_phy *phy;
	struct intel_dwc_otg_pdata *data;

	if (!otg || !otg->otg_data)
		return;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!phy)
		return;

	if ((data->usb2_phy_type == USB2_PHY_ULPI) && !!data->ulpi_eye_calibration)
		usb_phy_io_write(phy, data->ulpi_eye_calibration, TUSB1211_VENDOR_SPECIFIC1_SET);
	else if ((data->usb2_phy_type == USB2_PHY_UTMI) && !!data->utmi_eye_calibration) {
		addr = ioremap_nocache(UTMI_PHY_USB2PERPORT, 4);
		if (!addr) {
			otg_info(otg, "UTMI phy register ioremap failed, use default setup!\n");
			usb_put_phy(phy);
			return;
		}
		writel(data->utmi_eye_calibration, addr);
		iounmap(addr);
	} else
		otg_info(otg, "usb2 phy eye optimization fail, use default setup!\n");

	usb_put_phy(phy);
}


/* As we use SW mode to do charger detection, need to notify HW
 * the result SW get, charging port or not */
static int dwc_otg_charger_hwdet(bool enable)
{
	int				retval;
	struct usb_phy *phy;
	struct dwc_otg2 *otg = dwc3_get_otg();

	/* Just return if charger detection is not enabled */
	if (!charger_detect_enable(otg))
		return 0;

	phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!phy)
		return -ENODEV;

	if (enable) {
		retval = usb_phy_io_write(phy, PWCTRL_HWDETECT,
				TUSB1211_POWER_CONTROL_SET);
		if (retval)
			return retval;
		otg_dbg(otg, "set HWDETECT\n");
	} else {
		retval = usb_phy_io_write(phy, PWCTRL_HWDETECT,
				TUSB1211_POWER_CONTROL_CLR);
		if (retval)
			return retval;
		otg_dbg(otg, "clear HWDETECT\n");
	}
	usb_put_phy(phy);

	return 0;
}

static enum power_supply_charger_cable_type
			basin_cove_aca_check(struct dwc_otg2 *otg)
{
	u8 rarbrc;
	int ret;
	enum power_supply_charger_cable_type type =
		POWER_SUPPLY_CHARGER_TYPE_NONE;

	ret = intel_scu_ipc_update_register(PMIC_USBIDCTRL,
			USBIDCTRL_ACA_DETEN_D1,
			USBIDCTRL_ACA_DETEN_D1);
	if (ret)
		otg_err(otg, "Fail to enable ACA&ID detection logic\n");

	/* Wait >66.1ms (for TCHGD_SERX_DEB) */
	msleep(66);

	/* Read decoded RID value */
	ret = intel_scu_ipc_ioread8(PMIC_USBIDSTS, &rarbrc);
	if (ret)
		otg_err(otg, "Fail to read decoded RID value\n");
	rarbrc &= USBIDSTS_ID_RARBRC_STS(3);
	rarbrc >>= 1;

	/* If ID_RARBRC_STS==01: ACA-Dock detected
	 * If ID_RARBRC_STS==00: MHL detected
	 */
	if (rarbrc == 1) {
		/* ACA-Dock */
		type = POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK;
	} else if (!rarbrc) {
		/* MHL */
		type = POWER_SUPPLY_CHARGER_TYPE_MHL;
	}

	ret = intel_scu_ipc_update_register(PMIC_USBIDCTRL,
			USBIDCTRL_ACA_DETEN_D1,
			0);
	if (ret)
		otg_err(otg, "Fail to enable ACA&ID detection logic\n");

	return type;
}

static enum power_supply_charger_cable_type
		shady_cove_aca_check(struct dwc_otg2 *otg)
{

	if (!otg)
		return POWER_SUPPLY_CHARGER_TYPE_NONE;

	switch (shady_cove_get_id(otg)) {
	case RID_A:
		return POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK;
	case RID_B:
		return POWER_SUPPLY_CHARGER_TYPE_ACA_B;
	case RID_C:
		return POWER_SUPPLY_CHARGER_TYPE_ACA_C;
	default:
		return POWER_SUPPLY_CHARGER_TYPE_NONE;
	}
}

static enum power_supply_charger_cable_type
		dwc3_intel_aca_check(struct dwc_otg2 *otg)
{
	if (is_basin_cove(otg))
		return basin_cove_aca_check(otg);
	else
		return shady_cove_aca_check(otg);
}

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

static void dwc_a_bus_drop(struct usb_phy *x)
{
	struct dwc_otg2 *otg = dwc3_get_otg();
	unsigned long flags;

	if (otg->usb2_phy.vbus_state == VBUS_DISABLED) {
		spin_lock_irqsave(&otg->lock, flags);
		otg->user_events |= USER_A_BUS_DROP;
		dwc3_wakeup_otg_thread(otg);
		spin_unlock_irqrestore(&otg->lock, flags);
	}
}

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

/* This function is used to control VUSBPHY or assert/deassert USBRST_N
 * pin to control usb2 phy enter/exit low power mode.
 */
static int control_usb_phy_power(u16 addr, bool on_off)
{
	int ret;
	u8 mask, bits;

	if (addr == PMIC_VLDOCNT)
		mask = PMIC_VLDOCNT_VUSBPHYEN;
	else if (addr == PMIC_USBPHYCTRL)
		mask = PMIC_USBPHYCTRL_D0;
	else
		return -EINVAL;

	if (on_off)
		bits = mask;
	else
		bits = 0x00;

	ret = intel_scu_ipc_update_register(addr,
			bits, mask);

	/* Debounce 10ms for turn on VUSBPHY */
	if (on_off)
		usleep_range(10000, 11000);

	return ret;
}

/* This function will control VUSBPHY to power gate/ungate USBPHY.
 * If current platform haven't using VUSBPHY, then assert/deassert
 * USBRST_N pin to make PHY enter reset state.
 */
static int enable_usb_phy(struct dwc_otg2 *otg, bool on_off)
{
	struct intel_dwc_otg_pdata *data;
	int ret;

	if (!otg || !otg->otg_data)
		return -EINVAL;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;
	if (data->using_vusbphy)
		ret = control_usb_phy_power(PMIC_VLDOCNT, on_off);
	else
		ret = control_usb_phy_power(PMIC_USBPHYCTRL, on_off);

	if (ret)
		otg_err(otg, "dwc3 %s usb phy failed\n",
				on_off ? "enable" : "disable");

	return ret;
}


int dwc3_intel_platform_init(struct dwc_otg2 *otg)
{
	int retval;
	struct intel_dwc_otg_pdata *data;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	/* Init a_bus_drop callback */
	otg->usb2_phy.a_bus_drop = dwc_a_bus_drop;
	otg->usb2_phy.vbus_state = VBUS_ENABLED;
	/* Get usb2 phy type */
	otg->usb2_phy.intf = data->usb2_phy_type;

	/* Turn off VUSBPHY if it haven't used by USB2 PHY.
	 * Otherwise, it will consume ~2.6mA(on VSYS) on MOFD.
	 */
	if (!data->using_vusbphy) {
		retval = control_usb_phy_power(PMIC_VLDOCNT, false);
		if (retval)
			otg_err(otg, "Fail to turn off VUSBPHY\n");
	} else if (!is_utmi_phy(otg)) {
		/* If the current USB2 PHY low power controlled by VUSBPHY. Then
		 * we need to de-assert USBRST pin to make USB2 PHY always stay
		 * in active state.
		 */
		retval = control_usb_phy_power(PMIC_USBPHYCTRL, true);
		if (retval)
			otg_err(otg, "Fail to de-assert USBRST#\n");
	} else {
		/* If we are using utmi phy, and through VUSBPHY to do power
		 * control. Then we need to assert USBRST# for external ULPI phy
		 * to ask it under inactive state saving power.
		 */
		retval = control_usb_phy_power(PMIC_USBPHYCTRL, false);
		if (retval)
			otg_err(otg, "Fail to de-assert USBRST#\n");
	}

	/* Don't let phy go to suspend mode, which
	 * will cause FS/LS devices enum failed in host mode.
	 */
	set_sus_phy(otg, 0);

	retval = device_create_file(otg->dev, &dev_attr_otg_id);
	if (retval < 0) {
		otg_dbg(otg,
			"Can't register sysfs attribute: %d\n", retval);
		return -ENOMEM;
	}

	otg_dbg(otg, "\n");
	otg_write(otg, OEVTEN, 0);
	otg_write(otg, OCTL, 0);

	dwc3_switch_mode(otg, GCTL_PRT_CAP_DIR_OTG);

	return 0;
}

/* Disable auto-resume feature for USB2 PHY. This is one
 * silicon workaround. It will cause fabric timeout error
 * for LS case after resume from hibernation */
static void disable_phy_auto_resume(struct dwc_otg2 *otg)
{
	u32 data = 0;

	data = otg_read(otg, GUSB2PHYCFG0);
	data &= ~GUSB2PHYCFG_ULPI_AUTO_RESUME;
	otg_write(otg, GUSB2PHYCFG0, data);
}

int basin_cove_get_id(struct dwc_otg2 *otg)
{
	int ret, id = RID_UNKNOWN;
	u8 idsts, pmic_id;

	ret = intel_scu_ipc_update_register(PMIC_USBIDCTRL,
			USBIDCTRL_ACA_DETEN_D1 | PMIC_USBPHYCTRL_D0,
			USBIDCTRL_ACA_DETEN_D1 | PMIC_USBPHYCTRL_D0);
	if (ret)
		otg_err(otg, "Fail to enable ACA&ID detection logic\n");

	ret = intel_scu_ipc_ioread8(PMIC_USBIDSTS, &idsts);
	if (ret) {
		otg_err(otg, "Fail to read id\n");
		return id;
	}

	if (idsts & USBIDSTS_ID_FLOAT_STS)
		id = RID_FLOAT;
	else if (idsts & USBIDSTS_ID_RARBRC_STS(1))
		id = RID_A;
	else if (idsts & USBIDSTS_ID_RARBRC_STS(2))
		id = RID_B;
	else if (idsts & USBIDSTS_ID_RARBRC_STS(3))
		id = RID_C;
	else {
		/* PMIC A0 reports ID_GND = 0 for RID_GND but PMIC B0 reports
		*  ID_GND = 1 for RID_GND
		*/
		ret = intel_scu_ipc_ioread8(0x00, &pmic_id);
		if (ret) {
			otg_err(otg, "Fail to read PMIC ID register\n");
		} else if (((pmic_id & VENDOR_ID_MASK) == BASIN_COVE_PMIC_ID) &&
			((pmic_id & PMIC_MAJOR_REV) == PMIC_A0_MAJOR_REV)) {
				if (idsts & USBIDSTS_ID_GND)
					id = RID_GND;
		} else {
			if (!(idsts & USBIDSTS_ID_GND))
				id = RID_GND;
		}
	}

	ret = intel_scu_ipc_update_register(PMIC_USBIDCTRL,
			USBIDCTRL_ACA_DETEN_D1 | PMIC_USBPHYCTRL_D0,
			0);
	if (ret)
		otg_err(otg, "Fail to enable ACA&ID detection logic\n");

	return id;
}

int shady_cove_get_id(struct dwc_otg2 *otg)
{
	u8 schgrirq1;
	struct iio_channel *chan;
	int ret, rid, id = RID_UNKNOWN;

	ret = intel_scu_ipc_ioread8(PMIC_SCHGRIRQ1, &schgrirq1);
	if (ret) {
		otg_err(otg, "Fail to read id\n");
		return id;
	}

	/* PMIC_SCHGRIRQ1_SUSBIDDET bit definition:
	 * 0 = RID_A/B/C ; 1 = RID_GND ; 2 = RID_FLOAT */
	if (schgrirq1 & PMIC_SCHGRIRQ1_SUSBIDDET(2))
		return RID_FLOAT;
	else if (schgrirq1 & PMIC_SCHGRIRQ1_SUSBIDDET(1))
		return RID_GND;

	chan = iio_channel_get(NULL, "USBID");
	if (IS_ERR_OR_NULL(chan)) {
		otg_err(otg, "%s: Fail to get USBID channel\n", __func__);
		return id;
	}

	ret = iio_read_channel_raw(chan, &rid);
	if (ret) {
		otg_err(otg, "%s: Fail to read USBID channel", __func__);
		goto done;
	}

	if ((rid > 11150) && (rid < 13640))
		id = RID_A;
	else if ((rid > 6120) && (rid < 7480))
		id = RID_B;
	else if ((rid > 3285) && (rid < 4015))
		id = RID_C;

done:

	iio_channel_release(chan);
	return id;
}

int dwc3_intel_get_id(struct dwc_otg2 *otg)
{
	if (is_basin_cove(otg))
		return basin_cove_get_id(otg);
	else
		return shady_cove_get_id(otg);
}

int dwc3_intel_b_idle(struct dwc_otg2 *otg)
{
	u32 gctl, tmp;

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

	/* Force config to otg mode as default. */
	dwc3_switch_mode(otg, GCTL_PRT_CAP_DIR_OTG);

	if (!is_hybridvp(otg)) {
		dwc_otg_charger_hwdet(false);
		enable_usb_phy(otg, false);
	}

	mdelay(100);

	return 0;
}

static int dwc3_intel_set_power(struct usb_phy *_otg,
		unsigned ma)
{
	unsigned long flags;
	struct dwc_otg2 *otg = dwc3_get_otg();
	struct power_supply_cable_props cap;
	struct intel_dwc_otg_pdata *data;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

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
		otg_dbg(otg, "Notify EM");
		otg_dbg(otg, "POWER_SUPPLY_CHARGER_EVENT_SUSPEND\n");

		return 0;
	} else if (ma == OTG_DEVICE_RESUME) {
		otg_dbg(otg, "Notify EM");
		otg_dbg(otg, "POWER_SUPPLY_CHARGER_EVENT_CONNECT\n");
		dwc3_intel_notify_charger_type(otg,
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
	case OTG_USB2_0MA:
		ma = 0;
		break;
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

	dwc3_intel_notify_charger_type(otg,
			POWER_SUPPLY_CHARGER_EVENT_CONNECT);

	return 0;
}

int dwc3_intel_enable_vbus(struct dwc_otg2 *otg, int enable)
{
	atomic_notifier_call_chain(&otg->usb2_phy.notifier,
			USB_EVENT_DRIVE_VBUS, &enable);

	return 0;
}

static int dwc3_intel_notify_charger_type(struct dwc_otg2 *otg,
		enum power_supply_charger_event event)
{
	struct power_supply_cable_props cap;
	int ret = 0;
	unsigned long flags;

	if (!charger_detect_enable(otg) &&
		((otg->charging_cap.chrg_type !=
		POWER_SUPPLY_CHARGER_TYPE_USB_SDP) ||
		 event == POWER_SUPPLY_CHARGER_EVENT_DISCONNECT))
		return 0;

	if (event > POWER_SUPPLY_CHARGER_EVENT_DISCONNECT) {
		otg_err(otg,
		"%s: Invalid power_supply_charger_event!\n", __func__);
		return -EINVAL;
	}

	if ((otg->charging_cap.chrg_type ==
			POWER_SUPPLY_CHARGER_TYPE_USB_SDP) &&
			((otg->charging_cap.ma != 0) &&
			 (otg->charging_cap.ma != 100) &&
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

	return ret;
}

static enum power_supply_charger_cable_type
			dwc3_intel_get_charger_type(struct dwc_otg2 *otg)
{
	int ret;
	struct usb_phy *phy;
	u8 val, vdat_det, chgd_serx_dm;
	unsigned long timeout, interval;
	enum power_supply_charger_cable_type type =
		POWER_SUPPLY_CHARGER_TYPE_NONE;

	if (!charger_detect_enable(otg))
		return cap_record.chrg_type;

	if (dwc3_intel_get_id(otg) == RID_GND)
		return POWER_SUPPLY_CHARGER_TYPE_B_DEVICE;

	phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!phy) {
		otg_err(otg, "Get USB2 PHY failed\n");
		return POWER_SUPPLY_CHARGER_TYPE_NONE;
	}

	/* PHY Enable:
	 * Power on PHY
	 */
	enable_usb_phy(otg, true);

	if (is_basin_cove(otg)) {
		/* Enable ACA:
		 * Enable ACA & ID detection logic.
		 */
		ret = intel_scu_ipc_update_register(PMIC_USBIDCTRL,
				USBIDCTRL_ACA_DETEN_D1 | PMIC_USBPHYCTRL_D0,
				USBIDCTRL_ACA_DETEN_D1 | PMIC_USBPHYCTRL_D0);
		if (ret)
			otg_err(otg, "Fail to enable ACA&ID detection logic\n");
	}

	/* DCD Enable: Change OPMODE to 01 (Non-driving),
	 * TermSel to 0, &
	 * XcvrSel to 01 (enable FS xcvr)
	 */
	usb_phy_io_write(phy, FUNCCTRL_OPMODE(1) | FUNCCTRL_XCVRSELECT(1),
					TUSB1211_FUNC_CTRL_SET);

	usb_phy_io_write(phy, FUNCCTRL_OPMODE(2) | FUNCCTRL_XCVRSELECT(2)
					| FUNCCTRL_TERMSELECT,
					TUSB1211_FUNC_CTRL_CLR);

	/*Enable SW control*/
	usb_phy_io_write(phy, PWCTRL_SW_CONTROL, TUSB1211_POWER_CONTROL_SET);

	/* Enable IDPSRC */
	usb_phy_io_write(phy, VS3_CHGD_IDP_SRC_EN,
			TUSB1211_VENDOR_SPECIFIC3_SET);

	/* Check DCD result, use same polling parameter */
	timeout = jiffies + msecs_to_jiffies(DATACON_TIMEOUT);
	interval = DATACON_INTERVAL * 1000; /* us */

	/* DCD Check:
	 * Delay 66.5 ms. (Note:
	 * TIDP_SRC_ON + TCHGD_SERX_DEB =
	 * 347.8us + 66.1ms).
	 */
	usleep_range(66500, 67000);

	while (!time_after(jiffies, timeout)) {
		/* Read DP logic level. */
		val = usb_phy_io_read(phy, TUSB1211_VENDOR_SPECIFIC4);
		if (val < 0) {
			otg_err(otg, "ULPI read error! try again\n");
			continue;
		}

		if (!(val & VS4_CHGD_SERX_DP)) {
			otg_info(otg, "Data contact detected!\n");
			break;
		}

		/* Polling interval */
		usleep_range(interval, interval + 2000);
	}

	/* Disable DP pullup (Idp_src) */
	usb_phy_io_write(phy, VS3_CHGD_IDP_SRC_EN,
			TUSB1211_VENDOR_SPECIFIC3_CLR);

	/* ID Check:
	 * Check ID pin state.
	 */
	val = dwc3_intel_get_id(otg);
	if (val == RID_GND) {
		type = POWER_SUPPLY_CHARGER_TYPE_B_DEVICE;
		goto cleanup;
	} else if (val != RID_FLOAT) {
		type = dwc3_intel_aca_check(otg);
		goto cleanup;
	}

	/* SE1 Det Enable:
	 * Read DP/DM logic level. Note: use DEBUG
	 * because VS4 isn’t enabled in this situation.
     */
	val = usb_phy_io_read(phy, TUSB1211_DEBUG);
	if (val < 0)
		otg_err(otg, "ULPI read error!\n");

	val &= DEBUG_LINESTATE;

	/* If '11': SE1 detected; goto 'Cleanup'.
	 * Else: goto 'Pri Det Enable'.
	 */
	if (val == 3) {
		type = POWER_SUPPLY_CHARGER_TYPE_SE1;
		goto cleanup;
	}

	/* Pri Det Enable:
	 * Enable VDPSRC.
	 */
	usb_phy_io_write(phy, PWCTRL_DP_VSRC_EN, TUSB1211_POWER_CONTROL_SET);

	/* Wait >106.1ms (40ms for BC
	 * Tvdpsrc_on, 66.1ms for TI CHGD_SERX_DEB).
	 */
	msleep(107);

	/* Pri Det Check:
	 * Check if DM > VDATREF.
	 */
	vdat_det = usb_phy_io_read(phy, TUSB1211_POWER_CONTROL);
	if (vdat_det < 0)
		otg_err(otg, "ULPI read error!\n");

	vdat_det &= PWCTRL_VDAT_DET;

	/* Check if DM<VLGC */
	chgd_serx_dm = usb_phy_io_read(phy, TUSB1211_VENDOR_SPECIFIC4);
	if (chgd_serx_dm < 0)
		otg_err(otg, "ULPI read error!\n");

	chgd_serx_dm &= VS4_CHGD_SERX_DM;

	/* If VDAT_DET==0 || CHGD_SERX_DM==1: SDP detected
	 * If VDAT_DET==1 && CHGD_SERX_DM==0: CDP/DCP
	 */
	if (vdat_det == 0 || chgd_serx_dm == 1)
		type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP;

	/* Disable VDPSRC. */
	usb_phy_io_write(phy, PWCTRL_DP_VSRC_EN, TUSB1211_POWER_CONTROL_CLR);

	/* If SDP, goto “Cleanup”.
	 * Else, goto “Sec Det Enable”
	 */
	if (type == POWER_SUPPLY_CHARGER_TYPE_USB_SDP)
		goto cleanup;

	/* Sec Det Enable:
	 * delay 1ms.
	 */
	usleep_range(1000, 1500);

	/* Swap DP & DM */
	usb_phy_io_write(phy, VS1_DATAPOLARITY, TUSB1211_VENDOR_SPECIFIC1_CLR);

	/* Enable 'VDMSRC'. */
	usb_phy_io_write(phy, PWCTRL_DP_VSRC_EN, TUSB1211_POWER_CONTROL_SET);

	/* Wait >73ms (40ms for BC Tvdmsrc_on, 33ms for TI TVDPSRC_DEB) */
	msleep(80);

	/* Sec Det Check:
	 * Check if DP>VDATREF.
	 */
	val = usb_phy_io_read(phy, TUSB1211_POWER_CONTROL);
	if (val < 0)
		otg_err(otg, "ULPI read error!\n");

	val &= PWCTRL_VDAT_DET;

	/* If VDAT_DET==0: CDP detected.
	 * If VDAT_DET==1: DCP detected.
	 */
	if (!val)
		type = POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
	else
		type = POWER_SUPPLY_CHARGER_TYPE_USB_DCP;

	/* Disable VDMSRC. */
	usb_phy_io_write(phy, PWCTRL_DP_VSRC_EN, TUSB1211_POWER_CONTROL_CLR);

	/* Swap DP & DM. */
	usb_phy_io_write(phy, VS1_DATAPOLARITY, TUSB1211_VENDOR_SPECIFIC1_SET);

cleanup:

	/* If DCP detected, assert VDPSRC. */
	if (type == POWER_SUPPLY_CHARGER_TYPE_USB_DCP)
		usb_phy_io_write(phy, PWCTRL_SW_CONTROL | PWCTRL_DP_VSRC_EN,
				TUSB1211_POWER_CONTROL_SET);

	usb_put_phy(phy);

	switch (type) {
	case POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_A:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_B:
	case POWER_SUPPLY_CHARGER_TYPE_ACA_C:
	case POWER_SUPPLY_CHARGER_TYPE_USB_DCP:
	case POWER_SUPPLY_CHARGER_TYPE_USB_CDP:
	case POWER_SUPPLY_CHARGER_TYPE_SE1:
		dwc_otg_charger_hwdet(true);
		break;
	default:
		break;
	};

	return type;
}

static int dwc3_intel_handle_notification(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int state;
	unsigned long flags, valid_chrg_type;
	struct dwc_otg2 *otg = dwc3_get_otg();
	struct power_supply_cable_props *cap;

	if (!otg)
		return NOTIFY_BAD;

	valid_chrg_type = POWER_SUPPLY_CHARGER_TYPE_USB_SDP |
		POWER_SUPPLY_CHARGER_TYPE_USB_CDP |
		POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK;

	spin_lock_irqsave(&otg->lock, flags);
	switch (event) {
	case USB_EVENT_ID:
		otg->otg_events |= OEVT_CONN_ID_STS_CHNG_EVNT;
		state = NOTIFY_OK;
		break;
	case USB_EVENT_VBUS:
		/* WA for EM driver which should not sent VBUS event
		 * if UTMI PHY selected. */
		if (!charger_detect_enable(otg)) {
			state = NOTIFY_OK;
			goto done;
		}

		if (*(int *)data) {
			otg->otg_events |= OEVT_B_DEV_SES_VLD_DET_EVNT;
			otg->otg_events &= ~OEVT_A_DEV_SESS_END_DET_EVNT;
		} else {
			otg->otg_events |= OEVT_A_DEV_SESS_END_DET_EVNT;
			otg->otg_events &= ~OEVT_B_DEV_SES_VLD_DET_EVNT;
		}
		state = NOTIFY_OK;
		break;
	case USB_EVENT_CHARGER:
		if (charger_detect_enable(otg)) {
			state = NOTIFY_DONE;
			goto done;
		}
		cap = (struct power_supply_cable_props *)data;
		if (!(cap->chrg_type & valid_chrg_type)) {
			otg_err(otg, "Ignore invalid charger type!\n");
			state = NOTIFY_DONE;
			goto done;
		}

		/* Ignore the events which send by USB driver itself. */
		if (cap->chrg_evt == POWER_SUPPLY_CHARGER_EVENT_CONNECT)
			if (cap_record.chrg_type == POWER_SUPPLY_CHARGER_TYPE_USB_SDP) {
				state = NOTIFY_DONE;
				goto done;
			}

		if (cap->chrg_evt == POWER_SUPPLY_CHARGER_EVENT_CONNECT) {
			otg->otg_events |= OEVT_B_DEV_SES_VLD_DET_EVNT;
			otg->otg_events &= ~OEVT_A_DEV_SESS_END_DET_EVNT;

			cap_record.chrg_type = cap->chrg_type;
			cap_record.ma = cap->ma;
			cap_record.chrg_evt = cap->chrg_evt;
		} else if (cap->chrg_evt ==
				POWER_SUPPLY_CHARGER_EVENT_DISCONNECT) {
			otg->otg_events |= OEVT_A_DEV_SESS_END_DET_EVNT;
			otg->otg_events &= ~OEVT_B_DEV_SES_VLD_DET_EVNT;

			cap_record.chrg_type = POWER_SUPPLY_CHARGER_TYPE_NONE;
			cap_record.ma = 0;
			cap_record.chrg_evt =
				POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		}

		if (cap->chrg_type == POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK)
			otg->otg_events |= OEVT_CONN_ID_STS_CHNG_EVNT;

		state = NOTIFY_OK;
		break;
	default:
		otg_dbg(otg, "DWC OTG Notify unknow notify message\n");
		state = NOTIFY_DONE;
	}

	dwc3_wakeup_otg_thread(otg);
done:
	spin_unlock_irqrestore(&otg->lock, flags);

	return state;

}

int dwc3_intel_prepare_start_host(struct dwc_otg2 *otg)
{
	dwc3_switch_mode(otg, GCTL_PRT_CAP_DIR_HOST);

	if (!is_hybridvp(otg)) {
		enable_usb_phy(otg, true);
		usb2phy_eye_optimization(otg);
		disable_phy_auto_resume(otg);
	}

	return 0;
}

int dwc3_intel_prepare_start_peripheral(struct dwc_otg2 *otg)
{
	if (!is_hybridvp(otg)) {
		enable_usb_phy(otg, true);
		usb2phy_eye_optimization(otg);
		disable_phy_auto_resume(otg);
	}

	return 0;
}

int dwc3_intel_suspend(struct dwc_otg2 *otg)
{
	int ret;
	struct usb_phy *phy;
	struct pci_dev *pci_dev;
	struct usb_hcd *hcd = NULL;
	pci_power_t state = PCI_D3cold;

	if (!otg)
		return 0;

	hcd = container_of(otg->otg.host, struct usb_hcd, self);

	pci_dev = to_pci_dev(otg->dev);

	if (otg->state == DWC_STATE_A_HOST &&
			otg->suspend_host) {
		/* Check if USB2 ULPI PHY is hang via access its internal
		 * registers. If hang, then do hard reset before enter
		 * hibernation mode. Otherwise, the USB2 PHY can't enter
		 * suspended state which will blocking U2PMU can't get ready
		 * then can't enter D0i3hot forever in SCU FW.
		 */
		if (!is_utmi_phy(otg)) {
			phy = usb_get_phy(USB_PHY_TYPE_USB2);
			if (!phy)
				return -ENODEV;
			if (usb_phy_io_read(phy, ULPI_VENDOR_ID_LOW) < 0) {
				enable_usb_phy(otg, 0);
				enable_usb_phy(otg, 1);
			}
			usb_put_phy(phy);
		}

		ret = otg->suspend_host(hcd);
		if (ret) {
			otg_err(otg, "dwc3-host enter suspend faield: %d\n", ret);
			return ret;
		}
	}

	if (otg->state == DWC_STATE_B_PERIPHERAL ||
			otg->state == DWC_STATE_A_HOST)
		state = PCI_D3hot;

	set_sus_phy(otg, 1);

	if (pci_save_state(pci_dev)) {
		otg_err(otg, "pci_save_state failed!\n");
		return -EIO;
	}

	pci_disable_device(pci_dev);
	if ((state == PCI_D3cold) && is_utmi_phy(otg)) {
		/* Important!!  Whenever the VUSBPHY rail is disabled, SW
		 * must assert USBRST# to isolate the SOC’s DP/DM pins from the
		 * outside world.  There is a risk of damage to the SOC if a
		 * peripheral were to bias DP/DM to 3.3V when the SOC is
		 * unpowered. */
		ret = intel_scu_ipc_update_register(PMIC_USBPHYCTRL,
				0x0, USBPHYRSTB);
		if (ret)
			otg_err(otg, "%s: ipc update failed\n", __func__);
	}
	pci_set_power_state(pci_dev, state);
	return 0;
}

int dwc3_intel_resume(struct dwc_otg2 *otg)
{
	struct pci_dev *pci_dev;
	struct usb_hcd *hcd = NULL;
	u32 data;
	int ret;

	if (!otg)
		return 0;

	hcd = container_of(otg->otg.host, struct usb_hcd, self);
	/* After resume from D0i3cold. The UTMI PHY D+ drive issue
	 * reproduced due to all setting be reseted. So switch to OTG
	 * mode avoid D+ drive too early.
	 */
	if ((otg->state == DWC_STATE_B_IDLE ||
		otg->state == DWC_STATE_CHARGING ||
		otg->state == DWC_STATE_WAIT_VBUS_FALL ||
		otg->state == DWC_STATE_WAIT_VBUS_RAISE) &&
			is_utmi_phy(otg)) {
		/* Reconnect DP/DM between Pmic and SOC for support host
		 * and device mode. */
		ret = intel_scu_ipc_update_register(PMIC_USBPHYCTRL,
				USBPHYRSTB, USBPHYRSTB);
		if (ret)
			otg_err(otg, "%s: ipc update failed\n", __func__);

		otg_write(otg, OEVTEN, 0);
		otg_write(otg, OCTL, 0);
		dwc3_switch_mode(otg, GCTL_PRT_CAP_DIR_OTG);
	}

	/* This is one SCU WA. SCU should set GUSB2PHYCFG0
	 * bit 4 for ULPI setting. But SCU haven't do that.
	 * So do WA first until SCU fix.
	 */
	data = otg_read(otg, GUSB2PHYCFG0);
	if (is_utmi_phy(otg))
		data &= ~(1 << 4);
	else
		data |= (1 << 4);
	otg_write(otg, GUSB2PHYCFG0, data);

	pci_dev = to_pci_dev(otg->dev);

	/* From synopsys spec 12.2.11.
	 * Software cannot access memory-mapped I/O space
	 * for 10ms. Delay 5 ms here should be enough. Too
	 * long a delay causes hibernation exit failure.
	 */
	mdelay(5);

	pci_restore_state(pci_dev);
	if (pci_enable_device(pci_dev) < 0) {
		otg_err(otg, "pci_enable_device failed.\n");
		return -EIO;
	}

	set_sus_phy(otg, 0);

	/* Delay 1ms waiting PHY clock debounce.
	 * Without this debounce, will met fabric error randomly.
	 **/
	mdelay(1);

	if (otg->state == DWC_STATE_A_HOST &&
			otg->resume_host)
		otg->resume_host(hcd);


	return 0;
}

struct dwc3_otg_hw_ops dwc3_intel_otg_pdata = {
	.mode = DWC3_DRD,
	.bus = DWC3_PCI,
	.get_id = dwc3_intel_get_id,
	.b_idle = dwc3_intel_b_idle,
	.set_power = dwc3_intel_set_power,
	.enable_vbus = dwc3_intel_enable_vbus,
	.platform_init = dwc3_intel_platform_init,
	.get_charger_type = dwc3_intel_get_charger_type,
	.otg_notifier_handler = dwc3_intel_handle_notification,
	.prepare_start_peripheral = dwc3_intel_prepare_start_peripheral,
	.prepare_start_host = dwc3_intel_prepare_start_host,
	.notify_charger_type = dwc3_intel_notify_charger_type,

	.suspend = dwc3_intel_suspend,
	.resume = dwc3_intel_resume,
};

static int __init dwc3_intel_init(void)
{
	return dwc3_otg_register(&dwc3_intel_otg_pdata);
}
module_init(dwc3_intel_init);

static void __exit dwc3_intel_exit(void)
{
	dwc3_otg_unregister(&dwc3_intel_otg_pdata);
}
module_exit(dwc3_intel_exit);

MODULE_AUTHOR("Wang Yu <yu.y.wang@intel.com>");
MODULE_DESCRIPTION("DWC3 Intel OTG Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(VERSION);
