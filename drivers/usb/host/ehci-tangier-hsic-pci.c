/*
 * Intel MID Platform Tangier EHCI/HSIC Controller PCI Bus Glue.
 *
 * Copyright (c) 2008 - 2012, Intel Corporation.
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

#include <linux/usb/hcd.h>
#include <linux/wakelock.h>
#include <linux/lnw_gpio.h>
#include <linux/gpio.h>
#include <linux/usb/ehci-tangier-hsic-pci.h>
#include <asm/intel-mid.h>
#include <linux/jiffies.h>
#include <linux/usb.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#define INIT_TASK_PID	1

static struct pci_dev	*pci_dev;
static struct class *hsic_class;
static struct device *hsic_class_dev;


static int ehci_hsic_start_host(struct pci_dev  *pdev);
static int ehci_hsic_stop_host(struct pci_dev *pdev);
static int create_device_files(void);
static int create_class_device_files(void);
static void remove_class_device_files(void);
static void remove_device_files(void);

static int enabling_disabling;
static int hsic_enable;
static struct hsic_tangier_priv hsic;

/* pm_statistics structure, default disabled */
static unsigned			stats_enable;
#define IPC_STATS_NUM		8
static struct ipc_stats		stats[IPC_STATS_NUM];

static const char enabled[] = "enabled";
static const char disabled[] = "disabled";
static const char reset[] = "reset";


static struct dentry *hsic_debugfs_root;
static struct dentry *ipc_debug_control;
static struct dentry *ipc_stats;

#define IPCCOUNT(x, y) \
	do { if (x) (y)++; } while (0)

void count_ipc_stats(int retval, enum ipc_stats_type type)
{
	switch (retval) {
	case 0:
		IPCCOUNT(stats_enable, stats[type].success_cnt);
		break;
	case -EBUSY:
		IPCCOUNT(stats_enable, stats[type].ipc_failure[0].fail_cnt);
		break;
	case -EINVAL:
		IPCCOUNT(stats_enable, stats[type].ipc_failure[1].fail_cnt);
		break;
	case -ETIMEDOUT:
		IPCCOUNT(stats_enable, stats[type].ipc_failure[2].fail_cnt);
		break;
	case -ESHUTDOWN:
		IPCCOUNT(stats_enable, stats[type].ipc_failure[3].fail_cnt);
		break;
	default:
		break;
	}
}

static void ipc_counter_init(void)
{
	int		i, j;

	stats[0].name = "remote_wakeup(inband)";
	stats[1].name = "remote_wakeup(outband)";
	stats[2].name = "bus_suspend";
	stats[3].name = "bus_resume";
	stats[4].name = "d0i3_entry";
	stats[5].name = "d0i3_exit";
	stats[6].name = "d3_entry";
	stats[7].name = "d3_exit";

	for (i = 0; i < IPC_STATS_NUM; i++) {
		stats[i].success_cnt = 0;
		stats[i].ipc_failure[0].name = "EBUSY";
		stats[i].ipc_failure[1].name = "EINVAL";
		stats[i].ipc_failure[2].name = "ETIMEDOUT";
		stats[i].ipc_failure[3].name = "ESHUTDOWN";
	}

	for (i = 0; i < IPC_STATS_NUM; i++)
		for (j = 0; j < PM_FAILURE_COUNT; j++)
			stats[i].ipc_failure[j].fail_cnt = 0;
}

static int ipc_control_show(struct seq_file *s, void *unused)
{
	if (stats_enable)
		seq_printf(s, "%s\n", enabled);
	else
		seq_printf(s, "%s\n", disabled);
	return 0;
}

static int ipc_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, ipc_control_show, inode->i_private);
}

static ssize_t ipc_control_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	char			buf[32];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
			return -EFAULT;

	if (!strncmp(buf, "on", 2)) {
		stats_enable = STATS_ENABLE;
	} else if (!strncmp(buf, "off", 3)) {
		stats_enable = STATS_DISABLE;
	} else if (!strncmp(buf, "reset", 5)) {
		/* reset all counter and then enable stats */
		ipc_counter_init();
		stats_enable = STATS_ENABLE;
	}
	return count;
}
static const struct file_operations ipc_control_fops = {
	.open                   = ipc_control_open,
	.write                  = ipc_control_write,
	.read                   = seq_read,
	.llseek                 = seq_lseek,
	.release                = single_release,
};

static int ipc_stats_show(struct seq_file *s, void *unused)
{
	int		i, j;

	seq_puts(s, "USB IPC stats show:\n");

	for (i = 0; i < IPC_STATS_NUM; i++) {
		seq_printf(s, "status = %s success count = %lu\n",
				stats[i].name, stats[i].success_cnt);
		for (j = 0; j < PM_FAILURE_COUNT; j++)
			seq_printf(s, "failure = %s count = %lu\n",
				stats[i].ipc_failure[j].name,
				stats[i].ipc_failure[j].fail_cnt);

		seq_puts(s, "\n");
	}
	return 0;
}

static int ipc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ipc_stats_show, inode->i_private);
}

static const struct file_operations ipc_stats_fops = {
	.open                   = ipc_stats_open,
	.read                   = seq_read,
	.llseek                 = seq_lseek,
	.release                = single_release,
};

