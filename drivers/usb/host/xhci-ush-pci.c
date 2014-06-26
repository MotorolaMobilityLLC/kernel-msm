/*
 * Intel MID Platform BayTrail XHCI Controller PCI Bus Glue.
 *
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License 2 as published by the
 * Free Software Foundation.
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

#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>
#endif

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/xhci-ush-hsic-pci.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include "xhci.h"
#include "../core/usb.h"

static struct pci_dev	*pci_dev;
static struct class *hsic_class;
static struct device *hsic_class_dev;

static int create_device_files(struct pci_dev *pdev);
static void remove_device_files();
static int create_class_device_files(struct pci_dev *pdev);
static void remove_class_device_files(void);

static int hsic_enable;
static struct ush_hsic_priv hsic;

/* Workaround for OSPM, set PMCMD to ask SCU
 * power gate EHCI controller and DPHY
 */
static void hsic_enter_exit_d3(int enter_exit)
{
	if (enter_exit) {
		printk(KERN_CRIT "HSIC Enter D0I3!\n");
		pci_set_power_state(pci_dev, PCI_D3hot);
	} else {
		printk(KERN_CRIT "HSIC Exit D0I3!\n");
		pci_set_power_state(pci_dev, PCI_D0);
	}
}

/* HSIC AUX GPIO irq handler */
static irqreturn_t hsic_aux_gpio_irq(int irq, void *data)
{
	struct device *dev = data;

	dev_dbg(dev,
		"%s---> hsic aux gpio request irq: %d\n",
		__func__, irq);

	if (hsic.hsic_aux_irq_enable == 0) {
		dev_dbg(dev,
			"%s---->AUX IRQ is disabled\n", __func__);
		return IRQ_HANDLED;
	}

	if (delayed_work_pending(&hsic.hsic_aux)) {
		dev_dbg(dev,
			"%s---->Delayed work pending\n", __func__);
		return IRQ_HANDLED;
	}

	if (hsic.modem_dev == NULL) {
		dev_dbg(dev,
			"%s---->No enumeration ignore aux\n", __func__);
		return IRQ_HANDLED;
	}

	hsic.hsic_aux_finish = 0;
	schedule_delayed_work(&hsic.hsic_aux, 0);
	dev_dbg(dev,
		"%s<----\n", __func__);

	return IRQ_HANDLED;
}

/* HSIC Wakeup GPIO irq handler */
static irqreturn_t hsic_wakeup_gpio_irq(int irq, void *data)
{
	struct device *dev = data;

	dev_dbg(dev,
		"%s---> hsic wakeup gpio request irq: %d\n",
		__func__, irq);
	if (hsic.hsic_wakeup_irq_enable == 0) {
		dev_dbg(dev,
			"%s---->Wakeup IRQ is disabled\n", __func__);
		return IRQ_HANDLED;
	}

	/* take a wake lock during 25ms, because resume lasts 20ms, after that
	USB framework will prevent to go in low power if there is traffic */
	wake_lock_timeout(&hsic.resume_wake_lock, msecs_to_jiffies(25));

	queue_work(hsic.work_queue, &hsic.wakeup_work);
	dev_dbg(dev,
		"%s<----\n", __func__);

	return IRQ_HANDLED;
}

static int hsic_aux_irq_init(int pin)
{
	int retval;

	dev_dbg(&pci_dev->dev, "%s---->%d\n", __func__, pin);
	if (hsic.hsic_aux_irq_enable) {
		dev_dbg(&pci_dev->dev,
			"%s<----AUX IRQ is enabled\n", __func__);
		return 0;
	}
	hsic.hsic_aux_irq_enable = 1;
	gpio_direction_input(pin);
	retval = request_irq(gpio_to_irq(pin),
			hsic_aux_gpio_irq,
			IRQF_NO_SUSPEND | IRQF_TRIGGER_RISING,
			"hsic_disconnect_request", &pci_dev->dev);
	if (retval) {
		dev_err(&pci_dev->dev,
			"unable to request irq %i, err: %d\n",
			gpio_to_irq(pin), retval);
		goto err;
	}

	lnw_gpio_set_alt(pin, 0);
	dev_dbg(&pci_dev->dev, "%s<----\n", __func__);
	return retval;

err:
	hsic.hsic_aux_irq_enable = 0;
	free_irq(gpio_to_irq(pin), &pci_dev->dev);
	return retval;
}

static int hsic_wakeup_irq_init(void)
{
	int retval;

	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	if (hsic.hsic_wakeup_irq_enable) {
		dev_dbg(&pci_dev->dev,
			"%s<----Wakeup IRQ is enabled\n", __func__);
		return 0;
	}
	hsic.hsic_wakeup_irq_enable = 1;
	gpio_direction_input(hsic.wakeup_gpio);
	retval = request_irq(gpio_to_irq(hsic.wakeup_gpio),
			hsic_wakeup_gpio_irq,
			IRQF_SHARED | IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND,
			"hsic_remote_wakeup_request", &pci_dev->dev);
	if (retval) {
		dev_err(&pci_dev->dev,
			"unable to request irq %i, err: %d\n",
			gpio_to_irq(hsic.wakeup_gpio), retval);
		goto err;
	}

	lnw_gpio_set_alt(hsic.wakeup_gpio, 0);
	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);

	return retval;

err:
	hsic.hsic_wakeup_irq_enable = 0;
	free_irq(gpio_to_irq(hsic.wakeup_gpio), &pci_dev->dev);
	return retval;
}

/* Init HSIC AUX GPIO */
static int hsic_aux_gpio_init(int pin)
{
	int		retval = 0;

	dev_dbg(&pci_dev->dev, "%s----> %d\n", __func__, pin);

	if (gpio_is_valid(pin)) {
		retval = gpio_request(pin, "hsic_aux");
		if (retval < 0) {
			dev_err(&pci_dev->dev,
				"Request GPIO %d with error %d\n",
				pin, retval);
			retval = -ENODEV;
			goto err1;
		}
	} else {
		retval = -ENODEV;
		goto err1;
	}

	pr_debug("%s----> Enable AUX irq\n", __func__);
	retval = hsic_aux_irq_init(pin);
	if (retval) {
		dev_err(&pci_dev->dev,
			"unable to request IRQ\n");
		goto err2;
	}

	hsic.aux_gpio = pin;

	dev_dbg(&pci_dev->dev, "%s<----\n", __func__);
	return retval;

err2:
	gpio_free(hsic.aux_gpio);
err1:
	return retval;
}

