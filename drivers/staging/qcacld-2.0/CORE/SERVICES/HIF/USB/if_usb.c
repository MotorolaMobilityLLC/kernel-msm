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
#include <linux/usb/hcd.h>
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
#include "vos_sched.h"

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

/*
 * Disable lpm feature of usb2.0.
 */
static int hif_usb_disable_lpm(struct usb_device *udev)
{
	struct usb_hcd *hcd;
	int ret = -EPERM;
	pr_info("Enter:%s,Line:%d\n", __func__, __LINE__);
	if (!udev || !udev->bus) {
		pr_err("Invalid input parameters\n");
	} else {
		hcd = bus_to_hcd(udev->bus);
		if (udev->usb2_hw_lpm_enabled) {
			if (hcd->driver->set_usb2_hw_lpm) {
				ret = hcd->driver->set_usb2_hw_lpm(hcd,
							udev, FALSE);
				if (!ret) {
					udev->usb2_hw_lpm_enabled = FALSE;
					udev->usb2_hw_lpm_capable = FALSE;
					pr_info("%s: LPM is disabled\n",
								__func__);
				} else {
					pr_info("%s: Fail to disable LPM\n",
								__func__);
				}
			} else {
				pr_info("%s: hcd doesn't support LPM\n",
							__func__);
			}
		} else {
			pr_info("%s: LPM isn't enabled\n", __func__);
		}
	}

	pr_info("Exit:%s,Line:%d\n", __func__, __LINE__);
	return ret;
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
	/* disable lpm to avoid usb2.0 probe timeout */
	hif_usb_disable_lpm(pdev);

	if (hif_usb_configure(sc, &ol_sc->hif_hdl, interface))
		goto err_config;

	ol_sc->enableuartprint = 1;
	ol_sc->enablefwlog = 0;
	ol_sc->enablesinglebinary = FALSE;
	ol_sc->max_no_of_peers = 1;

	init_waitqueue_head(&ol_sc->sc_osdev->event_queue);

	ret = hif_init_adf_ctx(ol_sc);
	if (ret == 0)
		ret = hdd_wlan_startup(&pdev->dev, ol_sc);
	if (ret) {
		hif_nointrs(sc);
		if (sc->hif_device != NULL) {
			((HIF_DEVICE_USB *)(sc->hif_device))->sc = NULL;
		}
		athdiag_procfs_remove();
		goto err_config;
	}
	atomic_set(&sc->hdd_removed, -1);
	atomic_set(&sc->hdd_removed_processing, 0);
	sc->hdd_removed_wait_cnt = 0;

	sc->interface = interface;
	sc->reboot_notifier.notifier_call = hif_usb_reboot;
	register_reboot_notifier(&sc->reboot_notifier);

	usb_sc = sc;
	return 0;

err_config:
	hif_deinit_adf_ctx(ol_sc);
	HIFDiagWriteCOLDRESET(sc->hif_device);
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
	struct usb_device *udev = interface_to_usbdev(interface);
	struct hif_usb_softc *sc = device->sc;
	struct ol_softc *scn;

	/* Attach did not succeed, all resources have been
	 * freed in error handler
	 */
	if (!sc)
		return;

	pr_info("Try to remove hif_usb!\n");

	/* wait __hdd_wlan_exit until finished and no more than 4 seconds*/
	while(atomic_read(&usb_sc->hdd_removed_processing) == 1 &&
			usb_sc->hdd_removed_wait_cnt < 20) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(DELAY_INT_FOR_HDD_REMOVE));
		set_current_state(TASK_RUNNING);
		usb_sc->hdd_removed_wait_cnt ++;
	}
	atomic_set(&usb_sc->hdd_removed_processing, 1);

	/* disable lpm to avoid following cold reset will
	 *cause xHCI U1/U2 timeout
	 */
	usb_disable_lpm(udev);

	/* wait for disable lpm */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(DELAY_FOR_TARGET_READY));
	set_current_state(TASK_RUNNING);

	/* do cold reset */
	HIFDiagWriteCOLDRESET(sc->hif_device);

	if (usb_sc->suspend_state) {
		hif_usb_resume(usb_sc->interface);
	}
	unregister_reboot_notifier(&sc->reboot_notifier);
	usb_put_dev(interface_to_usbdev(interface));
	if (atomic_read(&hif_usb_unload_state) ==
			HIF_USB_UNLOAD_STATE_DRV_DEREG)
		atomic_set(&hif_usb_unload_state,
			   HIF_USB_UNLOAD_STATE_TARGET_RESET);
	scn = sc->ol_sc;

        /* The logp is set by target failure's ol_ramdump_handler.
         * Coldreset occurs and do this disconnect cb, try to issue
         * offline uevent to restart driver.
         */
        if (vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
                /* dispatch 'offline' uevent to restart module */
                kobject_uevent(&scn->adf_dev->dev->kobj, KOBJ_OFFLINE);
                vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, FALSE);
        }

	if (atomic_inc_and_test(&usb_sc->hdd_removed)) {
#ifndef REMOVE_PKT_LOG
		if (vos_get_conparam() != VOS_FTM_MODE &&
			!WLAN_IS_EPPING_ENABLED(vos_get_conparam()))
			pktlogmod_exit(scn);
#endif
		__hdd_wlan_exit();
		pr_info("Exit HDD wlan... done by %s\n", __func__);
	}

	hif_nointrs(sc);
	HIF_USBDeviceDetached(interface, 1);
	atomic_set(&usb_sc->hdd_removed_processing, 0);
	hif_deinit_adf_ctx(scn);
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

	printk("Enter:%s,Line:%d\n", __func__,__LINE__);

	temp_module = vos_get_context(VOS_MODULE_ID_WDA, vos);
	if (!temp_module) {
		printk("%s: WDA module is NULL\n", __func__);
		return (-1);
	}

	if (wma_check_scan_in_progress(temp_module)) {
		printk("%s: Scan in progress. Aborting suspend\n", __func__);
		return (-1);
	}

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

	sc->suspend_state = 1;
	usb_hif_flush_all(device);

	printk("Exit:%s,Line:%d\n", __func__,__LINE__);
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

	printk("Enter:%s,Line:%d\n", __func__,__LINE__);
	temp_module = vos_get_context(VOS_MODULE_ID_WDA, vos);
	if (!temp_module) {
		printk("%s: WDA module is NULL\n", __func__);
		return (-1);
	}

	sc->suspend_state = 0;
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
	printk("Exit:%s,Line:%d\n", __func__,__LINE__);
	return 0;
}