/* Workaround for OSPM, set PMCMD to ask SCU
 * power gate EHCI controller and DPHY
 */
static void hsic_enter_exit_d3(int enter_exit)
{
	if (enter_exit) {
		printk(KERN_CRIT "HSIC Enter D0I3!\n");
		pci_set_power_state(pci_dev, PCI_D3cold);
		count_ipc_stats(0, D0I3_ENTRY);
	} else {
		printk(KERN_CRIT "HSIC Exit D0I3!\n");
		pci_set_power_state(pci_dev, PCI_D0);
		count_ipc_stats(0, D0I3_EXIT);
	}
}

#if 0
static void ehci_hsic_port_power(struct ehci_hcd *ehci, int is_on)
{
	unsigned port;

	if (!HCS_PPC(ehci->hcs_params))
		return;

	dev_dbg(&pci_dev->dev, "...power%s ports...\n", is_on ? "up" : "down");
	for (port = HCS_N_PORTS(ehci->hcs_params); port > 0; )
		(void) ehci_hub_control(ehci_to_hcd(ehci),
				is_on ? SetPortFeature : ClearPortFeature,
				USB_PORT_FEAT_POWER,
				port--, NULL, 0);
	/* Flush those writes */
	ehci_readl(ehci, &ehci->regs->command);
}
#endif

static void ehci_hsic_phy_power(struct ehci_hcd *ehci, int is_low_power)
{
	unsigned port;

	port = HCS_N_PORTS(ehci->hcs_params);
	while (port--) {
		u32 __iomem	*hostpc_reg;
		u32		t3;

		hostpc_reg = (u32 __iomem *)((u8 *) ehci->regs
				+ 0x84 + 4 * port);
		t3 = ehci_readl(ehci, hostpc_reg);
		ehci_dbg(ehci, "Port %d phy low-power mode org %08x\n",
				port, t3);

		if (is_low_power)
			ehci_writel(ehci, t3 | HOSTPC_PHCD, hostpc_reg);
		else
			ehci_writel(ehci, t3 & ~HOSTPC_PHCD, hostpc_reg);

		t3 = ehci_readl(ehci, hostpc_reg);
		ehci_dbg(ehci, "Port %d phy low-power mode chg %08x\n",
				port, t3);
	}
}

/* Init HSIC AUX GPIO */
static int hsic_aux_gpio_init(void)
{
	int		retval = 0;

	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	hsic.aux_gpio = get_gpio_by_name(HSIC_AUX_GPIO_NAME);
	if (gpio_is_valid(hsic.aux_gpio)) {
		retval = gpio_request(hsic.aux_gpio, "hsic_aux");
		if (retval < 0) {
			dev_err(&pci_dev->dev,
				"Request GPIO %d with error %d\n",
				hsic.aux_gpio, retval);
			retval = -ENODEV;
			goto err;
		}
	} else {
		retval = -ENODEV;
		goto err;
	}

	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return retval;

err:
	gpio_free(hsic.aux_gpio);
	return retval;
}

