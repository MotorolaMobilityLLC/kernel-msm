/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
#include <osdep.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#include "if_usb.h"
#include "hif_usb_internal.h"
#include "bmi_msg.h"		/* TARGET_TYPE_ */
#include "regtable.h"
#include "ol_fw.h"
#include <osapi_linux.h>
#include "vos_api.h"
#include "wma_api.h"
#include "wlan_hdd_main.h"
#include "epping_main.h"

#ifdef WLAN_BTAMP_FEATURE
#include "wlan_btc_svc.h"
#include "wlan_nlink_common.h"
#endif

#ifndef REMOVE_PKT_LOG
#include "ol_txrx_types.h"
#include "pktlog_ac_api.h"
#include "pktlog_ac.h"
#endif
#define VENDOR_ATHR             0x0CF3
#define AR9888_DEVICE_ID (0x003c)
#define AR6320_DEVICE_ID (0x9378)
#define DELAY_FOR_TARGET_READY 200	/* 200ms */
#define DELAY_INT_FOR_HDD_REMOVE 200	/* 200ms */
#define HIFDiagWriteCOLDRESET(hifdevice) HIFDiagWriteAccess(sc->hif_device, \
				(ROME_USB_SOC_RESET_CONTROL_COLD_RST_LSB | \
				ROME_USB_RTC_SOC_BASE_ADDRESS), \
				SOC_RESET_CONTROL_COLD_RST_SET(1))

unsigned int msienable;
module_param(msienable, int, 0644);
#define HIF_USB_UNLOAD_TIMEOUT		(2*HZ)
enum hif_usb_drv_unload_state {
	HIF_USB_UNLOAD_STATE_NULL = 0,
	HIF_USB_UNLOAD_STATE_DRV_DEREG,
	HIF_USB_UNLOAD_STATE_TARGET_RESET,
	HIF_USB_UNLOAD_STATE_DEV_DISCONNECTED,
};

static int hif_usb_unload_dev_num = -1;
static wait_queue_head_t hif_usb_unload_event_wq;
static atomic_t hif_usb_unload_state;
struct hif_usb_softc *usb_sc = NULL;
static int hif_usb_resume(struct usb_interface *interface);

static int
hif_usb_configure(struct hif_usb_softc *sc, hif_handle_t *hif_hdl,
		  struct usb_interface *interface)
{
	int ret = 0;
	struct usb_device *dev = interface_to_usbdev(interface);

	if (HIF_USBDeviceInserted(interface, sc)) {
		pr_err("ath: %s: Target probe failed.\n", __func__);
		ret = -EIO;
		goto err_stalled;
	}

	if (athdiag_procfs_init(sc) != 0) {
		pr_err("athdiag_procfs_init failed\n");
		return A_ERROR;
	}
	hif_usb_unload_dev_num = dev->devnum;
	*hif_hdl = sc->hif_device;
	return 0;

err_stalled:

	return ret;
}

static void hif_nointrs(struct hif_usb_softc *sc)
{
}

static int hif_usb_reboot(struct notifier_block *nb, unsigned long val,
			     void *v)
{
	struct hif_usb_softc *sc;
	sc = container_of(nb, struct hif_usb_softc, reboot_notifier);
	/* do cold reset */
	HIFDiagWriteCOLDRESET(sc->hif_device);
	return NOTIFY_DONE;
}