/* Init HSIC AUX2 GPIO as side band remote wakeup source */
static int hsic_wakeup_gpio_init(int pin)
{
	int		retval = 0;

	dev_dbg(&pci_dev->dev, "%s----> %d\n", __func__, pin);
	if (gpio_is_valid(pin)) {
		retval = gpio_request(pin, "hsic_wakeup");
		if (retval < 0) {
			dev_err(&pci_dev->dev,
				"Request GPIO %d with error %d\n",
				pin, retval);
			retval = -ENODEV;
			goto err;
		}
		hsic.wakeup_gpio = pin;
	} else {
		retval = -ENODEV;
		goto err;
	}

	gpio_direction_input(hsic.wakeup_gpio);
	dev_dbg(&pci_dev->dev, "%s<----\n", __func__);
err:
	return retval;
}

static void hsic_aux_irq_free(void)
{
	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	if (hsic.hsic_aux_irq_enable) {
		hsic.hsic_aux_irq_enable = 0;
		free_irq(gpio_to_irq(hsic.aux_gpio), &pci_dev->dev);
	}
	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return;
}

static void hsic_wakeup_irq_free(void)
{
	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	if (hsic.hsic_wakeup_irq_enable) {
		hsic.hsic_wakeup_irq_enable = 0;
		free_irq(gpio_to_irq(hsic.wakeup_gpio), &pci_dev->dev);
	}
	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return;
}

static unsigned int is_ush_hsic(struct usb_device *udev)
{
	struct pci_dev *pdev = to_pci_dev(udev->bus->controller);

	pr_debug("pdev device ID: %d, portnum: %d",
			pdev->device, udev->portnum);
	/* Ignore and only valid for HSIC. Filter out
	 * the USB devices added by other USB2 host driver */
	if (pdev->device != USH_PCI_ID && pdev->device != PCI_DEVICE_ID_INTEL_CHT_USH)
		return 0;

	/* Ignore USB devices on external hub */
	if (udev->parent && udev->parent->parent)
		return 0;

	return 1;
}

static void s3_wake_lock(void)
{
	mutex_lock(&hsic.wlock_mutex);
	if (hsic.s3_wlock_state == UNLOCKED) {
		wake_lock(&hsic.s3_wake_lock);
		hsic.s3_wlock_state = LOCKED;
	}
	mutex_unlock(&hsic.wlock_mutex);
}

static void s3_wake_unlock(void)
{
	mutex_lock(&hsic.wlock_mutex);
	if (hsic.s3_wlock_state == LOCKED) {
		wake_unlock(&hsic.s3_wake_lock);
		hsic.s3_wlock_state = UNLOCKED;
	}
	mutex_unlock(&hsic.wlock_mutex);
}

static void hsicdev_add(struct usb_device *udev)
{

	pr_debug("Notify HSIC add device\n");
	if (is_ush_hsic(udev) == 0) {
		pr_debug("Not a USH HSIC device\n");
		return;
	}

	/* Root hub */
	if (!udev->parent) {
		if (udev->speed == USB_SPEED_HIGH) {
			pr_debug("%s rh device set\n", __func__);
			hsic.rh_dev = udev;
			pr_debug("%s Enable autosuspend\n", __func__);
			pm_runtime_set_autosuspend_delay(&udev->dev,
				hsic.bus_inactivityDuration);
			usb_enable_autosuspend(udev);
			hsic.autosuspend_enable = 1;
		}
	} else {
		if (udev->portnum != hsic.hsic_port_num) {
			pr_debug("%s ignore ush ports %d\n",
				__func__, udev->portnum);
			return;
		}

		/* Modem devices */
		s3_wake_lock();
		hsic.port_disconnect = 0;
		hsic.modem_dev = udev;
		pm_runtime_set_autosuspend_delay
			(&udev->dev, hsic.port_inactivityDuration);
		udev->persist_enabled = 0;

		if (hsic.remoteWakeup_enable) {
			pr_debug("%s Modem dev remote wakeup enabled\n",
					 __func__);
			device_set_wakeup_capable
				(&hsic.modem_dev->dev, 1);
			device_set_wakeup_capable
				(&hsic.rh_dev->dev, 1);
		} else {
			pr_debug("%s Modem dev remote wakeup disabled\n",
					 __func__);
			device_set_wakeup_capable
				(&hsic.modem_dev->dev, 0);
			device_set_wakeup_capable
				(&hsic.rh_dev->dev, 0);
		}

		hsic.autosuspend_enable = HSIC_AUTOSUSPEND;
		if (hsic.autosuspend_enable) {
			pr_debug("%s----> enable autosuspend\n",
				 __func__);
			usb_enable_autosuspend(hsic.modem_dev);
			usb_enable_autosuspend(hsic.rh_dev);
			hsic_wakeup_irq_init();
		}

		if (hsic.autosuspend_enable == 0) {
			pr_debug("%s Modem dev autosuspend disable\n",
					 __func__);
			usb_disable_autosuspend(hsic.modem_dev);
		}
	}
}

static void hsicdev_remove(struct usb_device *udev)
{
	pr_debug("Notify HSIC remove device\n");
	if (is_ush_hsic(udev) == 0) {
		pr_debug("Not a USH HSIC device\n");
		return;
	}

	/* Root hub */
	if (!udev->parent) {
		if (udev->speed == USB_SPEED_HIGH) {
			pr_debug("%s rh_dev deleted\n", __func__);
			hsic.rh_dev = NULL;
		}
	} else {
		if (udev->portnum != hsic.hsic_port_num) {
			pr_debug("%s ignore ush ports %d\n",
				__func__, udev->portnum);
			return;
		}
		/* Modem devices */
		pr_debug("%s----> modem dev deleted\n", __func__);
		mutex_lock(&hsic.hsic_mutex);
		hsic.modem_dev = NULL;
		mutex_unlock(&hsic.hsic_mutex);
		usb_enable_autosuspend(hsic.rh_dev);
		hsic.autosuspend_enable = 1;
		s3_wake_unlock();
	}
}

/* the root hub will call this callback when device added/removed */
static int hsic_notify(struct notifier_block *self,
		unsigned long action, void *dev)
{
	switch (action) {
	case USB_DEVICE_ADD:
		hsicdev_add(dev);
		break;
	case USB_DEVICE_REMOVE:
		hsicdev_remove(dev);
		break;
	}
	return NOTIFY_OK;
}

static void hsic_port_suspend(struct usb_device *udev)
{
	if (is_ush_hsic(udev) == 0) {
		pr_debug("Not a USH HSIC device\n");
		return;
	}

	if (udev->portnum != hsic.hsic_port_num) {
		pr_debug("%s ignore ush ports %d\n",
			__func__, udev->portnum);
		return;
	}

	/* Modem dev */
	if (udev->parent) {
		pr_debug("%s s3 wlock unlocked\n", __func__);
		s3_wake_unlock();
	}
}

