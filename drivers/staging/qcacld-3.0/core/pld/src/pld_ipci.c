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
#include <linux/list.h>
#include <linux/slab.h>

#ifdef CONFIG_PLD_IPCI_ICNSS
#include <soc/qcom/icnss2.h>
#endif

#include "pld_internal.h"
#include "pld_ipci.h"
#include "osif_psoc_sync.h"

#ifdef CONFIG_PLD_IPCI_ICNSS
/**
 * pld_ipci_probe() - Probe function for platform driver
 * @dev: device
 *
 * The probe function will be called when platform device
 * is detected.
 *
 * Return: int
 */
static int pld_ipci_probe(struct device *dev)
{
	struct pld_context *pld_context;
	int ret = 0;

	pld_context = pld_get_global_context();
	if (!pld_context) {
		ret = -ENODEV;
		goto out;
	}

	ret = pld_add_dev(pld_context, dev, NULL, PLD_BUS_TYPE_IPCI);
	if (ret)
		goto out;

	return pld_context->ops->probe(dev, PLD_BUS_TYPE_IPCI,
				       NULL, NULL);

out:
	return ret;
}

/**
 * pld_ipci_remove() - Remove function for platform device
 * @dev: device
 *
 * The remove function will be called when platform device
 * is disconnected
 *
 * Return: void
 */
static void pld_ipci_remove(struct device *dev)
{
	struct pld_context *pld_context;
	int errno;
	struct osif_psoc_sync *psoc_sync;

	errno = osif_psoc_sync_trans_start_wait(dev, &psoc_sync);
	if (errno)
		return;

	osif_psoc_sync_unregister(dev);
	osif_psoc_sync_wait_for_ops(psoc_sync);

	pld_context = pld_get_global_context();

	if (!pld_context)
		goto out;

	pld_context->ops->remove(dev, PLD_BUS_TYPE_SNOC);

	pld_del_dev(pld_context, dev);

out:
	osif_psoc_sync_trans_stop(psoc_sync);
	osif_psoc_sync_destroy(psoc_sync);
}

/**
 * pld_ipci_reinit() - SSR re-initialize function for platform device
 * @dev: device
 *
 * During subsystem restart(SSR), this function will be called to
 * re-initialize platform device.
 *
 * Return: int
 */
static int pld_ipci_reinit(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->reinit)
		return pld_context->ops->reinit(dev, PLD_BUS_TYPE_IPCI,
						NULL, NULL);

	return -ENODEV;
}

/**
 * pld_ipci_shutdown() - SSR shutdown function for platform device
 * @dev: device
 *
 * During SSR, this function will be called to shutdown platform device.
 *
 * Return: void
 */
static void pld_ipci_shutdown(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		pld_context->ops->shutdown(dev, PLD_BUS_TYPE_IPCI);
}

/**
 * pld_ipci_crash_shutdown() - Crash shutdown function for platform device
 * @dev: device
 *
 * This function will be called when a crash is detected, it will shutdown
 * platform device.
 *
 * Return: void
 */
static void pld_ipci_crash_shutdown(void *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (pld_context->ops->crash_shutdown)
		pld_context->ops->crash_shutdown(dev, PLD_BUS_TYPE_IPCI);
}

/**
 * pld_ipci_pm_suspend() - PM suspend callback function for power management
 * @dev: device
 *
 * This function is to suspend the platform device when power management
 * is enabled.
 *
 * Return: void
 */
static int pld_ipci_pm_suspend(struct device *dev)
{
	struct pld_context *pld_context;
	pm_message_t state;

	state.event = PM_EVENT_SUSPEND;
	pld_context = pld_get_global_context();
	return pld_context->ops->suspend(dev, PLD_BUS_TYPE_IPCI, state);
}

/**
 * pld_ipci_pm_resume() - PM resume callback function for power management
 * @pdev: device
 *
 * This function is to resume the platform device when power management
 * is enabled.
 *
 * Return: void
 */
static int pld_ipci_pm_resume(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	return pld_context->ops->resume(dev, PLD_BUS_TYPE_IPCI);
}

/**
 * pld_ipci_suspend_noirq() - Complete the actions started by suspend()
 * @dev: device
 *
 * Complete the actions started by suspend().  Carry out any
 * additional operations required for suspending the device that might be
 * racing with its driver's interrupt handler, which is guaranteed not to
 * run while suspend_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ipci_suspend_noirq(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->suspend_noirq)
		return pld_context->ops->suspend_noirq(dev, PLD_BUS_TYPE_IPCI);
	return 0;
}

/**
 * pld_ipci_resume_noirq() - Prepare for the execution of resume()
 * @pdev: device
 *
 * Prepare for the execution of resume() by carrying out any
 * operations required for resuming the device that might be racing with
 * its driver's interrupt handler, which is guaranteed not to run while
 * resume_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ipci_resume_noirq(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->resume_noirq)
		return pld_context->ops->resume_noirq(dev, PLD_BUS_TYPE_IPCI);

	return 0;
}

/**
 * pld_ipci_runtime_suspend() - Runtime suspend callback for power management
 * @dev: device
 *
 * This function is to runtime suspend the platform device when power management
 * is enabled.
 *
 * Return: status
 */