/* Init HSIC AUX2 GPIO as side band remote wakeup source */
static int hsic_wakeup_gpio_init(void)
{
	int		retval = 0;

	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	hsic.wakeup_gpio = get_gpio_by_name(HSIC_WAKEUP_GPIO_NAME);
	if (gpio_is_valid(hsic.wakeup_gpio)) {
		retval = gpio_request(hsic.wakeup_gpio, "hsic_wakeup");
		if (retval < 0) {
			dev_err(&pci_dev->dev,
				"Request GPIO %d with error %d\n",
				hsic.wakeup_gpio, retval);
			retval = -ENODEV;
			goto err;
		}
	} else {
		retval = -ENODEV;
		goto err;
	}

	gpio_direction_input(hsic.wakeup_gpio);
	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return retval;

err:
	gpio_free(hsic.wakeup_gpio);
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

	cancel_delayed_work(&hsic.wakeup_work);
	if (delayed_work_pending(&hsic.hsic_aux)) {
		dev_dbg(dev,
			"%s---->Delayed work pending\n", __func__);
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

	queue_delayed_work(hsic.work_queue, &hsic.wakeup_work, 0);
	dev_dbg(dev,
		"%s<----\n", __func__);
	count_ipc_stats(0, REMOTE_WAKEUP_OOB);

	return IRQ_HANDLED;
}

static int hsic_aux_irq_init(void)
{
	int retval;

	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	if (hsic.hsic_aux_irq_enable) {
		dev_dbg(&pci_dev->dev,
			"%s<----AUX IRQ is enabled\n", __func__);
		return 0;
	}
	hsic.hsic_aux_irq_enable = 1;
	gpio_direction_input(hsic.aux_gpio);
	retval = request_irq(gpio_to_irq(hsic.aux_gpio),
			hsic_aux_gpio_irq,
			IRQF_SHARED | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
			"hsic_disconnect_request", &pci_dev->dev);
	if (retval) {
		dev_err(&pci_dev->dev,
			"unable to request irq %i, err: %d\n",
			gpio_to_irq(hsic.aux_gpio), retval);
		goto err;
	}

	lnw_gpio_set_alt(hsic.aux_gpio, 0);
	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);

	return retval;

err:
	hsic.hsic_aux_irq_enable = 0;
	free_irq(gpio_to_irq(hsic.aux_gpio), &pci_dev->dev);
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

/* the root hub will call this callback when device added/removed */
static void hsic_notify(struct usb_device *udev, unsigned action)
{
	int retval;
	struct pci_dev *pdev = to_pci_dev(udev->bus->controller);

	/* Ignore and only valid for HSIC. Filter out
	 * the USB devices added by other USB2 host driver */
	if (pdev->device != 0x119d)
		return;

	/* Ignore USB devices on external hub */
	if (udev->parent && udev->parent->parent)
		return;

	/* Only valid for hsic port1 */
	if (udev->portnum == 2) {
		pr_debug("%s ignore hsic port2\n", __func__);
		return;
	}

	switch (action) {
	case USB_DEVICE_ADD:
		pr_debug("Notify HSIC add device\n");
		/* Root hub */
		if (!udev->parent) {
			hsic.rh_dev = udev;
			pr_debug("%s Enable autosuspend\n", __func__);
			pm_runtime_set_autosuspend_delay(&udev->dev,
					hsic.bus_inactivityDuration);
			hsic.autosuspend_enable = 1;
			usb_enable_autosuspend(udev);
		} else {
			/* Modem devices */
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
			pr_debug("%s Disable autosuspend\n", __func__);
			usb_disable_autosuspend(hsic.modem_dev);
			hsic.autosuspend_enable = 0;

			pr_debug("%s----> Enable AUX irq\n", __func__);
			retval = hsic_aux_irq_init();
			if (retval)
				dev_err(&pci_dev->dev,
					"unable to request IRQ\n");
		}
		break;
	case USB_DEVICE_REMOVE:
		pr_debug("Notify HSIC delete device\n");
		/* Root hub */
		if (!udev->parent) {
			pr_debug("%s rh_dev deleted\n", __func__);
			hsic.rh_dev = NULL;
			hsic.autosuspend_enable = 1;
		} else {
			/* Modem devices */
			pr_debug("%s----> modem dev deleted\n", __func__);
			hsic.modem_dev = NULL;
		}
		s3_wake_unlock();
		break;
	default:
		pr_debug("Notify action not supported\n");
		break ;
	}
	return;
}

static void hsic_port_suspend(struct usb_device *udev)
{
	struct pci_dev *pdev = to_pci_dev(udev->bus->controller);

	if (pdev->device != 0x119d)
		return;

	/* Ignore USB devices on external hub */
	if (udev->parent && udev->parent->parent)
		return;

	/* Only valid for hsic port1 */
	if (udev->portnum == 2) {
		pr_debug("%s ignore hsic port2\n", __func__);
		return;
	}

	/* Modem dev */
	if (udev->parent) {
		pr_debug("%s s3 wlock unlocked\n", __func__);
		s3_wake_unlock();
	}
	count_ipc_stats(0, BUS_SUSPEND);
}

static void hsic_port_resume(struct usb_device *udev)
{
	int retval;
	struct pci_dev *pdev = to_pci_dev(udev->bus->controller);

	if (pdev->device != 0x119d)
		return;

	/* Ignore USB devices on external hub */
	if (udev->parent && udev->parent->parent)
		return;

	/* Only valid for hsic port1 */
	if (udev->portnum == 2) {
		pr_debug("%s ignore hsic port2\n", __func__);
		return;
	}

	/* Modem dev */
	if ((udev->parent) && (hsic.s3_rt_state != SUSPENDING)) {
		pr_debug("%s s3 wlock locked\n", __func__);
		s3_wake_lock();
	}
	count_ipc_stats(0, BUS_RESUME);
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

static void hsic_aux_work(struct work_struct *work)
{
	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);
	if (hsic.modem_dev == NULL) {
		dev_dbg(&pci_dev->dev,
			"%s---->Modem not created\n", __func__);
		return;
	}

	mutex_lock(&hsic.hsic_mutex);
	/* Free the aux irq */
	hsic_aux_irq_free();
	dev_dbg(&pci_dev->dev,
		"%s---->AUX IRQ is disabled\n", __func__);

	if (hsic.hsic_stopped == 0)
		ehci_hsic_stop_host(pci_dev);
	hsic_enter_exit_d3(1);
	usleep_range(5000, 6000);
	hsic_enter_exit_d3(0);
	ehci_hsic_start_host(pci_dev);

	hsic.autosuspend_enable = 0;
	usb_disable_autosuspend(hsic.rh_dev);

	hsic.hsic_aux_finish = 1;
	wake_up(&hsic.aux_wq);
	mutex_unlock(&hsic.hsic_mutex);

	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);
	return;
}

static void wakeup_work(struct work_struct *work)
{
	dev_dbg(&pci_dev->dev,
		"%s---->\n", __func__);

	if (!mutex_trylock(&hsic.hsic_mutex)) {
		queue_delayed_work(hsic.work_queue, &hsic.wakeup_work,
				   msecs_to_jiffies(10));
		return;
	}

	if (hsic.modem_dev == NULL) {
		mutex_unlock(&hsic.hsic_mutex);
		dev_dbg(&pci_dev->dev,
			"%s---->Modem not created\n", __func__);
		return;
	}

	pm_runtime_get_sync(&hsic.modem_dev->dev);
	/* need some time to wait modem device resume */
	usleep_range(500, 600);
	pm_runtime_put_sync(&hsic.modem_dev->dev);

	mutex_unlock(&hsic.hsic_mutex);

	dev_dbg(&pci_dev->dev,
		"%s<----\n", __func__);

	return;
}