static void hsic_port_resume(struct usb_device *udev)
{
	if (is_ush_hsic(udev) == 0) {
		pr_debug("Not a USH HSIC device\n");
		return;
	}

	if (udev->portnum != hsic.hsic_port_num) {
		pr_debug("%s ignore ush ports %d\n",
			__func__, udev->portnum);
		return;
	}

	/* Modem dev */
	if ((udev->parent) && (hsic.s3_rt_state != SUSPENDING)) {
		pr_debug("%s s3 wlock locked\n", __func__);
		s3_wake_lock();
	}
}

static int hsic_pm_notify(struct notifier_block *self,
		unsigned long action, void *dev)
{
	switch (action) {
	case USB_PORT_SUSPEND:
		hsic_port_suspend(dev);
		break;
	case USB_PORT_RESUME:
		hsic_port_resume(dev);
		break;
	}
	return NOTIFY_OK;
}

static int hsic_s3_entry_notify(struct notifier_block *self,
		unsigned long action, void *dummy)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
		hsic.s3_rt_state = SUSPENDING;
		break;
	}
	return NOTIFY_OK;
}

static int clear_port_feature(struct usb_device *hdev, int port1, int feature)
{
	return usb_control_msg(hdev, usb_sndctrlpipe(hdev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RT_PORT, feature,
		port1, NULL, 0, 1000);
}

static int set_port_feature(struct usb_device *hdev, int port1, int feature)
{
	return usb_control_msg(hdev, usb_sndctrlpipe(hdev, 0),
		USB_REQ_SET_FEATURE, USB_RT_PORT, feature,
		port1, NULL, 0, 1000);
}

static void ush_hsic_port_disable(void)
{
	printk(KERN_ERR "%s---->\n", __func__);
	hsic_enable = 0;
	hsic.autosuspend_enable = 0;
	if (hsic.modem_dev) {
		dev_dbg(&pci_dev->dev,
			"Disable auto suspend in port disable\n");
		usb_disable_autosuspend(hsic.modem_dev);
	}
	if (hsic.rh_dev) {
		dev_dbg(&pci_dev->dev,
			"%s----> disable port\n", __func__);
		usb_disable_autosuspend(hsic.rh_dev);
		clear_port_feature(hsic.rh_dev, hsic.hsic_port_num,
				USB_PORT_FEAT_POWER);
		usb_enable_autosuspend(hsic.rh_dev);
	}
	s3_wake_unlock();
}

static void ush_hsic_port_enable(void)
{
	printk(KERN_ERR "%s---->\n", __func__);
	hsic_enable = 1;
	hsic.autosuspend_enable = 0;
	if (hsic.modem_dev) {
		dev_dbg(&pci_dev->dev,
			"Disable auto suspend in port enable\n");
		usb_disable_autosuspend(hsic.modem_dev);
	}

	if (hsic.rh_dev) {
		dev_dbg(&pci_dev->dev,
			"%s----> enable port\n", __func__);
		printk(KERN_ERR "%s----> Enable PP\n", __func__);
		usb_disable_autosuspend(hsic.rh_dev);
		set_port_feature(hsic.rh_dev, hsic.hsic_port_num,
				USB_PORT_FEAT_POWER);
		usb_enable_autosuspend(hsic.rh_dev);
	}
	s3_wake_lock();
}

static void hsic_port_logical_disconnect(struct usb_device *hdev,
		unsigned int port)
{
	dev_dbg(&pci_dev->dev, "logical disconnect on root hub\n");
	hsic.port_disconnect = 1;
	ush_hsic_port_disable();

	usb_set_change_bits(hdev, port);
	usb_kick_khubd(hdev);
}

static void hsic_aux_work(struct work_struct *work)
{
	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	mutex_lock(&hsic.hsic_mutex);
	if ((!hsic.rh_dev) || (hsic_enable == 0)) {
		dev_dbg(&pci_dev->dev,
			"root hub is already removed\n");
		mutex_unlock(&hsic.hsic_mutex);
		return;
	}

	if (hsic.port_disconnect == 0)
		hsic_port_logical_disconnect(hsic.rh_dev,
				hsic.hsic_port_num);
	else
		ush_hsic_port_disable();
	usleep_range(hsic.reenumeration_delay,
			hsic.reenumeration_delay + 1000);
	ush_hsic_port_enable();
	mutex_unlock(&hsic.hsic_mutex);

	hsic.hsic_aux_finish = 1;
	wake_up(&hsic.aux_wq);
	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return;
}

static void wakeup_work(struct work_struct *work)
{
	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	mutex_lock(&hsic.hsic_mutex);
	if ((hsic.modem_dev == NULL) || (hsic_enable == 0)) {
		mutex_unlock(&hsic.hsic_mutex);
		dev_dbg(&pci_dev->dev,
			"%s---->Modem not created\n", __func__);
		return -ENODEV;
	}

	pm_runtime_get_sync(&hsic.modem_dev->dev);
	usleep_range(5000, 6000);
	pm_runtime_put_sync(&hsic.modem_dev->dev);
	mutex_unlock(&hsic.hsic_mutex);

	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return;
}

static ssize_t
show_registers(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	char			*next;
	unsigned		size;
	unsigned		t;
	int                     max_ports;
	int                     i;

	next = buf;
	size = PAGE_SIZE;

	pm_runtime_get_sync(dev);
	usleep_range(1000, 1100);

	max_ports = HCS_MAX_PORTS(xhci->hcs_params1);
	t = scnprintf(next, size,
		"\n"
		"USBCMD = 0x%08x\n"
		"USBSTS = 0x%08x\n",
		xhci_readl(xhci, &xhci->op_regs->command),
		xhci_readl(xhci, &xhci->op_regs->status)
		);
	size -= t;
	next += t;

	for (i = 0; i < max_ports; i++) {
		t = scnprintf(next, size,
			"PORTSC%d = 0x%08x\n"
			"PORTPMSC%d = 0x%08x\n"
			"PORTSC%d = 0x%08x\n",
			i, xhci_readl(xhci, &xhci->op_regs->port_status_base + 4*i),
			i, xhci_readl(xhci, &xhci->op_regs->port_power_base + 4*i),
			i, xhci_readl(xhci, &xhci->op_regs->port_link_base + 4*i)
			);
		size -= t;
		next += t;
		if (size <= 0)
			break;
	}

	pm_runtime_put_sync(dev);
	usleep_range(1000, 1100);

	size -= t;
	next += t;

	return PAGE_SIZE - size;
}

static DEVICE_ATTR(registers, S_IRUGO, show_registers, NULL);

static ssize_t hsic_reenumeration_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.reenumeration_delay);
}