static int hif_usb_reset_resume(struct usb_interface *intf)
{
	HIF_DEVICE_USB *device = usb_get_intfdata(intf);
	struct hif_usb_softc *sc = device->sc;

	printk("Enter:%s,Line:%d \n\r", __func__,__LINE__);
	HIFDiagWriteCOLDRESET(sc->hif_device);
	printk("Exit:%s,Line:%d \n\r", __func__,__LINE__);
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

int hif_init_adf_ctx(void *ol_sc)
{
	adf_os_device_t adf_ctx;
	v_CONTEXT_t pVosContext = NULL;
	struct ol_softc *sc = (struct ol_softc *)ol_sc;
	struct hif_usb_softc *hif_sc = (struct hif_usb_softc *)sc->hif_sc;

	pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
	if(pVosContext == NULL)
		return -EFAULT;

	adf_ctx = vos_mem_malloc(sizeof(*adf_ctx));
	if (!adf_ctx)
		return -ENOMEM;
	vos_mem_zero(adf_ctx, sizeof(*adf_ctx));
	adf_ctx->drv = &hif_sc->aps_osdev;
	adf_ctx->drv_hdl = hif_sc->aps_osdev.bdev;
	adf_ctx->dev = hif_sc->aps_osdev.device;
	sc->adf_dev = adf_ctx;
	((VosContextType*)(pVosContext))->adf_ctx = adf_ctx;
	return 0;
}

void hif_deinit_adf_ctx(void *ol_sc)
{
	struct ol_softc *sc = (struct ol_softc *)ol_sc;

	if (sc == NULL)
		return;
	if (sc->adf_dev) {
		v_CONTEXT_t pVosContext = NULL;

		pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
		vos_mem_free(sc->adf_dev);
		sc->adf_dev = NULL;
		if (pVosContext)
			((VosContextType*)(pVosContext))->adf_ctx = NULL;
	}
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
	int status = 0;
	int probe_wait_cnt = 0;
	is_usb_driver_register = 1;
	init_waitqueue_head(&hif_usb_unload_event_wq);
	atomic_set(&hif_usb_unload_state, HIF_USB_UNLOAD_STATE_NULL);
	usb_register_notify(&hif_usb_dev_nb);
	status = usb_register(&hif_usb_drv_id);

	/* wait for usb probe done, 2s at most*/
	while(!usb_sc && probe_wait_cnt < 10) {
		A_MSLEEP(200);
		probe_wait_cnt++;
	}

	if (usb_sc && status == 0)
		return 0;
	else
		return -1;
}

void hif_unregister_driver(void)
{
	if (is_usb_driver_register) {
		long timeleft = 0;
		pr_info("Try to unregister hif_driver\n");
		if (usb_sc != NULL) {
			/* wait __hdd_wlan_exit until finished and no more than
			 * 4 seconds
			 */
			while(usb_sc &&
				atomic_read(&usb_sc->hdd_removed_processing) == 1 &&
				usb_sc->hdd_removed_wait_cnt < 20) {
				usb_sc->hdd_removed_wait_cnt ++;
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(msecs_to_jiffies(
						DELAY_INT_FOR_HDD_REMOVE));
				set_current_state(TASK_RUNNING);
			}

			/* usb_sc is freed by hif_usb_remove */
			if (!usb_sc)
				goto deregister;

			atomic_set(&usb_sc->hdd_removed_processing, 1);

			if (usb_sc->suspend_state) {
				hif_usb_resume(usb_sc->interface);
			}

			if (atomic_inc_and_test(&usb_sc->hdd_removed)) {
#ifndef REMOVE_PKT_LOG
				if (vos_get_conparam() != VOS_FTM_MODE &&
					!WLAN_IS_EPPING_ENABLED(vos_get_conparam()))
					pktlogmod_exit(usb_sc->ol_sc);
#endif
				__hdd_wlan_exit();
				pr_info("Exit HDD wlan... done by %s\n", __func__);
			}
			atomic_set(&usb_sc->hdd_removed_processing, 0);
		}

deregister:
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
		pr_info("hif_unregister_driver!!!!!!\n");
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
					case QCA9377_REV1_1_VERSION:
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

MODULE_LICENSE("Dual BSD/GPL");
