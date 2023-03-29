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

#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/slab.h>

#ifdef CONFIG_PLD_PCIE_CNSS
#include <net/cnss2.h>
#endif

#include "pld_internal.h"
#include "pld_pcie.h"
#include "osif_psoc_sync.h"

#ifdef CONFIG_PCI

#ifdef QCA_WIFI_3_0_ADRASTEA
#define CE_COUNT_MAX 12
#else
#define CE_COUNT_MAX 8
#endif

/**
 * pld_pcie_probe() - Probe function for PCIE platform driver
 * @pdev: PCIE device
 * @id: PCIE device ID table
 *
 * The probe function will be called when PCIE device provided
 * in the ID table is detected.
 *
 * Return: int
 */
static int pld_pcie_probe(struct pci_dev *pdev,
			  const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	int ret = 0;

	pld_context = pld_get_global_context();
	if (!pld_context) {
		ret = -ENODEV;
		goto out;
	}

	ret = pld_add_dev(pld_context, &pdev->dev, NULL, PLD_BUS_TYPE_PCIE);
	if (ret)
		goto out;

	return pld_context->ops->probe(&pdev->dev,
		       PLD_BUS_TYPE_PCIE, pdev, (void *)id);

out:
	return ret;
}


/**
 * pld_pcie_remove() - Remove function for PCIE device
 * @pdev: PCIE device
 *
 * The remove function will be called when PCIE device is disconnected
 *
 * Return: void
 */
static void pld_pcie_remove(struct pci_dev *pdev)
{
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

	pld_context->ops->remove(&pdev->dev, PLD_BUS_TYPE_PCIE);

	pld_del_dev(pld_context, &pdev->dev);

out:
	osif_psoc_sync_trans_stop(psoc_sync);
	osif_psoc_sync_destroy(psoc_sync);
}

#ifdef CONFIG_PLD_PCIE_CNSS
/**
 * pld_pcie_idle_restart_cb() - Perform idle restart
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle restart request
 *
 * Return: int
 */
static int pld_pcie_idle_restart_cb(struct pci_dev *pdev,
				    const struct pci_device_id *id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->idle_restart)
		return pld_context->ops->idle_restart(&pdev->dev,
						      PLD_BUS_TYPE_PCIE);

	return -ENODEV;
}

/**
 * pld_pcie_idle_shutdown_cb() - Perform idle shutdown
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle shutdown request
 *
 * Return: int
 */
static int pld_pcie_idle_shutdown_cb(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		return pld_context->ops->idle_shutdown(&pdev->dev,
						       PLD_BUS_TYPE_PCIE);

	return -ENODEV;
}

/**
 * pld_pcie_reinit() - SSR re-initialize function for PCIE device
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * During subsystem restart(SSR), this function will be called to
 * re-initialize PCIE device.
 *
 * Return: int
 */
static int pld_pcie_reinit(struct pci_dev *pdev,
			    const struct pci_device_id *id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->reinit)
		return pld_context->ops->reinit(&pdev->dev,
				PLD_BUS_TYPE_PCIE, pdev, (void *)id);

	return -ENODEV;
}

/**
 * pld_pcie_shutdown() - SSR shutdown function for PCIE device
 * @pdev: PCIE device
 *
 * During SSR, this function will be called to shutdown PCIE device.
 *
 * Return: void
 */
static void pld_pcie_shutdown(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		pld_context->ops->shutdown(&pdev->dev, PLD_BUS_TYPE_PCIE);
}

/**
 * pld_pcie_crash_shutdown() - Crash shutdown function for PCIE device
 * @pdev: PCIE device
 *
 * This function will be called when a crash is detected, it will shutdown
 * the PCIE device.
 *
 * Return: void
 */
static void pld_pcie_crash_shutdown(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->crash_shutdown)
		pld_context->ops->crash_shutdown(&pdev->dev, PLD_BUS_TYPE_PCIE);
}

/**
 * pld_pcie_notify_handler() - Modem state notification callback function
 * @pdev: PCIE device
 * @state: modem power state
 *
 * This function will be called when there's a modem power state change.
 *
 * Return: void
 */
static void pld_pcie_notify_handler(struct pci_dev *pdev, int state)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->modem_status)
		pld_context->ops->modem_status(&pdev->dev,
					       PLD_BUS_TYPE_PCIE, state);
}

/**
 * pld_pcie_uevent() - update wlan driver status callback function
 * @pdev: PCIE device
 * @status driver uevent status
 *
 * This function will be called when platform driver wants to update wlan
 * driver's status.
 *
 * Return: void
 */