static ssize_t hsic_reenumeration_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	unsigned delay;

	if (size > HSIC_DELAY_SIZE) {
		dev_dbg(dev, "Invalid, size = %d\n", size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &delay) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	mutex_lock(&hsic.hsic_mutex);
	hsic.reenumeration_delay = delay;
	dev_dbg(dev, "reenumeration delay: %d\n",
		hsic.reenumeration_delay);
	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(reenumeration_delay, S_IRUGO | S_IWUSR,
		hsic_reenumeration_delay_show,
		 hsic_reenumeration_delay_store);


/* Interfaces for host resume */
static ssize_t hsic_host_resume_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	dev_dbg(dev, "wakeup hsic\n");
	queue_work(hsic.work_queue, &hsic.wakeup_work);

	return -EINVAL;
}

static DEVICE_ATTR(host_resume, S_IWUSR,
		NULL, hsic_host_resume_store);

static ssize_t hsic_port_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic_enable);
}

static ssize_t hsic_port_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	int org_req;

	if (size > HSIC_ENABLE_SIZE)
		return -EINVAL;

	if (sscanf(buf, "%d", &org_req) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	if (delayed_work_pending(&hsic.hsic_aux)) {
		dev_dbg(dev,
			"%s---->Wait for delayed work finish\n",
			 __func__);
		retval = wait_event_interruptible(hsic.aux_wq,
						hsic.hsic_aux_finish);
		if (retval < 0)
			return retval;

		if (org_req)
			return size;
	}

	mutex_lock(&hsic.hsic_mutex);
	if (!hsic.rh_dev) {
		dev_dbg(&pci_dev->dev,
			"root hub is already removed\n");
		mutex_unlock(&hsic.hsic_mutex);
		return -ENODEV;
	}
	if (hsic.modem_dev) {
		pm_runtime_get_sync(&hsic.modem_dev->dev);
		pm_runtime_put(&hsic.modem_dev->dev);
	}
	if (hsic.rh_dev) {
		pm_runtime_get_sync(&hsic.rh_dev->dev);
		pm_runtime_put(&hsic.rh_dev->dev);
	}

	if (hsic.port_disconnect == 0)
		hsic_port_logical_disconnect(hsic.rh_dev,
				hsic.hsic_port_num);
	else
		ush_hsic_port_disable();

	if (org_req) {
		dev_dbg(dev, "enable hsic\n");
		msleep(20);
		ush_hsic_port_enable();
	} else {
		dev_dbg(dev, "disable hsic\n");
		if (hsic.rh_dev) {
			hsic.autosuspend_enable = 0;
			usb_enable_autosuspend(hsic.rh_dev);
		}
	}
	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(hsic_enable, S_IRUGO | S_IWUSR | S_IROTH | S_IWOTH,
		hsic_port_enable_show, hsic_port_enable_store);

static ssize_t hsic_port_inactivityDuration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.port_inactivityDuration);
}

static ssize_t hsic_port_inactivityDuration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	unsigned duration;

	if (size > HSIC_DURATION_SIZE) {
		dev_dbg(dev, "Invalid, size = %d\n", size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &duration) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	mutex_lock(&hsic.hsic_mutex);
	hsic.port_inactivityDuration = duration;
	dev_dbg(dev, "port Duration: %d\n",
		hsic.port_inactivityDuration);
	if (hsic.modem_dev != NULL) {
		pm_runtime_set_autosuspend_delay
		(&hsic.modem_dev->dev, hsic.port_inactivityDuration);
	}

	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(L2_inactivityDuration, S_IRUGO | S_IWUSR,
		hsic_port_inactivityDuration_show,
		 hsic_port_inactivityDuration_store);

/* Interfaces for L2 suspend */
static ssize_t hsic_autosuspend_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.autosuspend_enable);
}

static ssize_t hsic_autosuspend_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	int org_req;

	if (size > HSIC_ENABLE_SIZE) {
		dev_dbg(dev, "Invalid, size = %d\n", size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &org_req) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	mutex_lock(&hsic.hsic_mutex);
	hsic.autosuspend_enable = org_req;
	if (hsic.modem_dev != NULL) {
		if (hsic.autosuspend_enable == 0) {
			dev_dbg(dev, "Modem dev autosuspend disable\n");
			usb_disable_autosuspend(hsic.modem_dev);
		} else {
			dev_dbg(dev, "Enable auto suspend\n");
			usb_enable_autosuspend(hsic.modem_dev);
			hsic_wakeup_irq_init();
		}
	}

	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(L2_autosuspend_enable, S_IRUGO | S_IWUSR | S_IROTH | S_IWOTH,
		hsic_autosuspend_enable_show,
		 hsic_autosuspend_enable_store);

static ssize_t hsic_bus_inactivityDuration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.bus_inactivityDuration);
}

static ssize_t hsic_bus_inactivityDuration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	unsigned duration;

	if (size > HSIC_DURATION_SIZE) {
		dev_dbg(dev, "Invalid, size = %d\n", size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &duration) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	mutex_lock(&hsic.hsic_mutex);
	hsic.bus_inactivityDuration = duration;
	dev_dbg(dev, "bus Duration: %d\n",
		hsic.bus_inactivityDuration);
	if (hsic.rh_dev != NULL)
		pm_runtime_set_autosuspend_delay
			(&hsic.rh_dev->dev, hsic.bus_inactivityDuration);

	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(bus_inactivityDuration,
		S_IRUGO | S_IWUSR,
		hsic_bus_inactivityDuration_show,
		 hsic_bus_inactivityDuration_store);

static ssize_t hsic_remoteWakeup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.remoteWakeup_enable);
}