static int hsic_debugfs_host_resume_show(struct seq_file *s, void *unused)
{
	return 0;
}
static int hsic_debugfs_host_resume_open(struct inode *inode, struct file *file)
{
	return single_open(file, hsic_debugfs_host_resume_show,
						inode->i_private);
}


static ssize_t hsic_debugfs_host_resume_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct usb_hcd *hcd = s->private;

	dev_dbg(hcd->self.controller, "wakeup hsic\n");
	queue_delayed_work(hsic.work_queue, &hsic.wakeup_work, 0);

	return count;
}


static const struct file_operations hsic_debugfs_host_resume_fops = {
	.open			= hsic_debugfs_host_resume_open,
	.read			= seq_read,
	.write			= hsic_debugfs_host_resume_write,
	.release		= single_release,
};

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

	if (size > HSIC_ENABLE_SIZE) {
		dev_dbg(dev, "Invalid, size = %d\n", size);
		return -EINVAL;
	}

	if (sscanf(buf, "%d", &org_req) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	/* Free the aux irq */
	hsic_aux_irq_free();
	dev_dbg(dev,
		"%s---->AUX IRQ is disabled\n", __func__);

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
	if (org_req) {
		dev_dbg(dev, "enable hsic\n");
		/* add this due to hcd release
		 doesn't set hcd to NULL */
		if (hsic.hsic_stopped == 0)
			ehci_hsic_stop_host(pci_dev);
		hsic_enter_exit_d3(1);
		usleep_range(5000, 6000);
		hsic_enter_exit_d3(0);
		retval = ehci_hsic_start_host(pci_dev);
		if (retval < 0) {
			dev_err(&pci_dev->dev,
				"start host fail, retval %d\n", retval);
			mutex_unlock(&hsic.hsic_mutex);
			return retval;
		}

		hsic.autosuspend_enable = 0;
		usb_disable_autosuspend(hsic.rh_dev);
	} else {
		dev_dbg(dev, "disable hsic\n");

		/* If device enable auto suspend, disable it before disable hsic */
		if (hsic.autosuspend_enable) {
			dev_dbg(dev, "disable pm\n");
			if (hsic.modem_dev != NULL) {
				usb_disable_autosuspend(hsic.modem_dev);
				hsic.autosuspend_enable = 0;
			}
			if (hsic.rh_dev != NULL) {
				usb_disable_autosuspend(hsic.rh_dev);
				hsic.autosuspend_enable = 0;
			}
		}

		/* add this due to hcd release
		 doesn't set hcd to NULL */
		if (hsic.hsic_stopped == 0)
			ehci_hsic_stop_host(pci_dev);
	}

	mutex_unlock(&hsic.hsic_mutex);

	return size;
}

static DEVICE_ATTR(hsic_enable, S_IRUGO | S_IWUSR | S_IROTH,
		hsic_port_enable_show, hsic_port_enable_store);

static ssize_t hsic_port_inactivityDuration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.port_inactivityDuration);
}

static ssize_t hsic_port_inactivityDuration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
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

static DEVICE_ATTR(L2_inactivityDuration,
		S_IRUGO | S_IWUSR | S_IROTH,
		hsic_port_inactivityDuration_show,
		 hsic_port_inactivityDuration_store);

/* Interfaces for auto suspend */
static ssize_t hsic_autosuspend_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.autosuspend_enable);
}

static ssize_t hsic_autosuspend_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
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
			dev_dbg(dev, "Modem dev autosuspend enable\n");
			usb_enable_autosuspend(hsic.modem_dev);
			hsic_wakeup_irq_init();
		}
	}
	if (hsic.rh_dev != NULL) {
		if (hsic.autosuspend_enable == 0) {
			dev_dbg(dev, "Port dev autosuspend disable\n");
			usb_disable_autosuspend(hsic.rh_dev);
		} else {
			dev_dbg(dev, "Port dev autosuspend enable\n");
			usb_enable_autosuspend(hsic.rh_dev);
		}
	}
	mutex_unlock(&hsic.hsic_mutex);
	return size;
}

static DEVICE_ATTR(L2_autosuspend_enable, S_IRUGO | S_IWUSR | S_IROTH,
		hsic_autosuspend_enable_show,
		 hsic_autosuspend_enable_store);

static ssize_t hsic_pm_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.autosuspend_enable);
}

static ssize_t hsic_pm_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int rc;
	u8 pm_enable;

	if (size > HSIC_ENABLE_SIZE)
		return -EINVAL;

	if (sscanf(buf, "%d", &pm_enable) != 1) {
		dev_dbg(dev, "Invalid, value\n");
		return -EINVAL;
	}

	/*  pm_enable definition: 0b00 - L1 & L2 disabled, 0b01 - L2 only
	  *  0b10 - L1 only, 0b11 - L1 + L2 enabled
	   */
	switch (pm_enable) {
	case 0:
		rc = hsic_autosuspend_enable_store(dev, attr, "0", size);
		break;
	case 1:
		rc = hsic_autosuspend_enable_store(dev, attr, "1", size);
		break;
	case 2: /*Reserved for L1 only*/
		break;
	case 3: /* Reserved for L1 + L2*/
		break;
	default:
		rc = -EINVAL;
	}

	if (rc == size)
		return size;
	else
		return -EINVAL;
}

