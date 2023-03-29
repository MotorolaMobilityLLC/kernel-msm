/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "pld_usb.h"
#include "pld_internal.h"

#include <linux/atomic.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/list.h>
#include "osif_psoc_sync.h"

#ifdef CONFIG_PLD_USB_CNSS
#include <net/cnss2.h>
#endif


#define VENDOR_ATHR             0x0CF3
static struct usb_device_id pld_usb_id_table[] = {
	{USB_DEVICE_AND_INTERFACE_INFO(VENDOR_ATHR, 0x9378, 0xFF, 0xFF, 0xFF)},
	{USB_DEVICE_AND_INTERFACE_INFO(VENDOR_ATHR, 0x9379, 0xFF, 0xFF, 0xFF)},
	{}			/* Terminating entry */
};

atomic_t pld_usb_reg_done;

/**
 * pld_usb_probe() - pld_usb_probe
 * @interface: pointer to usb_interface structure
 * @id: pointer to usb_device_id obtained from the enumerated device

 * Return: int 0 on success and errno on failure.
 */
static int pld_usb_probe(struct usb_interface *interface,
					const struct usb_device_id *id)
{
	struct usb_device *pdev = interface_to_usbdev(interface);
	struct pld_context *pld_context;
	int ret = 0;

	pld_context = pld_get_global_context();
	if (!pld_context) {
		ret = -ENODEV;
		goto out;
	}

	ret = pld_add_dev(pld_context, &pdev->dev, &interface->dev,
			  PLD_BUS_TYPE_USB);
	if (ret)
		goto out;

	ret = pld_context->ops->probe(&pdev->dev,
				      PLD_BUS_TYPE_USB, interface, (void *)id);
	if (ret != 0) {
		pr_err("%s, probe returned %d", __func__, ret);
		atomic_set(&pld_usb_reg_done, false);
	} else {
		atomic_set(&pld_usb_reg_done, true);
	}

out:
	return ret;
}

/**
 * pld_usb_remove() - Remove function for USB device
 * @interface: pointer to usb_interface for the usb device being removed
 *
 * Return: void
 */
static void pld_usb_remove(struct usb_interface *interface)
{
	struct usb_device *pdev = interface_to_usbdev(interface);
	struct pld_context *pld_context;
	int errno;
	struct osif_psoc_sync *psoc_sync;

	errno = osif_psoc_sync_trans_start_wait(&pdev->dev, &psoc_sync);
	if (errno)
		return;

	osif_psoc_sync_unregister(&pdev->dev);
	osif_psoc_sync_wait_for_ops(psoc_sync);

	pld_context = pld_get_global_context();

	if (!pld_context)
		goto out;

	if (atomic_read(&pld_usb_reg_done) != true) {
		pr_info("%s: already de-registered!\n", __func__);
		goto out;
	}

	pld_context->ops->remove(&pdev->dev, PLD_BUS_TYPE_USB);

	pld_del_dev(pld_context, &pdev->dev);

	atomic_set(&pld_usb_reg_done, false);
out:
	osif_psoc_sync_trans_stop(psoc_sync);
	osif_psoc_sync_destroy(psoc_sync);

	pr_info("%s: done!\n", __func__);
}

/**
 * pld_usb_suspend() - Suspend callback function for power management
 * @interface: pointer to usb_interface for the usb device
 * @state: power state
 *
 * This function is to suspend the PCIE device when power management is
 * enabled.
 *
 * Return: void
 */
static int pld_usb_suspend(struct usb_interface *interface,
						pm_message_t state)
{
	struct usb_device *pdev = interface_to_usbdev(interface);
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	return pld_context->ops->suspend(&pdev->dev, PLD_BUS_TYPE_USB, state);
}

/**
 * pld_usb_resume() - Resume callback function for power management
 * @interface: pointer to usb_interface for the usb device
 *
 * This function is to resume the USB device when power management is
 * enabled.
 *
 * Return: void
 */
static int pld_usb_resume(struct usb_interface *interface)
{
	struct pld_context *pld_context;
	struct usb_device *pdev = interface_to_usbdev(interface);

	pld_context = pld_get_global_context();
	return pld_context->ops->resume(&pdev->dev, PLD_BUS_TYPE_USB);
}

/**
 * pld_usb_reset_resume() - pld_usb_reset_resume
 * @interface: pointer to usb_interface for the usb device
 *
 * Return: void
 */
static int pld_usb_reset_resume(struct usb_interface *interface)
{
	struct pld_context *pld_context;
	struct usb_device *pdev = interface_to_usbdev(interface);

	pld_context = pld_get_global_context();
	return pld_context->ops->reset_resume(&pdev->dev, PLD_BUS_TYPE_USB);
}

#ifdef CONFIG_PLD_USB_CNSS
/**
 * pld_usb_reinit() - SSR re-initialize function for USB device
 * @interface: Pointer to struct usb_interface
 * @id: Pointer to USB device ID
 *
 * Return: int
 */
static int pld_usb_reinit(struct usb_interface *interface,
			  const struct usb_device_id *id)
{
	struct pld_context *pld_context;
	struct usb_device *pdev = interface_to_usbdev(interface);

	pld_context = pld_get_global_context();
	if (pld_context->ops->reinit)
		return pld_context->ops->reinit(&pdev->dev, PLD_BUS_TYPE_USB,
						interface, (void *)id);

	return -ENODEV;
}

/**
 * pld_usb_shutdown() - SSR shutdown function for USB device
 * @interface: Pointer to struct usb_interface
 *
 * Return: void
 */
