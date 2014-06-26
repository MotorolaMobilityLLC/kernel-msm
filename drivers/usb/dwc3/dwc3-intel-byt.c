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
#include "otg.h"

#define VERSION "2.10a"

static int otg_id = -1;
static int enable_usb_phy(struct dwc_otg2 *otg, bool on_off);
static int dwc3_intel_byt_notify_charger_type(struct dwc_otg2 *otg,
		enum power_supply_charger_event event);

static int charger_detect_enable(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;

	if (!otg || !otg->otg_data)
		return 0;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	return data->charger_detect_enable;
}

static int sdp_charging(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;

	if (!otg || !otg->otg_data)
		return 0;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	return data->sdp_charging;
}

static void usb2phy_eye_optimization(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;
	struct usb_phy *phy;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!phy)
		return;

	/* Modify VS1 for better quality in eye diagram */
	if (data && data->ti_phy_vs1)
		usb_phy_io_write(phy, data->ti_phy_vs1,
			TUSB1211_VENDOR_SPECIFIC1_SET);

	usb_put_phy(phy);
}

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

static int dwc3_check_gpio_id(struct dwc_otg2 *otg2)
{
	struct dwc_otg2 *otg = dwc3_get_otg();
	struct intel_dwc_otg_pdata *data;
	int id = 0;
	int next = 0;
	int count = 0;
	unsigned long timeout;

	otg_dbg(otg, "start check gpio id\n");

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	/* Polling ID GPIO PIN value for SW debounce as HW debouce chip
	 * is not connected on BYT CR board */
	if (data && data->gpio_id) {
		id = gpio_get_value(data->gpio_id);

		/* If get 20 of the same value in a row by GPIO read,
		 * then end SW debouce and return the ID value.
		 * the total length of debouce time is 80ms~100ms for
		 * 20 times GPIO read on BYT CR, which is longer than
		 * normal debounce time done by HW chip.
		 * Also set 200ms timeout value to avoid impact from
		 * pin unstable cases */
		timeout = jiffies + msecs_to_jiffies(200);
		while ((count < 20) && (!time_after(jiffies, timeout))) {
			next = gpio_get_value(data->gpio_id);
			otg_dbg(otg, "id value pin %d = %d\n",
				data->gpio_id, next);
			if (next < 0)
				return -EINVAL;
			else if (id == next)
				count++;
			else {
				id = next;
				count = 0;
			}
		}
		if (count >= 20) {
			otg_dbg(otg, "id debounce done = %d\n", id);
			return id;
		}
	}

	return -ENODEV;
}

static irqreturn_t dwc3_gpio_id_irq(int irq, void *dev)
{
	struct dwc_otg2 *otg = dwc3_get_otg();
	struct intel_dwc_otg_pdata *data;
	int id;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	id = dwc3_check_gpio_id(otg);
	if (id == 0 || id == 1) {
		if (data->id != id) {
			data->id = id;
			dev_info(otg->dev, "ID notification (id = %d)\n",
					data->id);
			atomic_notifier_call_chain(&otg->usb2_phy.notifier,
				USB_EVENT_ID, &id);
		}
	}

	return IRQ_HANDLED;
}

static void dwc_otg_suspend_discon_work(struct work_struct *work)
{
	struct dwc_otg2 *otg = dwc3_get_otg();
	unsigned long flags;

	otg_dbg(otg, "start suspend_disconn work\n");

	spin_lock_irqsave(&otg->lock, flags);
	otg->otg_events |= OEVT_A_DEV_SESS_END_DET_EVNT;
	otg->otg_events &= ~OEVT_B_DEV_SES_VLD_DET_EVNT;
	dwc3_wakeup_otg_thread(otg);
	spin_unlock_irqrestore(&otg->lock, flags);
}

static int dwc3_intel_byt_get_id_status(struct usb_phy *x, void *data)
{
	struct intel_dwc_otg_pdata *pdata;
	struct dwc_otg2 *otg = dwc3_get_otg();

	if (!x)
		return -ENODEV;

	if (!data)
		return -EINVAL;

	if (otg && otg->otg_data) {
		pdata = (struct intel_dwc_otg_pdata *)otg->otg_data;
		*(int *)data = pdata->id;
	} else
		return -ENODEV;

	return 0;
}