static DEVICE_ATTR(pm_enable, S_IRUGO | S_IWUSR | S_IROTH,
		hsic_pm_enable_show,
		 hsic_pm_enable_store);

static ssize_t hsic_bus_inactivityDuration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hsic.bus_inactivityDuration);
}

static ssize_t hsic_bus_inactivityDuration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
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
		S_IRUGO | S_IWUSR | S_IROTH,
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

static DEVICE_ATTR(remoteWakeup, S_IRUGO | S_IWUSR | S_IROTH,
		hsic_remoteWakeup_show, hsic_remoteWakeup_store);

static int hsic_debugfs_registers_show(struct seq_file *s, void *unused)
{
	struct usb_hcd	*hcd = s->private;

	pm_runtime_get_sync(hcd->self.controller);

	seq_printf(s,
		"\n"
		"USBCMD = 0x%08x\n"
		"USBSTS = 0x%08x\n"
		"USBINTR = 0x%08x\n"
		"ASYNCLISTADDR = 0x%08x\n"
		"PORTSC1 = 0x%08x\n"
		"PORTSC2 = 0x%08x\n"
		"HOSTPC1 = 0x%08x\n"
		"HOSTPC2 = 0x%08x\n"
		"OTGSC = 0x%08x\n"
		"USBMODE = 0x%08x\n",
		readl(hcd->regs + 0x30),
		readl(hcd->regs + 0x34),
		readl(hcd->regs + 0x38),
		readl(hcd->regs + 0x48),
		readl(hcd->regs + 0x74),
		readl(hcd->regs + 0x78),
		readl(hcd->regs + 0xb4),
		readl(hcd->regs + 0xb8),
		readl(hcd->regs + 0xf4),
		readl(hcd->regs + 0xf8)
		);

	pm_runtime_put_sync(hcd->self.controller);

	return 0;
}

static int hsic_debugfs_registers_open(struct inode *inode, struct file *file)
{
	return single_open(file, hsic_debugfs_registers_show, inode->i_private);
}

static const struct file_operations hsic_debugfs_registers_fops = {
	.open			= hsic_debugfs_registers_open,
	.read			= seq_read,
	.release		= single_release,
};

static int create_class_device_files(void)
{
	int retval;

	hsic_class = class_create(NULL, "hsic");

	if (IS_ERR(hsic_class))
			return -EFAULT;

	hsic_class_dev = device_create(hsic_class, &pci_dev->dev,
			MKDEV(0, 0), NULL, "hsic0");

	if (IS_ERR(hsic_class_dev)) {
		retval = -EFAULT;
		goto hsic_class_fail;
	}

	retval = device_create_file(hsic_class_dev, &dev_attr_hsic_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "error create hsic_enable\n");
		goto hsic_class_dev_fail;
	}
	hsic.autosuspend_enable = 0;
	retval = device_create_file(hsic_class_dev,
			 &dev_attr_L2_autosuspend_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create autosuspend_enable\n");
		goto hsic_class_dev_fail;
	}

	hsic.port_inactivityDuration = HSIC_PORT_INACTIVITYDURATION;
	retval = device_create_file(hsic_class_dev,
			 &dev_attr_L2_inactivityDuration);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create port_inactiveDuration\n");
		goto hsic_class_dev_fail;
	}

	hsic.bus_inactivityDuration = HSIC_BUS_INACTIVITYDURATION;
	retval = device_create_file(hsic_class_dev,
			 &dev_attr_bus_inactivityDuration);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create bus_inactiveDuration\n");
		goto hsic_class_dev_fail;
	}

	hsic.remoteWakeup_enable = HSIC_REMOTEWAKEUP;
	retval = device_create_file(hsic_class_dev, &dev_attr_remoteWakeup);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "Error create remoteWakeup\n");
		goto hsic_class_dev_fail;
	}

	retval = device_create_file(hsic_class_dev,
		 &dev_attr_pm_enable);

	if (retval == 0)
		return retval;

	dev_dbg(&pci_dev->dev, "Error create pm_enable\n");

hsic_class_dev_fail:
	device_destroy(hsic_class, hsic_class_dev->devt);
hsic_class_fail:
	class_destroy(hsic_class);

	return retval;
}

static void remove_class_device_files(void)
{
	device_destroy(hsic_class, hsic_class_dev->devt);
	class_destroy(hsic_class);
}

/* FixMe: create_device_files() need to be removed */
static int create_device_files()
{
	int retval;

	retval = device_create_file(&pci_dev->dev, &dev_attr_hsic_enable);
	if (retval < 0) {
		dev_dbg(&pci_dev->dev, "error create hsic_enable\n");
		goto hsic_enable;
	}
	hsic.autosuspend_enable = 0;
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
host_resume:
	device_remove_file(&pci_dev->dev, &dev_attr_hsic_enable);
hsic_enable:
hsic_class_fail:

	return retval;
}