static int
hif_usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int ret = 0;
	struct hif_usb_softc *sc;
	struct ol_softc *ol_sc;
	struct usb_device *pdev = interface_to_usbdev(interface);
	int vendor_id, product_id;

	pr_info("hif_usb_probe\n");
	usb_get_dev(pdev);
	vendor_id = le16_to_cpu(pdev->descriptor.idVendor);
	product_id = le16_to_cpu(pdev->descriptor.idProduct);

	ret = 0;

	sc = A_MALLOC(sizeof(*sc));
	if (!sc) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	OS_MEMZERO(sc, sizeof(*sc));
	sc->pdev = (void *)pdev;
	sc->dev = &pdev->dev;

	sc->aps_osdev.bdev = pdev;
	sc->aps_osdev.device = &pdev->dev;
	sc->aps_osdev.bc.bc_bustype = HAL_BUS_TYPE_AHB;
	sc->devid = id->idProduct;

	adf_os_spinlock_init(&sc->target_lock);

	ol_sc = A_MALLOC(sizeof(*ol_sc));
	if (!ol_sc)
		goto err_attach;
	OS_MEMZERO(ol_sc, sizeof(*ol_sc));
	ol_sc->sc_osdev = &sc->aps_osdev;
	ol_sc->hif_sc = (void *)sc;
	sc->ol_sc = ol_sc;

	if ((usb_control_msg(pdev, usb_sndctrlpipe(pdev, 0),
			     USB_REQ_SET_CONFIGURATION, 0, 1, 0, NULL, 0,
			     HZ)) < 0) {
		pr_info("%s[%d]\n\r", __func__, __LINE__);
	}
	usb_set_interface(pdev, 0, 0);

	if (hif_usb_configure(sc, &ol_sc->hif_hdl, interface))
		goto err_config;

	ol_sc->enableuartprint = 1;
	ol_sc->enablefwlog = 0;
	ol_sc->enablesinglebinary = FALSE;
	ol_sc->max_no_of_peers = 1;

	init_waitqueue_head(&ol_sc->sc_osdev->event_queue);

	ret = hdd_wlan_startup(&pdev->dev, ol_sc);
	if (ret) {
		hif_nointrs(sc);
		if (sc->hif_device != NULL) {
			((HIF_DEVICE_USB *)(sc->hif_device))->sc = NULL;
		}
		athdiag_procfs_remove();
		goto err_config;
	}
	sc->hdd_removed = 0;
	sc->hdd_removed_processing = 0;
	sc->hdd_removed_wait_cnt = 0;

#ifndef REMOVE_PKT_LOG
	if (vos_get_conparam() != VOS_FTM_MODE &&
        !WLAN_IS_EPPING_ENABLED(vos_get_conparam())) {
		/*
		 * pktlog initialization
		 */
		ol_pl_sethandle(&ol_sc->pdev_txrx_handle->pl_dev, ol_sc);

		if (pktlogmod_init(ol_sc))
			pr_err("%s: pktlogmod_init failed\n", __func__);
	}
#endif

#ifdef WLAN_BTAMP_FEATURE
	/* Send WLAN UP indication to Nlink Service */
	send_btc_nlink_msg(WLAN_MODULE_UP_IND, 0);
#endif

	sc->interface = interface;
	sc->reboot_notifier.notifier_call = hif_usb_reboot;
	register_reboot_notifier(&sc->reboot_notifier);

	usb_sc = sc;
	return 0;

err_config:
	A_FREE(ol_sc);
err_attach:
	ret = -EIO;
	usb_sc = NULL;
	A_FREE(sc);
err_alloc:
	usb_put_dev(pdev);

	return ret;
}

static void hif_usb_remove(struct usb_interface *interface)
{
	HIF_DEVICE_USB *device = usb_get_intfdata(interface);
	struct hif_usb_softc *sc = device->sc;
	struct ol_softc *scn;

	/* Attach did not succeed, all resources have been
	 * freed in error handler
	 */
	if (!sc)
		return;
	/* wait __hdd_wlan_exit until finished and no more than 4 seconds*/
	while(usb_sc->hdd_removed_processing == 1 &&
			usb_sc->hdd_removed_wait_cnt < 20) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(DELAY_INT_FOR_HDD_REMOVE));
		set_current_state(TASK_RUNNING);
		usb_sc->hdd_removed_wait_cnt ++;
	}
	/* do cold reset */
	HIFDiagWriteCOLDRESET(sc->hif_device);
	/* wait for target jump to boot code and finish the initialization */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(DELAY_FOR_TARGET_READY));
	set_current_state(TASK_RUNNING);
	if (usb_sc->local_state.event != 0) {
		hif_usb_resume(usb_sc->interface);
		usb_sc->local_state.event = 0;
	}
	unregister_reboot_notifier(&sc->reboot_notifier);
	usb_put_dev(interface_to_usbdev(interface));
	if (atomic_read(&hif_usb_unload_state) ==
			HIF_USB_UNLOAD_STATE_DRV_DEREG)
		atomic_set(&hif_usb_unload_state,
			   HIF_USB_UNLOAD_STATE_TARGET_RESET);
	scn = sc->ol_sc;

	if (usb_sc->hdd_removed == 0) {
		usb_sc->hdd_removed_processing = 1;
#ifndef REMOVE_PKT_LOG
	if (vos_get_conparam() != VOS_FTM_MODE &&
		!WLAN_IS_EPPING_ENABLED(vos_get_conparam()))
		pktlogmod_exit(scn);
#endif
		__hdd_wlan_exit();
		usb_sc->hdd_removed_processing = 0;
		usb_sc->hdd_removed = 1;
	}
	hif_nointrs(sc);
	HIF_USBDeviceDetached(interface, 1);
	A_FREE(scn);
	A_FREE(sc);
	usb_sc = NULL;
	pr_info("hif_usb_remove!!!!!!\n");
}