static void pld_pcie_uevent(struct pci_dev *pdev, uint32_t status)
{
	struct pld_context *pld_context;
	struct pld_uevent_data data = {0};

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

/**
 * pld_pcie_update_event() - update wlan driver status callback function
 * @pdev: PCIE device
 * @cnss_uevent_data: driver uevent data
 *
 * This function will be called when platform driver wants to update wlan
 * driver's status.
 *
 * Return: void
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static int pld_pcie_update_event(struct pci_dev *pdev,
				 struct cnss_uevent_data *uevent_data)
{
	struct pld_context *pld_context;
	struct pld_uevent_data data = {0};
	struct cnss_hang_event *hang_event;

	pld_context = pld_get_global_context();

	if (!pld_context || !uevent_data)
		return -EINVAL;

	switch (uevent_data->status) {
	case CNSS_HANG_EVENT:
		if (!uevent_data->data)
			return -EINVAL;
		hang_event = (struct cnss_hang_event *)uevent_data->data;
		data.uevent = PLD_FW_HANG_EVENT;
		data.hang_data.hang_event_data = hang_event->hang_event_data;
		data.hang_data.hang_event_data_len =
					hang_event->hang_event_data_len;
		break;
	default:
		goto out;
	}

	if (pld_context->ops->uevent)
		pld_context->ops->uevent(&pdev->dev, &data);

out:
	return 0;
}
#endif

#ifdef FEATURE_RUNTIME_PM
/**
 * pld_pcie_runtime_suspend() - PM runtime suspend
 * @pdev: PCIE device
 *
 * PM runtime suspend callback function.
 *
 * Return: int
 */
static int pld_pcie_runtime_suspend(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->runtime_suspend)
		return pld_context->ops->runtime_suspend(&pdev->dev,
							 PLD_BUS_TYPE_PCIE);

	return -ENODEV;
}

/**
 * pld_pcie_runtime_resume() - PM runtime resume
 * @pdev: PCIE device
 *
 * PM runtime resume callback function.
 *
 * Return: int
 */
static int pld_pcie_runtime_resume(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->runtime_resume)
		return pld_context->ops->runtime_resume(&pdev->dev,
							PLD_BUS_TYPE_PCIE);

	return -ENODEV;
}
#endif

#ifdef FEATURE_GET_DRIVER_MODE
/**
 * pld_pcie_get_mode() - Get current WLAN driver mode
 *
 * This function is to get current driver mode
 *
 * Return: mission mode or ftm mode
 */
static
enum cnss_driver_mode pld_pcie_get_mode(void)
{
	struct pld_context *pld_ctx =  pld_get_global_context();
	enum cnss_driver_mode cnss_mode = CNSS_MISSION;

	if (!pld_ctx)
		return cnss_mode;

	switch (pld_ctx->mode) {
	case QDF_GLOBAL_MISSION_MODE:
		cnss_mode = CNSS_MISSION;
		break;
	case QDF_GLOBAL_WALTEST_MODE:
		cnss_mode = CNSS_WALTEST;
		break;
	case QDF_GLOBAL_FTM_MODE:
		cnss_mode = CNSS_FTM;
		break;
	case QDF_GLOBAL_COLDBOOT_CALIB_MODE:
		cnss_mode = CNSS_CALIBRATION;
		break;
	case QDF_GLOBAL_EPPING_MODE:
		cnss_mode = CNSS_EPPING;
		break;
	case QDF_GLOBAL_QVIT_MODE:
		cnss_mode = CNSS_QVIT;
		break;
	default:
		cnss_mode = CNSS_MISSION;
		break;
	}
	return cnss_mode;
}
#endif
#endif

#ifdef CONFIG_PM
#ifdef CONFIG_PLD_PCIE_CNSS
/**
 * pld_pcie_suspend() - Suspend callback function for power management
 * @pdev: PCIE device
 * @state: power state
 *
 * This function is to suspend the PCIE device when power management is
 * enabled.
 *
 * Return: void
 */
static int pld_pcie_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	return pld_context->ops->suspend(&pdev->dev,
					 PLD_BUS_TYPE_PCIE, state);
}

/**
 * pld_pcie_resume() - Resume callback function for power management
 * @pdev: PCIE device
 *
 * This function is to resume the PCIE device when power management is
 * enabled.
 *
 * Return: void
 */
static int pld_pcie_resume(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	return pld_context->ops->resume(&pdev->dev, PLD_BUS_TYPE_PCIE);
}