static void remove_device_files()
{
	device_remove_file(&pci_dev->dev, &dev_attr_L2_autosuspend_enable);
	device_remove_file(&pci_dev->dev, &dev_attr_L2_inactivityDuration);
	device_remove_file(&pci_dev->dev, &dev_attr_bus_inactivityDuration);
	device_remove_file(&pci_dev->dev, &dev_attr_remoteWakeup);
	device_remove_file(&pci_dev->dev, &dev_attr_hsic_enable);
}

static void hsic_debugfs_cleanup()
{
	debugfs_remove_recursive(hsic_debugfs_root);
	hsic_debugfs_root = NULL;
}

static int hsic_debugfs_init(struct usb_hcd *hcd)
{
	int retval = 0;
	struct dentry *file;

	if (!hsic_debugfs_root) {
		hsic_debugfs_root = debugfs_create_dir("hsic", usb_debug_root);
		if (!hsic_debugfs_root) {
			retval = -ENOMEM;
			dev_dbg(hcd->self.controller, "	Error create debugfs root failed !");
			return retval;
		}
		file = debugfs_create_file(
				"registers",
				S_IRUGO,
				hsic_debugfs_root,
				hcd,
				&hsic_debugfs_registers_fops);
		if (!file) {
			retval = -ENOMEM;
			dev_dbg(hcd->self.controller, "	Error create debugfs file registers failed !");
			goto remove_debugfs;
		}
		file = debugfs_create_file(
				"host_resume",
				S_IRUGO | S_IWUSR | S_IROTH,
				hsic_debugfs_root,
				hcd,
				&hsic_debugfs_host_resume_fops);
		if (!file) {
			retval = -ENOMEM;
			dev_dbg(hcd->self.controller, "	Error create debugfs file host_resume failed !");
			goto remove_debugfs;
		}
		ipc_debug_control = debugfs_create_file("ipc_control", S_IRUGO,
						hsic_debugfs_root, NULL,
						&ipc_control_fops);
		if (!ipc_debug_control) {
			retval = -ENOENT;
			goto remove_debugfs;
		}

		ipc_stats = debugfs_create_file("ipc_stats", S_IRUGO,
						hsic_debugfs_root, NULL,
						&ipc_stats_fops);
		if (!ipc_stats) {
			retval = -ENOENT;
			goto remove_debugfs;
		}

		stats_enable = STATS_DISABLE;
		ipc_counter_init();
	}

	if (retval != 0)
		goto remove_debugfs;

	return retval;
remove_debugfs:
	hsic_debugfs_cleanup();
	return retval;
}