static int pld_ipci_runtime_suspend(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops && pld_context->ops->runtime_suspend)
		return pld_context->ops->runtime_suspend(dev,
							 PLD_BUS_TYPE_IPCI);

	return 0;
}

/**
 * pld_ipci_runtime_resume() - Runtime resume callback for power management
 * @pdev: device
 *
 * This function is to runtime resume the platform device when power management
 * is enabled.
 *
 * Return: status
 */
static int pld_ipci_runtime_resume(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops && pld_context->ops->runtime_resume)
		return pld_context->ops->runtime_resume(dev, PLD_BUS_TYPE_IPCI);

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static int pld_update_hang_evt_data(struct icnss_uevent_hang_data *evt_data,
				    struct pld_uevent_data *data)
{
	if (!evt_data || !data)
		return -EINVAL;

	data->hang_data.hang_event_data = evt_data->hang_event_data;
	data->hang_data.hang_event_data_len = evt_data->hang_event_data_len;
	return 0;
}

static int pld_ipci_uevent(struct device *dev,
			   struct icnss_uevent_data *uevent)
{
	struct pld_context *pld_context;
	struct icnss_uevent_fw_down_data *uevent_data = NULL;
	struct pld_uevent_data data = {0};
	struct icnss_uevent_hang_data *hang_data = NULL;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (!pld_context->ops->uevent)
		goto out;

	if (!uevent)
		return -EINVAL;

	switch (uevent->uevent) {
	case ICNSS_UEVENT_FW_CRASHED:
		data.uevent = PLD_FW_CRASHED;
		break;
	case ICNSS_UEVENT_FW_DOWN:
		if (!uevent->data)
			return -EINVAL;
		uevent_data = (struct icnss_uevent_fw_down_data *)uevent->data;
		data.uevent = PLD_FW_DOWN;
		data.fw_down.crashed = uevent_data->crashed;
		break;
	case ICNSS_UEVENT_HANG_DATA:
		if (!uevent->data)
			return -EINVAL;
		hang_data = (struct icnss_uevent_hang_data *)uevent->data;
		data.uevent = PLD_FW_HANG_EVENT;
		pld_update_hang_evt_data(hang_data, &data);
		break;
	case ICNSS_UEVENT_SMMU_FAULT:
		if (!uevent->data)
			return -EINVAL;
		uevent_data = (struct icnss_uevent_fw_down_data *)uevent->data;
		data.uevent = PLD_SMMU_FAULT;
		data.fw_down.crashed = uevent_data->crashed;
		break;
	default:
		goto out;
	}

	pld_context->ops->uevent(dev, &data);
out:
	return 0;
}
#else
static int pld_ipci_uevent(struct device *dev,
			   struct icnss_uevent_data *uevent)
{
	struct pld_context *pld_context;
	struct icnss_uevent_fw_down_data *uevent_data = NULL;
	struct pld_uevent_data data = {0};

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (!pld_context->ops->uevent)
		goto out;

	if (!uevent)
		return -EINVAL;

	switch (uevent->uevent) {
	case ICNSS_UEVENT_FW_CRASHED:
		data.uevent = PLD_FW_CRASHED;
		break;
	case ICNSS_UEVENT_FW_DOWN:
		if (!uevent->data)
			return -EINVAL;
		uevent_data = (struct icnss_uevent_fw_down_data *)uevent->data;
		data.uevent = PLD_FW_DOWN;
		data.fw_down.crashed = uevent_data->crashed;
		break;
	default:
		goto out;
	}

	pld_context->ops->uevent(dev, &data);
out:
	return 0;
}
#endif

/**
 * pld_ipci_idle_restart_cb() - Perform idle restart
 * @pdev: platform device
 *
 * This function will be called if there is an idle restart request
 *
 * Return: int
 */
static int pld_ipci_idle_restart_cb(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();

	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->idle_restart)
		return pld_context->ops->idle_restart(dev,
						      PLD_BUS_TYPE_IPCI);

	return -ENODEV;
}

/**
 * pld_ipci_idle_shutdown_cb() - Perform idle shutdown
 * @pdev: PCIE device
 * @id: PCIE device ID
 *
 * This function will be called if there is an idle shutdown request
 *
 * Return: int
 */
static int pld_ipci_idle_shutdown_cb(struct device *dev)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();

	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->shutdown)
		return pld_context->ops->idle_shutdown(dev,
						       PLD_BUS_TYPE_IPCI);

	return -ENODEV;
}

/**
 * pld_ipci_set_thermal_state() - Set thermal state for thermal mitigation
 * @dev: device
 * @thermal_state: Thermal state set by thermal subsystem
 * @mon_id: Thermal cooling device ID
 *
 * This function will be called when thermal subsystem notifies platform
 * driver about change in thermal state.
 *
 * Return: 0 for success
 * Non zero failure code for errors
 */