/**
 * pld_pcie_suspend_noirq() - Complete the actions started by suspend()
 * @pdev: PCI device
 *
 * Complete the actions started by suspend().  Carry out any additional
 * operations required for suspending the device that might be racing
 * with its driver's interrupt handler, which is guaranteed not to run
 * while suspend_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_pcie_suspend_noirq(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->suspend_noirq)
		return pld_context->ops->
			suspend_noirq(&pdev->dev, PLD_BUS_TYPE_PCIE);
	return 0;
}

/**
 * pld_pcie_resume_noirq() - Prepare for the execution of resume()
 * @pdev: PCI device
 *
 * Prepare for the execution of resume() by carrying out any additional
 * operations required for resuming the device that might be racing with
 * its driver's interrupt handler, which is guaranteed not to run while
 * resume_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_pcie_resume_noirq(struct pci_dev *pdev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->resume_noirq)
		return pld_context->ops->
			resume_noirq(&pdev->dev, PLD_BUS_TYPE_PCIE);
	return 0;
}
#else
/**
 * pld_pcie_pm_suspend() - Suspend callback function for power management
 * @dev: device
 *
 * This function is to suspend the PCIE device when power management is
 * enabled.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_pcie_pm_suspend(struct device *dev)
{
	struct pld_context *pld_context;

	pm_message_t state = { .event = PM_EVENT_SUSPEND };

	pld_context = pld_get_global_context();
	return pld_context->ops->suspend(dev, PLD_BUS_TYPE_PCIE, state);
}

/**
 * pld_pcie_pm_resume() - Resume callback function for power management
 * @dev: device
 *
 * This function is to resume the PCIE device when power management is
 * enabled.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_pcie_pm_resume(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	return pld_context->ops->resume(dev, PLD_BUS_TYPE_PCIE);
}

/**
 * pld_pcie_pm_suspend_noirq() - Complete the actions started by suspend()
 * @dev: device
 *
 * Complete the actions started by suspend().  Carry out any additional
 * operations required for suspending the device that might be racing
 * with its driver's interrupt handler, which is guaranteed not to run
 * while suspend_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_pcie_pm_suspend_noirq(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->suspend_noirq)
		return pld_context->ops->suspend_noirq(dev, PLD_BUS_TYPE_PCIE);
	return 0;
}

/**
 * pld_pcie_pm_resume_noirq() - Prepare for the execution of resume()
 * @dev: device
 *
 * Prepare for the execution of resume() by carrying out any additional
 * operations required for resuming the device that might be racing with
 * its driver's interrupt handler, which is guaranteed not to run while
 * resume_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_pcie_pm_resume_noirq(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->resume_noirq)
		return pld_context->ops->
			resume_noirq(dev, PLD_BUS_TYPE_PCIE);
	return 0;
}
#endif
#endif

static struct pci_device_id pld_pcie_id_table[] = {
#ifdef CONFIG_AR6320_SUPPORT
	{ 0x168c, 0x003e, PCI_ANY_ID, PCI_ANY_ID },
#elif defined(QCA_WIFI_QCA6290)
	{ 0x17cb, 0x1100, PCI_ANY_ID, PCI_ANY_ID },
#elif defined(QCA_WIFI_QCA6390)
	{ 0x17cb, 0x1101, PCI_ANY_ID, PCI_ANY_ID },
#elif defined(QCA_WIFI_QCA6490)
	{ 0x17cb, 0x1103, PCI_ANY_ID, PCI_ANY_ID },
#elif defined(QCN7605_SUPPORT)
	{ 0x17cb, 0x1102, PCI_ANY_ID, PCI_ANY_ID },
#else
	{ 0x168c, 0x003c, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x0041, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0xabcd, PCI_ANY_ID, PCI_ANY_ID },
	{ 0x168c, 0x7021, PCI_ANY_ID, PCI_ANY_ID },
#endif
	{ 0 }
};

#ifdef MULTI_IF_NAME
#define PLD_PCIE_OPS_NAME "pld_pcie_" MULTI_IF_NAME
#else
#define PLD_PCIE_OPS_NAME "pld_pcie"
#endif

#ifdef CONFIG_PLD_PCIE_CNSS
#ifdef FEATURE_RUNTIME_PM
struct cnss_wlan_runtime_ops runtime_pm_ops = {
	.runtime_suspend = pld_pcie_runtime_suspend,
	.runtime_resume = pld_pcie_runtime_resume,
};
#endif

struct cnss_wlan_driver pld_pcie_ops = {
	.name       = PLD_PCIE_OPS_NAME,
	.id_table   = pld_pcie_id_table,
	.probe      = pld_pcie_probe,
	.remove     = pld_pcie_remove,
	.idle_restart  = pld_pcie_idle_restart_cb,
	.idle_shutdown = pld_pcie_idle_shutdown_cb,
	.reinit     = pld_pcie_reinit,
	.shutdown   = pld_pcie_shutdown,
	.crash_shutdown = pld_pcie_crash_shutdown,
	.modem_status   = pld_pcie_notify_handler,
	.update_status  = pld_pcie_uevent,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
	.update_event = pld_pcie_update_event,
#endif
#ifdef CONFIG_PM
	.suspend    = pld_pcie_suspend,
	.resume     = pld_pcie_resume,
	.suspend_noirq = pld_pcie_suspend_noirq,
	.resume_noirq  = pld_pcie_resume_noirq,
#endif
#ifdef FEATURE_RUNTIME_PM
	.runtime_ops = &runtime_pm_ops,
#endif
#ifdef FEATURE_GET_DRIVER_MODE
	.get_driver_mode  = pld_pcie_get_mode,
#endif
};

/**
 * pld_pcie_register_driver() - Register PCIE device callback functions
 *
 * Return: int
 */