#ifdef WLAN_LINK_UMAC_SUSPEND_WITH_BUS_SUSPEND
void hdd_suspend_wlan(void (*callback) (void *callbackContext),
		      void *callbackContext);
#endif

static int hif_usb_suspend(struct usb_interface *interface, pm_message_t state)
{
	HIF_DEVICE_USB *device = usb_get_intfdata(interface);
	struct hif_usb_softc *sc = device->sc;
	void *vos = vos_get_global_context(VOS_MODULE_ID_HIF, NULL);
	v_VOID_t * temp_module;

	if (vos == NULL)
		return 0;
	/* No need to send WMI_PDEV_SUSPEND_CMDID to FW if WOW is enabled */
	temp_module = vos_get_context(VOS_MODULE_ID_WDA, vos);
	if (!temp_module) {
		printk("%s: WDA module is NULL\n", __func__);
		return (-1);
	}

	if (wma_check_scan_in_progress(temp_module)) {
		printk("%s: Scan in progress. Aborting suspend\n", __func__);
		return (-1);
	}
	sc->local_state = state;
	/* No need to send WMI_PDEV_SUSPEND_CMDID to FW if WOW is enabled */
	if (wma_is_wow_mode_selected(temp_module)) {
		if (wma_enable_wow_in_fw(temp_module, 0)) {
			pr_warn("%s[%d]: fail\n", __func__, __LINE__);
			return -1;
		}
	} else if ((PM_EVENT_FREEZE & state.event) == PM_EVENT_FREEZE ||
		(PM_EVENT_SUSPEND & state.event) == PM_EVENT_SUSPEND ||
		(PM_EVENT_HIBERNATE & state.event) == PM_EVENT_HIBERNATE) {
		if (wma_suspend_target
		    (vos_get_context(VOS_MODULE_ID_WDA, vos), 0)) {
			pr_warn("%s[%d]: fail\n", __func__, __LINE__);
			return -1;
		}
	}
	usb_hif_flush_all(device);
	return 0;
}

#ifdef WLAN_LINK_UMAC_SUSPEND_WITH_BUS_SUSPEND
void hdd_resume_wlan(void);
#endif

static int hif_usb_resume(struct usb_interface *interface)
{
	HIF_DEVICE_USB *device = usb_get_intfdata(interface);
	struct hif_usb_softc *sc = device->sc;
	void *vos = vos_get_global_context(VOS_MODULE_ID_HIF, NULL);
	v_VOID_t * temp_module;

	if (vos == NULL)
		return 0;
	/* No need to send WMI_PDEV_SUSPEND_CMDID to FW if WOW is enabled */
	temp_module = vos_get_context(VOS_MODULE_ID_WDA, vos);
	if (!temp_module) {
		printk("%s: WDA module is NULL\n", __func__);
		return (-1);
	}

	if (wma_check_scan_in_progress(temp_module)) {
		printk("%s: Scan in progress. Aborting suspend\n", __func__);
		return (-1);
	}
	sc->local_state.event = 0;
	usb_hif_start_recv_pipes(device);

#ifdef USB_HIF_TEST_INTERRUPT_IN
	usb_hif_post_recv_transfers(&device->pipes[HIF_RX_INT_PIPE],
				    HIF_USB_RX_BUFFER_SIZE);
#endif
	/* No need to send WMI_PDEV_RESUME_CMDID to FW if WOW is enabled */
	if (!wma_is_wow_mode_selected(temp_module)) {
		wma_resume_target(temp_module, 0);
	} else if (wma_disable_wow_in_fw(temp_module, 0)) {
		pr_warn("%s[%d]: fail\n", __func__, __LINE__);
		return (-1);
	}

	return 0;
}