static int pld_ipci_set_thermal_state(struct device *dev,
				      unsigned long thermal_state,
				      int mon_id)
{
	struct pld_context *pld_context;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->set_curr_therm_cdev_state)
		return pld_context->ops->set_curr_therm_cdev_state(dev,
							      thermal_state,
							      mon_id);

	return -ENOTSUPP;
}

#ifdef MULTI_IF_NAME
#define PLD_IPCI_OPS_NAME "pld_ipci_" MULTI_IF_NAME
#else
#define PLD_IPCI_OPS_NAME "pld_ipci"
#endif

struct icnss_driver_ops pld_ipci_ops = {
	.name       = PLD_IPCI_OPS_NAME,
	.probe      = pld_ipci_probe,
	.remove     = pld_ipci_remove,
	.shutdown   = pld_ipci_shutdown,
	.reinit     = pld_ipci_reinit,
	.crash_shutdown = pld_ipci_crash_shutdown,
	.pm_suspend = pld_ipci_pm_suspend,
	.pm_resume  = pld_ipci_pm_resume,
	.suspend_noirq = pld_ipci_suspend_noirq,
	.resume_noirq = pld_ipci_resume_noirq,
	.runtime_suspend = pld_ipci_runtime_suspend,
	.runtime_resume  = pld_ipci_runtime_resume,
	.uevent = pld_ipci_uevent,
	.idle_restart = pld_ipci_idle_restart_cb,
	.idle_shutdown = pld_ipci_idle_shutdown_cb,
	.set_therm_cdev_state = pld_ipci_set_thermal_state,
};

/**
 * pld_ipci_register_driver() - Register platform device callback functions
 *
 * Return: int
 */
int pld_ipci_register_driver(void)
{
	return icnss_register_driver(&pld_ipci_ops);
}

/**
 * pld_ipci_unregister_driver() - Unregister platform device callback functions
 *
 * Return: void
 */
void pld_ipci_unregister_driver(void)
{
	icnss_unregister_driver(&pld_ipci_ops);
}

/**
 * pld_ipci_wlan_enable() - Enable WLAN
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

int pld_ipci_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version)
{
	struct icnss_wlan_enable_cfg cfg;
	enum icnss_driver_mode icnss_mode;

	if (!dev)
		return -ENODEV;

	cfg.num_ce_tgt_cfg = config->num_ce_tgt_cfg;
	cfg.ce_tgt_cfg = (struct ce_tgt_pipe_cfg *)
		config->ce_tgt_cfg;
	cfg.num_ce_svc_pipe_cfg = config->num_ce_svc_pipe_cfg;
	cfg.ce_svc_cfg = (struct ce_svc_pipe_cfg *)
		config->ce_svc_cfg;
	cfg.num_shadow_reg_cfg = config->num_shadow_reg_cfg;
	cfg.shadow_reg_cfg = (struct icnss_shadow_reg_cfg *)
		config->shadow_reg_cfg;
	cfg.num_shadow_reg_v2_cfg = config->num_shadow_reg_v2_cfg;
	cfg.shadow_reg_v2_cfg = (struct icnss_shadow_reg_v2_cfg *)
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
		icnss_mode = ICNSS_FTM;
		break;
	case PLD_EPPING:
		icnss_mode = ICNSS_EPPING;
		break;
	default:
		icnss_mode = ICNSS_MISSION;
		break;
	}

	return icnss_wlan_enable(dev, &cfg, icnss_mode, host_version);
}

/**
 * pld_ipci_wlan_disable() - Disable WLAN
 * @dev: device
 * @mode: WLAN mode
 *
 * This function disables WLAN FW. It passes WLAN mode to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ipci_wlan_disable(struct device *dev, enum pld_driver_mode mode)
{
	if (!dev)
		return -ENODEV;

	return icnss_wlan_disable(dev, ICNSS_OFF);
}

/**
 * pld_ipci_get_soc_info() - Get SOC information
 * @dev: device
 * @info: buffer to SOC information
 *
 * Return SOC info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ipci_get_soc_info(struct device *dev, struct pld_soc_info *info)
{
	int errno;
	struct icnss_soc_info icnss_info = {0};

	if (!info || !dev)
		return -ENODEV;

	errno = icnss_get_soc_info(dev, &icnss_info);
	if (errno)
		return errno;

	info->v_addr = icnss_info.v_addr;
	info->p_addr = icnss_info.p_addr;
	info->chip_id = icnss_info.chip_id;
	info->chip_family = icnss_info.chip_family;
	info->board_id = icnss_info.board_id;
	info->soc_id = icnss_info.soc_id;
	info->fw_version = icnss_info.fw_version;
	strlcpy(info->fw_build_timestamp, icnss_info.fw_build_timestamp,
		sizeof(info->fw_build_timestamp));

	return 0;
}
#endif