static void pld_usb_shutdown(struct usb_interface *interface)
{
	struct pld_context *pld_context;
	struct usb_device *pdev = interface_to_usbdev(interface);

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		pld_context->ops->shutdown(&pdev->dev, PLD_BUS_TYPE_USB);
}

/**
 * pld_usb_uevent() - update wlan driver status callback function
 * @interface: USB interface
 * @status driver uevent status
 *
 * This function will be called when platform driver wants to update wlan
 * driver's status.
 *
 * Return: void
 */
static void pld_usb_uevent(struct usb_interface *interface, uint32_t status)
{
	struct pld_context *pld_context;
	struct pld_uevent_data data = {0};
	struct usb_device *pdev = interface_to_usbdev(interface);

	pld_context = pld_get_global_context();
	if (!pld_context)
		return;

	switch (status) {
	case CNSS_RECOVERY:
		data.uevent = PLD_FW_RECOVERY_START;
		break;
	case CNSS_FW_DOWN:
		data.uevent = PLD_FW_DOWN;
		break;
	default:
		goto out;
	}

	if (pld_context->ops->uevent)
		pld_context->ops->uevent(&pdev->dev, &data);

out:
	return;
}
struct cnss_usb_wlan_driver pld_usb_ops = {
	.name = "pld_usb_cnss",
	.id_table = pld_usb_id_table,
	.probe = pld_usb_probe,
	.remove = pld_usb_remove,
	.shutdown = pld_usb_shutdown,
	.reinit = pld_usb_reinit,
	.update_status  = pld_usb_uevent,
#ifdef CONFIG_PM
	.suspend = pld_usb_suspend,
	.resume = pld_usb_resume,
	.reset_resume = pld_usb_reset_resume,
#endif
};

/**
 * pld_usb_register_driver() - registration routine for wlan usb drive
 *
 * Return: int negative error code on failure and 0 on success
 */
int pld_usb_register_driver(void)
{
	pr_info("%s usb_register\n", __func__);
	return cnss_usb_wlan_register_driver(&pld_usb_ops);
}

/**
 * pld_usb_unregister_driver() - de-registration routine for wlan usb driver
 *
 * Return: void
 */
void pld_usb_unregister_driver(void)
{
	cnss_usb_wlan_unregister_driver(&pld_usb_ops);
	pr_info("%s usb_deregister done!\n", __func__);
}

int pld_usb_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			enum pld_driver_mode mode, const char *host_version)
{
	struct cnss_wlan_enable_cfg cfg;
	enum cnss_driver_mode cnss_mode;

	switch (mode) {
	case PLD_FTM:
		cnss_mode = CNSS_FTM;
		break;
	case PLD_EPPING:
		cnss_mode = CNSS_EPPING;
		break;
	default:
		cnss_mode = CNSS_MISSION;
		break;
	}
	return cnss_wlan_enable(dev, &cfg, cnss_mode, host_version);
}

int pld_usb_is_fw_down(struct device *dev)
{
	return cnss_usb_is_device_down(dev);
}

int pld_usb_athdiag_read(struct device *dev, uint32_t offset,
			 uint32_t memtype, uint32_t datalen,
			 uint8_t *output)
{
	return cnss_athdiag_read(dev, offset, memtype, datalen, output);
}

int pld_usb_athdiag_write(struct device *dev, uint32_t offset,
			  uint32_t memtype, uint32_t datalen,
			  uint8_t *input)
{
	return cnss_athdiag_write(dev, offset, memtype, datalen, input);
}

#else /* CONFIG_PLD_USB_CNSS */

struct usb_driver pld_usb_ops = {
	.name = "pld_usb",
	.id_table = pld_usb_id_table,
	.probe = pld_usb_probe,
	.disconnect = pld_usb_remove,
#ifdef CONFIG_PM
	.suspend = pld_usb_suspend,
	.resume = pld_usb_resume,
	.reset_resume = pld_usb_reset_resume,
#endif
	.supports_autosuspend = true,
};

/**
 * pld_usb_register_driver() - registration routine for wlan usb driver
 *
 * Return: int negative error code on failure and 0 on success
 */
int pld_usb_register_driver(void)
{
	int status;

	usb_register(&pld_usb_ops);

	if (atomic_read(&pld_usb_reg_done) == true) {
		status = 0;
	} else {
		usb_deregister(&pld_usb_ops);
		status = -1;
	}

	pr_info("%s usb_register %s, status %d\n", __func__,
		(status == 0) ? "done" : "failed", status);

	return status;
}

/**
 * pld_usb_unregister_driver() - de-registration routine for wlan usb driver
 *
 * Return: void
 */
void pld_usb_unregister_driver(void)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (atomic_read(&pld_usb_reg_done) == false)
		return;

	pld_context->ops->remove(NULL, PLD_BUS_TYPE_USB);

	atomic_set(&pld_usb_reg_done, false);
	usb_deregister(&pld_usb_ops);
	pr_info("%s usb_deregister done!\n", __func__);
}

int pld_usb_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			enum pld_driver_mode mode, const char *host_version)
{
	return 0;
}

int pld_usb_is_fw_down(struct device *dev)
{
	return 0;
}

int pld_usb_athdiag_read(struct device *dev, uint32_t offset,
			 uint32_t memtype, uint32_t datalen,
			 uint8_t *output)
{
	return 0;
}

int pld_usb_athdiag_write(struct device *dev, uint32_t offset,
			  uint32_t memtype, uint32_t datalen,
			  uint8_t *input)
{
	return 0;
}

#endif /* CONFIG_PLD_USB_CNSS */