static int hif_usb_reset_resume(struct usb_interface *intf)
{
	HIF_DEVICE_USB *device = usb_get_intfdata(intf);
	struct hif_usb_softc *sc = device->sc;

	HIFDiagWriteCOLDRESET(sc->hif_device);

	return 0;
}

static struct usb_device_id hif_usb_id_table[] = {
	{USB_DEVICE_AND_INTERFACE_INFO(VENDOR_ATHR, 0x9378, 0xFF, 0xFF, 0xFF)},
	{}			/* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, hif_usb_id_table);
struct usb_driver hif_usb_drv_id = {

	.name = "hif_usb",
	.id_table = hif_usb_id_table,
	.probe = hif_usb_probe,
	.disconnect = hif_usb_remove,
#ifdef ATH_BUS_PM
	.suspend = hif_usb_suspend,
	.resume = hif_usb_resume,
	.reset_resume = hif_usb_reset_resume,
#endif
	.supports_autosuspend = true,
};

void hif_init_adf_ctx(adf_os_device_t adf_dev, void *ol_sc)
{
	struct ol_softc *sc = (struct ol_softc *)ol_sc;
	struct hif_usb_softc *hif_sc = (struct hif_usb_softc *)sc->hif_sc;
	adf_dev->drv = &hif_sc->aps_osdev;
	adf_dev->drv_hdl = hif_sc->aps_osdev.bdev;
	adf_dev->dev = hif_sc->aps_osdev.device;
	sc->adf_dev = adf_dev;
}

void hif_deinit_adf_ctx(void *ol_sc)
{
	struct ol_softc *sc = (struct ol_softc *)ol_sc;
	sc->adf_dev = NULL;
}

static int hif_usb_dev_notify(struct notifier_block *nb,
				 unsigned long action, void *dev)
{
	struct usb_device *udev;
	int ret = NOTIFY_DONE;

	if (action != USB_DEVICE_REMOVE)
		goto done;

	udev = (struct usb_device *) dev;
	if (hif_usb_unload_dev_num != udev->devnum)
		goto done;

	if (atomic_read(&hif_usb_unload_state) ==
			HIF_USB_UNLOAD_STATE_TARGET_RESET) {
		atomic_set(&hif_usb_unload_state,
			   HIF_USB_UNLOAD_STATE_DEV_DISCONNECTED);
		wake_up(&hif_usb_unload_event_wq);
	}

done:
	return ret;
}

static struct notifier_block hif_usb_dev_nb = {
	.notifier_call = hif_usb_dev_notify,
};
static int is_usb_driver_register = 0;
int hif_register_driver(void)
{
	is_usb_driver_register = 1;
	init_waitqueue_head(&hif_usb_unload_event_wq);
	atomic_set(&hif_usb_unload_state, HIF_USB_UNLOAD_STATE_NULL);
	usb_register_notify(&hif_usb_dev_nb);
	return usb_register(&hif_usb_drv_id);
}

void hif_unregister_driver(void)
{
	if (is_usb_driver_register) {
		long timeleft = 0;
		if (usb_sc != NULL) {
			/* wait __hdd_wlan_exit until finished and no more than
			 * 4 seconds
			 */
			while(usb_sc->hdd_removed_processing == 1 &&
					usb_sc->hdd_removed_wait_cnt < 20) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(msecs_to_jiffies(
						DELAY_INT_FOR_HDD_REMOVE));
				set_current_state(TASK_RUNNING);
				usb_sc->hdd_removed_wait_cnt ++;
			}
			if (usb_sc->local_state.event != 0) {
				hif_usb_resume(usb_sc->interface);
				usb_sc->local_state.event = 0;
			}

			if (usb_sc->hdd_removed == 0) {
				usb_sc->hdd_removed_processing = 1;
#ifndef REMOVE_PKT_LOG
	            if (vos_get_conparam() != VOS_FTM_MODE &&
		            !WLAN_IS_EPPING_ENABLED(vos_get_conparam()))
					pktlogmod_exit(usb_sc->ol_sc);
#endif
				__hdd_wlan_exit();
				usb_sc->hdd_removed_processing = 0;
				usb_sc->hdd_removed = 1;
			}
		}
		is_usb_driver_register = 0;
		atomic_set(&hif_usb_unload_state,
			   HIF_USB_UNLOAD_STATE_DRV_DEREG);
		usb_deregister(&hif_usb_drv_id);
		if (atomic_read(&hif_usb_unload_state) !=
				HIF_USB_UNLOAD_STATE_TARGET_RESET)
			goto finish;
		timeleft = wait_event_interruptible_timeout(
				hif_usb_unload_event_wq,
				atomic_read(&hif_usb_unload_state) ==
				HIF_USB_UNLOAD_STATE_DEV_DISCONNECTED,
				HIF_USB_UNLOAD_TIMEOUT);
		if (timeleft <= 0)
			pr_err("Fail to wait from DRV_DEREG to DISCONNECT,"
				"timeleft = %ld \n\r",
				timeleft);