static ssize_t hsic_remoteWakeup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int retval;
	int org_req;

	if (size > HSIC_ENABLE_SIZE) {
		dev_dbg(dev, "Invalid, size = %d\n", size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &org_req) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	mutex_lock(&hsic.hsic_mutex);
	hsic.remoteWakeup_enable = org_req;

	if ((hsic.modem_dev != NULL) &&
		(hsic.rh_dev != NULL)) {
		if (hsic.remoteWakeup_enable) {
			dev_dbg(dev, "Modem dev remote wakeup enabled\n");
			device_set_wakeup_capable(&hsic.modem_dev->dev, 1);
			device_set_wakeup_capable(&hsic.rh_dev->dev, 1);
		} else {
			dev_dbg(dev, "Modem dev remote wakeup disabled\n");
			device_set_wakeup_capable(&hsic.modem_dev->dev, 0);
			device_set_wakeup_capable(&hsic.rh_dev->dev, 0);
		}
		pm_runtime_get_sync(&hsic.modem_dev->dev);
		pm_runtime_put_sync(&hsic.modem_dev->dev);
	}

	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(remoteWakeup, S_IRUGO | S_IWUSR,
		hsic_remoteWakeup_show, hsic_remoteWakeup_store);

static int create_class_device_files(struct pci_dev *pdev)
{
	int retval;
	struct ush_hsic_pdata *hsic_pdata;

	hsic_class = class_create(NULL, "hsic");

	if (IS_ERR(hsic_class))
		return -EFAULT;

	hsic_class_dev = device_create(hsic_class, &pci_dev->dev,
			MKDEV(0, 0), NULL, "hsic0");

	if (IS_ERR(hsic_class_dev)) {
		retval = -EFAULT;
		goto hsic_class_fail;
	}

	hsic_pdata = pdev->dev.platform_data;
	hsic.reenumeration_delay = hsic_pdata->reenum_delay;
	dev_info(&pdev->dev, "Re-enumeration delay time %d\n",
				hsic.reenumeration_delay);
	retval = device_create_file(hsic_class_dev,
			&dev_attr_reenumeration_delay);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev,
			"error create reenumeration delay\n");
		goto reenumeration_delay;
	}

	retval = device_create_file(hsic_class_dev, &dev_attr_hsic_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "error create hsic_enable\n");
		goto hsic_enable;
	}

	retval = device_create_file(hsic_class_dev, &dev_attr_host_resume);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "error create host_resume\n");
		goto host_resume;
	}

	hsic.autosuspend_enable = HSIC_AUTOSUSPEND;
	retval = device_create_file(hsic_class_dev,
			 &dev_attr_L2_autosuspend_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create autosuspend_enable\n");
		goto autosuspend;
	}

	hsic.port_inactivityDuration = HSIC_PORT_INACTIVITYDURATION;
	retval = device_create_file(hsic_class_dev,
			 &dev_attr_L2_inactivityDuration);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create port_inactiveDuration\n");
		goto port_duration;
	}

	hsic.bus_inactivityDuration = HSIC_BUS_INACTIVITYDURATION;
	retval = device_create_file(hsic_class_dev,
			 &dev_attr_bus_inactivityDuration);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create bus_inactiveDuration\n");
		goto bus_duration;
	}

	hsic.remoteWakeup_enable = HSIC_REMOTEWAKEUP;
	retval = device_create_file(hsic_class_dev, &dev_attr_remoteWakeup);
	if (retval == 0)
		return retval;

	dev_dbg(&pci_dev->dev, "Error create remoteWakeup\n");

	device_remove_file(hsic_class_dev, &dev_attr_bus_inactivityDuration);
bus_duration:
	device_remove_file(hsic_class_dev, &dev_attr_L2_inactivityDuration);
port_duration:
	device_remove_file(hsic_class_dev, &dev_attr_L2_autosuspend_enable);
autosuspend:
	device_remove_file(hsic_class_dev, &dev_attr_host_resume);
host_resume:
	device_remove_file(hsic_class_dev, &dev_attr_hsic_enable);
hsic_enable:
	device_remove_file(hsic_class_dev, &dev_attr_reenumeration_delay);
reenumeration_delay:
	device_remove_file(hsic_class_dev, &dev_attr_registers);
dump_registers:
hsic_class_fail:
	return retval;
}

static void remove_class_device_files(void)
{
	device_destroy(hsic_class, hsic_class_dev->devt);
	class_destroy(hsic_class);
}

static int create_device_files(struct pci_dev *pdev)
{
	int retval;
	struct ush_hsic_pdata *hsic_pdata;

	retval = device_create_file(&pci_dev->dev,
			&dev_attr_registers);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev,
			"error create registers\n");
		goto dump_registers;
	}

	hsic_pdata = pdev->dev.platform_data;
	hsic.reenumeration_delay = hsic_pdata->reenum_delay;
	dev_info(&pdev->dev, "Re-enumeration delay time %d\n",
				hsic.reenumeration_delay);
	retval = device_create_file(&pci_dev->dev,
			&dev_attr_reenumeration_delay);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev,
			"error create reenumeration delay\n");
		goto reenumeration_delay;
	}

	retval = device_create_file(&pci_dev->dev, &dev_attr_hsic_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "error create hsic_enable\n");
		goto hsic_enable;
	}

	retval = device_create_file(&pci_dev->dev, &dev_attr_host_resume);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "error create host_resume\n");
		goto host_resume;
	}

	hsic.autosuspend_enable = HSIC_AUTOSUSPEND;
	retval = device_create_file(&pci_dev->dev,
			 &dev_attr_L2_autosuspend_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create autosuspend_enable\n");
		goto autosuspend;
	}

	hsic.port_inactivityDuration = HSIC_PORT_INACTIVITYDURATION;
	retval = device_create_file(&pci_dev->dev,
			 &dev_attr_L2_inactivityDuration);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create port_inactiveDuration\n");
		goto port_duration;
	}

	hsic.bus_inactivityDuration = HSIC_BUS_INACTIVITYDURATION;
	retval = device_create_file(&pci_dev->dev,
			 &dev_attr_bus_inactivityDuration);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create bus_inactiveDuration\n");
		goto bus_duration;
	}

	hsic.remoteWakeup_enable = HSIC_REMOTEWAKEUP;
	retval = device_create_file(&pci_dev->dev, &dev_attr_remoteWakeup);
	if (retval == 0)
		return retval;

	dev_dbg(&pci_dev->dev, "Error create remoteWakeup\n");

	device_remove_file(&pci_dev->dev, &dev_attr_bus_inactivityDuration);
bus_duration:
	device_remove_file(&pci_dev->dev, &dev_attr_L2_inactivityDuration);
port_duration:
	device_remove_file(&pci_dev->dev, &dev_attr_L2_autosuspend_enable);
autosuspend:
	device_remove_file(&pci_dev->dev, &dev_attr_host_resume);
host_resume:
	device_remove_file(&pci_dev->dev, &dev_attr_hsic_enable);
hsic_enable:
	device_remove_file(&pci_dev->dev, &dev_attr_reenumeration_delay);
reenumeration_delay:
	device_remove_file(&pci_dev->dev, &dev_attr_registers);
dump_registers:
	return retval;
}

static void remove_device_files()
{
	device_remove_file(&pci_dev->dev, &dev_attr_L2_autosuspend_enable);
	device_remove_file(&pci_dev->dev, &dev_attr_L2_inactivityDuration);
	device_remove_file(&pci_dev->dev, &dev_attr_bus_inactivityDuration);
	device_remove_file(&pci_dev->dev, &dev_attr_remoteWakeup);
	device_remove_file(&pci_dev->dev, &dev_attr_host_resume);
	device_remove_file(&pci_dev->dev, &dev_attr_hsic_enable);
	device_remove_file(&pci_dev->dev, &dev_attr_reenumeration_delay);
	device_remove_file(&pci_dev->dev, &dev_attr_registers);
}