static int dwc3_byt_phy_init(struct usb_phy *x)
{
	struct dwc_otg2 *otg = container_of(x, struct dwc_otg2, usb2_phy);
	struct intel_dwc_otg_pdata *data = otg->otg_data;

	if (!data || !data->gpio_cs || !data->gpio_reset)
		return -EINVAL;

	gpio_direction_output(data->gpio_cs, 1);
	udelay(200);

	gpio_direction_output(data->gpio_reset, 0);
	udelay(200);
	gpio_set_value(data->gpio_reset, 1);
	mdelay(30);

	return 0;
}

int dwc3_intel_byt_platform_init(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;
	u32 gctl;
	int id_value;
	int retval;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	otg->usb2_phy.get_id_status = dwc3_intel_byt_get_id_status;
	otg->usb2_phy.init = dwc3_byt_phy_init;

	if (data)
		INIT_DELAYED_WORK(&data->suspend_discon_work,
			dwc_otg_suspend_discon_work);

	if (data && data->gpio_cs && data->gpio_reset) {
		retval = gpio_request(data->gpio_cs, "phy_cs");
		if (retval < 0) {
			otg_err(otg, "failed to request CS pin %d\n",
					data->gpio_cs);
			return retval;
		}

		retval = gpio_request(data->gpio_reset, "phy_reset");
		if (retval < 0) {
			otg_err(otg, "failed to request RESET pin %d\n",
					data->gpio_reset);
			return retval;
		}
	}

	if (data && data->gpio_id) {
		dev_info(otg->dev,  "USB ID detection - Enabled - GPIO\n");

		/* Set ID default value to 1 Floating */
		data->id = 1;

		retval = gpio_request(data->gpio_id, "gpio_id");
		if (retval < 0) {
			otg_err(otg, "failed to request ID pin %d\n",
					data->gpio_id);
			return retval;
		}

		retval = gpio_direction_input(data->gpio_id);
		if (retval < 0) {
			otg_err(otg, "failed to request ID pin %d\n",
					data->gpio_id);
			return retval;
		}

		retval = request_threaded_irq(gpio_to_irq(data->gpio_id),
				NULL, dwc3_gpio_id_irq,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT, "dwc-gpio-id", otg->dev);

		if (retval < 0) {
			otg_err(otg, "failed to request interrupt gpio ID\n");
			return retval;
		}

		otg_dbg(otg, "GPIO ID request/Interrupt reuqest Done\n");

		id_value = dwc3_check_gpio_id(otg);
		if ((id_value == 0 || id_value == 1) &&
					(data->id != id_value)) {
			data->id = id_value;
			dev_info(otg->dev, "ID notification (id = %d)\n",
						data->id);

			atomic_notifier_call_chain(&otg->usb2_phy.notifier,
					USB_EVENT_ID, &id_value);
		} else
			otg_dbg(otg, "Get incorrect ID value %d\n", id_value);
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

/* This function will control VUSBPHY to power gate/ungate USBPHY */
static int enable_usb_phy(struct dwc_otg2 *otg, bool on_off)
{
	struct intel_dwc_otg_pdata *data;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;

	if (data && data->gpio_cs && data->gpio_reset) {
		if (on_off) {
			/* Turn ON phy via CS pin */
			gpio_direction_output(data->gpio_cs, 1);
			usleep_range(200, 300);

			/* Do PHY reset after enable the PHY */
			gpio_direction_output(data->gpio_reset, 0);
			usleep_range(200, 500);
			gpio_set_value(data->gpio_reset, 1);
			msleep(30);
		} else {
			/* Turn OFF phy via CS pin */
			gpio_direction_output(data->gpio_cs, 0);
		}
	}
	return 0;
}

int dwc3_intel_byt_get_id(struct dwc_otg2 *otg)
{
	/* For BYT ID is not connected to USB, always FLOAT */
	return RID_FLOAT;
}

int dwc3_intel_byt_b_idle(struct dwc_otg2 *otg)
{
	u32 gctl, tmp;

	enable_usb_phy(otg, false);
	dwc_otg_charger_hwdet(false);

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

static int dwc3_intel_byt_set_power(struct usb_phy *_otg,
		unsigned ma)
{
	unsigned long flags;
	struct dwc_otg2 *otg = dwc3_get_otg();
	struct power_supply_cable_props cap;
	struct intel_dwc_otg_pdata *data;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;
	if (!data)
		return -EINVAL;

	if (ma == OTG_USB2_0MA ||
		ma == OTG_USB2_100MA ||
		ma == OTG_USB3_150MA ||
		ma == OTG_USB2_500MA ||
		ma == OTG_USB3_900MA ||
		ma == OTG_DEVICE_RESUME) {
		otg_dbg(otg, "cancel discon work\n");
		__cancel_delayed_work(&data->suspend_discon_work);
	} else if (ma == OTG_DEVICE_SUSPEND) {
		otg_dbg(otg, "schedule discon work\n");
		schedule_delayed_work(&data->suspend_discon_work,
				SUSPEND_DISCONNECT_TIMEOUT);
	}

	/* Needn't notify charger capability if charger_detection disable */
	if (!charger_detect_enable(otg) && !sdp_charging(otg))
		return 0;

	if (ma == OTG_DEVICE_SUSPEND) {
		spin_lock_irqsave(&otg->lock, flags);
		cap.chrg_type = otg->charging_cap.chrg_type;
		cap.ma = otg->charging_cap.ma;
		cap.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_SUSPEND;
		spin_unlock_irqrestore(&otg->lock, flags);

		/* ma is zero mean D+/D- opened cable.
		 * If SMIP set, then notify 500ma.
		 * Otherwise, notify 0ma.
		*/
		if (!cap.ma) {
			if (!data->charging_compliance) {
				cap.ma = 500;
				cap.chrg_evt =
					POWER_SUPPLY_CHARGER_EVENT_CONNECT;
			}
		/* For standard SDP, if SMIP set, then ignore suspend */
		} else if (!data->charging_compliance)
			return 0;
		/* Stander SDP(cap.ma != 0) and SMIP not set.
		 * Should send 0ma with SUSPEND event
		 */
		else
			cap.ma = 2;

		if (sdp_charging(otg)) {
			otg_dbg(otg, "Notify EM cap.ma = %d\n", cap.ma);
			atomic_notifier_call_chain(&otg->usb2_phy.notifier,
					USB_EVENT_ENUMERATED, &cap.ma);
		} else
			atomic_notifier_call_chain(&otg->usb2_phy.notifier,
					USB_EVENT_CHARGER, &cap);

		otg_dbg(otg, "Notify EM	CHARGER_EVENT_SUSPEND\n");

		return 0;
	} else if (ma == OTG_DEVICE_RESUME) {
		otg_dbg(otg, "Notify EM CHARGER_EVENT_CONNECT\n");
		dwc3_intel_byt_notify_charger_type(otg,
				POWER_SUPPLY_CHARGER_EVENT_CONNECT);

		return 0;
	}

	/* For SMIP set case, only need to report 500/900ma */
	if (!data->charging_compliance) {
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

	dwc3_intel_byt_notify_charger_type(otg,
			POWER_SUPPLY_CHARGER_EVENT_CONNECT);

	return 0;
}

int dwc3_intel_byt_enable_vbus(struct dwc_otg2 *otg, int enable)
{
	/* Return 0, as VBUS is controlled by FSA in BYT */
	return 0;
}

static int dwc3_intel_byt_notify_charger_type(struct dwc_otg2 *otg,
		enum power_supply_charger_event event)
{
	struct power_supply_cable_props cap;
	unsigned long flags;

	/* Just return if charger detection is not enabled */
	if (!charger_detect_enable(otg) && !sdp_charging(otg))
		return 0;

	/* If OTG driver doesn't do charger detection, then no need
	 * to do notification on charger removal events */
	if (!charger_detect_enable(otg) &&
		(event == POWER_SUPPLY_CHARGER_EVENT_DISCONNECT)) {
		otg_err(otg, "%s: disconnect ignore!\n", __func__);
		return -EINVAL;
	}

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

	if (sdp_charging(otg)) {
		otg_dbg(otg, "Notify EM cap.ma = %d\n", cap.ma);
		atomic_notifier_call_chain(&otg->usb2_phy.notifier,
				USB_EVENT_ENUMERATED, &cap.ma);
	} else
		atomic_notifier_call_chain(&otg->usb2_phy.notifier,
				USB_EVENT_CHARGER, &cap);

	return 0;
}

static enum power_supply_charger_cable_type
			dwc3_intel_byt_get_charger_type(struct dwc_otg2 *otg)
{
	struct usb_phy *phy;
	u8 val, vdat_det, chgd_serx_dm;
	unsigned long timeout, interval;
	enum power_supply_charger_cable_type type =
		POWER_SUPPLY_CHARGER_TYPE_NONE;

	/* No need to do charger detection if not enabled */
	if (!charger_detect_enable(otg))
		return POWER_SUPPLY_CHARGER_TYPE_USB_SDP;

	phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (!phy) {
		otg_err(otg, "Get USB2 PHY failed\n");
		return POWER_SUPPLY_CHARGER_TYPE_NONE;
	}

	/* PHY Enable:
	 * Power on PHY
	 */
	enable_usb_phy(otg, true);

	/* Wait 10ms (~5ms before PHY de-asserts DIR,
	 * XXus for initial Link reg sync-up).*/
	msleep(20);

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

static int dwc3_intel_byt_handle_notification(struct notifier_block *nb,
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
		otg_dbg(otg, "VBUS event received %d\n", val);
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

int dwc3_intel_byt_prepare_start_host(struct dwc_otg2 *otg)
{
	return 0;
}

int dwc3_intel_byt_prepare_start_peripheral(struct dwc_otg2 *otg)
{
	enable_usb_phy(otg, true);
	usb2phy_eye_optimization(otg);
	disable_phy_auto_resume(otg);

	return 0;
}

int dwc3_intel_byt_after_stop_peripheral(struct dwc_otg2 *otg)
{
	struct intel_dwc_otg_pdata *data;

	data = (struct intel_dwc_otg_pdata *)otg->otg_data;
	if (!data)
		return -EINVAL;

	otg_dbg(otg, "cancel discon work\n");
	__cancel_delayed_work(&data->suspend_discon_work);

	return 0;
}

int dwc3_intel_byt_suspend(struct dwc_otg2 *otg)
{
	struct pci_dev *pci_dev;
	pci_power_t state = PCI_D3hot;

	if (!otg)
		return 0;

	pci_dev = to_pci_dev(otg->dev);

	set_sus_phy(otg, 1);

	if (pci_save_state(pci_dev)) {
		otg_err(otg, "pci_save_state failed!\n");
		return -EIO;
	}

	pci_disable_device(pci_dev);
	pci_set_power_state(pci_dev, state);

	return 0;
}

int dwc3_intel_byt_resume(struct dwc_otg2 *otg)
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

	set_sus_phy(otg, 0);

	return 0;
}

struct dwc3_otg_hw_ops dwc3_intel_byt_otg_pdata = {
	.mode = DWC3_DEVICE_ONLY,
	.bus = DWC3_PCI,
	.get_id = dwc3_intel_byt_get_id,
	.b_idle = dwc3_intel_byt_b_idle,
	.set_power = dwc3_intel_byt_set_power,
	.enable_vbus = dwc3_intel_byt_enable_vbus,
	.platform_init = dwc3_intel_byt_platform_init,
	.get_charger_type = dwc3_intel_byt_get_charger_type,
	.otg_notifier_handler = dwc3_intel_byt_handle_notification,
	.prepare_start_peripheral = dwc3_intel_byt_prepare_start_peripheral,
	.after_stop_peripheral = dwc3_intel_byt_after_stop_peripheral,
	.prepare_start_host = dwc3_intel_byt_prepare_start_host,
	.notify_charger_type = dwc3_intel_byt_notify_charger_type,

	.suspend = dwc3_intel_byt_suspend,
	.resume = dwc3_intel_byt_resume,
};

static int __init dwc3_intel_byt_init(void)
{
	return dwc3_otg_register(&dwc3_intel_byt_otg_pdata);
}
module_init(dwc3_intel_byt_init);

static void __exit dwc3_intel_byt_exit(void)
{
	dwc3_otg_unregister(&dwc3_intel_byt_otg_pdata);
}
module_exit(dwc3_intel_byt_exit);

MODULE_AUTHOR("Wang Yu <yu.y.wang@intel.com>");
MODULE_AUTHOR("Wu, Hao <hao.wu@intel.com>");
MODULE_DESCRIPTION("DWC3 Intel BYT OTG Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(VERSION);