int pld_pcie_register_driver(void)
{
	return cnss_wlan_register_driver(&pld_pcie_ops);
}

/**
 * pld_pcie_unregister_driver() - Unregister PCIE device callback functions
 *
 * Return: void
 */
void pld_pcie_unregister_driver(void)
{
	cnss_wlan_unregister_driver(&pld_pcie_ops);
}
#else
#ifdef CONFIG_PM
static const struct dev_pm_ops pld_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pld_pcie_pm_suspend, pld_pcie_pm_resume)
	.suspend_noirq = pld_pcie_pm_suspend_noirq,
	.resume_noirq = pld_pcie_pm_resume_noirq,
};
#endif

struct pci_driver pld_pcie_ops = {
	.name       = PLD_PCIE_OPS_NAME,
	.id_table   = pld_pcie_id_table,
	.probe      = pld_pcie_probe,
	.remove     = pld_pcie_remove,
	.driver     = {
#ifdef CONFIG_PM
		.pm = &pld_pm_ops,
#endif
	},
};

int pld_pcie_register_driver(void)
{
	return pci_register_driver(&pld_pcie_ops);
}

void pld_pcie_unregister_driver(void)
{
	pci_unregister_driver(&pld_pcie_ops);
}
#endif

/**
 * pld_pcie_get_ce_id() - Get CE number for the provided IRQ
 * @dev: device
 * @irq: IRQ number
 *
 * Return: CE number
 */
int pld_pcie_get_ce_id(struct device *dev, int irq)
{
	int ce_id = irq - 100;

	if (ce_id < CE_COUNT_MAX && ce_id >= 0)
		return ce_id;

	return -EINVAL;
}