static int hsic_get_gpio_num(struct pci_dev *pdev)
{
#ifdef CONFIG_ACPI
	struct ush_hsic_pdata *pdata;
	acpi_handle handle;
	acpi_status status;

	pdata = pdev->dev.platform_data;

	if (gpio_is_valid(pdata->aux_gpio)
		&& gpio_is_valid(pdata->wakeup_gpio))
		return 0;

	status = acpi_get_handle(NULL,
			"\\_SB.PCI0.XHC1.RHUB.HSC1", &handle);
	if (ACPI_FAILURE(status)) {
		dev_err(&pdev->dev, "HSIC: cannot get HSC1 acpi handle\n");
		/* Try to get GPIO pin number from fixed value */
		pdata->aux_gpio = acpi_get_gpio("\\_SB.GPO2", 6);
		pdata->wakeup_gpio = acpi_get_gpio("\\_SB.GPO2", 22);
		if (gpio_is_valid(pdata->aux_gpio) &&
			gpio_is_valid(pdata->wakeup_gpio)) {
			dev_info(&pdev->dev, "HSIC GPO2 aux %d wakeup %d\n",
					pdata->aux_gpio, pdata->wakeup_gpio);
			return 0;
		} else {
			dev_err(&pdev->dev, "HSIC: no GPO2 entry for GPIO\n");
			return -ENODEV;
		}
	}

	/* Get the GPIO value from ACPI table */
	pdata->aux_gpio = acpi_get_gpio_by_index(&pdev->dev, 0, NULL);
	if (pdata->aux_gpio < 0) {
		dev_err(&pdev->dev, "HSIC: fail to get AUX1 from acpi %d\n",
				pdata->aux_gpio);
		pdata->aux_gpio = acpi_get_gpio("\\_SB.GPO2", 6);
		if (!gpio_is_valid(pdata->aux_gpio)) {
			dev_err(&pdev->dev, "HSIC: no GPO2 entry for GPIO\n");
			return -ENODEV;
		}
	}

	pdata->wakeup_gpio = acpi_get_gpio_by_index(&pdev->dev, 1, NULL);
	if (pdata->wakeup_gpio < 0) {
		dev_err(&pdev->dev, "HSIC: fail to get WAKEUP from acpi %d\n",
				pdata->wakeup_gpio);
		pdata->wakeup_gpio = acpi_get_gpio("\\_SB.GPO2", 22);
		if (!gpio_is_valid(pdata->wakeup_gpio)) {
			dev_err(&pdev->dev, "HSIC: no GPO2 entry for GPIO\n");
			return -ENODEV;
		}
	}

	dev_info(&pdev->dev, "USH HSIC GPIO AUX %d WAKEUP %d\n",
			pdata->aux_gpio, pdata->wakeup_gpio);
#endif
	return 0;
}

static void xhci_no_power_gate_on_d3(struct pci_dev *dev, struct usb_hcd *hcd)
{
	u32 reg;

	dev_info(&dev->dev, "Disable Power gating xHCI\n");

	/* Clear D3_hot_en */
	pci_read_config_dword(dev, 0x9c, &reg);
	dev_info(&dev->dev, "pci config + 0x9c = 0x%08x\n", reg);
	reg &= ~BIT(18);
	pci_write_config_dword(dev, 0x9c, reg);
	pci_read_config_dword(dev, 0x9c, &reg);
	dev_info(&dev->dev, "After write, pci config + 0x9c = 0x%08x\n", reg);

	/* Enable Legacy PME_en */
	reg = readl(hcd->regs + 0x80a4);
	dev_info(&dev->dev, "MMIO + 0x80A4 = 0x%08x\n", reg);
	reg |= BIT(30);
	writel(reg, hcd->regs + 0x80a4);
	reg = readl(hcd->regs + 0x80a4);
	dev_info(&dev->dev, "After write MMIO + 0x80A4 = 0x%08x\n", reg);
}

/*
 * We need to register our own PCI probe function (instead of the USB core's
 * function) in order to create a second roothub under xHCI.
 */
static int xhci_ush_pci_probe(struct pci_dev *dev,
		const struct pci_device_id *id)
{
	int retval;
	struct xhci_hcd *xhci;
	struct hc_driver *driver;
	struct usb_hcd *hcd;
	struct ush_hsic_pdata *hsic_pdata;
	struct usb_phy *usb_phy;

	hsic_pdata = dev->dev.platform_data;
	if (!hsic_pdata->has_modem) {
		dev_err(&dev->dev, "Don't match this driver\n");
		return -ENODEV;
	}

	driver = (struct hc_driver *)id->driver_data;
	pci_dev = dev;

	wake_lock_init(&hsic.s3_wake_lock,
			WAKE_LOCK_SUSPEND, "hsic_s3_wlock");
	hsic.hsic_pm_nb.notifier_call = hsic_pm_notify;
	usb_register_notify(&hsic.hsic_pm_nb);
	hsic.hsic_s3_entry_nb.notifier_call = hsic_s3_entry_notify;
	register_pm_notifier(&hsic.hsic_s3_entry_nb);

	hsic.hsic_port_num = hsic_pdata->hsic_port_num;
	retval = hsic_get_gpio_num(pci_dev);
	if (retval < 0) {
		dev_err(&dev->dev, "failed to get gpio value\n");
		return -ENODEV;
	}

	/* AUX GPIO init */
	retval = hsic_aux_gpio_init(hsic_pdata->aux_gpio);
	if (retval < 0) {
		dev_err(&dev->dev, "AUX GPIO init fail\n");
		retval = -ENODEV;
	}

	/* AUX GPIO init */
	retval = hsic_wakeup_gpio_init(hsic_pdata->wakeup_gpio);
	if (retval < 0) {
		dev_err(&dev->dev, "Wakeup GPIO init fail\n");
		retval = -ENODEV;
	}

	wake_lock_init(&hsic.resume_wake_lock,
		WAKE_LOCK_SUSPEND, "hsic_aux2_wlock");

	hsic.hsicdev_nb.notifier_call = hsic_notify;
	usb_register_notify(&hsic.hsicdev_nb);

	/* Register the USB 2.0 roothub.
	 * FIXME: USB core must know to register the USB 2.0 roothub first.
	 * This is sort of silly, because we could just set the HCD driver flags
	 * to say USB 2.0, but I'm not sure what the implications would be in
	 * the other parts of the HCD code.
	 */
	retval = usb_hcd_pci_probe(dev, id);
	if (retval)
		return retval;

	/* USB 2.0 roothub is stored in the PCI device now. */
	hcd = dev_get_drvdata(&dev->dev);
	xhci = hcd_to_xhci(hcd);
	xhci->shared_hcd = usb_create_shared_hcd(driver, &dev->dev,
				pci_name(dev), hcd);
	if (!xhci->shared_hcd) {
		retval = -ENOMEM;
		goto dealloc_usb2_hcd;
	}

	/* Set the xHCI pointer before xhci_pci_setup() (aka hcd_driver.reset)
	 * is called by usb_add_hcd().
	 */
	*((struct xhci_hcd **) xhci->shared_hcd->hcd_priv) = xhci;

	if (hsic.hsic_enable_created == 0) {
		retval = create_device_files(dev);
		if (retval < 0) {
			dev_dbg(&dev->dev, "error create device files\n");
			goto dealloc_usb2_hcd;
		}

		retval = create_class_device_files(dev);
		if (retval < 0) {
			dev_dbg(&dev->dev, "error create class device files\n");
			goto dealloc_usb2_hcd;
		}
		hsic.hsic_enable_created = 1;
	}

	if (hsic.hsic_mutex_init == 0) {
		mutex_init(&hsic.hsic_mutex);
		mutex_init(&hsic.wlock_mutex);
		hsic.hsic_mutex_init = 1;
	}

	if (hsic.aux_wq_init == 0) {
		init_waitqueue_head(&hsic.aux_wq);
		hsic.aux_wq_init = 1;
	}

	hsic.work_queue = create_singlethread_workqueue("hsic");
	INIT_WORK(&hsic.wakeup_work, wakeup_work);
	INIT_DELAYED_WORK(&(hsic.hsic_aux), hsic_aux_work);

	retval = usb_add_hcd(xhci->shared_hcd, dev->irq,
			IRQF_SHARED);
	if (retval)
		goto put_usb3_hcd;
	/* Roothub already marked as USB 3.0 speed */

	/* CHT: pass host parameter to OTG */
	usb_phy = usb_get_phy(USB_PHY_TYPE_USB2);
	if (usb_phy)
		otg_set_host(usb_phy->otg, &hcd->self);
	usb_put_phy(usb_phy);

	/* Enable Controller wakeup capability */
	device_set_wakeup_enable(&dev->dev, true);

	pm_runtime_set_active(&dev->dev);

	/* Check here to avoid to call pm_runtime_put_noidle() twice */
	if (!pci_dev_run_wake(dev))
		pm_runtime_put_noidle(&dev->dev);

	/* WORKAROUND: CHT A1 can't resume from D3 correctly if controller
	 * is power gated in D3
	 */
	if (hsic_pdata->no_power_gate)
		xhci_no_power_gate_on_d3(dev, hcd);

	/* Disable the HSIC port */
	dev_info(&dev->dev, "disable hsic on driver init\n");
	clear_port_feature(hsic.rh_dev, hsic.hsic_port_num,
		USB_PORT_FEAT_POWER);

	pm_runtime_allow(&dev->dev);
	hsic.port_disconnect = 0;
	hsic_enable = 1;
	hsic.s3_rt_state = RESUMED;
	return 0;

put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);
dealloc_usb2_hcd:
	usb_hcd_pci_remove(dev);
	wake_lock_destroy(&(hsic.resume_wake_lock));
	wake_lock_destroy(&hsic.s3_wake_lock);
	return retval;
}