static int ehci_hsic_probe(struct pci_dev *pdev,
				const struct pci_device_id *id)
{
	struct hc_driver *driver;
	struct usb_hcd *hcd;
	struct ehci_hcd *ehci;
	int irq, retval;

	pr_debug("initializing Intel EHCI HSIC Host Controller\n");

	if (usb_disabled())
		return -ENODEV;

	if (!id)
		return -EINVAL;

	pci_dev = pdev;
	if (pci_enable_device(pdev) < 0)
		return -ENODEV;
	pdev->current_state = PCI_D0;

	wake_lock_init(&hsic.resume_wake_lock,
		WAKE_LOCK_SUSPEND, "hsic_aux2_wlock");
	wake_lock_init(&hsic.s3_wake_lock,
			WAKE_LOCK_SUSPEND, "hsic_s3_wlock");
	hsic.hsic_pm_nb.notifier_call = hsic_pm_notify;
	usb_register_notify(&hsic.hsic_pm_nb);
	hsic.hsic_s3_entry_nb.notifier_call = hsic_s3_entry_notify;
	register_pm_notifier(&hsic.hsic_s3_entry_nb);

	/* we need not call pci_enable_dev since otg transceiver already take
	 * the control of this device and this probe actaully gets called by
	 * otg transceiver driver with HNP protocol.
	 */
	irq = pdev->irq;
	if (!pdev->irq) {
		dev_dbg(&pdev->dev, "No IRQ.\n");
		retval = -ENODEV;
		goto disable_pci;
	}

	driver = (struct hc_driver *)id->driver_data;
	if (!driver)
		return -EINVAL;

	/* AUX GPIO init */
	retval = hsic_aux_gpio_init();
	if (retval < 0) {
		dev_err(&pdev->dev, "AUX GPIO init fail\n");
		retval = -ENODEV;
		goto disable_pci;
	}

	/* AUX GPIO init */
	retval = hsic_wakeup_gpio_init();
	if (retval < 0) {
		dev_err(&pdev->dev, "Wakeup GPIO init fail\n");
		retval = -ENODEV;
		goto disable_pci;
	}

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto disable_pci;
	}

	ehci = hcd_to_ehci(hcd);

	hcd->rsrc_start = pci_resource_start(pdev, 0);
	hcd->rsrc_len = pci_resource_len(pdev, 0);
	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,
			driver->description)) {
		dev_dbg(&pdev->dev, "controller already in use\n");
		retval = -EBUSY;
		goto clear_companion;
	}

	hcd->regs = ioremap_nocache(hcd->rsrc_start, hcd->rsrc_len);
	if (hcd->regs == NULL) {
		dev_dbg(&pdev->dev, "error mapping memory\n");
		retval = -EFAULT;
		goto release_mem_region;
	}

	pci_set_master(pdev);

	if (hsic.hsic_enable_created == 0) {
		retval = create_device_files();
		if (retval < 0) {
			dev_dbg(&pdev->dev, "error create device files\n");
			goto release_mem_region;
		}

		retval = create_class_device_files();
		if (retval < 0) {
			dev_dbg(&pdev->dev, "error create device files\n");
			goto release_mem_region;
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
	INIT_DELAYED_WORK(&hsic.wakeup_work, wakeup_work);
	INIT_DELAYED_WORK(&(hsic.hsic_aux), hsic_aux_work);

	hcd->hsic_notify = hsic_notify;

	retval = usb_add_hcd(hcd, irq, IRQF_DISABLED | IRQF_SHARED | IRQF_NO_SUSPEND);
	if (retval != 0)
		goto unmap_registers;
	dev_set_drvdata(&pdev->dev, hcd);
	/* Clear phy low power mode, enable phy clock */
	ehci_hsic_phy_power(ehci, 0);

	if (pci_dev_run_wake(pdev))
		pm_runtime_put_noidle(&pdev->dev);

	if (!enabling_disabling) {
		/* Check here to avoid to call pm_runtime_put_noidle() twice */
		if (!pci_dev_run_wake(pdev))
			pm_runtime_put_noidle(&pdev->dev);

		pm_runtime_allow(&pdev->dev);
	}
	hsic.hsic_stopped = 0;
	hsic_enable = 1;
	hsic.s3_rt_state = RESUMED;
	s3_wake_lock();
	hsic_debugfs_init(hcd);

	if (current->pid == INIT_TASK_PID) {
		dev_info(&pdev->dev, "disable hsic on driver init!\n");
		ehci_hsic_stop_host(pdev);
	}

	return retval;
unmap_registers:
	destroy_workqueue(hsic.work_queue);
	if (driver->flags & HCD_MEMORY) {
		iounmap(hcd->regs);
release_mem_region:
		release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	} else
		release_region(hcd->rsrc_start, hcd->rsrc_len);
clear_companion:
	dev_set_drvdata(&pdev->dev, NULL);
	usb_put_hcd(hcd);
disable_pci:
	pci_disable_device(pdev);
	dev_err(&pdev->dev, "init %s fail, %d\n", dev_name(&pdev->dev), retval);
	wake_lock_destroy(&(hsic.resume_wake_lock));
	wake_lock_destroy(&hsic.s3_wake_lock);
	return retval;
}

static void ehci_hsic_remove(struct pci_dev *pdev)
{
	struct usb_hcd    *hcd = pci_get_drvdata(pdev);
	struct ehci_hcd   *ehci = hcd_to_ehci(hcd);

	if (!hcd)
		return;

	hsic.hsic_stopped = 1;
	hsic_enable = 0;

	if (pci_dev_run_wake(pdev))
		pm_runtime_get_noresume(&pdev->dev);

	if (!enabling_disabling) {
		if (!pci_dev_run_wake(pdev))
			pm_runtime_get_noresume(&pdev->dev);

		pm_runtime_forbid(&pdev->dev);
	}

	/* Free the aux irq */
	hsic_aux_irq_free();
	hsic_wakeup_irq_free();

	/* Fake an interrupt request in order to give the driver a chance
	 * to test whether the controller hardware has been removed (e.g.,
	 * cardbus physical eject).
	 */
	local_irq_disable();
	usb_hcd_irq(0, hcd);
	local_irq_enable();

	usb_remove_hcd(hcd);

#if 0
	ehci_hsic_port_power(ehci, 0);
#endif
	/* Set phy low power mode, disable phy clock */
	ehci_hsic_phy_power(ehci, 1);

	if (hcd->driver->flags & HCD_MEMORY) {
		iounmap(hcd->regs);
		release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	} else {
		release_region(hcd->rsrc_start, hcd->rsrc_len);
	}

	usb_put_hcd(hcd);
	gpio_free(hsic.aux_gpio);
	gpio_free(hsic.wakeup_gpio);
	pci_disable_device(pdev);

	cancel_delayed_work_sync(&hsic.wakeup_work);

	destroy_workqueue(hsic.work_queue);
	wake_lock_destroy(&(hsic.resume_wake_lock));
	wake_lock_destroy(&hsic.s3_wake_lock);
	usb_unregister_notify(&hsic.hsic_pm_nb);
	unregister_pm_notifier(&hsic.hsic_s3_entry_nb);
	hsic_debugfs_cleanup();
}

static void ehci_hsic_shutdown(struct pci_dev *pdev)
{
	struct usb_hcd *hcd;

	mutex_lock(&hsic.hsic_mutex);
	if (hsic.hsic_stopped == 1) {
		dev_dbg(&pdev->dev, "hsic stopped return\n");
		mutex_unlock(&hsic.hsic_mutex);
		return;
	}
	mutex_unlock(&hsic.hsic_mutex);

	dev_dbg(&pdev->dev, "%s --->\n", __func__);
	hcd = pci_get_drvdata(pdev);
	if (!hcd)
		return;

	if (test_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags) &&
			hcd->driver->shutdown) {
		hcd->driver->shutdown(hcd);
		pci_disable_device(pdev);
	}
	dev_dbg(&pdev->dev, "%s <---\n", __func__);
}