#ifdef CONFIG_PLD_PCIE_CNSS
/**
 * pld_pcie_wlan_enable() - Enable WLAN
 * @dev: device
 * @config: WLAN configuration data
 * @mode: WLAN mode
 * @host_version: host software version
 *
 * This function enables WLAN FW. It passed WLAN configuration data,
 * WLAN mode and host software version to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version)
{
	struct cnss_wlan_enable_cfg cfg;
	enum cnss_driver_mode cnss_mode;

	cfg.num_ce_tgt_cfg = config->num_ce_tgt_cfg;
	cfg.ce_tgt_cfg = (struct cnss_ce_tgt_pipe_cfg *)
		config->ce_tgt_cfg;
	cfg.num_ce_svc_pipe_cfg = config->num_ce_svc_pipe_cfg;
	cfg.ce_svc_cfg = (struct cnss_ce_svc_pipe_cfg *)
		config->ce_svc_cfg;
	cfg.num_shadow_reg_cfg = config->num_shadow_reg_cfg;
	cfg.shadow_reg_cfg = (struct cnss_shadow_reg_cfg *)
		config->shadow_reg_cfg;
	cfg.num_shadow_reg_v2_cfg = config->num_shadow_reg_v2_cfg;
	cfg.shadow_reg_v2_cfg = (struct cnss_shadow_reg_v2_cfg *)
		config->shadow_reg_v2_cfg;
	cfg.rri_over_ddr_cfg_valid = config->rri_over_ddr_cfg_valid;
	if (config->rri_over_ddr_cfg_valid) {
		cfg.rri_over_ddr_cfg.base_addr_low =
			 config->rri_over_ddr_cfg.base_addr_low;
		cfg.rri_over_ddr_cfg.base_addr_high =
			 config->rri_over_ddr_cfg.base_addr_high;
	}

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

/**
 * pld_pcie_wlan_disable() - Disable WLAN
 * @dev: device
 * @mode: WLAN mode
 *
 * This function disables WLAN FW. It passes WLAN mode to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_wlan_disable(struct device *dev, enum pld_driver_mode mode)
{
	return cnss_wlan_disable(dev, CNSS_OFF);
}

/**
 * pld_pcie_get_fw_files_for_target() - Get FW file names
 * @dev: device
 * @pfw_files: buffer for FW file names
 * @target_type: target type
 * @target_version: target version
 *
 * Return target specific FW file names to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_get_fw_files_for_target(struct device *dev,
				     struct pld_fw_files *pfw_files,
				     u32 target_type, u32 target_version)
{
	int ret = 0;
	struct cnss_fw_files cnss_fw_files;

	if (!pfw_files)
		return -ENODEV;

	memset(pfw_files, 0, sizeof(*pfw_files));

	ret = cnss_get_fw_files_for_target(dev, &cnss_fw_files,
					   target_type, target_version);
	if (ret)
		return ret;

	scnprintf(pfw_files->image_file, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.image_file);
	scnprintf(pfw_files->board_data, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.board_data);
	scnprintf(pfw_files->otp_data, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.otp_data);
	scnprintf(pfw_files->utf_file, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.utf_file);
	scnprintf(pfw_files->utf_board_data, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.utf_board_data);
	scnprintf(pfw_files->epping_file, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.epping_file);
	scnprintf(pfw_files->evicted_data, PLD_MAX_FILE_NAME, PREFIX "%s",
		  cnss_fw_files.evicted_data);

	return 0;
}

/**
 * pld_pcie_get_platform_cap() - Get platform capabilities
 * @dev: device
 * @cap: buffer to the capabilities
 *
 * Return capabilities to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_get_platform_cap(struct device *dev, struct pld_platform_cap *cap)
{
	int ret = 0;
	struct cnss_platform_cap cnss_cap;

	if (!cap)
		return -ENODEV;

	ret = cnss_get_platform_cap(dev, &cnss_cap);
	if (ret)
		return ret;

	memcpy(cap, &cnss_cap, sizeof(*cap));
	return 0;
}

/**
 * pld_pcie_get_soc_info() - Get SOC information
 * @dev: device
 * @info: buffer to SOC information
 *
 * Return SOC info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_pcie_get_soc_info(struct device *dev, struct pld_soc_info *info)
{
	int ret = 0;
	struct cnss_soc_info cnss_info = {0};

	if (!info)
		return -ENODEV;

	ret = cnss_get_soc_info(dev, &cnss_info);
	if (ret)
		return ret;

	info->v_addr = cnss_info.va;
	info->p_addr = cnss_info.pa;
	info->chip_id = cnss_info.chip_id;
	info->chip_family = cnss_info.chip_family;
	info->board_id = cnss_info.board_id;
	info->soc_id = cnss_info.soc_id;
	info->fw_version = cnss_info.fw_version;
	strlcpy(info->fw_build_timestamp, cnss_info.fw_build_timestamp,
		sizeof(info->fw_build_timestamp));
	info->device_version.family_number =
		cnss_info.device_version.family_number;
	info->device_version.device_number =
		cnss_info.device_version.device_number;
	info->device_version.major_version =
		cnss_info.device_version.major_version;
	info->device_version.minor_version =
		cnss_info.device_version.minor_version;

	return 0;
}

/**
 * pld_pcie_schedule_recovery_work() - schedule recovery work
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_pcie_schedule_recovery_work(struct device *dev,
				     enum pld_recovery_reason reason)
{
	enum cnss_recovery_reason cnss_reason;

	switch (reason) {
	case PLD_REASON_LINK_DOWN:
		cnss_reason = CNSS_REASON_LINK_DOWN;
		break;
	default:
		cnss_reason = CNSS_REASON_DEFAULT;
		break;
	}
	cnss_schedule_recovery(dev, cnss_reason);
}

/**
 * pld_pcie_device_self_recovery() - device self recovery
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_pcie_device_self_recovery(struct device *dev,
				   enum pld_recovery_reason reason)
{
	enum cnss_recovery_reason cnss_reason;

	switch (reason) {
	case PLD_REASON_LINK_DOWN:
		cnss_reason = CNSS_REASON_LINK_DOWN;
		break;
	default:
		cnss_reason = CNSS_REASON_DEFAULT;
		break;
	}
	cnss_self_recovery(dev, cnss_reason);
}
#endif
#endif