static void xhci_ush_pci_remove(struct pci_dev *dev)
{
	struct xhci_hcd *xhci;

	xhci = hcd_to_xhci(pci_get_drvdata(dev));
	if (xhci->shared_hcd) {
		usb_remove_hcd(xhci->shared_hcd);
		usb_put_hcd(xhci->shared_hcd);
	}

	if (!pci_dev_run_wake(dev))
		pm_runtime_get_noresume(&dev->dev);

	pm_runtime_forbid(&dev->dev);

	usb_hcd_pci_remove(dev);

	/* Free the aux irq */
	hsic_aux_irq_free();
	hsic_wakeup_irq_free();
	gpio_free(hsic.aux_gpio);
	gpio_free(hsic.wakeup_gpio);

	hsic.port_disconnect = 1;
	hsic_enable = 0;
	wake_lock_destroy(&(hsic.resume_wake_lock));
	wake_lock_destroy(&hsic.s3_wake_lock);
	usb_unregister_notify(&hsic.hsic_pm_nb);
	unregister_pm_notifier(&hsic.hsic_s3_entry_nb);

	kfree(xhci);
}

/**
 * xhci_ush_pci_shutdown - shutdown host controller
 * @dev: USB Host Controller being shutdown
 */
static void xhci_ush_pci_shutdown(struct pci_dev *dev)
{
	struct usb_hcd		*hcd;

	hcd = pci_get_drvdata(dev);
	if (!hcd)
		return;

	if (test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags) &&
			hcd->driver->shutdown) {
		disable_irq(gpio_to_irq(hsic.aux_gpio));
		disable_irq(gpio_to_irq(hsic.wakeup_gpio));
		hcd->driver->shutdown(hcd);
		if (hsic.rh_dev) {
			dev_dbg(&pci_dev->dev,
				"%s: disable port\n", __func__);
			clear_port_feature(hsic.rh_dev, hsic.hsic_port_num,
					USB_PORT_FEAT_POWER);
		}
		pci_disable_device(dev);
	}
}

#ifdef CONFIG_PM_SLEEP
static int xhci_ush_hcd_pci_suspend_noirq(struct device *dev)
{
	int	retval;

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.suspend_noirq(dev);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}

static int xhci_ush_hcd_pci_suspend(struct device *dev)
{
	int     retval;

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.suspend(dev);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}


static int xhci_ush_hcd_pci_resume_noirq(struct device *dev)
{
	int                     retval;

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.resume_noirq(dev);
	hsic.s3_rt_state = RESUMED;
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}


static int xhci_ush_hcd_pci_resume(struct device *dev)
{
	int     retval;

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.resume(dev);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}

#else
#define xhci_ush_hcd_pci_suspend_noirq   NULL
#define xhci_ush_hcd_pci_suspend         NULL
#define xhci_ush_hcd_pci_resume_noirq    NULL
#define xhci_ush_hcd_pci_resume          NULL
#endif


#ifdef CONFIG_PM_RUNTIME
static int xhci_ush_hcd_pci_runtime_suspend(struct device *dev)
{
	int     retval;

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.runtime_suspend(dev);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}

static int xhci_ush_hcd_pci_runtime_resume(struct device *dev)
{
	struct pci_dev          *pci_dev = to_pci_dev(dev);
	struct usb_hcd          *hcd = pci_get_drvdata(pci_dev);
	int                     retval;

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.runtime_resume(dev);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}
#else
#define xhci_ush_hcd_pci_runtime_suspend NULL
#define xhci_ush_hcd_pci_runtime_resume  NULL
#endif


/* called after powerup, by probe or system-pm "wakeup" */
static int xhci_pci_reinit(struct xhci_hcd *xhci, struct pci_dev *pdev)
{
	/*
	 * TODO: Implement finding debug ports later.
	 * TODO: see if there are any quirks that need to be added to handle
	 * new extended capabilities.
	 */

	/* PCI Memory-Write-Invalidate cycle support is optional (uncommon) */
	if (!pci_set_mwi(pdev))
		xhci_dbg(xhci, "MWI active\n");

	xhci_dbg(xhci, "Finished xhci_pci_reinit\n");
	return 0;
}