finish:
		usb_unregister_notify(&hif_usb_dev_nb);
	}
}

/* Function to set the TXRX handle in the ol_sc context */
void hif_init_pdev_txrx_handle(void *ol_sc, void *txrx_handle)
{
	struct ol_softc *sc = (struct ol_softc *)ol_sc;
	sc->pdev_txrx_handle = txrx_handle;
}

void hif_disable_isr(void *ol_sc)
{
	/* TODO */
}

/* Function to reset SoC */
void hif_reset_soc(void *ol_sc)
{
	/* TODO */
}

void hif_get_hw_info(void *ol_sc, u32 *version, u32 *revision)
{
	u_int32_t hif_type, target_type;
	A_STATUS rv;
	A_INT32 ret = 0;
	A_UINT32 chip_id;
	struct hif_usb_softc *sc;

	sc = ((struct ol_softc *)ol_sc)->hif_sc;
	if (sc->hostdef == NULL && sc->targetdef == NULL) {
		switch (((struct ol_softc *)ol_sc)->target_type)
		{
			case TARGET_TYPE_AR6320:
				switch(((struct ol_softc *)ol_sc)->target_version) {
					case AR6320_REV1_VERSION:
					case AR6320_REV1_1_VERSION:
					case AR6320_REV1_3_VERSION:
						hif_type = HIF_TYPE_AR6320;
						target_type = TARGET_TYPE_AR6320;
						break;
					case AR6320_REV2_1_VERSION:
					case AR6320_REV3_VERSION:
					case AR6320_REV3_2_VERSION:
						hif_type = HIF_TYPE_AR6320V2;
						target_type = TARGET_TYPE_AR6320V2;
						break;
					default:
						ret = -1;
						break;
				}
				break;
			default:
				ret = -1;
				break;
		}

		if (!ret) {
			/* assign target register table if we find corresponding type */
			hif_register_tbl_attach(sc, hif_type);
			target_register_tbl_attach(sc, target_type);
			/* read the chip revision*/
			rv = HIFDiagReadAccess(sc->hif_device, (CHIP_ID_ADDRESS | RTC_SOC_BASE_ADDRESS), &chip_id);
			if (rv != A_OK) {
				pr_err("ath: HIF_PCIDeviceProbed get chip id val (%d)\n", rv);
			}
			((struct ol_softc *)ol_sc)->target_revision = CHIP_ID_REVISION_GET(chip_id);
		}
	}

	/* we need to get chip revision here */
	*version = ((struct ol_softc *)ol_sc)->target_version;
	/* Chip version should be supported, set to 0 for now */
	*revision = ((struct ol_softc *)ol_sc)->target_revision;
}

void hif_set_fw_info(void *ol_sc, u32 target_fw_version)
{
	((struct ol_softc *)ol_sc)->target_fw_version = target_fw_version;
}

int hif_pm_runtime_prevent_suspend(void *ol_sc)
{
	if (usb_sc && usb_sc->interface)
		return usb_autopm_get_interface_async(usb_sc->interface);
	else {
		pr_err("%s: USB interface isn't ready for autopm\n", __func__);
		return 0;
	}
}

int hif_pm_runtime_allow_suspend(void *ol_sc)
{
	if (usb_sc && usb_sc->interface)
		usb_autopm_put_interface_async(usb_sc->interface);
	else
		pr_err("%s: USB interface isn't ready for autopm\n", __func__);
	return 0;
}

int hif_pm_runtime_prevent_suspend_timeout(void *ol_sc, unsigned int delay)
{
        return 0;
}

MODULE_LICENSE("Dual BSD/GPL");