#ifdef CONFIG_PM_SLEEP
static int tangier_hsic_suspend_noirq(struct device *dev)
{
	int	retval;

	mutex_lock(&hsic.hsic_mutex);
	if (hsic.hsic_stopped == 1) {
		dev_dbg(dev, "hsic stopped return\n");
		mutex_unlock(&hsic.hsic_mutex);
		return 0;
	}

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.suspend_noirq(dev);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	mutex_unlock(&hsic.hsic_mutex);
	return retval;
}

static int tangier_hsic_suspend(struct device *dev)
{
	int	retval;

	mutex_lock(&hsic.hsic_mutex);
	if (hsic.hsic_stopped == 1) {
		dev_dbg(dev, "hsic stopped return\n");
		mutex_unlock(&hsic.hsic_mutex);
		return 0;
	}
	mutex_unlock(&hsic.hsic_mutex);

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.suspend(dev);
	count_ipc_stats(retval, D3_ENTRY);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}

static int tangier_hsic_resume_noirq(struct device *dev)
{
	int	retval;

	mutex_lock(&hsic.hsic_mutex);
	if (hsic.hsic_stopped == 1) {
		dev_dbg(dev, "hsic stopped return\n");
		mutex_unlock(&hsic.hsic_mutex);
		return 0;
	}

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.resume_noirq(dev);
	hsic.s3_rt_state = RESUMED;
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	mutex_unlock(&hsic.hsic_mutex);
	return retval;
}

static int tangier_hsic_resume(struct device *dev)
{
	int	retval;

	mutex_lock(&hsic.hsic_mutex);
	if (hsic.hsic_stopped == 1) {
		dev_dbg(dev, "hsic stopped return\n");
		mutex_unlock(&hsic.hsic_mutex);
		return 0;
	}
	mutex_unlock(&hsic.hsic_mutex);

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.resume(dev);
	count_ipc_stats(retval, D3_EXIT);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}
#else
#define tangier_hsic_suspend_noirq	NULL
#define tangier_hsic_suspend		NULL
#define tangier_hsic_resume_noirq	NULL
#define tangier_hsic_resume		NULL
#endif

#ifdef CONFIG_PM_RUNTIME
/* Runtime PM */
static int tangier_hsic_runtime_suspend(struct device *dev)
{
	int	retval;

	if (hsic.hsic_stopped == 1) {
		dev_dbg(dev, "hsic stopped return\n");
		return 0;
	}

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.runtime_suspend(dev);
	s3_wake_unlock();
	count_ipc_stats(retval, D0I3_ENTRY);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);
	return retval;
}

static int tangier_hsic_runtime_resume(struct device *dev)
{
	int			retval;

	if (hsic.hsic_stopped == 1) {
		dev_dbg(dev, "hsic stopped return\n");
		return 0;
	}

	dev_dbg(dev, "%s --->\n", __func__);
	retval = usb_hcd_pci_pm_ops.runtime_resume(dev);
	count_ipc_stats(retval, D0I3_EXIT);
	dev_dbg(dev, "%s <--- retval = %d\n", __func__, retval);

	return retval;
}
#else
#define tangier_hsic_runtime_suspend NULL
#define tangier_hsic_runtime_resume NULL
#endif


static DEFINE_PCI_DEVICE_TABLE(pci_hsic_ids) = {
	{
		.vendor =	0x8086,
		.device =	0x119D,
		.subvendor =	PCI_ANY_ID,
		.subdevice =	PCI_ANY_ID,
		.driver_data =  (unsigned long) &ehci_pci_hc_driver,
	},
	{ /* end: all zeroes */ }
};

static const struct dev_pm_ops tangier_hsic_pm_ops = {
	.runtime_suspend = tangier_hsic_runtime_suspend,
	.runtime_resume = tangier_hsic_runtime_resume,
	.suspend = tangier_hsic_suspend,
	.suspend_noirq = tangier_hsic_suspend_noirq,
	.resume = tangier_hsic_resume,
	.resume_noirq = tangier_hsic_resume_noirq,
};

/* Intel HSIC EHCI driver */
static struct pci_driver ehci_hsic_driver = {
	.name =	"ehci-intel-hsic",
	.id_table =	pci_hsic_ids,

	.probe =	ehci_hsic_probe,
	.remove =	ehci_hsic_remove,

#ifdef CONFIG_PM_SLEEP
	.driver =	{
		.pm =	&tangier_hsic_pm_ops
	},
#endif
	.shutdown =	ehci_hsic_shutdown,
};


static int ehci_hsic_start_host(struct pci_dev  *pdev)
{
	int		retval;

	pm_runtime_get_sync(&pdev->dev);
	enabling_disabling = 1;
	retval = ehci_hsic_probe(pdev, ehci_hsic_driver.id_table);
	if (retval)
		dev_dbg(&pdev->dev, "Failed to start host\n");
	enabling_disabling = 0;
	pm_runtime_put(&pdev->dev);

	return retval;
}
EXPORT_SYMBOL_GPL(ehci_hsic_start_host);

static int ehci_hsic_stop_host(struct pci_dev *pdev)
{
	pm_runtime_get_sync(&pdev->dev);
	enabling_disabling = 1;
	ehci_hsic_remove(pdev);
	enabling_disabling = 0;
	pm_runtime_put(&pdev->dev);

	return 0;
}
EXPORT_SYMBOL_GPL(ehci_hsic_stop_host);