static void xhci_ush_pci_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	xhci->quirks |= XHCI_SPURIOUS_SUCCESS;
	/*
	 * We found two USB Disk cannot pass Enumeration with LPM
	 * token sent on BYT, so disable LPM here.
	 */
	xhci->quirks |= XHCI_LPM_DISABLE_QUIRK;
	return;
}

/* called during probe() after chip reset completes */
static int xhci_ush_pci_setup(struct usb_hcd *hcd)
{
	struct xhci_hcd         *xhci;
	struct pci_dev          *pdev = to_pci_dev(hcd->self.controller);
	int                     retval;

	retval = xhci_gen_setup(hcd, xhci_ush_pci_quirks);
	if (retval)
		return retval;

	xhci = hcd_to_xhci(hcd);
	if (!usb_hcd_is_primary_hcd(hcd))
		return 0;

	pci_read_config_byte(pdev, XHCI_SBRN_OFFSET, &xhci->sbrn);
	xhci_dbg(xhci, "Got SBRN %u\n", (unsigned int) xhci->sbrn);

	/* Find any debug ports */
	retval = xhci_pci_reinit(xhci, pdev);
	if (!retval)
		return retval;

	kfree(xhci);
	return retval;
}


#ifdef CONFIG_PM
static int xhci_ush_pci_suspend(struct usb_hcd *hcd, bool do_wakeup)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int     retval = 0;

	if (hcd->state != HC_STATE_SUSPENDED ||
		xhci->shared_hcd->state != HC_STATE_SUSPENDED)
		return -EINVAL;

	retval = xhci_suspend(xhci);

	return retval;
}

static int xhci_ush_pci_resume(struct usb_hcd *hcd, bool hibernated)
{
	struct xhci_hcd         *xhci = hcd_to_xhci(hcd);
	struct pci_dev          *pdev = to_pci_dev(hcd->self.controller);
	int                     retval = 0;

	/* The BIOS on systems with the Intel Panther Point chipset may or may
	 * not support xHCI natively.  That means that during system resume, it
	 * may switch the ports back to EHCI so that users can use their
	 * keyboard to select a kernel from GRUB after resume from hibernate.
	 *
	 * The BIOS is supposed to remember whether the OS had xHCI ports
	 * enabled before resume, and switch the ports back to xHCI when the
	 * BIOS/OS semaphore is written, but we all know we can't trust BIOS
	 * writers.
	 *
	 * Unconditionally switch the ports back to xHCI after a system resume.
	 * We can't tell whether the EHCI or xHCI controller will be resumed
	 * first, so we have to do the port switchover in both drivers.  Writing
	 * a '1' to the port switchover registers should have no effect if the
	 * port was already switched over.
	 */
	if (usb_is_intel_switchable_xhci(pdev))
		usb_enable_xhci_ports(pdev);

	retval = xhci_resume(xhci, hibernated);
	return retval;
}
#endif

static const struct hc_driver xhci_ush_pci_hc_driver = {
	.description =          "Baytrail-USH",
	.product_desc =         "xHCI Host Controller",
	.hcd_priv_size =        sizeof(struct xhci_hcd *),

	/*
	* generic hardware linkag
	 */
	.irq =                  xhci_irq,
	.flags =                HCD_MEMORY | HCD_USB3 | HCD_SHARED,

	/*
	 * basic lifecycle operations
	 */
	.reset =                xhci_ush_pci_setup,
	.start =                xhci_run,
#ifdef CONFIG_PM
	.pci_suspend =          xhci_ush_pci_suspend,
	.pci_resume =           xhci_ush_pci_resume,
#endif
	.stop =                 xhci_stop,
	.shutdown =             xhci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =          xhci_urb_enqueue,
	.urb_dequeue =          xhci_urb_dequeue,
	.alloc_dev =            xhci_alloc_dev,
	.free_dev =             xhci_free_dev,
	.alloc_streams =        xhci_alloc_streams,
	.free_streams =         xhci_free_streams,
	.add_endpoint =         xhci_add_endpoint,
	.drop_endpoint =        xhci_drop_endpoint,
	.endpoint_reset =       xhci_endpoint_reset,
	.check_bandwidth =      xhci_check_bandwidth,
	.reset_bandwidth =      xhci_reset_bandwidth,
	.address_device =       xhci_address_device,
	.update_hub_device =    xhci_update_hub_device,
	.reset_device =         xhci_discover_or_reset_device,

	/*
	 * scheduling support
	 */
	.get_frame_number =     xhci_get_frame,

	/* Root hub support */
	.hub_control =          xhci_hub_control,
	.hub_status_data =      xhci_hub_status_data,
	.bus_suspend =          xhci_bus_suspend,
	.bus_resume =           xhci_bus_resume,
	/*
	 * call back when device connected and addressed
	 */
	.update_device =        xhci_update_device,
	.set_usb2_hw_lpm =      xhci_set_usb2_hardware_lpm,
};

static DEFINE_PCI_DEVICE_TABLE(xhci_ush_pci_ids) = {
	{
		.vendor =       PCI_VENDOR_ID_INTEL,
		.device =       0x0F35,
		.subvendor =    PCI_ANY_ID,
		.subdevice =    PCI_ANY_ID,
		.driver_data =  (unsigned long) &xhci_ush_pci_hc_driver,
	},
	{
		.vendor =       PCI_VENDOR_ID_INTEL,
		.device =       PCI_DEVICE_ID_INTEL_CHT_USH,
		.subvendor =    PCI_ANY_ID,
		.subdevice =    PCI_ANY_ID,
		.driver_data =  (unsigned long) &xhci_ush_pci_hc_driver,
	},

	{ /* end: all zeroes */ }
};

static const struct dev_pm_ops xhci_ush_pm_ops = {
	.suspend        = xhci_ush_hcd_pci_suspend,
	.suspend_noirq  = xhci_ush_hcd_pci_suspend_noirq,
	.resume         = xhci_ush_hcd_pci_resume,
	.resume_noirq   = xhci_ush_hcd_pci_resume_noirq,
	.runtime_suspend = xhci_ush_hcd_pci_runtime_suspend,
	.runtime_resume = xhci_ush_hcd_pci_runtime_resume,
};

static struct pci_driver xhci_ush_driver = {
	.name = "BYT-USH",
	.id_table =     xhci_ush_pci_ids,

	.probe =        xhci_ush_pci_probe,
	.remove =       xhci_ush_pci_remove,

#ifdef CONFIG_PM_SLEEP
	.driver =       {
		.pm = &xhci_ush_pm_ops
	},
#endif
	.shutdown =     xhci_ush_pci_shutdown,
};

int xhci_register_ush_pci(void)
{
	return pci_register_driver(&xhci_ush_driver);
}

void xhci_unregister_ush_pci(void)
{
	return pci_unregister_driver(&xhci_ush_driver);
}
